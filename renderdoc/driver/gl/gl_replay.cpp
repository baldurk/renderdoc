/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/intel/intel_gl_counters.h"
#include "maths/matrix.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

static const char *SPIRVDisassemblyTarget = "SPIR-V (RenderDoc)";

GLReplay::GLReplay()
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(GLReplay));

  m_pDriver = NULL;
  m_Proxy = false;

  m_Degraded = false;

  RDCEraseEl(m_ReplayCtx);
  m_DebugCtx = NULL;

  m_DebugID = 0;

  m_OutputWindowID = 1;

  RDCEraseEl(m_GetTexturePrevData);
  RDCEraseEl(m_DriverInfo);
}

void GLReplay::Shutdown()
{
  SAFE_DELETE(m_pAMDCounters);
  SAFE_DELETE(m_pIntelCounters);

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
}

ReplayStatus GLReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  return m_pDriver->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

void GLReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->ReplayLog(0, endEventID, replayType);

  // clear array cache
  for(size_t i = 0; i < ARRAY_COUNT(m_GetTexturePrevData); i++)
  {
    delete[] m_GetTexturePrevData[i];
    m_GetTexturePrevData[i] = NULL;
  }
}

const SDFile &GLReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
}

std::vector<uint32_t> GLReplay::GetPassEvents(uint32_t eventId)
{
  std::vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  const DrawcallDescription *start = draw;
  while(start && start->previous && !(start->previous->flags & DrawFlags::Clear))
  {
    const DrawcallDescription *prev = start->previous;

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
      passEvents.push_back(start->eventId);

    start = start->next;
  }

  return passEvents;
}

std::vector<WindowingSystem> GLReplay::GetSupportedWindowSystems()
{
  std::vector<WindowingSystem> ret;

#if ENABLED(RDOC_LINUX)
  // only Xlib supported for GLX. We can't report XCB here since we need
  // the Display, and that can't be obtained from XCB. The application is
  // free to use XCB internally but it would have to create a hybrid and
  // initialise XCB out of Xlib, to be able to provide the display and
  // drawable to us.
  ret.push_back(WindowingSystem::Xlib);
#elif ENABLED(RDOC_ANDROID)
  ret.push_back(WindowingSystem::Android);
#elif ENABLED(RDOC_APPLE)
  ret.push_back(WindowingSystem::MacOS);
#endif

  return ret;
}

FrameRecord GLReplay::GetFrameRecord()
{
  return m_pDriver->GetFrameRecord();
}

ResourceId GLReplay::GetLiveID(ResourceId id)
{
  if(!m_pDriver->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDriver->GetResourceManager()->GetLiveID(id);
}

APIProperties GLReplay::GetAPIProperties()
{
  APIProperties ret = m_pDriver->APIProps;

  ret.pipelineType = GraphicsAPI::OpenGL;
  ret.localRenderer = GraphicsAPI::OpenGL;
  ret.degraded = m_Degraded;
  ret.vendor = m_DriverInfo.vendor;
  ret.shadersMutable = true;

  return ret;
}

std::vector<ResourceId> GLReplay::GetBuffers()
{
  std::vector<ResourceId> ret;

  for(auto it = m_pDriver->m_Buffers.begin(); it != m_pDriver->m_Buffers.end(); ++it)
  {
    // skip buffers that aren't from the log
    if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  return ret;
}

ResourceDescription &GLReplay::GetResourceDesc(ResourceId id)
{
  auto it = m_ResourceIdx.find(id);
  if(it == m_ResourceIdx.end())
  {
    m_ResourceIdx[id] = m_Resources.size();
    m_Resources.push_back(ResourceDescription());
    m_Resources.back().resourceId = id;
    return m_Resources.back();
  }

  return m_Resources[it->second];
}

const std::vector<ResourceDescription> &GLReplay::GetResources()
{
  return m_Resources;
}

std::vector<ResourceId> GLReplay::GetTextures()
{
  std::vector<ResourceId> ret;
  ret.reserve(m_pDriver->m_Textures.size());

  for(auto it = m_pDriver->m_Textures.begin(); it != m_pDriver->m_Textures.end(); ++it)
  {
    auto &res = m_pDriver->m_Textures[it->first];

    // skip textures that aren't from the log (except the 'default backbuffer' textures)
    if(!(res.creationFlags & TextureCategory::SwapBuffer) &&
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
    m_pDriver->RegisterReplayContext(m_ReplayCtx, NULL, true, true);

  InitDebugData();

  AMDCounters *countersAMD = NULL;
  IntelGlCounters *countersIntel = NULL;

  bool isMesa = false;

  // try to identify mesa - don't enable any IHV counters when running mesa.
  {
    WrappedOpenGL &drv = *m_pDriver;

    const char *version = (const char *)drv.glGetString(eGL_VERSION);
    const char *vendor = (const char *)drv.glGetString(eGL_VENDOR);
    const char *renderer = (const char *)drv.glGetString(eGL_RENDERER);

    for(std::string haystack : {strlower(version), strlower(vendor), strlower(renderer)})
    {
      haystack = " " + haystack + " ";

      // the version should always contain 'mesa', but it's also commonly present in either vendor
      // or renderer - except for nouveau which we look for separately
      for(const char *needle : {" mesa ", "nouveau"})
      {
        if(haystack.find(needle) != std::string::npos)
        {
          isMesa = true;
          break;
        }
      }

      if(isMesa)
        break;
    }
  }

  if(isMesa)
  {
    if(m_DriverInfo.vendor == GPUVendor::Intel)
    {
      RDCLOG("Intel GPU detected - trying to initialise Intel GL counters");
      countersIntel = new IntelGlCounters();
    }
    else
      RDCLOG("Non Intel Mesa driver detected - skipping IHV counter initialisation");
  }
  else
  {
    if(m_DriverInfo.vendor == GPUVendor::AMD)
    {
      RDCLOG("AMD GPU detected - trying to initialise AMD counters");
      countersAMD = new AMDCounters();
    }
    else
    {
      RDCLOG("%s GPU detected - no counters available", ToStr(m_DriverInfo.vendor).c_str());
    }
  }

  if(countersAMD && countersAMD->Init(AMDCounters::ApiType::Ogl, m_ReplayCtx.ctx))
  {
    m_pAMDCounters = countersAMD;
  }
  else
  {
    delete countersAMD;
    m_pAMDCounters = NULL;
  }

  if(countersIntel && countersIntel->Init())
  {
    m_pIntelCounters = countersIntel;
  }
  else
  {
    delete countersIntel;
    m_pIntelCounters = NULL;
  }
}

void GLReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret)
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

  WrappedOpenGL &drv = *m_pDriver;

  GLuint oldbuf = 0;
  drv.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf);

  drv.glBindBuffer(eGL_COPY_READ_BUFFER, buf.resource.name);

  drv.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)len, &ret[0]);

  drv.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf);
}

bool GLReplay::IsRenderOutput(ResourceId id)
{
  for(const GLPipe::Attachment &att : m_CurPipelineState.framebuffer.drawFBO.colorAttachments)
  {
    if(att.resourceId == id)
      return true;
  }

  if(m_CurPipelineState.framebuffer.drawFBO.depthAttachment.resourceId == id ||
     m_CurPipelineState.framebuffer.drawFBO.stencilAttachment.resourceId == id)
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
  if(m_CachedTextures.find(id) != m_CachedTextures.end())
    return;

  TextureDescription tex = {};

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &res = m_pDriver->m_Textures[id];
  WrappedOpenGL &drv = *m_pDriver;

  tex.resourceId = m_pDriver->GetResourceManager()->GetOriginalID(id);

  if(res.resource.Namespace == eResUnknown || res.curType == eGL_NONE)
  {
    if(res.resource.Namespace == eResUnknown)
      RDCERR("Details for invalid texture id %llu requested", id);

    tex.format = ResourceFormat();
    tex.dimension = 1;
    tex.type = TextureType::Unknown;
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
    tex.type = TextureType::Texture2D;
    tex.width = res.width;
    tex.height = res.height;
    tex.depth = 1;
    tex.cubemap = false;
    tex.mips = 1;
    tex.arraysize = 1;
    tex.creationFlags = TextureCategory::ColorTarget;
    tex.msQual = 0;
    tex.msSamp = RDCMAX(1, res.samples);

    tex.format = MakeResourceFormat(eGL_TEXTURE_2D, res.internalFormat);

    if(IsDepthStencilFormat(res.internalFormat))
      tex.creationFlags |= TextureCategory::DepthTarget;

    tex.byteSize = (tex.width * tex.height) * (tex.format.compByteWidth * tex.format.compCount);

    m_CachedTextures[id] = tex;
    return;
  }

  GLenum target = TextureTarget(res.curType);

  GLenum levelQueryType = target;
  if(levelQueryType == eGL_TEXTURE_CUBE_MAP)
    levelQueryType = eGL_TEXTURE_CUBE_MAP_POSITIVE_X;

  GLint width = 1, height = 1, depth = 1, samples = 1;
  drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_WIDTH,
                                      &width);
  drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_HEIGHT,
                                      &height);
  drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_DEPTH,
                                      &depth);
  drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_SAMPLES,
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
    case eGL_TEXTURE_BUFFER: tex.type = TextureType::Buffer; break;
    case eGL_TEXTURE_1D: tex.type = TextureType::Texture1D; break;
    case eGL_TEXTURE_2D: tex.type = TextureType::Texture2D; break;
    case eGL_TEXTURE_3D: tex.type = TextureType::Texture3D; break;
    case eGL_TEXTURE_1D_ARRAY: tex.type = TextureType::Texture1DArray; break;
    case eGL_TEXTURE_2D_ARRAY: tex.type = TextureType::Texture2DArray; break;
    case eGL_TEXTURE_RECTANGLE: tex.type = TextureType::TextureRect; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: tex.type = TextureType::Texture2DMS; break;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: tex.type = TextureType::Texture2DMSArray; break;
    case eGL_TEXTURE_CUBE_MAP: tex.type = TextureType::TextureCube; break;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: tex.type = TextureType::TextureCubeArray; break;

    default:
      tex.type = TextureType::Unknown;
      RDCERR("Unexpected texture enum %s", ToStr(target).c_str());
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

    default: tex.dimension = 2; RDCERR("Unexpected texture enum %s", ToStr(target).c_str());
  }

  tex.creationFlags = res.creationFlags;

  // surely this will be the same for each level... right? that would be insane if it wasn't
  GLint fmt = 0;
  drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0,
                                      eGL_TEXTURE_INTERNAL_FORMAT, &fmt);

  tex.format = MakeResourceFormat(target, (GLenum)fmt);

  if(tex.format.compType == CompType::Depth)
    tex.creationFlags |= TextureCategory::DepthTarget;

  if(target == eGL_TEXTURE_BUFFER)
  {
    tex.dimension = 1;
    tex.height = tex.depth = 1;
    tex.cubemap = false;
    tex.mips = 1;
    tex.arraysize = 1;
    tex.creationFlags = TextureCategory::ShaderRead;
    tex.msQual = 0;
    tex.msSamp = 1;
    tex.byteSize = 0;

    if(HasExt[ARB_texture_buffer_range])
    {
      drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0,
                                          eGL_TEXTURE_BUFFER_SIZE, (GLint *)&tex.byteSize);
      tex.width = uint32_t(tex.byteSize / RDCMAX(1, tex.format.compByteWidth * tex.format.compCount));
    }

    m_CachedTextures[id] = tex;
    return;
  }

  if(res.view)
  {
    tex.mips = Log2Floor(res.mipsValid + 1);
  }
  else
  {
    tex.mips = GetNumMips(target, res.resource.name, tex.width, tex.height, tex.depth);
  }

  GLint compressed = 0;
  drv.glGetTextureLevelParameterivEXT(res.resource.name, levelQueryType, 0, eGL_TEXTURE_COMPRESSED,
                                      &compressed);
  tex.byteSize = 0;
  for(uint32_t a = 0; a < tex.arraysize; a++)
  {
    for(uint32_t m = 0; m < tex.mips; m++)
    {
      if(fmt == eGL_NONE)
      {
      }
      else if(compressed)
      {
        tex.byteSize += (uint64_t)GetCompressedByteSize(
            RDCMAX(1U, tex.width >> m), RDCMAX(1U, tex.height >> m), 1, (GLenum)fmt);
      }
      else if(tex.format.Special())
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
  BufferDescription ret = {};

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &res = m_pDriver->m_Buffers[id];

  if(res.resource.Namespace == eResUnknown)
  {
    RDCERR("Details for invalid buffer id %llu requested", id);
    RDCEraseEl(ret);
    return ret;
  }

  WrappedOpenGL &drv = *m_pDriver;

  ret.resourceId = m_pDriver->GetResourceManager()->GetOriginalID(id);

  GLint prevBind = 0;
  if(res.curType != eGL_NONE)
  {
    drv.glGetIntegerv(BufferBinding(res.curType), &prevBind);

    drv.glBindBuffer(res.curType, res.resource.name);
  }

  ret.creationFlags = res.creationFlags;

  GLint size = 0;
  // if the type is NONE it's probably a DSA created buffer
  if(res.curType == eGL_NONE)
  {
    drv.glGetNamedBufferParameterivEXT(res.resource.name, eGL_BUFFER_SIZE, &size);
  }
  else
  {
    drv.glGetBufferParameteriv(res.curType, eGL_BUFFER_SIZE, &size);
  }

  ret.length = size;

  if(res.size == 0)
  {
    RDCWARN("BufferData::size didn't get filled out, setting at last minute");
    res.size = ret.length;
  }

  if(res.curType != eGL_NONE)
    drv.glBindBuffer(res.curType, prevBind);

  return ret;
}

std::vector<DebugMessage> GLReplay::GetDebugMessages()
{
  return m_pDriver->GetDebugMessages();
}

rdcarray<ShaderEntryPoint> GLReplay::GetShaderEntryPoints(ResourceId shader)
{
  if(m_pDriver->m_Shaders.find(shader) == m_pDriver->m_Shaders.end())
    return {};

  WrappedOpenGL::ShaderData &shaderDetails = m_pDriver->m_Shaders[shader];

  if(shaderDetails.reflection.resourceId == ResourceId())
  {
    RDCERR("Can't get shader details without successful reflect");
    return {};
  }

  return {{shaderDetails.reflection.entryPoint, shaderDetails.reflection.stage}};
}

ShaderReflection *GLReplay::GetShader(ResourceId shader, ShaderEntryPoint entry)
{
  auto &shaderDetails = m_pDriver->m_Shaders[shader];

  if(shaderDetails.reflection.resourceId == ResourceId())
  {
    RDCERR("Can't get shader details without successful reflect");
    return NULL;
  }

  return &shaderDetails.reflection;
}

std::vector<std::string> GLReplay::GetDisassemblyTargets()
{
  std::vector<std::string> ret;

  // default is always first
  ret.insert(ret.begin(), SPIRVDisassemblyTarget);

  return ret;
}

std::string GLReplay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                        const std::string &target)
{
  auto &shaderDetails =
      m_pDriver->m_Shaders[m_pDriver->GetResourceManager()->GetLiveID(refl->resourceId)];

  if(shaderDetails.sources.empty() && shaderDetails.spirvWords.empty())
    return "; Invalid Shader Specified";

  if(target == SPIRVDisassemblyTarget || target.empty())
  {
    std::string &disasm = shaderDetails.disassembly;

    if(disasm.empty())
      disasm = shaderDetails.spirv.Disassemble(refl->entryPoint.c_str());

    return disasm;
  }

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
}

