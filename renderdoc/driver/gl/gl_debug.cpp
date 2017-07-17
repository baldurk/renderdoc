/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "serialise/string_utils.h"
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

  WrappedOpenGL &gl = *m_pDriver;

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

  WrappedOpenGL &gl = *m_pDriver;

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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.0f);

  {
    uint64_t id = MakeOutputWindow(WindowingSystem::Unknown, NULL, true);

    m_DebugID = id;
    m_DebugCtx = &m_OutputWindows[id];

    MakeCurrentReplayContext(m_DebugCtx);
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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.2f);

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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.4f);

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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.6f);

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
          defines += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
          defines += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
          defines += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";

          GenerateGLSLShader(cs, shaderType, defines, GetEmbeddedResource(glsl_minmaxtile_comp),
                             glslCSVer);

          DebugData.minmaxTileProgram[idx] = CreateCShaderProgram(cs);
        }

        {
          string defines = extensions;
          defines += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
          defines += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
          defines += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";

          GenerateGLSLShader(cs, shaderType, defines, GetEmbeddedResource(glsl_histogram_comp),
                             glslCSVer);

          DebugData.histogramProgram[idx] = CreateCShaderProgram(cs);
        }

        if(t == 1)
        {
          string defines = extensions;
          defines += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 0.8f);

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

  RenderDoc::Inst().SetProgress(DebugManagerInit, 1.0f);

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

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    gl.glDeleteBuffers(1, &it->second.vsout.buf);
    gl.glDeleteBuffers(1, &it->second.vsout.idxBuf);
    gl.glDeleteBuffers(1, &it->second.gsout.buf);
    gl.glDeleteBuffers(1, &it->second.gsout.idxBuf);
  }

  m_PostVSData.clear();

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

