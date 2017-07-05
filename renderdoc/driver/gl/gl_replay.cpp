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

#include "gl_replay.h"
#include "driver/ihv/amd/amd_isa.h"
#include "maths/matrix.h"
#include "serialise/string_utils.h"
#include "gl_driver.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/debuguniforms.h"

GLReplay::GLReplay()
{
  m_pDriver = NULL;
  m_Proxy = false;

  m_Degraded = false;

  RDCEraseEl(m_ReplayCtx);
  m_DebugCtx = NULL;

  m_DebugID = 0;

  m_OutputWindowID = 1;

  RDCEraseEl(m_GetTexturePrevData);
}

void GLReplay::Shutdown()
{
  PreContextShutdownCounters();

  DeleteDebugData();

  DestroyOutputWindow(m_DebugID);

  CloseReplayContext();

  // clean up cached GetTextureData allocations
  for(size_t i = 0; i < ARRAY_COUNT(m_GetTexturePrevData); i++)
  {
    delete[] m_GetTexturePrevData[i];
    m_GetTexturePrevData[i] = NULL;
  }

  delete m_pDriver;

  GLReplay::PostContextShutdownCounters();
}

#pragma region Implemented

void GLReplay::ReadLogInitialisation()
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->ReadLogInitialisation();
}

void GLReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->ReplayLog(0, endEventID, replayType);
}

vector<uint32_t> GLReplay::GetPassEvents(uint32_t eventID)
{
  vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventID);

  const DrawcallDescription *start = draw;
  while(start && start->previous != 0 &&
        !(m_pDriver->GetDrawcall((uint32_t)start->previous)->flags & DrawFlags::Clear))
  {
    const DrawcallDescription *prev = m_pDriver->GetDrawcall((uint32_t)start->previous);

    if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) ||
       start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  while(start)
  {
    if(start == draw)
      break;

    if(start->flags & DrawFlags::Drawcall)
      passEvents.push_back(start->eventID);

    start = m_pDriver->GetDrawcall((uint32_t)start->next);
  }

  return passEvents;
}

FrameRecord GLReplay::GetFrameRecord()
{
  return m_pDriver->GetFrameRecord();
}

ResourceId GLReplay::GetLiveID(ResourceId id)
{
  return m_pDriver->GetResourceManager()->GetLiveID(id);
}

APIProperties GLReplay::GetAPIProperties()
{
  APIProperties ret;

  ret.pipelineType = GraphicsAPI::OpenGL;
  ret.localRenderer = GraphicsAPI::OpenGL;
  ret.degraded = m_Degraded;

  return ret;
}

vector<ResourceId> GLReplay::GetBuffers()
{
  vector<ResourceId> ret;

  for(auto it = m_pDriver->m_Buffers.begin(); it != m_pDriver->m_Buffers.end(); ++it)
  {
    // skip buffers that aren't from the log
    if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  return ret;
}

vector<ResourceId> GLReplay::GetTextures()
{
  vector<ResourceId> ret;
  ret.reserve(m_pDriver->m_Textures.size());

  for(auto it = m_pDriver->m_Textures.begin(); it != m_pDriver->m_Textures.end(); ++it)
  {
    auto &res = m_pDriver->m_Textures[it->first];

    // skip textures that aren't from the log (except the 'default backbuffer' textures)
    if(res.resource.name != m_pDriver->m_FakeBB_Color &&
       res.resource.name != m_pDriver->m_FakeBB_DepthStencil &&
       m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
    CacheTexture(it->first);
  }

  return ret;
}

void GLReplay::SetReplayData(GLWindowingData data)
{
  m_ReplayCtx = data;
  if(m_pDriver != NULL)
    m_pDriver->RegisterContext(m_ReplayCtx, NULL, true, true);

  InitDebugData();

  PostContextInitCounters();
}

void GLReplay::InitCallstackResolver()
{
  m_pDriver->GetSerialiser()->InitCallstackResolver();
}

bool GLReplay::HasCallstacks()
{
  return m_pDriver->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *GLReplay::GetCallstackResolver()
{
  return m_pDriver->GetSerialiser()->GetCallstackResolver();
}

void GLReplay::CreateOutputWindowBackbuffer(OutputWindow &outwin, bool depth)
{
  if(m_pDriver == NULL)
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &gl = *m_pDriver;

  // create fake backbuffer for this output window.
  // We'll make an FBO for this backbuffer on the replay context, so we can
  // use the replay context to do the hard work of rendering to it, then just
  // blit across to the real default framebuffer on the output window context
  gl.glGenFramebuffers(1, &outwin.BlitData.windowFBO);
  gl.glBindFramebuffer(eGL_FRAMEBUFFER, outwin.BlitData.windowFBO);

  gl.glGenTextures(1, &outwin.BlitData.backbuffer);
  gl.glBindTexture(eGL_TEXTURE_2D, outwin.BlitData.backbuffer);

  gl.glTextureImage2DEXT(outwin.BlitData.backbuffer, eGL_TEXTURE_2D, 0, eGL_SRGB8, outwin.width,
                         outwin.height, 0, eGL_RGB, eGL_UNSIGNED_BYTE, NULL);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, outwin.BlitData.backbuffer, 0);

  if(depth)
  {
    gl.glGenTextures(1, &outwin.BlitData.depthstencil);
    gl.glBindTexture(eGL_TEXTURE_2D, outwin.BlitData.depthstencil);

    gl.glTextureImage2DEXT(outwin.BlitData.depthstencil, eGL_TEXTURE_2D, 0, eGL_DEPTH_COMPONENT24,
                           outwin.width, outwin.height, 0, eGL_DEPTH_COMPONENT, eGL_UNSIGNED_INT,
                           NULL);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  }
  else
  {
    outwin.BlitData.depthstencil = 0;
  }

  outwin.BlitData.replayFBO = 0;
}

void GLReplay::InitOutputWindow(OutputWindow &outwin)
{
  if(m_pDriver == NULL)
    return;

  MakeCurrentReplayContext(&outwin);

  WrappedOpenGL &gl = *m_pDriver;

  gl.glGenVertexArrays(1, &outwin.BlitData.emptyVAO);
  gl.glBindVertexArray(outwin.BlitData.emptyVAO);

  gl.glGenFramebuffers(1, &outwin.BlitData.readFBO);
  gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, outwin.BlitData.readFBO);
  gl.glReadBuffer(eGL_COLOR_ATTACHMENT0);
}

bool GLReplay::CheckResizeOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  OutputWindow &outw = m_OutputWindows[id];

  if(outw.wnd == 0)
    return false;

  int32_t w, h;
  GetOutputWindowDimensions(id, w, h);

  if(w != outw.width || h != outw.height)
  {
    outw.width = w;
    outw.height = h;

    MakeCurrentReplayContext(m_DebugCtx);

    WrappedOpenGL &gl = *m_pDriver;

    bool haddepth = false;

    gl.glDeleteTextures(1, &outw.BlitData.backbuffer);
    if(outw.BlitData.depthstencil)
    {
      haddepth = true;
      gl.glDeleteTextures(1, &outw.BlitData.depthstencil);
    }
    gl.glDeleteFramebuffers(1, &outw.BlitData.windowFBO);

    CreateOutputWindowBackbuffer(outw, haddepth);

    return true;
  }

  return false;
}

void GLReplay::BindOutputWindow(uint64_t id, bool depth)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glBindFramebuffer(eGL_FRAMEBUFFER, outw.BlitData.windowFBO);
  m_pDriver->glViewport(0, 0, outw.width, outw.height);

  m_pDriver->glFramebufferTexture(
      eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
      depth && outw.BlitData.depthstencil ? outw.BlitData.depthstencil : 0, 0);

  DebugData.outWidth = float(outw.width);
  DebugData.outHeight = float(outw.height);
}

void GLReplay::ClearOutputWindowColor(uint64_t id, float col[4])
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glClearBufferfv(eGL_COLOR, 0, col);
}

void GLReplay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  m_pDriver->glClearBufferfi(eGL_DEPTH_STENCIL, 0, depth, (GLint)stencil);
}

void GLReplay::FlipOutputWindow(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  MakeCurrentReplayContext(&outw);

  WrappedOpenGL &gl = *m_pDriver;

  // go directly to real function so we don't try to bind the 'fake' backbuffer FBO.
  gl.m_Real.glBindFramebuffer(eGL_FRAMEBUFFER, 0);
  gl.glViewport(0, 0, outw.width, outw.height);

  gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, outw.BlitData.readFBO);

  gl.glFramebufferTexture2D(eGL_READ_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                            outw.BlitData.backbuffer, 0);
  gl.glReadBuffer(eGL_COLOR_ATTACHMENT0);

  gl.glEnable(eGL_FRAMEBUFFER_SRGB);

  gl.glBlitFramebuffer(0, 0, outw.width, outw.height, 0, 0, outw.width, outw.height,
                       GL_COLOR_BUFFER_BIT, eGL_NEAREST);

  SwapBuffers(&outw);
}

void GLReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret)
{
  if(m_pDriver->m_Buffers.find(buff) == m_pDriver->m_Buffers.end())
  {
    RDCWARN("Requesting data for non-existant buffer %llu", buff);
    return;
  }

  auto &buf = m_pDriver->m_Buffers[buff];

  uint64_t bufsize = buf.size;

  if(len > 0 && offset + len > bufsize)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%llu)",
            offset, len, bufsize);

    if(offset < bufsize)
      len = ~0ULL;    // min below will clamp to max size
    else
      return;    // offset past buffer size, return empty array
  }
  else if(len == 0)
  {
    len = bufsize;
  }

  // need to ensure len+offset doesn't overrun buffer or the glGetBufferSubData call
  // will fail.
  len = RDCMIN(len, bufsize - offset);

  if(len == 0)
    return;

  ret.resize((size_t)len);

  WrappedOpenGL &gl = *m_pDriver;

  GLuint oldbuf = 0;
  gl.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf);

  gl.glBindBuffer(eGL_COPY_READ_BUFFER, buf.resource.name);

  gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)len, &ret[0]);

  gl.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf);
}

bool GLReplay::IsRenderOutput(ResourceId id)
{
  for(int32_t i = 0; i < m_CurPipelineState.m_FB.m_DrawFBO.Color.count; i++)
  {
    if(m_CurPipelineState.m_FB.m_DrawFBO.Color[i].Obj == id)
      return true;
  }

  if(m_CurPipelineState.m_FB.m_DrawFBO.Depth.Obj == id ||
     m_CurPipelineState.m_FB.m_DrawFBO.Stencil.Obj == id)
    return true;

  return false;
}

TextureDescription GLReplay::GetTexture(ResourceId id)
{
  auto it = m_CachedTextures.find(id);
  if(it == m_CachedTextures.end())
  {
    CacheTexture(id);
    return m_CachedTextures[id];
  }

  return it->second;
}

