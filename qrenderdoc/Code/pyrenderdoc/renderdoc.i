%module renderdoc

%feature("autodoc", "0");
%feature("autodoc:noret", "1");

// just define linux platform to make sure things compile with no extra __declspec attributes
#define RENDERDOC_PLATFORM_LINUX

// we don't need these for the interface, they just confuse things
#define NO_ENUM_CLASS_OPERATORS

// swig's pre-processor trips up on this definition, and it's only needed for template expansions
// in the conversion code
#define DECLARE_REFLECTION_STRUCT(t)
#define DECLARE_REFLECTION_ENUM(t)

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text
#define DOCUMENT2(text1, text2) %feature("docstring") text1 text2
#define DOCUMENT3(text1, text2, text3) %feature("docstring") text1 text2 text3
#define DOCUMENT4(text1, text2, text3, text4) %feature("docstring") text1 text2 text3 text4

// include header for typed enums (hopefully using PEP435 enums)
%include <enums.swg>

// used for declaring python IntFlag type enums
#define ENABLE_PYTHON_FLAG_ENUMS %feature("python:enum:flag");
#define DISABLE_PYTHON_FLAG_ENUMS %feature("python:enum:flag", "");

// ignore warning about base class rdcarray<char> methods in rdcstr, and similar warning in structured lists
#pragma SWIG nowarn=401
#pragma SWIG nowarn=315

// ignore warning about redundant declaration of typedef (byte)
#pragma SWIG nowarn=322

// strip off the RENDERDOC_ namespace prefix, it's unnecessary. We list this first since we want
// any other subsequent renames to override it.
%rename("%(strip:[RENDERDOC_])s") "";

// rename the interfaces to remove the I prefix
%rename("%(regex:/^I([A-Z].*)/\\1/)s", %$isclass) "";

// Since SWIG will inline all namespaces, and doesn't support nested structs, the namespaces
// for each pipeline state causes conflicts. We just fall back to a rename with just a concatenated
// prefix as that's still acceptable/intuitive. We don't have a _ as that's less pythonic naming
// (not that we follow python naming perfectly)
%rename("%(regex:/^D3D11Pipe::(.*)/D3D11\\1/)s", regextarget=1, fullname=1, %$isclass) "D3D11Pipe::.*";
%rename("%(regex:/^D3D12Pipe::(.*)/D3D12\\1/)s", regextarget=1, fullname=1, %$isclass) "D3D12Pipe::.*";
%rename("%(regex:/^GLPipe::(.*)/GL\\1/)s", regextarget=1, fullname=1, %$isclass) "GLPipe::.*";
%rename("%(regex:/^VKPipe::(.*)/VK\\1/)s", regextarget=1, fullname=1, %$isclass) "VKPipe::.*";

%begin %{
  #undef slots
%}

%{
  #include "datetime.h"
%}
%init %{
  PyDateTime_IMPORT;
%}

%include "pyconversion.i"

// typemaps for windowing data
%typemap(in) HWND (unsigned long long tmp, int err = 0) {
  // convert windowing data pointers from just plain integers
  err = SWIG_AsVal_unsigned_SS_long_SS_long($input, &tmp);
  if (!SWIG_IsOK(err)) {
    %argument_fail(err, "$*ltype", $symname, $argnum);
  } 
  $1 = ($1_type)tmp;
}

%typemap(in) Display* = HWND;
%typemap(in) xcb_connection_t* = HWND;

// completely ignore rdcdatetime, we custom convert to/from a native python datetime
%ignore rdcdatetime;

// special handling for RENDERDOC_GetDefaultCaptureOptions to transform output parameter to a return value
%typemap(in, numinputs=0) CaptureOptions *defaultOpts { $1 = new CaptureOptions; }
%typemap(argout) CaptureOptions *defaultOpts {
  $result = SWIG_NewPointerObj($1, $descriptor(struct CaptureOptions*), SWIG_POINTER_OWN);
}

