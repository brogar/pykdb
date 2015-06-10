#include <common1.h>
#include <dtm.h>
#include <datetime.h>
#include <k.h>

#define scalar(x) (x->t < 0)

static
PyObject* from_list_of_kobjects(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     result = PyList_New(length);
     for(i = 0; i != length; ++i)
	  PyList_SetItem(result, i, from_any_kobject(xK[i]));
     return result;
}

static
PyObject* from_int_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     if(scalar(x)) {
	  result = PyLong_FromLong((long)(x->i));
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       PyList_SetItem(result, i, PyLong_FromLong((long)xI[i]));
	  }
     }
     return result;
}

static 
void time_helper(int value, int* h, int* m, int* s, int* ms) {
     *ms = 1000 * (value%1000);
     *s = (value/1000)%60;
     *m = (value/(60*1000))%60;
     *h = (value/(60*1000*60));
     check_time_args(*h, *m, *s, *ms);
}


static
PyObject* from_time_kobject(K x) {
     PyDateTime_IMPORT;
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     int h, m, s, ms;
     
     if(scalar(x)) {
	  time_helper((int)(x->i), &h, &m, &s, &ms);
	  result = PyTime_FromTime(h, m, s, ms);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       time_helper((int)xI[i], &h, &m, &s, &ms);
	       PyList_SetItem(result, i, PyTime_FromTime(h, m, s, ms));
	  }
     }
     return result;
}

static 
void timestamp_helper(double value, int* year, int* month, int* day, int* hour, int* minute, int* second, int* usecond) {
     *usecond = (int) floor( (value - (int)value) * 1000000 ); 
     *second = (int)value;
     *year = 2000;
     *month = 1; 
     *day = 1; 
     *hour = 0; 
     *minute = 0;
     normalize_datetime(year, month, day, hour, minute, second, usecond);
}

static 
PyObject* from_timestamp_kobject(K x){
     PyDateTime_IMPORT;
     PyObject* result ;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     int year, month, day, hour, minute, second, usecond;

     if(scalar(x)) {
	  double value = x->j / 1e9;
	  timestamp_helper(value, &year, &month, &day, &hour, &minute, &second, &usecond);
	  result = PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, usecond);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0 ; i != length; ++i) {
	       double value = xJ[i] / 1e9;
	       timestamp_helper(value, &year, &month, &day, &hour, &minute, &second, &usecond);
	       PyList_SetItem(result, i, PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, usecond));
	  }
     }
     return result;
}

static 
PyObject* from_month_kobject(K x){
     return from_int_kobject(x);
}

static 
void datetime_helper(double value, int* year, int* month, int* day, int* hour, int* minute, int* second, int* usecond) {
     *usecond = 1000 * (int) ((value - (int)value) * 1000);
     *year = 2000;
     *month = 1;
     *day = 1;
     *hour = 0;
     *minute = 0;
     *second = (int) value;
     normalize_datetime(year, month, day, hour, minute, second, usecond);
}

static 
PyObject* from_datetime_kobject(K x){
     PyDateTime_IMPORT;
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     int year, month, day, hour, minute, second, usecond;

     if(scalar(x)) {
	  double value = (x->f) * 86400 ;
	  datetime_helper(value, &year, &month, &day, &hour, &minute, &second, &usecond);
	  result = PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, usecond);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length ; ++i) {
	       double value = ( kF(x)[i] ) * 86400 ;
	       datetime_helper(value, &year, &month, &day, &hour, &minute, &second, &usecond);
	       PyList_SetItem(result, i, PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, usecond));
	  }
     }
     return result;
}

static
void timespan_helper(double value, int* day, int* second, int* usecond) {
     *usecond = (int)floor( (value - (int)value) * 1000000 );
     *second = (int)value;
     *day = 0;
     normalize_pair(second, usecond, 1000000);
     normalize_pair(day, second, 60*60*24);
}

