// this file is included from renderdoc.i, it's not a module in itself

%define STRINGIZE(val) #val %enddef

///////////////////////////////////////////////////////////////////////////////////////////////
// typemaps for more sensible fixed-array handling, based on typemaps from SWIG documentation

%define FIXED_ARRAY_TYPEMAPS(BaseType)

%typemap(out) BaseType [ANY] {
  $result = PyList_New($1_dim0);
  for(int i = 0; i < $1_dim0; i++)
  {
    PyObject *o = TypeConversion<BaseType>::ConvertToPy( $1[i]);
    if(!o)
    {
      snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' returning type '$1_basetype', encoding element %d", i);
      SWIG_exception_fail(SWIG_ValueError, convert_error);
    }
    PyList_SetItem($result,i,o);
  }
}

%typemap(arginit) BaseType [ANY] {
   $1 = NULL;
}

%typemap(in) BaseType [ANY] {
  if(!PySequence_Check($input))
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' argument $argnum of type '$1_basetype'. Expected sequence"); 
  }
  if(PySequence_Length($input) != $1_dim0) {
    SWIG_exception_fail(SWIG_ValueError, "in method '$symname' argument $argnum of type '$1_basetype'. Expected $1_dim0 elements"); 
  }
  $1 = new BaseType[$1_dim0];
  for(int i = 0; i < $1_dim0; i++) {
    PyObject *o = PySequence_GetItem($input,i);

    int res = TypeConversion<BaseType>::ConvertFromPy(o, $1[i]);

    if(!SWIG_IsOK(res))
    {
      snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", i);
      SWIG_exception_fail(SWIG_ArgError(res), convert_error);
    }
  }
}

%typemap(freearg) BaseType [ANY] {
   delete[] $1;
}

%enddef

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

%typemap(argout) std::function (ExceptionHandling exHandle) {
  if(exHandle.failFlag) {
    PyErr_Restore(exHandle.exObj, exHandle.valueObj, exHandle.tracebackObj);
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
  DECLARE_STRINGISE_TYPE(rdcstrpair);

%}

%include "ext_refcounts.i"
%include "container_handling.i"
