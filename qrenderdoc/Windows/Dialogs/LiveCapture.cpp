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

#include "LiveCapture.h"
#include <QMenu>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QToolBar>
#include <QToolButton>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/Resources.h"
#include "Code/qprocessinfo.h"
#include "Widgets/Extended/RDLabel.h"
#include "Windows/MainWindow.h"
#include "ui_LiveCapture.h"

static const int PIDRole = Qt::UserRole + 1;
static const int IdentRole = Qt::UserRole + 2;
static const int LogPtrRole = Qt::UserRole + 3;

class NameEditOnlyDelegate : public QStyledItemDelegate
{
public:
  LiveCapture *live;
  NameEditOnlyDelegate(LiveCapture *l) : live(l) {}
  void setEditorData(QWidget *editor, const QModelIndex &index) const override
  {
    QByteArray n = editor->metaObject()->userProperty().name();
    QListWidgetItem *item = live->ui->captures->item(index.row());

    if(!n.isEmpty() && item)
    {
      LiveCapture::CaptureLog *log = live->GetLog(item);
      if(log)
        editor->setProperty(n, log->name);
    }
  }

  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
  {
    QByteArray n = editor->metaObject()->userProperty().name();
    QListWidgetItem *item = live->ui->captures->item(index.row());

    if(!n.isEmpty() && item)
    {
      LiveCapture::CaptureLog *log = live->GetLog(item);
      if(log)
      {
        log->name = editor->property(n).toString();
        item->setText(live->MakeText(log));
      }
    }
  }
};

LiveCapture::LiveCapture(ICaptureContext &ctx, const QString &hostname, const QString &friendlyname,
                         uint32_t ident, MainWindow *main, QWidget *parent)
    : QFrame(parent),
      ui(new Ui::LiveCapture),
      m_Ctx(ctx),
      m_Hostname(hostname),
      m_HostFriendlyname(friendlyname),
      m_RemoteIdent(ident),
      m_Main(main)
{
  ui->setupUi(this);

  m_Disconnect.release();

  QObject::connect(&childUpdateTimer, &QTimer::timeout, this, &LiveCapture::childUpdate);
  childUpdateTimer.setSingleShot(false);
  childUpdateTimer.setInterval(1000);
  childUpdateTimer.start();

  QObject::connect(&countdownTimer, &QTimer::timeout, this, &LiveCapture::captureCountdownTick);
  countdownTimer.setSingleShot(true);
  countdownTimer.setInterval(1000);

  childUpdate();

  ui->previewSplit->setCollapsible(1, true);
  ui->previewSplit->setSizes({1, 0});

  QObject::connect(ui->preview, &RDLabel::clicked, this, &LiveCapture::preview_mouseClick);
  QObject::connect(ui->preview, &RDLabel::mouseMoved, this, &LiveCapture::preview_mouseMove);

  ui->preview->setMouseTracking(true);

  setTitle(tr("Connecting.."));
  ui->connectionStatus->setText(tr("Connecting.."));
  ui->connectionIcon->setPixmap(Pixmaps::hourglass(ui->connectionIcon));

  ui->captures->setItemDelegate(new NameEditOnlyDelegate(this));

  {
    QToolBar *bottomTools = new QToolBar(this);

    QWidget *rightAlign = new QWidget(this);
    rightAlign->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    bottomTools->addWidget(rightAlign);

    previewToggle = new QAction(tr("Preview"), this);
    previewToggle->setCheckable(true);

    bottomTools->addAction(previewToggle);

    QMenu *openMenu = new QMenu(tr("&Open in..."), this);
    QAction *thisAction = new QAction(tr("This instance"), this);
    newWindowAction = new QAction(tr("New instance"), this);

    openMenu->addAction(thisAction);
    openMenu->addAction(newWindowAction);

    openButton = new QToolButton(this);
    openButton->setText(tr("Open"));
    openButton->setPopupMode(QToolButton::MenuButtonPopup);
    openButton->setMenu(openMenu);

    bottomTools->addWidget(openButton);

    saveAction = new QAction(tr("Save"), this);
    deleteAction = new QAction(tr("Delete"), this);

    bottomTools->addAction(saveAction);
    bottomTools->addAction(deleteAction);

    QObject::connect(previewToggle, &QAction::toggled, this, &LiveCapture::previewToggle_toggled);
    QObject::connect(openButton, &QToolButton::clicked, this, &LiveCapture::openCapture_triggered);
    QObject::connect(thisAction, &QAction::triggered, this, &LiveCapture::openCapture_triggered);
    QObject::connect(newWindowAction, &QAction::triggered, this,
                     &LiveCapture::openNewWindow_triggered);
    QObject::connect(saveAction, &QAction::triggered, this, &LiveCapture::saveCapture_triggered);
    QObject::connect(deleteAction, &QAction::triggered, this, &LiveCapture::deleteCapture_triggered);

    QObject::connect(ui->captures, &RDListWidget::keyPress, this, &LiveCapture::captures_keyPress);

    ui->mainLayout->addWidget(bottomTools);

    bottomTools->setStyleSheet(
        lit("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
            "stop: 0 #E1E1E1, stop: 1.0 #D3D3D3);"));
  }
}