void GLReplay::CacheTexture(ResourceId id)
{
  TextureDescription tex;

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &res = m_pDriver->m_Textures[id];
  WrappedOpenGL &gl = *m_pDriver;

  tex.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);

  if(res.resource.Namespace == eResUnknown || res.curType == eGL_NONE)
  {
    if(res.resource.Namespace == eResUnknown)
      RDCERR("Details for invalid texture id %llu requested", id);

    tex.name = "<Uninitialised Texture>";
    tex.customName = true;
    tex.format = ResourceFormat();
    tex.dimension = 1;
    tex.resType = TextureDim::Unknown;
    tex.width = tex.height = tex.depth = 1;
    tex.cubemap = false;
    tex.mips = 1;
    tex.arraysize = 1;
    tex.creationFlags = TextureCategory::NoFlags;
    tex.msQual = 0;
    tex.msSamp = 1;
    tex.byteSize = 1;

    m_CachedTextures[id] = tex;
    return;
  }

  if(res.resource.Namespace == eResRenderbuffer || res.curType == eGL_RENDERBUFFER)
  {
    tex.dimension = 2;
    tex.resType = TextureDim::Texture2D;
    tex.width = res.width;
    tex.height = res.height;
    tex.depth = 1;
    tex.cubemap = false;
    tex.mips = 1;
    tex.arraysize = 1;
    tex.creationFlags = TextureCategory::ColorTarget;
    tex.msQual = 0;
    tex.msSamp = RDCMAX(1, res.samples);

    tex.format = MakeResourceFormat(gl.GetHookset(), eGL_TEXTURE_2D, res.internalFormat);

    if(IsDepthStencilFormat(res.internalFormat))
      tex.creationFlags |= TextureCategory::DepthTarget;

    tex.byteSize = (tex.width * tex.height) * (tex.format.compByteWidth * tex.format.compCount);

    string str = m_pDriver->GetResourceManager()->GetName(tex.ID);
    tex.customName = true;

    if(str == "")
    {
      const char *suffix = "";
      const char *ms = "";

      if(tex.msSamp > 1)
        ms = "MS";

      if(tex.creationFlags & TextureCategory::ColorTarget)
        suffix = " RTV";
      if(tex.creationFlags & TextureCategory::DepthTarget)
        suffix = " DSV";

      tex.customName = false;

      str = StringFormat::Fmt("Renderbuffer%s%s %llu", ms, suffix, tex.ID);
    }

    tex.name = str;

    m_CachedTextures[id] = tex;
    return;
  }

  GLenum target = TextureTarget(res.curType);

  GLenum levelQueryType = target;
  if(levelQueryType == eGL_TEXTURE_CUBE_MAP)
    levelQueryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

  GLint width = 1, height = 1, depth = 1, samples = 1;
  gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_WIDTH, &width);
  gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_HEIGHT,
                                     &height);
  gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_DEPTH, &depth);
  gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_SAMPLES,
                                     &samples);

  // the above queries sometimes come back 0, if we have dimensions from creation functions, use
  // those
  if(width == 0 && res.width > 0)
    width = res.width;
  if(height == 0 && res.height > 0)
    height = res.height;
  if(depth == 0 && res.depth > 0)
    depth = res.depth;

  if(res.width == 0 && width > 0)
  {
    RDCWARN("TextureData::width didn't get filled out, setting at last minute");
    res.width = width;
  }
  if(res.height == 0 && height > 0)
  {
    RDCWARN("TextureData::height didn't get filled out, setting at last minute");
    res.height = height;
  }
  if(res.depth == 0 && depth > 0)
  {
    RDCWARN("TextureData::depth didn't get filled out, setting at last minute");
    res.depth = depth;
  }

  // reasonably common defaults
  tex.msQual = 0;
  tex.msSamp = 1;
  tex.width = tex.height = tex.depth = tex.arraysize = 1;
  tex.cubemap = false;

  switch(target)
  {
    case eGL_TEXTURE_BUFFER: tex.resType = TextureDim::Buffer; break;
    case eGL_TEXTURE_1D: tex.resType = TextureDim::Texture1D; break;
    case eGL_TEXTURE_2D: tex.resType = TextureDim::Texture2D; break;
    case eGL_TEXTURE_3D: tex.resType = TextureDim::Texture3D; break;
    case eGL_TEXTURE_1D_ARRAY: tex.resType = TextureDim::Texture1DArray; break;
    case eGL_TEXTURE_2D_ARRAY: tex.resType = TextureDim::Texture2DArray; break;
    case eGL_TEXTURE_RECTANGLE: tex.resType = TextureDim::TextureRect; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: tex.resType = TextureDim::Texture2DMS; break;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: tex.resType = TextureDim::Texture2DMSArray; break;
    case eGL_TEXTURE_CUBE_MAP: tex.resType = TextureDim::TextureCube; break;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: tex.resType = TextureDim::TextureCubeArray; break;

    default:
      tex.resType = TextureDim::Unknown;
      RDCERR("Unexpected texture enum %s", ToStr::Get(target).c_str());
  }

  switch(target)
  {
    case eGL_TEXTURE_1D:
    case eGL_TEXTURE_BUFFER:
      tex.dimension = 1;
      tex.width = (uint32_t)width;
      break;
    case eGL_TEXTURE_1D_ARRAY:
      tex.dimension = 1;
      tex.width = (uint32_t)width;
      tex.arraysize = depth;
      break;
    case eGL_TEXTURE_2D:
    case eGL_TEXTURE_RECTANGLE:
    case eGL_TEXTURE_2D_MULTISAMPLE:
    case eGL_TEXTURE_CUBE_MAP:
      tex.dimension = 2;
      tex.width = (uint32_t)width;
      tex.height = (uint32_t)height;
      tex.depth = 1;
      tex.arraysize = (target == eGL_TEXTURE_CUBE_MAP ? 6 : 1);
      tex.cubemap = (target == eGL_TEXTURE_CUBE_MAP);
      tex.msSamp = RDCMAX(1, target == eGL_TEXTURE_2D_MULTISAMPLE ? samples : 1);
      break;
    case eGL_TEXTURE_2D_ARRAY:
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY:
    case eGL_TEXTURE_CUBE_MAP_ARRAY:
      tex.dimension = 2;
      tex.width = (uint32_t)width;
      tex.height = (uint32_t)height;
      tex.depth = 1;
      tex.arraysize = depth;
      tex.cubemap = (target == eGL_TEXTURE_CUBE_MAP_ARRAY);
      tex.msSamp = RDCMAX(1, target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ? samples : 1);
      break;
    case eGL_TEXTURE_3D:
      tex.dimension = 3;
      tex.width = (uint32_t)width;
      tex.height = (uint32_t)height;
      tex.depth = (uint32_t)depth;
      break;

    default: tex.dimension = 2; RDCERR("Unexpected texture enum %s", ToStr::Get(target).c_str());
  }

  tex.creationFlags = res.creationFlags;
  if(res.resource.name == gl.m_FakeBB_Color || res.resource.name == gl.m_FakeBB_DepthStencil)
    tex.creationFlags |= TextureCategory::SwapBuffer;

  // surely this will be the same for each level... right? that would be insane if it wasn't
  GLint fmt = 0;
  gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0,
                                     eGL_TEXTURE_INTERNAL_FORMAT, &fmt);

  tex.format = MakeResourceFormat(gl.GetHookset(), target, (GLenum)fmt);

  if(tex.format.compType == CompType::Depth)
    tex.creationFlags |= TextureCategory::DepthTarget;

  string str = m_pDriver->GetResourceManager()->GetName(tex.ID);
  tex.customName = true;

  if(str == "")
  {
    const char *suffix = "";
    const char *ms = "";

    if(tex.msSamp > 1)
      ms = "MS";

    if(tex.creationFlags & TextureCategory::ColorTarget)
      suffix = " RTV";
    if(tex.creationFlags & TextureCategory::DepthTarget)
      suffix = " DSV";

    tex.customName = false;

    if(tex.cubemap)
    {
      if(tex.arraysize > 6)
        str = StringFormat::Fmt("TextureCube%sArray%s %llu", ms, suffix, tex.ID);
      else
        str = StringFormat::Fmt("TextureCube%s%s %llu", ms, suffix, tex.ID);
    }
    else
    {
      if(tex.arraysize > 1)
        str = StringFormat::Fmt("Texture%dD%sArray%s %llu", tex.dimension, ms, suffix, tex.ID);
      else
        str = StringFormat::Fmt("Texture%dD%s%s %llu", tex.dimension, ms, suffix, tex.ID);
    }
  }

  tex.name = str;

  if(target == eGL_TEXTURE_BUFFER)
  {
    tex.dimension = 1;
    tex.width = tex.height = tex.depth = 1;
    tex.cubemap = false;
    tex.mips = 1;
    tex.arraysize = 1;
    tex.creationFlags = TextureCategory::ShaderRead;
    tex.msQual = 0;
    tex.msSamp = 1;
    tex.byteSize = 0;

    gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0,
                                       eGL_TEXTURE_BUFFER_SIZE, (GLint *)&tex.byteSize);
    tex.width = uint32_t(tex.byteSize / (tex.format.compByteWidth * tex.format.compCount));

    m_CachedTextures[id] = tex;
    return;
  }

  tex.mips = GetNumMips(gl.m_Real, target, res.resource.name, tex.width, tex.height, tex.depth);

  GLint compressed;
  gl.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_COMPRESSED,
                                     &compressed);
  tex.byteSize = 0;
  for(uint32_t a = 0; a < tex.arraysize; a++)
  {
    for(uint32_t m = 0; m < tex.mips; m++)
    {
      if(compressed)
      {
        tex.byteSize += (uint64_t)GetCompressedByteSize(
            RDCMAX(1U, tex.width >> m), RDCMAX(1U, tex.height >> m), 1, (GLenum)fmt);
      }
      else if(tex.format.special)
      {
        tex.byteSize += GetByteSize(RDCMAX(1U, tex.width >> m), RDCMAX(1U, tex.height >> m),
                                    RDCMAX(1U, tex.depth >> m), GetBaseFormat((GLenum)fmt),
                                    GetDataType((GLenum)fmt));
      }
      else
      {
        tex.byteSize += RDCMAX(1U, tex.width >> m) * RDCMAX(1U, tex.height >> m) *
                        RDCMAX(1U, tex.depth >> m) * tex.format.compByteWidth * tex.format.compCount;
      }
    }
  }

  m_CachedTextures[id] = tex;
}

BufferDescription GLReplay::GetBuffer(ResourceId id)
{
  BufferDescription ret;

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &res = m_pDriver->m_Buffers[id];

  if(res.resource.Namespace == eResUnknown)
  {
    RDCERR("Details for invalid buffer id %llu requested", id);
    RDCEraseEl(ret);
    return ret;
  }

  WrappedOpenGL &gl = *m_pDriver;

  ret.ID = m_pDriver->GetResourceManager()->GetOriginalID(id);

  GLint prevBind = 0;
  if(res.curType != eGL_NONE)
  {
    gl.glGetIntegerv(BufferBinding(res.curType), &prevBind);

    gl.glBindBuffer(res.curType, res.resource.name);
  }

  ret.creationFlags = res.creationFlags;

  GLint size = 0;
  // if the type is NONE it's probably a DSA created buffer
  if(res.curType == eGL_NONE)
  {
    // if we have the DSA entry point
    if(gl.GetHookset().glGetNamedBufferParameterivEXT)
      gl.glGetNamedBufferParameterivEXT(res.resource.name, eGL_BUFFER_SIZE, &size);
  }
  else
  {
    gl.glGetBufferParameteriv(res.curType, eGL_BUFFER_SIZE, &size);
  }

  ret.length = size;

  if(res.size == 0)
  {
    RDCWARN("BufferData::size didn't get filled out, setting at last minute");
    res.size = ret.length;
  }

  string str = m_pDriver->GetResourceManager()->GetName(ret.ID);
  ret.customName = true;

  if(str == "")
  {
    ret.customName = false;
    str = StringFormat::Fmt("Buffer %llu", ret.ID);
  }

  ret.name = str;

  if(res.curType != eGL_NONE)
    gl.glBindBuffer(res.curType, prevBind);

  return ret;
}

vector<DebugMessage> GLReplay::GetDebugMessages()
{
  return m_pDriver->GetDebugMessages();
}

ShaderReflection *GLReplay::GetShader(ResourceId shader, string entryPoint)
{
  auto &shaderDetails = m_pDriver->m_Shaders[shader];

  if(shaderDetails.prog == 0)
  {
    RDCERR("Can't get shader details without separable program");
    return NULL;
  }

  return &shaderDetails.reflection;
}

vector<string> GLReplay::GetDisassemblyTargets()
{
  vector<string> ret;

  GCNISA::GetTargets(GraphicsAPI::OpenGL, ret);

  // default is always first
  ret.insert(ret.begin(), "SPIR-V (RenderDoc)");

  return ret;
}

string GLReplay::DisassembleShader(const ShaderReflection *refl, const string &target)
{
  auto &shaderDetails = m_pDriver->m_Shaders[m_pDriver->GetResourceManager()->GetLiveID(refl->ID)];

  if(shaderDetails.sources.empty())
    return "Invalid Shader Specified";

  if(target == "SPIR-V (RenderDoc)" || target.empty())
  {
    std::string &disasm = shaderDetails.disassembly;

    if(disasm.empty())
      disasm = shaderDetails.spirv.Disassemble(refl->EntryPoint.c_str());

    return disasm;
  }

  ShaderStage stages[] = {
      ShaderStage::Vertex,   ShaderStage::Tess_Control, ShaderStage::Tess_Eval,
      ShaderStage::Geometry, ShaderStage::Fragment,     ShaderStage::Compute,
  };

  return GCNISA::Disassemble(stages[ShaderIdx(shaderDetails.type)], shaderDetails.sources, target);
}

