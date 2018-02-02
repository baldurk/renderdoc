
// Documentation for use with the autodoc feature.

#ifdef SWIG_DOC_DOXYGEN_STYLE
%typemap(doc) SWIGTYPE "@param $1_name $1_type";
%typemap(doc) SWIGTYPE * "@param $1_name $1_type";
%typemap(doc) const SWIGTYPE & "@param $1_name $1_type";
%typemap(doc) const SWIGTYPE && "@param $1_name $1_type";
%typemap(doc) enum SWIGTYPE "@param $1_name enum $1_type";

%typemap(doc) SWIGTYPE *INOUT, SWIGTYPE &INOUT "@param $1_name $1_type (input/output)";
%typemap(doc) SWIGTYPE *INPUT, SWIGTYPE &INPUT "@param $1_name $1_type (input)";
%typemap(doc) SWIGTYPE *OUTPUT, SWIGTYPE &OUTPUT "@param $1_name $1_type (output)";
#else
%typemap(doc) SWIGTYPE "$1_name: $1_type";
%typemap(doc) SWIGTYPE * "$1_name: $1_type";
%typemap(doc) const SWIGTYPE & "$1_name: $1_type";
%typemap(doc) const SWIGTYPE && "$1_name: $1_type";
%typemap(doc) enum SWIGTYPE "$1_name: enum $1_type";

%typemap(doc) SWIGTYPE *INOUT, SWIGTYPE &INOUT "$1_name: $1_type (input/output)";
%typemap(doc) SWIGTYPE *INPUT, SWIGTYPE &INPUT "$1_name: $1_type (input)";
%typemap(doc) SWIGTYPE *OUTPUT, SWIGTYPE &OUTPUT "$1_name: $1_type (output)";
#endif
