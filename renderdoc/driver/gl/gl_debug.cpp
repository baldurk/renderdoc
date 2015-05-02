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

#include "gl_replay.h"
#include "gl_driver.h"
#include "gl_resources.h"
#include "maths/matrix.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"

#include "data/glsl/debuguniforms.h"

#include "serialise/string_utils.h"

#include <algorithm>

GLuint GLReplay::CreateCShaderProgram(const char *csSrc)
{
	if(m_pDriver == NULL) return 0;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	WrappedOpenGL &gl = *m_pDriver;

	GLuint cs = gl.glCreateShader(eGL_COMPUTE_SHADER);

	gl.glShaderSource(cs, 1, &csSrc, NULL);

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

GLuint GLReplay::CreateShaderProgram(const char *vsSrc, const char *fsSrc, const char *gsSrc)
{
	if(m_pDriver == NULL) return 0;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	WrappedOpenGL &gl = *m_pDriver;

	GLuint vs = 0;
	GLuint fs = 0;
	GLuint gs = 0;

	char buffer[1024];
	GLint status = 0;

	if(vsSrc)
	{
		vs = gl.glCreateShader(eGL_VERTEX_SHADER);
		gl.glShaderSource(vs, 1, &vsSrc, NULL);

		gl.glCompileShader(vs);

		gl.glGetShaderiv(vs, eGL_COMPILE_STATUS, &status);
		if(status == 0)
		{
			gl.glGetShaderInfoLog(vs, 1024, NULL, buffer);
			RDCERR("Shader error: %s", buffer);
		}
	}

	if(fsSrc)
	{
		fs = gl.glCreateShader(eGL_FRAGMENT_SHADER);
		gl.glShaderSource(fs, 1, &fsSrc, NULL);

		gl.glCompileShader(fs);

		gl.glGetShaderiv(fs, eGL_COMPILE_STATUS, &status);
		if(status == 0)
		{
			gl.glGetShaderInfoLog(fs, 1024, NULL, buffer);
			RDCERR("Shader error: %s", buffer);
		}
	}

	if(gsSrc)
	{
		gs = gl.glCreateShader(eGL_GEOMETRY_SHADER);
		gl.glShaderSource(gs, 1, &gsSrc, NULL);

		gl.glCompileShader(gs);

		gl.glGetShaderiv(gs, eGL_COMPILE_STATUS, &status);
		if(status == 0)
		{
			gl.glGetShaderInfoLog(gs, 1024, NULL, buffer);
			RDCERR("Shader error: %s", buffer);
		}
	}

	GLuint ret = gl.glCreateProgram();

	if(vs) gl.glAttachShader(ret, vs);
	if(fs) gl.glAttachShader(ret, fs);
	if(gs) gl.glAttachShader(ret, gs);
	
	gl.glProgramParameteri(ret, eGL_PROGRAM_SEPARABLE, GL_TRUE);

	gl.glLinkProgram(ret);

	gl.glGetShaderiv(ret, eGL_LINK_STATUS, &status);
	if(status == 0)
	{
		gl.glGetProgramInfoLog(ret, 1024, NULL, buffer);
		RDCERR("Shader error: %s", buffer);
	}

	if(vs) gl.glDetachShader(ret, vs);
	if(fs) gl.glDetachShader(ret, fs);
	if(gs) gl.glDetachShader(ret, gs);

	if(vs) gl.glDeleteShader(vs);
	if(fs) gl.glDeleteShader(fs);
	if(gs) gl.glDeleteShader(gs);

	return ret;
}

void GLReplay::InitDebugData()
{
	if(m_pDriver == NULL) return;
	
	{
		uint64_t id = MakeOutputWindow(NULL, true);

		m_DebugID = id;
		m_DebugCtx = &m_OutputWindows[id];

		MakeCurrentReplayContext(m_DebugCtx);
	}

	WrappedOpenGL &gl = *m_pDriver;

	DebugData.outWidth = 0.0f; DebugData.outHeight = 0.0f;
	
	string blitvsSource = GetEmbeddedResource(blit_vert);
	string blitfsSource = GetEmbeddedResource(blit_frag);

	DebugData.blitProg = CreateShaderProgram(blitvsSource.c_str(), blitfsSource.c_str());
	
	string glslheader = "#version 420 core\n\n";
	glslheader += GetEmbeddedResource(debuguniforms_h);

	string texfs = GetEmbeddedResource(texsample_h);
	texfs += GetEmbeddedResource(texdisplay_frag);

	DebugData.texDisplayVSProg = CreateShaderProgram(blitvsSource.c_str(), NULL);

	for(int i=0; i < 3; i++)
	{
		string glsl = glslheader;
		glsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
		glsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
		glsl += texfs;

		DebugData.texDisplayProg[i] = CreateShaderProgram(NULL, glsl.c_str());
	}

	GLint numsl = 0;
	gl.glGetIntegerv(eGL_NUM_SHADING_LANGUAGE_VERSIONS, &numsl);

	bool support450 = false;
	for(GLint i=0; i < numsl; i++)
	{
		const char *sl = (const char *)gl.glGetStringi(eGL_SHADING_LANGUAGE_VERSION, (GLuint)i);

		if(sl[0] == '4' && sl[1] == '5' && sl[2] == '0')
			support450 = true;
		if(sl[0] == '4' && sl[1] == '.' && sl[2] == '5')
			support450 = true;

		if(support450)
			break;
	}
	
	if(support450)
	{
		DebugData.quadoverdraw420 = false;

		string glsl = "#version 450 core\n\n";
		glsl += "#define RENDERDOC_QuadOverdrawPS\n\n";
		glsl += GetEmbeddedResource(quadoverdraw_frag);
		DebugData.quadoverdrawFSProg = CreateShaderProgram(NULL, glsl.c_str());

		glsl = "#version 420 core\n\n";
		glsl += "#define RENDERDOC_QOResolvePS\n\n";
		glsl += GetEmbeddedResource(quadoverdraw_frag);
		DebugData.quadoverdrawResolveProg = CreateShaderProgram(blitvsSource.c_str(), glsl.c_str());
	}
	else
	{
		DebugData.quadoverdraw420 = true;

		string glsl = "#version 420 core\n\n";
		glsl += "#define RENDERDOC_QuadOverdrawPS\n\n";
		glsl += "#define dFdxFine dFdx\n\n"; // dFdx fine functions not available before GLSL 450
		glsl += "#define dFdyFine dFdy\n\n"; // use normal dFdx, which might be coarse, so won't show quad overdraw properly
		glsl += GetEmbeddedResource(quadoverdraw_frag);
		DebugData.quadoverdrawFSProg = CreateShaderProgram(NULL, glsl.c_str());

		glsl = "#version 420 core\n\n";
		glsl += "#define RENDERDOC_QOResolvePS\n\n";
		glsl += GetEmbeddedResource(quadoverdraw_frag);
		DebugData.quadoverdrawResolveProg = CreateShaderProgram(blitvsSource.c_str(), glsl.c_str());
	}
	
	string checkerfs = GetEmbeddedResource(checkerboard_frag);
	
	DebugData.checkerProg = CreateShaderProgram(blitvsSource.c_str(), checkerfs.c_str());

	string genericvsSource = GetEmbeddedResource(generic_vert);
	string genericfsSource = GetEmbeddedResource(generic_frag);

	DebugData.genericProg = CreateShaderProgram(genericvsSource.c_str(), genericfsSource.c_str());
	DebugData.genericFSProg = CreateShaderProgram(NULL, genericfsSource.c_str());
	
	string meshvs = GetEmbeddedResource(mesh_vert);
	string meshgs = GetEmbeddedResource(mesh_geom);
	string meshfs = GetEmbeddedResource(mesh_frag);
	meshfs = glslheader + meshfs;
	
	DebugData.meshProg = CreateShaderProgram(meshvs.c_str(), meshfs.c_str());
	DebugData.meshgsProg = CreateShaderProgram(meshvs.c_str(), meshfs.c_str(), meshgs.c_str());
	
	void *ctx = gl.GetCtx();
	gl.glGenProgramPipelines(1, &DebugData.texDisplayPipe);

	{
		float data[] = {
			0.0f, -1.0f, 0.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f,
			1.0f,  0.0f, 0.0f, 1.0f,
			0.0f,  0.0f, 0.0f, 1.0f,
			0.0f, -1.1f, 0.0f, 1.0f,
		};

		gl.glGenBuffers(1, &DebugData.outlineStripVB);
		gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.outlineStripVB);
		gl.glNamedBufferDataEXT(DebugData.outlineStripVB, sizeof(data), data, eGL_STATIC_DRAW);
		
    gl.glGenVertexArrays(1, &DebugData.outlineStripVAO);
    gl.glBindVertexArray(DebugData.outlineStripVAO);
		
		gl.glVertexAttribPointer(0, 4, eGL_FLOAT, false, 0, (const void *)0);
		gl.glEnableVertexAttribArray(0);
	}

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
	for(size_t i=0; i < ARRAY_COUNT(DebugData.UBOs); i++)
	{
		gl.glBindBuffer(eGL_UNIFORM_BUFFER, DebugData.UBOs[i]);
		gl.glNamedBufferDataEXT(DebugData.UBOs[i], 512, NULL, eGL_DYNAMIC_DRAW);
		RDCCOMPILE_ASSERT(sizeof(texdisplay) < 512, "texdisplay UBO too large");
		RDCCOMPILE_ASSERT(sizeof(FontUniforms) < 512, "texdisplay UBO too large");
		RDCCOMPILE_ASSERT(sizeof(HistogramCBufferData) < 512, "texdisplay UBO too large");
	}

	DebugData.overlayTexWidth = DebugData.overlayTexHeight = 0;
	DebugData.overlayTex = DebugData.overlayFBO = 0;
	
	gl.glGenFramebuffers(1, &DebugData.customFBO);
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.customFBO);
	DebugData.customTex = 0;

	gl.glGenFramebuffers(1, &DebugData.pickPixelFBO);
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.pickPixelFBO);

	gl.glGenTextures(1, &DebugData.pickPixelTex);
	gl.glBindTexture(eGL_TEXTURE_2D, DebugData.pickPixelTex);
	
	gl.glTextureStorage2DEXT(DebugData.pickPixelTex, eGL_TEXTURE_2D, 1, eGL_RGBA32F, 1, 1); 
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.pickPixelTex, 0);

	gl.glGenVertexArrays(1, &DebugData.emptyVAO);
	gl.glBindVertexArray(DebugData.emptyVAO);
	
	// histogram/minmax data
	{
		string histogramglsl = GetEmbeddedResource(texsample_h);
		histogramglsl += GetEmbeddedResource(histogram_comp);

		RDCEraseEl(DebugData.minmaxTileProgram);
		RDCEraseEl(DebugData.histogramProgram);
		RDCEraseEl(DebugData.minmaxResultProgram);

		RDCCOMPILE_ASSERT(ARRAY_COUNT(DebugData.minmaxTileProgram) >= (TEXDISPLAY_SINT_TEX|TEXDISPLAY_TYPEMASK)+1, "not enough programs");

		for(int t=1; t <= RESTYPE_TEXTYPEMAX; t++)
		{
			// float, uint, sint
			for(int i=0; i < 3; i++)
			{
				int idx = t;
				if(i == 1) idx |= TEXDISPLAY_UINT_TEX;
				if(i == 2) idx |= TEXDISPLAY_SINT_TEX;

				{
					string glsl = glslheader;
					glsl += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
					glsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
					glsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
					glsl += string("#define RENDERDOC_TileMinMaxCS 1\n");
					glsl += histogramglsl;

					DebugData.minmaxTileProgram[idx] = CreateCShaderProgram(glsl.c_str());
				}

				{
					string glsl = glslheader;
					glsl += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
					glsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
					glsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
					glsl += string("#define RENDERDOC_HistogramCS 1\n");
					glsl += histogramglsl;

					DebugData.histogramProgram[idx] = CreateCShaderProgram(glsl.c_str());
				}

				if(t == 1)
				{
					string glsl = glslheader;
					glsl += string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
					glsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
					glsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
					glsl += string("#define RENDERDOC_ResultMinMaxCS 1\n");
					glsl += histogramglsl;

					DebugData.minmaxResultProgram[i] = CreateCShaderProgram(glsl.c_str());
				}
			}
		}

		gl.glGenBuffers(1, &DebugData.minmaxTileResult);
		gl.glGenBuffers(1, &DebugData.minmaxResult);
		gl.glGenBuffers(1, &DebugData.histogramBuf);
		
		const uint32_t maxTexDim = 16384;
		const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK;
		const uint32_t maxBlocksNeeded = (maxTexDim*maxTexDim)/(blockPixSize*blockPixSize);

		const size_t byteSize = 2*sizeof(Vec4f)*HGRAM_TILES_PER_BLOCK*HGRAM_TILES_PER_BLOCK*maxBlocksNeeded;

		gl.glNamedBufferStorageEXT(DebugData.minmaxTileResult, byteSize, NULL, 0);
		gl.glNamedBufferStorageEXT(DebugData.minmaxResult, sizeof(Vec4f)*2, NULL, GL_MAP_READ_BIT);
		gl.glNamedBufferStorageEXT(DebugData.histogramBuf, sizeof(uint32_t)*HGRAM_NUM_BUCKETS, NULL, GL_MAP_READ_BIT);
	}
	
	{
		string glsl = "#version 420 core\n\n#define MS2Array main\n\n";
		glsl += GetEmbeddedResource(arraymscopy_comp);

		DebugData.MS2Array = CreateCShaderProgram(glsl.c_str());
		
		glsl = "#version 420 core\n\n#define Array2MS main\n\n";
		glsl += GetEmbeddedResource(arraymscopy_comp);

		DebugData.Array2MS = CreateCShaderProgram(glsl.c_str());
	}

	gl.glGenVertexArrays(1, &DebugData.meshVAO);
	gl.glBindVertexArray(DebugData.meshVAO);
	
	gl.glGenBuffers(1, &DebugData.axisFrustumBuffer);
	gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.axisFrustumBuffer);
	
	Vec3f TLN = Vec3f(-1.0f,  1.0f, 0.0f); // TopLeftNear, etc...
	Vec3f TRN = Vec3f( 1.0f,  1.0f, 0.0f);
	Vec3f BLN = Vec3f(-1.0f, -1.0f, 0.0f);
	Vec3f BRN = Vec3f( 1.0f, -1.0f, 0.0f);

	Vec3f TLF = Vec3f(-1.0f,  1.0f, 1.0f);
	Vec3f TRF = Vec3f( 1.0f,  1.0f, 1.0f);
	Vec3f BLF = Vec3f(-1.0f, -1.0f, 1.0f);
	Vec3f BRF = Vec3f( 1.0f, -1.0f, 1.0f);

	Vec3f axisFrustum[] = {
		// axis marker vertices
		Vec3f(0.0f, 0.0f, 0.0f),
		Vec3f(1.0f, 0.0f, 0.0f),
		Vec3f(0.0f, 0.0f, 0.0f),
		Vec3f(0.0f, 1.0f, 0.0f),
		Vec3f(0.0f, 0.0f, 0.0f),
		Vec3f(0.0f, 0.0f, 1.0f),

		// frustum vertices
		TLN, TRN,
		TRN, BRN,
		BRN, BLN,
		BLN, TLN,

		TLN, TLF,
		TRN, TRF,
		BLN, BLF,
		BRN, BRF,

		TLF, TRF,
		TRF, BRF,
		BRF, BLF,
		BLF, TLF,
	};

	gl.glNamedBufferStorageEXT(DebugData.axisFrustumBuffer, sizeof(axisFrustum), axisFrustum, 0);
	
	gl.glGenVertexArrays(1, &DebugData.axisVAO);
	gl.glBindVertexArray(DebugData.axisVAO);
	gl.glVertexAttribPointer(0, 3, eGL_FLOAT, GL_FALSE, sizeof(Vec3f), NULL);
	gl.glEnableVertexAttribArray(0);
	
	gl.glGenVertexArrays(1, &DebugData.frustumVAO);
	gl.glBindVertexArray(DebugData.frustumVAO);
	gl.glVertexAttribPointer(0, 3, eGL_FLOAT, GL_FALSE, sizeof(Vec3f), (const void *)( sizeof(Vec3f) * 6 ));
	gl.glEnableVertexAttribArray(0);
	
	gl.glGenVertexArrays(1, &DebugData.triHighlightVAO);
	gl.glBindVertexArray(DebugData.triHighlightVAO);
	
	gl.glGenBuffers(1, &DebugData.triHighlightBuffer);
	gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
	
	gl.glNamedBufferStorageEXT(DebugData.triHighlightBuffer, sizeof(Vec4f)*16, NULL, GL_DYNAMIC_STORAGE_BIT);
	
	gl.glVertexAttribPointer(0, 4, eGL_FLOAT, GL_FALSE, sizeof(Vec4f), NULL);
	gl.glEnableVertexAttribArray(0);
	
	DebugData.replayQuadProg = CreateShaderProgram(blitvsSource.c_str(), genericfsSource.c_str());

	MakeCurrentReplayContext(&m_ReplayCtx);

	// these below need to be made on the replay context, as they are context-specific (not shared)
	// and will be used on the replay context.
	gl.glGenProgramPipelines(1, &DebugData.overlayPipe);

	gl.glGenTransformFeedbacks(1, &DebugData.feedbackObj);
	gl.glGenBuffers(1, &DebugData.feedbackBuffer);
	gl.glGenQueries(1, &DebugData.feedbackQuery);

	gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, DebugData.feedbackObj);
	gl.glBindBuffer(eGL_TRANSFORM_FEEDBACK_BUFFER, DebugData.feedbackBuffer);
	gl.glNamedBufferStorageEXT(DebugData.feedbackBuffer, 32*1024*1024, NULL, GL_MAP_READ_BIT);
	gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);
	gl.glBindTransformFeedback(eGL_TRANSFORM_FEEDBACK, 0);
}

