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

#ifdef slots
#undef slots
#define slots_was_defined
#endif

// must be included first
#include <Python.h>
#include <pythread.h>

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
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
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

extern "C" PyObject *GetCurrentGlobalHandle();

QSemaphore debuggerWaitSemaphore;

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
  bool selfDeleting;
  int isStdError;
  bool block;
  rdcstr extension;
  PyObject *compiled;
  PyObject *chain_trace;
};

static PyTypeObject OutputRedirectorType = {PyVarObject_HEAD_INIT(NULL, 0)};

static PyMethodDef OutputRedirector_methods[] = {
    {"write", NULL, METH_VARARGS, "Writes to the output window"},
    {"flush", NULL, METH_NOARGS, "Does nothing - only provided for compatibility"},
    {NULL}};

PyObject *PythonContext::main_dict = NULL;
PyObject *PythonContext::m_DebugPy = NULL;
PyObject *PythonContext::m_CallWrapper = NULL;
PyObject *PythonContext::m_Reflector = NULL;
PyObject *PythonContext::m_StubRD = NULL;
PyObject *PythonContext::m_StubQRD = NULL;
PyObject *PythonContext::m_pyrenderdoc = NULL;
ICaptureContext *PythonContext::m_CtxWrapper = NULL;
QAtomicInt PythonContext::m_DeferredInit = 0;
PyObject *PythonContext::m_CallWrapperGlobals = NULL;
PythonContext *PythonContext::m_ExtensionContext = NULL;
QMap<rdcstr, PyObject *> PythonContext::extensions;

// defined in PythonInvokers.cpp
ICaptureContext *MakeCaptureContextInvoker(ICaptureContext &ctx);
void FreeCaptureContextInvoker(ICaptureContext *ctx);

static PyObject *current_global_handle = NULL;

static QMutex decrefQueueMutex;
static QList<PyObject *> decrefQueue;
extern "C" void ProcessDecRefQueue();
extern "C" void HandleException(PyObject *global_handle);

// helper overload to give us the semantics we want - return NULL without an exception if the attr doesn't exist
PyObject *PyObject_SafeGetAttrString(PyObject *obj, const char *string)
{
  if(PyObject_HasAttrString(obj, string) == 0)
    return NULL;

  return PyObject_GetAttrString(obj, string);
}

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
      PyObject *func = PyObject_SafeGetAttrString(tracebackModule, "format_tb");

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
        Py_DecRef(func);
      }
      else
      {
        qCritical() << "Couldn't get format_tb from traceback module";
      }

      Py_DecRef(tracebackModule);
    }
  }

  Py_DecRef(exObj);
  Py_DecRef(valueObj);
  Py_DecRef(tracebackObj);
}

QStringList GetPystubsLocations(QString basePath)
{
  QString versionSubdir =
      QFormatStr("v%1_%2").arg(RENDERDOC_VERSION_MAJOR).arg(RENDERDOC_VERSION_MINOR);
  QString latestSubdir = lit("latest");

  QDir dir(basePath);
  if(dir.mkpath(versionSubdir))
  {
    QDir latestDir = dir;
    latestDir.mkpath(latestSubdir);
    latestDir.cd(latestSubdir);
    QString latestPath = latestDir.absolutePath();

    dir.cd(versionSubdir);
    QString versionedPath = dir.absolutePath();
    return {versionedPath, latestPath};
  }

  return {};
}

struct StubsVersion
{
  int major = 0, minor = 0;
  QString commit;

  static StubsVersion Current()
  {
    StubsVersion ret;
    ret.major = RENDERDOC_VERSION_MAJOR;
    ret.minor = RENDERDOC_VERSION_MINOR;
    if(RENDERDOC_STABLE_BUILD)
      ret.commit = lit("stable");
    else
      ret.commit = QString::fromUtf8(RENDERDOC_GetCommitHash());
    return ret;
  }

  bool ShouldReplace(const StubsVersion &o)
  {
    if(major != o.major)
      return major > o.major;
    if(minor != o.minor)
      return minor > o.minor;

    // if we're identical major/minor, don't regenerate if it's the same commit
    if(commit == o.commit)
      return false;

    // never replace a stable version with non-stable
    if(o.commit == lit("stable"))
      return false;

    // otherwise we don't know when this was m ade, regenerate
    return true;
  }
};

QDebug operator<<(QDebug debug, const StubsVersion &ver)
{
  debug << QFormatStr("%1.%2 (%3)").arg(ver.major).arg(ver.minor).arg(ver.commit.mid(0, 6));
  return debug;
}

StubsVersion GetStubsVersion(QString basePath)
{
  StubsVersion ret;
  QFile file(QDir(basePath).absoluteFilePath(lit("version.txt")));
  if(file.open(QFile::ReadOnly))
  {
    QByteArray version = file.readAll();
    QTextStream ts(version);

    ts >> ret.major >> ret.minor >> ret.commit;

    file.close();
  }

  return ret;
}

