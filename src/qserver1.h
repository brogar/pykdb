#ifndef QSERVER1_H
#define QSERVER1_H

#include <Python.h>

static PyObject* kx_py_open_connection(PyObject* self, PyObject* args) ;
static PyObject* kx_py_close_connection(PyObject* self, PyObject* args) ;
static PyObject* kx_py_execute(PyObject* self, PyObject* args) ;

#endif
