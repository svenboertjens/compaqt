#include <Python.h>

#define AVG_REALLOC_MIN 64
#define AVG_ITEM_MIN 4

size_t avg_item_size = 12;
size_t avg_realloc_size = 128;
int dynamic_allocation_tweaks = 1;

// Function to set manual allocation settings
PyObject *manual_allocations(PyObject *self, PyObject *args)
{
    Py_ssize_t item_size;
    Py_ssize_t realloc_size;

    if (!PyArg_ParseTuple(args, "nn", &item_size, &realloc_size))
        return NULL;

    if (item_size <= 0 || realloc_size <= 0)
    {
        PyErr_SetString(PyExc_ValueError, "Size values must be larger than zero");
        return NULL;
    }

    dynamic_allocation_tweaks = 0;
    avg_item_size = (size_t)item_size;
    avg_realloc_size = (size_t)realloc_size;

    Py_RETURN_NONE;
}

// Function to set dynamic allocation settings
PyObject *dynamic_allocations(PyObject *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t item_size = 0;
    Py_ssize_t realloc_size = 0;

    static char *kwlist[] = {"item_size", "realloc_size", NULL};

    // Don't check args issues as all are optional
    PyArg_ParseTupleAndKeywords(args, kwargs, "|nn", kwlist, &item_size, &realloc_size);

    if (item_size < 0 || realloc_size < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Size values must be larger than zero");
        return NULL;
    }

    dynamic_allocation_tweaks = 1;

    if (item_size != 0)
        avg_item_size = (size_t)item_size;
    if (realloc_size != 0)
        avg_realloc_size = (size_t)realloc_size;

    Py_RETURN_NONE;
}

void update_allocation_settings(const int reallocs, const size_t offset, const size_t initial_allocated, const size_t nitems)
{
    if (dynamic_allocation_tweaks == 1)
    {
        if (reallocs != 0)
        {
            const size_t difference = offset - initial_allocated;
            const size_t med_diff = difference / (nitems + 1);

            avg_realloc_size += difference >> 1;
            avg_item_size += med_diff >> 1;
        }
        else
        {
            const size_t difference = initial_allocated - offset;
            const size_t med_diff = difference / (nitems + 1);
            const size_t diff_small = difference >> 4;
            const size_t med_small = med_diff >> 5;

            if (diff_small + AVG_REALLOC_MIN < avg_realloc_size)
                avg_realloc_size -= diff_small;
            else
                avg_realloc_size = AVG_REALLOC_MIN;

            if (med_small + AVG_ITEM_MIN < avg_item_size)
                avg_item_size -= med_small;
            else
                avg_item_size = AVG_ITEM_MIN;
        }
    }
}