void GLReplay::DeleteDebugData()
{
	WrappedOpenGL &gl = *m_pDriver;

	MakeCurrentReplayContext(&m_ReplayCtx);
	
	gl.glDeleteProgramPipelines(1, &DebugData.overlayPipe);

	gl.glDeleteTransformFeedbacks(1, &DebugData.feedbackObj);
	gl.glDeleteBuffers(1, &DebugData.feedbackBuffer);
	gl.glDeleteQueries(1, &DebugData.feedbackQuery);

	MakeCurrentReplayContext(m_DebugCtx);

	for(auto it=m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
	{
		gl.glDeleteBuffers(1, &it->second.vsout.buf);
		gl.glDeleteBuffers(1, &it->second.vsout.idxBuf);
		gl.glDeleteBuffers(1, &it->second.gsout.buf);
		gl.glDeleteBuffers(1, &it->second.gsout.idxBuf);
	}

	m_PostVSData.clear();
	
	if(DebugData.overlayFBO)
	{
		gl.glDeleteFramebuffers(1, &DebugData.overlayFBO);
		gl.glDeleteTextures(1, &DebugData.overlayTex);
	}

	gl.glDeleteProgram(DebugData.blitProg);
	
	if(DebugData.quadoverdrawFSProg)
	{
		gl.glDeleteProgram(DebugData.quadoverdrawFSProg);
		gl.glDeleteProgram(DebugData.quadoverdrawResolveProg);
	}

	gl.glDeleteProgram(DebugData.texDisplayVSProg);
	for(int i=0; i < 3; i++)
		gl.glDeleteProgram(DebugData.texDisplayProg[i]);

	gl.glDeleteProgramPipelines(1, &DebugData.texDisplayPipe);

	gl.glDeleteProgram(DebugData.checkerProg);
	gl.glDeleteProgram(DebugData.genericProg);
	gl.glDeleteProgram(DebugData.genericFSProg);
	gl.glDeleteProgram(DebugData.meshProg);
	gl.glDeleteProgram(DebugData.meshgsProg);

	gl.glDeleteBuffers(1, &DebugData.outlineStripVB);
	gl.glDeleteVertexArrays(1, &DebugData.outlineStripVAO);

	gl.glDeleteSamplers(1, &DebugData.linearSampler);
	gl.glDeleteSamplers(1, &DebugData.pointSampler);
	gl.glDeleteSamplers(1, &DebugData.pointNoMipSampler);
	gl.glDeleteBuffers(ARRAY_COUNT(DebugData.UBOs), DebugData.UBOs);
	gl.glDeleteFramebuffers(1, &DebugData.pickPixelFBO);
	gl.glDeleteTextures(1, &DebugData.pickPixelTex);
	
	gl.glDeleteFramebuffers(1, &DebugData.customFBO);
	if(DebugData.customTex != 0)
		gl.glDeleteTextures(1, &DebugData.customTex);

	gl.glDeleteVertexArrays(1, &DebugData.emptyVAO);

	for(int t=1; t <= RESTYPE_TEXTYPEMAX; t++)
	{
		// float, uint, sint
		for(int i=0; i < 3; i++)
		{
			int idx = t;
			if(i == 1) idx |= TEXDISPLAY_UINT_TEX;
			if(i == 2) idx |= TEXDISPLAY_SINT_TEX;

			gl.glDeleteProgram(DebugData.minmaxTileProgram[idx]);
			gl.glDeleteProgram(DebugData.histogramProgram[idx]);

			if(t == 1)
				gl.glDeleteProgram(DebugData.minmaxResultProgram[i]);
		}
	}
	
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

	gl.glDeleteProgram(DebugData.replayQuadProg);
}

bool GLReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	if(m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
		return false;
	
	auto &texDetails = m_pDriver->m_Textures[texid];

	FetchTexture details = GetTexture(texid);

	const GLHookSet &gl = m_pDriver->GetHookset();
	
	int texSlot = 0;
	int intIdx = 0;

	bool renderbuffer = false;
	
	switch (texDetails.curType)
	{
		case eGL_RENDERBUFFER:
			texSlot = RESTYPE_TEX2D;
			renderbuffer = true;
			break;
		case eGL_TEXTURE_1D:
			texSlot = RESTYPE_TEX1D;
			break;
		default:
			RDCWARN("Unexpected texture type");
		case eGL_TEXTURE_2D:
			texSlot = RESTYPE_TEX2D;
			break;
		case eGL_TEXTURE_2D_MULTISAMPLE:
			texSlot = RESTYPE_TEX2DMS;
			break;
		case eGL_TEXTURE_RECTANGLE:
			texSlot = RESTYPE_TEXRECT;
			break;
		case eGL_TEXTURE_BUFFER:
			texSlot = RESTYPE_TEXBUFFER;
			break;
		case eGL_TEXTURE_3D:
			texSlot = RESTYPE_TEX3D;
			break;
		case eGL_TEXTURE_CUBE_MAP:
			texSlot = RESTYPE_TEXCUBE;
			break;
		case eGL_TEXTURE_1D_ARRAY:
			texSlot = RESTYPE_TEX1DARRAY;
			break;
		case eGL_TEXTURE_2D_ARRAY:
			texSlot = RESTYPE_TEX2DARRAY;
			break;
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			texSlot = RESTYPE_TEXCUBEARRAY;
			break;
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
		gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curDrawFBO);
		gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint*)&curReadFBO);
		
		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

		gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height,
		                     0, 0, texDetails.width, texDetails.height,
												 GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,
												 eGL_NEAREST);

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

		texname = texDetails.renderbufferReadTex;
		target = eGL_TEXTURE_2D;
	}
	
	MakeCurrentReplayContext(m_DebugCtx);

	gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
	HistogramCBufferData *cdata = (HistogramCBufferData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramCBufferData),
	                                                                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width>>mip, 1U);
	cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height>>mip, 1U);
	cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth>>mip, 1U);
	cdata->HistogramSlice = (float)sliceFace;
	cdata->HistogramMip = (int)mip;
	cdata->HistogramNumSamples = texDetails.samples;
	cdata->HistogramSample = (int)RDCCLAMP(sample, 0U, details.msSamp-1);
	if(sample == ~0U) cdata->HistogramSample = -int(details.msSamp);
	cdata->HistogramMin = 0.0f;
	cdata->HistogramMax = 1.0f;
	cdata->HistogramChannels = 0xf;
	
	int progIdx = texSlot;

	if(details.format.compType == eCompType_UInt)
	{
		progIdx |= TEXDISPLAY_UINT_TEX;
		intIdx = 1;
	}
	if(details.format.compType == eCompType_SInt)
	{
		progIdx |= TEXDISPLAY_SINT_TEX;
		intIdx = 2;
	}
	
	if(details.dimension == 3)
		cdata->HistogramSlice = float(sliceFace)/float(details.depth);
	
	int blocksX = (int)ceil(cdata->HistogramTextureResolution.x/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(cdata->HistogramTextureResolution.y/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

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

bool GLReplay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
{
	if(minval >= maxval) return false;

	if(m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
		return false;

	auto &texDetails = m_pDriver->m_Textures[texid];

	FetchTexture details = GetTexture(texid);

	const GLHookSet &gl = m_pDriver->GetHookset();

	int texSlot = 0;
	int intIdx = 0;

	bool renderbuffer = false;

	switch (texDetails.curType)
	{
		case eGL_RENDERBUFFER:
			texSlot = RESTYPE_TEX2D;
			renderbuffer = true;
			break;
		case eGL_TEXTURE_1D:
			texSlot = RESTYPE_TEX1D;
			break;
		default:
			RDCWARN("Unexpected texture type");
		case eGL_TEXTURE_2D:
			texSlot = RESTYPE_TEX2D;
			break;
		case eGL_TEXTURE_2D_MULTISAMPLE:
			texSlot = RESTYPE_TEX2DMS;
			break;
		case eGL_TEXTURE_RECTANGLE:
			texSlot = RESTYPE_TEXRECT;
			break;
		case eGL_TEXTURE_BUFFER:
			texSlot = RESTYPE_TEXBUFFER;
			break;
		case eGL_TEXTURE_3D:
			texSlot = RESTYPE_TEX3D;
			break;
		case eGL_TEXTURE_CUBE_MAP:
			texSlot = RESTYPE_TEXCUBE;
			break;
		case eGL_TEXTURE_1D_ARRAY:
			texSlot = RESTYPE_TEX1DARRAY;
			break;
		case eGL_TEXTURE_2D_ARRAY:
			texSlot = RESTYPE_TEX2DARRAY;
			break;
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			texSlot = RESTYPE_TEXCUBEARRAY;
			break;
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
		gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curDrawFBO);
		gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint*)&curReadFBO);
		
		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

		gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height,
		                     0, 0, texDetails.width, texDetails.height,
												 GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,
												 eGL_NEAREST);

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

		texname = texDetails.renderbufferReadTex;
		target = eGL_TEXTURE_2D;
	}
	
	MakeCurrentReplayContext(m_DebugCtx);

	gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
	HistogramCBufferData *cdata = (HistogramCBufferData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramCBufferData),
	                                                                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width>>mip, 1U);
	cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height>>mip, 1U);
	cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth>>mip, 1U);
	cdata->HistogramSlice = (float)sliceFace;
	cdata->HistogramMip = mip;
	cdata->HistogramNumSamples = texDetails.samples;
	cdata->HistogramSample = (int)RDCCLAMP(sample, 0U, details.msSamp-1);
	if(sample == ~0U) cdata->HistogramSample = -int(details.msSamp);
	cdata->HistogramMin = minval;
	cdata->HistogramMax = maxval;
	cdata->HistogramChannels = 0;
	if(channels[0]) cdata->HistogramChannels |= 0x1;
	if(channels[1]) cdata->HistogramChannels |= 0x2;
	if(channels[2]) cdata->HistogramChannels |= 0x4;
	if(channels[3]) cdata->HistogramChannels |= 0x8;
	cdata->HistogramFlags = 0;

	int progIdx = texSlot;

	if(details.format.compType == eCompType_UInt)
	{
		progIdx |= TEXDISPLAY_UINT_TEX;
		intIdx = 1;
	}
	if(details.format.compType == eCompType_SInt)
	{
		progIdx |= TEXDISPLAY_SINT_TEX;
		intIdx = 2;
	}

	if(details.dimension == 3)
		cdata->HistogramSlice = float(sliceFace)/float(details.depth);

	int blocksX = (int)ceil(cdata->HistogramTextureResolution.x/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(cdata->HistogramTextureResolution.y/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

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
	gl.glClearBufferData(eGL_SHADER_STORAGE_BUFFER, eGL_R32UI, eGL_RED, eGL_UNSIGNED_INT, &zero);

	gl.glUseProgram(DebugData.histogramProgram[progIdx]);
	gl.glDispatchCompute(blocksX, blocksY, 1);

	gl.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	histogram.clear();
	histogram.resize(HGRAM_NUM_BUCKETS);

	gl.glBindBuffer(eGL_COPY_READ_BUFFER, DebugData.histogramBuf);
	gl.glGetBufferSubData(eGL_COPY_READ_BUFFER, 0, sizeof(uint32_t)*HGRAM_NUM_BUCKETS, &histogram[0]);

	if(maxlevel >= 0)
		gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

	return true;
}

void GLReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4])
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
	texDisplay.rawoutput = true;
	texDisplay.offx = -float(x);
	texDisplay.offy = -float(y);

	RenderTextureInternal(texDisplay, false);

	gl.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixel);

	{
		auto &texDetails = m_pDriver->m_Textures[texture];

		// need to read stencil separately as GL can't read both depth and stencil
		// at the same time.
		if(texDetails.internalFormat == eGL_DEPTH24_STENCIL8 ||
			texDetails.internalFormat == eGL_DEPTH32F_STENCIL8)
		{
			texDisplay.Red = texDisplay.Blue = texDisplay.Alpha = false;

			RenderTextureInternal(texDisplay, false);
			
			uint32_t stencilpixel[4];
			gl.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)stencilpixel);

			pixel[1] = float(stencilpixel[1])/255.0f;
		}
	}
}

