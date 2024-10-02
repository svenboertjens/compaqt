#define PY_SSIZE_CLEAN_T

#include <Python.h>

// These macros will be overwritten by the setup.py when needed
#define IS_LITTLE_ENDIAN 1 // Endianness to follow
#define STRICT_ALIGNMENT 0 // Whether to use strict alignment

/* GLOBALS */

// Average size of items
static size_t avg_item_size = 32;
// Average size of re-allocations
static size_t avg_realloc_size = 512;

// Whether to dymically adjust allocation settings
static char dynamic_allocation_tweaks = 1;

// Allocation data
static size_t reallocs;
static size_t allocated;

// Message buffer data
static char *msg;
static size_t offset;

/* METADATA MACROS */

// The maximum bytes VLE metadata will take up
#define MAX_VLE_METADATA_SIZE 9

// Write Variable Length Encoding (VLE) metadata
#define WR_METADATA_VLE(dt_mask, length) do { \
    /* 
      Decide what mode to use by checking whether the length fits within 4 and 11 bits.

      0: length fits in 1 byte.
      1: length fits in 2 bytes.
      2: length is longer, write to a variable number of bytes.

    */ \
    const int mode = !!(length >> 4) + !!(length >> 11); \
    switch (mode) \
    { \
    case 0: *(msg + offset++) = dt_mask | (unsigned char)(length << 4); break; \
    case 1: \
    { \
        *(msg + offset++) = dt_mask | 0b00001000 | (unsigned char)(length << 5); \
        *(msg + offset++) = (unsigned char)(length >> 3); \
        break; \
    } \
    case 2: \
    { \
        const int num_bytes = !!(length >> (8 * 2)) + !!(length >> (8 * 3)) + !!(length >> (8 * 4)) + !!(length >> (8 * 5)) + !!(length >> (8 * 6)) + !!(length >> (8 * 7)); \
        \
        *(msg + offset++) = dt_mask | 0b00011000 | (num_bytes << 5); \
        *(msg + offset++) = length & 0xFF; \
        *(msg + offset++) = (length & 0xFF00) >> 8; \
        \
        switch (num_bytes) \
        { \
        case 6: *(msg + offset++) = (length >> (8 * 7)); \
        case 5: *(msg + offset++) = (length >> (8 * 6)); \
        case 4: *(msg + offset++) = (length >> (8 * 5)); \
        case 3: *(msg + offset++) = (length >> (8 * 4)); \
        case 2: *(msg + offset++) = (length >> (8 * 3)); \
        case 1: *(msg + offset++) = (length >> (8 * 2)); \
        } \
        break; \
    } \
    } \
} while (0)

// Read VLE metadata
#define RD_METADATA_VLE(length) do { \
    const int mode = *(msg + offset) & 0b00011000; \
    \
    switch (mode) \
    { \
    case 0b00010000: /* Extra case for if the first length bit is set, but not the first mode bit */\
    case 0b00000000: length = (*(msg + offset++) & 0b11110000) >> 4; break; \
    case 0b00001000: \
    { \
        length = (*(msg + offset++) & 0b11100000) >> 5; \
        length |= (*(msg + offset++) & 0xFF) << 3; \
        break; \
    } \
    case 0b00011000: \
    { \
        const int num_bytes = (*(msg + offset++) & 0b11100000) >> 5; \
        \
        length = (size_t)(*(msg + offset++) & 0xFF); \
        length |= (size_t)((*(msg + offset++) & 0xFF) << 8); \
        switch (num_bytes) \
        { \
        case 6: length |= (size_t)(*(msg + offset++) & 0xFF) << (8 * 7); \
        case 5: length |= (size_t)(*(msg + offset++) & 0xFF) << (8 * 6); \
        case 4: length |= (size_t)(*(msg + offset++) & 0xFF) << (8 * 5); \
        case 3: length |= (size_t)(*(msg + offset++) & 0xFF) << (8 * 4); \
        case 2: length |= (size_t)(*(msg + offset++) & 0xFF) << (8 * 3); \
        case 1: length |= (size_t)(*(msg + offset++) & 0xFF) << (8 * 2); \
        } \
        break; \
    } \
    } \
} while (0)

// Read a datamask (stored in first 3 bits)
#define RD_DTMASK() (*(msg + offset) & 0b00000111)
// Read a group datamask (takes up entire byte)
#define RD_DTMASK_GROUP() (*(msg + offset++))

