/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "common/common.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "replay_controller.h"

static uint64_t GetHandle(WindowingData window)
{
#if ENABLED(RDOC_LINUX)

  if(window.system == WindowingSystem::Xlib)
  {
#if ENABLED(RDOC_XLIB)
    return (uint64_t)window.xlib.window;
#else
    RDCERR("Xlib windowing system data passed in, but support is not compiled in");
#endif
  }

  if(window.system == WindowingSystem::XCB)
  {
#if ENABLED(RDOC_XCB)
    return (uint64_t)window.xcb.window;
#else
    RDCERR("XCB windowing system data passed in, but support is not compiled in");
#endif
  }

  if(window.system == WindowingSystem::Wayland)
  {
#if ENABLED(RDOC_WAYLAND)
    return (uint64_t)window.wayland.window;
#else
    RDCERR("Wayland windowing system data passed in, but support is not compiled in");
#endif
  }

  RDCERR("Unrecognised window system %s", ToStr(window.system).c_str());

  return 0;

#elif ENABLED(RDOC_WIN32)

  RDCASSERT(window.system == WindowingSystem::Win32);
  return (uint64_t)window.win32.window;    // HWND

#elif ENABLED(RDOC_ANDROID)

  RDCASSERT(window.system == WindowingSystem::Android);
  return (uint64_t)window.android.window;    // ANativeWindow *

#elif ENABLED(RDOC_APPLE)

  RDCASSERT(window.system == WindowingSystem::MacOS);
  return (uint64_t)window.macOS.view;    // NSView *

#else
  RDCFATAL("No windowing data defined for this platform! Must be implemented for replay outputs");
#endif
}

ReplayOutput::ReplayOutput(ReplayController *parent, WindowingData window, ReplayOutputType type)
{
  m_ThreadID = Threading::GetCurrentID();

  m_pRenderer = parent;

  m_MainOutput.dirty = true;

  m_OverlayDirty = false;
  m_ForceOverlayRefresh = false;

  m_pDevice = parent->GetDevice();

  m_EventID = parent->m_EventID;

  m_OverlayResourceId = ResourceId();

  RDCEraseEl(m_RenderData);

  m_PixelContext.outputID = 0;
  m_PixelContext.texture = ResourceId();
  m_PixelContext.depthMode = false;

  m_ContextX = -1.0f;
  m_ContextY = -1.0f;

  m_Type = type;

  if(window.system != WindowingSystem::Unknown)
    m_MainOutput.outputID = m_pDevice->MakeOutputWindow(window, type == ReplayOutputType::Mesh);
  else
    m_MainOutput.outputID = 0;
  m_MainOutput.texture = ResourceId();

  m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);

  m_CustomShaderResourceId = ResourceId();

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(ReplayController));
}

ReplayOutput::~ReplayOutput()
{
  CHECK_REPLAY_THREAD();

  m_pDevice->DestroyOutputWindow(m_MainOutput.outputID);
  m_pDevice->DestroyOutputWindow(m_PixelContext.outputID);

  m_CustomShaderResourceId = ResourceId();

  ClearThumbnails();
}

void ReplayOutput::Shutdown()
{
  CHECK_REPLAY_THREAD();

  m_pRenderer->ShutdownOutput(this);
}

void ReplayOutput::SetDimensions(int32_t width, int32_t height)
{
  CHECK_REPLAY_THREAD();

  m_pDevice->SetOutputWindowDimensions(m_MainOutput.outputID, width > 0 ? width : 1,
                                       height > 0 ? height : 1);
  m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);
}

bytebuf ReplayOutput::ReadbackOutputTexture()
{
  CHECK_REPLAY_THREAD();

  bytebuf data;
  m_pDevice->GetOutputWindowData(m_MainOutput.outputID, data);
  return data;
}

rdcpair<int32_t, int32_t> ReplayOutput::GetDimensions()
{
  CHECK_REPLAY_THREAD();

  return make_rdcpair(m_Width, m_Height);
}

