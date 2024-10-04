// This file contains the regular serialization methods

#include "regular.h"

#include <Python.h>
#include "globals.h"

// Average size of items
size_t avg_item_size = 32;
// Average size of re-allocations
size_t avg_realloc_size = 512;
// Whether to dymically adjust allocation settings
char dynamic_allocation_tweaks = 1;

/* ENCODING */

// Reallocate the message buffer by a set amount
#define REALLOC_MSG(new_length) do { \
    char *tmp = (char *)realloc(b->msg, new_length); \
    if (tmp == NULL) \
    { \
        PyErr_NoMemory(); \
        return 1; \
    } \
    b->allocated = new_length; \
    b->msg = tmp; \
} while (0)

// Ensure there's enough space allocated to write
#define OFFSET_CHECK(length) do { \
    if (b->offset + length > b->allocated) \
    { \
        REALLOC_MSG(b->offset + length + avg_realloc_size); \
        b->reallocs++; \
    } \
} while (0)

// Check datatype for a container item
#define DT_CHECK(expected) do { \
    if (type != &expected) \
    { \
        Py_DECREF(item); \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        return 1; \
    } \
} while (0)

// Encode an item from a container type
static inline int encode_container_item(buffer_t *b, PyObject *item)
{
    PyTypeObject *type = Py_TYPE(item);
    const char *tp_name = type->tp_name;

    switch (*tp_name)
    {
    case 'b': // Bytes / Boolean
    {
        if (tp_name[1] == 'y') // Bytes
        {
            DT_CHECK(PyBytes_Type);

            const size_t length = bytes_ln(item);

            OFFSET_CHECK(length + MAX_VLE_METADATA_SIZE);
            WR_METADATA_VLE(DT_BYTES, length);

            bytes_wr(b, item, length);
        }
        else // Boolean
        {
            OFFSET_CHECK(1);
            bool_wr(item);
        }
        break;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        const size_t length = string_ln(item);

        OFFSET_CHECK(length + MAX_VLE_METADATA_SIZE);
        WR_METADATA_VLE(DT_STRNG, length);

        string_wr(b, item, length);
        break;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);

        const size_t length = integer_ln(item);

        OFFSET_CHECK(length + MAX_VLE_METADATA_SIZE);
        WR_METADATA_VLE(DT_INTGR, length);

        integer_wr(b, item, length);
        break;
    }
    case 'f': // Float
    {
        DT_CHECK(PyFloat_Type);
        OFFSET_CHECK(9);

        *(b->msg + b->offset++) = DT_FLOAT;

        float_wr(b, item);
        break;
    }
    case 'c': // Complex
    {
        DT_CHECK(PyComplex_Type);
        OFFSET_CHECK(17);

        *(b->msg + b->offset++) = DT_CMPLX;

        complex_wr(b, item);
        break;
    }
    case 'N': // NoneType
    {
        *(b->msg + b->offset++) = DT_NONTP;
        break;
    }
    case 'l': // List
    {
        DT_CHECK(PyList_Type);

        const size_t num_items = (size_t)(PyList_GET_SIZE(item));
        WR_METADATA_VLE(DT_ARRAY, num_items);

        for (size_t i = 0; i < num_items; ++i)
            if (encode_container_item(b, PyList_GET_ITEM(item, i)) == 1) return 1;
        
        break;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);
        WR_METADATA_VLE(DT_DICTN, PyDict_GET_SIZE(item));

        Py_ssize_t pos = 0;
        PyObject *key, *val;
        while (PyDict_Next(item, &pos, &key, &val))
        {
            if (encode_container_item(b, key) == 1) return 1;
            if (encode_container_item(b, val) == 1) return 1;
        }
        break;
    }
    default: // Unsupported datatype
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name);
        return 1;
    }
    }

    return 0;
}

static inline void update_allocation_settings(buffer_t *b, const size_t initial_allocated, const size_t num_items)
{
    if (dynamic_allocation_tweaks == 1)
    {
        if (b->reallocs != 0)
        {
            const size_t difference = b->offset - initial_allocated;
            const size_t med_diff = difference / num_items;

            avg_realloc_size += difference >> 1;
            avg_item_size += med_diff >> 1;
        }
        else
        {
            const size_t difference = initial_allocated - b->offset;
            const size_t med_diff = difference / num_items;
            const size_t diff_small = difference >> 4;
            const size_t med_small = med_diff >> 5;

            if (diff_small + 64 < avg_realloc_size)
                avg_realloc_size -= diff_small;
            else
                avg_realloc_size = 64;

            if (med_small + 4 < avg_item_size)
                avg_item_size -= med_small;
            else
                avg_item_size = 4;
        }
    }
}

