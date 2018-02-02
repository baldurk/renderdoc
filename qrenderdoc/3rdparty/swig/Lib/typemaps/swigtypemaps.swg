/* -----------------------------------------------------------------------------
 * swigtypemaps.swg
 *
 * Unified Typemap Library frontend
 * ----------------------------------------------------------------------------- */

/*
  This file provides the frontend to the Unified Typemap Library.

  When using this library in a SWIG target language, you need to
  define a minimum set of fragments, specialize a couple of macros,
  and then include this file.

  Typically you will create a 'mytypemaps.swg' file in each target
  language, where you will have the following sections:

    === mytypemaps.swg ===

    //  Fragment section 
    %include <typemaps/fragments.swg>
    <include target language fragments>

    // Unified typemap section 
    <specialized the typemap library macros>
    %include <typemaps/swigtypemaps.swg>

    // Local typemap section 
    <add/replace extra target language typemaps>

    === mytypemaps.swg ===

  While we add more docs, please take a look at the following cases
  to see how you specialized the unified typemap library for a new
  target language:

      Lib/python/pytypemaps.swg
      Lib/tcl/tcltypemaps.swg
      Lib/ruby/rubytypemaps.swg
      Lib/perl5/perltypemaps.swg
    
*/

#define SWIGUTL SWIGUTL

/* -----------------------------------------------------------------------------
 *   Language specialization section. 
 *
 *   Tune these macros for each language as needed.
 * ----------------------------------------------------------------------------- */

/*
  The SWIG target language object must be provided.
  For example in python you define:

    #define SWIG_Object PyObject *
*/

#if !defined(SWIG_Object) 
#error "SWIG_Object must be defined as the SWIG target language object"
#endif

/*==== flags for new/convert methods ====*/


#ifndef %convertptr_flags
%define %convertptr_flags  0 %enddef
#endif

#ifndef %newpointer_flags
%define %newpointer_flags  0 %enddef
#endif

#ifndef %newinstance_flags
%define %newinstance_flags 0 %enddef
#endif

/*==== set output ====*/

#ifndef %set_output
/* simple set output operation */
#define %set_output(obj)                  $result = obj
#endif

/*==== set variable output  ====*/

#ifndef %set_varoutput
/* simple set varoutput operation */
#define %set_varoutput(obj)               $result = obj
#endif

/*==== append output ====*/

#ifndef %append_output
#if defined(SWIG_AppendOutput)
/* simple append operation */
#define %append_output(obj)               $result = SWIG_AppendOutput($result,obj) 
#else
#error "Language must define SWIG_AppendOutput or %append_output"
#endif
#endif

/*==== set constant ====*/

#ifndef %set_constant
#if defined(SWIG_SetConstant)
/* simple set constant operation */
#define %set_constant(name,value)         SWIG_SetConstant(name,value)
#else
#error "Language must define SWIG_SetConstant or %set_constant"
#endif
#endif

/*==== raise an exception ====*/

#ifndef %raise
#if defined(SWIG_Raise)
/* simple raise operation */
#define %raise(obj, type, desc)           SWIG_Raise(obj, type, desc); SWIG_fail
#else
#error "Language must define SWIG_Raise or %raise"
#endif
#endif

/*==== director output exception ====*/

#if defined(SWIG_DIRECTOR_TYPEMAPS)
#ifndef SWIG_DirOutFail
#define SWIG_DirOutFail(code, msg)        Swig::DirectorTypeMismatchException::raise(SWIG_ErrorType(code), msg)
#endif
#endif


/* -----------------------------------------------------------------------------
 *  Language independent definitions
 * ----------------------------------------------------------------------------- */

#define %error_block(Block...)                 %block(Block)
#define %default_code(code)                    SWIG_ArgError(code)  
#define %argument_fail(code, type, name, argn) SWIG_exception_fail(%default_code(code), %argfail_fmt(type, name, argn))
#define %argument_nullref(type, name, argn)    SWIG_exception_fail(SWIG_ValueError, %argnullref_fmt(type, name, argn))
#define %variable_fail(code, type, name)       SWIG_exception_fail(%default_code(code), %varfail_fmt(type, name))
#define %variable_nullref(type, name)          SWIG_exception_fail(SWIG_ValueError, %varnullref_fmt(type, name))

#if defined(SWIG_DIRECTOR_TYPEMAPS)
#define %dirout_fail(code, type)          SWIG_DirOutFail(%default_code(code), %outfail_fmt(type))
#define %dirout_nullref(type)             SWIG_DirOutFail(SWIG_ValueError, %outnullref_fmt(type))
#endif

/* -----------------------------------------------------------------------------
 *  All the typemaps
 * ----------------------------------------------------------------------------- */


%include <typemaps/fragments.swg>
%include <typemaps/exception.swg>
%include <typemaps/swigtype.swg>
%include <typemaps/void.swg>
%include <typemaps/swigobject.swg>
%include <typemaps/valtypes.swg>
%include <typemaps/ptrtypes.swg>
%include <typemaps/inoutlist.swg>
%include <typemaps/primtypes.swg>
%include <typemaps/string.swg>
%include <typemaps/misctypes.swg>
%include <typemaps/enumint.swg>


