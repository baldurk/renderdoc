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

%fragment("tempalloc", "header") {
  template<typename T, bool is_pointer = std::is_pointer<T>::value>
  struct pointer_unwrap;

  template<typename T>
  struct pointer_unwrap<T, false>
  {
    static void tempset(T &ptr, T *tempobj)
    {
    }

    static void tempalloc(T &ptr, unsigned char *tempmem)
    {
    }

    static void tempdealloc(T &ptr)
    {
    }

    static T &indirect(T &ptr)
    {
      return ptr;
    }
  };

  template<typename T>
  struct pointer_unwrap<T, true>
  {
    typedef typename std::remove_pointer<T>::type U;

    static void tempset(U *&ptr, U *tempobj)
    {
      ptr = tempobj;
    }

    static void tempalloc(U *&ptr, unsigned char *tempmem)
    {
      ptr = new (tempmem) U;
    }

    static void tempdealloc(U *ptr)
    {
      ptr->~U();
    }

    static U &indirect(U *ptr)
    {
      return *ptr;
    }
  };

  template<typename T>
  void tempalloc(T &ptr, unsigned char *tempmem)
  {
    pointer_unwrap<T>::tempalloc(ptr, tempmem);
  }

  template<typename T, typename U>
  void tempset(T &ptr, U *tempobj)
  {
    pointer_unwrap<T>::tempset(ptr, tempobj);
  }

  template<typename T>
  void tempdealloc(T ptr)
  {
    pointer_unwrap<T>::tempdealloc(ptr);
  }

  template<typename T>
  typename std::remove_pointer<T>::type &indirect(T &ptr)
  {
    return pointer_unwrap<T>::indirect(ptr);
  }
}

%define SIMPLE_TYPEMAPS_VARIANT(BaseType, SimpleType)
%typemap(in, fragment="tempalloc,pyconvert") SimpleType (BaseType temp) {
  tempset($1, &temp);

  int res = ConvertFromPy($input, indirect($1));
  if(!SWIG_IsOK(res))
  {
    SWIG_exception_fail(SWIG_ArgError(res), "in method '$symname' argument $argnum of type '$1_basetype'"); 
  }
}

%typemap(out, fragment="tempalloc,pyconvert") SimpleType {
  $result = ConvertToPy(self, indirect($1));
}
%enddef

%define SIMPLE_TYPEMAPS(SimpleType)

SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType *)
SIMPLE_TYPEMAPS_VARIANT(SimpleType, SimpleType &)

%enddef

%define CONTAINER_TYPEMAPS(ContainerType)

%typemap(in, fragment="tempalloc,pyconvert") ContainerType (unsigned char tempmem[32]) {
  static_assert(sizeof(tempmem) >= sizeof(std::remove_pointer<decltype($1)>::type), "not enough temp space for $1_basetype");
  
  if(!PyList_Check($input))
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' list expected for argument $argnum of type '$1_basetype'"); 
  }

  tempalloc($1, tempmem);

  int failIdx = 0;
  int res = TypeConversion<std::remove_pointer<decltype($1)>::type>::ConvertFromPy($input, indirect($1), &failIdx);

  if(!SWIG_IsOK(res))
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', decoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ArgError(res), convert_error); 
  }
}

%typemap(freearg, fragment="tempalloc") ContainerType {
  tempdealloc($1);
}

%typemap(argout, fragment="tempalloc,pyconvert") ContainerType {
  // empty the previous contents
  if(PyDict_Check($input))
  {
    PyDict_Clear($input);
  }
  else
  {
    Py_ssize_t sz = PySequence_Size($input);
    if(sz > 0)
      PySequence_DelSlice($input, 0, sz);
  }

  // overwrite with array contents
  int failIdx = 0;
  PyObject *res = TypeConversion<std::remove_pointer<decltype($1)>::type>::ConvertToPyInPlace(self, $input, indirect($1), &failIdx);

  if(!res)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' argument $argnum of type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error); 
  }
}

%typemap(out, fragment="tempalloc,pyconvert") ContainerType {
  int failIdx = 0;
  $result = TypeConversion<std::remove_pointer<$1_basetype>::type>::ConvertToPy(self, indirect($1), &failIdx);
  if(!$result)
  {
    snprintf(convert_error, sizeof(convert_error)-1, "in method '$symname' returning type '$1_basetype', encoding element %d", failIdx);
    SWIG_exception_fail(SWIG_ValueError, convert_error);
  }
}

%enddef

SIMPLE_TYPEMAPS(rdctype::str)

CONTAINER_TYPEMAPS(rdctype::arr)

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

// SWIG generates destructor wrappers for these interfaces that we don't want
%ignore IReplayOutput::~IReplayOutput();
%ignore IReplayRenderer::~IReplayRenderer();
%ignore ITargetControl::~ITargetControl();
%ignore IRemoteServer::~IRemoteServer();

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

%header %{
  #include <set>
%}

%init %{
  // verify that docstrings aren't duplicated, which is a symptom of missing DOCUMENT()
  // macros around newly added classes/members.
  #if !defined(RELEASE)
  static bool doc_checked = false;

  if(!doc_checked)
  {
    doc_checked = true;

    std::set<std::string> docstrings;
    for(size_t i=0; i < sizeof(swig_type_initial)/sizeof(swig_type_initial[0]); i++)
    {
      SwigPyClientData *typeinfo = (SwigPyClientData *)swig_type_initial[i]->clientdata;

      // opaque types have no typeinfo, skip these
      if(!typeinfo) continue;

      PyTypeObject *typeobj = typeinfo->pytype;

      std::string typedoc = typeobj->tp_doc;

      auto result = docstrings.insert(typedoc);

      if(!result.second)
      {
        snprintf(convert_error, sizeof(convert_error)-1, "Duplicate docstring '%s' found on struct '%s' - are you missing a DOCUMENT()?", typedoc.c_str(), typeobj->tp_name);
        RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
      }

      PyMethodDef *method = typeobj->tp_methods;

      while(method->ml_doc)
      {
        std::string typedoc = method->ml_doc;

        size_t i = 0;
        while(typedoc[i] == '\n')
          i++;

        // skip the first line as it's autodoc generated
        i = typedoc.find('\n', i);
        if(i != std::string::npos)
        {
          while(typedoc[i] == '\n')
            i++;

          typedoc.erase(0, i);

          result = docstrings.insert(typedoc);

          if(!result.second)
          {
            snprintf(convert_error, sizeof(convert_error)-1, "Duplicate docstring '%s' found on method '%s' - are you missing a DOCUMENT()?", typedoc.c_str(), method->ml_name);
            RENDERDOC_LogMessage(LogType::Fatal, "QTRD", __FILE__, __LINE__, convert_error);
          }
        }

        method++;
      }
    }
  }
  #endif
%}
