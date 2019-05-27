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

TEST(GL_Per_Type_Tex_Units, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "GL lets each type (2D, 3D, Cube) have a different binding to the same texture unit. This "
      "test uses that in various ways that might cause problems if tracking doesn't accurately "
      "account for that.";

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

layout(binding = 2) uniform sampler2D tex2;
layout(binding = 3) uniform sampler3D tex3;

void main()
{
	Color = texture(tex2, vertIn.uv.xy)*vec4(1.0f, 0.1f, 0.1f, 0.1f) +
          texture(tex3, vertIn.uv.xyz)*vec4(0.1f, 1.0f, 0.1f, 0.1f);
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

    GLuint tex2d = MakeTexture();
    GLuint tex3d = MakeTexture();

    uint32_t green[4 * 4 * 4], red[8 * 8];
    for(int i = 0; i < 4 * 4 * 4; i++)
      green[i] = 0xff00ff00;
    for(int i = 0; i < 8 * 8; i++)
      red[i] = 0xff0000ff;

    // be explicit, all this happens on slot 0
    glActiveTexture(GL_TEXTURE0);

    // bind tex3d to the 3D target, then clear the 3D target
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glBindTexture(GL_TEXTURE_2D, 0);

    // allocate storage and upload on 3D - even though 2D was the last bound target
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 4, 4, 4);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, green);

    // now do the same in reverse
    glBindTexture(GL_TEXTURE_2D, tex2d);
    glBindTexture(GL_TEXTURE_3D, 0);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, red);

    // unbind both
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);

    glObjectLabel(GL_TEXTURE, tex2d, -1, "Red 2D");
    glObjectLabel(GL_TEXTURE, tex3d, -1, "Green 3D");

    GLuint program = MakeProgram(common + vertex, common + pixel);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      glUseProgram(program);

      // bind both textures to both slots, only the 'right' one will be used by GL. To be extra
      // clear, bind the intended texture first, then 'overwrite' (which doesn't overwrite) with the
      // wrong one.
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, tex2d);
      glBindTexture(GL_TEXTURE_3D, tex3d);

      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_3D, tex3d);
      glBindTexture(GL_TEXTURE_2D, tex2d);

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