void GLReplay::SavePipelineState(uint32_t eventId)
{
  GLPipe::State &pipe = m_CurPipelineState;
  WrappedOpenGL &drv = *m_pDriver;
  GLResourceManager *rm = m_pDriver->GetResourceManager();

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLRenderState rs;
  rs.FetchState(&drv);

  // Index buffer

  ContextPair &ctx = drv.GetCtx();

  GLuint vao = 0;
  drv.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&vao);
  pipe.vertexInput.vertexArrayObject = rm->GetOriginalID(rm->GetID(VertexArrayRes(ctx, vao)));

  GLuint ibuffer = 0;
  drv.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
  pipe.vertexInput.indexBuffer = rm->GetOriginalID(rm->GetID(BufferRes(ctx, ibuffer)));

  pipe.vertexInput.primitiveRestart = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart];
  pipe.vertexInput.restartIndex = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex]
                                      ? ~0U
                                      : rs.PrimitiveRestartIndex;

  // Vertex buffers and attributes
  GLint numVBufferBindings = 16;
  drv.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numVBufferBindings);

  GLint numVAttribBindings = 16;
  drv.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numVAttribBindings);

  pipe.vertexInput.vertexBuffers.resize(numVBufferBindings);
  pipe.vertexInput.attributes.resize(numVAttribBindings);

  for(GLuint i = 0; i < (GLuint)numVBufferBindings; i++)
  {
    GLuint buffer = GetBoundVertexBuffer(i);

    pipe.vertexInput.vertexBuffers[i].resourceId =
        rm->GetOriginalID(rm->GetID(BufferRes(ctx, buffer)));

    drv.glGetIntegeri_v(eGL_VERTEX_BINDING_STRIDE, i,
                        (GLint *)&pipe.vertexInput.vertexBuffers[i].byteStride);
    drv.glGetIntegeri_v(eGL_VERTEX_BINDING_OFFSET, i,
                        (GLint *)&pipe.vertexInput.vertexBuffers[i].byteOffset);
    drv.glGetIntegeri_v(eGL_VERTEX_BINDING_DIVISOR, i,
                        (GLint *)&pipe.vertexInput.vertexBuffers[i].instanceDivisor);
  }

  for(GLuint i = 0; i < (GLuint)numVAttribBindings; i++)
  {
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_ENABLED,
                            (GLint *)&pipe.vertexInput.attributes[i].enabled);
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_BINDING,
                            (GLint *)&pipe.vertexInput.attributes[i].vertexBufferSlot);
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_RELATIVE_OFFSET,
                            (GLint *)&pipe.vertexInput.attributes[i].byteOffset);

    GLenum type = eGL_FLOAT;
    GLint normalized = 0;

    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint *)&type);
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);

    GLint integer = 0;
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);

    RDCEraseEl(pipe.vertexInput.attributes[i].genericValue);
    drv.glGetVertexAttribfv(i, eGL_CURRENT_VERTEX_ATTRIB,
                            pipe.vertexInput.attributes[i].genericValue.floatValue);

    ResourceFormat fmt;

    fmt.type = ResourceFormatType::Regular;
    fmt.compCount = 4;
    GLint compCount;
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&compCount);

    fmt.compCount = (uint8_t)compCount;

    bool intComponent = !normalized || integer;

    switch(type)
    {
      default:
      case eGL_BYTE:
        fmt.compByteWidth = 1;
        fmt.compType = intComponent ? CompType::SInt : CompType::SNorm;
        break;
      case eGL_UNSIGNED_BYTE:
        fmt.compByteWidth = 1;
        fmt.compType = intComponent ? CompType::UInt : CompType::UNorm;
        break;
      case eGL_SHORT:
        fmt.compByteWidth = 2;
        fmt.compType = intComponent ? CompType::SInt : CompType::SNorm;
        break;
      case eGL_UNSIGNED_SHORT:
        fmt.compByteWidth = 2;
        fmt.compType = intComponent ? CompType::UInt : CompType::UNorm;
        break;
      case eGL_INT:
        fmt.compByteWidth = 4;
        fmt.compType = intComponent ? CompType::SInt : CompType::SNorm;
        break;
      case eGL_UNSIGNED_INT:
        fmt.compByteWidth = 4;
        fmt.compType = intComponent ? CompType::UInt : CompType::UNorm;
        break;
      case eGL_FLOAT:
        fmt.compByteWidth = 4;
        fmt.compType = CompType::Float;
        break;
      case eGL_DOUBLE:
        fmt.compByteWidth = 8;
        fmt.compType = CompType::Double;
        break;
      case eGL_HALF_FLOAT:
        fmt.compByteWidth = 2;
        fmt.compType = CompType::Float;
        break;
      case eGL_INT_2_10_10_10_REV:
        fmt.type = ResourceFormatType::R10G10B10A2;
        fmt.compCount = 4;
        fmt.compType = CompType::UInt;
        break;
      case eGL_UNSIGNED_INT_2_10_10_10_REV:
        fmt.type = ResourceFormatType::R10G10B10A2;
        fmt.compCount = 4;
        fmt.compType = CompType::SInt;
        break;
      case eGL_UNSIGNED_INT_10F_11F_11F_REV:
        fmt.type = ResourceFormatType::R11G11B10;
        fmt.compCount = 3;
        fmt.compType = CompType::Float;
        break;
    }

    if(compCount == eGL_BGRA)
    {
      fmt.compByteWidth = 1;
      fmt.compCount = 4;
      fmt.SetBGRAOrder(true);
      fmt.compType = CompType::UNorm;

      if(type == eGL_UNSIGNED_INT_2_10_10_10_REV || type == eGL_INT_2_10_10_10_REV)
      {
        fmt.type = ResourceFormatType::R10G10B10A2;
        fmt.compType = type == eGL_UNSIGNED_INT_2_10_10_10_REV ? CompType::UInt : CompType::SInt;
      }
      else if(type != eGL_UNSIGNED_BYTE)
      {
        // haven't checked the other cases work properly
        RDCERR("Unexpected BGRA type");
      }
    }

    pipe.vertexInput.attributes[i].format = fmt;
  }

  pipe.vertexInput.provokingVertexLast = (rs.ProvokingVertex != eGL_FIRST_VERTEX_CONVENTION);

  memcpy(pipe.vertexProcessing.defaultInnerLevel, rs.PatchParams.defaultInnerLevel,
         sizeof(rs.PatchParams.defaultInnerLevel));
  memcpy(pipe.vertexProcessing.defaultOuterLevel, rs.PatchParams.defaultOuterLevel,
         sizeof(rs.PatchParams.defaultOuterLevel));

  pipe.vertexProcessing.discard = rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard];
  pipe.vertexProcessing.clipOriginLowerLeft = (rs.ClipOrigin != eGL_UPPER_LEFT);
  pipe.vertexProcessing.clipNegativeOneToOne = (rs.ClipDepth != eGL_ZERO_TO_ONE);
  for(int i = 0; i < 8; i++)
    pipe.vertexProcessing.clipPlanes[i] = rs.Enabled[GLRenderState::eEnabled_ClipDistance0 + i];

  // Shader stages & Textures

  GLint numTexUnits = 8;
  drv.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);
  pipe.textures.resize(numTexUnits);
  pipe.samplers.resize(numTexUnits);

  GLenum activeTexture = eGL_TEXTURE0;
  drv.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&activeTexture);

  pipe.vertexShader.stage = ShaderStage::Vertex;
  pipe.tessControlShader.stage = ShaderStage::Tess_Control;
  pipe.tessEvalShader.stage = ShaderStage::Tess_Eval;
  pipe.geometryShader.stage = ShaderStage::Geometry;
  pipe.fragmentShader.stage = ShaderStage::Fragment;
  pipe.computeShader.stage = ShaderStage::Compute;

  GLuint curProg = 0;
  drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

  GLPipe::Shader *stages[6] = {
      &pipe.vertexShader,   &pipe.tessControlShader, &pipe.tessEvalShader,
      &pipe.geometryShader, &pipe.fragmentShader,    &pipe.computeShader,
  };
  ShaderReflection *refls[6] = {NULL};
  ShaderBindpointMapping *mappings[6] = {NULL};
  bool spirv[6] = {false};

  for(int i = 0; i < 6; i++)
  {
    stages[i]->programResourceId = stages[i]->shaderResourceId = ResourceId();
    stages[i]->reflection = NULL;
    stages[i]->bindpointMapping = ShaderBindpointMapping();
  }

  if(curProg == 0)
  {
    drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curProg);

    if(curProg == 0)
    {
      for(GLint unit = 0; unit < numTexUnits; unit++)
      {
        RDCEraseEl(pipe.textures[unit]);
        RDCEraseEl(pipe.samplers[unit]);
      }
    }
    else
    {
      ResourceId id = rm->GetID(ProgramPipeRes(ctx, curProg));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      pipe.pipelineResourceId = rm->GetOriginalID(id);

      for(size_t i = 0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          curProg = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;

          auto &shaderDetails = m_pDriver->m_Shaders[pipeDetails.stageShaders[i]];

          if(shaderDetails.reflection.resourceId == ResourceId())
            stages[i]->reflection = refls[i] = NULL;
          else
            stages[i]->reflection = refls[i] = &shaderDetails.reflection;

          if(!shaderDetails.spirvWords.empty())
          {
            stages[i]->bindpointMapping = shaderDetails.mapping;
            spirv[i] = true;

            EvaluateSPIRVBindpointMapping(curProg, (int)i, refls[i], stages[i]->bindpointMapping);
          }
          else
          {
            GetBindpointMapping(curProg, (int)i, refls[i], stages[i]->bindpointMapping);
          }

          mappings[i] = &stages[i]->bindpointMapping;

          stages[i]->programResourceId = rm->GetOriginalID(pipeDetails.stagePrograms[i]);
          stages[i]->shaderResourceId = rm->GetOriginalID(pipeDetails.stageShaders[i]);
        }
        else
        {
          stages[i]->programResourceId = stages[i]->shaderResourceId = ResourceId();
        }
      }
    }
  }
  else
  {
    ResourceId id = rm->GetID(ProgramRes(ctx, curProg));
    auto &progDetails = m_pDriver->m_Programs[id];

    pipe.pipelineResourceId = ResourceId();

    for(size_t i = 0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        auto &shaderDetails = m_pDriver->m_Shaders[progDetails.stageShaders[i]];

        if(shaderDetails.reflection.resourceId == ResourceId())
          stages[i]->reflection = refls[i] = NULL;
        else
          stages[i]->reflection = refls[i] = &shaderDetails.reflection;

        if(!shaderDetails.spirvWords.empty())
        {
          stages[i]->bindpointMapping = shaderDetails.mapping;
          spirv[i] = true;

          EvaluateSPIRVBindpointMapping(curProg, (int)i, refls[i], stages[i]->bindpointMapping);
        }
        else
        {
          GetBindpointMapping(curProg, (int)i, refls[i], stages[i]->bindpointMapping);
        }

        mappings[i] = &stages[i]->bindpointMapping;

        stages[i]->programResourceId = rm->GetOriginalID(id);
        stages[i]->shaderResourceId = rm->GetOriginalID(progDetails.stageShaders[i]);
      }
      else
      {
        stages[i]->programResourceId = stages[i]->shaderResourceId = ResourceId();
      }
    }
  }

  // !!!NOTE!!! This function will MODIFY the refls[] binding arrays.
  // See inside this function for what it does and why.
  for(size_t i = 0; i < ARRAY_COUNT(refls); i++)
  {
    // don't resort if it's SPIR-V
    if(spirv[i])
      continue;

    ResortBindings(refls[i], mappings[i]);
  }

  RDCEraseEl(pipe.transformFeedback);

  if(HasExt[ARB_transform_feedback2])
  {
    GLuint feedback = 0;
    drv.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&feedback);

    if(feedback != 0)
      pipe.transformFeedback.feedbackResourceId =
          rm->GetOriginalID(rm->GetID(FeedbackRes(ctx, feedback)));
    else
      pipe.transformFeedback.feedbackResourceId = ResourceId();

    GLint maxCount = 0;
    drv.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(int i = 0; i < (int)ARRAY_COUNT(pipe.transformFeedback.bufferResourceId) && i < maxCount; i++)
    {
      GLuint buffer = 0;
      drv.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);
      pipe.transformFeedback.bufferResourceId[i] =
          rm->GetOriginalID(rm->GetID(BufferRes(ctx, buffer)));
      drv.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_START, i,
                            (GLint64 *)&pipe.transformFeedback.byteOffset[i]);
      drv.glGetInteger64i_v(eGL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i,
                            (GLint64 *)&pipe.transformFeedback.byteSize[i]);
    }

    GLint p = 0;
    drv.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BUFFER_PAUSED, &p);
    pipe.transformFeedback.paused = (p != 0);

    drv.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE, &p);
    pipe.transformFeedback.active = (p != 0) || m_pDriver->m_WasActiveFeedback;
  }

  for(int i = 0; i < 6; i++)
  {
    size_t num = RDCMIN(128, rs.Subroutines[i].numSubroutines);
    if(num == 0)
    {
      RDCEraseEl(stages[i]->subroutines);
    }
    else
    {
      stages[i]->subroutines.resize(num);
      memcpy(stages[i]->subroutines.data(), rs.Subroutines[i].Values, num * sizeof(uint32_t));
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
    TextureType resType = TextureType::Unknown;

    bool shadow = false;

    for(size_t s = 0; s < ARRAY_COUNT(refls); s++)
    {
      if(refls[s] == NULL)
        continue;

      for(const ShaderResource &res : refls[s]->readOnlyResources)
      {
        // bindPoint is the uniform value for this sampler
        if(mappings[s]->readOnlyResources[res.bindPoint].bind == unit)
        {
          GLenum t = eGL_NONE;

          if(strstr(res.variableType.descriptor.name.c_str(), "Shadow"))
            shadow = true;

          switch(res.resType)
          {
            case TextureType::Unknown: target = eGL_NONE; break;
            case TextureType::Buffer: target = eGL_TEXTURE_BUFFER; break;
            case TextureType::Texture1D: target = eGL_TEXTURE_1D; break;
            case TextureType::Texture1DArray: target = eGL_TEXTURE_1D_ARRAY; break;
            case TextureType::Texture2D: target = eGL_TEXTURE_2D; break;
            case TextureType::TextureRect: target = eGL_TEXTURE_RECTANGLE; break;
            case TextureType::Texture2DArray: target = eGL_TEXTURE_2D_ARRAY; break;
            case TextureType::Texture2DMS: target = eGL_TEXTURE_2D_MULTISAMPLE; break;
            case TextureType::Texture2DMSArray: target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
            case TextureType::Texture3D: target = eGL_TEXTURE_3D; break;
            case TextureType::TextureCube: target = eGL_TEXTURE_CUBE_MAP; break;
            case TextureType::TextureCubeArray: target = eGL_TEXTURE_CUBE_MAP_ARRAY; break;
            case TextureType::Count: RDCERR("Invalid shader resource type"); break;
          }

          if(target != eGL_NONE)
            t = TextureBinding(target);

          resType = res.resType;

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
                    ToStr(binding).c_str(), ToStr(t).c_str());
          }
        }
      }
    }

    if(binding != eGL_NONE)
    {
      drv.glActiveTexture(GLenum(eGL_TEXTURE0 + unit));

      GLuint tex = 0;

      if(binding == eGL_TEXTURE_CUBE_MAP_ARRAY && !HasExt[ARB_texture_cube_map_array])
        tex = 0;
      else
        drv.glGetIntegerv(binding, (GLint *)&tex);

      if(tex == 0)
      {
        pipe.textures[unit].resourceId = ResourceId();
        pipe.textures[unit].firstMip = 0;
        pipe.textures[unit].numMips = 1;
        pipe.textures[unit].type = TextureType::Unknown;
        pipe.textures[unit].depthReadChannel = -1;
        pipe.textures[unit].swizzle[0] = TextureSwizzle::Red;
        pipe.textures[unit].swizzle[1] = TextureSwizzle::Green;
        pipe.textures[unit].swizzle[2] = TextureSwizzle::Blue;
        pipe.textures[unit].swizzle[3] = TextureSwizzle::Alpha;

        RDCEraseEl(pipe.samplers[unit].borderColor);
        pipe.samplers[unit].addressS = AddressMode::Wrap;
        pipe.samplers[unit].addressT = AddressMode::Wrap;
        pipe.samplers[unit].addressR = AddressMode::Wrap;
        pipe.samplers[unit].compareFunction = CompareFunction::AlwaysTrue;
        pipe.samplers[unit].filter = TextureFilter();
        pipe.samplers[unit].seamlessCubeMap = false;
        pipe.samplers[unit].maxAnisotropy = 0.0f;
        pipe.samplers[unit].maxLOD = 0.0f;
        pipe.samplers[unit].minLOD = 0.0f;
        pipe.samplers[unit].mipLODBias = 0.0f;
      }
      else
      {
        GLint firstMip = 0, numMips = 1;

        if(target != eGL_TEXTURE_BUFFER)
        {
          drv.glGetTexParameteriv(target, eGL_TEXTURE_BASE_LEVEL, &firstMip);
          drv.glGetTexParameteriv(target, eGL_TEXTURE_MAX_LEVEL, &numMips);

          numMips = numMips - firstMip + 1;
        }

        pipe.textures[unit].resourceId = rm->GetOriginalID(rm->GetID(TextureRes(ctx, tex)));
        pipe.textures[unit].firstMip = (uint32_t)firstMip;
        pipe.textures[unit].numMips = (uint32_t)numMips;
        pipe.textures[unit].type = resType;

        pipe.textures[unit].depthReadChannel = -1;

        GLenum levelQueryType =
            target == eGL_TEXTURE_CUBE_MAP ? eGL_TEXTURE_CUBE_MAP_POSITIVE_X : target;
        GLenum fmt = eGL_NONE;
        drv.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
        if(IsDepthStencilFormat(fmt))
        {
          GLint depthMode = eGL_DEPTH_COMPONENT;

          if(HasExt[ARB_stencil_texturing])
            drv.glGetTexParameteriv(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &depthMode);

          if(depthMode == eGL_DEPTH_COMPONENT)
            pipe.textures[unit].depthReadChannel = 0;
          else if(depthMode == eGL_STENCIL_INDEX)
            pipe.textures[unit].depthReadChannel = 1;
        }

        GLint swizzles[4] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};
        if(target != eGL_TEXTURE_BUFFER &&
           (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
          GetTextureSwizzle(tex, target, (GLenum *)swizzles);

        for(int i = 0; i < 4; i++)
        {
          switch(swizzles[i])
          {
            default:
            case GL_ZERO: pipe.textures[unit].swizzle[i] = TextureSwizzle::Zero; break;
            case GL_ONE: pipe.textures[unit].swizzle[i] = TextureSwizzle::One; break;
            case eGL_RED: pipe.textures[unit].swizzle[i] = TextureSwizzle::Red; break;
            case eGL_GREEN: pipe.textures[unit].swizzle[i] = TextureSwizzle::Green; break;
            case eGL_BLUE: pipe.textures[unit].swizzle[i] = TextureSwizzle::Blue; break;
            case eGL_ALPHA: pipe.textures[unit].swizzle[i] = TextureSwizzle::Alpha; break;
          }
        }

        GLuint samp = 0;
        if(HasExt[ARB_sampler_objects])
          drv.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&samp);

        pipe.samplers[unit].resourceId = rm->GetOriginalID(rm->GetID(SamplerRes(ctx, samp)));

        if(target != eGL_TEXTURE_BUFFER && target != eGL_TEXTURE_2D_MULTISAMPLE &&
           target != eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        {
          if(samp != 0)
            drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_BORDER_COLOR,
                                        &pipe.samplers[unit].borderColor[0]);
          else
            drv.glGetTexParameterfv(target, eGL_TEXTURE_BORDER_COLOR,
                                    &pipe.samplers[unit].borderColor[0]);

          GLint v;
          v = 0;
          if(samp != 0)
            drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_S, &v);
          else
            drv.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_S, &v);
          pipe.samplers[unit].addressS = MakeAddressMode((GLenum)v);

          v = 0;
          if(samp != 0)
            drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_T, &v);
          else
            drv.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_T, &v);
          pipe.samplers[unit].addressT = MakeAddressMode((GLenum)v);

          v = 0;
          if(samp != 0)
            drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_R, &v);
          else
            drv.glGetTexParameteriv(target, eGL_TEXTURE_WRAP_R, &v);
          pipe.samplers[unit].addressR = MakeAddressMode((GLenum)v);

          v = 0;
          if(HasExt[ARB_seamless_cubemap_per_texture])
          {
            if(samp != 0)
              drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_CUBE_MAP_SEAMLESS, &v);
            else
              drv.glGetTexParameteriv(target, eGL_TEXTURE_CUBE_MAP_SEAMLESS, &v);
          }
          pipe.samplers[unit].seamlessCubeMap =
              (v != 0 || rs.Enabled[GLRenderState::eEnabled_TexCubeSeamless]);

          v = 0;
          if(samp != 0)
            drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_COMPARE_FUNC, &v);
          else
            drv.glGetTexParameteriv(target, eGL_TEXTURE_COMPARE_FUNC, &v);
          pipe.samplers[unit].compareFunction = MakeCompareFunc((GLenum)v);

          GLint minf = 0;
          GLint magf = 0;
          if(samp != 0)
            drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_MIN_FILTER, &minf);
          else
            drv.glGetTexParameteriv(target, eGL_TEXTURE_MIN_FILTER, &minf);

          if(samp != 0)
            drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_MAG_FILTER, &magf);
          else
            drv.glGetTexParameteriv(target, eGL_TEXTURE_MAG_FILTER, &magf);

          if(HasExt[ARB_texture_filter_anisotropic])
          {
            if(samp != 0)
              drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_MAX_ANISOTROPY,
                                          &pipe.samplers[unit].maxAnisotropy);
            else
              drv.glGetTexParameterfv(target, eGL_TEXTURE_MAX_ANISOTROPY,
                                      &pipe.samplers[unit].maxAnisotropy);
          }
          else
          {
            pipe.samplers[unit].maxAnisotropy = 0.0f;
          }

          pipe.samplers[unit].filter =
              MakeFilter((GLenum)minf, (GLenum)magf, shadow, pipe.samplers[unit].maxAnisotropy);

          if(samp != 0)
            drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_MAX_LOD, &pipe.samplers[unit].maxLOD);
          else
            drv.glGetTexParameterfv(target, eGL_TEXTURE_MAX_LOD, &pipe.samplers[unit].maxLOD);

          if(samp != 0)
            drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_MIN_LOD, &pipe.samplers[unit].minLOD);
          else
            drv.glGetTexParameterfv(target, eGL_TEXTURE_MIN_LOD, &pipe.samplers[unit].minLOD);

          if(!IsGLES)
          {
            if(samp != 0)
              drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_LOD_BIAS,
                                          &pipe.samplers[unit].mipLODBias);
            else
              drv.glGetTexParameterfv(target, eGL_TEXTURE_LOD_BIAS, &pipe.samplers[unit].mipLODBias);
          }
          else
          {
            pipe.samplers[unit].mipLODBias = 0.0f;
          }
        }
        else
        {
          // texture buffers don't support sampling
          RDCEraseEl(pipe.samplers[unit].borderColor);
          pipe.samplers[unit].addressS = AddressMode::Wrap;
          pipe.samplers[unit].addressT = AddressMode::Wrap;
          pipe.samplers[unit].addressR = AddressMode::Wrap;
          pipe.samplers[unit].compareFunction = CompareFunction::AlwaysTrue;
          pipe.samplers[unit].filter = TextureFilter();
          pipe.samplers[unit].seamlessCubeMap = false;
          pipe.samplers[unit].maxAnisotropy = 0.0f;
          pipe.samplers[unit].maxLOD = 0.0f;
          pipe.samplers[unit].minLOD = 0.0f;
          pipe.samplers[unit].mipLODBias = 0.0f;
        }
      }
    }
    else
    {
      // what should we do in this case? there could be something bound just not used,
      // it'd be nice to return that
    }
  }

  drv.glActiveTexture(activeTexture);

  pipe.uniformBuffers.resize(ARRAY_COUNT(rs.UniformBinding));
  for(size_t b = 0; b < pipe.uniformBuffers.size(); b++)
  {
    if(rs.UniformBinding[b].res.name == 0)
    {
      pipe.uniformBuffers[b].resourceId = ResourceId();
      pipe.uniformBuffers[b].byteOffset = pipe.uniformBuffers[b].byteSize = 0;
    }
    else
    {
      pipe.uniformBuffers[b].resourceId = rm->GetOriginalID(rm->GetID(rs.UniformBinding[b].res));
      pipe.uniformBuffers[b].byteOffset = rs.UniformBinding[b].start;
      pipe.uniformBuffers[b].byteSize = rs.UniformBinding[b].size;
    }
  }

  pipe.atomicBuffers.resize(ARRAY_COUNT(rs.AtomicCounter));
  for(size_t b = 0; b < pipe.atomicBuffers.size(); b++)
  {
    if(rs.AtomicCounter[b].res.name == 0)
    {
      pipe.atomicBuffers[b].resourceId = ResourceId();
      pipe.atomicBuffers[b].byteOffset = pipe.atomicBuffers[b].byteSize = 0;
    }
    else
    {
      pipe.atomicBuffers[b].resourceId = rm->GetOriginalID(rm->GetID(rs.AtomicCounter[b].res));
      pipe.atomicBuffers[b].byteOffset = rs.AtomicCounter[b].start;
      pipe.atomicBuffers[b].byteSize = rs.AtomicCounter[b].size;
    }
  }

  pipe.shaderStorageBuffers.resize(ARRAY_COUNT(rs.ShaderStorage));
  for(size_t b = 0; b < pipe.shaderStorageBuffers.size(); b++)
  {
    if(rs.ShaderStorage[b].res.name == 0)
    {
      pipe.shaderStorageBuffers[b].resourceId = ResourceId();
      pipe.shaderStorageBuffers[b].byteOffset = pipe.shaderStorageBuffers[b].byteSize = 0;
    }
    else
    {
      pipe.shaderStorageBuffers[b].resourceId = rm->GetOriginalID(rm->GetID(rs.ShaderStorage[b].res));
      pipe.shaderStorageBuffers[b].byteOffset = rs.ShaderStorage[b].start;
      pipe.shaderStorageBuffers[b].byteSize = rs.ShaderStorage[b].size;
    }
  }

  pipe.images.resize(ARRAY_COUNT(rs.Images));
  for(size_t i = 0; i < pipe.images.size(); i++)
  {
    if(rs.Images[i].res.name == 0)
    {
      RDCEraseEl(pipe.images[i]);
    }
    else
    {
      ResourceId id = rm->GetID(rs.Images[i].res);
      pipe.images[i].resourceId = rm->GetOriginalID(id);
      pipe.images[i].mipLevel = rs.Images[i].level;
      pipe.images[i].layered = rs.Images[i].layered;
      pipe.images[i].slice = rs.Images[i].layer;
      if(rs.Images[i].access == eGL_READ_ONLY)
      {
        pipe.images[i].readAllowed = true;
        pipe.images[i].writeAllowed = false;
      }
      else if(rs.Images[i].access == eGL_WRITE_ONLY)
      {
        pipe.images[i].readAllowed = false;
        pipe.images[i].writeAllowed = true;
      }
      else
      {
        pipe.images[i].readAllowed = true;
        pipe.images[i].writeAllowed = true;
      }
      pipe.images[i].imageFormat = MakeResourceFormat(eGL_TEXTURE_2D, rs.Images[i].format);

      CacheTexture(id);

      pipe.images[i].type = m_CachedTextures[id].type;
    }
  }

  // Vertex post processing and rasterization

  RDCCOMPILE_ASSERT(ARRAY_COUNT(rs.Viewports) == ARRAY_COUNT(rs.DepthRanges),
                    "GL Viewport count does not match depth ranges count");
  pipe.rasterizer.viewports.resize(ARRAY_COUNT(rs.Viewports));
  for(size_t v = 0; v < pipe.rasterizer.viewports.size(); ++v)
  {
    pipe.rasterizer.viewports[v].x = rs.Viewports[v].x;
    pipe.rasterizer.viewports[v].y = rs.Viewports[v].y;
    pipe.rasterizer.viewports[v].width = rs.Viewports[v].width;
    pipe.rasterizer.viewports[v].height = rs.Viewports[v].height;
    pipe.rasterizer.viewports[v].minDepth = (float)rs.DepthRanges[v].nearZ;
    pipe.rasterizer.viewports[v].maxDepth = (float)rs.DepthRanges[v].farZ;
  }

  pipe.rasterizer.scissors.resize(ARRAY_COUNT(rs.Scissors));
  for(size_t s = 0; s < pipe.rasterizer.scissors.size(); ++s)
  {
    pipe.rasterizer.scissors[s].x = rs.Scissors[s].x;
    pipe.rasterizer.scissors[s].y = rs.Scissors[s].y;
    pipe.rasterizer.scissors[s].width = rs.Scissors[s].width;
    pipe.rasterizer.scissors[s].height = rs.Scissors[s].height;
    pipe.rasterizer.scissors[s].enabled = rs.Scissors[s].enabled;
  }

  int polygonOffsetEnableEnum;
  switch(rs.PolygonMode)
  {
    default:
      RDCWARN("Unexpected value for POLYGON_MODE %x", rs.PolygonMode);
    // fall through
    case eGL_FILL:
      pipe.rasterizer.state.fillMode = FillMode::Solid;
      polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetFill;
      break;
    case eGL_LINE:
      pipe.rasterizer.state.fillMode = FillMode::Wireframe;
      polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetLine;
      break;
    case eGL_POINT:
      pipe.rasterizer.state.fillMode = FillMode::Point;
      polygonOffsetEnableEnum = GLRenderState::eEnabled_PolyOffsetPoint;
      break;
  }
  if(rs.Enabled[polygonOffsetEnableEnum])
  {
    pipe.rasterizer.state.depthBias = rs.PolygonOffset[1];
    pipe.rasterizer.state.slopeScaledDepthBias = rs.PolygonOffset[0];
    pipe.rasterizer.state.offsetClamp = rs.PolygonOffset[2];
  }
  else
  {
    pipe.rasterizer.state.depthBias = 0.0f;
    pipe.rasterizer.state.slopeScaledDepthBias = 0.0f;
    pipe.rasterizer.state.offsetClamp = 0.0f;
  }

  if(rs.Enabled[GLRenderState::eEnabled_CullFace])
  {
    switch(rs.CullFace)
    {
      default:
        RDCWARN("Unexpected value for CULL_FACE %x", rs.CullFace);
      // fall through
      case eGL_BACK: pipe.rasterizer.state.cullMode = CullMode::Back; break;
      case eGL_FRONT: pipe.rasterizer.state.cullMode = CullMode::Front; break;
      case eGL_FRONT_AND_BACK: pipe.rasterizer.state.cullMode = CullMode::FrontAndBack; break;
    }
  }
  else
  {
    pipe.rasterizer.state.cullMode = CullMode::NoCull;
  }

  RDCASSERT(rs.FrontFace == eGL_CCW || rs.FrontFace == eGL_CW);
  pipe.rasterizer.state.frontCCW = rs.FrontFace == eGL_CCW;
  pipe.rasterizer.state.depthClamp = rs.Enabled[GLRenderState::eEnabled_DepthClamp];

  pipe.rasterizer.state.multisampleEnable = rs.Enabled[GLRenderState::eEnabled_Multisample];
  pipe.rasterizer.state.sampleShading = rs.Enabled[GLRenderState::eEnabled_SampleShading];
  pipe.rasterizer.state.sampleMask = rs.Enabled[GLRenderState::eEnabled_SampleMask];
  pipe.rasterizer.state.sampleMaskValue =
      rs.SampleMask[0];    // assume number of samples is less than 32
  pipe.rasterizer.state.sampleCoverage = rs.Enabled[GLRenderState::eEnabled_SampleCoverage];
  pipe.rasterizer.state.sampleCoverageInvert = rs.SampleCoverageInvert;
  pipe.rasterizer.state.sampleCoverageValue = rs.SampleCoverage;
  pipe.rasterizer.state.alphaToCoverage = rs.Enabled[GLRenderState::eEnabled_SampleAlphaToCoverage];
  pipe.rasterizer.state.alphaToOne = rs.Enabled[GLRenderState::eEnabled_SampleAlphaToOne];
  pipe.rasterizer.state.minSampleShadingRate = rs.MinSampleShading;

  pipe.rasterizer.state.programmablePointSize = rs.Enabled[rs.eEnabled_ProgramPointSize];
  pipe.rasterizer.state.pointSize = rs.PointSize;
  pipe.rasterizer.state.lineWidth = rs.LineWidth;
  pipe.rasterizer.state.pointFadeThreshold = rs.PointFadeThresholdSize;
  pipe.rasterizer.state.pointOriginUpperLeft = (rs.PointSpriteOrigin != eGL_LOWER_LEFT);

  // depth and stencil states

  pipe.depthState.depthEnable = rs.Enabled[GLRenderState::eEnabled_DepthTest];
  pipe.depthState.depthWrites = rs.DepthWriteMask != 0;
  pipe.depthState.depthFunction = MakeCompareFunc(rs.DepthFunc);

  pipe.depthState.depthBounds = rs.Enabled[GLRenderState::eEnabled_DepthBoundsEXT];
  pipe.depthState.nearBound = rs.DepthBounds.nearZ;
  pipe.depthState.farBound = rs.DepthBounds.farZ;

  pipe.stencilState.stencilEnable = rs.Enabled[GLRenderState::eEnabled_StencilTest];
  pipe.stencilState.frontFace.compareMask = rs.StencilFront.valuemask;
  pipe.stencilState.frontFace.writeMask = rs.StencilFront.writemask;
  pipe.stencilState.frontFace.reference = uint8_t(rs.StencilFront.ref & 0xff);
  pipe.stencilState.frontFace.function = MakeCompareFunc(rs.StencilFront.func);
  pipe.stencilState.frontFace.passOperation = MakeStencilOp(rs.StencilFront.pass);
  pipe.stencilState.frontFace.failOperation = MakeStencilOp(rs.StencilFront.stencilFail);
  pipe.stencilState.frontFace.depthFailOperation = MakeStencilOp(rs.StencilFront.depthFail);
  pipe.stencilState.backFace.compareMask = rs.StencilBack.valuemask;
  pipe.stencilState.backFace.writeMask = rs.StencilBack.writemask;
  pipe.stencilState.backFace.reference = uint8_t(rs.StencilBack.ref & 0xff);
  pipe.stencilState.backFace.function = MakeCompareFunc(rs.StencilBack.func);
  pipe.stencilState.backFace.passOperation = MakeStencilOp(rs.StencilBack.pass);
  pipe.stencilState.backFace.failOperation = MakeStencilOp(rs.StencilBack.stencilFail);
  pipe.stencilState.backFace.depthFailOperation = MakeStencilOp(rs.StencilBack.depthFail);

  // Frame buffer

  GLuint curDrawFBO = 0;
  drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
  GLuint curReadFBO = 0;
  drv.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

  GLint numCols = 8;
  drv.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);

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
      drv.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curCol[i]);
      drv.glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
      if(type == eGL_RENDERBUFFER)
        rbCol[i] = true;
    }

    drv.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              (GLint *)&curDepth);
    drv.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbDepth = true;
    drv.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              (GLint *)&curStencil);
    drv.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbStencil = true;

    pipe.framebuffer.drawFBO.resourceId =
        rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curDrawFBO)));
    pipe.framebuffer.drawFBO.colorAttachments.resize(numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      ResourceId id =
          rm->GetID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i]));

      pipe.framebuffer.drawFBO.colorAttachments[i].resourceId = rm->GetOriginalID(id);

      if(pipe.framebuffer.drawFBO.colorAttachments[i].resourceId != ResourceId() && !rbCol[i])
        GetFramebufferMipAndLayer(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                  (GLint *)&pipe.framebuffer.drawFBO.colorAttachments[i].mipLevel,
                                  (GLint *)&pipe.framebuffer.drawFBO.colorAttachments[i].slice);

      GLint swizzles[4] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};
      if(!rbCol[i] && id != ResourceId() &&
         (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
      {
        GLenum target = m_pDriver->m_Textures[id].curType;
        GetTextureSwizzle(curCol[i], target, (GLenum *)swizzles);
      }

      for(int s = 0; s < 4; s++)
      {
        switch(swizzles[s])
        {
          default:
          case GL_ZERO:
            pipe.framebuffer.drawFBO.colorAttachments[i].swizzle[s] = TextureSwizzle::Zero;
            break;
          case GL_ONE:
            pipe.framebuffer.drawFBO.colorAttachments[i].swizzle[s] = TextureSwizzle::One;
            break;
          case eGL_RED:
            pipe.framebuffer.drawFBO.colorAttachments[i].swizzle[s] = TextureSwizzle::Red;
            break;
          case eGL_GREEN:
            pipe.framebuffer.drawFBO.colorAttachments[i].swizzle[s] = TextureSwizzle::Green;
            break;
          case eGL_BLUE:
            pipe.framebuffer.drawFBO.colorAttachments[i].swizzle[s] = TextureSwizzle::Blue;
            break;
          case eGL_ALPHA:
            pipe.framebuffer.drawFBO.colorAttachments[i].swizzle[s] = TextureSwizzle::Alpha;
            break;
        }
      }
    }

    pipe.framebuffer.drawFBO.depthAttachment.resourceId = rm->GetOriginalID(
        rm->GetID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
    pipe.framebuffer.drawFBO.stencilAttachment.resourceId = rm->GetOriginalID(
        rm->GetID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

    if(pipe.framebuffer.drawFBO.depthAttachment.resourceId != ResourceId() && !rbDepth)
      GetFramebufferMipAndLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                (GLint *)&pipe.framebuffer.drawFBO.depthAttachment.mipLevel,
                                (GLint *)&pipe.framebuffer.drawFBO.depthAttachment.slice);

    if(pipe.framebuffer.drawFBO.stencilAttachment.resourceId != ResourceId() && !rbStencil)
      GetFramebufferMipAndLayer(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                (GLint *)&pipe.framebuffer.drawFBO.stencilAttachment.mipLevel,
                                (GLint *)&pipe.framebuffer.drawFBO.stencilAttachment.slice);

    pipe.framebuffer.drawFBO.drawBuffers.resize(numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      GLenum b = eGL_NONE;
      drv.glGetIntegerv(GLenum(eGL_DRAW_BUFFER0 + i), (GLint *)&b);
      if(b >= eGL_COLOR_ATTACHMENT0 && b <= GLenum(eGL_COLOR_ATTACHMENT0 + numCols))
        pipe.framebuffer.drawFBO.drawBuffers[i] = b - eGL_COLOR_ATTACHMENT0;
      else
        pipe.framebuffer.drawFBO.drawBuffers[i] = -1;
    }

    pipe.framebuffer.drawFBO.readBuffer = -1;
  }

  {
    GLenum type = eGL_TEXTURE;
    for(GLint i = 0; i < numCols; i++)
    {
      drv.glGetFramebufferAttachmentParameteriv(
          eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curCol[i]);
      drv.glGetFramebufferAttachmentParameteriv(
          eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
          eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
      if(type == eGL_RENDERBUFFER)
        rbCol[i] = true;
    }

    drv.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              (GLint *)&curDepth);
    drv.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbDepth = true;
    drv.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              (GLint *)&curStencil);
    drv.glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                              eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, (GLint *)&type);
    if(type == eGL_RENDERBUFFER)
      rbStencil = true;

    pipe.framebuffer.readFBO.resourceId =
        rm->GetOriginalID(rm->GetID(FramebufferRes(ctx, curReadFBO)));
    pipe.framebuffer.readFBO.colorAttachments.resize(numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      pipe.framebuffer.readFBO.colorAttachments[i].resourceId = rm->GetOriginalID(
          rm->GetID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i])));

      if(pipe.framebuffer.readFBO.colorAttachments[i].resourceId != ResourceId() && !rbCol[i])
        GetFramebufferMipAndLayer(eGL_READ_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                  (GLint *)&pipe.framebuffer.readFBO.colorAttachments[i].mipLevel,
                                  (GLint *)&pipe.framebuffer.readFBO.colorAttachments[i].slice);
    }

    pipe.framebuffer.readFBO.depthAttachment.resourceId = rm->GetOriginalID(
        rm->GetID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
    pipe.framebuffer.readFBO.stencilAttachment.resourceId = rm->GetOriginalID(
        rm->GetID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

    if(pipe.framebuffer.readFBO.depthAttachment.resourceId != ResourceId() && !rbDepth)
      GetFramebufferMipAndLayer(eGL_READ_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                (GLint *)&pipe.framebuffer.readFBO.depthAttachment.mipLevel,
                                (GLint *)&pipe.framebuffer.readFBO.depthAttachment.slice);

    if(pipe.framebuffer.readFBO.stencilAttachment.resourceId != ResourceId() && !rbStencil)
      GetFramebufferMipAndLayer(eGL_READ_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                (GLint *)&pipe.framebuffer.readFBO.stencilAttachment.mipLevel,
                                (GLint *)&pipe.framebuffer.readFBO.stencilAttachment.slice);

    pipe.framebuffer.readFBO.drawBuffers.resize(numCols);
    for(GLint i = 0; i < numCols; i++)
      pipe.framebuffer.readFBO.drawBuffers[i] = -1;

    GLenum b = eGL_NONE;
    drv.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&b);
    if(b >= eGL_COLOR_ATTACHMENT0 && b <= GLenum(eGL_COLOR_ATTACHMENT0 + numCols))
      pipe.framebuffer.drawFBO.readBuffer = b - eGL_COLOR_ATTACHMENT0;
    else
      pipe.framebuffer.drawFBO.readBuffer = -1;
  }

  memcpy(pipe.framebuffer.blendState.blendFactor, rs.BlendColor, sizeof(rs.BlendColor));

  pipe.framebuffer.framebufferSRGB = rs.Enabled[GLRenderState::eEnabled_FramebufferSRGB];
  pipe.framebuffer.dither = rs.Enabled[GLRenderState::eEnabled_Dither];

  RDCCOMPILE_ASSERT(ARRAY_COUNT(rs.Blends) == ARRAY_COUNT(rs.ColorMasks),
                    "Color masks and blends mismatched");
  pipe.framebuffer.blendState.blends.resize(ARRAY_COUNT(rs.Blends));
  for(size_t i = 0; i < ARRAY_COUNT(rs.Blends); i++)
  {
    pipe.framebuffer.blendState.blends[i].enabled = rs.Blends[i].Enabled;
    pipe.framebuffer.blendState.blends[i].logicOperation = LogicOperation::NoOp;

    if(rs.LogicOp != eGL_NONE && rs.LogicOp != eGL_COPY)
      pipe.framebuffer.blendState.blends[i].logicOperation = MakeLogicOp(rs.LogicOp);

    pipe.framebuffer.blendState.blends[i].logicOperationEnabled =
        rs.Enabled[GLRenderState::eEnabled_ColorLogicOp];

    pipe.framebuffer.blendState.blends[i].colorBlend.source =
        MakeBlendMultiplier(rs.Blends[i].SourceRGB);
    pipe.framebuffer.blendState.blends[i].colorBlend.destination =
        MakeBlendMultiplier(rs.Blends[i].DestinationRGB);
    pipe.framebuffer.blendState.blends[i].colorBlend.operation =
        MakeBlendOp(rs.Blends[i].EquationRGB);

    pipe.framebuffer.blendState.blends[i].alphaBlend.source =
        MakeBlendMultiplier(rs.Blends[i].SourceAlpha);
    pipe.framebuffer.blendState.blends[i].alphaBlend.destination =
        MakeBlendMultiplier(rs.Blends[i].DestinationAlpha);
    pipe.framebuffer.blendState.blends[i].alphaBlend.operation =
        MakeBlendOp(rs.Blends[i].EquationAlpha);

    pipe.framebuffer.blendState.blends[i].writeMask = 0;
    if(rs.ColorMasks[i].red)
      pipe.framebuffer.blendState.blends[i].writeMask |= 1;
    if(rs.ColorMasks[i].green)
      pipe.framebuffer.blendState.blends[i].writeMask |= 2;
    if(rs.ColorMasks[i].blue)
      pipe.framebuffer.blendState.blends[i].writeMask |= 4;
    if(rs.ColorMasks[i].alpha)
      pipe.framebuffer.blendState.blends[i].writeMask |= 8;
  }

  switch(rs.Hints.Derivatives)
  {
    default:
    case eGL_DONT_CARE: pipe.hints.derivatives = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.hints.derivatives = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.hints.derivatives = QualityHint::Fastest; break;
  }

  switch(rs.Hints.LineSmooth)
  {
    default:
    case eGL_DONT_CARE: pipe.hints.lineSmoothing = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.hints.lineSmoothing = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.hints.lineSmoothing = QualityHint::Fastest; break;
  }

  switch(rs.Hints.PolySmooth)
  {
    default:
    case eGL_DONT_CARE: pipe.hints.polySmoothing = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.hints.polySmoothing = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.hints.polySmoothing = QualityHint::Fastest; break;
  }

  switch(rs.Hints.TexCompression)
  {
    default:
    case eGL_DONT_CARE: pipe.hints.textureCompression = QualityHint::DontCare; break;
    case eGL_NICEST: pipe.hints.textureCompression = QualityHint::Nicest; break;
    case eGL_FASTEST: pipe.hints.textureCompression = QualityHint::Fastest; break;
  }

  pipe.hints.lineSmoothingEnabled = rs.Enabled[GLRenderState::eEnabled_LineSmooth];
  pipe.hints.polySmoothingEnabled = rs.Enabled[GLRenderState::eEnabled_PolySmooth];
}