void GLReplay::SavePipelineState()
{
  GLPipe::State &pipe = m_CurPipelineState;
  WrappedOpenGL &gl = *m_pDriver;
  GLResourceManager *rm = m_pDriver->GetResourceManager();

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLRenderState rs(&gl.GetHookset(), NULL, READING);
  rs.FetchState(m_ReplayCtx.ctx, &gl);

  // Index buffer

  void *ctx = m_ReplayCtx.ctx;

  GLuint ibuffer = 0;
  gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
  pipe.m_VtxIn.ibuffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, ibuffer)));

  pipe.m_VtxIn.primitiveRestart = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart];
  pipe.m_VtxIn.restartIndex = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex]
                                  ? ~0U
                                  : rs.PrimitiveRestartIndex;

  // Vertex buffers and attributes
  GLint numVBufferBindings = 16;
  gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);

  GLint numVAttribBindings = 16;
  gl.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numVAttribBindings);

  create_array_uninit(pipe.m_VtxIn.vbuffers, numVBufferBindings);
  create_array_uninit(pipe.m_VtxIn.attributes, numVAttribBindings);

  for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
  {
    GLuint buffer = GetBoundVertexBuffer(gl.m_Real, i);

    pipe.m_VtxIn.vbuffers[i].Buffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, buffer)));

    gl.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Stride);
    gl.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Offset);
    gl.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i, (GLint *)&pipe.m_VtxIn.vbuffers[i].Divisor);
  }

  for(GLuint i = 0; i < (GLuint)numVAttribBindings; i++)
  {
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED,
                           (GLint *)&pipe.m_VtxIn.attributes[i].Enabled);
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING,
                           (GLint *)&pipe.m_VtxIn.attributes[i].BufferSlot);
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET,
                           (GLint *)&pipe.m_VtxIn.attributes[i].RelativeOffset);

    GLenum type = eGL_FLOAT;
    GLint normalized = 0;

    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&type);
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);

    GLint integer = 0;
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);

    RDCEraseEl(pipe.m_VtxIn.attributes[i].GenericValue);
    gl.glGetVertexAttribfv(i, eGL_CURRENT_VERTEX_ATTRIB,
                           pipe.m_VtxIn.attributes[i].GenericValue.value_f);

    ResourceFormat fmt;

    fmt.special = false;
    fmt.compCount = 4;
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&fmt.compCount);

    bool intComponent = !normalized || integer;

    switch(type)
    {
      default:
      case eGL_BYTE:
        fmt.compByteWidth = 1;
        fmt.compType = intComponent ? CompType::SInt : CompType::SNorm;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_BYTE%d", fmt.compCount)
                                         : string("GL_BYTE")) +
                      (intComponent ? "" : "_SNORM");
        break;
      case eGL_UNSIGNED_BYTE:
        fmt.compByteWidth = 1;
        fmt.compType = intComponent ? CompType::UInt : CompType::UNorm;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_UNSIGNED_BYTE%d", fmt.compCount)
                                         : string("GL_UNSIGNED_BYTE")) +
                      (intComponent ? "" : "_UNORM");
        break;
      case eGL_SHORT:
        fmt.compByteWidth = 2;
        fmt.compType = intComponent ? CompType::SInt : CompType::SNorm;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_SHORT%d", fmt.compCount)
                                         : string("GL_SHORT")) +
                      (intComponent ? "" : "_SNORM");
        break;
      case eGL_UNSIGNED_SHORT:
        fmt.compByteWidth = 2;
        fmt.compType = intComponent ? CompType::UInt : CompType::UNorm;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_UNSIGNED_SHORT%d", fmt.compCount)
                                         : string("GL_UNSIGNED_SHORT")) +
                      (intComponent ? "" : "_UNORM");
        break;
      case eGL_INT:
        fmt.compByteWidth = 4;
        fmt.compType = intComponent ? CompType::SInt : CompType::SNorm;
        fmt.strname =
            (fmt.compCount > 1 ? StringFormat::Fmt("GL_INT%d", fmt.compCount) : string("GL_INT")) +
            (intComponent ? "" : "_SNORM");
        break;
      case eGL_UNSIGNED_INT:
        fmt.compByteWidth = 4;
        fmt.compType = intComponent ? CompType::UInt : CompType::UNorm;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_UNSIGNED_INT%d", fmt.compCount)
                                         : string("GL_UNSIGNED_INT")) +
                      (intComponent ? "" : "_UNORM");
        break;
      case eGL_FLOAT:
        fmt.compByteWidth = 4;
        fmt.compType = CompType::Float;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_FLOAT%d", fmt.compCount)
                                         : string("GL_FLOAT"));
        break;
      case eGL_DOUBLE:
        fmt.compByteWidth = 8;
        fmt.compType = CompType::Double;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_DOUBLE%d", fmt.compCount)
                                         : string("GL_DOUBLE"));
        break;
      case eGL_HALF_FLOAT:
        fmt.compByteWidth = 2;
        fmt.compType = CompType::Float;
        fmt.strname = (fmt.compCount > 1 ? StringFormat::Fmt("GL_HALF_FLOAT%d", fmt.compCount)
                                         : string("GL_HALF_FLOAT"));
        break;
      case eGL_INT_2_10_10_10_REV:
        fmt.special = true;
        fmt.specialFormat = SpecialFormat::R10G10B10A2;
        fmt.compCount = 4;
        fmt.compType = CompType::UInt;
        fmt.strname = "GL_INT_2_10_10_10_REV";
        break;
      case eGL_UNSIGNED_INT_2_10_10_10_REV:
        fmt.special = true;
        fmt.specialFormat = SpecialFormat::R10G10B10A2;
        fmt.compCount = 4;
        fmt.compType = CompType::SInt;
        fmt.strname = "GL_UNSIGNED_INT_2_10_10_10_REV";
        break;
      case eGL_UNSIGNED_INT_10F_11F_11F_REV:
        fmt.special = true;
        fmt.specialFormat = SpecialFormat::R11G11B10;
        fmt.compCount = 3;
        fmt.compType = CompType::Float;
        fmt.strname = "GL_UNSIGNED_INT_10F_11F_11F_REV";
        break;
    }

    if(fmt.compCount == eGL_BGRA)
    {
      fmt.compByteWidth = 1;
      fmt.compCount = 4;
      fmt.bgraOrder = true;
      fmt.compType = CompType::UNorm;

      if(type == eGL_UNSIGNED_BYTE)
      {
        fmt.strname = "GL_BGRA8";
      }
      else if(type == eGL_UNSIGNED_INT_2_10_10_10_REV || type == eGL_INT_2_10_10_10_REV)
      {
        fmt.specialFormat = SpecialFormat::R10G10B10A2;
        fmt.compType = type == eGL_UNSIGNED_INT_2_10_10_10_REV ? CompType::UInt : CompType::SInt;
        fmt.strname = type == eGL_UNSIGNED_INT_2_10_10_10_REV ? "GL_UNSIGNED_INT_2_10_10_10_REV"
                                                              : "GL_INT_2_10_10_10_REV";
      }
      else
      {
        RDCERR("Unexpected BGRA type");
      }

      // haven't checked the other cases work properly
      RDCASSERT(type == eGL_UNSIGNED_BYTE);
    }

    pipe.m_VtxIn.attributes[i].Format = fmt;
  }

  pipe.m_VtxIn.provokingVertexLast = (rs.ProvokingVertex != eGL_FIRST_VERTEX_CONVENTION);

  memcpy(pipe.m_VtxProcess.defaultInnerLevel, rs.PatchParams.defaultInnerLevel,
         sizeof(rs.PatchParams.defaultInnerLevel));
  memcpy(pipe.m_VtxProcess.defaultOuterLevel, rs.PatchParams.defaultOuterLevel,
         sizeof(rs.PatchParams.defaultOuterLevel));

  pipe.m_VtxProcess.discard = rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard];
  pipe.m_VtxProcess.clipOriginLowerLeft = (rs.ClipOrigin != eGL_UPPER_LEFT);
  pipe.m_VtxProcess.clipNegativeOneToOne = (rs.ClipDepth != eGL_ZERO_TO_ONE);
  for(int i = 0; i < 8; i++)
    pipe.m_VtxProcess.clipPlanes[i] = rs.Enabled[GLRenderState::eEnabled_ClipDistance0 + i];

  // Shader stages & Textures

  GLint numTexUnits = 8;
  gl.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);
  create_array_uninit(pipe.Textures, numTexUnits);
  create_array_uninit(pipe.Samplers, numTexUnits);

  GLenum activeTexture = eGL_TEXTURE0;
  gl.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&activeTexture);

  pipe.m_VS.stage = ShaderStage::Vertex;
  pipe.m_TCS.stage = ShaderStage::Tess_Control;
  pipe.m_TES.stage = ShaderStage::Tess_Eval;
  pipe.m_GS.stage = ShaderStage::Geometry;
  pipe.m_FS.stage = ShaderStage::Fragment;
  pipe.m_CS.stage = ShaderStage::Compute;

  GLuint curProg = 0;
  gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

  GLPipe::Shader *stages[6] = {
      &pipe.m_VS, &pipe.m_TCS, &pipe.m_TES, &pipe.m_GS, &pipe.m_FS, &pipe.m_CS,
  };
  ShaderReflection *refls[6] = {NULL};
  ShaderBindpointMapping *mappings[6] = {NULL};

  for(int i = 0; i < 6; i++)
  {
    stages[i]->Object = ResourceId();
    stages[i]->ShaderDetails = NULL;
    stages[i]->BindpointMapping.ConstantBlocks.Delete();
    stages[i]->BindpointMapping.ReadOnlyResources.Delete();
    stages[i]->BindpointMapping.ReadWriteResources.Delete();
  }

  if(curProg == 0)
  {
    gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curProg);

    if(curProg == 0)
    {
      for(GLint unit = 0; unit < numTexUnits; unit++)
      {
        RDCEraseEl(pipe.Textures[unit]);
        RDCEraseEl(pipe.Samplers[unit]);
      }
    }
    else
    {
      ResourceId id = rm->GetID(ProgramPipeRes(ctx, curProg));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      string pipelineName = rm->GetName(rm->GetOriginalID(id));

      for(size_t i = 0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
      {
        stages[i]->PipelineActive = true;
        stages[i]->PipelineName = pipelineName;
        stages[i]->customPipelineName = (pipelineName != "");

        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          curProg = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
          stages[i]->Object = rm->GetOriginalID(pipeDetails.stageShaders[i]);
          refls[i] = GetShader(pipeDetails.stageShaders[i], "");
          GetBindpointMapping(gl.GetHookset(), curProg, (int)i, refls[i],
                              stages[i]->BindpointMapping);
          mappings[i] = &stages[i]->BindpointMapping;

          stages[i]->ProgramName = rm->GetName(rm->GetOriginalID(pipeDetails.stagePrograms[i]));
          stages[i]->customProgramName = !stages[i]->ProgramName.empty();

          stages[i]->ShaderName = rm->GetName(rm->GetOriginalID(pipeDetails.stageShaders[i]));
          stages[i]->customShaderName = !stages[i]->ShaderName.empty();
        }
        else
        {
          stages[i]->Object = ResourceId();
        }
      }
    }
  }
  else
  {
    ResourceId id = rm->GetID(ProgramRes(ctx, curProg));
    auto &progDetails = m_pDriver->m_Programs[id];

    string programName = rm->GetName(rm->GetOriginalID(id));

    for(size_t i = 0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        stages[i]->ProgramName = programName;
        stages[i]->customProgramName = (programName != "");

        stages[i]->Object = rm->GetOriginalID(progDetails.stageShaders[i]);
        refls[i] = GetShader(progDetails.stageShaders[i], "");
        GetBindpointMapping(gl.GetHookset(), curProg, (int)i, refls[i], stages[i]->BindpointMapping);
        mappings[i] = &stages[i]->BindpointMapping;

        stages[i]->ShaderName = rm->GetName(rm->GetOriginalID(progDetails.stageShaders[i]));
        stages[i]->customShaderName = !stages[i]->ShaderName.empty();
      }
    }
  }

  RDCEraseEl(pipe.m_Feedback);

  if(HasExt[ARB_transform_feedback2])
  {
    GLuint feedback = 0;
    gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&feedback);

    if(feedback != 0)
      pipe.m_Feedback.Obj = rm->GetOriginalID(rm->GetID(FeedbackRes(ctx, feedback)));

    GLint maxCount = 0;
    gl.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(int i = 0; i < (int)ARRAY_COUNT(pipe.m_Feedback.BufferBinding) && i < maxCount; i++)
    {
      GLuint buffer = 0;
      gl.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);
      pipe.m_Feedback.BufferBinding[i] = rm->GetOriginalID(rm->GetID(BufferRes(ctx, buffer)));
      gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i,
                           (GLint64 *)&pipe.m_Feedback.Offset[i]);
      gl.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i,
                           (GLint64 *)&pipe.m_Feedback.Size[i]);
    }

    GLint p = 0;
    gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BUFFER_PAUSED, &p);
    pipe.m_Feedback.Paused = (p != 0);

    gl.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE, &p);
    pipe.m_Feedback.Active = (p != 0);
  }

  for(int i = 0; i < 6; i++)
  {
    size_t num = RDCMIN(128, rs.Subroutines[i].numSubroutines);
    if(num == 0)
    {
      RDCEraseEl(stages[i]->Subroutines);
    }
    else
    {
      create_array_uninit(stages[i]->Subroutines, num);
      memcpy(stages[i]->Subroutines.elems, rs.Subroutines[i].Values, num);
    }
  }

  // GL is ass-backwards in its handling of texture units. When a shader is active
  // the types in the glsl samplers inform which targets are used from which texture units
  //
  // So texture unit 5 can have a 2D bound (texture 52) and a Cube bound (texture 77).
  // * if a uniform sampler2D has value 5 then the 2D texture is used, and we sample from 52
  // * if a uniform samplerCube has value 5 then the Cube texture is used, and we sample from 77
  // It's illegal for both a sampler2D and samplerCube to both have the same value (or any two
  // different types). It makes it all rather pointless and needlessly complex.
  //
  // What we have to do then, is consider the program, look at the values of the uniforms, and
  // then get the appropriate current binding based on the uniform type. We can warn/alert the
  // user if we hit the illegal case of two uniforms with different types but the same value
  //
  // Handling is different if no shaders are active, but we don't consider that case.

  for(GLint unit = 0; unit < numTexUnits; unit++)
  {
    GLenum binding = eGL_NONE;
    GLenum target = eGL_NONE;
    TextureDim resType = TextureDim::Unknown;

    bool shadow = false;

    for(size_t s = 0; s < ARRAY_COUNT(refls); s++)
    {
      if(refls[s] == NULL)
        continue;

      for(int32_t r = 0; r < refls[s]->ReadOnlyResources.count; r++)
      {
        // bindPoint is the uniform value for this sampler
        if(mappings[s]->ReadOnlyResources[refls[s]->ReadOnlyResources[r].bindPoint].bind == unit)
        {
          GLenum t = eGL_NONE;

          if(strstr(refls[s]->ReadOnlyResources[r].variableType.descriptor.name.elems, "Shadow"))
            shadow = true;

          switch(refls[s]->ReadOnlyResources[r].resType)
          {
            case TextureDim::Unknown: target = eGL_NONE; break;
            case TextureDim::Buffer: target = eGL_TEXTURE_BUFFER; break;
            case TextureDim::Texture1D: target = eGL_TEXTURE_1D; break;
            case TextureDim::Texture1DArray: target = eGL_TEXTURE_1D_ARRAY; break;
            case TextureDim::Texture2D: target = eGL_TEXTURE_2D; break;
            case TextureDim::TextureRect: target = eGL_TEXTURE_RECTANGLE; break;
            case TextureDim::Texture2DArray: target = eGL_TEXTURE_2D_ARRAY; break;
            case TextureDim::Texture2DMS: target = eGL_TEXTURE_2D_MULTISAMPLE; break;
            case TextureDim::Texture2DMSArray: target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
            case TextureDim::Texture3D: target = eGL_TEXTURE_3D; break;
            case TextureDim::TextureCube: target = eGL_TEXTURE_CUBE_MAP; break;
            case TextureDim::TextureCubeArray: target = eGL_TEXTURE_CUBE_MAP_ARRAY; break;
            case TextureDim::Count: RDCERR("Invalid shader resource type"); break;
          }

          if(target != eGL_NONE)
            t = TextureBinding(target);

          resType = refls[s]->ReadOnlyResources[r].resType;

          if(binding == eGL_NONE)
          {
            binding = t;
          }
          else if(binding == t)
          {
            // two uniforms with the same type pointing to the same slot is fine
            binding = t;
          }
          else if(binding != t)
          {
            RDCWARN("Two uniforms pointing to texture unit %d with types %s and %s", unit,
                    ToStr::Get(binding).c_str(), ToStr::Get(t).c_str());
          }
        }
      }
    }

    if(binding != eGL_NONE)
    {
      gl.glActiveTexture(GLenum(eGL_TEXTURE0 + unit));

      GLuint tex = 0;

      if(binding == eGL_TEXTURE_CUBE_MAP_ARRAY && !HasExt[ARB_texture_cube_map_array])
        tex = 0;
      else
        gl.glGetIntegerv(binding, (GLint *)&tex);

      if(tex == 0)
      {
        pipe.Textures[unit].Resource = ResourceId();
        pipe.Textures[unit].FirstSlice = 0;
        pipe.Textures[unit].ResType = TextureDim::Unknown;
        pipe.Textures[unit].DepthReadChannel = -1;
        pipe.Textures[unit].Swizzle[0] = TextureSwizzle::Red;
        pipe.Textures[unit].Swizzle[1] = TextureSwizzle::Green;
        pipe.Textures[unit].Swizzle[2] = TextureSwizzle::Blue;
        pipe.Textures[unit].Swizzle[3] = TextureSwizzle::Alpha;

        RDCEraseEl(pipe.Samplers[unit].BorderColor);
        pipe.Samplers[unit].AddressS = AddressMode::Wrap;
        pipe.Samplers[unit].AddressT = AddressMode::Wrap;
        pipe.Samplers[unit].AddressR = AddressMode::Wrap;
        pipe.Samplers[unit].Comparison = CompareFunc::AlwaysTrue;
        pipe.Samplers[unit].Filter = TextureFilter();
        pipe.Samplers[unit].SeamlessCube = false;
        pipe.Samplers[unit].MaxAniso = 0.0f;
        pipe.Samplers[unit].MaxLOD = 0.0f;
        pipe.Samplers[unit].MinLOD = 0.0f;
        pipe.Samplers[unit].MipLODBias = 0.0f;
      }
      else
      {
        // very bespoke/specific
        GLint firstSlice = 0, firstMip = 0;

        if(target != eGL_TEXTURE_BUFFER && HasExt[ARB_texture_view])
        {
          gl.glGetTexParameteriv(target, eGL_TEXTURE_VIEW_MIN_LEVEL, &firstMip);
          gl.glGetTexParameteriv(target, eGL_TEXTURE_VIEW_MIN_LAYER, &firstSlice);
        }

        pipe.Textures[unit].Resource = rm->GetOriginalID(rm->GetID(TextureRes(ctx, tex)));
        pipe.Textures[unit].HighestMip = (uint32_t)firstMip;
        pipe.Textures[unit].FirstSlice = (uint32_t)firstSlice;
        pipe.Textures[unit].ResType = resType;

        pipe.Textures[unit].DepthReadChannel = -1;

        GLenum levelQueryType =
            target == eGL_TEXTURE_CUBE_MAP ? eGL_TEXTURE_CUBE_MAP_POSITIVE_X : target;
        GLenum fmt = eGL_NONE;
        gl.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
        fmt = GetSizedFormat(gl.GetHookset(), target, fmt);
        if(IsDepthStencilFormat(fmt))
        {
          GLint depthMode = eGL_DEPTH_COMPONENT;

          if(HasExt[ARB_stencil_texturing])
            gl.glGetTexParameteriv(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &depthMode);

          if(depthMode == eGL_DEPTH_COMPONENT)
            pipe.Textures[unit].DepthReadChannel = 0;
          else if(depthMode == eGL_STENCIL_INDEX)
            pipe.Textures[unit].DepthReadChannel = 1;
        }

        GLint swizzles[4] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};
        if(target != eGL_TEXTURE_BUFFER &&
           (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
          GetTextureSwizzle(gl.GetHookset(), tex, target, (GLenum *)swizzles);

        for(int i = 0; i < 4; i++)
        {
          switch(swizzles[i])
          {
            default:
            case GL_ZERO: pipe.Textures[unit].Swizzle[i] = TextureSwizzle::Zero; break;
            case GL_ONE: pipe.Textures[unit].Swizzle[i] = TextureSwizzle::One; break;
            case eGL_RED: pipe.Textures[unit].Swizzle[i] = TextureSwizzle::Red; break;
            case eGL_GREEN: pipe.Textures[unit].Swizzle[i] = TextureSwizzle::Green; break;
            case eGL_BLUE: pipe.Textures[unit].Swizzle[i] = TextureSwizzle::Blue; break;
            case eGL_ALPHA: pipe.Textures[unit].Swizzle[i] = TextureSwizzle::Alpha; break;
          }
        }

        GLuint samp = 0;
        if(HasExt[ARB_sampler_objects])
          gl.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&samp);

        pipe.Samplers[unit].Samp = rm->GetOriginalID(rm->GetID(SamplerRes(ctx, samp)));

        if(target != eGL_TEXTURE_BUFFER)
        {
          if(samp != 0)
            gl.glGetSamplerParameterfv(samp, eGL_TEXTURE_BORDER_COLOR,
                                       &pipe.Samplers[unit].BorderColor[0]);
          else
            gl.glGetTexParameterfv(target, eGL_TEXTURE_BORDER_COLOR,
                                   &pipe.Samplers[unit].BorderColor[0]);

          GLint v;
          v = 0;
          if(samp != 0)
            gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_S, &v);
          else
            gl.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_S, &v);
          pipe.Samplers[unit].AddressS = MakeAddressMode((GLenum)v);

          v = 0;
          if(samp != 0)
            gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_T, &v);
          else
            gl.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_T, &v);
          pipe.Samplers[unit].AddressT = MakeAddressMode((GLenum)v);

          v = 0;
          if(samp != 0)
            gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_R, &v);
          else
            gl.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_R, &v);
          pipe.Samplers[unit].AddressR = MakeAddressMode((GLenum)v);

          v = 0;
          if(HasExt[ARB_seamless_cubemap_per_texture])
          {
            if(samp != 0)
              gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_CUBE_MAP_SEAMLESS, &v);
            else
              gl.glGetTexParameteriv(target, eGL_TEXTURE_CUBE_MAP_SEAMLESS, &v);
          }
          pipe.Samplers[unit].SeamlessCube =
              (v != 0 || rs.Enabled[GLRenderState::eEnabled_TexCubeSeamless]);

          v = 0;
          if(samp != 0)
            gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_COMPARE_FUNC, &v);
          else
            gl.glGetTexParameteriv(target, eGL_TEXTURE_COMPARE_FUNC, &v);
          pipe.Samplers[unit].Comparison = MakeCompareFunc((GLenum)v);

          GLint minf = 0;
          GLint magf = 0;
          if(samp != 0)
            gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_MIN_FILTER, &minf);
          else
            gl.glGetTexParameteriv(target, eGL_TEXTURE_MIN_FILTER, &minf);

          if(samp != 0)
            gl.glGetSamplerParameteriv(samp, eGL_TEXTURE_MAG_FILTER, &magf);
          else
            gl.glGetTexParameteriv(target, eGL_TEXTURE_MAG_FILTER, &magf);

          if(HasExt[EXT_texture_filter_anisotropic])
          {
            if(samp != 0)
              gl.glGetSamplerParameterfv(samp, eGL_TEXTURE_MAX_ANISOTROPY_EXT,
                                         &pipe.Samplers[unit].MaxAniso);
            else
              gl.glGetTexParameterfv(target, eGL_TEXTURE_MAX_ANISOTROPY_EXT,
                                     &pipe.Samplers[unit].MaxAniso);
          }
          else
          {
            pipe.Samplers[unit].MaxAniso = 0.0f;
          }

          pipe.Samplers[unit].Filter =
              MakeFilter((GLenum)minf, (GLenum)magf, shadow, pipe.Samplers[unit].MaxAniso);

          gl.glGetTexParameterfv(target, eGL_TEXTURE_MAX_LOD, &pipe.Samplers[unit].MaxLOD);
          gl.glGetTexParameterfv(target, eGL_TEXTURE_MIN_LOD, &pipe.Samplers[unit].MinLOD);
          if(!IsGLES)
            gl.glGetTexParameterfv(target, eGL_TEXTURE_LOD_BIAS, &pipe.Samplers[unit].MipLODBias);
          else
            pipe.Samplers[unit].MipLODBias = 0.0f;
        }
        else
        {
          // texture buffers don't support sampling
          RDCEraseEl(pipe.Samplers[unit].BorderColor);
          pipe.Samplers[unit].AddressS = AddressMode::Wrap;
          pipe.Samplers[unit].AddressT = AddressMode::Wrap;
          pipe.Samplers[unit].AddressR = AddressMode::Wrap;
          pipe.Samplers[unit].Comparison = CompareFunc::AlwaysTrue;
          pipe.Samplers[unit].Filter = TextureFilter();
          pipe.Samplers[unit].SeamlessCube = false;
          pipe.Samplers[unit].MaxAniso = 0.0f;
          pipe.Samplers[unit].MaxLOD = 0.0f;
          pipe.Samplers[unit].MinLOD = 0.0f;
          pipe.Samplers[unit].MipLODBias = 0.0f;
        }
      }
    }
    else
    {
      // what should we do in this case? there could be something bound just not used,
      // it'd be nice to return that
    }
  }

  gl.glActiveTexture(activeTexture);

  create_array_uninit(pipe.UniformBuffers, ARRAY_COUNT(rs.UniformBinding));
  for(int32_t b = 0; b < pipe.UniformBuffers.count; b++)
  {
    if(rs.UniformBinding[b].name == 0)
    {
      pipe.UniformBuffers[b].Resource = ResourceId();
      pipe.UniformBuffers[b].Offset = pipe.UniformBuffers[b].Size = 0;
    }
    else
    {
      pipe.UniformBuffers[b].Resource =
          rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.UniformBinding[b].name)));
      pipe.UniformBuffers[b].Offset = rs.UniformBinding[b].start;
      pipe.UniformBuffers[b].Size = rs.UniformBinding[b].size;
    }
  }

  create_array_uninit(pipe.AtomicBuffers, ARRAY_COUNT(rs.AtomicCounter));
  for(int32_t b = 0; b < pipe.AtomicBuffers.count; b++)
  {
    if(rs.AtomicCounter[b].name == 0)
    {
      pipe.AtomicBuffers[b].Resource = ResourceId();
      pipe.AtomicBuffers[b].Offset = pipe.AtomicBuffers[b].Size = 0;
    }
    else
    {
      pipe.AtomicBuffers[b].Resource =
          rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.AtomicCounter[b].name)));
      pipe.AtomicBuffers[b].Offset = rs.AtomicCounter[b].start;
      pipe.AtomicBuffers[b].Size = rs.AtomicCounter[b].size;
    }
  }

  create_array_uninit(pipe.ShaderStorageBuffers, ARRAY_COUNT(rs.ShaderStorage));
  for(int32_t b = 0; b < pipe.ShaderStorageBuffers.count; b++)
  {
    if(rs.ShaderStorage[b].name == 0)
    {
      pipe.ShaderStorageBuffers[b].Resource = ResourceId();
      pipe.ShaderStorageBuffers[b].Offset = pipe.ShaderStorageBuffers[b].Size = 0;
    }
    else
    {
      pipe.ShaderStorageBuffers[b].Resource =
          rm->GetOriginalID(rm->GetID(BufferRes(ctx, rs.ShaderStorage[b].name)));
      pipe.ShaderStorageBuffers[b].Offset = rs.ShaderStorage[b].start;
      pipe.ShaderStorageBuffers[b].Size = rs.ShaderStorage[b].size;
    }
  }

  create_array_uninit(pipe.Images, ARRAY_COUNT(rs.Images));
  for(int32_t i = 0; i < pipe.Images.count; i++)
  {
    if(rs.Images[i].name == 0)
    {
      RDCEraseEl(pipe.Images[i]);
    }
    else
    {
      ResourceId id = rm->GetID(TextureRes(ctx, rs.Images[i].name));
      pipe.Images[i].Resource = rm->GetOriginalID(id);
      pipe.Images[i].Level = rs.Images[i].level;
      pipe.Images[i].Layered = rs.Images[i].layered;
      pipe.Images[i].Layer = rs.Images[i].layer;
      if(rs.Images[i].access == eGL_READ_ONLY)
      {
        pipe.Images[i].readAllowed = true;
        pipe.Images[i].writeAllowed = false;
      }
      else if(rs.Images[i].access == eGL_WRITE_ONLY)
      {
        pipe.Images[i].readAllowed = false;
        pipe.Images[i].writeAllowed = true;
      }
      else
      {
        pipe.Images[i].readAllowed = true;
        pipe.Images[i].writeAllowed = true;
      }
      pipe.Images[i].Format =
          MakeResourceFormat(gl.GetHookset(), eGL_TEXTURE_2D, rs.Images[i].format);

      pipe.Images[i].ResType = m_CachedTextures[id].resType;
    }
  }

  // Vertex post processing and rasterization

  RDCCOMPILE_ASSERT(ARRAY_COUNT(rs.Viewports) == ARRAY_COUNT(rs.DepthRanges),
                    "GL Viewport count does not match depth ranges count");
  create_array_uninit(pipe.m_Rasterizer.Viewports, ARRAY_COUNT(rs.Viewports));
  for(int32_t v = 0; v < pipe.m_Rasterizer.Viewports.count; ++v)
  {
    pipe.m_Rasterizer.Viewports[v].Left = rs.Viewports[v].x;
    pipe.m_Rasterizer.Viewports[v].Bottom = rs.Viewports[v].y;
    pipe.m_Rasterizer.Viewports[v].Width = rs.Viewports[v].width;
    pipe.m_Rasterizer.Viewports[v].Height = rs.Viewports[v].height;
    pipe.m_Rasterizer.Viewports[v].MinDepth = rs.DepthRanges[v].nearZ;
    pipe.m_Rasterizer.Viewports[v].MaxDepth = rs.DepthRanges[v].farZ;
  }

  create_array_uninit(pipe.m_Rasterizer.Scissors, ARRAY_COUNT(rs.Scissors));
  for(int32_t s = 0; s < pipe.m_Rasterizer.Scissors.count; ++s)
  {
    pipe.m_Rasterizer.Scissors[s].Left = rs.Scissors[s].x;
    pipe.m_Rasterizer.Scissors[s].Bottom = rs.Scissors[s].y;
    pipe.m_Rasterizer.Scissors[s].Width = rs.Scissors[s].width;
    pipe.m_Rasterizer.Scissors[s].Height = rs.Scissors[s].height;
    pipe.m_Rasterizer.Scissors[s].Enabled = rs.Scissors[s].enabled;
  }

  int polygonOffsetEnableEnum;
  switch(rs.PolygonMode)
  {
    default:
      RDCWARN("Unexpected value for POLYGON_MODE %x", rs.PolygonMode);
    // fall through
    case eGL_FILL:
      pipe.m_Rasterizer.m_State.fillMode = FillMode::Solid;
      polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetFill;
      break;
    case eGL_LINE:
      pipe.m_Rasterizer.m_State.fillMode = FillMode::Wireframe;
      polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetLine;
      break;
    case eGL_POINT:
      pipe.m_Rasterizer.m_State.fillMode = FillMode::Point;
      polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetPoint;
      break;
  }
  if(rs.Enabled[polygonOffsetEnableEnum])
  {
    pipe.m_Rasterizer.m_State.DepthBias = rs.PolygonOffset[1];
    pipe.m_Rasterizer.m_State.SlopeScaledDepthBias = rs.PolygonOffset[0];
    pipe.m_Rasterizer.m_State.OffsetClamp = rs.PolygonOffset[2];
  }
  else
  {
    pipe.m_Rasterizer.m_State.DepthBias = 0.0f;
    pipe.m_Rasterizer.m_State.SlopeScaledDepthBias = 0.0f;
    pipe.m_Rasterizer.m_State.OffsetClamp = 0.0f;
  }

  if(rs.Enabled[GLRenderState::eEnabled_CullFace])
  {
    switch(rs.CullFace)
    {
      default:
        RDCWARN("Unexpected value for CULL_FACE %x", rs.CullFace);
      // fall through
      case eGL_BACK: pipe.m_Rasterizer.m_State.cullMode = CullMode::Back; break;
      case eGL_FRONT: pipe.m_Rasterizer.m_State.cullMode = CullMode::Front; break;
      case eGL_FRONT_AND_BACK: pipe.m_Rasterizer.m_State.cullMode = CullMode::FrontAndBack; break;
    }
  }
  else
  {
    pipe.m_Rasterizer.m_State.cullMode = CullMode::NoCull;
  }

  RDCASSERT(rs.FrontFace == eGL_CCW || rs.FrontFace == eGL_CW);
  pipe.m_Rasterizer.m_State.FrontCCW = rs.FrontFace == eGL_CCW;
  pipe.m_Rasterizer.m_State.DepthClamp = rs.Enabled[GLRenderState::eEnabled_DepthClamp];

  pipe.m_Rasterizer.m_State.MultisampleEnable = rs.Enabled[GLRenderState::eEnabled_Multisample];
  pipe.m_Rasterizer.m_State.SampleShading = rs.Enabled[GLRenderState::eEnabled_SampleShading];
  pipe.m_Rasterizer.m_State.SampleMask = rs.Enabled[GLRenderState::eEnabled_SampleMask];
  pipe.m_Rasterizer.m_State.SampleMaskValue =
      rs.SampleMask[0];    // assume number of samples is less than 32
  pipe.m_Rasterizer.m_State.SampleCoverage = rs.Enabled[GLRenderState::eEnabled_SampleCoverage];
  pipe.m_Rasterizer.m_State.SampleCoverageInvert = rs.SampleCoverageInvert;
  pipe.m_Rasterizer.m_State.SampleCoverageValue = rs.SampleCoverage;
  pipe.m_Rasterizer.m_State.SampleAlphaToCoverage =
      rs.Enabled[GLRenderState::eEnabled_SampleAlphaToCoverage];
  pipe.m_Rasterizer.m_State.SampleAlphaToOne = rs.Enabled[GLRenderState::eEnabled_SampleAlphaToOne];
  pipe.m_Rasterizer.m_State.MinSampleShadingRate = rs.MinSampleShading;

  pipe.m_Rasterizer.m_State.ProgrammablePointSize = rs.Enabled[rs.eEnabled_ProgramPointSize];
  pipe.m_Rasterizer.m_State.PointSize = rs.PointSize;
  pipe.m_Rasterizer.m_State.LineWidth = rs.LineWidth;
  pipe.m_Rasterizer.m_State.PointFadeThreshold = rs.PointFadeThresholdSize;
  pipe.m_Rasterizer.m_State.PointOriginUpperLeft = (rs.PointSpriteOrigin != eGL_LOWER_LEFT);

  // depth and stencil states

  pipe.m_DepthState.DepthEnable = rs.Enabled[GLRenderState::eEnabled_DepthTest];
  pipe.m_DepthState.DepthWrites = rs.DepthWriteMask != 0;
  pipe.m_DepthState.DepthFunc = MakeCompareFunc(rs.DepthFunc);

  pipe.m_DepthState.DepthBounds = rs.Enabled[GLRenderState::eEnabled_DepthBoundsEXT];
  pipe.m_DepthState.NearBound = rs.DepthBounds.nearZ;
  pipe.m_DepthState.FarBound = rs.DepthBounds.farZ;

  pipe.m_StencilState.StencilEnable = rs.Enabled[GLRenderState::eEnabled_StencilTest];
  pipe.m_StencilState.m_FrontFace.ValueMask = rs.StencilFront.valuemask;
  pipe.m_StencilState.m_FrontFace.WriteMask = rs.StencilFront.writemask;
  pipe.m_StencilState.m_FrontFace.Ref = uint8_t(rs.StencilFront.ref & 0xff);
  pipe.m_StencilState.m_FrontFace.Func = MakeCompareFunc(rs.StencilFront.func);
  pipe.m_StencilState.m_FrontFace.PassOp = MakeStencilOp(rs.StencilFront.pass);
  pipe.m_StencilState.m_FrontFace.FailOp = MakeStencilOp(rs.StencilFront.stencilFail);
  pipe.m_StencilState.m_FrontFace.DepthFailOp = MakeStencilOp(rs.StencilFront.depthFail);
  pipe.m_StencilState.m_BackFace.ValueMask = rs.StencilBack.valuemask;
  pipe.m_StencilState.m_BackFace.WriteMask = rs.StencilBack.writemask;
  pipe.m_StencilState.m_BackFace.Ref = uint8_t(rs.StencilBack.ref & 0xff);
  pipe.m_StencilState.m_BackFace.Func = MakeCompareFunc(rs.StencilBack.func);
  pipe.m_StencilState.m_BackFace.PassOp = MakeStencilOp(rs.StencilBack.pass);
  pipe.m_StencilState.m_BackFace.FailOp = MakeStencilOp(rs.StencilBack.stencilFail);
  pipe.m_StencilState.m_BackFace.DepthFailOp = MakeStencilOp(rs.StencilBack.depthFail);

  // Frame buffer

  GLuint curDrawFBO = 0;
  gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
  GLuint curReadFBO = 0;
  gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

  GLint numCols = 8;
  gl.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

  bool rbCol[32] = {false};
  bool rbDepth = false;
  bool rbStencil = false;
  GLuint curCol[32] = {0};
  GLuint curDepth = 0;
  GLuint curStencil = 0;

  RDCASSERT(numCols <= 32);

  // we should never bind the true default framebuffer - if the app did, we will have our fake bound
  RDCASSERT(curDrawFBO != 0);
  RDCASSERT(curReadFBO != 0);

  {
    GLenum type = eGL_TEXTURE;
    for(GLint i = 0; i < numCols; i++)
    {
      gl.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curCol[i]);
      gl.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
      if(type == eGL_RENDERBUFFER)
        rbCol[i] = true;
    }

    gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                             (GLint *)&curDepth);
    gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbDepth = true;
    gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                             (GLint *)&curStencil);
    gl.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbStencil = true;

    pipe.m_FB.m_DrawFBO.Obj = rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curDrawFBO)));
    create_array_uninit(pipe.m_FB.m_DrawFBO.Color, numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      ResourceId id =
          rm->GetID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i]));

      pipe.m_FB.m_DrawFBO.Color[i].Obj = rm->GetOriginalID(id);

      if(pipe.m_FB.m_DrawFBO.Color[i].Obj != ResourceId() && !rbCol[i])
        GetFramebufferMipAndLayer(gl.GetHookset(), eGL_DRAW_FRAMEBUFFER,
                                  GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                  (GLint *)&pipe.m_FB.m_DrawFBO.Color[i].Mip,
                                  (GLint *)&pipe.m_FB.m_DrawFBO.Color[i].Layer);

      GLint swizzles[4] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};
      if(!rbCol[i] && id != ResourceId() &&
         (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
      {
        GLenum target = m_pDriver->m_Textures[id].curType;
        GetTextureSwizzle(gl.GetHookset(), curCol[i], target, (GLenum *)swizzles);
      }

      for(int s = 0; s < 4; s++)
      {
        switch(swizzles[s])
        {
          default:
          case GL_ZERO: pipe.m_FB.m_DrawFBO.Color[i].Swizzle[s] = TextureSwizzle::Zero; break;
          case GL_ONE: pipe.m_FB.m_DrawFBO.Color[i].Swizzle[s] = TextureSwizzle::One; break;
          case eGL_RED: pipe.m_FB.m_DrawFBO.Color[i].Swizzle[s] = TextureSwizzle::Red; break;
          case eGL_GREEN: pipe.m_FB.m_DrawFBO.Color[i].Swizzle[s] = TextureSwizzle::Green; break;
          case eGL_BLUE: pipe.m_FB.m_DrawFBO.Color[i].Swizzle[s] = TextureSwizzle::Blue; break;
          case eGL_ALPHA: pipe.m_FB.m_DrawFBO.Color[i].Swizzle[s] = TextureSwizzle::Alpha; break;
        }
      }
    }

    pipe.m_FB.m_DrawFBO.Depth.Obj = rm->GetOriginalID(
        rm->GetID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
    pipe.m_FB.m_DrawFBO.Stencil.Obj = rm->GetOriginalID(
        rm->GetID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

    if(pipe.m_FB.m_DrawFBO.Depth.Obj != ResourceId() && !rbDepth)
      GetFramebufferMipAndLayer(gl.GetHookset(), eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                (GLint *)&pipe.m_FB.m_DrawFBO.Depth.Mip,
                                (GLint *)&pipe.m_FB.m_DrawFBO.Depth.Layer);

    if(pipe.m_FB.m_DrawFBO.Stencil.Obj != ResourceId() && !rbStencil)
      GetFramebufferMipAndLayer(gl.GetHookset(), eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                (GLint *)&pipe.m_FB.m_DrawFBO.Stencil.Mip,
                                (GLint *)&pipe.m_FB.m_DrawFBO.Stencil.Layer);

    create_array_uninit(pipe.m_FB.m_DrawFBO.DrawBuffers, numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      GLenum b = eGL_NONE;
      gl.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&b);
      if(b >= eGL_COLOR_ATTACHMENT0 && b <= GLenum(eGL_COLOR_ATTACHMENT0 + numCols))
        pipe.m_FB.m_DrawFBO.DrawBuffers[i] = b - eGL_COLOR_ATTACHMENT0;
      else
        pipe.m_FB.m_DrawFBO.DrawBuffers[i] = -1;
    }

    pipe.m_FB.m_DrawFBO.ReadBuffer = -1;
  }

  {
    GLenum type = eGL_TEXTURE;
    for(GLint i = 0; i < numCols; i++)
    {
      gl.glGetFramebufferAttachmentParameteriv(
          eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curCol[i]);
      gl.glGetFramebufferAttachmentParameteriv(
          eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
      if(type == eGL_RENDERBUFFER)
        rbCol[i] = true;
    }

    gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                             (GLint *)&curDepth);
    gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbDepth = true;
    gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                             (GLint *)&curStencil);
    gl.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                             eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbStencil = true;

    pipe.m_FB.m_ReadFBO.Obj = rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curReadFBO)));
    create_array_uninit(pipe.m_FB.m_ReadFBO.Color, numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      pipe.m_FB.m_ReadFBO.Color[i].Obj = rm->GetOriginalID(
          rm->GetID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i])));

      if(pipe.m_FB.m_ReadFBO.Color[i].Obj != ResourceId() && !rbCol[i])
        GetFramebufferMipAndLayer(gl.GetHookset(), eGL_READ_FRAMEBUFFER,
                                  GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                  (GLint *)&pipe.m_FB.m_ReadFBO.Color[i].Mip,
                                  (GLint *)&pipe.m_FB.m_ReadFBO.Color[i].Layer);
    }

    pipe.m_FB.m_ReadFBO.Depth.Obj = rm->GetOriginalID(
        rm->GetID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
    pipe.m_FB.m_ReadFBO.Stencil.Obj = rm->GetOriginalID(
        rm->GetID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

    if(pipe.m_FB.m_ReadFBO.Depth.Obj != ResourceId() && !rbDepth)
      GetFramebufferMipAndLayer(gl.GetHookset(), eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                (GLint *)&pipe.m_FB.m_ReadFBO.Depth.Mip,
                                (GLint *)&pipe.m_FB.m_ReadFBO.Depth.Layer);

    if(pipe.m_FB.m_ReadFBO.Stencil.Obj != ResourceId() && !rbStencil)
      GetFramebufferMipAndLayer(gl.GetHookset(), eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                (GLint *)&pipe.m_FB.m_ReadFBO.Stencil.Mip,
                                (GLint *)&pipe.m_FB.m_ReadFBO.Stencil.Layer);

    create_array_uninit(pipe.m_FB.m_ReadFBO.DrawBuffers, numCols);
    for(GLint i = 0; i < numCols; i++)
      pipe.m_FB.m_ReadFBO.DrawBuffers[i] = -1;

    GLenum b = eGL_NONE;
    gl.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&b);
    if(b >= eGL_COLOR_ATTACHMENT0 && b <= GLenum(eGL_COLOR_ATTACHMENT0 + numCols))
      pipe.m_FB.m_DrawFBO.ReadBuffer = b - eGL_COLOR_ATTACHMENT0;
    else
      pipe.m_FB.m_DrawFBO.ReadBuffer = -1;
  }

  memcpy(pipe.m_FB.m_Blending.BlendFactor, rs.BlendColor, sizeof(rs.BlendColor));

  pipe.m_FB.FramebufferSRGB = rs.Enabled[GLRenderState::eEnabled_FramebufferSRGB];
  pipe.m_FB.Dither = rs.Enabled[GLRenderState::eEnabled_Dither];

  RDCCOMPILE_ASSERT(ARRAY_COUNT(rs.Blends) == ARRAY_COUNT(rs.ColorMasks),
                    "Color masks and blends mismatched");
  create_array_uninit(pipe.m_FB.m_Blending.Blends, ARRAY_COUNT(rs.Blends));
  for(size_t i = 0; i < ARRAY_COUNT(rs.Blends); i++)
  {
    pipe.m_FB.m_Blending.Blends[i].Enabled = rs.Blends[i].Enabled;
    pipe.m_FB.m_Blending.Blends[i].Logic = LogicOp::NoOp;
    if(rs.LogicOp != eGL_NONE && rs.LogicOp != eGL_COPY &&
       rs.Enabled[GLRenderState::eEnabled_ColorLogicOp])
    {
      pipe.m_FB.m_Blending.Blends[i].Logic = MakeLogicOp(rs.LogicOp);
    }

    pipe.m_FB.m_Blending.Blends[i].m_Blend.Source = MakeBlendMultiplier(rs.Blends[i].SourceRGB);
    pipe.m_FB.m_Blending.Blends[i].m_Blend.Destination =
        MakeBlendMultiplier(rs.Blends[i].DestinationRGB);
    pipe.m_FB.m_Blending.Blends[i].m_Blend.Operation = MakeBlendOp(rs.Blends[i].EquationRGB);

    pipe.m_FB.m_Blending.Blends[i].m_AlphaBlend.Source =
        MakeBlendMultiplier(rs.Blends[i].SourceAlpha);
    pipe.m_FB.m_Blending.Blends[i].m_AlphaBlend.Destination =
        MakeBlendMultiplier(rs.Blends[i].DestinationAlpha);
    pipe.m_FB.m_Blending.Blends[i].m_AlphaBlend.Operation = MakeBlendOp(rs.Blends[i].EquationAlpha);

    pipe.m_FB.m_Blending.Blends[i].WriteMask = 0;
    if(rs.ColorMasks[i].red)
      pipe.m_FB.m_Blending.Blends[i].WriteMask |= 1;
    if(rs.ColorMasks[i].green)
      pipe.m_FB.m_Blending.Blends[i].WriteMask |= 2;
    if(rs.ColorMasks[i].blue)
      pipe.m_FB.m_Blending.Blends[i].WriteMask |= 4;
    if(rs.ColorMasks[i].alpha)
      pipe.m_FB.m_Blending.Blends[i].WriteMask |= 8;
  }

  switch(rs.Hints.Derivatives)
  {
    default:
    case eGL_DONT_CARE: pipe.m_Hints.Derivatives = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.m_Hints.Derivatives = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.m_Hints.Derivatives = QualityHint::Fastest; break;
  }

  switch(rs.Hints.LineSmooth)
  {
    default:
    case eGL_DONT_CARE: pipe.m_Hints.LineSmooth = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.m_Hints.LineSmooth = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.m_Hints.LineSmooth = QualityHint::Fastest; break;
  }

  switch(rs.Hints.PolySmooth)
  {
    default:
    case eGL_DONT_CARE: pipe.m_Hints.PolySmooth = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.m_Hints.PolySmooth = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.m_Hints.PolySmooth = QualityHint::Fastest; break;
  }

  switch(rs.Hints.TexCompression)
  {
    default:
    case eGL_DONT_CARE: pipe.m_Hints.TexCompression = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.m_Hints.TexCompression = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.m_Hints.TexCompression = QualityHint::Fastest; break;
  }

  pipe.m_Hints.LineSmoothEnabled = rs.Enabled[GLRenderState::eEnabled_LineSmooth];
  pipe.m_Hints.PolySmoothEnabled = rs.Enabled[GLRenderState::eEnabled_PolySmooth];
}

