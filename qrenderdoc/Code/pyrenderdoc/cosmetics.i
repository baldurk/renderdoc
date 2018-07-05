
// add some useful builtin functions for ResourceId
%feature("python:tp_str") ResourceId "resid_str";
%feature("python:tp_repr") ResourceId "resid_str";
%feature("python:nb_int") ResourceId "resid_int";
%feature("python:tp_hash") ResourceId "resid_hash";

%ignore ResourceId::operator <;

%extend ResourceId {

PyObject *__lt__(PyObject *other)
{
  bool result = false;
  void *resptr = NULL;
  ResourceId *id = NULL;

  if(other && other != Py_None)
  {
    int res = SWIG_ConvertPtr(other, &resptr, SWIGTYPE_p_ResourceId, 0);
    if (!SWIG_IsOK(res)) {
      SWIG_exception_fail(SWIG_ArgError(res), "incorrect comparison type");
    }

    id = (ResourceId *)resptr;

    result = (*$self < *id);
  }

  return PyBool_FromLong(result ? 1 : 0);
fail:
  return NULL;
}

} // %extend ResourceId

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

// macro to define safe == and != operators for classes, that don't throw an exception comparing to
// None

%define DEFINE_SAFE_EQUALITY(Class)

%ignore Class::operator ==;
%ignore Class::operator !=;

%extend Class {

PyObject *__eq__(PyObject *other)
{
  bool result = false;
  void *resptr = NULL;
  Class *id = NULL;

  if(other && other != Py_None)
  {
    int res = SWIG_ConvertPtr(other, &resptr, SWIGTYPE_p_##Class, 0);
    if (!SWIG_IsOK(res)) {
      SWIG_exception_fail(SWIG_ArgError(res), "incorrect comparison type");
    }

    id = (Class *)resptr;

    result = (*$self == *id);
  }
  
  return PyBool_FromLong(result ? 1 : 0);
fail:
  return NULL;
}

PyObject *__ne__(PyObject *other)
{
  bool result = true;
  void *resptr = NULL;
  Class *id = NULL;

  if(other && other != Py_None)
  {
    int res = SWIG_ConvertPtr(other, &resptr, SWIGTYPE_p_##Class, 0);
    if (!SWIG_IsOK(res)) {
      SWIG_exception_fail(SWIG_ArgError(res), "incorrect comparison type");
    }

    id = (Class *)resptr;

    result = !(*$self == *id);
  }
  
  return PyBool_FromLong(result ? 1 : 0);
fail:
  return NULL;
}

} // %extend Class

%enddef // %define DEFINE_SAFE_COMPARISONS

DEFINE_SAFE_EQUALITY(DrawcallDescription)
DEFINE_SAFE_EQUALITY(CounterResult)
DEFINE_SAFE_EQUALITY(APIEvent)
DEFINE_SAFE_EQUALITY(Bindpoint)
DEFINE_SAFE_EQUALITY(BufferDescription)
DEFINE_SAFE_EQUALITY(CaptureFileFormat)
DEFINE_SAFE_EQUALITY(ConstantBlock)
DEFINE_SAFE_EQUALITY(DebugMessage)
DEFINE_SAFE_EQUALITY(EnvironmentModification)
DEFINE_SAFE_EQUALITY(EventUsage)
DEFINE_SAFE_EQUALITY(PathEntry)
DEFINE_SAFE_EQUALITY(PixelModification)
DEFINE_SAFE_EQUALITY(ResourceDescription)
DEFINE_SAFE_EQUALITY(ResourceId)
DEFINE_SAFE_EQUALITY(LineColumnInfo)
DEFINE_SAFE_EQUALITY(ShaderCompileFlag)
DEFINE_SAFE_EQUALITY(ShaderConstant)
DEFINE_SAFE_EQUALITY(ShaderDebugState)
DEFINE_SAFE_EQUALITY(ShaderResource)
DEFINE_SAFE_EQUALITY(ShaderSampler)
DEFINE_SAFE_EQUALITY(ShaderSourceFile)
DEFINE_SAFE_EQUALITY(ShaderVariable)
DEFINE_SAFE_EQUALITY(RegisterRange)
DEFINE_SAFE_EQUALITY(LocalVariableMapping)
DEFINE_SAFE_EQUALITY(SigParameter)
DEFINE_SAFE_EQUALITY(TextureDescription)
DEFINE_SAFE_EQUALITY(ShaderEntryPoint)
DEFINE_SAFE_EQUALITY(Viewport)
DEFINE_SAFE_EQUALITY(Scissor)
DEFINE_SAFE_EQUALITY(ColorBlend)
DEFINE_SAFE_EQUALITY(BoundVBuffer)
DEFINE_SAFE_EQUALITY(VertexInputAttribute)
DEFINE_SAFE_EQUALITY(BoundResource)
DEFINE_SAFE_EQUALITY(BoundResourceArray)