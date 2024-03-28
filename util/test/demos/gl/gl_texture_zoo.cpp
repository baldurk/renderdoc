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

using namespace TextureZoo;

RD_TEST(GL_Texture_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests all possible combinations of texture type and format that are supported.";

  std::string blitVertex = R"EOSHADER(
#version 420 core

void main()
{
  const vec4 verts[4] = vec4[4](vec4(-1.0, -1.0, 0.5, 1.0), vec4(3.0, -1.0, 0.5, 1.0),
                                vec4(-1.0, 3.0, 0.5, 1.0), vec4(1.0, 1.0, 0.5, 1.0));

  gl_Position = verts[gl_VertexID];
}

)EOSHADER";

  std::string pixelTemplate = R"EOSHADER(
#version 420 core

layout(binding = 0) uniform &texdecl intex;

layout(location = 0, index = 0) out vec4 Color;

vec4 cubeFetch(samplerCube t, int i)
{
  return textureLod(t, vec3(1,0,0), 0.0);
}

vec4 cubeFetch(samplerCubeArray t, int i)
{
  return textureLod(t, vec4(1,0,0,0), 0.0);
}

vec4 cubeFetch(usamplerCube t, int i)
{
  return textureLod(t, vec3(1,0,0), 0.0);
}

vec4 cubeFetch(usamplerCubeArray t, int i)
{
  return textureLod(t, vec4(1,0,0,0), 0.0);
}

vec4 cubeFetch(isamplerCube t, int i)
{
  return textureLod(t, vec3(1,0,0), 0.0);
}

vec4 cubeFetch(isamplerCubeArray t, int i)
{
  return textureLod(t, vec4(1,0,0,0), 0.0);
}

void main()
{
	Color = vec4(texelFetch(intex, &params));
}
)EOSHADER";

  std::string pixelMSFloat = R"EOSHADER(
#version 420 core

uniform uint texWidth;
uniform uint slice;
uniform uint mip;
uniform uint flags;
uniform uint zlayer;

float srgb2linear(float f)
{
  if (f <= 0.04045f)
    return f / 12.92f;
  else
    return pow((f + 0.055f) / 1.055f, 2.4f);
}

layout(location = 0, index = 0) out vec4 Color;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  vec4 ret = vec4(0.1f, 0.35f, 0.6f, 0.85f);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + zlayer) % max(1u, texWidth >> mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += 0.075f.xxxx * (gl_SampleID + mip);

  // Signed normals are negative
  if((flags & 1) != 0)
    ret = -ret;

  // undo SRGB curve applied in output merger, to match the textures we just blat values into
  // without conversion (which are then interpreted as srgb implicitly)
  if((flags & 2) != 0)
  {
    ret.r = srgb2linear(ret.r);
    ret.g = srgb2linear(ret.g);
    ret.b = srgb2linear(ret.b);
  }

  // BGR flip - same as above, for BGRA textures
  if((flags & 4) != 0)
    ret.rgb = ret.bgr;

   // put red into alpha, because that's what we did in manual upload
  if((flags & 8) != 0)
    ret.a = ret.r;

  Color = ret;
}

)EOSHADER";

  std::string pixelMSDepth = R"EOSHADER(
#version 420 core

uniform uint texWidth;
uniform uint slice;
uniform uint mip;
uniform uint flags;
uniform uint zlayer;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  float ret = 0.1f;

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + zlayer) % max(1u, texWidth >> mip);

  // pixels off the diagonal invert the colors
  // second slice adds a coarse checkerboard pattern of inversion
  if((offs_x != y) != (slice > 0 && (((x / 2) % 2) != ((y / 2) % 2))))
  {
    ret = 0.85f;

    // so we can fill stencil data, clip off the inverted values
    if(flags == 1)
      discard;
  }

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += 0.075f * (gl_SampleID + mip);

  gl_FragDepth = ret;
}

)EOSHADER";

  std::string pixelMSUInt = R"EOSHADER(
#version 420 core

uniform uint texWidth;
uniform uint slice;
uniform uint mip;
uniform uint flags;
uniform uint zlayer;

layout(location = 0, index = 0) out uvec4 Color;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  uvec4 ret = uvec4(10, 40, 70, 100);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + zlayer) % max(1u, texWidth >> mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += uvec4(10, 10, 10, 10) * (gl_SampleID + mip);

  Color = ret;
}

)EOSHADER";

  std::string pixelMSSInt = R"EOSHADER(
#version 420 core

uniform uint texWidth;
uniform uint slice;
uniform uint mip;
uniform uint flags;
uniform uint zlayer;

layout(location = 0, index = 0) out ivec4 Color;

