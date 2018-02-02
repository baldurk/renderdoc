/* ------------------------------------------------------------
 * Void * -  Accepts any kind of pointer
 * ------------------------------------------------------------ */

/* in */

%typemap(in,noblock=1) void * (int res) {
  res = SWIG_ConvertPtr($input,%as_voidptrptr(&$1), 0, $disown);
  if (!SWIG_IsOK(res)) { 
    %argument_fail(res, "$type", $symname, $argnum); 
  }
}
%typemap(freearg) void * "";

%typemap(in,noblock=1) void * const& ($*ltype temp = 0, int res) {
  res = SWIG_ConvertPtr($input, %as_voidptrptr(&temp), 0, $disown);
  if (!SWIG_IsOK(res)) { 
    %argument_fail(res, "Stype", $symname, $argnum); 
  }
  $1 =  &temp;
}
%typemap(freearg) void * const& "";


/* out */

#if defined(VOID_Object)
%typemap(out,noblock=1) void { $result = VOID_Object; }
#else
%typemap(out,noblock=1) void {}
#endif

/* varin */

%typemap(varin) void * {
  void *temp = 0;
  int res = SWIG_ConvertPtr($input, &temp, 0, SWIG_POINTER_DISOWN);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  $1 = ($1_ltype) temp;
}

/* typecheck */

%typecheck(SWIG_TYPECHECK_VOIDPTR, noblock=1) void *
{
  void *ptr = 0;
  int res = SWIG_ConvertPtr($input, &ptr, 0, 0);
  $1 = SWIG_CheckState(res);
}

#if defined(SWIG_DIRECTOR_TYPEMAPS)

/* directorin */

%typemap(directorin,noblock=1) void *, void const*, void *const, void const *const, 
  void const *&, void *const &, void const *const & {
  $input = SWIG_NewPointerObj(%as_voidptr($1), $descriptor, %newpointer_flags);
}

/* directorout */

%typemap(directorout,noblock=1) void * (void *argp, int res) {
  res = SWIG_ConvertPtr($input, &argp, 0, 0);
  if (!SWIG_IsOK(res)) {
    %dirout_fail(res,"$type");
  }
  $result = %reinterpret_cast(argp, $ltype);
}

%typemap(directorout,noblock=1,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) void * const& (void *argp, int res) {
  res = SWIG_ConvertPtr($input, &argp, 0, $disown);
  if (!SWIG_IsOK(res)) { 
    %dirout_fail(res,"$type");
  }
  static $*ltype temp = %reinterpret_cast(argp, $*ltype);
  $result =  &temp;
}



#endif /* SWIG_DIRECTOR_TYPEMAPS */

