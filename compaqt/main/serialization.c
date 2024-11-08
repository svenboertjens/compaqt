// This file contains the main processing functions for serialization

#include "types/custom.h"
#include "metadata.h"

#include "exceptions.h"

/* ENCODING */

// Check datatype for a container item
#define DT_CHECK(expected) \
    if (type != &expected) { break; }

// Macro for calling and testing the offset check function
#define OFFSET_CHECK(length) do { \
    if (offset_check(b, length) == 1) return 1; \
} while (0)

int encode_item(buffer_t *b, PyObject *item, custom_types_wr_ob *custom_ob, buffer_check_t offset_check)
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
            WR_METADATA(b->msg, b->offset, DT_BYTES, length);

            const char *ptr = PyBytes_AS_STRING(item);
            memcpy(b->msg + b->offset, ptr, length);
            b->offset += length;
        }
        else // Boolean
        {
            OFFSET_CHECK(1);
            *(b->msg + b->offset++) = (unsigned char)(5) | (unsigned char)(0 << 3) | (!!(item == ((PyObject*)((&_Py_TrueStruct)))) << 3);
        }
        return 0;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        size_t length;
        const char *bytes = PyUnicode_AsUTF8AndSize(item, (Py_ssize_t *)&length);

        OFFSET_CHECK(MAX_METADATA_SIZE + length);
        WR_METADATA(b->msg, b->offset, DT_STRNG, length);

        memcpy(b->msg + b->offset, bytes, length);
        b->offset += length;

        return 0;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);
        OFFSET_CHECK(9);

        #if (PY_VERSION_HEX >= 0x030D0000)

            const size_t length = PyLong_AsNativeBytes(item, b->msg + b->offset + 1, 9, Py_ASNATIVEBYTES_LITTLE_ENDIAN);

            if (length == 9)
            {
                PyErr_SetString(EncodingError, "Only integers of up to 8 bytes are supported");
                return 1;
            }
            
            *(b->msg + b->offset) = DT_INTGR | (length << 3);
            b->offset += length + 1;

        #else

            const size_t length = (_PyLong_NumBits(item) + 8) >> 3;

            if (length > 8)
            {
                PyErr_SetString(EncodingError, "Only integers of up to 8 bytes are supported");
                return 1;
            }
            
            *(b->msg + b->offset++) = DT_INTGR | (length << 3);

            _PyLong_AsByteArray((PyLongObject *)item, (unsigned char *)(b->msg + b->offset), length, 1, 1);
            b->offset += length;

        #endif

        return 0;
    }
    case 'f': // Float
    {
        DT_CHECK(PyFloat_Type);
        OFFSET_CHECK(9);

        *(b->msg + b->offset++) = DT_FLOAT;

        double num = PyFloat_AS_DOUBLE(item);
        LITTLE_DOUBLE(num);

        memcpy(b->msg + b->offset, &num, 8);
        b->offset += 8;

        return 0;
    }
    case 'N': // NoneType
    {
        if (item != Py_None)
            break;
        
        OFFSET_CHECK(1);
        
        *(b->msg + b->offset++) = DT_NONTP;
        return 0;
    }
    case 'l': // List
    {
        DT_CHECK(PyList_Type);

        const size_t num_items = (size_t)PyList_GET_SIZE(item);

        OFFSET_CHECK(MAX_METADATA_SIZE);
        WR_METADATA(b->msg, b->offset, DT_ARRAY, num_items);

        for (size_t i = 0; i < num_items; ++i)
            if (encode_item(b, PyList_GET_ITEM(item, i), custom_ob, offset_check) == 1) return 1;
        
        return 0;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);

        const size_t num_items = PyDict_GET_SIZE(item);

        OFFSET_CHECK(MAX_METADATA_SIZE);
        WR_METADATA(b->msg, b->offset, DT_DICTN, num_items);

        Py_ssize_t pos = 0;
        PyObject *key, *val;
        
        while (PyDict_Next(item, &pos, &key, &val))
            if (encode_item(b, key, custom_ob, offset_check) == 1 || encode_item(b, val, custom_ob, offset_check) == 1) return 1;
        
        return 0;
    }
    }

    // Check for custom types
    return encode_custom(b, item, custom_ob, offset_check);
}

/* DECODING */

// Macro for calling and testing the overread check function
#define OVERREAD_CHECK(length) do { \
    if (overread_check(b, length) == 1) return NULL; \
} while (0)

