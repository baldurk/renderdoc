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

RD_TEST(GL_Discard_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description = "Tests texture discard methods in GL.";

  byte empty[16 * 1024 * 1024] = {};

  uint32_t greens10_2[300 * 300];

  void SetDebugName(GLuint t, std::string name)
  {
    glObjectLabel(GL_TEXTURE, t, -1, name.c_str());
  }
  void Clear(GLuint t)
  {
    GLenum fmt = GL_NONE;
    GLint mips = 0;
    GLint width = 0;
    GLint height = 0;

    glGetTextureLevelParameteriv(t, 0, GL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
    glGetTextureParameteriv(t, GL_TEXTURE_IMMUTABLE_LEVELS, &mips);
    glGetTextureLevelParameteriv(t, 0, GL_TEXTURE_WIDTH, &width);
    glGetTextureLevelParameteriv(t, 0, GL_TEXTURE_HEIGHT, &height);

    if(fmt == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || fmt == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
       fmt == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT || fmt == GL_COMPRESSED_RED_RGTC1 ||
       fmt == GL_COMPRESSED_RG_RGTC2 || fmt == GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB ||
       fmt == GL_COMPRESSED_RGBA_BPTC_UNORM_ARB)
    {
      GLsizei size = 16;

      if(fmt == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || fmt == GL_COMPRESSED_RED_RGTC1)
        size = 8;

      size *= (width / 4) * (height / 4);

      // can't clear compressed tex image
      for(GLint m = 0; m < mips; m++)
        glCompressedTextureSubImage2D(t, m, 0, 0, width, height, fmt, size, empty);

      return;
    }

    if(fmt == GL_RGB10_A2UI)
    {
      for(GLint m = 0; m < mips; m++)
        glTextureSubImage2D(t, m, 0, 0, width, height, GL_RGBA_INTEGER,
                            GL_UNSIGNED_INT_2_10_10_10_REV, greens10_2);
    }
    else if(fmt == GL_DEPTH_COMPONENT32F)
    {
      float depth = 0.4f;
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    }
    else if(fmt == GL_DEPTH32F_STENCIL8)
    {
      struct
      {
        float depth;
        uint32_t stencil;
      } ds;
      ds.depth = 0.4f;
      ds.stencil = 0x40;
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, &ds);
    }
    else if(fmt == GL_DEPTH24_STENCIL8)
    {
      uint32_t depth_stencil = 0x40666666;
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &depth_stencil);
    }
    else if(fmt == GL_STENCIL_INDEX8)
    {
      uint32_t stencil = 0x40;
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_STENCIL_INDEX, GL_UNSIGNED_INT, &stencil);
    }
    else if(fmt == GL_RGBA16UI)
    {
      uint16_t green[] = {0, 127, 0, 1};
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, &green);
    }
    else if(fmt == GL_RGBA16I)
    {
      uint16_t green[] = {0, 127, 0, 1};
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_RGBA_INTEGER, GL_SHORT, &green);
    }
    else
    {
      Vec4f green(0.0f, 1.0f, 0.0f, 1.0f);
      for(GLint m = 0; m < mips; m++)
        glClearTexImage(t, m, GL_RGBA, GL_FLOAT, &green);
    }
  }

  void Invalidate(GLuint t)
  {
    GLint mips = 1;
    glGetTextureParameteriv(t, GL_TEXTURE_IMMUTABLE_LEVELS, &mips);

    for(GLint m = 0; m < mips; m++)
      glInvalidateTexImage(t, m);
  }

  void InvalidateFBO(const std::vector<GLenum> &atts, int x = 0, int y = 0, int width = 0,
                     int height = 0)
  {
    if(width == 0)
      glInvalidateFramebuffer(GL_FRAMEBUFFER, (GLsizei)atts.size(), atts.data());
    else
      glInvalidateSubFramebuffer(GL_FRAMEBUFFER, (GLsizei)atts.size(), atts.data(), x, y, width,
                                 height);
  }

  GLuint MakeTex2D(GLenum fmt, GLuint width, GLuint height, GLsizei mips = 1)
  {
    GLuint ret = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, ret);
    glTexStorage2D(GL_TEXTURE_2D, mips, fmt, width, height);
    return ret;
  }

  GLuint MakeTex2DArray(GLenum fmt, GLuint width, GLuint height, GLuint slices, GLsizei mips = 1)
  {
    GLuint ret = MakeTexture();
    glBindTexture(GL_TEXTURE_2D_ARRAY, ret);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, mips, fmt, width, height, slices);
    return ret;
  }

  GLuint MakeTex2DMS(GLenum fmt, GLuint width, GLuint height, GLsizei samples, GLuint slices = 1)
  {
    GLuint ret = MakeTexture();
    if(slices == 1)
    {
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ret);
      glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, fmt, width, height, GL_TRUE);
    }
    else
    {
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, ret);
      glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples, fmt, width, height,
                                slices, GL_TRUE);
    }
    return ret;
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    for(size_t i = 0; i < 300 * 300; i++)
      greens10_2[i] = 0xC00FFC00;

    std::vector<GLuint> texs, fbos;

