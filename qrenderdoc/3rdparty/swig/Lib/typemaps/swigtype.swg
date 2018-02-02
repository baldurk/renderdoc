/* -----------------------------------------------------------------------------
 * --- Input arguments --- 
 * ----------------------------------------------------------------------------- */
/* Pointers and arrays */
%typemap(in, noblock=1) SWIGTYPE *(void  *argp = 0, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp,$descriptor, $disown | %convertptr_flags);
  if (!SWIG_IsOK(res)) { 
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  $1 = %reinterpret_cast(argp, $ltype);
}
%typemap(freearg) SWIGTYPE * "";

%typemap(in, noblock=1) SWIGTYPE [] (void *argp = 0, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp,$descriptor, $disown | %convertptr_flags);
  if (!SWIG_IsOK(res)) { 
    %argument_fail(res, "$type", $symname, $argnum); 
  } 
  $1 = %reinterpret_cast(argp, $ltype);
}
%typemap(freearg) SWIGTYPE [] "";


%typemap(in, noblock=1) SWIGTYPE *const&  (void *argp = 0, int res = 0, $*1_ltype temp) {
  res = SWIG_ConvertPtr($input, &argp, $*descriptor, $disown | %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$*ltype", $symname, $argnum); 
  }
  temp = %reinterpret_cast(argp, $*ltype);
  $1 = %reinterpret_cast(&temp, $1_ltype);
}
%typemap(freearg) SWIGTYPE *const& "";


/* Reference */
%typemap(in, noblock=1) SWIGTYPE & (void *argp = 0, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  if (!argp) { %argument_nullref("$type", $symname, $argnum); }
  $1 = %reinterpret_cast(argp, $ltype);
}
%typemap(freearg) SWIGTYPE & "";

#if defined(__cplusplus) && defined(%implicitconv_flag)
%typemap(in,noblock=1,implicitconv=1) const SWIGTYPE & (void *argp = 0, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags | %implicitconv_flag);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  if (!argp) { %argument_nullref("$type", $symname, $argnum); }
  $1 = %reinterpret_cast(argp, $ltype);
}
%typemap(freearg,noblock=1,match="in",implicitconv=1) const SWIGTYPE &
{
  if (SWIG_IsNewObj(res$argnum)) %delete($1);
}
#else
%typemap(in,noblock=1) const SWIGTYPE & (void *argp, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  if (!argp) { %argument_nullref("$type", $symname, $argnum); }
  $1 = %reinterpret_cast(argp, $ltype);
}
#endif

/* Rvalue reference */
%typemap(in, noblock=1) SWIGTYPE && (void *argp = 0, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  if (!argp) { %argument_nullref("$type", $symname, $argnum); }
  $1 = %reinterpret_cast(argp, $ltype);
}
%typemap(freearg) SWIGTYPE && "";

#if defined(__cplusplus) && defined(%implicitconv_flag)
%typemap(in,noblock=1,implicitconv=1) const SWIGTYPE && (void *argp = 0, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags | %implicitconv_flag);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  if (!argp) { %argument_nullref("$type", $symname, $argnum); }
  $1 = %reinterpret_cast(argp, $ltype);
}
%typemap(freearg,noblock=1,match="in",implicitconv=1) const SWIGTYPE &&
{
  if (SWIG_IsNewObj(res$argnum)) %delete($1);
}
#else
%typemap(in,noblock=1) const SWIGTYPE && (void *argp, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }
  if (!argp) { %argument_nullref("$type", $symname, $argnum); }
  $1 = %reinterpret_cast(argp, $ltype);
}
#endif

/* By value */
#if defined(__cplusplus) && defined(%implicitconv_flag)
%typemap(in,implicitconv=1) SWIGTYPE (void *argp, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $&descriptor, %convertptr_flags | %implicitconv_flag);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }  
  if (!argp) { 
    %argument_nullref("$type", $symname, $argnum);
  } else {
    $&ltype temp = %reinterpret_cast(argp, $&ltype);
    $1 = *temp;
    if (SWIG_IsNewObj(res)) %delete(temp);
  }
}
#else
%typemap(in) SWIGTYPE (void *argp, int res = 0) {
  res = SWIG_ConvertPtr($input, &argp, $&descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type", $symname, $argnum); 
  }  
  if (!argp) { 
    %argument_nullref("$type", $symname, $argnum);
  } else {
    $1 = *(%reinterpret_cast(argp, $&ltype));
  }
}
#endif


