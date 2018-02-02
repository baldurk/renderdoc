/* -----------------------------------------------------------------------------
 * exceptions.swg
 *
 * This SWIG library file provides language independent exception handling
 * ----------------------------------------------------------------------------- */

%include <typemaps/swigmacros.swg>


/* macros for error manipulation */
#define %nullref_fmt()                     "invalid null reference "		   
#define %varfail_fmt(_type,_name)          "in variable '"`_name`"' of type '"`_type`"'"
#ifndef %argfail_fmt
#define %argfail_fmt(_type,_name,_argn)    "in method '" `_name` "', argument " `_argn`" of type '" `_type`"'"
#endif
#define %outfail_fmt(_type)                "in output value of type '"_type"'"
#ifndef	%argnullref_fmt
#define %argnullref_fmt(_type,_name,_argn) %nullref_fmt() %argfail_fmt(_type, _name, _argn)
#endif  
#define %varnullref_fmt(_type,_name)       %nullref_fmt() %varfail_fmt(_type, _name)  		   
#define %outnullref_fmt(_type)             %nullref_fmt() %outfail_fmt(_type)         

/* setting an error */
#define %error(code,msg...)               SWIG_Error(code, msg)
#define %type_error(msg...)               SWIG_Error(SWIG_TypeError,  msg)



%insert("runtime") {

%define_as(SWIG_exception_fail(code, msg), %block(%error(code, msg); SWIG_fail))

%define_as(SWIG_contract_assert(expr, msg), if (!(expr)) { %error(SWIG_RuntimeError, msg); SWIG_fail; } else)

}

#ifdef __cplusplus
/*
  You can use the SWIG_CATCH_STDEXCEPT macro with the %exception
  directive as follows:

  %exception {
    try {
      $action
    }
    catch (my_except& e) {
      ...
    }
    SWIG_CATCH_STDEXCEPT // catch std::exception
    catch (...) {
     SWIG_exception_fail(SWIG_UnknownError, "Unknown exception");
    }
  }
*/

%fragment("<stdexcept>");

%define SWIG_CATCH_STDEXCEPT
  /* catching std::exception  */
  catch (std::invalid_argument& e) {
    SWIG_exception_fail(SWIG_ValueError, e.what() );
  } catch (std::domain_error& e) {
    SWIG_exception_fail(SWIG_ValueError, e.what() );
  } catch (std::overflow_error& e) {
    SWIG_exception_fail(SWIG_OverflowError, e.what() );
  } catch (std::out_of_range& e) {
    SWIG_exception_fail(SWIG_IndexError, e.what() );
  } catch (std::length_error& e) {
    SWIG_exception_fail(SWIG_IndexError, e.what() );
  } catch (std::runtime_error& e) {
    SWIG_exception_fail(SWIG_RuntimeError, e.what() );
  } catch (std::exception& e) {
    SWIG_exception_fail(SWIG_SystemError, e.what() );
  }
%enddef
%define SWIG_CATCH_UNKNOWN
  catch (std::exception& e) {
    SWIG_exception_fail(SWIG_SystemError, e.what() );
  }
  catch (...) {
    SWIG_exception_fail(SWIG_UnknownError, "unknown exception");
  }
%enddef


#endif /* __cplusplus */
