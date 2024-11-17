// This file contains the main processing functions for serialization

#include "types/usertypes.h"
#include "types/base.h"
#include "types/cbytes.h"
#include "types/cstr.h"

#include "globals/exceptions.h"
#include "globals/typemasks.h"
#include "globals/buftricks.h"
#include "globals/metadata.h"
#include "globals/typedefs.h"

#define MAX_METADATA_SIZE 9

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
            WR_METADATA(b->offset, DT_BYTES, length);

            const char *ptr = PyBytes_AS_STRING(item);
            memcpy(b->offset, ptr, length);
            b->offset += length;
        }
        else // Boolean
        {
            OFFSET_CHECK(1);
            *BUF_POST_INC = (unsigned char)(5) | (unsigned char)(0 << 3) | (!!(item == ((PyObject*)((&_Py_TrueStruct)))) << 3);
        }

        return 0;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        size_t length;
        const char *bytes = PyUnicode_AsUTF8AndSize(item, (Py_ssize_t *)&length);

        OFFSET_CHECK(MAX_METADATA_SIZE + length);
        WR_METADATA(b->offset, DT_STRNG, length);

        memcpy(b->offset, bytes, length);
        b->offset += length;

        return 0;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);
        OFFSET_CHECK(9);

        #if (PY_VERSION_HEX >= 0x030D0000)

            const size_t length = PyLong_AsNativeBytes(item, BUF_GET_OFFSET + 1, 9, Py_ASNATIVEBYTES_LITTLE_ENDIAN);

            if (length == 9)
            {
                PyErr_SetString(EncodingError, "Only integers of up to 8 bytes are supported");
                return 1;
            }
            
            *(b->offset) = DT_INTGR | (length << 3);
            b->offset += length + 1;

        #else

            const size_t length = (_PyLong_NumBits(item) + 8) >> 3;

            if (length > 8)
            {
                PyErr_SetString(EncodingError, "Only integers of up to 8 bytes are supported");
                return 1;
            }
            
            *BUF_POST_INC = DT_INTGR | (length << 3);

            _PyLong_AsByteArray((PyLongObject *)item, (unsigned char *)(b->offset), length, 1, 1);
            b->offset += length;

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

        const size_t num_items = (size_t)PyList_GET_SIZE(item);

        OFFSET_CHECK(MAX_METADATA_SIZE);
        WR_METADATA(b->offset, DT_ARRAY, num_items);

        for (size_t i = 0; i < num_items; ++i)
            if (encode_object(b, PyList_GET_ITEM(item, i)) == 1) return 1;
        
        return 0;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);

        const size_t num_items = PyDict_GET_SIZE(item);

        OFFSET_CHECK(MAX_METADATA_SIZE);
        WR_METADATA(b->offset, DT_DICTN, num_items);

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
    size_t num_items; \
    RD_LNx(num_items); \
    OVERREAD_CHECK(num_items); \
    PyObject *list = PyList_New(num_items); \
    \
    if (list == NULL) \
        return PyErr_NoMemory(); \
    \
    for (size_t i = 0; i < num_items; ++i) \
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
    size_t num_items; \
    RD_LNx(num_items); \
    PyObject *dict = PyDict_New(); \
    \
    if (dict == NULL) \
        return PyErr_NoMemory(); \
    \
    for (size_t i = 0; i < num_items; ++i) \
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

    // Integers use their own metadata mechanic
    case DT_INTGR | 0b00000:
    case DT_INTGR | 0b01000:
    case DT_INTGR | 0b10000:
    case DT_INTGR | 0b11000: // Data is stacked on top of the first 3 bits so accept all top 2 bit variations
    {
        const size_t num_bytes = (*BUF_POST_INC & 0xFF) >> 3;

        OVERREAD_CHECK(num_bytes);

        #if (PY_VERSION_HEX >= 0x030D0000)

            PyObject *value = PyLong_FromNativeBytes(b->offset, num_bytes, Py_ASNATIVEBYTES_LITTLE_ENDIAN);

        #else

            PyObject *value = _PyLong_FromByteArray((const unsigned char *)(b->offset), num_bytes, 1, 1);

        #endif

        OVERREAD_CHECK(num_bytes);

        b->offset += num_bytes;

        return value;
    }

    // Dynamic length types, with length metadata
    ANYMODE(DT_BYTES, TYPE_BYTES)
    ANYMODE(DT_STRNG, TYPE_STRNG)
    ANYMODE(DT_ARRAY, TYPE_ARRAY)
    ANYMODE(DT_DICTN, TYPE_DICTN)
    }

    // Try as a custom type
    PyObject *custom_result = decode_custom(b);

    return custom_result;
}

