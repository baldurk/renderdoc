/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <float.h>
#include <algorithm>
#include "common/common.h"
#include "data/glsl_shaders.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/debuguniforms.h"

GLuint GLReplay::CreateCShaderProgram(const vector<string> &csSources)
{
  if(m_pDriver == NULL)
    return 0;

  MakeCurrentReplayContext(m_DebugCtx);

  const GLHookSet &gl = m_pDriver->GetHookset();

  GLuint cs = gl.glCreateShader(eGL_COMPUTE_SHADER);

  vector<const char *> srcs;
  srcs.reserve(csSources.size());
  for(size_t i = 0; i < csSources.size(); i++)
    srcs.push_back(csSources[i].c_str());

  gl.glShaderSource(cs, (GLsizei)srcs.size(), &srcs[0], NULL);

  gl.glCompileShader(cs);

  char buffer[1024];
  GLint status = 0;

  gl.glGetShaderiv(cs, eGL_COMPILE_STATUS, &status);
  if(status == 0)
  {
    gl.glGetShaderInfoLog(cs, 1024, NULL, buffer);
    RDCERR("Shader error: %s", buffer);
  }

  GLuint ret = gl.glCreateProgram();

  gl.glAttachShader(ret, cs);

  gl.glLinkProgram(ret);

  gl.glGetProgramiv(ret, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    gl.glGetProgramInfoLog(ret, 1024, NULL, buffer);
    RDCERR("Link error: %s", buffer);
  }

  gl.glDetachShader(ret, cs);

  gl.glDeleteShader(cs);

  return ret;
}

GLuint GLReplay::CreateShaderProgram(const vector<string> &vs, const vector<string> &fs)
{
  vector<string> empty;
  return CreateShaderProgram(vs, fs, empty);
}

GLuint GLReplay::CreateShaderProgram(const vector<string> &vsSources,
                                     const vector<string> &fsSources, const vector<string> &gsSources)
{
  if(m_pDriver == NULL)
    return 0;

  MakeCurrentReplayContext(m_DebugCtx);

  const GLHookSet &gl = m_pDriver->GetHookset();

  GLuint vs = 0;
  GLuint fs = 0;
  GLuint gs = 0;

  char buffer[1024];
  GLint status = 0;

  if(!vsSources.empty())
  {
    vs = gl.glCreateShader(eGL_VERTEX_SHADER);

    vector<const char *> srcs;
    srcs.reserve(vsSources.size());
    for(size_t i = 0; i < vsSources.size(); i++)
      srcs.push_back(vsSources[i].c_str());

    gl.glShaderSource(vs, (GLsizei)srcs.size(), &srcs[0], NULL);

    gl.glCompileShader(vs);

    gl.glGetShaderiv(vs, eGL_COMPILE_STATUS, &status);
    if(status == 0)
    {
      gl.glGetShaderInfoLog(vs, 1024, NULL, buffer);
      RDCERR("Shader error: %s", buffer);
    }
  }

  if(!fsSources.empty())
  {
    fs = gl.glCreateShader(eGL_FRAGMENT_SHADER);

    vector<const char *> srcs;
    srcs.reserve(fsSources.size());
    for(size_t i = 0; i < fsSources.size(); i++)
      srcs.push_back(fsSources[i].c_str());

    gl.glShaderSource(fs, (GLsizei)srcs.size(), &srcs[0], NULL);

    gl.glCompileShader(fs);

    gl.glGetShaderiv(fs, eGL_COMPILE_STATUS, &status);
    if(status == 0)
    {
      gl.glGetShaderInfoLog(fs, 1024, NULL, buffer);
      RDCERR("Shader error: %s", buffer);
    }
  }

  if(!gsSources.empty())
  {
    gs = gl.glCreateShader(eGL_GEOMETRY_SHADER);

    vector<const char *> srcs;
    srcs.reserve(gsSources.size());
    for(size_t i = 0; i < gsSources.size(); i++)
      srcs.push_back(gsSources[i].c_str());

    gl.glShaderSource(gs, (GLsizei)srcs.size(), &srcs[0], NULL);

    gl.glCompileShader(gs);

    gl.glGetShaderiv(gs, eGL_COMPILE_STATUS, &status);
    if(status == 0)
    {
      gl.glGetShaderInfoLog(gs, 1024, NULL, buffer);
      RDCERR("Shader error: %s", buffer);
    }
  }

  GLuint ret = gl.glCreateProgram();

  if(vs)
    gl.glAttachShader(ret, vs);
  if(fs)
    gl.glAttachShader(ret, fs);
  if(gs)
    gl.glAttachShader(ret, gs);

  gl.glProgramParameteri(ret, eGL_PROGRAM_SEPARABLE, GL_TRUE);

  gl.glLinkProgram(ret);

  gl.glGetProgramiv(ret, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    gl.glGetProgramInfoLog(ret, 1024, NULL, buffer);
    RDCERR("Shader error: %s", buffer);
  }

  if(vs)
    gl.glDetachShader(ret, vs);
  if(fs)
    gl.glDetachShader(ret, fs);
  if(gs)
    gl.glDetachShader(ret, gs);

  if(vs)
    gl.glDeleteShader(vs);
  if(fs)
    gl.glDeleteShader(fs);
  if(gs)
    gl.glDeleteShader(gs);

  return ret;
}

void GLReplay::CheckGLSLVersion(const char *sl, int &glslVersion)
{
  // GL_SHADING_LANGUAGE_VERSION for OpenGL ES:
  //   "OpenGL ES GLSL ES N.M vendor-specific information"
  static const char *const GLSL_ES_STR = "OpenGL ES GLSL ES";
  if(strncmp(sl, GLSL_ES_STR, 17) == 0)
    sl += 18;

  if(sl[0] >= '0' && sl[0] <= '9' && sl[1] == '.' && sl[2] >= '0' && sl[2] <= '9')
  {
    int major = int(sl[0] - '0');
    int minor = int(sl[2] - '0');
    int ver = major * 100 + minor * 10;

    if(ver > glslVersion)
      glslVersion = ver;
  }

  if(sl[0] >= '0' && sl[0] <= '9' && sl[1] >= '0' && sl[1] <= '9' && sl[2] == '0')
  {
    int major = int(sl[0] - '0');
    int minor = int(sl[1] - '0');
    int ver = major * 100 + minor * 10;

    if(ver > glslVersion)
      glslVersion = ver;
  }
}

