/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include "gles_common.h"
#include "core/core.h"
#include "serialise/string_utils.h"
#include "gles_driver.h"

bool ExtensionSupported[ExtensionSupported_Count];
bool VendorCheck[VendorCheck_Count];

int GLCoreVersion = 0;
bool GLIsCore = false;

// simple wrapper for OS functions to make/delete a context
GLESWindowingData MakeContext(GLESWindowingData share);
void DeleteContext(GLESWindowingData context);

void MakeContextCurrent(GLESWindowingData data);

void DoExtensionChecks(const GLHookSet &gl)
{
  GLint numExts = 0;
  if(gl.glGetIntegerv)
    gl.glGetIntegerv(eGL_NUM_EXTENSIONS, &numExts);

  RDCEraseEl(ExtensionSupported);
  RDCEraseEl(VendorCheck);

  if(gl.glGetString)
  {
    const char *vendor = (const char *)gl.glGetString(eGL_VENDOR);
    const char *renderer = (const char *)gl.glGetString(eGL_RENDERER);
    const char *version = (const char *)gl.glGetString(eGL_VERSION);

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

#define EXT_CHECK(extname)             \
  if(!strcmp(ext, STRINGIZE(extname))) \
    ExtensionSupported[CONCAT(ExtensionSupported_, extname)] = true;

      EXT_CHECK(ARB_clip_control);
      EXT_CHECK(ARB_enhanced_layouts);
      EXT_CHECK(EXT_polygon_offset_clamp);
      EXT_CHECK(KHR_blend_equation_advanced_coherent);
      EXT_CHECK(EXT_raster_multisample);
      EXT_CHECK(ARB_indirect_parameters);
      EXT_CHECK(EXT_depth_bounds_test);
      EXT_CHECK(EXT_clip_cull_distance);
      EXT_CHECK(NV_polygon_mode);
      EXT_CHECK(NV_viewport_array);
      EXT_CHECK(OES_viewport_array);
      EXT_CHECK(EXT_buffer_storage);
      EXT_CHECK(EXT_texture_storage);
      EXT_CHECK(EXT_map_buffer_range);
      EXT_CHECK(EXT_base_instance);
      EXT_CHECK(EXT_debug_label);
      EXT_CHECK(EXT_multisample_compatibility);

#undef EXT_CHECK
    }
  }
}

void DoVendorChecks(const GLHookSet &gl, GLESWindowingData context)
{
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

  if(gl.glGetError && gl.glGenProgramPipelines && gl.glDeleteProgramPipelines &&
     gl.glGetProgramPipelineiv)
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
  if(GLCoreVersion >= 32 && gl.glGenVertexArrays && gl.glBindVertexArray && gl.glDeleteVertexArrays &&
     gl.glGenFramebuffers && gl.glBindFramebuffer && gl.glDeleteFramebuffers)
  {
    // gen & create an FBO and VAO
    GLuint fbo = 0;
    GLuint vao = 0;
    gl.glGenFramebuffers(1, &fbo);
    gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, fbo);
    gl.glGenVertexArrays(1, &vao);
    gl.glBindVertexArray(vao);

    // make a context that shares with the current one, and switch to it
    GLESWindowingData child = MakeContext(context);

    if(child.ctx)
    {
      // switch to child
      MakeContextCurrent(child);

      // these shouldn't be visible
      VendorCheck[VendorCheck_EXT_fbo_shared] = (gl.glIsFramebuffer(fbo) != GL_FALSE);
      VendorCheck[VendorCheck_EXT_vao_shared] = (gl.glIsVertexArray(vao) != GL_FALSE);

      if(VendorCheck[VendorCheck_EXT_fbo_shared])
        RDCWARN("FBOs are shared on this implementation");
      if(VendorCheck[VendorCheck_EXT_vao_shared])
        RDCWARN("VAOs are shared on this implementation");

      // switch back to context
      MakeContextCurrent(context);

      DeleteContext(child);
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
}

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
    case eGL_SHADER_STORAGE_BUFFER: return 9;
    case eGL_TEXTURE_BUFFER: return 10;
    case eGL_TRANSFORM_FEEDBACK_BUFFER: return 11;
    case eGL_UNIFORM_BUFFER: return 12;
    default: RDCERR("Unexpected enum as buffer target: %s", ToStr::Get(buf).c_str());
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
      eGL_SHADER_STORAGE_BUFFER,
      eGL_TEXTURE_BUFFER,
      eGL_TRANSFORM_FEEDBACK_BUFFER,
      eGL_UNIFORM_BUFFER,
  };

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
}

size_t QueryIdx(GLenum query)
{
  switch(query)
  {
    case eGL_ANY_SAMPLES_PASSED: return 0;
    case eGL_ANY_SAMPLES_PASSED_CONSERVATIVE: return 1;
    case eGL_PRIMITIVES_GENERATED: return 2;
    case eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: return 3;
    case eGL_TIME_ELAPSED_EXT: return 4;
    default: RDCERR("Unexpected enum as query target: %s", ToStr::Get(query).c_str());
  }

  return 0;
}

GLenum QueryEnum(size_t idx)
{
  GLenum enums[] = {
      eGL_ANY_SAMPLES_PASSED,
      eGL_ANY_SAMPLES_PASSED_CONSERVATIVE,
      eGL_PRIMITIVES_GENERATED,
      eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
      eGL_TIME_ELAPSED_EXT,
  };

  if(idx < ARRAY_COUNT(enums))
    return enums[idx];

  return eGL_NONE;
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
    default: RDCERR("Unexpected enum as shader enum: %s", ToStr::Get(buf).c_str());
  }

  return 0;
}

string ShaderName(GLenum id)
{
  switch(id)
  {
    case eGL_VERTEX_SHADER: return "vertex";
    case eGL_TESS_CONTROL_SHADER: return "tess_control";
    case eGL_TESS_EVALUATION_SHADER: return "tess_evaluation";
    case eGL_GEOMETRY_SHADER: return "geometry";
    case eGL_FRAGMENT_SHADER: return "fragment";
    case eGL_COMPUTE_SHADER: return "compute";
    default: RDCERR("Unexpected enum as shader enum: %s", ToStr::Get(id).c_str());
  }

  return "";
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
    default: break;
  }

  static string unknown = ToStr::Get(blendenum).substr(3);    // 3 = strlen("GL_");

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
    case eGL_CLAMP_TO_BORDER: return "CLAMP_BORDER";
    default: break;
  }

  static string unknown = ToStr::Get(smpenum).substr(3);    // 3 = strlen("GL_");

  RDCERR("Unknown blend enum: %s", unknown.c_str());

  return unknown.c_str();
}

