
/* ------------------------------------------------------------
 * --- ANSI/Posix C/C++ types ---
 * ------------------------------------------------------------ */


#ifdef __cplusplus

%apply size_t { std::size_t };
%apply const size_t& { const std::size_t& };

%apply ptrdiff_t { std::ptrdiff_t };
%apply const ptrdiff_t& { const std::ptrdiff_t& };

#ifndef SWIG_INOUT_NODEF
%apply size_t& { std::size_t& };
%apply ptrdiff_t& { std::ptrdiff_t& };
#endif

#endif