void GLReplay::InitDebugData()
{
  if(m_pDriver == NULL)
    return;

  m_HighlightCache.driver = m_pDriver->GetReplay();

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.0f);

  {
    WindowingData window = {WindowingSystem::Unknown};
    uint64_t id = MakeOutputWindow(window, true);

    m_DebugID = id;
    m_DebugCtx = &m_OutputWindows[id];

    MakeCurrentReplayContext(m_DebugCtx);

    m_pDriver->RegisterDebugCallback();
  }

  WrappedOpenGL &gl = *m_pDriver;

  DebugData.outWidth = 0.0f;
  DebugData.outHeight = 0.0f;

  vector<string> empty;

  vector<string> vs;
  vector<string> fs;
  vector<string> gs;
  vector<string> cs;

  int glslVersion;
  int glslBaseVer;
  int glslCSVer;    // compute shader
  ShaderType shaderType;

  if(IsGLES)
  {
    glslVersion = glslBaseVer = glslCSVer = 310;
    shaderType = eShaderGLSLES;
  }
  else
  {
    glslVersion = glslBaseVer = 150;
    glslCSVer = 420;
    shaderType = eShaderGLSL;
  }

  // TODO In case of GLES some currently unused shaders, which are guarded by HasExt[..] checks,
  // still contain compile errors (e.g. array2ms.comp, ms2array.comp, quad*, etc.).
  bool glesShadersAreComplete = !IsGLES;

  GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_blit_vert), glslBaseVer);

  DebugData.texDisplayVSProg = CreateShaderProgram(vs, empty);

  for(int i = 0; i < 3; i++)
  {
    string defines = string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
    defines += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";

    GenerateGLSLShader(fs, shaderType, defines, GetEmbeddedResource(glsl_texdisplay_frag),
                       glslBaseVer);

    DebugData.texDisplayProg[i] = CreateShaderProgram(empty, fs);
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.2f);

  if(GLCoreVersion >= 43 && !IsGLES)
  {
    GLint numsl = 0;
    gl.glGetIntegerv(eGL_NUM_SHADING_LANGUAGE_VERSIONS, &numsl);

    for(GLint i = 0; i < numsl; i++)
    {
      const char *sl = (const char *)gl.glGetStringi(eGL_SHADING_LANGUAGE_VERSION, (GLuint)i);

      CheckGLSLVersion(sl, glslVersion);
    }
  }
  else
  {
    const char *sl = (const char *)gl.glGetString(eGL_SHADING_LANGUAGE_VERSION);

    CheckGLSLVersion(sl, glslVersion);
  }

  DebugData.glslVersion = glslVersion;

  RDCLOG("GLSL version %d", glslVersion);

  GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_blit_vert), glslBaseVer);

  if(glesShadersAreComplete && HasExt[ARB_shader_image_load_store] && HasExt[ARB_gpu_shader5])
  {
    string defines = "";

    if(glslVersion < 450)
    {
      // dFdx fine functions not available before GLSL 450. Use normal dFdx, which might be coarse,
      // so won't show quad overdraw properly
      defines += "#define dFdxFine dFdx\n\n";
      defines += "#define dFdyFine dFdy\n\n";

      RDCWARN("Quad overdraw requires GLSL 4.50 for dFd(xy)fine, using possibly coarse dFd(xy).");
      m_pDriver->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          "Quad overdraw requires GLSL 4.50 for dFd(xy)fine, using possibly coarse dFd(xy).");
    }

    GenerateGLSLShader(fs, shaderType, defines, GetEmbeddedResource(glsl_quadwrite_frag),
                       RDCMIN(450, glslVersion));

    DebugData.quadoverdrawFSProg = CreateShaderProgram(empty, fs);

    GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_quadresolve_frag), glslBaseVer);

    DebugData.quadoverdrawResolveProg = CreateShaderProgram(vs, fs);
  }
  else
  {
    RDCWARN(
        "GL_ARB_shader_image_load_store/GL_ARB_gpu_shader5 not supported, disabling quad overdraw "
        "feature.");
    m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                               MessageSource::RuntimeWarning,
                               "GL_ARB_shader_image_load_store/GL_ARB_gpu_shader5 not supported, "
                               "disabling quad overdraw feature.");
    DebugData.quadoverdrawFSProg = 0;
    DebugData.quadoverdrawResolveProg = 0;
  }

  GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_checkerboard_frag), glslBaseVer);
  DebugData.checkerProg = CreateShaderProgram(vs, fs);

  GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_fixedcol_frag), glslBaseVer);

  DebugData.fixedcolFSProg = CreateShaderProgram(empty, fs);

  if(HasExt[ARB_geometry_shader4])
  {
    GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_mesh_vert), glslBaseVer);
    GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_mesh_frag), glslBaseVer);
    GenerateGLSLShader(gs, shaderType, "", GetEmbeddedResource(glsl_mesh_geom), glslBaseVer);

    DebugData.meshProg = CreateShaderProgram(vs, fs);
    DebugData.meshgsProg = CreateShaderProgram(vs, fs, gs);

    GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_trisize_frag), glslBaseVer);
    GenerateGLSLShader(gs, shaderType, "", GetEmbeddedResource(glsl_trisize_geom), glslBaseVer);

    DebugData.trisizeProg = CreateShaderProgram(vs, fs, gs);
  }
  else
  {
    GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_mesh_vert), glslBaseVer);
    GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_mesh_frag), glslBaseVer);

    DebugData.meshProg = CreateShaderProgram(vs, fs);
    DebugData.meshgsProg = 0;
    DebugData.trisizeProg = 0;

    const char *warning_msg =
        "GL_ARB_geometry_shader4/GL_EXT_geometry_shader not supported, disabling triangle size and "
        "lit solid shading feature.";
    RDCWARN(warning_msg);
    m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                               MessageSource::RuntimeWarning, warning_msg);
  }

  gl.glGenProgramPipelines(1, &DebugData.texDisplayPipe);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.4f);

  gl.glGenSamplers(1, &DebugData.linearSampler);
  gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR);
  gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
  gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

  gl.glGenSamplers(1, &DebugData.pointSampler);
  gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST_MIPMAP_NEAREST);
  gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

  gl.glGenSamplers(1, &DebugData.pointNoMipSampler);
  gl.glSamplerParameteri(DebugData.pointNoMipSampler, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  gl.glSamplerParameteri(DebugData.pointNoMipSampler, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  gl.glSamplerParameteri(DebugData.pointNoMipSampler, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  gl.glSamplerParameteri(DebugData.pointNoMipSampler, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

  gl.glGenBuffers(ARRAY_COUNT(DebugData.UBOs), DebugData.UBOs);
  for(size_t i = 0; i < ARRAY_COUNT(DebugData.UBOs); i++)
  {
    gl.glBindBuffer(eGL_UNIFORM_BUFFER, DebugData.UBOs[i]);
    gl.glNamedBufferDataEXT(DebugData.UBOs[i], 2048, NULL, eGL_DYNAMIC_DRAW);
    RDCCOMPILE_ASSERT(sizeof(TexDisplayUBOData) <= 2048, "UBO too small");
    RDCCOMPILE_ASSERT(sizeof(FontUBOData) <= 2048, "UBO too small");
    RDCCOMPILE_ASSERT(sizeof(HistogramUBOData) <= 2048, "UBO too small");
    RDCCOMPILE_ASSERT(sizeof(overdrawRamp) <= 2048, "UBO too small");
  }

  DebugData.overlayTexWidth = DebugData.overlayTexHeight = DebugData.overlayTexSamples = 0;
  DebugData.overlayTex = DebugData.overlayFBO = 0;

  gl.glGenFramebuffers(1, &DebugData.customFBO);
  gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.customFBO);
  DebugData.customTex = 0;

  gl.glGenFramebuffers(1, &DebugData.pickPixelFBO);
  gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.pickPixelFBO);

  gl.glGenTextures(1, &DebugData.pickPixelTex);
  gl.glBindTexture(eGL_TEXTURE_2D, DebugData.pickPixelTex);

  gl.glTextureImage2DEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, 0, eGL_RGBA32F, 1, 1, 0, eGL_RGBA,
                         eGL_FLOAT, NULL);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
  gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
  gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.pickPixelTex, 0);

  gl.glGenVertexArrays(1, &DebugData.emptyVAO);
  gl.glBindVertexArray(DebugData.emptyVAO);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.6f);

  // histogram/minmax data
  {
    RDCEraseEl(DebugData.minmaxTileProgram);
    RDCEraseEl(DebugData.histogramProgram);
    RDCEraseEl(DebugData.minmaxResultProgram);

    RDCCOMPILE_ASSERT(
        ARRAY_COUNT(DebugData.minmaxTileProgram) >= (TEXDISPLAY_SINT_TEX | TEXDISPLAY_TYPEMASK) + 1,
        "not enough programs");

    string extensions =
        "#extension GL_ARB_compute_shader : require\n"
        "#extension GL_ARB_shader_storage_buffer_object : require\n";

    for(int t = 1; glesShadersAreComplete && HasExt[ARB_compute_shader] && t <= RESTYPE_TEXTYPEMAX;
        t++)
    {
      // float, uint, sint
      for(int i = 0; i < 3; i++)
      {
        int idx = t;
        if(i == 1)
          idx |= TEXDISPLAY_UINT_TEX;
        if(i == 2)
          idx |= TEXDISPLAY_SINT_TEX;

        {
          string defines = extensions;
          defines += string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
          defines += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
          defines += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";

          GenerateGLSLShader(cs, shaderType, defines, GetEmbeddedResource(glsl_minmaxtile_comp),
                             glslCSVer);

          DebugData.minmaxTileProgram[idx] = CreateCShaderProgram(cs);
        }

        {
          string defines = extensions;
          defines += string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
          defines += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
          defines += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";

          GenerateGLSLShader(cs, shaderType, defines, GetEmbeddedResource(glsl_histogram_comp),
                             glslCSVer);

          DebugData.histogramProgram[idx] = CreateCShaderProgram(cs);
        }

        if(t == 1)
        {
          string defines = extensions;
          defines += string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
          defines += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
          defines += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";

          GenerateGLSLShader(cs, shaderType, defines, GetEmbeddedResource(glsl_minmaxresult_comp),
                             glslCSVer);

          DebugData.minmaxResultProgram[i] = CreateCShaderProgram(cs);
        }
      }
    }

    if(!HasExt[ARB_compute_shader])
    {
      RDCWARN("GL_ARB_compute_shader not supported, disabling min/max and histogram features.");
      m_pDriver->AddDebugMessage(
          MessageCategory::Portability, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          "GL_ARB_compute_shader not supported, disabling min/max and histogram features.");
    }

    gl.glGenBuffers(1, &DebugData.minmaxTileResult);
    gl.glGenBuffers(1, &DebugData.minmaxResult);
    gl.glGenBuffers(1, &DebugData.histogramBuf);

    const uint32_t maxTexDim = 16384;
    const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
    const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

    const size_t byteSize =
        2 * sizeof(Vec4f) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;

    gl.glNamedBufferDataEXT(DebugData.minmaxTileResult, byteSize, NULL, eGL_DYNAMIC_DRAW);
    gl.glNamedBufferDataEXT(DebugData.minmaxResult, sizeof(Vec4f) * 2, NULL, eGL_DYNAMIC_READ);
    gl.glNamedBufferDataEXT(DebugData.histogramBuf, sizeof(uint32_t) * 4 * HGRAM_NUM_BUCKETS, NULL,
                            eGL_DYNAMIC_READ);
  }

  if(glesShadersAreComplete && HasExt[ARB_compute_shader])
  {
    GenerateGLSLShader(cs, shaderType, "", GetEmbeddedResource(glsl_ms2array_comp), glslCSVer);
    DebugData.MS2Array = CreateCShaderProgram(cs);

    GenerateGLSLShader(cs, shaderType, "", GetEmbeddedResource(glsl_array2ms_comp), glslCSVer);
    DebugData.Array2MS = CreateCShaderProgram(cs);
  }
  else
  {
    DebugData.MS2Array = 0;
    DebugData.Array2MS = 0;
    RDCWARN("GL_ARB_compute_shader not supported, disabling 2DMS save/load.");
    m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                               MessageSource::RuntimeWarning,
                               "GL_ARB_compute_shader not supported, disabling 2DMS save/load.");
  }

  if(glesShadersAreComplete && HasExt[ARB_compute_shader])
  {
    string defines =
        "#extension GL_ARB_compute_shader : require\n"
        "#extension GL_ARB_shader_storage_buffer_object : require";
    GenerateGLSLShader(cs, shaderType, defines, GetEmbeddedResource(glsl_mesh_comp), glslCSVer);
    DebugData.meshPickProgram = CreateCShaderProgram(cs);
  }
  else
  {
    DebugData.meshPickProgram = 0;
    RDCWARN("GL_ARB_compute_shader not supported, disabling mesh picking.");
    m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                               MessageSource::RuntimeWarning,
                               "GL_ARB_compute_shader not supported, disabling mesh picking.");
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.8f);

  DebugData.pickResultBuf = 0;

  if(DebugData.meshPickProgram)
  {
    gl.glGenBuffers(1, &DebugData.pickResultBuf);
    gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickResultBuf);
    gl.glNamedBufferDataEXT(DebugData.pickResultBuf,
                            sizeof(Vec4f) * DebugRenderData::maxMeshPicks + sizeof(uint32_t) * 4,
                            NULL, eGL_DYNAMIC_READ);

    // sized/created on demand
    DebugData.pickVBBuf = DebugData.pickIBBuf = 0;
    DebugData.pickVBSize = DebugData.pickIBSize = 0;
  }

  gl.glGenVertexArrays(1, &DebugData.meshVAO);
  gl.glBindVertexArray(DebugData.meshVAO);

  gl.glGenBuffers(1, &DebugData.axisFrustumBuffer);
  gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.axisFrustumBuffer);

  Vec3f TLN = Vec3f(-1.0f, 1.0f, 0.0f);    // TopLeftNear, etc...
  Vec3f TRN = Vec3f(1.0f, 1.0f, 0.0f);
  Vec3f BLN = Vec3f(-1.0f, -1.0f, 0.0f);
  Vec3f BRN = Vec3f(1.0f, -1.0f, 0.0f);

  Vec3f TLF = Vec3f(-1.0f, 1.0f, 1.0f);
  Vec3f TRF = Vec3f(1.0f, 1.0f, 1.0f);
  Vec3f BLF = Vec3f(-1.0f, -1.0f, 1.0f);
  Vec3f BRF = Vec3f(1.0f, -1.0f, 1.0f);

  Vec3f axisFrustum[] = {
      // axis marker vertices
      Vec3f(0.0f, 0.0f, 0.0f), Vec3f(1.0f, 0.0f, 0.0f), Vec3f(0.0f, 0.0f, 0.0f),
      Vec3f(0.0f, 1.0f, 0.0f), Vec3f(0.0f, 0.0f, 0.0f), Vec3f(0.0f, 0.0f, 1.0f),

      // frustum vertices
      TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

      TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

      TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
  };

  gl.glNamedBufferDataEXT(DebugData.axisFrustumBuffer, sizeof(axisFrustum), axisFrustum,
                          eGL_STATIC_DRAW);

  gl.glGenVertexArrays(1, &DebugData.axisVAO);
  gl.glBindVertexArray(DebugData.axisVAO);
  gl.glVertexAttribPointer(0, 3, eGL_FLOAT, GL_FALSE, sizeof(Vec3f), NULL);
  gl.glEnableVertexAttribArray(0);

  gl.glGenVertexArrays(1, &DebugData.frustumVAO);
  gl.glBindVertexArray(DebugData.frustumVAO);
  gl.glVertexAttribPointer(0, 3, eGL_FLOAT, GL_FALSE, sizeof(Vec3f),
                           (const void *)(sizeof(Vec3f) * 6));
  gl.glEnableVertexAttribArray(0);

  gl.glGenVertexArrays(1, &DebugData.triHighlightVAO);
  gl.glBindVertexArray(DebugData.triHighlightVAO);

  gl.glGenBuffers(1, &DebugData.triHighlightBuffer);
  gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);

  gl.glNamedBufferDataEXT(DebugData.triHighlightBuffer, sizeof(Vec4f) * 24, NULL, eGL_DYNAMIC_DRAW);

  gl.glVertexAttribPointer(0, 4, eGL_FLOAT, GL_FALSE, sizeof(Vec4f), NULL);
  gl.glEnableVertexAttribArray(0);

  GenerateGLSLShader(vs, shaderType, "", GetEmbeddedResource(glsl_blit_vert), glslBaseVer);
  GenerateGLSLShader(fs, shaderType, "", GetEmbeddedResource(glsl_outline_frag), glslBaseVer);

  DebugData.outlineQuadProg = CreateShaderProgram(vs, fs);

  MakeCurrentReplayContext(&m_ReplayCtx);

  // these below need to be made on the replay context, as they are context-specific (not shared)
  // and will be used on the replay context.
  gl.glGenProgramPipelines(1, &DebugData.overlayPipe);

  gl.glGenTransformFeedbacks(1, &DebugData.feedbackObj);
  gl.glGenBuffers(1, &DebugData.feedbackBuffer);
  DebugData.feedbackQueries.push_back(0);
  gl.glGenQueries(1, &DebugData.feedbackQueries[0]);

  gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);
  gl.glBindBuffer(eGL_TRANSFORM_FEEDBACK_BUFFER, DebugData.feedbackBuffer);
  gl.glNamedBufferDataEXT(DebugData.feedbackBuffer, DebugData.feedbackBufferSize, NULL,
                          eGL_DYNAMIC_READ);
  gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);
  gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 1.0f);

  if(!HasExt[ARB_gpu_shader5])
  {
    RDCWARN(
        "ARB_gpu_shader5 not supported, pixel picking and saving of integer textures may be "
        "inaccurate.");
    m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                               MessageSource::RuntimeWarning,
                               "ARB_gpu_shader5 not supported, pixel picking and saving of integer "
                               "textures may be inaccurate.");

    m_Degraded = true;
  }

  if(!HasExt[ARB_stencil_texturing])
  {
    RDCWARN("ARB_stencil_texturing not supported, stencil values will not be displayed or picked.");
    m_pDriver->AddDebugMessage(
        MessageCategory::Portability, MessageSeverity::Medium, MessageSource::RuntimeWarning,
        "ARB_stencil_texturing not supported, stencil values will not be displayed or picked.");

    m_Degraded = true;
  }

  if(!HasExt[ARB_shader_image_load_store] || !HasExt[ARB_compute_shader])
  {
    m_Degraded = true;
  }
}

void GLReplay::DeleteDebugData()
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  gl.glDeleteProgramPipelines(1, &DebugData.overlayPipe);

  gl.glDeleteTransformFeedbacks(1, &DebugData.feedbackObj);
  gl.glDeleteBuffers(1, &DebugData.feedbackBuffer);
  gl.glDeleteQueries((GLsizei)DebugData.feedbackQueries.size(), DebugData.feedbackQueries.data());

  MakeCurrentReplayContext(m_DebugCtx);

  ClearPostVSCache();

  gl.glDeleteFramebuffers(1, &DebugData.overlayFBO);
  gl.glDeleteTextures(1, &DebugData.overlayTex);

  gl.glDeleteProgram(DebugData.quadoverdrawFSProg);
  gl.glDeleteProgram(DebugData.quadoverdrawResolveProg);

  gl.glDeleteProgram(DebugData.texDisplayVSProg);
  for(int i = 0; i < 3; i++)
    gl.glDeleteProgram(DebugData.texDisplayProg[i]);

  gl.glDeleteProgramPipelines(1, &DebugData.texDisplayPipe);

  gl.glDeleteProgram(DebugData.checkerProg);
  gl.glDeleteProgram(DebugData.fixedcolFSProg);
  gl.glDeleteProgram(DebugData.meshProg);
  gl.glDeleteProgram(DebugData.meshgsProg);
  gl.glDeleteProgram(DebugData.trisizeProg);

  gl.glDeleteSamplers(1, &DebugData.linearSampler);
  gl.glDeleteSamplers(1, &DebugData.pointSampler);
  gl.glDeleteSamplers(1, &DebugData.pointNoMipSampler);
  gl.glDeleteBuffers(ARRAY_COUNT(DebugData.UBOs), DebugData.UBOs);
  gl.glDeleteFramebuffers(1, &DebugData.pickPixelFBO);
  gl.glDeleteTextures(1, &DebugData.pickPixelTex);

  gl.glDeleteBuffers(1, &DebugData.genericUBO);

  gl.glDeleteFramebuffers(1, &DebugData.customFBO);
  gl.glDeleteTextures(1, &DebugData.customTex);

  gl.glDeleteVertexArrays(1, &DebugData.emptyVAO);

  for(int t = 1; t <= RESTYPE_TEXTYPEMAX; t++)
  {
    // float, uint, sint
    for(int i = 0; i < 3; i++)
    {
      int idx = t;
      if(i == 1)
        idx |= TEXDISPLAY_UINT_TEX;
      if(i == 2)
        idx |= TEXDISPLAY_SINT_TEX;

      gl.glDeleteProgram(DebugData.minmaxTileProgram[idx]);
      gl.glDeleteProgram(DebugData.histogramProgram[idx]);

      gl.glDeleteProgram(DebugData.minmaxResultProgram[i]);
      DebugData.minmaxResultProgram[i] = 0;
    }
  }

  gl.glDeleteProgram(DebugData.meshPickProgram);
  gl.glDeleteBuffers(1, &DebugData.pickIBBuf);
  gl.glDeleteBuffers(1, &DebugData.pickVBBuf);
  gl.glDeleteBuffers(1, &DebugData.pickResultBuf);

  gl.glDeleteProgram(DebugData.Array2MS);
  gl.glDeleteProgram(DebugData.MS2Array);

  gl.glDeleteBuffers(1, &DebugData.minmaxTileResult);
  gl.glDeleteBuffers(1, &DebugData.minmaxResult);
  gl.glDeleteBuffers(1, &DebugData.histogramBuf);

  gl.glDeleteVertexArrays(1, &DebugData.meshVAO);
  gl.glDeleteVertexArrays(1, &DebugData.axisVAO);
  gl.glDeleteVertexArrays(1, &DebugData.frustumVAO);
  gl.glDeleteVertexArrays(1, &DebugData.triHighlightVAO);

  gl.glDeleteBuffers(1, &DebugData.axisFrustumBuffer);
  gl.glDeleteBuffers(1, &DebugData.triHighlightBuffer);

  gl.glDeleteProgram(DebugData.outlineQuadProg);
}