ResourceFormat MakeResourceFormat(WrappedGLES &gl, GLenum target, GLenum fmt)
{
  ResourceFormat ret;

  ret.rawType = (uint32_t)fmt;
  ret.special = false;
  ret.specialFormat = eSpecial_Unknown;
  ret.strname = ToStr::Get(fmt).substr(3);    // 3 == strlen("GL_")

  // special handling for formats that don't query neatly
  if(fmt == eGL_LUMINANCE8_EXT || fmt == eGL_ALPHA8_EXT)
  {
    ret.compByteWidth = 1;
    ret.compCount = 1;
    ret.compType = eCompType_UNorm;
    ret.srgbCorrected = false;
    return ret;
  }

  if(IsCompressedFormat(fmt))
  {
    ret.special = true;

    switch(fmt)
    {
      case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_NV: ret.compCount = 3; break;
      case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV: ret.compCount = 4; break;

      case eGL_COMPRESSED_RGBA8_ETC2_EAC:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: ret.compCount = 4; break;
      case eGL_COMPRESSED_R11_EAC:
      case eGL_COMPRESSED_SIGNED_R11_EAC: ret.compCount = 1; break;
      case eGL_COMPRESSED_RG11_EAC:
      case eGL_COMPRESSED_SIGNED_RG11_EAC: ret.compCount = 2; break;

      case eGL_COMPRESSED_RGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_ETC2: ret.compCount = 3; break;
      case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: ret.compCount = 4; break;

      default: break;
    }

    switch(fmt)
    {
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_NV:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV:
      case eGL_COMPRESSED_SRGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: ret.srgbCorrected = true; break;
      default: break;
    }

    ret.compType = eCompType_UNorm;

    switch(fmt)
    {
      case eGL_COMPRESSED_SIGNED_R11_EAC:
      case eGL_COMPRESSED_SIGNED_RG11_EAC: ret.compType = eCompType_SNorm; break;
      default: break;
    }

    switch(fmt)
    {
      // BC1
      case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      case eGL_COMPRESSED_SRGB_S3TC_DXT1_NV:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV:
        ret.specialFormat = eSpecial_BC1;
        break;
      // BC2
      case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV:
        ret.specialFormat = eSpecial_BC2;
        break;
      // BC3
      case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
      case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV:
        ret.specialFormat = eSpecial_BC3;
        break;
      // ETC2
      case eGL_COMPRESSED_RGB8_ETC2:
      case eGL_COMPRESSED_SRGB8_ETC2:
      case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        ret.specialFormat = eSpecial_ETC2;
        break;
      // EAC
      case eGL_COMPRESSED_RGBA8_ETC2_EAC:
      case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      case eGL_COMPRESSED_R11_EAC:
      case eGL_COMPRESSED_SIGNED_R11_EAC:
      case eGL_COMPRESSED_RG11_EAC:
      case eGL_COMPRESSED_SIGNED_RG11_EAC:
        ret.specialFormat = eSpecial_EAC;
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
      case eGL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: ret.specialFormat = eSpecial_ASTC; break;
      default: RDCERR("Unexpected compressed format %#x", fmt); break;
    }
    return ret;
  }

  // handle certain non compressed but special formats
  if(fmt == eGL_R11F_G11F_B10F)
  {
    ret.special = true;
    ret.specialFormat = eSpecial_R11G11B10;
    return ret;
  }

  if(fmt == eGL_RGB565)
  {
    ret.special = true;
    ret.specialFormat = eSpecial_R5G6B5;
    return ret;
  }

  if(fmt == eGL_RGB5_A1)
  {
    ret.special = true;
    ret.specialFormat = eSpecial_R5G5B5A1;
    return ret;
  }

  if(fmt == eGL_RGB9_E5)
  {
    ret.special = true;
    ret.specialFormat = eSpecial_R9G9B9E5;
    return ret;
  }

  if(fmt == eGL_RGBA4)
  {
    ret.special = true;
    ret.specialFormat = eSpecial_R4G4B4A4;
    return ret;
  }

  if(fmt == eGL_RGB10_A2 || fmt == eGL_RGB10_A2UI)
  {
    ret.special = true;
    ret.specialFormat = eSpecial_R10G10B10A2;
    ret.compType = fmt == eGL_RGB10_A2 ? eCompType_UNorm : eCompType_UInt;
    return ret;
  }

  { // TODO pantos implement the code below and remove this hack
    if (fmt == eGL_SRGB8_ALPHA8) {
      ret.compByteWidth = 1;
      ret.compCount = 4;
      ret.compType = eCompType_UInt;
      ret.srgbCorrected = true;
      return ret;
    }

    if (fmt == eGL_DEPTH32F_STENCIL8) {
      ret.compByteWidth = 1;
      ret.compCount = 4;
      ret.compType = eCompType_Depth;
      ret.specialFormat = eSpecial_D32S8;
      ret.special = true;
      return ret;
    }

    if (fmt == eGL_DEPTH_COMPONENT24) {
      ret.compByteWidth = 3;
      ret.compCount = 1;
      ret.compType = eCompType_Depth;
      return ret;
    }

    if (fmt == eGL_RGB8) {
      ret.compByteWidth = 1;
      ret.compCount = 3;
      ret.compType = eCompType_UNorm;
      return ret;
    }

    if (fmt == eGL_RGBA8) {
      ret.compByteWidth = 1;
      ret.compCount = 4;
      ret.compType = eCompType_UNorm;
      return ret;
    }

    if (fmt == eGL_SRGB8) {
      ret.compByteWidth = 1;
      ret.compCount = 3;
      ret.compType = eCompType_UInt;
      ret.srgbCorrected = true;
      return ret;
    }

  }

  RDCERR("Unhandled resource format %#x", fmt);

  ret.compByteWidth = 1;
  ret.compCount = 4;
  ret.compType = eCompType_Float;

  GLint data[8];
  GLenum *edata = (GLenum *)data;

// TODO PEPE change to corresponding GLES code
//  GLint iscol = 0, isdepth = 0, isstencil = 0;
//  gl.glGetInternalformativ(target, fmt, eGL_COLOR_COMPONENTS, sizeof(GLint), &iscol);
//  gl.glGetInternalformativ(target, fmt, eGL_DEPTH_COMPONENTS, sizeof(GLint), &isdepth);
//  gl.glGetInternalformativ(target, fmt, eGL_STENCIL_COMPONENTS, sizeof(GLint), &isstencil);
//
//  if(iscol == GL_TRUE)
//  {
//    // colour format
//
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_SIZE, sizeof(GLint), &data[0]);
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_SIZE, sizeof(GLint), &data[1]);
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_SIZE, sizeof(GLint), &data[2]);
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_SIZE, sizeof(GLint), &data[3]);
//
//    ret.compCount = 0;
//    for(int i = 0; i < 4; i++)
//      if(data[i] > 0)
//        ret.compCount++;
//
//    for(int i = ret.compCount; i < 4; i++)
//      data[i] = data[0];
//
//    if(data[0] == data[1] && data[1] == data[2] && data[2] == data[3])
//    {
//      ret.compByteWidth = (uint32_t)(data[0] / 8);
//
//      // wasn't a byte format (8, 16, 32)
//      if(ret.compByteWidth * 8 != (uint32_t)data[0])
//        ret.special = true;
//    }
//    else
//    {
//      ret.special = true;
//    }
//
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_TYPE, sizeof(GLint), &data[0]);
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_TYPE, sizeof(GLint), &data[1]);
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_TYPE, sizeof(GLint), &data[2]);
//    gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_TYPE, sizeof(GLint), &data[3]);
//
//    for(int i = ret.compCount; i < 4; i++)
//      data[i] = data[0];
//
//    if(data[0] == data[1] && data[1] == data[2] && data[2] == data[3])
//    {
//      switch(edata[0])
//      {
//        case eGL_UNSIGNED_INT: ret.compType = eCompType_UInt; break;
//        case eGL_UNSIGNED_NORMALIZED: ret.compType = eCompType_UNorm; break;
//        case eGL_SIGNED_NORMALIZED: ret.compType = eCompType_SNorm; break;
//        case eGL_FLOAT: ret.compType = eCompType_Float; break;
//        case eGL_INT: ret.compType = eCompType_SInt; break;
//        default: RDCERR("Unexpected texture type");
//      }
//    }
//    else
//    {
//      ret.special = true;
//    }
//
//    gl.glGetInternalformativ(target, fmt, eGL_COLOR_ENCODING, sizeof(GLint), &data[0]);
//    ret.srgbCorrected = (edata[0] == eGL_SRGB);
//  }
//  else if(isdepth == GL_TRUE || isstencil == GL_TRUE)
//  {
//    // depth format
//    ret.compType = eCompType_Depth;
//
//    switch(fmt)
//    {
//      case eGL_DEPTH_COMPONENT16:
//        ret.compByteWidth = 2;
//        ret.compCount = 1;
//        break;
//      case eGL_DEPTH_COMPONENT24:
//        ret.compByteWidth = 3;
//        ret.compCount = 1;
//        break;
//      case eGL_DEPTH_COMPONENT32:
//      case eGL_DEPTH_COMPONENT32F:
//        ret.compByteWidth = 4;
//        ret.compCount = 1;
//        break;
//      case eGL_DEPTH24_STENCIL8:
//        ret.specialFormat = eSpecial_D24S8;
//        ret.special = true;
//        break;
//      case eGL_DEPTH32F_STENCIL8:
//        ret.specialFormat = eSpecial_D32S8;
//        ret.special = true;
//        break;
//      case eGL_STENCIL_INDEX8:
//        ret.specialFormat = eSpecial_S8;
//        ret.special = true;
//        break;
//      default: RDCERR("Unexpected depth or stencil format %x", fmt);
//    }
//  }
//  else
//  {
//    // not colour or depth!
//    RDCERR("Unexpected texture type, not colour or depth");
//  }

  return ret;
}

