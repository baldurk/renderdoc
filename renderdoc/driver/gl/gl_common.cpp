/******************************************************************************
 * The MIT License (MIT)
 * 
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


#include "core/core.h"
#include "serialise/string_utils.h"
#include "gl_common.h"
#include "gl_driver.h"

namespace TrackedResource
{
	static volatile int64_t globalIDCounter = 0;

	ResourceId GetNewUniqueID()
	{
		return ResourceId(Atomic::Inc64(&globalIDCounter), true);
	}

	void SetReplayResourceIDs()
	{
		globalIDCounter = RDCMAX(uint64_t(globalIDCounter), uint64_t(globalIDCounter|0x1000000000000000ULL));
	}
};

bool ExtensionSupported[ExtensionSupported_Count];
bool VendorCheck[VendorCheck_Count];

int GLCoreVersion = 0;
bool GLIsCore = false;

// simple wrapper for OS functions to make/delete a context
GLWindowingData MakeContext(GLWindowingData share);
void DeleteContext(GLWindowingData context);

void MakeContextCurrent(GLWindowingData data);

void DoVendorChecks(const GLHookSet &gl, GLWindowingData context)
{
	GLint numExts = 0;
	if(gl.glGetIntegerv) gl.glGetIntegerv(eGL_NUM_EXTENSIONS, &numExts);

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
		for(int i=0; i < numExts; i++)
		{
			const char *ext = (const char *)gl.glGetStringi(eGL_EXTENSIONS, (GLuint)i);

			if(ext == NULL || !ext[0] || !ext[1] || !ext[2] || !ext[3]) continue;

			ext += 3;

#define EXT_CHECK(extname) if(!strcmp(ext, STRINGIZE(extname))) ExtensionSupported[CONCAT(ExtensionSupported_, extname)] = true;

			EXT_CHECK(ARB_clip_control);
			EXT_CHECK(ARB_enhanced_layouts);
			EXT_CHECK(EXT_polygon_offset_clamp);
			EXT_CHECK(KHR_blend_equation_advanced_coherent);
			EXT_CHECK(EXT_raster_multisample);
			EXT_CHECK(ARB_indirect_parameters);

#undef EXT_CHECK
		}
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
	// i'th index is bound to (as above, vbslot) for GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, it returns the i'th
	// vertex buffer which is exactly what we wanted from GL_VERTEX_BINDING_BUFFER!
	// see: http://devgurus.amd.com/message/1306745#1306745
	
	if(gl.glGetError && gl.glGetIntegeri_v)
	{
		// clear all error flags.
		GLenum err = gl.glGetError();
		while(err != eGL_NONE) err = gl.glGetError();

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
	
	if(gl.glGetIntegerv && gl.glGenTextures && gl.glBindTexture &&
	   gl.glTextureStorage2DEXT && gl.glGetTextureLevelParameterivEXT && gl.glDeleteTextures)
	{
		// We need to determine if GL_TEXTURE_COMPRESSED_IMAGE_SIZE for a compressed cubemap face target
		// will return the size of the whole cubemap, or just one face. Since we fetch the cubemap
		// data face-by-face the distinction is important.
		// So we create a 4x4 cubemap with no mips that's DXT1 (BC1) compressed, which is 0.5 bytes per pixel.
		// So 4*4*0.5 = 8 bytes per face. If the returned size is 8 or 48 we can determine which result the
		// query returns. It's probably safe to assume it's consistent then for all sizes and formats of
		// cubemaps.
		// I'm not sure what the correct answer is, intuitively it feels like when you query for the size of
		// a single face target, it should give you the size of that face. The spec doesn't seem to say
		// though

		GLuint prevtex = 0; // should almost certainly be 0, but let's be careful anyway.
		gl.glGetIntegerv(eGL_TEXTURE_BINDING_CUBE_MAP, (GLint *)&prevtex);
				
		GLuint dummy = 0;
		gl.glGenTextures(1, &dummy);
		gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, dummy);
		
		gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, prevtex);

		gl.glTextureStorage2DEXT(dummy, eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 4, 4);
		
		GLint compSize = 0;
		gl.glGetTextureLevelParameterivEXT(dummy, eGL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, eGL_TEXTURE_COMPRESSED_IMAGE_SIZE, &compSize);

		if(compSize == 8)
		{
			VendorCheck[VendorCheck_EXT_compressed_cube_size] = false;
		}
		else if(compSize == 48)
		{
			VendorCheck[VendorCheck_EXT_compressed_cube_size] = true;
			RDCWARN("Compressed cubemap size returns whole cubemap");
		}
		else
		{
			RDCERR("Unexpected compressed size of +X face of BC1 compressed 4x4 cubemap mip 0! %d", compSize);
		}

		gl.glDeleteTextures(1, &dummy);
	}

	if(gl.glGetIntegerv && gl.glGetError)
	{
		// clear all error flags.
		GLenum err = gl.glGetError();
		while(err != eGL_NONE) err = gl.glGetError();

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

	// AMD throws an error if we try to copy the mips that are smaller than 4x4,
	if(gl.glGetError && gl.glGenTextures && gl.glBindTexture && gl.glCopyImageSubData &&
	   gl.glTexStorage2D && gl.glTexSubImage2D && gl.glTexParameteri && gl.glDeleteTextures)
	{
		GLuint texs[2];
		gl.glGenTextures(2, texs);

		gl.glBindTexture(eGL_TEXTURE_2D, texs[0]);
		gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1, 1);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 1);

		gl.glBindTexture(eGL_TEXTURE_2D, texs[1]);
		gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 1, 1);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAX_LEVEL, 1);
		
		// clear all error flags.
		GLenum err = gl.glGetError();
		while(err != eGL_NONE) err = gl.glGetError();

		gl.glCopyImageSubData(texs[0], eGL_TEXTURE_2D, 0,  0, 0, 0, texs[1], eGL_TEXTURE_2D, 0,  0, 0, 0,  1, 1, 1);
		
		err = gl.glGetError();

		if(err != eGL_NONE)
		{
			// if we got an error trying to query that, we should enable this hack
			VendorCheck[VendorCheck_AMD_copy_compressed_tinymips] = true;

			RDCWARN("Using hack to avoid glCopyImageSubData on lowest mips of compressed texture");
		}

		gl.glBindTexture(eGL_TEXTURE_2D, 0);
		gl.glDeleteTextures(2, texs);

		while(gl.glGetError());

		//////////////////////////////////////////////////////////////////////////
		// Check copying cubemaps

		gl.glGenTextures(2, texs);

		const size_t dim = 32;

		char buf[dim*dim/2];

		gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[0]);
		gl.glTexStorage2D(eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim, dim);
		gl.glTexParameteri(eGL_TEXTURE_CUBE_MAP, eGL_TEXTURE_MAX_LEVEL, 1);
		
		for(int i=0; i < 6; i++)
		{
			memset(buf, 0xba + i, sizeof(buf));
			gl.glCompressedTexSubImage2D(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X+i), 0, 0, 0, dim, dim, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim*dim/2, buf);
		}

		gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[1]);
		gl.glTexStorage2D(eGL_TEXTURE_CUBE_MAP, 1, eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT, dim, dim);
		gl.glTexParameteri(eGL_TEXTURE_CUBE_MAP, eGL_TEXTURE_MAX_LEVEL, 1);

		gl.glCopyImageSubData(texs[0], eGL_TEXTURE_CUBE_MAP, 0,  0, 0, 0, texs[1], eGL_TEXTURE_CUBE_MAP, 0,  0, 0, 0,  dim, dim, 6);

		char cmp[dim*dim/2];

		gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[0]);

		for(int i=0; i < 6; i++)
		{
			memset(buf, 0xba + i, sizeof(buf));
			RDCEraseEl(cmp);
			gl.glGetCompressedTexImage(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X+i), 0, cmp);

			RDCCOMPILE_ASSERT(sizeof(buf) == sizeof(buf), "Buffers are not matching sizes");

			if(memcmp(buf, cmp, sizeof(buf)))
			{
				RDCERR("glGetTexImage from the source texture returns incorrect data!");
				VendorCheck[VendorCheck_AMD_copy_compressed_cubemaps] = true; // to be safe, enable the hack
			}
		}

		gl.glBindTexture(eGL_TEXTURE_CUBE_MAP, texs[1]);

		for(int i=0; i < 6; i++)
		{
			memset(buf, 0xba + i, sizeof(buf));
			RDCEraseEl(cmp);
			gl.glGetCompressedTexImage(GLenum(eGL_TEXTURE_CUBE_MAP_POSITIVE_X+i), 0, cmp);

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

		while(gl.glGetError());
	}

	if(gl.glGetError && gl.glGenProgramPipelines && gl.glDeleteProgramPipelines && gl.glGetProgramPipelineiv)
	{
		GLuint pipe = 0;
		gl.glGenProgramPipelines(1, &pipe);
		
		// clear all error flags.
		GLenum err = gl.glGetError();
		while(err != eGL_NONE) err = gl.glGetError();

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
	if(GLCoreVersion >= 32 &&
		gl.glGenVertexArrays && gl.glBindVertexArray && gl.glDeleteVertexArrays &&
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
		GLWindowingData child = MakeContext(context);

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
		case eGL_ARRAY_BUFFER:              return 0;
		case eGL_ATOMIC_COUNTER_BUFFER:     return 1;
		case eGL_COPY_READ_BUFFER:          return 2;
		case eGL_COPY_WRITE_BUFFER:         return 3;
		case eGL_DRAW_INDIRECT_BUFFER:      return 4;
		case eGL_DISPATCH_INDIRECT_BUFFER:  return 5;
		case eGL_ELEMENT_ARRAY_BUFFER:      return 6;
		case eGL_PIXEL_PACK_BUFFER:         return 7;
		case eGL_PIXEL_UNPACK_BUFFER:       return 8;
		case eGL_QUERY_BUFFER:              return 9;
		case eGL_SHADER_STORAGE_BUFFER:     return 10;
		case eGL_TEXTURE_BUFFER:            return 11;
		case eGL_TRANSFORM_FEEDBACK_BUFFER: return 12;
		case eGL_UNIFORM_BUFFER:            return 13;
		case eGL_PARAMETER_BUFFER_ARB:      return 14;
		default:
			RDCERR("Unexpected enum as buffer target: %s", ToStr::Get(buf).c_str());
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
	switch(query)
	{
		case eGL_SAMPLES_PASSED:                        return 0;
		case eGL_ANY_SAMPLES_PASSED:                    return 1;
		case eGL_ANY_SAMPLES_PASSED_CONSERVATIVE:       return 2;
		case eGL_PRIMITIVES_GENERATED:                  return 3;
		case eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: return 4;
		case eGL_TIME_ELAPSED:                          return 5;
		default:
			RDCERR("Unexpected enum as query target: %s", ToStr::Get(query).c_str());
	}

	return 0;
}

GLenum QueryEnum(size_t idx)
{
	GLenum enums[] = {
		eGL_SAMPLES_PASSED,
		eGL_ANY_SAMPLES_PASSED,
		eGL_ANY_SAMPLES_PASSED_CONSERVATIVE,
		eGL_PRIMITIVES_GENERATED,
		eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,
		eGL_TIME_ELAPSED,
	};

	if(idx < ARRAY_COUNT(enums))
		return enums[idx];

	return eGL_NONE;
}

size_t ShaderIdx(GLenum buf)
{
	switch(buf)
	{
		case eGL_VERTEX_SHADER:           return 0;
		case eGL_TESS_CONTROL_SHADER:     return 1;
		case eGL_TESS_EVALUATION_SHADER:  return 2;
		case eGL_GEOMETRY_SHADER:         return 3;
		case eGL_FRAGMENT_SHADER:         return 4;
		case eGL_COMPUTE_SHADER:          return 5;
		default:
			RDCERR("Unexpected enum as shader enum: %s", ToStr::Get(buf).c_str());
	}

	return 0;
}

GLenum ShaderBit(size_t idx)
{
	GLenum enums[] = {
		eGL_VERTEX_SHADER_BIT,
		eGL_TESS_CONTROL_SHADER_BIT,
		eGL_TESS_EVALUATION_SHADER_BIT,
		eGL_GEOMETRY_SHADER_BIT,
		eGL_FRAGMENT_SHADER_BIT,
		eGL_COMPUTE_SHADER_BIT,
	};

	if(idx < ARRAY_COUNT(enums))
		return enums[idx];

	return eGL_NONE;
}

GLenum ShaderEnum(size_t idx)
{
	GLenum enums[] = {
		eGL_VERTEX_SHADER,
		eGL_TESS_CONTROL_SHADER,
		eGL_TESS_EVALUATION_SHADER,
		eGL_GEOMETRY_SHADER,
		eGL_FRAGMENT_SHADER,
		eGL_COMPUTE_SHADER,
	};

	if(idx < ARRAY_COUNT(enums))
		return enums[idx];

	return eGL_NONE;
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
		case eGL_FUNC_ADD:                         return "ADD";
		case eGL_FUNC_SUBTRACT:                    return "SUBTRACT";
		case eGL_FUNC_REVERSE_SUBTRACT:            return "INV_SUBTRACT";
		case eGL_MIN:                              return "MIN";
		case eGL_MAX:                              return "MAX";
		case  GL_ZERO:                             return "ZERO";
		case  GL_ONE:                              return "ONE";
		case eGL_SRC_COLOR:                        return "SRC_COLOR";
		case eGL_ONE_MINUS_SRC_COLOR:              return "INV_SRC_COLOR";
		case eGL_DST_COLOR:                        return "DST_COLOR";
		case eGL_ONE_MINUS_DST_COLOR:              return "INV_DST_COLOR";
		case eGL_SRC_ALPHA:                        return "SRC_ALPHA";
		case eGL_ONE_MINUS_SRC_ALPHA:              return "INV_SRC_ALPHA";
		case eGL_DST_ALPHA:                        return "DST_ALPHA";
		case eGL_ONE_MINUS_DST_ALPHA:              return "INV_DST_ALPHA";
		case eGL_CONSTANT_COLOR:                   return "CONST_COLOR";
		case eGL_ONE_MINUS_CONSTANT_COLOR:         return "INV_CONST_COLOR";
		case eGL_CONSTANT_ALPHA:                   return "CONST_ALPHA";
		case eGL_ONE_MINUS_CONSTANT_ALPHA:         return "INV_CONST_ALPHA";
		case eGL_SRC_ALPHA_SATURATE:               return "SRC_ALPHA_SAT";
		case eGL_SRC1_COLOR:                       return "SRC1_COL";
		case eGL_ONE_MINUS_SRC1_COLOR:             return "INV_SRC1_COL";
		case eGL_SRC1_ALPHA:                       return "SRC1_ALPHA";
		case eGL_ONE_MINUS_SRC1_ALPHA:             return "INV_SRC1_ALPHA";
		default:
			break;
	}

	static string unknown = ToStr::Get(blendenum).substr(3); // 3 = strlen("GL_");
	
	RDCERR("Unknown blend enum: %s", unknown.c_str());

	return unknown.c_str();
}

const char *SamplerString(GLenum smpenum)
{
	switch(smpenum)
	{
		case eGL_NONE:                      return "NONE";
		case eGL_NEAREST:                   return "NEAREST";
		case eGL_LINEAR:                    return "LINEAR";
		case eGL_NEAREST_MIPMAP_NEAREST:    return "NEAREST_MIP_NEAREST";
		case eGL_LINEAR_MIPMAP_NEAREST:     return "LINEAR_MIP_NEAREST";
		case eGL_NEAREST_MIPMAP_LINEAR:     return "NEAREST_MIP_LINEAR";
		case eGL_LINEAR_MIPMAP_LINEAR:      return "LINEAR_MIP_LINEAR";
		case eGL_CLAMP_TO_EDGE:             return "CLAMP_EDGE";
		case eGL_MIRRORED_REPEAT:           return "MIRR_REPEAT";
		case eGL_REPEAT:                    return "REPEAT";
		case eGL_MIRROR_CLAMP_TO_EDGE:      return "MIRR_CLAMP_EDGE";
		case eGL_CLAMP_TO_BORDER:           return "CLAMP_BORDER";
		default:
			break;
	}

	static string unknown = ToStr::Get(smpenum).substr(3); // 3 = strlen("GL_");
	
	RDCERR("Unknown blend enum: %s", unknown.c_str());

	return unknown.c_str();
}

ResourceFormat MakeResourceFormat(WrappedOpenGL &gl, GLenum target, GLenum fmt)
{
	ResourceFormat ret;

	ret.rawType = (uint32_t)fmt;
	ret.special = false;
	ret.specialFormat = eSpecial_Unknown;
	ret.strname = ToStr::Get(fmt).substr(3); // 3 == strlen("GL_")

	// special handling for formats that don't query neatly
	if(fmt == eGL_LUMINANCE8_EXT || fmt == eGL_INTENSITY8_EXT || fmt == eGL_ALPHA8_EXT)
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
			case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
				ret.compCount = 3;
				break;
			case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
				ret.compCount = 4;
				break;

			case eGL_COMPRESSED_RGBA8_ETC2_EAC:
			case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
				ret.compCount = 4;
				break;
			case eGL_COMPRESSED_R11_EAC:
			case eGL_COMPRESSED_SIGNED_R11_EAC:
				ret.compCount = 1;
				break;
			case eGL_COMPRESSED_RG11_EAC:
			case eGL_COMPRESSED_SIGNED_RG11_EAC:
				ret.compCount = 2;
				break;
				
			case eGL_COMPRESSED_RGB8_ETC2:
			case eGL_COMPRESSED_SRGB8_ETC2:
				ret.compCount = 3;
				break;
			case eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
			case eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
				ret.compCount = 4;
				break;

			default:
				break;
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
			case eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
				ret.srgbCorrected = true;
				break;
			default:
				break;
		}
		
		ret.compType = eCompType_UNorm;
		
		switch(fmt)
		{
			case eGL_COMPRESSED_SIGNED_RED_RGTC1:
			case eGL_COMPRESSED_SIGNED_RG_RGTC2:
			case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
			case eGL_COMPRESSED_SIGNED_R11_EAC:
			case eGL_COMPRESSED_SIGNED_RG11_EAC:
				ret.compType = eCompType_SNorm;
				break;
			default:
				break;
		}

		switch(fmt)
		{
			// BC1
			case eGL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			case eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			case eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
			case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
				ret.specialFormat = eSpecial_BC1;
				break;
			// BC2
			case eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
				ret.specialFormat = eSpecial_BC2;
				break;
			// BC3
			case eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			case eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
				ret.specialFormat = eSpecial_BC3;
				break;
			// BC4
			case eGL_COMPRESSED_RED_RGTC1:
			case eGL_COMPRESSED_SIGNED_RED_RGTC1:
				ret.specialFormat = eSpecial_BC4;
				break;
			// BC5
			case eGL_COMPRESSED_RG_RGTC2:
			case eGL_COMPRESSED_SIGNED_RG_RGTC2:
				ret.specialFormat = eSpecial_BC5;
				break;
			// BC6
			case eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:
			case eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:
				ret.specialFormat = eSpecial_BC6;
				break;
			// BC7
			case eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
			case eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
				ret.specialFormat = eSpecial_BC7;
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
			default:
				RDCERR("Unexpected compressed format %#x", fmt);
				break;
		}
		return ret;
	}

	ret.compByteWidth = 1;
	ret.compCount = 4;
	ret.compType = eCompType_Float;
	
	GLint data[8];
	GLenum *edata = (GLenum *)data;

	GLint iscol = 0, isdepth = 0, isstencil = 0;
	gl.glGetInternalformativ(target, fmt, eGL_COLOR_COMPONENTS, sizeof(GLint), &iscol);
	gl.glGetInternalformativ(target, fmt, eGL_DEPTH_COMPONENTS, sizeof(GLint), &isdepth);
	gl.glGetInternalformativ(target, fmt, eGL_STENCIL_COMPONENTS, sizeof(GLint), &isstencil);

	if(iscol == GL_TRUE)
	{
		// colour format

		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_SIZE, sizeof(GLint), &data[0]);
		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_SIZE, sizeof(GLint), &data[1]);
		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_SIZE, sizeof(GLint), &data[2]);
		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_SIZE, sizeof(GLint), &data[3]);

		ret.compCount = 0;
		for(int i=0; i < 4; i++)
			if(data[i] > 0)
				ret.compCount++;

		for(int i=ret.compCount; i < 4; i++)
			data[i] = data[0];

		if(data[0] == data[1] &&
			 data[1] == data[2] &&
			 data[2] == data[3])
		{
			ret.compByteWidth = (uint32_t)(data[0]/8);

			// wasn't a byte format (8, 16, 32)
			if(ret.compByteWidth*8 != (uint32_t)data[0])
				ret.special = true;
		}
		else
		{
			ret.special = true;
		}

		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_RED_TYPE, sizeof(GLint), &data[0]);
		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_GREEN_TYPE, sizeof(GLint), &data[1]);
		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_BLUE_TYPE, sizeof(GLint), &data[2]);
		gl.glGetInternalformativ(target, fmt, eGL_INTERNALFORMAT_ALPHA_TYPE, sizeof(GLint), &data[3]);
		
		for(int i=ret.compCount; i < 4; i++)
			data[i] = data[0];
		
		if(data[0] == data[1] &&
			 data[1] == data[2] &&
			 data[2] == data[3])
		{
			switch(edata[0])
			{
				case eGL_UNSIGNED_INT:
					ret.compType = eCompType_UInt;
					break;
				case eGL_UNSIGNED_NORMALIZED:
					ret.compType = eCompType_UNorm;
					break;
				case eGL_SIGNED_NORMALIZED:
					ret.compType = eCompType_SNorm;
					break;
				case eGL_FLOAT:
					ret.compType = eCompType_Float;
					break;
				case eGL_INT:
					ret.compType = eCompType_SInt;
					break;
				default:
					RDCERR("Unexpected texture type");
			}
		}
		else
		{
			ret.special = true;
		}

		gl.glGetInternalformativ(target, fmt, eGL_COLOR_ENCODING, sizeof(GLint), &data[0]);
		ret.srgbCorrected = (edata[0] == eGL_SRGB);
	}
	else if(isdepth == GL_TRUE || isstencil == GL_TRUE)
	{
		// depth format
		ret.compType = eCompType_Depth;

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
			case eGL_DEPTH24_STENCIL8:
				ret.specialFormat = eSpecial_D24S8;
				ret.special = true;
				break;
			case eGL_DEPTH32F_STENCIL8:
				ret.specialFormat = eSpecial_D32S8;
				ret.special = true;
				break;
			default:
				RDCERR("Unexpected depth or stencil format %x", fmt);
		}
	}
	else
	{
		// not colour or depth!
		RDCERR("Unexpected texture type, not colour or depth");
	}

	return ret;
}

GLenum MakeGLFormat(WrappedOpenGL &gl, ResourceFormat fmt)
{
	GLenum ret = eGL_NONE;

	if(fmt.special)
	{
		switch(fmt.specialFormat)
		{
			case eSpecial_BC1:
			{
				if(fmt.compCount == 3)
					ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_S3TC_DXT1_EXT : eGL_COMPRESSED_RGB_S3TC_DXT1_EXT;
				else
					ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT : eGL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				break;
			}
			case eSpecial_BC2:
				ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : eGL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				break;
			case eSpecial_BC3:
				ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : eGL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				break;
			case eSpecial_BC4:
				ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_SIGNED_RED_RGTC1 : eGL_COMPRESSED_RED_RGTC1;
				break;
			case eSpecial_BC5:
				ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_SIGNED_RG_RGTC2 : eGL_COMPRESSED_RG_RGTC2;
				break;
			case eSpecial_BC6:
				ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB : eGL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB;
				break;
			case eSpecial_BC7:
				ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB : eGL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
				break;
			case eSpecial_ETC2:
			{
				if(fmt.compCount == 3)
					ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_ETC2 : eGL_COMPRESSED_RGB8_ETC2;
				else
					ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 : eGL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
				break;
			}
			case eSpecial_EAC:
			{
				if(fmt.compCount == 1)
					ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_SIGNED_R11_EAC : eGL_COMPRESSED_R11_EAC;
				else if(fmt.compCount == 2)
					ret = fmt.compType == eCompType_SNorm ? eGL_COMPRESSED_SIGNED_RG11_EAC : eGL_COMPRESSED_RG11_EAC;
				else
					ret = fmt.srgbCorrected ? eGL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC : eGL_COMPRESSED_RGBA8_ETC2_EAC;
				break;
			}
			case eSpecial_R10G10B10A2:
				if(fmt.compType == eCompType_UNorm)
					ret = eGL_RGB10_A2;
				else
					ret = eGL_RGB10_A2UI;
				break;
			case eSpecial_R11G11B10:
				ret = eGL_R11F_G11F_B10F;
				break;
			case eSpecial_B5G6R5:
				ret = eGL_RGB565;
				break;
			case eSpecial_B5G5R5A1:
				ret = eGL_RGB5_A1;
				break;
			case eSpecial_R9G9B9E5:
				ret = eGL_RGB9_E5;
				break;
			case eSpecial_B8G8R8A8:
				ret = eGL_RGBA;
				break;
			case eSpecial_B4G4R4A4:
				ret = eGL_RGBA4;
				break;
			case eSpecial_D24S8:
				ret = eGL_DEPTH24_STENCIL8;
				break;
			case eSpecial_D32S8:
				ret = eGL_DEPTH32F_STENCIL8;
				break;
			default:
				RDCERR("Unsupported special format %u", fmt.specialFormat);
				break;
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
			     if(fmt.compType == eCompType_Float) ret = eGL_RGBA32F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_RGBA32I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RGBA32UI;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 2)
		{
			     if(fmt.compType == eCompType_Float) ret = eGL_RGBA16F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_RGBA16I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RGBA16UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_RGBA16_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_RGBA16;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 1)
		{
			     if(fmt.compType == eCompType_SInt)  ret = eGL_RGBA8I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RGBA8UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_RGBA8_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_RGBA8;
			else RDCERR("Unrecognised component type");
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
			     if(fmt.compType == eCompType_Float) ret = eGL_RGB32F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_RGB32I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RGB32UI;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 2)
		{
			     if(fmt.compType == eCompType_Float) ret = eGL_RGB16F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_RGB16I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RGB16UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_RGB16_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_RGB16;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 1)
		{
			     if(fmt.compType == eCompType_SInt)  ret = eGL_RGB8I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RGB8UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_RGB8_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_RGB8;
			else RDCERR("Unrecognised component type");
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
			     if(fmt.compType == eCompType_Float) ret = eGL_RG32F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_RG32I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RG32UI;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 2)
		{
			     if(fmt.compType == eCompType_Float) ret = eGL_RG16F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_RG16I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RG16UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_RG16_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_RG16;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 1)
		{
			     if(fmt.compType == eCompType_SInt)  ret = eGL_RG8I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_RG8UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_RG8_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_RG8;
			else RDCERR("Unrecognised component type");
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
			     if(fmt.compType == eCompType_Float) ret = eGL_R32F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_R32I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_R32UI;
			else if(fmt.compType == eCompType_Depth) ret = eGL_DEPTH_COMPONENT32F;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 3)
		{
			ret = eGL_DEPTH_COMPONENT24;
		}
		else if(fmt.compByteWidth == 2)
		{
			     if(fmt.compType == eCompType_Float) ret = eGL_R16F;
			else if(fmt.compType == eCompType_SInt)  ret = eGL_R16I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_R16UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_R16_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_R16;
			else if(fmt.compType == eCompType_Depth) ret = eGL_DEPTH_COMPONENT16;
			else RDCERR("Unrecognised component type");
		}
		else if(fmt.compByteWidth == 1)
		{
			     if(fmt.compType == eCompType_SInt)  ret = eGL_R8I;
			else if(fmt.compType == eCompType_UInt)  ret = eGL_R8UI;
			else if(fmt.compType == eCompType_SNorm) ret = eGL_R8_SNORM;
			else if(fmt.compType == eCompType_UNorm) ret = eGL_R8;
			else RDCERR("Unrecognised component type");
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
		default:                            return eGL_NONE;
		case eTopology_PointList:           return eGL_POINTS;
		case eTopology_LineStrip:           return eGL_LINE_STRIP;
		case eTopology_LineLoop:            return eGL_LINE_LOOP;
		case eTopology_LineList:            return eGL_LINES;
		case eTopology_LineStrip_Adj:       return eGL_LINE_STRIP_ADJACENCY;
		case eTopology_LineList_Adj:        return eGL_LINES_ADJACENCY;
		case eTopology_TriangleStrip:       return eGL_TRIANGLE_STRIP;
		case eTopology_TriangleFan:         return eGL_TRIANGLE_FAN;
		case eTopology_TriangleList:        return eGL_TRIANGLES;
		case eTopology_TriangleStrip_Adj:   return eGL_TRIANGLE_STRIP_ADJACENCY;
		case eTopology_TriangleList_Adj:    return eGL_TRIANGLES_ADJACENCY;
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
		case eTopology_PatchList_32CPs:
			return eGL_PATCHES;
	}
}

PrimitiveTopology MakePrimitiveTopology(const GLHookSet &gl, GLenum Topo)
{
	switch(Topo)
	{
		default:                             return eTopology_Unknown;
		case eGL_POINTS:                     return eTopology_PointList;
		case eGL_LINE_STRIP:                 return eTopology_LineStrip;
		case eGL_LINE_LOOP:                  return eTopology_LineLoop;
		case eGL_LINES:                      return eTopology_LineList;
		case eGL_LINE_STRIP_ADJACENCY:       return eTopology_LineStrip_Adj;
		case eGL_LINES_ADJACENCY:            return eTopology_LineList_Adj;
		case eGL_TRIANGLE_STRIP:             return eTopology_TriangleStrip;
		case eGL_TRIANGLE_FAN:               return eTopology_TriangleFan;
		case eGL_TRIANGLES:                  return eTopology_TriangleList;
		case eGL_TRIANGLE_STRIP_ADJACENCY:   return eTopology_TriangleStrip_Adj;
		case eGL_TRIANGLES_ADJACENCY:        return eTopology_TriangleList_Adj;
		case eGL_PATCHES:
		{
			GLint patchCount = 3;
			gl.glGetIntegerv(eGL_PATCH_VERTICES, &patchCount);
			return PrimitiveTopology(eTopology_PatchList_1CPs+patchCount-1);
		}
	}
}

template<const bool CopyUniforms, const bool SerialiseUniforms>
static void ForAllProgramUniforms(const GLHookSet &gl, Serialiser *ser, GLuint progSrc, GLuint progDst, map<GLint, GLint> *locTranslate, bool writing)
{
	const bool ReadSourceProgram = CopyUniforms || (SerialiseUniforms && writing);
	const bool WriteDestProgram = CopyUniforms || (SerialiseUniforms && !writing);

	RDCCOMPILE_ASSERT( (CopyUniforms && !SerialiseUniforms) || (!CopyUniforms && SerialiseUniforms), "Invalid call to ForAllProgramUniforms");

	GLint numUniforms = 0;
	if(ReadSourceProgram)
		gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM, eGL_ACTIVE_RESOURCES, &numUniforms);

	if(SerialiseUniforms)
	{
		// get accurate count of uniforms not in UBOs
		GLint numSerialisedUniforms = 0;

		for(GLint i=0; writing && i < numUniforms; i++)
		{
			GLenum prop = eGL_BLOCK_INDEX;
			GLint blockIdx;
			gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, 1, &prop, 1, NULL, (GLint *)&blockIdx);

			if(blockIdx >= 0) continue;

			numSerialisedUniforms++;
		}

		ser->Serialise("numUniforms", numSerialisedUniforms);

		if(!writing)
			numUniforms = numSerialisedUniforms;
	}
	
	const size_t numProps = 5;
	GLenum resProps[numProps] = { eGL_BLOCK_INDEX, eGL_TYPE, eGL_NAME_LENGTH, eGL_ARRAY_SIZE, eGL_LOCATION, };
	
	for(GLint i=0; i < numUniforms; i++)
	{
		GLenum type = eGL_NONE;
		int32_t arraySize = 0;
		int32_t srcLocation = 0;
		string basename;
		bool isArray = false;

		if(ReadSourceProgram)
		{
			GLint values[numProps];
			gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM, i, numProps, resProps, numProps, NULL, values);

			// we don't need to consider uniforms within UBOs
			if(values[0] >= 0) continue;

			type = (GLenum)values[1];
			arraySize = values[3];
			srcLocation = values[4];
		
			char n[1024] = {0};
			gl.glGetProgramResourceName(progSrc, eGL_UNIFORM, i, values[2], NULL, n);

			if(arraySize > 1)
			{
				isArray = true;

				size_t len = strlen(n);

				if(n[len-3] == '[' && n[len-2] == '0' && n[len-1] == ']')
					n[len-3] = 0;
			}
			else
			{
				arraySize = 1;
			}
			
			basename = n;
		}

		if(SerialiseUniforms)
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

		for(GLint arr=0; arr < arraySize; arr++)
		{
			string name = basename;

			if(isArray)
			{
				name += StringFormat::Fmt("[%d]", arr);

				if(ReadSourceProgram)
					srcLocation = gl.glGetUniformLocation(progDst, name.c_str());
			}
			
			if(SerialiseUniforms)
				ser->Serialise("srcLocation", srcLocation);

			GLint newloc = 0;
			if(WriteDestProgram)
			{
				newloc = gl.glGetUniformLocation(progDst, name.c_str());
				if(locTranslate) (*locTranslate)[srcLocation] = newloc;
			}

			if(CopyUniforms && newloc == -1)
				continue;

			if(ReadSourceProgram)
			{
				switch(type)
				{
					case eGL_FLOAT_MAT4:               gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT4x3:             gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT4x2:             gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT3:               gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT3x4:             gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT3x2:             gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT2:               gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT2x4:             gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_MAT2x3:             gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_DOUBLE_MAT4:              gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT4x3:            gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT4x2:            gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT3:              gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT3x4:            gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT3x2:            gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT2:              gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT2x4:            gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_MAT2x3:            gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_FLOAT:                    gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_VEC2:               gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_VEC3:               gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_FLOAT_VEC4:               gl.glGetUniformfv(progSrc, srcLocation, fv); break;
					case eGL_DOUBLE:                   gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_VEC2:              gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_VEC3:              gl.glGetUniformdv(progSrc, srcLocation, dv); break;
					case eGL_DOUBLE_VEC4:              gl.glGetUniformdv(progSrc, srcLocation, dv); break;

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
					case eGL_INT:                      gl.glGetUniformiv(progSrc, srcLocation, iv); break;
					case eGL_INT_VEC2:                 gl.glGetUniformiv(progSrc, srcLocation, iv); break;
					case eGL_INT_VEC3:                 gl.glGetUniformiv(progSrc, srcLocation, iv); break;
					case eGL_INT_VEC4:                 gl.glGetUniformiv(progSrc, srcLocation, iv); break;
					case eGL_UNSIGNED_INT:
					case eGL_BOOL:                     gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
					case eGL_UNSIGNED_INT_VEC2:
					case eGL_BOOL_VEC2:                gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
					case eGL_UNSIGNED_INT_VEC3:
					case eGL_BOOL_VEC3:                gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
					case eGL_UNSIGNED_INT_VEC4:
					case eGL_BOOL_VEC4:                gl.glGetUniformuiv(progSrc, srcLocation, uiv); break;
					default:
						RDCERR("Unhandled uniform type '%s'", ToStr::Get(type).c_str());
				}
			}

			if(SerialiseUniforms)
				ser->Serialise<16>("data", dv);

			if(WriteDestProgram)
			{
				switch(type)
				{
					case eGL_FLOAT_MAT4:               gl.glProgramUniformMatrix4fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT4x3:             gl.glProgramUniformMatrix4x3fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT4x2:             gl.glProgramUniformMatrix4x2fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT3:               gl.glProgramUniformMatrix3fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT3x4:             gl.glProgramUniformMatrix3x4fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT3x2:             gl.glProgramUniformMatrix3x2fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT2:               gl.glProgramUniformMatrix2fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT2x4:             gl.glProgramUniformMatrix2x4fv(progDst, newloc, 1, false, fv); break;
					case eGL_FLOAT_MAT2x3:             gl.glProgramUniformMatrix2x3fv(progDst, newloc, 1, false, fv); break;
					case eGL_DOUBLE_MAT4:              gl.glProgramUniformMatrix4dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT4x3:            gl.glProgramUniformMatrix4x3dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT4x2:            gl.glProgramUniformMatrix4x2dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT3:              gl.glProgramUniformMatrix3dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT3x4:            gl.glProgramUniformMatrix3x4dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT3x2:            gl.glProgramUniformMatrix3x2dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT2:              gl.glProgramUniformMatrix2dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT2x4:            gl.glProgramUniformMatrix2x4dv(progDst, newloc, 1, false, dv); break;
					case eGL_DOUBLE_MAT2x3:            gl.glProgramUniformMatrix2x3dv(progDst, newloc, 1, false, dv); break;
					case eGL_FLOAT:                    gl.glProgramUniform1fv(progDst, newloc, 1, fv); break;
					case eGL_FLOAT_VEC2:               gl.glProgramUniform2fv(progDst, newloc, 1, fv); break;
					case eGL_FLOAT_VEC3:               gl.glProgramUniform3fv(progDst, newloc, 1, fv); break;
					case eGL_FLOAT_VEC4:               gl.glProgramUniform4fv(progDst, newloc, 1, fv); break;
					case eGL_DOUBLE:                   gl.glProgramUniform1dv(progDst, newloc, 1, dv); break;
					case eGL_DOUBLE_VEC2:              gl.glProgramUniform2dv(progDst, newloc, 1, dv); break;
					case eGL_DOUBLE_VEC3:              gl.glProgramUniform3dv(progDst, newloc, 1, dv); break;
					case eGL_DOUBLE_VEC4:              gl.glProgramUniform4dv(progDst, newloc, 1, dv); break;

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
					case eGL_INT:                      gl.glProgramUniform1iv(progDst, newloc, 1, iv); break;
					case eGL_INT_VEC2:                 gl.glProgramUniform2iv(progDst, newloc, 1, iv); break;
					case eGL_INT_VEC3:                 gl.glProgramUniform3iv(progDst, newloc, 1, iv); break;
					case eGL_INT_VEC4:                 gl.glProgramUniform4iv(progDst, newloc, 1, iv); break;
					case eGL_UNSIGNED_INT:
					case eGL_BOOL:                     gl.glProgramUniform1uiv(progDst, newloc, 1, uiv); break;
					case eGL_UNSIGNED_INT_VEC2:
					case eGL_BOOL_VEC2:                gl.glProgramUniform2uiv(progDst, newloc, 1, uiv); break;
					case eGL_UNSIGNED_INT_VEC3:
					case eGL_BOOL_VEC3:                gl.glProgramUniform3uiv(progDst, newloc, 1, uiv); break;
					case eGL_UNSIGNED_INT_VEC4:
					case eGL_BOOL_VEC4:                gl.glProgramUniform4uiv(progDst, newloc, 1, uiv); break;
					default:
						RDCERR("Unhandled uniform type '%s'", ToStr::Get(type).c_str());
				}
			}
		}
	}

	GLint numUBOs = 0;
	if(ReadSourceProgram)
		gl.glGetProgramInterfaceiv(progSrc, eGL_UNIFORM_BLOCK, eGL_ACTIVE_RESOURCES, &numUBOs);

	if(SerialiseUniforms)
		ser->Serialise("numUBOs", numUBOs);
	
	for(GLint i=0; i < numUBOs; i++)
	{
		GLenum prop = eGL_BUFFER_BINDING;
		uint32_t bind = 0;
		string name;

		if(ReadSourceProgram)
		{
			gl.glGetProgramResourceiv(progSrc, eGL_UNIFORM_BLOCK, i, 1, &prop, 1, NULL, (GLint *)&bind);

			char n[1024] = {0};
			gl.glGetProgramResourceName(progSrc, eGL_UNIFORM_BLOCK, i, 1023, NULL, n);

			name = n;
		}

		if(SerialiseUniforms)
		{
			ser->Serialise("bind", bind);
			ser->Serialise("name", name);
		}

		if(WriteDestProgram)
		{
			GLuint idx = gl.glGetUniformBlockIndex(progDst, name.c_str());
			if(idx != GL_INVALID_INDEX)
				gl.glUniformBlockBinding(progDst, idx, bind);
		}
	}

	GLint numSSBOs = 0;
	if(ReadSourceProgram)
		gl.glGetProgramInterfaceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, eGL_ACTIVE_RESOURCES, &numSSBOs);
	
	if(SerialiseUniforms)
		ser->Serialise("numSSBOs", numSSBOs);
	
	for(GLint i=0; i < numSSBOs; i++)
	{
		GLenum prop = eGL_BUFFER_BINDING;
		uint32_t bind = 0;
		string name;

		if(ReadSourceProgram)
		{
			gl.glGetProgramResourceiv(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1, &prop, 1, NULL, (GLint *)&bind);

			char n[1024] = {0};
			gl.glGetProgramResourceName(progSrc, eGL_SHADER_STORAGE_BLOCK, i, 1023, NULL, n);

			name = n;
		}

		if(SerialiseUniforms)
		{
			ser->Serialise("bind", bind);
			ser->Serialise("name", name);
		}

		if(WriteDestProgram)
		{
			GLuint idx = gl.glGetProgramResourceIndex(progDst, eGL_SHADER_STORAGE_BLOCK, name.c_str());
			if(idx != GL_INVALID_INDEX)
				gl.glShaderStorageBlockBinding(progDst, i, bind);
		}
	}
}

void CopyProgramUniforms(const GLHookSet &gl, GLuint progSrc, GLuint progDst)
{
	const bool CopyUniforms = true;
	const bool SerialiseUniforms = false;
	ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(gl, NULL, progSrc, progDst, NULL, false);
}

void SerialiseProgramUniforms(const GLHookSet &gl, Serialiser *ser, GLuint prog, map<GLint, GLint> *locTranslate, bool writing)
{
	const bool CopyUniforms = false;
	const bool SerialiseUniforms = true;
	ForAllProgramUniforms<CopyUniforms, SerialiseUniforms>(gl, ser, prog, prog, locTranslate, writing);
}

void CopyProgramAttribBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst, ShaderReflection *refl)
{
	// copy over attrib bindings
	for(int32_t i=0; i < refl->InputSig.count; i++)
	{
		// skip built-ins
		if(refl->InputSig[i].systemValue != eAttr_None)
			continue;

		GLint idx = gl.glGetAttribLocation(progsrc, refl->InputSig[i].varName.elems);
		if(idx >= 0)
			gl.glBindAttribLocation(progdst, (GLuint)idx, refl->InputSig[i].varName.elems);
	}
}

void CopyProgramFragDataBindings(const GLHookSet &gl, GLuint progsrc, GLuint progdst, ShaderReflection *refl)
{
	// copy over fragdata bindings
	for(int32_t i=0; i < refl->OutputSig.count; i++)
	{
		// only look at colour outputs (should be the only outputs from fs)
		if(refl->OutputSig[i].systemValue != eAttr_ColourOutput)
			continue;

		GLint idx = gl.glGetFragDataLocation(progsrc, refl->OutputSig[i].varName.elems);
		if(idx >= 0)
			gl.glBindFragDataLocation(progdst, (GLuint)idx, refl->OutputSig[i].varName.elems);
	}
}

template<>
string ToStrHelper<false, WrappedOpenGL::UniformType>::Get(const WrappedOpenGL::UniformType &el)
{
	switch(el)
	{
		case WrappedOpenGL::UNIFORM_UNKNOWN: return "unk";

#define VEC2STR(suffix) case WrappedOpenGL::CONCAT(VEC, suffix): return STRINGIZE(suffix);
		VEC2STR(1fv)
		VEC2STR(1iv)
		VEC2STR(1uiv)
		VEC2STR(1dv)
		VEC2STR(2fv)
		VEC2STR(2iv)
		VEC2STR(2uiv)
		VEC2STR(2dv)
		VEC2STR(3fv)
		VEC2STR(3iv)
		VEC2STR(3uiv)
		VEC2STR(3dv)
		VEC2STR(4fv)
		VEC2STR(4iv)
		VEC2STR(4uiv)
		VEC2STR(4dv)
#undef VEC2STR

#define MAT2STR(suffix) case WrappedOpenGL::CONCAT(MAT, suffix): return STRINGIZE(suffix);
		MAT2STR(2fv)
		MAT2STR(2x3fv)
		MAT2STR(2x4fv)
		MAT2STR(3fv)
		MAT2STR(3x2fv)
		MAT2STR(3x4fv)
		MAT2STR(4fv)
		MAT2STR(4x2fv)
		MAT2STR(4x3fv)
		MAT2STR(2dv)
		MAT2STR(2x3dv)
		MAT2STR(2x4dv)
		MAT2STR(3dv)
		MAT2STR(3x2dv)
		MAT2STR(3x4dv)
		MAT2STR(4dv)
		MAT2STR(4x2dv)
		MAT2STR(4x3dv)
#undef MAT2STR

		default:
			break;
	}

	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "WrappedOpenGL::UniformType<%d>", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, RDCGLenum>::Get(const RDCGLenum &el)
{
#undef GLenum

// egrep -ih '#define[ \t]*[A-Z_0-9]*[ \t]*0x[0-9A-F]{4,}\s*$' glcorearb.h  glext.h  wglext.h  glxext.h
//			| awk '{print $2" "$3}' | grep -v '_BIT[_ ]' | sed -e '{s# 0x0*# #g}' | awk -F"[. ]" '!a[$2]++'
//			| sed -e '{s%\(.*\) \(.*\)%\t\tTOSTR_CASE_STRINGIZE_GLENUM(\1)%g}' | grep -v _BIT | awk ' !x[$0]++'

	RDCCOMPILE_ASSERT(sizeof(RDCGLenum) == sizeof(uint32_t), "Enum isn't 32bits - serialising is a problem!");
	
#define TOSTR_CASE_STRINGIZE_GLENUM(a) case e##a: return #a;

	switch((unsigned int)el)
	{
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NONE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_LOOP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_STRIP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_STRIP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_FAN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUADS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEVER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LESS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LEQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREATER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NOTEQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEQUAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALWAYS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_DST_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_DST_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC_ALPHA_SATURATE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_LEFT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_RIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK_LEFT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK_RIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LEFT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_AND_BACK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_ENUM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_OPERATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUT_OF_MEMORY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CCW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SIZE_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SIZE_GRANULARITY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_SMOOTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_WIDTH_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_WIDTH_GRANULARITY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_SMOOTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_FACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_FACE_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_FACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_CLEAR_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_CLEAR_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_VALUE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_PASS_DEPTH_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_PASS_DEPTH_PASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_REF)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DITHER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_DST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_SRC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOGIC_OP_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_LOGIC_OP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCISSOR_BOX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCISSOR_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_CLEAR_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLEBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STEREO)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_SMOOTH_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_SMOOTH_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SWAP_BYTES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_LSB_FIRST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_ROW_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_ROWS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_PIXELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SWAP_BYTES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_LSB_FIRST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_ROW_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SKIP_ROWS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SKIP_PIXELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VIEWPORT_DIMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_POINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_LINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_FILL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_FACTOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_INTERNAL_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BORDER_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DONT_CARE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FASTEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NICEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BYTE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_BYTE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STACK_OVERFLOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STACK_UNDERFLOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLEAR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AND_REVERSE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AND_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NOOP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_XOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EQUIV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OR_REVERSE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COPY_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OR_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NAND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREEN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_KEEP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INCR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DECR)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPEAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R3_G3_B2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB10)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB12)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB5_A1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB10_A2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA12)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_BYTE_3_3_2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_4_4_4_4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_5_5_5_1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_8_8_8_8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_10_10_10_2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SKIP_IMAGES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_IMAGE_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_IMAGES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_IMAGE_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WRAP_R)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_3D_TEXTURE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_BYTE_2_3_3_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_5_6_5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_5_6_5_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_4_4_4_4_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_1_5_5_5_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_8_8_8_8_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_2_10_10_10_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BGRA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ELEMENTS_VERTICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ELEMENTS_INDICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_TO_EDGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MIN_LOD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_LOD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BASE_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALIASED_LINE_WIDTH_RANGE)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_ALPHA_TO_COVERAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_ALPHA_TO_ONE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_COVERAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_COVERAGE_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_COVERAGE_INVERT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_CUBE_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_POSITIVE_X)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_NEGATIVE_X)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_POSITIVE_Y)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_CUBE_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CUBE_MAP_TEXTURE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSION_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSED_IMAGE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_COMPRESSED_TEXTURE_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_TEXTURE_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_TO_BORDER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_DST_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_SRC_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_DST_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_SRC_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_FADE_THRESHOLD_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT24)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT32)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIRRORED_REPEAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_LOD_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LOD_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INCR_WRAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DECR_WRAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FUNC_ADD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FUNC_SUBTRACT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FUNC_REVERSE_SUBTRACT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_CONSTANT_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_CONSTANT_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_USAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_QUERY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_RESULT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_RESULT_AVAILABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_ONLY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITE_ONLY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_WRITE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_ACCESS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAPPED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAP_POINTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_DRAW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_COPY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLES_PASSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC1_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_EQUATION_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_ENABLED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_VERTEX_ATTRIB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PROGRAM_POINT_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_POINTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_FUNC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_PASS_DEPTH_FAIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_PASS_DEPTH_PASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DRAW_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER0)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER6)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER7)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER9)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER10)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER11)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER12)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER13)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER14)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_BUFFER15)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_EQUATION_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIBS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VARYING_FLOATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_TYPE)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_1D_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DELETE_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPILE_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINK_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VALIDATE_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INFO_LOG_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATTACHED_SHADERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_UNIFORM_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_SOURCE_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_ATTRIBUTES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER_DERIVATIVE_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADING_LANGUAGE_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_PROGRAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SPRITE_COORD_ORIGIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOWER_LEFT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UPPER_LEFT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_REF)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_VALUE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_WRITEMASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_PACK_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_UNPACK_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_PACK_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_UNPACK_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT2x3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT2x4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT3x2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT3x4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT4x2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_MAT4x3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB8_ALPHA8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPARE_REF_TO_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE0)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE6)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE7)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CLIP_DISTANCES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAJOR_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINOR_VERSION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_EXTENSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_FLAGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA32F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB32F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ARRAY_TEXTURE_LAYERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_PROGRAM_TEXEL_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEXEL_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_READ_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIXED_ONLY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R11F_G11F_B10F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_10F_11F_11F_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB9_E5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_5_9_9_9_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SHARED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_VARYINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVES_GENERATED)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREEN_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLUE_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BGR_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BGRA_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_1D_ARRAY_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_ARRAY_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_VEC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_VEC3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_VEC4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_WAIT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_NO_WAIT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BY_REGION_WAIT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BY_REGION_NO_WAIT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_ACCESS_FLAGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAP_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_MAP_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT32F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH32F_STENCIL8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVALID_FRAMEBUFFER_OPERATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_UNDEFINED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_RENDERBUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_24_8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH24_STENCIL8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RED_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GREEN_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BLUE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_ALPHA_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEPTH_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_NORMALIZED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_FRAMEBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_FRAMEBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_FRAMEBUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_COMPLETE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_UNSUPPORTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COLOR_ATTACHMENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT0)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT5)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT6)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT7)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT9)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT10)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT11)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT12)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT13)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT14)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ATTACHMENT15)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_ATTACHMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_INTERNAL_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_INDEX16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_SRGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HALF_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RED_RGTC1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_RED_RGTC1)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RG_RGTC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_RG_RGTC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R32F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16F)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG32F)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_RECT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_RECT_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_RECT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_RECT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_DATA_STORE_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RECTANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_RECTANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_RECTANGLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_RECTANGLE_TEXTURE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_NORMALIZED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COPY_READ_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COPY_WRITE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_UNIFORM_BUFFER_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_UNIFORM_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINES_ADJACENCY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINE_STRIP_ADJACENCY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLES_ADJACENCY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_STRIP_ADJACENCY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_ATTACHMENT_LAYERED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_VERTICES_OUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_INPUT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_OUTPUT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_OUTPUT_VERTICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_INPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_INPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_PROFILE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_CLAMP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIRST_VERTEX_CONVENTION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LAST_VERTEX_CONVENTION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROVOKING_VERTEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_SEAMLESS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SERVER_WAIT_TIMEOUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_CONDITION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_STATUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_FLAGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_FENCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_GPU_COMMANDS_COMPLETE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNALED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNALED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALREADY_SIGNALED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TIMEOUT_EXPIRED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONDITION_SATISFIED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WAIT_FAILED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_POSITION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_MASK_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SAMPLE_MASK_WORDS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FIXED_SAMPLE_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COLOR_TEXTURE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEPTH_TEXTURE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_INTEGER_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_DIVISOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRC1_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC1_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_MINUS_SRC1_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ANY_SAMPLES_PASSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB10_A2UI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_R)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_G)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_B)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_A)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SWIZZLE_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TIME_ELAPSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TIMESTAMP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_2_10_10_10_REV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_SHADING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_SAMPLE_SHADING_VALUE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SHADER_INVOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_SHADER_INVOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_FRAGMENT_INTERPOLATION_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_INTERPOLATION_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_STREAMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_VEC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_VEC3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_VEC4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT2x3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT2x4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT3x2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT3x4)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT4x2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOUBLE_MAT4x3)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_SUBROUTINES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_SUBROUTINE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_SUBROUTINE_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SUBROUTINES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_COMPATIBLE_SUBROUTINES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPATIBLE_SUBROUTINES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATCHES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATCH_VERTICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATCH_DEFAULT_INNER_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATCH_DEFAULT_OUTER_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_OUTPUT_VERTICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_SPACING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_VERTEX_ORDER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_GEN_POINT_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ISOLINES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRACTIONAL_ODD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRACTIONAL_EVEN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PATCH_VERTICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_GEN_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_PATCH_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_INPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIXED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMPLEMENTATION_COLOR_READ_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMPLEMENTATION_COLOR_READ_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOW_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MEDIUM_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HIGH_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOW_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MEDIUM_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HIGH_INT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_COMPILER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_BINARY_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SHADER_BINARY_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNIFORM_VECTORS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VARYING_VECTORS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_UNIFORM_VECTORS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB565)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_RETRIEVABLE_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_PROGRAM_BINARY_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINARY_FORMATS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_SEPARABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_PROGRAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PIPELINE_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VIEWPORTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_BOUNDS_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LAYER_PROVOKING_VERTEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_INDEX_PROVOKING_VERTEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNDEFINED_VERTEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_COMPRESSED_BLOCK_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_COMPRESSED_BLOCK_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_COMPRESSED_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_COMPRESSED_BLOCK_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_COMPRESSED_BLOCK_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_COMPRESSED_BLOCK_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_COMPRESSED_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SAMPLE_COUNTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_MAP_BUFFER_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_ATOMIC_COUNTERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_ATOMIC_COUNTER_BUFFERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_ATOMIC_COUNTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_IMAGE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_IMAGE_UNITS_AND_FRAGMENT_OUTPUTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_NAME)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_LAYERED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_LAYER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_ACCESS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D_RECT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D_RECT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_3D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D_RECT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_CUBE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_1D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_IMAGE_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_BINDING_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_FORMAT_COMPATIBILITY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_FORMAT_COMPATIBILITY_BY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_FORMAT_COMPATIBILITY_BY_CLASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_IMAGE_UNIFORMS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_BPTC_UNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMMUTABLE_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SHADING_LANGUAGE_VERSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_LONG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB8_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA8_ETC2_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_R11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_R11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RG11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_RG11_EAC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_FIXED_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ELEMENT_INDEX)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISPATCH_INDIRECT_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISPATCH_INDIRECT_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_OUTPUT_SYNCHRONOUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CALLBACK_FUNCTION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CALLBACK_USER_PARAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_API)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_WINDOW_SYSTEM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_SHADER_COMPILER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_THIRD_PARTY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_APPLICATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SOURCE_OTHER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_ERROR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_PORTABILITY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_PERFORMANCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_OTHER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEBUG_MESSAGE_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEBUG_LOGGED_MESSAGES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_LOGGED_MESSAGES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_HIGH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_MEDIUM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_LOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_MARKER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_PUSH_GROUP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TYPE_POP_GROUP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_SEVERITY_NOTIFICATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEBUG_GROUP_STACK_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_GROUP_STACK_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PIPELINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_LABEL_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_OUTPUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_UNIFORM_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_LAYERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_LAYERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEBUFFER_SAMPLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_SUPPORTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_PREFERRED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_DEPTH_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_STENCIL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_SHARED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_RED_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_GREEN_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_BLUE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_ALPHA_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_DEPTH_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERNALFORMAT_STENCIL_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_LAYERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_DIMENSIONS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_COMPONENTS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_RENDERABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_RENDERABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_RENDERABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_RENDERABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_RENDERABLE_LAYERED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_BLEND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_PIXELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_PIXELS_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_PIXELS_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMAGE_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMAGE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GET_TEXTURE_IMAGE_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GET_TEXTURE_IMAGE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIPMAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MANUAL_GENERATE_MIPMAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AUTO_GENERATE_MIPMAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ENCODING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB_READ)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB_WRITE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GATHER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GATHER_SHADOW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_IMAGE_LOAD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_IMAGE_STORE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_IMAGE_ATOMIC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_TEXEL_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_COMPATIBILITY_CLASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_PIXEL_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_PIXEL_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_WRITE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_WRITE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSED_BLOCK_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPRESSED_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLEAR_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_COMPATIBILITY_CLASS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FULL_SUPPORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CAVEAT_SUPPORT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_4_X_32)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_2_X_32)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_1_X_32)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_4_X_16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_2_X_16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_1_X_16)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_4_X_8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_2_X_8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_1_X_8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_11_11_10)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CLASS_10_10_10_2)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_S3TC_DXT1_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_S3TC_DXT1_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_S3TC_DXT3_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_S3TC_DXT5_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_RGTC1_RED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_RGTC2_RG)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_BPTC_UNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEW_CLASS_BPTC_FLOAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BLOCK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_INPUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_OUTPUT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_VARIABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BLOCK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SUBROUTINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_SUBROUTINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_SUBROUTINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SUBROUTINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SUBROUTINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_SUBROUTINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SUBROUTINE_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_SUBROUTINE_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_SUBROUTINE_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SUBROUTINE_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SUBROUTINE_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_SUBROUTINE_UNIFORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_VARYING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_RESOURCES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_NAME_LENGTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_NUM_ACTIVE_VARIABLES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_NUM_COMPATIBLE_SUBROUTINES)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_TESS_CONTROL_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_TESS_EVALUATION_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_GEOMETRY_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_FRAGMENT_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCED_BY_COMPUTE_SHADER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TOP_LEVEL_ARRAY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TOP_LEVEL_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCATION_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IS_PER_PATCH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_START)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_STORAGE_BLOCK_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_TEXTURE_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_MIN_LEVEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_NUM_LEVELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_MIN_LAYER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_VIEW_NUM_LAYERS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_IMMUTABLE_LEVELS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_RELATIVE_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_DIVISOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIB_BINDINGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BINDING_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ATTRIB_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_IMMUTABLE_STORAGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_STORAGE_FLAGS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLEAR_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCATION_COMPONENT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BUFFER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_RESULT_NO_WAIT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIRROR_CLAMP_TO_EDGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_LOST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEGATIVE_ONE_TO_ONE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ZERO_TO_ONE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_ORIGIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DEPTH_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_WAIT_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_NO_WAIT_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BY_REGION_WAIT_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_BY_REGION_NO_WAIT_INVERTED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CULL_DISTANCES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_TARGET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_TARGET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GUILTY_CONTEXT_RESET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INNOCENT_CONTEXT_RESET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNKNOWN_CONTEXT_RESET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESET_NOTIFICATION_STRATEGY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOSE_CONTEXT_ON_RESET)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NO_RESET_NOTIFICATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_RELEASE_BEHAVIOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_CL_EVENT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_CL_EVENT_COMPLETE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_VARIABLE_GROUP_INVOCATIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COMPUTE_VARIABLE_GROUP_SIZE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PARAMETER_BUFFER_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PARAMETER_BUFFER_BINDING_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SRGB_DECODE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTICES_SUBMITTED_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVES_SUBMITTED_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_INVOCATIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_SHADER_PATCHES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER_INVOCATIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_SHADER_INVOCATIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIPPING_INPUT_PRIMITIVES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIPPING_OUTPUT_PRIMITIVES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_INCLUDE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NAMED_STRING_LENGTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NAMED_STRING_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPARSE_BUFFER_PAGE_SIZE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SPARSE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_INDEX_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_SPARSE_LEVELS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_VIRTUAL_PAGE_SIZES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_X_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_Y_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIRTUAL_PAGE_SIZE_Z_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPARSE_TEXTURE_SIZE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEXTURE_GATHER_COMPONENTS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_OVERFLOW_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_STREAM_OVERFLOW_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTEXT_ROBUST_ACCESS)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESCALE_NORMAL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LIGHT_MODEL_COLOR_CONTROL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SINGLE_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SEPARATE_SPECULAR_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALIASED_POINT_SIZE_RANGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIENT_ACTIVE_TEXTURE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_UNITS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_MODELVIEW_MATRIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_PROJECTION_MATRIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_TEXTURE_MATRIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_COLOR_MATRIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFLECTION_MAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_LUMINANCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_LUMINANCE_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_INTENSITY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINE_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINE_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE0_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE1_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE2_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE0_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE2_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND0_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND1_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND2_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND0_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND1_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND2_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ADD_SIGNED)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERPOLATE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUBTRACT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMARY_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PREVIOUS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT3_RGB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT3_RGBA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SIZE_MIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SIZE_MAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_DISTANCE_ATTENUATION)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GENERATE_MIPMAP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GENERATE_MIPMAP_HINT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE_SOURCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_FOG_COORDINATE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE_ARRAY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE_ARRAY_POINTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_SUM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_SECONDARY_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_STRIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_POINTER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FILTER_CONTROL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_TEXTURE_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_ARRAY_BUFFER_BINDING)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PROGRAM_TWO_SIDE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SPRITE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COORD_REPLACE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_COORDS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_RASTER_SECONDARY_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE8_ALPHA8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLUMINANCE8)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SLUMINANCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SLUMINANCE_ALPHA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LUMINANCE_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_INTENSITY_TYPE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_VERTEX_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLAMP_FRAGMENT_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_INTEGER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISPLAY_LIST)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_FLOAT_MODE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_PROGRAM_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_FORMAT_ASCII_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_LENGTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_FORMAT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_BINDING_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_TEMPORARIES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEMPORARIES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_TEMPORARIES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PARAMETERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_PARAMETERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_PARAMETERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_ATTRIBS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_ATTRIBS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_ATTRIBS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_ENV_PARAMETERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_ALU_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_TEX_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_TEX_INDIRECTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_STRING_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_ERROR_POSITION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_MATRIX_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_CURRENT_MATRIX_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_MATRIX_STACK_DEPTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_MATRICES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_ERROR_STRING_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX0_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX1_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX2_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX3_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX4_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX5_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX6_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX7_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX8_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX9_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX10_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX11_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX12_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX13_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX14_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX15_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX16_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX17_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX18_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX19_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX20_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX21_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX22_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX23_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX24_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX25_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX26_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX27_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX28_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX29_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX30_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX31_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_VERTICES_OUT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_INPUT_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_OUTPUT_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_VARYING_COMPONENTS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_VARYING_COMPONENTS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_1D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SEPARABLE_2D)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_BORDER_MODE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_FILTER_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_FILTER_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REDUCE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CONVOLUTION_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CONVOLUTION_HEIGHT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_RED_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_GREEN_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_BLUE_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_ALPHA_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_RED_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_GREEN_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_BLUE_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_ALPHA_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_HISTOGRAM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_LUMINANCE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HISTOGRAM_SINK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINMAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINMAX_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MINMAX_SINK)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TABLE_TOO_LARGE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_MATRIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_MATRIX_STACK_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_COLOR_MATRIX_STACK_DEPTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_RED_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_GREEN_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_BLUE_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_ALPHA_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_RED_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_GREEN_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_BLUE_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_ALPHA_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_CONVOLUTION_COLOR_TABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_COLOR_MATRIX_COLOR_TABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_COLOR_TABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_POST_CONVOLUTION_COLOR_TABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_SCALE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_BIAS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_FORMAT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_WIDTH)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_RED_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_GREEN_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_BLUE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_ALPHA_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_LUMINANCE_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_TABLE_INTENSITY_SIZE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_BORDER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLICATE_BORDER)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_BORDER_COLOR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_PALETTE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_MATRIX_PALETTE_STACK_DEPTH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PALETTE_MATRICES_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_PALETTE_MATRIX_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_INDEX_ARRAY_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_MATRIX_INDEX_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_INDEX_ARRAY_SIZE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_INDEX_ARRAY_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_INDEX_ARRAY_STRIDE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_INDEX_ARRAY_POINTER_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_OBJECT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_OBJECT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_FAIL_VALUE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BUFFER_FORMAT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA32F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY32F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE32F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA32F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA16F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY16F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA16F_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_UNITS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_VERTEX_UNITS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_SUM_UNITY_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_BLEND_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_WEIGHT_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_ARRAY_TYPE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_ARRAY_STRIDE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_ARRAY_SIZE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_ARRAY_POINTER_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WEIGHT_ARRAY_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW0_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW1_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW2_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW3_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW4_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW5_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW6_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW7_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW8_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW9_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW10_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW11_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW12_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW13_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW14_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW15_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW16_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW17_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW18_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW19_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW20_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW21_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW22_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW23_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW24_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW25_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW26_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW27_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW28_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW29_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW30_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW31_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PROGRAM_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_ADDRESS_REGISTERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_BUFFERS_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLES_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_FXT1_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_FXT1_3DFX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FACTOR_MIN_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FACTOR_MAX_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_API_ERROR_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_DEPRECATION_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_PERFORMANCE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_APPLICATION_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_CATEGORY_OTHER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_CLAMP_NEAR_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_CLAMP_FAR_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT64_NV)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ELEMENT_SWIZZLE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ID_SWIZZLE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DATA_BUFFER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFORMANCE_MONITOR_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUERY_OBJECT_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_OBJECT_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_OBJECT_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OCCLUSION_QUERY_EVENT_MASK_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COUNTER_TYPE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COUNTER_RANGE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT64_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERCENTAGE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_RESULT_AVAILABLE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_RESULT_SIZE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERFMON_RESULT_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUBSAMPLE_DISTANCE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_SPARSE_LEVEL_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIN_LOD_WARNING_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SET_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACE_VALUE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_OP_VALUE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_BACK_OP_VALUE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STREAM_RASTERIZATION_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_BUFFER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_BUFFER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_BUFFER_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESSELLATION_MODE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESSELLATION_FACTOR_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISCRETE_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTINUOUS_AMD)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AUX_DEPTH_STENCIL_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_CLIENT_STORAGE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_TYPE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_POINTER_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_PIXELS_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FENCE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_FLOAT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_SERIALIZED_MODIFY_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_FLUSHING_UNMAP_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_OBJECT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RELEASED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VOLATILE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RETAINED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNDEFINED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PURGEABLE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_422_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_8_8_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_SHORT_8_8_REV_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_RAW_422_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_ROW_BYTES_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_ROW_BYTES_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LIGHT_MODEL_SPECULAR_VECTOR_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RANGE_LENGTH_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RANGE_POINTER_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_STORAGE_HINT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STORAGE_PRIVATE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STORAGE_CACHED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STORAGE_SHARED_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_HINT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_RANGE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_RANGE_LENGTH_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_STORAGE_HINT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_RANGE_POINTER_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STORAGE_CLIENT_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP1_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP2_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP1_SIZE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP1_COEFF_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP1_ORDER_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP1_DOMAIN_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP2_SIZE_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP2_COEFF_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP2_ORDER_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_MAP2_DOMAIN_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCBCR_422_APPLE)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_TYPE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_POINTER_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUMP_ROT_MATRIX_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUMP_ROT_MATRIX_SIZE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUMP_NUM_TEX_UNITS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUMP_TEX_UNITS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUDV_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DU8DV8_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUMP_ENVMAP_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUMP_TARGET_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_SHADER_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_0_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_1_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_2_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_3_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_4_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_5_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_6_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_7_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_8_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_9_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_10_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_11_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_12_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_13_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_14_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_15_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_16_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_17_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_18_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_19_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_20_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_21_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_22_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_23_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_24_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_25_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_26_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_27_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_28_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_29_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_30_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REG_31_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_0_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_1_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_2_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_3_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_4_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_5_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_6_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_7_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_8_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_9_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_10_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_11_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_12_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_13_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_14_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_15_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_16_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_17_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_18_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_19_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_20_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_21_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_22_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_23_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_24_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_25_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_26_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_27_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_28_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_29_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_30_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CON_31_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MOV_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ADD_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MUL_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUB_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT3_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT4_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAD_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LERP_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CND_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CND0_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT2_ADD_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_INTERPOLATOR_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_FRAGMENT_REGISTERS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_FRAGMENT_CONSTANTS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_PASSES_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_INSTRUCTIONS_PER_PASS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_INSTRUCTIONS_TOTAL_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_INPUT_INTERPOLATOR_COMPONENTS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_LOOPBACK_COMPONENTS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ALPHA_PAIRING_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SWIZZLE_STR_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SWIZZLE_STQ_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SWIZZLE_STR_DR_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SWIZZLE_STQ_DQ_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SWIZZLE_STRQ_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SWIZZLE_STRQ_DQ_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VBO_FREE_MEMORY_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FREE_MEMORY_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_FREE_MEMORY_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_CLEAR_UNCLAMPED_VALUE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_POINT_MODE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_NORMAL_MODE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXT_FRAGMENT_SHADER_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODULATE_ADD_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODULATE_SIGNED_ADD_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODULATE_SUBTRACT_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIRROR_CLAMP_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STATIC_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DYNAMIC_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRESERVE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISCARD_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_OBJECT_BUFFER_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_OBJECT_OFFSET_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_STREAMS_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM0_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM1_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM2_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM3_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM4_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM5_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM6_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STREAM7_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SOURCE_ATI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_422_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_422_REV_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_422_AVERAGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_422_REV_AVERAGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ABGR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_BINDABLE_UNIFORMS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_BINDABLE_UNIFORMS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GEOMETRY_BINDABLE_UNIFORMS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_BINDABLE_UNIFORM_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_BINDING_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_VOLUME_CLIPPING_HINT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CMYK_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CMYKA_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_CMYK_HINT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_CMYK_HINT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_ELEMENT_LOCK_FIRST_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ARRAY_ELEMENT_LOCK_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TANGENT_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BINORMAL_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_TANGENT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_BINORMAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TANGENT_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TANGENT_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BINORMAL_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BINORMAL_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TANGENT_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BINORMAL_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_TANGENT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_TANGENT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_BINORMAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_BINORMAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_VERTEX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_VERTEX_EYE_POSITION_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_VERTEX_OBJECT_POSITION_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PIPELINE_OBJECT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_BOUNDS_TEST_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_BOUNDS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_PROGRAM_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_MATRIX_STACK_DEPTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALED_RESOLVE_FASTEST_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALED_RESOLVE_NICEST_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IUI_V2F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IUI_V3F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IUI_N3F_V2F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IUI_N3F_V3F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_T2F_IUI_V2F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_T2F_IUI_V3F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_T2F_IUI_N3F_V2F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_T2F_IUI_N3F_V3F_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_TEST_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_TEST_FUNC_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_TEST_REF_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_MATERIAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_MATERIAL_PARAMETER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_MATERIAL_FACE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_MATERIAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_NORMAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COLOR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATTENUATION_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADOW_ATTENUATION_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_APPLICATION_MODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LIGHT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MATERIAL_FACE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MATERIAL_PARAMETER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_1PASS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_2PASS_0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_2PASS_1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_4PASS_0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_4PASS_1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_4PASS_2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_4PASS_3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_PATTERN_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_SIGNED_COMPONENTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_INDEX1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_INDEX2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_INDEX4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_INDEX8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_INDEX12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_INDEX16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_INDEX_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TRANSFORM_2D_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_MAG_FILTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_MIN_FILTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_CUBIC_WEIGHT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CUBIC_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AVERAGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TRANSFORM_2D_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_BIAS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POLYGON_OFFSET_CLAMP_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_MULTISAMPLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_RASTER_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_RASTERIZATION_ALLOWED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EFFECTIVE_RASTER_SAMPLES_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHARED_TEXTURE_PALETTE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_CLEAR_TAG_VALUE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_TEST_TWO_SIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_STENCIL_FACE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE4_ALPHA4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE6_ALPHA2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8_ALPHA8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE12_ALPHA4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE12_ALPHA12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16_ALPHA16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LUMINANCE_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_INTENSITY_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_TOO_LARGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_LUMINANCE_LATC1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT3_RGB_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_ANISOTROPY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA32UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY32UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE32UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA32UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA16UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY16UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA16UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA8UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY8UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA8UI_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA32I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY32I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE32I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA32I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA16I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY16I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA16I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA8I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY8I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA8I_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_INTEGER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA_INTEGER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_INTEGER_MODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIRROR_CLAMP_TO_BORDER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_PRIORITY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RESIDENT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PERTURB_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_NORMAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SRGB_DECODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DECODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SKIP_DECODE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE_ALPHA_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE8_ALPHA8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY8_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LUMINANCE16_ALPHA16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTENSITY16_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RED_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RG_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_SNORM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_COUNT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_BINDING_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_INDEX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_NEGATE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_DOT3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_DOT4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_MUL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_ADD_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_MADD_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_FRAC_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_MAX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_MIN_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_SET_GE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_SET_LT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_CLAMP_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_FLOOR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_ROUND_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_EXP_BASE_2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_LOG_BASE_2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_POWER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_RECIP_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_RECIP_SQRT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_SUB_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_CROSS_PRODUCT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_MULTIPLY_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OP_MOV_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_VERTEX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_COLOR0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_COLOR1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD5_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD6_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD7_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD9_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD10_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD11_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD12_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD13_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD14_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD15_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD16_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD17_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD18_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD19_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD20_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD21_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD22_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD23_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD24_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD25_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD26_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD27_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD28_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD29_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD30_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_TEXTURE_COORD31_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OUTPUT_FOG_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALAR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VECTOR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVARIANT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCAL_CONSTANT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_INSTRUCTIONS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_VARIANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_INVARIANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_LOCAL_CONSTANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_SHADER_LOCALS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_OPTIMIZED_VERTEX_SHADER_INSTRUCTIONS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_OPTIMIZED_VERTEX_SHADER_VARIANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_OPTIMIZED_VERTEX_SHADER_INVARIANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCALS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_INSTRUCTIONS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_VARIANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_INVARIANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_LOCAL_CONSTANTS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_LOCALS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_SHADER_OPTIMIZED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_X_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Y_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_W_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEGATIVE_X_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEGATIVE_Y_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEGATIVE_Z_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEGATIVE_W_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ZERO_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ONE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEGATIVE_ONE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMALIZED_RANGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FULL_RANGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_VERTEX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MVP_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_VALUE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_DATATYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIANT_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVARIANT_VALUE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVARIANT_DATATYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCAL_CONSTANT_VALUE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LOCAL_CONSTANT_DATATYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW0_STACK_DEPTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW1_STACK_DEPTH_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW0_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW1_MATRIX_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_WEIGHTING_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_VERTEX_WEIGHT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_WEIGHT_ARRAY_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_WEIGHT_ARRAY_SIZE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_WEIGHT_ARRAY_TYPE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_WEIGHT_ARRAY_STRIDE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_WEIGHT_ARRAY_POINTER_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SYNC_X11_FENCE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TOOL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TOOL_NAME_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEBUG_TOOL_PURPOSE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IGNORE_BORDER_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_SCALE_X_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_SCALE_Y_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_TRANSLATE_X_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_TRANSLATE_Y_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_ROTATE_ANGLE_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_ROTATE_ORIGIN_X_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_ROTATE_ORIGIN_Y_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_MAG_FILTER_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_MIN_FILTER_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_CUBIC_WEIGHT_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CUBIC_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_AVERAGE_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IMAGE_TRANSFORM_2D_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_IMAGE_TRANSFORM_COLOR_TABLE_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_POST_IMAGE_TRANSFORM_COLOR_TABLE_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OCCLUSION_TEST_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OCCLUSION_TEST_RESULT_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LIGHTING_MODE_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_POST_SPECULAR_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_PRE_SPECULAR_HP)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RASTER_POSITION_UNCLIPPED_IBM)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RED_MIN_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREEN_MIN_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLUE_MIN_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_MIN_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RED_MAX_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GREEN_MAX_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLUE_MAX_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_MAX_CLAMP_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERLACE_READ_INGR)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MEMORY_LAYOUT_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PARALLEL_ARRAYS_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_PARALLEL_POINTERS_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_PARALLEL_POINTERS_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_PARALLEL_POINTERS_INTEL)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_PARALLEL_POINTERS_INTEL)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_1D_STACK_MESAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_STACK_MESAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_1D_STACK_MESAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_2D_STACK_MESAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_1D_STACK_BINDING_MESAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_2D_STACK_BINDING_MESAX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_INVERT_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCBCR_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_OVERLAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BLEND_PREMULTIPLIED_SRC_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONJOINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONTRAST_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISJOINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_ATOP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_IN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_OUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DST_OVER_NV)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ADDRESS_COMMAND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ATTRIBUTE_ADDRESS_COMMAND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_REF_COMMAND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIEWPORT_COMMAND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCISSOR_COMMAND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRONT_FACE_COMMAND_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_PROGRAM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMPUTE_PROGRAM_PARAMETER_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVATIVE_RASTERIZATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_TO_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_STENCIL_TO_BGRA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEEP_3D_TEXTURE_WIDTH_HEIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEEP_3D_TEXTURE_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_COMPONENT32F_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH32F_STENCIL8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_BUFFER_FLOAT_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_TRIANGULAR_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP_TESSELLATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP_ATTRIB_U_ORDER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP_ATTRIB_V_ORDER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_FRACTIONAL_TESSELLATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB0_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB5_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB6_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB7_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB9_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB10_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB11_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB12_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB13_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB14_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EVAL_VERTEX_ATTRIB15_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_MAP_TESSELLATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_RATIONAL_EVAL_ORDER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BINDING_RENDERBUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RENDERBUFFER_DATA_STORE_BINDING_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_RENDERBUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLER_RENDERBUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INT_SAMPLER_RENDERBUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_SAMPLER_RENDERBUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALL_COMPLETED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FENCE_STATUS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FENCE_CONDITION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILL_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_R_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RG_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_R16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_R32_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RG16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RG32_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGB16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGB32_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGBA16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGBA32_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FLOAT_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_CLEAR_COLOR_VALUE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FLOAT_RGBA_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_DISTANCE_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_RADIAL_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_PLANE_ABSOLUTE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COVERAGE_TO_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COVERAGE_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_PROGRAM_LOCAL_PARAMETERS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_PROGRAM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_PROGRAM_BINDING_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_EXEC_INSTRUCTIONS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_CALL_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_IF_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_LOOP_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_LOOP_COUNT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_MODULATION_TABLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPTH_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STENCIL_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIXED_DEPTH_SAMPLES_SUPPORTED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MIXED_STENCIL_SAMPLES_SUPPORTED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_MODULATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COVERAGE_MODULATION_TABLE_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RENDERBUFFER_COLOR_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_MULTISAMPLE_COVERAGE_MODES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_COVERAGE_MODES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_PROGRAM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_OUTPUT_VERTICES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_TOTAL_OUTPUT_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_ATTRIB_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_RESULT_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_ATTRIB_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_RESULT_COMPONENTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_GENERIC_ATTRIBS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_GENERIC_RESULTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_SUBROUTINE_PARAMETERS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_SUBROUTINE_NUM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUPERSAMPLE_SCALE_X_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUPERSAMPLE_SCALE_Y_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONFORMANT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHININESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SPOT_EXPONENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MULTISAMPLE_FILTER_HINT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_PARAMETER_BUFFER_BINDINGS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_PARAMETER_BUFFER_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GL_2_BYTES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_3_BYTES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_4_BYTES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_LINEAR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_LINEAR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_FOG_GEN_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMARY_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_GEN_COLOR_FORMAT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_PROJECTION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MAX_MODELVIEW_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_PROJECTION_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_PROJECTION_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PATH_MAX_PROJECTION_STACK_DEPTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_INPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITE_PIXEL_DATA_RANGE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_PIXEL_DATA_RANGE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITE_PIXEL_DATA_RANGE_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_PIXEL_DATA_RANGE_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITE_PIXEL_DATA_RANGE_POINTER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_READ_PIXEL_DATA_RANGE_POINTER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POINT_SPRITE_R_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAME_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIELDS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_FILL_STREAMS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRESENT_TIME_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRESENT_DURATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_RESTART_INDEX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REGISTER_COMBINERS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_A_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_B_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_C_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_E_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_F_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VARIABLE_G_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_COLOR0_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSTANT_COLOR1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPARE0_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPARE1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DISCARD_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_E_TIMES_F_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPARE0_PLUS_SECONDARY_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_IDENTITY_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INVERT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EXPAND_NORMAL_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EXPAND_NEGATE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HALF_BIAS_NORMAL_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HALF_BIAS_NEGATE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_IDENTITY_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_NEGATE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALE_BY_TWO_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALE_BY_FOUR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALE_BY_ONE_HALF_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BIAS_BY_NEGATIVE_ONE_HALF_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_INPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_MAPPING_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_COMPONENT_USAGE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_AB_DOT_PRODUCT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_CD_DOT_PRODUCT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_MUX_SUM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_AB_OUTPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_CD_OUTPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER_SUM_OUTPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_GENERAL_COMBINERS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_GENERAL_COMBINERS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_SUM_CLAMP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER0_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER5_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER6_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINER7_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PER_STAGE_CONSTANTS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_LOCATION_PIXEL_GRID_WIDTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SAMPLE_LOCATION_PIXEL_GRID_HEIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAMMABLE_SAMPLE_LOCATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEBUFFER_SAMPLE_LOCATION_PIXEL_GRID_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BUFFER_GPU_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GPU_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_SHADER_BUFFER_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WARP_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WARPS_PER_SM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SM_COUNT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_PROGRAM_PATCH_ATTRIBS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_PROGRAM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_PROGRAM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_CONTROL_PROGRAM_PARAMETER_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TESS_EVALUATION_PROGRAM_PARAMETER_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EMBOSS_LIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EMBOSS_CONSTANT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EMBOSS_MAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COMBINE4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE3_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SOURCE3_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND3_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OPERAND3_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_UNSIGNED_REMAP_MODE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COVERAGE_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COLOR_SAMPLES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_TEXTURE_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_TEXTURE_RECTANGLE_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_TEXTURE_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_S8_S8_8_8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNSIGNED_INT_8_8_S8_S8_REV_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT_MAG_INTENSITY_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_CONSISTENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_SHADER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHADER_OPERATION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_MODES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_TEXTURE_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_TEXTURE_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_TEXTURE_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PREVIOUS_TEXTURE_INPUT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONST_EYE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PASS_THROUGH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CULL_FRAGMENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPENDENT_AR_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPENDENT_GB_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_DEPTH_REPLACE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HILO_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT_MAG_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT_MAG_VIB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HILO16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_HILO_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_HILO16_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_RGBA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_RGBA8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_RGB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_RGB8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_LUMINANCE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_LUMINANCE8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_LUMINANCE_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_LUMINANCE8_ALPHA8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_ALPHA8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_INTENSITY_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_INTENSITY8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT8_MAG8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DSDT8_MAG8_INTENSITY8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_RGB_UNSIGNED_ALPHA_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HI_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LO_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DS_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DT_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAGNITUDE_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIBRANCE_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HI_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LO_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DS_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DT_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAGNITUDE_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIBRANCE_BIAS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_BORDER_VALUES_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_HI_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LO_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DS_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DT_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAG_SIZE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_TEXTURE_3D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_PROJECTIVE_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_PROJECTIVE_TEXTURE_2D_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_SCALE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_HILO_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_HILO_TEXTURE_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_HILO_PROJECTIVE_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OFFSET_HILO_PROJECTIVE_TEXTURE_RECTANGLE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPENDENT_HILO_TEXTURE_2D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPENDENT_RGB_TEXTURE_3D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEPENDENT_RGB_TEXTURE_CUBE_MAP_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_PASS_THROUGH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_TEXTURE_1D_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DOT_PRODUCT_AFFINE_DEPTH_REPLACE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_HILO8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SIGNED_HILO8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FORCE_BLUE_TO_ONE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK_PRIMARY_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK_SECONDARY_COLOR_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_DISTANCE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ID_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PRIMITIVE_ID_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GENERIC_ATTRIB_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_ATTRIBS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_VARYINGS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ACTIVE_VARYING_MAX_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSFORM_FEEDBACK_RECORD_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LAYER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_UNIFIED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNIFORM_BUFFER_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SURFACE_STATE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SURFACE_REGISTERED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SURFACE_MAPPED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRITE_DISCARD_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_UNIFIED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORD_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NORMAL_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_COLOR_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INDEX_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COORD_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EDGE_FLAG_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SECONDARY_COLOR_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_COORD_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ELEMENT_ARRAY_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_UNIFIED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_ADDRESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DRAW_INDIRECT_LENGTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_STATE_PROGRAM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MODELVIEW_PROJECTION_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IDENTITY_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERSE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRANSPOSE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERSE_TRANSPOSE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX0_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX5_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX6_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATRIX7_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_PARAMETER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_TARGET_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROGRAM_RESIDENT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRACK_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRACK_MATRIX_TRANSFORM_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PROGRAM_BINDING_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY0_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY1_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY2_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY3_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY5_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY6_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY7_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY8_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY9_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY10_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY11_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY12_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY13_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY14_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_ATTRIB_ARRAY15_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB0_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB1_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB2_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB3_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB4_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB5_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB6_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB7_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB8_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB9_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB10_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB11_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB12_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB13_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB14_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP1_VERTEX_ATTRIB15_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB0_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB1_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB2_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB3_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB4_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB5_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB6_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB8_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB9_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB10_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB11_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB12_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB13_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB14_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAP2_VERTEX_ATTRIB15_4_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_BUFFER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_BUFFER_BINDING_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIELD_UPPER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FIELD_LOWER_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NUM_VIDEO_CAPTURE_STREAMS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEXT_VIDEO_CAPTURE_BUFFER_STATUS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_CAPTURE_TO_422_SUPPORTED_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LAST_VIDEO_CAPTURE_STATUS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_BUFFER_PITCH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_COLOR_CONVERSION_MATRIX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_COLOR_CONVERSION_MAX_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_COLOR_CONVERSION_MIN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_COLOR_CONVERSION_OFFSET_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_BUFFER_INTERNAL_FORMAT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PARTIAL_SUCCESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SUCCESS_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FAILURE_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCBYCR8_422_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCBAYCR8A_4224_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z6Y10Z6CB10Z6Y10Z6CR10_422_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z6Y10Z6CB10Z6A10Z6Y10Z6CR10Z6A10_4224_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z4Y12Z4CB12Z4Y12Z4CR12_422_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z4Y12Z4CB12Z4A12Z4Y12Z4CR12Z4A12_4224_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_Z4Y12Z4CB12Z4CR12_444_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_CAPTURE_FRAME_WIDTH_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_CAPTURE_FRAME_HEIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_CAPTURE_FIELD_UPPER_HEIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_CAPTURE_FIELD_LOWER_HEIGHT_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VIDEO_CAPTURE_SURFACE_ORIGIN_NV)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERLACE_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERLACE_READ_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_RESAMPLE_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_RESAMPLE_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_REPLICATE_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_ZERO_FILL_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_AVERAGE_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_DECIMATE_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FORMAT_SUBSAMPLE_24_24_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FORMAT_SUBSAMPLE_244_244_OML)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PREFER_DOUBLEBUFFER_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONSERVE_MEMORY_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RECLAIM_MEMORY_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NATIVE_GRAPHICS_HANDLE_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NATIVE_GRAPHICS_BEGIN_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NATIVE_GRAPHICS_END_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALWAYS_FAST_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALWAYS_SOFT_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALLOW_DRAW_OBJ_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALLOW_DRAW_WIN_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALLOW_DRAW_FRG_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALLOW_DRAW_MEM_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STRICT_DEPTHFUNC_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STRICT_LIGHTING_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_STRICT_SCISSOR_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FULL_STIPPLE_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_NEAR_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CLIP_FAR_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WIDE_LINE_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_BACK_NORMALS_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_DATA_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_CONSISTENT_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MATERIAL_SIDE_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_VERTEX_HINT_PGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCREEN_COORDINATES_REND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INVERTED_SCREEN_W_REND)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB_S3TC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGB4_S3TC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_S3TC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA4_S3TC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA_DXT5_S3TC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RGBA4_DXT5_S3TC)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DETAIL_TEXTURE_2D_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DETAIL_TEXTURE_2D_BINDING_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_DETAIL_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_DETAIL_ALPHA_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_DETAIL_COLOR_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DETAIL_TEXTURE_LEVEL_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DETAIL_TEXTURE_MODE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DETAIL_TEXTURE_FUNC_POINTS_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_FUNC_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_FUNC_POINTS_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FOG_FUNC_POINTS_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TEXTURE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_FRAGMENT_RGB_SOURCE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_FRAGMENT_ALPHA_SOURCE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_GROUP_COLOR_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_DISTANCE_TO_POINT_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_DISTANCE_TO_POINT_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_DISTANCE_TO_LINE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_DISTANCE_TO_LINE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_POINT_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_POINT_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_EYE_LINE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_OBJECT_LINE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_SHARPEN_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_SHARPEN_ALPHA_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_SHARPEN_COLOR_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SHARPEN_TEXTURE_FUNC_POINTS_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SKIP_VOLUMES_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_IMAGE_DEPTH_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SKIP_VOLUMES_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_IMAGE_DEPTH_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_4D_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_4D_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_4DSIZE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_WRAP_Q_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_4D_TEXTURE_SIZE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_4D_BINDING_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COLOR_WRITEMASK_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FILTER4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_FILTER4_SIZE_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_ALPHA4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_ALPHA8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_ALPHA12_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_ALPHA16_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_LUMINANCE4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_LUMINANCE8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_LUMINANCE12_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_LUMINANCE16_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_INTENSITY4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_INTENSITY8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_INTENSITY12_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_INTENSITY16_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_LUMINANCE_ALPHA4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_LUMINANCE_ALPHA8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_ALPHA4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_ALPHA8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_LUMINANCE4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_LUMINANCE8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_INTENSITY4_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_INTENSITY8_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DUAL_TEXTURE_SELECT_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_TEXTURE_SELECT_SGIS)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ASYNC_MARKER_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ASYNC_HISTOGRAM_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ASYNC_HISTOGRAM_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ASYNC_TEX_IMAGE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ASYNC_DRAW_PIXELS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ASYNC_READ_PIXELS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ASYNC_TEX_IMAGE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ASYNC_DRAW_PIXELS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ASYNC_READ_PIXELS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_MIN_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_ALPHA_MAX_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CALLIGRAPHIC_FRAGMENT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_CLIPMAP_LINEAR_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CLIPMAP_CENTER_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CLIPMAP_FRAME_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CLIPMAP_OFFSET_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CLIPMAP_VIRTUAL_DEPTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CLIPMAP_LOD_OFFSET_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CLIPMAP_DEPTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CLIPMAP_DEPTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_CLIPMAP_VIRTUAL_DEPTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEAREST_CLIPMAP_NEAREST_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_NEAREST_CLIPMAP_LINEAR_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LINEAR_CLIPMAP_NEAREST_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CONVOLUTION_HINT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_OFFSET_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_OFFSET_VALUE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHTING_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COLOR_MATERIAL_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COLOR_MATERIAL_FACE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_COLOR_MATERIAL_PARAMETER_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAGMENT_LIGHTS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_ACTIVE_LIGHTS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_CURRENT_RASTER_NORMAL_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LIGHT_ENV_MODE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT_MODEL_LOCAL_VIEWER_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT_MODEL_TWO_SIDE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT_MODEL_NORMAL_INTERPOLATION_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT0_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT1_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT2_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT3_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT4_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT5_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT6_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAGMENT_LIGHT7_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEZOOM_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FRAMEZOOM_FACTOR_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_FRAMEZOOM_FACTOR_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INSTRUMENT_BUFFER_POINTER_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INSTRUMENT_MEASUREMENTS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_INTERLACE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_IR_INSTRUMENT1_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_LIST_PRIORITY_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TEX_GEN_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TEX_GEN_MODE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_BEST_ALIGNMENT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_CACHE_INCREMENT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_WIDTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_HEIGHT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_GRID_WIDTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_GRID_HEIGHT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_GRID_DEPTH_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_TILE_CACHE_SIZE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GEOMETRY_DEFORMATION_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_DEFORMATION_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_DEFORMATIONS_MASK_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_MAX_DEFORMATION_ORDER_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCE_PLANE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REFERENCE_PLANE_EQUATION_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_RESAMPLE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_RESAMPLE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_REPLICATE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_ZERO_FILL_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_RESAMPLE_DECIMATE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SCALEBIAS_HINT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COMPARE_OPERATOR_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LEQUAL_R_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_GEQUAL_R_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_MODE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_AXIS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_TRANSLATION_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_AXIAL_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_OBJECT_ALIGNED_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SPRITE_EYE_ALIGNED_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PACK_SUBSAMPLE_RATE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_SUBSAMPLE_RATE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_SUBSAMPLE_4444_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_SUBSAMPLE_2424_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PIXEL_SUBSAMPLE_4242_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_ENV_BIAS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_CLAMP_S_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_CLAMP_T_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MAX_CLAMP_R_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LOD_BIAS_S_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LOD_BIAS_T_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_LOD_BIAS_R_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_MULTI_BUFFER_HINT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_TEXTURE_FILTER_BIAS_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_TEXTURE_FILTER_SCALE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_TEXTURE_FILTER_BIAS_RANGE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_POST_TEXTURE_FILTER_SCALE_RANGE_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PRECLIP_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_VERTEX_PRECLIP_HINT_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCRCB_422_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCRCB_444_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCRCB_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_YCRCBA_SGIX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_COLOR_TABLE_SGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PROXY_TEXTURE_COLOR_TABLE_SGI)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_UNPACK_CONSTANT_DATA_SUNX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TEXTURE_CONSTANT_DATA_SUNX)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_WRAP_BORDER_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GLOBAL_ALPHA_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_GLOBAL_ALPHA_FACTOR_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_QUAD_MESH_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_MESH_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_SLICE_ACCUM_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_TRIANGLE_LIST_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACEMENT_CODE_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACEMENT_CODE_ARRAY_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACEMENT_CODE_ARRAY_TYPE_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACEMENT_CODE_ARRAY_STRIDE_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_REPLACEMENT_CODE_ARRAY_POINTER_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_C4UB_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_C3F_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_N3F_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_C4F_N3F_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_T2F_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_T2F_N3F_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_R1UI_T2F_C4F_N3F_V3F_SUN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PHONG_WIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_PHONG_HINT_WIN)
		TOSTR_CASE_STRINGIZE_GLENUM(GL_FOG_SPECULAR_TEXTURE_WIN)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_RELEASE_BEHAVIOR_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_MAJOR_VERSION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_MINOR_VERSION_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_LAYER_PLANE_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_CONTEXT_FLAGS_ARB)
		TOSTR_CASE_STRINGIZE_GLENUM(ERROR_INVALID_VERSION_ARB)
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
		TOSTR_CASE_STRINGIZE_GLENUM(WGL_NUMBER_PIXEL_FORMATS_ARB)
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
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_PBUFFER_CLOBBER_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_BACK_BUFFER_AGE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_STEREO_TREE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_SWAP_INTERVAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_MAX_SWAP_INTERVAL_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_LATE_SWAPS_TEAR_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_BIND_TO_MIPMAP_TEXTURE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_BIND_TO_TEXTURE_TARGETS_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_Y_INVERTED_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_FORMAT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_TARGET_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_MIPMAP_TEXTURE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_FORMAT_NONE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_FORMAT_RGB_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_FORMAT_RGBA_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_1D_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_2D_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_TEXTURE_RECTANGLE_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_FRONT_LEFT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_FRONT_RIGHT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_BACK_LEFT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_BACK_RIGHT_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX0_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX1_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX2_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX3_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX4_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX5_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX6_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX7_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX8_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_AUX9_EXT)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_DEVICE_ID_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_VERSION_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_ACCELERATED_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_VIDEO_MEMORY_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_UNIFIED_MEMORY_ARCHITECTURE_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_PREFERRED_PROFILE_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_RENDERER_OPENGL_CORE_PROFILE_VERSION_MESA)
		TOSTR_CASE_STRINGIZE_GLENUM(GLX_DEVICE_ID_NV)
		default: break;
	}
	
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "GLenum<%x>", (uint32_t)el);

	return tostrBuf;

#define GLenum RDCGLenum
}
