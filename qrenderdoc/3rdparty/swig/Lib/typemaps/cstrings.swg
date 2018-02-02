/* -----------------------------------------------------------------------------
 * cstrings.swg
 *
 * This file provides typemaps and macros for dealing with various forms
 * of C character string handling.   The primary use of this module
 * is in returning character data that has been allocated or changed in
 * some way.
 * ----------------------------------------------------------------------------- */

%define %typemaps_cstring(Name, Char,
			  SWIG_AsCharPtr,
			  SWIG_AsCharPtrAndSize,
			  SWIG_FromCharPtr,
			  SWIG_FromCharPtrAndSize)


/* %cstring_input_binary(TYPEMAP, SIZE)
 * 
 * Macro makes a function accept binary string data along with
 * a size.  For example:
 *
 *     %cstring_input_binary(Char *buff, int size);
 *     void foo(Char *buff, int size) {
 *     }
 *
 */

%define Name ## _input_binary(TYPEMAP, SIZE)                   
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize) (TYPEMAP, SIZE) 
  (int res, Char *buf = 0, size_t size = 0, int alloc = 0)  {
  res = SWIG_AsCharPtrAndSize($input, &buf, &size, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  $1 = ($1_ltype) buf;					       
  $2 = ($2_ltype) size - 1;				       
}
%typemap(freearg,noblock=1,match="in") (TYPEMAP, SIZE) {
  if (alloc$argnum == SWIG_NEWOBJ) %delete_array(buf$argnum);
}
%enddef								



/*
 * %cstring_bounded_output(TYPEMAP, MAX)
 *
 * This macro is used to return a NULL-terminated output string of
 * some maximum length.  For example:
 *
 *     %cstring_bounded_output(Char *outx, 512);
 *     void foo(Char *outx) {
 *         sprintf(outx,"blah blah\n");
 *     }
 *
 */

%define Name ## _bounded_output(TYPEMAP,MAX)        
%typemap(in,noblock=1,numinputs=0) TYPEMAP (Char temp[MAX+1])  {
  $1 = ($1_ltype) temp;
}
%typemap(freearg,match="in") TYPEMAP "";
%typemap(argout,noblock=1,fragment= #SWIG_FromCharPtr ) TYPEMAP {
  $1[MAX] = 0;  
  %append_output(SWIG_FromCharPtr($1));
}
%enddef



/*
 * %cstring_chunk_output(TYPEMAP, SIZE)
 *
 * This macro is used to return a chunk of binary string data.
 * Embedded NULLs are okay.  For example:
 *
 *     %cstring_chunk_output(Char *outx, 512);
 *     void foo(Char *outx) {
 *         memmove(outx, somedata, 512);
 *     }
 *
 */

%define Name ## _chunk_output(TYPEMAP,SIZE)           
%typemap(in,noblock=1,numinputs=0) TYPEMAP(Char temp[SIZE]) {
  $1 = ($1_ltype) temp;
}
%typemap(freearg,match="in") TYPEMAP "";
%typemap(argout,noblock=1,fragment= #SWIG_FromCharPtrAndSize) TYPEMAP {
  %append_output(SWIG_FromCharPtrAndSize($1,SIZE));
}
%enddef



/*
 * %cstring_bounded_mutable(TYPEMAP, SIZE)
 *
 * This macro is used to wrap a string that's going to mutate.
 *
 *     %cstring_bounded_mutable(Char *in, 512);
 *     void foo(in *x) {
 *         while (*x) {
 *            *x = toupper(*x);
 *            x++;
 *         }
 *     }
 *
 */


%define Name ## _bounded_mutable(TYPEMAP,MAX)                              
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize) TYPEMAP 
  (int res,Char temp[MAX+1], Char *t = 0, size_t n = 0, int alloc = 0) {  
  res = SWIG_AsCharPtrAndSize($input, &t, &n, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "TYPEMAP", $symname, $argnum);
  }
  if ( n > (size_t) MAX ) n = (size_t) MAX;
  memcpy(temp, t, sizeof(Char)*n);
  if (alloc == SWIG_NEWOBJ) %delete_array(t);
  temp[n - 1] = 0;                                                             
  $1 = ($1_ltype) temp;                                                    
}
%typemap(freearg,match="in") TYPEMAP "";
%typemap(argout,noblock=1,fragment=#SWIG_FromCharPtr) TYPEMAP {
  $1[MAX] = 0;
  %append_output(SWIG_FromCharPtr($1));
}
%enddef


/*
 * %cstring_mutable(TYPEMAP [, expansion])
 *
 * This macro is used to wrap a string that will mutate in place.
 * It may change size up to a user-defined expansion. 
 *
 *     %cstring_mutable(Char *in);
 *     void foo(in *x) {
 *         while (*x) {
 *            *x = toupper(*x);
 *            x++;
 *         }
 *     }
 *
 */

