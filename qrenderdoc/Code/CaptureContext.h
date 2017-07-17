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
#include <QX11Info>
#endif

class MainWindow;
class EventBrowser;
class APIInspector;
class PipelineStateViewer;
class BufferViewer;
class TextureViewer;
class CaptureDialog;
class DebugMessageView;
class StatisticsViewer;
class TimelineBar;
class PythonShell;

QString ConfigFilePath(const QString &filename);

class CaptureContext : public ICaptureContext
{
  Q_DECLARE_TR_FUNCTIONS(CaptureContext);

public:
  CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent, bool temp,
                 PersistantConfig &cfg);
  ~CaptureContext();

  bool isRunning();

  QString ConfigFilePath(const QString &filename) override { return ::ConfigFilePath(filename); }
  QString TempLogFilename(QString appname) override;

  //////////////////////////////////////////////////////////////////////////////
  // Control functions

  void LoadLogfile(const QString &logFile, const QString &origFilename, bool temporary,
                   bool local) override;

  void CloseLogfile() override;

  void SetEventID(const QVector<ILogViewer *> &exclude, uint32_t selectedEventID, uint32_t eventID,
                  bool force = false) override;
  void RefreshStatus() override { SetEventID({}, m_SelectedEventID, m_EventID, true); }
  void AddLogViewer(ILogViewer *f) override
  {
    m_LogViewers.push_back(f);

    if(LogLoaded())
    {
      f->OnLogfileLoaded();
      f->OnEventChanged(CurEvent());
    }
  }

  void RemoveLogViewer(ILogViewer *f) override { m_LogViewers.removeAll(f); }
  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  ReplayManager &Replay() override { return m_Renderer; }
  bool LogLoaded() override { return m_LogLoaded; }
  bool IsLogLocal() override { return m_LogLocal; }
  bool LogLoading() override { return m_LoadInProgress; }
  QString LogFilename() override { return m_LogFile; }
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
  const rdctype::array<DrawcallDescription> &CurDrawcalls() override { return m_Drawcalls; }
  TextureDescription *GetTexture(ResourceId id) override { return m_Textures[id]; }
  const rdctype::array<TextureDescription> &GetTextures() override { return m_TextureList; }
  BufferDescription *GetBuffer(ResourceId id) override { return m_Buffers[id]; }
  const rdctype::array<BufferDescription> &GetBuffers() override { return m_BufferList; }
  const DrawcallDescription *GetDrawcall(uint32_t eventID) override
  {
    return GetDrawcall(m_Drawcalls, eventID);
  }

  WindowingSystem CurWindowingSystem() override { return m_CurWinSystem; }
  void *FillWindowingData(uintptr_t winId) override;

  const QVector<DebugMessage> &DebugMessages() override { return m_DebugMessages; }
  int UnreadMessageCount() override { return m_UnreadMessageCount; }
  void MarkMessagesRead() override { m_UnreadMessageCount = 0; }
  void AddMessages(const rdctype::array<DebugMessage> &msgs) override;

  IMainWindow *GetMainWindow() override;
  IEventBrowser *GetEventBrowser() override;
  IAPIInspector *GetAPIInspector() override;
  ITextureViewer *GetTextureViewer() override;
  IBufferViewer *GetMeshPreview() override;
  IPipelineStateViewer *GetPipelineViewer() override;
  ICaptureDialog *GetCaptureDialog() override;
  IDebugMessageView *GetDebugMessageView() override;
  IStatisticsViewer *GetStatisticsViewer() override;
  ITimelineBar *GetTimelineBar() override;
  IPythonShell *GetPythonShell() override;

  bool HasEventBrowser() override { return m_EventBrowser != NULL; }
  bool HasAPIInspector() override { return m_APIInspector != NULL; }
  bool HasTextureViewer() override { return m_TextureViewer != NULL; }
  bool HasPipelineViewer() override { return m_PipelineViewer != NULL; }
  bool HasMeshPreview() override { return m_MeshPreview != NULL; }
  bool HasCaptureDialog() override { return m_CaptureDialog != NULL; }
  bool HasDebugMessageView() override { return m_DebugMessageView != NULL; }
  bool HasStatisticsViewer() override { return m_StatisticsViewer != NULL; }
  bool HasTimelineBar() override { return m_TimelineBar != NULL; }
  bool HasPythonShell() override { return m_PythonShell != NULL; }
  void ShowEventBrowser() override;
  void ShowAPIInspector() override;
  void ShowTextureViewer() override;
  void ShowMeshPreview() override;
  void ShowPipelineViewer() override;
  void ShowCaptureDialog() override;
  void ShowDebugMessageView() override;
  void ShowStatisticsViewer() override;
  void ShowTimelineBar() override;
  void ShowPythonShell() override;

  IShaderViewer *EditShader(bool customShader, const QString &entryPoint, const QStringMap &files,
                            IShaderViewer::SaveCallback saveCallback,
                            IShaderViewer::CloseCallback closeCallback) override;

  IShaderViewer *DebugShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                             ShaderStage stage, ShaderDebugTrace *trace,
                             const QString &debugContext) override;

  IShaderViewer *ViewShader(const ShaderBindpointMapping *bind, const ShaderReflection *shader,
                            ShaderStage stage) override;

  IBufferViewer *ViewBuffer(uint64_t byteOffset, uint64_t byteSize, ResourceId id,
                            const QString &format = QString()) override;
  IBufferViewer *ViewTextureAsBuffer(uint32_t arrayIdx, uint32_t mip, ResourceId id,
                                     const QString &format = QString()) override;

  IConstantBufferPreviewer *ViewConstantBuffer(ShaderStage stage, uint32_t slot,
                                               uint32_t idx) override;
  IPixelHistoryView *ViewPixelHistory(ResourceId texID, int x, int y,
                                      const TextureDisplay &display) override;

  QWidget *CreateBuiltinWindow(const QString &objectName) override;
  void BuiltinWindowClosed(QWidget *window) override;

  void RaiseDockWindow(QWidget *dockWindow) override;
  void AddDockWindow(QWidget *newWindow, DockReference ref, QWidget *refWindow,
                     float percentage = 0.5f) override;

  D3D11Pipe::State &CurD3D11PipelineState() override { return m_CurD3D11PipelineState; }
  D3D12Pipe::State &CurD3D12PipelineState() override { return m_CurD3D12PipelineState; }
  GLPipe::State &CurGLPipelineState() override { return m_CurGLPipelineState; }
  VKPipe::State &CurVulkanPipelineState() override { return m_CurVulkanPipelineState; }
  CommonPipelineState &CurPipelineState() override { return m_CurPipelineState; }
  PersistantConfig &Config() override { return m_Config; }
