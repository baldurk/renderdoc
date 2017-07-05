/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "common/common.h"
#include "spirv_common.h"

#undef min
#undef max

#include "3rdparty/glslang/SPIRV/GlslangToSpv.h"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

TBuiltInResource DefaultResources = {
    /*.maxLights =*/32,
    /*.maxClipPlanes =*/6,
    /*.maxTextureUnits =*/32,
    /*.maxTextureCoords =*/32,
    /*.maxVertexAttribs =*/64,
    /*.maxVertexUniformComponents =*/4096,
    /*.maxVaryingFloats =*/64,
    /*.maxVertexTextureImageUnits =*/32,
    /*.maxCombinedTextureImageUnits =*/80,
    /*.maxTextureImageUnits =*/32,
    /*.maxFragmentUniformComponents =*/4096,
    /*.maxDrawBuffers =*/32,
    /*.maxVertexUniformVectors =*/128,
    /*.maxVaryingVectors =*/8,
    /*.maxFragmentUniformVectors =*/16,
    /*.maxVertexOutputVectors =*/16,
    /*.maxFragmentInputVectors =*/15,
    /*.minProgramTexelOffset =*/-8,
    /*.maxProgramTexelOffset =*/7,
    /*.maxClipDistances =*/8,
    /*.maxComputeWorkGroupCountX =*/65535,
    /*.maxComputeWorkGroupCountY =*/65535,
    /*.maxComputeWorkGroupCountZ =*/65535,
    /*.maxComputeWorkGroupSizeX =*/1024,
    /*.maxComputeWorkGroupSizeY =*/1024,
    /*.maxComputeWorkGroupSizeZ =*/64,
    /*.maxComputeUniformComponents =*/1024,
    /*.maxComputeTextureImageUnits =*/16,
    /*.maxComputeImageUniforms =*/8,
    /*.maxComputeAtomicCounters =*/8,
    /*.maxComputeAtomicCounterBuffers =*/1,
    /*.maxVaryingComponents =*/60,
    /*.maxVertexOutputComponents =*/64,
    /*.maxGeometryInputComponents =*/64,
    /*.maxGeometryOutputComponents =*/128,
    /*.maxFragmentInputComponents =*/128,
    /*.maxImageUnits =*/8,
    /*.maxCombinedImageUnitsAndFragmentOutputs =*/8,
    /*.maxCombinedShaderOutputResources =*/8,
    /*.maxImageSamples =*/0,
    /*.maxVertexImageUniforms =*/0,
    /*.maxTessControlImageUniforms =*/0,
    /*.maxTessEvaluationImageUniforms =*/0,
    /*.maxGeometryImageUniforms =*/0,
    /*.maxFragmentImageUniforms =*/8,
    /*.maxCombinedImageUniforms =*/8,
    /*.maxGeometryTextureImageUnits =*/16,
    /*.maxGeometryOutputVertices =*/256,
    /*.maxGeometryTotalOutputComponents =*/1024,
    /*.maxGeometryUniformComponents =*/1024,
    /*.maxGeometryVaryingComponents =*/64,
    /*.maxTessControlInputComponents =*/128,
    /*.maxTessControlOutputComponents =*/128,
    /*.maxTessControlTextureImageUnits =*/16,
    /*.maxTessControlUniformComponents =*/1024,
    /*.maxTessControlTotalOutputComponents =*/4096,
    /*.maxTessEvaluationInputComponents =*/128,
    /*.maxTessEvaluationOutputComponents =*/128,
    /*.maxTessEvaluationTextureImageUnits =*/16,
    /*.maxTessEvaluationUniformComponents =*/1024,
    /*.maxTessPatchComponents =*/120,
    /*.maxPatchVertices =*/32,
    /*.maxTessGenLevel =*/64,
    /*.maxViewports =*/16,
    /*.maxVertexAtomicCounters =*/0,
    /*.maxTessControlAtomicCounters =*/0,
    /*.maxTessEvaluationAtomicCounters =*/0,
    /*.maxGeometryAtomicCounters =*/0,
    /*.maxFragmentAtomicCounters =*/8,
    /*.maxCombinedAtomicCounters =*/8,
    /*.maxAtomicCounterBindings =*/1,
    /*.maxVertexAtomicCounterBuffers =*/0,
    /*.maxTessControlAtomicCounterBuffers =*/0,
    /*.maxTessEvaluationAtomicCounterBuffers =*/0,
    /*.maxGeometryAtomicCounterBuffers =*/0,
    /*.maxFragmentAtomicCounterBuffers =*/1,
    /*.maxCombinedAtomicCounterBuffers =*/1,
    /*.maxAtomicCounterBufferSize =*/16384,
    /*.maxTransformFeedbackBuffers =*/4,
    /*.maxTransformFeedbackInterleavedComponents =*/64,
    /*.maxCullDistances =*/8,
    /*.maxCombinedClipAndCullDistances =*/8,
    /*.maxSamples =*/4,

    /*.limits*/
    {
        /*.limits.nonInductiveForLoops =*/1,
        /*.limits.whileLoops =*/1,
        /*.limits.doWhileLoops =*/1,
        /*.limits.generalUniformIndexing =*/1,
        /*.limits.generalAttributeMatrixVectorIndexing =*/1,
        /*.limits.generalVaryingIndexing =*/1,
        /*.limits.generalSamplerIndexing =*/1,
        /*.limits.generalVariableIndexing =*/1,
        /*.limits.generalConstantMatrixVectorIndexing =*/1,
    },
};

