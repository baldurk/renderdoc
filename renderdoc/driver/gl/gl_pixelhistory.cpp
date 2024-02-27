/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <algorithm>
#include "data/glsl_shaders.h"
#include "maths/formatpacking.h"
#include "gl_common.h"
#include "gl_driver.h"
#include "gl_replay.h"

namespace
{
bool isDirectWrite(ResourceUsage usage)
{
  return ((usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::CS_RWResource) ||
          usage == ResourceUsage::CopyDst || usage == ResourceUsage::Copy ||
          usage == ResourceUsage::Resolve || usage == ResourceUsage::ResolveDst ||
          usage == ResourceUsage::GenMips);
}

struct FramebufferKey
{
  GLenum colorFormat;
  GLenum depthFormat;
  GLenum stencilFormat;
  uint32_t numSamples;
  bool operator<(const FramebufferKey &other) const
  {
    if(colorFormat < other.colorFormat)
    {
      return true;
    }
    if(colorFormat > other.colorFormat)
    {
      return false;
    }
    if(depthFormat < other.depthFormat)
    {
      return true;
    }
    if(depthFormat > other.depthFormat)
    {
      return false;
    }
    if(stencilFormat < other.stencilFormat)
    {
      return true;
    }
    if(stencilFormat > other.stencilFormat)
    {
      return false;
    }
    return numSamples < other.numSamples;
  }
};

struct CopyFramebuffer
{
  GLuint framebufferId;
  GLuint colorTextureId;
  GLuint dsTextureId;
  GLuint depthTextureId;
  GLuint stencilTextureId;
  GLuint stencilViewId;
  uint32_t width;
  FramebufferKey format;
};

struct ShaderOutFramebuffer
{
  GLuint framebufferId;
  GLuint colorTextureId;
  GLuint dsTextureId;
  FramebufferKey format;
};

struct GLPixelHistoryResources
{
  ResourceId target;

  // Used for offscreen rendering for draw call events.
  GLuint fullPrecisionColorImage;
  GLuint fullPrecisionDsImage;
  GLuint fullPrecisionFrameBuffer;
  GLuint primitiveIdFragmentShader;
  GLuint primitiveIdFragmentShaderSPIRV;
  GLuint msCopyComputeProgram;
  GLuint msCopyDepthComputeProgram;
  GLuint msCopyDstBuffer;
  GLuint msCopyUniformBlockBuffer;
  std::unordered_map<GLuint, GLuint> programs;
  std::map<FramebufferKey, CopyFramebuffer> copyFramebuffers;
  std::map<FramebufferKey, ShaderOutFramebuffer> shaderOutIntFramebuffers;
};

enum class OpenGLTest
{
  FaceCulling,
  ScissorTest,
  StencilTest,
  DepthTest,
  SampleMask,
  NumTests
};

enum class PerFragmentQueryType
{
  ShaderOut,
  PostMod,
  PrimitiveId
};

GLuint GetPrimitiveIdProgram(WrappedOpenGL *driver, GLReplay *replay,
                             GLPixelHistoryResources &resources, GLuint currentProgram)
{
  auto programIterator = resources.programs.find(currentProgram);
  if(programIterator != resources.programs.end())
  {
    return programIterator->second;
  }

  GLRenderState rs;
  rs.FetchState(driver);

  GLuint primitiveIdProgram = driver->glCreateProgram();
  replay->CreateFragmentShaderReplacementProgram(
      rs.Program.name, primitiveIdProgram, rs.Pipeline.name, resources.primitiveIdFragmentShader,
      resources.primitiveIdFragmentShaderSPIRV);

  return primitiveIdProgram;
}

// Returns a Framebuffer that has the same depth and stencil formats of the currently bound
// framebuffer
// so that you can blit from the current bound framebuffer into the new framebuffer
const CopyFramebuffer &getCopyFramebuffer(WrappedOpenGL *driver,
                                          std::map<FramebufferKey, CopyFramebuffer> &copyFramebuffers,
                                          uint32_t numSamples, uint32_t numEvents, GLenum depthFormat,
                                          GLenum stencilFormat, GLenum colorFormat)
{
  bool multisampled = numSamples > 1;

  GLuint curDepth;
  GLint depthType;
  GLuint curStencil;
  GLint stencilType;

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curDepth);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depthType);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curStencil);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &stencilType);

  auto it = copyFramebuffers.find({colorFormat, depthFormat, stencilFormat, numSamples});
  if(it != copyFramebuffers.end())
  {
    return it->second;
  }

  GLint savedReadFramebuffer, savedDrawFramebuffer;
  driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
  driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

  CopyFramebuffer copyFramebuffer;
  RDCEraseEl(copyFramebuffer);
  copyFramebuffer.width = numEvents;
  // Allocate a framebuffer that will render to the textures
  driver->glGenFramebuffers(1, &copyFramebuffer.framebufferId);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, copyFramebuffer.framebufferId);

  // Allocate a texture for the pixel history colour values
  GLenum textureTarget = multisampled ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;
  driver->glGenTextures(1, &copyFramebuffer.colorTextureId);
  driver->CreateTextureImage(copyFramebuffer.colorTextureId, colorFormat, eGL_NONE, eGL_NONE,
                             textureTarget, 2, copyFramebuffer.width, 1, 1, numSamples, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                               copyFramebuffer.colorTextureId, 0);

  // Allocate a texture(s) for the pixel history depth/stencil values matching the capture's
  // frambebuffer's formats
  if(curStencil == curDepth && curStencil != 0)
  {
    driver->glGenTextures(1, &copyFramebuffer.dsTextureId);
    driver->CreateTextureImage(copyFramebuffer.dsTextureId, depthFormat, eGL_NONE, eGL_NONE,
                               textureTarget, 2, copyFramebuffer.width, 1, 1, numSamples, 1);
    driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                                 copyFramebuffer.dsTextureId, 0);

    if(multisampled)
    {
      driver->glGenTextures(1, &copyFramebuffer.stencilViewId);
      driver->glTextureView(copyFramebuffer.stencilViewId, textureTarget,
                            copyFramebuffer.dsTextureId, depthFormat, 0, 1, 0, 1);
    }
  }
  else
  {
    if(curDepth != 0)
    {
      driver->glGenTextures(1, &copyFramebuffer.depthTextureId);
      driver->CreateTextureImage(copyFramebuffer.depthTextureId, depthFormat, eGL_NONE, eGL_NONE,
                                 textureTarget, 2, copyFramebuffer.width, 1, 1, numSamples, 1);
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                   copyFramebuffer.depthTextureId, 0);
    }
    if(curStencil != 0)
    {
      driver->glGenTextures(1, &copyFramebuffer.stencilTextureId);
      driver->CreateTextureImage(copyFramebuffer.stencilTextureId, stencilFormat, eGL_NONE, eGL_NONE,
                                 textureTarget, 2, copyFramebuffer.width, 1, 1, numSamples, 1);
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                   copyFramebuffer.stencilTextureId, 0);
    }
  }

  // restore the capture's framebuffer
  driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
  driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

  FramebufferKey key = {colorFormat, depthFormat, stencilFormat, numSamples};
  copyFramebuffer.format = key;
  copyFramebuffers[key] = copyFramebuffer;
  return copyFramebuffers[key];
}

GLenum getTextureFormatType(GLenum internalFormat)
{
  if(IsUIntFormat(internalFormat))
  {
    return eGL_UNSIGNED_INT;
  }
  if(IsSIntFormat(internalFormat))
  {
    return eGL_INT;
  }
  // There are other format types that this format could belong to, but they can be blitted between
  // each other
  return eGL_UNSIGNED_NORMALIZED;
}

GLenum getCurrentTextureFormat(WrappedOpenGL *driver, uint32_t colIdx)
{
  GLuint curColor;
  GLint colorType;

  driver->glGetFramebufferAttachmentParameteriv(
      eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
      eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curColor);
  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER,
                                                GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &colorType);

  GLenum colorFormat = eGL_NONE;

  if(curColor != 0)
  {
    ResourceId id;
    if(colorType != eGL_RENDERBUFFER)
    {
      id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curColor));
    }
    else
    {
      id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curColor));
    }
    colorFormat = driver->m_Textures[id].internalFormat;
  }

  return colorFormat;
}

const CopyFramebuffer &getCopyFramebuffer(WrappedOpenGL *driver, uint32_t colIdx,
                                          std::map<FramebufferKey, CopyFramebuffer> &copyFramebuffers,
                                          uint32_t numSamples, uint32_t numEvents)
{
  GLuint curDepth;
  GLint depthType;
  GLuint curStencil;
  GLint stencilType;
  GLuint curColor;
  GLint colorType;

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curDepth);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depthType);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curStencil);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &stencilType);

  driver->glGetFramebufferAttachmentParameteriv(
      eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
      eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curColor);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER,
                                                GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &colorType);

  GLenum depthFormat = eGL_NONE;

  if(curDepth != 0)
  {
    ResourceId id;
    if(depthType != eGL_RENDERBUFFER)
    {
      id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curDepth));
    }
    else
    {
      id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curDepth));
    }
    depthFormat = driver->m_Textures[id].internalFormat;
  }

  GLenum stencilFormat = eGL_NONE;
  if(curStencil != 0)
  {
    ResourceId id;
    if(stencilType != eGL_RENDERBUFFER)
    {
      id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curStencil));
    }
    else
    {
      id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curStencil));
    }
    stencilFormat = driver->m_Textures[id].internalFormat;
  }

  GLenum colorFormat = eGL_NONE;
  if(curColor != 0)
  {
    ResourceId id;
    if(colorType != eGL_RENDERBUFFER)
    {
      id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curColor));
    }
    else
    {
      id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curColor));
    }
    colorFormat = driver->m_Textures[id].internalFormat;
  }

  return getCopyFramebuffer(driver, copyFramebuffers, numSamples, numEvents, depthFormat,
                            stencilFormat, colorFormat);
}

