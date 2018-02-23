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

#include "MainWindow.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmapCache>
#include <QProgressBar>
#include <QProgressDialog>
#include <QToolButton>
#include <QToolTip>
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Resources/resource.h"
#include "Widgets/Extended/RDLabel.h"
#include "Windows/Dialogs/AboutDialog.h"
#include "Windows/Dialogs/CaptureDialog.h"
#include "Windows/Dialogs/CrashDialog.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/Dialogs/RemoteManager.h"
#include "Windows/Dialogs/SettingsDialog.h"
#include "Windows/Dialogs/SuggestRemoteDialog.h"
#include "Windows/Dialogs/TipsDialog.h"
#include "Windows/Dialogs/UpdateDialog.h"
#include "ui_MainWindow.h"
#include "version.h"

#define JSON_ID "rdocLayoutData"
#define JSON_VER 1

#if defined(Q_OS_WIN32)
extern "C" void *__stdcall GetModuleHandleA(const char *);
#endif

MainWindow::MainWindow(ICaptureContext &ctx) : QMainWindow(NULL), ui(new Ui::MainWindow), m_Ctx(ctx)
{
  ui->setupUi(this);

  setProperty("ICaptureContext", QVariant::fromValue((void *)&ctx));

#if !defined(Q_OS_WIN32)
  // process injection is not supported on non-Windows, so remove the menu item rather than disable
  // it without a clear way to communicate that it is never supported
  ui->menu_File->removeAction(ui->action_Inject_into_Process);
#endif

  QToolTip::setPalette(palette());

  qApp->installEventFilter(this);

  setAcceptDrops(true);

  QObject::connect(ui->action_Load_Default_Layout, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_1, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_2, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_3, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_4, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_5, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);
  QObject::connect(ui->action_Load_Layout_6, &QAction::triggered, this,
                   &MainWindow::loadLayout_triggered);

  QObject::connect(ui->action_Save_Default_Layout, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_1, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_2, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_3, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_4, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_5, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);
  QObject::connect(ui->action_Save_Layout_6, &QAction::triggered, this,
                   &MainWindow::saveLayout_triggered);

  QObject::connect(ui->action_Launch_Application_Window, &QAction::triggered, this,
                   &MainWindow::on_action_Launch_Application_triggered);

  QObject::connect(ui->action_Clear_Capture_Files_History, &QAction::triggered, this,
                   &MainWindow::ClearRecentCaptureFiles);
  QObject::connect(ui->action_Clear_Capture_Settings_History, &QAction::triggered, this,
                   &MainWindow::ClearRecentCaptureSettings);

  contextChooserMenu = new QMenu(this);

  FillRemotesMenu(contextChooserMenu, true);

  contextChooser = new QToolButton(this);
  contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
  contextChooser->setIcon(Icons::house());
  contextChooser->setAutoRaise(true);
  contextChooser->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  contextChooser->setPopupMode(QToolButton::InstantPopup);
  contextChooser->setMenu(contextChooserMenu);
  contextChooser->setContextMenuPolicy(Qt::DefaultContextMenu);
  QObject::connect(contextChooserMenu, &QMenu::aboutToShow, this,
                   &MainWindow::contextChooser_menuShowing);

  ui->statusBar->addWidget(contextChooser);

  statusIcon = new RDLabel(this);
  ui->statusBar->addWidget(statusIcon);

  statusText = new RDLabel(this);
  ui->statusBar->addWidget(statusText);

  statusProgress = new QProgressBar(this);
  ui->statusBar->addWidget(statusProgress);

  statusProgress->setVisible(false);
  statusProgress->setMinimumSize(QSize(200, 0));
  statusProgress->setMinimum(0);
  statusProgress->setMaximum(1000);

  statusIcon->setText(QString());
  statusIcon->setPixmap(QPixmap());
  statusText->setText(QString());

  QObject::connect(statusIcon, &RDLabel::doubleClicked, this, &MainWindow::statusDoubleClicked);
  QObject::connect(statusText, &RDLabel::doubleClicked, this, &MainWindow::statusDoubleClicked);

  QObject::connect(&m_MessageTick, &QTimer::timeout, this, &MainWindow::messageCheck);
  m_MessageTick.setSingleShot(false);
  m_MessageTick.setInterval(175);
  m_MessageTick.start();

  m_RemoteProbeSemaphore.release();
  m_RemoteProbe = new LambdaThread([this]() {
    while(m_RemoteProbeSemaphore.available())
    {
      // do several small sleeps so we can respond quicker when we need to shut down
      for(int i = 0; i < 50; i++)
      {
        QThread::msleep(150);
        if(!m_RemoteProbeSemaphore.available())
          return;
      }
      remoteProbe();
    }
  });
  m_RemoteProbe->start();

  SetTitle();

#if defined(RELEASE)
  ui->action_Send_Error_Report->setEnabled(true);
#else
  ui->action_Send_Error_Report->setEnabled(false);
#endif

  m_NetManager = new QNetworkAccessManager(this);

  updateAction = new QAction(this);
  updateAction->setText(tr("Update Available!"));
  updateAction->setIcon(Icons::update());

  QObject::connect(updateAction, &QAction::triggered, this, &MainWindow::updateAvailable_triggered);

#if !defined(Q_OS_WIN32)
  // update checks only happen on windows
  {
    QList<QAction *> actions = ui->menu_Help->actions();
    int idx = actions.indexOf(ui->action_Check_for_Updates);
    idx++;
    if(idx < actions.count() && actions[idx]->isSeparator())
      delete actions[idx];

    delete ui->action_Check_for_Updates;
    ui->action_Check_for_Updates = NULL;

    delete updateAction;
    updateAction = NULL;
  }
#endif

  if(updateAction)
  {
    ui->menuBar->addAction(updateAction);
    updateAction->setVisible(false);
  }

  PopulateRecentCaptureFiles();
  PopulateRecentCaptureSettings();
  PopulateReportedBugs();

  CheckUpdates();

  rdcarray<BugReport> bugs = m_Ctx.Config().CrashReport_ReportedBugs;
  LambdaThread *bugupdate = new LambdaThread([this, bugs]() {
    QDateTime now = QDateTime::currentDateTimeUtc();

    // loop over all the bugs
    for(const BugReport &b : bugs)
    {
      // check bugs every two days
      qint64 diff = QDateTime(b.checkDate).secsTo(now);
      if(diff > 2 * 24 * 60 * 60)
      {
        // update the check date on the stored bug
        GUIInvoke::call([this, b, now]() {
          for(BugReport &bug : m_Ctx.Config().CrashReport_ReportedBugs)
          {
            if(bug.reportId == b.reportId)
            {
              bug.checkDate = now;
              break;
            }
          }
          m_Ctx.Config().Save();

          // call out to the status-check to see when the bug report was last updated
          QNetworkReply *reply =
              m_NetManager->get(QNetworkRequest(QUrl(QString(b.URL()) + lit("/check"))));

          QObject::connect(reply, &QNetworkReply::finished, [this, reply, b]() {
            QString response = QString::fromUtf8(reply->readAll());

            if(response.isEmpty())
              return;

            // only look at the first line of the response
            int idx = response.indexOf(QLatin1Char('\n'));

            if(idx > 0)
              response.truncate(idx);

            QDateTime update = QDateTime::fromString(response, lit("yyyy-MM-dd HH:mm:ss"));

            // if there's been an update since the last check, set unread
            if(update.isValid() && update > b.checkDate)
            {
              for(BugReport &bug : m_Ctx.Config().CrashReport_ReportedBugs)
              {
                if(bug.reportId == b.reportId)
                {
                  bug.unreadUpdates = true;
                  break;
                }
              }
              PopulateReportedBugs();
            }
          });
        });
      }
    }
  });
  bugupdate->selfDelete(true);
  bugupdate->start();

  ui->toolWindowManager->setToolWindowCreateCallback([this](const QString &objectName) -> QWidget * {
    return m_Ctx.CreateBuiltinWindow(objectName);
  });

  ui->action_Start_Replay_Loop->setEnabled(false);
  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

  ui->action_Recompress_Capture->setEnabled(false);

  LambdaThread *th = new LambdaThread([this]() {
    m_Ctx.Config().AddAndroidHosts();
    for(RemoteHost *host : m_Ctx.Config().RemoteHosts)
      host->CheckStatus();
  });
  th->selfDelete(true);
  th->start();

#if defined(Q_OS_WIN32)
  if(GetModuleHandleA("rdocself.dll"))
  {
    QAction *begin = new QAction(tr("Start Self-hosted Capture"), this);
    QAction *end = new QAction(tr("End Self-hosted Capture"), this);
    end->setEnabled(false);

    QObject::connect(begin, &QAction::triggered, [begin, end]() {
      begin->setEnabled(false);
      end->setEnabled(true);

      RENDERDOC_StartSelfHostCapture("rdocself.dll");
    });

    QObject::connect(end, &QAction::triggered, [begin, end]() {
      begin->setEnabled(true);
      end->setEnabled(false);

      RENDERDOC_EndSelfHostCapture("rdocself.dll");
    });

    ui->menu_Tools->addSeparator();
    ui->menu_Tools->addAction(begin);
    ui->menu_Tools->addAction(end);
  }
#endif

  m_Ctx.AddCaptureViewer(this);

  ui->action_Save_Capture_Inplace->setEnabled(false);
  ui->action_Save_Capture_As->setEnabled(false);
  ui->action_Close_Capture->setEnabled(false);
  ui->menu_Export_As->setEnabled(false);

  {
    ICaptureFile *tmp = RENDERDOC_OpenCaptureFile();
    rdcarray<CaptureFileFormat> formats = tmp->GetCaptureFileFormats();

    for(const CaptureFileFormat &fmt : formats)
    {
      if(fmt.extension == "rdc")
        continue;

      if(fmt.openSupported)
      {
        QAction *action = new QAction(fmt.name, this);

        QObject::connect(action, &QAction::triggered, [this, fmt]() { importCapture(fmt); });

        if(!fmt.description.isEmpty())
          action->setToolTip(fmt.description);

        ui->menu_Import_From->addAction(action);
      }

      if(fmt.convertSupported)
      {
        QAction *action = new QAction(fmt.name, this);

        QObject::connect(action, &QAction::triggered, [this, fmt]() { exportCapture(fmt); });

        if(!fmt.description.isEmpty())
          action->setToolTip(fmt.description);

        ui->menu_Export_As->addAction(action);
      }
    }

    tmp->Shutdown();
  }

  QList<QAction *> actions = ui->menuBar->actions();

  // register all the UI-designer created shortcut keys
  for(int i = 0; i < actions.count(); i++)
  {
    QAction *a = actions[i];

    QKeySequence ks = a->shortcut();
    if(!ks.isEmpty())
    {
      m_GlobalShortcutCallbacks[ks] = [a]() {
        if(a->isEnabled())
          a->trigger();
      };
    }

    // recurse into submenus by appending to the end of the list.
    if(a->menu())
      actions.append(a->menu()->actions());
  }
}

