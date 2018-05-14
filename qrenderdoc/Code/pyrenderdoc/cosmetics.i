
// add some useful builtin functions for ResourceId
%feature("python:tp_str") ResourceId "resid_str";
%feature("python:tp_repr") ResourceId "resid_str";
%feature("python:nb_int") ResourceId "resid_int";
%feature("python:tp_hash") ResourceId "resid_hash";

%wrapper %{
static PyObject *resid_str(PyObject *resid)
{
  void *resptr = NULL;
  unsigned long long *id = NULL;
  int res = SWIG_ConvertPtr(resid, &resptr, SWIGTYPE_p_ResourceId, 0);
  if (!SWIG_IsOK(res)) {
    SWIG_exception_fail(SWIG_ArgError(res), "in method 'ResourceId.str', ResourceId is not correct type");
  }

  // cast as unsigned long long
  id = (unsigned long long *)resptr;
  static_assert(sizeof(unsigned long long) == sizeof(ResourceId), "Wrong size");

  return PyUnicode_FromFormat("<ResourceId %llu>", *id);
fail:
  return NULL;
}

static Py_hash_t resid_hash(PyObject *resid)
{
  void *resptr = NULL;
  unsigned long long *id = NULL;
  int res = SWIG_ConvertPtr(resid, &resptr, SWIGTYPE_p_ResourceId, 0);
  if (!SWIG_IsOK(res)) {
    SWIG_exception_fail(SWIG_ArgError(res), "in method 'ResourceId.str', ResourceId is not correct type");
  }

  // cast as unsigned long long
  id = (unsigned long long *)resptr;
  static_assert(sizeof(unsigned long long) == sizeof(ResourceId), "Wrong size");

  return Py_hash_t(*id);
fail:
  return 0;
}

static PyObject *resid_int(PyObject *resid)
{
  void *resptr = NULL;
  unsigned long long *id = NULL;
  int res = SWIG_ConvertPtr(resid, &resptr, SWIGTYPE_p_ResourceId, 0);
  if (!SWIG_IsOK(res)) {
    SWIG_exception_fail(SWIG_ArgError(res), "in method 'ResourceId.str', ResourceId is not correct type");
  }

  // cast as unsigned long long
  id = (unsigned long long *)resptr;
  static_assert(sizeof(unsigned long long) == sizeof(ResourceId), "Wrong size");

  return PyLong_FromUnsignedLongLong(*id);
fail:
  return NULL;
}
%}