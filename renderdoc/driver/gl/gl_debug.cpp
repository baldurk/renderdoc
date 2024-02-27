/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "driver/shaders/spirv/glslang_compile.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "strings/string_utils.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

#define OPENGL 1
#include "data/glsl/glsl_ubos_cpp.h"

void GetGLSLVersions(ShaderType &shaderType, int &glslVersion, int &glslBaseVer, int &glslCSVer)
{
  if(IsGLES)
  {
    shaderType = ShaderType::GLSLES;
    glslVersion = glslBaseVer = glslCSVer = 300;

    if(GLCoreVersion >= 31)
      glslVersion = glslBaseVer = glslCSVer = 310;

    if(GLCoreVersion >= 32)
      glslVersion = glslBaseVer = glslCSVer = 320;
  }
  else
  {
    shaderType = ShaderType::GLSL;
    glslVersion = glslBaseVer = 150;
    glslCSVer = 420;
  }
}

GLuint CreateShader(GLenum shaderType, const rdcstr &src)
{
  GLuint ret = GL.glCreateShader(shaderType);

  const char *csrc = src.c_str();
  GL.glShaderSource(ret, 1, &csrc, NULL);

  GL.glCompileShader(ret);

  char buffer[1024] = {};
  GLint status = 0;
  GL.glGetShaderiv(ret, eGL_COMPILE_STATUS, &status);
  if(status == 0)
  {
    GL.glGetShaderInfoLog(ret, 1024, NULL, buffer);
    RDCERR("%s compile error: %s", ToStr(shaderType).c_str(), buffer);
    return 0;
  }

  return ret;
}

GLuint CreateSPIRVShader(GLenum shaderType, const rdcstr &src)
{
  if(!HasExt[ARB_gl_spirv])
  {
    RDCERR("Compiling SPIR-V shader without ARB_gl_spirv - should be checked above!");
    return 0;
  }

  rdcspv::CompilationSettings settings(rdcspv::InputLanguage::OpenGLGLSL,
                                       rdcspv::ShaderStage(ShaderIdx(shaderType)));

  rdcarray<uint32_t> spirv;
  rdcstr s = rdcspv::Compile(settings, {src}, spirv);

  if(spirv.empty())
  {
    RDCERR("Couldn't compile shader to SPIR-V: %s", s.c_str());
    return 0;
  }

  GLuint ret = GL.glCreateShader(shaderType);

  GL.glShaderBinary(1, &ret, eGL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(),
                    (GLsizei)spirv.size() * 4);

  GL.glSpecializeShader(ret, "main", 0, NULL, NULL);

  char buffer[1024] = {};
  GLint status = 0;
  GL.glGetShaderiv(ret, eGL_COMPILE_STATUS, &status);
  if(status == 0)
  {
    GL.glGetShaderInfoLog(ret, 1024, NULL, buffer);
    RDCERR("%s compile error: %s", ToStr(shaderType).c_str(), buffer);
    return 0;
  }

  return ret;
}

GLuint CreateCShaderProgram(const rdcstr &src)
{
  GLuint cs = CreateShader(eGL_COMPUTE_SHADER, src);
  if(cs == 0)
    return 0;

  GLuint ret = GL.glCreateProgram();

  GL.glAttachShader(ret, cs);

  GL.glLinkProgram(ret);

  char buffer[1024] = {};
  GLint status = 0;
  GL.glGetProgramiv(ret, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    GL.glGetProgramInfoLog(ret, 1024, NULL, buffer);
    RDCERR("Link error: %s", buffer);
  }

  GL.glDetachShader(ret, cs);

  GL.glDeleteShader(cs);

  return ret;
}

GLuint CreateShaderProgram(GLuint vs, GLuint fs, GLuint gs)
{
  GLuint ret = GL.glCreateProgram();

  GL.glAttachShader(ret, vs);
  GL.glAttachShader(ret, fs);
  if(gs)
    GL.glAttachShader(ret, gs);

  GL.glLinkProgram(ret);

  char buffer[1024] = {};
  GLint status = 0;
  GL.glGetProgramiv(ret, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    GL.glGetProgramInfoLog(ret, 1024, NULL, buffer);
    RDCERR("Shader error: %s", buffer);
  }

  return ret;
}

