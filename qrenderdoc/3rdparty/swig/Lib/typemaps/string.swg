%ensure_fragment(SWIG_AsCharPtrAndSize)
%ensure_fragment(SWIG_FromCharPtrAndSize)

%types(char *);

%fragment("SWIG_pchar_descriptor","header") {
SWIGINTERN swig_type_info*
SWIG_pchar_descriptor(void)
{
  static int init = 0;
  static swig_type_info* info = 0;
  if (!init) {
    info = SWIG_TypeQuery("_p_char");
    init = 1;
  }
  return info;
}
}

%fragment("SWIG_strnlen","header",fragment="SWIG_FromCharPtrAndSize") {
SWIGINTERN size_t
SWIG_strnlen(const char* s, size_t maxlen)
{
  const char *p;
  for (p = s; maxlen-- && *p; p++)
    ;
  return p - s;
}
}

%include <typemaps/strings.swg>
%typemaps_string(%checkcode(STRING), %checkcode(CHAR),
		 char, Char, SWIG_AsCharPtrAndSize, SWIG_FromCharPtrAndSize,
		 strlen, SWIG_strnlen,
		"<limits.h>", CHAR_MIN, CHAR_MAX)
