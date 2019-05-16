/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "gl_common.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "gl_dispatch_table.h"
#include "gl_driver.h"

Threading::CriticalSection glLock;

GLDispatchTable GL = {};

GLChunk gl_CurChunk = GLChunk::Max;

bool HasExt[GLExtension_Count] = {};
bool VendorCheck[VendorCheck_Count] = {};

int GLCoreVersion = 0;
bool GLIsCore = false;
bool IsGLES = false;

template <>
bool CheckConstParam(bool t)
{
  return t;
}

bool CheckReplayContext()
{
#define REQUIRE_FUNC(func)                            \
  if(!GL.func)                                        \
  {                                                   \
    RDCERR("Missing core function " STRINGIZE(func)); \
    return false;                                     \
  }

  REQUIRE_FUNC(glGetString);
  REQUIRE_FUNC(glGetStringi);
  REQUIRE_FUNC(glGetIntegerv);

// we can't do without these extensions, but they should be present on any reasonable driver
// as they should have minimal or no hardware requirement. They were present on mesa 10.6
// for all drivers which dates to mid 2015.
#undef EXT_TO_CHECK
#define EXT_TO_CHECK(ver, glesver, ext) ext,
  enum
  {
    EXTENSION_CHECKS() ext_count,
  };
  bool exts[ext_count] = {};

  RDCLOG("Running GL replay on: %s / %s / %s", GL.glGetString(eGL_VENDOR),
         GL.glGetString(eGL_RENDERER), GL.glGetString(eGL_VERSION));

  std::string extensionString = "";

  GLint numExts = 0;
  GL.glGetIntegerv(eGL_NUM_EXTENSIONS, &numExts);
  for(GLint e = 0; e < numExts; e++)
  {
    const char *ext = (const char *)GL.glGetStringi(eGL_EXTENSIONS, (GLuint)e);

    extensionString += StringFormat::Fmt("[%d]: %s, ", e, ext);

    if(e > 0 && (e % 25) == 0)
    {
      RDCLOG("%s", extensionString.c_str());
      extensionString = "";
    }

    // skip the "GL_"
    ext += 3;

#undef EXT_TO_CHECK
#define EXT_TO_CHECK(ver, glesver, extname)                                       \
  if((!IsGLES && GLCoreVersion >= ver) || (IsGLES && GLCoreVersion >= glesver) || \
     !strcmp(ext, STRINGIZE(extname)))                                            \
    exts[extname] = true;

    EXTENSION_CHECKS()
  }

  if(!extensionString.empty())
    RDCLOG("%s", extensionString.c_str());

  return true;
}

bool ValidateFunctionPointers()
{
  PFNGLGETSTRINGPROC *ptrs = (PFNGLGETSTRINGPROC *)&GL;
  size_t num = sizeof(GL) / sizeof(PFNGLGETSTRINGPROC);

  RDCLOG("Function pointers available:");
  for(size_t ptr = 0; ptr < num;)
  {
    uint64_t ptrmask = 0;

    for(size_t j = 0; j < 64; j++)
      if(ptr + j < num && ptrs[ptr + j])
        ptrmask |= 1ULL << (63 - j);

    ptr += 64;

    RDCLOG("%064llb", ptrmask);
  }

  // check for the presence of GL functions we will call unconditionally as part of the replay
  // process.
  // Other functions that are only called to deserialise are checked for presence separately

  bool ret = true;

#define CHECK_PRESENT(func)                                                            \
  if(!GL.func)                                                                         \
  {                                                                                    \
    RDCERR(                                                                            \
        "Missing function %s, required for replay. RenderDoc requires a 3.2 context, " \
        "and a handful of extensions, see the Documentation.",                         \
        STRINGIZE(func));                                                              \
    ret = false;                                                                       \
  }

  // these functions should all be present as part of a 3.2 context plus the extensions we require,
  // but let's just be extra-careful

  // both GL and GLES, some of them are emulated
  CHECK_PRESENT(glActiveTexture)
  CHECK_PRESENT(glAttachShader)
  CHECK_PRESENT(glBeginQuery)
  CHECK_PRESENT(glBindAttribLocation)
  CHECK_PRESENT(glBindBuffer)
  CHECK_PRESENT(glBindBufferBase)
  CHECK_PRESENT(glBindBufferRange)
  CHECK_PRESENT(glBindFramebuffer)
  CHECK_PRESENT(glBindProgramPipeline)
  CHECK_PRESENT(glBindSampler)
  CHECK_PRESENT(glBindTexture)
  CHECK_PRESENT(glBindVertexArray)
  CHECK_PRESENT(glBindVertexBuffer)
  CHECK_PRESENT(glBlendColor)
  CHECK_PRESENT(glBlendEquationSeparate)
  CHECK_PRESENT(glBlendFunc)
  CHECK_PRESENT(glBlendFuncSeparate)
  CHECK_PRESENT(glBlitFramebuffer)
  CHECK_PRESENT(glBufferData)
  CHECK_PRESENT(glBufferSubData)
  CHECK_PRESENT(glClearBufferData)
  CHECK_PRESENT(glClearBufferfi)
  CHECK_PRESENT(glClearBufferfv)
  CHECK_PRESENT(glClearBufferiv)
  CHECK_PRESENT(glClearBufferuiv)
  CHECK_PRESENT(glClearColor)
  CHECK_PRESENT(glClearDepthf)
  CHECK_PRESENT(glCompileShader)
  CHECK_PRESENT(glCopyImageSubData)
  CHECK_PRESENT(glCreateProgram)
  CHECK_PRESENT(glCreateShader)
  CHECK_PRESENT(glCreateShaderProgramv)
  CHECK_PRESENT(glCullFace)
  CHECK_PRESENT(glDeleteBuffers)
  CHECK_PRESENT(glDeleteFramebuffers)
  CHECK_PRESENT(glDeleteProgram)
  CHECK_PRESENT(glDeleteProgramPipelines)
  CHECK_PRESENT(glDeleteQueries)
  CHECK_PRESENT(glDeleteSamplers)
  CHECK_PRESENT(glDeleteShader)
  CHECK_PRESENT(glDeleteTextures)
  CHECK_PRESENT(glDeleteVertexArrays)
  CHECK_PRESENT(glDepthFunc)
  CHECK_PRESENT(glDepthMask)
  CHECK_PRESENT(glDetachShader)
  CHECK_PRESENT(glDisable)
  CHECK_PRESENT(glDisableVertexAttribArray)
  CHECK_PRESENT(glDrawArrays)
  CHECK_PRESENT(glDrawArraysInstanced)
  CHECK_PRESENT(glDrawBuffers)
  CHECK_PRESENT(glDrawElements)
  CHECK_PRESENT(glDrawElementsBaseVertex)
  CHECK_PRESENT(glEnable)
  CHECK_PRESENT(glEnableVertexAttribArray)
  CHECK_PRESENT(glEndQuery)
  CHECK_PRESENT(glFramebufferTexture2D)
  CHECK_PRESENT(glFramebufferTextureLayer)
  CHECK_PRESENT(glFrontFace)
  CHECK_PRESENT(glGenBuffers)
  CHECK_PRESENT(glGenFramebuffers)
  CHECK_PRESENT(glGenProgramPipelines)
  CHECK_PRESENT(glGenQueries)
  CHECK_PRESENT(glGenSamplers)
  CHECK_PRESENT(glGenTextures)
  CHECK_PRESENT(glGenVertexArrays)
  CHECK_PRESENT(glGetActiveUniformBlockiv)
  CHECK_PRESENT(glGetAttribLocation)
  CHECK_PRESENT(glGetBooleani_v)
  CHECK_PRESENT(glGetBooleanv)
  CHECK_PRESENT(glGetBufferParameteriv)
  CHECK_PRESENT(glGetBufferSubData)
  CHECK_PRESENT(glGetError)
  CHECK_PRESENT(glGetFloatv)
  CHECK_PRESENT(glGetFragDataLocation)
  CHECK_PRESENT(glGetFramebufferAttachmentParameteriv)
  CHECK_PRESENT(glGetInteger64i_v)
  CHECK_PRESENT(glGetIntegeri_v)
  CHECK_PRESENT(glGetIntegerv)
  CHECK_PRESENT(glGetInternalformativ)
  CHECK_PRESENT(glGetProgramInfoLog)
  CHECK_PRESENT(glGetProgramInterfaceiv)
  CHECK_PRESENT(glGetProgramiv)
  CHECK_PRESENT(glGetProgramPipelineiv)
  CHECK_PRESENT(glGetProgramResourceIndex)
  CHECK_PRESENT(glGetProgramResourceiv)
  CHECK_PRESENT(glGetProgramResourceName)
  CHECK_PRESENT(glGetQueryObjectuiv)
  CHECK_PRESENT(glGetSamplerParameterfv)
  CHECK_PRESENT(glGetSamplerParameteriv)
  CHECK_PRESENT(glGetShaderInfoLog)
  CHECK_PRESENT(glGetShaderiv)
  CHECK_PRESENT(glGetString)
  CHECK_PRESENT(glGetStringi)
  CHECK_PRESENT(glGetTexImage)
  CHECK_PRESENT(glGetTexLevelParameteriv)
  CHECK_PRESENT(glGetTexParameterfv)
  CHECK_PRESENT(glGetTexParameteriv)
  CHECK_PRESENT(glGetUniformBlockIndex)
  CHECK_PRESENT(glGetUniformfv)
  CHECK_PRESENT(glGetUniformiv)
  CHECK_PRESENT(glGetUniformLocation)
  CHECK_PRESENT(glGetUniformuiv)
  CHECK_PRESENT(glGetVertexAttribfv)
  CHECK_PRESENT(glGetVertexAttribiv)
  CHECK_PRESENT(glHint)
  CHECK_PRESENT(glIsEnabled)
  CHECK_PRESENT(glLineWidth)
  CHECK_PRESENT(glLinkProgram)
  CHECK_PRESENT(glMapBufferRange)
  CHECK_PRESENT(glPixelStorei)
  CHECK_PRESENT(glPolygonOffset)
  CHECK_PRESENT(glProgramParameteri)
  CHECK_PRESENT(glProgramUniform1fv)
  CHECK_PRESENT(glProgramUniform1iv)
  CHECK_PRESENT(glProgramUniform1ui)
  CHECK_PRESENT(glProgramUniform1uiv)
  CHECK_PRESENT(glProgramUniform2fv)
  CHECK_PRESENT(glProgramUniform2iv)
  CHECK_PRESENT(glProgramUniform2uiv)
  CHECK_PRESENT(glProgramUniform3fv)
  CHECK_PRESENT(glProgramUniform3iv)
  CHECK_PRESENT(glProgramUniform3uiv)
  CHECK_PRESENT(glProgramUniform4fv)
  CHECK_PRESENT(glProgramUniform4iv)
  CHECK_PRESENT(glProgramUniform4ui)
  CHECK_PRESENT(glProgramUniform4uiv)
  CHECK_PRESENT(glProgramUniformMatrix2fv)
  CHECK_PRESENT(glProgramUniformMatrix2x3fv)
  CHECK_PRESENT(glProgramUniformMatrix2x4fv)
  CHECK_PRESENT(glProgramUniformMatrix3fv)
  CHECK_PRESENT(glProgramUniformMatrix3x2fv)
  CHECK_PRESENT(glProgramUniformMatrix3x4fv)
  CHECK_PRESENT(glProgramUniformMatrix4fv)
  CHECK_PRESENT(glProgramUniformMatrix4x2fv)
  CHECK_PRESENT(glProgramUniformMatrix4x3fv)
  CHECK_PRESENT(glReadBuffer)
  CHECK_PRESENT(glReadPixels)
  CHECK_PRESENT(glSampleCoverage)
  CHECK_PRESENT(glSampleMaski)
  CHECK_PRESENT(glSamplerParameteri)
  CHECK_PRESENT(glShaderSource)
  CHECK_PRESENT(glStencilFuncSeparate)
  CHECK_PRESENT(glStencilMask)
  CHECK_PRESENT(glStencilMaskSeparate)
  CHECK_PRESENT(glStencilOpSeparate)
  CHECK_PRESENT(glTexImage2D)
  CHECK_PRESENT(glTexParameteri)
  CHECK_PRESENT(glUniform1i)
  CHECK_PRESENT(glUniform1ui)
  CHECK_PRESENT(glUniform2f)
  CHECK_PRESENT(glUniform2fv)
  CHECK_PRESENT(glUniform4fv)
  CHECK_PRESENT(glUniformBlockBinding)
  CHECK_PRESENT(glUniformMatrix4fv)
  CHECK_PRESENT(glUnmapBuffer)
  CHECK_PRESENT(glUseProgram)
  CHECK_PRESENT(glUseProgramStages)
  CHECK_PRESENT(glVertexAttrib4fv)
  CHECK_PRESENT(glVertexAttribBinding)
  CHECK_PRESENT(glVertexAttribFormat)
  CHECK_PRESENT(glVertexAttribIFormat)
  CHECK_PRESENT(glVertexAttribPointer)
  CHECK_PRESENT(glVertexBindingDivisor)
  CHECK_PRESENT(glViewport)

  // GL only
  if(!IsGLES)
  {
    CHECK_PRESENT(glBindFragDataLocation)
    CHECK_PRESENT(glEndConditionalRender)
    CHECK_PRESENT(glFramebufferTexture3D)
    CHECK_PRESENT(glGetCompressedTexImage)
    CHECK_PRESENT(glGetDoublev)
    CHECK_PRESENT(glGetUniformdv)
    CHECK_PRESENT(glLogicOp)
    CHECK_PRESENT(glPointParameterf)
    CHECK_PRESENT(glPointParameteri)
    CHECK_PRESENT(glPointSize)
    CHECK_PRESENT(glPolygonMode)
    CHECK_PRESENT(glPrimitiveRestartIndex)
    CHECK_PRESENT(glProgramUniform1dv)
    CHECK_PRESENT(glProgramUniform2dv)
    CHECK_PRESENT(glProgramUniform3dv)
    CHECK_PRESENT(glProgramUniform4dv)
    CHECK_PRESENT(glProgramUniformMatrix2dv)
    CHECK_PRESENT(glProgramUniformMatrix2x3dv)
    CHECK_PRESENT(glProgramUniformMatrix2x4dv)
    CHECK_PRESENT(glProgramUniformMatrix3dv)
    CHECK_PRESENT(glProgramUniformMatrix3x2dv)
    CHECK_PRESENT(glProgramUniformMatrix3x4dv)
    CHECK_PRESENT(glProgramUniformMatrix4dv)
    CHECK_PRESENT(glProgramUniformMatrix4x2dv)
    CHECK_PRESENT(glProgramUniformMatrix4x3dv)
    CHECK_PRESENT(glProvokingVertex)
    CHECK_PRESENT(glVertexAttribLFormat)
  }

  // other functions are either checked for presence explicitly (like
  // depth bounds or polygon offset clamp EXT functions), or they are
  // only called when such a call is serialised from the logfile, and
  // so they are checked for validity separately.

  return ret;
}