uint32_t GLReplay::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y)
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

  ResourceFormat resFmt;
  resFmt.compByteWidth = cfg.position.compByteWidth;
  resFmt.compCount = cfg.position.compCount;
  resFmt.compType = cfg.position.compType;
  resFmt.special = false;
  if(cfg.position.specialFormat != SpecialFormat::Unknown)
  {
    resFmt.special = true;
    resFmt.specialFormat = cfg.position.specialFormat;
  }

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
    ;
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

    /* Calculate the ray direction first in the regular way (above), so we can use the
       the output for testing if the ray we are picking is negative or not. This is similar
       to checking against the forward direction of the camera, but more robust
    */
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
  cdata->use_indices = cfg.position.idxByteWidth ? 1U : 0U;
  cdata->numVerts = cfg.position.numVerts;
  bool isTriangleMesh = true;
  switch(cfg.position.topo)
  {
    case Topology::TriangleList:
    {
      cdata->meshMode = MESH_TRIANGLE_LIST;
      break;
    };
    case Topology::TriangleStrip:
    {
      cdata->meshMode = MESH_TRIANGLE_STRIP;
      break;
    };
    case Topology::TriangleFan:
    {
      cdata->meshMode = MESH_TRIANGLE_FAN;
      break;
    };
    case Topology::TriangleList_Adj:
    {
      cdata->meshMode = MESH_TRIANGLE_LIST_ADJ;
      break;
    };
    case Topology::TriangleStrip_Adj:
    {
      cdata->meshMode = MESH_TRIANGLE_STRIP_ADJ;
      break;
    };
    default:    // points, lines, patchlists, unknown
    {
      cdata->meshMode = MESH_OTHER;
      isTriangleMesh = false;
    };
  }

  // line/point data
  cdata->unproject = cfg.position.unproject;
  cdata->mvp = cfg.position.unproject ? pickMVPProj : pickMVP;
  cdata->coords = Vec2f((float)x, (float)y);
  cdata->viewport = Vec2f(DebugData.outWidth, DebugData.outHeight);

  gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  GLuint ib = 0;

  if(cfg.position.idxByteWidth && cfg.position.idxbuf != ResourceId())
    ib = m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.idxbuf).name;

  // We copy into our own buffers to promote to the target type (uint32) that the
  // shader expects. Most IBs will be 16-bit indices, most VBs will not be float4.

  if(ib)
  {
    // resize up on demand
    if(DebugData.pickIBBuf == 0 || DebugData.pickIBSize < cfg.position.numVerts * sizeof(uint32_t))
    {
      gl.glDeleteBuffers(1, &DebugData.pickIBBuf);

      gl.glGenBuffers(1, &DebugData.pickIBBuf);
      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glNamedBufferDataEXT(DebugData.pickIBBuf, cfg.position.numVerts * sizeof(uint32_t), NULL,
                              eGL_STREAM_DRAW);

      DebugData.pickIBSize = cfg.position.numVerts * sizeof(uint32_t);
    }

    byte *idxs = new byte[cfg.position.numVerts * cfg.position.idxByteWidth];
    memset(idxs, 0, cfg.position.numVerts * cfg.position.idxByteWidth);
    uint32_t *outidxs = NULL;

    if(cfg.position.idxByteWidth < 4)
      outidxs = new uint32_t[cfg.position.numVerts];

    gl.glBindBuffer(eGL_COPY_READ_BUFFER, ib);

    GLint bufsize = 0;
    gl.glGetBufferParameteriv(eGL_COPY_READ_BUFFER, eGL_BUFFER_SIZE, &bufsize);

    gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)cfg.position.idxoffs,
                          RDCMIN(uint32_t(bufsize) - uint32_t(cfg.position.idxoffs),
                                 cfg.position.numVerts * cfg.position.idxByteWidth),
                          idxs);

    uint16_t *idxs16 = (uint16_t *)idxs;

    if(cfg.position.idxByteWidth == 1)
    {
      for(uint32_t i = 0; i < cfg.position.numVerts; i++)
        outidxs[i] = idxs[i];

      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numVerts * sizeof(uint32_t),
                         outidxs);
    }
    else if(cfg.position.idxByteWidth == 2)
    {
      for(uint32_t i = 0; i < cfg.position.numVerts; i++)
        outidxs[i] = idxs16[i];

      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numVerts * sizeof(uint32_t),
                         outidxs);
    }
    else
    {
      gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numVerts * sizeof(uint32_t),
                         idxs);
    }

    SAFE_DELETE_ARRAY(outidxs);
  }

  if(DebugData.pickVBBuf == 0 || DebugData.pickVBSize < cfg.position.numVerts * sizeof(Vec4f))
  {
    gl.glDeleteBuffers(1, &DebugData.pickVBBuf);

    gl.glGenBuffers(1, &DebugData.pickVBBuf);
    gl.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickVBBuf);
    gl.glNamedBufferDataEXT(DebugData.pickVBBuf, cfg.position.numVerts * sizeof(Vec4f), NULL,
                            eGL_DYNAMIC_DRAW);

    DebugData.pickVBSize = cfg.position.numVerts * sizeof(Vec4f);
  }

  // unpack and linearise the data
  {
    FloatVector *vbData = new FloatVector[cfg.position.numVerts];

    vector<byte> oldData;
    GetBufferData(cfg.position.buf, cfg.position.offset, 0, oldData);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    uint32_t idxclamp = 0;
    if(cfg.position.baseVertex < 0)
      idxclamp = uint32_t(-cfg.position.baseVertex);

    for(uint32_t i = 0; i < cfg.position.numVerts; i++)
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
    gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, cfg.position.numVerts * sizeof(Vec4f), vbData);

    delete[] vbData;
  }

  uint32_t reset[4] = {};
  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.pickResultBuf);
  gl.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t) * 4, &reset);

  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 1, DebugData.pickVBBuf);
  gl.glBindBufferRange(
      eGL_SHADER_STORAGE_BUFFER, 2, DebugData.pickIBBuf, (GLintptr)cfg.position.idxoffs,
      (GLsizeiptr)(cfg.position.idxoffs + sizeof(uint32_t) * cfg.position.numVerts));
  gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 3, DebugData.pickResultBuf);

  gl.glDispatchCompute(GLuint((cfg.position.numVerts) / 128 + 1), 1, 1);
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

  texDisplay.Red = texDisplay.Green = texDisplay.Blue = texDisplay.Alpha = true;
  texDisplay.FlipY = false;
  texDisplay.HDRMul = -1.0f;
  texDisplay.linearDisplayAsGamma = true;
  texDisplay.mip = mip;
  texDisplay.sampleIdx = sample;
  texDisplay.CustomShader = ResourceId();
  texDisplay.sliceFace = sliceFace;
  texDisplay.rangemin = 0.0f;
  texDisplay.rangemax = 1.0f;
  texDisplay.scale = 1.0f;
  texDisplay.texid = texture;
  texDisplay.typeHint = typeHint;
  texDisplay.rawoutput = true;
  texDisplay.offx = -float(x);
  texDisplay.offy = -float(y);

  RenderTextureInternal(texDisplay, eTexDisplay_MipShift);

  gl.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixel);

  if(!HasExt[ARB_gpu_shader5])
  {
    auto &texDetails = m_pDriver->m_Textures[texDisplay.texid];

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
      texDisplay.Red = texDisplay.Blue = texDisplay.Alpha = false;

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

void GLReplay::CopyTex2DMSToArray(GLuint destArray, GLuint srcMS, GLint width, GLint height,
                                  GLint arraySize, GLint samples, GLenum intFormat)
{
  WrappedOpenGL &gl = *m_pDriver;

  if(!HasExt[ARB_compute_shader])
    return;

  if(!HasExt[ARB_texture_view])
  {
    RDCWARN("Can't copy multisampled texture to array for serialisation without ARB_texture_view.");
    return;
  }

  GLRenderState rs(&gl.GetHookset(), NULL, READING);
  rs.FetchState(m_pDriver->GetCtx(), m_pDriver);

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

  rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);
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

  auto &texDetails = m_pDriver->m_Textures[cfg.texid];

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
      cfg.Red = false;
      cfg.Green = true;
      cfg.Blue = false;
      cfg.Alpha = false;
    }

    // depth-only, make sure we display it as such
    if(GetBaseFormat(texDetails.internalFormat) == eGL_DEPTH_COMPONENT)
    {
      cfg.Red = true;
      cfg.Green = false;
      cfg.Blue = false;
      cfg.Alpha = false;
    }

    if(!cfg.Red && cfg.Green)
    {
      dsTexMode = eGL_STENCIL_INDEX;

      // Stencil texture sampling is not normalized in OpenGL
      intIdx = 1;
      float rangeScale;
      switch(texDetails.internalFormat)
      {
        case eGL_STENCIL_INDEX1: rangeScale = 1.0f; break;
        case eGL_STENCIL_INDEX4: rangeScale = 16.0f; break;
        default:
          RDCWARN("Unexpected raw format for stencil visualization");
        // fall through
        case eGL_DEPTH24_STENCIL8:
        case eGL_DEPTH32F_STENCIL8:
        case eGL_STENCIL_INDEX8: rangeScale = 256.0f; break;
        case eGL_STENCIL_INDEX16: rangeScale = 65536.0f; break;
      }
      cfg.rangemin *= rangeScale;
      cfg.rangemax *= rangeScale;
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

  if(cfg.CustomShader != ResourceId() && gl.GetResourceManager()->HasCurrentResource(cfg.CustomShader))
  {
    GLuint customProg = gl.GetResourceManager()->GetCurrentResource(cfg.CustomShader).name;
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

  if(cfg.texid != DebugData.CustomShaderTexID)
    clampmaxlevel[0] = GLint(numMips - 1);

  gl.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);

  // need to ensure texture is mipmap complete by clamping TEXTURE_MAX_LEVEL.
  if(clampmaxlevel[0] != maxlevel[0] && cfg.texid != DebugData.CustomShaderTexID)
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

  float x = cfg.offx;
  float y = cfg.offy;

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

  ubo->HDRMul = cfg.HDRMul;

  ubo->FlipY = cfg.FlipY ? 1 : 0;

  if(cfg.rangemax <= cfg.rangemin)
    cfg.rangemax += 0.00001f;

  if(dsTexMode == eGL_NONE)
  {
    ubo->Channels.x = cfg.Red ? 1.0f : 0.0f;
    ubo->Channels.y = cfg.Green ? 1.0f : 0.0f;
    ubo->Channels.z = cfg.Blue ? 1.0f : 0.0f;
    ubo->Channels.w = cfg.Alpha ? 1.0f : 0.0f;
  }
  else
  {
    // Both depth and stencil texture mode use the red channel
    ubo->Channels.x = 1.0f;
    ubo->Channels.y = 0.0f;
    ubo->Channels.z = 0.0f;
    ubo->Channels.w = 0.0f;
  }

  ubo->RangeMinimum = cfg.rangemin;
  ubo->InverseRangeSize = 1.0f / (cfg.rangemax - cfg.rangemin);

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

  ubo->RawOutput = cfg.rawoutput ? 1 : 0;

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

  if(cfg.rawoutput || !blendAlpha)
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

void GLReplay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &gl = *m_pDriver;

  gl.glUseProgram(DebugData.checkerProg);

  gl.glDisable(eGL_DEPTH_TEST);

  gl.glEnable(eGL_FRAMEBUFFER_SRGB);

  gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  Vec4f *ubo = (Vec4f *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(Vec4f) * 2,
                                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  ubo[0] = Vec4f(light.x, light.y, light.z, 1.0f);
  ubo[1] = Vec4f(dark.x, dark.y, dark.z, 1.0f);

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
                                   uint32_t eventID, const vector<uint32_t> &passEvents)
{
  WrappedOpenGL &gl = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLMarkerRegion renderoverlay(StringFormat::Fmt("RenderOverlay %d", overlay));

  void *ctx = m_ReplayCtx.ctx;

  GLRenderState rs(&gl.GetHookset(), NULL, READING);
  rs.FetchState(ctx, &gl);

  // use our overlay pipeline that we'll fill up with all the right
  // shaders, then replace the fragment shader with our own.
  gl.glUseProgram(0);
  gl.glBindProgramPipeline(DebugData.overlayPipe);

  // we bind the separable program created for each shader, and copy
  // uniforms and attrib bindings from the 'real' programs, wherever
  // they are.
  SetupOverlayPipeline(rs.Program, rs.Pipeline, DebugData.fixedcolFSProg);

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
      GLint internalFormat = eGL_RGBA16;
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

    ReplayLog(eventID, eReplay_OnlyDraw);
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

    ReplayLog(eventID, eReplay_OnlyDraw);
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

    ReplayLog(eventID, eReplay_OnlyDraw);

    GLuint curDepth = 0, curStencil = 0;

    gl.glGetNamedFramebufferAttachmentParameterivEXT(
        rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint *)&curDepth);
    gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO, eGL_STENCIL_ATTACHMENT,
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
          rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

      GLenum fmt;

      if(type != eGL_RENDERBUFFER)
      {
        ResourceId id = m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, curDepth));
        WrappedOpenGL::TextureData &details = m_pDriver->m_Textures[id];

        fmt = details.internalFormat;

        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

        if(details.curType == eGL_TEXTURE_CUBE_MAP)
        {
          GLenum face;
          gl.glGetNamedFramebufferAttachmentParameterivEXT(
              rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
              (GLint *)&face);

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
          rs.DrawFBO, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

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
              rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
              (GLint *)&face);

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
    gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, rs.DrawFBO);

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

    ReplayLog(eventID, eReplay_OnlyDraw);

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

    ReplayLog(eventID, eReplay_OnlyDraw);

    // only enable cull face if it was enabled originally (otherwise
    // we just render green over the exact same area, so it shows up "passing")
    if(rs.Enabled[GLRenderState::eEnabled_CullFace])
      gl.glEnable(eGL_CULL_FACE);

    col[0] = 0.0f;
    col[1] = 1.0f;

    gl.glProgramUniform4fv(DebugData.fixedcolFSProg, colLoc, 1, col);

    ReplayLog(eventID, eReplay_OnlyDraw);
  }
  else if(overlay == DebugOverlay::ClearBeforeDraw || overlay == DebugOverlay::ClearBeforePass)
  {
    float col[] = {0.0f, 0.0f, 0.0f, 0.0f};
    gl.glClearBufferfv(eGL_COLOR, 0, col);

    vector<uint32_t> events = passEvents;

    if(overlay == DebugOverlay::ClearBeforeDraw)
      events.clear();

    events.push_back(eventID);

    if(!events.empty())
    {
      if(overlay == DebugOverlay::ClearBeforePass)
      {
        m_pDriver->ReplayLog(0, events[0], eReplay_WithoutDraw);
      }
      else
      {
        // if we don't replay the real state, restore what we've changed
        rs.ApplyState(ctx, &gl);
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

    events.push_back(eventID);

    if(!events.empty() && DebugData.trisizeProg)
    {
      if(overlay == DebugOverlay::TriangleSizePass)
        ReplayLog(events[0], eReplay_WithoutDraw);
      else
        rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);

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
          MeshFormat fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::GSOut);
          if(fmt.buf == ResourceId())
            fmt = GetPostVSBuffers(events[i], inst, MeshDataStage::VSOut);

          if(fmt.buf != ResourceId())
          {
            GLenum topo = MakeGLPrimitiveTopology(fmt.topo);

            gl.glBindVertexArray(tempVAO);

            {
              if(fmt.specialFormat != SpecialFormat::Unknown)
              {
                if(fmt.specialFormat == SpecialFormat::R10G10B10A2)
                {
                  if(fmt.compType == CompType::UInt)
                    gl.glVertexAttribIFormat(0, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
                  if(fmt.compType == CompType::SInt)
                    gl.glVertexAttribIFormat(0, 4, eGL_INT_2_10_10_10_REV, 0);
                }
                else if(fmt.specialFormat == SpecialFormat::R11G11B10)
                {
                  gl.glVertexAttribFormat(0, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
                }
                else
                {
                  RDCWARN("Unsupported special vertex attribute format: %x", fmt.specialFormat);
                }
              }
              else if(fmt.compType == CompType::Float || fmt.compType == CompType::UNorm ||
                      fmt.compType == CompType::SNorm)
              {
                GLenum fmttype = eGL_UNSIGNED_INT;

                if(fmt.compByteWidth == 4)
                {
                  if(fmt.compType == CompType::Float)
                    fmttype = eGL_FLOAT;
                  else if(fmt.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_INT;
                  else if(fmt.compType == CompType::SNorm)
                    fmttype = eGL_INT;
                }
                else if(fmt.compByteWidth == 2)
                {
                  if(fmt.compType == CompType::Float)
                    fmttype = eGL_HALF_FLOAT;
                  else if(fmt.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_SHORT;
                  else if(fmt.compType == CompType::SNorm)
                    fmttype = eGL_SHORT;
                }
                else if(fmt.compByteWidth == 1)
                {
                  if(fmt.compType == CompType::UNorm)
                    fmttype = eGL_UNSIGNED_BYTE;
                  else if(fmt.compType == CompType::SNorm)
                    fmttype = eGL_BYTE;
                }

                gl.glVertexAttribFormat(0, fmt.compCount, fmttype, fmt.compType != CompType::Float,
                                        0);
              }
              else if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt)
              {
                GLenum fmttype = eGL_UNSIGNED_INT;

                if(fmt.compByteWidth == 4)
                {
                  if(fmt.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_INT;
                  else if(fmt.compType == CompType::SInt)
                    fmttype = eGL_INT;
                }
                else if(fmt.compByteWidth == 2)
                {
                  if(fmt.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_SHORT;
                  else if(fmt.compType == CompType::SInt)
                    fmttype = eGL_SHORT;
                }
                else if(fmt.compByteWidth == 1)
                {
                  if(fmt.compType == CompType::UInt)
                    fmttype = eGL_UNSIGNED_BYTE;
                  else if(fmt.compType == CompType::SInt)
                    fmttype = eGL_BYTE;
                }

                gl.glVertexAttribIFormat(0, fmt.compCount, fmttype, 0);
              }
              else if(fmt.compType == CompType::Double)
              {
                gl.glVertexAttribLFormat(0, fmt.compCount, eGL_DOUBLE, 0);
              }

              GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.buf).name;
              gl.glBindVertexBuffer(0, vb, (GLintptr)fmt.offset, fmt.stride);
            }

            gl.glEnableVertexAttribArray(0);
            gl.glDisableVertexAttribArray(1);

            if(fmt.idxbuf != ResourceId())
            {
              GLenum idxtype = eGL_UNSIGNED_BYTE;
              if(fmt.idxByteWidth == 2)
                idxtype = eGL_UNSIGNED_SHORT;
              else if(fmt.idxByteWidth == 4)
                idxtype = eGL_UNSIGNED_INT;

              GLuint ib = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.idxbuf).name;
              gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
              gl.glDrawElementsBaseVertex(topo, fmt.numVerts, idxtype,
                                          (const void *)uintptr_t(fmt.idxoffs), fmt.baseVertex);
            }
            else
            {
              gl.glDrawArrays(topo, 0, fmt.numVerts);
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
        ReplayLog(eventID, eReplay_WithoutDraw);
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

      events.push_back(eventID);

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

        gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                         (GLint *)&curDepth);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO, eGL_DEPTH_ATTACHMENT,
                                                         eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                         (GLint *)&depthType);
        gl.glGetNamedFramebufferAttachmentParameterivEXT(
            rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

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
        gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, quadtexs[1], 0);

        if(overlay == DebugOverlay::QuadOverdrawPass)
          ReplayLog(events[0], eReplay_WithoutDraw);
        else
          rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);

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
          ReplayLog(eventID, eReplay_WithoutDraw);
      }
    }
  }
  else
  {
    RDCERR(
        "Unexpected/unimplemented overlay type - should implement a placeholder overlay for all "
        "types");
  }

  rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);

  return m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, DebugData.overlayTex));
}

