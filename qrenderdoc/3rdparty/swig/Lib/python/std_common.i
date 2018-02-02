%include <std/std_except.i>
%include <pystdcommon.swg>


/*
  Generate the traits for a 'primitive' type, such as 'double',
  for which the SWIG_AsVal and SWIG_From methods are already defined.
*/

%define %traits_ptypen(Type...)
  %fragment(SWIG_Traits_frag(Type),"header",
	    fragment=SWIG_AsVal_frag(Type),
	    fragment=SWIG_From_frag(Type),
	    fragment="StdTraits") {
namespace swig {
  template <> struct traits< Type > {
    typedef value_category category;
    static const char* type_name() { return  #Type; }
  };
  template <>  struct traits_asval< Type > {
    typedef Type value_type;
    static int asval(PyObject *obj, value_type *val) {
      return SWIG_AsVal(Type)(obj, val);
    }
  };
  template <>  struct traits_from< Type > {
    typedef Type value_type;
    static PyObject *from(const value_type& val) {
      return SWIG_From(Type)(val);
    }
  };
}
}
%enddef

/* Traits for enums. This is bit of a sneaky trick needed because a generic template specialization of enums
   is not possible (unless using template meta-programming which SWIG doesn't support because of the explicit
   instantiations required using %template). The STL containers define the 'front' method and the typemap
   below is used whenever the front method is wrapped returning an enum. This typemap simply picks up the
   standard enum typemap, but additionally drags in a fragment containing the traits_asval and traits_from
   required in the generated code for enums. */

%define %traits_enum(Type...)
  %fragment("SWIG_Traits_enum_"{Type},"header",
	    fragment=SWIG_AsVal_frag(int),
	    fragment=SWIG_From_frag(int),
	    fragment="StdTraits") {
namespace swig {
  template <>  struct traits_asval< Type > {
    typedef Type value_type;
    static int asval(PyObject *obj, value_type *val) {
      return SWIG_AsVal(int)(obj, (int *)val);
    }
  };
  template <>  struct traits_from< Type > {
    typedef Type value_type;
    static PyObject *from(const value_type& val) {
      return SWIG_From(int)((int)val);
    }
  };
}
}
%typemap(out, fragment="SWIG_Traits_enum_"{Type}) const enum SWIGTYPE& front %{$typemap(out, const enum SWIGTYPE&)%}
%enddef


%include <std/std_common.i>

//
// Generates the traits for all the known primitive
// C++ types (int, double, ...)
//
%apply_cpptypes(%traits_ptypen);