// ignore some operators SWIG doesn't have to worry about
%ignore SDType::operator=;
%ignore StructuredObjectList::swap;
%ignore StructuredChunkList::swap;
%ignore StructuredObjectList::operator=;
%ignore StructuredObjectList::operator=;
%ignore StructuredChunkList::operator=;
%ignore StructuredBufferList::operator=;

// these objects return a new copy which the python caller should own.
%newobject SDObject::Duplicate;
%newobject SDChunk::Duplicate;
%newobject makeSDObject;
%newobject makeSDArray;
%newobject makeSDStruct;

// Add custom conversion/__str__/__repr__ functions for beautification
%include "cosmetics.i"

// ignore all the array member functions, only wrap the ones that are in python's list
%ignore rdcarray::rdcarray;
%ignore rdcarray::begin;
%ignore rdcarray::end;
%ignore rdcarray::front;
%ignore rdcarray::back;
%ignore rdcarray::at;
%ignore rdcarray::data;
%ignore rdcarray::assign;
%ignore rdcarray::insert;
%ignore rdcarray::append;
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
%ignore rdcarray::takeAt;
%ignore rdcarray::indexOf;
%ignore rdcarray::contains;
%ignore rdcarray::removeOne;
%ignore rdcarray::operator=;
%ignore rdcarray::operator[];
%ignore rdcstr::operator=;
%ignore rdcstr::operator std::string;
%ignore rdcpair::operator=;
%ignore rdcpair::swap;

// simple typemap to delete old byte arrays in a buffer list before assigning the new one
%typemap(memberin) StructuredBufferList {
  // delete old byte arrays
  for(size_t i=0; i < $1.size(); i++)
    delete $1[i];

  // copy the values
  $1.assign(*$input);
}

SIMPLE_TYPEMAPS(rdcstr)
SIMPLE_TYPEMAPS(rdcdatetime)
SIMPLE_TYPEMAPS(bytebuf)

FIXED_ARRAY_TYPEMAPS(ResourceId)
FIXED_ARRAY_TYPEMAPS(double)
FIXED_ARRAY_TYPEMAPS(float)
FIXED_ARRAY_TYPEMAPS(bool)
FIXED_ARRAY_TYPEMAPS(uint64_t)
FIXED_ARRAY_TYPEMAPS(int64_t)
FIXED_ARRAY_TYPEMAPS(uint32_t)
FIXED_ARRAY_TYPEMAPS(int32_t)
FIXED_ARRAY_TYPEMAPS(uint16_t)
FIXED_ARRAY_TYPEMAPS(int16_t)
FIXED_ARRAY_TYPEMAPS(uint8_t)
FIXED_ARRAY_TYPEMAPS(int8_t)

REFCOUNTED_TYPE(SDChunk);
REFCOUNTED_TYPE(SDObject);

// Not really, but we do want to handle deleting it and removing refcounted children
REFCOUNTED_TYPE(SDFile);

// these arrays contain refcounted members
DEFINE_REFCOUNTED_ARRAY(StructuredChunkList);
DEFINE_REFCOUNTED_ARRAY(StructuredObjectList);

// these types are to be treated like python lists/arrays
NON_TEMPLATE_ARRAY_INSTANTIATE(StructuredChunkList)
NON_TEMPLATE_ARRAY_INSTANTIATE(StructuredObjectList)
NON_TEMPLATE_ARRAY_INSTANTIATE(StructuredBufferList)

// these types are to be treated like python lists/arrays, and will be instantiated after declaration
// below
TEMPLATE_ARRAY_DECLARE(rdcarray);

///////////////////////////////////////////////////////////////////////////////////////////
// Actually include header files here. Note that swig is configured not to recurse, so we
// need to list all headers in include order that we want to process

%include <stdint.i>

%include "renderdoc_replay.h"
%include "basic_types.h"
%include "stringise.h"
%include "structured_data.h"
%include "capture_options.h"
%include "control_types.h"
%include "common_pipestate.h"
%include "d3d11_pipestate.h"
%include "d3d12_pipestate.h"
%include "data_types.h"
%include "gl_pipestate.h"
%include "replay_enums.h"
%include "shader_types.h"
%include "vk_pipestate.h"
%include "pipestate.h"

%feature("docstring") "";