void SetStubsVersion(QString basePath, const StubsVersion &ver)
{
  QFile file(QDir(basePath).absoluteFilePath(lit("version.txt")));
  if(file.open(QFile::WriteOnly | QFile::Truncate))
  {
    QString version;
    QTextStream ts(&version);

    ts << ver.major << "\n" << ver.minor << "\n" << ver.commit;

    file.write(version.toUtf8());

    file.close();
  }
}
void PythonContext::GenerateStubs(const rdcarray<rdcstr> &extraPaths)
{
  QElapsedTimer timer;
  timer.start();

  StubsVersion cur = StubsVersion::Current();

  QStringList paths;

  // take the first standard location that works, these should be in priority order and typically
  // the first one is writeable as normal.
  for(QString path : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
  {
    paths << GetPystubsLocations(path + lit("/pystubs"));
    if(!paths.empty())
      break;
  }

  for(rdcstr path : extraPaths)
    paths << GetPystubsLocations(path);

  PyObject *stubgen_module = NULL;
  PyObject *gen = NULL;

  for(QString target : paths)
  {
    StubsVersion ver = GetStubsVersion(target);

    if(!cur.ShouldReplace(ver))
    {
      qInfo() << "Not modifying stubs of version " << ver << "in" << target;
      continue;
    }

    SetStubsVersion(target, cur);

    if(gen == NULL)
    {
      QByteArray stubgen;
      {
        QFile file(lit(":/py/stubgen.py"));

        file.open(QFile::ReadOnly);
        stubgen = file.readAll();
        file.close();
      }

      PyObject *stubgen_compiled = Py_CompileString(stubgen.data(), "stubgen.py", Py_file_input);
      stubgen_module = PyImport_ExecCodeModule("__rd_stubgen", stubgen_compiled);
      Py_XDECREF(stubgen_compiled);

      gen = PyObject_GetAttrString(stubgen_module, "gen");
    }

    PyObject *args =
        Py_BuildValue("(Os)", PyDict_GetItemString(main_dict, "renderdoc"), target.toUtf8().data());
    PyObject *retval = PyObject_CallObject(gen, args);

    if(!retval)
      qCritical() << "Didn't generate renderdoc stubs";

    Py_XDECREF(retval);
    Py_XDECREF(args);

    args =
        Py_BuildValue("(Os)", PyDict_GetItemString(main_dict, "qrenderdoc"), target.toUtf8().data());
    retval = PyObject_CallObject(gen, args);

    if(!retval)
      qCritical() << "Didn't generate qrenderdoc stubs";

    Py_XDECREF(retval);
    Py_XDECREF(args);

    qInfo() << "Generated stubs for " << cur << "into" << target;
  }

  Py_XDECREF(gen);
  Py_XDECREF(stubgen_module);

  qInfo() << "Stubs generation processed in" << timer.elapsed() << "ms";
}

void PythonContext::PrepareDebugTracing()
{
  // prep the frame for debugpy if we have it enabled, as if we're calling straight from C++ debugpy
  // won't be enabled and breakpoints won't get hit
  if(m_DebugPy && m_CallWrapper && m_CallWrapperGlobals)
  {
    Py_XDECREF(PyEval_EvalCode(m_CallWrapper, m_CallWrapperGlobals, NULL));
  }
}

void PythonContext::GlobalInit(PersistentConfig &config)
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
  PyConfig pyconfig;
  PyConfig_InitPythonConfig(&pyconfig);
  pyconfig.configure_c_stdio = 0;
  pyconfig.parse_argv = 0;
#endif

#if defined(STATIC_QRENDERDOC)
  // add the location where our libs will be for statically-linked python installs
  {
    QDir bin = QFileInfo(QCoreApplication::applicationFilePath()).absoluteDir();

    QString pylibs = QDir::cleanPath(bin.absoluteFilePath(lit("../share/renderdoc/pylibs")));

    pylibs.toWCharArray(python_home);

#if PY_VERSION_HEX > 0x030B0000
    pyconfig.home = python_home;
#else
    Py_SetPythonHome(python_home);
#endif
  }
#endif

  // pydev complains about the zip of standard library files, but that's fine
  // and the recommendation is to ignore this warning: https://github.com/microsoft/debugpy/issues/890
  //
  // we need to set this early so it's snapshotted by python for os.getenv/os.environ
  qputenv("PYDEVD_DISABLE_FILE_VALIDATION", lit("1").toUtf8());

#if PY_VERSION_HEX > 0x030B0000
  pyconfig.program_name = program_name;

  pyconfig.use_environment = 0;

  Py_InitializeFromConfig(&pyconfig);
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
  OutputRedirectorType.tp_call = &PythonContext::outstream_trace;

  OutputRedirector_methods[0].ml_meth = &PythonContext::outstream_write;
  OutputRedirector_methods[1].ml_meth = &PythonContext::outstream_flush;

  PyObject *main_module = PyImport_AddModule("__main__");

  PyModule_AddObject(main_module, "renderdoc", PyImport_ImportModule("renderdoc"));
  PyModule_AddObject(main_module, "qrenderdoc", PyImport_ImportModule("qrenderdoc"));

  main_dict = PyModule_GetDict(main_module);

  GenerateStubs(config.Python_StubDirs);

  // replace sys.stdout and sys.stderr with our own objects. These have a 'this' pointer of NULL,
  // which then indicates they need to forward to a global object

  // import sys
  PyObject *sysobj = PyImport_ImportModule("sys");
  PyDict_SetItemString(main_dict, "sys", sysobj);

  // try to import threading library to make debuggers happier, leak it deliberately
  if(!PyImport_ImportModule("threading"))
  {
    // ignore a failed import
    PyErr_Clear();
  }

  // sysobj.stdout = renderdoc_output_redirector()
  // sysobj.stderr = renderdoc_output_redirector()
  if(PyType_Ready(&OutputRedirectorType) >= 0)
  {
    // for compatibility with earlier versions of python that took a char * instead of const char *
    char noparams[1] = "";

    PyObject *redirector = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);
    PyObject_SetAttrString(sysobj, "stdout", redirector);
    PyObject_SetAttrString(sysobj, "_renderdoc_internal", redirector);

    OutputRedirector *output = (OutputRedirector *)redirector;
    output->isStdError = 0;
    output->selfDeleting = false;
    output->context = NULL;
    output->block = false;

    redirector = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);
    PyObject_SetAttrString(sysobj, "stderr", redirector);

    output = (OutputRedirector *)redirector;
    output->isStdError = 1;
    output->selfDeleting = false;
    output->context = NULL;
    output->block = false;
  }

