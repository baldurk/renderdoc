#pragma once

// take care before adding any more headers here, as they must be converted to python types. Any
// types in the RenderDoc core interface are already wrapped, and Qt types must either be manually
// converted directly to python, or interfaced with PySide, otherwise we get into the situation
// where pyside and SWIG have independent incompatible wrappers of Qt types
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

#include <functional>

// we depend on the internal RenderDoc API, but the bindings for that are imported entirely
#include "renderdoc_replay.h"

// this is pre-declared as an opaque type as we only support converting to QWidget* via PySide
class QWidget;

// we only support QVariant as an 'internal' interface, it's not exposed to python. However we need
// to use it in constructors/operators so conditionally compile it rather than split small structs
// into interface/implementations
#if defined(SWIG)

#define VARIANT_CAST(classname)

#else

#include <QVariant>

// conversion to/from QVariant
#define VARIANT_CAST(classname)   \
  classname(const QVariant &var); \
  operator QVariant() const;

#endif

#include "CommonPipelineState.h"
#include "PersistantConfig.h"
#include "RemoteHost.h"

struct ICaptureContext;

struct CaptureSettings
{
  CaptureSettings();

  VARIANT_CAST(CaptureSettings);

  CaptureOptions Options;
  bool Inject;
  bool AutoStart;
  QString Executable;
  QString WorkingDir;
  QString CmdLine;
  QList<EnvironmentModification> Environment;
};

DECLARE_REFLECTION_STRUCT(CaptureSettings);

struct IMainWindow
{
  virtual QWidget *Widget() = 0;

protected:
  IMainWindow() = default;
  ~IMainWindow() = default;
};

DECLARE_REFLECTION_STRUCT(IMainWindow);

struct IEventBrowser
{
  virtual QWidget *Widget() = 0;

protected:
  IEventBrowser() = default;
  ~IEventBrowser() = default;
};

DECLARE_REFLECTION_STRUCT(IEventBrowser);

struct IAPIInspector
{
  virtual QWidget *Widget() = 0;

  virtual void Refresh() = 0;

protected:
  IAPIInspector() = default;
  ~IAPIInspector() = default;
};

DECLARE_REFLECTION_STRUCT(IAPIInspector);

struct IPipelineStateViewer
{
  virtual QWidget *Widget() = 0;

  virtual bool SaveShaderFile(const ShaderReflection *shader) = 0;

protected:
  IPipelineStateViewer() = default;
  ~IPipelineStateViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IPipelineStateViewer);

struct ITextureViewer
{
  virtual QWidget *Widget() = 0;

  virtual void ViewTexture(ResourceId ID, bool focus) = 0;
  virtual void GotoLocation(int x, int y) = 0;

protected:
  ITextureViewer() = default;
  ~ITextureViewer() = default;
};

DECLARE_REFLECTION_STRUCT(ITextureViewer);

struct IBufferViewer
{
  virtual QWidget *Widget() = 0;

  virtual void ScrollToRow(int row, MeshDataStage stage = MeshDataStage::VSIn) = 0;
  virtual void ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                          const QString &format = "") = 0;
  virtual void ViewTexture(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                           const QString &format = "") = 0;

protected:
  IBufferViewer() = default;
  ~IBufferViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IBufferViewer);

struct ICaptureDialog
{
  virtual QWidget *Widget() = 0;

  virtual bool IsInjectMode() = 0;
  virtual void SetInjectMode(bool inject) = 0;

  virtual void SetExecutableFilename(const QString &filename) = 0;
  virtual void SetWorkingDirectory(const QString &dir) = 0;
  virtual void SetCommandLine(const QString &cmd) = 0;
  virtual void SetEnvironmentModifications(const QList<EnvironmentModification> &modifications) = 0;

  virtual void SetSettings(CaptureSettings settings) = 0;
  virtual CaptureSettings Settings() = 0;

  virtual void TriggerCapture() = 0;

  virtual void LoadSettings(QString filename) = 0;
  virtual void SaveSettings(QString filename) = 0;
  virtual void UpdateGlobalHook() = 0;

protected:
  ICaptureDialog() = default;
  ~ICaptureDialog() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureDialog);

struct IDebugMessageView
{
  virtual QWidget *Widget() = 0;

protected:
  IDebugMessageView() = default;
  ~IDebugMessageView() = default;
};

DECLARE_REFLECTION_STRUCT(IDebugMessageView);

struct IStatisticsViewer
{
  virtual QWidget *Widget() = 0;

protected:
  IStatisticsViewer() = default;
  ~IStatisticsViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IStatisticsViewer);

struct IShaderViewer
{
  typedef std::function<void(ICaptureContext *ctx, IShaderViewer *, const QStringMap &)> SaveCallback;
  typedef std::function<void(ICaptureContext *ctx)> CloseCallback;

  virtual QWidget *Widget() = 0;

