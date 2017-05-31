/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "maths/matrix.h"
#include "serialise/string_utils.h"
#include "replay_controller.h"

static uint64_t GetHandle(WindowingSystem system, void *data)
{
#if ENABLED(RDOC_LINUX)

  if(system == WindowingSystem::Xlib)
  {
#if ENABLED(RDOC_XLIB)
    return (uint64_t)((XlibWindowData *)data)->window;
#else
    RDCERR("Xlib windowing system data passed in, but support is not compiled in");
#endif
  }

  if(system == WindowingSystem::XCB)
  {
#if ENABLED(RDOC_XCB)
    return (uint64_t)((XCBWindowData *)data)->window;
#else
    RDCERR("XCB windowing system data passed in, but support is not compiled in");
#endif
  }

  RDCERR("Unrecognised window system %d", system);

  return 0;

#elif ENABLED(RDOC_WIN32)

  RDCASSERT(system == WindowingSystem::Win32);
  return (uint64_t)data;    // HWND

#elif ENABLED(RDOC_ANDROID)

  RDCASSERT(system == WindowingSystem::Android);
  return (uint64_t)data;    // ANativeWindow *

#else
  RDCFATAL("No windowing data defined for this platform! Must be implemented for replay outputs");
#endif
}

ReplayOutput::ReplayOutput(ReplayController *parent, WindowingSystem system, void *data,
                           ReplayOutputType type)
{
  m_pRenderer = parent;

  m_MainOutput.dirty = true;

  m_OverlayDirty = true;
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

  if(system != WindowingSystem::Unknown)
    m_MainOutput.outputID = m_pDevice->MakeOutputWindow(system, data, type == ReplayOutputType::Mesh);
  else
    m_MainOutput.outputID = 0;
  m_MainOutput.texture = ResourceId();

  m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);

  m_CustomShaderResourceId = ResourceId();
}

ReplayOutput::~ReplayOutput()
{
  m_pDevice->DestroyOutputWindow(m_MainOutput.outputID);
  m_pDevice->DestroyOutputWindow(m_PixelContext.outputID);

  m_CustomShaderResourceId = ResourceId();

  ClearThumbnails();
}

void ReplayOutput::SetTextureDisplay(const TextureDisplay &o)
{
  if(o.overlay != m_RenderData.texDisplay.overlay)
  {
    if(m_RenderData.texDisplay.overlay == DebugOverlay::ClearBeforeDraw ||
       m_RenderData.texDisplay.overlay == DebugOverlay::ClearBeforePass)
    {
      // by necessity these overlays modify the actual texture, not an
      // independent overlay texture. So if we disable them, we must
      // refresh the log.
      m_ForceOverlayRefresh = true;
    }
    m_OverlayDirty = true;
  }
  m_RenderData.texDisplay = o;
  m_MainOutput.dirty = true;
}

void ReplayOutput::SetMeshDisplay(const MeshDisplay &o)
{
  if(o.showWholePass != m_RenderData.meshDisplay.showWholePass)
    m_OverlayDirty = true;
  m_RenderData.meshDisplay = o;
  m_MainOutput.dirty = true;
}

void ReplayOutput::SetFrameEvent(int eventID)
{
  m_EventID = eventID;

  m_OverlayDirty = true;
  m_MainOutput.dirty = true;

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    m_Thumbnails[i].dirty = true;

  RefreshOverlay();
}

void ReplayOutput::RefreshOverlay()
{
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
      m_pDevice->InitPostVSBuffers(draw->eventID);

      if(postVSWholePass && !passEvents.empty())
      {
        m_pDevice->InitPostVSBuffers(passEvents);

        m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
      }
    }
  }

  if(m_Type == ReplayOutputType::Texture && m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay)
  {
    if(draw && m_pDevice->IsRenderOutput(m_RenderData.texDisplay.texid))
    {
      m_OverlayResourceId = m_pDevice->RenderOverlay(
          m_pDevice->GetLiveID(m_RenderData.texDisplay.texid), m_RenderData.texDisplay.typeHint,
          m_RenderData.texDisplay.overlay, m_EventID, passEvents);
      m_OverlayDirty = false;
    }
    else
    {
      m_OverlayResourceId = ResourceId();
    }
  }
}

void ReplayOutput::ClearThumbnails()
{
  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    m_pDevice->DestroyOutputWindow(m_Thumbnails[i].outputID);

  m_Thumbnails.clear();
}

