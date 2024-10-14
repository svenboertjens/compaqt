// This file contains the regular serialization methods

#include "regular.h"

#include <Python.h>
#include "metadata.h"
#include "serialization.h"

/* ENCODING */

typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
    int reallocs;
} encode_t;

static inline int offset_check(buffer_t *b, const size_t length)
{
    if (b->offset + length > b->allocated)
    {
        ((encode_t *)(b))->reallocs++;

        const size_t new_length = b->offset + length + avg_realloc_size;

        char *tmp = (char *)realloc(b->msg, new_length);
        if (tmp == NULL)
        {
            PyErr_NoMemory();
            return 0;
        }
        b->allocated = new_length;
        b->msg = tmp;
    }

    return 0;
}

PyObject *encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value;
    char *filename = NULL;

    static char *kwlist[] = {"value", "filename", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|s", kwlist, &value, &filename))
        return NULL;
    
    encode_t b;

    b.offset = 0;
    b.msg = (char *)malloc(avg_realloc_size);
    b.allocated = avg_realloc_size;
    b.reallocs = 0;

    if (b.msg == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    // Encode the value
    if (encode_item((buffer_t *)(&b), value, offset_check) == 1)
    {
        free(b.msg);
        return NULL;
    }

    // See if we should write to a file
    if (filename != NULL)
    {
        FILE *file = fopen(filename, "wb");

        if (file == NULL)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to open/create file '%s'", filename);
            free(b.msg);
            return NULL;
        }

        fwrite(b.msg, 1, b.offset, file);
        fclose(file);
        
        free(b.msg);
        Py_RETURN_NONE;
    }

    // Otherwise return as a Python bytes object
    PyObject *result = PyBytes_FromStringAndSize(b.msg, b.offset);
    
    free(b.msg);
    return result;
}

/* DECODING */

static inline int overread_check(buffer_t *b, const size_t length)
{
    if (b->offset + length > b->allocated)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid or corrupted bytes string");
        return 1;
    }

    return 0;
}

PyObject *decode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value = NULL;
    char *filename = NULL;

    static char *kwlist[] = {"value", "filename", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!s", kwlist, &PyBytes_Type, &value, &filename))
        return NULL;
    
    buffer_t b;

    // Read from the value if one is given
    if (value != NULL)
    {
        PyBytes_AsStringAndSize(value, &b.msg, (Py_ssize_t *)&b.allocated);

        if (b.allocated == 0)
        {
            PyErr_SetString(PyExc_ValueError, "Received an invalid or corrupted bytes string");
            return NULL;
        }
    }
    else if (filename != NULL)
    {
        // Otherwise read from the file if given
        FILE *file = fopen(filename, "rb");

        if (file == NULL)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Cannot open file '%s'", filename);
            return NULL;
        }

        // Get the length of the file
        if (fseek(file, 0, SEEK_END) != 0)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to find the end of file '%s'", filename);
            fclose(file);
            return NULL;
        }

        // The amount to allocate is the size of the file
        b.allocated = ftell(file);

        b.msg = (char *)malloc(b.allocated);
        if (b.msg == NULL)
        {
            PyErr_NoMemory();
            fclose(file);
            return NULL;
        }

        // Go back to the start of the file to read its contents
        if (fseek(file, 0, SEEK_SET) != 0)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to find the start of file '%s'", filename);
            fclose(file);
            return NULL;
        }

        // Copy the file content into the allocated message buffer
        fread(b.msg, 1, b.allocated, file);
        fclose(file);
    }
    else
    {
        PyErr_SetString(PyExc_ValueError, "Expected either the 'value' or 'filename' argument, got neither");
        return NULL;
    }

    b.offset = 0;

    return decode_item(&b, overread_check);
}

/* VALIDATION */

#define VALIDATE_OVERREAD_CHECK(length) do { \
    if (b->offset + length > b->allocated) return 1; \
} while (0)

static inline int _validate(buffer_t *b)
{
    VALIDATE_OVERREAD_CHECK(1);

    switch (RD_DTMASK(b))
    {
    case DT_GROUP: // Group datatypes
    {
        switch (RD_DTMASK_GROUP(b))
        {
        // Static lengths, no metadata to read, already incremented by `RD_DTMASK_GROUP`
        case DT_FLOAT: VALIDATE_OVERREAD_CHECK(8);  b->offset += 8;  return 0;
        case DT_CMPLX: VALIDATE_OVERREAD_CHECK(16); b->offset += 16; return 0;
        case DT_BOOLF: // Boolean and NoneType values don't have more bytes, nothing to increment
        case DT_BOOLT:
        case DT_NONTP: return 0;
        default: return 1;
        }
    }
    case DT_ARRAY:
    {
        size_t num_items = 0;
        RD_METADATA(b->msg, b->offset, num_items);
        VALIDATE_OVERREAD_CHECK(0); // Check whether the metadata reading didn't cross the boundary

        for (size_t i = 0; i < num_items; ++i)
            if (_validate(b) == 1) return 1;
        
        return 0;
    }
    case DT_DICTN:
    {
        size_t num_items = 0;
        RD_METADATA(b->msg, b->offset, num_items);
        VALIDATE_OVERREAD_CHECK(0);

        for (size_t i = 0; i < (num_items * 2); ++i)
            if (_validate(b) == 1) return 1;
        
        return 0;
    }
    case DT_EXTND:
    case DT_NOUSE: return 1; // Not supported yet, so currently means invalid
    default:
    {
        // All other cases use VLE metadata, so read length and increment offset using it
        size_t length = 0;
        RD_METADATA(b->msg, b->offset, length);
        VALIDATE_OVERREAD_CHECK(length);
        b->offset += length;
        return 0;
    }
    }
}

PyObject *validate(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value;
    int err_on_invalid = 0;

    static char *kwlist[] = {"value", "error_on_invalid", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|i", kwlist, &PyBytes_Type, &value, &err_on_invalid))
        return NULL;

    buffer_t b;
    b.offset = 0;

    PyBytes_AsStringAndSize(value, &b.msg, (Py_ssize_t *)&b.allocated);

    const int result = _validate(&b);

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

