// This file contains custom datatype handling

#include "metadata.h"
#include "exceptions.h"

typedef struct {
    PyObject_HEAD
    PyTypeObject **types; // The actual types of the values
    PyObject **writes;    // The write functions of the types
} custom_types_wr_ob;

typedef struct {
    PyObject_HEAD
    // No types list is necessary as indexing the read function is done based on metadata in encoded data
    PyObject **reads; // The read functions of the types
} custom_types_rd_ob;

/* OBJECT CREATION */

// Max types is 1 byte: 8 bits - 3 datatype mask bits = 5 bits: 2^5 - 1 = 32
#define MAX_CUSTOM_TYPES 32
#define MAX_TYPE_IDX (MAX_CUSTOM_TYPES - 1)

void custom_types_wr_dealloc(custom_types_wr_ob *self)
{
    // Decref all types and functions stored 
    for (size_t i = 0; i < MAX_TYPE_IDX; ++i)
    {
        // Only if not NULL as not all indexes have to be used
        if (self->types[i] != NULL)
        {
            Py_DECREF(self->types[i]);
            Py_DECREF(self->writes[i]);
        }
    }

    free(self->types);
    free(self->writes);

    if (Py_TYPE(self)->tp_free)
        Py_TYPE(self)->tp_free((PyObject *)self);
    else
        PyObject_Del(self);
}

void custom_types_rd_dealloc(custom_types_rd_ob *self)
{
    for (size_t i = 0; i < MAX_TYPE_IDX; ++i)
        Py_XDECREF(self->reads[i]);

    free(self->reads);

    if (Py_TYPE(self)->tp_free)
        Py_TYPE(self)->tp_free((PyObject *)self);
    else
        PyObject_Del(self);
}

PyTypeObject custom_types_wr_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.CustomWriteTypes",
    .tp_basicsize = sizeof(custom_types_wr_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)custom_types_wr_dealloc,
};

PyTypeObject custom_types_rd_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.CustomReadTypes",
    .tp_basicsize = sizeof(custom_types_rd_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)custom_types_rd_dealloc,
};

