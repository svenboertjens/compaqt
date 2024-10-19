// This file contains the streaming methods

#include <Python.h>
#include "metadata.h"
#include "serialization.h"

// Stream object class
typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
    FILE *file;
    char *filename;
    size_t num_items;
    PyTypeObject *type;
    size_t stream_offset;
} stream_t;

typedef struct {
    PyObject_HEAD
    stream_t *s;
} PyStreamEncoderObject;

typedef struct {
    PyObject_HEAD
    stream_t *s;
} PyStreamDecoderObject;

/* ENCODING */

// Function to write the chunk to the file and start a new chunk
static inline int flush_chunk(stream_t *s)
{
    // Write the current buffer
    fwrite(s->msg, s->offset, 1, s->file);
    // Start the chunk offset at 0 again
    s->offset = 0;

    return 0;
}

static inline int flush_check(buffer_t *b, const size_t length)
{
    if (b->offset + length > b->allocated)
    {
        /* Check whether the length doesn't exceed the chunk limit on its own */
        if (length > b->allocated)
        {
            PyErr_Format(PyExc_ValueError, "Needed at least %zu bytes in the chunk buffer, while the limit was set to %zu", length, b->allocated);
            return 1;
        }
        
        if (flush_chunk((stream_t *)(b)) == 1) return 1;
    }

    return 0;
}

// Encode a list type
static inline int encode_list(stream_t *s, PyObject *value)
{
    const size_t num_items = PyList_GET_SIZE(value);
    
    for (size_t i = 0; i < num_items; ++i)
        if (encode_item((buffer_t *)(s), PyList_GET_ITEM(value, i), flush_check) == 1) return 1;

    s->num_items += num_items;
    return 0;
}

static inline int encode_dict(stream_t *s, PyObject *value)
{
    Py_ssize_t pos = 0;
    PyObject *key, *val;

    while (PyDict_Next(value, &pos, &key, &val))
        if (encode_item((buffer_t *)(s), key, flush_check) == 1 || encode_item((buffer_t *)(s), val, flush_check) == 1) return 1;

    s->num_items += PyDict_GET_SIZE(value);
    return 0;
}

#define CLEAR_MEMORY do { \
    if (clear_memory == 1) \
    { \
        free(s->msg); \
        s->msg = NULL; \
    } \
} while (0)

static PyObject *update_encoder(PyStreamEncoderObject *stream_obj, PyObject *args, PyObject *kwargs)
{
    PyObject *value;
    int clear_memory = 0;
    Py_ssize_t chunk_size = 0;
    
    static char *kwlist[] = {"value", "clear_memory", "chunk_size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|in", kwlist, &value, &clear_memory, &chunk_size))
        return NULL;

    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamEncoder object");
        return NULL;
    }

    // Check if the chunk size was changed
    if (chunk_size != 0)
    {
        s->allocated = chunk_size;

        // Don't use realloc as we don't need to preserve data
        free(s->msg);
        s->msg = (char *)malloc(chunk_size);

        if (s->msg == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }
    }
    else if (s->msg == NULL)
    {
        s->msg = (char *)malloc(s->allocated);
        if (s->msg == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }
    }

    // Open the file for writing in binary append mode
    s->file = fopen(s->filename, "ab");
    if (s->file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", s->filename);
        return NULL;
    }

    // Set the chunk offset to 0
    s->offset = 0;

    // We only support container types for streaming
    int status;
    PyTypeObject *type = Py_TYPE(value);
    if (s->type == type)
    {
        if (type == &PyList_Type)
            status = encode_list(s, value);
        else
            status = encode_dict(s, value);
    }
    else
    {
        PyErr_Format(PyExc_ValueError, "Streaming mode requires values to continue as the same type. Started with type '%s', got '%s'", s->type->tp_name, type->tp_name);
        fclose(s->file);
        return NULL;
    }

    if (status == 1)
    {
        fclose(s->file);
        return NULL;
    }

    // Write the last changes
    fwrite(s->msg, s->offset, 1, s->file);
    fclose(s->file);

    // Re-open the file in r+ to write to the metadata bytes
    s->file = fopen(s->filename, "r+");
    if (s->file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to re-open file '%s'", s->filename);
        fclose(s->file);
        return NULL;
    }

    if (fseek(s->file, s->stream_offset + 1, SEEK_SET) != 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Unable to set the file offset to %zu\n", s->stream_offset);
        fclose(s->file);
        return NULL;
    }

    // Write the number of items metadata to the file
    char num_items_buf[8];
    size_t offset = 0;
    WR_METADATA_LM2(num_items_buf, offset, s->num_items, 8);
    fwrite(num_items_buf, 8, 1, s->file);
    fclose(s->file);

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

    free(s->msg);
    free(s->filename);
    free(s);

    stream_obj->s = NULL;

    Py_RETURN_NONE;
}

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

    if (Py_TYPE(stream_obj)->tp_free)
        Py_TYPE(stream_obj)->tp_free((PyObject *)stream_obj);
    else
        PyObject_Del(stream_obj);
}

