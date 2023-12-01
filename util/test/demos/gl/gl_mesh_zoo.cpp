/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

RD_TEST(GL_Mesh_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description = "Draws some primitives for testing the mesh view.";

  std::string common = R"EOSHADER(

#version 450 core

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;

layout(binding = 0, std140) uniform constsbuf
{
  vec4 scale;
  vec4 offset;
};

layout(location = 0) out vec2 col2;
layout(location = 1) out vec4 col;

void main()
{
	vec4 pos = vec4(Position.xy * scale.xy + offset.xy, Position.z, 1.0f);
	col = Color;

  if(gl_InstanceID > 0)
  {
    pos *= 0.3f;
    pos.xy += vec2(0.1f);
    col.x = 1.0f;
  }

  col2 = pos.xy;
	gl_Position = pos;
}

)EOSHADER";

  std::string multivertex = R"EOSHADER(
#version 460 core

layout(location = 0) out vec2 col2;
layout(location = 1) out vec4 col;
flat out uint basevtx;
flat out uint baseinst;
flat out uint draw;
flat out uint inst;
flat out uint vert;

void main()
{
  const vec4 verts[3] = vec4[3](vec4(-0.5, 0.5, 0.0, 1.0), vec4(0.0, -0.5, 0.0, 1.0),
                                vec4(0.5, 0.5, 0.0, 1.0));

  gl_Position = verts[gl_VertexID%3];
  col = vec4(0, 1, 1, 1);
  col2 = vec2(0.5, 0.5);

  basevtx = gl_BaseVertex;
  baseinst = gl_BaseInstance;
  draw = gl_DrawID;
  inst = gl_InstanceID;
  vert = gl_VertexID;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

layout(location = 0) in vec2 col2;
layout(location = 1) in vec4 col;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = col + 1.0e-20 * col2.xyxy;
}

)EOSHADER";

  std::string nopvertex = R"EOSHADER(
#version 420 core

void main()
{
}

)EOSHADER";

  std::string geometry = R"EOSHADER(
#version 420 core

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) out vec2 col2;
layout(location = 1) out vec4 col;

void main()
{
  const vec4 verts[3] = vec4[3](vec4(-0.4, -0.4, 0.5, 1.0), vec4(0.6, -0.6, 0.5, 1.0),
                                vec4(-0.5, 0.5, 0.5, 1.0));

  for(int i=0; i < 3; i++)
  {
    gl_Position = verts[i];
    col = vec4(1, 0, 0, 1);
    col2 = vec2(1, 0);
    EmitVertex();
  }

  EndPrimitive();
}

)EOSHADER";

  int main()
  {
    glMinor = 6;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    const DefaultA2V test[] = {
        // single color quad
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        // points, to test vertex picking
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(70.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(170.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(70.0f, 70.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(test), test, 0);

    Vec4f cbufferdata[] = {
        Vec4f(2.0f / (float)screenWidth, 2.0f / (float)screenHeight, 1.0f, 1.0f),
        Vec4f(-1.0f, -1.0f, 0.0f, 0.0f),
    };

    GLuint cb = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, cb);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(cbufferdata), cbufferdata, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, cb);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint stride0vao = MakeVAO();
    glBindVertexArray(stride0vao);

    // need to specify this using modern bindings, glVertexAttribPointer stride 0 is interpreted as
    // 'tightly packed'
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, offsetof(DefaultA2V, pos));
    glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, offsetof(DefaultA2V, col));
    glVertexAttribFormat(2, 3, GL_FLOAT, GL_FALSE, offsetof(DefaultA2V, uv));

    glVertexAttribBinding(0, 0);
    glVertexAttribBinding(1, 0);
    glVertexAttribBinding(2, 0);

    glBindVertexBuffer(0, vb, 0, 0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(common + vertex, common + pixel);

    GLuint geomprogram = MakeProgram(nopvertex, common + pixel, geometry);

    GLuint multiprogram = MakeProgram(multivertex, common + pixel);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint attachments[] = {MakeTexture(), MakeTexture(), MakeTexture()};

    glBindTexture(GL_TEXTURE_2D, attachments[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, attachments[0], 0);

    glBindTexture(GL_TEXTURE_2D, attachments[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           attachments[1], 0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_STENCIL_TEST);

    struct DrawElementsIndirectCommand
    {
      uint32_t count;
      uint32_t instanceCount;
      uint32_t firstIndex;
      int32_t baseVertex;
      uint32_t baseInstance;
    };

    GLuint cmdBuf = MakeBuffer();
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, cmdBuf);
    glBufferStorage(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawElementsIndirectCommand) * 4, NULL,
                    GL_DYNAMIC_STORAGE_BIT);

    GLuint countBuf = MakeBuffer();
    glBindBuffer(GL_PARAMETER_BUFFER, countBuf);
    glBufferStorage(GL_PARAMETER_BUFFER, sizeof(uint32_t), NULL, GL_DYNAMIC_STORAGE_BIT);

    uint32_t indices[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    GLuint idxBuf = MakeBuffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idxBuf);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, 0);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);
      glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 10, 3);

      setMarker("Quad");

      glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 2);

      setMarker("Points");

      glDrawArrays(GL_POINTS, 6, 4);

      setMarker("Lines");

      glDrawArrays(GL_LINES, 6, 4);

      setMarker("Stride 0");

      glBindVertexArray(stride0vao);

      glDrawArrays(GL_POINTS, 0, 1);

      setMarker("Geom Only");

      glUseProgram(geomprogram);

      glDrawArrays(GL_POINTS, 0, 1);

      setMarker("Multi Draw");

      glUseProgram(multiprogram);

      DrawElementsIndirectCommand cmd = {};
      cmd.count = 3;
      cmd.instanceCount = 2;
      cmd.baseVertex = 10;
      cmd.baseInstance = 20;

      uint32_t count = 2;
      glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(cmd), &cmd);

      cmd.instanceCount = 4;
      cmd.baseVertex = 11;
      cmd.baseInstance = 22;

      glBufferSubData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), sizeof(cmd), &cmd);
      glBufferSubData(GL_PARAMETER_BUFFER, 0, sizeof(count), &count);

      glMultiDrawElementsIndirectCount(GL_TRIANGLES, GL_UNSIGNED_INT, NULL, (GLintptr)0, 4,
                                       sizeof(DrawElementsIndirectCommand));

      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight,
                        GL_COLOR_BUFFER_BIT, GL_LINEAR);

      setMarker("Empty");

      glDrawArrays(GL_TRIANGLES, 0, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
