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
}

ReplayManager::~ReplayManager()
{
}

void ReplayManager::OpenCapture(const QString &logfile, float *progress)
{
  if(m_Running)
    return;

  m_ProxyRenderer = -1;
  m_ReplayHost = QString();
  m_Logfile = logfile;
  m_Progress = progress;

  *progress = 0.0f;

  m_Thread = new LambdaThread([this]() { run(); });
  m_Thread->start(QThread::HighestPriority);

  while(m_Thread->isRunning() && !m_Running)
  {
  }
}

void ReplayManager::DeleteCapture(const QString &logfile, bool local)
{
  if(IsRunning())
  {
    AsyncInvoke([this, logfile, local](IReplayController *) { DeleteCapture(logfile, local); });
    return;
  }

  if(local)
  {
    QFile::remove(logfile);
  }
  else
  {
    // this will be cleaned up automatically when the remote connection
    // is closed.
    if(m_Remote)
    {
      QMutexLocker autolock(&m_RemoteLock);
      m_Remote->TakeOwnershipCapture(logfile.toUtf8().data());
    }
  }
}

QStringList ReplayManager::GetRemoteSupport()
{
  QStringList ret;

  if(m_Remote && !IsRunning())
  {
    QMutexLocker autolock(&m_RemoteLock);

    rdctype::array<rdctype::str> supported = m_Remote->RemoteSupportedReplays();
    for(rdctype::str &s : supported)
      ret << ToQStr(s);
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
      cb(m_Remote->GetHomeFolder().c_str(), rdctype::array<PathEntry>());
    };

    if(synchronous)
      BlockInvoke(lambda);
    else
      AsyncInvoke(lambda);
    return;
  }

  rdctype::str home;

  {
    QMutexLocker autolock(&m_RemoteLock);
    home = m_Remote->GetHomeFolder();
  }

  cb(home.c_str(), rdctype::array<PathEntry>());
}

void ReplayManager::ListFolder(QString path, bool synchronous, DirectoryBrowseCallback cb)
{
  if(!m_Remote)
    return;

  QByteArray pathUTF8 = path.toUtf8();

  if(IsRunning() && m_Thread->isCurrentThread())
  {
    auto lambda = [cb, pathUTF8, this](IReplayController *r) {
      cb(pathUTF8.data(), m_Remote->ListFolder(pathUTF8.data()));
    };

    if(synchronous)
      BlockInvoke(lambda);
    else
      AsyncInvoke(lambda);
    return;
  }

  rdctype::array<PathEntry> contents;

  // prevent pings while fetching remote FS data
  {
    QMutexLocker autolock(&m_RemoteLock);
    contents = m_Remote->ListFolder(pathUTF8.data());
  }

  cb(pathUTF8.data(), contents);

  return;
}

QString ReplayManager::CopyCaptureToRemote(const QString &localpath, QWidget *window)
{
  if(!m_Remote)
    return QString();

  QString remotepath;

  bool copied = false;
  float progress = 0.0f;

  auto lambda = [this, localpath, &remotepath, &progress, &copied](IReplayController *r) {
    QMutexLocker autolock(&m_RemoteLock);
    remotepath = ToQStr(m_Remote->CopyCaptureToRemote(localpath.toUtf8().data(), &progress));
    copied = true;
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
    thread->start();
  }

  ShowProgressDialog(window, tr("Transferring..."), [&copied]() { return copied; },
                     [&progress]() { return progress; });

  return remotepath;
}

void ReplayManager::CopyCaptureFromRemote(const QString &remotepath, const QString &localpath,
                                          QWidget *window)
{
  if(!m_Remote)
    return;

  bool copied = false;
  float progress = 0.0f;

  auto lambda = [this, localpath, remotepath, &progress, &copied](IReplayController *r) {
    QMutexLocker autolock(&m_RemoteLock);
    m_Remote->CopyCaptureFromRemote(remotepath.toUtf8().data(), localpath.toUtf8().data(), &progress);
    copied = true;
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
    thread->start();
  }

  ShowProgressDialog(window, tr("Transferring..."), [&copied]() { return copied; },
                     [&progress]() { return progress; });
}

bool ReplayManager::IsRunning()
{
  return m_Thread && m_Thread->isRunning() && m_Running;
}

void ReplayManager::AsyncInvoke(const QString &tag, ReplayManager::InvokeCallback m)
{
  {
    QMutexLocker autolock(&m_RenderLock);
    for(int i = 0; i < m_RenderQueue.count();)
    {
      if(m_RenderQueue[i]->tag == tag)
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

  InvokeHandle *cmd = new InvokeHandle(m, tag);
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

void ReplayManager::CloseThread()
{
  m_Running = false;

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

ReplayStatus ReplayManager::ConnectToRemoteServer(RemoteHost *host)
{
  ReplayStatus status =
      RENDERDOC_CreateRemoteServerConnection(host->Hostname.toUtf8().data(), 0, &m_Remote);

  m_RemoteHost = host;

  if(status == ReplayStatus::Succeeded)
  {
    m_RemoteHost->Connected = true;
    return status;
  }

  return status;
}

void ReplayManager::DisconnectFromRemoteServer()
{
  if(m_RemoteHost)
    m_RemoteHost->Connected = false;

  if(m_Remote)
  {
    QMutexLocker autolock(&m_RemoteLock);
    m_Remote->ShutdownConnection();
  }

  m_RemoteHost = NULL;
  m_Remote = NULL;
}

void ReplayManager::ShutdownServer()
{
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
      if(!m_Remote->Ping())
        m_RemoteHost->ServerRunning = false;
    }
    m_RemoteLock.unlock();
  }
}

uint32_t ReplayManager::ExecuteAndInject(const QString &exe, const QString &workingDir,
                                         const QString &cmdLine,
                                         const QList<EnvironmentModification> &env,
                                         const QString &logfile, CaptureOptions opts)
{
  rdctype::array<EnvironmentModification> envList = env.toVector().toStdVector();

  uint32_t ret = 0;

  if(m_Remote)
  {
    QMutexLocker autolock(&m_RemoteLock);
    ret = m_Remote->ExecuteAndInject(exe.toUtf8().data(), workingDir.toUtf8().data(),
                                     cmdLine.toUtf8().data(), envList, opts);
  }
  else
  {
    ret = RENDERDOC_ExecuteAndInject(exe.toUtf8().data(), workingDir.toUtf8().data(),
                                     cmdLine.toUtf8().data(), envList, logfile.toUtf8().data(),
                                     opts, false);
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

void ReplayManager::run()
{
  IReplayController *renderer = NULL;

  if(m_Remote)
  {
    std::tie(m_CreateStatus, renderer) =
        m_Remote->OpenCapture(~0U, m_Logfile.toUtf8().data(), m_Progress);
  }
  else
  {
    ICaptureFile *file = RENDERDOC_OpenCaptureFile(m_Logfile.toUtf8().data());

    m_CreateStatus = file->OpenStatus();

    if(m_CreateStatus == ReplayStatus::Succeeded)
      std::tie(m_CreateStatus, renderer) = file->OpenCapture(m_Progress);

    file->Shutdown();
  }

  if(renderer == NULL)
    return;

  qInfo() << "QRenderDoc - renderer created for" << m_Logfile;

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
      cmd->method(renderer);

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

  // close the core renderer
  if(m_Remote)
    m_Remote->CloseCapture(renderer);
  else
    renderer->Shutdown();
}
