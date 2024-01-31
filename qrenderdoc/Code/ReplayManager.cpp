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

#include "ReplayManager.h"
#include <QApplication>
#include <QMutexLocker>
#include <QProgressDialog>
#include "CaptureContext.h"
#include "QRDUtils.h"

ReplayManager::ReplayManager()
{
  m_Running = false;
  m_Thread = NULL;

  RENDERDOC_RegisterMemoryRegion(this, sizeof(ReplayManager));
}

ReplayManager::~ReplayManager()
{
  RENDERDOC_UnregisterMemoryRegion(this);
}

void ReplayManager::OpenCapture(const QString &capturefile, const ReplayOptions &opts,
                                RENDERDOC_ProgressCallback progress)
{
  if(m_Running)
    return;

  m_FatalError = {};
  m_FatalError.code = ResultCode::Succeeded;

  // TODO maybe we could expose this choice to the user?
  int proxyRenderer = -1;

  m_Thread = new LambdaThread([this, proxyRenderer, capturefile, opts, progress]() {
    run(proxyRenderer, capturefile, opts, progress);
  });
  m_Thread->setName(lit("ReplayManager"));
  m_Thread->start(QThread::HighestPriority);

  while(m_Thread->isRunning() && !m_Running)
  {
    QThread::msleep(50);
  }
}

void ReplayManager::DeleteCapture(const rdcstr &capture, bool local)
{
  if(IsRunning() && !m_Thread->isCurrentThread())
  {
    AsyncInvoke([this, capture, local](IReplayController *) { DeleteCapture(capture, local); });
    return;
  }

  if(local)
  {
    QFile::remove(capture);
  }
  else
  {
    // this will be cleaned up automatically when the remote connection
    // is closed.
    if(m_Remote)
    {
      QMutexLocker autolock(&m_RemoteLock);
      m_Remote->TakeOwnershipCapture(capture);
    }
  }
}

rdcarray<rdcstr> ReplayManager::GetRemoteSupport()
{
  rdcarray<rdcstr> ret;

  if(m_Remote && !IsRunning())
  {
    QMutexLocker autolock(&m_RemoteLock);

    ret = m_Remote->RemoteSupportedReplays();
  }

  return ret;
}

void ReplayManager::GetHomeFolder(bool synchronous, DirectoryBrowseCallback cb)
{
  if(!m_Remote)
    return;

  if(IsRunning() && m_Thread->isCurrentThread())
  {
    auto lambda = [cb, this](IReplayController *r) {
      cb(m_Remote->GetHomeFolder().c_str(), rdcarray<PathEntry>());
    };

    if(synchronous)
      BlockInvoke(lambda);
    else
      AsyncInvoke(lambda);
    return;
  }

  rdcstr home;

  {
    QMutexLocker autolock(&m_RemoteLock);
    home = m_Remote->GetHomeFolder();
  }

  cb(home.c_str(), rdcarray<PathEntry>());
}

void ReplayManager::ListFolder(const rdcstr &path, bool synchronous, DirectoryBrowseCallback cb)
{
  if(!m_Remote)
    return;

  if(IsRunning() && m_Thread->isCurrentThread())
  {
    auto lambda = [cb, path, this](IReplayController *r) { cb(path, m_Remote->ListFolder(path)); };

    if(synchronous)
      BlockInvoke(lambda);
    else
      AsyncInvoke(lambda);
    return;
  }

  rdcarray<PathEntry> contents;

  // prevent pings while fetching remote FS data
  {
    QMutexLocker autolock(&m_RemoteLock);
    contents = m_Remote->ListFolder(path);
  }

  cb(path, contents);

  return;
}

rdcstr ReplayManager::CopyCaptureToRemote(const rdcstr &localpath, QWidget *window)
{
  if(!m_Remote)
    return "";

  rdcstr remotepath;

  QAtomicInt copied = 0;
  float progress = 0.0f;

  auto lambda = [this, localpath, &remotepath, &progress, &copied](IReplayController *r) {
    QMutexLocker autolock(&m_RemoteLock);
    remotepath = m_Remote->CopyCaptureToRemote(localpath, [&progress](float p) { progress = p; });
    copied = 1;
  };

  // we should never have the thread running at this point, but let's be safe.
  if(IsRunning())
  {
    AsyncInvoke(lambda);
  }
  else
  {
    LambdaThread *thread = new LambdaThread([&lambda]() { lambda(NULL); });
    thread->selfDelete(true);
    thread->setName(lit("CopyCaptureToRemote"));
    thread->start();
  }

  ShowProgressDialog(
      window, tr("Transferring..."), [&copied]() { return copied == 1; },
      [&progress]() { return progress; });

  return remotepath;
}

