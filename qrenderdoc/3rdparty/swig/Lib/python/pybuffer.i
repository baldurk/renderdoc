/* Implementing buffer protocol typemaps */

/* %pybuffer_mutable_binary(TYPEMAP, SIZE)
 *
 * Macro for functions accept mutable buffer pointer with a size.
 * This can be used for both input and output. For example:
 * 
 *      %pybuffer_mutable_binary(char *buff, int size);
 *      void foo(char *buff, int size) {
 *        for(int i=0; i<size; ++i)
 *          buff[i]++;  
 *      }
 */

%define %pybuffer_mutable_binary(TYPEMAP, SIZE)
%typemap(in) (TYPEMAP, SIZE)
  (int res, Py_ssize_t size = 0, void *buf = 0) {
  res = PyObject_AsWriteBuffer($input, &buf, &size);
  if (res<0) {
    PyErr_Clear();
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  $1 = ($1_ltype) buf;
  $2 = ($2_ltype) (size/sizeof($*1_type));
}
%enddef

/* %pybuffer_mutable_string(TYPEMAP, SIZE)
 *
 * Macro for functions accept mutable zero terminated string pointer.
 * This can be used for both input and output. For example:
 * 
 *      %pybuffer_mutable_string(char *str);
 *      void foo(char *str) {
 *        while(*str) {
 *          *str = toupper(*str);
 *          str++;
 *      }
 */

%define %pybuffer_mutable_string(TYPEMAP)
%typemap(in) (TYPEMAP)
  (int res, Py_ssize_t size = 0, void *buf = 0) {
  res = PyObject_AsWriteBuffer($input, &buf, &size);
  if (res<0) {
    PyErr_Clear();
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  $1 = ($1_ltype) buf;
}
%enddef

/* pybuffer_binary(TYPEMAP, SIZE)
 *
 * Macro for functions accept read only buffer pointer with a size.
 * This must be used for input. For example:
 * 
 *      %pybuffer_binary(char *buff, int size);
 *      int foo(char *buff, int size) {
 *        int count = 0;
 *        for(int i=0; i<size; ++i)
 *          if (0==buff[i]) count++;
 *        return count;
 *      }
 */

%define %pybuffer_binary(TYPEMAP, SIZE)
%typemap(in) (TYPEMAP, SIZE)
  (int res, Py_ssize_t size = 0, const void *buf = 0) {
  res = PyObject_AsReadBuffer($input, &buf, &size);
  if (res<0) {
    PyErr_Clear();
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  $1 = ($1_ltype) buf;
  $2 = ($2_ltype) (size / sizeof($*1_type));
}
%enddef

/* %pybuffer_string(TYPEMAP, SIZE)
 *
 * Macro for functions accept read only zero terminated string pointer.
 * This can be used for input. For example:
 * 
 *      %pybuffer_string(char *str);
 *      int foo(char *str) {
 *        int count = 0;
 *        while(*str) {
 *          if (isalnum(*str))
 *            count++;
 *          str++;
 *      }
 */

%define %pybuffer_string(TYPEMAP)
%typemap(in) (TYPEMAP)
  (int res, Py_ssize_t size = 0, const void *buf = 0) {
  res = PyObject_AsReadBuffer($input, &buf, &size);
  if (res<0) {
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  $1 = ($1_ltype) buf;
}
%enddef



