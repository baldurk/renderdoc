//
// std::multiset
//

%include <std_set.i>

// Multiset

%define %std_multiset_methods(multiset...)
  %std_set_methods_common(multiset);
%enddef


// ------------------------------------------------------------------------
// std::multiset
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::multiset<T>), f(const std::multiset<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::multiset<T> can be passed.
//   -- f(std::multiset<T>&), f(std::multiset<T>*):
//      the parameter may be modified; therefore, only a wrapped std::multiset
//      can be passed.
//   -- std::multiset<T> f(), const std::multiset<T>& f():
//      the set is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::multiset<T>& f(), std::multiset<T>* f():
//      the set is returned by reference; therefore, a wrapped std::multiset
//      is returned
//   -- const std::multiset<T>* f(), f(const std::multiset<T>*):
//      for consistency, they expect and return a plain set pointer.
// ------------------------------------------------------------------------


// exported classes

namespace std {

  //multiset

  template <class _Key, class _Compare = std::less< _Key >,
	    class _Alloc = allocator< _Key > >
  class multiset {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Key value_type;
    typedef _Key key_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Key);

    %fragment(SWIG_Traits_frag(std::multiset< _Key, _Compare, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Key),
	      fragment="StdMultisetTraits") {
      namespace swig {
	template <>  struct traits<std::multiset< _Key, _Compare, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::multiset<" #_Key "," #_Compare "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_MULTISET, std::multiset< _Key, _Compare, _Alloc >);

    multiset( const _Compare& );

#ifdef %swig_multiset_methods
    // Add swig/language extra methods
    %swig_multiset_methods(std::multiset< _Key, _Compare, _Alloc >);
#endif
  
    %std_multiset_methods(multiset);
  };
}
