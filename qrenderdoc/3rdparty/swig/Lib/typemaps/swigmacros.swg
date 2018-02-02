/* -----------------------------------------------------------------------------
 * SWIG API. Portion only visible from SWIG
 * ----------------------------------------------------------------------------- */
/*
  This file implements the internal macros of the 'SWIG API', which
  are useful to implement all the SWIG target languages.

  Basic preprocessor macros:
  --------------------------

    %arg(Arg)               Safe argument wrap
    %str(Arg)               Stringify the argument
    %begin_block            Begin an execution block
    %end_block              End an execution block
    %block(Block)           Execute Block as an execution block
    %define_as(Def, Val)    Define 'Def' as 'Val', expanding Def and Val first
    %ifcplusplus(V1, V2)    if C++ Mode; then V1; else V2; fi


  Casting Operations:
  -------------------

  SWIG provides the following casting macros, which implement the
  corresponding C++ casting operations:

    %const_cast(a, Type)         const_cast<Type >(a)
    %static_cast(a, Type)      	 static_cast<Type >(a)
    %reinterpret_cast(a, Type) 	 reinterpret_cast<Type >(a)
    %numeric_cast(a, Type)     	 static_cast<Type >(a)
    %as_voidptr(a)            	 const_cast<void *>(static_cast<const void *>(a))
    %as_voidptrptr(a)        	 reinterpret_cast<void **>(a)

  or their C unsafe versions. In C++ we use the safe version unless
  SWIG_NO_CPLUSPLUS_CAST is defined (usually via the -nocppcast swig flag).


  Memory allocation:
  ------------------

  These allocation/freeing macros are safe to use in C or C++ and
  dispatch the proper new/delete/delete[] or free/malloc calls as
  needed.

    %new_instance(Type)             Allocate a new instance of given Type
    %new_copy(value,Type)           Allocate and initialize a new instance with 'value'
    %new_array(size,Type)           Allocate a new array with given size and Type and zero initialize
    %new_copy_array(cptr,size,Type) Allocate and initialize a new array from 'cptr'
    %delete(cptr)                   Delete an instance
    %delete_array(cptr)             Delete an array


  Auxiliary loop macros:
  ----------------------

    %formacro(Macro, Args...) or %formacro_1(Macro, Args...)
       for i in Args
       do
          Macro($i)
       done

    %formacro_2(Macro2, Args...)
       for i,j in Args
       do
          Macro2($i, $j)
       done


  Flags and conditional macros:
  -----------------------------

     %mark_flag(flag)
       flag := True

     %evalif(flag,expr)
       if flag; then
         expr
       fi

     %evalif_2(flag1 flag2,expr)
       if flag1 and flag2; then
         expr
       fi


*/
/* -----------------------------------------------------------------------------
 * Basic preprocessor macros
 * ----------------------------------------------------------------------------- */

#define %arg(Arg...)        Arg
#define %str(Arg)           `Arg`
#ifndef %begin_block
#  define %begin_block      do {
#endif
#ifndef %end_block
#  define %end_block        } while(0)
#endif
#define %block(Block...)    %begin_block Block; %end_block

/* define a new macro */
%define %define_as(Def, Val...)%#define Def Val %enddef

/* include C++ or else value */
%define %ifcplusplus(cppval, nocppval)
#ifdef __cplusplus
cppval
#else
nocppval
#endif
%enddef

/* insert the SWIGVERSION in the interface and the wrapper code */
#if SWIG_VERSION
%insert("header") {
%define_as(SWIGVERSION,  SWIG_VERSION)
%#define SWIG_VERSION SWIGVERSION
}
#endif



/* -----------------------------------------------------------------------------
 * Casting operators
 * ----------------------------------------------------------------------------- */

#if defined(SWIG_NO_CPLUSPLUS_CAST)
/* Disable 'modern' cplusplus casting operators */
# if defined(SWIG_CPLUSPLUS_CAST)
#   undef SWIG_CPLUSPLUS_CAST
# endif
#endif

#if defined(__cplusplus) && defined(SWIG_CPLUSPLUS_CAST)
# define %const_cast(a,Type...)       const_cast< Type >(a)
# define %static_cast(a,Type...)      static_cast< Type >(a)
# define %reinterpret_cast(a,Type...) reinterpret_cast< Type >(a)
# define %numeric_cast(a,Type...)     static_cast< Type >(a)
#else /* C case */
# define %const_cast(a,Type...)       (Type)(a)
# define %static_cast(a,Type...)      (Type)(a)
# define %reinterpret_cast(a,Type...) (Type)(a)
# define %numeric_cast(a,Type...)     (Type)(a)
#endif /* __cplusplus */


