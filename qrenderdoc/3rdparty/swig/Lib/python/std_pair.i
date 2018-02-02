/*
  Pairs
*/
%include <pystdcommon.swg>

//#define SWIG_STD_PAIR_ASVAL

%fragment("StdPairTraits","header",fragment="StdTraits") {
  namespace swig {
#ifdef SWIG_STD_PAIR_ASVAL
    template <class T, class U >
    struct traits_asval<std::pair<T,U> >  {
      typedef std::pair<T,U> value_type;

      static int get_pair(PyObject* first, PyObject* second,
			  std::pair<T,U> *val)
      {
	if (val) {
	  T *pfirst = &(val->first);
	  int res1 = swig::asval((PyObject*)first, pfirst);
	  if (!SWIG_IsOK(res1)) return res1;
	  U *psecond = &(val->second);
	  int res2 = swig::asval((PyObject*)second, psecond);
	  if (!SWIG_IsOK(res2)) return res2;
	  return res1 > res2 ? res1 : res2;
	} else {
	  T *pfirst = 0;
	  int res1 = swig::asval((PyObject*)first, 0);
	  if (!SWIG_IsOK(res1)) return res1;
	  U *psecond = 0;
	  int res2 = swig::asval((PyObject*)second, psecond);
	  if (!SWIG_IsOK(res2)) return res2;
	  return res1 > res2 ? res1 : res2;
	}
      }

      static int asval(PyObject *obj, std::pair<T,U> *val) {
	int res = SWIG_ERROR;
	if (PyTuple_Check(obj)) {
	  if (PyTuple_GET_SIZE(obj) == 2) {
	    res = get_pair(PyTuple_GET_ITEM(obj,0),PyTuple_GET_ITEM(obj,1), val);
	  }
	} else if (PySequence_Check(obj)) {
	  if (PySequence_Size(obj) == 2) {
	    swig::SwigVar_PyObject first = PySequence_GetItem(obj,0);
	    swig::SwigVar_PyObject second = PySequence_GetItem(obj,1);
	    res = get_pair(first, second, val);
	  }
	} else {
	  value_type *p;
	  swig_type_info *descriptor = swig::type_info<value_type>();
	  res = descriptor ? SWIG_ConvertPtr(obj, (void **)&p, descriptor, 0) : SWIG_ERROR;
	  if (SWIG_IsOK(res) && val)  *val = *p;
	}
	return res;
      }
    };

#else
    template <class T, class U >
    struct traits_asptr<std::pair<T,U> >  {
      typedef std::pair<T,U> value_type;

      static int get_pair(PyObject* first, PyObject* second,
			  std::pair<T,U> **val) 
      {
	if (val) {
	  value_type *vp = %new_instance(std::pair<T,U>);
	  T *pfirst = &(vp->first);
	  int res1 = swig::asval((PyObject*)first, pfirst);
	  if (!SWIG_IsOK(res1)) {
	    %delete(vp);
	    return res1;
	  }
	  U *psecond = &(vp->second);
	  int res2 = swig::asval((PyObject*)second, psecond);
	  if (!SWIG_IsOK(res2)) {
	    %delete(vp);
	    return res2;
	  }
	  *val = vp;
	  return SWIG_AddNewMask(res1 > res2 ? res1 : res2);
	} else {
	  T *pfirst = 0;
	  int res1 = swig::asval((PyObject*)first, pfirst);
	  if (!SWIG_IsOK(res1)) return res1;
	  U *psecond = 0;
	  int res2 = swig::asval((PyObject*)second, psecond);
	  if (!SWIG_IsOK(res2)) return res2;
	  return res1 > res2 ? res1 : res2;
	}
      }

      static int asptr(PyObject *obj, std::pair<T,U> **val) {
	int res = SWIG_ERROR;
	if (PyTuple_Check(obj)) {
	  if (PyTuple_GET_SIZE(obj) == 2) {
	    res = get_pair(PyTuple_GET_ITEM(obj,0),PyTuple_GET_ITEM(obj,1), val);
	  }
	} else if (PySequence_Check(obj)) {
	  if (PySequence_Size(obj) == 2) {
	    swig::SwigVar_PyObject first = PySequence_GetItem(obj,0);
	    swig::SwigVar_PyObject second = PySequence_GetItem(obj,1);
	    res = get_pair(first, second, val);
	  }
	} else {
	  value_type *p;
	  swig_type_info *descriptor = swig::type_info<value_type>();
	  res = descriptor ? SWIG_ConvertPtr(obj, (void **)&p, descriptor, 0) : SWIG_ERROR;
	  if (SWIG_IsOK(res) && val)  *val = p;
	}
	return res;
      }
    };

#endif
    template <class T, class U >
    struct traits_from<std::pair<T,U> >   {
      static PyObject *from(const std::pair<T,U>& val) {
	PyObject* obj = PyTuple_New(2);
	PyTuple_SetItem(obj,0,swig::from(val.first));
	PyTuple_SetItem(obj,1,swig::from(val.second));
	return obj;
      }
    };
  }

#if defined(SWIGPYTHON_BUILTIN)
SWIGINTERN Py_ssize_t
SwigPython_std_pair_len (PyObject *a)
{
    return 2;
}

SWIGINTERN PyObject*
SwigPython_std_pair_repr (PyObject *o)
{
    PyObject *tuple = PyTuple_New(2);
    assert(tuple);
    PyTuple_SET_ITEM(tuple, 0, PyObject_GetAttrString(o, (char*) "first"));
    PyTuple_SET_ITEM(tuple, 1, PyObject_GetAttrString(o, (char*) "second"));
    PyObject *result = PyObject_Repr(tuple);
    Py_DECREF(tuple);
    return result;
}

SWIGINTERN PyObject*
SwigPython_std_pair_getitem (PyObject *a, Py_ssize_t b)
{
    PyObject *result = PyObject_GetAttrString(a, b % 2 ? (char*) "second" : (char*) "first");
    return result;
}

SWIGINTERN int
SwigPython_std_pair_setitem (PyObject *a, Py_ssize_t b, PyObject *c)
{
    int result = PyObject_SetAttrString(a, b % 2 ? (char*) "second" : (char*) "first", c);
    return result;
}
#endif

}

