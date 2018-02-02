#ifdef __cplusplus

%{
#include <cwchar>
%}

#else

%{
#include <wchar.h>
%}

#endif

%types(wchar_t *);
%include <pywstrings.swg>

/*
  Enable swig wchar support.
*/
#define SWIG_WCHAR