private:
  ReplayManager m_Renderer;

  D3D11Pipe::State m_CurD3D11PipelineState;
  D3D12Pipe::State m_CurD3D12PipelineState;
  GLPipe::State m_CurGLPipelineState;
  VKPipe::State m_CurVulkanPipelineState;
  CommonPipelineState m_CurPipelineState;

  PersistantConfig &m_Config;

  QVector<ILogViewer *> m_LogViewers;

  bool m_LogLoaded, m_LoadInProgress, m_LogLocal;
  QString m_LogFile;

  QVector<DebugMessage> m_DebugMessages;
  int m_UnreadMessageCount;

  bool PassEquivalent(const DrawcallDescription &a, const DrawcallDescription &b);
  bool ContainsMarker(const rdctype::array<DrawcallDescription> &m_Drawcalls);
  void AddFakeProfileMarkers();

  float m_LoadProgress = 0.0f;
  float m_PostloadProgress = 0.0f;
  float UpdateLoadProgress();

  void LoadLogfileThreaded(const QString &logFile, const QString &origFilename, bool temporary,
                           bool local);

  uint32_t m_SelectedEventID;
  uint32_t m_EventID;

  const DrawcallDescription *GetDrawcall(const rdctype::array<DrawcallDescription> &draws,
                                         uint32_t eventID)
  {
    for(const DrawcallDescription &d : draws)
    {
      if(!d.children.empty())
      {
        const DrawcallDescription *draw = GetDrawcall(d.children, eventID);
        if(draw != NULL)
          return draw;
      }

      if(d.eventID == eventID)
        return &d;
    }

    return NULL;
  }

  void setupDockWindow(QWidget *shad);

  rdctype::array<DrawcallDescription> m_Drawcalls;

  APIProperties m_APIProps;
  FrameDescription m_FrameInfo;
  DrawcallDescription *m_FirstDrawcall = NULL;
  DrawcallDescription *m_LastDrawcall = NULL;

  QMap<ResourceId, TextureDescription *> m_Textures;
  rdctype::array<TextureDescription> m_TextureList;
  QMap<ResourceId, BufferDescription *> m_Buffers;
  rdctype::array<BufferDescription> m_BufferList;

  rdctype::array<WindowingSystem> m_WinSystems;

  WindowingSystem m_CurWinSystem;

#if defined(RENDERDOC_PLATFORM_LINUX)
  xcb_connection_t *m_XCBConnection;
  Display *m_X11Display;
#endif

  QIcon *m_Icon;

  // Windows
  MainWindow *m_MainWindow = NULL;
  EventBrowser *m_EventBrowser = NULL;
  APIInspector *m_APIInspector = NULL;
  TextureViewer *m_TextureViewer = NULL;
  BufferViewer *m_MeshPreview = NULL;
  PipelineStateViewer *m_PipelineViewer = NULL;
  CaptureDialog *m_CaptureDialog = NULL;
  DebugMessageView *m_DebugMessageView = NULL;
  StatisticsViewer *m_StatisticsViewer = NULL;
  TimelineBar *m_TimelineBar = NULL;
  PythonShell *m_PythonShell = NULL;
};
