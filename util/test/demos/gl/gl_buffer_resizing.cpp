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

RD_TEST(GL_Buffer_Resizing, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Test that buffer resizing is handled correctly, both out of frame and in-frame.";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vbs[10];
    int i = 0;

    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);

    // create the buffer initially too small
    glBufferData(GL_ARRAY_BUFFER, 4, NULL, GL_DYNAMIC_DRAW);

    // then resize it up while at init time, to ensure we handle this correctly
    glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, GL_DYNAMIC_DRAW);

    // while harmless, test that we can resize *down* as well.
    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri) * 10, NULL, GL_DYNAMIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, GL_DYNAMIC_DRAW);

    // these will be resized in-frame
    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
    glBufferData(GL_ARRAY_BUFFER, 4, NULL, GL_DYNAMIC_DRAW);

    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri) * 10, NULL, GL_DYNAMIC_DRAW);

    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
    glBufferData(GL_ARRAY_BUFFER, 4, NULL, GL_DYNAMIC_DRAW);

    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
    glBufferData(GL_ARRAY_BUFFER, 1000, NULL, GL_DYNAMIC_DRAW);

    vbs[i] = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
    glBufferData(GL_ARRAY_BUFFER, 4, NULL, GL_DYNAMIC_DRAW);

    TEST_ASSERT(i < ARRAY_COUNT(vbs), "Make vbs[] bigger");

    GLuint program = MakeProgram(GLDefaultVertex, GLDefaultPixel);
    glUseProgram(program);

    glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

    float col[] = {0.2f, 0.2f, 0.2f, 1.0f};

    while(Running())
    {
      i = 0;
      // check these VBs are OK
      glClearBufferfv(GL_COLOR, 0, col);
      glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
      ConfigureDefaultVAO();
      glDrawArrays(GL_TRIANGLES, 0, 3);

      // check these VBs are OK
      glClearBufferfv(GL_COLOR, 0, col);
      glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
      ConfigureDefaultVAO();
      glDrawArrays(GL_TRIANGLES, 0, 3);

      if(curFrame == 10)
      {
        // resize this VB up to size in the captured frame
        glClearBufferfv(GL_COLOR, 0, col);
        glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, GL_DYNAMIC_DRAW);
        ConfigureDefaultVAO();
        if(glGetError() == GL_NO_ERROR)
          glDrawArrays(GL_TRIANGLES, 0, 3);
        else
          glDrawArrays(GL_TRIANGLES, 0, 0);

        // resize this VB down to size in the captured frame
        glClearBufferfv(GL_COLOR, 0, col);
        glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, GL_DYNAMIC_DRAW);
        ConfigureDefaultVAO();
        if(glGetError() == GL_NO_ERROR)
          glDrawArrays(GL_TRIANGLES, 0, 3);
        else
          glDrawArrays(GL_TRIANGLES, 0, 0);

        // resize this VB several times in the captured frame
        glClearBufferfv(GL_COLOR, 0, col);
        glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
        glBufferData(GL_ARRAY_BUFFER, 16, NULL, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, 8, NULL, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, 8, NULL, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, 9999, NULL, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, GL_DYNAMIC_DRAW);
        ConfigureDefaultVAO();
        if(glGetError() == GL_NO_ERROR)
          glDrawArrays(GL_TRIANGLES, 0, 3);
        else
          glDrawArrays(GL_TRIANGLES, 0, 0);

        // resize down and map this VB
        glClearBufferfv(GL_COLOR, 0, col);
        glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), NULL, GL_DYNAMIC_DRAW);
        void *ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(ptr, DefaultTri, sizeof(DefaultTri));
        glUnmapBuffer(GL_ARRAY_BUFFER);
        ConfigureDefaultVAO();
        if(glGetError() == GL_NO_ERROR)
          glDrawArrays(GL_TRIANGLES, 0, 3);
        else
          glDrawArrays(GL_TRIANGLES, 0, 0);

        // resize up and map this VB
        glClearBufferfv(GL_COLOR, 0, col);
        glBindBuffer(GL_ARRAY_BUFFER, vbs[i++]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(DefaultTri), NULL, GL_DYNAMIC_DRAW);
        ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        memcpy(ptr, DefaultTri, sizeof(DefaultTri));
        glUnmapBuffer(GL_ARRAY_BUFFER);
        ConfigureDefaultVAO();
        if(glGetError() == GL_NO_ERROR)
          glDrawArrays(GL_TRIANGLES, 0, 3);
        else
          glDrawArrays(GL_TRIANGLES, 0, 0);

        // now trash the VBs that had important data at the start of the frame, to ensure that this
        // resize doesn't invalid any of the data that was in them and used.
        glBindBuffer(GL_ARRAY_BUFFER, vbs[0]);
        glBufferData(GL_ARRAY_BUFFER, 50, NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbs[1]);
        glBufferData(GL_ARRAY_BUFFER, 50, NULL, GL_DYNAMIC_DRAW);
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
