// This file contains the streaming methods

#include <Python.h>

#include "main/serialization.h"

#include "globals/exceptions.h"
#include "globals/typemasks.h"
#include "globals/buftricks.h"
#include "globals/typedefs.h"

#include "types/usertypes.h"

// Default chunk size is 32KB
#define DEFAULT_CHUNK_SIZE 1024*32

typedef struct {
    PyObject_HEAD
    stream_encode_t b;
} stream_encode_ob;

typedef struct {
    PyObject_HEAD
    stream_decode_t b;
} stream_decode_ob;

/* ENCODING */

// Function to write the chunk to the file and start a new chunk
static inline int flush_chunk(stream_encode_t *b)
{
    // Write the current buffer
    fwrite(b->base, BUF_GET_OFFSET, 1, b->file);

    b->curr_offset += BUF_GET_OFFSET;
    
    // Start the chunk offset at the base again
    b->offset = b->base;

    return 0;
}

static inline int flush_check(stream_encode_t *b, const size_t length)
{
    if (b->offset + length >= b->max_offset)
    {
        /* Check whether the length doesn't exceed the chunk limit on its own */
        if (length > BUF_GET_LENGTH)
        {
            PyErr_Format(PyExc_ValueError, "Needed at least %zu bytes in the chunk buffer, while the limit was set to %zu", length, BUF_GET_LENGTH);
            return 1;
        }
        
        if (flush_chunk(b) == 1) return 1;
    }

    return 0;
}

// Encode a list type
static inline int encode_list(stream_encode_t *b, PyObject *value)
{
    const size_t nitems = PyList_GET_SIZE(value);
    
    for (size_t i = 0; i < nitems; ++i)
        if (encode_object((encode_t *)b, PyList_GET_ITEM(value, i)) == 1) return 1;

    b->nitems += nitems;
    return 0;
}

static inline int encode_dict(stream_encode_t *b, PyObject *value)
{
    Py_ssize_t pos = 0;
    PyObject *key, *val;

    while (PyDict_Next(value, &pos, &key, &val))
        if (encode_object((encode_t *)b, key) == 1 || encode_object((encode_t *)b, val) == 1) return 1;

    b->nitems += PyDict_GET_SIZE(value);
    return 0;
}

#define CLEAR_MEMORY do { \
    if (clear_memory == 1) \
    { \
        free(b->base); \
        b->base = NULL; \
    } \
} while (0)

static PyObject *update_encoder(stream_encode_ob *ob, PyObject *args, PyObject *kwargs)
{
    PyObject *value;
    int clear_memory = 0;
    Py_ssize_t chunk_size = 0;
    
    static char *kwlist[] = {"value", "clear_memory", "chunk_size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|in", kwlist, &value, &clear_memory, &chunk_size))
        return NULL;

    stream_encode_t *b = &ob->b;

    // Check if the chunk size was changed
    if (chunk_size > 0)
    {
        // Don't use realloc as we don't need to preserve data
        free(b->base);
        b->base = (char *)malloc(chunk_size);
        b->chunk_size = chunk_size;

        if (b->base == NULL)
            return PyErr_NoMemory();
    }
    else if (b->base == NULL)
    {
        b->base = (char *)malloc(b->chunk_size);

        if (b->base == NULL)
            return PyErr_NoMemory();
    }

    BUF_INIT_OFFSETS(b->chunk_size);

    // Open the file for writing in binary append mode
    b->file = fopen(b->filename, "ab");
    if (b->file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to open file '%s'", b->filename);
        return NULL;
    }

    int status;
    PyTypeObject *type = Py_TYPE(value);
    if (b->type == type)
    {
        if (type == &PyList_Type)
            status = encode_list(b, value);
        else
            status = encode_dict(b, value);
    }
    else
    {
        PyErr_Format(PyExc_ValueError, "Streaming mode requires values to continue as the same type. Started with type '%s', got '%s'", b->type->tp_name, type->tp_name);
        fclose(b->file);
        return NULL;
    }

    if (status == 1)
    {
        fclose(b->file);
        return NULL;
    }

    // Write the last changes
    fwrite(b->base, BUF_GET_OFFSET, 1, b->file);
    fclose(b->file);

    // Re-open the file in r+ to write to the metadata bytes
    b->file = fopen(b->filename, "r+");
    if (b->file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to open file '%s''", b->filename);
        fclose(b->file);
        return NULL;
    }

    if (fseek(b->file, b->start_offset + 1, SEEK_SET) != 0)
    {
        PyErr_Format(FileOffsetError, "Unable to set the file offset to %zu\n", b->curr_offset);
        fclose(b->file);
        return NULL;
    }

    // Write the number of items metadata to the file
    char nitems_buf[8];
    memcpy(nitems_buf, &b->nitems, 8);

    fwrite(nitems_buf, 8, 1, b->file);
    fclose(b->file);

    b->curr_offset += BUF_GET_OFFSET;

    CLEAR_MEMORY;
    Py_RETURN_NONE;
}

