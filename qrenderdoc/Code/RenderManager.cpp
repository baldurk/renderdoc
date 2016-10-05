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

#include "RenderManager.h"
#include <QMutexLocker>
#include "CaptureContext.h"

RenderManager::RenderManager()
{
  m_Running = false;
  m_Thread = NULL;
}

RenderManager::~RenderManager()
{
}

void RenderManager::Init(int proxyRenderer, QString replayHost, QString logfile, float *progress)
{
  if(m_Running)
    return;

  m_ProxyRenderer = proxyRenderer;
  m_ReplayHost = replayHost;
  m_Logfile = logfile;
  m_Progress = progress;

  *progress = 0.0f;

  m_Thread = new LambdaThread([this]() { run(); });
  m_Thread->start(QThread::HighestPriority);

  while(m_Thread->isRunning() && !m_Running)
  {
  }
}

bool RenderManager::IsRunning()
{
  return m_Thread->isRunning() && m_Running;
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

  while(!cmd->processed)
  {
  }
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

  // the thread deletes itself, don't delete here

  m_Thread = NULL;
}

void RenderManager::PushInvoke(RenderManager::InvokeHandle *cmd)
{
  if(m_Thread == NULL || !m_Thread->isRunning() || !m_Running)
  {
    cmd->processed = true;
    if(cmd->selfdelete)
      delete cmd;
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

  RENDERDOC_LogText(QString("QRenderDoc - renderer created for %1").arg(m_Logfile).toUtf8());

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

      cmd->processed = true;

      // if it's a throwaway command, delete it
      if(cmd->selfdelete)
        delete cmd;
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

      cmd->processed = true;

      if(cmd->selfdelete)
        delete cmd;
    }
  }

  // close the core renderer
  renderer->Shutdown();
}