bool ReplayOutput::SetPixelContext(WindowingSystem system, void *data)
{
  m_PixelContext.outputID = m_pDevice->MakeOutputWindow(system, data, false);
  m_PixelContext.texture = ResourceId();
  m_PixelContext.depthMode = false;

  RDCASSERT(m_PixelContext.outputID > 0);

  return m_PixelContext.outputID != 0;
}

bool ReplayOutput::AddThumbnail(WindowingSystem system, void *data, ResourceId texID,
                                CompType typeHint)
{
  OutputPair p;

  RDCASSERT(data);

  bool depthMode = false;

  for(size_t t = 0; t < m_pRenderer->m_Textures.size(); t++)
  {
    if(m_pRenderer->m_Textures[t].ID == texID)
    {
      depthMode = (m_pRenderer->m_Textures[t].creationFlags & TextureCategory::DepthTarget) ||
                  (m_pRenderer->m_Textures[t].format.compType == CompType::Depth);
      break;
    }
  }

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
  {
    if(m_Thumbnails[i].wndHandle == GetHandle(system, data))
    {
      m_Thumbnails[i].texture = texID;

      m_Thumbnails[i].depthMode = depthMode;

      m_Thumbnails[i].typeHint = typeHint;

      m_Thumbnails[i].dirty = true;

      return true;
    }
  }

  p.wndHandle = GetHandle(system, data);
  p.outputID = m_pDevice->MakeOutputWindow(system, data, false);
  p.texture = texID;
  p.depthMode = depthMode;
  p.typeHint = typeHint;
  p.dirty = true;

  RDCASSERT(p.outputID > 0);

  m_Thumbnails.push_back(p);

  return true;
}

rdctype::pair<PixelValue, PixelValue> ReplayOutput::GetMinMax()
{
  PixelValue minval;
  PixelValue maxval;

  ResourceId tex = m_pDevice->GetLiveID(m_RenderData.texDisplay.texid);

  CompType typeHint = m_RenderData.texDisplay.typeHint;
  uint32_t slice = m_RenderData.texDisplay.sliceFace;
  uint32_t mip = m_RenderData.texDisplay.mip;
  uint32_t sample = m_RenderData.texDisplay.sampleIdx;

  if(m_RenderData.texDisplay.CustomShader != ResourceId() && m_CustomShaderResourceId != ResourceId())
  {
    tex = m_CustomShaderResourceId;
    typeHint = CompType::Typeless;
    slice = 0;
    sample = 0;
  }

  m_pDevice->GetMinMax(tex, slice, mip, sample, typeHint, &minval.value_f[0], &maxval.value_f[0]);

  return rdctype::make_pair(minval, maxval);
}

rdctype::array<uint32_t> ReplayOutput::GetHistogram(float minval, float maxval, bool channels[4])
{
  vector<uint32_t> hist;

  ResourceId tex = m_pDevice->GetLiveID(m_RenderData.texDisplay.texid);

  CompType typeHint = m_RenderData.texDisplay.typeHint;
  uint32_t slice = m_RenderData.texDisplay.sliceFace;
  uint32_t mip = m_RenderData.texDisplay.mip;
  uint32_t sample = m_RenderData.texDisplay.sampleIdx;

  if(m_RenderData.texDisplay.CustomShader != ResourceId() && m_CustomShaderResourceId != ResourceId())
  {
    tex = m_CustomShaderResourceId;
    typeHint = CompType::Typeless;
    slice = 0;
    sample = 0;
  }

  m_pDevice->GetHistogram(tex, slice, mip, sample, typeHint, minval, maxval, channels, hist);

  return hist;
}