LiveCapture::~LiveCapture()
{
  m_Main->LiveCaptureClosed(this);

  cleanItems();
  killThread();

  delete ui;
}

void LiveCapture::QueueCapture(int frameNumber)
{
  m_CaptureFrameNum = frameNumber;
  m_QueueCapture = true;
}

void LiveCapture::showEvent(QShowEvent *event)
{
  if(!m_ConnectThread)
  {
    m_ConnectThread = new LambdaThread([this]() { this->connectionThreadEntry(); });
    m_ConnectThread->start();
  }

  on_captures_itemSelectionChanged();
}

void LiveCapture::on_captures_mouseClicked(QMouseEvent *e)
{
  if(e->buttons() & Qt::RightButton && !ui->captures->selectedItems().empty())
  {
    QListWidgetItem *item = ui->captures->itemAt(e->pos());

    if(item != NULL)
    {
      ui->captures->clearSelection();
      item->setSelected(true);
    }

    QMenu contextMenu(this);

    QMenu contextOpenMenu(tr("&Open in..."), this);
    QAction thisAction(tr("This instance"), this);
    QAction newAction(tr("New instance"), this);

    contextOpenMenu.addAction(&thisAction);
    contextOpenMenu.addAction(&newAction);

    QAction contextSaveAction(tr("&Save"), this);
    QAction contextDeleteAction(tr("&Delete"), this);

    contextMenu.addAction(contextOpenMenu.menuAction());
    contextMenu.addAction(&contextSaveAction);
    contextMenu.addAction(&contextDeleteAction);

    if(ui->captures->selectedItems().size() == 1)
    {
      newAction.setEnabled(GetLog(ui->captures->selectedItems()[0])->local);
    }
    else
    {
      contextOpenMenu.setEnabled(false);
      contextSaveAction.setEnabled(false);
    }

    QObject::connect(&thisAction, &QAction::triggered, this, &LiveCapture::openCapture_triggered);
    QObject::connect(&newAction, &QAction::triggered, this, &LiveCapture::openNewWindow_triggered);
    QObject::connect(&contextSaveAction, &QAction::triggered, this,
                     &LiveCapture::saveCapture_triggered);
    QObject::connect(&contextDeleteAction, &QAction::triggered, this,
                     &LiveCapture::deleteCapture_triggered);

    m_ContextMenu = &contextMenu;
    RDDialog::show(&contextMenu, QCursor::pos());
    m_ContextMenu = NULL;
  }
}

void LiveCapture::on_captures_itemActivated(QListWidgetItem *item)
{
  openCapture_triggered();
}

void LiveCapture::on_childProcesses_itemActivated(QListWidgetItem *item)
{
  QList<QListWidgetItem *> sel = ui->childProcesses->selectedItems();
  if(sel.count() == 1)
  {
    uint32_t ident = sel[0]->data(IdentRole).toUInt();
    if(ident > 0)
    {
      LiveCapture *live =
          new LiveCapture(m_Ctx, m_Hostname, m_HostFriendlyname, ident, m_Main, m_Main);
      m_Main->ShowLiveCapture(live);
    }
  }
}

void LiveCapture::on_queueCap_clicked()
{
  m_CaptureFrameNum = (int)ui->captureFrame->value();
  m_QueueCapture = true;
}