void ReplayManager::CopyCaptureFromRemote(const rdcstr &remotepath, const rdcstr &localpath,
                                          QWidget *window)
{
  if(!m_Remote)
    return;

  QAtomicInt copied = 0;
  float progress = 0.0f;

  auto lambda = [this, localpath, remotepath, &progress, &copied](IReplayController *r) {
    QMutexLocker autolock(&m_RemoteLock);
    m_Remote->CopyCaptureFromRemote(remotepath, localpath, [&progress](float p) { progress = p; });
    copied = 1;
  };

  // we should never have the thread running at this point, but let's be safe.
  if(IsRunning())
  {
    AsyncInvoke(lambda);
  }
  else
  {
    LambdaThread *thread = new LambdaThread([&lambda]() { lambda(NULL); });
    thread->selfDelete(true);
    thread->setName(lit("CopyCaptureFromRemote"));
    thread->start();
  }

  ShowProgressDialog(
      window, tr("Transferring..."), [&copied]() { return copied == 1; },
      [&progress]() { return progress; });
}

bool ReplayManager::IsRunning()
{
  return m_Thread && m_Thread->isRunning() && m_Running;
}

float ReplayManager::GetCurrentProcessingTime()
{
  QMutexLocker lock(&m_TimerLock);
  return m_CommandTimer.isValid() ? double(m_CommandTimer.elapsed()) / 1000.0 : 0.0;
}

QString ReplayManager::GetCurrentProcessingTag()
{
  QMutexLocker lock(&m_TimerLock);
  return m_CommandTag;
}

void ReplayManager::AsyncInvoke(const rdcstr &tag, ReplayManager::InvokeCallback m)
{
  QString qtag(tag);

  {
    QMutexLocker autolock(&m_RenderLock);
    for(int i = 0; i < m_RenderQueue.count();)
    {
      if(m_RenderQueue[i]->tag == qtag)
      {
        InvokeHandle *cmd = m_RenderQueue.takeAt(i);
        if(cmd->selfdelete)
          delete cmd;
      }
      else
      {
        i++;
      }
    }
  }

  InvokeHandle *cmd = new InvokeHandle(m, qtag);
  cmd->selfdelete = true;

  PushInvoke(cmd);
}

void ReplayManager::AsyncInvoke(ReplayManager::InvokeCallback m)
{
  InvokeHandle *cmd = new InvokeHandle(m);
  cmd->selfdelete = true;

  PushInvoke(cmd);
}

void ReplayManager::BlockInvoke(ReplayManager::InvokeCallback m)
{
  InvokeHandle *cmd = new InvokeHandle(m);

  PushInvoke(cmd);

  cmd->processed.acquire();

  delete cmd;
}

void ReplayManager::CancelReplayLoop()
{
  m_Renderer->CancelReplayLoop();
}

void ReplayManager::CloseThread()
{
  m_Running = false;
  m_FatalError = {};
  m_FatalError.code = ResultCode::Succeeded;

  m_RenderCondition.wakeAll();

  if(m_Thread == NULL)
    return;

  // wait for the thread to close and clean up
  while(m_Thread->isRunning())
  {
  }

  m_Thread->deleteLater();
  m_Thread = NULL;
}

ResultDetails ReplayManager::ConnectToRemoteServer(RemoteHost host)
{
  ResultDetails result = host.Connect(&m_Remote);

  if(host.Protocol() && host.Protocol()->GetProtocolName() == "adb")
  {
    ANALYTIC_SET(UIFeatures.AndroidRemoteReplay, true);
  }
  else
  {
    ANALYTIC_SET(UIFeatures.NonAndroidRemoteReplay, true);
  }

  m_RemoteHost = host;

  if(result.OK())
    m_RemoteHost.SetConnected(true);

  return result;
}

void ReplayManager::DisconnectFromRemoteServer()
{
  m_RemoteHost.SetConnected(false);

  if(m_Remote)
  {
    QMutexLocker autolock(&m_RemoteLock);
    // give the remote to the thread to shut down since the lifetime is tied to the replay
    // controller it has.
    if(IsRunning())
    {
      m_OrphanedRemote = m_Remote;
    }
    else
    {
      m_Remote->ShutdownConnection();
    }
    m_Remote = NULL;
  }

  m_RemoteHost = RemoteHost();
}

void ReplayManager::ShutdownServer()
{
  m_RemoteHost.SetShutdown();

  if(m_Remote)
  {
    QMutexLocker autolock(&m_RemoteLock);
    m_Remote->ShutdownServerAndConnection();
  }

  m_Remote = NULL;
}

void ReplayManager::PingRemote()
{
  if(!m_Remote)
    return;

  if(m_RemoteLock.tryLock(0))
  {
    if(!IsRunning() || m_Thread->isCurrentThread())
    {
      if(!m_Remote->Ping().OK())
        m_RemoteHost.SetShutdown();
    }
    m_RemoteLock.unlock();
  }
}

