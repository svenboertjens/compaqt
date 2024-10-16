#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <Python.h>
#include "metadata.h"

typedef struct {
    char *msg;
    size_t offset;
    size_t allocated;
} buffer_t;

typedef int (*buffer_check_t)(buffer_t *, const size_t);

int encode_item(buffer_t *b, PyObject *item, buffer_check_t offset_check);
PyObject *decode_item(buffer_t *b, buffer_check_t overread_check);

#endif // SERIALIZATION_H