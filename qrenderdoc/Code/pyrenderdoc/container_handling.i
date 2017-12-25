///////////////////////////////////////////////////////////////////////////////////////////////
// handling for making C++ types act as python lists/tuples, to allow a limited amount of
// by-reference access from python
//
// We do this mostly by adding named methods and slots that modify or access the list in place.

%header %{
// hacky macro to check if two variables are the same by looking at their length & second letter.
// Used to identify if we're dealing with 'self' in a typemap or not
#define STR_EQ(a, b) (sizeof(#a) == sizeof(#b) && (#a[1] == #b[1]))
%}

// a typemap that allows us to modify 'self' in place, but convert any inputs by value.
%define LIST_MODIFY_IN_PLACE_TYPEMAP(ContainerType)

%typemap(in) const ContainerType & (unsigned char tempmem[32], bool wasSelf = false) {
  using array_type = std::remove_pointer<decltype($1)>::type;

  {
    // convert the sequence by value using ConvertFromPy
    static_assert(sizeof(tempmem) >= sizeof(array_type), "not enough temp space for $1_basetype");

    tempalloc($1, tempmem);

    int failIdx = 0;
    int res = TypeConversion<array_type>::ConvertFromPy($input, indirect($1), &failIdx);

    if(!SWIG_IsOK(res))
    {
      if(res == SWIG_TypeError)
      {
        SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
      }
      else
      {
        snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", failIdx);
        SWIG_exception_fail(SWIG_ArgError(res), convert_error);
      }
    }
  }
}

%typemap(in) ContainerType * (unsigned char tempmem[32], bool wasSelf = false) {
  using array_type = std::remove_pointer<decltype($1)>::type;

  // don't convert 'self', leave as-is so we can modify by reference. Everything else needs to be
  // converted/copied to allow passing lists (or any sequence) python objects to a function
  // expecting a particular C++ array type
  constexpr bool isSelf = STR_EQ($input, self);
  wasSelf = isSelf;
  if(isSelf)
  {
    // we use an indirect dispatch class here (see self_dispatch) to avoid the need to instantiate
    // the conversion template for types we don't care about
    $1 = self_dispatch<isSelf>::getthis<array_type>(self);
  }
  else
  {
    // convert the sequence by value using ConvertFromPy
    static_assert(sizeof(tempmem) >= sizeof(array_type), "not enough temp space for $1_basetype");

    tempalloc($1, tempmem);

    int failIdx = 0;
    int res = TypeConversion<array_type>::ConvertFromPy($input, indirect($1), &failIdx);

    if(!SWIG_IsOK(res))
    {
      if(res == SWIG_TypeError)
      {
        SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
      }
      else
      {
        snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", failIdx);
        SWIG_exception_fail(SWIG_ArgError(res), convert_error);
      }
    }
  }
}

%typemap(freearg) ContainerType * {
  // if we converted the sequence by-value, then destroy it again
  if(!wasSelf$argnum)
    tempdealloc($1);
}

%enddef // %define LIST_MODIFY_IN_PLACE_TYPEMAP

// this macro defines the list array class named methods, forwarding to templated implementations
%define EXTEND_ARRAY_CLASS_METHODS(Container)
  %extend Container {
    // functions with optional parameters need to use kwargs otherwise SWIG wraps them in multiple
    // overloads
    %feature("kwargs") pop;
    %feature("kwargs") sort;
    %feature("kwargs") index;

    PyObject *append(PyObject *value)
    {
      return array_append($self, value);
    }

    PyObject *clear()
    {
      return array_clear($self);
    }

    PyObject *insert(PyObject *index, PyObject *value)
    {
      return array_insert($self, index, value);
    }

    PyObject *pop(PyObject *index = NULL)
    {
      return array_pop($self, index);
    }

    PyObject *sort(PyObject *key = NULL, bool reverse = false)
    {
      return array_sort($self, key, reverse);
    }

    PyObject *copy()
    {
      return array_copy($self);
    }

    PyObject *reverse()
    {
      return array_reverse($self);
    }

    PyObject *index(PyObject *item, PyObject *start = NULL, PyObject *end = NULL)
    {
      return array_indexOf($self, item, start, end);
    }

    PyObject *count(PyObject *item)
    {
      return array_countOf($self, item);
    }

    PyObject *extend(PyObject *items)
    {
      return array_selfconcat($self, items);
    }

    PyObject *remove(PyObject *item)
    {
      return array_removeOne($self, item);
    }
  } // %extend Container