void LiveCapture::on_triggerCapture_clicked()
{
  m_CaptureNumFrames = (int)ui->numFrames->value();
  if(ui->captureDelay->value() == 0.0)
  {
    m_TriggerCapture = true;
  }
  else
  {
    m_CaptureCounter = (int)ui->captureDelay->value();
    countdownTimer.start();
    ui->triggerCapture->setEnabled(false);
    ui->triggerCapture->setText(tr("Triggering in %1s").arg(m_CaptureCounter));
  }
}

void LiveCapture::openCapture_triggered()
{
  if(ui->captures->selectedItems().size() == 1)
    openCapture(GetLog(ui->captures->selectedItems()[0]));
}

void LiveCapture::openNewWindow_triggered()
{
  if(ui->captures->selectedItems().size() == 1)
  {
    CaptureLog *log = GetLog(ui->captures->selectedItems()[0]);

    QString temppath = m_Ctx.TempLogFilename(lit("newwindow"));

    if(!log->local)
    {
      RDDialog::critical(this, tr("Cannot open new instance"),
                         tr("Can't open log in new instance with remote server in use"));
      return;
    }

    QFile f(log->path);

    if(!f.copy(temppath))
    {
      RDDialog::critical(this, tr("Cannot save temporary log"),
                         tr("Couldn't save log to temporary location\n%1").arg(f.errorString()));
      return;
    }

    QStringList args;
    args << lit("--tempfile") << temppath;
    QProcess::startDetached(qApp->applicationFilePath(), args);
  }
}

void LiveCapture::saveCapture_triggered()
{
  if(ui->captures->selectedItems().size() == 1)
    saveCapture(GetLog(ui->captures->selectedItems()[0]));
}

void LiveCapture::deleteCapture_triggered()
{
  bool allow = checkAllowDelete();

  if(!allow)
    return;

  QList<QListWidgetItem *> sel = ui->captures->selectedItems();

  for(QListWidgetItem *item : sel)
  {
    CaptureLog *log = GetLog(item);

    if(!log->saved)
    {
      if(log->path == m_Ctx.LogFilename())
      {
        m_Main->takeLogOwnership();
        m_Main->CloseLogfile();
      }
      else
      {
        // if connected, prefer using the live connection
        if(m_Connection && m_Connection->Connected() && !log->local)
        {
          QMutexLocker l(&m_DeleteLogsLock);
          m_DeleteLogs.push_back(log->remoteID);
        }
        else
        {
          m_Ctx.Replay().DeleteCapture(log->path, log->local);
        }
      }
    }

    delete log;

    delete ui->captures->takeItem(ui->captures->row(item));
  }
}

void LiveCapture::childUpdate()
{
  // first do a small lock and check if the list is currently empty
  {
    QMutexLocker l(&m_ChildrenLock);

    if(m_Children.empty())
    {
      ui->childProcessLabel->setVisible(false);
      ui->childProcesses->setVisible(false);
    }
  }

  // enumerate processes outside of the lock
  QProcessList processes = QProcessInfo::enumerate();

  // now since we're adding and removing, we lock around the whole rest of the function. It won't be
  // too slow.
  {
    QMutexLocker l(&m_ChildrenLock);

    if(!m_Children.empty())
    {
      // remove any stale processes
      for(int i = 0; i < m_Children.size(); i++)
      {
        bool found = false;

        for(QProcessInfo &p : processes)
        {
          if(p.pid() == m_Children[i].PID)
          {
            found = true;
            break;
          }
        }

        if(!found)
        {
          if(m_Children[i].added)
          {
            for(int c = 0; c < ui->childProcesses->count(); c++)
            {
              QListWidgetItem *item = ui->childProcesses->item(c);
              if(item->data(PIDRole).toUInt() == m_Children[i].PID)
                delete ui->childProcesses->takeItem(c);
            }
          }

          // process expired/doesn't exist anymore
          m_Children.removeAt(i);

          // don't increment i, check the next element at i (if we weren't at the end
          i--;
        }
      }

      for(int i = 0; i < m_Children.size(); i++)
      {
        if(!m_Children[i].added)
        {
          QString name = tr("Unknown Process");

          // find the name
          for(QProcessInfo &p : processes)
          {
            if(p.pid() == m_Children[i].PID)
            {
              name = p.name();
              break;
            }
          }

          QString text = QFormatStr("%1 [PID %2]").arg(name).arg(m_Children[i].PID);

          m_Children[i].added = true;
          QListWidgetItem *item = new QListWidgetItem(text, ui->childProcesses);
          item->setData(PIDRole, QVariant(m_Children[i].PID));
          item->setData(IdentRole, QVariant(m_Children[i].ident));
          ui->childProcesses->addItem(item);
        }
      }
    }

    if(!m_Children.empty())
    {
      ui->childProcessLabel->setVisible(true);
      ui->childProcesses->setVisible(true);
    }
    else
    {
      ui->childProcessLabel->setVisible(false);
      ui->childProcesses->setVisible(false);
    }
  }
}

