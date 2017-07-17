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

#include "CaptureContext.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressDialog>
#include <QTimer>
#include "Windows/APIInspector.h"
#include "Windows/BufferViewer.h"
#include "Windows/ConstantBufferPreviewer.h"
#include "Windows/DebugMessageView.h"
#include "Windows/Dialogs/CaptureDialog.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/EventBrowser.h"
#include "Windows/MainWindow.h"
#include "Windows/PipelineState/PipelineStateViewer.h"
#include "Windows/PixelHistoryView.h"
#include "Windows/PythonShell.h"
#include "Windows/ShaderViewer.h"
#include "Windows/StatisticsViewer.h"
#include "Windows/TextureViewer.h"
#include "Windows/TimelineBar.h"
#include "QRDUtils.h"

CaptureContext::CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent,
                               bool temp, PersistantConfig &cfg)
    : m_Config(cfg)
{
  m_LogLoaded = false;
  m_LoadInProgress = false;

  m_EventID = 0;

  memset(&m_APIProps, 0, sizeof(m_APIProps));

  qApp->setApplicationVersion(QString::fromLatin1(RENDERDOC_GetVersionString()));

  m_Icon = new QIcon();
  m_Icon->addFile(QStringLiteral(":/logo.svg"), QSize(), QIcon::Normal, QIcon::Off);

  m_MainWindow = new MainWindow(*this);
  m_MainWindow->show();

  if(remoteIdent != 0)
  {
    m_MainWindow->ShowLiveCapture(
        new LiveCapture(*this, remoteHost, remoteHost, remoteIdent, m_MainWindow, m_MainWindow));
  }

  if(!paramFilename.isEmpty())
  {
    QFileInfo fi(paramFilename);

    m_MainWindow->LoadFromFilename(paramFilename, temp);
    if(temp)
      m_MainWindow->takeLogOwnership();
  }
}

CaptureContext::~CaptureContext()
{
  delete m_Icon;
  m_Renderer.CloseThread();
  delete m_MainWindow;
}

bool CaptureContext::isRunning()
{
  return m_MainWindow && m_MainWindow->isVisible();
}

QString CaptureContext::TempLogFilename(QString appname)
{
  QString folder = Config().TemporaryCaptureDirectory;

  QDir dir(folder);

  if(folder.isEmpty() || !dir.exists())
  {
    dir = QDir(QDir::tempPath());

    dir.mkdir(lit("RenderDoc"));

    dir = QDir(dir.absoluteFilePath(lit("RenderDoc")));
  }

  return dir.absoluteFilePath(
      QFormatStr("%1_%2.rdc")
          .arg(appname)
          .arg(QDateTime::currentDateTimeUtc().toString(lit("yyyy.MM.dd_HH.mm.ss"))));
}

void CaptureContext::LoadLogfile(const QString &logFile, const QString &origFilename,
                                 bool temporary, bool local)
{
  m_LoadInProgress = true;

  LambdaThread *thread = new LambdaThread([this, logFile, origFilename, temporary, local]() {
    LoadLogfileThreaded(logFile, origFilename, temporary, local);
  });
  thread->selfDelete(true);
  thread->start();

  ShowProgressDialog(m_MainWindow, tr("Loading Capture: %1").arg(origFilename),
                     [this]() { return !m_LoadInProgress; },
                     [this]() { return UpdateLoadProgress(); });

  m_MainWindow->setProgress(-1.0f);

  if(m_LogLoaded)
  {
    QVector<ILogViewer *> logviewers(m_LogViewers);

    // make sure we're on a consistent event before invoking log viewer forms
    if(m_LastDrawcall)
      SetEventID(logviewers, m_LastDrawcall->eventID, true);
    else if(!m_Drawcalls.empty())
      SetEventID(logviewers, m_Drawcalls.back().eventID, true);

    GUIInvoke::blockcall([&logviewers]() {
      // notify all the registers log viewers that a log has been loaded
      for(ILogViewer *logviewer : logviewers)
      {
        if(logviewer)
          logviewer->OnLogfileLoaded();
      }
    });
  }
}

float CaptureContext::UpdateLoadProgress()
{
  float val = 0.8f * m_LoadProgress + 0.19f * m_PostloadProgress + 0.01f;

  m_MainWindow->setProgress(val);

  return val;
}

