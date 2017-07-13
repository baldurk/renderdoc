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

#include "MainWindow.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QProgressBar>
#include <QProgressDialog>
#include <QToolButton>
#include "Code/CaptureContext.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Resources/resource.h"
#include "Widgets/Extended/RDLabel.h"
#include "Windows/Dialogs/AboutDialog.h"
#include "Windows/Dialogs/CaptureDialog.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/Dialogs/RemoteManager.h"
#include "Windows/Dialogs/SettingsDialog.h"
#include "Windows/Dialogs/SuggestRemoteDialog.h"
#include "Windows/Dialogs/TipsDialog.h"
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

  installEventFilter(this);

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

  contextChooserMenu = new QMenu(this);

  FillRemotesMenu(contextChooserMenu, true);

  contextChooser = new QToolButton(this);
  contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
  contextChooser->setIcon(Icons::house());
  contextChooser->setAutoRaise(true);
  contextChooser->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  contextChooser->setPopupMode(QToolButton::InstantPopup);
  contextChooser->setMenu(contextChooserMenu);
  contextChooser->setStyleSheet(lit("QToolButton::menu-indicator { image: none; }"));
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
  m_MessageTick.setInterval(500);
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

  ui->statusBar->setStyleSheet(lit("QStatusBar::item { border: 0px }"));

  SetTitle();

  PopulateRecentFiles();
  PopulateRecentCaptures();

  ui->toolWindowManager->setToolWindowCreateCallback([this](const QString &objectName) -> QWidget * {
    return m_Ctx.CreateBuiltinWindow(objectName);
  });

  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

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

  m_Ctx.AddLogViewer(this);

  ui->action_Save_Log->setEnabled(false);
  ui->action_Close_Log->setEnabled(false);

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

  return ConfigFilePath(filename);
}

void MainWindow::on_action_Exit_triggered()
{
  this->close();
}

void MainWindow::on_action_Open_Log_triggered()
{
  if(!PromptCloseLog())
    return;

  QString filename = RDDialog::getOpenFileName(
      this, tr("Select Logfile to open"), m_Ctx.Config().LastLogPath,
      tr("Capture Files (*.rdc);;Image Files (*.dds *.hdr *.exr *.bmp *.jpg "
         "*.jpeg *.png *.tga *.gif *.psd;;All Files (*.*)"));

  if(!filename.isEmpty())
    LoadFromFilename(filename, false);
}

void MainWindow::LoadFromFilename(const QString &filename, bool temporary)
{
  QFileInfo path(filename);
  QString ext = path.suffix().toLower();

  if(ext == lit("rdc"))
  {
    LoadLogfile(filename, temporary, true);
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
    LoadLogfile(filename, temporary, true);
  }
}