GLuint CreateShaderProgram(const rdcstr &vsSrc, const rdcstr &fsSrc, const rdcstr &gsSrc)
{
  GLuint vs = 0;
  GLuint fs = 0;
  GLuint gs = 0;

  if(vsSrc.empty())
  {
    RDCERR("Must have vertex shader - no separable programs supported.");
    return 0;
  }

  if(fsSrc.empty())
  {
    RDCERR("Must have fragment shader - no separable programs supported.");
    return 0;
  }

  vs = CreateShader(eGL_VERTEX_SHADER, vsSrc);
  if(vs == 0)
    return 0;

  fs = CreateShader(eGL_FRAGMENT_SHADER, fsSrc);
  if(fs == 0)
    return 0;

  if(!gsSrc.empty())
  {
    gs = CreateShader(eGL_GEOMETRY_SHADER, gsSrc);
    if(gs == 0)
      return 0;
  }

  GLuint ret = CreateShaderProgram(vs, fs, gs);

  GL.glDetachShader(ret, vs);
  GL.glDetachShader(ret, fs);
  if(gs)
    GL.glDetachShader(ret, gs);

  GL.glDeleteShader(vs);
  GL.glDeleteShader(fs);
  if(gs)
    GL.glDeleteShader(gs);

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

GLuint GLReplay::CreateMeshProgram(GLuint vs, GLuint fs, GLuint gs)
{
  GLuint program = CreateShaderProgram(vs, fs, gs);

  // set attrib locations
  GL.glBindAttribLocation(program, 0, "position");
  GL.glBindAttribLocation(program, 1, "IN_secondary");

  // relink
  GL.glLinkProgram(program);

  // check that the relink succeeded
  char buffer[1024] = {};
  GLint status = 0;
  GL.glGetProgramiv(program, eGL_LINK_STATUS, &status);
  if(status == 0)
  {
    GL.glGetProgramInfoLog(program, 1024, NULL, buffer);
    RDCERR("Link error: %s", buffer);
  }

  // detach the shaders
  GL.glDetachShader(program, vs);
  GL.glDetachShader(program, fs);
  if(gs)
    GL.glDetachShader(program, gs);

  // bind the UBO
  BindUBO(program, "MeshUBOData", 0);

  return program;
}

void GLReplay::ConfigureTexDisplayProgramBindings(GLuint program)
{
  GLint location = -1;

  GL.glUseProgram(program);

// since we split the shader up by type, not all texture slots are always available. Also on GLES
// some texture slots might be missing due to lack of extensions. So we need to check for a location
// of -1
#define SET_TEX_BINDING(name, bind)                  \
  location = GL.glGetUniformLocation(program, name); \
  if(location >= 0)                                  \
    GL.glUniform1i(location, bind);

  SET_TEX_BINDING("texUInt1D", 1);
  SET_TEX_BINDING("texUInt2D", 2);
  SET_TEX_BINDING("texUInt3D", 3);
  SET_TEX_BINDING("texUInt1DArray", 5);
  SET_TEX_BINDING("texUInt2DArray", 6);
  SET_TEX_BINDING("texUInt2DRect", 8);
  SET_TEX_BINDING("texUIntBuffer", 9);
  SET_TEX_BINDING("texUInt2DMS", 10);
  SET_TEX_BINDING("texUInt2DMSArray", 11);

  SET_TEX_BINDING("texSInt1D", 1);
  SET_TEX_BINDING("texSInt2D", 2);
  SET_TEX_BINDING("texSInt3D", 3);
  SET_TEX_BINDING("texSInt1DArray", 5);
  SET_TEX_BINDING("texSInt2DArray", 6);
  SET_TEX_BINDING("texSInt2DRect", 8);
  SET_TEX_BINDING("texSIntBuffer", 9);
  SET_TEX_BINDING("texSInt2DMS", 10);
  SET_TEX_BINDING("texSInt2DMSArray", 11);

  SET_TEX_BINDING("tex1D", 1);
  SET_TEX_BINDING("tex2D", 2);
  SET_TEX_BINDING("tex3D", 3);
  SET_TEX_BINDING("texCube", 4);
  SET_TEX_BINDING("tex1DArray", 5);
  SET_TEX_BINDING("tex2DArray", 6);
  SET_TEX_BINDING("texCubeArray", 7);
  SET_TEX_BINDING("tex2DRect", 8);
  SET_TEX_BINDING("texBuffer", 9);
  SET_TEX_BINDING("tex2DMS", 10);
  SET_TEX_BINDING("tex2DMSArray", 11);

#undef SET_TEX_BINDING
}

void GLReplay::BindUBO(GLuint program, const char *name, GLuint binding)
{
  GL.glUniformBlockBinding(program, GL.glGetUniformBlockIndex(program, name), binding);
}

void GLReplay::InitDebugData()
{
  if(m_pDriver == NULL)
    return;

  // don't reflect any shaders or programs we make
  m_pDriver->PushInternalShader();

  m_HighlightCache.driver = m_pDriver->GetReplay();

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.0f);

  {
    WindowingData window = {WindowingSystem::Headless};
    uint64_t id = MakeOutputWindow(window, true);

    m_DebugCtx = NULL;

    if(id == 0)
      return;

    m_DebugID = id;
    m_DebugCtx = &m_OutputWindows[id];

    MakeCurrentReplayContext(m_DebugCtx);

    m_pDriver->RegisterDebugCallback();
  }

  WrappedOpenGL &drv = *m_pDriver;

  DebugData.outWidth = 0.0f;
  DebugData.outHeight = 0.0f;

  rdcstr vs;
  rdcstr fs;
  rdcstr gs;
  rdcstr cs;

  ShaderType shaderType;
  int glslVersion;
  int glslBaseVer;
  int glslCSVer;    // compute shader

  GetGLSLVersions(shaderType, glslVersion, glslBaseVer, glslCSVer);

  rdcstr texSampleDefines;

  if(IsGLES)
  {
    if(HasExt[OES_texture_cube_map_array] || HasExt[EXT_texture_cube_map_array] || GLCoreVersion >= 32)
      texSampleDefines += "#define TEXSAMPLE_CUBE_ARRAY 1\n";

    if(HasExt[OES_texture_cube_map_array])
      texSampleDefines += "#extension GL_OES_texture_cube_map_array : require\n";

    if(HasExt[EXT_texture_cube_map_array])
      texSampleDefines += "#extension GL_EXT_texture_cube_map_array : require\n";

    if(HasExt[EXT_texture_buffer])
      texSampleDefines +=
          "#define TEXSAMPLE_BUFFER 1\n"
          "#extension GL_EXT_texture_buffer : require\n";

    texSampleDefines += "#define HAS_BIT_CONVERSION 1\n";
  }
  else
  {
    if(HasExt[ARB_texture_cube_map_array])
      texSampleDefines +=
          "#define TEXSAMPLE_CUBE_ARRAY 1\n"
          "#extension GL_ARB_texture_cube_map_array : require\n";

    if(HasExt[ARB_texture_multisample])
      texSampleDefines +=
          "#define TEXSAMPLE_MULTISAMPLE 1\n"
          "#extension GL_ARB_texture_multisample : require\n";

    if(HasExt[ARB_gpu_shader5])
      texSampleDefines +=
          "#define HAS_BIT_CONVERSION 1\n"
          "#extension GL_ARB_gpu_shader5 : require\n";
    else if(HasExt[ARB_shader_bit_encoding])
      texSampleDefines +=
          "#define HAS_BIT_CONVERSION 1\n"
          "#extension GL_ARB_shader_bit_encoding : require\n";
  }

  rdcstr vsDefines = "#define FORCE_IO_LOCATION 1";

  if(!IsGLES)
  {
    vsDefines =
        "#extension GL_ARB_separate_shader_objects : require\n"
        "#extension GL_ARB_explicit_attrib_location : require\n" +
        vsDefines;
  }

  vs = GenerateGLSLShader(GetEmbeddedResource(glsl_blit_vert), shaderType, glslBaseVer, vsDefines);

  // used to combine with custom shaders.
  // this has to have explicit locations on the output even though we don't normally use that,
  // because GL doesn't have a fallback to match by name, and custom shaders are expected to have an
  // explicit location on the input
  DebugData.texDisplayVertexShader = CreateShader(eGL_VERTEX_SHADER, vs);

  vs = GenerateGLSLShader(GetEmbeddedResource(glsl_blit_vert), shaderType, glslBaseVer);
  fs = GenerateGLSLShader(GetEmbeddedResource(glsl_fixedcol_frag), shaderType, glslBaseVer);
  DebugData.fullScreenFixedColProg = CreateShaderProgram(vs, fs);

  fs = GenerateGLSLShader(GetEmbeddedResource(glsl_depth_copy_frag), shaderType, glslBaseVer);
  DebugData.fullScreenCopyDepth = CreateShaderProgram(vs, fs);

  fs = GenerateGLSLShader(GetEmbeddedResource(glsl_depth_copyms_frag), shaderType, glslBaseVer);
  DebugData.fullScreenCopyDepthMS = CreateShaderProgram(vs, fs);

  DebugData.fixedcolFragShaderSPIRV = DebugData.quadoverdrawFragShaderSPIRV = 0;

  // pre-compile SPIR-V shaders up front since this is more expensive
  if(HasExt[ARB_gl_spirv])
  {
    // SPIR-V shaders are always generated as desktop GL 430, for ease
    rdcstr source =
        GenerateGLSLShader(GetEmbeddedResource(glsl_fixedcol_frag), ShaderType::GLSPIRV, 430);
    DebugData.fixedcolFragShaderSPIRV = CreateSPIRVShader(eGL_FRAGMENT_SHADER, source);

    if(HasExt[ARB_gpu_shader5] && HasExt[ARB_shader_image_load_store])
    {
      rdcstr defines = "";

      if(!HasExt[ARB_derivative_control])
      {
        defines += "#define dFdxFine dFdx\n\n";
        defines += "#define dFdyFine dFdy\n\n";
      }

      source = GenerateGLSLShader(GetEmbeddedResource(glsl_quadwrite_frag), ShaderType::GLSPIRV,
                                  430, defines);
      DebugData.quadoverdrawFragShaderSPIRV = CreateSPIRVShader(eGL_FRAGMENT_SHADER, source);
    }
  }

  for(int i = 0; i < 3; i++)
  {
    rdcstr defines = rdcstr("#define SHADER_BASETYPE ") + ToStr(i) + "\n";

    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_texdisplay_frag), shaderType, glslBaseVer,
                            defines + texSampleDefines);

    DebugData.texDisplayProg[i] = CreateShaderProgram(vs, fs);

    BindUBO(DebugData.texDisplayProg[i], "TexDisplayUBOData", 0);
    BindUBO(DebugData.texDisplayProg[i], "HeatmapData", 1);
    ConfigureTexDisplayProgramBindings(DebugData.texDisplayProg[i]);

    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_texremap_frag), shaderType, glslBaseVer,
                            defines + texSampleDefines);

    DebugData.texRemapProg[i] = CreateShaderProgram(vs, fs);

    BindUBO(DebugData.texRemapProg[i], "TexDisplayUBOData", 0);
    ConfigureTexDisplayProgramBindings(DebugData.texRemapProg[i]);
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.2f);

  if(GLCoreVersion >= 43 && !IsGLES)
  {
    GLint numsl = 0;
    drv.glGetIntegerv(eGL_NUM_SHADING_LANGUAGE_VERSIONS, &numsl);

    for(GLint i = 0; i < numsl; i++)
    {
      const char *sl = (const char *)drv.glGetStringi(eGL_SHADING_LANGUAGE_VERSION, (GLuint)i);

      CheckGLSLVersion(sl, glslVersion);
    }
  }
  else
  {
    const char *sl = (const char *)drv.glGetString(eGL_SHADING_LANGUAGE_VERSION);

    CheckGLSLVersion(sl, glslVersion);
  }

  DebugData.glslVersion = glslVersion;

  RDCLOG("GLSL version %d", glslVersion);

  vs = GenerateGLSLShader(GetEmbeddedResource(glsl_blit_vert), shaderType, glslBaseVer);

  DebugData.fixedcolFragShader = DebugData.quadoverdrawFragShader = 0;
  DebugData.quadoverdrawResolveProg = 0;

  if(IsGLES)
  {
    // quad overdraw not supported on GLES.
    // 1.
    //   dFdx doesn't support uints - potentially workaroundable with float casts, but highly
    //   doubtful GLES compilers will do that properly without exploding.
    // 2.
    //   quad overdraw write shader must be linked with user shaders in program, which requires
    //   matching ESSL version and features required for it aren't exposed as extensions to older
    //   versions but only in core versions.
  }
  else if(HasExt[ARB_shader_image_load_store] && HasExt[ARB_gpu_shader5])
  {
    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_quadresolve_frag), shaderType, glslBaseVer);

    DebugData.quadoverdrawResolveProg = CreateShaderProgram(vs, fs);

    GL.glUseProgram(DebugData.quadoverdrawResolveProg);

    GL.glUniform1i(GL.glGetUniformLocation(DebugData.quadoverdrawResolveProg, "overdrawImage"), 0);
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
  }

  fs = GenerateGLSLShader(GetEmbeddedResource(glsl_checkerboard_frag), shaderType, glslBaseVer);
  DebugData.checkerProg = CreateShaderProgram(vs, fs);

  BindUBO(DebugData.checkerProg, "CheckerboardUBOData", 0);

  for(size_t intIdx = 0; intIdx < 3; intIdx++)
  {
    for(size_t numViews = 0; numViews < ARRAY_COUNT(DebugData.discardProg[intIdx]); numViews++)
    {
      rdcstr defines;

      defines += rdcstr("#define SHADER_BASETYPE ") + ToStr(intIdx) + "\n";

      if(numViews > 0 && IsGLES && HasExt[OVR_multiview])
        defines = StringFormat::Fmt("#define NUM_VIEWS %zu\n", numViews + 1);

      rdcstr blitvs =
          GenerateGLSLShader(GetEmbeddedResource(glsl_blit_vert), shaderType, glslBaseVer, defines);

      fs = GenerateGLSLShader(GetEmbeddedResource(glsl_discard_frag), shaderType, glslBaseVer,
                              defines);
      DebugData.discardProg[intIdx][numViews] = CreateShaderProgram(blitvs, fs);

      BindUBO(DebugData.discardProg[intIdx][numViews], "DiscardUBOData", 0);

      if(!IsGLES)
      {
        rdcstr name = "col0";
        for(GLuint i = 0; i < 8; i++)
        {
          name[3] = char('0' + i);
          GL.glBindFragDataLocation(DebugData.discardProg[intIdx][numViews], i, name.c_str());
        }
      }
    }
  }

  {
    ResourceFormat fmt;
    fmt.type = ResourceFormatType::Regular;
    fmt.compType = CompType::Float;
    fmt.compByteWidth = 4;
    fmt.compCount = 1;
    bytebuf pattern = GetDiscardPattern(DiscardType::InvalidateCall, fmt, 1, true);
    fmt.compType = CompType::UInt;
    pattern.append(GetDiscardPattern(DiscardType::InvalidateCall, fmt, 1, true));
    drv.glGenBuffers(1, &DebugData.discardPatternBuffer);
    drv.glBindBuffer(eGL_UNIFORM_BUFFER, DebugData.discardPatternBuffer);
    drv.glNamedBufferDataEXT(DebugData.discardPatternBuffer, pattern.size(), pattern.data(),
                             eGL_STATIC_DRAW);
  }

  if(HasExt[ARB_geometry_shader4])
  {
    vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer);
    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_trisize_frag), shaderType, glslBaseVer);
    gs = GenerateGLSLShader(GetEmbeddedResource(glsl_trisize_geom), shaderType, glslBaseVer);

    // create the shaders
    GLuint vsShad = CreateShader(eGL_VERTEX_SHADER, vs);
    GLuint trifsShad = CreateShader(eGL_FRAGMENT_SHADER, fs);
    GLuint gsShad = CreateShader(eGL_GEOMETRY_SHADER, gs);

    DebugData.trisizeProg = CreateMeshProgram(vsShad, trifsShad, gsShad);

    // bind trisize-unique viewport size UBO
    BindUBO(DebugData.trisizeProg, "ViewportSizeUBO", 2);

    GL.glDeleteShader(trifsShad);
    GL.glDeleteShader(gsShad);

    // we have two fragment shaders, one that reads from the vs outputs and one that reads from the
    // gs outputs
    rdcstr vsfs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_frag), shaderType, glslBaseVer,
                                     "#define SECONDARY_NAME vsout_secondary\n"
                                     "#define NORM_NAME vsout_norm\n");
    rdcstr gsfs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_frag), shaderType, glslBaseVer,
                                     "#define SECONDARY_NAME gsout_secondary\n"
                                     "#define NORM_NAME gsout_norm\n");
    gs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_geom), shaderType, glslBaseVer);

    // recreate the shaders
    GLuint vsfsShad = CreateShader(eGL_FRAGMENT_SHADER, vsfs);
    GLuint gsfsShad = CreateShader(eGL_FRAGMENT_SHADER, gsfs);
    gsShad = CreateShader(eGL_GEOMETRY_SHADER, gs);

    DebugData.meshProg[0] = CreateMeshProgram(vsShad, vsfsShad);
    DebugData.meshgsProg[0] = CreateMeshProgram(vsShad, gsfsShad, gsShad);

    if(HasExt[ARB_gpu_shader_fp64] && HasExt[ARB_vertex_attrib_64bit])
    {
      rdcstr extensions =
          "#extension GL_ARB_gpu_shader_fp64 : require\n"
          "#extension GL_ARB_vertex_attrib_64bit : require\n";

      // position only dvec4
      vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer,
                              extensions + "#define POSITION_TYPE dvec4\n");

      // delete old shader and recreate with new source
      GL.glDeleteShader(vsShad);
      vsShad = CreateShader(eGL_VERTEX_SHADER, vs);

      DebugData.meshProg[1] = CreateMeshProgram(vsShad, vsfsShad);
      DebugData.meshgsProg[1] = CreateMeshProgram(vsShad, gsfsShad, gsShad);

      // secondary only dvec4
      vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer,
                              extensions + "#define SECONDARY_TYPE dvec4\n");

      // delete old shader and recreate with new source
      GL.glDeleteShader(vsShad);
      vsShad = CreateShader(eGL_VERTEX_SHADER, vs);

      DebugData.meshProg[2] = CreateMeshProgram(vsShad, vsfsShad);
      DebugData.meshgsProg[2] = CreateMeshProgram(vsShad, gsfsShad, gsShad);

      // both dvec4
      vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer,
                              extensions +
                                  "#define POSITION_TYPE dvec4\n"
                                  "#define SECONDARY_TYPE dvec4\n");

      // delete old shader and recreate with new source
      GL.glDeleteShader(vsShad);
      vsShad = CreateShader(eGL_VERTEX_SHADER, vs);

      DebugData.meshProg[3] = CreateMeshProgram(vsShad, vsfsShad);
      DebugData.meshgsProg[3] = CreateMeshProgram(vsShad, gsfsShad, gsShad);
    }
    else
    {
      // we don't warn about the lack of double support, assuming that if the driver doesn't support
      // it then it's highly unlikely that the capture uses it.
      DebugData.meshProg[1] = DebugData.meshProg[2] = DebugData.meshProg[3] = 0;
      DebugData.meshgsProg[1] = DebugData.meshgsProg[2] = DebugData.meshgsProg[3] = 0;
    }

    GL.glDeleteShader(vsShad);
    GL.glDeleteShader(vsfsShad);
    GL.glDeleteShader(gsfsShad);
    GL.glDeleteShader(gsShad);
  }
  else
  {
    vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer);

    // without a geometry shader, the fragment shader always reads from vs outputs
    fs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_frag), shaderType, glslBaseVer,
                            "#define SECONDARY_NAME vsout_secondary\n"
                            "#define NORM_NAME vsout_norm\n");

    // create the shaders
    GLuint vsShad = CreateShader(eGL_VERTEX_SHADER, vs);
    GLuint fsShad = CreateShader(eGL_FRAGMENT_SHADER, fs);

    DebugData.meshProg[0] = CreateMeshProgram(vsShad, fsShad);
    RDCEraseEl(DebugData.meshgsProg);
    DebugData.trisizeProg = 0;

    const char *warning_msg =
        "GL_ARB_geometry_shader4/GL_EXT_geometry_shader not supported, disabling triangle size and "
        "lit solid shading feature.";
    RDCWARN(warning_msg);
    m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                               MessageSource::RuntimeWarning, warning_msg);

    if(HasExt[ARB_gpu_shader_fp64] && HasExt[ARB_vertex_attrib_64bit])
    {
      rdcstr extensions =
          "#extension GL_ARB_gpu_shader_fp64 : require\n"
          "#extension GL_ARB_vertex_attrib_64bit : require\n";

      // position only dvec4
      vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer,
                              extensions + "#define POSITION_TYPE dvec4");

      // delete old shader and recreate with new source
      GL.glDeleteShader(vsShad);
      vsShad = CreateShader(eGL_VERTEX_SHADER, vs);

      DebugData.meshProg[1] = CreateMeshProgram(vsShad, fsShad);

      // secondary only dvec4
      vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer,
                              extensions + "#define SECONDARY_TYPE dvec4");

      // delete old shader and recreate with new source
      GL.glDeleteShader(vsShad);
      vsShad = CreateShader(eGL_VERTEX_SHADER, vs);

      DebugData.meshProg[2] = CreateMeshProgram(vsShad, fsShad);

      // both dvec4
      vs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_vert), shaderType, glslBaseVer,
                              extensions +
                                  "#define POSITION_TYPE dvec4\n"
                                  "#define SECONDARY_TYPE dvec4");

      // delete old shader and recreate with new source
      GL.glDeleteShader(vsShad);
      vsShad = CreateShader(eGL_VERTEX_SHADER, vs);

      DebugData.meshProg[3] = CreateMeshProgram(vsShad, fsShad);
    }
    else
    {
      // we don't warn about the lack of double support, assuming that if the driver doesn't support
      // it then it's highly unlikely that the capture uses it.
      DebugData.meshProg[1] = DebugData.meshProg[2] = DebugData.meshProg[3] = 0;
    }

    GL.glDeleteShader(vsShad);
    GL.glDeleteShader(fsShad);
  }

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.4f);

  drv.glGenBuffers(ARRAY_COUNT(DebugData.UBOs), DebugData.UBOs);
  for(size_t i = 0; i < ARRAY_COUNT(DebugData.UBOs); i++)
  {
    drv.glBindBuffer(eGL_UNIFORM_BUFFER, DebugData.UBOs[i]);
    drv.glNamedBufferDataEXT(DebugData.UBOs[i], 2048, NULL, eGL_DYNAMIC_DRAW);
    RDCCOMPILE_ASSERT(sizeof(TexDisplayUBOData) <= 2048, "UBO too small");
    RDCCOMPILE_ASSERT(sizeof(FontUBOData) <= 2048, "UBO too small");
    RDCCOMPILE_ASSERT(sizeof(HistogramUBOData) <= 2048, "UBO too small");
  }

  DebugData.overlayTexWidth = DebugData.overlayTexHeight = DebugData.overlayTexSamples = 0;
  DebugData.overlayTex = DebugData.overlayFBO = 0;

  DebugData.overlayProg = 0;

  drv.glGenFramebuffers(1, &DebugData.customFBO);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.customFBO);
  DebugData.customTex = 0;

  drv.glGenFramebuffers(1, &DebugData.pickPixelFBO);
  drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.pickPixelFBO);

  if(HasExt[ARB_texture_buffer_object])
  {
    drv.glGenBuffers(1, &DebugData.dummyTexBufferStore);
    drv.glBindBuffer(eGL_TEXTURE_BUFFER, DebugData.dummyTexBufferStore);
    drv.glNamedBufferDataEXT(DebugData.dummyTexBufferStore, 32, NULL, eGL_STATIC_DRAW);
    drv.glBindBuffer(eGL_TEXTURE_BUFFER, 0);

    drv.glGenTextures(1, &DebugData.dummyTexBuffer);
    drv.glBindTexture(eGL_TEXTURE_BUFFER, DebugData.dummyTexBuffer);
    drv.glTextureBufferEXT(DebugData.dummyTexBuffer, eGL_TEXTURE_BUFFER, eGL_RGBA32F,
                           DebugData.dummyTexBufferStore);
    drv.glBindTexture(eGL_TEXTURE_BUFFER, 0);
  }
  else
  {
    DebugData.dummyTexBuffer = DebugData.dummyTexBufferStore = 0;
  }

  drv.glGenTextures(1, &DebugData.pickPixelTex);
  drv.glBindTexture(eGL_TEXTURE_2D, DebugData.pickPixelTex);

  drv.glTextureImage2DEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, 0, eGL_RGBA32F, 1, 1, 0, eGL_RGBA,
                          eGL_FLOAT, NULL);
  drv.glTextureParameteriEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);
  drv.glTextureParameteriEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER,
                             eGL_NEAREST);
  drv.glTextureParameteriEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER,
                             eGL_NEAREST);
  drv.glTextureParameteriEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S,
                             eGL_CLAMP_TO_EDGE);
  drv.glTextureParameteriEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T,
                             eGL_CLAMP_TO_EDGE);
  drv.glFramebufferTexture2D(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, eGL_TEXTURE_2D,
                             DebugData.pickPixelTex, 0);

  drv.glGenVertexArrays(1, &DebugData.emptyVAO);
  drv.glBindVertexArray(DebugData.emptyVAO);

  RenderDoc::Inst().SetProgress(LoadProgress::DebugManagerInit, 0.6f);

  // histogram/minmax data
  {
    RDCEraseEl(DebugData.minmaxTileProgram);
    RDCEraseEl(DebugData.histogramProgram);
    RDCEraseEl(DebugData.minmaxResultProgram);

    RDCCOMPILE_ASSERT(
        ARRAY_COUNT(DebugData.minmaxTileProgram) >= (TEXDISPLAY_SINT_TEX | TEXDISPLAY_TYPEMASK) + 1,
        "not enough programs");

    if(HasExt[ARB_compute_shader] && HasExt[ARB_shader_storage_buffer_object] &&
       HasExt[ARB_shading_language_420pack])
    {
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

          {
            rdcstr defines;
            defines += rdcstr("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
            defines += rdcstr("#define SHADER_BASETYPE ") + ToStr(i) + "\n";
            defines += texSampleDefines;

            cs = GenerateGLSLShader(GetEmbeddedResource(glsl_minmaxtile_comp), shaderType,
                                    glslCSVer, defines);

            DebugData.minmaxTileProgram[idx] = CreateCShaderProgram(cs);

            BindUBO(DebugData.minmaxTileProgram[idx], "HistogramUBOData", 2);
            ConfigureTexDisplayProgramBindings(DebugData.minmaxTileProgram[idx]);
          }

          {
            rdcstr defines;
            defines += rdcstr("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
            defines += rdcstr("#define SHADER_BASETYPE ") + ToStr(i) + "\n";
            defines += texSampleDefines;

            cs = GenerateGLSLShader(GetEmbeddedResource(glsl_histogram_comp), shaderType, glslCSVer,
                                    defines);

            DebugData.histogramProgram[idx] = CreateCShaderProgram(cs);

            BindUBO(DebugData.histogramProgram[idx], "HistogramUBOData", 2);
            ConfigureTexDisplayProgramBindings(DebugData.histogramProgram[idx]);
          }

          if(t == 1)
          {
            rdcstr defines;
            defines += rdcstr("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
            defines += rdcstr("#define SHADER_BASETYPE ") + ToStr(i) + "\n";

            cs = GenerateGLSLShader(GetEmbeddedResource(glsl_minmaxresult_comp), shaderType,
                                    glslCSVer, defines);

            DebugData.minmaxResultProgram[i] = CreateCShaderProgram(cs);

            BindUBO(DebugData.minmaxResultProgram[i], "HistogramUBOData", 2);
            ConfigureTexDisplayProgramBindings(DebugData.minmaxResultProgram[i]);
          }
        }
      }
    }
    else
    {
      RDCWARN(
          "GL_ARB_compute_shader or ARB_shader_storage_buffer_object not supported, disabling "
          "min/max and histogram features.");
      m_pDriver->AddDebugMessage(MessageCategory::Portability, MessageSeverity::Medium,
                                 MessageSource::RuntimeWarning,
                                 "GL_ARB_compute_shader or ARB_shader_storage_buffer_object not "
                                 "supported, disabling min/max and histogram features.");
    }

    drv.glGenBuffers(1, &DebugData.minmaxTileResult);
    drv.glGenBuffers(1, &DebugData.minmaxResult);
    drv.glGenBuffers(1, &DebugData.histogramBuf);

    const uint32_t maxTexDim = 16384;
    const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
    const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

    const size_t byteSize =
        2 * sizeof(Vec4f) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;

    drv.glNamedBufferDataEXT(DebugData.minmaxTileResult, byteSize, NULL, eGL_DYNAMIC_DRAW);
    drv.glNamedBufferDataEXT(DebugData.minmaxResult, sizeof(Vec4f) * 2, NULL, eGL_DYNAMIC_READ);
    drv.glNamedBufferDataEXT(DebugData.histogramBuf, sizeof(uint32_t) * HGRAM_NUM_BUCKETS, NULL,
                             eGL_DYNAMIC_READ);
  }

  if(HasExt[ARB_compute_shader] && HasExt[ARB_shading_language_420pack])
  {
    cs = GenerateGLSLShader(GetEmbeddedResource(glsl_mesh_comp), shaderType, glslCSVer);
    DebugData.meshPickProgram = CreateCShaderProgram(cs);

    BindUBO(DebugData.meshPickProgram, "MeshPickUBOData", 0);
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
    drv.glGenBuffers(1, &DebugData.pickResultBuf);
    drv.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickResultBuf);
    drv.glNamedBufferDataEXT(DebugData.pickResultBuf,
                             sizeof(Vec4f) * DebugRenderData::maxMeshPicks + sizeof(uint32_t) * 4,
                             NULL, eGL_DYNAMIC_READ);

    // sized/created on demand
    DebugData.pickVBBuf = DebugData.pickIBBuf = 0;
    DebugData.pickVBSize = DebugData.pickIBSize = 0;
  }

  drv.glGenVertexArrays(1, &DebugData.meshVAO);
  drv.glBindVertexArray(DebugData.meshVAO);

  drv.glGenBuffers(1, &DebugData.axisFrustumBuffer);
  drv.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.axisFrustumBuffer);

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
      Vec3f(0.0f, 0.0f, 0.0f),
      Vec3f(1.0f, 0.0f, 0.0f),
      Vec3f(0.0f, 0.0f, 0.0f),
      Vec3f(0.0f, 1.0f, 0.0f),
      Vec3f(0.0f, 0.0f, 0.0f),
      Vec3f(0.0f, 0.0f, 1.0f),

      // frustum vertices
      TLN,
      TRN,
      TRN,
      BRN,
      BRN,
      BLN,
      BLN,
      TLN,

      TLN,
      TLF,
      TRN,
      TRF,
      BLN,
      BLF,
      BRN,
      BRF,

      TLF,
      TRF,
      TRF,
      BRF,
      BRF,
      BLF,
      BLF,
      TLF,
  };

  drv.glNamedBufferDataEXT(DebugData.axisFrustumBuffer, sizeof(axisFrustum), axisFrustum,
                           eGL_STATIC_DRAW);

  drv.glGenVertexArrays(1, &DebugData.axisVAO);
  drv.glBindVertexArray(DebugData.axisVAO);
  drv.glVertexAttribPointer(0, 3, eGL_FLOAT, GL_FALSE, sizeof(Vec3f), NULL);
  drv.glEnableVertexAttribArray(0);

  drv.glGenVertexArrays(1, &DebugData.frustumVAO);
  drv.glBindVertexArray(DebugData.frustumVAO);
  drv.glVertexAttribPointer(0, 3, eGL_FLOAT, GL_FALSE, sizeof(Vec3f),
                            (const void *)(sizeof(Vec3f) * 6));
  drv.glEnableVertexAttribArray(0);

  drv.glGenVertexArrays(1, &DebugData.triHighlightVAO);
  drv.glBindVertexArray(DebugData.triHighlightVAO);

  drv.glGenBuffers(1, &DebugData.triHighlightBuffer);
  drv.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);

  drv.glNamedBufferDataEXT(DebugData.triHighlightBuffer, sizeof(Vec4f) * 24, NULL, eGL_DYNAMIC_DRAW);

  drv.glVertexAttribPointer(0, 4, eGL_FLOAT, GL_FALSE, sizeof(Vec4f), NULL);
  drv.glEnableVertexAttribArray(0);

  MakeCurrentReplayContext(&m_ReplayCtx);

  // try to identify the GPU we're running on.
  {
    RDCEraseEl(m_DriverInfo);

    const char *vendor = (const char *)drv.glGetString(eGL_VENDOR);
    const char *renderer = (const char *)drv.glGetString(eGL_RENDERER);
    const char *version = (const char *)drv.glGetString(eGL_VERSION);

    // we're just doing substring searches, so combine both for ease.
    rdcstr combined = (vendor ? vendor : "");
    combined += " ";
    combined += (renderer ? renderer : "");

    // make lowercase, for case-insensitive matching, and add preceding/trailing space for easier
    // 'word' matching
    combined = " " + strlower(combined) + " ";

    RDCDEBUG("Identifying vendor from '%s'", combined.c_str());

    struct pattern
    {
      const char *search;
      GPUVendor vendor;
    } patterns[] = {
        {" arm ", GPUVendor::ARM},
        {" mali ", GPUVendor::ARM},
        {" mali-", GPUVendor::ARM},
        {" amd ", GPUVendor::AMD},
        {"advanced micro devices", GPUVendor::AMD},
        {"ati technologies", GPUVendor::AMD},
        {"radeon", GPUVendor::AMD},
        {"broadcom", GPUVendor::Broadcom},
        {"imagination", GPUVendor::Imagination},
        {"powervr", GPUVendor::Imagination},
        {"intel", GPUVendor::Intel},
        {"geforce", GPUVendor::nVidia},
        {"quadro", GPUVendor::nVidia},
        {"nouveau", GPUVendor::nVidia},
        {"nvidia", GPUVendor::nVidia},
        {"adreno", GPUVendor::Qualcomm},
        {"qualcomm", GPUVendor::Qualcomm},
        {"vivante", GPUVendor::Verisilicon},
        {"llvmpipe", GPUVendor::Software},
        {"softpipe", GPUVendor::Software},
        {"bluestacks", GPUVendor::Software},
    };

    for(const pattern &p : patterns)
    {
      if(combined.contains(p.search))
      {
        if(m_DriverInfo.vendor == GPUVendor::Unknown)
        {
          m_DriverInfo.vendor = p.vendor;
        }
        else
        {
          // either we already found this with another pattern, or we've identified two patterns and
          // it's ambiguous. Keep the first one we found, arbitrarily, but print a warning.
          if(m_DriverInfo.vendor != p.vendor)
          {
            RDCWARN("Already identified '%s' as %s, but now identified as %s", combined.c_str(),
                    ToStr(m_DriverInfo.vendor).c_str(), ToStr(p.vendor).c_str());
          }
        }
      }
    }

    RDCDEBUG("Identified GPU vendor '%s'", ToStr(m_DriverInfo.vendor).c_str());

    rdcstr versionString = version;

    versionString += " / ";
    versionString += renderer ? renderer : "";
    versionString += " / ";
    versionString += vendor ? vendor : "";

    versionString.resize(RDCMIN(versionString.size(), ARRAY_COUNT(m_DriverInfo.version) - 1));
    memcpy(m_DriverInfo.version, versionString.c_str(), versionString.size());
  }

  // these below need to be made on the replay context, as they are context-specific (not shared)
  // and will be used on the replay context.

  if(HasExt[ARB_transform_feedback2])
    drv.glGenTransformFeedbacks(1, &DebugData.feedbackObj);
  drv.glGenBuffers(1, &DebugData.feedbackBuffer);
  DebugData.feedbackQueries.push_back(0);
  drv.glGenQueries(1, &DebugData.feedbackQueries[0]);

  if(HasExt[ARB_transform_feedback2])
    drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);
  drv.glBindBuffer(eGL_TRANSFORM_FEEDBACK_BUFFER, DebugData.feedbackBuffer);
  drv.glNamedBufferDataEXT(DebugData.feedbackBuffer, (GLsizeiptr)DebugData.feedbackBufferSize, NULL,
                           eGL_DYNAMIC_READ);
  drv.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);
  if(HasExt[ARB_transform_feedback2])
    drv.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);

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

  if(!HasExt[ARB_shader_image_load_store] || !HasExt[ARB_compute_shader] ||
     !HasExt[ARB_shading_language_420pack])
  {
    RDCWARN(
        "Don't have shader image load/store or compute shaders, functionality will be degraded.");
    m_Degraded = true;
  }