void CaptureContext::LoadLogfileThreaded(const QString &logFile, const QString &origFilename,
                                         bool temporary, bool local)
{
  m_LogFile = origFilename;

  m_LogLocal = local;

  Config().Save();

  m_LoadProgress = 0.0f;
  m_PostloadProgress = 0.0f;

  // this function call will block until the log is either loaded, or there's some failure
  m_Renderer.OpenCapture(logFile, &m_LoadProgress);

  // if the renderer isn't running, we hit a failure case so display an error message
  if(!m_Renderer.IsRunning())
  {
    QString errmsg = ToQStr(m_Renderer.GetCreateStatus());

    QString messageText = tr("%1\nFailed to open logfile for replay: %2.\n\n"
                             "Check diagnostic log in Help menu for more details.")
                              .arg(logFile)
                              .arg(errmsg);

    RDDialog::critical(NULL, tr("Error opening log"), messageText);

    m_LoadInProgress = false;
    return;
  }

  if(!temporary)
  {
    AddRecentFile(Config().RecentLogFiles, origFilename, 10);

    Config().Save();
  }

  m_EventID = 0;

  m_FirstDrawcall = m_LastDrawcall = NULL;

  // fetch initial data like drawcalls, textures and buffers
  m_Renderer.BlockInvoke([this](IReplayController *r) {
    m_FrameInfo = r->GetFrameInfo();

    m_APIProps = r->GetAPIProperties();

    m_PostloadProgress = 0.2f;

    m_Drawcalls = r->GetDrawcalls();

    AddFakeProfileMarkers();

    m_FirstDrawcall = &m_Drawcalls[0];
    while(!m_FirstDrawcall->children.empty())
      m_FirstDrawcall = &m_FirstDrawcall->children[0];

    m_LastDrawcall = &m_Drawcalls.back();
    while(!m_LastDrawcall->children.empty())
      m_LastDrawcall = &m_LastDrawcall->children.back();

    m_PostloadProgress = 0.4f;

    m_WinSystems = r->GetSupportedWindowSystems();

#if defined(RENDERDOC_PLATFORM_WIN32)
    m_CurWinSystem = WindowingSystem::Win32;
#elif defined(RENDERDOC_PLATFORM_LINUX)
    m_CurWinSystem = WindowingSystem::Xlib;

    // prefer XCB, if supported
    for(WindowingSystem sys : m_WinSystems)
    {
      if(sys == WindowingSystem::XCB)
      {
        m_CurWinSystem = WindowingSystem::XCB;
        break;
      }
    }

    if(m_CurWinSystem == WindowingSystem::XCB)
      m_XCBConnection = QX11Info::connection();
    else
      m_X11Display = QX11Info::display();
#endif

    m_BufferList = r->GetBuffers();
    for(BufferDescription &b : m_BufferList)
      m_Buffers[b.ID] = &b;

    m_PostloadProgress = 0.8f;

    m_TextureList = r->GetTextures();
    for(TextureDescription &t : m_TextureList)
      m_Textures[t.ID] = &t;

    m_PostloadProgress = 0.9f;

    m_CurD3D11PipelineState = r->GetD3D11PipelineState();
    m_CurD3D12PipelineState = r->GetD3D12PipelineState();
    m_CurGLPipelineState = r->GetGLPipelineState();
    m_CurVulkanPipelineState = r->GetVulkanPipelineState();
    m_CurPipelineState.SetStates(m_APIProps, &m_CurD3D11PipelineState, &m_CurD3D12PipelineState,
                                 &m_CurGLPipelineState, &m_CurVulkanPipelineState);

    m_UnreadMessageCount = 0;
    AddMessages(m_FrameInfo.debugMessages);

    m_PostloadProgress = 1.0f;
  });

  QThread::msleep(20);

  QDateTime today = QDateTime::currentDateTimeUtc();
  QDateTime compare = today.addDays(-21);

  if(compare > Config().DegradedLog_LastUpdate && m_APIProps.degraded)
  {
    Config().DegradedLog_LastUpdate = today;

    RDDialog::critical(
        NULL, tr("Degraded support of log"),
        tr("%1\nThis log opened with degraded support - "
           "this could mean missing hardware support caused a fallback to software rendering.\n\n"
           "This warning will not appear every time this happens, "
           "check debug errors/warnings window for more details.")
            .arg(origFilename));
  }

  m_LoadInProgress = false;
  m_LogLoaded = true;
}

