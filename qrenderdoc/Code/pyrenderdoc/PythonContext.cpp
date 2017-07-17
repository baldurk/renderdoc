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

#ifdef slots
#undef slots
#define slots_was_defined
#endif

// must be included first
#include <Python.h>
#include <frameobject.h>

#ifdef slots_was_defined
#define slots
#endif

#if PYSIDE2_ENABLED
// PySide Qt integration, must be included before Qt headers
// warning C4522: 'Shiboken::AutoDecRef': multiple assignment operators specified
#pragma warning(disable : 4522)
#include <pyside.h>
#include <pyside2_qtwidgets_python.h>
#include <shiboken.h>

PyTypeObject **SbkPySide2_QtCoreTypes = NULL;
PyTypeObject **SbkPySide2_QtGuiTypes = NULL;
PyTypeObject **SbkPySide2_QtWidgetsTypes = NULL;
#endif

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include "Code/QRDUtils.h"
#include "PythonContext.h"
#include "renderdoc_replay.h"

// defined in SWIG-generated renderdoc_python.cpp
extern "C" PyObject *PyInit__renderdoc(void);
extern "C" PyObject *PassObjectToPython(const char *type, void *obj);
// this one is in qrenderdoc_python.cpp
extern "C" PyObject *PyInit__qrenderdoc(void);
extern "C" PyObject *WrapBareQWidget(PyObject *, QWidget *);
extern "C" QWidget *UnwrapBareQWidget(PyObject *);

#ifdef WIN32

// on Win32 the renderdoc.py is compiled in as a windows resource. Extract and return
#include <windows.h>
#include "Resources/resource.h"

QByteArray GetResourceContents(int resource)
{
  HRSRC res = FindResource(NULL, MAKEINTRESOURCE(resource), MAKEINTRESOURCE(TYPE_EMBED));
  HGLOBAL data = LoadResource(NULL, res);

  if(!data)
    return QByteArray();

  DWORD resSize = SizeofResource(NULL, res);
  const char *resData = (const char *)LockResource(data);

  return QByteArray(resData, (int)resSize);
}

#define GetWrapperModule(name) GetResourceContents(name##_py_module)

#else

// Otherwise it's compiled in via include-bin which converts to a .c with extern array
extern unsigned char renderdoc_py[];
extern unsigned int renderdoc_py_len;
extern unsigned char qrenderdoc_py[];
extern unsigned int qrenderdoc_py_len;

#define GetWrapperModule(name) QByteArray((const char *)name##_py, (int)name##_py_len);

#endif

// little utility function to convert a PyObject * that we know is a string to a QString
static inline QString ToQStr(PyObject *value)
{
  if(value)
  {
    PyObject *repr = PyObject_Str(value);
    if(repr == NULL)
      return QString();

    PyObject *decoded = PyUnicode_AsUTF8String(repr);
    if(decoded == NULL)
      return QString();

    QString ret = QString::fromUtf8(PyBytes_AsString(decoded));

    Py_DecRef(decoded);
    Py_DecRef(repr);

    return ret;
  }

  return QString();
}

static wchar_t program_name[] = L"qrenderdoc";
static wchar_t python_home[1024] = {0};

struct OutputRedirector
{
  PyObject_HEAD;
  union
  {
    // we union with a uint64_t to ensure it's always a ulonglong even on 32-bit
    uint64_t dummy;
    PythonContext *context;
  };
  int isStdError;
};

static PyTypeObject OutputRedirectorType = {PyVarObject_HEAD_INIT(NULL, 0)};

static PyMethodDef OutputRedirector_methods[] = {
    {"write", NULL, METH_VARARGS, "Writes to the output window"},
    {"flush", NULL, METH_NOARGS, "Does nothing - only provided for compatibility"},
    {NULL}};

PyObject *PythonContext::main_dict = NULL;