#if ENABLED(RDOC_APPLE)
  // temporary hack - just never consider apple degraded, since there's never going to be an
  // improvement so there's no point warning users.
  m_Degraded = false;
#endif

  m_pDriver->PopInternalShader();
}

void GLReplay::DeleteDebugData()
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(&m_ReplayCtx);

  if(HasExt[KHR_debug])
  {
    GL.glDebugMessageCallback(NULL, NULL);
  }

  if(DebugData.overlayProg)
    drv.glDeleteProgram(DebugData.overlayProg);

  for(size_t b = 0; b < 3; b++)
    for(size_t i = 0; i < ARRAY_COUNT(DebugData.discardProg[b]); i++)
      if(DebugData.discardProg[b][i])
        drv.glDeleteProgram(DebugData.discardProg[b][i]);

  if(HasExt[ARB_transform_feedback2])
    drv.glDeleteTransformFeedbacks(1, &DebugData.feedbackObj);
  drv.glDeleteBuffers(1, &DebugData.feedbackBuffer);
  drv.glDeleteQueries((GLsizei)DebugData.feedbackQueries.size(), DebugData.feedbackQueries.data());

  MakeCurrentReplayContext(m_DebugCtx);

  if(HasExt[KHR_debug])
  {
    GL.glDebugMessageCallback(NULL, NULL);
  }

  ClearPostVSCache();

  drv.glDeleteFramebuffers(1, &DebugData.overlayFBO);
  drv.glDeleteTextures(1, &DebugData.overlayTex);

  if(DebugData.fullScreenFixedColProg)
    drv.glDeleteProgram(DebugData.fullScreenFixedColProg);
  if(DebugData.fullScreenCopyDepth)
    drv.glDeleteProgram(DebugData.fullScreenCopyDepth);
  if(DebugData.fullScreenCopyDepthMS)
    drv.glDeleteProgram(DebugData.fullScreenCopyDepthMS);

  if(DebugData.quadoverdrawFragShader)
    drv.glDeleteShader(DebugData.quadoverdrawFragShader);
  if(DebugData.quadoverdrawFragShaderSPIRV)
    drv.glDeleteShader(DebugData.quadoverdrawFragShaderSPIRV);
  if(DebugData.quadoverdrawResolveProg)
    drv.glDeleteProgram(DebugData.quadoverdrawResolveProg);

  if(DebugData.texDisplayVertexShader)
    drv.glDeleteShader(DebugData.texDisplayVertexShader);
  for(int i = 0; i < 3; i++)
  {
    if(DebugData.texDisplayProg[i])
      drv.glDeleteProgram(DebugData.texDisplayProg[i]);
    if(DebugData.texRemapProg[i])
      drv.glDeleteProgram(DebugData.texRemapProg[i]);
  }

  if(DebugData.checkerProg)
    drv.glDeleteProgram(DebugData.checkerProg);
  if(DebugData.fixedcolFragShader)
    drv.glDeleteShader(DebugData.fixedcolFragShader);
  if(DebugData.fixedcolFragShaderSPIRV)
    drv.glDeleteShader(DebugData.fixedcolFragShaderSPIRV);

  for(size_t i = 0; i < ARRAY_COUNT(DebugData.meshProg); i++)
  {
    if(DebugData.meshProg[i])
      drv.glDeleteProgram(DebugData.meshProg[i]);
    if(DebugData.meshgsProg[i])
      drv.glDeleteProgram(DebugData.meshgsProg[i]);
  }
  if(DebugData.trisizeProg)
    drv.glDeleteProgram(DebugData.trisizeProg);

  drv.glDeleteBuffers(ARRAY_COUNT(DebugData.UBOs), DebugData.UBOs);
  drv.glDeleteFramebuffers(1, &DebugData.pickPixelFBO);
  drv.glDeleteTextures(1, &DebugData.pickPixelTex);

  drv.glDeleteTextures(1, &DebugData.dummyTexBuffer);
  drv.glDeleteBuffers(1, &DebugData.dummyTexBufferStore);

  drv.glDeleteFramebuffers(1, &DebugData.customFBO);
  drv.glDeleteTextures(1, &DebugData.customTex);

  drv.glDeleteVertexArrays(1, &DebugData.emptyVAO);

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

      if(DebugData.minmaxTileProgram[idx])
        drv.glDeleteProgram(DebugData.minmaxTileProgram[idx]);
      if(DebugData.histogramProgram[idx])
        drv.glDeleteProgram(DebugData.histogramProgram[idx]);

      if(t == 1)
      {
        if(DebugData.minmaxResultProgram[i])
          drv.glDeleteProgram(DebugData.minmaxResultProgram[i]);
      }
    }
  }

  if(DebugData.meshPickProgram)
    drv.glDeleteProgram(DebugData.meshPickProgram);
  drv.glDeleteBuffers(1, &DebugData.pickIBBuf);
  drv.glDeleteBuffers(1, &DebugData.pickVBBuf);
  drv.glDeleteBuffers(1, &DebugData.pickResultBuf);

  drv.glDeleteBuffers(1, &DebugData.minmaxTileResult);
  drv.glDeleteBuffers(1, &DebugData.minmaxResult);
  drv.glDeleteBuffers(1, &DebugData.histogramBuf);

  drv.glDeleteVertexArrays(1, &DebugData.meshVAO);
  drv.glDeleteVertexArrays(1, &DebugData.axisVAO);
  drv.glDeleteVertexArrays(1, &DebugData.frustumVAO);
  drv.glDeleteVertexArrays(1, &DebugData.triHighlightVAO);

  drv.glDeleteBuffers(1, &DebugData.axisFrustumBuffer);
  drv.glDeleteBuffers(1, &DebugData.triHighlightBuffer);
}

