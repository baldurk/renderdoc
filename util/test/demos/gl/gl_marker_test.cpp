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

RD_TEST(GL_Marker_Test, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests all variants of OpenGL marker functions to ensure they are recorded correctly.";

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

    while(Running())
    {
      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glPushGroupMarkerEXT(-1, "EXT marker 1");
      glPushGroupMarkerEXT(0, "EXT marker 2");
      glPushGroupMarkerEXT(12, "EXT marker 3foobar");

      glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "KHR marker 1");
      glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, 0, "KHR marker 2");
      glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, 12, "KHR marker 3foobar");

      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Core marker 1");
      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, 0, "Core marker 2");
      glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, 13, "Core marker 3foobar");

      glInsertEventMarkerEXT(-1, "EXT event 1");
      glInsertEventMarkerEXT(0, "EXT event 2");
      glInsertEventMarkerEXT(11, "EXT event 3foobar");

      glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                              GL_DEBUG_SEVERITY_NOTIFICATION, -1, "KHR event 1");
      glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, "KHR event 2");
      glDebugMessageInsertKHR(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 11, "KHR event 3foobar");

      glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                           GL_DEBUG_SEVERITY_NOTIFICATION, -1, "Core event 1");
      glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                           GL_DEBUG_SEVERITY_NOTIFICATION, 0, "Core event 2");
      glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                           GL_DEBUG_SEVERITY_NOTIFICATION, 12, "Core event 3foobar");

      glStringMarkerGREMEDY(-1, "GREMEDY event 1");
      glStringMarkerGREMEDY(0, "GREMEDY event 2");
      glStringMarkerGREMEDY(15, "GREMEDY event 3foobar");

      glDrawArrays(GL_TRIANGLES, 0, 3);

      glPopDebugGroup();
      glPopDebugGroup();
      glPopDebugGroup();

      glPopDebugGroupKHR();
      glPopDebugGroupKHR();
      glPopDebugGroupKHR();

      glPopGroupMarkerEXT();
      glPopGroupMarkerEXT();
      glPopGroupMarkerEXT();

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
