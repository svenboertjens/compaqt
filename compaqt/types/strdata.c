#include <Python.h>

// Cases for continuation bytes
#define CONTN_CASES \
    case 0b1000:    \
    case 0b1001:    \
    case 0b1010:    \
    case 0b1011:

// Cases for 1-byte data
#define BYTE1_CASES \
    case 0b0000:    \
    case 0b0001:    \
    case 0b0010:    \
    case 0b0011:    \
    case 0b0100:    \
    case 0b0101:    \
    case 0b0110:    \
    case 0b0111:

// Cases for 2-byte data
#define BYTE2_CASES \
    case 0b1100:    \
    case 0b1101:

// Cases for 3-byte data
#define BYTE3_CASES \
    case 0b1110:

// Cases for 4-byte data
#define BYTE4_CASES \
    case 0b1111:

// Get a buffer casted to an unsigned char
#define UCHAR_CAST(data) ((unsigned char)(*(data)))


/*  
 *  Get the number of codepoints in the UTF-8 data.
 *
 *  Returns -1 if the data is not valid UTF-8.
 *  
*/
Py_ssize_t utf8_codepoints(char *data, const Py_ssize_t len)
{
    Py_ssize_t codepoints = 0;

    const char *max_data = data + len;
    while (data < max_data)
    {
        switch (UCHAR_CAST(data) >> 4)
        {
        CONTN_CASES
        {
            switch (UCHAR_CAST(data - 1) >> 4)
            {
            BYTE4_CASES
            {
                uint32_t tmp;
                memcpy(&tmp, data, 4);

                if ((tmp & 0b110000001100000011000000) != 0b100000001000000010000000)
                    return -1;

                data += 3;
                break;
            }
            BYTE3_CASES
            {
                uint16_t tmp;
                memcpy(&tmp, data, 2);

                if ((tmp & 0b1100000011000000) != 0b1000000010000000)
                    return -1;

                data += 2;
                break;
            }
            BYTE2_CASES
            {
                ++data;
                break;
            }
            default:
            {
                return -1;
            }
            }
            
            break;
        }
        BYTE2_CASES
        {
            if ((UCHAR_CAST(data + 1) & 0b11000000) != 0b10000000)
                return -1;
            
            ++codepoints;
            data += 2;
            break;
        }
        BYTE3_CASES
        {
            uint32_t tmp;
            memcpy(&tmp, data, 4);

            if ((tmp & 0b110000001100000000000000) != 0b100000001000000000000000)
                return -1;

            ++codepoints;
            data += 3;
            break;
        }
        BYTE4_CASES
        {
            uint32_t tmp;
            memcpy(&tmp, data, 4);

            if ((tmp & 0b11000000110000001100000011111000) != 0b10000000100000001000000011110000)
                return -1;

            ++codepoints;
            data += 4;
            break;
        }
        BYTE1_CASES
        {
            codepoints += 2;
            data += 2;
            break;
        }
        default:
        {
            return -1;
        }
        }
    }

    if (data > max_data)
    {
        --data;

        if (data > max_data)
            return -1;
        
        if (UCHAR_CAST(data - 1) >= 0x80)
            return -1;
        
        --codepoints;
    }

    return codepoints;
}


/*  Returns the absolute offset of a specific codepoint.
 *
 *  `bytesize` will be set to the size of the character (1-4 bytes).
 *
 *  Returns -1 if the codepoint wasn't found within the buffer
 *  or if an invalid UTF-8 byte was found.
*/
Py_ssize_t utf8_index(const char *data, const Py_ssize_t len, const Py_ssize_t codepoint, Py_ssize_t *bytesize)
{
    // Start codepoints at -1 to have it pass once extra, which reads the bytesize of the codepoint we want
    Py_ssize_t codepoints = -1;
    Py_ssize_t offset = 0;

    while (codepoints < codepoint)
    {
        switch (UCHAR_CAST(data + offset) >> 4)
        {
        BYTE1_CASES
        {
            *bytesize = 1;
            break;
        }
        BYTE2_CASES
        {
            *bytesize = 2;
            break;
        }
        BYTE3_CASES
        {
            *bytesize = 3;
            break;
        }
        BYTE4_CASES
        {
            *bytesize = 4;
            break;
        }
        default:
        {
            ++data;
            break;
        }
        }

        offset += *bytesize;

        if (offset >= len)
        {
            if (offset > len || codepoints + 2 < codepoint)
            {
                *bytesize = 0; // Set to zero for safety
                return -1;
            }
        }
        
        ++codepoints;
    }

    // Decrement the offset by the bytesize as it was added to the offset before reaching here
    return offset - *bytesize;
}


/*  Returns the amount of times `pattern` occurs in `data`.
 */
Py_ssize_t utf8_count(char *data, const Py_ssize_t data_len, const char *pattern, const Py_ssize_t pattern_len, const int overlap)
{
    if (pattern_len == 0 || pattern_len > data_len)
        return 0;
    
    Py_ssize_t count = 0;
    const char *max_data = data + data_len - pattern_len + 1;

    uint64_t full = ((1ULL << ((pattern_len & 7) << 3)) - 1);

    if (full == 0)
        full = ~0;

    const size_t num_masks = (pattern_len + 7) >> 3;

    uint64_t *masks = (uint64_t *)malloc(num_masks * 8);

    for (size_t i = 0; i < num_masks; ++i)
        memcpy(&masks[i], pattern + (i << 3), 8);
    
    const size_t upper_mask_idx = num_masks - 1;
    
    masks[upper_mask_idx] &= full;

    while (data < max_data)
    {
        Py_ssize_t matches = 0;

        for (; matches < upper_mask_idx; ++matches)
        {
            uint64_t chunk;
            memcpy(&chunk, data + (matches << 3), 8);

            if (chunk != masks[matches])
            {
                matches = -1;
                break;
            }
        }

        if (matches == (upper_mask_idx))
        {
            uint64_t chunk;
            memcpy(&chunk, data + ((upper_mask_idx) << 3), 8);
            chunk &= full;

            if (chunk == masks[upper_mask_idx])
            {
                ++count;
                if (overlap == 0)
                    data += num_masks << 3;
                else
                    ++data;
            }
        }
        else
        {
            ++data;
        }
    }

    free(masks);

    return count;
}

