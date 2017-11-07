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

#include "gl_common.h"
#include "core/core.h"
#include "strings/string_utils.h"
#include "gl_driver.h"

GLChunk gl_CurChunk = GLChunk::Max;

bool HasExt[GLExtension_Count] = {};
bool VendorCheck[VendorCheck_Count] = {};

int GLCoreVersion = 0;
bool GLIsCore = false;
bool IsGLES = false;

bool CheckReplayContext(PFNGLGETSTRINGPROC getStr, PFNGLGETINTEGERVPROC getInt,
                        PFNGLGETSTRINGIPROC getStri)
{
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

  if(getStr)
    RDCLOG("Running GL replay on: %s / %s / %s", getStr(eGL_VENDOR), getStr(eGL_RENDERER),
           getStr(eGL_VERSION));

  string extensionString = "";

  GLint numExts = 0;
  getInt(eGL_NUM_EXTENSIONS, &numExts);
  for(GLint e = 0; e < numExts; e++)
  {
    const char *ext = (const char *)getStri(eGL_EXTENSIONS, (GLuint)e);

    extensionString += StringFormat::Fmt("[%d]: %s, ", e, ext);

    if(e > 0 && (e % 100) == 0)
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

  string missingExts = "";

#define REQUIRE_EXTENSION(extname) \
  if(!exts[extname])               \
    missingExts += STRINGIZE(extname) " ";

  // we require the below extensions on top of a 3.2 context. Some of these we could in theory
  // do without, but support for them is so widespread it's not worthwhile

  // used for reflecting out vertex bindings with most generality and remapping from old-style
  // Pointer functions
  // Should be possible to remove by falling back to reflecting vertex bindings the old way only and
  // remapping to 'fake' new-style bindings for representing in the UI
  REQUIRE_EXTENSION(ARB_vertex_attrib_binding);
  // for program introspection, needed for shader reflection.
  // Possible to remove by compiling shaders to SPIR-V and reflecting ourselves.
  REQUIRE_EXTENSION(ARB_program_interface_query);
  // strangely, this is the extension needed for layout(binding = X) on uniforms, textures, etc.
  // Possible to remove by #defining out the layout specifiers and manually setting the uniform
  // values.
  REQUIRE_EXTENSION(ARB_shading_language_420pack);
  // needed for program pipelines, glProgramUniform*, and reflecting shaders on their own
  // Possible to remove this with self-compiled SPIR-V for reflection - see above. Likewise
  // convenience for our own pipelines when replacing single shaders or such.
  REQUIRE_EXTENSION(ARB_separate_shader_objects);
  // needed for layout(location = X) on fragment shader outputs
  // similar to ARB_shading_language_420pack we could do without this by assigning locations in code
  REQUIRE_EXTENSION(ARB_explicit_attrib_location);
  // adds sampler objects so we can swap sampling behaviour when sampling from the capture's
  // textures
  // Possible to remove by manually pushing and popping all of the sampler state that we're
  // interested in on each texture instead of binding sampler objects.
  REQUIRE_EXTENSION(ARB_sampler_objects);

  if(!missingExts.empty())
  {
    RDCERR("RenderDoc requires these missing extensions: %s. Try updating your drivers.",
           missingExts.c_str());
    return true;
  }

  return false;
}

bool ValidateFunctionPointers(const GLHookSet &real)
{
  PFNGLGETSTRINGPROC *ptrs = (PFNGLGETSTRINGPROC *)&real;
  size_t num = sizeof(real) / sizeof(PFNGLGETSTRINGPROC);

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
  if(!real.func)                                                                       \
  {                                                                                    \
    RDCERR(                                                                            \
        "Missing function %s, required for replay. RenderDoc requires a 3.2 context, " \
        "and a handful of extensions, see the Documentation.",                         \
        #func);                                                                        \
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
  CHECK_PRESENT(glColorMaski)
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
  CHECK_PRESENT(glDisablei)
  CHECK_PRESENT(glDisableVertexAttribArray)
  CHECK_PRESENT(glDrawArrays)
  CHECK_PRESENT(glDrawArraysInstanced)
  CHECK_PRESENT(glDrawBuffers)
  CHECK_PRESENT(glDrawElements)
  CHECK_PRESENT(glDrawElementsBaseVertex)
  CHECK_PRESENT(glEnable)
  CHECK_PRESENT(glEnablei)
  CHECK_PRESENT(glEnableVertexAttribArray)
  CHECK_PRESENT(glEndQuery)
  CHECK_PRESENT(glFramebufferTexture)
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
  CHECK_PRESENT(glIsEnabledi)
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

void CheckExtensions(const GLHookSet &gl)
{
  GLint numExts = 0;
  if(gl.glGetIntegerv)
    gl.glGetIntegerv(eGL_NUM_EXTENSIONS, &numExts);

  RDCEraseEl(HasExt);

  if(gl.glGetString)
  {
    const char *vendor = (const char *)gl.glGetString(eGL_VENDOR);
    const char *renderer = (const char *)gl.glGetString(eGL_RENDERER);
    const char *version = (const char *)gl.glGetString(eGL_VERSION);

    // check whether we are using OpenGL ES
    // GL_VERSION for OpenGL ES:
    //   "OpenGL ES N.M vendor-specific information"
    if(strncmp(version, "OpenGL ES", 9) == 0)
    {
      IsGLES = true;

      int mj = int(version[10] - '0');
      int mn = int(version[12] - '0');
      GLCoreVersion = mj * 10 + mn;
    }

    RDCLOG("Vendor checks for %u (%s / %s / %s)", GLCoreVersion, vendor, renderer, version);
  }

  if(gl.glGetStringi)
  {
    for(int i = 0; i < numExts; i++)
    {
      const char *ext = (const char *)gl.glGetStringi(eGL_EXTENSIONS, (GLuint)i);

      if(ext == NULL || !ext[0] || !ext[1] || !ext[2] || !ext[3])
        continue;

      ext += 3;

#undef EXT_TO_CHECK
#define EXT_TO_CHECK(ver, glesver, extname)                                 \
  if((!IsGLES && GLCoreVersion >= ver) || !strcmp(ext, STRINGIZE(extname))) \
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
  }

  if(IsGLES)
  {
#undef EXT_TO_CHECK
#define EXT_TO_CHECK(ver, glesver, extname) \
  if(GLCoreVersion >= glesver)              \
    HasExt[extname] = true;

    EXTENSION_CHECKS()
  }
}

void DoVendorChecks(const GLHookSet &gl, GLPlatform &platform, GLWindowingData context)
{
  const char *vendor = "";

  if(gl.glGetString)
    vendor = (const char *)gl.glGetString(eGL_VENDOR);

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

  if(gl.glGetError && gl.glGetIntegeri_v)
  {
    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors(gl);

    GLint dummy = 0;
    gl.glGetIntegeri_v(eGL_VERTEX_BINDING_BUFFER, 0, &dummy);
    err = gl.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_vertex_buffer_query] = true;

      RDCWARN("Using AMD hack to avoid GL_VERTEX_BINDING_BUFFER");
    }
  }

  if(gl.glGetIntegerv && gl.glGetError && !IsGLES)
  {
    // NOTE: in case of OpenGL ES the GL_NV_polygon_mode extension can be used, however even if the
    // driver reports that the extension is supported, it always throws errors when we try to use it
    // (at least with the current NVIDIA driver)

    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors(gl);

    GLint dummy[2] = {0};
    gl.glGetIntegerv(eGL_POLYGON_MODE, dummy);
    err = gl.glGetError();

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
  else if(gl.glGetError && gl.glGenTextures && gl.glBindTexture && gl.glCopyImageSubData &&
          gl.glTexStorage2D && gl.glTexSubImage2D && gl.glTexParameteri && gl.glDeleteTextures &&
          HasExt[ARB_copy_image] && HasExt[ARB_texture_storage] && !IsGLES)
  {
    GLuint texs[2];
    gl.glGenTextures(2, texs);

    gl.glBindTexture(eGL_TEXTURE_2D, texs[0]);
    gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1, 1);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    gl.glBindTexture(eGL_TEXTURE_2D, texs[1]);
    gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1, 1);
    gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 0);

    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors(gl);

    gl.glCopyImageSubData(texs[0], eGL_TEXTURE_2D, 0, 0, 0, 0, texs[1], eGL_TEXTURE_2D, 0, 0, 0, 0,
                          1, 1, 1);

    err = gl.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] = true;

      RDCWARN("Using hack to avoid glCopyImageSubData on lowest mips of compressed texture");
    }

    gl.glBindTexture(eGL_TEXTURE_2D, 0);
    gl.glDeleteTextures(2, texs);

    ClearGLErrors(gl);

    //////////////////////////////////////////////////////////////////////////
    // Check copying cubemaps

    gl.glGenTextures(2, texs);

    const size_t dim = 32;

    char buf[dim * dim / 2];

    gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[0]);
    gl.glTexStorage2D(eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim, dim);
    gl.glTexParameteri(eGL_TEXTURE_CUBE_MAP, eGL_TEXTURE_MAX_LEVEL, 0);

    for(int i = 0; i < 6; i++)
    {
      memset(buf, 0xba + i, sizeof(buf));
      gl.glCompressedTexSubImage2D(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, 0, 0, dim, dim,
                                   eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim * dim / 2, buf);
    }

    gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[1]);
    gl.glTexStorage2D(eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim, dim);
    gl.glTexParameteri(eGL_TEXTURE_CUBE_MAP, eGL_TEXTURE_MAX_LEVEL, 0);

    gl.glCopyImageSubData(texs[0], eGL_TEXTURE_CUBE_MAP, 0, 0, 0, 0, texs[1], eGL_TEXTURE_CUBE_MAP,
                          0, 0, 0, 0, dim, dim, 6);

    char cmp[dim * dim / 2];

    gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[0]);

    for(int i = 0; i < 6; i++)
    {
      memset(buf, 0xba + i, sizeof(buf));
      RDCEraseEl(cmp);
      gl.glGetCompressedTexImage(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, cmp);

      RDCCOMPILE_ASSERT(sizeof(buf) == sizeof(buf), "Buffers are not matching sizes");

      if(memcmp(buf, cmp, sizeof(buf)))
      {
        RDCERR("glGetTexImage from the source texture returns incorrect data!");
        VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] =
            true;    // to be safe, enable the hack
      }
    }

    gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[1]);

    for(int i = 0; i < 6; i++)
    {
      memset(buf, 0xba + i, sizeof(buf));
      RDCEraseEl(cmp);
      gl.glGetCompressedTexImage(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X + i), 0, cmp);

      RDCCOMPILE_ASSERT(sizeof(buf) == sizeof(buf), "Buffers are not matching sizes");

      if(memcmp(buf, cmp, sizeof(buf)))
      {
        RDCWARN("Using hack to avoid glCopyImageSubData on cubemap textures");
        VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] = true;
        break;
      }
    }

    gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, 0);
    gl.glDeleteTextures(2, texs);

    ClearGLErrors(gl);
  }

  if(gl.glGetError && gl.glGenProgramPipelines && gl.glDeleteProgramPipelines &&
     gl.glGetProgramPipelineiv && HasExt[ARB_compute_shader] && HasExt[ARB_program_interface_query])
  {
    GLuint pipe = 0;
    gl.glGenProgramPipelines(1, &pipe);

    // clear all error flags.
    GLenum err = eGL_NONE;
    ClearGLErrors(gl);

    GLint dummy = 0;
    gl.glGetProgramPipelineiv(pipe, eGL_COMPUTE_SHADER, &dummy);

    err = gl.glGetError();

    if(err != eGL_NONE)
    {
      // if we got an error trying to query that, we should enable this hack
      VendorCheck[VendorCheck_AMD_pipeline_compute_query] = true;

      RDCWARN("Using hack to avoid glGetProgramPipelineiv with GL_COMPUTE_SHADER");
    }

    gl.glDeleteProgramPipelines(1, &pipe);
  }

  // only do this when we have a proper context e.g. on windows where an old
  // context is first created. Check to see if FBOs or VAOs are shared between
  // contexts.
  if((IsGLES || GLCoreVersion >= 32) && gl.glGenVertexArrays && gl.glBindVertexArray &&
     gl.glDeleteVertexArrays && gl.glGenFramebuffers && gl.glBindFramebuffer &&
     gl.glDeleteFramebuffers)
  {
    // gen & create an FBO and VAO
    GLuint fbo = 0;
    GLuint vao = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbo);
    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);

    // make a context that shares with the current one, and switch to it
    GLWindowingData child = platform.MakeContext(context);

    if(child.ctx)
    {
      // switch to child
      platform.MakeContextCurrent(child);

      // these shouldn't be visible
      VendorCheck[VendorCheck_EXT_fbo_shared] = (gl.glIsFramebuffer(fbo) != GL_FALSE);
      VendorCheck[VendorCheck_EXT_vao_shared] = (gl.glIsVertexArray(vao) != GL_FALSE);

      if(VendorCheck[VendorCheck_EXT_fbo_shared])
        RDCWARN("FBOs are shared on this implementation");
      if(VendorCheck[VendorCheck_EXT_vao_shared])
        RDCWARN("VAOs are shared on this implementation");

      // switch back to context
      platform.MakeContextCurrent(context);

      platform.DeleteContext(child);
    }

    gl.glDeleteFramebuffers(1, &fbo);
    gl.glDeleteVertexArrays(1, &vao);
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
  VendorCheck[VendorCheck_NV_ClearNamedFramebufferfiBugs] = true;

  // glVertexArrayElementBuffer doesn't update the GL_ELEMENT_ARRAY_BUFFER_BINDING global query,
  // when binding the VAO subsequently *will*.
  // I'm not sure if that's correct (weird) behaviour or buggy, but we can work around it just by
  // avoiding use of the DSA function and always doing our emulated version.
  VendorCheck[VendorCheck_AMD_vertex_array_elem_buffer_query] = true;

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

