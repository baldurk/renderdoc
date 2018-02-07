/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
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

#include <QDebug>
#include <QList>
#include <QMap>
#include <QMessageBox>
#include <QString>
#include <QtWidgets/QWidget>
#include "Interface/QRDInterface.h"
#include "ReplayManager.h"

#if defined(RENDERDOC_PLATFORM_LINUX)
#if defined(RENDERDOC_WINDOWING_XLIB) || defined(RENDERDOC_WINDOWING_XCB)
#include <QX11Info>
#endif
#if defined(RENDERDOC_WINDOWING_WAYLAND)
#include <wayland-client.h>
#endif
#endif

class MainWindow;
class EventBrowser;
class APIInspector;
class PipelineStateViewer;
class BufferViewer;
class TextureViewer;
class CaptureDialog;
class DebugMessageView;
class CommentView;
class PerformanceCounterViewer;
class StatisticsViewer;
class TimelineBar;
class PythonShell;
class ResourceInspector;

class CaptureContext : public ICaptureContext
{
  Q_DECLARE_TR_FUNCTIONS(CaptureContext);

public:
  CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent, bool temp,
                 PersistantConfig &cfg);
  ~CaptureContext();

  bool isRunning();

  rdcstr TempCaptureFilename(const rdcstr &appname) override;

  //////////////////////////////////////////////////////////////////////////////
  // Control functions

  void LoadCapture(const rdcstr &captureFile, const rdcstr &origFilename, bool temporary,
                   bool local) override;
  bool SaveCaptureTo(const rdcstr &captureFile) override;
  void RecompressCapture() override;
  void CloseCapture() override;

  void SetEventID(const rdcarray<ICaptureViewer *> &exclude, uint32_t selectedEventID,
                  uint32_t eventId, bool force = false) override;

  void RefreshStatus() override { SetEventID({}, m_SelectedEventID, m_EventID, true); }
  void RefreshUIStatus(const rdcarray<ICaptureViewer *> &exclude, bool updateSelectedEvent,
                       bool updateEvent);

  void AddCaptureViewer(ICaptureViewer *f) override
  {
    m_CaptureViewers.push_back(f);

    if(IsCaptureLoaded())
    {
      f->OnCaptureLoaded();
      f->OnEventChanged(CurEvent());
    }
  }

  void RemoveCaptureViewer(ICaptureViewer *f) override { m_CaptureViewers.removeAll(f); }
  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  ReplayManager &Replay() override { return m_Renderer; }
  bool IsCaptureLoaded() override { return m_CaptureLoaded; }
  bool IsCaptureLocal() override { return m_CaptureLocal; }
  bool IsCaptureTemporary() override { return m_CaptureTemporary; }
  bool IsCaptureLoading() override { return m_LoadInProgress; }
  rdcstr GetCaptureFilename() override { return m_CaptureFile; }
  CaptureModifications GetCaptureModifications() override { return m_CaptureMods; }
  const FrameDescription &FrameInfo() override { return m_FrameInfo; }
  const APIProperties &APIProps() override { return m_APIProps; }
  uint32_t CurSelectedEvent() override { return m_SelectedEventID; }
  uint32_t CurEvent() override { return m_EventID; }
  const DrawcallDescription *CurSelectedDrawcall() override
  {
    return GetDrawcall(CurSelectedEvent());
  }
  const DrawcallDescription *CurDrawcall() override { return GetDrawcall(CurEvent()); }
  const DrawcallDescription *GetFirstDrawcall() override { return m_FirstDrawcall; };
  const DrawcallDescription *GetLastDrawcall() override { return m_LastDrawcall; };
  void CreateRGPMapping(uint32_t version) override;
  uint32_t GetRGPIdFromEventId(uint32_t eventId) override
  {
    if(eventId >= m_Event2RGP.size())
      return 0;
    return m_Event2RGP[eventId];
  }
  uint32_t GetEventIdFromRGPId(uint32_t RGPId) override
  {
    if(RGPId >= m_RGP2Event.size())
      return 0;
    return m_RGP2Event[RGPId];
  }
  const rdcarray<DrawcallDescription> &CurDrawcalls() override { return m_Drawcalls; }
  ResourceDescription *GetResource(ResourceId id) override { return m_Resources[id]; }
  const rdcarray<ResourceDescription> &GetResources() override { return m_ResourceList; }
  rdcstr GetResourceName(ResourceId id) override;
  bool IsAutogeneratedName(ResourceId id) override;
  bool HasResourceCustomName(ResourceId id) override;
  void SetResourceCustomName(ResourceId id, const rdcstr &name) override;
  int ResourceNameCacheID() override;
  TextureDescription *GetTexture(ResourceId id) override { return m_Textures[id]; }
  const rdcarray<TextureDescription> &GetTextures() override { return m_TextureList; }
  BufferDescription *GetBuffer(ResourceId id) override { return m_Buffers[id]; }
  const rdcarray<BufferDescription> &GetBuffers() override { return m_BufferList; }
  const DrawcallDescription *GetDrawcall(uint32_t eventId) override
  {
    return GetDrawcall(m_Drawcalls, eventId);
  }
  const SDFile &GetStructuredFile() override { return *m_StructuredFile; }
  WindowingSystem CurWindowingSystem() override { return m_CurWinSystem; }
  WindowingData CreateWindowingData(uintptr_t winId) override;

  const rdcarray<DebugMessage> &DebugMessages() override { return m_DebugMessages; }
  int UnreadMessageCount() override { return m_UnreadMessageCount; }
  void MarkMessagesRead() override { m_UnreadMessageCount = 0; }
  void AddMessages(const rdcarray<DebugMessage> &msgs) override;

  rdcstr GetNotes(const rdcstr &key) override { return m_Notes[key]; }
  void SetNotes(const rdcstr &key, const rdcstr &contents) override;
  rdcarray<EventBookmark> GetBookmarks() override { return m_Bookmarks; }
  void SetBookmark(const EventBookmark &mark) override;
  void RemoveBookmark(uint32_t EID) override;

  IMainWindow *GetMainWindow() override;
  IEventBrowser *GetEventBrowser() override;
  IAPIInspector *GetAPIInspector() override;
  ITextureViewer *GetTextureViewer() override;
  IBufferViewer *GetMeshPreview() override;
  IPipelineStateViewer *GetPipelineViewer() override;
  ICaptureDialog *GetCaptureDialog() override;
  IDebugMessageView *GetDebugMessageView() override;
  ICommentView *GetCommentView() override;
  IPerformanceCounterViewer *GetPerformanceCounterViewer() override;
  IStatisticsViewer *GetStatisticsViewer() override;
  ITimelineBar *GetTimelineBar() override;
  IPythonShell *GetPythonShell() override;
  IResourceInspector *GetResourceInspector() override;

  bool HasEventBrowser() override { return m_EventBrowser != NULL; }
  bool HasAPIInspector() override { return m_APIInspector != NULL; }
  bool HasTextureViewer() override { return m_TextureViewer != NULL; }
  bool HasPipelineViewer() override { return m_PipelineViewer != NULL; }
  bool HasMeshPreview() override { return m_MeshPreview != NULL; }
  bool HasCaptureDialog() override { return m_CaptureDialog != NULL; }
  bool HasDebugMessageView() override { return m_DebugMessageView != NULL; }
  bool HasCommentView() override { return m_CommentView != NULL; }
  bool HasPerformanceCounterViewer() override { return m_PerformanceCounterViewer != NULL; }
  bool HasStatisticsViewer() override { return m_StatisticsViewer != NULL; }
  bool HasTimelineBar() override { return m_TimelineBar != NULL; }
  bool HasPythonShell() override { return m_PythonShell != NULL; }
  bool HasResourceInspector() override { return m_ResourceInspector != NULL; }
  void ShowEventBrowser() override;
  void ShowAPIInspector() override;
  void ShowTextureViewer() override;
  void ShowMeshPreview() override;
  void ShowPipelineViewer() override;
  void ShowCaptureDialog() override;
  void ShowDebugMessageView() override;
  void ShowCommentView() override;
  void ShowPerformanceCounterViewer() override;
  void ShowStatisticsViewer() override;
  void ShowTimelineBar() override;
  void ShowPythonShell() override;
  void ShowResourceInspector() override;

  IShaderViewer *EditShader(bool customShader, const rdcstr &entryPoint, const rdcstrpairs &files,
                            IShaderViewer::SaveCallback saveCallback,
                            IShaderViewer::CloseCallback closeCallback) override;

  IShaderViewer *DebugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                             ResourceId pipeline, ShaderDebugTrace *trace,
                             const rdcstr &debugContext) override;

  IShaderViewer *ViewShader(const ShaderReflection *shader, ResourceId pipeline) override;

  IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                            const rdcstr &format = "") override;
  IBufferViewer *ViewTextureAsBuffer(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                                     const rdcstr &format = "") override;

  IConstantBufferPreviewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot,
                                               uint32_t idx) override;
  IPixelHistoryView *ViewPixelHistory(ResourceId texID, int x, int y,
                                      const TextureDisplay &display) override;

  QWidget *CreateBuiltinWindow(const rdcstr &objectName) override;
  void BuiltinWindowClosed(QWidget *window) override;

  void RaiseDockWindow(QWidget *dockWindow) override;
  void AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                     float percentage = 0.5f) override;

  const D3D11Pipe::State &CurD3D11PipelineState() override { return *m_CurD3D11PipelineState; }
  const D3D12Pipe::State &CurD3D12PipelineState() override { return *m_CurD3D12PipelineState; }
  const GLPipe::State &CurGLPipelineState() override { return *m_CurGLPipelineState; }
  const VKPipe::State &CurVulkanPipelineState() override { return *m_CurVulkanPipelineState; }
  CommonPipelineState &CurPipelineState() override { return m_CurPipelineState; }
  PersistantConfig &Config() override { return m_Config; }