/*

 # Might be used later for static arrays

#define WR_LENGTH(length) (*(msg + offset++) = (unsigned char)(length))
#define RD_LENGTH() ((size_t)(*(msg + offset++) & 0xFF))

#define WR_METADATA_STATIC(dt_mask_container, dt_mask, ln_repr, length) do { \
    *(msg + offset++) = dt_mask_container | (dt_mask << 3) | (ln_repr << 6); \
    switch (ln_repr) \
    { \
    case 3: *(msg + offset++) = (unsigned char)(length >> 24); \
    case 2: *(msg + offset++) = (unsigned char)(length >> 16); \
    case 1: *(msg + offset++) = (unsigned char)(length >> 8); \
    case 0: *(msg + offset++) = (unsigned char)(length); \
    } \
} while (0)

#define RD_METADATA_STATIC(dt_mask, length) do { \
    dt_mask = *(msg + offset) & 0b00011100; \
    const int ln_repr = *(msg + offset++) & 0b00000011; \
    \
    switch (ln_repr) \
    { \
    case 3: length |= *(msg + offset++) << 24; \
    case 2: length |= *(msg + offset++) << 16; \
    case 1: length |= *(msg + offset++) <<  8; \
    case 0: length |= *(msg + offset++); \
    } \
} while (0)

*/

/* ENDIANNESS TRANSFORMATIONS */

#if (IS_LITTLE_ENDIAN == 1)

    #define LL_LITTLE_ENDIAN(value) (value) // Long to little-endian
    #define DB_LITTLE_ENDIAN(value) (value) // Double to little-endian

#else

     // Long to little-endian
    static inline long long LL_LITTLE_ENDIAN(long long value)
    {
        if (transform_endianness == 0)
            return value;
        
        long long result = 0;

        if (sizeof(long long) == 4)
        {
            result |= (value & 0xFF000000) >> 24;
            result |= (value & 0x00FF0000) >> 8;
            result |= (value & 0x0000FF00) << 8;
            result |= (value & 0x000000FF) << 24;
        }
        else
        {
            result |= (value & 0xFF00000000000000LL) >> 56;
            result |= (value & 0x00FF000000000000LL) >> 40;
            result |= (value & 0x0000FF0000000000LL) >> 24;
            result |= (value & 0x000000FF00000000LL) >> 8;
            result |= (value & 0x00000000FF000000LL) << 8;
            result |= (value & 0x0000000000FF0000LL) << 24;
            result |= (value & 0x000000000000FF00LL) << 40;
            result |= (value & 0x00000000000000FFLL) << 56;
        }

        return result;
    }

    // Double to little-endian
    static inline double DB_LITTLE_ENDIAN(double value)
    {
        if (transform_endianness == 0)
            return value;
        
        union {
            double d;
            uint8_t bytes[sizeof(double)];
        } val, result;

        val.d = value;

        for (size_t i = 0; i < sizeof(double); i++) {
            result.bytes[i] = val.bytes[sizeof(double) - 1 - i];
        }

        return result.d;
    }

    /* MAKE BIG-ENDIAN VERSION */

#endif

/* SUPPORTED DATATYPES */

#define DT_ARRAY (unsigned char)(0) // Arrays
#define DT_DICTN (unsigned char)(1) // Dicts
#define DT_BYTES (unsigned char)(2) // Binary
#define DT_STRNG (unsigned char)(3) // Strings
#define DT_INTGR (unsigned char)(4) // Integers

// Group datatypes stores datatypes not requiring length metadata (thus can take up whole byte)
#define DT_GROUP (unsigned char)(5) // Group datatype mask for identifying them
#define DT_BOOLF DT_GROUP | (unsigned char)(0 << 3) // False Booleans
#define DT_BOOLT DT_GROUP | (unsigned char)(1 << 3) // True Booleans
#define DT_FLOAT DT_GROUP | (unsigned char)(3 << 3) // Floats
#define DT_NONTP DT_GROUP | (unsigned char)(4 << 3) // NoneTypes
#define DT_CMPLX DT_GROUP | (unsigned char)(6 << 3) // Complexes

#define DT_NOUSE (unsigned char)(6) // Not yet used for anything, suggestions?
#define DT_EXTND (unsigned char)(7) // Custom datatypes

/* DATATYPE CONVERSION */