ShaderOutFramebuffer getShaderOutFramebuffer(WrappedOpenGL *driver, uint32_t colIdx,
                                             GLPixelHistoryResources &resources, FramebufferKey key,
                                             uint32_t width, uint32_t height)
{
  std::map<FramebufferKey, ShaderOutFramebuffer> &shaderOutFramebuffers =
      resources.shaderOutIntFramebuffers;
  auto it = shaderOutFramebuffers.find(key);

  if(it != shaderOutFramebuffers.end())
  {
    driver->glBindFramebuffer(eGL_FRAMEBUFFER, it->second.framebufferId);
    for(uint32_t i = 0; i < 8; i++)
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + i), 0, 0);
    driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
                                 it->second.colorTextureId, 0);
    return it->second;
  }

  GLint savedReadFramebuffer, savedDrawFramebuffer;
  driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
  driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

  bool multisampled = key.numSamples > 1;
  GLenum textureTarget = multisampled ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;
  ShaderOutFramebuffer shaderOutFramebuffer;
  RDCEraseEl(shaderOutFramebuffer);

  // Allocate a framebuffer that will render to the textures
  driver->glGenFramebuffers(1, &shaderOutFramebuffer.framebufferId);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, shaderOutFramebuffer.framebufferId);

  // Allocate a texture for the pixel history colour values
  driver->glGenTextures(1, &shaderOutFramebuffer.colorTextureId);
  driver->glBindTexture(textureTarget, shaderOutFramebuffer.colorTextureId);
  driver->CreateTextureImage(shaderOutFramebuffer.colorTextureId, key.colorFormat, eGL_NONE,
                             eGL_NONE, textureTarget, 2, width, height, 1, key.numSamples, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
                               shaderOutFramebuffer.colorTextureId, 0);

  // Allocate a texture for the pixel history depth/stencil values
  driver->glGenTextures(1, &shaderOutFramebuffer.dsTextureId);
  driver->glBindTexture(textureTarget, shaderOutFramebuffer.dsTextureId);
  driver->CreateTextureImage(shaderOutFramebuffer.dsTextureId, eGL_DEPTH32F_STENCIL8, eGL_NONE,
                             eGL_NONE, textureTarget, 2, width, height, 1, key.numSamples, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                               shaderOutFramebuffer.dsTextureId, 0);

  // restore the capture's framebuffer
  driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
  driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

  return (shaderOutFramebuffers[key] = shaderOutFramebuffer);
}

GLbitfield getFramebufferCopyMask(WrappedOpenGL *driver)
{
  GLuint curDepth = 0;
  GLuint curStencil = 0;
  GLuint curColor = 0;

  GLint colorAttachment;
  driver->glGetIntegerv(eGL_READ_BUFFER, &colorAttachment);

  if(colorAttachment)
  {
    driver->glGetFramebufferAttachmentParameteriv(eGL_READ_FRAMEBUFFER, GLenum(colorAttachment),
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                  (GLint *)&curColor);
  }
  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curDepth);

  driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                (GLint *)&curStencil);

  GLbitfield mask = 0;
  if(curColor)
  {
    mask |= eGL_COLOR_BUFFER_BIT;
  }
  if(curDepth)
  {
    mask |= eGL_DEPTH_BUFFER_BIT;
  }
  if(curStencil)
  {
    mask |= eGL_STENCIL_BUFFER_BIT;
  }
  return mask;
}

uint32_t getFramebufferColIndex(WrappedOpenGL *driver, ResourceId target)
{
  WrappedOpenGL &drv = *driver;
  GLResourceManager *rm = driver->GetResourceManager();

  GLuint numCols = 8;
  drv.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, (GLint *)&numCols);

  GLuint curCol[32] = {0};
  GLuint curDepth = 0;
  GLuint curStencil = 0;
  bool rbCol[32] = {false};
  bool rbDepth = false;
  bool rbStencil = false;

  GLenum type = eGL_TEXTURE;
  for(GLuint i = 0; i < numCols; i++)
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

  for(GLuint i = 0; i < numCols; i++)
  {
    ResourceId id = rm->GetResID(rbCol[i] ? RenderbufferRes(driver->GetCtx(), curCol[i])
                                          : TextureRes(driver->GetCtx(), curCol[i]));

    if(id == target)
      return i;
  }

  ResourceId depth = rm->GetResID(rbDepth ? RenderbufferRes(driver->GetCtx(), curDepth)
                                          : TextureRes(driver->GetCtx(), curDepth));
  ResourceId stencil = rm->GetResID(rbStencil ? RenderbufferRes(driver->GetCtx(), curStencil)
                                              : TextureRes(driver->GetCtx(), curStencil));

  // silently return attachment 0, it doesn't matter which colour attachment we use
  if(depth == target || stencil == target)
    return 0;

  RDCWARN("Couldn't find attachment %s anywhere in this framebuffer", ToStr(target).c_str());

  return 0;
}

bool PixelHistorySetupResources(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                const TextureDescription &desc, const Subresource &sub,
                                uint32_t numEvents, GLuint glslVersion, uint32_t numSamples)
{
  // Allocate a framebuffer that will render to the textures
  driver->glGenFramebuffers(1, &resources.fullPrecisionFrameBuffer);
  driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);

  // Allocate a texture for the pixel history colour values
  bool multisampled = numSamples > 1;
  GLenum textureTarget = multisampled ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;
  driver->glGenTextures(1, &resources.fullPrecisionColorImage);
  driver->glBindTexture(textureTarget, resources.fullPrecisionColorImage);
  driver->CreateTextureImage(resources.fullPrecisionColorImage, eGL_RGBA32F, eGL_NONE, eGL_NONE,
                             textureTarget, 2, desc.width >> sub.mip, desc.height >> sub.mip, 1,
                             numSamples, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                               resources.fullPrecisionColorImage, 0);

  // Allocate a texture for the pixel history depth/stencil values
  driver->glGenTextures(1, &resources.fullPrecisionDsImage);
  driver->glBindTexture(textureTarget, resources.fullPrecisionDsImage);
  driver->CreateTextureImage(resources.fullPrecisionDsImage, eGL_DEPTH32F_STENCIL8, eGL_NONE,
                             eGL_NONE, textureTarget, 2, desc.width >> sub.mip,
                             desc.height >> sub.mip, 1, numSamples, 1);
  driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT,
                               resources.fullPrecisionDsImage, 0);

  // We will blit the color values to this texture and then use a compute shader to get the values
  // for the sample that we want
  driver->glGenBuffers(1, &resources.msCopyDstBuffer);
  driver->glBindBuffer(eGL_SHADER_STORAGE_BUFFER, resources.msCopyDstBuffer);
  driver->glNamedBufferDataEXT(
      resources.msCopyDstBuffer,
      8 * (sizeof(float)) * numEvents,    // 8 floats per event (r,g,b,a,depth,null,null,null)
      NULL, eGL_DYNAMIC_READ);

  driver->glGenBuffers(1, &resources.msCopyUniformBlockBuffer);
  driver->glBindBuffer(eGL_UNIFORM_BUFFER, resources.msCopyUniformBlockBuffer);
  driver->glNamedBufferDataEXT(resources.msCopyUniformBlockBuffer, 8 * sizeof(uint32_t), NULL,
                               eGL_DYNAMIC_DRAW);

  // If the GLSL version is greater than or equal to 330, we can use IntBitsToFloat, otherwise we
  // need to write the float value directly.
  rdcstr glslSource;
  if(glslVersion >= 330)
  {
    glslSource = GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag),
                                    ShaderType::GLSL, glslVersion);
  }
  else
  {
    glslSource =
        GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag), ShaderType::GLSL,
                           glslVersion, "#define INT_BITS_TO_FLOAT_NOT_SUPPORTED\n");
  }
  // SPIR-V shaders are always generated as desktop GL 430, for ease
  rdcstr spirvSource = GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_primid_frag),
                                          ShaderType::GLSPIRV, 430);
  rdcstr msCopySource =
      GenerateGLSLShader(GetEmbeddedResource(glsl_pixelhistory_mscopy_comp), ShaderType::GLSL, 430);
  rdcstr msCopySourceDepth = GenerateGLSLShader(
      GetEmbeddedResource(glsl_pixelhistory_mscopy_depth_comp), ShaderType::GLSL, 430);
  resources.primitiveIdFragmentShaderSPIRV = CreateSPIRVShader(eGL_FRAGMENT_SHADER, spirvSource);
  resources.primitiveIdFragmentShader = CreateShader(eGL_FRAGMENT_SHADER, glslSource);
  resources.msCopyComputeProgram = CreateCShaderProgram(msCopySource);
  resources.msCopyDepthComputeProgram = CreateCShaderProgram(msCopySourceDepth);
  return true;
}

