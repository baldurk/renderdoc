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

#include "gl_test.h"

RD_TEST(GL_Unshared_Context, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Given an application with contexts A and B (not shared). Starts the capture with context A, "
      "then activates context B mid-frame and renders using some resources that are deferred, then "
      "activates context A again before the end of the frame";

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

layout(binding = 0) uniform sampler2D tex;

void main()
{
	Color = vertIn.col * texture(tex, vertIn.uv.xy);
}

)EOSHADER";

  GLuint MakeShader(GLenum type, std::string src)
  {
    GLuint shader = glCreateShader(type);

    const char *cstr = src.c_str();
    glShaderSource(shader, 1, &cstr, NULL);
    glCompileShader(shader);

    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    char buffer[1024];
    if(status == 0)
    {
      glGetShaderInfoLog(shader, 1024, NULL, buffer);
      TEST_ERROR("Shader error: %s", buffer);
      glDeleteShader(shader);
      return 0;
    }

    return shader;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    void *contextA = mainContext;
    void *contextB = MakeContext(mainWindow, NULL);

    GLuint vao, vb, program = 0;
    GLuint fboB, texB;

    ActivateContext(mainWindow, contextB);
    {
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);

      glGenBuffers(1, &vb);
      glBindBuffer(GL_ARRAY_BUFFER, vb);
      glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                            (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);

      {
        GLuint vs = MakeShader(GL_VERTEX_SHADER, common + vertex);
        GLuint fs = MakeShader(GL_FRAGMENT_SHADER, common + pixel);

        if(vs && fs)
        {
          program = glCreateProgram();

          glAttachShader(program, vs);
          glAttachShader(program, fs);

          glLinkProgram(program);

          GLint status = 0;
          glGetProgramiv(program, GL_LINK_STATUS, &status);

          glDetachShader(program, vs);
          glDeleteShader(vs);

          glDetachShader(program, fs);
          glDeleteShader(fs);

          if(status == 0)
          {
            char buffer[1024];
            glGetProgramInfoLog(program, 1024, NULL, buffer);
            TEST_ERROR("Link error: %s", buffer);

            glDeleteProgram(program);
            program = 0;
          }
        }
      }

      glGenFramebuffers(1, &fboB);
      glBindFramebuffer(GL_FRAMEBUFFER, fboB);

      glGenTextures(1, &texB);
      glBindTexture(GL_TEXTURE_2D, texB);

      glTextureStorage2D(texB, 1, GL_RGBA8, screenWidth, screenHeight);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texB, 0);

      glBindFramebuffer(GL_FRAMEBUFFER, fboB);

      float col[] = {1.0f, 0.5f, 0.25f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);
    }

    // wait 3 seconds to make sure texB becomes a persistent resource
    msleep(3000);

    while(Running())
    {
      ActivateContext(mainWindow, contextA);
      {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        float col[] = {1.0f, 0.0f, 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, col);
      }

      ActivateContext(mainWindow, contextB);
      {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glBindTextureUnit(0, texB);

        glBindVertexArray(vao);

        glUseProgram(program);

        glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

        glDrawArrays(GL_TRIANGLES, 0, 3);
      }

      ActivateContext(mainWindow, contextA);
      {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        float col[] = {0.0f, 1.0f, 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, col);
      }

      Present();
    }

    ActivateContext(mainWindow, contextB);

    glDeleteTextures(1, &texB);
    glDeleteFramebuffers(1, &fboB);

    glDeleteProgram(program);
    glDeleteBuffers(1, &vb);
    glDeleteVertexArrays(1, &vao);

    DestroyContext(contextB);

    return 0;
  }
};

REGISTER_TEST();
