/* 
   adding backward compatibility macros
*/

#define SWIG_arg(x...)     %arg(x)
#define SWIG_Mangle(x...)  %mangle(x)

#define SWIG_As_frag(Type...)      %fragment_name(As, Type)
#define SWIG_As_name(Type...)      %symbol_name(As, Type) 
#define SWIG_As(Type...)     	   SWIG_As_name(Type) SWIG_AS_CALL_ARGS 

#define SWIG_Check_frag(Type...)   %fragment_name(Check, Type)
#define SWIG_Check_name(Type...)   %symbol_name(Check, Type) 
#define SWIG_Check(Type...)        SWIG_Check_name(Type) SWIG_AS_CALL_ARGS 

%define %ascheck_methods(Code, Type...)
%fragment(SWIG_As_frag(Type),"header", fragment=SWIG_AsVal_frag(Type)) {
SWIGINTERNINLINE Type
SWIG_As(Type)(PyObject* obj)
{
  Type v;
  int res = SWIG_AsVal(Type)(obj, &v);
  if (!SWIG_IsOK(res)) {
    /*
      this is needed to make valgrind/purify happier. 
     */
    memset((void*)&v, 0, sizeof(Type));
    SWIG_Error(res, "");
  }
  return v;
}
}

%fragment(SWIG_Check_frag(Type),"header",fragment=SWIG_AsVal_frag(Type)) {
SWIGINTERNINLINE int
SWIG_Check(Type)(PyObject* obj)
{
  int res = SWIG_AsVal(Type)(obj, (Type*)0);
  return SWIG_IsOK(res);
}
}
%enddef

%apply_checkctypes(%ascheck_methods)

