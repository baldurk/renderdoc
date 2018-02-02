/* ------------------------------------------------------------
 * Primitive Types
 * ------------------------------------------------------------ */

/* boolean */

%fragment(SWIG_From_frag(bool),"header") {
SWIGINTERNINLINE PyObject*
  SWIG_From_dec(bool)(bool value)
{
  return PyBool_FromLong(value ? 1 : 0);
}
}

#ifdef SWIG_PYTHON_LEGACY_BOOL
// Default prior to SWIG 3.0.0
%fragment(SWIG_AsVal_frag(bool),"header",
	  fragment=SWIG_AsVal_frag(long)) {
SWIGINTERN int
SWIG_AsVal_dec(bool)(PyObject *obj, bool *val)
{
  int r = PyObject_IsTrue(obj);
  if (r == -1)
    return SWIG_ERROR;
  if (val) *val = r ? true : false;
  return SWIG_OK;
}
}
#else
%fragment(SWIG_AsVal_frag(bool),"header",
	  fragment=SWIG_AsVal_frag(long)) {
SWIGINTERN int
SWIG_AsVal_dec(bool)(PyObject *obj, bool *val)
{
  int r;
  if (!PyBool_Check(obj))
    return SWIG_ERROR;
  r = PyObject_IsTrue(obj);
  if (r == -1)
    return SWIG_ERROR;
  if (val) *val = r ? true : false;
  return SWIG_OK;
}
}
#endif

/* int */

%fragment(SWIG_From_frag(int),"header") {
SWIGINTERNINLINE PyObject*
  SWIG_From_dec(int)(int value)
{
  return PyInt_FromLong((long) value);
}
}

/* unsigned int */

%fragment(SWIG_From_frag(unsigned int),"header") {
SWIGINTERNINLINE PyObject*
  SWIG_From_dec(unsigned int)(unsigned int value)
{
  return PyInt_FromSize_t((size_t) value);
}
}

/* long */

%fragment(SWIG_From_frag(long),"header") {
  %define_as(SWIG_From_dec(long),           PyInt_FromLong)
}

%fragment(SWIG_AsVal_frag(long),"header",
	  fragment="SWIG_CanCastAsInteger") {
SWIGINTERN int
SWIG_AsVal_dec(long)(PyObject *obj, long* val)
{
%#if PY_VERSION_HEX < 0x03000000
  if (PyInt_Check(obj)) {
    if (val) *val = PyInt_AsLong(obj);
    return SWIG_OK;
  } else
%#endif
  if (PyLong_Check(obj)) {
    long v = PyLong_AsLong(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_OK;
    } else {
      PyErr_Clear();
      return SWIG_OverflowError;
    }
  }
%#ifdef SWIG_PYTHON_CAST_MODE
  {
    int dispatch = 0;
    long v = PyInt_AsLong(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_AddCast(SWIG_OK);
    } else {
      PyErr_Clear();
    }
    if (!dispatch) {
      double d;
      int res = SWIG_AddCast(SWIG_AsVal(double)(obj,&d));
      if (SWIG_IsOK(res) && SWIG_CanCastAsInteger(&d, LONG_MIN, LONG_MAX)) {
	if (val) *val = (long)(d);
	return res;
      }
    }
  }
%#endif
  return SWIG_TypeError;
}
}

/* unsigned long */

%fragment(SWIG_From_frag(unsigned long),"header",
	  fragment=SWIG_From_frag(long)) {
SWIGINTERNINLINE PyObject* 
SWIG_From_dec(unsigned long)(unsigned long value)
{
  return (value > LONG_MAX) ?
    PyLong_FromUnsignedLong(value) : PyInt_FromLong(%numeric_cast(value,long));
}
}

%fragment(SWIG_AsVal_frag(unsigned long),"header",
	  fragment="SWIG_CanCastAsInteger") {
SWIGINTERN int
SWIG_AsVal_dec(unsigned long)(PyObject *obj, unsigned long *val) 
{
%#if PY_VERSION_HEX < 0x03000000
  if (PyInt_Check(obj)) {
    long v = PyInt_AsLong(obj);
    if (v >= 0) {
      if (val) *val = v;
      return SWIG_OK;
    } else {
      return SWIG_OverflowError;
    }
  } else
%#endif
  if (PyLong_Check(obj)) {
    unsigned long v = PyLong_AsUnsignedLong(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_OK;
    } else {
      PyErr_Clear();
      return SWIG_OverflowError;
    }
  }
%#ifdef SWIG_PYTHON_CAST_MODE
  {
    int dispatch = 0;
    unsigned long v = PyLong_AsUnsignedLong(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_AddCast(SWIG_OK);
    } else {
      PyErr_Clear();
    }
    if (!dispatch) {
      double d;
      int res = SWIG_AddCast(SWIG_AsVal(double)(obj,&d));
      if (SWIG_IsOK(res) && SWIG_CanCastAsInteger(&d, 0, ULONG_MAX)) {
	if (val) *val = (unsigned long)(d);
	return res;
      }
    }
  }
%#endif
  return SWIG_TypeError;
}
}

/* long long */

