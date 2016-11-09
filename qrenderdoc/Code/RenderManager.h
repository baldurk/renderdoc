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
#include <QSemaphore>
#include <QString>
#include <QThread>
#include <QVariantMap>
#include <QWaitCondition>
#include <functional>
#include "renderdoc_replay.h"

struct IReplayRenderer;
class LambdaThread;

// simple helper for the common case of 'we just need to run this on the render thread
#define INVOKE_MEMFN(function) \
  m_Ctx->Renderer()->AsyncInvoke([this](IReplayRenderer *r) { function(r); });

struct EnvironmentModification
{
  QString variable;
  QString value;

  EnvironmentModificationType type;
  EnvironmentSeparator separator;

  QString GetTypeString() const
  {
    QString ret;

    if(type == eEnvMod_Append)
      ret = QString("Append, %1").arg("TODO ToStr");
    else if(type == eEnvMod_Prepend)
      ret = QString("Prepend, %1").arg("TODO ToStr");
    else
      ret = "Set";

    return ret;
  }

  QString GetDescription() const
  {
    QString ret;

    if(type == eEnvMod_Append)
      ret = QString("Append %1 with %2 using %3").arg(variable).arg(value).arg("TODO ToStr");
    else if(type == eEnvMod_Prepend)
      ret = QString("Prepend %1 with %2 using %3").arg(variable).arg(value).arg("TODO ToStr");
    else
      ret = QString("Set %1 to %2").arg(variable).arg(value);

    return ret;
  }

  QVariantMap toJSON() const
  {
    QVariantMap ret;
    ret["variable"] = variable;
    ret["value"] = value;
    ret["type"] = "Append";             // TODO ToStr
    ret["separator"] = "Semi-colon";    // TODO ToStr
    return ret;
  }

  void fromJSON(const QVariantMap &data)
  {
    variable = data["variable"].toString();
    value = data["value"].toString();
    type = eEnvMod_Append;            // TODO ToStr
    separator = eEnvSep_SemiColon;    // TODO ToStr
  }
};

class RenderManager
{
public:
  typedef std::function<void(IReplayRenderer *)> InvokeMethod;

  RenderManager();
  ~RenderManager();

  void OpenCapture(const QString &logfile, float *progress);
  void DeleteCapture(const QString &logfile, bool local);

  bool IsRunning();
  ReplayCreateStatus GetCreateStatus() { return m_CreateStatus; }
  void AsyncInvoke(InvokeMethod m);
  void BlockInvoke(InvokeMethod m);

  void CloseThread();

  uint32_t ExecuteAndInject(const QString &exe, const QString &workingDir, const QString &cmdLine,
                            const QList<EnvironmentModification> &env, const QString &logfile,
                            CaptureOptions opts);

private:
  struct InvokeHandle
  {
    InvokeHandle(InvokeMethod m)
    {
      method = m;
      selfdelete = false;
    }

    InvokeMethod method;
    QSemaphore processed;
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
