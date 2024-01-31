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

RD_TEST(GL_Shader_Editing, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that shader editing works with different combinations of shader re-use and handles "
      "locations that change between the pre-edit and post-edit shaders.";

  const char *vertex = R"EOSHADER(
#version 430 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

out v2f vertOut;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  const char *pixel = R"EOSHADER(
#version 430 core

layout(location = 0, index = 0) out vec4 Color;

layout(location = 9) uniform vec4 col;

void main()
{
	Color = col.rgba;
}

)EOSHADER";

  const char *pixel2 = R"EOSHADER(
#version 430 core

layout(location = 0, index = 0) out vec4 Color;

// we hope that having these uniforms be first both alphabetically, by use, and by declaration, that
// they'll be assigned earlier locations.
// Then when we remove the declration and use it should force zcol to get a lower location value
// after the edit.
#if 1
uniform vec4 acol;
uniform vec4 bcol;
uniform vec4 ccol;
#endif
uniform vec4 zcol;

void main()
{
  Color = vec4(0);
#if 1
  Color += acol + bcol + ccol;
#endif
	Color += zcol.rgba;
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

    GLuint fixedprog = MakeProgram();
    GLuint dynamicprog = MakeProgram();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex, NULL);
    glCompileShader(vs);

    GLuint fs1 = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs1, 1, &pixel, NULL);
    glCompileShader(fs1);

    GLuint fs2 = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs2, 1, &pixel2, NULL);
    glCompileShader(fs2);

    glAttachShader(fixedprog, vs);
    glAttachShader(fixedprog, fs1);
    glLinkProgram(fixedprog);
    glDetachShader(fixedprog, vs);
    glDetachShader(fixedprog, fs1);

    glAttachShader(dynamicprog, vs);
    glAttachShader(dynamicprog, fs2);
    glLinkProgram(dynamicprog);
    glDetachShader(dynamicprog, vs);
    glDetachShader(dynamicprog, fs2);

    glDeleteShader(vs);
    glDeleteShader(fs1);
    glDeleteShader(fs2);

    GLuint pipe = MakePipeline();

    GLuint vssepprog = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vertex);
    GLuint fssepprog = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &pixel);

    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vssepprog);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fssepprog);

    // force the pipeline to be dirty
    for(int i = 0; i < 100; i++)
    {
      glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vssepprog);
      glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fssepprog);
    }

    glProgramUniform4f(fssepprog, 9, 0.0f, 1.0f, 0.0f, 1.0f);

    // render offscreen to make picked values accurate
    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint colattach = MakeTexture();

    glBindTexture(GL_TEXTURE_2D, colattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colattach, 0);

    GLuint zcol = glGetUniformLocation(dynamicprog, "zcol");

    while(Running())
    {
      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glUseProgram(fixedprog);

      GLsizei hw = GLsizei(screenWidth) / 2;
      GLsizei hh = GLsizei(screenHeight) / 2;

      glViewport(0, hh, hw, hh);

      glUniform4f(9, 0.0f, 1.0f, 0.0f, 1.0f);
      glUniform4f(10, 1.0f, 0.0f, 0.0f, 1.0f);

      glDrawArrays(GL_TRIANGLES, 0, 3);

      glViewport(hw, hh, hw, hh);

      glUniform4f(9, 0.0f, 0.5f, 0.0f, 1.0f);
      glUniform4f(10, 0.5f, 0.0f, 0.0f, 1.0f);

      setMarker("fixedprog");
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glViewport(0, 0, hw, hh);

      glUseProgram(dynamicprog);
      glUniform4f(zcol, 0.0f, 1.0f, 0.0f, 1.0f);
      setMarker("dynamicprog");
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glViewport(hw, 0, hw, hh);

      // finally draw with the separable pipeline to ensure we can edit that
      glBindProgramPipeline(pipe);
      glUseProgram(0);
      setMarker("sepprog");
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glBindProgramPipeline(0);

      // give us a point to select where all uniforms are trashed
      glUseProgram(fixedprog);
      glUniform4f(9, 0.0f, 0.0f, 0.0f, 1.0f);
      glUniform4f(10, 0.0f, 0.0f, 0.0f, 1.0f);
      glUseProgram(dynamicprog);
      glUniform4f(zcol, 0.0f, 0.0f, 0.0f, 1.0f);

      glBlitNamedFramebuffer(fbo, 0, 0, 0, screenWidth, screenHeight, 0, 0, screenWidth,
                             screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