// Metadata reading macros for specific metadata modes
#define RD_LN0(msg, offset, length) do { \
    (length) = (*((msg) + (offset)++) & 0xFF) >> 4; \
} while (0)
#define RD_LN1(msg, offset, length) do { \
    (length)  = (*((msg) + (offset)++) & 0xFF) >> 5; \
    (length) |= (*((msg) + (offset)++) & 0xFF) << 3; \
} while (0)
#define RD_LN2(msg, offset, length) do { \
    const int num_bytes = ((*((msg) + (offset)++) & 0b11100000) >> 5) + 1; \
    RD_METADATA_LM2((msg), (offset), (length), num_bytes); \
} while (0)

// Mode 0 metadata cases (`0b10000` case for mode 0 where first length bit is set)
#define MODE0(dt) case (dt): case ((dt) | 0b10000):
// Mode 1 metadata case
#define MODE1(dt) case ((dt) | 0b01000):
// Mode 2 metadata case
#define MODE2(dt) case ((dt) | 0b11000):

#define TYPE_BYTES(RD_LNx) { \
    size_t length; \
    RD_LNx(b->msg, b->offset, length); \
    OVERREAD_CHECK(length); \
    PyObject *value = PyBytes_FromStringAndSize(b->msg + b->offset, (Py_ssize_t)(length)); \
    b->offset += length; \
    return value; \
}

#define TYPE_STRNG(RD_LNx) { \
    size_t length; \
    RD_LNx(b->msg, b->offset, length); \
    OVERREAD_CHECK(length); \
    PyObject *value = PyUnicode_DecodeUTF8(b->msg + b->offset, length, NULL); \
    b->offset += length; \
    return value; \
}

#define TYPE_ARRAY(RD_LNx) { \
    size_t num_items; \
    RD_LNx(b->msg, b->offset, num_items); \
    OVERREAD_CHECK(num_items); \
    PyObject *list = PyList_New(num_items); \
    if (list == NULL) \
    { \
        PyErr_NoMemory(); \
        return NULL; \
    } \
    for (size_t i = 0; i < num_items; ++i) \
    { \
        PyObject *item = decode_item(b, custom_ob, overread_check); \
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
    RD_LNx(b->msg, b->offset, num_items); \
    OVERREAD_CHECK(num_items); \
    PyObject *dict = PyDict_New(); \
    if (dict == NULL) \
    { \
        PyErr_NoMemory(); \
        return NULL; \
    } \
    for (size_t i = 0; i < num_items; ++i) \
    { \
        PyObject *key = decode_item(b, custom_ob, overread_check); \
        if (key == NULL) \
        { \
            Py_DECREF(dict); \
            return NULL; \
        } \
        PyObject *val = decode_item(b, custom_ob, overread_check); \
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

PyObject *decode_item(buffer_t *b, custom_types_rd_ob *custom_ob, buffer_check_t overread_check)
{
    const char byte = *(b->msg + b->offset);
    switch (byte & 0b11111)
    {
    // Static length types, no length metadata
    case DT_FLOAT:
    {
        ++(b->offset);

        OVERREAD_CHECK(8);

        double num = 0;
        memcpy(&num, b->msg + b->offset, 8);

        LITTLE_DOUBLE(num);

        PyObject *value = PyFloat_FromDouble(num);

        b->offset += 8;
        return value;
    }
    case DT_BOOLT: OVERREAD_CHECK(0); ++(b->offset); Py_RETURN_TRUE;
    case DT_BOOLF: OVERREAD_CHECK(0); ++(b->offset); Py_RETURN_FALSE;
    case DT_NONTP: OVERREAD_CHECK(0); ++(b->offset); Py_RETURN_NONE;

    // Integers use their own metadata mechanic
    case DT_INTGR | 0b00000:
    case DT_INTGR | 0b01000:
    case DT_INTGR | 0b10000:
    case DT_INTGR | 0b11000: // Data is stacked on top of the first 3 bits so accept any combination
    {
        const size_t num_bytes = (*(b->msg + b->offset) & 0xFF) >> 3;
        ++(b->offset);

        #if (PY_VERSION_HEX >= 0x030D0000)

            PyObject *value = PyLong_FromNativeBytes(b->msg + b->offset, num_bytes, Py_ASNATIVEBYTES_LITTLE_ENDIAN);

        #else

            PyObject *value = _PyLong_FromByteArray((const unsigned char *)(b->msg + b->offset), num_bytes, 1, 1);

        #endif

        b->offset += num_bytes;
        return value;
    }

    // Dynamic length types, with length metadata
    ANYMODE(DT_BYTES, TYPE_BYTES)
    ANYMODE(DT_STRNG, TYPE_STRNG)
    ANYMODE(DT_ARRAY, TYPE_ARRAY)
    ANYMODE(DT_DICTN, TYPE_DICTN)
    }

    // See if it's a custom type (returns NULL if not)
    return decode_custom(b, custom_ob, overread_check);
}

