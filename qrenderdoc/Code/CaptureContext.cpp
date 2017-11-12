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
#include "Windows/CommentView.h"
#include "Windows/ConstantBufferPreviewer.h"
#include "Windows/DebugMessageView.h"
#include "Windows/Dialogs/CaptureDialog.h"
#include "Windows/Dialogs/LiveCapture.h"
#include "Windows/EventBrowser.h"
#include "Windows/MainWindow.h"
#include "Windows/PerformanceCounterViewer.h"
#include "Windows/PipelineState/PipelineStateViewer.h"
#include "Windows/PixelHistoryView.h"
#include "Windows/PythonShell.h"
#include "Windows/ResourceInspector.h"
#include "Windows/ShaderViewer.h"
#include "Windows/StatisticsViewer.h"
#include "Windows/TextureViewer.h"
#include "Windows/TimelineBar.h"
#include "QRDUtils.h"

CaptureContext::CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent,
                               bool temp, PersistantConfig &cfg)
    : m_Config(cfg), m_CurPipelineState(*this)
{
  m_CaptureLoaded = false;
  m_LoadInProgress = false;

  m_EventID = 0;

  memset(&m_APIProps, 0, sizeof(m_APIProps));

  m_CurD3D11PipelineState = &m_DummyD3D11;
  m_CurD3D12PipelineState = &m_DummyD3D12;
  m_CurGLPipelineState = &m_DummyGL;
  m_CurVulkanPipelineState = &m_DummyVK;

  m_StructuredFile = &m_DummySDFile;

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
    m_MainWindow->LoadFromFilename(paramFilename, temp);
    if(temp)
      m_MainWindow->takeCaptureOwnership();
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

QString CaptureContext::TempCaptureFilename(QString appname)
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

void CaptureContext::LoadCapture(const QString &captureFile, const QString &origFilename,
                                 bool temporary, bool local)
{
  m_LoadInProgress = true;

  bool newCapture = (!temporary && !Config().RecentCaptureFiles.contains(origFilename));

  LambdaThread *thread = new LambdaThread([this, captureFile, origFilename, temporary, local]() {
    LoadCaptureThreaded(captureFile, origFilename, temporary, local);
  });
  thread->selfDelete(true);
  thread->start();

  ShowProgressDialog(m_MainWindow, tr("Loading Capture: %1").arg(origFilename),
                     [this]() { return !m_LoadInProgress; },
                     [this]() { return UpdateLoadProgress(); });

  m_MainWindow->setProgress(-1.0f);

  if(m_CaptureLoaded)
  {
    m_CaptureTemporary = temporary;

    m_CaptureMods = CaptureModifications::NoModifications;

    QVector<ICaptureViewer *> viewers(m_CaptureViewers);

    // make sure we're on a consistent event before invoking viewer forms
    if(m_LastDrawcall)
      SetEventID(viewers, m_LastDrawcall->eventID, true);
    else if(!m_Drawcalls.empty())
      SetEventID(viewers, m_Drawcalls.back().eventID, true);

    GUIInvoke::blockcall([&viewers]() {
      // notify all the registers viewers that a capture has been loaded
      for(ICaptureViewer *viewer : viewers)
      {
        if(viewer)
          viewer->OnCaptureLoaded();
      }
    });

    if(newCapture && m_Notes.contains(lit("comments")))
    {
      if(!HasCommentView())
        ShowCommentView();
      RaiseDockWindow(GetCommentView()->Widget());
    }
  }
}

float CaptureContext::UpdateLoadProgress()
{
  float val = 0.8f * m_LoadProgress + 0.19f * m_PostloadProgress + 0.01f;

  m_MainWindow->setProgress(val);

  return val;
}

void CaptureContext::LoadCaptureThreaded(const QString &captureFile, const QString &origFilename,
                                         bool temporary, bool local)
{
  m_CaptureFile = origFilename;

  m_CaptureLocal = local;

  Config().Save();

  m_LoadProgress = 0.0f;
  m_PostloadProgress = 0.0f;

  // this function call will block until the capture is either loaded, or there's some failure
  m_Renderer.OpenCapture(captureFile, &m_LoadProgress);

  // if the renderer isn't running, we hit a failure case so display an error message
  if(!m_Renderer.IsRunning())
  {
    QString errmsg = ToQStr(m_Renderer.GetCreateStatus());

    QString messageText = tr("%1\nFailed to open capture for replay: %2.\n\n"
                             "Check diagnostic log in Help menu for more details.")
                              .arg(captureFile)
                              .arg(errmsg);

    RDDialog::critical(NULL, tr("Error opening capture"), messageText);

    m_LoadInProgress = false;
    return;
  }

  if(!temporary)
  {
    AddRecentFile(Config().RecentCaptureFiles, origFilename, 10);

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

    m_StructuredFile = &r->GetStructuredFile();

    m_ResourceList = r->GetResources();
    for(ResourceDescription &res : m_ResourceList)
      m_Resources[res.ID] = &res;

    m_BufferList = r->GetBuffers();
    for(BufferDescription &b : m_BufferList)
      m_Buffers[b.ID] = &b;

    m_PostloadProgress = 0.8f;

    m_TextureList = r->GetTextures();
    for(TextureDescription &t : m_TextureList)
      m_Textures[t.ID] = &t;

    m_PostloadProgress = 0.9f;

    m_CurD3D11PipelineState = &r->GetD3D11PipelineState();
    m_CurD3D12PipelineState = &r->GetD3D12PipelineState();
    m_CurGLPipelineState = &r->GetGLPipelineState();
    m_CurVulkanPipelineState = &r->GetVulkanPipelineState();
    m_CurPipelineState.SetStates(m_APIProps, m_CurD3D11PipelineState, m_CurD3D12PipelineState,
                                 m_CurGLPipelineState, m_CurVulkanPipelineState);

    m_UnreadMessageCount = 0;
    AddMessages(m_FrameInfo.debugMessages);

    m_PostloadProgress = 1.0f;
  });

  QThread::msleep(20);

  QDateTime today = QDateTime::currentDateTimeUtc();
  QDateTime compare = today.addDays(-21);

  if(compare > Config().DegradedCapture_LastUpdate && m_APIProps.degraded)
  {
    Config().DegradedCapture_LastUpdate = today;

    RDDialog::critical(
        NULL, tr("Degraded support of capture"),
        tr("%1\nThis capture opened with degraded support - "
           "this could mean missing hardware support caused a fallback to software rendering.\n\n"
           "This warning will not appear every time this happens, "
           "check debug errors/warnings window for more details.")
            .arg(origFilename));
  }

  ICaptureAccess *access = Replay().GetCaptureAccess();

  if(access)
  {
    int idx = access->FindSectionByType(SectionType::ResourceRenames);
    if(idx >= 0)
    {
      bytebuf buf = access->GetSectionContents(idx);
      LoadRenames(QString::fromUtf8((const char *)buf.data(), buf.count()));
    }

    idx = access->FindSectionByType(SectionType::Bookmarks);
    if(idx >= 0)
    {
      bytebuf buf = access->GetSectionContents(idx);
      LoadBookmarks(QString::fromUtf8((const char *)buf.data(), buf.count()));
    }

    idx = access->FindSectionByType(SectionType::Notes);
    if(idx >= 0)
    {
      bytebuf buf = access->GetSectionContents(idx);
      LoadNotes(QString::fromUtf8((const char *)buf.data(), buf.count()));
    }
  }

  m_LoadInProgress = false;
  m_CaptureLoaded = true;
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

bool CaptureContext::ContainsMarker(const rdcarray<DrawcallDescription> &draws)
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
  rdcarray<DrawcallDescription> &draws = m_Drawcalls;

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

  for(int32_t i = 1; i < draws.count(); i++)
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

    mark.children.resize(end - start + 1);

    for(int j = start; j <= end; j++)
    {
      mark.children[j - start] = draws[j];
      draws[j].parent = mark.eventID;
    }

    ret.push_back(mark);

    start = i;
    refdraw = i;
  }

  if(start < draws.count())
  {
    for(int j = start; j < draws.count(); j++)
      ret.push_back(draws[j]);
  }

  m_Drawcalls = ret;
}

