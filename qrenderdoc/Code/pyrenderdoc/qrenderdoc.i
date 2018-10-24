%module(docstring="This is the API to QRenderDoc's high-level UI panels and functionality.") qrenderdoc

%feature("autodoc", "0");

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text
#define DOCUMENT2(text1, text2) %feature("docstring") text1 text2
#define DOCUMENT3(text1, text2, text3) %feature("docstring") text1 text2 text3
#define DOCUMENT4(text1, text2, text3, text4) %feature("docstring") text1 text2 text3 text4

%begin %{
  #undef slots

  #define SWIG_GENERATED
%}

%{
  #include "datetime.h"
%}
%init %{
  PyDateTime_IMPORT;
%}

%include "pyconversion.i"

// import the renderdoc interface that we depend on
%import "renderdoc.i"

TEMPLATE_ARRAY_DECLARE(rdcarray);

// pass QWidget objects to PySide
%{
  class QWidget;

  extern "C" QWidget *QWidgetFromPy(PyObject *widget);
  extern "C" PyObject *QWidgetToPy(QWidget *widget);

  DECLARE_STRINGISE_TYPE(QWidget);
%}

%typemap(in) QWidget * {
  if($input == Py_None)
    $1 = NULL;
  else
    $1 = QWidgetFromPy($input);
  if($input && $input != Py_None && !$1)
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' QWidget expected for argument $argnum of type '$1_basetype'");
  }
}

%typemap(out) QWidget * {
  $result = QWidgetToPy($1);
}

// need to ignore the original function and add a helper that releases the python GIL while calling
%ignore IReplayManager::BlockInvoke;

// ignore these functions as we don't map QVariantMap to/from python
%ignore EnvironmentModification::toJSON;
%ignore EnvironmentModification::fromJSON;

// rename the interfaces to remove the I prefix
%rename("%(regex:/^I([A-Z].*)/\\1/)s", %$isclass) "";

%{
  #include "Code/Interface/QRDInterface.h"

  #ifndef slots
  #define slots
  #endif
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

  bool CheckQtInterface()
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