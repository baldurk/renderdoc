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

RD_TEST(GL_VAO_0, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Uses VAO 0 (i.e. never binds a VAO) as well as testing client memory pointer behaviour.";

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

uniform vec4 instance_xform;

void main()
{
	vertOut.pos = vec4(Position.x + float(gl_InstanceID % 4), Position.y + float(gl_InstanceID / 4), Position.z, 1) * vec4(instance_xform.xy, 1, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
}

)EOSHADER";

  int main()
  {
    coreProfile = false;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    uint32_t idxs[3] = {0, 1, 2};

    for(int i = 0; i < 100; i++)
    {
      GraphicsWindow *win2 = MakeWindow(32, 32, "extra");
      void *ctx2 = MakeContext(win2, mainContext);
      ActivateContext(win2, ctx2);
      ActivateContext(mainWindow, mainContext);
      DestroyContext(ctx2);
      delete win2;
    }

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint ib = MakeBuffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * 3, idxs, 0);

    GLuint program = MakeProgram(common + vertex, common + pixel);

    GLint loc = glGetUniformLocation(program, "instance_xform");

    glEnable(GL_SCISSOR_TEST);

    while(Running())
    {
      glScissor(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      // try to delete VAO 0. Should do nothing
      GLuint zero = 0;
      glDeleteVertexArrays(1, &zero);

      // same with FBO 0
      glDeleteFramebuffers(1, &zero);

      glUseProgram(program);

      glUniform4f(loc, 1.0f, 1.0f, 0.0f, 0.0f);

      // use both buffers
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
      glBindBuffer(GL_ARRAY_BUFFER, vb);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                            (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

      glViewport(0, 0, GLsizei(screenWidth) / 4, GLsizei(screenHeight));
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);

      // use direct pointers for indices
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      glViewport(screenWidth / 4, 0, GLsizei(screenWidth) / 4, GLsizei(screenHeight));
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, idxs);

      // use direct pointers for vertices
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);

      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), &DefaultTri[0].pos);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), &DefaultTri[0].col);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), &DefaultTri[0].uv);

      glViewport(screenWidth / 2, 0, GLsizei(screenWidth) / 4, GLsizei(screenHeight));
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);

      // use direct pointers for both
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      glViewport((screenWidth * 3) / 4, 0, GLsizei(screenWidth) / 4, GLsizei(screenHeight));
      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, idxs);

      // draw with instance data that requires more data than a non-instanced stream would need
      glViewport(0, 0, GLsizei(screenWidth) / 2, GLsizei(screenHeight) / 2);
      glScissor(0, 0, GLsizei(screenWidth) / 2, GLsizei(screenHeight) / 2);
      glClearBufferfv(GL_COLOR, 0, col);

      Vec4f instcols[20] = {};
      for(size_t i = 4; i < ARRAY_COUNT(instcols); i++)
        instcols[i].z = 0.5f * (i - 3);

      glUniform4f(loc, 0.25f, 0.25f, 0.0f, 0.0f);

      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vec4f), instcols);
      glVertexAttribDivisor(1, 1);

      glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_INT, idxs, 16, 4);

      glVertexAttribDivisor(1, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