void FetchException(QString &typeStr, QString &valueStr, QList<QString> &frames)
{
  PyObject *exObj = NULL, *valueObj = NULL, *tracebackObj = NULL;

  PyErr_Fetch(&exObj, &valueObj, &tracebackObj);

  PyErr_NormalizeException(&exObj, &valueObj, &tracebackObj);

  if(exObj && PyType_Check(exObj))
  {
    PyTypeObject *type = (PyTypeObject *)exObj;

    typeStr = QString::fromUtf8(type->tp_name);
  }
  else
  {
    typeStr = QString();
  }

  if(valueObj)
    valueStr = ToQStr(valueObj);

  if(tracebackObj)
  {
    PyObject *tracebackModule = PyImport_ImportModule("traceback");

    if(tracebackModule)
    {
      PyObject *func = PyObject_GetAttrString(tracebackModule, "format_tb");

      if(func && PyCallable_Check(func))
      {
        PyObject *args = Py_BuildValue("(N)", tracebackObj);
        PyObject *formattedTB = PyObject_CallObject(func, args);

        if(formattedTB)
        {
          Py_ssize_t size = PyList_Size(formattedTB);
          for(Py_ssize_t i = 0; i < size; i++)
          {
            PyObject *el = PyList_GetItem(formattedTB, i);

            frames << ToQStr(el);
          }

          Py_DecRef(formattedTB);
        }

        Py_DecRef(args);
      }
    }
  }

  Py_DecRef(exObj);
  Py_DecRef(valueObj);
  Py_DecRef(tracebackObj);
}

