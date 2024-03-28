/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "core/settings.h"
#include "data/glsl_shaders.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/arm/arm_counters.h"
#include "driver/ihv/intel/intel_gl_counters.h"
#include "driver/ihv/nv/nv_gl_counters.h"
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

RDOC_CONFIG(bool, OpenGL_HardwareCounters, true,
            "Enable support for IHV-specific hardware counters on OpenGL.");

static const char *SPIRVDisassemblyTarget = "SPIR-V (RenderDoc)";

GLReplay::GLReplay(WrappedOpenGL *d)
{
  m_pDriver = d;

  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(GLReplay));

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
  SAFE_DELETE(m_pARMCounters);
#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
  SAFE_DELETE(m_pNVCounters);
#endif

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

RDResult GLReplay::FatalErrorCheck()
{
  return m_pDriver->FatalErrorCheck();
}

IReplayDriver *GLReplay::MakeDummyDriver()
{
  // gather up the shaders we've allocated to pass to the dummy driver
  rdcarray<ShaderReflection *> shaders;
  for(auto it = m_pDriver->m_Shaders.begin(); it != m_pDriver->m_Shaders.end(); it++)
  {
    shaders.push_back(it->second.reflection);
    it->second.reflection = NULL;
  }

  IReplayDriver *dummy = new DummyDriver(this, shaders, m_pDriver->DetachStructuredFile());

  return dummy;
}

RDResult GLReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
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

SDFile *GLReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
}

