/* -----------------------------------------------------------------------------
 * carrays.swg
 *
 * This library file contains macros that can be used to manipulate simple
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
  static TYPE *new_##NAME(size_t nelements) { 
    return %new_array(nelements, TYPE);
  }

  static void delete_##NAME(TYPE *ary) {
    %delete_array(ary);
  }

  static TYPE NAME##_getitem(TYPE *ary, size_t index) {
    return ary[index];
  }
  static void NAME##_setitem(TYPE *ary, size_t index, TYPE value) {
    ary[index] = value;
  }
%}

TYPE *new_##NAME(size_t nelements);
void delete_##NAME(TYPE *ary);
TYPE NAME##_getitem(TYPE *ary, size_t index);
void NAME##_setitem(TYPE *ary, size_t index, TYPE value);

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
 * Use
 *
 *    %array_class_wrap(TYPE,NAME,GET,SET) 
 *
 * if you want  different names for the get/set methods.
 * ----------------------------------------------------------------------------- */

%define %array_class_wrap(TYPE,NAME,getitem,setitem)
%{
typedef TYPE NAME;
%}


typedef struct {
} NAME;

%extend NAME {

  NAME(size_t nelements) {
    return %new_array(nelements, TYPE);
  }

  ~NAME() {
    %delete_array(self);
  }
  
  TYPE getitem(size_t index) {
    return self[index];
  }

  void setitem(size_t index, TYPE value) {
    self[index] = value;
  }

  TYPE * cast() {
    return self;
  }

  static NAME *frompointer(TYPE *t) {
    return %static_cast(t, NAME *);
  }
};

%types(NAME = TYPE);

%enddef


#ifndef %array_class
%define %array_class(TYPE,NAME)
  %array_class_wrap(TYPE,NAME,getitem,setitem)
%enddef
#endif