MainWindow::~MainWindow()
{
  // explicitly delete our children here, so that the MainWindow is still alive while they are
  // closing.

  setUpdatesEnabled(false);
  qDeleteAll(findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));

  m_RemoteProbeSemaphore.acquire();
  m_RemoteProbe->wait();
  m_RemoteProbe->deleteLater();
  delete ui;
}

QString MainWindow::GetLayoutPath(int layout)
{
  QString filename = lit("DefaultLayout.config");

  if(layout > 0)
    filename = lit("Layout%1.config").arg(layout);

  return configFilePath(filename);
}

void MainWindow::on_action_Exit_triggered()
{
  this->close();
}

void MainWindow::on_action_Open_Capture_triggered()
{
  if(!PromptCloseCapture())
    return;

  QString filename = RDDialog::getOpenFileName(
      this, tr("Select file to open"), m_Ctx.Config().LastCaptureFilePath,
      tr("Capture Files (*.rdc);;Image Files (*.dds *.hdr *.exr *.bmp *.jpg "
         "*.jpeg *.png *.tga *.gif *.psd;;All Files (*)"));

  if(!filename.isEmpty())
    LoadFromFilename(filename, false);
}

void MainWindow::importCapture(const CaptureFileFormat &fmt)
{
  if(!PromptCloseCapture())
    return;

  QString ext = fmt.extension;
  QString title = fmt.name;

  QString filename =
      RDDialog::getOpenFileName(this, tr("Select file to open"), m_Ctx.Config().LastCaptureFilePath,
                                tr("%1 Files (*.%2);;All Files (*)").arg(title).arg(ext));

  if(!filename.isEmpty())
  {
    QString rdcfile = m_Ctx.TempCaptureFilename(lit("imported_") + ext);

    bool success = m_Ctx.ImportCapture(fmt, filename, rdcfile);

    if(success)
    {
      // open file as temporary, in case the user wants to save the imported rdc
      LoadFromFilename(rdcfile, true);
      takeCaptureOwnership();
    }
  }
}

void MainWindow::captureModified()
{
  // once the capture is modified, enable the save-in-place option. It might already have been
  // enabled if this capture was a temporary one
  if(m_Ctx.IsCaptureLoaded())
    ui->action_Save_Capture_Inplace->setEnabled(true);
}

void MainWindow::LoadFromFilename(const QString &filename, bool temporary)
{
  QFileInfo path(filename);
  QString ext = path.suffix().toLower();

  if(ext == lit("rdc"))
  {
    LoadCapture(filename, temporary, true);
  }
  else if(ext == lit("cap"))
  {
    OpenCaptureConfigFile(filename, false);
  }
  else if(ext == lit("exe"))
  {
    OpenCaptureConfigFile(filename, true);
  }
  else
  {
    // not a recognised filetype, see if we can load it anyway
    LoadCapture(filename, temporary, true);
  }
}

void MainWindow::OnCaptureTrigger(const QString &exe, const QString &workingDir,
                                  const QString &cmdLine,
                                  const rdcarray<EnvironmentModification> &env, CaptureOptions opts,
                                  std::function<void(LiveCapture *)> callback)
{
  if(!PromptCloseCapture())
    return;

  LambdaThread *th = new LambdaThread([this, exe, workingDir, cmdLine, env, opts, callback]() {
    QString capturefile = m_Ctx.TempCaptureFilename(QFileInfo(exe).baseName());

    ExecuteResult ret =
        m_Ctx.Replay().ExecuteAndInject(exe, workingDir, cmdLine, env, capturefile, opts);

    GUIInvoke::call([this, exe, ret, callback]() {

      if(ret.status == ReplayStatus::JDWPFailure)
      {
        RDDialog::critical(
            this, tr("Error connecting to debugger"),
            tr("Error launching %1 for capture.\n\n"
               "Something went wrong connecting to the debugger on the Android device.\n\n"
               "This can happen if the package is not "
               "marked as debuggable, or if another android tool such as android studio "
               "or other tool is interfering with the debug connection.")
                .arg(exe));
        return;
      }

      if(ret.status != ReplayStatus::Succeeded)
      {
        RDDialog::critical(this, tr("Error kicking capture"),
                           tr("Error launching %1 for capture.\n\n%2. Check the diagnostic log in "
                              "the help menu for more details")
                               .arg(exe)
                               .arg(ToQStr(ret.status)));
        return;
      }

      LiveCapture *live = new LiveCapture(
          m_Ctx, m_Ctx.Replay().CurrentRemote() ? m_Ctx.Replay().CurrentRemote()->hostname : "",
          m_Ctx.Replay().CurrentRemote() ? m_Ctx.Replay().CurrentRemote()->Name() : "", ret.ident,
          this, this);
      ShowLiveCapture(live);
      callback(live);
    });
  });
  th->start();
  // wait a few ms before popping up a progress bar
  th->wait(500);
  if(th->isRunning())
  {
    QString filename = QFileInfo(exe).fileName();
    ShowProgressDialog(this, tr("Launching %1, please wait...").arg(filename),
                       [th]() { return !th->isRunning(); });
  }
  th->deleteLater();
}

void MainWindow::OnInjectTrigger(uint32_t PID, const rdcarray<EnvironmentModification> &env,
                                 const QString &name, CaptureOptions opts,
                                 std::function<void(LiveCapture *)> callback)
{
  if(!PromptCloseCapture())
    return;

  LambdaThread *th = new LambdaThread([this, PID, env, name, opts, callback]() {
    QString capturefile = m_Ctx.TempCaptureFilename(name);

    ExecuteResult ret =
        RENDERDOC_InjectIntoProcess(PID, env, capturefile.toUtf8().data(), opts, false);

    GUIInvoke::call([this, PID, ret, callback]() {

      if(ret.status == ReplayStatus::JDWPFailure)
      {
        RDDialog::critical(
            this, tr("Error connecting to debugger"),
            tr("Error injecting into process %1 for capture.\n\n"
               "Something went wrong connecting to the debugger on the Android device.\n\n"
               "This can happen if the package is not "
               "marked as debuggable, or if another android tool such as android studio "
               "or other tool is interfering with the debug connection.")
                .arg(PID));
        return;
      }

      if(ret.status != ReplayStatus::Succeeded)
      {
        RDDialog::critical(
            this, tr("Error kicking capture"),
            tr("Error injecting into process %1 for capture.\n\n%2. Check the diagnostic log in "
               "the help menu for more details")
                .arg(PID)
                .arg(ToQStr(ret.status)));
        return;
      }

      LiveCapture *live = new LiveCapture(m_Ctx, QString(), QString(), ret.ident, this, this);
      ShowLiveCapture(live);
    });
  });
  th->start();
  // wait a few ms before popping up a progress bar
  th->wait(500);
  if(th->isRunning())
  {
    ShowProgressDialog(this, tr("Injecting into %1, please wait...").arg(PID),
                       [th]() { return !th->isRunning(); });
  }
  th->deleteLater();
}