static inline size_t bytes_ln(PyObject *value)
{
    return PyBytes_GET_SIZE(value);
}
static inline void bytes_wr(const PyObject *value, const size_t length)
{
    const char *ptr = PyBytes_AS_STRING(value);
    memcpy(msg + offset, ptr, length);
    offset += length;
}
static inline PyObject *bytes_rd(const size_t length)
{
    PyObject *result = PyBytes_FromStringAndSize(msg + offset, (Py_ssize_t)(length));
    offset += length;
    return result;
}

static inline size_t string_ln(PyObject *value)
{
    return PyUnicode_GET_LENGTH(value) * PyUnicode_KIND(value);
}
static inline void string_wr(PyObject *value, const size_t length)
{
    memcpy(msg + offset, PyUnicode_DATA(value), length);
    offset += length;
}
static inline PyObject *string_rd(const size_t length)
{
    PyObject *result = PyUnicode_FromStringAndSize(msg + offset, length);
    offset += length;
    return result;
}

static inline size_t integer_ln(PyObject *value)
{
    const size_t length = (_PyLong_NumBits(value) + 8) >> 3;
    return length == 0 ? 1 : length;
}
static inline void integer_wr(PyObject *value, const size_t length)
{
    if (length > sizeof(long long))
        _PyLong_AsByteArray((PyLongObject *)value, (unsigned char *)(msg + offset), length, IS_LITTLE_ENDIAN, 1);
    else
    {
        #if (STRICT_ALIGNMENT == 0)

            *(long long *)(msg + offset) = LL_LITTLE_ENDIAN(PyLong_AsLongLong(value));

        #else

            const long long num = LL_LITTLE_ENDIAN(PyLong_AsLongLong(value));
            memcpy(msg + offset, &num, length);

        #endif

        offset += length;
    }
}
static inline PyObject *integer_rd(const size_t length)
{
    PyObject *result = _PyLong_FromByteArray((const unsigned char *)(msg + offset), length, IS_LITTLE_ENDIAN, 1);
    offset += length;
    return result;
}

#if (STRICT_ALIGNMENT == 0)

    static inline void float_wr(const PyObject *value)
    {
        *(double *)(msg + offset) = DB_LITTLE_ENDIAN(PyFloat_AS_DOUBLE(value));
        offset += sizeof(double);
    }
    static inline PyObject *float_rd()
    {
        PyObject *result = PyFloat_FromDouble(DB_LITTLE_ENDIAN(*(double *)(msg + offset)));
        offset += sizeof(double);
        return result;
    }

    static inline void complex_wr(PyObject *value)
    {
        *(Py_complex *)(msg + offset) = PyComplex_AsCComplex(value);
        offset += sizeof(Py_complex);
    }
    static inline PyObject *complex_rd()
    {
        PyObject *result = PyComplex_FromCComplex(*(Py_complex *)(msg + offset));
        offset += sizeof(Py_complex);
        return result;
    }

#else

    static inline void float_wr(const PyObject *value)
    {
        double num = DB_LITTLE_ENDIAN(PyFloat_AS_DOUBLE(value));
        memcpy(msg + offset, &num, sizeof(double));
        offset += sizeof(double);
    }
    static inline PyObject *float_rd()
    {
        double num;
        memcpy(&num, msg + offset, sizeof(double));
        offset += sizeof(double);
        return PyFloat_FromDouble(DB_LITTLE_ENDIAN(num));
    }

    static inline void complex_wr(const PyObject *value)
    {
        Py_complex complex = PyComplex_AsCComplex(value);
        memcpy(msg + offset, &complex, sizeof(Py_complex));
        offset += sizeof(Py_complex);
    }
    static inline PyObject *complex_rd()
    {
        Py_complex complex;
        memcpy(&complex, msg + offset, sizeof(Py_complex));
        PyObject *result = PyComplex_FromCComplex(complex);
        offset += sizeof(Py_complex);
        return result;
    }

#endif

/* ENCODING */

// Reallocate the message buffer by a set amount
#define REALLOC_MSG(new_length) do { \
    char *tmp = (char *)realloc(msg, new_length); \
    if (tmp == NULL) \
    { \
        PyErr_NoMemory(); \
        return 1; \
    } \
    allocated = new_length; \
    msg = tmp; \
} while (0)