  virtual int CurrentStep() = 0;
  virtual void SetCurrentStep(int step) = 0;

  virtual void ToggleBreakpoint(int instruction = -1) = 0;

  virtual void ShowErrors(const QString &errors) = 0;

protected:
  IShaderViewer() = default;
  ~IShaderViewer() = default;
};

DECLARE_REFLECTION_STRUCT(IShaderViewer);

struct IConstantBufferPreviewer
{
  virtual QWidget *Widget() = 0;

protected:
  IConstantBufferPreviewer() = default;
  ~IConstantBufferPreviewer() = default;
};

DECLARE_REFLECTION_STRUCT(IConstantBufferPreviewer);

struct IPixelHistoryView
{
  virtual QWidget *Widget() = 0;
  virtual void SetHistory(const rdctype::array<PixelModification> &history) = 0;

protected:
  IPixelHistoryView() = default;
  ~IPixelHistoryView() = default;
};

DECLARE_REFLECTION_STRUCT(IPixelHistoryView);

struct ILogViewer
{
  virtual void OnLogfileLoaded() = 0;
  virtual void OnLogfileClosed() = 0;

  // These 2 functions distinguish between the event which is actually
  // selected and the event which the displayed state should be taken from. In
  // the case of an event with children, OnSelectedEventChanged receives the
  // ID of the event itself, whereas OnEventChanged receives that of the last
  // child. This means that selecting an event with children displays the
  // state after all of its children have completed, the exception being that
  // the API inspector uses the selected event ID to display the API calls of
  // that event rather than of the last child.
  virtual void OnSelectedEventChanged(uint32_t eventID) = 0;
  virtual void OnEventChanged(uint32_t eventID) = 0;

protected:
  ILogViewer() = default;
  ~ILogViewer() = default;
};

DECLARE_REFLECTION_STRUCT(ILogViewer);

struct IReplayManager
{
  typedef std::function<void(IReplayController *)> InvokeCallback;
  typedef std::function<void(const rdctype::str &, const rdctype::array<PathEntry> &)> DirectoryBrowseCallback;

  virtual void DeleteCapture(const QString &logfile, bool local) = 0;

  // this tagged version is for cases when we might send a request - e.g. to pick a vertex or pixel
  // - and want to pre-empt it with a new request before the first has returned. Either because some
  // other work is taking a while or because we're sending requests faster than they can be
  // processed.
  // the manager processes only the request on the top of the queue, so when a new tagged invoke
  // comes in, we remove any other requests in the queue before it that have the same tag
  virtual void AsyncInvoke(const QString &tag, InvokeCallback m) = 0;
  virtual void AsyncInvoke(InvokeCallback m) = 0;
  virtual void BlockInvoke(InvokeCallback m) = 0;

  virtual ReplayStatus ConnectToRemoteServer(RemoteHost *host) = 0;
  virtual void DisconnectFromRemoteServer() = 0;
  virtual void ShutdownServer() = 0;
  virtual void PingRemote() = 0;

  virtual const RemoteHost *CurrentRemote() = 0;
  virtual uint32_t ExecuteAndInject(const QString &exe, const QString &workingDir,
                                    const QString &cmdLine, const QList<EnvironmentModification> &env,
                                    const QString &logfile, CaptureOptions opts) = 0;

  virtual QStringList GetRemoteSupport() = 0;
  virtual void GetHomeFolder(bool synchronous, DirectoryBrowseCallback cb) = 0;
  virtual void ListFolder(QString path, bool synchronous, DirectoryBrowseCallback cb) = 0;
  virtual QString CopyCaptureToRemote(const QString &localpath, QWidget *window) = 0;
  virtual void CopyCaptureFromRemote(const QString &remotepath, const QString &localpath,
                                     QWidget *window) = 0;

protected:
  IReplayManager() = default;
  ~IReplayManager() = default;
};

DECLARE_REFLECTION_STRUCT(IReplayManager);

// should match ToolWindowManager::AreaReferenceType
enum class DockReference : int
{
  LastUsedArea,
  NewFloatingArea,
  EmptySpace,
  NoArea,
  AddTo,
  LeftOf,
  RightOf,
  TopOf,
  BottomOf,

  // extra values here
  MainToolArea,
  LeftToolArea,
};

struct ICaptureContext
{
  virtual QString ConfigFilePath(const QString &filename) = 0;

  virtual QString TempLogFilename(QString appname) = 0;

  virtual void LoadLogfile(const QString &logFile, const QString &origFilename, bool temporary,
                           bool local) = 0;
  virtual void CloseLogfile() = 0;

  virtual void SetEventID(const QVector<ILogViewer *> &exclude, uint32_t selectedEventID,
                          uint32_t eventID, bool force = false) = 0;
  virtual void RefreshStatus() = 0;