void MainWindow::LoadCapture(const QString &filename, bool temporary, bool local)
{
  if(PromptCloseCapture())
  {
    if(m_Ctx.IsCaptureLoading())
      return;

    QString driver;
    QString machineIdent;
    ReplaySupport support = ReplaySupport::Unsupported;

    bool remoteReplay =
        !local || (m_Ctx.Replay().CurrentRemote() && m_Ctx.Replay().CurrentRemote()->connected);

    if(local)
    {
      ICaptureFile *file = RENDERDOC_OpenCaptureFile();

      ReplayStatus status = file->OpenFile(filename.toUtf8().data(), "rdc", NULL);

      if(status != ReplayStatus::Succeeded)
      {
        QString text = tr("Couldn't open file '%1'\n").arg(filename);
        QString message = file->ErrorString();
        if(message.isEmpty())
          text += tr("%1").arg(ToQStr(status));
        else
          text += tr("%1: %2").arg(ToQStr(status)).arg(message);

        RDDialog::critical(this, tr("Error opening capture"), text);

        file->Shutdown();
        return;
      }

      driver = QString::fromUtf8(file->DriverName());
      machineIdent = QString::fromUtf8(file->RecordedMachineIdent());
      support = file->LocalReplaySupport();

      file->Shutdown();

      // if the return value suggests remote replay, and it's not already selected, AND the user
      // hasn't previously chosen to always replay locally without being prompted, ask if they'd
      // prefer to switch to a remote context for replaying.
      if(support == ReplaySupport::SuggestRemote && !remoteReplay &&
         !m_Ctx.Config().AlwaysReplayLocally)
      {
        SuggestRemoteDialog dialog(driver, machineIdent, this);

        FillRemotesMenu(dialog.remotesMenu(), false);

        dialog.remotesAdded();

        RDDialog::show(&dialog);

        if(dialog.choice() == SuggestRemoteDialog::Cancel)
        {
          return;
        }
        else if(dialog.choice() == SuggestRemoteDialog::Remote)
        {
          // we only get back here from the dialog once the context switch has begun,
          // so contextChooser will have been disabled.
          // Check once to see if it's enabled before even popping up the dialog in case
          // it has finished already. Otherwise pop up a waiting dialog until it completes
          // one way or another, then process the result.

          if(!contextChooser->isEnabled())
          {
            ShowProgressDialog(this, tr("Please Wait - Checking remote connection..."),
                               [this]() { return contextChooser->isEnabled(); });
          }

          remoteReplay =
              (m_Ctx.Replay().CurrentRemote() && m_Ctx.Replay().CurrentRemote()->connected);

          if(!remoteReplay)
          {
            QString remoteMessage = tr("Failed to make a connection to the remote server.\n\n");

            remoteMessage += tr("More information may be available in the status bar.");

            RDDialog::information(this, tr("Couldn't connect to remote server"), remoteMessage);
            return;
          }
        }
        else
        {
          // nothing to do - we just continue replaying locally
          // however we need to check if the user selected 'always replay locally' and
          // set that bit as sticky in the config
          if(dialog.alwaysReplayLocally())
          {
            m_Ctx.Config().AlwaysReplayLocally = true;

            m_Ctx.Config().Save();
          }
        }
      }

      if(remoteReplay)
      {
        support = ReplaySupport::Unsupported;

        rdcarray<rdcstr> remoteDrivers = m_Ctx.Replay().GetRemoteSupport();

        for(const rdcstr &d : remoteDrivers)
        {
          if(driver == QString(d))
            support = ReplaySupport::Supported;
        }
      }
    }

    QString origFilename = filename;

    // if driver is empty something went wrong loading the capture, let it be handled as usual
    // below. Otherwise indicate that support is missing.
    if(!driver.isEmpty() && support == ReplaySupport::Unsupported)
    {
      if(remoteReplay)
      {
        QString remoteMessage =
            tr("This capture was captured with %1 and cannot be replayed on %2.\n\n")
                .arg(driver)
                .arg(m_Ctx.Replay().CurrentRemote()->Name());

        remoteMessage += tr("Try selecting a different remote context in the status bar.");

        RDDialog::critical(this, tr("Unsupported capture driver"), remoteMessage);
      }
      else
      {
        QString remoteMessage =
            tr("This capture was captured with %1 and cannot be replayed locally.\n\n").arg(driver);

        remoteMessage += tr("Try selecting a remote context in the status bar.");

        RDDialog::critical(this, tr("Unsupported capture driver"), remoteMessage);
      }

      return;
    }
    else
    {
      QString fileToLoad = filename;

      if(remoteReplay && local)
      {
        fileToLoad = m_Ctx.Replay().CopyCaptureToRemote(filename, this);

        // deliberately leave local as true so that we keep referring to the locally saved capture

        // some error
        if(fileToLoad.isEmpty())
        {
          RDDialog::critical(this, tr("Error copying to remote"),
                             tr("Couldn't copy %1 to remote host for replaying").arg(filename));
          return;
        }
      }

      statusText->setText(tr("Loading %1...").arg(origFilename));

      if(driver == lit("Image"))
      {
        ANALYTIC_SET(UIFeatures.ImageViewer, true);
      }
      else
      {
        ANALYTIC_ADDUNIQ(APIs, driver);
      }

      m_Ctx.LoadCapture(fileToLoad, origFilename, temporary, local);
    }

    if(!remoteReplay)
    {
      m_Ctx.Config().LastCaptureFilePath = QFileInfo(filename).absolutePath();
    }
  }
}

void MainWindow::OpenCaptureConfigFile(const QString &filename, bool exe)
{
  ICaptureDialog *capDialog = m_Ctx.GetCaptureDialog();

  if(exe)
    capDialog->SetExecutableFilename(filename);
  else
    capDialog->LoadSettings(filename);

  if(!ui->toolWindowManager->toolWindows().contains(capDialog->Widget()))
    ui->toolWindowManager->addToolWindow(capDialog->Widget(), mainToolArea());

  ToolWindowManager::raiseToolWindow(capDialog->Widget());
}

QString MainWindow::GetSavePath(QString title, QString filter)
{
  QString dir;

  if(!m_Ctx.Config().DefaultCaptureSaveDirectory.isEmpty())
  {
    if(m_LastSaveCapturePath.isEmpty())
      dir = m_Ctx.Config().DefaultCaptureSaveDirectory;
    else
      dir = m_LastSaveCapturePath;
  }

  if(title.isEmpty())
    title = tr("Save Capture As");

  if(filter.isEmpty())
    filter = tr("Capture Files (*.rdc)");

  QString filename = RDDialog::getSaveFileName(this, title, dir, filter);

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
      m_LastSaveCapturePath = dirinfo.absolutePath();

    return filename;
  }

  return QString();
}

bool MainWindow::PromptSaveCaptureAs()
{
  QString saveFilename = GetSavePath();

  if(!saveFilename.isEmpty())
  {
    bool success = m_Ctx.SaveCaptureTo(saveFilename);

    if(!success)
      return false;

    AddRecentFile(m_Ctx.Config().RecentCaptureFiles, saveFilename, 10);
    PopulateRecentCaptureFiles();
    SetTitle(saveFilename);

    return true;
  }

  return false;
}

void MainWindow::exportCapture(const CaptureFileFormat &fmt)
{
  if(!m_Ctx.IsCaptureLocal())
  {
    RDDialog::information(
        this, tr("Save changes to capture?"),
        tr("The capture is on a remote host, it must be saved locally before it can be exported."));
    PromptSaveCaptureAs();
    return;
  }

  QString saveFilename =
      GetSavePath(tr("Export Capture As"),
                  tr("%1 Files (*.%2)").arg(QString(fmt.name)).arg(QString(fmt.extension)));

  if(!saveFilename.isEmpty())
    m_Ctx.ExportCapture(fmt, saveFilename);
}

