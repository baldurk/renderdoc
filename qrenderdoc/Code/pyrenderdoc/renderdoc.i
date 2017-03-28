%module(docstring="This is the API to RenderDoc's internals.") renderdoc

%feature("autodoc", "0");

// just define linux platform to make sure things compile with no extra __declspec attributes
#define RENDERDOC_PLATFORM_LINUX

// we don't need these for the interface, they just confuse things
#define NO_ENUM_CLASS_OPERATORS

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text

// ignore warning about base class rdctype::array<char> methods in rdctype::str
#pragma SWIG nowarn=401

// ignore warning about redundant declaration of typedef (byte/bool32)
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

%fragment("pyconvert", "header") {
  static char convert_error[1024] = {};

  %#include "Code/pyrenderdoc/pyconversion.h"
}

%include "pyconversion.i"

SIMPLE_TYPEMAPS(rdctype::str)

CONTAINER_TYPEMAPS(rdctype::array)

%typemap(in, fragment="pyconvert") std::function {
  PyObject *func = $input;
  failed$argnum = false;
  $1 = ConvertFunc<$1_ltype>(self, "$symname", func, failed$argnum);
}

%typemap(argout) std::function (bool failed) {
  if(failed) SWIG_fail;
}

// ignore some operators SWIG doesn't have to worry about
%ignore rdctype::array::operator=;
%ignore rdctype::array::operator[];
%ignore rdctype::str::operator=;
%ignore rdctype::str::operator const char *;

// add __str__ functions
%feature("python:tp_str") ResourceId "resid_str";

%wrapper %{
static PyObject *resid_str(PyObject *resid)
{
  return PyUnicode_FromFormat("<ResourceId %S>", PyObject_GetAttrString(resid, "id"));
}
%}

%{
  #include "renderdoc_replay.h"
%}

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

// declare a function for passing external objects into python
%wrapper %{

PyObject *PassObjectToPython(const char *type, void *obj)
{
  swig_type_info *t = SWIG_TypeQuery(type);
  if(t == NULL)
    return NULL;

  return SWIG_InternalNewPointerObj(obj, t, 0);
}

// this is defined elsewhere for managing the opaque global_handle object
PyThreadState *GetExecutingThreadState(PyObject *global_handle);
void HandleException(PyObject *global_handle);

// this function handles failures in callback functions. If we're synchronously calling the callback from within an execute scope, then we can assign to failflag and let the error propagate upwards. If we're not, then the callback is being executed on another thread with no knowledge of python, so we need to use the global handle to try and emit the exception through the context. None of this is multi-threaded because we're inside the GIL at all times
void HandleCallbackFailure(PyObject *global_handle, bool &fail_flag)
{
  // if there's no global handle assume we are not running in the usual environment, so there are no external-to-python threads
  if(!global_handle)
  {
    fail_flag = true;
    return;
  }

  PyThreadState *current = PyGILState_GetThisThreadState();
  PyThreadState *executing = GetExecutingThreadState(global_handle);

  // we are executing synchronously, set the flag and return
  if(current == executing)
  {
    fail_flag = true;
    return;
  }

  // in this case we are executing asynchronously, and must handle the exception manually as there's nothing above us that knows about python exceptions
  HandleException(global_handle);
}

%}

%include "document_check.i"