bool GLReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                         CompType typeHint, float *minval, float *maxval)
{
  if(texid == ResourceId() || m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
    return false;

  if(!HasExt[ARB_compute_shader])
    return false;

  auto &texDetails = m_pDriver->m_Textures[texid];

  TextureDescription details = GetTexture(texid);

  const GLHookSet &gl = m_pDriver->GetHookset();

  int texSlot = 0;
  int intIdx = 0;

  bool renderbuffer = false;

  switch(texDetails.curType)
  {
    case eGL_RENDERBUFFER:
      texSlot = RESTYPE_TEX2D;
      renderbuffer = true;
      break;
    case eGL_TEXTURE_1D: texSlot = RESTYPE_TEX1D; break;
    default:
      RDCWARN("Unexpected texture type");
    // fall through
    case eGL_TEXTURE_2D: texSlot = RESTYPE_TEX2D; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: texSlot = RESTYPE_TEX2DMS; break;
    case eGL_TEXTURE_RECTANGLE: texSlot = RESTYPE_TEXRECT; break;
    case eGL_TEXTURE_BUFFER: texSlot = RESTYPE_TEXBUFFER; break;
    case eGL_TEXTURE_3D: texSlot = RESTYPE_TEX3D; break;
    case eGL_TEXTURE_CUBE_MAP: texSlot = RESTYPE_TEXCUBE; break;
    case eGL_TEXTURE_1D_ARRAY: texSlot = RESTYPE_TEX1DARRAY; break;
    case eGL_TEXTURE_2D_ARRAY: texSlot = RESTYPE_TEX2DARRAY; break;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: texSlot = RESTYPE_TEXCUBEARRAY; break;
  }

  GLenum target = texDetails.curType;
  GLuint texname = texDetails.resource.name;

  // do blit from renderbuffer to texture, then sample from texture
  if(renderbuffer)
  {
    // need replay context active to do blit (as FBOs aren't shared)
    MakeCurrentReplayContext(&m_ReplayCtx);

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

    gl.glBlitFramebuffer(
        0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width, texDetails.height,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

    texname = texDetails.renderbufferReadTex;
    target = eGL_TEXTURE_2D;
  }

  MakeCurrentReplayContext(m_DebugCtx);

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[0]);
  HistogramUBOData *cdata =
      (HistogramUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width >> mip, 1U);
  cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height >> mip, 1U);
  cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth >> mip, 1U);
  if(texDetails.curType != eGL_TEXTURE_3D)
    cdata->HistogramSlice = (float)sliceFace + 0.001f;
  else
    cdata->HistogramSlice = (float)(sliceFace >> mip);
  cdata->HistogramMip = (int)mip;
  cdata->HistogramNumSamples = texDetails.samples;
  cdata->HistogramSample = (int)RDCCLAMP(sample, 0U, details.msSamp - 1);
  if(sample == ~0U)
    cdata->HistogramSample = -int(details.msSamp);
  cdata->HistogramMin = 0.0f;
  cdata->HistogramMax = 1.0f;
  cdata->HistogramChannels = 0xf;

  int progIdx = texSlot;

  if(details.format.compType == CompType::UInt)
  {
    progIdx |= TEXDISPLAY_UINT_TEX;
    intIdx = 1;
  }
  if(details.format.compType == CompType::SInt)
  {
    progIdx |= TEXDISPLAY_SINT_TEX;
    intIdx = 2;
  }

  int blocksX = (int)ceil(cdata->HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata->HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  gl.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + texSlot));
  gl.glBindTexture(target, texname);
  if(texSlot == RESTYPE_TEXRECT || texSlot == RESTYPE_TEXBUFFER)
    gl.glBindSampler(texSlot, DebugData.pointNoMipSampler);
  else
    gl.glBindSampler(texSlot, DebugData.pointSampler);

  int maxlevel = -1;

  int clampmaxlevel = details.mips - 1;

  gl.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

  // need to ensure texture is mipmap complete by clamping TEXTURE_MAX_LEVEL.
  if(clampmaxlevel != maxlevel)
  {
    gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&clampmaxlevel);
  }
  else
  {
    maxlevel = -1;
  }

  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.minmaxTileResult);

  gl.glUseProgram(DebugData.minmaxTileProgram[progIdx]);
  gl.glDispatchCompute(blocksX, blocksY, 1);

  gl.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.minmaxResult);
  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 1, DebugData.minmaxTileResult);

  gl.glUseProgram(DebugData.minmaxResultProgram[intIdx]);
  gl.glDispatchCompute(1, 1, 1);

  gl.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  Vec4f minmax[2];
  gl.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.minmaxResult);
  gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(minmax), minmax);

  if(maxlevel >= 0)
    gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

  minval[0] = minmax[0].x;
  minval[1] = minmax[0].y;
  minval[2] = minmax[0].z;
  minval[3] = minmax[0].w;

  maxval[0] = minmax[1].x;
  maxval[1] = minmax[1].y;
  maxval[2] = minmax[1].z;
  maxval[3] = minmax[1].w;

  return true;
}

bool GLReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float minval, float maxval, bool channels[4],
                            vector<uint32_t> &histogram)
{
  if(minval >= maxval || texid == ResourceId())
    return false;

  if(m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
    return false;

  if(!HasExt[ARB_compute_shader])
    return false;

  auto &texDetails = m_pDriver->m_Textures[texid];

  TextureDescription details = GetTexture(texid);

  const GLHookSet &gl = m_pDriver->GetHookset();

  int texSlot = 0;
  int intIdx = 0;

  bool renderbuffer = false;

  switch(texDetails.curType)
  {
    case eGL_RENDERBUFFER:
      texSlot = RESTYPE_TEX2D;
      renderbuffer = true;
      break;
    case eGL_TEXTURE_1D: texSlot = RESTYPE_TEX1D; break;
    default:
      RDCWARN("Unexpected texture type");
    // fall through
    case eGL_TEXTURE_2D: texSlot = RESTYPE_TEX2D; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: texSlot = RESTYPE_TEX2DMS; break;
    case eGL_TEXTURE_RECTANGLE: texSlot = RESTYPE_TEXRECT; break;
    case eGL_TEXTURE_BUFFER: texSlot = RESTYPE_TEXBUFFER; break;
    case eGL_TEXTURE_3D: texSlot = RESTYPE_TEX3D; break;
    case eGL_TEXTURE_CUBE_MAP: texSlot = RESTYPE_TEXCUBE; break;
    case eGL_TEXTURE_1D_ARRAY: texSlot = RESTYPE_TEX1DARRAY; break;
    case eGL_TEXTURE_2D_ARRAY: texSlot = RESTYPE_TEX2DARRAY; break;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: texSlot = RESTYPE_TEXCUBEARRAY; break;
  }

  GLenum target = texDetails.curType;
  GLuint texname = texDetails.resource.name;

  // do blit from renderbuffer to texture, then sample from texture
  if(renderbuffer)
  {
    // need replay context active to do blit (as FBOs aren't shared)
    MakeCurrentReplayContext(&m_ReplayCtx);

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

    gl.glBlitFramebuffer(
        0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width, texDetails.height,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

    texname = texDetails.renderbufferReadTex;
    target = eGL_TEXTURE_2D;
  }

  MakeCurrentReplayContext(m_DebugCtx);

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[0]);
  HistogramUBOData *cdata =
      (HistogramUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width >> mip, 1U);
  cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height >> mip, 1U);
  cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth >> mip, 1U);
  if(texDetails.curType != eGL_TEXTURE_3D)
    cdata->HistogramSlice = (float)sliceFace + 0.001f;
  else
    cdata->HistogramSlice = (float)(sliceFace >> mip);
  cdata->HistogramMip = mip;
  cdata->HistogramNumSamples = texDetails.samples;
  cdata->HistogramSample = (int)RDCCLAMP(sample, 0U, details.msSamp - 1);
  if(sample == ~0U)
    cdata->HistogramSample = -int(details.msSamp);
  cdata->HistogramMin = minval;

  // The calculation in the shader normalises each value between min and max, then multiplies by the
  // number of buckets.
  // But any value equal to HistogramMax must go into NUM_BUCKETS-1, so add a small delta.
  cdata->HistogramMax = maxval + maxval * 1e-6f;

  cdata->HistogramChannels = 0;
  if(channels[0])
    cdata->HistogramChannels |= 0x1;
  if(channels[1])
    cdata->HistogramChannels |= 0x2;
  if(channels[2])
    cdata->HistogramChannels |= 0x4;
  if(channels[3])
    cdata->HistogramChannels |= 0x8;
  cdata->HistogramFlags = 0;

  int progIdx = texSlot;

  if(details.format.compType == CompType::UInt)
  {
    progIdx |= TEXDISPLAY_UINT_TEX;
    intIdx = 1;
  }
  if(details.format.compType == CompType::SInt)
  {
    progIdx |= TEXDISPLAY_SINT_TEX;
    intIdx = 2;
  }

  int blocksX = (int)ceil(cdata->HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata->HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  gl.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + texSlot));
  gl.glBindTexture(target, texname);
  if(texSlot == RESTYPE_TEXRECT || texSlot == RESTYPE_TEXBUFFER)
    gl.glBindSampler(texSlot, DebugData.pointNoMipSampler);
  else
    gl.glBindSampler(texSlot, DebugData.pointSampler);

  int maxlevel = -1;

  int clampmaxlevel = details.mips - 1;

  gl.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

  // need to ensure texture is mipmap complete by clamping TEXTURE_MAX_LEVEL.
  if(clampmaxlevel != maxlevel)
  {
    gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&clampmaxlevel);
  }
  else
  {
    maxlevel = -1;
  }

  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.histogramBuf);

  GLuint zero = 0;
  gl.glClearBufferData(eGL_SHADER_STORAGE_BUFFER, eGL_R32UI, eGL_RED_INTEGER, eGL_UNSIGNED_INT,
                       &zero);

  gl.glUseProgram(DebugData.histogramProgram[progIdx]);
  gl.glDispatchCompute(blocksX, blocksY, 1);

  gl.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  histogram.clear();
  histogram.resize(HGRAM_NUM_BUCKETS * 4);

  gl.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.histogramBuf);
  gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(uint32_t) * 4 * HGRAM_NUM_BUCKETS,
                        &histogram[0]);

  // compress down from uvec4, then resize down
  for(size_t i = 1; i < HGRAM_NUM_BUCKETS; i++)
    histogram[i] = histogram[i * 4];

  histogram.resize(HGRAM_NUM_BUCKETS);

  if(maxlevel >= 0)
    gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

  return true;
}

uint32_t GLReplay::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  WrappedOpenGL &gl = *m_pDriver;

  if(!HasExt[ARB_compute_shader])
    return ~0U;

  MakeCurrentReplayContext(m_DebugCtx);

  gl.glUseProgram(DebugData.meshPickProgram);

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, DebugData.outWidth / DebugData.outHeight);

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f pickMVP = projMat.Mul(camMat);

  Matrix4f pickMVPProj;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    pickMVPProj = projMat.Mul(camMat.Mul(guessProj.Inverse()));
  }

  vec3 rayPos;
  vec3 rayDir;
  // convert mouse pos to world space ray
  {
    Matrix4f inversePickMVP = pickMVP.Inverse();

    float pickX = ((float)x) / ((float)DebugData.outWidth);
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)DebugData.outHeight);
    // flip the Y axis
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    vec3 cameraToWorldNearPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

    vec3 cameraToWorldFarPosition =
        inversePickMVP.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

    vec3 testDir = (cameraToWorldFarPosition - cameraToWorldNearPosition);
    testDir.Normalise();

    // Calculate the ray direction first in the regular way (above), so we can use the
    // the output for testing if the ray we are picking is negative or not. This is similar
    // to checking against the forward direction of the camera, but more robust
    if(cfg.position.unproject)
    {
      Matrix4f inversePickMVPGuess = pickMVPProj.Inverse();

      vec3 nearPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, -1), 1);

      vec3 farPosProj = inversePickMVPGuess.Transform(Vec3f(pickXCanonical, pickYCanonical, 1), 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else
    {
      rayDir = testDir;
      rayPos = cameraToWorldNearPosition;
    }
  }

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
  MeshPickUBOData *cdata =
      (MeshPickUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshPickUBOData),
                                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  cdata->rayPos = rayPos;
  cdata->rayDir = rayDir;
  cdata->use_indices = cfg.position.indexByteStride ? 1U : 0U;
  cdata->numVerts = cfg.position.numIndices;
  bool isTriangleMesh = true;
  switch(cfg.position.topology)
  {
    case Topology::TriangleList:
    {
      cdata->meshMode = MESH_TRIANGLE_LIST;
      break;
    }
    case Topology::TriangleStrip:
    {
      cdata->meshMode = MESH_TRIANGLE_STRIP;
      break;
    }
    case Topology::TriangleFan:
    {
      cdata->meshMode = MESH_TRIANGLE_FAN;
      break;
    }
    case Topology::TriangleList_Adj:
    {
      cdata->meshMode = MESH_TRIANGLE_LIST_ADJ;
      break;
    }
    case Topology::TriangleStrip_Adj:
    {
      cdata->meshMode = MESH_TRIANGLE_STRIP_ADJ;
      break;
    }
    default:    // points, lines, patchlists, unknown
    {
      cdata->meshMode = MESH_OTHER;
      isTriangleMesh = false;
    }
  }

  // line/point data
  cdata->unproject = cfg.position.unproject;
  cdata->mvp = cfg.position.unproject ? pickMVPProj : pickMVP;
  cdata->coords = Vec2f((float)x, (float)y);
  cdata->viewport = Vec2f(DebugData.outWidth, DebugData.outHeight);

  gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  GLuint ib = 0;

  if(cfg.position.indexByteStride && cfg.position.indexResourceId != ResourceId())
    ib = m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.indexResourceId).name;

  // We copy into our own buffers to promote to the target type (uint32) that the
  // shader expects. Most IBs will be 16-bit indices, most VBs will not be float4.

  if(ib)
  {
    // resize up on demand
    if(DebugData.pickIBBuf == 0 || DebugData.pickIBSize < cfg.position.numIndices * sizeof(uint32_t))
    {
      gl.glDeleteBuffers(1, &DebugData.pickIBBuf);

      gl.glGenBuffers(1, &DebugData.pickIBBuf);
      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glNamedBufferDataEXT(DebugData.pickIBBuf, cfg.position.numIndices * sizeof(uint32_t), NULL,
                              eGL_STREAM_DRAW);

      DebugData.pickIBSize = cfg.position.numIndices * sizeof(uint32_t);
    }

    byte *idxs = new byte[cfg.position.numIndices * cfg.position.indexByteStride];
    memset(idxs, 0, cfg.position.numIndices * cfg.position.indexByteStride);
    uint32_t *outidxs = NULL;

    if(cfg.position.indexByteStride < 4)
      outidxs = new uint32_t[cfg.position.numIndices];

    gl.glBindBuffer(eGL_COPY_READ_BUFFER, ib);

    GLint bufsize = 0;
    gl.glGetBufferParameteriv(eGL_COPY_READ_BUFFER, eGL_BUFFER_SIZE, &bufsize);

    gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)cfg.position.indexByteOffset,
                          RDCMIN(uint32_t(bufsize) - uint32_t(cfg.position.indexByteOffset),
                                 cfg.position.numIndices * cfg.position.indexByteStride),
                          idxs);

    uint16_t *idxs16 = (uint16_t *)idxs;

    if(cfg.position.indexByteStride == 1)
    {
      for(uint32_t i = 0; i < cfg.position.numIndices; i++)
        outidxs[i] = idxs[i];

      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numIndices * sizeof(uint32_t),
                         outidxs);
    }
    else if(cfg.position.indexByteStride == 2)
    {
      for(uint32_t i = 0; i < cfg.position.numIndices; i++)
        outidxs[i] = idxs16[i];

      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numIndices * sizeof(uint32_t),
                         outidxs);
    }
    else
    {
      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numIndices * sizeof(uint32_t),
                         idxs);
    }

    SAFE_DELETE_ARRAY(outidxs);
  }

  if(DebugData.pickVBBuf == 0 || DebugData.pickVBSize < cfg.position.numIndices * sizeof(Vec4f))
  {
    gl.glDeleteBuffers(1, &DebugData.pickVBBuf);

    gl.glGenBuffers(1, &DebugData.pickVBBuf);
    gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickVBBuf);
    gl.glNamedBufferDataEXT(DebugData.pickVBBuf, cfg.position.numIndices * sizeof(Vec4f), NULL,
                            eGL_DYNAMIC_DRAW);

    DebugData.pickVBSize = cfg.position.numIndices * sizeof(Vec4f);
  }

  // unpack and linearise the data
  {
    FloatVector *vbData = new FloatVector[cfg.position.numIndices];

    bytebuf oldData;
    GetBufferData(cfg.position.vertexResourceId, cfg.position.vertexByteOffset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numIndices; i++)
    {
      uint32_t idx = i;

      // apply baseVertex but clamp to 0 (don't allow index to become negative)
      if(idx < idxclamp)
        idx = 0;
      else if(cfg.position.baseVertex < 0)
        idx -= idxclamp;
      else if(cfg.position.baseVertex > 0)
        idx += cfg.position.baseVertex;

      vbData[i] = HighlightCache::InterpretVertex(data, idx, cfg, dataEnd, valid);
    }

    gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickVBBuf);
    gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numIndices * sizeof(Vec4f), vbData);

    delete[] vbData;
  }

  uint32_t reset[4] = {};
  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.pickResultBuf);
  gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t) * 4, &reset);

  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 1, DebugData.pickVBBuf);
  gl.glBindBufferRange(
      eGL_SHADER_STORAGE_BUFFER, 2, DebugData.pickIBBuf, (GLintptr)cfg.position.indexByteOffset,
      (GLsizeiptr)(cfg.position.indexByteOffset + sizeof(uint32_t) * cfg.position.numIndices));
  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 3, DebugData.pickResultBuf);

  gl.glDispatchCompute(GLuint((cfg.position.numIndices) / 128 + 1), 1, 1);
  gl.glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

  uint32_t numResults = 0;

  gl.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.pickResultBuf);
  gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(uint32_t), &numResults);

  if(numResults > 0)
  {
    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        vec3 intersectionPoint;
      };

      byte *mapped = (byte *)gl.glMapNamedBufferEXT(DebugData.pickResultBuf, eGL_READ_ONLY);

      mapped += sizeof(uint32_t) * 4;

      PickResult *pickResults = (PickResult *)mapped;

      PickResult *closest = pickResults;
      // distance from raycast hit to nearest worldspace position of the mouse
      float closestPickDistance = (closest->intersectionPoint - rayPos).Length();

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)DebugRenderData::maxMeshPicks, numResults); i++)
      {
        float pickDistance = (pickResults[i].intersectionPoint - rayPos).Length();
        if(pickDistance < closestPickDistance)
        {
          closest = pickResults + i;
        }
      }

      gl.glUnmapNamedBufferEXT(DebugData.pickResultBuf);

      return closest->vertid;
    }
    else
    {
      struct PickResult
      {
        uint32_t vertid;
        uint32_t idx;
        float len;
        float depth;
      };

      byte *mapped = (byte *)gl.glMapNamedBufferEXT(DebugData.pickResultBuf, eGL_READ_ONLY);

      mapped += sizeof(uint32_t) * 4;

      PickResult *pickResults = (PickResult *)mapped;

      PickResult *closest = pickResults;

      // min with size of results buffer to protect against overflows
      for(uint32_t i = 1; i < RDCMIN((uint32_t)DebugRenderData::maxMeshPicks, numResults); i++)
      {
        // We need to keep the picking order consistent in the face
        // of random buffer appends, when multiple vertices have the
        // identical position (e.g. if UVs or normals are different).
        //
        // We could do something to try and disambiguate, but it's
        // never going to be intuitive, it's just going to flicker
        // confusingly.
        if(pickResults[i].len < closest->len ||
           (pickResults[i].len == closest->len && pickResults[i].depth < closest->depth) ||
           (pickResults[i].len == closest->len && pickResults[i].depth == closest->depth &&
            pickResults[i].vertid < closest->vertid))
          closest = pickResults + i;
      }

      gl.glUnmapNamedBufferEXT(DebugData.pickResultBuf);

      return closest->vertid;
    }
  }

  return ~0U;
}

void GLReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                         uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.pickPixelFBO);
  gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, DebugData.pickPixelFBO);

  pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0.0f;
  gl.glClearBufferfv(eGL_COLOR, 0, pixel);

  DebugData.outWidth = DebugData.outHeight = 1.0f;
  gl.glViewport(0, 0, 1, 1);

  TextureDisplay texDisplay;

  texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
  texDisplay.flipY = false;
  texDisplay.hdrMultiplier = -1.0f;
  texDisplay.linearDisplayAsGamma = true;
  texDisplay.mip = mip;
  texDisplay.sampleIdx = sample;
  texDisplay.customShaderId = ResourceId();
  texDisplay.sliceFace = sliceFace;
  texDisplay.rangeMin = 0.0f;
  texDisplay.rangeMax = 1.0f;
  texDisplay.scale = 1.0f;
  texDisplay.resourceId = texture;
  texDisplay.typeHint = typeHint;
  texDisplay.rawOutput = true;
  texDisplay.xOffset = -float(x);
  texDisplay.yOffset = -float(y);

  RenderTextureInternal(texDisplay, eTexDisplay_MipShift);

  gl.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixel);

  if(!HasExt[ARB_gpu_shader5])
  {
    auto &texDetails = m_pDriver->m_Textures[texDisplay.resourceId];

    if(IsSIntFormat(texDetails.internalFormat))
    {
      int32_t casted[4] = {
          (int32_t)pixel[0], (int32_t)pixel[1], (int32_t)pixel[2], (int32_t)pixel[3],
      };

      memcpy(pixel, casted, sizeof(casted));
    }
    else if(IsUIntFormat(texDetails.internalFormat))
    {
      uint32_t casted[4] = {
          (uint32_t)pixel[0], (uint32_t)pixel[1], (uint32_t)pixel[2], (uint32_t)pixel[3],
      };

      memcpy(pixel, casted, sizeof(casted));
    }
  }

  {
    auto &texDetails = m_pDriver->m_Textures[texture];

    // need to read stencil separately as GL can't read both depth and stencil
    // at the same time.
    if(texDetails.internalFormat == eGL_DEPTH24_STENCIL8 ||
       texDetails.internalFormat == eGL_DEPTH32F_STENCIL8 ||
       texDetails.internalFormat == eGL_STENCIL_INDEX8)
    {
      texDisplay.red = texDisplay.blue = texDisplay.alpha = false;

      RenderTextureInternal(texDisplay, eTexDisplay_MipShift);

      uint32_t stencilpixel[4];
      gl.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)stencilpixel);

      if(!HasExt[ARB_gpu_shader5])
      {
        // bits weren't aliased, so re-cast back to uint.
        float fpix[4];
        memcpy(fpix, stencilpixel, sizeof(fpix));

        stencilpixel[0] = (uint32_t)fpix[0];
        stencilpixel[1] = (uint32_t)fpix[1];
      }

      // not sure whether [0] or [1] will return stencil values, so use
      // max of two because other channel should be 0
      pixel[1] = float(RDCMAX(stencilpixel[0], stencilpixel[1])) / 255.0f;

      // the first depth read will have read stencil instead.
      // NULL it out so the UI sees only stencil
      if(texDetails.internalFormat == eGL_STENCIL_INDEX8)
      {
        pixel[1] = float(RDCMAX(stencilpixel[0], stencilpixel[1])) / 255.0f;
        pixel[0] = 0.0f;
      }
    }
  }
}

void GLReplay::CopyTex2DMSToArray(GLuint &destArray, GLuint srcMS, GLint width, GLint height,
                                  GLint arraySize, GLint samples, GLenum intFormat)
{
  WrappedOpenGL &gl = *m_pDriver;

  // create temporary texture array, which we'll initialise to be the width/height in same format,
  // with the same number of array slices as multi samples.
  gl.glGenTextures(1, &destArray);
  gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, destArray);

  bool failed = false;

  if(!failed && !HasExt[ARB_compute_shader])
  {
    RDCWARN(
        "Can't copy multisampled texture to array for serialisation without ARB_compute_shader.");
    failed = true;
  }

  if(!failed && !HasExt[ARB_texture_view])
  {
    RDCWARN("Can't copy multisampled texture to array for serialisation without ARB_texture_view.");
    failed = true;
  }

  if(!failed && !HasExt[ARB_texture_storage])
  {
    RDCWARN(
        "Can't copy multisampled texture to array for serialisation without ARB_texture_view, and "
        "ARB_texture_view requires ARB_texture_storage.");
    failed = true;
  }

  if(failed)
  {
    // create using the non-storage API which is always available, so the texture is at least valid
    // (but with undefined/empty contents).
    gl.glTextureImage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 0, intFormat, width, height,
                           arraySize * samples, 0, GetBaseFormat(intFormat), GetDataType(intFormat),
                           NULL);
    gl.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);
    return;
  }

  // initialise the texture using texture storage, as required for texture views.
  gl.glTextureStorage3DEXT(destArray, eGL_TEXTURE_2D_ARRAY, 1, intFormat, width, height,
                           arraySize * samples);

  GLRenderState rs(&gl.GetHookset());
  rs.FetchState(m_pDriver);

  GLenum viewClass;
  gl.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS,
                           sizeof(GLenum), (GLint *)&viewClass);

  GLenum fmt = eGL_R32UI;
  if(viewClass == eGL_VIEW_CLASS_8_BITS)
    fmt = eGL_R8UI;
  else if(viewClass == eGL_VIEW_CLASS_16_BITS)
    fmt = eGL_R16UI;
  else if(viewClass == eGL_VIEW_CLASS_24_BITS)
    fmt = eGL_RGB8UI;
  else if(viewClass == eGL_VIEW_CLASS_32_BITS)
    fmt = eGL_RGBA8UI;
  else if(viewClass == eGL_VIEW_CLASS_48_BITS)
    fmt = eGL_RGB16UI;
  else if(viewClass == eGL_VIEW_CLASS_64_BITS)
    fmt = eGL_RG32UI;
  else if(viewClass == eGL_VIEW_CLASS_96_BITS)
    fmt = eGL_RGB32UI;
  else if(viewClass == eGL_VIEW_CLASS_128_BITS)
    fmt = eGL_RGBA32UI;

  GLuint texs[2];
  gl.glGenTextures(2, texs);
  gl.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, fmt, 0, 1, 0, arraySize * samples);
  gl.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, fmt, 0, 1, 0, arraySize);

  gl.glBindImageTexture(2, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
  gl.glActiveTexture(eGL_TEXTURE0);
  gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
  gl.glBindSampler(0, DebugData.pointNoMipSampler);
  gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
  gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

  gl.glUseProgram(DebugData.MS2Array);

  GLint loc = gl.glGetUniformLocation(DebugData.MS2Array, "mscopy");
  if(loc >= 0)
  {
    gl.glProgramUniform4ui(DebugData.MS2Array, loc, samples, 0, 0, 0);

    gl.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize * samples));
  }
  gl.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  gl.glDeleteTextures(2, texs);

  rs.ApplyState(m_pDriver);
}

bool GLReplay::RenderTexture(TextureDisplay cfg)
{
  return RenderTextureInternal(cfg, eTexDisplay_BlendAlpha | eTexDisplay_MipShift);
}

