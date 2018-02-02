/* ------------------------------------------------------------
 *  utility methods for char strings 
 * ------------------------------------------------------------ */
%fragment("SWIG_AsCharPtrAndSize","header",fragment="SWIG_pchar_descriptor") {
SWIGINTERN int
SWIG_AsCharPtrAndSize(PyObject *obj, char** cptr, size_t* psize, int *alloc)
{
%#if PY_VERSION_HEX>=0x03000000
%#if defined(SWIG_PYTHON_STRICT_BYTE_CHAR)
  if (PyBytes_Check(obj))
%#else
  if (PyUnicode_Check(obj))
%#endif
%#else  
  if (PyString_Check(obj))
%#endif
  {
    char *cstr; Py_ssize_t len;
%#if PY_VERSION_HEX>=0x03000000
%#if !defined(SWIG_PYTHON_STRICT_BYTE_CHAR)
    if (!alloc && cptr) {
        /* We can't allow converting without allocation, since the internal
           representation of string in Python 3 is UCS-2/UCS-4 but we require
           a UTF-8 representation.
           TODO(bhy) More detailed explanation */
        return SWIG_RuntimeError;
    }
    obj = PyUnicode_AsUTF8String(obj);
    if(alloc) *alloc = SWIG_NEWOBJ;
%#endif
    PyBytes_AsStringAndSize(obj, &cstr, &len);
%#else
    PyString_AsStringAndSize(obj, &cstr, &len);
%#endif
    if (cptr) {
      if (alloc) {
	/* 
	   In python the user should not be able to modify the inner
	   string representation. To warranty that, if you define
	   SWIG_PYTHON_SAFE_CSTRINGS, a new/copy of the python string
	   buffer is always returned.

	   The default behavior is just to return the pointer value,
	   so, be careful.
	*/ 
%#if defined(SWIG_PYTHON_SAFE_CSTRINGS)
	if (*alloc != SWIG_OLDOBJ) 
%#else
	if (*alloc == SWIG_NEWOBJ) 
%#endif
	{
	  *cptr = %new_copy_array(cstr, len + 1, char);
	  *alloc = SWIG_NEWOBJ;
	} else {
	  *cptr = cstr;
	  *alloc = SWIG_OLDOBJ;
	}
      } else {
%#if PY_VERSION_HEX>=0x03000000
%#if defined(SWIG_PYTHON_STRICT_BYTE_CHAR)
	*cptr = PyBytes_AsString(obj);
%#else
	assert(0); /* Should never reach here with Unicode strings in Python 3 */
%#endif
%#else
	*cptr = SWIG_Python_str_AsChar(obj);
%#endif
      }
    }
    if (psize) *psize = len + 1;
%#if PY_VERSION_HEX>=0x03000000 && !defined(SWIG_PYTHON_STRICT_BYTE_CHAR)
    Py_XDECREF(obj);
%#endif
    return SWIG_OK;
  } else {
%#if defined(SWIG_PYTHON_2_UNICODE)
%#if defined(SWIG_PYTHON_STRICT_BYTE_CHAR)
%#error "Cannot use both SWIG_PYTHON_2_UNICODE and SWIG_PYTHON_STRICT_BYTE_CHAR at once"
%#endif
%#if PY_VERSION_HEX<0x03000000
    if (PyUnicode_Check(obj)) {
      char *cstr; Py_ssize_t len;
      if (!alloc && cptr) {
        return SWIG_RuntimeError;
      }
      obj = PyUnicode_AsUTF8String(obj);
      if (PyString_AsStringAndSize(obj, &cstr, &len) != -1) {
        if (cptr) {
          if (alloc) *alloc = SWIG_NEWOBJ;
          *cptr = %new_copy_array(cstr, len + 1, char);
        }
        if (psize) *psize = len + 1;

        Py_XDECREF(obj);
        return SWIG_OK;
      } else {
        Py_XDECREF(obj);
      }
    }
%#endif
%#endif

    swig_type_info* pchar_descriptor = SWIG_pchar_descriptor();
    if (pchar_descriptor) {
      void* vptr = 0;
      if (SWIG_ConvertPtr(obj, &vptr, pchar_descriptor, 0) == SWIG_OK) {
	if (cptr) *cptr = (char *) vptr;
	if (psize) *psize = vptr ? (strlen((char *)vptr) + 1) : 0;
	if (alloc) *alloc = SWIG_OLDOBJ;
	return SWIG_OK;
      }
    }
  }
  return SWIG_TypeError;
}
}

%fragment("SWIG_FromCharPtrAndSize","header",fragment="SWIG_pchar_descriptor") {
SWIGINTERNINLINE PyObject *
SWIG_FromCharPtrAndSize(const char* carray, size_t size)
{
  if (carray) {
    if (size > INT_MAX) {
      swig_type_info* pchar_descriptor = SWIG_pchar_descriptor();
      return pchar_descriptor ? 
	SWIG_InternalNewPointerObj(%const_cast(carray,char *), pchar_descriptor, 0) : SWIG_Py_Void();
    } else {
%#if PY_VERSION_HEX >= 0x03000000
%#if defined(SWIG_PYTHON_STRICT_BYTE_CHAR)
      return PyBytes_FromStringAndSize(carray, %numeric_cast(size, Py_ssize_t));
%#else
%#if PY_VERSION_HEX >= 0x03010000
      return PyUnicode_DecodeUTF8(carray, %numeric_cast(size, Py_ssize_t), "surrogateescape");
%#else
      return PyUnicode_FromStringAndSize(carray, %numeric_cast(size, Py_ssize_t));
%#endif
%#endif
%#else
      return PyString_FromStringAndSize(carray, %numeric_cast(size, Py_ssize_t));
%#endif
    }
  } else {
    return SWIG_Py_Void();
  }
}
}

