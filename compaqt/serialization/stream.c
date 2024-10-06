// This file contains the streaming methods

#include <Python.h>
#include "globals.h"

// Stream object class
typedef struct {
    char *msg;
    size_t offset;
    size_t chunk_size;
    char *filename;
    size_t num_items;
    PyTypeObject *type;
    size_t stream_offset;
} stream_t;

typedef struct {
    PyObject_HEAD
    stream_t *s;
} PyStreamEncoderObject;

static void encoder_dealloc(PyStreamEncoderObject *stream_obj)
{
    stream_t *s = stream_obj->s;

    if (s != NULL)
    {
        free(s->filename);
        free(s->msg);
        free(s);

        stream_obj->s = NULL;
    }

    Py_TYPE(stream_obj)->tp_free((PyObject *)stream_obj);
}

// Function to write the chunk to the file and start a new chunk
static inline int flush_chunk(stream_t *s, FILE *file)
{
    // Write the current buffer
    fwrite(s->msg, s->offset, 1, file);
    // Start the chunk offset at 0 again
    s->offset = 0;

    return 0;
}

#define CHUNK_LIMIT_CHECK(length) do { \
    if (s->offset + length > s->chunk_size) \
    { \
        /* Check whether the length doesn't exceed the chunk limit on its own */ \
        if (length > s->chunk_size) \
        { \
            PyErr_Format(PyExc_ValueError, "Found a value that requires %zu bytes to store, while the chunk limit is %s", length, s->chunk_size); \
            return 1; \
        } \
        \
        if (flush_chunk(s, file) == 1) return 1; \
    } \
} while (0)

#define DT_CHECK(expected) do { \
    if (type != &expected) \
    { \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        return 1; \
    } \
} while (0)

static inline int encode_item(stream_t *s, FILE *file, PyObject *item)
{
    PyTypeObject *type = Py_TYPE(item);
    const char *tp_name = type->tp_name;

    switch (*tp_name)
    {
    case 'b': // Bytes / Boolean
    {
        if (tp_name[1] == 'y') // Bytes
        {
            DT_CHECK(PyBytes_Type);

            const size_t length = bytes_ln(item);

            CHUNK_LIMIT_CHECK(length + MAX_VLE_METADATA_SIZE);
            WR_METADATA_VLE(s, DT_BYTES, length);

            bytes_wr(s, item, length);
        }
        else // Boolean
        {
            CHUNK_LIMIT_CHECK(1);
            bool_wr(s, item);
        }
        break;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        const size_t length = string_ln(item);

        CHUNK_LIMIT_CHECK(length + MAX_VLE_METADATA_SIZE);
        WR_METADATA_VLE(s, DT_STRNG, length);

        string_wr(s, item, length);
        break;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);

        const size_t length = integer_ln(item);

        CHUNK_LIMIT_CHECK(length + MAX_VLE_METADATA_SIZE);
        WR_METADATA_VLE(s, DT_INTGR, length);

        integer_wr(s, item, length);
        break;
    }
    case 'f': // Float
    {
        DT_CHECK(PyFloat_Type);
        CHUNK_LIMIT_CHECK(9);

        *(s->msg + s->offset++) = DT_FLOAT;

        float_wr(s, item);
        break;
    }
    case 'c': // Complex
    {
        DT_CHECK(PyComplex_Type);
        CHUNK_LIMIT_CHECK(17);

        *(s->msg + s->offset++) = DT_CMPLX;

        complex_wr(s, item);
        break;
    }
    case 'N': // NoneType
    {
        *(s->msg + s->offset++) = DT_NONTP;
        break;
    }
    case 'l': // List
    {
        DT_CHECK(PyList_Type);
        CHUNK_LIMIT_CHECK(MAX_VLE_METADATA_SIZE);

        const size_t num_items = (size_t)(PyList_GET_SIZE(item));
        WR_METADATA_VLE(s, DT_ARRAY, num_items);

        for (size_t i = 0; i < num_items; ++i)
            if (encode_item(s, file, PyList_GET_ITEM(item, i)) == 1) return 1;
        
        break;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);
        CHUNK_LIMIT_CHECK(MAX_VLE_METADATA_SIZE);

        WR_METADATA_VLE(s, DT_DICTN, PyDict_GET_SIZE(item));

        Py_ssize_t pos = 0;
        PyObject *key, *val;
        while (PyDict_Next(item, &pos, &key, &val))
        {
            if (encode_item(s, file, key) == 1) return 1;
            if (encode_item(s, file, val) == 1) return 1;
        }
        break;
    }
    default: // Unsupported datatype
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name);
        return 1;
    }
    }

    return 0;
}

