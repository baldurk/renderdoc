%include <typemaps/cstrings.swg>

%typemaps_cstring(%cstring,
		 char,
		 SWIG_AsCharPtr,
		 SWIG_AsCharPtrAndSize,
		 SWIG_FromCharPtr,
		 SWIG_FromCharPtrAndSize);

