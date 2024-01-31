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

RD_TEST(GL_Large_Buffer, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle over the span of a very large buffer to ensure readbacks work correctly.";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    uint32_t indices[3] = {0, 1000000, 61982400};

    GLuint ib = MakeBuffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, 0);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, 2128ULL * 1024 * 1024, 0, GL_DYNAMIC_STORAGE_BIT);

    glBufferSubData(GL_ARRAY_BUFFER, indices[0] * sizeof(DefaultA2V), sizeof(DefaultA2V),
                    &DefaultTri[0]);
    glBufferSubData(GL_ARRAY_BUFFER, indices[1] * sizeof(DefaultA2V), sizeof(DefaultA2V),
                    &DefaultTri[1]);
    glBufferSubData(GL_ARRAY_BUFFER, indices[2] * sizeof(DefaultA2V), sizeof(DefaultA2V),
                    &DefaultTri[2]);

    ConfigureDefaultVAO();

    GLuint program = MakeProgram(GLDefaultVertex, GLDefaultPixel);

    // make a simple texture so that the structured data includes texture initial states
    GLuint tex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 4, 4);

    while(Running())
    {
      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
