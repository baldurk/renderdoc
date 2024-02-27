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

#include "data/glsl_shaders.h"
#include "maths/matrix.h"
#include "gl_driver.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

void WrappedOpenGL::ArrayMSPrograms::Create()
{
  rdcstr cs, vs, fs;

  ShaderType shaderType;
  int glslVersion;
  int glslBaseVer;
  int glslCSVer;    // compute shader

  GetGLSLVersions(shaderType, glslVersion, glslBaseVer, glslCSVer);

  if(HasExt[ARB_compute_shader] && HasExt[ARB_shader_image_load_store] &&
     HasExt[ARB_texture_multisample])
  {
    cs = GenerateGLSLShader(GetEmbeddedResource(glsl_ms2array_comp), shaderType, glslCSVer);
    MS2Array = CreateCShaderProgram(cs);

    // GLES doesn't have multisampled image load/store even with any extension
    Array2MS = 0;
    if(!IsGLES)
    {
      cs = GenerateGLSLShader(GetEmbeddedResource(glsl_array2ms_comp), shaderType, glslCSVer);
      Array2MS = CreateCShaderProgram(cs);
    }
  }
  else
  {
    MS2Array = 0;
    Array2MS = 0;
    RDCWARN(
        "GL_ARB_compute_shader or ARB_shader_image_load_store or ARB_texture_multisample not "
        "supported, disabling 2DMS save/load.");
  }

  DepthArray2MS = DepthMS2Array = 0;

  if(HasExt[ARB_texture_multisample] && HasExt[ARB_sample_shading])
  {
    GLuint prevProg = 0;
    GL.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prevProg);

    vs = GenerateGLSLShader(GetEmbeddedResource(glsl_blit_vert), shaderType, glslBaseVer);

    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_depthms2arr_frag), shaderType, glslBaseVer);
    DepthMS2Array = CreateShaderProgram(vs, fs);

    GL.glUseProgram(DepthMS2Array);

    GL.glUniform1i(GL.glGetUniformLocation(DepthMS2Array, "srcDepthMS"), 0);
    GL.glUniform1i(GL.glGetUniformLocation(DepthMS2Array, "srcStencilMS"), 1);

    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_deptharr2ms_frag), shaderType, glslBaseVer);
    DepthArray2MS = CreateShaderProgram(vs, fs);

    GL.glUseProgram(DepthArray2MS);

    GL.glUniform1i(GL.glGetUniformLocation(DepthArray2MS, "srcDepthArray"), 0);
    GL.glUniform1i(GL.glGetUniformLocation(DepthArray2MS, "srcStencilArray"), 1);

    GL.glUseProgram(prevProg);
  }
  else
  {
    MS2Array = 0;
    Array2MS = 0;
    RDCWARN(
        "GL_ARB_texture_multisample or GL_ARB_sample_shading not supported, disabling 2DMS "
        "depth-stencil save/load.");
  }
}

void WrappedOpenGL::ArrayMSPrograms::Destroy()
{
  if(MS2Array)
    GL.glDeleteProgram(MS2Array);
  if(Array2MS)
    GL.glDeleteProgram(Array2MS);
  if(DepthMS2Array)
    GL.glDeleteProgram(DepthMS2Array);
  if(DepthArray2MS)
    GL.glDeleteProgram(DepthArray2MS);
}

