/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "3rdparty/pythoncapi_compat.h"

#ifdef slots_was_defined
#define slots
#endif

#if PYSIDE2_ENABLED
// PySide Qt integration, must be included before Qt headers
// warning C4522: 'Shiboken::AutoDecRef': multiple assignment operators specified
#pragma warning(disable : 4522)
#include <pyside.h>
#include <shiboken.h>

PyTypeObject **SbkPySide2_QtCoreTypes = NULL;
PyTypeObject **SbkPySide2_QtGuiTypes = NULL;
PyTypeObject **SbkPySide2_QtWidgetsTypes = NULL;
#else

// for non-windows, this message is displayed at CMake time.
#ifdef _MSC_VER
#pragma message( \
    "Building without PySide2 - Qt will not be accessible in python scripting. See https://github.com/baldurk/renderdoc/wiki/PySide2")
#endif

#endif

#ifdef _MSC_VER
// for the LoadLibrary call on 32-bit windows
#include <windows.h>
#endif

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include "Code/QRDUtils.h"
#include "PythonContext.h"
#include "version.h"

// exported by generated files, used to check interface compliance
bool CheckCoreInterface(rdcstr &log);
bool CheckQtInterface(rdcstr &log);

// defined in SWIG-generated renderdoc_python.cpp
extern "C" PyObject *PyInit_renderdoc(void);
extern "C" PyObject *PassObjectToPython(const char *type, void *obj);
extern "C" PyObject *PassNewObjectToPython(const char *type, void *obj);
// this one is in qrenderdoc_python.cpp
extern "C" PyObject *PyInit_qrenderdoc(void);
extern "C" PyObject *WrapBareQWidget(QWidget *);
extern "C" QWidget *UnwrapBareQWidget(PyObject *);

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
  bool block;
};

static PyTypeObject OutputRedirectorType = {PyVarObject_HEAD_INIT(NULL, 0)};

static PyMethodDef OutputRedirector_methods[] = {
    {"write", NULL, METH_VARARGS, "Writes to the output window"},
    {"flush", NULL, METH_NOARGS, "Does nothing - only provided for compatibility"},
    {NULL}};

PyObject *PythonContext::main_dict = NULL;
QMap<rdcstr, PyObject *> PythonContext::extensions;

static PyObject *current_global_handle = NULL;

static QMutex decrefQueueMutex;
static QList<PyObject *> decrefQueue;
extern "C" void ProcessDecRefQueue();

