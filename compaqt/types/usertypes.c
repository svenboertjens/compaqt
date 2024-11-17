// This file contains custom datatype handling

#include "globals/exceptions.h"
#include "globals/typemasks.h"
#include "globals/internals.h"
#include "globals/typedefs.h"

/* HASH TABLE */

#define HASH(x) (((uintptr_t)(x) >> 8) & 31)

static inline hash_table_t *get_hash_table(PyTypeObject **keys, PyObject **vals, uint8_t *idxs, int amt)
{
    hash_table_t *buf = (hash_table_t *)malloc(sizeof(hash_table_t) + (sizeof(keyval_t) * amt));

    if (buf == NULL)
        return NULL;

    uint8_t lengths[32] = {0};

    for (int i = 0; i < amt; ++i)
    {
        const uint8_t hash = HASH(keys[i]);
        lengths[hash]++;
    }

    memcpy(buf->lengths, lengths, sizeof(lengths));

    buf->offsets[0] = 0;

    for (int i = 1; i < 32; ++i)
        buf->offsets[i] = buf->offsets[i - 1] + buf->lengths[i - 1];
    
    memset(buf->idxs, 0, sizeof(buf->idxs));

    for (int i = 0; i < amt; ++i)
    {
        const uint8_t hash = HASH(keys[i]);
        const uint8_t length = --lengths[hash];
        const uint8_t offset = buf->offsets[hash] + length;

        buf->keyvals[offset].key = keys[i];
        buf->keyvals[offset].val = vals[i];
        buf->idxs[offset] = idxs[i];
    }

    return buf;
}

static inline PyObject *pull_from_table(hash_table_t *table, PyTypeObject *key, size_t *index)
{
    const uint8_t hash = HASH(key);

    uint8_t offset = table->offsets[hash];

    const uint8_t end_offset = offset + table->lengths[hash];
    for (; offset < end_offset; ++offset)
    {
        keyval_t keyval = table->keyvals[offset];

        if (keyval.key == key)
        {
            *index = table->idxs[offset];
            return keyval.val;
        }
    }

    return NULL;
}

/* USER TYPES */

// Max types is 1 byte: 8 bits - 3 datatype mask bits = 5 bits: 2^5 - 1 = 32
#define MAX_CUSTOM_TYPES 32
#define MAX_TYPE_IDX (MAX_CUSTOM_TYPES - 1)

void utypes_encode_dealloc(utypes_encode_ob *self)
{
    hash_table_t *table = self->table;

    if (table != NULL)
    {
        // Calculate the total length
        size_t num_chains = 0;
        for (int i = 0; i < 32; ++i)
            num_chains += table->lengths[i];

        // Decref all types and functions stored 
        for (int i = 0; i < num_chains; ++i)
        {
            Py_DECREF(table->keyvals[i].key);
            Py_DECREF(table->keyvals[i].val);
        }

        free(self->table);
    }

    if (Py_TYPE(self)->tp_free)
        Py_TYPE(self)->tp_free((PyObject *)self);
    else
        PyObject_Del(self);
}

void utypes_decode_dealloc(utypes_decode_ob *self)
{
    for (size_t i = 0; i < MAX_TYPE_IDX; ++i)
        Py_XDECREF(self->reads[i]);

    free(self->reads);

    if (Py_TYPE(self)->tp_free)
        Py_TYPE(self)->tp_free((PyObject *)self);
    else
        PyObject_Del(self);
}

PyTypeObject utypes_encode_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.CustomWriteTypes",
    .tp_basicsize = sizeof(utypes_encode_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)utypes_encode_dealloc,
};

PyTypeObject utypes_decode_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.CustomReadTypes",
    .tp_basicsize = sizeof(utypes_decode_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)utypes_decode_dealloc,
};

