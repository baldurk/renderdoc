%module(docstring="This is the API to QRenderDoc's high-level UI panels and functionality.") qrenderdoc

%feature("autodoc", "0");

// use documentation for docstrings
#define DOCUMENT(text) %feature("docstring") text

// import the renderdoc interface that we depend on
%import "renderdoc.i"

SIMPLE_TYPEMAPS(QString)
SIMPLE_TYPEMAPS(QDateTime)

CONTAINER_TYPEMAPS(QList &)
CONTAINER_TYPEMAPS(QList *)
CONTAINER_TYPEMAPS(QList)
CONTAINER_TYPEMAPS(QStringList &)
CONTAINER_TYPEMAPS(QStringList *)
CONTAINER_TYPEMAPS(QStringList)
CONTAINER_TYPEMAPS(QVector &)
CONTAINER_TYPEMAPS(QVector *)
CONTAINER_TYPEMAPS(QVector)
CONTAINER_TYPEMAPS(QMap &)
CONTAINER_TYPEMAPS(QMap *)
CONTAINER_TYPEMAPS(QMap)

// need to ignore the original function and add a helper that releases the python GIL while calling
%ignore IRenderManager::BlockInvoke;

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

  #include "Code/Interface/QRDInterface.h"
%}

%include <stdint.i>

%include "Code/Interface/QRDInterface.h"
%include "Code/Interface/CommonPipelineState.h"
%include "Code/Interface/PersistantConfig.h"
%include "Code/Interface/RemoteHost.h"

// unignore the function from above
%rename("%s") IRenderManager::BlockInvoke;

%extend IRenderManager {
  void BlockInvoke(InvokeMethod m) {
    Py_BEGIN_ALLOW_THREADS
    $self->BlockInvoke(m);
    Py_END_ALLOW_THREADS
  }
};

%init %{
  PyDateTime_IMPORT;
%}
