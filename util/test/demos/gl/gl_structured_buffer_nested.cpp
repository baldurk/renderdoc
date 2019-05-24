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

TEST(GL_Structured_Buffer_Nested, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Just draws a simple triangle, using normal pipeline. Basic test that can be used "
      "for any dead-simple tests that don't require any particular API use";

  std::string common = R"EOSHADER(

#version 430 core

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

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

struct supernest
{
  float x;
};

struct nest
{
  vec3 v;
  supernest s;
  float a, b, c;
};

layout(binding = 0, std430) buffer nest_struct_buffer
{
  nest n[3];
  vec4 p;
  nest rtarray[];
} nestbuf;

layout(binding = 1) uniform samplerBuffer plainbuf;

layout(binding = 2, std430) buffer struct_buffer
{
  nest rtarray[];
} structbuf;

layout(binding = 3, std430) buffer output_buffer
{
  vec4 dump[];
} out_buf;

void main()
{
  int idx = 0;
  out_buf.dump[idx++] = vec4(nestbuf.n[0].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.n[1].a, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.n[2].c, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.n[2].s.x, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = nestbuf.p;
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[0].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[3].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[6].v, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[4].a, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[5].b, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[7].c, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(nestbuf.rtarray[8].s.x, 0.0f, 0.0f, 1.0f);
  idx++;
  out_buf.dump[idx++] = texelFetch(plainbuf, 3);
  out_buf.dump[idx++] = texelFetch(plainbuf, 4);
  out_buf.dump[idx++] = texelFetch(plainbuf, 5);
  idx++;
  out_buf.dump[idx++] = vec4(structbuf.rtarray[0].v, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[3].v, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[6].v, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[4].a, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[5].b, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[7].c, 0.0f, 0.0f, 1.0f);
  out_buf.dump[idx++] = vec4(structbuf.rtarray[8].s.x, 0.0f, 0.0f, 1.0f);
	Color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
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

    GLuint program = MakeProgram(common + vertex, common + pixel);

    float data[16 * 100];

    for(int i = 0; i < 16 * 100; i++)
      data[i] = float(i);

    GLuint buf = MakeBuffer();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(data), data, 0);

    GLuint outbuf = MakeBuffer();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, outbuf);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, 1024, NULL, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outbuf);

    GLuint tbuf_store = MakeBuffer();
    glBindBuffer(GL_TEXTURE_BUFFER, tbuf_store);
    glBufferStorage(GL_TEXTURE_BUFFER, sizeof(data), data, 0);

    GLuint tbuf = MakeTexture();
    glBindTexture(GL_TEXTURE_BUFFER, tbuf);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbuf_store);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, tbuf);

    while(Running())
    {
      float zeros[4] = {};
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, outbuf);
      glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_RGBA32F, 0, 1024, GL_RGBA, GL_FLOAT, zeros);

      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);

      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