bool PixelHistoryDestroyResources(WrappedOpenGL *driver, const GLPixelHistoryResources &resources)
{
  driver->glDeleteTextures(1, &resources.fullPrecisionColorImage);
  driver->glDeleteTextures(1, &resources.fullPrecisionDsImage);
  driver->glDeleteFramebuffers(1, &resources.fullPrecisionFrameBuffer);
  driver->glDeleteShader(resources.primitiveIdFragmentShader);
  driver->glDeleteShader(resources.primitiveIdFragmentShaderSPIRV);
  driver->glDeleteProgram(resources.msCopyComputeProgram);
  driver->glDeleteProgram(resources.msCopyDepthComputeProgram);
  driver->glDeleteBuffers(1, &resources.msCopyDstBuffer);
  driver->glDeleteBuffers(1, &resources.msCopyUniformBlockBuffer);

  for(const std::pair<const GLuint, GLuint> &resourceProgram : resources.programs)
  {
    driver->glDeleteProgram(resourceProgram.second);
  }

  for(const auto &pair : resources.copyFramebuffers)
  {
    const CopyFramebuffer &cf = pair.second;
    driver->glDeleteFramebuffers(1, &cf.framebufferId);
    driver->glDeleteTextures(1, &cf.colorTextureId);
    driver->glDeleteTextures(1, &cf.dsTextureId);
    driver->glDeleteTextures(1, &cf.depthTextureId);
    driver->glDeleteTextures(1, &cf.stencilTextureId);
    driver->glDeleteTextures(1, &cf.stencilViewId);
  }

  for(const auto &pair : resources.shaderOutIntFramebuffers)
  {
    const ShaderOutFramebuffer &f = pair.second;
    driver->glDeleteFramebuffers(1, &f.framebufferId);
    driver->glDeleteTextures(1, &f.colorTextureId);
    driver->glDeleteTextures(1, &f.dsTextureId);
  }

  return true;
}

void CopyMSSample(WrappedOpenGL *driver, const GLPixelHistoryResources &resources,
                  const CopyFramebuffer &copyFramebuffer, int sampleIdx, int x, int y,
                  float *pixelDst)
{
  GLint savedProgram;
  driver->glGetIntegerv(eGL_CURRENT_PROGRAM, &savedProgram);
  GLint savedActiveTexture;
  driver->glGetIntegerv(eGL_ACTIVE_TEXTURE, &savedActiveTexture);
  GLint savedShaderStorageBuffer;
  driver->glGetIntegerv(eGL_SHADER_STORAGE_BUFFER_BINDING, &savedShaderStorageBuffer);
  GLint savedUniformBuffer;
  driver->glGetIntegerv(eGL_UNIFORM_BUFFER_BINDING, &savedUniformBuffer);

  struct Bufbind
  {
    GLuint buf;
    GLuint64 start;
    GLuint64 size;
  } ubo3, ssbo2;

  GL.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, 3, (GLint *)&ubo3.buf);
  GL.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, 3, (GLint64 *)&ubo3.start);
  GL.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, 3, (GLint64 *)&ubo3.size);

  GL.glGetIntegeri_v(eGL_SHADER_STORAGE_BUFFER_BINDING, 2, (GLint *)&ssbo2.buf);
  GL.glGetInteger64i_v(eGL_SHADER_STORAGE_BUFFER_START, 2, (GLint64 *)&ssbo2.start);
  GL.glGetInteger64i_v(eGL_SHADER_STORAGE_BUFFER_SIZE, 2, (GLint64 *)&ssbo2.size);

  driver->glUseProgram(resources.msCopyComputeProgram);
  GLint srcMSLoc = driver->glGetUniformLocation(resources.msCopyComputeProgram, "srcMS");
  driver->glUniform1i(srcMSLoc, 0);

  uint32_t uniforms[4] = {uint32_t(sampleIdx), uint32_t(x), uint32_t(y),
                          0};    // { sampleIdx, x, y, dstOffset }
  driver->glBindBuffer(eGL_UNIFORM_BUFFER, resources.msCopyUniformBlockBuffer);
  driver->glNamedBufferSubDataEXT(resources.msCopyUniformBlockBuffer, 0, sizeof(uniforms), uniforms);

  driver->glBindBufferBase(eGL_UNIFORM_BUFFER, 3, resources.msCopyUniformBlockBuffer);

  driver->glActiveTexture(eGL_TEXTURE0);
  GLint savedMSTexture0;
  driver->glGetIntegerv(eGL_TEXTURE_BINDING_2D_MULTISAMPLE, &savedMSTexture0);
  driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, copyFramebuffer.colorTextureId);

  driver->glBindBuffer(eGL_SHADER_STORAGE_BUFFER, resources.msCopyDstBuffer);
  driver->glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 2, resources.msCopyDstBuffer);

  driver->glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
  driver->glDispatchCompute(1, 1, 1);

  driver->glUseProgram(resources.msCopyDepthComputeProgram);
  GLint depthMSLoc = driver->glGetUniformLocation(resources.msCopyDepthComputeProgram, "depthMS");
  GLint stencilMSLoc =
      driver->glGetUniformLocation(resources.msCopyDepthComputeProgram, "stencilMS");

  driver->glUniform1i(depthMSLoc, 0);
  driver->glUniform1i(stencilMSLoc, 1);

  // { sampleIdx, x, y, dstOffset, hasDepth, hasStencil }
  uint32_t newUniforms[6] = {
      uint32_t(sampleIdx),
      uint32_t(x),
      uint32_t(y),
      1,
      copyFramebuffer.dsTextureId != 0 || copyFramebuffer.depthTextureId != 0,
      copyFramebuffer.dsTextureId != 0 || copyFramebuffer.stencilTextureId != 0,
  };
  driver->glNamedBufferSubDataEXT(resources.msCopyUniformBlockBuffer, 0, sizeof(newUniforms),
                                  newUniforms);

  driver->glActiveTexture(eGL_TEXTURE1);
  GLint savedMSTexture1;
  driver->glGetIntegerv(eGL_TEXTURE_BINDING_2D_MULTISAMPLE, &savedMSTexture1);

  if(copyFramebuffer.dsTextureId)
  {
    driver->glActiveTexture(eGL_TEXTURE0);
    driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, copyFramebuffer.dsTextureId);
    driver->glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                            eGL_DEPTH_COMPONENT);

    driver->glActiveTexture(eGL_TEXTURE1);
    driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, copyFramebuffer.stencilViewId);
    driver->glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                            eGL_STENCIL_INDEX);
  }
  else
  {
    driver->glActiveTexture(eGL_TEXTURE0);
    driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, copyFramebuffer.depthTextureId);

    driver->glActiveTexture(eGL_TEXTURE1);
    driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, copyFramebuffer.stencilTextureId);
  }

  driver->glDispatchCompute(1, 1, 1);
  driver->glMemoryBarrier(eGL_SHADER_STORAGE_BARRIER_BIT);

  driver->glGetBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, 8 * 4, pixelDst);

  if(ubo3.buf == 0 || (ubo3.start == 0 && ubo3.size == 0))
    GL.glBindBufferBase(eGL_UNIFORM_BUFFER, 3, ubo3.buf);
  else
    GL.glBindBufferRange(eGL_UNIFORM_BUFFER, 3, ubo3.buf, (GLintptr)ubo3.start,
                         (GLsizeiptr)ubo3.size);

  if(ssbo2.buf == 0 || (ssbo2.start == 0 && ssbo2.size == 0))
    GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 2, ssbo2.buf);
  else
    GL.glBindBufferRange(eGL_SHADER_STORAGE_BUFFER, 2, ssbo2.buf, (GLintptr)ssbo2.start,
                         (GLsizeiptr)ssbo2.size);

  driver->glUseProgram(savedProgram);
  driver->glActiveTexture(eGL_TEXTURE0);
  driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, savedMSTexture0);
  driver->glActiveTexture(eGL_TEXTURE1);
  driver->glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE, savedMSTexture1);
  driver->glActiveTexture((GLenum)savedActiveTexture);
  driver->glBindBuffer(eGL_SHADER_STORAGE_BUFFER, savedShaderStorageBuffer);
  driver->glBindBuffer(eGL_UNIFORM_BUFFER, savedUniformBuffer);
}