%fragment(SWIG_From_frag(long long),"header",
	  fragment="SWIG_LongLongAvailable") {
%#ifdef SWIG_LONG_LONG_AVAILABLE
SWIGINTERNINLINE PyObject* 
SWIG_From_dec(long long)(long long value)
{
  return ((value < LONG_MIN) || (value > LONG_MAX)) ?
    PyLong_FromLongLong(value) : PyInt_FromLong(%numeric_cast(value,long));
}
%#endif
}

%fragment(SWIG_AsVal_frag(long long),"header",
	  fragment=SWIG_AsVal_frag(long),
	  fragment="SWIG_CanCastAsInteger",
	  fragment="SWIG_LongLongAvailable") {
%#ifdef SWIG_LONG_LONG_AVAILABLE
SWIGINTERN int
SWIG_AsVal_dec(long long)(PyObject *obj, long long *val)
{
  int res = SWIG_TypeError;
  if (PyLong_Check(obj)) {
    long long v = PyLong_AsLongLong(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_OK;
    } else {
      PyErr_Clear();
      res = SWIG_OverflowError;
    }
  } else {
    long v;
    res = SWIG_AsVal(long)(obj,&v);
    if (SWIG_IsOK(res)) {
      if (val) *val = v;
      return res;
    }
  }
%#ifdef SWIG_PYTHON_CAST_MODE
  {
    const double mant_max = 1LL << DBL_MANT_DIG;
    const double mant_min = -mant_max;
    double d;
    res = SWIG_AsVal(double)(obj,&d);
    if (SWIG_IsOK(res) && !SWIG_CanCastAsInteger(&d, mant_min, mant_max))
      return SWIG_OverflowError;
    if (SWIG_IsOK(res) && SWIG_CanCastAsInteger(&d, mant_min, mant_max)) {
      if (val) *val = (long long)(d);
      return SWIG_AddCast(res);
    }
    res = SWIG_TypeError;
  }
%#endif
  return res;
}
%#endif
}

/* unsigned long long */

%fragment(SWIG_From_frag(unsigned long long),"header",
	  fragment="SWIG_LongLongAvailable") {
%#ifdef SWIG_LONG_LONG_AVAILABLE
SWIGINTERNINLINE PyObject* 
SWIG_From_dec(unsigned long long)(unsigned long long value)
{
  return (value > LONG_MAX) ?
    PyLong_FromUnsignedLongLong(value) : PyInt_FromLong(%numeric_cast(value,long));
}
%#endif
}

%fragment(SWIG_AsVal_frag(unsigned long long),"header",
	  fragment=SWIG_AsVal_frag(unsigned long),
	  fragment="SWIG_CanCastAsInteger",
	  fragment="SWIG_LongLongAvailable") {
%#ifdef SWIG_LONG_LONG_AVAILABLE
SWIGINTERN int
SWIG_AsVal_dec(unsigned long long)(PyObject *obj, unsigned long long *val)
{
  int res = SWIG_TypeError;
  if (PyLong_Check(obj)) {
    unsigned long long v = PyLong_AsUnsignedLongLong(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_OK;
    } else {
      PyErr_Clear();
      res = SWIG_OverflowError;
    }
  } else {
    unsigned long v;
    res = SWIG_AsVal(unsigned long)(obj,&v);
    if (SWIG_IsOK(res)) {
      if (val) *val = v;
      return res;
    }
  }
%#ifdef SWIG_PYTHON_CAST_MODE
  {
    const double mant_max = 1LL << DBL_MANT_DIG;
    double d;
    res = SWIG_AsVal(double)(obj,&d);
    if (SWIG_IsOK(res) && !SWIG_CanCastAsInteger(&d, 0, mant_max))
      return SWIG_OverflowError;
    if (SWIG_IsOK(res) && SWIG_CanCastAsInteger(&d, 0, mant_max)) {
      if (val) *val = (unsigned long long)(d);
      return SWIG_AddCast(res);
    }
    res = SWIG_TypeError;
  }
%#endif
  return res;
}
%#endif
}

/* double */

%fragment(SWIG_From_frag(double),"header") {
  %define_as(SWIG_From_dec(double),          PyFloat_FromDouble)
}

%fragment(SWIG_AsVal_frag(double),"header") {
SWIGINTERN int
SWIG_AsVal_dec(double)(PyObject *obj, double *val)
{
  int res = SWIG_TypeError;
  if (PyFloat_Check(obj)) {
    if (val) *val = PyFloat_AsDouble(obj);
    return SWIG_OK;
%#if PY_VERSION_HEX < 0x03000000
  } else if (PyInt_Check(obj)) {
    if (val) *val = (double) PyInt_AsLong(obj);
    return SWIG_OK;
%#endif
  } else if (PyLong_Check(obj)) {
    double v = PyLong_AsDouble(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = v;
      return SWIG_OK;
    } else {
      PyErr_Clear();
    }
  }
%#ifdef SWIG_PYTHON_CAST_MODE
  {
    int dispatch = 0;
    double d = PyFloat_AsDouble(obj);
    if (!PyErr_Occurred()) {
      if (val) *val = d;
      return SWIG_AddCast(SWIG_OK);
    } else {
      PyErr_Clear();
    }
    if (!dispatch) {
      long v = PyLong_AsLong(obj);
      if (!PyErr_Occurred()) {
	if (val) *val = v;
	return SWIG_AddCast(SWIG_AddCast(SWIG_OK));
      } else {
	PyErr_Clear();
      }
    }
  }
%#endif
  return res;
}
}