private:
  ReplayManager m_Renderer;

  const D3D11Pipe::State *m_CurD3D11PipelineState;
  const D3D12Pipe::State *m_CurD3D12PipelineState;
  const GLPipe::State *m_CurGLPipelineState;
  const VKPipe::State *m_CurVulkanPipelineState;
  CommonPipelineState m_CurPipelineState;

  D3D11Pipe::State m_DummyD3D11;
  D3D12Pipe::State m_DummyD3D12;
  GLPipe::State m_DummyGL;
  VKPipe::State m_DummyVK;

  PersistantConfig &m_Config;

  QVector<ICaptureViewer *> m_CaptureViewers;

  bool m_CaptureLoaded = false, m_LoadInProgress = false, m_CaptureLocal = false,
       m_CaptureTemporary = false;
  QString m_CaptureFile;
  CaptureModifications m_CaptureMods = CaptureModifications::NoModifications;

  rdcarray<DebugMessage> m_DebugMessages;
  int m_UnreadMessageCount = 0;

  void CreateRGPMapping(const QStringList &eventNames,
                        const rdcarray<DrawcallDescription> &drawcalls);

  bool PassEquivalent(const DrawcallDescription &a, const DrawcallDescription &b);
  bool ContainsMarker(const rdcarray<DrawcallDescription> &m_Drawcalls);
  void AddFakeProfileMarkers();

  void SaveChanges();

  void SaveRenames();
  void LoadRenames(const QString &data);

  void SaveBookmarks();
  void LoadBookmarks(const QString &data);

  void SaveNotes();
  void LoadNotes(const QString &data);

  float m_LoadProgress = 0.0f;
  float m_PostloadProgress = 0.0f;
  float UpdateLoadProgress();

  void LoadCaptureThreaded(const QString &captureFile, const QString &origFilename, bool temporary,
                           bool local);

  uint32_t m_SelectedEventID = 0;
  uint32_t m_EventID = 0;

  const DrawcallDescription *GetDrawcall(const rdcarray<DrawcallDescription> &draws, uint32_t eventId)
  {
    for(const DrawcallDescription &d : draws)
    {
      if(!d.children.empty())
      {
        const DrawcallDescription *draw = GetDrawcall(d.children, eventId);
        if(draw != NULL)
          return draw;
      }

      if(d.eventId == eventId)
        return &d;
    }

    return NULL;
  }

  void setupDockWindow(QWidget *shad);
  rdcarray<DrawcallDescription> m_Drawcalls;

  APIProperties m_APIProps;
  FrameDescription m_FrameInfo;
  DrawcallDescription *m_FirstDrawcall = NULL;
  DrawcallDescription *m_LastDrawcall = NULL;

  rdcarray<uint32_t> m_Event2RGP;
  rdcarray<uint32_t> m_RGP2Event;

  QMap<ResourceId, TextureDescription *> m_Textures;
  rdcarray<TextureDescription> m_TextureList;
  QMap<ResourceId, BufferDescription *> m_Buffers;
  rdcarray<BufferDescription> m_BufferList;
  QMap<ResourceId, ResourceDescription *> m_Resources;
  rdcarray<ResourceDescription> m_ResourceList;

  QList<EventBookmark> m_Bookmarks;

  QMap<QString, QString> m_Notes;

  QMap<ResourceId, QString> m_CustomNames;
  int m_CustomNameCachedID = 1;

  const SDFile *m_StructuredFile = NULL;
  SDFile m_DummySDFile;

  rdcarray<WindowingSystem> m_WinSystems;

  WindowingSystem m_CurWinSystem = WindowingSystem::Unknown;

