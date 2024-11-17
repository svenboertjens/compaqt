#ifndef CBYTES_H
#define CBYTES_H

#include <Python.h>
#include "typedefs.h"
#include "types/base.h"

typedef struct {
    PyObject_HEAD
    bufdata_t *bufd;
    char *data;
    Py_ssize_t len;
} cbytes_ob;

extern PyTypeObject cbytes_t;

PyObject *cbytes_create(bufdata_t *bufd, const char *data, const Py_ssize_t len);

#endif // CBYTES_H