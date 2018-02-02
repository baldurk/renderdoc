
/* defining the String asptr/from methods */

%define %std_string_asptr(String, Char, SWIG_AsCharPtrAndSize, Frag)
%fragment(SWIG_AsPtr_frag(String),"header",fragment=Frag) {
SWIGINTERN int
SWIG_AsPtr_dec(String)(SWIG_Object obj, String **val) 
{
  Char* buf = 0 ; size_t size = 0; int alloc = SWIG_OLDOBJ;
  if (SWIG_IsOK((SWIG_AsCharPtrAndSize(obj, &buf, &size, &alloc)))) {
    if (buf) {
      if (val) *val = new String(buf, size - 1);
      if (alloc == SWIG_NEWOBJ) %delete_array(buf);
      return SWIG_NEWOBJ;
    } else {
      if (val) *val = 0;
      return SWIG_OLDOBJ;
    }
  } else {
    static int init = 0;
    static swig_type_info* descriptor = 0;
    if (!init) {
      descriptor = SWIG_TypeQuery(#String " *");
      init = 1;
    }
    if (descriptor) {
      String *vptr;
      int res = SWIG_ConvertPtr(obj, (void**)&vptr, descriptor, 0);
      if (SWIG_IsOK(res) && val) *val = vptr;
      return res;
    }
  }
  return SWIG_ERROR;
}
}
%enddef

%define %std_string_from(String, SWIG_FromCharPtrAndSize, Frag)
%fragment(SWIG_From_frag(String),"header",fragment=Frag) {
SWIGINTERNINLINE SWIG_Object
SWIG_From_dec(String)(const String& s)
{
  return SWIG_FromCharPtrAndSize(s.data(), s.size());
}
}
%enddef

%define %std_string_asval(String)
%fragment(SWIG_AsVal_frag(String),"header", fragment=SWIG_AsPtr_frag(String)) {
SWIGINTERN int
SWIG_AsVal_dec(String)(SWIG_Object obj, String *val)
{
  String* v = (String *) 0;
  int res = SWIG_AsPtr(String)(obj, &v);
  if (!SWIG_IsOK(res)) return res;
  if (v) {
    if (val) *val = *v;
    if (SWIG_IsNewObj(res)) {
      %delete(v);
      res = SWIG_DelNewMask(res);
    }
    return res;
  }
  return SWIG_ERROR;
}
}
%enddef


%define %typemaps_std_string(String, Char, AsPtrMethod, FromMethod, CheckCode)

%std_string_asptr(String, Char, AsPtrMethod, #AsPtrMethod)
%std_string_asval(String)
%std_string_from(String, FromMethod, #FromMethod)

%typemaps_asptrfromn(%arg(CheckCode), String);

%enddef
