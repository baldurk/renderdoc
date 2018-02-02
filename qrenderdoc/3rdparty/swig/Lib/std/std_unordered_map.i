//
// std::unordered_map
// Work in progress - the code is not compilable yet:
// operator--() and constructor(compare function) not available for unordered_
// types
//

%include <std_pair.i>
%include <std_container.i>

%define %std_unordered_map_methods_common(unordered_map...)
  %std_container_methods(unordered_map);

  size_type erase(const key_type& x);
  size_type count(const key_type& x) const;

#ifdef SWIG_EXPORT_ITERATOR_METHODS
%extend {
  // %extend wrapper used for differing definitions of these methods introduced in C++11
  void erase(iterator position) { $self->erase(position); }
  void erase(iterator first, iterator last) { $self->erase(first, last); }
}

  iterator find(const key_type& x);
  iterator lower_bound(const key_type& x);
  iterator upper_bound(const key_type& x);
#endif
%enddef

%define %std_unordered_map_methods(unordered_map...)
  %std_unordered_map_methods_common(unordered_map);

  #ifdef SWIG_EXPORT_ITERATOR_METHODS
//  iterator insert(const value_type& x);
  #endif
%enddef


// ------------------------------------------------------------------------
// std::unordered_map
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::unordered_map<T>), f(const std::unordered_map<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::unordered_map<T> can be passed.
//   -- f(std::unordered_map<T>&), f(std::unordered_map<T>*):
//      the parameter may be modified; therefore, only a wrapped std::unordered_map
//      can be passed.
//   -- std::unordered_map<T> f(), const std::unordered_map<T>& f():
//      the unordered_map is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::unordered_map<T>& f(), std::unordered_map<T>* f():
//      the unordered_map is returned by reference; therefore, a wrapped std::unordered_map
//      is returned
//   -- const std::unordered_map<T>* f(), f(const std::unordered_map<T>*):
//      for consistency, they expect and return a plain unordered_map pointer.
// ------------------------------------------------------------------------

%{
#include <unordered_map>
%}
%fragment("<algorithm>");
%fragment("<stdexcept>");

// exported class

namespace std {

  template<class _Key, class _Tp, class _Compare = std::less< _Key >,
	   class _Alloc = allocator<std::pair< const _Key, _Tp > > >
  class unordered_map {
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

    %fragment(SWIG_Traits_frag(std::unordered_map< _Key, _Tp, _Compare, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(std::pair< _Key, _Tp >),
	      fragment="StdMapTraits") {
      namespace swig {
	template <>  struct traits<std::unordered_map< _Key, _Tp, _Compare, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::unordered_map<" #_Key "," #_Tp "," #_Compare "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_MAP, std::unordered_map< _Key, _Tp, _Compare, _Alloc >);

    unordered_map( const _Compare& );

#ifdef %swig_unordered_map_methods
    // Add swig/language extra methods
    %swig_unordered_map_methods(std::unordered_map< _Key, _Tp, _Compare, _Alloc >);
#endif
  
    %std_unordered_map_methods(unordered_map);
  };

}
