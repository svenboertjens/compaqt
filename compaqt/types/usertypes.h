#ifndef USERTYPES_H
#define USERTYPES_H

#include <Python.h>
#include "globals/typedefs.h"

extern PyTypeObject utypes_encode_t;
extern PyTypeObject utypes_decode_t;

PyObject *get_utypes_encode_ob(PyObject *self, PyObject *args);
PyObject *get_utypes_decode_ob(PyObject *self, PyObject *args);

int encode_custom(encode_t *b, PyObject *value);
PyObject *decode_custom(decode_t *b);

#endif // USERTYPES_H