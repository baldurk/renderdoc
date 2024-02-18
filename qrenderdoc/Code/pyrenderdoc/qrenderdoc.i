
%module(docstring="This is the API to QRenderDoc's high-level UI panels and functionality.") qrenderdoc

%feature("autodoc", "0");

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text
#define DOCUMENT2(text1, text2) %feature("docstring") text1 text2
#define DOCUMENT3(text1, text2, text3) %feature("docstring") text1 text2 text3
#define DOCUMENT4(text1, text2, text3, text4) %feature("docstring") text1 text2 text3 text4

%header %{
#include "3rdparty/pythoncapi_compat.h"
%}

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

%{
  #include "Code/Interface/QRDInterface.h"
%}

%include "pyconversion.i"

// import the renderdoc interface that we depend on
%import "renderdoc.i"

TEMPLATE_ARRAY_DECLARE(rdcarray);
TEMPLATE_FIXEDARRAY_DECLARE(rdcfixedarray);

// pass QWidget objects to PySide
%{
  class QWidget;

  extern "C" QWidget *QWidgetFromPy(PyObject *widget);
  extern "C" PyObject *QWidgetToPy(QWidget *widget);

  DECLARE_STRINGISE_TYPE(QWidget);
%}

%typemap(in) QWidget * {
  if(Py_IsNone($input))
    $1 = NULL;
  else
    $1 = QWidgetFromPy($input);
  if($input && !Py_IsNone($input) && !$1)
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' QWidget expected for argument $argnum of type '$1_basetype'");
  }
}

%typemap(out) QWidget * {
  $result = QWidgetToPy($1);
}

// create a wrapper for passing python ICaptureViewer interface implementations to C++

%{
  struct PythonCaptureViewer : public ICaptureViewer
  {
    PythonCaptureViewer(PyObject *s) : self(s)
    {
      Py_INCREF(self);
      
      StackExceptionHandler ex;

      PyObject *meth = NULL;

      {
        meth = PyObject_GetAttrString(self, "OnCaptureLoaded");
        m_OnCaptureLoaded = ConvertFunc<std::function<void()>>("ICaptureViewer::OnCaptureLoaded", meth, ex);
        Py_XDECREF(meth);
      }

      {
        meth = PyObject_GetAttrString(self, "OnCaptureClosed");
        m_OnCaptureClosed = ConvertFunc<std::function<void()>>("ICaptureViewer::OnCaptureClosed", meth, ex);
        Py_XDECREF(meth);
      }

      {
        meth = PyObject_GetAttrString(self, "OnSelectedEventChanged");
        m_OnSelectedEventChanged = ConvertFunc<std::function<void(uint32_t)>>("ICaptureViewer::OnSelectedEventChanged", meth, ex);
        Py_XDECREF(meth);
      }

      {
        meth = PyObject_GetAttrString(self, "OnEventChanged");
        m_OnEventChanged = ConvertFunc<std::function<void(uint32_t)>>("ICaptureViewer::OnEventChanged", meth, ex);
        Py_XDECREF(meth);
      }
    }

    virtual ~PythonCaptureViewer()
    {
      Py_DECREF(self);
    }
    void OnCaptureLoaded() override { if(m_OnCaptureLoaded) m_OnCaptureLoaded(); }
    void OnCaptureClosed() override { if(m_OnCaptureClosed) m_OnCaptureClosed(); }
    void OnSelectedEventChanged(uint32_t eventId) override { if(m_OnSelectedEventChanged) m_OnSelectedEventChanged(eventId); }
    void OnEventChanged(uint32_t eventId) override { if(m_OnEventChanged) m_OnEventChanged(eventId); }

  private:
    PyObject *self;

    std::function<void()> m_OnCaptureLoaded, m_OnCaptureClosed;
    std::function<void(uint32_t)> m_OnSelectedEventChanged, m_OnEventChanged;
  };

  static int capviewer_init(PyObject *self, PyObject *args) {
    PyObject *resultobj = 0;
    ICaptureViewer *result = 0;

    result = new PythonCaptureViewer(self);
    resultobj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_ICaptureViewer, SWIG_BUILTIN_INIT | 0);
    return Py_IsNone(resultobj) ? -1 : 0;
  }

  SWIGINTERN PyObject *capviewer_deinit(PyObject *self, PyObject *args)
  {
    PythonCaptureViewer *viewer = NULL;
    void *ptr = NULL;
    int res = SWIG_ConvertPtr(self, &ptr, SWIGTYPE_p_ICaptureViewer, SWIG_POINTER_DISOWN | 0);

    if(!SWIG_IsOK(res))
    {
      SWIG_exception_fail(SWIG_ArgError(res), "in method '" "delete_CaptureViewer" "', argument " "1"" of type '" "CaptureViewer *""'");
    }

    viewer = (PythonCaptureViewer *)ptr;
    delete viewer;

    return SWIG_Py_Void();
  fail:
    return NULL;
  }

