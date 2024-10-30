#define PY_SSIZE_CLEAN_T

#include <Python.h>

#include "main/regular.h"
#include "main/stream.h"
#include "main/validation.h"

#include "types/custom.h"

#include "settings/allocations.h"

#include "exceptions.h"

/* MODULE DEFINITIONS */

// Main module methods
static PyMethodDef CompaqtMethods[] = {
    {"encode", (PyCFunction)encode, METH_VARARGS | METH_KEYWORDS, NULL},
    {"decode", (PyCFunction)decode, METH_VARARGS | METH_KEYWORDS, NULL},

    {"validate", (PyCFunction)validate, METH_VARARGS | METH_KEYWORDS, NULL},

    {"StreamEncoder", (PyCFunction)get_stream_encoder, METH_VARARGS | METH_KEYWORDS, NULL},
    {"StreamDecoder", (PyCFunction)get_stream_decoder, METH_VARARGS | METH_KEYWORDS, NULL},

    {NULL, NULL, 0, NULL}
};

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
    {"encoder_types", (PyCFunction)get_custom_types_wr, METH_VARARGS, NULL},
    {"decoder_types", (PyCFunction)get_custom_types_rd, METH_VARARGS, NULL},

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

// Main module
static struct PyModuleDef compaqt = {
    PyModuleDef_HEAD_INIT,
    "compaqt",
    NULL,
    -1,
    CompaqtMethods
};

// Macro to call on init failure
#define INIT_CLEANUP do { \
    Py_XDECREF(settings); \
    Py_XDECREF(types); \
    Py_XDECREF(EncodingError); \
    Py_XDECREF(DecodingError); \
    Py_XDECREF(ValidationError); \
    Py_XDECREF(FileOffsetError); \
    \
    /* Module is never NULL here */ \
    Py_DECREF(m); \
    return NULL; \
} while (0)

// Module init function
PyMODINIT_FUNC PyInit_compaqt(void) {
    /* PREPARE CUSTOM TYPES */

    if (PyType_Ready(&settings_obj) < 0)
        return NULL;
    
    if (PyType_Ready(&types_obj) < 0)
        return NULL;
    if (PyType_Ready(&custom_types_wr_t) < 0)
        return NULL;
    if (PyType_Ready(&custom_types_rd_t) < 0)
        return NULL;
    
    if (PyType_Ready(&stream_encoder_t) < 0)
        return NULL;
    if (PyType_Ready(&stream_decoder_t) < 0)
        return NULL;

    /* CREATE MAIN MODULE */
    
    PyObject *m = PyModule_Create(&compaqt);
    if (m == NULL)
        return NULL;
    
    /* CREATE CUSTOM OBJECTS */
    
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
    
    /* ADD CUSTOM OBJECTS */

    PyModule_AddObject(m, "settings", settings);
    PyModule_AddObject(m, "types", types);

    PyModule_AddObject(m, "EncodingError", EncodingError);
    PyModule_AddObject(m, "DecodingError", DecodingError);
    PyModule_AddObject(m, "ValidationError", ValidationError);
    PyModule_AddObject(m, "FileOffsetError", FileOffsetError);

    return m;
}