void GLReplay::CopyTex2DMSToArray(GLuint destArray, GLuint srcMS, GLint width, GLint height, GLint arraySize, GLint samples, GLenum intFormat)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	GLRenderState rs(&gl.GetHookset(), NULL, READING);
	rs.FetchState(m_pDriver->GetCtx(), m_pDriver);
	
	GLenum viewClass;
	gl.glGetInternalformativ(eGL_TEXTURE_2D_ARRAY, intFormat, eGL_VIEW_COMPATIBILITY_CLASS, sizeof(GLenum), (GLint *)&viewClass);

	GLenum fmt = eGL_R32UI;
	     if(viewClass == eGL_VIEW_CLASS_8_BITS)   fmt = eGL_R8UI;
	else if(viewClass == eGL_VIEW_CLASS_16_BITS)  fmt = eGL_R16UI;
	else if(viewClass == eGL_VIEW_CLASS_24_BITS)  fmt = eGL_RGB8UI;
	else if(viewClass == eGL_VIEW_CLASS_32_BITS)  fmt = eGL_RGBA8UI;
	else if(viewClass == eGL_VIEW_CLASS_48_BITS)  fmt = eGL_RGB16UI;
	else if(viewClass == eGL_VIEW_CLASS_64_BITS)  fmt = eGL_RG32UI;
	else if(viewClass == eGL_VIEW_CLASS_96_BITS)  fmt = eGL_RGB32UI;
	else if(viewClass == eGL_VIEW_CLASS_128_BITS) fmt = eGL_RGBA32UI;

	GLuint texs[2];
	gl.glGenTextures(2, texs);
	gl.glTextureView(texs[0], eGL_TEXTURE_2D_ARRAY, destArray, fmt, 0, 1, 0, arraySize*samples);
	gl.glTextureView(texs[1], eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, srcMS, fmt, 0, 1, 0, arraySize);

	gl.glBindImageTexture(0, texs[0], 0, GL_TRUE, 0, eGL_WRITE_ONLY, fmt);
	gl.glActiveTexture(eGL_TEXTURE0);
	gl.glBindTexture(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, texs[1]);
	gl.glBindSampler(0, DebugData.pointNoMipSampler);
	gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_BASE_LEVEL, 0);
	gl.glTexParameteri(eGL_TEXTURE_2D_MULTISAMPLE_ARRAY, eGL_TEXTURE_MAX_LEVEL, 1);

	gl.glUseProgram(DebugData.MS2Array);
	
	GLint loc = gl.glGetUniformLocation(DebugData.MS2Array, "numMultiSamples");
	gl.glUniform1i(loc, samples);

	gl.glDispatchCompute((GLuint)width, (GLuint)height, GLuint(arraySize*samples));
	gl.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	gl.glDeleteTextures(2, texs);

	rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);
}

bool GLReplay::RenderTexture(TextureDisplay cfg)
{
	return RenderTextureInternal(cfg, true);
}

bool GLReplay::RenderTextureInternal(TextureDisplay cfg, bool blendAlpha)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	auto &texDetails = m_pDriver->m_Textures[cfg.texid];

	if(texDetails.internalFormat == eGL_NONE)
		return false;

	bool renderbuffer = false;

	int intIdx = 0;

	int resType;
	switch (texDetails.curType)
	{
		case eGL_RENDERBUFFER:
			resType = RESTYPE_TEX2D;
			renderbuffer = true;
			break;
		case eGL_TEXTURE_1D:
			resType = RESTYPE_TEX1D;
			break;
		default:
			RDCWARN("Unexpected texture type");
		case eGL_TEXTURE_2D:
			resType = RESTYPE_TEX2D;
			break;
		case eGL_TEXTURE_2D_MULTISAMPLE:
			resType = RESTYPE_TEX2DMS;
			break;
		case eGL_TEXTURE_RECTANGLE:
			resType = RESTYPE_TEXRECT;
			break;
		case eGL_TEXTURE_BUFFER:
			resType = RESTYPE_TEXBUFFER;
			break;
		case eGL_TEXTURE_3D:
			resType = RESTYPE_TEX3D;
			break;
		case eGL_TEXTURE_CUBE_MAP:
			resType = RESTYPE_TEXCUBE;
			break;
		case eGL_TEXTURE_1D_ARRAY:
			resType = RESTYPE_TEX1DARRAY;
			break;
		case eGL_TEXTURE_2D_ARRAY:
			resType = RESTYPE_TEX2DARRAY;
			break;
		case eGL_TEXTURE_CUBE_MAP_ARRAY:
			resType = RESTYPE_TEXCUBEARRAY;
			break;
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
		gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curDrawFBO);
		gl.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, (GLint*)&curReadFBO);
		
		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, texDetails.renderbufferFBOs[1]);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, texDetails.renderbufferFBOs[0]);

		gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height,
		                     0, 0, texDetails.width, texDetails.height,
												 GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT,
												 eGL_NEAREST);

		gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

		texname = texDetails.renderbufferReadTex;
		target = eGL_TEXTURE_2D;
	}
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	RDCGLenum dsTexMode = eGL_NONE;
	if(IsDepthStencilFormat(texDetails.internalFormat))
	{
		if (!cfg.Red && cfg.Green)
		{
			dsTexMode = eGL_STENCIL_INDEX;

			// Stencil texture sampling is not normalized in OpenGL
			intIdx = 1;
			float rangeScale;
			switch (texDetails.internalFormat)
			{
				case eGL_STENCIL_INDEX1:
					rangeScale = 1.0f;
					break;
				case eGL_STENCIL_INDEX4:
					rangeScale = 16.0f;
					break;
				default:
					RDCWARN("Unexpected raw format for stencil visualization");
				case eGL_DEPTH24_STENCIL8:
				case eGL_DEPTH32F_STENCIL8:
				case eGL_STENCIL_INDEX8:
					rangeScale = 256.0f;
					break;
				case eGL_STENCIL_INDEX16:
					rangeScale = 65536.0f;
					break;
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
	gl.glUseProgramStages(DebugData.texDisplayPipe, eGL_FRAGMENT_SHADER_BIT, DebugData.texDisplayProg[intIdx]);

	if(cfg.CustomShader != ResourceId() && gl.GetResourceManager()->HasCurrentResource(cfg.CustomShader))
	{
		GLuint customProg = gl.GetResourceManager()->GetCurrentResource(cfg.CustomShader).name;
		gl.glUseProgramStages(DebugData.texDisplayPipe, eGL_FRAGMENT_SHADER_BIT, customProg);

		GLint loc = -1;

		loc = gl.glGetUniformLocation(customProg, "RENDERDOC_TexDim");
		if(loc >= 0)
			gl.glProgramUniform4ui(customProg, loc, texDetails.width, texDetails.height, texDetails.depth, m_CachedTextures[cfg.texid].mips);

		loc = gl.glGetUniformLocation(customProg, "RENDERDOC_SelectedMip");
		if(loc >= 0)
			gl.glProgramUniform1ui(customProg, loc, cfg.mip);

		loc = gl.glGetUniformLocation(customProg, "RENDERDOC_TextureType");
		if(loc >= 0)
			gl.glProgramUniform1ui(customProg, loc, resType);
	}
	gl.glBindProgramPipeline(DebugData.texDisplayPipe);

	gl.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + resType));
	gl.glBindTexture(target, texname);

	GLint origDSTexMode = eGL_DEPTH_COMPONENT;
	if (dsTexMode != eGL_NONE)
	{
		gl.glGetTexParameteriv(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, &origDSTexMode);
		gl.glTexParameteri(target, eGL_DEPTH_STENCIL_TEXTURE_MODE, dsTexMode);
	}

	int maxlevel = -1;

	int clampmaxlevel = 0;
	if(cfg.texid != DebugData.CustomShaderTexID)
		clampmaxlevel = m_CachedTextures[cfg.texid].mips - 1;
	
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

	if(cfg.mip == 0 && cfg.scale < 1.0f && dsTexMode == eGL_NONE && resType != RESTYPE_TEXBUFFER && resType != RESTYPE_TEXRECT)
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

	texdisplay *ubo = (texdisplay *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(texdisplay), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
	
	float x = cfg.offx;
	float y = cfg.offy;
	
	ubo->Position.x = x;
	ubo->Position.y = y;
	ubo->Scale = cfg.scale;
	
	if(cfg.scale <= 0.0f)
	{
		float xscale = DebugData.outWidth/float(tex_x);
		float yscale = DebugData.outHeight/float(tex_y);

		ubo->Scale = RDCMIN(xscale, yscale);

		if(yscale > xscale)
		{
			ubo->Position.x = 0;
			ubo->Position.y = (DebugData.outHeight-(tex_y*ubo->Scale) )*0.5f;
		}
		else
		{
			ubo->Position.y = 0;
			ubo->Position.x = (DebugData.outWidth-(tex_x*ubo->Scale) )*0.5f;
		}
	}

	ubo->HDRMul = cfg.HDRMul;

	ubo->FlipY = cfg.FlipY ? 1 : 0;
	
	if(cfg.rangemax <= cfg.rangemin) cfg.rangemax += 0.00001f;

	if (dsTexMode == eGL_NONE)
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
	ubo->InverseRangeSize = 1.0f/(cfg.rangemax-cfg.rangemin);
	
	ubo->MipLevel = (float)cfg.mip;
	if(texDetails.curType != eGL_TEXTURE_3D)
		ubo->Slice = (float)cfg.sliceFace;
	else
		ubo->Slice = (float)(cfg.sliceFace>>cfg.mip);

	ubo->OutputDisplayFormat = resType;
	
	if(cfg.overlay == eTexOverlay_NaN)
		ubo->OutputDisplayFormat |= TEXDISPLAY_NANS;

	if(cfg.overlay == eTexOverlay_Clipping)
		ubo->OutputDisplayFormat |= TEXDISPLAY_CLIPPING;
	
	if(!IsSRGBFormat(texDetails.internalFormat) && cfg.linearDisplayAsGamma)
		ubo->OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;

	ubo->RawOutput = cfg.rawoutput ? 1 : 0;

	ubo->TextureResolutionPS.x = float(tex_x);
	ubo->TextureResolutionPS.y = float(tex_y);
	ubo->TextureResolutionPS.z = float(tex_z);

	float mipScale = float(1<<cfg.mip);

	ubo->Scale *= mipScale;
	ubo->TextureResolutionPS.x /= mipScale;
	ubo->TextureResolutionPS.y /= mipScale;
	ubo->TextureResolutionPS.z /= mipScale;

	ubo->OutputRes.x = DebugData.outWidth;
	ubo->OutputRes.y = DebugData.outHeight;

	ubo->NumSamples = texDetails.samples;
	ubo->SampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, (uint32_t)texDetails.samples-1);

	// hacky resolve
	if(cfg.sampleIdx == ~0U) ubo->SampleIdx = -1;

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
	
	if(maxlevel >= 0)
		gl.glTextureParameterivEXT(texname, target, eGL_TEXTURE_MAX_LEVEL, (GLint *)&maxlevel);

	gl.glBindSampler(0, 0);

	if (dsTexMode != eGL_NONE)
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
	
	Vec4f *ubo = (Vec4f *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(Vec4f)*2, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	ubo[0] = Vec4f(light.x, light.y, light.z, 1.0f);
	ubo[1] = Vec4f(dark.x, dark.y, dark.z, 1.0f);
	
	gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);
	
	gl.glBindVertexArray(DebugData.emptyVAO);
	gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
}

