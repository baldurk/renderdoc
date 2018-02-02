/* -----------------------------------------------------------------------------
 * cdata.swg
 *
 * This library file contains macros for manipulating raw C data as strings.
 * ----------------------------------------------------------------------------- */

%{
typedef struct SWIGCDATA {
    char *data;
    size_t   len;
} SWIGCDATA;
%}

/* -----------------------------------------------------------------------------
 * Typemaps for returning binary data
 * ----------------------------------------------------------------------------- */

%typemap(out,noblock=1,fragment="SWIG_FromCharPtrAndSize") SWIGCDATA {
  %set_output(SWIG_FromCharPtrAndSize($1.data,$1.len));
}
%typemap(in) (const void *indata, size_t inlen) = (char *STRING, size_t SIZE);


/* -----------------------------------------------------------------------------
 * %cdata(TYPE [, NAME]) 
 *
 * Convert raw C data to a binary string.
 * ----------------------------------------------------------------------------- */

%define %cdata(TYPE,NAME...)

%insert("header") {
#ifdef __cplusplus
extern "C"  {    
#endif
#if #NAME == ""
static SWIGCDATA cdata_##TYPE(TYPE *ptr, size_t nelements)
#else
static SWIGCDATA cdata_##NAME(TYPE *ptr, size_t nelements)
#endif
{
  SWIGCDATA d;
  d.data = (char *) ptr;
#if #TYPE != "void"
  d.len  = nelements*sizeof(TYPE);
#else
  d.len  = nelements;
#endif
   return d;
}
#ifdef __cplusplus
}
#endif 
}

#ifdef __cplusplus
extern "C"
#endif
#if #NAME == ""
SWIGCDATA cdata_##TYPE(TYPE *ptr, size_t nelements = 1);
#else
SWIGCDATA cdata_##NAME(TYPE *ptr, size_t nelements = 1);
#endif
%enddef

%rename(cdata) ::cdata_void(void *ptr, size_t nelements = 1);

%cdata(void);

/* Memory move function. Due to multi-argument typemaps this appears to be wrapped as
void memmove(void *data, const char *s); */
void memmove(void *data, const void *indata, size_t inlen);




