/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "maths/matrix.h"
#include "gl_driver.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

void GLReplay::CopyTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                                  GLint arraySize, GLint samples, GLenum intFormat)
{
  WrappedOpenGL &drv = *m_pDriver;

  // create temporary texture array, which we'll initialise to be the width/height in same format,
  // with the same number of array slices as multi samples.
  drv.glGenTextures(1, &destArray);
  drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, destArray);

  bool failed = false;

  if(!failed && !HasExt[ARB_compute_shader])
  {
    RDCWARN(
        "Can't copy multisampled texture to array for serialisation without ARB_compute_shader.");
    failed = true;
  }

  if(!failed && !HasExt[ARB_texture_view])
  {
    RDCWARN("Can't copy multisampled texture to array for serialisation without ARB_texture_view.");
    failed = true;
  }

  if(!failed && !HasExt[ARB_texture_storage])
  {
    RDCWARN(
        "Can't copy multisampled texture to array for serialisation without ARB_texture_view, and "
        "ARB_texture_view requires ARB_texture_storage.");
    failed = true;
  }

  if(failed)
  {
    // create using the non-storage API which is always available, so the texture is at least valid
    // (but with undefined/empty contents).
    drv.glTextureImage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 0, intFormat, width, height,
                            arraySize * samples, 0, GetBaseFormat(intFormat),
                            GetDataType(intFormat), NULL);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    return;
  }

  // initialise the texture using texture storage, as required for texture views.
  drv.glTextureStorage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 1, intFormat, width, height,
                            arraySize * samples);

  if(IsDepthStencilFormat(intFormat))
  {
    CopyDepthTex2DMSToArray(destArray, srcMS, width, height, arraySize, samples, intFormat);
    return;
  }

  GLMarkerRegion renderoverlay("CopyTex2DMSToArray");

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  GLenum viewClass;
  drv.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS,
                            sizeof(GLenum), (GLint *)&viewClass);

  GLenum fmt = eGL_R32UI;
  if(viewClass == eGL_VIEW_CLASS_8_BITS)
    fmt = eGL_R8UI;
  else if(viewClass == eGL_VIEW_CLASS_16_BITS)
    fmt = eGL_R16UI;
  else if(viewClass == eGL_VIEW_CLASS_24_BITS)
    fmt = eGL_RGB8UI;
  else if(viewClass == eGL_VIEW_CLASS_32_BITS)
    fmt = eGL_RGBA8UI;
  else if(viewClass == eGL_VIEW_CLASS_48_BITS)
    fmt = eGL_RGB16UI;
  else if(viewClass == eGL_VIEW_CLASS_64_BITS)
    fmt = eGL_RG32UI;
  else if(viewClass == eGL_VIEW_CLASS_96_BITS)
    fmt = eGL_RGB32UI;
  else if(viewClass == eGL_VIEW_CLASS_128_BITS)
    fmt = eGL_RGBA32UI;

  GLuint texs[2];
  drv.glGenTextures(2, texs);
  drv.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, fmt, 0, 1, 0, arraySize * samples);
  drv.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, fmt, 0, 1, 0, arraySize);

  drv.glBindImageTexture(2, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
  drv.glActiveTexture(eGL_TEXTURE0);
  drv.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  drv.glUseProgram(DebugData.MS2Array);

  GLint loc = drv.glGetUniformLocation(DebugData.MS2Array, "mscopy");
  if(loc >= 0)
  {
    drv.glProgramUniform4i(DebugData.MS2Array, loc, samples, 0, 0, 0);

    drv.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize * samples));
  }
  drv.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  drv.glDeleteTextures(2, texs);

  rs.ApplyState(m_pDriver);
}