rdcarray<EventUsage> QueryModifyingEvents(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                          const rdcarray<EventUsage> &events, int x, int y,
                                          int mipLevel, rdcarray<PixelModification> &history)
{
  GLMarkerRegion region("QueryModifyingEvents");
  rdcarray<EventUsage> modEvents;
  rdcarray<GLuint> occlusionQueries;
  std::set<uint32_t> ignoredEvents;
  occlusionQueries.resize(events.size());
  driver->glGenQueries((GLsizei)occlusionQueries.size(), occlusionQueries.data());

  driver->ReplayLog(0, events[0].eventId, eReplay_WithoutDraw);
  // execute the occlusion queries
  for(size_t i = 0; i < events.size(); i++)
  {
    uint32_t colIdx = getFramebufferColIndex(driver, resources.target);

    bool ignored = false;
    int objectType;
    driver->glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
        eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objectType);
    // Ignore the event if the framebuffer is attached to a different mip level than the one we're
    // interested in
    if(objectType == eGL_TEXTURE)
    {
      int attachedMipLevel;
      driver->glGetFramebufferAttachmentParameteriv(
          eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
          eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &attachedMipLevel);
      if(mipLevel != attachedMipLevel)
      {
        ignoredEvents.insert(events[i].eventId);
        ignored = true;
      }
    }
    if(!ignored && !(events[i].usage == ResourceUsage::Clear || isDirectWrite(events[i].usage)))
    {
      GLRenderState state;
      state.FetchState(driver);

      driver->glDisable(eGL_DEPTH_TEST);
      driver->glDisable(eGL_STENCIL_TEST);
      driver->glDisable(eGL_CULL_FACE);
      driver->glDisable(eGL_SAMPLE_MASK);
      driver->glDisable(eGL_DEPTH_CLAMP);
      driver->glEnable(eGL_SCISSOR_TEST);
      driver->glScissor(x, y, 1, 1);

      driver->SetFetchCounters(true);
      driver->glBeginQuery(eGL_SAMPLES_PASSED, occlusionQueries[i]);
      driver->ReplayLog(events[i].eventId, events[i].eventId, eReplay_OnlyDraw);
      driver->glEndQuery(eGL_SAMPLES_PASSED);
      driver->SetFetchCounters(false);

      state.ApplyState(driver);
    }
    else
    {
      driver->ReplayLog(events[i].eventId, events[i].eventId, eReplay_OnlyDraw);
    }

    if(i < events.size() - 1)
    {
      driver->ReplayLog(events[i].eventId + 1, events[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
  // read back the occlusion queries and generate the list of potentially modifying events
  for(size_t i = 0; i < events.size(); i++)
  {
    if(ignoredEvents.count(events[i].eventId) == 1)
    {
      continue;
    }
    if(events[i].usage == ResourceUsage::Clear || isDirectWrite(events[i].usage))
    {
      PixelModification mod;
      RDCEraseEl(mod);
      mod.eventId = events[i].eventId;
      mod.directShaderWrite = isDirectWrite(events[i].usage);
      history.push_back(mod);

      modEvents.push_back(events[i]);
    }
    else
    {
      uint32_t numSamples;
      driver->glGetQueryObjectuiv(occlusionQueries[i], eGL_QUERY_RESULT, &numSamples);

      if(numSamples > 0)
      {
        PixelModification mod;
        RDCEraseEl(mod);
        mod.eventId = events[i].eventId;
        history.push_back(mod);
        modEvents.push_back(events[i]);
      }
    }
  }

  driver->glDeleteQueries((GLsizei)occlusionQueries.size(), occlusionQueries.data());
  return modEvents;
}

void readPixelValuesMS(WrappedOpenGL *driver, const GLPixelHistoryResources &resources,
                       const CopyFramebuffer &copyFramebuffer, int sampleIdx, int x, int y,
                       rdcarray<PixelModification> &history, int historyIndex, bool readStencil)
{
  rdcarray<float> pixelValue;
  pixelValue.resize(8);
  CopyMSSample(driver, resources, copyFramebuffer, sampleIdx, x, y, pixelValue.data());

  const int depthOffset = 4;
  const int stencilOffset = 5;
  ModificationValue &modValue = history[historyIndex].postMod;

  for(int j = 0; j < 4; ++j)
  {
    modValue.col.floatValue[j] = pixelValue[j];
  }
  modValue.depth = pixelValue[depthOffset];
  if(readStencil)
  {
    modValue.stencil = *(int *)&pixelValue[stencilOffset];
  }
}

struct ScopedReadPixelsSanitiser
{
  PixelUnpackState unpack;
  PixelPackState pack;
  GLuint pixelPackBuffer = 0;
  GLuint pixelUnpackBuffer = 0;

  ScopedReadPixelsSanitiser()
  {
    unpack.Fetch(false);
    pack.Fetch(false);

    GL.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, (GLint *)&pixelPackBuffer);
    GL.glGetIntegerv(eGL_PIXEL_UNPACK_BUFFER_BINDING, (GLint *)&pixelUnpackBuffer);

    ResetPixelPackState(false, 1);
    ResetPixelUnpackState(false, 1);
    GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
    GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);
  }

  ~ScopedReadPixelsSanitiser()
  {
    unpack.Apply(false);
    pack.Apply(false);
    GL.glBindBuffer(eGL_PIXEL_PACK_BUFFER, pixelPackBuffer);
    GL.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, pixelUnpackBuffer);
  }
};

void readPixelValues(WrappedOpenGL *driver, const GLPixelHistoryResources &resources,
                     const CopyFramebuffer &copyFramebuffer, rdcarray<PixelModification> &history,
                     int historyIndex, bool readStencil, uint32_t numPixels, bool isIntegerColour)
{
  ScopedReadPixelsSanitiser scope;

  driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, copyFramebuffer.framebufferId);
  rdcarray<int> intColourValues;
  intColourValues.resize(4 * numPixels);
  rdcarray<float> floatColourValues;
  floatColourValues.resize(4 * numPixels);
  rdcarray<float> depthValues;
  depthValues.resize(numPixels);
  rdcarray<int> stencilValues;
  stencilValues.resize(numPixels);
  if(isIntegerColour)
  {
    driver->glReadPixels(0, 0, GLint(numPixels), 1, eGL_RGBA_INTEGER, eGL_INT,
                         (void *)intColourValues.data());
  }
  else
  {
    driver->glReadPixels(0, 0, GLint(numPixels), 1, eGL_RGBA, eGL_FLOAT,
                         (void *)floatColourValues.data());
    if(IsSRGBFormat(copyFramebuffer.format.colorFormat))
      for(float &f : floatColourValues)
        f = ConvertSRGBToLinear(f);
  }
  if(copyFramebuffer.dsTextureId != 0 || copyFramebuffer.depthTextureId != 0)
  {
    driver->glReadPixels(0, 0, GLint(numPixels), 1, eGL_DEPTH_COMPONENT, eGL_FLOAT,
                         (void *)depthValues.data());
  }

  if(copyFramebuffer.dsTextureId != 0 || copyFramebuffer.stencilTextureId != 0)
  {
    driver->glReadPixels(0, 0, GLint(numPixels), 1, eGL_STENCIL_INDEX, eGL_INT,
                         (void *)stencilValues.data());
  }

  for(size_t i = 0; i < numPixels; i++)
  {
    ModificationValue modValue;

    for(int j = 0; j < 4; ++j)
    {
      if(isIntegerColour)
      {
        modValue.col.intValue[j] = intColourValues[i * 4 + j];
      }
      else
      {
        modValue.col.floatValue[j] = floatColourValues[i * 4 + j];
      }
    }

    modValue.depth = depthValues[i];
    if(readStencil)
    {
      modValue.stencil = stencilValues[i];
    }
    else
    {
      // if this isn't the last fragment in this event, the stencil is unavailable
      if(historyIndex + i + 1 < history.size() &&
         history[historyIndex + i].eventId == history[historyIndex + i + 1].eventId)
        modValue.stencil = -2;
      else
        modValue.stencil = history[historyIndex + i].postMod.stencil;
    }

    history[historyIndex + i].postMod = modValue;
  }
}

void QueryPostModPixelValues(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                             const rdcarray<EventUsage> &modEvents, int x, int y,
                             rdcarray<PixelModification> &history, uint32_t numSamples,
                             uint32_t sampleIndex)
{
  GLMarkerRegion region("QueryPostModPixelValues");
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);
  CopyFramebuffer copyFramebuffer;
  RDCEraseEl(copyFramebuffer);
  copyFramebuffer.framebufferId = ~0u;
  int lastHistoryIdx = 0;

  // get the premod of the first event
  {
    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

    uint32_t colIdx = getFramebufferColIndex(driver, resources.target);

    CopyFramebuffer preModFramebuffer = getCopyFramebuffer(
        driver, colIdx, resources.copyFramebuffers, numSamples, int(modEvents.size()));
    GLenum colourFormatType = getTextureFormatType(preModFramebuffer.format.colorFormat);
    bool integerColour = colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT;

    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, preModFramebuffer.framebufferId);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

    rdcarray<PixelModification> tmp;
    tmp.resize(1);

    GLenum savedReadBuffer;
    driver->glGetIntegerv(eGL_READ_BUFFER, (GLint *)&savedReadBuffer);

    driver->glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
    SafeBlitFramebuffer(x, y, x + 1, y + 1, 0, 0, 1, 1, getFramebufferCopyMask(driver), eGL_NEAREST);
    if(numSamples > 1)
      readPixelValuesMS(driver, resources, preModFramebuffer, sampleIndex, 0, 0, tmp, 0, true);
    else
      readPixelValues(driver, resources, preModFramebuffer, tmp, 0, true, 1, integerColour);

    driver->glReadBuffer(savedReadBuffer);

    history[0].preMod = tmp[0].postMod;

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);
  }

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

    uint32_t colIdx = getFramebufferColIndex(driver, resources.target);

    if(numSamples > 1)
    {
      copyFramebuffer = getCopyFramebuffer(driver, colIdx, resources.copyFramebuffers, numSamples,
                                           int(modEvents.size()));

      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

      GLenum savedReadBuffer;
      driver->glGetIntegerv(eGL_READ_BUFFER, (GLint *)&savedReadBuffer);

      driver->glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
      SafeBlitFramebuffer(x, y, x + 1, y + 1, 0, 0, 1, 1, getFramebufferCopyMask(driver),
                          eGL_NEAREST);
      readPixelValuesMS(driver, resources, copyFramebuffer, sampleIndex, 0, 0, history, int(i), true);

      driver->glReadBuffer(savedReadBuffer);
    }
    else
    {
      CopyFramebuffer newCopyFramebuffer = getCopyFramebuffer(
          driver, colIdx, resources.copyFramebuffers, 1 /*single sampled*/, int(modEvents.size()));
      GLenum colourFormatType = getTextureFormatType(copyFramebuffer.format.colorFormat);
      bool integerColour = colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT;
      if(newCopyFramebuffer.framebufferId != copyFramebuffer.framebufferId)
      {
        if(copyFramebuffer.framebufferId != ~0u)
        {
          readPixelValues(driver, resources, copyFramebuffer, history, lastHistoryIdx, true,
                          (uint32_t)(i - lastHistoryIdx), integerColour);
        }
        lastHistoryIdx = int(i);
      }

      copyFramebuffer = newCopyFramebuffer;
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

      GLenum savedReadBuffer;
      driver->glGetIntegerv(eGL_READ_BUFFER, (GLint *)&savedReadBuffer);

      driver->glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
      SafeBlitFramebuffer(x, y, x + 1, y + 1, GLint(i) - lastHistoryIdx, 0,
                          GLint(i) + 1 - lastHistoryIdx, 1, getFramebufferCopyMask(driver),
                          eGL_NEAREST);

      driver->glReadBuffer(savedReadBuffer);
    }

    // restore the capture's framebuffer
    driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
    driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }

  if(numSamples == 1 && copyFramebuffer.framebufferId != 0u)
  {
    GLenum colourFormatType = getTextureFormatType(copyFramebuffer.format.colorFormat);
    bool integerColour = colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT;
    readPixelValues(driver, resources, copyFramebuffer, history, lastHistoryIdx, true,
                    int(modEvents.size()) - lastHistoryIdx, integerColour);
  }
}

