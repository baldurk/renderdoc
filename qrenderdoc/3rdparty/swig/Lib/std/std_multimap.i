//
// std::multimap
//

%include <std_map.i>


%define %std_multimap_methods(mmap...)
  %std_map_methods_common(mmap);

#ifdef SWIG_EXPORT_ITERATOR_METHODS
  std::pair<iterator,iterator> equal_range(const key_type& x);
  std::pair<const_iterator,const_iterator> equal_range(const key_type& x) const;
#endif
%enddef

// ------------------------------------------------------------------------
// std::multimap
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::multimap<T>), f(const std::multimap<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::multimap<T> can be passed.
//   -- f(std::multimap<T>&), f(std::multimap<T>*):
//      the parameter may be modified; therefore, only a wrapped std::multimap
//      can be passed.
//   -- std::multimap<T> f(), const std::multimap<T>& f():
//      the map is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::multimap<T>& f(), std::multimap<T>* f():
//      the map is returned by reference; therefore, a wrapped std::multimap
//      is returned
//   -- const std::multimap<T>* f(), f(const std::multimap<T>*):
//      for consistency, they expect and return a plain map pointer.
// ------------------------------------------------------------------------


// exported class


namespace std {
  template<class _Key, class _Tp, class _Compare = std::less< _Key >,
	   class _Alloc = allocator<std::pair< const _Key, _Tp > > >
  class multimap {
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

    %fragment(SWIG_Traits_frag(std::pair< _Key, _Tp >), "header",
	      fragment=SWIG_Traits_frag(_Key),
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdPairTraits") {
      namespace swig {
	template <>  struct traits<std::pair< _Key, _Tp > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::pair<" #_Key "," #_Tp " >";
	  }
	};
      }
    }

    %fragment(SWIG_Traits_frag(std::multimap< _Key, _Tp, _Compare, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(std::pair< _Key, _Tp >),
	      fragment="StdMultimapTraits") {
      namespace swig {
	template <>  struct traits<std::multimap< _Key, _Tp, _Compare, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::multimap<" #_Key "," #_Tp "," #_Compare "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_MULTIMAP, std::multimap< _Key, _Tp, _Compare, _Alloc >);
  
    multimap( const _Compare& );

#ifdef %swig_multimap_methods
    // Add swig/language extra methods
    %swig_multimap_methods(std::multimap< _Key, _Tp, _Compare, _Alloc >);
#endif

    %std_multimap_methods(multimap);
  };
}