static void encoder_dealloc(stream_encode_ob *ob)
{
    stream_encode_t b = ob->b;

    free(b.filename);
    free(b.base);

    PyObject_Del(ob);
}

static PyObject *start_offset_encoder(stream_encode_ob *ob)
{
    return PyLong_FromSize_t(ob->b.start_offset);
}

static PyObject *total_offset_encoder(stream_encode_ob *ob)
{
    return PyLong_FromSize_t(ob->b.curr_offset);
}

static PyGetSetDef stream_encoder_getset[] = {
    {"start_offset", (getter)start_offset_encoder, NULL, "The initial file offset the encoder started writing to", NULL},
    {"curr_offset", (getter)total_offset_encoder, NULL, "The total file offset the encoder is currently at", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef stream_encoder_methods[] = {
    {"write", (PyCFunction)update_encoder, METH_VARARGS | METH_KEYWORDS, "Update the stream encoder with new data"},
    {NULL, NULL, 0, NULL}
};

PyTypeObject stream_encoder_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.StreamEncoder",
    .tp_basicsize = sizeof(stream_encode_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = stream_encoder_methods,
    .tp_dealloc = (destructor)encoder_dealloc,
    .tp_getset = stream_encoder_getset,
};

// Init function for encoder objects
PyObject *get_stream_encoder(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *filename;
    PyTypeObject *value_type = &PyList_Type;
    size_t chunk_size = DEFAULT_CHUNK_SIZE;
    utypes_encode_ob *utypes;
    int resume_stream = 0;
    int preserve_file = 0;
    size_t start_offset = 0;

    static char *kwlist[] = {"file_name", "value_type", "chunk_size", "custom_types", "resume_stream", "file_offset", "preserve_file", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|OnO!ini", kwlist, &filename, (PyObject **)&value_type, (Py_ssize_t *)&chunk_size, &utypes_encode_t, &utypes, &resume_stream, (Py_ssize_t *)&start_offset, &preserve_file))
        return NULL;

    if (value_type != &PyList_Type && value_type != &PyDict_Type)
    {
        PyErr_Format(PyExc_ValueError, "Streaming mode only supports initialization with list/dict types, got '%s'", ((PyTypeObject *)value_type)->tp_name);
        return NULL;
    }

    stream_encode_ob *ob = PyObject_New(stream_encode_ob, &stream_encoder_t);

    if (ob == NULL)
        return PyErr_NoMemory();
    
    stream_encode_t *b = &ob->b;

    b->filename = (char *)malloc(strlen(filename) + 1);
    b->base = NULL;

    if (b->filename == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }
    
    memcpy(b->filename, filename, strlen(filename) + 1);

    b->chunk_size = chunk_size;
    b->start_offset = start_offset;
    b->curr_offset = start_offset + MAX_METADATA_SIZE;
    b->utypes = utypes;
    b->bufcheck = (bufcheck_t)flush_check;

    // Check if we need to resume a previous stream
    if (resume_stream == 1)
    {
        // Open the file in binary read mode as we don't have to write, and we have to read the current number of items
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
            Py_DECREF(b);
            return NULL;
        }

        if (fseek(file, b->start_offset, SEEK_SET) != 0)
        {
            PyErr_Format(FileOffsetError, "Unable to set the file offset to %zu\n", b->start_offset);
            fclose(file);
            Py_DECREF(b);
            return NULL;
        }

        // Buffer to write the current metadata to
        char buf[9];
        if (fread(buf, 9, 1, file) == 0)
        {
            PyErr_Format(FileOffsetError, "Failed to read the file from offset %zu", b->start_offset);
            fclose(file);
            Py_DECREF(b);
            return NULL;
        }

        // Read the container datatype
        const char tpmask = *buf & 0b00000111;

        // The mode-3 8-byte length metadata is equal to `0b11111000`
        if ((*buf & 0b11111000) != 0b11111000 || (tpmask != DT_ARRAY && tpmask != DT_DICTN))
        {
            PyErr_SetString(PyExc_ValueError, "The existing file data does not match the encoding stream expectations");
            fclose(file);
            Py_DECREF(b);
            return NULL;
        }
        
        b->type = tpmask == DT_ARRAY ? &PyList_Type : &PyDict_Type;
        memcpy(&b->nitems, buf + 1, 8);

        fclose(file);
    }
    else
    {
        FILE *file;

        if (preserve_file == 1)
        {
            // Open in append mode and set the metadata offset to the current end of the file
            file = fopen(filename, "ab");
            b->curr_offset = ftell(file);
            if (file == NULL)
            {
                PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
                Py_DECREF(ob);
                return NULL;
            }
        }
        else
        {
            // Open in binary write mode to overwrite any existing data
            file = fopen(filename, "wb");
            if (file == NULL)
            {
                PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
                Py_DECREF(ob);
                return NULL;
            }

            // Set the file to the stream offset
            if (fseek(file, b->start_offset, SEEK_SET) != 0)
            {
                PyErr_Format(FileOffsetError, "Unable to set the file offset to %zu\n", b->start_offset);
                fclose(file);
                Py_DECREF(ob);
                return NULL;
            }
        }

        /*  Write the extendable metadata to the file in advance. Initialize it with
         *  zero items; that number will be updated as new values are written.
         */

        const unsigned char tpmask = value_type == &PyList_Type ? DT_ARRAY : DT_DICTN;

        char buf[9];
        b->offset = buf; // Set the buffer to the struct for metadata method compatability
        
        METADATA_VARLEN_WR_MODE3(tpmask, 0, 8);

        fwrite(buf, 9, 1, file);
        fclose(file);

        b->type = value_type;
        b->nitems = 0;
    }

    return (PyObject *)ob;
}

/* DECODING */

static inline int refresh_chunk(stream_decode_t *b)
{
    // Update the total offset and reset the chunk offset
    b->curr_offset += BUF_GET_OFFSET;
    b->offset = b->base;

    // Go to the reading point of the file
    if (fseek(b->file, b->curr_offset, SEEK_SET) != 0)
    {
        PyErr_Format(FileOffsetError, "Failed to open the file at offset %zu", b->curr_offset);
        return 1;
    }

    // Set the chunk size to the number of items read, so that it gets smaller if the file limit is reached.
    // It doesn't matter that this overrides the user value, as the end of the file is reached and thus only the smaller chunk size is needed
    if ((b->max_offset = b->base - fread(b->base, 1, b->chunk_size, b->file)) == b->base)
    {
        PyErr_Format(FileOffsetError, "Failed to read the file from offset %zu", b->curr_offset);
        return 1;
    }

    return 0;
}

static inline int chunk_refresh_check(stream_decode_t *b, const size_t length)
{
    if (b->offset + length > b->max_offset)
    {
        if (length > b->chunk_size)
        {
            PyErr_Format(PyExc_ValueError, "Found a value that requires %zu bytes to store, while the chunk limit is %s", length, b->chunk_size);
            return 1;
        }

        if (refresh_chunk(b) == 1) return 1;
    }

    return 0;
}

static inline PyObject *decode_list(stream_decode_t *b, const size_t nitems)
{
    PyObject *list = PyList_New(nitems);

    if (list == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    for (size_t i = 0; i < nitems; ++i)
    {
        PyObject *val = decode_bytes((decode_t *)b);

        if (val == NULL)
        {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SET_ITEM(list, i, val);
    }

    b->nitems -= nitems;

    return list;
}

static inline PyObject *decode_dict(stream_decode_t *b, const size_t nitems)
{
    PyObject *dict = PyDict_New();

    if (dict == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    for (size_t i = 0; i < nitems; ++i)
    {
        PyObject *key = decode_bytes((decode_t *)b);

        if (key == NULL)
        {
            Py_DECREF(dict);
            return NULL;
        }

        PyObject *val = decode_bytes((decode_t *)b);

        if (val == NULL)
        {
            Py_DECREF(dict);
            Py_DECREF(key);
            return NULL;
        }

        PyDict_SetItem(dict, key, val);
    }

    b->nitems -= nitems;

    return dict;
}

static PyObject *update_decoder(stream_decode_ob *ob, PyObject *args, PyObject *kwargs)
{
    stream_decode_t *b = &ob->b;

    size_t nitems = b->nitems;
    int clear_memory = 0;
    size_t chunk_size = 0;

    static char *kwlist[] = {"num_items", "clear_memory", "chunk_size", NULL};

    PyArg_ParseTupleAndKeywords(args, kwargs, "|nin", kwlist, (Py_ssize_t *)&nitems, &clear_memory, (Py_ssize_t *)&chunk_size);

    // Limit the number of items to the max available. Don't throw error as the items-remaining variable will state 0 remaining
    if (nitems > b->nitems)
        nitems = b->nitems;
    
    // Return an empty object is the number of items to read is zero
    if (nitems == 0)
        return b->type == &PyList_Type ? PyList_New(0) : PyDict_New();
    
    // Check if the chunk size was changed
    if (chunk_size != 0)
    {
        b->chunk_size = chunk_size;

        // Don't use realloc as we don't need to copy existing data along
        free(b->base);
        b->base = (char *)malloc(b->chunk_size);

        if (b->base == NULL)
            return PyErr_NoMemory();
    }
    else if (b->base == NULL)
    {
        b->base = (char *)malloc(b->chunk_size);

        if (b->base == NULL)
            return PyErr_NoMemory();
    }

    BUF_INIT_OFFSETS(b->chunk_size);

    b->file = fopen(b->filename, "rb");

    if (b->file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to open file '%s'", b->filename);
        return NULL;
    }

    // Go to the current stream offset to read from where we left off
    if (fseek(b->file, b->curr_offset, SEEK_SET) != 0)
    {
        PyErr_Format(FileOffsetError, "Failed to open the file at offset %zu", b->curr_offset);
        fclose(b->file);
        return NULL;
    }

    // Copy the first chunk into the message buffer
    if (fread(b->base, 1, b->chunk_size, b->file) == 0)
    {
        PyErr_Format(FileOffsetError, "Failed to read the file from offset %zu", b->curr_offset);
        fclose(b->file);
        return NULL;
    }

    PyObject *result;

    if (b->type == &PyList_Type)
        result = decode_list(b, nitems);
    else
        result = decode_dict(b, nitems);
    
    b->curr_offset += BUF_GET_OFFSET;

    CLEAR_MEMORY;
    return result;
}

static void decoder_dealloc(stream_decode_ob *ob)
{
    stream_decode_t b = ob->b;

    free(b.filename);
    free(b.base);

    PyObject_Del(ob);
}

static PyObject *items_remaining_decoder(stream_decode_ob *ob)
{
    return PyLong_FromSize_t(ob->b.nitems);
}

static PyObject *start_offset_decoder(stream_encode_ob *ob)
{
    return PyLong_FromSize_t(ob->b.start_offset);
}

static PyObject *curr_offset_decoder(stream_encode_ob *ob)
{
    return PyLong_FromSize_t(ob->b.curr_offset);
}

static PyGetSetDef stream_decoder_getset[] = {
    {"start_offset", (getter)start_offset_decoder, NULL, "The offset the decoder started reading from", NULL},
    {"curr_offset", (getter)curr_offset_decoder, NULL, "The total file offset the decoder is currently at", NULL},
    {"items_remaining", (getter)items_remaining_decoder, NULL, "The amount of items remaining to be read", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef stream_decoder_methods[] = {
    {"read", (PyCFunction)update_decoder, METH_VARARGS | METH_KEYWORDS, "De-serialize data from the stream decoder"},
    {NULL, NULL, 0, NULL}
};

PyTypeObject stream_decoder_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.StreamDecoder",
    .tp_basicsize = sizeof(stream_decode_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = stream_decoder_methods,
    .tp_dealloc = (destructor)decoder_dealloc,
    .tp_getset = stream_decoder_getset,
};

// Init function for encoder objects
PyObject *get_stream_decoder(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *filename;
    size_t chunk_size = DEFAULT_CHUNK_SIZE;
    utypes_decode_ob *utypes = NULL;
    size_t stream_offset = 0;

    static char *kwlist[] = {"file_name", "chunk_size", "custom_types", "file_offset", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|nO!n", kwlist, &filename, (Py_ssize_t *)&chunk_size, &utypes_decode_t, &utypes, (Py_ssize_t *)&stream_offset))
        return NULL;
    
    stream_decode_ob *ob = PyObject_New(stream_decode_ob, &stream_decoder_t);

    if (ob == NULL)
        return PyErr_NoMemory();
    
    stream_decode_t *b = &ob->b;

    b->filename = (char *)malloc(strlen(filename) + 1);
    b->base = NULL;

    if (b->filename == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }

    memcpy(b->filename, filename, strlen(filename) + 1);

    b->start_offset = stream_offset;
    b->chunk_size = chunk_size;
    b->utypes = utypes;
    b->bufcheck = (bufcheck_t)chunk_refresh_check;
    b->bufd = NULL;

    // Open the file in binary read mode to read the current number of items
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
        Py_DECREF(ob);
        return NULL;
    }

    if (fseek(file, stream_offset, SEEK_SET) != 0)
    {
        PyErr_Format(FileOffsetError, "Unable to set the file offset to offset %zu\n", stream_offset);
        fclose(file);
        Py_DECREF(ob);
        return NULL;
    }

    // Read the type and current number of items
    char buf[9];
    if (fread(buf, 9, 1, file) == 0)
    {
        PyErr_Format(FileOffsetError, "Failed to read the file from offset %zu", stream_offset);
        fclose(file);
        Py_DECREF(ob);
        return NULL;
    }
    fclose(file);

    // Get which datatype we have stored
    const char tpmask = buf[0] & 0b111;

    if (tpmask == DT_ARRAY)
        b->type = &PyList_Type;
    else if (tpmask == DT_DICTN)
        b->type = &PyDict_Type;
    else
    {
        PyErr_SetString(PyExc_ValueError, "Encoded data must start with a list or dict object for stream objects");
        Py_DECREF(ob);
        return NULL;
    }

    // Set the buffer to the offset of the decode struct for using it in the metadata macro
    b->offset = buf;
    METADATA_VARLEN_RD(b->nitems);

    // Calculate the current offset based on the increment of the offset
    b->curr_offset = b->start_offset + ((size_t)b->offset - (size_t)buf);

    return (PyObject *)ob;
}

