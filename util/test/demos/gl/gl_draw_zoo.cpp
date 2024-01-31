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

RD_TEST(GL_Draw_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Draws several variants using different vertex/index offsets.";

  std::string vertex = R"EOSHADER(
#version 420 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec4 COLOR;
layout(location = 1) out vec4 TEXCOORD;
layout(location = 2) out float VID;
layout(location = 3) out float IID;

void main()
{
	gl_Position = vec4(Position.xyz, 1);
  gl_Position.x += Color.w;
	COLOR = Color;
	TEXCOORD = vec4(UV.xy, 0, 1);

  VID = float(gl_VertexID);
  IID = float(gl_InstanceID);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 420 core

layout(location = 0) in vec4 COLOR;
layout(location = 1) in vec4 TEXCOORD;
layout(location = 2) in float VID;
layout(location = 3) in float IID;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vec4(0,0,0,0);
  Color.r = VID;
  Color.g = IID;
  Color.b = COLOR.w;
  Color.a = COLOR.g + TEXCOORD.x;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    DefaultA2V triangle[] = {
        // 0
        {Vec3f(-1.0f, -1.0f, -1.0f), Vec4f(1.0f, 1.0f, 1.0f, 0.0f), Vec2f(-1.0f, -1.0f)},
        // 1, 2, 3
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // 4, 5, 6
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // 7, 8, 9
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // 10, 11, 12
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // strips: 13, 14, 15, ...
        {Vec3f(-0.5f, 0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.2f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.2f, 0.0f), Vec4f(0.4f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.3f, 0.0f, 0.0f), Vec4f(0.6f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.2f, 0.0f), Vec4f(0.8f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.0f, 0.0f), Vec4f(1.0f, 0.5f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.1f, 0.2f, 0.0f), Vec4f(0.0f, 0.8f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.1f, 0.0f, 0.0f), Vec4f(0.2f, 0.1f, 0.5f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.3f, 0.2f, 0.0f), Vec4f(0.4f, 0.3f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.3f, 0.0f, 0.0f), Vec4f(0.6f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, 0.2f, 0.0f), Vec4f(0.8f, 0.3f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
    };

    std::vector<DefaultA2V> vbData;
    vbData.resize(600);

    {
      DefaultA2V *src = (DefaultA2V *)triangle;
      DefaultA2V *dst = (DefaultA2V *)&vbData[0];

      // up-pointing src to offset 0
      memcpy(dst + 0, src + 1, sizeof(DefaultA2V));
      memcpy(dst + 1, src + 2, sizeof(DefaultA2V));
      memcpy(dst + 2, src + 3, sizeof(DefaultA2V));

      // invalid vert for index 3 and 4
      memcpy(dst + 3, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 4, src + 0, sizeof(DefaultA2V));

      // down-pointing src at offset 5
      memcpy(dst + 5, src + 4, sizeof(DefaultA2V));
      memcpy(dst + 6, src + 5, sizeof(DefaultA2V));
      memcpy(dst + 7, src + 6, sizeof(DefaultA2V));

      // invalid vert for 8 - 12
      memcpy(dst + 8, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 9, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 10, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 11, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 12, src + 0, sizeof(DefaultA2V));

      // left-pointing src data to offset 13
      memcpy(dst + 13, src + 7, sizeof(DefaultA2V));
      memcpy(dst + 14, src + 8, sizeof(DefaultA2V));
      memcpy(dst + 15, src + 9, sizeof(DefaultA2V));

      // invalid vert for 16-22
      memcpy(dst + 16, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 17, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 18, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 19, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 20, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 21, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 22, src + 0, sizeof(DefaultA2V));

      // right-pointing src data to offset 23
      memcpy(dst + 23, src + 10, sizeof(DefaultA2V));
      memcpy(dst + 24, src + 11, sizeof(DefaultA2V));
      memcpy(dst + 25, src + 12, sizeof(DefaultA2V));

      // strip after 30
      memcpy(dst + 30, src + 13, sizeof(DefaultA2V));
      memcpy(dst + 31, src + 14, sizeof(DefaultA2V));
      memcpy(dst + 32, src + 15, sizeof(DefaultA2V));
      memcpy(dst + 33, src + 16, sizeof(DefaultA2V));
      memcpy(dst + 34, src + 17, sizeof(DefaultA2V));
      memcpy(dst + 35, src + 18, sizeof(DefaultA2V));
      memcpy(dst + 36, src + 19, sizeof(DefaultA2V));
      memcpy(dst + 37, src + 20, sizeof(DefaultA2V));
      memcpy(dst + 38, src + 21, sizeof(DefaultA2V));
      memcpy(dst + 39, src + 22, sizeof(DefaultA2V));
      memcpy(dst + 40, src + 23, sizeof(DefaultA2V));
      memcpy(dst + 41, src + 24, sizeof(DefaultA2V));
    }

    for(size_t i = 0; i < vbData.size(); i++)
    {
      vbData[i].uv.x = float(i);
      vbData[i].col.y = float(i) / 200.0f;
    }

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, vbData.size() * sizeof(DefaultA2V), vbData.data(), 0);

    Vec4f instData[16] = {};
    for(int i = 0; i < ARRAY_COUNT(instData); i++)
      instData[i] = Vec4f(-100.0f, -100.0f, -100.0f, -100.0f);

    {
      instData[0] = Vec4f(0.0f, 0.4f, 1.0f, 0.0f);
      instData[1] = Vec4f(0.5f, 0.5f, 0.0f, 0.5f);

      instData[5] = Vec4f(0.0f, 0.6f, 0.5f, 0.0f);
      instData[6] = Vec4f(0.5f, 0.7f, 1.0f, 0.5f);

      instData[13] = Vec4f(0.0f, 0.8f, 0.3f, 0.0f);
      instData[14] = Vec4f(0.5f, 0.9f, 0.1f, 0.5f);
    }

    GLuint instvb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, instvb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(instData), instData, 0);

    std::vector<uint16_t> idxData;
    idxData.resize(100);

    {
      idxData[0] = 0;
      idxData[1] = 1;
      idxData[2] = 2;

      idxData[5] = 5;
      idxData[6] = 6;
      idxData[7] = 7;

      idxData[13] = 63;
      idxData[14] = 64;
      idxData[15] = 65;

      idxData[23] = 103;
      idxData[24] = 104;
      idxData[25] = 105;

      idxData[37] = 104;
      idxData[38] = 105;
      idxData[39] = 106;

      idxData[42] = 30;
      idxData[43] = 31;
      idxData[44] = 32;
      idxData[45] = 33;
      idxData[46] = 34;
      idxData[47] = 0xffff;
      idxData[48] = 36;
      idxData[49] = 37;
      idxData[50] = 38;
      idxData[51] = 39;
      idxData[52] = 40;
      idxData[53] = 41;

      idxData[54] = 130;
      idxData[55] = 131;
      idxData[56] = 132;
      idxData[57] = 133;
      idxData[58] = 134;
      idxData[59] = 0xffff;
      idxData[60] = 136;
      idxData[61] = 137;
      idxData[62] = 138;
      idxData[63] = 139;
      idxData[64] = 140;
      idxData[65] = 141;
    }

    GLuint ib = MakeBuffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, idxData.size() * sizeof(uint16_t), idxData.data(), 0);

    glBindVertexArray(vao);

    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vec3f));
    glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vec3f) + sizeof(Vec4f));

    glVertexAttribBinding(0, 0);
    glVertexAttribBinding(1, 0);
    glVertexAttribBinding(2, 0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint instvao = MakeVAO();
    glBindVertexArray(instvao);

    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribDivisor(1, 1);
    glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vec3f) + sizeof(Vec4f));

    glVertexAttribBinding(0, 0);
    glVertexAttribBinding(1, 1);
    glVertexAttribBinding(2, 0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(vertex, pixel);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint colattach = MakeTexture();

    glBindTexture(GL_TEXTURE_2D, colattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colattach, 0);

    // Depth texture
    GLuint depthattach = MakeTexture();

    glBindTexture(GL_TEXTURE_2D, depthattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthattach,
                           0);
    glClearDepth(0.0f);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      setMarker("GL_ClearDepth");
      glClear(GL_DEPTH_BUFFER_BIT);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glBindVertexBuffer(0, instvb, 0, sizeof(Vec4f));

      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);

      glUseProgram(program);

      GLint w = 48, h = 48;
      GLint x = 0, y = screenHeight - h;

      glViewport(x, y, w, h);

      setMarker("Test Begin");

      ///////////////////////////////////////////////////
      // non-indexed, non-instanced

      // basic test
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawArrays(GL_TRIANGLES, 0, 3);
      x += w;

      // test with vertex offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawArrays(GL_TRIANGLES, 5, 3);
      x += w;

      // test with vertex offset and vbuffer offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 5 * sizeof(DefaultA2V), sizeof(DefaultA2V));
      glDrawArrays(GL_TRIANGLES, 8, 3);
      x += w;

      // adjust to next row
      x = 0;
      y -= h;

      ///////////////////////////////////////////////////
      // indexed, non-instanced

      // basic test
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void *)(0 * sizeof(uint16_t)), 0);
      x += w;

      // test with first index
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void *)(5 * sizeof(uint16_t)), 0);
      x += w;

      // test with first index and vertex offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void *)(13 * sizeof(uint16_t)),
                               -50);
      x += w;

      // test with first index and vertex offset and vbuffer offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 10 * sizeof(DefaultA2V), sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void *)(23 * sizeof(uint16_t)),
                               -100);
      x += w;

      // GL can't have an ibuffer offset, so first index & ibuffer offset are merged
      // test with first index and vertex offset and vbuffer offset and ibuffer offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 19 * sizeof(DefaultA2V), sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
                               (void *)((14 + 23) * sizeof(uint16_t)), -100);
      x += w;

      glEnable(GL_PRIMITIVE_RESTART);
      glPrimitiveRestartIndex(0xffff);

      setMarker("GL_PRIMITIVE_RESTART");

      // indexed strip with primitive restart
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLE_STRIP, 12, GL_UNSIGNED_SHORT,
                               (void *)(42 * sizeof(uint16_t)), 0);
      x += w;

      glDisable(GL_PRIMITIVE_RESTART);
      glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

      setMarker("GL_PRIMITIVE_RESTART_FIXED_INDEX");

      // indexed strip with primitive restart and vertex offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glDrawElementsBaseVertex(GL_TRIANGLE_STRIP, 12, GL_UNSIGNED_SHORT,
                               (void *)(54 * sizeof(uint16_t)), -100);
      x += w;

      // adjust to next row
      x = 0;
      y -= h;

      glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

      glBindVertexArray(instvao);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);

      ///////////////////////////////////////////////////
      // non-indexed, instanced

      // basic test
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glBindVertexBuffer(1, instvb, 0, sizeof(Vec4f));
      glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 2, 0);
      x += w;

      // basic test with first instance
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 5 * sizeof(DefaultA2V), sizeof(DefaultA2V));
      glBindVertexBuffer(1, instvb, 0, sizeof(Vec4f));
      glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 2, 5);
      x += w;

      // basic test with first instance and instance buffer offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 13 * sizeof(DefaultA2V), sizeof(DefaultA2V));
      glBindVertexBuffer(1, instvb, 8 * sizeof(Vec4f), sizeof(Vec4f));
      glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, 2, 5);
      x += w;

      // adjust to next row
      x = 0;
      y -= h;

      ///////////////////////////////////////////////////
      // indexed, instanced

      // basic test
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glBindVertexBuffer(1, instvb, 0, sizeof(Vec4f));
      glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
                                                    (void *)(5 * sizeof(uint16_t)), 2, 0, 0);
      x += w;

      // basic test with first instance
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glBindVertexBuffer(1, instvb, 0, sizeof(Vec4f));
      glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
                                                    (void *)(13 * sizeof(uint16_t)), 2, -50, 5);
      x += w;

      // basic test with first instance and instance buffer offset
      glViewport(x, y, w, h);
      glBindVertexBuffer(0, vb, 0, sizeof(DefaultA2V));
      glBindVertexBuffer(1, instvb, 8 * sizeof(Vec4f), sizeof(Vec4f));
      glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT,
                                                    (void *)(23 * sizeof(uint16_t)), 2, -80, 5);
      x += w;

      blitToSwap(colattach);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
