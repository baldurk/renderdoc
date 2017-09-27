///////////////////////////////////////////////////////////////////////////////////////////////
// handling of 'externally refcounted' C++ types. The idea here is we have a C++ type that's
// passed around by pointer, so we can potentially keep it wrapped as a pointer and access it
// from python by reference instead of converting to/from python and getting by-value semantics.
//
// We make a few assumptions about lifetime:
//  - Python allocated instances of these types are only ever borrowed by C++. ie. we might pass
//    them to C++ but we won't transfer ownership. Thus, the refcount can be tracked in python
//    entirely and be sure that we won't have dangling references on the C++ side.
//    (The only sensible way to ensure this is to make any C++ calls only use the pointer until
//     they return, either by only using it temporarily or making local duplicates).
//  - The reverse holds true - any C++ objects returned to python may be modified or passed around,
//    but will not be deleted until python is done with them.
//    (This kind of depends on the user's scripts to enforce that they keep the C++ object alive
//     until they're done with the object).
//  - Any lists of references are only ever modified from one side or another. This is a bit more
//    opaque but basically we require this so that if we have a list of pointers in python and we
//    take a ref on the python-owned object, that the C++ side doesn't then silently remove the
//    object behind our back and leak a reference.
//
// The general summary is - each side (C++/Python) should own objects allocated on its side, and
// treat any pointers/containers of pointers from the other side as somewhat temporary and read-only.

%define REFCOUNTED_TYPE(typeName)

// refcounted types have custom init/dealloc to update the external refcount
%feature("python:tp_init") typeName STRINGIZE(typeName##_init);
%feature("python:tp_dealloc") typeName STRINGIZE(typeName##_dealloc_destructor_closure);

%wrapper %{
// mostly identical to generated version, but registers with ExtRefcount
static int typeName##_init(PyObject *self, PyObject *args)
{
  typeName *result = MakeFromArgsTuple<typeName>(args);

  if(!result)
    return -1;

  PyObject *resultobj = SWIG_NewPointerObj((void *)result, SWIGTYPE_p_##typeName, SWIG_BUILTIN_INIT);

  if(resultobj == Py_None)
  {
    delete result;
    return -1;
  }

  ExtRefcount<typeName *>::NewPyObject(resultobj, result);

  return 0;
}

static PyObject *typeName##_dealloc(PyObject *self, PyObject *args)
{
  typeName *thisptr = NULL;

  int res = SWIG_ConvertPtr(self, (void **)&thisptr, SWIGTYPE_p_##typeName, SWIG_POINTER_DISOWN);
  if(!SWIG_IsOK(res))
    SWIG_exception_fail(SWIG_ArgError(res), "Invalid " #typeName " being deleted");

  ExtRefcount<typeName *>::DelPyObject(self, thisptr);
  delete thisptr;

  return SWIG_Py_Void();
fail:
  return NULL;
}

SWIGPY_DESTRUCTOR_CLOSURE(typeName##_dealloc)

%}

%enddef // %define REFCOUNTED_TYPE

// add a typemap for a refcounted array
%define DEFINE_REFCOUNTED_ARRAY(typeName)

%typemap(memberin) typeName {
  // remove a reference on all the old items
  for(size_t i = 0; i < $1.size(); i++)
    ExtRefcount<typeName::value_type>::Dec($1[i]);

  $1.clear();

  // copy the values
  $1.assign(*$input);

  // the input values were already ref'd as needed during the 'in' typemap to convert them in the
  // first place, so we just steal those references
}

%enddef // %define DEFINE_REFCOUNTED_ARRAY
