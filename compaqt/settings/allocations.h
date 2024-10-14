#ifndef ALLOCATIONS_H
#define ALLOCATIONS_H

#include <Python.h>
#include "metadata.h"

PyObject *manual_allocations(PyObject *self, PyObject *args);
PyObject *dynamic_allocations(PyObject *self, PyObject *args, PyObject *kwargs);

#endif // ALLOCATIONS_H