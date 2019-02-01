/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "RemoteManager.h"
#include <QKeyEvent>
#include "3rdparty/flowlayout/FlowLayout.h"
#include "Code/Interface/QRDInterface.h"
#include "Code/Resources.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/MainWindow.h"
#include "ui_RemoteManager.h"

struct RemoteConnect
{
  RemoteConnect() {}
  RemoteConnect(const QString &h, const QString &f, uint32_t i) : host(h), friendly(f), ident(i) {}
  QString host;
  QString friendly;
  uint32_t ident = 0;
};

Q_DECLARE_METATYPE(RemoteConnect);

static void setRemoteConnect(RDTreeWidgetItem *item, const RemoteConnect &connect)
{
  if(!item)
    return;

  item->setTag(QVariant::fromValue(connect));
}

static RemoteConnect getRemoteConnect(RDTreeWidgetItem *item)
{
  if(!item)
    return RemoteConnect();

  return item->tag().value<RemoteConnect>();
}

static void setRemoteHost(RDTreeWidgetItem *item, RemoteHost *host)
{
  if(!item)
    return;

  item->setTag(QVariant::fromValue((uintptr_t)host));
}

static RemoteHost *getRemoteHost(RDTreeWidgetItem *item)
{
  if(!item)
    return NULL;

  return (RemoteHost *)item->tag().value<uintptr_t>();
}

void deleteItemAndHost(RDTreeWidgetItem *item)
{
  delete getRemoteHost(item);
  delete item;
}

RemoteManager::RemoteManager(ICaptureContext &ctx, MainWindow *main)
    : QDialog(NULL), ui(new Ui::RemoteManager), m_Ctx(ctx), m_Main(main)
{
  ui->setupUi(this);

  m_ExternalRef.release(1);

  ui->hosts->setFont(Formatter::PreferredFont());
  ui->hostname->setFont(Formatter::PreferredFont());
  ui->runCommand->setFont(Formatter::PreferredFont());

  ui->hosts->setColumns({tr("Hostname"), tr("Running")});

  ui->hosts->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui->hosts->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  lookupsProgressFlow = new QWidget(this);

  FlowLayout *flow = new FlowLayout(lookupsProgressFlow, 0, 3, 3);

  lookupsProgressFlow->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  flow->addWidget(ui->progressIcon);
  flow->addWidget(ui->progressText);
  flow->addWidget(ui->progressCount);

  QVBoxLayout *vertical = new QVBoxLayout(this);

  vertical->addWidget(ui->hosts);
  vertical->addWidget(lookupsProgressFlow);
  vertical->addWidget(ui->bottomLayout->parentWidget());

  m_Ctx.Config().AddAndroidHosts();

  for(RemoteHost *h : m_Ctx.Config().RemoteHosts)
    addHost(h);

  on_hosts_itemSelectionChanged();
}

RemoteManager::~RemoteManager()
{
  for(RDTreeWidgetItem *item : m_QueuedDeletes)
    deleteItemAndHost(item);
  delete ui;
}

void RemoteManager::closeWhenFinished()
{
  m_ExternalRef.acquire(1);
  updateStatus();
}

void RemoteManager::setRemoteServerLive(RDTreeWidgetItem *node, bool live, bool busy)
{
  RemoteHost *host = getRemoteHost(node);

  if(!host)
    return;

  host->serverRunning = live;
  host->busy = busy;

  if(host->IsLocalhost())
  {
    node->setIcon(0, QIcon());
    node->setText(1, QString());
  }
  else
  {
    QString text = live ? tr("Remote server running") : tr("No remote server");

    if(host->connected)
      text += tr(" (Active Context)");
    else if(host->versionMismatch)
      text += tr(" (Version Mismatch)");
    else if(host->busy)
      text += tr(" (Busy)");

    node->setText(1, text);

    node->setIcon(0, live ? Icons::connect() : Icons::disconnect());
  }
}

bool RemoteManager::isRemoteServerLive(RDTreeWidgetItem *node)
{
  RemoteHost *host = getRemoteHost(node);
  return host && host->serverRunning;
}

