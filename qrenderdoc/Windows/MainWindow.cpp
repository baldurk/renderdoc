/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProgressBar>
#include "Code/CaptureContext.h"
#include "Windows/Dialogs/AboutDialog.h"
#include "EventBrowser.h"
#include "TextureViewer.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(CaptureContext *ctx) : QMainWindow(NULL), ui(new Ui::MainWindow), m_Ctx(ctx)
{
  ui->setupUi(this);

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

  statusIcon = new QLabel(this);
  ui->statusBar->addWidget(statusIcon);

  statusText = new QLabel(this);
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

  SetTitle();

  PopulateRecentFiles();
  PopulateRecentCaptures();

  ui->toolWindowManager->setRubberBandLineWidth(50);
  ui->toolWindowManager->setToolWindowCreateCallback([this](const QString &objectName) -> QWidget * {
    if(objectName == "textureViewer")
    {
      TextureViewer *textureViewer = new TextureViewer(m_Ctx);
      textureViewer->setObjectName("textureViewer");
      return textureViewer;
    }
    else if(objectName == "eventBrowser")
    {
      EventBrowser *eventBrowser = new EventBrowser(m_Ctx);
      eventBrowser->setObjectName("eventBrowser");
      return eventBrowser;
    }

    return NULL;
  });

  bool loaded = LoadLayout(0);

  // create default layout if layout failed to load
  if(!loaded)
  {
    EventBrowser *eventBrowser = new EventBrowser(m_Ctx);
    eventBrowser->setObjectName("eventBrowser");

    ui->toolWindowManager->addToolWindow(eventBrowser, ToolWindowManager::EmptySpace);

    TextureViewer *textureViewer = new TextureViewer(m_Ctx);
    textureViewer->setObjectName("textureViewer");

    ui->toolWindowManager->addToolWindow(
        textureViewer,
        ToolWindowManager::AreaReference(ToolWindowManager::RightOf,
                                         ui->toolWindowManager->areaOf(eventBrowser), 0.75f));
  }
}

MainWindow::~MainWindow()
{
  delete ui;
}

QString MainWindow::GetLayoutPath(int layout)
{
  QString filename = "DefaultLayout.config";

  if(layout > 0)
    filename = QString("Layout%1.config").arg(layout);

  return m_Ctx->ConfigFile(filename);
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
      RDDialog::getOpenFileName(this, "Select Logfile to open", m_Ctx->Config.LastLogPath,
                                "Log Files (*.rdc);;Image Files (*.dds *.hdr *.exr *.bmp *.jpg "
                                "*.jpeg *.png *.tga *.gif *.psd;;All Files (*.*)");

  if(filename != "")
    LoadFromFilename(filename);
}

void MainWindow::LoadFromFilename(const QString &filename)
{
  QFileInfo path(filename);
  QString ext = path.suffix().toLower();

  if(ext == ".rdc")
  {
    LoadLogfile(filename, false, true);
  }
  else if(ext == ".cap")
  {
    // OpenCaptureConfigFile(filename, false);
  }
  else if(ext == ".exe")
  {
    // OpenCaptureConfigFile(filename, true);
  }
  else
  {
    // not a recognised filetype, see if we can load it anyway
    LoadLogfile(filename, false, true);
  }
}