%extend rdcarray {
  // we ignored some functions before, need to restore them so we can declare our own impls
  %rename("%s") insert;
  %rename("%s") append;
  %rename("%s") clear;
  %rename("%s") count;
}

%extend SDObject {
  %feature("docstring") R"(Interprets the object as an integer and returns its value.
Invalid if the object is not actually an integer.
)";
  PyObject *AsInt()
  {
    if($self->type.basetype == SDBasic::UnsignedInteger)
      return ConvertToPy($self->data.basic.u);
    else
      return ConvertToPy($self->data.basic.i);
  }
  
  %feature("docstring") R"(Interprets the object as a floating point number and returns its value.
Invalid if the object is not actually a floating point number.
)";
  PyObject *AsFloat() { return ConvertToPy($self->data.basic.d); }
  
  %feature("docstring") R"(Interprets the object as a string and returns its value.
Invalid if the object is not actually a string.
)";
  PyObject *AsString() { return ConvertToPy($self->data.str); }
}

// add python array members that aren't in slots
EXTEND_ARRAY_CLASS_METHODS(rdcarray)
EXTEND_ARRAY_CLASS_METHODS(StructuredChunkList)
EXTEND_ARRAY_CLASS_METHODS(StructuredObjectList)
EXTEND_ARRAY_CLASS_METHODS(StructuredBufferList)

// list of array types. These are the concrete types used in rdcarray that will be bound
// If you get an error with add_your_use_of_rdcarray_to_swig_interface missing, add your type here
// or in qrenderdoc.i, depending on which one is appropriate
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, int)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, float)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, uint32_t)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, uint64_t)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, rdcstr)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, WindowingSystem)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DrawcallDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, GPUCounter)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, CounterResult)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, APIEvent)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Bindpoint)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BufferDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, CaptureFileFormat)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ConstantBlock)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DebugMessage)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, EnvironmentModification)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, EventUsage)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, PathEntry)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, PixelModification)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ResourceDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ResourceId)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, LineColumnInfo)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderCompileFlag)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderConstant)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderDebugState)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderResource)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderSampler)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderSourceFile)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderVariable)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderEncoding)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, RegisterRange)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, LocalVariableMapping)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, SigParameter)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, TextureDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderEntryPoint)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Viewport)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Scissor)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ColorBlend)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BoundVBuffer)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, VertexInputAttribute)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BoundResource)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BoundResourceArray)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, FloatVector)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, Attachment)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, BindingElement)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, DescriptorBinding)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, DescriptorSet)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, ImageData)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, ImageLayout)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, RenderArea)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, SpecializationConstant)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, XFBBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, VertexAttribute)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, VertexBinding)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, ViewportScissor)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, ConstantBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, Layout)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, Sampler)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, StreamOutBind)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, View)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, ConstantBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, Layout)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, RegisterSpace)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, ResourceData)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, ResourceState)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, Sampler)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, StreamOutBind)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, View)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, Attachment)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, Buffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, ImageLoadStore)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, Sampler)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, Texture)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, VertexAttribute)

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

PyObject *PassNewObjectToPython(const char *type, void *obj)
{
  swig_type_info *t = SWIG_TypeQuery(type);
  if(t == NULL)
    return NULL;

  return SWIG_InternalNewPointerObj(obj, t, SWIG_POINTER_OWN);
}

%}

///////////////////////////////////////////////////////////////////////////////////////////
// Check documentation for types is set up correctly

%header %{
  #include <set>
  #include "Code/pyrenderdoc/interface_check.h"

  // check interface, see interface_check.h for more information
  static swig_type_info **interfaceCheckTypes;
  static size_t interfaceCheckNumTypes = 0;

  bool CheckCoreInterface()
  {
#if defined(RELEASE)
    return false;
#else
    if(interfaceCheckNumTypes == 0)
      return false;

    return check_interface(interfaceCheckTypes, interfaceCheckNumTypes);
#endif
  }
%}

%init %{
  interfaceCheckTypes = swig_type_initial;
  interfaceCheckNumTypes = sizeof(swig_type_initial)/sizeof(swig_type_initial[0]);
%}
