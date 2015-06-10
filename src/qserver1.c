#include <Python.h>
#include <datetime.h>

#include <stdio.h>
#include <stdlib.h>

#include <common1.h>
#include <k.h>

static
PyObject* kx_py_open_connection(PyObject* self, PyObject* args) {
     int connection=0, port;
     char *host = NULL;
     char *user = NULL;

     if(!PyArg_ParseTuple(args, "si|s", &host, &port, &user)) {
	  return NULL;
     } 
	
     if(!user) {
	  connection = khp(host, port);
     } else {
	  connection = khpu(host, port, user);
     }

     if(connection <= 0 ) {
	  PyErr_SetString(PyExc_RuntimeError, "Error: Unable to connect to kdb+ server\n") ;
	  return NULL;
     }

     return Py_BuildValue("i", connection);
}

static
PyObject* kx_py_close_connection(PyObject* self, PyObject* args) {
     int connection; 

     if(!PyArg_ParseTuple(args, "i", &connection)) {
	  return NULL;
     }

     /* Close the connection. */
     kclose(connection);

     return Py_BuildValue("i", 0);
}

static
PyObject* kx_py_execute(PyObject* self, PyObject* args) {
     K result;
     PyObject* s;
     int connection;
     const char *query = NULL;

     if(!PyArg_ParseTuple(args, "is", &connection, &query)){
	  return NULL;
     }
	
     result = k(connection, (char*)query, (K)0);
     if (0 == result) {
	  PyErr_SetString(PyExc_RuntimeError, "Error: not connected to kdb+ server\n") ;
	  return NULL;
     }
     else if (-128 == result->t) {
	  char e[5001]={'\0'};
	  snprintf(e, 5000, "Error: `%s", result->s);
	  r0(result);
	  PyErr_SetString(PyExc_RuntimeError, e);
	  return NULL ;
     }
     s = from_any_kobject(result);
     r0(result);
     return s;
}

static 
PyMethodDef PykdbMethods[] = {
     {"kx_py_open_connection",  kx_py_open_connection, METH_VARARGS, "Open connection."},
     {"kx_py_close_connection", kx_py_close_connection, METH_VARARGS, "Close connection."},
     {"kx_py_execute", kx_py_execute, METH_VARARGS, "Execute query."},
     {NULL, NULL, 0, NULL}
};
 
#if PY_MAJOR_VERSION == 2

PyMODINIT_FUNC
initkdb(void) {
     (void) Py_InitModule3("kdb", PykdbMethods, "Python2/kdb+ interface");
}

#elif PY_MAJOR_VERSION == 3

static struct PyModuleDef kdbmodule = {
     PyModuleDef_HEAD_INIT,
     "kdb",
     "Python3/kdb+ interface",
     -1,
     PykdbMethods
};

PyMODINIT_FUNC
PyInit_kdb(void) {
     PyObject* m;
     m = PyModule_Create(&kdbmodule);
     if(m == NULL)
	  return NULL;
     return m;
}

#else
#error "Unknown Python version"
#endif