rdcarray<uint32_t> GLReplay::GetPassEvents(uint32_t eventId)
{
  rdcarray<uint32_t> passEvents;

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  const ActionDescription *start = action;
  while(start && start->previous && !(start->previous->flags & ActionFlags::Clear))
  {
    const ActionDescription *prev = start->previous;

    if(start->outputs != prev->outputs || start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  while(start)
  {
    if(start == action)
      break;

    if(start->flags & ActionFlags::Drawcall)
      passEvents.push_back(start->eventId);

    start = start->next;
  }

  return passEvents;
}

rdcarray<WindowingSystem> GLReplay::GetSupportedWindowSystems()
{
  rdcarray<WindowingSystem> ret;

#if ENABLED(RDOC_LINUX)

#if ENABLED(RDOC_WAYLAND)
  // if wayland is supported and a display is configured, we *must* get wayland surfaces to render
  // on
  if(RenderDoc::Inst().GetGlobalEnvironment().waylandDisplay)
  {
    ret.push_back(WindowingSystem::Wayland);
  }
  else
#endif
  {
    // only Xlib supported for GLX. We can't report XCB here since we need the Display, and that
    // can't be obtained from XCB. The application is free to use XCB internally but it would have
    // to create a hybrid and initialise XCB out of Xlib, to be able to provide the display and
    // drawable to us.
    ret.push_back(WindowingSystem::Xlib);
  }

#elif ENABLED(RDOC_ANDROID)
  ret.push_back(WindowingSystem::Android);
#elif ENABLED(RDOC_APPLE)
  ret.push_back(WindowingSystem::MacOS);
#elif ENABLED(RDOC_WIN32)
  ret.push_back(WindowingSystem::Win32);
#endif

  return ret;
}

ResourceId GLReplay::GetLiveID(ResourceId id)
{
  if(!m_pDriver->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDriver->GetResourceManager()->GetLiveID(id);
}

rdcarray<GPUDevice> GLReplay::GetAvailableGPUs()
{
  // GL doesn't support multiple GPUs, return an empty list
  return {};
}

APIProperties GLReplay::GetAPIProperties()
{
  APIProperties ret = m_pDriver->APIProps;

  ret.pipelineType = GraphicsAPI::OpenGL;
  ret.localRenderer = GraphicsAPI::OpenGL;
  ret.degraded = m_Degraded;
  ret.vendor = m_DriverInfo.vendor;
  ret.pixelHistory = true;

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

rdcarray<ResourceDescription> GLReplay::GetResources()
{
  return m_Resources;
}

void GLReplay::SetReplayData(GLWindowingData data)
{
  m_ReplayCtx = data;
  m_pDriver->RegisterReplayContext(m_ReplayCtx, NULL, true, true);

  m_pDriver->RegisterDebugCallback();

  InitDebugData();

  if(!HasDebugContext())
    return;

  if(!m_Proxy && OpenGL_HardwareCounters())
  {
    AMDCounters *countersAMD = NULL;
    IntelGlCounters *countersIntel = NULL;
    ARMCounters *countersARM = NULL;
#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
    NVGLCounters *countersNV = NULL;
#endif

    bool isMesa = false;

    // try to identify mesa - don't enable any IHV counters when running mesa.
    {
      WrappedOpenGL &drv = *m_pDriver;

      const char *version = (const char *)drv.glGetString(eGL_VERSION);
      const char *vendor = (const char *)drv.glGetString(eGL_VENDOR);
      const char *renderer = (const char *)drv.glGetString(eGL_RENDERER);

      for(rdcstr haystack : {strlower(version), strlower(vendor), strlower(renderer)})
      {
        haystack = " " + haystack + " ";

        // the version should always contain 'mesa', but it's also commonly present in either vendor
        // or renderer - except for nouveau which we look for separately
        for(const char *needle : {" mesa ", "nouveau"})
        {
          if(haystack.contains(needle))
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
      if(m_DriverInfo.vendor == GPUVendor::Intel)
      {
        RDCLOG("Intel GPU detected - trying to initialise Intel GL counters");
        countersIntel = new IntelGlCounters();
      }
      else if(m_DriverInfo.vendor == GPUVendor::AMD || m_DriverInfo.vendor == GPUVendor::Samsung)
      {
        RDCLOG("AMD or Samsung GPU detected - trying to initialise AMD counters");
        countersAMD = new AMDCounters();
      }
      else if(m_DriverInfo.vendor == GPUVendor::ARM)
      {
        RDCLOG("ARM Mali GPU detected - trying to initialise ARM counters");
        countersARM = new ARMCounters();
      }
#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
      else if(m_DriverInfo.vendor == GPUVendor::nVidia)
      {
        RDCLOG("NVIDIA GPU detected - trying to initialise NVIDIA counters");
        countersNV = new NVGLCounters();
      }
#endif
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

    if(countersARM && countersARM->Init())
    {
      m_pARMCounters = countersARM;
    }
    else
    {
      delete countersARM;
      m_pARMCounters = NULL;
    }

#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
    if(countersNV && countersNV->Init(m_pDriver))
    {
      m_pNVCounters = countersNV;
    }
    else
    {
      delete countersNV;
      m_pNVCounters = NULL;
    }
#endif
  }
}

void GLReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret)
{
  if(m_pDriver->m_Buffers.find(buff) == m_pDriver->m_Buffers.end())
  {
    RDCWARN("Requesting data for non-existant buffer %s", ToStr(buff).c_str());
    ret.clear();
    return;
  }

  auto &buf = m_pDriver->m_Buffers[buff];

  uint64_t bufsize = buf.size;

  if(offset >= bufsize)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(len == 0 || len > bufsize)
  {
    len = bufsize;
  }

  if(offset + len > bufsize)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%llu)",
            offset, len, bufsize);
    len = RDCMIN(len, bufsize - offset);
  }

  ret.resize((size_t)len);

  WrappedOpenGL &drv = *m_pDriver;

  GLuint oldbuf = 0;
  drv.glGetIntegerv(eGL_COPY_READ_BUFFER_BINDING, (GLint *)&oldbuf);

  drv.glBindBuffer(eGL_COPY_READ_BUFFER, buf.resource.name);

  drv.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)offset, (GLsizeiptr)len, &ret[0]);

  drv.glBindBuffer(eGL_COPY_READ_BUFFER, oldbuf);
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
      RDCERR("Details for invalid texture id %s requested", ToStr(id).c_str());

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

    if(res.internalFormat == eGL_NONE)
    {
      tex.format = ResourceFormat();
    }
    else
    {
      tex.format = MakeResourceFormat(eGL_TEXTURE_2D, res.internalFormat);

      if(IsDepthStencilFormat(res.internalFormat))
        tex.creationFlags |= TextureCategory::DepthTarget;
    }

    tex.byteSize =
        (tex.width * tex.height) * (tex.format.compByteWidth * tex.format.compCount) * tex.msSamp;

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
      tex.arraysize = height;
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
  for(uint32_t m = 0; m < tex.mips; m++)
  {
    if(fmt == eGL_NONE)
    {
    }
    else if(compressed)
    {
      tex.byteSize += (uint64_t)GetCompressedByteSize(RDCMAX(1U, tex.width >> m),
                                                      RDCMAX(1U, tex.height >> m), 1, (GLenum)fmt);
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

  tex.byteSize *= tex.arraysize;
  tex.byteSize *= tex.msSamp;

  m_CachedTextures[id] = tex;
}

BufferDescription GLReplay::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &res = m_pDriver->m_Buffers[id];

  if(res.resource.Namespace == eResUnknown)
  {
    RDCERR("Details for invalid buffer id %s requested", ToStr(id).c_str());
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

  ret.length = res.size;

  if(res.curType != eGL_NONE)
    drv.glBindBuffer(res.curType, prevBind);

  return ret;
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

rdcarray<BufferDescription> GLReplay::GetBuffers()
{
  rdcarray<BufferDescription> ret;

  for(auto it = m_pDriver->m_Buffers.begin(); it != m_pDriver->m_Buffers.end(); ++it)
  {
    // skip buffers that aren't from the log
    if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(GetBuffer(it->first));
  }

  return ret;
}

rdcarray<TextureDescription> GLReplay::GetTextures()
{
  rdcarray<TextureDescription> ret;
  ret.reserve(m_pDriver->m_Textures.size());

  for(auto it = m_pDriver->m_Textures.begin(); it != m_pDriver->m_Textures.end(); ++it)
  {
    auto &res = m_pDriver->m_Textures[it->first];

    // skip textures that aren't from the log (except the 'default backbuffer' textures)
    if(!(res.creationFlags & TextureCategory::SwapBuffer) &&
       m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    CacheTexture(it->first);
    ret.push_back(m_CachedTextures[it->first]);
  }

  return ret;
}

rdcarray<DebugMessage> GLReplay::GetDebugMessages()
{
  return m_pDriver->GetDebugMessages();
}

rdcarray<ShaderEntryPoint> GLReplay::GetShaderEntryPoints(ResourceId shader)
{
  if(m_pDriver->m_Shaders.find(shader) == m_pDriver->m_Shaders.end())
    return {};

  WrappedOpenGL::ShaderData &shaderDetails = m_pDriver->m_Shaders[shader];

  if(shaderDetails.reflection->resourceId == ResourceId())
  {
    RDCERR("Can't get shader details without successful reflect");
    return {};
  }

  return {{shaderDetails.reflection->entryPoint, shaderDetails.reflection->stage}};
}

ShaderReflection *GLReplay::GetShader(ResourceId pipeline, ResourceId shader, ShaderEntryPoint entry)
{
  auto &shaderDetails = m_pDriver->m_Shaders[shader];

  if(shaderDetails.reflection->resourceId == ResourceId())
  {
    RDCERR("Can't get shader details without successful reflect");
    return NULL;
  }

  return shaderDetails.reflection;
}

rdcarray<rdcstr> GLReplay::GetDisassemblyTargets(bool withPipeline)
{
  return {SPIRVDisassemblyTarget};
}

rdcstr GLReplay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                   const rdcstr &target)
{
  auto &shaderDetails =
      m_pDriver->m_Shaders[m_pDriver->GetResourceManager()->GetLiveID(refl->resourceId)];

  if(shaderDetails.sources.empty() && shaderDetails.spirvWords.empty())
    return "; Invalid Shader Specified";

  if(target == SPIRVDisassemblyTarget || target.empty())
  {
    rdcstr &disasm = shaderDetails.disassembly;

    if(disasm.empty())
      disasm = shaderDetails.spirv.Disassemble(refl->entryPoint, shaderDetails.spirvInstructionLines);

    return disasm;
  }

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
}

void GLReplay::SavePipelineState(uint32_t eventId)
{
  if(!m_GLPipelineState)
    return;

  GLPipe::State &pipe = *m_GLPipelineState;
  WrappedOpenGL &drv = *m_pDriver;
  GLResourceManager *rm = m_pDriver->GetResourceManager();

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLRenderState rs;
  rs.FetchState(&drv);

  // Index buffer

  ContextPair &ctx = drv.GetCtx();

  pipe.descriptorStore = m_pDriver->m_DescriptorsID;
  pipe.descriptorCount = EncodeGLDescriptorIndex({GLDescriptorMapping::Count, 0});
  pipe.descriptorByteSize = 1;

  m_Access.clear();

  GLuint vao = 0;
  drv.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&vao);
  pipe.vertexInput.vertexArrayObject = rm->GetOriginalID(rm->GetResID(VertexArrayRes(ctx, vao)));

  GLuint ibuffer = 0;
  drv.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&ibuffer);
  pipe.vertexInput.indexBuffer = rm->GetOriginalID(rm->GetResID(BufferRes(ctx, ibuffer)));

  pipe.vertexInput.primitiveRestart = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart] ||
                                      rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex];
  pipe.vertexInput.restartIndex = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex]
                                      ? ~0U
                                      : rs.PrimitiveRestartIndex;

  const GLDrawParams &drawParams = m_pDriver->GetDrawParameters(eventId);

  pipe.vertexInput.indexByteStride = drawParams.indexWidth;
  pipe.vertexInput.topology = drawParams.topo;

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
        rm->GetOriginalID(rm->GetResID(BufferRes(ctx, buffer)));

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
                            (GLfloat *)pipe.vertexInput.attributes[i].genericValue.floatValue.data());

    ResourceFormat fmt;

    fmt.type = ResourceFormatType::Regular;
    GLint compCount;
    drv.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint *)&compCount);

    fmt.compCount = (uint8_t)compCount;

    switch(type)
    {
      default:
      case eGL_BYTE:
        fmt.compByteWidth = 1;
        fmt.compType = CompType::SInt;
        break;
      case eGL_UNSIGNED_BYTE:
        fmt.compByteWidth = 1;
        fmt.compType = CompType::UInt;
        break;
      case eGL_SHORT:
        fmt.compByteWidth = 2;
        fmt.compType = CompType::SInt;
        break;
      case eGL_UNSIGNED_SHORT:
        fmt.compByteWidth = 2;
        fmt.compType = CompType::UInt;
        break;
      case eGL_INT:
        fmt.compByteWidth = 4;
        fmt.compType = CompType::SInt;
        break;
      case eGL_UNSIGNED_INT:
        fmt.compByteWidth = 4;
        fmt.compType = CompType::UInt;
        break;
      case eGL_FLOAT:
        fmt.compByteWidth = 4;
        fmt.compType = CompType::Float;
        break;
      case eGL_DOUBLE:
        fmt.compByteWidth = 8;
        fmt.compType = CompType::Float;
        break;
      case eGL_HALF_FLOAT:
        fmt.compByteWidth = 2;
        fmt.compType = CompType::Float;
        break;
      case eGL_INT_2_10_10_10_REV:
        fmt.type = ResourceFormatType::R10G10B10A2;
        fmt.compCount = 4;
        fmt.compType = CompType::SInt;
        break;
      case eGL_UNSIGNED_INT_2_10_10_10_REV:
        fmt.type = ResourceFormatType::R10G10B10A2;
        fmt.compCount = 4;
        fmt.compType = CompType::UInt;
        break;
      case eGL_UNSIGNED_INT_10F_11F_11F_REV:
        fmt.type = ResourceFormatType::R11G11B10;
        fmt.compCount = 3;
        fmt.compType = CompType::Float;
        // spec says this format is never normalized regardless.
        normalized = 0;
        break;
    }

    if(compCount == eGL_BGRA)
    {
      fmt.compByteWidth = 1;
      fmt.compCount = 4;
      fmt.SetBGRAOrder(true);
      fmt.compType = CompType::UNorm;

      // spec says BGRA inputs are ALWAYS normalised
      normalized = 1;

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

    // normalized/floatCast flags are irrelevant for float formats
    if(fmt.compType == CompType::SInt || fmt.compType == CompType::UInt)
    {
      // if it wasn't an integer, it's cast to float
      pipe.vertexInput.attributes[i].floatCast = !integer;

      // if we're casting, change the component type as appropriate
      if(!integer)
      {
        if(normalized != 0)
          fmt.compType = (fmt.compType == CompType::SInt) ? CompType::SNorm : CompType::UNorm;
      }
    }
    else
    {
      pipe.vertexInput.attributes[i].floatCast = false;
    }

    pipe.vertexInput.attributes[i].format = fmt;
  }

  pipe.vertexInput.provokingVertexLast = (rs.ProvokingVertex != eGL_FIRST_VERTEX_CONVENTION);

  pipe.vertexProcessing.defaultInnerLevel = rs.PatchParams.defaultInnerLevel;
  pipe.vertexProcessing.defaultOuterLevel = rs.PatchParams.defaultOuterLevel;

  pipe.vertexProcessing.discard = rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard];
  pipe.vertexProcessing.clipOriginLowerLeft = (rs.ClipOrigin != eGL_UPPER_LEFT);
  pipe.vertexProcessing.clipNegativeOneToOne = (rs.ClipDepth != eGL_ZERO_TO_ONE);
  for(int i = 0; i < 8; i++)
    pipe.vertexProcessing.clipPlanes[i] = rs.Enabled[GLRenderState::eEnabled_ClipDistance0 + i];

  // Shader stages & Textures

  GLint numTexUnits = 8;
  drv.glGetIntegerv(eGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numTexUnits);

  GLenum activeTexture = eGL_TEXTURE0;
  drv.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&activeTexture);

  pipe.vertexShader.stage = ShaderStage::Vertex;
  pipe.tessControlShader.stage = ShaderStage::Tess_Control;
  pipe.tessEvalShader.stage = ShaderStage::Tess_Eval;
  pipe.geometryShader.stage = ShaderStage::Geometry;
  pipe.fragmentShader.stage = ShaderStage::Fragment;
  pipe.computeShader.stage = ShaderStage::Compute;

  GLPipe::Shader *stages[NumShaderStages] = {
      &pipe.vertexShader,   &pipe.tessControlShader, &pipe.tessEvalShader,
      &pipe.geometryShader, &pipe.fragmentShader,    &pipe.computeShader,
  };
  ShaderReflection *refls[NumShaderStages] = {NULL};
  ResourceId progIds[NumShaderStages];
  ResourceId shadIds[NumShaderStages];
  GLuint progForStage[NumShaderStages] = {};
  bool spirv[NumShaderStages] = {false};

  for(size_t i = 0; i < NumShaderStages; i++)
  {
    if(!stages[i])
      continue;

    stages[i]->programResourceId = stages[i]->shaderResourceId = ResourceId();
    stages[i]->reflection = NULL;
  }

  rdcarray<int32_t> vertexAttrBindings;

  {
    GLuint curProg = 0;
    drv.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&curProg);

    if(curProg == 0)
    {
      GLuint curPipe = 0;
      drv.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&curPipe);

      if(curPipe != 0)
      {
        ResourceId id = rm->GetResID(ProgramPipeRes(ctx, curPipe));
        const WrappedOpenGL::PipelineData &pipeDetails = m_pDriver->m_Pipelines[id];

        pipe.pipelineResourceId = rm->GetUnreplacedOriginalID(id);

        for(size_t i = 0; i < ARRAY_COUNT(pipeDetails.stageShaders); i++)
        {
          if(!stages[i])
            continue;

          if(pipeDetails.stageShaders[i] != ResourceId())
          {
            progIds[i] = pipeDetails.stagePrograms[i];
            shadIds[i] = pipeDetails.stageShaders[i];

            progForStage[i] = rm->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
          }
        }
      }
    }
    else
    {
      ResourceId id = rm->GetResID(ProgramRes(ctx, curProg));
      const WrappedOpenGL::ProgramData &progDetails = m_pDriver->m_Programs[id];

      pipe.pipelineResourceId = ResourceId();

      for(size_t i = 0; i < ARRAY_COUNT(progDetails.stageShaders); i++)
      {
        if(!stages[i])
          continue;

        if(progDetails.stageShaders[i] != ResourceId())
        {
          progIds[i] = id;
          shadIds[i] = progDetails.stageShaders[i];

          progForStage[i] = curProg;
        }
      }
    }
  }

  for(size_t i = 0; i < NumShaderStages; i++)
  {
    if(progForStage[i])
    {
      progForStage[i] = rm->GetCurrentResource(progIds[i]).name;
      stages[i]->programResourceId = rm->GetUnreplacedOriginalID(progIds[i]);
      stages[i]->shaderResourceId = rm->GetUnreplacedOriginalID(shadIds[i]);

      const WrappedOpenGL::ShaderData &shaderDetails = m_pDriver->m_Shaders[shadIds[i]];

      if(shaderDetails.reflection->resourceId == ResourceId())
        stages[i]->reflection = refls[i] = NULL;
      else
        stages[i]->reflection = refls[i] = shaderDetails.reflection;

      if(!shaderDetails.spirvWords.empty())
        spirv[i] = true;

      if(i == 0)
        EvaluateVertexAttributeBinds(progForStage[i], refls[i], spirv[i], vertexAttrBindings);
    }
    else if(stages[i])
    {
      stages[i]->programResourceId = stages[i]->shaderResourceId = ResourceId();
      stages[i]->reflection = NULL;
    }
  }

  for(size_t i = 0; i < pipe.vertexInput.attributes.size(); i++)
  {
    if(i < vertexAttrBindings.size())
      pipe.vertexInput.attributes[i].boundShaderInput = vertexAttrBindings[i];
    else
      pipe.vertexInput.attributes[i].boundShaderInput = -1;
  }

  pipe.textureCompleteness.clear();

  for(size_t s = 0; s < NumShaderStages; s++)
  {
    ShaderReflection *refl = refls[s];

    if(!refl)
      continue;

    GLuint prog = progForStage[s];

    DescriptorAccess access;
    access.descriptorStore = m_pDriver->m_DescriptorsID;
    access.stage = refl->stage;
    access.byteSize = 1;

    m_Access.reserve(m_Access.size() + refl->constantBlocks.size() +
                     refl->readOnlyResources.size() + refl->readWriteResources.size());

    RDCASSERT(refl->constantBlocks.size() < 0xffff, refl->constantBlocks.size());
    for(uint16_t i = 0; i < refl->constantBlocks.size(); i++)
    {
      uint32_t slot = 0;
      bool used = false;
      GetCurrentBinding(prog, refl, refl->constantBlocks[i], slot, used);

      access.staticallyUnused = !used;
      access.type = DescriptorType::ConstantBuffer;
      access.index = i;
      if(!refl->constantBlocks[i].bufferBacked)
        access.byteOffset = EncodeGLDescriptorIndex({GLDescriptorMapping::BareUniforms, (uint32_t)s});
      else
        access.byteOffset =
            EncodeGLDescriptorIndex({GLDescriptorMapping::UniformBinding, (uint32_t)slot});
      m_Access.push_back(access);
    }

    RDCASSERT(refl->readOnlyResources.size() < 0xffff, refl->readOnlyResources.size());
    for(uint16_t i = 0; i < refl->readOnlyResources.size(); i++)
    {
      uint32_t slot = 0;
      bool used = false;
      GetCurrentBinding(prog, refl, refl->readOnlyResources[i], slot, used);

      access.staticallyUnused = !used;

      GLDescriptorMapping descType = GLDescriptorMapping::Tex2D;

      switch(refl->readOnlyResources[i].textureType)
      {
        case TextureType::Buffer: descType = GLDescriptorMapping::TexBuffer; break;
        case TextureType::Texture1D: descType = GLDescriptorMapping::Tex1D; break;
        case TextureType::Texture1DArray: descType = GLDescriptorMapping::Tex1DArray; break;
        case TextureType::Texture2D: descType = GLDescriptorMapping::Tex2D; break;
        case TextureType::TextureRect: descType = GLDescriptorMapping::TexRect; break;
        case TextureType::Texture2DArray: descType = GLDescriptorMapping::Tex2DArray; break;
        case TextureType::Texture2DMS: descType = GLDescriptorMapping::Tex2DMS; break;
        case TextureType::Texture2DMSArray: descType = GLDescriptorMapping::Tex2DMSArray; break;
        case TextureType::Texture3D: descType = GLDescriptorMapping::Tex3D; break;
        case TextureType::TextureCube: descType = GLDescriptorMapping::TexCube; break;
        case TextureType::TextureCubeArray: descType = GLDescriptorMapping::TexCubeArray; break;
        case TextureType::Unknown:
        case TextureType::Count:
          RDCERR("Invalid resource type on binding %s", refl->readOnlyResources[i].name.c_str());
          break;
      }

      access.type = DescriptorType::ImageSampler;
      if(descType == GLDescriptorMapping::TexBuffer)
        access.type = DescriptorType::TypedBuffer;
      access.index = i;
      access.byteOffset = EncodeGLDescriptorIndex({descType, slot});
      m_Access.push_back(access);

      // checking texture completeness is a pretty expensive operation since it requires a lot of
      // queries against the driver's texture properties.
      // We assume that if a texture and sampler are complete at any point, even if their
      // properties change mid-frame they will stay complete. Similarly if they are _incomplete_
      // they will stay incomplete. Thus we can cache the results for a given pair, which if
      // samplers don't change (or are only ever used consistently with the same texture) amounts
      // to one entry per texture.
      // Note that textures can't change target, so we don't need to icnlude the target in the key
      drv.glActiveTexture(GLenum(eGL_TEXTURE0 + slot));

      GLenum binding = eGL_NONE;
      switch(refl->readOnlyResources[i].textureType)
      {
        case TextureType::Unknown: binding = eGL_NONE; break;
        case TextureType::Buffer: binding = eGL_TEXTURE_BINDING_BUFFER; break;
        case TextureType::Texture1D: binding = eGL_TEXTURE_BINDING_1D; break;
        case TextureType::Texture1DArray: binding = eGL_TEXTURE_BINDING_1D_ARRAY; break;
        case TextureType::Texture2D: binding = eGL_TEXTURE_BINDING_2D; break;
        case TextureType::TextureRect: binding = eGL_TEXTURE_BINDING_RECTANGLE; break;
        case TextureType::Texture2DArray: binding = eGL_TEXTURE_BINDING_2D_ARRAY; break;
        case TextureType::Texture2DMS: binding = eGL_TEXTURE_BINDING_2D_MULTISAMPLE; break;
        case TextureType::Texture2DMSArray:
          binding = eGL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
          break;
        case TextureType::Texture3D: binding = eGL_TEXTURE_BINDING_3D; break;
        case TextureType::TextureCube: binding = eGL_TEXTURE_BINDING_CUBE_MAP; break;
        case TextureType::TextureCubeArray: binding = eGL_TEXTURE_BINDING_CUBE_MAP_ARRAY; break;
        case TextureType::Count: RDCERR("Invalid shader resource type"); break;
      }

      GLuint tex = 0;

      if(descType == GLDescriptorMapping::TexCubeArray && !HasExt[ARB_texture_cube_map_array])
        tex = 0;
      else
        drv.glGetIntegerv(binding, (GLint *)&tex);

      GLuint samp = 0;
      if(HasExt[ARB_sampler_objects])
        drv.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&samp);

      CompleteCacheKey complete = {tex, samp};

      auto it = m_CompleteCache.find(complete);
      if(it == m_CompleteCache.end())
        it = m_CompleteCache.insert(
            it,
            std::make_pair(complete, GetTextureCompleteStatus(TextureTarget(binding), tex, samp)));
      if(!it->second.empty())
      {
        GLPipe::TextureCompleteness completeness;
        completeness.descriptorByteOffset = access.byteOffset;
        completeness.completeStatus = it->second;
        pipe.textureCompleteness.push_back(completeness);
      }
    }

    RDCASSERT(refl->readWriteResources.size() < 0xffff, refl->readWriteResources.size());
    for(uint16_t i = 0; i < refl->readWriteResources.size(); i++)
    {
      uint32_t slot = 0;
      bool used = false;
      GetCurrentBinding(prog, refl, refl->readWriteResources[i], slot, used);

      access.staticallyUnused = !used;

      GLDescriptorMapping descType = GLDescriptorMapping::Images;

      if(refl->readWriteResources[i].isTexture)
      {
        access.type = DescriptorType::ReadWriteImage;
        if(refl->readWriteResources[i].textureType == TextureType::Buffer)
          access.type = DescriptorType::ReadWriteTypedBuffer;
      }
      else
      {
        access.type = DescriptorType::ReadWriteBuffer;
        descType = GLDescriptorMapping::ShaderStorage;

        if(refl->readWriteResources[i].variableType.rows == 1 &&
           refl->readWriteResources[i].variableType.columns == 1 &&
           refl->readWriteResources[i].variableType.baseType == VarType::UInt)
        {
          descType = GLDescriptorMapping::AtomicCounter;
        }
      }

      access.index = i;
      access.byteOffset = EncodeGLDescriptorIndex({descType, slot});
      m_Access.push_back(access);
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
  // then check if two uniforms with different types point to the same binding
  for(uint32_t unit = 0; unit < (uint32_t)numTexUnits; unit++)
  {
    rdcstr typeConflict;
    GLenum binding = eGL_NONE;
    GLenum target = eGL_NONE;
    TextureType resType = TextureType::Unknown;
    rdcstr firstBindName;

    rdcarray<uint32_t> descriptorsReferenced;

    for(const DescriptorAccess &access : m_Access)
    {
      // only look at read-only descriptors, these are the texture units that can clash
      if(!IsReadOnlyDescriptor(access.type))
        continue;

      ShaderReflection *refl = refls[(uint32_t)access.stage];
      if(refl == NULL)
      {
        RDCERR("Unexpected NULL reflection on %s shader with a descriptor access",
               ToStr(access.stage).c_str());
        continue;
      }

      uint32_t accessedUnit = DecodeGLDescriptorIndex(access.byteOffset).idx;

      // accessed the same unit, check its binding
      if(accessedUnit == unit)
      {
        if(!descriptorsReferenced.contains(access.byteOffset))
          descriptorsReferenced.push_back(access.byteOffset);

        const ShaderResource &res = refl->readOnlyResources[access.index];
        GLenum t = eGL_NONE;

        switch(res.textureType)
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

        if(binding == eGL_NONE)
        {
          binding = t;
          firstBindName = res.name;
          resType = res.textureType;
        }
        else if(binding == t)
        {
          // two uniforms with the same type pointing to the same slot is fine
          binding = t;
        }
        else if(binding != t)
        {
          RDCERR("Two uniforms pointing to texture unit %d with types %s and %s", unit,
                 ToStr(binding).c_str(), ToStr(t).c_str());

          if(typeConflict.empty())
          {
            typeConflict = StringFormat::Fmt("First binding found '%s' is %s",
                                             firstBindName.c_str(), ToStr(resType).c_str());
          }

          typeConflict +=
              StringFormat::Fmt(", '%s' is %s", res.name.c_str(), ToStr(res.textureType).c_str());
        }
      }
    }

    // if we found a type conflict, add an entry for all descriptors
    if(!typeConflict.empty())
    {
      for(uint32_t descriptor : descriptorsReferenced)
      {
        bool found = false;
        for(GLPipe::TextureCompleteness &completeness : pipe.textureCompleteness)
        {
          if(completeness.descriptorByteOffset == descriptor)
          {
            // don't worry about overwriting, the descriptor byte offset is unique to the unit so
            // we should only set this at most once
            completeness.typeConflict = typeConflict;
            found = true;
          }
        }

        if(!found)
        {
          GLPipe::TextureCompleteness completeness;
          completeness.descriptorByteOffset = descriptor;
          completeness.typeConflict = typeConflict;
          pipe.textureCompleteness.push_back(completeness);
        }
      }
    }
  }

  RDCEraseEl(pipe.transformFeedback);

  if(HasExt[ARB_transform_feedback2])
  {
    GLuint feedback = 0;
    drv.glGetIntegerv(eGL_TRANSFORM_FEEDBACK_BINDING, (GLint *)&feedback);

    if(feedback != 0)
      pipe.transformFeedback.feedbackResourceId =
          rm->GetOriginalID(rm->GetResID(FeedbackRes(ctx, feedback)));
    else
      pipe.transformFeedback.feedbackResourceId = ResourceId();

    GLint maxCount = 0;
    drv.glGetIntegerv(eGL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxCount);

    for(int i = 0; i < (int)ARRAY_COUNT(pipe.transformFeedback.bufferResourceId) && i < maxCount; i++)
    {
      GLuint buffer = 0;
      drv.glGetIntegeri_v(eGL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i, (GLint *)&buffer);
      pipe.transformFeedback.bufferResourceId[i] =
          rm->GetOriginalID(rm->GetResID(BufferRes(ctx, buffer)));
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

  for(size_t i = 0; i < ARRAY_COUNT(rs.Subroutines); i++)
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

  drv.glActiveTexture(activeTexture);

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
      DELIBERATE_FALLTHROUGH();
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
      default: RDCWARN("Unexpected value for CULL_FACE %x", rs.CullFace); DELIBERATE_FALLTHROUGH();
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
        rm->GetOriginalID(rm->GetResID(FramebufferRes(ctx, curDrawFBO)));
    pipe.framebuffer.drawFBO.colorAttachments.resize(numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      ResourceId id =
          rm->GetResID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i]));

      pipe.framebuffer.drawFBO.colorAttachments[i].resource = rm->GetOriginalID(id);

      GLenum attachment = GLenum(eGL_COLOR_ATTACHMENT0 + i);

      if(pipe.framebuffer.drawFBO.colorAttachments[i].resource != ResourceId() && !rbCol[i])
        GetFramebufferMipAndLayer(curDrawFBO, attachment,
                                  (GLint *)&pipe.framebuffer.drawFBO.colorAttachments[i].firstMip,
                                  (GLint *)&pipe.framebuffer.drawFBO.colorAttachments[i].firstSlice);

      pipe.framebuffer.drawFBO.colorAttachments[i].numSlices = 1;

      if(!rbCol[i] && id != ResourceId())
      {
        // desktop GL allows layered attachments which attach all slices from 0 to N
        if(!IsGLES)
        {
          GLint layered = 0;
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              curDrawFBO, attachment, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

          if(layered)
          {
            pipe.framebuffer.drawFBO.colorAttachments[i].numSlices =
                m_pDriver->m_Textures[id].depth & 0xffff;
          }
        }
        else
        {
          // on GLES there's an OVR extension that allows attaching multiple layers
          if(HasExt[OVR_multiview])
          {
            GLint numViews = 0, startView = 0;
            GL.glGetNamedFramebufferAttachmentParameterivEXT(
                curDrawFBO, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR, &numViews);
            GL.glGetNamedFramebufferAttachmentParameterivEXT(
                curDrawFBO, attachment, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR,
                &startView);

            if(numViews > 1)
            {
              pipe.framebuffer.drawFBO.colorAttachments[i].numSlices = numViews & 0xffff;
              pipe.framebuffer.drawFBO.colorAttachments[i].firstSlice = startView & 0xffff;
            }
          }
        }
      }

      GLenum swizzles[4] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};
      if(!rbCol[i] && id != ResourceId() &&
         (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
      {
        GLenum target = m_pDriver->m_Textures[id].curType;
        GetTextureSwizzle(curCol[i], target, swizzles);
      }

      pipe.framebuffer.drawFBO.colorAttachments[i].swizzle.red = MakeSwizzle(swizzles[0]);
      pipe.framebuffer.drawFBO.colorAttachments[i].swizzle.green = MakeSwizzle(swizzles[1]);
      pipe.framebuffer.drawFBO.colorAttachments[i].swizzle.blue = MakeSwizzle(swizzles[2]);
      pipe.framebuffer.drawFBO.colorAttachments[i].swizzle.alpha = MakeSwizzle(swizzles[3]);
    }

    ResourceId id =
        rm->GetResID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth));
    pipe.framebuffer.drawFBO.depthAttachment.resource = rm->GetOriginalID(id);
    pipe.framebuffer.drawFBO.stencilAttachment.resource = rm->GetOriginalID(
        rm->GetResID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

    if(pipe.framebuffer.drawFBO.depthAttachment.resource != ResourceId() && !rbDepth)
      GetFramebufferMipAndLayer(curDrawFBO, eGL_DEPTH_ATTACHMENT,
                                &pipe.framebuffer.drawFBO.depthAttachment.firstMip,
                                &pipe.framebuffer.drawFBO.depthAttachment.firstSlice);

    if(pipe.framebuffer.drawFBO.stencilAttachment.resource != ResourceId() && !rbStencil)
      GetFramebufferMipAndLayer(curDrawFBO, eGL_STENCIL_ATTACHMENT,
                                &pipe.framebuffer.drawFBO.stencilAttachment.firstMip,
                                &pipe.framebuffer.drawFBO.stencilAttachment.firstSlice);

    pipe.framebuffer.drawFBO.depthAttachment.numSlices = 1;
    pipe.framebuffer.drawFBO.stencilAttachment.numSlices = 1;

    if(!rbDepth && id != ResourceId())
    {
      // desktop GL allows layered attachments which attach all slices from 0 to N
      if(!IsGLES)
      {
        GLint layered = 0;
        GL.glGetNamedFramebufferAttachmentParameterivEXT(
            curDrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

        if(layered)
        {
          pipe.framebuffer.drawFBO.depthAttachment.numSlices =
              m_pDriver->m_Textures[id].depth & 0xffff;
        }
      }
      else
      {
        // on GLES there's an OVR extension that allows attaching multiple layers
        if(HasExt[OVR_multiview])
        {
          GLint numViews = 0, startView = 0;
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              curDrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR,
              &numViews);
          GL.glGetNamedFramebufferAttachmentParameterivEXT(
              curDrawFBO, eGL_DEPTH_ATTACHMENT,
              eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR, &startView);

          if(numViews > 1)
          {
            pipe.framebuffer.drawFBO.depthAttachment.numSlices = numViews & 0xffff;
            pipe.framebuffer.drawFBO.depthAttachment.firstSlice = startView & 0xffff;
          }
        }
      }

      if(pipe.framebuffer.drawFBO.stencilAttachment.resource ==
         pipe.framebuffer.drawFBO.depthAttachment.resource)
      {
        pipe.framebuffer.drawFBO.stencilAttachment.firstSlice =
            pipe.framebuffer.drawFBO.depthAttachment.firstSlice;
        pipe.framebuffer.drawFBO.stencilAttachment.numSlices =
            pipe.framebuffer.drawFBO.depthAttachment.numSlices;
      }
    }

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
        rm->GetOriginalID(rm->GetResID(FramebufferRes(ctx, curReadFBO)));
    pipe.framebuffer.readFBO.colorAttachments.resize(numCols);
    for(GLint i = 0; i < numCols; i++)
    {
      pipe.framebuffer.readFBO.colorAttachments[i].resource = rm->GetOriginalID(
          rm->GetResID(rbCol[i] ? RenderbufferRes(ctx, curCol[i]) : TextureRes(ctx, curCol[i])));

      if(pipe.framebuffer.readFBO.colorAttachments[i].resource != ResourceId() && !rbCol[i])
        GetFramebufferMipAndLayer(curReadFBO, GLenum(eGL_COLOR_ATTACHMENT0 + i),
                                  &pipe.framebuffer.readFBO.colorAttachments[i].firstMip,
                                  &pipe.framebuffer.readFBO.colorAttachments[i].firstSlice);
    }

    pipe.framebuffer.readFBO.depthAttachment.resource = rm->GetOriginalID(
        rm->GetResID(rbDepth ? RenderbufferRes(ctx, curDepth) : TextureRes(ctx, curDepth)));
    pipe.framebuffer.readFBO.stencilAttachment.resource = rm->GetOriginalID(
        rm->GetResID(rbStencil ? RenderbufferRes(ctx, curStencil) : TextureRes(ctx, curStencil)));

    if(pipe.framebuffer.readFBO.depthAttachment.resource != ResourceId() && !rbDepth)
      GetFramebufferMipAndLayer(curReadFBO, eGL_DEPTH_ATTACHMENT,
                                &pipe.framebuffer.readFBO.depthAttachment.firstMip,
                                &pipe.framebuffer.readFBO.depthAttachment.firstSlice);

    if(pipe.framebuffer.readFBO.stencilAttachment.resource != ResourceId() && !rbStencil)
      GetFramebufferMipAndLayer(curReadFBO, eGL_STENCIL_ATTACHMENT,
                                &pipe.framebuffer.readFBO.stencilAttachment.firstMip,
                                &pipe.framebuffer.readFBO.stencilAttachment.firstSlice);

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

  pipe.framebuffer.blendState.blendFactor = rs.BlendColor;

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

rdcarray<Descriptor> GLReplay::GetDescriptors(ResourceId descriptorStore,
                                              const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<Descriptor> ret;

  if(descriptorStore != m_pDriver->m_DescriptorsID)
  {
    RDCERR("Descriptors query for invalid descriptor store on fixed bindings API (OpenGL)");
    return ret;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);
  WrappedOpenGL &drv = *m_pDriver;
  GLResourceManager *rm = m_pDriver->GetResourceManager();

  ContextPair &ctx = drv.GetCtx();

  GLRenderState rs;
  rs.FetchState(&drv);

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorId = r.offset;

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorId++)
    {
      GLDescriptorLocation idx = DecodeGLDescriptorIndex(descriptorId);

      if(idx.type == GLDescriptorMapping::Invalid)
      {
      }
      else if(idx.type == GLDescriptorMapping::BareUniforms)
      {
        // not feasible to emulate backing store for bare uniforms. We could synthesise our own byte
        // layout and query uniform values into a buffer but... let's not.
        ret[dst].type = DescriptorType::ConstantBuffer;
        ret[dst].byteSize = 16 * 1024;
      }
      else if(idx.type == GLDescriptorMapping::UniformBinding)
      {
        ret[dst].type = DescriptorType::ConstantBuffer;
        if(rs.UniformBinding[idx.idx].res.name != 0)
        {
          ResourceId id = rm->GetResID(rs.UniformBinding[idx.idx].res);
          ret[dst].resource = rm->GetOriginalID(id);
          ret[dst].byteOffset = rs.UniformBinding[idx.idx].start;
          ret[dst].byteSize = rs.UniformBinding[idx.idx].size;

          if(ret[dst].byteSize == 0)
            ret[dst].byteSize = m_pDriver->m_Buffers[id].size;
        }
      }
      else if(idx.type == GLDescriptorMapping::AtomicCounter)
      {
        ret[dst].type = DescriptorType::ReadWriteBuffer;
        if(rs.AtomicCounter[idx.idx].res.name != 0)
        {
          ResourceId id = rm->GetResID(rs.AtomicCounter[idx.idx].res);
          ret[dst].resource = rm->GetOriginalID(id);
          ret[dst].byteOffset = rs.AtomicCounter[idx.idx].start;
          ret[dst].byteSize = rs.AtomicCounter[idx.idx].size;

          if(ret[dst].byteSize == 0)
            ret[dst].byteSize = m_pDriver->m_Buffers[id].size;
        }
      }
      else if(idx.type == GLDescriptorMapping::ShaderStorage)
      {
        ret[dst].type = DescriptorType::ReadWriteBuffer;
        if(rs.ShaderStorage[idx.idx].res.name != 0)
        {
          ResourceId id = rm->GetResID(rs.ShaderStorage[idx.idx].res);
          ret[dst].resource = rm->GetOriginalID(id);
          ret[dst].byteOffset = rs.ShaderStorage[idx.idx].start;
          ret[dst].byteSize = rs.ShaderStorage[idx.idx].size;

          if(ret[dst].byteSize == 0)
            ret[dst].byteSize = m_pDriver->m_Buffers[id].size;
        }
      }
      else if(idx.type == GLDescriptorMapping::Images)
      {
        ret[dst].type = DescriptorType::ReadWriteImage;
        if(rs.Images[idx.idx].res.name != 0)
        {
          ResourceId id = rm->GetResID(rs.Images[idx.idx].res);
          ret[dst].resource = rm->GetOriginalID(id);
          ret[dst].firstMip = rs.Images[idx.idx].level & 0xff;
          ret[dst].numMips = 1;
          ret[dst].firstSlice = rs.Images[idx.idx].layer & 0xffff;
          if(rs.Images[idx.idx].access == eGL_READ_ONLY)
          {
            ret[dst].flags = DescriptorFlags::ReadOnlyAccess;
          }
          else if(rs.Images[idx.idx].access == eGL_WRITE_ONLY)
          {
            ret[dst].flags = DescriptorFlags::WriteOnlyAccess;
          }
          ret[dst].format = MakeResourceFormat(eGL_TEXTURE_2D, rs.Images[idx.idx].format);

          CacheTexture(id);

          ret[dst].textureType = m_CachedTextures[id].type;
          if(ret[dst].textureType == TextureType::Texture3D)
            ret[dst].numSlices = rs.Images[idx.idx].layered
                                     ? (m_CachedTextures[id].depth >> ret[dst].firstMip) & 0xffff
                                     : 1;
          else
            ret[dst].numSlices =
                rs.Images[idx.idx].layered ? m_CachedTextures[id].arraysize & 0xffff : 1;

          if(ret[dst].textureType == TextureType::Buffer)
            ret[dst].type = DescriptorType::ReadWriteTypedBuffer;
        }
      }
      else
      {
        ret[dst].type = DescriptorType::ImageSampler;
        if(idx.type == GLDescriptorMapping::TexBuffer)
          ret[dst].type = DescriptorType::TypedBuffer;

        drv.glActiveTexture(GLenum(eGL_TEXTURE0 + idx.idx));

        GLuint tex = 0;
        GLenum target = eGL_NONE;

        if(idx.type == GLDescriptorMapping::TexCubeArray && !HasExt[ARB_texture_cube_map_array])
        {
          tex = 0;
          target = eGL_TEXTURE_CUBE_MAP_ARRAY;
        }
        else
        {
          switch(idx.type)
          {
            case GLDescriptorMapping::Tex1D:
              target = eGL_TEXTURE_1D;
              ret[dst].textureType = TextureType::Texture1D;
              break;
            case GLDescriptorMapping::Tex2D:
              target = eGL_TEXTURE_2D;
              ret[dst].textureType = TextureType::Texture2D;
              break;
            case GLDescriptorMapping::Tex3D:
              target = eGL_TEXTURE_3D;
              ret[dst].textureType = TextureType::Texture3D;
              break;
            case GLDescriptorMapping::Tex1DArray:
              target = eGL_TEXTURE_1D_ARRAY;
              ret[dst].textureType = TextureType::Texture1DArray;
              break;
            case GLDescriptorMapping::Tex2DArray:
              target = eGL_TEXTURE_2D_ARRAY;
              ret[dst].textureType = TextureType::Texture2DArray;
              break;
            case GLDescriptorMapping::TexCubeArray:
              target = eGL_TEXTURE_CUBE_MAP_ARRAY;
              ret[dst].textureType = TextureType::TextureCubeArray;
              break;
            case GLDescriptorMapping::TexRect:
              target = eGL_TEXTURE_RECTANGLE;
              ret[dst].textureType = TextureType::TextureRect;
              break;
            case GLDescriptorMapping::TexBuffer:
              target = eGL_TEXTURE_BUFFER;
              ret[dst].textureType = TextureType::Buffer;
              break;
            case GLDescriptorMapping::TexCube:
              target = eGL_TEXTURE_CUBE_MAP;
              ret[dst].textureType = TextureType::TextureCube;
              break;
            case GLDescriptorMapping::Tex2DMS:
              target = eGL_TEXTURE_2D_MULTISAMPLE;
              ret[dst].textureType = TextureType::Texture2DMS;
              break;
            case GLDescriptorMapping::Tex2DMSArray:
              target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY;
              ret[dst].textureType = TextureType::Texture2DMSArray;
              break;
            case GLDescriptorMapping::AtomicCounter:
            case GLDescriptorMapping::ShaderStorage:
            case GLDescriptorMapping::BareUniforms:
            case GLDescriptorMapping::UniformBinding:
            case GLDescriptorMapping::Images:
            case GLDescriptorMapping::Invalid:
            case GLDescriptorMapping::Count: target = eGL_NONE; break;
          }
        }

        if(target == eGL_NONE)
          continue;

        GLenum binding = TextureBinding(target);

        drv.glGetIntegerv(binding, (GLint *)&tex);

        if(tex == 0)
          continue;

        GLint firstMip = 0, numMips = 1;

        if(target != eGL_TEXTURE_BUFFER)
        {
          drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_BASE_LEVEL, &firstMip);
          drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MAX_LEVEL, &numMips);

          numMips = numMips - firstMip + 1;
        }

        ResourceId id = rm->GetResID(TextureRes(ctx, tex));
        ret[dst].resource = rm->GetOriginalID(id);
        ret[dst].firstMip = firstMip & 0xff;
        ret[dst].numMips = numMips & 0xff;

        GLenum levelQueryType =
            target == eGL_TEXTURE_CUBE_MAP ? eGL_TEXTURE_CUBE_MAP_POSITIVE_X : target;
        GLenum fmt = eGL_NONE;
        drv.glGetTexLevelParameteriv(levelQueryType, 0, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
        ret[dst].format = MakeResourceFormat(target, fmt);
        if(IsDepthStencilFormat(fmt))
        {
          GLint depthMode = eGL_DEPTH_COMPONENT;

          if(HasExt[ARB_stencil_texturing])
            drv.glGetTextureParameterivEXT(tex, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &depthMode);

          ret[dst].format = ResourceFormat();
          ret[dst].format.type = ResourceFormatType::Regular;
          ret[dst].format.compByteWidth = 1;
          ret[dst].format.compCount = 1;
          if(depthMode == eGL_DEPTH_COMPONENT)
            ret[dst].format.compType = CompType::Depth;
          else if(depthMode == eGL_STENCIL_INDEX)
            ret[dst].format.compType = CompType::UInt;
        }

        GLenum swizzles[4] = {eGL_RED, eGL_GREEN, eGL_BLUE, eGL_ALPHA};
        if(target != eGL_TEXTURE_BUFFER &&
           (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
          GetTextureSwizzle(tex, target, swizzles);

        ret[dst].swizzle.red = MakeSwizzle(swizzles[0]);
        ret[dst].swizzle.green = MakeSwizzle(swizzles[1]);
        ret[dst].swizzle.blue = MakeSwizzle(swizzles[2]);
        ret[dst].swizzle.alpha = MakeSwizzle(swizzles[3]);

        GLuint samp = 0;
        if(HasExt[ARB_sampler_objects])
          drv.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&samp);

        ret[dst].secondary = rm->GetOriginalID(rm->GetResID(SamplerRes(ctx, samp)));
      }
    }
  }

  return ret;
}