void WrappedOpenGL::CopyTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                                       GLint arraySize, GLint samples, GLenum intFormat)
{
  const ArrayMSPrograms &arrms = GetArrayMS();

  intFormat = GetSizedFormat(intFormat);

  bool needInit = false;

  // create temporary texture array, which we'll initialise to be the width/height in same format,
  // with the same number of array slices as multi samples.
  if(destArray == 0)
  {
    GL.glGenTextures(1, &destArray);
    GL.glBindTexture(eGL_TEXTURE_2D_ARRAY, destArray);

    needInit = true;
  }

  bool failed = false;

  if(!HasExt[ARB_compute_shader])
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

  if(!arrms.MS2Array || (IsDepthStencilFormat(intFormat) && !arrms.DepthMS2Array))
  {
    failed = true;
  }

  if(failed)
  {
    // create using the non-storage API which is always available, so the texture is at least valid
    // (but with undefined/empty contents).
    if(needInit)
    {
      GL.glTextureImage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 0, intFormat, width, height,
                             arraySize * samples, 0, GetBaseFormat(intFormat),
                             GetDataType(intFormat), NULL);
      GL.glTextureParameteriEXT(destArray, eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    }
    return;
  }

  // initialise the texture using texture storage, as required for texture views.
  if(needInit)
    GL.glTextureStorage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 1, intFormat, width, height,
                             arraySize * samples);

  if(IsDepthStencilFormat(intFormat))
  {
    CopyDepthTex2DMSToArray(destArray, srcMS, width, height, arraySize, samples, intFormat);
    return;
  }

  GLMarkerRegion renderoverlay("CopyTex2DMSToArray");

  GLRenderState rs;
  rs.FetchState(this);

  GLenum viewClass;
  GL.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS, 1,
                           (GLint *)&viewClass);

  GLenum fmt = eGL_R32UI;
  if(viewClass == eGL_VIEW_CLASS_8_BITS)
    fmt = eGL_R8UI;
  else if(viewClass == eGL_VIEW_CLASS_16_BITS)
    fmt = eGL_R16UI;
  else if(viewClass == eGL_VIEW_CLASS_24_BITS)
    fmt = eGL_RGB8UI;
  else if(viewClass == eGL_VIEW_CLASS_32_BITS)
    fmt = eGL_RGBA8UI;
  else if(viewClass == eGL_VIEW_CLASS_64_BITS)
    fmt = eGL_RG32UI;
  else if(viewClass == eGL_VIEW_CLASS_128_BITS)
    fmt = eGL_RGBA32UI;
  else
    return;

  GLuint texs[2];
  GL.glGenTextures(2, texs);
  GL.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, fmt, 0, 1, 0, arraySize * samples);
  GL.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, fmt, 0, 1, 0, arraySize);

  GL.glBindImageTexture(2, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
  GL.glActiveTexture(eGL_TEXTURE0);
  GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
  GL.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  GL.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  GL.glUseProgram(arrms.MS2Array);

  GLint loc = GL.glGetUniformLocation(arrms.MS2Array, "mscopy");
  if(loc >= 0)
  {
    GL.glProgramUniform4i(arrms.MS2Array, loc, samples, 0, 0, 0);

    GL.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize * samples));
  }
  GL.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  GL.glDeleteTextures(2, texs);

  rs.ApplyState(this);
}

