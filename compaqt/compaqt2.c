#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <stdbool.h>

#include "internals.h"


/* TYPEDEFS */

// Buffer data holding the base, writing offset, and space allocated
typedef struct {
    PyBytesObject *ob_base;
    char *base;
    size_t offset;
    size_t allocated;
} buffer_t;

/* TYPEMASKS */

// 2-bit
#define DT_STRNG (unsigned char)0b00
#define DT_INTGR (unsigned char)0b01

// 0b10 and 0b11 used as based for further types
// 0b10 used for 3-bit masks
// 0b11 used for 4-bit masks

// 3-bit
#define DT_ARRAY (unsigned char)0b010
#define DT_DICTN (unsigned char)0b110

// 4-bit
#define DT_BYTES (unsigned char)0b0011
#define DT_FLOAT (unsigned char)0b0111
#define DT_UTYPE (unsigned char)0b1011
#define DT_STATE (unsigned char)0b1111

#define DT_BOOLF (DT_STATE | (0b00 << 4))
#define DT_BOOLT (DT_STATE | (0b01 << 4))
#define DT_NONTP (DT_STATE | (0b10 << 4))
#define DT_UNSUP (DT_STATE | (0b11 << 4))

/* # What types are represented by which typemask?
 * 
 * DT_STRNG: str
 * DT_INTGR: int
 * DT_ARRAY: list
 * DT_DICTN: dict
 * DT_BYTES: bytes
 * DT_FLOAT: float
 * DT_UTYPE: utypes (custom types defined by the user: usertypes)
 * DT_STATE: state values (true, false, none)
 * 
 */

/* # What are modes?
 * 
 * In this format, modes are used to specify different "modes" or methods
 * for storing length metadata. Mode 1 sets the first bit to 0, leaving the
 * upper bits for storing the length. Mode 2 and 3 set the first bit to 1,
 * and are separated by setting the second bit to 0 for mode 2 and 1 for
 * mode 3. For mode 2, store lower bits of the length in the remaining upper
 * bits and the rest in a new byte. For mode 3, calculate how many bytes the
 * length takes up as a `size_t` (called NBYTES). This will range from 1 to 8.
 * In cases where only 3 bits are available for storing NBYTES, subtract 1 to
 * set the range to 0-7. The NBYTES is stored above the mode bits. The length
 * is placed in the bytes after.
 * 
 * Note: For the mode bits, the "first bit" is the first bit that comes AFTER the
 * typemask bits.
 * 
 */

/* # How do typemasks store metadata?
 * 
 * DT_STRNG:
 * Use modes, with the first mode bit on bit 3. Mode 1 stores up to 31, mode
 * 2 stores up to 4095. Mode 3 stores the rest.
 * 
 * DT_ARRAY & DT_DICTN:
 * Use modes, with the first mode bit on bit 4. Mode 1 stores up to 15, mode
 * 2 stores up to 2047. Mode 3 stores the rest.
 * 
 * DT_INTGR:
 * Store the size of the number (1-8 bytes) on the 3rd, 4th, and 5th bits.
 * The size has 1 subtracted from it so that it stores 0-7 to fit within 3
 * bits. The number itself is stored on top of this metadata, starting on the
 * 6th bit of the first metadata byte already.
 * 
 * DT_BYTES:
 * Calculate NBYTES and store in upper 4 bits. Place length in the bytes
 * that follow.
 * 
 * DT_UTYPE:
 * Calculate NBYTES and store in upper 4 bits. Store the ID of the type, which
 * is defined by the user, in the next byte (0-255). Place the length of the
 * value in the bytes that follow.
 * 
 * DT_FLOAT:
 * No additional metadata. This value has a fixed length of 8.
 * 
 * DT_STATE:
 * Stores no length metadata, as the value itself is encoded in the first byte.
 * The value itself is stored on the 5th and 6th bit. 00 is true, 01 is false,
 * and 10 is none. Optionally, DT_STATE with value 11 can be used for unsupported
 * types.
 * 
 */

/* METADATA WRITING FOR TYPES */