bool MainWindow::PromptCloseCapture()
{
  if(!m_Ctx.IsCaptureLoaded())
    return true;

  QString deletepath;
  bool caplocal = false;

  if(m_OwnTempCapture && m_Ctx.IsCaptureTemporary())
  {
    QString temppath = m_Ctx.GetCaptureFilename();
    caplocal = m_Ctx.IsCaptureLocal();

    QMessageBox::StandardButton res =
        RDDialog::question(this, tr("Unsaved capture"), tr("Save this capture?"),
                           QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if(res == QMessageBox::Cancel)
      return false;

    if(res == QMessageBox::Yes)
    {
      bool success = PromptSaveCaptureAs();

      if(!success)
      {
        return false;
      }
    }

    if(temppath != m_Ctx.GetCaptureFilename() || res == QMessageBox::No)
      deletepath = temppath;
    m_OwnTempCapture = false;
  }
  else if(m_Ctx.GetCaptureModifications() != CaptureModifications::NoModifications)
  {
    QMessageBox::StandardButton res = QMessageBox::No;

    QString text = tr("This capture has the following modifications:\n\n");

    CaptureModifications mods = m_Ctx.GetCaptureModifications();

    if(mods & CaptureModifications::Renames)
      text += tr("Resources have been renamed.\n");
    if(mods & CaptureModifications::Bookmarks)
      text += tr("Bookmarks have been changed.\n");
    if(mods & CaptureModifications::Notes)
      text += tr("Capture notes have been changed.\n");

    bool saveas = false;

    if(m_Ctx.IsCaptureLocal())
    {
      text += tr("\nWould you like to save those changes to '%1'?").arg(m_Ctx.GetCaptureFilename());
    }
    else
    {
      saveas = true;
      text +=
          tr("\nThe capture is on a remote host, would you like to save these changes locally?");
    }

    res = RDDialog::question(this, tr("Save changes to capture?"), text,
                             QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if(res == QMessageBox::Cancel)
      return false;

    if(res == QMessageBox::Yes)
    {
      bool success = false;

      if(saveas)
        success = PromptSaveCaptureAs();
      else
        success = m_Ctx.SaveCaptureTo(m_Ctx.GetCaptureFilename());

      if(!success)
        return false;
    }
  }

  CloseCapture();

  if(!deletepath.isEmpty())
    m_Ctx.Replay().DeleteCapture(deletepath, caplocal);

  return true;
}

void MainWindow::CloseCapture()
{
  m_Ctx.CloseCapture();

  ui->action_Save_Capture_Inplace->setEnabled(false);
  ui->action_Save_Capture_As->setEnabled(false);
  ui->menu_Export_As->setEnabled(false);
}

void MainWindow::SetTitle(const QString &filename)
{
  QString prefix;

  if(m_Ctx.IsCaptureLoaded())
  {
    prefix = QFileInfo(filename).fileName();
    if(m_Ctx.APIProps().degraded)
      prefix += tr(" !DEGRADED PERFORMANCE!");
    prefix += lit(" - ");
  }

  if(m_Ctx.Replay().CurrentRemote())
    prefix += tr("Remote: %1 - ").arg(m_Ctx.Replay().CurrentRemote()->Name());

  QString text = prefix + lit("RenderDoc ");

  if(RENDERDOC_STABLE_BUILD)
    text += lit(FULL_VERSION_STRING);
  else
    text += tr("Unstable release (%1 - %2)")
                .arg(lit(FULL_VERSION_STRING))
                .arg(QString::fromLatin1(GitVersionHash));

  if(QString::fromLatin1(RENDERDOC_GetVersionString()) != lit(MAJOR_MINOR_VERSION_STRING))
    text += tr(" - !! VERSION MISMATCH DETECTED !!");

  setWindowTitle(text);
}

void MainWindow::SetTitle()
{
  SetTitle(m_Ctx.GetCaptureFilename());
}

bool MainWindow::HandleMismatchedVersions()
{
  if(IsVersionMismatched())
  {
    qCritical() << "Version mismatch between UI (" << lit(MAJOR_MINOR_VERSION_STRING) << ")"
                << "and core"
                << "(" << QString::fromUtf8(RENDERDOC_GetVersionString()) << ")";

#if !RENDERDOC_OFFICIAL_BUILD
    RDDialog::critical(
        this, tr("Unofficial build - mismatched versions"),
        tr("You are running an unofficial build with mismatched core and UI versions.\n"
           "Double check where you got your build from and do a sanity check!"));
#else
    QMessageBox::StandardButton res = RDDialog::critical(
        this, tr("Mismatched versions"),
        tr("RenderDoc has detected mismatched versions between its internal module and UI.\n"
           "This is likely caused by a buggy update in the past which partially updated your "
           "install."
           "Likely because a program was running with renderdoc while the update happened.\n"
           "You should reinstall RenderDoc immediately as this configuration is almost guaranteed "
           "to crash.\n\n"
           "Would you like to open the downloads page to reinstall?"),
        QMessageBox::Yes | QMessageBox::No);

    if(res == QMessageBox::Yes)
      QDesktopServices::openUrl(QUrl(lit("https://renderdoc.org/builds")));

    SetUpdateAvailable();
#endif
    return true;
  }

  return false;
}

bool MainWindow::IsVersionMismatched()
{
  return QString::fromLatin1(RENDERDOC_GetVersionString()) != lit(MAJOR_MINOR_VERSION_STRING);
}

void MainWindow::ClearRecentCaptureFiles()
{
  m_Ctx.Config().RecentCaptureFiles.clear();
  PopulateRecentCaptureFiles();
}

void MainWindow::PopulateRecentCaptureFiles()
{
  ui->menu_Recent_Capture_Files->clear();

  ui->menu_Recent_Capture_Files->setEnabled(false);

  int idx = 1;
  for(int i = m_Ctx.Config().RecentCaptureFiles.count() - 1; i >= 0; i--)
  {
    const QString &filename = m_Ctx.Config().RecentCaptureFiles[i];
    ui->menu_Recent_Capture_Files->addAction(QFormatStr("&%1 %2").arg(idx).arg(filename),
                                             [this, filename] { recentCaptureFile(filename); });
    idx++;

    ui->menu_Recent_Capture_Files->setEnabled(true);
  }

  ui->menu_Recent_Capture_Files->addSeparator();
  ui->menu_Recent_Capture_Files->addAction(ui->action_Clear_Capture_Files_History);
}

void MainWindow::ClearRecentCaptureSettings()
{
  m_Ctx.Config().RecentCaptureSettings.clear();
  PopulateRecentCaptureSettings();
}

void MainWindow::PopulateRecentCaptureSettings()
{
  ui->menu_Recent_Capture_Settings->clear();

  ui->menu_Recent_Capture_Settings->setEnabled(false);

  int idx = 1;
  for(int i = m_Ctx.Config().RecentCaptureSettings.count() - 1; i >= 0; i--)
  {
    const QString &filename = m_Ctx.Config().RecentCaptureSettings[i];
    ui->menu_Recent_Capture_Settings->addAction(QFormatStr("&%1 %2").arg(idx).arg(filename),
                                                [this, filename] { recentCaptureSetting(filename); });
    idx++;

    ui->menu_Recent_Capture_Settings->setEnabled(true);
  }

  ui->menu_Recent_Capture_Settings->addSeparator();
  ui->menu_Recent_Capture_Settings->addAction(ui->action_Clear_Capture_Settings_History);
}

void MainWindow::PopulateReportedBugs()
{
  ui->menu_Reported_Bugs->clear();

  ui->menu_Reported_Bugs->setEnabled(false);

  bool unread = false;

  int idx = 1;
  for(int i = m_Ctx.Config().CrashReport_ReportedBugs.count() - 1; i >= 0; i--)
  {
    BugReport &bug = m_Ctx.Config().CrashReport_ReportedBugs[i];
    QString fmt = tr("&%1: Bug reported at %2");

    if(bug.unreadUpdates)
      fmt = tr("&%1: (Update) Bug reported at %2");

    QAction *action = ui->menu_Reported_Bugs->addAction(
        fmt.arg(idx).arg(QDateTime(bug.submitDate).toString()), [this, i] {
          BugReport &bug = m_Ctx.Config().CrashReport_ReportedBugs[i];

          QDesktopServices::openUrl(QString(bug.URL()));

          bug.unreadUpdates = false;
          m_Ctx.Config().Save();

          PopulateReportedBugs();
        });
    idx++;

    if(bug.unreadUpdates)
    {
      action->setIcon(Icons::bug());
      unread = true;
    }

    ui->menu_Reported_Bugs->setEnabled(true);
  }

  ui->menu_Reported_Bugs->addSeparator();
  ui->menu_Reported_Bugs->addAction(ui->action_Clear_Reported_Bugs);

  if(unread)
  {
    if(!m_Ctx.Config().CheckUpdate_UpdateAvailable)
      ui->menu_Help->setIcon(Icons::bug());
    ui->menu_Reported_Bugs->setIcon(Icons::bug());
  }
  else
  {
    if(!m_Ctx.Config().CheckUpdate_UpdateAvailable)
      ui->menu_Help->setIcon(QIcon());
    ui->menu_Reported_Bugs->setIcon(QIcon());
  }
}

void MainWindow::CheckUpdates(bool forceCheck, UpdateResultMethod callback)
{
  if(!updateAction)
    return;

  bool mismatch = HandleMismatchedVersions();
  if(mismatch)
    return;

  if(!forceCheck && !m_Ctx.Config().CheckUpdate_AllowChecks)
  {
    updateAction->setVisible(false);
    if(callback)
      callback(UpdateResult::Disabled);
    return;
  }

#if RENDERDOC_OFFICIAL_BUILD
  QDateTime today = QDateTime::currentDateTime();

  // check by default every 2 days
  QDateTime compare = today.addDays(-2);

  // if there's already an update available, go down to checking every week.
  if(m_Ctx.Config().CheckUpdate_UpdateAvailable)
    compare = today.addDays(-7);

  bool checkDue = compare.secsTo(m_Ctx.Config().CheckUpdate_LastUpdate) < 0;

  if(m_Ctx.Config().CheckUpdate_UpdateAvailable)
  {
    // Mark an update available
    SetUpdateAvailable();

    // If we don't have a proper update response, or we're overdue for a check, then do it again.
    // The reason for this is twofold: first, if someone has been delaying their updates for a long
    // time then there might be a newer update available that we should refresh to, so we should
    // find out and refresh the update status. The other reason is that when we get a positive
    // response from the server we force-display the popup which means the user will get reminded
    // every week or so that an update is pending.
    if(m_Ctx.Config().CheckUpdate_UpdateResponse.isEmpty() || checkDue)
    {
      forceCheck = true;
    }

    // If we're not forcing a recheck, we're done.
    if(!forceCheck)
      return;
  }

  if(!forceCheck && checkDue)
  {
    if(callback)
      callback(UpdateResult::Toosoon);
    return;
  }

  m_Ctx.Config().CheckUpdate_LastUpdate = today;
  m_Ctx.Config().Save();

#if QT_POINTER_SIZE == 4
  QString bitness = lit("32");
#else
  QString bitness = lit("64");
#endif
  QString versionCheck = lit(MAJOR_MINOR_VERSION_STRING);

  statusText->setText(tr("Checking for updates..."));

  statusProgress->setVisible(true);
  statusProgress->setMaximum(0);

  // call out to the status-check to see when the bug report was last updated
  QNetworkReply *req = m_NetManager->get(QNetworkRequest(QUrl(
      lit("https://renderdoc.org/getupdateurl/%1/%2?htmlnotes=1").arg(bitness).arg(versionCheck))));

  QObject::connect(req, OverloadedSlot<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
                   [this, req](QNetworkReply::NetworkError) {
                     qCritical() << "Network error:" << req->errorString();
                   });

  QObject::connect(req, &QNetworkReply::finished, [this, req, callback]() {
    QString response = QString::fromUtf8(req->readAll());

    statusText->setText(QString());
    statusProgress->setVisible(false);

    if(response.isEmpty())
    {
      m_Ctx.Config().CheckUpdate_UpdateAvailable = false;
      m_Ctx.Config().CheckUpdate_UpdateResponse = "";
      m_Ctx.Config().Save();
      SetNoUpdate();

      if(callback)
        callback(UpdateResult::Latest);

      return;
    }

    m_Ctx.Config().CheckUpdate_UpdateAvailable = true;
    m_Ctx.Config().CheckUpdate_UpdateResponse = response;
    m_Ctx.Config().Save();
    SetUpdateAvailable();
    UpdatePopup();
  });
#else    //! RENDERDOC_OFFICIAL_BUILD
  {
    if(callback)
      callback(UpdateResult::Unofficial);
    return;
  }
#endif
}

void MainWindow::SetUpdateAvailable()
{
  if(updateAction)
    updateAction->setVisible(true);
}

void MainWindow::SetNoUpdate()
{
  if(updateAction)
    updateAction->setVisible(false);
}

void MainWindow::UpdatePopup()
{
  if(!m_Ctx.Config().CheckUpdate_UpdateAvailable || !m_Ctx.Config().CheckUpdate_AllowChecks)
    return;

  UpdateDialog update((QString)m_Ctx.Config().CheckUpdate_UpdateResponse);
  RDDialog::show(&update);
}

void MainWindow::ShowLiveCapture(LiveCapture *live)
{
  m_LiveCaptures.push_back(live);

  m_Ctx.AddDockWindow(live, DockReference::MainToolArea, this);
}

void MainWindow::LiveCaptureClosed(LiveCapture *live)
{
  m_LiveCaptures.removeOne(live);
}

ToolWindowManager *MainWindow::mainToolManager()
{
  return ui->toolWindowManager;
}

ToolWindowManager::AreaReference MainWindow::mainToolArea()
{
  // bit of a hack. Maybe the ToolWindowManager should track this?
  // Try and identify where to add new windows, by searching a
  // priority list of other windows to use their area
  if(m_Ctx.HasTextureViewer() &&
     ui->toolWindowManager->toolWindows().contains(m_Ctx.GetTextureViewer()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::AddTo, ui->toolWindowManager->areaOf(m_Ctx.GetTextureViewer()->Widget()));
  else if(m_Ctx.HasPipelineViewer() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.GetPipelineViewer()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::AddTo, ui->toolWindowManager->areaOf(m_Ctx.GetPipelineViewer()->Widget()));
  else if(m_Ctx.HasMeshPreview() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.GetMeshPreview()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::AddTo, ui->toolWindowManager->areaOf(m_Ctx.GetMeshPreview()->Widget()));
  else if(m_Ctx.HasCaptureDialog() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.GetCaptureDialog()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::AddTo, ui->toolWindowManager->areaOf(m_Ctx.GetCaptureDialog()->Widget()));

  // if all else fails just add to the last place we placed something.
  return ToolWindowManager::AreaReference(ToolWindowManager::LastUsedArea);
}

ToolWindowManager::AreaReference MainWindow::leftToolArea()
{
  // see mainToolArea()
  if(m_Ctx.HasTextureViewer() &&
     ui->toolWindowManager->toolWindows().contains(m_Ctx.GetTextureViewer()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::LeftOf, ui->toolWindowManager->areaOf(m_Ctx.GetTextureViewer()->Widget()));
  else if(m_Ctx.HasPipelineViewer() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.GetPipelineViewer()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::LeftOf,
        ui->toolWindowManager->areaOf(m_Ctx.GetPipelineViewer()->Widget()));
  else if(m_Ctx.HasCaptureDialog() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.GetCaptureDialog()->Widget()))
    return ToolWindowManager::AreaReference(
        ToolWindowManager::LeftOf, ui->toolWindowManager->areaOf(m_Ctx.GetCaptureDialog()->Widget()));

  return ToolWindowManager::AreaReference(ToolWindowManager::LastUsedArea);
}

void MainWindow::show()
{
  bool loaded = LoadLayout(0);

  // create default layout if layout failed to load
  if(!loaded)
  {
    QWidget *eventBrowser = m_Ctx.GetEventBrowser()->Widget();

    ui->toolWindowManager->addToolWindow(eventBrowser, ToolWindowManager::EmptySpace);

    QWidget *textureViewer = m_Ctx.GetTextureViewer()->Widget();

    ui->toolWindowManager->addToolWindow(
        textureViewer,
        ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                         ui->toolWindowManager->areaOf(eventBrowser), 0.75f));

    QWidget *pipe = m_Ctx.GetPipelineViewer()->Widget();

    ui->toolWindowManager->addToolWindow(
        pipe, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                               ui->toolWindowManager->areaOf(textureViewer)));

    QWidget *mesh = m_Ctx.GetMeshPreview()->Widget();

    ui->toolWindowManager->addToolWindow(
        mesh, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                               ui->toolWindowManager->areaOf(textureViewer)));

    QWidget *capDialog = m_Ctx.GetCaptureDialog()->Widget();

    ui->toolWindowManager->addToolWindow(
        capDialog, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                    ui->toolWindowManager->areaOf(textureViewer)));

    QWidget *apiInspector = m_Ctx.GetAPIInspector()->Widget();

    ui->toolWindowManager->addToolWindow(
        apiInspector,
        ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                         ui->toolWindowManager->areaOf(eventBrowser), 0.3f));

    QWidget *timelineBar = m_Ctx.GetTimelineBar()->Widget();

    ui->toolWindowManager->addToolWindow(
        timelineBar,
        ToolWindowManager::AreaReference(ToolWindowManager::TopWindowSide,
                                         ui->toolWindowManager->areaOf(textureViewer), 0.2f));
  }

  QMainWindow::show();
}

