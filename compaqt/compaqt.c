#define PY_SSIZE_CLEAN_T

#include <Python.h>

#include "main/regular.h"
#include "main/stream.h"
#include "main/validation.h"

#include "settings/allocations.h"

/* MODULE DEFINITIONS */

static PyMethodDef CompaqtMethods[] = {
    {"encode", (PyCFunction)encode, METH_VARARGS | METH_KEYWORDS, "Encode a value to bytes"},
    {"decode", (PyCFunction)decode, METH_VARARGS | METH_KEYWORDS, "Decode a value from bytes"},
    {"validate", (PyCFunction)validate, METH_VARARGS | METH_KEYWORDS, "Validate an encoded object"},
    {"StreamEncoder", (PyCFunction)get_stream_encoder, METH_VARARGS | METH_KEYWORDS, "Get a stream encoder object to serialize data to a file"},
    {"StreamDecoder", (PyCFunction)get_stream_decoder, METH_VARARGS | METH_KEYWORDS, "Get a stream decoder object to de-serialize binary from a file"},
    {NULL, NULL, 0, NULL}
};

// Submodule for settings
static PyMethodDef SettingsMethods[] = {
    {"manual_allocations", (PyCFunction)manual_allocations, METH_VARARGS, "Set manual allocation settings"},
    {"dynamic_allocations", (PyCFunction)dynamic_allocations, METH_VARARGS | METH_KEYWORDS, "Enable dynamic allocation settings, and optionally set start values"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject settings_obj = {
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
    if (PyType_Ready(&settings_obj) < 0)
        return NULL;
    if (PyType_Ready(&PyStreamEncoderType) < 0)
        return NULL;
    if (PyType_Ready(&PyStreamDecoderType) < 0)
        return NULL;

    // Create the main module
    PyObject *m = PyModule_Create(&compaqt);
    if (m == NULL) return NULL;

    // Create settings submodule
    PyObject *settings = PyObject_New(PyObject, &settings_obj);
    if (settings == NULL)
    {
        Py_DECREF(m);
        return NULL;
    }

    // Add submodules to the main module
    PyModule_AddObject(m, "settings", settings);

    return m;
}

