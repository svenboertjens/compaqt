#ifndef GLOBALS_H
#define GLOBALS_H

// This file contains methods used in multiple files

#include <Python.h>

// These macros will be overwritten by the setup.py when needed
#define IS_LITTLE_ENDIAN 1 // Endianness to follow
#define STRICT_ALIGNMENT 0 // Whether to use strict alignment

/* ALLOC SETTINGS */

// Average size of items
extern size_t avg_item_size;
// Average size of re-allocations
extern size_t avg_realloc_size;
// Whether to dymically adjust allocation settings
extern char dynamic_allocation_tweaks;

// Struct to hold serialization data
typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
    int reallocs;
} buffer_t;

static inline void update_allocation_settings(const int reallocs, const size_t offset, const size_t initial_allocated, const size_t num_items)
{
    if (dynamic_allocation_tweaks == 1)
    {
        if (reallocs != 0)
        {
            const size_t difference = offset - initial_allocated;
            const size_t med_diff = difference / num_items;

            avg_realloc_size += difference >> 1;
            avg_item_size += med_diff >> 1;
        }
        else
        {
            const size_t difference = initial_allocated - offset;
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

/* METADATA */

// FOR METADATA DOCUMENTATION, SEE "docs/metadata.md"


// The maximum bytes VLE metadata will take up
#define MAX_VLE_METADATA_SIZE 9

// Write Variable Length Encoding (VLE) metadata
#define WR_METADATA_VLE(b, dt_mask, length) do { \
    const int mode = !!(length >> 4) + !!(length >> 11); \
    switch (mode) \
    { \
    case 0: *(b->msg + b->offset++) = dt_mask | (unsigned char)(length << 4); break; \
    case 1: \
    { \
        *(b->msg + b->offset++) = dt_mask | 0b00001000 | (unsigned char)(length << 5); \
        *(b->msg + b->offset++) = (unsigned char)(length >> 3); \
        break; \
    } \
    case 2: \
    { \
        const int num_bytes = !!(length >> (8 * 2)) + !!(length >> (8 * 3)) + !!(length >> (8 * 4)) + !!(length >> (8 * 5)) + !!(length >> (8 * 6)) + !!(length >> (8 * 7)); \
        \
        *(b->msg + b->offset++) = dt_mask | 0b00011000 | (num_bytes << 5); \
        *(b->msg + b->offset++) = length & 0xFF; \
        *(b->msg + b->offset++) = (length & 0xFF00) >> 8; \
        \
        switch (num_bytes) \
        { \
        case 6: *(b->msg + b->offset++) = (length >> (8 * 7)); \
        case 5: *(b->msg + b->offset++) = (length >> (8 * 6)); \
        case 4: *(b->msg + b->offset++) = (length >> (8 * 5)); \
        case 3: *(b->msg + b->offset++) = (length >> (8 * 4)); \
        case 2: *(b->msg + b->offset++) = (length >> (8 * 3)); \
        case 1: *(b->msg + b->offset++) = (length >> (8 * 2)); \
        } \
        break; \
    } \
    } \
} while (0)

// Read VLE metadata
#define RD_METADATA_VLE(b, length) do { \
    const int mode = *(b->msg + b->offset) & 0b00011000; \
    \
    switch (mode) \
    { \
    case 0b00010000: /* Extra case for if the first length bit is set, but not the first mode bit */\
    case 0b00000000: length = (*(b->msg + b->offset++) & 0b11110000) >> 4; break; \
    case 0b00001000: \
    { \
        length = (*(b->msg + b->offset++) & 0b11100000) >> 5; \
        length |= (*(b->msg + b->offset++) & 0xFF) << 3; \
        break; \
    } \
    case 0b00011000: \
    { \
        const int num_bytes = (*(b->msg + b->offset++) & 0b11100000) >> 5; \
        \
        length = (size_t)(*(b->msg + b->offset++) & 0xFF); \
        length |= (size_t)((*(b->msg + b->offset++) & 0xFF) << 8); \
        switch (num_bytes) \
        { \
        case 6: length |= (size_t)(*(b->msg + b->offset++) & 0xFF) << (8 * 7); \
        case 5: length |= (size_t)(*(b->msg + b->offset++) & 0xFF) << (8 * 6); \
        case 4: length |= (size_t)(*(b->msg + b->offset++) & 0xFF) << (8 * 5); \
        case 3: length |= (size_t)(*(b->msg + b->offset++) & 0xFF) << (8 * 4); \
        case 2: length |= (size_t)(*(b->msg + b->offset++) & 0xFF) << (8 * 3); \
        case 1: length |= (size_t)(*(b->msg + b->offset++) & 0xFF) << (8 * 2); \
        } \
        break; \
    } \
    } \
} while (0)

// Read a datamask (stored in first 3 bits)
#define RD_DTMASK(b) (*(b->msg + b->offset) & 0b00000111)
// Read a group datamask (takes up entire byte)
#define RD_DTMASK_GROUP(b) (*(b->msg + b->offset++))

/* ENDIANNESS TRANSFORMATIONS */

#if (IS_LITTLE_ENDIAN == 1)

    #define LL_LITTLE_ENDIAN(value) (value) // Long to little-endian
    #define DB_LITTLE_ENDIAN(value) (value) // Double to little-endian

#else

     // Long to little-endian
    static inline long long LL_LITTLE_ENDIAN(long long value)
    {
        long long result = 0;

        if (sizeof(long long) == 4)
        {
            result |= (value & 0xFF000000) >> 24;
            result |= (value & 0x00FF0000) >> 8;
            result |= (value & 0x0000FF00) << 8;
            result |= (value & 0x000000FF) << 24;
        }
        else
        {
            result |= (value & 0xFF00000000000000LL) >> 56;
            result |= (value & 0x00FF000000000000LL) >> 40;
            result |= (value & 0x0000FF0000000000LL) >> 24;
            result |= (value & 0x000000FF00000000LL) >> 8;
            result |= (value & 0x00000000FF000000LL) << 8;
            result |= (value & 0x0000000000FF0000LL) << 24;
            result |= (value & 0x000000000000FF00LL) << 40;
            result |= (value & 0x00000000000000FFLL) << 56;
        }

        return result;
    }

    // Double to little-endian
    static inline double DB_LITTLE_ENDIAN(double value)
    {
        union {
            double d;
            uint8_t bytes[sizeof(double)];
        } val, result;

        val.d = value;

        for (size_t i = 0; i < sizeof(double); i++) {
            result.bytes[i] = val.bytes[sizeof(double) - 1 - i];
        }

        return result.d;
    }

    /* MAKE BIG-ENDIAN VERSION */

#endif

/* SUPPORTED DATATYPES */

#define DT_ARRAY (unsigned char)(0) // Arrays
#define DT_DICTN (unsigned char)(1) // Dicts
#define DT_BYTES (unsigned char)(2) // Binary
#define DT_STRNG (unsigned char)(3) // Strings
#define DT_INTGR (unsigned char)(4) // Integers

// Group datatypes stores datatypes not requiring length metadata (thus can take up whole byte)
#define DT_GROUP (unsigned char)(5) // Group datatype mask for identifying them
#define DT_BOOLF DT_GROUP | (unsigned char)(0 << 3) // False Booleans
#define DT_BOOLT DT_GROUP | (unsigned char)(1 << 3) // True Booleans
#define DT_FLOAT DT_GROUP | (unsigned char)(2 << 3) // Floats
#define DT_NONTP DT_GROUP | (unsigned char)(3 << 3) // NoneTypes
#define DT_CMPLX DT_GROUP | (unsigned char)(4 << 3) // Complexes

#define DT_NOUSE (unsigned char)(6) // Not yet used for anything, suggestions?
#define DT_EXTND (unsigned char)(7) // Custom datatypes

/* DATATYPE CONVERSION */

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

#define string_ln(value) (PyUnicode_GET_LENGTH(value) * PyUnicode_KIND(value))
#define string_wr(b, value, length) do { \
    memcpy(b->msg + b->offset, PyUnicode_DATA(value), length); \
    b->offset += length; \
} while (0)
#define string_rd(b, value, length) do { \
    value = PyUnicode_FromStringAndSize(b->msg + b->offset, length); \
    b->offset += length; \
} while (0)

