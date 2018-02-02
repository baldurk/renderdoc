%include <std_ios.i>
%{
#ifndef SWIG_STD_NOMODERN_STL
#include <streambuf>
#else
#include <streambuf.h>
#endif
%}

namespace std {

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_streambuf 
  {
  public:
    // Types:
    typedef _CharT 					char_type;
    typedef _Traits 					traits_type;
    typedef typename traits_type::int_type 		int_type;
    typedef typename traits_type::pos_type 		pos_type;
    typedef typename traits_type::off_type 		off_type;

  public:
    virtual 
    ~basic_streambuf();

    // Locales:
    locale 
    pubimbue(const locale &__loc);

    locale   
    getloc() const; 

    // Buffer and positioning:
    basic_streambuf<_CharT, _Traits>* 
    pubsetbuf(char_type* __s, streamsize __n);

    pos_type 
    pubseekoff(off_type __off, ios_base::seekdir __way, 
	       ios_base::openmode __mode = std::ios_base::in | std::ios_base::out);

    pos_type 
    pubseekpos(pos_type __sp,
	       ios_base::openmode __mode = std::ios_base::in | std::ios_base::out);

    int 
    pubsync() ;

    // Get and put areas:
    // Get area:
    streamsize 
    in_avail();

    int_type 
    snextc();

    int_type 
    sbumpc();

    int_type 
    sgetc();

    streamsize 
    sgetn(char_type* __s, streamsize __n);

    // Putback:
    int_type 
    sputbackc(char_type __c);

    int_type 
    sungetc();

    // Put area:
    int_type 
    sputc(char_type __c);

    streamsize 
    sputn(const char_type* __s, streamsize __n);

  protected:
    basic_streambuf();

  private:
    basic_streambuf(const basic_streambuf&);

  }; 
}

namespace std {
  %template(streambuf) basic_streambuf<char>;
#if defined(SWIG_WCHAR)
  %template(wstreambuf) basic_streambuf<wchar_t>;
#endif
}
