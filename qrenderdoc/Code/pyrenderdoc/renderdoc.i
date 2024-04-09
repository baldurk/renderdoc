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

%header %{
#include "3rdparty/pythoncapi_compat.h"
%}

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

%rename("string") "SDObjectData::str";

// convenience - in C++ we have both duplicating and non-duplicating adds, but in python we only
// expose the duplicating add. Rename it to be simpler
%rename("AddChild") "SDObject::DuplicateAndAddChild";

%begin %{
#undef slots

#ifndef SWIG_GENERATED
#define SWIG_GENERATED
#endif

// we want visual assist to ignore this file, because it's a *lot* of generated code and has no
// useful results. This macro does nothing on normal builds, but is defined to _asm { in va_stdafx.h
#define VA_IGNORE_REST_OF_FILE
VA_IGNORE_REST_OF_FILE
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
%typemap(in) wl_surface* = HWND;

// completely ignore types that we custom convert to/from a native python type
%ignore rdcdatetime;
%ignore rdcstr;
%ignore rdcinflexiblestr;
%ignore rdcfixedarray;
%ignore rdcfixedarray::operator[];
%ignore rdcliteral;
%ignore rdcpair;
%ignore rdhalf;
%ignore bytebuf;

// special handling for RENDERDOC_GetDefaultCaptureOptions to transform output parameter to a return value
%typemap(in, numinputs=0) CaptureOptions *defaultOpts { $1 = new CaptureOptions; }
%typemap(argout) CaptureOptions *defaultOpts {
  $result = SWIG_NewPointerObj($1, $descriptor(struct CaptureOptions*), SWIG_POINTER_OWN);
}

// same for RENDERDOC_GetSupportedDeviceProtocols
%typemap(in, numinputs=0) rdcarray<rdcstr> *supportedProtocols { $1 = new rdcarray<rdcstr>; }
%typemap(argout) rdcarray<rdcstr> *supportedProtocols {
  $result = ConvertToPy(*$1);
  delete $1;
}
%typemap(freearg) rdcarray<rdcstr> *supportedProtocols { }

// same for RENDERDOC_CreateRemoteServerConnection
%typemap(in, numinputs=0) IRemoteServer **rend (IRemoteServer *outRenderer) {
  outRenderer = NULL;
  $1 = &outRenderer;
}
%typemap(argout) IRemoteServer **rend {
  PyObject *retVal = $result;
  $result = PyTuple_New(2);
  if($result)
  {
    PyTuple_SetItem($result, 0, retVal);
    PyTuple_SetItem($result, 1, SWIG_NewPointerObj(SWIG_as_voidptr(outRenderer$argnum), SWIGTYPE_p_IRemoteServer, 0));
  }
}

// ignore some operators SWIG doesn't have to worry about
%ignore *::operator=;
%ignore *::operator new;
%ignore *::operator delete;
%ignore *::operator new[];
%ignore *::operator delete[];

// ignore constructors/destructors for objects with disabled new/delete
%ignore SDType::SDType;
%ignore SDType::~SDType;
%ignore SDChunkMetaData::SDChunkMetaData;
%ignore SDChunkMetaData::~SDChunkMetaData;
%ignore SDObjectPODData::SDObjectPODData;
%ignore SDObjectPODData::~SDObjectPODData;
%ignore SDObjectData::SDObjectData;
%ignore SDObjectData::~SDObjectData;
%ignore StructuredObjectList::StructuredObjectList;
%ignore StructuredObjectList::~StructuredObjectList;
%ignore StructuredBufferList::StructuredBufferList;
%ignore StructuredBufferList::~StructuredBufferList;
%ignore StructuredChunkList::StructuredChunkList;
%ignore StructuredChunkList::~StructuredChunkList;

// don't allow user code to create ResultDetails objects (they can't allocate the string)
// or access the internal message, which we can't hide as ResultDetails must be POD.
%ignore ResultDetails::ResultDetails;
%ignore ResultDetails::internal_msg;

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

// simple typemap to delete old byte arrays in a buffer list before assigning the new one
%typemap(memberin) StructuredBufferList {
  // delete old byte arrays
  for(size_t i=0; i < $1.size(); i++)
    delete $1[i];

  // copy the values
  $1.assign(*$input);
}

SIMPLE_TYPEMAPS(rdcstr)
SIMPLE_TYPEMAPS(rdcinflexiblestr)
SIMPLE_TYPEMAPS(rdcdatetime)
SIMPLE_TYPEMAPS(bytebuf)

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
TEMPLATE_FIXEDARRAY_DECLARE(rdcfixedarray);

///////////////////////////////////////////////////////////////////////////////////////////
// Actually include header files here. Note that swig is configured not to recurse, so we
// need to list all headers in include order that we want to process

%include <stdint.i>

%include "apidefs.h"
%include "renderdoc_replay.h"
%include "resourceid.h"
%include "rdcarray.h"
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

  %feature("docstring") R"(Returns a string representation of an object. This is quite similar to
the built-in repr() function but it iterates over struct members and prints them out, where normally
repr() would stop and say something like 'Swig Object of type ...'.

:param Any obj: The object to dump
:return: The string representation of the object.
:rtype: str
)";