// if we need to append to sys.path to locate PySide2, do that now
#if defined(PYSIDE2_SYS_PATH)
  {
    PyObject *syspath = PyObject_SafeGetAttrString(sysobj, "path");

    if(!syspath)
      qCritical() << "couldn't get sys.path";

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
      PyObject *syspath = PyObject_SafeGetAttrString(sysobj, "path");

      if(!syspath)
        qCritical() << "couldn't get sys.path";

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

  // load debugpy from installed vscode if we find it. This requires a minimum of python 3.8
  std::function<void()> initDebugPy;
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 8
  if(config.Python_DebugEnabled)
  {
    QString debugpy_path;

    // use the user's specified debugpy path if it exists
    if(QDir(config.Python_DebugPyDir).exists() &&
       QDir(config.Python_DebugPyDir).exists(lit("__init__.py")))
    {
      debugpy_path = config.Python_DebugPyDir;
    }

    // next load debugpy from VS Code, if it exists
    if(debugpy_path.isEmpty())
    {
      QString homePath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation)[0];
      QDir homeDir(homePath);
      homeDir.cd(lit(".vscode/extensions"));

      if(homeDir.exists())
      {
        QStringList debugpy_exts = homeDir.entryList(QStringList() << lit("ms-python.debugpy*"));

        if(debugpy_exts.size() >= 1)
        {
          // assume that sorting works by version, so if there are multiple versions of the
          // extension we will pick the latest
          debugpy_exts.sort();

          QDir debugpy_libs = homeDir;
          debugpy_libs.cd(debugpy_exts.last());
          debugpy_libs.cd(lit("bundled/libs"));

          if(debugpy_libs.exists())
          {
            debugpy_path = debugpy_libs.absolutePath();
          }
        }
      }
    }

    // next try pycharm, if we can find it
    if(debugpy_path.isEmpty())
    {
#ifdef Q_OS_WIN32
      QDir programBase(lit("C:/Program Files/JetBrains"));
#else
      QDir programBase(lit("/opt"));
#endif

      QString pycharm;

      QStringList pycharms =
          programBase.entryList(QStringList() << lit("PyCharm*") << lit("pycharm-*"));

      if(!pycharms.empty())
      {
        // assume sorting will pick the latest
        pycharms.sort();

        pycharm = programBase.absoluteFilePath(pycharms.last());
      }

#ifdef Q_OS_WIN32
      if(pycharm.isEmpty())
      {
        // if we didn't find it in the default location, check in the registry on windows
        QSettings pycharm_reg(lit("HKEY_LOCAL_MACHINE\\SOFTWARE\\JetBrains\\PyCharm"),
                              QSettings::NativeFormat);

        QStringList groups = pycharm_reg.childGroups();

        if(!groups.empty())
        {
          // assume sorting will pick the latest
          groups.sort();

          pycharm = pycharm_reg.value(groups.last() + lit("/Default")).toString();
        }
      }
#endif

      if(!pycharm.isEmpty())
      {
        QDir helpers(pycharm);
        helpers.cd(lit("plugins/python-ce/helpers"));

        if(helpers.exists() && helpers.exists(lit("debugpy")))
        {
          debugpy_path = helpers.absolutePath();
        }
      }
    }

    // if we got a path to debugpy, try to load it
    if(!debugpy_path.isEmpty())
    {
      QObject *invokeObj = new QObject();
      initDebugPy = [sysobj, debugpy_path, invokeObj]() {
        qInfo() << "Importing debugpy from" << debugpy_path;

        PyObject *syspath = PyObject_SafeGetAttrString(sysobj, "path");

        {
          PyObject *str = PyUnicode_FromString(debugpy_path.toUtf8().data());
          PyList_Append(syspath, str);
          Py_DecRef(str);
        }

        PyObject *debugpy = PyImport_ImportModule("debugpy");

        if(!debugpy)
        {
          qCritical() << "Failed to import debugpy";
          HandleException(NULL);
        }
        else
        {
          PyObject *configure = PyObject_SafeGetAttrString(debugpy, "configure");

          // don't let debugpy create a subprocess, for obvious reasons
          if(configure)
          {
            PyObject *props = PyDict_New();
            PyDict_SetItemString(props, "subProcess", Py_False);
            PyObject *ret = PyObject_CallFunction(configure, "O", props);

            if(!ret)
            {
              qCritical() << "Failed calling debugpy.configure";
              HandleException(NULL);
              Py_XDECREF(debugpy);
              debugpy = NULL;
            }

            Py_XDECREF(ret);
            Py_XDECREF(props);
          }
          else
          {
            qCritical() << "Couldn't find debugpy.configure";
          }

          Py_XDECREF(configure);

          if(debugpy)
          {
            PyObject *listen = PyObject_SafeGetAttrString(debugpy, "listen");

            if(listen)
            {
              // listen on default port
              PyObject *args = PyTuple_Pack(1, PyLong_FromLong(5678));
              PyObject *kwargs = PyDict_New();
              PyDict_SetItemString(kwargs, "in_process_debug_adapter", Py_True);

              PyObject *ret = PyObject_Call(listen, args, kwargs);

              if(!ret)
              {
                qCritical() << "Failed calling debugpy.listen";
                HandleException(NULL);
                Py_XDECREF(debugpy);
                debugpy = NULL;
              }
              else
              {
                m_DebugPy = debugpy;
              }

              Py_XDECREF(ret);

              Py_XDECREF(args);
              Py_XDECREF(kwargs);
            }
            else
            {
              qCritical() << "Couldn't find debugpy.listen";
            }

            Py_XDECREF(listen);
          }
        }

        // remove the search path from sys.path now
        Py_XDECREF(PyObject_CallMethod(syspath, "pop", NULL));
        Py_DecRef(syspath);

        RemoveDebuggableThread();

        if(m_DebugPy)
        {
          GUIInvoke::defer(invokeObj, [invokeObj]() {
            PyGILState_STATE gil = PyGILState_Ensure();

            AddDebuggableThread();

            m_CallWrapper = Py_CompileString("debugpy.trace_this_thread(True)", "__callwrapper.py",
                                             Py_eval_input);
            m_CallWrapperGlobals = PyDict_Copy(main_dict);
            PyDict_SetItemString(m_CallWrapperGlobals, "debugpy", m_DebugPy);

            // don't pollute the globals
            PyObject *tmpGlobals = PyDict_Copy(main_dict);

            // monkey patch to get around a bug in debugpy/pydevd: https://github.com/microsoft/debugpy/issues/2011
            PyObject *ret = PyRun_String(R"(
try:
    monkey_class = sys.modules['_pydevd_bundle'].pydevd_process_net_command_json.PyDevJsonCommandProcessor
    orig = monkey_class.on_continue_request

    def on_continue_request_hack(self, py_db, request):
        request.arguments.threadId = '*'
        orig(self, py_db, request)

    monkey_class.on_continue_request = on_continue_request_hack

    # while we're here, disable termination. No clean way to do this
    # otherwise
    def _request_terminate_process_hack(self, py_db):
        self.api.request_resume_thread('*')

    monkey_class._request_terminate_process = _request_terminate_process_hack

    print("Monkey-patched debugpy successfully")
except:
    print("Failed to monkey-patch debugpy")
    pass
)",
                                         Py_file_input, tmpGlobals, NULL);

            Py_XDECREF(ret);
            Py_XDECREF(tmpGlobals);

            PyGILState_Release(gil);

            invokeObj->deleteLater();
          });
        }
      };
    }
  }
