/* -------------------------------------------------------------------------
 *  Special user directives
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */

/* shadow code */
#define %shadow      %insert("shadow")
#define %pythoncode  %insert("python")
#define %pythonbegin %insert("pythonbegin")


/* ------------------------------------------------------------------------- */
/* 
Use the "nondynamic" feature to make a wrapped class behave as a "nondynamic"
one, ie, a python class that doesn't dynamically add new attributes.

For example, for the class

%pythonnondynamic A;
struct A
{
  int a;
  int b;
};

you will get:

 aa = A()
 aa.a = 1  # Ok
 aa.b = 1  # Ok
 aa.c = 3  # error

Since nondynamic is a feature, if you use it like

 %pythonnondynamic;

it will make all the wrapped classes nondynamic ones.

The implementation is based on this recipe:

   http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/252158

and works for modern (-modern) and plain python. We do not use __slots__, 
so, it works with old python versions.

*/

#define %pythonnondynamic      %feature("python:nondynamic", "1")
#define %nopythonnondynamic    %feature("python:nondynamic", "0")
#define %clearpythonnondynamic %feature("python:nondynamic", "")
#define %pythondynamic         %nopythonnondynamic


/* ------------------------------------------------------------------------- */
/*

Use %pythonmaybecall to flag a method like __add__ or __radd__.  These
don't produce an error when called, they just return NotImplemented.

These methods "may be called" if needed.

*/

#define %pythonmaybecall      %feature("python:maybecall", "1") 
#define %nopythonmaybecall    %feature("python:maybecall", "0")
#define %clearpythonmaybecall %feature("python:maybecall", "")

/* ------------------------------------------------------------------------- */
/*
  The %pythoncallback feature produce a more natural callback wrapper
  than the %callback mechanism, ie, it uses the original name for
  the callback and callable objects. 

  Just use it as

    %pythoncallback(1) foo;
    int foo(int a);

    %pythoncallback(1) A::foo;
    struct A {
     static int foo(int a);
    };

    int bar(int, int (*pf)(int));

  then, you can use it as:

   a = foo(1)
   b = bar(2, foo)

   c = A.foo(3)
   d = bar(4, A.foo)


   If you use it with a member method
   %pythoncallback(1) A::foom;
   struct A {
      int foom(int a);
   };

   then you can use it as

     r = a.foom(3)             # eval the method
     mptr = A.foom_cb_ptr      # returns the callback pointer

   where the '_cb_ptr' suffix is added for the callback pointer.

*/

#define %pythoncallback      %feature("python:callback")
#define %nopythoncallback    %feature("python:callback","0")
#define %clearpythoncallback %feature("python:callback","")

/* ------------------------------------------------------------------------- */
/*
  Support for the old %callback directive name
*/
#ifdef %callback
#undef %callback
#endif

#ifdef %nocallback
#undef %nocallback
#endif

#ifdef %clearcallback
#undef %clearcallback
#endif

#define %callback(x)     %feature("python:callback",`x`)
#define %nocallback      %nopythoncallback
#define %clearcallback   %clearpythoncallback

/* ------------------------------------------------------------------------- */
/*
  Thread support - Advance control
  
*/

#define %nothread           %feature("nothread")
#define %thread             %feature("nothread","0")
#define %clearnothread      %feature("nothread","")

#define %nothreadblock      %feature("nothreadblock")
#define %threadblock        %feature("nothreadblock","0")
#define %clearnothreadblock %feature("nothreadblock","")

#define %nothreadallow      %feature("nothreadallow")
#define %threadallow        %feature("nothreadallow","0")
#define %clearnothreadallow %feature("nothreadallow","")


/* ------------------------------------------------------------------------- */
/*
  Implicit Conversion using the C++ constructor mechanism
*/

#define %implicitconv      %feature("implicitconv") 
#define %noimplicitconv    %feature("implicitconv", "0")
#define %clearimplicitconv %feature("implicitconv", "")


/* ------------------------------------------------------------------------- */
/*
  Enable keywords paramaters
*/

#define %kwargs      %feature("kwargs") 
#define %nokwargs    %feature("kwargs", "0")
#define %clearkwargs %feature("kwargs", "")

/* ------------------------------------------------------------------------- */
/*
  Add python code to the proxy/shadow code 
  
   %pythonprepend   - Add code before the C++ function is called
   %pythonappend    - Add code after the C++ function is called
*/

#define %pythonprepend       %feature("pythonprepend") 
#define %clearpythonprepend  %feature("pythonprepend","")

#define %pythonappend         %feature("pythonappend") 
#define %clearpythonappend    %feature("pythonappend","")


/* ------------------------------------------------------------------------- */
/* 
   %extend_smart_pointer extend the smart pointer support.

   For example, if you have a smart pointer as:
	    
     template <class Type> class RCPtr {
     public:
       ...
       RCPtr(Type *p);
   	Type * operator->() const;
   	...
     };
     
   you use the %extend_smart_pointer directive as:
   
     %extend_smart_pointer(RCPtr<A>);
     %template(RCPtr_A)  RCPtr<A>;
   
   then, if you have something like:

     RCPtr<A> make_ptr();
     int foo(A *);

   you can do the following:

     a = make_ptr();
     b = foo(a);

   ie, swig will accept a RCPtr<A> object where a 'A *' is
   expected.

   Also, when using vectors
   
     %extend_smart_pointer(RCPtr<A>);
     %template(RCPtr_A) RCPtr<A>;
     %template(vector_A) std::vector<RCPtr<A> >;
   	
   you can type

     a = A();
     v = vector_A(2)
     v[0] = a

   ie, an 'A *' object is accepted, via implicit conversion, 
   where a RCPtr<A> object is expected. Additionally

     x = v[0]

   returns (and sets 'x' as) a copy of v[0], making reference
   counting possible and consistent.
*/

%define %extend_smart_pointer(Type...)
%implicitconv Type;
%apply const SWIGTYPE& SMARTPOINTER { const Type& };
%apply SWIGTYPE SMARTPOINTER { Type };
%enddef
