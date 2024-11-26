#include <Python.h>

#include "globals/typemasks.h"
#include "globals/internals.h"

typedef struct {
    char *base;     // Pointer to data
    size_t size;    // Size of data
    uint64_t mdata; // Metadata
} preval_t;

typedef struct {
    size_t nitems;      // Number of items in the array
    preval_t prevals[]; // The preval structs
} preval_array_t;


/* A preval array is an array that stores pre-fetched data from values.
 * The array starts off with how many items the array has, followed by
 * preval structs.
 * 
 * 
 * Boolean and None values set their `base` to NULL. This indicates we
 * only have to write one byte: the first byte from mdata.
 * 
 * 
 * Marks are used for types that don't use the preval_t as normal.
 * This is the case for integers, floats, arrays, and dicts.
 * 
 * As only the lower 56 bits (7 bytes) of the size are used at MOST,
 * we use the upper 8 bits for storing other data to use less memory.
 * 
 * When using a mark, we set the size part to zero, and the upper byte
 * to the marker belonging to the type, so we can decide what to do.
 * 
 * Integers have their value stored in the `base` itself, rather than
 * storing a pointer to the value. The length of the integer is set on
 * mdata.
 * 
 * Floats have their value stored in `base` itself as well, but they
 * have a static length and don't require anything special for fetching
 * length.
 * 
 * Arrays and dicts have `base` point towards a newly allocated buffer.
 * This buffer is a new preval array.
 */

#define DT_INTGR_MARK 0
#define DT_FLOAT_MARK 1
#define DT_ARRAY_MARK 2
#define DT_DICTN_MARK 3

#define MARK_WRITABLE(mark) ((uint64_t)(mark) << 56)
#define MARK_EXTRACT(size) (size >> 56)

#define VARLEN_METADATA_WR(tpmask, pre) do { \
    if (pre->size < 16) \
    { \
        pre->size |= (1ULL << 56); \
        pre->mdata = tpmask | (pre->size << 4); \
        *total += pre->size + 1; \
    } \
    else if (pre->size < 2048) \
    { \
        pre->size |= (2ULL << 56); \
        pre->mdata = tpmask | (0b01 << 3) | (pre->size << 5); \
        *total += pre->size + 2; \
    } \
    else \
    { \
        const uint64_t nbytes = USED_BYTES_64(pre->size); \
        pre->size |= (nbytes << 56); \
        pre->mdata = tpmask | (0b11 << 3) | (pre->size << 5); \
        *total += pre->size + nbytes + 1; \
    } \
} while (0)


#define ZIGZAG_ENCODE(x) (((uint64_t)(x) << 1) ^ ((int64_t)(x) >> 63))
#define ZIGZAG_DECODE(x) ((int64_t)((x) >> 1) ^ -((int64_t)(x) & 1))


static inline preval_array_t *get_preval_array(PyObject *value, size_t *total);

static inline int fill_preval(PyObject *value, preval_t *pre, size_t *total)
{
    PyTypeObject *tp = Py_TYPE(value);
    const char *tpname = tp->tp_name;

    switch(tpname[0])
    {
    case 'b':
    {
        if (tp == &PyBytes_Type)
        {
            PyBytes_AsStringAndSize(value, &pre->base, &pre->size);
            VARLEN_METADATA_WR(DT_BYTES, pre);
            return 0;
        }
        else if (tp == &PyBool_Type)
        {
            pre->base = NULL;
            pre->mdata = value == Py_True ? DT_BOOLT : DT_BOOLF;

            total += 1;

            return 0;
        }
    }
    case 's':
    {
        if (tp != &PyUnicode_Type)
            break;
        
        pre->base = PyUnicode_AsUTF8AndSize(value, &pre->size);
        VARLEN_METADATA_WR(DT_STRNG, pre);
        
        return 0;
    }
    case 'i':
    {
        if (tp != &PyLong_Type)
            break;

        long num = PyLong_AsLong(value);
        num = ZIGZAG_ENCODE(num);

        const unsigned int nbytes = USED_BYTES_64(num);
        *total += nbytes + 1;

        num = LITTLE_64(num);

        memcpy(pre->base, &num, sizeof(long));

        pre->size = MARK_WRITABLE(DT_INTGR_MARK);
        pre->mdata = nbytes;

        return 0;
    }
    case 'f':
    {
        if (tp != &PyFloat_Type)
            break;

        double num = PyFloat_AsDouble(value);
        num = LITTLE_DOUBLE(num);

        memcpy(pre->base, &num, sizeof(double));

        pre->size = MARK_WRITABLE(DT_FLOAT_MARK);

        *total += 9;
        return 0;
    }
    case 'N':
    {
        if (value != Py_None)
            break;

        pre->base = NULL;
        pre->mdata = DT_NONTP;

        total += 1;

        return 0;
    }
    case 'l':
    {
        if (tp != &PyList_Type)
            break;

        preval_array_t *new = get_preval_array(value, total);

        if (new == NULL)
            return 1;

        pre->size = MARK_WRITABLE(DT_ARRAY_MARK);
        pre->base = (char *)new;

        return 0;
    }
    case 'd':
    {
        if (tp != &PyDict_Type)
            break;

        preval_array_t *new = get_preval_array(value, total);

        if (new == NULL)
            return 1;

        pre->size = MARK_WRITABLE(DT_DICTN_MARK);
        pre->base = (char *)new;

        return 0;
    }
    }

    return 1;
}

