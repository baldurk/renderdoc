
// add some useful builtin functions for ResourceId
%feature("python:tp_str") ResourceId "resid_str";
%feature("python:tp_repr") ResourceId "resid_str";
%feature("python:nb_int") ResourceId "resid_int";

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