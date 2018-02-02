/*
  Unordered Maps
*/

%fragment("StdMapTraits","header",fragment="StdSequenceTraits")
{
  namespace swig {
    template <class SwigPySeq, class K, class T >
    inline void
    assign(const SwigPySeq& swigpyseq, std::unordered_map<K,T > *unordered_map) {
      typedef typename std::unordered_map<K,T>::value_type value_type;
      typename SwigPySeq::const_iterator it = swigpyseq.begin();
      for (;it != swigpyseq.end(); ++it) {
	unordered_map->insert(value_type(it->first, it->second));
      }
    }

    template <class K, class T>
    struct traits_reserve<std::unordered_map<K,T> > {
      static void reserve(std::unordered_map<K,T> &seq, typename std::unordered_map<K,T>::size_type n) {
        seq.reserve(n);
      }
    };

    template <class K, class T>
    struct traits_asptr<std::unordered_map<K,T> >  {
      typedef std::unordered_map<K,T> unordered_map_type;
      static int asptr(PyObject *obj, unordered_map_type **val) {
	int res = SWIG_ERROR;
	if (PyDict_Check(obj)) {
	  SwigVar_PyObject items = PyObject_CallMethod(obj,(char *)"items",NULL);
%#if PY_VERSION_HEX >= 0x03000000
          /* In Python 3.x the ".items()" method return a dict_items object */
          items = PySequence_Fast(items, ".items() havn't returned a sequence!");
%#endif
	  res = traits_asptr_stdseq<std::unordered_map<K,T>, std::pair<K, T> >::asptr(items, val);
	} else {
	  unordered_map_type *p;
	  swig_type_info *descriptor = swig::type_info<unordered_map_type>();
	  res = descriptor ? SWIG_ConvertPtr(obj, (void **)&p, descriptor, 0) : SWIG_ERROR;
	  if (SWIG_IsOK(res) && val)  *val = p;
	}
	return res;
      }      
    };
      
    template <class K, class T >
    struct traits_from<std::unordered_map<K,T> >  {
      typedef std::unordered_map<K,T> unordered_map_type;
      typedef typename unordered_map_type::const_iterator const_iterator;
      typedef typename unordered_map_type::size_type size_type;
            
      static PyObject *from(const unordered_map_type& unordered_map) {
	swig_type_info *desc = swig::type_info<unordered_map_type>();
	if (desc && desc->clientdata) {
	  return SWIG_NewPointerObj(new unordered_map_type(unordered_map), desc, SWIG_POINTER_OWN);
	} else {
	  size_type size = unordered_map.size();
	  Py_ssize_t pysize = (size <= (size_type) INT_MAX) ? (Py_ssize_t) size : -1;
	  if (pysize < 0) {
	    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	    PyErr_SetString(PyExc_OverflowError, "unordered_map size not valid in python");
	    SWIG_PYTHON_THREAD_END_BLOCK;
	    return NULL;
	  }
	  PyObject *obj = PyDict_New();
	  for (const_iterator i= unordered_map.begin(); i!= unordered_map.end(); ++i) {
	    swig::SwigVar_PyObject key = swig::from(i->first);
	    swig::SwigVar_PyObject val = swig::from(i->second);
	    PyDict_SetItem(obj, key, val);
	  }
	  return obj;
	}
      }
    };

    template <class ValueType>
    struct from_key_oper 
    {
      typedef const ValueType& argument_type;
      typedef  PyObject *result_type;
      result_type operator()(argument_type v) const
      {
	return swig::from(v.first);
      }
    };

    template <class ValueType>
    struct from_value_oper 
    {
      typedef const ValueType& argument_type;
      typedef  PyObject *result_type;
      result_type operator()(argument_type v) const
      {
	return swig::from(v.second);
      }
    };

    template<class OutIterator, class FromOper, class ValueType = typename OutIterator::value_type>
    struct SwigPyMapIterator_T : SwigPyIteratorClosed_T<OutIterator, ValueType, FromOper>
    {
      SwigPyMapIterator_T(OutIterator curr, OutIterator first, OutIterator last, PyObject *seq)
	: SwigPyIteratorClosed_T<OutIterator,ValueType,FromOper>(curr, first, last, seq)
      {
      }
    };


    template<class OutIterator,
	     class FromOper = from_key_oper<typename OutIterator::value_type> >
    struct SwigPyMapKeyIterator_T : SwigPyMapIterator_T<OutIterator, FromOper>
    {
      SwigPyMapKeyIterator_T(OutIterator curr, OutIterator first, OutIterator last, PyObject *seq)
	: SwigPyMapIterator_T<OutIterator, FromOper>(curr, first, last, seq)
      {
      }
    };

    template<typename OutIter>
    inline SwigPyIterator*
    make_output_key_iterator(const OutIter& current, const OutIter& begin, const OutIter& end, PyObject *seq = 0)
    {
      return new SwigPyMapKeyIterator_T<OutIter>(current, begin, end, seq);
    }

    template<class OutIterator,
	     class FromOper = from_value_oper<typename OutIterator::value_type> >
    struct SwigPyMapValueITerator_T : SwigPyMapIterator_T<OutIterator, FromOper>
    {
      SwigPyMapValueITerator_T(OutIterator curr, OutIterator first, OutIterator last, PyObject *seq)
	: SwigPyMapIterator_T<OutIterator, FromOper>(curr, first, last, seq)
      {
      }
    };
    

    template<typename OutIter>
    inline SwigPyIterator*
    make_output_value_iterator(const OutIter& current, const OutIter& begin, const OutIter& end, PyObject *seq = 0)
    {
      return new SwigPyMapValueITerator_T<OutIter>(current, begin, end, seq);
    }
  }
}