// Ensure there's enough space allocated to write
#define OFFSET_CHECK(length) do { \
    if (offset + length > allocated) \
    { \
        REALLOC_MSG(offset + length + avg_realloc_size); \
        ++reallocs; \
    } \
} while (0)

// Check datatype for a container item
#define DT_CHECK(expected) do { \
    if (type != &expected) \
    { \
        Py_DECREF(item); \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        return 1; \
    } \
} while (0)

// Encode an item from a container type
static inline int encode_container_item(PyObject *item)
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

            OFFSET_CHECK(length + MAX_VLE_METADATA_SIZE);
            WR_METADATA_VLE(DT_BYTES, length);

            bytes_wr(item, length);
        }
        else // Boolean
        {
            OFFSET_CHECK(1);
            *(msg + offset++) = DT_BOOLF | (!!(item == Py_True) << 3);
        }
        break;
    }
    case 's': // String
    {
        DT_CHECK(PyUnicode_Type);

        const size_t length = string_ln(item);

        OFFSET_CHECK(length + MAX_VLE_METADATA_SIZE);
        WR_METADATA_VLE(DT_STRNG, length);

        string_wr(item, length);
        break;
    }
    case 'i': // Integer
    {
        DT_CHECK(PyLong_Type);

        const size_t length = integer_ln(item);

        OFFSET_CHECK(length + MAX_VLE_METADATA_SIZE);
        WR_METADATA_VLE(DT_INTGR, length);

        integer_wr(item, length);
        break;
    }
    case 'f': // Float
    {
        DT_CHECK(PyFloat_Type);
        OFFSET_CHECK(9);

        *(msg + offset++) = DT_FLOAT;

        float_wr(item);
        break;
    }
    case 'c': // Complex
    {
        DT_CHECK(PyComplex_Type);
        OFFSET_CHECK(17);

        *(msg + offset++) = DT_CMPLX;

        complex_wr(item);
        break;
    }
    case 'N': // NoneType
    {
        *(msg + offset++) = DT_NONTP;
        break;
    }
    case 'l': // List
    {
        DT_CHECK(PyList_Type);

        const size_t num_items = (size_t)(PyList_GET_SIZE(item));
        WR_METADATA_VLE(DT_ARRAY, num_items);

        for (size_t i = 0; i < num_items; ++i)
            if (encode_container_item(PyList_GET_ITEM(item, i)) == 1) return 1;
        
        break;
    }
    case 'd': // Dict
    {
        DT_CHECK(PyDict_Type);
        WR_METADATA_VLE(DT_DICTN, PyDict_GET_SIZE(item));

        Py_ssize_t pos = 0;
        PyObject *key, *val;
        while (PyDict_Next(item, &pos, &key, &val))
        {
            if (encode_container_item(key) == 1) return 1;
            if (encode_container_item(val) == 1) return 1;
        }
        break;
    }
    default: // Unsupported datatype
    {
        if (type != &PyUnicode_Type)
        {
            PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name);
            return 1;
        }
    }
    }

    return 0;
}

// Encode a list type
static inline PyObject *encode_list(PyObject *value, const int strict)
{
    if (!PyList_Check(value))
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return NULL;
    }

    size_t num_items = PyList_GET_SIZE(value);
    if (num_items == 0)
    {
        char buf = DT_ARRAY;
        return PyBytes_FromStringAndSize(&buf, 1);
    }

    const size_t initial_allocated = (avg_item_size * num_items) + avg_realloc_size;
    
    msg = (char *)malloc(initial_allocated);
    if (msg == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    allocated = initial_allocated;
    reallocs = 0;
    offset = 0;

    WR_METADATA_VLE(DT_ARRAY, num_items);

    for (size_t i = 0; i < num_items; ++i)
    {
        if (encode_container_item(PyList_GET_ITEM(value, i)) == 1)
        {
            free(msg);
            return NULL;
        }
    }
    
    if (dynamic_allocation_tweaks == 1)
    {
        if (reallocs != 0)
        {
            const size_t difference = offset - initial_allocated;
            const size_t med_diff = difference / num_items;

            avg_realloc_size += difference >> 1;
            avg_item_size += med_diff >> 1;
        }
        else
        {
            const size_t difference = initial_allocated - offset;
            const size_t med_diff = difference / num_items;
            const size_t diff_small = difference >> 4;
            const size_t med_small = med_diff >> 5;

            if (diff_small + 64 < avg_realloc_size)
                avg_realloc_size -= diff_small;
            else
                avg_realloc_size = 64;

            if (med_small + 4 < avg_item_size)
                avg_item_size -= med_small;
            else
                avg_item_size = 4;
        }
    }

    PyObject *result = PyBytes_FromStringAndSize(msg, offset);

    free(msg);
    return result;
}