void GLReplay::InitPostVSBuffers(uint32_t eventID)
{
  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    return;

  MakeCurrentReplayContext(&m_ReplayCtx);

  void *ctx = m_ReplayCtx.ctx;

  WrappedOpenGL &gl = *m_pDriver;
  GLResourceManager *rm = m_pDriver->GetResourceManager();

  GLRenderState rs(&gl.GetHookset(), NULL, READING);
  rs.FetchState(ctx, &gl);
  GLuint elArrayBuffer = 0;
  if(rs.VAO)
    gl.glGetIntegerv(eGL_ELEMENT_ARRAY_BUFFER_BINDING, (GLint *)&elArrayBuffer);

  // reflection structures
  ShaderReflection *vsRefl = NULL;
  ShaderReflection *tesRefl = NULL;
  ShaderReflection *gsRefl = NULL;

  // non-program used separable programs of each shader.
  // we'll add our feedback varings to these programs, relink,
  // and combine into a pipeline for use.
  GLuint vsProg = 0;
  GLuint tcsProg = 0;
  GLuint tesProg = 0;
  GLuint gsProg = 0;

  // these are the 'real' programs with uniform values that we need
  // to copy over to our separable programs.
  GLuint vsProgSrc = 0;
  GLuint tcsProgSrc = 0;
  GLuint tesProgSrc = 0;
  GLuint gsProgSrc = 0;

  if(rs.Program == 0)
  {
    if(rs.Pipeline == 0)
    {
      return;
    }
    else
    {
      ResourceId id = rm->GetID(ProgramPipeRes(ctx, rs.Pipeline));
      auto &pipeDetails = m_pDriver->m_Pipelines[id];

      if(pipeDetails.stageShaders[0] != ResourceId())
      {
        vsRefl = GetShader(pipeDetails.stageShaders[0], "");
        vsProg = m_pDriver->m_Shaders[pipeDetails.stageShaders[0]].prog;
        vsProgSrc = rm->GetCurrentResource(pipeDetails.stagePrograms[0]).name;
      }
      if(pipeDetails.stageShaders[1] != ResourceId())
      {
        tcsProg = m_pDriver->m_Shaders[pipeDetails.stageShaders[1]].prog;
        tcsProgSrc = rm->GetCurrentResource(pipeDetails.stagePrograms[1]).name;
      }
      if(pipeDetails.stageShaders[2] != ResourceId())
      {
        tesRefl = GetShader(pipeDetails.stageShaders[2], "");
        tesProg = m_pDriver->m_Shaders[pipeDetails.stageShaders[2]].prog;
        tesProgSrc = rm->GetCurrentResource(pipeDetails.stagePrograms[2]).name;
      }
      if(pipeDetails.stageShaders[3] != ResourceId())
      {
        gsRefl = GetShader(pipeDetails.stageShaders[3], "");
        gsProg = m_pDriver->m_Shaders[pipeDetails.stageShaders[3]].prog;
        gsProgSrc = rm->GetCurrentResource(pipeDetails.stagePrograms[3]).name;
      }
    }
  }
  else
  {
    auto &progDetails = m_pDriver->m_Programs[rm->GetID(ProgramRes(ctx, rs.Program))];

    if(progDetails.stageShaders[0] != ResourceId())
    {
      vsRefl = GetShader(progDetails.stageShaders[0], "");
      vsProg = m_pDriver->m_Shaders[progDetails.stageShaders[0]].prog;
    }
    if(progDetails.stageShaders[1] != ResourceId())
    {
      tcsProg = m_pDriver->m_Shaders[progDetails.stageShaders[1]].prog;
    }
    if(progDetails.stageShaders[2] != ResourceId())
    {
      tesRefl = GetShader(progDetails.stageShaders[2], "");
      tesProg = m_pDriver->m_Shaders[progDetails.stageShaders[2]].prog;
    }
    if(progDetails.stageShaders[3] != ResourceId())
    {
      gsRefl = GetShader(progDetails.stageShaders[3], "");
      gsProg = m_pDriver->m_Shaders[progDetails.stageShaders[3]].prog;
    }

    vsProgSrc = tcsProgSrc = tesProgSrc = gsProgSrc = rs.Program;
  }

  if(vsRefl == NULL)
  {
    // no vertex shader bound (no vertex processing - compute only program
    // or no program bound, for a clear etc)
    m_PostVSData[eventID] = GLPostVSData();
    return;
  }

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventID);

  if(drawcall->numIndices == 0)
  {
    // draw is 0 length, nothing to do
    m_PostVSData[eventID] = GLPostVSData();
    return;
  }

  list<string> matrixVaryings;    // matrices need some fixup
  vector<const char *> varyings;

  // we don't want to do any work, so just discard before rasterizing
  gl.glEnable(eGL_RASTERIZER_DISCARD);

  CopyProgramAttribBindings(gl.GetHookset(), vsProgSrc, vsProg, vsRefl);

  varyings.clear();

  uint32_t stride = 0;
  int32_t posidx = -1;

  for(int32_t i = 0; i < vsRefl->OutputSig.count; i++)
  {
    const char *name = vsRefl->OutputSig[i].varName.elems;
    int32_t len = vsRefl->OutputSig[i].varName.count;

    bool include = true;

    // for matrices with names including :row1, :row2 etc we only include :row0
    // as a varying (but increment the stride for all rows to account for the space)
    // and modify the name to remove the :row0 part
    const char *colon = strchr(name, ':');
    if(colon)
    {
      if(name[len - 1] != '0')
      {
        include = false;
      }
      else
      {
        matrixVaryings.push_back(string(name, colon));
        name = matrixVaryings.back().c_str();
      }
    }

    if(include)
      varyings.push_back(name);

    if(vsRefl->OutputSig[i].systemValue == ShaderBuiltin::Position)
      posidx = int32_t(varyings.size()) - 1;

    stride += sizeof(float) * vsRefl->OutputSig[i].compCount;
  }

  // shift position attribute up to first, keeping order otherwise
  // the same
  if(posidx > 0)
  {
    const char *pos = varyings[posidx];
    varyings.erase(varyings.begin() + posidx);
    varyings.insert(varyings.begin(), pos);
  }

  // this is REALLY ugly, but I've seen problems with varying specification, so we try and
  // do some fixup by removing prefixes from the results we got from PROGRAM_OUTPUT.
  //
  // the problem I've seen is:
  //
  // struct vertex
  // {
  //   vec4 Color;
  // };
  //
  // layout(location = 0) out vertex Out;
  //
  // (from g_truc gl-410-primitive-tessellation-2). On AMD the varyings are what you might expect
  // (from
  // the PROGRAM_OUTPUT interface names reflected out): "Out.Color", "gl_Position"
  // however nvidia complains unless you use "Color", "gl_Position". This holds even if you add
  // other
  // variables to the vertex struct.
  //
  // strangely another sample that in-lines the output block like so:
  //
  // out block
  // {
  //   vec2 Texcoord;
  // } Out;
  //
  // uses "block.Texcoord" (reflected name from PROGRAM_OUTPUT and accepted by varyings string on
  // both
  // vendors). This is inconsistent as it's type.member not structname.member as move.
  //
  // The spec is very vague on exactly what these names should be, so I can't say which is correct
  // out of these three possibilities.
  //
  // So our 'fix' is to loop while we have problems linking with the varyings (since we know
  // otherwise
  // linking should succeed, as we only get here with a successfully linked separable program - if
  // it fails
  // to link, it's assigned 0 earlier) and remove any prefixes from variables seen in the link error
  // string.
  // The error string is something like:
  // "error: Varying (named Out.Color) specified but not present in the program object."
  //
  // Yeh. Ugly. Not guaranteed to work at all, but hopefully the common case will just be a single
  // block
  // without any nesting so this might work.
  // At least we don't have to reallocate strings all over, since the memory is
  // already owned elsewhere, we just need to modify pointers to trim prefixes. Bright side?

  GLint status = 0;
  bool finished = false;
  for(;;)
  {
    // specify current varyings & relink
    gl.glTransformFeedbackVaryings(vsProg, (GLsizei)varyings.size(), &varyings[0],
                                   eGL_INTERLEAVED_ATTRIBS);
    gl.glLinkProgram(vsProg);

    gl.glGetProgramiv(vsProg, eGL_LINK_STATUS, &status);

    // all good! Hopefully we'll mostly hit this
    if(status == 1)
      break;

    // if finished is true, this was our last attempt - there are no
    // more fixups possible
    if(finished)
      break;

    char buffer[1025] = {0};
    gl.glGetProgramInfoLog(vsProg, 1024, NULL, buffer);

    // assume we're finished and can't retry any more after this.
    // if we find a potential 'fixup' we'll set this back to false
    finished = true;

    // see if any of our current varyings are present in the buffer string
    for(size_t i = 0; i < varyings.size(); i++)
    {
      if(strstr(buffer, varyings[i]))
      {
        const char *prefix_removed = strchr(varyings[i], '.');

        // does it contain a prefix?
        if(prefix_removed)
        {
          prefix_removed++;    // now this is our string without the prefix

          // first check this won't cause a duplicate - if it does, we have to try something else
          bool duplicate = false;
          for(size_t j = 0; j < varyings.size(); j++)
          {
            if(!strcmp(varyings[j], prefix_removed))
            {
              duplicate = true;
              break;
            }
          }

          if(!duplicate)
          {
            // we'll attempt this fixup
            RDCWARN("Attempting XFB varying fixup, subst '%s' for '%s'", varyings[i], prefix_removed);
            varyings[i] = prefix_removed;
            finished = false;

            // don't try more than one at once (just in case)
            break;
          }
        }
      }
    }
  }

  if(status == 0)
  {
    char buffer[1025] = {0};
    gl.glGetProgramInfoLog(vsProg, 1024, NULL, buffer);
    RDCERR("Failed to fix-up. Link error making xfb vs program: %s", buffer);
    m_PostVSData[eventID] = GLPostVSData();
    return;
  }

  // make a pipeline to contain just the vertex shader
  GLuint vsFeedbackPipe = 0;
  gl.glGenProgramPipelines(1, &vsFeedbackPipe);

  // bind the separable vertex program to it
  gl.glUseProgramStages(vsFeedbackPipe, eGL_VERTEX_SHADER_BIT, vsProg);

  // copy across any uniform values, bindings etc from the real program containing
  // the vertex stage
  CopyProgramUniforms(gl.GetHookset(), vsProgSrc, vsProg);

  // bind our program and do the feedback draw
  gl.glUseProgram(0);
  gl.glBindProgramPipeline(vsFeedbackPipe);

  gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);

  GLuint idxBuf = 0;

  if(!(drawcall->flags & DrawFlags::UseIBuffer))
  {
    uint32_t outputSize = drawcall->numIndices * drawcall->numInstances * stride;

    if(drawcall->flags & DrawFlags::Instanced)
      outputSize *= drawcall->numInstances;

    // resize up the buffer if needed for the vertex output data
    if(DebugData.feedbackBufferSize < outputSize)
    {
      uint32_t oldSize = DebugData.feedbackBufferSize;
      while(DebugData.feedbackBufferSize < outputSize)
        DebugData.feedbackBufferSize *= 2;
      RDCWARN("Resizing xfb buffer from %u to %u for output", oldSize, DebugData.feedbackBufferSize);
      gl.glNamedBufferDataEXT(DebugData.feedbackBuffer, DebugData.feedbackBufferSize, NULL,
                              eGL_DYNAMIC_READ);
    }

    // need to rebind this here because of an AMD bug that seems to ignore the buffer
    // bindings in the feedback object - or at least it errors if the default feedback
    // object has no buffers bound. Fortunately the state is still object-local so
    // we don't have to restore the buffer binding on the default feedback object.
    gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

    gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
    gl.glBeginTransformFeedback(eGL_POINTS);

    if(drawcall->flags & DrawFlags::Instanced)
    {
      if(HasExt[ARB_base_instance])
      {
        gl.glDrawArraysInstancedBaseInstance(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices,
                                             drawcall->numInstances, drawcall->instanceOffset);
      }
      else
      {
        gl.glDrawArraysInstanced(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices,
                                 drawcall->numInstances);
      }
    }
    else
    {
      gl.glDrawArrays(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices);
    }
  }
  else    // drawcall is indexed
  {
    ResourceId idxId = rm->GetID(BufferRes(NULL, elArrayBuffer));

    vector<byte> idxdata;
    GetBufferData(idxId, drawcall->indexOffset * drawcall->indexByteWidth,
                  drawcall->numIndices * drawcall->indexByteWidth, idxdata);

    vector<uint32_t> indices;

    uint8_t *idx8 = (uint8_t *)&idxdata[0];
    uint16_t *idx16 = (uint16_t *)&idxdata[0];
    uint32_t *idx32 = (uint32_t *)&idxdata[0];

    // only read as many indices as were available in the buffer
    uint32_t numIndices =
        RDCMIN(uint32_t(idxdata.size() / drawcall->indexByteWidth), drawcall->numIndices);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = 0;
      if(drawcall->indexByteWidth == 1)
        i32 = uint32_t(idx8[i]);
      else if(drawcall->indexByteWidth == 2)
        i32 = uint32_t(idx16[i]);
      else if(drawcall->indexByteWidth == 4)
        i32 = idx32[i];

      auto it = std::lower_bound(indices.begin(), indices.end(), i32);

      if(it != indices.end() && *it == i32)
        continue;

      indices.insert(it, i32);
    }

    // if we read out of bounds, we'll also have a 0 index being referenced
    // (as 0 is read). Don't insert 0 if we already have 0 though
    if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
      indices.insert(indices.begin(), 0);

    // An index buffer could be something like: 500, 501, 502, 501, 503, 502
    // in which case we can't use the existing index buffer without filling 499 slots of vertex
    // data with padding. Instead we rebase the indices based on the smallest vertex so it becomes
    // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
    //
    // Note that there could also be gaps, like: 500, 501, 502, 510, 511, 512
    // which would become 0, 1, 2, 3, 4, 5 and so the old index buffer would no longer be valid.
    // We just stream-out a tightly packed list of unique indices, and then remap the index buffer
    // so that what did point to 500 points to 0 (accounting for rebasing), and what did point
    // to 510 now points to 3 (accounting for the unique sort).

    // we use a map here since the indices may be sparse. Especially considering if an index
    // is 'invalid' like 0xcccccccc then we don't want an array of 3.4 billion entries.
    map<uint32_t, size_t> indexRemap;
    for(size_t i = 0; i < indices.size(); i++)
    {
      // by definition, this index will only appear once in indices[]
      indexRemap[indices[i]] = i;
    }

    // generate a temporary index buffer with our 'unique index set' indices,
    // so we can transform feedback each referenced vertex once
    GLuint indexSetBuffer = 0;
    gl.glGenBuffers(1, &indexSetBuffer);
    gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, indexSetBuffer);
    gl.glNamedBufferDataEXT(indexSetBuffer, sizeof(uint32_t) * indices.size(), &indices[0],
                            eGL_STATIC_DRAW);

    uint32_t outputSize = (uint32_t)indices.size() * drawcall->numInstances * stride;

    if(drawcall->flags & DrawFlags::Instanced)
      outputSize *= drawcall->numInstances;

    // resize up the buffer if needed for the vertex output data
    if(DebugData.feedbackBufferSize < outputSize)
    {
      uint32_t oldSize = DebugData.feedbackBufferSize;
      while(DebugData.feedbackBufferSize < outputSize)
        DebugData.feedbackBufferSize *= 2;
      RDCWARN("Resizing xfb buffer from %u to %u for output", oldSize, DebugData.feedbackBufferSize);
      gl.glNamedBufferDataEXT(DebugData.feedbackBuffer, DebugData.feedbackBufferSize, NULL,
                              eGL_DYNAMIC_READ);
    }

    // need to rebind this here because of an AMD bug that seems to ignore the buffer
    // bindings in the feedback object - or at least it errors if the default feedback
    // object has no buffers bound. Fortunately the state is still object-local so
    // we don't have to restore the buffer binding on the default feedback object.
    gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

    gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
    gl.glBeginTransformFeedback(eGL_POINTS);

    if(drawcall->flags & DrawFlags::Instanced)
    {
      if(HasExt[ARB_base_instance])
      {
        gl.glDrawElementsInstancedBaseVertexBaseInstance(
            eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL, drawcall->numInstances,
            drawcall->baseVertex, drawcall->instanceOffset);
      }
      else
      {
        gl.glDrawElementsInstancedBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT,
                                             NULL, drawcall->numInstances, drawcall->baseVertex);
      }
    }
    else
    {
      gl.glDrawElementsBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL,
                                  drawcall->baseVertex);
    }

    // delete the buffer, we don't need it anymore
    gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
    gl.glDeleteBuffers(1, &indexSetBuffer);

    uint32_t stripRestartValue32 = 0;

    if(IsStrip(drawcall->topology) && rs.Enabled[GLRenderState::eEnabled_PrimitiveRestart])
    {
      stripRestartValue32 = rs.Enabled[GLRenderState::eEnabled_PrimitiveRestartFixedIndex]
                                ? ~0U
                                : rs.PrimitiveRestartIndex;
    }

    // rebase existing index buffer to point from 0 onwards (which will index into our
    // stream-out'd vertex buffer)
    if(drawcall->indexByteWidth == 1)
    {
      uint8_t stripRestartValue = stripRestartValue32 & 0xff;

      for(uint32_t i = 0; i < numIndices; i++)
      {
        // preserve primitive restart indices
        if(stripRestartValue && idx8[i] == stripRestartValue)
          continue;

        idx8[i] = uint8_t(indexRemap[idx8[i]]);
      }
    }
    else if(drawcall->indexByteWidth == 2)
    {
      uint16_t stripRestartValue = stripRestartValue32 & 0xffff;

      for(uint32_t i = 0; i < numIndices; i++)
      {
        // preserve primitive restart indices
        if(stripRestartValue && idx16[i] == stripRestartValue)
          continue;

        idx16[i] = uint16_t(indexRemap[idx16[i]]);
      }
    }
    else
    {
      uint32_t stripRestartValue = stripRestartValue32;

      for(uint32_t i = 0; i < numIndices; i++)
      {
        // preserve primitive restart indices
        if(stripRestartValue && idx32[i] == stripRestartValue)
          continue;

        idx32[i] = uint32_t(indexRemap[idx32[i]]);
      }
    }

    // make the index buffer that can be used to render this postvs data - the original
    // indices, repointed (since we transform feedback to the start of our feedback
    // buffer and only tightly packed unique indices).
    if(!idxdata.empty())
    {
      gl.glGenBuffers(1, &idxBuf);
      gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, idxBuf);
      gl.glNamedBufferDataEXT(idxBuf, (GLsizeiptr)idxdata.size(), &idxdata[0], eGL_STATIC_DRAW);
    }

    // restore previous element array buffer binding
    gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
  }

  gl.glEndTransformFeedback();
  gl.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

  bool error = false;

  // this should be the same as the draw size
  GLuint primsWritten = 0;
  gl.glGetQueryObjectuiv(DebugData.feedbackQueries[0], eGL_QUERY_RESULT, &primsWritten);

  if(primsWritten == 0)
  {
    // we bailed out much earlier if this was a draw of 0 verts
    RDCERR("No primitives written - but we must have had some number of vertices in the draw");
    error = true;
  }

  // get buffer data from buffer attached to feedback object
  float *data = (float *)gl.glMapNamedBufferEXT(DebugData.feedbackBuffer, eGL_READ_ONLY);

  if(data == NULL)
  {
    gl.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);
    RDCERR("Couldn't map feedback buffer!");
    error = true;
  }

  if(error)
  {
    // delete temporary pipelines we made
    gl.glDeleteProgramPipelines(1, &vsFeedbackPipe);

    // restore replay state we trashed
    gl.glUseProgram(rs.Program);
    gl.glBindProgramPipeline(rs.Pipeline);

    gl.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array]);
    gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

    gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj);

    if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
      gl.glDisable(eGL_RASTERIZER_DISCARD);
    else
      gl.glEnable(eGL_RASTERIZER_DISCARD);

    m_PostVSData[eventID] = GLPostVSData();
    return;
  }

  // create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
  // can render from it to display previews).
  GLuint vsoutBuffer = 0;
  gl.glGenBuffers(1, &vsoutBuffer);
  gl.glBindBuffer(eGL_ARRAY_BUFFER, vsoutBuffer);
  gl.glNamedBufferDataEXT(vsoutBuffer, stride * primsWritten, data, eGL_STATIC_DRAW);

  byte *byteData = (byte *)data;

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f *pos0 = (Vec4f *)byteData;

  bool found = false;

  for(GLuint i = 1; posidx != -1 && i < primsWritten; i++)
  {
    //////////////////////////////////////////////////////////////////////////////////
    // derive near/far, assuming a standard perspective matrix
    //
    // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
    // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
    // and we know Wpost = Zpre from the perspective matrix.
    // we can then see from the perspective matrix that
    // m = F/(F-N)
    // c = -(F*N)/(F-N)
    //
    // with re-arranging and substitution, we then get:
    // N = -c/m
    // F = c/(1-m)
    //
    // so if we can derive m and c then we can determine N and F. We can do this with
    // two points, and we pick them reasonably distinct on z to reduce floating-point
    // error

    Vec4f *pos = (Vec4f *)(byteData + i * stride);

    if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
    {
      Vec2f A(pos0->w, pos0->z);
      Vec2f B(pos->w, pos->z);

      float m = (B.y - A.y) / (B.x - A.x);
      float c = B.y - B.x * m;

      if(m == 1.0f)
        continue;

      nearp = -c / m;
      farp = c / (1 - m);

      found = true;

      break;
    }
  }

  // if we didn't find anything, all z's and w's were identical.
  // If the z is positive and w greater for the first element then
  // we detect this projection as reversed z with infinite far plane
  if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
  {
    nearp = pos0->z;
    farp = FLT_MAX;
  }

  gl.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);

  // store everything out to the PostVS data cache
  m_PostVSData[eventID].vsin.topo = drawcall->topology;
  m_PostVSData[eventID].vsout.buf = vsoutBuffer;
  m_PostVSData[eventID].vsout.vertStride = stride;
  m_PostVSData[eventID].vsout.nearPlane = nearp;
  m_PostVSData[eventID].vsout.farPlane = farp;

  m_PostVSData[eventID].vsout.useIndices = bool(drawcall->flags & DrawFlags::UseIBuffer);
  m_PostVSData[eventID].vsout.numVerts = drawcall->numIndices;

  m_PostVSData[eventID].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVSData[eventID].vsout.instStride =
        (stride * primsWritten) / RDCMAX(1U, drawcall->numInstances);

  m_PostVSData[eventID].vsout.idxBuf = 0;
  m_PostVSData[eventID].vsout.idxByteWidth = drawcall->indexByteWidth;
  if(m_PostVSData[eventID].vsout.useIndices && idxBuf)
  {
    m_PostVSData[eventID].vsout.idxBuf = idxBuf;
  }

  m_PostVSData[eventID].vsout.hasPosOut = posidx >= 0;

  m_PostVSData[eventID].vsout.topo = drawcall->topology;

  // set vsProg back to no varyings, for future use
  gl.glTransformFeedbackVaryings(vsProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
  gl.glLinkProgram(vsProg);

  GLuint lastFeedbackPipe = 0;

  if(tesProg || gsProg)
  {
    GLuint lastProg = gsProg;
    ShaderReflection *lastRefl = gsRefl;

    if(lastProg == 0)
    {
      lastProg = tesProg;
      lastRefl = tesRefl;
    }

    RDCASSERT(lastProg && lastRefl);

    varyings.clear();

    stride = 0;
    posidx = -1;

    for(int32_t i = 0; i < lastRefl->OutputSig.count; i++)
    {
      const char *name = lastRefl->OutputSig[i].varName.elems;
      int32_t len = lastRefl->OutputSig[i].varName.count;

      bool include = true;

      // for matrices with names including :row1, :row2 etc we only include :row0
      // as a varying (but increment the stride for all rows to account for the space)
      // and modify the name to remove the :row0 part
      const char *colon = strchr(name, ':');
      if(colon)
      {
        if(name[len - 1] != '0')
        {
          include = false;
        }
        else
        {
          matrixVaryings.push_back(string(name, colon));
          name = matrixVaryings.back().c_str();
        }
      }

      if(include)
        varyings.push_back(name);

      if(lastRefl->OutputSig[i].systemValue == ShaderBuiltin::Position)
        posidx = int32_t(varyings.size()) - 1;

      stride += sizeof(float) * lastRefl->OutputSig[i].compCount;
    }

    // shift position attribute up to first, keeping order otherwise
    // the same
    if(posidx > 0)
    {
      const char *pos = varyings[posidx];
      varyings.erase(varyings.begin() + posidx);
      varyings.insert(varyings.begin(), pos);
    }

    // see above for the justification/explanation of this monstrosity.

    status = 0;
    finished = false;
    for(;;)
    {
      // specify current varyings & relink
      gl.glTransformFeedbackVaryings(lastProg, (GLsizei)varyings.size(), &varyings[0],
                                     eGL_INTERLEAVED_ATTRIBS);
      gl.glLinkProgram(lastProg);

      gl.glGetProgramiv(lastProg, eGL_LINK_STATUS, &status);

      // all good! Hopefully we'll mostly hit this
      if(status == 1)
        break;

      // if finished is true, this was our last attempt - there are no
      // more fixups possible
      if(finished)
        break;

      char buffer[1025] = {0};
      gl.glGetProgramInfoLog(lastProg, 1024, NULL, buffer);

      // assume we're finished and can't retry any more after this.
      // if we find a potential 'fixup' we'll set this back to false
      finished = true;

      // see if any of our current varyings are present in the buffer string
      for(size_t i = 0; i < varyings.size(); i++)
      {
        if(strstr(buffer, varyings[i]))
        {
          const char *prefix_removed = strchr(varyings[i], '.');

          // does it contain a prefix?
          if(prefix_removed)
          {
            prefix_removed++;    // now this is our string without the prefix

            // first check this won't cause a duplicate - if it does, we have to try something else
            bool duplicate = false;
            for(size_t j = 0; j < varyings.size(); j++)
            {
              if(!strcmp(varyings[j], prefix_removed))
              {
                duplicate = true;
                break;
              }
            }

            if(!duplicate)
            {
              // we'll attempt this fixup
              RDCWARN("Attempting XFB varying fixup, subst '%s' for '%s'", varyings[i],
                      prefix_removed);
              varyings[i] = prefix_removed;
              finished = false;

              // don't try more than one at once (just in case)
              break;
            }
          }
        }
      }
    }

    if(status == 0)
    {
      char buffer[1025] = {0};
      gl.glGetProgramInfoLog(lastProg, 1024, NULL, buffer);
      RDCERR("Failed to fix-up. Link error making xfb last program: %s", buffer);
    }
    else
    {
      // make a pipeline to contain all the vertex processing shaders
      gl.glGenProgramPipelines(1, &lastFeedbackPipe);

      // bind the separable vertex program to it
      gl.glUseProgramStages(lastFeedbackPipe, eGL_VERTEX_SHADER_BIT, vsProg);

      // copy across any uniform values, bindings etc from the real program containing
      // the vertex stage
      CopyProgramUniforms(gl.GetHookset(), vsProgSrc, vsProg);

      // if tessellation is enabled, bind & copy uniforms. Note, control shader is optional
      // independent of eval shader (default values are used for the tessellation levels).
      if(tcsProg)
      {
        gl.glUseProgramStages(lastFeedbackPipe, eGL_TESS_CONTROL_SHADER_BIT, tcsProg);
        CopyProgramUniforms(gl.GetHookset(), tcsProgSrc, tcsProg);
      }
      if(tesProg)
      {
        gl.glUseProgramStages(lastFeedbackPipe, eGL_TESS_EVALUATION_SHADER_BIT, tesProg);
        CopyProgramUniforms(gl.GetHookset(), tesProgSrc, tesProg);
      }

      // if we have a geometry shader, bind & copy uniforms
      if(gsProg)
      {
        gl.glUseProgramStages(lastFeedbackPipe, eGL_GEOMETRY_SHADER_BIT, gsProg);
        CopyProgramUniforms(gl.GetHookset(), gsProgSrc, gsProg);
      }

      // bind our program and do the feedback draw
      gl.glUseProgram(0);
      gl.glBindProgramPipeline(lastFeedbackPipe);

      gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);

      // need to rebind this here because of an AMD bug that seems to ignore the buffer
      // bindings in the feedback object - or at least it errors if the default feedback
      // object has no buffers bound. Fortunately the state is still object-local so
      // we don't have to restore the buffer binding on the default feedback object.
      gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

      idxBuf = 0;

      GLenum shaderOutMode = eGL_TRIANGLES;
      GLenum lastOutTopo = eGL_TRIANGLES;

      uint32_t maxOutputSize = stride;

      if(drawcall->flags & DrawFlags::Instanced)
        maxOutputSize *= drawcall->numInstances;

      uint32_t numInputPrimitives = drawcall->numIndices;
      GLenum drawtopo = MakeGLPrimitiveTopology(drawcall->topology);

      switch(drawcall->topology)
      {
        case Topology::Unknown:
        case Topology::PointList: break;
        case Topology::LineList: numInputPrimitives /= 2; break;
        case Topology::LineStrip: numInputPrimitives -= 1; break;
        case Topology::LineLoop: break;
        case Topology::TriangleList: numInputPrimitives /= 3; break;
        case Topology::TriangleStrip:
        case Topology::TriangleFan: numInputPrimitives -= 2; break;
        case Topology::LineList_Adj: numInputPrimitives /= 4; break;
        case Topology::LineStrip_Adj: numInputPrimitives -= 3; break;
        case Topology::TriangleList_Adj: numInputPrimitives /= 6; break;
        case Topology::TriangleStrip_Adj: numInputPrimitives -= 5; break;
        case Topology::PatchList_1CPs:
        case Topology::PatchList_2CPs:
        case Topology::PatchList_3CPs:
        case Topology::PatchList_4CPs:
        case Topology::PatchList_5CPs:
        case Topology::PatchList_6CPs:
        case Topology::PatchList_7CPs:
        case Topology::PatchList_8CPs:
        case Topology::PatchList_9CPs:
        case Topology::PatchList_10CPs:
        case Topology::PatchList_11CPs:
        case Topology::PatchList_12CPs:
        case Topology::PatchList_13CPs:
        case Topology::PatchList_14CPs:
        case Topology::PatchList_15CPs:
        case Topology::PatchList_16CPs:
        case Topology::PatchList_17CPs:
        case Topology::PatchList_18CPs:
        case Topology::PatchList_19CPs:
        case Topology::PatchList_20CPs:
        case Topology::PatchList_21CPs:
        case Topology::PatchList_22CPs:
        case Topology::PatchList_23CPs:
        case Topology::PatchList_24CPs:
        case Topology::PatchList_25CPs:
        case Topology::PatchList_26CPs:
        case Topology::PatchList_27CPs:
        case Topology::PatchList_28CPs:
        case Topology::PatchList_29CPs:
        case Topology::PatchList_30CPs:
        case Topology::PatchList_31CPs:
        case Topology::PatchList_32CPs:
          numInputPrimitives /= PatchList_Count(drawcall->topology);
          break;
      }

      if(lastProg == gsProg)
      {
        gl.glGetProgramiv(gsProg, eGL_GEOMETRY_OUTPUT_TYPE, (GLint *)&shaderOutMode);

        GLint maxVerts = 1;

        gl.glGetProgramiv(gsProg, eGL_GEOMETRY_VERTICES_OUT, (GLint *)&maxVerts);

        if(shaderOutMode == eGL_TRIANGLE_STRIP)
        {
          lastOutTopo = eGL_TRIANGLES;
          maxVerts = RDCMAX(3, maxVerts);
        }
        else if(shaderOutMode == eGL_LINE_STRIP)
        {
          lastOutTopo = eGL_LINES;
          maxVerts = RDCMAX(2, maxVerts);
        }
        else if(shaderOutMode == eGL_POINTS)
        {
          lastOutTopo = eGL_POINTS;
          maxVerts = RDCMAX(1, maxVerts);
        }

        maxOutputSize *= maxVerts * numInputPrimitives;
      }
      else if(lastProg == tesProg)
      {
        gl.glGetProgramiv(tesProg, eGL_TESS_GEN_MODE, (GLint *)&shaderOutMode);

        uint32_t outputPrimitiveVerts = 1;

        if(shaderOutMode == eGL_QUADS)
        {
          lastOutTopo = eGL_TRIANGLES;
          outputPrimitiveVerts = 3;
        }
        else if(shaderOutMode == eGL_ISOLINES)
        {
          lastOutTopo = eGL_LINES;
          outputPrimitiveVerts = 2;
        }
        else if(shaderOutMode == eGL_TRIANGLES)
        {
          lastOutTopo = eGL_TRIANGLES;
          outputPrimitiveVerts = 3;
        }

        // assume an average maximum tessellation level of 32
        maxOutputSize *= 32 * outputPrimitiveVerts * numInputPrimitives;
      }

      // resize up the buffer if needed for the vertex output data
      if(DebugData.feedbackBufferSize < maxOutputSize)
      {
        uint32_t oldSize = DebugData.feedbackBufferSize;
        while(DebugData.feedbackBufferSize < maxOutputSize)
          DebugData.feedbackBufferSize *= 2;
        RDCWARN("Conservatively resizing xfb buffer from %u to %u for output", oldSize,
                DebugData.feedbackBufferSize);
        gl.glNamedBufferDataEXT(DebugData.feedbackBuffer, DebugData.feedbackBufferSize, NULL,
                                eGL_DYNAMIC_READ);
      }

      GLenum idxType = eGL_UNSIGNED_BYTE;
      if(drawcall->indexByteWidth == 2)
        idxType = eGL_UNSIGNED_SHORT;
      else if(drawcall->indexByteWidth == 4)
        idxType = eGL_UNSIGNED_INT;

      // instanced draws must be replayed one at a time so we can record the number of primitives
      // from
      // each drawcall, as due to expansion this can vary per-instance.
      if(drawcall->flags & DrawFlags::Instanced)
      {
        // if there is only one instance it's a trivial case and we don't need to bother with the
        // expensive path
        if(drawcall->numInstances > 1)
        {
          // ensure we have enough queries
          uint32_t curSize = (uint32_t)DebugData.feedbackQueries.size();
          if(curSize < drawcall->numInstances)
          {
            DebugData.feedbackQueries.resize(drawcall->numInstances);
            gl.glGenQueries(drawcall->numInstances - curSize,
                            DebugData.feedbackQueries.data() + curSize);
          }

          // do incremental draws to get the output size. We have to do this O(N^2) style because
          // there's no way to replay only a single instance. We have to replay 1, 2, 3, ... N
          // instances and count the total number of verts each time, then we can see from the
          // difference how much each instance wrote.
          for(uint32_t inst = 1; inst <= drawcall->numInstances; inst++)
          {
            gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);
            gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
                            DebugData.feedbackQueries[inst - 1]);
            gl.glBeginTransformFeedback(lastOutTopo);

            if(!(drawcall->flags & DrawFlags::UseIBuffer))
            {
              if(HasExt[ARB_base_instance])
              {
                gl.glDrawArraysInstancedBaseInstance(drawtopo, drawcall->vertexOffset,
                                                     drawcall->numIndices, inst,
                                                     drawcall->instanceOffset);
              }
              else
              {
                gl.glDrawArraysInstanced(drawtopo, drawcall->vertexOffset, drawcall->numIndices,
                                         inst);
              }
            }
            else
            {
              if(HasExt[ARB_base_instance])
              {
                gl.glDrawElementsInstancedBaseVertexBaseInstance(
                    drawtopo, drawcall->numIndices, idxType,
                    (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth), inst,
                    drawcall->baseVertex, drawcall->instanceOffset);
              }
              else
              {
                gl.glDrawElementsInstancedBaseVertex(
                    drawtopo, drawcall->numIndices, idxType,
                    (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth), inst,
                    drawcall->baseVertex);
              }
            }

            gl.glEndTransformFeedback();
            gl.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
          }
        }
        else
        {
          gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
          gl.glBeginTransformFeedback(lastOutTopo);

          if(!(drawcall->flags & DrawFlags::UseIBuffer))
          {
            if(HasExt[ARB_base_instance])
            {
              gl.glDrawArraysInstancedBaseInstance(drawtopo, drawcall->vertexOffset,
                                                   drawcall->numIndices, drawcall->numInstances,
                                                   drawcall->instanceOffset);
            }
            else
            {
              gl.glDrawArraysInstanced(drawtopo, drawcall->vertexOffset, drawcall->numIndices,
                                       drawcall->numInstances);
            }
          }
          else
          {
            if(HasExt[ARB_base_instance])
            {
              gl.glDrawElementsInstancedBaseVertexBaseInstance(
                  drawtopo, drawcall->numIndices, idxType,
                  (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth),
                  drawcall->numInstances, drawcall->baseVertex, drawcall->instanceOffset);
            }
            else
            {
              gl.glDrawElementsInstancedBaseVertex(
                  drawtopo, drawcall->numIndices, idxType,
                  (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth),
                  drawcall->numInstances, drawcall->baseVertex);
            }
          }

          gl.glEndTransformFeedback();
          gl.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
        }
      }
      else
      {
        gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQueries[0]);
        gl.glBeginTransformFeedback(lastOutTopo);

        if(!(drawcall->flags & DrawFlags::UseIBuffer))
        {
          gl.glDrawArrays(drawtopo, drawcall->vertexOffset, drawcall->numIndices);
        }
        else
        {
          gl.glDrawElementsBaseVertex(
              drawtopo, drawcall->numIndices, idxType,
              (const void *)uintptr_t(drawcall->indexOffset * drawcall->indexByteWidth),
              drawcall->baseVertex);
        }

        gl.glEndTransformFeedback();
        gl.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
      }

      std::vector<GLPostVSData::InstData> instData;

      if((drawcall->flags & DrawFlags::Instanced) && drawcall->numInstances > 1)
      {
        uint64_t prevVertCount = 0;

        for(uint32_t inst = 0; inst < drawcall->numInstances; inst++)
        {
          gl.glGetQueryObjectuiv(DebugData.feedbackQueries[inst], eGL_QUERY_RESULT, &primsWritten);

          uint32_t vertCount = 3 * primsWritten;

          GLPostVSData::InstData d;
          d.numVerts = uint32_t(vertCount - prevVertCount);
          d.bufOffset = uint32_t(stride * prevVertCount);
          prevVertCount = vertCount;

          instData.push_back(d);
        }
      }
      else
      {
        primsWritten = 0;
        gl.glGetQueryObjectuiv(DebugData.feedbackQueries[0], eGL_QUERY_RESULT, &primsWritten);
      }

      error = false;

      if(primsWritten == 0)
      {
        RDCWARN("No primitives written by last vertex processing stage");
        error = true;
      }

      // get buffer data from buffer attached to feedback object
      data = (float *)gl.glMapNamedBufferEXT(DebugData.feedbackBuffer, eGL_READ_ONLY);

      if(data == NULL)
      {
        gl.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);
        RDCERR("Couldn't map feedback buffer!");
        error = true;
      }

      if(error)
      {
        // delete temporary pipelines we made
        gl.glDeleteProgramPipelines(1, &vsFeedbackPipe);
        if(lastFeedbackPipe)
          gl.glDeleteProgramPipelines(1, &lastFeedbackPipe);

        // restore replay state we trashed
        gl.glUseProgram(rs.Program);
        gl.glBindProgramPipeline(rs.Pipeline);

        gl.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array]);
        gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

        gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj);

        if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
          gl.glDisable(eGL_RASTERIZER_DISCARD);
        else
          gl.glEnable(eGL_RASTERIZER_DISCARD);

        return;
      }

      if(lastProg == tesProg)
      {
        // primitive counter is the number of primitives, not vertices
        if(shaderOutMode == eGL_TRIANGLES ||
           shaderOutMode == eGL_QUADS)    // query for quads returns # triangles
          m_PostVSData[eventID].gsout.numVerts = primsWritten * 3;
        else if(shaderOutMode == eGL_ISOLINES)
          m_PostVSData[eventID].gsout.numVerts = primsWritten * 2;
      }
      else if(lastProg == gsProg)
      {
        // primitive counter is the number of primitives, not vertices
        if(shaderOutMode == eGL_POINTS)
          m_PostVSData[eventID].gsout.numVerts = primsWritten;
        else if(shaderOutMode == eGL_LINE_STRIP)
          m_PostVSData[eventID].gsout.numVerts = primsWritten * 2;
        else if(shaderOutMode == eGL_TRIANGLE_STRIP)
          m_PostVSData[eventID].gsout.numVerts = primsWritten * 3;
      }

      // create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
      // can render from it to display previews).
      GLuint lastoutBuffer = 0;
      gl.glGenBuffers(1, &lastoutBuffer);
      gl.glBindBuffer(eGL_ARRAY_BUFFER, lastoutBuffer);
      gl.glNamedBufferDataEXT(lastoutBuffer, stride * m_PostVSData[eventID].gsout.numVerts, data,
                              eGL_STATIC_DRAW);

      byteData = (byte *)data;

      nearp = 0.1f;
      farp = 100.0f;

      pos0 = (Vec4f *)byteData;

      found = false;

      for(uint32_t i = 1; posidx != -1 && i < m_PostVSData[eventID].gsout.numVerts; i++)
      {
        //////////////////////////////////////////////////////////////////////////////////
        // derive near/far, assuming a standard perspective matrix
        //
        // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
        // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
        // and we know Wpost = Zpre from the perspective matrix.
        // we can then see from the perspective matrix that
        // m = F/(F-N)
        // c = -(F*N)/(F-N)
        //
        // with re-arranging and substitution, we then get:
        // N = -c/m
        // F = c/(1-m)
        //
        // so if we can derive m and c then we can determine N and F. We can do this with
        // two points, and we pick them reasonably distinct on z to reduce floating-point
        // error

        Vec4f *pos = (Vec4f *)(byteData + i * stride);

        if(fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
        {
          Vec2f A(pos0->w, pos0->z);
          Vec2f B(pos->w, pos->z);

          float m = (B.y - A.y) / (B.x - A.x);
          float c = B.y - B.x * m;

          if(m == 1.0f)
            continue;

          nearp = -c / m;
          farp = c / (1 - m);

          found = true;

          break;
        }
      }

      // if we didn't find anything, all z's and w's were identical.
      // If the z is positive and w greater for the first element then
      // we detect this projection as reversed z with infinite far plane
      if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
      {
        nearp = pos0->z;
        farp = FLT_MAX;
      }

      gl.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);

      // store everything out to the PostVS data cache
      m_PostVSData[eventID].gsout.buf = lastoutBuffer;
      m_PostVSData[eventID].gsout.instStride = 0;
      if(drawcall->flags & DrawFlags::Instanced)
      {
        m_PostVSData[eventID].gsout.numVerts /= RDCMAX(1U, drawcall->numInstances);
        m_PostVSData[eventID].gsout.instStride = stride * m_PostVSData[eventID].gsout.numVerts;
      }
      m_PostVSData[eventID].gsout.vertStride = stride;
      m_PostVSData[eventID].gsout.nearPlane = nearp;
      m_PostVSData[eventID].gsout.farPlane = farp;

      m_PostVSData[eventID].gsout.useIndices = false;

      m_PostVSData[eventID].gsout.hasPosOut = posidx >= 0;

      m_PostVSData[eventID].gsout.idxBuf = 0;
      m_PostVSData[eventID].gsout.idxByteWidth = 0;

      m_PostVSData[eventID].gsout.topo = MakePrimitiveTopology(gl.GetHookset(), lastOutTopo);

      m_PostVSData[eventID].gsout.instData = instData;
    }

    // set lastProg back to no varyings, for future use
    gl.glTransformFeedbackVaryings(lastProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
    gl.glLinkProgram(lastProg);
  }

  // delete temporary pipelines we made
  gl.glDeleteProgramPipelines(1, &vsFeedbackPipe);
  if(lastFeedbackPipe)
    gl.glDeleteProgramPipelines(1, &lastFeedbackPipe);

  // restore replay state we trashed
  gl.glUseProgram(rs.Program);
  gl.glBindProgramPipeline(rs.Pipeline);

  gl.glBindBuffer(eGL_ARRAY_BUFFER, rs.BufferBindings[GLRenderState::eBufIdx_Array]);
  gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);

  gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, rs.FeedbackObj);

  if(!rs.Enabled[GLRenderState::eEnabled_RasterizerDiscard])
    gl.glDisable(eGL_RASTERIZER_DISCARD);
  else
    gl.glEnable(eGL_RASTERIZER_DISCARD);
}