#define integer_ln(value) ((_PyLong_NumBits(value) + 8) >> 3)
#if (STRICT_ALIGNMENT == 0)

    #define integer_wr(b, value, length) do { \
        *(long long *)(b->msg + b->offset) = LL_LITTLE_ENDIAN(PyLong_AsLongLong(value)); \
        b->offset += length; \
    } while (0)
    #define integer_rd(b, value, length) do { \
        value = PyLong_FromLongLong(*(long long *)(b->msg + b->offset)); \
        b->offset += length; \
    } while (0)

    #define float_wr(b, value) do { \
        *(double *)(b->msg + b->offset) = DB_LITTLE_ENDIAN(PyFloat_AS_DOUBLE(value)); \
        b->offset += 8; \
    } while (0)
    #define float_rd(b, value) do { \
        value = PyFloat_FromDouble(*(double *)(b->msg + b->offset)); \
        b->offset = 8; \
    } while (0)

    #define complex_wr(b, value) do { \
        *(Py_complex *)(b->msg + b->offset) = PyComplex_AsCComplex(value); \
        b->offset += sizeof(Py_complex); \
    } while (0)
    #define complex_rd(b, value) do { \
        value = PyComplex_FromCComplex(*(Py_complex *)(b->msg + b->offset)); \
        b->offset += sizeof(Py_complex); \
    } while (0)

#else

    #define integer_wr(b, value, length) do { \
        const long long num = LL_LITTLE_ENDIAN(PyLong_AsLongLong(value)); \
            memcpy(msg + offset, &num, length); \
        b->offset += length; \
    } while (0)
    #define integer_rd(b, value, length) do { \
        const long long num;
        memcpy(&num, b->msg + b->offset, sizeof(long long)); \
        value = PyLong_FromLongLong(num); \
        b->offset += length; \
    } while (0)

    #define float_wr(b, value) do { \
        double num = DB_LITTLE_ENDIAN(PyFloat_AS_DOUBLE(value)); \
        memcpy(b->msg + b->offset, &num, 8); \
        b->offset += 8; \
    } while (0)
    #define float_rd(b, value) do { \
        double num; \
        memcpy(&num, b->msg + b->offset, 8); \
        value = PyFloat_FromDouble(DB_LITTLE_ENDIAN(num)); \
        b->offset += 8; \
    } while (0)

    #define complex_wr(b, value) do { \
        Py_complex complex = PyComplex_AsCComplex(value); \
        memcpy(b->msg + b->offset, &complex, sizeof(Py_complex)); \
        b->offset += sizeof(Py_complex); \
    } while (0)
    #define complex_rd(b, value) do { \
        Py_complex complex; \
        memcpy(&complex, b->msg + b->offset, sizeof(Py_complex)); \
        value = PyComplex_FromCComplex(complex); \
        b->offset += sizeof(Py_complex); \
    } while (0)

#endif

#define bool_wr(b, value) (*(b->msg + b->offset++) = DT_BOOLF | (!!(value == Py_True) << 3))

#endif // GLOBALS_H