rdcarray<SamplerDescriptor> GLReplay::GetSamplerDescriptors(ResourceId descriptorStore,
                                                            const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<SamplerDescriptor> ret;

  if(descriptorStore != m_pDriver->m_DescriptorsID)
  {
    RDCERR("Descriptors query for invalid descriptor store on fixed bindings API (OpenGL)");
    return ret;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);
  WrappedOpenGL &drv = *m_pDriver;
  GLResourceManager *rm = m_pDriver->GetResourceManager();

  ContextPair &ctx = drv.GetCtx();

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorId = r.offset;

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorId++)
    {
      GLDescriptorLocation idx = DecodeGLDescriptorIndex(descriptorId);

      if(idx.type == GLDescriptorMapping::Invalid || idx.type == GLDescriptorMapping::Images ||
         idx.type == GLDescriptorMapping::AtomicCounter ||
         idx.type == GLDescriptorMapping::ShaderStorage ||
         idx.type == GLDescriptorMapping::UniformBinding)
        continue;

      drv.glActiveTexture(GLenum(eGL_TEXTURE0 + idx.idx));

      GLuint tex = 0;
      GLenum target = eGL_NONE;

      if(idx.type == GLDescriptorMapping::TexCubeArray && !HasExt[ARB_texture_cube_map_array])
      {
        tex = 0;
        target = eGL_TEXTURE_CUBE_MAP_ARRAY;
      }
      else
      {
        switch(idx.type)
        {
          case GLDescriptorMapping::Tex1D: target = eGL_TEXTURE_1D; break;
          case GLDescriptorMapping::Tex2D: target = eGL_TEXTURE_2D; break;
          case GLDescriptorMapping::Tex3D: target = eGL_TEXTURE_3D; break;
          case GLDescriptorMapping::Tex1DArray: target = eGL_TEXTURE_1D_ARRAY; break;
          case GLDescriptorMapping::Tex2DArray: target = eGL_TEXTURE_2D_ARRAY; break;
          case GLDescriptorMapping::TexCubeArray: target = eGL_TEXTURE_CUBE_MAP_ARRAY; break;
          case GLDescriptorMapping::TexRect: target = eGL_TEXTURE_RECTANGLE; break;
          case GLDescriptorMapping::TexBuffer: target = eGL_TEXTURE_BUFFER; break;
          case GLDescriptorMapping::TexCube: target = eGL_TEXTURE_CUBE_MAP; break;
          case GLDescriptorMapping::Tex2DMS: target = eGL_TEXTURE_2D_MULTISAMPLE; break;
          case GLDescriptorMapping::Tex2DMSArray: target = eGL_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
          case GLDescriptorMapping::AtomicCounter:
          case GLDescriptorMapping::ShaderStorage:
          case GLDescriptorMapping::BareUniforms:
          case GLDescriptorMapping::UniformBinding:
          case GLDescriptorMapping::Images:
          case GLDescriptorMapping::Invalid:
          case GLDescriptorMapping::Count: target = eGL_NONE; break;
        }
      }

      if(target == eGL_NONE)
        continue;

      GLenum binding = TextureBinding(target);

      GLuint samp = 0;
      if(HasExt[ARB_sampler_objects])
        drv.glGetIntegerv(eGL_SAMPLER_BINDING, (GLint *)&samp);

      drv.glGetIntegerv(binding, (GLint *)&tex);

      if(samp == 0 && tex == 0)
        continue;

      ret[dst].object = rm->GetOriginalID(rm->GetResID(SamplerRes(ctx, samp)));

      // GL has separate sampler objects but they don't exist as separate sampler descriptors
      ret[dst].type = DescriptorType::ImageSampler;

      if(samp != 0)
        drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_BORDER_COLOR,
                                    ret[dst].borderColorValue.floatValue.data());
      else
        drv.glGetTextureParameterfvEXT(tex, target, eGL_TEXTURE_BORDER_COLOR,
                                       ret[dst].borderColorValue.floatValue.data());

      ret[dst].borderColorType = CompType::Float;

      GLint v;
      v = 0;
      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_S, &v);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_WRAP_S, &v);
      ret[dst].addressU = MakeAddressMode((GLenum)v);

      v = 0;
      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_T, &v);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_WRAP_T, &v);
      ret[dst].addressV = MakeAddressMode((GLenum)v);

      v = 0;
      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_WRAP_R, &v);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_WRAP_R, &v);
      ret[dst].addressW = MakeAddressMode((GLenum)v);

      // GLES 3 is always seamless
      if(IsGLES && GLCoreVersion > 30)
      {
        ret[dst].seamlessCubemaps = true;
      }
      else if(!IsGLES)
      {
        // on GLES 2 this is always going to be false, GL has a toggle
        ret[dst].seamlessCubemaps = drv.glIsEnabled(eGL_TEXTURE_CUBE_MAP_SEAMLESS) != GL_FALSE;
      }

      v = 0;
      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_COMPARE_FUNC, &v);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_COMPARE_FUNC, &v);
      ret[dst].compareFunction = MakeCompareFunc((GLenum)v);

      GLint minf = 0;
      GLint magf = 0;
      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_MIN_FILTER, &minf);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MIN_FILTER, &minf);

      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_MAG_FILTER, &magf);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_MAG_FILTER, &magf);

      if(HasExt[ARB_texture_filter_anisotropic])
      {
        if(samp != 0)
          drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_MAX_ANISOTROPY, &ret[dst].maxAnisotropy);
        else
          drv.glGetTextureParameterfvEXT(tex, target, eGL_TEXTURE_MAX_ANISOTROPY,
                                         &ret[dst].maxAnisotropy);
      }
      else
      {
        ret[dst].maxAnisotropy = 0.0f;
      }

      ret[dst].filter = MakeFilter((GLenum)minf, (GLenum)magf, ret[dst].maxAnisotropy);

      v = 0;
      if(samp != 0)
        drv.glGetSamplerParameteriv(samp, eGL_TEXTURE_COMPARE_MODE, &v);
      else
        drv.glGetTextureParameterivEXT(tex, target, eGL_TEXTURE_COMPARE_MODE, &v);
      ret[dst].filter.filter = (GLenum)v == eGL_COMPARE_REF_TO_TEXTURE ? FilterFunction::Comparison
                                                                       : FilterFunction::Normal;

      if(samp != 0)
        drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_MAX_LOD, &ret[dst].maxLOD);
      else
        drv.glGetTextureParameterfvEXT(tex, target, eGL_TEXTURE_MAX_LOD, &ret[dst].maxLOD);

      if(samp != 0)
        drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_MIN_LOD, &ret[dst].minLOD);
      else
        drv.glGetTextureParameterfvEXT(tex, target, eGL_TEXTURE_MIN_LOD, &ret[dst].minLOD);

      if(!IsGLES)
      {
        if(samp != 0)
          drv.glGetSamplerParameterfv(samp, eGL_TEXTURE_LOD_BIAS, &ret[dst].mipBias);
        else
          drv.glGetTextureParameterfvEXT(tex, target, eGL_TEXTURE_LOD_BIAS, &ret[dst].mipBias);
      }
    }
  }

  return ret;
}