void LiveCapture::captureCountdownTick()
{
  m_CaptureCounter--;

  if(m_CaptureCounter == 0)
  {
    m_TriggerCapture = true;
    ui->triggerCapture->setEnabled(true);
    ui->triggerCapture->setText(tr("Trigger Capture"));
  }
  else
  {
    countdownTimer.start();
    ui->triggerCapture->setText(tr("Triggering in %1s").arg(m_CaptureCounter));
  }
}

void LiveCapture::killThread()
{
  if(m_ConnectThread)
  {
    m_Disconnect.acquire();
    m_ConnectThread->wait();
    m_ConnectThread->deleteLater();
  }
}

void LiveCapture::setTitle(const QString &title)
{
  setWindowTitle((!m_HostFriendlyname.isEmpty() ? (m_HostFriendlyname + lit(" - ")) : QString()) +
                 title);
}

LiveCapture::CaptureLog *LiveCapture::GetLog(QListWidgetItem *item)
{
  return (CaptureLog *)item->data(LogPtrRole).value<void *>();
}

void LiveCapture::SetLog(QListWidgetItem *item, CaptureLog *log)
{
  item->setData(LogPtrRole, QVariant::fromValue<void *>(log));
}

QImage LiveCapture::MakeThumb(const QImage &screenshot)
{
  const QSizeF thumbSize(ui->captures->iconSize());
  const QSizeF imSize(screenshot.size());

  float x = 0, y = 0;
  float width = 0, height = 0;

  const float srcaspect = imSize.width() / imSize.height();
  const float dstaspect = thumbSize.width() / thumbSize.height();

  if(srcaspect > dstaspect)
  {
    width = thumbSize.width();
    height = width / srcaspect;

    y = (thumbSize.height() - height) / 2;
  }
  else
  {
    height = thumbSize.height();
    width = height * srcaspect;

    x = (thumbSize.width() - width) / 2;
  }

  QImage ret(thumbSize.width(), thumbSize.height(), QImage::Format_RGBA8888);
  ret.fill(Qt::transparent);
  QPainter paint(&ret);
  paint.drawImage(QRectF(x, y, width, height),
                  screenshot.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
  return ret;
}

bool LiveCapture::checkAllowDelete()
{
  bool needcheck = false;

  for(int i = 0; i < ui->captures->count(); i++)
  {
    CaptureLog *log = GetLog(ui->captures->item(i));
    needcheck |= !log->saved;
  }

  if(!needcheck || ui->captures->selectedItems().empty())
    return true;

  ToolWindowManager::raiseToolWindow(this);

  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("Unsaved log(s)", "", ui->captures->selectedItems().size()),
      tr("Are you sure you wish to delete the log(s)?\nAny log currently opened will be closed", "",
         ui->captures->selectedItems().size()),
      RDDialog::YesNoCancel);

  return (res == QMessageBox::Yes);
}

QString LiveCapture::MakeText(CaptureLog *log)
{
  QString text = log->name;
  if(!log->local)
    text += tr(" (Remote)");

  text += lit("\n") + log->api;
  text += lit("\n") + log->timestamp.toString(lit("yyyy-MM-dd HH:mm:ss"));

  return text;
}