#define TEX_TEST(name, x)                                                          \
  if(first)                                                                        \
  {                                                                                \
    texs.push_back(x);                                                             \
    Clear(texs.back());                                                            \
    SetDebugName(texs.back(), "Tex" + std::to_string(texs.size()) + ": " + +name); \
  }                                                                                \
  tex = texs[t++];

#define FBO_TEST()             \
  if(first)                    \
  {                            \
    fbos.push_back(MakeFBO()); \
  }                            \
  fbo = fbos[f++];             \
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    memset(empty, 0x88, sizeof(empty));

    GLuint buf = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, buf);
    glBufferStorage(GL_UNIFORM_BUFFER, 1024, NULL, GL_DYNAMIC_STORAGE_BIT);
    glObjectLabel(GL_BUFFER, buf, -1, "Buffer");

    GLuint subbuf = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, subbuf);
    glBufferStorage(GL_UNIFORM_BUFFER, 1024, NULL, GL_DYNAMIC_STORAGE_BIT);
    glObjectLabel(GL_BUFFER, subbuf, -1, "BufferSub");

    GLuint tex1d = MakeTexture();
    glBindTexture(GL_TEXTURE_1D_ARRAY, tex1d);
    glTexStorage2D(GL_TEXTURE_1D_ARRAY, 3, GL_RGBA16F, 300, 5);
    GLuint tex3d = MakeTexture();
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA16F, 300, 300, 15);
    GLuint tex1dsub = MakeTexture();
    glBindTexture(GL_TEXTURE_1D_ARRAY, tex1dsub);
    glTexStorage2D(GL_TEXTURE_1D_ARRAY, 3, GL_RGBA16F, 300, 5);
    GLuint tex3dsub = MakeTexture();
    glBindTexture(GL_TEXTURE_3D, tex3dsub);
    glTexStorage3D(GL_TEXTURE_3D, 3, GL_RGBA16F, 300, 300, 15);
    GLuint tex3dsub2 = MakeTexture();
    glBindTexture(GL_TEXTURE_3D, tex3dsub2);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, 300, 300, 15);

    GLuint texcube = MakeTexture();
    glBindTexture(GL_TEXTURE_CUBE_MAP, texcube);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA16F, 300, 300);
    GLuint texcubesub = MakeTexture();
    glBindTexture(GL_TEXTURE_CUBE_MAP, texcubesub);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA16F, 300, 300);

    GLuint rb = 0;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA16F, 300, 300);

    glObjectLabel(GL_TEXTURE, tex1d, -1, "Tex1D: DiscardAll");
    glObjectLabel(GL_TEXTURE, tex3d, -1, "Tex3D: DiscardAll");
    glObjectLabel(GL_TEXTURE, tex1dsub, -1, "Tex1D: DiscardRect Mip1 Slice1,2");
    glObjectLabel(GL_TEXTURE, tex3dsub, -1, "Tex3D: DiscardRect Mip1 Slice1,2");
    glObjectLabel(GL_TEXTURE, texcube, -1, "TexCube: DiscardAll");
    glObjectLabel(GL_TEXTURE, texcubesub, -1, "TexCube: DiscardAll Slice2");
    glObjectLabel(GL_TEXTURE, tex3dsub2, -1, "Tex3D: DiscardRect Slice7");
    glObjectLabel(GL_RENDERBUFFER, rb, -1, "RB: DiscardAll");

    GLuint tmpfbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, tmpfbo);

    bool first = true;

    while(Running())
    {
      if(!first)
      {
        pushMarker("Clears");
        for(GLuint t : texs)
          Clear(t);

        Vec4f green(0.0f, 1.0f, 0.0f, 1.0f);

        glBindFramebuffer(GL_FRAMEBUFFER, tmpfbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
        glClearBufferfv(GL_COLOR, 0, &green.x);

        for(int m = 0; m < 3; m++)
        {
          glClearTexImage(tex1d, m, GL_RGBA, GL_FLOAT, &green.x);
          glClearTexImage(tex3d, m, GL_RGBA, GL_FLOAT, &green.x);
          glClearTexImage(tex1dsub, m, GL_RGBA, GL_FLOAT, &green.x);
          glClearTexImage(tex3dsub, m, GL_RGBA, GL_FLOAT, &green.x);
        }

        glClearTexImage(texcube, 0, GL_RGBA, GL_FLOAT, &green.x);
        glClearTexImage(texcubesub, 0, GL_RGBA, GL_FLOAT, &green.x);
        glClearTexImage(tex3dsub2, 0, GL_RGBA, GL_FLOAT, &green.x);

        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 1024, empty);
        glBindBuffer(GL_ARRAY_BUFFER, subbuf);
        glBufferSubData(GL_ARRAY_BUFFER, 0, 1024, empty);
        popMarker();
      }

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};

      setMarker("TestStart");
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glClearBufferfv(GL_COLOR, 0, col);

      glInvalidateBufferData(buf);
      glInvalidateBufferSubData(subbuf, 50, 75);

      int t = 0, f = 0;
      GLuint tex, fbo;

      // test a few different formats
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGBA16F, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGB10_A2, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGB10_A2UI, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGB9_E5, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGBA8, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RED_RGTC1, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RG_RGTC2, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, 300, 300));
      Invalidate(tex);

      // test with different mips/array sizes
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGBA16F, 300, 300, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DArray(GL_RGBA16F, 300, 300, 4));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DArray(GL_RGBA16F, 300, 300, 4, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGBA16F, 30, 5));
      Invalidate(tex);

      // test MSAA textures
      TEX_TEST("DiscardAll", MakeTex2DMS(GL_RGBA16F, 300, 300, 4));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(GL_RGBA16F, 300, 300, 4, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(GL_RGBA16UI, 300, 300, 4, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(GL_RGBA16I, 300, 300, 4, 5));
      Invalidate(tex);

      // test depth textures
      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH_COMPONENT32F, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH24_STENCIL8, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_STENCIL_INDEX8, 300, 300));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH_COMPONENT32F, 300, 300, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DArray(GL_DEPTH_COMPONENT32F, 300, 300, 4));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DArray(GL_DEPTH_COMPONENT32F, 300, 300, 4, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DArray(GL_DEPTH32F_STENCIL8, 300, 300, 4));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DArray(GL_DEPTH32F_STENCIL8, 300, 300, 4, 5));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(GL_DEPTH32F_STENCIL8, 300, 300, 4));
      Invalidate(tex);
      TEX_TEST("DiscardAll", MakeTex2DMS(GL_DEPTH32F_STENCIL8, 300, 300, 4, 5));
      Invalidate(tex);

      // test discarding rects within a texture
      TEX_TEST("DiscardRect Mip0", MakeTex2D(GL_RGBA16F, 300, 300));
      glInvalidateTexSubImage(tex, 0, 50, 50, 0, 75, 75, 1);
      TEX_TEST("DiscardRect Mip1", MakeTex2D(GL_RGBA16F, 300, 300, 2));
      glInvalidateTexSubImage(tex, 1, 50, 50, 0, 75, 75, 1);

      TEX_TEST("DiscardRect Mip0", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300));
      glInvalidateTexSubImage(tex, 0, 50, 50, 0, 75, 75, 1);
      TEX_TEST("DiscardRect Mip1", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300, 2));
      glInvalidateTexSubImage(tex, 1, 50, 50, 0, 75, 75, 1);

      TEX_TEST("DiscardAll Slice2", MakeTex2DMS(GL_RGBA16F, 300, 300, 4, 5));
      glInvalidateTexSubImage(tex, 0, 0, 0, 2, 300, 300, 1);

      // test 1D/3D/Cube textures
      Invalidate(tex1d);
      Invalidate(tex3d);
      Invalidate(texcube);

      glInvalidateTexSubImage(tex1dsub, 1, 50, 1, 0, 75, 2, 1);
      glInvalidateTexSubImage(tex3dsub, 1, 50, 50, 1, 75, 75, 2);

      // test invalidating framebuffer attachments
      TEX_TEST("DiscardAll", MakeTex2D(GL_RGBA16F, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_COLOR_ATTACHMENT0});

      TEX_TEST("DiscardRect", MakeTex2D(GL_RGBA16F, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_COLOR_ATTACHMENT0}, 50, 50, 75, 75);

      // test invalidating depth and stencil components in different combinations
      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH_COMPONENT32F, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_DEPTH_ATTACHMENT});

      TEX_TEST("DiscardAll", MakeTex2D(GL_STENCIL_INDEX8, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_STENCIL_ATTACHMENT});

      TEX_TEST("DiscardAll", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_DEPTH_STENCIL_ATTACHMENT});

      TEX_TEST("DiscardAll DepthOnly", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_DEPTH_ATTACHMENT});

      TEX_TEST("DiscardAll StencilOnly", MakeTex2D(GL_DEPTH32F_STENCIL8, 300, 300));
      FBO_TEST();
      glBindTexture(GL_TEXTURE_2D, tex);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
      InvalidateFBO({GL_STENCIL_ATTACHMENT});

      FBO_TEST();
      glBindTexture(GL_TEXTURE_CUBE_MAP, texcubesub);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                             texcubesub, 0);
      InvalidateFBO({GL_COLOR_ATTACHMENT0});

      FBO_TEST();
      glBindTexture(GL_TEXTURE_3D, tex3dsub2);
      glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, tex3dsub2, 0, 7);
      InvalidateFBO({GL_COLOR_ATTACHMENT0});

      FBO_TEST();
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
      InvalidateFBO({GL_COLOR_ATTACHMENT0});

      glFlush();

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      setMarker("TestEnd");
      glClearBufferfv(GL_COLOR, 0, col);

      Present();

      first = false;
    }

    glDeleteRenderbuffers(1, &rb);

    return 0;
  }
};

REGISTER_TEST();
