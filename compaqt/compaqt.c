#define PY_SSIZE_CLEAN_T

#include <Python.h>
#include "globals.h"
#include "regular.h"

#define ENCODE_SINGLE_VALUE_TOFILE(expected, dt_mask, DT_LN, DT_WR) do { \
    if (type != &expected) \
    { \
        PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", type->tp_name); \
        fclose(file); \
        return NULL; \
    } \
    \
    const size_t length = DT_LN(value); \
    \
    msg = (char *)malloc(length + 1); \
    if (msg == NULL) \
    { \
        PyErr_NoMemory(); \
        fclose(file); \
        return NULL; \
    } \
    \
    offset = 1; \
    DT_WR(value, length); \
    *(msg) = dt_mask; \
    \
    fwrite(&msg, length + 1, 1, file); \
    fclose(file); \
    \
    free(msg); \
    Py_RETURN_NONE; \
} while (0)

// Main function for encoding objects
static PyObject *encode(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value;

    static char *kwlist[] = {"value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|", kwlist, &value))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the 'value' (any) input");
        return NULL;
    }

    return encode_regular(value);
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

    return decode_regular(value);
}

/* VALIDATION */

#define VALIDATE_OVERREAD_CHECK(length) do { \
    if (b->offset + length > b->allocated) return 1; \
} while (0)

static inline int _validate(buffer_t *b)
{
    VALIDATE_OVERREAD_CHECK(1);

    switch (RD_DTMASK())
    {
    case DT_GROUP: // Group datatypes
    {
        switch (RD_DTMASK_GROUP())
        {
        // Static lengths, no metadata to read, already incremented by `RD_DTMASK_GROUP`
        case DT_FLOAT: VALIDATE_OVERREAD_CHECK(8); b->offset += 8;  return 0;
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
        RD_METADATA_VLE(num_items);
        VALIDATE_OVERREAD_CHECK(0); // Check whether the metadata reading didn't cross the boundary

        for (size_t i = 0; i < num_items; ++i)
            if (_validate(b) == 1) return 1;
        
        return 0;
    }
    case DT_DICTN:
    {
        size_t num_items = 0;
        RD_METADATA_VLE(num_items);
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
        RD_METADATA_VLE(length);
        VALIDATE_OVERREAD_CHECK(length);
        b->offset += length;
        return 0;
    }
    }
}

static PyObject *validate(PyObject *self, PyObject *args, PyObject *kwargs)
{
    PyObject *value;
    int err_on_invalid = 0;

    static char *kwlist[] = {"value", "error_on_invalid", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|i", kwlist, &PyBytes_Type, &value, &err_on_invalid))
    {
        PyErr_SetString(PyExc_ValueError, "Expected at least the 'value' (bytes) argument.");
        return NULL;
    }

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
    {"validate", (PyCFunction)validate, METH_VARARGS | METH_KEYWORDS, "Validate an encoded object"},
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