GLenum MakeGLFormat(WrappedGLES &gl, ResourceFormat fmt)
{
  GLenum ret = eGL_NONE;

  if(fmt.special)
  {
    switch(fmt.specialFormat)
    {
      case eSpecial_BC1:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_S3TC_DXT1_NV
                                  : eGL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        else
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV
                                  : eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
      }
      case eSpecial_BC2:
        ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV
                                : eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
      case eSpecial_BC3:
        ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV
                                : eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;
      case eSpecial_ETC2:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_ETC2 : eGL_COMPRESSED_RGB8_ETC2;
        else
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2
                                  : eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
        break;
      }
      case eSpecial_EAC:
      {
        if(fmt.compCount == 1)
          ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_SIGNED_R11_EAC
                                                : eGL_COMPRESSED_R11_EAC;
        else if(fmt.compCount == 2)
          ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_SIGNED_RG11_EAC
                                                : eGL_COMPRESSED_RG11_EAC;
        else
          ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
                                  : eGL_COMPRESSED_RGBA8_ETC2_EAC;
        break;
      }
      case eSpecial_R10G10B10A2:
        if(fmt.compType == eCompType_UNorm)
          ret = eGL_RGB10_A2;
        else
          ret = eGL_RGB10_A2UI;
        break;
      case eSpecial_R11G11B10: ret = eGL_R11F_G11F_B10F; break;
      case eSpecial_R5G6B5: ret = eGL_RGB565; break;
      case eSpecial_R5G5B5A1: ret = eGL_RGB5_A1; break;
      case eSpecial_R9G9B9E5: ret = eGL_RGB9_E5; break;
      case eSpecial_R4G4B4A4: ret = eGL_RGBA4; break;
      case eSpecial_D24S8: ret = eGL_DEPTH24_STENCIL8; break;
      case eSpecial_D32S8: ret = eGL_DEPTH32F_STENCIL8; break;
      case eSpecial_ASTC: RDCERR("ASTC can't be decoded unambiguously"); break;
      case eSpecial_S8: ret = eGL_STENCIL_INDEX8; break;
      default: RDCERR("Unsupported special format %u", fmt.specialFormat); break;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.srgbCorrected)
    {
      ret = eGL_SRGB8_ALPHA8;
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == eCompType_Float)
        ret = eGL_RGBA32F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_RGBA32I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RGBA32UI;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == eCompType_Float)
        ret = eGL_RGBA16F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_RGBA16I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RGBA16UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_RGBA16_SNORM_EXT;
      else if(fmt.compType == eCompType_UNorm)
        ret = eGL_RGBA16_EXT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == eCompType_SInt)
        ret = eGL_RGBA8I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RGBA8UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_RGBA8_SNORM;
      else if(fmt.compType == eCompType_UNorm)
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
      if(fmt.compType == eCompType_Float)
        ret = eGL_RGB32F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_RGB32I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RGB32UI;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == eCompType_Float)
        ret = eGL_RGB16F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_RGB16I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RGB16UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_RGB16_SNORM_EXT;
      else if(fmt.compType == eCompType_UNorm)
        ret = eGL_RGB16_EXT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == eCompType_SInt)
        ret = eGL_RGB8I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RGB8UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_RGB8_SNORM;
      else if(fmt.compType == eCompType_UNorm)
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
      if(fmt.compType == eCompType_Float)
        ret = eGL_RG32F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_RG32I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RG32UI;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == eCompType_Float)
        ret = eGL_RG16F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_RG16I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RG16UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_RG16_SNORM_EXT;
      else if(fmt.compType == eCompType_UNorm)
        ret = eGL_RG16_EXT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == eCompType_SInt)
        ret = eGL_RG8I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_RG8UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_RG8_SNORM;
      else if(fmt.compType == eCompType_UNorm)
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
      if(fmt.compType == eCompType_Float)
        ret = eGL_R32F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_R32I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_R32UI;
      else if(fmt.compType == eCompType_Depth)
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
      if(fmt.compType == eCompType_Float)
        ret = eGL_R16F;
      else if(fmt.compType == eCompType_SInt)
        ret = eGL_R16I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_R16UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_R16_SNORM_EXT;
      else if(fmt.compType == eCompType_UNorm)
        ret = eGL_R16_EXT;
      else if(fmt.compType == eCompType_Depth)
        ret = eGL_DEPTH_COMPONENT16;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == eCompType_SInt)
        ret = eGL_R8I;
      else if(fmt.compType == eCompType_UInt)
        ret = eGL_R8UI;
      else if(fmt.compType == eCompType_SNorm)
        ret = eGL_R8_SNORM;
      else if(fmt.compType == eCompType_UNorm)
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

GLenum MakeGLPrimitiveTopology(PrimitiveTopology Topo)
{
  switch(Topo)
  {
    default: return eGL_NONE;
    case eTopology_PointList: return eGL_POINTS;
    case eTopology_LineStrip: return eGL_LINE_STRIP;
    case eTopology_LineLoop: return eGL_LINE_LOOP;
    case eTopology_LineList: return eGL_LINES;
    case eTopology_LineStrip_Adj: return eGL_LINE_STRIP_ADJACENCY;
    case eTopology_LineList_Adj: return eGL_LINES_ADJACENCY;
    case eTopology_TriangleStrip: return eGL_TRIANGLE_STRIP;
    case eTopology_TriangleFan: return eGL_TRIANGLE_FAN;
    case eTopology_TriangleList: return eGL_TRIANGLES;
    case eTopology_TriangleStrip_Adj: return eGL_TRIANGLE_STRIP_ADJACENCY;
    case eTopology_TriangleList_Adj: return eGL_TRIANGLES_ADJACENCY;
    case eTopology_PatchList_1CPs:
    case eTopology_PatchList_2CPs:
    case eTopology_PatchList_3CPs:
    case eTopology_PatchList_4CPs:
    case eTopology_PatchList_5CPs:
    case eTopology_PatchList_6CPs:
    case eTopology_PatchList_7CPs:
    case eTopology_PatchList_8CPs:
    case eTopology_PatchList_9CPs:
    case eTopology_PatchList_10CPs:
    case eTopology_PatchList_11CPs:
    case eTopology_PatchList_12CPs:
    case eTopology_PatchList_13CPs:
    case eTopology_PatchList_14CPs:
    case eTopology_PatchList_15CPs:
    case eTopology_PatchList_16CPs:
    case eTopology_PatchList_17CPs:
    case eTopology_PatchList_18CPs:
    case eTopology_PatchList_19CPs:
    case eTopology_PatchList_20CPs:
    case eTopology_PatchList_21CPs:
    case eTopology_PatchList_22CPs:
    case eTopology_PatchList_23CPs:
    case eTopology_PatchList_24CPs:
    case eTopology_PatchList_25CPs:
    case eTopology_PatchList_26CPs:
    case eTopology_PatchList_27CPs:
    case eTopology_PatchList_28CPs:
    case eTopology_PatchList_29CPs:
    case eTopology_PatchList_30CPs:
    case eTopology_PatchList_31CPs:
    case eTopology_PatchList_32CPs: return eGL_PATCHES;
  }
}

