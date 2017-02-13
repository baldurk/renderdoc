/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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
#include "Code/RemoteHost.h"
#include "Code/Resources.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/MainWindow.h"
#include "ui_RemoteManager.h"

struct RemoteConnect
{
  RemoteConnect() {}
  RemoteConnect(const QString &h, uint32_t i) : host(h), ident(i) {}
  QString host;
  uint32_t ident = 0;
};

Q_DECLARE_METATYPE(RemoteConnect);

static void setRemoteConnect(QTreeWidgetItem *item, const RemoteConnect &connect)
{
  if(!item)
    return;

  item->setData(0, Qt::UserRole, QVariant::fromValue(connect));
}

static RemoteConnect getRemoteConnect(QTreeWidgetItem *item)
{
  if(!item)
    return RemoteConnect();

  return item->data(0, Qt::UserRole).value<RemoteConnect>();
}

static void setRemoteHost(QTreeWidgetItem *item, RemoteHost *host)
{
  if(!item)
    return;

  item->setData(0, Qt::UserRole + 1, QVariant::fromValue((uintptr_t)host));
}

static RemoteHost *getRemoteHost(QTreeWidgetItem *item)
{
  if(!item)
    return NULL;

  return (RemoteHost *)item->data(0, Qt::UserRole + 1).value<uintptr_t>();
}

static void setItalic(QTreeWidgetItem *node, bool italic)
{
  QFont f = node->font(0);
  f.setItalic(italic);
  node->setFont(0, f);
  node->setFont(1, f);
}

RemoteManager::RemoteManager(CaptureContext &ctx, MainWindow *main)
    : QDialog(NULL), ui(new Ui::RemoteManager), m_Ctx(ctx), m_Main(main)
{
  ui->setupUi(this);

  ui->hosts->setClearSelectionOnFocusLoss(false);

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

  m_Ctx.Config.AddAndroidHosts();

  for(RemoteHost *h : m_Ctx.Config.RemoteHosts)
    addHost(h);

  on_hosts_itemClicked(ui->hosts->topLevelItem(0), 0);
}

RemoteManager::~RemoteManager()
{
  delete ui;
}

void RemoteManager::setRemoteServerLive(QTreeWidgetItem *node, bool live, bool busy)
{
  RemoteHost *host = getRemoteHost(node);

  if(!host)
    return;

  host->ServerRunning = live;
  host->Busy = busy;

  if(host->Hostname == "localhost")
  {
    node->setIcon(0, QIcon());
    node->setText(1, "");
  }
  else
  {
    QString text = live ? tr("Remote server running") : tr("No remote server");

    if(host->Connected)
      text += tr(" (Active Context)");
    else if(host->VersionMismatch)
      text += tr(" (Version Mismatch)");
    else if(host->Busy)
      text += tr(" (Busy)");

    node->setText(1, text);

    node->setIcon(0,
                  QIcon(QPixmap(QString::fromUtf8(live ? ":/connect.png" : ":/disconnect.png"))));
  }
}

bool RemoteManager::isRemoteServerLive(QTreeWidgetItem *node)
{
  RemoteHost *host = getRemoteHost(node);
  return host && host->ServerRunning;
}

void RemoteManager::addHost(RemoteHost *host)
{
  QTreeWidgetItem *node = makeTreeNode({host->Hostname, "..."});

  setItalic(node, true);
  node->setIcon(0, Icons::hourglass());
  setRemoteHost(node, host);

  ui->hosts->addTopLevelItem(node);
  ui->hosts->clearSelection();
  node->setSelected(true);

  ui->refreshOne->setEnabled(false);
  ui->refreshAll->setEnabled(false);

  m_Lookups.release();

  refreshHost(node);

  updateLookupsStatus();
}

void RemoteManager::updateLookupsStatus()
{
  lookupsProgressFlow->setVisible(!ui->refreshAll->isEnabled());
  ui->progressCount->setText(tr("%1 lookups remaining").arg(m_Lookups.available()));
}

void RemoteManager::runRemoteServer(QTreeWidgetItem *node)
{
  RemoteHost *host = getRemoteHost(node);

  if(!host)
    return;

  host->Launch();

  // now refresh this host
  refreshHost(node);
}

