// This file contains the regular serialization methods

#include <Python.h>

#include "main/serialization.h"

#include "globals/exceptions.h"
#include "globals/buftricks.h"
#include "globals/typemasks.h"
#include "globals/typedefs.h"

#include "settings/allocations.h"

#include "types/usertypes.h"


/* ENCODING */

int offset_check(reg_encode_t *b, const size_t length)
{
    if (b->offset + length >= b->max_offset)
    {
        const size_t new_length = BUF_GET_OFFSET + length + avg_realloc_size;

        char *tmp = (char *)realloc(b->base, new_length);
        if (tmp == NULL)
        {
            PyErr_NoMemory();
            return 1;
        }

        b->offset = tmp + BUF_GET_OFFSET;
        b->max_offset = tmp + new_length;
        b->base = tmp;

        ++(b->reallocs);
    }

    return 0;
}

static inline int encode_container(reg_encode_t *b, PyObject *cont, PyTypeObject *type, const int stream_compatible)
{
    size_t nitems = Py_SIZE(cont);
    size_t initial_alloc;

    if (type == &PyList_Type)
    {
        initial_alloc = (nitems * avg_item_size) + avg_realloc_size;
        b->base = b->offset = (char *)malloc(initial_alloc);
        b->max_offset = b->base + initial_alloc;

        if (b->base == NULL)
        {
            PyErr_NoMemory();
            return 1;
        }

        if (stream_compatible == 0)
        {
            METADATA_VARLEN_WR(DT_ARRAY, nitems);
        }
        else
        {
            __SETBYTE(DT_ARRAY | 0b111);
            memcpy(b->offset, &nitems, 8);

            b->offset += 8;
        }

        for (size_t i = 0; i < nitems; ++i)
            if (encode_object((encode_t *)b, PyList_GET_ITEM(cont, i)) == 1) return 1;
    }
    else
    {
        // Allocate for twice as many items as we have pairs of keys and values
        initial_alloc = (nitems * 2 * avg_item_size) + avg_realloc_size;
        b->base = b->offset = (char *)malloc(initial_alloc);
        b->max_offset = b->base + initial_alloc;

        if (b->base == NULL)
        {
            PyErr_NoMemory();
            return 1;
        }

        if (stream_compatible == 0)
        {
            METADATA_VARLEN_WR(DT_DICTN, nitems);
        }
        else
        {
            __SETBYTE(DT_DICTN | 0b111);
            memcpy(b->offset, &nitems, 8);

            b->offset += 8;
        }

        nitems <<= 1;

        PyObject *key;
        PyObject *val;
        Py_ssize_t pos = 0;

        while (PyDict_Next(cont, &pos, &key, &val))
        {
            if (encode_object((encode_t *)b, key) == 1) return 1;
            if (encode_object((encode_t *)b, val) == 1) return 1;
        }
    }

    update_allocation_settings(b->reallocs, BUF_GET_OFFSET, initial_alloc, nitems);
    return 0;
}

