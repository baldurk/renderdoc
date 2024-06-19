/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <QDateTime>
#include <QFrame>
#include <QMutex>
#include <QSemaphore>
#include <QTimer>
#include "Code/Interface/QRDInterface.h"

namespace Ui
{
class LiveCapture;
}

class QSplitter;
class QAction;
class QToolButton;
class QListWidgetItem;
class QMenu;
class LambdaThread;
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

  void QueueCapture(int frameNumber, int numFrames);
  const QString &hostname() { return m_Hostname; }
  void cleanItems();
  void fileSaved(QString from, QString to);
  int unsavedCaptureCount();
  bool checkAllowClose(int totalUnsavedCaptures, bool &noToAll);

public slots:
  bool checkAllowClose();

private slots:
  void on_captures_itemSelectionChanged();
  void on_captures_mouseClicked(QMouseEvent *e);
  void on_captures_itemActivated(QListWidgetItem *item);
  void on_childProcesses_itemActivated(QListWidgetItem *item);
  void on_triggerImmediateCapture_clicked();
  void on_cycleActiveWindow_clicked();
  void on_triggerDelayedCapture_clicked();
  void on_queueCap_clicked();
  void on_previewSplit_splitterMoved(int pos, int index);
  void on_apiIcon_clicked(QMouseEvent *event);

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

  struct Capture
  {
    uint32_t remoteID;
    QString name;
    QString api;
    QDateTime timestamp;
    uint32_t frameNumber;
    uint64_t byteSize;
    QString title;

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

  struct APIStatus
  {
    APIStatus() = default;
    APIStatus(bool p, bool s, rdcstr m) : presenting(p), supported(s), supportMessage(m) {}
    bool presenting = false;
    bool supported = false;
    rdcstr supportMessage;
  };

  Capture *GetCapture(QListWidgetItem *item);
  void AddCapture(QListWidgetItem *item, Capture *cap);

  QString MakeText(Capture *cap);
  QImage MakeThumb(const QImage &screenshot);

  void updateAPIStatus();

  void connectionThreadEntry();
  void captureCopied(uint32_t ID, const QString &localPath);
  void captureAdded(const QString &name, const NewCaptureData &newCapture);
  void connectionClosed();

  void selfClose();

  void killThread();

  void setTitle(const QString &title);
  void openCapture(Capture *cap);
  bool saveCapture(Capture *cap, QString path);
  bool checkAllowDelete();
  void deleteCaptureUnprompted(QListWidgetItem *item);

  bool isLocal() const;

  Ui::LiveCapture *ui;
  ICaptureContext &m_Ctx;
  QString m_Hostname;
  QString m_HostFriendlyname;
  uint32_t m_RemoteIdent;
  MainWindow *m_Main;

  LambdaThread *m_ConnectThread = NULL;
  QSemaphore m_TriggerCapture;
  QSemaphore m_QueueCapture;
  QSemaphore m_CopyCapture;
  QSemaphore m_Disconnect;
  QSemaphore m_CycleWindow;
  int m_CaptureNumFrames = 1;
  int m_QueueCaptureFrameNum = 0;
  int m_CaptureCounter = 0;
  QSemaphore m_Connected;

  uint32_t m_CopyCaptureID = ~0U;
  QString m_CopyCaptureLocalPath;
  QMutex m_DeleteCapturesLock;
  QVector<uint32_t> m_DeleteCaptures;

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
  QMap<QString, APIStatus> m_APIs;
};