void CaptureContext::RecompressCapture()
{
  QString destFilename = GetCaptureFilename();
  QString tempFilename;

  ICaptureFile *cap = NULL;
  ICaptureFile *tempCap = NULL;

  bool inplace = false;

  if(IsCaptureTemporary() || !IsCaptureLocal())
  {
    QMessageBox::StandardButton res =
        RDDialog::question(m_MainWindow, tr("Unsaved capture"),
                           tr("To recompress a capture you must save it first. Save this capture?"),
                           QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if(res == QMessageBox::Cancel || res == QMessageBox::No)
      return;

    destFilename = m_MainWindow->GetSavePath();

    // if it's already local, we'll do the save as part of the recompression convert. If it's
    // remote, we need to copy it first, but we copy it to a temporary so we can do the conversion
    // to the target location

    if(IsCaptureLocal())
    {
      tempFilename = GetCaptureFilename();
    }
    else
    {
      tempFilename = TempCaptureFilename(lit("recompress"));
      Replay().CopyCaptureFromRemote(GetCaptureFilename(), tempFilename, m_MainWindow);

      if(!QFile::exists(tempFilename))
      {
        RDDialog::critical(m_MainWindow, tr("Failed to save capture"),
                           tr("Capture couldn't be saved from remote."));
        return;
      }
    }
  }
  else
  {
    // if we're doing this inplace on an already saved capture, then we need to recompress to a
    // temporary and close/move it afterwards.
    inplace = true;
    destFilename = TempCaptureFilename(lit("recompress"));
  }

  if(IsCaptureLocal())
  {
    // for local files we already have a handle. We'll reuse it, then re-open
    cap = Replay().GetCaptureFile();
  }
  else
  {
    // for remote files we open a new short-lived handle on the temporary file
    tempCap = cap = RENDERDOC_OpenCaptureFile();
    cap->OpenFile(tempFilename.toUtf8().data(), "rdc");
  }

  if(!cap)
  {
    RDDialog::critical(m_MainWindow, tr("Unexpected missing handle"),
                       tr("Couldn't get open handle to file for recompression."));
    return;
  }

  int index = cap->FindSectionByType(SectionType::FrameCapture);
  SectionProperties props = cap->GetSectionProperties(index);

  if(props.flags & SectionFlags::ZstdCompressed)
  {
    RDDialog::information(m_MainWindow, tr("Capture already compressed"),
                          tr("This capture is already compressed as much as is possible."));

    if(tempCap)
      tempCap->Shutdown();
    if(!tempFilename.isEmpty())
      QFile::remove(tempFilename);
    return;
  }

  // convert from the currently open cap to the destination
  float progress = 0.0f;

  LambdaThread *th = new LambdaThread([this, cap, destFilename, &progress]() {
    cap->Convert(destFilename.toUtf8().data(), "rdc", &progress);
  });
  th->start();
  // wait a few ms before popping up a progress bar
  th->wait(500);
  if(th->isRunning())
  {
    ShowProgressDialog(m_MainWindow, tr("Recompressing file."), [th]() { return !th->isRunning(); },
                       [&progress]() { return progress; });
  }
  th->deleteLater();

  if(inplace)
  {
    // if we're recompressing "in place", we need to close our capture, move the temporary over
    // the original, then re-open.

    // this releases the hold over the real desired location.
    cap->OpenFile("", "");

    // now remove the old capture
    QFile::remove(GetCaptureFilename());

    // move the recompressed one over
    QFile::rename(destFilename, GetCaptureFilename());

    // and re-open
    cap->OpenFile(GetCaptureFilename().toUtf8().data(), "rdc");
  }
  else
  {
    // we've converted into the desired location. We don't have to do anything else but mark our
    // new locally saved non-temporary status.

    m_CaptureFile = destFilename;
    m_CaptureLocal = true;
    m_CaptureTemporary = false;

    // open the saved capture file. This will let us remove the old file too
    Replay().ReopenCaptureFile(m_CaptureFile);

    m_CaptureMods = CaptureModifications::All;

    SaveChanges();
  }

  // close any temporary resources
  if(tempCap)
    tempCap->Shutdown();
  if(!tempFilename.isEmpty())
    QFile::remove(tempFilename);
}

bool CaptureContext::SaveCaptureTo(const QString &captureFile)
{
  bool success = false;
  QString error;

  if(IsCaptureLocal())
  {
    if(QFileInfo(GetCaptureFilename()).exists())
    {
      if(GetCaptureFilename() == captureFile)
      {
        success = true;
      }
      else
      {
        ICaptureFile *capFile = Replay().GetCaptureFile();

        if(capFile)
        {
          // this will overwrite
          success = capFile->CopyFileTo(captureFile.toUtf8().data());
        }
        else
        {
          // QFile::copy won't overwrite, so remove the destination first (the save dialog already
          // prompted for overwrite)
          QFile::remove(captureFile);
          success = QFile::copy(GetCaptureFilename(), captureFile);
        }

        error = tr("Couldn't save to %1").arg(captureFile);
      }
    }
    else
    {
      RDDialog::critical(
          NULL, tr("File not found"),
          error =
              tr("Capture '%1' couldn't be found on disk, cannot save.").arg(GetCaptureFilename()));
      success = false;
    }
  }
  else
  {
    Replay().CopyCaptureFromRemote(GetCaptureFilename(), captureFile, m_MainWindow);
    success = QFile::exists(captureFile);

    error = tr("File couldn't be transferred from remote host");
  }

  if(!success)
  {
    RDDialog::critical(NULL, tr("Error Saving"), error);
    return false;
  }

  // if it was a temporary capture, remove the old instnace
  if(m_CaptureTemporary)
    QFile::remove(m_CaptureFile);

  // Update the filename, and mark that it's local and not temporary now.
  m_CaptureFile = captureFile;
  m_CaptureLocal = true;
  m_CaptureTemporary = false;

  Replay().ReopenCaptureFile(captureFile);
  SaveChanges();

  return true;
}

void CaptureContext::CloseCapture()
{
  if(!m_CaptureLoaded)
    return;

  m_CaptureTemporary = false;

  m_CaptureFile = QString();

  m_Renderer.CloseThread();

  memset(&m_APIProps, 0, sizeof(m_APIProps));
  memset(&m_FrameInfo, 0, sizeof(m_FrameInfo));
  m_Buffers.clear();
  m_BufferList.clear();
  m_Textures.clear();
  m_TextureList.clear();
  m_Resources.clear();
  m_ResourceList.clear();

  m_CustomNames.clear();
  m_Bookmarks.clear();
  m_Notes.clear();

  m_Drawcalls.clear();
  m_FirstDrawcall = m_LastDrawcall = NULL;

  m_CurD3D11PipelineState = &m_DummyD3D11;
  m_CurD3D12PipelineState = &m_DummyD3D12;
  m_CurGLPipelineState = &m_DummyGL;
  m_CurVulkanPipelineState = &m_DummyVK;
  m_CurPipelineState.SetStates(m_APIProps, NULL, NULL, NULL, NULL);

  m_StructuredFile = &m_DummySDFile;

  m_DebugMessages.clear();
  m_UnreadMessageCount = 0;

  m_CaptureLoaded = false;

  QVector<ICaptureViewer *> capviewers(m_CaptureViewers);

  for(ICaptureViewer *viewer : capviewers)
  {
    if(viewer)
      viewer->OnCaptureClosed();
  }
}

void CaptureContext::SetEventID(const QVector<ICaptureViewer *> &exclude, uint32_t selectedEventID,
                                uint32_t eventID, bool force)
{
  uint32_t prevSelectedEventID = m_SelectedEventID;
  m_SelectedEventID = selectedEventID;
  uint32_t prevEventID = m_EventID;
  m_EventID = eventID;

  m_Renderer.BlockInvoke([this, eventID, force](IReplayController *r) {
    r->SetFrameEvent(eventID, force);
    m_CurD3D11PipelineState = &r->GetD3D11PipelineState();
    m_CurD3D12PipelineState = &r->GetD3D12PipelineState();
    m_CurGLPipelineState = &r->GetGLPipelineState();
    m_CurVulkanPipelineState = &r->GetVulkanPipelineState();
    m_CurPipelineState.SetStates(m_APIProps, m_CurD3D11PipelineState, m_CurD3D12PipelineState,
                                 m_CurGLPipelineState, m_CurVulkanPipelineState);
  });

  bool updateSelectedEvent = force || prevSelectedEventID != selectedEventID;
  bool updateEvent = force || prevEventID != eventID;

  RefreshUIStatus(exclude, updateSelectedEvent, updateEvent);
}

void CaptureContext::RefreshUIStatus(const QVector<ICaptureViewer *> &exclude,
                                     bool updateSelectedEvent, bool updateEvent)
{
  for(ICaptureViewer *viewer : m_CaptureViewers)
  {
    if(exclude.contains(viewer))
      continue;

    if(updateSelectedEvent)
      viewer->OnSelectedEventChanged(m_SelectedEventID);
    if(updateEvent)
      viewer->OnEventChanged(m_EventID);
  }
}

void CaptureContext::AddMessages(const rdcarray<DebugMessage> &msgs)
{
  m_UnreadMessageCount += msgs.count();
  for(const DebugMessage &msg : msgs)
    m_DebugMessages.push_back(msg);

  if(m_DebugMessageView)
  {
    GUIInvoke::call([this]() { m_DebugMessageView->RefreshMessageList(); });
  }
}

void CaptureContext::SetNotes(const QString &key, const QString &contents)
{
  // ignore no-op changes
  if(m_Notes.contains(key) && m_Notes[key] == contents)
    return;

  m_Notes[key] = contents;

  m_CaptureMods |= CaptureModifications::Notes;
  m_MainWindow->captureModified();

  RefreshUIStatus({}, true, true);
}

void CaptureContext::SetBookmark(const EventBookmark &mark)
{
  int index = m_Bookmarks.indexOf(mark);
  if(index >= 0)
  {
    // ignore no-op bookmarks
    if(m_Bookmarks[index].text == mark.text)
      return;

    m_Bookmarks[index] = mark;
  }
  else
  {
    m_Bookmarks.push_back(mark);
  }

  m_CaptureMods |= CaptureModifications::Bookmarks;
  m_MainWindow->captureModified();

  RefreshUIStatus({}, true, true);
}

void CaptureContext::RemoveBookmark(uint32_t EID)
{
  m_Bookmarks.removeOne(EventBookmark(EID));

  m_CaptureMods |= CaptureModifications::Bookmarks;
  m_MainWindow->captureModified();

  RefreshUIStatus({}, true, true);
}

void CaptureContext::SaveChanges()
{
  if(m_CaptureMods & CaptureModifications::Renames)
    SaveRenames();

  if(m_CaptureMods & CaptureModifications::Bookmarks)
    SaveBookmarks();

  if(m_CaptureMods & CaptureModifications::Notes)
    SaveNotes();

  m_CaptureMods = CaptureModifications::NoModifications;
}

void CaptureContext::SaveRenames()
{
  QVariantMap resources;
  for(ResourceId id : m_CustomNames.keys())
  {
    resources[ToQStr(id)] = m_CustomNames[id];
  }

  QVariantMap root;
  root[lit("CustomResourceNames")] = resources;

  QString json = VariantToJSON(root);

  SectionProperties props;
  props.type = SectionType::ResourceRenames;
  props.version = 1;

  Replay().GetCaptureAccess()->WriteSection(props, json.toUtf8());
}

void CaptureContext::LoadRenames(const QString &data)
{
  QVariantMap root = JSONToVariant(data);

  if(root.contains(lit("CustomResourceNames")))
  {
    QVariantMap resources = root[lit("CustomResourceNames")].toMap();

    for(const QString &str : resources.keys())
    {
      ResourceId id;

      if(str.startsWith(lit("resourceid::")))
      {
        qulonglong num = str.mid(sizeof("resourceid::") - 1).toULongLong();
        memcpy(&id, &num, sizeof(num));
      }
      else
      {
        qCritical() << "Unrecognised resourceid encoding" << str;
      }

      if(id != ResourceId())
        m_CustomNames[id] = resources[str].toString();
    }
  }
}

void CaptureContext::SaveBookmarks()
{
  QVariantList bookmarks;
  for(const EventBookmark &mark : m_Bookmarks)
  {
    QVariantMap variantmark;
    variantmark[lit("EID")] = mark.EID;
    variantmark[lit("text")] = mark.text;

    bookmarks.push_back(variantmark);
  }

  QVariantMap root;
  root[lit("Bookmarks")] = bookmarks;

  QString json = VariantToJSON(root);

  SectionProperties props;
  props.type = SectionType::Bookmarks;
  props.version = 1;

  Replay().GetCaptureAccess()->WriteSection(props, json.toUtf8());
}

void CaptureContext::LoadBookmarks(const QString &data)
{
  QVariantMap root = JSONToVariant(data);

  if(root.contains(lit("Bookmarks")))
  {
    QVariantList bookmarks = root[lit("Bookmarks")].toList();

    for(QVariant v : bookmarks)
    {
      QVariantMap variantmark = v.toMap();

      EventBookmark mark;
      mark.EID = variantmark[lit("EID")].toUInt();
      mark.text = variantmark[lit("text")].toString();

      if(mark.EID != 0)
        m_Bookmarks.push_back(mark);
    }
  }
}

void CaptureContext::SaveNotes()
{
  QVariantMap root;
  for(const QString &key : m_Notes.keys())
    root[key] = m_Notes[key];

  QString json = VariantToJSON(root);

  SectionProperties props;
  props.type = SectionType::Notes;
  props.version = 1;

  Replay().GetCaptureAccess()->WriteSection(props, json.toUtf8());
}

void CaptureContext::LoadNotes(const QString &data)
{
  QVariantMap root = JSONToVariant(data);

  for(QString key : root.keys())
  {
    if(!key.isEmpty())
      m_Notes[key] = root[key].toString();
  }
}

QString CaptureContext::GetResourceName(ResourceId id)
{
  if(id == ResourceId())
    return tr("No Resource");

  if(m_CustomNames.contains(id))
    return m_CustomNames[id];

  ResourceDescription *desc = GetResource(id);

  if(desc)
    return desc->name;

  return tr("Unknown %1").arg(ToQStr(id));
}

bool CaptureContext::IsAutogeneratedName(ResourceId id)
{
  if(id == ResourceId())
    return true;

  if(m_CustomNames.contains(id))
    return false;

  ResourceDescription *desc = GetResource(id);

  if(desc)
    return desc->autogeneratedName;

  return true;
}

bool CaptureContext::HasResourceCustomName(ResourceId id)
{
  return m_CustomNames.contains(id);
}

void CaptureContext::SetResourceCustomName(ResourceId id, const QString &name)
{
  if(name.isEmpty())
  {
    if(m_CustomNames.contains(id))
      m_CustomNames.remove(id);
  }
  else
  {
    m_CustomNames[id] = name;
  }

  m_CustomNameCachedID++;

  m_CaptureMods |= CaptureModifications::Renames;
  m_MainWindow->captureModified();

  RefreshUIStatus({}, true, true);
}

int CaptureContext::ResourceNameCacheID()
{
  return m_CustomNameCachedID;
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
      m_MainWindow, m_MainWindow);
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

ICommentView *CaptureContext::GetCommentView()
{
  if(m_CommentView)
    return m_CommentView;

  m_CommentView = new CommentView(*this, m_MainWindow);
  m_CommentView->setObjectName(lit("commentView"));
  setupDockWindow(m_CommentView);

  return m_CommentView;
}

IPerformanceCounterViewer *CaptureContext::GetPerformanceCounterViewer()
{
  if(m_PerformanceCounterViewer)
    return m_PerformanceCounterViewer;

  m_PerformanceCounterViewer = new PerformanceCounterViewer(*this, m_MainWindow);
  m_PerformanceCounterViewer->setObjectName(lit("performanceCounterViewer"));
  setupDockWindow(m_PerformanceCounterViewer);

  return m_PerformanceCounterViewer;
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

IResourceInspector *CaptureContext::GetResourceInspector()
{
  if(m_ResourceInspector)
    return m_ResourceInspector;

  m_ResourceInspector = new ResourceInspector(*this, m_MainWindow);
  m_ResourceInspector->setObjectName(lit("resourceInspector"));
  setupDockWindow(m_ResourceInspector);

  return m_ResourceInspector;
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

void CaptureContext::ShowCommentView()
{
  m_MainWindow->showCommentView();
}

void CaptureContext::ShowPerformanceCounterViewer()
{
  m_MainWindow->showPerformanceCounterViewer();
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

void CaptureContext::ShowResourceInspector()
{
  m_MainWindow->showResourceInspector();
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
                                           const ShaderReflection *shader, ResourceId pipeline,
                                           ShaderStage stage, ShaderDebugTrace *trace,
                                           const QString &debugContext)
{
  return ShaderViewer::DebugShader(*this, bind, shader, pipeline, stage, trace, debugContext,
                                   m_MainWindow->Widget());
}

IShaderViewer *CaptureContext::ViewShader(const ShaderBindpointMapping *bind,
                                          const ShaderReflection *shader, ResourceId pipeline,
                                          ShaderStage stage)
{
  return ShaderViewer::ViewShader(*this, bind, shader, pipeline, stage, m_MainWindow->Widget());
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
  else if(objectName == lit("commentView"))
  {
    return GetCommentView()->Widget();
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
  else if(objectName == lit("resourceInspector"))
  {
    return GetResourceInspector()->Widget();
  }
  else if(objectName == lit("performanceCounterViewer"))
  {
    return GetPerformanceCounterViewer()->Widget();
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
  else if(m_CommentView && m_CommentView->Widget() == window)
    m_CommentView = NULL;
  else if(m_StatisticsViewer && m_StatisticsViewer->Widget() == window)
    m_StatisticsViewer = NULL;
  else if(m_TimelineBar && m_TimelineBar->Widget() == window)
    m_TimelineBar = NULL;
  else if(m_PythonShell && m_PythonShell->Widget() == window)
    m_PythonShell = NULL;
  else if(m_ResourceInspector && m_ResourceInspector->Widget() == window)
    m_ResourceInspector = NULL;
  else if(m_PerformanceCounterViewer && m_PerformanceCounterViewer->Widget() == window)
    m_PerformanceCounterViewer = NULL;
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