GLReplay::TextureSamplerState GLReplay::SetSamplerParams(GLenum target, GLuint texname,
                                                         TextureSamplerMode mode)
{
  TextureSamplerState ret;

  if(target == eGL_TEXTURE_BUFFER || target == eGL_TEXTURE_2D_MULTISAMPLE ||
     target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    return ret;

  // fetch previous texture sampler settings
  GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MIN_FILTER, (GLint *)&ret.minFilter);
  GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAG_FILTER, (GLint *)&ret.magFilter);
  GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_S, (GLint *)&ret.wrapS);
  GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_T, (GLint *)&ret.wrapT);
  GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_R, (GLint *)&ret.wrapR);
  GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_COMPARE_MODE, (GLint *)&ret.compareMode);

  // disable depth comparison
  GLenum compareMode = eGL_NONE;
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_COMPARE_MODE, (GLint *)&compareMode);

  // always want to clamp
  GLenum param = eGL_CLAMP_TO_EDGE;
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_S, (GLint *)&param);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_T, (GLint *)&param);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_R, (GLint *)&param);

  // depending on the mode, set min/mag filter
  GLenum minFilter = eGL_NEAREST;
  GLenum magFilter = eGL_NEAREST;

  switch(mode)
  {
    case TextureSamplerMode::Point:
      minFilter = eGL_NEAREST_MIPMAP_NEAREST;
      magFilter = eGL_NEAREST;
      break;
    case TextureSamplerMode::PointNoMip:
      minFilter = eGL_NEAREST;
      magFilter = eGL_NEAREST;
      break;
    case TextureSamplerMode::Linear:
      minFilter = eGL_LINEAR;
      magFilter = eGL_LINEAR;
      break;
  }

  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MIN_FILTER, (GLint *)&minFilter);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAG_FILTER, (GLint *)&magFilter);

  return ret;
}