// DT_STRNG
static inline void write_str_metadata(buffer_t *b, const size_t size)
{
    if (size <= 31)
    {
        (b->base + b->offset++)[0] = DT_STRNG | (size << 3);
        return;
    }
    else if (size <= 4095)
    {
        (b->base + b->offset++)[0] = DT_STRNG | (0b01 << 2) | (size << 4);
        (b->base + b->offset++)[0] = size >> 4;
    }
    else
    {
        // Get how many bytes SIZE takes up
        const size_t nbytes = USED_BYTES_64(size);

        // Get the size in little-endian format.
        // Shift up by 1 byte to have the lowest byte cleared, so it can be placed on the metadata
        const size_t shifted_size = LITTLE_64(size << 8);

        // Shift all metadata on a uint64_t for efficiency
        const uint64_t metadata = DT_STRNG | (0b11 << 2) | (nbytes << 4) | shifted_size;
        memcpy(b->base + b->offset, &metadata, 8);
        b->offset += nbytes + 1;
    }
}

// DT_ARRAY & DT_DICTN
static inline void write_container_metadata(buffer_t *b, const size_t size, const unsigned char tpmask)
{
    if (size <= 15)
    {
        (b->base + b->offset++)[0] = tpmask | (size << 4);
    }
    else if (size <= 2047)
    {
        (b->base + b->offset++)[0] = DT_STRNG | (0b01 << 2) | (size << 5);
        (b->base + b->offset++)[0] = size >> 3;

        //const uint16_t metadata = tpmask | (0b01 << 3) | (size << 5);
        //memcpy(b->base + b->offset, &metadata, 2);
        //b->offset += 2;
    }
    else
    {
        // Get how many bytes SIZE takes up
        const size_t nbytes = USED_BYTES_64(size);

        // Get the size in little-endian format.
        // Shift up by 1 byte to have the lowest byte cleared, so it can be placed on the metadata
        const size_t shifted_size = LITTLE_64(size << 8);

        // Shift all metadata on a uint64_t for efficiency
        const uint64_t metadata = tpmask | (0b11 << 3) | ((nbytes - 1) << 5) | shifted_size;
        memcpy(b->base + b->offset, &metadata, 8);
        b->offset += nbytes + 1;
    }
}

// DT_INTGR
static inline void write_int_metadata_and_data(buffer_t *b, const size_t size, const uint64_t num)
{
    const uint64_t metadata = DT_INTGR | ((size - 1) << 2) | (num << 5);
    const size_t metadata_size = USED_BYTES_64(metadata);

    memcpy(b->base + b->offset, &metadata, 8);
    b->offset += metadata_size;

    const size_t num_metadata_overflow = 1ULL << (64 - 5) - 1;
    if (num >= num_metadata_overflow)
        (b->base + b->offset++)[0] = size >> (64 - 5);
}

// DT_BYTES
static inline void write_bytes_metadata(buffer_t *b, const size_t size)
{
    const size_t nbytes = USED_BYTES_64(size);
    const size_t shifted_size = LITTLE_64(size << 8);

    const uint64_t metadata = DT_BYTES | (nbytes << 4) | shifted_size;

    memcpy(b->base + b->offset, &metadata, 8);
    b->offset += nbytes + 1;
}

// DT_UTYPE
static inline void write_utype_metadata(buffer_t *b, const size_t size, const unsigned char id)
{
    const size_t nbytes = USED_BYTES_64(size);

    (b->base + b->offset++)[0] = DT_BYTES | (nbytes << 4);
    (b->base + b->offset++)[0] = id;

    const size_t _size = LITTLE_64(size);
    memcpy(b->base + b->offset, &_size, 8);
    b->offset += nbytes;
}

/* DATA FETCHING OF TYPES */

static void fetch_str(PyObject *obj, char **base, size_t *size)
{
    if (_LIKELY(PyUnicode_IS_COMPACT_ASCII(obj))) {
        *size = ((PyASCIIObject *)obj)->length;
        *base = (char *)(((PyASCIIObject *)obj) + 1);
    }
    else
    {
        *size = ((PyCompactUnicodeObject *)obj)->utf8_length;
        *base = ((PyCompactUnicodeObject *)obj)->utf8;
    }

    if (_UNLIKELY(base == NULL))
        *base = (char *)PyUnicode_AsUTF8AndSize(obj, (Py_ssize_t *)size);
}

static inline void fetch_bytes(PyObject *obj, char **base, size_t *size)
{
    *base = PyBytes_AS_STRING(obj);
    *size = PyBytes_GET_SIZE(obj);
}