rdcarray<DescriptorAccess> GLReplay::GetDescriptorAccess(uint32_t eventId)
{
  return m_Access;
}

rdcarray<DescriptorLogicalLocation> GLReplay::GetDescriptorLocations(
    ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<DescriptorLogicalLocation> ret;

  if(descriptorStore != m_pDriver->m_DescriptorsID)
  {
    RDCERR("Descriptors query for invalid descriptor store on fixed bindings API (OpenGL)");
    return ret;
  }

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorByteOffset = r.offset;

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorByteOffset++)
    {
      DescriptorLogicalLocation &dstLoc = ret[dst];
      GLDescriptorLocation srcLoc = DecodeGLDescriptorIndex(descriptorByteOffset);

      const char *prefix = "Unknown";
      dstLoc.stageMask = ShaderStageMask::All;
      switch(srcLoc.type)
      {
        case GLDescriptorMapping::BareUniforms:
          prefix = "Uniforms";
          dstLoc.category = DescriptorCategory::ConstantBlock;
          break;
        case GLDescriptorMapping::UniformBinding:
          prefix = "UBO";
          dstLoc.category = DescriptorCategory::ConstantBlock;
          break;
        case GLDescriptorMapping::Tex1D:
          prefix = "Tex1D";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Tex2D:
          prefix = "Tex2D";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Tex3D:
          prefix = "Tex3D";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Tex1DArray:
          prefix = "Tex1DArray";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Tex2DArray:
          prefix = "Tex2DArray";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::TexCubeArray:
          prefix = "TexCubeArray";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::TexRect:
          prefix = "TexRect";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::TexBuffer:
          prefix = "TexBuffer";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::TexCube:
          prefix = "TexCube";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Tex2DMS:
          prefix = "Tex2DMS";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Tex2DMSArray:
          prefix = "Tex2DMSArray";
          dstLoc.category = DescriptorCategory::ReadOnlyResource;
          break;
        case GLDescriptorMapping::Images:
          prefix = "Image";
          dstLoc.category = DescriptorCategory::ReadWriteResource;
          break;
        case GLDescriptorMapping::AtomicCounter:
          prefix = "Atomic";
          dstLoc.category = DescriptorCategory::ReadWriteResource;
          break;
        case GLDescriptorMapping::ShaderStorage:
          prefix = "SSBO";
          dstLoc.category = DescriptorCategory::ReadWriteResource;
          break;
        case GLDescriptorMapping::Count:
        case GLDescriptorMapping::Invalid: dstLoc.category = DescriptorCategory::Unknown; break;
      }
      dstLoc.fixedBindNumber = srcLoc.idx;

      if(srcLoc.type == GLDescriptorMapping::BareUniforms)
        dstLoc.logicalBindName =
            StringFormat::Fmt("%s %s", prefix, ToStr(ShaderStage(srcLoc.idx)).c_str());
      else
        dstLoc.logicalBindName = StringFormat::Fmt("%s %u", prefix, srcLoc.idx);
    }
  }

  return ret;
}

