#ifndef TYPEMASKS_H
#define TYPEMASKS_H

//      NAME                    MASK                | Metadata RW method
// 3 bits
#define DT_ARRAY (unsigned char)0x00 // Arrays      | VARLEN
#define DT_DICTN (unsigned char)0x01 // Dicts       | VARLEN
#define DT_BYTES (unsigned char)0x02 // Binary      | VARLEN
#define DT_STRNG (unsigned char)0x03 // Strings     | VARLEN
#define DT_INTGR (unsigned char)0x04 // Integers    | INTEGER
#define DT_UTYPE (unsigned char)0x06 // Usertype    | UTYPE
// 5 bits
#define DT_BOOLF (unsigned char)0x05 // False       | BOOLEAN, no reading method
#define DT_BOOLT (unsigned char)0x0D // True        | BOOLEAN, no reading method
#define DT_FLOAT (unsigned char)0x15 // Float       | <no methods>
#define DT_NONTP (unsigned char)0x1D // NoneType    | <no methods>

// 0x07 is available but not used for anything


// Max size for metadata
#define MAX_METADATA_SIZE 8


#include "globals/internals.h"

// Metadata functions automatically increment the offset

// Access offset through the struct
#define __offset b->offset

// Memcpy macro for efficiently copying bytes to the offset
#define __MEMCPY_WRITE(length, nbytes) do { \
    /* Ensure endianness */ \
    const size_t __length = LITTLE_64(length); \
    /* Always copy 8 bytes for speed (8 bytes are made available through offset checks) */ \
    memcpy(__offset, &__length, 8); \
    \
    __offset += nbytes; \
} while (0)

// Memcpy macro for efficiently reading bytes from the offset
#define __MEMCPY_READ(length, nbytes) do { \
    /* Always read 8 bytes for speed, mask them by nbytes to get the actual length */ \
    memcpy(&length, __offset, 8); \
    /* Convert back to system endianness */ \
    length = LITTLE_64(length); \
    \
    /* Create a mask with 0xFF on used bytes and 0x00 on unused bytes */ \
    const size_t len_mask = (nbytes == 8) ? ~0ULL : (1ULL << (nbytes << 3)) - 1; \
    length &= len_mask; \
    \
    __offset += nbytes; \
} while (0)

// Set the current offset byte and increment
#define __SETBYTE(x) ((__offset++)[0] = x)

// Get the current offset byte and increment
#define __GETBYTE() ((__offset++)[0] & 0xFF)


// Mode 3 read function for varlen data (used for extendable containers)
#define METADATA_VARLEN_WR_MODE3(tpmask, length, nbytes) do { \
    /* Mode 3 indicated with 4th and 5th bit as `11` */ \
    __SETBYTE(tpmask | (0b11 << 3) | ((nbytes - 1) << 5)); \
    /* Use nbytes memcpy method for the length */ \
    __MEMCPY_WRITE(length, nbytes); \
} while (0)

// VARLEN metadata write
#define METADATA_VARLEN_WR(tpmask, length) do { \
    if (length < 16) /* Lower than 16 (4 bits space) fits mode 1 */ \
    { \
        /* Mode 1 indicated by a zero bit on the 4th bit, place length above */ \
        __SETBYTE(tpmask | (0b0 << 3) | (length << 4)); \
    } \
    else if (length < 2048) /* Lower than 2048 (11 bits space) fits in mode 2 */ \
    { \
        /* Mode 2 indicated with 4th and 5th bit as `01`. Place first 3 bits in top of byte */ \
        __SETBYTE(tpmask | (0b01 << 3) | (length << 5)); \
        __SETBYTE(length >> 3); /* Write rest of length in the next byte */ \
    } \
    else \
    { \
        /* Calculate used bytes of the length */ \
        const unsigned int nbytes = USED_BYTES_64(length); \
        METADATA_VARLEN_WR_MODE3(tpmask, length, nbytes); \
    } \
} while (0)

// Mode 1 read function for varlen data
#define METADATA_VARLEN_RD_MODE1(length) do { \
    /* The length is stored in the upper 4 bits so shift right by 45 to correctly place the length bits */ \
    length = __GETBYTE() >> 4; \
} while (0)

// Mode 2 read function for varlen data
#define METADATA_VARLEN_RD_MODE2(length) do { \
    /* First 3 length bits are in the upper 3 bits, the rest is in the next byte */ \
    length = (__GETBYTE() >> 5); \
    length |= (__GETBYTE() << 3); \
} while (0)

