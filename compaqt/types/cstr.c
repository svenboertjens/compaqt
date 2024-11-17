#include <Python.h>
#include "globals/typedefs.h"
#include "types/strdata.h"
#include "types/base.h"

#include "globals/exceptions.h"

// Custom string object
typedef struct {
    PyObject_HEAD
    bufdata_t *bufd;
    char *data;
    Py_ssize_t len;
    Py_ssize_t codepoints;
} cstr_ob;

// Check if a cstr is ASCII by comparing the total length to the number of codepoints
#define IS_ASCII(ob) (ob->len == ob->codepoints)

PyTypeObject cstr_t;

static inline int get_ob_data(PyObject *self, char **data, Py_ssize_t *len)
{
    PyTypeObject *other_tp = Py_TYPE(self);

    if (other_tp == &PyUnicode_Type)
    {
        *data = (char *)PyUnicode_AsUTF8AndSize(self, len);

        if (data == NULL)
            return 1;
    }
    else if (other_tp == &cstr_t)
    {
        cstr_ob *ob = (cstr_ob *)self;

        *data = ob->data;
        *len = ob->len;
    }
    else
    {
        PyErr_Format(PyExc_ValueError, "Cannot add a 'compaqt.str' object to type '%s'", other_tp->tp_name);
        return 1;
    }

    return 0;
}

static inline PyObject *_cstr_add(const char *ldata, const char *rdata, const Py_ssize_t llen, const Py_ssize_t rlen)
{
    char *total = (char *)malloc(llen + rlen);

    if (total == NULL)
        return PyErr_NoMemory();

    memcpy(total, ldata, llen);
    memcpy(total + llen, rdata, rlen);

    PyObject *result = PyUnicode_DecodeUTF8(total, llen + rlen, "strict");

    free(total);
    return result;
}

static PyObject *cstr_add(PyObject *left, PyObject *right)
{
    char *ldata;
    char *rdata;

    Py_ssize_t llen;
    Py_ssize_t rlen;

    if (get_ob_data(left, &ldata, &llen) != 0)
        return NULL;

    if (get_ob_data(right, &rdata, &rlen) != 0)
        return NULL;
    
    return _cstr_add(ldata, rdata, llen, rlen);
}

static PyObject *cstr_concat(cstr_ob *ob, PyObject *args)
{
    if (PyTuple_GET_SIZE(args) != 1)
    {
        PyErr_SetString(PyExc_ValueError, "Concatting only supports 1 argument");
        return NULL;
    }

    PyObject *right = PyTuple_GET_ITEM(args, 0);

    char *rdata;
    Py_ssize_t rlen;

    if (get_ob_data(right, &rdata, &rlen) != 0)
        return NULL;
    
    return _cstr_add(ob->data, rdata, ob->len, rlen);
}

static PyObject *cstr_mul(cstr_ob *ob, PyObject *py_cnt)
{
    const Py_ssize_t cnt = PyLong_AsSsize_t(py_cnt);
    const size_t total_size = ob->len * cnt;

    char *total = (char *)malloc(total_size);

    if (total == NULL)
        return PyErr_NoMemory();
    
    for (size_t i = 0; i < cnt; ++i)
        memcpy(total + (ob->len * i), ob->data, ob->len);
    
    PyObject *result = PyUnicode_DecodeUTF8(total, total_size, "strict");

    free(total);
    return result;
}

static PyObject *cstr_getitem(PyObject *self, Py_ssize_t index)
{
    cstr_ob *ob = (cstr_ob *)self;

    if (index < 0)
        index = -(index + 1);
    
    if (index >= ob->codepoints)
    {
        PyErr_Format(PyExc_IndexError, "Index %zi is out of range (max is %zi)", index, ob->codepoints - 1);
        return NULL;
    }

    // Check if ASCII
    if (ob->len == ob->codepoints)
        return PyUnicode_DecodeUTF8(ob->data + index, 1, "strict");

    Py_ssize_t bytesize;
    Py_ssize_t offset = utf8_index(ob->data, ob->len, index, &bytesize);

    return PyUnicode_DecodeUTF8(ob->data + offset, bytesize, "strict");
}

static Py_ssize_t cstr_len(cstr_ob *ob)
{
    return ob->codepoints;
}

static PyObject *cstr_hash(cstr_ob *ob)
{
    Py_hash_t hash = _Py_HashBytes(ob->data, ob->len);
    return PyLong_FromLongLong(hash);
}

static PyObject *cstr_copy(cstr_ob *ob)
{
    Py_INCREF(ob);
    return (PyObject *)ob;
}

static int cstr_getbuffer(PyObject *self, Py_buffer *buf, int flags)
{
    if (buf == NULL)
    {
        PyErr_SetString(PyExc_BufferError, "Got a NULL buffer");
        return -1;
    }

    cstr_ob *ob = (cstr_ob *)self;

    buf->buf = (void *)ob->data;
    buf->len = ob->len;
    buf->readonly = 1;
    buf->itemsize = 1;
    buf->format = "B";
    buf->ndim = 1;
    buf->shape = NULL;
    buf->strides = NULL;
    buf->suboffsets = NULL;
    buf->internal = (void *)ob->bufd;

    ob->bufd->refcnt++;

    return 0;
}