// Zigzag-encode a long
static inline uint64_t zigzag_encode(const long x) {
    return ((uint64_t)x << 1) ^ (x >> (sizeof(long) * 8 - 1));
}
// Zigzag-decode a long
static inline long zigzag_decode(const uint64_t x) {
    return (long)(x >> 1) ^ -(long)(x & 1);
}

static bool fetch_long(PyObject *obj, uint64_t *num, size_t *size)
{
    // Use custom integer extraction on 3.12+
    #if PY_VERSION_HEX >= 0x030c0000

    PyLongObject *lobj = (PyLongObject *)obj;

    const size_t digits = lobj->long_value.lv_tag >> _PyLong_NON_SIZE_BITS;

    switch (digits)
    {
    case 0:
    case 1:
    {
        *num = lobj->long_value.ob_digit[0];
        break;
    }
    case 2:
    {
        *num = lobj->long_value.ob_digit[0] |
               (lobj->long_value.ob_digit[1] << PyLong_SHIFT);
        
        break;
    }
    case 3:
    {
        *num = (uint64_t)lobj->long_value.ob_digit[0] |
               ((uint64_t)lobj->long_value.ob_digit[1] << (PyLong_SHIFT * 1)) |
               ((uint64_t)lobj->long_value.ob_digit[2] << (PyLong_SHIFT * 2));
        
        const digit dig3 = lobj->long_value.ob_digit[2];
        const digit dig3_overflow = ((1ULL << (64 - (2 * PyLong_SHIFT))) - 1);

        // Don't break if there's overflow
        if (_LIKELY(dig3 < dig3_overflow))
            break;
    }
    default:
    {
        PyErr_SetString(PyExc_OverflowError, "integers cannot be more than 8 bytes");
        return false;
    }
    }

    bool neg = (lobj->long_value.lv_tag & _PyLong_SIGN_MASK) != 0;

    if (neg)
        *num = -*num;
    
    *num = zigzag_encode(*num);
    *size = USED_BYTES_64(*num);

    return true;

    #else

    int overflow = 0;
    long _num = PyLong_AsLongAndOverflow(obj, &overflow);

    if (_UNLIKELY(overflow != 0))
    {
        PyErr_SetString(PyExc_OverflowError, "Python ints cannot be more than 8 bytes");
        return false;
    }

    // Zigzag-encode the long into a uint
    *num = zigzag_encode(_num);
    // Get the number of bytes the num uses
    *size = USED_BYTES_64(*num);
    // Convert to little-endian (after getting num bytes, otherwise it'll be `8 - nbytes` on big-endian systems)
    *num = LITTLE_64(*num);

    return true;

    #endif
}

static void fetch_float(PyObject *obj, double *num)
{
    *num = PyFloat_AS_DOUBLE(obj);
    LITTLE_DOUBLE(*num);
    return;
}

/* ALLOCATION METHODS */

// Copied from cpython/Objects/bytesobject.c, as required for allocations
#include <stddef.h>
#define PyBytesObject_SIZE (offsetof(PyBytesObject, ob_sval) + 1)

#define INITIAL_ALLOC_SIZE 512

// Ensure that there's ENSURE_SIZE space available in a buffer
static inline bool buffer_ensure_space(buffer_t *b, size_t ensure_size)
{
    const size_t required_size = b->offset + ensure_size;
    if (_UNLIKELY(required_size >= b->allocated))
    {
        b->allocated = required_size * 1.5;

        PyBytesObject *new_ob = (PyBytesObject *)PyObject_Realloc(b->ob_base, PyBytesObject_SIZE + b->allocated);

        if (_UNLIKELY(new_ob == NULL))
        {
            PyErr_NoMemory();
            return false;
        }

        b->ob_base = new_ob;
        b->base = PyBytes_AS_STRING(new_ob);
    }

    return true;
}


/* ENCODING OF OBJECTS */