void MainWindow::recentCaptureFile(const QString &filename)
{
  if(QFileInfo::exists(filename))
  {
    LoadCapture(filename, false, true);
  }
  else
  {
    QMessageBox::StandardButton res =
        RDDialog::question(this, tr("File not found"),
                           tr("File %1 couldn't be found.\nRemove from recent list?").arg(filename));

    if(res == QMessageBox::Yes)
    {
      m_Ctx.Config().RecentCaptureFiles.removeOne(filename);

      PopulateRecentCaptureFiles();
    }
  }
}

void MainWindow::recentCaptureSetting(const QString &filename)
{
  if(QFileInfo::exists(filename))
  {
    OpenCaptureConfigFile(filename, false);
  }
  else
  {
    QMessageBox::StandardButton res =
        RDDialog::question(this, tr("File not found"),
                           tr("File %1 couldn't be found.\nRemove from recent list?").arg(filename));

    if(res == QMessageBox::Yes)
    {
      m_Ctx.Config().RecentCaptureSettings.removeOne(filename);

      PopulateRecentCaptureSettings();
    }
  }
}

void MainWindow::setProgress(float val)
{
  if(val < 0.0f || val >= 1.0f)
  {
    statusProgress->setVisible(false);
    statusText->setText(QString());
  }
  else
  {
    statusProgress->setVisible(true);
    statusProgress->setMaximum(1000);
    statusProgress->setValue(1000 * val);
  }
}

void MainWindow::setCaptureHasErrors(bool errors)
{
  QString filename = QFileInfo(m_Ctx.GetCaptureFilename()).fileName();
  if(errors)
  {
    const QPixmap &del = Pixmaps::del(this);
    QPixmap empty(del.width(), del.height());
    empty.fill(Qt::transparent);
    statusIcon->setPixmap(m_messageAlternate ? empty : del);

    QString text;
    text = tr("%1 loaded. Capture has %2 issues.").arg(filename).arg(m_Ctx.DebugMessages().size());
    if(m_Ctx.UnreadMessageCount() > 0)
      text += tr(" %1 Unread.").arg(m_Ctx.UnreadMessageCount());
    statusText->setText(text);
  }
  else
  {
    statusIcon->setPixmap(Pixmaps::tick(this));
    statusText->setText(tr("%1 loaded. No problems detected.").arg(filename));
  }
}

void MainWindow::remoteProbe()
{
  if(!m_Ctx.IsCaptureLoaded() && !m_Ctx.IsCaptureLoading())
  {
    GUIInvoke::call([this] { m_Ctx.Config().AddAndroidHosts(); });

    for(RemoteHost *host : m_Ctx.Config().RemoteHosts)
    {
      // don't mess with a host we're connected to - this is handled anyway
      if(host->connected)
        continue;

      host->CheckStatus();

      // bail as soon as we notice that we're done
      if(!m_RemoteProbeSemaphore.available())
        return;
    }
  }
}

