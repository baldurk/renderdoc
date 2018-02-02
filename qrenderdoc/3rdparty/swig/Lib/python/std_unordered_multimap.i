/*
  Unordered Multimaps
*/
%include <std_unordered_map.i>

%fragment("StdUnorderedMultimapTraits","header",fragment="StdSequenceTraits")
{
  namespace swig {
    template <class SwigPySeq, class K, class T >
    inline void 
    assign(const SwigPySeq& swigpyseq, std::unordered_multimap<K,T > *unordered_multimap) {
      typedef typename std::unordered_multimap<K,T>::value_type value_type;
      typename SwigPySeq::const_iterator it = swigpyseq.begin();
      for (;it != swigpyseq.end(); ++it) {
	unordered_multimap->insert(value_type(it->first, it->second));
      }
    }

    template <class K, class T>
    struct traits_reserve<std::unordered_multimap<K,T> >  {
      static void reserve(std::unordered_multimap<K,T> &seq, typename std::unordered_multimap<K,T>::size_type n) {
        seq.reserve(n);
      }
    };

    template <class K, class T>
    struct traits_asptr<std::unordered_multimap<K,T> >  {
      typedef std::unordered_multimap<K,T> unordered_multimap_type;
      static int asptr(PyObject *obj, std::unordered_multimap<K,T> **val) {
	int res = SWIG_ERROR;
	if (PyDict_Check(obj)) {
	  SwigVar_PyObject items = PyObject_CallMethod(obj,(char *)"items",NULL);
	  return traits_asptr_stdseq<std::unordered_multimap<K,T>, std::pair<K, T> >::asptr(items, val);
	} else {
	  unordered_multimap_type *p;
	  swig_type_info *descriptor = swig::type_info<unordered_multimap_type>();
	  res = descriptor ? SWIG_ConvertPtr(obj, (void **)&p, descriptor, 0) : SWIG_ERROR;
	  if (SWIG_IsOK(res) && val)  *val = p;
	}
	return res;
      }
    };
      
    template <class K, class T >
    struct traits_from<std::unordered_multimap<K,T> >  {
      typedef std::unordered_multimap<K,T> unordered_multimap_type;
      typedef typename unordered_multimap_type::const_iterator const_iterator;
      typedef typename unordered_multimap_type::size_type size_type;
            
      static PyObject *from(const unordered_multimap_type& unordered_multimap) {
	swig_type_info *desc = swig::type_info<unordered_multimap_type>();
	if (desc && desc->clientdata) {
	  return SWIG_NewPointerObj(new unordered_multimap_type(unordered_multimap), desc, SWIG_POINTER_OWN);
	} else {
	  size_type size = unordered_multimap.size();
	  Py_ssize_t pysize = (size <= (size_type) INT_MAX) ? (Py_ssize_t) size : -1;
	  if (pysize < 0) {
	    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	    PyErr_SetString(PyExc_OverflowError, "unordered_multimap size not valid in python");
	    SWIG_PYTHON_THREAD_END_BLOCK;
	    return NULL;
	  }
	  PyObject *obj = PyDict_New();
	  for (const_iterator i= unordered_multimap.begin(); i!= unordered_multimap.end(); ++i) {
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

%define %swig_unordered_multimap_methods(Type...) 
  %swig_map_common(Type);
  %extend {
    void __setitem__(const key_type& key, const mapped_type& x) throw (std::out_of_range) {
      self->insert(Type::value_type(key,x));
    }
  }
%enddef

%include <std/std_unordered_multimap.i>