// Encode a list type
static inline PyObject *encode_list(buffer_t *b, PyObject *value)
{
    if (!PyList_Check(value))
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return NULL;
    }

    size_t num_items = PyList_GET_SIZE(value);
    if (num_items == 0)
    {
        char buf = DT_ARRAY;
        return PyBytes_FromStringAndSize(&buf, 1);
    }

    const size_t initial_allocated = (avg_item_size * num_items) + avg_realloc_size;
    
    b->msg = (char *)malloc(initial_allocated);
    if (b->msg == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    b->allocated = initial_allocated;

    WR_METADATA_VLE(DT_ARRAY, num_items);

    for (size_t i = 0; i < num_items; ++i)
    {
        if (encode_container_item(b, PyList_GET_ITEM(value, i)) == 1)
        {
            free(b->msg);
            return NULL;
        }
    }
    
    update_allocation_settings(b, initial_allocated, num_items);

    PyObject *result = PyBytes_FromStringAndSize(b->msg, b->offset);

    free(b->msg);
    return result;
}

// Set up encoding basis for a dict type
static inline PyObject *encode_dict(buffer_t *b, PyObject *value)
{
    if (!PyDict_Check(value))
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return NULL;
    }

    size_t num_items = (size_t)(PyDict_GET_SIZE(value));
    if (num_items == 0)
    {
        char buf = DT_DICTN;
        return PyBytes_FromStringAndSize(&buf, 1);
    }

    const size_t initial_allocated = (avg_item_size * (num_items * 2)) + avg_realloc_size;
    
    b->msg = (char *)malloc(initial_allocated);
    if (b->msg == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    b->allocated = initial_allocated;

    WR_METADATA_VLE(DT_DICTN, num_items);

    Py_ssize_t pos = 0;
    PyObject *key, *val;
    while (PyDict_Next(value, &pos, &key, &val))
    {
        if (encode_container_item(b, key) == 1 || encode_container_item(b, val) == 1)
        {
            free(b->msg);
            return NULL;
        }
    }
    
    update_allocation_settings(b, initial_allocated, num_items * 2);

    PyObject *result = PyBytes_FromStringAndSize(b->msg, b->offset);

    free(b->msg);
    return result;
}

// Encode and return a single non-container value
#define ENCODE_SINGLE_VALUE(expected, dt_mask, DT_LN, DT_WR) do { \
    if (type != &expected) \
    { \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        return NULL; \
    } \
    \
    const size_t length = DT_LN(value); \
    \
    b.msg = (char *)malloc(length + 1); \
    if (b.msg == NULL) \
    { \
        PyErr_NoMemory(); \
        return NULL; \
    } \
    \
    b.offset = 1; \
    DT_WR(&b, value, length); \
    *(b.msg) = dt_mask; \
    \
    PyObject *result = PyBytes_FromStringAndSize(b.msg, length + 1); \
    \
    free(b.msg); \
    return result; \
} while (0)

PyObject *encode_regular(PyObject *value)
{
    buffer_t b;
    b.offset = 0;
    b.reallocs = 0;

    PyTypeObject *type = Py_TYPE(value);
    const char *tp_name = type->tp_name;

    switch (*tp_name)
    {
    case 'b': // Bytes / Boolean
    {
        if (tp_name[1] == 'y') // Bytes
            ENCODE_SINGLE_VALUE(PyBytes_Type, DT_BYTES, bytes_ln, bytes_wr);
        else // Boolean
        {
            char buf = DT_BOOLF | (!!(value == Py_True) << 3);
            return PyBytes_FromStringAndSize(&buf, 1);
        }
    }
    case 's': // String
        ENCODE_SINGLE_VALUE(PyUnicode_Type, DT_STRNG, string_ln, string_wr);
    case 'i': // Integer
        ENCODE_SINGLE_VALUE(PyLong_Type, DT_INTGR, integer_ln, integer_wr);
    case 'f': // Float
    {
        if (type != &PyFloat_Type) break;

        char buf[9];
        buf[0] = DT_FLOAT;

        #if (STRICT_ALIGNMENT == 0)

            *(double *)(buf + 1) = PyFloat_AS_DOUBLE(value);

        #else

            const double num = PyFloat_AS_DOUBLE(value);
            memcpy(buf + 1, &num, 8);

        #endif

        return PyBytes_FromStringAndSize(buf, 9);
    }
    case 'c': // Complex
    {
        if (type != &PyComplex_Type) break;

        char buf[17];
        buf[0] = DT_CMPLX;

        #if (STRICT_ALIGNMENT == 0)

            *(Py_complex *)(buf + 1) = PyComplex_AsCComplex(value);

        #else

            const Py_complex complex = PyComplex_AsCComplex(value);
            memcpy(buf + 1, &complex, 16);

        #endif

        return PyBytes_FromStringAndSize(buf, 17);
    }
    case 'N': // NoneType
    {
        if (value != Py_None) break;
        char buf = DT_NONTP;
        return PyBytes_FromStringAndSize(&buf, 1);
    }
    case 'l': // List
        return encode_list(&b, value);
    case 'd': // Dict
        return encode_dict(&b, value);
    }

    PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name);
    return NULL;
}

/* DECODING */

// Set an error stating we received an invalid input
#define SET_INVALID_ERR() (PyErr_SetString(PyExc_ValueError, "Received an invalid or corrupted bytes string"))

