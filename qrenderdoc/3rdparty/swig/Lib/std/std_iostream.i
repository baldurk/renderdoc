/* 
   For wchar support, you need to include the wchar.i file
   before this file, ie:
   
   %include <wchar.i>
   %include <std_iostream.i>

   or equivalently, just include

   %include <std_wiostream.i>
*/

%include <std_ios.i>
%include <std_basic_string.i>
%include <std_string.i>
#if defined(SWIG_WCHAR)
%include <std_wstring.i>
#endif

%{
#include <iostream>
%}


namespace std
{
  // 27.6.2.1 Template class basic_ostream
  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_ostream : virtual public basic_ios<_CharT, _Traits>
  {
  public:
    // Types (inherited from basic_ios (27.4.4)):
    typedef _CharT                     		char_type;
    typedef typename _Traits::int_type 		int_type;
    typedef typename _Traits::pos_type 		pos_type;
    typedef typename _Traits::off_type 		off_type;
    typedef _Traits                    		traits_type;
      
    // 27.6.2.2 Constructor/destructor:
    explicit 
    basic_ostream(basic_streambuf<_CharT, _Traits>* __sb);

    virtual 
    ~basic_ostream();
    
    // 27.6.2.5 Formatted output:
    // 27.6.2.5.3  basic_ostream::operator<<
    basic_ostream<_CharT, _Traits>&
    operator<<(basic_ostream<_CharT, _Traits>& (*__pf)(basic_ostream<_CharT, _Traits>&));

      
    basic_ostream<_CharT, _Traits>&
    operator<<(basic_ios<_CharT, _Traits>& (*__pf)(basic_ios<_CharT, _Traits>&));


    basic_ostream<_CharT, _Traits>&
    operator<<(ios_base& (*__pf) (ios_base&));
    
    // 27.6.2.5.2 Arithmetic Inserters

    basic_ostream<_CharT, _Traits>& 
    operator<<(long __n);
    
    basic_ostream<_CharT, _Traits>& 
    operator<<(unsigned long __n);
    
    basic_ostream<_CharT, _Traits>& 
    operator<<(bool __n);
    
    basic_ostream<_CharT, _Traits>& 
    operator<<(short __n);

    basic_ostream<_CharT, _Traits>& 
    operator<<(unsigned short __n);

    basic_ostream<_CharT, _Traits>& 
    operator<<(int __n);

    basic_ostream<_CharT, _Traits>& 
    operator<<(unsigned int __n);

    basic_ostream<_CharT, _Traits>& 
    operator<<(long long __n);

    basic_ostream<_CharT, _Traits>& 
    operator<<(unsigned long long __n);

    basic_ostream<_CharT, _Traits>& 
    operator<<(double __f);

    basic_ostream<_CharT, _Traits>& 
    operator<<(float __f);

    basic_ostream<_CharT, _Traits>& 
    operator<<(long double __f);

    basic_ostream<_CharT, _Traits>& 
    operator<<(const void* __p);

    basic_ostream<_CharT, _Traits>& 
    operator<<(basic_streambuf<_CharT, _Traits>* __sb);

    %extend {
      std::basic_ostream<_CharT, _Traits >& 
	operator<<(const std::basic_string<_CharT,_Traits, std::allocator<_CharT> >& s)
	{
	  *self << s;
	  return *self;
	}
    }

    // Unformatted output:
    basic_ostream<_CharT, _Traits>& 
    put(char_type __c);

    basic_ostream<_CharT, _Traits>& 
    write(const char_type* __s, streamsize __n);

    basic_ostream<_CharT, _Traits>& 
    flush();

    // Seeks:
    pos_type 
    tellp();

    basic_ostream<_CharT, _Traits>& 
    seekp(pos_type);

    basic_ostream<_CharT, _Traits>& 
    seekp(off_type, ios_base::seekdir);

  };

