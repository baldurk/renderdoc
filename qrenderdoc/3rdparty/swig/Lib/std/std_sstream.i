/* 
   For wchar support, you need to include the wchar.i file
   before this file, ie:
   
   %include <wchar.i>
   %include <std_sstream.i>

   or equivalently, just include

   %include <std_wsstream.i>
*/

%include <std_alloc.i>
%include <std_basic_string.i>
%include <std_string.i>
%include <std_ios.i>
#if defined(SWIG_WCHAR)
%include <std_wstring.i>
#endif
%include <std_streambuf.i>
%include <std_iostream.i>

%{
#include <sstream>
%}


namespace std
{
  template<typename _CharT, typename _Traits = char_traits<_CharT>,
	   typename _Alloc = allocator<_CharT> >
    class basic_stringbuf : public basic_streambuf<_CharT, _Traits>
    {
    public:
      // Types:
      typedef _CharT 					char_type;
      typedef _Traits 					traits_type;
// 251. basic_stringbuf missing allocator_type
      typedef _Alloc				       	allocator_type;
      typedef typename traits_type::int_type 		int_type;
      typedef typename traits_type::pos_type 		pos_type;
      typedef typename traits_type::off_type 		off_type;

    public:
      // Constructors:
      explicit
      basic_stringbuf(ios_base::openmode __mode = ios_base::in | ios_base::out);

      explicit
      basic_stringbuf(const basic_string<_CharT, _Traits, _Alloc>& __str,
		      ios_base::openmode __mode = ios_base::in | ios_base::out);

      // Get and set:
      basic_string<_CharT, _Traits, _Alloc>
      str() const;

      void
      str(const basic_string<_CharT, _Traits, _Alloc>& __s);

    };


  // 27.7.2  Template class basic_istringstream
  template<typename _CharT, typename _Traits = char_traits<_CharT>,
	   typename _Alloc = allocator<_CharT> >
  class basic_istringstream : public basic_istream<_CharT, _Traits>
  {
    public:
      // Types:
      typedef _CharT 					char_type;
      typedef _Traits 					traits_type;
// 251. basic_stringbuf missing allocator_type
      typedef _Alloc				       	allocator_type;
      typedef typename traits_type::int_type 		int_type;
      typedef typename traits_type::pos_type 		pos_type;
      typedef typename traits_type::off_type 		off_type;


    public:
      // Constructors:
      explicit
      basic_istringstream(ios_base::openmode __mode = ios_base::in);

      explicit
      basic_istringstream(const basic_string<_CharT, _Traits, _Alloc>& __str,
			  ios_base::openmode __mode = ios_base::in);

      ~basic_istringstream();

      // Members:
      basic_stringbuf<_CharT, _Traits, _Alloc>*
      rdbuf() const;

      basic_string<_CharT, _Traits, _Alloc>
      str() const;

      void
      str(const basic_string<_CharT, _Traits, _Alloc>& __s);
    };


  // 27.7.3  Template class basic_ostringstream
  template<typename _CharT, typename _Traits = char_traits<_CharT>,
	   typename _Alloc = allocator<_CharT> >
  class basic_ostringstream : public basic_ostream<_CharT, _Traits>
  {
    public:
      // Types:
      typedef _CharT 					char_type;
      typedef _Traits 					traits_type;
// 251. basic_stringbuf missing allocator_type
      typedef _Alloc				       	allocator_type;
      typedef typename traits_type::int_type 		int_type;
      typedef typename traits_type::pos_type 		pos_type;
      typedef typename traits_type::off_type 		off_type;


    public:
     // Constructors/destructor:
      explicit
      basic_ostringstream(ios_base::openmode __mode = ios_base::out);

      explicit
      basic_ostringstream(const basic_string<_CharT, _Traits, _Alloc>& __str,
			  ios_base::openmode __mode = ios_base::out);

      ~basic_ostringstream();

      // Members:
      basic_stringbuf<_CharT, _Traits, _Alloc>*
      rdbuf() const;

      basic_string<_CharT, _Traits, _Alloc>
      str() const;

#if 0
      void
      str(const basic_string<_CharT, _Traits, _Alloc>& __s);
#endif
    };


  // 27.7.4  Template class basic_stringstream
  template<typename _CharT, typename _Traits = char_traits<_CharT>,
	   typename _Alloc = allocator<_CharT> >
  class basic_stringstream : public basic_iostream<_CharT, _Traits>
  {
    public:
      // Types:
      typedef _CharT 					char_type;
      typedef _Traits 					traits_type;
// 251. basic_stringbuf missing allocator_type
      typedef _Alloc				       	allocator_type;
      typedef typename traits_type::int_type 		int_type;
      typedef typename traits_type::pos_type 		pos_type;
      typedef typename traits_type::off_type 		off_type;

    public:
      // Constructors/destructors
      explicit
      basic_stringstream(ios_base::openmode __m = ios_base::out | ios_base::in);

      explicit
      basic_stringstream(const basic_string<_CharT, _Traits, _Alloc>& __str,
			 ios_base::openmode __m = ios_base::out | ios_base::in);

      ~basic_stringstream();

      // Members:
      basic_stringbuf<_CharT, _Traits, _Alloc>*
      rdbuf() const;

      basic_string<_CharT, _Traits, _Alloc>
      str() const;

      void
      str(const basic_string<_CharT, _Traits, _Alloc>& __s);
    };


} // namespace std


namespace std {
  %template(istringstream) basic_istringstream<char>;
  %template(ostringstream) basic_ostringstream<char>;
  %template(stringstream)  basic_stringstream<char>;


#if defined(SWIG_WCHAR)
  %template(wistringstream) basic_istringstream<wchar_t>;
  %template(wostringstream) basic_ostringstream<wchar_t>;
  %template(wstringstream)  basic_stringstream<wchar_t>;
#endif
}