void GLReplay::OpenGLFillCBufferVariables(GLuint prog, bool bufferBacked, std::string prefix,
                                          const rdcarray<ShaderConstant> &variables,
                                          rdcarray<ShaderVariable> &outvars,
                                          const bytebuf &bufferData)
{
  bytebuf uniformData;
  const bytebuf &data = bufferBacked ? bufferData : uniformData;

  if(!bufferBacked)
    uniformData.resize(128);

  for(int32_t i = 0; i < variables.count(); i++)
  {
    const ShaderVariableDescriptor &desc = variables[i].type.descriptor;

    // remove implicit '.' for recursing through "structs" if it's actually a multi-dimensional
    // array.
    if(!prefix.empty() && prefix.back() == '.' && variables[i].name[0] == '[')
      prefix.pop_back();

    ShaderVariable var;
    var.name = variables[i].name;
    var.rows = desc.rows;
    var.columns = desc.columns;
    var.type = desc.type;
    var.rowMajor = desc.rowMajorStorage;

    const uint32_t matStride = desc.matrixByteStride;

    if(!variables[i].type.members.empty())
    {
      if(desc.elements == 0)
      {
        OpenGLFillCBufferVariables(prog, bufferBacked, prefix + var.name.c_str() + ".",
                                   variables[i].type.members, var.members, data);
        var.isStruct = true;
      }
      else
      {
        var.members.resize(desc.elements);
        for(uint32_t a = 0; a < desc.elements; a++)
        {
          ShaderVariable &arrEl = var.members[a];
          arrEl.rows = var.rows;
          arrEl.columns = var.columns;
          arrEl.name = StringFormat::Fmt("%s[%u]", var.name.c_str(), a);
          arrEl.type = var.type;
          arrEl.isStruct = true;
          arrEl.rowMajor = var.rowMajor;

          OpenGLFillCBufferVariables(prog, bufferBacked, prefix + arrEl.name.c_str() + ".",
                                     variables[i].type.members, arrEl.members, data);
        }
        var.isStruct = false;
        var.rows = var.columns = 0;
      }
    }
    else
    {
      RDCEraseEl(var.value);

      // need to query offset and strides as there's no way to know what layout was used
      // (and if it's not an std layout it's implementation defined :( )
      std::string fullname = prefix + var.name.c_str();

      GLuint idx = GL.glGetProgramResourceIndex(prog, eGL_UNIFORM, fullname.c_str());

      if(idx == GL_INVALID_INDEX)
      {
        // this might not be an error, this might be the corresponding member in an array-of-structs
        // that doesn't exist because it's not in a UBO.
        // e.g.:
        // struct foo { float a; float b; }
        // uniform foo bar[2];
        //
        // If the program only references bar[0].a and bar[1].b then we'd reflect the full structure
        // but only bar[0].a and bar[1].b  would have indices - bar[0].b and bar[1].a would not.
        RDCWARN("Can't find program resource index for %s", fullname.c_str());

        if(bufferBacked)
          RDCERR("Uniform is buffer backed - index expected");

        // if this is an array, generate empty members
        if(desc.elements > 0)
        {
          std::vector<ShaderVariable> elems;
          for(uint32_t a = 0; a < desc.elements; a++)
          {
            ShaderVariable el = var;

            // if this is the last part of a multidimensional array, don't include the variable name
            if(var.name[0] != '[')
              el.name = StringFormat::Fmt("%s[%u]", var.name.c_str(), a);
            else
              el.name = StringFormat::Fmt("[%u]", a);

            el.isStruct = false;

            elems.push_back(el);
          }

          var.members = elems;
          var.isStruct = false;
          var.rows = var.columns = 0;
        }
      }
      else
      {
        GLenum props[] = {bufferBacked ? eGL_OFFSET : eGL_LOCATION};
        GLint values[] = {0};

        GL.glGetProgramResourceiv(prog, eGL_UNIFORM, idx, ARRAY_COUNT(props), props,
                                  ARRAY_COUNT(props), NULL, values);

        GLint location = values[0];
        GLint offset = values[0];

        if(!bufferBacked)
          offset = 0;

        if(desc.elements == 0)
        {
          if(!bufferBacked)
          {
            switch(var.type)
            {
              case VarType::Unknown:
              case VarType::SLong:
              case VarType::ULong:
              case VarType::SShort:
              case VarType::UShort:
              case VarType::SByte:
              case VarType::UByte:
              case VarType::Half:
                RDCERR("Unexpected base variable type %s, treating as float",
                       ToStr(var.type).c_str());
              // deliberate fall-through
              case VarType::Float:
                GL.glGetUniformfv(prog, location, (float *)uniformData.data());
                break;
              case VarType::SInt:
                GL.glGetUniformiv(prog, location, (int32_t *)uniformData.data());
                break;
              case VarType::UInt:
                GL.glGetUniformuiv(prog, location, (uint32_t *)uniformData.data());
                break;
              case VarType::Double:
                GL.glGetUniformdv(prog, location, (double *)uniformData.data());
                break;
            }
          }

          StandardFillCBufferVariable(offset, data, var, matStride);
        }
        else
        {
          std::vector<ShaderVariable> elems;
          for(uint32_t a = 0; a < desc.elements; a++)
          {
            ShaderVariable el = var;

            // if this is the last part of a multidimensional array, don't include the variable name
            if(var.name[0] != '[')
              el.name = StringFormat::Fmt("%s[%u]", var.name.c_str(), a);
            else
              el.name = StringFormat::Fmt("[%u]", a);

            if(!bufferBacked)
            {
              switch(var.type)
              {
                case VarType::Unknown:
                case VarType::SLong:
                case VarType::ULong:
                case VarType::SShort:
                case VarType::UShort:
                case VarType::SByte:
                case VarType::UByte:
                case VarType::Half:
                  RDCERR("Unexpected base variable type %s, treating as float",
                         ToStr(var.type).c_str());
                // deliberate fall-through
                case VarType::Float:
                  GL.glGetUniformfv(prog, location + a, (float *)uniformData.data());
                  break;
                case VarType::SInt:
                  GL.glGetUniformiv(prog, location + a, (int32_t *)uniformData.data());
                  break;
                case VarType::UInt:
                  GL.glGetUniformuiv(prog, location + a, (uint32_t *)uniformData.data());
                  break;
                case VarType::Double:
                  GL.glGetUniformdv(prog, location + a, (double *)uniformData.data());
                  break;
              }
            }

            StandardFillCBufferVariable(offset, data, el, matStride);

            if(bufferBacked)
              offset += desc.arrayByteStride;

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

void GLReplay::FillCBufferVariables(ResourceId shader, std::string entryPoint, uint32_t cbufSlot,
                                    rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &shaderDetails = m_pDriver->m_Shaders[shader];

  if((int32_t)cbufSlot >= shaderDetails.reflection.constantBlocks.count())
  {
    RDCERR("Requesting invalid constant block");
    return;
  }

  GLuint curProg = 0;
  drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

  if(curProg == 0)
  {
    drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curProg);

    if(curProg == 0)
    {
      RDCERR("No program or pipeline bound");
      return;
    }
    else
    {
      ResourceId id =
          m_pDriver->GetResourceManager()->GetID(ProgramPipeRes(m_pDriver->GetCtx(), curProg));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      size_t s = ShaderIdx(shaderDetails.type);

      curProg =
          m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[s]).name;
    }
  }

  const ConstantBlock &cblock = shaderDetails.reflection.constantBlocks[cbufSlot];

  if(shaderDetails.spirvWords.empty())
  {
    OpenGLFillCBufferVariables(curProg, cblock.bufferBacked ? true : false, "", cblock.variables,
                               outvars, data);
  }
  else
  {
    if(shaderDetails.mapping.constantBlocks[cbufSlot].bindset == SpecializationConstantBindSet)
    {
      std::vector<SpecConstant> specconsts;

      for(size_t i = 0; i < shaderDetails.specIDs.size(); i++)
      {
        SpecConstant spec;
        spec.specID = shaderDetails.specIDs[i];
        spec.data.resize(sizeof(shaderDetails.specValues[i]));
        memcpy(&spec.data[0], &shaderDetails.specValues[i], spec.data.size());
        specconsts.push_back(spec);
      }

      FillSpecConstantVariables(cblock.variables, outvars, specconsts);
    }
    else if(!cblock.bufferBacked)
    {
      OpenGLFillCBufferVariables(curProg, false, "", cblock.variables, outvars, data);
    }
    else
    {
      StandardFillCBufferVariables(cblock.variables, outvars, data);
    }
  }
}

void GLReplay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                              const GetTextureDataParams &params, bytebuf &data)
{
  WrappedOpenGL &drv = *m_pDriver;

  WrappedOpenGL::TextureData &texDetails = m_pDriver->m_Textures[tex];

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
    return;
  }

  if(texType == eGL_TEXTURE_BUFFER)
  {
    GLuint bufName = 0;
    drv.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_DATA_STORE_BINDING,
                                        (GLint *)&bufName);
    ResourceId id = m_pDriver->GetResourceManager()->GetID(BufferRes(m_pDriver->GetCtx(), bufName));

    GLuint offs = 0, size = 0;
    drv.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_OFFSET,
                                        (GLint *)&offs);
    drv.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_SIZE, (GLint *)&size);

    GetBufferData(id, offs, size, data);
    return;
  }

  if(texType == eGL_TEXTURE_2D_ARRAY || texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
     texType == eGL_TEXTURE_1D_ARRAY || texType == eGL_TEXTURE_CUBE_MAP ||
     texType == eGL_TEXTURE_CUBE_MAP_ARRAY)
  {
    // array size doesn't get mip'd down
    depth = 1;
    arraysize = texDetails.depth;
  }

  if(params.remap != RemapTexture::NoRemap)
  {
    GLenum remapFormat = eGL_RGBA8;
    if(params.remap == RemapTexture::RGBA8)
      remapFormat = eGL_RGBA8;
    else if(params.remap == RemapTexture::RGBA16)
      remapFormat = eGL_RGBA16F;
    else if(params.remap == RemapTexture::RGBA32)
      remapFormat = eGL_RGBA32F;

    if(intFormat != remapFormat)
    {
      MakeCurrentReplayContext(m_DebugCtx);

      GLenum finalFormat = IsSRGBFormat(intFormat) ? eGL_SRGB8_ALPHA8 : remapFormat;
      GLenum newtarget = (texType == eGL_TEXTURE_3D ? eGL_TEXTURE_3D : eGL_TEXTURE_2D);

      // create temporary texture of width/height in the new format to render to
      drv.glGenTextures(1, &tempTex);
      drv.glBindTexture(newtarget, tempTex);
      if(newtarget == eGL_TEXTURE_3D)
        drv.glTextureImage3DEXT(tempTex, newtarget, 0, finalFormat, width, height, depth, 0,
                                GetBaseFormat(finalFormat), GetDataType(finalFormat), NULL);
      else
        drv.glTextureImage2DEXT(tempTex, newtarget, 0, finalFormat, width, height, 0,
                                GetBaseFormat(finalFormat), GetDataType(finalFormat), NULL);
      drv.glTexParameteri(newtarget, eGL_TEXTURE_MAX_LEVEL, 0);

      // create temp framebuffer
      GLuint fbo = 0;
      drv.glGenFramebuffers(1, &fbo);
      drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);

      drv.glTexParameteri(newtarget, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
      drv.glTexParameteri(newtarget, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
      drv.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
      drv.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      drv.glTexParameteri(newtarget, eGL_TEXTURE_WRAP_R, eGL_CLAMP_TO_EDGE);
      if(newtarget == eGL_TEXTURE_3D)
        drv.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D, tempTex,
                                   0, 0);
      else if(newtarget == eGL_TEXTURE_2D)
        drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, newtarget, tempTex, 0);

      float col[] = {0.0f, 0.0f, 0.0f, 1.0f};
      drv.glClearBufferfv(eGL_COLOR, 0, col);

      // render to the temp texture to do the downcast
      float oldW = DebugData.outWidth;
      float oldH = DebugData.outHeight;

      DebugData.outWidth = float(width);
      DebugData.outHeight = float(height);

      GLenum baseFormat = !IsCompressedFormat(intFormat) ? GetBaseFormat(intFormat) : eGL_RGBA;

      for(GLsizei d = 0; d < (newtarget == eGL_TEXTURE_3D ? depth : 1); d++)
      {
        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = params.resolve ? ~0U : arrayIdx;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        if(samples > 1)
          texDisplay.sliceFace /= samples;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = CompType::Typeless;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        if(newtarget == eGL_TEXTURE_3D)
        {
          drv.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D,
                                     tempTex, 0, (GLint)d);
          texDisplay.sliceFace = (uint32_t)d;
        }

        drv.glViewport(0, 0, width, height);

        GLboolean color_mask[4];
        drv.glGetBooleanv(eGL_COLOR_WRITEMASK, color_mask);

        // for depth, ensure we only write to the red channel, don't write into 'stencil' in green
        // with depth data
        if(baseFormat == eGL_DEPTH_COMPONENT || baseFormat == eGL_DEPTH_STENCIL)
        {
          drv.glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
        }

        RenderTextureInternal(texDisplay, 0);

        drv.glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
      }

      // do one more time for the stencil
      if(baseFormat == eGL_DEPTH_STENCIL)
      {
        TextureDisplay texDisplay;

        texDisplay.green = true;
        texDisplay.red = texDisplay.blue = texDisplay.alpha = false;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.mip = mip;
        texDisplay.sampleIdx = params.resolve ? ~0U : arrayIdx;
        texDisplay.customShaderId = ResourceId();
        texDisplay.sliceFace = arrayIdx;
        if(samples > 1)
          texDisplay.sliceFace /= samples;
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeHint = CompType::Typeless;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        drv.glViewport(0, 0, width, height);

        GLboolean color_mask[4];
        drv.glGetBooleanv(eGL_COLOR_WRITEMASK, color_mask);
        drv.glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);

        RenderTextureInternal(texDisplay, 0);

        drv.glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
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

      drv.glDeleteFramebuffers(1, &fbo);
    }
  }
  else if(params.resolve && samples > 1)
  {
    MakeCurrentReplayContext(m_DebugCtx);

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    drv.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    // create temporary texture of width/height in same format to render to
    drv.glGenTextures(1, &tempTex);
    drv.glBindTexture(eGL_TEXTURE_2D, tempTex);
    drv.glTextureImage2DEXT(tempTex, eGL_TEXTURE_2D, 0, intFormat, width, height, 0,
                            GetBaseFormat(intFormat), GetDataType(intFormat), NULL);
    drv.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    // create temp framebuffers
    GLuint fbos[2] = {0};
    drv.glGenFramebuffers(2, fbos);

    drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[0]);
    drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D, tempTex, 0);

    drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[1]);
    if(texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texname, 0, arrayIdx);
    else
      drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texType, texname, 0);

    // do default resolve (framebuffer blit)
    drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbos[0]);
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, fbos[1]);

    float col[] = {0.3f, 0.4f, 0.5f, 1.0f};
    drv.glClearBufferfv(eGL_COLOR, 0, col);

    SafeBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, eGL_NEAREST);

    // rewrite the variables to temporary texture
    texType = eGL_TEXTURE_2D;
    texname = tempTex;
    depth = 1;
    mip = 0;
    arrayIdx = 0;
    arraysize = 1;
    samples = 1;

    drv.glDeleteFramebuffers(2, fbos);

    drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);
  }
  else if(samples > 1)
  {
    MakeCurrentReplayContext(m_DebugCtx);

    // copy multisampled texture to an array. This creates tempTex and returns it in that variable,
    // for us to own
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
    MakeCurrentReplayContext(m_DebugCtx);

    PixelPackState pack;
    pack.Fetch(true);

    ResetPixelPackState(true, 1);

    if(texType == eGL_RENDERBUFFER)
    {
      // do blit from renderbuffer to texture
      MakeCurrentReplayContext(&m_ReplayCtx);

      GLuint curDrawFBO = 0;
      GLuint curReadFBO = 0;
      drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
      drv.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

      drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
      drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

      GLenum b = GetBaseFormat(texDetails.internalFormat);

      GLbitfield mask = GL_COLOR_BUFFER_BIT;

      if(b == eGL_DEPTH_COMPONENT)
        mask = GL_DEPTH_BUFFER_BIT;
      else if(b == eGL_STENCIL)
        mask = GL_STENCIL_BUFFER_BIT;
      else if(b == eGL_DEPTH_STENCIL)
        mask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;

      SafeBlitFramebuffer(0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width,
                          texDetails.height, mask, eGL_NEAREST);

      drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
      drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

      // then proceed to read from the texture
      texname = texDetails.renderbufferReadTex;
      texType = eGL_TEXTURE_2D;

      MakeCurrentReplayContext(m_DebugCtx);
    }

    GLenum binding = TextureBinding(texType);

    GLuint prevtex = 0;
    drv.glGetIntegerv(binding, (GLint *)&prevtex);

    drv.glBindTexture(texType, texname);

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

    size_t dataSize = 0;

    if(IsCompressedFormat(intFormat))
    {
      dataSize = (size_t)GetCompressedByteSize(width, height, depth, intFormat);

      // contains a single slice
      data.resize(dataSize);

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
            texDetails.GetCompressedImageDataGLES(mip, target, dataSize * arraysize,
                                                  m_GetTexturePrevData[mip]);
          else
            drv.glGetCompressedTexImage(target, mip, m_GetTexturePrevData[mip]);
        }

        // now copy the slice from the cache into ret
        byte *src = m_GetTexturePrevData[mip];
        src += dataSize * arrayIdx;

        memcpy(data.data(), src, dataSize);
      }
      else
      {
        // for non-arrays we can just readback without caching
        if(IsGLES)
          texDetails.GetCompressedImageDataGLES(mip, target, dataSize, data.data());
        else
          drv.glGetCompressedTexImage(target, mip, data.data());
      }
    }
    else
    {
      GLenum fmt = GetBaseFormat(intFormat);
      GLenum type = GetDataType(intFormat);

      size_t rowSize = GetByteSize(width, 1, 1, fmt, type);
      dataSize = GetByteSize(width, height, depth, fmt, type);
      data.resize(dataSize);

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
          drv.glGetTexImage(target, (GLint)mip, fmt, type, m_GetTexturePrevData[mip]);
        }

        // now copy the slice from the cache into ret
        byte *src = m_GetTexturePrevData[mip];
        src += dataSize * arrayIdx;

        memcpy(data.data(), src, dataSize);
      }
      else
      {
        drv.glGetTexImage(target, (GLint)mip, fmt, type, data.data());
      }

      // packed D24S8 comes out the wrong way around from what we expect, so we re-swizzle it.
      if(intFormat == eGL_DEPTH24_STENCIL8)
      {
        uint32_t *ptr = (uint32_t *)data.data();

        for(GLsizei y = 0; y < height; y++)
        {
          for(GLsizei x = 0; x < width; x++)
          {
            const uint32_t val = *ptr;
            *ptr = (val >> 8) | ((val & 0xff) << 24);
            ptr++;
          }
        }
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
          dst = data.data() + d * sliceSize;
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

    pack.Apply(true);

    drv.glBindTexture(texType, prevtex);
  }

  if(tempTex)
    drv.glDeleteTextures(1, &tempTex);
}

