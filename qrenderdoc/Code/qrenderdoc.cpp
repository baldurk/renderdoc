/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2026 Baldur Karlsson
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

#include <stdio.h>
#if defined(RENDERDOC_PLATFORM_LINUX)
#include <execinfo.h>
#endif
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include <QSysInfo>
#include "Code/CaptureContext.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "Windows/Dialogs/CrashDialog.h"
#include "Windows/MainWindow.h"
#include "version.h"

#if defined(RENDERDOC_PLATFORM_LINUX)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/qguiapplication_platform.h>
#include <QtGui/QGuiApplication>
#else
#include <QX11Info>
#endif
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)

#include <QOperatingSystemVersion>

QString getOSVersion()
{
  QOperatingSystemVersion ver = QOperatingSystemVersion::current();

  if(ver.type() == QOperatingSystemVersion::Windows && ver.majorVersion() >= 10)
  {
    int major = ver.majorVersion();
    int build = ver.microVersion();
    if(build >= 22000)
      major = 11;

    return QFormatStr("Windows %1 Build num %2").arg(major).arg(build);
  }

  return QSysInfo::prettyProductName();
}

#else

QString getOSVersion()
{
  return QSysInfo::prettyProductName();
}

#endif

#if ENABLE_UNIT_TESTS

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_NOSTDOUT

#include "3rdparty/catch/catch.hpp"

// since we force use of ToStr for everything and don't allow using catch's stringstream (so that
// enums get forwarded to ToStr) we need to implement ToStr for one of Catch's structs.
template <>
rdcstr DoStringise(const Catch::SourceLineInfo &el)
{
  return QFormatStr("%1:%2").arg(QString::fromUtf8(el.file)).arg(el.line);
}

class LogOutputter : public std::stringbuf
{
  FILE *file;

public:
  LogOutputter(FILE *f) : file(f) {}
  void finish()
  {
    std::string msg = this->str();
    RENDERDOC_LogMessage(LogType::Comment, "EXTN", __FILE__, __LINE__, msg.c_str());
    fputs(msg.c_str(), file);
  }
  virtual int sync() override
  {
    rdcstr str = this->str().c_str();
    int idx = str.indexOf('\n');
    if(idx >= 0)
    {
      rdcstr msg = str.substr(0, idx + 1);
      RENDERDOC_LogMessage(LogType::Comment, "EXTN", __FILE__, __LINE__, msg);
      if(file)
        fputs(msg.c_str(), file);
      str = str.substr(idx + 1);
      this->str("");
      this->sputn(str.c_str(), str.size());
    }
    return 0;
  }

  // force a sync on every output
  virtual std::streamsize xsputn(const char *s, std::streamsize n) override
  {
    std::streamsize ret = std::stringbuf::xsputn(s, n);
    sync();
    return ret;
  }
};

std::ostream *catch_stream = NULL;

namespace Catch
{
std::ostream &cout()
{
  return *catch_stream;
}
std::ostream &cerr()
{
  return *catch_stream;
}
std::ostream &clog()
{
  return *catch_stream;
}
}

#endif

