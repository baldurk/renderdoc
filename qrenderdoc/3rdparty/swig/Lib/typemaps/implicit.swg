/*
  The %implicit macro allows a SwigType (Class) to be accepted
  as an input parameter and use its implicit constructors when needed.

  For example:


  %implicit(A, int, double, B);

  %inline 
  {
    struct B { };  
    struct A
    {
      int ii;
      A(int i) { ii = 1; }
      A(double d) { ii = 2; }
      A(const B& b) { ii = 3; }
    };
  
    int get(A a) { return a.ii; }
  }

  Here, you can call 'get' as 

    get(1)    ==> get(A(1))
    get(2.0)  ==> get(A(2.0))
    get(B())  ==> get(A(B()))

   and swig will construct an 'A' temporal variable using the
   corresponding implicit constructor.


  The plain implicit macro takes care of simple type list. If it doesn't
  work because you are passing template types with commas, then use
  the %implicit_{1,2,3} versions and/or the %arg macro.

*/

%define %implicit_type(Type...)
%traits_swigtype(Type);
%enddef

%define %implicit_frag(Type...) ,fragment=SWIG_Traits_frag(Type) %enddef

%define %implicit_code(Type...)
{
  Type _v;
  int res = swig::asval<Type >(obj, &_v);  
  if (SWIG_IsOK(res)) {
    if (val) *val = new value_type(static_cast<const Type& >(_v));
    return SWIG_AddNewMask(res);
  }
}
%enddef

/* implicit */

%define %implicit(Type, ...)

%formacro_1(%implicit_type,__VA_ARGS__);

%fragment(SWIG_Traits_frag(Type),"header",
	  fragment="StdTraits"
          %formacro_1(%implicit_frag,__VA_ARGS__)) %{
namespace swig {
  template <>  struct traits<Type > {   
    typedef pointer_category category;
    static const char* type_name() { return "Type"; }
  };
   
  template <> struct traits_asptr< Type > {
  typedef Type value_type;
  static int asptr(SWIG_Object obj, value_type **val) { 
    Type *vptr;
    static swig_type_info* descriptor = SWIG_TypeQuery("Type *");
    int res = descriptor ? SWIG_ConvertPtr(obj, (void **)&vptr, descriptor, 0) : SWIG_ERROR;
    if (SWIG_IsOK(res)) {
      if (val) *val = vptr;
      return res;
    } else {
      %formacro_1(%implicit_code,__VA_ARGS__)
    }
    return SWIG_TypeError;
  }
 };
}
%}

%typemap_traits_ptr(%checkcode(POINTER),Type);
%enddef

/* implicit_1 */


%define %implicit_1(Type, Imp1)
%traits_swigtype(Imp1);

%fragment(SWIG_Traits_frag(Type),"header",
	  fragment="StdTraits",
	  fragment=SWIG_Traits_frag(Imp1)) %{
namespace swig {
  template <>  struct traits< Type > {   
    typedef pointer_category category;
    static const char* type_name() { return "Type"; }
  };
   
  template <> struct traits_asptr< Type > {   
  typedef Type value_type;
  static int asptr(SWIG_Object obj, value_type **val) { 
    Type *vptr;
    static swig_type_info* descriptor = SWIG_TypeQuery("Type *");
    int res = descriptor ? SWIG_ConvertPtr(obj, (void **)&vptr, descriptor, 0) : SWIG_ERROR;
    if (SWIG_IsOK(res)) {
      if (val) *val = vptr;
      return res;
    } else {
      %implicit_code(Imp1);
    }
    return SWIG_TypeError;
  }
 };
}
%}

%typemap_traits_ptr(%checkcode(POINTER),Type);

%enddef

/* implicit_2 */

%define %implicit_2(Type, Imp1, Imp2)
%traits_swigtype(Imp1);
%traits_swigtype(Imp2);

%fragment(SWIG_Traits_frag(Type),"header",
	  fragment="StdTraits",
	  fragment=SWIG_Traits_frag(Imp1),
	  fragment=SWIG_Traits_frag(Imp2)) %{
namespace swig {
  template <>  struct traits< Type > {   
    typedef pointer_category category;
    static const char* type_name() { return "Type"; }
  };

  template <> struct traits_asptr< Type > {   
  typedef Type value_type;
  static int asptr(SWIG_Object obj, value_type **val) { 
    Type *vptr;
    static swig_type_info* descriptor = SWIG_TypeQuery("Type *");
    int res = descriptor ? SWIG_ConvertPtr(obj, (void **)&vptr, descriptor, 0) : SWIG_ERROR;
    if (SWIG_IsOK(res)) {
      if (val) *val = vptr;
      return SWIG_OLDOBJ;
    } else {
      %implicit_code(Imp1);
      %implicit_code(Imp2);
    }
    return SWIG_TypeError;
  }
 };
}
%}

%typemap_traits_ptr(%checkcode(POINTER),Type);
%enddef


/* implicit_3 */

%define %implicit_3(Type, Imp1, Imp2, Imp3)
%traits_swigtype(Imp1);
%traits_swigtype(Imp2);
%traits_swigtype(Imp3);

%fragment(SWIG_Traits_frag(Type),"header",
	  fragment="StdTraits",
	  fragment=SWIG_Traits_frag(Imp1),
	  fragment=SWIG_Traits_frag(Imp2),
	  fragment=SWIG_Traits_frag(Imp3)) %{
namespace swig {
  template <>  struct traits< Type > {   
    typedef pointer_category category;
    static const char* type_name() { return "Type"; }
  };

  template <> struct traits_asptr< Type > {   
    typedef Type value_type;
    static int asptr(SWIG_Object obj, value_type **val) { 
    Type *vptr;
    static swig_type_info* descriptor = SWIG_TypeQuery("Type *");
    int res = descriptor ? SWIG_ConvertPtr(obj, (void **)&vptr, descriptor, 0) : SWIG_ERROR;
    if (SWIG_IsOK(res)) {
      if (val) *val = vptr;
      return res;
    } else {
      %implicit_code(Imp1);
      %implicit_code(Imp2);
      %implicit_code(Imp3);
    }
    return SWIG_TypeError;
  }
 };
}
%}

%typemap_traits_ptr(%checkcode(POINTER),Type);
%enddef
