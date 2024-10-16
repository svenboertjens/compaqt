// This file contains the regular serialization methods

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