void readShaderOutMS(WrappedOpenGL *driver, const GLPixelHistoryResources &resources,
                     const CopyFramebuffer &copyFramebuffer, int sampleIdx, int x, int y,
                     rdcarray<PixelModification> &history, int historyIndex)
{
  rdcarray<float> pixelValue;
  pixelValue.resize(8);
  CopyMSSample(driver, resources, copyFramebuffer, sampleIdx, x, y, pixelValue.data());

  const int depthOffset = 4;
  const int stencilOffset = 5;
  ModificationValue modValue;

  for(int j = 0; j < 4; ++j)
  {
    modValue.col.floatValue[j] = pixelValue[j];
  }
  modValue.depth = pixelValue[depthOffset];
  modValue.stencil = *(int *)&pixelValue[stencilOffset];
  history[historyIndex].shaderOut = modValue;
}

GLenum getShaderOutColourFormat(GLenum originalColourFormat)
{
  switch(originalColourFormat)
  {
    case eGL_RGBA8I:
    case eGL_RGB8I:
    case eGL_RG8I:
    case eGL_R8I: return eGL_RGBA8I;
    case eGL_RGBA8UI:
    case eGL_RGB8UI:
    case eGL_RG8UI:
    case eGL_R8UI: return eGL_RGBA8UI;
    case eGL_RGBA16I:
    case eGL_RGB16I:
    case eGL_RG16I:
    case eGL_R16I: return eGL_RGBA16I;
    case eGL_RGBA16UI:
    case eGL_RGB16UI:
    case eGL_RG16UI:
    case eGL_R16UI: return eGL_RGBA16UI;
    case eGL_RGBA32I:
    case eGL_RGB32I:
    case eGL_RG32I:
    case eGL_R32I: return eGL_RGBA32I;
    case eGL_RGBA32UI:
    case eGL_RGB32UI:
    case eGL_RG32UI:
    case eGL_R32UI: return eGL_RGBA32UI;
    case eGL_RGB10_A2UI: return eGL_RGB10_A2UI;
    default: RDCERR("Unexpected colour format: %d", originalColourFormat); return eGL_NONE;
  }
}

// This function a) calculates the number of fagments per event
//           and b) calculates the shader output values per event
std::map<uint32_t, uint32_t> QueryNumFragmentsByEvent(
    WrappedOpenGL *driver, GLPixelHistoryResources &resources,
    const rdcarray<EventUsage> &modEvents, rdcarray<PixelModification> &history, int x, int y,
    uint32_t numSamples, uint32_t sampleIndex, uint32_t width, uint32_t height)
{
  GLMarkerRegion region("QueryNumFragmentsByEvent");
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  std::map<uint32_t, uint32_t> eventFragments;

  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    GLRenderState state;
    state.FetchState(driver);

    uint32_t colIdx = getFramebufferColIndex(driver, resources.target);

    GLenum colourFormat = getCurrentTextureFormat(driver, colIdx);
    GLenum colourFormatType = getTextureFormatType(colourFormat);
    GLenum shaderOutColourFormat = eGL_NONE;

    if(colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT)
    {
      shaderOutColourFormat = getShaderOutColourFormat(colourFormat);
      FramebufferKey key = {shaderOutColourFormat, eGL_DEPTH32F_STENCIL8, eGL_DEPTH32F_STENCIL8,
                            numSamples};
      ShaderOutFramebuffer framebuffer =
          getShaderOutFramebuffer(driver, colIdx, resources, key, width, height);
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer.framebufferId);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, framebuffer.framebufferId);
      glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
    }
    else
    {
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
      for(uint32_t a = 0; a < 8; a++)
        driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + a), 0, 0);
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
                                   resources.fullPrecisionColorImage, 0);
      glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
    }

    rdcarray<GLenum> drawBufs;
    drawBufs.resize(colIdx);
    drawBufs.push_back(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
    driver->glDrawBuffers((GLsizei)drawBufs.size(), drawBufs.data());

    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glStencilMask(0xff);    // default for 1 byte
    driver->glStencilFunc(eGL_ALWAYS, 0, 0xff);
    driver->glClearStencil(0);
    driver->glClearColor(0, 0, 0, 0);
    if(driver->isGLESMode())
    {
      driver->glClearDepthf(0.0f);
    }
    else
    {
      driver->glClearDepth(0);
    }
    driver->glClear(eGL_STENCIL_BUFFER_BIT | eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT);
    driver->glEnable(eGL_STENCIL_TEST);
    // depth test enable
    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);
    driver->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // enable the sample we're looking at so we get the shaderOut even if it's masked off
    driver->glEnable(eGL_SAMPLE_MASK);

    driver->glSampleMaski(0, 1u << sampleIndex);

    // replay event
    driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

    uint32_t numFragments = 0;
    if(numSamples == 1)
    {
      ScopedReadPixelsSanitiser scope;

      ModificationValue modValue;
      if(colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT)
      {
        driver->glReadPixels(x, y, 1, 1, eGL_RGBA_INTEGER, eGL_INT,
                             (void *)modValue.col.intValue.data());
      }
      else
      {
        driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)modValue.col.floatValue.data());
      }
      driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
      driver->glReadPixels(x, y, 1, 1, eGL_STENCIL_INDEX, eGL_UNSIGNED_INT, (void *)&numFragments);

      // We're not reading the stencil value here, so use the postMod instead.
      // Shaders don't actually output stencil values, those are determined by the stencil op.
      modValue.stencil = history[i].postMod.stencil;

      history[i].shaderOut = modValue;
    }
    else
    {
      GLenum copyFramebufferColourFormat =
          (colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT)
              ? shaderOutColourFormat
              : eGL_RGBA32F;

      const CopyFramebuffer &copyFramebuffer = getCopyFramebuffer(
          driver, resources.copyFramebuffers, numSamples, int(modEvents.size()),
          eGL_DEPTH32F_STENCIL8, eGL_DEPTH32F_STENCIL8, copyFramebufferColourFormat);
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
      glReadBuffer(eGL_COLOR_ATTACHMENT0);
      SafeBlitFramebuffer(x, y, x + 1, y + 1, 0, 0, 1, 1, getFramebufferCopyMask(driver),
                          eGL_NEAREST);
      readShaderOutMS(driver, resources, copyFramebuffer, sampleIndex, 0, 0, history, int(i));
      numFragments = history[i].shaderOut.stencil;
      history[i].shaderOut.stencil = history[i].postMod.stencil;
    }

    eventFragments.emplace(modEvents[i].eventId, numFragments);

    state.ApplyState(driver);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }

  return eventFragments;
}

bool QueryScissorTest(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                      const EventUsage event, int x, int y)
{
  driver->ReplayLog(0, event.eventId, eReplay_WithoutDraw);
  bool scissorTestFailed = false;
  if(driver->glIsEnabled(eGL_SCISSOR_TEST))
  {
    int scissorBox[4];    // [x, y, width, height]
    driver->glGetIntegerv(eGL_SCISSOR_BOX, scissorBox);
    scissorTestFailed = x < scissorBox[0] || x - scissorBox[0] >= scissorBox[2] ||
                        y < scissorBox[1] || y - scissorBox[1] >= scissorBox[3];
  }
  return scissorTestFailed;
}