void RemoteManager::addHost(RemoteHost *host)
{
  RDTreeWidgetItem *node = new RDTreeWidgetItem({host->Name(), lit("...")});

  node->setItalic(true);
  node->setIcon(0, Icons::hourglass());
  setRemoteHost(node, host);

  ui->hosts->addTopLevelItem(node);
  ui->hosts->setSelectedItem(node);

  ui->refreshOne->setEnabled(false);
  ui->refreshAll->setEnabled(false);

  refreshHost(node);

  updateLookupsStatus();
}

void RemoteManager::updateLookupsStatus()
{
  lookupsProgressFlow->setVisible(!ui->refreshAll->isEnabled());
  ui->progressCount->setText(tr("%1 lookups remaining").arg(m_Lookups.available()));
}

void RemoteManager::runRemoteServer(RDTreeWidgetItem *node)
{
  RemoteHost *host = getRemoteHost(node);

  if(!host)
  {
    m_Lookups.acquire();
    return;
  }

  host->Launch();

  // now refresh this host
  refreshHost(node);

  m_Lookups.acquire();
}

void RemoteManager::refreshHost(RDTreeWidgetItem *node)
{
  RemoteHost *host = getRemoteHost(node);

  if(!host)
    return;

  m_Lookups.release();

  // this function looks up the remote connections and for each one open
  // queries it for the API, target (usually executable name) and if any user is already connected
  LambdaThread *th = new LambdaThread([this, node, host]() {
    QByteArray username = GetSystemUsername().toUtf8();

    host->CheckStatus();

    GUIInvoke::call(
        this, [this, node, host]() { setRemoteServerLive(node, host->serverRunning, host->busy); });

    uint32_t nextIdent = 0;

    for(;;)
    {
      // just a sanity check to make sure we don't hit some unexpected case and infinite loop
      uint32_t prevIdent = nextIdent;

      nextIdent = RENDERDOC_EnumerateRemoteTargets(host->hostname.c_str(), nextIdent);

      if(nextIdent == 0 || prevIdent >= nextIdent)
        break;

      ITargetControl *conn =
          RENDERDOC_CreateTargetControl(host->hostname.c_str(), nextIdent, username.data(), false);

      if(conn)
      {
        QString target = QString::fromUtf8(conn->GetTarget());
        QString api = QString::fromUtf8(conn->GetAPI());
        QString busy = QString::fromUtf8(conn->GetBusyClient());

        QString running;

        if(!busy.isEmpty())
          running = tr("Running %1, %2 is connected").arg(api).arg(busy);
        else
          running = tr("Running %1").arg(api);

        RemoteConnect tag(host->hostname, host->Name(), nextIdent);

        GUIInvoke::call(this, [this, node, target, running, tag]() {
          RDTreeWidgetItem *child = new RDTreeWidgetItem({target, running});
          setRemoteConnect(child, tag);
          node->addChild(child);
          ui->hosts->expandItem(node);
        });

        conn->Shutdown();
      }
    }

    GUIInvoke::call(this, [node]() { node->setItalic(false); });

    GUIInvoke::call(this, [this]() {
      m_Lookups.acquire();
      updateStatus();
    });
  });
  th->selfDelete(true);
  th->start();
}

// don't allow the user to refresh until all pending connections have been checked
// (to stop flooding)
void RemoteManager::updateStatus()
{
  if(m_Lookups.available() == 0)
  {
    ui->refreshOne->setEnabled(true);
    ui->refreshAll->setEnabled(true);

    for(RDTreeWidgetItem *item : m_QueuedDeletes)
      deleteItemAndHost(item);
    m_QueuedDeletes.clear();

    // if the external ref is gone now, we can delete ourselves
    if(m_ExternalRef.available() == 0)
    {
      deleteLater();
      return;
    }
  }

  updateConnectButton();
  updateLookupsStatus();
}

void RemoteManager::connectToApp(RDTreeWidgetItem *node)
{
  if(node)
  {
    RemoteConnect connect = getRemoteConnect(node);

    if(connect.ident > 0)
    {
      LiveCapture *live =
          new LiveCapture(m_Ctx, connect.host, connect.friendly, connect.ident, m_Main, m_Main);
      m_Main->ShowLiveCapture(live);
      accept();
    }
  }
}

