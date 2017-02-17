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
#include <QProgressBar>
#include <QProgressDialog>
#include <QToolButton>
#include "Code/CaptureContext.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "PipelineState/PipelineStateViewer.h"
#include "Resources/resource.h"
#include "Widgets/Extended/RDLabel.h"
#include "Windows/Dialogs/AboutDialog.h"
#include "Windows/Dialogs/CaptureDialog.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/Dialogs/RemoteManager.h"
#include "Windows/Dialogs/SettingsDialog.h"
#include "Windows/Dialogs/SuggestRemoteDialog.h"
#include "APIInspector.h"
#include "BufferViewer.h"
#include "ConstantBufferPreviewer.h"
#include "DebugMessageView.h"
#include "EventBrowser.h"
#include "StatisticsViewer.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"
#include "version.h"

#define JSON_ID "rdocLayoutData"
#define JSON_VER 1

struct Version
{
  static bool isOfficialVersion() { return QString(GIT_COMMIT_HASH).indexOf("-official") > 0; }
  static bool isBetaVersion() { return QString(GIT_COMMIT_HASH).indexOf("-beta") > 0; }
  static QString bareString() { return RENDERDOC_VERSION_STRING; }
  static QString string() { return "v" RENDERDOC_VERSION_STRING; }
  static QString gitCommitHash()
  {
    return QString(GIT_COMMIT_HASH).replace("-official", "").replace("-beta", "");
  }

  static bool isMismatched() { return RENDERDOC_GetVersionString() != bareString(); }
};

#if defined(Q_OS_WIN32)
extern "C" void *GetModuleHandleA(const char *);
#endif

MainWindow::MainWindow(CaptureContext &ctx) : QMainWindow(NULL), ui(new Ui::MainWindow), m_Ctx(ctx)
{
  ui->setupUi(this);

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

  contextChooserMenu = new QMenu(this);

  FillRemotesMenu(contextChooserMenu, true);

  contextChooser = new QToolButton(this);
  contextChooser->setText(tr("Replay Context: %1").arg("Local"));
  contextChooser->setIcon(Icons::house());
  contextChooser->setAutoRaise(true);
  contextChooser->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  contextChooser->setPopupMode(QToolButton::InstantPopup);
  contextChooser->setMenu(contextChooserMenu);
  contextChooser->setStyleSheet("QToolButton::menu-indicator { image: none; }");
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

  statusIcon->setText("");
  statusIcon->setPixmap(QPixmap());
  statusText->setText("");

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

  ui->statusBar->setStyleSheet("QStatusBar::item { border: 0px }");

  SetTitle();

  PopulateRecentFiles();
  PopulateRecentCaptures();

  ui->toolWindowManager->setRubberBandLineWidth(50);
  ui->toolWindowManager->setToolWindowCreateCallback(
      [this](const QString &objectName) -> QWidget * { return m_Ctx.createToolWindow(objectName); });

  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

  bool loaded = LoadLayout(0);

  LambdaThread *th = new LambdaThread([this]() {
    m_Ctx.Config.AddAndroidHosts();
    for(RemoteHost *host : m_Ctx.Config.RemoteHosts)
      host->CheckStatus();
  });
  th->selfDelete(true);
  th->start();

  // create default layout if layout failed to load
  if(!loaded)
  {
    EventBrowser *eventBrowser = m_Ctx.eventBrowser();

    ui->toolWindowManager->addToolWindow(eventBrowser, ToolWindowManager::EmptySpace);

    TextureViewer *textureViewer = m_Ctx.textureViewer();

    ui->toolWindowManager->addToolWindow(
        textureViewer,
        ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                         ui->toolWindowManager->areaOf(eventBrowser), 0.75f));

    PipelineStateViewer *pipe = m_Ctx.pipelineViewer();

    ui->toolWindowManager->addToolWindow(
        pipe, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                               ui->toolWindowManager->areaOf(textureViewer)));

    BufferViewer *mesh = m_Ctx.meshPreview();

    ui->toolWindowManager->addToolWindow(
        mesh, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                               ui->toolWindowManager->areaOf(textureViewer)));

    CaptureDialog *capDialog = m_Ctx.captureDialog();

    ui->toolWindowManager->addToolWindow(
        capDialog, ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                                    ui->toolWindowManager->areaOf(textureViewer)));

    APIInspector *apiInspector = m_Ctx.apiInspector();

    ui->toolWindowManager->addToolWindow(
        apiInspector,
        ToolWindowManager::AreaReference(ToolWindowManager::BottomOf,
                                         ui->toolWindowManager->areaOf(eventBrowser), 0.3f));
  }

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
  QString filename = "DefaultLayout.config";

  if(layout > 0)
    filename = QString("Layout%1.config").arg(layout);

  return m_Ctx.ConfigFile(filename);
}

