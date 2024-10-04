#ifndef REGULAR_H
#define REGULAR_H

#include <Python.h>
#include "globals.h"

extern size_t avg_item_size;
extern size_t avg_realloc_size;
extern char dynamic_allocation_tweaks;

PyObject *encode_regular(PyObject *value);
PyObject *decode_regular(PyObject *value);

#endif // REGULAR_H