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

TEST(GL_Vertex_Attr_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle but using different kinds of vertex attributes, including doubles, arrays, "
      "and matrices.";

  struct vertin
  {
    int16_t i16[4];
    uint16_t u16[4];
    double df[2];
    float arr0[2];
    float arr1[2];
    float arr2[2];
    float mat0[2];
    float mat1[2];
  };

  std::string vertex = R"EOSHADER(
#version 450 core

layout(location = 0) in vec4 InSNorm;
layout(location = 1) in vec4 InUNorm;
layout(location = 2) in uvec4 InUInt;
layout(location = 3) in dvec2 InDouble;
layout(location = 4) in vec2 InArray[3];
layout(location = 7) in mat2x2 InMatrix;

layout(location = 0) out vec4 OutSNorm;
layout(location = 1) out vec4 OutUNorm;
layout(location = 2) out uvec4 OutUInt;
layout(location = 3) out dvec2 OutDouble;
layout(location = 4) out vec2 OutArray[3];
layout(location = 7) out mat2x2 OutMatrix;

void main()
{
  const vec4 verts[3] = vec4[3](vec4(-0.5, 0.5, 0.0, 1.0), vec4(0.0, -0.5, 0.0, 1.0),
                                vec4(0.5, 0.5, 0.0, 1.0));

  gl_Position = verts[gl_VertexID];

  OutSNorm = InSNorm;
  OutDouble = InDouble;
  OutUInt = InUInt;
  OutUNorm = InUNorm;
  OutArray = InArray;
  OutMatrix = InMatrix;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 450 core

layout(location = 0) in vec4 InSNorm;
layout(location = 1) in vec4 InUNorm;
layout(location = 2) flat in uvec4 InUInt;
layout(location = 3) flat in dvec2 InDouble;
layout(location = 4) in vec2 InArray[3];
layout(location = 7) in mat2x2 InMatrix;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  Color = vec4(0, 1.0f, 0, 1);

  // check values came through correctly

  // SNorm should be in [-1, 1]
  if(clamp(InSNorm, -1.0, 1.0) != InSNorm)
    Color = vec4(0.1f, 0, 0, 1);

  // UNorm should be in [0, 1]
  if(clamp(InUNorm, 0.0, 1.0) != InUNorm)
    Color = vec4(0.2f, 0, 0, 1);

  // Similar for UInt
  if(InUInt.x > 65535 || InUInt.y > 65535 || InUInt.z > 65535 || InUInt.w > 65535)
    Color = vec4(0.3f, 0, 0, 1);

  // doubles are all in range [-10, 10]
  if(clamp(InDouble, -10.0, 10.0) != InDouble)
    Color = vec4(0.4f, 0, 0, 1);
}

)EOSHADER";

  std::string geom = R"EOSHADER(
#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec4 InSNorm[3];
layout(location = 1) in vec4 InUNorm[3];
layout(location = 2) in uvec4 InUInt[3];
layout(location = 3) in dvec2 InDouble[3];
layout(location = 4) in vec2 InArray[3][3];
layout(location = 7) in mat2x2 InMatrix[3];

layout(location = 0) out vec4 OutSNorm;
layout(location = 1) out vec4 OutUNorm;
layout(location = 2) out uvec4 OutUInt;
layout(location = 3) out dvec2 OutDouble;
layout(location = 4) out vec2 OutArray[3];
layout(location = 7) out mat2x2 OutMatrix;

void main()
{
  for(int i = 0; i < 3; i++)
  {
    gl_Position = vec4(gl_in[i].gl_Position.yx, 0.4f, 1.2f);

    OutSNorm = InSNorm[i];
    OutDouble = InDouble[i];
    OutUInt = InUInt[i];
    OutUNorm = InUNorm[i];
    OutArray = InArray[i];
    OutMatrix = InMatrix[i];

    EmitVertex();
  }
  EndPrimitive();
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    vertin triangle[] = {
        {
            {32767, -32768, 32767, -32767},
            {12345, 6789, 1234, 567},
            {9.8765432109, -5.6789012345},
            {1.0f, 2.0f},
            {3.0f, 4.0f},
            {5.0f, 6.0f},
            {7.0, 8.0f},
            {9.0f, 10.0f},
        },
        {
            {32766, -32766, 16000, -16000},
            {56, 7890, 123, 4567},
            {-7.89012345678, 6.54321098765},
            {11.0f, 12.0f},
            {13.0f, 14.0f},
            {15.0f, 16.0f},
            {17.0, 18.0f},
            {19.0f, 20.0f},
        },
        {
            {5, -5, 0, 0},
            {8765, 43210, 987, 65432},
            {0.1234567890123, 4.5678901234},
            {21.0f, 22.0f},
            {23.0f, 24.0f},
            {25.0f, 26.0f},
            {27.0, 28.0f},
            {29.0f, 30.0f},
        },
    };

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(triangle), triangle, 0);

    // R16G16B16A16_SNORM
    glVertexAttribPointer(0, 4, GL_SHORT, GL_TRUE, sizeof(vertin), (void *)offsetof(vertin, i16));
    // R16G16B16A16_UNORM
    glVertexAttribPointer(1, 4, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(vertin),
                          (void *)offsetof(vertin, u16));
    // R16G16B16A16_UINT
    glVertexAttribIPointer(2, 4, GL_UNSIGNED_SHORT, sizeof(vertin), (void *)offsetof(vertin, u16));

    // R64G64_FLOAT
    glVertexAttribLPointer(3, 2, GL_DOUBLE, sizeof(vertin), (void *)offsetof(vertin, df));

    // Array[]
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(vertin), (void *)offsetof(vertin, arr0));
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(vertin), (void *)offsetof(vertin, arr1));
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(vertin), (void *)offsetof(vertin, arr2));

    // Matrix
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(vertin), (void *)offsetof(vertin, mat0));
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, sizeof(vertin), (void *)offsetof(vertin, mat1));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glEnableVertexAttribArray(7);
    glEnableVertexAttribArray(8);

    GLuint program = MakeProgram(vertex, pixel, geom);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

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
