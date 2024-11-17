#ifndef CSTR_H
#define CSTR_H

#include <Python.h>
#include "globals/typedefs.h"
#include "types/base.h"

typedef struct {
    PyObject_HEAD
    bufdata_t *bufd;
    char *data;
    Py_ssize_t len;
    Py_ssize_t codepoints;
} cstr_ob;

extern PyTypeObject cstr_t;

PyObject *cstr_create(bufdata_t *bufd, char *data, Py_ssize_t len);

#endif // CSTR_H