void MainWindow::OnCaptureTrigger(const QString &exe, const QString &workingDir,
                                  const QString &cmdLine, const QList<EnvironmentModification> &env,
                                  CaptureOptions opts, std::function<void(LiveCapture *)> callback)
{
  if(!PromptCloseLog())
    return;

  LambdaThread *th = new LambdaThread([this, exe, workingDir, cmdLine, env, opts, callback]() {
    QString logfile = m_Ctx.TempLogFilename(QFileInfo(exe).baseName());

    uint32_t ret = m_Ctx.Replay().ExecuteAndInject(exe, workingDir, cmdLine, env, logfile, opts);

    GUIInvoke::call([this, exe, ret, callback]() {
      if(ret == 0)
      {
        RDDialog::critical(this, tr("Error kicking capture"),
                           tr("Error launching %1 for capture.\n\nCheck diagnostic log in Help "
                              "menu for more details.")
                               .arg(exe));
        return;
      }

      LiveCapture *live = new LiveCapture(
          m_Ctx,
          m_Ctx.Replay().CurrentRemote() ? m_Ctx.Replay().CurrentRemote()->Hostname : QString(),
          m_Ctx.Replay().CurrentRemote() ? m_Ctx.Replay().CurrentRemote()->Name() : QString(), ret,
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
    ShowProgressDialog(this, tr("Launching %1, please wait...").arg(exe),
                       [th]() { return !th->isRunning(); });
  }
  th->deleteLater();
}

void MainWindow::OnInjectTrigger(uint32_t PID, const QList<EnvironmentModification> &env,
                                 const QString &name, CaptureOptions opts,
                                 std::function<void(LiveCapture *)> callback)
{
  if(!PromptCloseLog())
    return;

  rdctype::array<EnvironmentModification> envList = env.toVector().toStdVector();

  LambdaThread *th = new LambdaThread([this, PID, envList, name, opts, callback]() {
    QString logfile = m_Ctx.TempLogFilename(name);

    uint32_t ret = RENDERDOC_InjectIntoProcess(PID, envList, logfile.toUtf8().data(), opts, false);

    GUIInvoke::call([this, PID, ret, callback]() {
      if(ret == 0)
      {
        RDDialog::critical(
            this, tr("Error kicking capture"),
            tr("Error injecting into process %1 for capture.\n\nCheck diagnostic log in "
               "Help menu for more details.")
                .arg(PID));
        return;
      }

      LiveCapture *live = new LiveCapture(m_Ctx, QString(), QString(), ret, this, this);
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

void MainWindow::LoadLogfile(const QString &filename, bool temporary, bool local)
{
  if(PromptCloseLog())
  {
    if(m_Ctx.LogLoading())
      return;

    QString driver;
    QString machineIdent;
    ReplaySupport support = ReplaySupport::Unsupported;

    bool remoteReplay =
        !local || (m_Ctx.Replay().CurrentRemote() && m_Ctx.Replay().CurrentRemote()->Connected);

    if(local)
    {
      ICaptureFile *file = RENDERDOC_OpenCaptureFile(filename.toUtf8().data());

      if(file->OpenStatus() != ReplayStatus::Succeeded)
      {
        RDDialog::critical(NULL, tr("Error opening capture"),
                           tr("Couldn't open file '%1'").arg(filename));

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
              (m_Ctx.Replay().CurrentRemote() && m_Ctx.Replay().CurrentRemote()->Connected);

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

        QStringList remoteDrivers = m_Ctx.Replay().GetRemoteSupport();

        for(const QString &d : remoteDrivers)
        {
          if(driver == d)
            support = ReplaySupport::Supported;
        }
      }
    }

    QString origFilename = filename;

    // if driver is empty something went wrong loading the log, let it be handled as usual
    // below. Otherwise indicate that support is missing.
    if(!driver.isEmpty() && support == ReplaySupport::Unsupported)
    {
      if(remoteReplay)
      {
        QString remoteMessage =
            tr("This log was captured with %1 and cannot be replayed on %2.\n\n")
                .arg(driver)
                .arg(m_Ctx.Replay().CurrentRemote()->Name());

        remoteMessage += tr("Try selecting a different remote context in the status bar.");

        RDDialog::critical(NULL, tr("Unsupported logfile type"), remoteMessage);
      }
      else
      {
        QString remoteMessage =
            tr("This log was captured with %1 and cannot be replayed locally.\n\n").arg(driver);

        remoteMessage += tr("Try selecting a remote context in the status bar.");

        RDDialog::critical(NULL, tr("Unsupported logfile type"), remoteMessage);
      }

      return;
    }
    else
    {
      QString fileToLoad = filename;

      if(remoteReplay && local)
      {
        fileToLoad = m_Ctx.Replay().CopyCaptureToRemote(filename, this);

        // deliberately leave local as true so that we keep referring to the locally saved log

        // some error
        if(fileToLoad.isEmpty())
        {
          RDDialog::critical(NULL, tr("Error copying to remote"),
                             tr("Couldn't copy %1 to remote host for replaying").arg(filename));
          return;
        }
      }

      m_Ctx.LoadLogfile(fileToLoad, origFilename, temporary, local);
    }

    if(!remoteReplay)
    {
      m_Ctx.Config().LastLogPath = QFileInfo(filename).absolutePath();
    }

    statusText->setText(tr("Loading %1...").arg(origFilename));
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
}

QString MainWindow::GetSavePath()
{
  QString dir;

  if(!m_Ctx.Config().DefaultCaptureSaveDirectory.isEmpty())
  {
    if(m_LastSaveCapturePath.isEmpty())
      dir = m_Ctx.Config().DefaultCaptureSaveDirectory;
    else
      dir = m_LastSaveCapturePath;
  }

  QString filename =
      RDDialog::getSaveFileName(this, tr("Save Capture As"), dir, tr("Capture Files (*.rdc)"));

  if(!filename.isEmpty())
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
      m_LastSaveCapturePath = dirinfo.absolutePath();

    return filename;
  }

  return QString();
}

bool MainWindow::PromptSaveLog()
{
  QString saveFilename = GetSavePath();

  if(!saveFilename.isEmpty())
  {
    if(m_Ctx.IsLogLocal() && !QFileInfo(m_Ctx.LogFilename()).exists())
    {
      RDDialog::critical(NULL, tr("File not found"),
                         tr("Logfile %1 couldn't be found, cannot save.").arg(m_Ctx.LogFilename()));
      return false;
    }

    bool success = false;
    QString error;

    if(m_Ctx.IsLogLocal())
    {
      // we copy the (possibly) temp log to the desired path, but the log item remains referring to
      // the original path.
      // This ensures that if the user deletes the saved path we can still open or re-save it.

      // QFile::copy won't overwrite, so remove the destination first (the save dialog already
      // prompted for overwrite)
      QFile::remove(saveFilename);
      success = QFile::copy(m_Ctx.LogFilename(), saveFilename);

      error = tr("Couldn't save to %1").arg(saveFilename);
    }
    else
    {
      m_Ctx.Replay().CopyCaptureFromRemote(m_Ctx.LogFilename(), saveFilename, this);
      success = QFile::exists(saveFilename);

      error = tr("File couldn't be transferred from remote host");
    }

    if(!success)
    {
      RDDialog::critical(NULL, tr("Error Saving"), error);
      return false;
    }

    AddRecentFile(m_Ctx.Config().RecentLogFiles, saveFilename, 10);
    PopulateRecentFiles();
    SetTitle(saveFilename);

    // we don't prompt to save on closing - if the user deleted the log that we just saved, then
    // that is up to them.
    m_SavedTempLog = true;

    return true;
  }

  return false;
}

bool MainWindow::PromptCloseLog()
{
  if(!m_Ctx.LogLoaded())
    return true;

  QString deletepath;
  bool loglocal = false;

  if(m_OwnTempLog)
  {
    QString temppath = m_Ctx.LogFilename();
    loglocal = m_Ctx.IsLogLocal();

    QMessageBox::StandardButton res = QMessageBox::No;

    // unless we've saved the log, prompt to save
    if(!m_SavedTempLog)
      res = RDDialog::question(NULL, tr("Unsaved log"), tr("Save this logfile?"),
                               QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if(res == QMessageBox::Cancel)
    {
      return false;
    }

    if(res == QMessageBox::Yes)
    {
      bool success = PromptSaveLog();

      if(!success)
      {
        return false;
      }
    }

    if(temppath != m_Ctx.LogFilename() || res == QMessageBox::No)
      deletepath = temppath;
    m_OwnTempLog = false;
    m_SavedTempLog = false;
  }

  CloseLogfile();

  if(!deletepath.isEmpty())
    m_Ctx.Replay().DeleteCapture(deletepath, loglocal);

  return true;
}

void MainWindow::CloseLogfile()
{
  m_Ctx.CloseLogfile();

  ui->action_Save_Log->setEnabled(false);
}

void MainWindow::SetTitle(const QString &filename)
{
  QString prefix;

  if(m_Ctx.LogLoaded())
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
    text += tr("Unstable release (%1 - %2)").arg(lit(FULL_VERSION_STRING)).arg(lit(GIT_COMMIT_HASH));

  if(QString::fromLatin1(RENDERDOC_GetVersionString()) != lit(MAJOR_MINOR_VERSION_STRING))
    text += tr(" - !! VERSION MISMATCH DETECTED !!");

  setWindowTitle(text);
}

void MainWindow::SetTitle()
{
  SetTitle(m_Ctx.LogFilename());
}

void MainWindow::PopulateRecentFiles()
{
  ui->menu_Recent_Logs->clear();

  ui->menu_Recent_Logs->setEnabled(false);

  int idx = 1;
  for(int i = m_Ctx.Config().RecentLogFiles.size() - 1; i >= 0; i--)
  {
    const QString &filename = m_Ctx.Config().RecentLogFiles[i];
    ui->menu_Recent_Logs->addAction(QFormatStr("&%1 %2").arg(idx).arg(filename),
                                    [this, filename] { recentLog(filename); });
    idx++;

    ui->menu_Recent_Logs->setEnabled(true);
  }

  ui->menu_Recent_Logs->addSeparator();
  ui->menu_Recent_Logs->addAction(ui->action_Clear_Log_History);
}

void MainWindow::PopulateRecentCaptures()
{
  ui->menu_Recent_Capture_Settings->clear();

  ui->menu_Recent_Capture_Settings->setEnabled(false);

  int idx = 1;
  for(int i = m_Ctx.Config().RecentCaptureSettings.size() - 1; i >= 0; i--)
  {
    const QString &filename = m_Ctx.Config().RecentCaptureSettings[i];
    ui->menu_Recent_Capture_Settings->addAction(QFormatStr("&%1 %2").arg(idx).arg(filename),
                                                [this, filename] { recentCapture(filename); });
    idx++;

    ui->menu_Recent_Capture_Settings->setEnabled(true);
  }

  ui->menu_Recent_Capture_Settings->addSeparator();
  ui->menu_Recent_Capture_Settings->addAction(ui->action_Clear_Log_History);
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
  }

  QMainWindow::show();
}

void MainWindow::recentLog(const QString &filename)
{
  if(QFileInfo::exists(filename))
  {
    LoadLogfile(filename, false, true);
  }
  else
  {
    QMessageBox::StandardButton res =
        RDDialog::question(NULL, tr("File not found"),
                           tr("File %1 couldn't be found.\nRemove from recent list?").arg(filename));

    if(res == QMessageBox::Yes)
    {
      m_Ctx.Config().RecentLogFiles.removeOne(filename);

      PopulateRecentFiles();
    }
  }
}

void MainWindow::recentCapture(const QString &filename)
{
  if(QFileInfo::exists(filename))
  {
    OpenCaptureConfigFile(filename, false);
  }
  else
  {
    QMessageBox::StandardButton res =
        RDDialog::question(NULL, tr("File not found"),
                           tr("File %1 couldn't be found.\nRemove from recent list?").arg(filename));

    if(res == QMessageBox::Yes)
    {
      m_Ctx.Config().RecentLogFiles.removeOne(filename);

      PopulateRecentFiles();
    }
  }
}

void MainWindow::setProgress(float val)
{
  if(val < 0.0f || val >= 1.0f)
  {
    statusProgress->setVisible(false);
  }
  else
  {
    statusProgress->setVisible(true);
    statusProgress->setValue(1000 * val);
  }
}

void MainWindow::setLogHasErrors(bool errors)
{
  QString filename = QFileInfo(m_Ctx.LogFilename()).fileName();
  if(errors)
  {
    const QPixmap &del = Pixmaps::del(this);
    QPixmap empty(del.width(), del.height());
    empty.fill(Qt::transparent);
    statusIcon->setPixmap(m_messageAlternate ? empty : del);

    QString text;
    text = tr("%1 loaded. Capture has %2 errors, warnings or performance notes. "
              "See the 'Errors and Warnings' window.")
               .arg(filename)
               .arg(m_Ctx.DebugMessages().size());
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
  if(!m_Ctx.LogLoaded() && !m_Ctx.LogLoading())
  {
    for(RemoteHost *host : m_Ctx.Config().RemoteHosts)
    {
      // don't mess with a host we're connected to - this is handled anyway
      if(host->Connected)
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
  if(m_Ctx.LogLoaded())
  {
    m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
      rdctype::array<DebugMessage> msgs = r->GetDebugMessages();

      bool disconnected = false;

      if(m_Ctx.Replay().CurrentRemote())
      {
        bool prev = m_Ctx.Replay().CurrentRemote()->ServerRunning;

        m_Ctx.Replay().PingRemote();

        if(prev != m_Ctx.Replay().CurrentRemote()->ServerRunning)
          disconnected = true;
      }

      GUIInvoke::call([this, disconnected, msgs] {
        // if we just got disconnected while replaying a log, alert the user.
        if(disconnected)
        {
          RDDialog::critical(this, tr("Remote server disconnected"),
                             tr("Remote server disconnected during replaying of this capture.\n"
                                "The replay will now be non-functional. To restore you will have "
                                "to close the capture, allow "
                                "RenderDoc to reconnect and load the capture again"));
        }

        if(m_Ctx.Replay().CurrentRemote() && !m_Ctx.Replay().CurrentRemote()->ServerRunning)
          contextChooser->setIcon(Icons::cross());

        if(!msgs.empty())
        {
          m_Ctx.AddMessages(msgs);
        }

        if(m_Ctx.UnreadMessageCount() > 0)
          m_messageAlternate = !m_messageAlternate;
        else
          m_messageAlternate = false;

        setLogHasErrors(!m_Ctx.DebugMessages().empty());
      });
    });
  }
  else if(!m_Ctx.LogLoaded() && !m_Ctx.LogLoading())
  {
    if(m_Ctx.Replay().CurrentRemote())
      m_Ctx.Replay().PingRemote();

    GUIInvoke::call([this]() {
      if(m_Ctx.Replay().CurrentRemote() && !m_Ctx.Replay().CurrentRemote()->ServerRunning)
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
    if(host->Hostname == lit("localhost"))
      continue;

    QAction *action = new QAction(menu);

    action->setIcon(host->ServerRunning && !host->VersionMismatch ? Icons::tick() : Icons::cross());
    if(host->Connected)
      action->setText(tr("%1 (Connected)").arg(host->Name()));
    else if(host->ServerRunning && host->VersionMismatch)
      action->setText(tr("%1 (Bad Version)").arg(host->Name()));
    else if(host->ServerRunning && host->Busy)
      action->setText(tr("%1 (Busy)").arg(host->Name()));
    else if(host->ServerRunning)
      action->setText(tr("%1 (Online)").arg(host->Name()));
    else
      action->setText(tr("%1 (Offline)").arg(host->Name()));
    QObject::connect(action, &QAction::triggered, this, &MainWindow::switchContext);
    action->setData(i);

    // don't allow switching to the connected host
    if(host->Connected)
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
    if(host && live->hostname() == host->Hostname)
      continue;

    if(!live->checkAllowClose())
      return;
  }

  if(!PromptCloseLog())
    return;

  for(LiveCapture *live : m_LiveCaptures)
  {
    // allow live captures to this host to stay open, that way
    // we can connect to a live capture, then switch into that
    // context
    if(host && live->hostname() == host->Hostname)
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
    contextChooser->setIcon(host->ServerRunning ? Icons::connect() : Icons::disconnect());

    // disable until checking is done
    contextChooser->setEnabled(false);

    ui->action_Inject_into_Process->setEnabled(false);

    SetTitle();

    statusText->setText(tr("Checking remote server status..."));

    LambdaThread *th = new LambdaThread([this, host]() {
      // see if the server is up
      host->CheckStatus();

      if(!host->ServerRunning && !host->RunCommand.isEmpty())
      {
        GUIInvoke::call([this]() { statusText->setText(tr("Running remote server command...")); });

        host->Launch();

        // check if it's running now
        host->CheckStatus();
      }

      ReplayStatus status = ReplayStatus::Succeeded;

      if(host->ServerRunning && !host->Busy)
      {
        status = m_Ctx.Replay().ConnectToRemoteServer(host);
      }

      GUIInvoke::call([this, host, status]() {
        contextChooser->setIcon(host->ServerRunning && !host->Busy ? Icons::connect()
                                                                   : Icons::disconnect());

        if(status != ReplayStatus::Succeeded)
        {
          contextChooser->setIcon(Icons::cross());
          contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
          statusText->setText(tr("Connection failed: %1").arg(ToQStr(status)));
        }
        else if(host->VersionMismatch)
        {
          statusText->setText(
              tr("Remote server is not running RenderDoc %1").arg(lit(FULL_VERSION_STRING)));
        }
        else if(host->Busy)
        {
          statusText->setText(tr("Remote server in use elsewhere"));
        }
        else if(host->ServerRunning)
        {
          statusText->setText(tr("Remote server ready"));
        }
        else
        {
          if(!host->RunCommand.isEmpty())
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

void MainWindow::OnLogfileLoaded()
{
  ui->action_Save_Log->setEnabled(true);
  ui->action_Close_Log->setEnabled(true);

  // don't allow changing context while log is open
  contextChooser->setEnabled(false);

  statusProgress->setVisible(false);

  setLogHasErrors(!m_Ctx.DebugMessages().empty());

  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) {
    bool hasResolver = r->HasCallstacks();

    GUIInvoke::call([this, hasResolver]() {
      ui->action_Resolve_Symbols->setEnabled(hasResolver);
      ui->action_Resolve_Symbols->setText(hasResolver ? tr("Resolve Symbols")
                                                      : tr("Resolve Symbols - None in log"));
    });
  });

  ui->action_Save_Log->setEnabled(true);

  SetTitle();

  PopulateRecentFiles();

  ToolWindowManager::raiseToolWindow(m_Ctx.GetEventBrowser()->Widget());
}

void MainWindow::OnLogfileClosed()
{
  ui->action_Save_Log->setEnabled(false);
  ui->action_Close_Log->setEnabled(false);

  contextChooser->setEnabled(true);

  statusText->setText(QString());
  statusIcon->setPixmap(QPixmap());
  statusProgress->setVisible(false);

  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

  SetTitle();

  // if the remote sever disconnected during log replay, resort back to a 'disconnected' state
  if(m_Ctx.Replay().CurrentRemote() && !m_Ctx.Replay().CurrentRemote()->ServerRunning)
  {
    statusText->setText(
        tr("Remote server disconnected. To attempt to reconnect please select it again."));
    contextChooser->setText(tr("Replay Context: %1").arg(tr("Local")));
    m_Ctx.Replay().DisconnectFromRemoteServer();
  }
}

void MainWindow::OnEventChanged(uint32_t eventID)
{
}

void MainWindow::RegisterShortcut(const QString &shortcut, QWidget *widget, ShortcutCallback callback)
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

void MainWindow::on_action_Close_Log_triggered()
{
  PromptCloseLog();
}

void MainWindow::on_action_Save_Log_triggered()
{
  PromptSaveLog();
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
    if(m_Ctx.HasEventBrowser())
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
  m_Ctx.Replay().AsyncInvoke([this](IReplayController *r) { r->InitResolver(); });

  ShowProgressDialog(this, tr("Please Wait - Resolving Symbols"), [this]() {
    bool running = true;
    m_Ctx.Replay().BlockInvoke(
        [&running](IReplayController *r) { running = r->HasCallstacks() && !r->InitResolver(); });
    return !running;
  });

  if(m_Ctx.HasAPIInspector())
    m_Ctx.GetAPIInspector()->Refresh();
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
  QDir curDir = QFileInfo(QGuiApplication::applicationFilePath()).absoluteDir();

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

void MainWindow::on_action_Source_on_github_triggered()
{
  QDesktopServices::openUrl(QUrl::fromUserInput(lit("https://github.com/baldurk/renderdoc")));
}

void MainWindow::on_action_Build_Release_downloads_triggered()
{
  QDesktopServices::openUrl(QUrl::fromUserInput(lit("https://renderdoc.org/builds")));
}

void MainWindow::on_actionShow_Tips_triggered()
{
  TipsDialog tipsDialog(m_Ctx, this);
  RDDialog::show(&tipsDialog);
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

  if(!PromptCloseLog())
  {
    event->ignore();
    return;
  }

  for(LiveCapture *live : m_LiveCaptures)
  {
    live->cleanItems();
    live->close();
  }

  SaveLayout(0);
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
    LoadFromFilename(fn, false);
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
