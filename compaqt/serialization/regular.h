#ifndef REGULAR_H
#define REGULAR_H

#include <Python.h>
#include "globals.h"

PyObject *encode_regular(PyObject *value);
PyObject *decode_regular(PyObject *value);

#endif // REGULAR_H