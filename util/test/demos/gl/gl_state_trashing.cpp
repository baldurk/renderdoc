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

RD_TEST(GL_State_Trashing, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that implicit shadowed GL state isn't trashed by initial states or capture overlay.";

  std::string pixel = R"EOSHADER(
#version 420 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

layout(binding = 0, std140) uniform constsbuf
{
  vec4 tint;
};

uniform vec4 tint2;

void main()
{
	Color = vertIn.col * tint * tint2;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    // only time we bind the VAO, to ensure VAO state isn't trashed
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    // only time we bind the array buffer
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultTri), NULL, GL_DYNAMIC_STORAGE_BIT);

    GLuint ubo = MakeBuffer();
    // only time we bind the uniform buffer
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(DefaultTri), NULL, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);

    GLuint program = MakeProgram(GLDefaultVertex, pixel);
    glUseProgram(program);

    GLint loc = glGetUniformLocation(program, "tint2");

    uint32_t empty[1024] = {};

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // create some things to force different types of initial states
    GLuint texs[] = {MakeTexture(), MakeTexture(), MakeTexture()};
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texs[0]);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB16F, screenWidth, screenHeight,
                            false);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
    glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_DEPTH_COMPONENT24, screenWidth,
                            screenHeight, 6, false);

    glBindTexture(GL_TEXTURE_3D, texs[2]);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, 64, 48, 16, 0, GL_RGBA, GL_FLOAT, NULL);

    GLuint samp = 0;
    glGenSamplers(1, &samp);

    GLuint pipe = MakePipeline();
    GLuint sepprog = MakeProgram("", GLDefaultPixel);

    // force things to be dirty
    for(int i = 0; i < 100; i++)
    {
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DefaultTri), DefaultTri);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(DefaultTri), DefaultTri);

      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texs[0], 0);
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texs[1], 0, 0);

      glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, sepprog);

      glSamplerParameterf(samp, GL_TEXTURE_LOD_BIAS, 0.0f);

      glEnableVertexAttribArray(0);
    }

    while(Running())
    {
      // forcibly reference all objects to ensure we prepare AND serialise their initial contents
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texs[0]);
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
      glBindTexture(GL_TEXTURE_3D, texs[2]);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);

      glBindProgramPipeline(pipe);
      glBindProgramPipeline(0);

      glBindSampler(6, samp);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      // configure the VAO. If state tracking has been corrupted this won't modify the right VAO
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                            (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glEnableVertexAttribArray(2);

      // upload the data to the implicit buffer binding - same thing as above this won't modify the
      // right buffer.
      Vec4f tint = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DefaultTri), DefaultTri);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Vec4f), &tint);

      // set the bare uniform
      glUniform4f(loc, 1.0f, 1.0f, 1.0f, 1.0f);

      glDrawArrays(GL_TRIANGLES, 0, 3);

      // trash everything so we don't get the state saved as initial contents
      glUniform4f(loc, 0.0f, 0.0f, 0.0f, 0.0f);

      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DefaultTri), empty);
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(DefaultTri), empty);

      glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 64, (void *)(0));
      glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 64, (void *)(0));
      glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 64, (void *)(0));

      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);

      Present();
    }

    glDeleteSamplers(1, &samp);

    return 0;
  }
};

REGISTER_TEST();