const GLHookSet *GLMarkerRegion::gl;

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
  if(gl == NULL || !HasExt[KHR_debug] || !gl->glPushDebugGroup)
    return;

  gl->glPushDebugGroup(source, id, -1, marker.c_str());
}

void GLMarkerRegion::Set(const std::string &marker, GLenum source, GLuint id, GLenum severity)
{
  if(gl == NULL || !HasExt[KHR_debug] || !gl->glDebugMessageInsert)
    return;

  gl->glDebugMessageInsert(source, eGL_DEBUG_TYPE_MARKER, id, severity, -1, marker.c_str());
}

void GLMarkerRegion::End()
{
  if(gl == NULL || !HasExt[KHR_debug] || !gl->glPopDebugGroup)
    return;

  gl->glPopDebugGroup();
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
}

bool GLInitParams::IsSupportedVersion(uint64_t ver)
{
  if(ver == CurrentVersion)
    return true;

  // we can check other older versions we support here.

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
}

INSTANTIATE_SERIALISE_TYPE(GLInitParams);

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
    case eGL_SHORT:
    case eGL_HALF_FLOAT_OES:
    case eGL_HALF_FLOAT: return 2;
    case eGL_UNSIGNED_INT:
    case eGL_INT:
    case eGL_FLOAT: return 4;
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