bool GLReplay::RenderTextureInternal(TextureDisplay cfg, int flags)
{
  const bool blendAlpha = (flags & eTexDisplay_BlendAlpha) != 0;
  const bool mipShift = (flags & eTexDisplay_MipShift) != 0;

  WrappedOpenGL &gl = *m_pDriver;

  auto &texDetails = m_pDriver->m_Textures[cfg.resourceId];

  if(texDetails.internalFormat == eGL_NONE)
    return false;

  bool renderbuffer = false;

  int intIdx = 0;

  int resType;
  switch(texDetails.curType)
  {
    case eGL_RENDERBUFFER:
      resType = RESTYPE_TEX2D;
      if(texDetails.samples > 1)
        resType = RESTYPE_TEX2DMS;
      renderbuffer = true;
      break;
    case eGL_TEXTURE_1D: resType = RESTYPE_TEX1D; break;
    default:
      RDCWARN("Unexpected texture type");
    // fall through
    case eGL_TEXTURE_2D: resType = RESTYPE_TEX2D; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: resType = RESTYPE_TEX2DMS; break;
    case eGL_TEXTURE_RECTANGLE: resType = RESTYPE_TEXRECT; break;
    case eGL_TEXTURE_BUFFER: resType = RESTYPE_TEXBUFFER; break;
    case eGL_TEXTURE_3D: resType = RESTYPE_TEX3D; break;
    case eGL_TEXTURE_CUBE_MAP: resType = RESTYPE_TEXCUBE; break;
    case eGL_TEXTURE_1D_ARRAY: resType = RESTYPE_TEX1DARRAY; break;
    case eGL_TEXTURE_2D_ARRAY: resType = RESTYPE_TEX2DARRAY; break;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: resType = RESTYPE_TEXCUBEARRAY; break;
  }

  GLuint texname = texDetails.resource.name;
  GLenum target = texDetails.curType;

  // do blit from renderbuffer to texture, then sample from texture
  if(renderbuffer)
  {
    // need replay context active to do blit (as FBOs aren't shared)
    MakeCurrentReplayContext(&m_ReplayCtx);

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

    gl.glBlitFramebuffer(
        0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width, texDetails.height,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

    texname = texDetails.renderbufferReadTex;
    if(resType == RESTYPE_TEX2D)
      target = eGL_TEXTURE_2D;
    else
      target = eGL_TEXTURE_2D_MULTISAMPLE;
  }

  MakeCurrentReplayContext(m_DebugCtx);

  RDCGLenum dsTexMode = eGL_NONE;
  if(IsDepthStencilFormat(texDetails.internalFormat))
  {
    // stencil-only, make sure we display it as such
    if(texDetails.internalFormat == eGL_STENCIL_INDEX8)
    {
      cfg.red = false;
      cfg.green = true;
      cfg.blue = false;
      cfg.alpha = false;
    }

    // depth-only, make sure we display it as such
    if(GetBaseFormat(texDetails.internalFormat) == eGL_DEPTH_COMPONENT)
    {
      cfg.red = true;
      cfg.green = false;
      cfg.blue = false;
      cfg.alpha = false;
    }

    if(!cfg.red && cfg.green)
    {
      dsTexMode = eGL_STENCIL_INDEX;

      // Stencil texture sampling is not normalized in OpenGL
      intIdx = 1;
      float rangeScale = 1.0f;
      switch(texDetails.internalFormat)
      {
        case eGL_STENCIL_INDEX1: rangeScale = 1.0f; break;
        case eGL_STENCIL_INDEX4: rangeScale = 16.0f; break;
        default:
          RDCWARN("Unexpected raw format for stencil visualization");
        // fall through
        case eGL_DEPTH24_STENCIL8:
        case eGL_DEPTH32F_STENCIL8:
        case eGL_STENCIL_INDEX8: rangeScale = 255.0f; break;
        case eGL_STENCIL_INDEX16: rangeScale = 65535.0f; break;
      }
      cfg.rangeMin *= rangeScale;
      cfg.rangeMax *= rangeScale;
    }
    else
      dsTexMode = eGL_DEPTH_COMPONENT;
  }
  else
  {
    if(IsUIntFormat(texDetails.internalFormat))
      intIdx = 1;
    if(IsSIntFormat(texDetails.internalFormat))
      intIdx = 2;
  }

  gl.glUseProgram(0);
  gl.glUseProgramStages(DebugData.texDisplayPipe, eGL_VERTEX_SHADER_BIT, DebugData.texDisplayVSProg);
  gl.glUseProgramStages(DebugData.texDisplayPipe, eGL_FRAGMENT_SHADER_BIT,
                        DebugData.texDisplayProg[intIdx]);

  int numMips =
      GetNumMips(gl.m_Real, target, texname, texDetails.width, texDetails.height, texDetails.depth);

  if(cfg.customShaderId != ResourceId() &&
     gl.GetResourceManager()->HasCurrentResource(cfg.customShaderId))
  {
    GLuint customProg = gl.GetResourceManager()->GetCurrentResource(cfg.customShaderId).name;
    gl.glUseProgramStages(DebugData.texDisplayPipe, eGL_FRAGMENT_SHADER_BIT, customProg);

    GLint loc = -1;

    loc = gl.glGetUniformLocation(customProg, "RENDERDOC_TexDim");
    if(loc >= 0)
      gl.glProgramUniform4ui(customProg, loc, texDetails.width, texDetails.height, texDetails.depth,
                             (uint32_t)numMips);

    loc = gl.glGetUniformLocation(customProg, "RENDERDOC_SelectedMip");
    if(loc >= 0)
      gl.glProgramUniform1ui(customProg, loc, cfg.mip);

    loc = gl.glGetUniformLocation(customProg, "RENDERDOC_SelectedSliceFace");
    if(loc >= 0)
      gl.glProgramUniform1ui(customProg, loc, cfg.sliceFace);

    loc = gl.glGetUniformLocation(customProg, "RENDERDOC_SelectedSample");
    if(loc >= 0)
    {
      if(cfg.sampleIdx == ~0U)
        gl.glProgramUniform1i(customProg, loc, -texDetails.samples);
      else
        gl.glProgramUniform1i(customProg, loc,
                              (int)RDCCLAMP(cfg.sampleIdx, 0U, (uint32_t)texDetails.samples - 1));
    }

    loc = gl.glGetUniformLocation(customProg, "RENDERDOC_TextureType");
    if(loc >= 0)
      gl.glProgramUniform1ui(customProg, loc, resType);
  }
  gl.glBindProgramPipeline(DebugData.texDisplayPipe);

  gl.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + resType));
  gl.glBindTexture(target, texname);

  GLint origDSTexMode = eGL_DEPTH_COMPONENT;
  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
  {
    gl.glGetTexParameteriv(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &origDSTexMode);
    gl.glTexParameteri(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, dsTexMode);
  }

  // defined as arrays mostly for Coverity code analysis to stay calm about passing
  // them to the *TexParameter* functions
  GLint maxlevel[4] = {-1};
  GLint clampmaxlevel[4] = {};

  if(cfg.resourceId != DebugData.CustomShaderTexID)
    clampmaxlevel[0] = GLint(numMips - 1);

  gl.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);

  // need to ensure texture is mipmap complete by clamping TEXTURE_MAX_LEVEL.
  if(clampmaxlevel[0] != maxlevel[0] && cfg.resourceId != DebugData.CustomShaderTexID)
  {
    gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, clampmaxlevel);
  }
  else
  {
    maxlevel[0] = -1;
  }

  if(cfg.mip == 0 && cfg.scale < 1.0f && dsTexMode == eGL_NONE && resType != RESTYPE_TEXBUFFER &&
     resType != RESTYPE_TEXRECT)
  {
    gl.glBindSampler(resType, DebugData.linearSampler);
  }
  else
  {
    if(resType == RESTYPE_TEXRECT || resType == RESTYPE_TEX2DMS || resType == RESTYPE_TEXBUFFER)
      gl.glBindSampler(resType, DebugData.pointNoMipSampler);
    else
      gl.glBindSampler(resType, DebugData.pointSampler);
  }

  GLint tex_x = texDetails.width, tex_y = texDetails.height, tex_z = texDetails.depth;

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  TexDisplayUBOData *ubo =
      (TexDisplayUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(TexDisplayUBOData),
                                               GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  float x = cfg.xOffset;
  float y = cfg.yOffset;

  ubo->Position.x = x;
  ubo->Position.y = y;
  ubo->Scale = cfg.scale;

  if(cfg.scale <= 0.0f)
  {
    float xscale = DebugData.outWidth / float(tex_x);
    float yscale = DebugData.outHeight / float(tex_y);

    ubo->Scale = RDCMIN(xscale, yscale);

    if(yscale > xscale)
    {
      ubo->Position.x = 0;
      ubo->Position.y = (DebugData.outHeight - (tex_y * ubo->Scale)) * 0.5f;
    }
    else
    {
      ubo->Position.y = 0;
      ubo->Position.x = (DebugData.outWidth - (tex_x * ubo->Scale)) * 0.5f;
    }
  }

  ubo->HDRMul = cfg.hdrMultiplier;

  ubo->FlipY = cfg.flipY ? 1 : 0;

  if(cfg.rangeMax <= cfg.rangeMin)
    cfg.rangeMax += 0.00001f;

  if(dsTexMode == eGL_NONE)
  {
    ubo->Channels.x = cfg.red ? 1.0f : 0.0f;
    ubo->Channels.y = cfg.green ? 1.0f : 0.0f;
    ubo->Channels.z = cfg.blue ? 1.0f : 0.0f;
    ubo->Channels.w = cfg.alpha ? 1.0f : 0.0f;
  }
  else
  {
    // Both depth and stencil texture mode use the red channel
    ubo->Channels.x = 1.0f;
    ubo->Channels.y = 0.0f;
    ubo->Channels.z = 0.0f;
    ubo->Channels.w = 0.0f;
  }

  ubo->RangeMinimum = cfg.rangeMin;
  ubo->InverseRangeSize = 1.0f / (cfg.rangeMax - cfg.rangeMin);

  ubo->MipLevel = (int)cfg.mip;
  if(texDetails.curType != eGL_TEXTURE_3D)
    ubo->Slice = (float)cfg.sliceFace + 0.001f;
  else
    ubo->Slice = (float)(cfg.sliceFace >> cfg.mip);

  ubo->OutputDisplayFormat = resType;

  if(cfg.overlay == DebugOverlay::NaN)
    ubo->OutputDisplayFormat |= TEXDISPLAY_NANS;

  if(cfg.overlay == DebugOverlay::Clipping)
    ubo->OutputDisplayFormat |= TEXDISPLAY_CLIPPING;

  if(!IsSRGBFormat(texDetails.internalFormat) && cfg.linearDisplayAsGamma)
    ubo->OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

  ubo->RawOutput = cfg.rawOutput ? 1 : 0;

  ubo->TextureResolutionPS.x = float(RDCMAX(1, tex_x >> cfg.mip));
  ubo->TextureResolutionPS.y = float(RDCMAX(1, tex_y >> cfg.mip));
  ubo->TextureResolutionPS.z = float(RDCMAX(1, tex_z >> cfg.mip));

  if(mipShift)
    ubo->MipShift = float(1 << cfg.mip);
  else
    ubo->MipShift = 1.0f;

  ubo->OutputRes.x = DebugData.outWidth;
  ubo->OutputRes.y = DebugData.outHeight;

  ubo->SampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, (uint32_t)texDetails.samples - 1);

  // hacky resolve
  if(cfg.sampleIdx == ~0U)
    ubo->SampleIdx = -texDetails.samples;

  gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  if(cfg.rawOutput || !blendAlpha)
  {
    gl.glDisable(eGL_BLEND);
  }
  else
  {
    gl.glEnable(eGL_BLEND);
    gl.glBlendFunc(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA);
  }

  gl.glDisable(eGL_DEPTH_TEST);

  gl.glEnable(eGL_FRAMEBUFFER_SRGB);

  gl.glBindVertexArray(DebugData.emptyVAO);
  gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

  if(maxlevel[0] >= 0)
    gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);

  gl.glBindSampler(0, 0);

  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
    gl.glTexParameteri(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, origDSTexMode);

  return true;
}

void GLReplay::RenderCheckerboard()
{
  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &gl = *m_pDriver;

  gl.glUseProgram(DebugData.checkerProg);

  gl.glDisable(eGL_DEPTH_TEST);

  gl.glEnable(eGL_FRAMEBUFFER_SRGB);

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  Vec4f *ubo = (Vec4f *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(Vec4f) * 2,
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  ubo[0] = RenderDoc::Inst().LightCheckerboardColor();
  ubo[1] = RenderDoc::Inst().DarkCheckerboardColor();

  gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  gl.glBindVertexArray(DebugData.emptyVAO);
  gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
}

void GLReplay::RenderHighlightBox(float w, float h, float scale)
{
  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &gl = *m_pDriver;

  GLint sz = GLint(scale);

  struct rect
  {
    GLint x, y;
    GLint w, h;
  };

  rect tl = {GLint(w / 2.0f + 0.5f), GLint(h / 2.0f + 0.5f), 1, 1};

  rect scissors[4] = {
      {tl.x, tl.y - (GLint)sz - 1, 1, sz + 1},
      {tl.x + (GLint)sz, tl.y - (GLint)sz - 1, 1, sz + 2},
      {tl.x, tl.y, sz, 1},
      {tl.x, tl.y - (GLint)sz - 1, sz, 1},
  };

  // inner
  gl.glEnable(eGL_SCISSOR_TEST);
  gl.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  for(size_t i = 0; i < ARRAY_COUNT(scissors); i++)
  {
    gl.glScissor(scissors[i].x, scissors[i].y, scissors[i].w, scissors[i].h);
    gl.glClear(eGL_COLOR_BUFFER_BIT);
  }

  scissors[0].x--;
  scissors[1].x++;
  scissors[2].x--;
  scissors[3].x--;

  scissors[0].y--;
  scissors[1].y--;
  scissors[2].y++;
  scissors[3].y--;

  scissors[0].h += 2;
  scissors[1].h += 2;
  scissors[2].w += 2;
  scissors[3].w += 2;

  // outer
  gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  for(size_t i = 0; i < ARRAY_COUNT(scissors); i++)
  {
    gl.glScissor(scissors[i].x, scissors[i].y, scissors[i].w, scissors[i].h);
    gl.glClear(eGL_COLOR_BUFFER_BIT);
  }

  gl.glDisable(eGL_SCISSOR_TEST);
}

void GLReplay::SetupOverlayPipeline(GLuint Program, GLuint Pipeline, GLuint fragProgram)
{
  WrappedOpenGL &gl = *m_pDriver;

  void *ctx = m_ReplayCtx.ctx;

  if(Program == 0)
  {
    if(Pipeline == 0)
    {
      return;
    }
    else
    {
      ResourceId id = m_pDriver->GetResourceManager()->GetID(ProgramPipeRes(ctx, Pipeline));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      for(size_t i = 0; i < 4; i++)
      {
        if(pipeDetails.stageShaders[i] != ResourceId())
        {
          GLuint progsrc =
              m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
          GLuint progdst = m_pDriver->m_Shaders[pipeDetails.stageShaders[i]].prog;

          gl.glUseProgramStages(DebugData.overlayPipe, ShaderBit(i), progdst);

          CopyProgramUniforms(gl.GetHookset(), progsrc, progdst);

          if(i == 0)
          {
            CopyProgramAttribBindings(gl.GetHookset(), progsrc, progdst,
                                      GetShader(pipeDetails.stageShaders[i], ""));

            gl.glLinkProgram(progdst);
          }
        }
      }
    }
  }
  else
  {
    auto &progDetails =
        m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetID(ProgramRes(ctx, Program))];

    for(size_t i = 0; i < 4; i++)
    {
      if(progDetails.stageShaders[i] != ResourceId())
      {
        GLuint progdst = m_pDriver->m_Shaders[progDetails.stageShaders[i]].prog;

        gl.glUseProgramStages(DebugData.overlayPipe, ShaderBit(i), progdst);

        // we have to link the program first, as this trashes all uniform values
        if(i == 0)
        {
          CopyProgramAttribBindings(gl.GetHookset(), Program, progdst,
                                    GetShader(progDetails.stageShaders[i], ""));

          gl.glLinkProgram(progdst);
        }

        CopyProgramUniforms(gl.GetHookset(), Program, progdst);
      }
    }
  }

  // use the generic FS program by default, can be overridden for specific overlays if needed
  gl.glUseProgramStages(DebugData.overlayPipe, eGL_FRAGMENT_SHADER_BIT, fragProgram);
}