static PyObject *start_offset_encoder(PyStreamEncoderObject *stream_obj)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamDecoder object");
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(s->stream_offset);
}

static PyObject *total_offset_encoder(PyStreamEncoderObject *stream_obj)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamDecoder object");
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(s->offset);
}

static PyGetSetDef PyStreamEncoderGetSet[] = {
    {"start_offset", (getter)start_offset_encoder, NULL, "The initial file offset the encoder started writing to", NULL},
    {"total_offset", (getter)total_offset_encoder, NULL, "The total file offset the encoder is currently at", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef PyStreamEncoderMethods[] = {
    {"write", (PyCFunction)update_encoder, METH_VARARGS | METH_KEYWORDS, "Update the stream encoder with new data"},
    {"finalize", (PyCFunction)finalize_encoder, METH_NOARGS, "Finalize the stream encoder and clean it up"},
    {NULL, NULL, 0, NULL}
};

PyTypeObject PyStreamEncoderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.StreamEncoder",
    .tp_basicsize = sizeof(PyStreamEncoderObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = PyStreamEncoderMethods,
    .tp_dealloc = (destructor)encoder_dealloc,
    .tp_getset = PyStreamEncoderGetSet,
};

// Init function for encoder objects
PyObject *get_stream_encoder(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *filename;
    PyTypeObject *value_type = &PyList_Type;
    size_t chunk_size = DEFAULT_CHUNK_SIZE;
    int resume_stream = 0;
    int preserve_file = 0;
    size_t stream_offset = 0;

    static char *kwlist[] = {"file_name", "value_type", "chunk_size", "resume_stream", "file_offset", "preserve_file", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Onini", kwlist, &filename, (PyObject **)&value_type, (Py_ssize_t *)&chunk_size, &resume_stream, (Py_ssize_t *)&stream_offset, &preserve_file))
        return NULL;

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

    s->allocated = chunk_size;
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

        if (fseek(file, stream_offset, SEEK_SET) != 0)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Unable to set the file offset to %zu\n", stream_offset);
            fclose(file);
            free(s);
            return NULL;
        }

        // Buffer to write the current metadata to
        char buf[9];
        if (fread(buf, 9, 1, file) == 0)
        {
            PyErr_Format(PyExc_FileNotFoundError, "Failed to read the file from offset %zu", stream_offset);
            fclose(file);
            free(s);
            return NULL;
        }

        // Read the container datatype
        const char datachar = *buf & 0b00000111;

        // The MODE 2 8-byte length metadata is equal to `0b11111000`
        if ((*buf & 0b11111000) != 0b11111000 || (datachar != DT_ARRAY && datachar != DT_DICTN))
        {
            PyErr_SetString(PyExc_ValueError, "The existing file data does not match the encoding stream expectations");
            fclose(file);
            free(s);
            return NULL;
        }
        
        s->type = datachar == DT_ARRAY ? &PyList_Type : &PyDict_Type;
        s->stream_offset = stream_offset;

        // Rebuild the value in the metadata MODE 2 format
        size_t offset = 1;
        RD_METADATA_LM2(buf, offset, s->num_items, 8);

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
            // Open in binary write mode to overwrite any existing data
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

        // Write the first metadata byte in the way in mode 2 8-bytes format
        char buf[9] = {0};
        size_t offset = 0;
        const char datachar = value_type == &PyList_Type ? DT_ARRAY : DT_DICTN;
        WR_METADATA_LM2_MASK(buf, offset, datachar, 8);

        fwrite(&buf, 9, 1, file);
        fclose(file);

        s->type = value_type;
        s->num_items = 0;
    }

    // Set the filename to write to
    s->filename = (char *)malloc(strlen(filename) + 1);
    if (s->filename == NULL)
    {
        PyErr_NoMemory();
        free(s);
        return NULL;
    }
    // Copy the filename, plus 1 also copies the NULL-terminator
    memcpy(s->filename, filename, strlen(filename) + 1);

    PyStreamEncoderObject *stream_obj = PyObject_New(PyStreamEncoderObject, &PyStreamEncoderType);

    if (stream_obj == NULL)
    {
        PyErr_NoMemory();
        free(s->filename);
        free(s);
        return NULL;
    }

    stream_obj->s = s;

    return (PyObject *)stream_obj;
}

/* DECODING */

static PyObject *finalize_decoder(PyStreamDecoderObject *stream_obj, PyObject *args)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamDecoder object");
        return NULL;
    }

    free(s->msg); \
    free(s->filename); \
    free(s); \
    stream_obj->s = NULL; \

    Py_RETURN_NONE;
}

