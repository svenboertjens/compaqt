#ifndef STREAM_H
#define STREAM_H

#include <Python.h>

extern PyTypeObject stream_encoder_t;
extern PyTypeObject stream_decoder_t;

PyObject *get_stream_encoder(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject *get_stream_decoder(PyObject *self, PyObject *args, PyObject *kwargs);

#endif // STREAM_H