void MainWindow::on_action_Exit_triggered()
{
  this->close();
}

void MainWindow::on_action_Open_Log_triggered()
{
  if(!PromptCloseLog())
    return;

  QString filename =
      RDDialog::getOpenFileName(this, "Select Logfile to open", m_Ctx.Config.LastLogPath,
                                "Log Files (*.rdc);;Image Files (*.dds *.hdr *.exr *.bmp *.jpg "
                                "*.jpeg *.png *.tga *.gif *.psd;;All Files (*.*)");

  if(filename != "")
    LoadFromFilename(filename);
}

void MainWindow::LoadFromFilename(const QString &filename)
{
  QFileInfo path(filename);
  QString ext = path.suffix().toLower();

  if(ext == "rdc")
  {
    LoadLogfile(filename, false, true);
  }
  else if(ext == "cap")
  {
    OpenCaptureConfigFile(filename, false);
  }
  else if(ext == "exe")
  {
    OpenCaptureConfigFile(filename, true);
  }
  else
  {
    // not a recognised filetype, see if we can load it anyway
    LoadLogfile(filename, false, true);
  }
}

LiveCapture *MainWindow::OnCaptureTrigger(const QString &exe, const QString &workingDir,
                                          const QString &cmdLine,
                                          const QList<EnvironmentModification> &env,
                                          CaptureOptions opts)
{
  if(!PromptCloseLog())
    return NULL;

  QString logfile = m_Ctx.TempLogFilename(QFileInfo(exe).baseName());

  uint32_t ret = m_Ctx.Renderer().ExecuteAndInject(exe, workingDir, cmdLine, env, logfile, opts);

  if(ret == 0)
  {
    RDDialog::critical(
        this, tr("Error kicking capture"),
        tr("Error launching %1 for capture.\n\nCheck diagnostic log in Help menu for more details.")
            .arg(exe));
    return NULL;
  }

  LiveCapture *live = new LiveCapture(
      m_Ctx, m_Ctx.Renderer().remote() ? m_Ctx.Renderer().remote()->Hostname : "", ret, this, this);
  ShowLiveCapture(live);
  return live;
}

LiveCapture *MainWindow::OnInjectTrigger(uint32_t PID, const QList<EnvironmentModification> &env,
                                         const QString &name, CaptureOptions opts)
{
  if(!PromptCloseLog())
    return NULL;

  QString logfile = m_Ctx.TempLogFilename(name);

  void *envList = RENDERDOC_MakeEnvironmentModificationList(env.size());

  for(int i = 0; i < env.size(); i++)
    RENDERDOC_SetEnvironmentModification(envList, i, env[i].variable.toUtf8().data(),
                                         env[i].value.toUtf8().data(), env[i].type, env[i].separator);

  uint32_t ret = RENDERDOC_InjectIntoProcess(PID, envList, logfile.toUtf8().data(), &opts, false);

  RENDERDOC_FreeEnvironmentModificationList(envList);

  if(ret == 0)
  {
    RDDialog::critical(this, tr("Error kicking capture"),
                       tr("Error injecting into process %1 for capture.\n\nCheck diagnostic log in "
                          "Help menu for more details.")
                           .arg(PID));
    return NULL;
  }

  LiveCapture *live = new LiveCapture(m_Ctx, "", ret, this, this);
  ShowLiveCapture(live);
  return live;
}