void GLReplay::RestoreSamplerParams(GLenum target, GLuint texname, TextureSamplerState state)
{
  if(target == eGL_TEXTURE_BUFFER || target == eGL_TEXTURE_2D_MULTISAMPLE ||
     target == eGL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    return;

  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_S, (GLint *)&state.wrapS);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_T, (GLint *)&state.wrapT);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_WRAP_R, (GLint *)&state.wrapR);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MIN_FILTER, (GLint *)&state.minFilter);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAG_FILTER, (GLint *)&state.magFilter);
  GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_COMPARE_MODE, (GLint *)&state.compareMode);
}

void GLReplay::FillWithDiscardPattern(DiscardType type, GLuint framebuffer, GLsizei numAttachments,
                                      const GLenum *attachments, GLint x, GLint y, GLsizei width,
                                      GLsizei height)
{
  RDCASSERT(type == DiscardType::InvalidateCall);

  WrappedOpenGL &drv = *m_pDriver;

  GLMarkerRegion region("FillWithDiscardPattern FBO");

  GLRenderState rs;
  rs.FetchState(&drv);

  // we use the original framebuffer, and mask off any attachments that shouldn't be discarded
  drv.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, framebuffer);

  int numviews = 1;

  if(IsGLES && HasExt[OVR_multiview])
  {
    GL.glGetNamedFramebufferAttachmentParameterivEXT(
        framebuffer, eGL_COLOR_ATTACHMENT0, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR,
        &numviews);
  }

  drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.discardPatternBuffer);

  drv.glDisable(eGL_BLEND);
  drv.glDisable(eGL_CULL_FACE);
  drv.glEnable(eGL_DEPTH_TEST);
  drv.glEnable(eGL_STENCIL_TEST);
  drv.glDepthFunc(eGL_ALWAYS);
  drv.glStencilFunc(eGL_ALWAYS, 0, 0xff);
  if(!IsGLES)
    drv.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);

  // scissor to the rect
  drv.glEnable(eGL_SCISSOR_TEST);
  drv.glScissor(x, y, width, height);

  GLint maxview[2] = {2048, 2048};
  drv.glGetIntegerv(eGL_MAX_VIEWPORT_DIMS, maxview);
  drv.glViewport(0, 0, maxview[0], maxview[1]);

  // disable all masks at first
  drv.glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  drv.glDepthMask(GL_FALSE);
  drv.glStencilMask(0);

  int intIdx = -1;

  // enable masks specified by attachments
  bool stencil = false;
  for(GLsizei i = 0; i < numAttachments; i++)
  {
    if(attachments[i] == eGL_DEPTH_STENCIL_ATTACHMENT)
    {
      drv.glDepthMask(GL_TRUE);
      drv.glStencilMask(0xff);
      stencil = true;
    }
    else if(attachments[i] == eGL_DEPTH_ATTACHMENT)
    {
      drv.glDepthMask(GL_TRUE);
    }
    else if(attachments[i] == eGL_STENCIL_ATTACHMENT)
    {
      drv.glStencilMask(0xff);
      stencil = true;
    }
    else
    {
      drv.glColorMaski(attachments[i] - eGL_COLOR_ATTACHMENT0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

      if(intIdx == -1)
      {
        GLuint att;
        drv.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachments[i],
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                                  (GLint *)&att);
        GLenum attType;
        drv.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, attachments[i],
                                                  eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                                  (GLint *)&attType);

        ResourceId id = drv.GetResourceManager()->GetResID(attType == eGL_RENDERBUFFER
                                                               ? RenderbufferRes(drv.GetCtx(), att)
                                                               : TextureRes(drv.GetCtx(), att));

        GLenum fmt = m_pDriver->m_Textures[id].internalFormat;

        if(IsUIntFormat(fmt))
          intIdx = 1;
        else if(IsSIntFormat(fmt))
          intIdx = 2;
        else
          intIdx = 0;
      }
    }
  }

  if(intIdx == -1)
    intIdx = 0;

  GLuint prog = DebugData.discardProg[intIdx][RDCCLAMP(numviews, 1, 4) - 1];

  drv.glUseProgram(prog);

  GLint loc = drv.glGetUniformLocation(prog, "flags");

  uint32_t flags = 0U;

  if(height == 1)
    flags |= 0x10U;

  if(stencil)
  {
    drv.glStencilOp(eGL_REPLACE, eGL_REPLACE, eGL_REPLACE);

    drv.glStencilFunc(eGL_ALWAYS, 0, 0xff);
    drv.glProgramUniform1ui(prog, loc, flags | 1U);
    drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

    drv.glStencilFunc(eGL_ALWAYS, 0xff, 0xff);
    drv.glProgramUniform1ui(prog, loc, flags | 2U);
    drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
  }
  else
  {
    drv.glProgramUniform1ui(prog, loc, flags);

    // if we're not writing stencil we can just do one draw
    drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
  }

  rs.ApplyState(&drv);
}

void GLReplay::FillWithDiscardPattern(DiscardType type, ResourceId id, GLuint mip, GLint xoffset,
                                      GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                      GLsizei depth)
{
  RDCASSERT(type == DiscardType::InvalidateCall);

  WrappedOpenGL &drv = *m_pDriver;

  GLMarkerRegion region("FillWithDiscardPattern Texture");

  auto &texDetails = drv.m_Textures[id];

  GLenum fmt = texDetails.internalFormat;

  bytebuf &pattern = m_DiscardPatterns[fmt];

  GLenum target = texDetails.curType;

  if(pattern.empty())
    pattern = GetDiscardPattern(type, MakeResourceFormat(target, fmt), 1, true);

  bool compressed = IsCompressedFormat(fmt);

  if(target == eGL_TEXTURE_CUBE_MAP)
    depth = RDCMIN(depth, 6);
  else
    depth = RDCMIN(texDetails.depth, depth);
  height = RDCMIN(texDetails.height, height);
  width = RDCMIN(texDetails.width, width);

  GLint dim = texDetails.dimension;

  GLint oldRowLength = 0;
  GL.glGetIntegerv(eGL_UNPACK_ROW_LENGTH, &oldRowLength);

  // ensure even if we're uploading a sub-rect that the row length is the same.
  GL.glPixelStorei(eGL_UNPACK_ROW_LENGTH, DiscardPatternWidth);

  GLuint tex = texDetails.resource.name;

  GLenum format = eGL_NONE;
  GLenum datatype = eGL_NONE;

  if(!compressed)
  {
    format = GetBaseFormat(fmt);
    datatype = GetDataType(fmt);
  }

  for(GLsizei zc = 0; zc < depth; zc++)
  {
    GLsizei z = zoffset + zc;

    if(texDetails.curType == eGL_TEXTURE_CUBE_MAP)
    {
      GLenum targets[] = {
          eGL_TEXTURE_CUBE_MAP_POSITIVE_X, eGL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Y, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          eGL_TEXTURE_CUBE_MAP_POSITIVE_Z, eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      };

      target = targets[z % 6];
    }

    for(GLsizei yc = 0; yc < height; yc += DiscardPatternHeight)
    {
      GLsizei y = yoffset + yc;

      for(GLsizei xc = 0; xc < width; xc += DiscardPatternWidth)
      {
        GLsizei x = xoffset + xc;

        GLsizei subwidth = RDCMIN((GLsizei)DiscardPatternWidth, texDetails.width - x);
        GLsizei subheight = RDCMIN((GLsizei)DiscardPatternHeight, texDetails.height - y);

        if(compressed)
        {
          GLsizei dataSize = (GLsizei)GetCompressedByteSize(subwidth, subheight, 1, fmt);

          if(dim == 1)
            GL.glCompressedTextureSubImage1DEXT(tex, target, mip, x, subwidth, fmt, dataSize,
                                                pattern.data());
          else if(dim == 2)
            GL.glCompressedTextureSubImage2DEXT(tex, target, mip, x, y, subwidth, subheight, fmt,
                                                dataSize, pattern.data());
          else if(dim == 3)
            GL.glCompressedTextureSubImage3DEXT(tex, target, mip, x, y, z, subwidth, subheight, 1,
                                                fmt, dataSize, pattern.data());
        }
        else
        {
          if(dim == 1)
            GL.glTextureSubImage1DEXT(tex, target, mip, x, subwidth, format, datatype,
                                      pattern.data());
          else if(dim == 2)
            GL.glTextureSubImage2DEXT(tex, target, mip, x, y, subwidth, subheight, format, datatype,
                                      pattern.data());
          else if(dim == 3)
            GL.glTextureSubImage3DEXT(tex, target, mip, x, y, z, subwidth, subheight, 1, format,
                                      datatype, pattern.data());
        }
      }
    }
  }

  GL.glPixelStorei(eGL_UNPACK_ROW_LENGTH, oldRowLength);
}

void GLReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                         CompType typeCast, float pixel[4])
{
  WrappedOpenGL &drv = *m_pDriver;

  MakeCurrentReplayContext(m_DebugCtx);

  drv.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.pickPixelFBO);
  drv.glBindFramebuffer(eGL_READ_FRAMEBUFFER, DebugData.pickPixelFBO);

  pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0.0f;
  drv.glClearBufferfv(eGL_COLOR, 0, pixel);

  DebugData.outWidth = DebugData.outHeight = 1.0f;
  drv.glViewport(0, 0, 1, 1);

  TextureDisplay texDisplay;

  texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
  texDisplay.flipY = false;
  texDisplay.hdrMultiplier = -1.0f;
  texDisplay.linearDisplayAsGamma = true;
  texDisplay.subresource = sub;
  texDisplay.customShaderId = ResourceId();
  texDisplay.rangeMin = 0.0f;
  texDisplay.rangeMax = 1.0f;
  texDisplay.scale = 1.0f;
  texDisplay.resourceId = texture;
  texDisplay.typeCast = typeCast;
  texDisplay.rawOutput = true;
  texDisplay.xOffset = -float(x << sub.mip);
  texDisplay.yOffset = -float(y << sub.mip);

  auto &texDetails = m_pDriver->m_Textures[texDisplay.resourceId];

  uint32_t mipWidth = RDCMAX(1U, (uint32_t)texDetails.width >> sub.mip);
  uint32_t mipHeight = RDCMAX(1U, (uint32_t)texDetails.height >> sub.mip);

  texDisplay.xOffset = -(float(x) / float(mipWidth)) * texDetails.width;
  texDisplay.yOffset = -(float(y) / float(mipHeight)) * texDetails.height;

  RenderTextureInternal(texDisplay, eTexDisplay_MipShift);

  drv.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixel);

  if(!HasExt[ARB_gpu_shader5])
  {
    if(IsSIntFormat(texDetails.internalFormat))
    {
      int32_t casted[4] = {
          (int32_t)pixel[0],
          (int32_t)pixel[1],
          (int32_t)pixel[2],
          (int32_t)pixel[3],
      };

      memcpy(pixel, casted, sizeof(casted));
    }
    else if(IsUIntFormat(texDetails.internalFormat))
    {
      uint32_t casted[4] = {
          (uint32_t)pixel[0],
          (uint32_t)pixel[1],
          (uint32_t)pixel[2],
          (uint32_t)pixel[3],
      };

      memcpy(pixel, casted, sizeof(casted));
    }
  }

  {
    // need to read stencil separately as GL can't read both depth and stencil
    // at the same time.
    if(texDetails.internalFormat == eGL_DEPTH24_STENCIL8 ||
       texDetails.internalFormat == eGL_DEPTH32F_STENCIL8 ||
       texDetails.internalFormat == eGL_DEPTH_STENCIL)
    {
      texDisplay.red = texDisplay.blue = texDisplay.alpha = false;

      RenderTextureInternal(texDisplay, eTexDisplay_MipShift);

      uint32_t stencilpixel[4];
      drv.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)stencilpixel);

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
    }
  }
}

bool GLReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, float *minval,
                         float *maxval)
{
  auto &texDetails = m_pDriver->m_Textures[texid];

  if(!IsCompressedFormat(texDetails.internalFormat) &&
     GetBaseFormat(texDetails.internalFormat) == eGL_DEPTH_STENCIL)
  {
    // for depth/stencil we need to run the code twice - once to fetch depth and once to fetch
    // stencil - since we can't process float depth and int stencil at the same time
    Vec4f depth[2] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    };
    Vec4u stencil[2] = {{0, 0, 0, 0}, {1, 1, 1, 1}};

    bool success = GetMinMax(texid, sub, typeCast, false, &depth[0].x, &depth[1].x);

    if(!success)
      return false;

    success = GetMinMax(texid, sub, typeCast, true, (float *)&stencil[0].x, (float *)&stencil[1].x);

    if(!success)
      return false;

    // copy across into green channel, casting up to float, dividing by the range for this texture
    float rangeScale = 1.0f;
    switch(texDetails.internalFormat)
    {
      case eGL_STENCIL_INDEX1: rangeScale = 1.0f; break;
      case eGL_STENCIL_INDEX4: rangeScale = 16.0f; break;
      default: RDCWARN("Unexpected raw format for stencil visualization"); DELIBERATE_FALLTHROUGH();
      case eGL_DEPTH24_STENCIL8:
      case eGL_DEPTH32F_STENCIL8:
      case eGL_DEPTH_STENCIL:
      case eGL_STENCIL_INDEX8: rangeScale = 255.0f; break;
      case eGL_STENCIL_INDEX16: rangeScale = 65535.0f; break;
    }

    depth[0].y = float(stencil[0].x) / rangeScale;
    depth[1].y = float(stencil[1].x) / rangeScale;

    memcpy(minval, &depth[0], sizeof(depth[0]));
    memcpy(maxval, &depth[1], sizeof(depth[1]));

    return true;
  }

  return GetMinMax(texid, sub, typeCast, false, minval, maxval);
}

bool GLReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast, bool stencil,
                         float *minval, float *maxval)
{
  if(texid == ResourceId() || m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
    return false;

  if(!HasExt[ARB_compute_shader] || !HasExt[ARB_shading_language_420pack])
    return false;

  auto &texDetails = m_pDriver->m_Textures[texid];

  TextureDescription details = GetTexture(texid);

  int texSlot = 0;
  int intIdx = 0;

  bool renderbuffer = false;

  switch(texDetails.curType)
  {
    case eGL_RENDERBUFFER:
      texSlot = texDetails.samples > 1 ? RESTYPE_TEX2DMS : RESTYPE_TEX2D;
      renderbuffer = true;
      break;
    case eGL_TEXTURE_1D: texSlot = RESTYPE_TEX1D; break;
    default: RDCWARN("Unexpected texture type"); DELIBERATE_FALLTHROUGH();
    case eGL_TEXTURE_2D: texSlot = RESTYPE_TEX2D; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: texSlot = RESTYPE_TEX2DMS; break;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: texSlot = RESTYPE_TEX2DMSARRAY; break;
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

    GLMarkerRegion blitRegion("Renderbuffer Blit");

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

    SafeBlitFramebuffer(
        0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width, texDetails.height,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

    texname = texDetails.renderbufferReadTex;
    target = texDetails.samples > 1 ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;
  }

  MakeCurrentReplayContext(m_DebugCtx);

  GLMarkerRegion region("GetMinMax");

  RDCGLenum dsTexMode = eGL_NONE;
  if(IsDepthStencilFormat(texDetails.internalFormat))
  {
    if(stencil)
    {
      dsTexMode = eGL_STENCIL_INDEX;
      intIdx = 1;
    }
    else
    {
      dsTexMode = eGL_DEPTH_COMPONENT;
    }
  }
  else
  {
    if(details.format.compType == CompType::UInt)
      intIdx = 1;
    if(details.format.compType == CompType::SInt)
      intIdx = 2;
  }

  GL.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[0]);
  HistogramUBOData *cdata =
      (HistogramUBOData *)GL.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width >> sub.mip, 1U);
  cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height >> sub.mip, 1U);
  cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth >> sub.mip, 1U);
  if(texDetails.curType == eGL_TEXTURE_3D)
    cdata->HistogramSlice =
        (float)RDCCLAMP(sub.slice, 0U, uint32_t(details.depth >> sub.mip) - 1) + 0.001f;
  else
    cdata->HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, details.arraysize - 1) + 0.001f;
  cdata->HistogramMip = (int)sub.mip;
  cdata->HistogramNumSamples = texDetails.samples;
  cdata->HistogramSample = (int)RDCCLAMP(sub.sample, 0U, details.msSamp - 1);
  if(sub.sample == ~0U)
    cdata->HistogramSample = -int(details.msSamp);
  cdata->HistogramMin = 0.0f;
  cdata->HistogramMax = 1.0f;
  cdata->HistogramChannels = 0xf;

  cdata->HistogramYUVDownsampleRate = {};
  cdata->HistogramYUVAChannels = {};

  int progIdx = texSlot;

  if(intIdx == 1)
    progIdx |= TEXDISPLAY_UINT_TEX;
  if(intIdx == 2)
    progIdx |= TEXDISPLAY_SINT_TEX;

  int blocksX = (int)ceil(cdata->HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata->HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  GL.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  GL.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + texSlot));
  GL.glBindTexture(target, texname);

  TextureSamplerMode mode = TextureSamplerMode::Point;

  if(texSlot == RESTYPE_TEXRECT || texSlot == RESTYPE_TEXBUFFER)
    mode = TextureSamplerMode::PointNoMip;

  TextureSamplerState prevSampState = SetSamplerParams(target, texname, mode);

  GLint origDSTexMode = eGL_DEPTH_COMPONENT;
  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
  {
    GL.glGetTextureParameterivEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &origDSTexMode);
    GL.glTextureParameteriEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, dsTexMode);
  }

  GLint baseLevel[4] = {-1};
  GLint maxlevel[4] = {-1};
  GLint forcedparam[4] = {};

  bool levelsTex = (target != eGL_TEXTURE_BUFFER && target != eGL_TEXTURE_2D_MULTISAMPLE &&
                    target != eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

  if(levelsTex)
  {
    GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, baseLevel);
    GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);
  }
  else
  {
    baseLevel[0] = maxlevel[0] = -1;
  }

  // ensure texture is mipmap complete and we can view all mips (if the range has been reduced) by
  // forcing TEXTURE_MAX_LEVEL to cover all valid mips.
  if(levelsTex && texid != DebugData.CustomShaderTexID)
  {
    forcedparam[0] = 0;
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, forcedparam);
    forcedparam[0] = GLint(details.mips - 1);
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, forcedparam);
  }
  else
  {
    maxlevel[0] = -1;
  }

  GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.minmaxTileResult);

  GL.glUseProgram(DebugData.minmaxTileProgram[progIdx]);
  GL.glDispatchCompute(blocksX, blocksY, 1);

  GL.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.minmaxResult);
  GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 1, DebugData.minmaxTileResult);

  GL.glUseProgram(DebugData.minmaxResultProgram[intIdx]);
  GL.glDispatchCompute(1, 1, 1);

  GL.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  Vec4f minmax[2];
  GL.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.minmaxResult);
  GL.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(minmax), minmax);

  if(baseLevel[0] >= 0)
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, baseLevel);

  if(maxlevel[0] >= 0)
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);

  RestoreSamplerParams(target, texname, prevSampState);

  minval[0] = minmax[0].x;
  minval[1] = minmax[0].y;
  minval[2] = minmax[0].z;
  minval[3] = minmax[0].w;

  maxval[0] = minmax[1].x;
  maxval[1] = minmax[1].y;
  maxval[2] = minmax[1].z;
  maxval[3] = minmax[1].w;

  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
    GL.glTextureParameteriEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, origDSTexMode);

  return true;
}