void ReplayOutput::SetTextureDisplay(const TextureDisplay &o)
{
  CHECK_REPLAY_THREAD();

  bool wasClearBeforeDraw = (m_RenderData.texDisplay.overlay == DebugOverlay::ClearBeforeDraw ||
                             m_RenderData.texDisplay.overlay == DebugOverlay::ClearBeforePass);

  if(o.overlay != m_RenderData.texDisplay.overlay ||
     (o.overlay != DebugOverlay::NoOverlay && (o.subresource != m_RenderData.texDisplay.subresource ||
                                               o.typeCast != m_RenderData.texDisplay.typeCast ||
                                               o.resourceId != m_RenderData.texDisplay.resourceId)))
  {
    if(wasClearBeforeDraw)
    {
      // by necessity these overlays modify the actual texture, not an
      // independent overlay texture. So if we disable them, we must
      // refresh the log.
      m_ForceOverlayRefresh = true;
    }
    m_OverlayDirty = true;
  }
  if(wasClearBeforeDraw && o.backgroundColor != m_RenderData.texDisplay.backgroundColor)
    m_OverlayDirty = true;
  m_RenderData.texDisplay = o;
  m_MainOutput.dirty = true;
}

void ReplayOutput::SetMeshDisplay(const MeshDisplay &o)
{
  CHECK_REPLAY_THREAD();

  if(o.showWholePass != m_RenderData.meshDisplay.showWholePass)
    m_OverlayDirty = true;
  m_RenderData.meshDisplay = o;
  m_MainOutput.dirty = true;
}

void ReplayOutput::SetFrameEvent(int eventId)
{
  CHECK_REPLAY_THREAD();

  m_EventID = eventId;

  m_OverlayDirty = (m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay);
  m_MainOutput.dirty = true;

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    m_Thumbnails[i].dirty = true;

  RefreshOverlay();
}

void ReplayOutput::RefreshOverlay()
{
  CHECK_REPLAY_THREAD();

  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(m_EventID);

  passEvents = m_pDevice->GetPassEvents(m_EventID);

  bool postVSBuffers = false;
  bool postVSWholePass = false;

  if(m_Type == ReplayOutputType::Mesh && m_OverlayDirty)
  {
    postVSBuffers = true;
    postVSWholePass = m_RenderData.meshDisplay.showWholePass != 0;
  }

  if(m_Type == ReplayOutputType::Texture)
  {
    postVSBuffers = m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass ||
                    m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizeDraw;
    postVSWholePass = m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass;
  }

  if(postVSBuffers)
  {
    if(m_Type == ReplayOutputType::Mesh)
      m_OverlayDirty = false;

    if(draw != NULL && (draw->flags & DrawFlags::Drawcall))
    {
      m_pDevice->InitPostVSBuffers(draw->eventId);

      if(postVSWholePass && !passEvents.empty())
      {
        m_pDevice->InitPostVSBuffers(passEvents);

        m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
      }
    }
  }

  if(m_Type == ReplayOutputType::Texture && m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay)
  {
    ResourceId id = m_pDevice->GetLiveID(m_RenderData.texDisplay.resourceId);

    if(draw && m_pDevice->IsRenderOutput(id))
    {
      FloatVector f = m_RenderData.texDisplay.backgroundColor;

      m_OverlayResourceId =
          m_pDevice->RenderOverlay(id, f, m_RenderData.texDisplay.overlay, m_EventID, passEvents);
      m_OverlayDirty = false;
    }
    else
    {
      m_OverlayResourceId = ResourceId();
    }
  }
  else
  {
    m_OverlayDirty = false;
  }
}

ResourceId ReplayOutput::GetCustomShaderTexID()
{
  CHECK_REPLAY_THREAD();

  return m_CustomShaderResourceId;
}

