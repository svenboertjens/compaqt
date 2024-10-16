#ifndef REGULAR_H
#define REGULAR_H

#include <Python.h>
#include "metadata.h"
#include "serialization.h"

PyObject *encode(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject *decode(PyObject *self, PyObject *args, PyObject *kwargs);

#endif // REGULAR_H