SWIGPY_DESTRUCTOR_CLOSURE(capviewer_deinit) /* defines capviewer_deinit_destructor_closure */

%}

%feature("python:tp_init") ICaptureViewer "&capviewer_init";
%feature("python:tp_dealloc") ICaptureViewer "&capviewer_deinit_destructor_closure";


// need to ignore the original function and add a helper that releases the python GIL while calling
%ignore IReplayManager::BlockInvoke;

// ignore these functions as we don't map QVariantMap to/from python
%ignore EnvironmentModification::toJSON;
%ignore EnvironmentModification::fromJSON;

// rename the interfaces to remove the I prefix
%rename("%(regex:/^I([A-Z].*)/\\1/)s", %$isclass) "";

%{
  #ifndef slots
  #define slots
  #endif

  DECLARE_STRINGISE_TYPE(rdcstrpair);
%}

%include <stdint.i>

%include "Code/Interface/QRDInterface.h"
%include "Code/Interface/PersistantConfig.h"
%include "Code/Interface/RemoteHost.h"
%include "Code/Interface/Extensions.h"

DOCUMENT("");

TEMPLATE_ARRAY_INSTANTIATE(rdcarray, EventBookmark)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ShaderProcessingTool)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, rdcstrpair)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, BugReport)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, ExtensionMetadata)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, DialogButton)
TEMPLATE_ARRAY_INSTANTIATE(rdcarray, RemoteHost)
TEMPLATE_ARRAY_INSTANTIATE_PTR(rdcarray, ICaptureViewer)

// unignore the function from above
%rename("%s") IReplayManager::BlockInvoke;

%extend IReplayManager {
  void BlockInvoke(InvokeCallback m) {
    PyObject *global_internal_handle = NULL;

    PyObject *globals = PyEval_GetGlobals();
    if(globals)
      global_internal_handle = PyDict_GetItemString(globals, "_renderdoc_internal");

    SetThreadBlocking(global_internal_handle, true);

    Py_BEGIN_ALLOW_THREADS
    $self->BlockInvoke(m);
    Py_END_ALLOW_THREADS

    SetThreadBlocking(global_internal_handle, false);
  }
};

%header %{
  #include <set>
  #include "Code/pyrenderdoc/interface_check.h"

  // check interface, see interface_check.h for more information
  static swig_type_info **interfaceCheckTypes;
  static size_t interfaceCheckNumTypes = 0;

  bool CheckQtInterface(rdcstr &log)
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

// declare functions for using swig opaque wrap/unwrap of QWidget, for when pyside isn't available.
%wrapper %{

PyObject *WrapBareQWidget(QWidget *widget)
{
  return SWIG_InternalNewPointerObj(SWIG_as_voidptr(widget), SWIGTYPE_p_QWidget, 0);
}

QWidget *UnwrapBareQWidget(PyObject *obj)
{
  QWidget *ret = NULL;
  int res = 0;

  res = SWIG_ConvertPtr(obj, (void **)&ret,SWIGTYPE_p_QWidget, 0);
  if(!SWIG_IsOK(res))
  {
    return NULL;
  }

  return ret;
}

%}
