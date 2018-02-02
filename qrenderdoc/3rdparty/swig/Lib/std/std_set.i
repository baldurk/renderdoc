//
// std::set
//

%include <std_container.i>
%include <std_pair.i>

// Set
%define %std_set_methods_common(set...)
  set();
  set( const set& );

  bool empty() const;
  size_type size() const;
  void clear();

  void swap(set& v);


  size_type erase(const key_type& x);
  size_type count(const key_type& x) const;
  
#ifdef SWIG_EXPORT_ITERATOR_METHODS
  class iterator;
  class reverse_iterator;

  iterator begin();
  iterator end();
  reverse_iterator rbegin();
  reverse_iterator rend();

%extend {
  // %extend wrapper used for differing definitions of these methods introduced in C++11
  void erase(iterator pos) { $self->erase(pos); }
  void erase(iterator first, iterator last) { $self->erase(first, last); }
}

  iterator find(const key_type& x);
  iterator lower_bound(const key_type& x);
  iterator upper_bound(const key_type& x);
  std::pair<iterator,iterator> equal_range(const key_type& x);
#endif
%enddef

%define %std_set_methods(set...)
  %std_set_methods_common(set);
#ifdef SWIG_EXPORT_ITERATOR_METHODS
  std::pair<iterator,bool> insert(const value_type& __x);
#endif
%enddef

// ------------------------------------------------------------------------
// std::set
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::set<T>), f(const std::set<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::set<T> can be passed.
//   -- f(std::set<T>&), f(std::set<T>*):
//      the parameter may be modified; therefore, only a wrapped std::set
//      can be passed.
//   -- std::set<T> f(), const std::set<T>& f():
//      the set is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::set<T>& f(), std::set<T>* f():
//      the set is returned by reference; therefore, a wrapped std::set
//      is returned
//   -- const std::set<T>* f(), f(const std::set<T>*):
//      for consistency, they expect and return a plain set pointer.
// ------------------------------------------------------------------------

%{
#include <set>
%}

// exported classes

namespace std {

  template <class _Key, class _Compare = std::less< _Key >,
	    class _Alloc = allocator< _Key > >
  class set {
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

    %fragment(SWIG_Traits_frag(std::set< _Key, _Compare, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Key),
	      fragment="StdSetTraits") {
      namespace swig {
	template <>  struct traits<std::set< _Key, _Compare, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::set<" #_Key "," #_Compare "," #_Alloc " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_SET, std::set< _Key, _Compare, _Alloc >);

    set( const _Compare& );

#ifdef %swig_set_methods
    // Add swig/language extra methods
    %swig_set_methods(std::set< _Key, _Compare, _Alloc >);
#endif
  
    %std_set_methods(set);
  };
}