// Set up encoding basis for a dict type
static inline PyObject *encode_dict(PyObject *value, const int strict)
{
    if (!PyDict_Check(value))
    {
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
        return NULL;
    }

    size_t num_items = (size_t)(PyDict_GET_SIZE(value));
    if (num_items == 0)
    {
        char buf = DT_DICTN;
        return PyBytes_FromStringAndSize(&buf, 1);
    }

    const size_t initial_allocated = (avg_item_size * (num_items * 2)) + avg_realloc_size;
    
    msg = (char *)malloc(initial_allocated);
    if (msg == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    allocated = initial_allocated;
    reallocs = 0;
    offset = 0;

    WR_METADATA_VLE(DT_DICTN, num_items);

    Py_ssize_t pos = 0;
    PyObject *key, *val;
    while (PyDict_Next(value, &pos, &key, &val))
    {
        if (encode_container_item(key) == 1 || encode_container_item(val) == 1)
        {
            free(msg);
            return NULL;
        }
    }
    
    if (dynamic_allocation_tweaks == 1)
    {
        if (reallocs != 0)
        {
            const size_t difference = offset - initial_allocated;
            const size_t med_diff = difference / (num_items * 2);

            avg_realloc_size += difference >> 1;
            avg_item_size += med_diff >> 1;
        }
        else
        {
            const size_t difference = initial_allocated - offset;
            const size_t med_diff = difference / (num_items * 2);
            const size_t diff_small = difference >> 4;
            const size_t med_small = med_diff >> 5;

            if (diff_small + 64 < avg_realloc_size)
                avg_realloc_size -= diff_small;
            else
                avg_realloc_size = 64;

            if (med_small + 4 < avg_item_size)
                avg_item_size -= med_small;
            else
                avg_item_size = 4;
        }
    }

    PyObject *result = PyBytes_FromStringAndSize(msg, offset);

    free(msg);
    return result;
}

// Encode and return a single non-container value
#define ENCODE_SINGLE_VALUE(expected, dt_mask, DT_LN, DT_WR) do { \
    if (type != &expected) \
    { \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        return NULL; \
    } \
    \
    const size_t length = DT_LN(value); \
    \
    msg = (char *)malloc(length + 1); \
    if (msg == NULL) \
    { \
        PyErr_NoMemory(); \
        return NULL; \
    } \
    \
    offset = 1; \
    DT_WR(value, length); \
    *(msg) = dt_mask; \
    \
    PyObject *result = PyBytes_FromStringAndSize(msg, length + 1); \
    \
    free(msg); \
    return result; \
} while (0)

// Main function for encoding objects
static PyObject *encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"value", "strict", NULL};

    PyObject *value;
    int strict = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i", kwlist, &value, &strict))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'any' input");
        return NULL;
    }

    PyTypeObject *type = Py_TYPE(value);
    const char *tp_name = type->tp_name;

    switch (*tp_name)
    {
    case 'b': // Bytes / Boolean
    {
        if (tp_name[1] == 'y') // Bytes
            ENCODE_SINGLE_VALUE(PyBytes_Type, DT_BYTES, bytes_ln, bytes_wr);
        else // Boolean
        {
            char buf = DT_BOOLF | (!!(value == Py_True) << 3);
            return PyBytes_FromStringAndSize(&buf, 1);
        }
    }
    case 's': // String
        ENCODE_SINGLE_VALUE(PyUnicode_Type, DT_STRNG, string_ln, string_wr);
    case 'i': // Integer
        ENCODE_SINGLE_VALUE(PyLong_Type, DT_INTGR, integer_ln, integer_wr);
    case 'f': // Float
    {
        if (type != &PyFloat_Type) break;

        char buf[9];
        buf[0] = DT_FLOAT;

        #if (STRICT_ALIGNMENT == 0)

            *(double *)(buf + 1) = PyFloat_AS_DOUBLE(value);

        #else

            const double num = PyFloat_AS_DOUBLE(value);
            memcpy(buf + 1, &num, 8);

        #endif

        return PyBytes_FromStringAndSize(buf, 9);
    }
    case 'c': // Complex
    {
        if (type != &PyComplex_Type) break;

        char buf[17];
        buf[0] = DT_CMPLX;

        #if (STRICT_ALIGNMENT == 0)

            *(Py_complex *)(buf + 1) = PyComplex_AsCComplex(value);

        #else

            const Py_complex complex = PyComplex_AsCComplex(value);
            memcpy(buf + 1, &complex, 16);

        #endif

        return PyBytes_FromStringAndSize(buf, 17);
    }
    case 'N': // NoneType
    {
        if (value != Py_None) break;
        char buf = DT_NONTP;
        return PyBytes_FromStringAndSize(&buf, 1);
    }
    case 'l': // List
        return encode_list(value, strict);
    case 'd': // Dict
        return encode_dict(value, strict);
    }

    PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name);
    return NULL;
}