bool LiveCapture::checkAllowClose()
{
  m_IgnoreThreadClosed = true;

  bool suppressRemoteWarning = false;

  for(int i = 0; i < ui->captures->count(); i++)
  {
    QListWidgetItem *item = ui->captures->item(i);
    CaptureLog *log = GetLog(ui->captures->item(i));

    if(log->saved)
      continue;

    ui->captures->clearSelection();
    ToolWindowManager::raiseToolWindow(this);
    ui->captures->setFocus();
    item->setSelected(true);

    QMessageBox::StandardButton res = QMessageBox::No;

    if(!suppressRemoteWarning)
    {
      res = RDDialog::question(this, tr("Unsaved log"),
                               tr("Save this logfile '%1' at %2?")
                                   .arg(log->name)
                                   .arg(log->timestamp.toString(lit("HH:mm:ss"))),
                               RDDialog::YesNoCancel);
    }

    if(res == QMessageBox::Cancel)
    {
      m_IgnoreThreadClosed = false;
      return false;
    }

    // we either have to save or delete the log. Make sure that if it's remote that we are able
    // to by having an active connection or replay context on that host.
    if(suppressRemoteWarning == false && (!m_Connection || !m_Connection->Connected()) &&
       !log->local &&
       (!m_Ctx.Replay().CurrentRemote() || m_Ctx.Replay().CurrentRemote()->Hostname != m_Hostname ||
        !m_Ctx.Replay().CurrentRemote()->Connected))
    {
      QMessageBox::StandardButton res2 = RDDialog::question(
          this, tr("No active replay context"),
          tr("This capture is on remote host %1 and there is no active replay context on that "
             "host.\n")
                  .arg(m_HostFriendlyname) +
              tr("Without an active replay context the capture cannot be %1.\n\n")
                  .arg(tr(res == QMessageBox::Yes ? "saved" : "deleted")) +
              tr("Would you like to continue and discard this capture and any others, to be left "
                 "in the temporary folder on the remote machine?"),
          RDDialog::YesNoCancel);

      if(res2 == QMessageBox::Yes)
      {
        suppressRemoteWarning = true;
        res = QMessageBox::No;
      }
      else
      {
        m_IgnoreThreadClosed = false;
        return false;
      }
    }

    if(res == QMessageBox::Yes)
    {
      bool success = saveCapture(log);

      if(!success)
      {
        m_IgnoreThreadClosed = false;
        return false;
      }
    }
  }

  m_IgnoreThreadClosed = false;
  return true;
}

void LiveCapture::openCapture(CaptureLog *log)
{
  log->opened = true;

  if(!log->local &&
     (!m_Ctx.Replay().CurrentRemote() || m_Ctx.Replay().CurrentRemote()->Hostname != m_Hostname ||
      !m_Ctx.Replay().CurrentRemote()->Connected))
  {
    RDDialog::critical(
        this, tr("No active replay context"),
        tr("This capture is on remote host %1 and there is no active replay context on that "
           "host.\n") +
            tr("You can either save the log locally, or switch to a replay context on %1.")
                .arg(m_Hostname));
    return;
  }

  m_Main->LoadLogfile(log->path, !log->saved, log->local);
}

bool LiveCapture::saveCapture(CaptureLog *log)
{
  QString path = m_Main->GetSavePath();

  // we copy the temp log to the desired path, but the log item remains referring to the temp path.
  // This ensures that if the user deletes the saved path we can still open or re-save it.
  if(!path.isEmpty())
  {
    if(log->local)
    {
      QFile src(log->path);
      QFile dst(path);

      // remove any existing file, the user was already prompted to overwrite
      if(dst.exists())
      {
        if(!dst.remove())
        {
          RDDialog::critical(this, tr("Cannot save"),
                             tr("Couldn't remove file at %1\n%2").arg(path).arg(dst.errorString()));
          return false;
        }
      }

      if(!src.copy(path))
      {
        RDDialog::critical(this, tr("Cannot save"),
                           tr("Couldn't copy file to %1\n%2").arg(path).arg(src.errorString()));
        return false;
      }
    }
    else if(m_Connection && m_Connection->Connected())
    {
      // if we have a current live connection, prefer using it
      m_CopyLogLocalPath = path;
      m_CopyLogID = log->remoteID;
    }
    else
    {
      if(!m_Ctx.Replay().CurrentRemote() || m_Ctx.Replay().CurrentRemote()->Hostname != m_Hostname ||
         !m_Ctx.Replay().CurrentRemote()->Connected)
      {
        RDDialog::critical(this, tr("No active replay context"),
                           tr("This capture is on remote host %1 and there is no active replay "
                              "context on that host.\n") +
                               tr("Without an active replay context the capture cannot be saved, "
                                  "try switching to a replay context on %1.")
                                   .arg(m_Hostname));
        return false;
      }

      m_Ctx.Replay().CopyCaptureFromRemote(log->path, path, this);

      if(!QFile::exists(path))
      {
        RDDialog::critical(this, tr("Cannot save"),
                           tr("File couldn't be transferred from remote host"));
        return false;
      }

      m_Ctx.Replay().DeleteCapture(log->path, false);
    }

    log->saved = true;
    log->path = path;
    AddRecentFile(m_Ctx.Config().RecentLogFiles, path, 10);
    m_Main->PopulateRecentFiles();
    return true;
  }

  return false;
}

