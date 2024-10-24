// This file contains a method to validate encoded data

#include <Python.h>
#include "metadata.h"

typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
} buffer_t;

#define OVERREAD_CHECK(length) do { \
    if (b->offset + length > b->allocated) return 1; \
} while (0)

#define BUFFER_REFRESH() do { \
    if (b->offset + MAX_METADATA_SIZE > b->allocated) \
    { \
        if (fseek(file, b->offset, SEEK_CUR) != 0) return 1; \
        if ((b->allocated = fread(b->msg, 1, b->allocated, file)) == 0) return 1; \
        b->offset = 0; \
    } \
    else if (b->offset + 1 > b->allocated) return 1; \
} while (0)

static inline int _validate(buffer_t *b, FILE *file)
{
    if (file != NULL)
        BUFFER_REFRESH();
    else
        OVERREAD_CHECK(1);
    
    const char dt_mask = RD_DTMASK(b);
    switch (dt_mask)
    {
    case DT_GROUP: // Group datatypes
    {
        switch (RD_DTMASK_GROUP(b))
        {
        // Static lengths, no metadata to read, already incremented by `RD_DTMASK_GROUP`
        case DT_FLOAT: OVERREAD_CHECK(8);  b->offset += 8;  return 0;
        case DT_BOOLF: // Boolean and NoneType values don't have more bytes, nothing to increment
        case DT_BOOLT:
        case DT_NONTP: return 0;
        default: return 1;
        }
    }
    case DT_ARRAY: // Combine the two container types as all values are processed the same anyway.
    case DT_DICTN: // Multiply dict number of items by 2 as it has pairs, so double the num items
    {
        size_t num_items = 0;
        RD_METADATA(b->msg, b->offset, num_items);
        OVERREAD_CHECK(0);

        if (dt_mask == DT_DICTN)
            num_items <<= 1;

        for (size_t i = 0; i < num_items; ++i)
            if (_validate(b, file) == 1) return 1;
        
        return 0;
    }
    case DT_EXTND:
    case DT_NOUSE: return 1; // Not supported yet, so currently means invalid
    default:
    {
        // All other cases use VLE metadata, so read length and increment offset using it
        size_t length = 0;
        RD_METADATA(b->msg, b->offset, length);
        OVERREAD_CHECK(length);
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
    size_t chunk_size = DEFAULT_CHUNK_SIZE;
    int err_on_invalid = 0;

    static char *kwlist[] = {"value", "file_name", "file_offset", "chunk_size", "err_on_invalid", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!siii", kwlist, &PyBytes_Type, &value, &filename, (Py_ssize_t *)(&file_offset), (Py_ssize_t *)(&chunk_size), &err_on_invalid))
        return NULL;

    buffer_t b;
    b.offset = 0;

    int result;
    if (value != NULL)
    {
        PyBytes_AsStringAndSize(value, &b.msg, (Py_ssize_t *)&b.allocated);
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
            PyErr_Format(PyExc_FileNotFoundError, "Unable to find position %zu of file '%s'", file_offset, filename);
            return NULL;
        }

        b.allocated = chunk_size;
        b.msg = (char *)malloc(chunk_size);

        if (b.msg == NULL)
        {
            PyErr_NoMemory();
            fclose(file);
            return NULL;
        }

        result = _validate(&b, file);
        fclose(file);
        free(b.msg);
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
        PyErr_SetString(PyExc_ValueError, "The received object does not appear to be valid");
        return NULL;
    }
}

