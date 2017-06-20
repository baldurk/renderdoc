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

#pragma once

#include <QFrame>
#include <QMutex>
#include <QSemaphore>
#include <QTimer>
#include "Code/CaptureContext.h"

namespace Ui
{
class LiveCapture;
}

class QSplitter;
class QAction;
class QToolButton;
class QListWidgetItem;
class RDLabel;
class MainWindow;
class QKeyEvent;
class NameEditOnlyDelegate;

class LiveCapture : public QFrame
{
  Q_OBJECT

public:
  explicit LiveCapture(ICaptureContext &ctx, const QString &hostname, const QString &friendlyname,
                       uint32_t ident, MainWindow *main, QWidget *parent = 0);

  ~LiveCapture();

  void QueueCapture(int frameNumber);
  const QString &hostname() { return m_Hostname; }
  void cleanItems();

public slots:
  bool checkAllowClose();

private slots:
  void on_captures_itemSelectionChanged();
  void on_captures_mouseClicked(QMouseEvent *e);
  void on_captures_itemActivated(QListWidgetItem *item);
  void on_childProcesses_itemActivated(QListWidgetItem *item);
  void on_triggerCapture_clicked();
  void on_queueCap_clicked();
  void on_previewSplit_splitterMoved(int pos, int index);

  // manual slots
  void captures_keyPress(QKeyEvent *e);

  void childUpdate();
  void captureCountdownTick();

  void openCapture_triggered();
  void openNewWindow_triggered();
  void saveCapture_triggered();
  void deleteCapture_triggered();
  void previewToggle_toggled(bool);

  void preview_mouseClick(QMouseEvent *e);
  void preview_mouseMove(QMouseEvent *e);

private:
  void showEvent(QShowEvent *event) override;

  friend class NameEditOnlyDelegate;

  struct CaptureLog
  {
    uint32_t remoteID;
    QString name;
    QString api;
    QDateTime timestamp;

    QImage thumb;

    bool saved;
    bool opened;

    QString path;
    bool local;
  };

  struct ChildProcess
  {
    uint32_t PID = 0;
    uint32_t ident = 0;
    bool added = false;
  };

  CaptureLog *GetLog(QListWidgetItem *item);
  void SetLog(QListWidgetItem *item, CaptureLog *log);

  QString MakeText(CaptureLog *log);
  QImage MakeThumb(const QImage &screenshot);

  void connectionThreadEntry();
  void captureCopied(uint32_t ID, const QString &localPath);
  void captureAdded(uint32_t ID, const QString &executable, const QString &api,
                    const rdctype::array<byte> &thumbnail, int32_t thumbWidth, int32_t thumbHeight,
                    QDateTime timestamp, const QString &path, bool local);
  void connectionClosed();

  void selfClose();

  void killThread();

  void setTitle(const QString &title);
  void openCapture(CaptureLog *log);
  bool saveCapture(CaptureLog *log);
  bool checkAllowDelete();
  void deleteCaptureUnprompted(QListWidgetItem *item);

  Ui::LiveCapture *ui;
  ICaptureContext &m_Ctx;
  QString m_Hostname;
  QString m_HostFriendlyname;
  uint32_t m_RemoteIdent;
  MainWindow *m_Main;

  LambdaThread *m_ConnectThread = NULL;
  bool m_TriggerCapture = false;
  bool m_QueueCapture = false;
  int m_CaptureNumFrames = 1;
  int m_CaptureFrameNum = 0;
  int m_CaptureCounter = 0;
  QSemaphore m_Disconnect;
  ITargetControl *m_Connection = NULL;

  uint32_t m_CopyLogID = ~0U;
  QString m_CopyLogLocalPath;
  QMutex m_DeleteLogsLock;
  QVector<uint32_t> m_DeleteLogs;

  bool m_IgnoreThreadClosed = false;
  bool m_IgnorePreviewToggle = false;

  QMenu *m_ContextMenu = NULL;

  QAction *previewToggle;
  QToolButton *openButton;
  QAction *newWindowAction;
  QAction *saveAction;
  QAction *deleteAction;

  QTimer childUpdateTimer, countdownTimer;

  QPoint previewDragStart;

  QMutex m_ChildrenLock;
  QList<ChildProcess> m_Children;
};
