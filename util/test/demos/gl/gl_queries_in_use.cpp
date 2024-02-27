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

#include "3rdparty/fmt/core.h"
#include "gl_test.h"

RD_TEST(GL_Queries_In_Use, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests that we can still fetch mesh output and queries even when the capture itself makes "
      "uses of those features.";

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

    GLuint program = MakeProgram();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    {
      const char *cstr = GLDefaultVertex.c_str();
      glShaderSource(vs, 1, &cstr, NULL);
      glCompileShader(vs);
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    {
      const char *cstr = GLDefaultPixel.c_str();
      glShaderSource(fs, 1, &cstr, NULL);
      glCompileShader(fs);
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);

    const GLchar *posOnly = "gl_Position";
    glTransformFeedbackVaryings(program, 1, &posOnly, GL_INTERLEAVED_ATTRIBS);

    glLinkProgram(program);

    glDetachShader(program, vs);
    glDetachShader(program, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status;
    char buffer[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(status == 0)
    {
      glGetProgramInfoLog(program, 1024, NULL, buffer);
      TEST_ERROR("Link error: %s", buffer);

      glDeleteProgram(program);
      program = 0;
    }

    GLuint xfb;
    glGenTransformFeedbacks(1, &xfb);

    GLuint xfbBuffer = MakeBuffer();
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfbBuffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1024, NULL, GL_DYNAMIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfbBuffer);

    GLuint queries[4];
    glGenQueries(4, queries);

    while(Running())
    {
      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
      glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfbBuffer);
      glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, queries[0]);

      glBeginTransformFeedback(GL_TRIANGLES);

      setMarker("XFB Draw");
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glEndTransformFeedback();
      glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

      GLuint primsWritten = 0;
      glGetQueryObjectuiv(queries[0], GL_QUERY_RESULT, &primsWritten);

      Vec4f vert;
      glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(Vec4f), &vert);

      setMarker(fmt::format("XFBResult: {} prims, first vert {},{},{},{}", primsWritten, vert.x,
                            vert.y, vert.z, vert.w));

      setMarker("Counters Draw");
      glBeginQuery(GL_CLIPPING_OUTPUT_PRIMITIVES_ARB, queries[1]);
      glBeginQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB, queries[2]);
      glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB, queries[3]);

      glDrawArrays(GL_TRIANGLES, 0, 3);

      glEndQuery(GL_CLIPPING_OUTPUT_PRIMITIVES_ARB);
      glEndQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB);
      glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB);

      glGetQueryObjectuiv(queries[1], GL_QUERY_RESULT, &primsWritten);
      glGetQueryObjectuiv(queries[2], GL_QUERY_RESULT, &vs);
      glGetQueryObjectuiv(queries[3], GL_QUERY_RESULT, &fs);

      setMarker(fmt::format("CounterResult: {} prims, {} vs {} fs", primsWritten, vs, fs));

      Present();
    }

    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteQueries(4, queries);

    return 0;
  }
};

REGISTER_TEST();
