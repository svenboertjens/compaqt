// This file contains the regular serialization methods

#include <Python.h>
#include "metadata.h"
#include "main/serialization.h"
#include "exceptions.h"

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
            return 1;
        }
        b->allocated = new_length;
        b->msg = tmp;
    }

    return 0;
}

static inline int encode_container(encode_t *b, PyObject *cont, PyTypeObject *type, custom_types_wr_ob *custom_ob, const int stream_compatible)
{
    size_t num_items = Py_SIZE(cont);
    size_t initial_alloc;

    if (type == &PyList_Type)
    {
        initial_alloc = num_items * avg_item_size + avg_realloc_size;
        b->msg = (char *)malloc(initial_alloc);
        b->allocated = initial_alloc;

        if (b->msg == NULL)
        {
            PyErr_NoMemory();
            return 1;
        }

        if (stream_compatible == 0)
        {
            WR_METADATA(b->msg, b->offset, DT_ARRAY, num_items);
        }
        else
        {
            // Write LM2 metadata if it needs to be streaming compatible
            WR_METADATA_LM2_MASK(b->msg, b->offset, DT_ARRAY, 8);
            WR_METADATA_LM2(b->msg, b->offset, num_items, 8);
        }

        for (size_t i = 0; i < num_items; ++i)
            if (encode_item((buffer_t *)b, PyList_GET_ITEM(cont, i), custom_ob, offset_check) == 1) return 1;
    }
    else
    {
        // Allocate for twice as many items as we have pairs of keys and values
        initial_alloc = (num_items << 1) * avg_item_size + avg_realloc_size;
        b->msg = (char *)malloc(initial_alloc);
        b->allocated = initial_alloc;

        if (b->msg == NULL)
        {
            PyErr_NoMemory();
            return 1;
        }

        if (stream_compatible == 0)
        {
            WR_METADATA(b->msg, b->offset, DT_DICTN, num_items);
        }
        else
        {
            // Write LM2 metadata if it needs to be streaming compatible
            WR_METADATA_LM2_MASK(b->msg, b->offset, DT_DICTN, 8);
            WR_METADATA_LM2(b->msg, b->offset, num_items, 8);
        }

        num_items <<= 1;

        PyObject *key;
        PyObject *val;
        Py_ssize_t pos = 0;

        while (PyDict_Next(cont, &pos, &key, &val))
        {
            if (encode_item((buffer_t *)b, key, custom_ob, offset_check) == 1) return 1;
            if (encode_item((buffer_t *)b, val, custom_ob, offset_check) == 1) return 1;
        }
    }

    update_allocation_settings(b->reallocs, b->offset, initial_alloc, num_items);
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
    custom_types_wr_ob *custom_ob = NULL;
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

        custom_ob = (custom_types_wr_ob *)PyDict_GetItemString(kwargs, "custom_types");

        if (custom_ob != NULL && Py_TYPE(custom_ob) != &custom_types_wr_t)
        {
            PyErr_Format(PyExc_ValueError, "The 'custom_types' argument must be of type 'compaqt.CustomWriteTypes', got '%s'", Py_TYPE(custom_ob)->tp_name);
            return NULL;
        }
        else if (--remaining == 0)
            goto kwargs_parse_end;

        PyObject *py_stream_compatible = PyDict_GetItemString(kwargs, "stream_compatible");

        if (py_stream_compatible != NULL && Py_IsTrue(py_stream_compatible))
            stream_compatible = 1;
    }

    // We jump here if all kwargs are parsed
    kwargs_parse_end:

    PyObject *value = PyTuple_GET_ITEM(args, 0);

    /* END OF CUSTOM PARSING */
    
    encode_t b;

    b.offset = 0;
    b.reallocs = 0;

    // See if we got a list or dict type
    PyTypeObject *type = Py_TYPE(value);

    if (type == &PyList_Type || type == &PyDict_Type)
    {
        // This manages the initial buffer allocation itself
        if (encode_container(&b, value, type, custom_ob, stream_compatible) == 1)
        {
            free(b.msg);
            return NULL;
        }
    }
    else
    {
        // Set the message to NULL and 0 allocation space to let `encode_item` handle the initial alloc using its overwrite checks
        // Realloc accepts NULL and will treat it as a malloc call
        b.msg = NULL;
        b.allocated = 0;

        if (encode_item((buffer_t *)(&b), value, custom_ob, offset_check) == 1)
        {
            free(b.msg);
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
        PyErr_SetString(DecodingError, "Received an invalid or corrupted bytes string");
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

    */

    PyObject *value = NULL;
    char *filename = NULL;
    custom_types_rd_ob *custom_ob = NULL;

    if (PyTuple_GET_SIZE(args) != 0)
    {
        value = PyTuple_GET_ITEM(args, 0);

        if (!PyBytes_Check(value))
        {
            PyErr_Format(PyExc_ValueError, "The 'encoded' argument must be of type 'bytes', got '%s'", Py_TYPE(value)->tp_name);
            return NULL;
        }
    }

    if (kwargs != NULL)
    {
        size_t remaining = PyDict_GET_SIZE(kwargs);

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
        
        custom_ob = (custom_types_rd_ob *)PyDict_GetItemString(kwargs, "custom_types");

        if (custom_ob != NULL && Py_TYPE(custom_ob) != &custom_types_rd_t)
        {
            PyErr_Format(PyExc_ValueError, "The 'custom_types' argument must be of type 'compaqt.CustomReadTypes', got '%s'", Py_TYPE(custom_ob)->tp_name);
            return NULL;
        }
    }

    kwargs_parse_end:

    /* END OF CUSTOM PARSING */
    
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
            PyErr_Format(FileOffsetError, "Unable to find the end of file '%s'", filename);
            fclose(file);
            return NULL;
        }

        // The amount to allocate is the size of the file
        b.allocated = ftell(file);

        // Go back to the start of the file to read its contents
        if (fseek(file, 0, SEEK_SET) != 0)
        {
            PyErr_Format(FileOffsetError, "Unable to find the start of file '%s'", filename);
            fclose(file);
            return NULL;
        }

        b.msg = (char *)malloc(b.allocated);
        if (b.msg == NULL)
        {
            PyErr_NoMemory();
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

    PyObject *result = decode_item(&b, custom_ob, overread_check);

    if (value == NULL)
        free(b.msg);
    
    return result;
}

