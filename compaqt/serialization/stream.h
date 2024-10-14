#ifndef STREAM_H
#define STREAM_H

#include <Python.h>
#include "metadata.h"

extern PyTypeObject PyStreamEncoderType;
extern PyTypeObject PyStreamDecoderType;

PyObject *get_stream_encoder(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject *get_stream_decoder(PyObject *self, PyObject *args, PyObject *kwargs);

#endif // STREAM_H