void MainWindow::LoadLogfile(const QString &filename, bool temporary, bool local)
{
  if(PromptCloseLog())
  {
    if(m_Ctx.LogLoading())
      return;

    rdctype::str driver;
    rdctype::str machineIdent;
    ReplaySupport support = eReplaySupport_Unsupported;

    bool remoteReplay = !local || (m_Ctx.Renderer().remote() && m_Ctx.Renderer().remote()->Connected);

    if(local)
    {
      support = RENDERDOC_SupportLocalReplay(filename.toUtf8().data(), &driver, &machineIdent);

      // if the return value suggests remote replay, and it's not already selected, AND the user
      // hasn't previously chosen to always replay locally without being prompted, ask if they'd
      // prefer to switch to a remote context for replaying.
      if(support == eReplaySupport_SuggestRemote && !remoteReplay && !m_Ctx.Config.AlwaysReplayLocally)
      {
        SuggestRemoteDialog dialog(ToQStr(driver), ToQStr(machineIdent), this);

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

          remoteReplay = (m_Ctx.Renderer().remote() && m_Ctx.Renderer().remote()->Connected);

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
            m_Ctx.Config.AlwaysReplayLocally = true;

            m_Ctx.Config.Save();
          }
        }
      }

      if(remoteReplay)
      {
        support = eReplaySupport_Unsupported;

        QStringList remoteDrivers = m_Ctx.Renderer().GetRemoteSupport();

        for(const QString &d : remoteDrivers)
        {
          if(driver == d)
            support = eReplaySupport_Supported;
        }
      }
    }

    QString origFilename = filename;

    // if driver is empty something went wrong loading the log, let it be handled as usual
    // below. Otherwise indicate that support is missing.
    if(driver.count > 0 && support == eReplaySupport_Unsupported)
    {
      if(remoteReplay)
      {
        QString remoteMessage =
            tr("This log was captured with %1 and cannot be replayed on %2.\n\n")
                .arg(driver.c_str())
                .arg(m_Ctx.Renderer().remote()->Hostname);

        remoteMessage += "Try selecting a different remote context in the status bar.";

        RDDialog::critical(NULL, tr("Unsupported logfile type"), remoteMessage);
      }
      else
      {
        QString remoteMessage =
            tr("This log was captured with %1 and cannot be replayed locally.\n\n").arg(driver.c_str());

        remoteMessage += "Try selecting a remote context in the status bar.";

        RDDialog::critical(NULL, tr("Unsupported logfile type"), remoteMessage);
      }

      return;
    }
    else
    {
      QString fileToLoad = filename;

      if(remoteReplay && local)
      {
        fileToLoad = m_Ctx.Renderer().CopyCaptureToRemote(filename, this);

        // deliberately leave local as true so that we keep referring to the locally saved log

        // some error
        if(fileToLoad == "")
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
      m_Ctx.Config.LastLogPath = QFileInfo(filename).absolutePath();
    }

    statusText->setText(tr("Loading ") + origFilename + "...");
  }
}

void MainWindow::OpenCaptureConfigFile(const QString &filename, bool exe)
{
  CaptureDialog *capDialog = m_Ctx.captureDialog();

  if(exe)
    capDialog->setExecutableFilename(filename);
  else
    capDialog->loadSettings(filename);

  if(!ui->toolWindowManager->toolWindows().contains(capDialog))
    ui->toolWindowManager->addToolWindow(capDialog, mainToolArea());
}