%inline %{
  
extern "C" PyObject *RENDERDOC_DumpObject(PyObject *obj);

%}

  %feature("docstring") "";

%extend SDObject {
  %feature("docstring") R"(Interprets the object as an integer and returns its value.
Invalid if the object is not actually an integer.

:return: The interpreted integer.
:rtype: int
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

:return: The interpreted float.
:rtype: float
)";
  PyObject *AsFloat() { return ConvertToPy($self->data.basic.d); }
  
  %feature("docstring") R"(Interprets the object as a string and returns its value.
Invalid if the object is not actually a string.

:return: The interpreted string.
:rtype: str
)";
  PyObject *AsString() { return ConvertToPy($self->data.str); }
}

// add python array members that aren't in slots
EXTEND_ARRAY_CLASS_METHODS(rdcarray)
EXTEND_ARRAY_CLASS_METHODS(StructuredChunkList)
EXTEND_ARRAY_CLASS_METHODS(StructuredObjectList)
EXTEND_ARRAY_CLASS_METHODS(StructuredBufferList)

// If you get an error with add_your_use_of_rdcfixedarray_to_swig_interface missing, add your type here
// or in qrenderdoc.i, depending on which one is appropriate
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, float, 2)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, float, 4)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint32_t, 3)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint32_t, 4)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint64_t, 4)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, int32_t, 4)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, ResourceId, 4)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, ResourceId, 8)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, bool, 4)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, bool, 8)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, float, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, rdhalf, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, int32_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint32_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, double, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint64_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, int64_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint16_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, int16_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, uint8_t, 16)
TEMPLATE_FIXEDARRAY_INSTANTIATE(rdcfixedarray, int8_t, 16)

// list of array types. These are the concrete types used in rdcarray that will be bound
// If you get an error with add_your_use_of_rdcarray_to_swig_interface missing, add your type here
// or in qrenderdoc.i, depending on which one is appropriate
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, int)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, float)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, uint32_t)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, uint64_t)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, rdcstr)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, WindowingSystem)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ActionDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, GPUCounter)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, CounterResult)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, APIEvent)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DescriptorStoreDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BufferDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, CaptureFileFormat)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ConstantBlock)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DebugMessage)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, EnvironmentModification)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, EventUsage)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, PathEntry)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, PixelModification)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, TaskGroupSize)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, MeshletSize)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ResourceDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ResourceId)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, LineColumnInfo)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, InstructionSourceInfo)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderCompileFlag)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderConstant)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderDebugState)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderMessage)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderResource)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderSampler)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderSourceFile)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderSourcePrefix)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderVariable)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderEncoding)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderVariableChange)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DebugVariableReference)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, SourceVariableMapping)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, SigParameter)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, TextureDescription)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderEntryPoint)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Viewport)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Scissor)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ColorBlend)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BoundVBuffer)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Offset)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, VertexInputAttribute)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, FloatVector)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, GraphicsAPI)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, GPUDevice)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderConstantType)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderChangeStats)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ResourceBindStats)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, SamplerBindStats)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ConstantBindStats)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DescriptorRange)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, Descriptor)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, SamplerDescriptor)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DescriptorAccess)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DescriptorLogicalLocation)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, UsedDescriptor)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, DynamicOffset)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, DescriptorSet)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, ImageData)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, ImageLayout)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, RenderArea)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, XFBBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, VertexAttribute)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, VertexBinding)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, VKPipe, ViewportScissor)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, Layout)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, StreamOutBind)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D11Pipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, Layout)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, ResourceData)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, ResourceState)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, StreamOutBind)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, RootTableRange)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, RootParam)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, D3D12Pipe, StaticSampler)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, VertexBuffer)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, VertexAttribute)
TEMPLATE_NAMESPACE_ARRAY_INSTANTIATE(rdcarray, GLPipe, TextureCompleteness)

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

