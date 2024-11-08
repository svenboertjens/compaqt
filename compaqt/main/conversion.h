#ifndef CONVERSION_H
#define CONVERSION_H

#include "metadata.h"

#define bytes_ln(value) (PyBytes_GET_SIZE(value))
#define bytes_wr(b, value, length) do { \
    const char *ptr = PyBytes_AS_STRING(value); \
    memcpy(b->msg + b->offset, ptr, length); \
    b->offset += length; \
} while (0)
#define bytes_rd(b, value, length) do { \
    value = PyBytes_FromStringAndSize(b->msg + b->offset, (Py_ssize_t)(length)); \
    b->offset += length; \
} while (0)

#define string_ln(value, length) (PyUnicode_AsUTF8AndSize(value, (Py_ssize_t *)&length))
#define string_wr(b, bytes, length) do { \
    memcpy(b->msg + b->offset, bytes, length); \
    b->offset += length; \
} while (0)
#define string_rd(b, value, length) do { \
    value = PyUnicode_DecodeUTF8(b->msg + b->offset, length, NULL); \
    b->offset += length; \
} while (0)

// Use more efficient methods that released in 3.13

#if PY_VERSION_HEX >= 0x030D0000 // Hex code for 3.13.0

    #define integer_wr(b, value, length, success) do { \
        /* Write with max length of 9 instead of 8 to detect overwrites (max write length is 8) */ \
        const size_t num_bytes = PyLong_AsNativeBytes(value, b->msg + b->offset, 9, Py_ASNATIVEBYTES_LITTLE_ENDIAN); \
        b->offset += num_bytes; \
    } while (0)

    #define integer_rd(b, value, length) do { \
        value = PyLong_FromNativeBytes(b->msg + b->offset, length, Py_ASNATIVEBYTES_LITTLE_ENDIAN); \
        b->offset += length; \
    } while (0)

#else

    #define integer_ln(value) ((_PyLong_NumBits(value) + 8) >> 3)
    #define integer_wr(b, value, length) do { \
        _PyLong_AsByteArray((PyLongObject *)value, (unsigned char *)(b->msg + b->offset), length, 1, 1); \
        b->offset += length; \
    } while (0)
    #define integer_rd(b, value, length) do { \
        value = _PyLong_FromByteArray((const unsigned char *)(b->msg + b->offset), length, 1, 1); \
        b->offset += length; \
    } while (0)

#endif

#define float_wr(b, value) do { \
    double num = PyFloat_AS_DOUBLE(value); \
    LITTLE_DOUBLE(num); \
    memcpy(b->msg + b->offset, &num, 8); \
    b->offset += 8; \
} while (0)
#define float_rd(b, value) do { \
    double num = 0; \
    memcpy(&num, b->msg + b->offset, 8); \
    LITTLE_DOUBLE(num); \
    value = PyFloat_FromDouble(num); \
    b->offset += 8; \
} while (0)

#define bool_wr(b, value) (*(b->msg + b->offset++) = DT_BOOLF | (!!(value == Py_True) << 3))

#endif // CONVERSION_H