/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
#include "common/formatting.h"
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

  m_pController = parent;

  m_MainOutput.dirty = true;

  m_CustomDirty = false;
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

  m_pController->FatalErrorCheck();
  m_pDevice = parent->GetDevice();

  m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);

  m_CustomShaderResourceId = ResourceId();

  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(ReplayController));
}

ReplayOutput::~ReplayOutput()
{
  CHECK_REPLAY_THREAD();

  m_CustomShaderResourceId = ResourceId();

  m_pDevice->DestroyOutputWindow(m_MainOutput.outputID);
  m_pDevice->DestroyOutputWindow(m_PixelContext.outputID);

  ClearThumbnails();
}

void ReplayOutput::Shutdown()
{
  CHECK_REPLAY_THREAD();

  m_pController->ShutdownOutput(this);
}

void ReplayOutput::SetDimensions(int32_t width, int32_t height)
{
  CHECK_REPLAY_THREAD();

  m_pDevice->SetOutputWindowDimensions(m_MainOutput.outputID, width > 0 ? width : 1,
                                       height > 0 ? height : 1);
  m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);
  m_pController->FatalErrorCheck();
}

bytebuf ReplayOutput::ReadbackOutputTexture()
{
  CHECK_REPLAY_THREAD();

  bytebuf data;
  m_pDevice->GetOutputWindowData(m_MainOutput.outputID, data);
  m_pController->FatalErrorCheck();
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
  m_CustomDirty = true;
  m_RenderData.texDisplay = o;
  m_MainOutput.dirty = true;

  m_TextureDim = {0, 0};
  for(size_t t = 0; t < m_pController->m_Textures.size(); t++)
  {
    if(m_pController->m_Textures[t].resourceId == m_RenderData.texDisplay.resourceId)
    {
      m_TextureDim = {m_pController->m_Textures[t].width, m_pController->m_Textures[t].height};
    }
  }
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
  m_CustomDirty = true;
  m_MainOutput.dirty = true;

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    m_Thumbnails[i].dirty = true;

  RefreshOverlay();
}

