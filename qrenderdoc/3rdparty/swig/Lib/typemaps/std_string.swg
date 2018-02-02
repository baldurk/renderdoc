//
// String
//


#ifndef SWIG_STD_BASIC_STRING
#define SWIG_STD_STRING

%include <typemaps/std_strings.swg>

%fragment("<string>");

namespace std
{
  %naturalvar string;
  class string;
}

%typemaps_std_string(std::string, char, SWIG_AsCharPtrAndSize, SWIG_FromCharPtrAndSize, %checkcode(STDSTRING));

#else

%include <std/std_string.i>

#endif