PyObject *encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    /* CUSTOM ARG PARSING */

    /*
      # Args:
      - value;

      # Kwargs:
      - file_name;
      - stream_compatible;
      - custom_types;

    */

    // The `value` is the only non-kwarg and has to be given, so arg size should be 1
    if (PyTuple_GET_SIZE(args) != 1)
    {
        if (PyTuple_GET_SIZE(args) == 0)
            PyErr_SetString(PyExc_ValueError, "Expected at least the 'value' argument");
        else
            PyErr_SetString(PyExc_ValueError, "Only the 'value' argument can be non-keyword");

        return NULL;
    }

    char *filename = NULL;
    utypes_encode_ob *utypes = NULL;
    int stream_compatible = 0;

    // Check if we received kwargs
    if (kwargs != NULL)
    {
        // The amount of kwargs so we can quit if there's no more remaining
        size_t remaining = PyDict_GET_SIZE(kwargs);

        PyObject *py_filename = PyDict_GetItemString(kwargs, "file_name");

        if (py_filename != NULL)
        {
            if (!PyUnicode_Check(py_filename))
            {
                PyErr_Format(PyExc_ValueError, "The 'file_name' argument must be of type 'str', got '%s'", Py_TYPE(py_filename)->tp_name);
                return NULL;
            }

            filename = PyUnicode_AsUTF8(py_filename);

            // Check if the decremented remaining count is zero, exit if so
            if (--remaining == 0)
                goto kwargs_parse_end;
        }

        utypes = (utypes_encode_ob *)PyDict_GetItemString(kwargs, "custom_types");

        if (utypes != NULL && Py_TYPE(utypes) != &utypes_encode_t)
        {
            PyErr_Format(PyExc_ValueError, "The 'custom_types' argument must be of type 'compaqt.CustomWriteTypes', got '%s'", Py_TYPE(utypes)->tp_name);
            return NULL;
        }
        else if (--remaining == 0)
            goto kwargs_parse_end;

        PyObject *py_stream_compatible = PyDict_GetItemString(kwargs, "stream_compatible");

        if (py_stream_compatible != NULL && py_stream_compatible == Py_True)
            stream_compatible = 1;
    }

    // We jump here if all kwargs are parsed
    kwargs_parse_end:

    PyObject *value = PyTuple_GET_ITEM(args, 0);

    /* END OF CUSTOM PARSING */
    
    reg_encode_t b;

    b.reallocs = 0;
    b.bufcheck = (bufcheck_t)offset_check;
    b.utypes = utypes;

    // See if we got a list or dict type
    PyTypeObject *type = Py_TYPE(value);

    if (type == &PyList_Type || type == &PyDict_Type)
    {
        // This manages the initial buffer allocation itself
        if (encode_container(&b, value, type, stream_compatible) == 1)
        {
            free(b.base);
            return NULL;
        }
    }
    else
    {
        b.base = b.offset = b.max_offset = NULL;

        if (encode_object((encode_t *)&b, value) == 1)
        {
            free(b.base);
            return NULL;
        }
    }

    // See if we should write to a file
    if (filename != NULL)
    {
        FILE *file = fopen(filename, "wb");

        if (file == NULL)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to open/create file '%s'", filename);
            free(b.base);
            return NULL;
        }

        fwrite(b.base, 1, (size_t)(b.offset - b.base), file);
        fclose(file);
        
        free(b.base);
        Py_RETURN_NONE;
    }

    // Otherwise return as a Python bytes object
    PyObject *result = PyBytes_FromStringAndSize(b.base, b.offset - b.base);
    
    free(b.base);
    return result;
}

/* DECODING */

static inline int overread_check(decode_t *b, const size_t length)
{
    if (b->offset + length > b->max_offset)
    {
        PyErr_SetString(DecodingError, "Received invalid or corrupted bytes");
        return 1;
    }

    return 0;
}