void ReplayOutput::RefreshOverlay()
{
  CHECK_REPLAY_THREAD();

  ActionDescription *action = m_pController->GetActionByEID(m_EventID);

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

    if(action != NULL && (action->flags & ActionFlags::Drawcall))
    {
      m_pDevice->InitPostVSBuffers(action->eventId);
      m_pController->FatalErrorCheck();

      if(postVSWholePass && !passEvents.empty())
      {
        m_pDevice->InitPostVSBuffers(passEvents);
        m_pController->FatalErrorCheck();

        m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
        m_pController->FatalErrorCheck();
      }
    }
  }

  if(m_Type == ReplayOutputType::Texture && m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay)
  {
    ResourceId id = m_pDevice->GetLiveID(m_RenderData.texDisplay.resourceId);

    if(id != ResourceId() && action && m_pDevice->IsRenderOutput(id))
    {
      FloatVector f = m_RenderData.texDisplay.backgroundColor;

      m_OverlayResourceId =
          m_pDevice->RenderOverlay(id, f, m_RenderData.texDisplay.overlay, m_EventID, passEvents);
      m_pController->FatalErrorCheck();
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

  if(m_CustomDirty)
  {
    TextureDisplay texDisplay = m_RenderData.texDisplay;
    texDisplay.rawOutput = false;
    texDisplay.resourceId = m_pDevice->GetLiveID(texDisplay.resourceId);

    m_CustomShaderResourceId = m_pDevice->ApplyCustomShader(texDisplay);
    m_pController->FatalErrorCheck();

    m_CustomDirty = false;
  }

  return m_CustomShaderResourceId;
}

ResourceId ReplayOutput::GetDebugOverlayTexID()
{
  CHECK_REPLAY_THREAD();

  if(m_OverlayDirty)
  {
    m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
    m_pController->FatalErrorCheck();
    RefreshOverlay();
    m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
    m_pController->FatalErrorCheck();
  }

  return m_OverlayResourceId;
}

void ReplayOutput::ClearThumbnails()
{
  CHECK_REPLAY_THREAD();

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    m_pDevice->DestroyOutputWindow(m_Thumbnails[i].outputID);

  for(auto it = m_ThumbnailGenerators.begin(); it != m_ThumbnailGenerators.end(); ++it)
    m_pDevice->DestroyOutputWindow(it->second);

  m_Thumbnails.clear();
}

ResultDetails ReplayOutput::SetPixelContext(WindowingData window)
{
  CHECK_REPLAY_THREAD();

  m_PixelContext.outputID = m_pDevice->MakeOutputWindow(window, false);
  m_PixelContext.texture = ResourceId();
  m_PixelContext.depthMode = false;

  m_pController->FatalErrorCheck();

  if(m_PixelContext.outputID == 0)
  {
    RETURN_ERROR_RESULT(ResultCode::InternalError, "Window creation failed");
  }

  return RDResult();
}

bytebuf ReplayOutput::DrawThumbnail(int32_t width, int32_t height, ResourceId textureId,
                                    const Subresource &sub, CompType typeCast)
{
  bytebuf ret;

  width = RDCMAX(width, 1);
  height = RDCMAX(height, 1);

  uint64_t key = (uint64_t(width) << 32) | height;
  int idx = -1;
  uint64_t outputID = 0;
  for(int i = 0; i < m_ThumbnailGenerators.count(); i++)
  {
    if(m_ThumbnailGenerators[i].first == key)
    {
      idx = i;
      outputID = m_ThumbnailGenerators[i].second;
      break;
    }
  }

  if(idx < 0)
  {
    // resize oldest generator if we have hit the max
    if(m_ThumbnailGenerators.size() == MaxThumbnailGenerators)
    {
      outputID = m_ThumbnailGenerators.back().second;
      m_pDevice->SetOutputWindowDimensions(outputID, width, height);
      m_ThumbnailGenerators.pop_back();
    }
    else
    {
      outputID = m_pDevice->MakeOutputWindow(CreateHeadlessWindowingData(width, height), false);
    }
  }
  else
  {
    // remove the found one, so it can get inserted into the front
    m_ThumbnailGenerators.erase(idx);
  }
  // make this the most recent generator, so the oldest one will be kicked out
  m_ThumbnailGenerators.insert(0, {key, outputID});

  bool depthMode = false;

  for(size_t t = 0; t < m_pController->m_Textures.size(); t++)
  {
    if(m_pController->m_Textures[t].resourceId == textureId)
    {
      depthMode = (m_pController->m_Textures[t].creationFlags & TextureCategory::DepthTarget) ||
                  (m_pController->m_Textures[t].format.compType == CompType::Depth);
      break;
    }
  }

  if(textureId == ResourceId())
  {
    m_pDevice->BindOutputWindow(outputID, false);

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
    m_pController->FatalErrorCheck();

    m_pDevice->FlipOutputWindow(outputID);
    m_pController->FatalErrorCheck();
  }
  else
  {
    m_pDevice->BindOutputWindow(outputID, false);
    m_pDevice->ClearOutputWindowColor(outputID, FloatVector());

    TextureDisplay disp;

    disp.red = disp.green = disp.blue = true;
    disp.alpha = false;
    disp.hdrMultiplier = -1.0f;
    disp.linearDisplayAsGamma = true;
    disp.flipY = false;
    disp.subresource = sub;
    disp.subresource.sample = 0;
    disp.customShaderId = ResourceId();
    disp.resourceId = m_pDevice->GetLiveID(textureId);
    disp.typeCast = typeCast;
    disp.scale = -1.0f;
    disp.rangeMin = 0.0f;
    disp.rangeMax = 1.0f;
    disp.xOffset = 0.0f;
    disp.yOffset = 0.0f;
    disp.rawOutput = false;
    disp.overlay = DebugOverlay::NoOverlay;

    if(typeCast == CompType::SNorm)
      disp.rangeMin = -1.0f;

    if(depthMode)
      disp.green = disp.blue = false;

    m_pDevice->RenderTexture(disp);
    m_pController->FatalErrorCheck();

    m_pDevice->FlipOutputWindow(outputID);
    m_pController->FatalErrorCheck();
  }

  m_pDevice->GetOutputWindowData(outputID, ret);
  m_pController->FatalErrorCheck();

  return ret;
}

ResultDetails ReplayOutput::AddThumbnail(WindowingData window, ResourceId texID,
                                         const Subresource &sub, CompType typeCast)
{
  CHECK_REPLAY_THREAD();

  OutputPair p;

  RDCASSERT(window.system != WindowingSystem::Unknown && window.system != WindowingSystem::Headless);

  bool depthMode = false;

  for(size_t t = 0; t < m_pController->m_Textures.size(); t++)
  {
    if(m_pController->m_Textures[t].resourceId == texID)
    {
      depthMode = (m_pController->m_Textures[t].creationFlags & TextureCategory::DepthTarget) ||
                  (m_pController->m_Textures[t].format.compType == CompType::Depth);
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

      return RDResult();
    }
  }

  p.wndHandle = GetHandle(window);
  p.outputID = m_pDevice->MakeOutputWindow(window, false);
  p.texture = texID;
  p.depthMode = depthMode;
  p.sub = sub;
  p.typeCast = typeCast;
  p.dirty = true;

  m_pController->FatalErrorCheck();

  m_Thumbnails.push_back(p);

  if(p.outputID == 0)
  {
    RETURN_ERROR_RESULT(ResultCode::InternalError, "Window creation failed");
  }

  return RDResult();
}

rdcpair<uint32_t, uint32_t> ReplayOutput::PickVertex(uint32_t x, uint32_t y)
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  ActionDescription *action = m_pController->GetActionByEID(m_EventID);

  const rdcpair<uint32_t, uint32_t> errorReturn = {~0U, ~0U};

  if(!action)
    return errorReturn;
  if(m_RenderData.meshDisplay.type == MeshDataStage::Unknown)
    return errorReturn;
  if(!(action->flags & ActionFlags::Drawcall))
    return errorReturn;

  MeshDisplay cfg = m_RenderData.meshDisplay;

  if(cfg.position.vertexResourceId == ResourceId() || cfg.position.numIndices == 0)
    return errorReturn;

  cfg.position.vertexResourceId = m_pDevice->GetLiveID(cfg.position.vertexResourceId);
  cfg.position.indexResourceId = m_pDevice->GetLiveID(cfg.position.indexResourceId);
  cfg.second.vertexResourceId = m_pDevice->GetLiveID(cfg.second.vertexResourceId);
  cfg.second.indexResourceId = m_pDevice->GetLiveID(cfg.second.indexResourceId);

  // input data either doesn't vary with instance, or is trivial (all verts the same for that
  // element), so only care about fetching the right instance for post-VS stages
  if((action->flags & ActionFlags::Instanced) && m_RenderData.meshDisplay.type != MeshDataStage::VSIn)
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
      maxInst = RDCMAX(1U, action->numInstances);
    }

    // used for post-VS output, calculate the offset of the element we're using as position,
    // relative to 0
    MeshFormat fmt =
        m_pDevice->GetPostVSBuffers(action->eventId, m_RenderData.meshDisplay.curInstance,
                                    m_RenderData.meshDisplay.curView, m_RenderData.meshDisplay.type);

    m_pController->FatalErrorCheck();

    uint64_t elemOffset = cfg.position.vertexByteOffset - fmt.vertexByteOffset;

    for(uint32_t inst = firstInst; inst < maxInst; inst++)
    {
      // find the start of this buffer, and apply the element offset, then pick in that instance
      fmt = m_pDevice->GetPostVSBuffers(action->eventId, inst, m_RenderData.meshDisplay.curView,
                                        m_RenderData.meshDisplay.type);
      m_pController->FatalErrorCheck();

      if(fmt.vertexResourceId != ResourceId())
        cfg.position.vertexByteOffset = fmt.vertexByteOffset + elemOffset;

      uint32_t vert = m_pDevice->PickVertex(m_EventID, m_Width, m_Height, cfg, x, y);
      m_pController->FatalErrorCheck();

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
    m_pController->FatalErrorCheck();

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

  m_pController->FatalErrorCheck();
}

