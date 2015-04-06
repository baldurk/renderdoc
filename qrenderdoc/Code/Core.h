#ifndef CORE_H
#define CORE_H

#include "RenderManager.h"

#include <QString>
#include <QList>
#include <QMap>

struct ILogViewerForm
{
    virtual void OnLogfileLoaded() = 0;
    virtual void OnLogfileClosed() = 0;
    virtual void OnEventSelected(uint32_t frameID, uint32_t eventID) = 0;
};

struct ILogLoadProgressListener
{
    virtual void LogfileProgressBegin() = 0;
    virtual void LogfileProgress(float progress) = 0;
};

class MainWindow;

class Core
{
  public:
    Core(QString paramFilename, QString remoteHost, uint32_t remoteIdent, bool temp);
    ~Core();

    //////////////////////////////////////////////////////////////////////////////
    // Control functions

    // loading a local log, no remote replay
    void LoadLogfile(QString logFile, bool temporary);

    // when loading a log while replaying remotely, provide the proxy renderer that will be used
    // as well as the hostname to replay on.
    void LoadLogfile(int proxyRenderer, QString replayHost, QString logFile, bool temporary);

    void CloseLogfile();

    QString TempLogFilename(QString appname);

    void SetEventID(ILogViewerForm *exclude, uint32_t frameID, uint32_t eventID);

    void AddLogProgressListener(ILogLoadProgressListener *p);

    void AddLogViewer(ILogViewerForm *f)
    {
      m_LogViewers.push_back(f);

      if(LogLoaded())
      {
        f->OnLogfileLoaded();
        f->OnEventSelected(CurFrame(), CurEvent());
      }
    }

    void RemoveLogViewer(ILogViewerForm *f)
    {
      m_LogViewers.removeAll(f);
    }

    //////////////////////////////////////////////////////////////////////////////
    // Singleton windows

    /*
    private MainWindow m_MainWindow = null;
    private EventBrowser m_EventBrowser = null;
    private APIInspector m_APIInspector = null;
    private DebugMessages m_DebugMessages = null;
    private TimelineBar m_TimelineBar = null;
    private TextureViewer m_TextureViewer = null;
    private PipelineStateViewer m_PipelineStateViewer = null;
    */

    //////////////////////////////////////////////////////////////////////////////
    // Accessors

    RenderManager *Renderer() { return &m_Renderer; }

    bool LogLoaded() { return m_LogLoaded; }
    bool LogLoading() { return m_LoadInProgress; }
    QString LogFilename() { return m_LogFile; }

    const rdctype::array<FetchFrameInfo> &FrameInfo() { return m_FrameInfo; }
    const APIProperties &APIProps() { return m_APIProps; }

    // TODO: support multiple frames
    uint32_t CurFrame() { return m_FrameID; }
    uint32_t CurEvent() { return m_EventID; }

    const FetchDrawcall *CurDrawcall() { return GetDrawcall(CurFrame(), CurEvent()); }
    const rdctype::array<FetchDrawcall> &CurDrawcalls(uint32_t frame) { return m_Drawcalls[frame]; }

    FetchTexture *GetTexture(ResourceId id) { return m_Textures[id]; }
    const rdctype::array<FetchTexture> &GetTextures() { return m_TextureList; }

    FetchBuffer *GetBuffer(ResourceId id) { return m_Buffers[id]; }
    const rdctype::array<FetchBuffer> &GetBuffers() { return m_BufferList; }

    QList<DebugMessage> DebugMessages;
    int UnreadMessageCount;
    void AddMessages(rdctype::array<DebugMessage> &msgs)
    {
      UnreadMessageCount += msgs.count;
      for(int i=0; i < msgs.count; i++)
        DebugMessages.push_back(msgs[i]);
    }

    D3D11PipelineState CurD3D11PipelineState;
    GLPipelineState CurGLPipelineState;
    //CommonPipelineState CurPipelineState;

  private:
    RenderManager m_Renderer;

    QList<ILogViewerForm*> m_LogViewers;
    QList<ILogLoadProgressListener*> m_ProgressListeners;

    bool m_LogLoaded, m_LoadInProgress;
    QString m_LogFile;

    uint32_t m_FrameID, m_EventID;

    const FetchDrawcall *GetDrawcall(const rdctype::array<FetchDrawcall> &draws, uint32_t eventID)
    {
      for(int i=0; i < draws.count; i++)
      {
        if (draws[i].children.count > 0)
        {
          const FetchDrawcall *draw = GetDrawcall(draws[i].children, eventID);
          if (draw != NULL) return draw;
        }

        if (draws[i].eventID == eventID)
          return &draws[i];
      }

      return NULL;
    }

    const FetchDrawcall *GetDrawcall(uint32_t frameID, uint32_t eventID)
    {
      if (m_Drawcalls == NULL || frameID >= (uint32_t)m_FrameInfo.count)
        return NULL;

      return GetDrawcall(m_Drawcalls[frameID], eventID);
    }

    rdctype::array<FetchDrawcall> *m_Drawcalls;

    APIProperties m_APIProps;
    rdctype::array<FetchFrameInfo> m_FrameInfo;

    QMap<ResourceId, FetchTexture*> m_Textures;
    rdctype::array<FetchTexture> m_TextureList;
    QMap<ResourceId, FetchBuffer*> m_Buffers;
    rdctype::array<FetchBuffer> m_BufferList;

    // Windows
    MainWindow *m_MainWindow;
};

// Utility class for invoking a lambda on the GUI thread.
// This is supported by QTimer::singleShot on Qt 5.4 but it's probably
// wise not to require a higher version that necessary.
#include <functional>

class GUIInvoke : public QObject
{
    Q_OBJECT
    GUIInvoke(const std::function<void()> &f) : func(f) {}
    std::function<void()> func;
  public:
    static void call(const std::function<void()> &f);
    static void blockcall(const std::function<void()> &f);

  protected slots:
    void doInvoke()
    {
      func();
      deleteLater();
    }
};

// Utility class for calling a lambda on a new thread.
#include <QThread>

class LambdaThread : public QObject
{
    Q_OBJECT

    std::function<void()> m_func;
    QThread *m_Thread;

  public slots:
    void process()
    {
      m_func();
      m_Thread->quit();
      deleteLater();
      m_Thread->deleteLater();
    }

  public:
    explicit LambdaThread(std::function<void()> f)
    {
      m_Thread = new QThread();
      m_func = f;
      moveToThread(m_Thread);
      connect(m_Thread, SIGNAL(started()), this, SLOT(process()));
    }

    void start(QThread::Priority prio = QThread::InheritPriority) { m_Thread->start(prio); }
    bool isRunning() { return m_Thread->isRunning(); }
};

#endif // CORE_H