static inline int refresh_chunk(stream_t *s)
{
    // Update the total offset and reset the chunk offset
    s->stream_offset += s->offset;
    s->offset = 0;

    // Go to the reading point of the file
    if (fseek(s->file, s->stream_offset, SEEK_SET) != 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to open the file at offset %zu", s->stream_offset);
        return 1;
    }

    // Set the chunk size to the number of items read, so that it gets smaller if the file limit is reached.
    // It doesn't matter that this overrides the user value, as the end of the file is reached and thus only the smaller chunk size is needed
    if ((s->allocated = fread(s->msg, 1, s->allocated, s->file)) == 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to read the file from offset %zu", s->stream_offset);
        return 1;
    }

    return 0;
}

static inline int chunk_refresh_check(buffer_t *b, const size_t length)
{
    if (b->offset + length > b->allocated)
    {
        if (length > b->allocated)
        {
            PyErr_Format(PyExc_ValueError, "Found a value that requires %zu bytes to store, while the chunk limit is %s", length, b->allocated);
            return 1;
        }

        if (refresh_chunk((stream_t *)(b)) == 1) return 1;
    }

    return 0;
}

static inline PyObject *decode_list(stream_t *s, const size_t num_items)
{
    PyObject *list = PyList_New(num_items);

    if (list == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    for (size_t i = 0; i < num_items; ++i)
    {
        PyObject *val = decode_item((buffer_t *)(s), chunk_refresh_check);

        if (val == NULL)
        {
            Py_DECREF(list);
            return NULL;
        }

        PyList_SET_ITEM(list, i, val);
    }

    s->num_items -= num_items;

    return list;
}

static inline PyObject *decode_dict(stream_t *s, const size_t num_items)
{
    PyObject *dict = PyDict_New();

    if (dict == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    for (size_t i = 0; i < num_items; ++i)
    {
        PyObject *key = decode_item((buffer_t *)(s), chunk_refresh_check);

        if (key == NULL)
        {
            Py_DECREF(dict);
            return NULL;
        }

        PyObject *val = decode_item((buffer_t *)(s), chunk_refresh_check);

        if (val == NULL)
        {
            Py_DECREF(dict);
            Py_DECREF(key);
            return NULL;
        }

        PyDict_SetItem(dict, key, val);
    }

    s->num_items -= num_items;

    return dict;
}

static PyObject *update_decoder(PyStreamDecoderObject *stream_obj, PyObject *args, PyObject *kwargs)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamDecoder object");
        return NULL;
    }

    size_t num_items = s->num_items;
    int clear_memory = 0;
    size_t chunk_size = 0;

    static char *kwlist[] = {"num_items", "clear_memory", "chunk_size", NULL};

    PyArg_ParseTupleAndKeywords(args, kwargs, "|nin", kwlist, (Py_ssize_t *)&num_items, &clear_memory, (Py_ssize_t *)&chunk_size);

    // Limit the number of items to the max available. Don't throw error as the `exhausted` class variable will indicate exhaustion
    if (num_items > s->num_items)
        num_items = s->num_items;
    
    // Return an empty object is the number of items to read is zero
    if (num_items == 0)
        return s->type == &PyList_Type ? PyList_New(0) : PyDict_New();
    
    // Check if the chunk size was changed
    if (chunk_size != 0)
    {
        s->allocated = chunk_size;

        // Don't use realloc as we don't need to preserve data
        free(s->msg);
        s->msg = (char *)malloc(chunk_size);

        if (s->msg == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }
    }
    else if (s->msg == NULL)
    {
        s->msg = (char *)malloc(s->allocated);
        if (s->msg == NULL)
        {
            PyErr_NoMemory();
            return NULL;
        }
    }

    s->file = fopen(s->filename, "rb");

    if (s->file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to open file '%s'", s->filename);
        return NULL;
    }

    // Go to the current stream offset to read from where we left off
    if (fseek(s->file, s->stream_offset, SEEK_SET) != 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to open the file at offset %zu", s->stream_offset);
        fclose(s->file);
        return NULL;
    }

    // Copy the first chunk into the message buffer
    if (fread(s->msg, 1, s->allocated, s->file) == 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to read the file from offset %zu", s->stream_offset);
        fclose(s->file);
        return NULL;
    }

    s->offset = 0;

    PyObject *result;

    if (s->type == &PyList_Type)
        result = decode_list(s, num_items);
    else
        result = decode_dict(s, num_items);
    
    s->stream_offset += s->offset;

    CLEAR_MEMORY;
    return result;
}