void GLReplay::RenderHighlightBox(float w, float h, float scale)
{
	MakeCurrentReplayContext(m_DebugCtx);
	
	const float xpixdim = 2.0f/w;
	const float ypixdim = 2.0f/h;
	
	const float xdim = scale*xpixdim;
	const float ydim = scale*ypixdim;

	WrappedOpenGL &gl = *m_pDriver;
	
	gl.glUseProgram(DebugData.genericProg);

	GLint offsetLoc = gl.glGetUniformLocation(DebugData.genericProg, "RENDERDOC_GenericVS_Offset");
	GLint scaleLoc = gl.glGetUniformLocation(DebugData.genericProg, "RENDERDOC_GenericVS_Scale");
	GLint colLoc = gl.glGetUniformLocation(DebugData.genericProg, "RENDERDOC_GenericFS_Color");
	
	Vec4f offsetVal(0.0f, 0.0f, 0.0f, 0.0f);
	Vec4f scaleVal(xdim, ydim, 1.0f, 1.0f);
	Vec4f colVal(1.0f, 1.0f, 1.0f, 1.0f);

	gl.glUniform4fv(offsetLoc, 1, &offsetVal.x);
	gl.glUniform4fv(scaleLoc, 1, &scaleVal.x);
	gl.glUniform4fv(colLoc, 1, &colVal.x);

	gl.glDisable(eGL_DEPTH_TEST);
	
	gl.glBindVertexArray(DebugData.outlineStripVAO);
	gl.glDrawArrays(eGL_LINE_STRIP, 0, 5);

	offsetVal = Vec4f(-xpixdim, ypixdim, 0.0f, 0.0f);
	scaleVal = Vec4f(xdim+xpixdim*2, ydim+ypixdim*2, 1.0f, 1.0f);
	colVal = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
	
	gl.glUniform4fv(offsetLoc, 1, &offsetVal.x);
	gl.glUniform4fv(scaleLoc, 1, &scaleVal.x);
	gl.glUniform4fv(colLoc, 1, &colVal.x);

	gl.glBindVertexArray(DebugData.outlineStripVAO);
	gl.glDrawArrays(eGL_LINE_STRIP, 0, 5);
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

			for(size_t i=0; i < 4; i++)
			{
				if(pipeDetails.stageShaders[i] != ResourceId())
				{
					GLuint progsrc = m_pDriver->GetResourceManager()->GetCurrentResource(pipeDetails.stagePrograms[i]).name;
					GLuint progdst = m_pDriver->m_Shaders[pipeDetails.stageShaders[i]].prog;

					gl.glUseProgramStages(DebugData.overlayPipe, ShaderBit(i), progdst);

					CopyProgramUniforms(gl.GetHookset(), progsrc, progdst);

					if(i == 0)
						CopyProgramAttribBindings(gl.GetHookset(), progsrc, progdst, GetShader(pipeDetails.stageShaders[i]));
				}
			}
		}
	}
	else
	{
		auto &progDetails = m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetID(ProgramRes(ctx, Program))];

		for(size_t i=0; i < 4; i++)
		{
			if(progDetails.stageShaders[i] != ResourceId())
			{
				GLuint progdst = m_pDriver->m_Shaders[progDetails.stageShaders[i]].prog;

				gl.glUseProgramStages(DebugData.overlayPipe, ShaderBit(i), progdst);

				CopyProgramUniforms(gl.GetHookset(), Program, progdst);

				if(i == 0)
					CopyProgramAttribBindings(gl.GetHookset(), Program, progdst, GetShader(progDetails.stageShaders[i]));
			}
		}
	}

	// use the generic FS program by default, can be overridden for specific overlays if needed
	gl.glUseProgramStages(DebugData.overlayPipe, eGL_FRAGMENT_SHADER_BIT, fragProgram);
}

ResourceId GLReplay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(&m_ReplayCtx);

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
	SetupOverlayPipeline(rs.Program, rs.Pipeline, DebugData.genericFSProg);

	auto &texDetails = m_pDriver->m_Textures[texid];
	
	// resize (or create) the overlay texture and FBO if necessary
	if(DebugData.overlayTexWidth != texDetails.width || DebugData.overlayTexHeight != texDetails.height)
	{
		if(DebugData.overlayFBO)
		{
			gl.glDeleteFramebuffers(1, &DebugData.overlayFBO);
			gl.glDeleteTextures(1, &DebugData.overlayTex);
		}

		gl.glGenFramebuffers(1, &DebugData.overlayFBO);
		gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);

		GLuint curTex = 0;
		gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint*)&curTex);

		gl.glGenTextures(1, &DebugData.overlayTex);
		gl.glBindTexture(eGL_TEXTURE_2D, DebugData.overlayTex);

		DebugData.overlayTexWidth = texDetails.width;
		DebugData.overlayTexHeight = texDetails.height;

		gl.glTextureStorage2DEXT(DebugData.overlayTex, eGL_TEXTURE_2D, 1, eGL_RGBA16, texDetails.width, texDetails.height); 
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
		gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);
		
		gl.glBindTexture(eGL_TEXTURE_2D, curTex);
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
	gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
	gl.glDisable(eGL_DEPTH_TEST);
	gl.glDisable(eGL_STENCIL_TEST);
	gl.glStencilMask(0);

	if(overlay == eTexOverlay_NaN || overlay == eTexOverlay_Clipping)
	{
		// just need the basic texture
		float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, black);
	}
	else if(overlay == eTexOverlay_Drawcall)
	{
		float black[] = { 0.0f, 0.0f, 0.0f, 0.5f };
		gl.glClearBufferfv(eGL_COLOR, 0, black);

		GLint colLoc = gl.glGetUniformLocation(DebugData.genericFSProg, "RENDERDOC_GenericFS_Color");
		float colVal[] = { 0.8f, 0.1f, 0.8f, 1.0f };
		gl.glProgramUniform4fv(DebugData.genericFSProg, colLoc, 1, colVal);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
	}
	else if(overlay == eTexOverlay_Wireframe)
	{
		float wireCol[] = { 200.0f/255.0f, 255.0f/255.0f, 0.0f/255.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, wireCol);

		GLint colLoc = gl.glGetUniformLocation(DebugData.genericFSProg, "RENDERDOC_GenericFS_Color");
		wireCol[3] = 1.0f;
		gl.glProgramUniform4fv(DebugData.genericFSProg, colLoc, 1, wireCol);
		
		gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
	}
	else if(overlay == eTexOverlay_ViewportScissor)
	{
		float col[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, col);

		// don't need to use the existing program at all!
		gl.glUseProgram(DebugData.replayQuadProg);
		gl.glBindProgramPipeline(0);
		
		GLint colLoc = gl.glGetUniformLocation(DebugData.replayQuadProg, "RENDERDOC_GenericFS_Color");
		float viewportConsts[] = { 0.15f, 0.3f, 0.6f, 0.3f };
		gl.glUniform4fv(colLoc, 1, viewportConsts);

		gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
		
		gl.glEnablei(eGL_SCISSOR_TEST, 0);
		
		float scissorConsts[] = { 0.5f, 0.6f, 0.8f, 0.3f };
		gl.glUniform4fv(colLoc, 1, scissorConsts);

		gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
	}
	else if(overlay == eTexOverlay_DepthBoth || overlay == eTexOverlay_StencilBoth)
	{
		float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, black);
		
		GLint colLoc = gl.glGetUniformLocation(DebugData.genericFSProg, "RENDERDOC_GenericFS_Color");
		float red[] = { 1.0f, 0.0f, 0.0f, 1.0f };
		gl.glProgramUniform4fv(DebugData.genericFSProg, colLoc, 1, red);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		GLuint curDepth = 0, curStencil = 0;

		gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curDepth);
		gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO, eGL_STENCIL_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curStencil);

		GLuint depthCopy = 0, stencilCopy = 0;

		// TODO handle non-2D depth/stencil attachments and fetch slice or cubemap face
		GLint mip = 0;
		
		gl.glGetNamedFramebufferAttachmentParameterivEXT(rs.DrawFBO, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &mip);

		// create matching depth for existing FBO
		if(curDepth != 0)
		{
			GLuint curTex = 0;
			gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint*)&curTex);

			GLenum fmt;
			gl.glGetTextureLevelParameterivEXT(curDepth, eGL_TEXTURE_2D, mip, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);

			gl.glGenTextures(1, &depthCopy);
			gl.glBindTexture(eGL_TEXTURE_2D, depthCopy);
			gl.glTextureStorage2DEXT(depthCopy, eGL_TEXTURE_2D, 1, fmt, DebugData.overlayTexWidth, DebugData.overlayTexHeight);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

			gl.glBindTexture(eGL_TEXTURE_2D, curTex);
		}

		// create matching separate stencil if relevant
		if(curStencil != curDepth && curStencil != 0)
		{
			GLuint curTex = 0;
			gl.glGetIntegerv(eGL_TEXTURE_BINDING_2D, (GLint*)&curTex);

			GLenum fmt;
			gl.glGetTextureLevelParameterivEXT(curStencil, eGL_TEXTURE_2D, mip, eGL_TEXTURE_INTERNAL_FORMAT, (GLint *)&fmt);

			gl.glGenTextures(1, &stencilCopy);
			gl.glBindTexture(eGL_TEXTURE_2D, stencilCopy);
			gl.glTextureStorage2DEXT(stencilCopy, eGL_TEXTURE_2D, 1, fmt, DebugData.overlayTexWidth, DebugData.overlayTexHeight);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
			gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);

			gl.glBindTexture(eGL_TEXTURE_2D, curTex);
		}

		// bind depth/stencil to overlay FBO (currently bound to DRAW_FRAMEBUFFER)
		if(curDepth != 0 && curDepth == curStencil)
			gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, depthCopy, mip);
		else if(curDepth != 0)
			gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, depthCopy, mip);
		else if(curStencil != 0)
			gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, stencilCopy, mip);

		// bind the 'real' fbo to the read framebuffer, so we can blit from it
		gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, rs.DrawFBO);

		float green[] = { 0.0f, 1.0f, 0.0f, 1.0f };
		gl.glProgramUniform4fv(DebugData.genericFSProg, colLoc, 1, green);

		if(overlay == eTexOverlay_DepthBoth)
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
		gl.glBlitFramebuffer(0, 0, DebugData.overlayTexWidth, DebugData.overlayTexHeight,
		                     0, 0, DebugData.overlayTexWidth, DebugData.overlayTexHeight,
												 GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		// unset depth/stencil textures from overlay FBO and delete temp depth/stencil
		if(curDepth != 0 && curDepth == curStencil)
			gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
		else if(curDepth != 0)
			gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, 0, 0);
		else if(curStencil != 0)
			gl.glFramebufferTexture(eGL_DRAW_FRAMEBUFFER, eGL_STENCIL_ATTACHMENT, 0, 0);
		if(depthCopy != 0)   gl.glDeleteTextures(1, &depthCopy);
		if(stencilCopy != 0) gl.glDeleteTextures(1, &stencilCopy);
	}
	else if(overlay == eTexOverlay_BackfaceCull)
	{
		float col[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, col);
		
		col[0] = 1.0f;
		col[3] = 1.0f;

		GLint colLoc = gl.glGetUniformLocation(DebugData.genericFSProg, "RENDERDOC_GenericFS_Color");
		gl.glProgramUniform4fv(DebugData.genericFSProg, colLoc, 1, col);
		
		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
		
		// only enable cull face if it was enabled originally (otherwise
		// we just render green over the exact same area, so it shows up "passing")
		if(rs.Enabled[GLRenderState::eEnabled_CullFace])
			gl.glEnable(eGL_CULL_FACE);

		col[0] = 0.0f;
		col[1] = 1.0f;

		gl.glProgramUniform4fv(DebugData.genericFSProg, colLoc, 1, col);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
	}
	else if(overlay == eTexOverlay_QuadOverdrawDraw || overlay == eTexOverlay_QuadOverdrawPass)
	{
		if(DebugData.quadoverdraw420)
		{
			RDCWARN("Quad overdraw requires GLSL 4.50 for dFd(xy)fine, using possibly coarse dFd(xy).");
			m_pDriver->AddDebugMessage(eDbgCategory_Portability, eDbgSeverity_Medium, eDbgSource_RuntimeWarning,
				"Quad overdraw requires GLSL 4.50 for dFd(xy)fine, using possibly coarse dFd(xy).");
		}

		{
			SCOPED_TIMER("Quad Overdraw");

			float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			gl.glClearBufferfv(eGL_COLOR, 0, black);

			vector<uint32_t> events = passEvents;

			if(overlay == eTexOverlay_QuadOverdrawDraw)
				events.clear();

			events.push_back(eventID);

			if(!events.empty())
			{
				GLuint replacefbo = 0;
				GLuint quadtexs[3] = { 0 };
				gl.glGenFramebuffers(1, &replacefbo);
				gl.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);

				gl.glGenTextures(3, quadtexs);
				
				// image for quad usage
				gl.glBindTexture(eGL_TEXTURE_2D_ARRAY, quadtexs[2]);
				gl.glTextureStorage3DEXT(quadtexs[2], eGL_TEXTURE_2D_ARRAY, 1, eGL_R32UI, texDetails.width>>1, texDetails.height>>1, 4);

				// temporarily attach to FBO to clear it
				GLint zero = 0;
				gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 0);
				gl.glClearBufferiv(eGL_COLOR, 0, &zero);
				gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 1);
				gl.glClearBufferiv(eGL_COLOR, 0, &zero);
				gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 2);
				gl.glClearBufferiv(eGL_COLOR, 0, &zero);
				gl.glFramebufferTextureLayer(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[2], 0, 3);
				gl.glClearBufferiv(eGL_COLOR, 0, &zero);
				
				gl.glBindTexture(eGL_TEXTURE_2D, quadtexs[0]);
				gl.glTextureStorage2DEXT(quadtexs[0], eGL_TEXTURE_2D, 1, eGL_RGBA8, texDetails.width, texDetails.height);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
				gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, quadtexs[0], 0);

				gl.glBindTexture(eGL_TEXTURE_2D, quadtexs[1]);
				gl.glTextureStorage2DEXT(quadtexs[1], eGL_TEXTURE_2D, 1, eGL_DEPTH32F_STENCIL8, texDetails.width, texDetails.height);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
				gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
				gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, quadtexs[1], 0);

				if(overlay == eTexOverlay_QuadOverdrawPass)
					ReplayLog(frameID, 0, events[0], eReplay_WithoutDraw);
				else
					rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);
				
				GLuint lastProg = 0, lastPipe = 0;
				for(size_t i=0; i < events.size(); i++)
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
						gl.glGetIntegeri_v(eGL_IMAGE_BINDING_LEVEL, 0, (GLint*)&curimage0.level);
						gl.glGetIntegeri_v(eGL_IMAGE_BINDING_ACCESS, 0, (GLint*)&curimage0.access);
						gl.glGetIntegeri_v(eGL_IMAGE_BINDING_FORMAT, 0, (GLint*)&curimage0.format);
						gl.glGetBooleani_v(eGL_IMAGE_BINDING_LAYERED, 0, &curimage0.layered);
						if(curimage0.layered)
							gl.glGetIntegeri_v(eGL_IMAGE_BINDING_LAYER, 0, (GLint*)&curimage0.layer);
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

					lastProg = prog;
					lastPipe = pipe;
					
					gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curdrawfbo);
					gl.glBlitFramebuffer(0, 0, texDetails.width, texDetails.height,
															 0, 0, texDetails.width, texDetails.height,
															 GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, eGL_NEAREST);
					
					ReplayLog(frameID, events[i], events[i], eReplay_OnlyDraw);

					// pop the state that we messed with
					{
						gl.glBindProgramPipeline(pipe);
						gl.glUseProgram(prog);

						if(curimage0.name)
							gl.glBindImageTexture(0, curimage0.name, curimage0.level, curimage0.layered ? GL_TRUE : GL_FALSE, curimage0.layer, curimage0.access, curimage0.format);
						else
							gl.glBindImageTexture(0, 0, 0, GL_FALSE, 0, eGL_READ_ONLY, eGL_R32UI);

						gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curdrawfbo);
						gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curreadfbo);

						gl.glDepthMask(depthwritemask ? GL_TRUE : GL_FALSE);
						gl.glStencilMaskSeparate(eGL_FRONT, (GLuint)stencilfmask);
						gl.glStencilMaskSeparate(eGL_BACK, (GLuint)stencilbmask);
					}

					if(overlay == eTexOverlay_QuadOverdrawPass)
					{
						ReplayLog(frameID, events[i], events[i], eReplay_OnlyDraw);

						if(i+1 < events.size())
							ReplayLog(frameID, events[i], events[i+1], eReplay_WithoutDraw);
					}
				}

				// resolve pass
				{
					gl.glUseProgram(DebugData.quadoverdrawResolveProg);
					gl.glBindProgramPipeline(0);

					GLint rampLoc = gl.glGetUniformLocation(DebugData.quadoverdrawResolveProg, "overdrawRampColours");
					gl.glProgramUniform4fv(DebugData.quadoverdrawResolveProg, rampLoc, ARRAY_COUNT(overdrawRamp), (float *)&overdrawRamp[0].x);
					
					// modify our fbo to attach the overlay texture instead
					gl.glBindFramebuffer(eGL_FRAMEBUFFER, replacefbo);
					gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);
					gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
					
					gl.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					gl.glDisable(eGL_BLEND);
					gl.glDisable(eGL_SCISSOR_TEST);
					gl.glDepthMask(GL_FALSE);
					gl.glDisable(eGL_CULL_FACE);
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

				if(overlay == eTexOverlay_QuadOverdrawPass)
					ReplayLog(frameID, 0, eventID, eReplay_WithoutDraw);
			}
		}
	}
	else
	{
		RDCERR("Unexpected/unimplemented overlay type - should implement a placeholder overlay for all types");
	}

	rs.ApplyState(m_pDriver->GetCtx(), m_pDriver);

	return m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, DebugData.overlayTex));
}