ResourceId GLReplay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                   uint32_t eventId, const vector<uint32_t> &passEvents)
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLMarkerRegion renderoverlay(StringFormat::Fmt("RenderOverlay %d", overlay));

  void *ctx = m_ReplayCtx.ctx;

  GLRenderState rs(&gl.GetHookset());
  rs.FetchState(&gl);

  // use our overlay pipeline that we'll fill up with all the right
  // shaders, then replace the fragment shader with our own.
  gl.glUseProgram(0);
  gl.glBindProgramPipeline(DebugData.overlayPipe);

  // we bind the separable program created for each shader, and copy
  // uniforms and attrib bindings from the 'real' programs, wherever
  // they are.
  SetupOverlayPipeline(rs.Program.name, rs.Pipeline.name, DebugData.fixedcolFSProg);

  auto &texDetails = m_pDriver->m_Textures[texid];

  GLenum texBindingEnum = eGL_TEXTURE_2D;
  GLenum texQueryEnum = eGL_TEXTURE_BINDING_2D;

  if(texDetails.samples > 1)
  {
    texBindingEnum = eGL_TEXTURE_2D_MULTISAMPLE;
    texQueryEnum = eGL_TEXTURE_BINDING_2D_MULTISAMPLE;
  }

  // resize (or create) the overlay texture and FBO if necessary
  if(DebugData.overlayTexWidth != texDetails.width ||
     DebugData.overlayTexHeight != texDetails.height ||
     DebugData.overlayTexSamples != texDetails.samples)
  {
    if(DebugData.overlayFBO)
    {
      gl.glDeleteFramebuffers(1, &DebugData.overlayFBO);
      gl.glDeleteTextures(1, &DebugData.overlayTex);
    }

    gl.glGenFramebuffers(1, &DebugData.overlayFBO);
    gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

    GLuint curTex = 0;
    gl.glGetIntegerv(texQueryEnum, (GLint *)&curTex);

    gl.glGenTextures(1, &DebugData.overlayTex);
    gl.glBindTexture(texBindingEnum, DebugData.overlayTex);

    DebugData.overlayTexWidth = texDetails.width;
    DebugData.overlayTexHeight = texDetails.height;
    DebugData.overlayTexSamples = texDetails.samples;

    if(DebugData.overlayTexSamples > 1)
    {
      gl.glTextureStorage2DMultisampleEXT(DebugData.overlayTex, texBindingEnum, texDetails.samples,
                                          eGL_RGBA16, texDetails.width, texDetails.height, true);
    }
    else
    {
      GLint internalFormat = eGL_RGBA16F;
      GLenum format = eGL_RGBA;
      GLenum type = eGL_FLOAT;

      if(IsGLES && !HasExt[EXT_color_buffer_float])
      {
        internalFormat = eGL_RGBA8;
        type = eGL_UNSIGNED_BYTE;
      }

      gl.glTextureImage2DEXT(DebugData.overlayTex, texBindingEnum, 0, internalFormat,
                             texDetails.width, texDetails.height, 0, format, type, NULL);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
      gl.glTexParameteri(texBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
    }
    gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);

    gl.glBindTexture(texBindingEnum, curTex);
  }

  gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

  // disable several tests/allow rendering - some overlays will override
  // these states but commonly we don't want to inherit these states from
  // the program's state.
  gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl.glDisable(eGL_BLEND);
  gl.glDisable(eGL_SCISSOR_TEST);
  gl.glDepthMask(GL_FALSE);
  gl.glDisable(eGL_CULL_FACE);
  if(!IsGLES)
    gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
  gl.glDisable(eGL_DEPTH_TEST);
  gl.glDisable(eGL_STENCIL_TEST);
  gl.glStencilMask(0);

  if(overlay == DebugOverlay::NaN || overlay == DebugOverlay::Clipping)
  {
    // just need the basic texture
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);
  }
  else if(overlay == DebugOverlay::Drawcall)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.5f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    float colVal[] = {0.8f, 0.1f, 0.8f, 1.0f};
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, colVal);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::Wireframe)
  {
    float wireCol[] = {200.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, wireCol);

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    wireCol[3] = 1.0f;
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, wireCol);

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::ViewportScissor)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    // don't need to use the existing program at all!
    gl.glUseProgram(DebugData.outlineQuadProg);
    gl.glBindProgramPipeline(0);

    gl.glDisablei(eGL_SCISSOR_TEST, 0);

    if(HasExt[ARB_viewport_array])
      gl.glViewportIndexedf(0, rs.Viewports[0].x, rs.Viewports[0].y, rs.Viewports[0].width,
                            rs.Viewports[0].height);
    else
      gl.glViewport((GLint)rs.Viewports[0].x, (GLint)rs.Viewports[0].y,
                    (GLsizei)rs.Viewports[0].width, (GLsizei)rs.Viewports[0].height);

    gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
    OutlineUBOData *cdata =
        (OutlineUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(OutlineUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    cdata->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
    cdata->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
    cdata->ViewRect =
        Vec4f(rs.Viewports[0].x, rs.Viewports[0].y, rs.Viewports[0].width, rs.Viewports[0].height);
    cdata->Scissor = 0;

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

    if(rs.Scissors[0].enabled)
    {
      Vec4f scissor((float)rs.Scissors[0].x, (float)rs.Scissors[0].y, (float)rs.Scissors[0].width,
                    (float)rs.Scissors[0].height);

      if(HasExt[ARB_viewport_array])
        gl.glViewportIndexedf(0, scissor.x, scissor.y, scissor.z, scissor.w);
      else
        gl.glViewport(rs.Scissors[0].x, rs.Scissors[0].y, rs.Scissors[0].width,
                      rs.Scissors[0].height);

      cdata = (OutlineUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(OutlineUBOData),
                                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

      cdata->Inner_Color = Vec4f(0.2f, 0.2f, 0.9f, 0.7f);
      cdata->Border_Color = Vec4f(0.1f, 0.1f, 0.1f, 1.0f);
      cdata->ViewRect = scissor;
      cdata->Scissor = 1;

      gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
    }
  }
  else if(overlay == DebugOverlay::Depth || overlay == DebugOverlay::Stencil)
  {
    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    float red[] = {1.0f, 0.0f, 0.0f, 1.0f};
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, red);

    ReplayLog(eventId, eReplay_OnlyDraw);

    GLuint curDepth = 0, curStencil = 0;

    gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&curDepth);
    gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_STENCIL_ATTACHMENT,
                                                     eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                     (GLint *)&curStencil);

    GLenum copyBindingEnum = texBindingEnum;
    GLenum copyQueryEnum = texQueryEnum;

    GLuint depthCopy = 0, stencilCopy = 0;

    GLint mip = 0;
    GLint layer = 0;

    // create matching depth for existing FBO
    if(curDepth != 0)
    {
      GLint type = 0;
      gl.glGetNamedFramebufferAttachmentParameterivEXT(
          rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;

        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
              eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

          layer = CubeTargetIndex(face);
        }
      }
      else
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(RenderbufferRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;
      }

      if(copyBindingEnum == eGL_TEXTURE_CUBE_MAP)
      {
        copyBindingEnum = eGL_TEXTURE_2D;
        copyQueryEnum = eGL_TEXTURE_BINDING_2D;
      }

      GLuint curTex = 0;
      gl.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      gl.glGenTextures(1, &depthCopy);
      gl.glBindTexture(copyBindingEnum, depthCopy);
      if(DebugData.overlayTexSamples > 1)
      {
        gl.glTextureStorage2DMultisampleEXT(depthCopy, copyBindingEnum, DebugData.overlayTexSamples,
                                            fmt, DebugData.overlayTexWidth,
                                            DebugData.overlayTexHeight, true);
      }
      else
      {
        gl.glTextureImage2DEXT(depthCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                               DebugData.overlayTexHeight, 0, GetBaseFormat(fmt), GetDataType(fmt),
                               NULL);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      }

      gl.glBindTexture(copyBindingEnum, curTex);
    }

    // create matching separate stencil if relevant
    if(curStencil != curDepth && curStencil != 0)
    {
      GLint type = 0;
      gl.glGetNamedFramebufferAttachmentParameterivEXT(
          rs.DrawFBO.name, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
              eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

          layer = CubeTargetIndex(face);
        }
      }
      else
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(RenderbufferRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;
      }

      GLuint curTex = 0;
      gl.glGetIntegerv(copyQueryEnum, (GLint *)&curTex);

      gl.glGenTextures(1, &stencilCopy);
      gl.glBindTexture(copyBindingEnum, stencilCopy);
      if(DebugData.overlayTexSamples > 1)
      {
        gl.glTextureStorage2DMultisampleEXT(
            stencilCopy, copyBindingEnum, DebugData.overlayTexSamples, fmt,
            DebugData.overlayTexWidth, DebugData.overlayTexHeight, true);
      }
      else
      {
        gl.glTextureImage2DEXT(stencilCopy, copyBindingEnum, 0, fmt, DebugData.overlayTexWidth,
                               DebugData.overlayTexHeight, 0, GetBaseFormat(fmt), GetDataType(fmt),
                               NULL);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(copyBindingEnum, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
      }

      gl.glBindTexture(copyBindingEnum, curTex);
    }

    // bind depth/stencil to overlay FBO (currently bound to DRAW_FRAMEBUFFER)
    if(curDepth != 0 && curDepth == curStencil)
    {
      if(layer == 0)
        gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, depthCopy, mip);
      else
        gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, depthCopy,
                                     mip, layer);
    }
    else if(curDepth != 0)
    {
      if(layer == 0)
        gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depthCopy, mip);
      else
        gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depthCopy, mip,
                                     layer);
    }
    else if(curStencil != 0)
    {
      if(layer == 0)
        gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, stencilCopy, mip);
      else
        gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, stencilCopy, mip,
                                     layer);
    }

    // bind the 'real' fbo to the read framebuffer, so we can blit from it
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, rs.DrawFBO.name);

    float green[] = {0.0f, 1.0f, 0.0f, 1.0f};
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, green);

    if(overlay == DebugOverlay::Depth)
    {
      if(rs.Enabled[GLRenderState::eEnabled_DepthTest])
        gl.glEnable(eGL_DEPTH_TEST);
      else
        gl.glDisable(eGL_DEPTH_TEST);

      if(rs.DepthWriteMask)
        gl.glDepthMask(GL_TRUE);
      else
        gl.glDepthMask(GL_FALSE);
    }
    else
    {
      if(rs.Enabled[GLRenderState::eEnabled_StencilTest])
        gl.glEnable(eGL_STENCIL_TEST);
      else
        gl.glDisable(eGL_STENCIL_TEST);

      gl.glStencilMaskSeparate(eGL_FRONT, rs.StencilFront.writemask);
      gl.glStencilMaskSeparate(eGL_BACK, rs.StencilBack.writemask);
    }

    // get latest depth/stencil from read FBO (existing FBO) into draw FBO (overlay FBO)
    gl.glBlitFramebuffer(0, 0, DebugData.overlayTexWidth, DebugData.overlayTexHeight, 0, 0,
                         DebugData.overlayTexWidth, DebugData.overlayTexHeight,
                         GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    ReplayLog(eventId, eReplay_OnlyDraw);

    // unset depth/stencil textures from overlay FBO and delete temp depth/stencil
    if(curDepth != 0 && curDepth == curStencil)
      gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
    else if(curDepth != 0)
      gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, 0, 0);
    else if(curStencil != 0)
      gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, 0, 0);
    if(depthCopy != 0)
      gl.glDeleteTextures(1, &depthCopy);
    if(stencilCopy != 0)
      gl.glDeleteTextures(1, &stencilCopy);
  }
  else if(overlay == DebugOverlay::BackfaceCull)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    col[0] = 1.0f;
    col[3] = 1.0f;

    GLint colLoc = gl.glGetUniformLocation(DebugData.fixedcolFSProg, "RENDERDOC_Fixed_Color");
    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);

    // only enable cull face if it was enabled originally (otherwise
    // we just render green over the exact same area, so it shows up "passing")
    if(rs.Enabled[GLRenderState::eEnabled_CullFace])
      gl.glEnable(eGL_CULL_FACE);

    col[0] = 0.0f;
    col[1] = 1.0f;

    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, col);

    ReplayLog(eventId, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }
      else
      {
        // if we don't replay the real state, restore what we've changed
        rs.ApplyState(&gl);
      }

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      for(int i = 0; i < 8; i++)
        gl.glClearBufferfv(eGL_COLOR, i, black);

      for(size_t i = 0; i < events.size(); i++)
      {
        m_pDriver->ReplayLog(events[i], events[i], eReplay_OnlyDraw);

        if(overlay == DebugOverlay::ClearBeforePass && i + 1 < events.size())
          m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
      }
    }
  }
  else if(overlay == DebugOverlay::TriangleSizeDraw || overlay == DebugOverlay::TriangleSizePass)
  {
    SCOPED_TIMER("Triangle Size");

    float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, black);

    MeshUBOData uboParams = {};
    uboParams.homogenousInput = 1;
    uboParams.invProj = Matrix4f::Identity();
    uboParams.mvp = Matrix4f::Identity();

    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[0]);

    MeshUBOData *uboptr =
        (MeshUBOData *)gl.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(MeshUBOData),
                                           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[1]);
    Vec4f *v = (Vec4f *)gl.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(overdrawRamp),
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    memcpy(v, overdrawRamp, sizeof(overdrawRamp));
    gl.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    gl.glBindBuffer(eGL_COPY_WRITE_BUFFER, DebugData.UBOs[2]);
    v = (Vec4f *)gl.glMapBufferRange(eGL_COPY_WRITE_BUFFER, 0, sizeof(Vec4f),
                                     GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *v = Vec4f((float)texDetails.width, (float)texDetails.height);
    gl.glUnmapBuffer(eGL_COPY_WRITE_BUFFER);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::TriangleSizeDraw)
      events.clear();

    events.push_back(eventId);

    if(!events.empty() && DebugData.trisizeProg)
    {
      if(overlay == DebugOverlay::TriangleSizePass)
        ReplayLog(events[0], eReplay_WithoutDraw);
      else
        rs.ApplyState(m_pDriver);

      // this all happens on the replay context so we need a temp FBO/VAO
      GLuint overlayFBO = 0, tempVAO = 0;
      gl.glGenFramebuffers(1, &overlayFBO);
      gl.glGenVertexArrays(1, &tempVAO);

      for(size_t i = 0; i < events.size(); i++)
      {
        GLboolean blending = GL_FALSE;
        GLint depthwritemask = 1;
        GLint stencilfmask = 0xff, stencilbmask = 0xff;
        GLuint drawFBO = 0, prevVAO = 0;
        struct UBO
        {
          GLuint buf;
          GLint64 offs;
          GLint64 size;
        } ubos[3];

        // save the state we're going to mess with
        {
          gl.glGetIntegerv(eGL_DEPTH_WRITEMASK, &depthwritemask);
          gl.glGetIntegerv(eGL_STENCIL_WRITEMASK, &stencilfmask);
          gl.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &stencilbmask);

          gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&drawFBO);
          gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

          blending = gl.glIsEnabled(eGL_BLEND);

          for(uint32_t u = 0; u < 3; u++)
          {
            gl.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, u, (GLint *)&ubos[u].buf);
            gl.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, u, (GLint64 *)&ubos[u].offs);
            gl.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, u, (GLint64 *)&ubos[u].size);
          }
        }

        // disable depth and stencil writes
        gl.glDepthMask(GL_FALSE);
        gl.glStencilMask(GL_FALSE);

        // disable blending
        gl.glDisable(eGL_BLEND);

        // bind our UBOs
        gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
        gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, DebugData.UBOs[1]);
        gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[2]);

        const GLenum att = eGL_DEPTH_ATTACHMENT;
        GLuint depthObj = 0;
        GLint type = 0, level = 0, layered = 0, layer = 0;

        // fetch the details of the 'real' depth attachment
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&depthObj);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

        if(depthObj)
        {
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level);
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_LAYERED, &layered);

          layered = (layered != 0);

          layer = 0;
          if(layered == 0)
            gl.glGetNamedFramebufferAttachmentParameterivEXT(
                drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer);

          if(type != eGL_RENDERBUFFER)
          {
            ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, depthObj));
            WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

            if(details.curType == eGL_TEXTURE_CUBE_MAP)
            {
              GLenum face;
              gl.glGetNamedFramebufferAttachmentParameterivEXT(
                  drawFBO, att, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, (GLint *)&face);

              layer = CubeTargetIndex(face);
            }
          }
        }

        // bind our FBO
        gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, overlayFBO);
        gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);

        // now apply the depth texture binding
        if(depthObj)
        {
          if(type == eGL_RENDERBUFFER)
          {
            gl.glNamedFramebufferRenderbufferEXT(overlayFBO, att, eGL_RENDERBUFFER, depthObj);
          }
          else
          {
            if(!layered)
            {
              // we use old-style non-DSA for this because binding cubemap faces with EXT_dsa
              // is completely messed up and broken

              // if obj is a cubemap use face-specific targets
              ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, depthObj));
              WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

              if(details.curType == eGL_TEXTURE_CUBE_MAP)
              {
                GLenum faces[] = {
                    eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                    eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                    eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
                };

                if(layer < 6)
                {
                  gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, att, faces[layer], depthObj, level);
                }
                else
                {
                  RDCWARN(
                      "Invalid layer %u used to bind cubemap to framebuffer. Binding POSITIVE_X");
                  gl.glFramebufferTexture2D(eGL_DRAW_FRAMEBUFFER, att, faces[0], depthObj, level);
                }
              }
              else if(details.curType == eGL_TEXTURE_CUBE_MAP_ARRAY ||
                      details.curType == eGL_TEXTURE_1D_ARRAY ||
                      details.curType == eGL_TEXTURE_2D_ARRAY)
              {
                gl.glFramebufferTextureLayer(eGL_DRAW_FRAMEBUFFER, att, depthObj, level, layer);
              }
              else
              {
                RDCASSERT(layer == 0);
                gl.glNamedFramebufferTextureEXT(overlayFBO, att, depthObj, level);
              }
            }
            else
            {
              gl.glNamedFramebufferTextureEXT(overlayFBO, att, depthObj, level);
            }
          }
        }

        GLuint prog = 0, pipe = 0;
        gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
        gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

        gl.glUseProgram(DebugData.trisizeProg);
        gl.glBindProgramPipeline(0);

        const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[i]);

        for(uint32_t inst = 0; draw && inst < RDCMAX(1U, draw->numInstances); inst++)
        {
          MeshFormat postvs = GetPostVSBuffers(events[i], inst, MeshDataStage::GSOut);
          if(postvs.vertexResourceId == ResourceId())
            postvs = GetPostVSBuffers(events[i], inst, MeshDataStage::VSOut);

          if(postvs.vertexResourceId != ResourceId())
          {
            GLenum topo = MakeGLPrimitiveTopology(postvs.topology);

            gl.glBindVertexArray(tempVAO);

            {
              if(postvs.format.Special())
              {
                if(postvs.format.type == ResourceFormatType::R10G10B10A2)
                {
                  if(postvs.format.compType == CompType::UInt)
                    gl.glVertexAttribIFormat(0, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
                  if(postvs.format.compType == CompType::SInt)
                    gl.glVertexAttribIFormat(0, 4, eGL_INT_2_10_10_10_REV, 0);
                }
                else if(postvs.format.type == ResourceFormatType::R11G11B10)
                {
                  gl.glVertexAttribFormat(0, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
                }
                else
                {
                  RDCWARN("Unsupported vertex attribute format: %x", postvs.format.type);
                }
              }
              else if(postvs.format.compType == CompType::Float ||
                      postvs.format.compType == CompType::UNorm ||
                      postvs.format.compType == CompType::SNorm)
              {
                GLenum fmttype = eGL_UNSIGNED_INT;

                if(postvs.format.compByteWidth == 4)
                {
                  if(postvs.format.compType == CompType::Float)
                    fmttype = eGL_FLOAT;
                  else if(postvs.format.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_INT;
                  else if(postvs.format.compType == CompType::SNorm)
                    fmttype = eGL_INT;
                }
                else if(postvs.format.compByteWidth == 2)
                {
                  if(postvs.format.compType == CompType::Float)
                    fmttype = eGL_HALF_FLOAT;
                  else if(postvs.format.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_SHORT;
                  else if(postvs.format.compType == CompType::SNorm)
                    fmttype = eGL_SHORT;
                }
                else if(postvs.format.compByteWidth == 1)
                {
                  if(postvs.format.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_BYTE;
                  else if(postvs.format.compType == CompType::SNorm)
                    fmttype = eGL_BYTE;
                }

                gl.glVertexAttribFormat(0, postvs.format.compCount, fmttype,
                                        postvs.format.compType != CompType::Float, 0);
              }
              else if(postvs.format.compType == CompType::UInt ||
                      postvs.format.compType == CompType::SInt)
              {
                GLenum fmttype = eGL_UNSIGNED_INT;

                if(postvs.format.compByteWidth == 4)
                {
                  if(postvs.format.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_INT;
                  else if(postvs.format.compType == CompType::SInt)
                    fmttype = eGL_INT;
                }
                else if(postvs.format.compByteWidth == 2)
                {
                  if(postvs.format.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_SHORT;
                  else if(postvs.format.compType == CompType::SInt)
                    fmttype = eGL_SHORT;
                }
                else if(postvs.format.compByteWidth == 1)
                {
                  if(postvs.format.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_BYTE;
                  else if(postvs.format.compType == CompType::SInt)
                    fmttype = eGL_BYTE;
                }

                gl.glVertexAttribIFormat(0, postvs.format.compCount, fmttype, 0);
              }
              else if(postvs.format.compType == CompType::Double)
              {
                gl.glVertexAttribLFormat(0, postvs.format.compCount, eGL_DOUBLE, 0);
              }

              GLuint vb =
                  m_pDriver->GetResourceManager()->GetCurrentResource(postvs.vertexResourceId).name;
              gl.glBindVertexBuffer(0, vb, (GLintptr)postvs.vertexByteOffset,
                                    postvs.vertexByteStride);
            }

            gl.glEnableVertexAttribArray(0);
            gl.glDisableVertexAttribArray(1);

            if(postvs.indexByteStride)
            {
              GLenum idxtype = eGL_UNSIGNED_BYTE;
              if(postvs.indexByteStride == 2)
                idxtype = eGL_UNSIGNED_SHORT;
              else if(postvs.indexByteStride == 4)
                idxtype = eGL_UNSIGNED_INT;

              GLuint ib =
                  m_pDriver->GetResourceManager()->GetCurrentResource(postvs.indexResourceId).name;
              gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
              gl.glDrawElementsBaseVertex(topo, postvs.numIndices, idxtype,
                                          (const void *)uintptr_t(postvs.indexByteOffset),
                                          postvs.baseVertex);
            }
            else
            {
              gl.glDrawArrays(topo, 0, postvs.numIndices);
            }
          }
        }

        // pop the state that we messed with
        {
          gl.glBindProgramPipeline(pipe);
          gl.glUseProgram(prog);

          if(blending)
            gl.glEnable(eGL_BLEND);
          else
            gl.glDisable(eGL_BLEND);

          // restore the previous FBO/VAO
          gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);
          gl.glBindVertexArray(prevVAO);

          for(uint32_t u = 0; u < 3; u++)
          {
            if(ubos[u].buf == 0 || (ubos[u].offs == 0 && ubos[u].size == 0))
              gl.glBindBufferBase(eGL_UNIFORM_BUFFER, u, ubos[u].buf);
            else
              gl.glBindBufferRange(eGL_UNIFORM_BUFFER, u, ubos[u].buf, (GLintptr)ubos[u].offs,
                                   (GLsizeiptr)ubos[u].size);
          }

          gl.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
          gl.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
          gl.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
        }

        if(overlay == DebugOverlay::TriangleSizePass)
        {
          m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

          if(i + 1 < events.size())
            m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
        }
      }

      gl.glDeleteFramebuffers(1, &overlayFBO);
      gl.glDeleteVertexArrays(1, &tempVAO);

      if(overlay == DebugOverlay::TriangleSizePass)
        ReplayLog(eventId, eReplay_WithoutDraw);
    }
  }
  else if(overlay == DebugOverlay::QuadOverdrawDraw || overlay == DebugOverlay::QuadOverdrawPass)
  {
    if(DebugData.quadoverdrawFSProg)
    {
      SCOPED_TIMER("Quad Overdraw");

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      gl.glClearBufferfv(eGL_COLOR, 0, black);

      vector<uint32_t> events = passEvents;

      if(overlay == DebugOverlay::QuadOverdrawDraw)
        events.clear();

      events.push_back(eventId);

      if(!events.empty())
      {
        GLuint replacefbo = 0;
        GLuint quadtexs[3] = {0};
        gl.glGenFramebuffers(1, &replacefbo);
        gl.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);

        gl.glGenTextures(3, quadtexs);

        // image for quad usage
        gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, quadtexs[2]);
        gl.glTextureImage3DEXT(quadtexs[2], eGL_TEXTURE_2D_ARRAY, 0, eGL_R32UI,
                               RDCMAX(1, texDetails.width >> 1), RDCMAX(1, texDetails.height >> 1),
                               4, 0, eGL_RED_INTEGER, eGL_UNSIGNED_INT, NULL);
        gl.glTexParameteri(eGL_TEXTURE_2D_ARRAY, eGL_TEXTURE_MAX_LEVEL, 0);

        // temporarily attach to FBO to clear it
        GLint zero[4] = {0};
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 0);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 1);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 2);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);
        gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 3);
        gl.glClearBufferiv(eGL_COLOR, 0, zero);

        gl.glBindTexture(eGL_TEXTURE_2D, quadtexs[0]);
        gl.glTextureImage2DEXT(quadtexs[0], eGL_TEXTURE_2D, 0, eGL_RGBA8, texDetails.width,
                               texDetails.height, 0, eGL_RGBA, eGL_UNSIGNED_BYTE, NULL);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
        gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[0], 0);

        GLuint curDepth = 0, depthType = 0;

        // TODO handle non-2D depth/stencil attachments and fetch slice or cubemap face
        GLint mip = 0;

        gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&curDepth);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                         (GLint *)&depthType);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO.name, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

        GLenum fmt = eGL_DEPTH32F_STENCIL8;

        if(depthType == eGL_TEXTURE)
        {
          gl.glGetTextureLevelParameterivEXT(curDepth, texBindingEnum, mip,
                                             eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);
        }
        else
        {
          gl.glGetNamedRenderbufferParameterivEXT(curDepth, eGL_RENDERBUFFER_INTERNAL_FORMAT,
                                                  (GLint *)&fmt);
        }

        gl.glBindTexture(eGL_TEXTURE_2D, quadtexs[1]);
        gl.glTextureImage2DEXT(quadtexs[1], eGL_TEXTURE_2D, 0, fmt, texDetails.width,
                               texDetails.height, 0, GetBaseFormat(fmt), GetDataType(fmt), NULL);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
        gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

        GLenum dsAttach = eGL_DEPTH_STENCIL_ATTACHMENT;

        if(GetBaseFormat(fmt) == eGL_DEPTH_COMPONENT)
          dsAttach = eGL_DEPTH_ATTACHMENT;

        gl.glFramebufferTexture(eGL_FRAMEBUFFER, dsAttach, quadtexs[1], 0);

        if(overlay == DebugOverlay::QuadOverdrawPass)
          ReplayLog(events[0], eReplay_WithoutDraw);
        else
          rs.ApplyState(m_pDriver);

        for(size_t i = 0; i < events.size(); i++)
        {
          GLint depthwritemask = 1;
          GLint stencilfmask = 0xff, stencilbmask = 0xff;
          GLuint curdrawfbo = 0, curreadfbo = 0;
          struct
          {
            GLuint name;
            GLuint level;
            GLboolean layered;
            GLuint layer;
            GLenum access;
            GLenum format;
          } curimage0 = {0};

          // save the state we're going to mess with
          {
            gl.glGetIntegerv(eGL_DEPTH_WRITEMASK, &depthwritemask);
            gl.glGetIntegerv(eGL_STENCIL_WRITEMASK, &stencilfmask);
            gl.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &stencilbmask);

            gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curdrawfbo);
            gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curreadfbo);

            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_NAME, 0, (GLint *)&curimage0.name);
            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_LEVEL, 0, (GLint *)&curimage0.level);
            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_ACCESS, 0, (GLint *)&curimage0.access);
            gl.glGetIntegeri_v(eGL_IMAGE_BINDING_FORMAT, 0, (GLint *)&curimage0.format);
            gl.glGetBooleani_v(eGL_IMAGE_BINDING_LAYERED, 0, &curimage0.layered);
            if(curimage0.layered)
              gl.glGetIntegeri_v(eGL_IMAGE_BINDING_LAYER, 0, (GLint *)&curimage0.layer);
          }

          // disable depth and stencil writes
          gl.glDepthMask(GL_FALSE);
          gl.glStencilMask(GL_FALSE);

          // bind our FBO
          gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, replacefbo);
          // bind image
          gl.glBindImageTexture(0, quadtexs[2], 0, GL_TRUE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint prog = 0, pipe = 0;
          gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);
          gl.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

          // replace fragment shader. This is exactly what we did
          // at the start of this function for the single-event case, but now we have
          // to do it for every event
          SetupOverlayPipeline(prog, pipe, DebugData.quadoverdrawFSProg);
          gl.glUseProgram(0);
          gl.glBindProgramPipeline(DebugData.overlayPipe);

          gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curdrawfbo);
          gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width,
                               texDetails.height, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                               eGL_NEAREST);

          m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

          // pop the state that we messed with
          {
            gl.glBindProgramPipeline(pipe);
            gl.glUseProgram(prog);

            if(curimage0.name)
              gl.glBindImageTexture(0, curimage0.name, curimage0.level,
                                    curimage0.layered ? GL_TRUE : GL_FALSE, curimage0.layer,
                                    curimage0.access, curimage0.format);
            else
              gl.glBindImageTexture(0, 0, 0, GL_FALSE, 0, eGL_READ_ONLY, eGL_R32UI);

            gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curdrawfbo);
            gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curreadfbo);

            gl.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
            gl.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
            gl.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
          }

          if(overlay == DebugOverlay::QuadOverdrawPass)
          {
            m_pDriver->ReplayLog(0, events[i], eReplay_OnlyDraw);

            if(i + 1 < events.size())
              m_pDriver->ReplayLog(events[i], events[i + 1], eReplay_WithoutDraw);
          }
        }

        // resolve pass
        {
          gl.glUseProgram(DebugData.quadoverdrawResolveProg);
          gl.glBindProgramPipeline(0);

          gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 1, DebugData.UBOs[0]);

          Vec4f *v = (Vec4f *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(overdrawRamp),
                                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
          memcpy(v, overdrawRamp, sizeof(overdrawRamp));
          gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

          // modify our fbo to attach the overlay texture instead
          gl.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);
          gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);
          gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0);

          gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
          gl.glDisable(eGL_BLEND);
          gl.glDisable(eGL_SCISSOR_TEST);
          gl.glDepthMask(GL_FALSE);
          gl.glDisable(eGL_CULL_FACE);
          if(!IsGLES)
            gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
          gl.glDisable(eGL_DEPTH_TEST);
          gl.glDisable(eGL_STENCIL_TEST);
          gl.glStencilMask(0);
          gl.glViewport(0, 0, texDetails.width, texDetails.height);

          gl.glBindImageTexture(0, quadtexs[2], 0, GL_FALSE, 0, eGL_READ_WRITE, eGL_R32UI);

          GLuint emptyVAO = 0;
          gl.glGenVertexArrays(1, &emptyVAO);
          gl.glBindVertexArray(emptyVAO);
          gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
          gl.glBindVertexArray(0);
          gl.glDeleteVertexArrays(1, &emptyVAO);

          gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[0], 0);
        }

        gl.glDeleteFramebuffers(1, &replacefbo);
        gl.glDeleteTextures(3, quadtexs);

        if(overlay == DebugOverlay::QuadOverdrawPass)
          ReplayLog(eventId, eReplay_WithoutDraw);
      }
    }
  }
  else
  {
    RDCERR(
        "Unexpected/unimplemented overlay type - should implement a placeholder overlay for all "
        "types");
  }

  rs.ApplyState(m_pDriver);

  return m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, DebugData.overlayTex));
}

