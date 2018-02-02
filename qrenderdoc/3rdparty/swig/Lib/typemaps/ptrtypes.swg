/* -----------------------------------------------------------------------------
 * ptrtypes.swg
 *
 * Value typemaps (Type, const Type&) for "Ptr" types, such as swig
 * wrapped classes, that define the AsPtr/From methods
 *
 * To apply them, just use one of the following macros:
 *
 *    %typemaps_asptr(CheckCode, AsPtrMeth, AsPtrFrag, Type)
 *    %typemaps_asptrfrom(CheckCode, AsPtrMeth, FromMeth, AsPtrFrag, FromFrag, Type)
 *
 * or the simpler and normalize form:
 *
 *    %typemaps_asptrfromn(CheckCode, Type)
 *
 * Also, you can use the individual typemap definitions:
 *
 *    %ptr_in_typemap(asptr_meth,frag,Type)
 *    %ptr_varin_typemap(asptr_meth,frag,Type)
 *    %ptr_typecheck_typemap(check,asptr_meth,frag,Type)
 *    %ptr_directorout_typemap(asptr_meth,frag,Type)
 * ----------------------------------------------------------------------------- */

%include <typemaps/valtypes.swg>

/* in */

%define %ptr_in_typemap(asptr_meth,frag,Type...)
  %typemap(in,fragment=frag) Type {
    Type *ptr = (Type *)0;
    int res = asptr_meth($input, &ptr);
    if (!SWIG_IsOK(res) || !ptr) { 
      %argument_fail((ptr ? res : SWIG_TypeError), "$type", $symname, $argnum); 
    }
    $1 = *ptr;
    if (SWIG_IsNewObj(res)) %delete(ptr);
  }
  %typemap(freearg) Type "";
  %typemap(in,fragment=frag) const Type & (int res = SWIG_OLDOBJ) {
    Type *ptr = (Type *)0;
    res = asptr_meth($input, &ptr);
    if (!SWIG_IsOK(res)) { %argument_fail(res,"$type",$symname, $argnum); }
    if (!ptr) { %argument_nullref("$type",$symname, $argnum); }
    $1 = ptr;
  }
  %typemap(freearg,noblock=1) const Type &  {
    if (SWIG_IsNewObj(res$argnum)) %delete($1);
  }
%enddef

/* varin */

%define %ptr_varin_typemap(asptr_meth,frag,Type...)
  %typemap(varin,fragment=frag) Type {
    Type *ptr = (Type *)0;
    int res = asptr_meth($input, &ptr);
    if (!SWIG_IsOK(res) || !ptr) { 
      %variable_fail((ptr ? res : SWIG_TypeError), "$type", "$name"); 
    }
    $1 = *ptr;
    if (SWIG_IsNewObj(res)) %delete(ptr);
  }
%enddef

#if defined(SWIG_DIRECTOR_TYPEMAPS)
/* directorout */

%define %ptr_directorout_typemap(asptr_meth,frag,Type...)
  %typemap(directorargout,noblock=1,fragment=frag) Type *DIRECTOROUT ($*ltype temp, int swig_ores) {
    Type *swig_optr = 0;
    swig_ores = $result ? asptr_meth($result, &swig_optr) : 0;
    if (!SWIG_IsOK(swig_ores) || !swig_optr) { 
      %dirout_fail((swig_optr ? swig_ores : SWIG_TypeError),"$type");
    }
    temp = *swig_optr;
    $1 = &temp;
    if (SWIG_IsNewObj(swig_ores)) %delete(swig_optr);
  }

  %typemap(directorout,noblock=1,fragment=frag) Type {
    Type *swig_optr = 0;
    int swig_ores = asptr_meth($input, &swig_optr);
    if (!SWIG_IsOK(swig_ores) || !swig_optr) { 
      %dirout_fail((swig_optr ? swig_ores : SWIG_TypeError),"$type");
    }
    $result = *swig_optr;
    if (SWIG_IsNewObj(swig_ores)) %delete(swig_optr);
  }

  %typemap(directorout,noblock=1,fragment=frag,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) Type* {
    Type *swig_optr = 0;
    int swig_ores = asptr_meth($input, &swig_optr);
    if (!SWIG_IsOK(swig_ores)) { 
      %dirout_fail(swig_ores,"$type");
    }    
    $result = swig_optr;
    if (SWIG_IsNewObj(swig_ores)) {
      swig_acquire_ownership(swig_optr);
    }
  }
  %typemap(directorfree,noblock=1) Type*
  {
    if (director)  {
      director->swig_release_ownership(%as_voidptr($input));
    }
  }

  %typemap(directorout,noblock=1,fragment=frag,warning=SWIGWARN_TYPEMAP_DIRECTOROUT_PTR_MSG) Type& {
    Type *swig_optr = 0;
    int swig_ores = asptr_meth($input, &swig_optr);
    if (!SWIG_IsOK(swig_ores)) { 
      %dirout_fail(swig_ores,"$type");
    } else {
      if (!swig_optr) { 
	%dirout_nullref("$type");
      } 
    }    
    $result = swig_optr;
    if (SWIG_IsNewObj(swig_ores)) {
      swig_acquire_ownership(swig_optr);
    }
  }
  %typemap(directorfree,noblock=1) Type&
  {
    if (director) {
      director->swig_release_ownership(%as_voidptr($input));
    }
  }


  %typemap(directorout,fragment=frag) Type &DIRECTOROUT = Type

