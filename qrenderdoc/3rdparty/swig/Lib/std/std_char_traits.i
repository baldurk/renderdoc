%include <std_common.i>
#if defined(SWIG_WCHAR)
%include <wchar.i>
#endif

namespace std 
{
  
  /// 21.1.2 Basis for explicit _Traits specialization 
  /// NB: That for any given actual character type this definition is
  /// probably wrong.
  template<class _CharT>
  struct char_traits
  {
  };


  /// 21.1.4  char_traits specializations
  template<>
  struct char_traits<char> {
    typedef char 		char_type;
    typedef int 	        int_type;
    typedef streampos 	pos_type;
    typedef streamoff 	off_type;
    typedef mbstate_t 	state_type;

    static void 
    assign(char_type& __c1, const char_type& __c2);
    
    static bool 
    eq(const char_type& __c1, const char_type& __c2);

    static bool 
    lt(const char_type& __c1, const char_type& __c2);

    static int 
    compare(const char_type* __s1, const char_type* __s2, size_t __n);

    static size_t
    length(const char_type* __s);

    static const char_type* 
    find(const char_type* __s, size_t __n, const char_type& __a);

    static char_type* 
    move(char_type* __s1, const char_type* __s2, size_t __n);

    static char_type* 
    copy(char_type* __s1, const char_type* __s2, size_t __n);

    static char_type* 
    assign(char_type* __s, size_t __n, char_type __a);

    static char_type 
    to_char_type(const int_type& __c);

    // To keep both the byte 0xff and the eof symbol 0xffffffff
    // from ending up as 0xffffffff.
    static int_type 
    to_int_type(const char_type& __c);

    static bool 
    eq_int_type(const int_type& __c1, const int_type& __c2);

    static int_type 
    eof() ;

    static int_type 
    not_eof(const int_type& __c);
  };


#if defined(SWIG_WCHAR)
  template<>
  struct char_traits<wchar_t>
  {
    typedef wchar_t 		char_type;
    typedef wint_t 		int_type;
    typedef streamoff 	off_type;
    typedef wstreampos 	pos_type;
    typedef mbstate_t 	state_type;
      
    static void 
    assign(char_type& __c1, const char_type& __c2);

    static bool 
    eq(const char_type& __c1, const char_type& __c2);

    static bool 
    lt(const char_type& __c1, const char_type& __c2);

    static int 
    compare(const char_type* __s1, const char_type* __s2, size_t __n);

    static size_t
    length(const char_type* __s);

    static const char_type* 
    find(const char_type* __s, size_t __n, const char_type& __a);

    static char_type* 
    move(char_type* __s1, const char_type* __s2, int_type __n);

    static char_type* 
    copy(char_type* __s1, const char_type* __s2, size_t __n);

    static char_type* 
    assign(char_type* __s, size_t __n, char_type __a);

    static char_type 
    to_char_type(const int_type& __c) ;

    static int_type 
    to_int_type(const char_type& __c) ;

    static bool 
    eq_int_type(const int_type& __c1, const int_type& __c2);

    static int_type 
    eof() ;

    static int_type 
    not_eof(const int_type& __c);
  };
#endif
}

namespace std {
#ifndef SWIG_STL_WRAP_TRAITS
%template() char_traits<char>;
#if defined(SWIG_WCHAR)
%template() char_traits<wchar_t>;
#endif
#else
%template(char_traits_c) char_traits<char>;
#if defined(SWIG_WCHAR)
%template(char_traits_w) char_traits<wchar_t>;
#endif
#endif
}