static void CheckExtFromString(const char *ext)
{
  if(ext == NULL || !ext[0] || !ext[1] || !ext[2] || !ext[3])
    return;

  ext += 3;

#undef EXT_TO_CHECK
#define EXT_TO_CHECK(ver, glesver, extname)                                       \
  if((!IsGLES && GLCoreVersion >= ver) || (IsGLES && GLCoreVersion >= glesver) || \
     !strcmp(ext, STRINGIZE(extname)))                                            \
    HasExt[extname] = true;

  EXTENSION_CHECKS()

  if(IsGLES)
  {
#define EXT_COMP_CHECK(extname, glesextname) \
  if(!strcmp(ext, STRINGIZE(glesextname)))   \
    HasExt[extname] = true;

    EXTENSION_COMPATIBILITY_CHECKS()
  }
}

void GetContextVersion(bool &ctxGLES, int &ctxVersion)
{
  if(GL.glGetString)
  {
    const char *version = (const char *)GL.glGetString(eGL_VERSION);

    // check whether we are using OpenGL ES
    // GL_VERSION for OpenGL ES:
    //   "OpenGL ES N.M vendor-specific information"
    // for 1.x:
    //   "OpenGL ES-XX N.M vendor-specific information"
    if(!strncmp(version, "OpenGL ES-", 10))
    {
      ctxGLES = true;

      // assume 1.0, doesn't matter if it's 1.1
      ctxVersion = 10;
    }
    else if(!strncmp(version, "OpenGL ES", 9))
    {
      ctxGLES = true;

      int mj = int(version[10] - '0');
      int mn = int(version[12] - '0');
      ctxVersion = mj * 10 + mn;
    }
    else
    {
      ctxGLES = false;

      int mj = int(version[0] - '0');
      int mn = int(version[1] - '0');
      ctxVersion = mj * 10 + mn;
    }
  }

  if(GL.glGetIntegerv)
  {
    GLint mj = 0, mn = 0;
    GL.glGetIntegerv(eGL_MAJOR_VERSION, &mj);
    GL.glGetIntegerv(eGL_MINOR_VERSION, &mn);

    if(mj > 0)
      ctxVersion = mj * 10 + mn;
  }
}

void FetchEnabledExtensions()
{
  RDCEraseEl(HasExt);

  int ctxVersion = 0;
  bool ctxGLES = false;
  GetContextVersion(ctxGLES, ctxVersion);

  GLCoreVersion = RDCMAX(GLCoreVersion, ctxVersion);
  IsGLES = ctxGLES;

  RDCLOG("Checking enabled extensions, running as %s %d.%d", IsGLES ? "OpenGL ES" : "OpenGL",
         (ctxVersion / 10), (ctxVersion % 10));

  // only use glGetStringi on 3.0 contexts and above (ES and GL), even if we have the function
  // pointer
  if(GL.glGetStringi && ctxVersion >= 30)
  {
    GLint numExts = 0;
    if(GL.glGetIntegerv)
      GL.glGetIntegerv(eGL_NUM_EXTENSIONS, &numExts);

    for(int i = 0; i < numExts; i++)
    {
      const char *ext = (const char *)GL.glGetStringi(eGL_EXTENSIONS, (GLuint)i);

      CheckExtFromString(ext);
    }
  }
  else if(GL.glGetString)
  {
    std::string extstr = (const char *)GL.glGetString(eGL_EXTENSIONS);

    std::vector<std::string> extlist;
    split(extstr, extlist, ' ');

    for(const std::string &e : extlist)
      CheckExtFromString(e.c_str());
  }

  if(!HasExt[ARB_separate_shader_objects])
  {
    if(HasExt[ARB_program_interface_query])
      RDCWARN(
          "Because ARB_separate_shader_objects is not supported, forcibly disabling "
          "ARB_program_interface_query");

    HasExt[ARB_program_interface_query] = false;
  }
}