void LiveCapture::cleanItems()
{
  for(int i = 0; i < ui->captures->count(); i++)
  {
    CaptureLog *log = GetLog(ui->captures->item(i));

    if(!log->saved)
    {
      if(log->path == m_Ctx.LogFilename())
      {
        m_Main->takeLogOwnership();
      }
      else
      {
        // if connected, prefer using the live connection
        if(m_Connection && m_Connection->Connected() && !log->local)
        {
          QMutexLocker l(&m_DeleteLogsLock);
          m_DeleteLogs.push_back(log->remoteID);
        }
        else
        {
          m_Ctx.Replay().DeleteCapture(log->path, log->local);
        }
      }
    }

    delete log;
  }
  ui->captures->clear();
}

void LiveCapture::previewToggle_toggled(bool checked)
{
  if(m_IgnorePreviewToggle)
    return;

  if(checked)
    ui->previewSplit->setSizes({1, 1});
  else
    ui->previewSplit->setSizes({1, 0});
}

void LiveCapture::on_previewSplit_splitterMoved(int pos, int index)
{
  m_IgnorePreviewToggle = true;

  QList<int> sizes = ui->previewSplit->sizes();

  previewToggle->setChecked(sizes[1] != 0);

  m_IgnorePreviewToggle = false;
}

void LiveCapture::captures_keyPress(QKeyEvent *e)
{
  if(e->key() == Qt::Key_Delete)
  {
    deleteCapture_triggered();
  }
}

void LiveCapture::preview_mouseClick(QMouseEvent *e)
{
  QPoint mouse = QCursor::pos();
  if(e->buttons() & Qt::LeftButton)
  {
    previewDragStart = mouse;
    ui->preview->setCursor(QCursor(Qt::SizeAllCursor));
  }
}

void LiveCapture::preview_mouseMove(QMouseEvent *e)
{
  QPoint mouse = QCursor::pos();
  if(e->buttons() & Qt::LeftButton)
  {
    QScrollBar *h = ui->previewScroll->horizontalScrollBar();
    QScrollBar *v = ui->previewScroll->verticalScrollBar();

    h->setValue(h->value() + previewDragStart.x() - mouse.x());
    v->setValue(v->value() + previewDragStart.y() - mouse.y());

    previewDragStart = mouse;
  }
  else
  {
    ui->preview->unsetCursor();
  }
}

void LiveCapture::on_captures_itemSelectionChanged()
{
  int numSelected = ui->captures->selectedItems().size();

  deleteAction->setEnabled(numSelected == 1);
  saveAction->setEnabled(numSelected == 1);
  openButton->setEnabled(numSelected == 1);

  if(ui->captures->selectedItems().size() == 1)
  {
    QListWidgetItem *item = ui->captures->selectedItems()[0];
    CaptureLog *log = GetLog(item);

    newWindowAction->setEnabled(log->local);

    if(log->thumb.width() > 0)
    {
      ui->preview->setPixmap(QPixmap::fromImage(log->thumb));
      ui->preview->setMinimumSize(log->thumb.size());
      ui->preview->setMaximumSize(log->thumb.size());
    }
    else
    {
      ui->preview->setPixmap(QPixmap());
      ui->preview->setMinimumSize(QSize(16, 16));
      ui->preview->setMaximumSize(QSize(16, 16));
    }
  }
}

void LiveCapture::captureCopied(uint32_t ID, const QString &localPath)
{
  for(int i = 0; i < ui->captures->count(); i++)
  {
    QListWidgetItem *item = ui->captures->item(i);
    CaptureLog *log = GetLog(ui->captures->item(i));

    if(log && log->remoteID == ID)
    {
      log->local = true;
      log->path = localPath;
      QFont f = item->font();
      f.setItalic(false);
      item->setFont(f);
      item->setText(MakeText(log));
    }
  }
}

