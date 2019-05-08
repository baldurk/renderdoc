/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

class PythonContext : public QObject
{
private:
  Q_OBJECT

  // don't allow destruction from outside, you must heap-allocate the context and let it delete
  // itself when all references are done. This handles the case where e.g. some Async work is going
  // on and needs to finish executing after the external code is done with the context
  ~PythonContext();

public:
  explicit PythonContext(QObject *parent = NULL);
  void Finish();

  PyThreadState *GetExecutingThreadState() { return m_State; }
  static void GlobalInit();
  static void GlobalShutdown();

  static void ProcessExtensionWork(std::function<void()> callback);
  static bool LoadExtension(ICaptureContext &ctx, const rdcstr &extension);
  static void ConvertPyArgs(const ExtensionCallbackData &data,
                            rdcarray<rdcpair<rdcstr, PyObject *>> &args);
  static void FreePyArgs(rdcarray<rdcpair<rdcstr, PyObject *>> &args);

  bool CheckInterfaces();

  QString versionString();

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
      emit exception(lit("RuntimeError"), tr("Failed to set variable '%1' of type '%2'")
                                              .arg(QString::fromUtf8(varName))
                                              .arg(QString::fromUtf8(typeName)),
                     -1, {});
  }

  static PyObject *QWidgetToPy(QWidget *widget) { return QtObjectToPython("QWidget", widget); }
  static QWidget *QWidgetFromPy(PyObject *widget);

  QStringList completionOptions(QString base);

  void setThreadBlocking(bool block) { m_Block = block; }
  bool threadBlocking() { return m_Block; }
  void abort() { m_Abort = true; }
  bool shouldAbort() { return m_Abort; }
  QString currentFile() { return location.file; }
  int currentLine() { return location.line; }
signals:
  void traceLine(const QString &file, int line);
  void exception(const QString &type, const QString &value, int finalLine, QList<QString> frames);
  void textOutput(bool isStdError, const QString &output);

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

  struct
  {
    QString file;
    int line = 0;
  } location;

  bool m_Abort = false;

  bool m_Block = false;

  static PyObject *QtObjectToPython(const char *typeName, QObject *object);

  QTimer *outputTicker = NULL;
  QMutex outputMutex;
  QString outstr, errstr;

  void outputTick();
  void addText(bool isStdError, const QString &output);

  // Python callbacks
  static void outstream_del(PyObject *self);
  static PyObject *outstream_write(PyObject *self, PyObject *args);
  static PyObject *outstream_flush(PyObject *self, PyObject *args);
  static int traceEvent(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg);
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