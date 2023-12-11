/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "gl_test.h"

RD_TEST(GL_Overlay_Test, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Makes a couple of draws that show off all the overlays in some way";

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

  std::string whitepixel = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vec4(1,1,1,1);
}

)EOSHADER";

  std::string fragdepthpixel = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;

	if ((gl_FragCoord.x > 180.0) && (gl_FragCoord.x < 185.0) &&
      (gl_FragCoord.y > 135.0) && (gl_FragCoord.y < 145.0))
	{
		gl_FragDepth = 0.0;
	}
  else
  {
		gl_FragDepth = gl_FragCoord.z;
  }
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    // note that the Z position values are rescaled for GL default -1.0 to 1.0 clipspace, relative
    // to all other APIs
    const DefaultA2V VBData[] = {
        // this triangle occludes in depth
        {Vec3f(-0.5f, -0.5f, -1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.0f, -1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.0f, -1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle occludes in stencil
        {Vec3f(-0.5f, 0.0f, 0.8f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.5f, 0.8f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.0f, 0.8f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle is just in the background to contribute to overdraw
        {Vec3f(-0.9f, -0.9f, 0.9f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.9f, 0.9f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.9f, -0.9f, 0.9f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(1.0f, 0.0f)},

        // the draw has a few triangles, main one that is occluded for depth, another that is
        // adding to overdraw complexity, one that is backface culled, then a few more of various
        // sizes for triangle size overlay
        {Vec3f(-0.3f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.5f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.2f, -0.2f, 0.2f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.2f, 0.0f, 0.2f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.2f, -0.4f, 0.2f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // backface culled
        {Vec3f(0.1f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, -0.2f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.2f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // depth clipped (i.e. not clamped)
        {Vec3f(0.6f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, 0.2f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.0f, 2.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // small triangles
        // size=0.005
        {Vec3f(0.0f, 0.4f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.41f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.01f, 0.4f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.015
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.515f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.015f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.02
        {Vec3f(0.0f, 0.6f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.62f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.02f, 0.6f, 0.0f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.025
        {Vec3f(0.0f, 0.7f, 0.0f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.725f, 0.0f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.025f, 0.7f, 0.0f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle deliberately goes out of the viewport, it will test viewport & scissor
        // clipping
        {Vec3f(-1.3f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.3f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(VBData), VBData, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(common + vertex, common + pixel);
    GLuint whiteprogram = MakeProgram(common + vertex, whitepixel);
    GLuint fragdepthprogram = MakeProgram(common + vertex, common + fragdepthpixel);

    const char *fmtNames[] = {"D24_S8", "D32F_S8", "D16_S0", "D24_S0", "D32F_S0"};
    GLenum fmts[] = {GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT16,
                     GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F};
    const size_t countFmts = ARRAY_COUNT(fmts);

    GLuint fbos[countFmts];
    GLuint msaafbos[countFmts];
    for(size_t f = 0; f < countFmts; f++)
    {
      GLenum fmt = fmts[f];

      // Color render texture
      GLuint attachments[] = {MakeTexture(), MakeTexture(), MakeTexture(), MakeTexture()};
      GLenum dsAttachment = GL_DEPTH_ATTACHMENT;
      if(fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8)
        dsAttachment = GL_DEPTH_STENCIL_ATTACHMENT;
      if(fmt == GL_STENCIL_INDEX8)
        dsAttachment = GL_STENCIL_ATTACHMENT;

      {
        GLuint fbo = MakeFBO();
        fbos[f] = fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glBindTexture(GL_TEXTURE_2D, attachments[0]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, screenWidth, screenHeight);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, attachments[0],
                               0);

        glBindTexture(GL_TEXTURE_2D, attachments[1]);
        glTexStorage2D(GL_TEXTURE_2D, 1, fmt, screenWidth, screenHeight);
        glFramebufferTexture2D(GL_FRAMEBUFFER, dsAttachment, GL_TEXTURE_2D, attachments[1], 0);
      }

      {
        GLuint msaafbo = MakeFBO();
        msaafbos[f] = msaafbo;
        glBindFramebuffer(GL_FRAMEBUFFER, msaafbo);

        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, attachments[2]);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_SRGB8_ALPHA8, screenWidth,
                                  screenHeight, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE,
                               attachments[2], 0);

        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, attachments[3]);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, fmt, screenWidth, screenHeight,
                                  GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, dsAttachment, GL_TEXTURE_2D_MULTISAMPLE,
                               attachments[3], 0);
      }
    }

    GLuint subtex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D_ARRAY, subtex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_SRGB8_ALPHA8, screenWidth, screenHeight, 5);

    GLuint subfbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, subfbo);

    // clear all mips/slices first
    GLfloat black[4] = {};
    for(GLint s = 0; s < 5; s++)
    {
      for(GLint m = 0; m < 4; m++)
      {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, subtex, m, s);
        glClearBufferfv(GL_COLOR, 0, black);
      }
    }

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, subtex, 2, 2);

    GLuint subfbo2 = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, subfbo2);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, subtex, 3, 2);

    // keep a trash buffer bound to pixel pack/unpack
    GLuint trash = MakeBuffer();
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, trash);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, 1024, 0, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, trash);

    while(Running())
    {
      glBindVertexArray(vao);

      for(size_t i = 0; i < countFmts; ++i)
      {
        GLenum fmt = fmts[i];
        std::string fmtName(fmtNames[i]);
        for(bool is_msaa : {false, true})
        {
          bool hasStencil = false;
          if(fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8)
            hasStencil = true;

          GLuint fb = is_msaa ? msaafbos[i] : fbos[i];

          glEnable(GL_CULL_FACE);
          glFrontFace(GL_CW);

          glDepthMask(GL_TRUE);
          glEnable(GL_DEPTH_TEST);
          glEnable(GL_SCISSOR_TEST);
          glDisable(GL_DEPTH_CLAMP);
          glDisable(GL_STENCIL_TEST);
          glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
          glStencilFunc(GL_ALWAYS, 0x55, 0xff);

          glViewport(10, 10, GLsizei(screenWidth) - 20, GLsizei(screenHeight) - 20);
          glScissor(0, 0, screenWidth, screenHeight);

          glBindFramebuffer(GL_FRAMEBUFFER, fb);
          float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
          glClearBufferfv(GL_COLOR, 0, col);
          glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

          if(hasStencil)
          {
            glScissor(32, GLsizei(screenHeight) - 32, 6, 6);
            glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 1);
            glScissor(0, 0, screenWidth, screenHeight);
          }

          glUseProgram(program);

          // 1: write depth
          glDepthFunc(GL_ALWAYS);
          glDrawArrays(GL_TRIANGLES, 0, 3);

          glDepthFunc(GL_LEQUAL);
          if(hasStencil)
          {
            // 2: write stencil
            glEnable(GL_STENCIL_TEST);
            glDrawArrays(GL_TRIANGLES, 3, 3);
          }

          // 3: write background
          glDisable(GL_STENCIL_TEST);
          glDrawArrays(GL_TRIANGLES, 6, 3);

          // add a marker so we can easily locate this draw
          std::string markerName(is_msaa ? "MSAA Test " : "Normal Test ");
          markerName += fmtName;
          setMarker(markerName);

          glEnable(GL_STENCIL_TEST);
          glStencilFunc(GL_GREATER, 0x55, 0xff);
          glUseProgram(fragdepthprogram);
          glDrawArrays(GL_TRIANGLES, 9, 24);
          glUseProgram(program);

          if(!is_msaa)
          {
            setMarker("Viewport Test " + fmtName);
            glDisable(GL_STENCIL_TEST);
            glViewport(10, screenHeight - 90, 80, 80);
            glScissor(24, screenHeight - 76, 52, 52);
            glDrawArrays(GL_TRIANGLES, 33, 3);
          }

          if(is_msaa)
          {
            setMarker("Sample Mask Test " + fmtName);
            glDisable(GL_STENCIL_TEST);
            glEnable(GL_SAMPLE_MASK);
            glSampleMaski(0, 0x2);
            glViewport(0, screenHeight - 80, 80, 80);
            glScissor(0, screenHeight - 80, 80, 80);
            glDrawArrays(GL_TRIANGLES, 6, 3);
            glSampleMaski(0, ~0U);
            glDisable(GL_SAMPLE_MASK);
          }

          glScissor(0, 0, screenWidth, screenHeight);
        }
      }

      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight,
                        GL_COLOR_BUFFER_BIT, GL_LINEAR);

      glBindFramebuffer(GL_FRAMEBUFFER, subfbo);
      float col2[] = {0.0f, 0.0f, 0.0f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col2);

      glDepthFunc(GL_ALWAYS);
      glDisable(GL_STENCIL_TEST);

      glUseProgram(whiteprogram);

      glViewport(5, 5, GLsizei(screenWidth) / 4 - 10, GLsizei(screenHeight) / 4 - 10);
      glScissor(0, 0, screenWidth / 4, screenHeight / 4);

      setMarker("Subresources mip 2");
      glDrawArrays(GL_TRIANGLES, 9, 24);

      glBindFramebuffer(GL_FRAMEBUFFER, subfbo2);
      glClearBufferfv(GL_COLOR, 0, col2);

      glViewport(2, 2, GLsizei(screenWidth) / 8 - 4, GLsizei(screenHeight) / 8 - 4);
      glScissor(0, 0, screenWidth / 8, screenHeight / 8);

      setMarker("Subresources mip 3");
      glDrawArrays(GL_TRIANGLES, 9, 24);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
