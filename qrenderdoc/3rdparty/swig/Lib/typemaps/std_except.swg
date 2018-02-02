%include <typemaps/exception.swg>

/* 
   Mark all of std exception classes as "exception classes" via
   the "exceptionclass" feature.
   
   If needed, you can disable it by using %noexceptionclass.
*/

%define %std_exception_map(Exception, Code)
  %exceptionclass  Exception; 
#if !defined(SWIG_STD_EXCEPTIONS_AS_CLASSES)
  %typemap(throws,noblock=1) Exception {
    SWIG_exception_fail(Code, $1.what());
  }
  %ignore Exception;
  struct Exception {
  };
#endif
%enddef

namespace std {
  %std_exception_map(bad_cast,           SWIG_TypeError);
  %std_exception_map(bad_exception,      SWIG_SystemError);
  %std_exception_map(domain_error,       SWIG_ValueError);
  %std_exception_map(exception,          SWIG_SystemError);
  %std_exception_map(invalid_argument,   SWIG_ValueError);
  %std_exception_map(length_error,       SWIG_IndexError);
  %std_exception_map(logic_error,        SWIG_RuntimeError);
  %std_exception_map(out_of_range,       SWIG_IndexError);
  %std_exception_map(overflow_error,     SWIG_OverflowError);
  %std_exception_map(range_error,        SWIG_OverflowError);
  %std_exception_map(runtime_error,      SWIG_RuntimeError);
  %std_exception_map(underflow_error,    SWIG_OverflowError);
}

%include <std/std_except.i>