#define %as_voidptr(a)               SWIG_as_voidptr(a)
#define %as_voidptrptr(a)            SWIG_as_voidptrptr(a)

%insert("header") {
%define_as(SWIG_as_voidptr(a),    %const_cast(%static_cast(a,const void *), void *))
%define_as(SWIG_as_voidptrptr(a), ((void)%as_voidptr(*a),%reinterpret_cast(a, void**)))
}


/* -----------------------------------------------------------------------------
 * Allocating/freeing elements
 * ----------------------------------------------------------------------------- */

#if defined(__cplusplus)
# define %new_instance(Type...)             (new Type())
# define %new_copy(val,Type...)             (new Type(%static_cast(val, const Type&)))
# define %new_array(size,Type...)           (new Type[size]())
# define %new_copy_array(ptr,size,Type...)  %reinterpret_cast(memcpy(new Type[size], ptr, sizeof(Type)*(size)), Type*)
# define %delete(cptr)                      delete cptr
# define %delete_array(cptr)                delete[] cptr
#else /* C case */
# define %new_instance(Type...)             (Type *)calloc(1,sizeof(Type))
# define %new_copy(val,Type...)             (Type *)memcpy(%new_instance(Type),&val,sizeof(Type))
# define %new_array(size,Type...)           (Type *)calloc(size, sizeof(Type))
# define %new_copy_array(ptr,size,Type...)  (Type *)memcpy(malloc((size)*sizeof(Type)), ptr, sizeof(Type)*(size))
# define %delete(cptr)                      free((char*)cptr)
# define %delete_array(cptr)                free((char*)cptr)
#endif /* __cplusplus */

/* -----------------------------------------------------------------------------
 * SWIG names and mangling
 * ----------------------------------------------------------------------------- */

#define %mangle(Type...)                  #@Type
#define %descriptor(Type...)               SWIGTYPE_ ## #@Type
#define %string_name(Name)                "SWIG_" %str(Name)
#define %symbol_name(Name, Type...)       SWIG_ ## Name ## _ #@Type
#define %checkcode(Code)           	  SWIG_TYPECHECK_ ## Code


/* -----------------------------------------------------------------------------
 * Auxiliary loop macros
 * ----------------------------------------------------------------------------- */


/* for loop for macro with one argument */
%define %_formacro_1(macro, arg1,...)macro(arg1)
#if #__VA_ARGS__ != "__fordone__"
%_formacro_1(macro, __VA_ARGS__)
#endif
%enddef

/* for loop for macro with one argument */
%define %formacro_1(macro,...)%_formacro_1(macro,__VA_ARGS__,__fordone__)%enddef
%define %formacro(macro,...)%_formacro_1(macro,__VA_ARGS__,__fordone__)%enddef

/* for loop for macro with two arguments */
%define %_formacro_2(macro, arg1, arg2, ...)macro(arg1, arg2)
#if #__VA_ARGS__ != "__fordone__"
%_formacro_2(macro, __VA_ARGS__)
#endif
%enddef

/* for loop for macro with two arguments */
%define %formacro_2(macro,...)%_formacro_2(macro, __VA_ARGS__, __fordone__)%enddef

/* -----------------------------------------------------------------------------
 * SWIG flags
 * ----------------------------------------------------------------------------- */

/*
  mark a flag, ie, define a macro name but ignore it in
  the interface.

  the flag can be later used with %evalif
*/

%define %mark_flag(x) %define x 1 %enddef %enddef


/*
  %evalif and %evalif_2 are use to evaluate or process
  an expression if the given predicate is 'true' (1).
*/
%define %_evalif(_x,_expr)
#if _x == 1
_expr
#endif
%enddef

%define %_evalif_2(_x,_y,_expr)
#if _x == 1 && _y == 1
_expr
#endif
%enddef

%define %evalif(_x,_expr...) %_evalif(%arg(_x),%arg(_expr)) %enddef

%define %evalif_2(_x,_y,_expr...) %_evalif_2(%arg(_x),%arg(_y),%arg(_expr)) %enddef

