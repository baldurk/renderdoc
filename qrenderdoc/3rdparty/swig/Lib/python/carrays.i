%define %array_class(TYPE,NAME)
#if defined(SWIGPYTHON_BUILTIN)
  %feature("python:slot", "sq_item", functype="ssizeargfunc") NAME::__getitem__;
  %feature("python:slot", "sq_ass_item", functype="ssizeobjargproc") NAME::__setitem__;
#endif
%array_class_wrap(TYPE,NAME,__getitem__,__setitem__)
%enddef

%include <typemaps/carrays.swg>




