//
// std::deque

%include <std_container.i>

// Deque

%define %std_deque_methods(deque...)  
  %std_sequence_methods(deque)

  void pop_front();
  void push_front(const value_type& x);
%enddef

%define %std_deque_methods_val(deque...)
  %std_sequence_methods_val(deque)

  void pop_front();
  void push_front(value_type x);
%enddef

// ------------------------------------------------------------------------
// std::deque
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::deque<T>), f(const std::deque<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::deque<T> can be passed.
//   -- f(std::deque<T>&), f(std::deque<T>*):
//      the parameter may be modified; therefore, only a wrapped std::deque
//      can be passed.
//   -- std::deque<T> f(), const std::deque<T>& f():
//      the deque is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::deque<T>& f(), std::deque<T>* f():
//      the deque is returned by reference; therefore, a wrapped std::deque
//      is returned
//   -- const std::deque<T>* f(), f(const std::deque<T>*):
//      for consistency, they expect and return a plain deque pointer.
// ------------------------------------------------------------------------

%{
#include <deque>
%}

// exported classes

namespace std {

  template<class _Tp, class _Alloc = allocator< _Tp > >
  class deque {
  public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef _Tp value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef _Alloc allocator_type;

    %traits_swigtype(_Tp);

    %fragment(SWIG_Traits_frag(std::deque< _Tp, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdDequeTraits") {
      namespace swig {
	template <>  struct traits<std::deque< _Tp, _Alloc > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::deque<" #_Tp " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_DEQUE, std::deque< _Tp, _Alloc >);
  
#ifdef %swig_deque_methods
    // Add swig/language extra methods
    %swig_deque_methods(std::deque< _Tp, _Alloc >);
#endif

    %std_deque_methods(deque);
  };

  template<class _Tp, class _Alloc > 
  class deque< _Tp*, _Alloc > {
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

    %fragment(SWIG_Traits_frag(std::deque< _Tp*, _Alloc >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdDequeTraits") {
      namespace swig {
	template <>  struct traits<std::deque< _Tp*, _Alloc > > {
	  typedef value_category category;
	  static const char* type_name() {
	    return "std::deque<" #_Tp " * >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_DEQUE, std::deque< _Tp*, _Alloc >);

#ifdef %swig_deque_methods_val
    // Add swig/language extra methods
    %swig_deque_methods_val(std::deque< _Tp*, _Alloc >);
#endif

    %std_deque_methods_val(deque);
  };

}

