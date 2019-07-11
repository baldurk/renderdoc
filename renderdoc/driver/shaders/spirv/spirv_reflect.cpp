/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "spirv_reflect.h"
#include "replay/replay_driver.h"
#include "spirv_editor.h"

void FillSpecConstantVariables(const rdcarray<ShaderConstant> &invars,
                               rdcarray<ShaderVariable> &outvars,
                               const std::vector<SpecConstant> &specInfo)
{
  StandardFillCBufferVariables(invars, outvars, bytebuf());

  RDCASSERTEQUAL(invars.size(), outvars.size());

  for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
  {
    outvars[v].value.uv[0] = (invars[v].defaultValue & 0xFFFFFFFF);
    outvars[v].value.uv[1] = ((invars[v].defaultValue >> 32) & 0xFFFFFFFF);
  }

  // find any actual values specified
  for(size_t i = 0; i < specInfo.size(); i++)
  {
    for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
    {
      if(specInfo[i].specID == invars[v].byteOffset)
      {
        memcpy(outvars[v].value.uv, specInfo[i].data.data(),
               RDCMIN(specInfo[i].data.size(), sizeof(outvars[v].value.uv)));
      }
    }
  }
}

void AddXFBAnnotations(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                       const char *entryName, std::vector<uint32_t> &modSpirv, uint32_t &xfbStride)
{
  rdcspv::Editor editor(modSpirv);

  rdcarray<SigParameter> outsig = refl.outputSignature;
  std::vector<SPIRVPatchData::InterfaceAccess> outpatch = patchData.outputs;

  rdcspv::Id entryid;
  for(const rdcspv::OpEntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
    {
      entryid = entry.entryPoint;
      break;
    }
  }

  bool hasXFB = false;

  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::ExecutionMode);
      it < editor.End(rdcspv::Section::ExecutionMode); ++it)
  {
    rdcspv::OpExecutionMode execMode(it);

    if(execMode.entryPoint == entryid && execMode.mode == rdcspv::ExecutionMode::Xfb)
    {
      hasXFB = true;
      break;
    }
  }

  if(hasXFB)
  {
    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations);
        it < editor.End(rdcspv::Section::Annotations); ++it)
    {
      // remove any existing xfb decorations
      if(it.opcode() == rdcspv::Op::Decorate)
      {
        rdcspv::OpDecorate decorate(it);

        if(decorate.decoration == rdcspv::Decoration::XfbBuffer ||
           decorate.decoration == rdcspv::Decoration::XfbStride)
        {
          editor.Remove(it);
        }
      }

      // offset is trickier, need to see if it'll match one we want later
      if((it.opcode() == rdcspv::Op::Decorate &&
          rdcspv::OpDecorate(it).decoration == rdcspv::Decoration::Offset) ||
         (it.opcode() == rdcspv::Op::MemberDecorate &&
          rdcspv::OpMemberDecorate(it).decoration == rdcspv::Decoration::Offset))
      {
        for(size_t i = 0; i < outsig.size(); i++)
        {
          if(outpatch[i].structID)
          {
            if(it.opcode() == rdcspv::Op::MemberDecorate && it.word(1) == outpatch[i].structID &&
               it.word(2) == outpatch[i].structMemberIndex)
            {
              editor.Remove(it);
            }
          }
          else
          {
            if(it.opcode() == rdcspv::Op::Decorate && rdcspv::OpDecorate(it).target == outpatch[i].ID)
            {
              editor.Remove(it);
            }
          }
        }
      }
    }
  }
  else
  {
    editor.AddExecutionMode(rdcspv::OpExecutionMode(entryid, rdcspv::ExecutionMode::Xfb));
  }

  editor.AddCapability(rdcspv::Capability::TransformFeedback);

  // find the position output and move it to the front
  for(size_t i = 0; i < outsig.size(); i++)
  {
    if(outsig[i].systemValue == ShaderBuiltin::Position)
    {
      outsig.insert(0, outsig[i]);
      outsig.erase(i + 1);

      outpatch.insert(outpatch.begin(), outpatch[i]);
      outpatch.erase(outpatch.begin() + i + 1);
      break;
    }
  }

  for(size_t i = 0; i < outsig.size(); i++)
  {
    if(outpatch[i].isArraySubsequentElement)
    {
      // do not patch anything as we only patch the base array, but reserve space in the stride
    }
    else if(outpatch[i].structID && !outpatch[i].accessChain.empty())
    {
      editor.AddDecoration(rdcspv::OpMemberDecorate(
          rdcspv::Id::fromWord(outpatch[i].structID), outpatch[i].structMemberIndex,
          rdcspv::DecorationParam<rdcspv::Decoration::Offset>(xfbStride)));
    }
    else if(outpatch[i].ID)
    {
      editor.AddDecoration(
          rdcspv::OpDecorate(rdcspv::Id::fromWord(outpatch[i].ID),
                             rdcspv::DecorationParam<rdcspv::Decoration::Offset>(xfbStride)));
    }

    uint32_t compByteSize = 4;

    if(outsig[i].compType == CompType::Double)
      compByteSize = 8;

    xfbStride += outsig[i].compCount * compByteSize;
  }

  std::set<uint32_t> vars;

  for(size_t i = 0; i < outpatch.size(); i++)
  {
    if(outpatch[i].ID && !outpatch[i].isArraySubsequentElement &&
       vars.find(outpatch[i].ID) == vars.end())
    {
      editor.AddDecoration(
          rdcspv::OpDecorate(rdcspv::Id::fromWord(outpatch[i].ID),
                             rdcspv::DecorationParam<rdcspv::Decoration::XfbBuffer>(0)));
      editor.AddDecoration(
          rdcspv::OpDecorate(rdcspv::Id::fromWord(outpatch[i].ID),
                             rdcspv::DecorationParam<rdcspv::Decoration::XfbStride>(xfbStride)));
      vars.insert(outpatch[i].ID);
    }
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"
#include "data/glsl_shaders.h"
#include "glslang_compile.h"

TEST_CASE("Validate SPIR-V reflection", "[spirv][reflection]")
{
  ShaderType type = ShaderType::eShaderVulkan;
  auto compiler = [&type](ShaderStage stage, const std::string &source, const std::string &entryPoint,
                          ShaderReflection &refl, ShaderBindpointMapping &mapping) {

    rdcspv::Init();
    RenderDoc::Inst().RegisterShutdownFunction(&rdcspv::Shutdown);

    std::vector<uint32_t> spirv;
    rdcspv::CompilationSettings settings(type == ShaderType::eShaderVulkan
                                             ? rdcspv::InputLanguage::VulkanGLSL
                                             : rdcspv::InputLanguage::OpenGLGLSL,
                                         rdcspv::ShaderStage(stage));
    std::string errors = rdcspv::Compile(settings, {source}, spirv);

    INFO("SPIR-V compile output: " << errors);

    REQUIRE(!spirv.empty());

    SPVModule spv;
    ParseSPIRV(spirv.data(), spirv.size(), spv);

    SPIRVPatchData patchData;
    spv.MakeReflection(type == ShaderType::eShaderVulkan ? GraphicsAPI::Vulkan : GraphicsAPI::OpenGL,
                       stage, entryPoint, refl, mapping, patchData);
  };

  // test both Vulkan and GL SPIR-V reflection
  SECTION("Vulkan GLSL reflection")
  {
    type = ShaderType::eShaderVulkan;
    TestGLSLReflection(type, compiler);
  };

  SECTION("OpenGL GLSL reflection")
  {
    type = ShaderType::eShaderGLSPIRV;
    TestGLSLReflection(type, compiler);
  };
}

#endif