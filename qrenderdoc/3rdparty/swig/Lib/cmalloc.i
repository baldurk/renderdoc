/* -----------------------------------------------------------------------------
 * cmalloc.i
 *
 * SWIG library file containing macros that can be used to create objects using
 * the C malloc function.
 * ----------------------------------------------------------------------------- */

%{
#include <stdlib.h>
%}

/* %malloc(TYPE [, NAME = TYPE])
   %calloc(TYPE [, NAME = TYPE])
   %realloc(TYPE [, NAME = TYPE])
   %free(TYPE [, NAME = TYPE])
   %allocators(TYPE [,NAME = TYPE])

   Creates functions for allocating/reallocating memory.

   TYPE *malloc_NAME(int nbytes = sizeof(TYPE);
   TYPE *calloc_NAME(int nobj=1, int size=sizeof(TYPE));
   TYPE *realloc_NAME(TYPE *ptr, int nbytes);
   void free_NAME(TYPE *ptr);

*/

%define %malloc(TYPE,NAME...)
#if #NAME != ""
%rename(malloc_##NAME) ::malloc(int nbytes);
#else
%rename(malloc_##TYPE) ::malloc(int nbytes);
#endif

#if #TYPE != "void"
%typemap(default) int nbytes "$1 = (int) sizeof(TYPE);"
#endif
TYPE *malloc(int nbytes);
%typemap(default) int nbytes;
%enddef

%define %calloc(TYPE,NAME...)
#if #NAME != ""
%rename(calloc_##NAME) ::calloc(int nobj, int sz);
#else
%rename(calloc_##TYPE) ::calloc(int nobj, int sz);
#endif
#if #TYPE != "void"
%typemap(default) int sz "$1 = (int) sizeof(TYPE);"
#else
%typemap(default) int sz "$1 = 1;"
#endif
%typemap(default) int nobj "$1 = 1;"
TYPE *calloc(int nobj, int sz);
%typemap(default) int sz;
%typemap(default) int nobj;
%enddef

%define %realloc(TYPE,NAME...)
%insert("header") {
#if #NAME != ""
TYPE *realloc_##NAME(TYPE *ptr, int nitems)
#else
TYPE *realloc_##TYPE(TYPE *ptr, int nitems)
#endif
{
#if #TYPE != "void"
return (TYPE *) realloc(ptr, nitems*sizeof(TYPE));
#else
return (TYPE *) realloc(ptr, nitems);
#endif
}
}
#if #NAME != ""
TYPE *realloc_##NAME(TYPE *ptr, int nitems);
#else
TYPE *realloc_##TYPE(TYPE *ptr, int nitems);
#endif
%enddef

%define %free(TYPE,NAME...)
#if #NAME != ""
%rename(free_##NAME) ::free(TYPE *ptr);
#else
%rename(free_##TYPE) ::free(TYPE *ptr);
#endif
void free(TYPE *ptr);
%enddef

%define %sizeof(TYPE,NAME...)
#if #NAME != ""
%constant int sizeof_##NAME = sizeof(TYPE);
#else
%constant int sizeof_##TYPE = sizeof(TYPE);
#endif
%enddef

%define %allocators(TYPE,NAME...)
%malloc(TYPE,NAME)
%calloc(TYPE,NAME)
%realloc(TYPE,NAME)
%free(TYPE,NAME)
#if #TYPE != "void"
%sizeof(TYPE,NAME)
#endif
%enddef