void ReplayOutput::DisplayContext()
{
  CHECK_REPLAY_THREAD();

  if(m_PixelContext.outputID == 0)
    return;

  m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);
  m_pController->FatalErrorCheck();

  ClearBackground(m_PixelContext.outputID, m_RenderData.texDisplay.backgroundColor);

  if((m_Type != ReplayOutputType::Texture) || (m_ContextX < 0.0f && m_ContextY < 0.0f) ||
     (m_RenderData.texDisplay.resourceId == ResourceId()))
  {
    m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
    m_pController->FatalErrorCheck();
    return;
  }

  TextureDisplay disp = m_RenderData.texDisplay;
  disp.rawOutput = false;
  disp.customShaderId = ResourceId();

  if(m_RenderData.texDisplay.customShaderId != ResourceId())
  {
    disp.resourceId = m_CustomShaderResourceId;

    disp.typeCast = CompType::Typeless;
    disp.customShaderId = ResourceId();
    disp.subresource.slice = 0;
  }

  if((m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawPass ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizeDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass) &&
     m_OverlayResourceId != ResourceId())
  {
    disp.resourceId = m_OverlayResourceId;
    disp.typeCast = CompType::Typeless;
    disp.red = disp.green = disp.blue = disp.alpha = true;
    disp.rawOutput = false;
    disp.customShaderId = ResourceId();
    disp.hdrMultiplier = -1.0f;
    disp.rangeMin = 0.0f;
    disp.rangeMax = 1.0f;
    disp.linearDisplayAsGamma = false;
  }

  const float contextZoom = 8.0f;

  disp.scale = contextZoom / float(1 << disp.subresource.mip);

  int32_t width = 0, height = 0;
  m_pDevice->GetOutputWindowDimensions(m_PixelContext.outputID, width, height);

  float w = (float)width;
  float h = (float)height;

  int x = (int)m_ContextX;
  int y = (int)m_ContextY;

  if(m_TextureDim.first > 0 && m_TextureDim.second > 0)
  {
    rdcpair<uint32_t, uint32_t> mipDim = {RDCMAX(1U, m_TextureDim.first >> disp.subresource.mip),
                                          RDCMAX(1U, m_TextureDim.second >> disp.subresource.mip)};

    x = int((float(x) / float(m_TextureDim.first) + 1e-6f) * mipDim.first);
    x = int((float(x) / float(mipDim.first) + 1e-6f) * m_TextureDim.first);

    y = int((float(y) / float(m_TextureDim.second) + 1e-6f) * mipDim.second);
    y = int((float(y) / float(mipDim.second) + 1e-6f) * m_TextureDim.second);
  }
  else
  {
    x >>= disp.subresource.mip;
    x <<= disp.subresource.mip;

    y >>= disp.subresource.mip;
    y <<= disp.subresource.mip;
  }

  disp.xOffset = -(float)x * disp.scale;
  disp.yOffset = -(float)y * disp.scale;

  disp.xOffset += w / 2.0f;
  disp.yOffset += h / 2.0f;

  disp.resourceId = m_pDevice->GetLiveID(disp.resourceId);

  m_pDevice->RenderTexture(disp);
  m_pController->FatalErrorCheck();

  m_pDevice->RenderHighlightBox(w, h, contextZoom);
  m_pController->FatalErrorCheck();

  m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
  m_pController->FatalErrorCheck();
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

  m_pController->FatalErrorCheck();

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
    m_pController->FatalErrorCheck();

    DisplayContext();
  }
  else
  {
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->FlipOutputWindow(m_MainOutput.outputID);
    m_pController->FatalErrorCheck();
    m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);
    m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
    m_pController->FatalErrorCheck();
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
      m_pController->FatalErrorCheck();

      m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
      m_pController->FatalErrorCheck();
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
    disp.subresource.sample = 0;
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
    m_pController->FatalErrorCheck();

    m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
    m_pController->FatalErrorCheck();

    m_Thumbnails[i].dirty = false;
  }
}