void MainWindow::LoadLogfile(const QString &filename, bool temporary, bool local)
{
  if(PromptCloseLog())
  {
    if(m_Ctx->LogLoading())
      return;

    rdctype::str driver;
    rdctype::str machineIdent;
    ReplaySupport support = eReplaySupport_Unsupported;

    bool remoteReplay =
        !local /*|| (m_Core.Renderer.Remote != null && m_Core.Renderer.Remote.Connected)*/;

    if(local)
    {
      support = RENDERDOC_SupportLocalReplay(filename.toUtf8().data(), &driver, &machineIdent);

      // if the return value suggests remote replay, and it's not already selected, AND the user
      // hasn't
      // previously chosen to always replay locally without being prompted, ask if they'd prefer to
      // switch to a remote context for replaying.
      if(support == eReplaySupport_SuggestRemote && !remoteReplay &&
         !m_Ctx->Config.AlwaysReplayLocally)
      {
        RDDialog::information(NULL, tr("Not Implemented"),
                              tr("Can't suggest a remote host, replaying locally"));
        /*
        var dialog = new Dialogs.SuggestRemoteDialog(driver, machineIdent);

        FillRemotesToolStrip(dialog.RemoteItems, false);

        dialog.ShowDialog();

        if(dialog.Result == Dialogs.SuggestRemoteDialog.SuggestRemoteResult.Cancel)
        {
          return;
        }
        else if(dialog.Result == Dialogs.SuggestRemoteDialog.SuggestRemoteResult.Remote)
        {
          // we only get back here from the dialog once the context switch has begun,
          // so contextChooser will have been disabled.
          // Check once to see if it's enabled before even popping up the dialog in case
          // it has finished already. Otherwise pop up a waiting dialog until it completes
          // one way or another, then process the result.

          if(!contextChooser.Enabled)
          {
            ProgressPopup modal = new ProgressPopup((ModalCloseCallback)delegate
            {
              return contextChooser.Enabled;
            }, false);
            modal.SetModalText("Please Wait - Checking remote connection...");

            modal.ShowDialog();
          }

          remoteReplay = (m_Core.Renderer.Remote != null && m_Core.Renderer.Remote.Connected);

          if(!remoteReplay)
          {
            string remoteMessage = "Failed to make a connection to the remote server.\n\n";

            remoteMessage += "More information may be available in the status bar.";

            MessageBox.Show(remoteMessage, "Couldn't connect to remote server",
        MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
            return;
          }
        }
        else
        {
          // nothing to do - we just continue replaying locally
          // however we need to check if the user selected 'always replay locally' and
          // set that bit as sticky in the config
          if(dialog.AlwaysReplayLocally)
          {
            m_Core.Config.AlwaysReplayLocally = true;

            m_Core.Config.Serialize(Core.ConfigFilename);
          }
        }
        */
      }

      if(remoteReplay)
      {
        support = eReplaySupport_Unsupported;

        QVector<QString> remoteDrivers = {};    // m_Ctx->Renderer.GetRemoteSupport();

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
                .arg("localhost" /*m_Ctx->Renderer.Remote.Hostname*/);

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
      if(remoteReplay && local)
      {
        RDDialog::critical(NULL, tr("Not Implemented"), tr("Not Implemented"));
        return;
        /*
        try
        {
          filename = m_Ctx->Renderer.CopyCaptureToRemote(filename, this);

          // deliberately leave local as true so that we keep referring to the locally saved log

          // some error
          if(filename == "")
            throw new ApplicationException();
        }
        catch(Exception)
        {
          MessageBox.Show("Couldn't copy " + filename + " to remote host for replaying", "Error
        copying to remote",
            MessageBoxButtons.OK, MessageBoxIcon.Error);
          return;
        }
        */
      }

      m_Ctx->LoadLogfile(filename, origFilename, temporary, local);
    }

    if(!remoteReplay)
    {
      m_Ctx->Config.LastLogPath = QFileInfo(filename).dir().absolutePath();
    }

    statusText->setText(tr("Loading ") + origFilename + "...");
  }
}

QString MainWindow::GetSavePath()
{
  QString dir;

  if(m_Ctx->Config.DefaultCaptureSaveDirectory != "")
  {
    if(m_LastSaveCapturePath == "")
      dir = m_Ctx->Config.DefaultCaptureSaveDirectory;
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
    if(m_Ctx->IsLogLocal() && !QFileInfo(m_Ctx->LogFilename()).exists())
    {
      RDDialog::critical(NULL, tr("File not found"),
                         tr("Logfile %1 couldn't be found, cannot save.").arg(m_Ctx->LogFilename()));
      return false;
    }

    bool success = false;

    if(m_Ctx->IsLogLocal())
    {
      // we copy the (possibly) temp log to the desired path, but the log item remains referring to
      // the original path.
      // This ensures that if the user deletes the saved path we can still open or re-save it.
      success = QFile::copy(m_Ctx->LogFilename(), saveFilename);
    }
    else
    {
      // TODO
      // m_Core.Renderer.CopyCaptureFromRemote(m_Core.LogFileName, saveFilename, this);
    }

    if(!success)
    {
      RDDialog::critical(NULL, tr("File not found"), tr("Couldn't save to %1").arg(saveFilename));
      return false;
    }

    PersistantConfig::AddRecentFile(m_Ctx->Config.RecentLogFiles, saveFilename, 10);
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
  if(!m_Ctx->LogLoaded())
    return true;

  QString deletepath = "";
  bool loglocal = false;

  if(m_OwnTempLog)
  {
    QString temppath = m_Ctx->LogFilename();
    loglocal = m_Ctx->IsLogLocal();

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

    if(temppath != m_Ctx->LogFilename() || res == QMessageBox::No)
      deletepath = temppath;
    m_OwnTempLog = false;
    m_SavedTempLog = false;
  }

  CloseLogfile();

  if(!deletepath.isEmpty())
    m_Ctx->Renderer()->DeleteCapture(deletepath, loglocal);

  return true;
}

void MainWindow::CloseLogfile()
{
  m_Ctx->CloseLogfile();

  ui->action_Save_Log->setEnabled(false);
}

void MainWindow::SetTitle(const QString &filename)
{
  QString prefix = "";

  if(m_Ctx != NULL && m_Ctx->LogLoaded())
  {
    prefix = QFileInfo(filename).fileName();
    if(m_Ctx->APIProps().degraded)
      prefix += " !DEGRADED PERFORMANCE!";
    prefix += " - ";
  }

  // TODO
  // if(m_Ctx != NULL && m_Ctx->Renderer.Remote != null)
  // prefix += String.Format("Remote: {0} - ", m_Core.Renderer.Remote.Hostname);

  QString text = prefix + "RenderDoc ";

  // TODO
  /*
  if(OfficialVersion)
    text += VersionString;
  else if(BetaVersion)
    text += String.Format("{0}-beta - {1}", VersionString, GitCommitHash);
  else */
  text +=
      tr("Unofficial release (%1 - %2)").arg(RENDERDOC_GetVersionString()).arg(RENDERDOC_GetCommitHash());

  // TODO
  // if(IsVersionMismatched())
  // text += " - !! VERSION MISMATCH DETECTED !!";

  setWindowTitle(text);
}

void MainWindow::SetTitle()
{
  SetTitle(m_Ctx != NULL ? m_Ctx->LogFilename() : "");
}

void MainWindow::PopulateRecentFiles()
{
  ui->menu_Recent_Logs->clear();

  ui->menu_Recent_Logs->setEnabled(false);

  int idx = 1;
  for(const QString &filename : m_Ctx->Config.RecentLogFiles)
  {
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
  for(const QString &filename : m_Ctx->Config.RecentCaptureSettings)
  {
    ui->menu_Recent_Capture_Settings->addAction("&" + QString::number(idx) + " " + filename,
                                                [this, filename] { recentCapture(filename); });
    idx++;

    ui->menu_Recent_Capture_Settings->setEnabled(true);
  }

  ui->menu_Recent_Capture_Settings->addSeparator();
  ui->menu_Recent_Capture_Settings->addAction(ui->action_Clear_Log_History);
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
      m_Ctx->Config.RecentLogFiles.removeOne(filename);

      PopulateRecentFiles();
    }
  }
}

void MainWindow::recentCapture(const QString &filename)
{
  if(QFileInfo::exists(filename))
  {
    // OpenCaptureConfigFile(filename, false);
  }
  else
  {
    QMessageBox::StandardButton res =
        RDDialog::question(NULL, tr("File not found"),
                           tr("File %1 couldn't be found.\nRemove from recent list?").arg(filename));

    if(res == QMessageBox::Yes)
    {
      m_Ctx->Config.RecentLogFiles.removeOne(filename);

      PopulateRecentFiles();
    }
  }
}

void MainWindow::setProgress(float val)
{
  if(val < 0.0f || val >= 1.0f)
  {
    statusProgress->setVisible(false);
    statusText->setText("");
  }
  else
  {
    statusProgress->setVisible(true);
    statusProgress->setValue(1000 * val);
  }
}

void MainWindow::OnLogfileLoaded()
{
  // TODO
  // don't allow changing context while log is open
  // contextChooser.Enabled = false;

  // LogHasErrors = (m_Core.DebugMessages.Count > 0);

  statusProgress->setVisible(false);

  m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) {
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

  // m_Core.GetEventBrowser().Focus();
}

void MainWindow::OnLogfileClosed()
{
  // contextChooser.Enabled = true;

  statusText->setText("");
  statusIcon->setPixmap(QPixmap());
  statusProgress->setVisible(false);

  ui->action_Resolve_Symbols->setEnabled(false);
  ui->action_Resolve_Symbols->setText(tr("Resolve Symbols"));

  SetTitle();

  // if the remote sever disconnected during log replay, resort back to a 'disconnected' state
  // TODO
  /*
  if(m_Core.Renderer.Remote != null && !m_Core.Renderer.Remote.ServerRunning)
  {
    statusText.Text = "Remote server disconnected. To attempt to reconnect please select it again.";
    contextChooser.Text = "Replay Context: Local";
    m_Core.Renderer.DisconnectFromRemoteServer();
  }
  */
}

void MainWindow::OnEventSelected(uint32_t eventID)
{
}

void MainWindow::on_action_Close_Log_triggered()
{
  PromptCloseLog();
}

void MainWindow::on_action_About_triggered()
{
  AboutDialog about(this);
  RDDialog::show(&about);
}

void MainWindow::on_action_Mesh_Output_triggered()
{
}

void MainWindow::on_action_Event_Viewer_triggered()
{
  EventBrowser *eventBrowser = new EventBrowser(m_Ctx);
  eventBrowser->setObjectName("eventBrowser");

  ui->toolWindowManager->addToolWindow(eventBrowser, ToolWindowManager::EmptySpace);
}

void MainWindow::on_action_Texture_Viewer_triggered()
{
  TextureViewer *textureViewer = new TextureViewer(m_Ctx);
  textureViewer->setObjectName("textureViewer");

  ui->toolWindowManager->addToolWindow(textureViewer, ToolWindowManager::EmptySpace);
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
  SaveLayout(0);
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

  // marker that this is indeed a valid state to load from
  state["renderdocLayoutData"] = 1;

  QFile f(path);
  if(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
  {
    QJsonDocument doc = QJsonDocument::fromVariant(state);

    if(doc.isEmpty() || doc.isNull())
    {
      qCritical() << "Failed to convert state data to JSON document";
      return false;
    }

    QByteArray jsontext = doc.toJson(QJsonDocument::Indented);

    qint64 ret = f.write(jsontext);

    if(ret != jsontext.size())
    {
      qCritical() << "Failed to write JSON data to file: " << ret << " " << f.errorString();
      return false;
    }

    return true;
  }

  qWarning() << "Couldn't write to " << path << " " << f.errorString();

  return false;
}

bool MainWindow::LoadLayout(int layout)
{
  QString path = GetLayoutPath(layout);

  QFile f(path);
  if(f.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QByteArray json = f.readAll();

    if(json.isEmpty())
    {
      qCritical() << "Read invalid empty JSON data from file " << f.errorString();
      return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(json);

    if(doc.isEmpty() || doc.isNull())
    {
      qCritical() << "Failed to convert file to JSON document";
      return false;
    }

    QVariantMap state = doc.toVariant().toMap();

    if(state.isEmpty() || !state.contains("renderdocLayoutData"))
    {
      qCritical() << "Converted state data is invalid or unrecognised";
      return false;
    }

    return restoreState(state);
  }

  qInfo() << "Couldn't load layout from " << path << " " << f.errorString();

  return false;
}