/* -----------------------------------------------------------------------------
 * --- Output arguments --- 
 * ----------------------------------------------------------------------------- */

/* Pointers, references */
%typemap(out,noblock=1) SWIGTYPE *, SWIGTYPE &, SWIGTYPE &&, SWIGTYPE[] {
  %set_output(SWIG_NewPointerObj(%as_voidptr($1), $descriptor, $owner | %newpointer_flags));
}

%typemap(out, noblock=1) SWIGTYPE *const& {
  %set_output(SWIG_NewPointerObj(%as_voidptr(*$1), $*descriptor, $owner | %newpointer_flags));
}

/* Return by value */
%typemap(out, noblock=1) SWIGTYPE {
  %set_output(SWIG_NewPointerObj(%new_copy($1, $ltype), $&descriptor, SWIG_POINTER_OWN | %newpointer_flags));
}

/* -----------------------------------------------------------------------------
 * --- Variable input --- 
 * ----------------------------------------------------------------------------- */

/* memberin/globalin/varin, for fix arrays. */

%typemap(memberin) SWIGTYPE [ANY] {
  if ($input) {
    size_t ii = 0;
    for (; ii < (size_t)$1_dim0; ++ii) *($1_basetype *)&$1[ii] = *(($1_basetype *)$input + ii);
  } else {
    %variable_nullref("$type","$name");
  }
}

%typemap(globalin) SWIGTYPE [ANY] {
  if ($input) {
    size_t ii = 0;
    for (; ii < (size_t)$1_dim0; ++ii) *($1_basetype *)&$1[ii] = *(($1_basetype *)$input + ii);
  } else {
    %variable_nullref("$type","$name");
  }
}

%typemap(varin) SWIGTYPE [ANY] {
  $basetype *inp = 0;
  int res = SWIG_ConvertPtr($input, %as_voidptrptr(&inp), $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  } else if (inp) {
    size_t ii = 0;
    for (; ii < (size_t)$1_dim0; ++ii) *($1_basetype *)&$1[ii] = *(($1_basetype *)inp + ii);
  } else {
    %variable_nullref("$type", "$name");
  }
}


/* memberin/globalin/varin, for fix double arrays. */

%typemap(memberin) SWIGTYPE [ANY][ANY] {
  if ($input) {
    size_t ii = 0;
    for (; ii < (size_t)$1_dim0; ++ii) {
      if ($input[ii]) {
	size_t jj = 0;
	for (; jj < (size_t)$1_dim1; ++jj) $1[ii][jj] = $input[ii][jj];
      } else {
	%variable_nullref("$type","$name");
      }
    }
  } else {
    %variable_nullref("$type","$name");
  }
}

%typemap(globalin) SWIGTYPE [ANY][ANY] {
  if ($input) {
    size_t ii = 0;
    for (; ii < (size_t)$1_dim0; ++ii) {
      if ($input[ii]) {
	size_t jj = 0;
	for (; jj < (size_t)$1_dim1; ++jj) $1[ii][jj] = $input[ii][jj];
      } else {
	%variable_nullref("$type","$name");
      }
    }
  } else {
    %variable_nullref("$type","$name");
  }
}

%typemap(varin) SWIGTYPE [ANY][ANY] {
  $basetype (*inp)[$1_dim1] = 0;
  int res = SWIG_ConvertPtr($input, %as_voidptrptr(&inp), $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  } else if (inp) {
    size_t ii = 0;
    for (; ii < (size_t)$1_dim0; ++ii) {
      if (inp[ii]) {
	size_t jj = 0;
	for (; jj < (size_t)$1_dim1; ++jj) $1[ii][jj] = inp[ii][jj];
      } else {
	%variable_nullref("$type", "$name");
      }
    }
  } else {
    %variable_nullref("$type", "$name");
  }
}

/* Pointers, references, and variable size arrays */