void ClearGLErrors(const GLHookSet &gl)
{
  int i = 0;
  GLenum err = gl.glGetError();
  while(err)
  {
    err = gl.glGetError();
    i++;
    if(i > 100)
    {
      RDCERR("Couldn't clear GL errors - something very wrong!");
      return;
    }
  }
}

GLuint GetBoundVertexBuffer(const GLHookSet &gl, GLuint i)
{
  GLuint buffer = 0;

  if(VendorCheck[VendorCheck_AMD_vertex_buffer_query])
    gl.glGetVertexAttribiv(i, eGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, (GLint *)&buffer);
  else
    gl.glGetIntegeri_v(eGL_VERTEX_BINDING_BUFFER, i, (GLint *)&buffer);

  return buffer;
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
      ret.minify = (minf == eGL_LINEAR_MIPMAP_LINEAR) ? FilterMode::Linear : FilterMode::Point;
      ret.mip = FilterMode::Point;
    }

    ret.magnify = (magf == eGL_LINEAR) ? FilterMode::Linear : FilterMode::Point;
  }
  ret.func = shadowSampler ? FilterFunc::Comparison : FilterFunc::Normal;

  return ret;
}

CompareFunc MakeCompareFunc(GLenum func)
{
  switch(func)
  {
    case GL_NEVER: return CompareFunc::Never;
    case GL_LESS: return CompareFunc::Less;
    case GL_EQUAL: return CompareFunc::Equal;
    case GL_LEQUAL: return CompareFunc::LessEqual;
    case GL_GREATER: return CompareFunc::Greater;
    case GL_NOTEQUAL: return CompareFunc::NotEqual;
    case GL_GEQUAL: return CompareFunc::GreaterEqual;
    case GL_ALWAYS: return CompareFunc::AlwaysTrue;
    default: break;
  }

  return CompareFunc::AlwaysTrue;
}

