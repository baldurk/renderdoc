/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
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

RD_TEST(GL_Shader_Debug_Zoo, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Tests shader debugging on GL shaders, focussing on GL-specific functionality that is not "
      "available or implemented separately from Vulkan.";

  std::string vertex = R"EOSHADER(

#ifndef LOCATION
#define LOCATION layout
#endif

out gl_PerVertex
{
  vec4 gl_Position;
};

LOCATION(location = 0) in vec3 inPosition;
LOCATION(location = 1) in vec4 inColor;
LOCATION(location = 2) in vec2 inUV;

LOCATION(location = 1) out vec4 v2fColor;
LOCATION(location = 2) out vec2 v2fUV;

#ifdef MIXED
// no location interpolators
in float a2v_a;
out float v2f_a;
#endif

#ifdef BLOCK
LOCATION(location = 3) in float a2v_a;
LOCATION(location = 4) in int a2v_b;

LOCATION(location = 3) out v2fBlock
{
  float a;
  flat int b;
  flat vec2 flatv2;
} v2f;
#endif

#ifdef WEIRD_V2F
LOCATION(location = 3) in float a2v_a;
LOCATION(location = 4) in int a2v_b;

LOCATION(location = 3) out mat3 v2fMat;
LOCATION(location = 6) out float v2fArr[2];
#endif
#ifdef MULTI
LOCATION(location = 8) flat out int v2fInstData;
flat out vec3 v2fFlatFloat;
noperspective out vec3 v2fNoPerspFloat;
#endif

void main()
{
	gl_Position = vec4(inPosition.xyz, 1);
  gl_Position.w = inPosition.z;
	v2fColor = inColor;
	v2fUV = inUV;

#ifdef BLOCK
  v2f.a = a2v_a;
  v2f.b = a2v_b;
  v2f.flatv2.x = a2v_a;
  v2f.flatv2.y = float(a2v_b);
#endif

#ifdef MIXED
  v2f_a = a2v_a;
#endif

#ifdef WEIRD_V2F
  v2fMat = mat3(a2v_a * inPosition.xyz, (a2v_a + 1) * inPosition.xyz, (a2v_a + 2) * inPosition.xyz);
  v2fArr[0] = sqrt(a2v_a);
  v2fArr[1] = cos(a2v_a);
#endif

#ifdef MULTI
  v2fInstData = a2v_b + gl_VertexID/3;
  v2fFlatFloat = inPosition;
  v2fNoPerspFloat = inPosition;
  if(gl_InstanceID != 1 && a2v_a > 5.0)
    gl_Position.y += 10.0f;
#endif
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

#ifndef LOCATION
#define LOCATION layout
#endif

LOCATION(location = 1) in vec4 v2fColor;
LOCATION(location = 2) in vec2 v2fUV;

LOCATION(location = 0) out vec4 outColor;

#ifdef MIXED
// no location interpolators
in float v2f_a;
#endif

#ifdef BLOCK
LOCATION(location = 3) in v2fBlock
{
  float a;
  flat int b;
  flat vec2 flatv2;
} v2f;
#endif

#ifdef WEIRD_V2F
LOCATION(location = 3) in mat3 v2fMat;
LOCATION(location = 6) in float v2fArr[2];
#endif
#ifdef MULTI
LOCATION(location = 8) flat in int v2fInstData;
flat in vec3 v2fFlatFloat;
noperspective in vec3 v2fNoPerspFloat;
#endif

void main()
{
	outColor = v2fColor + v2fUV.xyxy;

#ifdef BLOCK
  outColor.r += v2f.a;
  outColor.g += float(v2f.b);
  outColor.ba += v2f.flatv2.xy;
#endif

#ifdef MIXED
  outColor.r += v2f_a;
#endif

#ifdef WEIRD_V2F
	outColor.rgb += v2fMat * outColor.rgb;
	outColor.g += v2fArr[0] * v2fArr[1];
#endif

#ifdef MULTI
  if(v2fInstData != 51) discard;
  outColor.xyz += v2fFlatFloat;
  outColor.xyz += v2fNoPerspFloat;
#endif
}

)EOSHADER";

  std::string bindingZooVertex = R"EOSHADER(

#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 1) out vec4 v2fColor;
layout(location = 2) out vec2 v2fUV;

void main()
{
	gl_Position = vec4(inPosition.xyz, 1);
	v2fColor = inColor;
	v2fUV = inUV;
}

)EOSHADER";

  std::string bindingZooPixel = R"EOSHADER(