PixelValue ReplayOutput::PickPixel(ResourceId tex, bool customShader, uint32_t x, uint32_t y,
                                   uint32_t sliceFace, uint32_t mip, uint32_t sample)
{
  PixelValue ret;

  RDCEraseEl(ret.value_f);

  if(tex == ResourceId())
    return ret;

  bool decodeRamp = false;

  CompType typeHint = m_RenderData.texDisplay.typeHint;

  if(customShader && m_RenderData.texDisplay.CustomShader != ResourceId() &&
     m_CustomShaderResourceId != ResourceId())
  {
    tex = m_CustomShaderResourceId;
    typeHint = CompType::Typeless;
  }
  if((m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawPass ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizeDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass) &&
     m_OverlayResourceId != ResourceId())
  {
    decodeRamp = true;
    tex = m_OverlayResourceId;
    typeHint = CompType::Typeless;
  }

  m_pDevice->PickPixel(m_pDevice->GetLiveID(tex), x, y, sliceFace, mip, sample, typeHint,
                       ret.value_f);

  if(decodeRamp)
  {
    for(size_t c = 0; c < ARRAY_COUNT(overdrawRamp); c++)
    {
      if(fabs(ret.value_f[0] - overdrawRamp[c].x) < 0.00005f &&
         fabs(ret.value_f[1] - overdrawRamp[c].y) < 0.00005f &&
         fabs(ret.value_f[2] - overdrawRamp[c].z) < 0.00005f)
      {
        ret.value_i[0] = (int32_t)c;
        ret.value_i[1] = 0;
        ret.value_i[2] = 0;
        ret.value_i[3] = 0;
        break;
      }
    }

    // decode back into approximate pixel size area
    if(m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass ||
       m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizeDraw)
    {
      float bucket = (float)ret.value_i[0];

      // decode bucket into approximate triangle area
      if(bucket <= 0.5f)
        ret.value_f[0] = 0.0f;
      else if(bucket < 2.0f)
        ret.value_f[0] = 16.0f;
      else
        ret.value_f[0] = -2.5f * logf(1.0f + (bucket - 22.0f) / 20.1f);
    }
  }

  return ret;
}

rdctype::pair<uint32_t, uint32_t> ReplayOutput::PickVertex(uint32_t eventID, uint32_t x, uint32_t y)
{
  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(eventID);

  const rdctype::pair<uint32_t, uint32_t> errorReturn = rdctype::make_pair(~0U, ~0U);

  if(!draw)
    return errorReturn;
  if(m_RenderData.meshDisplay.type == MeshDataStage::Unknown)
    return errorReturn;
  if(!(draw->flags & DrawFlags::Drawcall))
    return errorReturn;

  MeshDisplay cfg = m_RenderData.meshDisplay;

  if(cfg.position.buf == ResourceId())
    return errorReturn;

  cfg.position.buf = m_pDevice->GetLiveID(cfg.position.buf);
  cfg.position.idxbuf = m_pDevice->GetLiveID(cfg.position.idxbuf);
  cfg.second.buf = m_pDevice->GetLiveID(cfg.second.buf);
  cfg.second.idxbuf = m_pDevice->GetLiveID(cfg.second.idxbuf);

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
      maxInst = RDCMAX(1U, m_RenderData.meshDisplay.curInstance);
    }

    if(m_RenderData.meshDisplay.showAllInstances)
    {
      firstInst = 0;
      maxInst = RDCMAX(1U, draw->numInstances);
    }

    // used for post-VS output, calculate the offset of the element we're using as position,
    // relative to 0
    MeshFormat fmt = m_pDevice->GetPostVSBuffers(
        draw->eventID, m_RenderData.meshDisplay.curInstance, m_RenderData.meshDisplay.type);
    uint64_t elemOffset = cfg.position.offset - fmt.offset;

    for(uint32_t inst = firstInst; inst < maxInst; inst++)
    {
      // find the start of this buffer, and apply the element offset, then pick in that instance
      fmt = m_pDevice->GetPostVSBuffers(draw->eventID, inst, m_RenderData.meshDisplay.type);
      if(fmt.buf != ResourceId())
        cfg.position.offset = fmt.offset + elemOffset;

      uint32_t vert = m_pDevice->PickVertex(m_EventID, cfg, x, y);
      if(vert != ~0U)
      {
        return rdctype::make_pair(vert, inst);
      }
    }

    return errorReturn;
  }
  else
  {
    return rdctype::make_pair(m_pDevice->PickVertex(m_EventID, cfg, x, y), 0U);
  }
}

void ReplayOutput::SetPixelContextLocation(uint32_t x, uint32_t y)
{
  m_ContextX = RDCMAX((float)x, 0.0f);
  m_ContextY = RDCMAX((float)y, 0.0f);

  DisplayContext();
}

void ReplayOutput::DisablePixelContext()
{
  m_ContextX = -1.0f;
  m_ContextY = -1.0f;

  DisplayContext();
}

