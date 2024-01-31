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

RD_TEST(GL_Simple_Triangle, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Just draws a simple triangle, using normal pipeline. Basic test that can be used "
      "for any dead-simple tests that don't require any particular API use";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, GL_STATIC_DRAW);

    ConfigureDefaultVAO();

    GLuint program = MakeProgram(GLDefaultVertex, GLDefaultPixel);

    // make a simple texture so that the structured data includes texture initial states
    GLuint tex = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 4, 4);

    float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
    float textureColourData[4 * 4 * 4];
    for(int i = 0; i < 4 * 4; ++i)
    {
      for(int c = 0; c < 4; ++c)
        textureColourData[i * 4 + c] = col[c];
    }

    GLuint texMS = MakeTexture();
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texMS);
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, 4, 4, GL_TRUE);

    while(Running())
    {
      glClearBufferfv(GL_COLOR, 0, col);

      glBindTexture(GL_TEXTURE_2D, tex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_FLOAT, textureColourData);

      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texMS);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