void GLReplay::InitPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	auto idx = std::make_pair(frameID, eventID);
	if(m_PostVSData.find(idx) != m_PostVSData.end())
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
				vsRefl = GetShader(pipeDetails.stageShaders[0]);
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
				tesRefl = GetShader(pipeDetails.stageShaders[2]);
				tesProg = m_pDriver->m_Shaders[pipeDetails.stageShaders[2]].prog;
				tesProgSrc = rm->GetCurrentResource(pipeDetails.stagePrograms[2]).name;
			}
			if(pipeDetails.stageShaders[3] != ResourceId())
			{
				gsRefl = GetShader(pipeDetails.stageShaders[3]);
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
			vsRefl = GetShader(progDetails.stageShaders[0]);
			vsProg = m_pDriver->m_Shaders[progDetails.stageShaders[0]].prog;
		}
		if(progDetails.stageShaders[1] != ResourceId())
		{
			tcsProg = m_pDriver->m_Shaders[progDetails.stageShaders[1]].prog;
		}
		if(progDetails.stageShaders[2] != ResourceId())
		{
			tesRefl = GetShader(progDetails.stageShaders[2]);
			tesProg = m_pDriver->m_Shaders[progDetails.stageShaders[2]].prog;
		}
		if(progDetails.stageShaders[3] != ResourceId())
		{
			gsRefl = GetShader(progDetails.stageShaders[3]);
			gsProg = m_pDriver->m_Shaders[progDetails.stageShaders[3]].prog;
		}

		vsProgSrc = tcsProgSrc = tesProgSrc = gsProgSrc = rs.Program;
	}

	if(vsRefl == NULL)
	{
		// no vertex shader bound (no vertex processing - compute only program
		// or no program bound, for a clear etc)
		m_PostVSData[idx] = GLPostVSData();
		return;
	}

	const FetchDrawcall *drawcall = m_pDriver->GetDrawcall(frameID, eventID);

	if(drawcall->numIndices == 0)
	{
		// draw is 0 length, nothing to do
		m_PostVSData[idx] = GLPostVSData();
		return;
	}
	
	list<string> matrixVaryings; // matrices need some fixup
	vector<const char *> varyings;

	// we don't want to do any work, so just discard before rasterizing
	gl.glEnable(eGL_RASTERIZER_DISCARD);

	CopyProgramAttribBindings(gl.GetHookset(), vsProgSrc, vsProg, vsRefl);

	varyings.clear();

	uint32_t stride = 0;
	int32_t posidx = -1;

	for(int32_t i=0; i < vsRefl->OutputSig.count; i++)
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
			if(name[len-1] != '0')
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

		if(vsRefl->OutputSig[i].systemValue == eAttr_Position)
			posidx = int32_t(varyings.size())-1;

		stride += sizeof(float)*vsRefl->OutputSig[i].compCount;
	}
	
	// shift position attribute up to first, keeping order otherwise
	// the same
	if(posidx > 0)
	{
		const char *pos = varyings[posidx];
		varyings.erase(varyings.begin()+posidx);
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
	// (from g_truc gl-410-primitive-tessellation-2). On AMD the varyings are what you might expect (from
	// the PROGRAM_OUTPUT interface names reflected out): "Out.Color", "gl_Position"
	// however nvidia complains unless you use "Color", "gl_Position". This holds even if you add other
	// variables to the vertex struct.
	//
	// strangely another sample that in-lines the output block like so:
	//
	// out block
	// {
	//   vec2 Texcoord;
	// } Out;
	//
	// uses "block.Texcoord" (reflected name from PROGRAM_OUTPUT and accepted by varyings string on both
	// vendors). This is inconsistent as it's type.member not structname.member as move.
	//
	// The spec is very vague on exactly what these names should be, so I can't say which is correct
	// out of these three possibilities.
	//
	// So our 'fix' is to loop while we have problems linking with the varyings (since we know otherwise
	// linking should succeed, as we only get here with a successfully linked separable program - if it fails
	// to link, it's assigned 0 earlier) and remove any prefixes from variables seen in the link error string.
	// The error string is something like:
	// "error: Varying (named Out.Color) specified but not present in the program object."
	//
	// Yeh. Ugly. Not guaranteed to work at all, but hopefully the common case will just be a single block
	// without any nesting so this might work.
	// At least we don't have to reallocate strings all over, since the memory is
	// already owned elsewhere, we just need to modify pointers to trim prefixes. Bright side?
	
	GLint status = 0;
	bool finished = false;
	while(true)
	{
		// specify current varyings & relink
		gl.glTransformFeedbackVaryings(vsProg, (GLsizei)varyings.size(), &varyings[0], eGL_INTERLEAVED_ATTRIBS);
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
		for(size_t i=0; i < varyings.size(); i++)
		{
			if(strstr(buffer, varyings[i]))
			{
				const char *prefix_removed = strchr(varyings[i], '.');

				// does it contain a prefix?
				if(prefix_removed)
				{
					prefix_removed++; // now this is our string without the prefix

					// first check this won't cause a duplicate - if it does, we have to try something else
					bool duplicate = false;
					for(size_t j=0; j < varyings.size(); j++)
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
		m_PostVSData[idx] = GLPostVSData();
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

	// need to rebind this here because of an AMD bug that seems to ignore the buffer
	// bindings in the feedback object - or at least it errors if the default feedback
	// object has no buffers bound. Fortunately the state is still object-local so
	// we don't have to restore the buffer binding on the default feedback object.
	gl.glBindBufferBase(eGL_TRANSFORM_FEEDBACK_BUFFER, 0, DebugData.feedbackBuffer);

	GLuint idxBuf = 0;
	
	gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQuery);
	gl.glBeginTransformFeedback(eGL_POINTS);

	if((drawcall->flags & eDraw_UseIBuffer) == 0)
	{
		if(drawcall->flags & eDraw_Instanced)
			gl.glDrawArraysInstancedBaseInstance(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices,
				    drawcall->numInstances, drawcall->instanceOffset);
		else
			gl.glDrawArrays(eGL_POINTS, drawcall->vertexOffset, drawcall->numIndices);
	}
	else // drawcall is indexed
	{
		ResourceId idxId = rm->GetID(BufferRes(NULL, elArrayBuffer));

		vector<byte> idxdata = GetBufferData(idxId, drawcall->indexOffset*drawcall->indexByteWidth, drawcall->numIndices*drawcall->indexByteWidth);
		
		vector<uint32_t> indices;
		
		uint8_t  *idx8 =  (uint8_t *) &idxdata[0];
		uint16_t *idx16 = (uint16_t *)&idxdata[0];
		uint32_t *idx32 = (uint32_t *)&idxdata[0];

		// only read as many indices as were available in the buffer
		uint32_t numIndices = RDCMIN(uint32_t(idxdata.size()/drawcall->indexByteWidth), drawcall->numIndices);

		// grab all unique vertex indices referenced
		for(uint32_t i=0; i < numIndices; i++)
		{
			uint32_t i32 = 0;
			     if(drawcall->indexByteWidth == 1) i32 = uint32_t(idx8 [i]);
			else if(drawcall->indexByteWidth == 2) i32 = uint32_t(idx16[i]);
			else if(drawcall->indexByteWidth == 4) i32 =          idx32[i];

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
		map<uint32_t,size_t> indexRemap;
		for(size_t i=0; i < indices.size(); i++)
		{
			// by definition, this index will only appear once in indices[]
			indexRemap[ indices[i] ] = i;
		}
		
		// generate a temporary index buffer with our 'unique index set' indices,
		// so we can transform feedback each referenced vertex once
		GLuint indexSetBuffer = 0;
		gl.glGenBuffers(1, &indexSetBuffer);
		gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, indexSetBuffer);
		gl.glNamedBufferStorageEXT(indexSetBuffer, sizeof(uint32_t)*indices.size(), &indices[0], 0);
		
		if(drawcall->flags & eDraw_Instanced)
		{
			gl.glDrawElementsInstancedBaseVertexBaseInstance(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL,
					drawcall->numInstances, drawcall->vertexOffset, drawcall->instanceOffset);
		}
		else
		{
			gl.glDrawElementsBaseVertex(eGL_POINTS, (GLsizei)indices.size(), eGL_UNSIGNED_INT, NULL, drawcall->vertexOffset);
		}
		
		// delete the buffer, we don't need it anymore
		gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
		gl.glDeleteBuffers(1, &indexSetBuffer);
		
		// rebase existing index buffer to point from 0 onwards (which will index into our
		// stream-out'd vertex buffer)
		if(drawcall->indexByteWidth == 1)
		{
			for(uint32_t i=0; i < numIndices; i++)
				idx8[i] = uint8_t(indexRemap[ idx8[i] ]);
		}
		else if(drawcall->indexByteWidth == 2)
		{
			for(uint32_t i=0; i < numIndices; i++)
				idx16[i] = uint16_t(indexRemap[ idx16[i] ]);
		}
		else
		{
			for(uint32_t i=0; i < numIndices; i++)
				idx32[i] = uint32_t(indexRemap[ idx32[i] ]);
		}
		
		// make the index buffer that can be used to render this postvs data - the original
		// indices, repointed (since we transform feedback to the start of our feedback
		// buffer and only tightly packed unique indices).
		if(!idxdata.empty())
		{
			gl.glGenBuffers(1, &idxBuf);
			gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, idxBuf);
			gl.glNamedBufferStorageEXT(idxBuf, (GLsizeiptr)idxdata.size(), &idxdata[0], 0);
		}
		
		// restore previous element array buffer binding
		gl.glBindBuffer(eGL_ELEMENT_ARRAY_BUFFER, elArrayBuffer);
	}
	
	gl.glEndTransformFeedback();
	gl.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
	
	bool error = false;

	// this should be the same as the draw size
	GLuint primsWritten = 0;
	gl.glGetQueryObjectuiv(DebugData.feedbackQuery, eGL_QUERY_RESULT, &primsWritten);

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
		
		m_PostVSData[idx] = GLPostVSData();
		return;
	}

	// create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
	// can render from it to display previews).
	GLuint vsoutBuffer = 0;
	gl.glGenBuffers(1, &vsoutBuffer);
	gl.glBindBuffer(eGL_ARRAY_BUFFER, vsoutBuffer);
	gl.glNamedBufferStorageEXT(vsoutBuffer, stride*primsWritten, data, 0);

	byte *byteData = (byte *)data;

	float nearp = 0.1f;
	float farp = 100.0f;

	Vec4f *pos0 = (Vec4f *)byteData;

	for(GLuint i=1; posidx != -1 && i < primsWritten; i++)
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

		Vec4f *pos = (Vec4f *)(byteData + i*stride);

		if(fabs(pos->w - pos0->w) > 0.01f)
		{
			Vec2f A(pos0->w, pos0->z);
			Vec2f B(pos->w, pos->z);

			float m = (B.y-A.y)/(B.x-A.x);
			float c = B.y - B.x*m;

			if(m == 1.0f) continue;

			nearp = -c/m;
			farp = c/(1-m);

			break;
		}
	}

	gl.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);

	// store everything out to the PostVS data cache
	m_PostVSData[idx].vsin.topo = drawcall->topology;
	m_PostVSData[idx].vsout.buf = vsoutBuffer;
	m_PostVSData[idx].vsout.vertStride = stride;
	m_PostVSData[idx].vsout.nearPlane = nearp;
	m_PostVSData[idx].vsout.farPlane = farp;

	m_PostVSData[idx].vsout.useIndices = (drawcall->flags & eDraw_UseIBuffer) > 0;
	m_PostVSData[idx].vsout.numVerts = drawcall->numIndices;
	
	m_PostVSData[idx].vsout.instStride = 0;
	if(drawcall->flags & eDraw_Instanced)
		m_PostVSData[idx].vsout.instStride = (stride*primsWritten) / RDCMAX(1U, drawcall->numInstances);

	m_PostVSData[idx].vsout.idxBuf = 0;
	m_PostVSData[idx].vsout.idxByteWidth = drawcall->indexByteWidth;
	if(m_PostVSData[idx].vsout.useIndices && idxBuf)
	{
		m_PostVSData[idx].vsout.idxBuf = idxBuf;
	}

	m_PostVSData[idx].vsout.hasPosOut = posidx >= 0;

	m_PostVSData[idx].vsout.topo = drawcall->topology;

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

		for(int32_t i=0; i < lastRefl->OutputSig.count; i++)
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
				if(name[len-1] != '0')
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

			if(lastRefl->OutputSig[i].systemValue == eAttr_Position)
				posidx = int32_t(varyings.size())-1;

			stride += sizeof(float)*lastRefl->OutputSig[i].compCount;
		}
		
		// shift position attribute up to first, keeping order otherwise
		// the same
		if(posidx > 0)
		{
			const char *pos = varyings[posidx];
			varyings.erase(varyings.begin()+posidx);
			varyings.insert(varyings.begin(), pos);
		}

		// see above for the justification/explanation of this monstrosity.

		status = 0;
		finished = false;
		while(true)
		{
			// specify current varyings & relink
			gl.glTransformFeedbackVaryings(lastProg, (GLsizei)varyings.size(), &varyings[0], eGL_INTERLEAVED_ATTRIBS);
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
			for(size_t i=0; i < varyings.size(); i++)
			{
				if(strstr(buffer, varyings[i]))
				{
					const char *prefix_removed = strchr(varyings[i], '.');

					// does it contain a prefix?
					if(prefix_removed)
					{
						prefix_removed++; // now this is our string without the prefix

						// first check this won't cause a duplicate - if it does, we have to try something else
						bool duplicate = false;
						for(size_t j=0; j < varyings.size(); j++)
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

			if(lastProg == gsProg)
			{
				gl.glGetProgramiv(gsProg, eGL_GEOMETRY_OUTPUT_TYPE, (GLint *)&shaderOutMode);
				     if(shaderOutMode == eGL_TRIANGLE_STRIP) lastOutTopo = eGL_TRIANGLES;
				else if(shaderOutMode == eGL_LINE_STRIP)     lastOutTopo = eGL_LINES;
				else if(shaderOutMode == eGL_POINTS)         lastOutTopo = eGL_POINTS;
			}
			else if(lastProg == tesProg)
			{
				gl.glGetProgramiv(tesProg, eGL_TESS_GEN_MODE, (GLint *)&shaderOutMode);
				     if(shaderOutMode == eGL_QUADS)     lastOutTopo = eGL_TRIANGLES;
				else if(shaderOutMode == eGL_ISOLINES)  lastOutTopo = eGL_LINES;
				else if(shaderOutMode == eGL_TRIANGLES) lastOutTopo = eGL_TRIANGLES;
			}

			gl.glBeginQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, DebugData.feedbackQuery);
			gl.glBeginTransformFeedback(lastOutTopo);

			GLenum drawtopo = MakeGLPrimitiveTopology(drawcall->topology);

			if((drawcall->flags & eDraw_UseIBuffer) == 0)
			{
				if(drawcall->flags & eDraw_Instanced)
					gl.glDrawArraysInstancedBaseInstance(drawtopo, drawcall->vertexOffset, drawcall->numIndices,
					      drawcall->numInstances, drawcall->instanceOffset);
				else
					gl.glDrawArrays(drawtopo, drawcall->vertexOffset, drawcall->numIndices);
			}
			else // drawcall is indexed
			{
				GLenum idxType = eGL_UNSIGNED_BYTE;
				if(drawcall->indexByteWidth == 2) idxType = eGL_UNSIGNED_SHORT;
				else if(drawcall->indexByteWidth == 4) idxType = eGL_UNSIGNED_INT;

				if(drawcall->flags & eDraw_Instanced)
				{
					gl.glDrawElementsInstancedBaseVertexBaseInstance(drawtopo, drawcall->numIndices, idxType,
						(const void *)uintptr_t(drawcall->indexOffset*drawcall->indexByteWidth), drawcall->numInstances,
						drawcall->vertexOffset, drawcall->instanceOffset);
				}
				else
				{
					gl.glDrawElementsBaseVertex(drawtopo, drawcall->numIndices, idxType,
						(const void *)uintptr_t(drawcall->indexOffset*drawcall->indexByteWidth), drawcall->vertexOffset);
				}
			}
			gl.glEndTransformFeedback();
			gl.glEndQuery(eGL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);

			// this should be the same as the draw size
			primsWritten = 0;
			gl.glGetQueryObjectuiv(DebugData.feedbackQuery, eGL_QUERY_RESULT, &primsWritten);

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
				if(lastFeedbackPipe) gl.glDeleteProgramPipelines(1, &lastFeedbackPipe);

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
				if(shaderOutMode == eGL_TRIANGLES || shaderOutMode == eGL_QUADS) // query for quads returns # triangles
					m_PostVSData[idx].gsout.numVerts = primsWritten*3;
				else if(shaderOutMode == eGL_ISOLINES)
					m_PostVSData[idx].gsout.numVerts = primsWritten*2;
			}
			else if(lastProg == gsProg)
			{
				// primitive counter is the number of primitives, not vertices
				if(shaderOutMode == eGL_POINTS)
					m_PostVSData[idx].gsout.numVerts = primsWritten;
				else if(shaderOutMode == eGL_LINE_STRIP)
					m_PostVSData[idx].gsout.numVerts = primsWritten*2;
				else if(shaderOutMode == eGL_TRIANGLE_STRIP)
					m_PostVSData[idx].gsout.numVerts = primsWritten*3;
			}

			// create a buffer with this data, for future use (typed to ARRAY_BUFFER so we
			// can render from it to display previews).
			GLuint lastoutBuffer = 0;
			gl.glGenBuffers(1, &lastoutBuffer);
			gl.glBindBuffer(eGL_ARRAY_BUFFER, lastoutBuffer);
			gl.glNamedBufferStorageEXT(lastoutBuffer, stride*m_PostVSData[idx].gsout.numVerts, data, 0);

			byteData = (byte *)data;

			nearp = 0.1f;
			farp = 100.0f;

			pos0 = (Vec4f *)byteData;

			for(uint32_t i=1; posidx != -1 && i < m_PostVSData[idx].gsout.numVerts; i++)
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

				Vec4f *pos = (Vec4f *)(byteData + i*stride);

				if(fabs(pos->w - pos0->w) > 0.01f)
				{
					Vec2f A(pos0->w, pos0->z);
					Vec2f B(pos->w, pos->z);

					float m = (B.y-A.y)/(B.x-A.x);
					float c = B.y - B.x*m;

					if(m == 1.0f) continue;

					nearp = -c/m;
					farp = c/(1-m);

					break;
				}
			}

			gl.glUnmapNamedBufferEXT(DebugData.feedbackBuffer);

			// store everything out to the PostVS data cache
			m_PostVSData[idx].gsout.buf = lastoutBuffer;
			m_PostVSData[idx].gsout.instStride = 0;
			if(drawcall->flags & eDraw_Instanced)
			{
				m_PostVSData[idx].gsout.numVerts /= RDCMAX(1U, drawcall->numInstances);
				m_PostVSData[idx].gsout.instStride = stride*m_PostVSData[idx].gsout.numVerts;
			}
			m_PostVSData[idx].gsout.vertStride = stride;
			m_PostVSData[idx].gsout.nearPlane = nearp;
			m_PostVSData[idx].gsout.farPlane = farp;

			m_PostVSData[idx].gsout.useIndices = false;

			m_PostVSData[idx].gsout.hasPosOut = posidx >= 0;

			m_PostVSData[idx].gsout.idxBuf = 0;
			m_PostVSData[idx].gsout.idxByteWidth = 0;

			m_PostVSData[idx].gsout.topo = MakePrimitiveTopology(gl.GetHookset(), lastOutTopo);
		}

		// set lastProg back to no varyings, for future use
		gl.glTransformFeedbackVaryings(lastProg, 0, NULL, eGL_INTERLEAVED_ATTRIBS);
		gl.glLinkProgram(lastProg);
	}
	
	// delete temporary pipelines we made
	gl.glDeleteProgramPipelines(1, &vsFeedbackPipe);
	if(lastFeedbackPipe) gl.glDeleteProgramPipelines(1, &lastFeedbackPipe);

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

