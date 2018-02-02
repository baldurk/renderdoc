/* -----------------------------------------------------------------------------
 * swigarch.i
 *
 * SWIG library file for 32bit/64bit code specialization and checking.
 *
 * Use only in extreme cases, when no arch. independent code can be
 * generated
 * 
 * To activate architecture specific code, use
 *
 *     swig -DSWIGWORDSIZE32
 *
 * or
 *
 *     swig -DSWIGWORDSIZE64
 *
 * Note that extra checking code will be added to the wrapped code,
 * which will prevent the compilation in a different architecture.
 *
 * If you don't specify the SWIGWORDSIZE (the default case), swig will
 * generate architecture independent and/or 32bits code, with no extra
 * checking code added.
 * ----------------------------------------------------------------------------- */

#if !defined(SWIGWORDSIZE32) &&  !defined(SWIGWORDSIZE64)
# if (__WORDSIZE == 32)
#  define SWIGWORDSIZE32
# endif
#endif
  
#if !defined(SWIGWORDSIZE64) &&  !defined(SWIGWORDSIZE32) 
# if defined(__x86_64) || defined(__x86_64__) || (__WORDSIZE == 64)
#  define SWIGWORDSIZE64
# endif
#endif


#ifdef SWIGWORDSIZE32
%{
#define SWIGWORDSIZE32
#ifndef LONG_MAX
#include <limits.h>
#endif
#if (__WORDSIZE == 64) || (LONG_MAX != INT_MAX)
# error "SWIG wrapped code invalid in 64 bit architecture, regenerate code using -DSWIGWORDSIZE64"
#endif
%}
#endif

#ifdef SWIGWORDSIZE64
%{
#define SWIGWORDSIZE64
#ifndef LONG_MAX
#include <limits.h>
#endif
#if (__WORDSIZE == 32) || (LONG_MAX == INT_MAX)
# error "SWIG wrapped code invalid in 32 bit architecture, regenerate code using -DSWIGWORDSIZE32"
#endif
%}
#endif
  