void LiveCapture::captureAdded(uint32_t ID, const QString &executable, const QString &api,
                               const rdctype::array<byte> &thumbnail, int32_t thumbWidth,
                               int32_t thumbHeight, QDateTime timestamp, const QString &path,
                               bool local)
{
  CaptureLog *log = new CaptureLog();
  log->remoteID = ID;
  log->name = executable;
  log->api = api;
  log->timestamp = timestamp;
  log->thumb = QImage(thumbnail.elems, thumbWidth, thumbHeight, QImage::Format_RGB888)
                   .copy(0, 0, thumbWidth, thumbHeight);
  log->saved = false;
  log->path = path;
  log->local = local;

  QListWidgetItem *item = new QListWidgetItem();
  item->setFlags(item->flags() | Qt::ItemIsEditable);
  item->setText(MakeText(log));
  item->setIcon(QIcon(QPixmap::fromImage(MakeThumb(log->thumb))));
  if(!local)
  {
    QFont f = item->font();
    f.setItalic(true);
    item->setFont(f);
  }

  SetLog(item, log);

  ui->captures->addItem(item);
}

void LiveCapture::connectionClosed()
{
  if(m_IgnoreThreadClosed)
    return;

  if(ui->captures->count() <= 1)
  {
    if(ui->captures->count() == 1)
    {
      CaptureLog *log = GetLog(ui->captures->item(0));

      // only auto-open a non-local log if we are successfully connected
      // to this machine as a remote context
      if(!log->local)
      {
        if(!m_Ctx.Replay().CurrentRemote() || m_Ctx.Replay().CurrentRemote()->Hostname != m_Hostname ||
           !m_Ctx.Replay().CurrentRemote()->Connected)
          return;
      }

      if(log->opened)
        return;

      openCapture(log);
      if(!log->saved)
      {
        log->saved = true;
        m_Main->takeLogOwnership();
      }
    }

    // auto-close and load log if we got a capture. If we
    // don't have any captures but DO have child processes,
    // then don't close just yet.
    if(ui->captures->count() == 1 || m_Children.count() == 0)
    {
      selfClose();
      return;
    }

    // if we have no captures and only one child, close and
    // open up a connection to it (similar to behaviour with
    // only one capture
    if(ui->captures->count() == 0 && m_Children.count() == 1)
    {
      LiveCapture *live =
          new LiveCapture(m_Ctx, m_Hostname, m_HostFriendlyname, m_Children[0].ident, m_Main);
      m_Main->ShowLiveCapture(live);
      selfClose();
      return;
    }
  }
}

void LiveCapture::selfClose()
{
  if(m_ContextMenu)
  {
    qInfo() << "preventing race";
    // hide the menu and close our window shortly after
    m_ContextMenu->close();
    QTimer *timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, [this]() { ToolWindowManager::closeToolWindow(this); });
    timer->setSingleShot(true);
    timer->start(250);
  }
  else
  {
    ToolWindowManager::closeToolWindow(this);
  }
}