%typemap(varin,warning=SWIGWARN_TYPEMAP_SWIGTYPELEAK_MSG) SWIGTYPE * {
  void *argp = 0;
  int res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);  
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  $1 = %reinterpret_cast(argp, $ltype);
}

%typemap(varin,noblock=1,warning="462:Unable to set dimensionless array variable") SWIGTYPE []
{
  %variable_fail(SWIG_AttributeError, "$type", "read-only $name");
}

%typemap(varin,warning=SWIGWARN_TYPEMAP_SWIGTYPELEAK_MSG) SWIGTYPE & {
  void *argp = 0;
  int res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  if (!argp) {
    %variable_nullref("$type", "$name");
  }
  $1 = *(%reinterpret_cast(argp, $ltype));
}

%typemap(varin,warning=SWIGWARN_TYPEMAP_SWIGTYPELEAK_MSG) SWIGTYPE && {
  void *argp = 0;
  int res = SWIG_ConvertPtr($input, &argp, $descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  if (!argp) {
    %variable_nullref("$type", "$name");
  }
  $1 = *(%reinterpret_cast(argp, $ltype));
}

#if defined(__cplusplus) && defined(%implicitconv_flag)
%typemap(varin,implicitconv=1) SWIGTYPE {
  void *argp = 0;
  int res = SWIG_ConvertPtr($input, &argp, $&descriptor, %convertptr_flags | %implicitconv_flag);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  if (!argp) {
    %variable_nullref("$type", "$name");
  } else {
    $&type temp;
    temp  = %reinterpret_cast(argp, $&type);
    $1 = *temp;
    if (SWIG_IsNewObj(res)) %delete(temp);
  }
}
#else
%typemap(varin) SWIGTYPE {
  void *argp = 0;
  int res = SWIG_ConvertPtr($input, &argp, $&descriptor, %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  if (!argp) {
    %variable_nullref("$type", "$name");
  } else {
    $1 = *(%reinterpret_cast(argp, $&type));
  }
}
#endif

/* -----------------------------------------------------------------------------
 * --- Variable output --- 
 * ----------------------------------------------------------------------------- */

/* Pointers and arrays */
%typemap(varout, noblock=1) SWIGTYPE * {
  %set_varoutput(SWIG_NewPointerObj(%as_voidptr($1), $descriptor, %newpointer_flags));
}

%typemap(varout, noblock=1) SWIGTYPE [] {
  %set_varoutput(SWIG_NewPointerObj(%as_voidptr($1), $descriptor, %newpointer_flags));
}

/* References */
%typemap(varout, noblock=1) SWIGTYPE & {
  %set_varoutput(SWIG_NewPointerObj(%as_voidptr(&$1), $descriptor, %newpointer_flags));
}

%typemap(varout, noblock=1) SWIGTYPE && {
  %set_varoutput(SWIG_NewPointerObj(%as_voidptr(&$1), $descriptor, %newpointer_flags));
}

/* Value */
%typemap(varout, noblock=1) SWIGTYPE {
  %set_varoutput(SWIG_NewPointerObj(%as_voidptr(&$1), $&descriptor, %newpointer_flags));
}

/* ------------------------------------------------------------
 * --- Typechecking rules ---
 * ------------------------------------------------------------ */

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) SWIGTYPE * {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $descriptor, 0);
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) SWIGTYPE *const& {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $*descriptor, 0);
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) SWIGTYPE & {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $descriptor, 0);
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) SWIGTYPE && {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $descriptor, 0);
  $1 = SWIG_CheckState(res);
}

#if defined(__cplusplus) && defined(%implicitconv_flag)
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1,implicitconv=1) const SWIGTYPE & {
  int res = SWIG_ConvertPtr($input, 0, $descriptor, %implicitconv_flag);
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1,implicitconv=1) const SWIGTYPE && {
  int res = SWIG_ConvertPtr($input, 0, $descriptor, %implicitconv_flag);
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1,implicitconv=1) SWIGTYPE {
  int res = SWIG_ConvertPtr($input, 0, $&descriptor, %implicitconv_flag);
  $1 = SWIG_CheckState(res);
}
#else
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) const SWIGTYPE & {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $descriptor, 0);
  $1 = SWIG_CheckState(res);
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) const SWIGTYPE && {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $descriptor, 0);
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER,noblock=1) SWIGTYPE {
  void *vptr = 0;
  int res = SWIG_ConvertPtr($input, &vptr, $&descriptor, 0);
  $1 = SWIG_CheckState(res);
}
#endif

