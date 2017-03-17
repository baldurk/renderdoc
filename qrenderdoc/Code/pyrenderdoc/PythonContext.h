/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include <QObject>
#include <QString>
#include "renderdoc_replay.h"

class QThread;

typedef struct _object PyObject;
typedef struct _frame PyFrameObject;
typedef struct _ts PyThreadState;

struct GILContext;

class PythonContext : public QObject
{
private:
  Q_OBJECT

public:
  explicit PythonContext(QObject *parent = NULL);
  ~PythonContext();

  static void GlobalInit();
  static void GlobalShutdown();

  template <typename T>
  void setGlobal(const char *varName, T *object)
  {
    QByteArray baseTypeName = TypeName<T>();
    baseTypeName += " *";
    setGlobal(varName, baseTypeName.data(), (void *)object);
  }

  QString currentFile() { return location.file; }
  int currentLine() { return location.line; }
signals:
  void traceLine(const QString &file, int line);
  void exception(const QString &type, const QString &value, const QList<QString> &frames);
  void textOutput(bool isStdError, const QString &output);

public slots:
  void executeString(const QString &source);
  void executeString(const QString &filename, const QString &source);
  void executeFile(const QString &filename);
  void setGlobal(const char *varName, const char *typeName, void *object);

private:
  // this remains constant between GlobalInit and GlobalShutdown to avoid recompiling the wrapper
  static PyObject *renderdoc_py_compiled;

  // this is the global thread state. We only use this to create new contexts and finalize at
  // program shutdown
  static PyThreadState *mainThread;

  static bool initialised();

  // this is local to this context, containing a handle to the __main__ module
  PyObject *main_module = NULL;

  // this is also local to the context, it has the thread state on the thread that created the
  // context
  PyThreadState *interpreter;
  // the thread that originally created the interpreter
  QThread *interpreterThread;

  // helpers for starting/stopping python work on a thread on this interpreter
  GILContext *PythonInterpStart();
  void PythonInterpEnd(GILContext *);

  struct
  {
    QString file;
    int line = 0;
  } location;

  // Python callbacks
  static PyObject *outstream_write(PyObject *self, PyObject *args);
  static PyObject *outstream_flush(PyObject *self, PyObject *args);
  static int traceEvent(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg);
};