void GLReplay::BuildCustomShader(ShaderEncoding sourceEncoding, bytebuf source,
                                 const std::string &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId *id, std::string *errors)
{
  BuildTargetShader(sourceEncoding, source, entry, compileFlags, type, id, errors);
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
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = shader;
  disp.resourceId = texid;
  disp.typeHint = typeHint;
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.mip = mip;
  disp.sampleIdx = sampleIdx;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = 0.0f;
  disp.rangeMax = 1.0f;
  disp.rawOutput = false;
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

  m_pDriver->glDeleteShader(m_pDriver->GetResourceManager()->GetCurrentResource(id).name);
}

void GLReplay::BuildTargetShader(ShaderEncoding sourceEncoding, bytebuf source,
                                 const std::string &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId *id, std::string *errors)
{
  if(id == NULL || errors == NULL)
  {
    if(id)
      *id = ResourceId();
    return;
  }

  WrappedOpenGL &drv = *m_pDriver;

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

  std::string glsl((char *)source.begin(), (char *)source.end());
  const char *src = glsl.c_str();
  GLuint shader = drv.glCreateShader(shtype);
  drv.glShaderSource(shader, 1, &src, NULL);
  drv.glCompileShader(shader);

  GLint status = 0;
  drv.glGetShaderiv(shader, eGL_COMPILE_STATUS, &status);

  if(errors)
  {
    GLint len = 1024;
    drv.glGetShaderiv(shader, eGL_INFO_LOG_LENGTH, &len);
    char *buffer = new char[len + 1];
    drv.glGetShaderInfoLog(shader, len, NULL, buffer);
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
  ClearPostVSCache();
}

void GLReplay::RemoveReplacement(ResourceId id)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->RemoveReplacement(id);
  ClearPostVSCache();
}

