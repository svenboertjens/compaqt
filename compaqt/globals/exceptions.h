// This file contains custom error types

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <Python.h>

// Error for when encoding data
extern PyObject *EncodingError;

// Error for when decoding data
extern PyObject *DecodingError;

// Error for when validating data
extern PyObject *ValidationError;

// Error for when finding a file offset
extern PyObject *FileOffsetError;

#endif // EXCEPTIONS_H