%include <std/std_basic_string.i>

/* plain strings */

namespace std
{
  %std_comp_methods(basic_string<char>);
  %naturalvar string;
  typedef basic_string<char> string;
}


%template(string) std::basic_string<char>;
