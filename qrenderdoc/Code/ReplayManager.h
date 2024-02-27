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

#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QQueue>
#include <QSemaphore>
#include <QString>
#include <QThread>
#include <QVariantMap>
#include <QWaitCondition>
#include <functional>
#include "Interface/QRDInterface.h"
#include "QRDUtils.h"

struct IReplayController;
class LambdaThread;
class RemoteHost;

class ReplayManager : public IReplayManager
{
  Q_DECLARE_TR_FUNCTIONS(ReplayManager);

public:
  ReplayManager();
  ~ReplayManager();

  void OpenCapture(const QString &capturefile, const ReplayOptions &opts,
                   RENDERDOC_ProgressCallback progress);
  void DeleteCapture(const rdcstr &capturefile, bool local);

  bool IsRunning();
  ResultDetails GetCreateStatus() { return m_CreateResult; }
  void SetFatalErrorCallback(std::function<void()> cb) { m_FatalErrorCallback = cb; }
  ResultDetails GetFatalError() { return m_FatalError; }
  float GetCurrentProcessingTime();
  QString GetCurrentProcessingTag();
  // this tagged version is for cases when we might send a request - e.g. to pick a vertex or pixel
  // - and want to pre-empt it with a new request before the first has returned. Either because some
  // other work is taking a while or because we're sending requests faster than they can be
  // processed.
  // the manager processes only the request on the top of the queue, so when a new tagged invoke
  // comes in, we remove any other requests in the queue before it that have the same tag
  void AsyncInvoke(const rdcstr &tag, InvokeCallback m);
  void AsyncInvoke(InvokeCallback m);
  void BlockInvoke(InvokeCallback m);

  void CancelReplayLoop();

  void CloseThread();

  ResultDetails ConnectToRemoteServer(RemoteHost host);
  void DisconnectFromRemoteServer();
  void ShutdownServer();
  void PingRemote();

  ICaptureAccess *GetCaptureAccess()
  {
    if(m_Remote)
      return m_Remote;
    if(m_CaptureFile)
      return m_CaptureFile;
    return NULL;
  }

  // may return NULL if the capture file is not open locally. Consider using ICaptureAccess above to
  // work whether local or remote.
  ICaptureFile *GetCaptureFile() { return m_CaptureFile; }
  void ReopenCaptureFile(const QString &path);
  RemoteHost CurrentRemote() { return m_RemoteHost; }
  ExecuteResult ExecuteAndInject(const rdcstr &exe, const rdcstr &workingDir, const rdcstr &cmdLine,
                                 const rdcarray<EnvironmentModification> &env,
                                 const rdcstr &capturefile, CaptureOptions opts);

  rdcarray<rdcstr> GetRemoteSupport();
  void GetHomeFolder(bool synchronous, DirectoryBrowseCallback cb);
  void ListFolder(const rdcstr &path, bool synchronous, DirectoryBrowseCallback cb);
  rdcstr CopyCaptureToRemote(const rdcstr &localpath, QWidget *window);
  void CopyCaptureFromRemote(const rdcstr &remotepath, const rdcstr &localpath, QWidget *window);

private:
  struct InvokeHandle
  {
    InvokeHandle(InvokeCallback m, const QString &t = QString())
    {
      tag = t;
      method = m;
      selfdelete = false;
    }

    QString tag;
    InvokeCallback method;
    QSemaphore processed;
    bool selfdelete;
  };

  void run(int proxyRenderer, const QString &capturefile, const ReplayOptions &opts,
           RENDERDOC_ProgressCallback progress);

  QMutex m_TimerLock;
  QElapsedTimer m_CommandTimer;
  QString m_CommandTag;

  QMutex m_RenderLock;
  QQueue<InvokeHandle *> m_RenderQueue;
  QWaitCondition m_RenderCondition;

  ICaptureFile *m_CaptureFile = NULL;
  IReplayController *m_Renderer = NULL;

  void PushInvoke(InvokeHandle *cmd);

  QMutex m_RemoteLock;
  RemoteHost m_RemoteHost;
  IRemoteServer *m_Remote = NULL;
  IRemoteServer *m_OrphanedRemote = NULL;

  std::function<void()> m_FatalErrorCallback;
  ResultDetails m_FatalError = {ResultCode::Succeeded};
  volatile bool m_Running;
  LambdaThread *m_Thread;
  ResultDetails m_CreateResult = {ResultCode::Succeeded};
};
