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

TEST(GL_Resource_Lifetimes, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Test various edge-case resource lifetimes: a resource that is first dirtied within a frame "
      "so needs initial contents created for it, and a resource that is created and destroyed "
      "mid-frame (which also gets dirtied after use).";

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

out gl_PerVertex {
  vec4 gl_Position;
};

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

layout(binding = 0) uniform sampler2D checker;
layout(binding = 1) uniform sampler2D smiley;

layout(std140) uniform constsbuf
{
  vec4 flags;
};

uniform vec4 flags2;

void main()
{
  if(flags.x != 1.0f || flags.y != 2.0f || flags.z != 4.0f || flags.w != 8.0f)
  {
    Color = vec4(1.0f, 0.0f, 1.0f, 1.0f);
    return;
  }

  if(flags != flags2)
  {
    Color = vec4(0.5f, 0.0f, 0.5f, 1.0f);
    return;
  }

  Color = texture(smiley, vertIn.uv.xy * 2.0f) * texture(checker, vertIn.uv.xy * 5.0f);
  Color.w = 1.0f;
}

)EOSHADER";

  std::string dummy = R"EOSHADER(
#version 420 core

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), DefaultTri, 0);

    uint16_t indices[] = {0, 1, 2};
    GLuint ib = MakeBuffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, 0);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    GLuint offscreen = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, offscreen);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 128, 128);

    GLuint smiley = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, smiley);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, rgba8.width, rgba8.height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba8.width, rgba8.height, GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba8.data.data());

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, smiley);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint vsprog = MakeProgram(common + vertex, "");

    std::string fssrc = common + pixel;
    const char *fssrc_c = fssrc.c_str();

    const char *dummysrc_c = dummy.c_str();

    // function to set up a VAO
    auto SetupVAO = [ib]() {
      GLuint vao = 0;
      glGenVertexArrays(1, &vao);
      glBindVertexArray(vao);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                            (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);

      return vao;
    };

    // function to trash it again afterwards so that we are forced to reset the state to properly
    // replay
    auto TrashVAO = [](GLuint vao) {
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

      glDeleteVertexArrays(1, &vao);
    };

    // Sampler setup and trashing
    auto SetupSampler = []() {
      GLuint sampler = 0;
      glGenSamplers(1, &sampler);
      glBindSampler(1, sampler);

      glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_REPEAT);
      glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

      return sampler;
    };

    auto TrashSampler = [](GLuint sampler) {
      glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

      glDeleteSamplers(1, &sampler);
    };

    auto SetupProgram = [fssrc_c]() {
      GLuint prog = glCreateProgram();
      glProgramParameteri(prog, GL_PROGRAM_SEPARABLE, GL_TRUE);

      GLuint shad = glCreateShader(GL_FRAGMENT_SHADER);

      glShaderSource(shad, 1, &fssrc_c, NULL);
      glCompileShader(shad);

      glAttachShader(prog, shad);

      glLinkProgram(prog);

      glDetachShader(prog, shad);
      glDeleteShader(shad);

      glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "constsbuf"), 5);

      const Vec4f flags = {1.0f, 2.0f, 4.0f, 8.0f};

      glProgramUniform4fv(prog, glGetUniformLocation(prog, "flags2"), 1, &flags.x);

      return prog;
    };

    auto TrashProgram = [dummysrc_c](GLuint prog) {
      glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "constsbuf"), 4);

      const Vec4f empty = {};
      glProgramUniform4fv(prog, glGetUniformLocation(prog, "flags2"), 1, &empty.x);

      glDeleteProgram(prog);
    };

    // Program pipeline setup and trashing
    auto SetupPipe = [vsprog](GLuint prog) {
      GLuint pipe = 0;
      glGenProgramPipelines(1, &pipe);
      glBindProgramPipeline(pipe);

      glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vsprog);
      glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, prog);

      return pipe;
    };

    auto TrashPipe = [](GLuint pipe) {
      glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, 0);
      glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, 0);

      glDeleteProgramPipelines(1, &pipe);
    };

    auto SetupFBO = [offscreen]() {
      GLuint fbo = 0;
      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);

      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, offscreen, 0);
      GLenum col0 = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &col0);

      return fbo;
    };

    auto TrashFBO = [](GLuint fbo) {
      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);

      GLenum col0 = GL_NONE;
      glDrawBuffers(1, &col0);

      glDeleteFramebuffers(1, &fbo);
    };

    auto SetupTex = []() {
      const uint32_t checker[4 * 4] = {
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
      };

      GLuint tex = 0;
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, checker);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

      return tex;
    };

    auto TrashTex = [](GLuint tex) {
      const uint32_t empty[4 * 4] = {};
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, empty);

      glDeleteTextures(1, &tex);
    };

    auto SetupBuf = []() {
      const float empty = {};
      const Vec4f flags = {1.0f, 2.0f, 4.0f, 8.0f};

      GLuint buf = 0;
      glGenBuffers(1, &buf);
      glBindBuffer(GL_UNIFORM_BUFFER, buf);
      glBufferData(GL_UNIFORM_BUFFER, sizeof(empty), &empty, GL_STATIC_DRAW);

      glBufferData(GL_UNIFORM_BUFFER, sizeof(flags), &flags, GL_STATIC_DRAW);

      glBindBufferBase(GL_UNIFORM_BUFFER, 5, buf);

      return buf;
    };

    auto TrashBuf = [](GLuint buf) {
      const float empty = {};
      glBufferData(GL_UNIFORM_BUFFER, sizeof(empty), &empty, GL_STATIC_DRAW);

      glDeleteBuffers(1, &buf);
    };

    glUseProgram(0);
    glViewport(0, 0, 128, 128);

    GLuint fbo = SetupFBO();
    GLuint vao = SetupVAO();
    GLuint sampler = SetupSampler();
    GLuint fsprog = SetupProgram();
    GLuint pipe = SetupPipe(fsprog);
    GLuint tex = SetupTex();
    GLuint buf = SetupBuf();
    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearNamedFramebufferfv(0, GL_COLOR, 0, col);

      // render with last frame's resources
      float col1[] = {0.5f, 0.1f, 0.1f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col1);

      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
      glBlitNamedFramebuffer(fbo, 0, 0, 0, 128, 128, 0, 0, 128, 128, GL_COLOR_BUFFER_BIT, GL_NEAREST);

      // trash last frame's resources
      TrashFBO(fbo);
      TrashVAO(vao);
      TrashSampler(sampler);
      TrashProgram(fsprog);
      TrashPipe(pipe);
      TrashTex(tex);
      TrashBuf(buf);

      // create resources mid-frame and use then trash them
      fbo = SetupFBO();
      vao = SetupVAO();
      sampler = SetupSampler();
      fsprog = SetupProgram();
      pipe = SetupPipe(fsprog);
      tex = SetupTex();
      buf = SetupBuf();
      float col2[] = {0.1f, 0.1f, 0.5f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col2);

      glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
      glBlitNamedFramebuffer(fbo, 0, 0, 0, 128, 128, 128, 0, 256, 128, GL_COLOR_BUFFER_BIT,
                             GL_NEAREST);
      TrashFBO(fbo);
      TrashVAO(vao);
      TrashSampler(sampler);
      TrashProgram(fsprog);
      TrashPipe(pipe);
      TrashTex(tex);
      TrashBuf(buf);

      // set up resources for next frame
      fbo = SetupFBO();
      vao = SetupVAO();
      sampler = SetupSampler();
      fsprog = SetupProgram();
      pipe = SetupPipe(fsprog);
      tex = SetupTex();
      buf = SetupBuf();

      Present();
    }

    // destroy resources
    TrashFBO(fbo);
    TrashVAO(vao);
    TrashSampler(sampler);
    TrashProgram(fsprog);
    TrashPipe(pipe);
    TrashBuf(buf);

    return 0;
  }
};

REGISTER_TEST();
