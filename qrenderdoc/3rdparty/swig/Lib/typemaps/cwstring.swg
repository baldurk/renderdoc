%include <typemaps/cstrings.swg>
%include <typemaps/wstring.swg>

%typemaps_cstring(%cwstring,
		 wchar_t,
		 SWIG_AsWCharPtr,
		 SWIG_AsWCharPtrAndSize,
		 SWIG_FromWCharPtr,
		 SWIG_FromWCharPtrAndSize);

