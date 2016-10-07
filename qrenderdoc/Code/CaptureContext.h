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

#pragma once

#include <QDebug>
#include <QFileDialog>
#include <QList>
#include <QMap>
#include <QMessageBox>
#include <QString>
#include <QtWidgets/QWidget>
#include "CommonPipelineState.h"
#include "PersistantConfig.h"
#include "RenderManager.h"

#if defined(RENDERDOC_PLATFORM_LINUX)
#include <QX11Info>
#endif

struct ILogViewerForm
{
  virtual void OnLogfileLoaded() = 0;
  virtual void OnLogfileClosed() = 0;
  virtual void OnEventSelected(uint32_t eventID) = 0;
};

struct ILogLoadProgressListener
{
  virtual void LogfileProgressBegin() = 0;
  virtual void LogfileProgress(float progress) = 0;
};

class MainWindow;

class CaptureContext
{
public:
  CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent, bool temp,
                 PersistantConfig &cfg);
  ~CaptureContext();

  bool isRunning();

  static QString ConfigFile(const QString &filename);

  static QString TempLogFilename(QString appname);

  //////////////////////////////////////////////////////////////////////////////
  // Control functions

  // loading a local log, no remote replay
  void LoadLogfile(QString logFile, bool temporary);

  // when loading a log while replaying remotely, provide the proxy renderer that will be used
  // as well as the hostname to replay on.
  void LoadLogfile(int proxyRenderer, QString replayHost, QString logFile, bool temporary);

  void CloseLogfile();

  void SetEventID(ILogViewerForm *exclude, uint32_t eventID);

  void AddLogProgressListener(ILogLoadProgressListener *p);

  void AddLogViewer(ILogViewerForm *f)
  {
    m_LogViewers.push_back(f);

    if(LogLoaded())
    {
      f->OnLogfileLoaded();
      f->OnEventSelected(CurEvent());
    }
  }

  void RemoveLogViewer(ILogViewerForm *f) { m_LogViewers.removeAll(f); }
  //////////////////////////////////////////////////////////////////////////////
  // Singleton windows

  /*
  private MainWindow m_MainWindow = null;
  private EventBrowser m_EventBrowser = null;
  private APIInspector m_APIInspector = null;
  private DebugMessages m_DebugMessages = null;
  private TimelineBar m_TimelineBar = null;
  private TextureViewer m_TextureViewer = null;
  private PipelineStateViewer m_PipelineStateViewer = null;
  */

  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  RenderManager *Renderer() { return &m_Renderer; }
  bool LogLoaded() { return m_LogLoaded; }
  bool LogLoading() { return m_LoadInProgress; }
  QString LogFilename() { return m_LogFile; }
  const FetchFrameInfo &FrameInfo() { return m_FrameInfo; }
  const APIProperties &APIProps() { return m_APIProps; }
  uint32_t CurEvent() { return m_EventID; }
  const FetchDrawcall *CurDrawcall() { return GetDrawcall(CurEvent()); }
  const rdctype::array<FetchDrawcall> &CurDrawcalls() { return m_Drawcalls; }
  FetchTexture *GetTexture(ResourceId id) { return m_Textures[id]; }
  const rdctype::array<FetchTexture> &GetTextures() { return m_TextureList; }
  FetchBuffer *GetBuffer(ResourceId id) { return m_Buffers[id]; }
  const rdctype::array<FetchBuffer> &GetBuffers() { return m_BufferList; }
  QVector<DebugMessage> DebugMessages;
  int UnreadMessageCount;
  void AddMessages(rdctype::array<DebugMessage> &msgs)
  {
    UnreadMessageCount += msgs.count;
    for(DebugMessage &msg : msgs)
      DebugMessages.push_back(msg);
  }

  WindowingSystem m_CurWinSystem;
  void *FillWindowingData(WId widget);

  D3D11PipelineState CurD3D11PipelineState;
  D3D12PipelineState CurD3D12PipelineState;
  GLPipelineState CurGLPipelineState;
  VulkanPipelineState CurVulkanPipelineState;
  CommonPipelineState CurPipelineState;

  PersistantConfig &Config;