PyObject *get_custom_types_wr(PyObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &data))
        return NULL;

    custom_types_wr_ob *ob = PyObject_New(custom_types_wr_ob, &custom_types_wr_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    // Allocate space for the number of total types for the index-based approach
    ob->types = (PyTypeObject **)malloc(sizeof(PyTypeObject *) * MAX_CUSTOM_TYPES);

    if (ob->types == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }

    ob->writes = (PyObject **)malloc(sizeof(PyObject *) * MAX_CUSTOM_TYPES);

    if (ob->writes == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }

    // Initialize all pointers with NULL to keep unused ones as NULL
    memset(ob->types, 0, sizeof(PyTypeObject *) * MAX_CUSTOM_TYPES);
    memset(ob->writes, 0, sizeof(PyObject *) * MAX_CUSTOM_TYPES);
    
    PyObject *idx;
    PyObject *tuple;
    Py_ssize_t pos = 0;

    while (PyDict_Next(data, &pos, &idx, &tuple))
    {
        if (!PyLong_Check(idx))
        {
            PyErr_Format(PyExc_ValueError, "Expected a key of type 'int', got '%s'", Py_TYPE(idx)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        // The index in the array to store the type in
        const size_t ptr_idx = PyLong_AsUnsignedLongLong(idx);

        if (ptr_idx > MAX_TYPE_IDX) {
            PyErr_Format(PyExc_IndexError, "Custom type index out of range: got %zu, max is %zu", ptr_idx, (size_t)(MAX_TYPE_IDX));
            Py_DECREF(ob);
            return NULL;
        }

        if (!PyTuple_Check(tuple) || PyTuple_GET_SIZE(tuple) != 2)
        {
            PyErr_Format(PyExc_ValueError, "Expected values of 'tuple' type of size 2, got '%s' of size %zu", Py_TYPE(tuple)->tp_name, Py_SIZE(tuple));
            Py_DECREF(ob);
            return NULL;
        }

        // Values can be the function and type interchangeably, see which is what and then link them to the `func` and `type`
        PyObject *val1 = PyTuple_GET_ITEM(tuple, 0);
        PyObject *val2 = PyTuple_GET_ITEM(tuple, 1);

        PyObject *func;
        PyObject *type;

        PyTypeObject *type1 = Py_TYPE(val1);
        PyTypeObject *type2 = Py_TYPE(val2);

        if (type1 == &PyFunction_Type && type2 == &PyType_Type)
        {
            func = val1;
            type = val2;
        }
        else if (type2 == &PyFunction_Type && type1 == &PyType_Type)
        {
            func = val2;
            type = val1;
        }
        else
        {
            PyErr_Format(PyExc_ValueError, "Expected a tuple pair with a 'function' and 'type' object, got '%s' and '%s'", type1->tp_name, type2->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        // Make sure the objects stay available by increasing the refcount
        Py_INCREF(func);
        Py_INCREF(type);

        // Assign the write function and type to their respective index
        ob->types[ptr_idx] = ((PyTypeObject *)type);
        ob->writes[ptr_idx] = func;
    }

    return (PyObject *)ob;
}

PyObject *get_custom_types_rd(PyObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &data))
        return NULL;

    custom_types_rd_ob *ob = PyObject_New(custom_types_rd_ob, &custom_types_rd_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    ob->reads = (PyObject **)malloc(sizeof(PyObject *) * MAX_CUSTOM_TYPES);

    if (ob->reads == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }

    memset(ob->reads, 0, sizeof(PyObject *) * MAX_CUSTOM_TYPES);
    
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

int encode_custom(buffer_t *b, PyObject *value, custom_types_wr_ob *ob, buffer_check_t offset_check)
{
    // Check if we actually got a custom types object as it's optional
    if (ob == NULL)
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return 1;
    }
    
    PyTypeObject *type = Py_TYPE(value);

    // Iterate over all types to see if it's in the types list
    for (size_t i = 0; i < MAX_CUSTOM_TYPES; ++i)
    {
        if (ob->types[i] == type)
        {
            // Call the write function provided by the user and pass the value to encode
            PyObject *result = PyObject_CallFunctionObjArgs(ob->writes[i], value, NULL);

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

            offset_check(b, length + MAX_METADATA_SIZE);

            // Write the extension mask along with the index above it
            *(b->msg + b->offset++) = DT_EXTND | (i << 3);

            // Separate case if length is zero
            if (length == 0)
            {
                *(b->msg + b->offset++) = (char)0;
            }
            else
            {
                // Count how many bytes the length takes up
                const size_t num_bytes = USED_BYTES_64(length);

                // Write the number of bytes
                *(b->msg + b->offset++) = (char)num_bytes;

                // Write the length as little-endian
                const size_t length_little = LITTLE_64(length);
                memcpy(b->msg + b->offset, &length_little, num_bytes);
                b->offset += num_bytes;

                // Write the actual bytes
                memcpy(b->msg + b->offset, ptr, length);
                b->offset += length;
            }

            Py_DECREF(result);
            return 0;
        }
    }

    // No match found with any stored custom type
    PyErr_Format(EncodingError, "Received unsupported datatype '%s'", type->tp_name);
    return 1;
}

PyObject *decode_custom(buffer_t *b, custom_types_rd_ob *ob, buffer_check_t overread_check)
{
    // Custom types object is NULL if we didn't get one, in that case the bytes are invalid
    if (ob == NULL)
    {
        PyErr_SetString(DecodingError, "Likely received an invalid or corrupted bytes object");
        return NULL;
    }
    
    // Custom types take at least 2 bytes
    overread_check(b, 2);
    
    // Get the pointer index we stored earlier to find the function to call
    const size_t ptr_idx = (*(b->msg + b->offset++) & 0xFF) >> 3;
    PyObject *func = ob->reads[ptr_idx];

    // No function was provided for this pointer index
    if (func == NULL)
    {
        PyErr_Format(DecodingError, "Could not find a valid function on ID %zu. Did you use the same custom type IDs as when encoding?", ptr_idx);
        return NULL;
    }
    
    // Read how many bytes the length was
    const size_t num_bytes = *(b->msg + b->offset++) & 0xFF;

    // Retrieve the value length
    size_t length = 0;
    memcpy(&length, b->msg + b->offset, num_bytes);
    b->offset += num_bytes;

    overread_check(b, length);

    // Create a bytes buffer to pass to the function for decoding
    PyObject *buffer = PyBytes_FromStringAndSize(b->msg + b->offset, length);
    b->offset += length;

    // Call the user's decode function and pass the buffer of the value
    PyObject *result = PyObject_CallFunctionObjArgs(func, buffer, NULL);
    Py_DECREF(buffer);

    // Would just return NULL if we received NULL
    return result;
}

