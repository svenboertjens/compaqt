// This file contains the main processing functions for serialization

#include "types/usertypes.h"
#include "types/base.h"
#include "types/cbytes.h"
#include "types/cstr.h"

#include "globals/exceptions.h"
#include "globals/typemasks.h"
#include "globals/buftricks.h"
#include "globals/typedefs.h"

/* ENCODING */

// Check datatype for a container item
#define DT_CHECK(expected) \
    if (type != &expected) { break; }

// Macro for calling and testing the offset check function
#define OFFSET_CHECK(length) do { \
    if (b->bufcheck(b, length) == 1) return 1; \
} while (0)

int encode_object(encode_t *b, PyObject *item)
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

            const size_t length = PyBytes_GET_SIZE(item);

            OFFSET_CHECK(MAX_METADATA_SIZE + length);
            METADATA_VARLEN_WR(DT_BYTES, length);

            const char *ptr = PyBytes_AS_STRING(item);
            memcpy(b->offset, ptr, length);
            b->offset += length;
        }
        else // Boolean
        {
            OFFSET_CHECK(1);
            BOOLEAN_WR(item);
        }

        return 0;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        size_t length;
        const char *bytes = PyUnicode_AsUTF8AndSize(item, (Py_ssize_t *)&length);

        OFFSET_CHECK(MAX_METADATA_SIZE + length);
        METADATA_VARLEN_WR(DT_STRNG, length);

        memcpy(b->offset, bytes, length);
        b->offset += length;

        return 0;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);
        OFFSET_CHECK(9);

        #if (PY_VERSION_HEX >= 0x030D0000)

            const size_t nbytes = PyLong_AsNativeBytes(item, b->offset + 1, 9, Py_ASNATIVEBYTES_LITTLE_ENDIAN);

            if (nbytes == 9)
            {
                PyErr_SetString(EncodingError, "Only integers of up to 8 bytes are supported");
                return 1;
            }

            METADATA_INTEGER_WR(nbytes);
            b->offset += nbytes;

        #else

            const size_t nbytes = (_PyLong_NumBits(item) + 8) >> 3;

            if (nbytes > 8)
            {
                PyErr_SetString(EncodingError, "Only integers of up to 8 bytes are supported");
                return 1;
            }

            METADATA_INTEGER_WR(nbytes);

            _PyLong_AsByteArray((PyLongObject *)item, (unsigned char *)(b->offset), nbytes, 1, 1);
            b->offset += nbytes;

        #endif

        return 0;
    }
    case 'f': // Float
    {
        DT_CHECK(PyFloat_Type);
        OFFSET_CHECK(9);

        b->offset[0] = DT_FLOAT;
        BUF_PRE_INC;

        double num = PyFloat_AS_DOUBLE(item);
        LITTLE_DOUBLE(num);

        memcpy(b->offset, &num, 8);
        b->offset += 8;

        return 0;
    }
    case 'N': // NoneType
    {
        if (item != Py_None)
            break;
        
        OFFSET_CHECK(1);
        
        *BUF_POST_INC = DT_NONTP;
        return 0;
    }
    case 'l': // List
    {
        DT_CHECK(PyList_Type);

        const size_t nitems = (size_t)PyList_GET_SIZE(item);

        OFFSET_CHECK(MAX_METADATA_SIZE);
        METADATA_VARLEN_WR(DT_ARRAY, nitems);

        for (size_t i = 0; i < nitems; ++i)
            if (encode_object(b, PyList_GET_ITEM(item, i)) == 1) return 1;
        
        return 0;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);

        const size_t nitems = PyDict_GET_SIZE(item);

        OFFSET_CHECK(MAX_METADATA_SIZE);
        METADATA_VARLEN_WR(DT_DICTN, nitems);

        Py_ssize_t pos = 0;
        PyObject *key, *val;
        
        while (PyDict_Next(item, &pos, &key, &val))
            if (encode_object(b, key) == 1 || encode_object(b, val) == 1) return 1;
        
        return 0;
    }
    }

    // Check for custom types
    return encode_custom(b, item);
}