void GLReplay::FillCBufferValue(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, bool rowMajor,
                                uint32_t offs, uint32_t matStride, const vector<byte> &data,
                                ShaderVariable &outVar)
{
  const byte *bufdata = data.empty() ? NULL : &data[offs];
  size_t datasize = data.size() - offs;
  if(offs > data.size())
    datasize = 0;

  if(bufferBacked)
  {
    size_t rangelen = outVar.rows * outVar.columns * sizeof(float);

    if(outVar.rows > 1 && outVar.columns > 1)
    {
      uint32_t *dest = &outVar.value.uv[0];

      uint32_t majorsize = outVar.columns;
      uint32_t minorsize = outVar.rows;

      if(rowMajor)
      {
        majorsize = outVar.rows;
        minorsize = outVar.columns;
      }

      for(uint32_t c = 0; c < majorsize; c++)
      {
        if(bufdata != 0 && datasize > 0)
          memcpy((byte *)dest, bufdata, RDCMIN(rangelen, minorsize * sizeof(float)));

        datasize -= RDCMIN(datasize, (size_t)matStride);
        if(bufdata != 0)
          bufdata += matStride;
        dest += minorsize;
      }
    }
    else
    {
      if(bufdata != 0 && datasize > 0)
        memcpy(&outVar.value.uv[0], bufdata, RDCMIN(rangelen, datasize));
    }
  }
  else
  {
    switch(outVar.type)
    {
      case VarType::Unknown:
      case VarType::Float: gl.glGetUniformfv(prog, offs, outVar.value.fv); break;
      case VarType::Int: gl.glGetUniformiv(prog, offs, outVar.value.iv); break;
      case VarType::UInt: gl.glGetUniformuiv(prog, offs, outVar.value.uv); break;
      case VarType::Double: gl.glGetUniformdv(prog, offs, outVar.value.dv); break;
    }
  }

  if(!rowMajor)
  {
    if(outVar.type != VarType::Double)
    {
      uint32_t uv[16];
      memcpy(&uv[0], &outVar.value.uv[0], sizeof(uv));

      for(uint32_t r = 0; r < outVar.rows; r++)
        for(uint32_t c = 0; c < outVar.columns; c++)
          outVar.value.uv[r * outVar.columns + c] = uv[c * outVar.rows + r];
    }
    else
    {
      double dv[16];
      memcpy(&dv[0], &outVar.value.dv[0], sizeof(dv));

      for(uint32_t r = 0; r < outVar.rows; r++)
        for(uint32_t c = 0; c < outVar.columns; c++)
          outVar.value.dv[r * outVar.columns + c] = dv[c * outVar.rows + r];
    }
  }
}