bool GLReplay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                            float minval, float maxval, const rdcfixedarray<bool, 4> &channels_,
                            rdcarray<uint32_t> &histogram)
{
  if(minval >= maxval || texid == ResourceId())
    return false;

  if(m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
    return false;

  if(!HasExt[ARB_compute_shader] || !HasExt[ARB_shading_language_420pack])
    return false;

  // take a local copy so we can modify it
  rdcfixedarray<bool, 4> channels = channels_;

  auto &texDetails = m_pDriver->m_Textures[texid];

  TextureDescription details = GetTexture(texid);

  int texSlot = 0;
  int intIdx = 0;

  bool renderbuffer = false;

  switch(texDetails.curType)
  {
    case eGL_RENDERBUFFER:
      texSlot = texDetails.samples > 1 ? RESTYPE_TEX2DMS : RESTYPE_TEX2D;
      renderbuffer = true;
      break;
    case eGL_TEXTURE_1D: texSlot = RESTYPE_TEX1D; break;
    default: RDCWARN("Unexpected texture type"); DELIBERATE_FALLTHROUGH();
    case eGL_TEXTURE_2D: texSlot = RESTYPE_TEX2D; break;
    case eGL_TEXTURE_2D_MULTISAMPLE: texSlot = RESTYPE_TEX2DMS; break;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: texSlot = RESTYPE_TEX2DMSARRAY; break;
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

    GLMarkerRegion blitRegion("Renderbuffer Blit");

    GLuint curDrawFBO = 0;
    GLuint curReadFBO = 0;
    GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&curDrawFBO);
    GL.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint *)&curReadFBO);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

    SafeBlitFramebuffer(
        0, 0, texDetails.width, texDetails.height, 0, 0, texDetails.width, texDetails.height,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
    GL.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

    texname = texDetails.renderbufferReadTex;
    target = texDetails.samples > 1 ? eGL_TEXTURE_2D_MULTISAMPLE : eGL_TEXTURE_2D;
  }

  MakeCurrentReplayContext(m_DebugCtx);

  GLMarkerRegion region("GetHistogram");

  RDCGLenum dsTexMode = eGL_NONE;
  if(IsDepthStencilFormat(texDetails.internalFormat))
  {
    // stencil-only, make sure we display it as such
    if(texDetails.internalFormat == eGL_STENCIL_INDEX8)
    {
      channels[0] = false;
      channels[1] = true;
      channels[2] = false;
      channels[3] = false;
    }

    // depth-only, make sure we display it as such
    if(GetBaseFormat(texDetails.internalFormat) == eGL_DEPTH_COMPONENT)
    {
      channels[0] = true;
      channels[1] = false;
      channels[2] = false;
      channels[3] = false;
    }

    if(!channels[0] && channels[1])
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
          DELIBERATE_FALLTHROUGH();
        case eGL_DEPTH24_STENCIL8:
        case eGL_DEPTH32F_STENCIL8:
        case eGL_DEPTH_STENCIL:
        case eGL_STENCIL_INDEX8: rangeScale = 255.0f; break;
        case eGL_STENCIL_INDEX16: rangeScale = 65535.0f; break;
      }
      minval *= rangeScale;
      maxval *= rangeScale;
    }
    else
    {
      dsTexMode = eGL_DEPTH_COMPONENT;
    }
  }
  else
  {
    if(details.format.compType == CompType::UInt)
      intIdx = 1;
    if(details.format.compType == CompType::SInt)
      intIdx = 2;
  }

  GL.glBindBufferBase(eGL_UNIFORM_BUFFER, 2, DebugData.UBOs[0]);
  HistogramUBOData *cdata =
      (HistogramUBOData *)GL.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width >> sub.mip, 1U);
  cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height >> sub.mip, 1U);
  cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth >> sub.mip, 1U);
  if(texDetails.curType == eGL_TEXTURE_3D)
    cdata->HistogramSlice =
        (float)RDCCLAMP(sub.slice, 0U, uint32_t(details.depth >> sub.mip) - 1) + 0.001f;
  else
    cdata->HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, details.arraysize - 1) + 0.001f;
  cdata->HistogramMip = sub.mip;
  cdata->HistogramNumSamples = texDetails.samples;
  cdata->HistogramSample = (int)RDCCLAMP(sub.sample, 0U, details.msSamp - 1);
  if(sub.sample == ~0U)
    cdata->HistogramSample = -int(details.msSamp);
  cdata->HistogramMin = minval;

  cdata->HistogramYUVDownsampleRate = {};
  cdata->HistogramYUVAChannels = {};

  // The calculation in the shader normalises each value between min and max, then multiplies by the
  // number of buckets.
  // But any value equal to HistogramMax must go into NUM_BUCKETS-1, so add a small delta.
  cdata->HistogramMax = maxval + maxval * 1e-6f;

  cdata->HistogramChannels = 0;
  if(dsTexMode == eGL_NONE)
  {
    if(channels[0])
      cdata->HistogramChannels |= 0x1;
    if(channels[1])
      cdata->HistogramChannels |= 0x2;
    if(channels[2])
      cdata->HistogramChannels |= 0x4;
    if(channels[3])
      cdata->HistogramChannels |= 0x8;
  }
  else
  {
    // Both depth and stencil texture mode use the red channel
    cdata->HistogramChannels |= 0x1;
  }
  cdata->HistogramFlags = 0;

  int progIdx = texSlot;

  if(intIdx == 1)
    progIdx |= TEXDISPLAY_UINT_TEX;
  if(intIdx == 2)
    progIdx |= TEXDISPLAY_SINT_TEX;

  int blocksX = (int)ceil(cdata->HistogramTextureResolution.x /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY = (int)ceil(cdata->HistogramTextureResolution.y /
                          float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  GL.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  GL.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + texSlot));
  GL.glBindTexture(target, texname);

  TextureSamplerMode mode = TextureSamplerMode::Point;

  if(texSlot == RESTYPE_TEXRECT || texSlot == RESTYPE_TEXBUFFER)
    mode = TextureSamplerMode::PointNoMip;

  TextureSamplerState prevSampState = SetSamplerParams(target, texname, mode);

  GLint origDSTexMode = eGL_DEPTH_COMPONENT;
  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
  {
    GL.glGetTextureParameterivEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &origDSTexMode);
    GL.glTextureParameteriEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, dsTexMode);
  }

  GLint baseLevel[4] = {-1};
  GLint maxlevel[4] = {-1};
  GLint forcedparam[4] = {};

  bool levelsTex = (target != eGL_TEXTURE_BUFFER && target != eGL_TEXTURE_2D_MULTISAMPLE &&
                    target != eGL_TEXTURE_2D_MULTISAMPLE_ARRAY);

  if(levelsTex)
  {
    GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, baseLevel);
    GL.glGetTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);
  }
  else
  {
    baseLevel[0] = maxlevel[0] = -1;
  }

  // ensure texture is mipmap complete and we can view all mips (if the range has been reduced) by
  // forcing TEXTURE_MAX_LEVEL to cover all valid mips.
  if(levelsTex && texid != DebugData.CustomShaderTexID)
  {
    forcedparam[0] = 0;
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, forcedparam);
    forcedparam[0] = GLint(details.mips - 1);
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, forcedparam);
  }
  else
  {
    maxlevel[0] = -1;
  }

  GL.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.histogramBuf);

  GLuint zero = 0;
  GL.glClearBufferData(eGL_SHADER_STORAGE_BUFFER, eGL_R32UI, eGL_RED_INTEGER, eGL_UNSIGNED_INT,
                       &zero);

  GL.glUseProgram(DebugData.histogramProgram[progIdx]);
  GL.glDispatchCompute(blocksX, blocksY, 1);

  GL.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  histogram.clear();
  histogram.resize(HGRAM_NUM_BUCKETS);

  GL.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.histogramBuf);
  GL.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(uint32_t) * HGRAM_NUM_BUCKETS, &histogram[0]);

  if(baseLevel[0] >= 0)
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_BASE_LEVEL, baseLevel);

  if(maxlevel[0] >= 0)
    GL.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, maxlevel);

  RestoreSamplerParams(target, texname, prevSampState);

  if(dsTexMode != eGL_NONE && HasExt[ARB_stencil_texturing])
    GL.glTextureParameteriEXT(texname, target, eGL_DEPTH_STENCIL_TEXTURE_MODE, origDSTexMode);

  return true;
}