ResourceId ReplayOutput::GetDebugOverlayTexID()
{
  CHECK_REPLAY_THREAD();

  if(m_OverlayDirty)
  {
    m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
    RefreshOverlay();
    m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
  }

  return m_OverlayResourceId;
}

void ReplayOutput::ClearThumbnails()
{
  CHECK_REPLAY_THREAD();

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    m_pDevice->DestroyOutputWindow(m_Thumbnails[i].outputID);

  m_Thumbnails.clear();
}

bool ReplayOutput::SetPixelContext(WindowingData window)
{
  CHECK_REPLAY_THREAD();

  m_PixelContext.outputID = m_pDevice->MakeOutputWindow(window, false);
  m_PixelContext.texture = ResourceId();
  m_PixelContext.depthMode = false;

  RDCASSERT(m_PixelContext.outputID > 0);

  return m_PixelContext.outputID != 0;
}

bool ReplayOutput::AddThumbnail(WindowingData window, ResourceId texID, const Subresource &sub,
                                CompType typeCast)
{
  CHECK_REPLAY_THREAD();

  OutputPair p;

  RDCASSERT(window.system != WindowingSystem::Unknown && window.system != WindowingSystem::Headless);

  bool depthMode = false;

  for(size_t t = 0; t < m_pRenderer->m_Textures.size(); t++)
  {
    if(m_pRenderer->m_Textures[t].resourceId == texID)
    {
      depthMode = (m_pRenderer->m_Textures[t].creationFlags & TextureCategory::DepthTarget) ||
                  (m_pRenderer->m_Textures[t].format.compType == CompType::Depth);
      break;
    }
  }

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
  {
    if(m_Thumbnails[i].wndHandle == GetHandle(window))
    {
      m_Thumbnails[i].texture = texID;
      m_Thumbnails[i].depthMode = depthMode;
      m_Thumbnails[i].sub = sub;
      m_Thumbnails[i].typeCast = typeCast;
      m_Thumbnails[i].dirty = true;

      return true;
    }
  }

  p.wndHandle = GetHandle(window);
  p.outputID = m_pDevice->MakeOutputWindow(window, false);
  p.texture = texID;
  p.depthMode = depthMode;
  p.sub = sub;
  p.typeCast = typeCast;
  p.dirty = true;

  RDCASSERT(p.outputID > 0);

  m_Thumbnails.push_back(p);

  return true;
}

rdcpair<uint32_t, uint32_t> ReplayOutput::PickVertex(uint32_t x, uint32_t y)
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(m_EventID);

  const rdcpair<uint32_t, uint32_t> errorReturn = {~0U, ~0U};

  if(!draw)
    return errorReturn;
  if(m_RenderData.meshDisplay.type == MeshDataStage::Unknown)
    return errorReturn;
  if(!(draw->flags & DrawFlags::Drawcall))
    return errorReturn;

  MeshDisplay cfg = m_RenderData.meshDisplay;

  if(cfg.position.vertexResourceId == ResourceId())
    return errorReturn;

  cfg.position.vertexResourceId = m_pDevice->GetLiveID(cfg.position.vertexResourceId);
  cfg.position.indexResourceId = m_pDevice->GetLiveID(cfg.position.indexResourceId);
  cfg.second.vertexResourceId = m_pDevice->GetLiveID(cfg.second.vertexResourceId);
  cfg.second.indexResourceId = m_pDevice->GetLiveID(cfg.second.indexResourceId);

  // input data either doesn't vary with instance, or is trivial (all verts the same for that
  // element), so only care about fetching the right instance for post-VS stages
  if((draw->flags & DrawFlags::Instanced) && m_RenderData.meshDisplay.type != MeshDataStage::VSIn)
  {
    // if no special options are enabled, just look at the current instance
    uint32_t firstInst = m_RenderData.meshDisplay.curInstance;
    uint32_t maxInst = m_RenderData.meshDisplay.curInstance + 1;

    if(m_RenderData.meshDisplay.showPrevInstances)
    {
      firstInst = 0;
    }

    if(m_RenderData.meshDisplay.showAllInstances)
    {
      firstInst = 0;
      maxInst = RDCMAX(1U, draw->numInstances);
    }

    // used for post-VS output, calculate the offset of the element we're using as position,
    // relative to 0
    MeshFormat fmt =
        m_pDevice->GetPostVSBuffers(draw->eventId, m_RenderData.meshDisplay.curInstance,
                                    m_RenderData.meshDisplay.curView, m_RenderData.meshDisplay.type);
    uint64_t elemOffset = cfg.position.vertexByteOffset - fmt.vertexByteOffset;

    for(uint32_t inst = firstInst; inst < maxInst; inst++)
    {
      // find the start of this buffer, and apply the element offset, then pick in that instance
      fmt = m_pDevice->GetPostVSBuffers(draw->eventId, inst, m_RenderData.meshDisplay.curView,
                                        m_RenderData.meshDisplay.type);
      if(fmt.vertexResourceId != ResourceId())
        cfg.position.vertexByteOffset = fmt.vertexByteOffset + elemOffset;

      uint32_t vert = m_pDevice->PickVertex(m_EventID, m_Width, m_Height, cfg, x, y);
      if(vert != ~0U)
      {
        return make_rdcpair(vert, inst);
      }
    }

    return errorReturn;
  }
  else
  {
    uint32_t vert = m_pDevice->PickVertex(m_EventID, m_Width, m_Height, cfg, x, y);

    if(vert != ~0U)
      return make_rdcpair(vert, m_RenderData.meshDisplay.curInstance);

    return errorReturn;
  }
}