void ReplayOutput::DisplayTex()
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  ActionDescription *action = m_pController->GetActionByEID(m_EventID);

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

  if(m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay && action)
  {
    if(m_OverlayDirty)
    {
      m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
      m_pController->FatalErrorCheck();
      RefreshOverlay();
      m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
      m_pController->FatalErrorCheck();
    }
  }
  else if(m_ForceOverlayRefresh)
  {
    m_ForceOverlayRefresh = false;
    m_pDevice->ReplayLog(m_EventID, eReplay_Full);
    m_pController->FatalErrorCheck();
  }

  if(m_RenderData.texDisplay.customShaderId != ResourceId())
  {
    m_CustomShaderResourceId = m_pDevice->ApplyCustomShader(texDisplay);
    m_pController->FatalErrorCheck();

    texDisplay.resourceId = m_pDevice->GetLiveID(m_CustomShaderResourceId);
    texDisplay.typeCast = CompType::Typeless;
    texDisplay.customShaderId = ResourceId();
    texDisplay.subresource.slice = 0;

    m_CustomDirty = false;
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
  m_pController->FatalErrorCheck();

  ResourceId id = m_pDevice->GetLiveID(m_RenderData.texDisplay.resourceId);

  if(m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay && action &&
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
    m_pController->FatalErrorCheck();
  }
}

