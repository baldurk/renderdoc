/*
  Fragments
  =========
  See the "Typemap fragments" section in the documentation for understanding
  fragments. Below is some info on how fragments and automatic type
  specialization is used.

  Macros that make the automatic generation of typemaps easier are provided.

  Consider the following code:

      %fragment(SWIG_From_frag(bool), "header") {     
      static PyObject*		      
      SWIG_From_dec(bool)(bool value)	       
      {					       
        PyObject *obj = value ? Py_True : Py_False;  
        Py_INCREF(obj);			       
        return obj;				       
      }					       
      }					       
      					 
      %typemap(out, fragment=SWIG_From_frag(bool)) bool {
        $result = SWIG_From(bool)($1));
      }

  Here the macros

      SWIG_From_frag  => fragment 
      SWIG_From_dec   => declaration 
      SWIG_From       => call 
      
  allow you to define/include a fragment, and declare and call the
  'from-bool' method as needed. In the simpler case, these macros 
  just return something like

      SWIG_From_frag(bool)  => "SWIG_From_bool"
      SWIG_From_dec(bool)   =>  SWIG_From_bool
      SWIG_From(bool)       =>  SWIG_From_bool

  But they are specialized for the different languages requirements,
  such as perl or tcl that requires passing the interpreter pointer,
  and also they can manage C++ ugly types, for example:
  
      SWIG_From_frag(std::complex<double>)  => "SWIG_From_std_complex_Sl_double_Sg_"
      SWIG_From_dec(std::complex<double>)   =>  SWIG_From_std_complex_Sl_double_Sg_
      SWIG_From(std::complex<double>)       =>  SWIG_From_std_complex_Sl_double_Sg_


  Hence, to declare methods to use with typemaps, always use the
  SWIG_From* macros. In the same way, the SWIG_AsVal* and SWIG_AsPtr*
  set of macros are provided.
    
*/


/* -----------------------------------------------------------------------------
 * Define the basic macros to 'normalize' the type fragments
 * ----------------------------------------------------------------------------- */

#ifndef SWIG_AS_DECL_ARGS
#define SWIG_AS_DECL_ARGS
#endif

#ifndef SWIG_FROM_DECL_ARGS
#define SWIG_FROM_DECL_ARGS
#endif

#ifndef SWIG_AS_CALL_ARGS
#define SWIG_AS_CALL_ARGS
#endif

#ifndef SWIG_FROM_CALL_ARGS
#define SWIG_FROM_CALL_ARGS
#endif

#define %fragment_name(Name, Type...)     %string_name(Name) "_" {Type}

#define SWIG_Traits_frag(Type...) %fragment_name(Traits, Type) 
#define SWIG_AsPtr_frag(Type...)  %fragment_name(AsPtr, Type)	 
#define SWIG_AsVal_frag(Type...)  %fragment_name(AsVal, Type)	 
#define SWIG_From_frag(Type...)   %fragment_name(From, Type)	 

#define SWIG_AsVal_name(Type...)  %symbol_name(AsVal, Type) 
#define SWIG_AsPtr_name(Type...)  %symbol_name(AsPtr, Type) 
#define SWIG_From_name(Type...)   %symbol_name(From, Type)  

#define SWIG_AsVal_dec(Type...)   SWIG_AsVal_name(Type) SWIG_AS_DECL_ARGS
#define SWIG_AsPtr_dec(Type...)   SWIG_AsPtr_name(Type) SWIG_AS_DECL_ARGS
#define SWIG_From_dec(Type...)    SWIG_From_name(Type)  SWIG_FROM_DECL_ARGS 

#define SWIG_AsVal(Type...)       SWIG_AsVal_name(Type) SWIG_AS_CALL_ARGS 
#define SWIG_AsPtr(Type...)  	  SWIG_AsPtr_name(Type) SWIG_AS_CALL_ARGS 	 
#define SWIG_From(Type...)   	  SWIG_From_name(Type)  SWIG_FROM_CALL_ARGS 

/* ------------------------------------------------------------
 * common fragments 
 * ------------------------------------------------------------ */

/* Default compiler options for gcc allow long_long but not LLONG_MAX. 
 * Define SWIG_NO_LLONG_MAX if this added limits support is not wanted. */
%fragment("<limits.h>","header") %{
#include <limits.h>
#if !defined(SWIG_NO_LLONG_MAX)
# if !defined(LLONG_MAX) && defined(__GNUC__) && defined (__LONG_LONG_MAX__)
#   define LLONG_MAX __LONG_LONG_MAX__
#   define LLONG_MIN (-LLONG_MAX - 1LL)
#   define ULLONG_MAX (LLONG_MAX * 2ULL + 1ULL)
# endif
#endif
%}

%fragment("<math.h>","header") %{
#include <math.h>
%}

%fragment("<wchar.h>","header") %{
#include <wchar.h>
#include <limits.h>
#ifndef WCHAR_MIN
#  define WCHAR_MIN 0
#endif
#ifndef WCHAR_MAX
#  define WCHAR_MAX 65535
#endif
%}

%fragment("<float.h>","header") %{
#include <float.h>
%}

%fragment("<stdio.h>","header") %{
#include <stdio.h>
#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(_WATCOM)
# ifndef snprintf
#  define snprintf _snprintf
# endif
#endif
%}