void GLReplay::RenderMesh(uint32_t eventId, const vector<MeshFormat> &secondaryDraws,
                          const MeshDisplay &cfg)
{
  WrappedOpenGL &gl = *m_pDriver;

  if(cfg.position.vertexResourceId == ResourceId())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, DebugData.outWidth / DebugData.outHeight);

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f ModelViewProj = projMat.Mul(camMat);
  Matrix4f guessProjInv;

  gl.glBindVertexArray(DebugData.meshVAO);

  const MeshFormat *meshData[2] = {&cfg.position, &cfg.second};

  GLenum topo = MakeGLPrimitiveTopology(cfg.position.topology);

  GLuint prog = DebugData.meshProg;

  MeshUBOData uboParams = {};
  MeshUBOData *uboptr = NULL;

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  gl.glUseProgram(prog);

  gl.glEnable(eGL_FRAMEBUFFER_SRGB);

  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
    {
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
    }

    guessProjInv = guessProj.Inverse();

    ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
  }

  uboParams.mvp = ModelViewProj;
  uboParams.homogenousInput = cfg.position.unproject;
  uboParams.pointSpriteSize = Vec2f(0.0f, 0.0f);

  if(!secondaryDraws.empty())
  {
    uboParams.displayFormat = MESHDISPLAY_SOLID;

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

    // secondary draws have to come from gl_Position which is float4
    gl.glVertexAttribFormat(0, 4, eGL_FLOAT, GL_FALSE, 0);
    gl.glEnableVertexAttribArray(0);
    gl.glDisableVertexAttribArray(1);

    for(size_t i = 0; i < secondaryDraws.size(); i++)
    {
      const MeshFormat &fmt = secondaryDraws[i];

      if(fmt.vertexResourceId != ResourceId())
      {
        uboParams.color = Vec4f(fmt.meshColor.x, fmt.meshColor.y, fmt.meshColor.z, fmt.meshColor.w);

        uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.vertexResourceId).name;
        gl.glBindVertexBuffer(0, vb, (GLintptr)fmt.vertexByteOffset, fmt.vertexByteStride);

        GLenum secondarytopo = MakeGLPrimitiveTopology(fmt.topology);

        if(fmt.indexByteStride)
        {
          GLuint ib = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.indexResourceId).name;
          gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);

          GLenum idxtype = eGL_UNSIGNED_BYTE;
          if(fmt.indexByteStride == 2)
            idxtype = eGL_UNSIGNED_SHORT;
          else if(fmt.indexByteStride == 4)
            idxtype = eGL_UNSIGNED_INT;

          gl.glDrawElementsBaseVertex(secondarytopo, fmt.numIndices, idxtype,
                                      (const void *)uintptr_t(fmt.indexByteOffset), fmt.baseVertex);
        }
        else
        {
          gl.glDrawArrays(secondarytopo, 0, fmt.numIndices);
        }
      }
    }
  }

  for(uint32_t i = 0; i < 2; i++)
  {
    if(meshData[i]->vertexResourceId == ResourceId())
      continue;

    if(meshData[i]->format.Special())
    {
      if(meshData[i]->format.type == ResourceFormatType::R10G10B10A2)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          gl.glVertexAttribIFormat(i, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
        if(meshData[i]->format.compType == CompType::SInt)
          gl.glVertexAttribIFormat(i, 4, eGL_INT_2_10_10_10_REV, 0);
      }
      else if(meshData[i]->format.type == ResourceFormatType::R11G11B10)
      {
        gl.glVertexAttribFormat(i, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
      }
      else
      {
        RDCWARN("Unsupported vertex attribute format: %x", meshData[i]->format.type);
      }
    }
    else if(meshData[i]->format.compType == CompType::Float ||
            meshData[i]->format.compType == CompType::UNorm ||
            meshData[i]->format.compType == CompType::SNorm)
    {
      GLenum fmttype = eGL_UNSIGNED_INT;

      if(meshData[i]->format.compByteWidth == 4)
      {
        if(meshData[i]->format.compType == CompType::Float)
          fmttype = eGL_FLOAT;
        else if(meshData[i]->format.compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_INT;
        else if(meshData[i]->format.compType == CompType::SNorm)
          fmttype = eGL_INT;
      }
      else if(meshData[i]->format.compByteWidth == 2)
      {
        if(meshData[i]->format.compType == CompType::Float)
          fmttype = eGL_HALF_FLOAT;
        else if(meshData[i]->format.compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_SHORT;
        else if(meshData[i]->format.compType == CompType::SNorm)
          fmttype = eGL_SHORT;
      }
      else if(meshData[i]->format.compByteWidth == 1)
      {
        if(meshData[i]->format.compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_BYTE;
        else if(meshData[i]->format.compType == CompType::SNorm)
          fmttype = eGL_BYTE;
      }

      gl.glVertexAttribFormat(i, meshData[i]->format.compCount, fmttype,
                              meshData[i]->format.compType != CompType::Float, 0);
    }
    else if(meshData[i]->format.compType == CompType::UInt ||
            meshData[i]->format.compType == CompType::SInt)
    {
      GLenum fmttype = eGL_UNSIGNED_INT;

      if(meshData[i]->format.compByteWidth == 4)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_INT;
        else if(meshData[i]->format.compType == CompType::SInt)
          fmttype = eGL_INT;
      }
      else if(meshData[i]->format.compByteWidth == 2)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_SHORT;
        else if(meshData[i]->format.compType == CompType::SInt)
          fmttype = eGL_SHORT;
      }
      else if(meshData[i]->format.compByteWidth == 1)
      {
        if(meshData[i]->format.compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_BYTE;
        else if(meshData[i]->format.compType == CompType::SInt)
          fmttype = eGL_BYTE;
      }

      gl.glVertexAttribIFormat(i, meshData[i]->format.compCount, fmttype, 0);
    }
    else if(meshData[i]->format.compType == CompType::Double)
    {
      gl.glVertexAttribLFormat(i, meshData[i]->format.compCount, eGL_DOUBLE, 0);
    }

    GLuint vb =
        m_pDriver->GetResourceManager()->GetCurrentResource(meshData[i]->vertexResourceId).name;
    gl.glBindVertexBuffer(i, vb, (GLintptr)meshData[i]->vertexByteOffset,
                          meshData[i]->vertexByteStride);
  }

  // enable position attribute
  gl.glEnableVertexAttribArray(0);
  gl.glDisableVertexAttribArray(1);

  gl.glEnable(eGL_DEPTH_TEST);

  // solid render
  if(cfg.solidShadeMode != SolidShade::NoSolid && topo != eGL_PATCHES)
  {
    gl.glDepthFunc(eGL_LESS);

    GLuint solidProg = prog;

    if(cfg.solidShadeMode == SolidShade::Lit && DebugData.meshgsProg)
    {
      // pick program with GS for per-face lighting
      solidProg = DebugData.meshgsProg;

      ClearGLErrors(gl.GetHookset());
      gl.glUseProgram(solidProg);
      GLenum err = gl.glGetError();

      err = eGL_NONE;
    }

    MeshUBOData *soliddata = (MeshUBOData *)gl.glMapBufferRange(
        eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    soliddata->mvp = ModelViewProj;
    soliddata->pointSpriteSize = Vec2f(0.0f, 0.0f);
    soliddata->homogenousInput = cfg.position.unproject;

    soliddata->color = Vec4f(0.8f, 0.8f, 0.0f, 1.0f);

    uint32_t OutputDisplayFormat = (uint32_t)cfg.solidShadeMode;
    if(cfg.solidShadeMode == SolidShade::Secondary && cfg.second.showAlpha)
      OutputDisplayFormat = MESHDISPLAY_SECONDARY_ALPHA;
    soliddata->displayFormat = OutputDisplayFormat;

    if(cfg.solidShadeMode == SolidShade::Lit)
      soliddata->invProj = projMat.Inverse();

    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    if(cfg.second.vertexResourceId != ResourceId())
      gl.glEnableVertexAttribArray(1);

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    if(cfg.position.indexByteStride)
    {
      GLenum idxtype = eGL_UNSIGNED_BYTE;
      if(cfg.position.indexByteStride == 2)
        idxtype = eGL_UNSIGNED_SHORT;
      else if(cfg.position.indexByteStride == 4)
        idxtype = eGL_UNSIGNED_INT;

      if(cfg.position.indexResourceId != ResourceId())
      {
        GLuint ib =
            m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.indexResourceId).name;
        gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
      }
      gl.glDrawElementsBaseVertex(topo, cfg.position.numIndices, idxtype,
                                  (const void *)uintptr_t(cfg.position.indexByteOffset),
                                  cfg.position.baseVertex);
    }
    else
    {
      gl.glDrawArrays(topo, 0, cfg.position.numIndices);
    }

    gl.glDisableVertexAttribArray(1);

    gl.glUseProgram(prog);
  }

  gl.glDepthFunc(eGL_ALWAYS);

  // wireframe render
  if(cfg.solidShadeMode == SolidShade::NoSolid || cfg.wireframeDraw || topo == eGL_PATCHES)
  {
    uboParams.color = Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y,
                            cfg.position.meshColor.z, cfg.position.meshColor.w);

    uboParams.displayFormat = MESHDISPLAY_SOLID;

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

    uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    if(cfg.position.indexByteStride)
    {
      GLenum idxtype = eGL_UNSIGNED_BYTE;
      if(cfg.position.indexByteStride == 2)
        idxtype = eGL_UNSIGNED_SHORT;
      else if(cfg.position.indexByteStride == 4)
        idxtype = eGL_UNSIGNED_INT;

      if(cfg.position.indexResourceId != ResourceId())
      {
        GLuint ib =
            m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.indexResourceId).name;
        gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);

        gl.glDrawElementsBaseVertex(topo != eGL_PATCHES ? topo : eGL_POINTS, cfg.position.numIndices,
                                    idxtype, (const void *)uintptr_t(cfg.position.indexByteOffset),
                                    cfg.position.baseVertex);
      }
    }
    else
    {
      gl.glDrawArrays(topo != eGL_PATCHES ? topo : eGL_POINTS, 0, cfg.position.numIndices);
    }
  }

  if(cfg.showBBox)
  {
    Vec4f a = Vec4f(cfg.minBounds.x, cfg.minBounds.y, cfg.minBounds.z, cfg.minBounds.w);
    Vec4f b = Vec4f(cfg.maxBounds.x, cfg.maxBounds.y, cfg.maxBounds.z, cfg.maxBounds.w);

    Vec4f TLN = Vec4f(a.x, b.y, a.z, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(b.x, b.y, a.z, 1.0f);
    Vec4f BLN = Vec4f(a.x, a.y, a.z, 1.0f);
    Vec4f BRN = Vec4f(b.x, a.y, a.z, 1.0f);

    Vec4f TLF = Vec4f(a.x, b.y, b.z, 1.0f);
    Vec4f TRF = Vec4f(b.x, b.y, b.z, 1.0f);
    Vec4f BLF = Vec4f(a.x, a.y, b.z, 1.0f);
    Vec4f BRF = Vec4f(b.x, a.y, b.z, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
    gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(bbox), &bbox[0]);

    gl.glBindVertexArray(DebugData.triHighlightVAO);

    uboParams.color = Vec4f(0.2f, 0.2f, 1.0f, 1.0f);

    Matrix4f mvpMat = projMat.Mul(camMat);

    uboParams.mvp = mvpMat;

    uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    // we want this to clip
    gl.glDepthFunc(eGL_LESS);

    gl.glDrawArrays(eGL_LINES, 0, 24);

    gl.glDepthFunc(eGL_ALWAYS);
  }

  // draw axis helpers
  if(!cfg.position.unproject)
  {
    gl.glBindVertexArray(DebugData.axisVAO);

    uboParams.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
    uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    gl.glDrawArrays(eGL_LINES, 0, 2);

    uboParams.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
    uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);
    gl.glDrawArrays(eGL_LINES, 2, 2);

    uboParams.color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
    uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);
    gl.glDrawArrays(eGL_LINES, 4, 2);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
  {
    gl.glBindVertexArray(DebugData.frustumVAO);

    uboParams.color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    uboParams.mvp = ModelViewProj;

    uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    *uboptr = uboParams;
    gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

    gl.glDrawArrays(eGL_LINES, 0, 24);
  }

  if(!IsGLES)
    gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

  // show highlighted vertex
  if(cfg.highlightVert != ~0U)
  {
    m_HighlightCache.CacheHighlightingData(eventId, cfg);

    GLenum meshtopo = topo;

    ///////////////////////////////////////////////////////////////
    // vectors to be set from buffers, depending on topology

    // this vert (blue dot, required)
    FloatVector activeVertex;

    // primitive this vert is a part of (red prim, optional)
    vector<FloatVector> activePrim;

    // for patch lists, to show other verts in patch (green dots, optional)
    // for non-patch lists, we use the activePrim and adjacentPrimVertices
    // to show what other verts are related
    vector<FloatVector> inactiveVertices;

    // adjacency (line or tri, strips or lists) (green prims, optional)
    // will be N*M long, N adjacent prims of M verts each. M = primSize below
    vector<FloatVector> adjacentPrimVertices;

    GLenum primTopo = eGL_TRIANGLES;
    uint32_t primSize = 3;    // number of verts per primitive

    if(meshtopo == eGL_LINES || meshtopo == eGL_LINES_ADJACENCY || meshtopo == eGL_LINE_STRIP ||
       meshtopo == eGL_LINE_STRIP_ADJACENCY)
    {
      primSize = 2;
      primTopo = eGL_LINES;
    }

    bool valid = m_HighlightCache.FetchHighlightPositions(cfg, activeVertex, activePrim,
                                                          adjacentPrimVertices, inactiveVertices);

    if(valid)
    {
      ////////////////////////////////////////////////////////////////
      // prepare rendering (for both vertices & primitives)

      // if data is from post transform, it will be in clipspace
      if(cfg.position.unproject)
        ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
      else
        ModelViewProj = projMat.Mul(camMat);

      uboParams.homogenousInput = cfg.position.unproject;

      uboParams.mvp = ModelViewProj;

      gl.glBindVertexArray(DebugData.triHighlightVAO);

      ////////////////////////////////////////////////////////////////
      // render primitives

      // Draw active primitive (red)
      uboParams.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);

      if(activePrim.size() >= primSize)
      {
        uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
        gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(Vec4f) * primSize, &activePrim[0]);

        gl.glDrawArrays(primTopo, 0, primSize);
      }

      // Draw adjacent primitives (green)
      uboParams.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
        gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(Vec4f) * adjacentPrimVertices.size(),
                           &adjacentPrimVertices[0]);

        gl.glDrawArrays(primTopo, 0, (GLsizei)adjacentPrimVertices.size());
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots
      float scale = 800.0f / float(DebugData.outHeight);
      float asp = float(DebugData.outWidth) / float(DebugData.outHeight);

      uboParams.pointSpriteSize = Vec2f(scale / asp, scale);

      // Draw active vertex (blue)
      uboParams.color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);

      uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      *uboptr = uboParams;
      gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      FloatVector vertSprite[4] = {
          activeVertex, activeVertex, activeVertex, activeVertex,
      };

      gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
      gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(vertSprite), &vertSprite[0]);

      gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

      // Draw inactive vertices (green)
      uboParams.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);

      uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
      *uboptr = uboParams;
      gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

      for(size_t i = 0; i < inactiveVertices.size(); i++)
      {
        vertSprite[0] = vertSprite[1] = vertSprite[2] = vertSprite[3] = inactiveVertices[i];

        gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(vertSprite), &vertSprite[0]);

        gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
      }
    }
  }
}
