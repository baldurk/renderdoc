//
// std::unordered_set
// Work in progress - the code is not compilable yet:
// operator--() and constructor(compare function) not available for unordered_
// types
//

%include <std_container.i>
%include <std_pair.i>

// Unordered Set
%define %std_unordered_set_methods_common(unordered_set...)
  unordered_set();
  unordered_set( const unordered_set& );

  bool empty() const;
  size_type size() const;
  void clear();

  void swap(unordered_set& v);


  size_type erase(const key_type& x);
  size_type count(const key_type& x) const;
  
#ifdef SWIG_EXPORT_ITERATOR_METHODS
  class iterator;

  iterator begin();
  iterator end();

%extend {
  // %extend wrapper used for differing definitions of these methods introduced in C++11
  void erase(iterator pos) { $self->erase(pos); }
  void erase(iterator first, iterator last) { $self->erase(first, last); }
}

  iterator find(const key_type& x);
  std::pair<iterator,iterator> equal_range(const key_type& x);
#endif
%enddef

%define %std_unordered_set_methods(unordered_set...)
  %std_unordered_set_methods_common(unordered_set);
#ifdef SWIG_EXPORT_ITERATOR_METHODS
  std::pair<iterator,bool> insert(const value_type& __x);
#endif
%enddef

// ------------------------------------------------------------------------
// std::unordered_set
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::unordered_set<T>), f(const std::unordered_set<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::unordered_set<T> can be passed.
//   -- f(std::unordered_set<T>&), f(std::unordered_set<T>*):
//      the parameter may be modified; therefore, only a wrapped std::unordered_set
//      can be passed.
//   -- std::unordered_set<T> f(), const std::unordered_set<T>& f():
//      the unordered_set is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::unordered_set<T>& f(), std::unordered_set<T>* f():
//      the unordered_set is returned by reference; therefore, a wrapped std::unordered_set
//      is returned
//   -- const std::unordered_set<T>* f(), f(const std::unordered_set<T>*):
//      for consistency, they expect and return a plain unordered_set pointer.
// ------------------------------------------------------------------------

%{
#include <unordered_set>
%}

// exported classes

namespace std {

  template <class _Key, class _Hash = std::hash< _Key >,
            class _Compare = std::equal_to< _Key >,
	    class _Alloc = allocator< _Key > >
  class unordered_set {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Hash hasher;
    typedef _Key value_type;
    typedef _Key key_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Key);

    %fragment(SWIG_Traits_frag(std::unordered_set< _Key, _Hash, _Compare, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Key),
	      fragment="StdUnorderedSetTraits") {
      namespace swig {
	template <>  struct traits<std::unordered_set< _Key, _Hash, _Compare, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::unordered_set<" #_Key "," #_Hash "," #_Compare "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_SET, std::unordered_set< _Key, _Hash, _Compare, _Alloc >);

    unordered_set( const _Compare& );

#ifdef %swig_unordered_set_methods
    // Add swig/language extra methods
    %swig_unordered_set_methods(std::unordered_set< _Key, _Hash, _Compare, _Alloc >);
#endif
  
    %std_unordered_set_methods(unordered_set);
  };
}