void ReplayOutput::SetPixelContextLocation(uint32_t x, uint32_t y)
{
  CHECK_REPLAY_THREAD();

  m_ContextX = RDCMAX((float)x, 0.0f);
  m_ContextY = RDCMAX((float)y, 0.0f);

  DisplayContext();
}

void ReplayOutput::DisablePixelContext()
{
  CHECK_REPLAY_THREAD();

  m_ContextX = -1.0f;
  m_ContextY = -1.0f;

  DisplayContext();
}

void ReplayOutput::ClearBackground(uint64_t outputID, const FloatVector &backgroundColor)
{
  CHECK_REPLAY_THREAD();

  if(backgroundColor.x == 0.0f && backgroundColor.y == 0.0f && backgroundColor.z == 0.0f &&
     backgroundColor.w == 0.0f)
    m_pDevice->RenderCheckerboard(RenderDoc::Inst().DarkCheckerboardColor(),
                                  RenderDoc::Inst().LightCheckerboardColor());
  else
    m_pDevice->ClearOutputWindowColor(outputID, ConvertSRGBToLinear(backgroundColor));
}

void ReplayOutput::DisplayContext()
{
  CHECK_REPLAY_THREAD();

  if(m_PixelContext.outputID == 0)
    return;
  m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);
  ClearBackground(m_PixelContext.outputID, m_RenderData.texDisplay.backgroundColor);

  if((m_Type != ReplayOutputType::Texture) || (m_ContextX < 0.0f && m_ContextY < 0.0f) ||
     (m_RenderData.texDisplay.resourceId == ResourceId()))
  {
    m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
    return;
  }

  TextureDisplay disp = m_RenderData.texDisplay;
  disp.rawOutput = false;
  disp.customShaderId = ResourceId();

  if(m_RenderData.texDisplay.customShaderId != ResourceId())
    disp.resourceId = m_CustomShaderResourceId;

  if((m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawPass ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizeDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass) &&
     m_OverlayResourceId != ResourceId())
    disp.resourceId = m_OverlayResourceId;

  const float contextZoom = 8.0f;

  disp.scale = contextZoom / float(1 << disp.subresource.mip);

  int32_t width = 0, height = 0;
  m_pDevice->GetOutputWindowDimensions(m_PixelContext.outputID, width, height);

  float w = (float)width;
  float h = (float)height;

  int x = (int)m_ContextX;
  int y = (int)m_ContextY;

  x >>= disp.subresource.mip;
  x <<= disp.subresource.mip;

  y >>= disp.subresource.mip;
  y <<= disp.subresource.mip;

  disp.xOffset = -(float)x * disp.scale;
  disp.yOffset = -(float)y * disp.scale;

  disp.xOffset += w / 2.0f;
  disp.yOffset += h / 2.0f;

  disp.resourceId = m_pDevice->GetLiveID(disp.resourceId);

  m_pDevice->RenderTexture(disp);

  m_pDevice->RenderHighlightBox(w, h, contextZoom);

  m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
}

