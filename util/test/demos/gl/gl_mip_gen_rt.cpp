/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

TEST(GL_Mip_Gen_RT, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests rendering from one mip to another to do a downsample chain";

  std::string vertex = R"EOSHADER(
#version 420 core

out vec2 uv;

void main()
{
  const vec4 verts[4] = vec4[4](vec4(-1.0, -1.0, 0.5, 1.0), vec4(1.0, -1.0, 0.5, 1.0),
                                vec4(-1.0, 1.0, 0.5, 1.0), vec4(1.0, 1.0, 0.5, 1.0));

  gl_Position = verts[gl_VertexID];
  uv = gl_Position.xy * 0.5f + 0.5f;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 420 core

in vec2 uv;

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0) uniform sampler2D tex2D;

void main()
{
	Color = textureLod(tex2D, -uv, 0.0f);
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
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(vertex, pixel);

    GLuint tex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 8, GL_SRGB8_ALPHA8, 1024, 1024);

    GLuint fbo[8];
    for(int i = 0; i < 8; i++)
    {
      fbo[i] = MakeFBO();
      glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, i);
    }

    // fill upper mip with colour ramp
    uint32_t *ramp = new uint32_t[1024 * 1024];
    for(uint32_t i = 0; i < 1024 * 1024; i++)
    {
      float x = float(i % 1024);
      float y = float(i / 1024);
      ramp[i] = uint32_t(uint32_t(255.0f * (x / 1024.0f)) | (uint32_t(255.0f * (y / 1024.0f)) << 8) |
                         (uint32_t(255.0f * ((x + y) / 2048.0f)) << 16) | 0xff000000);
    }

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glClearBufferfv(GL_COLOR, 0, col);

      // clear all FBOs
      for(int i = 0; i < 8; i++)
      {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glClearBufferfv(GL_COLOR, 0, col);
      }

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glBindVertexArray(vao);

      glUseProgram(program);

      {
        // view first mip and upload data
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, ramp);

        for(int i = 1; i < 8; i++)
        {
          // bind relevant fbo
          glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);

          // change texture parameters to view previous mip
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, i - 1);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, i - 1);

          // set viewport
          glViewport(0, 0, 1024 >> i, 1024 >> i);

          // do downsample
          glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        // reset texture parameters to default
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        // bind default framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }

      Present();
    }

    delete[] ramp;

    return 0;
  }
};

REGISTER_TEST();