void GLReplay::InitPostVSBuffers(const vector<uint32_t> &passEvents)
{
  uint32_t prev = 0;

  // since we can always replay between drawcalls, just loop through all the events
  // doing partial replays and calling InitPostVSBuffers for each
  for(size_t i = 0; i < passEvents.size(); i++)
  {
    if(prev != passEvents[i])
    {
      m_pDriver->ReplayLog(prev, passEvents[i], eReplay_WithoutDraw);

      prev = passEvents[i];
    }

    const DrawcallDescription *d = m_pDriver->GetDrawcall(passEvents[i]);

    if(d)
      InitPostVSBuffers(passEvents[i]);
  }
}

MeshFormat GLReplay::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  GLPostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVSData.find(eventID) != m_PostVSData.end())
    postvs = m_PostVSData[eventID];

  const GLPostVSData::StageData &s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf)
    ret.idxbuf = m_pDriver->GetResourceManager()->GetID(BufferRes(NULL, s.idxBuf));
  else
    ret.idxbuf = ResourceId();
  ret.idxoffs = 0;
  ret.idxByteWidth = s.idxByteWidth;
  ret.baseVertex = 0;

  if(s.buf)
    ret.buf = m_pDriver->GetResourceManager()->GetID(BufferRes(NULL, s.buf));
  else
    ret.buf = ResourceId();

  ret.offset = s.instStride * instID;
  ret.stride = s.vertStride;

  ret.compCount = 4;
  ret.compByteWidth = 4;
  ret.compType = CompType::Float;
  ret.specialFormat = SpecialFormat::Unknown;

  ret.showAlpha = false;
  ret.bgraOrder = false;

  ret.topo = s.topo;
  ret.numVerts = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  if(instID < s.instData.size())
  {
    GLPostVSData::InstData inst = s.instData[instID];

    ret.offset = inst.bufOffset;
    ret.numVerts = inst.numVerts;
  }

  return ret;
}