void ReplayOutput::Display()
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  if(m_pDevice->CheckResizeOutputWindow(m_MainOutput.outputID))
  {
    m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);
    m_MainOutput.dirty = true;
  }

  if(m_pDevice->CheckResizeOutputWindow(m_PixelContext.outputID))
    m_MainOutput.dirty = true;

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    if(m_pDevice->CheckResizeOutputWindow(m_Thumbnails[i].outputID))
      m_Thumbnails[i].dirty = true;

  if(m_MainOutput.dirty)
  {
    m_MainOutput.dirty = false;

    switch(m_Type)
    {
      case ReplayOutputType::Mesh: DisplayMesh(); break;
      case ReplayOutputType::Texture: DisplayTex(); break;
      default: RDCERR("Unexpected display type! %d", m_Type); break;
    }

    m_pDevice->FlipOutputWindow(m_MainOutput.outputID);

    DisplayContext();
  }
  else
  {
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->FlipOutputWindow(m_MainOutput.outputID);
    m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);
    m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
  }

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
  {
    if(!m_Thumbnails[i].dirty)
    {
      m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);
      m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
      continue;
    }

    if(!m_pDevice->IsOutputWindowVisible(m_Thumbnails[i].outputID))
      continue;

    FloatVector color;

    if(m_Thumbnails[i].texture == ResourceId())
    {
      m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);

      FloatVector dark = RenderDoc::Inst().DarkCheckerboardColor();
      FloatVector light = RenderDoc::Inst().LightCheckerboardColor();

      FloatVector dark2;
      dark2.x = light.x;
      dark2.y = dark.y;
      dark2.z = dark.z;
      dark2.w = 1.0f;
      FloatVector light2;
      light2.x = dark.x;
      light2.y = light.y;
      light2.z = light.z;
      light2.w = 1.0f;
      m_pDevice->RenderCheckerboard(dark2, light2);

      m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
      continue;
    }

    m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);
    m_pDevice->ClearOutputWindowColor(m_Thumbnails[i].outputID, color);

    TextureDisplay disp;

    disp.red = disp.green = disp.blue = true;
    disp.alpha = false;
    disp.hdrMultiplier = -1.0f;
    disp.linearDisplayAsGamma = true;
    disp.flipY = false;
    disp.subresource = m_Thumbnails[i].sub;
    disp.subresource.sample = ~0U;
    disp.customShaderId = ResourceId();
    disp.resourceId = m_pDevice->GetLiveID(m_Thumbnails[i].texture);
    disp.typeCast = m_Thumbnails[i].typeCast;
    disp.scale = -1.0f;
    disp.rangeMin = 0.0f;
    disp.rangeMax = 1.0f;
    disp.xOffset = 0.0f;
    disp.yOffset = 0.0f;
    disp.rawOutput = false;
    disp.overlay = DebugOverlay::NoOverlay;

    if(m_Thumbnails[i].typeCast == CompType::SNorm)
      disp.rangeMin = -1.0f;

    if(m_Thumbnails[i].depthMode)
      disp.green = disp.blue = false;

    m_pDevice->RenderTexture(disp);

    m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);

    m_Thumbnails[i].dirty = false;
  }
}

