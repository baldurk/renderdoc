/* -----------------------------------------------------------------------------
 * swig.swg
 *
 * Common macro definitions for various SWIG directives.  This file is always 
 * included at the top of each input file.
 * ----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * User Directives 
 * ----------------------------------------------------------------------------- */

/* Deprecated SWIG-1.1 directives */

#define %disabledoc     %warn "104:%disabledoc is deprecated"
#define %enabledoc      %warn "105:%enabledoc is deprecated"
#define %doconly        %warn "106:%doconly is deprecated"
#define %style          %warn "107:%style is deprecated" /##/
#define %localstyle     %warn "108:%localstyle is deprecated" /##/
#define %title          %warn "109:%title is deprecated" /##/
#define %section        %warn "110:%section is deprecated" /##/
#define %subsection     %warn "111:%subsection is deprecated" /##/
#define %subsubsection  %warn "112:%subsubsection is deprecated" /##/
#define %new            %warn "117:%new is deprecated. Use %newobject"
#define %text           %insert("null")

/* Code insertion directives such as %wrapper %{ ... %} */

#define %begin       %insert("begin")
#define %runtime     %insert("runtime")
#define %header      %insert("header")
#define %wrapper     %insert("wrapper")
#define %init        %insert("init")

/* Class extension */

#define %addmethods  %warn "113:%addmethods is now %extend" %extend

/* %ignore directive */

#define %ignore         %rename($ignore)
#define %ignorewarn(x)  %rename("$ignore:" x)

/* Access control directives */

#define %readonly    %warn "114:%readonly is deprecated. Use %immutable; " %feature("immutable");
#define %readwrite   %warn "115:%readwrite is deprecated. Use %mutable; " %feature("immutable","");

#define %immutable       %feature("immutable")
#define %noimmutable     %feature("immutable","0")
#define %clearimmutable  %feature("immutable","")
#define %mutable         %clearimmutable

/* Generation of default constructors/destructors (old form, don't use) */
#define %nodefault       %feature("nodefault","1")
#define %default         %feature("nodefault","0")
#define %clearnodefault  %feature("nodefault","")
#define %makedefault     %clearnodefault

/* Disable the generation of implicit default constructor */
#define %nodefaultctor       %feature("nodefaultctor","1")
#define %defaultctor         %feature("nodefaultctor","0")
#define %clearnodefaultctor  %feature("nodefaultctor","")

/* Disable the generation of implicit default destructor (dangerous) */
#define %nodefaultdtor       %feature("nodefaultdtor","1")
#define %defaultdtor         %feature("nodefaultdtor","0")
#define %clearnodefaultdtor  %feature("nodefaultdtor","")

/* Enable the generation of copy constructor */
#define %copyctor       %feature("copyctor","1")
#define %nocopyctor     %feature("copyctor","0")
#define %clearcopyctor  %feature("copyctor","")

/* Force the old nodefault behavior, ie disable both constructor and destructor */
#define %oldnodefault       %feature("oldnodefault","1")
#define %nooldnodefault     %feature("oldnodefault","0")
#define %clearoldnodefault  %feature("oldnodefault","")

/* the %exception directive */
#if defined(SWIGCSHARP) || defined(SWIGD)
#define %exception      %feature("except", canthrow=1)
#else
#define %exception      %feature("except")
#endif
#define %noexception    %feature("except","0")
#define %clearexception %feature("except","")

/* the %allowexception directive allows the %exception feature to
   be applied to set/get variable methods */
#define %allowexception      %feature("allowexcept")
#define %noallowexception    %feature("allowexcept","0")
#define %clearallowexception %feature("allowexcept","")

/* the %exceptionvar directive, as %exception but it is only applied
   to set/get variable methods. You don't need to use the
   %allowexception directive when using %exceptionvar.
*/
#if defined(SWIGCSHARP) || defined(SWIGD)
#define %exceptionvar      %feature("exceptvar", canthrow=1)
#else
#define %exceptionvar      %feature("exceptvar")
#endif
#define %noexceptionvar    %feature("exceptvar","0")
#define %clearexceptionvar %feature("exceptvar","")

/* the %catches directive */
#define %catches(tlist...)    %feature("catches","("`tlist`")")
#define %clearcatches         %feature("catches","")

/* the %exceptionclass directive */
#define %exceptionclass      %feature("exceptionclass")
#define %noexceptionclass    %feature("exceptionclass","0")
#define %clearexceptionclass %feature("exceptionclass","")

