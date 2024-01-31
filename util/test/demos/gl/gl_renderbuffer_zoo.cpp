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

RD_TEST(GL_Renderbuffer_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests different types of renderbuffers to ensure they work correctly in normal texture "
      "operations";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    const DefaultA2V Tri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.5f), Vec4f(0.2f, 0.75f, 0.2f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.5f), Vec4f(0.2f, 0.75f, 0.2f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.5f), Vec4f(0.2f, 0.75f, 0.2f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Tri), Tri, GL_STATIC_DRAW);

    ConfigureDefaultVAO();

    GLuint program = MakeProgram(GLDefaultVertex, GLDefaultPixel);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLenum db = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &db);

    GLuint rbs[9] = {};
    glGenRenderbuffers(ARRAY_COUNT(rbs), rbs);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 512, 512);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16F, 512, 512);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[2]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16F, 640, 480);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[3]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 640, 480);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[4]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 640, 480);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[5]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, 640, 480);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[6]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA16F, 640, 480);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[7]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_RGBA16F, 640, 480);

    glBindRenderbuffer(GL_RENDERBUFFER, rbs[8]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, 640, 480);

    float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
    while(Running())
    {
      glBindVertexArray(vao);
      glUseProgram(program);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
      glDisable(GL_DEPTH_TEST);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[0]);
      glViewport(0, 0, 512, 512);

      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[1]);
      glViewport(0, 0, 512, 512);

      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[2]);
      glViewport(0, 0, 640, 480);

      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glEnable(GL_DEPTH_TEST);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbs[3]);

      glClearBufferfv(GL_COLOR, 0, col);
      glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.9f, 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbs[4]);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);

      glClearBufferfv(GL_COLOR, 0, col);
      glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.9f, 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glDisable(GL_DEPTH_TEST);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[5]);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);

      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[6]);

      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[7]);

      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glEnable(GL_DEPTH_TEST);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbs[5]);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbs[8]);

      glClearBufferfv(GL_COLOR, 0, col);
      glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.9f, 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      Present();
    }

    glDeleteRenderbuffers(ARRAY_COUNT(rbs), rbs);

    return 0;
  }
};

REGISTER_TEST();