void GLReplay::FillCBufferVariables(WrappedOpenGL &gl, GLuint prog, bool bufferBacked,
                                    string prefix, const rdctype::array<ShaderConstant> &variables,
                                    vector<ShaderVariable> &outvars, const vector<byte> &data)
{
  for(int32_t i = 0; i < variables.count; i++)
  {
    auto desc = variables[i].type.descriptor;

    ShaderVariable var;
    var.name = variables[i].name.elems;
    var.rows = desc.rows;
    var.columns = desc.cols;
    var.type = desc.type;

    if(variables[i].type.members.count > 0)
    {
      if(desc.elements == 0)
      {
        vector<ShaderVariable> ov;
        FillCBufferVariables(gl, prog, bufferBacked, prefix + var.name.elems + ".",
                             variables[i].type.members, ov, data);
        var.isStruct = true;
        var.members = ov;
      }
      else
      {
        vector<ShaderVariable> arrelems;
        for(uint32_t a = 0; a < desc.elements; a++)
        {
          ShaderVariable arrEl = var;
          arrEl.name = StringFormat::Fmt("%s[%u]", var.name.elems, a);

          vector<ShaderVariable> ov;
          FillCBufferVariables(gl, prog, bufferBacked, prefix + arrEl.name.elems + ".",
                               variables[i].type.members, ov, data);
          arrEl.members = ov;

          arrEl.isStruct = true;

          arrelems.push_back(arrEl);
        }
        var.members = arrelems;
        var.isStruct = false;
        var.rows = var.columns = 0;
      }
    }
    else
    {
      RDCEraseEl(var.value);

      // need to query offset and strides as there's no way to know what layout was used
      // (and if it's not an std layout it's implementation defined :( )
      string fullname = prefix + var.name.elems;

      GLuint idx = gl.glGetProgramResourceIndex(prog, eGL_UNIFORM, fullname.c_str());

      if(idx == GL_INVALID_INDEX)
      {
        RDCERR("Can't find program resource index for %s", fullname.c_str());
      }
      else
      {
        GLenum props[] = {eGL_OFFSET, eGL_MATRIX_STRIDE, eGL_ARRAY_STRIDE, eGL_LOCATION};
        GLint values[] = {0, 0, 0, 0};

        gl.glGetProgramResourceiv(prog, eGL_UNIFORM, idx, ARRAY_COUNT(props), props,
                                  ARRAY_COUNT(props), NULL, values);

        if(!bufferBacked)
        {
          values[0] = values[3];
          values[2] = 1;
        }

        if(desc.elements == 0)
        {
          FillCBufferValue(gl, prog, bufferBacked, desc.rowMajorStorage ? true : false, values[0],
                           values[1], data, var);
        }
        else
        {
          vector<ShaderVariable> elems;
          for(uint32_t a = 0; a < desc.elements; a++)
          {
            ShaderVariable el = var;
            el.name = StringFormat::Fmt("%s[%u]", var.name.elems, a);

            FillCBufferValue(gl, prog, bufferBacked, desc.rowMajorStorage ? true : false,
                             values[0] + values[2] * a, values[1], data, el);

            el.isStruct = false;

            elems.push_back(el);
          }

          var.members = elems;
          var.isStruct = false;
          var.rows = var.columns = 0;
        }
      }
    }

    outvars.push_back(var);
  }
}

