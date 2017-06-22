%module(docstring="This is the API to QRenderDoc's high-level UI panels and functionality.") qrenderdoc

%feature("autodoc", "0");

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text

%begin %{

#undef slots

%}

// import the renderdoc interface that we depend on
%import "renderdoc.i"

SIMPLE_TYPEMAPS(QString)
SIMPLE_TYPEMAPS(QDateTime)

CONTAINER_TYPEMAPS(QList)
CONTAINER_TYPEMAPS(QStringList)
CONTAINER_TYPEMAPS(QVector)
CONTAINER_TYPEMAPS(QMap)

// pass QWidget objects to PySide
%typemap(in) QWidget * {
  $1 = PythonContext::QWidgetFromPy($input);
  if($input && !$1)
  {
    SWIG_exception_fail(SWIG_TypeError, "in method '$symname' QWidget expected for argument $argnum of type '$1_basetype'");
  }
}

%typemap(out) QWidget * {
  $result = PythonContext::QWidgetToPy(self, $1);
}

// need to ignore the original function and add a helper that releases the python GIL while calling
%ignore IReplayManager::BlockInvoke;

// ignore these functions as we don't map QVariantMap to/from python
%ignore EnvironmentModification::toJSON;
%ignore EnvironmentModification::fromJSON;

// rename the interfaces to remove the I prefix
%rename("%(regex:/^I([A-Z].*)/\\1/)s", %$isclass) "";

%{
  #define ENABLE_QT_CONVERT

  #include <QDateTime>
  #include <QTimeZone>
  #include <QMap>
  #include <QString>
  #include <QList>
  #include <QVector>

  #include "datetime.h"

#ifndef slots
#define slots
#endif

  #include "Code/Interface/QRDInterface.h"
  #include "Code/pyrenderdoc/PythonContext.h"
%}

%include <stdint.i>

%include "Code/Interface/QRDInterface.h"
%include "Code/Interface/CommonPipelineState.h"
%include "Code/Interface/PersistantConfig.h"
%include "Code/Interface/RemoteHost.h"

// unignore the function from above
%rename("%s") IReplayManager::BlockInvoke;

%extend IReplayManager {
  void BlockInvoke(InvokeCallback m) {
    Py_BEGIN_ALLOW_THREADS
    $self->BlockInvoke(m);
    Py_END_ALLOW_THREADS
  }
};

%header %{
  #include <set>
  #include "Code/pyrenderdoc/document_check.h"
%}

%init %{
  PyDateTime_IMPORT;

  // verify that docstrings aren't duplicated, which is a symptom of missing DOCUMENT()
  // macros around newly added classes/members.
  // For enums, verify that all constants are documented in the parent docstring
  #if !defined(RELEASE)
  static bool doc_checked = false;

  if(!doc_checked)
  {
    doc_checked = true;

    check_docstrings(swig_type_initial, sizeof(swig_type_initial)/sizeof(swig_type_initial[0]));
  }
  #endif
%}

// declare functions for using swig opaque wrap/unwrap of QWidget, for when pyside isn't available.
%wrapper %{

PyObject *WrapBareQWidget(PyObject *self, QWidget *widget)
{
  return SWIG_NewPointerObj(SWIG_as_voidptr(widget), SWIGTYPE_p_QWidget, SWIG_BUILTIN_INIT);
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