// Encode a list type
static inline void encode_list(stream_t *s, FILE *file, PyObject *value)
{
    const size_t num_items = PyList_GET_SIZE(value);
    
    for (size_t i = 0; i < num_items; ++i)
    {
        if (encode_item(s, file, PyList_GET_ITEM(value, i)) == 1)
        {
            s->offset = 0;
            return;
        }
    }

    s->num_items += num_items;
}

static inline void encode_dict(stream_t *s, FILE *file, PyObject *value)
{
    const size_t num_items = PyDict_GET_SIZE(value);

    Py_ssize_t pos = 0;
    PyObject *key, *val;
    while (PyDict_Next(value, &pos, &key, &val))
    {
        if (encode_item(s, file, key) == 1 || encode_item(s, file, val) == 1)
        {
            s->offset = 0;
            return;
        }
    }

    s->num_items += num_items;
}

#define CLEAR_MEMORY do { \
    if (clear_memory == 1) \
    { \
        free(s->msg); \
        s->msg = NULL; \
    } \
} while (0)

static inline int write_metadata(stream_t *s)
{
    FILE *file = fopen(s->filename, "r+");

    if (file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", s->filename);
        return 1;
    }

    fseek(file, s->stream_offset + 1, SEEK_SET);

    // Construct MODE 2 metadata length for it to be readable by the global metadata functions
    char buf[8];
    buf[0] = s->num_items & 0xFF;
    buf[1] = s->num_items >> (8 * 1);
    buf[2] = s->num_items >> (8 * 7);
    buf[3] = s->num_items >> (8 * 6);
    buf[4] = s->num_items >> (8 * 5);
    buf[5] = s->num_items >> (8 * 4);
    buf[6] = s->num_items >> (8 * 3);
    buf[7] = s->num_items >> (8 * 2);

    fwrite(buf, 8, 1, file);
    fclose(file);

    return 0;
}

static PyObject *update_encoder(PyStreamEncoderObject *stream_obj, PyObject *args, PyObject *kwargs)
{
    PyObject *value;
    int clear_memory = 0;
    Py_ssize_t chunk_size = 0;

    static char *kwlist[] = {"value", "clear_memory", "chunk_size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|in", kwlist, &value, &clear_memory, &chunk_size))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the 'value' (any) argument");
        return NULL;
    }

    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamEncoder object");
        return NULL;
    }

    if (chunk_size != 0)
    {
        s->chunk_size = chunk_size;
        // Free the current chunk and set it to NULL to allocate with new chunk size
        free(s->msg);
        s->msg = NULL;
    }

    if (s->msg == NULL)
    {
        s->msg = (char *)malloc(s->chunk_size);
        if (s->msg == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }
    }

    // Open the file for writing in binary append mode
    FILE *file = fopen(s->filename, "ab");
    if (file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", s->filename);
        return NULL;
    }

    // Set the chunk offset to 0
    s->offset = 0;

    // We only support container types for streaming
    PyTypeObject *type = Py_TYPE(value);
    if (s->type == type)
    {
        if (type == &PyList_Type)
            encode_list(s, file, value);
        else if (type == &PyDict_Type)
            encode_dict(s, file, value);
        else
        {
            PyErr_Format(PyExc_ValueError, "Streaming mode only supports list/dict types, got '%s'", type->tp_name);
            CLEAR_MEMORY;
            fclose(file);
            return NULL;
        }
    }
    else
    {
        PyErr_Format(PyExc_ValueError, "Streaming mode requires values to continue as the same type. Started with type '%s', got '%s'", s->type->tp_name, type->tp_name);
        CLEAR_MEMORY;
        fclose(file);
        return NULL;
    }

    // Write the last changes
    fwrite(s->msg, s->offset, 1, file);
    fclose(file);

    if (write_metadata(s) == 1) return NULL;

    CLEAR_MEMORY;
    Py_RETURN_NONE;
}

static PyObject *finalize_encoder(PyStreamEncoderObject *stream_obj, PyObject *args)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamEncoder object");
        return NULL;
    }

    if (write_metadata(s) == 1) return NULL;

    free(s->msg);
    free(s->filename);
    free(s);

    stream_obj->s = NULL;

    Py_RETURN_NONE;
}