void MainWindow::messageCheck()
{
  if(m_Ctx.IsCaptureLoaded())
  {
    if(m_Ctx.Replay().GetCurrentProcessingTime() >= 1.5f)
    {
      statusProgress->setVisible(true);
      statusProgress->setMaximum(0);
    }
    else
    {
      statusProgress->hide();
    }

    m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
      rdcarray<DebugMessage> msgs = r->GetDebugMessages();

      bool disconnected = false;

      if(m_Ctx.Replay().CurrentRemote())
      {
        bool prev = m_Ctx.Replay().CurrentRemote()->serverRunning;

        m_Ctx.Replay().PingRemote();

        if(prev != m_Ctx.Replay().CurrentRemote()->serverRunning)
          disconnected = true;
      }

      GUIInvoke::call([this, disconnected, msgs] {
        // if we just got disconnected while replaying a capture, alert the user.
        if(disconnected)
        {
          RDDialog::critical(this, tr("Remote server disconnected"),
                             tr("Remote server disconnected during replaying of this capture.\n"
                                "The replay will now be non-functional. To restore you will have "
                                "to close the capture, allow "
                                "RenderDoc to reconnect and load the capture again"));
        }

        if(m_Ctx.Replay().CurrentRemote() && !m_Ctx.Replay().CurrentRemote()->serverRunning)
          contextChooser->setIcon(Icons::cross());

        if(!msgs.empty())
        {
          m_Ctx.AddMessages(msgs);
        }

        if(m_Ctx.UnreadMessageCount() > 0)
          m_messageAlternate = !m_messageAlternate;
        else
          m_messageAlternate = false;

        setCaptureHasErrors(!m_Ctx.DebugMessages().empty());
      });
    });
  }
  else if(!m_Ctx.IsCaptureLoaded() && !m_Ctx.IsCaptureLoading())
  {
    if(m_Ctx.Replay().CurrentRemote())
      m_Ctx.Replay().PingRemote();

    GUIInvoke::call([this]() {
      if(m_Ctx.Replay().CurrentRemote() && !m_Ctx.Replay().CurrentRemote()->serverRunning)
      {
        contextChooser->setIcon(Icons::cross());
        contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
        statusText->setText(
            tr("Remote server disconnected. To attempt to reconnect please select it again."));

        m_Ctx.Replay().DisconnectFromRemoteServer();
      }
    });
  }
}

void MainWindow::FillRemotesMenu(QMenu *menu, bool includeLocalhost)
{
  menu->clear();

  for(int i = 0; i < m_Ctx.Config().RemoteHosts.count(); i++)
  {
    RemoteHost *host = m_Ctx.Config().RemoteHosts[i];

    // add localhost at the end
    if(host->IsLocalhost())
      continue;

    QAction *action = new QAction(menu);

    action->setIcon(host->serverRunning && !host->versionMismatch ? Icons::tick() : Icons::cross());
    if(host->connected)
      action->setText(tr("%1 (Connected)").arg(host->Name()));
    else if(host->serverRunning && host->versionMismatch)
      action->setText(tr("%1 (Bad Version)").arg(host->Name()));
    else if(host->serverRunning && host->busy)
      action->setText(tr("%1 (Busy)").arg(host->Name()));
    else if(host->serverRunning)
      action->setText(tr("%1 (Online)").arg(host->Name()));
    else
      action->setText(tr("%1 (Offline)").arg(host->Name()));
    QObject::connect(action, &QAction::triggered, this, &MainWindow::switchContext);
    action->setData(i);

    // don't allow switching to the connected host
    if(host->connected)
      action->setEnabled(false);

    menu->addAction(action);
  }

  if(includeLocalhost)
  {
    QAction *localContext = new QAction(menu);

    localContext->setText(tr("Local"));
    localContext->setIcon(Icons::house());

    QObject::connect(localContext, &QAction::triggered, this, &MainWindow::switchContext);
    localContext->setData(-1);

    menu->addAction(localContext);
  }
}

void MainWindow::switchContext()
{
  QAction *item = qobject_cast<QAction *>(QObject::sender());

  if(!item)
    return;

  bool ok = false;
  int hostIdx = item->data().toInt(&ok);

  if(!ok)
    return;

  RemoteHost *host = NULL;

  if(hostIdx >= 0 && hostIdx < m_Ctx.Config().RemoteHosts.count())
  {
    host = m_Ctx.Config().RemoteHosts[hostIdx];
  }

  for(LiveCapture *live : m_LiveCaptures)
  {
    // allow live captures to this host to stay open, that way
    // we can connect to a live capture, then switch into that
    // context
    if(host && live->hostname() == host->hostname)
      continue;

    if(!live->checkAllowClose())
      return;
  }

  if(!PromptCloseCapture())
    return;

  for(LiveCapture *live : m_LiveCaptures)
  {
    // allow live captures to this host to stay open, that way
    // we can connect to a live capture, then switch into that
    // context
    if(host && live->hostname() == host->hostname)
      continue;

    live->cleanItems();
    live->close();
  }

  m_Ctx.Replay().DisconnectFromRemoteServer();

  if(!host)
  {
    contextChooser->setIcon(Icons::house());
    contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));

    ui->action_Inject_into_Process->setEnabled(true);

    statusText->setText(QString());

    SetTitle();
  }
  else
  {
    contextChooser->setText(tr("Replay Context: %1").arg(host->Name()));
    contextChooser->setIcon(host->serverRunning ? Icons::connect() : Icons::disconnect());

    // disable until checking is done
    contextChooser->setEnabled(false);

    ui->action_Inject_into_Process->setEnabled(false);

    SetTitle();

    statusText->setText(tr("Checking remote server status..."));

    LambdaThread *th = new LambdaThread([this, host]() {
      // see if the server is up
      host->CheckStatus();

      if(!host->serverRunning && !host->runCommand.isEmpty())
      {
        GUIInvoke::call([this]() {
          statusText->setText(tr("Running remote server command..."));
          statusProgress->setVisible(true);
          statusProgress->setMaximum(0);
        });

        host->Launch();

        // check if it's running now
        host->CheckStatus();

        GUIInvoke::call([this]() { statusProgress->setVisible(false); });
      }

      ReplayStatus status = ReplayStatus::Succeeded;

      if(host->serverRunning && !host->busy)
      {
        status = m_Ctx.Replay().ConnectToRemoteServer(host);
      }

      GUIInvoke::call([this, host, status]() {
        contextChooser->setIcon(host->serverRunning && !host->busy ? Icons::connect()
                                                                   : Icons::disconnect());

        if(status != ReplayStatus::Succeeded)
        {
          contextChooser->setIcon(Icons::cross());
          contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
          statusText->setText(tr("Connection failed: %1").arg(ToQStr(status)));
        }
        else if(host->versionMismatch)
        {
          statusText->setText(
              tr("Remote server is not running RenderDoc %1").arg(lit(FULL_VERSION_STRING)));
        }
        else if(host->busy)
        {
          statusText->setText(tr("Remote server in use elsewhere"));
        }
        else if(host->serverRunning)
        {
          statusText->setText(tr("Remote server ready"));
        }
        else
        {
          if(!host->runCommand.isEmpty())
            statusText->setText(tr("Remote server not running or failed to start"));
          else
            statusText->setText(tr("Remote server not running - no start command configured"));
        }

        contextChooser->setEnabled(true);
      });
    });
    th->selfDelete(true);
    th->start();
  }
}

void MainWindow::contextChooser_menuShowing()
{
  FillRemotesMenu(contextChooserMenu, true);
}

void MainWindow::statusDoubleClicked(QMouseEvent *event)
{
  showDebugMessageView();
}

void MainWindow::OnCaptureLoaded()
{
  // at first only allow the default save for temporary captures. It should be disabled if we have
  // loaded a 'permanent' capture from disk and haven't made any changes. It will be enabled as soon
  // as any changes are made.
  ui->action_Save_Capture_Inplace->setEnabled(m_Ctx.IsCaptureTemporary());
  ui->action_Save_Capture_As->setEnabled(true);
  ui->action_Close_Capture->setEnabled(true);
  ui->menu_Export_As->setEnabled(true);

  // don't allow changing context while capture is open
  contextChooser->setEnabled(false);

  statusProgress->setVisible(false);

  ui->action_Recompress_Capture->setEnabled(true);

  ui->action_Start_Replay_Loop->setEnabled(true);

  setCaptureHasErrors(!m_Ctx.DebugMessages().empty());

  ui->action_Resolve_Symbols->setEnabled(false);

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *) {
    bool hasResolver = m_Ctx.Replay().GetCaptureAccess()->HasCallstacks();

    GUIInvoke::call([this, hasResolver]() {
      ui->action_Resolve_Symbols->setEnabled(hasResolver);
      ui->action_Resolve_Symbols->setText(hasResolver ? tr("Resolve Symbols")
                                                      : tr("Resolve Symbols - None in capture"));
    });
  });

  SetTitle();

  PopulateRecentCaptureFiles();

  ToolWindowManager::raiseToolWindow(m_Ctx.GetEventBrowser()->Widget());
}

void MainWindow::OnCaptureClosed()
{
  ui->action_Save_Capture_Inplace->setEnabled(false);
  ui->action_Save_Capture_As->setEnabled(false);
  ui->action_Close_Capture->setEnabled(false);
  ui->menu_Export_As->setEnabled(false);

  ui->action_Start_Replay_Loop->setEnabled(false);

  contextChooser->setEnabled(true);

  statusText->setText(QString());
  statusIcon->setPixmap(QPixmap());
  statusProgress->setVisible(false);

  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

  ui->action_Recompress_Capture->setEnabled(false);

  SetTitle();

  // if the remote sever disconnected during capture replay, resort back to a 'disconnected' state
  if(m_Ctx.Replay().CurrentRemote() && !m_Ctx.Replay().CurrentRemote()->serverRunning)
  {
    statusText->setText(
        tr("Remote server disconnected. To attempt to reconnect please select it again."));
    contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
    m_Ctx.Replay().DisconnectFromRemoteServer();
  }
}

void MainWindow::OnEventChanged(uint32_t eventId)
{
}

void MainWindow::RegisterShortcut(const rdcstr &shortcut, QWidget *widget, ShortcutCallback callback)
{
  QKeySequence ks = QKeySequence::fromString(shortcut);

  if(widget)
  {
    m_WidgetShortcutCallbacks[ks][widget] = callback;
  }
  else
  {
    if(m_GlobalShortcutCallbacks[ks])
    {
      qCritical() << "Assigning duplicate global shortcut for" << ks;
      return;
    }

    m_GlobalShortcutCallbacks[ks] = callback;
  }
}