bool QueryTest(WrappedOpenGL *driver, GLPixelHistoryResources &resources, const EventUsage event,
               int x, int y, OpenGLTest test, uint32_t sampleIndex)
{
  driver->ReplayLog(0, event.eventId, eReplay_WithoutDraw);
  GLuint samplesPassedQuery;
  driver->glGenQueries(1, &samplesPassedQuery);
  driver->glEnable(eGL_SCISSOR_TEST);
  driver->glScissor(x, y, 1, 1);
  if(test < OpenGLTest::DepthTest)
  {
    driver->glDisable(eGL_DEPTH_TEST);
  }
  if(test < OpenGLTest::StencilTest)
  {
    driver->glDisable(eGL_STENCIL_TEST);
  }
  if(test < OpenGLTest::FaceCulling)
  {
    driver->glDisable(eGL_CULL_FACE);
  }
  if(test < OpenGLTest::SampleMask)
  {
    driver->glDisable(eGL_SAMPLE_MASK);
  }
  else if(test == OpenGLTest::SampleMask)
  {
    GLboolean sampleMaskEnabled = driver->glIsEnabled(eGL_SAMPLE_MASK);
    if(sampleMaskEnabled)
    {
      uint32_t currentSampleMask;
      driver->glGetIntegeri_v(eGL_SAMPLE_MASK_VALUE, 0, (GLint *)&currentSampleMask);
      uint32_t newSampleMask = currentSampleMask & (sampleIndex == ~0u ? ~0u : (1u << sampleIndex));
      driver->glSampleMaski(0, newSampleMask);
    }
    else
    {
      driver->glEnable(eGL_SAMPLE_MASK);
      driver->glSampleMaski(0, 1u << sampleIndex);
    }
  }

  driver->SetFetchCounters(true);
  driver->glBeginQuery(eGL_ANY_SAMPLES_PASSED, samplesPassedQuery);
  driver->ReplayLog(event.eventId, event.eventId, eReplay_OnlyDraw);
  driver->glEndQuery(eGL_ANY_SAMPLES_PASSED);
  driver->SetFetchCounters(false);
  int anySamplesPassed = GL_FALSE;
  driver->glGetQueryObjectiv(samplesPassedQuery, eGL_QUERY_RESULT, &anySamplesPassed);
  driver->glDeleteQueries(1, &samplesPassedQuery);
  return anySamplesPassed == GL_FALSE;
}

void QueryFailedTests(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                      const rdcarray<EventUsage> &modEvents, int x, int y,
                      rdcarray<PixelModification> &history, uint32_t sampleIndex)
{
  GLMarkerRegion region("QueryFailedTests");
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    OpenGLTest failedTest = OpenGLTest::NumTests;
    if(!isDirectWrite(modEvents[i].usage))
    {
      if(modEvents[i].usage == ResourceUsage::Clear)
      {
        bool failed = QueryScissorTest(driver, resources, modEvents[i], x, y);
        if(failed)
        {
          failedTest = OpenGLTest::ScissorTest;
        }
      }
      else
      {
        for(int test = 0; test < int(OpenGLTest::NumTests); ++test)
        {
          bool failed;
          if(test == int(OpenGLTest::ScissorTest))
          {
            failed = QueryScissorTest(driver, resources, modEvents[i], x, y);
          }
          else
          {
            failed = QueryTest(driver, resources, modEvents[i], x, y, OpenGLTest(test), sampleIndex);
          }
          if(failed)
          {
            failedTest = OpenGLTest(test);
            break;
          }
        }
      }
    }

    // ensure that history objects are one-to-one mapped with modEvent objects
    RDCASSERT(history[i].eventId == modEvents[i].eventId);
    history[i].scissorClipped = failedTest == OpenGLTest::ScissorTest;
    history[i].stencilTestFailed = failedTest == OpenGLTest::StencilTest;
    history[i].depthTestFailed = failedTest == OpenGLTest::DepthTest;
    history[i].backfaceCulled = failedTest == OpenGLTest::FaceCulling;
    history[i].sampleMasked = failedTest == OpenGLTest::SampleMask;
  }
}

void QueryShaderOutPerFragment(WrappedOpenGL *driver, GLReplay *replay,
                               GLPixelHistoryResources &resources,
                               const rdcarray<EventUsage> &modEvents, int x, int y,
                               rdcarray<PixelModification> &history,
                               const std::map<uint32_t, uint32_t> &eventFragments,
                               uint32_t numSamples, uint32_t sampleIndex, uint32_t width,
                               uint32_t height)
{
  GLMarkerRegion region("QueryShaderOutPerFragment");
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

    if(numFragments <= 1)
    {
      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }
      continue;
    }

    GLRenderState state;
    state.FetchState(driver);

    driver->glEnable(eGL_SCISSOR_TEST);
    driver->glScissor(x, y, 1, 1);

    driver->glClearStencil(0);
    driver->glClearColor(0, 0, 0, 0);
    driver->glClearDepth(0);
    driver->glClearStencil(0);

    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);
    driver->glEnable(eGL_SAMPLE_MASK);
    if(sampleIndex != ~0u)
    {
      driver->glSampleMaski(0, 1u << sampleIndex);
    }
    else
    {
      driver->glSampleMaski(0, ~0u);
    }

    driver->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    driver->glStencilMask(0xff);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glEnable(eGL_STENCIL_TEST);

    // enable the sample we're looking at so we get the shaderOut even if it's masked off
    if(sampleIndex != ~0u)
    {
      driver->glSampleMaski(0, 1u << sampleIndex);
    }

    PixelModification referenceHistory;
    referenceHistory.eventId = modEvents[i].eventId;

    auto historyIndex =
        std::lower_bound(history.begin(), history.end(), referenceHistory,
                         [](const PixelModification &h1, const PixelModification &h2) -> bool {
                           return h1.eventId < h2.eventId;
                         });

    RDCASSERT(historyIndex != history.end());

    uint32_t colIdx = getFramebufferColIndex(driver, resources.target);

    GLenum colourFormat = getCurrentTextureFormat(driver, colIdx);
    GLenum colourFormatType = getTextureFormatType(colourFormat);
    GLenum shaderOutColourFormat = eGL_NONE;

    for(size_t j = 0; j < RDCMAX(numFragments, 1u); ++j)
    {
      //  Set the stencil function so only jth fragment will pass.
      driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);

      if(colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT)
      {
        shaderOutColourFormat = getShaderOutColourFormat(colourFormat);
        FramebufferKey key = {shaderOutColourFormat, eGL_DEPTH32F_STENCIL8, eGL_DEPTH32F_STENCIL8,
                              numSamples};
        ShaderOutFramebuffer framebuffer =
            getShaderOutFramebuffer(driver, colIdx, resources, key, width, height);
        driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer.framebufferId);
        driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, framebuffer.framebufferId);
        glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
      }
      else
      {
        driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
        driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
        for(uint32_t a = 0; a < 8; a++)
          driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + a), 0, 0);
        driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
                                     resources.fullPrecisionColorImage, 0);
        glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
      }

      rdcarray<GLenum> drawBufs;
      drawBufs.resize(colIdx);
      drawBufs.push_back(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
      driver->glDrawBuffers((GLsizei)drawBufs.size(), drawBufs.data());

      driver->glClear(eGL_STENCIL_BUFFER_BIT | eGL_COLOR_BUFFER_BIT | eGL_DEPTH_BUFFER_BIT);

      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

      if(numSamples == 1)
      {
        ScopedReadPixelsSanitiser scope;

        ModificationValue modValue;

        if(colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT)
        {
          driver->glReadPixels(x, y, 1, 1, eGL_RGBA_INTEGER, eGL_INT,
                               (void *)modValue.col.intValue.data());
        }
        else
        {
          driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT,
                               (void *)modValue.col.floatValue.data());
        }
        driver->glReadPixels(x, y, 1, 1, eGL_DEPTH_COMPONENT, eGL_FLOAT, (void *)&modValue.depth);
        modValue.stencil = historyIndex->shaderOut.stencil;

        historyIndex->shaderOut = modValue;
      }
      else
      {
        GLenum copyFramebufferColourFormat =
            (colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT)
                ? shaderOutColourFormat
                : eGL_RGBA32F;

        const CopyFramebuffer &copyFramebuffer = getCopyFramebuffer(
            driver, resources.copyFramebuffers, numSamples, int(modEvents.size()),
            eGL_DEPTH32F_STENCIL8, eGL_DEPTH32F_STENCIL8, copyFramebufferColourFormat);
        driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
        glReadBuffer(eGL_COLOR_ATTACHMENT0);
        SafeBlitFramebuffer(x, y, x + 1, y + 1, 0, 0, 1, 1, getFramebufferCopyMask(driver),
                            eGL_NEAREST);
        int oldStencil = historyIndex->shaderOut.stencil;
        readShaderOutMS(driver, resources, copyFramebuffer, sampleIndex, 0, 0, history,
                        int(historyIndex - history.begin()));
        historyIndex->shaderOut.stencil = oldStencil;
      }
      historyIndex++;
    }

    state.ApplyState(driver);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

