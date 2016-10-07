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

#include <QMutex>
#include <QQueue>
#include <QString>
#include <QThread>
#include <QWaitCondition>
#include <functional>
#include "renderdoc_replay.h"

struct IReplayRenderer;
class LambdaThread;

// simple helper for the common case of 'we just need to run this on the render thread
#define INVOKE_MEMFN(function) \
  m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) { function(r); });

class RenderManager
{
public:
  typedef std::function<void(IReplayRenderer *)> InvokeMethod;

  RenderManager();
  ~RenderManager();

  void Init(int proxyRenderer, QString replayHost, QString logfile, float *progress);

  bool IsRunning();
  ReplayCreateStatus GetCreateStatus() { return m_CreateStatus; }
  void AsyncInvoke(InvokeMethod m);
  void BlockInvoke(InvokeMethod m);

  void CloseThread();

private:
  struct InvokeHandle
  {
    InvokeHandle(InvokeMethod m)
    {
      method = m;
      processed = false;
      selfdelete = true;
    }

    InvokeMethod method;
    bool processed;
    bool selfdelete;
  };

  void run();

  QMutex m_RenderLock;
  QQueue<InvokeHandle *> m_RenderQueue;
  QWaitCondition m_RenderCondition;

  void PushInvoke(InvokeHandle *cmd);

  int m_ProxyRenderer;
  QString m_ReplayHost;
  QString m_Logfile;
  float *m_Progress;

  volatile bool m_Running;
  LambdaThread *m_Thread;
  ReplayCreateStatus m_CreateStatus;
};
