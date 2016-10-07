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

#include "CaptureContext.h"
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QMetaObject>
#include <QStandardPaths>
#include <QTimer>
#include "Windows/MainWindow.h"

CaptureContext::CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent,
                               bool temp, PersistantConfig &cfg)
    : Config(cfg)
{
  m_LogLoaded = false;
  m_LoadInProgress = false;

  m_EventID = 0;

  memset(&m_APIProps, 0, sizeof(m_APIProps));

  qApp->setApplicationVersion(RENDERDOC_GetVersionString());

  m_MainWindow = new MainWindow(this);
  m_MainWindow->show();

  if(!paramFilename.isEmpty())
  {
    QFileInfo fi(paramFilename);

    if(fi.suffix() == "rdc")
    {
      LoadLogfile(paramFilename, temp);
    }
  }
}

CaptureContext::~CaptureContext()
{
  m_Renderer.CloseThread();
  delete m_MainWindow;
}

bool CaptureContext::isRunning()
{
  return m_MainWindow && m_MainWindow->isVisible();
}

QString CaptureContext::ConfigFile(const QString &filename)
{
  QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  QDir dir(path);
  if(!dir.exists())
    dir.mkdir(".");

  return QDir::cleanPath(dir.absoluteFilePath(filename));
}

void CaptureContext::LoadLogfile(QString logFile, bool temporary)
{
  LoadLogfile(-1, "", logFile, temporary);
}

void CaptureContext::LoadLogfile(int proxyRenderer, QString replayHost, QString logFile,
                                 bool temporary)
{
  m_LogFile = logFile;

  m_LoadInProgress = true;

  float loadProgress = 0.0f;
  float postloadProgress = 0.0f;

  // this function call will block until the log is either loaded, or there's some failure
  m_Renderer.Init(proxyRenderer, replayHost, logFile, &loadProgress);

  // if the renderer isn't running, we hit a failure case so display an error message
  if(!m_Renderer.IsRunning())
  {
    QString errmsg = "Unknown error message";
    ReplayCreateStatus status = m_Renderer.GetCreateStatus();
    errmsg = status;

    if(proxyRenderer >= 0)
      RDDialog::critical(NULL, "Error opening log",
                         QString("%1\nFailed to transfer and replay on remote host %2: %3.\n\n"
                                 "Check diagnostic log in Help menu for more details.")
                             .arg(logFile, replayHost, errmsg));
    else
      RDDialog::critical(NULL, "Error opening log",
                         QString("%1\nFailed to open logfile for replay: %1.\n\n"
                                 "Check diagnostic log in Help menu for more details.")
                             .arg(logFile, errmsg));

    m_LoadInProgress = false;

    return;
  }

  m_EventID = 0;

  // fetch initial data like drawcalls, textures and buffers
  m_Renderer.BlockInvoke([this, &postloadProgress](IReplayRenderer *r) {
    r->GetFrameInfo(&m_FrameInfo);

    m_APIProps = r->GetAPIProperties();

    postloadProgress = 0.2f;

    r->GetDrawcalls(&m_Drawcalls);

    postloadProgress = 0.4f;

    r->GetSupportedWindowSystems(&m_WinSystems);

#if defined(RENDERDOC_PLATFORM_WIN32)
    m_CurWinSystem = eWindowingSystem_Win32;
#elif defined(RENDERDOC_PLATFORM_LINUX)
    m_CurWinSystem = eWindowingSystem_Xlib;

    // prefer XCB, if supported
    for(WindowingSystem sys : m_WinSystems)
    {
      if(sys == eWindowingSystem_XCB)
      {
        m_CurWinSystem = eWindowingSystem_XCB;
        break;
      }
    }

    if(m_CurWinSystem == eWindowingSystem_XCB)
      m_XCBConnection = QX11Info::connection();
    else
      m_X11Display = QX11Info::display();
#endif

    r->GetBuffers(&m_BufferList);
    for(FetchBuffer &b : m_BufferList)
      m_Buffers[b.ID] = &b;

    postloadProgress = 0.8f;

    r->GetTextures(&m_TextureList);
    for(FetchTexture &t : m_TextureList)
      m_Textures[t.ID] = &t;

    postloadProgress = 0.9f;

    r->GetD3D11PipelineState(&CurD3D11PipelineState);
    r->GetD3D12PipelineState(&CurD3D12PipelineState);
    r->GetGLPipelineState(&CurGLPipelineState);
    r->GetVulkanPipelineState(&CurVulkanPipelineState);
    CurPipelineState.SetStates(m_APIProps, &CurD3D11PipelineState, &CurD3D12PipelineState,
                               &CurGLPipelineState, &CurVulkanPipelineState);

    UnreadMessageCount = 0;
    AddMessages(m_FrameInfo.debugMessages);

    postloadProgress = 1.0f;
  });

  QThread::msleep(20);

  m_LogLoaded = true;

  QVector<ILogViewerForm *> logviewers(m_LogViewers);

  GUIInvoke::blockcall([&logviewers]() {
    // notify all the registers log viewers that a log has been loaded
    for(ILogViewerForm *logviewer : logviewers)
    {
      if(logviewer)
        logviewer->OnLogfileLoaded();
    }
  });

  m_LoadInProgress = false;
}