void RemoteManager::updateConnectButton()
{
  RDTreeWidgetItem *item = ui->hosts->selectedItem();

  if(item)
  {
    ui->connect->setEnabled(true);
    ui->connect->setText(tr("Connect to App"));

    RemoteHost *host = getRemoteHost(item);

    if(host)
    {
      if(host->IsLocalhost())
      {
        ui->connect->setText(tr("Run Server"));
        ui->connect->setEnabled(false);
      }
      else if(host->serverRunning)
      {
        ui->connect->setText(tr("Shutdown"));

        if(host->busy && !host->connected)
          ui->connect->setEnabled(false);
      }
      else
      {
        ui->connect->setText(tr("Run Server"));

        if(host->runCommand.isEmpty())
          ui->connect->setEnabled(false);
      }
    }
  }
  else
  {
    ui->connect->setEnabled(false);
  }
}

void RemoteManager::addNewHost()
{
  QString host = ui->hostname->text().trimmed();
  if(!host.isEmpty())
  {
    bool found = false;

    for(int i = 0; i < m_Ctx.Config().RemoteHosts.count(); i++)
    {
      QString hostname = m_Ctx.Config().RemoteHosts[i]->hostname;
      if(hostname.compare(host, Qt::CaseInsensitive) == 0)
      {
        found = true;
        break;
      }
    }

    if(!found)
    {
      RemoteHost *h = new RemoteHost();
      h->hostname = host;
      h->runCommand = ui->runCommand->text().trimmed();

      m_Ctx.Config().RemoteHosts.push_back(h);
      m_Ctx.Config().Save();

      addHost(h);
    }
  }
  ui->hostname->setText(host);
  on_hostname_textEdited(host);
}

void RemoteManager::setRunCommand()
{
  RDTreeWidgetItem *item = ui->hosts->selectedItem();

  if(!item)
    return;

  RemoteHost *h = getRemoteHost(item);

  if(h)
  {
    h->runCommand = ui->runCommand->text().trimmed();
    m_Ctx.Config().Save();
  }
}

void RemoteManager::queueDelete(RDTreeWidgetItem *item)
{
  // if there are refreshes pending, queue it for deletion when they complete.
  if(m_Lookups.available() > 0)
  {
    m_QueuedDeletes.push_back(item);
  }
  else
  {
    deleteItemAndHost(item);
  }
}

void RemoteManager::on_hosts_itemActivated(RDTreeWidgetItem *item, int column)
{
  RemoteConnect connect = getRemoteConnect(item);
  if(connect.ident > 0)
    connectToApp(item);
}

void RemoteManager::on_hosts_itemSelectionChanged()
{
  ui->addUpdateHost->setText(tr("Add"));
  ui->addUpdateHost->setEnabled(true);
  ui->deleteHost->setEnabled(false);
  ui->refreshOne->setEnabled(false);
  ui->hostname->setEnabled(true);
  ui->runCommand->setEnabled(true);

  RDTreeWidgetItem *item = ui->hosts->selectedItem();

  RemoteHost *host = item ? getRemoteHost(item) : NULL;

  ui->runCommand->setText(QString());

  if(host)
  {
    if(ui->refreshAll->isEnabled())
      ui->refreshOne->setEnabled(true);

    ui->runCommand->setText(host->runCommand);
    ui->hostname->setText(host->Name());

    ui->addUpdateHost->setText(tr("Update"));

    if(host->IsLocalhost() || host->IsADB())
    {
      // localhost and android hosts cannot be updated or have their run command changed
      ui->addUpdateHost->setEnabled(false);
      ui->runCommand->setEnabled(false);
    }
    else
    {
      // any other host can be deleted
      ui->deleteHost->setEnabled(true);
    }
  }

  updateConnectButton();
}

void RemoteManager::on_hostname_textEdited(const QString &text)
{
  RDTreeWidgetItem *node = NULL;

  for(int i = 0; i < ui->hosts->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *n = ui->hosts->topLevelItem(i);

    if(n->text(0) == text)
    {
      node = n;
      break;
    }
  }

  if(node)
    ui->hosts->setSelectedItem(node);
  else
    ui->hosts->clearSelection();
}

void RemoteManager::on_hosts_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    if(ui->connect->isEnabled())
      on_connect_clicked();
  }

  if(event->key() == Qt::Key_Delete && ui->deleteHost->isEnabled())
    on_deleteHost_clicked();
}

void RemoteManager::on_hostname_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    if(ui->addUpdateHost->isEnabled())
      on_addUpdateHost_clicked();
  }
}