void RemoteManager::refreshHost(QTreeWidgetItem *node)
{
  RemoteHost *host = getRemoteHost(node);

  if(!host)
    return;

  // this function looks up the remote connections and for each one open
  // queries it for the API, target (usually executable name) and if any user is already connected
  LambdaThread *th = new LambdaThread([this, node, host]() {
    QString hostname = node->text(0);

    QByteArray username = GetSystemUsername().toUtf8();

    host->CheckStatus();

    GUIInvoke::call(
        [this, node, host]() { setRemoteServerLive(node, host->ServerRunning, host->Busy); });

    QByteArray hostnameBytes = hostname.toUtf8();

    uint32_t nextIdent = 0;

    for(;;)
    {
      // just a sanity check to make sure we don't hit some unexpected case and infinite loop
      uint32_t prevIdent = nextIdent;

      nextIdent = RENDERDOC_EnumerateRemoteTargets(hostnameBytes.data(), nextIdent);

      if(nextIdent == ~0U || prevIdent >= nextIdent)
        break;

      TargetControl *conn =
          RENDERDOC_CreateTargetControl(hostnameBytes.data(), nextIdent, username.data(), false);

      if(conn)
      {
        QString target = QString::fromUtf8(conn->GetTarget());
        QString api = QString::fromUtf8(conn->GetAPI());
        QString busy = QString::fromUtf8(conn->GetBusyClient());

        QString running;

        if(busy != "")
          running = tr("Running %1, %2 is connected").arg(api).arg(busy);
        else
          running = tr("Running %1").arg(api);

        RemoteConnect tag(hostname, nextIdent);

        GUIInvoke::call([this, node, target, running, tag]() {
          QTreeWidgetItem *child = makeTreeNode({target, running});
          setRemoteConnect(child, tag);
          node->addChild(child);
          ui->hosts->expandItem(node);
        });

        conn->Shutdown();
      }
    }

    GUIInvoke::call([node]() { setItalic(node, false); });

    m_Lookups.acquire();

    GUIInvoke::call([this]() { lookupComplete(); });
  });
  th->selfDelete(true);
  th->start();
}

// don't allow the user to refresh until all pending connections have been checked
// (to stop flooding)
void RemoteManager::lookupComplete()
{
  if(m_Lookups.available() == 0)
  {
    ui->refreshOne->setEnabled(true);
    ui->refreshAll->setEnabled(true);

    if(!isVisible())
    {
      delete this;
      return;
    }
  }

  updateConnectButton();
  updateLookupsStatus();
}

void RemoteManager::connectToApp(QTreeWidgetItem *node)
{
  if(node)
  {
    RemoteConnect connect = getRemoteConnect(node);

    if(connect.ident > 0)
    {
      LiveCapture *live = new LiveCapture(m_Ctx, connect.host, connect.ident, m_Main, m_Main);
      m_Main->ShowLiveCapture(live);
      accept();
      lookupComplete();
    }
  }
}

void RemoteManager::updateConnectButton()
{
  if(!ui->hosts->selectedItems().isEmpty())
  {
    QTreeWidgetItem *item = ui->hosts->selectedItems()[0];

    ui->connect->setEnabled(true);
    ui->connect->setText(tr("Connect to App"));

    RemoteHost *host = getRemoteHost(item);

    if(host)
    {
      if(host->Hostname == "localhost")
      {
        ui->connect->setEnabled(false);
      }
      else if(host->ServerRunning)
      {
        ui->connect->setText(tr("Shutdown"));

        if(host->Busy && !host->Connected)
          ui->connect->setEnabled(false);
      }
      else
      {
        ui->connect->setText(tr("Run Server"));

        if(host->RunCommand == "")
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

    for(int i = 0; i < m_Ctx.Config.RemoteHosts.count(); i++)
    {
      if(m_Ctx.Config.RemoteHosts[i]->Hostname.compare(host, Qt::CaseInsensitive) == 0)
      {
        found = true;
        break;
      }
    }

    if(!found)
    {
      RemoteHost *h = new RemoteHost();
      h->Hostname = host;
      h->RunCommand = ui->runCommand->text().trimmed();

      m_Ctx.Config.RemoteHosts.push_back(h);
      m_Ctx.Config.Save();

      addHost(h);
    }
  }
  ui->hostname->setText(host);
  on_hostname_textEdited(host);
}

void RemoteManager::setRunCommand()
{
  if(ui->hosts->selectedItems().isEmpty())
    return;

  RemoteHost *h = getRemoteHost(ui->hosts->selectedItems()[0]);

  if(h)
  {
    h->RunCommand = ui->runCommand->text().trimmed();
    m_Ctx.Config.Save();
  }
}

void RemoteManager::on_hosts_itemActivated(QTreeWidgetItem *item, int column)
{
  RemoteConnect connect = getRemoteConnect(item);
  if(connect.ident > 0)
    connectToApp(item);
}

void RemoteManager::on_hosts_itemClicked(QTreeWidgetItem *item, int column)
{
  ui->addUpdateHost->setText(tr("Add"));
  ui->addUpdateHost->setEnabled(true);
  ui->deleteHost->setEnabled(false);
  ui->refreshOne->setEnabled(false);
  ui->hostname->setEnabled(true);
  ui->addUpdateHost->setEnabled(true);
  ui->runCommand->setEnabled(true);

  ui->runCommand->setText("");
  ui->hostname->setText("");

  RemoteHost *host = getRemoteHost(item);

  if(host)
  {
    if(ui->refreshAll->isEnabled())
      ui->refreshOne->setEnabled(true);

    if(host->Hostname == "localhost")
    {
      ui->runCommand->setText("");
      ui->hostname->setText("");
    }
    else
    {
      ui->deleteHost->setEnabled(true);
      ui->runCommand->setText(host->RunCommand);
      ui->hostname->setText(host->Hostname);

      ui->addUpdateHost->setText(tr("Update"));
    }
  }

  updateConnectButton();
}

void RemoteManager::on_hostname_textEdited(const QString &text)
{
  ui->addUpdateHost->setText(tr("Add"));
  ui->addUpdateHost->setEnabled(true);
  ui->deleteHost->setEnabled(false);
  ui->refreshOne->setEnabled(false);
  ui->hostname->setEnabled(true);
  ui->addUpdateHost->setEnabled(true);
  ui->runCommand->setEnabled(true);

  QTreeWidgetItem *node = NULL;

  for(int i = 0; i < ui->hosts->topLevelItemCount(); i++)
  {
    QTreeWidgetItem *n = ui->hosts->topLevelItem(i);

    RemoteHost *host = getRemoteHost(n);

    if(n->text(0) == text)
    {
      if(ui->refreshAll->isEnabled())
        ui->refreshOne->setEnabled(true);

      ui->addUpdateHost->setText(tr("Update"));

      if(text == "localhost")
      {
        ui->hostname->setEnabled(false);
        ui->addUpdateHost->setEnabled(false);
        ui->runCommand->setEnabled(false);
      }
      else
      {
        ui->deleteHost->setEnabled(true);
        ui->runCommand->setText(host->RunCommand);
      }

      node = n;
      break;
    }
  }

  ui->hosts->clearSelection();
  if(node)
    node->setSelected(true);

  updateConnectButton();
}

void RemoteManager::on_hosts_keyPress(QKeyEvent *event)
{
  if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    if(ui->connect->isEnabled())
      on_connect_clicked();
  }

  if(event->key() == Qt::Key_Delete)
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
  if(!ui->hosts->selectedItems().isEmpty() && getRemoteHost(ui->hosts->selectedItems()[0]))
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
    QTreeWidgetItem *n = ui->hosts->topLevelItem(i);

    deleteChildren(n);
    setItalic(n, true);
    n->setIcon(0, Icons::hourglass());

    refreshHost(n);
  }

  updateLookupsStatus();
}