/* -----------------------------------------------------------------------------
 * --- Director typemaps --- *  
 * ----------------------------------------------------------------------------- */

#if defined(SWIG_DIRECTOR_TYPEMAPS)

/* directorin */

%typemap(directorin,noblock=1) SWIGTYPE *, SWIGTYPE *const& {
  $input = SWIG_NewPointerObj(%as_voidptr($1), $descriptor, %newpointer_flags);
}

%typemap(directorin,noblock=1) SWIGTYPE {
  $input = SWIG_NewPointerObj(%as_voidptr(new $1_ltype((const $1_ltype &)$1)), $&descriptor, SWIG_POINTER_OWN | %newpointer_flags);
}

%typemap(directorin,noblock=1) SWIGTYPE & {
  $input = SWIG_NewPointerObj(%as_voidptr(&$1), $descriptor, %newpointer_flags);
}

%typemap(directorin,noblock=1) SWIGTYPE && {
  $input = SWIG_NewPointerObj(%as_voidptr(&$1_name), $descriptor, %newpointer_flags);
}

/* directorout */
#if defined(__cplusplus) && defined(%implicitconv_flag)
%typemap(directorout,noblock=1,implicitconv=1) SWIGTYPE (void * swig_argp, int swig_res = 0) {
  swig_res = SWIG_ConvertPtr($input,&swig_argp,$&descriptor, %convertptr_flags | %implicitconv_flag);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
  $result = *(%reinterpret_cast(swig_argp, $&ltype));
  if (SWIG_IsNewObj(swig_res)) %delete(%reinterpret_cast(swig_argp, $&ltype));
}
#else
%typemap(directorout,noblock=1) SWIGTYPE (void * swig_argp, int swig_res = 0) {
  swig_res = SWIG_ConvertPtr($input,&swig_argp,$&descriptor, %convertptr_flags);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
  $result = *(%reinterpret_cast(swig_argp, $&ltype));
}
#endif

%typemap(directorout,noblock=1,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) 
  SWIGTYPE *(void *swig_argp, int swig_res, swig_owntype own) {
  swig_res = SWIG_ConvertPtrAndOwn($input, &swig_argp, $descriptor, %convertptr_flags | SWIG_POINTER_DISOWN, &own);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
  $result = %reinterpret_cast(swig_argp, $ltype);
  swig_acquire_ownership_obj(%as_voidptr($result), own /* & TODO: SWIG_POINTER_OWN */);
}
%typemap(directorfree,noblock=1,match="directorout") SWIGTYPE * {
  if (director) {
    SWIG_AcquirePtr($result, director->swig_release_ownership(%as_voidptr($input)));
  }
}

%typemap(directorout,noblock=1,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) 
  SWIGTYPE &(void *swig_argp, int swig_res, swig_owntype own) {
  swig_res = SWIG_ConvertPtrAndOwn($input, &swig_argp, $descriptor, %convertptr_flags | SWIG_POINTER_DISOWN, &own);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
  if (!swig_argp) { %dirout_nullref("$type"); }
  $result = %reinterpret_cast(swig_argp, $ltype);
  swig_acquire_ownership_obj(%as_voidptr($result), own /* & TODO: SWIG_POINTER_OWN */);
}
%typemap(directorfree,noblock=1,match="directorout") SWIGTYPE & {
  if (director) {
    SWIG_AcquirePtr($result, director->swig_release_ownership(%as_voidptr($input)));
  }
}

%typemap(directorout,noblock=1,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) 
  SWIGTYPE &&(void *swig_argp, int swig_res, swig_owntype own) {
  swig_res = SWIG_ConvertPtrAndOwn($input, &swig_argp, $descriptor, %convertptr_flags | SWIG_POINTER_DISOWN, &own);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
  if (!swig_argp) { %dirout_nullref("$type"); }
  $result = %reinterpret_cast(swig_argp, $ltype);
  swig_acquire_ownership_obj(%as_voidptr($result), own /* & TODO: SWIG_POINTER_OWN */);
}
%typemap(directorfree,noblock=1,match="directorout") SWIGTYPE && {
  if (director) {
    SWIG_AcquirePtr($result, director->swig_release_ownership(%as_voidptr($input)));
  }
}