void GLReplay::FreeTargetResource(ResourceId id)
{
  MakeCurrentReplayContext(&m_ReplayCtx);
  m_pDriver->FreeTargetResource(id);
}

ResourceId GLReplay::CreateProxyTexture(const TextureDescription &templateTex)
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  GLuint tex = 0;
  drv.glGenTextures(1, &tex);

  GLenum intFormat = MakeGLFormat(templateTex.format);
  bool isCompressed = IsCompressedFormat(intFormat);

  GLenum baseFormat = eGL_RGBA;
  GLenum dataType = eGL_UNSIGNED_BYTE;
  if(!isCompressed)
  {
    baseFormat = GetBaseFormat(intFormat);
    dataType = GetDataType(intFormat);
  }

  GLenum target = eGL_NONE;

  switch(templateTex.type)
  {
    case TextureType::Unknown: break;
    case TextureType::Buffer:
    case TextureType::Texture1D: target = eGL_TEXTURE_1D; break;
    case TextureType::Texture1DArray: target = eGL_TEXTURE_1D_ARRAY; break;
    case TextureType::TextureRect:
    case TextureType::Texture2D: target = eGL_TEXTURE_2D; break;
    case TextureType::Texture2DArray: target = eGL_TEXTURE_2D_ARRAY; break;
    case TextureType::Texture2DMS: target = eGL_TEXTURE_2D_MULTISAMPLE; break;
    case TextureType::Texture2DMSArray: target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
    case TextureType::Texture3D: target = eGL_TEXTURE_3D; break;
    case TextureType::TextureCube: target = eGL_TEXTURE_CUBE_MAP; break;
    case TextureType::TextureCubeArray: target = eGL_TEXTURE_CUBE_MAP_ARRAY; break;
    case TextureType::Count: RDCERR("Invalid texture dimension"); break;
  }

  if(target != eGL_NONE)
  {
    drv.glBindTexture(target, tex);

    if(target == eGL_TEXTURE_2D_MULTISAMPLE)
    {
      drv.glTextureStorage2DMultisampleEXT(tex, target, templateTex.msSamp, intFormat,
                                           templateTex.width, templateTex.height, GL_TRUE);
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
      drv.glTextureStorage3DMultisampleEXT(tex, target, templateTex.msSamp, intFormat,
                                           templateTex.width, templateTex.height,
                                           templateTex.arraysize, GL_TRUE);
    }
    else
    {
      GLsizei w = (GLsizei)templateTex.width;
      GLsizei h = (GLsizei)templateTex.height;
      GLsizei d = (GLsizei)templateTex.depth;
      int dim = (int)templateTex.dimension;

      if(target == eGL_TEXTURE_1D_ARRAY)
      {
        h = templateTex.arraysize;
        dim = 2;
      }
      else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
      {
        d = templateTex.arraysize;
        dim = 3;
      }

      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      int count = ARRAY_COUNT(targets);

      if(target != eGL_TEXTURE_CUBE_MAP)
      {
        targets[0] = target;
        count = 1;
      }

      for(int m = 0; m < (int)templateTex.mips; m++)
      {
        for(int t = 0; t < count; t++)
        {
          if(isCompressed)
          {
            GLsizei compSize = (GLsizei)GetCompressedByteSize(w, h, d, intFormat);

            std::vector<byte> dummy;
            dummy.resize(compSize);

            if(dim == 1)
              drv.glCompressedTextureImage1DEXT(tex, targets[t], m, intFormat, w, 0, compSize,
                                                &dummy[0]);
            else if(dim == 2)
              drv.glCompressedTextureImage2DEXT(tex, targets[t], m, intFormat, w, h, 0, compSize,
                                                &dummy[0]);
            else if(dim == 3)
              drv.glCompressedTextureImage3DEXT(tex, targets[t], m, intFormat, w, h, d, 0, compSize,
                                                &dummy[0]);
          }
          else
          {
            if(dim == 1)
              drv.glTextureImage1DEXT(tex, targets[t], m, intFormat, w, 0, baseFormat, dataType,
                                      NULL);
            else if(dim == 2)
              drv.glTextureImage2DEXT(tex, targets[t], m, intFormat, w, h, 0, baseFormat, dataType,
                                      NULL);
            else if(dim == 3)
              drv.glTextureImage3DEXT(tex, targets[t], m, intFormat, w, h, d, 0, baseFormat,
                                      dataType, NULL);
          }
        }

        w = RDCMAX(1, w >> 1);
        if(target != eGL_TEXTURE_1D_ARRAY)
          h = RDCMAX(1, h >> 1);
        if(target != eGL_TEXTURE_2D_ARRAY && target != eGL_TEXTURE_CUBE_MAP_ARRAY)
          d = RDCMAX(1, d >> 1);
      }
    }

    drv.glTexParameteri(target, eGL_TEXTURE_MAX_LEVEL, templateTex.mips - 1);
  }

  // Swizzle R/B channels only for non BGRA textures
  if(templateTex.format.BGRAOrder() && target != eGL_NONE && baseFormat != eGL_BGRA)
  {
    if(HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle])
    {
      GLint bgraSwizzle[] = {eGL_BLUE, eGL_GREEN, eGL_RED, eGL_ALPHA};
      GLint bgrSwizzle[] = {eGL_BLUE, eGL_GREEN, eGL_RED, eGL_ONE};

      if(templateTex.format.compCount == 4)
        SetTextureSwizzle(tex, target, (GLenum *)bgraSwizzle);
      else if(templateTex.format.compCount == 3)
        SetTextureSwizzle(tex, target, (GLenum *)bgrSwizzle);
      else
        RDCERR("Unexpected component count %d for BGRA order format", templateTex.format.compCount);
    }
    else
    {
      RDCERR("Can't create a BGRA proxy texture without texture swizzle extension");
    }
  }

  ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(m_pDriver->GetCtx(), tex));

  return id;
}

