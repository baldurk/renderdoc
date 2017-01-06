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

#include "RenderManager.h"
#include <QMutexLocker>
#include "CaptureContext.h"
#include "QRDUtils.h"

RenderManager::RenderManager()
{
  m_Running = false;
  m_Thread = NULL;
}

RenderManager::~RenderManager()
{
}

void RenderManager::OpenCapture(const QString &logfile, float *progress)
{
  if(m_Running)
    return;

  m_ProxyRenderer = -1;
  m_ReplayHost = "";
  m_Logfile = logfile;
  m_Progress = progress;

  *progress = 0.0f;

  m_Thread = new LambdaThread([this]() { run(); });
  m_Thread->start(QThread::HighestPriority);

  while(m_Thread->isRunning() && !m_Running)
  {
  }
}

void RenderManager::DeleteCapture(const QString &logfile, bool local)
{
  if(IsRunning())
  {
    AsyncInvoke([this, logfile, local](IReplayRenderer *) { DeleteCapture(logfile, local); });
    return;
  }

  if(local)
  {
    QFile::remove(logfile);
  }
  else
  {
    // TODO Remote
    // m_Remote.TakeOwnershipCapture(logfile);
  }
}

bool RenderManager::IsRunning()
{
  return m_Thread && m_Thread->isRunning() && m_Running;
}

void RenderManager::AsyncInvoke(RenderManager::InvokeMethod m)
{
  InvokeHandle *cmd = new InvokeHandle(m);
  cmd->selfdelete = true;

  PushInvoke(cmd);
}

void RenderManager::BlockInvoke(RenderManager::InvokeMethod m)
{
  InvokeHandle *cmd = new InvokeHandle(m);

  PushInvoke(cmd);

  for(;;)
  {
    if(cmd->processed.tryAcquire())
      break;
  }

  delete cmd;
}

void RenderManager::CloseThread()
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

uint32_t RenderManager::ExecuteAndInject(const QString &exe, const QString &workingDir,
                                         const QString &cmdLine,
                                         const QList<EnvironmentModification> &env,
                                         const QString &logfile, CaptureOptions opts)
{
  // if (m_Remote == null)
  {
    void *envList = RENDERDOC_MakeEnvironmentModificationList(env.size());

    for(int i = 0; i < env.size(); i++)
      RENDERDOC_SetEnvironmentModification(envList, i, env[i].variable.toUtf8().data(),
                                           env[i].value.toUtf8().data(), env[i].type,
                                           env[i].separator);

    uint32_t ret = RENDERDOC_ExecuteAndInject(exe.toUtf8().data(), workingDir.toUtf8().data(),
                                              cmdLine.toUtf8().data(), envList,
                                              logfile.toUtf8().data(), &opts, false);

    RENDERDOC_FreeEnvironmentModificationList(envList);

    return ret;
  }
  /*
  else
  {
  }
  */
}

void RenderManager::PushInvoke(RenderManager::InvokeHandle *cmd)
{
  if(m_Thread == NULL || !m_Thread->isRunning() || !m_Running)
  {
    if(cmd->selfdelete)
      delete cmd;
    else
      cmd->processed.release();
    return;
  }

  m_RenderLock.lock();
  m_RenderQueue.push_back(cmd);
  m_RenderCondition.wakeAll();
  m_RenderLock.unlock();
}

void RenderManager::run()
{
  IReplayRenderer *renderer = NULL;

  m_CreateStatus = RENDERDOC_CreateReplayRenderer(m_Logfile.toUtf8(), m_Progress, &renderer);

  if(renderer == NULL)
    return;

  qInfo() << QString("QRenderDoc - renderer created for %1").arg(m_Logfile);

  m_Running = true;

  // main render command loop
  while(m_Running)
  {
    QQueue<InvokeHandle *> queue;

    // wait for the condition to be woken, grab current queue,
    // unlock again.
    {
      m_RenderLock.lock();
      m_RenderCondition.wait(&m_RenderLock, 10);
      m_RenderQueue.swap(queue);
      m_RenderLock.unlock();
    }

    // process all the commands
    for(InvokeHandle *cmd : queue)
    {
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
  }

  // clean up anything left in the queue
  {
    QQueue<InvokeHandle *> queue;

    m_RenderLock.lock();
    m_RenderQueue.swap(queue);
    m_RenderLock.unlock();

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
  renderer->Shutdown();
}
