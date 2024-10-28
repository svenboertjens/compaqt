#define PY_SSIZE_CLEAN_T

#include <Python.h>

#include "main/regular.h"
#include "main/stream.h"
#include "main/validation.h"

#include "types/custom.h"

#include "settings/allocations.h"

/* MODULE DEFINITIONS */

static PyMethodDef CompaqtMethods[] = {
    {"encode", (PyCFunction)encode, METH_VARARGS | METH_KEYWORDS, NULL},
    {"decode", (PyCFunction)decode, METH_VARARGS | METH_KEYWORDS, NULL},

    {"validate", (PyCFunction)validate, METH_VARARGS | METH_KEYWORDS, NULL},

    {"StreamEncoder", (PyCFunction)get_stream_encoder, METH_VARARGS | METH_KEYWORDS, NULL},
    {"StreamDecoder", (PyCFunction)get_stream_decoder, METH_VARARGS | METH_KEYWORDS, NULL},

    {NULL, NULL, 0, NULL}
};

static PyMethodDef SettingsMethods[] = {
    {"manual_allocations", (PyCFunction)manual_allocations, METH_VARARGS, NULL},
    {"dynamic_allocations", (PyCFunction)dynamic_allocations, METH_VARARGS | METH_KEYWORDS, NULL},

    {NULL, NULL, 0, NULL}
};

static PyTypeObject settings_obj = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.settings",
    .tp_basicsize = sizeof(PyObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = SettingsMethods,
};

static PyMethodDef TypesMethods[] = {
    {"encoder_types", (PyCFunction)get_custom_types_wr, METH_VARARGS, NULL},
    {"decoder_types", (PyCFunction)get_custom_types_rd, METH_VARARGS, NULL},

    {NULL, NULL, 0, NULL}
};

static PyTypeObject types_obj = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.types",
    .tp_basicsize = sizeof(PyObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = TypesMethods,
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
    
    PyObject *m = PyModule_Create(&compaqt);
    if (m == NULL)
        return NULL;
    
    PyObject *settings = PyObject_New(PyObject, &settings_obj);
    PyObject *types = PyObject_New(PyObject, &types_obj);

    if (settings == NULL || types == NULL)
    {
        Py_XDECREF(settings);
        Py_XDECREF(types);
        Py_DECREF(m);
        return NULL;
    }

    PyModule_AddObject(m, "settings", settings);
    PyModule_AddObject(m, "types", types);

    return m;
}