bool CaptureContext::PassEquivalent(const DrawcallDescription &a, const DrawcallDescription &b)
{
  // executing command lists can have children
  if(!a.children.empty() || !b.children.empty())
    return false;

  // don't group draws and compute executes
  if((a.flags & DrawFlags::Dispatch) != (b.flags & DrawFlags::Dispatch))
    return false;

  // don't group present with anything
  if((a.flags & DrawFlags::Present) != (b.flags & DrawFlags::Present))
    return false;

  // don't group things with different depth outputs
  if(a.depthOut != b.depthOut)
    return false;

  int numAOuts = 0, numBOuts = 0;
  for(int i = 0; i < 8; i++)
  {
    if(a.outputs[i] != ResourceId())
      numAOuts++;
    if(b.outputs[i] != ResourceId())
      numBOuts++;
  }

  int numSame = 0;

  if(a.depthOut != ResourceId())
  {
    numAOuts++;
    numBOuts++;
    numSame++;
  }

  for(int i = 0; i < 8; i++)
  {
    if(a.outputs[i] != ResourceId())
    {
      for(int j = 0; j < 8; j++)
      {
        if(a.outputs[i] == b.outputs[j])
        {
          numSame++;
          break;
        }
      }
    }
    else if(b.outputs[i] != ResourceId())
    {
      for(int j = 0; j < 8; j++)
      {
        if(a.outputs[j] == b.outputs[i])
        {
          numSame++;
          break;
        }
      }
    }
  }

  // use a kind of heuristic to group together passes where the outputs are similar enough.
  // could be useful for example if you're rendering to a gbuffer and sometimes you render
  // without one target, but the draws are still batched up.
  if(numSame > qMax(numAOuts, numBOuts) / 2 && qMax(numAOuts, numBOuts) > 1)
    return true;

  if(numSame == qMax(numAOuts, numBOuts))
    return true;

  return false;
}

bool CaptureContext::ContainsMarker(const rdctype::array<DrawcallDescription> &draws)
{
  bool ret = false;

  for(const DrawcallDescription &d : draws)
  {
    ret |=
        (d.flags & DrawFlags::PushMarker) && !(d.flags & DrawFlags::CmdList) && !d.children.empty();
    ret |= ContainsMarker(d.children);

    if(ret)
      break;
  }

  return ret;
}

void CaptureContext::AddFakeProfileMarkers()
{
  rdctype::array<DrawcallDescription> &draws = m_Drawcalls;

  if(!Config().EventBrowser_AddFake)
    return;

  if(ContainsMarker(draws))
    return;

  QList<DrawcallDescription> ret;

  int depthpassID = 1;
  int copypassID = 1;
  int computepassID = 1;
  int passID = 1;

  int start = 0;
  int refdraw = 0;

  DrawFlags drawFlags =
      DrawFlags::Copy | DrawFlags::Resolve | DrawFlags::SetMarker | DrawFlags::CmdList;

  for(int i = 1; i < draws.count; i++)
  {
    if(draws[refdraw].flags & drawFlags)
    {
      refdraw = i;
      continue;
    }

    if(draws[i].flags & drawFlags)
      continue;

    if(PassEquivalent(draws[i], draws[refdraw]))
      continue;

    int end = i - 1;

    if(end - start < 2 || !draws[i].children.empty() || !draws[refdraw].children.empty())
    {
      for(int j = start; j <= end; j++)
        ret.push_back(draws[j]);

      start = i;
      refdraw = i;
      continue;
    }

    int minOutCount = 100;
    int maxOutCount = 0;
    bool copyOnly = true;

    for(int j = start; j <= end; j++)
    {
      int outCount = 0;

      if(!(draws[j].flags & (DrawFlags::Copy | DrawFlags::Resolve | DrawFlags::Clear)))
        copyOnly = false;

      for(ResourceId o : draws[j].outputs)
        if(o != ResourceId())
          outCount++;
      minOutCount = qMin(minOutCount, outCount);
      maxOutCount = qMax(maxOutCount, outCount);
    }

    DrawcallDescription mark;

    mark.eventID = draws[start].eventID;
    mark.drawcallID = draws[start].drawcallID;

    mark.flags = DrawFlags::PushMarker;
    memcpy(mark.outputs, draws[end].outputs, sizeof(mark.outputs));
    mark.depthOut = draws[end].depthOut;

    mark.name = "Guessed Pass";

    minOutCount = qMax(1, minOutCount);

    QString targets = draws[end].depthOut == ResourceId() ? tr("Targets") : tr("Targets + Depth");

    if(copyOnly)
      mark.name = tr("Copy/Clear Pass #%1").arg(copypassID++).toUtf8().data();
    else if(draws[refdraw].flags & DrawFlags::Dispatch)
      mark.name = tr("Compute Pass #%1").arg(computepassID++).toUtf8().data();
    else if(maxOutCount == 0)
      mark.name = tr("Depth-only Pass #%1").arg(depthpassID++).toUtf8().data();
    else if(minOutCount == maxOutCount)
      mark.name =
          tr("Colour Pass #%1 (%2 %3)").arg(passID++).arg(minOutCount).arg(targets).toUtf8().data();
    else
      mark.name = tr("Colour Pass #%1 (%2-%3 %4)")
                      .arg(passID++)
                      .arg(minOutCount)
                      .arg(maxOutCount)
                      .arg(targets)
                      .toUtf8()
                      .data();

    mark.children.create(end - start + 1);

    for(int j = start; j <= end; j++)
    {
      mark.children[j - start] = draws[j];
      draws[j].parent = mark.eventID;
    }

    ret.push_back(mark);

    start = i;
    refdraw = i;
  }

  if(start < draws.count)
  {
    for(int j = start; j < draws.count; j++)
      ret.push_back(draws[j]);
  }

  m_Drawcalls.clear();
  m_Drawcalls.create(ret.count());
  for(int i = 0; i < ret.count(); i++)
    m_Drawcalls[i] = ret[i];
}