PyObject *decode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    /* CUSTOM ARG PARSING */

    /*
      # Args:
      - value; (optional, also supported by kwargs)

      # Kwargs:
      - file_name;
      - custom_types;
      - referenced;

    */

    PyObject *value = NULL;
    char *filename = NULL;
    utypes_decode_ob *utypes = NULL;
    int referenced = 0;

    if (PyTuple_GET_SIZE(args) != 0)
    {
        value = PyTuple_GET_ITEM(args, 0);

        if (!PyBytes_Check(value))
        {
            PyErr_Format(PyExc_ValueError, "The 'encoded' argument must be of type 'bytes', got '%s'", Py_TYPE(value)->tp_name);
            return NULL;
        }
    }

    decode_t b;

    if (kwargs != NULL)
    {
        size_t remaining = PyDict_GET_SIZE(kwargs);

        referenced = PyDict_GetItemString(kwargs, "referenced") == Py_True;
        if (referenced == 1 && --remaining == 0)
                goto kwargs_parse_end;

        if (value == NULL)
        {
            value = PyDict_GetItemString(kwargs, "encoded");

            if (value != NULL)
            {
                if (!PyBytes_Check(value))
                {
                    PyErr_Format(PyExc_ValueError, "The 'encoded' argument must be of type 'bytes', got '%s'", Py_TYPE(value)->tp_name);
                    return NULL;
                }
            }
            else
            {
                PyObject *py_filename = PyDict_GetItemString(kwargs, "file_name");

                if (py_filename == NULL)
                {
                    PyErr_SetString(PyExc_ValueError, "Expected either the 'encoded' or 'file_name' argument, got neither");
                    return NULL;
                }
                
                if (!PyUnicode_Check(py_filename))
                {
                    PyErr_Format(PyExc_ValueError, "The 'file_name' argument must be of type 'str', got '%s'", Py_TYPE(py_filename)->tp_name);
                    return NULL;
                }

                filename = PyUnicode_AsUTF8(py_filename);
            }

            if (--remaining == 0)
                goto kwargs_parse_end;
        }
        
        utypes = (utypes_decode_ob *)PyDict_GetItemString(kwargs, "custom_types");

        if (utypes != NULL && Py_TYPE(utypes) != &utypes_decode_t)
        {
            if (Py_TYPE(utypes) != &utypes_decode_t)
            {
                PyErr_Format(PyExc_ValueError, "The 'custom_types' argument must be of type 'compaqt.CustomReadTypes', got '%s'", Py_TYPE(utypes)->tp_name);
                return NULL;
            }
        }
    }

    kwargs_parse_end:

    /* END OF CUSTOM PARSING */

    // Read from the value if one is given
    if (value != NULL)
    {
        Py_ssize_t size;
        PyBytes_AsStringAndSize(value, &b.base, &size);

        if (size == 0)
        {
            PyErr_SetString(PyExc_ValueError, "Received an empty bytes object");
            return NULL;
        }

        b.offset = b.base;
        b.max_offset = b.base + size;
    }
    else
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
            PyErr_Format(FileOffsetError, "Unable to find the end of file '%s'", filename);
            fclose(file);
            return NULL;
        }

        // The amount to allocate is the size of the file
        const size_t size = ftell(file);

        if (size == 0)
        {
            PyErr_SetString(PyExc_ValueError, "Received an empty file");
            return NULL;
        }

        // Go back to the start of the file to read its contents
        if (fseek(file, 0, SEEK_SET) != 0)
        {
            PyErr_Format(FileOffsetError, "Unable to find the start of file '%s'", filename);
            fclose(file);
            return NULL;
        }

        b.base = (char *)malloc(size);
        if (b.base == NULL)
        {
            PyErr_NoMemory();
            fclose(file);
            return NULL;
        }
        
        // Copy the file content into the allocated message buffer
        fread(b.base, 1, size, file);
        fclose(file);

        b.offset = b.base;
        b.max_offset = b.base + size;
    }

    if (referenced == 1)
    {
        b.bufd = (bufdata_t *)PyObject_Malloc(sizeof(bufdata_t));

        if (b.bufd == NULL)
            return PyErr_NoMemory();
        
        if (value == NULL)
        {
            b.bufd->base = b.base;
            b.bufd->is_PyOb = 0;
        }
        else
        {
            Py_INCREF(value);
            b.bufd->base = (void *)value;
            b.bufd->is_PyOb = 1;
        }
        
        b.bufd->refcnt = 0;
    }
    else
    {
        b.bufd = NULL;
    }

    b.bufcheck = (bufcheck_t)overread_check;
    b.utypes = utypes;

    PyObject *result = decode_bytes(&b);

    // Free the buffer if we read from a file AND aren't referencing the buffer
    if (value == NULL && referenced == 0)
        free(b.base);
    
    return result;
}

