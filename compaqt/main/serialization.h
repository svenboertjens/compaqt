#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <Python.h>
#include "globals/typedefs.h"

int encode_object(encode_t *b, PyObject *item);
PyObject *decode_bytes(decode_t *b);

#endif // SERIALIZATION_H