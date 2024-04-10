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

#include "glsl_shaders.h"
#include "common/common.h"
#include "common/formatting.h"
#include "driver/shaders/spirv/glslang_compile.h"
#include "glslang/glslang/Public/ResourceLimits.h"
#include "glslang/glslang/Public/ShaderLang.h"
#include "os/os_specific.h"

#define GLSL_HEADERS(HEADER) \
  HEADER(glsl_globals)       \
  HEADER(glsl_ubos)          \
  HEADER(vk_texsample)       \
  HEADER(gl_texsample)       \
  HEADER(gles_texsample)

class EmbeddedIncluder : public glslang::TShader::Includer
{
#define DECL(header) rdcstr header = GetEmbeddedResource(CONCAT(glsl_, CONCAT(header, _h)));
  GLSL_HEADERS(DECL)
#undef DECL

public:
  // For the "system" or <>-style includes; search the "system" paths.
  virtual IncludeResult *includeSystem(const char *headerName, const char *includerName,
                                       size_t inclusionDepth) override
  {
#define GET(header)                               \
  if(!strcmp(headerName, STRINGIZE(header) ".h")) \
    return new IncludeResult(headerName, header.data(), header.length(), NULL);
    GLSL_HEADERS(GET)
#undef GET

    return NULL;
  }

  // For the "local"-only aspect of a "" include. Should not search in the
  // "system" paths, because on returning a failure, the parser will
  // call includeSystem() to look in the "system" locations.
  virtual IncludeResult *includeLocal(const char *headerName, const char *includerName,
                                      size_t inclusionDepth) override
  {
    return includeSystem(headerName, includerName, inclusionDepth);
  }

  virtual void releaseInclude(IncludeResult *result) override { delete result; }
};

rdcstr GenerateGLSLShader(const rdcstr &shader, ShaderType type, int version, const rdcstr &defines)
{
  // shader stage doesn't matter for us since we're just pre-processing.
  glslang::TShader sh(EShLangFragment);

  rdcstr combined;

  if(type == ShaderType::GLSLES)
  {
    if(version == 100)
      combined = "#version 100\n";    // no es suffix
    else
      combined = StringFormat::Fmt("#version %d es\n", version);
  }
  else
  {
    if(version == 110)
      combined = "#version 110\n";    // no core suffix
    else
      combined = StringFormat::Fmt("#version %d core\n", version);
  }

  // glslang requires the google extension, but we don't want it in the final shader, so remember it
  // and remove it later.
  rdcstr include_ext = "#extension GL_GOOGLE_include_directive : require\n";

  combined += include_ext;

  if(type == ShaderType::GLSLES)
    combined +=
        "#define OPENGL 1\n"
        "#define OPENGL_ES 1\n";
  else if(type == ShaderType::GLSL)
    combined +=
        "#define OPENGL 1\n"
        "#define OPENGL_CORE 1\n";

  combined += defines;

  combined += shader;

  const char *c_src = combined.c_str();
  glslang::EShClient client = type == ShaderType::Vulkan    ? glslang::EShClientVulkan
                              : type == ShaderType::GLSPIRV ? glslang::EShClientOpenGL
                                                            : glslang::EShClientNone;
  glslang::EShTargetClientVersion targetversion =
      type == ShaderType::Vulkan ? glslang::EShTargetVulkan_1_0 : glslang::EShTargetOpenGL_450;
  int inputVersion = client != glslang::EShClientNone ? 100 : 0;

  sh.setStrings(&c_src, 1);
  sh.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, client, inputVersion);
  sh.setEnvClient(client, targetversion);
  sh.setEnvTarget(glslang::EShTargetNone, glslang::EShTargetSpv_1_0);

  EmbeddedIncluder incl;

  EShMessages flags = EShMsgOnlyPreprocessor;

  if(type == ShaderType::Vulkan)
    flags = EShMessages(flags | EShMsgSpvRules | EShMsgVulkanRules);
  else if(type == ShaderType::GLSPIRV)
    flags = EShMessages(flags | EShMsgSpvRules);

  rdcstr ret;
  bool success;

  {
    std::string outstr;
    success =
        sh.preprocess(GetDefaultResources(), 100, ENoProfile, false, false, flags, &outstr, incl);
    ret.assign(outstr.c_str(), outstr.size());
  }

  int offs = ret.find(include_ext);
  if(offs >= 0)
    ret.erase(offs, include_ext.size());

  // strip any #line directives that got added
  offs = ret.find("\n#line ");
  while(offs >= 0)
  {
    int eol = ret.find('\n', offs + 2);

    if(eol < 0)
      ret.erase(offs + 1, ~0U);
    else
      ret.erase(offs + 1, eol - offs);

    offs = ret.find("\n#line ", offs);
  }

  if(!success)
  {
    RDCLOG("glslang failed to build internal shader:\n\n%s\n\n%s", sh.getInfoLog(),
           sh.getInfoDebugLog());

    return "";
  }

  return ret;
}

static bool isspacetab(char c)
{
  return c == '\t' || c == ' ';
}

static bool isnewline(char c)
{
  return c == '\r' || c == '\n';
}

rdcstr InsertSnippetAfterVersion(ShaderType type, const char *source, int len, const char *snippet)
{
  // we require these enums to be compatible elsewhere
  glslang::TShader sh(EShLangFragment);

  glslang::EShClient client =
      type == ShaderType::Vulkan ? glslang::EShClientVulkan : glslang::EShClientNone;
  glslang::EShTargetClientVersion targetversion =
      type == ShaderType::Vulkan ? glslang::EShTargetVulkan_1_0 : glslang::EShTargetOpenGL_450;
  int inputVersion = client != glslang::EShClientNone ? 100 : 0;

  sh.setStringsWithLengths(&source, &len, 1);
  sh.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, client, inputVersion);
  sh.setEnvClient(client, targetversion);
  sh.setEnvTarget(glslang::EShTargetNone, glslang::EShTargetSpv_1_0);

  glslang::TShader::ForbidIncluder incl;

  bool success;

  EShMessages flags = EShMsgOnlyPreprocessor;
  if(type == ShaderType::Vulkan)
    flags = EShMessages(flags | EShMsgSpvRules | EShMsgVulkanRules);
  else if(type == ShaderType::GLSPIRV)
    flags = EShMessages(flags | EShMsgSpvRules);

  rdcstr src;
  std::string outstr;
  {
    success =
        sh.preprocess(GetDefaultResources(), 100, ENoProfile, false, false, flags, &outstr, incl);
    src.assign(outstr.c_str(), outstr.size());
  }

  // find if this source contains a #version, accounting for whitespace. It must be the first thing
  // (except for whitespace and comments, and comments have been removed)
  int32_t it = src.find("#");
  len = src.count();

  if(it >= 0)
  {
    // advance past the #
    ++it;

    // skip whitespace
    while(it < len && isspacetab(src[it]))
      ++it;

    if(it + 7 < len && !strncmp(&src[it], "version", 7))
    {
      it = src.find_first_of("\r\n", it);
      while(it < len && isnewline(src[it]))
        it++;

      // it points after the #version statement
    }
    else
    {
      it = -1;
    }
  }

  // no #version statement found - insert our own
  if(it < 0)
  {
    rdcstr version = "#version 430 core\n\n";
    src.insert(0, version);
    it = version.count();
  }

  src.insert(it, snippet);

  return src;
}

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"
#include "driver/shaders/spirv/spirv_reflect.h"