string CompileSPIRV(const SPIRVCompilationSettings &settings,
                    const std::vector<std::string> &sources, vector<uint32_t> &spirv)
{
  if(settings.stage == SPIRVShaderStage::Invalid)
    return "Invalid shader stage specified";

  string errors = "";

  const char **strs = new const char *[sources.size()];

  for(size_t i = 0; i < sources.size(); i++)
    strs[i] = sources[i].c_str();

  RDCCOMPILE_ASSERT((int)EShLangVertex == (int)SPIRVShaderStage::Vertex &&
                        (int)EShLangTessControl == (int)SPIRVShaderStage::TessControl &&
                        (int)EShLangTessEvaluation == (int)SPIRVShaderStage::TessEvaluation &&
                        (int)EShLangGeometry == (int)SPIRVShaderStage::Geometry &&
                        (int)EShLangCompute == (int)SPIRVShaderStage::Compute,
                    "Shader language enums don't match");

  {
    // these enums are matched
    EShLanguage lang = EShLanguage(settings.stage);

    glslang::TShader *shader = new glslang::TShader(lang);

    shader->setStrings(strs, (int)sources.size());

    if(!settings.entryPoint.empty())
      shader->setEntryPoint(settings.entryPoint.c_str());

    EShMessages flags = EShMsgSpvRules;

    if(settings.lang == SPIRVSourceLanguage::VulkanGLSL)
      flags = EShMessages(flags | EShMsgVulkanRules);
    if(settings.lang == SPIRVSourceLanguage::VulkanHLSL)
      flags = EShMessages(flags | EShMsgVulkanRules | EShMsgReadHlsl);

    bool success = shader->parse(&DefaultResources, 110, false, flags);

    if(!success)
    {
      errors = "Shader failed to compile:\n\n";
      errors += shader->getInfoLog();
      errors += "\n\n";
      errors += shader->getInfoDebugLog();
    }
    else
    {
      glslang::TProgram *program = new glslang::TProgram();

      program->addShader(shader);

      success = program->link(EShMsgDefault);

      if(!success)
      {
        errors = "Program failed to link:\n\n";
        errors += program->getInfoLog();
        errors += "\n\n";
        errors += program->getInfoDebugLog();
      }
      else
      {
        glslang::TIntermediate *intermediate = program->getIntermediate(lang);

        // if we successfully compiled and linked, we must have the stage we started with
        RDCASSERT(intermediate);

        glslang::GlslangToSpv(*intermediate, spirv);
      }

      delete program;
    }

    delete shader;
  }

  delete[] strs;

  return errors;
}