void PythonContext::GlobalInit()
{
  // must happen on the UI thread
  if(qApp->thread() != QThread::currentThread())
  {
    qFatal("PythonContext::GlobalInit MUST be called from the UI thread");
    return;
  }

  // for the exception signal
  qRegisterMetaType<QList<QString>>("QList<QString>");

  PyImport_AppendInittab("_renderdoc", &PyInit__renderdoc);
  PyImport_AppendInittab("_qrenderdoc", &PyInit__qrenderdoc);

#if defined(STATIC_QRENDERDOC)
  // add the location where our libs will be for statically-linked python installs
  {
    QDir bin = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir();

    QString pylibs = QDir::cleanPath(bin.absoluteFilePath(lit("../share/renderdoc/pylibs")));

    pylibs.toWCharArray(python_home);

    Py_SetPythonHome(python_home);
  }
#endif

  Py_SetProgramName(program_name);

  Py_Initialize();

  PyEval_InitThreads();

  QByteArray renderdoc_py_src = GetWrapperModule(renderdoc);

  if(renderdoc_py_src.isEmpty())
  {
    qCritical() << "renderdoc.py wrapper is corrupt/empty. Check build configuration to ensure "
                   "SWIG compiled properly with python support.";
    return;
  }

  QByteArray qrenderdoc_py_src = GetWrapperModule(qrenderdoc);

  if(qrenderdoc_py_src.isEmpty())
  {
    qCritical() << "qrenderdoc.py wrapper is corrupt/empty. Check build configuration to ensure "
                   "SWIG compiled properly with python support.";
    return;
  }

  PyObject *renderdoc_py_compiled =
      Py_CompileString(renderdoc_py_src.data(), "renderdoc.py", Py_file_input);

  if(!renderdoc_py_compiled)
  {
    qCritical() << "Failed to compile renderdoc.py wrapper, python will not be available";
    return;
  }

  PyObject *qrenderdoc_py_compiled =
      Py_CompileString(qrenderdoc_py_src.data(), "qrenderdoc.py", Py_file_input);

  if(!qrenderdoc_py_compiled)
  {
    Py_DecRef(renderdoc_py_compiled);
    qCritical() << "Failed to compile qrenderdoc.py wrapper, python will not be available";
    return;
  }

  OutputRedirectorType.tp_name = "renderdoc_output_redirector";
  OutputRedirectorType.tp_basicsize = sizeof(OutputRedirector);
  OutputRedirectorType.tp_flags = Py_TPFLAGS_DEFAULT;
  OutputRedirectorType.tp_doc =
      "Output redirector, to be able to catch output to stdout and stderr";
  OutputRedirectorType.tp_new = PyType_GenericNew;
  OutputRedirectorType.tp_dealloc = &PythonContext::outstream_del;
  OutputRedirectorType.tp_methods = OutputRedirector_methods;

  OutputRedirector_methods[0].ml_meth = &PythonContext::outstream_write;
  OutputRedirector_methods[1].ml_meth = &PythonContext::outstream_flush;

  PyObject *main_module = PyImport_AddModule("__main__");

  // for compatibility with earlier versions of python that took a char * instead of const char *
  char renderdoc_name[] = "renderdoc";
  char qrenderdoc_name[] = "qrenderdoc";

  PyObject *rdoc_module = PyImport_ExecCodeModule(renderdoc_name, renderdoc_py_compiled);
  PyObject *qrdoc_module = PyImport_ExecCodeModule(qrenderdoc_name, qrenderdoc_py_compiled);

  Py_XDECREF(renderdoc_py_compiled);
  Py_XDECREF(qrenderdoc_py_compiled);

  PyModule_AddObject(main_module, "renderdoc", rdoc_module);
  PyModule_AddObject(main_module, "qrenderdoc", qrdoc_module);

  main_dict = PyModule_GetDict(main_module);

  // replace sys.stdout and sys.stderr with our own objects. These have a 'this' pointer of NULL,
  // which then indicates they need to forward to a global object

  // import sys
  PyDict_SetItemString(main_dict, "sys", PyImport_ImportModule("sys"));

  // sysobj = sys
  PyObject *sysobj = PyDict_GetItemString(main_dict, "sys");

  // sysobj.stdout = renderdoc_output_redirector()
  // sysobj.stderr = renderdoc_output_redirector()
  if(PyType_Ready(&OutputRedirectorType) >= 0)
  {
    // for compatibility with earlier versions of python that took a char * instead of const char *
    char noparams[1] = "";

    PyObject *redirector = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);
    PyObject_SetAttrString(sysobj, "stdout", redirector);

    OutputRedirector *output = (OutputRedirector *)redirector;
    output->isStdError = 0;
    output->context = NULL;

    redirector = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);
    PyObject_SetAttrString(sysobj, "stderr", redirector);

    output = (OutputRedirector *)redirector;
    output->isStdError = 1;
    output->context = NULL;
  }

// if we need to append to sys.path to locate PySide2, do that now
#if defined(PYSIDE2_SYS_PATH)
  {
    PyObject *syspath = PyObject_GetAttrString(sysobj, "path");

#ifndef STRINGIZE
#define STRINGIZE2(a) #a
#define STRINGIZE(a) STRINGIZE2(a)
#endif

    PyObject *str = PyUnicode_FromString(STRINGIZE(PYSIDE2_SYS_PATH));

    PyList_Append(syspath, str);

    Py_DecRef(str);
    Py_DecRef(syspath);
  }
#endif

// set up PySide
#if PYSIDE2_ENABLED
  {
    Shiboken::AutoDecRef core(Shiboken::Module::import("PySide2.QtCore"));
    if(!core.isNull())
      SbkPySide2_QtCoreTypes = Shiboken::Module::getTypes(core);
    else
      qCritical() << "Failed to load PySide2.QtCore";

    Shiboken::AutoDecRef gui(Shiboken::Module::import("PySide2.QtGui"));
    if(!gui.isNull())
      SbkPySide2_QtGuiTypes = Shiboken::Module::getTypes(gui);
    else
      qCritical() << "Failed to load PySide2.QtGui";

    Shiboken::AutoDecRef widgets(Shiboken::Module::import("PySide2.QtWidgets"));
    if(!widgets.isNull())
      SbkPySide2_QtWidgetsTypes = Shiboken::Module::getTypes(widgets);
    else
      qCritical() << "Failed to load PySide2.QtWidgets";
  }
