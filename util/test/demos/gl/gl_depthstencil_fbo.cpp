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

TEST(GL_DepthStencil_FBO, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Creates a depth-stencil FBO and writes both depth and stencil to it";

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

void main()
{
	Color = vertIn.col;
}

)EOSHADER";

  std::string copypixel = R"EOSHADER(

layout (binding = 0) uniform usampler2D stencilAttach;
layout (binding = 1) uniform sampler2D colorAttach;

in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	if(gl_FragCoord.x > gl_FragCoord.y*2.0)
	{
		Color = texelFetch(colorAttach, ivec2(gl_FragCoord.xy), 0);
	}
	else
	{
		uint stencil = texelFetch(stencilAttach, ivec2(gl_FragCoord.xy), 0).x;

		if(stencil > 50U)
			Color = vec4(0, 1, 0, 1);
		else
			Color = vec4(1, 0, 0, 1);
	}
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

    GLuint copyprogram = MakeProgram(common + vertex, common + copypixel);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint attachments[] = {MakeTexture(), MakeTexture(), MakeTexture()};

    glBindTexture(GL_TEXTURE_2D, attachments[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, attachments[0], 0);

    glBindTexture(GL_TEXTURE_2D, attachments[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           attachments[1], 0);

    GLuint view = MakeTexture();
    glTextureView(view, GL_TEXTURE_2D, attachments[1], GL_DEPTH24_STENCIL8, 0, 1, 0, 1);

    glTextureParameteri(view, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);

    glDepthFunc(GL_ALWAYS);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glStencilFunc(GL_ALWAYS, 0xcc, 0xff);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
      glDrawBuffers(1, bufs);

      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);
      glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

      glBindVertexArray(vao);

      glUseProgram(program);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 0, 3);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      glUseProgram(copyprogram);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, view);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, attachments[0]);

      glDrawArrays(GL_TRIANGLES, 0, 3);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, 0);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
