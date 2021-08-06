/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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
  // call this as the very first thing - no-op on other platforms, but on linux it means
  // XInitThreads will be called allowing driver access to xlib on multiple threads.
  QCoreApplication::setAttribute(Qt::AA_X11InitThreads);

  qInstallMessageHandler(sharedLogOutput);

  // there seems to be a persistent crash in QWidgetPrivate::subtractOpaqueSiblings where a widget
  // has no parent but is not a window. Try to work around it by setting this env var, as it's only
  // an optimisation
  qputenv("QT_NO_SUBTRACTOPAQUESIBLINGS", lit("1").toUtf8());

  qInfo() << "QRenderDoc initialising.";

  if(IsRunningAsAdmin())
    qInfo() << "Running as administrator";

  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

#if(QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

  QApplication::setApplicationVersion(lit(FULL_VERSION_STRING));

// shortcut here so we can run this with a non-GUI application
#if !defined(RELEASE)
  if(QString::fromUtf8(argv[1]) == lit("--unittest"))
  {
    QCoreApplication application(argc, argv);
    PythonContext::GlobalInit();

    bool errors = false;

    FILE *logOut = NULL;

    if(argc >= 3)
      logOut = fopen(argv[2], "w");

    if(logOut == NULL)
      logOut = stdout;

    fputs("Checking python binding consistency.\n", logOut);

    rdcstr errorLog;

    {
      PythonContextHandle py;
      errors = py.ctx().CheckInterfaces(errorLog);
    }

    if(errors)
    {
      RENDERDOC_LogMessage(LogType::Error, "EXTN", __FILE__, __LINE__, errorLog);
      fputs("Found errors in python bindings. Please fix!\n", logOut);
      fputs(errorLog.c_str(), logOut);
      return 1;
    }
    else
    {
      fputs("Python bindings are consistent.\n", logOut);
      return 0;
    }
  }
#endif

  QApplication application(argc, argv);

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

  // secret non-described options
  QCommandLineOption installLayer(lit("install_vulkan_layer"), QString(), lit("root_or_not"));
  hideOption(installLayer);
  parser.addOption(installLayer);

  QCommandLineOption updateFailed(lit("updatefailed"), QString(), lit("errormsg"));
  hideOption(updateFailed);
  parser.addOption(updateFailed);

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
    updateApplied = true;

    RENDERDOC_UpdateInstalledVersionNumber();
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
      env.xlibDisplay = QX11Info::display();
      if(QGuiApplication::platformName() == lit("wayland"))
      {
        env.waylandDisplay = (wl_display *)AccessWaylandPlatformInterface("display", NULL);

        QString warning =
            tr("Running directly on Wayland is NOT SUPPORTED and is likely to crash, hang, or "
               "fail to render.");

        qInfo() << "------ !!!! WARNING !!!! ------";
        qInfo() << warning;
        qInfo() << "------ !!!! WARNING !!!! ------";

        RDDialog::critical(NULL, tr("Wayland Qt platform not supported"), warning);
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
      ANALYTIC_SET(Metadata.OSVersion, QSysInfo::prettyProductName());

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
        ctx.Begin(filename, remoteHost, remoteIdent, temp);

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
