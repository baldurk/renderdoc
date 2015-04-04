#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#include "renderdoc_replay.h"

#include <QString>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>

struct IReplayRenderer;

class RenderManager : public QThread
{
    Q_OBJECT
    void run();

  public:
    typedef void (*InvokeMethod)(IReplayRenderer *r);

    RenderManager();
    ~RenderManager();

    void Init(int proxyRenderer, QString replayHost, QString logfile, float *progress);

    bool IsRunning() { return isRunning() && m_Running; }
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

    QMutex m_RenderLock;
    QQueue<InvokeHandle *> m_RenderQueue;
    QWaitCondition m_RenderCondition;

    void PushInvoke(InvokeHandle *cmd);

    int m_ProxyRenderer;
    QString m_ReplayHost;
    QString m_Logfile;
    float *m_Progress;

    volatile bool m_Running;
    ReplayCreateStatus m_CreateStatus;
};

#endif // RENDERMANAGER_H