void QueryPostModPerFragment(WrappedOpenGL *driver, GLReplay *replay,
                             GLPixelHistoryResources &resources,
                             const rdcarray<EventUsage> &modEvents, int x, int y,
                             rdcarray<PixelModification> &history,
                             const std::map<uint32_t, uint32_t> &eventFragments, uint32_t numSamples,
                             uint32_t sampleIndex, uint32_t width, uint32_t height)
{
  GLMarkerRegion region("QueryPostModPerFragment");
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

    if(numFragments <= 1)
    {
      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }
      continue;
    }

    GLRenderState state;
    state.FetchState(driver);

    driver->glEnable(eGL_SCISSOR_TEST);
    driver->glScissor(x, y, 1, 1);

    driver->glClearStencil(0);

    driver->glStencilMask(0xff);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glEnable(eGL_STENCIL_TEST);
    driver->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    PixelModification referenceHistory;
    referenceHistory.eventId = modEvents[i].eventId;

    auto historyIndex =
        std::lower_bound(history.begin(), history.end(), referenceHistory,
                         [](const PixelModification &h1, const PixelModification &h2) -> bool {
                           return h1.eventId < h2.eventId;
                         });
    RDCASSERT(historyIndex != history.end());

    GLint savedReadFramebuffer, savedDrawFramebuffer;
    driver->glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, &savedDrawFramebuffer);
    driver->glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

    uint32_t colIdx = getFramebufferColIndex(driver, resources.target);

    GLenum colourFormat = getCurrentTextureFormat(driver, colIdx);
    GLenum colourFormatType = getTextureFormatType(colourFormat);
    bool integerColour = colourFormatType == eGL_UNSIGNED_INT || colourFormatType == eGL_INT;

    CopyFramebuffer copyFramebuffer;
    RDCEraseEl(copyFramebuffer);
    copyFramebuffer.framebufferId = ~0u;
    int lastJ = 0;

    // save D/S attachment since we'll substitute our own depth
    GLuint curDepth;
    GLint curDepthType;
    GLuint curStencil;
    GLint curStencilType;
    GLint curStencilLevel = 0, curDepthLevel = 0;

    driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                  (GLint *)&curDepth);
    driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                  &curDepthType);
    if(curDepth)
    {
      driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                                    eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
                                                    &curDepthLevel);
    }

    driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                  (GLint *)&curStencil);
    driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                  &curStencilType);
    if(curStencil)
    {
      driver->glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                                    eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
                                                    &curStencilLevel);
    }

    GLuint forcedDepth = 0;
    // replace depth and clear it to premod value
    {
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
      driver->glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, forcedDepth,
                                   0);

      GLenum textureTarget = numSamples > 1 ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;
      driver->glGenTextures(1, &forcedDepth);
      driver->CreateTextureImage(forcedDepth, eGL_DEPTH32F_STENCIL8, eGL_NONE, eGL_NONE,
                                 textureTarget, 2, width, height, 1, numSamples, 1);
      driver->glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, forcedDepth,
                                   0);

      driver->glClearDepth(historyIndex->preMod.depth);
      driver->glClear(eGL_DEPTH_BUFFER_BIT);
    }

    GLuint curColor = 0;
    GLint colorType = 0;
    driver->glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
        eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curColor);

    driver->glGetFramebufferAttachmentParameteriv(
        eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + colIdx),
        eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &colorType);

    GLenum colorFormat = eGL_NONE;
    if(curColor != 0)
    {
      ResourceId id;
      if(colorType != eGL_RENDERBUFFER)
      {
        id = driver->GetResourceManager()->GetResID(TextureRes(driver->GetCtx(), curColor));
      }
      else
      {
        id = driver->GetResourceManager()->GetResID(RenderbufferRes(driver->GetCtx(), curColor));
      }
      colorFormat = driver->m_Textures[id].internalFormat;
    }

    for(size_t j = 0; j < std::max(numFragments, 1u); ++j)
    {
      // Set the stencil function so only jth fragment will pass.
      driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
      driver->glClear(eGL_STENCIL_BUFFER_BIT);

      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

      if(numSamples > 1)
      {
        copyFramebuffer = getCopyFramebuffer(driver, resources.copyFramebuffers, numSamples,
                                             int(modEvents.size()), eGL_DEPTH32F_STENCIL8,
                                             eGL_DEPTH32F_STENCIL8, colorFormat);

        driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
        driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

        GLenum savedReadBuffer;
        driver->glGetIntegerv(eGL_READ_BUFFER, (GLint *)&savedReadBuffer);

        driver->glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
        SafeBlitFramebuffer(x, y, x + 1, y + 1, 0, 0, 1, 1, getFramebufferCopyMask(driver),
                            eGL_NEAREST);

        readPixelValuesMS(driver, resources, copyFramebuffer, sampleIndex, 0, 0, history,
                          int(historyIndex - history.begin()), false);
        historyIndex++;

        driver->glReadBuffer(savedReadBuffer);
      }
      else
      {
        // Blit the values into out framebuffer
        CopyFramebuffer newCopyFramebuffer =
            getCopyFramebuffer(driver, resources.copyFramebuffers, numSamples, int(modEvents.size()),
                               eGL_DEPTH32F_STENCIL8, eGL_DEPTH32F_STENCIL8, colorFormat);
        if(newCopyFramebuffer.framebufferId != copyFramebuffer.framebufferId ||
           (j - lastJ >= copyFramebuffer.width))
        {
          if(copyFramebuffer.framebufferId != ~0u)
          {
            readPixelValues(driver, resources, copyFramebuffer, history,
                            lastJ + int(historyIndex - history.begin()), false,
                            (uint32_t)(j - lastJ), integerColour);
          }
          lastJ = int(j);
        }
        copyFramebuffer = newCopyFramebuffer;
        driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
        driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedDrawFramebuffer);

        GLenum savedReadBuffer;
        driver->glGetIntegerv(eGL_READ_BUFFER, (GLint *)&savedReadBuffer);

        driver->glReadBuffer(GLenum(eGL_COLOR_ATTACHMENT0 + colIdx));
        SafeBlitFramebuffer(x, y, x + 1, y + 1, GLint(j) - lastJ, 0, GLint(j) + 1 - lastJ, 1,
                            getFramebufferCopyMask(driver), eGL_NEAREST);

        driver->glReadBuffer(savedReadBuffer);
      }

      // restore the capture's framebuffer
      driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, savedDrawFramebuffer);
      driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, savedReadFramebuffer);
    }

    // restore original D/S attachments
    if(curDepthType != eGL_RENDERBUFFER)
    {
      driver->glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, curDepth,
                                   curDepthLevel);
    }
    else
    {
      driver->glFramebufferRenderbuffer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT,
                                        eGL_RENDERBUFFER, curDepth);
    }
    if(curStencilType != eGL_RENDERBUFFER)
    {
      driver->glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, curStencil,
                                   curStencilLevel);
    }
    else
    {
      driver->glFramebufferRenderbuffer(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT,
                                        eGL_RENDERBUFFER, curStencil);
    }

    driver->glDeleteTextures(1, &forcedDepth);

    if(numSamples == 1 && copyFramebuffer.framebufferId != ~0u)
    {
      readPixelValues(driver, resources, copyFramebuffer, history,
                      lastJ + int(historyIndex - history.begin()), false, numFragments - lastJ,
                      integerColour);
    }

    state.ApplyState(driver);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

void QueryPrimitiveIdPerFragment(WrappedOpenGL *driver, GLReplay *replay,
                                 GLPixelHistoryResources &resources,
                                 const rdcarray<EventUsage> &modEvents, int x, int y,
                                 rdcarray<PixelModification> &history,
                                 const std::map<uint32_t, uint32_t> &eventFragments,
                                 bool usingFloatForPrimitiveId, uint32_t numSamples,
                                 uint32_t sampleIndex)
{
  GLMarkerRegion region("QueryPrimitiveIdPerFragment");
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);

  for(size_t i = 0; i < modEvents.size(); i++)
  {
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;

    if(numFragments == 0)
    {
      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }
      continue;
    }

    GLRenderState state;
    state.FetchState(driver);

    driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
    driver->glReadBuffer(eGL_COLOR_ATTACHMENT0);
    // rebind colour image to 0
    for(uint32_t a = 1; a < 8; a++)
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + a), 0, 0);
    driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                 resources.fullPrecisionColorImage, 0);
    driver->glDrawBuffer(eGL_COLOR_ATTACHMENT0);

    driver->glEnable(eGL_SCISSOR_TEST);
    driver->glScissor(x, y, 1, 1);

    driver->glClearStencil(0);
    driver->glEnable(eGL_DEPTH_TEST);
    driver->glDepthFunc(eGL_ALWAYS);
    driver->glDepthMask(GL_TRUE);
    driver->glDisable(eGL_BLEND);
    driver->glDisable(eGL_SAMPLE_MASK);
    driver->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    driver->glStencilMask(0xff);
    driver->glStencilOp(eGL_INCR, eGL_INCR, eGL_INCR);
    driver->glEnable(eGL_STENCIL_TEST);

    PixelModification referenceHistory;
    referenceHistory.eventId = modEvents[i].eventId;

    auto historyIndex =
        std::lower_bound(history.begin(), history.end(), referenceHistory,
                         [](const PixelModification &h1, const PixelModification &h2) -> bool {
                           return h1.eventId < h2.eventId;
                         });
    RDCASSERT(historyIndex != history.end());

    // we expect this value to be overwritten by the primitive id shader. if not
    // it will cause an assertion that all color values are the same to fail
    driver->glClearColor(0.84f, 0.17f, 0.2f, 0.49f);
    driver->glClear(eGL_COLOR_BUFFER_BIT);

    GLint currentProgram = 0;
    driver->glGetIntegerv(eGL_CURRENT_PROGRAM, &currentProgram);

    driver->glUseProgram(GetPrimitiveIdProgram(driver, replay, resources, currentProgram));

    driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);

    // rebind output colour to 0
    for(uint32_t a = 1; a < 8; a++)
      driver->glFramebufferTexture(eGL_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0 + a), 0, 0);
    driver->glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0,
                                 resources.fullPrecisionColorImage, 0);
    driver->glDrawBuffer(eGL_COLOR_ATTACHMENT0);
    driver->glReadBuffer(eGL_COLOR_ATTACHMENT0);

    for(size_t j = 0; j < std::max(numFragments, 1u); ++j)
    {
      ModificationValue modValue;
      modValue.stencil = 0;
      // Set the stencil function so only jth fragment will pass.
      driver->glBindFramebuffer(eGL_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
      driver->glStencilFunc(eGL_EQUAL, (int)j, 0xff);
      driver->glClear(eGL_STENCIL_BUFFER_BIT);

      driver->ReplayLog(modEvents[i].eventId, modEvents[i].eventId, eReplay_OnlyDraw);

      // Primitive id array is given size 8 for the multisampled case
      float primitiveIds[8];
      if(numSamples == 1)
      {
        ScopedReadPixelsSanitiser scope;

        driver->glReadPixels(x, y, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)primitiveIds);
      }
      else
      {
        const CopyFramebuffer &copyFramebuffer =
            getCopyFramebuffer(driver, resources.copyFramebuffers, numSamples, int(modEvents.size()),
                               eGL_DEPTH32F_STENCIL8, eGL_DEPTH32F_STENCIL8, eGL_RGBA32F);
        driver->glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, copyFramebuffer.framebufferId);
        driver->glBindFramebuffer(eGL_READ_FRAMEBUFFER, resources.fullPrecisionFrameBuffer);
        glReadBuffer(eGL_COLOR_ATTACHMENT0);
        SafeBlitFramebuffer(x, y, x + 1, y + 1, 0, 0, 1, 1, getFramebufferCopyMask(driver),
                            eGL_NEAREST);
        CopyMSSample(driver, resources, copyFramebuffer, sampleIndex, 0, 0, primitiveIds);
      }

      if(primitiveIds[0] == primitiveIds[1] && primitiveIds[0] == primitiveIds[2] &&
         primitiveIds[0] == primitiveIds[3])
      {
        if(usingFloatForPrimitiveId)
        {
          historyIndex->primitiveID = int(primitiveIds[0]);
        }
        else
        {
          historyIndex->primitiveID = *(int *)primitiveIds;
        }
      }
      else
      {
        historyIndex->primitiveID = ~0u;
        RDCERR("Primitive ID was not written correctly");
      }

      ++historyIndex;
    }
    driver->glUseProgram(currentProgram);

    state.ApplyState(driver);

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId + 1, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