void GLReplay::OpenGLFillCBufferVariables(ResourceId shader, GLuint prog, bool bufferBacked,
                                          rdcstr prefix, const rdcarray<ShaderConstant> &variables,
                                          rdcarray<ShaderVariable> &outvars,
                                          const bytebuf &bufferData)
{
  bytebuf uniformData;
  const bytebuf &data = bufferBacked ? bufferData : uniformData;

  if(!bufferBacked)
    uniformData.resize(128);

  for(int32_t i = 0; i < variables.count(); i++)
  {
    const ShaderConstantType &desc = variables[i].type;

    // remove implicit '.' for recursing through "structs" if it's actually a multi-dimensional
    // array.
    if(!prefix.empty() && prefix.back() == '.' && variables[i].name[0] == '[')
      prefix.pop_back();

    ShaderVariable var;
    var.name = variables[i].name;
    var.rows = desc.rows;
    var.columns = desc.columns;
    var.type = desc.baseType;
    var.flags = desc.flags;

    const uint32_t matStride = desc.matrixByteStride;

    if(!variables[i].type.members.empty())
    {
      if(desc.elements <= 1)
      {
        OpenGLFillCBufferVariables(shader, prog, bufferBacked, prefix + var.name.c_str() + ".",
                                   variables[i].type.members, var.members, data);
        var.type = VarType::Struct;
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
          arrEl.type = VarType::Struct;
          arrEl.flags = var.flags;

          OpenGLFillCBufferVariables(shader, prog, bufferBacked, prefix + arrEl.name.c_str() + ".",
                                     variables[i].type.members, arrEl.members, data);
        }
        var.rows = var.columns = 0;
      }
    }
    else
    {
      RDCEraseEl(var.value);

      // need to query offset and strides as there's no way to know what layout was used
      // (and if it's not an std layout it's implementation defined :( )
      rdcstr fullname = prefix + var.name;

      GLuint idx = GL.glGetProgramResourceIndex(prog, eGL_UNIFORM, fullname.c_str());

      // if this is an array of size 1 try looking for <array_variable_name>[0].<member_name>
      if((idx == GL_INVALID_INDEX) && (desc.elements == 1))
      {
        rdcstr arrayZeroName = prefix;
        if(!arrayZeroName.empty() && arrayZeroName.back() == '.')
          arrayZeroName.pop_back();
        arrayZeroName += "[0]." + var.name;

        idx = GL.glGetProgramResourceIndex(prog, eGL_UNIFORM, arrayZeroName.c_str());
      }
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
        if(desc.elements > 1)
        {
          rdcarray<ShaderVariable> elems;
          for(uint32_t a = 0; a < desc.elements; a++)
          {
            ShaderVariable el = var;

            // if this is the last part of a multidimensional array, don't include the variable name
            if(var.name[0] != '[')
              el.name = StringFormat::Fmt("%s[%u]", var.name.c_str(), a);
            else
              el.name = StringFormat::Fmt("[%u]", a);

            elems.push_back(el);
          }

          var.members = elems;
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

        if(desc.elements <= 1)
        {
          if(!bufferBacked)
          {
            switch(var.type)
            {
              case VarType::Unknown:
              case VarType::GPUPointer:
              case VarType::ConstantBlock:
              case VarType::ReadOnlyResource:
              case VarType::ReadWriteResource:
              case VarType::Sampler:
              case VarType::SLong:
              case VarType::ULong:
              case VarType::SShort:
              case VarType::UShort:
              case VarType::SByte:
              case VarType::UByte:
              case VarType::Half:
              case VarType::Struct:
              case VarType::Enum:
                RDCERR("Unexpected base variable type %s, treating as float",
                       ToStr(var.type).c_str());
                DELIBERATE_FALLTHROUGH();
              case VarType::Float:
                GL.glGetUniformfv(prog, location, (float *)uniformData.data());
                break;
              case VarType::SInt:
                GL.glGetUniformiv(prog, location, (int32_t *)uniformData.data());
                break;
              case VarType::Bool:
              case VarType::UInt:
                GL.glGetUniformuiv(prog, location, (uint32_t *)uniformData.data());
                break;
              case VarType::Double:
                GL.glGetUniformdv(prog, location, (double *)uniformData.data());
                break;
            }
          }

          StandardFillCBufferVariable(shader, desc, offset, data, var, matStride);
        }
        else
        {
          rdcarray<ShaderVariable> elems;
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
                case VarType::GPUPointer:
                case VarType::ConstantBlock:
                case VarType::ReadOnlyResource:
                case VarType::ReadWriteResource:
                case VarType::Sampler:
                case VarType::SLong:
                case VarType::ULong:
                case VarType::SShort:
                case VarType::UShort:
                case VarType::SByte:
                case VarType::UByte:
                case VarType::Half:
                case VarType::Struct:
                case VarType::Enum:
                  RDCERR("Unexpected base variable type %s, treating as float",
                         ToStr(var.type).c_str());
                  DELIBERATE_FALLTHROUGH();
                case VarType::Float:
                  GL.glGetUniformfv(prog, location + a, (float *)uniformData.data());
                  break;
                case VarType::SInt:
                  GL.glGetUniformiv(prog, location + a, (int32_t *)uniformData.data());
                  break;
                case VarType::Bool:
                case VarType::UInt:
                  GL.glGetUniformuiv(prog, location + a, (uint32_t *)uniformData.data());
                  break;
                case VarType::Double:
                  GL.glGetUniformdv(prog, location + a, (double *)uniformData.data());
                  break;
              }
            }

            StandardFillCBufferVariable(shader, desc, offset, data, el, matStride);

            if(bufferBacked)
              offset += desc.arrayByteStride;

            elems.push_back(el);
          }

          var.members = elems;
          var.rows = var.columns = 0;
        }
      }
    }

    outvars.push_back(var);
  }
}

