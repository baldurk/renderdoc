/**
 * @file   std_stack.i
 * @date   Sun May  6 01:48:07 2007
 * 
 * @brief  A wrapping of std::stack for Ruby.
 * 
 * 
 */

%include <std_container.i>

// Stack

%define %std_stack_methods(stack...)
  stack();
  stack( const _Sequence& );

  bool empty() const;
  size_type size() const;
  const value_type& top() const;
  void pop();
  void push( const value_type& );
%enddef

%define %std_stack_methods_val(stack...) 
  %std_stack_methods(stack)
%enddef

// ------------------------------------------------------------------------
// std::stack
// 
// const declarations are used to guess the intent of the function being
// exported; therefore, the following rationale is applied:
// 
//   -- f(std::stack<T>), f(const std::stack<T>&):
//      the parameter being read-only, either a sequence or a
//      previously wrapped std::stack<T> can be passed.
//   -- f(std::stack<T>&), f(std::stack<T>*):
//      the parameter may be modified; therefore, only a wrapped std::stack
//      can be passed.
//   -- std::stack<T> f(), const std::stack<T>& f():
//      the stack is returned by copy; therefore, a sequence of T:s 
//      is returned which is most easily used in other functions
//   -- std::stack<T>& f(), std::stack<T>* f():
//      the stack is returned by reference; therefore, a wrapped std::stack
//      is returned
//   -- const std::stack<T>* f(), f(const std::stack<T>*):
//      for consistency, they expect and return a plain stack pointer.
// ------------------------------------------------------------------------

%{
#include <stack>
%}

// exported classes

namespace std {

  template<class _Tp, class _Sequence = std::deque< _Tp > >
  class stack {
  public:
    typedef size_t size_type;
    typedef _Tp value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef _Sequence container_type;

    %traits_swigtype(_Tp);

    %fragment(SWIG_Traits_frag(std::stack< _Tp, _Sequence >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdStackTraits") {
      namespace swig {
	template <>  struct traits<std::stack< _Tp, _Sequence > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::stack<" #_Tp "," #_Sequence " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_STACK, std::stack< _Tp, _Sequence >);
  
#ifdef %swig_stack_methods
    // Add swig/language extra methods
    %swig_stack_methods(std::stack< _Tp, _Sequence >);
#endif

    %std_stack_methods(stack);
  };

  template<class _Tp, class _Sequence > 
  class stack< _Tp*, _Sequence > {
  public:
    typedef size_t size_type;
    typedef _Sequence::value_type value_type;
    typedef value_type reference;
    typedef value_type const_reference;
    typedef _Sequence container_type;

    %traits_swigtype(_Tp);

    %fragment(SWIG_Traits_frag(std::stack< _Tp*, _Sequence >), "header",
	      fragment=SWIG_Traits_frag(_Tp),
	      fragment="StdStackTraits") {
      namespace swig {
	template <>  struct traits<std::stack< _Tp*, _Sequence > > {
	  typedef value_category category;
	  static const char* type_name() {
	    return "std::stack<" #_Tp "," #_Sequence " * >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_STACK, std::stack< _Tp*, _Sequence >);

#ifdef %swig_stack_methods_val
    // Add swig/language extra methods
    %swig_stack_methods_val(std::stack< _Tp*, _Sequence >);
#endif

    %std_stack_methods_val(stack);
  };

}

