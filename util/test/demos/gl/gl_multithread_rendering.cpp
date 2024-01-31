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

#include <atomic>
#include <mutex>
#include <thread>
#include "gl_test.h"

RD_TEST(GL_Multithread_Rendering, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Draws from two threads simultaneously, to test automatic catching of thread switching.";

  std::string pixel = R"EOSHADER(
#version 420 core

in v2f_block
{
	vec4 pos;
	vec4 col;
	vec4 uv;
} vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
  Color.b = 

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const DefaultA2V GreenTri[3] = {
        {Vec3f(-1.0f, -1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.0f, -1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    const DefaultA2V RedTri[3] = {
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, 1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.0f, -1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
    };

    struct ctxdata
    {
      void *ctx;
      GraphicsWindow *win;

      std::atomic_bool rendering;

      GLuint VB, VAO, prog, FBO, tex;
    } A, B;

    A.rendering = true;
    B.rendering = true;

    A.VB = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, A.VB);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(RedTri), RedTri, 0);

    A.prog = MakeProgram(GLDefaultVertex, pixel + "0.25f;\n}");

    A.tex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, A.tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, screenWidth, screenHeight);

    B.VB = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, B.VB);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(GreenTri), GreenTri, 0);

    B.prog = MakeProgram(GLDefaultVertex, pixel + "0.75f;\n}");

    B.tex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, B.tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, screenWidth, screenHeight);

    // make FBOs on the main context for reading
    GLuint Afbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, Afbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, A.tex, 0);

    GLuint Bfbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, Bfbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, B.tex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    A.win = MakeWindow(32, 32, "A");
    B.win = MakeWindow(32, 32, "B");
    A.ctx = MakeContext(A.win, mainContext);
    B.ctx = MakeContext(B.win, mainContext);

    ActivateContext(mainWindow, mainContext);

    std::atomic_bool quit;
    quit = false;

    auto windowThread = [&](int idx) {
      ctxdata &ctx = (idx == 0 ? A : B);

      ActivateContext(ctx.win, ctx.ctx);

      glEnable(GL_FRAMEBUFFER_SRGB);

      glGenVertexArrays(1, &ctx.VAO);
      glBindVertexArray(ctx.VAO);

      glGenFramebuffers(1, &ctx.FBO);
      glBindFramebuffer(GL_FRAMEBUFFER, ctx.FBO);

      while(true)
      {
        if(quit)
          break;

        glBindFramebuffer(GL_FRAMEBUFFER, ctx.FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.tex, 0);
        float col[] = {0.2f + (1 - idx) * 0.1f, 0.2f + idx * 0.1f, 0.2f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, col);

        const int div = 40;

        GLsizei w = GLsizei(screenWidth) / div;
        GLsizei h = GLsizei(screenHeight) / div;

        for(GLsizei y = 0; y < div; y++)
        {
          for(GLsizei x = 0; x < div / 2; x++)
          {
            glBindFramebuffer(GL_FRAMEBUFFER, ctx.FBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx.tex, 0);
            glBindVertexArray(ctx.VAO);
            glBindBuffer(GL_ARRAY_BUFFER, ctx.VB);
            ConfigureDefaultVAO();
            glUseProgram(ctx.prog);
            glViewport((GLsizei(idx * screenWidth) / 2) + w * x, h * y, w, h);
            glDrawArrays(GL_TRIANGLES, 0, 3);
          }
        }

        glFinish();

        // don't present until both contexts are done. This isn't necessary but ensures captures
        // always start at the same point for both and only diverge within a frame.
        ctx.rendering = false;
        while(!quit && !ctx.rendering)
        {
          // busy loop waiting to be woken up. ha ha.
        }
      }

      glDeleteFramebuffers(1, &ctx.FBO);
      glDeleteVertexArrays(1, &ctx.VAO);

      ActivateContext(ctx.win, NULL);
    };

    std::thread thread_A(windowThread, 0);
    std::thread thread_B(windowThread, 1);

    while(Running())
    {
      if(!A.rendering && !B.rendering)
      {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, Afbo);

        float black[4] = {};
        glClearBufferfv(GL_COLOR, 0, black);

        glBlitFramebuffer(0, 0, screenWidth / 2, screenHeight - 10, 0, 0, screenWidth / 2,
                          screenHeight - 10, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, Bfbo);

        glBlitFramebuffer(screenWidth / 2, 0, screenWidth, screenHeight - 10, screenWidth / 2, 0,
                          screenWidth, screenHeight - 10, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glFinish();

        Present(mainWindow);

        A.rendering = true;
        B.rendering = true;
      }
    }

    quit = true;

    thread_A.join();
    thread_B.join();

    DestroyContext(A.ctx);
    DestroyContext(B.ctx);
    delete A.win;
    delete B.win;

    return 0;
  }
};

REGISTER_TEST();
