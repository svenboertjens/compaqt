// This file contains the main processing functions for serialization

#include <Python.h>
#include "metadata.h"

// The first values of all buffer structs, to accept and use generic structs
typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
} buffer_t;

// Function pointer for buffer checks
typedef int (*buffer_check_t)(buffer_t *, const size_t);

/* ENCODING */

// Check datatype for a container item
#define DT_CHECK(expected) do { \
    if (type != &expected) \
    { \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        return 1; \
    } \
} while (0)

// Macro for calling and testing the offset check function
#define OFFSET_CHECK(length) do { \
    if (offset_check(b, length) == 1) return 1; \
} while (0)

int encode_item(buffer_t *b, PyObject *item, buffer_check_t offset_check)
{
    OFFSET_CHECK(MAX_METADATA_SIZE);

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

            WR_METADATA(b->msg, b->offset, DT_BYTES, length);
            OFFSET_CHECK(length);

            bytes_wr(b, item, length);
        }
        else // Boolean
        {
            bool_wr(b, item);
        }
        break;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        size_t length;
        const char *bytes = string_ln(item, length);

        WR_METADATA(b->msg, b->offset, DT_STRNG, length);
        OFFSET_CHECK(length);

        string_wr(b, bytes, length);
        break;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);

        const size_t length = integer_ln(item);

        WR_METADATA(b->msg, b->offset, DT_INTGR, length);
        OFFSET_CHECK(length);

        integer_wr(b, item, length);
        break;
    }
    case 'f': // Float
    {
        DT_CHECK(PyFloat_Type);

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

        const size_t num_items = (size_t)PyList_GET_SIZE(item);
        WR_METADATA(b->msg, b->offset, DT_ARRAY, num_items);

        for (size_t i = 0; i < num_items; ++i)
            if (encode_item(b, PyList_GET_ITEM(item, i), offset_check) == 1) return 1;
        
        break;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);

        const size_t num_items = PyDict_GET_SIZE(item);
        WR_METADATA(b->msg, b->offset, DT_DICTN, num_items);

        Py_ssize_t pos = 0;
        PyObject *key, *val;
        
        while (PyDict_Next(item, &pos, &key, &val))
            if (encode_item(b, key, offset_check) == 1 || encode_item(b, val, offset_check) == 1) return 1;
        
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

/* DECODING */

// Macro for calling and testing the overread check function
#define OVERREAD_CHECK(length) do { \
    if (overread_check(b, length) == 1) return NULL; \
} while (0)

PyObject *decode_item(buffer_t *b, buffer_check_t overread_check)
{
    const char dt_mask = RD_DTMASK(b);

    switch (dt_mask)
    {
    case DT_BYTES:
    {
        size_t length = 0;
        RD_METADATA(b->msg, b->offset, length);

        OVERREAD_CHECK(length);

        PyObject *value;
        bytes_rd(b, value, length);

        return value;
    }
    case DT_STRNG:
    {
        size_t length = 0;
        RD_METADATA(b->msg, b->offset, length);
        
        OVERREAD_CHECK(length);

        PyObject *value;
        string_rd(b, value, length);

        return value;
    }
    case DT_INTGR:
    {
        size_t length = 0;
        RD_METADATA(b->msg, b->offset, length);
        
        OVERREAD_CHECK(length);

        PyObject *value;
        integer_rd(b, value, length);

        return value;
    }
    case DT_GROUP:
    {
        switch (RD_DTMASK_GROUP(b))
        {
        case DT_FLOAT:
        {
            OVERREAD_CHECK(8);
            PyObject *value;
            float_rd(b, value);
            return value;
        }
        case DT_CMPLX:
        {
            OVERREAD_CHECK(16);
            PyObject *value;
            complex_rd(b, value);
            return value;
        }
        case DT_BOOLT: OVERREAD_CHECK(0); Py_RETURN_TRUE;
        case DT_BOOLF: OVERREAD_CHECK(0); Py_RETURN_FALSE;
        case DT_NONTP: OVERREAD_CHECK(0); Py_RETURN_NONE;
        }
    }
    case DT_ARRAY:
    {
        size_t num_items = 0;
        RD_METADATA(b->msg, b->offset, num_items);
        OVERREAD_CHECK(0);

        PyObject *list = PyList_New(num_items);

        if (list == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }

        // Decode all items and append to the list
        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *item = decode_item(b, overread_check);

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
        RD_METADATA(b->msg, b->offset, num_items);
        OVERREAD_CHECK(0);

        PyObject *dict = PyDict_New();

        if (dict == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }

        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *key = decode_item(b, overread_check);

            if (key == NULL)
            {
                Py_DECREF(dict);
                return NULL;
            }

            PyObject *val = decode_item(b, overread_check);

            if (val == NULL)
            {
                Py_DECREF(dict);
                Py_DECREF(key);
                return NULL;
            }

            PyDict_SetItem(dict, key, val);
        }

        return dict;
    }
    }

    // Invalid input received
    PyErr_SetString(PyExc_ValueError, "Received an invalid or corrupted bytes string");
    return NULL;
}