void GLReplay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                   size_t dataSize)
{
  WrappedOpenGL &drv = *m_pDriver;

  GLuint tex = m_pDriver->GetResourceManager()->GetCurrentResource(texid).name;

  auto &texdetails = m_pDriver->m_Textures[texid];

  GLenum fmt = texdetails.internalFormat;
  GLenum target = texdetails.curType;

  GLint depth = 1;
  if(target == eGL_TEXTURE_3D)
    depth = RDCMAX(1, texdetails.depth >> mip);

  GLint width = RDCMAX(1, texdetails.width >> mip);
  GLint height = RDCMAX(1, texdetails.height >> mip);

  if(IsCompressedFormat(fmt))
  {
    if(target == eGL_TEXTURE_1D)
    {
      drv.glCompressedTextureSubImage1DEXT(tex, target, (GLint)mip, 0, width, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_1D_ARRAY)
    {
      drv.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, (GLint)arrayIdx, width, 1,
                                           fmt, (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D)
    {
      drv.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, width, height, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    {
      drv.glCompressedTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, (GLint)arrayIdx, width,
                                           height, 1, fmt, (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_3D)
    {
      drv.glCompressedTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, 0, width, height, depth,
                                           fmt, (GLsizei)dataSize, data);
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

      drv.glCompressedTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, width, height, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE || target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
      RDCERR("Unexpected compressed MSAA texture!");
    }
  }
  else
  {
    GLenum baseformat = GetBaseFormat(fmt);
    GLenum datatype = GetDataType(fmt);

    if(dataSize < GetByteSize(width, height, depth, baseformat, datatype))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    if(target == eGL_TEXTURE_1D)
    {
      drv.glTextureSubImage1DEXT(tex, target, (GLint)mip, 0, width, baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_1D_ARRAY)
    {
      drv.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, (GLint)arrayIdx, width, 1, baseformat,
                                 datatype, data);
    }
    else if(target == eGL_TEXTURE_2D)
    {
      drv.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, width, height, baseformat, datatype,
                                 data);
    }
    else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    {
      drv.glTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, (GLint)arrayIdx, width, height, 1,
                                 baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_3D)
    {
      drv.glTextureSubImage3DEXT(tex, target, (GLint)mip, 0, 0, 0, width, height, depth, baseformat,
                                 datatype, data);
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

      drv.glTextureSubImage2DEXT(tex, target, (GLint)mip, 0, 0, width, height, baseformat, datatype,
                                 data);
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE || target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
      // create a temporary 2D array texture to upload the data to. Must use TexStorage to allow
      // texture views inside CopyArrayToTex2DMS
      GLuint uploadTex = 0;
      drv.glGenTextures(1, &uploadTex);
      drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, uploadTex);
      drv.glTextureStorage3DEXT(uploadTex, eGL_TEXTURE_2D_ARRAY, 1, texdetails.internalFormat,
                                width, height, texdetails.samples * RDCMAX(1, texdetails.depth));
      drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

      // packed D24S8 is expected the wrong way around from comes in, so we re-swizzle it here
      if(texdetails.internalFormat == eGL_DEPTH24_STENCIL8)
      {
        const uint32_t *srcptr = (const uint32_t *)data;
        bytebuf swizzled;
        swizzled.resize(dataSize);
        uint32_t *dstptr = (uint32_t *)swizzled.data();

        for(GLsizei y = 0; y < height; y++)
        {
          for(GLsizei x = 0; x < width; x++)
          {
            const uint32_t val = *srcptr;
            *dstptr = (val << 8) | ((val & 0xff000000) >> 24);
            srcptr++;
            dstptr++;
          }
        }

        // upload the data to the given slice
        drv.glTextureSubImage3DEXT(uploadTex, eGL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)arrayIdx, width,
                                   height, 1, baseformat, datatype, swizzled.data());
      }
      else
      {
        // upload the data to the given slice
        drv.glTextureSubImage3DEXT(uploadTex, eGL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)arrayIdx, width,
                                   height, 1, baseformat, datatype, data);
      }

      // copy this slice into the 2D MSAA texture
      CopyArrayToTex2DMS(tex, uploadTex, width, height, 1, texdetails.samples,
                         texdetails.internalFormat, arrayIdx);

      // delete the temporary texture
      drv.glDeleteTextures(1, &uploadTex);
    }
  }
}