%enddef

#else

#define %ptr_directorout_typemap(asptr_meth,frag,Type...) 

#endif /* SWIG_DIRECTOR_TYPEMAPS */

/* typecheck */

%define %ptr_typecheck_typemap(check,asptr_meth,frag,Type...)
%typemap(typecheck,noblock=1,precedence=check,fragment=frag) Type * {
  int res = asptr_meth($input, (Type**)(0));
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,noblock=1,precedence=check,fragment=frag) Type, const Type& {  
  int res = asptr_meth($input, (Type**)(0));
  $1 = SWIG_CheckState(res);
}
%enddef


/*---------------------------------------------------------------------
 * typemap definition for types with asptr method
 *---------------------------------------------------------------------*/

%define %typemaps_asptr(CheckCode, AsPtrMeth, AsPtrFrag, Type...)
  %fragment(SWIG_AsVal_frag(Type),"header",fragment=SWIG_AsPtr_frag(Type)) {
    SWIGINTERNINLINE int
    SWIG_AsVal(Type)(SWIG_Object obj, Type *val)
    {
      Type *v = (Type *)0;
      int res = SWIG_AsPtr(Type)(obj, &v);
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
  %ptr_in_typemap(%arg(AsPtrMeth), %arg(AsPtrFrag), Type);
  %ptr_varin_typemap(%arg(AsPtrMeth), %arg(AsPtrFrag), Type);
  %ptr_directorout_typemap(%arg(AsPtrMeth), %arg(AsPtrFrag), Type);
  %ptr_typecheck_typemap(%arg(CheckCode), %arg(AsPtrMeth),%arg(AsPtrFrag), Type);
  %ptr_input_typemap(%arg(CheckCode),%arg(AsPtrMeth),%arg(AsPtrFrag),Type);
%enddef

/*---------------------------------------------------------------------
 * typemap definition for types with asptr/from methods
 *---------------------------------------------------------------------*/

%define %typemaps_asptrfrom(CheckCode, AsPtrMeth, FromMeth, AsPtrFrag, FromFrag, Type...)
  %typemaps_asptr(%arg(CheckCode), %arg(AsPtrMeth), %arg(AsPtrFrag), Type)
  %typemaps_from(%arg(FromMeth), %arg(FromFrag), Type);
  %value_output_typemap(%arg(FromMeth), %arg(FromFrag), Type);
  %ptr_inout_typemap(Type);
%enddef

/*---------------------------------------------------------------------
 * typemap definition for types  with for 'normalized' asptr/from methods
 *---------------------------------------------------------------------*/

%define %typemaps_asptrfromn(CheckCode, Type...)
%typemaps_asptrfrom(%arg(CheckCode),
		   %arg(SWIG_AsPtr(Type)), 
		   %arg(SWIG_From(Type)), 
		   %arg(SWIG_AsPtr_frag(Type)), 
		   %arg(SWIG_From_frag(Type)), 
		   Type);
%enddef