#endif /* SWIG_DIRECTOR_TYPEMAPS */


/* ------------------------------------------------------------
 * --- Constants ---
 * ------------------------------------------------------------ */

%typemap(constcode,noblock=1) SWIGTYPE *, SWIGTYPE &, SWIGTYPE &&, SWIGTYPE [] {
  %set_constant("$symname", SWIG_NewPointerObj(%as_voidptr($value),$descriptor,%newpointer_flags));
}

%typemap(constcode,noblock=1) SWIGTYPE {
  %set_constant("$symname", SWIG_NewPointerObj(%as_voidptr(&$value),$&descriptor,%newpointer_flags));
}

/* ------------------------------------------------------------
 * --- Exception handling ---
 * ------------------------------------------------------------ */

%typemap(throws,noblock=1) SWIGTYPE {
  %raise(SWIG_NewPointerObj(%new_copy($1, $ltype),$&descriptor,SWIG_POINTER_OWN), "$type", $&descriptor);
}

%typemap(throws,noblock=1) SWIGTYPE * {
  %raise(SWIG_NewPointerObj(%as_voidptr($1),$descriptor,0), "$type", $descriptor);
}

%typemap(throws,noblock=1) SWIGTYPE [ANY] {
  %raise(SWIG_NewPointerObj(%as_voidptr($1),$descriptor,0), "$type", $descriptor);
}

%typemap(throws,noblock=1) SWIGTYPE & {
  %raise(SWIG_NewPointerObj(%as_voidptr(&$1),$descriptor,0), "$type", $descriptor);
}

%typemap(throws,noblock=1) SWIGTYPE && {
  %raise(SWIG_NewPointerObj(%as_voidptr(&$1),$descriptor,0), "$type", $descriptor);
}

%typemap(throws,noblock=1) (...) {
  SWIG_exception_fail(SWIG_RuntimeError,"unknown exception");
}

/* ------------------------------------------------------------
 * --- CLASS::* typemaps --- 
 * ------------------------------------------------------------ */

%typemap(in) SWIGTYPE (CLASS::*) {  
  int res = SWIG_ConvertMember($input, %as_voidptr(&$1), sizeof($type),$descriptor);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum); 
  }
}

%typemap(out,noblock=1) SWIGTYPE (CLASS::*) {
  %set_output(SWIG_NewMemberObj(%as_voidptr(&$1), sizeof($type), $descriptor));
}

%typemap(varin) SWIGTYPE (CLASS::*) {
  int res = SWIG_ConvertMember($input,%as_voidptr(&$1), sizeof($type), $descriptor);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name"); 
  }
}

%typemap(varout,noblock=1) SWIGTYPE (CLASS::*) {
  %set_varoutput(SWIG_NewMemberObj(%as_voidptr(&$1), sizeof($type), $descriptor));
}

%typemap(constcode,noblock=1) SWIGTYPE (CLASS::*) {
  %set_constant("$symname", SWIG_NewMemberObj(%as_voidptr(&$value), sizeof($type), $descriptor));
}

#if defined(SWIG_DIRECTOR_TYPEMAPS)

/* directorin */

%typemap(directorin,noblock=1) SWIGTYPE (CLASS::*) {
  $input = SWIG_NewMemberObj(%as_voidptr(&$1), sizeof($type), $descriptor);
}

/* directorout */

%typemap(directorout) SWIGTYPE (CLASS::*) {
  int swig_res = SWIG_ConvertMember($input,%as_voidptr(&$result), sizeof($type), $descriptor);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
}
#endif

/* ------------------------------------------------------------
 * --- function ptr typemaps --- 
 * ------------------------------------------------------------ */

/*
  ISO C++ doesn't allow direct casting of a function ptr to a object
  ptr. So, maybe the ptr sizes are not the same, and we need to take
  some providences.
 */
%typemap(in) SWIGTYPE ((*)(ANY)) {
  int res = SWIG_ConvertFunctionPtr($input, (void**)(&$1), $descriptor);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum); 
  }
}