MeshFormat GLReplay::GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
	GLPostVSData postvs;
	RDCEraseEl(postvs);

	auto idx = std::make_pair(frameID, eventID);
	if(m_PostVSData.find(idx) != m_PostVSData.end())
		postvs = m_PostVSData[idx];

	GLPostVSData::StageData s = postvs.GetStage(stage);
	
	MeshFormat ret;
	
	if(s.useIndices && s.idxBuf)
		ret.idxbuf = m_pDriver->GetResourceManager()->GetID(BufferRes(NULL, s.idxBuf));
	else
		ret.idxbuf = ResourceId();
	ret.idxoffs = 0;
	ret.idxByteWidth = s.idxByteWidth;

	if(s.buf)
		ret.buf = m_pDriver->GetResourceManager()->GetID(BufferRes(NULL, s.buf));
	else
		ret.buf = ResourceId();

	ret.offset = s.instStride*instID;
	ret.stride = s.vertStride;

	ret.compCount = 4;
	ret.compByteWidth = 4;
	ret.compType = eCompType_Float;
	ret.specialFormat = eSpecial_Unknown;

	ret.showAlpha = false;

	ret.topo = s.topo;
	ret.numVerts = s.numVerts;

	ret.unproject = s.hasPosOut;
	ret.nearPlane = s.nearPlane;
	ret.farPlane = s.farPlane;

	return ret;
}