QString MainWindow::GetSavePath()
{
  QString dir;

  if(m_Ctx.Config.DefaultCaptureSaveDirectory != "")
  {
    if(m_LastSaveCapturePath == "")
      dir = m_Ctx.Config.DefaultCaptureSaveDirectory;
    else
      dir = m_LastSaveCapturePath;
  }

  QString filename =
      RDDialog::getSaveFileName(this, tr("Save Capture As"), dir, "Log Files (*.rdc)");

  if(filename != "")
  {
    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
      m_LastSaveCapturePath = dirinfo.absolutePath();

    return filename;
  }

  return "";
}

bool MainWindow::PromptSaveLog()
{
  QString saveFilename = GetSavePath();

  if(saveFilename != "")
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
      success = QFile::copy(m_Ctx.LogFilename(), saveFilename);

      error = tr("Couldn't save to %1").arg(saveFilename);
    }
    else
    {
      m_Ctx.Renderer().CopyCaptureFromRemote(m_Ctx.LogFilename(), saveFilename, this);
      success = QFile::exists(saveFilename);

      error = tr("File couldn't be transferred from remote host");
    }

    if(!success)
    {
      RDDialog::critical(NULL, tr("Error Saving"), error);
      return false;
    }

    PersistantConfig::AddRecentFile(m_Ctx.Config.RecentLogFiles, saveFilename, 10);
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

  QString deletepath = "";
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
    m_Ctx.Renderer().DeleteCapture(deletepath, loglocal);

  return true;
}

void MainWindow::CloseLogfile()
{
  m_Ctx.CloseLogfile();

  ui->action_Save_Log->setEnabled(false);
}

void MainWindow::SetTitle(const QString &filename)
{
  QString prefix = "";

  if(m_Ctx.LogLoaded())
  {
    prefix = QFileInfo(filename).fileName();
    if(m_Ctx.APIProps().degraded)
      prefix += " !DEGRADED PERFORMANCE!";
    prefix += " - ";
  }

  if(m_Ctx.Renderer().remote())
    prefix += tr("Remote: %1 - ").arg(m_Ctx.Renderer().remote()->Hostname);

  QString text = prefix + "RenderDoc ";

  if(Version::isOfficialVersion())
    text += Version::string();
  else if(Version::isBetaVersion())
    text += tr("%1-beta - %2").arg(Version::string()).arg(Version::gitCommitHash());
  else
    text += tr("Unofficial release (%1 - %2)").arg(Version::string()).arg(Version::gitCommitHash());

  if(Version::isMismatched())
    text += " - !! VERSION MISMATCH DETECTED !!";

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
  for(int i = m_Ctx.Config.RecentLogFiles.size() - 1; i >= 0; i--)
  {
    const QString &filename = m_Ctx.Config.RecentLogFiles[i];
    ui->menu_Recent_Logs->addAction("&" + QString::number(idx) + " " + filename,
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
  for(int i = m_Ctx.Config.RecentCaptureSettings.size() - 1; i >= 0; i--)
  {
    const QString &filename = m_Ctx.Config.RecentCaptureSettings[i];
    ui->menu_Recent_Capture_Settings->addAction("&" + QString::number(idx) + " " + filename,
                                                [this, filename] { recentCapture(filename); });
    idx++;

    ui->menu_Recent_Capture_Settings->setEnabled(true);
  }

  ui->menu_Recent_Capture_Settings->addSeparator();
  ui->menu_Recent_Capture_Settings->addAction(ui->action_Clear_Log_History);
}

void MainWindow::ShowLiveCapture(LiveCapture *live)
{
  live->setWindowIcon(m_Ctx.winIcon());
  m_LiveCaptures.push_back(live);
  ui->toolWindowManager->addToolWindow(live, mainToolArea());
}

void MainWindow::LiveCaptureClosed(LiveCapture *live)
{
  m_LiveCaptures.removeOne(live);
}

ToolWindowManager::AreaReference MainWindow::mainToolArea()
{
  // bit of a hack. Maybe the ToolWindowManager should track this?
  // Try and identify where to add new windows, by searching a
  // priority list of other windows to use their area
  if(m_Ctx.hasTextureViewer() && ui->toolWindowManager->toolWindows().contains(m_Ctx.textureViewer()))
    return ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                            ui->toolWindowManager->areaOf(m_Ctx.textureViewer()));
  else if(m_Ctx.hasPipelineViewer() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.pipelineViewer()))
    return ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                            ui->toolWindowManager->areaOf(m_Ctx.pipelineViewer()));
  else if(m_Ctx.hasMeshPreview() && ui->toolWindowManager->toolWindows().contains(m_Ctx.meshPreview()))
    return ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                            ui->toolWindowManager->areaOf(m_Ctx.meshPreview()));
  else if(m_Ctx.hasCaptureDialog() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.captureDialog()))
    return ToolWindowManager::AreaReference(ToolWindowManager::AddTo,
                                            ui->toolWindowManager->areaOf(m_Ctx.captureDialog()));

  // if all else fails just add to the last place we placed something.
  return ToolWindowManager::AreaReference(ToolWindowManager::LastUsedArea);
}