private:
  RenderManager m_Renderer;

  QVector<ILogViewerForm *> m_LogViewers;
  QVector<ILogLoadProgressListener *> m_ProgressListeners;

  bool m_LogLoaded, m_LoadInProgress;
  QString m_LogFile;

  uint32_t m_EventID;

  const FetchDrawcall *GetDrawcall(const rdctype::array<FetchDrawcall> &draws, uint32_t eventID)
  {
    for(const FetchDrawcall &d : draws)
    {
      if(!d.children.empty())
      {
        const FetchDrawcall *draw = GetDrawcall(d.children, eventID);
        if(draw != NULL)
          return draw;
      }

      if(d.eventID == eventID)
        return &d;
    }

    return NULL;
  }

  const FetchDrawcall *GetDrawcall(uint32_t eventID) { return GetDrawcall(m_Drawcalls, eventID); }
  rdctype::array<FetchDrawcall> m_Drawcalls;

  APIProperties m_APIProps;
  FetchFrameInfo m_FrameInfo;

  QMap<ResourceId, FetchTexture *> m_Textures;
  rdctype::array<FetchTexture> m_TextureList;
  QMap<ResourceId, FetchBuffer *> m_Buffers;
  rdctype::array<FetchBuffer> m_BufferList;

  rdctype::array<WindowingSystem> m_WinSystems;

#if defined(RENDERDOC_PLATFORM_LINUX)
  xcb_connection_t *m_XCBConnection;
  Display *m_X11Display;
#endif

  // Windows
  MainWindow *m_MainWindow;
};

// Utility class for invoking a lambda on the GUI thread.
// This is supported by QTimer::singleShot on Qt 5.4 but it's probably
// wise not to require a higher version that necessary.
#include <functional>

class GUIInvoke : public QObject
{
private:
  Q_OBJECT
  GUIInvoke(const std::function<void()> &f) : func(f) {}
  std::function<void()> func;

public:
  static void call(const std::function<void()> &f);
  static void blockcall(const std::function<void()> &f);

protected slots:
  void doInvoke()
  {
    func();
    deleteLater();
  }
};

// Utility class for calling a lambda on a new thread.
#include <QThread>

class LambdaThread : public QObject
{
private:
  Q_OBJECT

  std::function<void()> m_func;
  QThread *m_Thread;

public slots:
  void process()
  {
    m_func();
    m_Thread->quit();
    deleteLater();
    m_Thread->deleteLater();
    m_Thread = NULL;
  }

public:
  explicit LambdaThread(std::function<void()> f)
  {
    m_Thread = new QThread();
    m_func = f;
    moveToThread(m_Thread);
    QObject::connect(m_Thread, &QThread::started, this, &LambdaThread::process);
  }

  void start(QThread::Priority prio = QThread::InheritPriority) { m_Thread->start(prio); }
  bool isRunning() { return m_Thread; }
};

// helper for doing a manual blocking invoke of a dialog
struct RDDialog
{
  static void show(QDialog *dialog);
  static QMessageBox::StandardButton messageBox(
      QMessageBox::Icon, QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

  static QMessageBox::StandardButton information(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Information, parent, title, text, buttons, defaultButton);
  }

  static QMessageBox::StandardButton question(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes |
                                                                          QMessageBox::No),
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Question, parent, title, text, buttons, defaultButton);
  }

  static QMessageBox::StandardButton warning(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Warning, parent, title, text, buttons, defaultButton);
  }

  static QMessageBox::StandardButton critical(
      QWidget *parent, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
  {
    return messageBox(QMessageBox::Critical, parent, title, text, buttons, defaultButton);
  }

  static QString getExistingDirectory(QWidget *parent = NULL, const QString &caption = QString(),
                                      const QString &dir = QString(),
                                      QFileDialog::Options options = QFileDialog::ShowDirsOnly);

  static QString getOpenFileName(QWidget *parent = NULL, const QString &caption = QString(),
                                 const QString &dir = QString(), const QString &filter = QString(),
                                 QString *selectedFilter = NULL,
                                 QFileDialog::Options options = QFileDialog::Options());

  static QString getSaveFileName(QWidget *parent = NULL, const QString &caption = QString(),
                                 const QString &dir = QString(), const QString &filter = QString(),
                                 QString *selectedFilter = NULL,
                                 QFileDialog::Options options = QFileDialog::Options());
};

// useful delegate for enforcing a given size
#include <QItemDelegate>

class SizeDelegate : public QItemDelegate
{
private:
  Q_OBJECT

  QSize m_Size;

public:
  SizeDelegate(QSize size) : m_Size(size) {}
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
  {
    return m_Size;
  }
};