bool depthTestPassed(int depthFunc, float shaderOutputDepth, float depthInBuffer)
{
  switch(depthFunc)
  {
    case eGL_NEVER: return false;
    case eGL_LESS: return shaderOutputDepth < depthInBuffer;
    case eGL_EQUAL: return shaderOutputDepth == depthInBuffer;
    case eGL_LEQUAL: return shaderOutputDepth <= depthInBuffer;
    case eGL_GREATER: return shaderOutputDepth > depthInBuffer;
    case eGL_NOTEQUAL: return shaderOutputDepth != depthInBuffer;
    case eGL_GEQUAL: return shaderOutputDepth >= depthInBuffer;
    case eGL_ALWAYS: return true;

    default: RDCERR("Unexpected depth function: %d", depthFunc);
  }
  return false;
}

void CalculateFragmentDepthTests(WrappedOpenGL *driver, GLPixelHistoryResources &resources,
                                 const rdcarray<EventUsage> &modEvents,
                                 rdcarray<PixelModification> &history,
                                 const std::map<uint32_t, uint32_t> &eventFragments)
{
  GLMarkerRegion region("CalculateFragmentDepthTests");
  driver->ReplayLog(0, modEvents[0].eventId, eReplay_WithoutDraw);
  size_t historyIndex = 0;
  for(size_t i = 0; i < modEvents.size(); ++i)
  {
    // We only need to calculate the depth test for events with multiple fragments.
    auto it = eventFragments.find(modEvents[i].eventId);
    uint32_t numFragments = (it != eventFragments.end()) ? it->second : 0;
    if(numFragments <= 1)
    {
      // keep the historyIndex in sync with the event index
      while(historyIndex < history.size() && modEvents[i].eventId == history[historyIndex].eventId)
      {
        ++historyIndex;
      }

      if(i < modEvents.size() - 1)
      {
        driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
      }

      continue;
    }

    GLboolean depthTestEnabled = driver->glIsEnabled(eGL_DEPTH_TEST);
    // default for no depth test
    int depthFunc = eGL_ALWAYS;
    if(depthTestEnabled)
      driver->glGetIntegerv(eGL_DEPTH_FUNC, &depthFunc);
    for(; historyIndex < history.size() && modEvents[i].eventId == history[historyIndex].eventId;
        ++historyIndex)
    {
      if(historyIndex == 0)
      {
        continue;
      }

      history[historyIndex].depthTestFailed = !depthTestPassed(
          depthFunc, history[historyIndex].shaderOut.depth, history[historyIndex].preMod.depth);
    }

    if(i < modEvents.size() - 1)
    {
      driver->ReplayLog(modEvents[i].eventId, modEvents[i + 1].eventId, eReplay_WithoutDraw);
    }
  }
}

};    // end of anonymous namespace

rdcarray<PixelModification> GLReplay::PixelHistory(rdcarray<EventUsage> events, ResourceId target,
                                                   uint32_t x, uint32_t y, const Subresource &sub,
                                                   CompType typeCast)
{
  rdcarray<PixelModification> history;

  if(events.empty())
    return history;

  TextureDescription textureDesc = GetTexture(target);
  if(textureDesc.format.type == ResourceFormatType::Undefined)
    return history;

  // When RenderDoc passed y, the value being passed in is with the Y axis starting from the top
  // However, we need to have it starting from the bottom, so flip it by subtracting y from the
  // height.
  uint32_t flippedY = (textureDesc.height >> sub.mip) - y - 1;

  rdcstr regionName = StringFormat::Fmt(
      "PixelHistory: pixel: (%u, %u) on %s subresource (%u, %u, %u) cast to %s with %zu events", x,
      flippedY, ToStr(target).c_str(), sub.mip, sub.slice, sub.sample, ToStr(typeCast).c_str(),
      events.size());

  uint32_t sampleIdx = sub.sample;

  if(sampleIdx > textureDesc.msSamp)
  {
    sampleIdx = 0;    // if the sample index is out of range (ie. average value) then default to 0
  }

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
  {
    sampleMask = 1U << sampleIdx;
  }

  GLPixelHistoryResources resources;

  resources.target = target;

  MakeCurrentReplayContext(&m_ReplayCtx);

  m_pDriver->ReplayMarkers(false);

  GLMarkerRegion region(regionName);

  int glslVersion = DebugData.glslVersion;
  bool usingFloatForPrimitiveId = glslVersion < 330;

  PixelHistorySetupResources(m_pDriver, resources, textureDesc, sub, (uint32_t)events.size(),
                             glslVersion, textureDesc.msSamp);

  rdcarray<EventUsage> modEvents =
      QueryModifyingEvents(m_pDriver, resources, events, x, flippedY, sub.mip, history);

  if(modEvents.empty())
  {
    PixelHistoryDestroyResources(m_pDriver, resources);
    m_pDriver->ReplayMarkers(true);
    return history;
  }

  QueryFailedTests(m_pDriver, resources, modEvents, x, flippedY, history, sampleIdx);
  QueryPostModPixelValues(m_pDriver, resources, modEvents, x, flippedY, history, textureDesc.msSamp,
                          sampleIdx);

  uint32_t textureWidth = textureDesc.width >> sub.mip;
  uint32_t textureHeight = textureDesc.height >> sub.mip;
  std::map<uint32_t, uint32_t> eventFragments =
      QueryNumFragmentsByEvent(m_pDriver, resources, modEvents, history, x, flippedY,
                               textureDesc.msSamp, sampleIdx, textureWidth, textureHeight);

  // copy history entries to create one history per fragment
  for(size_t h = 0; h < history.size();)
  {
    uint32_t frags = 1;
    auto it = eventFragments.find(history[h].eventId);
    if(it != eventFragments.end())
    {
      frags = it->second;
    }
    for(uint32_t f = 1; f < frags; ++f)
    {
      history.insert(h + 1, history[h]);
    }
    for(uint32_t f = 0; f < frags; ++f)
    {
      history[h + f].fragIndex = f;
    }
    h += RDCMAX(1u, frags);
  }

  QueryShaderOutPerFragment(m_pDriver, this, resources, modEvents, x, flippedY, history,
                            eventFragments, textureDesc.msSamp, sampleIdx, textureWidth,
                            textureHeight);
  QueryPrimitiveIdPerFragment(m_pDriver, this, resources, modEvents, x, flippedY, history,
                              eventFragments, usingFloatForPrimitiveId, textureDesc.msSamp,
                              sampleIdx);
  // copy the postMod depth to next history's preMod depth
  // (preMode depth is to prime the depth buffer in QueryPostModPerFragment)
  for(size_t i = 1; i < history.size(); ++i)
  {
    history[i].preMod.depth = history[i - 1].postMod.depth;
  }
  QueryPostModPerFragment(m_pDriver, this, resources, modEvents, x, flippedY, history,
                          eventFragments, textureDesc.msSamp, sampleIdx, textureWidth, textureHeight);

  // copy the postMod to next history's preMod
  for(size_t i = 1; i < history.size(); ++i)
  {
    history[i].preMod = history[i - 1].postMod;
  }

  CalculateFragmentDepthTests(m_pDriver, resources, modEvents, history, eventFragments);

  PixelHistoryDestroyResources(m_pDriver, resources);
  m_pDriver->ReplayMarkers(true);
  return history;
}
