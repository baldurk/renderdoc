/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QStandardPaths>
#include "Code/CaptureContext.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Code/pyrenderdoc/PythonContext.h"
#include "Windows/MainWindow.h"

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

int main(int argc, char *argv[])
{
  // call this as the very first thing - no-op on other platforms, but on linux it means
  // XInitThreads will be called allowing driver access to xlib on multiple threads.
  QCoreApplication::setAttribute(Qt::AA_X11InitThreads);

  qInstallMessageHandler(sharedLogOutput);

  qInfo() << "QRenderDoc initialising.";

  QString filename;
  bool temp = false;

  for(int i = 0; i < argc; i++)
  {
    if(!QString::compare(QString::fromUtf8(argv[i]), lit("--tempfile"), Qt::CaseInsensitive))
      temp = true;
  }

  for(int i = 0; i < argc; i++)
  {
    if(!QString::compare(QString::fromUtf8(argv[i]), lit("--install_vulkan_layer")) && i + 1 < argc)
    {
      if(!QString::compare(QString::fromUtf8(argv[i + 1]), lit("root")))
        RENDERDOC_UpdateVulkanLayerRegistration(true);
      else
        RENDERDOC_UpdateVulkanLayerRegistration(false);
      return 0;
    }
  }

  QString remoteHost;
  uint remoteIdent = 0;

  for(int i = 0; i + 1 < argc; i++)
  {
    if(!QString::compare(QString::fromUtf8(argv[i]), lit("--remoteaccess"), Qt::CaseInsensitive))
    {
      QRegularExpression regexp(lit("^([a-zA-Z0-9_-]+:)?([0-9]+)$"));

      QRegularExpressionMatch match = regexp.match(QString::fromUtf8(argv[i + 1]));

      if(match.hasMatch())
      {
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
      }
    }
  }

  QList<QString> pyscripts;

  for(int i = 0; i + 1 < argc; i++)
  {
    QString a = QString::fromUtf8(argv[i]);
    if(!QString::compare(a, lit("--python"), Qt::CaseInsensitive) ||
       !QString::compare(a, lit("--py"), Qt::CaseInsensitive) ||
       !QString::compare(a, lit("--script"), Qt::CaseInsensitive))
    {
      QString f = QString::fromUtf8(argv[i + 1]);
      QFileInfo checkFile(f);
      if(checkFile.exists() && checkFile.isFile())
      {
        pyscripts.push_back(f);
      }
    }
  }

  if(argc > 1)
  {
    filename = QString::fromUtf8(argv[argc - 1]);
    QFileInfo checkFile(filename);
    if(!checkFile.exists() || !checkFile.isFile() || checkFile.suffix().toLower() == lit("py"))
      filename = QString();
  }

  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  QApplication application(argc, argv);

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

    config.SetupFormatting();

    Resources::Initialise();

    GUIInvoke::init();

    PythonContext::GlobalInit();

    {
      GlobalEnvironment env;
#if defined(RENDERDOC_PLATFORM_LINUX)
      env.xlibDisplay = QX11Info::display();
#endif
      RENDERDOC_InitGlobalEnv(env, rdctype::array<rdctype::str>());
    }

    {
      CaptureContext ctx(filename, remoteHost, remoteIdent, temp, config);

      if(!pyscripts.isEmpty())
      {
        PythonContextHandle py;

        py.ctx().setGlobal("pyrenderdoc", (ICaptureContext *)&ctx);

        QObject::connect(&py.ctx(), &PythonContext::exception,
                         [](const QString &type, const QString &value, QList<QString> frames) {

                           QString exString;

                           if(!frames.isEmpty())
                           {
                             exString += QApplication::translate(
                                 "qrenderdoc", "Traceback (most recent call last):\n");
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
          qInfo() << "running" << f;
          py.ctx().executeFile(f);
        }
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
