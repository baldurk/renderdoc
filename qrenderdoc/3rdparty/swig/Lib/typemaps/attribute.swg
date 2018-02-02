/* -----------------------------------------------------------------------------
 * attribute.swg
 *
 * Attribute implementation
 * ----------------------------------------------------------------------------- */

/*
  The following macros convert a pair of set/get methods
  into a "native" attribute.

  Use %attribute when you have a pair of get/set methods to a primitive type
  like in:

      %attribute(A, int, a, get_a, set_a);

      struct A
      {
        int get_a() const;
        void set_a(int aa);
      };

  If you don't provide a 'set' method, a 'read-only' attribute
  is generated, ie, like in:

      %attribute(A, int, c, get_c);

  Use %attributeref when you have const/non-const reference access methods
  for primitive types or class/structs, like in:

      %attributeref(A, int, b);

      struct A
      {
        const int& b() const;
        int& b();
      };

      %attributeref(B, int, c);

      struct B
      {
        int& c();
      };

  You can also use

      %attributeref(Class, AttributeType, AttributeName, AccessorMethod)

  if the internal C++ reference methods have a different name from the
  attribute you want, so

      %attributeref(B, int, d, c);

  is the same as the last example, but instead of the attribute 'c' being
  called 'c', it is called 'd'.

  Now you can use the attributes like so:

      x = A()
      x.a = 3        # calls A::set_a
      print x.a      # calls A::get_a

      x.b = 3        # calls A::b()
      print x.b      # calls A::b() const

  Use %attribute2 instead of %attribute to indicate that reference-pointer
  translation is required. You use %attribute2 instead of %attribute in
  cases like this:
 
  %attribute2(MyClass, MyFoo, Foo, GetFoo, SetFoo);
  %inline %{
    struct MyFoo { 
      int x;
    };
    class MyClass {
      MyFoo foo;
    public:
      MyFoo& GetFoo() { return foo; }
      void SetFoo(const MyFoo& other) { foo = other; }
    };
  %}

  Here, the data type of the property is a wrapped type (MyFoo) and on the
  C++ side it is passed by reference. The problem is that the SWIG wrapper will
  pass around a pointer (MyFoo *) which is not compatible with the reference
  type of the accessors (MyFoo &). Therefore, if you use %attribute, you'll get
  an error from your C/C++ compiler. %attribute2 translates between a pointer
  and a reference to eliminate the error. In case you're confused, let's make it
  simple: just use %attribute at first, but if the C/C++ compiler gives an error
  while compiling the wrapper, try %attribute2 instead.

  NOTE: remember that if the type contains commas, such as 'std::pair<int,int>',
  you need to use the macro like:

  %attributeref(A, %arg(std::pair<int,int>), pval);

  where %arg() 'normalizes' the type to be understood as a single
  argument, otherwise the macro will get confused by the comma.

  The %attributeval is the same as %attribute, but should be used when the type
  is a class/struct (ie a non-primitive type) and when the get and set methods 
  return/pass by value. The following is very similar to the above example, but 
  note that the access is by value rather than reference.

    %attributeval(MyClassVal, MyFoo, ReadWriteFoo, GetFoo, SetFoo);
    %attributeval(MyClassVal, MyFoo, ReadOnlyFoo, GetFoo);
    %inline %{
      class MyClassVal {
	MyFoo foo;
      public:
	MyFoo GetFoo() { return foo; }
	void SetFoo(MyFoo other) { foo = other; }
      };
    %} 

  The %attributestring is the same as %attributeval, but should be used for string
  class types, which are unusual as they are a class on the C++ side, but normally an
  immutable/primitive type in the target language. Example usage for std::string:

    %include <std_string.i>
    %attributestring(MyStringyClass, std::string, ReadWriteString, GetString, SetString);
    %attributestring(MyStringyClass, std::string, ReadOnlyString, GetString);
    %inline %{
      class MyStringyClass {
	std::string str;
      public:
	MyStringyClass(const std::string &val) : str(val) {}
	std::string GetString() { return str; }
	void SetString(std::string other) { str = other; }
      };
    %} 

  The %attributestring also works for class types that have %naturalvar turned
  on and so is also useful for shared_ptr which has %naturalvar turned on in %shared_ptr.

*/

//
// Define SWIG_ATTRIBUTE_TEMPLATE if you want to use templates instead of macros for the C++ get and set wrapper methods
// Does not always generate compilable code, use at your peril!
//
//#define SWIG_ATTRIBUTE_TEMPLATE

%define %attribute_custom(Class, AttributeType, AttributeName, GetMethod, SetMethod, GetMethodCall, SetMethodCall)
  %ignore Class::GetMethod();
  %ignore Class::GetMethod() const;
  #if #SetMethod != #AttributeName
    %ignore Class::SetMethod;
  #endif
  %extend Class {
    AttributeType AttributeName;
  }
#if defined(__cplusplus) && defined(SWIG_ATTRIBUTE_TEMPLATE)
  %{
    template < class C > inline AttributeType %mangle(Class) ##_## AttributeName ## _get(const C* self_) {
      return GetMethodCall;
    }
    template < class C > inline AttributeType %mangle(Class) ##_## AttributeName ## _get(C* self_) {
      return GetMethodCall;
    }
    template < class C > inline void %mangle(Class) ##_## AttributeName ## _set(C* self_, AttributeType val_) {
      SetMethodCall;
    }
  %}