void WrappedOpenGL::CopyDepthTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width,
                                            GLint height, GLint arraySize, GLint samples,
                                            GLenum intFormat)
{
  GLMarkerRegion renderoverlay("CopyDepthTex2DMSToArray");

  const ArrayMSPrograms &arrms = GetArrayMS();

  GLRenderState rs;
  rs.FetchState(this);

  GLuint vao = 0;
  GL.glGenVertexArrays(1, &vao);
  GL.glBindVertexArray(vao);

  GLuint texs[3];
  GL.glGenTextures(3, texs);
  GL.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, intFormat, 0, 1, 0, arraySize * samples);
  GL.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, intFormat, 0, 1, 0, arraySize);
  GL.glTextureView(texs[2], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, intFormat, 0, 1, 0, arraySize);
  GL.glTextureParameteriEXT(texs[0], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  GL.glTextureParameteriEXT(texs[0], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  GLuint fbo = 0;
  GL.glGenFramebuffers(1, &fbo);
  GL.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);
  GL.glDrawBuffers(0, NULL);

  GL.glUseProgram(arrms.DepthMS2Array);
  GL.glViewport(0, 0, width, height);

  GL.glDisable(eGL_CULL_FACE);
  GL.glDisable(eGL_BLEND);
  GL.glDisable(eGL_SCISSOR_TEST);
  if(!IsGLES)
    GL.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
  GL.glEnable(eGL_DEPTH_TEST);
  GL.glEnable(eGL_STENCIL_TEST);
  GL.glDepthFunc(eGL_ALWAYS);
  GL.glDepthMask(GL_TRUE);
  GL.glStencilOp(eGL_REPLACE, eGL_REPLACE, eGL_REPLACE);
  GL.glStencilMask(0xff);

  uint32_t numStencil = 1;
  GLenum attach = eGL_DEPTH_ATTACHMENT;

  switch(GetBaseFormat(intFormat))
  {
    case eGL_DEPTH_STENCIL:
      numStencil = 256;
      attach = eGL_DEPTH_STENCIL_ATTACHMENT;
      break;
    case eGL_DEPTH_COMPONENT:
      numStencil = 1;
      attach = eGL_DEPTH_ATTACHMENT;
      break;
    case eGL_STENCIL_INDEX:
      numStencil = 256;
      attach = eGL_STENCIL_ATTACHMENT;
      break;
    default: RDCERR("Unexpected base format! %s", ToStr(intFormat).c_str()); break;
  }

  if(attach == eGL_DEPTH_STENCIL_ATTACHMENT || attach == eGL_DEPTH_ATTACHMENT)
  {
    // depth aspect
    GL.glActiveTexture(eGL_TEXTURE0);
    GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
    GL.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                              eGL_DEPTH_STENCIL_TEXTURE_MODE, eGL_DEPTH_COMPONENT);
  }

  if(numStencil > 1)
  {
    // stencil aspect
    GL.glActiveTexture(eGL_TEXTURE1);
    GL.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[2]);
    GL.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                              eGL_DEPTH_STENCIL_TEXTURE_MODE, eGL_STENCIL_INDEX);
  }

  GLint loc = GL.glGetUniformLocation(arrms.DepthMS2Array, "mscopy");
  if(loc >= 0)
  {
    for(GLint i = 0; i < arraySize * samples; i++)
    {
      GL.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, attach, texs[0], 0, i);

      for(uint32_t s = 0; s < numStencil; s++)
      {
        uint32_t currentStencil = numStencil == 1 ? 1000 : s;

        GL.glStencilFunc(eGL_ALWAYS, int(s), 0xff);

        GL.glProgramUniform4i(arrms.DepthMS2Array, loc, samples, i % samples, i / samples,
                              currentStencil);

        GL.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      }
    }
  }

  rs.ApplyState(this);

  GL.glDeleteVertexArrays(1, &vao);
  GL.glDeleteFramebuffers(1, &fbo);
  GL.glDeleteTextures(3, texs);
}

void WrappedOpenGL::CopyArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width, GLint height,
                                       GLint arraySize, GLint samples, GLenum intFormat,
                                       uint32_t selectedSlice)
{
  WrappedOpenGL &drv = *this;

  intFormat = GetSizedFormat(intFormat);

  const ArrayMSPrograms &arrms = GetArrayMS();

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

  if(!arrms.Array2MS || (IsDepthStencilFormat(intFormat) && !arrms.DepthArray2MS))
  {
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
  rs.FetchState(this);

  GLenum viewClass;
  drv.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS, 1,
                            (GLint *)&viewClass);

  GLenum fmt = eGL_R32UI;
  if(viewClass == eGL_VIEW_CLASS_8_BITS)
    fmt = eGL_R8UI;
  else if(viewClass == eGL_VIEW_CLASS_16_BITS)
    fmt = eGL_R16UI;
  else if(viewClass == eGL_VIEW_CLASS_24_BITS)
    fmt = eGL_RGB8UI;
  else if(viewClass == eGL_VIEW_CLASS_32_BITS)
    fmt = eGL_RGBA8UI;
  else if(viewClass == eGL_VIEW_CLASS_64_BITS)
    fmt = eGL_RG32UI;
  else if(viewClass == eGL_VIEW_CLASS_128_BITS)
    fmt = eGL_RGBA32UI;
  else
    return;

  GLuint texs[2];
  drv.glGenTextures(2, texs);
  drv.glTextureView(texs[0], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, destMS, fmt, 0, 1, 0, arraySize);
  drv.glTextureView(texs[1], eGL_TEXTURE_2D_ARRAY, srcArray, fmt, 0, 1, 0, arraySize * samples);

  drv.glBindImageTexture(2, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
  drv.glActiveTexture(eGL_TEXTURE0);
  drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, texs[1]);
  drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  drv.glUseProgram(arrms.Array2MS);

  GLint loc = drv.glGetUniformLocation(arrms.Array2MS, "mscopy");
  if(loc >= 0)
  {
    if(singleSliceMode)
    {
      GLint sampleOffset = (selectedSlice % samples);
      GLint sliceOffset = (selectedSlice / samples);
      drv.glProgramUniform4i(arrms.Array2MS, loc, samples, sampleOffset, sliceOffset, 0);

      drv.glDispatchCompute((GLuint)width, (GLuint)height, 1);
    }
    else
    {
      drv.glProgramUniform4i(arrms.Array2MS, loc, samples, 0, 0, 0);

      drv.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize * samples));
    }
  }
  drv.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  drv.glDeleteTextures(2, texs);

  rs.ApplyState(this);
}