// define tests of GLSL reflection that can be re-used by both GL emulated and SPIR-V reflection
void TestGLSLReflection(ShaderType testType, ReflectionMaker compile)
{
#define REQUIRE_ARRAY_SIZE(size, min) \
  REQUIRE(size >= min);               \
  CHECK(size == min);

  if(testType == ShaderType::GLSL || testType == ShaderType::GLSPIRV)
  {
    // test GL only features

    SECTION("GL global uniforms")
    {
      rdcstr source = R"(
#version 450 core

layout(location = 100) uniform vec3 global_var[5];
layout(location = 200) uniform mat3x2 global_var2[3];

void main() {
  gl_FragDepth = global_var[4].y + global_var2[2][2][0];
}

)";

      ShaderReflection refl;
      compile(ShaderStage::Fragment, source, "main", refl);

      if(testType == ShaderType::GLSPIRV)
        CHECK(refl.encoding == ShaderEncoding::OpenGLSPIRV);
      else
        CHECK(refl.encoding == ShaderEncoding::GLSL);

      REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

      REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 1);
      {
        CHECK(refl.constantBlocks[0].name == "$Globals");
        {
          const ConstantBlock &cblock = refl.constantBlocks[0];
          INFO("UBO: " << cblock.name.c_str());

          CHECK(!cblock.bufferBacked);
          CHECK(!cblock.compileConstants);

          REQUIRE_ARRAY_SIZE(cblock.variables.size(), 2);
          {
            CHECK(cblock.variables[0].name == "global_var");
            {
              const ShaderConstant &member = cblock.variables[0];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::Float);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 3);
              CHECK(member.type.elements == 5);
            }

            CHECK(cblock.variables[1].name == "global_var2");
            {
              const ShaderConstant &member = cblock.variables[1];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::Float);
              CHECK(member.type.rows == 2);
              CHECK(member.type.columns == 3);
              CHECK(member.type.elements == 3);
              CHECK(member.type.ColMajor());
            }
          }
        }
      }
    };

    SECTION("GL atomic counters")
    {
      rdcstr source = R"(
#version 450 core

layout(binding = 0) uniform atomic_uint atom;

void main() {
  gl_FragDepth = float(atomicCounter(atom));
}

)";

      ShaderReflection refl;
      compile(ShaderStage::Fragment, source, "main", refl);

      REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

      REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 1);
      {
        CHECK(refl.readWriteResources[0].name == "atom");
        {
          const ShaderResource &res = refl.readWriteResources[0];
          INFO("read-write resource: " << res.name.c_str());

          // GLSL does not have register bindings as they're dynamic
          if(testType != ShaderType::GLSL)
          {
            CHECK(res.fixedBindSetOrSpace == 0);
            CHECK(res.fixedBindNumber == 0);
          }
          CHECK(res.bindArraySize == 1);
          CHECK(res.textureType == TextureType::Buffer);
          CHECK(res.variableType.members.empty());
          CHECK(res.variableType.baseType == VarType::UInt);
          CHECK(res.variableType.rows == 1);
          CHECK(res.variableType.columns == 1);
        }
      }
    };
  }
  else if(testType == ShaderType::Vulkan)
  {
    // test Vulkan only features

    SECTION("Vulkan separate sampler objects")
    {
      rdcstr source = R"(
#version 450 core

layout (set=1, binding=2) uniform sampler S;
layout (set=2, binding=4) uniform texture2D T;
layout (set=2, binding=5) uniform sampler2D ST;

void main() {
  gl_FragDepth = textureLod(ST, gl_FragCoord.xy, gl_FragCoord.z).z +
                 textureLod(sampler2D(T, S), gl_FragCoord.xy, gl_FragCoord.z).z;
}
)";
      ShaderReflection refl;
      compile(ShaderStage::Fragment, source, "main", refl);

      CHECK(refl.encoding == ShaderEncoding::SPIRV);

      REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);

      REQUIRE_ARRAY_SIZE(refl.samplers.size(), 1);
      {
        CHECK(refl.samplers[0].name == "S");
        {
          const ShaderSampler &samp = refl.samplers[0];
          INFO("read-only resource: " << samp.name.c_str());

          // GLSL does not have register bindings as they're dynamic
          if(testType != ShaderType::GLSL)
          {
            CHECK(samp.fixedBindSetOrSpace == 1);
            CHECK(samp.fixedBindNumber == 2);
          }
          CHECK(samp.bindArraySize == 1);
        }
      }

      REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 2);
      {
        CHECK(refl.readOnlyResources[0].name == "T");
        {
          const ShaderResource &res = refl.readOnlyResources[0];
          INFO("read-only resource: " << res.name.c_str());

          // GLSL does not have register bindings as they're dynamic
          if(testType != ShaderType::GLSL)
          {
            CHECK(res.fixedBindSetOrSpace == 2);
            CHECK(res.fixedBindNumber == 4);
          }
          CHECK(res.bindArraySize == 1);
          CHECK(res.textureType == TextureType::Texture2D);
          CHECK(res.variableType.members.empty());
          CHECK(res.variableType.baseType == VarType::Float);
        }

        CHECK(refl.readOnlyResources[1].name == "ST");
        {
          const ShaderResource &res = refl.readOnlyResources[1];
          INFO("read-only resource: " << res.name.c_str());

          // GLSL does not have register bindings as they're dynamic
          if(testType != ShaderType::GLSL)
          {
            CHECK(res.fixedBindSetOrSpace == 2);
            CHECK(res.fixedBindNumber == 5);
          }
          CHECK(res.bindArraySize == 1);
          CHECK(res.textureType == TextureType::Texture2D);
          CHECK(res.variableType.members.empty());
          CHECK(res.variableType.baseType == VarType::Float);
        }
      }
    };

    SECTION("Vulkan specialization constants")
    {
      rdcstr source = R"(
#version 450 core

layout(constant_id = 17) const int foo = 12;
layout(constant_id = 19) const float bar = 0.5f;

void main() {
  gl_FragDepth = float(foo) + bar;
}
)";

      ShaderReflection refl;
      compile(ShaderStage::Fragment, source, "main", refl);

      REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

      REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 1);
      {
        CHECK(refl.constantBlocks[0].name == "Specialization Constants");
        {
          const ConstantBlock &cblock = refl.constantBlocks[0];
          INFO("UBO: " << cblock.name.c_str());

          CHECK(cblock.bindArraySize == 1);
          CHECK(!cblock.bufferBacked);
          CHECK(cblock.compileConstants);
          CHECK(cblock.byteSize == 0);

          REQUIRE_ARRAY_SIZE(cblock.variables.size(), 2);
          {
            CHECK(cblock.variables[0].name == "foo");
            {
              const ShaderConstant &member = cblock.variables[0];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::SInt);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 1);
              CHECK(member.type.name == "int");
              CHECK(member.byteOffset == 0);

              CHECK(member.defaultValue == 12);
            }

            CHECK(cblock.variables[1].name == "bar");
            {
              const ShaderConstant &member = cblock.variables[1];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::Float);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 1);
              CHECK(member.type.name == "float");
              CHECK(member.byteOffset == 8);

              float defaultValueFloat;
              memcpy(&defaultValueFloat, &member.defaultValue, sizeof(float));
              CHECK(defaultValueFloat == 0.5f);
            }
          }
        }
      }
    };

    SECTION("Vulkan push constants")
    {
      rdcstr source = R"(
#version 450 core

layout(push_constant) uniform push
{
  int a;
  float b;
  uvec2 c;
} push_data;

void main() {
  gl_FragDepth = push_data.b;
}
)";

      ShaderReflection refl;
      compile(ShaderStage::Fragment, source, "main", refl);

      REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
      REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

      REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 1);
      {
        CHECK(refl.constantBlocks[0].name == "push_data");
        {
          const ConstantBlock &cblock = refl.constantBlocks[0];
          INFO("UBO: " << cblock.name.c_str());

          CHECK(cblock.bindArraySize == 1);
          CHECK(!cblock.bufferBacked);
          CHECK(!cblock.compileConstants);
          CHECK(cblock.byteSize == 16);

          REQUIRE_ARRAY_SIZE(cblock.variables.size(), 3);
          {
            CHECK(cblock.variables[0].name == "a");
            {
              const ShaderConstant &member = cblock.variables[0];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.byteOffset == 0);
              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::SInt);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 1);
              CHECK(member.type.name == "int");
            }

            CHECK(cblock.variables[1].name == "b");
            {
              const ShaderConstant &member = cblock.variables[1];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.byteOffset == 4);
              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::Float);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 1);
            }

            CHECK(cblock.variables[2].name == "c");
            {
              const ShaderConstant &member = cblock.variables[2];
              INFO("UBO member: " << member.name.c_str());

              CHECK(member.byteOffset == 8);
              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::UInt);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 2);
            }
          }
        }
      }
    };
  }
  else
  {
    RDCFATAL("Unexpected test type");
  }

  SECTION("Debug information")
  {
    rdcstr source = R"(
#version 450 core

layout(location = 3) in vec2 a_input;
layout(location = 6) flat in uvec3 z_input;

layout(location = 0) out vec4 a_output;
layout(location = 1) out vec3 z_output;
layout(location = 2) out int b_output;

void main() {
  a_output = vec4(a_input.y + gl_FragCoord.x, 0, 0, 1);
  z_output = vec3(a_output.xy, a_output.z);
  b_output = int(z_input.x);
  gl_FragDepth = float(z_output.y);
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

    CHECK(refl.entryPoint == "main");
    CHECK(refl.stage == ShaderStage::Fragment);

    CHECK(refl.debugInfo.encoding == ShaderEncoding::GLSL);

    REQUIRE(refl.debugInfo.files.size() == 1);

    CHECK(refl.debugInfo.files[0].contents == source);

    if(testType == ShaderType::GLSL)
    {
      CHECK(refl.debugInfo.files[0].filename == "main.glsl");
    }
    else
    {
      CHECK(refl.debugInfo.files[0].filename == "source0.glsl");

      REQUIRE(refl.debugInfo.compileFlags.flags.size() == 2);

      CHECK(refl.debugInfo.compileFlags.flags[0].name == "@cmdline");

      if(testType == ShaderType::GLSPIRV)
        CHECK(refl.debugInfo.compileFlags.flags[0].value ==
              " --client opengl100 --target-env opengl --entry-point main");
      else
        CHECK(refl.debugInfo.compileFlags.flags[0].value ==
              " --client vulkan100 --target-env vulkan1.0 --entry-point main");

      CHECK(refl.debugInfo.compileFlags.flags[1].name == "@spirver");
      CHECK(refl.debugInfo.compileFlags.flags[1].value == "spirv1.0");
    }
  };

  SECTION("Input and output signatures")
  {
    rdcstr source = R"(
#version 450 core

layout(location = 3) in vec2 a_input;
layout(location = 6) flat in uvec3 z_input;

layout(location = 0) out vec4 a_output;
layout(location = 1) out vec3 z_output;
layout(location = 2) out int b_output;

void main() {
  a_output = vec4(a_input.y + gl_FragCoord.x, 0, 0, 1);
  z_output = vec3(a_output.xy, a_output.z);
  b_output = int(z_input.x);
  gl_FragDepth = float(z_output.y);
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.inputSignature.size(), 3);
    {
      CHECK(refl.inputSignature[0].varName == "gl_FragCoord");
      {
        const SigParameter &sig = refl.inputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.inputSignature[1].varName == "a_input");
      {
        const SigParameter &sig = refl.inputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 3);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.inputSignature[2].varName == "z_input");
      {
        const SigParameter &sig = refl.inputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 6);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::UInt);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }
    }

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 4);
    {
      CHECK(refl.outputSignature[0].varName == "a_output");
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::ColorOutput);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName == "z_output");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 1);
        CHECK(sig.systemValue == ShaderBuiltin::ColorOutput);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[2].varName == "b_output");
      {
        const SigParameter &sig = refl.outputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 2);
        CHECK(sig.systemValue == ShaderBuiltin::ColorOutput);
        CHECK(sig.varType == VarType::SInt);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[3].varName == "gl_FragDepth");
      {
        const SigParameter &sig = refl.outputSignature[3];
        INFO("signature element: " << sig.varName.c_str());

        // when not running with a driver we default to just using the index instead of looking up
        // the location of outputs, so this will be wrong
        // CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::DepthOutput);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }
    }
  };

  SECTION("constant buffers")
  {
    rdcstr source = R"(
#version 450 core

struct glstruct
{
  float a;
  int b;
  mat2x2 c;
};

layout(binding = 8, std140) uniform ubo_block {
	float ubo_a;
	layout(column_major) mat4x3 ubo_b;
	layout(row_major) mat4x3 ubo_c;
  ivec2 ubo_d;
  vec2 ubo_e[3];
  glstruct ubo_f;
  layout(offset = 256) vec4 ubo_z;
} ubo_root;

void main() {
  gl_FragDepth = ubo_root.ubo_a;
}
)";

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 1);
    {
      // blocks get different reflected names in SPIR-V
      const rdcstr ubo_name = testType == ShaderType::GLSL ? "ubo_block" : "ubo_root";

      CHECK(refl.constantBlocks[0].name == ubo_name);
      {
        const ConstantBlock &cblock = refl.constantBlocks[0];
        INFO("UBO: " << cblock.name.c_str());

        // GLSL does not have register bindings as they're dynamic
        if(testType != ShaderType::GLSL)
        {
          CHECK(cblock.fixedBindSetOrSpace == 0);
          CHECK(cblock.fixedBindNumber == 8);
        }
        CHECK(cblock.bindArraySize == 1);
        CHECK(cblock.bufferBacked);
        CHECK(cblock.byteSize == 272);

        // GLSL reflects out a root structure
        if(testType == ShaderType::GLSL)
        {
          REQUIRE_ARRAY_SIZE(cblock.variables.size(), 1);

          CHECK(cblock.variables[0].name == ubo_name);
        }

        const rdcarray<ShaderConstant> &ubo_root =
            testType == ShaderType::GLSL ? cblock.variables[0].type.members : cblock.variables;

        REQUIRE_ARRAY_SIZE(ubo_root.size(), 7);
        {
          CHECK(ubo_root[0].name == "ubo_a");
          {
            const ShaderConstant &member = ubo_root[0];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 0);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.rows == 1);
            CHECK(member.type.columns == 1);
            CHECK(member.type.name == "float");
          }

          CHECK(ubo_root[1].name == "ubo_b");
          {
            const ShaderConstant &member = ubo_root[1];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 16);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.rows == 3);
            CHECK(member.type.columns == 4);
            CHECK(member.type.ColMajor());
          }

          CHECK(ubo_root[2].name == "ubo_c");
          {
            const ShaderConstant &member = ubo_root[2];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 80);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.rows == 3);
            CHECK(member.type.columns == 4);
            CHECK(member.type.RowMajor());
          }

          CHECK(ubo_root[3].name == "ubo_d");
          {
            const ShaderConstant &member = ubo_root[3];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 128);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::SInt);
            CHECK(member.type.rows == 1);
            CHECK(member.type.columns == 2);
          }

          CHECK(ubo_root[4].name == "ubo_e");
          {
            const ShaderConstant &member = ubo_root[4];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 144);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.rows == 1);
            CHECK(member.type.columns == 2);
            CHECK(member.type.elements == 3);
            CHECK(member.type.arrayByteStride == 16);
          }

          CHECK(ubo_root[5].name == "ubo_f");
          {
            const ShaderConstant &member = ubo_root[5];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 192);
            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.arrayByteStride == 48);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 3);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("UBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 0);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.rows == 1);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.name == "float");
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("UBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 4);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.baseType == VarType::SInt);
                CHECK(submember.type.rows == 1);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.name == "int");
              }

              CHECK(member.type.members[2].name == "c");
              {
                const ShaderConstant &submember = member.type.members[2];
                INFO("UBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 16);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.rows == 2);
                CHECK(submember.type.columns == 2);
                CHECK(submember.type.ColMajor());
              }
            }
          }

          CHECK(ubo_root[6].name == "ubo_z");
          {
            const ShaderConstant &member = ubo_root[6];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 256);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.rows == 1);
            CHECK(member.type.columns == 4);
          }
        }
      }
    }
  };

  SECTION("Textures")
  {
    rdcstr source = R"(
#version 450 core

layout(binding = 3) uniform sampler2D tex2D;
layout(binding = 5) uniform isampler3D tex3D;
layout(binding = 7) uniform samplerBuffer texBuf;

void main() {
  gl_FragDepth = textureLod(tex2D, gl_FragCoord.xy, gl_FragCoord.z).z +
                 float(texelFetch(tex3D, ivec3(gl_FragCoord.xyz), 0).y) + 
                 texelFetch(texBuf, int(gl_FragCoord.x)).x;
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 3);
    {
      CHECK(refl.readOnlyResources[0].name == "tex2D");
      {
        const ShaderResource &res = refl.readOnlyResources[0];
        INFO("read-only resource: " << res.name.c_str());

        // GLSL does not have register bindings as they're dynamic
        if(testType != ShaderType::GLSL)
        {
          CHECK(res.fixedBindSetOrSpace == 0);
          CHECK(res.fixedBindNumber == 3);
        }
        CHECK(res.bindArraySize == 1);
        CHECK(res.textureType == TextureType::Texture2D);
        CHECK(res.variableType.members.empty());
        CHECK(res.variableType.baseType == VarType::Float);
      }

      CHECK(refl.readOnlyResources[1].name == "tex3D");
      {
        const ShaderResource &res = refl.readOnlyResources[1];
        INFO("read-only resource: " << res.name.c_str());

        // GLSL does not have register bindings as they're dynamic
        if(testType != ShaderType::GLSL)
        {
          CHECK(res.fixedBindSetOrSpace == 0);
          CHECK(res.fixedBindNumber == 5);
        }
        CHECK(res.bindArraySize == 1);
        CHECK(res.textureType == TextureType::Texture3D);
        CHECK(res.variableType.members.empty());
        CHECK(res.variableType.baseType == VarType::SInt);
      }

      CHECK(refl.readOnlyResources[2].name == "texBuf");
      {
        const ShaderResource &res = refl.readOnlyResources[2];
        INFO("read-only resource: " << res.name.c_str());

        // GLSL does not have register bindings as they're dynamic
        if(testType != ShaderType::GLSL)
        {
          CHECK(res.fixedBindSetOrSpace == 0);
          CHECK(res.fixedBindNumber == 7);
        }
        CHECK(res.bindArraySize == 1);
        CHECK(res.textureType == TextureType::Buffer);
        CHECK(res.variableType.members.empty());
        CHECK(res.variableType.baseType == VarType::Float);
      }
    }
  };

