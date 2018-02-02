#if defined(SWIGJAVA) || defined(SWIGCSHARP)
#error "do not use this version of std_except.i"
#endif

%{
#include <typeinfo>
#include <stdexcept>
%}

#if defined(SWIG_STD_EXCEPTIONS_AS_CLASSES)

namespace std {
  struct exception 
  {
    virtual ~exception() throw();
    virtual const char* what() const throw();
  };

  struct bad_cast : exception 
  {
  };

  struct bad_exception : exception 
  {
  };

  struct logic_error : exception 
  {
    logic_error(const string& msg);
  };

  struct domain_error : logic_error 
  {
    domain_error(const string& msg);
  };

  struct invalid_argument : logic_error 
  {
    invalid_argument(const string& msg);
  };

  struct length_error : logic_error 
  {
    length_error(const string& msg);
  };

  struct out_of_range : logic_error 
  {
    out_of_range(const string& msg);
  };

  struct runtime_error : exception 
  {
    runtime_error(const string& msg);
  };

  struct range_error : runtime_error 
  {
    range_error(const string& msg);
  };

  struct overflow_error : runtime_error 
  {
    overflow_error(const string& msg);
  };

  struct underflow_error : runtime_error 
  {
    underflow_error(const string& msg);
  };
}

#endif