void GLReplay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                    vector<ShaderVariable> &outvars, const vector<byte> &data)
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &shaderDetails = m_pDriver->m_Shaders[shader];

  if((int32_t)cbufSlot >= shaderDetails.reflection.ConstantBlocks.count)
  {
    RDCERR("Requesting invalid constant block");
    return;
  }

  GLuint curProg = 0;
  gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

  if(curProg == 0)
  {
    gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curProg);

    if(curProg == 0)
    {
      RDCERR("No program or pipeline bound");
      return;
    }
    else
    {
      ResourceId id =
          m_pDriver->GetResourceManager()->GetID(ProgramPipeRes(m_ReplayCtx.ctx, curProg));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      size_t s = ShaderIdx(shaderDetails.type);

      curProg =
          m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[s]).name;
    }
  }

  auto cblock = shaderDetails.reflection.ConstantBlocks.elems[cbufSlot];

  FillCBufferVariables(gl, curProg, cblock.bufferBacked ? true : false, "", cblock.variables,
                       outvars, data);
}

byte *GLReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                               const GetTextureDataParams &params, size_t &dataSize)
{
  WrappedOpenGL &gl = *m_pDriver;

  auto &texDetails = m_pDriver->m_Textures[tex];

  byte *ret = NULL;

  GLuint tempTex = 0;

  GLenum texType = texDetails.curType;
  GLuint texname = texDetails.resource.name;
  GLenum intFormat = texDetails.internalFormat;
  GLsizei width = RDCMAX(1, texDetails.width >> mip);
  GLsizei height = RDCMAX(1, texDetails.height >> mip);
  GLsizei depth = RDCMAX(1, texDetails.depth >> mip);
  GLsizei arraysize = 1;
  GLint samples = texDetails.samples;

  if(texType == eGL_NONE)
  {
    RDCERR("Trying to get texture data for unknown ID %llu!", tex);
    dataSize = 0;
    return new byte[0];
  }

  if(texType == eGL_TEXTURE_BUFFER)
  {
    GLuint bufName = 0;
    gl.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_DATA_STORE_BINDING,
                                       (GLint *)&bufName);
    ResourceId id = m_pDriver->GetResourceManager()->GetID(BufferRes(m_pDriver->GetCtx(), bufName));

    GLuint offs = 0, size = 0;
    gl.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_OFFSET,
                                       (GLint *)&offs);
    gl.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_SIZE, (GLint *)&size);

    vector<byte> data;
    GetBufferData(id, offs, size, data);

    dataSize = data.size();
    ret = new byte[dataSize];
    memcpy(ret, &data[0], dataSize);

    return ret;
  }

  if(texType == eGL_TEXTURE_2D_ARRAY || texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
     texType == eGL_TEXTURE_1D_ARRAY || texType == eGL_TEXTURE_CUBE_MAP ||
     texType == eGL_TEXTURE_CUBE_MAP_ARRAY)
  {
    // array size doesn't get mip'd down
    depth = 1;
    arraysize = texDetails.depth;
  }

  if(params.remap && intFormat != eGL_RGBA8 && intFormat != eGL_SRGB8_ALPHA8)
  {
    RDCASSERT(params.remap == eRemap_RGBA8);

    MakeCurrentReplayContext(m_DebugCtx);

    GLenum finalFormat = IsSRGBFormat(intFormat) ? eGL_SRGB8_ALPHA8 : eGL_RGBA8;
    GLenum newtarget = (texType == eGL_TEXTURE_3D ? eGL_TEXTURE_3D : eGL_TEXTURE_2D);

    // create temporary texture of width/height in RGBA8 format to render to
    gl.glGenTextures(1, &tempTex);
    gl.glBindTexture(newtarget, tempTex);
    if(newtarget == eGL_TEXTURE_3D)
      gl.glTextureImage3DEXT(tempTex, newtarget, 0, finalFormat, width, height, depth, 0,
                             GetBaseFormat(finalFormat), GetDataType(finalFormat), NULL);
    else
      gl.glTextureImage2DEXT(tempTex, newtarget, 0, finalFormat, width, height, 0,
                             GetBaseFormat(finalFormat), GetDataType(finalFormat), NULL);
    gl.glTexParameteri(newtarget, eGL_TEXTURE_MAX_LEVEL, 0);

    // create temp framebuffer
    GLuint fbo = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);

    gl.glTexParameteri(newtarget, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    gl.glTexParameteri(newtarget, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    gl.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    gl.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    gl.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_R, eGL_CLAMP_TO_EDGE);
    if(newtarget == eGL_TEXTURE_3D)
      gl.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D, tempTex, 0,
                                0);
    else if(newtarget == eGL_TEXTURE_2D || newtarget == eGL_TEXTURE_2D_MULTISAMPLE)
      gl.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, newtarget, tempTex, 0);
    else
      gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, tempTex, 0);

    float col[] = {0.3f, 0.6f, 0.9f, 1.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    // render to the temp texture to do the downcast
    float oldW = DebugData.outWidth;
    float oldH = DebugData.outHeight;

    DebugData.outWidth = float(width);
    DebugData.outHeight = float(height);

    for(GLsizei d = 0; d < (newtarget == eGL_TEXTURE_3D ? depth : 1); d++)
    {
      TextureDisplay texDisplay;

      texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
      texDisplay.HDRMul = -1.0f;
      texDisplay.linearDisplayAsGamma = false;
      texDisplay.overlay = DebugOverlay::NoOverlay;
      texDisplay.FlipY = false;
      texDisplay.mip = mip;
      texDisplay.sampleIdx = ~0U;
      texDisplay.CustomShader = ResourceId();
      texDisplay.sliceFace = arrayIdx;
      texDisplay.rangemin = params.blackPoint;
      texDisplay.rangemax = params.whitePoint;
      texDisplay.scale = 1.0f;
      texDisplay.texid = tex;
      texDisplay.typeHint = CompType::Typeless;
      texDisplay.rawoutput = false;
      texDisplay.offx = 0;
      texDisplay.offy = 0;

      if(newtarget == eGL_TEXTURE_3D)
      {
        gl.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D, tempTex,
                                  0, (GLint)d);
        texDisplay.sliceFace = (uint32_t)d;
      }

      gl.glViewport(0, 0, width, height);

      RenderTextureInternal(texDisplay, 0);
    }

    DebugData.outWidth = oldW;
    DebugData.outHeight = oldH;

    // rewrite the variables to temporary texture
    texType = newtarget;
    texname = tempTex;
    intFormat = finalFormat;
    if(newtarget != eGL_TEXTURE_3D)
      depth = 1;
    arraysize = 1;
    samples = 1;
    mip = 0;
    arrayIdx = 0;

    gl.glDeleteFramebuffers(1, &fbo);
  }
  else if(params.resolve && samples > 1)
  {
    MakeCurrentReplayContext(m_DebugCtx);

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    // create temporary texture of width/height in same format to render to
    gl.glGenTextures(1, &tempTex);
    gl.glBindTexture(eGL_TEXTURE_2D, tempTex);
    gl.glTextureImage2DEXT(tempTex, eGL_TEXTURE_2D, 0, intFormat, width, height, 0,
                           GetBaseFormat(intFormat), GetDataType(intFormat), NULL);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    // create temp framebuffers
    GLuint fbos[2] = {0};
    gl.glGenFramebuffers(2, fbos);

    gl.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[0]);
    gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, tempTex, 0);

    gl.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[1]);
    if(texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texname, 0, arrayIdx);
    else
      gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texname, 0);

    // do default resolve (framebuffer blit)
    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbos[0]);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, fbos[1]);

    float col[] = {0.3f, 0.4f, 0.5f, 1.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, eGL_NEAREST);

    // rewrite the variables to temporary texture
    texType = eGL_TEXTURE_2D;
    texname = tempTex;
    depth = 1;
    mip = 0;
    arrayIdx = 0;
    arraysize = 1;
    samples = 1;

    gl.glDeleteFramebuffers(2, fbos);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);
  }
  else if(samples > 1)
  {
    MakeCurrentReplayContext(m_DebugCtx);

    // create temporary texture array of width/height in same format to render to,
    // with the same number of array slices as multi samples.
    gl.glGenTextures(1, &tempTex);
    gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, tempTex);
    gl.glTextureImage3DEXT(tempTex, eGL_TEXTURE_2D_ARRAY, 0, intFormat, width, height,
                           arraysize * samples, 0, GetBaseFormat(intFormat), GetDataType(intFormat),
                           NULL);
    gl.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

    // copy multisampled texture to an array
    CopyTex2DMSToArray(tempTex, texname, width, height, arraysize, samples, intFormat);

    // rewrite the variables to temporary texture
    texType = eGL_TEXTURE_2D_ARRAY;
    texname = tempTex;
    depth = 1;
    arraysize = arraysize * samples;
    samples = 1;
  }

  // fetch and return data now
  {
    PixelUnpackState unpack;
    unpack.Fetch(&gl.GetHookset(), true);

    ResetPixelUnpackState(gl.GetHookset(), true, 1);

    if(texType == eGL_RENDERBUFFER)
    {
      // do blit from renderbuffer to texture
      MakeCurrentReplayContext(&m_ReplayCtx);

      GLuint curDrawFBO = 0;
      GLuint curReadFBO = 0;
      gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
      gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
      gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

      GLenum b = GetBaseFormat(texDetails.internalFormat);

      GLbitfield mask = GL_COLOR_BUFFER_BIT;

      if(b == eGL_DEPTH_COMPONENT)
        mask = GL_DEPTH_BUFFER_BIT;
      else if(b == eGL_STENCIL)
        mask = GL_STENCIL_BUFFER_BIT;
      else if(b == eGL_DEPTH_STENCIL)
        mask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;

      gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width,
                           texDetails.height, mask, eGL_NEAREST);

      gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
      gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

      // then proceed to read from the texture
      texname = texDetails.renderbufferReadTex;
      texType = eGL_TEXTURE_2D;

      MakeCurrentReplayContext(m_DebugCtx);
    }

    GLenum binding = TextureBinding(texType);

    GLuint prevtex = 0;
    gl.glGetIntegerv(binding, (GLint *)&prevtex);

    gl.glBindTexture(texType, texname);

    GLenum target = texType;
    if(texType == eGL_TEXTURE_CUBE_MAP)
    {
      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      RDCASSERT(arrayIdx < ARRAY_COUNT(targets));
      target = targets[arrayIdx];
    }

    if(IsCompressedFormat(intFormat))
    {
      dataSize = (size_t)GetCompressedByteSize(width, height, depth, intFormat);

      // contains a single slice
      ret = new byte[dataSize];

      // Note that for array textures we fetch the whole mip level (all slices at that mip). Since
      // GL returns all slices together, we cache it and keep the data around. This is because in
      // many cases we don't just want one slice we want all of them, but to preserve the API
      // querying slice-at-a-time we must cache the results of calling glGetTexImage to avoid
      // allocating the whole N layers N times.

      // check arraysize, since if we remapped or otherwise picked out a slice above, this will now
      // be 1 and we don't have to worry about anything
      if(arraysize > 1)
      {
        // if we don't have this texture cached, delete the previous data
        // we don't have to use anything else as the cache key, because if we still have an array at
        // this point then none of the GetTextureDataParams are relevant - only mip/arrayIdx
        if(m_GetTexturePrevID != tex)
        {
          for(size_t i = 0; i < ARRAY_COUNT(m_GetTexturePrevData); i++)
          {
            delete[] m_GetTexturePrevData[i];
            m_GetTexturePrevData[i] = NULL;
          }
        }

        m_GetTexturePrevID = tex;

        RDCASSERT(mip < ARRAY_COUNT(m_GetTexturePrevData));

        // if we don't have this mip cached, fetch it now
        if(m_GetTexturePrevData[mip] == NULL)
        {
          m_GetTexturePrevData[mip] = new byte[dataSize * arraysize];
          if(IsGLES)
          {
            const vector<byte> &data = texDetails.compressedData[mip];
            if(data.size() == dataSize * arraysize)
              memcpy(m_GetTexturePrevData[mip], data.data(), data.size());
            else
              RDCERR("Different expected and stored compressed texture sizes for array texture!");
          }
          else
          {
            gl.glGetCompressedTexImage(target, mip, m_GetTexturePrevData[mip]);
          }
        }

        // now copy the slice from the cache into ret
        byte *src = m_GetTexturePrevData[mip];
        src += dataSize * arrayIdx;

        memcpy(ret, src, dataSize);
      }
      else
      {
        // for non-arrays we can just readback without caching
        if(IsGLES)
        {
          const vector<byte> &data = texDetails.compressedData[mip];
          if(data.size() == dataSize)
            memcpy(m_GetTexturePrevData[mip], data.data(), data.size());
          else
            RDCERR("Different expected and stored compressed texture sizes!");
        }
        else
        {
          gl.glGetCompressedTexImage(target, mip, m_GetTexturePrevData[mip]);
        }
      }
    }
    else
    {
      GLenum fmt = GetBaseFormat(intFormat);
      GLenum type = GetDataType(intFormat);

      size_t rowSize = GetByteSize(width, 1, 1, fmt, type);
      dataSize = GetByteSize(width, height, depth, fmt, type);
      ret = new byte[dataSize];

      // see above for the logic of handling arrays
      if(arraysize > 1)
      {
        if(m_GetTexturePrevID != tex)
        {
          for(size_t i = 0; i < ARRAY_COUNT(m_GetTexturePrevData); i++)
          {
            delete[] m_GetTexturePrevData[i];
            m_GetTexturePrevData[i] = NULL;
          }
        }

        m_GetTexturePrevID = tex;

        RDCASSERT(mip < ARRAY_COUNT(m_GetTexturePrevData));

        // if we don't have this mip cached, fetch it now
        if(m_GetTexturePrevData[mip] == NULL)
        {
          m_GetTexturePrevData[mip] = new byte[dataSize * arraysize];
          gl.glGetTexImage(target, (GLint)mip, fmt, type, m_GetTexturePrevData[mip]);
        }

        // now copy the slice from the cache into ret
        byte *src = m_GetTexturePrevData[mip];
        src += dataSize * arrayIdx;

        memcpy(ret, src, dataSize);
      }
      else
      {
        gl.glGetTexImage(target, (GLint)mip, fmt, type, ret);
      }

      // if we're saving to disk we make the decision to vertically flip any non-compressed
      // images. This is a bit arbitrary, but really origin top-left is common for all disk
      // formats so we do this flip from bottom-left origin. We only do this for saving to
      // disk so that if we're transferring over the network etc for remote replay, the image
      // order is consistent (and we just need to take care to apply an extra vertical flip
      // for display when proxying).

      if(params.forDiskSave)
      {
        // need to vertically flip the image now to get conventional row ordering
        // we either do this when copying out the slice of interest, or just
        // on its own
        byte *src, *dst;

        byte *row = new byte[rowSize];

        size_t sliceSize = GetByteSize(width, height, 1, fmt, type);

        // invert all slices in a 3D texture
        for(GLsizei d = 0; d < depth; d++)
        {
          dst = ret + d * sliceSize;
          src = dst + (height - 1) * rowSize;

          for(GLsizei i = 0; i<height>> 1; i++)
          {
            memcpy(row, src, rowSize);
            memcpy(src, dst, rowSize);
            memcpy(dst, row, rowSize);

            dst += rowSize;
            src -= rowSize;
          }
        }

        delete[] row;
      }
    }

    unpack.Apply(&gl.GetHookset(), true);

    gl.glBindTexture(texType, prevtex);
  }

  if(tempTex)
    gl.glDeleteTextures(1, &tempTex);

  return ret;
}