void MainWindow::UnregisterShortcut(const rdcstr &shortcut, QWidget *widget)
{
  if(widget)
  {
    if(shortcut.isEmpty())
    {
      // if no shortcut is specified, remove all shortcuts for this widget
      for(QMap<QWidget *, ShortcutCallback> &sh : m_WidgetShortcutCallbacks)
        sh.remove(widget);
    }
    else
    {
      QKeySequence ks = QKeySequence::fromString(shortcut);

      m_WidgetShortcutCallbacks[ks].remove(widget);
    }
  }
  else
  {
    QKeySequence ks = QKeySequence::fromString(shortcut);

    m_GlobalShortcutCallbacks.remove(ks);
  }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if(event->type() == QEvent::ShortcutOverride)
  {
    QKeyEvent *ke = (QKeyEvent *)event;

    QKeySequence pressed(ke->modifiers() | ke->key());

    // first see if there's a widget shortcut registered for this key. If so, check the focus
    // hierarchy to see if we have any matches
    QWidget *focus = QApplication::focusWidget();

    if(focus && m_WidgetShortcutCallbacks.contains(pressed))
    {
      const QMap<QWidget *, ShortcutCallback> callbacks = m_WidgetShortcutCallbacks[pressed];
      QList<QWidget *> widgets = callbacks.keys();

      while(focus)
      {
        // if we find a direct ancestor to the focus widget which is registered for this shortcut,
        // then use that callback
        if(widgets.contains(focus))
        {
          callbacks[focus]();
          event->accept();
          return true;
        }

        // keep searching up the hierarchy
        focus = focus->parentWidget();
      }
    }

    // if we didn't find matches or no such shortcut is registered, try global shortcuts
    if(m_GlobalShortcutCallbacks.contains(pressed))
    {
      m_GlobalShortcutCallbacks[pressed]();
      event->accept();
      return true;
    }
  }

  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_action_Close_Capture_triggered()
{
  PromptCloseCapture();
}

void MainWindow::on_action_Save_Capture_Inplace_triggered()
{
  bool saved = false;

  if(m_Ctx.IsCaptureTemporary() || !m_Ctx.IsCaptureLocal())
  {
    saved = PromptSaveCaptureAs();
  }
  else
  {
    if(m_Ctx.GetCaptureModifications() != CaptureModifications::NoModifications &&
       m_Ctx.IsCaptureLocal())
    {
      saved = m_Ctx.SaveCaptureTo(m_Ctx.GetCaptureFilename());
    }
  }

  if(saved)
    ui->action_Save_Capture_Inplace->setEnabled(false);
}

void MainWindow::on_action_Save_Capture_As_triggered()
{
  PromptSaveCaptureAs();
}

void MainWindow::on_action_About_triggered()
{
  AboutDialog about(this);
  RDDialog::show(&about);
}

void MainWindow::on_action_Mesh_Output_triggered()
{
  QWidget *meshPreview = m_Ctx.GetMeshPreview()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(meshPreview))
    ToolWindowManager::raiseToolWindow(meshPreview);
  else
    ui->toolWindowManager->addToolWindow(meshPreview, mainToolArea());
}

void MainWindow::on_action_API_Inspector_triggered()
{
  QWidget *apiInspector = m_Ctx.GetAPIInspector()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(apiInspector))
  {
    ToolWindowManager::raiseToolWindow(apiInspector);
  }
  else
  {
    if(m_Ctx.HasEventBrowser() &&
       ui->toolWindowManager->toolWindows().contains(m_Ctx.GetEventBrowser()->Widget()))
    {
      ToolWindowManager::AreaReference ref(
          ToolWindowManager::BottomOf,
          ui->toolWindowManager->areaOf(m_Ctx.GetEventBrowser()->Widget()));
      ui->toolWindowManager->addToolWindow(apiInspector, ref);
    }
    else
    {
      ui->toolWindowManager->addToolWindow(apiInspector, leftToolArea());
    }
  }
}

void MainWindow::on_action_Event_Browser_triggered()
{
  QWidget *eventBrowser = m_Ctx.GetEventBrowser()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(eventBrowser))
    ToolWindowManager::raiseToolWindow(eventBrowser);
  else
    ui->toolWindowManager->addToolWindow(eventBrowser, leftToolArea());
}

void MainWindow::on_action_Texture_Viewer_triggered()
{
  QWidget *textureViewer = m_Ctx.GetTextureViewer()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(textureViewer))
    ToolWindowManager::raiseToolWindow(textureViewer);
  else
    ui->toolWindowManager->addToolWindow(textureViewer, mainToolArea());
}

void MainWindow::on_action_Pipeline_State_triggered()
{
  QWidget *pipelineViewer = m_Ctx.GetPipelineViewer()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(pipelineViewer))
    ToolWindowManager::raiseToolWindow(pipelineViewer);
  else
    ui->toolWindowManager->addToolWindow(pipelineViewer, mainToolArea());
}

void MainWindow::on_action_Launch_Application_triggered()
{
  ICaptureDialog *capDialog = m_Ctx.GetCaptureDialog();

  capDialog->SetInjectMode(false);

  if(ui->toolWindowManager->toolWindows().contains(capDialog->Widget()))
    ToolWindowManager::raiseToolWindow(capDialog->Widget());
  else
    ui->toolWindowManager->addToolWindow(capDialog->Widget(), mainToolArea());
}

void MainWindow::on_action_Inject_into_Process_triggered()
{
  ICaptureDialog *capDialog = m_Ctx.GetCaptureDialog();

  capDialog->SetInjectMode(true);

  if(ui->toolWindowManager->toolWindows().contains(capDialog->Widget()))
    ToolWindowManager::raiseToolWindow(capDialog->Widget());
  else
    ui->toolWindowManager->addToolWindow(capDialog->Widget(), mainToolArea());
}

void MainWindow::on_action_Errors_and_Warnings_triggered()
{
  QWidget *debugMessages = m_Ctx.GetDebugMessageView()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(debugMessages))
    ToolWindowManager::raiseToolWindow(debugMessages);
  else
    ui->toolWindowManager->addToolWindow(debugMessages, mainToolArea());
}

void MainWindow::on_action_Comments_triggered()
{
  QWidget *comments = m_Ctx.GetCommentView()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(comments))
    ToolWindowManager::raiseToolWindow(comments);
  else
    ui->toolWindowManager->addToolWindow(comments, mainToolArea());
}

void MainWindow::on_action_Statistics_Viewer_triggered()
{
  QWidget *stats = m_Ctx.GetStatisticsViewer()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(stats))
    ToolWindowManager::raiseToolWindow(stats);
  else
    ui->toolWindowManager->addToolWindow(stats, mainToolArea());
}

void MainWindow::on_action_Timeline_triggered()
{
  QWidget *stats = m_Ctx.GetTimelineBar()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(stats))
    ToolWindowManager::raiseToolWindow(stats);
  else
    ui->toolWindowManager->addToolWindow(
        stats,
        ToolWindowManager::AreaReference(ToolWindowManager::TopWindowSide, mainToolArea().area()));
}

void MainWindow::on_action_Python_Shell_triggered()
{
  QWidget *py = m_Ctx.GetPythonShell()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(py))
    ToolWindowManager::raiseToolWindow(py);
  else
    ui->toolWindowManager->addToolWindow(py, mainToolArea());
}

void MainWindow::on_action_Resolve_Symbols_triggered()
{
  ANALYTIC_SET(UIFeatures.CallstackResolve, true);

  if(!m_Ctx.Replay().GetCaptureAccess())
  {
    RDDialog::critical(
        this, tr("Not Available"),
        tr("Callstack resolution is not available.\n\nCheck remote server connection."));
    return;
  }

  float progress = 0.0f;
  bool finished = false;

  m_Ctx.Replay().AsyncInvoke([this, &progress, &finished](IReplayController *) {
    bool success =
        m_Ctx.Replay().GetCaptureAccess()->InitResolver([&progress](float p) { progress = p; });

    if(!success)
    {
      RDDialog::critical(
          this, tr("Error loading symbols"),
          tr("Couldn't load symbols for callstack resolution.\n\nCheck diagnostic log in "
             "Help menu for more details."));
    }

    finished = true;
  });

  ShowProgressDialog(this, tr("Resolving symbols, please wait..."),
                     [&finished]() { return finished; }, [&progress]() { return progress; });

  if(m_Ctx.HasAPIInspector())
    m_Ctx.GetAPIInspector()->Refresh();
}

void MainWindow::on_action_Recompress_Capture_triggered()
{
  m_Ctx.RecompressCapture();
}

