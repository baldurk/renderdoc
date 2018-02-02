//
// std::array
//

%include <std_container.i>

%define %std_array_methods(array...)
  %std_sequence_methods_non_resizable(array)
  void fill(const value_type& u);
%enddef


%define %std_array_methods_val(array...)
  %std_sequence_methods_non_resizable_val(array)
  void fill(const value_type& u);
%enddef

// ------------------------------------------------------------------------
// std::array
// 
// The aim of all that follows would be to integrate std::array with 
// as much as possible, namely, to allow the user to pass and 
// be returned tuples or lists.
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::array<T, N>), f(const std::array<T, N>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::array<T, N> can be passed.
//   -- f(std::array<T, N>&), f(std::array<T, N>*):
//      the parameter may be modified; therefore, only a wrapped std::array
//      can be passed.
//   -- std::array<T, N> f(), const std::array<T, N>& f():
//      the array is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::array<T, N>& f(), std::array<T, N>* f():
//      the array is returned by reference; therefore, a wrapped std::array
//      is returned
//   -- const std::array<T, N>* f(), f(const std::array<T, N>*):
//      for consistency, they expect and return a plain array pointer.
// ------------------------------------------------------------------------


// exported classes

namespace std {

  template<class _Tp, size_t _Nm >
  class array {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Tp value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef _Tp& reference;
    typedef const _Tp& const_reference;

    %traits_swigtype(_Tp);
    %traits_enum(_Tp);

    %fragment(SWIG_Traits_frag(std::array< _Tp, _Nm >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdArrayTraits") {
      namespace swig {
	template <>  struct traits<std::array< _Tp, _Nm > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::array<" #_Tp "," #_Nm " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_STDARRAY, std::array< _Tp, _Nm >);

#ifdef %swig_array_methods
    // Add swig/language extra methods
    %swig_array_methods(std::array< _Tp, _Nm >);
#endif

    %std_array_methods(array);
  };
}

