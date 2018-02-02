%include <exception.i>
%include <std_container.i>
%include <std_alloc.i>
%include <std_char_traits.i>

%fragment("<string>");

namespace std
{
  %naturalvar basic_string;
}


namespace std {

  template <class _CharT, class _Traits = char_traits<_CharT>, typename _Alloc = allocator<_CharT> > 
  class basic_string
  {
#if !defined(SWIG_STD_MODERN_STL) || defined(SWIG_STD_NOMODERN_STL)
    %ignore push_back;
    %ignore clear;
    %ignore compare;
    %ignore append;
#endif

  public:
    typedef size_t size_type;    
    typedef ptrdiff_t difference_type;
    typedef _CharT value_type;
    typedef value_type reference;
    typedef value_type const_reference;
    typedef _Alloc allocator_type;
    
    static const size_type npos;

#ifdef SWIG_EXPORT_ITERATOR_METHODS
  class iterator;
  class reverse_iterator;
  class const_iterator;
  class const_reverse_iterator;
#endif


    %traits_swigtype(_CharT);
    %fragment(SWIG_Traits_frag(_CharT));
    

    basic_string(const _CharT* __s, size_type __n);

    // Capacity:

    size_type length() const;

    size_type max_size() const;

    size_type capacity() const;

    void reserve(size_type __res_arg = 0);


    // Modifiers:

    basic_string& 
    append(const basic_string& __str);

    basic_string& 
    append(const basic_string& __str, size_type __pos, size_type __n);

    basic_string& 
    append(const _CharT* __s, size_type __n);
    
    basic_string& 
    append(size_type __n, _CharT __c);

    basic_string& 
    assign(const basic_string& __str);

    basic_string& 
    assign(const basic_string& __str, size_type __pos, size_type __n);
    
    basic_string& 
    assign(const _CharT* __s, size_type __n);

    basic_string& 
    insert(size_type __pos1, const basic_string& __str);    

    basic_string& 
    insert(size_type __pos1, const basic_string& __str,
	   size_type __pos2, size_type __n);

    basic_string& 
    insert(size_type __pos, const _CharT* __s, size_type __n);

    basic_string& 
    insert(size_type __pos, size_type __n, _CharT __c);

    basic_string& 
    erase(size_type __pos = 0, size_type __n = npos);

    basic_string& 
    replace(size_type __pos, size_type __n, const basic_string& __str);

    basic_string& 
    replace(size_type __pos1, size_type __n1, const basic_string& __str,
	    size_type __pos2, size_type __n2);

    basic_string& 
    replace(size_type __pos, size_type __n1, const _CharT* __s,
	    size_type __n2);

    basic_string& 
    replace(size_type __pos, size_type __n1, size_type __n2, _CharT __c);


    size_type 
    copy(_CharT* __s, size_type __n, size_type __pos = 0) const;    

    // String operations:
    const _CharT* c_str() const;

    size_type 
    find(const _CharT* __s, size_type __pos, size_type __n) const;
    
    size_type 
    find(const basic_string& __str, size_type __pos = 0) const;

    size_type 
    find(_CharT __c, size_type __pos = 0) const;

    size_type 
    rfind(const basic_string& __str, size_type __pos = npos) const;

    size_type 
    rfind(const _CharT* __s, size_type __pos, size_type __n) const;

    size_type 
    rfind(_CharT __c, size_type __pos = npos) const;

    size_type 
    find_first_of(const basic_string& __str, size_type __pos = 0) const;

    size_type 
    find_first_of(const _CharT* __s, size_type __pos, size_type __n) const;

    size_type 
    find_first_of(_CharT __c, size_type __pos = 0) const;

    size_type 
    find_last_of(const basic_string& __str, size_type __pos = npos) const;
    
    size_type 
    find_last_of(const _CharT* __s, size_type __pos, size_type __n) const;

    size_type 
    find_last_of(_CharT __c, size_type __pos = npos) const;
    
    size_type 
    find_first_not_of(const basic_string& __str, size_type __pos = 0) const;

    size_type 
    find_first_not_of(const _CharT* __s, size_type __pos, 
		      size_type __n) const;

    size_type 
    find_first_not_of(_CharT __c, size_type __pos = 0) const;

    size_type 
    find_last_not_of(const basic_string& __str, size_type __pos = npos) const;

    size_type 
    find_last_not_of(const _CharT* __s, size_type __pos, 
		     size_type __n) const;
    
    size_type 
    find_last_not_of(_CharT __c, size_type __pos = npos) const;

    basic_string 
    substr(size_type __pos = 0, size_type __n = npos) const;

    int 
    compare(const basic_string& __str) const;

    int 
    compare(size_type __pos, size_type __n, const basic_string& __str) const;

    int 
    compare(size_type __pos1, size_type __n1, const basic_string& __str,
	    size_type __pos2, size_type __n2) const;


    %ignore pop_back();
    %ignore front() const;
    %ignore back() const;
    %ignore basic_string(size_type n);
    %std_sequence_methods_val(basic_string);    


    %ignore pop();


#ifdef %swig_basic_string
    // Add swig/language extra methods
    %swig_basic_string(std::basic_string< _CharT, _Traits, _Alloc >);
#endif

#ifdef SWIG_EXPORT_ITERATOR_METHODS

    
    class iterator;
    class reverse_iterator;
    class const_iterator;
    class const_reverse_iterator;


    void 
    insert(iterator __p, size_type __n, _CharT __c);

    basic_string& 
    replace(iterator __i1, iterator __i2, const basic_string& __str);

    basic_string& 
    replace(iterator __i1, iterator __i2, const _CharT* __s, size_type __n);

    basic_string& 
    replace(iterator __i1, iterator __i2, size_type __n, _CharT __c);


    basic_string& 
    replace(iterator __i1, iterator __i2, const _CharT* __k1, const _CharT* __k2);

    basic_string& 
    replace(iterator __i1, iterator __i2, const_iterator __k1, const_iterator __k2);
#endif

    basic_string& operator +=(const basic_string& v);

    %newobject __add__;   
    %newobject __radd__;
    %extend {

      std::basic_string< _CharT,_Traits,_Alloc >* __add__(const basic_string& v) {
	std::basic_string< _CharT,_Traits,_Alloc >* res = new std::basic_string< _CharT,_Traits,_Alloc >(*self);
	*res += v;      
	return res;
      }
      
      std::basic_string< _CharT,_Traits,_Alloc >* __radd__(const basic_string& v) {
	std::basic_string< _CharT,_Traits,_Alloc >* res = new std::basic_string< _CharT,_Traits,_Alloc >(v);
	*res += *self;      
	return res;
      }
      
      std::basic_string< _CharT,_Traits,_Alloc > __str__() {
	return *self;
      }

      std::basic_ostream<_CharT, std::char_traits<_CharT> >&
	__rlshift__(std::basic_ostream<_CharT, std::char_traits<_CharT> >& out) {
	out << *self;
	return out;
      }
    }

  };
}


