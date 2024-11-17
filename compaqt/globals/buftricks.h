// This file contains some convenience macros for encode and decode buffers

#ifndef BUFTRICKS_H
#define BUFTRICKS_H

#define BUF_INIT_OFFSETS(max_length) do { \
        b->offset = b->base; \
        b->max_offset = b->base + max_length; \
} while (0)


#define BUF_GET_OFFSET (b->offset - b->base)
#define BUF_GET_LENGTH (b->max_offset - b->base)


#define BUF_PRE_INC (++(b->offset))
#define BUF_POST_INC ((b->offset)++)


#endif // BUFTRICKS_H