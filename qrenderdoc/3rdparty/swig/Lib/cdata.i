/* -----------------------------------------------------------------------------
 * cdata.i
 *
 * SWIG library file containing macros for manipulating raw C data as strings.
 * ----------------------------------------------------------------------------- */

%{
typedef struct SWIGCDATA {
    char *data;
    int   len;
} SWIGCDATA;
%}

/* -----------------------------------------------------------------------------
 * Typemaps for returning binary data
 * ----------------------------------------------------------------------------- */

#if SWIGGUILE
%typemap(out) SWIGCDATA {
   $result = scm_from_locale_stringn($1.data,$1.len);
}
%typemap(in) (const void *indata, int inlen) = (char *STRING, int LENGTH);

#elif SWIGCHICKEN

%typemap(out) SWIGCDATA {
  C_word *string_space = C_alloc(C_SIZEOF_STRING($1.len));
  $result = C_string(&string_space, $1.len, $1.data);
}
%typemap(in) (const void *indata, int inlen) = (char *STRING, int LENGTH);

#elif SWIGPHP5

%typemap(out) SWIGCDATA {
  ZVAL_STRINGL($result, $1.data, $1.len, 1);
}
%typemap(in) (const void *indata, int inlen) = (char *STRING, int LENGTH);

#elif SWIGPHP7

%typemap(out) SWIGCDATA {
  ZVAL_STRINGL($result, $1.data, $1.len);
}
%typemap(in) (const void *indata, int inlen) = (char *STRING, int LENGTH);

#elif SWIGJAVA

%apply (char *STRING, int LENGTH) { (const void *indata, int inlen) }
%typemap(jni) SWIGCDATA "jbyteArray"
%typemap(jtype) SWIGCDATA "byte[]"
%typemap(jstype) SWIGCDATA "byte[]"
%fragment("SWIG_JavaArrayOutCDATA", "header") {
static jbyteArray SWIG_JavaArrayOutCDATA(JNIEnv *jenv, char *result, jsize sz) {
  jbyte *arr;
  int i;
  jbyteArray jresult = JCALL1(NewByteArray, jenv, sz);
  if (!jresult)
    return NULL;
  arr = JCALL2(GetByteArrayElements, jenv, jresult, 0);
  if (!arr)
    return NULL;
  for (i=0; i<sz; i++)
    arr[i] = (jbyte)result[i];
  JCALL3(ReleaseByteArrayElements, jenv, jresult, arr, 0);
  return jresult;
}
}
%typemap(out, fragment="SWIG_JavaArrayOutCDATA") SWIGCDATA
%{$result = SWIG_JavaArrayOutCDATA(jenv, (char *)$1.data, $1.len); %}
%typemap(javaout) SWIGCDATA {
    return $jnicall;
  }

#endif


/* -----------------------------------------------------------------------------
 * %cdata(TYPE [, NAME]) 
 *
 * Convert raw C data to a binary string.
 * ----------------------------------------------------------------------------- */

%define %cdata(TYPE,NAME...)

%insert("header") {
#if #NAME == ""
static SWIGCDATA cdata_##TYPE(TYPE *ptr, int nelements) {
#else
static SWIGCDATA cdata_##NAME(TYPE *ptr, int nelements) {
#endif
   SWIGCDATA d;
   d.data = (char *) ptr;
#if #TYPE != "void"
   d.len  = nelements*sizeof(TYPE);
#else
   d.len  = nelements;
#endif
   return d;
}
}

%typemap(default) int nelements "$1 = 1;"

#if #NAME == ""
SWIGCDATA cdata_##TYPE(TYPE *ptr, int nelements);
#else
SWIGCDATA cdata_##NAME(TYPE *ptr, int nelements);
#endif
%enddef

%typemap(default) int nelements;

%rename(cdata) ::cdata_void(void *ptr, int nelements);

%cdata(void);

/* Memory move function. Due to multi-argument typemaps this appears to be wrapped as
void memmove(void *data, const char *s); */
void memmove(void *data, const void *indata, int inlen);