/* the %newobject directive */
#define %newobject        %feature("new")
#define %nonewobject      %feature("new","0")
#define %clearnewobject   %feature("new","")

/* the %delobject directive */
#define %delobject        %feature("del")
#define %nodelobject      %feature("del","0")
#define %cleardelobject   %feature("del","")

/* the %refobject/%unrefobject directives */
#define %refobject         %feature("ref")
#define %norefobject       %feature("ref","0")
#define %clearrefobject    %feature("ref","")

#define %unrefobject       %feature("unref")
#define %nounrefobject     %feature("unref","0")
#define %clearunrefobject  %feature("unref","")

/* Directives for callback functions (experimental) */
#define %callback(x)    %feature("callback",`x`)
#define %nocallback     %feature("callback","0")
#define %clearcallback  %feature("callback","")

/* the %nestedworkaround directive (deprecated) */
#define %nestedworkaround       %feature("nestedworkaround")
#define %nonestedworkaround     %feature("nestedworkaround","0")
#define %clearnestedworkaround  %feature("nestedworkaround","")

/* the %flatnested directive */
#define %flatnested       %feature("flatnested")
#define %noflatnested     %feature("flatnested","0")
#define %clearflatnested  %feature("flatnested","")

/* the %fastdispatch directive */
#define %fastdispatch        %feature("fastdispatch")
#define %nofastdispatch      %feature("fastdispatch","0")
#define %clearfastdispatch   %feature("fastdispatch","")

/* directors directives */
#define %director      %feature("director")
#define %nodirector    %feature("director","0")
#define %cleardirector %feature("director","")

/* naturalvar directives */
#define %naturalvar      %feature("naturalvar")
#define %nonaturalvar    %feature("naturalvar","0")
#define %clearnaturalvar %feature("naturalvar","")

/* nspace directives */
#define %nspace      %feature("nspace")
#define %nonspace    %feature("nspace","0")
#define %clearnspace %feature("nspace","")

/* valuewrapper directives */
#define %valuewrapper        %feature("valuewrapper")
#define %clearvaluewrapper   %feature("valuewrapper","")
#define %novaluewrapper      %feature("novaluewrapper")
#define %clearnovaluewrapper %feature("novaluewrapper","")

/* Contract support - Experimental and undocumented */
#define %contract      %feature("contract")
#define %nocontract    %feature("contract","0")
#define %clearcontract %feature("contract","")

/* Macro for setting a dynamic cast function */
%define DYNAMIC_CAST(mangle,func)
%init %{
   mangle->dcast = (swig_dycast_func) func;
%}
%enddef

/* aggregation support */
/*
  This macro performs constant aggregation.  Basically the idea of
  constant aggregation is that you can group a collection of constants
  together.  For example, suppose you have some code like this:

       #define UP  1
       #define DOWN 2
       #define LEFT 3
       #define RIGHT 4

  Now, suppose you had a function like this:

       int move(int direction)

  In this case, you might want to restrict the direction argument to
  one of the supplied constant names. To do this, you could write some
  typemap code by hand.  Alternatively, you can use the
  %aggregate_check macro defined here to create a simple check
  function for you.  Here is an example:

    %aggregate_check(int, check_direction, UP, DOWN, LEFT, RIGHT);

  Now, using a typemap

    %typemap(check) int direction {
      if (!check_direction($1)) SWIG_exception(SWIG_ValueError,"Bad direction.");
    }

  or a contract (better)

    %contract move(int x) {
    require:
        check_direction(x);
    }

*/
   