#endif

  LambdaThread *thread = new LambdaThread([initDebugPy, sysobj]() {
    PyGILState_STATE gil = PyGILState_Ensure();

    if(initDebugPy)
      initDebugPy();

    {
      QByteArray parse_reflection;
      {
        QFile file(lit(":/py/parse_reflection.py"));

        file.open(QFile::ReadOnly);
        parse_reflection = file.readAll();
        file.close();
      }

      QString filename = lit("parse_reflection.py");
      // in debug, to allow attaching with local builds, set the expected/typical real path to the py file
#if !defined(RELEASE)
      {
        QDir dir(QApplication::applicationDirPath());
        dir.cdUp();
        dir.cdUp();
        dir.cd(lit("qrenderdoc"));
        dir.cd(lit("Code"));
        dir.cd(lit("pyrenderdoc"));
        filename = dir.absoluteFilePath(lit("parse_reflection.py"));
      }
#endif

      PyObject *parse_compiled =
          Py_CompileString(parse_reflection.data(), filename.toUtf8().data(), Py_file_input);
      PyObject *parse_module = PyImport_ExecCodeModule("__rd_refl", parse_compiled);
      Py_XDECREF(parse_compiled);

      m_Reflector = PyObject_GetAttrString(parse_module, "PyReflector");
      Py_XDECREF(parse_module);
    }

    if(m_Reflector)
    {
      bool found = false;
      // take the first standard location that works, these should be in priority order
      // and typically the first one is writeable as normal.
      for(QString path : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
      {
        QStringList paths = GetPystubsLocations(path + lit("/pystubs"));
        if(paths.empty())
          continue;

        found = true;

        QDir versioned_stubpath(paths[0]);
        // move up to the parent path to ensure we can import without conflicting with the real module
        versioned_stubpath.cdUp();

        PyObject *syspath = PyObject_SafeGetAttrString(sysobj, "path");

        PyObject *str = PyUnicode_FromString(versioned_stubpath.absolutePath().toUtf8().data());
        PyList_Append(syspath, str);
        Py_DecRef(str);

        Py_XDECREF(syspath);

        m_StubRD = PyImport_ImportModule(QFormatStr("v%1_%2.renderdoc")
                                             .arg(RENDERDOC_VERSION_MAJOR)
                                             .arg(RENDERDOC_VERSION_MINOR)
                                             .toUtf8()
                                             .data());

        if(!m_StubRD)
        {
          qCritical() << "Failed importing stubs for renderdoc";
          HandleException(NULL);
        }

        m_StubQRD = PyImport_ImportModule(QFormatStr("v%1_%2.qrenderdoc")
                                              .arg(RENDERDOC_VERSION_MAJOR)
                                              .arg(RENDERDOC_VERSION_MINOR)
                                              .toUtf8()
                                              .data());

        if(!m_StubQRD)
        {
          qCritical() << "Failed importing stubs for qrenderdoc";
          HandleException(NULL);
        }

        PyObject *alias_modules = PyObject_SafeGetAttrString(m_Reflector, "alias_modules");

        if(m_StubRD)
          PyDict_SetItemString(alias_modules, "renderdoc", m_StubRD);

        if(m_StubQRD)
          PyDict_SetItemString(alias_modules, "qrenderdoc", m_StubQRD);

        Py_XDECREF(alias_modules);

        break;
      }

      if(!found)
        qCritical() << "Couldn't find valid stubs path";
    }

    Py_XDECREF(sysobj);

    m_DeferredInit = 1;

    PyGILState_Release(gil);
  });

  thread->setName(lit("Python deferred initialisation"));
  thread->selfDelete(true);
  thread->start();

  // this will leak effectively
  m_ExtensionContext = new PythonContext(true, NULL);
}

void PythonContext::setCtxGlobal(ICaptureContext &ctx)
{
  m_CtxWrapper = MakeCaptureContextInvoker(ctx);

  PyGILState_STATE gil = PyGILState_Ensure();

  m_pyrenderdoc =
      PassObjectToPython((rdcstr(TypeName<ICaptureContext>()) + " *").c_str(), m_CtxWrapper);

  PyGILState_Release(gil);
}

bool PythonContext::initialised()
{
  return main_dict != NULL;
}

PythonContext::PythonContext(bool extensionContext, QObject *parent) : QObject(parent)
{
  if(!initialised())
    return;

  // acquire the GIL and make sure this thread is init'd
  PyGILState_STATE gil = PyGILState_Ensure();

  // clone our own local context
  context_namespace = PyDict_Copy(main_dict);

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
    output->selfDeleting = !extensionContext;
    output->block = false;
    Py_XDECREF(redirector);
  }

  if(m_pyrenderdoc)
  {
    PyDict_SetItemString(context_namespace, "pyrenderdoc", m_pyrenderdoc);
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

    Py_XDECREF(mod);
  }

  PyGILState_Release(gil);

  log.trim();
  return errors;
}

void PythonContext::Finish()
{
  PyGILState_STATE gil = PyGILState_Ensure();

  if(m_Completer)
  {
    Py_DecRef(m_Completer);
    m_Completer = NULL;
  }

  // release our external handle to globals. It'll now only be ref'd from inside
  Py_XDECREF(context_namespace);

  // force a GC to detect the cycle left
  PyObject *gc = PyImport_ImportModule("gc");
  if(gc)
  {
    Py_XDECREF(PyObject_CallMethod(gc, "collect", NULL));
    Py_XDECREF(gc);
  }

  PyGILState_Release(gil);
}

void *PythonContext::PausePythonThreading()
{
  return PyGILState_Check() == 0 ? NULL : PyEval_SaveThread();
}

void PythonContext::ResumePythonThreading(void *ctx)
{
  if(ctx)
    PyEval_RestoreThread((PyThreadState *)ctx);
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

  FreeCaptureContextInvoker(m_CtxWrapper);
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

  PrepareDebugTracing();

  callback();

  PyGILState_Release(gil);
}