#endif

  // release GIL so that python work can now happen on any thread
  PyEval_SaveThread();
}

bool PythonContext::initialised()
{
  return main_dict != NULL;
}

PythonContext::PythonContext(QObject *parent) : QObject(parent)
{
  if(!initialised())
    return;

  // acquire the GIL and make sure this thread is init'd
  PyGILState_STATE gil = PyGILState_Ensure();

  // clone our own local context
  context_namespace = PyDict_Copy(main_dict);

  QString typeStr;
  QString valueStr;
  QList<QString> frames;

  // for compatibility with earlier versions of python that took a char * instead of const char *
  char noparams[1] = "";

  // set global output that point to this context. It is responsible for deleting the context when
  // it goes out of scope
  PyObject *redirector = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);
  if(redirector)
  {
    PyDict_SetItemString(context_namespace, "_renderdoc_internal", redirector);

    OutputRedirector *output = (OutputRedirector *)redirector;
    output->context = this;
    Py_DECREF(redirector);
  }

  // release the GIL again
  PyGILState_Release(gil);

  // every 100ms while running, check for new output
  outputTicker = new QTimer(this);
  outputTicker->setInterval(100);
  QObject::connect(outputTicker, &QTimer::timeout, this, &PythonContext::outputTick);

  // we have to start it here, because we can't start on another thread.
  outputTicker->start();
}

PythonContext::~PythonContext()
{
  // do a final tick to gather any remaining output
  outputTick();
}

void PythonContext::Finish()
{
  PyGILState_STATE gil = PyGILState_Ensure();

  // release our external handle to globals. It'll now only be ref'd from inside
  Py_XDECREF(context_namespace);

  PyGILState_Release(gil);
}

void PythonContext::GlobalShutdown()
{
  // must happen on the UI thread
  if(qApp->thread() != QThread::currentThread())
  {
    qFatal("PythonContext::GlobalShutdown MUST be called from the UI thread");
    return;
  }

  // acquire the GIL, so we can shut down
  PyGILState_Ensure();

  Py_Finalize();
}

QString PythonContext::versionString()
{
  return QFormatStr("%1.%2.%3").arg(PY_MAJOR_VERSION).arg(PY_MINOR_VERSION).arg(PY_MICRO_VERSION);
}

void PythonContext::executeString(const QString &filename, const QString &source, bool interactive)
{
  if(!initialised())
  {
    emit exception(
        lit("SystemError"),
        tr("Python integration failed to initialise, see diagnostic log for more information."), {});
    return;
  }

  location.file = filename;
  location.line = 1;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *compiled = Py_CompileString(source.toUtf8().data(), filename.toUtf8().data(),
                                        interactive ? Py_single_input : Py_file_input);

  PyObject *ret = NULL;

  if(compiled)
  {
    PyObject *traceContext = PyDict_New();

    uintptr_t thisint = (uintptr_t) this;
    uint64_t thisuint64 = (uint64_t)thisint;
    PyObject *thisobj = PyLong_FromUnsignedLongLong(thisuint64);

    PyDict_SetItemString(traceContext, "thisobj", thisobj);
    PyDict_SetItemString(traceContext, "compiled", compiled);

    PyEval_SetTrace(&PythonContext::traceEvent, traceContext);

    m_Abort = false;

    m_State = PyGILState_GetThisThreadState();

    ret = PyEval_EvalCode(compiled, context_namespace, context_namespace);

    m_State = NULL;

    // catch any output
    outputTick();

    Py_XDECREF(thisobj);
    Py_XDECREF(traceContext);
  }

  Py_DecRef(compiled);

  QString typeStr;
  QString valueStr;
  QList<QString> frames;
  bool caughtException = (ret == NULL);

  if(caughtException)
    FetchException(typeStr, valueStr, frames);

  Py_XDECREF(ret);

  PyGILState_Release(gil);

  if(caughtException)
    emit exception(typeStr, valueStr, frames);
}

