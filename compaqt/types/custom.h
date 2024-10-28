#ifndef CUSTOM_H
#define CUSTOM_H

#include "metadata.h"

typedef struct {
    PyObject_HEAD
    PyTypeObject **types;
    PyObject **writes;
} custom_types_wr_ob;

typedef struct {
    PyObject_HEAD
    PyObject **reads;
} custom_types_rd_ob;

void custom_types_wr_dealloc(custom_types_wr_ob *self);
void custom_types_rd_dealloc(custom_types_rd_ob *self);

extern PyTypeObject custom_types_wr_t;
extern PyTypeObject custom_types_rd_t;

PyObject *get_custom_types_wr(PyObject *self, PyObject *args);
PyObject *get_custom_types_rd(PyObject *self, PyObject *args);

int encode_custom(buffer_t *b, PyObject *value, custom_types_wr_ob *ob, buffer_check_t offset_check);
PyObject *decode_custom(buffer_t *b, custom_types_rd_ob *ob, buffer_check_t overread_check);

#endif //CUSTOM_H