static inline preval_array_t *get_preval_array(PyObject *value, size_t *total)
{
    size_t nitems = Py_SIZE(value);

    if (PyDict_Check(value))
        nitems *= 2;
    
    preval_array_t *array = (preval_array_t *)malloc(sizeof(size_t) + sizeof(preval_t) * nitems);

    if (array == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    if (nitems < 16)
        total += 1;
    else if (nitems < 2048)
        total += 2;
    else
        total += 1 + USED_BYTES_64(nitems);

    array->nitems = nitems;

    if (PyList_Check(value))
    {
        for (size_t i = 0; i < nitems; ++i)
        {
            PyObject *item = PyList_GET_ITEM(value, i);
            fill_preval(item, &(array->prevals[i]), total);
        }
    }
    else
    {
        return NULL;
    }

    return array;
}

static inline void encode_preval_array(char **offset, preval_array_t *pre, unsigned int tpmask);

static inline void encode_preval(char **offset, preval_t pre)
{
    if (pre.base == NULL)
    {
        ((*offset)++)[0] = ((char *)pre.mdata)[0];
        return;
    }
    
    const size_t size = pre.size & 0xFFFFFFFFFFFFFF;

    if (size == 0)
    {
        const unsigned int mask = MARK_EXTRACT(pre.size);

        switch (mask)
        {
        case DT_INTGR_MARK:
        {
            ((*offset)++)[0] = DT_INTGR | ((unsigned char)pre.mdata << 3);

            memcpy(*offset, &pre.base, 8);
            offset += pre.mdata;

            break;
        }
        case DT_FLOAT_MARK:
        {
            ((*offset)++)[0] = DT_FLOAT;

            memcpy(*offset, &pre.base, sizeof(double));
            offset += sizeof(double);

            break;
        }
        case DT_ARRAY_MARK:
        {
            preval_array_t *array = (preval_array_t *)pre.base;
            encode_preval_array(offset, array, DT_ARRAY);

            break;
        }
        case DT_DICTN_MARK:
        {
            preval_array_t *array = (preval_array_t *)pre.base;
            encode_preval_array(offset, array, DT_DICTN);

            break;
        }
        }

        return;
    }

    printf("size: %zu\n", size);

    memcpy(*offset, &pre.mdata, 8);
    *offset += pre.size >> 56;

    memcpy(*offset, pre.base, size);
    *offset += size;

    return;
}

static inline void encode_preval_array(char **offset, preval_array_t *pre, unsigned int tpmask)
{
    if (pre->nitems < 16)
    {
        ((*offset)++)[0] = tpmask | (pre->nitems << 4);
    }
    else if (pre->nitems < 2048)
    {
        ((*offset)++)[0] = tpmask | (0b01 << 3) | (pre->nitems << 5);
        ((*offset)++)[0] = pre->nitems >> 3;
    }
    else
    {
        const unsigned int nbytes = USED_BYTES_64(pre->nitems);
        ((*offset)++)[0] = tpmask | (0b11 << 3) | (nbytes << 5);

        memcpy(*offset, &pre->nitems, 8);
        *offset += nbytes;
    }

    for (size_t i = 0; i < pre->nitems; ++i)
        encode_preval(offset, pre->prevals[i]);
    
    free(pre);

    return;
}

static inline PyObject *encode_object(PyObject *value)
{
    PyTypeObject *tp = Py_TYPE(value);

    if (tp == &PyList_Type || tp == &PyDict_Type)
    {
        size_t total = 0;
        preval_array_t *array = get_preval_array(value, &total);

        if (array == NULL)
            return NULL;

        char *base = (char *)malloc(total + 16); // Plus 16 for memcpy operations headroom
        char *offset = base;

        if (base == NULL)
        {
            free(array);
            return PyErr_NoMemory();
        }

        encode_preval_array(&offset, array, tp == &PyList_Type ? DT_ARRAY : DT_DICTN);

        return PyBytes_FromStringAndSize(base, total);
    }

    return NULL;
}

PyObject *encode_test(PyObject *self, PyObject *args)
{
    PyObject *value = PyTuple_GET_ITEM(args, 0);

    return encode_object(value);
}