void PythonContext::executeString(const QString &source, bool interactive)
{
  executeString(lit("<interactive.py>"), source, interactive);
}

void PythonContext::executeFile(const QString &filename)
{
  QFile f(filename);

  if(!f.exists())
  {
    emit exception(lit("FileNotFoundError"), tr("No such file or directory: %1").arg(filename), {});
    return;
  }

  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QByteArray py = f.readAll();

    executeString(filename, QString::fromUtf8(py));
  }
  else
  {
    emit exception(lit("IOError"), QFormatStr("%1: %2").arg(f.errorString()).arg(filename), {});
  }
}

void PythonContext::setGlobal(const char *varName, const char *typeName, void *object)
{
  if(!initialised())
  {
    emit exception(
        lit("SystemError"),
        tr("Python integration failed to initialise, see diagnostic log for more information."), {});
    return;
  }

  PyGILState_STATE gil = PyGILState_Ensure();

  // we don't need separate functions for each module, as they share type info
  PyObject *obj = PassObjectToPython(typeName, object);

  int ret = -1;

  if(obj)
    ret = PyDict_SetItemString(context_namespace, varName, obj);

  PyGILState_Release(gil);

  if(ret != 0)
  {
    emit exception(lit("RuntimeError"), tr("Failed to set variable '%1' of type '%2'")
                                            .arg(QString::fromUtf8(varName))
                                            .arg(QString::fromUtf8(typeName)),
                   {});
    return;
  }

  setPyGlobal(varName, obj);
}

template <>
void PythonContext::setGlobal(const char *varName, PyObject *object)
{
  setPyGlobal(varName, object);
}

template <>
void PythonContext::setGlobal(const char *varName, QObject *object)
{
  setQtGlobal(varName, object);
}

template <>
void PythonContext::setGlobal(const char *varName, QWidget *object)
{
  setQtGlobal(varName, object);
}

QWidget *PythonContext::QWidgetFromPy(PyObject *widget)
{
#if PYSIDE2_ENABLED
  if(!initialised())
    return NULL;

  if(!SbkPySide2_QtCoreTypes || !SbkPySide2_QtGuiTypes || !SbkPySide2_QtWidgetsTypes)
    return UnwrapBareQWidget(widget);

  if(!Shiboken::Object::checkType(widget))
    return UnwrapBareQWidget(widget);

  return (QWidget *)Shiboken::Object::cppPointer((SbkObject *)widget, Shiboken::SbkType<QWidget>());
#else
  return UnwrapBareQWidget(widget);
#endif
}

PyObject *PythonContext::QtObjectToPython(PyObject *self, const char *typeName, QObject *object)
{
#if PYSIDE2_ENABLED
  if(!initialised())
    Py_RETURN_NONE;

  if(!SbkPySide2_QtCoreTypes || !SbkPySide2_QtGuiTypes || !SbkPySide2_QtWidgetsTypes)
  {
    QWidget *w = qobject_cast<QWidget *>(object);
    if(self && w)
      return WrapBareQWidget(self, w);

    Py_RETURN_NONE;
  }

  PyObject *obj =
      Shiboken::Object::newObject(reinterpret_cast<SbkObjectType *>(Shiboken::SbkType<QObject>()),
                                  object, false, false, typeName);

  return obj;
#else
  QWidget *w = qobject_cast<QWidget *>(object);
  if(self && w)
    return WrapBareQWidget(self, w);

  Py_RETURN_NONE;
#endif
}