ToolWindowManager::AreaReference MainWindow::leftToolArea()
{
  // see mainToolArea()
  if(m_Ctx.hasTextureViewer() && ui->toolWindowManager->toolWindows().contains(m_Ctx.textureViewer()))
    return ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                            ui->toolWindowManager->areaOf(m_Ctx.textureViewer()));
  else if(m_Ctx.hasPipelineViewer() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.pipelineViewer()))
    return ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                            ui->toolWindowManager->areaOf(m_Ctx.pipelineViewer()));
  else if(m_Ctx.hasCaptureDialog() &&
          ui->toolWindowManager->toolWindows().contains(m_Ctx.captureDialog()))
    return ToolWindowManager::AreaReference(ToolWindowManager::LeftOf,
                                            ui->toolWindowManager->areaOf(m_Ctx.captureDialog()));

  return ToolWindowManager::AreaReference(ToolWindowManager::LastUsedArea);
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
      m_Ctx.Config.RecentLogFiles.removeOne(filename);

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
      m_Ctx.Config.RecentLogFiles.removeOne(filename);

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
    QPixmap empty(Pixmaps::del().width(), Pixmaps::del().height());
    empty.fill(Qt::transparent);
    statusIcon->setPixmap(m_messageAlternate ? empty : Pixmaps::del());

    QString text;
    text = tr("%1 loaded. Log has %2 errors, warnings or performance notes. "
              "See the 'Log Errors and Warnings' window.")
               .arg(filename)
               .arg(m_Ctx.DebugMessages.size());
    if(m_Ctx.UnreadMessageCount > 0)
      text += tr(" %1 Unread.").arg(m_Ctx.UnreadMessageCount);
    statusText->setText(text);
  }
  else
  {
    statusIcon->setPixmap(Pixmaps::tick());
    statusText->setText(tr("%1 loaded. No problems detected.").arg(filename));
  }
}