%define Name ## _mutable(TYPEMAP,EXP...)                  
%typemap(in,noblock=1,fragment=#SWIG_AsCharPtrAndSize) TYPEMAP (int res, Char *t = 0, size_t n = 0, int alloc = 0, size_t expansion = 0) {
#if #EXP != ""
  expansion += EXP;
#endif
  res = SWIG_AsCharPtrAndSize($input, &t, &n, &alloc);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "TYPEMAP", $symname, $argnum);
  }
  $1 = %new_array(n+expansion, $*1_ltype);          
  memcpy($1,t,sizeof(Char)*n);
  if (alloc == SWIG_NEWOBJ) %delete_array(t);
  $1[n-1] = 0;
}
%typemap(freearg,match="in") TYPEMAP "";
%typemap(argout,noblock=1,fragment=#SWIG_FromCharPtr) TYPEMAP { 
  %append_output(SWIG_FromCharPtr($1));
  %delete_array($1);                                  
}
%enddef


/*
 * %cstring_output_maxsize(TYPEMAP, SIZE)
 *
 * This macro returns data in a string of some user-defined size.
 *
 *     %cstring_output_maxsize(Char *outx, int max) {
 *     void foo(Char *outx, int max) {
 *         sprintf(outx,"blah blah\n");
 *     }
 */

%define Name ## _output_maxsize(TYPEMAP, SIZE)                       
%typemap(in,noblock=1,fragment=SWIG_AsVal_frag(size_t)) (TYPEMAP, SIZE) (int res, size_t size, Char *buff = 0) {   
  res = SWIG_AsVal(size_t)($input, &size);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  buff= %new_array(size+1, Char);
  $2 = %numeric_cast(size, $2_ltype);
  $1 = %static_cast(buff, $1_ltype);
}
%typemap(freearg,noblock=1,match="in") (TYPEMAP,SIZE) {
  if (buff$argnum) %delete_array(buff$argnum);
} 
%typemap(argout,noblock=1,fragment=#SWIG_FromCharPtr) (TYPEMAP,SIZE) { 
  %append_output(SWIG_FromCharPtr($1));
}
%enddef

/*
 * %cstring_output_withsize(TYPEMAP, SIZE)
 *
 * This macro is used to return Character data along with a size
 * parameter.
 *
 *     %cstring_output_withsize(Char *outx, int *max) {
 *     void foo(Char *outx, int *max) {
 *         sprintf(outx,"blah blah\n");
 *         *max = strlen(outx);  
 *     }
 */

%define Name ## _output_withsize(TYPEMAP, SIZE)                        
%typemap(in,noblock=1,fragment=SWIG_AsVal_frag(size_t)) (TYPEMAP, SIZE) (int res, size_t n, Char *buff = 0, $*2_ltype size) {    
  res = SWIG_AsVal(size_t)($input, &n);
  if (!SWIG_IsOK(res)) {
    %argument_fail(res, "(TYPEMAP, SIZE)", $symname, $argnum);
  }
  buff= %new_array(n+1, Char);
  $1 = %static_cast(buff, $1_ltype);
  size = %numeric_cast(n,$*2_ltype);
  $2 = &size;
}								       
%typemap(freearg,noblock=1,match="in")(TYPEMAP,SIZE) {
  if (buff$argnum) %delete_array(buff$argnum);
} 
%typemap(argout,noblock=1,fragment=#SWIG_FromCharPtrAndSize) (TYPEMAP,SIZE) { 
  %append_output(SWIG_FromCharPtrAndSize($1,*$2));
}
%enddef


/*
 * %cstring_output_allocate(TYPEMAP, RELEASE)
 *
 * This macro is used to return Character data that was
 * allocated with new or malloc.
 *
 *     %cstring_output_allocate(Char **outx, free($1));
 *     void foo(Char **outx) {
 *         *outx = (Char *) malloc(512);
 *         sprintf(outx,"blah blah\n");
 *     }
 */
 
%define Name ## _output_allocate(TYPEMAP, RELEASE)           
%typemap(in,noblock=1,numinputs=0) TYPEMAP($*1_ltype temp = 0) {
  $1 = &temp;
}
%typemap(freearg,match="in") TYPEMAP "";
%typemap(argout,noblock=1,fragment=#SWIG_FromCharPtr) TYPEMAP { 
  if (*$1) {
    %append_output(SWIG_FromCharPtr(*$1));
    RELEASE;					  	     
  }					  	     
}							     
%enddef


/*
 * %cstring_output_allocate_size(TYPEMAP, SIZE, RELEASE)
 *
 * This macro is used to return Character data that was
 * allocated with new or malloc.
 *
 *     %cstring_output_allocate_size(Char **outx, int *sz, free($1));
 *     void foo(Char **outx, int *sz) {
 *         *outx = (Char *) malloc(512);
 *         sprintf(outx,"blah blah\n");
 *         *sz = strlen(outx);
 *     }
 */

%define Name ## _output_allocate_size(TYPEMAP, SIZE, RELEASE)
%typemap(in,noblock=1,numinputs=0) (TYPEMAP, SIZE) ($*1_ltype temp = 0, $*2_ltype tempn) {
  $1 = &temp; $2 = &tempn;
}
%typemap(freearg,match="in") (TYPEMAP,SIZE) "";
%typemap(argout,noblock=1,fragment=#SWIG_FromCharPtrAndSize)(TYPEMAP,SIZE) {   
  if (*$1) {
    %append_output(SWIG_FromCharPtrAndSize(*$1,*$2));
    RELEASE;
  }
}
%enddef

%enddef