QString PythonContext::LoadExtension(ICaptureContext &ctx, const rdcstr &extension)
{
  QString ret;

  PyObject *sysobj = PyDict_GetItemString(main_dict, "sys");

  PyObject *syspath = PyObject_SafeGetAttrString(sysobj, "path");

  if(!syspath)
    qCritical() << "couldn't get sys.path";

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

  QString typeStr;
  QString valueStr;
  int finalLine = -1;
  QList<QString> frames;

  bool reload = false;

  if(extensions[extension] == NULL)
  {
    qInfo() << "First load of " << QString(extension);
    ext = PyImport_ImportModule(extension.c_str());
  }
  else
  {
    qInfo() << "Reloading " << QString(extension);

    reload = true;

    // call unregister() if it exists
    PyObject *unregister_func = PyObject_SafeGetAttrString(extensions[extension], "unregister");

    bool reloadSuccess = true;

    if(unregister_func)
    {
      PyObject *retval = PyObject_CallFunction(unregister_func, "");

      if(retval == NULL)
      {
        FetchException(typeStr, valueStr, finalLine, frames);
        reloadSuccess = false;
      }

      // discard the return value, regardless of error we don't abort the reload
      Py_XDECREF(retval);
      Py_XDECREF(unregister_func);
    }

    if(reloadSuccess)
    {
      // if the extension is a package, we need to manually reload any loaded submodules
      PyObject *sysmodules = PyObject_SafeGetAttrString(sysobj, "modules");

      if(!syspath)
        qCritical() << "couldn't get sys.modules";

      PyObject *keys = PyDict_Keys(sysmodules);

      QString search = extension + lit(".");

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
            Py_XDECREF(mod);

            value = PyDict_GetItem(sysmodules, key);

            if(value != mod)
              qCritical() << "sys.modules[" << keystr << "]"
                          << " after reload doesn't match reloaded object";
          }
        }

        Py_XDECREF(keys);
      }

      Py_XDECREF(sysmodules);
    }

    if(reloadSuccess)
      ext = PyImport_ReloadModule(extensions[extension]);
  }

  if(!m_pyrenderdoc)
    qCritical() << "pyrenderdoc variable is NULL";

  // if import succeeded, store this extension module in our map. If import failed, we might have
  // failed a reimport in which case the original module is still there and valid, so don't
  // overwrite the value.
  if(ext)
  {
    extensions[extension] = ext;

    // for compatibility with earlier versions of python that took a char * instead of const char *
    char noparams[1] = "";

    PyObject *ext_context = PyObject_CallFunction((PyObject *)&OutputRedirectorType, noparams);

    OutputRedirector *redir = (OutputRedirector *)ext_context;
    redir->isStdError = 0;
    redir->selfDeleting = false;
    redir->context = NULL;
    redir->block = false;
    redir->extension = extension;

    PyModule_AddObject(ext, "_renderdoc_internal", ext_context);

    Py_XINCREF(m_pyrenderdoc);

    int pyret = PyModule_AddObject(ext, "pyrenderdoc", m_pyrenderdoc);

    if(pyret != 0)
    {
      Py_XDECREF(m_pyrenderdoc);

      qCritical() << "Couldn't set pyrenderdoc global in loaded module";
      ret += tr("Couldn't set pyrenderdoc global in loaded module\n");
      ext = NULL;
    }
  }

  if(ext)
  {
    PrepareDebugTracing();

    // if import succeeded, call register()
    PyObject *register_func = PyObject_SafeGetAttrString(ext, "register");

    if(register_func)
    {
      PyObject *retval = NULL;
      if(m_pyrenderdoc)
      {
        retval =
            PyObject_CallFunction(register_func, "sO", MAJOR_MINOR_VERSION_STRING, m_pyrenderdoc);
      }
      else
      {
        qCritical() << "Internal error passing pyrenderdoc to extension register()";
        ret += tr("Internal error passing pyrenderdoc to extension register()\n");
      }

      Py_XDECREF(register_func);

      if(retval == NULL)
      {
        qCritical() << "register() function failed";
        ret += tr("register() function failed\n");
        ext = NULL;
      }

      Py_XDECREF(retval);
    }
    else
    {
      qCritical() << "register() function not found in extension";
      ret += tr("register() function not found in extension\n");
      ext = NULL;
    }
  }
  else
  {
    ext = NULL;
  }

  Py_XDECREF(m_pyrenderdoc);

  if(ext)
  {
    emit m_ExtensionContext->extensionLoaded(extension);
  }
  else
  {
    if(typeStr.isEmpty())
      FetchException(typeStr, valueStr, finalLine, frames);

    ret = ret.trimmed();

    ret += lit("\n");

    if(!valueStr.isEmpty())
    {
      QString errorStr;

      errorStr +=
          tr("Error importing extension module '%1'. %2: %3\n\n").arg(extension).arg(typeStr).arg(valueStr);

      if(!frames.isEmpty())
      {
        errorStr += tr("Traceback (most recent call last):\n");
        for(const QString &f : frames)
        {
          QStringList lines = f.split(QLatin1Char('\n'));
          for(const QString &line : lines)
          {
            errorStr += lit("  %1\n").arg(line);
          }
        }
      }

      qCritical("%s", errorStr.toUtf8().data());
      ret += errorStr;
    }

    if(!ret.isEmpty())
      m_ExtensionContext->addText(extension, true, ret);
  }

  Py_ssize_t len = PyList_Size(syspath);
  PyList_SetSlice(syspath, len - 1, len, NULL);

  Py_DecRef(syspath);

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

QString PythonContext::GetTempFilename(QString filename)
{
  for(QString path : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
  {
    QDir tmpDir(path);
    tmpDir.mkpath(lit("pytmp"));
    tmpDir.cd(lit("pytmp"));
    if(tmpDir.exists())
      return tmpDir.absoluteFilePath(filename);
  }

  return QString();
}

void PythonContext::executeString(const QString &filename, const QString &source)
{
  if(!initialised())
  {
    FlushOutput();
    emit exception(QString(), lit("SystemError"), tr("Python integration failed to initialise."),
                   -1, {});
    return;
  }

  QString tempFilename = filename;

  if(filename.isEmpty())
  {
    tempFilename = lit("<interactive.py>");
  }

  location.file = tempFilename;
  location.line = 1;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *compiled =
      Py_CompileString(source.toUtf8().data(), tempFilename.toUtf8().data(),
                       source.count(QLatin1Char('\n')) == 0 ? Py_single_input : Py_file_input);

  bool debugAttached = false;

  bool caughtException = false;
  QString typeStr;
  QString valueStr;
  int finalLine = -1;
  QList<QString> frames;

  if(compiled)
  {
    PrepareDebugTracing();

    PyObject *sys = PyImport_ImportModule("sys");
    PyObject *settrace = NULL;
    PyObject *gettrace = NULL;
    PyObject *tracer = NULL;
    if(sys)
    {
      settrace = PyObject_SafeGetAttrString(sys, "settrace");
      gettrace = PyObject_SafeGetAttrString(sys, "gettrace");
      tracer = PyDict_GetItemString(context_namespace, "_renderdoc_internal");
    }

    if(settrace && gettrace && tracer)
    {
      OutputRedirector *redir = (OutputRedirector *)tracer;
      redir->compiled = compiled;
      redir->chain_trace = PyObject_CallNoArgs(gettrace);

      Py_XDECREF(PyObject_CallFunction(settrace, "O", tracer));
    }

    m_Abort = false;

    m_State = PyGILState_GetThisThreadState();

    PyObject *ret = PyEval_EvalCode(compiled, context_namespace, context_namespace);

    caughtException = (ret == NULL);

    if(caughtException)
      FetchException(typeStr, valueStr, finalLine, frames);

    Py_XDECREF(ret);

    m_State = NULL;

    // catch any output
    outputTick();

    if(settrace && gettrace && tracer)
    {
      OutputRedirector *redir = (OutputRedirector *)tracer;
      redir->compiled = NULL;

      Py_XDECREF(PyObject_CallFunction(settrace, "O", redir->chain_trace));

      redir->chain_trace = NULL;
    }

    ProcessDecRefQueue();

    Py_XDECREF(sys);
    Py_XDECREF(settrace);
    Py_XDECREF(gettrace);
  }
  else
  {
    caughtException = true;
    FetchException(typeStr, valueStr, finalLine, frames);
  }

  Py_XDECREF(compiled);

  PyGILState_Release(gil);

  if(caughtException)
  {
    FlushOutput();
    emit exception(QString(), typeStr, valueStr, finalLine, frames);
  }
}

void PythonContext::executeString(const QString &source)
{
  executeString(QString(), source);
}

void PythonContext::executeFile(const QString &filename)
{
  QFile f(filename);

  if(!f.exists())
  {
    FlushOutput();
    emit exception(QString(), lit("FileNotFoundError"),
                   tr("No such file or directory: %1").arg(filename), -1, {});
    return;
  }

  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QByteArray py = f.readAll();

    executeString(filename, QString::fromUtf8(py));
  }
  else
  {
    FlushOutput();
    emit exception(QString(), lit("IOError"),
                   QFormatStr("%1: %2").arg(f.errorString()).arg(filename), -1, {});
  }
}