StencilOp MakeStencilOp(GLenum op)
{
  switch(op)
  {
    case eGL_KEEP: return StencilOp::Keep;
    case eGL_ZERO: return StencilOp::Zero;
    case eGL_REPLACE: return StencilOp::Replace;
    case eGL_INCR: return StencilOp::IncSat;
    case eGL_DECR: return StencilOp::DecSat;
    case eGL_INVERT: return StencilOp::Invert;
    case eGL_INCR_WRAP: return StencilOp::IncWrap;
    case eGL_DECR_WRAP: return StencilOp::DecWrap;
    default: break;
  }

  return StencilOp::Keep;
}

LogicOp MakeLogicOp(GLenum op)
{
  switch(op)
  {
    case GL_CLEAR: return LogicOp::Clear;
    case GL_AND: return LogicOp::And;
    case GL_AND_REVERSE: return LogicOp::AndReverse;
    case GL_COPY: return LogicOp::Copy;
    case GL_AND_INVERTED: return LogicOp::AndInverted;
    case GL_NOOP: return LogicOp::NoOp;
    case GL_XOR: return LogicOp::Xor;
    case GL_OR: return LogicOp::Or;
    case GL_NOR: return LogicOp::Nor;
    case GL_EQUIV: return LogicOp::Equivalent;
    case GL_INVERT: return LogicOp::Invert;
    case GL_OR_REVERSE: return LogicOp::OrReverse;
    case GL_COPY_INVERTED: return LogicOp::CopyInverted;
    case GL_OR_INVERTED: return LogicOp::OrInverted;
    case GL_NAND: return LogicOp::Nand;
    case GL_SET: return LogicOp::Set;
    default: break;
  }

  return LogicOp::NoOp;
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

BlendOp MakeBlendOp(GLenum op)
{
  switch(op)
  {
    case eGL_FUNC_ADD: return BlendOp::Add;
    case eGL_FUNC_SUBTRACT: return BlendOp::Subtract;
    case eGL_FUNC_REVERSE_SUBTRACT: return BlendOp::ReversedSubtract;
    case eGL_MIN: return BlendOp::Minimum;
    case eGL_MAX: return BlendOp::Maximum;
    default: break;
  }

  return BlendOp::Add;
}

const char *BlendString(GLenum blendenum)
{
  switch(blendenum)
  {
    case eGL_FUNC_ADD: return "ADD";
    case eGL_FUNC_SUBTRACT: return "SUBTRACT";
    case eGL_FUNC_REVERSE_SUBTRACT: return "INV_SUBTRACT";
    case eGL_MIN: return "MIN";
    case eGL_MAX: return "MAX";
    case GL_ZERO: return "ZERO";
    case GL_ONE: return "ONE";
    case eGL_SRC_COLOR: return "SRC_COLOR";
    case eGL_ONE_MINUS_SRC_COLOR: return "INV_SRC_COLOR";
    case eGL_DST_COLOR: return "DST_COLOR";
    case eGL_ONE_MINUS_DST_COLOR: return "INV_DST_COLOR";
    case eGL_SRC_ALPHA: return "SRC_ALPHA";
    case eGL_ONE_MINUS_SRC_ALPHA: return "INV_SRC_ALPHA";
    case eGL_DST_ALPHA: return "DST_ALPHA";
    case eGL_ONE_MINUS_DST_ALPHA: return "INV_DST_ALPHA";
    case eGL_CONSTANT_COLOR: return "CONST_COLOR";
    case eGL_ONE_MINUS_CONSTANT_COLOR: return "INV_CONST_COLOR";
    case eGL_CONSTANT_ALPHA: return "CONST_ALPHA";
    case eGL_ONE_MINUS_CONSTANT_ALPHA: return "INV_CONST_ALPHA";
    case eGL_SRC_ALPHA_SATURATE: return "SRC_ALPHA_SAT";
    case eGL_SRC1_COLOR: return "SRC1_COL";
    case eGL_ONE_MINUS_SRC1_COLOR: return "INV_SRC1_COL";
    case eGL_SRC1_ALPHA: return "SRC1_ALPHA";
    case eGL_ONE_MINUS_SRC1_ALPHA: return "INV_SRC1_ALPHA";
    default: break;
  }

  static string unknown = ToStr(blendenum).substr(3);    // 3 = strlen("GL_");

  RDCERR("Unknown blend enum: %s", unknown.c_str());

  return unknown.c_str();
}

const char *SamplerString(GLenum smpenum)
{
  switch(smpenum)
  {
    case eGL_NONE: return "NONE";
    case eGL_NEAREST: return "NEAREST";
    case eGL_LINEAR: return "LINEAR";
    case eGL_NEAREST_MIPMAP_NEAREST: return "NEAREST_MIP_NEAREST";
    case eGL_LINEAR_MIPMAP_NEAREST: return "LINEAR_MIP_NEAREST";
    case eGL_NEAREST_MIPMAP_LINEAR: return "NEAREST_MIP_LINEAR";
    case eGL_LINEAR_MIPMAP_LINEAR: return "LINEAR_MIP_LINEAR";
    case eGL_CLAMP_TO_EDGE: return "CLAMP_EDGE";
    case eGL_MIRRORED_REPEAT: return "MIRR_REPEAT";
    case eGL_REPEAT: return "REPEAT";
    case eGL_MIRROR_CLAMP_TO_EDGE: return "MIRR_CLAMP_EDGE";
    case eGL_CLAMP_TO_BORDER: return "CLAMP_BORDER";
    default: break;
  }

  static string unknown = ToStr(smpenum).substr(3);    // 3 = strlen("GL_");

  RDCERR("Unknown blend enum: %s", unknown.c_str());

  return unknown.c_str();
}

ResourceFormat MakeResourceFormat(const GLHookSet &gl, GLenum target, GLenum fmt)
{
  ResourceFormat ret;

  ret.type = ResourceFormatType::Regular;

  if(fmt == eGL_NONE)
  {
    ret.type = ResourceFormatType::Undefined;
    return ret;
  }

  // special handling for formats that don't query neatly
  if(fmt == eGL_LUMINANCE8_EXT || fmt == eGL_INTENSITY8_EXT || fmt == eGL_ALPHA8_EXT)
  {
    ret.compByteWidth = 1;
    ret.compCount = 1;
    ret.compType = CompType::UNorm;
    ret.srgbCorrected = false;
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

      case eGL_ETC1_RGB8_OES:
      case eGL_COMPRESSED_RGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_ETC2: ret.compCount = 3; break;
      case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: ret.compCount = 4; break;

      default: break;
    }

    switch(fmt)
    {
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
      case eGL_COMPRESSED_SRGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: ret.srgbCorrected = true; break;
      default: break;
    }

    ret.compType = CompType::UNorm;

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
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: ret.type = ResourceFormatType::ASTC; break;
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
  gl.glGetInternalformativ(target, fmt, eGL_COLOR_COMPONENTS, sizeof(GLint), &iscol);
  gl.glGetInternalformativ(target, fmt, eGL_DEPTH_COMPONENTS, sizeof(GLint), &isdepth);
  gl.glGetInternalformativ(target, fmt, eGL_STENCIL_COMPONENTS, sizeof(GLint), &isstencil);

  if(iscol == GL_TRUE)
  {
    if(fmt == eGL_BGRA8_EXT || fmt == eGL_BGRA)
      ret.bgraOrder = true;

    // colour format

    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_SIZE, sizeof(GLint), &data[0]);
    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_SIZE, sizeof(GLint), &data[1]);
    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_SIZE, sizeof(GLint), &data[2]);
    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_SIZE, sizeof(GLint), &data[3]);

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

    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_TYPE, sizeof(GLint), &data[0]);
    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_TYPE, sizeof(GLint), &data[1]);
    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_TYPE, sizeof(GLint), &data[2]);
    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_TYPE, sizeof(GLint), &data[3]);

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

    gl.glGetInternalformativ(target, fmt, eGL_COLOR_ENCODING, sizeof(GLint), &data[0]);
    ret.srgbCorrected = (edata[0] == eGL_SRGB);
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
      case eGL_DEPTH_COMPONENT32:
      case eGL_DEPTH_COMPONENT32F:
        ret.compByteWidth = 4;
        ret.compCount = 1;
        break;
      case eGL_DEPTH24_STENCIL8: ret.type = ResourceFormatType::D24S8; break;
      case eGL_DEPTH32F_STENCIL8: ret.type = ResourceFormatType::D32S8; break;
      case eGL_STENCIL_INDEX8: ret.type = ResourceFormatType::S8; break;
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
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT
                                  : eGL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        else
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                                  : eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
      }
      case ResourceFormatType::BC2:
        ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                                : eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
      case ResourceFormatType::BC3:
        ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
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
        ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB
                                : eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
        break;
      case ResourceFormatType::ETC2:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_ETC2 : eGL_COMPRESSED_RGB8_ETC2;
        else
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2
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
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
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
      case ResourceFormatType::S8: ret = eGL_STENCIL_INDEX8; break;
      case ResourceFormatType::Undefined: return eGL_NONE;
      default: RDCERR("Unsupported resource format type %u", fmt.type); break;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.srgbCorrected)
    {
      ret = eGL_SRGB8_ALPHA8;
    }
    else if(fmt.bgraOrder)
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
    if(fmt.srgbCorrected)
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

