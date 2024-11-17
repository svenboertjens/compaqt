#ifndef BASE_H
#define BASE_H

#include <Python.h>

#define INCREF_BUFD(bufd) do { \
    ++((bufd)->refcnt); \
} while (0)

#define DECREF_BUFD(bufd) do { \
    if (--((bufd)->refcnt) == 0) \
    { \
        if ((bufd)->is_PyOb == 1) \
            Py_DECREF((bufd)->base); \
        else \
            free((bufd)->base); \
        PyObject_Free(bufd); \
    } \
} while (0)

#endif // BASE_H