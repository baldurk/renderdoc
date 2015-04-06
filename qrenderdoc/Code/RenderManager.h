#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#include "renderdoc_replay.h"

#include <functional>

#include <QString>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

struct IReplayRenderer;
class LambdaThread;

class RenderManager
{
  public:
    typedef std::function<void(IReplayRenderer*)> InvokeMethod;

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

#endif // RENDERMANAGER_H
