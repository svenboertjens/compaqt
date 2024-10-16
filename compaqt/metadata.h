#ifndef GLOBALS_H
#define GLOBALS_H

// This file contains methods used in multiple files

#include <Python.h>

// These macros will be overwritten by the setup.py if needed
#define IS_LITTLE_ENDIAN 1 // Whether the system is little-endian
#define STRICT_ALIGNMENT 0 // Whether to use strict alignment

// Default chunk size is 256KB
#define DEFAULT_CHUNK_SIZE 1024*256

/* ENDIANNESS */

// Convert values to little-endian format
#if (IS_LITTLE_ENDIAN == 1)

    // Already little-endian, don't change anything
    #define LITTLE_64(x) (x)

    #define LITTLE_DOUBLE(x) (x)

#else

    // GCC and CLANG builtins
    #if defined(__GNUC__) || defined(__clang__)

        #define LITTLE_64(x) (__builtin_bswap64(x))

    // MSCV builtins
    #elif defined(_MSC_VER)

        #include <intrin.h>
        #define LITTLE_64(x) (_byteswap_uint64(x))

    // Fallbacks with manual swapping
    #else

        #define LITTLE_64(x) ( \
            (((x) >> 56) & 0x00000000000000FF) | \
            (((x) >> 40) & 0x000000000000FF00) | \
            (((x) >> 24) & 0x0000000000FF0000) | \
            (((x) >>  8) & 0x00000000FF000000) | \
            (((x) <<  8) & 0x000000FF00000000) | \
            (((x) << 24) & 0x0000FF0000000000) | \
            (((x) << 40) & 0x00FF000000000000) | \
            (((x) << 56) & 0xFF00000000000000)   \
        )

    #endif

    // Don't cast doubles to integers as that can be undefined, instead use the safe copy method
    #define LITTLE_DOUBLE(x) do { \
        uint64_t __temp; \
        memcpy(&__temp, &(x), 8); \
        __temp = LITTLE_64(__temp); \
        memcpy(&(x), &__temp, 8); \
    } while (0)

#endif

/* ALLOC SETTINGS */

// Average size of items
extern size_t avg_item_size;
// Average size of re-allocations
extern size_t avg_realloc_size;
// Whether to dymically adjust allocation settings
extern char dynamic_allocation_tweaks;

#define AVG_REALLOC_MIN 64
#define AVG_ITEM_MIN 4

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

            if (diff_small + AVG_REALLOC_MIN < avg_realloc_size)
                avg_realloc_size -= diff_small;
            else
                avg_realloc_size = AVG_REALLOC_MIN;

            if (med_small + AVG_ITEM_MIN < avg_item_size)
                avg_item_size -= med_small;
            else
                avg_item_size = AVG_ITEM_MIN;
        }
    }
}

/* METADATA */

// FOR METADATA DOCUMENTATION, SEE "docs/metadata.md"


// The maximum bytes metadata will take up
#define MAX_METADATA_SIZE 9

// Write the length mode 2 mask (separated from main LM2 macro for streaming)
#define WR_METADATA_LM2_MASK(msg, offset, mask, num_bytes) do { \
    *((msg) + offset++) = (mask) | 0b00011000 | (num_bytes << 5); \
} while (0)

// Write length mode 2
#define WR_METADATA_LM2(msg, offset, length, num_bytes) do { \
    const size_t __length = LITTLE_64(length); \
    memcpy(msg + offset, &(__length), num_bytes + 2); \
    offset += num_bytes + 2; \
} while (0)

// Read length mode 2
#define RD_METADATA_LM2(msg, offset, length, num_bytes) do { \
    length = 0; \
    size_t __length; \
    memcpy(&(__length), msg + offset, num_bytes + 2); \
    (length) = LITTLE_64(__length); \
    offset += num_bytes + 2; \
} while (0)

// Write metadata
#define WR_METADATA(msg, offset, dt_mask, length) do { \
    if (((length) >> 4) == 0) \
        *((msg) + offset++) = (dt_mask) | ((length) << 4); \
    else if (((length) >> 11) == 0) \
    { \
        *((msg) + offset++) = (dt_mask) | 0b00001000 | ((length) << 5); \
        *((msg) + offset++) = (length) >> 3; \
    } \
    else \
    { \
        /* The number of bytes doesn't count the first 2 bytes, which are guaranteed to be used */ \
        const int num_bytes = !!((length) >> (8 * 2)) + !!((length) >> (8 * 3)) + !!((length) >> (8 * 4)) + !!((length) >> (8 * 5)) + !!((length) >> (8 * 6)) + !!((length) >> (8 * 7)); \
        WR_METADATA_LM2_MASK(msg, offset, dt_mask, num_bytes); \
        WR_METADATA_LM2(msg, offset, length, num_bytes); \
        break; \
    } \
} while (0)

#define RD_METADATA(msg, offset, length) do { \
    switch (*((msg) + offset) & 0b00011000) \
    { \
    case 0b00010000: \
    case 0b00000000: \
        (length) = (*((msg) + offset++) & 0xFF) >> 4; break; \
    case 0b00001000: \
    { \
        (length)  = (*((msg) + offset++) & 0xFF) >> 5; \
        (length) |= (*((msg) + offset++) & 0xFF) << 3; \
        break; \
    } \
    default: \
    { \
        const int num_bytes = (*((msg) + offset++) & 0b11100000) >> 5; \
        RD_METADATA_LM2(msg, offset, length, num_bytes); \
        break; \
    } \
    } \
} while (0)

// Read a datatype mask (also stores other data so don't increment)
#define RD_DTMASK(b) (*(b->msg + b->offset) & 0b00000111)
// Read a group datatype mask (takes up entire byte so increment)
#define RD_DTMASK_GROUP(b) (*(b->msg + b->offset++))

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

#define DT_NOUSE (unsigned char)(6) // Not used by anything yet
#define DT_EXTND (unsigned char)(7) // Custom datatypes (not implemented yet)

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

#if (STRICT_ALIGNMENT == 0)

    #define float_wr(b, value) do { \
        double *num = (double *)(b->msg + b->offset); \
        *num = PyFloat_AS_DOUBLE(value); \
        LITTLE_DOUBLE(*num); \
        b->offset += 8; \
    } while (0)
    #define float_rd(b, value) do { \
        double *num = (double *)(b->msg + b->offset); \
        LITTLE_DOUBLE(*num); \
        value = PyFloat_FromDouble(*num); \
        b->offset += 8; \
    } while (0)

    #define complex_wr(b, value) do { \
        *(Py_complex *)(b->msg + b->offset) = PyComplex_AsCComplex(value); \
        LITTLE_DOUBLE(*(double *)(b->msg + b->offset)); \
        b->offset += 8; \
        LITTLE_DOUBLE(*(double *)(b->msg + b->offset)); \
        b->offset += 8; \
    } while (0)
    #define complex_rd(b, value) do { \
        Py_complex temp; \
        temp.real = *(double *)(b->msg + b->offset); \
        LITTLE_DOUBLE(temp.real); \
        b->offset += 8; \
        temp.imag = *(double *)(b->msg + b->offset); \
        LITTLE_DOUBLE(temp.imag); \
        b->offset += 8; \
        value = PyComplex_FromCComplex(temp); \
    } while (0)

#else

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

#endif

#define bool_wr(b, value) (*(b->msg + b->offset++) = DT_BOOLF | (!!(value == Py_True) << 3))

#endif // GLOBALS_H