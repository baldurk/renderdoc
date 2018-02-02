%include <typemaps/wstring.swg>

#ifndef SWIG_STD_BASIC_STRING
#define SWIG_STD_WSTRING

%include <typemaps/std_strings.swg>

%{
#include <cwchar>
%}
%fragment("<string>");

namespace std
{
  %naturalvar wstring;
  class wstring;
}

%typemaps_std_string(std::wstring, wchar_t, SWIG_AsWCharPtrAndSize, SWIG_FromWCharPtrAndSize, %checkcode(STDUNISTRING));


#else

%include <std/std_wstring.i>

#endif
