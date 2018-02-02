// Allow for different namespaces for shared_ptr / intrusive_ptr - they could be boost or std or std::tr1
// For example for std::tr1, use:
// #define SWIG_SHARED_PTR_NAMESPACE std
// #define SWIG_SHARED_PTR_SUBNAMESPACE tr1
// #define SWIG_INTRUSIVE_PTR_NAMESPACE boost
// #define SWIG_INTRUSIVE_PTR_SUBNAMESPACE 

#if !defined(SWIG_INTRUSIVE_PTR_NAMESPACE)
# define SWIG_INTRUSIVE_PTR_NAMESPACE boost
#endif

#if defined(SWIG_INTRUSIVE_PTR_SUBNAMESPACE)
# define SWIG_INTRUSIVE_PTR_QNAMESPACE SWIG_INTRUSIVE_PTR_NAMESPACE::SWIG_INTRUSIVE_PTR_SUBNAMESPACE
#else
# define SWIG_INTRUSIVE_PTR_QNAMESPACE SWIG_INTRUSIVE_PTR_NAMESPACE
#endif

namespace SWIG_INTRUSIVE_PTR_NAMESPACE {
#if defined(SWIG_INTRUSIVE_PTR_SUBNAMESPACE)
  namespace SWIG_INTRUSIVE_PTR_SUBNAMESPACE {
#endif
    template <class T> class intrusive_ptr {
    };
#if defined(SWIG_INTRUSIVE_PTR_SUBNAMESPACE)
  }
#endif
}

%fragment("SWIG_intrusive_deleter", "header") {
template<class T> struct SWIG_intrusive_deleter {
    void operator()(T *p) {
        if (p) 
          intrusive_ptr_release(p);
    }
};
}

%fragment("SWIG_null_deleter", "header") {
struct SWIG_null_deleter {
  void operator() (void const *) const {
  }
};
%#define SWIG_NO_NULL_DELETER_0 , SWIG_null_deleter()
%#define SWIG_NO_NULL_DELETER_1
}

// Workaround empty first macro argument bug
#define SWIGEMPTYHACK
// Main user macro for defining intrusive_ptr typemaps for both const and non-const pointer types
%define %intrusive_ptr(TYPE...)
%feature("smartptr", noblock=1) TYPE { SWIG_SHARED_PTR_QNAMESPACE::shared_ptr< TYPE > }
SWIG_INTRUSIVE_PTR_TYPEMAPS(SWIGEMPTYHACK, TYPE)
SWIG_INTRUSIVE_PTR_TYPEMAPS(const, TYPE)
%enddef

%define %intrusive_ptr_no_wrap(TYPE...)
%feature("smartptr", noblock=1) TYPE { SWIG_SHARED_PTR_QNAMESPACE::shared_ptr< TYPE > }
SWIG_INTRUSIVE_PTR_TYPEMAPS_NO_WRAP(SWIGEMPTYHACK, TYPE)
SWIG_INTRUSIVE_PTR_TYPEMAPS_NO_WRAP(const, TYPE)
%enddef

// Legacy macros
%define SWIG_INTRUSIVE_PTR(PROXYCLASS, TYPE...)
#warning "SWIG_INTRUSIVE_PTR(PROXYCLASS, TYPE) is deprecated. Please use %intrusive_ptr(TYPE) instead."
%intrusive_ptr(TYPE)
%enddef

%define SWIG_INTRUSIVE_PTR_DERIVED(PROXYCLASS, BASECLASSTYPE, TYPE...)
#warning "SWIG_INTRUSIVE_PTR_DERIVED(PROXYCLASS, BASECLASSTYPE, TYPE) is deprecated. Please use %intrusive_ptr(TYPE) instead."
%intrusive_ptr(TYPE)
%enddef

%define SWIG_INTRUSIVE_PTR_NO_WRAP(PROXYCLASS, TYPE...)
#warning "SWIG_INTRUSIVE_PTR_NO_WRAP(PROXYCLASS, TYPE) is deprecated. Please use %intrusive_ptr_no_wrap(TYPE) instead."
%intrusive_ptr_no_wrap(TYPE)
%enddef

%define SWIG_INTRUSIVE_PTR_DERIVED_NO_WRAP(PROXYCLASS, BASECLASSTYPE, TYPE...)
#warning "SWIG_INTRUSIVE_PTR_DERIVED_NO_WRAP(PROXYCLASS, BASECLASSTYPE, TYPE) is deprecated. Please use %intrusive_ptr_no_wrap(TYPE) instead."
%intrusive_ptr_no_wrap(TYPE)
%enddef