void ReplayOutput::DisplayTex()
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(m_EventID);

  if(m_MainOutput.outputID == 0)
    return;
  if(m_RenderData.texDisplay.resourceId == ResourceId() || m_Width <= 0 || m_Height <= 0)
  {
    FloatVector color;
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);
    return;
  }

  TextureDisplay texDisplay = m_RenderData.texDisplay;
  texDisplay.rawOutput = false;
  texDisplay.resourceId = m_pDevice->GetLiveID(texDisplay.resourceId);

  if(m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay && draw)
  {
    if(m_OverlayDirty)
    {
      m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
      RefreshOverlay();
      m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
    }
  }
  else if(m_ForceOverlayRefresh)
  {
    m_ForceOverlayRefresh = false;
    m_pDevice->ReplayLog(m_EventID, eReplay_Full);
  }

  if(m_RenderData.texDisplay.customShaderId != ResourceId())
  {
    m_CustomShaderResourceId =
        m_pDevice->ApplyCustomShader(m_RenderData.texDisplay.customShaderId, texDisplay.resourceId,
                                     texDisplay.subresource, texDisplay.typeCast);

    texDisplay.resourceId = m_pDevice->GetLiveID(m_CustomShaderResourceId);
    texDisplay.typeCast = CompType::Typeless;
    texDisplay.customShaderId = ResourceId();
    texDisplay.subresource.slice = 0;
  }

  FloatVector color;

  m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
  m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);

  ClearBackground(m_MainOutput.outputID, texDisplay.backgroundColor);

  // of the overlay isn't one that's applied while rendering the base texture - NaN/inf/-ve or
  // clipping - then don't try and render any overlay. This prevents underlying code from trying to
  // e.g. decode overlay data as a heatmap for quad overdraw/triangle size overlays
  if(texDisplay.overlay != DebugOverlay::NaN && texDisplay.overlay != DebugOverlay::Clipping)
  {
    texDisplay.overlay = DebugOverlay::NoOverlay;
  }

  m_pDevice->RenderTexture(texDisplay);

  ResourceId id = m_pDevice->GetLiveID(m_RenderData.texDisplay.resourceId);

  if(m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay && draw &&
     m_pDevice->IsRenderOutput(id) && m_RenderData.texDisplay.overlay != DebugOverlay::NaN &&
     m_RenderData.texDisplay.overlay != DebugOverlay::Clipping && m_OverlayResourceId != ResourceId())
  {
    texDisplay.resourceId = m_pDevice->GetLiveID(m_OverlayResourceId);
    texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
    texDisplay.rawOutput = false;
    texDisplay.overlay = m_RenderData.texDisplay.overlay;
    texDisplay.customShaderId = ResourceId();
    texDisplay.scale = m_RenderData.texDisplay.scale;
    texDisplay.hdrMultiplier = -1.0f;
    texDisplay.flipY = m_RenderData.texDisplay.flipY;
    texDisplay.rangeMin = 0.0f;
    texDisplay.rangeMax = 1.0f;
    texDisplay.linearDisplayAsGamma = false;
    texDisplay.typeCast = CompType::Typeless;

    m_pDevice->RenderTexture(texDisplay);
  }
}

