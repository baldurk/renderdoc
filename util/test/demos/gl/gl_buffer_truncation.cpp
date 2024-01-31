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

RD_TEST(GL_Buffer_Truncation, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests using a uniform buffer that is truncated by range, as well as "
      "vertex/index buffers truncated by size.";

  const std::string vertex = R"EOSHADER(
#version 460 core

layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec4 COLOR;

layout(location = 0) out vec4 OUTPOSITION;
layout(location = 1) out vec4 OUTCOLOR;

void main()
{
	gl_Position = OUTPOSITION = vec4(POSITION.xyz, 1);
	OUTCOLOR = COLOR;
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0) in vec4 OUTPOSITION;
layout(location = 1) in vec4 OUTCOLOR;

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0, std140) uniform constsbuf
{
  vec4 padding[16];
  vec4 outcol;
};

void main()
{
	Color = outcol + 1e-6f * OUTPOSITION + 1e-6f * OUTCOLOR;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const DefaultA2V OffsetTri[] = {
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(9.9f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(8.8f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };
    uint16_t indices[] = {99, 99, 99, 1, 2, 3, 4, 5};
    Vec4f cbufferdata[64] = {};
    cbufferdata[32] = Vec4f(1.0f, 2.0f, 3.0f, 4.0f);

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(OffsetTri), OffsetTri, 0);

    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vec3f));
    glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vec3f) + sizeof(Vec4f));

    glVertexAttribBinding(0, 0);
    glVertexAttribBinding(1, 0);
    glVertexAttribBinding(2, 0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint ib = MakeBuffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, 0);

    GLuint program = MakeProgram(vertex, pixel);

    GLuint cb = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, cb);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(cbufferdata), cbufferdata, GL_MAP_WRITE_BIT);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint colattach = MakeTexture();

    glBindTexture(GL_TEXTURE_2D, colattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colattach, 0);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glBindVertexArray(vao);

      glBindBufferRange(GL_UNIFORM_BUFFER, 0, cb, 16 * sizeof(Vec4f), 16 * sizeof(Vec4f));

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glBindVertexBuffer(0, vb, sizeof(DefaultA2V) * 3, sizeof(DefaultA2V));

      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (void *)(sizeof(uint16_t) * 3));

      blitToSwap(colattach);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