void PythonContext::setGlobal(const char *varName, const char *typeName, void *object)
{
  if(!initialised())
  {
    FlushOutput();
    emit exception(QString(), lit("SystemError"), tr("Python integration failed to initialise."),
                   -1, {});
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
    FlushOutput();
    emit exception(QString(), lit("RuntimeError"),
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

void PythonContext::reflectSource(QString src)
{
  if(!m_Reflector)
  {
    for(int i = 0; i < 50 && m_DeferredInit == 0; i++)
      QThread::msleep(20);
    m_DeferredInit = 1;

    if(!m_Reflector)
      return;
  }

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *refl = PyObject_CallFunction(m_Reflector, "sOO", (const char *)src.toUtf8().data(),
                                         context_namespace, Py_False);

  if(refl)
  {
    PyDict_SetItemString(context_namespace, "_renderdoc_refl", refl);

    Py_XDECREF(refl);
  }
  else
  {
    HandleException(NULL);
  }

  PyGILState_Release(gil);
}

void PythonContext::makeHelpContext()
{
  // expand out members of renderdoc and qrenderdoc using the stubs
  if(!m_StubRD || !m_StubQRD)
  {
    for(int i = 0; i < 50 && m_DeferredInit == 0; i++)
      QThread::msleep(20);

    m_DeferredInit = 1;

    if(!m_StubRD || !m_StubQRD)
      return;
  }

  PyGILState_STATE gil = PyGILState_Ensure();

  PyDict_SetItemString(context_namespace, "stub_rd", m_StubRD);
  PyDict_SetItemString(context_namespace, "stub_qrd", m_StubQRD);

  PyGILState_Release(gil);

  executeString(lit(R"(
def publicise_module(mod):
  for name in dir(mod):
    if name[0] == '_' or '_circular' in name:
      continue
    obj = getattr(mod, name)
    if hasattr(obj, '__module__') and mod.__name__ not in getattr(obj, '__module__'):
      continue
    globals()[name] = obj

publicise_module(stub_rd)
publicise_module(stub_qrd)

del publicise_module

)"));

  gil = PyGILState_Ensure();

  PyDict_DelItemString(context_namespace, "stub_rd");
  PyDict_DelItemString(context_namespace, "stub_qrd");

  if(PyDict_ContainsString(context_namespace, "pyrenderdoc"))
    PyDict_DelItemString(context_namespace, "pyrenderdoc");
  if(PyDict_ContainsString(context_namespace, "sys"))
    PyDict_DelItemString(context_namespace, "sys");

  PyGILState_Release(gil);

  reflectSource(QString());
}

QString PythonContext::tooltipForLoc(int line, int col)
{
  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *refl = PyDict_GetItemString(context_namespace, "_renderdoc_refl");

  if(!refl)
  {
    PyGILState_Release(gil);
    return QString();
  }

  PyObject *tooltip = PyObject_CallMethod(refl, "get_location_tooltip", "ii", line, col);

  QString ret;
  if(tooltip)
  {
    ret = ToQStr(tooltip);

    Py_XDECREF(tooltip);
  }
  else
  {
    HandleException(NULL);
  }

  PyGILState_Release(gil);

  return ret;
}

QString PythonContext::typenameForLoc(int line, int col)
{
  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *refl = PyDict_GetItemString(context_namespace, "_renderdoc_refl");

  if(!refl)
  {
    PyGILState_Release(gil);
    return QString();
  }

  PyObject *typeObj = PyObject_CallMethod(refl, "get_location_type", "ii", line, col);

  if(typeObj)
  {
    PyObject *typing = PyImport_ImportModule("typing");
    PyObject *Any = PyObject_SafeGetAttrString(typing, "Any");

    if(typeObj == Any)
    {
      Py_XDECREF(typeObj);
      typeObj = NULL;
    }

    Py_XDECREF(Any);
    Py_XDECREF(typing);
  }
  else
  {
    HandleException(NULL);
  }

  QString ret;

  if(typeObj)
  {
    PyObject *name = PyObject_CallMethod(refl, "get_name", "O", typeObj);

    if(name)
    {
      ret = ToQStr(name);

      Py_XDECREF(name);
    }
    else
    {
      HandleException(NULL);
    }

    Py_XDECREF(typeObj);
  }

  PyGILState_Release(gil);

  return ret;
}

QList<QPair<QString, QString>> PythonContext::completionOptions(int line, QString expr,
                                                                int &prefix_len)
{
  QList<QPair<QString, QString>> ret;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *refl = PyDict_GetItemString(context_namespace, "_renderdoc_refl");

  if(!refl)
  {
    PyGILState_Release(gil);
    return ret;
  }

  PyObject *completions = PyObject_CallMethod(refl, "get_autocompletion", "is", line,
                                              (const char *)expr.toUtf8().data());

  prefix_len = 0;

  if(completions)
  {
    PyObject *comp_list = PyTuple_GetItem(completions, 0);
    prefix_len = PyLong_AsLong(PyTuple_GetItem(completions, 1));
    PyObject *tip_list = PyTuple_GetItem(completions, 2);

    if(comp_list)
    {
      for(Py_ssize_t i = 0, len = PyList_Size(comp_list); i < len; i++)
      {
        QPair<QString, QString> item;
        item.first = ToQStr(PyList_GetItem(comp_list, i));
        if(tip_list)
          item.second = ToQStr(PyList_GetItem(tip_list, i));
        ret << item;
      }
    }
  }
  else
  {
    HandleException(NULL);
  }

  Py_XDECREF(completions);

  PyGILState_Release(gil);

  return ret;
}

QString PythonContext::tryFunctionCompletion(int line, QString expr)
{
  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *refl = PyDict_GetItemString(context_namespace, "_renderdoc_refl");

  if(!refl)
  {
    PyGILState_Release(gil);
    return QString();
  }

  PyObject *funcComp = PyObject_CallMethod(refl, "get_funccompletion", "is", line,
                                           (const char *)expr.toUtf8().data());

  QString ret;
  if(funcComp)
  {
    ret = ToQStr(PyTuple_GetItem(funcComp, 2));

    Py_XDECREF(funcComp);
  }
  else
  {
    HandleException(NULL);
  }

  PyGILState_Release(gil);

  return ret;
}

void PythonContext::AddDebuggableThread()
{
  if(!m_DebugPy)
    return;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *debug_this_thread = PyObject_SafeGetAttrString(m_DebugPy, "debug_this_thread");

  if(debug_this_thread)
  {
    PyObject *ret = PyObject_CallNoArgs(debug_this_thread);

    if(!ret)
      qCritical() << "Failed calling debugpy.debug_this_thread";

    Py_XDECREF(ret);
  }
  else
  {
    qCritical() << "Couldn't find debugpy.debug_this_thread";
  }

  Py_XDECREF(debug_this_thread);

  QString threadName = QThread::currentThread()->objectName();

  if(!threadName.isEmpty())
  {
    PyObject *threading = PyImport_ImportModule("threading");

    // expect to load threading
    if(threading)
    {
      PyObject *current_thread = PyObject_SafeGetAttrString(threading, "current_thread");

      Q_ASSERT(current_thread);

      PyObject *cur = PyObject_CallNoArgs(current_thread);

      if(cur)
      {
        if(PyObject_HasAttrString(cur, "name"))
          PyObject_SetAttrString(cur, "name", PyUnicode_FromString(threadName.toUtf8().data()));

        Py_XDECREF(cur);
      }

      Py_XDECREF(current_thread);
    }
    else
    {
      qCritical() << "Couldn't get threading module";
    }

    Py_XDECREF(threading);
  }

  PyGILState_Release(gil);
}

void PythonContext::RemoveDebuggableThread()
{
  if(!m_DebugPy)
    return;

    // 3.13 fixes this to track the lifetime of dummy/external threads, before that we do it manually
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 13
  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *threading = PyImport_ImportModule("threading");

  // expect to load threading
  if(threading)
  {
    PyObject *_active = PyObject_SafeGetAttrString(threading, "_active");

    if(_active)
    {
      PyObject *key = PyLong_FromInt32(PyThread_get_thread_ident());
      if(PyDict_Contains(_active, key) == 1)
        PyDict_DelItem(_active, key);
      Py_XDECREF(key);
    }

    Py_XDECREF(_active);
  }
  else
  {
    qCritical() << "Couldn't get threading module";
  }

  Py_XDECREF(threading);

  PyGILState_Release(gil);
#endif
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

  for(QString &extension : outputCaches.keys())
  {
    OutputPair &o = outputCaches[extension];

    if(!o.outstr.isEmpty())
    {
      emit textOutput(extension, false, o.outstr);
    }

    if(!o.errstr.isEmpty())
    {
      emit textOutput(extension, true, o.errstr);
    }

    o.outstr.clear();
    o.errstr.clear();
  }
}

void PythonContext::addText(QString context, bool isStdError, const QString &output)
{
  QMutexLocker lock(&outputMutex);

  if(isStdError)
    outputCaches[context].errstr += output;
  else
    outputCaches[context].outstr += output;
}

void PythonContext::setPyGlobal(const char *varName, PyObject *obj)
{
  if(!initialised())
  {
    FlushOutput();
    emit exception(QString(), lit("SystemError"), tr("Python integration failed to initialise."),
                   -1, {});
    return;
  }

  int ret = -1;

  PyGILState_STATE gil = PyGILState_Ensure();

  if(obj)
    ret = PyDict_SetItemString(context_namespace, varName, obj);

  PyGILState_Release(gil);

  if(ret == 0)
    return;

  FlushOutput();
  emit exception(QString(), lit("RuntimeError"),
                 tr("Failed to set variable '%1'").arg(QString::fromUtf8(varName)), -1, {});
}

void PythonContext::outstream_del(PyObject *self)
{
  OutputRedirector *redirector = (OutputRedirector *)self;

  if(redirector && redirector->selfDeleting)
  {
    PythonContext *context = redirector->context;

    // delete the context on the UI thread.
    if(context)
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
    QString extension = redirector->extension;
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
          {
            context = global->context;
            extension = global->extension;
          }
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
      context->addText(extension, redirector->isStdError ? true : false, QString::fromUtf8(text));
    }
    else
    {
      if(!extension.isEmpty())
      {
        m_ExtensionContext->addText(extension, redirector->isStdError ? true : false,
                                    QString::fromUtf8(text));
      }

      // if context is still NULL we're running in the extension context
      rdcstr message = text;

      PyFrameObject *frame = PyEval_GetFrame();

      while(!message.empty() && (message.back() == '\n' || message.back() == '\r'))
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
        RENDERDOC_LogMessage(redirector->isStdError ? LogType::Warning : LogType::Comment, "EXTN",
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

PyObject *PythonContext::outstream_trace(PyObject *self, PyObject *args, PyObject *kwargs)
{
  if(PyErr_Occurred())
    return NULL;

  const char *what = NULL;
  PyObject *frameObj = NULL;
  PyObject *arg = NULL;

  if(!PyArg_ParseTuple(args, "OzO:trace", &frameObj, &what, &arg))
    return NULL;

  OutputRedirector *redirector = (OutputRedirector *)self;

  if(PyFrame_Check(frameObj) && what && strcmp(what, "line") == 0)
  {
    PyFrameObject *frame = (PyFrameObject *)frameObj;
    PythonContext *context = redirector->context;

    // increment ref here so we can loop with a held ref either from the base frame or from PyFrame_GetBack()
    Py_XINCREF(frame);

    // step up the frame looking for one in our code we're watching
    while(frame)
    {
      PyCodeObject *code = PyFrame_GetCode(frame);

      if(redirector->compiled == (PyObject *)code && context)
      {
        context->location.line = PyFrame_GetLineNumber(frame);

        emit context->traceLine(context->location.file, context->location.line);

        Py_XDECREF(frame);
        Py_XDECREF(code);
        break;
      }

      Py_XDECREF(code);

      // first get the next frame without decrefing the current
      PyFrameObject *back = PyFrame_GetBack(frame);
      // now decref the old frame
      Py_XDECREF(frame);
      // and iterate on the next one, if we got one
      frame = back;
    }

    if(context && context->shouldAbort())
    {
      PyErr_SetString(PyExc_SystemExit, "Execution aborted.");
      return NULL;
    }
  }

  if(redirector->chain_trace && !Py_IsNone(redirector->chain_trace))
  {
    PyObject *prev_trace = redirector->chain_trace;
    PyObject *new_trace = PyObject_Call(redirector->chain_trace, args, kwargs);
    Py_XDECREF(prev_trace);
    redirector->chain_trace = new_trace;
  }

  Py_XINCREF(self);
  return self;
}

bool PythonContext::IsDebuggerConnected()
{
  if(!m_DebugPy)
    return false;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *is_connected = PyObject_CallMethod(m_DebugPy, "is_client_connected", NULL);
  bool ret = (PyBool_Check(is_connected) && is_connected == Py_True);
  Py_XDECREF(is_connected);

  PyGILState_Release(gil);

  return ret;
}

void PythonContext::PrepareDebuggerWait()
{
  if(!m_DebugPy)
    return;

  // reset the semaphore
  while(debuggerWaitSemaphore.available())
    debuggerWaitSemaphore.tryAcquire();

  // set up one count in the semaphore
  debuggerWaitSemaphore.release();
}

bool PythonContext::WaitForDebugger()
{
  if(!m_DebugPy)
    return false;

  PyGILState_STATE gil = PyGILState_Ensure();

  // don't care about the return value
  Py_XDECREF(PyObject_CallMethod(m_DebugPy, "wait_for_client", NULL));

  // return whether a client connected

  PyObject *is_connected = PyObject_CallMethod(m_DebugPy, "is_client_connected", NULL);
  bool ret = (PyBool_Check(is_connected) && is_connected == Py_True);
  Py_XDECREF(is_connected);

  // indicate that the work has finished
  debuggerWaitSemaphore.tryAcquire(1);

  PyGILState_Release(gil);

  return ret;
}

void PythonContext::LaunchDebugger(QWidget *window, PersistentConfig &config, QString context_location)
{
  if(!m_DebugPy)
    return;

  if(context_location.isEmpty())
    return;

  // don't overwrite an existing file, to allow user customisation
  QDir context_dir(context_location);
  if(!context_dir.exists(lit(".vscode/launch.json")))
  {
    context_dir.mkpath(lit(".vscode"));
    QFile launch(context_dir.absoluteFilePath(lit(".vscode/launch.json")));
    launch.open(QFile::Truncate | QFile::WriteOnly);
    launch.write(R"(
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Python Debugger: Remote Attach",
            "type": "debugpy",
            "request": "attach",
            "connect": { "host": "localhost", "port": 5678 }
        }
    ]
}
	)");
    launch.close();

    // write tasks to auto-attach. This will require user approval
    QFile tasks(context_dir.absoluteFilePath(lit(".vscode/tasks.json")));
    tasks.open(QFile::Truncate | QFile::WriteOnly);
    tasks.write(R"(
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "attach on startup",
            "command": "${command:workbench.action.debug.start}",
            "runOptions": {
                "runOn": "folderOpen"
            }
        }
    ]
}
	)");
    tasks.close();
  }

  GUIInvoke::defer(window, [window, &config, context_location]() {
    // wait a short while before displaying the progress dialog in case a debugger is already connected
    for(int i = 0; debuggerWaitSemaphore.available() == 1 && i < 40; i++)
      QThread::msleep(5);

    // if we should launch vs code and there's nothing connected, do that now
    if(config.Python_LaunchVSCode)
    {
      PyGILState_STATE gil = PyGILState_Ensure();

      PyObject *is_connected_ret = PyObject_CallMethod(m_DebugPy, "is_client_connected", NULL);
      bool debugger_connected = (PyBool_Check(is_connected_ret) && is_connected_ret == Py_True);
      Py_XDECREF(is_connected_ret);

      PyGILState_Release(gil);

      QString code_path = config.Python_VSCodePath;

      if(!QFileInfo(code_path).isExecutable())
        code_path = QStandardPaths::findExecutable(lit("code"));

      if(!debugger_connected && !code_path.isEmpty())
      {
        QStringList args;
        args << lit("-n");
        if(!context_location.isEmpty())
          args << context_location;

        QProcess::startDetached(code_path, args);
      }
    }

    ShowProgressDialog(
        window, tr("Waiting for debugger to connect.\n\nListening on localhost:5678"),
        []() { return debuggerWaitSemaphore.available() == 0; }, NULL,
        []() {
          PyGILState_STATE gil = PyGILState_Ensure();

          PyObject *wait_for_client = PyObject_SafeGetAttrString(m_DebugPy, "wait_for_client");
          if(wait_for_client)
          {
            Py_XDECREF(PyObject_CallMethod(wait_for_client, "cancel", NULL));
            Py_XDECREF(wait_for_client);
          }

          PyGILState_Release(gil);
        });
  });
}

PyParseError PythonContext::CheckPyParse(const QByteArray &script, const rdcstr &scriptNameForErrors)
{
  PyParseError parseError;

  PyGILState_STATE gil = PyGILState_Ensure();

  PyObject *ast = PyImport_ImportModule("ast");

  PyObject *scriptText = PyUnicode_FromStringAndSize(script.data(), script.size());
  PyObject *scriptName = PyUnicode_FromString(scriptNameForErrors.c_str());

  PyObject *parseResult = PyObject_CallMethod(ast, "parse", "OO", scriptText, scriptName);

  if(!parseResult)
  {
    PyObject *exObj = NULL, *valueObj = NULL, *tracebackObj = NULL;

    PyErr_Fetch(&exObj, &valueObj, &tracebackObj);
    PyErr_NormalizeException(&exObj, &valueObj, &tracebackObj);

    {
      PyObject *a;

      PyObject_GetOptionalAttrString(valueObj, "lineno", &a);
      parseError.lineno = PyLong_AsInt(a);
      Py_XDECREF(a);

      PyObject_GetOptionalAttrString(valueObj, "offset", &a);
      parseError.offset = PyLong_AsInt(a);
      Py_XDECREF(a);

      PyObject *repr = PyObject_Str(valueObj);
      PyObject *utf8 = PyUnicode_AsUTF8String(repr);
      parseError.errStr = PyBytes_AsString(utf8);
      for(int i = 1; i < parseError.offset - 1; i++)
        parseError.errStr.insert(0, ' ');
      parseError.errStr.insert(parseError.offset - 2, "^\n");

      Py_XDECREF(repr);
      Py_XDECREF(utf8);
    }

    Py_XDECREF(exObj);
    Py_XDECREF(valueObj);
    Py_XDECREF(tracebackObj);
  }

  Py_XDECREF(scriptText);
  Py_XDECREF(scriptName);
  Py_XDECREF(parseResult);

  Py_XDECREF(ast);

  PyGILState_Release(gil);

  return parseError;
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
    PyObject *ret = PyObject_SafeGetAttrString(sys, "_renderdoc_internal");

    Py_XDECREF(sys);
    Py_XDECREF(ret);
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
    redirector->context->FlushOutput();
    emit redirector->context->exception(redirector->extension, typeStr, valueStr, finalLine, frames);
  }
  else
  {
    if(redirector && !redirector->extension.empty())
    {
      PythonContext::GetExtensionContext()->FlushOutput();
      emit PythonContext::GetExtensionContext()->exception(redirector->extension, typeStr, valueStr,
                                                           finalLine, frames);
    }

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
    else if(redirector)
    {
      filename = redirector->extension;
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

extern "C" PyObject *DoFunctionCall(PyObject *object, PyObject *args)
{
  if(!PyCFunction_Check(object))
    PythonContext::PrepareDebugTracing();

  return PyObject_Call(object, args, NULL);
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