PyObject *get_utypes_encode_ob(PyObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &data))
        return NULL;
    
    // Number of custom types
    const size_t amt = PyDict_GET_SIZE(data);

    if (amt > MAX_CUSTOM_TYPES)
    {
        PyErr_Format(PyExc_ValueError, "Only up to 32 custom types are allowed, got a dict with %zu pairs", amt);
        return NULL;
    }

    utypes_encode_ob *ob = PyObject_New(utypes_encode_ob, &utypes_encode_t);

    if (ob == NULL)
        return PyErr_NoMemory();
    
    ob->table = NULL;
    
    PyTypeObject *keys[32];
    PyObject *vals[32];
    uint8_t idxs[32];
    
    PyObject *idx;
    PyObject *tuple;
    Py_ssize_t pos = 0;

    for (int i = 0; i < amt; ++i)
    {
        PyDict_Next(data, &pos, &idx, &tuple);

        if (!PyLong_Check(idx))
        {
            PyErr_Format(PyExc_ValueError, "Expected the value on index 0 to be of type 'int', got '%s'", Py_TYPE(idx)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        if (!PyTuple_Check(tuple) || PyTuple_GET_SIZE(tuple) != 2)
        {
            PyErr_Format(PyExc_ValueError, "Expected values of 'tuple' type of size 2, got a '%s' of size %zu", Py_TYPE(tuple)->tp_name, Py_SIZE(tuple));
            Py_DECREF(ob);
            return NULL;
        }

        PyObject *type = PyTuple_GET_ITEM(tuple, 0);
        PyObject *func = PyTuple_GET_ITEM(tuple, 1);

        if (Py_TYPE(type) != &PyType_Type)
        {
            PyErr_Format(PyExc_ValueError, "Expected a key of type 'type', got '%s'", Py_TYPE(type)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }
        if (!PyCallable_Check(func))
        {
            PyErr_Format(PyExc_ValueError, "Expected the value on index 1 to be a callable object, got '%s'", Py_TYPE(func)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        // The index in the array to store the type in
        const unsigned long ptr_idx = PyLong_AsUnsignedLong(idx);

        // Check if the index was negative
        if (ptr_idx == (unsigned long)-1 && PyErr_Occurred())
        {
            PyErr_Format(PyExc_IndexError, "Custom type index should be positive, got %lu", ptr_idx);
            Py_DECREF(ob);
            return NULL;
        }

        if (ptr_idx > MAX_TYPE_IDX)
        {
            PyErr_Format(PyExc_IndexError, "Custom type index out of range: got %lu, max is %i", ptr_idx, MAX_TYPE_IDX);
            Py_DECREF(ob);
            return NULL;
        }

        // Make sure the objects stay available by increasing the refcount
        Py_INCREF(func);
        Py_INCREF(type);

        idxs[i] = ptr_idx << 3;
        keys[i] = (PyTypeObject *)type;
        vals[i] = func;
    }

    ob->table = get_hash_table(keys, vals, idxs, amt);

    if (ob->table == NULL)
        return PyErr_NoMemory();

    return (PyObject *)ob;
}

PyObject *get_utypes_decode_ob(PyObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &data))
        return NULL;

    utypes_decode_ob *ob = PyObject_New(utypes_decode_ob, &utypes_decode_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    // Allocate with calloc to set unused values to NULL
    ob->reads = (PyObject **)calloc(MAX_CUSTOM_TYPES, sizeof(PyObject *));

    if (ob->reads == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }
    
    PyObject *idx;
    PyObject *func;
    Py_ssize_t pos = 0;

    while (PyDict_Next(data, &pos, &idx, &func))
    {
        if (!PyLong_Check(idx))
        {
            PyErr_Format(PyExc_ValueError, "Expected a key of type 'int', got '%s'", Py_TYPE(idx)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        const size_t ptr_idx = PyLong_AsUnsignedLongLong(idx);

        if (!PyCallable_Check(func))
        {
            PyErr_Format(PyExc_ValueError, "Expected values of type 'function' (or other callables), got '%s'", Py_TYPE(func)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        Py_INCREF(func);

        ob->reads[ptr_idx] = func;
    }

    return (PyObject *)ob;
}

/* SERIALIZATION */

extern inline int encode_custom(encode_t *b, PyObject *value)
{
    // Check if we actually got a custom types object as it's optional
    if (b->utypes == NULL)
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return 1;
    }
    
    PyTypeObject *type = Py_TYPE(value);

    size_t idx;
    PyObject *func = pull_from_table(((utypes_encode_ob *)b->utypes)->table, type, &idx);

    if (func == NULL)
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return 1;
    }

    // Call the write function provided by the user and pass the value to encode
    PyObject *result = PyObject_CallFunctionObjArgs(func, value, NULL);

    // See if something went wrong
    if (result == NULL)
        return 1; // Error already set

    // We expect to receive a bytes object back from the user
    if (!PyBytes_Check(result))
    {
        PyErr_Format(PyExc_ValueError, "Expected a 'bytes' object from a custom type write function, got '%s'", Py_TYPE(result)->tp_name);
        Py_DECREF(result);
        return 1;
    }

    char *ptr;
    size_t length;
    PyBytes_AsStringAndSize(result, &ptr, (Py_ssize_t *)(&length));

    b->bufcheck(b, length + 9);

    // Write the extension mask along with the index above it
    *(b->offset++) = DT_EXTND | idx;

    // Separate case if length is zero
    if (length == 0)
    {
        *(b->offset++) = (char)0;
    }
    else
    {
        // Count how many bytes the length takes up
        const size_t num_bytes = USED_BYTES_64(length);

        // Write the number of bytes
        *(b->offset++) = (char)num_bytes;

        // Write the length as little-endian
        const size_t length_little = LITTLE_64(length);
        memcpy(b->offset, &length_little, num_bytes);
        b->offset += num_bytes;

        // Write the actual bytes
        memcpy(b->offset, ptr, length);
        b->offset += length;
    }

    Py_DECREF(result);
    return 0;
}

extern inline PyObject *decode_custom(decode_t *b)
{
    // Custom types object is NULL if we didn't get one, in that case the bytes are invalid
    if (b->utypes == NULL)
    {
        PyErr_SetString(DecodingError, "Likely received an invalid or corrupted bytes object");
        return NULL;
    }
    
    // Custom types take at least 2 bytes
    b->bufcheck(b, 2);
    
    // Get the pointer index we stored earlier to find the function to call
    const size_t ptr_idx = (*(b->offset++) & 0xFF) >> 3;
    PyObject *func = ((utypes_decode_ob *)b->utypes)->reads[ptr_idx];

    // No function was provided for this pointer index
    if (func == NULL)
    {
        PyErr_Format(DecodingError, "Could not find a valid function on ID %zu. Did you use the same custom type ID as when encoding?", ptr_idx);
        return NULL;
    }
    
    // Read how many bytes the length was
    const size_t num_bytes = *(b->offset++) & 0xFF;

    // Retrieve the value length
    size_t length = 0;
    memcpy(&length, b->offset, num_bytes);
    b->offset += num_bytes;

    b->bufcheck(b, length);

    // Create a bytes buffer to pass to the function for decoding
    PyObject *buffer = PyBytes_FromStringAndSize(b->offset, length);
    b->offset += length;

    // Call the user's decode function and pass the buffer of the value
    PyObject *result = PyObject_CallFunctionObjArgs(func, buffer, NULL);
    Py_DECREF(buffer);

    // Would just return NULL if we received NULL
    return result;
}