#define REQUIRE_ARRAY_SIZE(size, min) \
  REQUIRE(size >= min);               \
  CHECK(size == min);

  SECTION("Infinite arrays")
  {
    rdcstr source = R"(
#version 450 core

layout(binding = 0, std430) buffer ssbo0
{
  float blah;
  vec4 normal_array[3];
  vec4 non_array;
} ssbo_root0;

layout(binding = 0, std430) buffer ssbo1
{
  float blah;
  vec4 normal_array[3];
  vec4 bounded_array[5];
} ssbo_root1;

layout(binding = 2, std430) buffer ssbo2
{
  float blah;
  vec4 normal_array[3];
  vec4 infinite_array[];
} ssbo_root2;

struct glstruct
{
  float a;
  int b;
};

struct struct_with_arrays
{
  float a[2];
  int b;
};

layout(binding = 3, std430) buffer ssbo3
{
  float blah;
  struct_with_arrays test;
  glstruct s[2];
} ssbo_root3;

layout(binding = 4, std430) buffer ssbo4
{
  float blah;
  struct_with_arrays test;
  glstruct s[];
} ssbo_root4;

layout(binding = 4, std430) buffer ssbo5
{
  float ssbo5_blah;
  struct_with_arrays ssbo5_test;
  glstruct ssbo5_s[2];
};

layout(binding = 5, std430) buffer ssbo6
{
  float ssbo6_blah;
  struct_with_arrays ssbo6_test;
  glstruct ssbo6_s[];
};

layout(binding = 0, std140) uniform ubo_block
{
  float blah;
  vec4 normal_array[3];
  vec4 infinite_array[];
} ubo_root;

void main() {
  ssbo_root0.blah = 0.0f;
  ssbo_root1.blah = 0.0f;
  ssbo_root2.blah = 0.0f;
  ssbo_root3.s[0].a = 0.0f;
  ssbo_root4.s[0].a = 0.0f;
  ssbo5_blah = 0.0f;
  ssbo6_blah = 0.0f;
  gl_FragDepth = ubo_root.blah + ubo_root.normal_array[0].x + ubo_root.infinite_array[4].x;
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 1);
    {
      // blocks get different reflected names in SPIR-V
      const rdcstr ubo_name = testType == ShaderType::GLSL ? "ubo_block" : "ubo_root";

      CHECK(refl.constantBlocks[0].name == ubo_name);
      {
        const ConstantBlock &cblock = refl.constantBlocks[0];
        INFO("UBO: " << cblock.name.c_str());

        // GLSL reflects out a root structure
        if(testType == ShaderType::GLSL)
        {
          REQUIRE_ARRAY_SIZE(cblock.variables.size(), 1);

          CHECK(cblock.variables[0].name == ubo_name);
        }

        const rdcarray<ShaderConstant> &ubo_root =
            testType == ShaderType::GLSL ? cblock.variables[0].type.members : cblock.variables;

        REQUIRE_ARRAY_SIZE(ubo_root.size(), 3);
        {
          CHECK(ubo_root[0].name == "blah");
          {
            const ShaderConstant &member = ubo_root[0];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(ubo_root[1].name == "normal_array");
          {
            const ShaderConstant &member = ubo_root[1];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == 3);
          }

          CHECK(ubo_root[2].name == "infinite_array");
          {
            const ShaderConstant &member = ubo_root[2];
            INFO("UBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            // UBOs don't really support infinite arrays - it will just be declared big enough for
            // the amount used statically
            CHECK(member.type.elements == 5);
          }
        }
      }
    }

    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 7);
    {
      // blocks get different reflected names in SPIR-V
      const rdcstr ssbo_name = testType == ShaderType::GLSL ? "ssbo" : "ssbo_root";
      const rdcstr ssbo_suffix = testType == ShaderType::GLSL ? "" : "_var";

      CHECK(refl.readWriteResources[0].name == ssbo_name + "0");
      {
        const ShaderResource &res = refl.readWriteResources[0];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "normal_array");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == 3);
          }

          CHECK(res.variableType.members[2].name == "non_array");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == 1);
          }
        }
      }

      CHECK(refl.readWriteResources[1].name == ssbo_name + "1");
      {
        const ShaderResource &res = refl.readWriteResources[1];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "normal_array");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == 3);
          }

          CHECK(res.variableType.members[2].name == "bounded_array");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == 5);
          }
        }
      }

      CHECK(refl.readWriteResources[2].name == ssbo_name + "2");
      {
        const ShaderResource &res = refl.readWriteResources[2];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "normal_array");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == 3);
          }

          CHECK(res.variableType.members[2].name == "infinite_array");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.columns == 4);
            CHECK(member.type.elements == ~0U);
          }
        }
      }

      CHECK(refl.readWriteResources[3].name == ssbo_name + "3");
      {
        const ShaderResource &res = refl.readWriteResources[3];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "test");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.elements == 1);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 2);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 2);
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::SInt);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 1);
              }
            }
          }

          CHECK(res.variableType.members[2].name == "s");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            if(testType == ShaderType::GLSL)
            {
              // GL has no way of telling us the fixed size of a trailing array of structs, so we
              // report that it's infinite
              CHECK(member.type.elements == ~0U);
            }
            else
            {
              CHECK(member.type.elements == 2);
            }
          }
        }
      }

      CHECK(refl.readWriteResources[4].name == ssbo_name + "4");
      {
        const ShaderResource &res = refl.readWriteResources[4];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "test");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.elements == 1);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 2);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 2);
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::SInt);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 1);
              }
            }
          }

          CHECK(res.variableType.members[2].name == "s");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.elements == ~0U);
          }
        }
      }

      CHECK(refl.readWriteResources[5].name == "ssbo5" + ssbo_suffix);
      {
        const ShaderResource &res = refl.readWriteResources[5];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "ssbo5_blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "ssbo5_test");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.elements == 1);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 2);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 2);
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::SInt);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 1);
              }
            }
          }

          CHECK(res.variableType.members[2].name == "ssbo5_s");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            if(testType == ShaderType::GLSL)
            {
              // GL has no way of telling us the fixed size of a trailing array of structs, so we
              // report that it's infinite
              CHECK(member.type.elements == ~0U);
            }
            else
            {
              CHECK(member.type.elements == 2);
            }
          }
        }
      }

      CHECK(refl.readWriteResources[6].name == "ssbo6" + ssbo_suffix);
      {
        const ShaderResource &res = refl.readWriteResources[6];
        INFO("read-write resource: " << res.name.c_str());

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "ssbo6_blah");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.elements == 1);
          }

          CHECK(res.variableType.members[1].name == "ssbo6_test");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.elements == 1);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 2);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 2);
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.type.baseType == VarType::SInt);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.elements == 1);
              }
            }
          }

          CHECK(res.variableType.members[2].name == "ssbo6_s");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.elements == ~0U);
          }
        }
      }
    }
  }

  SECTION("SSBOs")
  {
    rdcstr source = R"(
#version 450 core

struct glstruct
{
  float a;
  int b;
  mat2x2 c;
};

layout(binding = 2, std430) buffer ssbo
{
  uint ssbo_a[10];
  glstruct ssbo_b[3];
  float ssbo_c;
} ssbo_root;

struct nested
{
  glstruct first;
  glstruct second;
};

layout(binding = 5, std430) buffer ssbo2
{
  nested n[];
} ssbo_root2;

void main() {
  ssbo_root.ssbo_a[5] = 4;
  ssbo_root.ssbo_b[1].b = 6;
  ssbo_root2.n[5].second.b = 4;
  gl_FragDepth = ssbo_root.ssbo_c + ssbo_root2.n[0].first.a;
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 2);
    {
      // blocks get different reflected names in SPIR-V
      const rdcstr ssbo_name = testType == ShaderType::GLSL ? "ssbo" : "ssbo_root";

      CHECK(refl.readWriteResources[0].name == ssbo_name);
      {
        const ShaderResource &res = refl.readWriteResources[0];
        INFO("read-write resource: " << res.name.c_str());

        // GLSL does not have register bindings as they're dynamic
        if(testType != ShaderType::GLSL)
        {
          CHECK(res.fixedBindSetOrSpace == 0);
          CHECK(res.fixedBindNumber == 2);
        }
        CHECK(res.bindArraySize == 1);
        CHECK(res.textureType == TextureType::Buffer);

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 3);
        {
          CHECK(res.variableType.members[0].name == "ssbo_a");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 0);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::UInt);
            CHECK(member.type.rows == 1);
            CHECK(member.type.columns == 1);
            CHECK(member.type.elements == 10);
            CHECK(member.type.arrayByteStride == 4);
            CHECK(member.type.name == "uint");
          }

          CHECK(res.variableType.members[1].name == "ssbo_b");
          {
            const ShaderConstant &member = res.variableType.members[1];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 40);
            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.arrayByteStride == 24);
            CHECK(member.type.elements == 3);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 3);
            {
              CHECK(member.type.members[0].name == "a");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 0);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.rows == 1);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.name == "float");
              }

              CHECK(member.type.members[1].name == "b");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 4);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.baseType == VarType::SInt);
                CHECK(submember.type.rows == 1);
                CHECK(submember.type.columns == 1);
                CHECK(submember.type.name == "int");
              }

              CHECK(member.type.members[2].name == "c");
              {
                const ShaderConstant &submember = member.type.members[2];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 8);
                CHECK(submember.type.members.empty());
                CHECK(submember.type.baseType == VarType::Float);
                CHECK(submember.type.rows == 2);
                CHECK(submember.type.columns == 2);
                CHECK(submember.type.ColMajor());
              }
            }
          }

          CHECK(res.variableType.members[2].name == "ssbo_c");
          {
            const ShaderConstant &member = res.variableType.members[2];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 112);
            CHECK(member.type.members.empty());
            CHECK(member.type.baseType == VarType::Float);
            CHECK(member.type.rows == 1);
            CHECK(member.type.columns == 1);
            CHECK(member.type.name == "float");
          }
        }
      }

      CHECK(refl.readWriteResources[1].name == (ssbo_name + "2"));
      {
        const ShaderResource &res = refl.readWriteResources[1];
        INFO("read-write resource: " << res.name.c_str());

        // GLSL does not have register bindings as they're dynamic
        if(testType != ShaderType::GLSL)
        {
          CHECK(res.fixedBindSetOrSpace == 0);
          CHECK(res.fixedBindNumber == 5);
        }
        CHECK(res.bindArraySize == 1);
        CHECK(res.textureType == TextureType::Buffer);

        REQUIRE_ARRAY_SIZE(res.variableType.members.size(), 1);
        {
          CHECK(res.variableType.members[0].name == "n");
          {
            const ShaderConstant &member = res.variableType.members[0];
            INFO("SSBO member: " << member.name.c_str());

            CHECK(member.byteOffset == 0);
            CHECK(member.type.baseType == VarType::Struct);
            CHECK(member.type.arrayByteStride == 48);
            CHECK(member.type.elements == ~0U);

            REQUIRE_ARRAY_SIZE(member.type.members.size(), 2);
            {
              CHECK(member.type.members[0].name == "first");
              {
                const ShaderConstant &submember = member.type.members[0];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 0);
                CHECK(submember.type.baseType == VarType::Struct);
                CHECK(submember.type.arrayByteStride == 24);

                REQUIRE_ARRAY_SIZE(submember.type.members.size(), 3);
                {
                  CHECK(submember.type.members[0].name == "a");
                  {
                    const ShaderConstant &subsubmember = submember.type.members[0];
                    INFO("SSBO subsubmember: " << subsubmember.name.c_str());

                    CHECK(subsubmember.byteOffset == 0);
                    CHECK(subsubmember.type.members.empty());
                    CHECK(subsubmember.type.baseType == VarType::Float);
                    CHECK(subsubmember.type.rows == 1);
                    CHECK(subsubmember.type.columns == 1);
                    CHECK(subsubmember.type.name == "float");
                  }

                  CHECK(submember.type.members[1].name == "b");
                  {
                    const ShaderConstant &subsubmember = submember.type.members[1];
                    INFO("SSBO subsubmember: " << subsubmember.name.c_str());

                    CHECK(subsubmember.byteOffset == 4);
                    CHECK(subsubmember.type.members.empty());
                    CHECK(subsubmember.type.baseType == VarType::SInt);
                    CHECK(subsubmember.type.rows == 1);
                    CHECK(subsubmember.type.columns == 1);
                    CHECK(subsubmember.type.name == "int");
                  }

                  CHECK(submember.type.members[2].name == "c");
                  {
                    const ShaderConstant &subsubmember = submember.type.members[2];
                    INFO("SSBO subsubmember: " << subsubmember.name.c_str());

                    CHECK(subsubmember.byteOffset == 8);
                    CHECK(subsubmember.type.members.empty());
                    CHECK(subsubmember.type.baseType == VarType::Float);
                    CHECK(subsubmember.type.rows == 2);
                    CHECK(subsubmember.type.columns == 2);
                    CHECK(subsubmember.type.ColMajor());
                  }
                }
              }

              CHECK(member.type.members[1].name == "second");
              {
                const ShaderConstant &submember = member.type.members[1];
                INFO("SSBO submember: " << submember.name.c_str());

                CHECK(submember.byteOffset == 24);
                CHECK(submember.type.baseType == VarType::Struct);
                CHECK(submember.type.arrayByteStride == 24);

                REQUIRE_ARRAY_SIZE(submember.type.members.size(), 3);
                {
                  CHECK(submember.type.members[0].name == "a");
                  {
                    const ShaderConstant &subsubmember = submember.type.members[0];
                    INFO("SSBO subsubmember: " << subsubmember.name.c_str());

                    CHECK(subsubmember.byteOffset == 0);
                    CHECK(subsubmember.type.members.empty());
                    CHECK(subsubmember.type.baseType == VarType::Float);
                    CHECK(subsubmember.type.rows == 1);
                    CHECK(subsubmember.type.columns == 1);
                    CHECK(subsubmember.type.name == "float");
                  }

                  CHECK(submember.type.members[1].name == "b");
                  {
                    const ShaderConstant &subsubmember = submember.type.members[1];
                    INFO("SSBO subsubmember: " << subsubmember.name.c_str());

                    CHECK(subsubmember.byteOffset == 4);
                    CHECK(subsubmember.type.members.empty());
                    CHECK(subsubmember.type.baseType == VarType::SInt);
                    CHECK(subsubmember.type.rows == 1);
                    CHECK(subsubmember.type.columns == 1);
                    CHECK(subsubmember.type.name == "int");
                  }

                  CHECK(submember.type.members[2].name == "c");
                  {
                    const ShaderConstant &subsubmember = submember.type.members[2];
                    INFO("SSBO subsubmember: " << subsubmember.name.c_str());

                    CHECK(subsubmember.byteOffset == 8);
                    CHECK(subsubmember.type.members.empty());
                    CHECK(subsubmember.type.baseType == VarType::Float);
                    CHECK(subsubmember.type.rows == 2);
                    CHECK(subsubmember.type.columns == 2);
                    CHECK(subsubmember.type.ColMajor());
                  }
                }
              }
            }
          }
        }
      }
    }
  };

  SECTION("vertex shader fixed function outputs")
  {
    rdcstr source = R"(
#version 450 core

void main() {
  gl_Position = vec4(0, 1, 0, 1);
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Vertex, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 1);
    {
      CHECK(refl.outputSignature[0].varName.contains("gl_Position"));
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }
    }

    rdcstr source2 = R"(
