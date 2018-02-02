%include <typemaps/exception.swg>


%insert("runtime") {
  %define_as(SWIG_exception(code, msg), %block(%error(code, msg); SWIG_fail; ))
}