void GLReplay::FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                    rdcstr entryPoint, uint32_t cbufSlot,
                                    rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  auto &shaderDetails = m_pDriver->m_Shaders[shader];

  if((int32_t)cbufSlot >= shaderDetails.reflection->constantBlocks.count())
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
          m_pDriver->GetResourceManager()->GetResID(ProgramPipeRes(m_pDriver->GetCtx(), curProg));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      size_t s = ShaderIdx(shaderDetails.type);

      curProg =
          m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[s]).name;
    }
  }

  const ConstantBlock &cblock = shaderDetails.reflection->constantBlocks[cbufSlot];

  if(shaderDetails.spirvWords.empty())
  {
    OpenGLFillCBufferVariables(shaderDetails.reflection->resourceId, curProg,
                               cblock.bufferBacked ? true : false, "", cblock.variables, outvars,
                               data);
  }
  else
  {
    if(cblock.compileConstants)
    {
      rdcarray<SpecConstant> specconsts;

      for(size_t i = 0; i < shaderDetails.specIDs.size(); i++)
      {
        SpecConstant spec;
        spec.specID = shaderDetails.specIDs[i];
        spec.value = shaderDetails.specValues[i];
        spec.dataSize = 4;
        specconsts.push_back(spec);
      }

      FillSpecConstantVariables(shaderDetails.reflection->resourceId, shaderDetails.patchData,
                                cblock.variables, outvars, specconsts);
    }
    else if(!cblock.bufferBacked)
    {
      OpenGLFillCBufferVariables(shaderDetails.reflection->resourceId, curProg, false, "",
                                 cblock.variables, outvars, data);
    }
    else
    {
      StandardFillCBufferVariables(shaderDetails.reflection->resourceId, cblock.variables, outvars,
                                   data);
    }
  }
}

void GLReplay::GetTextureData(ResourceId tex, const Subresource &sub,
                              const GetTextureDataParams &params, bytebuf &data)
{
  WrappedOpenGL &drv = *m_pDriver;

  if(m_pDriver->m_Textures.find(tex) == m_pDriver->m_Textures.end())
  {
    data.clear();
    RDCWARN("Requesting data for non-existant texture %s", ToStr(tex).c_str());
    return;
  }

  WrappedOpenGL::TextureData &texDetails = m_pDriver->m_Textures[tex];

  GLuint tempTex = 0;

  Subresource s = sub;

  GLenum texType = texDetails.curType;
  GLuint texname = texDetails.resource.name;

  int numMips = GetNumMips(texType, texname, texDetails.width, texDetails.height, texDetails.depth);

  s.mip = RDCMIN(uint32_t(numMips - 1), s.mip);

  GLenum intFormat = texDetails.internalFormat;
  GLsizei width = RDCMAX(1, texDetails.width >> s.mip);
  GLsizei height = RDCMAX(1, texDetails.height >> s.mip);
  GLsizei depth = RDCMAX(1, texDetails.depth >> s.mip);
  GLsizei arraysize = 1;
  GLint samples = texDetails.samples;

  if(texType == eGL_NONE)
  {
    RDCERR("Trying to get texture data for unknown ID %s!", ToStr(tex).c_str());
    return;
  }

  if(texType == eGL_TEXTURE_BUFFER)
  {
    GLuint bufName = 0;
    drv.glGetTextureLevelParameterivEXT(texname, texType, 0, eGL_TEXTURE_BUFFER_DATA_STORE_BINDING,
                                        (GLint *)&bufName);
    ResourceId id =
        m_pDriver->GetResourceManager()->GetResID(BufferRes(m_pDriver->GetCtx(), bufName));

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
    if(texType == eGL_TEXTURE_1D_ARRAY)
    {
      height = 1;
      arraysize = texDetails.height;
    }
    if(texType == eGL_TEXTURE_CUBE_MAP)
    {
      arraysize = 6;
    }
  }

  s.sample = RDCMIN(uint32_t(texDetails.samples - 1), s.sample);
  s.slice = RDCMIN(uint32_t(arraysize - 1), s.slice);

  if(params.remap != RemapTexture::NoRemap)
  {
    GLenum remapFormat = eGL_RGBA8;
    if(params.remap == RemapTexture::RGBA8)
      remapFormat = eGL_RGBA8;
    else if(params.remap == RemapTexture::RGBA16)
      remapFormat = eGL_RGBA16F;
    else if(params.remap == RemapTexture::RGBA32)
      remapFormat = eGL_RGBA32F;

    CompType typeCast = BaseRemapType(params);
    if(typeCast == CompType::Typeless && IsSRGBFormat(intFormat))
      typeCast = CompType::UNormSRGB;

    remapFormat = GetViewCastedFormat(remapFormat, typeCast);

    GLenum newtarget = (texType == eGL_TEXTURE_3D ? eGL_TEXTURE_3D : eGL_TEXTURE_2D);

    if(intFormat != remapFormat || newtarget != texType)
    {
      MakeCurrentReplayContext(m_DebugCtx);

      GLenum finalFormat = remapFormat;

      // create temporary texture of width/height in the new format to render to
      drv.glGenTextures(1, &tempTex);
      drv.glBindTexture(newtarget, tempTex);
      if(newtarget == eGL_TEXTURE_3D)
        drv.glTextureImage3DEXT(tempTex, newtarget, 0, finalFormat, width, height, depth, 0,
                                GetBaseFormat(finalFormat), GetDataType(finalFormat), NULL);
      else
        drv.glTextureImage2DEXT(tempTex, newtarget, 0, finalFormat, width, height, 0,
                                GetBaseFormat(finalFormat), GetDataType(finalFormat), NULL);
      drv.glTextureParameteriEXT(tempTex, newtarget, eGL_TEXTURE_MAX_LEVEL, 0);

      // create temp framebuffer
      GLuint fbo = 0;
      drv.glGenFramebuffers(1, &fbo);
      drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);

      drv.glTextureParameteriEXT(tempTex, newtarget, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
      drv.glTextureParameteriEXT(tempTex, newtarget, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
      drv.glTextureParameteriEXT(tempTex, newtarget, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
      drv.glTextureParameteriEXT(tempTex, newtarget, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      drv.glTextureParameteriEXT(tempTex, newtarget, eGL_TEXTURE_WRAP_R, eGL_CLAMP_TO_EDGE);
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

      TexDisplayFlags flags = eTexDisplay_None;

      if(IsUIntFormat(intFormat))
        flags = eTexDisplay_RemapUInt;
      else if(IsSIntFormat(intFormat))
        flags = eTexDisplay_RemapSInt;
      else
        flags = eTexDisplay_RemapFloat;

      for(GLsizei d = 0; d < (newtarget == eGL_TEXTURE_3D ? depth : 1); d++)
      {
        TextureDisplay texDisplay;

        texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.subresource.mip = s.mip;
        texDisplay.subresource.sample = params.resolve ? ~0U : s.sample;
        texDisplay.subresource.slice = s.slice;
        texDisplay.customShaderId = ResourceId();
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeCast = params.typeCast;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        if(newtarget == eGL_TEXTURE_3D)
        {
          drv.glFramebufferTexture3D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_3D,
                                     tempTex, 0, (GLint)d);
          texDisplay.subresource.slice = (uint32_t)d;
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

        RenderTextureInternal(texDisplay, flags);

        drv.glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
      }

      // do one more time for the stencil
      if(baseFormat == eGL_DEPTH_STENCIL || baseFormat == eGL_STENCIL_INDEX)
      {
        TextureDisplay texDisplay;

        texDisplay.green = true;
        texDisplay.red = texDisplay.blue = texDisplay.alpha = false;
        texDisplay.hdrMultiplier = -1.0f;
        texDisplay.linearDisplayAsGamma = false;
        texDisplay.overlay = DebugOverlay::NoOverlay;
        texDisplay.flipY = false;
        texDisplay.subresource.mip = s.mip;
        texDisplay.subresource.sample = params.resolve ? ~0U : s.sample;
        texDisplay.subresource.slice = s.slice;
        texDisplay.customShaderId = ResourceId();
        texDisplay.rangeMin = params.blackPoint;
        texDisplay.rangeMax = params.whitePoint;
        texDisplay.scale = 1.0f;
        texDisplay.resourceId = tex;
        texDisplay.typeCast = CompType::Typeless;
        texDisplay.rawOutput = false;
        texDisplay.xOffset = 0;
        texDisplay.yOffset = 0;

        drv.glViewport(0, 0, width, height);

        GLboolean color_mask[4];
        drv.glGetBooleanv(eGL_COLOR_WRITEMASK, color_mask);
        drv.glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
        if(baseFormat == eGL_STENCIL_INDEX)
          drv.glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE);

        flags = TexDisplayFlags(
            flags & ~(eTexDisplay_RemapFloat | eTexDisplay_RemapUInt | eTexDisplay_RemapSInt));

        RenderTextureInternal(texDisplay, flags);

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
      s.mip = 0;
      s.slice = 0;

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
    drv.glTextureParameteriEXT(tempTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    // create temp framebuffers
    GLuint fbos[2] = {0};
    drv.glGenFramebuffers(2, fbos);

    drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[0]);
    drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D, tempTex, 0);

    drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbos[1]);
    if(texType == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      drv.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, texname, 0, s.slice);
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
    s.mip = 0;
    s.slice = 0;
    s.sample = 0;
    arraysize = 1;
    samples = 1;

    drv.glDeleteFramebuffers(2, fbos);

    drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);
  }
  else if(samples > 1)
  {
    MakeCurrentReplayContext(m_DebugCtx);

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
      texType = texDetails.samples > 1 ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;

      MakeCurrentReplayContext(m_DebugCtx);
    }

    // copy multisampled texture to an array. This creates tempTex and returns it in that variable,
    // for us to own
    tempTex = 0;
    m_pDriver->CopyTex2DMSToArray(tempTex, texname, width, height, arraysize, samples, intFormat);

    // CopyTex2DMSToArray is unwrapped, so register the resource here now
    m_pDriver->GetResourceManager()->RegisterResource(TextureRes(m_pDriver->GetCtx(), tempTex));

    // rewrite the variables to temporary texture
    texType = eGL_TEXTURE_2D_ARRAY;
    texname = tempTex;
    depth = 1;
    arraysize = arraysize * samples;
    // remap from slice & sample to just slice, given that each slice is expanded to N slices - one
    // for each sample.
    s.slice = s.slice * samples + s.sample;
    s.sample = 0;
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
      texType = texDetails.samples > 1 ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;

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

      RDCASSERT(s.slice < ARRAY_COUNT(targets));
      target = targets[s.slice];

      // we've "used" the slice, it's not actually a real slice anymore...
      s.slice = 0;
      arraysize = 1;
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

        RDCASSERT(s.mip < ARRAY_COUNT(m_GetTexturePrevData));

        // if we don't have this mip cached, fetch it now
        if(m_GetTexturePrevData[s.mip] == NULL)
        {
          m_GetTexturePrevData[s.mip] = new byte[dataSize * arraysize];
          if(IsGLES)
            texDetails.GetCompressedImageDataGLES(s.mip, target, dataSize * arraysize,
                                                  m_GetTexturePrevData[s.mip]);
          else
            drv.glGetCompressedTexImage(target, s.mip, m_GetTexturePrevData[s.mip]);
        }

        // now copy the slice from the cache into ret
        byte *src = m_GetTexturePrevData[s.mip];
        src += dataSize * s.slice;

        memcpy(data.data(), src, dataSize);
      }
      else
      {
        // for non-arrays we can just readback without caching
        if(IsGLES)
          texDetails.GetCompressedImageDataGLES(s.mip, target, dataSize, data.data());
        else
          drv.glGetCompressedTexImage(target, s.mip, data.data());
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

        RDCASSERT(s.mip < ARRAY_COUNT(m_GetTexturePrevData));

        // if we don't have this mip cached, fetch it now
        if(m_GetTexturePrevData[s.mip] == NULL)
        {
          m_GetTexturePrevData[s.mip] = new byte[dataSize * arraysize];
          drv.glGetTexImage(target, (GLint)s.mip, fmt, type, m_GetTexturePrevData[s.mip]);
        }

        // now copy the slice from the cache into ret
        byte *src = m_GetTexturePrevData[s.mip];
        src += dataSize * s.slice;

        memcpy(data.data(), src, dataSize);
      }
      else
      {
        drv.glGetTexImage(target, (GLint)s.mip, fmt, type, data.data());
      }

      if(params.standardLayout)
      {
        // GL puts D24 in the top bits (whether or not there's stencil). We choose to standardise it
        // to be in the low bits, so swizzle here. for D24 with no stencil, the stencil bits are
        // undefined so we can move them around and it means nothing.
        if(intFormat == eGL_DEPTH24_STENCIL8 || intFormat == eGL_DEPTH_COMPONENT24)
        {
          uint32_t *ptr = (uint32_t *)data.data();

          for(GLsizei z = 0; z < depth; z++)
          {
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
        }

        // GL's RGBA4/RGB5A1 is BGRA order, but it puts alpha in the bottom bits where we expect it
        // in the top
        if(intFormat == eGL_RGBA4)
        {
          uint16_t *ptr = (uint16_t *)data.data();

          for(size_t i = 0; i < data.size(); i += sizeof(uint16_t))
          {
            const uint16_t val = *ptr;
            *ptr = (val >> 4) | ((val & 0xf) << 12);
            ptr++;
          }
        }
        else if(intFormat == eGL_RGB5_A1)
        {
          uint16_t *ptr = (uint16_t *)data.data();

          for(size_t i = 0; i < data.size(); i += sizeof(uint16_t))
          {
            const uint16_t val = *ptr;
            *ptr = (val >> 1) | ((val & 0x1) << 15);
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

          for(GLsizei i = 0; i < height >> 1; i++)
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

void GLReplay::SetCustomShaderIncludes(const rdcarray<rdcstr> &directories)
{
}

void GLReplay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                 const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId &id, rdcstr &errors)
{
  if(sourceEncoding == ShaderEncoding::GLSL)
  {
    rdcstr sourceText = InsertSnippetAfterVersion(ShaderType::GLSL, (const char *)source.data(),
                                                  source.count(), GLSL_CUSTOM_PREFIX);

    bytebuf patchedSource;
    patchedSource.assign((byte *)sourceText.begin(), sourceText.size());

    return BuildTargetShader(sourceEncoding, patchedSource, entry, compileFlags, type, id, errors);
  }

  BuildTargetShader(sourceEncoding, source, entry, compileFlags, type, id, errors);
}

ResourceId GLReplay::ApplyCustomShader(TextureDisplay &display)
{
  if(display.customShaderId == ResourceId() || display.resourceId == ResourceId())
    return ResourceId();

  auto &texDetails = m_pDriver->m_Textures[display.resourceId];

  MakeCurrentReplayContext(m_DebugCtx);

  CreateCustomShaderTex(texDetails.width, texDetails.height);

  m_pDriver->glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.customFBO);
  m_pDriver->glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                                    DebugData.customTex, display.subresource.mip);

  m_pDriver->glViewport(0, 0, RDCMAX(1, texDetails.width >> display.subresource.mip),
                        RDCMAX(1, texDetails.height >> display.subresource.mip));

  DebugData.outWidth = float(RDCMAX(1, texDetails.width));
  DebugData.outHeight = float(RDCMAX(1, texDetails.height));

  float clr[] = {0.0f, 0.8f, 0.0f, 0.0f};
  m_pDriver->glClearBufferfv(eGL_COLOR, 0, clr);

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = display.customShaderId;
  disp.resourceId = display.resourceId;
  disp.typeCast = display.typeCast;
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.subresource = display.subresource;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = 0.0f;
  disp.rangeMax = 1.0f;
  disp.rawOutput = false;
  disp.scale = 1.0f;

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
  m_pDriver->glTextureParameteriEXT(DebugData.customTex, eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER,
                                    eGL_NEAREST);
  m_pDriver->glTextureParameteriEXT(DebugData.customTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER,
                                    eGL_NEAREST);
  m_pDriver->glTextureParameteriEXT(DebugData.customTex, eGL_TEXTURE_2D, eGL_TEXTURE_BASE_LEVEL, 0);
  m_pDriver->glTextureParameteriEXT(DebugData.customTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL,
                                    mips - 1);
  m_pDriver->glTextureParameteriEXT(DebugData.customTex, eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S,
                                    eGL_CLAMP_TO_EDGE);
  m_pDriver->glTextureParameteriEXT(DebugData.customTex, eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T,
                                    eGL_CLAMP_TO_EDGE);

  DebugData.CustomShaderTexID =
      m_pDriver->GetResourceManager()->GetResID(TextureRes(m_pDriver->GetCtx(), DebugData.customTex));
}

void GLReplay::FreeCustomShader(ResourceId id)
{
  if(id == ResourceId())
    return;

  m_pDriver->glDeleteShader(m_pDriver->GetResourceManager()->GetCurrentResource(id).name);
}

void GLReplay::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                 const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                 ShaderStage type, ResourceId &id, rdcstr &errors)
{
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
      id = ResourceId();
      return;
    }
  }

  const char *src = (const char *)source.data();
  GLint len = source.count();
  GLuint shader = drv.glCreateShader(shtype);
  drv.glShaderSource(shader, 1, &src, &len);
  drv.glCompileShader(shader);

  GLint status = 0;
  drv.glGetShaderiv(shader, eGL_COMPILE_STATUS, &status);

  {
    len = 1024;
    drv.glGetShaderiv(shader, eGL_INFO_LOG_LENGTH, &len);
    char *buffer = new char[len + 1];
    drv.glGetShaderInfoLog(shader, len, NULL, buffer);
    buffer[len] = 0;
    errors = buffer;
    delete[] buffer;
  }

  if(status == 0)
    id = ResourceId();
  else
    id = m_pDriver->GetResourceManager()->GetResID(ShaderRes(m_pDriver->GetCtx(), shader));
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

  GLenum intFormat = MakeGLFormat(templateTex.format);
  bool isCompressed = IsCompressedFormat(intFormat);

  GLenum baseFormat = eGL_RGBA;
  GLenum dataType = eGL_UNSIGNED_BYTE;
  if(!isCompressed)
  {
    baseFormat = GetBaseFormat(intFormat);
    dataType = GetDataType(intFormat);
  }

  if(baseFormat == eGL_NONE || dataType == eGL_NONE)
    return ResourceId();

  GLuint tex = 0;
  drv.glGenTextures(1, &tex);

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

            bytebuf dummy;
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

    drv.glTextureParameteriEXT(tex, target, eGL_TEXTURE_MAX_LEVEL, templateTex.mips - 1);
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

  ResourceId id = m_pDriver->GetResourceManager()->GetResID(TextureRes(m_pDriver->GetCtx(), tex));

  return id;
}