void CaptureContext::CloseLogfile()
{
  if(!m_LogLoaded)
    return;

  m_LogFile = QString();

  m_Renderer.CloseThread();

  memset(&m_APIProps, 0, sizeof(m_APIProps));
  memset(&m_FrameInfo, 0, sizeof(m_FrameInfo));
  m_Buffers.clear();
  m_BufferList.clear();
  m_Textures.clear();
  m_TextureList.clear();

  m_Drawcalls.clear();
  m_FirstDrawcall = m_LastDrawcall = NULL;

  m_CurD3D11PipelineState = D3D11Pipe::State();
  m_CurD3D12PipelineState = D3D12Pipe::State();
  m_CurGLPipelineState = GLPipe::State();
  m_CurVulkanPipelineState = VKPipe::State();
  m_CurPipelineState.SetStates(m_APIProps, NULL, NULL, NULL, NULL);

  m_DebugMessages.clear();
  m_UnreadMessageCount = 0;

  m_LogLoaded = false;

  QVector<ILogViewer *> logviewers(m_LogViewers);

  for(ILogViewer *logviewer : logviewers)
  {
    if(logviewer)
      logviewer->OnLogfileClosed();
  }
}

void CaptureContext::SetEventID(const QVector<ILogViewer *> &exclude, uint32_t selectedEventID,
                                uint32_t eventID, bool force)
{
  uint32_t prevSelectedEventID = m_SelectedEventID;
  m_SelectedEventID = selectedEventID;
  uint32_t prevEventID = m_EventID;
  m_EventID = eventID;

  m_Renderer.BlockInvoke([this, eventID, force](IReplayController *r) {
    r->SetFrameEvent(eventID, force);
    m_CurD3D11PipelineState = r->GetD3D11PipelineState();
    m_CurD3D12PipelineState = r->GetD3D12PipelineState();
    m_CurGLPipelineState = r->GetGLPipelineState();
    m_CurVulkanPipelineState = r->GetVulkanPipelineState();
    m_CurPipelineState.SetStates(m_APIProps, &m_CurD3D11PipelineState, &m_CurD3D12PipelineState,
                                 &m_CurGLPipelineState, &m_CurVulkanPipelineState);
  });

  for(ILogViewer *logviewer : m_LogViewers)
  {
    if(exclude.contains(logviewer))
      continue;

    if(force || prevSelectedEventID != selectedEventID)
      logviewer->OnSelectedEventChanged(selectedEventID);
    if(force || prevEventID != eventID)
      logviewer->OnEventChanged(eventID);
  }
}