void ReplayOutput::DisplayContext()
{
  if(m_PixelContext.outputID == 0)
    return;
  float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);

  if((m_Type != ReplayOutputType::Texture) || (m_ContextX < 0.0f && m_ContextY < 0.0f) ||
     (m_RenderData.texDisplay.texid == ResourceId()))
  {
    m_pDevice->RenderCheckerboard(Vec3f(0.81f, 0.81f, 0.81f), Vec3f(0.57f, 0.57f, 0.57f));
    m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
    return;
  }

  m_pDevice->ClearOutputWindowColor(m_PixelContext.outputID, color);

  TextureDisplay disp = m_RenderData.texDisplay;
  disp.rawoutput = false;
  disp.CustomShader = ResourceId();

  if(m_RenderData.texDisplay.CustomShader != ResourceId())
    disp.texid = m_CustomShaderResourceId;

  if((m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::QuadOverdrawPass ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizeDraw ||
      m_RenderData.texDisplay.overlay == DebugOverlay::TriangleSizePass) &&
     m_OverlayResourceId != ResourceId())
    disp.texid = m_OverlayResourceId;

  const float contextZoom = 8.0f;

  disp.scale = contextZoom / float(1 << disp.mip);

  int32_t width = 0, height = 0;
  m_pDevice->GetOutputWindowDimensions(m_PixelContext.outputID, width, height);

  float w = (float)width;
  float h = (float)height;

  int x = (int)m_ContextX;
  int y = (int)m_ContextY;

  x >>= disp.mip;
  x <<= disp.mip;

  y >>= disp.mip;
  y <<= disp.mip;

  disp.offx = -(float)x * disp.scale;
  disp.offy = -(float)y * disp.scale;

  disp.offx += w / 2.0f;
  disp.offy += h / 2.0f;

  disp.texid = m_pDevice->GetLiveID(disp.texid);

  m_pDevice->RenderTexture(disp);

  m_pDevice->RenderHighlightBox(w, h, contextZoom);

  m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
}

void ReplayOutput::Display()
{
  if(m_pDevice->CheckResizeOutputWindow(m_MainOutput.outputID))
  {
    m_pDevice->GetOutputWindowDimensions(m_MainOutput.outputID, m_Width, m_Height);
    m_MainOutput.dirty = true;
  }

  for(size_t i = 0; i < m_Thumbnails.size(); i++)
    if(m_pDevice->CheckResizeOutputWindow(m_Thumbnails[i].outputID))
      m_Thumbnails[i].dirty = true;

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

    float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if(m_Thumbnails[i].texture == ResourceId())
    {
      m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);

      color[0] = 0.4f;
      m_pDevice->ClearOutputWindowColor(m_Thumbnails[i].outputID, color);

      m_pDevice->RenderCheckerboard(Vec3f(0.6f, 0.6f, 0.7f), Vec3f(0.5f, 0.5f, 0.6f));

      m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);
      continue;
    }

    m_pDevice->BindOutputWindow(m_Thumbnails[i].outputID, false);
    m_pDevice->ClearOutputWindowColor(m_Thumbnails[i].outputID, color);

    TextureDisplay disp;

    disp.Red = disp.Green = disp.Blue = true;
    disp.Alpha = false;
    disp.HDRMul = -1.0f;
    disp.linearDisplayAsGamma = true;
    disp.FlipY = false;
    disp.mip = 0;
    disp.sampleIdx = ~0U;
    disp.CustomShader = ResourceId();
    disp.texid = m_pDevice->GetLiveID(m_Thumbnails[i].texture);
    disp.typeHint = m_Thumbnails[i].typeHint;
    disp.scale = -1.0f;
    disp.rangemin = 0.0f;
    disp.rangemax = 1.0f;
    disp.sliceFace = 0;
    disp.offx = 0.0f;
    disp.offy = 0.0f;
    disp.rawoutput = false;
    disp.overlay = DebugOverlay::NoOverlay;

    disp.lightBackgroundColor = disp.darkBackgroundColor = FloatVector();

    if(m_Thumbnails[i].typeHint == CompType::SNorm)
      disp.rangemin = -1.0f;

    if(m_Thumbnails[i].depthMode)
      disp.Green = disp.Blue = false;

    m_pDevice->RenderTexture(disp);

    m_pDevice->FlipOutputWindow(m_Thumbnails[i].outputID);

    m_Thumbnails[i].dirty = false;
  }

  if(m_pDevice->CheckResizeOutputWindow(m_PixelContext.outputID))
    m_MainOutput.dirty = true;

  if(!m_MainOutput.dirty)
  {
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->FlipOutputWindow(m_MainOutput.outputID);
    m_pDevice->BindOutputWindow(m_PixelContext.outputID, false);
    m_pDevice->FlipOutputWindow(m_PixelContext.outputID);
    return;
  }

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