void DoVendorChecks(GLPlatform &platform, GLWindowingData context)
{
  const char *vendor = "";
  const char *renderer = "";

  if(GL.glGetString)
  {
    vendor = (const char *)GL.glGetString(eGL_VENDOR);
    renderer = (const char *)GL.glGetString(eGL_RENDERER);
    const char *version = (const char *)GL.glGetString(eGL_VERSION);

    RDCLOG("Vendor checks for %u (%s / %s / %s)", GLCoreVersion, vendor, renderer, version);
  }

  //////////////////////////////////////////////////////////
  // version/driver/vendor specific hacks and checks go here
  // doing these in a central place means they're all documented and
  // can be removed ASAP from a single place.
  // It also means any work done to figure them out is only ever done
  // in one place, when first activating a new context, so hopefully
  // shouldn't interfere with the running program

  // The linux AMD driver doesn't recognise GL_VERTEX_BINDING_BUFFER.
  // However it has a "two wrongs make a right" type deal. Instead of returning the buffer that the
  // i'th index is bound to (as above, vbslot) for GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, it returns
  // the i'th
  // vertex buffer which is exactly what we wanted from GL_VERTEX_BINDING_BUFFER!
  // see: http://devgurus.amd.com/message/1306745#1306745

  RDCEraseEl(VendorCheck);

  if(GL.glGetError && GL.glGetIntegeri_v && HasExt[ARB_vertex_attrib_binding])
  {
    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors();

    GLint dummy = 0;
    GL.glGetIntegeri_v(eGL_VERTEX_BINDING_BUFFER, 0, &dummy);
    err = GL.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_vertex_buffer_query] = true;

      RDCWARN("Using AMD hack to avoid GL_VERTEX_BINDING_BUFFER");
    }
  }

  if(GL.glGetIntegerv && GL.glGetError && !IsGLES)
  {
    // NOTE: in case of OpenGL ES the GL_NV_polygon_mode extension can be used, however even if the
    // driver reports that the extension is supported, it always throws errors when we try to use it
    // (at least with the current NVIDIA driver)

    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors();

    GLint dummy[2] = {0};
    GL.glGetIntegerv(eGL_POLYGON_MODE, dummy);
    err = GL.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_polygon_mode_query] = true;

      RDCWARN("Using AMD hack to avoid GL_POLYGON_MODE");
    }
  }

  // AMD throws an error if we try to copy the mips that are smaller than 4x4.
  //
  // Intel seems to completely break everything if we even run this check, so we just
  // skip this check and assume the hack is enabled.

  if(!strcmp(vendor, "Intel") || !strcmp(vendor, "intel") || !strcmp(vendor, "INTEL"))
  {
    RDCWARN("Using super hack-on-a-hack to avoid glCopyImageSubData tests on intel.");
    VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] = true;
    VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] = true;
  }
  else if(GL.glGetError && GL.glGenTextures && GL.glBindTexture && GL.glCopyImageSubData &&
          GL.glTexStorage2D && GL.glTexSubImage2D && GL.glTexParameteri && GL.glDeleteTextures &&
          HasExt[ARB_copy_image] && HasExt[ARB_texture_storage] && !IsGLES)
  {
    GLuint prevTex = 0;
    GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&prevTex);

    GLuint texs[2];
    GL.glGenTextures(2, texs);

    GL.glBindTexture(eGL_TEXTURE_2D, texs[0]);
    GL.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1, 1);
    GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    GL.glBindTexture(eGL_TEXTURE_2D, texs[1]);
    GL.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1, 1);
    GL.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors();

    GL.glCopyImageSubData(texs[0], eGL_TEXTURE_2D, 0, 0, 0, 0, texs[1], eGL_TEXTURE_2D, 0, 0, 0, 0,
                          1, 1, 1);

    err = GL.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] = true;

      RDCWARN("Using hack to avoid glCopyImageSubData on lowest mips of compressed texture");
    }

    GL.glBindTexture(eGL_TEXTURE_2D, prevTex);
    GL.glDeleteTextures(2, texs);

    ClearGLErrors();

    //////////////////////////////////////////////////////////////////////////
    // Check copying cubemaps

    GL.glGetIntegerv(eGL_TEXTURE_BINDING_CUBE_MAP, (GLint *)&prevTex);
    GL.glGenTextures(2, texs);

    const size_t dim = 32;

    char buf[dim * dim / 2];

    GL.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[0]);
    GL.glTexStorage2D(eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim, dim);
    GL.glTexParameteri(eGL_TEXTURE_CUBE_MAP, eGL_TEXTURE_MAX_LEVEL, 0);

    for(int i = 0; i < 6; i++)
    {
      memset(buf, 0xba + i, sizeof(buf));
      GL.glCompressedTexSubImage2D(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, 0, 0, dim, dim,
                                   eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim * dim / 2, buf);
    }

    GL.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[1]);
    GL.glTexStorage2D(eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim, dim);
    GL.glTexParameteri(eGL_TEXTURE_CUBE_MAP, eGL_TEXTURE_MAX_LEVEL, 0);

    GL.glCopyImageSubData(texs[0], eGL_TEXTURE_CUBE_MAP, 0, 0, 0, 0, texs[1], eGL_TEXTURE_CUBE_MAP,
                          0, 0, 0, 0, dim, dim, 6);

    char cmp[dim * dim / 2];

    GL.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[0]);

    for(int i = 0; i < 6; i++)
    {
      memset(buf, 0xba + i, sizeof(buf));
      RDCEraseEl(cmp);
      GL.glGetCompressedTexImage(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, cmp);

      RDCCOMPILE_ASSERT(sizeof(buf) == sizeof(buf), "Buffers are not matching sizes");

      if(memcmp(buf, cmp, sizeof(buf)))
      {
        RDCERR("glGetTexImage from the source texture returns incorrect data!");
        VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] =
            true;    // to be safe, enable the hack
      }
    }

    GL.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[1]);

    for(int i = 0; i < 6; i++)
    {
      memset(buf, 0xba + i, sizeof(buf));
      RDCEraseEl(cmp);
      GL.glGetCompressedTexImage(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, cmp);

      RDCCOMPILE_ASSERT(sizeof(buf) == sizeof(buf), "Buffers are not matching sizes");

      if(memcmp(buf, cmp, sizeof(buf)))
      {
        RDCWARN("Using hack to avoid glCopyImageSubData on cubemap textures");
        VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] = true;
        break;
      }
    }

    GL.glBindTexture(eGL_TEXTURE_CUBE_MAP, prevTex);
    GL.glDeleteTextures(2, texs);

    ClearGLErrors();
  }

  if(GL.glGetError && GL.glGenProgramPipelines && GL.glDeleteProgramPipelines &&
     GL.glGetProgramPipelineiv && HasExt[ARB_compute_shader] && HasExt[ARB_program_interface_query])
  {
    GLuint pipe = 0;
    GL.glGenProgramPipelines(1, &pipe);

    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors();

    GLint dummy = 0;
    GL.glGetProgramPipelineiv(pipe, eGL_COMPUTE_SHADER, &dummy);

    err = GL.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_pipeline_compute_query] = true;

      RDCWARN("Using hack to avoid glGetProgramPipelineiv with GL_COMPUTE_SHADER");
    }

    GL.glDeleteProgramPipelines(1, &pipe);
  }

  // only do this when we have a proper context e.g. on windows where an old
  // context is first created. Check to see if FBOs or VAOs are shared between
  // contexts.
  if((IsGLES || GLCoreVersion >= 32) && GL.glGenVertexArrays && GL.glBindVertexArray &&
     GL.glDeleteVertexArrays && GL.glGenFramebuffers && GL.glBindFramebuffer &&
     GL.glDeleteFramebuffers)
  {
    GLuint prevFBO = 0, prevVAO = 0;
    GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&prevFBO);
    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&prevVAO);

    // gen & create an FBO and VAO
    GLuint fbo = 0;
    GLuint vao = 0;
    GL.glGenFramebuffers(1, &fbo);
    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbo);
    GL.glGenVertexArrays(1, &vao);
    GL.glBindVertexArray(vao);

    // make a context that shares with the current one, and switch to it
    GLWindowingData child = platform.CloneTemporaryContext(context);

    if(child.ctx)
    {
      // switch to child
      platform.MakeContextCurrent(child);

      // these shouldn't be visible
      VendorCheck[VendorCheck_EXT_fbo_shared] = (GL.glIsFramebuffer(fbo) != GL_FALSE);
      VendorCheck[VendorCheck_EXT_vao_shared] = (GL.glIsVertexArray(vao) != GL_FALSE);

      if(VendorCheck[VendorCheck_EXT_fbo_shared])
        RDCWARN("FBOs are shared on this implementation");
      if(VendorCheck[VendorCheck_EXT_vao_shared])
        RDCWARN("VAOs are shared on this implementation");

      // switch back to context
      platform.MakeContextCurrent(context);

      platform.DeleteClonedContext(child);
    }

    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, prevFBO);
    GL.glBindVertexArray(prevVAO);

    GL.glDeleteFramebuffers(1, &fbo);
    GL.glDeleteVertexArrays(1, &vao);
  }

  // don't have a test for this, just have to enable it all the time, for now.
  VendorCheck[VendorCheck_NV_avoid_D32S8_copy] = true;

  // On 32-bit calling this function could actually lead to crashes (issues with
  // esp being saved across the call), so since the work-around is low-cost of just
  // emulating that function we just always enable it.
  //
  // NOTE: Vendor Checks are initialised after the function pointers will be set up
  // so we have to do this unconditionally, this value isn't checked anywhere.
  // Search for where this is applied in gl_emulated.cpp
  //
  // Update 2018-Jan - this might be the problem with the registry having the wrong signature for
  // glClearNamedFramebufferfi - if the arguments were mismatched it would explain both invalid
  // argument errors and ABI problems. For now though (and since as mentioned above it's cheap to
  // emulate) we leave it on. See issue #842
  VendorCheck[VendorCheck_NV_ClearNamedFramebufferfiBugs] = true;

  // glVertexArrayElementBuffer doesn't update the GL_ELEMENT_ARRAY_BUFFER_BINDING global query,
  // when binding the VAO subsequently *will*.
  // I'm not sure if that's correct (weird) behaviour or buggy, but we can work around it just by
  // avoiding use of the DSA function and always doing our emulated version.
  VendorCheck[VendorCheck_AMD_vertex_array_elem_buffer_query] = true;

  // Qualcomm's implementation of glCopyImageSubData is buggy on some drivers and can cause GPU
  // crashes or corrupted data. We force the initial state copies to happen via our emulation which
  // uses framebuffer blits.
  if(strstr(vendor, "Qualcomm") || strstr(vendor, "Adreno") || strstr(renderer, "Qualcomm") ||
     strstr(vendor, "Adreno"))
  {
    RDCWARN("Using hack to avoid glCopyImageSubData on Qualcomm");

    VendorCheck[VendorCheck_Qualcomm_avoid_glCopyImageSubData] = true;
  }

  if(IsGLES)
  {
    // Check whether reading from the depth, stencil and depth-stencil buffers using glReadPixels is
    // supported or not.
    if(!HasExt[NV_read_depth])
      RDCWARN(
          "Reading from the depth buffer using glReadPixels is not supported (GL_NV_read_depth)");
    if(!HasExt[NV_read_stencil])
      RDCWARN(
          "Reading from the stencil buffer using glReadPixels is not supported "
          "(GL_NV_read_stencil)");
    if(!HasExt[NV_read_depth_stencil])
      RDCWARN(
          "Reading from the packed depth-stencil buffers using glReadPixels is not supported "
          "(GL_NV_read_depth_stencil)");
  }
}

GLMarkerRegion::GLMarkerRegion(const std::string &marker, GLenum source, GLuint id)
{
  Begin(marker, source, id);
}

GLMarkerRegion::~GLMarkerRegion()
{
  End();
}

void GLMarkerRegion::Begin(const std::string &marker, GLenum source, GLuint id)
{
  if(!HasExt[KHR_debug] || !GL.glPushDebugGroup)
    return;

  GL.glPushDebugGroup(source, id, -1, marker.c_str());
}

void GLMarkerRegion::Set(const std::string &marker, GLenum source, GLuint id, GLenum severity)
{
  if(!HasExt[KHR_debug] || !GL.glDebugMessageInsert)
    return;

  GL.glDebugMessageInsert(source, eGL_DEBUG_TYPE_MARKER, id, severity, -1, marker.c_str());
}

void GLMarkerRegion::End()
{
  if(!HasExt[KHR_debug] || !GL.glPopDebugGroup)
    return;

  GL.glPopDebugGroup();
}

