#ifndef ALLOCATIONS_H
#define ALLOCATIONS_H

#include <Python.h>

extern size_t avg_item_size;
extern size_t avg_realloc_size;
extern int dynamic_allocation_tweaks;

PyObject *manual_allocations(PyObject *self, PyObject *args);
PyObject *dynamic_allocations(PyObject *self, PyObject *args, PyObject *kwargs);

void update_allocation_settings(const int reallocs, const size_t offset, const size_t initial_allocated, const size_t nitems);

#endif // ALLOCATIONS_H