%define %swig_unordered_map_common(Map...)
  %swig_sequence_iterator(Map);
  %swig_container_methods(Map)

  %extend {
    mapped_type __getitem__(const key_type& key) const throw (std::out_of_range) {
      Map::const_iterator i = self->find(key);
      if (i != self->end())
	return i->second;
      else
	throw std::out_of_range("key not found");
    }
    
    void __delitem__(const key_type& key) throw (std::out_of_range) {
      Map::iterator i = self->find(key);
      if (i != self->end())
	self->erase(i);
      else
	throw std::out_of_range("key not found");
    }
    
    bool has_key(const key_type& key) const {
      Map::const_iterator i = self->find(key);
      return i != self->end();
    }
    
    PyObject* keys() {
      Map::size_type size = self->size();
      Py_ssize_t pysize = (size <= (Map::size_type) INT_MAX) ? (Py_ssize_t) size : -1;
      if (pysize < 0) {
	SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	PyErr_SetString(PyExc_OverflowError, "unordered_map size not valid in python");
	SWIG_PYTHON_THREAD_END_BLOCK;
	return NULL;
      }
      PyObject* keyList = PyList_New(pysize);
      Map::const_iterator i = self->begin();
      for (Py_ssize_t j = 0; j < pysize; ++i, ++j) {
	PyList_SET_ITEM(keyList, j, swig::from(i->first));
      }
      return keyList;
    }
    
    PyObject* values() {
      Map::size_type size = self->size();
      Py_ssize_t pysize = (size <= (Map::size_type) INT_MAX) ? (Py_ssize_t) size : -1;
      if (pysize < 0) {
	SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	PyErr_SetString(PyExc_OverflowError, "unordered_map size not valid in python");
	SWIG_PYTHON_THREAD_END_BLOCK;
	return NULL;
      }
      PyObject* valList = PyList_New(pysize);
      Map::const_iterator i = self->begin();
      for (Py_ssize_t j = 0; j < pysize; ++i, ++j) {
	PyList_SET_ITEM(valList, j, swig::from(i->second));
      }
      return valList;
    }
    
    PyObject* items() {
      Map::size_type size = self->size();
      Py_ssize_t pysize = (size <= (Map::size_type) INT_MAX) ? (Py_ssize_t) size : -1;
      if (pysize < 0) {
	SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	PyErr_SetString(PyExc_OverflowError, "unordered_map size not valid in python");
	SWIG_PYTHON_THREAD_END_BLOCK;
	return NULL;
      }    
      PyObject* itemList = PyList_New(pysize);
      Map::const_iterator i = self->begin();
      for (Py_ssize_t j = 0; j < pysize; ++i, ++j) {
	PyList_SET_ITEM(itemList, j, swig::from(*i));
      }
      return itemList;
    }
    
    // Python 2.2 methods
    bool __contains__(const key_type& key) {
      return self->find(key) != self->end();
    }

    %newobject key_iterator(PyObject **PYTHON_SELF);
    swig::SwigPyIterator* key_iterator(PyObject **PYTHON_SELF) {
      return swig::make_output_key_iterator(self->begin(), self->begin(), self->end(), *PYTHON_SELF);
    }

    %newobject value_iterator(PyObject **PYTHON_SELF);
    swig::SwigPyIterator* value_iterator(PyObject **PYTHON_SELF) {
      return swig::make_output_value_iterator(self->begin(), self->begin(), self->end(), *PYTHON_SELF);
    }

    %pythoncode %{def __iter__(self):
    return self.key_iterator()%}
    %pythoncode %{def iterkeys(self):
    return self.key_iterator()%}
    %pythoncode %{def itervalues(self):
    return self.value_iterator()%}
    %pythoncode %{def iteritems(self):
    return self.iterator()%}
  }
%enddef

%define %swig_unordered_map_methods(Map...)
  %swig_unordered_map_common(Map)
  %extend {
    void __setitem__(const key_type& key, const mapped_type& x) throw (std::out_of_range) {
      (*self)[key] = x;
    }
  }
%enddef


%include <std/std_unordered_map.i>
