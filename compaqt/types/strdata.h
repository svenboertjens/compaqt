#ifndef STRDATA_H
#define STRDATA_H

#include <Python.h>

Py_ssize_t utf8_codepoints(char *data, const Py_ssize_t len);
Py_ssize_t utf8_index(char *data, const Py_ssize_t len, const Py_ssize_t codepoint, Py_ssize_t *bytesize);
Py_ssize_t utf8_count(char *data, const Py_ssize_t data_len, const char *pattern, const Py_ssize_t pattern_len, const int overlap);

#endif // STRDATA_H