static void cstr_releasebuffer(PyObject *self, Py_buffer *buf)
{
    DECREF_BUFD((bufdata_t *)buf->internal);
}

#define RETURN_TF(x) return ((x) ? Py_True : Py_False);

static PyObject *cstr_richcompare(PyObject *val1, PyObject *val2, int op)
{
    char *data1;
    char *data2;

    Py_ssize_t len1;
    Py_ssize_t len2;

    if (get_ob_data(val1, &data1, &len1) != 0)
        return NULL;

    if (get_ob_data(val2, &data2, &len2) != 0)
        return NULL;

    switch (op)
    {
    case Py_EQ:
    {
        if (len1 != len2)
            Py_RETURN_FALSE;
        
        if (strncmp(data1, data2, len1))
            Py_RETURN_FALSE;
        
        Py_RETURN_TRUE;
    }
    case Py_NE:
    {
        if (len1 != len2)
            Py_RETURN_TRUE;
        
        if (strncmp(data1, data2, len1))
            Py_RETURN_TRUE;
        
        Py_RETURN_FALSE;
    }
    case Py_LT: RETURN_TF(len1 < len2)
    case Py_LE: RETURN_TF(len1 <=len2)
    case Py_GT: RETURN_TF(len1 > len2)
    case Py_GE: RETURN_TF(len1 >=len2)
    default: Py_RETURN_NOTIMPLEMENTED;
    }
}

static PyObject *cstr_count(cstr_ob *ob, PyObject *args, PyObject *kwargs)
{
    PyObject *py_pattern;
    Py_ssize_t start = 0;
    Py_ssize_t end = ob->len;

    static char *kwlist[] = {"x", "start", "end", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|nn", kwlist, &py_pattern, &start, &end))
        return NULL;
    
    Py_ssize_t pattern_len;
    const char *pattern = PyUnicode_AsUTF8AndSize(py_pattern, &pattern_len);
    
    Py_ssize_t count = utf8_count(ob->data + start, end - start, pattern, pattern_len, 0);

    return PyLong_FromSsize_t(count);
}

static PyObject *cstr_str(cstr_ob *ob)
{
    return PyUnicode_DecodeUTF8(ob->data, ob->len, "strict");
}

static PyObject *cstr_sizeof(cstr_ob *ob)
{
    return PyLong_FromSize_t(sizeof(cstr_ob));
}

static PyMethodDef cstr_methods[] = {
    {"concat", (PyCFunction)cstr_concat, METH_VARARGS, NULL},
    {"copy", (PyCFunction)cstr_copy, METH_VARARGS, NULL},
    {"count", (PyCFunction)cstr_count, METH_VARARGS | METH_KEYWORDS, NULL},
    {"__sizeof__", (PyCFunction)cstr_sizeof, METH_NOARGS, NULL},

    {NULL, NULL, 0, NULL}
};

static PySequenceMethods cstr_assequence = {
    .sq_length = (lenfunc)cstr_len,
    .sq_item = (ssizeargfunc)cstr_getitem,
};

static PyNumberMethods cstr_asnumber = {
    .nb_add = (binaryfunc)cstr_add,
    .nb_multiply = (binaryfunc)cstr_mul,
};

static PyBufferProcs cstr_asbuffer = {
    .bf_getbuffer = cstr_getbuffer,
    .bf_releasebuffer = cstr_releasebuffer,
};

void cstr_dealloc(cstr_ob *ob)
{
    DECREF_BUFD(ob->bufd);
    PyObject_Del(ob);
}

PyTypeObject cstr_t = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "compaqt.str",
    .tp_doc = "a string object referencing a buffer of another object",
    .tp_basicsize = sizeof(cstr_ob),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_alloc = PyType_GenericAlloc,
    .tp_dealloc = (destructor)cstr_dealloc,
    .tp_free = PyObject_Del,
    .tp_str = (reprfunc)cstr_str,
    .tp_repr = (reprfunc)cstr_str,
    .tp_hash = (hashfunc)cstr_hash,
    .tp_methods = cstr_methods,
    .tp_richcompare = (richcmpfunc)cstr_richcompare,
    .tp_as_sequence = &cstr_assequence,
    .tp_as_number = &cstr_asnumber,
    .tp_as_buffer = &cstr_asbuffer,
    .tp_itemsize = sizeof(cstr_ob),
};

PyObject *cstr_create(bufdata_t *bufd, char *data, Py_ssize_t len)
{
    Py_ssize_t codepoints = utf8_codepoints(data, len);

    if (codepoints == -1)
    {
        PyErr_SetString(DecodingError, "Found a string object with invalid UTF-8 data");
        return NULL;
    }

    cstr_ob *ob = PyObject_New(cstr_ob, &cstr_t);

    if (ob == NULL)
        return PyErr_NoMemory();

    ob->bufd = bufd;
    ob->data = data;
    ob->len = len;
    ob->codepoints = codepoints;

    INCREF_BUFD(ob->bufd);

    return (PyObject *)ob;
}

