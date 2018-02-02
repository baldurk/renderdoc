/* ------------------------------------------------------------
 * Language Object *  - Just pass straight through unmodified
 * ------------------------------------------------------------ */

%typemap(in)   SWIG_Object "$1 = $input;";

%typemap(in,noblock=1)   SWIG_Object const & ($*ltype temp)
{
  temp = %static_cast($input, $*ltype);
  $1 = &temp;
}

%typemap(out,noblock=1) SWIG_Object {
  %set_output($1);
}

%typemap(out,noblock=1)  SWIG_Object const & {
  %set_output(*$1);
}

%typecheck(SWIG_TYPECHECK_SWIGOBJECT) SWIG_Object "$1 = ($input != 0);";

%typemap(throws,noblock=1) SWIG_Object {
  %raise($1, "$type", 0);
}

%typemap(constcode,noblock=1) SWIG_Object {
  %set_constant("$symname", $value);
}

#if defined(SWIG_DIRECTOR_TYPEMAPS)

%typemap(directorin) SWIG_Object "$input = $1;";
%typemap(directorout) SWIG_Object "$result = $input;";

#endif /* SWIG_DIRECTOR_TYPEMAPS */

