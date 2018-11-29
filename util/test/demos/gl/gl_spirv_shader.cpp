/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Baldur Karlsson
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

struct SPIRV_Shader : OpenGLGraphicsTest
{
  static constexpr const char *Description = "Draws using a SPIR-V shader pipeline.";

  std::string vertex = R"EOSHADER(
#version 420 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out vec4 oPos;
layout(location = 1) out vec4 oCol;
layout(location = 2) out vec2 oUV;

layout(binding = 0, std140) uniform vsconstsbuf
{
  vec4 offset;
  vec4 scale;

  vec2 UVscroll;
} vsconsts;

void main()
{
	gl_Position = oPos = vec4(Position.xyz * vsconsts.scale.xyz + vsconsts.offset.xyz, 1);
	oCol = Color;
	oUV = UV + vsconsts.UVscroll.xy;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 420 core

layout(location = 0) in vec4 iPos;
layout(location = 1) in vec4 iCol;
layout(location = 2) in vec2 iUV;

layout(location = 0) out vec4 Color;

layout(binding = 0) uniform sampler2D tex2D;

layout(binding = 1, std140) uniform fsconstsbuf
{
  vec4 tint;
} fsconsts;

void main()
{
	Color = (iCol + fsconsts.tint) * textureLod(tex2D, iUV, 0.0f);
}

)EOSHADER";

  int main(int argc, char **argv)
  {
    debugDevice = true;

    // initialise, create window, create context, etc
    if(!Init(argc, argv))
      return 3;

    if(!SpvCompilationSupported())
    {
      TEST_ERROR("Can't run SPIR-V test without glslc in PATH");
      return 2;
    }

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

    GLuint tex = MakeTexture();

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    uint32_t pixels[] = {
        // row 0
        0xff0e1f00, 0xfff0b207, 0xff02ff00, 0xff03ff00,
        // row 1
        0xff090f00, 0xff081eb0, 0xff010005, 0xff905f00,
        // row 2
        0xff502f03, 0xff004550, 0xff1020a0, 0xff120000,
        // row 3
        0xff0d3f00, 0xff6091d0, 0xff304ff0, 0xff800000,
    };

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    GLuint vsbuf = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, vsbuf);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f) * 3, 0, GL_DYNAMIC_STORAGE_BIT);

    GLuint fsbuf = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, fsbuf);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(Vec4f), 0, GL_DYNAMIC_STORAGE_BIT);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, vsbuf);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, fsbuf);

    GLuint glslprogram = MakeProgram(vertex, pixel);
    glObjectLabel(GL_PROGRAM, glslprogram, -1, "Full program");

    GLuint spirvprogram = MakeProgram();

    {
      GLuint vs = glCreateShader(GL_VERTEX_SHADER);
      GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

      std::vector<uint32_t> vsSPIRV =
          CompileShaderToSpv(vertex, ShaderLang::glsl, ShaderStage::vert, "main");
      std::vector<uint32_t> fsSPIRV =
          CompileShaderToSpv(pixel, ShaderLang::glsl, ShaderStage::frag, "main");

      glShaderBinary(1, &vs, GL_SHADER_BINARY_FORMAT_SPIR_V, vsSPIRV.data(),
                     (GLsizei)vsSPIRV.size() * 4);
      glShaderBinary(1, &fs, GL_SHADER_BINARY_FORMAT_SPIR_V, fsSPIRV.data(),
                     (GLsizei)fsSPIRV.size() * 4);

      std::string entry_point = "main";
      glSpecializeShaderARB(vs, (const GLchar *)entry_point.c_str(), 0, nullptr, nullptr);
      glSpecializeShaderARB(fs, (const GLchar *)entry_point.c_str(), 0, nullptr, nullptr);

      char buffer[1024];
      GLint status = 0;

      glGetShaderiv(vs, GL_COMPILE_STATUS, &status);

      if(status == 0)
      {
        glGetShaderInfoLog(vs, 1024, NULL, buffer);
        TEST_ERROR("Shader error: %s", buffer);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
      }

      glGetShaderiv(fs, GL_COMPILE_STATUS, &status);

      if(status == 0)
      {
        glGetShaderInfoLog(fs, 1024, NULL, buffer);
        TEST_ERROR("Shader error: %s", buffer);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
      }

      glAttachShader(spirvprogram, vs);
      glAttachShader(spirvprogram, fs);

      glLinkProgram(spirvprogram);

      glDetachShader(spirvprogram, vs);
      glDetachShader(spirvprogram, fs);

      glDeleteShader(vs);
      glDeleteShader(fs);

      glGetProgramiv(spirvprogram, GL_LINK_STATUS, &status);
      if(status == 0)
      {
        glGetProgramInfoLog(spirvprogram, 1024, NULL, buffer);
        TEST_ERROR("Link error: %s", buffer);
        return 3;
      }
    }

    struct
    {
      Vec4f offset;
      Vec4f scale;
      Vec2f UVscroll;
    } vsdata = {};

    vsdata.scale = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);

    Vec4f fsdata;

    fsdata = Vec4f(0.1f, 0.2f, 0.3f, 1.0f);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      vsdata.UVscroll.x += 0.01f;
      vsdata.UVscroll.y += 0.02f;

      glBindVertexArray(vao);

      glViewport(0, 0, GLsizei(screenWidth) >> 1, GLsizei(screenHeight));

      glNamedBufferSubData(vsbuf, 0, sizeof(vsdata), &vsdata);
      glNamedBufferSubData(fsbuf, 0, sizeof(fsdata), &fsdata);

      glUseProgram(glslprogram);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      glViewport(GLsizei(screenWidth) >> 1, 0, GLsizei(screenWidth) >> 1, GLsizei(screenHeight));

      glUseProgram(spirvprogram);
      glDrawArrays(GL_TRIANGLES, 0, 3);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST(SPIRV_Shader);