void FetchException(QString &typeStr, QString &valueStr, int &finalLine, QList<QString> &frames)
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
        PyObject *args = Py_BuildValue("(O)", tracebackObj);
        PyObject *formattedTB = PyObject_CallObject(func, args);

        PyTracebackObject *tb = (PyTracebackObject *)tracebackObj;

        while(tb->tb_next)
          tb = tb->tb_next;

        finalLine = tb->tb_lineno;

        if(formattedTB)
        {
          Py_ssize_t size = PyList_Size(formattedTB);
          for(Py_ssize_t i = 0; i < size; i++)
          {
            PyObject *el = PyList_GetItem(formattedTB, i);

            frames << ToQStr(el).trimmed();
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

  PyImport_AppendInittab("renderdoc", &PyInit_renderdoc);
  PyImport_AppendInittab("qrenderdoc", &PyInit_qrenderdoc);

#if PY_VERSION_HEX > 0x030B0000
  PyConfig config;
  PyConfig_InitPythonConfig(&config);
  config.configure_c_stdio = 0;
  config.parse_argv = 0;
#endif

#if defined(STATIC_QRENDERDOC)
  // add the location where our libs will be for statically-linked python installs
  {
    QDir bin = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir();

    QString pylibs = QDir::cleanPath(bin.absoluteFilePath(lit("../share/renderdoc/pylibs")));

    pylibs.toWCharArray(python_home);

#if PY_VERSION_HEX > 0x030B0000
    config.home = python_home;
#else
    Py_SetPythonHome(python_home);
#endif
  }
#endif

#if PY_VERSION_HEX > 0x030B0000
  config.program_name = program_name;

  config.use_environment = 0;

  Py_InitializeFromConfig(&config);
#else
  Py_SetProgramName(program_name);

  Py_IgnoreEnvironmentFlag = 1;

  Py_Initialize();
#endif

#if PY_VERSION_HEX < 0x03090000
  PyEval_InitThreads();
#endif

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

  PyModule_AddObject(main_module, "renderdoc", PyImport_ImportModule("renderdoc"));
  PyModule_AddObject(main_module, "qrenderdoc", PyImport_ImportModule("qrenderdoc"));

  main_dict = PyModule_GetDict(main_module);

  // replace sys.stdout and sys.stderr with our own objects. These have a 'this' pointer of NULL,
  // which then indicates they need to forward to a global object

  // import sys
  PyDict_SetItemString(main_dict, "sys", PyImport_ImportModule("sys"));

  PyObject *rlcompleter = PyImport_ImportModule("rlcompleter");

  if(rlcompleter)
  {
    PyDict_SetItemString(main_dict, "rlcompleter", rlcompleter);
  }
  else
  {
    // ignore a failed import
    PyErr_Clear();
  }

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
    output->block = false;

    redirector = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);
    PyObject_SetAttrString(sysobj, "stderr", redirector);

    output = (OutputRedirector *)redirector;
    output->isStdError = 1;
    output->context = NULL;
    output->block = false;
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

#if RENDERDOC_STABLE_BUILD == 0
  // if we're running in the git checkout and we can find the test scripts, add that location to the
  // path
  {
    QDir bin = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir();

    QString testpath = QDir::cleanPath(bin.absoluteFilePath(lit("../../util/test")));

    if(QDir(testpath).exists(lit("run_tests.py")))
    {
      PyObject *syspath = PyObject_GetAttrString(sysobj, "path");

      PyObject *str = PyUnicode_FromString(testpath.toUtf8().data());

      PyList_Append(syspath, str);

      Py_DecRef(str);
      Py_DecRef(syspath);
    }
  }
#endif

// set up PySide
#if PYSIDE2_ENABLED
  {
// hack for win32 builds, where our pyside2 accidentally depends on Qt5Qml.dll for no good
// reason and we ship a stub to allow the dll to load instead of rebuilding the whole of pyside2
// :S
#if defined(_MSC_VER) && !defined(_M_X64)
    QString Qt5QmlStub = QApplication::applicationDirPath() + lit("/PySide2/Qt5Qml.dll");
    LoadLibraryA(Qt5QmlStub.toUtf8().data());
#endif

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

  PyObject *rlcompleter = PyDict_GetItemString(main_dict, "rlcompleter");

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
    output->block = false;
    Py_DECREF(redirector);
  }

  if(rlcompleter)
  {
    PyObject *Completer = PyObject_GetAttrString(rlcompleter, "Completer");

    if(Completer)
    {
      // create a completer for our context's namespace
      m_Completer = PyObject_CallFunction(Completer, "O", context_namespace);

      if(m_Completer)
      {
        PyDict_SetItemString(context_namespace, "_renderdoc_completer", m_Completer);
      }
      else
      {
        QString typeStr;
        QString valueStr;
        int finalLine = -1;
        QList<QString> frames;
        FetchException(typeStr, valueStr, finalLine, frames);

        // failure is not fatal
        qWarning() << "Couldn't create completion object. " << typeStr << ": " << valueStr;
        PyErr_Clear();
      }
    }

    Py_DecRef(Completer);
  }
  else
  {
    m_Completer = NULL;
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
  PyGILState_STATE gil = PyGILState_Ensure();
  if(m_Completer)
    Py_DecRef(m_Completer);
  PyGILState_Release(gil);

  // do a final tick to gather any remaining output
  outputTick();
}

bool PythonContext::CheckInterfaces(rdcstr &log)
{
  bool errors = false;

  PyGILState_STATE gil = PyGILState_Ensure();
  errors |= CheckCoreInterface(log);
  errors |= CheckQtInterface(log);

  for(rdcstr module_name : {"renderdoc", "qrenderdoc"})
  {
    PyObject *mod = PyImport_ImportModule(module_name.c_str());
    PyObject *dict = PyModule_GetDict(mod);

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while(PyDict_Next(dict, &pos, &key, &value))
    {
      rdcstr name = ToQStr(key);

      if(name.beginsWith("__"))
        continue;

      if(!PyCallable_Check(value))
      {
        log += "Non-callable object found: " + module_name + "." + name +
               ". Expected only classes and functions.\n";
        errors = true;
      }
    }

    Py_DECREF(mod);
  }

  PyGILState_Release(gil);

  log.trim();
  return errors;
}

void PythonContext::Finish()
{
  PyGILState_STATE gil = PyGILState_Ensure();

  // release our external handle to globals. It'll now only be ref'd from inside
  Py_XDECREF(context_namespace);

  PyGILState_Release(gil);
}

void PythonContext::PausePythonThreading()
{
  m_SavedThread = PyEval_SaveThread();
}

void PythonContext::ResumePythonThreading()
{
  PyEval_RestoreThread((PyThreadState *)m_SavedThread);
  m_SavedThread = NULL;
}

void PythonContext::GlobalShutdown()
{
  if(!initialised())
    return;

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

QStringList PythonContext::GetApplicationExtensionsPaths()
{
  QStringList ret;

  for(QString d : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
  {
    QDir dir(d);
    dir.cd(lit("extensions"));

    if(dir.exists())
      ret.append(dir.absolutePath());
  }

  return ret;
}

void PythonContext::ProcessExtensionWork(std::function<void()> callback)
{
  PyGILState_STATE gil = PyGILState_Ensure();

  callback();

  PyGILState_Release(gil);
}

QString PythonContext::LoadExtension(ICaptureContext &ctx, const rdcstr &extension)
{
  QString ret;

  PyObject *sysobj = PyDict_GetItemString(main_dict, "sys");

  PyObject *syspath = PyObject_GetAttrString(sysobj, "path");

  // add extensions directories in
  for(QString p : PythonContext::GetApplicationExtensionsPaths())
  {
    QDir dir(p);

    rdcstr path = dir.absolutePath();

    if(dir.exists())
    {
      PyObject *str = PyUnicode_FromString(path.c_str());
      PyList_Append(syspath, str);
      Py_DecRef(str);
    }
  }

  PyObject *ext = NULL;

  current_global_handle = PyObject_GetAttrString(sysobj, "stdout");

  if(extensions[extension] == NULL)
  {
    qInfo() << "First load of " << QString(extension);
    ext = PyImport_ImportModule(extension.c_str());
  }
  else
  {
    qInfo() << "Reloading " << QString(extension);

    // call unregister() if it exists
    PyObject *unregister_func = PyObject_GetAttrString(extensions[extension], "unregister");

    if(unregister_func)
    {
      PyObject *retval = PyObject_CallFunction(unregister_func, "");

      // discard the return value, regardless of error we don't abort the reload
      Py_XDECREF(retval);
    }

    // if the extension is a package, we need to manually reload any loaded submodules
    PyObject *sysmodules = PyObject_GetAttrString(sysobj, "modules");

    PyObject *keys = PyDict_Keys(sysmodules);

    QString search = extension + lit(".");

    bool reloadSuccess = true;

    if(keys)
    {
      Py_ssize_t len = PyList_Size(keys);
      for(Py_ssize_t i = 0; i < len; i++)
      {
        PyObject *key = PyList_GetItem(keys, i);
        PyObject *value = PyDict_GetItem(sysmodules, key);

        QString keystr = ToQStr(key);

        if(keystr.contains(search))
        {
          qInfo() << "Reloading submodule " << keystr;
          PyObject *mod = PyImport_ReloadModule(value);

          if(mod == NULL)
          {
            qCritical() << "Failed to reload " << keystr;
            ret += tr("Failed to reload submodule '%1'\n").arg(keystr);
            reloadSuccess = false;
            break;
          }

          // we don't need the reference, we just wanted to reload it
          Py_DECREF(mod);

          value = PyDict_GetItem(sysmodules, key);

          if(value != mod)
            qCritical() << "sys.modules[" << keystr << "]"
                        << " after reload doesn't match reloaded object";
        }
      }

      Py_DECREF(keys);
    }

    if(reloadSuccess)
      ext = PyImport_ReloadModule(extensions[extension]);
  }

  // if import succeeded, store this extension module in our map. If import failed, we might have
  // failed a reimport in which case the original module is still there and valid, so don't
  // overwrite the value.
  if(ext)
  {
    extensions[extension] = ext;

    PyModule_AddObject(ext, "_renderdoc_internal", current_global_handle);
  }

  QString typeStr;
  QString valueStr;
  int finalLine = -1;
  QList<QString> frames;

  if(ext)
  {
    // if import succeeded, call register()
    PyObject *register_func = PyObject_GetAttrString(ext, "register");

    if(register_func)
    {
      PyObject *pyctx =
          PassObjectToPython((rdcstr(TypeName<ICaptureContext>()) + " *").c_str(), &ctx);

      PyObject *retval = NULL;
      if(pyctx)
      {
        retval = PyObject_CallFunction(register_func, "sO", MAJOR_MINOR_VERSION_STRING, pyctx);
      }
      else
      {
        qCritical() << "Internal error passing pyrenderdoc to extension register()";
        ret += tr("Internal error passing pyrenderdoc to extension register()\n");
      }

      if(retval == NULL)
      {
        qCritical() << "register() function failed";
        ret += tr("register() function failed\n");
        ext = NULL;
      }

      Py_XDECREF(retval);

      if(ext)
      {
        int pyret = PyModule_AddObject(ext, "pyrenderdoc", pyctx);

        if(pyret != 0)
        {
          qCritical() << "Couldn't set pyrenderdoc global in loaded module";
          ret += tr("Couldn't set pyrenderdoc global in loaded module\n");
          ext = NULL;
        }
      }

      Py_XDECREF(pyctx);
    }
    else
    {
      ext = NULL;
    }
  }
  else
  {
    ext = NULL;
  }

  if(!ext)
  {
    FetchException(typeStr, valueStr, finalLine, frames);

    ret += lit("\n");

    ret = ret.trimmed();

    qCritical("Error importing extension module. %s: %s", typeStr.toUtf8().data(),
              valueStr.toUtf8().data());
    ret += tr("Error importing extension module. %1: %2\n\n").arg(typeStr).arg(valueStr);

    if(!frames.isEmpty())
    {
      qCritical() << "Traceback (most recent call last):";
      ret += tr("Traceback (most recent call last):\n");
      for(const QString &f : frames)
      {
        QStringList lines = f.split(QLatin1Char('\n'));
        for(const QString &line : lines)
        {
          qCritical("  %s", line.toUtf8().data());
          ret += line + lit("\n");
        }
      }
    }
  }

  Py_ssize_t len = PyList_Size(syspath);
  PyList_SetSlice(syspath, len - 1, len, NULL);

  Py_DecRef(syspath);

  current_global_handle = NULL;

  return ret;
}

void PythonContext::ConvertPyArgs(const ExtensionCallbackData &data,
                                  rdcarray<rdcpair<rdcstr, PyObject *>> &args)
{
  PyGILState_STATE gil = PyGILState_Ensure();

  args.resize(data.size());
  for(size_t i = 0; i < data.size(); i++)
  {
    rdcpair<rdcstr, PyObject *> &a = args[i];
    a.first = data[i].first;

    // convert QVariant to python object
    const QVariant &in = data[i].second;
    PyObject *&out = a.second;

    // coverity[mixed_enums]
    QMetaType::Type type = (QMetaType::Type)in.type();
    switch(type)
    {
      case QMetaType::Bool: out = PyBool_FromLong(in.toBool()); break;
      case QMetaType::Short:
      case QMetaType::Long:
      case QMetaType::Int: out = PyLong_FromLong(in.toInt()); break;
      case QMetaType::UShort:
      case QMetaType::ULong:
      case QMetaType::UInt: out = PyLong_FromUnsignedLong(in.toUInt()); break;
      case QMetaType::LongLong: out = PyLong_FromLongLong(in.toLongLong()); break;
      case QMetaType::ULongLong: out = PyLong_FromUnsignedLongLong(in.toULongLong()); break;
      case QMetaType::Float: out = PyFloat_FromDouble(in.toFloat()); break;
      case QMetaType::Double: out = PyFloat_FromDouble(in.toDouble()); break;
      case QMetaType::QString: out = PyUnicode_FromString(in.toString().toUtf8().data()); break;
      default: break;
    }

    if(!out)
    {
      // try various other types
      if(in.userType() == qMetaTypeId<ResourceId>())
        out = PassNewObjectToPython("ResourceId *", new ResourceId(in.value<ResourceId>()));
    }

    if(!out)
    {
      qCritical() << "Couldn't convert" << in << "to python object";
      out = Py_XNewRef(Py_None);
    }
  }

  PyGILState_Release(gil);
}

void PythonContext::FreePyArgs(rdcarray<rdcpair<rdcstr, PyObject *>> &args)
{
  PyGILState_STATE gil = PyGILState_Ensure();

  for(rdcpair<rdcstr, PyObject *> &a : args)
  {
    Py_XDECREF(a.second);
  }

  PyGILState_Release(gil);
}

QString PythonContext::versionString()
{
  return QFormatStr("%1.%2.%3").arg(PY_MAJOR_VERSION).arg(PY_MINOR_VERSION).arg(PY_MICRO_VERSION);
}

void PythonContext::executeString(const QString &filename, const QString &source)
{
  if(!initialised())
  {
    emit exception(lit("SystemError"), tr("Python integration failed to initialise."), -1, {});
    return;
  }

  location.file = filename;
  location.line = 1;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *compiled =
      Py_CompileString(source.toUtf8().data(), filename.toUtf8().data(),
                       source.count(QLatin1Char('\n')) == 0 ? Py_single_input : Py_file_input);

  PyObject *ret = NULL;

  if(compiled)
  {
    PyObject *traceContext = PyDict_New();

    uintptr_t thisint = (uintptr_t)this;
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

    PyEval_SetTrace(NULL, NULL);

    ProcessDecRefQueue();

    Py_XDECREF(thisobj);
    Py_XDECREF(traceContext);
  }

  Py_DecRef(compiled);

  QString typeStr;
  QString valueStr;
  int finalLine = -1;
  QList<QString> frames;
  bool caughtException = (ret == NULL);

  if(caughtException)
    FetchException(typeStr, valueStr, finalLine, frames);

  Py_XDECREF(ret);

  PyGILState_Release(gil);

  if(caughtException)
    emit exception(typeStr, valueStr, finalLine, frames);
}

void PythonContext::executeString(const QString &source)
{
  executeString(lit("<interactive.py>"), source);
}

void PythonContext::executeFile(const QString &filename)
{
  QFile f(filename);

  if(!f.exists())
  {
    emit exception(lit("FileNotFoundError"), tr("No such file or directory: %1").arg(filename), -1,
                   {});
    return;
  }

  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QByteArray py = f.readAll();

    executeString(filename, QString::fromUtf8(py));
  }
  else
  {
    emit exception(lit("IOError"), QFormatStr("%1: %2").arg(f.errorString()).arg(filename), -1, {});
  }
}

void PythonContext::setGlobal(const char *varName, const char *typeName, void *object)
{
  if(!initialised())
  {
    emit exception(lit("SystemError"), tr("Python integration failed to initialise."), -1, {});
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
    emit exception(lit("RuntimeError"),
                   tr("Failed to set variable '%1' of type '%2'")
                       .arg(QString::fromUtf8(varName))
                       .arg(QString::fromUtf8(typeName)),
                   -1, {});
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

  if(Py_IsNone(widget) || widget == NULL)
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

QStringList PythonContext::completionOptions(QString base)
{
  QStringList ret;

  if(!m_Completer)
    return ret;

  QByteArray bytes = base.toUtf8();
  const char *input = (const char *)bytes.data();

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *completeFunction = PyObject_GetAttrString(m_Completer, "complete");

  int idx = 0;
  PyObject *opt = NULL;
  do
  {
    opt = PyObject_CallFunction(completeFunction, "si", input, idx);

    if(opt && !Py_IsNone(opt))
    {
      QString optstr = ToQStr(opt);

      bool add = true;

      // little hack, remove some of the ugly swig template instantiations that we can't avoid.
      if(optstr.contains(lit("renderdoc.rdcarray")) || optstr.contains(lit("renderdoc.rdcstr")) ||
         optstr.contains(lit("renderdoc.bytebuf")))
        add = false;

      if(add)
        ret << optstr;
    }

    idx++;
  } while(opt && !Py_IsNone(opt));

  // extra hack, remove the swig object functions/data but ONLY if we find a sure-fire identifier
  // (thisown) since otherwise we could remove append from a list object
  bool containsSwigInternals = false;
  for(const QString &optstr : ret)
  {
    if(optstr.contains(lit(".thisown")))
    {
      containsSwigInternals = true;
      break;
    }
  }

  if(containsSwigInternals)
  {
    for(int i = 0; i < ret.count();)
    {
      if(ret[i].endsWith(lit(".acquire(")) || ret[i].endsWith(lit(".append(")) ||
         ret[i].endsWith(lit(".disown(")) || ret[i].endsWith(lit(".next(")) ||
         ret[i].endsWith(lit(".own(")) || ret[i].endsWith(lit(".this")) ||
         ret[i].endsWith(lit(".thisown")))
        ret.removeAt(i);
      else
        i++;
    }
  }

  Py_DecRef(completeFunction);

  PyGILState_Release(gil);

  return ret;
}

PyObject *PythonContext::QtObjectToPython(const char *typeName, QObject *object)
{
#if PYSIDE2_ENABLED
  if(!initialised())
    Py_RETURN_NONE;

  if(!SbkPySide2_QtCoreTypes || !SbkPySide2_QtGuiTypes || !SbkPySide2_QtWidgetsTypes)
  {
    QWidget *w = qobject_cast<QWidget *>(object);
    if(w)
      return WrapBareQWidget(w);

    Py_RETURN_NONE;
  }

  if(object == NULL)
  {
    Py_RETURN_NONE;
  }

  PyObject *obj =
      Shiboken::Object::newObject(reinterpret_cast<SbkObjectType *>(Shiboken::SbkType<QObject>()),
                                  object, false, false, typeName);

  return obj;
#else
  QWidget *w = qobject_cast<QWidget *>(object);
  if(w)
    return WrapBareQWidget(w);

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
    emit exception(lit("SystemError"), tr("Python integration failed to initialise."), -1, {});
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
                 tr("Failed to set variable '%1'").arg(QString::fromUtf8(varName)), -1, {});
}

void PythonContext::outstream_del(PyObject *self)
{
  OutputRedirector *redirector = (OutputRedirector *)self;

  if(redirector)
  {
    PythonContext *context = redirector->context;

    // delete the context on the UI thread.
    GUIInvoke::call(context, [context]() { delete context; });
  }
}

PyObject *PythonContext::outstream_write(PyObject *self, PyObject *args)
{
  const char *text = NULL;

  if(!PyArg_ParseTuple(args, "z:write", &text))
    return NULL;

  if(PyErr_Occurred())
    return NULL;

  OutputRedirector *redirector = (OutputRedirector *)self;

  if(redirector)
  {
    PythonContext *context = redirector->context;
    // most likely this is NULL because the sys.stdout override is static and shared amongst
    // contexts. So look up the global variable that stores the context
    if(context == NULL)
    {
      PyFrameObject *frame = PyEval_GetFrame();
      // inc reference count here so all frames can be decref'd whether they come from here or
      // PyFrame_GetBack
      Py_XINCREF(frame);

      while(frame)
      {
        PyObject *globals = PyFrame_GetGlobals(frame);
        if(globals)
        {
          OutputRedirector *global =
              (OutputRedirector *)PyDict_GetItemString(globals, "_renderdoc_internal");
          if(global)
            context = global->context;
        }

        Py_XDECREF(globals);

        // first get the next frame without decrefing the current
        PyFrameObject *back = PyFrame_GetBack(frame);
        // now decref the old frame
        Py_XDECREF(frame);
        // and iterate on the next one, if we got one
        frame = back;

        if(context)
        {
          // release the last trailing ref, which we know is either NULL or a strong reference from
          // PyFrame_GetBack
          Py_XDECREF(frame);
          break;
        }
      }
    }

    if(context)
    {
      context->addText(redirector->isStdError ? true : false, QString::fromUtf8(text));
    }
    else
    {
      // if context is still NULL we're running in the extension context
      rdcstr message = text;

      PyFrameObject *frame = PyEval_GetFrame();

      while(message.back() == '\n' || message.back() == '\r')
        message.pop_back();

      QString filename = lit("unknown");
      int line = 0;

      if(frame)
      {
        PyCodeObject *code = PyFrame_GetCode(frame);
        filename = ToQStr(code->co_filename);
        Py_XDECREF(code);
        line = PyFrame_GetLineNumber(frame);
      }

      if(!message.empty())
        RENDERDOC_LogMessage(redirector->isStdError ? LogType::Error : LogType::Comment, "EXTN",
                             filename, line, message);
    }
  }

  Py_RETURN_NONE;
}

PyObject *PythonContext::outstream_flush(PyObject *self, PyObject *args)
{
  if(PyErr_Occurred())
    return NULL;

  Py_RETURN_NONE;
}

int PythonContext::traceEvent(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{
  PyObject *thisobj = PyDict_GetItemString(obj, "thisobj");

  uint64_t thisuint64 = PyLong_AsUnsignedLongLong(thisobj);
  uintptr_t thisint = (uintptr_t)thisuint64;
  PythonContext *context = (PythonContext *)thisint;

  PyCodeObject *code = PyFrame_GetCode(frame);

  PyObject *compiled = PyDict_GetItemString(obj, "compiled");
  if(compiled == (PyObject *)code && what == PyTrace_LINE)
  {
    context->location.line = PyFrame_GetLineNumber(frame);

    emit context->traceLine(context->location.file, context->location.line);
  }

  Py_XDECREF(code);

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

extern "C" PyObject *GetCurrentGlobalHandle()
{
  PyObject *frame_global_handle = NULL;

  // walk the frames until we find one with _renderdoc_internal. If we call a function in another
  // module the globals may not have the entry, but the root level is expected to.
  {
    PyFrameObject *frame = PyEval_GetFrame();
    // inc reference count here so all frames can be decref'd whether they come from here or
    // PyFrame_GetBack
    Py_XINCREF(frame);

    while(frame)
    {
      PyObject *globals = PyFrame_GetGlobals(frame);
      frame_global_handle = PyDict_GetItemString(globals, "_renderdoc_internal");
      Py_XDECREF(globals);

      // first get the next frame without decrefing the current
      PyFrameObject *back = PyFrame_GetBack(frame);
      // now decref the old frame
      Py_XDECREF(frame);
      // and iterate on the next one, if we got one
      frame = back;

      if(frame_global_handle)
      {
        // release the last trailing ref, which we know is either NULL or a strong reference from
        // PyFrame_GetBack
        Py_XDECREF(frame);
        break;
      }
    }
  }

  if(frame_global_handle)
    return frame_global_handle;

  if(current_global_handle)
    return current_global_handle;

  PyObject *sys = PyImport_ImportModule("sys");
  if(sys)
  {
    PyObject *ret = PyObject_GetAttrString(sys, "stdout");
    Py_XDECREF(sys);
    return ret;
  }

  return NULL;
}

extern "C" void HandleException(PyObject *global_handle)
{
  QString typeStr;
  QString valueStr;
  int finalLine = -1;
  QList<QString> frames;

  FetchException(typeStr, valueStr, finalLine, frames);

  OutputRedirector *redirector = (OutputRedirector *)global_handle;
  if(redirector && redirector->context)
  {
    emit redirector->context->exception(typeStr, valueStr, finalLine, frames);
  }
  else if(redirector && !redirector->context)
  {
    // if still NULL we're running in the extension context
    rdcstr exString;

    if(!frames.isEmpty())
    {
      exString += "Traceback (most recent call last):\n";
      for(const QString &f : frames)
      {
        exString += "  ";
        exString += f;
        exString += "\n";
      }
    }

    exString += typeStr;
    exString += ": ";
    exString += valueStr;
    exString += "\n";

    PyFrameObject *frame = PyEval_GetFrame();

    QString filename = lit("unknown");
    int linenum = 0;

    if(frame)
    {
      PyCodeObject *code = PyFrame_GetCode(frame);
      filename = ToQStr(code->co_filename);
      Py_XDECREF(code);
      linenum = PyFrame_GetLineNumber(frame);
    }

    RENDERDOC_LogMessage(LogType::Error, "EXTN", filename, linenum, exString);
  }
}

extern "C" bool IsThreadBlocking(PyObject *global_handle)
{
  OutputRedirector *redirector = (OutputRedirector *)global_handle;
  if(redirector)
    return redirector->block;
  return false;
}

extern "C" void SetThreadBlocking(PyObject *global_handle, bool block)
{
  OutputRedirector *redirector = (OutputRedirector *)global_handle;
  if(redirector)
    redirector->block = block;
}

extern "C" void QueueDecRef(PyObject *obj)
{
  QMutexLocker lock(&decrefQueueMutex);

  decrefQueue.push_back(obj);
}

extern "C" void ProcessDecRefQueue()
{
  QMutexLocker lock(&decrefQueueMutex);

  if(decrefQueue.isEmpty())
    return;

  for(PyObject *obj : decrefQueue)
    Py_XDECREF(obj);

  decrefQueue.clear();
}

extern "C" QWidget *QWidgetFromPy(PyObject *widget)
{
  return PythonContext::QWidgetFromPy(widget);
}

extern "C" PyObject *QWidgetToPy(QWidget *widget)
{
  return PythonContext::QWidgetToPy(widget);
}
