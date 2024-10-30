#ifndef GLOBALS_H
#define GLOBALS_H

// This file contains methods used in multiple files

#include <Python.h>

// These macros will be overwritten by the setup.py if needed
#ifndef IS_LITTLE_ENDIAN

    #define IS_LITTLE_ENDIAN 1 // Whether the system is little-endian

#endif // IS_LITTLE_ENDIAN

// Default chunk size is 256KB
#define DEFAULT_CHUNK_SIZE 1024*256

// The first values of all buffer structs, to accept and use generic structs
typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
} buffer_t;

// Function pointer for buffer checks
typedef int (*buffer_check_t)(buffer_t *, const size_t);

/* ENDIANNESS */

// Convert values to little-endian format
#if (IS_LITTLE_ENDIAN == 1)

    // Already little-endian, don't change anything
    #define LITTLE_64(x) (x)

    #define LITTLE_DOUBLE(x) (x)

#else

    // GCC and CLANG intrinsics
    #if defined(__GNUC__) || defined(__clang__)

        #define LITTLE_64(x) (__builtin_bswap64(x))

    // MSCV intrinsics
    #elif defined(_MSC_VER)

        #include <intrin.h>
        #define LITTLE_64(x) (_byteswap_uint64(x))

    // Fallback with manual swapping
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

/* INTRINSICS */

// GCC and CLANG
#if (defined(__GNUC__) || defined(__clang__))

    #define LEADING_ZEROES_64(x) (__builtin_clzll(x))

// MSCV
#elif defined(_MSC_VER)

    #include <intrin.h>
    #define LEADING_ZEROES_64(x) (8 - _BitScanReverse64(x))

// Fallback
#else

    static inline int LEADING_ZEROES_64(uint64_t x)
    {
        int n = 64;

        if (x <= 0x00000000FFFFFFFF) { n -= 32; x <<= 32; }
        if (x <= 0x0000FFFFFFFFFFFF) { n -= 16; x <<= 16; }
        if (x <= 0x00FFFFFFFFFFFFFF) { n -=  8; x <<=  8; } 
        if (x <= 0x0FFFFFFFFFFFFFFF) { n -=  4; x <<=  4; } 
        if (x <= 0x3FFFFFFFFFFFFFFF) { n -=  2; x <<=  2; } 
        if (x <= 0x7FFFFFFFFFFFFFFF) { n -=  1; }

        return n;
    }

#endif

// Count the number of used bytes in a 64-bit unsigned integer
#define USED_BYTES_64(x) (8 - (LEADING_ZEROES_64(x) >> 3))

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
            const size_t med_diff = difference / (num_items + 1);

            avg_realloc_size += difference >> 1;
            avg_item_size += med_diff >> 1;
        }
        else
        {
            const size_t difference = initial_allocated - offset;
            const size_t med_diff = difference / (num_items + 1);
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

// The maximum size metadata will take up
#define MAX_METADATA_SIZE 9

// Write the length mode 2 mask (separated from main LM2 macro for streaming methods)
#define WR_METADATA_LM2_MASK(msg, offset, mask, num_bytes) do { \
    /* Minus 1 because we're guaranteed to use at least 1 byte, and only up to the number 7 fits in 3 bytes */ \
    *((msg) + offset++) = (mask) | 0b00011000 | ((num_bytes - 1) << 5); \
} while (0)

// Write length mode 2
#define WR_METADATA_LM2(msg, offset, length, num_bytes) do { \
    const size_t __length = LITTLE_64(length); \
    memcpy(msg + offset, &(__length), num_bytes); \
    offset += num_bytes; \
} while (0)

// Write metadata
#define WR_METADATA(msg, offset, dt_mask, length) do { \
    if (length < 16) \
        *((msg) + offset++) = (dt_mask) | ((length) << 4); \
    else if (length < 2048) \
    { \
        *((msg) + offset++) = (dt_mask) | 0b00001000 | ((length) << 5); \
        *((msg) + offset++) = (length) >> 3; \
        break; \
    } \
    else \
    { \
        const int num_bytes = USED_BYTES_64(length); \
        WR_METADATA_LM2_MASK(msg, offset, dt_mask, num_bytes); \
        WR_METADATA_LM2(msg, offset, length, num_bytes); \
        break; \
    } \
} while (0)

// Read length mode 2
#define RD_METADATA_LM2(msg, offset, length, num_bytes) do { \
    length = 0; \
    size_t __length = 0; \
    memcpy(&(__length), msg + offset, num_bytes); \
    (length) = LITTLE_64(__length); \
    offset += num_bytes; \
} while (0)

// Read metadata
#define RD_METADATA(msg, offset, length) do { \
    switch ((*((msg) + offset) & 0b00011000) >> 3) \
    { \
    case 0b10: \
    case 0b00: \
    { \
        (length) = (*((msg) + offset++) & 0xFF) >> 4; \
        break; \
    } \
    case 0b01: \
    { \
        (length)  = (*((msg) + offset++) & 0xFF) >> 5; \
        (length) |= (*((msg) + offset++) & 0xFF) << 3; \
        break; \
    } \
    default: \
    { \
        /* Plus 1 because we stored 1 less earlier */ \
        const int num_bytes = ((*((msg) + offset++) & 0b11100000) >> 5) + 1; \
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
#define DT_GROUP (unsigned char)(5) // Group datatype mask for identifying it's a grouped type
#define DT_BOOLF DT_GROUP | (unsigned char)(0 << 3) // False Booleans
#define DT_BOOLT DT_GROUP | (unsigned char)(1 << 3) // True Booleans
#define DT_FLOAT DT_GROUP | (unsigned char)(2 << 3) // Floats
#define DT_NONTP DT_GROUP | (unsigned char)(3 << 3) // NoneTypes

#define DT_EXTND (unsigned char)(6) // Custom types

#define DT_NOUSE (unsigned char)(7) // Not in use for anything (yet?)


#endif // GLOBALS_H