/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QWidget>
#include <typeinfo>
#include "Code/QRDUtils.h"

class QThread;

typedef struct _object PyObject;
typedef struct _frame PyFrameObject;
typedef struct _ts PyThreadState;

struct PyParseError
{
  int lineno = -1, offset = -1;
  rdcstr errStr;
};

class PythonContext : public QObject
{
private:
  Q_OBJECT

  // don't allow destruction from outside, you must heap-allocate the context and let it delete
  // itself when all references are done. This handles the case where e.g. some Async work is going
  // on and needs to finish executing after the external code is done with the context
  ~PythonContext();

  explicit PythonContext(bool extensionContext, QObject *parent);
public:
  explicit PythonContext(QObject *parent = NULL) : PythonContext(false, parent) {}
  void Finish();

  PyThreadState *GetExecutingThreadState() { return m_State; }
  static void *PausePythonThreading();
  static void ResumePythonThreading(void *ctx);

  static void GlobalInit(PersistentConfig &config);
  static void setCtxGlobal(ICaptureContext &ctx);
  static void GlobalShutdown();

  static QStringList GetApplicationExtensionsPaths();
  static void ProcessExtensionWork(std::function<void()> callback);
  static QString LoadExtension(ICaptureContext &ctx, const rdcstr &extension);
  static void ConvertPyArgs(const ExtensionCallbackData &data,
                            rdcarray<rdcpair<rdcstr, PyObject *>> &args);
  static void FreePyArgs(rdcarray<rdcpair<rdcstr, PyObject *>> &args);

  static void GenerateStubs(const rdcarray<rdcstr> &extraPaths);

  static void PrepareDebugTracing();

  static bool IsDebuggingEnabled() { return m_DebugPy != NULL; }
  static bool IsDebuggerConnected();

  static void PrepareDebuggerWait();
  static bool WaitForDebugger();
  static void LaunchDebugger(QWidget *window, PersistentConfig &config, QString context_location);

  PyParseError CheckPyParse(const QByteArray &script, const rdcstr &scriptNameForErrors);

  bool CheckInterfaces(rdcstr &log);

  static QString versionString();

  template <typename T>
  void setGlobal(const char *varName, T *object)
  {
    setGlobal(varName, (rdcstr(TypeName<T>()) + " *").c_str(), (void *)object);
  }

  template <typename QtObjectType>
  void setQtGlobal(const char *varName, QtObjectType *object)
  {
    const char *typeName = typeid(*const_cast<QtObjectType *>(object)).name();

    // forward non-template part on
    PyObject *obj = QtObjectToPython(typeName, object);

    if(obj)
      setPyGlobal(varName, obj);
    else
      emit exception(QString(), lit("RuntimeError"),
                     tr("Failed to set variable '%1' of type '%2'")
                         .arg(QString::fromUtf8(varName))
                         .arg(QString::fromUtf8(typeName)),
                     -1, {});
  }

  static PyObject *QWidgetToPy(QWidget *widget) { return QtObjectToPython("QWidget", widget); }
  static QWidget *QWidgetFromPy(PyObject *widget);

  void reflectSource(QString src);
  void makeHelpContext();
  QString tooltipForLoc(int line, int col);
  QList<QPair<QString, QString>> completionOptions(int line, QString expr, int &prefix_len);
  QString tryFunctionCompletion(int line, QString expr);
  QString typenameForLoc(int line, int col);

  QString GetTempFilename(QString filename);

  void FlushOutput() { outputTick(); }

  void abort() { m_Abort = true; }
  bool shouldAbort() { return m_Abort; }
  QString currentFile() { return location.file; }
  int currentLine() { return location.line; }
  static void AddDebuggableThread();
  static void RemoveDebuggableThread();

  // for extension callbacks we want to pass the python wrapper
  static ICaptureContext *GetExtensionPyrenderdoc() { return m_CtxWrapper; }
  static PythonContext *GetExtensionContext() { return m_ExtensionContext; }

signals:
  void traceLine(const QString &file, int line);
  void exception(const QString &extension, const QString &type, const QString &value, int finalLine,
                 QList<QString> frames);
  void textOutput(const QString &extension, bool isStdError, const QString &output);

  void extensionLoaded(const QString &extension);
  void extensionsUpdated();

public slots:
  void executeString(const QString &source);
  void executeString(const QString &filename, const QString &source);

  void executeFile(const QString &filename);
  void setGlobal(const char *varName, const char *typeName, void *object);
  void setPyGlobal(const char *varName, PyObject *object);

private:
  // this is the dict for __main__ after importing our modules, which is copied for each actual
  // python context
  static PyObject *main_dict;

  // this is the debugpy module
  static PyObject *m_DebugPy;
  // these are used for callbacks where we have no python frame (e.g. C code on the UI calling into
  // a registered python callback) so need to quickly set up things for debugging if enabled
  static PyObject *m_CallWrapper;
  static PyObject *m_CallWrapperGlobals;

  // the PyReflector from parse_reflection
  static PyObject *m_Reflector;
  // the stub modules
  static PyObject *m_StubRD;
  static PyObject *m_StubQRD;
  static QAtomicInt m_DeferredInit;

  // the pyrenderdoc wrapper around ICaptureContext
  static PyObject *m_pyrenderdoc;
  static ICaptureContext *m_CtxWrapper;

  // a statically created PythonContext for extension events/output.
  // each extension has its own dictionary but this is used so that users can connect to it and receieve events
  static PythonContext *m_ExtensionContext;

  // the list of extension objects, to be able to reload them
  static QMap<rdcstr, PyObject *> extensions;

  static bool initialised();

  // this is local to this context, containing a dict copied from a pristine __main__ that any
  // globals are set into and any scripts execute in
  PyObject *context_namespace = NULL;

  // a rlcompleter.Completer object used for tab-completion
  PyObject *m_Completer = NULL;

  // this is set during an execute, so we can identify when a callback happens within our execute or
  // not
  PyThreadState *m_State = NULL;

  // this is stored so we can push/pop the GIL state properly
  void *m_SavedThread = NULL;

  struct
  {
    QString file;
    int line = 0;
  } location;

  bool m_Abort = false;

  static PyObject *QtObjectToPython(const char *typeName, QObject *object);

  QTimer *outputTicker = NULL;
  QMutex outputMutex;

  struct OutputPair
  {
    QString outstr, errstr;
  };
  QMap<QString, OutputPair> outputCaches;

  void outputTick();
  void addText(QString extension, bool isStdError, const QString &output);

  // Python callbacks
  static void outstream_del(PyObject *self);
  static PyObject *outstream_write(PyObject *self, PyObject *args);
  static PyObject *outstream_flush(PyObject *self, PyObject *args);
  static PyObject *outstream_trace(PyObject *self, PyObject *args, PyObject *kwargs);
};

template <>
void PythonContext::setGlobal(const char *varName, PyObject *object);

template <>
void PythonContext::setGlobal(const char *varName, QObject *object);

template <>
void PythonContext::setGlobal(const char *varName, QWidget *object);

// helper struct to handle dynamically allocating then calling Finish()

struct PythonContextHandle
{
public:
  PythonContextHandle() { m_ctx = new PythonContext; }
  ~PythonContextHandle() { m_ctx->Finish(); }
  // don't allow copying
  PythonContextHandle(const PythonContextHandle &) = delete;
  PythonContextHandle &operator=(const PythonContextHandle &) = delete;

  PythonContext &ctx() { return *m_ctx; }
private:
  PythonContext *m_ctx;
};
