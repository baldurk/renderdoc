// this file is included from renderdoc.i, it's not a module in itself

%define SIMPLE_TYPEMAPS_VARIANT(BaseType, SimpleType)
%typemap(in, fragment="pyconvert") SimpleType (BaseType temp) {
  tempset($1, &temp);

  int res = ConvertFromPy($input, indirect($1));
  if(!SWIG_IsOK(res))
  {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
  }
}

%typemap(out, fragment="pyconvert") SimpleType {
  $result = ConvertToPy(self, indirect($1));
}
%enddef

%define SIMPLE_TYPEMAPS(SimpleType)

SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType *)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType &)

%enddef

%define CONTAINER_TYPEMAPS_VARIANT(ContainerType)

%typemap(in, fragment="pyconvert") ContainerType (unsigned char tempmem[32]) {
  static_assert(sizeof(tempmem) >= sizeof(std::remove_pointer<decltype($1)>::type), "not enough temp space for $1_basetype");
  
  if(!PyList_Check($input))
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' list expected for argument $argnum of type '$1_basetype'"); 
  }

  tempalloc($1, tempmem);

  int failIdx = 0;
  int res = TypeConversion<std::remove_pointer<decltype($1)>::type>::ConvertFromPy($input, indirect($1), &failIdx);

  if(!SWIG_IsOK(res))
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ArgError(res), convert_error); 
  }
}

%typemap(freearg, fragment="pyconvert") ContainerType {
  tempdealloc($1);
}

%typemap(argout, fragment="pyconvert") ContainerType {
  // empty the previous contents
  if(PyDict_Check($input))
  {
    PyDict_Clear($input);
  }
  else
  {
    Py_ssize_t sz = PySequence_Size($input);
    if(sz > 0)
      PySequence_DelSlice($input, 0, sz);
  }

  // overwrite with array contents
  int failIdx = 0;
  PyObject *res = TypeConversion<std::remove_pointer<decltype($1)>::type>::ConvertToPyInPlace(self, $input, indirect($1), &failIdx);

  if(!res)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error); 
  }
}

%typemap(out, fragment="pyconvert") ContainerType {
  int failIdx = 0;
  $result = TypeConversion<std::remove_pointer<$1_basetype>::type>::ConvertToPy(self, indirect($1), &failIdx);
  if(!$result)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' returning type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error);
  }
}

%enddef

%define CONTAINER_TYPEMAPS(ContainerType)

CONTAINER_TYPEMAPS_VARIANT(ContainerType)
CONTAINER_TYPEMAPS_VARIANT(ContainerType *)
CONTAINER_TYPEMAPS_VARIANT(ContainerType &)

%enddef