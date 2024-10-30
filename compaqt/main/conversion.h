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

#define integer_ln(value) ((_PyLong_NumBits(value) + 8) >> 3)
#define integer_wr(b, value, length) do { \
    _PyLong_AsByteArray((PyLongObject *)value, (unsigned char *)(b->msg + b->offset), length, 1, 1); \
    b->offset += length; \
} while (0)
#define integer_rd(b, value, length) do { \
    value = _PyLong_FromByteArray((const unsigned char *)(b->msg + b->offset), length, 1, 1); \
    b->offset += length; \
} while (0)

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

#define complex_wr(b, value) do { \
    Py_complex complex = PyComplex_AsCComplex(value); \
    LITTLE_DOUBLE(complex.real); \
    LITTLE_DOUBLE(complex.imag); \
    memcpy(b->msg + b->offset, &complex, sizeof(Py_complex)); \
    b->offset += sizeof(Py_complex); \
} while (0)
#define complex_rd(b, value) do { \
    Py_complex complex; \
    memcpy(&complex, b->msg + b->offset, sizeof(Py_complex)); \
    LITTLE_DOUBLE(complex.real); \
    LITTLE_DOUBLE(complex.imag); \
    value = PyComplex_FromCComplex(complex); \
    b->offset += sizeof(Py_complex); \
} while (0)

#define bool_wr(b, value) (*(b->msg + b->offset++) = DT_BOOLF | (!!(value == Py_True) << 3))

#endif // CONVERSION_H