#if defined(Q_OS_WIN32)
extern "C" {
_declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

REPLAY_PROGRAM_MARKER()

void sharedLogOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
  LogType logtype = LogType::Comment;

  switch(type)
  {
    case QtDebugMsg: logtype = LogType::Debug; break;
    case QtInfoMsg: logtype = LogType::Comment; break;
    case QtWarningMsg: logtype = LogType::Warning; break;
    case QtCriticalMsg: logtype = LogType::Error; break;
    case QtFatalMsg: logtype = LogType::Fatal; break;
  }

  static bool inHandler = false;
  static bool shownShmErrorWorkaround = false;
#if defined(RENDERDOC_PLATFORM_LINUX)
  if(!inHandler && type != QtDebugMsg && type != QtInfoMsg)
  {
    bool isWaylandMessage =
        msg.contains(QLatin1String("wl_")) || msg.contains(QLatin1String("Wayland"));

    bool isActualError = msg.contains(QLatin1String("error"), Qt::CaseInsensitive) ||
                         msg.contains(QLatin1String("failed"), Qt::CaseInsensitive) ||
                         msg.contains(QLatin1String("invalid"), Qt::CaseInsensitive);

    bool isNonError = msg.contains(QLatin1String("discarded"), Qt::CaseInsensitive) ||
                      msg.contains(QLatin1String("wl_buffer#"), Qt::CaseInsensitive) ||
                      msg.contains(QLatin1String(".release()"), Qt::CaseInsensitive) ||
                      msg.contains(QLatin1String("wl_callback"), Qt::CaseInsensitive) ||
                      msg.contains(QLatin1String(".done("), Qt::CaseInsensitive);

    if(isWaylandMessage && isActualError && !isNonError)
    {
      inHandler = true;
      fprintf(stderr, "\n================ WAYLAND ERROR DETECTED ================\n");
      fprintf(stderr, "Qt Message Type: %d\n", type);
      fprintf(stderr, "Qt Message: %s\n", msg.toUtf8().constData());
      if(context.file)
        fprintf(stderr, "  at %s:%d\n", context.file, context.line);

      bool isKnownShmError = msg.contains(QLatin1String("QWaylandShmBackingStore")) ||
                             msg.contains(QLatin1String("fatal error during blocking read"));

      if(isKnownShmError && !shownShmErrorWorkaround)
      {
        fprintf(stderr, "\n");
        fprintf(stderr, "!!! DETECTED KNOWN WAYLAND/SHM PROTOCOL ERROR !!!\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "This error occurs when Qt's Wayland SHM backing store\n");
        fprintf(stderr, "is incompatible with your compositor's configuration.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "RECOMMENDED WORKAROUND:\n");
        fprintf(stderr, "  Run with XWayland backend:\n");
        fprintf(stderr, "    QT_QPA_PLATFORM=xcb %s\n",
                qApp->arguments().first().toUtf8().constData());
        fprintf(stderr, "\n");
        fprintf(stderr, "Or add to your shell profile (~/.bashrc or ~/.zshrc):\n");
        fprintf(stderr, "  export QT_QPA_PLATFORM=xcb\n");
        fprintf(stderr, "\n");
        shownShmErrorWorkaround = true;
      }

      fprintf(stderr, "\nStack trace:\n");

      void *buffer[64];
      int nptrs = backtrace(buffer, 64);
      char **strings = backtrace_symbols(buffer, nptrs);
      if(strings)
      {
        for(int j = 0; j < nptrs; j++)
          fprintf(stderr, "  #%d %s\n", j, strings[j]);
        free(strings);
      }
      fprintf(stderr, "============================================================\n\n");
      fflush(stderr);
      inHandler = false;
    }
  }
#else
  (void)inHandler;                  // suppress unused warning
  (void)shownShmErrorWorkaround;    // suppress unused warning
#endif

  RENDERDOC_LogMessage(logtype, "QTRD", context.file ? context.file : rdcstr(), context.line, msg);
}

static QString tr(const char *string)
{
  return QApplication::translate("qrenderdoc", string);
}

void hideOption(QCommandLineOption &opt)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
  opt.setFlags(QCommandLineOption::HiddenFromHelp);
#else
  opt.setHidden(true);
#endif
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  // call this as the very first thing - no-op on other platforms, but on linux it means
  // XInitThreads will be called allowing driver access to xlib on multiple threads.
  QCoreApplication::setAttribute(Qt::AA_X11InitThreads);
#endif

  qInstallMessageHandler(sharedLogOutput);

  // there seems to be a persistent crash in QWidgetPrivate::subtractOpaqueSiblings where a widget
  // has no parent but is not a window. Try to work around it by setting this env var, as it's only
  // an optimisation
  qputenv("QT_NO_SUBTRACTOPAQUESIBLINGS", lit("1").toUtf8());

  qInfo() << "QRenderDoc initialising.";

  if(IsRunningAsAdmin())
    qInfo() << "Running as administrator";

#if defined(RENDERDOC_PLATFORM_LINUX) && !defined(RENDERDOC_WINDOWING_WAYLAND)
  bool envChanged = false;
  {
    const char *qpa_plat = getenv("QT_QPA_PLATFORM");
    // if not set or empty, force non-wayland to help go through backwards compatibility path on wayland.
    if(!qpa_plat || qpa_plat[0] == 0)
    {
      setenv("QT_QPA_PLATFORM", "xcb", 1);
      envChanged = true;
    }
  }
#elif defined(RENDERDOC_PLATFORM_LINUX) && defined(RENDERDOC_WINDOWING_WAYLAND)
  bool envChanged = false;
  bool explicitSyncDetected = CheckWaylandExplicitSyncEarly();
  if(explicitSyncDetected)
  {
    fprintf(stderr,
            "============================================================================\n");
    fprintf(stderr, "WAYLAND EXPLICIT SYNC DETECTED - AUTOMATIC XWAYLAND FALLBACK\n");
    fprintf(stderr,
            "============================================================================\n");
    fprintf(stderr, "Your Wayland compositor uses explicit synchronization\n");
    fprintf(stderr, "(wp_linux_drm_syncobj_manager_v1), which is incompatible\n");
    fprintf(stderr, "with Qt's default SHM backing store.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Automatically falling back to XWayland for stability.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "To override this fallback, set QT_QPA_PLATFORM=wayland\n");
    fprintf(stderr, "or disable explicit sync in your compositor settings.\n");
    fprintf(stderr,
            "============================================================================\n\n");

    setenv("QT_QPA_PLATFORM", "xcb", 1);
    envChanged = true;
  }
#endif

  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

#if(QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

  QApplication::setApplicationVersion(lit(FULL_VERSION_STRING));

// shortcut here so we can run this with a non-GUI application
#if ENABLE_UNIT_TESTS
  if(argc >= 2 && QString::fromUtf8(argv[1]) == lit("--unittest"))
  {
    rdcarray<char *> appArgs;
    appArgs.push_back(argv[0]);

    FILE *test_logOut = NULL;

    for(int i = 2; i < argc; i++)
    {
      QString arg = QString::fromUtf8(argv[i]);
      if(arg.left(4) == lit("log="))
      {
        test_logOut = fopen(argv[i] + 4, "w");
        if(test_logOut == NULL)
        {
          fprintf(stderr, "Failed to open log file '%s' for writing\n", argv[i] + 4);
          test_logOut = stdout;
        }
      }
      else
      {
        appArgs.push_back(argv[i]);
      }
    }

    appArgs.push_back(NULL);

    int appArgc = appArgs.count() - 1;
    char **appArgv = appArgs.data();

    if(test_logOut == NULL)
      test_logOut = stdout;

    LogOutputter logbuf(test_logOut);
    std::ostream logstream(&logbuf);

    int ret = 0;

    // catch tests first
    {
      catch_stream = &logstream;

      Catch::Session session;

      session.configData().name = "QRenderDoc";
      session.configData().shouldDebugBreak = Catch::isDebuggerActive();

      ret = session.applyCommandLine(appArgc, appArgv);

      if(ret == 0)
      {
        int numFailed = session.run();

        // Note that on unices only the lower 8 bits are usually used, clamping
        // the return value to 255 prevents false negative when some multiple
        // of 256 tests has failed
        if(numFailed != 0)
          ret = (numFailed < 0xff ? numFailed : 0xff);
      }
    }

    GlobalEnvironment env;
    env.enumerateGPUs = false;
    rdcarray<rdcstr> coreargs;
    RENDERDOC_InitialiseReplay(env, coreargs);

    {
      QCoreApplication application(appArgc, appArgv);

      PythonContext::GlobalInit();

      logstream << "Checking python binding consistency.\n";

      rdcstr errorLog;
      bool errors = false;
      {
        PythonContextHandle py;
        errors = py.ctx().CheckInterfaces(errorLog);
      }

      if(errors)
      {
        logstream << errorLog;
        qCritical() << "Found errors in python bindings. Please fix!\n";
        ret = 1;
      }
      else
      {
        logstream << "Python bindings are consistent.\n";
      }

      PythonContext::GlobalShutdown();
    }

    RENDERDOC_ShutdownReplay();

    logbuf.finish();

    // Only close if we opened a file, not if using stdout
    if(test_logOut != stdout)
      fclose(test_logOut);

    return ret;
  }
#endif

  QApplication application(argc, argv);

#if defined(RENDERDOC_PLATFORM_LINUX)
  InstallGlobalWaylandWorkaround();
#endif

  QCommandLineParser parser;
  parser.setApplicationDescription(tr("Qt UI for RenderDoc"));
  QCommandLineOption helpOption = parser.addHelpOption();
  QCommandLineOption versionOption = parser.addVersionOption();

  QCommandLineOption tempfile(
      lit("tempfile"), tr("The filename to be opened is a temporary file owned by this instance."));
  parser.addOption(tempfile);

  QCommandLineOption targetcontrol({lit("targetcontrol"), lit("remoteaccess")},
                                   tr("A target control connection to open on startup."),
                                   lit("host:port"));
  parser.addOption(targetcontrol);

  QCommandLineOption replayhost(lit("replayhost"), tr("The replay host to connect to on startup."),
                                lit("host"));
  parser.addOption(replayhost);

  QCommandLineOption python({lit("python"), lit("script"), lit("py")},
                            tr("Run a python script before opening the main UI."),
                            lit("filename.py"));
  parser.addOption(python);

  QCommandLineOption uiscript({lit("ui-python"), lit("ui-script"), lit("ui-py")},
                              tr("Run a python script after opening the main UI."),
                              lit("filename.py"));
  parser.addOption(uiscript);

  // secret non-described options
  QCommandLineOption installLayer(lit("install_vulkan_layer"), QString(), lit("root_or_not"));
  hideOption(installLayer);
  parser.addOption(installLayer);

  QCommandLineOption updateFailed(lit("updatefailed"), QString(), lit("errormsg"));
  hideOption(updateFailed);
  parser.addOption(updateFailed);

  QCommandLineOption updateDoneAdmin(lit("updatedone_admin"));
  hideOption(updateDoneAdmin);
  parser.addOption(updateDoneAdmin);

  QCommandLineOption updateDone(lit("updatedone"));
  hideOption(updateDone);
  parser.addOption(updateDone);

  QCommandLineOption crashReport(lit("crash"), QString(), lit("reportpath"));
  hideOption(crashReport);
  parser.addOption(crashReport);

  parser.addPositionalArgument(lit("filename"), tr("The file to open."));

  bool parsedCommands = parser.parse(application.arguments());

  if(!parsedCommands)
    qCritical() << parser.errorText();

  if(parser.isSet(helpOption))
  {
    parser.showHelp();
    return 0;
  }

  if(parser.isSet(versionOption))
  {
    printf("QRenderDoc v%s (%s)\n", MAJOR_MINOR_VERSION_STRING, RENDERDOC_GetCommitHash());
#if defined(DISTRIBUTION_VERSION)
    printf("Packaged for %s - %s\n", DISTRIBUTION_NAME, DISTRIBUTION_CONTACT);
#endif
    return 0;
  }

  if(parser.isSet(installLayer))
  {
    qInfo() << "Updating Vulkan layer registration";
    if(parser.value(installLayer) == lit("root"))
      RENDERDOC_UpdateVulkanLayerRegistration(true);
    else
      RENDERDOC_UpdateVulkanLayerRegistration(false);
    return 0;
  }

  bool temp = parser.isSet(tempfile);

  bool updateApplied = false;

  if(parser.isSet(updateFailed))
  {
    RDDialog::critical(NULL, tr("Error updating"),
                       tr("Error applying update: %1").arg(parser.value(updateFailed)));
  }

  if(parser.isSet(updateDone))
  {
    qInfo() << "Finishing update as user";
    updateApplied = true;

    // the renderdoccmd updater that runs us is from the old version, so older versions might be
    // running us as admin expecting the version number to be updated.
    // if we're not running as admin, this will immediately exit
    RENDERDOC_UpdateInstalledVersionNumber();
  }

  if(parser.isSet(updateDoneAdmin))
  {
    qInfo() << "Finishing update as admin";
    RENDERDOC_UpdateInstalledVersionNumber();
    return 0;
  }

  QString remoteHost;
  uint remoteIdent = 0;

  if(parser.isSet(targetcontrol))
  {
    QRegularExpression regexp(lit("^([a-zA-Z\\.0-9_-]+)?(:([0-9]+))?$"));

    QRegularExpressionMatch match = regexp.match(parser.value(targetcontrol));

    if(!match.hasMatch())
    {
      qCritical() << "--targetcontrol option must be followed by host:port or host";
      return 1;
    }

    QString host = match.captured(1);

    bool ok = false;
    uint32_t ident = 0;
    if(match.capturedLength(2) > 0)
    {
      ident = match.captured(3).toUInt(&ok);
    }
    else
    {
      // no port specified, find the first open port.
      ident = RENDERDOC_EnumerateRemoteTargets(host, ident);
      ok = (ident != 0);
    }

    if(!ok)
    {
      if(match.capturedLength(2) > 0)
      {
        qCritical() << "--targetcontrol port " << match.captured(3) << "malformed";
      }
      else
      {
        qCritical() << "All ports are busy, cannot find an available port";
      }
      return 1;
    }
    remoteHost = host;
    remoteIdent = ident;
  }

  QString crashReportPath;
  if(parser.isSet(crashReport))
    crashReportPath = parser.value(crashReport);

  QString uiscriptFile;
  if(parser.isSet(uiscript))
    uiscriptFile = parser.value(uiscript);

  QStringList pyscripts = parser.values(python);

  // load the first filename in the positional arguments.
  QStringList remaining = parser.positionalArguments();

  QString filename;
  for(int i = 0; i < remaining.count(); i++)
  {
    const QString &fn = remaining[i];
    QFileInfo checkFile(fn);
    if(checkFile.exists() && checkFile.isFile())
    {
      filename = fn;
      remaining.removeAt(i);
      break;
    }
  }

  RegisterMetatypeConversions();

  {
    PersistantConfig config;

    {
      QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      QDir dir(configPath);

      if(!dir.exists())
        dir.mkpath(configPath);
    }

    QString configFilename = ConfigFilePath(lit("UI.config"));

    if(!config.Load(configFilename))
    {
      RDDialog::critical(
          NULL, CaptureContext::tr("Error loading config"),
          CaptureContext::tr(
              "Error loading config file\n%1\nA default config is loaded and will be saved out.")
              .arg(configFilename));
    }

    int replayHostIndex = -1;
    if(parser.isSet(replayhost))
    {
      rdcstr replayHost = parser.value(replayhost);
      rdcarray<RemoteHost> hosts = config.GetRemoteHosts();
      for(int i = 0; i < hosts.count(); i++)
      {
        if(hosts[i].Hostname() == replayHost)
        {
          replayHostIndex = i;
          break;
        }
      }
      if(replayHostIndex < 0)
      {
        RDDialog::critical(
            NULL, tr("Error loading remote host"),
            tr("Remote host %1 doesn't exist. Please add it in Remote Host Manager first.")
                .arg(parser.value(replayhost)));
      }
    }

    if(config.Analytics_TotalOptOut)
      Analytics::Disable();
    else
      Analytics::Load();

    bool isDarkTheme = IsDarkTheme();

    bool styleSet = config.SetStyle();

    // unrecognised style, or empty (none set), choose a default
    if(!styleSet)
    {
      config.UIStyle = isDarkTheme ? lit("RDDark") : lit("RDLight");

      config.SetStyle();
    }

    config.SetupFormatting();

    Resources::Initialise();

    GUIInvoke::init();

    {
      GlobalEnvironment env;
#if defined(RENDERDOC_PLATFORM_LINUX)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      QNativeInterface::QX11Application *x11Interface =
          qApp->nativeInterface<QNativeInterface::QX11Application>();
      if(x11Interface)
        env.xlibDisplay = x11Interface->display();
#else
      env.xlibDisplay = QX11Info::display();
#endif
      if(QGuiApplication::platformName() == lit("wayland"))
      {
        env.waylandDisplay = (wl_display *)AccessWaylandPlatformInterface("display", NULL);

        bool hasExplicitSync = CheckWaylandExplicitSyncSupport();
        if(hasExplicitSync)
        {
          qWarning() << "============================================================";
          qWarning() << "WAYLAND EXPLICIT SYNC DETECTED";
          qWarning() << "============================================================";
          qWarning() << "Your Wayland compositor uses explicit synchronization";
          qWarning() << "(wp_linux_drm_syncobj_manager_v1), which is incompatible";
          qWarning() << "with Qt's default SHM backing store.";
          qWarning() << "";
          qWarning() << "You have overridden the automatic XWayland fallback.";
          qWarning() << "Errors may occur. Proceeding with native Wayland support.";
          qWarning() << "============================================================";
        }
      }
#endif
      rdcarray<rdcstr> coreargs;
      if(!crashReportPath.isEmpty())
        coreargs.push_back("--crash");
      for(const QString &arg : remaining)
        coreargs.push_back(arg);

      // don't enumerate GPUs when reporting a crash, in case enumerating GPUs *causes* the crash.
      if(!crashReportPath.isEmpty())
        env.enumerateGPUs = false;

      RENDERDOC_InitialiseReplay(env, coreargs);
    }

#if defined(RENDERDOC_PLATFORM_LINUX)
    if(envChanged)
      unsetenv("QT_QPA_PLATFORM");
#endif

    if(!crashReportPath.isEmpty())
    {
      QVariantMap json;

      {
        QFile f(crashReportPath);

        if(f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text))
          json = JSONToVariant(QString::fromUtf8(f.readAll()));
      }

      if(json.contains(lit("report")))
      {
        CrashDialog dialog(config, json);

        RDDialog::show(&dialog);
      }
    }
    else
    {
      PythonContext::GlobalInit();

      if(updateApplied)
      {
        config.CheckUpdate_UpdateAvailable = false;
        config.CheckUpdate_UpdateResponse = "";
        config.Save();
      }

      CaptureContext ctx(config);
      if(replayHostIndex >= 0)
      {
        ctx.SetRemoteHost(replayHostIndex);
      }
      Analytics::Prompt(ctx, config);

      ANALYTIC_SET(Metadata.RenderDocVersion, lit(FULL_VERSION_STRING));
#if defined(DISTRIBUTION_VERSION)
      ANALYTIC_SET(Metadata.DistributionVersion, lit(DISTRIBUTION_NAME));
#endif
      ANALYTIC_SET(Metadata.Bitness, ((sizeof(void *) == sizeof(uint64_t)) ? 64 : 32));
      ANALYTIC_SET(Metadata.OSVersion, getOSVersion());

#if RENDERDOC_STABLE_BUILD
      ANALYTIC_SET(Metadata.OfficialBuildRun, true);
#else
      ANALYTIC_SET(Metadata.DevelBuildRun, true);
#endif

      ANALYTIC_SET(Metadata.DaysUsed[QDateTime::currentDateTime().date().day()], true);

      bool pythonExited = false;

      if(!pyscripts.isEmpty())
      {
        PythonContextHandle py;

        ANALYTIC_SET(UIFeatures.PythonInterop, true);

        py.ctx().setGlobal("pyrenderdoc", (ICaptureContext *)&ctx);

        QObject::connect(
            &py.ctx(), &PythonContext::exception,
            [&pythonExited](const QString &type, const QString &value, int, QList<QString> frames) {
              if(type == lit("SystemExit"))
              {
                pythonExited = true;
                return;
              }

              QString exString;

              if(!frames.isEmpty())
              {
                exString += tr("Traceback (most recent call last):\n");
                for(const QString &f : frames)
                  exString += QFormatStr("  %1\n").arg(f);
              }

              exString += QFormatStr("%1: %2\n").arg(type).arg(value);

              qCritical("%s", exString.toUtf8().data());
            });

        QObject::connect(&py.ctx(), &PythonContext::textOutput,
                         [](bool isStdError, const QString &output) {
                           if(isStdError)
                             qCritical("%s", output.toUtf8().data());
                           else
                             qInfo("%s", output.toUtf8().data());
                         });

        for(const QString &f : pyscripts)
        {
          QFileInfo checkFile(f);
          if(checkFile.exists() && checkFile.isFile())
          {
            qInfo() << "running" << f;
            py.ctx().executeFile(f);
          }
          else
          {
            qWarning() << "Invalid python script" << f;
          }

          if(pythonExited)
            break;
        }
      }

      if(!pythonExited)
      {
        ctx.Begin(filename, remoteHost, remoteIdent, temp, uiscriptFile);

        while(ctx.isRunning())
        {
          application.processEvents(QEventLoop::WaitForMoreEvents);
          QCoreApplication::sendPostedEvents();
          QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
        }
      }

      config.Save();
    }

    RENDERDOC_ShutdownReplay();

    PythonContext::GlobalShutdown();

    Formatter::shutdown();
  }

  return 0;
}