void ReplayOutput::DisplayMesh()
{
  CHECK_REPLAY_THREAD();

  RENDERDOC_PROFILEFUNCTION();

  ActionDescription *action = m_pController->GetActionByEID(m_EventID);

  if(action == NULL || m_MainOutput.outputID == 0 || m_Width <= 0 || m_Height <= 0 ||
     (m_RenderData.meshDisplay.type == MeshDataStage::Unknown) ||
     !(action->flags & ActionFlags::Drawcall))
  {
    FloatVector color;
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);
    m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);
    m_pDevice->RenderCheckerboard(RenderDoc::Inst().DarkCheckerboardColor(),
                                  RenderDoc::Inst().LightCheckerboardColor());
    m_pController->FatalErrorCheck();

    return;
  }

  if(m_OverlayDirty)
  {
    m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
    m_pController->FatalErrorCheck();
    RefreshOverlay();
    m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
    m_pController->FatalErrorCheck();
  }

  m_pDevice->BindOutputWindow(m_MainOutput.outputID, true);
  m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

  m_pDevice->RenderCheckerboard(RenderDoc::Inst().DarkCheckerboardColor(),
                                RenderDoc::Inst().LightCheckerboardColor());
  m_pController->FatalErrorCheck();

  m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);
  m_pController->FatalErrorCheck();

  MeshDisplay mesh = m_RenderData.meshDisplay;
  mesh.position.vertexResourceId = m_pDevice->GetLiveID(mesh.position.vertexResourceId);
  mesh.position.indexResourceId = m_pDevice->GetLiveID(mesh.position.indexResourceId);
  mesh.second.vertexResourceId = m_pDevice->GetLiveID(mesh.second.vertexResourceId);
  mesh.second.indexResourceId = m_pDevice->GetLiveID(mesh.second.indexResourceId);

  rdcarray<MeshFormat> secondaryDraws;

  // we choose a pallette here so that the colours stay consistent (i.e the
  // current action is always the same colour), but also to indicate somewhat
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
      ActionDescription *d = m_pController->GetActionByEID(passEvents[i]);

      if(d)
      {
        for(uint32_t inst = 0; inst < RDCMAX(1U, d->numInstances); inst++)
        {
          // get the 'most final' stage
          MeshFormat fmt = m_pDevice->GetPostVSBuffers(
              passEvents[i], inst, m_RenderData.meshDisplay.curView, MeshDataStage::GSOut);
          m_pController->FatalErrorCheck();
          if(fmt.vertexResourceId == ResourceId())
          {
            fmt = m_pDevice->GetPostVSBuffers(passEvents[i], inst, m_RenderData.meshDisplay.curView,
                                              MeshDataStage::VSOut);
            m_pController->FatalErrorCheck();
          }

          fmt.meshColor = passDraws;

          // if unproject is marked, this output had a 'real' system position output
          if(fmt.unproject)
            secondaryDraws.push_back(fmt);
        }
      }
    }

    // action previous instances in the current action
    if(action->flags & ActionFlags::Instanced)
    {
      uint32_t maxInst = 0;
      if(m_RenderData.meshDisplay.showPrevInstances)
        maxInst = RDCMAX(1U, m_RenderData.meshDisplay.curInstance);
      if(m_RenderData.meshDisplay.showAllInstances)
        maxInst = RDCMAX(1U, action->numInstances);

      for(uint32_t inst = 0; inst < maxInst; inst++)
      {
        // get the 'most final' stage
        MeshFormat fmt = m_pDevice->GetPostVSBuffers(
            action->eventId, inst, m_RenderData.meshDisplay.curView, MeshDataStage::GSOut);
        m_pController->FatalErrorCheck();
        if(fmt.vertexResourceId == ResourceId())
        {
          fmt = m_pDevice->GetPostVSBuffers(action->eventId, inst, m_RenderData.meshDisplay.curView,
                                            MeshDataStage::VSOut);
          m_pController->FatalErrorCheck();
        }

        fmt.meshColor = otherInstances;

        // if unproject is marked, this output had a 'real' system position output
        if(fmt.unproject)
          secondaryDraws.push_back(fmt);
      }
    }
  }

  mesh.position.meshColor = drawItself;

  m_pDevice->RenderMesh(m_EventID, secondaryDraws, mesh);
  m_pController->FatalErrorCheck();
}