#version 450 core

void main() {
  gl_Position = vec4(0, 1, 0, 1);
  gl_PointSize = 1.5f;
}

)";

    refl = ShaderReflection();
    compile(ShaderStage::Vertex, source2, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 2);
    {
      CHECK(refl.outputSignature[0].varName.contains("gl_Position"));
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName.contains("gl_PointSize"));
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::PointSize);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }
    }
  };

  SECTION("matrix and 1D array outputs")
  {
    rdcstr source = R"(
#version 450 core

layout(location = 0) out vec3 outarr[3];
layout(location = 6) out mat2 outmat;
layout(location = 9) out mat2 outmatarr[2];

void main()
{
  gl_Position = vec4(0, 0, 0, 1);
  outarr[0] = gl_Position.xyz;
  outarr[1] = gl_Position.xyz;
  outarr[2] = gl_Position.xyz;
  outmat = mat2(0, 0, 0, 0);
  outmatarr[0] = mat2(0, 0, 0, 0);
  outmatarr[1] = mat2(0, 0, 0, 0);
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Vertex, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 10);
    {
      CHECK(refl.outputSignature[0].varName.contains("gl_Position"));
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName == "outarr[0]");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[2].varName == "outarr[1]");
      {
        const SigParameter &sig = refl.outputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 1);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[3].varName == "outarr[2]");
      {
        const SigParameter &sig = refl.outputSignature[3];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 2);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[4].varName == "outmat:col0");
      {
        const SigParameter &sig = refl.outputSignature[4];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 6);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[5].varName == "outmat:col1");
      {
        const SigParameter &sig = refl.outputSignature[5];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 7);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[6].varName == "outmatarr[0]:col0");
      {
        const SigParameter &sig = refl.outputSignature[6];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 9);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[7].varName == "outmatarr[0]:col1");
      {
        const SigParameter &sig = refl.outputSignature[7];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 10);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[8].varName == "outmatarr[1]:col0");
      {
        const SigParameter &sig = refl.outputSignature[8];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 11);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }

      CHECK(refl.outputSignature[9].varName == "outmatarr[1]:col1");
      {
        const SigParameter &sig = refl.outputSignature[9];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 12);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }
    }
  };

  // this is an annoying one. We want to specify a location explicitly to be GL/SPIR-V compatible,
  // but on GL if we specify a location the location assignment handling breaks. Since we only
  // need to handle this for tests (real drivers will let us query the locations when needed) AND
  // it's an extremely obtuse scenario, we just let GL have no location
  const rdcstr locDefine = (testType == ShaderType::GLSPIRV || testType == ShaderType::Vulkan)
                               ? "#define LOC(l) layout(location = l)"
                               : "#define LOC(l)";

  SECTION("nested struct/array inputs/outputs")
  {
    rdcstr source = R"(
#version 450 core

)" + locDefine + R"(

struct leaf
{
  float x;
};

struct nest
{
  float a[2];
  leaf b[2];
};

struct base
{
  float a;
  vec3 b;
  nest c[2];
};

layout(binding = 0, std140) uniform ubo_block {
	base inB;
} ubo_root;

LOC(0) out base outB;

void main()
{
  gl_Position = vec4(0, 0, 0, 1);
  outB = ubo_root.inB;
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Vertex, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 11);
    {
      CHECK(refl.outputSignature[0].varName.contains("gl_Position"));
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName == "outB.a");
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[2].varName == "outB.b");
      {
        const SigParameter &sig = refl.outputSignature[2];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 1);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 3);
        CHECK(sig.regChannelMask == 0x7);
        CHECK(sig.channelUsedMask == 0x7);
      }

      CHECK(refl.outputSignature[3].varName == "outB.c[0].a[0]");
      {
        const SigParameter &sig = refl.outputSignature[3];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 2);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[4].varName == "outB.c[0].a[1]");
      {
        const SigParameter &sig = refl.outputSignature[4];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 3);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[5].varName == "outB.c[0].b[0].x");
      {
        const SigParameter &sig = refl.outputSignature[5];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 4);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[6].varName == "outB.c[0].b[1].x");
      {
        const SigParameter &sig = refl.outputSignature[6];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 5);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[7].varName == "outB.c[1].a[0]");
      {
        const SigParameter &sig = refl.outputSignature[7];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 6);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[8].varName == "outB.c[1].a[1]");
      {
        const SigParameter &sig = refl.outputSignature[8];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 7);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[9].varName == "outB.c[1].b[0].x");
      {
        const SigParameter &sig = refl.outputSignature[9];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 8);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }

      CHECK(refl.outputSignature[10].varName == "outB.c[1].b[1].x");
      {
        const SigParameter &sig = refl.outputSignature[10];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 9);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 1);
        CHECK(sig.regChannelMask == 0x1);
        CHECK(sig.channelUsedMask == 0x1);
      }
    }
  }

  SECTION("multi-dimensional array inputs/outputs")
  {
    rdcstr source = R"(
#version 450 core

)" + locDefine + R"(

LOC(0) in vec3 inarr[1][3][2];

LOC(0) out vec3 outarr[1][3][2];

void main()
{
  gl_Position = vec4(0, 0, 0, 1);
  for(int i=0; i < 1*3*2; i++)
    outarr[(i/6)][(i/2)%3][i%2] = inarr[(i/6)][(i/2)%3][i%2];
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Vertex, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);

    REQUIRE(refl.inputSignature.size() >= 6);

    // glslang will insert gl_VertexID and gl_InstanceID here in SPIR-V compilation
    CHECK((refl.inputSignature.size() == 6 || refl.inputSignature.size() == 8));

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 7);
    for(size_t i = 0; i < 2; i++)
    {
      const rdcarray<SigParameter> &sigarray = (i == 0) ? refl.inputSignature : refl.outputSignature;
      size_t idx = 0;

      if(i == 0)
      {
        if(sigarray[0].varName.contains("gl_VertexID"))
        {
          // skip without checking
          idx++;
        }
        if(sigarray[1].varName.contains("gl_InstanceID"))
        {
          // skip without checking
          idx++;
        }
      }
      else if(i == 1)
      {
        CHECK(sigarray[0].varName.contains("gl_Position"));
        {
          const SigParameter &sig = sigarray[0];
          INFO("signature element: " << sig.varName.c_str());

          CHECK(sig.regIndex == 0);
          CHECK(sig.systemValue == ShaderBuiltin::Position);
          CHECK(sig.varType == VarType::Float);
          CHECK(sig.compCount == 4);
          CHECK(sig.regChannelMask == 0xf);
          CHECK(sig.channelUsedMask == 0xf);
        }

        idx++;
      }

      for(uint32_t a = 0; a < 6; a++)
      {
        rdcstr expectedName = StringFormat::Fmt("%sarr[%d][%d][%d]", i == 0 ? "in" : "out", a / 6,
                                                (a / 2) % 3, (a % 2));

        CHECK(sigarray[idx].varName == expectedName);
        {
          const SigParameter &sig = sigarray[idx];
          INFO("signature element: " << sig.varName.c_str());

          CHECK(sig.regIndex == a);
          CHECK(sig.systemValue == ShaderBuiltin::Undefined);
          CHECK(sig.varType == VarType::Float);
          CHECK(sig.compCount == 3);
          CHECK(sig.regChannelMask == 0x7);
          CHECK(sig.channelUsedMask == 0x7);
        }

        idx++;
      }
    }
  };

  SECTION("shader input/output blocks")
  {
    rdcstr source = R"(
#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location = 0) in block
{
	vec2 Texcoord;
} In[];

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out block
{
	vec2 Texcoord;
} Out;

void main()
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = gl_in[i].gl_Position;
		Out.Texcoord = In[i].Texcoord;
		EmitVertex();
	}
	EndPrimitive();
}

)";

    ShaderReflection refl;
    compile(ShaderStage::Geometry, source, "main", refl);

    REQUIRE_ARRAY_SIZE(refl.samplers.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), 0);
    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.inputSignature.size(), 2);
    {
      // blocks get different reflected names in SPIR-V
      const rdcstr gl_in_name = testType == ShaderType::GLSL ? "gl_PerVertex" : "gl_in";
      const rdcstr block_name = testType == ShaderType::GLSL ? "block" : "In";

      CHECK(refl.inputSignature[0].varName == (gl_in_name + ".gl_Position"));
      {
        const SigParameter &sig = refl.inputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.inputSignature[1].varName == (block_name + ".Texcoord"));
      {
        const SigParameter &sig = refl.inputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }
    }

    REQUIRE_ARRAY_SIZE(refl.outputSignature.size(), 2);
    {
      const rdcstr block_name = testType == ShaderType::GLSL ? "block" : "Out";

      CHECK(refl.outputSignature[0].varName.contains("gl_Position"));
      {
        const SigParameter &sig = refl.outputSignature[0];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Position);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 4);
        CHECK(sig.regChannelMask == 0xf);
        CHECK(sig.channelUsedMask == 0xf);
      }

      CHECK(refl.outputSignature[1].varName == (block_name + ".Texcoord"));
      {
        const SigParameter &sig = refl.outputSignature[1];
        INFO("signature element: " << sig.varName.c_str());

        CHECK(sig.regIndex == 0);
        CHECK(sig.systemValue == ShaderBuiltin::Undefined);
        CHECK(sig.varType == VarType::Float);
        CHECK(sig.compCount == 2);
        CHECK(sig.regChannelMask == 0x3);
        CHECK(sig.channelUsedMask == 0x3);
      }
    }
  };

  SECTION("Arrays of opaque resources")
  {
    rdcstr source = R"(
#version 450 core

layout(binding = 2, std430) buffer ssbo
{
  float a;
  int b;
} ssbo_root[];

layout(binding = 3) uniform sampler2D tex2D[];

void main() {
  ssbo_root[4].b = 4;
  gl_FragDepth = ssbo_root[4].a + textureLod(tex2D[6], gl_FragCoord.xy, gl_FragCoord.z).z;
}

)";

