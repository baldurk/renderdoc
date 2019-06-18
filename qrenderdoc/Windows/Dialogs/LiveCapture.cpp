/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include <QDesktopServices>
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
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Code/qprocessinfo.h"
#include "Widgets/Extended/RDLabel.h"
#include "Windows/MainWindow.h"
#include "ui_LiveCapture.h"

static const int PIDRole = Qt::UserRole + 1;
static const int IdentRole = Qt::UserRole + 2;
static const int CapPtrRole = Qt::UserRole + 3;

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
      LiveCapture::Capture *cap = live->GetCapture(item);
      if(cap)
        editor->setProperty(n, cap->name);
    }
  }

  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
  {
    QByteArray n = editor->metaObject()->userProperty().name();
    QListWidgetItem *item = live->ui->captures->item(index.row());

    if(!n.isEmpty() && item)
    {
      LiveCapture::Capture *cap = live->GetCapture(item);
      if(cap)
      {
        cap->name = editor->property(n).toString();
        item->setText(live->MakeText(cap));
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

  setTitle(tr("Connecting"));
  ui->connectionStatus->setText(tr("Connecting"));
  ui->connectionIcon->setPixmap(Pixmaps::hourglass(ui->connectionIcon));

  ui->apiIcon->setVisible(false);

  ui->triggerDelayedCapture->setEnabled(false);
  ui->triggerImmediateCapture->setEnabled(false);
  ui->queueCap->setEnabled(false);
  ui->cycleActiveWindow->setEnabled(false);

  ui->target->setText(QString());

  ui->progressLabel->setVisible(false);
  ui->progressBar->setVisible(false);

  ui->captures->setItemDelegate(new NameEditOnlyDelegate(this));

  ui->captures->verticalScrollBar()->setSingleStep(20);

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
  }
}

LiveCapture::~LiveCapture()
{
  m_Main->LiveCaptureClosed(this);

  cleanItems();
  killThread();

  delete ui;
}

void LiveCapture::QueueCapture(int frameNumber, int numFrames)
{
  m_QueueCaptureFrameNum = frameNumber;
  m_CaptureNumFrames = numFrames;
  m_QueueCapture.release();
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
      newAction.setEnabled(GetCapture(ui->captures->selectedItems()[0])->local);
    }
    else
    {
      contextOpenMenu.setEnabled(false);
      contextSaveAction.setText(tr("&Save All"));
      contextDeleteAction.setText(tr("&Delete All"));
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
  m_CaptureNumFrames = (int)ui->numFrames->value();
  m_QueueCaptureFrameNum = (int)ui->captureFrame->value();
  m_QueueCapture.release();
}

void LiveCapture::on_triggerImmediateCapture_clicked()
{
  m_CaptureNumFrames = (int)ui->numFrames->value();
  m_TriggerCapture.release();
}

void LiveCapture::on_cycleActiveWindow_clicked()
{
  m_Connection->CycleActiveWindow();
}

void LiveCapture::on_triggerDelayedCapture_clicked()
{
  if(ui->captureDelay->value() == 0.0)
  {
    on_triggerImmediateCapture_clicked();
  }
  else
  {
    m_CaptureCounter = (int)ui->captureDelay->value();
    countdownTimer.start();
    ui->triggerDelayedCapture->setEnabled(false);
    ui->triggerDelayedCapture->setText(tr("Triggering in %1s").arg(m_CaptureCounter));
  }
}

void LiveCapture::openCapture_triggered()
{
  if(ui->captures->selectedItems().size() == 1)
    openCapture(GetCapture(ui->captures->selectedItems()[0]));
}

void LiveCapture::openNewWindow_triggered()
{
  if(ui->captures->selectedItems().size() == 1)
  {
    Capture *cap = GetCapture(ui->captures->selectedItems()[0]);

    QString temppath = m_Ctx.TempCaptureFilename(lit("newwindow"));

    if(!cap->local)
    {
      RDDialog::critical(this, tr("Cannot open new instance"),
                         tr("Can't open capture in new instance with remote server in use"));
      return;
    }

    QFile f(cap->path);

    if(!f.copy(temppath))
    {
      RDDialog::critical(this, tr("Cannot save temporary capture"),
                         tr("Couldn't save capture to temporary location\n%1").arg(f.errorString()));
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
  {
    saveCapture(GetCapture(ui->captures->selectedItems()[0]), QString());
  }
  else
  {
    QString path = m_Main->GetSavePath(tr("Save All Captures As"));

    if(!path.isEmpty())
    {
      if(path.endsWith(lit(".rdc")))
        path.chop(4);

      for(QListWidgetItem *item : ui->captures->selectedItems())
      {
        Capture *cap = GetCapture(item);
        saveCapture(cap, QFormatStr("%1-frame%2.rdc").arg(path).arg(cap->frameNumber));
      }
    }
  }
}

void LiveCapture::deleteCapture_triggered()
{
  bool allow = checkAllowDelete();

  if(!allow)
    return;

  QList<QListWidgetItem *> sel = ui->captures->selectedItems();

  for(QListWidgetItem *item : sel)
  {
    Capture *cap = GetCapture(item);

    if(!cap->saved)
    {
      if(cap->path == m_Ctx.GetCaptureFilename())
      {
        m_Main->takeCaptureOwnership();
        m_Main->CloseCapture();
      }
      else
      {
        // if connected, prefer using the live connection
        if(m_Connection && m_Connection->Connected() && !cap->local)
        {
          QMutexLocker l(&m_DeleteCapturesLock);
          m_DeleteCaptures.push_back(cap->remoteID);
        }
        else
        {
          m_Ctx.Replay().DeleteCapture(cap->path, cap->local);
        }
      }
    }

    delete cap;

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
  QProcessList processes = QProcessInfo::enumerate(false);

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
    m_CaptureNumFrames = (int)ui->numFrames->value();
    ui->triggerDelayedCapture->setEnabled(true);
    ui->triggerDelayedCapture->setText(tr("Trigger After Delay"));
    m_TriggerCapture.release();
  }
  else
  {
    countdownTimer.start();
    ui->triggerDelayedCapture->setText(tr("Triggering in %1s").arg(m_CaptureCounter));
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

LiveCapture::Capture *LiveCapture::GetCapture(QListWidgetItem *item)
{
  return (Capture *)item->data(CapPtrRole).value<void *>();
}

void LiveCapture::AddCapture(QListWidgetItem *item, Capture *cap)
{
  item->setData(CapPtrRole, QVariant::fromValue<void *>(cap));
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
    Capture *cap = GetCapture(ui->captures->item(i));
    needcheck |= !cap->saved;
  }

  if(!needcheck || ui->captures->selectedItems().empty())
    return true;

  ToolWindowManager::raiseToolWindow(this);

  QMessageBox::StandardButton res =
      RDDialog::question(this, tr("Unsaved capture(s)", "", ui->captures->selectedItems().size()),
                         tr("Are you sure you wish to delete the capture(s)?\nAny capture "
                            "currently opened will be closed",
                            "", ui->captures->selectedItems().size()),
                         RDDialog::YesNoCancel);

  return (res == QMessageBox::Yes);
}

void LiveCapture::updateAPIStatus()
{
  QString apiStatus;

  bool nonpresenting = false;

  // add any fully working APIs first in the list.
  for(QString api : m_APIs.keys())
  {
    if(m_APIs[api].supported && m_APIs[api].presenting)
      apiStatus += lit(", <b>%1 (Active)</b>").arg(api);
  }

  // then add any problem APIs
  for(QString api : m_APIs.keys())
  {
    if(!m_APIs[api].supported)
    {
      apiStatus += tr(", %1 (Unsupported)").arg(api);
    }
    else if(!m_APIs[api].presenting)
    {
      apiStatus += tr(", %1 (Not Presenting)").arg(api);
      nonpresenting = true;
    }
  }

  // remove the redundant starting ", "
  apiStatus.remove(0, 2);

  ui->apiStatus->setText(apiStatus);

  ui->apiIcon->setVisible(nonpresenting);
}

QString LiveCapture::MakeText(Capture *cap)
{
  QString text = cap->name;
  if(!cap->local)
    text += tr(" (Remote)");

  text += lit("\n") + cap->api;
  text += tr("\nFrame #%1 ").arg(cap->frameNumber) +
          cap->timestamp.toString(lit("yyyy-MM-dd HH:mm:ss"));

  return text;
}

bool LiveCapture::checkAllowClose()
{
  m_IgnoreThreadClosed = true;

  bool suppressRemoteWarning = false;
  bool notoall = false;

  QMessageBox::StandardButtons msgFlags = RDDialog::YesNoCancel;

  if(ui->captures->count() > 1)
    msgFlags |= QMessageBox::NoToAll;

  for(int i = 0; i < ui->captures->count(); i++)
  {
    QListWidgetItem *item = ui->captures->item(i);
    Capture *cap = GetCapture(ui->captures->item(i));

    if(cap->saved)
      continue;

    ui->captures->clearSelection();
    ToolWindowManager::raiseToolWindow(this);
    ui->captures->setFocus();
    item->setSelected(true);

    QMessageBox::StandardButton res = QMessageBox::No;

    if(!suppressRemoteWarning && !notoall)
    {
      res = RDDialog::question(this, tr("Unsaved capture"),
                               tr("Save this capture '%1 Frame #%2' at %3?")
                                   .arg(cap->name)
                                   .arg(cap->frameNumber)
                                   .arg(cap->timestamp.toString(lit("HH:mm:ss"))),
                               msgFlags);

      if(res == QMessageBox::NoToAll)
      {
        notoall = true;
        res = QMessageBox::No;
      }
    }

    if(res == QMessageBox::Cancel)
    {
      m_IgnoreThreadClosed = false;
      return false;
    }

    // we either have to save or delete the capture. Make sure that if it's remote that we are able
    // to by having an active connection or replay context on that host.
    if(suppressRemoteWarning == false && (!m_Connection || !m_Connection->Connected()) &&
       !cap->local && (!m_Ctx.Replay().CurrentRemote() ||
                       QString(m_Ctx.Replay().CurrentRemote()->hostname) != m_Hostname ||
                       !m_Ctx.Replay().CurrentRemote()->connected))
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
      bool success = saveCapture(cap, QString());

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

void LiveCapture::openCapture(Capture *cap)
{
  cap->opened = true;

  if(!cap->local && (!m_Ctx.Replay().CurrentRemote() ||
                     QString(m_Ctx.Replay().CurrentRemote()->hostname) != m_Hostname ||
                     !m_Ctx.Replay().CurrentRemote()->connected))
  {
    RDDialog::critical(
        this, tr("No active replay context"),
        tr("This capture is on remote host %1 and there is no active replay context on that "
           "host.\nYou can either save the capture locally, or switch to a replay context on %1.")
            .arg(m_HostFriendlyname));
    return;
  }

  m_Main->LoadCapture(cap->path, !cap->saved, cap->local);
}

bool LiveCapture::saveCapture(Capture *cap, QString path)
{
  if(path.isEmpty())
  {
    path = m_Main->GetSavePath();

    if(path.isEmpty())
      return false;
  }

  if(QString(m_Ctx.GetCaptureFilename()) == path)
  {
    RDDialog::critical(this, tr("Cannot save"), tr("Can't overwrite currently open capture at %1\n"
                                                   "Close the capture or save to another location.")
                                                    .arg(path));
    return false;
  }

  // we copy the temp capture to the desired path, but the capture item remains referring to the
  // temp path.
  // This ensures that if the user deletes the saved path we can still open or re-save it.
  if(cap->local)
  {
    QFile src(cap->path);
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
    m_CopyCaptureLocalPath = path;
    m_CopyCaptureID = cap->remoteID;

    m_CopyCapture.release();
  }
  else
  {
    if(!m_Ctx.Replay().CurrentRemote() ||
       QString(m_Ctx.Replay().CurrentRemote()->hostname) != m_Hostname ||
       !m_Ctx.Replay().CurrentRemote()->connected)
    {
      RDDialog::critical(this, tr("No active replay context"),
                         tr("This capture is on remote host %1 and there is no active replay "
                            "context on that host.\n") +
                             tr("Without an active replay context the capture cannot be saved, "
                                "try switching to a replay context on %1.")
                                 .arg(m_Hostname));
      return false;
    }

    m_Ctx.Replay().CopyCaptureFromRemote(cap->path, path, this);

    if(!QFile::exists(path))
    {
      RDDialog::critical(this, tr("Cannot save"),
                         tr("File couldn't be transferred from remote host"));
      return false;
    }

    m_Ctx.Replay().DeleteCapture(cap->path, false);
  }

  // delete the temporary copy
  if(!cap->saved)
    m_Ctx.Replay().DeleteCapture(cap->path, cap->local);

  cap->saved = true;
  cap->path = path;
  AddRecentFile(m_Ctx.Config().RecentCaptureFiles, path, 10);
  m_Main->PopulateRecentCaptureFiles();
  return true;
}

void LiveCapture::cleanItems()
{
  for(int i = 0; i < ui->captures->count(); i++)
  {
    Capture *cap = GetCapture(ui->captures->item(i));

    if(!cap->saved)
    {
      if(cap->path == m_Ctx.GetCaptureFilename())
      {
        m_Main->takeCaptureOwnership();
      }
      else
      {
        // if connected, prefer using the live connection
        if(m_Connection && m_Connection->Connected() && !cap->local)
        {
          QMutexLocker l(&m_DeleteCapturesLock);
          m_DeleteCaptures.push_back(cap->remoteID);
        }
        else
        {
          m_Ctx.Replay().DeleteCapture(cap->path, cap->local);
        }
      }
    }

    delete cap;
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

void LiveCapture::on_apiIcon_clicked(QMouseEvent *event)
{
  QDesktopServices::openUrl(QUrl(lit("https://renderdoc.org/docs/in_application_api.html")));
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

  openButton->setEnabled(numSelected == 1);

  if(ui->captures->selectedItems().size() == 1)
  {
    QListWidgetItem *item = ui->captures->selectedItems()[0];
    Capture *cap = GetCapture(item);

    newWindowAction->setEnabled(cap->local);

    if(cap->thumb.width() > 0)
    {
      ui->preview->setPixmap(QPixmap::fromImage(cap->thumb));
      ui->preview->setMinimumSize(cap->thumb.size());
      ui->preview->setMaximumSize(cap->thumb.size());
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
    Capture *cap = GetCapture(ui->captures->item(i));

    if(cap && cap->remoteID == ID)
    {
      cap->local = true;
      cap->path = localPath;
      QFont f = item->font();
      f.setItalic(false);
      item->setFont(f);
      item->setText(MakeText(cap));
    }
  }
}

void LiveCapture::captureAdded(const NewCaptureData &newCapture)
{
  Capture *cap = new Capture();

  cap->name = QString::fromUtf8(m_Connection->GetTarget());

  cap->api = newCapture.api;
  if(cap->api.isEmpty())
    cap->api = QString::fromUtf8(m_Connection->GetAPI());

  cap->timestamp =
      QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), Qt::UTC).addSecs(newCapture.timestamp).toLocalTime();

  cap->thumb = QImage(newCapture.thumbnail.data(), newCapture.thumbWidth, newCapture.thumbHeight,
                      newCapture.thumbWidth * 3, QImage::Format_RGB888)
                   .copy(0, 0, newCapture.thumbWidth, newCapture.thumbHeight);

  cap->remoteID = newCapture.captureId;
  cap->saved = false;
  cap->path = newCapture.path;
  cap->local = newCapture.local;
  cap->frameNumber = newCapture.frameNumber;

  QListWidgetItem *item = new QListWidgetItem();
  item->setFlags(item->flags() | Qt::ItemIsEditable);
  item->setText(MakeText(cap));
  item->setIcon(QIcon(QPixmap::fromImage(MakeThumb(cap->thumb))));
  if(!newCapture.local)
  {
    QFont f = item->font();
    f.setItalic(true);
    item->setFont(f);
  }

  AddCapture(item, cap);

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
      Capture *cap = GetCapture(ui->captures->item(0));

      // only auto-open a non-local capture if we are successfully connected
      // to this machine as a remote context
      if(!cap->local)
      {
        if(!m_Ctx.Replay().CurrentRemote() ||
           QString(m_Ctx.Replay().CurrentRemote()->hostname) != m_Hostname ||
           !m_Ctx.Replay().CurrentRemote()->connected)
          return;
      }

      // don't close if a dialog is open
      if(QApplication::activeModalWidget() || QApplication::activePopupWidget())
        return;

      if(cap->opened)
        return;

      openCapture(cap);
      if(!cap->saved)
      {
        cap->saved = true;
        m_Main->takeCaptureOwnership();
      }
    }

    // auto-close and load capture if we got a capture. If we
    // don't have any captures but DO have child processes,
    // then don't close just yet.
    if(ui->captures->count() == 1 || m_Children.count() == 0)
    {
      // raise the texture viewer if it exists, instead of falling back to most likely the capture
      // executable dialog which is not useful.
      if(ui->captures->count() == 1 && m_Ctx.HasTextureViewer())
        m_Ctx.ShowTextureViewer();
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
    GUIInvoke::call(this, [this]() {
      setTitle(tr("Connection failed"));
      ui->connectionStatus->setText(tr("Failed"));
      ui->connectionIcon->setPixmap(Pixmaps::del(ui->connectionIcon));

      connectionClosed();
    });

    return;
  }

  GUIInvoke::call(this, [this]() {
    uint32_t pid = m_Connection->GetPID();
    QString target = QString::fromUtf8(m_Connection->GetTarget());
    if(pid)
      setTitle(QFormatStr("%1 [PID %2]").arg(target).arg(pid));
    else
      setTitle(target);

    ui->target->setText(windowTitle());
    ui->connectionIcon->setPixmap(Pixmaps::connect(ui->connectionIcon));
    ui->connectionStatus->setText(tr("Established"));
  });

  while(m_Connection && m_Connection->Connected())
  {
    if(m_TriggerCapture.tryAcquire())
    {
      m_Connection->TriggerCapture((uint)m_CaptureNumFrames);
      m_CaptureNumFrames = 1;
    }

    if(m_QueueCapture.tryAcquire())
    {
      m_Connection->QueueCapture((uint32_t)m_QueueCaptureFrameNum, (uint32_t)m_CaptureNumFrames);
      m_QueueCaptureFrameNum = 0;
      m_CaptureNumFrames = 1;
    }

    if(m_CopyCapture.tryAcquire())
    {
      m_Connection->CopyCapture(m_CopyCaptureID, m_CopyCaptureLocalPath.toUtf8().data());
      m_CopyCaptureLocalPath = QString();
      m_CopyCaptureID = ~0U;
    }

    QVector<uint32_t> dels;
    {
      QMutexLocker l(&m_DeleteCapturesLock);
      dels.swap(m_DeleteCaptures);
    }

    for(uint32_t del : dels)
      m_Connection->DeleteCapture(del);

    if(!m_Disconnect.available())
    {
      m_Connection->Shutdown();
      m_Connection = NULL;
      return;
    }

    TargetControlMessage msg = m_Connection->ReceiveMessage([this](float progress) {
      GUIInvoke::call(this, [this, progress]() {
        if(progress >= 0.0f && progress < 1.0f)
        {
          ui->progressLabel->setText(tr("Copy in Progress:"));
          ui->progressLabel->setVisible(true);
          ui->progressBar->setVisible(true);
          ui->progressBar->setMaximum(1000);
          ui->progressBar->setValue(1000 * progress);
        }
        else
        {
          ui->progressLabel->setVisible(false);
          ui->progressBar->setVisible(false);
        }
      });
    });

    if(msg.type == TargetControlMessageType::RegisterAPI)
    {
      QString api = msg.apiUse.name;
      bool presenting = msg.apiUse.presenting;
      bool supported = msg.apiUse.supported;
      GUIInvoke::call(this, [this, api, presenting, supported]() {
        m_APIs[api] = APIStatus(presenting, supported);

        if(presenting && supported)
        {
          ui->triggerImmediateCapture->setEnabled(true);
          ui->triggerDelayedCapture->setEnabled(true);
          ui->queueCap->setEnabled(true);
        }

        updateAPIStatus();
      });
    }

    if(msg.type == TargetControlMessageType::CaptureProgress)
    {
      float progress = msg.capProgress;
      GUIInvoke::call(this, [this, progress]() {

        if(progress >= 0.0f && progress < 1.0f)
        {
          ui->progressLabel->setText(tr("Capture in Progress:"));
          ui->progressLabel->setVisible(true);
          ui->progressBar->setVisible(true);
          ui->progressBar->setMaximum(1000);
          ui->progressBar->setValue(1000 * progress);
        }
        else
        {
          ui->progressLabel->setVisible(false);
          ui->progressBar->setVisible(false);
        }

      });
    }

    if(msg.type == TargetControlMessageType::NewCapture)
    {
      NewCaptureData cap = msg.newCapture;
      GUIInvoke::call(this, [this, cap]() { captureAdded(cap); });
    }

    if(msg.type == TargetControlMessageType::CaptureCopied)
    {
      uint32_t capID = msg.newCapture.captureId;
      QString path = msg.newCapture.path;

      GUIInvoke::call(this, [this, capID, path]() { captureCopied(capID, path); });
    }

    if(msg.type == TargetControlMessageType::NewChild)
    {
      if(msg.newChild.processId != 0)
      {
        ChildProcess c;
        c.PID = (int)msg.newChild.processId;
        c.ident = msg.newChild.ident;

        {
          QMutexLocker l(&m_ChildrenLock);
          m_Children.push_back(c);
        }
      }
    }

    if(msg.type == TargetControlMessageType::CapturableWindowCount)
    {
      uint32_t windows = msg.capturableWindowCount;
      GUIInvoke::call(this, [this, windows]() { ui->cycleActiveWindow->setEnabled(windows > 1); });
    }
  }

  GUIInvoke::call(this, [this]() {
    ui->connectionStatus->setText(tr("Closed"));
    ui->connectionIcon->setPixmap(Pixmaps::disconnect(ui->connectionIcon));

    ui->numFrames->setEnabled(false);
    ui->captureDelay->setEnabled(false);
    ui->captureFrame->setEnabled(false);
    ui->triggerDelayedCapture->setEnabled(false);
    ui->triggerImmediateCapture->setEnabled(false);
    ui->queueCap->setEnabled(false);
    ui->cycleActiveWindow->setEnabled(false);

    ui->apiStatus->setText(tr("None"));
    ui->apiIcon->setVisible(false);

    connectionClosed();
  });
}
