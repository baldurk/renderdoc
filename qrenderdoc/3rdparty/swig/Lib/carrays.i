/* -----------------------------------------------------------------------------
 * carrays.i
 *
 * SWIG library file containing macros that can be used to manipulate simple
 * pointers as arrays.
 * ----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * %array_functions(TYPE,NAME)
 *
 * Generates functions for creating and accessing elements of a C array
 * (as pointers).  Creates the following functions:
 *
 *        TYPE *new_NAME(int nelements)
 *        void delete_NAME(TYPE *);
 *        TYPE NAME_getitem(TYPE *, int index);
 *        void NAME_setitem(TYPE *, int index, TYPE value);
 * 
 * ----------------------------------------------------------------------------- */

%define %array_functions(TYPE,NAME)
%{
static TYPE *new_##NAME(int nelements) { %}
#ifdef __cplusplus
%{  return new TYPE[nelements](); %}
#else
%{  return (TYPE *) calloc(nelements,sizeof(TYPE)); %}
#endif
%{}

static void delete_##NAME(TYPE *ary) { %}
#ifdef __cplusplus
%{  delete [] ary; %}
#else
%{  free(ary); %}
#endif
%{}

static TYPE NAME##_getitem(TYPE *ary, int index) {
    return ary[index];
}
static void NAME##_setitem(TYPE *ary, int index, TYPE value) {
    ary[index] = value;
}
%}

TYPE *new_##NAME(int nelements);
void delete_##NAME(TYPE *ary);
TYPE NAME##_getitem(TYPE *ary, int index);
void NAME##_setitem(TYPE *ary, int index, TYPE value);

%enddef


/* -----------------------------------------------------------------------------
 * %array_class(TYPE,NAME)
 *
 * Generates a class wrapper around a C array.  The class has the following
 * interface:
 *
 *          struct NAME {
 *              NAME(int nelements);
 *             ~NAME();
 *              TYPE getitem(int index);
 *              void setitem(int index, TYPE value);
 *              TYPE * cast();
 *              static NAME *frompointer(TYPE *t);
  *         }
 *
 * ----------------------------------------------------------------------------- */

%define %array_class(TYPE,NAME)
%{
typedef TYPE NAME;
%}
typedef struct {
  /* Put language specific enhancements here */
} NAME;

%extend NAME {

#ifdef __cplusplus
NAME(int nelements) {
  return new TYPE[nelements]();
}
~NAME() {
  delete [] self;
}
#else
NAME(int nelements) {
  return (TYPE *) calloc(nelements,sizeof(TYPE));
}
~NAME() {
  free(self);
}
#endif

TYPE getitem(int index) {
  return self[index];
}
void setitem(int index, TYPE value) {
  self[index] = value;
}
TYPE * cast() {
  return self;
}
static NAME *frompointer(TYPE *t) {
  return (NAME *) t;
}

};

%types(NAME = TYPE);

%enddef