#define REQUIRE_ARRAY_SIZE(size, min) \
  REQUIRE(size >= min);               \
  CHECK(size == min);

    ShaderReflection refl;
    compile(ShaderStage::Fragment, source, "main", refl);

    // GLSL 'expands' these arrays
    size_t countRO = (testType == ShaderType::GLSL ? 7 : 1);
    size_t arraySizeRO = (testType == ShaderType::GLSL ? 1 : 7);

    REQUIRE_ARRAY_SIZE(refl.constantBlocks.size(), 0);

    REQUIRE_ARRAY_SIZE(refl.readOnlyResources.size(), countRO);
    {
      for(size_t i = 0; i < countRO; i++)
      {
        const rdcstr ro_name =
            (testType == ShaderType::GLSL ? StringFormat::Fmt("tex2D[%zu]", i) : "tex2D");

        CHECK(refl.readOnlyResources[i].name == ro_name);
        {
          const ShaderResource &res = refl.readOnlyResources[i];
          INFO("read-only resource: " << res.name.c_str());

          // GLSL does not have register bindings as they're dynamic
          if(testType != ShaderType::GLSL)
          {
            CHECK(res.fixedBindSetOrSpace == 0);
            CHECK(res.fixedBindNumber == 3 + i);
          }
          CHECK(res.bindArraySize == arraySizeRO);
          CHECK(res.textureType == TextureType::Texture2D);
          CHECK(res.variableType.members.empty());
          CHECK(res.variableType.baseType == VarType::Float);
        }
      }
    }

    size_t countRW = (testType == ShaderType::GLSL ? 5 : 1);
    size_t arraySizeRW = (testType == ShaderType::GLSL ? 1 : 5);

    REQUIRE_ARRAY_SIZE(refl.readWriteResources.size(), countRW);
    {
      for(size_t i = 0; i < countRW; i++)
      {
        // blocks get different reflected names in SPIR-V
        const rdcstr ssbo_name =
            (testType == ShaderType::GLSL ? StringFormat::Fmt("ssbo[%zu]", i) : "ssbo_root");

        CHECK(refl.readWriteResources[i].name == ssbo_name);
        {
          const ShaderResource &res = refl.readWriteResources[i];
          INFO("read-write resource: " << res.name.c_str());

          // GLSL does not have register bindings as they're dynamic
          if(testType != ShaderType::GLSL)
          {
            CHECK(res.fixedBindSetOrSpace == 0);
            CHECK(res.fixedBindNumber == 2 + i);
          }
          CHECK(res.bindArraySize == arraySizeRW);
          CHECK(res.textureType == TextureType::Buffer);

          // due to a bug in glslang the reflection is broken for these SSBOs. So we can still run
          // this test on GLSL we do a little hack here, which can get removed when we update
          // glslang with the fix
          const ShaderConstantType *varType = &res.variableType;

          REQUIRE_ARRAY_SIZE(varType->members.size(), 2);
          {
            CHECK(varType->members[0].name == "a");
            {
              const ShaderConstant &member = varType->members[0];
              INFO("SSBO member: " << member.name.c_str());

              CHECK(member.byteOffset == 0);
              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::Float);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 1);
              CHECK(member.type.name == "float");
            }

            CHECK(varType->members[1].name == "b");
            {
              const ShaderConstant &member = varType->members[1];
              INFO("SSBO member: " << member.name.c_str());

              CHECK(member.byteOffset == 4);
              CHECK(member.type.members.empty());
              CHECK(member.type.baseType == VarType::SInt);
              CHECK(member.type.rows == 1);
              CHECK(member.type.columns == 1);
              CHECK(member.type.name == "int");
            }
          }
        }
      }
    }
  };
}

#endif