#if defined(RENDERDOC_PLATFORM_LINUX)
#if defined(RENDERDOC_WINDOWING_XLIB) || defined(RENDERDOC_WINDOWING_XCB)
  xcb_connection_t *m_XCBConnection = NULL;
  Display *m_X11Display = NULL;
#endif
#if defined(RENDERDOC_WINDOWING_WAYLAND)
  struct wl_display *m_WaylandDisplay = NULL;
#endif
#endif

  QIcon *m_Icon = NULL;

  // Windows
  MainWindow *m_MainWindow = NULL;
  EventBrowser *m_EventBrowser = NULL;
  APIInspector *m_APIInspector = NULL;
  TextureViewer *m_TextureViewer = NULL;
  BufferViewer *m_MeshPreview = NULL;
  PipelineStateViewer *m_PipelineViewer = NULL;
  CaptureDialog *m_CaptureDialog = NULL;
  DebugMessageView *m_DebugMessageView = NULL;
  CommentView *m_CommentView = NULL;
  PerformanceCounterViewer *m_PerformanceCounterViewer = NULL;
  StatisticsViewer *m_StatisticsViewer = NULL;
  TimelineBar *m_TimelineBar = NULL;
  PythonShell *m_PythonShell = NULL;
  ResourceInspector *m_ResourceInspector = NULL;
};