%enddef // define EXTEND_ARRAY_CLASS_METHODS(Container)

// Declare the slots that we're going to add - see ARRAY_DEFINE_SLOTS for the definitions
%define ARRAY_ADD_SLOTS(array_type, unique_name)

// define slots
%feature("python:tp_repr") array_type STRINGIZE(repr_##unique_name);
%feature("python:tp_str") array_type STRINGIZE(repr_##unique_name);
%feature("python:sq_item") array_type STRINGIZE(getitem_##unique_name);
%feature("python:sq_ass_item") array_type STRINGIZE(setitem_##unique_name);
%feature("python:sq_length") array_type STRINGIZE(length_##unique_name);
%feature("python:sq_concat") array_type STRINGIZE(concat_##unique_name);
%feature("python:sq_repeat") array_type STRINGIZE(repeat_##unique_name);
%feature("python:sq_inplace_concat") array_type STRINGIZE(selfconcat_##unique_name);
%feature("python:sq_inplace_repeat") array_type STRINGIZE(selfrepeat_##unique_name);
%feature("python:mp_subscript") array_type STRINGIZE(getsubscript_##unique_name);
%feature("python:mp_ass_subscript") array_type STRINGIZE(setsubscript_##unique_name);

// https://docs.python.org/3/library/collections.abc.html
// https://docs.python.org/3/library/stdtypes.html#typesseq-common
// https://docs.python.org/3/library/stdtypes.html#typesseq-mutable
// https://docs.python.org/3/library/stdtypes.html#list

%enddef

// define C exported wrappers to bind to the slots that forward to templated implementations
%define ARRAY_DEFINE_SLOTS(array_type, unique_name)

// define C-exported wrappers that forward to templated implementations
%wrapper %{
PyObject *repr_##unique_name(PyObject *self)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  return array_repr(thisptr);
}

PyObject *getitem_##unique_name(PyObject *self, Py_ssize_t idx)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  return array_getitem(thisptr, idx);
}

int setitem_##unique_name(PyObject *self, Py_ssize_t idx, PyObject *val)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return -1;

  return array_setitem(thisptr, idx, val);
}

Py_ssize_t length_##unique_name(PyObject *self, Py_ssize_t idx, PyObject *val)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return -1;

  return array_len(thisptr);
}

PyObject *getsubscript_##unique_name(PyObject *self, PyObject *idx)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  return array_getsubscript(thisptr, idx);
}

int setsubscript_##unique_name(PyObject *self, PyObject *idx, PyObject *val)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return -1;

  return array_setsubscript(thisptr, idx, val);
}

PyObject *concat_##unique_name(PyObject *self, PyObject *vals)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  return array_concat(thisptr, vals);
}

PyObject *repeat_##unique_name(PyObject *self, Py_ssize_t count)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  return array_repeat(thisptr, count);
}

PyObject *selfconcat_##unique_name(PyObject *self, PyObject *vals)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  PyObject *ret = array_selfconcat(thisptr, vals);

  if(ret)
  {
    Py_DECREF(ret);
    Py_INCREF(self);
    return self;
  }

  return NULL;
}

PyObject *selfrepeat_##unique_name(PyObject *self, Py_ssize_t count)
{
  array_type *thisptr = array_thisptr<array_type>(self);

  if(!thisptr)
    return NULL;

  PyObject *ret = array_selfrepeat(thisptr, count);

  if(ret)
  {
    Py_DECREF(ret);
    Py_INCREF(self);
    return self;
  }

  return NULL;
}
%}

%enddef

%define ARRAY_INSTANTIATION_CHECK_NAME(typeName) add_your_use_of_##typeName##_to_swig_interface %enddef

// these pair of macros (TEMPLATE_ARRAY_DECLARE / NON_TEMPLATE_ARRAY_INSTANTIATE) are defined before
// including the header that defines a class, to set up typemaps and other things that must be
// available before the definition is parsed.
//
// Afterwards, you must call EXTEND_ARRAY_CLASS_METHODS to add the necessary named methods.

%define TEMPLATE_ARRAY_DECLARE(typeName)

LIST_MODIFY_IN_PLACE_TYPEMAP(typeName)

// add a check to make sure that we explicitly instantiate all uses of this template (as a reference
// type, not as a purely in parameter to a function - those are converted by value to C++ to allow
// passing pure lists that aren't C++ side at all).
%header %{
template<typename innerType>
void ARRAY_INSTANTIATION_CHECK_NAME(typeName)(typeName<innerType> *);
%}

// override these typemaps to instantiate a checking template.
%typemap(check) typeName * { ARRAY_INSTANTIATION_CHECK_NAME(typeName)($1); }
%typemap(check) typeName & { ARRAY_INSTANTIATION_CHECK_NAME(typeName)($1); }
%typemap(check) typeName   { ARRAY_INSTANTIATION_CHECK_NAME(typeName)($1); }
%typemap(ret) typeName * { ARRAY_INSTANTIATION_CHECK_NAME(typeName)($1); }
%typemap(ret) typeName & { ARRAY_INSTANTIATION_CHECK_NAME(typeName)($1); }
%typemap(ret) typeName   { ARRAY_INSTANTIATION_CHECK_NAME(typeName)(&$1); }

%enddef

%define NON_TEMPLATE_ARRAY_INSTANTIATE(typeName)

ARRAY_ADD_SLOTS(typeName, typeName)
ARRAY_DEFINE_SLOTS(typeName, typeName)

LIST_MODIFY_IN_PLACE_TYPEMAP(typeName)

%enddef

// this instantiates a templated array for a particular type and sets it up to act like a python
// sequence.
%define TEMPLATE_ARRAY_INSTANTIATE(arrayType, innerType)

ARRAY_ADD_SLOTS(arrayType<innerType>, arrayType##_of_##innerType)

// instantiate template
%rename(arrayType##_of_##innerType) arrayType<innerType>;
%template(arrayType##_of_##innerType) arrayType<innerType>;

ARRAY_DEFINE_SLOTS(arrayType<innerType>, arrayType##_of_##innerType)

%header %{

template<>
void ARRAY_INSTANTIATION_CHECK_NAME(arrayType)(arrayType<innerType> *)
{
}

%}

%enddef

// variation of the above to handle pointer'd inner type
%define TEMPLATE_ARRAY_INSTANTIATE_PTR(arrayType, innerType)

ARRAY_ADD_SLOTS(arrayType<innerType *>, arrayType##_of_ptr_##innerType)

// instantiate template
%rename(arrayType##_of_ptr_##innerType) arrayType<innerType *>;
%template(arrayType##_of_ptr_##innerType) arrayType<innerType *>;

ARRAY_DEFINE_SLOTS(arrayType<innerType *>, arrayType##_of_ptr_##innerType)

%header %{

template<>
void ARRAY_INSTANTIATION_CHECK_NAME(arrayType)(arrayType<innerType *> *)
{
}

%}

%enddef

%define TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(arrayType, nspace, innerType)

ARRAY_ADD_SLOTS(arrayType<nspace::innerType>, arrayType##_of_##nspace##_##innerType)

// instantiate template
%rename(arrayType##_of_##nspace##_##innerType) arrayType<nspace::innerType>;
%template(arrayType##_of_##nspace##_##innerType) arrayType<nspace::innerType>;

ARRAY_DEFINE_SLOTS(arrayType<nspace::innerType>, arrayType##_of_##nspace##_##innerType)

%header %{

template<>
void ARRAY_INSTANTIATION_CHECK_NAME(arrayType)(arrayType<nspace::innerType> *)
{
}

%}

%enddef