void GLReplay::CopyDepthTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                                       GLint arraySize, GLint samples, GLenum intFormat)
{
  GLMarkerRegion renderoverlay("CopyDepthTex2DMSToArray");

  WrappedOpenGL &drv = *m_pDriver;

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  GLuint texs[3];
  drv.glGenTextures(3, texs);
  drv.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, intFormat, 0, 1, 0,
                    arraySize * samples);
  drv.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, intFormat, 0, 1, 0, arraySize);
  drv.glTextureView(texs[2], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, intFormat, 0, 1, 0, arraySize);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  GLuint fbo = 0;
  drv.glGenFramebuffers(1, &fbo);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);
  drv.glDrawBuffers(0, NULL);

  drv.glUseProgram(DebugData.DepthMS2Array);
  drv.glViewport(0, 0, width, height);

  drv.glDisable(eGL_CULL_FACE);
  drv.glDisable(eGL_BLEND);
  drv.glDisable(eGL_SCISSOR_TEST);
  if(!IsGLES)
    drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
  drv.glEnable(eGL_DEPTH_TEST);
  drv.glEnable(eGL_STENCIL_TEST);
  drv.glDepthFunc(eGL_ALWAYS);
  drv.glDepthMask(GL_TRUE);
  drv.glStencilOp(eGL_REPLACE, eGL_REPLACE, eGL_REPLACE);
  drv.glStencilMask(0xff);

  uint32_t numStencil = 1;
  GLenum attach = eGL_DEPTH_ATTACHMENT;

  switch(GetBaseFormat(intFormat))
  {
    case eGL_DEPTH_STENCIL:
      numStencil = 256;
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
      break;
    case eGL_DEPTH:
      numStencil = 1;
      attach = eGL_DEPTH_ATTACHMENT;
      break;
    case eGL_STENCIL:
      numStencil = 256;
      attach = eGL_STENCIL_ATTACHMENT;
      break;
    default: RDCERR("Unexpected base format! %s", ToStr(intFormat).c_str()); break;
  }

  if(attach == eGL_DEPTH_STENCIL_ATTACHMENT || attach == eGL_DEPTH_ATTACHMENT)
  {
    // depth aspect
    drv.glActiveTexture(eGL_TEXTURE0);
    drv.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                        eGL_DEPTH_COMPONENT);
  }

  if(numStencil > 1)
  {
    // stencil aspect
    drv.glActiveTexture(eGL_TEXTURE1);
    drv.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[2]);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                        eGL_STENCIL_INDEX);
  }

  GLint loc = drv.glGetUniformLocation(DebugData.DepthMS2Array, "mscopy");
  if(loc >= 0)
  {
    for(GLint i = 0; i < arraySize * samples; i++)
    {
      drv.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texs[0], 0, i);

      for(uint32_t s = 0; s < numStencil; s++)
      {
        uint32_t currentStencil = numStencil == 1 ? 1000 : s;

        drv.glStencilFunc(eGL_ALWAYS, int(s), 0xff);

        drv.glProgramUniform4i(DebugData.DepthMS2Array, loc, samples, i % samples, i / samples,
                               currentStencil);

        drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      }
    }
  }

  drv.glDeleteFramebuffers(1, &fbo);
  drv.glDeleteTextures(3, texs);

  rs.ApplyState(m_pDriver);
}

void GLReplay::CopyArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width, GLint height,
                                  GLint arraySize, GLint samples, GLenum intFormat,
                                  uint32_t selectedSlice)
{
  WrappedOpenGL &drv = *m_pDriver;

  if(!HasExt[ARB_compute_shader])
  {
    RDCWARN(
        "Can't copy array to multisampled texture for serialisation without ARB_compute_shader.");
    return;
  }

  if(!HasExt[ARB_texture_view])
  {
    RDCWARN("Can't copy array to multisampled texture for serialisation without ARB_texture_view.");
    return;
  }

  if(!HasExt[ARB_texture_storage])
  {
    RDCWARN(
        "Can't copy array to multisampled texture for serialisation without ARB_texture_view, and "
        "ARB_texture_view requires ARB_texture_storage.");
    return;
  }

  if(IsDepthStencilFormat(intFormat))
  {
    CopyDepthArrayToTex2DMS(destMS, srcArray, width, height, arraySize, samples, intFormat,
                            selectedSlice);
    return;
  }

  GLMarkerRegion renderoverlay("CopyArrayToTex2DMS");

  bool singleSliceMode = (selectedSlice != ~0U);

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  GLenum viewClass;
  drv.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS,
                            sizeof(GLenum), (GLint *)&viewClass);

  GLenum fmt = eGL_R32UI;
  if(viewClass == eGL_VIEW_CLASS_8_BITS)
    fmt = eGL_R8UI;
  else if(viewClass == eGL_VIEW_CLASS_16_BITS)
    fmt = eGL_R16UI;
  else if(viewClass == eGL_VIEW_CLASS_24_BITS)
    fmt = eGL_RGB8UI;
  else if(viewClass == eGL_VIEW_CLASS_32_BITS)
    fmt = eGL_RGBA8UI;
  else if(viewClass == eGL_VIEW_CLASS_48_BITS)
    fmt = eGL_RGB16UI;
  else if(viewClass == eGL_VIEW_CLASS_64_BITS)
    fmt = eGL_RG32UI;
  else if(viewClass == eGL_VIEW_CLASS_96_BITS)
    fmt = eGL_RGB32UI;
  else if(viewClass == eGL_VIEW_CLASS_128_BITS)
    fmt = eGL_RGBA32UI;

  GLuint texs[2];
  drv.glGenTextures(2, texs);
  drv.glTextureView(texs[0], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, destMS, fmt, 0, 1, 0, arraySize);
  drv.glTextureView(texs[1], eGL_TEXTURE_2D_ARRAY, srcArray, fmt, 0, 1, 0, arraySize * samples);

  drv.glBindImageTexture(2, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
  drv.glActiveTexture(eGL_TEXTURE0);
  drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, texs[1]);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  drv.glUseProgram(DebugData.Array2MS);

  GLint loc = drv.glGetUniformLocation(DebugData.Array2MS, "mscopy");
  if(loc >= 0)
  {
    if(singleSliceMode)
    {
      GLint sampleOffset = (selectedSlice % samples);
      GLint sliceOffset = (selectedSlice / samples);
      drv.glProgramUniform4i(DebugData.Array2MS, loc, samples, sampleOffset, sliceOffset, 0);

      drv.glDispatchCompute((GLuint)width, (GLuint)height, 1);
    }
    else
    {
      drv.glProgramUniform4i(DebugData.Array2MS, loc, samples, 0, 0, 0);

      drv.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize * samples));
    }
  }
  drv.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  drv.glDeleteTextures(2, texs);

  rs.ApplyState(m_pDriver);
}

