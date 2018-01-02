/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include "data/glsl/debuguniforms.h"

void GLReplay::CopyTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                                  GLint arraySize, GLint samples, GLenum intFormat)
{
  WrappedOpenGL &gl = *m_pDriver;

  // create temporary texture array, which we'll initialise to be the width/height in same format,
  // with the same number of array slices as multi samples.
  gl.glGenTextures(1, &destArray);
  gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, destArray);

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
    gl.glTextureImage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 0, intFormat, width, height,
                           arraySize * samples, 0, GetBaseFormat(intFormat), GetDataType(intFormat),
                           NULL);
    gl.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    return;
  }

  // initialise the texture using texture storage, as required for texture views.
  gl.glTextureStorage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 1, intFormat, width, height,
                           arraySize * samples);

  GLRenderState rs(&gl.GetHookset());
  rs.FetchState(m_pDriver);

  GLenum viewClass;
  gl.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS,
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
  gl.glGenTextures(2, texs);
  gl.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, fmt, 0, 1, 0, arraySize * samples);
  gl.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, fmt, 0, 1, 0, arraySize);

  gl.glBindImageTexture(2, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
  gl.glActiveTexture(eGL_TEXTURE0);
  gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
  gl.glBindSampler(0, DebugData.pointNoMipSampler);
  gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  gl.glUseProgram(DebugData.MS2Array);

  GLint loc = gl.glGetUniformLocation(DebugData.MS2Array, "mscopy");
  if(loc >= 0)
  {
    gl.glProgramUniform4ui(DebugData.MS2Array, loc, samples, 0, 0, 0);

    gl.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize * samples));
  }
  gl.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  gl.glDeleteTextures(2, texs);

  rs.ApplyState(m_pDriver);
}