void GLReplay::BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                 ShaderStage type, ResourceId *id, string *errors)
{
  if(id == NULL || errors == NULL)
  {
    if(id)
      *id = ResourceId();
    return;
  }

  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  GLenum shtype = eGL_VERTEX_SHADER;
  switch(type)
  {
    case ShaderStage::Vertex: shtype = eGL_VERTEX_SHADER; break;
    case ShaderStage::Tess_Control: shtype = eGL_TESS_CONTROL_SHADER; break;
    case ShaderStage::Tess_Eval: shtype = eGL_TESS_EVALUATION_SHADER; break;
    case ShaderStage::Geometry: shtype = eGL_GEOMETRY_SHADER; break;
    case ShaderStage::Fragment: shtype = eGL_FRAGMENT_SHADER; break;
    case ShaderStage::Compute: shtype = eGL_COMPUTE_SHADER; break;
    default:
    {
      RDCERR("Unknown shader type %u", type);
      if(id)
        *id = ResourceId();
      return;
    }
  }

  const char *src = source.c_str();
  GLuint shaderprog = gl.glCreateShaderProgramv(shtype, 1, &src);

  GLint status = 0;
  gl.glGetProgramiv(shaderprog, eGL_LINK_STATUS, &status);

  if(errors)
  {
    GLint len = 1024;
    gl.glGetProgramiv(shaderprog, eGL_INFO_LOG_LENGTH, &len);
    char *buffer = new char[len + 1];
    gl.glGetProgramInfoLog(shaderprog, len, NULL, buffer);
    buffer[len] = 0;
    *errors = buffer;
    delete[] buffer;
  }

  if(status == 0)
    *id = ResourceId();
  else
    *id = m_pDriver->GetResourceManager()->GetID(ProgramRes(m_pDriver->GetCtx(), shaderprog));
}

ResourceId GLReplay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                       uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint)
{
  if(shader == ResourceId() || texid == ResourceId())
    return ResourceId();

  auto &texDetails = m_pDriver->m_Textures[texid];

  MakeCurrentReplayContext(m_DebugCtx);

  CreateCustomShaderTex(texDetails.width, texDetails.height);

  m_pDriver->glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.customFBO);
  m_pDriver->glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                                    DebugData.customTex, mip);

  m_pDriver->glViewport(0, 0, RDCMAX(1, texDetails.width >> mip),
                        RDCMAX(1, texDetails.height >> mip));

  DebugData.outWidth = float(RDCMAX(1, texDetails.width >> mip));
  DebugData.outHeight = float(RDCMAX(1, texDetails.height >> mip));

  float clr[] = {0.0f, 0.8f, 0.0f, 0.0f};
  m_pDriver->glClearBufferfv(eGL_COLOR, 0, clr);

  TextureDisplay disp;
  disp.Red = disp.Green = disp.Blue = disp.Alpha = true;
  disp.FlipY = false;
  disp.offx = 0.0f;
  disp.offy = 0.0f;
  disp.CustomShader = shader;
  disp.texid = texid;
  disp.typeHint = typeHint;
  disp.lightBackgroundColor = disp.darkBackgroundColor = FloatVector(0, 0, 0, 0);
  disp.HDRMul = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.mip = mip;
  disp.sampleIdx = sampleIdx;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangemin = 0.0f;
  disp.rangemax = 1.0f;
  disp.rawoutput = false;
  disp.scale = 1.0f;
  disp.sliceFace = arrayIdx;

  RenderTextureInternal(disp, eTexDisplay_MipShift);

  return DebugData.CustomShaderTexID;
}

void GLReplay::CreateCustomShaderTex(uint32_t w, uint32_t h)
{
  if(DebugData.customTex)
  {
    uint32_t oldw = 0, oldh = 0;
    m_pDriver->glGetTextureLevelParameterivEXT(DebugData.customTex, eGL_TEXTURE_2D, 0,
                                               eGL_TEXTURE_WIDTH, (GLint *)&oldw);
    m_pDriver->glGetTextureLevelParameterivEXT(DebugData.customTex, eGL_TEXTURE_2D, 0,
                                               eGL_TEXTURE_HEIGHT, (GLint *)&oldh);

    if(oldw == w && oldh == h)
      return;

    m_pDriver->glDeleteTextures(1, &DebugData.customTex);
    DebugData.customTex = 0;
  }

  uint32_t mips = CalcNumMips((int)w, (int)h, 1);

  m_pDriver->glGenTextures(1, &DebugData.customTex);
  m_pDriver->glBindTexture(eGL_TEXTURE_2D, DebugData.customTex);
  for(uint32_t i = 0; i < mips; i++)
  {
    m_pDriver->glTextureImage2DEXT(DebugData.customTex, eGL_TEXTURE_2D, i, eGL_RGBA16F,
                                   (GLsizei)RDCMAX(1U, w >> i), (GLsizei)RDCMAX(1U, h >> i), 0,
                                   eGL_RGBA, eGL_FLOAT, NULL);
  }
  m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_BASE_LEVEL, 0);
  m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, mips - 1);
  m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  m_pDriver->glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

  DebugData.CustomShaderTexID =
      m_pDriver->GetResourceManager()->GetID(TextureRes(m_pDriver->GetCtx(), DebugData.customTex));
}

void GLReplay::FreeCustomShader(ResourceId id)
{
  if(id == ResourceId())
    return;

  m_pDriver->glDeleteProgram(m_pDriver->GetResourceManager()->GetCurrentResource(id).name);
}

void GLReplay::BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                 ShaderStage type, ResourceId *id, string *errors)
{
  if(id == NULL || errors == NULL)
  {
    if(id)
      *id = ResourceId();
    return;
  }

  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  GLenum shtype = eGL_VERTEX_SHADER;
  switch(type)
  {
    case ShaderStage::Vertex: shtype = eGL_VERTEX_SHADER; break;
    case ShaderStage::Tess_Control: shtype = eGL_TESS_CONTROL_SHADER; break;
    case ShaderStage::Tess_Eval: shtype = eGL_TESS_EVALUATION_SHADER; break;
    case ShaderStage::Geometry: shtype = eGL_GEOMETRY_SHADER; break;
    case ShaderStage::Fragment: shtype = eGL_FRAGMENT_SHADER; break;
    case ShaderStage::Compute: shtype = eGL_COMPUTE_SHADER; break;
    default:
    {
      RDCERR("Unknown shader type %u", type);
      if(id)
        *id = ResourceId();
      return;
    }
  }

  const char *src = source.c_str();
  GLuint shader = gl.glCreateShader(shtype);
  gl.glShaderSource(shader, 1, &src, NULL);
  gl.glCompileShader(shader);

  GLint status = 0;
  gl.glGetShaderiv(shader, eGL_COMPILE_STATUS, &status);

  if(errors)
  {
    GLint len = 1024;
    gl.glGetShaderiv(shader, eGL_INFO_LOG_LENGTH, &len);
    char *buffer = new char[len + 1];
    gl.glGetShaderInfoLog(shader, len, NULL, buffer);
    buffer[len] = 0;
    *errors = buffer;
    delete[] buffer;
  }

  if(status == 0)
    *id = ResourceId();
  else
    *id = m_pDriver->GetResourceManager()->GetID(ShaderRes(m_pDriver->GetCtx(), shader));
}