void GLReplay::CopyDepthArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width, GLint height,
                                       GLint arraySize, GLint samples, GLenum intFormat,
                                       uint32_t selectedSlice)
{
  GLMarkerRegion renderoverlay("CopyDepthArrayToTex2DMS");

  bool singleSliceMode = (selectedSlice != ~0U);

  WrappedOpenGL &drv = *m_pDriver;

  GLRenderState rs;
  rs.FetchState(m_pDriver);

  GLuint texs[3];
  drv.glGenTextures(3, texs);
  drv.glTextureView(texs[0], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, destMS, intFormat, 0, 1, 0, arraySize);
  drv.glTextureView(texs[1], eGL_TEXTURE_2D_ARRAY, srcArray, intFormat, 0, 1, 0, arraySize * samples);
  drv.glTextureView(texs[2], eGL_TEXTURE_2D_ARRAY, srcArray, intFormat, 0, 1, 0, arraySize * samples);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  drv.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  GLuint fbo = 0;
  drv.glGenFramebuffers(1, &fbo);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);
  drv.glDrawBuffers(0, NULL);

  drv.glUseProgram(DebugData.DepthArray2MS);
  drv.glViewport(0, 0, width, height);

  drv.glDisable(eGL_CULL_FACE);
  drv.glDisable(eGL_BLEND);
  drv.glDisable(eGL_SCISSOR_TEST);
  if(!IsGLES)
    drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
  drv.glEnable(eGL_DEPTH_TEST);
  drv.glEnable(eGL_STENCIL_TEST);
  drv.glDepthFunc(eGL_ALWAYS);
  drv.glDepthMask(GL_TRUE);
  drv.glEnable(eGL_SAMPLE_SHADING);
  drv.glEnable(eGL_SAMPLE_MASK);
  drv.glStencilOp(eGL_REPLACE, eGL_REPLACE, eGL_REPLACE);
  drv.glStencilMask(0xff);

  uint32_t numStencil = 1;
  GLenum attach = eGL_DEPTH_ATTACHMENT;

  switch(GetBaseFormat(intFormat))
  {
    case eGL_DEPTH_STENCIL:
      numStencil = 256;
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
      break;
    case eGL_DEPTH:
      numStencil = 1;
      attach = eGL_DEPTH_ATTACHMENT;
      break;
    case eGL_STENCIL:
      numStencil = 256;
      attach = eGL_STENCIL_ATTACHMENT;
      break;
    default: RDCERR("Unexpected base format! %s", ToStr(intFormat).c_str()); break;
  }

  if(attach == eGL_DEPTH_STENCIL_ATTACHMENT || attach == eGL_DEPTH_ATTACHMENT)
  {
    // depth aspect
    drv.glActiveTexture(eGL_TEXTURE0);
    drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, texs[1]);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_DEPTH_STENCIL_TEXTURE_MODE, eGL_DEPTH_COMPONENT);
  }

  if(numStencil > 1)
  {
    // stencil aspect
    drv.glActiveTexture(eGL_TEXTURE1);
    drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, texs[2]);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_DEPTH_STENCIL_TEXTURE_MODE, eGL_STENCIL_INDEX);
  }

  GLint loc = drv.glGetUniformLocation(DebugData.DepthArray2MS, "mscopy");
  if(loc >= 0)
  {
    for(GLint i = 0; i < arraySize; i++)
    {
      if(singleSliceMode)
      {
        i = selectedSlice / samples;
        drv.glSampleMaski(0, 1U << (selectedSlice % samples));
      }

      drv.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texs[0], 0, i);

      for(uint32_t s = 0; s < numStencil; s++)
      {
        uint32_t currentStencil = numStencil == 1 ? 1000 : s;

        drv.glStencilFunc(eGL_ALWAYS, int(s), 0xff);

        drv.glProgramUniform4i(DebugData.DepthArray2MS, loc, samples, 0, i, currentStencil);

        drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      }

      if(singleSliceMode)
        break;
    }
  }

  drv.glDeleteFramebuffers(1, &fbo);
  drv.glDeleteTextures(3, texs);

  rs.ApplyState(m_pDriver);
}