FloatVector GLReplay::InterpretVertex(byte *data, uint32_t vert, MeshDisplay cfg, byte *end, bool &valid)
{
	FloatVector ret(0.0f, 0.0f, 0.0f, 1.0f);

	if(m_HighlightCache.useidx)
	{
		if(vert >= (uint32_t)m_HighlightCache.indices.size())
		{
			valid = false;
			return ret;
		}

		vert = m_HighlightCache.indices[vert];
	}

	data += vert*cfg.position.stride;

	float *out = &ret.x;

	ResourceFormat fmt;
	fmt.compByteWidth = cfg.position.compByteWidth;
	fmt.compCount = cfg.position.compCount;
	fmt.compType = cfg.position.compType;

	if(cfg.position.specialFormat == eSpecial_R10G10B10A2)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		Vec4f v = ConvertFromR10G10B10A2(*(uint32_t *)data);
		ret.x = v.x;
		ret.y = v.y;
		ret.z = v.z;
		ret.w = v.w;
		return ret;
	}
	else if(cfg.position.specialFormat == eSpecial_R11G11B10)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		Vec3f v = ConvertFromR11G11B10(*(uint32_t *)data);
		ret.x = v.x;
		ret.y = v.y;
		ret.z = v.z;
		return ret;
	}
	else if(cfg.position.specialFormat == eSpecial_B8G8R8A8)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		fmt.compByteWidth = 1;
		fmt.compCount = 4;
		fmt.compType = eCompType_UNorm;
	}
	
	if(data + cfg.position.compCount*cfg.position.compByteWidth > end)
	{
		valid = false;
		return ret;
	}

	for(uint32_t i=0; i < cfg.position.compCount; i++)
	{
		*out = ConvertComponent(fmt, data);

		data += cfg.position.compByteWidth;
		out++;
	}

	if(cfg.position.specialFormat == eSpecial_B8G8R8A8)
	{
		FloatVector reversed;
		reversed.x = ret.x;
		reversed.y = ret.y;
		reversed.z = ret.z;
		reversed.w = ret.w;
		return reversed;
	}

	return ret;
}