Topology MakePrimitiveTopology(const GLHookSet &gl, GLenum Topo)
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
      gl.glGetIntegerv(eGL_PATCH_VERTICES, &patchCount);
      return PatchList_Topology(patchCount);
    }
  }
}

// bit of a hack, to work around C4127: conditional expression is constant
// on template parameters
template <typename T>
T CheckConstParam(T t);
template <>
bool CheckConstParam(bool t)
{
  return t;
}

template <const bool CopyUniforms, const bool SerialiseUniforms>
static void ForAllProgramUniforms(const GLHookSet &gl, Serialiser *ser, GLuint progSrc,
                                  GLuint progDst, map<GLint, GLint> *locTranslate, bool writing)
{
  const bool ReadSourceProgram = CopyUniforms || (SerialiseUniforms && writing);
  const bool WriteDestProgram = CopyUniforms || (SerialiseUniforms && !writing);

  RDCCOMPILE_ASSERT((CopyUniforms && !SerialiseUniforms) || (!CopyUniforms && SerialiseUniforms),
                    "Invalid call to ForAllProgramUniforms");

  GLint numUniforms = 0;
  if(CheckConstParam(ReadSourceProgram))
    gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &numUniforms);

  if(CheckConstParam(SerialiseUniforms))
  {
    // get accurate count of uniforms not in UBOs
    GLint numSerialisedUniforms = 0;

    for(GLint i = 0; writing && i < numUniforms; i++)
    {
      GLenum prop = eGL_BLOCK_INDEX;
      GLint blockIdx;
      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, 1, &prop, 1, NULL, (GLint *)&blockIdx);

      if(blockIdx >= 0)
        continue;

      numSerialisedUniforms++;
    }

    ser->Serialise("numUniforms", numSerialisedUniforms);

    if(!writing)
      numUniforms = numSerialisedUniforms;
  }

  const size_t numProps = 5;
  GLenum resProps[numProps] = {
      eGL_BLOCK_INDEX, eGL_TYPE, eGL_NAME_LENGTH, eGL_ARRAY_SIZE, eGL_LOCATION,
  };

  for(GLint i = 0; i < numUniforms; i++)
  {
    GLenum type = eGL_NONE;
    int32_t arraySize = 0;
    int32_t srcLocation = 0;
    string basename;
    bool isArray = false;

    if(CheckConstParam(ReadSourceProgram))
    {
      GLint values[numProps];
      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, numProps, resProps, numProps, NULL, values);

      // we don't need to consider uniforms within UBOs
      if(values[0] >= 0)
        continue;

      type = (GLenum)values[1];
      arraySize = values[3];
      srcLocation = values[4];

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_UNIFORM, i, values[2], NULL, n);

      if(arraySize > 1)
      {
        isArray = true;

        size_t len = strlen(n);

        if(n[len - 3] == '[' && n[len - 2] == '0' && n[len - 1] == ']')
          n[len - 3] = 0;
      }
      else
      {
        arraySize = 1;
      }

      basename = n;
    }

    if(CheckConstParam(SerialiseUniforms))
    {
      ser->Serialise("type", type);
      ser->Serialise("arraySize", arraySize);
      ser->Serialise("basename", basename);
      ser->Serialise("isArray", isArray);
    }

    double dv[16];
    float *fv = (float *)dv;
    int32_t *iv = (int32_t *)dv;
    uint32_t *uiv = (uint32_t *)dv;

    for(GLint arr = 0; arr < arraySize; arr++)
    {
      string name = basename;

      if(isArray)
      {
        name += StringFormat::Fmt("[%d]", arr);

        if(CheckConstParam(ReadSourceProgram))
          srcLocation = gl.glGetUniformLocation(progSrc, name.c_str());
      }

      if(CheckConstParam(SerialiseUniforms))
        ser->Serialise("srcLocation", srcLocation);

      GLint newloc = 0;
      if(CheckConstParam(WriteDestProgram))
      {
        newloc = gl.glGetUniformLocation(progDst, name.c_str());
        if(locTranslate)
          (*locTranslate)[srcLocation] = newloc;
      }

      if(CheckConstParam(CopyUniforms) && newloc == -1)
        continue;

      if(CheckConstParam(ReadSourceProgram))
      {
        switch(type)
        {
          case eGL_FLOAT_MAT4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT4x3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT4x2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT3x4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT3x2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT2x4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_MAT2x3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_DOUBLE_MAT4: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT4x3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT4x2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT3x4: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT3x2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT2x4: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_MAT2x3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_FLOAT: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_DOUBLE: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_VEC2: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_VEC3: gl.glGetUniformdv(progSrc, srcLocation, dv); break;
          case eGL_DOUBLE_VEC4:
            gl.glGetUniformdv(progSrc, srcLocation, dv);
            break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_1D:
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_1D_SHADOW:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_1D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_1D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_SAMPLER_2D_RECT:
          case eGL_SAMPLER_2D_RECT_SHADOW:
          case eGL_INT_SAMPLER_1D:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_1D_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D_RECT:
          case eGL_UNSIGNED_INT_SAMPLER_1D:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_IMAGE_1D:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_2D_RECT:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_1D_ARRAY:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_IMAGE_2D_MULTISAMPLE:
          case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_IMAGE_1D:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_2D_RECT:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_1D_ARRAY:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_1D:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_ATOMIC_COUNTER:
          case eGL_INT: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_INT_VEC2: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_INT_VEC3: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_INT_VEC4: gl.glGetUniformiv(progSrc, srcLocation, iv); break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(type).c_str());
        }
      }

      if(CheckConstParam(SerialiseUniforms))
        ser->SerialisePODArray<16>("data", dv);

      if(CheckConstParam(WriteDestProgram))
      {
        switch(type)
        {
          case eGL_FLOAT_MAT4: gl.glProgramUniformMatrix4fv(progDst, newloc, 1, false, fv); break;
          case eGL_FLOAT_MAT4x3:
            gl.glProgramUniformMatrix4x3fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT4x2:
            gl.glProgramUniformMatrix4x2fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3: gl.glProgramUniformMatrix3fv(progDst, newloc, 1, false, fv); break;
          case eGL_FLOAT_MAT3x4:
            gl.glProgramUniformMatrix3x4fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT3x2:
            gl.glProgramUniformMatrix3x2fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2: gl.glProgramUniformMatrix2fv(progDst, newloc, 1, false, fv); break;
          case eGL_FLOAT_MAT2x4:
            gl.glProgramUniformMatrix2x4fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_FLOAT_MAT2x3:
            gl.glProgramUniformMatrix2x3fv(progDst, newloc, 1, false, fv);
            break;
          case eGL_DOUBLE_MAT4: gl.glProgramUniformMatrix4dv(progDst, newloc, 1, false, dv); break;
          case eGL_DOUBLE_MAT4x3:
            gl.glProgramUniformMatrix4x3dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT4x2:
            gl.glProgramUniformMatrix4x2dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3: gl.glProgramUniformMatrix3dv(progDst, newloc, 1, false, dv); break;
          case eGL_DOUBLE_MAT3x4:
            gl.glProgramUniformMatrix3x4dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT3x2:
            gl.glProgramUniformMatrix3x2dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2: gl.glProgramUniformMatrix2dv(progDst, newloc, 1, false, dv); break;
          case eGL_DOUBLE_MAT2x4:
            gl.glProgramUniformMatrix2x4dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_DOUBLE_MAT2x3:
            gl.glProgramUniformMatrix2x3dv(progDst, newloc, 1, false, dv);
            break;
          case eGL_FLOAT: gl.glProgramUniform1fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC2: gl.glProgramUniform2fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC3: gl.glProgramUniform3fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC4: gl.glProgramUniform4fv(progDst, newloc, 1, fv); break;
          case eGL_DOUBLE: gl.glProgramUniform1dv(progDst, newloc, 1, dv); break;
          case eGL_DOUBLE_VEC2: gl.glProgramUniform2dv(progDst, newloc, 1, dv); break;
          case eGL_DOUBLE_VEC3: gl.glProgramUniform3dv(progDst, newloc, 1, dv); break;
          case eGL_DOUBLE_VEC4:
            gl.glProgramUniform4dv(progDst, newloc, 1, dv);
            break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_1D:
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_1D_SHADOW:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_1D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_1D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_SAMPLER_2D_RECT:
          case eGL_SAMPLER_2D_RECT_SHADOW:
          case eGL_INT_SAMPLER_1D:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_1D_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D_RECT:
          case eGL_UNSIGNED_INT_SAMPLER_1D:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D_RECT:
          case eGL_IMAGE_1D:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_2D_RECT:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_1D_ARRAY:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_IMAGE_2D_MULTISAMPLE:
          case eGL_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_IMAGE_1D:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_2D_RECT:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_1D_ARRAY:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_1D:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_2D_RECT:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_1D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_ATOMIC_COUNTER:
          case eGL_INT: gl.glProgramUniform1iv(progDst, newloc, 1, iv); break;
          case eGL_INT_VEC2: gl.glProgramUniform2iv(progDst, newloc, 1, iv); break;
          case eGL_INT_VEC3: gl.glProgramUniform3iv(progDst, newloc, 1, iv); break;
          case eGL_INT_VEC4: gl.glProgramUniform4iv(progDst, newloc, 1, iv); break;
          case eGL_UNSIGNED_INT:
          case eGL_BOOL: gl.glProgramUniform1uiv(progDst, newloc, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC2:
          case eGL_BOOL_VEC2: gl.glProgramUniform2uiv(progDst, newloc, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC3:
          case eGL_BOOL_VEC3: gl.glProgramUniform3uiv(progDst, newloc, 1, uiv); break;
          case eGL_UNSIGNED_INT_VEC4:
          case eGL_BOOL_VEC4: gl.glProgramUniform4uiv(progDst, newloc, 1, uiv); break;
          default: RDCERR("Unhandled uniform type '%s'", ToStr(type).c_str());
        }
      }
    }
  }

  GLint numUBOs = 0;
  if(CheckConstParam(ReadSourceProgram))
    gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

  if(CheckConstParam(SerialiseUniforms))
    ser->Serialise("numUBOs", numUBOs);

  for(GLint i = 0; i < numUBOs; i++)
  {
    GLenum prop = eGL_BUFFER_BINDING;
    uint32_t bind = 0;
    string name;

    if(CheckConstParam(ReadSourceProgram))
    {
      gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM_BLOCK, i, 1, &prop, 1, NULL, (GLint *)&bind);

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_UNIFORM_BLOCK, i, 1023, NULL, n);

      name = n;
    }

    if(CheckConstParam(SerialiseUniforms))
    {
      ser->Serialise("bind", bind);
      ser->Serialise("name", name);
    }

    if(CheckConstParam(WriteDestProgram))
    {
      GLuint idx = gl.glGetUniformBlockIndex(progDst, name.c_str());
      if(idx != GL_INVALID_INDEX)
        gl.glUniformBlockBinding(progDst, idx, bind);
    }
  }

  GLint numSSBOs = 0;
  if(CheckConstParam(ReadSourceProgram) && HasExt[ARB_shader_storage_buffer_object])
    gl.glGetProgramInterfaceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);

  if(CheckConstParam(SerialiseUniforms))
    ser->Serialise("numSSBOs", numSSBOs);

  for(GLint i = 0; i < numSSBOs; i++)
  {
    GLenum prop = eGL_BUFFER_BINDING;
    uint32_t bind = 0;
    string name;

    if(CheckConstParam(ReadSourceProgram))
    {
      gl.glGetProgramResourceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1, &prop, 1, NULL,
                                (GLint *)&bind);

      char n[1024] = {0};
      gl.glGetProgramResourceName(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1023, NULL, n);

      name = n;
    }

    if(CheckConstParam(SerialiseUniforms))
    {
      ser->Serialise("bind", bind);
      ser->Serialise("name", name);
    }

    if(CheckConstParam(WriteDestProgram))
    {
      GLuint idx = gl.glGetProgramResourceIndex(progDst, eGL_SHADER_STORAGE_BLOCK, name.c_str());
      if(idx != GL_INVALID_INDEX)
      {
        if(gl.glShaderStorageBlockBinding)
        {
          gl.glShaderStorageBlockBinding(progDst, i, bind);
        }
        else
        {
          // TODO glShaderStorageBlockBinding is not core GLES
          RDCERR("glShaderStorageBlockBinding is not supported!");
        }
      }
    }
  }
}

