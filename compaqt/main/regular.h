#ifndef REGULAR_H
#define REGULAR_H

#include <Python.h>

PyObject *encode(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject *decode(PyObject *self, PyObject *args, PyObject *kwargs);

#endif // REGULAR_H