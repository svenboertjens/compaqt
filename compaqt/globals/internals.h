// This file contains 'internal' macros (macros that use intrinsics)

#ifndef INTERNALS_H
#define INTERNALS_H

#ifndef IS_LITTLE_ENDIAN // Don't overwrite if set already

    #define IS_LITTLE_ENDIAN 1 // Whether the system is little-endian

#endif // IS_LITTLE_ENDIAN

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

    inline int LEADING_ZEROES_64(uint64_t x)
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
#define USED_BYTES_64(x) (x == 0 ? 1 : 8 - (LEADING_ZEROES_64(x) >> 3))

#endif // INTERNALS_H