void CopyProgramUniforms(const GLHookSet &gl, GLuint progSrc, GLuint progDst)
{
  const bool CopyUniforms = true;
  const bool SerialiseUniforms = false;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(gl, NULL, progSrc, progDst, NULL, false);
}

void SerialiseProgramUniforms(const GLHookSet &gl, Serialiser *ser, GLuint prog,
                              map<GLint, GLint> *locTranslate, bool writing)
{
  const bool CopyUniforms = false;
  const bool SerialiseUniforms = true;
  ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(gl, ser, prog, prog, locTranslate, writing);
}

void CopyProgramAttribBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                               ShaderReflection *refl)
{
  // copy over attrib bindings
  for(const SigParameter &sig : refl->InputSig)
  {
    // skip built-ins
    if(sig.systemValue != ShaderBuiltin::Undefined)
      continue;

    GLint idx = gl.glGetAttribLocation(progsrc, sig.varName.c_str());
    if(idx >= 0)
      gl.glBindAttribLocation(progdst, (GLuint)idx, sig.varName.c_str());
  }
}

void CopyProgramFragDataBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                                 ShaderReflection *refl)
{
  uint64_t used = 0;

  // copy over fragdata bindings
  for(size_t i = 0; i < refl->OutputSig.size(); i++)
  {
    // only look at colour outputs (should be the only outputs from fs)
    if(refl->OutputSig[i].systemValue != ShaderBuiltin::ColorOutput)
      continue;

    if(!strncmp("gl_", refl->OutputSig[i].varName.c_str(), 3))
      continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix

    GLint idx = gl.glGetFragDataLocation(progsrc, refl->OutputSig[i].varName.c_str());
    if(idx >= 0)
    {
      uint64_t mask = 1ULL << idx;

      if(used & mask)
      {
        RDCWARN("Multiple signatures bound to output %zu, ignoring %s", i,
                refl->OutputSig[i].varName.c_str());
        continue;
      }

      used |= mask;

      if(gl.glBindFragDataLocation)
      {
        gl.glBindFragDataLocation(progdst, (GLuint)idx, refl->OutputSig[i].varName.c_str());
      }
      else
      {
        // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
        // TODO what to do if that extension is not supported
        RDCERR("glBindFragDataLocation is not supported!");
      }
    }
  }
}

