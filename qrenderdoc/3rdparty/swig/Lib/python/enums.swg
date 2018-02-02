/* -----------------------------------------------------------------------------
 * enums.swg
 *
 * Include this file in order for C/C++ enums to be wrapped by Python classes.
 * ----------------------------------------------------------------------------- */

// when returning a typed enum, check if there's a tp_dict. If there is, we
// failed to create a true Enum (e.g. running on < 3.4 where it's not available)
// and we just return the int value.
// If there's no tp_dict though, we can use the tp_base that points to the
// python Enum class type object, instead of registering our actual type.
%typemap(out) enum SWIGTYPE {
  SwigPyClientData *cd = NULL;
  PyObject *enum_constructor = NULL;
  PyObject *numerical = SWIG_From_int(static_cast< int >(result));

  cd = (SwigPyClientData*)$&1_descriptor->clientdata;
  if (cd->pytype->tp_dict) {
    $result = numerical;
  } else {
    enum_constructor = (PyObject *)cd->pytype->tp_base;
    resultobj = PyObject_CallFunctionObjArgs(enum_constructor, numerical, NULL);
    Py_DECREF(numerical);
  }
}

#define %pythonenumstrong %feature("python:enum:strong")
#define %pythonenumscoped %feature("python:enum:scoped")
#define %pythonenumclass %feature("python:enum",strong=1,scoped=1)