/* DECODING */

// Set an error stating we received an invalid input
#define SET_INVALID_ERR() (PyErr_SetString(PyExc_ValueError, "Received an invalid or corrupted bytes string"))

// Check whether we aren't overreading the buffer (means invalid input)
#define OVERREAD_CHECK(length) do { \
    if (offset + length > allocated) \
    { \
        SET_INVALID_ERR(); \
        return NULL; \
    } \
} while (0)

// Decode a single item
#define DECODE_ITEM(rd_func) do { \
    size_t length = 0; \
    RD_METADATA_VLE(length); \
    OVERREAD_CHECK(length); \
    return rd_func(length); \
} while (0)

// Decode an item inside a container
static PyObject *decode_container_item()
{
    const char dt_mask = RD_DTMASK();

    switch (dt_mask)
    {
    case DT_BYTES: DECODE_ITEM(bytes_rd);
    case DT_STRNG: DECODE_ITEM(string_rd);
    case DT_INTGR: DECODE_ITEM(integer_rd);
    case DT_GROUP:
    {
        const char act_mask = RD_DTMASK_GROUP();

        switch (act_mask)
        {
        case DT_FLOAT: return float_rd();
        case DT_CMPLX: return complex_rd();
        case DT_BOOLT: Py_RETURN_TRUE;
        case DT_BOOLF: Py_RETURN_FALSE;
        case DT_NONTP: Py_RETURN_NONE;
        }
    }
    case DT_ARRAY:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);

        PyObject *list = PyList_New(num_items);

        if (list == NULL)
        {
            PyErr_Format(PyExc_RuntimeError, "Failed to create a list object of size '%zu'", num_items);
            return NULL;
        }

        // Decode all items and append to the list
        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *item = decode_container_item();

            if (item == NULL)
            {
                Py_DECREF(list);
                return NULL;
            }

            PyList_SET_ITEM(list, i, item);
        }

        return list;
    }
    case DT_DICTN:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);

        PyObject *dict = PyDict_New();

        if (dict == NULL)
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create a dict object");
            return NULL;
        }

        // Go over the dict and decode all items, place them into the dict
        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *key, *val;

            if ((key = decode_container_item()) == NULL || (val = decode_container_item()) == NULL)
            {
                Py_DECREF(dict);
                return NULL;
            }

            PyDict_SetItem(dict, key, val);
        }

        return dict;
    }
    }

    // Invalid input received
    SET_INVALID_ERR();
    return NULL;
}