void GLReplay::ReplaceResource(ResourceId from, ResourceId to)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->ReplaceResource(from, to);
}

void GLReplay::RemoveReplacement(ResourceId id)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->RemoveReplacement(id);
}

void GLReplay::FreeTargetResource(ResourceId id)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->FreeTargetResource(id);
}

ResourceId GLReplay::CreateProxyTexture(const TextureDescription &templateTex)
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  GLuint tex = 0;
  gl.glGenTextures(1, &tex);

  GLenum intFormat = MakeGLFormat(gl, templateTex.format);

  GLenum binding = eGL_NONE;

  GLenum baseFormat = eGL_RGBA;
  GLenum dataType = eGL_UNSIGNED_BYTE;
  if(!IsCompressedFormat(intFormat))
  {
    baseFormat = GetBaseFormat(intFormat);
    dataType = GetDataType(intFormat);
  }

  switch(templateTex.resType)
  {
    case TextureDim::Unknown: break;
    case TextureDim::Buffer:
    case TextureDim::Texture1D:
    {
      binding = eGL_TEXTURE_1D;
      gl.glBindTexture(eGL_TEXTURE_1D, tex);
      uint32_t w = templateTex.width;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage1DEXT(tex, eGL_TEXTURE_1D, i, intFormat, w, 0, baseFormat,
                                       dataType, NULL);
        w = RDCMAX(1U, w >> 1);
      }
      break;
    }
    case TextureDim::Texture1DArray:
    {
      binding = eGL_TEXTURE_1D_ARRAY;
      gl.glBindTexture(eGL_TEXTURE_1D_ARRAY, tex);
      uint32_t w = templateTex.width;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_1D_ARRAY, i, intFormat, w,
                                       templateTex.arraysize, 0, baseFormat, dataType, NULL);
        w = RDCMAX(1U, w >> 1);
      }
      break;
    }
    case TextureDim::TextureRect:
    case TextureDim::Texture2D:
    {
      binding = eGL_TEXTURE_2D;
      gl.glBindTexture(eGL_TEXTURE_2D, tex);
      uint32_t w = templateTex.width;
      uint32_t h = templateTex.height;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_2D, i, intFormat, w, h, 0, baseFormat,
                                       dataType, NULL);
        w = RDCMAX(1U, w >> 1);
        h = RDCMAX(1U, h >> 1);
      }
      break;
    }
    case TextureDim::Texture2DArray:
    {
      binding = eGL_TEXTURE_2D_ARRAY;
      gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, tex);
      uint32_t w = templateTex.width;
      uint32_t h = templateTex.height;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage3DEXT(tex, eGL_TEXTURE_2D_ARRAY, i, intFormat, w, h,
                                       templateTex.arraysize, 0, baseFormat, dataType, NULL);
        w = RDCMAX(1U, w >> 1);
        h = RDCMAX(1U, h >> 1);
      }
      break;
    }
    case TextureDim::Texture2DMS:
    {
      binding = eGL_TEXTURE_2D_MULTISAMPLE;
      gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, tex);
      gl.glTextureStorage2DMultisampleEXT(tex, eGL_TEXTURE_2D_MULTISAMPLE, templateTex.msSamp,
                                          intFormat, templateTex.width, templateTex.height, GL_TRUE);
      break;
    }
    case TextureDim::Texture2DMSArray:
    {
      binding = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY;
      gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex);
      gl.glTextureStorage3DMultisampleEXT(tex, eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, templateTex.msSamp,
                                          intFormat, templateTex.width, templateTex.height,
                                          templateTex.arraysize, GL_TRUE);
      break;
    }
    case TextureDim::Texture3D:
    {
      binding = eGL_TEXTURE_3D;
      gl.glBindTexture(eGL_TEXTURE_3D, tex);
      uint32_t w = templateTex.width;
      uint32_t h = templateTex.height;
      uint32_t d = templateTex.depth;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage3DEXT(tex, eGL_TEXTURE_3D, i, intFormat, w, h, d, 0, baseFormat,
                                       dataType, NULL);
        w = RDCMAX(1U, w >> 1);
        h = RDCMAX(1U, h >> 1);
        d = RDCMAX(1U, d >> 1);
      }
      break;
    }
    case TextureDim::TextureCube:
    {
      binding = eGL_TEXTURE_CUBE_MAP;
      gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, tex);
      uint32_t w = templateTex.width;
      uint32_t h = templateTex.height;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_CUBE_MAP_POSITIVE_X, i, intFormat, w, h, 0,
                                       baseFormat, dataType, NULL);
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X, i, intFormat, w, h, 0,
                                       baseFormat, dataType, NULL);
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, i, intFormat, w, h, 0,
                                       baseFormat, dataType, NULL);
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y, i, intFormat, w, h, 0,
                                       baseFormat, dataType, NULL);
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, i, intFormat, w, h, 0,
                                       baseFormat, dataType, NULL);
        m_pDriver->glTextureImage2DEXT(tex, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z, i, intFormat, w, h, 0,
                                       baseFormat, dataType, NULL);
        w = RDCMAX(1U, w >> 1);
        h = RDCMAX(1U, h >> 1);
      }
      break;
    }
    case TextureDim::TextureCubeArray:
    {
      binding = eGL_TEXTURE_CUBE_MAP_ARRAY;
      gl.glBindTexture(eGL_TEXTURE_CUBE_MAP_ARRAY, tex);
      uint32_t w = templateTex.width;
      uint32_t h = templateTex.height;
      for(uint32_t i = 0; i < templateTex.mips; i++)
      {
        m_pDriver->glTextureImage3DEXT(tex, eGL_TEXTURE_2D_ARRAY, i, intFormat, w, h,
                                       templateTex.arraysize, 0, baseFormat, dataType, NULL);
        w = RDCMAX(1U, w >> 1);
        h = RDCMAX(1U, h >> 1);
      }
      break;
    }
    case TextureDim::Count:
    {
      RDCERR("Invalid shader resource type");
      break;
    }
  }

  gl.glTexParameteri(binding, eGL_TEXTURE_MAX_LEVEL, templateTex.mips - 1);

  if(templateTex.format.bgraOrder && binding != eGL_NONE)
  {
    if(HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle])
    {
      GLint bgraSwizzle[] = {eGL_BLUE, eGL_GREEN, eGL_RED, eGL_ALPHA};
      GLint bgrSwizzle[] = {eGL_BLUE, eGL_GREEN, eGL_RED, eGL_ONE};

      if(templateTex.format.compCount == 4)
        SetTextureSwizzle(gl.GetHookset(), tex, binding, (GLenum *)bgraSwizzle);
      else if(templateTex.format.compCount == 3)
        SetTextureSwizzle(gl.GetHookset(), tex, binding, (GLenum *)bgrSwizzle);
      else
        RDCERR("Unexpected component count %d for BGRA order format", templateTex.format.compCount);
    }
    else
    {
      RDCERR("Can't create a BGRA proxy texture without texture swizzle extension");
    }
  }

  ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(m_pDriver->GetCtx(), tex));

  if(templateTex.customName)
    m_pDriver->GetResourceManager()->SetName(id, templateTex.name.c_str());

  return id;
}

void GLReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                   size_t dataSize)
{
  WrappedOpenGL &gl = *m_pDriver;

  GLuint tex = m_pDriver->GetResourceManager()->GetCurrentResource(texid).name;

  auto &texdetails = m_pDriver->m_Textures[texid];

  GLenum fmt = texdetails.internalFormat;
  GLenum target = texdetails.curType;

  if(IsCompressedFormat(fmt))
  {
    if(target == eGL_TEXTURE_1D)
    {
      gl.glCompressedTextureSubImage1DEXT(tex, target, (GLint)mip, 0, texdetails.width, fmt,
                                          (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_1D_ARRAY)
    {
      gl.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, (GLint)arrayIdx,
                                          texdetails.width, 1, fmt, (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D)
    {
      gl.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width,
                                          texdetails.height, fmt, (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    {
      gl.glCompressedTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, (GLint)arrayIdx,
                                          texdetails.width, texdetails.height, 1, fmt,
                                          (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_3D)
    {
      gl.glCompressedTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, 0, texdetails.width,
                                          texdetails.height, texdetails.depth, fmt,
                                          (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_CUBE_MAP)
    {
      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      RDCASSERT(arrayIdx < ARRAY_COUNT(targets));
      target = targets[arrayIdx];

      gl.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width,
                                          texdetails.height, fmt, (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE)
    {
      RDCUNIMPLEMENTED("multisampled proxy textures");
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
      RDCUNIMPLEMENTED("multisampled proxy textures");
    }
  }
  else
  {
    GLenum baseformat = GetBaseFormat(fmt);
    GLenum datatype = GetDataType(fmt);

    GLint depth = 1;
    if(target == eGL_TEXTURE_3D)
      depth = texdetails.depth;

    if(dataSize < GetByteSize(texdetails.width, texdetails.height, depth, baseformat, datatype))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    if(target == eGL_TEXTURE_1D)
    {
      gl.glTextureSubImage1DEXT(tex, target, (GLint)mip, 0, texdetails.width, baseformat, datatype,
                                data);
    }
    else if(target == eGL_TEXTURE_1D_ARRAY)
    {
      gl.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, (GLint)arrayIdx, texdetails.width, 1,
                                baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_2D)
    {
      gl.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width, texdetails.height,
                                baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    {
      gl.glTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, (GLint)arrayIdx, texdetails.width,
                                texdetails.height, 1, baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_3D)
    {
      gl.glTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, 0, texdetails.width,
                                texdetails.height, texdetails.depth, baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_CUBE_MAP)
    {
      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      RDCASSERT(arrayIdx < ARRAY_COUNT(targets));
      target = targets[arrayIdx];

      gl.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, texdetails.width, texdetails.height,
                                baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE)
    {
      RDCUNIMPLEMENTED("multisampled proxy textures");
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
      RDCUNIMPLEMENTED("multisampled proxy textures");
    }
  }
}

bool GLReplay::IsTextureSupported(const ResourceFormat &format)
{
  return true;
}

ResourceId GLReplay::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  GLenum target = eGL_ARRAY_BUFFER;

  if(templateBuf.creationFlags & BufferCategory::Indirect)
    target = eGL_DRAW_INDIRECT_BUFFER;
  if(templateBuf.creationFlags & BufferCategory::Index)
    target = eGL_ELEMENT_ARRAY_BUFFER;
  if(templateBuf.creationFlags & BufferCategory::Constants)
    target = eGL_UNIFORM_BUFFER;
  if(templateBuf.creationFlags & BufferCategory::ReadWrite)
    target = eGL_SHADER_STORAGE_BUFFER;

  GLuint buf = 0;
  gl.glGenBuffers(1, &buf);
  gl.glBindBuffer(target, buf);
  gl.glNamedBufferDataEXT(buf, (GLsizeiptr)templateBuf.length, NULL, eGL_DYNAMIC_DRAW);

  ResourceId id = m_pDriver->GetResourceManager()->GetID(BufferRes(m_pDriver->GetCtx(), buf));

  if(templateBuf.customName)
    m_pDriver->GetResourceManager()->SetName(id, templateBuf.name.c_str());

  return id;
}

void GLReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
  GLuint buf = m_pDriver->GetResourceManager()->GetCurrentResource(bufid).name;

  m_pDriver->glNamedBufferSubDataEXT(buf, 0, dataSize, data);
}

vector<EventUsage> GLReplay::GetUsage(ResourceId id)
{
  return m_pDriver->GetUsage(id);
}

#pragma endregion

vector<PixelModification> GLReplay::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                 uint32_t x, uint32_t y, uint32_t slice,
                                                 uint32_t mip, uint32_t sampleIdx, CompType typeHint)
{
  GLNOTIMP("GLReplay::PixelHistory");
  return vector<PixelModification>();
}

ShaderDebugTrace GLReplay::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                       uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  GLNOTIMP("DebugVertex");
  return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                      uint32_t primitive)
{
  GLNOTIMP("DebugPixel");
  return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                       const uint32_t threadid[3])
{
  GLNOTIMP("DebugThread");
  return ShaderDebugTrace();
}

void GLReplay::MakeCurrentReplayContext(GLWindowingData *ctx)
{
  static GLWindowingData *prev = NULL;

  if(ctx && ctx != prev)
  {
    m_pDriver->m_Platform.MakeContextCurrent(*ctx);
    prev = ctx;
    m_pDriver->ActivateContext(*ctx);
  }
}

void GLReplay::SwapBuffers(GLWindowingData *ctx)
{
  m_pDriver->m_Platform.SwapBuffers(*ctx);
}

void GLReplay::CloseReplayContext()
{
  m_pDriver->m_Platform.DeleteReplayContext(m_ReplayCtx);
}

uint64_t GLReplay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  OutputWindow win = m_pDriver->m_Platform.MakeOutputWindow(system, data, depth, m_ReplayCtx);
  if(!win.wnd)
    return 0;

  m_pDriver->m_Platform.GetOutputWindowDimensions(win, win.width, win.height);

  MakeCurrentReplayContext(&win);
  InitOutputWindow(win);
  CreateOutputWindowBackbuffer(win, depth);

  uint64_t ret = m_OutputWindowID++;

  m_OutputWindows[ret] = win;

  return ret;
}

void GLReplay::DestroyOutputWindow(uint64_t id)
{
  auto it = m_OutputWindows.find(id);
  if(id == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  MakeCurrentReplayContext(&outw);

  WrappedOpenGL &gl = *m_pDriver;
  gl.glDeleteFramebuffers(1, &outw.BlitData.readFBO);

  m_pDriver->m_Platform.DeleteReplayContext(outw);

  m_OutputWindows.erase(it);
}

void GLReplay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return;

  OutputWindow &outw = m_OutputWindows[id];

  m_pDriver->m_Platform.GetOutputWindowDimensions(outw, w, h);
}

bool GLReplay::IsOutputWindowVisible(uint64_t id)
{
  if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
    return false;

  return m_pDriver->m_Platform.IsOutputWindowVisible(m_OutputWindows[id]);
}

#if defined(RENDERDOC_SUPPORT_GL)

// defined in gl_replay_<platform>.cpp
ReplayStatus GL_CreateReplayDevice(const char *logfile, IReplayDriver **driver);

static DriverRegistration GLDriverRegistration(RDC_OpenGL, "OpenGL", &GL_CreateReplayDevice);

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

// defined in gl_replay_egl.cpp
ReplayStatus GLES_CreateReplayDevice(const char *logfile, IReplayDriver **driver);

static DriverRegistration GLESDriverRegistration(RDC_OpenGLES, "OpenGLES", &GLES_CreateReplayDevice);

#endif