void ReplayOutput::DisplayTex()
{
  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(m_EventID);

  if(m_MainOutput.outputID == 0)
    return;
  if(m_RenderData.texDisplay.texid == ResourceId())
  {
    float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);
    return;
  }
  if(m_Width <= 0 || m_Height <= 0)
    return;

  TextureDisplay texDisplay = m_RenderData.texDisplay;
  texDisplay.rawoutput = false;
  texDisplay.texid = m_pDevice->GetLiveID(texDisplay.texid);

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

  if(m_RenderData.texDisplay.CustomShader != ResourceId())
  {
    m_CustomShaderResourceId = m_pDevice->ApplyCustomShader(
        m_RenderData.texDisplay.CustomShader, texDisplay.texid, texDisplay.mip,
        texDisplay.sliceFace, texDisplay.sampleIdx, texDisplay.typeHint);

    texDisplay.texid = m_pDevice->GetLiveID(m_CustomShaderResourceId);
    texDisplay.typeHint = CompType::Typeless;
    texDisplay.CustomShader = ResourceId();
    texDisplay.sliceFace = 0;
  }

  float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
  m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);

  m_pDevice->RenderCheckerboard(
      Vec3f(texDisplay.lightBackgroundColor.x, texDisplay.lightBackgroundColor.y,
            texDisplay.lightBackgroundColor.z),
      Vec3f(texDisplay.darkBackgroundColor.x, texDisplay.darkBackgroundColor.y,
            texDisplay.darkBackgroundColor.z));

  m_pDevice->RenderTexture(texDisplay);

  if(m_RenderData.texDisplay.overlay != DebugOverlay::NoOverlay && draw &&
     m_pDevice->IsRenderOutput(m_RenderData.texDisplay.texid) &&
     m_RenderData.texDisplay.overlay != DebugOverlay::NaN &&
     m_RenderData.texDisplay.overlay != DebugOverlay::Clipping)
  {
    RDCASSERT(m_OverlayResourceId != ResourceId());
    texDisplay.texid = m_pDevice->GetLiveID(m_OverlayResourceId);
    texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
    texDisplay.rawoutput = false;
    texDisplay.CustomShader = ResourceId();
    texDisplay.scale = m_RenderData.texDisplay.scale;
    texDisplay.HDRMul = -1.0f;
    texDisplay.FlipY = m_RenderData.texDisplay.FlipY;
    texDisplay.rangemin = 0.0f;
    texDisplay.rangemax = 1.0f;

    m_pDevice->RenderTexture(texDisplay);
  }
}