void SerialiseProgramBindings(const GLHookSet &gl, Serialiser *ser, GLuint prog, bool writing)
{
  char name[128] = {0};

  for(int sigType = 0; sigType < 2; sigType++)
  {
    GLenum sigEnum = (sigType == 0 ? eGL_PROGRAM_INPUT : eGL_PROGRAM_OUTPUT);

    uint64_t used = 0;

    int32_t numAttrs = 0;

    if(writing)
      gl.glGetProgramInterfaceiv(prog, sigEnum, eGL_ACTIVE_RESOURCES, (GLint *)&numAttrs);

    ser->Serialise("numAttrs", numAttrs);

    for(GLint i = 0; i < numAttrs; i++)
    {
      int32_t idx = -1;

      if(writing)
      {
        gl.glGetProgramResourceName(prog, sigEnum, i, 128, NULL, name);

        if(sigType == 0)
          idx = gl.glGetAttribLocation(prog, name);
        else
          idx = gl.glGetFragDataLocation(prog, name);
      }

      string n = name;

      ser->Serialise("name", n);
      ser->Serialise("idx", idx);

      if(!writing && idx >= 0)
      {
        uint64_t mask = 1ULL << idx;

        if(used & mask)
        {
          RDCWARN("Multiple %s items bound to location %d, ignoring %s",
                  sigType == 0 ? "attrib" : "fragdata", idx, n.c_str());
          continue;
        }

        used |= mask;

        if(!strncmp("gl_", n.c_str(), 3))
          continue;    // GL_INVALID_OPERATION if name starts with reserved gl_ prefix (for both
                       // glBindAttribLocation and glBindFragDataLocation)

        if(sigType == 0)
        {
          gl.glBindAttribLocation(prog, (GLuint)idx, n.c_str());
        }
        else
        {
          if(gl.glBindFragDataLocation)
          {
            gl.glBindFragDataLocation(prog, (GLuint)idx, n.c_str());
          }
          else
          {
            // glBindFragDataLocation is not core GLES, but it is in GL_EXT_blend_func_extended
            // TODO what to do if that extension is not supported
            RDCERR("glBindFragDataLocation is not supported!");
          }
        }
      }
    }
  }
}