void WrappedOpenGL::CopyDepthArrayToTex2DMS(GLuint destMS, GLuint srcArray, GLint width,
                                            GLint height, GLint arraySize, GLint samples,
                                            GLenum intFormat, uint32_t selectedSlice)
{
  GLMarkerRegion renderoverlay("CopyDepthArrayToTex2DMS");

  bool singleSliceMode = (selectedSlice != ~0U);

  WrappedOpenGL &drv = *this;

  const ArrayMSPrograms &arrms = GetArrayMS();

  GLRenderState rs;
  rs.FetchState(this);

  GLuint vao = 0;
  drv.glGenVertexArrays(1, &vao);
  drv.glBindVertexArray(vao);

  GLuint texs[3];
  drv.glGenTextures(3, texs);
  drv.glTextureView(texs[0], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, destMS, intFormat, 0, 1, 0, arraySize);
  drv.glTextureView(texs[1], eGL_TEXTURE_2D_ARRAY, srcArray, intFormat, 0, 1, 0, arraySize * samples);
  drv.glTextureView(texs[2], eGL_TEXTURE_2D_ARRAY, srcArray, intFormat, 0, 1, 0, arraySize * samples);
  drv.glTextureParameteriEXT(texs[0], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  drv.glTextureParameteriEXT(texs[0], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  GLuint fbo = 0;
  drv.glGenFramebuffers(1, &fbo);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, fbo);
  drv.glDrawBuffers(0, NULL);

  drv.glUseProgram(arrms.DepthArray2MS);
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
    case eGL_DEPTH_COMPONENT:
      numStencil = 1;
      attach = eGL_DEPTH_ATTACHMENT;
      break;
    case eGL_STENCIL_INDEX:
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
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTextureParameteriEXT(texs[1], eGL_TEXTURE_2D_ARRAY, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                               eGL_DEPTH_COMPONENT);
  }

  if(numStencil > 1)
  {
    // stencil aspect
    drv.glActiveTexture(eGL_TEXTURE1);
    drv.glBindTexture(eGL_TEXTURE_2D_ARRAY, texs[2]);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    drv.glTextureParameteriEXT(texs[2], eGL_TEXTURE_2D_ARRAY, eGL_DEPTH_STENCIL_TEXTURE_MODE,
                               eGL_STENCIL_INDEX);
  }

  GLint loc = drv.glGetUniformLocation(arrms.DepthArray2MS, "mscopy");
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

        drv.glProgramUniform4i(arrms.DepthArray2MS, loc, samples, 0, i, currentStencil);

        drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      }

      if(singleSliceMode)
        break;
    }
  }

  rs.ApplyState(this);

  drv.glDeleteVertexArrays(1, &vao);
  drv.glDeleteFramebuffers(1, &fbo);
  drv.glDeleteTextures(3, texs);
}