void MainWindow::remoteProbe()
{
  if(!m_Ctx.LogLoaded() && !m_Ctx.LogLoading())
  {
    for(RemoteHost *host : m_Ctx.Config.RemoteHosts)
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
    m_Ctx.Renderer().AsyncInvoke([this](IReplayRenderer *r) {
      rdctype::array<DebugMessage> msgs;
      r->GetDebugMessages(&msgs);

      bool disconnected = false;

      if(m_Ctx.Renderer().remote())
      {
        bool prev = m_Ctx.Renderer().remote()->ServerRunning;

        m_Ctx.Renderer().PingRemote();

        if(prev != m_Ctx.Renderer().remote()->ServerRunning)
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

        if(m_Ctx.Renderer().remote() && !m_Ctx.Renderer().remote()->ServerRunning)
          contextChooser->setIcon(Icons::cross());

        if(!msgs.empty())
        {
          m_Ctx.AddMessages(msgs);
          m_Ctx.debugMessageView()->RefreshMessageList();
        }

        if(m_Ctx.UnreadMessageCount > 0)
          m_messageAlternate = !m_messageAlternate;
        else
          m_messageAlternate = false;

        setLogHasErrors(!m_Ctx.DebugMessages.empty());
      });
    });
  }
  else if(!m_Ctx.LogLoaded() && !m_Ctx.LogLoading())
  {
    if(m_Ctx.Renderer().remote())
      m_Ctx.Renderer().PingRemote();

    GUIInvoke::call([this]() {
      if(m_Ctx.Renderer().remote() && !m_Ctx.Renderer().remote()->ServerRunning)
      {
        contextChooser->setIcon(Icons::cross());
        contextChooser->setText(tr("Replay Context: %1").arg("Local"));
        statusText->setText(
            tr("Remote server disconnected. To attempt to reconnect please select it again."));

        m_Ctx.Renderer().DisconnectFromRemoteServer();
      }
    });
  }
}

