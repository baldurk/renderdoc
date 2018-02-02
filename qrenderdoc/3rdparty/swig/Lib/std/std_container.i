%include <std_common.i>
%include <exception.i>
%include <std_alloc.i>

%{
#include <algorithm>
%}

// Common non-resizable container methods

%define %std_container_methods_non_resizable(container...)

  container();
  container(const container&);

  bool empty() const;
  size_type size() const;
  void swap(container& v);

%enddef

%define %std_container_methods_forward_iterators(container...)

  #ifdef SWIG_EXPORT_ITERATOR_METHODS
  class iterator;
  class const_iterator;
  iterator begin();
  iterator end();
  #endif

%enddef

%define %std_container_methods_reverse_iterators(container...)

  #ifdef SWIG_EXPORT_ITERATOR_METHODS
  class reverse_iterator;
  class const_reverse_iterator;
  reverse_iterator rbegin();
  reverse_iterator rend();
  #endif

%enddef

// Common container methods

%define %std_container_methods(container...)

  %std_container_methods_non_resizable(%arg(container))
  %std_container_methods_forward_iterators(%arg(container))
  %std_container_methods_reverse_iterators(%arg(container))

  void clear();
  allocator_type get_allocator() const;

%enddef

%define %std_container_methods_without_reverse_iterators(container...)

  %std_container_methods_non_resizable(%arg(container))
  %std_container_methods_forward_iterators(%arg(container))

  void clear();
  allocator_type get_allocator() const;

%enddef

// Common sequence

%define %std_sequence_methods_common(sequence)

  %std_container_methods(%arg(sequence));

  sequence(size_type size);
  void pop_back();

  void resize(size_type new_size);

  #ifdef SWIG_EXPORT_ITERATOR_METHODS
%extend {
  // %extend wrapper used for differing definitions of these methods introduced in C++11
  iterator erase(iterator pos) { return $self->erase(pos); }
  iterator erase(iterator first, iterator last) { return $self->erase(first, last); }
}
  #endif

%enddef

%define %std_sequence_methods_non_resizable(sequence)

  %std_container_methods_non_resizable(%arg(sequence))
  %std_container_methods_forward_iterators(%arg(container))
  %std_container_methods_reverse_iterators(%arg(container))

  const value_type& front() const;
  const value_type& back() const;

%enddef

%define %std_sequence_methods(sequence)

  %std_sequence_methods_common(%arg(sequence));

  sequence(size_type size, const value_type& value);
  void push_back(const value_type& x);

  const value_type& front() const;
  const value_type& back() const;

  void assign(size_type n, const value_type& x);
  void resize(size_type new_size, const value_type& x);

  #ifdef SWIG_EXPORT_ITERATOR_METHODS
%extend {
  // %extend wrapper used for differing definitions of these methods introduced in C++11
  iterator insert(iterator pos, const value_type& x) { return $self->insert(pos, x); }
  void insert(iterator pos, size_type n, const value_type& x) { $self->insert(pos, n, x); }
}
  #endif

%enddef

%define %std_sequence_methods_non_resizable_val(sequence...)

  %std_container_methods_non_resizable(%arg(sequence))
  %std_container_methods_forward_iterators(%arg(container))
  %std_container_methods_reverse_iterators(%arg(container))

  value_type front() const;
  value_type back() const;

#endif

%enddef

%define %std_sequence_methods_val(sequence...)

  %std_sequence_methods_common(%arg(sequence));

  sequence(size_type size, value_type value);
  void push_back(value_type x);

  value_type front() const;
  value_type back() const;

  void assign(size_type n, value_type x);
  void resize(size_type new_size, value_type x);

  #ifdef SWIG_EXPORT_ITERATOR_METHODS
%extend {
  // %extend wrapper used for differing definitions of these methods introduced in C++11
  iterator insert(iterator pos, value_type x) { return $self->insert(pos, x); }
  void insert(iterator pos, size_type n, value_type x) { $self->insert(pos, n, x); }
}
  #endif

%enddef


//
// Ignore member methods for Type with no default constructor
//
%define %std_nodefconst_type(Type...)
%feature("ignore") std::vector< Type >::vector(size_type size);
%feature("ignore") std::vector< Type >::resize(size_type size);
%feature("ignore") std::deque< Type >::deque(size_type size);
%feature("ignore") std::deque< Type >::resize(size_type size);
%feature("ignore") std::list< Type >::list(size_type size);
%feature("ignore") std::list< Type >::resize(size_type size);
%enddef