void GLReplay::SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data,
                                   size_t dataSize)
{
  WrappedOpenGL &drv = *m_pDriver;

  GLuint tex = m_pDriver->GetResourceManager()->GetCurrentResource(texid).name;

  auto &texdetails = m_pDriver->m_Textures[texid];

  if(texdetails.curType == eGL_NONE)
    return;

  GLenum fmt = texdetails.internalFormat;
  GLenum target = texdetails.curType;

  GLint depth = 1;
  if(target == eGL_TEXTURE_3D)
    depth = RDCMAX(1, texdetails.depth >> sub.mip);

  GLint width = RDCMAX(1, texdetails.width >> sub.mip);
  GLint height = RDCMAX(1, texdetails.height >> sub.mip);

  GLint mip =
      RDCMIN((GLint)sub.mip,
             GetNumMips(target, tex, texdetails.width, texdetails.height, texdetails.depth) - 1);
  GLint slice = (GLint)sub.slice;
  GLint sample = RDCMIN((GLint)sub.sample, texdetails.samples - 1);

  if(target == eGL_TEXTURE_1D_ARRAY)
    slice = RDCMIN(slice, texdetails.height);
  if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
     target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    slice = RDCMIN(slice, texdetails.depth);

  if(target == eGL_TEXTURE_1D_ARRAY)
    height = 1;

  if(IsCompressedFormat(fmt))
  {
    PixelUnpackState unpack;
    unpack.Fetch(true);

    ResetPixelUnpackState(true, 1);

    if(target == eGL_TEXTURE_1D)
    {
      drv.glCompressedTextureSubImage1DEXT(tex, target, mip, 0, width, fmt, (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_1D_ARRAY)
    {
      drv.glCompressedTextureSubImage2DEXT(tex, target, mip, 0, slice, width, 1, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D)
    {
      drv.glCompressedTextureSubImage2DEXT(tex, target, mip, 0, 0, width, height, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    {
      drv.glCompressedTextureSubImage3DEXT(tex, target, mip, 0, 0, slice, width, height, 1, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_3D)
    {
      drv.glCompressedTextureSubImage3DEXT(tex, target, mip, 0, 0, 0, width, height, depth, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_CUBE_MAP)
    {
      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      target = targets[RDCMIN(slice, GLint(ARRAY_COUNT(targets) - 1))];

      drv.glCompressedTextureSubImage2DEXT(tex, target, mip, 0, 0, width, height, fmt,
                                           (GLsizei)dataSize, data);
    }
    else if(target == eGL_TEXTURE_2D_MULTISAMPLE || target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    {
      RDCERR("Unexpected compressed MSAA texture!");
    }

    unpack.Apply(true);
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

    PixelUnpackState unpack;
    unpack.Fetch(false);

    ResetPixelUnpackState(false, 1);

    bytebuf swizzled;

    // GL puts D24 in the top bits (whether or not there's stencil). We choose to standardise it
    // to be in the low bits, so swizzle here. for D24 with no stencil, the stencil bits are
    // undefined so we can move them around and it means nothing.
    if(texdetails.internalFormat == eGL_DEPTH24_STENCIL8 ||
       texdetails.internalFormat == eGL_DEPTH_COMPONENT24)
    {
      const uint32_t *srcptr = (const uint32_t *)data;
      swizzled.resize(dataSize);
      uint32_t *dstptr = (uint32_t *)swizzled.data();

      for(size_t i = 0; i < dataSize; i += 4)
      {
        const uint32_t val = *srcptr;
        *dstptr = (val << 8) | ((val & 0xff000000) >> 24);
        srcptr++;
        dstptr++;
      }

      data = swizzled.data();
    }
    // GL's RGBA4/RGB5A1 is BGRA order, but it puts alpha in the bottom bits where we expect it
    // in the top
    else if(texdetails.internalFormat == eGL_RGBA4)
    {
      const uint16_t *srcptr = (const uint16_t *)data;
      swizzled.resize(dataSize);
      uint16_t *dstptr = (uint16_t *)swizzled.data();

      for(size_t i = 0; i < dataSize; i += 2)
      {
        const uint16_t val = *srcptr;
        *dstptr = ((val & 0x0fff) << 4) | ((val & 0xf000) >> 12);
        srcptr++;
        dstptr++;
      }

      data = swizzled.data();
    }
    else if(texdetails.internalFormat == eGL_RGB5_A1)
    {
      const uint16_t *srcptr = (const uint16_t *)data;
      swizzled.resize(dataSize);
      uint16_t *dstptr = (uint16_t *)swizzled.data();

      for(size_t i = 0; i < dataSize; i += 2)
      {
        const uint16_t val = *srcptr;
        *dstptr = ((val & 0x7fff) << 1) | ((val & 0x8000) >> 12);
        srcptr++;
        dstptr++;
      }

      data = swizzled.data();
    }

    if(target == eGL_TEXTURE_1D)
    {
      drv.glTextureSubImage1DEXT(tex, target, mip, 0, width, baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_1D_ARRAY)
    {
      drv.glTextureSubImage2DEXT(tex, target, mip, 0, slice, width, 1, baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_2D)
    {
      drv.glTextureSubImage2DEXT(tex, target, mip, 0, 0, width, height, baseformat, datatype, data);
    }
    else if(target == eGL_TEXTURE_2D_ARRAY || target == eGL_TEXTURE_CUBE_MAP_ARRAY)
    {
      drv.glTextureSubImage3DEXT(tex, target, mip, 0, 0, slice, width, height, 1, baseformat,
                                 datatype, data);
    }
    else if(target == eGL_TEXTURE_3D)
    {
      drv.glTextureSubImage3DEXT(tex, target, mip, 0, 0, 0, width, height, depth, baseformat,
                                 datatype, data);
    }
    else if(target == eGL_TEXTURE_CUBE_MAP)
    {
      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      target = targets[RDCMIN(slice, GLint(ARRAY_COUNT(targets) - 1))];

      drv.glTextureSubImage2DEXT(tex, target, mip, 0, 0, width, height, baseformat, datatype, data);
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
      drv.glTextureParameteriEXT(uploadTex, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

      GLint unpackedSlice = slice * texdetails.samples + sample;

      // upload the data to the given slice
      drv.glTextureSubImage3DEXT(uploadTex, eGL_TEXTURE_2D_ARRAY, 0, 0, 0, unpackedSlice, width,
                                 height, 1, baseformat, datatype, data);

      // copy this slice into the 2D MSAA texture
      m_pDriver->CopyArrayToTex2DMS(tex, uploadTex, width, height, texdetails.depth,
                                    texdetails.samples, texdetails.internalFormat, unpackedSlice);

      // delete the temporary texture
      drv.glDeleteTextures(1, &uploadTex);
    }

    unpack.Apply(false);
  }
}

bool GLReplay::IsTextureSupported(const TextureDescription &tex)
{
  // GL can't decide if these formats are BGRA or RGBA order.
  // The bit order in memory for e.g. R4G4B4A4 is:
  // 15 .. ..  0
  //   R G B A
  //
  // but if you upload bits in that order with GL_RGBA it gets flipped.
  // It's more reliable to report no support and force a remap
  switch(tex.format.type)
  {
    case ResourceFormatType::R4G4:
    case ResourceFormatType::R4G4B4A4:
    case ResourceFormatType::R5G6B5:
    case ResourceFormatType::R5G5B5A1: return false;
    default: break;
  }

  // We couldn't create proxy textures for ASTC textures (see MakeGLFormat). So we give back false
  // and let RemapProxyTextureIfNeeded to set remap type for them.
  if(tex.format.type == ResourceFormatType::ASTC)
    return false;

  // we don't try to replay alpha8 textures, as we stick strictly to core profile GL
  if(tex.format.type == ResourceFormatType::A8)
    return false;

  // don't support 1D/3D block compressed textures
  if(tex.dimension != 2 &&
     (tex.format.type == ResourceFormatType::BC1 || tex.format.type == ResourceFormatType::BC2 ||
      tex.format.type == ResourceFormatType::BC3 || tex.format.type == ResourceFormatType::BC4 ||
      tex.format.type == ResourceFormatType::BC5 || tex.format.type == ResourceFormatType::BC6 ||
      tex.format.type == ResourceFormatType::BC7 || tex.format.type == ResourceFormatType::ASTC ||
      tex.format.type == ResourceFormatType::ETC2 || tex.format.type == ResourceFormatType::EAC))
    return false;

  // don't support 3D depth textures
  if(tex.dimension == 3 &&
     (tex.format.compType == CompType::Depth || tex.format.type == ResourceFormatType::D16S8 ||
      tex.format.type == ResourceFormatType::D24S8 || tex.format.type == ResourceFormatType::D32S8))
    return false;

  GLenum fmt = MakeGLFormat(tex.format);

  if(fmt == eGL_NONE)
    return false;

  // BGRA is not accepted as an internal format in case of GL
  // EXT_texture_format_BGRA8888 is required for creating BGRA proxy textures in case of GLES
  if(fmt == eGL_BGRA8_EXT && (!IsGLES || !HasExt[EXT_texture_format_BGRA8888]))
    return false;

  GLenum target = eGL_TEXTURE_2D;

  switch(tex.type)
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

  GLint supported = 0, fragment = 0;
  m_pDriver->glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_SUPPORTED, 1, &supported);
  m_pDriver->glGetInternalformativ(target, fmt, eGL_FRAGMENT_TEXTURE, 1, &fragment);

  // check the texture is supported
  if(supported == 0 || fragment == 0)
    return false;

  // for multisampled textures it must be in a view compatibility class, to let us copy to/from the
  // MSAA texture.
  if(tex.msSamp > 1 && !IsDepthStencilFormat(fmt))
  {
    GLenum viewClass = eGL_NONE;
    m_pDriver->glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, fmt, eGL_VIEW_COMPATIBILITY_CLASS, 1,
                                     (GLint *)&viewClass);

    if(viewClass == eGL_NONE)
      return false;
  }

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

  ResourceId id = m_pDriver->GetResourceManager()->GetResID(BufferRes(m_pDriver->GetCtx(), buf));

  return id;
}

void GLReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
  GLuint buf = m_pDriver->GetResourceManager()->GetCurrentResource(bufid).name;

  m_pDriver->glNamedBufferSubDataEXT(buf, 0, dataSize, data);
}

rdcarray<EventUsage> GLReplay::GetUsage(ResourceId id)
{
  return m_pDriver->GetUsage(id);
}

ShaderDebugTrace *GLReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                        uint32_t idx, uint32_t view)
{
  GLNOTIMP("DebugVertex");
  return new ShaderDebugTrace();
}

ShaderDebugTrace *GLReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                       const DebugPixelInputs &inputs)
{
  GLNOTIMP("DebugPixel");
  return new ShaderDebugTrace();
}

ShaderDebugTrace *GLReplay::DebugThread(uint32_t eventId, const rdcfixedarray<uint32_t, 3> &groupid,
                                        const rdcfixedarray<uint32_t, 3> &threadid)
{
  GLNOTIMP("DebugThread");
  return new ShaderDebugTrace();
}

rdcarray<ShaderDebugState> GLReplay::ContinueDebug(ShaderDebugger *debugger)
{
  GLNOTIMP("ContinueDebug");
  return {};
}

void GLReplay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
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
  m_pDriver->UnregisterReplayContext(m_ReplayCtx);
}

RDResult CreateReplayDevice(RDCDriver rdcdriver, RDCFile *rdc, const ReplayOptions &opts,
                            GLPlatform &platform, IReplayDriver **&driver)
{
  GLInitParams initParams;
  uint64_t ver = GLInitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!GLInitParams::IsSupportedVersion(ver))
    {
      RETURN_ERROR_RESULT(
          ResultCode::APIIncompatibleVersion,
          "Incompatible OpenGL serialise version %llu, newest version supported is %llu", ver,
          GLInitParams::CurrentVersion);
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    ser.SetVersion(ver);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted,
                          "Expected to get a DriverInit chunk, instead got %u", chunk);
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
      return ser.GetError();

    if(!initParams.renderer.empty())
      RDCLOG("Capture was created on %s / %s", initParams.renderer.c_str(),
             initParams.version.c_str());
  }

  GLWindowingData data = {};

  RDResult status = platform.InitialiseAPI(data, rdcdriver, opts.apiValidation);

  // any errors will be already printed, just pass the error up
  if(status != ResultCode::Succeeded)
    return status;

  bool current = platform.MakeContextCurrent(data);
  if(!current)
  {
    platform.DeleteReplayContext(data);
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Couldn't activate the created replay context");
  }

  // we use the platform's function which tries GL's GetProcAddress first, then falls back to
  // regular function lookup
  GL.PopulateWithCallback([&platform](const char *func) { return platform.GetReplayFunction(func); });

  FetchEnabledExtensions();

  // see gl_emulated.cpp
  GL.EmulateUnsupportedFunctions();
  GL.EmulateRequiredExtensions();

  RDResult extensionsValidated = CheckReplayContext();
  if(extensionsValidated != ResultCode::Succeeded)
  {
    platform.DeleteReplayContext(data);
    return extensionsValidated;
  }

  RDResult functionsValidated = ValidateFunctionPointers();
  if(functionsValidated != ResultCode::Succeeded)
  {
    platform.DeleteReplayContext(data);
    return functionsValidated;
  }

  WrappedOpenGL *gldriver = new WrappedOpenGL(platform);
  gldriver->SetDriverType(rdcdriver);

  GL.DriverForEmulation(gldriver);

  RDCLOG("Created %s replay device.", ToStr(rdcdriver).c_str());

  GLReplay *replay = gldriver->GetReplay();
  replay->SetProxy(rdc == NULL);
  replay->SetReplayData(data);

  if(!replay->HasDebugContext())
  {
    delete gldriver;
    platform.DeleteReplayContext(data);
    RETURN_ERROR_RESULT(ResultCode::APIHardwareUnsupported, "Failed to create analysis context");
  }

  gldriver->Initialise(initParams, ver, opts);

  *driver = (IReplayDriver *)replay;
  return ResultCode::Succeeded;
}

RDResult GL_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  GLDummyPlatform dummy;
  WrappedOpenGL device(dummy);

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  RDResult status = device.ReadLogInitialisation(rdc, true);

  if(status == ResultCode::Succeeded)
    device.GetStructuredFile()->Swap(output);

  return status;
}

static StructuredProcessRegistration GLProcessRegistration(RDCDriver::OpenGL, &GL_ProcessStructured);
static StructuredProcessRegistration GLESProcessRegistration(RDCDriver::OpenGLES,
                                                             &GL_ProcessStructured);

rdcarray<GLVersion> GetReplayVersions(RDCDriver api)
{
  // try to create all versions from highest down to lowest in order to get the highest versioned
  // context we can
  if(api == RDCDriver::OpenGLES)
  {
    return {
        {3, 2},
        {3, 1},
        {3, 0},
    };
  }
  else
  {
    return {
        {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}, {3, 2},
    };
  }
}

#if defined(RENDERDOC_SUPPORT_GLES)

RDResult GLES_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  RDCLOG("Creating an OpenGL ES replay device");

  // for GLES replay, we try to use EGL if it's available. If it's not available, we look to see if
  // we can create an OpenGL ES context via the platform GL functions
  if(GetEGLPlatform().CanCreateGLESContext())
  {
    bool load_ok = GetEGLPlatform().PopulateForReplay();

    if(!load_ok)
    {
      RETURN_ERROR_RESULT(ResultCode::APIInitFailed,
                          "Couldn't find required EGL function addresses");
    }

    RDCLOG("Initialising GLES replay via libEGL");

    return CreateReplayDevice(rdc ? rdc->GetDriver() : RDCDriver::OpenGLES, rdc, opts,
                              GetEGLPlatform(), driver);
  }
#if defined(RENDERDOC_SUPPORT_GL)
  else if(GetGLPlatform().CanCreateGLESContext())
  {
    RDCLOG("libEGL is not available, falling back to EXT_create_context_es2_profile");

    bool load_ok = GetGLPlatform().PopulateForReplay();

    if(!load_ok)
    {
      RETURN_ERROR_RESULT(ResultCode::APIInitFailed,
                          "Couldn't find required GL function addresses");
    }

    return CreateReplayDevice(rdc ? rdc->GetDriver() : RDCDriver::OpenGLES, rdc, opts,
                              GetGLPlatform(), driver);
  }

  RETURN_ERROR_RESULT(ResultCode::APIInitFailed,
                      "libEGL not available, and GL cannot initialise or doesn't support "
                      "EXT_create_context_es2_profile");
#else
  // no GL support, no fallback apart from EGL

  RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "libEGL is not available");
#endif
}