void MainWindow::on_action_Start_Replay_Loop_triggered()
{
  if(!m_Ctx.IsCaptureLoaded())
    return;

  QDialog popup;
  popup.setWindowFlags(popup.windowFlags() & ~Qt::WindowContextHelpButtonHint);
  popup.setWindowIcon(windowIcon());

  const TextureDescription *displayTex = NULL;

  const DrawcallDescription *lastDraw = m_Ctx.GetLastDrawcall();

  displayTex = m_Ctx.GetTexture(lastDraw->copyDestination);
  if(!displayTex)
    displayTex = m_Ctx.GetTexture(lastDraw->outputs[0]);

  if(!displayTex)
  {
    // if no texture was bound, then use the first colour swapbuffer
    for(const TextureDescription &tex : m_Ctx.GetTextures())
    {
      if((tex.creationFlags & TextureCategory::SwapBuffer) &&
         tex.format.compType != CompType::Depth && tex.format.type != ResourceFormatType::D16S8 &&
         tex.format.type != ResourceFormatType::D24S8 && tex.format.type != ResourceFormatType::D32S8)
      {
        displayTex = &tex;
        break;
      }
    }
  }

  ResourceId id;

  if(displayTex)
  {
    id = displayTex->resourceId;
    popup.resize((int)displayTex->width, (int)displayTex->height);
    popup.setWindowTitle(tr("Looping replay of %1 Displaying %2")
                             .arg(m_Ctx.GetCaptureFilename())
                             .arg(m_Ctx.GetResourceName(id)));
  }
  else
  {
    popup.resize(100, 100);
    popup.setWindowTitle(tr("Looping replay of %1 Displaying %2")
                             .arg(m_Ctx.GetCaptureFilename())
                             .arg(tr("nothing")));
  }

  WindowingData winData = m_Ctx.CreateWindowingData(popup.winId());

  m_Ctx.Replay().AsyncInvoke([winData, id](IReplayController *r) { r->ReplayLoop(winData, id); });

  RDDialog::show(&popup);

  m_Ctx.Replay().CancelReplayLoop();
}

void MainWindow::on_action_Attach_to_Running_Instance_triggered()
{
  on_action_Manage_Remote_Servers_triggered();
}

void MainWindow::on_action_Manage_Remote_Servers_triggered()
{
  RemoteManager *rm = new RemoteManager(m_Ctx, this);
  RDDialog::show(rm);
  // now that we're done with it, the manager deletes itself when all lookups terminate (or
  // immediately if there are no lookups ongoing).
  rm->closeWhenFinished();
}

void MainWindow::on_action_Settings_triggered()
{
  SettingsDialog about(m_Ctx, this);
  RDDialog::show(&about);
}

void MainWindow::on_action_View_Documentation_triggered()
{
  QFileInfo fi(QGuiApplication::applicationFilePath());

  if(fi.absoluteDir().exists(lit("renderdoc.chm")))
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(fi.absoluteDir().absoluteFilePath(lit("renderdoc.chm"))));
  else
    QDesktopServices::openUrl(QUrl::fromUserInput(lit("https://renderdoc.org/docs")));
}

void MainWindow::on_action_View_Diagnostic_Log_File_triggered()
{
  QString logPath = QString::fromUtf8(RENDERDOC_GetLogFile());
  if(QFileInfo::exists(logPath))
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void MainWindow::on_action_Source_on_GitHub_triggered()
{
  QDesktopServices::openUrl(QUrl::fromUserInput(lit("https://github.com/baldurk/renderdoc")));
}

void MainWindow::on_action_Build_Release_Downloads_triggered()
{
  QDesktopServices::openUrl(QUrl::fromUserInput(lit("https://renderdoc.org/builds")));
}

void MainWindow::on_action_Show_Tips_triggered()
{
  TipsDialog tipsDialog(m_Ctx, this);
  RDDialog::show(&tipsDialog);
}

void MainWindow::on_action_Counter_Viewer_triggered()
{
  QWidget *performanceCounterViewer = m_Ctx.GetPerformanceCounterViewer()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(performanceCounterViewer))
    ToolWindowManager::raiseToolWindow(performanceCounterViewer);
  else
    ui->toolWindowManager->addToolWindow(performanceCounterViewer, mainToolArea());
}

void MainWindow::on_action_Resource_Inspector_triggered()
{
  QWidget *resourceInspector = m_Ctx.GetResourceInspector()->Widget();

  if(ui->toolWindowManager->toolWindows().contains(resourceInspector))
    ToolWindowManager::raiseToolWindow(resourceInspector);
  else
    ui->toolWindowManager->addToolWindow(resourceInspector, mainToolArea());
}

void MainWindow::on_action_Send_Error_Report_triggered()
{
  rdcstr report;
  RENDERDOC_CreateBugReport(RENDERDOC_GetLogFile(), "", report);

  QVariantMap json;

  json[lit("version")] = lit(FULL_VERSION_STRING);
  json[lit("gitcommit")] = QString::fromLatin1(GitVersionHash);
  json[lit("replaycrash")] = 1;
  json[lit("report")] = (QString)report;

  CrashDialog crash(m_Ctx.Config(), json, this);

  RDDialog::show(&crash);

  m_Ctx.Config().Save();
  PopulateReportedBugs();

  QFile::remove(QString(report));
}

void MainWindow::on_action_Check_for_Updates_triggered()
{
  CheckUpdates(true, [this](UpdateResult updateResult) {
    switch(updateResult)
    {
      case UpdateResult::Disabled:
      case UpdateResult::Toosoon:
      {
        // won't happen, we forced the check
        break;
      }
      case UpdateResult::Unofficial:
      {
        QMessageBox::StandardButton res =
            RDDialog::question(this, tr("Unofficial build"),
                               tr("You are running an unofficial build, not a stable release.\n"
                                  "Updates are only available for installed release builds\n\n"
                                  "Would you like to open the builds list in a browser?"));

        if(res == QMessageBox::Yes)
          QDesktopServices::openUrl(lit("https://renderdoc.org/builds"));
        break;
      }
      case UpdateResult::Latest:
      {
        RDDialog::information(this, tr("Latest version"),
                              tr("You are running the latest version."));
        break;
      }
      case UpdateResult::Upgrade:
      {
        // CheckUpdates() will have shown a dialog for this
        break;
      }
    }
  });
}

void MainWindow::updateAvailable_triggered()
{
  bool mismatch = HandleMismatchedVersions();
  if(mismatch)
    return;

  SetUpdateAvailable();
  UpdatePopup();
}

void MainWindow::saveLayout_triggered()
{
  LoadSaveLayout(qobject_cast<QAction *>(QObject::sender()), true);
}

void MainWindow::loadLayout_triggered()
{
  LoadSaveLayout(qobject_cast<QAction *>(QObject::sender()), false);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  for(LiveCapture *live : m_LiveCaptures)
  {
    if(!live->checkAllowClose())
    {
      event->ignore();
      return;
    }
  }

  if(RENDERDOC_IsGlobalHookActive())
  {
    RDDialog::critical(this, tr("Global hook active"),
                       tr("Cannot close RenderDoc while global hook is active."));
    event->ignore();
    return;
  }

  if(!PromptCloseCapture())
  {
    event->ignore();
    return;
  }

  for(LiveCapture *live : m_LiveCaptures)
  {
    live->cleanItems();
    delete live;
  }

  SaveLayout(0);
}

void MainWindow::changeEvent(QEvent *event)
{
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    QPixmapCache::clear();
}

QString MainWindow::dragFilename(const QMimeData *mimeData)
{
  if(mimeData->hasUrls())
  {
    QList<QUrl> urls = mimeData->urls();
    if(urls.size() == 1 && urls[0].isLocalFile())
    {
      QFileInfo f(urls[0].toLocalFile());
      if(f.exists())
        return f.absoluteFilePath();
    }
  }

  return QString();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if(!dragFilename(event->mimeData()).isEmpty())
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
  QString fn = dragFilename(event->mimeData());
  if(!fn.isEmpty())
  {
    // we defer this so we can return immediately and unblock whichever application dropped the
    // item.
    GUIInvoke::defer([this, fn]() { LoadFromFilename(fn, false); });
  }
}

void MainWindow::LoadSaveLayout(QAction *action, bool save)
{
  if(action == NULL)
  {
    qWarning() << "NULL action passed to LoadSaveLayout - bad signal?";
    return;
  }

  bool success = false;

  if(action == ui->action_Save_Default_Layout)
  {
    success = SaveLayout(0);
  }
  else if(action == ui->action_Load_Default_Layout)
  {
    success = LoadLayout(0);
  }
  else
  {
    QString name = action->objectName();
    name.remove(0, name.size() - 1);
    int idx = name.toInt();

    if(idx > 0)
    {
      if(save)
        success = SaveLayout(idx);
      else
        success = LoadLayout(idx);
    }
  }

  if(!success)
  {
    if(save)
      RDDialog::critical(this, tr("Error saving layout"), tr("Couldn't save layout"));
    else
      RDDialog::critical(this, tr("Error loading layout"), tr("Couldn't load layout"));
  }
}

QVariantMap MainWindow::saveState()
{
  QVariantMap state = ui->toolWindowManager->saveState();

  state[lit("mainWindowGeometry")] = saveGeometry().toBase64();

  return state;
}

bool MainWindow::restoreState(QVariantMap &state)
{
  restoreGeometry(QByteArray::fromBase64(state[lit("mainWindowGeometry")].toByteArray()));

  ui->toolWindowManager->restoreState(state);

  return true;
}

bool MainWindow::SaveLayout(int layout)
{
  QString path = GetLayoutPath(layout);

  QVariantMap state = saveState();

  QFile f(path);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return SaveToJSON(state, f, JSON_ID, JSON_VER);

  qWarning() << "Couldn't write to " << path << " " << f.errorString();

  return false;
}

bool MainWindow::LoadLayout(int layout)
{
  QString path = GetLayoutPath(layout);

  QFile f(path);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QVariantMap state;

    bool success = LoadFromJSON(state, f, JSON_ID, JSON_VER);

    if(!success)
      return false;

    return restoreState(state);
  }

  qInfo() << "Couldn't load layout from " << path << " " << f.errorString();

  return false;
}