PrimitiveTopology MakePrimitiveTopology(const GLHookSet &gl, GLenum Topo)
{
  switch(Topo)
  {
    default: return eTopology_Unknown;
    case eGL_POINTS: return eTopology_PointList;
    case eGL_LINE_STRIP: return eTopology_LineStrip;
    case eGL_LINE_LOOP: return eTopology_LineLoop;
    case eGL_LINES: return eTopology_LineList;
    case eGL_LINE_STRIP_ADJACENCY: return eTopology_LineStrip_Adj;
    case eGL_LINES_ADJACENCY: return eTopology_LineList_Adj;
    case eGL_TRIANGLE_STRIP: return eTopology_TriangleStrip;
    case eGL_TRIANGLE_FAN: return eTopology_TriangleFan;
    case eGL_TRIANGLES: return eTopology_TriangleList;
    case eGL_TRIANGLE_STRIP_ADJACENCY: return eTopology_TriangleStrip_Adj;
    case eGL_TRIANGLES_ADJACENCY: return eTopology_TriangleList_Adj;
    case eGL_PATCHES:
    {
      GLint patchCount = 3;
      gl.glGetIntegerv(eGL_PATCH_VERTICES, &patchCount);
      return PrimitiveTopology(eTopology_PatchList_1CPs + patchCount - 1);
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
          case eGL_FLOAT: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC2: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC3: gl.glGetUniformfv(progSrc, srcLocation, fv); break;
          case eGL_FLOAT_VEC4: gl.glGetUniformfv(progSrc, srcLocation, fv); break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
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
          default: RDCERR("Unhandled uniform type '%s'", ToStr::Get(type).c_str());
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
          case eGL_FLOAT: gl.glProgramUniform1fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC2: gl.glProgramUniform2fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC3: gl.glProgramUniform3fv(progDst, newloc, 1, fv); break;
          case eGL_FLOAT_VEC4: gl.glProgramUniform4fv(progDst, newloc, 1, fv); break;

          // treat all samplers as just an int (since they just store their binding value)
          case eGL_SAMPLER_2D:
          case eGL_SAMPLER_3D:
          case eGL_SAMPLER_CUBE:
          case eGL_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_SAMPLER_2D_SHADOW:
          case eGL_SAMPLER_2D_ARRAY:
          case eGL_SAMPLER_2D_ARRAY_SHADOW:
          case eGL_SAMPLER_2D_MULTISAMPLE:
          case eGL_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_SAMPLER_CUBE_SHADOW:
          case eGL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
          case eGL_SAMPLER_BUFFER:
          case eGL_INT_SAMPLER_2D:
          case eGL_INT_SAMPLER_3D:
          case eGL_INT_SAMPLER_CUBE:
          case eGL_INT_SAMPLER_CUBE_MAP_ARRAY:
          case eGL_INT_SAMPLER_2D_ARRAY:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_INT_SAMPLER_BUFFER:
          case eGL_UNSIGNED_INT_SAMPLER_2D:
          case eGL_UNSIGNED_INT_SAMPLER_3D:
          case eGL_UNSIGNED_INT_SAMPLER_CUBE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
          case eGL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
          case eGL_UNSIGNED_INT_SAMPLER_BUFFER:
          case eGL_IMAGE_2D:
          case eGL_IMAGE_3D:
          case eGL_IMAGE_CUBE:
          case eGL_IMAGE_BUFFER:
          case eGL_IMAGE_2D_ARRAY:
          case eGL_IMAGE_CUBE_MAP_ARRAY:
          case eGL_INT_IMAGE_2D:
          case eGL_INT_IMAGE_3D:
          case eGL_INT_IMAGE_CUBE:
          case eGL_INT_IMAGE_BUFFER:
          case eGL_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_2D:
          case eGL_UNSIGNED_INT_IMAGE_3D:
          case eGL_UNSIGNED_INT_IMAGE_CUBE:
          case eGL_UNSIGNED_INT_IMAGE_BUFFER:
          case eGL_UNSIGNED_INT_IMAGE_2D_ARRAY:
          case eGL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:
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
          default: RDCERR("Unhandled uniform type '%s'", ToStr::Get(type).c_str());
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
  if(CheckConstParam(ReadSourceProgram))
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
        RDCWARN("TODO PEPE CHECK %s:%d", __FILE__ ,__LINE__);
        //gl.glShaderStorageBlockBinding(progDst, i, bind);

        GLint prevProgram = 0;
        gl.glGetIntegerv(eGL_CURRENT_PROGRAM, &prevProgram);
        gl.glUseProgram(progDst);
        gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, i, bind);
        gl.glUseProgram(prevProgram);
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
  for(int32_t i = 0; i < refl->InputSig.count; i++)
  {
    // skip built-ins
    if(refl->InputSig[i].systemValue != eAttr_None)
      continue;

    GLint idx = gl.glGetAttribLocation(progsrc, refl->InputSig[i].varName.elems);
    if(idx >= 0)
      gl.glBindAttribLocation(progdst, (GLuint)idx, refl->InputSig[i].varName.elems);
  }
}

void CopyProgramFragDataBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst,
                                 ShaderReflection *refl)
{
  // copy over fragdata bindings
  for(int32_t i = 0; i < refl->OutputSig.count; i++)
  {
    // only look at colour outputs (should be the only outputs from fs)
    if(refl->OutputSig[i].systemValue != eAttr_ColourOutput)
      continue;

    GLint idx = gl.glGetFragDataLocation(progsrc, refl->OutputSig[i].varName.elems);
    if(idx >= 0)
      gl.glBindAttribLocation(progdst, (GLuint)idx, refl->OutputSig[i].varName.elems);
  }
}

template <>
string ToStrHelper<false, WrappedGLES::UniformType>::Get(const WrappedGLES::UniformType &el)
{
  switch(el)
  {
    case WrappedGLES::UNIFORM_UNKNOWN: return "unk";

#define VEC2STR(suffix) \
  case WrappedGLES::CONCAT(VEC, suffix): return STRINGIZE(suffix);
      VEC2STR(1fv)
      VEC2STR(1iv)
      VEC2STR(1uiv)
      VEC2STR(2fv)
      VEC2STR(2iv)
      VEC2STR(2uiv)
      VEC2STR(3fv)
      VEC2STR(3iv)
      VEC2STR(3uiv)
      VEC2STR(4fv)
      VEC2STR(4iv)
      VEC2STR(4uiv)
#undef VEC2STR

#define MAT2STR(suffix) \
  case WrappedGLES::CONCAT(MAT, suffix): return STRINGIZE(suffix);
      MAT2STR(2fv)
      MAT2STR(2x3fv)
      MAT2STR(2x4fv)
      MAT2STR(3fv)
      MAT2STR(3x2fv)
      MAT2STR(3x4fv)
      MAT2STR(4fv)
      MAT2STR(4x2fv)
      MAT2STR(4x3fv)
#undef MAT2STR

    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "WrappedGLES::UniformType<%d>", el);

  return tostrBuf;
}