void CaptureContext::AddMessages(const rdctype::array<DebugMessage> &msgs)
{
  m_UnreadMessageCount += msgs.count;
  for(const DebugMessage &msg : msgs)
    m_DebugMessages.push_back(msg);

  if(m_DebugMessageView)
    m_DebugMessageView->RefreshMessageList();
}

void *CaptureContext::FillWindowingData(uintptr_t widget)
{
#if defined(WIN32)

  return (void *)widget;

#elif defined(RENDERDOC_PLATFORM_LINUX)

  static XCBWindowData xcb;
  static XlibWindowData xlib;

  if(m_CurWinSystem == WindowingSystem::XCB)
  {
    xcb.connection = m_XCBConnection;
    xcb.window = (xcb_window_t)widget;
    return &xcb;
  }
  else
  {
    xlib.display = m_X11Display;
    xlib.window = (Drawable)widget;
    return &xlib;
  }

#elif defined(RENDERDOC_PLATFORM_APPLE)

  return (void *)widget;

#else

#error "Unknown platform"

#endif
}

IMainWindow *CaptureContext::GetMainWindow()
{
  return m_MainWindow;
}

IEventBrowser *CaptureContext::GetEventBrowser()
{
  if(m_EventBrowser)
    return m_EventBrowser;

  m_EventBrowser = new EventBrowser(*this, m_MainWindow);
  m_EventBrowser->setObjectName(lit("eventBrowser"));
  setupDockWindow(m_EventBrowser);

  return m_EventBrowser;
}

IAPIInspector *CaptureContext::GetAPIInspector()
{
  if(m_APIInspector)
    return m_APIInspector;

  m_APIInspector = new APIInspector(*this, m_MainWindow);
  m_APIInspector->setObjectName(lit("apiInspector"));
  setupDockWindow(m_APIInspector);

  return m_APIInspector;
}

ITextureViewer *CaptureContext::GetTextureViewer()
{
  if(m_TextureViewer)
    return m_TextureViewer;

  m_TextureViewer = new TextureViewer(*this, m_MainWindow);
  m_TextureViewer->setObjectName(lit("textureViewer"));
  setupDockWindow(m_TextureViewer);

  return m_TextureViewer;
}

IBufferViewer *CaptureContext::GetMeshPreview()
{
  if(m_MeshPreview)
    return m_MeshPreview;

  m_MeshPreview = new BufferViewer(*this, true, m_MainWindow);
  m_MeshPreview->setObjectName(lit("meshPreview"));
  setupDockWindow(m_MeshPreview);

  return m_MeshPreview;
}

IPipelineStateViewer *CaptureContext::GetPipelineViewer()
{
  if(m_PipelineViewer)
    return m_PipelineViewer;

  m_PipelineViewer = new PipelineStateViewer(*this, m_MainWindow);
  m_PipelineViewer->setObjectName(lit("pipelineViewer"));
  setupDockWindow(m_PipelineViewer);

  return m_PipelineViewer;
}

ICaptureDialog *CaptureContext::GetCaptureDialog()
{
  if(m_CaptureDialog)
    return m_CaptureDialog;

  m_CaptureDialog = new CaptureDialog(
      *this,
      [this](const QString &exe, const QString &workingDir, const QString &cmdLine,
             const QList<EnvironmentModification> &env, CaptureOptions opts,
             std::function<void(LiveCapture *)> callback) {
        return m_MainWindow->OnCaptureTrigger(exe, workingDir, cmdLine, env, opts, callback);
      },
      [this](uint32_t PID, const QList<EnvironmentModification> &env, const QString &name,
             CaptureOptions opts, std::function<void(LiveCapture *)> callback) {
        return m_MainWindow->OnInjectTrigger(PID, env, name, opts, callback);
      },
      m_MainWindow);
  m_CaptureDialog->setObjectName(lit("capDialog"));
  m_CaptureDialog->setWindowIcon(*m_Icon);

  return m_CaptureDialog;
}

IDebugMessageView *CaptureContext::GetDebugMessageView()
{
  if(m_DebugMessageView)
    return m_DebugMessageView;

  m_DebugMessageView = new DebugMessageView(*this, m_MainWindow);
  m_DebugMessageView->setObjectName(lit("debugMessageView"));
  setupDockWindow(m_DebugMessageView);

  return m_DebugMessageView;
}