void GLReplay::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                          const MeshDisplay &cfg)
{
  WrappedOpenGL &gl = *m_pDriver;

  if(cfg.position.buf == ResourceId())
    return;

  MakeCurrentReplayContext(m_DebugCtx);

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, DebugData.outWidth / DebugData.outHeight);

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f ModelViewProj = projMat.Mul(camMat);
  Matrix4f guessProjInv;

  gl.glBindVertexArray(DebugData.meshVAO);

  const MeshFormat *fmts[2] = {&cfg.position, &cfg.second};

  GLenum topo = MakeGLPrimitiveTopology(cfg.position.topo);

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

      if(fmt.buf != ResourceId())
      {
        uboParams.color = Vec4f(fmt.meshColor.x, fmt.meshColor.y, fmt.meshColor.z, fmt.meshColor.w);

        uboptr = (MeshUBOData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshUBOData),
                                                    GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        *uboptr = uboParams;
        gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

        GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.buf).name;
        gl.glBindVertexBuffer(0, vb, (GLintptr)fmt.offset, fmt.stride);

        GLenum secondarytopo = MakeGLPrimitiveTopology(fmt.topo);

        if(fmt.idxbuf != ResourceId())
        {
          GLuint ib = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.idxbuf).name;
          gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);

          GLenum idxtype = eGL_UNSIGNED_BYTE;
          if(fmt.idxByteWidth == 2)
            idxtype = eGL_UNSIGNED_SHORT;
          else if(fmt.idxByteWidth == 4)
            idxtype = eGL_UNSIGNED_INT;

          gl.glDrawElementsBaseVertex(secondarytopo, fmt.numVerts, idxtype,
                                      (const void *)uintptr_t(fmt.idxoffs), fmt.baseVertex);
        }
        else
        {
          gl.glDrawArrays(secondarytopo, 0, fmt.numVerts);
        }
      }
    }
  }

  for(uint32_t i = 0; i < 2; i++)
  {
    if(fmts[i]->buf == ResourceId())
      continue;

    if(fmts[i]->specialFormat != SpecialFormat::Unknown)
    {
      if(fmts[i]->specialFormat == SpecialFormat::R10G10B10A2)
      {
        if(fmts[i]->compType == CompType::UInt)
          gl.glVertexAttribIFormat(i, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
        if(fmts[i]->compType == CompType::SInt)
          gl.glVertexAttribIFormat(i, 4, eGL_INT_2_10_10_10_REV, 0);
      }
      else if(fmts[i]->specialFormat == SpecialFormat::R11G11B10)
      {
        gl.glVertexAttribFormat(i, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
      }
      else
      {
        RDCWARN("Unsupported special vertex attribute format: %x", fmts[i]->specialFormat);
      }
    }
    else if(fmts[i]->compType == CompType::Float || fmts[i]->compType == CompType::UNorm ||
            fmts[i]->compType == CompType::SNorm)
    {
      GLenum fmttype = eGL_UNSIGNED_INT;

      if(fmts[i]->compByteWidth == 4)
      {
        if(fmts[i]->compType == CompType::Float)
          fmttype = eGL_FLOAT;
        else if(fmts[i]->compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_INT;
        else if(fmts[i]->compType == CompType::SNorm)
          fmttype = eGL_INT;
      }
      else if(fmts[i]->compByteWidth == 2)
      {
        if(fmts[i]->compType == CompType::Float)
          fmttype = eGL_HALF_FLOAT;
        else if(fmts[i]->compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_SHORT;
        else if(fmts[i]->compType == CompType::SNorm)
          fmttype = eGL_SHORT;
      }
      else if(fmts[i]->compByteWidth == 1)
      {
        if(fmts[i]->compType == CompType::UNorm)
          fmttype = eGL_UNSIGNED_BYTE;
        else if(fmts[i]->compType == CompType::SNorm)
          fmttype = eGL_BYTE;
      }

      gl.glVertexAttribFormat(i, fmts[i]->compCount, fmttype, fmts[i]->compType != CompType::Float,
                              0);
    }
    else if(fmts[i]->compType == CompType::UInt || fmts[i]->compType == CompType::SInt)
    {
      GLenum fmttype = eGL_UNSIGNED_INT;

      if(fmts[i]->compByteWidth == 4)
      {
        if(fmts[i]->compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_INT;
        else if(fmts[i]->compType == CompType::SInt)
          fmttype = eGL_INT;
      }
      else if(fmts[i]->compByteWidth == 2)
      {
        if(fmts[i]->compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_SHORT;
        else if(fmts[i]->compType == CompType::SInt)
          fmttype = eGL_SHORT;
      }
      else if(fmts[i]->compByteWidth == 1)
      {
        if(fmts[i]->compType == CompType::UInt)
          fmttype = eGL_UNSIGNED_BYTE;
        else if(fmts[i]->compType == CompType::SInt)
          fmttype = eGL_BYTE;
      }

      gl.glVertexAttribIFormat(i, fmts[i]->compCount, fmttype, 0);
    }
    else if(fmts[i]->compType == CompType::Double)
    {
      gl.glVertexAttribLFormat(i, fmts[i]->compCount, eGL_DOUBLE, 0);
    }

    GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmts[i]->buf).name;
    gl.glBindVertexBuffer(i, vb, (GLintptr)fmts[i]->offset, fmts[i]->stride);
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

    if(cfg.second.buf != ResourceId())
      gl.glEnableVertexAttribArray(1);

    if(!IsGLES)
      gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

    if(cfg.position.idxByteWidth)
    {
      GLenum idxtype = eGL_UNSIGNED_BYTE;
      if(cfg.position.idxByteWidth == 2)
        idxtype = eGL_UNSIGNED_SHORT;
      else if(cfg.position.idxByteWidth == 4)
        idxtype = eGL_UNSIGNED_INT;

      if(cfg.position.idxbuf != ResourceId())
      {
        GLuint ib = m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.idxbuf).name;
        gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);
      }
      gl.glDrawElementsBaseVertex(topo, cfg.position.numVerts, idxtype,
                                  (const void *)uintptr_t(cfg.position.idxoffs),
                                  cfg.position.baseVertex);
    }
    else
    {
      gl.glDrawArrays(topo, 0, cfg.position.numVerts);
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

    if(cfg.position.idxByteWidth)
    {
      GLenum idxtype = eGL_UNSIGNED_BYTE;
      if(cfg.position.idxByteWidth == 2)
        idxtype = eGL_UNSIGNED_SHORT;
      else if(cfg.position.idxByteWidth == 4)
        idxtype = eGL_UNSIGNED_INT;

      if(cfg.position.idxbuf != ResourceId())
      {
        GLuint ib = m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.idxbuf).name;
        gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, ib);

        gl.glDrawElementsBaseVertex(topo != eGL_PATCHES ? topo : eGL_POINTS, cfg.position.numVerts,
                                    idxtype, (const void *)uintptr_t(cfg.position.idxoffs),
                                    cfg.position.baseVertex);
      }
    }
    else
    {
      gl.glDrawArrays(topo != eGL_PATCHES ? topo : eGL_POINTS, 0, cfg.position.numVerts);
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
    m_HighlightCache.CacheHighlightingData(eventID, cfg);

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
