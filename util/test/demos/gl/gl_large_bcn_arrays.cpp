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

TEST(GL_Large_BCn_Arrays, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Test creating large texture 2D arrays of BC4, BC5, BC6, BC7 textures";

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

    const int width = 4;
    const int height = 4;
    const int numMips = 1;

    int activeTex = GL_TEXTURE0;

    GLuint texs[2][4];

    GLenum fmts[] = {GL_COMPRESSED_RED_RGTC1, GL_COMPRESSED_RG_RGTC2,
                     GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, GL_COMPRESSED_RGBA_BPTC_UNORM};

    for(int arraySize = 1; arraySize <= 2; arraySize++)
    {
      GLenum texbind = arraySize > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;

      const char *names[] = {
          arraySize > 1 ? "BC4 array" : "BC4", arraySize > 1 ? "BC5 array" : "BC5",
          arraySize > 1 ? "BC6 array" : "BC6", arraySize > 1 ? "BC7 array" : "BC7",
      };

      for(int fmt = 0; fmt < 4; fmt++)
      {
        glActiveTexture(activeTex);
        activeTex++;

        GLuint tex = texs[arraySize - 1][fmt] = MakeTexture();

        glBindTexture(texbind, tex);
        if(texbind == GL_TEXTURE_2D_ARRAY)
          glTexStorage3D(texbind, numMips, fmts[fmt], width, height, arraySize);
        else
          glTexStorage2D(texbind, numMips, fmts[fmt], width, height);

        // force renderdoc to late-fetch the texture contents, and not serialise the
        // subimage data calls below
        for(int blah = 0; blah < 100; blah++)
          glTexParameteri(texbind, GL_TEXTURE_MAX_LEVEL, numMips - 1);

        glObjectLabel(GL_TEXTURE, tex, -1, names[fmt]);

        int w = width;
        int h = height;

        for(int mip = 0; mip < numMips; mip++)
        {
          byte *foo = new byte[w * h * arraySize];
          for(int len = 0; len < w * h * arraySize; len++)
            foo[len] = rand() & 0xff;

          GLsizei size = w * h * arraySize;
          // BC4 is 0.5 bytes per pixel
          if(fmt == 0)
            size /= 2;

          if(texbind == GL_TEXTURE_2D_ARRAY)
            glCompressedTexSubImage3D(texbind, mip, 0, 0, 0, w, h, arraySize, fmts[fmt], size, foo);
          else
            glCompressedTexSubImage2D(texbind, mip, 0, 0, w, h, fmts[fmt], size, foo);

          w = w >> 1;
          h = h >> 1;

          delete[] foo;
        }
      }
    }

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