void GLPushPopState::Push(bool modern)
{
  enableBits[0] = GL.glIsEnabled(eGL_DEPTH_TEST) != 0;
  enableBits[1] = GL.glIsEnabled(eGL_STENCIL_TEST) != 0;
  enableBits[2] = GL.glIsEnabled(eGL_CULL_FACE) != 0;
  if(modern)
  {
    if(!IsGLES)
      enableBits[3] = GL.glIsEnabled(eGL_DEPTH_CLAMP) != 0;

    if(HasExt[ARB_draw_buffers_blend])
      enableBits[4] = GL.glIsEnabledi(eGL_BLEND, 0) != 0;
    else
      enableBits[4] = GL.glIsEnabled(eGL_BLEND) != 0;

    if(HasExt[ARB_viewport_array])
      enableBits[5] = GL.glIsEnabledi(eGL_SCISSOR_TEST, 0) != 0;
    else
      enableBits[5] = GL.glIsEnabled(eGL_SCISSOR_TEST) != 0;

    if(HasExt[EXT_transform_feedback])
      enableBits[6] = GL.glIsEnabled(eGL_RASTERIZER_DISCARD) != 0;
  }
  else
  {
    enableBits[3] = GL.glIsEnabled(eGL_BLEND) != 0;
    enableBits[4] = GL.glIsEnabled(eGL_SCISSOR_TEST) != 0;
    enableBits[5] = GL.glIsEnabled(eGL_TEXTURE_2D) != 0;
    enableBits[6] = GL.glIsEnabled(eGL_LIGHTING) != 0;
    enableBits[7] = GL.glIsEnabled(eGL_ALPHA_TEST) != 0;
  }

  if(modern && HasExt[ARB_clip_control])
  {
    GL.glGetIntegerv(eGL_CLIP_ORIGIN, (GLint *)&ClipOrigin);
    GL.glGetIntegerv(eGL_CLIP_DEPTH_MODE, (GLint *)&ClipDepth);
  }
  else
  {
    ClipOrigin = eGL_LOWER_LEFT;
    ClipDepth = eGL_NEGATIVE_ONE_TO_ONE;
  }

  if(modern && HasExt[ARB_draw_buffers_blend])
  {
    GL.glGetIntegeri_v(eGL_BLEND_EQUATION_RGB, 0, (GLint *)&EquationRGB);
    GL.glGetIntegeri_v(eGL_BLEND_EQUATION_ALPHA, 0, (GLint *)&EquationAlpha);

    GL.glGetIntegeri_v(eGL_BLEND_SRC_RGB, 0, (GLint *)&SourceRGB);
    GL.glGetIntegeri_v(eGL_BLEND_SRC_ALPHA, 0, (GLint *)&SourceAlpha);

    GL.glGetIntegeri_v(eGL_BLEND_DST_RGB, 0, (GLint *)&DestinationRGB);
    GL.glGetIntegeri_v(eGL_BLEND_DST_ALPHA, 0, (GLint *)&DestinationAlpha);
  }
  else
  {
    GL.glGetIntegerv(eGL_BLEND_EQUATION_RGB, (GLint *)&EquationRGB);
    GL.glGetIntegerv(eGL_BLEND_EQUATION_ALPHA, (GLint *)&EquationAlpha);

    GL.glGetIntegerv(eGL_BLEND_SRC_RGB, (GLint *)&SourceRGB);
    GL.glGetIntegerv(eGL_BLEND_SRC_ALPHA, (GLint *)&SourceAlpha);

    GL.glGetIntegerv(eGL_BLEND_DST_RGB, (GLint *)&DestinationRGB);
    GL.glGetIntegerv(eGL_BLEND_DST_ALPHA, (GLint *)&DestinationAlpha);
  }

  if(modern && (HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend]))
  {
    GL.glGetBooleani_v(eGL_COLOR_WRITEMASK, 0, ColorMask);
  }
  else
  {
    GL.glGetBooleanv(eGL_COLOR_WRITEMASK, ColorMask);
  }

  if(!VendorCheck[VendorCheck_AMD_polygon_mode_query] && !IsGLES)
  {
    GLenum dummy[2] = {eGL_FILL, eGL_FILL};
    // docs suggest this is enumeration[2] even though polygon mode can't be set independently for
    // front and back faces.
    GL.glGetIntegerv(eGL_POLYGON_MODE, (GLint *)&dummy);
    PolygonMode = dummy[0];
  }
  else
  {
    PolygonMode = eGL_FILL;
  }

  if(modern && HasExt[ARB_viewport_array])
    GL.glGetFloati_v(eGL_VIEWPORT, 0, &Viewportf[0]);
  else
    GL.glGetIntegerv(eGL_VIEWPORT, &Viewport[0]);

  GL.glGetIntegerv(eGL_ACTIVE_TEXTURE, (GLint *)&ActiveTexture);
  GL.glActiveTexture(eGL_TEXTURE0);
  GL.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint *)&tex0);

  GL.glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint *)&arraybuf);

  // we get the current program but only try to restore it if it's non-0
  prog = 0;
  if(modern)
    GL.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint *)&prog);

  drawFBO = 0;
  GL.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&drawFBO);

  // since we will use the fixed function pipeline, also need to check for program pipeline
  // bindings (if we weren't, our program would override)
  pipe = 0;
  if(modern && HasExt[ARB_separate_shader_objects])
    GL.glGetIntegerv(eGL_PROGRAM_PIPELINE_BINDING, (GLint *)&pipe);

  if(modern)
  {
    // the non-indexed bind is separate from the indexed binds
    GL.glGetIntegerv(eGL_UNIFORM_BUFFER_BINDING, (GLint *)&ubo);

    for(size_t i = 0; i < ARRAY_COUNT(idxubo); i++)
    {
      GL.glGetIntegeri_v(eGL_UNIFORM_BUFFER_BINDING, (GLuint)i, (GLint *)&idxubo[i].buf);
      GL.glGetInteger64i_v(eGL_UNIFORM_BUFFER_START, (GLuint)i, (GLint64 *)&idxubo[i].offs);
      GL.glGetInteger64i_v(eGL_UNIFORM_BUFFER_SIZE, (GLuint)i, (GLint64 *)&idxubo[i].size);
    }

    GL.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint *)&VAO);
  }

  ClearGLErrors();
}

void GLPushPopState::Pop(bool modern)
{
  if(enableBits[0])
    GL.glEnable(eGL_DEPTH_TEST);
  else
    GL.glDisable(eGL_DEPTH_TEST);
  if(enableBits[1])
    GL.glEnable(eGL_STENCIL_TEST);
  else
    GL.glDisable(eGL_STENCIL_TEST);
  if(enableBits[2])
    GL.glEnable(eGL_CULL_FACE);
  else
    GL.glDisable(eGL_CULL_FACE);

  if(modern)
  {
    if(!IsGLES)
    {
      if(enableBits[3])
        GL.glEnable(eGL_DEPTH_CLAMP);
      else
        GL.glDisable(eGL_DEPTH_CLAMP);
    }

    if(HasExt[ARB_draw_buffers_blend])
    {
      if(enableBits[4])
        GL.glEnablei(eGL_BLEND, 0);
      else
        GL.glDisablei(eGL_BLEND, 0);
    }
    else
    {
      if(enableBits[4])
        GL.glEnable(eGL_BLEND);
      else
        GL.glDisable(eGL_BLEND);
    }

    if(HasExt[ARB_viewport_array])
    {
      if(enableBits[5])
        GL.glEnablei(eGL_SCISSOR_TEST, 0);
      else
        GL.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      if(enableBits[5])
        GL.glEnable(eGL_SCISSOR_TEST);
      else
        GL.glDisable(eGL_SCISSOR_TEST);
    }

    if(HasExt[EXT_transform_feedback])
    {
      if(enableBits[6])
        GL.glEnable(eGL_RASTERIZER_DISCARD);
      else
        GL.glDisable(eGL_RASTERIZER_DISCARD);
    }
  }
  else
  {
    if(enableBits[3])
      GL.glEnable(eGL_BLEND);
    else
      GL.glDisable(eGL_BLEND);
    if(enableBits[4])
      GL.glEnable(eGL_SCISSOR_TEST);
    else
      GL.glDisable(eGL_SCISSOR_TEST);
    if(enableBits[5])
      GL.glEnable(eGL_TEXTURE_2D);
    else
      GL.glDisable(eGL_TEXTURE_2D);
    if(enableBits[6])
      GL.glEnable(eGL_LIGHTING);
    else
      GL.glDisable(eGL_LIGHTING);
    if(enableBits[7])
      GL.glEnable(eGL_ALPHA_TEST);
    else
      GL.glDisable(eGL_ALPHA_TEST);
  }

  if(modern && GL.glClipControl && HasExt[ARB_clip_control])
    GL.glClipControl(ClipOrigin, ClipDepth);

  if(modern && HasExt[ARB_draw_buffers_blend])
  {
    GL.glBlendFuncSeparatei(0, SourceRGB, DestinationRGB, SourceAlpha, DestinationAlpha);
    GL.glBlendEquationSeparatei(0, EquationRGB, EquationAlpha);
  }
  else
  {
    GL.glBlendFuncSeparate(SourceRGB, DestinationRGB, SourceAlpha, DestinationAlpha);
    GL.glBlendEquationSeparate(EquationRGB, EquationAlpha);
  }

  if(modern && (HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend]))
  {
    GL.glColorMaski(0, ColorMask[0], ColorMask[1], ColorMask[2], ColorMask[3]);
  }
  else
  {
    GL.glColorMask(ColorMask[0], ColorMask[1], ColorMask[2], ColorMask[3]);
  }

  if(!IsGLES)
    GL.glPolygonMode(eGL_FRONT_AND_BACK, PolygonMode);

  if(modern && HasExt[ARB_viewport_array])
    GL.glViewportIndexedf(0, Viewportf[0], Viewportf[1], Viewportf[2], Viewportf[3]);
  else
    GL.glViewport(Viewport[0], Viewport[1], (GLsizei)Viewport[2], (GLsizei)Viewport[3]);

  GL.glActiveTexture(eGL_TEXTURE0);
  GL.glBindTexture(eGL_TEXTURE_2D, tex0);
  GL.glActiveTexture(ActiveTexture);

  GL.glBindBuffer(eGL_ARRAY_BUFFER, arraybuf);

  if(drawFBO != 0 && GL.glBindFramebuffer)
    GL.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, drawFBO);

  if(modern)
  {
    for(size_t i = 0; i < ARRAY_COUNT(idxubo); i++)
      GL.glBindBufferRange(eGL_UNIFORM_BUFFER, (GLuint)i, idxubo[i].buf, (GLintptr)idxubo[i].offs,
                           (GLsizeiptr)idxubo[i].size);

    GL.glBindBuffer(eGL_UNIFORM_BUFFER, ubo);

    GL.glUseProgram(prog);

    GL.glBindVertexArray(VAO);
  }
  else
  {
    // only restore these if there was a setting and the function pointer exists
    if(GL.glUseProgram && prog != 0)
      GL.glUseProgram(prog);
    if(GL.glBindProgramPipeline && pipe != 0)
      GL.glBindProgramPipeline(pipe);
  }
}

GLInitParams::GLInitParams()
{
  colorBits = 32;
  depthBits = 32;
  stencilBits = 8;
  isSRGB = 1;
  multiSamples = 1;
  width = 32;
  height = 32;
  isYFlipped = false;
}

bool GLInitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // 0x1A -> 0x1B - supported MSAA and Multiview framebuffer attachments, which added
  // number of samples, number of views, and base view index to the serialized data
  if(ver == 0x1A)
    return true;

  // 0x1B -> 0x1C - fixed incorrect float/double serialisation in serialisation of
  // ProgramUniformValue
  if(ver == 0x1B)
    return true;

  // 0x1C -> 0x1D - added isYFlipped init parameter for backbuffers on ANGLE.
  if(ver == 0x1C)
    return true;

  // 0x1D -> 0x1E - added new chunk for context parameters and per-context tracking of backbuffers
  if(ver == 0x1D)
    return true;

  // 0x1E -> 0x1F - added initial states for samplers that are modified a lot
  if(ver == 0x1E)
    return true;

  return false;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, GLInitParams &el)
{
  SERIALISE_MEMBER(colorBits);
  SERIALISE_MEMBER(depthBits);
  SERIALISE_MEMBER(stencilBits);
  SERIALISE_MEMBER(isSRGB);
  SERIALISE_MEMBER(multiSamples);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
  if(ser.VersionAtLeast(0x1D))
    SERIALISE_MEMBER(isYFlipped);
}

INSTANTIATE_SERIALISE_TYPE(GLInitParams);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DrawElementsIndirectCommand &el)
{
  SERIALISE_MEMBER(count);
  SERIALISE_MEMBER(instanceCount);
  SERIALISE_MEMBER(firstIndex);
  SERIALISE_MEMBER(baseVertex);
  SERIALISE_MEMBER(baseInstance);
}

