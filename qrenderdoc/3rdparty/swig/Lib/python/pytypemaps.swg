/* ------------------------------------------------------------
 *  Typemap specializations for Python
 * ------------------------------------------------------------ */

/* ------------------------------------------------------------
 *  Fragment section
 * ------------------------------------------------------------ */
#ifdef SWIG_PYTHON_LEGACY_BOOL
// Default prior to SWIG 3.0.0
#undef SWIG_TYPECHECK_BOOL
%define SWIG_TYPECHECK_BOOL             10000 %enddef
#endif

/* Include fundamental fragment definitions */
%include <typemaps/fragments.swg>

/* Look for user fragments file. */
%include <pyfragments.swg>

/* Python fragments for fundamental types */
%include <pyprimtypes.swg>

/* Python fragments for char* strings */
%include <pystrings.swg>

/* Backward compatibility output helper */
%fragment("t_output_helper","header") %{
#define t_output_helper SWIG_Python_AppendOutput
%}


/* ------------------------------------------------------------
 *  Unified typemap section
 * ------------------------------------------------------------ */

/* directors are supported in Python */
#ifndef SWIG_DIRECTOR_TYPEMAPS
#define SWIG_DIRECTOR_TYPEMAPS
#endif


/* Python types */
#define SWIG_Object                     PyObject *
#define VOID_Object                     SWIG_Py_Void()

/* Python allows implicit conversion */
#define %implicitconv_flag              $implicitconv 


/* Overload of the output/constant/exception/dirout handling */

/* append output */
#define SWIG_AppendOutput(result, obj)  SWIG_Python_AppendOutput(result, obj)

/* set constant */
#if defined(SWIGPYTHON_BUILTIN)
#define SWIG_SetConstant(name, obj) SWIG_Python_SetConstant(d, d == md ? public_interface : NULL, name,obj) 
#else
#define SWIG_SetConstant(name, obj) SWIG_Python_SetConstant(d, name,obj) 
#endif

/* raise */
#define SWIG_Raise(obj, type, desc)     SWIG_Python_Raise(obj, type, desc)

/* Include the unified typemap library */
%include <typemaps/swigtypemaps.swg>


/*  ------------------------------------------------------------
 *  Python extra typemaps / typemap overrides
 * ------------------------------------------------------------ */

/* Get the address of the 'python self' object */

%typemap(in,numinputs=0,noblock=1) PyObject **PYTHON_SELF {
  $1 = &$self;
}


/* Consttab, needed for callbacks, it should be removed later */

%typemap(consttab) SWIGTYPE ((*)(ANY))
{ SWIG_PY_POINTER, (char*)"$symname", 0, 0, (void *)($value), &$descriptor }
%typemap(consttab) SWIGTYPE ((* const)(ANY)) = SWIGTYPE ((*)(ANY));

%typemap(constcode) SWIGTYPE ((*)(ANY)) "";
%typemap(constcode) SWIGTYPE ((* const)(ANY)) = SWIGTYPE ((*)(ANY));


/* Smart Pointers */
%typemap(out,noblock=1) const SWIGTYPE & SMARTPOINTER  {
  $result = SWIG_NewPointerObj(%new_copy(*$1, $*ltype), $descriptor, SWIG_POINTER_OWN | %newpointer_flags);
}

%typemap(ret,noblock=1) const SWIGTYPE & SMARTPOINTER, SWIGTYPE SMARTPOINTER {
  if ($result) {
    PyObject *robj = PyObject_CallMethod($result, (char *)"__deref__", NULL);
    if (robj && !PyErr_Occurred()) {
      SwigPyObject_append((PyObject *) SWIG_Python_GetSwigThis($result), 
			  (PyObject *) SWIG_Python_GetSwigThis(robj));
      Py_DECREF(robj);
    }
  }
}