%fragment("<stdlib.h>","header") %{
#include <stdlib.h>
#ifdef _MSC_VER
# ifndef strtoull
#  define strtoull _strtoui64
# endif
# ifndef strtoll
#  define strtoll _strtoi64
# endif
#endif
%}

%fragment("<stddef.h>", "header") %{
#include <stddef.h>
%}

%fragment("<string>", "header") %{
#include <string>
%}

%fragment("<stdexcept>", "header") %{
#include <stdexcept>
%}

%fragment("<algorithm>", "header") %{
#include <algorithm>
%}

%fragment("SWIG_isfinite","header",fragment="<math.h>,<float.h>") %{
/* Getting isfinite working pre C99 across multiple platforms is non-trivial. Users can provide SWIG_isfinite on older platforms. */
#ifndef SWIG_isfinite
/* isfinite() is a macro for C99 */
# if defined(isfinite)
#  define SWIG_isfinite(X) (isfinite(X))
# elif defined __cplusplus && __cplusplus >= 201103L
/* Use a template so that this works whether isfinite() is std::isfinite() or
 * in the global namespace.  The reality seems to vary between compiler
 * versions.
 *
 * Make sure namespace std exists to avoid compiler warnings.
 *
 * extern "C++" is required as this fragment can end up inside an extern "C" { } block
 */
namespace std { }
extern "C++" template<typename T>
inline int SWIG_isfinite_func(T x) {
  using namespace std;
  return isfinite(x);
}
#  define SWIG_isfinite(X) (SWIG_isfinite_func(X))
# elif defined(_MSC_VER)
#  define SWIG_isfinite(X) (_finite(X))
# elif defined(__sun) && defined(__SVR4)
#  include <ieeefp.h>
#  define SWIG_isfinite(X) (finite(X))
# endif
#endif
%}

%fragment("SWIG_Float_Overflow_Check","header",fragment="<float.h>,SWIG_isfinite") %{
/* Accept infinite as a valid float value unless we are unable to check if a value is finite */
#ifdef SWIG_isfinite
# define SWIG_Float_Overflow_Check(X) ((X < -FLT_MAX || X > FLT_MAX) && SWIG_isfinite(X))
#else
# define SWIG_Float_Overflow_Check(X) ((X < -FLT_MAX || X > FLT_MAX))
#endif
%}

/* -----------------------------------------------------------------------------
 * special macros for fragments
 * ----------------------------------------------------------------------------- */

/* Macros to derive numeric types */

%define %numeric_type_from(Type, Base)
%fragment(SWIG_From_frag(Type),"header",
	  fragment=SWIG_From_frag(Base)) {
SWIGINTERNINLINE SWIG_Object
SWIG_From_dec(Type)(Type value)
{    
  return SWIG_From(Base)(value);
}
}
%enddef

%define %numeric_type_asval(Type, Base, Frag, OverflowCond)
%fragment(SWIG_AsVal_frag(Type),"header",
	  fragment=Frag,
	  fragment=SWIG_AsVal_frag(Base)) {
SWIGINTERN int
SWIG_AsVal_dec(Type)(SWIG_Object obj, Type *val)
{
  Base v;
  int res = SWIG_AsVal(Base)(obj, &v);
  if (SWIG_IsOK(res)) {
    if (OverflowCond) {
      return SWIG_OverflowError;
    } else {
      if (val) *val = %numeric_cast(v, Type);
    }
  }  
  return res;
}
}
%enddef

#define %numeric_signed_type_asval(Type, Base, Frag, Min, Max) \
%numeric_type_asval(Type, Base, Frag, (v < Min || v > Max))

#define %numeric_unsigned_type_asval(Type, Base, Frag, Max) \
%numeric_type_asval(Type, Base, Frag, (v > Max))


/* Macro for 'signed long' derived types */

%define %numeric_slong(Type, Frag, Min, Max)
%numeric_type_from(Type, long)
%numeric_signed_type_asval(Type, long, Frag , Min, Max)
%enddef

/* Macro for 'unsigned long' derived types */

%define %numeric_ulong(Type, Frag, Max)
%numeric_type_from(Type, unsigned long)
%numeric_unsigned_type_asval(Type, unsigned long, Frag, Max)
%enddef


/* Macro for floating point derived types (original macro) */

%define %numeric_double(Type, Frag, Min, Max)
%numeric_type_from(Type, double)
%numeric_signed_type_asval(Type, double, Frag , Min, Max)
%enddef

/* Macro for floating point derived types */

%define %numeric_float(Type, Frag, OverflowCond)
%numeric_type_from(Type, double)
%numeric_type_asval(Type, double, Frag, OverflowCond)
%enddef


/* Macros for missing fragments */

%define %ensure_fragment(Fragment)
%fragment(`Fragment`,"header") {
%#error "SWIG language implementation must provide the Fragment fragment"
}
%enddef

%define %ensure_type_fragments(Type)
%fragment(SWIG_From_frag(Type),"header") {
%#error "SWIG language implementation must provide a SWIG_From_frag(Type) fragment"
}
%fragment(SWIG_AsVal_frag(Type),"header") {
%#error "SWIG language implementation must provide a SWIG_AsVal_frag(Type) fragment"
}
%enddef