  virtual void AddLogViewer(ILogViewer *f) = 0;
  virtual void RemoveLogViewer(ILogViewer *f) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  virtual IReplayManager &Replay() = 0;
  virtual bool LogLoaded() = 0;
  virtual bool IsLogLocal() = 0;
  virtual bool LogLoading() = 0;
  virtual QString LogFilename() = 0;
  virtual const FrameDescription &FrameInfo() = 0;
  virtual const APIProperties &APIProps() = 0;
  virtual uint32_t CurSelectedEvent() = 0;
  virtual uint32_t CurEvent() = 0;
  virtual const DrawcallDescription *CurSelectedDrawcall() = 0;
  virtual const DrawcallDescription *CurDrawcall() = 0;
  virtual const rdctype::array<DrawcallDescription> &CurDrawcalls() = 0;
  virtual TextureDescription *GetTexture(ResourceId id) = 0;
  virtual const rdctype::array<TextureDescription> &GetTextures() = 0;
  virtual BufferDescription *GetBuffer(ResourceId id) = 0;
  virtual const rdctype::array<BufferDescription> &GetBuffers() = 0;

  virtual const DrawcallDescription *GetDrawcall(uint32_t eventID) = 0;

  virtual WindowingSystem CurWindowingSystem() = 0;
  virtual void *FillWindowingData(uintptr_t winId) = 0;

  virtual const QVector<DebugMessage> &DebugMessages() = 0;
  virtual int UnreadMessageCount() = 0;
  virtual void MarkMessagesRead() = 0;
  virtual void AddMessages(const rdctype::array<DebugMessage> &msgs) = 0;

  virtual IMainWindow *GetMainWindow() = 0;
  virtual IEventBrowser *GetEventBrowser() = 0;
  virtual IAPIInspector *GetAPIInspector() = 0;
  virtual ITextureViewer *GetTextureViewer() = 0;
  virtual IBufferViewer *GetMeshPreview() = 0;
  virtual IPipelineStateViewer *GetPipelineViewer() = 0;
  virtual ICaptureDialog *GetCaptureDialog() = 0;
  virtual IDebugMessageView *GetDebugMessageView() = 0;
  virtual IStatisticsViewer *GetStatisticsViewer() = 0;

  virtual bool HasEventBrowser() = 0;
  virtual bool HasAPIInspector() = 0;
  virtual bool HasTextureViewer() = 0;
  virtual bool HasPipelineViewer() = 0;
  virtual bool HasMeshPreview() = 0;
  virtual bool HasCaptureDialog() = 0;
  virtual bool HasDebugMessageView() = 0;
  virtual bool HasStatisticsViewer() = 0;

  virtual void ShowEventBrowser() = 0;
  virtual void ShowAPIInspector() = 0;
  virtual void ShowTextureViewer() = 0;
  virtual void ShowMeshPreview() = 0;
  virtual void ShowPipelineViewer() = 0;
  virtual void ShowCaptureDialog() = 0;
  virtual void ShowDebugMessageView() = 0;
  virtual void ShowStatisticsViewer() = 0;

  virtual IShaderViewer *EditShader(bool customShader, const QString &entryPoint,
                                    const QStringMap &files, IShaderViewer::SaveCallback saveCallback,
                                    IShaderViewer::CloseCallback closeCallback) = 0;
  virtual IShaderViewer *DebugShader(const ShaderBindpointMapping *bind,
                                     const ShaderReflection *shader, ShaderStage stage,
                                     ShaderDebugTrace *trace, const QString &debugContext) = 0;


  virtual IShaderViewer *ViewShader(const ShaderBindpointMapping *bind,
                                    const ShaderReflection *shader, ShaderStage stage) = 0;
  virtual IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                                    const QString &format = "") = 0;
  virtual IBufferViewer *ViewTextureAsBuffer(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                                             const QString &format = "") = 0;

  virtual IConstantBufferPreviewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot,
                                                       uint32_t idx) = 0;
  virtual IPixelHistoryView *ViewPixelHistory(ResourceId texID, int x, int y,
                                              const TextureDisplay &display) = 0;

  virtual QWidget *CreateBuiltinWindow(const QString &objectName) = 0;
  virtual void BuiltinWindowClosed(QWidget *window) = 0;

  virtual void RaiseDockWindow(QWidget *dockWindow) = 0;
  virtual void AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                             float percentage = 0.5f) = 0;

  virtual D3D11Pipe::State &CurD3D11PipelineState() = 0;
  virtual D3D12Pipe::State &CurD3D12PipelineState() = 0;
  virtual GLPipe::State &CurGLPipelineState() = 0;
  virtual VKPipe::State &CurVulkanPipelineState() = 0;
  virtual CommonPipelineState &CurPipelineState() = 0;

  virtual PersistantConfig &Config() = 0;

protected:
  ICaptureContext() = default;
  ~ICaptureContext() = default;
};

DECLARE_REFLECTION_STRUCT(ICaptureContext);
