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

RD_TEST(GL_Leak_Check, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Checks that we don't have memory leaks by exiting if memory usage gets too high, so we can "
      "wait and capture a late frame to check for leaks.";

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

    ConfigureDefaultVAO();

    GLuint program = MakeProgram(GLDefaultVertex, GLDefaultPixel);

    // make a simple texture so that the structured data includes texture initial states
    GLuint tex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 4, 4);

    GLuint rb = 0;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);

    while(Running())
    {
      // allow a generous 500MB, we're really only after catching big leaks here
      if(GetMemoryUsage() > 500 * 1000 * 1000)
      {
        TEST_ERROR("Memory usage of %llu is too high!", GetMemoryUsage());
        break;
      }

      glBindRenderbuffer(GL_RENDERBUFFER, rb);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16F, 300, 300);

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    glDeleteRenderbuffers(1, &rb);

    return 0;
  }
};

REGISTER_TEST();
