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

#include <stdio.h>
#include "gl_test.h"

bool IsUIntFormat(GLenum internalFormat)
{
  switch(internalFormat)
  {
    case GL_R8UI:
    case GL_RG8UI:
    case GL_RGB8UI:
    case GL_RGBA8UI:
    case GL_R16UI:
    case GL_RG16UI:
    case GL_RGB16UI:
    case GL_RGBA16UI:
    case GL_R32UI:
    case GL_RG32UI:
    case GL_RGB32UI:
    case GL_RGBA32UI:
    case GL_RGB10_A2UI: return true;
    default: break;
  }

  return false;
}

bool IsSIntFormat(GLenum internalFormat)
{
  switch(internalFormat)
  {
    case GL_R8I:
    case GL_RG8I:
    case GL_RGB8I:
    case GL_RGBA8I:
    case GL_R16I:
    case GL_RG16I:
    case GL_RGB16I:
    case GL_RGBA16I:
    case GL_R32I:
    case GL_RG32I:
    case GL_RGB32I:
    case GL_RGBA32I: return true;
    default: break;
  }

  return false;
}

RD_TEST(GL_Pixel_History, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Draw a triangle to a variety of texture formats (to test pixel history).";

  std::string common = R"EOSHADER(

#version 420 core

#define v2f v2f_block \
{                     \
    vec4 pos;           \
    vec4 col;           \
    vec4 uv;            \
}

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

out v2f vertOut;

void main()
{
    vertOut.pos = vec4(Position.xyz, 1);
    gl_Position = vertOut.pos;
    vertOut.col = Color;
    vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
    Color = vertIn.col;
}

)EOSHADER";

  std::string pixelSigned = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out ivec4 Color;

void main()
{
    vec4 a = vertIn.col * 127.0;
    Color = ivec4(a.r, a.g, a.b, a.a);
}

)EOSHADER";

  std::string pixelUnsigned = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out uvec4 Color;

void main()
{
    vec4 a = vertIn.col * 255.0;
    Color = uvec4(a.r, a.g, a.b, a.a);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    const DefaultA2V DefaultTriWithALessBoringColour[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.57721f, 0.27182f, 0.1385f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.57721f, 0.27182f, 0.1385f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.57721f, 0.27182f, 0.1385f, 1.0f), Vec2f(1.0f, 0.0f)},
    };
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTriWithALessBoringColour),
                    DefaultTriWithALessBoringColour, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(common + vertex, common + pixel);
    GLuint programSigned = MakeProgram(common + vertex, common + pixelSigned);
    GLuint programUnsigned = MakeProgram(common + vertex, common + pixelUnsigned);

    GLenum colorFormats[] = {
        GL_RGBA8,      GL_RGBA16,         GL_RGBA16F, GL_RGBA32F,  GL_RGBA8I,  GL_RGBA8UI,
        GL_RGBA16I,    GL_RGBA16UI,       GL_RGBA32I, GL_RGBA32UI, GL_RGB8,    GL_RGB16,
        GL_RGB16F,     GL_RGB32F,         GL_RGB8I,   GL_RGB8UI,   GL_RG8,     GL_RG16,
        GL_RG16F,      GL_RG32F,          GL_RG8I,    GL_RG8UI,    GL_R8,      GL_R16,
        GL_R16F,       GL_R32F,           GL_R8I,     GL_R8UI,     GL_RGB5_A1, GL_RGB10_A2,
        GL_RGB10_A2UI, GL_R11F_G11F_B10F, GL_RGB565};

    GLenum depthFormats[] = {GL_NONE,
                             GL_DEPTH_COMPONENT16,
                             GL_DEPTH_COMPONENT24,
                             GL_DEPTH_COMPONENT32,
                             GL_DEPTH_COMPONENT32F,
                             GL_DEPTH24_STENCIL8,
                             GL_DEPTH32F_STENCIL8};

    constexpr size_t colorFormatSize = sizeof(colorFormats) / sizeof(GLenum);
    constexpr size_t depthFormatSize = sizeof(depthFormats) / sizeof(GLenum);

    GLuint colorTextures[colorFormatSize];
    GLuint multisampledColorTextures[colorFormatSize];
    GLuint depthTextures[depthFormatSize];
    GLuint multisampledDepthTextures[depthFormatSize];

    for(size_t i = 0; i < colorFormatSize; ++i)
    {
      colorTextures[i] = MakeTexture();
      multisampledColorTextures[i] = MakeTexture();
      glBindTexture(GL_TEXTURE_2D, colorTextures[i]);
      glTexStorage2D(GL_TEXTURE_2D, 1, colorFormats[i], screenWidth, screenHeight);
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multisampledColorTextures[i]);
      glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 2, colorFormats[i], screenWidth,
                                screenHeight, GL_TRUE);
    }

    for(size_t i = 0; i < depthFormatSize; ++i)
    {
      depthTextures[i] = MakeTexture();
      multisampledDepthTextures[i] = MakeTexture();
      glBindTexture(GL_TEXTURE_2D, depthTextures[i]);
      glTexStorage2D(GL_TEXTURE_2D, 1, depthFormats[i], screenWidth, screenHeight);
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multisampledDepthTextures[i]);
      glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 2, depthFormats[i], screenWidth,
                                screenHeight, GL_TRUE);
    }

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glDepthFunc(GL_ALWAYS);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glStencilFunc(GL_ALWAYS, 0xcc, 0xff);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);

      for(size_t multisampled = 0; multisampled < 2; multisampled++)
      {
        for(size_t h = 0; h < depthFormatSize; ++h)
        {
          for(size_t i = 0; i < colorFormatSize; ++i)
          {
            if(multisampled)
            {
              glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D_MULTISAMPLE, multisampledColorTextures[i], 0);
            }
            else
            {
              glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                     colorTextures[i], 0);
            }

            if(depthFormats[h] == GL_NONE)
            {
              glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                                     0);
            }
            else if(depthFormats[h] == GL_DEPTH24_STENCIL8 || depthFormats[h] == GL_DEPTH32F_STENCIL8)
            {
              if(multisampled)
              {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_TEXTURE_2D_MULTISAMPLE, multisampledDepthTextures[h], 0);
              }
              else
              {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                       depthTextures[h], 0);
              }
            }
            else
            {
              if(multisampled)
              {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                       GL_TEXTURE_2D_MULTISAMPLE, multisampledDepthTextures[h], 0);
              }
              else
              {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                       depthTextures[h], 0);
              }

              glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            }

            GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
            glDrawBuffers(1, bufs);

            GLenum check = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if(check != GL_FRAMEBUFFER_COMPLETE)
            {
              TEST_ERROR(
                  "Framebuffer is not complete with depth format %x colour format %x multisampled "
                  "%d\n ",
                  depthFormats[h], colorFormats[i], multisampled);
              continue;
            }

            glBindVertexArray(vao);

            if(IsUIntFormat(colorFormats[i]))
            {
              GLuint col[] = {20, 20, 20, 255};
              glClearBufferuiv(GL_COLOR, 0, col);
              glUseProgram(programUnsigned);
            }
            else if(IsSIntFormat(colorFormats[i]))
            {
              GLint col[] = {20, 20, -20, 127};
              glClearBufferiv(GL_COLOR, 0, col);
              glUseProgram(programSigned);
            }
            else
            {
              float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
              glClearBufferfv(GL_COLOR, 0, col);
              glUseProgram(program);
            }

            glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

            glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

            glDrawArrays(GL_TRIANGLES, 0, 3);
          }
        }
      }

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, 0);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