// Mode 3 read function for varlen data
#define METADATA_VARLEN_RD_MODE3(length) do { \
    const unsigned int nbytes = (__GETBYTE() >> 5) + 1; \
    __MEMCPY_READ(length, nbytes); \
} while (0)

// VARLEN metadata read
#define METADATA_VARLEN_RD(length) do { \
    /* Mask the mode bits of the first byte */ \
    const unsigned int mode = (__offset[0] >> 3) & 0b11; \
    \
    switch (mode) \
    { \
    case 0b00: /* Mode 1 */ \
    case 0b10: /* can also be `10`, as mode 1 only sets bit 4 to 0. Bit 5 is used for the length already */ \
    { \
        METADATA_VARLEN_RD_MODE1(length); \
        break; \
    } \
    case 0b01: /* Mode 2 */ \
    { \
        METADATA_VARLEN_RD_MODE2(length); \
        break; \
    } \
    case 0b11: /* Mode 3 */ \
    { \
        METADATA_VARLEN_RD_MODE3(length); \
        break; \
    } \
    } \
} while (0)


// INTEGER metadata writing
#define METADATA_INTEGER_WR(nbytes) do { \
    __SETBYTE(DT_INTGR | (nbytes << 3)); \
} while (0)

// INTEGER metadata reading
#define METADATA_INTEGER_RD(nbytes) do { \
    nbytes = __GETBYTE() >> 3; \
} while (0)


// UTYPE metadata writing
#define METADATA_UTYPE_WR(idx, length) do { \
    __SETBYTE(DT_UTYPE | idx); /* idx was pre-shifted up by 3 on setup */ \
    /* Create the shifted length that has the 3 extra bits in the bottom, to get an accurate nbytes */ \
    size_t __shift_length = length << 3; \
    const unsigned int nbytes = USED_BYTES_64(__shift_length); \
    __shift_length |= nbytes; /* Push the nbytes to the bottom 3 bits of the length value */ \
    __MEMCPY_WRITE(__shift_length, nbytes); \
} while (0)

// UTYPE metadata reading
#define METADATA_UTYPE_RD(idx, length) do { \
    /* idx is stored in upper 5 bits */ \
    idx = __GETBYTE() >> 3; \
    /* Get the number of bytes stored in the bottom 3 bits, add 1 as that was subtracted before storing */ \
    const unsigned int nbytes = __offset[0] & 0b111; /* Without increment, length is also stored on this byte */ \
    /* Get the length back using memcpy. Shift right by 3 to remove the nbytes bits */ \
    __MEMCPY_READ(length, nbytes); \
    length >>= 3; \
} while (0)


// BOOLEAN writing
#define BOOLEAN_WR(val) do { \
    /*  BOOLF = 0b011
     *  BOOLT = 0b111
     *  
     *  We can set the 4th bit by checking if the value is a True object.
     *  If it is, the 4th bit is set to 1, creating 0b1111. If it isn't,
     *  the 4th bit is set to 0, creating 0b0111.
     */ \
    __SETBYTE(DT_BOOLF | ((val == Py_True) << 3)); \
} while (0)


// 3-bit case as 5-bit cases
#define CASES_AS_5BIT(tpmask)  \
    case tpmask | (0b00 << 3): \
    case tpmask | (0b01 << 3): \
    case tpmask | (0b10 << 3): \
    case tpmask | (0b11 << 3):

// Mode 1 cases
#define VARLEN_MODE1_CASES(tpmask) \
    case tpmask | (0b00 << 3): \
    case tpmask | (0b10 << 3):

// Mode 2 cases
#define VARLEN_MODE2_CASES(tpmask) \
    case tpmask | (0b01 << 3):

// Mode 3 cases
#define VARLEN_MODE3_CASES(tpmask) \
    case tpmask | (0b11 << 3):


// Execute a block of code, fetching the length by default, with a specified length method
#define VARLEN_READ_MODEx(code, getlength) { \
    size_t length; \
    getlength(length); \
    { code } \
}

// Execute a block of code for a specific typemask. The macro separates the code across different types for performance by using more direct metadata methods
#define VARLEN_READ_CASES(tpmask, code) \
    VARLEN_MODE1_CASES(tpmask) VARLEN_READ_MODEx(code, METADATA_VARLEN_RD_MODE1) \
    VARLEN_MODE2_CASES(tpmask) VARLEN_READ_MODEx(code, METADATA_VARLEN_RD_MODE2) \
    VARLEN_MODE3_CASES(tpmask) VARLEN_READ_MODEx(code, METADATA_VARLEN_RD_MODE3) \


#endif // TYPEMASKS_H