/* DECODING */

// Macro for calling and testing the overread check function
#define OVERREAD_CHECK(length) do { \
    if (b->bufcheck(b, length) == 1) return NULL; \
} while (0)

// Metadata reading macros for specific metadata modes
#define RD_LN0(length) do { \
    (length) = (byte & 0xFF) >> 4; \
    BUF_PRE_INC; \
} while (0)
#define RD_LN1(length) do { \
    (length)  = (*BUF_POST_INC & 0xFF) >> 5; \
    (length) |= (*BUF_POST_INC & 0xFF) << 3; \
} while (0)
#define RD_LN2(length) do { \
    const int num_bytes = ((*BUF_POST_INC & 0b11100000) >> 5) + 1; \
    RD_METADATA_LM2(b->offset, length, num_bytes); \
} while (0)

// Mode 0 metadata cases (`0b10000` case for mode 0 where first length bit is set)
#define MODE0(dt) case (dt): case ((dt) | 0b10000):
// Mode 1 metadata case
#define MODE1(dt) case ((dt) | 0b01000):
// Mode 2 metadata case
#define MODE2(dt) case ((dt) | 0b11000):

#define TYPE_BYTES(RD_LNx) { \
    size_t length; \
    RD_LNx(length); \
    OVERREAD_CHECK(length); \
    \
    PyObject *value; \
    if (b->bufd != NULL) \
        value = cbytes_create(b->bufd, b->offset, length); \
    else \
        value = PyBytes_FromStringAndSize(b->offset, (Py_ssize_t)(length)); \
    \
    b->offset += length; \
    return value; \
}

#define TYPE_STRNG(RD_LNx) { \
    size_t length; \
    RD_LNx(length); \
    OVERREAD_CHECK(length); \
    PyObject *value; \
    \
    if (b->bufd != NULL) \
        value = cstr_create(b->bufd, b->offset, length); \
    else \
        value = PyUnicode_DecodeUTF8(b->offset, length, "strict"); \
    \
    b->offset += length; \
    return value; \
}

#define TYPE_ARRAY(RD_LNx) { \
    size_t nitems; \
    RD_LNx(nitems); \
    OVERREAD_CHECK(nitems); \
    PyObject *list = PyList_New(nitems); \
    \
    if (list == NULL) \
        return PyErr_NoMemory(); \
    \
    for (size_t i = 0; i < nitems; ++i) \
    { \
        PyObject *item = decode_bytes(b); \
        if (item == NULL) \
        { \
            Py_DECREF(list); \
            return NULL; \
        } \
        PyList_SET_ITEM(list, i, item); \
    } \
    return list; \
}

#define TYPE_DICTN(RD_LNx) { \
    size_t nitems; \
    RD_LNx(nitems); \
    PyObject *dict = PyDict_New(); \
    \
    if (dict == NULL) \
        return PyErr_NoMemory(); \
    \
    for (size_t i = 0; i < nitems; ++i) \
    { \
        PyObject *key = decode_bytes(b); \
        if (key == NULL) \
        { \
            Py_DECREF(dict); \
            return NULL; \
        } \
        PyObject *val = decode_bytes(b); \
        if (val == NULL) \
        { \
            Py_DECREF(dict); \
            Py_DECREF(key); \
            return NULL; \
        } \
        PyDict_SetItem(dict, key, val); \
        Py_DECREF(key); \
        Py_DECREF(val); \
    } \
    return dict; \
}

#define ANYMODE(dt, TYPE_x) MODE0(dt) TYPE_x(RD_LN0) MODE1(dt) TYPE_x(RD_LN1) MODE2(dt) TYPE_x(RD_LN2) 

