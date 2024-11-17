#include <Python.h>
#include "globals/typedefs.h"
#include "types/base.h"

// Custom bytes object
typedef struct {
    PyObject_HEAD
    bufdata_t *bufd;
    char *data;
    Py_ssize_t len;
} cbytes_ob;

PyTypeObject cbytes_t;

static PyObject *cbytes_concat(cbytes_ob *ob, PyObject *args)
{
    char *other_data;
    Py_ssize_t other_len;

    if (Py_SIZE(args) != 1)
    {
        PyErr_SetString(PyExc_ValueError, "Expected just one argument with 'concat'");
        return NULL;
    }
    
    PyObject *other = PyTuple_GET_ITEM(args, 0);
    PyTypeObject *other_tp = Py_TYPE(other);

    if (other_tp == &PyBytes_Type)
    {
        PyBytes_AsStringAndSize(other, &other_data, &other_len);
    }
    else if (other_tp == &cbytes_t)
    {
        cbytes_ob *other_ob = (cbytes_ob *)other;

        other_data = other_ob->data;
        other_len = other_ob->len;
    }
    else
    {
        PyErr_Format(PyExc_ValueError, "Concatting with a Compaqt bytes only supports a Python bytes or another Compaqt bytes, got '%s'", other_tp->tp_name);
        return NULL;
    }

    char *total = (char *)malloc(ob->len + other_len);

    memcpy(total, ob->data, ob->len);
    memcpy(total + ob->len, other_data, other_len);

    PyObject *result = PyBytes_FromStringAndSize(total, ob->len + other_len);

    free(total);
    return result;
}

static PyMethodDef cbytes_methods[] = {
    {"concat", (PyCFunction)cbytes_concat, METH_VARARGS, NULL},

    {NULL, NULL, 0, NULL}
};

void cbytes_dealloc(cbytes_ob *ob)
{
    DECREF_BUFD(ob->bufd);
    PyObject_Del(ob);
}

PyTypeObject cbytes_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.bytes",
    .tp_basicsize = sizeof(cbytes_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_methods = cbytes_methods,
    .tp_new = PyType_GenericNew,
    .tp_alloc = PyType_GenericAlloc,
    .tp_dealloc = (destructor)cbytes_dealloc,
    .tp_free = PyObject_Del
};

PyObject *cbytes_create(bufdata_t *bufd, const char *data, const Py_ssize_t len)
{
    cbytes_ob *ob = PyObject_New(cbytes_ob, &cbytes_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    ob->bufd = bufd;
    ob->data = data;
    ob->len = len;

    INCREF_BUFD(ob->bufd);

    return (PyObject *)ob;
}