static bool encode_object(PyObject *obj, buffer_t *b)
{
    PyTypeObject *tp = Py_TYPE(obj);

    if (tp == &PyUnicode_Type)
    {
        char *base;
        size_t size;

        fetch_str(obj, &base, &size);

        // Ensure there's enough space for metadata plus the string size
        if (_UNLIKELY(!buffer_ensure_space(b, 8 + size)))
            return false;
        
        write_str_metadata(b, size);
        
        memcpy(b->base + b->offset, base, size);
        b->offset += size;

        return true;
    }
    else if (tp == &PyBytes_Type)
    {
        char *base;
        size_t size;

        fetch_bytes(obj, &base, &size);

        if (_UNLIKELY(!buffer_ensure_space(b, 8 + size)))
            return false;
        
        write_bytes_metadata(b, size);
        
        memcpy(b->base + b->offset, base, size);
        b->offset += size;

        return true;
    }

    // Ensure 9 bytes are available. This is the max size that'll be stored by any of the following types (except for utypes)
    if (_UNLIKELY(!buffer_ensure_space(b, 9)))
        return false;

    if (tp == &PyLong_Type)
    {
        uint64_t num;
        size_t size;

        if (_UNLIKELY(!fetch_long(obj, &num, &size)))
            return false;

        write_int_metadata_and_data(b, size, num);
    }
    else if (tp == &PyFloat_Type)
    {
        double num;
        fetch_float(obj, &num);

        LITTLE_DOUBLE(num);

        // No length metadata, is always 8
        (b->base + b->offset++)[0] = DT_FLOAT;

        memcpy(b->base + b->offset, &num, 8);
        b->offset += 8;
    }
    else if (tp == &PyBool_Type)
    {
        // Write a typemask based on if the value is true or false
        (b->base + b->offset++)[0] = obj == Py_True ? DT_BOOLT : DT_BOOLF;
    }
    else if (tp == Py_TYPE(Py_None)) // Public API doesn't expose a PyNone_Type
    {
        (b->base + b->offset++)[0] = DT_NONTP;
    }
    else if (tp == &PyList_Type)
    {
        // Get the number of items in the list
        const size_t nitems = PyList_GET_SIZE(obj);

        // Write metadata of the array
        write_container_metadata(b, nitems, DT_ARRAY);

        // Iterate over all items in the array and encode them
        for (size_t i = 0; i < nitems; ++i)
        {
            if (_UNLIKELY(!encode_object(PyList_GET_ITEM(obj, i), b)))
                return false;
        }
    }
    else if (tp == &PyDict_Type)
    {
        // Get the number of pairs in the dict
        const size_t npairs = PyDict_GET_SIZE(obj);

        write_container_metadata(b, npairs, DT_ARRAY);

        // Iterate over all pairs and encode them
        Py_ssize_t pos = 0;
        for (size_t i = 0; i < npairs; ++i)
        {
            PyObject *key;
            PyObject *val;

            PyDict_Next(obj, &pos, &key, &val);

            if (
                _UNLIKELY(!encode_object(key, b)) ||
                _UNLIKELY(!encode_object(val, b))
            ) return false;
        }
    }
    else
    {
        // Custom type logic will be placed here
        PyErr_SetString(PyExc_NotImplementedError, "CUSTOM/INCORRECT TYPES NOT IMPLEMENTED");
        return false;
    }

    return true;
}

static PyObject *encode(PyObject *self, PyObject *const *args, Py_ssize_t nargs)
{
    if (_UNLIKELY(nargs < 1))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the 'obj' argument");
        return NULL;
    }

    PyObject *obj = args[0];
    
    // Create a manually allocated bytes object
    PyObject *bytes_ob = (PyObject *)PyObject_Malloc(PyBytesObject_SIZE + INITIAL_ALLOC_SIZE);

    if (bytes_ob == NULL)
        return NULL;

    // Set up the bytes object
    PyBytesObject *bobj = (PyBytesObject *)bytes_ob;
    Py_SET_REFCNT(bobj, 1);
    Py_SET_TYPE(bobj, &PyBytes_Type);
    
    buffer_t b = {
        .ob_base = bobj,
        .base = PyBytes_AS_STRING(bobj),
        .allocated = INITIAL_ALLOC_SIZE,
        .offset = 0,
    };
    
    if (_UNLIKELY(encode_object(obj, &b) == false))
    {
        // Error should be set already
        PyObject_Free(b.ob_base);
        return NULL;
    }

    // Correct the size of the bytes object and null-terminate before returning
    Py_SET_SIZE(b.ob_base, b.offset);
    b.base[b.offset] = 0;

    return (PyObject *)b.ob_base;
}

/* MODULE DEFS */

