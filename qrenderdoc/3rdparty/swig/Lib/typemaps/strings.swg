//
// Use the macro SWIG_PRESERVE_CARRAY_SIZE if you prefer to preserve
// the size of char arrays, ie
//  ------------------------------------------
//       C Side             =>   Language Side
//  ------------------------------------------
//   char name[5] = "hola"  =>   'hola\0'
//
// the default behaviour is 
//
//   char name[5] = "hola"  =>   'hola'
//
//
//#define SWIG_PRESERVE_CARRAY_SIZE

/* ------------------------------------------------------------
 *  String typemaps for type Char (char or wchar_t)
 * ------------------------------------------------------------ */

%define %_typemap_string(StringCode, 
			 Char,
			 SWIG_AsCharPtrAndSize,
			 SWIG_FromCharPtrAndSize,
			 SWIG_CharPtrLen,
			 SWIG_CharBufLen,
			 SWIG_AsCharPtr,
			 SWIG_FromCharPtr,
			 SWIG_AsCharArray,
                         SWIG_NewCopyCharArray,
                         SWIG_DeleteCharArray)

/* in */

%typemap(in,noblock=1,fragment=#SWIG_AsCharPtr) 
  Char * (int res, Char *buf = 0, int alloc = 0),
  const Char * (int res, Char *buf = 0, int alloc = 0) {
  res = SWIG_AsCharPtr($input, &buf, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = %reinterpret_cast(buf, $1_ltype);
}
%typemap(freearg,noblock=1,match="in") Char *, const Char * {
  if (alloc$argnum == SWIG_NEWOBJ) SWIG_DeleteCharArray(buf$argnum);
}

%typemap(in,noblock=1,fragment=#SWIG_AsCharPtr) Char const*& (int res, Char *buf = 0, int alloc = 0) {
  res = SWIG_AsCharPtr($input, &buf, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = &buf;
}    
%typemap(freearg, noblock=1,match="in") Char const*& {
  if (alloc$argnum == SWIG_NEWOBJ) SWIG_DeleteCharArray(buf$argnum);
}

/* out */

%typemap(out,noblock=1,fragment=#SWIG_FromCharPtr) Char *, const Char * {
  %set_output(SWIG_FromCharPtr((const Char *)$1));
}


%typemap(out,noblock=1,fragment=#SWIG_FromCharPtr) Char const*& {
  %set_output(SWIG_FromCharPtr(*$1));
}

%typemap(newfree,noblock=1) Char * {
  SWIG_DeleteCharArray($1);
}

/* varin */

%typemap(varin,fragment=#SWIG_AsCharPtrAndSize) Char * {
  Char *cptr = 0; size_t csize = 0; int alloc = SWIG_NEWOBJ;
  int res = SWIG_AsCharPtrAndSize($input, &cptr, &csize, &alloc);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res,"$type","$name");
  }
  if ($1) SWIG_DeleteCharArray($1);
  if (alloc == SWIG_NEWOBJ) {
    $1 = cptr;
  } else {
    $1 = csize ? ($1_type)SWIG_NewCopyCharArray(cptr, csize, Char) : 0;
  }
}

%typemap(varin,fragment=#SWIG_AsCharPtrAndSize,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const Char * {
  Char *cptr = 0; size_t csize = 0; int alloc = SWIG_NEWOBJ;
  int res = SWIG_AsCharPtrAndSize($input, &cptr, &csize, &alloc);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
  if (alloc == SWIG_NEWOBJ) {
    $1 = cptr;
  } else {
    $1 = csize ? ($1_type)SWIG_NewCopyCharArray(cptr, csize, Char) : 0;
  }
}

/* varout */

%typemap(varout,noblock=1,fragment=#SWIG_FromCharPtr) Char *, const Char * {
  %set_varoutput(SWIG_FromCharPtr($1));
}

/* memberin */

%typemap(memberin,noblock=1) Char * {
  if ($1) SWIG_DeleteCharArray($1);
  if ($input) {
    size_t size = SWIG_CharPtrLen(%reinterpret_cast($input, const Char *)) + 1;
    $1 = ($1_type)SWIG_NewCopyCharArray(%reinterpret_cast($input, const Char *), size, Char);
  } else {
    $1 = 0;
  }
}

%typemap(memberin,noblock=1,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const Char * {
  if ($input) {
    size_t size = SWIG_CharPtrLen(%reinterpret_cast(%reinterpret_cast($input, const Char *), const Char *)) + 1;
    $1 = ($1_type)SWIG_NewCopyCharArray($input, size, Char);
  } else {
    $1 = 0;
  }
}

/* globalin */

%typemap(globalin,noblock=1) Char * {
  if ($1) SWIG_DeleteCharArray($1);
  if ($input) {
    size_t size = SWIG_CharPtrLen(%reinterpret_cast(%reinterpret_cast($input, const Char *), const Char *)) + 1;
    $1 = ($1_type)SWIG_NewCopyCharArray($input, size, Char);
  } else {
    $1 = 0;
  }
}

%typemap(globalin,noblock=1,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const Char * {
  if ($input) {
    size_t size = SWIG_CharPtrLen($input) + 1;
    $1 = ($1_type)SWIG_NewCopyCharArray($input, size, Char);
  } else {
    $1 = 0;
  }
}

/* constant */

%typemap(constcode,noblock=1,fragment=#SWIG_FromCharPtr)
  Char *, Char const*, Char * const, Char const* const {
  %set_constant("$symname", SWIG_FromCharPtr($value));
}


#if defined(SWIG_DIRECTOR_TYPEMAPS)

/* directorin */

%typemap(directorin,noblock=1,fragment=#SWIG_FromCharPtr)
  Char *, Char const*, Char *const, Char const *const, 
  Char const *&, Char *const &, Char const *const & {
  $input = SWIG_FromCharPtr((const Char *)$1);
}


/* directorout */

%typemap(directorout,noblock=1,fragment=#SWIG_AsCharPtr,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) Char * (int res, Char *buf = 0, int alloc = SWIG_NEWOBJ) {
  res = SWIG_AsCharPtr($input, &buf, &alloc);
  if (!SWIG_IsOK(res)) {
    %dirout_fail(res, "$type");
  }
  if (alloc == SWIG_NEWOBJ) {
    swig_acquire_ownership_array(buf);
  }
  $result = %reinterpret_cast(buf, $1_ltype);
}
%typemap(directorfree,noblock=1) Char *
{
  if (director) {
    director->swig_release_ownership(%as_voidptr($input));
  }
}


%typemap(directorout,noblock=1,fragment=#SWIG_AsCharPtr,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) Char *const& (int res, Char *buf = 0, int alloc = SWIG_NEWOBJ), Char const*const& (int res, Char *buf = 0, int alloc = SWIG_NEWOBJ) { 
  res = SWIG_AsCharPtr($input, &buf, &alloc);
  if (!SWIG_IsOK(res)) {
    %dirout_fail(res, "$type");
  }
  static $*1_ltype tmp = buf;
  $result = &tmp;
  if (alloc == SWIG_NEWOBJ) {
    swig_acquire_ownership_array(buf);
  }
}
%typemap(directorfree,noblock=1)
  Char * const&, Char const* const& {
  if (director) {
    director->swig_release_ownership(%as_voidptr(*$input));
  }
}

#endif /* SWIG_DIRECTOR_TYPEMAPS */

/* typecheck */

%typemap(typecheck,noblock=1,precedence=StringCode,
	 fragment=#SWIG_AsCharPtr) Char *, const Char *, Char const*& {
  int res = SWIG_AsCharPtr($input, 0, 0);
  $1 = SWIG_CheckState(res);
}


/* throws */

%typemap(throws,noblock=1,fragment=#SWIG_FromCharPtr) Char * {
  %raise(SWIG_FromCharPtr($1), "$type", 0);
}


/* ------------------------------------------------------------
 *  Unknown size const Character array Char[ANY] handling
 * ------------------------------------------------------------ */

%apply Char * { Char [] };
%apply const Char * { const Char [] };

%typemap(varin,noblock=1,warning="462:Unable to set variable of type Char []") Char []
{
  %variable_fail(SWIG_AttributeError, "$type", "read-only $name");
}


/* ------------------------------------------------------------
 *  Fixed size Character array Char[ANY] handling
 * ------------------------------------------------------------ */

/*  memberin and globalin typemaps  */

%typemap(memberin,noblock=1) Char [ANY]
{
  if ($input) memcpy($1,$input,$1_dim0*sizeof(Char));
  else memset($1,0,$1_dim0*sizeof(Char));
}

%typemap(globalin,noblock=1) Char [ANY]
{
  if ($input) memcpy($1,$input,$1_dim0*sizeof(Char));
  else memset($1,0,$1_dim0*sizeof(Char));
}

/* in */

%typemap(in,noblock=1,fragment=#SWIG_AsCharArray)
  Char [ANY] (Char temp[$1_dim0], int res), 
  const Char [ANY](Char temp[$1_dim0], int res)
{  
  res = SWIG_AsCharArray($input, temp, $1_dim0);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = %reinterpret_cast(temp, $1_ltype);
}
%typemap(freearg) Char [ANY], const Char [ANY] "";

%typemap(in,noblock=1,fragment=#SWIG_AsCharArray) const Char (&)[ANY] (Char temp[$1_dim0], int res)
{  
  res = SWIG_AsCharArray($input, temp, $1_dim0);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = &temp;
}
%typemap(freearg) const Char (&)[ANY] "";

%typemap(out,fragment=#SWIG_FromCharPtrAndSize,fragment=#SWIG_CharBufLen)
  Char [ANY], const Char[ANY] 
{
%#ifndef SWIG_PRESERVE_CARRAY_SIZE
  size_t size = SWIG_CharBufLen($1, $1_dim0);
%#else
  size_t size = $1_dim0;
%#endif
   %set_output(SWIG_FromCharPtrAndSize($1, size));
}

/* varin */

%typemap(varin,fragment=#SWIG_AsCharArray) Char [ANY]
{
  int res = SWIG_AsCharArray($input, $1, $1_dim0);
  if (!SWIG_IsOK(res)) {
    %variable_fail(res, "$type", "$name");
  }
}

/* varout */

%typemap(varout,fragment=#SWIG_CharBufLen)
  Char [ANY], const Char [ANY] {
%#ifndef SWIG_PRESERVE_CARRAY_SIZE
  size_t size = SWIG_CharBufLen($1, $1_dim0);
%#else
  size_t size = $1_dim0;
%#endif
  %set_varoutput(SWIG_FromCharPtrAndSize($1, size));
}

/* constant */

%typemap(constcode,fragment=#SWIG_CharBufLen)
  Char [ANY], const Char [ANY]
{
%#ifndef SWIG_PRESERVE_CARRAY_SIZE
  size_t size = SWIG_CharBufLen($1, $1_dim0);
%#else
  size_t size = $value_dim0;
%#endif
  %set_constant("$symname", SWIG_FromCharPtrAndSize($value,size));
}


#if defined(SWIG_DIRECTOR_TYPEMAPS)

/* directorin */
%typemap(directorin,fragment=#SWIG_CharBufLen)
  Char [ANY], const Char [ANY] 
{
%#ifndef SWIG_PRESERVE_CARRAY_SIZE
  size_t size = SWIG_CharBufLen($1, $1_dim0);
%#else
  size_t size = $1_dim0;
%#endif
  $input = SWIG_FromCharPtrAndSize($1, size);
}

/* directorout */

%typemap(directorout,noblock=1,fragment=#SWIG_AsCharArray)
  Char [ANY] (Char temp[$result_dim0]),
  const Char [ANY] (Char temp[$result_dim0], int res)
{ 
  res = SWIG_AsCharArray($input, temp, $result_dim0);
  if (!SWIG_IsOK(res)) {
    %dirout_fail(res, "$type");
  }
  $result = temp;
}

#endif /* SWIG_DIRECTOR_TYPEMAPS */

/* typecheck */

%typemap(typecheck,noblock=1,precedence=StringCode,
	 fragment=#SWIG_AsCharArray)  
  Char [ANY], const Char[ANY] {
  int res = SWIG_AsCharArray($input, (Char *)0, $1_dim0);
  $1 = SWIG_CheckState(res);
}


/* throws */

%typemap(throws,fragment=#SWIG_CharBufLen)
  Char [ANY], const Char[ANY]
{
%#ifndef SWIG_PRESERVE_CARRAY_SIZE
  size_t size = SWIG_CharBufLen($1, $1_dim0);
%#else
  size_t size = $1_dim0;
%#endif
  %raise(SWIG_FromCharPtrAndSize($1, size), "$type", 0); 
}

/* -------------------------------------------------------------------
 * --- Really fix size Char arrays, including '\0'chars at the end ---
 * ------------------------------------------------------------------- */

%typemap(varout,noblock=1,fragment=#SWIG_FromCharPtrAndSize)
  Char FIXSIZE[ANY], const Char FIXSIZE[ANY]
{
  %set_varoutput(SWIG_FromCharPtrAndSize($1, $1_dim0));
}

%typemap(out,noblock=1,fragment=#SWIG_FromCharPtrAndSize)
  Char FIXSIZE[ANY], const Char FIXSIZE[ANY]
{
  %set_output(SWIG_FromCharPtrAndSize($1, $1_dim0));
}

#if defined(SWIG_DIRECTOR_TYPEMAPS)

%typemap(directorin,noblock=1,fragment=#SWIG_FromCharPtrAndSize)
  Char FIXSIZE[ANY], const Char FIXSIZE[ANY]
{
  $input = SWIG_FromCharPtrAndSize($1, $1_dim0);
}

#endif /* SWIG_DIRECTOR_TYPEMAPS */

%typemap(throws,noblock=1,fragment=#SWIG_FromCharPtrAndSize)
  Char FIXSIZE[ANY], const Char FIXSIZE[ANY] {
  %raise(SWIG_FromCharPtrAndSize($1, $1_dim0), "$type", 0); 
}

/* ------------------------------------------------------------
 * --- String & length ---
 * ------------------------------------------------------------ */

/* Here len doesn't include the '0' terminator */
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize) 
  (Char *STRING, size_t LENGTH) (int res, Char *buf = 0, size_t size = 0, int alloc = 0),
  (const Char *STRING, size_t LENGTH) (int res, Char *buf = 0, size_t size = 0, int alloc = 0)
{
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) { 
    %argument_fail(res,"$type",$symname, $argnum);
  }  
  $1 = %reinterpret_cast(buf, $1_ltype);
  $2 = %numeric_cast(size - 1, $2_ltype);
}
%typemap(freearg,noblock=1,match="in") (Char *STRING, size_t LENGTH) {
  if (alloc$argnum == SWIG_NEWOBJ) SWIG_DeleteCharArray(buf$argnum);
}
/* old 'int' form */
%typemap(in) (Char *STRING, int LENGTH) = (Char *STRING, size_t LENGTH);
%typemap(freearg) (Char *STRING, int LENGTH) = (Char *STRING, size_t LENGTH);


/* Here size includes the '0' terminator */
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize)
  (Char *STRING, size_t SIZE) (int res, Char *buf = 0, size_t size = 0, int alloc = 0),
  (const Char *STRING, size_t SIZE) (int res, Char *buf = 0, size_t size = 0, int alloc = 0)
{
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res,"$type",$symname, $argnum);
  }
  $1 = %reinterpret_cast(buf, $1_ltype);
  $2 = %numeric_cast(size, $2_ltype);
}
%typemap(freearg,noblock=1,match="in") (Char *STRING, size_t SIZE) {
  if (alloc$argnum == SWIG_NEWOBJ) SWIG_DeleteCharArray(buf$argnum);
}
/* old 'int' form */
%typemap(in) (Char *STRING, int SIZE) = (Char *STRING, size_t SIZE);
%typemap(freearg) (Char *STRING, int SIZE) = (Char *STRING, size_t SIZE);


/* reverse order versions */

/* Here len doesn't include the '0' terminator */
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize) 
  (size_t LENGTH, Char *STRING) (int res, Char *buf = 0, size_t size = 0, int alloc = 0),
  (size_t LENGTH, const Char *STRING) (int res, Char *buf = 0, size_t size = 0, int alloc = 0)
{
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) { 
    %argument_fail(res,"$type",$symname, $argnum);
  }  
  $2 = %reinterpret_cast(buf, $2_ltype) ;
  $1 = %numeric_cast(size - 1, $1_ltype) ;
}
%typemap(freearg, noblock=1, match="in") (size_t LENGTH, Char *STRING) {
  if (alloc$argnum == SWIG_NEWOBJ) SWIG_DeleteCharArray(buf$argnum);
}
/* old 'int' form */
%typemap(in) (int LENGTH, Char *STRING) = (size_t LENGTH, Char *STRING);
%typemap(freearg) (int LENGTH, Char *STRING) = (size_t LENGTH, Char *STRING);

/* Here size includes the '0' terminator */
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize)
  (size_t SIZE, Char *STRING) (int res, Char *buf = 0, size_t size = 0, int alloc = 0),
  (size_t SIZE, const Char *STRING) (int res, Char *buf = 0, size_t size = 0, int alloc = 0)
{
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "$type",$symname, $argnum);
  }
  $2 = %reinterpret_cast(buf, $2_ltype) ;
  $1 = %numeric_cast(size, $1_ltype) ;
}
%typemap(freearg, noblock=1, match="in") (size_t SIZE, Char *STRING) {
  if (alloc$argnum == SWIG_NEWOBJ) SWIG_DeleteCharArray(buf$argnum);
}
/* old 'int' form */
%typemap(in) (int SIZE, Char *STRING) = (size_t SIZE, Char *STRING);
%typemap(freearg) (int SIZE, Char *STRING) = (size_t SIZE, Char *STRING);


%enddef


/* ------------------------------------------------------------
 * --- String fragment methods ---
 * ------------------------------------------------------------ */

#ifndef %_typemap2_string
%define %_typemap2_string(StringCode, CharCode,
			 Char, CharName,
			 SWIG_AsCharPtrAndSize,
			 SWIG_FromCharPtrAndSize,
			 SWIG_CharPtrLen,
			 SWIG_CharBufLen,
			 SWIG_NewCopyCharArray,
			 SWIG_DeleteCharArray,
			 FragLimits, CHAR_MIN, CHAR_MAX)
  
%fragment("SWIG_From"#CharName"Ptr","header",fragment=#SWIG_FromCharPtrAndSize) {
SWIGINTERNINLINE SWIG_Object 
SWIG_From##CharName##Ptr(const Char *cptr)
{ 
  return SWIG_FromCharPtrAndSize(cptr, (cptr ? SWIG_CharPtrLen(cptr) : 0));
}
}

%fragment("SWIG_From"#CharName"Array","header",fragment=#SWIG_FromCharPtrAndSize) {
SWIGINTERNINLINE SWIG_Object 
SWIG_From##CharName##Array(const Char *cptr, size_t size)
{ 
  return SWIG_FromCharPtrAndSize(cptr, size);
}
}

%fragment("SWIG_As" #CharName "Ptr","header",fragment=#SWIG_AsCharPtrAndSize) {
%define_as(SWIG_As##CharName##Ptr(obj, val, alloc), SWIG_AsCharPtrAndSize(obj, val, NULL, alloc))
}

%fragment("SWIG_As" #CharName "Array","header",fragment=#SWIG_AsCharPtrAndSize) {
SWIGINTERN int
SWIG_As##CharName##Array(SWIG_Object obj, Char *val, size_t size)
{ 
  Char* cptr = 0; size_t csize = 0; int alloc = SWIG_OLDOBJ;
  int res = SWIG_AsCharPtrAndSize(obj, &cptr, &csize, &alloc);
  if (SWIG_IsOK(res)) {
    /* special case of single char conversion when we don't need space for NUL */
    if (size == 1 && csize == 2 && cptr && !cptr[1]) --csize;
    if (csize <= size) {
      if (val) {
	if (csize) memcpy(val, cptr, csize*sizeof(Char));
	if (csize < size) memset(val + csize, 0, (size - csize)*sizeof(Char));
      }
      if (alloc == SWIG_NEWOBJ) {
	SWIG_DeleteCharArray(cptr);
	res = SWIG_DelNewMask(res);
      }      
      return res;
    }
    if (alloc == SWIG_NEWOBJ) SWIG_DeleteCharArray(cptr);
  }
  return SWIG_TypeError;
}
}

/* Char */

%fragment(SWIG_From_frag(Char),"header",fragment=#SWIG_FromCharPtrAndSize) {
SWIGINTERNINLINE SWIG_Object
SWIG_From_dec(Char)(Char c) 
{ 
  return SWIG_FromCharPtrAndSize(&c,1);
}
}

%fragment(SWIG_AsVal_frag(Char),"header",
	  fragment="SWIG_As"#CharName"Array",
	  fragment=FragLimits,
	  fragment=SWIG_AsVal_frag(long)) {
SWIGINTERN int
SWIG_AsVal_dec(Char)(SWIG_Object obj, Char *val)
{    
  int res = SWIG_As##CharName##Array(obj, val, 1);
  if (!SWIG_IsOK(res)) {
    long v;
    res = SWIG_AddCast(SWIG_AsVal(long)(obj, &v));
    if (SWIG_IsOK(res)) {
      if ((CHAR_MIN <= v) && (v <= CHAR_MAX)) {
	if (val) *val = %numeric_cast(v, Char);
      } else {
	res = SWIG_OverflowError;
      }
    }
  }
  return res;
}
}

%_typemap_string(StringCode, 
		 Char,
		 SWIG_AsCharPtrAndSize,
		 SWIG_FromCharPtrAndSize,
		 SWIG_CharPtrLen,
		 SWIG_CharBufLen,
		 SWIG_As##CharName##Ptr,
		 SWIG_From##CharName##Ptr,
		 SWIG_As##CharName##Array,
		 SWIG_NewCopyCharArray,
		 SWIG_DeleteCharArray)

%enddef
#endif

/* ------------------------------------------------------------
 *  String typemaps and fragments, with default allocators
 * ------------------------------------------------------------ */

%define %typemaps_string(StringCode, CharCode,
			 Char, CharName,
			 SWIG_AsCharPtrAndSize,
			 SWIG_FromCharPtrAndSize,
			 SWIG_CharPtrLen,
			 SWIG_CharBufLen,
			 FragLimits, CHAR_MIN, CHAR_MAX)
%_typemap2_string(StringCode, CharCode,
		  Char, CharName,
		  SWIG_AsCharPtrAndSize,
		  SWIG_FromCharPtrAndSize,
		  SWIG_CharPtrLen,
		  SWIG_CharBufLen,
		  %new_copy_array,
		  %delete_array,
		  FragLimits, CHAR_MIN, CHAR_MAX)
%enddef

/* ------------------------------------------------------------
 *  String typemaps and fragments, with custom allocators
 * ------------------------------------------------------------ */

%define %typemaps_string_alloc(StringCode, CharCode,
			       Char, CharName,
			       SWIG_AsCharPtrAndSize,
			       SWIG_FromCharPtrAndSize,
			       SWIG_CharPtrLen,
			       SWIG_CharBufLen,
			       SWIG_NewCopyCharArray,
			       SWIG_DeleteCharArray,
			       FragLimits, CHAR_MIN, CHAR_MAX)
%_typemap2_string(StringCode, CharCode,
		  Char, CharName,
		  SWIG_AsCharPtrAndSize,
		  SWIG_FromCharPtrAndSize,
		  SWIG_CharPtrLen,
		  SWIG_CharBufLen,
		  SWIG_NewCopyCharArray,
		  SWIG_DeleteCharArray,
		  FragLimits, CHAR_MIN, CHAR_MAX)
%enddef
