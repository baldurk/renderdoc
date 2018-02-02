/*
  Multimaps
*/
%include <std_map.i>

%fragment("StdMultimapTraits","header",fragment="StdMapCommonTraits")
{
  namespace swig {
    template <class SwigPySeq, class K, class T >
    inline void 
    assign(const SwigPySeq& swigpyseq, std::multimap<K,T > *multimap) {
      typedef typename std::multimap<K,T>::value_type value_type;
      typename SwigPySeq::const_iterator it = swigpyseq.begin();
      for (;it != swigpyseq.end(); ++it) {
	multimap->insert(value_type(it->first, it->second));
      }
    }

    template <class K, class T>
    struct traits_asptr<std::multimap<K,T> >  {
      typedef std::multimap<K,T> multimap_type;
      static int asptr(PyObject *obj, std::multimap<K,T> **val) {
	int res = SWIG_ERROR;
	if (PyDict_Check(obj)) {
	  SwigVar_PyObject items = PyObject_CallMethod(obj,(char *)"items",NULL);
	  return traits_asptr_stdseq<std::multimap<K,T>, std::pair<K, T> >::asptr(items, val);
	} else {
	  multimap_type *p;
	  swig_type_info *descriptor = swig::type_info<multimap_type>();
	  res = descriptor ? SWIG_ConvertPtr(obj, (void **)&p, descriptor, 0) : SWIG_ERROR;
	  if (SWIG_IsOK(res) && val)  *val = p;
	}
	return res;
      }
    };
      
    template <class K, class T >
    struct traits_from<std::multimap<K,T> >  {
      typedef std::multimap<K,T> multimap_type;
      typedef typename multimap_type::const_iterator const_iterator;
      typedef typename multimap_type::size_type size_type;
            
      static PyObject *from(const multimap_type& multimap) {
	swig_type_info *desc = swig::type_info<multimap_type>();
	if (desc && desc->clientdata) {
	  return SWIG_InternalNewPointerObj(new multimap_type(multimap), desc, SWIG_POINTER_OWN);
	} else {
	  size_type size = multimap.size();
	  Py_ssize_t pysize = (size <= (size_type) INT_MAX) ? (Py_ssize_t) size : -1;
	  if (pysize < 0) {
	    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	    PyErr_SetString(PyExc_OverflowError, "multimap size not valid in python");
	    SWIG_PYTHON_THREAD_END_BLOCK;
	    return NULL;
	  }
	  PyObject *obj = PyDict_New();
	  for (const_iterator i= multimap.begin(); i!= multimap.end(); ++i) {
	    swig::SwigVar_PyObject key = swig::from(i->first);
	    swig::SwigVar_PyObject val = swig::from(i->second);
	    PyDict_SetItem(obj, key, val);
	  }
	  return obj;
	}
      }
    };
  }
}

%define %swig_multimap_methods(Type...) 
  %swig_map_common(Type);
  %extend {
    void __setitem__(const key_type& key, const mapped_type& x) throw (std::out_of_range) {
      self->insert(Type::value_type(key,x));
    }
  }
%enddef

%include <std/std_multimap.i>