template <>
string ToStrHelper<false, RDCGLenum>::Get(const RDCGLenum &el)
{
#undef GLenum

  // in official/
  // grep -Eih '#define[ \t]*[A-Z_0-9]*[ \t]*0x[0-9A-F]{4,}\s*$' *.h
  //  | awk '{print $2" "$3}' | grep -v '_BIT[_ ]'
  //  | sed -e '{s# 0x0*# #g}' | awk -F"[. ]" '!a[$2]++'
  //  | sed -e '{s%\(.*\) \(.*\)%\t\tTOSTR_CASE_STRINGIZE_GLENUM(\1)%g}'
  //  | grep -v _BIT | awk '!x[$0]++'

  RDCCOMPILE_ASSERT(sizeof(RDCGLenum) == sizeof(uint32_t),
                    "Enum isn't 32bits - serialising is a problem!");

#define TOSTR_CASE_STRINGIZE_GLENUM(a) \
  case e##a: return #a;

  switch((unsigned int)el)
  {
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_ACCESS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_ALLOC)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_ATTRIBUTE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_CONFIG)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_CONTEXT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_CURRENT_SURFACE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_DISPLAY)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_MATCH)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_NATIVE_PIXMAP)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_NATIVE_WINDOW)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_PARAMETER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BAD_SURFACE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONFIG_CAVEAT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONFIG_ID)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CORE_NATIVE_ENGINE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_EXTENSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_LARGEST_PBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MAX_PBUFFER_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MAX_PBUFFER_PIXELS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MAX_PBUFFER_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NATIVE_RENDERABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NATIVE_VISUAL_ID)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NATIVE_VISUAL_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NONE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NON_CONFORMANT_CONFIG)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NOT_INITIALIZED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SAMPLE_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SLOW_CONFIG)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SUCCESS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SURFACE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TRANSPARENT_BLUE_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TRANSPARENT_GREEN_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TRANSPARENT_RED_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TRANSPARENT_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TRANSPARENT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_VENDOR)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BACK_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BIND_TO_TEXTURE_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BIND_TO_TEXTURE_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_LOST)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MIN_SWAP_INTERVAL)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MAX_SWAP_INTERVAL)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MIPMAP_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MIPMAP_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NO_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TEXTURE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TEXTURE_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TEXTURE_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TEXTURE_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TEXTURE_TARGET)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_ALPHA_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_ALPHA_FORMAT_NONPRE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_ALPHA_FORMAT_PRE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_ALPHA_MASK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BUFFER_PRESERVED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_BUFFER_DESTROYED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CLIENT_APIS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_COLORSPACE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_COLORSPACE_sRGB)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_COLORSPACE_LINEAR)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_COLOR_BUFFER_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_CLIENT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_HORIZONTAL_RESOLUTION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_LUMINANCE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_LUMINANCE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_OPENGL_ES_API)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_OPENVG_API)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_OPENVG_IMAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_PIXEL_ASPECT_RATIO)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_RENDERABLE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_RENDER_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_RGB_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SINGLE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SWAP_BEHAVIOR)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_VERTICAL_RESOLUTION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONFORMANT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_CLIENT_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MATCH_NATIVE_PIXMAP)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MULTISAMPLE_RESOLVE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MULTISAMPLE_RESOLVE_DEFAULT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_MULTISAMPLE_RESOLVE_BOX)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_OPENGL_API)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_MINOR_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_OPENGL_PROFILE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_NO_RESET_NOTIFICATION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_LOSE_CONTEXT_ON_RESET)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_OPENGL_DEBUG)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONTEXT_OPENGL_ROBUST_ACCESS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CL_EVENT_HANDLE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_CL_EVENT)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_CL_EVENT_COMPLETE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_PRIOR_COMMANDS_COMPLETE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_CONDITION)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SIGNALED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_UNSIGNALED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_TIMEOUT_EXPIRED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_CONDITION_SATISFIED)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_SYNC_FENCE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_COLORSPACE)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_RENDERBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_ZOFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		TOSTR_CASE_STRINGIZE_GLENUM(EGL_IMAGE_PRESERVED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTIPLY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCREEN_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OVERLAY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DARKEN_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LIGHTEN_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLORDODGE_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLORBURN_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HARDLIGHT_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOFTLIGHT_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DIFFERENCE_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EXCLUSION_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HSL_HUE_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HSL_SATURATION_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HSL_COLOR_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HSL_LUMINOSITY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_ADVANCED_COHERENT_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_RELEASE_BEHAVIOR_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CALLBACK_FUNCTION_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CALLBACK_USER_PARAM_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_API_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_SHADER_COMPILER_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_THIRD_PARTY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_APPLICATION_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_OTHER_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_ERROR_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_PORTABILITY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_PERFORMANCE_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_OTHER_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_MARKER_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_PUSH_GROUP_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_POP_GROUP_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_NOTIFICATION_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_GROUP_STACK_DEPTH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PIPELINE_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_LABEL_LENGTH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEBUG_MESSAGE_LENGTH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEBUG_LOGGED_MESSAGES_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_LOGGED_MESSAGES_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_HIGH_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_MEDIUM_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_LOW_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_OUTPUT_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STACK_OVERFLOW_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STACK_UNDERFLOW_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_ROBUST_ACCESS_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOSE_CONTEXT_ON_RESET_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GUILTY_CONTEXT_RESET_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INNOCENT_CONTEXT_RESET_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNKNOWN_CONTEXT_RESET_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESET_NOTIFICATION_STRATEGY_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NO_RESET_NOTIFICATION_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_LOST_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_4x4_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_5x4_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_5x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_6x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_6x6_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_8x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_8x6_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_8x8_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_10x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_10x6_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_10x8_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_10x10_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_12x10_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_12x12_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_EXTERNAL_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_EXTERNAL_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_EXTERNAL_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ETC1_RGB8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE4_RGB8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE4_RGBA8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE4_R5_G6_B5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE4_RGBA4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE4_RGB5_A1_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE8_RGB8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE8_RGBA8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE8_R5_G6_B5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE8_RGBA4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PALETTE8_RGB5_A1_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT24_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT32_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SHADER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_LINKED_VERTICES_OUT_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_LINKED_INPUT_TYPE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_LINKED_OUTPUT_TYPE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SHADER_INVOCATIONS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LAYER_PROVOKING_VERTEX_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINES_ADJACENCY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_STRIP_ADJACENCY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLES_ADJACENCY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_STRIP_ADJACENCY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_UNIFORM_BLOCKS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_INPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_OUTPUT_VERTICES_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_SHADER_INVOCATIONS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_ATOMIC_COUNTERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_IMAGE_UNIFORMS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIRST_VERTEX_CONVENTION_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LAST_VERTEX_CONVENTION_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNDEFINED_VERTEX_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVES_GENERATED_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_LAYERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_LAYERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_LAYERED_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_GEOMETRY_SHADER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_LENGTH_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_PROGRAM_BINARY_FORMATS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_FORMATS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITE_ONLY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_ACCESS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAPPED_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAP_POINTER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_24_8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH24_STENCIL8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_BOUNDING_BOX_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT16_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE4_ALPHA4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8_ALPHA8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB5_A1_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB565_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB10_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB10_A2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_SHADING_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_SAMPLE_SHADING_VALUE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX1_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_UNDEFINED_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATCHES_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATCH_VERTICES_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_OUTPUT_VERTICES_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_MODE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_SPACING_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_VERTEX_ORDER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_POINT_MODE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ISOLINES_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUADS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRACTIONAL_ODD_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRACTIONAL_EVEN_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PATCH_VERTICES_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_GEN_LEVEL_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_PATCH_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_INPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IS_PER_PATCH_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_TESS_CONTROL_SHADER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_TESS_EVALUATION_SHADER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_SHADER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_SHADER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WRAP_R_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_3D_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_3D_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_3D_TEXTURE_SIZE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_3D_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BORDER_COLOR_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_TO_BORDER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_BUFFER_SIZE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_DATA_STORE_BINDING_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_BUFFER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_OFFSET_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_SIZE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_3x3x3_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_4x3x3_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_4x4x3_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_4x4x4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_5x4x4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_5x5x4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_5x5x5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_6x5x5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_6x6x5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_ASTC_6x6x6_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HALF_FLOAT_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX8_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_MULTISAMPLE_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_MIN_LEVEL_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_NUM_LEVELS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_MIN_LAYER_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_NUM_LAYERS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMMUTABLE_LEVELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_BINDING_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_10_10_10_2_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_10_10_10_2_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VIEWPORTS_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_BOUNDS_RANGE_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_INDEX_PROVOKING_VERTEX_OES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_3DC_X_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_3DC_XY_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATC_RGB_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATC_RGBA_EXPLICIT_ALPHA_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COUNTER_TYPE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COUNTER_RANGE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERCENTAGE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_RESULT_AVAILABLE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_RESULT_SIZE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_RESULT_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z400_BINARY_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_FRAMEBUFFER_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_FRAMEBUFFER_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_FRAMEBUFFER_BINDING_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_FRAMEBUFFER_BINDING_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_SAMPLES_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SAMPLES_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_REVERSE_ROW_ORDER_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_USAGE_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CLIP_DISTANCES_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_422_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_8_8_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_8_8_REV_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_RAW_422_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_OBJECT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SERVER_WAIT_TIMEOUT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_TYPE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_CONDITION_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_STATUS_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_FLAGS_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_FENCE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_GPU_COMMANDS_COMPLETE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNALED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNALED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALREADY_SIGNALED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TIMEOUT_EXPIRED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONDITION_SATISFIED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WAIT_FAILED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BGRA_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BGRA8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_LEVEL_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_10F_11F_11F_REV_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_5_9_9_9_REV_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R11F_G11F_B10F_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB9_E5_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MALI_PROGRAM_BINARY_ARM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MALI_SHADER_BINARY_ARM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FETCH_PER_SAMPLE_ARM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER_FRAMEBUFFER_FETCH_MRT_ARM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SMAPHS30_PROGRAM_BINARY_DMP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SMAPHS_PROGRAM_BINARY_DMP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DMP_PROGRAM_BINARY_DMP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_BINARY_DMP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC1_COLOR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC1_ALPHA_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC1_COLOR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC1_ALPHA_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_ALPHA_SATURATE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCATION_INDEX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_IMMUTABLE_STORAGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_STORAGE_FLAGS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CULL_DISTANCES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_NORMALIZED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PIPELINE_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_QUERY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_RESULT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_RESULT_AVAILABLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TIME_ELAPSED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TIMESTAMP_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_DISJOINT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COLOR_ATTACHMENTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DRAW_BUFFERS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER5_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER6_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER7_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER9_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER10_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER11_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER13_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER14_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER15_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT5_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT6_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT7_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT9_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT10_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT11_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT13_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT14_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT15_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_ALPHA_TO_ONE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTIVIEW_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_BUFFER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_MULTIVIEW_BUFFERS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ANY_SAMPLES_PASSED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_CLAMP_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_PROTECTED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_MULTISAMPLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_RASTER_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_RASTERIZATION_ALLOWED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EFFECTIVE_RASTER_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16_SNORM_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16_SNORM_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16_SNORM_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB_ALPHA_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB8_ALPHA8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_SRGB_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_PROGRAM_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_SEPARABLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PIPELINE_BINDING_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER_DISCARDS_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_FAST_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_PIXEL_LOCAL_STORAGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_COMBINED_LOCAL_STORAGE_FAST_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_COMBINED_LOCAL_STORAGE_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_INSUFFICIENT_SHADER_COMBINED_LOCAL_STORAGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_MODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_FUNC_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPARE_REF_TO_TEXTURE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_SHADOW_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SPARSE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_INDEX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SPARSE_LEVELS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_VIRTUAL_PAGE_SIZES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_X_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_Y_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_Z_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPARSE_TEXTURE_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPARSE_3D_TEXTURE_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_ANISOTROPY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16_SNORM_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SR8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRG8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SRGB_DECODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DECODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SKIP_DECODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMMUTABLE_FORMAT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA16F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG32F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_2_10_10_10_REV_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_ROW_LENGTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_ROWS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_PIXELS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INCLUSIVE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EXCLUSIVE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WINDOW_RECTANGLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WINDOW_RECTANGLE_MODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_WINDOW_RECTANGLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_WINDOW_RECTANGLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GCCSO_SHADER_BINARY_FJ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_AND_DOWNSAMPLE_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_DOWNSAMPLE_SCALES_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOWNSAMPLE_SCALES_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SCALE_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_SAMPLES_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SAMPLES_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SAMPLES_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SGX_PROGRAM_BINARY_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SGX_BINARY_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CUBIC_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CUBIC_MIPMAP_NEAREST_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CUBIC_MIPMAP_LINEAR_IMG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVATIVE_RASTERIZATION_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_SINGLE_CONTEXT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_GLOBAL_CONTEXT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_WAIT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_FLUSH_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_DONOT_FLUSH_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_EVENT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DURATION_NORM_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DURATION_RAW_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_THROUGHPUT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_RAW_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_TIMESTAMP_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DATA_UINT32_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DATA_UINT64_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DATA_FLOAT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DATA_DOUBLE_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DATA_BOOL32_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_QUERY_NAME_LENGTH_MAX_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_NAME_LENGTH_MAX_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_COUNTER_DESC_LENGTH_MAX_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFQUERY_GPA_EXTENDED_COUNTERS_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_OVERLAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_PREMULTIPLIED_SRC_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLUE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONJOINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTRAST_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISJOINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_ATOP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_IN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_OUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_OVER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREEN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HARDMIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERT_OVG_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERT_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEARBURN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEARDODGE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEARLIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINUS_CLAMPED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINUS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PINLIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PLUS_CLAMPED_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PLUS_CLAMPED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PLUS_DARKER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PLUS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_ATOP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_IN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_OUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_OVER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNCORRELATED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIVIDLIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_XOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_WAIT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_NO_WAIT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BY_REGION_WAIT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BY_REGION_NO_WAIT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVATIVE_RASTERIZATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVATIVE_RASTER_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVATIVE_RASTER_MODE_POST_SNAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVATIVE_RASTER_MODE_PRE_SNAP_TRIANGLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COPY_READ_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COPY_WRITE_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_COMPONENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_COMPONENT4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_ATTACHMENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_BUFFERS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_ALL_FRAGMENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_EDGE_FRAGMENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_AUTOMATIC_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT16_NONLINEAR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALL_COMPLETED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FENCE_STATUS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FENCE_CONDITION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILL_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COVERAGE_TO_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COVERAGE_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_MODULATION_TABLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIXED_DEPTH_SAMPLES_SUPPORTED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIXED_STENCIL_SAMPLES_SUPPORTED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_MODULATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_MODULATION_TABLE_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT64_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT8_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT8_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT8_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT16_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT16_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT16_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT64_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT64_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT64_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT8_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT8_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT8_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT16_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT16_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT16_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT16_VEC2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT16_VEC3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT16_VEC4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUPERSAMPLE_SCALE_X_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUPERSAMPLE_SCALE_Y_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONFORMANT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT2x3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT2x4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT3x2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT3x4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT4x2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT4x3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FORMAT_SVG_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FORMAT_PS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STANDARD_FONT_NAME_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYSTEM_FONT_NAME_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILE_NAME_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STROKE_WIDTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_END_CAPS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_INITIAL_END_CAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_TERMINAL_END_CAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_JOIN_STYLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MITER_LIMIT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_DASH_CAPS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_INITIAL_DASH_CAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_TERMINAL_DASH_CAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_DASH_OFFSET_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_CLIENT_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FILL_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FILL_MASK_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FILL_COVER_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STROKE_COVER_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STROKE_MASK_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COUNT_UP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COUNT_DOWN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_OBJECT_BOUNDING_BOX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVEX_HULL_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BOUNDING_BOX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSLATE_X_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSLATE_Y_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSLATE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSLATE_3D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AFFINE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AFFINE_3D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_AFFINE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_AFFINE_3D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UTF8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UTF16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_COMMAND_COUNT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_COORD_COUNT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_DASH_ARRAY_COUNT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_COMPUTED_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FILL_BOUNDING_BOX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STROKE_BOUNDING_BOX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SQUARE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ROUND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGULAR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BEVEL_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MITER_REVERT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MITER_TRUNCATE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SKIP_MISSING_GLYPH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_USE_MISSING_GLYPH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_ERROR_POSITION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACCUM_ADJACENT_PAIRS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ADJACENT_PAIRS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIRST_TO_REST_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_GEN_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_GEN_COEFF_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_GEN_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STENCIL_FUNC_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STENCIL_REF_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STENCIL_VALUE_MASK_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STENCIL_DEPTH_OFFSET_FACTOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_STENCIL_DEPTH_OFFSET_UNITS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_COVER_DEPTH_FUNC_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_DASH_OFFSET_RESET_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MOVE_TO_RESETS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MOVE_TO_CONTINUES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FONT_GLYPHS_AVAILABLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FONT_TARGET_UNAVAILABLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FONT_UNAVAILABLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FONT_UNINTELLIGIBLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STANDARD_FONT_FORMAT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_PROJECTION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MODELVIEW_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MODELVIEW_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MODELVIEW_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MAX_MODELVIEW_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_TRANSPOSE_MODELVIEW_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_PROJECTION_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_PROJECTION_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MAX_PROJECTION_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_TRANSPOSE_PROJECTION_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_INPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_POINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_LINE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILL_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE8_ALPHA8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_S3TC_DXT1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ETC1_SRGB8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_LOCATION_PIXEL_GRID_WIDTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_LOCATION_PIXEL_GRID_HEIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_LOCATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAMMABLE_SAMPLE_LOCATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_SAMPLE_LOCATION_PIXEL_GRID_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_ARRAY_SHADOW_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE_SHADOW_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_POSITIVE_X_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_NEGATIVE_X_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_POSITIVE_Y_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_NEGATIVE_Y_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_POSITIVE_Z_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_NEGATIVE_Z_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_POSITIVE_W_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_NEGATIVE_W_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_X_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_Y_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_Z_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_SWIZZLE_W_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VIEWS_OVR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_TEST_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_TEST_FUNC_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_TEST_REF_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BINNING_CONTROL_HINT_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CPU_OPTIMIZED_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_OPTIMIZED_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WIDTH_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_HEIGHT_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_INTERNAL_FORMAT_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FORMAT_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_TYPE_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMAGE_VALID_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_NUM_LEVELS_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_TARGET_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_OBJECT_VALID_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATE_RESTORE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_GLOBAL_MODE_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITEONLY_RENDERING_QCOM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_BINARY_VIV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_STRIP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_STRIP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_FAN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_DST_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_DST_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FUNC_ADD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_EQUATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_EQUATION_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FUNC_SUBTRACT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FUNC_REVERSE_SUBTRACT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_DST_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_SRC_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_DST_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_SRC_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_CONSTANT_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_CONSTANT_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_USAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_VERTEX_ATTRIB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_AND_BACK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_FACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DITHER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCISSOR_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_FILL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_ALPHA_TO_COVERAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_COVERAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_ENUM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_OPERATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUT_OF_MEMORY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CCW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALIASED_POINT_SIZE_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALIASED_LINE_WIDTH_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_FACE_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_FACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_CLEAR_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_CLEAR_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_PASS_DEPTH_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_PASS_DEPTH_PASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_REF)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_VALUE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_PASS_DEPTH_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_PASS_DEPTH_PASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_REF)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_VALUE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCISSOR_BOX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_CLEAR_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VIEWPORT_DIMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_FACTOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_COVERAGE_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_COVERAGE_INVERT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_COMPRESSED_TEXTURE_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_TEXTURE_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DONT_CARE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FASTEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NICEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GENERATE_MIPMAP_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BYTE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_BYTE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIXED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_4_4_4_4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_5_5_5_1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_5_6_5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIBS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNIFORM_VECTORS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VARYING_VECTORS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_UNIFORM_VECTORS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DELETE_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINK_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VALIDATE_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATTACHED_SHADERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_UNIFORM_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_ATTRIBUTES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADING_LANGUAGE_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_PROGRAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LESS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LEQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREATER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NOTEQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALWAYS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_KEEP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INCR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DECR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INCR_WRAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DECR_WRAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VENDOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EXTENSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEAREST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEAREST_MIPMAP_NEAREST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_MIPMAP_NEAREST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEAREST_MIPMAP_LINEAR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_MIPMAP_LINEAR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAG_FILTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MIN_FILTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WRAP_S)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WRAP_T)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_CUBE_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_POSITIVE_X)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_NEGATIVE_X)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_POSITIVE_Y)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CUBE_MAP_TEXTURE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE0)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE6)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE7)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE9)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE10)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE11)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE12)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE13)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE14)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE15)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE17)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE18)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE19)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE20)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE21)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE22)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE23)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE24)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE25)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE26)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE27)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE28)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE29)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE30)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE31)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPEAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_TO_EDGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIRRORED_REPEAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_VEC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_VEC3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_VEC4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_VEC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_VEC3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_VEC4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BOOL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BOOL_VEC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BOOL_VEC3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BOOL_VEC4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_ENABLED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_POINTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMPLEMENTATION_COLOR_READ_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMPLEMENTATION_COLOR_READ_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPILE_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INFO_LOG_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_SOURCE_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_COMPILER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_BINARY_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SHADER_BINARY_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOW_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MEDIUM_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HIGH_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOW_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MEDIUM_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HIGH_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_INTERNAL_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_COMPLETE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_UNSUPPORTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_RENDERBUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_FRAMEBUFFER_OPERATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_ROW_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SKIP_ROWS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SKIP_PIXELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_IMAGES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_IMAGE_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ELEMENTS_VERTICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ELEMENTS_INDICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MIN_LOD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_LOD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BASE_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_LOD_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_PACK_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_UNPACK_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_PACK_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_UNPACK_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAJOR_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINOR_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_EXTENSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ARRAY_TEXTURE_LAYERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_PROGRAM_TEXEL_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEXEL_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VARYING_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_VARYINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTERIZER_DISCARD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERLEAVED_ATTRIBS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SEPARATE_ATTRIBS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA32UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB32UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA8UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB8UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA32I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB32I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA8I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB8I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RED_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_VEC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_VEC3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_VEC4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_ACCESS_FLAGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAP_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAP_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT32F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH32F_STENCIL8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT17)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT18)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT19)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT20)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT21)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT22)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT23)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT24)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT25)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT26)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT27)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT28)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT29)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT30)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT31)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HALF_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R8I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R8UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R32I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R32UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG8I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG8UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG32I)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG32UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_NORMALIZED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_FIXED_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_UNIFORM_BUFFER_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_UNIFORM_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_NAME_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_MATRIX_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_IS_ROW_MAJOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_DATA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_NAME_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_INPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB10_A2UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_R)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_G)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_B)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_A)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_2_10_10_10_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_PAUSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_ACTIVE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_RETRIEVABLE_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_R11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_R11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RG11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_RG11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB8_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA8_ETC2_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ELEMENT_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SAMPLE_COUNTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_WORK_GROUP_COUNT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_WORK_GROUP_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_WORK_GROUP_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISPATCH_INDIRECT_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISPATCH_INDIRECT_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_UNIFORM_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_INPUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_OUTPUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_VARIABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BLOCK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_VARYING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_RESOURCES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_NAME_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_NUM_ACTIVE_VARIABLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NAME_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLOCK_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IS_ROW_MAJOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_DATA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_ACTIVE_VARIABLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_VARIABLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_VERTEX_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_COMPUTE_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TOP_LEVEL_ARRAY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TOP_LEVEL_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_ATOMIC_COUNTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_NAME)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_LAYERED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_LAYER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_ACCESS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_FORMAT_COMPATIBILITY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_FORMAT_COMPATIBILITY_BY_CLASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_ONLY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_WRITE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_STORAGE_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_TEXTURE_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_MASK_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SAMPLE_MASK_WORDS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COLOR_TEXTURE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEPTH_TEXTURE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_INTEGER_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_INTERNAL_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SHARED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RED_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GREEN_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BLUE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_ALPHA_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_RELATIVE_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_DIVISOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIB_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIB_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_LINE_WIDTH_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_FLAGS)
		TOSTR_CASE_STRINGIZE_GLENUM(KHRONOS_MAX_ENUM)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_RELEASE_BEHAVIOR_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_MAJOR_VERSION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_MINOR_VERSION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_LAYER_PLANE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_FLAGS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_INVALID_VERSION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_PROFILE_MASK_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_INVALID_PROFILE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_INVALID_PIXEL_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_INCOMPATIBLE_DEVICE_CONTEXTS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SAMPLE_BUFFERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SAMPLES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DRAW_TO_PBUFFER_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_MAX_PBUFFER_PIXELS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_MAX_PBUFFER_WIDTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_MAX_PBUFFER_HEIGHT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_PBUFFER_LARGEST_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_PBUFFER_WIDTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_PBUFFER_HEIGHT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_PBUFFER_LOST_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DRAW_TO_WINDOW_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_ACCELERATION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NEED_PALETTE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NEED_SYSTEM_PALETTE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SWAP_LAYER_BUFFERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SWAP_METHOD_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NUMBER_OVERLAYS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NUMBER_UNDERLAYS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_RED_VALUE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_GREEN_VALUE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_BLUE_VALUE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_ALPHA_VALUE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_INDEX_VALUE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SHARE_DEPTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SHARE_STENCIL_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SHARE_ACCUM_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SUPPORT_GDI_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SUPPORT_OPENGL_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DOUBLE_BUFFER_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_STEREO_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_PIXEL_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_RED_SHIFT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GREEN_SHIFT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BLUE_SHIFT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_ALPHA_SHIFT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX_BUFFERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NO_ACCELERATION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENERIC_ACCELERATION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_FULL_ACCELERATION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SWAP_EXCHANGE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SWAP_COPY_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SWAP_UNDEFINED_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TYPE_RGBA_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TYPE_COLORINDEX_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TYPE_RGBA_FLOAT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RGB_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RGBA_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_FORMAT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_TARGET_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_MIPMAP_TEXTURE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_RGB_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_RGBA_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NO_TEXTURE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_1D_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_2D_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_MIPMAP_LEVEL_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CUBE_MAP_FACE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_FRONT_LEFT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_FRONT_RIGHT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BACK_LEFT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BACK_RIGHT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX0_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX1_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX2_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX3_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX4_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX5_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX6_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX7_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX8_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_AUX9_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SAMPLE_BUFFERS_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_SAMPLES_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_STEREO_EMITTER_ENABLE_3DL)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_STEREO_EMITTER_DISABLE_3DL)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_STEREO_POLARITY_NORMAL_3DL)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_STEREO_POLARITY_INVERT_3DL)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_FASTEST_TARGET_GPUS_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_RAM_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_CLOCK_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_NUM_PIPES_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_NUM_SIMD_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_NUM_RB_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GPU_NUM_SPI_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DEPTH_FLOAT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_OPTIMAL_PBUFFER_WIDTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_OPTIMAL_PBUFFER_HEIGHT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TRANSPARENT_VALUE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DIGITAL_VIDEO_CURSOR_ALPHA_FRAMEBUFFER_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DIGITAL_VIDEO_CURSOR_ALPHA_VALUE_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DIGITAL_VIDEO_CURSOR_INCLUDED_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DIGITAL_VIDEO_GAMMA_CORRECTED_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GAMMA_TABLE_SIZE_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GAMMA_EXCLUDE_DESKTOP_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_MULTIVIEW_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_EXTERNAL_SYNC_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_EXTERNAL_FIELD_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_EXTERNAL_TTL_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_DIGITAL_SYNC_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_DIGITAL_FIELD_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_EDGE_FALLING_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_EDGE_RISING_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_GENLOCK_SOURCE_EDGE_BOTH_I3D)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_FLOAT_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_R_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RG_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_FLOAT_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_FLOAT_R_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_FLOAT_RG_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_FLOAT_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_FLOAT_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_INCOMPATIBLE_AFFINITY_MASKS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_MISSING_AFFINITY_MASK_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_COLOR_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NUM_VIDEO_SLOTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DEPTH_TEXTURE_FORMAT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_DEPTH_COMPONENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_DEPTH_COMPONENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_TEXTURE_RECTANGLE_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_TEXTURE_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_UNIQUE_ID_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NUM_VIDEO_CAPTURE_SLOTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_VIDEO_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_VIDEO_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_BIND_TO_VIDEO_RGB_AND_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_COLOR_AND_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_COLOR_AND_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_FRAME)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_FIELD_1)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_FIELD_2)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_STACKED_FIELDS_1_2)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_VIDEO_OUT_STACKED_FIELDS_2_1)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "GLenum<%x>", (uint32_t)el);

  return tostrBuf;

#define GLenum RDCGLenum
}
