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

#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

bool GLReplay::RenderTexture(TextureDisplay cfg)
{
  return RenderTextureInternal(cfg, TexDisplayFlags(eTexDisplay_BlendAlpha | eTexDisplay_MipShift));
}

bool GLReplay::RenderTextureInternal(TextureDisplay cfg, TexDisplayFlags flags)
{
  const bool blendAlpha = (flags & eTexDisplay_BlendAlpha) != 0;
  const bool mipShift = (flags & eTexDisplay_MipShift) != 0;

  WrappedOpenGL &drv = *m_pDriver;

  auto &texDetails = m_pDriver->m_Textures[cfg.resourceId];

  if(texDetails.internalFormat == eGL_NONE)
    return false;

  CacheTexture(cfg.resourceId);

  bool renderbuffer = false;

  int intIdx = 0;

  int resType;
  switch(texDetails.curType)
  {
    case eGL_RENDERBUFFER:
      resType = RESTYPE_TEX2D;
      if(texDetails.samples > 1)
        resType = RESTYPE_TEX2DMS;
      renderbuffer = true;
      break;
    case eGL_TEXTURE_1D: resType = RESTYPE_TEX1D; break;
    default: RDCWARN("Unexpected texture type"); DELIBERATE_FALLTHROUGH();
    case eGL_TEXTURE_2D: resType = RESTYPE_TEX2D; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: resType = RESTYPE_TEX2DMS; break;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: resType = RESTYPE_TEX2DMSARRAY; break;
    case eGL_TEXTURE_RECTANGLE: resType = RESTYPE_TEXRECT; break;
    case eGL_TEXTURE_BUFFER: resType = RESTYPE_TEXBUFFER; break;
    case eGL_TEXTURE_3D: resType = RESTYPE_TEX3D; break;
    case eGL_TEXTURE_CUBE_MAP: resType = RESTYPE_TEXCUBE; break;
    case eGL_TEXTURE_1D_ARRAY: resType = RESTYPE_TEX1DARRAY; break;
    case eGL_TEXTURE_2D_ARRAY: resType = RESTYPE_TEX2DARRAY; break;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: resType = RESTYPE_TEXCUBEARRAY; break;
  }

  GLuint texname = texDetails.resource.name;
  GLenum target = texDetails.curType;

  // do blit from renderbuffer to texture, then sample from texture
  if(renderbuffer)
  {
    // need replay context active to do blit (as FBOs aren't shared)
    MakeCurrentReplayContext(&m_ReplayCtx);

    GLMarkerRegion blitRegion("Renderbuffer Blit");

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    drv.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    drv.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

    SafeBlitFramebuffer(
        0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width, texDetails.height,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

    texname = texDetails.renderbufferReadTex;
    if(resType == RESTYPE_TEX2D)
      target = eGL_TEXTURE_2D;
    else
      target = eGL_TEXTURE_2D_MULTISAMPLE;
  }

  MakeCurrentReplayContext(m_DebugCtx);

  GLMarkerRegion region("RenderTextureInternal");

  uint32_t numMips = m_CachedTextures[cfg.resourceId].mips;

  GLuint castTexture = 0;

  GLenum displayFormat = texDetails.internalFormat;

  if(cfg.typeCast != CompType::Typeless &&
     cfg.typeCast != MakeResourceFormat(target, displayFormat).compType)
  {
    displayFormat = GetViewCastedFormat(displayFormat, cfg.typeCast);

    // if the format didn't change we can't re-interpret this format anyway
    if(displayFormat != texDetails.internalFormat)
    {
      GLMarkerRegion castRegion("Casting texture for view");

      drv.glGenTextures(1, &castTexture);
      drv.glActiveTexture(eGL_TEXTURE0);
      drv.glBindTexture(target, castTexture);

      // can't use texture views because the underlying image isn't immutable because we don't
      // want to rely on texture storage. We also can't rely on texture views, meaning we need a
      // fallback path ANYWAY, so might as well always use it. Yayyy opengl...
      drv.CreateTextureImage(castTexture, displayFormat, eGL_NONE, eGL_NONE, target,
                             texDetails.dimension, texDetails.width, texDetails.height,
                             texDetails.depth, texDetails.samples, (int)numMips);

      bool isCompressed = IsCompressedFormat(displayFormat);

      GLint values[4] = {};
      float fvalues[4] = {};

      // ensure source texture is complete
      GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, values);

      int prevMaxLevel = values[0];

      int maxlevel = numMips - 1;
      GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

      // copy state

      if(!texDetails.emulated && (HasExt[ARB_texture_swizzle] || HasExt[EXT_texture_swizzle]))
      {
        GetTextureSwizzle(texname, target, (GLenum *)values);
        SetTextureSwizzle(castTexture, target, (GLenum *)values);
      }

      if((target == eGL_TEXTURE_CUBE_MAP || target == eGL_TEXTURE_CUBE_MAP_ARRAY) &&
         HasExt[ARB_seamless_cubemap_per_texture])
      {
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_CUBE_MAP_SEAMLESS, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_CUBE_MAP_SEAMLESS, values);
      }

      if(target != eGL_TEXTURE_2D_MULTISAMPLE && target != eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      {
        if(HasExt[EXT_texture_sRGB_decode])
        {
          GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_SRGB_DECODE_EXT, values);
          GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_SRGB_DECODE_EXT, values);
        }

        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_COMPARE_FUNC, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_COMPARE_FUNC, values);
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_COMPARE_MODE, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_COMPARE_MODE, values);
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MIN_FILTER, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_MIN_FILTER, values);
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAG_FILTER, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_MAG_FILTER, values);
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_R, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_WRAP_R, values);
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_S, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_WRAP_S, values);
        GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_T, values);
        GL.glTextureParameterivEXT(castTexture, target, eGL_TEXTURE_WRAP_T, values);

        if(HasExt[ARB_texture_border_clamp])
        {
          GL.glGetTextureParameterfvEXT(texname, target, eGL_TEXTURE_BORDER_COLOR, fvalues);
          GL.glTextureParameterfvEXT(castTexture, target, eGL_TEXTURE_BORDER_COLOR, fvalues);
        }

        if(!IsGLES)
        {
          GL.glGetTextureParameterfvEXT(texname, target, eGL_TEXTURE_LOD_BIAS, fvalues);
          GL.glTextureParameterfvEXT(castTexture, target, eGL_TEXTURE_LOD_BIAS, fvalues);
        }

        if(target != eGL_TEXTURE_RECTANGLE)
        {
          GL.glGetTextureParameterfvEXT(texname, target, eGL_TEXTURE_MIN_LOD, fvalues);
          GL.glTextureParameterfvEXT(castTexture, target, eGL_TEXTURE_MIN_LOD, fvalues);
          GL.glGetTextureParameterfvEXT(texname, target, eGL_TEXTURE_MAX_LOD, fvalues);
          GL.glTextureParameterfvEXT(castTexture, target, eGL_TEXTURE_MAX_LOD, fvalues);
        }
      }

      // copy mip data
      for(uint32_t i = 0; i < numMips; i++)
      {
        int w = RDCMAX(texDetails.width >> i, 1);
        int h = RDCMAX(texDetails.height >> i, 1);
        int d = RDCMAX(texDetails.depth >> i, 1);

        if(target == eGL_TEXTURE_CUBE_MAP)
          d *= 6;
        else if(target == eGL_TEXTURE_CUBE_MAP_ARRAY || target == eGL_TEXTURE_2D_ARRAY)
          d = texDetails.depth;

        // glCopyImageSubData treats 1D arrays sanely - with depth as array size - but at odds
        // with the rest of the API.
        if(target == eGL_TEXTURE_1D_ARRAY)
        {
          h = 1;
          d = texDetails.height;
        }

        if((isCompressed && VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] && (w < 4 || h < 4)) ||
           (isCompressed && VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] &&
            texDetails.curType == eGL_TEXTURE_CUBE_MAP) ||
           (isCompressed && IsGLES))
        {
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

          for(int trg = 0; trg < count; trg++)
          {
            size_t size = GetCompressedByteSize(w, h, d, displayFormat);

            if(target == eGL_TEXTURE_CUBE_MAP)
              size /= 6;

            byte *buf = new byte[size];

            if(IsGLES)
            {
              texDetails.GetCompressedImageDataGLES(i, targets[trg], size, buf);
            }
            else
            {
              // read to CPU
              GL.glGetCompressedTextureImageEXT(texname, targets[trg], i, buf);
            }

            // write to GPU
            if(texDetails.dimension == 1)
              GL.glCompressedTextureSubImage1DEXT(castTexture, targets[trg], i, 0, w, displayFormat,
                                                  (GLsizei)size, buf);
            else if(texDetails.dimension == 2)
              GL.glCompressedTextureSubImage2DEXT(castTexture, targets[trg], i, 0, 0, w, h,
                                                  displayFormat, (GLsizei)size, buf);
            else if(texDetails.dimension == 3)
              GL.glCompressedTextureSubImage3DEXT(castTexture, targets[trg], i, 0, 0, 0, w, h, d,
                                                  displayFormat, (GLsizei)size, buf);

            delete[] buf;
          }
        }
        else
        {
          GL.glCopyImageSubData(texname, target, i, 0, 0, 0, castTexture, target, i, 0, 0, 0, w, h,
                                d);
        }
      }

      GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&prevMaxLevel);

      texname = castTexture;
    }
  }

  RDCGLenum dsTexMode = eGL_NONE;
  if(IsDepthStencilFormat(displayFormat))
  {
    // stencil-only, make sure we display it as such
    if(displayFormat == eGL_STENCIL_INDEX8)
    {
      cfg.red = false;
      cfg.green = true;
      cfg.blue = false;
      cfg.alpha = false;
    }

    // depth-only, make sure we display it as such
    if(GetBaseFormat(displayFormat) == eGL_DEPTH_COMPONENT)
    {
      cfg.red = true;
      cfg.green = false;
      cfg.blue = false;
      cfg.alpha = false;
    }

    if(!cfg.red && cfg.green)
    {
      dsTexMode = eGL_STENCIL_INDEX;

      // Stencil texture sampling is not normalized in OpenGL
      intIdx = 1;
      float rangeScale = 1.0f;
      switch(displayFormat)
      {
        case eGL_STENCIL_INDEX1: rangeScale = 1.0f; break;
        case eGL_STENCIL_INDEX4: rangeScale = 16.0f; break;
        default:
          RDCWARN("Unexpected raw format for stencil visualization");
          DELIBERATE_FALLTHROUGH();
        case eGL_DEPTH24_STENCIL8:
        case eGL_DEPTH32F_STENCIL8:
        case eGL_DEPTH_STENCIL:
        case eGL_STENCIL_INDEX8: rangeScale = 255.0f; break;
        case eGL_STENCIL_INDEX16: rangeScale = 65535.0f; break;
      }
      cfg.rangeMin *= rangeScale;
      cfg.rangeMax *= rangeScale;

      if(displayFormat == eGL_STENCIL_INDEX8)
        cfg.red = true;
    }
    else
    {
      dsTexMode = eGL_DEPTH_COMPONENT;
    }
  }
  else
  {
    if(IsUIntFormat(displayFormat))
      intIdx = 1;
    if(IsSIntFormat(displayFormat))
      intIdx = 2;
  }

  drv.glBindProgramPipeline(0);
  if(flags & eTexDisplay_RemapFloat)
    drv.glUseProgram(DebugData.texRemapProg[0]);
  else if(flags & eTexDisplay_RemapUInt)
    drv.glUseProgram(DebugData.texRemapProg[1]);
  else if(flags & eTexDisplay_RemapSInt)
    drv.glUseProgram(DebugData.texRemapProg[2]);
  else
    drv.glUseProgram(DebugData.texDisplayProg[intIdx]);

  GLuint customProgram = 0;

  if(cfg.customShaderId != ResourceId() &&
     drv.GetResourceManager()->HasCurrentResource(cfg.customShaderId))
  {
    GLuint customShader = drv.GetResourceManager()->GetCurrentResource(cfg.customShaderId).name;

    customProgram = drv.glCreateProgram();

    drv.glAttachShader(customProgram, DebugData.texDisplayVertexShader);
    drv.glAttachShader(customProgram, customShader);

    drv.glLinkProgram(customProgram);

    drv.glDetachShader(customProgram, DebugData.texDisplayVertexShader);
    drv.glDetachShader(customProgram, customShader);

    char buffer[1024] = {};
    GLint status = 0;
    drv.glGetProgramiv(customProgram, eGL_LINK_STATUS, &status);
    if(status == 0)
    {
      drv.glGetProgramInfoLog(customProgram, 1024, NULL, buffer);
      RDCERR("Error linking custom shader program: %s", buffer);

      drv.glDeleteProgram(customProgram);
      customProgram = 0;
    }

    if(customProgram)
    {
      drv.glUseProgram(customProgram);

      GLint loc = -1;

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_TexDim");
      if(loc >= 0)
        drv.glProgramUniform4ui(customProgram, loc, texDetails.width, texDetails.height,
                                texDetails.depth, (uint32_t)numMips);

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_SelectedMip");
      if(loc >= 0)
        drv.glProgramUniform1ui(customProgram, loc, cfg.subresource.mip);

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_SelectedSliceFace");
      if(loc >= 0)
        drv.glProgramUniform1ui(customProgram, loc, cfg.subresource.slice);

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_SelectedSample");
      if(loc >= 0)
      {
        if(cfg.subresource.sample == ~0U)
          drv.glProgramUniform1i(customProgram, loc, -texDetails.samples);
        else
          drv.glProgramUniform1i(
              customProgram, loc,
              (int)RDCCLAMP(cfg.subresource.sample, 0U, (uint32_t)texDetails.samples - 1));
      }

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_TextureType");
      if(loc >= 0)
        drv.glProgramUniform1ui(customProgram, loc, resType);

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_SelectedRangeMin");
      if(loc >= 0)
        drv.glProgramUniform1f(customProgram, loc, cfg.rangeMin);

      loc = drv.glGetUniformLocation(customProgram, "RENDERDOC_SelectedRangeMax");
      if(loc >= 0)
        drv.glProgramUniform1f(customProgram, loc, cfg.rangeMax);
    }
  }

  // bind a dummy texbuffer - some drivers (macOS) have trouble when a buffer isn't bound.
  if(resType != RESTYPE_TEXBUFFER && DebugData.dummyTexBuffer)
  {
    drv.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + RESTYPE_TEXBUFFER));
    drv.glBindTexture(eGL_TEXTURE_BUFFER, DebugData.dummyTexBuffer);
  }

  drv.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + resType));
  drv.glBindTexture(target, texname);

  GLint origDSTexMode = eGL_DEPTH_COMPONENT;
  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
  {
    drv.glGetTextureParameterivEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &origDSTexMode);
    drv.glTextureParameteriEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, dsTexMode);
  }

  // defined as arrays mostly for Coverity code analysis to stay calm about passing
  // them to the *TexParameter* functions
  GLint baseLevel[4] = {-1};
  GLint maxlevel[4] = {-1};
  GLint forcedparam[4] = {};

  bool levelsTex = (target != eGL_TEXTURE_BUFFER && target != eGL_TEXTURE_2D_MULTISAMPLE &&
                    target != eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

  if(levelsTex)
  {
    drv.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, baseLevel);
    drv.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);
  }
  else
  {
    baseLevel[0] = maxlevel[0] = -1;
  }

  // ensure texture is mipmap complete and we can view all mips (if the range has been reduced) by
  // forcing TEXTURE_MAX_LEVEL to cover all valid mips.
  if(levelsTex && cfg.resourceId != DebugData.CustomShaderTexID)
  {
    forcedparam[0] = 0;
    drv.glTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, forcedparam);
    forcedparam[0] = GLint(numMips - 1);
    drv.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, forcedparam);
  }
  else
  {
    maxlevel[0] = -1;
  }

  bool intTexture = intIdx > 0 || (flags & eTexDisplay_RemapUInt) || (flags & eTexDisplay_RemapSInt);

  GLint tex_x = texDetails.width, tex_y = texDetails.height, tex_z = texDetails.depth;

  drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  RDCCOMPILE_ASSERT(sizeof(RD_CustomShader_UBO_Type) <= sizeof(TexDisplayUBOData),
                    "Custom shader UBO is bigger than tex display UBO, map is not valid");
  TexDisplayUBOData *ubo =
      (TexDisplayUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(TexDisplayUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  if(!ubo)
  {
    RDCERR("Map buffer failed %d", drv.glGetError());
    return false;
  }

  float x = cfg.xOffset;
  float y = cfg.yOffset;

  ubo->Position.x = x;
  ubo->Position.y = y;
  ubo->Scale = cfg.scale;

  if(cfg.scale <= 0.0f)
  {
    float xscale = DebugData.outWidth / float(tex_x);
    float yscale = DebugData.outHeight / float(tex_y);

    ubo->Scale = cfg.scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      ubo->Position.x = 0;
      ubo->Position.y = (DebugData.outHeight - (tex_y * ubo->Scale)) * 0.5f;
    }
    else
    {
      ubo->Position.y = 0;
      ubo->Position.x = (DebugData.outWidth - (tex_x * ubo->Scale)) * 0.5f;
    }
  }

  ubo->HDRMul = cfg.hdrMultiplier;

  ubo->FlipY = cfg.flipY ? 1 : 0;

  if(cfg.rangeMax <= cfg.rangeMin)
    cfg.rangeMax += 0.00001f;

  if(dsTexMode == eGL_NONE)
  {
    ubo->Channels.x = cfg.red ? 1.0f : 0.0f;
    ubo->Channels.y = cfg.green ? 1.0f : 0.0f;
    ubo->Channels.z = cfg.blue ? 1.0f : 0.0f;
    ubo->Channels.w = cfg.alpha ? 1.0f : 0.0f;
  }
  else
  {
    // Both depth and stencil texture mode use the red channel
    ubo->Channels.x = 1.0f;
    ubo->Channels.y = 0.0f;
    ubo->Channels.z = 0.0f;
    ubo->Channels.w = 0.0f;
  }

  ubo->RangeMinimum = cfg.rangeMin;
  ubo->InverseRangeSize = 1.0f / (cfg.rangeMax - cfg.rangeMin);

  TextureSamplerMode mode = TextureSamplerMode::Point;

  if(cfg.subresource.mip == 0 && cfg.scale < 1.0f && dsTexMode == eGL_NONE &&
     resType != RESTYPE_TEXBUFFER && resType != RESTYPE_TEXRECT && !intTexture)
  {
    mode = TextureSamplerMode::Linear;
  }
  else
  {
    if(resType == RESTYPE_TEXRECT || resType == RESTYPE_TEX2DMS ||
       resType == RESTYPE_TEX2DMSARRAY || resType == RESTYPE_TEXBUFFER)
      mode = TextureSamplerMode::PointNoMip;
    else
      mode = TextureSamplerMode::Point;
  }

  ubo->MipLevel = (int)cfg.subresource.mip;
  if(texDetails.curType != eGL_TEXTURE_3D)
  {
    uint32_t numSlices = RDCMAX((uint32_t)texDetails.depth, 1U);
    if(texDetails.curType == eGL_TEXTURE_CUBE_MAP)
      numSlices *= 6;

    if(texDetails.curType == eGL_TEXTURE_1D_ARRAY)
      numSlices = RDCMAX((uint32_t)texDetails.height, 1U);

    uint32_t sliceFace = RDCCLAMP(cfg.subresource.slice, 0U, numSlices - 1);
    ubo->Slice = (float)sliceFace + 0.001f;
  }
  else
  {
    float slice =
        (float)RDCCLAMP(cfg.subresource.slice, 0U, RDCMAX((uint32_t)texDetails.depth, 1U) - 1);

    // when sampling linearly, we need to add half a pixel to ensure we only sample the desired
    // slice
    if(mode == TextureSamplerMode::Linear)
      slice += 0.5f;
    else
      slice += 0.001f;

    ubo->Slice = slice;
  }

  ubo->OutputDisplayFormat = resType;

  if(cfg.overlay == DebugOverlay::NaN)
    ubo->OutputDisplayFormat |= TEXDISPLAY_NANS;

  if(cfg.overlay == DebugOverlay::Clipping)
    ubo->OutputDisplayFormat |= TEXDISPLAY_CLIPPING;

  if(!IsSRGBFormat(displayFormat) && cfg.linearDisplayAsGamma)
    ubo->OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

  ubo->RawOutput = cfg.rawOutput ? 1 : 0;

  ubo->TextureResolutionPS.x = float(RDCMAX(1, tex_x >> cfg.subresource.mip));
  ubo->TextureResolutionPS.y = float(RDCMAX(1, tex_y >> cfg.subresource.mip));
  ubo->TextureResolutionPS.z = float(RDCMAX(1, tex_z >> cfg.subresource.mip));

  if(mipShift)
    ubo->MipShift = float(1 << cfg.subresource.mip);
  else
    ubo->MipShift = 1.0f;

  ubo->OutputRes.x = DebugData.outWidth;
  ubo->OutputRes.y = DebugData.outHeight;

  ubo->SampleIdx = (int)RDCCLAMP(cfg.subresource.sample, 0U, (uint32_t)texDetails.samples - 1);

  // hacky resolve
  if(cfg.subresource.sample == ~0U)
    ubo->SampleIdx = -texDetails.samples;

  ubo->DecodeYUV = 0;
  ubo->YUVDownsampleRate = {};
  ubo->YUVAChannels = {};

  if(customProgram)
  {
    // overwrite with data for custom UBO
    RD_CustomShader_UBO_Type customUBO = {};

    customUBO.TexDim.x = texDetails.width;
    customUBO.TexDim.y = texDetails.height;
    customUBO.TexDim.z = texDetails.depth;
    customUBO.TexDim.w = (uint32_t)numMips;
    customUBO.SelectedMip = cfg.subresource.mip;
    customUBO.TextureType = resType;
    customUBO.SelectedSliceFace = cfg.subresource.slice;
    if(cfg.subresource.sample == ~0U)
      customUBO.SelectedSample = -texDetails.samples;
    else
      customUBO.SelectedSample = cfg.subresource.sample;
    customUBO.YUVDownsampleRate = Vec4u(1, 1, 1, 8);
    customUBO.YUVAChannels = Vec4u(0, 1, 2, 3);
    customUBO.SelectedRange = Vec2f(cfg.rangeMin, cfg.rangeMax);

    memcpy(ubo, &customUBO, sizeof(RD_CustomShader_UBO_Type));
  }

  drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  TextureSamplerState prevSampState = SetSamplerParams(target, texname, mode);

  HeatmapData heatmapData = {};

  {
    if(cfg.overlay == DebugOverlay::QuadOverdrawDraw || cfg.overlay == DebugOverlay::QuadOverdrawPass)
    {
      heatmapData.HeatmapMode = HEATMAP_LINEAR;
    }
    else if(cfg.overlay == DebugOverlay::TriangleSizeDraw ||
            cfg.overlay == DebugOverlay::TriangleSizePass)
    {
      heatmapData.HeatmapMode = HEATMAP_TRISIZE;
    }

    if(heatmapData.HeatmapMode)
    {
      memcpy(heatmapData.ColorRamp, colorRamp, sizeof(colorRamp));

      RDCCOMPILE_ASSERT(sizeof(heatmapData.ColorRamp) == sizeof(colorRamp),
                        "C++ color ramp array is not the same size as the shader array");
    }
  }

  drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, DebugData.UBOs[1]);

  {
    HeatmapData *ptr = (HeatmapData *)drv.glMapBufferRange(
        eGL_UNIFORM_BUFFER, 0, sizeof(HeatmapData), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if(!ptr)
    {
      RDCERR("Map buffer failed %d", drv.glGetError());
      return false;
    }

    memcpy(ptr, &heatmapData, sizeof(heatmapData));

    drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);
  }

  if(cfg.rawOutput || !blendAlpha || cfg.customShaderId != ResourceId())
  {
    drv.glDisable(eGL_BLEND);
  }
  else
  {
    drv.glEnable(eGL_BLEND);
    drv.glBlendFunc(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA);
  }

  drv.glDisable(eGL_DEPTH_TEST);

  if(HasExt[EXT_framebuffer_sRGB])
    drv.glEnable(eGL_FRAMEBUFFER_SRGB);

  drv.glBindVertexArray(DebugData.emptyVAO);
  drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

  if(baseLevel[0] >= 0)
    drv.glTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, baseLevel);

  if(maxlevel[0] >= 0)
    drv.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);

  RestoreSamplerParams(target, texname, prevSampState);

  if(customProgram)
  {
    drv.glUseProgram(0);
    drv.glDeleteProgram(customProgram);
  }

  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
    drv.glTextureParameteriEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, origDSTexMode);

  if(castTexture)
    drv.glDeleteTextures(1, &castTexture);

  return true;
}
