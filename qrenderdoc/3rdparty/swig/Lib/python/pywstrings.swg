/* ------------------------------------------------------------
 *  utility methods for wchar_t strings 
 * ------------------------------------------------------------ */

%{
#if PY_VERSION_HEX >= 0x03020000
# define SWIGPY_UNICODE_ARG(obj) ((PyObject*) (obj))
#else
# define SWIGPY_UNICODE_ARG(obj) ((PyUnicodeObject*) (obj))
#endif
%}

%fragment("SWIG_AsWCharPtrAndSize","header",fragment="<wchar.h>",fragment="SWIG_pwchar_descriptor") {
SWIGINTERN int
SWIG_AsWCharPtrAndSize(PyObject *obj, wchar_t **cptr, size_t *psize, int *alloc)
{
  PyObject *tmp = 0;
  int isunicode = PyUnicode_Check(obj);
%#if PY_VERSION_HEX < 0x03000000 && !defined(SWIG_PYTHON_STRICT_UNICODE_WCHAR)
  if (!isunicode && PyString_Check(obj)) {
    obj = tmp = PyUnicode_FromObject(obj);
    isunicode = 1;
  }
%#endif
  if (isunicode) {
    Py_ssize_t len = PyUnicode_GetSize(obj);
    if (cptr) {
      *cptr = %new_array(len + 1, wchar_t);
      PyUnicode_AsWideChar(SWIGPY_UNICODE_ARG(obj), *cptr, len);
      (*cptr)[len] = 0;
    }
    if (psize) *psize = (size_t) len + 1;
    if (alloc) *alloc = cptr ? SWIG_NEWOBJ : 0;
    Py_XDECREF(tmp);
    return SWIG_OK;
  } else {
    swig_type_info* pwchar_descriptor = SWIG_pwchar_descriptor();
    if (pwchar_descriptor) {
      void * vptr = 0;
      if (SWIG_ConvertPtr(obj, &vptr, pwchar_descriptor, 0) == SWIG_OK) {
	if (cptr) *cptr = (wchar_t *)vptr;
	if (psize) *psize = vptr ? (wcslen((wchar_t *)vptr) + 1) : 0;
	return SWIG_OK;
      }
    }
  }
  return SWIG_TypeError;
}
}

%fragment("SWIG_FromWCharPtrAndSize","header",fragment="<wchar.h>",fragment="SWIG_pwchar_descriptor") {
SWIGINTERNINLINE PyObject *
SWIG_FromWCharPtrAndSize(const wchar_t * carray, size_t size)
{
  if (carray) {
    if (size > INT_MAX) {
      swig_type_info* pwchar_descriptor = SWIG_pwchar_descriptor();
      return pwchar_descriptor ? 
	SWIG_InternalNewPointerObj(%const_cast(carray,wchar_t *), pwchar_descriptor, 0) : SWIG_Py_Void();
    } else {
      return PyUnicode_FromWideChar(carray, %numeric_cast(size, Py_ssize_t));
    }
  } else {
    return SWIG_Py_Void();
  }
}
}


