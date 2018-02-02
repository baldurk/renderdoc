%{
#include <algorithm>
%}

//
// std::carray - is really an extension to the 'std' namespace.
// 
// A simple fix C array wrapper, more or less as presented in
//
//   "The C++ Standarf Library", by Nicolai M. Josuttis
//
// which is also derived from the example in
//
//   "The C++ Programming Language", by Bjarne Stroustup.
//

%inline %{
namespace std {    
  template <class _Type, size_t _Size>
  class carray 
  {
  public:
    typedef _Type value_type;    
    typedef size_t size_type;
    
    typedef _Type * iterator;
    typedef const _Type * const_iterator;
    
    carray() { }
    
    carray(const carray& c) {
      std::copy(c.v, c.v + size(), v);
    }
    
    template <class _Iterator>
    carray(_Iterator first, _Iterator last) {
      assign(first, last);
    }

    iterator begin() { return v; }
    iterator end() { return v + _Size; }

    const_iterator begin() const { return v; }
    const_iterator end() const { return v + _Size; }
    
    _Type& operator[](size_t i) { return v[i]; }
    const _Type& operator[](size_t i) const { return v[i]; }

    static size_t size() { return _Size; }    

    template <class _Iterator>
    void assign(_Iterator first, _Iterator last)  {
      if (std::distance(first,last) == size()) {
	std::copy(first, last, v);
      } else {
	throw std::length_error("bad range length");
      }
    }
      
  private:
    _Type v[_Size];
  };
}
%}
