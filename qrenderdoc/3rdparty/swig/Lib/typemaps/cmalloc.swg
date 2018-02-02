/* -----------------------------------------------------------------------------
 * cmalloc.swg
 *
 * This library file contains macros that can be used to create objects using
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

   TYPE *malloc_NAME(size_t nbytes = sizeof(TYPE);
   TYPE *calloc_NAME(size_t nobj=1, size_t size=sizeof(TYPE));
   TYPE *realloc_NAME(TYPE *ptr, size_t nbytes);
   void free_NAME(TYPE *ptr);

*/

%define %malloc(TYPE,NAME...)
#if #NAME != ""
%rename(malloc_##NAME) ::malloc(size_t nbytes);
#else
%rename(malloc_##TYPE) ::malloc(size_t nbytes);
#endif

#if #TYPE != "void"
%typemap(default) size_t nbytes "$1 = (size_t) sizeof(TYPE);"
#endif
TYPE *malloc(size_t nbytes);
%typemap(default) size_t nbytes;
%enddef

%define %calloc(TYPE,NAME...)
#if #NAME != ""
%rename(calloc_##NAME) ::calloc(size_t nobj, size_t sz);
#else
%rename(calloc_##TYPE) ::calloc(size_t nobj, size_t sz);
#endif
#if #TYPE != "void"
%typemap(default) size_t sz "$1 = (size_t) sizeof(TYPE);"
#else
%typemap(default) size_t sz "$1 = 1;"
#endif
%typemap(default) size_t nobj "$1 = 1;"
TYPE *calloc(size_t nobj, size_t sz);
%typemap(default) size_t sz;
%typemap(default) size_t nobj;
%enddef

%define %realloc(TYPE,NAME...)
%insert("header") {
#if #NAME != ""
TYPE *realloc_##NAME(TYPE *ptr, size_t nitems)
#else
TYPE *realloc_##TYPE(TYPE *ptr, size_t nitems)
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
TYPE *realloc_##NAME(TYPE *ptr, size_t nitems);
#else
TYPE *realloc_##TYPE(TYPE *ptr, size_t nitems);
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
%constant size_t sizeof_##NAME = sizeof(TYPE);
#else
%constant size_t sizeof_##TYPE = sizeof(TYPE);
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





