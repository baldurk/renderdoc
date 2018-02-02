%include <std_char_traits.i>
%include <std_basic_string.i>
%include <std_except.i>
%{
#ifndef SWIG_STD_NOMODERN_STL
# include <ios>
#else
# include <streambuf.h>
#endif
%}

namespace std {

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_streambuf;

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_istream;

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_ostream;

  // 27.4.2  Class ios_base
  typedef size_t streamsize;

  class locale;
  
  
  class ios_base
  {
  public:
    
#ifdef SWIG_NESTED_CLASSES
    // 27.4.2.1.1  Class ios_base::failure
    class failure : public exception
    {
    public:
      explicit failure(const string& __str) throw();
    };
#endif

    // 27.4.2.1.2  Type ios_base::fmtflags
    typedef int fmtflags;
    // 27.4.2.1.2  Type fmtflags
    static const fmtflags boolalpha ;
    static const fmtflags dec ;
    static const fmtflags fixed ;
    static const fmtflags hex ;
    static const fmtflags internal ;
    static const fmtflags left ;
    static const fmtflags oct ;
    static const fmtflags right ;
    static const fmtflags scientific ;
    static const fmtflags showbase ;
    static const fmtflags showpoint ;
    static const fmtflags showpos ;
    static const fmtflags skipws ;
    static const fmtflags unitbuf ;
    static const fmtflags uppercase ;
    static const fmtflags adjustfield ;
    static const fmtflags basefield ;
    static const fmtflags floatfield ;

    // 27.4.2.1.3  Type ios_base::iostate
    typedef int iostate;
    static const iostate badbit ;
    static const iostate eofbit ;
    static const iostate failbit ;
    static const iostate goodbit ;

    // 27.4.2.1.4  Type openmode
    typedef int openmode;
    static const openmode app ;
    static const openmode ate ;
    static const openmode binary ;
    static const openmode in ;
    static const openmode out ;
    static const openmode trunc ;

    // 27.4.2.1.5  Type seekdir
    typedef int seekdir;
    static const seekdir beg ;
    static const seekdir cur ;
    static const seekdir end ;


    // Callbacks;
    enum event
      {
	erase_event,
	imbue_event,
	copyfmt_event
      };

    typedef void (*event_callback) (event, ios_base&, int);

    void 
    register_callback(event_callback __fn, int __index);

    // Fmtflags state:
    inline fmtflags 
    flags() const ;

    inline fmtflags 
    flags(fmtflags __fmtfl);

    inline fmtflags 
    setf(fmtflags __fmtfl);

    inline fmtflags 
    setf(fmtflags __fmtfl, fmtflags __mask);

    inline void 
    unsetf(fmtflags __mask) ;

    inline streamsize 
    precision() const ;

    inline streamsize 
    precision(streamsize __prec);

    inline streamsize 
    width() const ;

    inline streamsize 
    width(streamsize __wide);

    static bool 
    sync_with_stdio(bool __sync = true);

    // Locales:
    locale 
    imbue(const locale& __loc);

    inline locale 
    getloc() const { return _M_ios_locale; }

    // Storage:
    static int 
    xalloc() throw();

    inline long& 
    iword(int __ix);

    inline void*& 
    pword(int __ix);

    // Destructor
    ~ios_base();

  protected:
    ios_base();

  //50.  Copy constructor and assignment operator of ios_base
  private:
    ios_base(const ios_base&);

    ios_base& 
    operator=(const ios_base&);
  };

  template<typename _CharT, typename _Traits = char_traits<_CharT> >
  class basic_ios : public ios_base
  {
  public:
    // Types:
    typedef _CharT 				char_type;
    typedef typename _Traits::int_type 	int_type;
    typedef typename _Traits::pos_type 	pos_type;
    typedef typename _Traits::off_type 	off_type;
    typedef _Traits 				traits_type;
      
  public:

    iostate 
    rdstate() const;

    void 
    clear(iostate __state = goodbit);

    void 
    setstate(iostate __state);

    bool 
    good() const;

    bool 
    eof() const;

    bool 
    fail() const;

    bool 
    bad() const;

    iostate 
    exceptions() const;

    void 
    exceptions(iostate __except);

    // Constructor/destructor:
    explicit 
    basic_ios(basic_streambuf<_CharT, _Traits>* __sb) : ios_base();

    virtual 
    ~basic_ios() ;
      
    // Members:
    basic_ostream<_CharT, _Traits>*
    tie() const;

    basic_ostream<_CharT, _Traits>*
    tie(basic_ostream<_CharT, _Traits>* __tiestr);

    basic_streambuf<_CharT, _Traits>*
    rdbuf() const;

    basic_streambuf<_CharT, _Traits>* 
    rdbuf(basic_streambuf<_CharT, _Traits>* __sb);

    basic_ios&
    copyfmt(const basic_ios& __rhs);

    char_type 
    fill() const;

    char_type 
    fill(char_type __ch);

    // Locales:
    locale 
    imbue(const locale& __loc);

    char 
    narrow(char_type __c, char __dfault) const;

    char_type 
    widen(char __c) const;
     
  protected:
    // 27.4.5.1  basic_ios constructors
    basic_ios();
  private:
    basic_ios(const basic_ios&);

    basic_ios&
    operator=(const basic_ios&);
  };
  
}

namespace std {
  typedef basic_ios<char> ios;
  %template(ios) basic_ios<char>;
#if defined(SWIG_WCHAR)
 typedef basic_ios<wchar_t> wios;
  %template(wios) basic_ios<wchar_t>;
#endif
}

  