%typecheck(SWIG_TYPECHECK_POINTER,noblock=1) SWIGTYPE ((*)(ANY)) {
  void *ptr = 0;
  int res = SWIG_ConvertFunctionPtr($input, &ptr, $descriptor);
  $1 = SWIG_CheckState(res);
}


%typemap(out, noblock=1) SWIGTYPE ((*)(ANY)) {
  %set_output(SWIG_NewFunctionPtrObj((void *)($1), $descriptor));
}

%typemap(varin) SWIGTYPE ((*)(ANY)) {
  int res = SWIG_ConvertFunctionPtr($input, (void**)(&$1), $descriptor);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name"); 
  }
}

%typemap(varout,noblock=1) SWIGTYPE ((*)(ANY)) {  
  %set_varoutput(SWIG_NewFunctionPtrObj((void *)($1), $descriptor));
}

%typemap(constcode, noblock=1) SWIGTYPE ((*)(ANY)){
  %set_constant("$symname", SWIG_NewFunctionPtrObj((void *)$value, $descriptor));
}
%typemap(constcode) SWIGTYPE ((* const)(ANY)) = SWIGTYPE ((*)(ANY));

#if defined(SWIG_DIRECTOR_TYPEMAPS)

/* directorin */

%typemap(directorin,noblock=1) SWIGTYPE ((*)(ANY)) {
  $input = SWIG_NewFunctionPtrObj((void*)($1), $descriptor);
}

/* directorout */

%typemap(directorout) SWIGTYPE ((*)(ANY)) {
  int swig_res = SWIG_ConvertFunctionPtr($input,(void**)(&$result),$descriptor);
  if (!SWIG_IsOK(swig_res)) {
    %dirout_fail(swig_res,"$type");
  }
}
#endif

%apply SWIGTYPE * { SWIGTYPE *const }

/* ------------------------------------------------------------
 * --- Special typemaps ---
 * ------------------------------------------------------------ */

/* DISOWN typemap */

%typemap(in, noblock=1) SWIGTYPE *DISOWN (int res = 0) {
  res = SWIG_ConvertPtr($input, %as_voidptrptr(&$1), $descriptor, SWIG_POINTER_DISOWN | %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type", $symname, $argnum);
  }
}

%typemap(varin) SWIGTYPE *DISOWN {
  void *temp = 0;
  int res = SWIG_ConvertPtr($input, &temp, $descriptor, SWIG_POINTER_DISOWN | %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  $1 = ($ltype) temp;
}

/* DYNAMIC typemap */

%typemap(out,noblock=1) SWIGTYPE *DYNAMIC, SWIGTYPE &DYNAMIC {
  %set_output(SWIG_NewPointerObj(%as_voidptr($1), SWIG_TypeDynamicCast($descriptor, %as_voidptrptr(&$1)), $owner | %newpointer_flags));
}

/* INSTANCE typemap */

%typemap(out,noblock=1) SWIGTYPE INSTANCE {
  %set_output(SWIG_NewInstanceObj(%new_copy($1, $1_ltype), $&1_descriptor, SWIG_POINTER_OWN | %newinstance_flags));
}

%typemap(out,noblock=1) SWIGTYPE *INSTANCE, SWIGTYPE &INSTANCE, SWIGTYPE INSTANCE[] {
  %set_output(SWIG_NewInstanceObj(%as_voidptr($1), $1_descriptor, $owner | %newinstance_flags));
}

%typemap(varout,noblock=1) SWIGTYPE *INSTANCE, SWIGTYPE INSTANCE[] {
  %set_varoutput(SWIG_NewInstanceObj(%as_voidptr($1), $1_descriptor, %newinstance_flags));
}

%typemap(varout,noblock=1) SWIGTYPE &INSTANCE {
  %set_varoutput(SWIG_NewInstanceObj(%as_voidptr($1), $1_descriptor, %newinstance_flags));
}

%typemap(varout,noblock=1) SWIGTYPE INSTANCE {
  %set_varoutput(SWIG_NewInstanceObj(%as_voidptr(&$1), $&1_descriptor, %newinstance_flags));
}