void MainWindow::FillRemotesMenu(QMenu *menu, bool includeLocalhost)
{
  menu->clear();

  for(int i = 0; i < m_Ctx.Config.RemoteHosts.count(); i++)
  {
    RemoteHost *host = m_Ctx.Config.RemoteHosts[i];

    // add localhost at the end
    if(host->Hostname == "localhost")
      continue;

    QAction *action = new QAction(menu);

    action->setIcon(host->ServerRunning && !host->VersionMismatch ? Icons::tick() : Icons::cross());
    if(host->Connected)
      action->setText(tr("%1 (Connected)").arg(host->Hostname));
    else if(host->ServerRunning && host->VersionMismatch)
      action->setText(tr("%1 (Bad Version)").arg(host->Hostname));
    else if(host->ServerRunning && host->Busy)
      action->setText(tr("%1 (Busy)").arg(host->Hostname));
    else if(host->ServerRunning)
      action->setText(tr("%1 (Online)").arg(host->Hostname));
    else
      action->setText(tr("%1 (Offline)").arg(host->Hostname));
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

    localContext->setText("Local");
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

  if(hostIdx >= 0 && hostIdx < m_Ctx.Config.RemoteHosts.count())
  {
    host = m_Ctx.Config.RemoteHosts[hostIdx];
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

  m_Ctx.Renderer().DisconnectFromRemoteServer();

  if(!host)
  {
    contextChooser->setIcon(Icons::house());
    contextChooser->setText(tr("Replay Context: %1").arg("Local"));

    ui->action_Inject_into_Process->setEnabled(true);

    statusText->setText("");

    SetTitle();
  }
  else
  {
    contextChooser->setText(tr("Replay Context: %1").arg(host->Hostname));
    contextChooser->setIcon(host->ServerRunning ? Icons::connect() : Icons::disconnect());

    // disable until checking is done
    contextChooser->setEnabled(false);

    ui->action_Inject_into_Process->setEnabled(false);

    SetTitle();

    statusText->setText(tr("Checking remote server status..."));

    LambdaThread *th = new LambdaThread([this, host]() {
      // see if the server is up
      host->CheckStatus();

      if(!host->ServerRunning && host->RunCommand != "")
      {
        GUIInvoke::call([this]() { statusText->setText(tr("Running remote server command...")); });

        host->Launch();

        // check if it's running now
        host->CheckStatus();
      }

      ReplayCreateStatus status = eReplayCreate_Success;

      if(host->ServerRunning && !host->Busy)
      {
        status = m_Ctx.Renderer().ConnectToRemoteServer(host);
      }

      GUIInvoke::call([this, host, status]() {
        contextChooser->setIcon(host->ServerRunning && !host->Busy ? Icons::connect()
                                                                   : Icons::disconnect());

        if(status != eReplayCreate_Success)
        {
          contextChooser->setIcon(Icons::cross());
          contextChooser->setText(tr("Replay Context: %1").arg("Local"));
          statusText->setText(tr("Connection failed: %1").arg(ToQStr(status)));
        }
        else if(host->VersionMismatch)
        {
          statusText->setText(tr("Remote server is not running RenderDoc %1").arg(Version::string()));
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
          if(host->RunCommand != "")
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

void MainWindow::statusDoubleClicked()
{
  showDebugMessageView();
}

void MainWindow::OnLogfileLoaded()
{
  // don't allow changing context while log is open
  contextChooser->setEnabled(false);

  statusProgress->setVisible(false);

  setLogHasErrors(!m_Ctx.DebugMessages.empty());

  m_Ctx.Renderer().AsyncInvoke([this](IReplayRenderer *r) {
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

  ToolWindowManager::raiseToolWindow(m_Ctx.eventBrowser());
}

void MainWindow::OnLogfileClosed()
{
  contextChooser->setEnabled(true);

  statusText->setText("");
  statusIcon->setPixmap(QPixmap());
  statusProgress->setVisible(false);

  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

  SetTitle();

  // if the remote sever disconnected during log replay, resort back to a 'disconnected' state
  if(m_Ctx.Renderer().remote() && !m_Ctx.Renderer().remote()->ServerRunning)
  {
    statusText->setText(
        tr("Remote server disconnected. To attempt to reconnect please select it again."));
    contextChooser->setText(tr("Replay Context: %1").arg("Local"));
    m_Ctx.Renderer().DisconnectFromRemoteServer();
  }
}

void MainWindow::OnEventChanged(uint32_t eventID)
{
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
  BufferViewer *meshPreview = m_Ctx.meshPreview();

  if(ui->toolWindowManager->toolWindows().contains(meshPreview))
    ToolWindowManager::raiseToolWindow(meshPreview);
  else
    ui->toolWindowManager->addToolWindow(meshPreview, mainToolArea());
}

void MainWindow::on_action_API_Inspector_triggered()
{
  APIInspector *apiInspector = m_Ctx.apiInspector();

  if(ui->toolWindowManager->toolWindows().contains(apiInspector))
  {
    ToolWindowManager::raiseToolWindow(apiInspector);
  }
  else
  {
    if(m_Ctx.hasEventBrowser())
    {
      ToolWindowManager::AreaReference ref(ToolWindowManager::BottomOf,
                                           ui->toolWindowManager->areaOf(m_Ctx.eventBrowser()));
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
  EventBrowser *eventBrowser = m_Ctx.eventBrowser();

  if(ui->toolWindowManager->toolWindows().contains(eventBrowser))
    ToolWindowManager::raiseToolWindow(eventBrowser);
  else
    ui->toolWindowManager->addToolWindow(eventBrowser, leftToolArea());
}

void MainWindow::on_action_Texture_Viewer_triggered()
{
  TextureViewer *textureViewer = m_Ctx.textureViewer();

  if(ui->toolWindowManager->toolWindows().contains(textureViewer))
    ToolWindowManager::raiseToolWindow(textureViewer);
  else
    ui->toolWindowManager->addToolWindow(textureViewer, mainToolArea());
}

void MainWindow::on_action_Pipeline_State_triggered()
{
  PipelineStateViewer *pipelineViewer = m_Ctx.pipelineViewer();

  if(ui->toolWindowManager->toolWindows().contains(pipelineViewer))
    ToolWindowManager::raiseToolWindow(pipelineViewer);
  else
    ui->toolWindowManager->addToolWindow(pipelineViewer, mainToolArea());
}

void MainWindow::on_action_Capture_Log_triggered()
{
  CaptureDialog *capDialog = m_Ctx.captureDialog();

  capDialog->setInjectMode(false);

  if(ui->toolWindowManager->toolWindows().contains(capDialog))
    ToolWindowManager::raiseToolWindow(capDialog);
  else
    ui->toolWindowManager->addToolWindow(capDialog, mainToolArea());
}

void MainWindow::on_action_Inject_into_Process_triggered()
{
  CaptureDialog *capDialog = m_Ctx.captureDialog();

  capDialog->setInjectMode(true);

  if(ui->toolWindowManager->toolWindows().contains(capDialog))
    ToolWindowManager::raiseToolWindow(capDialog);
  else
    ui->toolWindowManager->addToolWindow(capDialog, mainToolArea());
}

void MainWindow::on_action_Errors_and_Warnings_triggered()
{
  DebugMessageView *debugMessages = m_Ctx.debugMessageView();

  if(ui->toolWindowManager->toolWindows().contains(debugMessages))
    ToolWindowManager::raiseToolWindow(debugMessages);
  else
    ui->toolWindowManager->addToolWindow(debugMessages, mainToolArea());
}

void MainWindow::on_action_Statistics_Viewer_triggered()
{
  StatisticsViewer *stats = m_Ctx.statisticsViewer();

  if(ui->toolWindowManager->toolWindows().contains(stats))
    ToolWindowManager::raiseToolWindow(stats);
  else
    ui->toolWindowManager->addToolWindow(stats, mainToolArea());
}

void MainWindow::on_action_Resolve_Symbols_triggered()
{
  m_Ctx.Renderer().AsyncInvoke([this](IReplayRenderer *r) { r->InitResolver(); });

  ShowProgressDialog(this, tr("Please Wait - Resolving Symbols"), [this]() {
    bool running = true;
    m_Ctx.Renderer().BlockInvoke(
        [&running](IReplayRenderer *r) { running = r->HasCallstacks() && !r->InitResolver(); });
    return !running;
  });

  if(m_Ctx.hasAPIInspector())
    m_Ctx.apiInspector()->on_apiEvents_itemSelectionChanged();
}

void MainWindow::on_action_Attach_to_Running_Instance_triggered()
{
  on_action_Manage_Remote_Servers_triggered();
}

void MainWindow::on_action_Manage_Remote_Servers_triggered()
{
  // the manager deletes itself when all lookups terminate
  RDDialog::show(new RemoteManager(m_Ctx, this));
}

void MainWindow::on_action_Start_Android_Remote_Server_triggered()
{
  RENDERDOC_StartAndroidRemoteServer();
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

  if(fi.absoluteDir().exists("renderdoc.chm"))
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(fi.absoluteDir().absoluteFilePath("renderdoc.chm")));
  else
    QDesktopServices::openUrl(QUrl::fromUserInput("https://renderdoc.org/docs"));
}

void MainWindow::on_action_View_Diagnostic_Log_File_triggered()
{
  QString logPath = QString::fromUtf8(RENDERDOC_GetLogFile());
  if(QFileInfo::exists(logPath))
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void MainWindow::on_action_Source_on_github_triggered()
{
  QDesktopServices::openUrl(QUrl::fromUserInput("https://github.com/baldurk/renderdoc"));
}

void MainWindow::on_action_Build_Release_downloads_triggered()
{
  QDesktopServices::openUrl(QUrl::fromUserInput("https://renderdoc.org/builds"));
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

  return "";
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if(dragFilename(event->mimeData()) != "")
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
  QString fn = dragFilename(event->mimeData());
  if(fn != "")
    LoadFromFilename(fn);
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
      RDDialog::critical(this, "Error saving layout", "Couldn't save layout");
    else
      RDDialog::critical(this, "Error loading layout", "Couldn't load layout");
  }
}

QVariantMap MainWindow::saveState()
{
  QVariantMap state = ui->toolWindowManager->saveState();

  state["mainWindowGeometry"] = saveGeometry().toBase64();

  return state;
}

bool MainWindow::restoreState(QVariantMap &state)
{
  restoreGeometry(QByteArray::fromBase64(state["mainWindowGeometry"].toByteArray()));

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