uint32_t GLReplay::PickVertex(uint32_t eventId, int32_t width, int32_t height,
                              const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  WrappedOpenGL &drv = *m_pDriver;

  if(!HasExt[ARB_compute_shader] || !HasExt[ARB_shading_language_420pack])
    return ~0U;

  MakeCurrentReplayContext(m_DebugCtx);

  drv.glUseProgram(DebugData.meshPickProgram);

  Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(width) / float(height));

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();
  Matrix4f pickMVP = projMat.Mul(camMat);
  if(!cfg.position.unproject)
  {
    pickMVP = pickMVP.Mul(Matrix4f(cfg.axisMapping));
  }

  bool reverseProjection = false;
  Matrix4f guessProj;
  Matrix4f guessProjInverse;
  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    if(cfg.position.farPlane != FLT_MAX)
    {
      guessProj =
          Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect);
    }
    else
    {
      reverseProjection = true;
      guessProj = Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);
    }

    if(cfg.ortho)
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);

    if(cfg.position.flipY)
      guessProj[5] *= -1.0f;

    guessProjInverse = guessProj.Inverse();
  }

  Vec3f rayPos;
  Vec3f rayDir;
  // convert mouse pos to world space ray
  {
    float pickX = ((float)x) / ((float)width);
    float pickXCanonical = RDCLERP(-1.0f, 1.0f, pickX);

    float pickY = ((float)y) / ((float)height);
    // flip the Y axis by default for Y-up
    float pickYCanonical = RDCLERP(1.0f, -1.0f, pickY);

    if(cfg.position.flipY && !cfg.ortho)
      pickYCanonical = -pickYCanonical;

    // x/y is inside the window. Since we're not using the window projection we need to correct
    // for the aspect ratio here.
    if(cfg.position.unproject && !cfg.ortho)
      pickXCanonical *= (float(width) / float(height)) / cfg.aspect;

    // set up the NDC near/far pos
    Vec3f nearPosNDC = Vec3f(pickXCanonical, pickYCanonical, 0);
    Vec3f farPosNDC = Vec3f(pickXCanonical, pickYCanonical, 1);

    if(cfg.position.unproject && cfg.ortho)
    {
      // orthographic projections we raycast in NDC space
      Matrix4f inversePickMVP = pickMVP.Inverse();

      // transform from the desired NDC co-ordinates into camera space
      Vec3f nearPosCamera = inversePickMVP.Transform(nearPosNDC, 1);
      Vec3f farPosCamera = inversePickMVP.Transform(farPosNDC, 1);

      Vec3f testDir = (farPosCamera - nearPosCamera);
      testDir.Normalise();

      Matrix4f pickMVPguessProjInverse = guessProj.Mul(inversePickMVP);

      Vec3f nearPosProj = pickMVPguessProjInverse.Transform(nearPosNDC, 1);
      Vec3f farPosProj = pickMVPguessProjInverse.Transform(farPosNDC, 1);

      rayDir = (farPosProj - nearPosProj);
      rayDir.Normalise();

      // Calculate the ray direction first in the regular way (above), so we can use the
      // the output for testing if the ray we are picking is negative or not. This is similar
      // to checking against the forward direction of the camera, but more robust
      if(testDir.z < 0)
      {
        rayDir = -rayDir;
      }
      rayPos = nearPosProj;
    }
    else if(cfg.position.unproject)
    {
      // projected data we pick in world-space to avoid problems with handling unusual transforms

      if(reverseProjection)
      {
        farPosNDC.z = 1e-6f;
        nearPosNDC.z = 1e+6f;
      }

      // invert the guessed projection matrix to get the near/far pos in camera space
      Vec3f nearPosCamera = guessProjInverse.Transform(nearPosNDC, 1.0f);
      Vec3f farPosCamera = guessProjInverse.Transform(farPosNDC, 1.0f);

      // normalise and generate the ray
      rayDir = (farPosCamera - nearPosCamera);
      rayDir.Normalise();

      farPosCamera = nearPosCamera + rayDir;

      // invert the camera transform to transform the ray as camera-relative into world space
      Matrix4f inverseCamera = camMat.Inverse();

      Vec3f nearPosWorld = inverseCamera.Transform(nearPosCamera, 1);
      Vec3f farPosWorld = inverseCamera.Transform(farPosCamera, 1);

      // again normalise our final ray
      rayDir = (farPosWorld - nearPosWorld);
      rayDir.Normalise();

      rayPos = nearPosWorld;
    }
    else
    {
      Matrix4f inversePickMVP = pickMVP.Inverse();

      // transform from the desired NDC co-ordinates into model space
      Vec3f nearPosCamera = inversePickMVP.Transform(nearPosNDC, 1);
      Vec3f farPosCamera = inversePickMVP.Transform(farPosNDC, 1);

      rayDir = (farPosCamera - nearPosCamera);
      rayDir.Normalise();
      rayPos = nearPosCamera;
    }
  }

  GLuint ib = 0;

  uint32_t minIndex = 0;
  uint32_t maxIndex = cfg.position.numIndices;

  uint32_t idxclamp = 0;
  if(cfg.position.baseVertex < 0)
    idxclamp = uint32_t(-cfg.position.baseVertex);

  if(cfg.position.indexByteStride && cfg.position.indexResourceId != ResourceId())
    ib = m_pDriver->GetResourceManager()->GetCurrentResource(cfg.position.indexResourceId).name;

  const bool fandecode =
      (cfg.position.topology == Topology::TriangleFan && cfg.position.allowRestart);
  uint32_t numIndices = cfg.position.numIndices;

  // We copy into our own buffers to promote to the target type (uint32) that the shader expects.
  // Most IBs will be 16-bit indices, most VBs will not be float4. We also apply baseVertex here

  if(ib)
  {
    rdcarray<uint32_t> idxtmp;

    // if it's a triangle fan that allows restart, we'll have to unpack it.
    // Allocate enough space for the list on the GPU, and enough temporary space to upcast into
    // first
    if(fandecode)
    {
      idxtmp.resize(numIndices);

      numIndices *= 3;
    }

    // resize up on demand
    if(DebugData.pickIBBuf == 0 || DebugData.pickIBSize < numIndices * sizeof(uint32_t))
    {
      drv.glDeleteBuffers(1, &DebugData.pickIBBuf);

      drv.glGenBuffers(1, &DebugData.pickIBBuf);
      drv.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
      drv.glNamedBufferDataEXT(DebugData.pickIBBuf, numIndices * sizeof(uint32_t), NULL,
                               eGL_STREAM_DRAW);

      DebugData.pickIBSize = numIndices * sizeof(uint32_t);
    }

    byte *idxs = new byte[numIndices * cfg.position.indexByteStride];
    memset(idxs, 0, numIndices * cfg.position.indexByteStride);

    rdcarray<uint32_t> outidxs;
    outidxs.resize(numIndices);

    drv.glBindBuffer(eGL_COPY_READ_BUFFER, ib);

    GLuint bufsize = 0;
    drv.glGetBufferParameteriv(eGL_COPY_READ_BUFFER, eGL_BUFFER_SIZE, (GLint *)&bufsize);

    drv.glGetBufferSubData(eGL_COPY_READ_BUFFER, (GLintptr)cfg.position.indexByteOffset,
                           RDCMIN(uint32_t(bufsize) - uint32_t(cfg.position.indexByteOffset),
                                  cfg.position.numIndices * cfg.position.indexByteStride),
                           idxs);

    uint8_t *idxs8 = (uint8_t *)idxs;
    uint16_t *idxs16 = (uint16_t *)idxs;
    uint32_t *idxs32 = (uint32_t *)idxs;

    size_t idxcount = 0;

    if(cfg.position.indexByteStride == 1)
    {
      for(uint32_t i = 0; i < cfg.position.numIndices; i++)
      {
        uint32_t idx = idxs8[i];

        if(idx < idxclamp)
          idx = 0;
        else if(cfg.position.baseVertex < 0)
          idx -= idxclamp;
        else if(cfg.position.baseVertex > 0)
          idx += cfg.position.baseVertex;

        if(i == 0)
        {
          minIndex = maxIndex = idx;
        }
        else
        {
          minIndex = RDCMIN(idx, minIndex);
          maxIndex = RDCMAX(idx, maxIndex);
        }

        outidxs[i] = idx;
        idxcount++;
      }
    }
    else if(cfg.position.indexByteStride == 2)
    {
      for(uint32_t i = 0; i < cfg.position.numIndices; i++)
      {
        uint32_t idx = idxs16[i];

        if(idx < idxclamp)
          idx = 0;
        else if(cfg.position.baseVertex < 0)
          idx -= idxclamp;
        else if(cfg.position.baseVertex > 0)
          idx += cfg.position.baseVertex;

        if(i == 0)
        {
          minIndex = maxIndex = idx;
        }
        else
        {
          minIndex = RDCMIN(idx, minIndex);
          maxIndex = RDCMAX(idx, maxIndex);
        }

        outidxs[i] = idx;
        idxcount++;
      }
    }
    else
    {
      for(uint32_t i = 0; i < cfg.position.numIndices; i++)
      {
        uint32_t idx = idxs32[i];

        if(idx < idxclamp)
          idx = 0;
        else if(cfg.position.baseVertex < 0)
          idx -= idxclamp;
        else if(cfg.position.baseVertex > 0)
          idx += cfg.position.baseVertex;

        if(i == 0)
        {
          minIndex = maxIndex = idx;
        }
        else
        {
          minIndex = RDCMIN(idx, minIndex);
          maxIndex = RDCMAX(idx, maxIndex);
        }

        outidxs[i] = idx;
        idxcount++;
      }
    }

    // if it's a triangle fan that allows restart, unpack it
    if(cfg.position.topology == Topology::TriangleFan && cfg.position.allowRestart)
    {
      // resize to how many indices were actually read
      outidxs.resize(idxcount);

      // patch the index buffer
      PatchTriangleFanRestartIndexBufer(outidxs, cfg.position.restartIndex);

      for(uint32_t &idx : idxtmp)
      {
        if(idx == cfg.position.restartIndex)
          idx = 0;
      }

      numIndices = (uint32_t)outidxs.size();
    }

    drv.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickIBBuf);
    drv.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, numIndices * sizeof(uint32_t), outidxs.data());
  }

  // unpack and linearise the data
  {
    bytebuf oldData;
    GetBufferData(cfg.position.vertexResourceId, cfg.position.vertexByteOffset, 0, oldData);

    // clamp maxIndex to upper bound in case we got invalid indices or primitive restart indices
    maxIndex = RDCMIN(maxIndex, uint32_t(oldData.size() / RDCMAX(1U, cfg.position.vertexByteStride)));

    if(DebugData.pickVBBuf == 0 || DebugData.pickVBSize < (maxIndex + 1) * sizeof(Vec4f))
    {
      drv.glDeleteBuffers(1, &DebugData.pickVBBuf);

      drv.glGenBuffers(1, &DebugData.pickVBBuf);
      drv.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickVBBuf);
      drv.glNamedBufferDataEXT(DebugData.pickVBBuf, (maxIndex + 1) * sizeof(Vec4f), NULL,
                               eGL_DYNAMIC_DRAW);

      DebugData.pickVBSize = (maxIndex + 1) * sizeof(Vec4f);
    }

    rdcarray<FloatVector> vbData;
    vbData.resize(maxIndex + 1);

    byte *data = &oldData[0];
    byte *dataEnd = data + oldData.size();

    bool valid;

    // the index buffer may refer to vertices past the start of the vertex buffer, so we can't just
    // conver the first N vertices we'll need.
    // Instead we grab min and max above, and convert every vertex in that range. This might
    // slightly over-estimate but not as bad as 0-max or the whole buffer.
    for(uint32_t idx = minIndex; idx <= maxIndex; idx++)
      vbData[idx] = HighlightCache::InterpretVertex(data, idx, cfg.position.vertexByteStride,
                                                    cfg.position.format, dataEnd, valid);

    drv.glBindBuffer(eGL_SHADER_STORAGE_BUFFER, DebugData.pickVBBuf);
    drv.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, (maxIndex + 1) * sizeof(Vec4f), vbData.data());
  }

  drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
  MeshPickUBOData *cdata =
      (MeshPickUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(MeshPickUBOData),
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  if(!cdata)
  {
    RDCERR("Map buffer failed %d", drv.glGetError());
    return ~0U;
  }

  cdata->rayPos = rayPos;
  cdata->rayDir = rayDir;
  cdata->use_indices = cfg.position.indexByteStride ? 1U : 0U;
  cdata->numVerts = numIndices;
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
      if(fandecode)
        cdata->meshMode = MESH_TRIANGLE_LIST;
      else
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
  cdata->flipY = cfg.position.flipY;
  cdata->ortho = cfg.ortho;
  cdata->coords = Vec2f((float)x, (float)y);
  cdata->viewport = Vec2f((float)width, (float)height);

  if(cfg.position.unproject && isTriangleMesh)
  {
    // projected triangle meshes we transform the vertices into world space, and ray-cast against
    // that
    //
    // NOTE: for ortho, this matrix is not used and we just do the perspective W division on model
    // vertices. The ray is cast in NDC
    if(cfg.ortho)
      cdata->transformMat = Matrix4f::Identity();
    else
      cdata->transformMat = guessProjInverse;
  }
  else if(cfg.position.unproject)
  {
    // projected non-triangles are just point clouds, so we transform the vertices into world space
    // then project them back onto the output and compare that against the picking 2D co-ordinates
    cdata->transformMat = pickMVP.Mul(guessProjInverse);
  }
  else
  {
    // plain meshes of either type, we just transform from model space to the output, and raycast or
    // co-ordinate check
    cdata->transformMat = pickMVP;
  }

  drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  uint32_t reset[4] = {};
  drv.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.pickResultBuf);
  drv.glBufferSubData(eGL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t) * 4, &reset);

  drv.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 1, DebugData.pickVBBuf);
  drv.glBindBufferRange(eGL_SHADER_STORAGE_BUFFER, 2, DebugData.pickIBBuf, (GLintptr)0,
                        (GLsizeiptr)(sizeof(uint32_t) * cfg.position.numIndices));
  drv.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 3, DebugData.pickResultBuf);

  drv.glDispatchCompute(GLuint((cfg.position.numIndices) / 128 + 1), 1, 1);
  drv.glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

  uint32_t numResults = 0;

  drv.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.pickResultBuf);
  drv.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(uint32_t), &numResults);

  uint32_t ret = ~0U;

  if(numResults > 0)
  {
    if(isTriangleMesh)
    {
      struct PickResult
      {
        uint32_t vertid;
        vec3 intersectionPoint;
      };

      byte *mapped = (byte *)drv.glMapNamedBufferEXT(DebugData.pickResultBuf, eGL_READ_ONLY);

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

      drv.glUnmapNamedBufferEXT(DebugData.pickResultBuf);

      ret = closest->vertid;
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

      byte *mapped = (byte *)drv.glMapNamedBufferEXT(DebugData.pickResultBuf, eGL_READ_ONLY);

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

      drv.glUnmapNamedBufferEXT(DebugData.pickResultBuf);

      ret = closest->vertid;
    }
  }

  if(fandecode)
  {
    // undo the triangle list expansion
    if(ret > 2)
      ret = (ret + 3) / 3 + 1;
  }

  return ret;
}

void GLReplay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &drv = *m_pDriver;

  drv.glUseProgram(DebugData.checkerProg);

  drv.glDisable(eGL_DEPTH_TEST);

  if(HasExt[EXT_framebuffer_sRGB])
    drv.glEnable(eGL_FRAMEBUFFER_SRGB);

  drv.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);

  CheckerboardUBOData *ubo =
      (CheckerboardUBOData *)drv.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(CheckerboardUBOData),
                                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

  if(!ubo)
  {
    RDCERR("Map buffer failed %d", drv.glGetError());
    return;
  }

  ubo->BorderWidth = 0.0f;
  ubo->RectPosition = Vec2f();
  ubo->RectSize = Vec2f();
  ubo->CheckerSquareDimension = 64.0f;
  ubo->InnerColor = Vec4f();

  ubo->PrimaryColor = ConvertSRGBToLinear(dark);
  ubo->SecondaryColor = ConvertSRGBToLinear(light);

  drv.glUnmapBuffer(eGL_UNIFORM_BUFFER);

  drv.glBindVertexArray(DebugData.emptyVAO);
  drv.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
}

void GLReplay::RenderHighlightBox(float w, float h, float scale)
{
  MakeCurrentReplayContext(m_DebugCtx);

  WrappedOpenGL &drv = *m_pDriver;

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
  drv.glEnable(eGL_SCISSOR_TEST);
  drv.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  for(size_t i = 0; i < ARRAY_COUNT(scissors); i++)
  {
    drv.glScissor(scissors[i].x, scissors[i].y, scissors[i].w, scissors[i].h);
    drv.glClear(eGL_COLOR_BUFFER_BIT);
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
  drv.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  for(size_t i = 0; i < ARRAY_COUNT(scissors); i++)
  {
    drv.glScissor(scissors[i].x, scissors[i].y, scissors[i].w, scissors[i].h);
    drv.glClear(eGL_COLOR_BUFFER_BIT);
  }

  drv.glDisable(eGL_SCISSOR_TEST);
}