static void decoder_dealloc(PyStreamDecoderObject *stream_obj)
{
    stream_t *s = stream_obj->s;

    if (s != NULL)
    {
        free(s->filename);
        free(s->msg);
        free(s);

        stream_obj->s = NULL;
    }

    if (Py_TYPE(stream_obj)->tp_free)
        Py_TYPE(stream_obj)->tp_free((PyObject *)stream_obj);
    else
        PyObject_Del(stream_obj);
}

static PyObject *items_remaining_decoder(PyStreamDecoderObject *stream_obj, void *closure)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamDecoder object");
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(s->num_items);
}

static PyObject *total_offset_decoder(PyStreamEncoderObject *stream_obj)
{
    stream_t *s = stream_obj->s;

    if (s == NULL)
    {
        PyErr_SetString(PyExc_ValueError, "Received an invalid StreamDecoder object");
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(s->offset);
}

static PyGetSetDef PyStreamDecoderGetSet[] = {
    {"total_offset", (getter)total_offset_decoder, NULL, "The total file offset the encoder is currently at", NULL},
    {"items_remaining", (getter)items_remaining_decoder, NULL, "The amount of items remaining to be read", NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

static PyMethodDef PyStreamDecoderMethods[] = {
    {"read", (PyCFunction)update_decoder, METH_VARARGS | METH_KEYWORDS, "De-serialize data from the stream decoder"},
    {"finalize", (PyCFunction)finalize_decoder, METH_NOARGS, "Finalize the stream decoder and clean it up"},
    {NULL, NULL, 0, NULL}
};

PyTypeObject PyStreamDecoderType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.StreamEncoder",
    .tp_basicsize = sizeof(PyStreamDecoderObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = PyStreamDecoderMethods,
    .tp_dealloc = (destructor)decoder_dealloc,
    .tp_getset = PyStreamDecoderGetSet,
};

// Init function for encoder objects
PyObject *get_stream_decoder(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *filename;
    size_t chunk_size = DEFAULT_CHUNK_SIZE;
    size_t stream_offset = 0;

    static char *kwlist[] = {"file_name", "chunk_size", "file_offset", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|nn", kwlist, &filename, (Py_ssize_t *)&chunk_size, (Py_ssize_t *)&stream_offset))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the `file_name` (str) argument");
        return NULL;
    }

    stream_t *s = (stream_t *)malloc(sizeof(stream_t));

    if (s == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    s->stream_offset = stream_offset;
    s->allocated = chunk_size;
    s->msg = NULL;

    // Open the file in binary read mode to read the current number of items
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to create/open file '%s'", filename);
        free(s);
        return NULL;
    }

    if (fseek(file, stream_offset, SEEK_SET) != 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Unable to set the file offset to offset %zu\n", stream_offset);
        fclose(file);
        free(s);
        return NULL;
    }

    // Read the type and current number of items
    char buf[9];
    if (fread(buf, 9, 1, file) == 0)
    {
        PyErr_Format(PyExc_FileNotFoundError, "Failed to read the file from offset %zu", stream_offset);
        fclose(file);
        free(s);
        return NULL;
    }
    fclose(file);

    // Get which datatype we have stored
    const char dt_mask = buf[0] & 0b00000111;

    if (dt_mask == DT_ARRAY)
        s->type = &PyList_Type;
    else if (dt_mask == DT_DICTN)
        s->type = &PyDict_Type;
    else
    {
        PyErr_SetString(PyExc_ValueError, "Encoded data must start with a list or dict object for stream objects");
        free(s);
        return NULL;
    }

    // Read the number of items
    size_t offset = 0;
    size_t num_items = 0;
    RD_METADATA(buf, offset, num_items);

    // Add the offset to start after the metadata, so at the first value
    s->stream_offset += offset;
    s->num_items = num_items;

    // Set the filename of the stream to write to
    s->filename = (char *)malloc(strlen(filename) + 1);
    if (s->filename == NULL)
    {
        PyErr_NoMemory();
        free(s);
        return NULL;
    }
    // Copy the filename, plus 1 also copies the NULL-terminator
    memcpy(s->filename, filename, strlen(filename) + 1);

    PyStreamDecoderObject *stream_obj = PyObject_New(PyStreamDecoderObject, &PyStreamDecoderType);

    if (stream_obj == NULL)
    {
        PyErr_NoMemory();
        free(s->filename);
        free(s);
        return NULL;
    }

    stream_obj->s = s;

    return (PyObject *)stream_obj;
}