void GLReplay::RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg)
{
	WrappedOpenGL &gl = *m_pDriver;

	if(cfg.position.buf == ResourceId())
		return;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, DebugData.outWidth/DebugData.outHeight);

	Camera cam;
	if(cfg.arcballCamera)
		cam.Arcball(cfg.cameraPos.x, Vec3f(cfg.cameraRot.x, cfg.cameraRot.y, cfg.cameraRot.z));
	else
		cam.fpsLook(Vec3f(cfg.cameraPos.x, cfg.cameraPos.y, cfg.cameraPos.z), Vec3f(cfg.cameraRot.x, cfg.cameraRot.y, cfg.cameraRot.z));

	Matrix4f camMat = cam.GetMatrix();

	Matrix4f ModelViewProj = projMat.Mul(camMat);
	Matrix4f guessProjInv;
	
	gl.glBindVertexArray(DebugData.meshVAO);

	const MeshFormat *fmts[2] = { &cfg.position, &cfg.second };
	
	GLenum topo = MakeGLPrimitiveTopology(cfg.position.topo);

	GLuint prog = DebugData.meshProg;
	
	GLint colLoc = gl.glGetUniformLocation(prog, "RENDERDOC_GenericFS_Color");
	GLint mvpLoc = gl.glGetUniformLocation(prog, "ModelViewProj");
	GLint fmtLoc = gl.glGetUniformLocation(prog, "Mesh_DisplayFormat");
	GLint sizeLoc = gl.glGetUniformLocation(prog, "PointSpriteSize");
	GLint homogLoc = gl.glGetUniformLocation(prog, "HomogenousInput");
	
	gl.glUseProgram(prog);

	gl.glEnable(eGL_FRAMEBUFFER_SRGB);

	if(cfg.position.unproject)
	{
		// the derivation of the projection matrix might not be right (hell, it could be an
		// orthographic projection). But it'll be close enough likely.
		Matrix4f guessProj = Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect);

		if(cfg.ortho)
		{
			guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
		}
		
		guessProjInv = guessProj.Inverse();

		ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
	}
	
	gl.glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, ModelViewProj.Data());
	gl.glUniform1ui(homogLoc, cfg.position.unproject);
	gl.glUniform2f(sizeLoc, 0.0f, 0.0f);
	
	if(!secondaryDraws.empty())
	{
		gl.glUniform4fv(colLoc, 1, &cfg.prevMeshColour.x);

		gl.glUniform1ui(fmtLoc, MESHDISPLAY_SOLID);
		
		gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

		// secondary draws have to come from gl_Position which is float4
		gl.glVertexAttribFormat(0, 4, eGL_FLOAT, GL_FALSE, 0);
		gl.glEnableVertexAttribArray(0);
		gl.glDisableVertexAttribArray(1);

		for(size_t i=0; i < secondaryDraws.size(); i++)
		{
			const MeshFormat &fmt = secondaryDraws[i];

			if(fmt.buf != ResourceId())
			{
				GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmt.buf).name;
				gl.glBindVertexBuffer(0, vb, fmt.offset, fmt.stride);

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

					gl.glDrawElements(secondarytopo, fmt.numVerts, idxtype, (const void *)uintptr_t(fmt.idxoffs));
				}
				else
				{
					gl.glDrawArrays(secondarytopo, 0, fmt.numVerts);
				}
			}
		}
	}

	for(uint32_t i=0; i < 2; i++)
	{
		if(fmts[i]->buf == ResourceId()) continue;

		if(fmts[i]->specialFormat != eSpecial_Unknown)
		{
			if(fmts[i]->specialFormat == eSpecial_R10G10B10A2)
			{
				if(fmts[i]->compType == eCompType_UInt)
					gl.glVertexAttribIFormat(i, 4, eGL_UNSIGNED_INT_2_10_10_10_REV, 0);
				if(fmts[i]->compType == eCompType_SInt)
					gl.glVertexAttribIFormat(i, 4, eGL_INT_2_10_10_10_REV, 0);
			}
			else if(fmts[i]->specialFormat == eSpecial_R11G11B10)
			{
				gl.glVertexAttribFormat(i, 4, eGL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, 0);
			}
			else
			{
				RDCWARN("Unsupported special vertex attribute format: %x", fmts[i]->specialFormat);
			}
		}
		else if(fmts[i]->compType == eCompType_Float ||
			fmts[i]->compType == eCompType_UNorm ||
			fmts[i]->compType == eCompType_SNorm)
		{
			GLenum fmttype = eGL_UNSIGNED_INT;

			if(fmts[i]->compByteWidth == 4)
			{
				if(fmts[i]->compType == eCompType_Float) fmttype = eGL_FLOAT;
				else if(fmts[i]->compType == eCompType_UNorm) fmttype = eGL_UNSIGNED_INT;
				else if(fmts[i]->compType == eCompType_SNorm) fmttype = eGL_INT;
			}
			else if(fmts[i]->compByteWidth == 2)
			{
				if(fmts[i]->compType == eCompType_Float) fmttype = eGL_HALF_FLOAT;
				else if(fmts[i]->compType == eCompType_UNorm) fmttype = eGL_UNSIGNED_SHORT;
				else if(fmts[i]->compType == eCompType_SNorm) fmttype = eGL_SHORT;
			}
			else if(fmts[i]->compByteWidth == 1)
			{
				if(fmts[i]->compType == eCompType_UNorm) fmttype = eGL_UNSIGNED_BYTE;
				else if(fmts[i]->compType == eCompType_SNorm) fmttype = eGL_BYTE;
			}

			gl.glVertexAttribFormat(i, fmts[i]->compCount, fmttype, fmts[i]->compType != eCompType_Float, 0);
		}
		else if(fmts[i]->compType == eCompType_UInt ||
			fmts[i]->compType == eCompType_SInt)
		{
			GLenum fmttype = eGL_UNSIGNED_INT;

			if(fmts[i]->compByteWidth == 4)
			{
				if(fmts[i]->compType == eCompType_UInt)  fmttype = eGL_UNSIGNED_INT;
				else if(fmts[i]->compType == eCompType_SInt)  fmttype = eGL_INT;
			}
			else if(fmts[i]->compByteWidth == 2)
			{
				if(fmts[i]->compType == eCompType_UInt)  fmttype = eGL_UNSIGNED_SHORT;
				else if(fmts[i]->compType == eCompType_SInt)  fmttype = eGL_SHORT;
			}
			else if(fmts[i]->compByteWidth == 1)
			{
				if(fmts[i]->compType == eCompType_UInt)  fmttype = eGL_UNSIGNED_BYTE;
				else if(fmts[i]->compType == eCompType_SInt)  fmttype = eGL_BYTE;
			}

			gl.glVertexAttribIFormat(i, fmts[i]->compCount, fmttype, 0);
		}
		else if(fmts[i]->compType == eCompType_Double)
		{
			gl.glVertexAttribLFormat(i, fmts[i]->compCount, eGL_DOUBLE, 0);
		}

		GLuint vb = m_pDriver->GetResourceManager()->GetCurrentResource(fmts[i]->buf).name;
		gl.glBindVertexBuffer(i, vb, fmts[i]->offset, fmts[i]->stride);
	}

	// enable position attribute
	gl.glEnableVertexAttribArray(0);
	gl.glDisableVertexAttribArray(1);

	// solid render
	if(cfg.solidShadeMode != eShade_None && topo != eGL_PATCHES)
	{
		gl.glEnable(eGL_DEPTH_TEST);
		gl.glDepthFunc(eGL_LESS);

		GLuint solidProg = prog;
		
		if(cfg.solidShadeMode == eShade_Lit)
		{
			// pick program with GS for per-face lighting
			solidProg = DebugData.meshgsProg;

			gl.glUseProgram(solidProg);

			GLint invProjLoc = gl.glGetUniformLocation(solidProg, "InvProj");

			Matrix4f InvProj = projMat.Inverse();

			gl.glUniformMatrix4fv(invProjLoc, 1, GL_FALSE, InvProj.Data());
		}

		GLint solidcolLoc = gl.glGetUniformLocation(solidProg, "RENDERDOC_GenericFS_Color");
		GLint solidmvpLoc = gl.glGetUniformLocation(solidProg, "ModelViewProj");
		GLint solidfmtLoc = gl.glGetUniformLocation(solidProg, "Mesh_DisplayFormat");
		GLint solidsizeLoc = gl.glGetUniformLocation(solidProg, "PointSpriteSize");
		GLint solidhomogLoc = gl.glGetUniformLocation(solidProg, "HomogenousInput");
		
		gl.glUniformMatrix4fv(solidmvpLoc, 1, GL_FALSE, ModelViewProj.Data());
		gl.glUniform2f(solidsizeLoc, 0.0f, 0.0f);
		gl.glUniform1ui(solidhomogLoc, cfg.position.unproject);
	
		if(cfg.second.buf != ResourceId())
			gl.glEnableVertexAttribArray(1);

		float wireCol[] = { 0.8f, 0.8f, 0.0f, 1.0f };
		gl.glUniform4fv(solidcolLoc, 1, wireCol);
		
		GLint OutputDisplayFormat = (int)cfg.solidShadeMode;
		if(cfg.solidShadeMode == eShade_Secondary && cfg.second.showAlpha)
			OutputDisplayFormat = MESHDISPLAY_SECONDARY_ALPHA;
		gl.glUniform1ui(solidfmtLoc, OutputDisplayFormat);
		
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
			gl.glDrawElements(topo, cfg.position.numVerts, idxtype, (const void *)uintptr_t(cfg.position.idxoffs));
		}
		else
		{
			gl.glDrawArrays(topo, 0, cfg.position.numVerts);
		}

		gl.glDisableVertexAttribArray(1);
		
		gl.glUseProgram(prog);
	}
	
	gl.glDisable(eGL_DEPTH_TEST);

	// wireframe render
	if(cfg.solidShadeMode == eShade_None || cfg.wireframeDraw || topo == eGL_PATCHES)
	{
		float wireCol[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		if(!secondaryDraws.empty())
		{
			wireCol[0] = cfg.currentMeshColour.x;
			wireCol[1] = cfg.currentMeshColour.y;
			wireCol[2] = cfg.currentMeshColour.z;
		}
		gl.glUniform4fv(colLoc, 1, wireCol);

		gl.glUniform1ui(fmtLoc, MESHDISPLAY_SOLID);

		gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

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
			gl.glDrawElements(topo != eGL_PATCHES ? topo : eGL_POINTS, cfg.position.numVerts, idxtype, (const void *)uintptr_t(cfg.position.idxoffs));
		}
		else
		{
			gl.glDrawArrays(topo != eGL_PATCHES ? topo : eGL_POINTS, 0, cfg.position.numVerts);
		}
	}
	
	// draw axis helpers
	if(!cfg.position.unproject)
	{
		gl.glBindVertexArray(DebugData.axisVAO);

		Vec4f wireCol(1.0f, 0.0f, 0.0f, 1.0f);
		gl.glUniform4fv(colLoc, 1, &wireCol.x);
		gl.glDrawArrays(eGL_LINES, 0, 2);

		wireCol = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
		gl.glUniform4fv(colLoc, 1, &wireCol.x);
		gl.glDrawArrays(eGL_LINES, 2, 2);

		wireCol = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
		gl.glUniform4fv(colLoc, 1, &wireCol.x);
		gl.glDrawArrays(eGL_LINES, 4, 2);
	}
	
	// 'fake' helper frustum
	if(cfg.position.unproject)
	{
		gl.glBindVertexArray(DebugData.frustumVAO);
		
		float wireCol[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		gl.glUniform4fv(colLoc, 1, wireCol);

		gl.glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, ModelViewProj.Data());
		
		gl.glDrawArrays(eGL_LINES, 0, 24);
	}

	gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_FILL);
	
	// show highlighted vertex
	if(cfg.highlightVert != ~0U)
	{
		MeshDataStage stage = cfg.type;
		
		if(m_HighlightCache.EID != eventID || stage != m_HighlightCache.stage ||
		   cfg.position.buf != m_HighlightCache.buf || cfg.position.offset != m_HighlightCache.offs)
		{
			m_HighlightCache.EID = eventID;
			m_HighlightCache.buf = cfg.position.buf;
			m_HighlightCache.offs = cfg.position.offset;
			m_HighlightCache.stage = stage;
			
			uint32_t bytesize = cfg.position.idxByteWidth; 

			m_HighlightCache.data = GetBufferData(cfg.position.buf, 0, 0);

			if(cfg.position.idxByteWidth == 0 || stage == eMeshDataStage_GSOut)
			{
				m_HighlightCache.indices.clear();
				m_HighlightCache.useidx = false;
			}
			else
			{
				m_HighlightCache.useidx = true;

				vector<byte> idxdata;
				if(cfg.position.idxbuf != ResourceId())
					idxdata = GetBufferData(cfg.position.idxbuf, cfg.position.idxoffs, cfg.position.numVerts*bytesize);

				uint8_t *idx8 = (uint8_t *)&idxdata[0];
				uint16_t *idx16 = (uint16_t *)&idxdata[0];
				uint32_t *idx32 = (uint32_t *)&idxdata[0];

				uint32_t numIndices = RDCMIN(cfg.position.numVerts, uint32_t(idxdata.size()/bytesize));

				m_HighlightCache.indices.resize(numIndices);

				if(bytesize == 1)
				{
					for(uint32_t i=0; i < numIndices; i++)
						m_HighlightCache.indices[i] = uint32_t(idx8[i]);
				}
				else if(bytesize == 2)
				{
					for(uint32_t i=0; i < numIndices; i++)
						m_HighlightCache.indices[i] = uint32_t(idx16[i]);
				}
				else if(bytesize == 4)
				{
					for(uint32_t i=0; i < numIndices; i++)
						m_HighlightCache.indices[i] = idx32[i];
				}
			}
		}

		GLenum meshtopo = topo;

		uint32_t idx = cfg.highlightVert;

		byte *data = &m_HighlightCache.data[0]; // buffer start
		byte *dataEnd = data + m_HighlightCache.data.size();

		data += cfg.position.offset; // to start of position data
		
		///////////////////////////////////////////////////////////////
		// vectors to be set from buffers, depending on topology

		bool valid = true;

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
		uint32_t primSize = 3; // number of verts per primitive
		
		if(meshtopo == eGL_LINES ||
		   meshtopo == eGL_LINES_ADJACENCY ||
		   meshtopo == eGL_LINE_STRIP ||
		   meshtopo == eGL_LINE_STRIP_ADJACENCY)
		{
			primSize = 2;
			primTopo = eGL_LINES;
		}
		
		activeVertex = InterpretVertex(data, idx, cfg, dataEnd, valid);

		// see Section 10.1 of the OpenGL 4.5 spec for
		// how primitive topologies are laid out
		if(meshtopo == eGL_LINES)
		{
			uint32_t v = uint32_t(idx/2) * 2; // find first vert in primitive

			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
		}
		else if(meshtopo == eGL_TRIANGLES)
		{
			uint32_t v = uint32_t(idx/3) * 3; // find first vert in primitive

			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+2, cfg, dataEnd, valid));
		}
		else if(meshtopo == eGL_LINES_ADJACENCY)
		{
			uint32_t v = uint32_t(idx/4) * 4; // find first vert in primitive
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);

			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);

			activePrim.push_back(vs[1]);
			activePrim.push_back(vs[2]);
		}
		else if(meshtopo == eGL_TRIANGLES_ADJACENCY)
		{
			uint32_t v = uint32_t(idx/6) * 6; // find first vert in primitive
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, valid),
				InterpretVertex(data, v+4, cfg, dataEnd, valid),
				InterpretVertex(data, v+5, cfg, dataEnd, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);
			adjacentPrimVertices.push_back(vs[2]);
			
			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);
			adjacentPrimVertices.push_back(vs[4]);
			
			adjacentPrimVertices.push_back(vs[4]);
			adjacentPrimVertices.push_back(vs[5]);
			adjacentPrimVertices.push_back(vs[0]);

			activePrim.push_back(vs[0]);
			activePrim.push_back(vs[2]);
			activePrim.push_back(vs[4]);
		}
		else if(meshtopo == eGL_LINE_STRIP)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 1U) - 1;
			
			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
		}
		else if(meshtopo == eGL_TRIANGLE_STRIP)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 2U) - 2;
			
			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+2, cfg, dataEnd, valid));
		}
		else if(meshtopo == eGL_LINE_STRIP_ADJACENCY)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 3U) - 3;
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);

			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);

			activePrim.push_back(vs[1]);
			activePrim.push_back(vs[2]);
		}
		else if(meshtopo == eGL_TRIANGLE_STRIP_ADJACENCY)
		{
			// Triangle strip with adjacency is the most complex topology, as
			// we need to handle the ends separately where the pattern breaks.

			uint32_t numidx = cfg.position.numVerts;

			if(numidx < 6)
			{
				// not enough indices provided, bail to make sure logic below doesn't
				// need to have tons of edge case detection
				valid = false;
			}
			else if(idx <= 4 || numidx <= 7)
			{
				FloatVector vs[] = {
					InterpretVertex(data, 0, cfg, dataEnd, valid),
					InterpretVertex(data, 1, cfg, dataEnd, valid),
					InterpretVertex(data, 2, cfg, dataEnd, valid),
					InterpretVertex(data, 3, cfg, dataEnd, valid),
					InterpretVertex(data, 4, cfg, dataEnd, valid),

					// note this one isn't used as it's adjacency for the next triangle
					InterpretVertex(data, 5, cfg, dataEnd, valid),

					// min() with number of indices in case this is a tiny strip
					// that is basically just a list
					InterpretVertex(data, RDCMIN(6U, numidx-1), cfg, dataEnd, valid),
				};

				// these are the triangles on the far left of the MSDN diagram above
				adjacentPrimVertices.push_back(vs[0]);
				adjacentPrimVertices.push_back(vs[1]);
				adjacentPrimVertices.push_back(vs[2]);

				adjacentPrimVertices.push_back(vs[4]);
				adjacentPrimVertices.push_back(vs[3]);
				adjacentPrimVertices.push_back(vs[0]);

				adjacentPrimVertices.push_back(vs[4]);
				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[6]);

				activePrim.push_back(vs[0]);
				activePrim.push_back(vs[2]);
				activePrim.push_back(vs[4]);
			}
			else if(idx > numidx-4)
			{
				// in diagram, numidx == 14

				FloatVector vs[] = {
					/*[0]=*/ InterpretVertex(data, numidx-8, cfg, dataEnd, valid), // 6 in diagram

					// as above, unused since this is adjacency for 2-previous triangle
					/*[1]=*/ InterpretVertex(data, numidx-7, cfg, dataEnd, valid), // 7 in diagram
					/*[2]=*/ InterpretVertex(data, numidx-6, cfg, dataEnd, valid), // 8 in diagram
					
					// as above, unused since this is adjacency for previous triangle
					/*[3]=*/ InterpretVertex(data, numidx-5, cfg, dataEnd, valid), // 9 in diagram
					/*[4]=*/ InterpretVertex(data, numidx-4, cfg, dataEnd, valid), // 10 in diagram
					/*[5]=*/ InterpretVertex(data, numidx-3, cfg, dataEnd, valid), // 11 in diagram
					/*[6]=*/ InterpretVertex(data, numidx-2, cfg, dataEnd, valid), // 12 in diagram
					/*[7]=*/ InterpretVertex(data, numidx-1, cfg, dataEnd, valid), // 13 in diagram
				};

				// these are the triangles on the far right of the MSDN diagram above
				adjacentPrimVertices.push_back(vs[2]); // 8 in diagram
				adjacentPrimVertices.push_back(vs[0]); // 6 in diagram
				adjacentPrimVertices.push_back(vs[4]); // 10 in diagram

				adjacentPrimVertices.push_back(vs[4]); // 10 in diagram
				adjacentPrimVertices.push_back(vs[7]); // 13 in diagram
				adjacentPrimVertices.push_back(vs[6]); // 12 in diagram

				adjacentPrimVertices.push_back(vs[6]); // 12 in diagram
				adjacentPrimVertices.push_back(vs[5]); // 11 in diagram
				adjacentPrimVertices.push_back(vs[2]); // 8 in diagram

				activePrim.push_back(vs[2]); // 8 in diagram
				activePrim.push_back(vs[4]); // 10 in diagram
				activePrim.push_back(vs[6]); // 12 in diagram
			}
			else
			{
				// we're in the middle somewhere. Each primitive has two vertices for it
				// so our step rate is 2. The first 'middle' primitive starts at indices 5&6
				// and uses indices all the way back to 0
				uint32_t v = RDCMAX( ( (idx+1) / 2) * 2, 6U) - 6;

				// these correspond to the indices in the MSDN diagram, with {2,4,6} as the
				// main triangle
				FloatVector vs[] = {
					InterpretVertex(data, v+0, cfg, dataEnd, valid),

					// this one is adjacency for 2-previous triangle
					InterpretVertex(data, v+1, cfg, dataEnd, valid),
					InterpretVertex(data, v+2, cfg, dataEnd, valid),

					// this one is adjacency for previous triangle
					InterpretVertex(data, v+3, cfg, dataEnd, valid),
					InterpretVertex(data, v+4, cfg, dataEnd, valid),
					InterpretVertex(data, v+5, cfg, dataEnd, valid),
					InterpretVertex(data, v+6, cfg, dataEnd, valid),
					InterpretVertex(data, v+7, cfg, dataEnd, valid),
					InterpretVertex(data, v+8, cfg, dataEnd, valid),
				};

				// these are the triangles around {2,4,6} in the MSDN diagram above
				adjacentPrimVertices.push_back(vs[0]);
				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[4]);

				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[5]);
				adjacentPrimVertices.push_back(vs[6]);

				adjacentPrimVertices.push_back(vs[6]);
				adjacentPrimVertices.push_back(vs[8]);
				adjacentPrimVertices.push_back(vs[4]);

				activePrim.push_back(vs[2]);
				activePrim.push_back(vs[4]);
				activePrim.push_back(vs[6]);
			}
		}
		else if(meshtopo == eGL_PATCHES)
		{
			uint32_t dim = (cfg.position.topo - eTopology_PatchList_1CPs + 1);

			uint32_t v0 = uint32_t(idx/dim) * dim;

			for(uint32_t v = v0; v < v0+dim; v++)
			{
				if(v != idx && valid)
					inactiveVertices.push_back(InterpretVertex(data, v, cfg, dataEnd, valid));
			}
		}
		else // if(meshtopo == eGL_POINTS) point list, or unknown/unhandled type
		{
			// no adjacency, inactive verts or active primitive
		}

		if(valid)
		{
			////////////////////////////////////////////////////////////////
			// prepare rendering (for both vertices & primitives)
			
			// if data is from post transform, it will be in clipspace
			if(cfg.position.unproject)
				ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
			else
				ModelViewProj = projMat.Mul(camMat);
			
			gl.glUniform1ui(homogLoc, cfg.position.unproject);
			
			gl.glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, ModelViewProj.Data());
			
			gl.glBindVertexArray(DebugData.triHighlightVAO);

			////////////////////////////////////////////////////////////////
			// render primitives
			
			// Draw active primitive (red)
			Vec4f WireframeColour(1.0f, 0.0f, 0.0f, 1.0f);
			gl.glUniform4fv(colLoc, 1, &WireframeColour.x);

			if(activePrim.size() >= primSize)
			{
				gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
				gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(Vec4f)*primSize, &activePrim[0]);

				gl.glDrawArrays(primTopo, 0, primSize);
			}

			// Draw adjacent primitives (green)
			WireframeColour = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
			gl.glUniform4fv(colLoc, 1, &WireframeColour.x);

			if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
			{
				gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
				gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(Vec4f)*adjacentPrimVertices.size(), &adjacentPrimVertices[0]);
				
				gl.glDrawArrays(primTopo, 0, (GLsizei)adjacentPrimVertices.size());
			}

			////////////////////////////////////////////////////////////////
			// prepare to render dots
			float scale = 800.0f/float(DebugData.outHeight);
			float asp = float(DebugData.outWidth)/float(DebugData.outHeight);

			Vec2f SpriteSize = Vec2f(scale/asp, scale);
			gl.glUniform2fv(sizeLoc, 1, &SpriteSize.x);

			// Draw active vertex (blue)
			WireframeColour = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
			gl.glUniform4fv(colLoc, 1, &WireframeColour.x);

			FloatVector vertSprite[4] = {
				activeVertex,
				activeVertex,
				activeVertex,
				activeVertex,
			};
			
			gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.triHighlightBuffer);
			gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(vertSprite), &vertSprite[0]);

			gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);

			// Draw inactive vertices (green)
			WireframeColour = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
			gl.glUniform4fv(colLoc, 1, &WireframeColour.x);

			for(size_t i=0; i < inactiveVertices.size(); i++)
			{
				vertSprite[0] = vertSprite[1] = vertSprite[2] = vertSprite[3] = inactiveVertices[i];
				
				gl.glBufferSubData(eGL_ARRAY_BUFFER, 0, sizeof(vertSprite), &vertSprite[0]);
				
				gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
			}
		}
	}
}