#if ENABLED(ENABLE_UNIT_TESTS)

#undef None

#include "3rdparty/catch/catch.hpp"

#define CATCH_TOSTR(type)                                                  \
  namespace Catch                                                          \
  {                                                                        \
  template <>                                                              \
  struct StringMaker<type>                                                 \
  {                                                                        \
    static std::string convert(type const &value) { return ToStr(value); } \
  };                                                                       \
  }

CATCH_TOSTR(CompType);
CATCH_TOSTR(GLenum);

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
  };

  // we use our emulated queries for the format, as we don't want to init a context here, and anyway
  // we'd rather have an isolated test-case of only our code, not be testing a GL driver
  // implementation
  GLHookSet gl;
  glEmulate::EmulateRequiredExtensions(&gl);

  SECTION("Only GL_NONE returns unknown")
  {
    for(GLenum f : supportedFormats)
    {
      ResourceFormat fmt = MakeResourceFormat(gl, eGL_TEXTURE_2D, f);

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

      ResourceFormat fmt = MakeResourceFormat(gl, eGL_TEXTURE_2D, f);

      // we don't support ASTC formats currently
      if(fmt.type == ResourceFormatType::ASTC)
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
      ResourceFormat fmt = MakeResourceFormat(gl, eGL_TEXTURE_2D, f);

      if(fmt.type != ResourceFormatType::Regular)
        continue;

      INFO("Format is " << ToStr(f));

      uint32_t size = fmt.compCount * fmt.compByteWidth * 123 * 456;

      // this takes up a full int, even if the byte width is listed as 3.
      if(f == eGL_DEPTH_COMPONENT24)
        size = fmt.compCount * 4 * 123 * 456;

      CHECK(size == GetByteSize(123, 456, 1, GetBaseFormat(f), GetDataType(f)));
    }
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