%define %aggregate_check(TYPE, NAME, FIRST, ...)
%wrapper %{
static int NAME(TYPE x) {
    static  TYPE values[] = { FIRST, ##__VA_ARGS__ };
    static  int size = sizeof(values);
    int     i,j;
    for (i = 0, j = 0; i < size; i+=sizeof(TYPE),j++) {
        if (x == values[j]) return 1; 
    }
    return 0;
}
%}
%enddef


/* -----------------------------------------------------------------------------
 * %rename predicates
 * ----------------------------------------------------------------------------- */
/* 
   Predicates to be used with %rename, for example:

   - to rename all the functions:

     %rename("%(utitle)s", %$isfunction) "";

   - to rename only the member methods:

     %rename("m_%(utitle)s", %$isfunction, %$ismember) "";

   - to rename only the global functions:

      %rename("m_%(utitle)s", %$isfunction, %$not %$ismember) "";

     or

      %rename("g_%(utitle)s", %$isfunction, %$isglobal) "";

   - to ignore the enumitems in a given class:

     %rename("$ignore", %$isenumitem, %$classname="MyClass") "";

   we use the prefix '%$' to avoid clashes with other swig
   macros/directives.

*/

%define %$not            "not" %enddef 
%define %$isenum         "match"="enum"  %enddef
%define %$isenumitem     "match"="enumitem"  %enddef
%define %$isaccess       "match"="access"   %enddef
%define %$isclass        "match"="class","notmatch$template$templatetype"="class"   %enddef
%define %$isextend       "match"="extend"  %enddef
%define %$isconstructor  "match"="constructor"  %enddef
%define %$isdestructor   "match"="destructor"  %enddef
%define %$isnamespace    "match"="namespace"  %enddef
%define %$istemplate     "match"="template"  %enddef
%define %$isconstant     "match"="constant"  %enddef  /* %constant definition */

%define %$isunion        "match$kind"="union"  %enddef
%define %$isfunction     "match$kind"="function"  %enddef
%define %$isvariable     "match$kind"="variable"  %enddef
%define %$isimmutable    "match$feature:immutable"="1"  %enddef
%define %$hasconsttype   "match$hasconsttype"="1"  %enddef
%define %$hasvalue       "match$hasvalue"="1"  %enddef
%define %$isextension    "match$isextension"="1"  %enddef

%define %$isstatic       "match$storage"="static"  %enddef
%define %$isfriend       "match$storage"="friend"  %enddef
%define %$istypedef      "match$storage"="typedef"  %enddef
%define %$isvirtual      "match$storage"="virtual"  %enddef
%define %$isexplicit     "match$storage"="explicit"  %enddef
%define %$isextern       "match$storage"="extern"  %enddef

%define %$ismember       "match$ismember"="1"  %enddef
%define %$isglobal       %$not %$ismember  %enddef
%define %$isextendmember "match$isextendmember"="1"  %enddef
%define %$innamespace    "match$parentNode$nodeType"="namespace"  %enddef

%define %$ispublic       "match$access"="public"  %enddef
%define %$isprotected    "match$access"="protected"  %enddef
%define %$isprivate      "match$access"="private"  %enddef

%define %$ismemberget    "match$memberget"="1"  %enddef
%define %$ismemberset    "match$memberset"="1"  %enddef

%define %$classname      %$ismember,"match$parentNode$name"  %enddef
%define %$isnested       "match$nested"="1"  %enddef
/* -----------------------------------------------------------------------------
 * Include all the warnings labels and macros 
 * ----------------------------------------------------------------------------- */

%include <swigwarnings.swg>

/* -----------------------------------------------------------------------------
 * Overloading support
 * ----------------------------------------------------------------------------- */

/*
 * Function/method overloading support.   This is done through typemaps,
 * but also involves a precedence level.
 */

/* Macro for overload resolution */

%define %typecheck(_x...) %typemap(typecheck, precedence=_x) %enddef

/* Macros for precedence levels */

%define SWIG_TYPECHECK_POINTER       0     %enddef
%define SWIG_TYPECHECK_ITERATOR      5     %enddef
%define SWIG_TYPECHECK_VOIDPTR       10    %enddef
%define SWIG_TYPECHECK_BOOL          15    %enddef
%define SWIG_TYPECHECK_UINT8         20    %enddef
%define SWIG_TYPECHECK_INT8          25    %enddef
%define SWIG_TYPECHECK_UINT16        30    %enddef
%define SWIG_TYPECHECK_INT16         35    %enddef
%define SWIG_TYPECHECK_UINT32        40    %enddef
%define SWIG_TYPECHECK_INT32         45    %enddef
%define SWIG_TYPECHECK_SIZE          47    %enddef
%define SWIG_TYPECHECK_PTRDIFF       48    %enddef
%define SWIG_TYPECHECK_UINT64        50    %enddef
%define SWIG_TYPECHECK_INT64         55    %enddef
%define SWIG_TYPECHECK_UINT128       60    %enddef
%define SWIG_TYPECHECK_INT128        65    %enddef
%define SWIG_TYPECHECK_INTEGER       70    %enddef
%define SWIG_TYPECHECK_FLOAT         80    %enddef
%define SWIG_TYPECHECK_DOUBLE        90    %enddef
%define SWIG_TYPECHECK_CPLXFLT       95    %enddef
%define SWIG_TYPECHECK_CPLXDBL      100    %enddef
%define SWIG_TYPECHECK_COMPLEX      105    %enddef
%define SWIG_TYPECHECK_UNICHAR      110    %enddef
%define SWIG_TYPECHECK_STDUNISTRING 115    %enddef
%define SWIG_TYPECHECK_UNISTRING    120    %enddef
%define SWIG_TYPECHECK_CHAR         130    %enddef
%define SWIG_TYPECHECK_STDSTRING    135    %enddef
%define SWIG_TYPECHECK_STRING       140    %enddef
%define SWIG_TYPECHECK_PAIR         150    %enddef
%define SWIG_TYPECHECK_STDARRAY     155    %enddef
%define SWIG_TYPECHECK_VECTOR       160    %enddef
%define SWIG_TYPECHECK_DEQUE        170    %enddef
%define SWIG_TYPECHECK_LIST         180    %enddef
%define SWIG_TYPECHECK_SET          190    %enddef
%define SWIG_TYPECHECK_MULTISET     200    %enddef
%define SWIG_TYPECHECK_MAP          210    %enddef
%define SWIG_TYPECHECK_MULTIMAP     220    %enddef
%define SWIG_TYPECHECK_STACK        230    %enddef
%define SWIG_TYPECHECK_QUEUE        240    %enddef

%define SWIG_TYPECHECK_BOOL_ARRAY        1015    %enddef
%define SWIG_TYPECHECK_INT8_ARRAY        1025    %enddef
%define SWIG_TYPECHECK_INT16_ARRAY       1035    %enddef
%define SWIG_TYPECHECK_INT32_ARRAY       1045    %enddef
%define SWIG_TYPECHECK_INT64_ARRAY       1055    %enddef
%define SWIG_TYPECHECK_INT128_ARRAY      1065    %enddef
%define SWIG_TYPECHECK_FLOAT_ARRAY       1080    %enddef
%define SWIG_TYPECHECK_DOUBLE_ARRAY      1090    %enddef
%define SWIG_TYPECHECK_CHAR_ARRAY        1130    %enddef
%define SWIG_TYPECHECK_STRING_ARRAY      1140    %enddef
%define SWIG_TYPECHECK_OBJECT_ARRAY      1150    %enddef

%define SWIG_TYPECHECK_BOOL_PTR          2015    %enddef
%define SWIG_TYPECHECK_UINT8_PTR         2020    %enddef
%define SWIG_TYPECHECK_INT8_PTR          2025    %enddef
%define SWIG_TYPECHECK_UINT16_PTR        2030    %enddef
%define SWIG_TYPECHECK_INT16_PTR         2035    %enddef
%define SWIG_TYPECHECK_UINT32_PTR        2040    %enddef
%define SWIG_TYPECHECK_INT32_PTR         2045    %enddef
%define SWIG_TYPECHECK_UINT64_PTR        2050    %enddef
%define SWIG_TYPECHECK_INT64_PTR         2055    %enddef
%define SWIG_TYPECHECK_FLOAT_PTR         2080    %enddef
%define SWIG_TYPECHECK_DOUBLE_PTR        2090    %enddef
%define SWIG_TYPECHECK_CHAR_PTR          2130    %enddef

%define SWIG_TYPECHECK_SWIGOBJECT        5000    %enddef


/* -----------------------------------------------------------------------------
 * Default handling of certain overloaded operators 
 * ----------------------------------------------------------------------------- */

#ifdef __cplusplus
%ignoreoperator(NEW)     operator new;
%ignoreoperator(DELETE)  operator delete;
%ignoreoperator(NEWARR)  operator new[];
%ignoreoperator(DELARR)  operator delete[];

/* add C++ operator aliases */
%rename("operator &&") operator and;    // `and'    `&&'
%rename("operator ||") operator or;     // `or'     `||'
%rename("operator !")  operator not;    // `not'     `!'
%rename("operator &=") operator and_eq; // `and_eq'  `&='
%rename("operator &")  operator bitand; // `bitand'  `&'
%rename("operator |")  operator bitor;  // `bitor'   `|'
%rename("operator ~")  operator compl;  // `compl'   `~'
%rename("operator !=") operator not_eq; // `not_eq'  `!='
%rename("operator |=") operator or_eq;  // `or_eq'   `|='
%rename("operator ^")  operator xor;    // `xor'     `^'
%rename("operator ^=") operator xor_eq; // `xor_eq'  `^='

/* Smart pointer handling */

%rename(__deref__) *::operator->;
%rename(__ref__)   *::operator*();
%rename(__ref__)   *::operator*() const;

/* Define std namespace */
namespace std {
  /* Warn about std::initializer_list usage. The constructor/method where used should probably be ignored. See docs. */
  template<typename T> class initializer_list {};
  %typemap(in, warning=SWIGWARN_TYPEMAP_INITIALIZER_LIST_MSG) initializer_list<T> ""
  %typemap(typecheck, precedence=SWIG_TYPECHECK_POINTER) initializer_list<T> ""
}
#endif

/* -----------------------------------------------------------------------------
 * Default char * and C array typemaps
 * ----------------------------------------------------------------------------- */

/* Set up the typemap for handling new return strings */

#ifdef __cplusplus
%typemap(newfree) char * "delete [] $1;";
#else
%typemap(newfree) char * "free($1);";
#endif

/* Default typemap for handling char * members */

#ifdef __cplusplus
%typemap(memberin) char * {
  delete [] $1;
  if ($input) {
     $1 = ($1_type) (new char[strlen((const char *)$input)+1]);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
%typemap(memberin,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const char * {
  if ($input) {
     $1 = ($1_type) (new char[strlen((const char *)$input)+1]);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin) char * {
  delete [] $1;
  if ($input) {
     $1 = ($1_type) (new char[strlen((const char *)$input)+1]);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const char * {
  if ($input) {
     $1 = ($1_type) (new char[strlen((const char *)$input)+1]);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
#else
%typemap(memberin) char * {
  free($1);
  if ($input) {
     $1 = ($1_type) malloc(strlen((const char *)$input)+1);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
%typemap(memberin,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const char * {
  if ($input) {
     $1 = ($1_type) malloc(strlen((const char *)$input)+1);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin) char * {
  free($1);
  if ($input) {
     $1 = ($1_type) malloc(strlen((const char *)$input)+1);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}
%typemap(globalin,warning=SWIGWARN_TYPEMAP_CHARLEAK_MSG) const char * {
  if ($input) {
     $1 = ($1_type) malloc(strlen((const char *)$input)+1);
     strcpy((char *)$1, (const char *)$input);
  } else {
     $1 = 0;
  }
}

#endif

/* Character array handling */

%typemap(memberin) char [ANY] {
  if($input) {
    strncpy((char*)$1, (const char *)$input, $1_dim0-1);
    $1[$1_dim0-1] = 0;
  } else {
    $1[0] = 0;
  }
}

%typemap(globalin) char [ANY] {
  if($input) {
    strncpy((char*)$1, (const char *)$input, $1_dim0-1);
    $1[$1_dim0-1] = 0;
  } else {
    $1[0] = 0;
  }
}

%typemap(memberin) char [] {
  if ($input) strcpy((char *)$1, (const char *)$input);
  else $1[0] = 0;
}

%typemap(globalin) char [] {
  if ($input) strcpy((char *)$1, (const char *)$input);
  else $1[0] = 0;
}

/* memberin/globalin typemap for arrays. */

%typemap(memberin) SWIGTYPE [ANY] {
  size_t ii;
  $1_basetype *b = ($1_basetype *) $1;
  for (ii = 0; ii < (size_t)$1_size; ii++) b[ii] = *(($1_basetype *) $input + ii);
}

%typemap(globalin) SWIGTYPE [ANY] {
  size_t ii;
  $1_basetype *b = ($1_basetype *) $1;
  for (ii = 0; ii < (size_t)$1_size; ii++) b[ii] = *(($1_basetype *) $input + ii);
}

/* memberin/globalin typemap for double arrays. */

%typemap(memberin) SWIGTYPE [ANY][ANY] {
  $basetype (*inp)[$1_dim1] = ($basetype (*)[$1_dim1])($input);
  $basetype (*dest)[$1_dim1] = ($basetype (*)[$1_dim1])($1);
  size_t ii = 0;
  for (; ii < $1_dim0; ++ii) {
    $basetype *ip = inp[ii];
    $basetype *dp = dest[ii];
    size_t jj = 0;
    for (; jj < $1_dim1; ++jj) dp[jj] = ip[jj];
  }
}

%typemap(globalin) SWIGTYPE [ANY][ANY] {
  $basetype (*inp)[$1_dim1] = ($basetype (*)[$1_dim1])($input);
  $basetype (*dest)[$1_dim1] = ($basetype (*)[$1_dim1])($1);
  size_t ii = 0;
  for (; ii < $1_dim0; ++ii) {
    $basetype *ip = inp[ii];
    $basetype *dp = dest[ii];
    size_t jj = 0;
    for (; jj < $1_dim1; ++jj) dp[jj] = ip[jj];
  }
}

/* -----------------------------------------------------------------------------
 *  Runtime code
 * ----------------------------------------------------------------------------- */

/*  The SwigValueWrapper class  */

/*  
 * This template wrapper is used to handle C++ objects that are passed or 
 * returned by value.   This is necessary to handle objects that define
 * no default-constructor (making it difficult for SWIG to properly declare
 * local variables).
 *
 * The wrapper is used as follows.  First consider a function like this:
 *
 *      Vector cross_product(Vector a, Vector b)
 *
 * Now, if Vector is defined as a C++ class with no default constructor, 
 * code is generated as follows:
 *
 *     Vector *wrap_cross_product(Vector *inarg1, Vector *inarg2) {
 *          SwigValueWrapper<Vector>  arg1;
 *          SwigValueWrapper<Vector>  arg2;
 *          SwigValueWrapper<Vector> result;
 *
 *          arg1 = *inarg1;
 *          arg2 = *inarg2;
 *          ...            
 *          result = cross_product(arg1,arg2);
 *          ...
 *          return new Vector(result);
 *    }
 *         
 * In the wrappers, the template SwigValueWrapper simply provides a thin
 * layer around a Vector *.  However, it does this in a way that allows
 * the object to be bound after the variable declaration (which is not possible
 * with the bare object when it lacks a default constructor).  
 *
 * An observant reader will notice that the code after the variable declarations
 * is *identical* to the code used for classes that do define default constructors.
 * Thus, this neat trick allows us to fix this special case without having to
 * make massive changes to typemaps and other parts of the SWIG code generator.
 *
 * Note: this code is not included when SWIG runs in C-mode, when classes
 * define default constructors, or when pointers and references are used.
 * SWIG tries to avoid doing this except in very special circumstances.
 *
 * Note: This solution suffers from making a large number of copies
 * of the underlying object.  However, this is needed in the interest of
 * safety and in order to cover all of the possible ways in which a value
 * might be assigned.  For example:
 *
 *       arg1 = *inarg1;       // Assignment from a pointer
 *       arg1 = Vector(1,2,3); // Assignment from a value  
 *
 * The class offers a strong guarantee of exception safety.
 * With regards to the implementation, the private SwigMovePointer nested class is 
 * a simple smart pointer with move semantics, much like std::auto_ptr.
 *
 * This wrapping technique was suggested by William Fulton and is henceforth
 * known as the "Fulton Transform" :-).
 */

#ifdef __cplusplus
%insert("runtime") %{
#ifdef __cplusplus
/* SwigValueWrapper is described in swig.swg */
template<typename T> class SwigValueWrapper {
  struct SwigMovePointer {
    T *ptr;
    SwigMovePointer(T *p) : ptr(p) { }
    ~SwigMovePointer() { delete ptr; }
    SwigMovePointer& operator=(SwigMovePointer& rhs) { T* oldptr = ptr; ptr = 0; delete oldptr; ptr = rhs.ptr; rhs.ptr = 0; return *this; }
  } pointer;
  SwigValueWrapper& operator=(const SwigValueWrapper<T>& rhs);
  SwigValueWrapper(const SwigValueWrapper<T>& rhs);
public:
  SwigValueWrapper() : pointer(0) { }
  SwigValueWrapper& operator=(const T& t) { SwigMovePointer tmp(new T(t)); pointer = tmp; return *this; }
  operator T&() const { return *pointer.ptr; }
  T *operator&() { return pointer.ptr; }
};%}

/*
 * SwigValueInit() is a generic initialisation solution as the following approach:
 * 
 *       T c_result = T();
 * 
 * doesn't compile for all types for example:
 * 
 *       unsigned int c_result = unsigned int();
 */
%insert("runtime") %{
template <typename T> T SwigValueInit() {
  return T();
}
#endif
%}
#endif

/*  The swiglabels  */

%insert("runtime") "swiglabels.swg"


