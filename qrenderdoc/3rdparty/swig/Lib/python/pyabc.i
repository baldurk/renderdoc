%define %pythonabc(Type, Abc)
  %feature("python:abc", #Abc) Type;
%enddef
%pythoncode %{import collections%}
%pythonabc(std::vector, collections.MutableSequence);
%pythonabc(std::list, collections.MutableSequence);
%pythonabc(std::map, collections.MutableMapping);
%pythonabc(std::multimap, collections.MutableMapping);
%pythonabc(std::set, collections.MutableSet);
%pythonabc(std::multiset, collections.MutableSet);
