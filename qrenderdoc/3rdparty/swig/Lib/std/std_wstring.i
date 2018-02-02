%include <wchar.i>
%include <std/std_basic_string.i>

/* wide strings */

namespace std
{
  %std_comp_methods(basic_string<wchar_t>);
  %naturalvar wstring;
  typedef basic_string<wchar_t> wstring;
}

%template(wstring) std::basic_string<wchar_t>;

