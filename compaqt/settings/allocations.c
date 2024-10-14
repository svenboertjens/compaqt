#include <Python.h>
#include "metadata.h"

// Function to set manual allocation settings
PyObject *manual_allocations(PyObject *self, PyObject *args)
{
    Py_ssize_t item_size;
    Py_ssize_t realloc_size;

    if (!PyArg_ParseTuple(args, "nn", &item_size, &realloc_size))
        return NULL;

    if (item_size <= 0 || realloc_size <= 0)
    {
        PyErr_SetString(PyExc_ValueError, "Size values must be larger than zero");
        return NULL;
    }

    dynamic_allocation_tweaks = 0;
    avg_item_size = (size_t)item_size;
    avg_realloc_size = (size_t)realloc_size;

    Py_RETURN_NONE;
}

// Function to set dynamic allocation settings
PyObject *dynamic_allocations(PyObject *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t item_size = 0;
    Py_ssize_t realloc_size = 0;

    static char *kwlist[] = {"item_size", "realloc_size", NULL};

    // Don't check args issues as all are optional
    PyArg_ParseTupleAndKeywords(args, kwargs, "|nn", kwlist, &item_size, &realloc_size);

    if (item_size < 0 || realloc_size < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Size values must be larger than zero");
        return NULL;
    }

    dynamic_allocation_tweaks = 1;

    if (item_size != 0)
        avg_item_size = (size_t)item_size;
    if (realloc_size != 0)
        avg_realloc_size = (size_t)realloc_size;

    Py_RETURN_NONE;
}