/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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

  RENDERDOC_LogMessage(logtype, "QTRD", context.file, context.line, msg.toUtf8().data());
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

  qInfo() << "QRenderDoc initialising.";

  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  QApplication::setApplicationVersion(lit(FULL_VERSION_STRING));

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

#if !defined(RELEASE)
  QCommandLineOption unittests(lit("unittest"), tr("Run unit tests"));
  parser.addOption(unittests);
#endif

  parser.addPositionalArgument(lit("filename"), tr("The file to open."));

  bool parsedCommands = parser.parse(application.arguments());

  if(!parsedCommands)
  {
    QString error = parser.errorText();
    printf("%s\n", error.toUtf8().data());
    return 1;
  }

  if(parser.isSet(helpOption))
  {
    parser.showHelp();
    return 0;
  }

  if(parser.isSet(versionOption))
  {
    printf("QRenderDoc v%s (%s)\n", MAJOR_MINOR_VERSION_STRING, GitVersionHash);
#if defined(DISTRIBUTION_VERSION)
    printf("Packaged for %s - %s\n", DISTRIBUTION_NAME, DISTRIBUTION_CONTACT);
#endif
    return 0;
  }

#if !defined(RELEASE)
  if(parser.isSet(unittests))
  {
    PythonContext::GlobalInit();

    bool errors = false;

    qInfo() << "Checking python binding consistency.";

    {
      PythonContextHandle py;
      errors = py.ctx().CheckInterfaces();
    }

    if(errors)
    {
      qCritical() << "Found errors in python bindings. Please fix!";
      return 1;
    }

    qInfo() << "Python bindings are consistent.";
    return 0;
  }
#endif

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
    QRegularExpression regexp(lit("^([a-zA-Z0-9_-]+:)?([0-9]+)$"));

    QRegularExpressionMatch match = regexp.match(parser.value(targetcontrol));

    if(!match.hasMatch())
    {
      qCritical() << "--targetcontrol option must be followed by host:port";
      return 1;
    }

    QString host = match.captured(1);

    if(host.length() > 0 && host[host.length() - 1] == QLatin1Char(':'))
      host.chop(1);

    bool ok = false;
    uint32_t ident = match.captured(2).toUInt(&ok);

    if(ok)
    {
      remoteHost = host;
      remoteIdent = ident;
    }
    else
    {
      qCritical() << "--targetcontrol parameter" << match.captured(1) << "malformed";
      return 1;
    }
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

    QString configFilename = configFilePath(lit("UI.config"));

    if(!config.Load(configFilename))
    {
      RDDialog::critical(
          NULL, CaptureContext::tr("Error loading config"),
          CaptureContext::tr(
              "Error loading config file\n%1\nA default config is loaded and will be saved out.")
              .arg(configFilename));
    }

    if(!config.Analytics_TotalOptOut)
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
#endif
      rdcarray<rdcstr> coreargs;
      if(!crashReportPath.isEmpty())
        coreargs.push_back("--crash");
      for(const QString &arg : remaining)
        coreargs.push_back(arg);
      RENDERDOC_InitGlobalEnv(env, coreargs);
    }

    if(!crashReportPath.isEmpty())
    {
      QFile f(crashReportPath);

      if(f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text))
      {
        QVariantMap json = JSONToVariant(QString::fromUtf8(f.readAll()));

        if(json.contains(lit("report")))
        {
          CrashDialog dialog(config, json);

          RDDialog::show(&dialog);
        }
      }
    }
    else
    {
      PythonContext::GlobalInit();

      CaptureContext ctx(filename, remoteHost, remoteIdent, temp, config);

      Analytics::Prompt(ctx, config);

      ANALYTIC_SET(Environment.RenderDocVersion, lit(FULL_VERSION_STRING));
#if defined(DISTRIBUTION_VERSION)
      ANALYTIC_SET(Environment.DistributionVersion, lit(DISTRIBUTION_NAME));
#endif
      ANALYTIC_SET(Environment.Bitness, ((sizeof(void *) == sizeof(uint64_t)) ? 64 : 32));
      ANALYTIC_SET(Environment.OSVersion, QSysInfo::prettyProductName());

#if RENDERDOC_STABLE_BUILD
      ANALYTIC_SET(Environment.OfficialBuildRun, true);
#else
      ANALYTIC_SET(Environment.DevelBuildRun, true);
#endif

      ANALYTIC_SET(DaysUsed[QDateTime::currentDateTime().date().day()], true);

      if(!pyscripts.isEmpty())
      {
        PythonContextHandle py;

        ANALYTIC_SET(UIFeatures.PythonInterop, true);

        py.ctx().setGlobal("pyrenderdoc", (ICaptureContext *)&ctx);

        QObject::connect(&py.ctx(), &PythonContext::exception,
                         [](const QString &type, const QString &value, int, QList<QString> frames) {

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
        }
      }

      if(updateApplied)
      {
        config.CheckUpdate_UpdateAvailable = false;
        config.CheckUpdate_UpdateResponse = "";
        config.Save();
      }

      while(ctx.isRunning())
      {
        application.processEvents(QEventLoop::WaitForMoreEvents);
        QCoreApplication::sendPostedEvents();
        QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
      }

      config.Save();
    }

    PythonContext::GlobalShutdown();

    Formatter::shutdown();
  }

  return 0;
}
