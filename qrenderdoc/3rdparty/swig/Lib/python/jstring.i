%include <typemaps/valtypes.swg>

%fragment(SWIG_AsVal_frag(jstring),"header") {
SWIGINTERN int
SWIG_AsVal(jstring)(PyObject *obj, jstring *val)
{
  if (obj == Py_None) {
    if (val) *val = 0;
    return SWIG_OK;
  }
  
  PyObject *tmp = 0;
  int isunicode = PyUnicode_Check(obj);
  if (!isunicode && PyString_Check(obj)) {
    if (val) {
      obj = tmp = PyUnicode_FromObject(obj);
    }
    isunicode = 1;
  }
  if (isunicode) {
    if (val) {
      if (sizeof(Py_UNICODE) == sizeof(jchar)) {
	*val = JvNewString((const jchar *) PyUnicode_AS_UNICODE(obj),PyUnicode_GET_SIZE(obj));
	return SWIG_NEWOBJ;
      } else {
	int len = PyUnicode_GET_SIZE(obj);
	Py_UNICODE *pchars = PyUnicode_AS_UNICODE(obj);
	*val = JvAllocString (len);
	jchar *jchars = JvGetStringChars (*val);	
	for (int i = 0; i < len; ++i) {
	  jchars[i] = pchars[i];
	}
	return SWIG_NEWOBJ;
      }
    }
    Py_XDECREF(tmp);
    return SWIG_OK;
  }
  return SWIG_TypeError;
}
}

%fragment(SWIG_From_frag(jstring),"header") {
SWIGINTERNINLINE PyObject *
SWIG_From(jstring)(jstring val)
{
  if (!val) {
    return SWIG_Py_Void();
  } 
  if (sizeof(Py_UNICODE) == sizeof(jchar)) {    
    return PyUnicode_FromUnicode((const Py_UNICODE *) JvGetStringChars(val),
				 JvGetStringUTFLength(val));
  } else {
    int len = JvGetStringUTFLength(val);
    Py_UNICODE pchars[len];
    jchar *jchars = JvGetStringChars(val);
    
    for (int i = 0; i < len; i++) {      
      pchars[i] = jchars[i];
    }
    return PyUnicode_FromUnicode((const Py_UNICODE *) pchars, len);
  }
}
}

%typemaps_asvalfrom(%checkcode(STRING),
		    %arg(SWIG_AsVal(jstring)), 
		    %arg(SWIG_From(jstring)), 
		    %arg(SWIG_AsVal_frag(jstring)), 
		    %arg(SWIG_From_frag(jstring)), 
		    java::lang::String *);

