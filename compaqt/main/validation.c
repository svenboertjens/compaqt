// This file contains a method to validate encoded data

#include <Python.h>

#include "globals/metadata.h"
#include "globals/exceptions.h"
#include "globals/typemasks.h"
#include "globals/buftricks.h"
#include "globals/typedefs.h"

#define OVERREAD_CHECK(length) do { \
    if (b->offset + length > b->max_offset) return 1; \
} while (0)

#define BUFFER_REFRESH(length) do { \
    if (b->offset + length > b->max_offset) \
    { \
        b->max_offset = b->base + fread(b->base, 1, BUF_GET_LENGTH, file); \
        if (b->max_offset == b->base) return 1; /* EOF or error */ \
        if ((size_t)(b->max_offset - b->base) <= length) return 1; /* File not big enough */ \
        b->offset = b->base; \
    } \
} while (0)

#define CHECK(length) do { \
    if (file != NULL) \
        BUFFER_REFRESH(length); \
    else \
        OVERREAD_CHECK(length); \
} while (0)

static inline int _validate(decode_t *b, FILE *file)
{
    CHECK(0);
    
    const char dt_mask = b->offset[0];
    switch (dt_mask)
    {
    case DT_GROUP: // Group datatypes
    {
        switch (BUF_POST_INC[0])
        {
        // Static lengths, no metadata to read, already incremented by `RD_DTMASK_GROUP`
        case DT_FLOAT: CHECK(8);  b->offset += 8; return 0;
        case DT_BOOLF: // Boolean and NoneType values don't have more bytes, nothing to increment
        case DT_BOOLT:
        case DT_NONTP: return 0;
        default: return 1;
        }
    }
    case DT_ARRAY: // Combine the two container types as they're processed the same
    case DT_DICTN:
    {
        size_t num_items = 0;
        RD_METADATA(b->offset, num_items);

        // Multiply dict number of items by 2 as it has pairs
        if (dt_mask == DT_DICTN)
            num_items *= 2;
        
        for (size_t i = 0; i < num_items; ++i)
            if (_validate(b, file) != 0) return 1;
        
        return 0;
    }
    case DT_EXTND:
    {
        ++(b->offset);

        const size_t num_bytes = *BUF_POST_INC & 0xFF;
        CHECK(num_bytes);

        size_t length;
        RD_METADATA_LM2(b->offset, length, num_bytes);

        b->offset += length;

        return 0;
    }
    case DT_INTGR:
    {
        const size_t total_len = ((b->offset[0] & 0xFF) >> 3) + 1;
        CHECK(total_len);

        b->offset += total_len;

        return 0;
    }
    case DT_NOUSE: return 1; // Not in use, so this would mean invalid
    default:
    {
        // All other cases use regular metadata, so read length and increment offset using it
        size_t length;
        RD_METADATA(b->offset, length);

        b->offset += length;
        return 0;
    }
    }
}

PyObject *validate(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value = NULL;
    char *filename = NULL;
    size_t file_offset = 0;
    size_t chunk_size = 1024*32;
    int err_on_invalid = 0;

    static char *kwlist[] = {"value", "file_name", "file_offset", "chunk_size", "err_on_invalid", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!siii", kwlist, &PyBytes_Type, &value, &filename, (Py_ssize_t *)(&file_offset), (Py_ssize_t *)(&chunk_size), &err_on_invalid))
        return NULL;

    decode_t b;

    int result;
    if (value != NULL)
    {
        PyBytes_AsStringAndSize(value, &b.base, (Py_ssize_t *)&b.max_offset);

        b.offset = b.base;
        b.max_offset += (size_t)b.base;

        result = _validate(&b, NULL);
    }
    else if (filename != NULL)
    {
        FILE *file = fopen(filename, "rb");

        if (file == NULL)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to open file '%s'", filename);
            return NULL;
        }

        if (fseek(file, file_offset, SEEK_SET) != 0)
        {
            PyErr_Format(FileOffsetError, "Unable to find position %zu of file '%s'", file_offset, filename);
            return NULL;
        }

        b.base = b.offset = (char *)malloc(chunk_size);

        if (b.base == NULL)
        {
            PyErr_NoMemory();
            fclose(file);
            return NULL;
        }

        b.max_offset = b.base + fread(b.base, 1, chunk_size, file);

        result = _validate(&b, file);

        // Do an extra overread check at the end
        if (result == 0)
        {
            // The file offset before the last read
            const size_t last_offset = ftell(file) - (size_t)(b.max_offset - b.base);

            // The file offset of the end of the file
            fseek(file, 0, SEEK_END);
            const size_t end_offset = ftell(file);

            // Invalid data if we exceeded the highest possible offset (the end of the file)
            if (last_offset + (size_t)(b.offset - b.base) > end_offset)
                result = 1;
        }

        fclose(file);
        free(b.base);
    }
    else
    {
        PyErr_SetString(PyExc_ValueError, "Expected either the 'value' or 'filename' argument, got neither");
        return NULL;
    }

    if (result == 0)
        Py_RETURN_TRUE;
    else if (err_on_invalid == 0)
        Py_RETURN_FALSE;
    else
    {
        PyErr_SetString(ValidationError, "The received object does not appear to be valid");
        return NULL;
    }
}