PyObject *decode_bytes(decode_t *b)
{
    const char byte = *b->offset;
    switch (byte & 0b11111)
    {
    // Static length types, no length metadata
    case DT_FLOAT:
    {
        BUF_PRE_INC;

        OVERREAD_CHECK(8);

        double num;
        memcpy(&num, b->offset, 8);

        LITTLE_DOUBLE(num);

        b->offset += 8;

        return PyFloat_FromDouble(num);
    }
    case DT_BOOLT:
    {
        BUF_PRE_INC;
        OVERREAD_CHECK(0);
        Py_RETURN_TRUE;
    }
    case DT_BOOLF:
    {
        BUF_PRE_INC;
        OVERREAD_CHECK(0);
        Py_RETURN_FALSE;
    }
    case DT_NONTP:
    {
        BUF_PRE_INC;
        OVERREAD_CHECK(0);
        Py_RETURN_NONE;
    }

    CASES_AS_5BIT(DT_INTGR)
    {
        size_t nbytes;
        METADATA_INTEGER_RD(nbytes);
        OVERREAD_CHECK(nbytes);

        /* Use the efficient memcpy to get the integer value */
        //int64_t num;
        //__MEMCPY_READ(num, nbytes);

        //num = LITTLE_64(num);

        #if (PY_VERSION_HEX >= 0x030D0000)

            PyObject *value = PyLong_FromNativeBytes(b->offset, nbytes, Py_ASNATIVEBYTES_LITTLE_ENDIAN);

        #else

            PyObject *value = _PyLong_FromByteArray((const unsigned char *)(b->offset), nbytes, 1, 1);

        #endif

        b->offset += nbytes;

        return value;
    }

    VARLEN_READ_CASES(DT_BYTES,
    {
        OVERREAD_CHECK(length);
        
        PyObject *value;
        if (b->bufd != NULL)
            value = cbytes_create(b->bufd, b->offset, length);
        else
            value = PyBytes_FromStringAndSize(b->offset, (Py_ssize_t)(length));
        
        b->offset += length;
        return value;
    })

    VARLEN_READ_CASES(DT_STRNG,
    {
        OVERREAD_CHECK(length);

        PyObject *value;
        if (b->bufd != NULL)
            value = cstr_create(b->bufd, b->offset, length);
        else
            value = PyUnicode_DecodeUTF8(b->offset, length, "strict");
        
        b->offset += length;
        return value;
    })

    VARLEN_READ_CASES(DT_ARRAY,
    {
        OVERREAD_CHECK(0);
        
        PyObject *list = PyList_New(length);
    
        if (list == NULL)
            return PyErr_NoMemory();
    
        for (size_t i = 0; i < length; ++i)
        {
            PyObject *item = decode_bytes(b);
            if (item == NULL)
            {
                Py_DECREF(list);
                return NULL;
            }
            PyList_SET_ITEM(list, i, item);
        }
        return list;
    })

    VARLEN_READ_CASES(DT_DICTN, {
        OVERREAD_CHECK(0);

        PyObject *dict = PyDict_New();
    
        if (dict == NULL)
            return PyErr_NoMemory();
    
        for (size_t i = 0; i < length; ++i)
        {
            PyObject *key = decode_bytes(b);
            if (key == NULL)
            {
                Py_DECREF(dict);
                return NULL;
            }

            PyObject *val = decode_bytes(b);
            if (val == NULL)
            {
                Py_DECREF(dict);
                Py_DECREF(key);
                return NULL;
            }

            PyDict_SetItem(dict, key, val);

            Py_DECREF(key);
            Py_DECREF(val);
        }
        
        return dict;
    })
    }

    // Try as a custom type
    return decode_custom(b);
}

#define TYPE_DICTN(RD_LNx) { \
    size_t nitems; \
    RD_LNx(nitems); \
    PyObject *dict = PyDict_New(); \
    \
    if (dict == NULL) \
        return PyErr_NoMemory(); \
    \
    for (size_t i = 0; i < nitems; ++i) \
    { \
        PyObject *key = decode_bytes(b); \
        if (key == NULL) \
        { \
            Py_DECREF(dict); \
            return NULL; \
        } \
        PyObject *val = decode_bytes(b); \
        if (val == NULL) \
        { \
            Py_DECREF(dict); \
            Py_DECREF(key); \
            return NULL; \
        } \
        PyDict_SetItem(dict, key, val); \
        Py_DECREF(key); \
        Py_DECREF(val); \
    } \
    return dict; \
}