// Check whether we aren't overreading the buffer (means invalid input)
#define OVERREAD_CHECK(length) do { \
    if (b->offset + length > b->allocated) \
    { \
        SET_INVALID_ERR(); \
        return NULL; \
    } \
} while (0)

// Decode a single item
#define DECODE_ITEM(rd_func) do { \
    size_t length = 0; \
    RD_METADATA_VLE(length); \
    OVERREAD_CHECK(length); \
    return rd_func(b, length); \
} while (0)

// Decode an item inside a container
static PyObject *decode_container_item(buffer_t *b)
{
    const char dt_mask = RD_DTMASK();

    switch (dt_mask)
    {
    case DT_BYTES: DECODE_ITEM(bytes_rd);
    case DT_STRNG: DECODE_ITEM(string_rd);
    case DT_INTGR: DECODE_ITEM(integer_rd);
    case DT_GROUP:
    {
        const char act_mask = RD_DTMASK_GROUP();
        OVERREAD_CHECK(0);

        switch (act_mask)
        {
        case DT_FLOAT: OVERREAD_CHECK(8);  return float_rd(b);
        case DT_CMPLX: OVERREAD_CHECK(16); return complex_rd(b);
        case DT_BOOLT: Py_RETURN_TRUE;
        case DT_BOOLF: Py_RETURN_FALSE;
        case DT_NONTP: Py_RETURN_NONE;
        }
    }
    case DT_ARRAY:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);
        OVERREAD_CHECK(0);

        PyObject *list = PyList_New(num_items);

        if (list == NULL)
        {
            PyErr_Format(PyExc_RuntimeError, "Failed to create a list object of size '%zu'", num_items);
            return NULL;
        }

        // Decode all items and append to the list
        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *item = decode_container_item(b);

            if (item == NULL)
            {
                Py_DECREF(list);
                return NULL;
            }

            PyList_SET_ITEM(list, i, item);
        }

        return list;
    }
    case DT_DICTN:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);
        OVERREAD_CHECK(0);

        PyObject *dict = PyDict_New();

        if (dict == NULL)
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create a dict object");
            return NULL;
        }

        // Go over the dict and decode all items, place them into the dict
        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *key, *val;

            if ((key = decode_container_item(b)) == NULL || (val = decode_container_item(b)) == NULL)
            {
                Py_DECREF(dict);
                return NULL;
            }

            PyDict_SetItem(dict, key, val);
        }

        return dict;
    }
    }

    // Invalid input received
    SET_INVALID_ERR();
    return NULL;
}

PyObject *decode_regular(PyObject *value)
{
    buffer_t b_local;
    buffer_t *b = &b_local;

    PyBytes_AsStringAndSize(value, &b->msg, (Py_ssize_t *)(&b->allocated));

    if (b->allocated == 0)
    {
        SET_INVALID_ERR();
        return NULL;
    }

    switch (*(b->msg) & 0b00000111)
    {
    case DT_BYTES: b->offset = 1; return bytes_rd(b, b->allocated - 1);
    case DT_STRNG: b->offset = 1; return string_rd(b, b->allocated - 1);
    case DT_INTGR: b->offset = 1; return integer_rd(b, b->allocated - 1);
    case DT_GROUP:
    {
        b->offset = 1;

        switch (*(b->msg))
        {
        case DT_FLOAT: OVERREAD_CHECK(8);  return float_rd(b);
        case DT_CMPLX: OVERREAD_CHECK(16); return complex_rd(b);
        case DT_BOOLT: Py_RETURN_TRUE;
        case DT_BOOLF: Py_RETURN_FALSE;
        case DT_NONTP: Py_RETURN_NONE;
        default: SET_INVALID_ERR(); return NULL;
        }
    }
    case DT_ARRAY:
    {
        b->offset = 0;

        size_t num_items = 0;
        RD_METADATA_VLE(num_items);
        OVERREAD_CHECK(0); // Check whether reading the metadata didn't exceed the allocation size

        PyObject *list = PyList_New(num_items);

        if (list == NULL)
        {
            PyErr_Format(PyExc_RuntimeError, "Failed to create a list object of size '%zu'", num_items);
            return NULL;
        }

        PyObject *item;
        for (size_t i = 0; i < num_items; ++i)
        {
            if ((item = decode_container_item(b)) == NULL)
            {
                Py_DECREF(list);
                return NULL;
            }

            PyList_SET_ITEM(list, i, item);
        }

        return list;
    }
    case DT_DICTN:
    {
        b->offset = 0;

        size_t num_items = 0;
        RD_METADATA_VLE(num_items);
        OVERREAD_CHECK(0);

        PyObject *dict = PyDict_New();

        if (dict == NULL)
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create a dict object");
            return NULL;
        }

        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *key, *val;

            if ((key = decode_container_item(b)) == NULL || (val = decode_container_item(b)) == NULL)
            {
                Py_DECREF(dict);
                return NULL;
            }

            PyDict_SetItem(dict, key, val);
        }

        return dict;
    }
    }

    // Invalid input
    SET_INVALID_ERR();
    return NULL;
}

