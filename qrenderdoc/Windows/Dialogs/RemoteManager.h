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

#pragma once

#include <QDialog>
#include <QSemaphore>

namespace Ui
{
class RemoteManager;
}

class QTreeWidgetItem;
struct ICaptureContext;
class MainWindow;
class RemoteHost;

class RemoteManager : public QDialog
{
  Q_OBJECT

public:
  explicit RemoteManager(ICaptureContext &ctx, MainWindow *main);
  ~RemoteManager();

private slots:
  // automatic slots
  void on_hosts_itemClicked(QTreeWidgetItem *item, int column);
  void on_hosts_itemActivated(QTreeWidgetItem *item, int column);
  void on_hostname_textEdited(const QString &text);
  void on_hosts_keyPress(QKeyEvent *event);
  void on_hostname_keyPress(QKeyEvent *event);
  void on_runCommand_keyPress(QKeyEvent *event);
  void on_refreshOne_clicked();
  void on_addUpdateHost_clicked();
  void on_refreshAll_clicked();
  void on_connect_clicked();
  void on_deleteHost_clicked();

private:
  Ui::RemoteManager *ui;
  ICaptureContext &m_Ctx;
  MainWindow *m_Main;
  QWidget *lookupsProgressFlow;

  QSemaphore m_Lookups;

  bool isRemoteServerLive(QTreeWidgetItem *node);
  void setRemoteServerLive(QTreeWidgetItem *node, bool live, bool busy);

  void addHost(RemoteHost *host);
  void updateLookupsStatus();
  void runRemoteServer(QTreeWidgetItem *node);

  void refreshHost(QTreeWidgetItem *node);
  void lookupComplete();
  void connectToApp(QTreeWidgetItem *node);

  void updateConnectButton();
  void addNewHost();
  void setRunCommand();
};