void ReplayManager::ReopenCaptureFile(const QString &path)
{
  if(!m_CaptureFile)
    m_CaptureFile = RENDERDOC_OpenCaptureFile();
  m_CaptureFile->OpenFile(path, "rdc", NULL);
}

ExecuteResult ReplayManager::ExecuteAndInject(const rdcstr &exe, const rdcstr &workingDir,
                                              const rdcstr &cmdLine,
                                              const rdcarray<EnvironmentModification> &env,
                                              const rdcstr &capturefile, CaptureOptions opts)
{
  ExecuteResult ret;

  if(m_Remote)
  {
    QMutexLocker autolock(&m_RemoteLock);
    ret = m_Remote->ExecuteAndInject(exe, workingDir, cmdLine, env, opts);
  }
  else
  {
    ret = RENDERDOC_ExecuteAndInject(exe, workingDir, cmdLine, env, capturefile, opts, false);
  }

  return ret;
}

void ReplayManager::PushInvoke(ReplayManager::InvokeHandle *cmd)
{
  if(m_Thread == NULL || !m_Thread->isRunning() || !m_Running)
  {
    if(cmd->selfdelete)
      delete cmd;
    else
      cmd->processed.release();
    return;
  }

  QMutexLocker autolock(&m_RenderLock);
  m_RenderQueue.enqueue(cmd);
  m_RenderCondition.wakeAll();
}

void ReplayManager::run(int proxyRenderer, const QString &capturefile, const ReplayOptions &opts,
                        RENDERDOC_ProgressCallback progress)
{
  m_Renderer = NULL;

  if(m_Remote)
  {
    rdctie(m_CreateResult, m_Renderer) =
        m_Remote->OpenCapture(proxyRenderer, capturefile, opts, progress);
  }
  else
  {
    m_CaptureFile = RENDERDOC_OpenCaptureFile();

    m_CreateResult = m_CaptureFile->OpenFile(capturefile, "rdc", NULL);

    if(m_CreateResult.OK())
      rdctie(m_CreateResult, m_Renderer) = m_CaptureFile->OpenCapture(opts, progress);
  }

  if(m_Renderer == NULL)
  {
    if(m_CaptureFile)
      m_CaptureFile->Shutdown();
    m_CaptureFile = NULL;

    return;
  }

  qInfo() << "QRenderDoc - renderer created for" << capturefile;

  m_Running = true;

  // main render command loop
  while(m_Running)
  {
    InvokeHandle *cmd = NULL;

    // wait for the condition to be woken, grab top of current queue,
    // unlock again.
    {
      QMutexLocker autolock(&m_RenderLock);
      if(m_RenderQueue.isEmpty())
        m_RenderCondition.wait(&m_RenderLock, 10);

      if(!m_RenderQueue.isEmpty())
        cmd = m_RenderQueue.dequeue();
    }

    if(cmd == NULL)
      continue;

    if(cmd->method != NULL)
    {
      {
        QMutexLocker lock(&m_TimerLock);
        m_CommandTimer.start();
        m_CommandTag = cmd->tag;
      }

      cmd->method(m_Renderer);

      ResultDetails err = m_Renderer->GetFatalErrorStatus();
      if(m_FatalError.OK() && !err.OK())
      {
        m_FatalError = err;
        m_FatalErrorCallback();
      }

      {
        QMutexLocker lock(&m_TimerLock);
        m_CommandTimer.invalidate();
        m_CommandTag = QString();
      }
    }

    // if it's a throwaway command, delete it
    if(cmd->selfdelete)
      delete cmd;
    else
      cmd->processed.release();
  }

  // clean up anything left in the queue
  {
    QQueue<InvokeHandle *> queue;

    {
      QMutexLocker autolock(&m_RenderLock);
      m_RenderQueue.swap(queue);
    }

    for(InvokeHandle *cmd : queue)
    {
      if(cmd == NULL)
        continue;

      if(cmd->selfdelete)
        delete cmd;
      else
        cmd->processed.release();
    }
  }

  // if the remote has been orphaned due to disconnection during replay, close the capture using it
  // and then shut it down.
  if(m_OrphanedRemote)
  {
    m_OrphanedRemote->CloseCapture(m_Renderer);
    m_OrphanedRemote->ShutdownConnection();
    m_OrphanedRemote = NULL;
  }
  else
  {
    // close the core renderer
    if(m_Remote)
      m_Remote->CloseCapture(m_Renderer);
    else
      m_Renderer->Shutdown();
  }

  m_Renderer = NULL;

  if(m_CaptureFile)
    m_CaptureFile->Shutdown();
  m_CaptureFile = NULL;
}