void CaptureContext::SetEventID(ILogViewerForm *exclude, uint32_t eventID)
{
  m_EventID = eventID;

  m_Renderer.BlockInvoke([eventID, this](IReplayRenderer *r) {
    r->SetFrameEvent(eventID, false);
    r->GetD3D11PipelineState(&CurD3D11PipelineState);
    r->GetD3D12PipelineState(&CurD3D12PipelineState);
    r->GetGLPipelineState(&CurGLPipelineState);
    r->GetVulkanPipelineState(&CurVulkanPipelineState);
    CurPipelineState.SetStates(m_APIProps, &CurD3D11PipelineState, &CurD3D12PipelineState,
                               &CurGLPipelineState, &CurVulkanPipelineState);
  });

  for(ILogViewerForm *logviewer : m_LogViewers)
  {
    if(logviewer == exclude)
      continue;

    logviewer->OnEventSelected(eventID);
  }
}

void *CaptureContext::FillWindowingData(WId widget)
{
#if defined(WIN32)

  return (void *)widget;

#elif defined(RENDERDOC_PLATFORM_LINUX)

  static XCBWindowData xcb = {
      m_XCBConnection, (xcb_window_t)widget,
  };

  static XlibWindowData xlib = {m_X11Display, (Drawable)widget};

  if(m_CurWinSystem == eWindowingSystem_XCB)
    return &xcb;
  else
    return &xlib;

#else

#error "Unknown platform"

#endif
}

void GUIInvoke::call(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  // TODO: could maybe do away with string compare here via caching
  // invoke->metaObject()->indexOfMethod("doInvoke"); ?

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  QMetaObject::invokeMethod(invoke, "doInvoke", Qt::QueuedConnection);
}

void GUIInvoke::blockcall(const std::function<void()> &f)
{
  if(qApp->thread() == QThread::currentThread())
  {
    f();
    return;
  }

  // TODO: could maybe do away with string compare here via caching
  // invoke->metaObject()->indexOfMethod("doInvoke"); ?

  GUIInvoke *invoke = new GUIInvoke(f);
  invoke->moveToThread(qApp->thread());
  QMetaObject::invokeMethod(invoke, "doInvoke", Qt::BlockingQueuedConnection);
}

void RDDialog::show(QDialog *dialog)
{
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->show();
  QEventLoop loop;
  while(dialog->isVisible())
  {
    loop.processEvents(QEventLoop::WaitForMoreEvents);
    QCoreApplication::sendPostedEvents();
  }
}

QMessageBox::StandardButton RDDialog::messageBox(QMessageBox::Icon icon, QWidget *parent,
                                                 const QString &title, const QString &text,
                                                 QMessageBox::StandardButtons buttons,
                                                 QMessageBox::StandardButton defaultButton)
{
  QMessageBox mb(icon, title, text, buttons, parent);
  mb.setDefaultButton(defaultButton);
  show(&mb);
  return mb.standardButton(mb.clickedButton());
}

QString RDDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir,
                                       QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, QString());
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setFileMode(QFileDialog::DirectoryOnly);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptOpen);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}

QString RDDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir,
                                  const QString &filter, QString *selectedFilter,
                                  QFileDialog::Options options)
{
  QFileDialog fd(parent, caption, dir, filter);
  fd.setAcceptMode(QFileDialog::AcceptSave);
  fd.setOptions(options);
  show(&fd);

  if(fd.result() == QFileDialog::Accepted)
  {
    if(selectedFilter)
      *selectedFilter = fd.selectedNameFilter();

    QStringList files = fd.selectedFiles();
    if(!files.isEmpty())
      return files[0];
  }

  return QString();
}
