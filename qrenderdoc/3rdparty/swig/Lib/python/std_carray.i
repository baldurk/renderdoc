%include <pycontainer.swg>


%fragment("StdCarrayTraits","header",fragment="StdSequenceTraits")
{
namespace swig {
  template <class T, size_t S>
  struct traits_asptr<std::carray<T, S> >  {
    static int asptr(PyObject *obj, std::carray<T, S> **array) {
      return traits_asptr_stdseq<std::carray<T, S> >::asptr(obj, array);
    }
  };
}
}

%warnfilter(SWIGWARN_IGNORE_OPERATOR_INDEX) std::carray::operator[];

%extend std::carray {
  %fragment(SWIG_Traits_frag(std::carray<_Type, _Size >), "header",
	    fragment="SwigPyIterator_T",
	    fragment=SWIG_Traits_frag(_Type),
	    fragment="StdCarrayTraits") {
    namespace swig {
      template <>  struct traits<std::carray<_Type, _Size > > {
	typedef pointer_category category;
	static const char* type_name() {
	  return "std::carray<" #_Type "," #_Size " >";
	}
      };
    }
  }
  
  %typemaps_asptr(SWIG_TYPECHECK_VECTOR, swig::asptr,
		  SWIG_Traits_frag(std::carray<_Type, _Size >),
		  std::carray<_Type, _Size >);

  %typemap(out,noblock=1) iterator, const_iterator {
    $result = SWIG_NewPointerObj(swig::make_output_iterator((const $type &)$1),
				 swig::SwigPyIterator::descriptor(),SWIG_POINTER_OWN);
  }
  
  inline size_t __len__() const { return self->size(); }
  
  inline const _Type& __getitem__(size_t i) const { return (*self)[i]; }
  
  inline void __setitem__(size_t i, const _Type& v) { (*self)[i] = v; }

  
  swig::SwigPyIterator* __iter__(PyObject **PYTHON_SELF) {
    return swig::make_output_iterator(self->begin(), self->begin(), self->end(), *PYTHON_SELF);
  }
}

%include <std/std_carray.swg>
