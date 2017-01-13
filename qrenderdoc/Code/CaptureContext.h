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
#include "CommonPipelineState.h"
#include "PersistantConfig.h"
#include "RenderManager.h"

#if defined(RENDERDOC_PLATFORM_LINUX)
#include <QX11Info>
#endif

struct ILogViewerForm
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
};

class MainWindow;
class EventBrowser;
class APIInspector;
class PipelineStateViewer;
class BufferViewer;
class TextureViewer;
class CaptureDialog;
class QProgressDialog;

class CaptureContext
{
public:
  CaptureContext(QString paramFilename, QString remoteHost, uint32_t remoteIdent, bool temp,
                 PersistantConfig &cfg);
  ~CaptureContext();

  bool isRunning();

  static QString ConfigFile(const QString &filename);

  QString TempLogFilename(QString appname);

  //////////////////////////////////////////////////////////////////////////////
  // Control functions

  void LoadLogfile(const QString &logFile, const QString &origFilename, bool temporary, bool local);
  void CloseLogfile();

  void SetEventID(ILogViewerForm *exclude, uint32_t selectedEventID, uint32_t eventID,
                  bool force = false);
  void RefreshStatus() { SetEventID(NULL, m_SelectedEventID, m_EventID, true); }
  void AddLogViewer(ILogViewerForm *f)
  {
    m_LogViewers.push_back(f);

    if(LogLoaded())
    {
      f->OnLogfileLoaded();
      f->OnEventChanged(CurEvent());
    }
  }

  void RemoveLogViewer(ILogViewerForm *f) { m_LogViewers.removeAll(f); }
  //////////////////////////////////////////////////////////////////////////////
  // Accessors

  RenderManager *Renderer() { return &m_Renderer; }
  bool LogLoaded() { return m_LogLoaded; }
  bool IsLogLocal() { return m_LogLocal; }
  bool LogLoading() { return m_LoadInProgress; }
  QString LogFilename() { return m_LogFile; }
  const FetchFrameInfo &FrameInfo() { return m_FrameInfo; }
  const APIProperties &APIProps() { return m_APIProps; }
  uint32_t CurSelectedEvent() { return m_SelectedEventID; }
  uint32_t CurEvent() { return m_EventID; }
  const FetchDrawcall *CurSelectedDrawcall() { return GetDrawcall(CurSelectedEvent()); }
  const FetchDrawcall *CurDrawcall() { return GetDrawcall(CurEvent()); }
  const rdctype::array<FetchDrawcall> &CurDrawcalls() { return m_Drawcalls; }
  FetchTexture *GetTexture(ResourceId id) { return m_Textures[id]; }
  const rdctype::array<FetchTexture> &GetTextures() { return m_TextureList; }
  FetchBuffer *GetBuffer(ResourceId id) { return m_Buffers[id]; }
  const rdctype::array<FetchBuffer> &GetBuffers() { return m_BufferList; }
  QVector<DebugMessage> DebugMessages;
  int UnreadMessageCount;
  void AddMessages(const rdctype::array<DebugMessage> &msgs)
  {
    UnreadMessageCount += msgs.count;
    for(const DebugMessage &msg : msgs)
      DebugMessages.push_back(msg);
  }

  const FetchDrawcall *GetDrawcall(uint32_t eventID) { return GetDrawcall(m_Drawcalls, eventID); }
  WindowingSystem m_CurWinSystem;
  void *FillWindowingData(WId widget);

  const QIcon &winIcon() { return *m_Icon; }
  MainWindow *mainWindow() { return m_MainWindow; }
  EventBrowser *eventBrowser();
  APIInspector *apiInspector();
  TextureViewer *textureViewer();
  BufferViewer *meshPreview();
  PipelineStateViewer *pipelineViewer();
  CaptureDialog *captureDialog();

  bool hasEventBrowser() { return m_EventBrowser != NULL; }
  bool hasAPIInspector() { return m_APIInspector != NULL; }
  bool hasTextureViewer() { return m_TextureViewer != NULL; }
  bool hasPipelineViewer() { return m_PipelineViewer != NULL; }
  bool hasMeshPreview() { return m_MeshPreview != NULL; }
  bool hasCaptureDialog() { return m_CaptureDialog != NULL; }
  void showEventBrowser();
  void showAPIInspector();
  void showTextureViewer();
  void showMeshPreview();
  void showPipelineViewer();
  void showCaptureDialog();

  QWidget *createToolWindow(const QString &objectName);
  void windowClosed(QWidget *window);

  D3D11PipelineState CurD3D11PipelineState;
  D3D12PipelineState CurD3D12PipelineState;
  GLPipelineState CurGLPipelineState;
  VulkanPipelineState CurVulkanPipelineState;
  CommonPipelineState CurPipelineState;

  PersistantConfig &Config;

private:
  RenderManager m_Renderer;

  QVector<ILogViewerForm *> m_LogViewers;

  bool m_LogLoaded, m_LoadInProgress, m_LogLocal;
  QString m_LogFile;

  void LoadLogfileThreaded(const QString &logFile, const QString &origFilename, bool temporary,
                           bool local);

  uint32_t m_SelectedEventID;
  uint32_t m_EventID;

  const FetchDrawcall *GetDrawcall(const rdctype::array<FetchDrawcall> &draws, uint32_t eventID)
  {
    for(const FetchDrawcall &d : draws)
    {
      if(!d.children.empty())
      {
        const FetchDrawcall *draw = GetDrawcall(d.children, eventID);
        if(draw != NULL)
          return draw;
      }

      if(d.eventID == eventID)
        return &d;
    }

    return NULL;
  }

  rdctype::array<FetchDrawcall> m_Drawcalls;

  APIProperties m_APIProps;
  FetchFrameInfo m_FrameInfo;

  QMap<ResourceId, FetchTexture *> m_Textures;
  rdctype::array<FetchTexture> m_TextureList;
  QMap<ResourceId, FetchBuffer *> m_Buffers;
  rdctype::array<FetchBuffer> m_BufferList;

  rdctype::array<WindowingSystem> m_WinSystems;

#if defined(RENDERDOC_PLATFORM_LINUX)
  xcb_connection_t *m_XCBConnection;
  Display *m_X11Display;
#endif

  QIcon *m_Icon;

  // Windows
  QProgressDialog *m_Progress;
  MainWindow *m_MainWindow = NULL;
  EventBrowser *m_EventBrowser = NULL;
  APIInspector *m_APIInspector = NULL;
  TextureViewer *m_TextureViewer = NULL;
  BufferViewer *m_MeshPreview = NULL;
  PipelineStateViewer *m_PipelineViewer = NULL;
  CaptureDialog *m_CaptureDialog = NULL;
};