static PyMethodDef PyStreamEncoderMethods[] = {
    {"update", (PyCFunction)update_encoder, METH_VARARGS | METH_KEYWORDS, "Update the stream encoder with new data"},
    {"finalize", (PyCFunction)finalize_encoder, METH_NOARGS, "Finalize the stream encoder and clean it up"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject PyStreamEncoderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.StreamEncoder",
    .tp_basicsize = sizeof(PyStreamEncoderObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = PyStreamEncoderMethods,
    .tp_dealloc = (destructor)encoder_dealloc,
};

// Use a max of 8 MB per chunk by default
#define CHUNK_SIZE_DEFAULT (1024 * 1024 * 8)

// Init function for encoder objects
PyObject *get_stream_encoder(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *filename;
    PyTypeObject *value_type;
    size_t chunk_size = CHUNK_SIZE_DEFAULT;
    int resume_stream = 0;
    int preserve_file = 0;
    size_t stream_offset = 0;

    static char *kwlist[] = {"file_name", "value_type", "chunk_size", "resume_stream", "file_offset", "preserve_file", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|nini", kwlist, &filename, (PyObject **)&value_type, (Py_ssize_t *)&chunk_size, &resume_stream, (Py_ssize_t *)&stream_offset, &preserve_file))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the `file_name` (str) and `value_type` (type) argument");
        return NULL;
    }

    if (value_type != &PyList_Type && value_type != &PyDict_Type)
    {
        PyErr_SetString(PyExc_ValueError, "Streaming mode only supports initialization with list/dict types");
        return NULL;
    }

    stream_t *s = (stream_t *)malloc(sizeof(stream_t));

    if (s == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    s->chunk_size = chunk_size;
    s->type = value_type;
    s->msg = NULL;

    // Check if we need to resume a previous stream
    if (resume_stream == 1)
    {
        // Open the file in binary read mode as we don't have to write, and we have to read the current number of items
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
            free(s);
            return NULL;
        }

        if (fseek(file, stream_offset + 1, SEEK_SET) != 0)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to set the file offset to offset %zu\n", stream_offset);
            fclose(file);
            free(s);
            return NULL;
        }

        // Read the current number of items
        char buf[8];
        fread(buf, 8, 1, file);

        // Rebuild the value in the metadata MODE 2 format
        s->num_items = buf[0];
        s->num_items |= buf[1] << (8 * 1);
        s->num_items |= buf[2] << (8 * 2);
        s->num_items |= buf[3] << (8 * 3);
        s->num_items |= buf[4] << (8 * 4);
        s->num_items |= buf[5] << (8 * 5);
        s->num_items |= buf[6] << (8 * 6);
        s->num_items |= buf[7] << (8 * 7);

        s->stream_offset = stream_offset;

        fclose(file);
    }
    else
    {
        FILE *file;

        if (preserve_file == 1)
        {
            // Open in append mode and set the metadata offset to the current end of the file
            file = fopen(filename, "ab");
            s->stream_offset = ftell(file);
            if (file == NULL)
            {
                PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
                free(s);
                return NULL;
            }
        }
        else
        {
            // Open in binary write mode to extend and overwrite any existing data
            file = fopen(filename, "wb");
            if (file == NULL)
            {
                PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
                free(s);
                return NULL;
            }

            // Set the file to the received stream offset to follow
            if (fseek(file, stream_offset, SEEK_SET) != 0)
            {
                PyErr_Format(PyExc_FileNotFoundError, "Unable to set the file offset to %zu\n", (size_t)stream_offset);
                fclose(file);
                free(s);
                return NULL;
            }
            s->stream_offset = stream_offset;
        }

        // Write the first metadata byte in the way in MODE 2 format
        char buf[9];
        buf[0] = (value_type == &PyList_Type ? DT_ARRAY : DT_DICTN) | 0b00011000 | (6 << 5);
        memset(buf + 1, 0, 8); // Set the 8 length bytes to zeroes as there aren't any items yet

        fwrite(&buf, 9, 1, file);
        fclose(file);

        s->num_items = 0;
    }

    // Set the filename of the stream to write to
    s->filename = (char *)malloc(strlen(filename) + 1);
    if (s->filename == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }
    // Copy the filename, plus 1 also copies the NULL-terminator
    memcpy(s->filename, filename, strlen(filename) + 1);

    PyStreamEncoderObject *stream_obj = PyObject_New(PyStreamEncoderObject, &PyStreamEncoderType);

    if (stream_obj == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    stream_obj->s = s;

    return (PyObject *)stream_obj;
}