static 
PyObject* from_timespan_kobject(K x){
     PyDateTime_IMPORT;
     PyObject* result ;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     int /*year, month, */ day, /*hour, minute,*/ second, usecond;

     if(scalar(x)) {
	  double value = (x->j) / 1e9;
	  timespan_helper(value, &day, &second, &usecond);
	  result = PyDelta_FromDSU(day, second, usecond) ;
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       double value = xJ[i] / 1e9;
	       timespan_helper(value, &day, &second, &usecond);
	       PyList_SetItem(result, i, PyDelta_FromDSU(day, second, usecond));
	  }
     }
     return result;
}

static 
PyObject* from_minute_kobject(K x){
     /* 99:10 is a valid minute object whatever that means
	likewise the parser seems to take 10000:10 */
     PyDateTime_IMPORT;
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  int value = (x->i) * 60 ;
	  result = PyDelta_FromDSU(0, value, 0);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i!= length ; ++i) {
	       int value = 60 * (xI[i]);
	       PyList_SetItem(result, i, PyDelta_FromDSU(0, value, 0)) ;
	  }
     }
     return result;
}

static 
PyObject* from_second_kobject(K x){
     PyDateTime_IMPORT;
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  int value = (x->i);
	  result = PyDelta_FromDSU(0, value, 0);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       int value = xI[i];
	       PyList_SetItem(result, i, PyDelta_FromDSU(0, value, 0));
	  }
     }
     return result;
}

static 
PyObject* from_bool_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  if(x->g)
	       result = Py_True;
	  else
	       result = Py_False;
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       if(x->G0[i])
		    PyList_SetItem(result, i, Py_True);
	       else
		    PyList_SetItem(result, i, Py_False);
	  }
     }
     return result;
}

static 
PyObject* from_guid_kobject(K x) {
     Py_RETURN_NONE;
}

static 
PyObject* from_byte_kobject(K x) {
     Py_RETURN_NONE;
}

static 
PyObject* from_short_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  result = PyLong_FromLong((long)x->h);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       PyList_SetItem(result, i, PyLong_FromLong((long) xH[i]));
	  }
     }
     return result;
}

static 
PyObject* from_long_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  result = PyLong_FromDouble((double) (x->j));
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       PyList_SetItem(result, i, PyLong_FromDouble((double)xJ[i]));
	  }
     }
     return result;
}

static 
PyObject* from_double_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  result = PyFloat_FromDouble(x->f);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       PyList_SetItem(result, i, PyFloat_FromDouble(xF[i]));
	  }
     }
     return result;
}

static 
PyObject* from_float_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  result = PyFloat_FromDouble((double)x->e);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       PyList_SetItem(result, i, PyFloat_FromDouble( (double) xE[i]));
	  }
     }
     return result;
}

static 
void date_helper(int value, int* y, int* m, int* d) {
     *d = value + 1; /* add 1 since value is relative */
     *m = 1;
     *y = 2000;
     normalize_date(y, m, d);
}

static 
PyObject* from_date_kobject(K x) {
     /* on the python side we need to add date(2000, 1, 1) to these values */
     PyDateTime_IMPORT;
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     int value, y, m, d;

     if(scalar(x)) {
	  value = (x->i);
	  date_helper(value, &y, &m, &d);
	  result = PyDate_FromDate(y, m, d);
     }
     else {
	  result = PyList_New(length);
	  for(i = 0; i != length; ++i) {
	       value = (int) xI[i] ;
	       date_helper(value, &y, &m, &d);
	       PyList_SetItem(result, i, PyDate_FromDate(y, m, d) );
	  }
     }
     return result;
}

static 
PyObject* from_string_column_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     char buffer[2];
     buffer[1] = '\0';
     result = PyList_New(length);
     for(i = 0; i != length; ++i) {
	  buffer[0] = kC(x)[i];
	  PyList_SetItem(result, i, PyUnicode_FromString(buffer));
     }
     return result;
}

static 
PyObject* from_columns_kobject(K x) {
     PyObject *col, *result;
     Py_ssize_t i, length ;
     int type;
     K c;
     length = (Py_ssize_t)(x->n);
     result = PyList_New(length);
     for(i = 0; i != length; ++i) {
	  c = xK[i];
	  type = abs(c->t);
	  if( type == 10) {
	       col = from_string_column_kobject(c);
	  }
	  else {
	       col = from_any_kobject(c);
	  }
	  PyList_SetItem(result, i, col);
     }
     return result;
}

