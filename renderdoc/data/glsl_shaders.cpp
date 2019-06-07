/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"
#include "common/common.h"
#include "driver/shaders/spirv/glslang_compile.h"

#define GLSL_HEADERS(HEADER) \
  HEADER(glsl_globals)       \
  HEADER(glsl_ubos)          \
  HEADER(vk_texsample)       \
  HEADER(gl_texsample)       \
  HEADER(gles_texsample)

class EmbeddedIncluder : public glslang::TShader::Includer
{
#define DECL(header) std::string header = GetEmbeddedResource(CONCAT(glsl_, CONCAT(header, _h)));
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

std::string GenerateGLSLShader(const std::string &shader, ShaderType type, int version,
                               const std::string &defines)
{
  // shader stage doesn't matter for us since we're just pre-processing.
  glslang::TShader sh(EShLangFragment);

  std::string combined;

  if(type == eShaderGLSLES)
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
  std::string include_ext = "#extension GL_GOOGLE_include_directive : require\n";

  combined += include_ext;

  if(type == eShaderGLSLES)
    combined +=
        "#define OPENGL 1\n"
        "#define OPENGL_ES 1\n";
  else if(type == eShaderGLSL)
    combined +=
        "#define OPENGL 1\n"
        "#define OPENGL_CORE 1\n";

  combined += defines;

  combined += shader;

  const char *c_src = combined.c_str();
  glslang::EShClient client =
      type == eShaderVulkan ? glslang::EShClientVulkan : glslang::EShClientOpenGL;
  glslang::EShTargetClientVersion targetversion =
      type == eShaderVulkan ? glslang::EShTargetVulkan_1_0 : glslang::EShTargetOpenGL_450;

  sh.setStrings(&c_src, 1);
  sh.setEnvInput(glslang::EShSourceGlsl, EShLangFragment, client, 100);
  sh.setEnvClient(client, targetversion);
  sh.setEnvTarget(glslang::EShTargetNone, glslang::EShTargetSpv_1_0);

  EmbeddedIncluder incl;

  EShMessages flags = EShMsgOnlyPreprocessor;

  if(type == eShaderVulkan)
    flags = EShMessages(flags | EShMsgSpvRules | EShMsgVulkanRules);
  else if(type == eShaderGLSPIRV)
    flags = EShMessages(flags | EShMsgSpvRules);

  std::string ret;

  bool success =
      sh.preprocess(GetDefaultResources(), 100, ENoProfile, false, false, flags, &ret, incl);

  size_t offs = ret.find(include_ext);
  if(offs != std::string::npos)
    ret.erase(offs, include_ext.size());

  // strip any #line directives that got added
  offs = ret.find("\n#line ");
  while(offs != std::string::npos)
  {
    size_t eol = ret.find('\n', offs + 2);

    if(eol == std::string::npos)
      ret.erase(offs + 1);
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