void main()
{
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);

  ivec4 ret = ivec4(10, 40, 70, 100);

  // each 3D slice cycles the x. This only affects the primary diagonal
  uint offs_x = (x + zlayer) % max(1u, texWidth >> mip);

  // pixels off the diagonal invert the colors
  if(offs_x != y)
    ret = ret.wzyx;

  // second slice adds a coarse checkerboard pattern of inversion
  if(slice > 0 && (((x / 2) % 2) != ((y / 2) % 2)))
    ret = ret.wzyx;

  // second sample/mip is shifted up a bit. MSAA textures have no mips,
  // textures with mips have no samples.
  ret += ivec4(10 * (gl_SampleID + mip));

  Color = -ret;
}

)EOSHADER";

  struct GLFormat
  {
    const std::string name;
    GLenum internalFormat;
    TexConfig cfg;
  };

  struct TestCase
  {
    GLFormat fmt;
    GLenum target;
    uint32_t dim;
    bool isArray;
    bool isMSAA;
    bool isRect;
    bool isCube;
    bool canRender;
    bool canDepth;
    bool canStencil;
    bool hasData;
    GLuint tex;
  };

  struct TexImageTestCase
  {
    TestCase base;
    TexData data;
    Vec4i dimensions;
  };

  std::string MakeName(const TestCase &test)
  {
    std::string name = "Texture " + std::to_string(test.dim) + "D";

    if(test.isRect)
      name = "Texture Rect";
    if(test.isCube)
      name = "Texture Cube";

    if(test.isMSAA)
      name += " MSAA";
    if(test.isArray)
      name += " Array";

    return name;
  }

  GLuint GetProgram(const TestCase &test)
  {
    static std::map<uint32_t, GLuint> programs;

    uint32_t key = uint32_t(test.fmt.cfg.data);
    key |= test.dim << 6;
    key |= test.isMSAA ? 0x80000 : 0;
    key |= test.isArray ? 0x100000 : 0;
    key |= test.isRect ? 0x200000 : 0;
    key |= test.isCube ? 0x400000 : 0;

    GLuint ret = programs[key];
    if(!ret)
    {
      std::string texType = "sampler" + std::to_string(test.dim) + "D";
      if(test.isMSAA)
        texType += "MS";
      if(test.isRect)
        texType += "Rect";
      if(test.dim < 3 && test.isArray)
        texType += "Array";

      if(test.isCube)
      {
        texType = "samplerCube";
        if(test.isArray)
          texType += "Array";
      }

      std::string typemod = "";

      if(test.fmt.cfg.data == DataType::UInt)
        typemod = "u";
      else if(test.fmt.cfg.data == DataType::SInt)
        typemod = "i";

      std::string src = pixelTemplate;

      uint32_t dim = test.dim + (test.isArray ? 1 : 0);

      if(test.isCube)
      {
        src.replace(src.find("&params"), 7, "int(0)");
        src.replace(src.find("texelFetch"), 10, "cubeFetch");
      }
      else if(test.isRect)
      {
        src.replace(src.find("&params"), 7, "ivec2(0)");
      }
      else if(dim == 1)
      {
        src.replace(src.find("&params"), 7, "int(0), 0");
      }
      else if(dim == 2)
      {
        src.replace(src.find("&params"), 7, "ivec2(0), 0");
      }
      else if(dim == 3)
      {
        src.replace(src.find("&params"), 7, "ivec3(0), 0");
      }

      src.replace(src.find("&texdecl"), 8, typemod + texType);

      ret = programs[key] = MakeProgram(blitVertex, src);
    }

    return ret;
  }

  bool QueryFormatBool(GLenum target, GLenum format, GLenum pname)
  {
    GLint param = 0;
    glGetInternalformativ(target, format, pname, 1, &param);
    return param != 0;
  }

  void FinaliseTest(TestCase & test)
  {
    test.canRender = QueryFormatBool(test.target, test.fmt.internalFormat, GL_COLOR_RENDERABLE);
    test.canDepth = QueryFormatBool(test.target, test.fmt.internalFormat, GL_DEPTH_RENDERABLE);
    test.canStencil = QueryFormatBool(test.target, test.fmt.internalFormat, GL_STENCIL_RENDERABLE);

    GLint numSamples = 0;
    glGetInternalformativ(test.target, test.fmt.internalFormat, GL_NUM_SAMPLE_COUNTS, 1, &numSamples);

    GLint samples[8];
    glGetInternalformativ(test.target, test.fmt.internalFormat, GL_SAMPLES, numSamples, samples);

    Vec4i dimensions(texWidth, texHeight, texDepth);

    bool isCompressed =
        (test.fmt.cfg.type != TextureType::R9G9B9E5 && test.fmt.cfg.type != TextureType::Regular) ||
        test.fmt.internalFormat == GL_STENCIL_INDEX8;

    // Some GL drivers report that block compressed textures are supported for MSAA and color
    // rendering. Save them from themselves. Similarly they report support for 1D and 3D but then it
    // doesn't work properly
    if(isCompressed && (test.dim == 1 || test.dim == 3 || test.isRect || test.isCube || test.isMSAA))
      return;

    // don't create integer cubemaps, or non-regular format cubemaps
    if(test.isCube && (test.fmt.cfg.type != TextureType::Regular ||
                       test.fmt.cfg.data == DataType::SInt || test.fmt.cfg.data == DataType::UInt))
      return;

    // if the format is MSAA check we have our sample count
    if(test.isMSAA)
    {
      bool found = false;
      for(GLint i = 0; i < numSamples; i++)
        found |= (samples[i] == texSamples);

      if(!found)
        return;
    }

    // any format that supports MSAA but can't be rendered to is unsupported
    if(!test.canRender && !test.canDepth && !test.canStencil && test.isMSAA)
      return;

    test.tex = MakeTexture();
    glBindTexture(test.target, test.tex);

    // MSAA textures don't have sampler state at all
    if(!test.isMSAA)
    {
      // rectangle textures can't have mipmap minification
      glTexParameteri(test.target, GL_TEXTURE_MIN_FILTER,
                      test.isRect ? GL_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
      glTexParameteri(test.target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    if(test.dim == 1)
    {
      if(test.isArray)
        glTexStorage2D(test.target, texMips, test.fmt.internalFormat, texWidth, texSlices);
      else
        glTexStorage1D(test.target, texMips, test.fmt.internalFormat, texWidth);

      dimensions.y = dimensions.z = 1;
    }
    else if(test.isRect)
    {
      glTexStorage2D(test.target, 1, test.fmt.internalFormat, texWidth, texHeight);

      dimensions.z = 1;
    }
    else if(test.isCube)
    {
      if(test.isArray)
        glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, texMips, test.fmt.internalFormat, texWidth,
                       texHeight, 12);
      else
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, texMips, test.fmt.internalFormat, texWidth, texHeight);

      dimensions.z = 1;
    }
    else if(test.dim == 2)
    {
      if(test.isMSAA)
      {
        if(test.isArray)
          glTexStorage3DMultisample(test.target, texSamples, test.fmt.internalFormat, texWidth,
                                    texHeight, texSlices, GL_TRUE);
        else
          glTexStorage2DMultisample(test.target, texSamples, test.fmt.internalFormat, texWidth,
                                    texHeight, GL_TRUE);
      }
      else
      {
        if(test.isArray)
          glTexStorage3D(test.target, texMips, test.fmt.internalFormat, texWidth, texHeight,
                         texSlices);
        else
          glTexStorage2D(test.target, texMips, test.fmt.internalFormat, texWidth, texHeight);
      }

      dimensions.z = 1;
    }
    else if(test.dim == 3)
    {
      glTexStorage3D(test.target, texMips, test.fmt.internalFormat, texWidth, texHeight, texDepth);
    }

    if(test.canRender || test.canDepth || test.canStencil)
    {
      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
      glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);

      GLenum attach = GL_COLOR_ATTACHMENT0;
      if(test.canDepth && test.canStencil)
        attach = GL_DEPTH_STENCIL_ATTACHMENT;
      else if(test.canDepth)
        attach = GL_DEPTH_ATTACHMENT;
      else if(test.canStencil)
        attach = GL_STENCIL_ATTACHMENT;

      if(test.dim == 3 || test.isArray)
        glFramebufferTextureLayer(GL_FRAMEBUFFER, attach, test.tex, 0, 0);
      else
        glFramebufferTexture(GL_FRAMEBUFFER, attach, test.tex, 0);

      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

      // sometimes the GL driver lies about being able to render to a format! weeee!
      if(status != GL_FRAMEBUFFER_COMPLETE)
      {
        test.canRender = false;
        test.canDepth = false;
        test.canStencil = false;
      }

      if(!test.canRender && !test.canDepth && !test.canStencil && test.isMSAA)
      {
        test.tex = 0;
        return;
      }
    }

    glObjectLabel(GL_TEXTURE, test.tex, -1, (MakeName(test) + " " + test.fmt.name).c_str());

    // invalidate the texture, this makes renderdoc treat it as dirty
    glInvalidateTexImage(test.tex, 0);

    if(!test.isMSAA)
    {
      pushMarker("Set data for " + test.fmt.name + " " + MakeName(test));

      test.hasData = SetData(test, dimensions);

      popMarker();
    }
  }

  bool SetData(const TestCase &test, Vec4i dim)
  {
    bool isCompressed =
        (test.fmt.cfg.type != TextureType::R9G9B9E5 && test.fmt.cfg.type != TextureType::Regular);

    TexData data;

    // tightly packed data
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    GLenum format = GL_RGBA;
    GLenum type;

    if(test.fmt.cfg.data == DataType::UInt || test.fmt.cfg.data == DataType::UNorm)
    {
      type = GL_UNSIGNED_BYTE;
      if(test.fmt.cfg.componentBytes == 2)
        type = GL_UNSIGNED_SHORT;
      else if(test.fmt.cfg.componentBytes == 4)
        type = GL_UNSIGNED_INT;
    }
    else if(test.fmt.cfg.data == DataType::SInt || test.fmt.cfg.data == DataType::SNorm)
    {
      type = GL_BYTE;
      if(test.fmt.cfg.componentBytes == 2)
        type = GL_SHORT;
      else if(test.fmt.cfg.componentBytes == 4)
        type = GL_INT;
    }
    else
    {
      type = GL_FLOAT;
      if(test.fmt.cfg.componentBytes == 2)
        type = GL_HALF_FLOAT;
    }

    bool isInt = (test.fmt.cfg.data == DataType::SInt || test.fmt.cfg.data == DataType::UInt);
    if(test.fmt.cfg.componentCount == 4)
      format = isInt ? GL_RGBA_INTEGER : GL_RGBA;
    else if(test.fmt.cfg.componentCount == 3)
      format = isInt ? GL_RGB_INTEGER : GL_RGB;
    else if(test.fmt.cfg.componentCount == 2)
      format = isInt ? GL_RG_INTEGER : GL_RG;
    else if(test.fmt.cfg.componentCount == 1)
      format = isInt ? GL_RED_INTEGER : GL_RED;

    if(test.fmt.internalFormat == GL_STENCIL_INDEX8)
      format = GL_STENCIL_INDEX;

    if(test.fmt.cfg.type == TextureType::R9G9B9E5)
    {
      format = GL_RGB;
      type = GL_UNSIGNED_INT_5_9_9_9_REV;
    }

    GLint slices = test.isArray ? texSlices : 1;
    GLint mips = test.isMSAA || test.isRect ? 1 : texMips;

    if(test.isCube)
      slices = test.isArray ? 12 : 6;

    for(GLint s = 0; s < slices; s++)
    {
      for(GLint m = 0; m < mips; m++)
      {
        MakeData(data, test.fmt.cfg, dim, m, s);

        if(data.byteData.empty())
          return false;

        uint32_t mipWidth = std::max(texWidth >> m, 1U);
        uint32_t mipHeight = std::max(texHeight >> m, 1U);
        uint32_t mipDepth = std::max(texDepth >> m, 1U);

        if(isCompressed)
        {
          if(test.dim == 1)
          {
            if(test.isArray)
              glCompressedTexSubImage2D(test.target, m, 0, s, mipWidth, 1, test.fmt.internalFormat,
                                        (GLsizei)data.byteData.size(), data.byteData.data());
            else
              glCompressedTexSubImage1D(test.target, m, 0, mipWidth, format,
                                        (GLsizei)data.byteData.size(), data.byteData.data());
          }
          else if(test.isRect)
          {
            glCompressedTexSubImage2D(test.target, 0, 0, 0, mipWidth, mipHeight,
                                      test.fmt.internalFormat, (GLsizei)data.byteData.size(),
                                      data.byteData.data());
          }
          else if(test.dim == 2)
          {
            if(test.isArray)
              glCompressedTexSubImage3D(test.target, m, 0, 0, s, mipWidth, mipHeight, 1,
                                        test.fmt.internalFormat, (GLsizei)data.byteData.size(),
                                        data.byteData.data());
            else
              glCompressedTexSubImage2D(test.target, m, 0, 0, mipWidth, mipHeight,
                                        test.fmt.internalFormat, (GLsizei)data.byteData.size(),
                                        data.byteData.data());
          }
          else if(test.dim == 3)
          {
            glCompressedTexSubImage3D(test.target, m, 0, 0, 0, mipWidth, mipHeight, mipDepth,
                                      test.fmt.internalFormat, (GLsizei)data.byteData.size(),
                                      data.byteData.data());
          }
        }
        else
        {
          if(test.dim == 1)
          {
            if(test.isArray)
              glTexSubImage2D(test.target, m, 0, s, mipWidth, 1, format, type, data.byteData.data());
            else
              glTexSubImage1D(test.target, m, 0, mipWidth, format, type, data.byteData.data());
          }
          else if(test.isRect)
          {
            glTexSubImage2D(test.target, 0, 0, 0, mipWidth, mipHeight, format, type,
                            data.byteData.data());
          }
          else if(test.isCube)
          {
            if(test.isArray)
            {
              glTexSubImage3D(test.target, m, 0, 0, s, mipWidth, mipHeight, 1, format, type,
                              data.byteData.data());
            }
            else
            {
              GLenum faces[] = {
                  GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                  GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                  GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
              };

              glTexSubImage2D(faces[s], m, 0, 0, mipWidth, mipHeight, format, type,
                              data.byteData.data());
            }
          }
          else if(test.dim == 2)
          {
            if(test.isArray)
              glTexSubImage3D(test.target, m, 0, 0, s, mipWidth, mipHeight, 1, format, type,
                              data.byteData.data());
            else
              glTexSubImage2D(test.target, m, 0, 0, mipWidth, mipHeight, format, type,
                              data.byteData.data());
          }
          else if(test.dim == 3)
          {
            glTexSubImage3D(test.target, m, 0, 0, 0, mipWidth, mipHeight, mipDepth, format, type,
                            data.byteData.data());
          }
        }
      }
    }

    return true;
  }

  void AddSupportedTests(const GLFormat &f, std::vector<TestCase> &test_textures, bool depthMode)
  {
    // TODO: disable 1D depth textures for now, we don't support displaying them
    if(!depthMode)
    {
      test_textures.push_back({f, GL_TEXTURE_1D, 1, false});
      test_textures.push_back({f, GL_TEXTURE_1D_ARRAY, 1, true});
    }

    test_textures.push_back({f, GL_TEXTURE_2D, 2, false});
    test_textures.push_back({f, GL_TEXTURE_2D_ARRAY, 2, true});

    test_textures.push_back({f, GL_TEXTURE_3D, 3, false});

    // TODO: we don't support MSAA<->Array copies for these odd sized pixels, and I suspect drivers
    // emulate the formats anyway. Disable for now
    if(f.cfg.type != TextureType::Regular || f.cfg.componentCount != 3)
    {
      test_textures.push_back({f, GL_TEXTURE_2D_MULTISAMPLE, 2, false, true});
      test_textures.push_back({f, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 2, true, true});
    }

    test_textures.push_back({f, GL_TEXTURE_RECTANGLE, 2, false, false, true});

    if(!depthMode)
    {
      test_textures.push_back({f, GL_TEXTURE_CUBE_MAP, 2, false, false, false, true});
      test_textures.push_back({f, GL_TEXTURE_CUBE_MAP_ARRAY, 2, true, false, false, true});
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    pushMarker("Add tests");

#define TEST_CASE_NAME(texFmt) #texFmt

#define TEST_CASE(texType, texFmt, compCount, byteWidth, dataType) \
  {                                                                \
      &(#texFmt)[3],                                               \
      texFmt,                                                      \
      {texType, compCount, byteWidth, dataType},                   \
  }

    std::vector<TestCase> test_textures;
    std::vector<TexImageTestCase> test_teximage_textures;

    const GLFormat color_tests[] = {
        TEST_CASE(TextureType::Regular, GL_RGBA32F, 4, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_RGBA32UI, 4, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RGBA32I, 4, 4, DataType::SInt),

        TEST_CASE(TextureType::Regular, GL_RGB32F, 3, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_RGB32UI, 3, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RGB32I, 3, 4, DataType::SInt),

        TEST_CASE(TextureType::Regular, GL_RG32F, 2, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_RG32UI, 2, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RG32I, 2, 4, DataType::SInt),

        TEST_CASE(TextureType::Regular, GL_R32F, 1, 4, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_R32UI, 1, 4, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_R32I, 1, 4, DataType::SInt),

        TEST_CASE(TextureType::Regular, GL_RGBA16F, 4, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_RGBA16UI, 4, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RGBA16I, 4, 2, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_RGBA16, 4, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_RGBA16_SNORM, 4, 2, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_RGB16F, 3, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_RGB16UI, 3, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RGB16I, 3, 2, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_RGB16, 3, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_RGB16_SNORM, 3, 2, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_RG16F, 2, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_RG16UI, 2, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RG16I, 2, 2, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_RG16, 2, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_RG16_SNORM, 2, 2, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_R16F, 1, 2, DataType::Float),
        TEST_CASE(TextureType::Regular, GL_R16UI, 1, 2, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_R16I, 1, 2, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_R16, 1, 2, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_R16_SNORM, 1, 2, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_RGBA8UI, 4, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RGBA8I, 4, 1, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_RGBA8, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_SRGB8_ALPHA8, 4, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_RGBA8_SNORM, 4, 1, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_RGB8UI, 3, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RGB8I, 3, 1, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_RGB8, 3, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_SRGB8, 3, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_RGB8_SNORM, 3, 1, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_RG8UI, 2, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_RG8I, 2, 1, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_RG8, 2, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_SRG8_EXT, 1, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_RG8_SNORM, 2, 1, DataType::SNorm),

        TEST_CASE(TextureType::Regular, GL_R8UI, 1, 1, DataType::UInt),
        TEST_CASE(TextureType::Regular, GL_R8I, 1, 1, DataType::SInt),
        TEST_CASE(TextureType::Regular, GL_R8, 1, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_SR8_EXT, 1, 1, DataType::UNorm),
        TEST_CASE(TextureType::Regular, GL_R8_SNORM, 1, 1, DataType::SNorm),

        TEST_CASE(TextureType::Unknown, GL_RGB565, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, GL_RGB5_A1, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::Unknown, GL_RGB10_A2, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::Unknown, GL_RGB10_A2UI, 0, 0, DataType::UInt),

        TEST_CASE(TextureType::Unknown, GL_RGBA4, 0, 0, DataType::UNorm),

        // formats we don't support in RenderDoc currently
        // TEST_CASE(TextureType::Unknown, GL_RGB4, 0, 0, DataType::UNorm),
        // TEST_CASE(TextureType::Unknown, GL_RGB5, 0, 0, DataType::UNorm),
        // TEST_CASE(TextureType::Unknown, GL_RGB10, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::Unknown, GL_R11F_G11F_B10F, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::R9G9B9E5, GL_RGB9_E5, 0, 0, DataType::Float),

        TEST_CASE(TextureType::BC1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC1, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::BC2, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC2, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::BC3, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC3, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0, 0, DataType::UNorm),

        TEST_CASE(TextureType::BC4, GL_COMPRESSED_RED_RGTC1_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC4, GL_COMPRESSED_SIGNED_RED_RGTC1_EXT, 0, 0, DataType::SNorm),

        TEST_CASE(TextureType::BC5, GL_COMPRESSED_RED_GREEN_RGTC2_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC5, GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT, 0, 0, DataType::SNorm),

        TEST_CASE(TextureType::BC6, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC6, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT, 0, 0, DataType::SNorm),

        TEST_CASE(TextureType::BC7, GL_COMPRESSED_RGBA_BPTC_UNORM, 0, 0, DataType::UNorm),
        TEST_CASE(TextureType::BC7, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM, 0, 0, DataType::UNorm),
    };

    for(GLFormat f : color_tests)
    {
      if(f.internalFormat == GL_SR8_EXT && !GLAD_GL_EXT_texture_sRGB_R8)
        continue;

      if(f.internalFormat == GL_SRG8_EXT && !GLAD_GL_EXT_texture_sRGB_RG8)
        continue;

      AddSupportedTests(f, test_textures, false);
    }

    // finally add the depth tests
    const GLFormat depth_tests[] = {
        TEST_CASE(TextureType::Unknown, GL_DEPTH32F_STENCIL8, 0, 0, DataType::Float),
        TEST_CASE(TextureType::Unknown, GL_DEPTH_COMPONENT32F, 0, 0, DataType::Float),

        TEST_CASE(TextureType::Unknown, GL_DEPTH24_STENCIL8, 0, 0, DataType::Float),
        TEST_CASE(TextureType::Unknown, GL_DEPTH_COMPONENT24, 0, 0, DataType::Float),

        TEST_CASE(TextureType::Unknown, GL_DEPTH_COMPONENT16, 0, 0, DataType::Float),

        TEST_CASE(TextureType::Regular, GL_STENCIL_INDEX8, 1, 1, DataType::UInt),
    };

    for(GLFormat f : depth_tests)
      AddSupportedTests(f, test_textures, true);

    GLuint renderFBO = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);

    for(TestCase &t : test_textures)
    {
      if(QueryFormatBool(t.target, t.fmt.internalFormat, GL_INTERNALFORMAT_SUPPORTED) &&
         QueryFormatBool(t.target, t.fmt.internalFormat, GL_FRAGMENT_TEXTURE))
      {
        FinaliseTest(t);
      }
    }
    const GLFormat teximage_test = TEST_CASE(TextureType::Regular, GL_RGBA8, 4, 1, DataType::UNorm);
    TestCase tests[] = {
        {teximage_test, GL_TEXTURE_1D, 1, false},
        {teximage_test, GL_TEXTURE_2D, 2, false},
        {teximage_test, GL_TEXTURE_3D, 3, false},
    };
    for(TestCase test : tests)
    {
      if(QueryFormatBool(test.target, test.fmt.internalFormat, GL_INTERNALFORMAT_SUPPORTED) &&
         QueryFormatBool(test.target, test.fmt.internalFormat, GL_FRAGMENT_TEXTURE))
      {
        test.isMSAA = false;
        test.isRect = false;
        test.isCube = false;
        test.canRender = false;
        test.canDepth = false;
        test.canStencil = false;

        test.tex = MakeTexture();
        glBindTexture(test.target, test.tex);
        glTexParameteri(test.target, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(test.target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(test.target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        Vec4i dimensions(texWidth, texHeight, texDepth);

        if(test.dim == 1)
          dimensions.y = dimensions.z = 1;
        else if(test.dim == 2)
          dimensions.z = 1;

        glObjectLabel(GL_TEXTURE, test.tex, -1, (MakeName(test) + " " + test.fmt.name).c_str());

        GLint m = 0;
        GLint s = 0;
        TexData packed_data;
        MakeData(packed_data, test.fmt.cfg, dimensions, m, s);

        uint32_t mipHeight = dimensions.y;
        uint32_t mipDepth = dimensions.z;

        const uint32_t rowLengthMultiplier = 2;

        TexData data;
        data.rowPitch = packed_data.rowPitch * rowLengthMultiplier;
        data.slicePitch = packed_data.slicePitch * rowLengthMultiplier;
        data.byteData.resize(packed_data.byteData.size() * rowLengthMultiplier);
        for(uint32_t z = 0; z < mipDepth; ++z)
        {
          for(uint32_t y = 0; y < mipHeight; ++y)
          {
            memcpy(&data.byteData[z * data.slicePitch + y * data.rowPitch],
                   &packed_data.byteData[z * packed_data.slicePitch + y * packed_data.rowPitch],
                   packed_data.rowPitch);
          }
        }
        test.hasData = !data.byteData.empty();
        test_teximage_textures.push_back({test, data, dimensions});
      }
    }

    popMarker();

    GLuint msprog[(size_t)DataType::Count];

    msprog[(size_t)DataType::Float] = msprog[(size_t)DataType::UNorm] =
        msprog[(size_t)DataType::SNorm] = MakeProgram(blitVertex, pixelMSFloat);
    msprog[(size_t)DataType::UInt] = MakeProgram(blitVertex, pixelMSUInt);
    msprog[(size_t)DataType::SInt] = MakeProgram(blitVertex, pixelMSSInt);

    GLuint msdepthprog = MakeProgram(blitVertex, pixelMSDepth);

    for(TestCase &t : test_textures)
    {
      if(!t.tex || t.hasData)
        continue;

      if(!t.canRender && !t.canDepth && !t.canStencil)
      {
        TEST_ERROR("Need data for test %s, but it's not a renderable/depthable format",
                   t.fmt.name.c_str());
        continue;
      }

      glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
      glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);

      if(t.canDepth || t.canStencil)
      {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glDepthMask(0xff);
        glDepthFunc(GL_ALWAYS);
        glStencilFunc(GL_ALWAYS, 0, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      }
      else
      {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
      }

      pushMarker("Render data for " + t.fmt.name + " " + MakeName(t));

      t.hasData = true;

      bool srgb = false;
      bool bgr = false;
      switch(t.fmt.internalFormat)
      {
        // only need to handle renderable SRGB formats here
        case GL_SRGB8:
        case GL_SRGB8_ALPHA8: srgb = true; break;
        case GL_BGRA8_EXT:
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_RGB565: bgr = true;
        default: break;
      }

      int flags = 0;

      if(t.fmt.cfg.data == DataType::SNorm)
        flags |= 1;
      if(srgb)
        flags |= 2;
      if(bgr)
        flags |= 4;

      GLuint slices = t.isArray ? texSlices : 1u;
      GLuint mips = t.isMSAA || t.isRect ? 1u : texMips;

      for(GLuint mp = 0; mp < mips; mp++)
      {
        GLuint SlicesOrDepth = slices;
        if(t.dim == 3)
          SlicesOrDepth >>= mp;

        for(GLuint sl = 0; sl < SlicesOrDepth; sl++)
        {
          if(t.canDepth || t.canStencil)
          {
            GLenum attach = GL_NONE;
            if(t.canDepth && t.canStencil)
              attach = GL_DEPTH_STENCIL_ATTACHMENT;
            else if(t.canDepth)
              attach = GL_DEPTH_ATTACHMENT;
            else if(t.canStencil)
              attach = GL_STENCIL_ATTACHMENT;

            if(t.dim == 3 || t.isArray)
              glFramebufferTextureLayer(GL_FRAMEBUFFER, attach, t.tex, mp, sl);
            else
              glFramebufferTexture(GL_FRAMEBUFFER, attach, t.tex, mp);

            glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.0, 0);

            GLuint p = msdepthprog;

            glUseProgram(p);

            glUniform1ui(glGetUniformLocation(p, "texWidth"), texWidth);

            glUniform1ui(glGetUniformLocation(p, "slice"), t.dim == 3 ? 0 : sl);
            glUniform1ui(glGetUniformLocation(p, "mip"), mp);
            glUniform1ui(glGetUniformLocation(p, "flags"), flags);
            glUniform1ui(glGetUniformLocation(p, "zlayer"), t.dim == 3 ? sl : 0);

            glViewport(0, 0, texWidth, texHeight);

            uint32_t sampleCount = t.isMSAA ? texSamples : 1;

            // need to do each sample separately to let us vary the stencil value
            for(uint32_t sm = 0; sm < sampleCount; sm++)
            {
              glSampleMaski(0, 1 << sm);

              glStencilFunc(GL_ALWAYS, 100 + (mp + sm) * 10, 0xff);

              glDrawArrays(GL_TRIANGLES, 0, 3);

              // clip off the diagonal
              glUniform1ui(glGetUniformLocation(p, "flags"), 1);

              glStencilFunc(GL_ALWAYS, 10 + (mp + sm) * 10, 0xff);

              glDrawArrays(GL_TRIANGLES, 0, 3);
            }
          }
          else
          {
            if(t.dim == 3 || t.isArray)
              glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t.tex, mp, sl);
            else
              glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t.tex, mp);

            GLuint p = msprog[(size_t)t.fmt.cfg.data];

            glUseProgram(p);

            glUniform1ui(glGetUniformLocation(p, "texWidth"), texWidth);

            glUniform1ui(glGetUniformLocation(p, "slice"), t.dim == 3 ? 0 : sl);
            glUniform1ui(glGetUniformLocation(p, "mip"), mp);
            glUniform1ui(glGetUniformLocation(p, "flags"), flags);
            glUniform1ui(glGetUniformLocation(p, "zlayer"), t.dim == 3 ? sl : 0);

            glViewport(0, 0, texWidth, texHeight);

            glDrawArrays(GL_TRIANGLES, 0, 3);
          }
        }
      }

      popMarker();
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint colattach = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, colattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colattach, 0);

    std::vector<Vec4f> blue;
    blue.resize(64 * 64 * 64, Vec4f(0.0f, 0.0f, 1.0f, 1.0f));

    std::vector<Vec4f> green;
    green.resize(64 * 64, Vec4f(0.0f, 1.0f, 0.0f, 1.0f));

    // slice testing textures

    TestCase slice_test_array = {};
    TestCase slice_test_3d = {};
    slice_test_array.tex = MakeTexture();
    slice_test_array.dim = 2;
    slice_test_array.isArray = true;
    glBindTexture(GL_TEXTURE_2D_ARRAY, slice_test_array.tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 2, GL_RGBA32F, 64, 64, 64);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 64, 64, 64, GL_RGBA, GL_FLOAT, blue.data());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 0, 32, 32, 32, GL_RGBA, GL_FLOAT, blue.data());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 17, 64, 64, 1, GL_RGBA, GL_FLOAT, green.data());
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 17, 32, 32, 1, GL_RGBA, GL_FLOAT, green.data());

    slice_test_3d.tex = MakeTexture();
    slice_test_3d.dim = 3;
    glBindTexture(GL_TEXTURE_3D, slice_test_3d.tex);
    glTexStorage3D(GL_TEXTURE_3D, 2, GL_RGBA32F, 64, 64, 64);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 64, 64, 64, GL_RGBA, GL_FLOAT, blue.data());
    glTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 32, 32, 32, GL_RGBA, GL_FLOAT, blue.data());
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 17, 64, 64, 1, GL_RGBA, GL_FLOAT, green.data());
    glTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 17, 32, 32, 1, GL_RGBA, GL_FLOAT, green.data());

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);

      float col[] = {0.2f, 0.2f, 0.2f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      GLsizei viewX = 0, viewY = screenHeight - 10;
      glEnable(GL_SCISSOR_TEST);

      // dummy draw for each slice test texture
      pushMarker("slice tests");
      setMarker("2D array");
      glBindTextureUnit(0, slice_test_array.tex);
      glUseProgram(GetProgram(slice_test_array));
      glDrawArrays(GL_TRIANGLES, 0, 0);

      setMarker("3D");
      glBindTextureUnit(0, slice_test_3d.tex);
      glUseProgram(GetProgram(slice_test_3d));
      glDrawArrays(GL_TRIANGLES, 0, 0);
      popMarker();

      for(size_t i = 0; i < test_textures.size(); i++)
      {
        if(i == 0 || test_textures[i].fmt.internalFormat != test_textures[i - 1].fmt.internalFormat)
        {
          if(i != 0)
            popMarker();

          pushMarker(test_textures[i].fmt.name);
        }

        setMarker(MakeName(test_textures[i]));

        glViewport(viewX, viewY, 10, 10);
        glScissor(viewX + 1, viewY + 1, 8, 8);

        glUseProgram(GetProgram(test_textures[i]));

        if(test_textures[i].tex)
        {
          glBindTextureUnit(0, test_textures[i].tex);
          glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        else
        {
          setMarker("UNSUPPORTED");
        }

        // advance to next viewport
        viewX += 10;
        if(viewX + 10 > (float)screenWidth)
        {
          viewX = 0;
          viewY -= 10;
        }
      }

      // pop the last format region
      popMarker();

      pushMarker("TexImage tests");
      for(TexImageTestCase &test : test_teximage_textures)
      {
        if(test.base.hasData && test.base.tex)
        {
          setMarker(MakeName(test.base));
          glBindTexture(test.base.target, test.base.tex);
          // ROW_LENGTH is in pixels not bytes
          glPixelStorei(GL_UNPACK_ROW_LENGTH, test.data.rowPitch / test.base.fmt.cfg.componentCount);
          glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
          glPixelStorei(GL_PACK_ALIGNMENT, 1);

          uint32_t mipWidth = test.dimensions.x;
          uint32_t mipHeight = test.dimensions.y;
          uint32_t mipDepth = test.dimensions.z;
          GLenum format = GL_RGBA;
          GLenum type = GL_UNSIGNED_BYTE;
          GLint border = 0;
          GLint mip = 0;
          if(test.base.dim == 1)
          {
            glTexImage1D(test.base.target, mip, test.base.fmt.internalFormat, mipWidth, border,
                         format, type, test.data.byteData.data());
          }
          else if(test.base.dim == 2)
          {
            glTexImage2D(test.base.target, mip, test.base.fmt.internalFormat, mipWidth, mipHeight,
                         border, format, type, test.data.byteData.data());
          }
          else if(test.base.dim == 3)
          {
            glTexImage3D(test.base.target, mip, test.base.fmt.internalFormat, mipWidth, mipHeight,
                         mipDepth, border, format, type, test.data.byteData.data());
          }
          glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

          glViewport(viewX, viewY, 10, 10);
          glScissor(viewX + 1, viewY + 1, 8, 8);

          glUseProgram(GetProgram(test.base));

          glBindTextureUnit(0, test.base.tex);
          glDrawArrays(GL_TRIANGLES, 0, 3);

          viewX += 10;
          if(viewX + 10 > (float)screenWidth)
          {
            viewX = 0;
            viewY -= 10;
          }
        }
      }
      popMarker();

      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));
      glDisable(GL_SCISSOR_TEST);

      // blit to the screen for a nicer preview
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

      glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