IStatisticsViewer *CaptureContext::GetStatisticsViewer()
{
  if(m_StatisticsViewer)
    return m_StatisticsViewer;

  m_StatisticsViewer = new StatisticsViewer(*this, m_MainWindow);
  m_StatisticsViewer->setObjectName(lit("statisticsViewer"));
  setupDockWindow(m_StatisticsViewer);

  return m_StatisticsViewer;
}

ITimelineBar *CaptureContext::GetTimelineBar()
{
  if(m_TimelineBar)
    return m_TimelineBar;

  m_TimelineBar = new TimelineBar(*this, m_MainWindow);
  m_TimelineBar->setObjectName(lit("timelineBar"));
  setupDockWindow(m_TimelineBar);

  return m_TimelineBar;
}

IPythonShell *CaptureContext::GetPythonShell()
{
  if(m_PythonShell)
    return m_PythonShell;

  m_PythonShell = new PythonShell(*this, m_MainWindow);
  m_PythonShell->setObjectName(lit("pythonShell"));
  setupDockWindow(m_PythonShell);

  return m_PythonShell;
}

void CaptureContext::ShowEventBrowser()
{
  m_MainWindow->showEventBrowser();
}

void CaptureContext::ShowAPIInspector()
{
  m_MainWindow->showAPIInspector();
}

void CaptureContext::ShowTextureViewer()
{
  m_MainWindow->showTextureViewer();
}

void CaptureContext::ShowMeshPreview()
{
  m_MainWindow->showMeshPreview();
}

void CaptureContext::ShowPipelineViewer()
{
  m_MainWindow->showPipelineViewer();
}

void CaptureContext::ShowCaptureDialog()
{
  m_MainWindow->showCaptureDialog();
}

void CaptureContext::ShowDebugMessageView()
{
  m_MainWindow->showDebugMessageView();
}

void CaptureContext::ShowStatisticsViewer()
{
  m_MainWindow->showStatisticsViewer();
}

void CaptureContext::ShowTimelineBar()
{
  m_MainWindow->showTimelineBar();
}

void CaptureContext::ShowPythonShell()
{
  m_MainWindow->showPythonShell();
}

IShaderViewer *CaptureContext::EditShader(bool customShader, const QString &entryPoint,
                                          const QStringMap &files,
                                          IShaderViewer::SaveCallback saveCallback,
                                          IShaderViewer::CloseCallback closeCallback)
{
  return ShaderViewer::EditShader(*this, customShader, entryPoint, files, saveCallback,
                                  closeCallback, m_MainWindow->Widget());
}

IShaderViewer *CaptureContext::DebugShader(const ShaderBindpointMapping *bind,
                                           const ShaderReflection *shader, ShaderStage stage,
                                           ShaderDebugTrace *trace, const QString &debugContext)
{
  return ShaderViewer::DebugShader(*this, bind, shader, stage, trace, debugContext,
                                   m_MainWindow->Widget());
}

IShaderViewer *CaptureContext::ViewShader(const ShaderBindpointMapping *bind,
                                          const ShaderReflection *shader, ShaderStage stage)
{
  return ShaderViewer::ViewShader(*this, bind, shader, stage, m_MainWindow->Widget());
}

IBufferViewer *CaptureContext::ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                                          const QString &format)
{
  BufferViewer *viewer = new BufferViewer(*this, false, m_MainWindow);

  viewer->ViewBuffer(byteOffset, byteSize, id, format);

  return viewer;
}

IBufferViewer *CaptureContext::ViewTextureAsBuffer(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                                                   const QString &format)
{
  BufferViewer *viewer = new BufferViewer(*this, false, m_MainWindow);

  viewer->ViewTexture(arrayIdx, mip, id, format);

  return viewer;
}

IConstantBufferPreviewer *CaptureContext::ViewConstantBuffer(ShaderStage stage, uint32_t slot,
                                                             uint32_t idx)
{
  ConstantBufferPreviewer *existing = ConstantBufferPreviewer::has(stage, slot, idx);
  if(existing != NULL)
    return existing;

  return new ConstantBufferPreviewer(*this, stage, slot, idx, m_MainWindow);
}

IPixelHistoryView *CaptureContext::ViewPixelHistory(ResourceId texID, int x, int y,
                                                    const TextureDisplay &display)
{
  return new PixelHistoryView(*this, texID, QPoint(x, y), display, m_MainWindow);
}