INSTANTIATE_SERIALISE_TYPE(DrawElementsIndirectCommand);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, DrawArraysIndirectCommand &el)
{
  SERIALISE_MEMBER(count);
  SERIALISE_MEMBER(instanceCount);
  SERIALISE_MEMBER(first);
  SERIALISE_MEMBER(baseInstance);
}

INSTANTIATE_SERIALISE_TYPE(DrawArraysIndirectCommand);

size_t BufferIdx(GLenum buf)
{
  switch(buf)
  {
    case eGL_ARRAY_BUFFER: return 0;
    case eGL_ATOMIC_COUNTER_BUFFER: return 1;
    case eGL_COPY_READ_BUFFER: return 2;
    case eGL_COPY_WRITE_BUFFER: return 3;
    case eGL_DRAW_INDIRECT_BUFFER: return 4;
    case eGL_DISPATCH_INDIRECT_BUFFER: return 5;
    case eGL_ELEMENT_ARRAY_BUFFER: return 6;
    case eGL_PIXEL_PACK_BUFFER: return 7;
    case eGL_PIXEL_UNPACK_BUFFER: return 8;
    case eGL_QUERY_BUFFER: return 9;
    case eGL_SHADER_STORAGE_BUFFER: return 10;
    case eGL_TEXTURE_BUFFER: return 11;
    case eGL_TRANSFORM_FEEDBACK_BUFFER: return 12;
    case eGL_UNIFORM_BUFFER: return 13;
    case eGL_PARAMETER_BUFFER_ARB: return 14;
    default: RDCERR("Unexpected enum as buffer target: %s", ToStr(buf).c_str());
  }

  return 0;
}

GLenum BufferEnum(size_t idx)
{
  GLenum enums[] = {
      eGL_ARRAY_BUFFER,
      eGL_ATOMIC_COUNTER_BUFFER,
      eGL_COPY_READ_BUFFER,
      eGL_COPY_WRITE_BUFFER,
      eGL_DRAW_INDIRECT_BUFFER,
      eGL_DISPATCH_INDIRECT_BUFFER,
      eGL_ELEMENT_ARRAY_BUFFER,
      eGL_PIXEL_PACK_BUFFER,
      eGL_PIXEL_UNPACK_BUFFER,
      eGL_QUERY_BUFFER,
      eGL_SHADER_STORAGE_BUFFER,
      eGL_TEXTURE_BUFFER,
      eGL_TRANSFORM_FEEDBACK_BUFFER,
      eGL_UNIFORM_BUFFER,
      eGL_PARAMETER_BUFFER_ARB,
  };

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
}

size_t TextureIdx(GLenum buf)
{
  switch(buf)
  {
    case eGL_TEXTURE_1D: return 0;
    case eGL_TEXTURE_1D_ARRAY: return 1;
    case eGL_TEXTURE_2D: return 2;
    case eGL_TEXTURE_2D_ARRAY: return 3;
    case eGL_TEXTURE_2D_MULTISAMPLE: return 4;
    case eGL_TEXTURE_2D_MULTISAMPLE_ARRAY: return 5;
    case eGL_TEXTURE_RECTANGLE: return 6;
    case eGL_TEXTURE_3D: return 7;
    case eGL_TEXTURE_CUBE_MAP:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case eGL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case eGL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return 8;
    case eGL_TEXTURE_CUBE_MAP_ARRAY: return 9;
    case eGL_TEXTURE_BUFFER: return 10;
    default: RDCERR("Unexpected enum as texture target: %s", ToStr(buf).c_str());
  }

  return 0;
}

GLenum TextureEnum(size_t idx)
{
  GLenum enums[] = {
      eGL_TEXTURE_1D,
      eGL_TEXTURE_1D_ARRAY,
      eGL_TEXTURE_2D,
      eGL_TEXTURE_2D_ARRAY,
      eGL_TEXTURE_2D_MULTISAMPLE,
      eGL_TEXTURE_2D_MULTISAMPLE_ARRAY,
      eGL_TEXTURE_RECTANGLE,
      eGL_TEXTURE_3D,
      eGL_TEXTURE_CUBE_MAP,
      eGL_TEXTURE_CUBE_MAP_ARRAY,
      eGL_TEXTURE_BUFFER,
  };

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
}

size_t QueryIdx(GLenum query)
{
  size_t idx = 0;

  switch(query)
  {
    case eGL_SAMPLES_PASSED: idx = 0; break;
    case eGL_ANY_SAMPLES_PASSED: idx = 1; break;
    case eGL_ANY_SAMPLES_PASSED_CONSERVATIVE: idx = 2; break;
    case eGL_PRIMITIVES_GENERATED: idx = 3; break;
    case eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: idx = 4; break;
    case eGL_TIME_ELAPSED: idx = 5; break;
    case eGL_VERTICES_SUBMITTED_ARB: idx = 6; break;
    case eGL_PRIMITIVES_SUBMITTED_ARB: idx = 7; break;
    case eGL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB: idx = 8; break;
    case eGL_CLIPPING_INPUT_PRIMITIVES_ARB: idx = 9; break;
    case eGL_CLIPPING_OUTPUT_PRIMITIVES_ARB: idx = 10; break;
    case eGL_VERTEX_SHADER_INVOCATIONS_ARB: idx = 11; break;
    case eGL_TESS_CONTROL_SHADER_PATCHES_ARB: idx = 12; break;
    case eGL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB: idx = 13; break;
    case eGL_GEOMETRY_SHADER_INVOCATIONS: idx = 14; break;
    case eGL_FRAGMENT_SHADER_INVOCATIONS_ARB: idx = 15; break;
    case eGL_COMPUTE_SHADER_INVOCATIONS_ARB: idx = 16; break;

    default: RDCERR("Unexpected enum as query target: %s", ToStr(query).c_str());
  }

  if(idx >= WrappedOpenGL::MAX_QUERIES)
    RDCERR("Query index for enum %s out of range %d", ToStr(query).c_str(), idx);

  return idx;
}

GLenum QueryEnum(size_t idx)
{
  GLenum enums[] = {eGL_SAMPLES_PASSED,
                    eGL_ANY_SAMPLES_PASSED,
                    eGL_ANY_SAMPLES_PASSED_CONSERVATIVE,
                    eGL_PRIMITIVES_GENERATED,
                    eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
                    eGL_TIME_ELAPSED,
                    eGL_VERTICES_SUBMITTED_ARB,
                    eGL_PRIMITIVES_SUBMITTED_ARB,
                    eGL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB,
                    eGL_CLIPPING_INPUT_PRIMITIVES_ARB,
                    eGL_CLIPPING_OUTPUT_PRIMITIVES_ARB,
                    eGL_VERTEX_SHADER_INVOCATIONS_ARB,
                    eGL_TESS_CONTROL_SHADER_PATCHES_ARB,
                    eGL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB,
                    eGL_GEOMETRY_SHADER_INVOCATIONS,
                    eGL_FRAGMENT_SHADER_INVOCATIONS_ARB,
                    eGL_COMPUTE_SHADER_INVOCATIONS_ARB};

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
}

size_t GLTypeSize(GLenum type)
{
  switch(type)
  {
    case eGL_UNSIGNED_BYTE:
    case eGL_BYTE: return 1;
    case eGL_UNSIGNED_SHORT:
    case eGL_UNSIGNED_SHORT_5_6_5:
    case eGL_SHORT:
    case eGL_HALF_FLOAT_OES:
    case eGL_HALF_FLOAT: return 2;
    case eGL_UNSIGNED_INT:
    case eGL_INT:
    case eGL_FLOAT:
    case eGL_UNSIGNED_INT_8_8_8_8_REV: return 4;
    case eGL_DOUBLE: return 8;
    default: RDCWARN("Unhandled element type %s", ToStr(type).c_str());
  }
  return 0;
}

size_t ShaderIdx(GLenum buf)
{
  switch(buf)
  {
    case eGL_VERTEX_SHADER: return 0;
    case eGL_TESS_CONTROL_SHADER: return 1;
    case eGL_TESS_EVALUATION_SHADER: return 2;
    case eGL_GEOMETRY_SHADER: return 3;
    case eGL_FRAGMENT_SHADER: return 4;
    case eGL_COMPUTE_SHADER: return 5;
    default: RDCERR("Unexpected enum as shader enum: %s", ToStr(buf).c_str());
  }

  return 0;
}

GLenum ShaderBit(size_t idx)
{
  GLenum enums[] = {
      eGL_VERTEX_SHADER_BIT,   eGL_TESS_CONTROL_SHADER_BIT, eGL_TESS_EVALUATION_SHADER_BIT,
      eGL_GEOMETRY_SHADER_BIT, eGL_FRAGMENT_SHADER_BIT,     eGL_COMPUTE_SHADER_BIT,
  };

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
}

GLenum ShaderEnum(size_t idx)
{
  GLenum enums[] = {
      eGL_VERTEX_SHADER,   eGL_TESS_CONTROL_SHADER, eGL_TESS_EVALUATION_SHADER,
      eGL_GEOMETRY_SHADER, eGL_FRAGMENT_SHADER,     eGL_COMPUTE_SHADER,
  };

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
}

void ClearGLErrors()
{
  int i = 0;
  GLenum err = GL.glGetError();
  while(err)
  {
    err = GL.glGetError();
    i++;
    if(i > 100)
    {
      RDCERR("Couldn't clear GL errors - something very wrong!");
      return;
    }
  }
}

GLint GetNumVertexBuffers()
{
  GLint numBindings = 16;

  // when the extension isn't present we pretend attribs == vertex buffers
  if(HasExt[ARB_vertex_attrib_binding])
    GL.glGetIntegerv(eGL_MAX_VERTEX_ATTRIB_BINDINGS, &numBindings);
  else
    GL.glGetIntegerv(eGL_MAX_VERTEX_ATTRIBS, &numBindings);

  return numBindings;
}

GLuint GetBoundVertexBuffer(GLuint i)
{
  GLuint buffer = 0;

  if(VendorCheck[VendorCheck_AMD_vertex_buffer_query] || !HasExt[ARB_vertex_attrib_binding])
    GL.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint *)&buffer);
  else
    GL.glGetIntegeri_v(eGL_VERTEX_BINDING_BUFFER, i, (GLint *)&buffer);

  return buffer;
}

void SafeBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                         GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
  bool scissorEnabled = false;
  GLboolean ColorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  GLboolean DepthMask = GL_TRUE;
  GLint StencilMask = 0xff, StencilBackMask = 0xff;

  // fetch current state
  {
    if(HasExt[ARB_viewport_array])
      scissorEnabled = GL.glIsEnabledi(eGL_SCISSOR_TEST, 0) != 0;
    else
      scissorEnabled = GL.glIsEnabled(eGL_SCISSOR_TEST) != 0;

    if(HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend])
      GL.glGetBooleani_v(eGL_COLOR_WRITEMASK, 0, ColorMask);
    else
      GL.glGetBooleanv(eGL_COLOR_WRITEMASK, ColorMask);

    GL.glGetBooleanv(eGL_DEPTH_WRITEMASK, &DepthMask);

    GL.glGetIntegerv(eGL_STENCIL_WRITEMASK, &StencilMask);
    GL.glGetIntegerv(eGL_STENCIL_BACK_WRITEMASK, &StencilBackMask);
  }

  // apply safe state
  {
    if(HasExt[ARB_viewport_array])
      GL.glDisablei(eGL_SCISSOR_TEST, 0);
    else
      GL.glDisable(eGL_SCISSOR_TEST);

    if(HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend])
      GL.glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    else
      GL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    GL.glDepthMask(GL_TRUE);

    GL.glStencilMaskSeparate(eGL_FRONT, 0xff);
    GL.glStencilMaskSeparate(eGL_BACK, 0xff);
  }

  GL.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

  // restore original state
  {
    if(HasExt[ARB_viewport_array])
    {
      if(scissorEnabled)
        GL.glEnablei(eGL_SCISSOR_TEST, 0);
      else
        GL.glDisablei(eGL_SCISSOR_TEST, 0);
    }
    else
    {
      if(scissorEnabled)
        GL.glEnable(eGL_SCISSOR_TEST);
      else
        GL.glDisable(eGL_SCISSOR_TEST);
    }

    if(HasExt[EXT_draw_buffers2] || HasExt[ARB_draw_buffers_blend])
      GL.glColorMaski(0, ColorMask[0], ColorMask[1], ColorMask[2], ColorMask[3]);
    else
      GL.glColorMask(ColorMask[0], ColorMask[1], ColorMask[2], ColorMask[3]);

    GL.glDepthMask(DepthMask);

    GL.glStencilMaskSeparate(eGL_FRONT, StencilMask);
    GL.glStencilMaskSeparate(eGL_BACK, StencilBackMask);
  }
}

BufferCategory MakeBufferCategory(GLenum bufferTarget)
{
  switch(bufferTarget)
  {
    case eGL_ARRAY_BUFFER: return BufferCategory::Vertex; break;
    case eGL_ELEMENT_ARRAY_BUFFER: return BufferCategory::Index; break;
    case eGL_UNIFORM_BUFFER: return BufferCategory::Constants; break;
    case eGL_SHADER_STORAGE_BUFFER: return BufferCategory::ReadWrite; break;
    case eGL_DRAW_INDIRECT_BUFFER:
    case eGL_DISPATCH_INDIRECT_BUFFER:
    case eGL_PARAMETER_BUFFER_ARB: return BufferCategory::Indirect; break;
    default: break;
  }
  return BufferCategory::NoFlags;
}

AddressMode MakeAddressMode(GLenum addr)
{
  switch(addr)
  {
    case eGL_REPEAT: return AddressMode::Wrap;
    case eGL_MIRRORED_REPEAT: return AddressMode::Mirror;
    case eGL_CLAMP_TO_EDGE: return AddressMode::ClampEdge;
    case eGL_CLAMP_TO_BORDER: return AddressMode::ClampBorder;
    case eGL_MIRROR_CLAMP_TO_EDGE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

TextureFilter MakeFilter(GLenum minf, GLenum magf, bool shadowSampler, float maxAniso)
{
  TextureFilter ret;

  if(maxAniso > 1.0f)
  {
    ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
  }
  else
  {
    if(minf == eGL_NEAREST || minf == eGL_LINEAR)
    {
      ret.minify = (minf == eGL_LINEAR) ? FilterMode::Linear : FilterMode::Point;
      ret.mip = FilterMode::NoFilter;
    }
    else if(minf == eGL_NEAREST_MIPMAP_LINEAR || minf == eGL_LINEAR_MIPMAP_LINEAR)
    {
      ret.minify = (minf == eGL_LINEAR_MIPMAP_LINEAR) ? FilterMode::Linear : FilterMode::Point;
      ret.mip = FilterMode::Linear;
    }
    else if(minf == eGL_NEAREST_MIPMAP_NEAREST || minf == eGL_LINEAR_MIPMAP_NEAREST)
    {
      ret.minify = (minf == eGL_LINEAR_MIPMAP_NEAREST) ? FilterMode::Linear : FilterMode::Point;
      ret.mip = FilterMode::Point;
    }

    ret.magnify = (magf == eGL_LINEAR) ? FilterMode::Linear : FilterMode::Point;
  }
  ret.filter = shadowSampler ? FilterFunction::Comparison : FilterFunction::Normal;

  return ret;
}

ShaderStage MakeShaderStage(GLenum type)
{
  switch(type)
  {
    case eGL_VERTEX_SHADER: return ShaderStage::Vertex;
    case eGL_TESS_CONTROL_SHADER: return ShaderStage::Tess_Control;
    case eGL_TESS_EVALUATION_SHADER: return ShaderStage::Tess_Eval;
    case eGL_GEOMETRY_SHADER: return ShaderStage::Geometry;
    case eGL_FRAGMENT_SHADER: return ShaderStage::Fragment;
    case eGL_COMPUTE_SHADER: return ShaderStage::Compute;
    default: RDCERR("Unexpected shader stage %s", ToStr(type).c_str());
  }

  return ShaderStage::Count;
}

CompareFunction MakeCompareFunc(GLenum func)
{
  switch(func)
  {
    case eGL_NEVER: return CompareFunction::Never;
    case eGL_LESS: return CompareFunction::Less;
    case eGL_EQUAL: return CompareFunction::Equal;
    case eGL_LEQUAL: return CompareFunction::LessEqual;
    case eGL_GREATER: return CompareFunction::Greater;
    case eGL_NOTEQUAL: return CompareFunction::NotEqual;
    case eGL_GEQUAL: return CompareFunction::GreaterEqual;
    case eGL_ALWAYS: return CompareFunction::AlwaysTrue;
    default: break;
  }

  return CompareFunction::AlwaysTrue;
}

StencilOperation MakeStencilOp(GLenum op)
{
  switch(op)
  {
    case eGL_KEEP: return StencilOperation::Keep;
    case eGL_ZERO: return StencilOperation::Zero;
    case eGL_REPLACE: return StencilOperation::Replace;
    case eGL_INCR: return StencilOperation::IncSat;
    case eGL_DECR: return StencilOperation::DecSat;
    case eGL_INVERT: return StencilOperation::Invert;
    case eGL_INCR_WRAP: return StencilOperation::IncWrap;
    case eGL_DECR_WRAP: return StencilOperation::DecWrap;
    default: break;
  }

  return StencilOperation::Keep;
}

LogicOperation MakeLogicOp(GLenum op)
{
  switch(op)
  {
    case eGL_CLEAR: return LogicOperation::Clear;
    case eGL_AND: return LogicOperation::And;
    case eGL_AND_REVERSE: return LogicOperation::AndReverse;
    case eGL_COPY: return LogicOperation::Copy;
    case eGL_AND_INVERTED: return LogicOperation::AndInverted;
    case eGL_NOOP: return LogicOperation::NoOp;
    case eGL_XOR: return LogicOperation::Xor;
    case eGL_OR: return LogicOperation::Or;
    case eGL_NOR: return LogicOperation::Nor;
    case eGL_EQUIV: return LogicOperation::Equivalent;
    case eGL_INVERT: return LogicOperation::Invert;
    case eGL_OR_REVERSE: return LogicOperation::OrReverse;
    case eGL_COPY_INVERTED: return LogicOperation::CopyInverted;
    case eGL_OR_INVERTED: return LogicOperation::OrInverted;
    case eGL_NAND: return LogicOperation::Nand;
    case eGL_SET: return LogicOperation::Set;
    default: break;
  }

  return LogicOperation::NoOp;
}

BlendMultiplier MakeBlendMultiplier(GLenum blend)
{
  switch(blend)
  {
    case eGL_ZERO: return BlendMultiplier::Zero;
    case eGL_ONE: return BlendMultiplier::One;
    case eGL_SRC_COLOR: return BlendMultiplier::SrcCol;
    case eGL_ONE_MINUS_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case eGL_DST_COLOR: return BlendMultiplier::DstCol;
    case eGL_ONE_MINUS_DST_COLOR: return BlendMultiplier::InvDstCol;
    case eGL_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case eGL_ONE_MINUS_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case eGL_DST_ALPHA: return BlendMultiplier::DstAlpha;
    case eGL_ONE_MINUS_DST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case eGL_CONSTANT_COLOR: return BlendMultiplier::FactorRGB;
    case eGL_ONE_MINUS_CONSTANT_COLOR: return BlendMultiplier::InvFactorRGB;
    case eGL_CONSTANT_ALPHA: return BlendMultiplier::FactorAlpha;
    case eGL_ONE_MINUS_CONSTANT_ALPHA: return BlendMultiplier::InvFactorAlpha;
    case eGL_SRC_ALPHA_SATURATE: return BlendMultiplier::SrcAlphaSat;
    case eGL_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case eGL_ONE_MINUS_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case eGL_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case eGL_ONE_MINUS_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(GLenum op)
{
  switch(op)
  {
    case eGL_FUNC_ADD: return BlendOperation::Add;
    case eGL_FUNC_SUBTRACT: return BlendOperation::Subtract;
    case eGL_FUNC_REVERSE_SUBTRACT: return BlendOperation::ReversedSubtract;
    case eGL_MIN: return BlendOperation::Minimum;
    case eGL_MAX: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

ResourceFormat MakeResourceFormat(GLenum target, GLenum fmt)
{
  ResourceFormat ret;

  ret.type = ResourceFormatType::Regular;

  if(fmt == eGL_NONE)
  {
    ret.type = ResourceFormatType::Undefined;
    return ret;
  }

  // special handling for formats that don't query neatly
  if(fmt == eGL_LUMINANCE8_EXT || fmt == eGL_INTENSITY8_EXT || fmt == eGL_ALPHA8_EXT ||
     fmt == eGL_LUMINANCE || fmt == eGL_ALPHA)
  {
    ret.compByteWidth = 1;
    ret.compCount = 1;
    ret.compType = CompType::UNorm;
    return ret;
  }
  else if(fmt == eGL_LUMINANCE8_ALPHA8_EXT || fmt == eGL_LUMINANCE_ALPHA)
  {
    ret.compByteWidth = 1;
    ret.compCount = 2;
    ret.compType = CompType::UNorm;
    return ret;
  }

  if(IsCompressedFormat(fmt))
  {
    switch(fmt)
    {
      case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT: ret.compCount = 3; break;
      case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: ret.compCount = 4; break;

      case eGL_COMPRESSED_RGBA8_ETC2_EAC:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: ret.compCount = 4; break;
      case eGL_COMPRESSED_R11_EAC:
      case eGL_COMPRESSED_SIGNED_R11_EAC: ret.compCount = 1; break;
      case eGL_COMPRESSED_RG11_EAC:
      case eGL_COMPRESSED_SIGNED_RG11_EAC: ret.compCount = 2; break;

      case eGL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
      case eGL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT: ret.compCount = 3; break;
      case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT: ret.compCount = 4; break;

      case eGL_ETC1_RGB8_OES:
      case eGL_COMPRESSED_RGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_ETC2: ret.compCount = 3; break;
      case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: ret.compCount = 4; break;

      default: break;
    }

    ret.compType = CompType::UNorm;

    switch(fmt)
    {
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
      case eGL_COMPRESSED_SRGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: ret.compType = CompType::UNormSRGB; break;
      default: break;
    }

    switch(fmt)
    {
      case eGL_COMPRESSED_SIGNED_RED_RGTC1:
      case eGL_COMPRESSED_SIGNED_RG_RGTC2:
      case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
      case eGL_COMPRESSED_SIGNED_R11_EAC:
      case eGL_COMPRESSED_SIGNED_RG11_EAC: ret.compType = CompType::SNorm; break;
      default: break;
    }

    ret.type = ResourceFormatType::Undefined;

    switch(fmt)
    {
      // BC1
      case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        ret.type = ResourceFormatType::BC1;
        break;
      // BC2
      case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        ret.type = ResourceFormatType::BC2;
        break;
      // BC3
      case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        ret.type = ResourceFormatType::BC3;
        break;
      // BC4
      case eGL_COMPRESSED_RED_RGTC1:
      case eGL_COMPRESSED_SIGNED_RED_RGTC1:
        ret.type = ResourceFormatType::BC4;
        break;
      // BC5
      case eGL_COMPRESSED_RG_RGTC2:
      case eGL_COMPRESSED_SIGNED_RG_RGTC2:
        ret.type = ResourceFormatType::BC5;
        break;
      // BC6
      case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
      case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
        ret.type = ResourceFormatType::BC6;
        break;
      // BC7
      case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
      case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
        ret.type = ResourceFormatType::BC7;
        break;
      // ETC1
      case eGL_ETC1_RGB8_OES:    // handle it as ETC2
      // ETC2
      case eGL_COMPRESSED_RGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_ETC2:
      case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        ret.type = ResourceFormatType::ETC2;
        break;
      // EAC
      case eGL_COMPRESSED_RGBA8_ETC2_EAC:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      case eGL_COMPRESSED_R11_EAC:
      case eGL_COMPRESSED_SIGNED_R11_EAC:
      case eGL_COMPRESSED_RG11_EAC:
      case eGL_COMPRESSED_SIGNED_RG11_EAC:
        ret.type = ResourceFormatType::EAC;
        break;
      // ASTC
      case eGL_COMPRESSED_RGBA_ASTC_4x4_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_5x4_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_5x5_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_6x5_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_6x6_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_8x5_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_8x6_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_8x8_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_10x5_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_10x6_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_10x8_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_10x10_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_12x10_KHR:
      case eGL_COMPRESSED_RGBA_ASTC_12x12_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        ret.type = ResourceFormatType::ASTC;
        break;
      // PVRTC
      case eGL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT:
      case eGL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT: ret.type = ResourceFormatType::PVRTC; break;
      default: RDCERR("Unexpected compressed format %#x", fmt); break;
    }

    return ret;
  }

  // handle certain non compressed but special formats
  switch(fmt)
  {
    case eGL_R11F_G11F_B10F: ret.type = ResourceFormatType::R11G11B10; break;
    case eGL_RGB565: ret.type = ResourceFormatType::R5G6B5; break;
    case eGL_RGB5_A1: ret.type = ResourceFormatType::R5G5B5A1; break;
    case eGL_RGB9_E5: ret.type = ResourceFormatType::R9G9B9E5; break;
    case eGL_RGBA4: ret.type = ResourceFormatType::R4G4B4A4; break;
    case eGL_RGB10_A2:
    case eGL_RGB10_A2UI:
      ret.type = ResourceFormatType::R10G10B10A2;
      ret.compType = fmt == eGL_RGB10_A2 ? CompType::UNorm : CompType::UInt;
      break;
    default: break;
  }

  if(ret.Special())
    return ret;

  ret.compByteWidth = 1;
  ret.compCount = 4;
  ret.compType = CompType::Float;

  GLint data[8];
  GLenum *edata = (GLenum *)data;

  GLint iscol = 0, isdepth = 0, isstencil = 0;
  GL.glGetInternalformativ(target, fmt, eGL_COLOR_COMPONENTS, sizeof(GLint), &iscol);
  GL.glGetInternalformativ(target, fmt, eGL_DEPTH_COMPONENTS, sizeof(GLint), &isdepth);
  GL.glGetInternalformativ(target, fmt, eGL_STENCIL_COMPONENTS, sizeof(GLint), &isstencil);

  if(iscol == GL_TRUE)
  {
    if(fmt == eGL_BGRA8_EXT || fmt == eGL_BGRA)
      ret.SetBGRAOrder(true);

    // colour format

    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_SIZE, sizeof(GLint), &data[0]);
    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_SIZE, sizeof(GLint), &data[1]);
    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_SIZE, sizeof(GLint), &data[2]);
    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_SIZE, sizeof(GLint), &data[3]);

    ret.compCount = 0;
    for(int i = 0; i < 4; i++)
      if(data[i] > 0)
        ret.compCount++;

    for(int i = ret.compCount; i < 4; i++)
      data[i] = data[0];

    if(data[0] == data[1] && data[1] == data[2] && data[2] == data[3])
    {
      ret.compByteWidth = (uint8_t)(data[0] / 8);

      // wasn't a byte format (8, 16, 32)
      if(int32_t(ret.compByteWidth) * 8 != data[0])
      {
        ret.type = ResourceFormatType::Undefined;
        RDCERR("Unexpected/unhandled non-uniform format: '%s'", ToStr(fmt).c_str());
      }
    }
    else
    {
      ret.type = ResourceFormatType::Undefined;
      RDCERR("Unexpected/unhandled non-uniform format: '%s'", ToStr(fmt).c_str());
    }

    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_TYPE, sizeof(GLint), &data[0]);
    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_TYPE, sizeof(GLint), &data[1]);
    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_TYPE, sizeof(GLint), &data[2]);
    GL.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_TYPE, sizeof(GLint), &data[3]);

    for(int i = ret.compCount; i < 4; i++)
      data[i] = data[0];

    if(data[0] == data[1] && data[1] == data[2] && data[2] == data[3])
    {
      switch(edata[0])
      {
        case eGL_UNSIGNED_INT: ret.compType = CompType::UInt; break;
        case eGL_UNSIGNED_BYTE:
        case eGL_UNSIGNED_NORMALIZED: ret.compType = CompType::UNorm; break;
        case eGL_SIGNED_NORMALIZED: ret.compType = CompType::SNorm; break;
        case eGL_FLOAT: ret.compType = CompType::Float; break;
        case eGL_INT: ret.compType = CompType::SInt; break;
        default: RDCERR("Unexpected texture type");
      }
    }
    else
    {
      ret.type = ResourceFormatType::Undefined;
      RDCERR("Unexpected/unhandled non-uniform format: '%s'", ToStr(fmt).c_str());
    }

    GL.glGetInternalformativ(target, fmt, eGL_COLOR_ENCODING, sizeof(GLint), &data[0]);
    if(edata[0] == eGL_SRGB)
      ret.compType = CompType::UNormSRGB;
  }
  else if(isdepth == GL_TRUE || isstencil == GL_TRUE)
  {
    // depth format
    ret.compType = CompType::Depth;

    switch(fmt)
    {
      case eGL_DEPTH_COMPONENT16:
        ret.compByteWidth = 2;
        ret.compCount = 1;
        break;
      case eGL_DEPTH_COMPONENT24:
        ret.compByteWidth = 3;
        ret.compCount = 1;
        break;
      case eGL_DEPTH_COMPONENT:
      case eGL_DEPTH_COMPONENT32:
      case eGL_DEPTH_COMPONENT32F:
        ret.compByteWidth = 4;
        ret.compCount = 1;
        break;
      case eGL_DEPTH24_STENCIL8:
        ret.compByteWidth = 0;
        ret.compCount = 2;
        ret.type = ResourceFormatType::D24S8;
        break;
      case eGL_DEPTH_STENCIL:
      case eGL_DEPTH32F_STENCIL8:
        ret.compByteWidth = 0;
        ret.compCount = 2;
        ret.type = ResourceFormatType::D32S8;
        break;
      case eGL_STENCIL_INDEX:
      case eGL_STENCIL_INDEX8:
        ret.compByteWidth = 1;
        ret.compCount = 1;
        ret.type = ResourceFormatType::S8;
        break;
      default: RDCERR("Unexpected depth or stencil format '%s'", ToStr(fmt).c_str());
    }
  }
  else
  {
    // not colour or depth!
    RDCERR("Unexpected texture type, not colour or depth: '%s'", ToStr(fmt).c_str());
  }

  return ret;
}

