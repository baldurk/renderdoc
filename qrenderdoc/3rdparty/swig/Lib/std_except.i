/* -----------------------------------------------------------------------------
 * std_except.i
 *
 * SWIG library file with typemaps to handle and throw STD exceptions in a
 * language and STL independent way, i.e., the target language doesn't
 * require to support STL but only the 'exception.i' mechanism.
 *
 * These typemaps are used when methods are declared with an STD
 * exception specification, such as
 *
 *   size_t at() const throw (std::out_of_range);
 *
 * The typemaps here are based on the language independent
 * 'exception.i' library. If that is working in your target language,
 * this file will work.
 * 
 * If the target language doesn't implement a robust 'exception.i'
 * mechanism, or you prefer other ways to map the STD exceptions, write
 * a new std_except.i file in the target library directory.
 * ----------------------------------------------------------------------------- */

#if defined(SWIGJAVA) || defined(SWIGCSHARP) || defined(SWIGGUILE) || defined(SWIGUTL) || defined(SWIGD)
#error "This version of std_except.i should not be used"
#endif

%{
#include <typeinfo>
#include <stdexcept>
%}

%include <exception.i>


%define %std_exception_map(Exception, Code)
  %typemap(throws,noblock=1) Exception {
    SWIG_exception(Code, $1.what());
  }
  %ignore Exception;
  struct Exception {
  };
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

