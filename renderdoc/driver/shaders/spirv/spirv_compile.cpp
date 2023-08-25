/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "spirv_compile.h"
#include <vector>
#include "common/common.h"
#include "common/formatting.h"
#include "glslang_compile.h"

#undef min
#undef max

#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/glslang/Public/ResourceLimits.h"
#include "glslang/glslang/Public/ShaderLang.h"

rdcstr rdcspv::Compile(const rdcspv::CompilationSettings &settings, const rdcarray<rdcstr> &sources,
                       rdcarray<uint32_t> &spirv)
{
  if(settings.stage == rdcspv::ShaderStage::Invalid)
    return "Invalid shader stage specified";

  rdcstr errors = "";

  const char **strs = new const char *[sources.size()];
  const char **names = new const char *[sources.size()];
  rdcarray<rdcstr> names_str;
  names_str.resize(sources.size());

  for(size_t i = 0; i < sources.size(); i++)
  {
    strs[i] = sources[i].c_str();
    names_str[i] = StringFormat::Fmt("source%d.glsl", i);
    names[i] = names_str[i].c_str();
  }

  RDCCOMPILE_ASSERT((int)EShLangVertex == (int)rdcspv::ShaderStage::Vertex &&
                        (int)EShLangTessControl == (int)rdcspv::ShaderStage::TessControl &&
                        (int)EShLangTessEvaluation == (int)rdcspv::ShaderStage::TessEvaluation &&
                        (int)EShLangGeometry == (int)rdcspv::ShaderStage::Geometry &&
                        (int)EShLangCompute == (int)rdcspv::ShaderStage::Compute &&
                        (int)EShLangTask == (int)rdcspv::ShaderStage::Task &&
                        (int)EShLangMesh == (int)rdcspv::ShaderStage::Mesh,
                    "Shader language enums don't match");

  {
    // these enums are matched
    EShLanguage lang = EShLanguage(settings.stage);

    glslang::TShader *shader = new glslang::TShader(lang);

    shader->setStringsWithLengthsAndNames(strs, NULL, names, (int)sources.size());

    if(!settings.entryPoint.empty())
      shader->setEntryPoint(settings.entryPoint.c_str());

    EShMessages flags = EShMsgSpvRules;

    if(settings.lang == rdcspv::InputLanguage::VulkanGLSL)
      flags = EShMessages(flags | EShMsgVulkanRules);
    if(settings.lang == rdcspv::InputLanguage::VulkanHLSL)
      flags = EShMessages(flags | EShMsgVulkanRules | EShMsgReadHlsl);

    if(settings.debugInfo)
      flags = EShMessages(flags | EShMsgDebugInfo);

    bool success = shader->parse(GetDefaultResources(), settings.gles ? 100 : 110, false, flags);

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

        glslang::SpvOptions opts;
        if(settings.debugInfo)
          opts.generateDebugInfo = true;

        std::vector<uint32_t> spirvVec;
        glslang::GlslangToSpv(*intermediate, spirvVec, &opts);

        spirv.assign(spirvVec.data(), spirvVec.size());
      }

      delete program;
    }

    delete shader;
  }

  delete[] strs;
  delete[] names;

  return errors;
}