%feature("python:sq_length") std::pair "SwigPython_std_pair_len";
%feature("python:sq_length") std::pair<T*,U> "SwigPython_std_pair_len";
%feature("python:sq_length") std::pair<T,U*> "SwigPython_std_pair_len";
%feature("python:sq_length") std::pair<T*,U*> "SwigPython_std_pair_len";

%feature("python:tp_repr") std::pair "SwigPython_std_pair_repr";
%feature("python:tp_repr") std::pair<T*,U> "SwigPython_std_pair_repr";
%feature("python:tp_repr") std::pair<T,U*> "SwigPython_std_pair_repr";
%feature("python:tp_repr") std::pair<T*,U*> "SwigPython_std_pair_repr";

%feature("python:sq_item") std::pair "SwigPython_std_pair_getitem";
%feature("python:sq_item") std::pair<T*,U> "SwigPython_std_pair_getitem";
%feature("python:sq_item") std::pair<T,U*> "SwigPython_std_pair_getitem";
%feature("python:sq_item") std::pair<T*,U*> "SwigPython_std_pair_getitem";

%feature("python:sq_ass_item") std::pair "SwigPython_std_pair_setitem";
%feature("python:sq_ass_item") std::pair<T*,U> "SwigPython_std_pair_setitem";
%feature("python:sq_ass_item") std::pair<T,U*> "SwigPython_std_pair_setitem";
%feature("python:sq_ass_item") std::pair<T*,U*> "SwigPython_std_pair_setitem";

%define %swig_pair_methods(pair...)
#if !defined(SWIGPYTHON_BUILTIN)
%extend {      
%pythoncode %{def __len__(self):
    return 2
def __repr__(self):
    return str((self.first, self.second))
def __getitem__(self, index): 
    if not (index % 2):
        return self.first
    else:
        return self.second
def __setitem__(self, index, val):
    if not (index % 2):
        self.first = val
    else:
        self.second = val%}
}
#endif
%enddef

%include <std/std_pair.i>