GLenum MakeGLFormat(ResourceFormat fmt)
{
  GLenum ret = eGL_NONE;

  if(fmt.Special())
  {
    switch(fmt.type)
    {
      case ResourceFormatType::BC1:
      {
        if(fmt.compCount == 3)
          ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT
                                    : eGL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        else
          ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                                    : eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
      }
      case ResourceFormatType::BC2:
        ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                                  : eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
      case ResourceFormatType::BC3:
        ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                                  : eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;
      case ResourceFormatType::BC4:
        ret = fmt.compType == CompType::SNorm ? eGL_COMPRESSED_SIGNED_RED_RGTC1
                                              : eGL_COMPRESSED_RED_RGTC1;
        break;
      case ResourceFormatType::BC5:
        ret = fmt.compType == CompType::SNorm ? eGL_COMPRESSED_SIGNED_RG_RGTC2
                                              : eGL_COMPRESSED_RG_RGTC2;
        break;
      case ResourceFormatType::BC6:
        ret = fmt.compType == CompType::SNorm ? eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB
                                              : eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
        break;
      case ResourceFormatType::BC7:
        ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB
                                  : eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
        break;
      case ResourceFormatType::ETC2:
      {
        if(fmt.compCount == 3)
          ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB8_ETC2 : eGL_COMPRESSED_RGB8_ETC2;
        else
          ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2
                                    : eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
        break;
      }
      case ResourceFormatType::EAC:
      {
        if(fmt.compCount == 1)
          ret = fmt.compType == CompType::SNorm ? eGL_COMPRESSED_SIGNED_R11_EAC
                                                : eGL_COMPRESSED_R11_EAC;
        else if(fmt.compCount == 2)
          ret = fmt.compType == CompType::SNorm ? eGL_COMPRESSED_SIGNED_RG11_EAC
                                                : eGL_COMPRESSED_RG11_EAC;
        else
          ret = fmt.SRGBCorrected() ? eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
                                    : eGL_COMPRESSED_RGBA8_ETC2_EAC;
        break;
      }
      case ResourceFormatType::R10G10B10A2:
        if(fmt.compType == CompType::UNorm)
          ret = eGL_RGB10_A2;
        else
          ret = eGL_RGB10_A2UI;
        break;
      case ResourceFormatType::R11G11B10: ret = eGL_R11F_G11F_B10F; break;
      case ResourceFormatType::R5G6B5: ret = eGL_RGB565; break;
      case ResourceFormatType::R5G5B5A1: ret = eGL_RGB5_A1; break;
      case ResourceFormatType::R9G9B9E5: ret = eGL_RGB9_E5; break;
      case ResourceFormatType::R4G4B4A4: ret = eGL_RGBA4; break;
      case ResourceFormatType::D24S8: ret = eGL_DEPTH24_STENCIL8; break;
      case ResourceFormatType::D32S8: ret = eGL_DEPTH32F_STENCIL8; break;
      case ResourceFormatType::ASTC: RDCERR("ASTC can't be decoded unambiguously"); break;
      case ResourceFormatType::PVRTC: RDCERR("PVRTC can't be decoded unambiguously"); break;
      case ResourceFormatType::S8: ret = eGL_STENCIL_INDEX8; break;
      case ResourceFormatType::Undefined: return eGL_NONE;
      default: RDCERR("Unsupported resource format type %u", fmt.type); break;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.SRGBCorrected())
    {
      ret = eGL_SRGB8_ALPHA8;
    }
    else if(fmt.BGRAOrder())
    {
      ret = eGL_BGRA8_EXT;
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_RGBA32F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_RGBA32I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RGBA32UI;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_RGBA16F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_RGBA16I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RGBA16UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_RGBA16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_RGBA16;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = eGL_RGBA8I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RGBA8UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_RGBA8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_RGBA8;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 4-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 3)
  {
    if(fmt.SRGBCorrected())
    {
      ret = eGL_SRGB8;
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_RGB32F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_RGB32I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RGB32UI;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_RGB16F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_RGB16I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RGB16UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_RGB16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_RGB16;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = eGL_RGB8I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RGB8UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_RGB8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_RGB8;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 2)
  {
    if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_RG32F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_RG32I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RG32UI;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_RG16F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_RG16I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RG16UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_RG16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_RG16;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = eGL_RG8I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_RG8UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_RG8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_RG8;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 1)
  {
    if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_R32F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_R32I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_R32UI;
      else if(fmt.compType == CompType::Depth)
        ret = eGL_DEPTH_COMPONENT32F;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 3)
    {
      ret = eGL_DEPTH_COMPONENT24;
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = eGL_R16F;
      else if(fmt.compType == CompType::SInt)
        ret = eGL_R16I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_R16UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_R16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_R16;
      else if(fmt.compType == CompType::Depth)
        ret = eGL_DEPTH_COMPONENT16;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = eGL_R8I;
      else if(fmt.compType == CompType::UInt)
        ret = eGL_R8UI;
      else if(fmt.compType == CompType::SNorm)
        ret = eGL_R8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = eGL_R8;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else
  {
    RDCERR("Unrecognised component count: %d", fmt.compCount);
  }

  if(ret == eGL_NONE)
    RDCERR("No known GL format corresponding to resource format!");

  return ret;
}

GLenum MakeGLPrimitiveTopology(Topology Topo)
{
  switch(Topo)
  {
    default: return eGL_NONE;
    case Topology::PointList: return eGL_POINTS;
    case Topology::LineStrip: return eGL_LINE_STRIP;
    case Topology::LineLoop: return eGL_LINE_LOOP;
    case Topology::LineList: return eGL_LINES;
    case Topology::LineStrip_Adj: return eGL_LINE_STRIP_ADJACENCY;
    case Topology::LineList_Adj: return eGL_LINES_ADJACENCY;
    case Topology::TriangleStrip: return eGL_TRIANGLE_STRIP;
    case Topology::TriangleFan: return eGL_TRIANGLE_FAN;
    case Topology::TriangleList: return eGL_TRIANGLES;
    case Topology::TriangleStrip_Adj: return eGL_TRIANGLE_STRIP_ADJACENCY;
    case Topology::TriangleList_Adj: return eGL_TRIANGLES_ADJACENCY;
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
    case Topology::PatchList_32CPs: return eGL_PATCHES;
  }
}

Topology MakePrimitiveTopology(GLenum Topo)
{
  switch(Topo)
  {
    default: return Topology::Unknown;
    case eGL_POINTS: return Topology::PointList;
    case eGL_LINE_STRIP: return Topology::LineStrip;
    case eGL_LINE_LOOP: return Topology::LineLoop;
    case eGL_LINES: return Topology::LineList;
    case eGL_LINE_STRIP_ADJACENCY: return Topology::LineStrip_Adj;
    case eGL_LINES_ADJACENCY: return Topology::LineList_Adj;
    case eGL_TRIANGLE_STRIP: return Topology::TriangleStrip;
    case eGL_TRIANGLE_FAN: return Topology::TriangleFan;
    case eGL_TRIANGLES: return Topology::TriangleList;
    case eGL_TRIANGLE_STRIP_ADJACENCY: return Topology::TriangleStrip_Adj;
    case eGL_TRIANGLES_ADJACENCY: return Topology::TriangleList_Adj;
    case eGL_PATCHES:
    {
      GLint patchCount = 3;
      GL.glGetIntegerv(eGL_PATCH_VERTICES, &patchCount);
      return PatchList_Topology(patchCount);
    }
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

TEST_CASE("GL formats", "[format][gl]")
{
  // must be updated by hand
  GLenum supportedFormats[] = {
      eGL_NONE,
      eGL_R8,
      eGL_R8_SNORM,
      eGL_R8UI,
      eGL_R8I,
      eGL_RG8,
      eGL_RG8_SNORM,
      eGL_RG8UI,
      eGL_RG8I,
      eGL_RGB8,
      eGL_RGB8_SNORM,
      eGL_RGB8UI,
      eGL_RGB8I,
      eGL_SRGB8,
      eGL_RGBA8,
      eGL_RGBA8_SNORM,
      eGL_RGBA8UI,
      eGL_RGBA8I,
      eGL_SRGB8_ALPHA8,
      eGL_BGRA8_EXT,
      eGL_R16,
      eGL_R16_SNORM,
      eGL_R16UI,
      eGL_R16I,
      eGL_R16F,
      eGL_RG16,
      eGL_RG16_SNORM,
      eGL_RG16UI,
      eGL_RG16I,
      eGL_RG16F,
      eGL_RGB16,
      eGL_RGB16_SNORM,
      eGL_RGB16UI,
      eGL_RGB16I,
      eGL_RGB16F,
      eGL_RGBA16,
      eGL_RGBA16_SNORM,
      eGL_RGBA16UI,
      eGL_RGBA16I,
      eGL_RGBA16F,
      eGL_R32UI,
      eGL_R32I,
      eGL_R32F,
      eGL_RG32UI,
      eGL_RG32I,
      eGL_RG32F,
      eGL_RGB32UI,
      eGL_RGB32I,
      eGL_RGB32F,
      eGL_RGBA32UI,
      eGL_RGBA32I,
      eGL_RGBA32F,
      eGL_RGBA4,
      eGL_RGB565,
      eGL_RGB5_A1,
      eGL_R11F_G11F_B10F,
      eGL_RGB9_E5,
      eGL_RGB10_A2,
      eGL_RGB10_A2UI,
      eGL_DEPTH_COMPONENT16,
      eGL_DEPTH_COMPONENT24,
      eGL_DEPTH_COMPONENT32,
      eGL_DEPTH_COMPONENT32F,
      eGL_DEPTH24_STENCIL8,
      eGL_DEPTH32F_STENCIL8,
      eGL_COMPRESSED_RGB_S3TC_DXT1_EXT,
      eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT,
      eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
      eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
      eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
      eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
      eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
      eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
      eGL_COMPRESSED_RED_RGTC1,
      eGL_COMPRESSED_SIGNED_RED_RGTC1,
      eGL_COMPRESSED_RG_RGTC2,
      eGL_COMPRESSED_SIGNED_RG_RGTC2,
      eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB,
      eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB,
      eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB,
      eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB,
      eGL_ETC1_RGB8_OES,
      eGL_COMPRESSED_RGB8_ETC2,
      eGL_COMPRESSED_SRGB8_ETC2,
      eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
      eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
      eGL_COMPRESSED_RGBA8_ETC2_EAC,
      eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
      eGL_COMPRESSED_R11_EAC,
      eGL_COMPRESSED_SIGNED_R11_EAC,
      eGL_COMPRESSED_RG11_EAC,
      eGL_COMPRESSED_SIGNED_RG11_EAC,
      eGL_COMPRESSED_RGBA_ASTC_4x4_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
      eGL_COMPRESSED_RGBA_ASTC_5x4_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
      eGL_COMPRESSED_RGBA_ASTC_5x5_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
      eGL_COMPRESSED_RGBA_ASTC_6x5_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
      eGL_COMPRESSED_RGBA_ASTC_6x6_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
      eGL_COMPRESSED_RGBA_ASTC_8x5_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
      eGL_COMPRESSED_RGBA_ASTC_8x6_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
      eGL_COMPRESSED_RGBA_ASTC_8x8_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
      eGL_COMPRESSED_RGBA_ASTC_10x5_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
      eGL_COMPRESSED_RGBA_ASTC_10x6_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
      eGL_COMPRESSED_RGBA_ASTC_10x8_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
      eGL_COMPRESSED_RGBA_ASTC_10x10_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
      eGL_COMPRESSED_RGBA_ASTC_12x10_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
      eGL_COMPRESSED_RGBA_ASTC_12x12_KHR,
      eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR,
      eGL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT,
      eGL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT,
      eGL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT,
      eGL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT,
  };

  // we use our emulated queries for the format, as we don't want to init a context here, and anyway
  // we'd rather have an isolated test-case of only our code, not be testing a GL driver
  // implementation
  GL.EmulateRequiredExtensions();

  SECTION("Only GL_NONE returns unknown")
  {
    for(GLenum f : supportedFormats)
    {
      ResourceFormat fmt = MakeResourceFormat(eGL_TEXTURE_2D, f);

      if(f == eGL_NONE)
        CHECK(fmt.type == ResourceFormatType::Undefined);
      else
        CHECK(fmt.type != ResourceFormatType::Undefined);
    }
  };

  SECTION("MakeGLFormat is reflexive with MakeResourceFormat")
  {
    for(GLenum f : supportedFormats)
    {
      // we don't support ETC1
      if(f == GL_ETC1_RGB8_OES)
        continue;

      ResourceFormat fmt = MakeResourceFormat(eGL_TEXTURE_2D, f);

      // we don't support ASTC/PVRTC formats currently
      if(fmt.type == ResourceFormatType::ASTC || fmt.type == ResourceFormatType::PVRTC)
        continue;

      GLenum glf = MakeGLFormat(fmt);

      // it's OK to 'lose' the non-float flag on this format
      if(f == eGL_DEPTH_COMPONENT32)
      {
        CHECK(glf == eGL_DEPTH_COMPONENT32F);
      }
      else
      {
        CHECK(glf == f);
      }
    }
  };

  SECTION("GetByteSize and GetFormatBPP return expected values for regular formats")
  {
    for(GLenum f : supportedFormats)
    {
      ResourceFormat fmt = MakeResourceFormat(eGL_TEXTURE_2D, f);

      if(fmt.type != ResourceFormatType::Regular)
        continue;

      INFO("Format is " << f);

      uint32_t size = fmt.compCount * fmt.compByteWidth * 123 * 456;

      // this takes up a full int, even if the byte width is listed as 3.
      if(f == eGL_DEPTH_COMPONENT24)
        size = fmt.compCount * 4 * 123 * 456;

      CHECK(size == GetByteSize(123, 456, 1, GetBaseFormat(f), GetDataType(f)));
    }
  };

  GL = GLDispatchTable();
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