static DriverRegistration GLESDriverRegistration(RDCDriver::OpenGLES, &GLES_CreateReplayDevice);

#endif

#if defined(RENDERDOC_SUPPORT_GL)

RDResult GL_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  GLPlatform *gl_platform = &GetGLPlatform();

  if(RenderDoc::Inst().GetGlobalEnvironment().waylandDisplay)
  {
#if defined(RENDERDOC_SUPPORT_EGL)
    RDCLOG("Forcing EGL device creation for wayland");
    gl_platform = &GetEGLPlatform();
#else
    RETURN_ERROR_RESULT(ResultCode::InternalError,
                        "EGL support must be enabled at build time when using Wayland");
#endif
  }

  bool can_create_gl_context = gl_platform->CanCreateGLContext();

#if defined(RENDERDOC_SUPPORT_EGL)
  if(!can_create_gl_context && gl_platform == &GetGLPlatform())
  {
    RDCLOG("Cannot create GL context with GL platform, falling back to EGL");
    gl_platform = &GetEGLPlatform();
    can_create_gl_context = gl_platform->CanCreateGLContext();
  }
#endif

  if(!can_create_gl_context)
  {
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed,
                        "Current platform doesn't support OpenGL contexts");
  }

  RDCDEBUG("Creating an OpenGL replay device");

  bool load_ok = gl_platform->PopulateForReplay();

  if(!load_ok)
  {
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed,
                        "Couldn't find required platform %s function addresses",
                        gl_platform == &GetGLPlatform() ? "GL" : "EGL");
  }

  return CreateReplayDevice(rdc ? rdc->GetDriver() : RDCDriver::OpenGL, rdc, opts, *gl_platform,
                            driver);
}

static DriverRegistration GLDriverRegistration(RDCDriver::OpenGL, &GL_CreateReplayDevice);

#endif