bool GLReplay::IsTextureSupported(const ResourceFormat &format)
{
  // We couldn't create proxy textures for ASTC textures (see MakeGLFormat). So we give back false
  // and let RemapProxyTextureIfNeeded to set remap type for them.
  if(format.type == ResourceFormatType::ASTC)
    return false;

  // BGRA is not accepted as an internal format in case of GL
  // EXT_texture_format_BGRA8888 is required for creating BGRA proxy textures in case of GLES
  if(format.BGRAOrder())
    return IsGLES && HasExt[EXT_texture_format_BGRA8888];

  return true;
}

bool GLReplay::NeedRemapForFetch(const ResourceFormat &format)
{
  if(format.compType == CompType::Depth || format.type == ResourceFormatType::D16S8 ||
     format.type == ResourceFormatType::D24S8 || format.type == ResourceFormatType::D32S8)
    return IsGLES && !HasExt[NV_read_depth];
  return false;
}

ResourceId GLReplay::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  WrappedOpenGL &drv = *m_pDriver;

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
  drv.glGenBuffers(1, &buf);
  drv.glBindBuffer(target, buf);
  drv.glNamedBufferDataEXT(buf, (GLsizeiptr)templateBuf.length, NULL, eGL_DYNAMIC_DRAW);

  ResourceId id = m_pDriver->GetResourceManager()->GetID(BufferRes(m_pDriver->GetCtx(), buf));

  return id;
}

void GLReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
  GLuint buf = m_pDriver->GetResourceManager()->GetCurrentResource(bufid).name;

  m_pDriver->glNamedBufferSubDataEXT(buf, 0, dataSize, data);
}

std::vector<EventUsage> GLReplay::GetUsage(ResourceId id)
{
  return m_pDriver->GetUsage(id);
}

std::vector<PixelModification> GLReplay::PixelHistory(std::vector<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      uint32_t slice, uint32_t mip,
                                                      uint32_t sampleIdx, CompType typeHint)
{
  GLNOTIMP("GLReplay::PixelHistory");
  return std::vector<PixelModification>();
}

ShaderDebugTrace GLReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                       uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  GLNOTIMP("DebugVertex");
  return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                      uint32_t primitive)
{
  GLNOTIMP("DebugPixel");
  return ShaderDebugTrace();
}

ShaderDebugTrace GLReplay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
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
#if ENABLED(RDOC_APPLE)
    GL.glFinish();
#endif

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

ReplayStatus CreateReplayDevice(RDCDriver rdcdriver, RDCFile *rdc, GLPlatform &platform,
                                IReplayDriver **&driver)
{
  GLInitParams initParams;
  uint64_t ver = GLInitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!GLInitParams::IsSupportedVersion(ver))
    {
      RDCERR("Incompatible OpenGL serialise version %llu", ver);
      return ReplayStatus::APIIncompatibleVersion;
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    ser.SetVersion(ver);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RDCERR("Expected to get a DriverInit chunk, instead got %u", chunk);
      return ReplayStatus::FileCorrupted;
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      RDCERR("Failed reading driver init params.");
      return ReplayStatus::FileIOFailed;
    }
  }

  GLWindowingData data = {};

  ReplayStatus status = platform.InitialiseAPI(data, rdcdriver);

  // any errors will be already printed, just pass the error up
  if(status != ReplayStatus::Succeeded)
    return status;

  bool current = platform.MakeContextCurrent(data);
  if(!current)
  {
    RDCERR("Couldn't active the created GL ES context");
    platform.DeleteReplayContext(data);
    return ReplayStatus::APIInitFailed;
  }

  // we use the platform's function which tries GL's GetProcAddress first, then falls back to
  // regular function lookup
  GL.PopulateWithCallback([&platform](const char *func) { return platform.GetReplayFunction(func); });

  FetchEnabledExtensions();

  // see gl_emulated.cpp
  GL.EmulateUnsupportedFunctions();
  GL.EmulateRequiredExtensions();

  bool extensionsValidated = CheckReplayContext();

  if(!extensionsValidated)
  {
    platform.DeleteReplayContext(data);
    return ReplayStatus::APIInitFailed;
  }

  bool functionsValidated = ValidateFunctionPointers();
  if(!functionsValidated)
  {
    platform.DeleteReplayContext(data);
    return ReplayStatus::APIHardwareUnsupported;
  }

  WrappedOpenGL *gldriver = new WrappedOpenGL(platform);
  gldriver->SetDriverType(rdcdriver);

  GL.DriverForEmulation(gldriver);

  RDCLOG("Created %s replay device.", ToStr(rdcdriver).c_str());

  GLReplay *replay = gldriver->GetReplay();
  replay->SetProxy(rdc == NULL);
  replay->SetReplayData(data);

  gldriver->Initialise(initParams, ver);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}

void GL_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  GLDummyPlatform dummy;
  WrappedOpenGL device(dummy);

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = device.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    device.GetStructuredFile().Swap(output);
}

static StructuredProcessRegistration GLProcessRegistration(RDCDriver::OpenGL, &GL_ProcessStructured);
static StructuredProcessRegistration GLESProcessRegistration(RDCDriver::OpenGLES,
                                                             &GL_ProcessStructured);

std::vector<GLVersion> GetReplayVersions(RDCDriver api)
{
  // try to create all versions from highest down to lowest in order to get the highest versioned
  // context we can
  if(api == RDCDriver::OpenGLES)
  {
    return {
        {3, 2}, {3, 1}, {3, 0},
    };
  }
  else
  {
    return {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2},
    };
  }
}

#if defined(RENDERDOC_SUPPORT_GL)

ReplayStatus GL_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating an OpenGL replay device");

  bool load_ok = GetGLPlatform().PopulateForReplay();

  if(!load_ok)
  {
    RDCERR("Couldn't find required platform GL function addresses");
    return ReplayStatus::APIInitFailed;
  }

  return CreateReplayDevice(rdc ? rdc->GetDriver() : RDCDriver::OpenGL, rdc, GetGLPlatform(), driver);
}

static DriverRegistration GLDriverRegistration(RDCDriver::OpenGL, &GL_CreateReplayDevice);

#endif

#if defined(RENDERDOC_SUPPORT_GLES)

ReplayStatus GLES_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCLOG("Creating an OpenGL ES replay device");

  // for GLES replay, we try to use EGL if it's available. If it's not available, we look to see if
  // we can create an OpenGL ES context via the platform GL functions
  if(GetEGLPlatform().CanCreateGLESContext())
  {
    bool load_ok = GetEGLPlatform().PopulateForReplay();

    if(!load_ok)
    {
      RDCERR("Couldn't find required EGL function addresses");
      return ReplayStatus::APIInitFailed;
    }

    RDCLOG("Initialising GLES replay via libEGL");

    return CreateReplayDevice(rdc ? rdc->GetDriver() : RDCDriver::OpenGLES, rdc, GetEGLPlatform(),
                              driver);
  }
#if defined(RENDERDOC_SUPPORT_GL)
  else if(GetGLPlatform().CanCreateGLESContext())
  {
    RDCLOG("libEGL is not available, falling back to EXT_create_context_es2_profile");

    bool load_ok = GetGLPlatform().PopulateForReplay();

    if(!load_ok)
    {
      RDCERR("Couldn't find required GLX function addresses");
      return ReplayStatus::APIInitFailed;
    }

    return CreateReplayDevice(rdc ? rdc->GetDriver() : RDCDriver::OpenGLES, rdc, GetGLPlatform(),
                              driver);
  }

  RDCERR(
      "libEGL not available, and GL cannot initialise or doesn't support "
      "EXT_create_context_es2_profile");
  return ReplayStatus::APIInitFailed;
#else
  // no GL support, no fallback apart from EGL

  RDCERR("libEGL is not available");
  return ReplayStatus::APIInitFailed;
#endif
}

static DriverRegistration GLESDriverRegistration(RDCDriver::OpenGLES, &GLES_CreateReplayDevice);

#endif
