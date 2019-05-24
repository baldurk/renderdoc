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

TEST(GL_Separable_Geometry_Shaders, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Draws using geometry shadess and separable programs created with glCreateShaderProgramv";

  std::string common = R"EOSHADER(

#version 420 core

#define v2f v2f_block \
{                     \
	vec4 col;           \
	vec4 uv;            \
}

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

out v2f vertOut;

out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
};

void main()
{
	gl_Position = vec4(Position.xyz, 1);
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string geom = R"EOSHADER(

in v2f vertIn[3];
out v2f vertOut;

in gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
}
gl_in[];

out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
};

layout(triangles) in;
layout(triangle_strip, max_vertices = 9) out;

void main()
{
  for(int i=0; i < 3; i++)
  {
    gl_Position = gl_in[i].gl_Position + vec4(0.7, 0.0, 0.0, 0.0);
    vertOut.col = vertIn[i].col;
    vertOut.uv = vertIn[i].uv;
    EmitVertex();
  }

  EndPrimitive();

  for(int i=0; i < 3; i++)
  {
    gl_Position = gl_in[i].gl_Position + vec4(-0.7, 0.0, 0.0, 0.0);
    vertOut.col = vec4(1.0)-vertIn[i].col;
    vertOut.uv = vertIn[i].uv;
    EmitVertex();
  }

  EndPrimitive();

  for(int i=0; i < 3; i++)
  {
    gl_Position = gl_in[i].gl_Position + vec4(0.0, 0.7, 0.0, 0.0);
    vertOut.col = vertIn[i].col.yzxw;
    vertOut.uv = vertIn[i].uv;
    EmitVertex();
  }

  EndPrimitive();
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
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    std::string a = common + vertex;
    const char *str = a.c_str();
    GLuint vs = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &str);

    a = common + pixel;
    str = a.c_str();
    GLuint fs = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &str);

    a = common + geom;
    str = a.c_str();
    GLuint gs = glCreateShaderProgramv(GL_GEOMETRY_SHADER, 1, &str);

    GLint status = 0;
    char buffer[1024];
    glGetProgramiv(gs, GL_LINK_STATUS, &status);
    if(status == 0)
    {
      glGetProgramInfoLog(gs, 1024, NULL, buffer);
      TEST_ERROR("Link error: %s", buffer);
    }

    GLuint pipe = MakePipeline();

    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vs);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fs);
    glUseProgramStages(pipe, GL_GEOMETRY_SHADER_BIT, gs);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glBindProgramPipeline(pipe);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    glDeleteProgram(vs);
    glDeleteProgram(fs);
    glDeleteProgram(gs);

    return 0;
  }
};

REGISTER_TEST();