static PyMethodDef CompaqtMethods[] = {
    {"encode", (PyCFunction)encode, METH_FASTCALL, NULL},
    //{"decode", (PyCFunction)decode, METH_VARARGS | METH_KEYWORDS, NULL},

    //{"validate", (PyCFunction)validate, METH_VARARGS | METH_KEYWORDS, NULL},

    //{"StreamEncoder", (PyCFunction)get_stream_encoder, METH_VARARGS | METH_KEYWORDS, NULL},
    //{"StreamDecoder", (PyCFunction)get_stream_decoder, METH_VARARGS | METH_KEYWORDS, NULL},

    {NULL, NULL, 0, NULL}
};

/*

// SETTINGS methods
static PyMethodDef SettingsMethods[] = {
    {"manual_allocations", (PyCFunction)manual_allocations, METH_VARARGS, NULL},
    {"dynamic_allocations", (PyCFunction)dynamic_allocations, METH_VARARGS | METH_KEYWORDS, NULL},

    {NULL, NULL, 0, NULL}
};

// SETTINGS object
static PyTypeObject settings_obj = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.settings",
    .tp_basicsize = sizeof(PyObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = SettingsMethods,
};

// TYPES methods
static PyMethodDef TypesMethods[] = {
    {"encoder_types", (PyCFunction)get_utypes_encode_ob, METH_VARARGS, NULL},
    {"decoder_types", (PyCFunction)get_utypes_decode_ob, METH_VARARGS, NULL},

    {NULL, NULL, 0, NULL}
};

// TYPES object
static PyTypeObject types_obj = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.types",
    .tp_basicsize = sizeof(PyObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = TypesMethods,
};

*/

// Main module
static struct PyModuleDef compaqt = {
    PyModuleDef_HEAD_INIT,
    "compaqt",
    NULL,
    -1,
    CompaqtMethods
};

// Module init function
PyMODINIT_FUNC PyInit_compaqt(void) {
    /* PREPARE CUSTOM TYPES */
    
    /*

    if (PyType_Ready(&settings_obj) < 0)
        return NULL;
    
    if (PyType_Ready(&types_obj) < 0)
        return NULL;
    if (PyType_Ready(&utypes_encode_t) < 0)
        return NULL;
    if (PyType_Ready(&utypes_decode_t) < 0)
        return NULL;
    
    if (PyType_Ready(&stream_encoder_t) < 0)
        return NULL;
    if (PyType_Ready(&stream_decoder_t) < 0)
        return NULL;
    
    if (PyType_Ready(&cbytes_t) < 0)
        return NULL;
    if (PyType_Ready(&cstr_t) < 0)
        return NULL;
    
    */

    /* CREATE MAIN MODULE */

    /*
    
    PyObject *m = PyModule_Create(&compaqt);
    if (m == NULL)
        return NULL;

    */
    
    /* CREATE CUSTOM OBJECTS */

    /*
    
    PyObject *settings = PyObject_New(PyObject, &settings_obj);
    PyObject *types = PyObject_New(PyObject, &types_obj);

    EncodingError = PyErr_NewException("compaqt.EncodingError", NULL, NULL);
    DecodingError = PyErr_NewException("compaqt.DecodingError", NULL, NULL);
    ValidationError = PyErr_NewException("compaqt.ValidationError", NULL, NULL);
    FileOffsetError = PyErr_NewException("compaqt.FileOffsetError", NULL, NULL);

    // Check if any object is NULL
    if (
        settings        == NULL ||
        types           == NULL ||

        EncodingError   == NULL ||
        DecodingError   == NULL ||
        ValidationError == NULL ||
        FileOffsetError == NULL
    )
    {
        // Cleanup any successfully created types and return NULL
        Py_XDECREF(settings);
        Py_XDECREF(types);

        Py_XDECREF(EncodingError);
        Py_XDECREF(DecodingError);
        Py_XDECREF(ValidationError);
        Py_XDECREF(FileOffsetError);

        Py_DECREF(m);
        return NULL;
    }

    */
    
    /* ADD CUSTOM OBJECTS */

    /*

    PyModule_AddObject(m, "settings", settings);
    PyModule_AddObject(m, "types", types);

    PyModule_AddObject(m, "EncodingError", EncodingError);
    PyModule_AddObject(m, "DecodingError", DecodingError);
    PyModule_AddObject(m, "ValidationError", ValidationError);
    PyModule_AddObject(m, "FileOffsetError", FileOffsetError);

    */

    PyObject *m = PyModule_Create(&compaqt); // temporary, is placed somewhere in commented out code
    return m;
}

