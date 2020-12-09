// this file is included from renderdoc.i, it's not a module in itself

%define STRINGIZE(val) #val %enddef

///////////////////////////////////////////////////////////////////////////////////////////////
// simple typemaps for an object that's converted directly by-value. This is perfect for python
// immutable objects like strings or datetimes

%define SIMPLE_TYPEMAPS_VARIANT(BaseType, SimpleType)
%typemap(in) SimpleType (BaseType temp) {
  tempset($1, &temp);

  int res = ConvertFromPy($input, indirect($1));
  if(!SWIG_IsOK(res))
  {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
  }
}

%typemap(out) SimpleType {
  $result = ConvertToPy(indirect($1));
}
%enddef

%define SIMPLE_TYPEMAPS(SimpleType)

SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType *)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType &)

%enddef

///////////////////////////////////////////////////////////////////////////////////////////////
// typemaps for std::function

%typemap(in) std::function {
  PyObject *func = $input;
  $1 = ConvertFunc<$1_ltype>("$symname", func, exHandle$argnum);
}

%typemap(argout) std::function (StackExceptionHandler exHandle) {
  if(exHandle.data().failFlag) {
    PyErr_Restore(exHandle.data().exObj, exHandle.data().valueObj, exHandle.data().tracebackObj);
    SWIG_fail;
  }
}

%typemap(out) rdcpair {
  $result = ConvertToPy(($1_ltype&)$1);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// inserted code to include C++ conversion header

%{
  #include "renderdoc_replay.h"

  static char convert_error[1024] = {};
  
  #include "Code/pyrenderdoc/pyconversion.h"

  // declare the basic building blocks as stringize types
  DECLARE_STRINGISE_TYPE(int8_t);
  DECLARE_STRINGISE_TYPE(uint8_t);
  DECLARE_STRINGISE_TYPE(int16_t);
  DECLARE_STRINGISE_TYPE(uint16_t);
  DECLARE_STRINGISE_TYPE(int32_t);
  DECLARE_STRINGISE_TYPE(uint32_t);
  DECLARE_STRINGISE_TYPE(int64_t);
  DECLARE_STRINGISE_TYPE(uint64_t);
  DECLARE_STRINGISE_TYPE(float);
  DECLARE_STRINGISE_TYPE(double);
  DECLARE_STRINGISE_TYPE(rdcstr);

%}

%include "ext_refcounts.i"
%include "container_handling.i"
