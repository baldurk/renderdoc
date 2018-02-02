%include <std_common.i>

%{
#include <utility>
%}


namespace std {
  template <class T, class U > struct pair {      
    typedef T first_type;
    typedef U second_type;
    
    %traits_swigtype(T);
    %traits_swigtype(U);

    %fragment(SWIG_Traits_frag(std::pair< T, U >), "header",
	      fragment=SWIG_Traits_frag(T),
	      fragment=SWIG_Traits_frag(U),
	      fragment="StdPairTraits") {
      namespace swig {
	template <>  struct traits<std::pair< T, U > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::pair<" #T "," #U " >";
	  }
	};
      }
    }

#ifndef SWIG_STD_PAIR_ASVAL
    %typemap_traits_ptr(SWIG_TYPECHECK_PAIR, std::pair< T, U >);
#else
    %typemap_traits(SWIG_TYPECHECK_PAIR, std::pair< T, U >);
#endif

    pair();
    pair(T first, U second);
    pair(const pair& p);

    template <class U1, class U2> pair(const pair< U1, U2 > &p);

    T first;
    U second;

#ifdef %swig_pair_methods
    // Add swig/language extra methods
    %swig_pair_methods(std::pair< T, U >)
#endif
  };

  // ***
  // The following specializations should disappear or get
  // simplified when a 'const SWIGTYPE*&' can be defined
  // ***
  template <class T, class U > struct pair< T, U* > {
    typedef T first_type;
    typedef U* second_type;
    
    %traits_swigtype(T);
    %traits_swigtype(U);
      
    %fragment(SWIG_Traits_frag(std::pair< T, U* >), "header",
	      fragment=SWIG_Traits_frag(T),
	      fragment=SWIG_Traits_frag(U),
	      fragment="StdPairTraits") {
      namespace swig {
	template <>  struct traits<std::pair< T, U* > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::pair<" #T "," #U " * >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_PAIR, std::pair< T, U* >);

    pair();
    pair(T __a, U* __b);
    pair(const pair& __p);

    T first;
    U* second;

#ifdef %swig_pair_methods
    // Add swig/language extra methods
    %swig_pair_methods(std::pair< T, U* >)
#endif
  };

  template <class T, class U > struct pair< T*, U > {
    typedef T* first_type;
    typedef U second_type;
    
    %traits_swigtype(T);
    %traits_swigtype(U);
      
    %fragment(SWIG_Traits_frag(std::pair< T*, U >), "header",
	      fragment=SWIG_Traits_frag(T),
	      fragment=SWIG_Traits_frag(U),
	      fragment="StdPairTraits") {
      namespace swig {
	template <>  struct traits<std::pair< T*, U > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::pair<" #T " *," #U " >";
	  }
	};
      }
    }

    %typemap_traits_ptr(SWIG_TYPECHECK_PAIR, std::pair< T*, U >);

    pair();
    pair(T* __a, U __b);
    pair(const pair& __p);

    T* first;
    U second;

#ifdef %swig_pair_methods
    // Add swig/language extra methods
    %swig_pair_methods(std::pair< T*, U >)
#endif
  };

  template <class T, class U > struct pair< T*, U* > {
    typedef T* first_type;
    typedef U* second_type;

    %traits_swigtype(T);
    %traits_swigtype(U);
      
    %fragment(SWIG_Traits_frag(std::pair< T*, U* >), "header",
	      fragment=SWIG_Traits_frag(T),
	      fragment=SWIG_Traits_frag(U),
	      fragment="StdPairTraits") {
      namespace swig {
	template <>  struct traits<std::pair< T*, U* > > {
	  typedef pointer_category category;
	  static const char* type_name() {
	    return "std::pair<" #T " *," #U " * >";
	  }
	};
      }
    }

    %typemap_traits(SWIG_TYPECHECK_PAIR, std::pair< T*, U* >);

    pair();
    pair(T* __a, U* __b);
    pair(const pair& __p);

    T* first;
    U* second;
 
#ifdef %swig_pair_methods
    // Add swig/language extra methods
    %swig_pair_methods(std::pair< T*, U* >)
#endif
  };

}