void RemoteManager::on_refreshOne_clicked()
{
  if(m_Lookups.available() || ui->hosts->selectedItems().isEmpty())
    return;

  ui->refreshOne->setEnabled(false);
  ui->refreshAll->setEnabled(false);

  QTreeWidgetItem *n = ui->hosts->selectedItems()[0];
  {
    deleteChildren(n);
    setItalic(n, true);
    n->setIcon(0, Icons::hourglass());

    refreshHost(n);
  }

  updateLookupsStatus();
}

void RemoteManager::on_connect_clicked()
{
  if(ui->hosts->selectedItems().isEmpty())
    return;

  QTreeWidgetItem *node = ui->hosts->selectedItems()[0];

  RemoteConnect connect = getRemoteConnect(node);
  RemoteHost *host = getRemoteHost(node);

  if(connect.ident > 0)
  {
    connectToApp(node);
  }
  else if(host)
  {
    if(host->ServerRunning)
    {
      QMessageBox::StandardButton res = RDDialog::question(
          this, tr("Remote server shutdown"),
          tr("Are you sure you wish to shut down running remote server on %1?").arg(host->Hostname),
          RDDialog::YesNoCancel);

      if(res == QMessageBox::Cancel || res == QMessageBox::No)
        return;

      // shut down
      if(host->Connected)
      {
        m_Ctx.Renderer().ShutdownServer();
        setRemoteServerLive(node, false, false);
      }
      else
      {
        RemoteServer *server = NULL;
        ReplayCreateStatus status =
            RENDERDOC_CreateRemoteServerConnection(host->Hostname.toUtf8().data(), 0, &server);
        if(server)
          server->ShutdownServerAndConnection();
        setRemoteServerLive(node, false, false);

        if(status != eReplayCreate_Success)
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
  if(ui->hosts->selectedItems().isEmpty())
    return;

  QTreeWidgetItem *item = ui->hosts->selectedItems()[0];

  RemoteHost *host = getRemoteHost(item);

  // don't delete running instances on a host
  if(item->parent() != NULL || !host)
    return;

  QString hostname = item->text(0);

  if(hostname == "localhost")
    return;

  QMessageBox::StandardButton res = RDDialog::question(
      this, tr("Deleting host"), tr("Are you sure you wish to delete %1?").arg(hostname),
      RDDialog::YesNoCancel);

  if(res == QMessageBox::Cancel || res == QMessageBox::No)
    return;

  if(res == QMessageBox::Yes)
  {
    int idx = m_Ctx.Config.RemoteHosts.indexOf(host);
    delete m_Ctx.Config.RemoteHosts.takeAt(idx);
    m_Ctx.Config.Save();

    deleteChildren(item);

    delete ui->hosts->takeTopLevelItem(ui->hosts->indexOfTopLevelItem(item));

    ui->hosts->clearSelection();

    ui->hostname->setText(hostname);
    on_hostname_textEdited(hostname);
  }
}
