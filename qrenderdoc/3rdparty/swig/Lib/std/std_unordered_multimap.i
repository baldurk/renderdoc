//
// std::unordered_multimap
// Work in progress - the code is not compilable yet:
// operator--() and constructor(compare function) not available for unordered_
// types
//

%include <std_unordered_map.i>


%define %std_unordered_multimap_methods(mmap...)
  %std_map_methods_common(mmap);

#ifdef SWIG_EXPORT_ITERATOR_METHODS
  std::pair<iterator,iterator> equal_range(const key_type& x);
  std::pair<const_iterator,const_iterator> equal_range(const key_type& x) const;
#endif
%enddef

// ------------------------------------------------------------------------
// std::unordered_multimap
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::unordered_multimap<T>), f(const std::unordered_multimap<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::unordered_multimap<T> can be passed.
//   -- f(std::unordered_multimap<T>&), f(std::unordered_multimap<T>*):
//      the parameter may be modified; therefore, only a wrapped std::unordered_multimap
//      can be passed.
//   -- std::unordered_multimap<T> f(), const std::unordered_multimap<T>& f():
//      the map is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::unordered_multimap<T>& f(), std::unordered_multimap<T>* f():
//      the map is returned by reference; therefore, a wrapped std::unordered_multimap
//      is returned
//   -- const std::unordered_multimap<T>* f(), f(const std::unordered_multimap<T>*):
//      for consistency, they expect and return a plain map pointer.
// ------------------------------------------------------------------------


// exported class


namespace std {
  template<class _Key, class _Tp, class _Compare = std::less< _Key >,
	   class _Alloc = allocator<std::pair< const _Key, _Tp > > >
  class unordered_multimap {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Key key_type;
    typedef _Tp mapped_type;
    typedef std::pair< const _Key, _Tp > value_type;

    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Key);
    %traits_swigtype(_Tp);	    

    %fragment(SWIG_Traits_frag(std::unordered_multimap< _Key, _Tp, _Compare, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(std::pair< _Key, _Tp >),
	      fragment="StdMultimapTraits") {
      namespace swig {
	template <>  struct traits<std::unordered_multimap< _Key, _Tp, _Compare, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::unordered_multimap<" #_Key "," #_Tp "," #_Compare "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_MULTIMAP, std::unordered_multimap< _Key, _Tp, _Compare, _Alloc >);
  
    unordered_multimap( const _Compare& );

#ifdef %swig_unordered_multimap_methods
    // Add swig/language extra methods
    %swig_unordered_multimap_methods(std::unordered_multimap< _Key, _Tp, _Compare, _Alloc >);
#endif

    %std_unordered_multimap_methods(unordered_multimap);
  };
}
