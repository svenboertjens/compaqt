#ifndef METADATA_H
#define METADATA_H

#include <Python.h>
#include "globals/internals.h"

// The maximum size metadata will take up
#define MAX_METADATA_SIZE 9

// Write the length mode 2 mask (separated from main LM2 macro for streaming methods)
#define WR_METADATA_LM2_MASK(offset, mask, num_bytes) do { \
    /* Minus 1 because we're guaranteed to use at least 1 byte, and only up to the number 7 fits in 3 bytes */ \
    *(offset++) = (mask) | 0b00011000 | ((num_bytes - 1) << 5); \
} while (0)

// Write length mode 2
#define WR_METADATA_LM2(offset, length, num_bytes) do { \
    const size_t __length = LITTLE_64(length); \
    memcpy(offset, &(__length), num_bytes); \
    offset += num_bytes; \
} while (0)

// Write metadata
#define WR_METADATA(offset, dt_mask, length) do { \
    if (length < 16) \
        *(offset++) = (dt_mask) | ((length) << 4); \
    else if (length < 2048) \
    { \
        *(offset++) = (dt_mask) | 0b00001000 | ((length) << 5); \
        *(offset++) = (length) >> 3; \
        break; \
    } \
    else \
    { \
        const int num_bytes = USED_BYTES_64(length); \
        WR_METADATA_LM2_MASK(offset, dt_mask, num_bytes); \
        WR_METADATA_LM2(offset, length, num_bytes); \
        break; \
    } \
} while (0)

// Read length mode 2
#define RD_METADATA_LM2(offset, length, num_bytes) do { \
    size_t __length; \
    /* Copy 8 by default, mask away redundant bytes */ \
    memcpy(&(__length), offset, 8); \
    (length) = LITTLE_64(__length); \
    size_t length_mask = (size_t)-1; \
    if (num_bytes != 8) \
        length_mask = ((1ULL << (num_bytes << 3)) - 1); \
    (length) &= length_mask; \
    offset += num_bytes; \
} while (0)

// Read metadata
#define RD_METADATA(offset, length) do { \
    switch ((*(offset) & 0b00011000) >> 3) \
    { \
    case 0b10: \
    case 0b00: \
    { \
        (length) = (*(offset++) & 0xFF) >> 4; \
        break; \
    } \
    case 0b01: \
    { \
        (length)  = (*(offset++) & 0xFF) >> 5; \
        (length) |= (*(offset++) & 0xFF) << 3; \
        break; \
    } \
    default: \
    { \
        /* Plus 1 because we stored 1 less earlier */ \
        const int num_bytes = ((*(offset++) & 0b11100000) >> 5) + 1; \
        RD_METADATA_LM2(offset, length, num_bytes); \
        break; \
    } \
    } \
} while (0)

#endif // METADATA_H