void RemoteManager::on_runCommand_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    if(ui->addUpdateHost->isEnabled())
      on_addUpdateHost_clicked();
  }
}

void RemoteManager::on_addUpdateHost_clicked()
{
  RDTreeWidgetItem *item = ui->hosts->selectedItem();
  if(item && getRemoteHost(item))
    setRunCommand();
  else
    addNewHost();
}

void RemoteManager::on_refreshAll_clicked()
{
  if(m_Lookups.available())
    return;

  ui->refreshOne->setEnabled(false);
  ui->refreshAll->setEnabled(false);

  for(int i = 0; i < ui->hosts->topLevelItemCount(); i++)
  {
    RDTreeWidgetItem *n = ui->hosts->topLevelItem(i);

    n->clear();
    n->setItalic(true);
    n->setIcon(0, Icons::hourglass());

    refreshHost(n);
  }

  updateLookupsStatus();
}

void RemoteManager::on_refreshOne_clicked()
{
  RDTreeWidgetItem *n = ui->hosts->selectedItem();

  if(m_Lookups.available() || !n)
    return;

  ui->refreshOne->setEnabled(false);
  ui->refreshAll->setEnabled(false);

  {
    n->clear();
    n->setItalic(true);
    n->setIcon(0, Icons::hourglass());

    refreshHost(n);
  }

  updateLookupsStatus();
}

void RemoteManager::on_connect_clicked()
{
  RDTreeWidgetItem *node = ui->hosts->selectedItem();

  if(!node)
    return;

  RemoteConnect connect = getRemoteConnect(node);
  RemoteHost *host = getRemoteHost(node);

  if(connect.ident > 0)
  {
    connectToApp(node);
  }
  else if(host)
  {
    if(host->serverRunning)
    {
      QMessageBox::StandardButton res = RDDialog::question(
          this, tr("Remote server shutdown"),
          tr("Are you sure you wish to shut down running remote server on %1?").arg(host->Name()),
          RDDialog::YesNoCancel);

      if(res == QMessageBox::Cancel || res == QMessageBox::No)
        return;

      // shut down
      if(host->connected)
      {
        m_Ctx.Replay().ShutdownServer();
        setRemoteServerLive(node, false, false);
      }
      else
      {
        IRemoteServer *server = NULL;
        ReplayStatus status =
            RENDERDOC_CreateRemoteServerConnection(host->hostname.c_str(), 0, &server);
        if(server)
          server->ShutdownServerAndConnection();
        setRemoteServerLive(node, false, false);

        if(status != ReplayStatus::Succeeded)
          RDDialog::critical(this, tr("Shutdown error"),
                             tr("Error shutting down remote server: %1").arg(ToQStr(status)));
      }

      updateConnectButton();
    }
    else
    {
      // try to run
      ui->refreshOne->setEnabled(false);
      ui->refreshAll->setEnabled(false);

      // hold a ref for running the remote server
      m_Lookups.release();

      LambdaThread *th = new LambdaThread([this, node]() { runRemoteServer(node); });
      th->selfDelete(true);
      th->start();

      updateLookupsStatus();
    }
  }
}

void RemoteManager::on_deleteHost_clicked()
{
  RDTreeWidgetItem *item = ui->hosts->selectedItem();

  if(!item)
    return;

  RemoteHost *host = getRemoteHost(item);

  int itemIdx = ui->hosts->indexOfTopLevelItem(item);

  // don't delete running instances on a host
  if(item->parent() != ui->hosts->invisibleRootItem() || itemIdx < 0 || !host)
    return;

  QString hostname = item->text(0);

  if(hostname == lit("localhost"))
    return;

  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("Deleting host"), tr("Are you sure you wish to delete %1?").arg(hostname),
      RDDialog::YesNoCancel);

  if(res == QMessageBox::Cancel || res == QMessageBox::No)
    return;

  if(res == QMessageBox::Yes)
  {
    int32_t idx = m_Ctx.Config().RemoteHosts.indexOf(host);
    if(idx < 0)
      return;

    // the host will be removed in queueDelete.
    m_Ctx.Config().RemoteHosts.erase(idx);
    m_Ctx.Config().Save();

    item->clear();

    queueDelete(ui->hosts->takeTopLevelItem(itemIdx));

    ui->hosts->clearSelection();

    ui->hostname->setText(hostname);
    on_hostname_textEdited(hostname);
  }
}