#version 460 core

// have uniforms unused - our glslang recompilation will likely assign these locations but drivers likely won't.
uniform int unused;

// ensure alphabetical order does not match declaration order, also to test that we don't depend on that
// by accident
uniform vec4 zzzTest;

uniform mat4 matrix_test;

uniform float array_test[4];

struct TestStruct
{
  float a;
  int b;
};

uniform TestStruct struct_test;

uniform bool bool_test;

layout(binding = 0, std140) uniform ubo_test
{
  vec4 data;
} ubo;

layout(binding = 0, std430) buffer ssbo_test
{
  vec4 data;
} ssbo;

layout(binding = 0) uniform sampler2D tex2d_test;
layout(binding = 1) uniform samplerBuffer texBuf_test;
layout(binding = 2) uniform sampler2D bias_test;
layout(binding = 3) uniform sampler2D resArray_test[2];

layout(location = 1) in vec4 v2fColor;
layout(location = 2) in vec2 v2fUV;

layout(location = 0) out vec4 outColor;

void main()
{
  vec4 col = vec4(0, 0, 0, 0);
  col += matrix_test * zzzTest;
  col.r += array_test[0];
  col.g += array_test[1];
  col.b += array_test[2];
  col.a += array_test[3];
  col.r += bool_test ? 1.0f : 0.0f;
  col += float(struct_test.b) * struct_test.a;
  col += ubo.data;
  col += ssbo.data;
  col += dFdx(v2fUV).xyxy;
  col += texture(tex2d_test, v2fUV);
  col += texelFetch(texBuf_test, int(v2fUV.x*10));
  col += texture(bias_test, v2fUV, -0.8f);
  col += texture(resArray_test[0], v2fUV);
  col += texture(resArray_test[1], v2fUV);
  outColor = col;
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    OpenGLGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(!SpvCompilationSupported())
      Avail = InternalSpvCompiler() ? "Internal SPIR-V compiler did not initialise"
                                    : "Couldn't find 'glslc' or 'glslangValidator' in PATH - "
                                      "required for SPIR-V compilation";
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    const DefaultA2V tri[9] = {
        {Vec3f(-1.0f, 1.0f, 0.1f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, 1.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-1.0f, -1.0f, 0.3f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-1.0f, 1.0f, 0.2f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, 1.0f, 0.3f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-1.0f, -1.0f, 0.1f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-1.0f, 1.0f, 0.3f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, 1.0f, 0.1f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-1.0f, -1.0f, 0.2f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(tri), tri, 0);

    ConfigureDefaultVAO();

    struct ExtraA2V
    {
      float a;
      int b;
    };

    const ExtraA2V tri2[6] = {
        {2.5f, 50}, {3.5f, 7}, {4.5f, 8}, {5.5f, 40}, {6.5f, 50}, {7.5f, 60},
    };

    GLuint vb2 = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb2);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(tri2), tri2, 0);

    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ExtraA2V), (void *)(0));
    glVertexAttribDivisor(3, 1);
    glVertexAttribIPointer(4, 1, GL_INT, sizeof(ExtraA2V), (void *)(sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    std::string vs = "#version 460 core\n";
    vs += vertex;

    std::string ps = "#version 460 core\n";
    ps += pixel;

    GLuint spirvprogram = MakeProgram();
    GLuint spirvStrippedProgram = MakeProgram();

    {
      GLuint vsShad = glCreateShader(GL_VERTEX_SHADER);
      GLuint psShad = glCreateShader(GL_FRAGMENT_SHADER);

      std::vector<uint32_t> vsSPIRV =
          CompileShaderToSpv(vs, SPIRVTarget::opengl, ShaderLang::glsl, ShaderStage::vert, "main");
      std::vector<uint32_t> fsSPIRV =
          CompileShaderToSpv(ps, SPIRVTarget::opengl, ShaderLang::glsl, ShaderStage::frag, "main");

      glShaderBinary(1, &vsShad, GL_SHADER_BINARY_FORMAT_SPIR_V, vsSPIRV.data(),
                     (GLsizei)vsSPIRV.size() * 4);
      glShaderBinary(1, &psShad, GL_SHADER_BINARY_FORMAT_SPIR_V, fsSPIRV.data(),
                     (GLsizei)fsSPIRV.size() * 4);

      std::string entry_point = "main";
      glSpecializeShaderARB(vsShad, (const GLchar *)entry_point.c_str(), 0, nullptr, nullptr);
      glSpecializeShaderARB(psShad, (const GLchar *)entry_point.c_str(), 0, nullptr, nullptr);

      glAttachShader(spirvprogram, vsShad);
      glAttachShader(spirvprogram, psShad);

      glLinkProgram(spirvprogram);

      glDetachShader(spirvprogram, vsShad);
      glDetachShader(spirvprogram, psShad);

      glDeleteShader(vsShad);
      glDeleteShader(psShad);

      // strip any OpName/OpMemberName

      for(size_t i = 5; i < vsSPIRV.size();)
      {
        uint16_t WordCount = vsSPIRV[i] >> 16;
        uint16_t opcode = vsSPIRV[i] & 0xffff;

        // OpName / OpMemberName
        if(opcode == 5 || opcode == 6)
        {
          vsSPIRV.erase(vsSPIRV.begin() + i, vsSPIRV.begin() + i + WordCount);
          continue;
        }

        i += WordCount;
      }

      for(size_t i = 5; i < fsSPIRV.size();)
      {
        uint16_t WordCount = fsSPIRV[i] >> 16;
        uint16_t opcode = fsSPIRV[i] & 0xffff;

        // OpName / OpMemberName
        if(opcode == 5 || opcode == 6)
        {
          fsSPIRV.erase(fsSPIRV.begin() + i, fsSPIRV.begin() + i + WordCount);
          continue;
        }

        i += WordCount;
      }

      vsShad = glCreateShader(GL_VERTEX_SHADER);
      psShad = glCreateShader(GL_FRAGMENT_SHADER);

      glShaderBinary(1, &vsShad, GL_SHADER_BINARY_FORMAT_SPIR_V, vsSPIRV.data(),
                     (GLsizei)vsSPIRV.size() * 4);
      glShaderBinary(1, &psShad, GL_SHADER_BINARY_FORMAT_SPIR_V, fsSPIRV.data(),
                     (GLsizei)fsSPIRV.size() * 4);

      glSpecializeShaderARB(vsShad, (const GLchar *)entry_point.c_str(), 0, nullptr, nullptr);
      glSpecializeShaderARB(psShad, (const GLchar *)entry_point.c_str(), 0, nullptr, nullptr);

      glAttachShader(spirvStrippedProgram, vsShad);
      glAttachShader(spirvStrippedProgram, psShad);

      glLinkProgram(spirvStrippedProgram);

      glDetachShader(spirvStrippedProgram, vsShad);
      glDetachShader(spirvStrippedProgram, psShad);

      glDeleteShader(vsShad);
      glDeleteShader(psShad);
    }

    GLuint simpleModernGLSLProgram = MakeProgram(vs, ps);

    vs = "#version 460 core\n";
    vs += "#define LOCATION(x)\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define LOCATION(x)\n";
    ps += pixel;

    GLuint noLocationGLSLProgram = MakeProgram(vs, ps, "", [](GLuint prog) {
      glBindAttribLocation(prog, 0, "inPosition");
      glBindAttribLocation(prog, 1, "inColor");
      glBindAttribLocation(prog, 2, "inUV");
    });

    vs = "#version 460 core\n";
    vs += "#define BLOCK 1\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define BLOCK 1\n";
    ps += pixel;

    GLuint blockLocationGLSLProgram = MakeProgram(vs, ps);

    vs = "#version 460 core\n";
    vs += "#define BLOCK 1\n";
    vs += "#define LOCATION(x)\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define BLOCK 1\n";
    ps += "#define LOCATION(x)\n";
    ps += pixel;

    GLuint blockNoLocationGLSLProgram = MakeProgram(vs, ps, "", [](GLuint prog) {
      glBindAttribLocation(prog, 0, "inPosition");
      glBindAttribLocation(prog, 1, "inColor");
      glBindAttribLocation(prog, 2, "inUV");
      glBindAttribLocation(prog, 3, "a2v_a");
      glBindAttribLocation(prog, 4, "a2v_b");
    });

    vs = "#version 460 core\n";
    vs += "#define MIXED 1\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define MIXED 1\n";
    ps += pixel;

    GLuint mixedLocationGLSLProgram =
        MakeProgram(vs, ps, "", [](GLuint prog) { glBindAttribLocation(prog, 3, "a2v_a"); });

    vs = "#version 460 core\n";
    vs += "#define WEIRD_V2F 1\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define WEIRD_V2F 1\n";
    ps += pixel;

    GLuint weirdLocationGLSLProgram = MakeProgram(vs, ps);

    GLuint pipeline = MakePipeline();
    GLuint pipelineVert = MakePipelineProgram(GL_VERTEX_SHADER, vs);
    GLuint pipelinePix = MakePipelineProgram(GL_FRAGMENT_SHADER, ps);
    glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, pipelineVert);
    glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, pipelinePix);

    vs = "#version 460 core\n";
    vs += "#define WEIRD_V2F 1\n";
    vs += "#define MULTI 1\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define WEIRD_V2F 1\n";
    ps += "#define MULTI 1\n";
    ps += pixel;

    GLuint multiGLSLProgram = MakeProgram(vs, ps);

    vs = "#version 460 core\n";
    vs += "#define WEIRD_V2F 1\n";
    vs += "#define LOCATION(x)\n";
    vs += vertex;

    ps = "#version 460 core\n";
    ps += "#define WEIRD_V2F 1\n";
    ps += "#define LOCATION(x)\n";
    ps += pixel;

    GLuint weirdNoLocationGLSLProgram = MakeProgram(vs, ps, "", [](GLuint prog) {
      glBindAttribLocation(prog, 0, "inPosition");
      glBindAttribLocation(prog, 1, "inColor");
      glBindAttribLocation(prog, 2, "inUV");
      glBindAttribLocation(prog, 3, "a2v_a");
      glBindAttribLocation(prog, 4, "a2v_b");
    });

    GLuint bindingZooProgram = MakeProgram(bindingZooVertex, bindingZooPixel);
    (void)bindingZooProgram;

    glUseProgram(bindingZooProgram);

    glUniform4f(glGetUniformLocation(bindingZooProgram, "zzzTest"), 0.2f, 0.3f, 0.4f, 0.5f);
    float mat[16] = {};
    for(int i = 0; i < 16; i++)
      mat[i] = 0.6f + float(i) / 10.0f;
    glUniformMatrix4fv(glGetUniformLocation(bindingZooProgram, "matrix_test"), 1, GL_FALSE, mat);
    float arr[4] = {};
    for(int i = 0; i < 4; i++)
      arr[i] = 2.1f + float(i) / 10.0f;
    glUniform1fv(glGetUniformLocation(bindingZooProgram, "array_test"), 4, arr);
    glUniform1f(glGetUniformLocation(bindingZooProgram, "struct_test.a"), 9.9f);
    glUniform1i(glGetUniformLocation(bindingZooProgram, "struct_test.b"), 99);
    glUniform1ui(glGetUniformLocation(bindingZooProgram, "bool_test"), 1);

    const size_t bindOffset = 16;

    Vec4f cbufferdata[1024 + bindOffset];

    for(int i = 0; i < bindOffset; i++)
      cbufferdata[i] = Vec4f(-99.9f, -88.8f, -77.7f, -66.6f);

    for(int i = 0; i < 1024; i++)
      cbufferdata[bindOffset + i] =
          Vec4f(float(i * 4 + 0), float(i * 4 + 1), float(i * 4 + 2), float(i * 4 + 3));

    GLuint cb = MakeBuffer();
    glBindBuffer(GL_UNIFORM_BUFFER, cb);
    glBufferStorage(GL_UNIFORM_BUFFER, sizeof(cbufferdata), cbufferdata, GL_MAP_WRITE_BIT);

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, cb, bindOffset * sizeof(Vec4f), 16 * sizeof(Vec4f));

    GLuint ssbo = MakeBuffer();
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(cbufferdata), cbufferdata, GL_MAP_WRITE_BIT);

    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, ssbo, bindOffset * sizeof(Vec4f) * 2,
                      16 * sizeof(Vec4f));

    GLuint tbuf_store = MakeBuffer();
    glBindBuffer(GL_TEXTURE_BUFFER, tbuf_store);
    glBufferStorage(GL_TEXTURE_BUFFER, sizeof(Vec4f) * 16,
                    cbufferdata + bindOffset * sizeof(Vec4f) * 3, 0);

    GLuint tbuf = MakeTexture();
    glBindTexture(GL_TEXTURE_BUFFER, tbuf);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbuf_store);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, tbuf);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    glActiveTexture(GL_TEXTURE0);
    GLuint smiley = MakeTexture();
    glBindTexture(GL_TEXTURE_2D, smiley);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, rgba8.width, rgba8.height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba8.width, rgba8.height, GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba8.data.data());

    int dim = 128, x = 0;
    GLuint bias_tex = MakeTexture();
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bias_tex);
    glTexStorage2D(GL_TEXTURE_2D, 8, GL_RGBA8, dim, dim);
    std::vector<uint32_t> mipdata;

    uint32_t bias_col[10] = {
        0xffff0000, 0xff00ff00, 0xff0000ff, 0xffff00ff,
        0xffffff00, 0xff00ffff, 0xff000000, 0xffffffff,
    };
    while(dim > 0)
    {
      mipdata.resize(dim * dim);
      for(uint32_t &p : mipdata)
        p = bias_col[x];

      glTexSubImage2D(GL_TEXTURE_2D, x, 0, 0, dim, dim, GL_RGBA, GL_UNSIGNED_BYTE, mipdata.data());
      dim >>= 1;
      x++;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

    {
      std::vector<uint32_t> pixels;
      uint32_t countPixels = rgba8.height * rgba8.width;
      uint32_t srcIdx = countPixels - 1;
      for(uint32_t dstIdx = 0; dstIdx < countPixels; ++dstIdx)
      {
        pixels.push_back(rgba8.data[srcIdx]);
        --srcIdx;
      }
      GLuint resArray_tex0 = MakeTexture();
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, resArray_tex0);
      glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, rgba8.width, rgba8.height);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba8.width, rgba8.height, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels.data());
    }
    {
      std::vector<uint32_t> pixels;
      uint32_t countPixels = rgba8.height * rgba8.width;
      uint32_t srcIdx = countPixels - 1;
      for(uint32_t dstIdx = 0; dstIdx < countPixels; ++dstIdx)
      {
        pixels.push_back(rgba8.data[srcIdx % countPixels]);
        srcIdx -= 2;
      }
      GLuint resArray_tex1 = MakeTexture();
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, resArray_tex1);
      glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, rgba8.width, rgba8.height);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rgba8.width, rgba8.height, GL_RGBA, GL_UNSIGNED_BYTE,
                      pixels.data());
    }

    // render offscreen to make picked values accurate
    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Color render texture
    GLuint colattach = MakeTexture();

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, colattach);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, screenWidth, screenHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colattach, 0);

    while(Running())
    {
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);

      float col[] = {0.0f, 0.0f, 0.0f, 1.0f};
      glClearBufferfv(GL_COLOR, 0, col);

      glBindVertexArray(vao);

      int test = 0;

      pushMarker("GLSL tests");
      {
        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(simpleModernGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(noLocationGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(blockLocationGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(blockNoLocationGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(mixedLocationGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(weirdLocationGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(multiGLSLProgram);
        glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 3, 3, 3, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(multiGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 9);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(weirdNoLocationGLSLProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(bindingZooProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(0);
        glBindProgramPipeline(pipeline);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindProgramPipeline(0);
      }
      popMarker();

      pushMarker("SPIRV tests");
      {
        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(spirvprogram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glViewport(test * 4, screenHeight - 4, 4, 4);
        test++;
        glUseProgram(spirvStrippedProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);
      }
      popMarker();

      glBlitNamedFramebuffer(fbo, 0, 0, 0, screenWidth, screenHeight, 0, 0, screenWidth,
                             screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