void LiveCapture::connectionThreadEntry()
{
  m_Connection = RENDERDOC_CreateTargetControl(m_Hostname.toUtf8().data(), m_RemoteIdent,
                                               GetSystemUsername().toUtf8().data(), true);

  if(!m_Connection || !m_Connection->Connected())
  {
    GUIInvoke::call([this]() {
      setTitle(tr("Connection failed"));
      ui->connectionStatus->setText(tr("Connection failed"));
      ui->connectionIcon->setPixmap(Pixmaps::del(ui->connectionIcon));

      connectionClosed();
    });

    return;
  }

  GUIInvoke::call([this]() {
    QString api = QString::fromUtf8(m_Connection->GetAPI());
    if(api.isEmpty())
      api = tr("No API detected");

    QString target = QString::fromUtf8(m_Connection->GetTarget());
    uint32_t pid = m_Connection->GetPID();

    if(pid == 0)
    {
      ui->connectionStatus->setText(tr("Connection established to %1 (%2)").arg(target).arg(api));
      setTitle(target);
    }
    else
    {
      ui->connectionStatus->setText(
          tr("Connection established to %1 [PID %2] (%3)").arg(target).arg(pid).arg(api));
      setTitle(QFormatStr("%1 [PID %2]").arg(target).arg(pid));
    }
    ui->connectionIcon->setPixmap(Pixmaps::connect(ui->connectionIcon));
  });

  while(m_Connection && m_Connection->Connected())
  {
    if(m_TriggerCapture)
    {
      m_Connection->TriggerCapture((uint)m_CaptureNumFrames);
      m_TriggerCapture = false;
    }

    if(m_QueueCapture)
    {
      m_Connection->QueueCapture((uint)m_CaptureFrameNum);
      m_QueueCapture = false;
      m_CaptureFrameNum = 0;
    }

    if(!m_CopyLogLocalPath.isEmpty())
    {
      m_Connection->CopyCapture(m_CopyLogID, m_CopyLogLocalPath.toUtf8().data());
      m_CopyLogLocalPath = QString();
      m_CopyLogID = ~0U;
    }

    QVector<uint32_t> dels;
    {
      QMutexLocker l(&m_DeleteLogsLock);
      dels.swap(m_DeleteLogs);
    }

    for(uint32_t del : dels)
      m_Connection->DeleteCapture(del);

    if(!m_Disconnect.available())
    {
      m_Connection->Shutdown();
      m_Connection = NULL;
      return;
    }

    TargetControlMessage msg = m_Connection->ReceiveMessage();

    if(msg.Type == TargetControlMessageType::RegisterAPI)
    {
      QString api = ToQStr(msg.RegisterAPI.APIName);
      GUIInvoke::call([this, api]() {
        QString target = QString::fromUtf8(m_Connection->GetTarget());
        uint32_t pid = m_Connection->GetPID();

        if(pid == 0)
        {
          ui->connectionStatus->setText(tr("Connection established to %1 (%2)").arg(target).arg(api));
          setTitle(target);
        }
        else
        {
          ui->connectionStatus->setText(
              tr("Connection established to %1 [PID %2] (%3)").arg(target).arg(pid).arg(api));
          setTitle(QFormatStr("%1 [PID %2]").arg(target).arg(pid));
        }
        ui->connectionIcon->setPixmap(Pixmaps::connect(ui->connectionIcon));
      });
    }

    if(msg.Type == TargetControlMessageType::NewCapture)
    {
      uint32_t capID = msg.NewCapture.ID;
      QDateTime timestamp = QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0));
      timestamp = timestamp.addSecs(msg.NewCapture.timestamp).toLocalTime();
      rdctype::array<byte> thumb = msg.NewCapture.thumbnail;
      int32_t thumbWidth = msg.NewCapture.thumbWidth;
      int32_t thumbHeight = msg.NewCapture.thumbHeight;
      QString path = ToQStr(msg.NewCapture.path);
      bool local = msg.NewCapture.local;

      GUIInvoke::call([this, capID, timestamp, thumb, thumbWidth, thumbHeight, path, local]() {
        QString target = QString::fromUtf8(m_Connection->GetTarget());
        QString api = QString::fromUtf8(m_Connection->GetAPI());

        captureAdded(capID, target, api, thumb, thumbWidth, thumbHeight, timestamp, path, local);
      });
    }

    if(msg.Type == TargetControlMessageType::CaptureCopied)
    {
      uint32_t capID = msg.NewCapture.ID;
      QString path = ToQStr(msg.NewCapture.path);

      GUIInvoke::call([=]() { captureCopied(capID, path); });
    }

    if(msg.Type == TargetControlMessageType::NewChild)
    {
      if(msg.NewChild.PID != 0)
      {
        ChildProcess c;
        c.PID = (int)msg.NewChild.PID;
        c.ident = msg.NewChild.ident;

        {
          QMutexLocker l(&m_ChildrenLock);
          m_Children.push_back(c);
        }
      }
    }
  }

  GUIInvoke::call([this]() {
    ui->connectionStatus->setText(tr("Connection closed"));
    ui->connectionIcon->setPixmap(Pixmaps::disconnect(ui->connectionIcon));

    ui->numFrames->setEnabled(false);
    ui->captureDelay->setEnabled(false);
    ui->captureFrame->setEnabled(false);
    ui->triggerCapture->setEnabled(false);
    ui->queueCap->setEnabled(false);

    connectionClosed();
  });
}