static 
PyObject* from_string_kobject(K x) {
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);
     char* buffer = NULL;
     if(scalar(x)) {
	  buffer = (char*)malloc(2 * sizeof(char));
	  //char buffer[2];
	  buffer[0] = x->g;
	  buffer[1] = '\0';
	  result = PyUnicode_FromString(buffer);
     }
     else {
	  buffer = (char*)malloc( (length+1) * sizeof(char));
	  // char buffer[length + 1];
	  for(i = 0; i != length; ++i) {
	       buffer[i] = xG[i];
	  }
	  buffer[length] = '\0';
	  result = PyUnicode_FromString(buffer);
     }
	
     if(buffer)
	  free((void*)buffer);
		
     return result;
}

static 
PyObject* from_symbol_kobject(K x){
     PyObject* result;
     Py_ssize_t i, length ;
     length = (Py_ssize_t)(x->n);

     if(scalar(x)) {
	  result = PyUnicode_FromString(xs);
     }
     else {
	  result  = PyList_New(length);
	  for(i=0; i != length; ++i) {
	       PyList_SetItem(result, i, PyUnicode_FromString( xS[i] ));
	  }
     }
     return result;
}

static 
PyObject* from_table_kobject(K x){
     PyObject* result ;
     Py_ssize_t i, length;
     PyObject *keys ;
     PyObject *vals ;
	
     keys = from_any_kobject( kK(x->k)[0] );
     vals = from_columns_kobject( kK(x->k)[1] );
     result = PyDict_New();
     if( PyList_CheckExact(keys)) {
	  length = PyList_Size(keys);
	  for(i = 0; i != length; ++i) {
	       PyDict_SetItem(result, PyList_GetItem(keys, i), PyList_GetItem(vals, i));
	  }
     }
	
     Py_DECREF(keys);
     Py_DECREF(vals);
	
     return result;
}

static 
PyObject* from_dictionary_kobject(K x){
     PyObject *result;
     PyObject* keys;
     PyObject* vals;
     K table;
     Py_ssize_t i, length ;

     if(98 == xx->t && 98 == xy->t) {
	  r1(x);
	  if(table = ktd(x)) {
	       result = from_table_kobject(table);
	       r0(table);
	       return result;
	  }
     }

     result = PyDict_New();
     length = (Py_ssize_t)(xx->n);
     keys = from_any_kobject(xx);
     vals = from_any_kobject(xy);

     if( PyList_CheckExact(keys)) {
	  for(i=0; i!= length; ++i) {
	       PyDict_SetItem(result, PyList_GetItem(keys, i), PyList_GetItem(vals, i)) ;
	  }
     }
     else {
	  PyDict_SetItem(result, keys, vals);
     }
	
     Py_DECREF(keys);
     Py_DECREF(vals);
	
     return result;
}

static 
PyObject* error_broken_kobject(K x){
     PyErr_SetString(PyExc_NotImplementedError, " broken k object");
     return NULL;
}

typedef PyObject* (*conversion_function)(K);

conversion_function kdbplus_types[] = {
     from_list_of_kobjects,
     from_bool_kobject,
     from_guid_kobject,
     error_broken_kobject,
     from_byte_kobject,
     from_short_kobject,
     from_int_kobject,
     from_long_kobject,
     from_float_kobject,
     from_double_kobject,
     from_string_kobject,
     from_symbol_kobject,
     from_timestamp_kobject,
     from_month_kobject,
     from_date_kobject,
     from_datetime_kobject,
     from_timespan_kobject,
     from_minute_kobject,
     from_second_kobject,
     from_time_kobject
};

PyObject* from_any_kobject(K x) {
     PyObject* result;
     int type = abs(x->t);
	
     if (98 == type)
	  result = from_table_kobject(x);
     else if (99 == type)
	  result = from_dictionary_kobject(x);
     else if (105 == type || 101 == type)
	  result = from_int_kobject(ki(0));
     else if (-1 < type && type < 20)
	  result = kdbplus_types[type](x);
     else
	  result = error_broken_kobject(x);

     return result;
}