QWidget *CaptureContext::CreateBuiltinWindow(const QString &objectName)
{
  if(objectName == lit("textureViewer"))
  {
    return GetTextureViewer()->Widget();
  }
  else if(objectName == lit("eventBrowser"))
  {
    return GetEventBrowser()->Widget();
  }
  else if(objectName == lit("pipelineViewer"))
  {
    return GetPipelineViewer()->Widget();
  }
  else if(objectName == lit("meshPreview"))
  {
    return GetMeshPreview()->Widget();
  }
  else if(objectName == lit("apiInspector"))
  {
    return GetAPIInspector()->Widget();
  }
  else if(objectName == lit("capDialog"))
  {
    return GetCaptureDialog()->Widget();
  }
  else if(objectName == lit("debugMessageView"))
  {
    return GetDebugMessageView()->Widget();
  }
  else if(objectName == lit("statisticsViewer"))
  {
    return GetStatisticsViewer()->Widget();
  }
  else if(objectName == lit("timelineBar"))
  {
    return GetTimelineBar()->Widget();
  }
  else if(objectName == lit("pythonShell"))
  {
    return GetPythonShell()->Widget();
  }

  return NULL;
}

void CaptureContext::BuiltinWindowClosed(QWidget *window)
{
  if(m_EventBrowser && m_EventBrowser->Widget() == window)
    m_EventBrowser = NULL;
  else if(m_TextureViewer && m_TextureViewer->Widget() == window)
    m_TextureViewer = NULL;
  else if(m_CaptureDialog && m_CaptureDialog->Widget() == window)
    m_CaptureDialog = NULL;
  else if(m_APIInspector && m_APIInspector->Widget() == window)
    m_APIInspector = NULL;
  else if(m_PipelineViewer && m_PipelineViewer->Widget() == window)
    m_PipelineViewer = NULL;
  else if(m_MeshPreview && m_MeshPreview->Widget() == window)
    m_MeshPreview = NULL;
  else if(m_DebugMessageView && m_DebugMessageView->Widget() == window)
    m_DebugMessageView = NULL;
  else if(m_StatisticsViewer && m_StatisticsViewer->Widget() == window)
    m_StatisticsViewer = NULL;
  else if(m_TimelineBar && m_TimelineBar->Widget() == window)
    m_TimelineBar = NULL;
  else if(m_PythonShell && m_PythonShell->Widget() == window)
    m_PythonShell = NULL;
  else
    qCritical() << "Unrecognised window being closed: " << window;
}

void CaptureContext::setupDockWindow(QWidget *shad)
{
  shad->setWindowIcon(*m_Icon);
}

void CaptureContext::RaiseDockWindow(QWidget *dockWindow)
{
  ToolWindowManager::raiseToolWindow(dockWindow);
}

void CaptureContext::AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                                   float percentage)
{
  if(!newWindow)
  {
    qCritical() << "Unexpected NULL newWindow in AddDockWindow";
    return;
  }
  setupDockWindow(newWindow);

  if(ref == DockReference::MainToolArea)
  {
    m_MainWindow->mainToolManager()->addToolWindow(newWindow, m_MainWindow->mainToolArea());
    return;
  }
  if(ref == DockReference::LeftToolArea)
  {
    m_MainWindow->mainToolManager()->addToolWindow(newWindow, m_MainWindow->leftToolArea());
    return;
  }

  if(!refWindow)
  {
    qCritical() << "Unexpected NULL refWindow in AddDockWindow";
    return;
  }

  if(ref == DockReference::ConstantBufferArea)
  {
    if(ConstantBufferPreviewer::getOne())
    {
      ToolWindowManager *manager = ToolWindowManager::managerOf(refWindow);

      manager->addToolWindow(newWindow, ToolWindowManager::AreaReference(
                                            ToolWindowManager::AddTo,
                                            manager->areaOf(ConstantBufferPreviewer::getOne())));
      return;
    }

    ref = DockReference::RightOf;
  }

  ToolWindowManager *manager = ToolWindowManager::managerOf(refWindow);

  ToolWindowManager::AreaReference areaRef((ToolWindowManager::AreaReferenceType)ref,
                                           manager->areaOf(refWindow), percentage);
  manager->addToolWindow(newWindow, areaRef);
}
