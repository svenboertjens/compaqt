#ifndef STREAM_H
#define STREAM_H

#include <Python.h>
#include "globals.h"

PyObject *get_stream_encoder(PyObject *self, PyObject *args, PyObject *kwargs);

#endif // STREAM_H