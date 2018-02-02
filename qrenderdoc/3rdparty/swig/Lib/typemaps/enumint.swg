/* ------------------------------------------------------------
 *  Enums mapped as integer values
 * ------------------------------------------------------------ */

%apply int { enum SWIGTYPE };
%apply const int& { const enum SWIGTYPE & };
%apply const int& { const enum SWIGTYPE && };

%typemap(in,fragment=SWIG_AsVal_frag(int),noblock=1) const enum SWIGTYPE & (int val, int ecode, $basetype temp) {  
  ecode = SWIG_AsVal(int)($input, &val);
  if (!SWIG_IsOK(ecode)) {
    %argument_fail(ecode, "$type", $symname, $argnum);
  } else {
    temp = %static_cast(val,$basetype);
    $1 = &temp;
  }
}

%typemap(in,fragment=SWIG_AsVal_frag(int),noblock=1) const enum SWIGTYPE && (int val, int ecode, $basetype temp) {  
  ecode = SWIG_AsVal(int)($input, &val);
  if (!SWIG_IsOK(ecode)) {
    %argument_fail(ecode, "$type", $symname, $argnum);
  } else {
    temp = %static_cast(val,$basetype);
    $1 = &temp;
  }
}

%typemap(varin,fragment=SWIG_AsVal_frag(int),noblock=1) enum SWIGTYPE {
  if (sizeof(int) != sizeof($1)) {
    %variable_fail(SWIG_AttributeError,"$type", "arch, read-only $name");
  }  else {
    int ecode = SWIG_AsVal(int)($input, %reinterpret_cast(&$1,int*));
    if (!SWIG_IsOK(ecode)) {
      %variable_fail(ecode, "$type", "$name");
    }
  }
}