void ReplayOutput::DisplayMesh()
{
  DrawcallDescription *draw = m_pRenderer->GetDrawcallByEID(m_EventID);

  if(draw == NULL || m_MainOutput.outputID == 0 || m_Width <= 0 || m_Height <= 0 ||
     (m_RenderData.meshDisplay.type == MeshDataStage::Unknown) ||
     !(draw->flags & DrawFlags::Drawcall))
  {
    float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_pDevice->BindOutputWindow(m_MainOutput.outputID, false);
    m_pDevice->ClearOutputWindowColor(m_MainOutput.outputID, color);
    m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);
    m_pDevice->RenderCheckerboard(Vec3f(0.81f, 0.81f, 0.81f), Vec3f(0.57f, 0.57f, 0.57f));

    return;
  }

  if(draw && m_OverlayDirty)
  {
    m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);
    RefreshOverlay();
    m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);
  }

  m_pDevice->BindOutputWindow(m_MainOutput.outputID, true);
  m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

  m_pDevice->RenderCheckerboard(Vec3f(0.81f, 0.81f, 0.81f), Vec3f(0.57f, 0.57f, 0.57f));

  m_pDevice->ClearOutputWindowDepth(m_MainOutput.outputID, 1.0f, 0);

  MeshDisplay mesh = m_RenderData.meshDisplay;
  mesh.position.buf = m_pDevice->GetLiveID(mesh.position.buf);
  mesh.position.idxbuf = m_pDevice->GetLiveID(mesh.position.idxbuf);
  mesh.second.buf = m_pDevice->GetLiveID(mesh.second.buf);
  mesh.second.idxbuf = m_pDevice->GetLiveID(mesh.second.idxbuf);

  vector<MeshFormat> secondaryDraws;

  // we choose a pallette here so that the colours stay consistent (i.e the
  // current draw is always the same colour), but also to indicate somewhat
  // the relationship - ie. instances are closer in colour than other draws
  // in the pass

  // very slightly dark red
  const FloatVector drawItself(0.06f, 0.0f, 0.0f, 1.0f);

  // more desaturated/lighter, but still reddish
  const FloatVector otherInstances(0.18f, 0.1f, 0.1f, 1.0f);

  // lighter grey with blue tinge to contrast from main/instance draws
  const FloatVector passDraws(0.2f, 0.2f, 0.25f, 1.0f);

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
          MeshFormat fmt = m_pDevice->GetPostVSBuffers(passEvents[i], inst, MeshDataStage::GSOut);
          if(fmt.buf == ResourceId())
            fmt = m_pDevice->GetPostVSBuffers(passEvents[i], inst, MeshDataStage::VSOut);

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
        MeshFormat fmt = m_pDevice->GetPostVSBuffers(draw->eventID, inst, MeshDataStage::GSOut);
        if(fmt.buf == ResourceId())
          fmt = m_pDevice->GetPostVSBuffers(draw->eventID, inst, MeshDataStage::VSOut);

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

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_SetTextureDisplay(IReplayOutput *output,
                                                                          const TextureDisplay &o)
{
  output->SetTextureDisplay(o);
}
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_SetMeshDisplay(IReplayOutput *output,
                                                                       const MeshDisplay &o)
{
  output->SetMeshDisplay(o);
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_ClearThumbnails(IReplayOutput *output)
{
  output->ClearThumbnails();
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_AddThumbnail(IReplayOutput *output,
                                                                       WindowingSystem system,
                                                                       void *data, ResourceId texID,
                                                                       CompType typeHint)
{
  return output->AddThumbnail(system, data, texID, typeHint);
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_Display(IReplayOutput *output)
{
  output->Display();
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayOutput_SetPixelContext(IReplayOutput *output,
                                                                          WindowingSystem system,
                                                                          void *data)
{
  return output->SetPixelContext(system, data);
}
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_SetPixelContextLocation(IReplayOutput *output,
                                                                                uint32_t x,
                                                                                uint32_t y)
{
  output->SetPixelContextLocation(x, y);
}
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_DisablePixelContext(IReplayOutput *output)
{
  output->DisablePixelContext();
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_GetCustomShaderTexID(IReplayOutput *output,
                                                                             ResourceId *id)
{
  if(id)
    *id = output->GetCustomShaderTexID();
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_GetDebugOverlayTexID(IReplayOutput *output,
                                                                             ResourceId *id)
{
  if(id)
    *id = output->GetDebugOverlayTexID();
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_GetMinMax(IReplayOutput *output,
                                                                  PixelValue *minval,
                                                                  PixelValue *maxval)
{
  auto ret = output->GetMinMax();
  *minval = ret.first;
  *maxval = ret.second;
}
extern "C" RENDERDOC_API void RENDERDOC_CC
ReplayOutput_GetHistogram(IReplayOutput *output, float minval, float maxval, bool32 channels[4],
                          rdctype::array<uint32_t> *histogram)
{
  bool chans[4] = {channels[0] != 0, channels[1] != 0, channels[2] != 0, channels[3] != 0};
  *histogram = output->GetHistogram(minval, maxval, chans);
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayOutput_PickPixel(
    IReplayOutput *output, ResourceId texID, bool32 customShader, uint32_t x, uint32_t y,
    uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *val)
{
  *val = output->PickPixel(texID, customShader != 0, x, y, sliceFace, mip, sample);
}

extern "C" RENDERDOC_API uint32_t RENDERDOC_CC ReplayOutput_PickVertex(IReplayOutput *output,
                                                                       uint32_t eventID, uint32_t x,
                                                                       uint32_t y,
                                                                       uint32_t *pickedInstance)
{
  auto ret = output->PickVertex(eventID, x, y);
  *pickedInstance = ret.second;
  return ret.first;
}