#else
  %{
    #define %mangle(Class) ##_## AttributeName ## _get(self_) GetMethodCall
    #define %mangle(Class) ##_## AttributeName ## _set(self_, val_) SetMethodCall
  %}
#endif
%enddef

%define %attribute_readonly(Class, AttributeType, AttributeName, GetMethod, GetMethodCall)
  %ignore Class::GetMethod();
  %ignore Class::GetMethod() const;
  %immutable Class::AttributeName;
  %extend Class {
    AttributeType AttributeName;
  }
#if defined(__cplusplus) && defined(SWIG_ATTRIBUTE_TEMPLATE)
  %{
    template < class C > inline AttributeType %mangle(Class) ##_## AttributeName ## _get(const C* self_) {
      return GetMethodCall;
    }
    template < class C > inline AttributeType %mangle(Class) ##_## AttributeName ## _get(C* self_) {
      return GetMethodCall;
    }
  %}
#else
  %{
    #define %mangle(Class) ##_## AttributeName ## _get(self_) GetMethodCall
  %}
#endif
%enddef


// User macros

%define %attribute(Class, AttributeType, AttributeName, GetMethod, SetMethod...)
  #if #SetMethod != ""
    %attribute_custom(%arg(Class), %arg(AttributeType), AttributeName, GetMethod, SetMethod, self_->GetMethod(), self_->SetMethod(val_))
  #else
    %attribute_readonly(%arg(Class), %arg(AttributeType), AttributeName, GetMethod, self_->GetMethod())
  #endif
%enddef

%define %attribute2(Class, AttributeType, AttributeName, GetMethod, SetMethod...)
  #if #SetMethod != ""
    %attribute_custom(%arg(Class), %arg(AttributeType), AttributeName, GetMethod, SetMethod, &self_->GetMethod(), self_->SetMethod(*val_))
  #else
    %attribute_readonly(%arg(Class), %arg(AttributeType), AttributeName, GetMethod, &self_->GetMethod())
  #endif
%enddef

%define %attributeref(Class, AttributeType, AttributeName, AccessorMethod...)
  #if #AccessorMethod != ""
    %attribute_custom(%arg(Class), %arg(AttributeType), AttributeName, AccessorMethod, AccessorMethod, self_->AccessorMethod(), self_->AccessorMethod() = val_)
  #else
    %attribute_custom(%arg(Class), %arg(AttributeType), AttributeName, AttributeName, AttributeName, self_->AttributeName(), self_->AttributeName() = val_)
  #endif
%enddef

%define %attribute2ref(Class, AttributeType, AttributeName, AccessorMethod...)
  #if #AccessorMethod != ""
    %attribute_custom(%arg(Class), %arg(AttributeType), AttributeName, AccessorMethod, AccessorMethod, &self_->AccessorMethod(), self_->AccessorMethod() = *val_)
  #else
    %attribute_custom(%arg(Class), %arg(AttributeType), AccessorName, AccessorName, AccessorName, &self_->AccessorName(), self_->AccessorName() = *val_)
  #endif
%enddef

// deprecated (same as %attributeref, but there is an argument order inconsistency)
%define %attribute_ref(Class, AttributeType, AccessorMethod, AttributeName...)
  #if #AttributeName != ""
    %attribute_custom(%arg(Class), %arg(AttributeType), AttributeName, AccessorMethod, AccessorMethod, self_->AccessorMethod(), self_->AccessorMethod() = val_)
  #else
    %attribute_custom(%arg(Class), %arg(AttributeType), AccessorMethod, AccessorMethod, AccessorMethod, self_->AccessorMethod(), self_->AccessorMethod() = val_)
  #endif
%enddef


%define %attributeval(Class, AttributeType, AttributeName, GetMethod, SetMethod...)
  %{
    #define %mangle(Class) ##_## AttributeName ## _get(self_) new AttributeType(self_->GetMethod())
  %}
  #if #SetMethod != ""
    %{
      #define %mangle(Class) ##_## AttributeName ## _set(self_, val_) self_->SetMethod(*val_)
    %}
    #if #SetMethod != #AttributeName
      %ignore Class::SetMethod;
    #endif
  #else
    %immutable Class::AttributeName;
  #endif
  %ignore Class::GetMethod();
  %ignore Class::GetMethod() const;
  %newobject Class::AttributeName;
  %extend Class {
    AttributeType AttributeName;
  }
%enddef


%define %attributestring(Class, AttributeType, AttributeName, GetMethod, SetMethod...)
  %{
    #define %mangle(Class) ##_## AttributeName ## _get(self_) *new AttributeType(self_->GetMethod())
  %}
  #if #SetMethod != ""
    %{
      #define %mangle(Class) ##_## AttributeName ## _set(self_, val_) self_->SetMethod(val_)
    %}
    #if #SetMethod != #AttributeName
      %ignore Class::SetMethod;
    #endif
  #else
    %immutable Class::AttributeName;
  #endif
  %ignore Class::GetMethod();
  %ignore Class::GetMethod() const;
  %newobject Class::AttributeName;
  %typemap(newfree) const AttributeType &AttributeName "delete $1;"
  %extend Class {
    AttributeType AttributeName;
  }
%enddef