  // 27.6.1.1 Template class basic_istream
  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_istream : virtual public basic_ios<_CharT, _Traits>
  {
  public:
    // Types (inherited from basic_ios (27.4.4)):
    typedef _CharT                     		char_type;
    typedef typename _Traits::int_type 		int_type;
    typedef typename _Traits::pos_type 		pos_type;
    typedef typename _Traits::off_type 		off_type;
    typedef _Traits                    		traits_type;


  public:
    // 27.6.1.1.1 Constructor/destructor:
    explicit 
    basic_istream(basic_streambuf<_CharT, _Traits>* __sb);

    virtual 
    ~basic_istream();

    // 27.6.1.2.3 basic_istream::operator>>
    basic_istream<_CharT, _Traits>&
    operator>>(basic_istream<_CharT, _Traits>& (*__pf)(basic_istream<_CharT, _Traits>&));
    
    basic_istream<_CharT, _Traits>&
    operator>>(basic_ios<_CharT, _Traits>& (*__pf)(basic_ios<_CharT, _Traits>&));
    
    basic_istream<_CharT, _Traits>&
    operator>>(ios_base& (*__pf)(ios_base&));
      
    // 27.6.1.2.2 Arithmetic Extractors
    basic_istream<_CharT, _Traits>& 
    operator>>(bool& __n);
      
    basic_istream<_CharT, _Traits>& 
    operator>>(short& __n);
      
    basic_istream<_CharT, _Traits>& 
    operator>>(unsigned short& __n);

    basic_istream<_CharT, _Traits>& 
    operator>>(int& __n);
      
    basic_istream<_CharT, _Traits>& 
    operator>>(unsigned int& __n);

    basic_istream<_CharT, _Traits>& 
    operator>>(long& __n);
      
    basic_istream<_CharT, _Traits>& 
    operator>>(unsigned long& __n);

    basic_istream<_CharT, _Traits>& 
    operator>>(long long& __n);

    basic_istream<_CharT, _Traits>& 
    operator>>(unsigned long long& __n);

    basic_istream<_CharT, _Traits>& 
    operator>>(float& __f);

    basic_istream<_CharT, _Traits>& 
    operator>>(double& __f);

    basic_istream<_CharT, _Traits>& 
    operator>>(long double& __f);

    basic_istream<_CharT, _Traits>& 
    operator>>(void*& __p);

    basic_istream<_CharT, _Traits>& 
    operator>>(basic_streambuf<_CharT, _Traits>* __sb);
      
    // 27.6.1.3 Unformatted input:
    inline streamsize 
    gcount(void) const;
      
    int_type 
    get(void);

    basic_istream<_CharT, _Traits>& 
    get(char_type& __c);

    basic_istream<_CharT, _Traits>& 
    get(char_type* __s, streamsize __n, char_type __delim);

    inline basic_istream<_CharT, _Traits>& 
    get(char_type* __s, streamsize __n);

    basic_istream<_CharT, _Traits>&
    get(basic_streambuf<_CharT, _Traits>& __sb, char_type __delim);

    inline basic_istream<_CharT, _Traits>&
    get(basic_streambuf<_CharT, _Traits>& __sb);

    basic_istream<_CharT, _Traits>& 
    getline(char_type* __s, streamsize __n, char_type __delim);

    inline basic_istream<_CharT, _Traits>& 
    getline(char_type* __s, streamsize __n);

    basic_istream<_CharT, _Traits>& 
    ignore(streamsize __n = 1, int_type __delim = _Traits::eof());
      
    int_type 
    peek(void);
      
    basic_istream<_CharT, _Traits>& 
    read(char_type* __s, streamsize __n);

    streamsize 
    readsome(char_type* __s, streamsize __n);
      
    basic_istream<_CharT, _Traits>& 
    putback(char_type __c);

    basic_istream<_CharT, _Traits>& 
    unget(void);

    int 
    sync(void);

    pos_type 
    tellg(void);

    basic_istream<_CharT, _Traits>& 
    seekg(pos_type);

    basic_istream<_CharT, _Traits>& 
    seekg(off_type, ios_base::seekdir);
  };  

  // 27.6.1.5 Template class basic_iostream
  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_iostream
    : public basic_istream<_CharT, _Traits>, 
      public basic_ostream<_CharT, _Traits>
  {
  public:
    typedef _CharT                     		char_type;
    typedef typename _Traits::int_type 		int_type;
    typedef typename _Traits::pos_type 		pos_type;
    typedef typename _Traits::off_type 		off_type;
    typedef _Traits                    		traits_type;

    explicit 
    basic_iostream(basic_streambuf<_CharT, _Traits>* __sb);

    virtual 
    ~basic_iostream();    
  };

  typedef basic_ostream<char> ostream ;
  typedef basic_istream<char> istream;
  typedef basic_iostream<char> iostream;

  extern istream cin;
  extern ostream cout;
  extern ostream cerr;
  extern ostream clog;

#if defined(SWIG_WCHAR)
  typedef basic_ostream<wchar_t>  wostream;
  typedef basic_istream<wchar_t>  wistream;
  typedef basic_iostream<wchar_t> wiostream;

  extern wistream wcin;
  extern wostream wcout;
  extern wostream wcerr;
  extern wostream wclog;
#endif

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  std::basic_ostream<_CharT, _Traits>& 
  endl(std::basic_ostream<_CharT, _Traits>&);

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  std::basic_ostream<_CharT, _Traits>& 
  ends(std::basic_ostream<_CharT, _Traits>&);

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  std::basic_ostream<_CharT, _Traits>& 
  flush(std::basic_ostream<_CharT, _Traits>&);
}

namespace std {
  %template(ostream) basic_ostream<char>;
  %template(istream) basic_istream<char>;
  %template(iostream) basic_iostream<char>;

  %template(endl) endl<char, std::char_traits<char> >;
  %template(ends) ends<char, std::char_traits<char> >;
  %template(flush) flush<char, std::char_traits<char> >;

#if defined(SWIG_WCHAR)
  %template(wostream) basic_ostream<wchar_t>;
  %template(wistream) basic_istream<wchar_t>;
  %template(wiostream) basic_iostream<wchar_t>;  

  %template(wendl) endl<wchar_t, std::char_traits<wchar_t> >;
  %template(wends) ends<wchar_t, std::char_traits<wchar_t> >;
  %template(wflush) flush<wchar_t, std::char_traits<wchar_t> >;  
#endif
}