extern "C" PyObject *RENDERDOC_DumpObject(PyObject *obj)
{
  void *resptr = NULL;

  // for basic types, return the repr directly
  if(Py_IsTrue(obj) ||
     Py_IsFalse(obj) ||
     Py_IsNone(obj) ||
     PyObject_IsInstance(obj, (PyObject*)&PyFloat_Type) ||
     PyObject_IsInstance(obj, (PyObject*)&PyLong_Type) ||
     PyObject_IsInstance(obj, (PyObject*)&PyBytes_Type) ||
     PyObject_IsInstance(obj, (PyObject*)&PyUnicode_Type) ||
     PyObject_IsInstance(obj, (PyObject*)&PyList_Type) ||
     PyObject_IsInstance(obj, (PyObject*)&PyDict_Type) ||
     PyObject_IsInstance(obj, (PyObject*)&PyTuple_Type))
  {
    return PyObject_Repr(obj);
  }

  // also for ResourceId
  if(SWIG_ConvertPtr(obj, &resptr, SWIGTYPE_p_ResourceId, 0) != -1)
  {
    return PyObject_Repr(obj);
  }

  PyObject *ret;
  
  if(PySequence_Check(obj))
  {
    ret = PyList_New(0);

    Py_ssize_t size = PySequence_Size(obj);

    for(Py_ssize_t i = 0; i < size; i++)
    {
      PyObject *entry = PySequence_GetItem(obj, i);

      // don't add callables
      if(PyCallable_Check(entry) == 0)
      {
        PyObject *childDump = RENDERDOC_DumpObject(entry);
        PyList_Append(ret, childDump);
        Py_XDECREF(childDump);
      }

      Py_XDECREF(entry);
    }
  }
  else
  {
    ret = PyDict_New();

    // otherwise iterate over the dir
    PyObject *dir = PyObject_Dir(obj);

    Py_ssize_t size = PyList_Size(dir);

    for(Py_ssize_t i = 0; i < size; i++)
    {
      PyObject *member = PyList_GetItem(dir, i);

      PyObject *bytes = PyUnicode_AsUTF8String(member);

      if(!bytes)
        continue;

      char *buf = NULL;
      Py_ssize_t size = 0;

      if(PyBytes_AsStringAndSize(bytes, &buf, &size) == 0)
      {
        rdcstr name;
        name.assign(buf, size);

        if(name.beginsWith("__") || name == "this" || name == "thisown" || name == "acquire")
        {
          // skip this member, it's internal
        }
        else
        {
          PyObject *child = PyObject_GetAttr(obj, member);

          // don't add callables
          if(PyCallable_Check(child) == 0)
          {
            PyObject *childDump = RENDERDOC_DumpObject(child);
            PyDict_SetItem(ret, member, childDump);
            Py_XDECREF(childDump);
          }
        }
      }

      Py_XDECREF(bytes);
    }

    Py_XDECREF(dir);
  }

  return ret;
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

  bool CheckCoreInterface(rdcstr &log)
  {
#if defined(RELEASE)
    return false;
#else
    if(interfaceCheckNumTypes == 0)
      return false;

    return check_interface(log, interfaceCheckTypes, interfaceCheckNumTypes);
#endif
  }
%}

%init %{
  interfaceCheckTypes = swig_type_initial;
  interfaceCheckNumTypes = sizeof(swig_type_initial)/sizeof(swig_type_initial[0]);
%}
