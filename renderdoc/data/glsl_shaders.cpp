/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "os/os_specific.h"

void GenerateGLSLShader(std::vector<std::string> &sources, ShaderType type,
                        const std::string &defines, const std::string &shader, int version,
                        bool uniforms)
{
  sources.resize(4);
  sources[0] =
      StringFormat::Fmt("#version %d %s\n", version, type == eShaderGLSLES ? "es" : "core");

  if(uniforms)
    sources[1] = GetEmbeddedResource(glsl_debuguniforms_h);
  else
    sources[1] = "";

  if(shader.find("#include \"texsample.h\"") != string::npos)
  {
    if(type == eShaderVulkan)
      sources[2] = GetEmbeddedResource(glsl_vk_texsample_h);
    else if(type == eShaderGLSL)
      sources[2] = GetEmbeddedResource(glsl_gl_texsample_h);
    else if(type == eShaderGLSLES)
      sources[2] = GetEmbeddedResource(glsl_gles_texsample_h);
    else
      RDCERR("Unknown type! %d", type);
  }
  else
  {
    sources[2] = "";
  }

  sources[3] = shader;

  for(int i = 0; i < 4; i++)
  {
    // hoist up any #extension directives
    size_t extsearch = 0;
    do
    {
      extsearch = sources[i].find("#extension", extsearch);

      if(extsearch == string::npos)
        break;

      size_t begin = extsearch;
      extsearch = sources[i].find('\n', extsearch);

      string ext = sources[i].substr(begin, extsearch - begin + 1);

      if(ext.find("#extension_gles") == 0)
      {
        if(type != eShaderGLSLES)
          continue;

        ext.erase(ext.find("_gles"), 5);
      }
      else if(ext.find("#extension_nongles") == 0)
      {
        if(type == eShaderGLSLES)
          continue;

        ext.erase(ext.find("_nongles"), 8);
      }

      sources[0] += ext;
    } while(extsearch != string::npos);
  }

  sources[0] += "\n" + defines + "\n";
}
