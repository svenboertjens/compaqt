// This file contains custom datatype handling

#include "main/serialization.h"

typedef struct {
    PyObject_HEAD
    size_t amt;           // The amount of types
    PyTypeObject **types; // The actual types of the values
    PyObject **writes;    // The write functions of the types
} custom_types_wr_ob;

typedef struct {
    PyObject_HEAD
    size_t amt;
    // No types list is necessary as indexing the read function is done based on metadata in encoded data
    PyObject **reads; // The read functions of the types
} custom_types_rd_ob;

/* OBJECT CREATION */

static void custom_types_wr_dealloc(custom_types_wr_ob *self)
{
    for (size_t i = 0; i < self->amt; ++i)
    {
        Py_DECREF(self->types[i]);
        Py_DECREF(self->writes[i]);
    }

    free(self->types);
    free(self->writes);

    if (Py_TYPE(self)->tp_free)
        Py_TYPE(self)->tp_free((PyObject *)self);
    else
        PyObject_Del(self);
}

static void custom_types_rd_dealloc(custom_types_rd_ob *self)
{
    for (size_t i = 0; i < self->amt; ++i)
        Py_DECREF(self->reads[i]);

    free(self->reads);

    if (Py_TYPE(self)->tp_free)
        Py_TYPE(self)->tp_free((PyObject *)self);
    else
        PyObject_Del(self);
}

PyTypeObject custom_types_wr_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.CustomTypesWrite",
    .tp_basicsize = sizeof(custom_types_wr_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)custom_types_wr_dealloc,
};

PyTypeObject custom_types_rd_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.CustomTypesRead",
    .tp_basicsize = sizeof(custom_types_rd_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)custom_types_rd_dealloc,
};

PyObject *get_custom_types_wr(PyObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &data))
        return NULL;

    custom_types_wr_ob *ob = PyObject_New(custom_types_wr_ob, &custom_types_wr_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    ob->amt = PyDict_GET_SIZE(data);

    ob->types = (PyTypeObject **)malloc(sizeof(PyTypeObject *) * ob->amt);

    if (ob->types == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }

    ob->writes = (PyObject **)malloc(sizeof(PyObject *) * ob->amt);

    if (ob->writes == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }
    
    PyObject *key;
    PyObject *val;
    Py_ssize_t *pos = 0;

    for (size_t i = 0; i < ob->amt; ++i)
    {
        PyDict_Next(data, &pos, &key, &val);

        if (!PyType_Check(key))
        {
            PyErr_Format(PyExc_ValueError, "Keys must be type objects, got '%s'", Py_TYPE(key)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        if (!PyFunction_Check(val))
        {
            PyErr_Format(PyExc_ValueError, "Values must be functions, got '%s'", Py_TYPE(val)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        Py_INCREF(key);
        Py_INCREF(val);

        ob->types[i] = ((PyTypeObject *)key);
        ob->writes[i] = val;
    }

    return (PyObject *)ob;
}

PyObject *get_custom_types_rd(PyObject *self, PyObject *args)
{
    PyObject *data;

    if (!PyArg_ParseTuple(args, "O!", &PyTuple_Type, &data))
        return NULL;

    custom_types_rd_ob *ob = PyObject_New(custom_types_rd_ob, &custom_types_rd_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    ob->amt = PyTuple_GET_SIZE(data);

    ob->reads = (PyObject **)malloc(sizeof(PyObject *) * ob->amt);

    if (ob->reads == NULL)
    {
        Py_DECREF(ob);
        return PyErr_NoMemory();
    }
    
    for (size_t i = 0; i < ob->amt; ++i)
    {
        PyObject *val = PyTuple_GET_ITEM(data, i);

        if (!PyFunction_Check(val))
        {
            PyErr_Format(PyExc_ValueError, "Values must be functions, got '%s'", Py_TYPE(val)->tp_name);
            Py_DECREF(ob);
            return NULL;
        }

        Py_INCREF(val);

        ob->reads[i] = val;
    }

    return (PyObject *)ob;
}

/* SERIALIZATION */

int encode_custom(buffer_t *b, PyObject *value, custom_types_wr_ob *ob, buffer_check_t offset_check)
{
    if (ob == NULL)
        return 1;
    
    PyTypeObject *type = Py_TYPE(value);

    for (size_t i = 0; i < ob->amt; ++i)
    {
        if (ob->types[i] == type)
        {
            PyObject *result = PyObject_CallFunctionObjArgs(ob->writes[i], value, NULL);

            if (!PyBytes_Check(result))
            {
                PyErr_Format(PyExc_ValueError, "Expected a 'bytes' object from a custom type write function, got '%s'", Py_TYPE(result)->tp_name);
                Py_DECREF(result);
                return 1;
            }

            char *ptr;
            size_t length;
            PyBytes_AsStringAndSize(result, &ptr, &length);

            offset_check(b, length + MAX_METADATA_SIZE);

            *(b->msg + b->offset++) = DT_EXTND | (i << 3);

            const size_t num_bytes = USED_BYTES_64(length);

            memcpy(b->msg + b->offset, ptr, length);

            Py_DECREF(result);
            return 0;
        }
    }

    PyErr_Format(PyExc_ValueError, "Received unsupported datatype '%s'", Py_TYPE(value)->tp_name);
    return 1;
}