static PyObject *decode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"value", "strict", NULL};

    PyObject *value;
    int strict = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|i", kwlist, &PyBytes_Type, &value, &strict))
    {
        PyErr_SetString(PyExc_ValueError, "Expected 1 'any' input");
        return NULL;
    }

    PyBytes_AsStringAndSize(value, &msg, (Py_ssize_t *)(&allocated));

    offset = 0;

    const char dt_mask = RD_DTMASK();

    // Don't use the decode_container_item as single items require different care
    switch (dt_mask)
    {
    case DT_BYTES: ++offset; return bytes_rd(allocated - 1);
    case DT_STRNG: ++offset; return string_rd(allocated - 1);
    case DT_INTGR: ++offset; return integer_rd(allocated - 1);
    case DT_GROUP:
    {
        const char act_mask = RD_DTMASK_GROUP();

        switch (act_mask)
        {
        case DT_FLOAT: return float_rd();
        case DT_CMPLX: return complex_rd();
        case DT_BOOLT: Py_RETURN_TRUE;
        case DT_BOOLF: Py_RETURN_FALSE;
        case DT_NONTP: Py_RETURN_NONE;
        }
    }
    case DT_ARRAY:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);

        PyObject *list = PyList_New(num_items);

        if (list == NULL)
        {
            PyErr_Format(PyExc_RuntimeError, "Failed to create a list object of size '%zu'", num_items);
            return NULL;
        }

        PyObject *item;
        for (size_t i = 0; i < num_items; ++i)
        {
            if ((item = decode_container_item()) == NULL)
            {
                Py_DECREF(list);
                return NULL;
            }

            PyList_SET_ITEM(list, i, item);
        }

        return list;
    }
    case DT_DICTN:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);

        PyObject *dict = PyDict_New();

        if (dict == NULL)
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to create a dict object");
            return NULL;
        }

        for (size_t i = 0; i < num_items; ++i)
        {
            PyObject *key, *val;

            if ((key = decode_container_item()) == NULL || (val = decode_container_item()) == NULL)
            {
                Py_DECREF(dict);
                return NULL;
            }

            PyDict_SetItem(dict, key, val);
        }

        return dict;
    }
    }

    // Invalid input
    SET_INVALID_ERR();
    return NULL;
}

/* SETTING METHODS */

// Function to set manual allocation settings
static PyObject *manual_allocations(PyObject *self, PyObject *args)
{
    Py_ssize_t item_size;
    Py_ssize_t realloc_size;

    if (!PyArg_ParseTuple(args, "nn", &item_size, &realloc_size))
    {
        PyErr_SetString(PyExc_ValueError, "Expected two 'int' types");
        return NULL;
    }

    if (item_size <= 0 || realloc_size <= 0)
    {
        PyErr_SetString(PyExc_ValueError, "Size values must be positive and larger than zero!");
        return NULL;
    }

    dynamic_allocation_tweaks = 0;
    avg_item_size = (size_t)item_size;
    avg_realloc_size = (size_t)realloc_size;

    Py_RETURN_NONE;
}

// Function to set dynamic allocation settings
static PyObject *dynamic_allocations(PyObject *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t item_size = 0;
    Py_ssize_t realloc_size = 0;

    static char *kwlist[] = {"item_size", "realloc_size", NULL};

    // Don't check args issues as all are optional
    PyArg_ParseTupleAndKeywords(args, kwargs, "|nn", kwlist, &item_size, &realloc_size);

    if (item_size < 0 || realloc_size < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Size values must be positive and larger than zero!");
        return NULL;
    }

    dynamic_allocation_tweaks = 1;

    if (item_size != 0)
        avg_item_size = (size_t)item_size;
    if (realloc_size != 0)
        avg_realloc_size = (size_t)realloc_size;

    Py_RETURN_NONE;
}

/* MODULE DEFINITIONS */

static PyMethodDef CompaqtMethods[] = {
    {"encode", (PyCFunction)encode, METH_VARARGS | METH_KEYWORDS, "Encode a value to bytes"},
    {"decode", (PyCFunction)decode, METH_VARARGS | METH_KEYWORDS, "Decode a value from bytes"},
    {NULL, NULL, 0, NULL}
};

// Submodule for settings
static PyMethodDef SettingsMethods[] = {
    {"manual_allocations", (PyCFunction)manual_allocations, METH_VARARGS, "Set manual allocation settings"},
    {"dynamic_allocations", (PyCFunction)dynamic_allocations, METH_VARARGS | METH_KEYWORDS, "Enable dynamic allocation settings, and optionally set start values"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject settings = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.settings",
    .tp_basicsize = sizeof(PyObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = SettingsMethods,
};

// Module definition
static struct PyModuleDef compaqt = {
    PyModuleDef_HEAD_INIT,
    "compaqt",
    NULL,
    -1,
    CompaqtMethods
};

PyMODINIT_FUNC PyInit_compaqt(void) {
    if (PyType_Ready(&settings) < 0)
        return NULL;

    PyObject *m = PyModule_Create(&compaqt);
    if (m == NULL) return NULL;

    PyObject *s = PyObject_New(PyObject, &settings);
    if (s == NULL)
    {
        Py_DECREF(m);
        return NULL;
    }

    PyModule_AddObject(m, "settings", s);

    return m;
}

