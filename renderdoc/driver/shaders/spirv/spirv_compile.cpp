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

#include "spirv_compile.h"
#include "common/common.h"
#include "glslang_compile.h"

#undef min
#undef max

#include "3rdparty/glslang/SPIRV/GlslangToSpv.h"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

std::string CompileSPIRV(const SPIRVCompilationSettings &settings,
                         const std::vector<std::string> &sources, std::vector<uint32_t> &spirv)
{
  if(settings.stage == SPIRVShaderStage::Invalid)
    return "Invalid shader stage specified";

  std::string errors = "";

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

    bool success = shader->parse(GetDefaultResources(), 110, false, flags);

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