// callback to flush output every so often (not constantly, to avoid spamming signals)
void PythonContext::outputTick()
{
  QMutexLocker lock(&outputMutex);

  if(!outstr.isEmpty())
  {
    emit textOutput(false, outstr);
  }

  if(!errstr.isEmpty())
  {
    emit textOutput(true, errstr);
  }

  outstr.clear();
  errstr.clear();
}

void PythonContext::addText(bool isStdError, const QString &output)
{
  QMutexLocker lock(&outputMutex);

  if(isStdError)
    errstr += output;
  else
    outstr += output;
}

void PythonContext::setPyGlobal(const char *varName, PyObject *obj)
{
  if(!initialised())
  {
    emit exception(
        lit("SystemError"),
        tr("Python integration failed to initialise, see diagnostic log for more information."), {});
    return;
  }

  int ret = -1;

  PyGILState_STATE gil = PyGILState_Ensure();

  if(obj)
    ret = PyDict_SetItemString(context_namespace, varName, obj);

  PyGILState_Release(gil);

  if(ret == 0)
    return;

  emit exception(lit("RuntimeError"),
                 tr("Failed to set variable '%1'").arg(QString::fromUtf8(varName)), {});
}

void PythonContext::outstream_del(PyObject *self)
{
  OutputRedirector *redirector = (OutputRedirector *)self;

  if(redirector)
  {
    PythonContext *context = redirector->context;

    // delete the context on the UI thread.
    GUIInvoke::call([context]() { delete context; });
  }
}

PyObject *PythonContext::outstream_write(PyObject *self, PyObject *args)
{
  const char *text = NULL;

  if(!PyArg_ParseTuple(args, "z:write", &text))
    return NULL;

  OutputRedirector *redirector = (OutputRedirector *)self;

  if(redirector)
  {
    PythonContext *context = redirector->context;
    // most likely this is NULL because the sys.stdout override is static and shared amongst
    // contexts. So look up the global variable that stores the context
    if(context == NULL)
    {
      _frame *frame = PyEval_GetFrame();

      while(frame)
      {
        PyObject *globals = frame->f_globals;
        if(globals)
        {
          OutputRedirector *global =
              (OutputRedirector *)PyDict_GetItemString(globals, "_renderdoc_internal");
          if(global)
            context = global->context;
        }

        if(context)
          break;

        frame = frame->f_back;
      }
    }

    if(context)
    {
      context->addText(redirector->isStdError ? true : false, QString::fromUtf8(text));
    }
  }

  Py_RETURN_NONE;
}

PyObject *PythonContext::outstream_flush(PyObject *self, PyObject *args)
{
  Py_RETURN_NONE;
}

int PythonContext::traceEvent(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{
  PyObject *thisobj = PyDict_GetItemString(obj, "thisobj");

  uint64_t thisuint64 = PyLong_AsUnsignedLongLong(thisobj);
  uintptr_t thisint = (uintptr_t)thisuint64;
  PythonContext *context = (PythonContext *)thisint;

  PyObject *compiled = PyDict_GetItemString(obj, "compiled");
  if(compiled == (PyObject *)frame->f_code && what == PyTrace_LINE)
  {
    context->location.line = PyFrame_GetLineNumber(frame);

    emit context->traceLine(context->location.file, context->location.line);
  }

  if(context->shouldAbort())
  {
    PyErr_SetString(PyExc_SystemExit, "Execution aborted.");
    return -1;
  }

  return 0;
}

extern "C" PyThreadState *GetExecutingThreadState(PyObject *global_handle)
{
  OutputRedirector *redirector = (OutputRedirector *)global_handle;
  if(redirector->context)
    return redirector->context->GetExecutingThreadState();

  return NULL;
}

extern "C" void HandleException(PyObject *global_handle)
{
  QString typeStr;
  QString valueStr;
  QList<QString> frames;

  FetchException(typeStr, valueStr, frames);

  OutputRedirector *redirector = (OutputRedirector *)global_handle;
  if(redirector->context)
    emit redirector->context->exception(typeStr, valueStr, frames);
}
