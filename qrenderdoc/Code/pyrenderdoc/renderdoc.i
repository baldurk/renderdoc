%module renderdoc

%feature("autodoc", "0");
%feature("autodoc:noret", "1");

// just define linux platform to make sure things compile with no extra __declspec attributes
#define RENDERDOC_PLATFORM_LINUX

// we don't need these for the interface, they just confuse things
#define NO_ENUM_CLASS_OPERATORS

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text
#define DOCUMENT2(text1, text2) %feature("docstring") text1 text2
#define DOCUMENT3(text1, text2, text3) %feature("docstring") text1 text2 text3
#define DOCUMENT4(text1, text2, text3, text4) %feature("docstring") text1 text2 text3 text4

// ignore warning about base class rdcarray<char> methods in rdcstr
#pragma SWIG nowarn=401

// ignore warning about redundant declaration of typedef (byte)
#pragma SWIG nowarn=322

// strip off the RENDERDOC_ namespace prefix, it's unnecessary. We list this first since we want
// any other subsequent renames to override it.
%rename("%(strip:[RENDERDOC_])s") "";

// rename the interfaces to remove the I prefix
%rename("%(regex:/^I([A-Z].*)/\\1/)s", %$isclass) "";

// Since SWIG will inline all namespaces, and doesn't support nested structs, the namespaces
// for each pipeline state causes conflicts. We just fall back to a rename with _ as that's
// still acceptable/intuitive.
%rename("%(regex:/^D3D11Pipe::(.*)/D3D11_\\1/)s", regextarget=1, fullname=1, %$isclass) "D3D11Pipe::.*";
%rename("%(regex:/^D3D12Pipe::(.*)/D3D12_\\1/)s", regextarget=1, fullname=1, %$isclass) "D3D12Pipe::.*";
%rename("%(regex:/^GLPipe::(.*)/GL_\\1/)s", regextarget=1, fullname=1, %$isclass) "GLPipe::.*";
%rename("%(regex:/^VKPipe::(.*)/VK_\\1/)s", regextarget=1, fullname=1, %$isclass) "VKPipe::.*";

%begin %{

#undef slots

%}

%include "pyconversion.i"


// Add custom conversion/__str__/__repr__ functions for beautification
%include "cosmetics.i"

// ignore all the array member functions, only wrap the ones that are in python's list
%ignore rdcarray::rdcarray;
%ignore rdcarray::begin;
%ignore rdcarray::end;
%ignore rdcarray::front;
%ignore rdcarray::back;
%ignore rdcarray::data;
%ignore rdcarray::assign;
%ignore rdcarray::insert;
%ignore rdcarray::erase;
%ignore rdcarray::count;
%ignore rdcarray::capacity;
%ignore rdcarray::size;
%ignore rdcarray::byteSize;
%ignore rdcarray::empty;
%ignore rdcarray::isEmpty;
%ignore rdcarray::resize;
%ignore rdcarray::clear;
%ignore rdcarray::reserve;
%ignore rdcarray::swap;
%ignore rdcarray::push_back;
%ignore rdcarray::operator=;
%ignore rdcarray::operator[];
%ignore rdcstr::operator=;
%ignore rdcstr::operator std::string;

SIMPLE_TYPEMAPS(rdcstr)
SIMPLE_TYPEMAPS(bytebuf)

FIXED_ARRAY_TYPEMAPS(ResourceId)
FIXED_ARRAY_TYPEMAPS(double)
FIXED_ARRAY_TYPEMAPS(float)
FIXED_ARRAY_TYPEMAPS(bool)
FIXED_ARRAY_TYPEMAPS(uint64_t)
FIXED_ARRAY_TYPEMAPS(uint32_t)
FIXED_ARRAY_TYPEMAPS(int32_t)
FIXED_ARRAY_TYPEMAPS(uint16_t)

// these types are to be treated like python lists/arrays, and will be instantiated after declaration
// below
TEMPLATE_ARRAY_DECLARE(rdcarray);

///////////////////////////////////////////////////////////////////////////////////////////
// Actually include header files here. Note that swig is configured not to recurse, so we
// need to list all headers in include order that we want to process

%include <stdint.i>

%include "renderdoc_replay.h"
%include "basic_types.h"
%include "capture_options.h"
%include "control_types.h"
%include "d3d11_pipestate.h"
%include "d3d12_pipestate.h"
%include "data_types.h"
%include "gl_pipestate.h"
%include "replay_enums.h"
%include "shader_types.h"
%include "vk_pipestate.h"

%feature("docstring") "";

%extend rdcarray {
  // we ignored insert and clear before, need to restore them so we can declare our own impls
  %rename("%s") insert;
  %rename("%s") clear;
}

// add python array members that aren't in slots
EXTEND_ARRAY_CLASS_METHODS(rdcarray)

// list of array types. These are the concrete types used in rdcarray that will be bound
// If you get an error with add_your_use_of_rdcarray_to_swig_interface missing, add your type here
// or in qrenderdoc.i, depending on which one is appropriate
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, int)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, rdcstr)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, float)

///////////////////////////////////////////////////////////////////////////////////////////
// declare a function for passing external objects into python
%wrapper %{

PyObject *PassObjectToPython(const char *type, void *obj)
{
  swig_type_info *t = SWIG_TypeQuery(type);
  if(t == NULL)
    return NULL;

  return SWIG_InternalNewPointerObj(obj, t, 0);
}

%}

///////////////////////////////////////////////////////////////////////////////////////////
// Check documentation for types is set up correctly

%header %{
  #include <set>
  #include "Code/pyrenderdoc/document_check.h"

  // verify that docstrings aren't duplicated, which is a symptom of missing DOCUMENT()
  // macros around newly added classes/members.
  // For enums, verify that all constants are documented in the parent docstring
  static swig_type_info **docCheckTypes;
  static size_t docCheckNumTypes = 0;

  bool CheckCoreDocstrings()
  {
#if defined(RELEASE)
    return false;
#else
    if(docCheckNumTypes == 0)
      return false;

    return check_docstrings(docCheckTypes, docCheckNumTypes);
#endif
  }
%}

%init %{
  docCheckTypes = swig_type_initial;
  docCheckNumTypes = sizeof(swig_type_initial)/sizeof(swig_type_initial[0]);
%}
