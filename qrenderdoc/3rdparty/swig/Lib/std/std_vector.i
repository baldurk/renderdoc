//
// std::vector
//

%include <std_container.i>

// Vector

%define %std_vector_methods(vector...)
  %std_sequence_methods(vector)
  
  void reserve(size_type n);
  size_type capacity() const;
%enddef


%define %std_vector_methods_val(vector...)
  %std_sequence_methods_val(vector)
  
  void reserve(size_type n);
  size_type capacity() const;
%enddef


// ------------------------------------------------------------------------
// std::vector
// 
// The aim of all that follows would be to integrate std::vector with 
// as much as possible, namely, to allow the user to pass and 
// be returned tuples or lists.
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::vector<T>), f(const std::vector<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::vector<T> can be passed.
//   -- f(std::vector<T>&), f(std::vector<T>*):
//      the parameter may be modified; therefore, only a wrapped std::vector
//      can be passed.
//   -- std::vector<T> f(), const std::vector<T>& f():
//      the vector is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::vector<T>& f(), std::vector<T>* f():
//      the vector is returned by reference; therefore, a wrapped std::vector
//      is returned
//   -- const std::vector<T>* f(), f(const std::vector<T>*):
//      for consistency, they expect and return a plain vector pointer.
// ------------------------------------------------------------------------

%{
#include <vector>
%}    

// exported classes


namespace std {

  template<class _Tp, class _Alloc = allocator< _Tp > >
  class vector {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Tp value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef _Tp& reference;
    typedef const _Tp& const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Tp);
    %traits_enum(_Tp);

    %fragment(SWIG_Traits_frag(std::vector< _Tp, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdVectorTraits") {
      namespace swig {
	template <>  struct traits<std::vector< _Tp, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::vector<" #_Tp "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_VECTOR, std::vector< _Tp, _Alloc >);

#ifdef %swig_vector_methods
    // Add swig/language extra methods
    %swig_vector_methods(std::vector< _Tp, _Alloc >);
#endif
  
    %std_vector_methods(vector);
  };

  // ***
  // This specialization should disappear or get simplified when
  // a 'const SWIGTYPE*&' can be defined
  // ***
  template<class _Tp, class _Alloc >
  class vector< _Tp*, _Alloc > {
  public:
    typedef size_t size_type;    
    typedef ptrdiff_t difference_type;
    typedef _Tp* value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type reference;
    typedef value_type const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Tp);

    %fragment(SWIG_Traits_frag(std::vector< _Tp*, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdVectorTraits") {
      namespace swig {
	template <>  struct traits<std::vector< _Tp*, _Alloc > > {
	  typedef value_category category;
	  static const char* type_name() {
	    return "std::vector<" #_Tp " *," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_VECTOR, std::vector< _Tp*, _Alloc >);

#ifdef %swig_vector_methods_val
    // Add swig/language extra methods
    %swig_vector_methods_val(std::vector< _Tp*, _Alloc >);
#endif

    %std_vector_methods_val(vector);
  };

  // ***
  // const pointer specialization
  // ***
  template<class _Tp, class _Alloc >
  class vector< _Tp const *, _Alloc > {
  public:
    typedef size_t size_type;    
    typedef ptrdiff_t difference_type;
    typedef _Tp const * value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type reference;
    typedef value_type const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Tp);

    %fragment(SWIG_Traits_frag(std::vector< _Tp const*, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdVectorTraits") {
      namespace swig {
	template <>  struct traits<std::vector< _Tp const*, _Alloc > > {
	  typedef value_category category;
	  static const char* type_name() {
	    return "std::vector<" #_Tp " const*," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_VECTOR, std::vector< _Tp const*, _Alloc >);

#ifdef %swig_vector_methods_val
    // Add swig/language extra methods
    %swig_vector_methods_val(std::vector< _Tp const*, _Alloc >);
#endif

    %std_vector_methods_val(vector);
  };

  // ***
  // bool specialization
  // ***

  template<class _Alloc > 
  class vector<bool,_Alloc > {
  public:
    typedef size_t size_type;    
    typedef ptrdiff_t difference_type;
    typedef bool value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type reference;
    typedef value_type const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(bool);

    %fragment(SWIG_Traits_frag(std::vector<bool, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(bool),
	      fragment="StdVectorTraits") {
      namespace swig {
	template <>  struct traits<std::vector<bool, _Alloc > > {
	  typedef value_category category;
	  static const char* type_name() {
	    return "std::vector<bool, _Alloc >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_VECTOR, std::vector<bool, _Alloc >);


#ifdef %swig_vector_methods_val
    // Add swig/language extra methods
    %swig_vector_methods_val(std::vector<bool, _Alloc >);
#endif

    %std_vector_methods_val(vector);

#if defined(SWIG_STD_MODERN_STL) && !defined(SWIG_STD_NOMODERN_STL) 
    void flip();
#endif

  };

}
