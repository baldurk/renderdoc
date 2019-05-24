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

TEST(GL_Entry_Points, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Test that RenderDoc correctly lists the different function call aliases used.";

  std::string vertex = R"EOSHADER(
#version 420 core

layout(location = 0) in vec3 Position;

void main()
{
	gl_Position = vec4(Position.xyz, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

uniform uint path;
uniform vec4 a;

void main()
{
  if(path == 1u)
	  Color = a;
  else
    Color = vec4(1.0, 0.0, 1.0, 1.0);
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
    glEnableVertexAttribArray(0);

    GLuint program = MakeProgram(vertex, pixel);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(100), GLsizei(100));
      setMarker("First Test");
      glUniform1ui(glGetUniformLocation(program, "path"), 0);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glViewport(100, 0, GLsizei(100), GLsizei(100));
      glUniform1ui(glGetUniformLocation(program, "path"), 1);
      setMarker("Second Test");
      glVertexAttribBinding(1, 1);
      glProgramUniform4f(program, glGetUniformLocation(program, "a"), 0.0f, 1.0f, 1.0f, 1.0f);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glViewport(200, 0, GLsizei(100), GLsizei(100));
      setMarker("Third Test");
      glVertexArrayAttribBinding(vao, 1, 1);
      glUniform4f(glGetUniformLocation(program, "a"), 1.0f, 1.0f, 0.0f, 1.0f);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