void ReplayOutput::DisplayMesh()
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(m_EventID);

  if(draw == NULL || m_MainOutput.outputID == 0 || m_Width <= 0 || m_Height <= 0 ||
     (m_RenderData.meshDisplay.type == MeshDataStage::Unknown) ||
     !(draw->flags & DrawFlags::Drawcall))
  {
    FloatVector color;
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);
    m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);
    m_pDevice->RenderCheckerboard(RenderDoc::Inst().DarkCheckerboardColor(),
                                  RenderDoc::Inst().LightCheckerboardColor());

    return;
  }

  if(m_OverlayDirty)
  {
    m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
    RefreshOverlay();
    m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
  }

  m_pDevice->BindOutputWindow(m_MainOutput.outputID, true);
  m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

  m_pDevice->RenderCheckerboard(RenderDoc::Inst().DarkCheckerboardColor(),
                                RenderDoc::Inst().LightCheckerboardColor());

  m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

  MeshDisplay mesh = m_RenderData.meshDisplay;
  mesh.position.vertexResourceId = m_pDevice->GetLiveID(mesh.position.vertexResourceId);
  mesh.position.indexResourceId = m_pDevice->GetLiveID(mesh.position.indexResourceId);
  mesh.second.vertexResourceId = m_pDevice->GetLiveID(mesh.second.vertexResourceId);
  mesh.second.indexResourceId = m_pDevice->GetLiveID(mesh.second.indexResourceId);

  rdcarray<MeshFormat> secondaryDraws;

  // we choose a pallette here so that the colours stay consistent (i.e the
  // current draw is always the same colour), but also to indicate somewhat
  // the relationship - ie. instances are closer in colour than other draws
  // in the pass

  // very slightly dark red
  FloatVector drawItself(0.06f, 0.0f, 0.0f, 1.0f);

  // more desaturated/lighter, but still reddish
  FloatVector otherInstances(0.18f, 0.1f, 0.1f, 1.0f);

  // lighter grey with blue tinge to contrast from main/instance draws
  FloatVector passDraws(0.2f, 0.2f, 0.25f, 1.0f);

  if(RenderDoc::Inst().IsDarkTheme())
  {
    drawItself = FloatVector(1.0f, 0.8f, 0.8f, 1.0f);
    otherInstances = FloatVector(0.78f, 0.6f, 0.6f, 1.0f);
    passDraws = FloatVector(0.4f, 0.4f, 0.45f, 1.0f);
  }

  if(m_RenderData.meshDisplay.type != MeshDataStage::VSIn)
  {
    for(size_t i = 0; m_RenderData.meshDisplay.showWholePass && i < passEvents.size(); i++)
    {
      DrawcallDescription *d = m_pRenderer->GetDrawcallByEID(passEvents[i]);

      if(d)
      {
        for(uint32_t inst = 0; inst < RDCMAX(1U, d->numInstances); inst++)
        {
          // get the 'most final' stage
          MeshFormat fmt = m_pDevice->GetPostVSBuffers(
              passEvents[i], inst, m_RenderData.meshDisplay.curView, MeshDataStage::GSOut);
          if(fmt.vertexResourceId == ResourceId())
            fmt = m_pDevice->GetPostVSBuffers(passEvents[i], inst, m_RenderData.meshDisplay.curView,
                                              MeshDataStage::VSOut);

          fmt.meshColor = passDraws;

          // if unproject is marked, this output had a 'real' system position output
          if(fmt.unproject)
            secondaryDraws.push_back(fmt);
        }
      }
    }

    // draw previous instances in the current drawcall
    if(draw->flags & DrawFlags::Instanced)
    {
      uint32_t maxInst = 0;
      if(m_RenderData.meshDisplay.showPrevInstances)
        maxInst = RDCMAX(1U, m_RenderData.meshDisplay.curInstance);
      if(m_RenderData.meshDisplay.showAllInstances)
        maxInst = RDCMAX(1U, draw->numInstances);

      for(uint32_t inst = 0; inst < maxInst; inst++)
      {
        // get the 'most final' stage
        MeshFormat fmt = m_pDevice->GetPostVSBuffers(
            draw->eventId, inst, m_RenderData.meshDisplay.curView, MeshDataStage::GSOut);
        if(fmt.vertexResourceId == ResourceId())
          fmt = m_pDevice->GetPostVSBuffers(draw->eventId, inst, m_RenderData.meshDisplay.curView,
                                            MeshDataStage::VSOut);

        fmt.meshColor = otherInstances;

        // if unproject is marked, this output had a 'real' system position output
        if(fmt.unproject)
          secondaryDraws.push_back(fmt);
      }
    }
  }

  mesh.position.meshColor = drawItself;

  m_pDevice->RenderMesh(m_EventID, secondaryDraws, mesh);
}
