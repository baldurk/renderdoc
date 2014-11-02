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

#include "common/string_utils.h"

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
		RDCERR("Shader error: %hs", buffer);
	}

	GLuint ret = gl.glCreateProgram();

	gl.glAttachShader(ret, cs);

	gl.glLinkProgram(ret);
	
	gl.glGetProgramiv(ret, eGL_LINK_STATUS, &status);
	if(status == 0)
	{
		gl.glGetProgramInfoLog(ret, 1024, NULL, buffer);
		RDCERR("Link error: %hs", buffer);
	}

	gl.glDeleteShader(cs);

	return ret;
}

GLuint GLReplay::CreateShaderProgram(const char *vsSrc, const char *psSrc)
{
	if(m_pDriver == NULL) return 0;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	WrappedOpenGL &gl = *m_pDriver;

	GLuint vs = gl.glCreateShader(eGL_VERTEX_SHADER);
	GLuint fs = gl.glCreateShader(eGL_FRAGMENT_SHADER);

	const char *src = vsSrc;
	gl.glShaderSource(vs, 1, &src, NULL);
	src = psSrc;
	gl.glShaderSource(fs, 1, &src, NULL);

	gl.glCompileShader(vs);
	gl.glCompileShader(fs);

	char buffer[1024];
	GLint status = 0;

	gl.glGetShaderiv(vs, eGL_COMPILE_STATUS, &status);
	if(status == 0)
	{
		gl.glGetShaderInfoLog(vs, 1024, NULL, buffer);
		RDCERR("Shader error: %hs", buffer);
	}

	gl.glGetShaderiv(fs, eGL_COMPILE_STATUS, &status);
	if(status == 0)
	{
		gl.glGetShaderInfoLog(fs, 1024, NULL, buffer);
		RDCERR("Shader error: %hs", buffer);
	}

	GLuint ret = gl.glCreateProgram();

	gl.glAttachShader(ret, vs);
	gl.glAttachShader(ret, fs);

	gl.glLinkProgram(ret);

	gl.glDeleteShader(vs);
	gl.glDeleteShader(fs);

	return ret;
}

#include "data/glsl/debuguniforms.h"

void GLReplay::InitDebugData()
{
	if(m_pDriver == NULL) return;
	
	{
		uint64_t id = MakeOutputWindow(NULL, true);

		m_DebugCtx = &m_OutputWindows[id];

		MakeCurrentReplayContext(m_DebugCtx);
	}

	DebugData.outWidth = 0.0f; DebugData.outHeight = 0.0f;
	
	DebugData.blitvsSource = GetEmbeddedResource(blit_vert);
	DebugData.blitfsSource = GetEmbeddedResource(blit_frag);

	DebugData.blitProg = CreateShaderProgram(DebugData.blitvsSource.c_str(), DebugData.blitfsSource.c_str());

	string texfs = GetEmbeddedResource(debuguniforms_h);
	texfs += GetEmbeddedResource(texsample_h);
	texfs += GetEmbeddedResource(texdisplay_frag);

	DebugData.texDisplayProg = CreateShaderProgram(DebugData.blitvsSource.c_str(), texfs.c_str());

	string checkerfs = GetEmbeddedResource(checkerboard_frag);
	
	DebugData.checkerProg = CreateShaderProgram(DebugData.blitvsSource.c_str(), checkerfs.c_str());

	DebugData.genericvsSource = GetEmbeddedResource(generic_vert);
	DebugData.genericfsSource = GetEmbeddedResource(generic_frag);

	DebugData.genericProg = CreateShaderProgram(DebugData.genericvsSource.c_str(), DebugData.genericfsSource.c_str());
	
	string meshvs = GetEmbeddedResource(mesh_vert);
	
	DebugData.meshProg = CreateShaderProgram(meshvs.c_str(), DebugData.genericfsSource.c_str());
	
	WrappedOpenGL &gl = *m_pDriver;

	{
		float data[] = {
			0.0f, -1.0f, 0.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f,
			1.0f,  0.0f, 0.0f, 1.0f,
			0.0f,  0.0f, 0.0f, 1.0f,
		};

		gl.glGenBuffers(1, &DebugData.outlineStripVB);
		gl.glBindBuffer(eGL_ARRAY_BUFFER, DebugData.outlineStripVB);
		gl.glBufferData(eGL_ARRAY_BUFFER, sizeof(data), data, eGL_STATIC_DRAW);
		
    gl.glGenVertexArrays(1, &DebugData.outlineStripVAO);
    gl.glBindVertexArray(DebugData.outlineStripVAO);
		
		gl.glVertexAttribPointer(0, 4, eGL_FLOAT, false, 0, (const void *)0);
		gl.glEnableVertexAttribArray(0);
	}

	gl.glGenSamplers(1, &DebugData.linearSampler);
	gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_MIN_FILTER, eGL_LINEAR_MIPMAP_NEAREST);
	gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_MAG_FILTER, eGL_LINEAR);
	gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glSamplerParameteri(DebugData.linearSampler, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	
	gl.glGenSamplers(1, &DebugData.pointSampler);
	gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST_MIPMAP_NEAREST);
	gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glSamplerParameteri(DebugData.pointSampler, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	
	gl.glGenBuffers(ARRAY_COUNT(DebugData.UBOs), DebugData.UBOs);
	for(size_t i=0; i < ARRAY_COUNT(DebugData.UBOs); i++)
	{
		gl.glBindBuffer(eGL_UNIFORM_BUFFER, DebugData.UBOs[i]);
		gl.glBufferData(eGL_UNIFORM_BUFFER, 512, NULL, eGL_DYNAMIC_DRAW);
		RDCCOMPILE_ASSERT(sizeof(texdisplay) < 512, "texdisplay UBO too large");
		RDCCOMPILE_ASSERT(sizeof(FontUniforms) < 512, "texdisplay UBO too large");
		RDCCOMPILE_ASSERT(sizeof(HistogramCBufferData) < 512, "texdisplay UBO too large");
	}

	DebugData.overlayTexWidth = DebugData.overlayTexHeight = 0;
	DebugData.overlayTex = DebugData.overlayFBO = 0;
	
	gl.glGenFramebuffers(1, &DebugData.pickPixelFBO);
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.pickPixelFBO);

	gl.glGenTextures(1, &DebugData.pickPixelTex);
	gl.glBindTexture(eGL_TEXTURE_2D, DebugData.pickPixelTex);
	
	gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_RGBA32F, 1, 1); 
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.pickPixelTex, 0);

	gl.glGenVertexArrays(1, &DebugData.emptyVAO);
	gl.glBindVertexArray(DebugData.emptyVAO);
	
	// histogram/minmax data
	{
		string glslheader = GetEmbeddedResource(debuguniforms_h);

		string histogramglsl = GetEmbeddedResource(texsample_h);
		histogramglsl += GetEmbeddedResource(histogram_comp);

		RDCEraseEl(DebugData.minmaxTileProgram);
		RDCEraseEl(DebugData.histogramProgram);
		RDCEraseEl(DebugData.minmaxResultProgram);

		for(int t=1; t <= TEXDISPLAY_TYPEMASK; t++)
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
		
		const uint32_t maxTexDim = 16384;
		const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK;
		const uint32_t maxBlocksNeeded = (maxTexDim*maxTexDim)/(blockPixSize*blockPixSize);

		const size_t byteSize = 2*sizeof(Vec4f)*HGRAM_TILES_PER_BLOCK*HGRAM_TILES_PER_BLOCK*maxBlocksNeeded;

		gl.glNamedBufferStorageEXT(DebugData.minmaxTileResult, byteSize, NULL, 0);
		gl.glNamedBufferStorageEXT(DebugData.minmaxResult, sizeof(Vec4f)*2, NULL, GL_MAP_READ_BIT);
	}

	MakeCurrentReplayContext(&m_ReplayCtx);

	gl.glGenVertexArrays(1, &DebugData.meshVAO);
	gl.glBindVertexArray(DebugData.meshVAO);
}

bool GLReplay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	if(m_pDriver->m_Textures.find(texid) == m_pDriver->m_Textures.end())
		return false;
	
	auto &texDetails = m_pDriver->m_Textures[texid];

	FetchTexture details = GetTexture(texid);

	const GLHookSet &gl = m_pDriver->GetHookset();
	
	gl.glBindBufferBase(eGL_UNIFORM_BUFFER, 0, DebugData.UBOs[0]);
	HistogramCBufferData *cdata = (HistogramCBufferData *)gl.glMapBufferRange(eGL_UNIFORM_BUFFER, 0, sizeof(HistogramCBufferData),
	                                                                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	cdata->HistogramTextureResolution.x = (float)RDCMAX(details.width>>mip, 1U);
	cdata->HistogramTextureResolution.y = (float)RDCMAX(details.height>>mip, 1U);
	cdata->HistogramTextureResolution.z = (float)RDCMAX(details.depth>>mip, 1U);
	cdata->HistogramSlice = (float)sliceFace;
	cdata->HistogramMip = (int)mip;
	cdata->HistogramSample = (int)RDCCLAMP(sample, 0U, details.msSamp-1);
	if(sample == ~0U) cdata->HistogramSample = -int(details.msSamp);
	cdata->HistogramMin = 0.0f;
	cdata->HistogramMax = 1.0f;
	cdata->HistogramChannels = 0xf;
	
	int texSlot = 0;
	int intIdx = 0;
	
	switch (texDetails.curType)
	{
		case eGL_TEXTURE_1D:
			texSlot = RESTYPE_TEX1D;
			break;
		default:
			RDCWARN("Unexpected texture type");
		case eGL_TEXTURE_2D:
			texSlot = RESTYPE_TEX2D;
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

	if(details.format.compType == eCompType_UInt)
	{
		texSlot |= TEXDISPLAY_UINT_TEX;
		intIdx = 1;
	}
	if(details.format.compType == eCompType_SInt)
	{
		texSlot |= TEXDISPLAY_SINT_TEX;
		intIdx = 2;
	}
	
	if(details.dimension == 3)
		cdata->HistogramSlice = float(sliceFace)/float(details.depth);
	
	int blocksX = (int)ceil(cdata->HistogramTextureResolution.x/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));
	int blocksY = (int)ceil(cdata->HistogramTextureResolution.y/float(HGRAM_PIXELS_PER_TILE*HGRAM_TILES_PER_BLOCK));

	gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

	gl.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + texSlot));
	gl.glBindTexture(texDetails.curType, texDetails.resource.name);
	gl.glBindSampler(texSlot, DebugData.pointSampler);
	
	gl.glBindBufferBase(eGL_SHADER_STORAGE_BUFFER, 0, DebugData.minmaxTileResult);

	gl.glUseProgram(DebugData.minmaxTileProgram[texSlot]);
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

	{
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

		RenderTexture(texDisplay);
	}

	gl.glReadPixels(0, 0, 1, 1, eGL_RGBA, eGL_FLOAT, (void *)pixel);
}

bool GLReplay::RenderTexture(TextureDisplay cfg)
{
	FetchTexture tex = GetTexture(cfg.texid);

	MakeCurrentReplayContext(m_DebugCtx);
	
	WrappedOpenGL &gl = *m_pDriver;
	
	gl.glUseProgram(DebugData.texDisplayProg);

	auto &texDetails = m_pDriver->m_Textures[cfg.texid];

	int resType;
	switch (texDetails.curType)
	{
		case eGL_TEXTURE_1D:
			resType = RESTYPE_TEX1D;
			break;
		default:
			RDCWARN("Unexpected texture type");
		case eGL_TEXTURE_2D:
			resType = RESTYPE_TEX2D;
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

	RDCGLenum dsTexMode = eGL_NONE;
	if (tex.creationFlags & eTextureCreate_DSV)
	{
		if (!cfg.Red && cfg.Green)
		{
			dsTexMode = eGL_STENCIL_INDEX;

			// Stencil texture sampling is not normalized in OpenGL
			resType |= TEXDISPLAY_UINT_TEX;
			float rangeScale;
			switch (tex.format.rawType)
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
		switch (tex.format.compType)
		{
			case eCompType_UInt:
				resType |= TEXDISPLAY_UINT_TEX;
				break;
			case eCompType_SInt:
				resType |= TEXDISPLAY_SINT_TEX;
				break;
			default:
				break;
		}
	}
	
	gl.glActiveTexture((RDCGLenum)(eGL_TEXTURE0 + resType));
	gl.glBindTexture(texDetails.curType, texDetails.resource.name);

	GLint origDSTexMode = eGL_DEPTH_COMPONENT;
	if (dsTexMode != eGL_NONE)
	{
		gl.glGetTexParameteriv(texDetails.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE, &origDSTexMode);
		gl.glTexParameteri(texDetails.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE, dsTexMode);
	}

	if(cfg.mip == 0 && cfg.scale < 1.0f && dsTexMode == eGL_NONE)
		gl.glBindSampler(resType, DebugData.linearSampler);
	else
		gl.glBindSampler(resType, DebugData.pointSampler);

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
	ubo->Slice = (float)cfg.sliceFace;

	ubo->OutputDisplayFormat = resType;

	if(dsTexMode != eGL_NONE)
		ubo->OutputDisplayFormat |= TEXDISPLAY_DEPTH_TEX;

	ubo->RawOutput = cfg.rawoutput ? 1 : 0;

	ubo->TextureResolutionPS.x = float(tex_x);
	ubo->TextureResolutionPS.y = float(tex_y);
	ubo->TextureResolutionPS.z = float(tex_z);

	ubo->OutputRes.x = DebugData.outWidth;
	ubo->OutputRes.y = DebugData.outHeight;

	gl.glUnmapBuffer(eGL_UNIFORM_BUFFER);

	if(cfg.rawoutput)
	{
		gl.glDisable(eGL_BLEND);
	}
	else
	{
		gl.glEnable(eGL_BLEND);
		gl.glBlendFunc(eGL_SRC_ALPHA, eGL_ONE_MINUS_SRC_ALPHA);
	}

	gl.glBindVertexArray(DebugData.emptyVAO);
	gl.glDrawArrays(eGL_TRIANGLE_STRIP, 0, 4);
	
	gl.glBindSampler(0, 0);

	if (dsTexMode != eGL_NONE)
		gl.glTexParameteri(texDetails.curType, eGL_DEPTH_STENCIL_TEXTURE_MODE, origDSTexMode);

	return true;
}

void GLReplay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
	MakeCurrentReplayContext(m_DebugCtx);
	
	WrappedOpenGL &gl = *m_pDriver;
	
	gl.glUseProgram(DebugData.checkerProg);

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
	
	gl.glBindVertexArray(DebugData.outlineStripVAO);
	gl.glDrawArrays(eGL_LINE_LOOP, 0, 4);

	offsetVal = Vec4f(-xpixdim, ypixdim, 0.0f, 0.0f);
	scaleVal = Vec4f(xdim+xpixdim*2, ydim+ypixdim*2, 1.0f, 1.0f);
	colVal = Vec4f(0.0f, 0.0f, 0.0f, 1.0f);
	
	gl.glUniform4fv(offsetLoc, 1, &offsetVal.x);
	gl.glUniform4fv(scaleLoc, 1, &scaleVal.x);
	gl.glUniform4fv(colLoc, 1, &colVal.x);

	gl.glBindVertexArray(DebugData.outlineStripVAO);
	gl.glDrawArrays(eGL_LINE_LOOP, 0, 4);
}

ResourceId GLReplay::RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(&m_ReplayCtx);

	GLuint curProg = 0;
	gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint*)&curProg);

	GLuint curDrawFBO = 0;
	GLuint curReadFBO = 0;
	gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curDrawFBO);
	gl.glGetIntegerv(eGL_DRAW_FRAMEBUFFER_BINDING, (GLint*)&curReadFBO);

	void *ctx = m_ReplayCtx.ctx;
	
	auto &progDetails = m_pDriver->m_Programs[m_pDriver->GetResourceManager()->GetID(ProgramRes(ctx, curProg))];

	if(progDetails.colOutProg == 0)
	{
		progDetails.colOutProg = gl.glCreateProgram();
		GLuint shad = gl.glCreateShader(eGL_FRAGMENT_SHADER);

		const char *src = DebugData.genericfsSource.c_str();
		gl.glShaderSource(shad, 1, &src, NULL);
		gl.glCompileShader(shad);
		gl.glAttachShader(progDetails.colOutProg, shad);
		gl.glDeleteShader(shad);

		for(size_t i=0; i < progDetails.shaders.size(); i++)
		{
			const auto &shadDetails = m_pDriver->m_Shaders[progDetails.shaders[i]];

			if(shadDetails.type != eGL_FRAGMENT_SHADER)
			{
				shad = gl.glCreateShader(shadDetails.type);
				for(size_t s=0; s < shadDetails.sources.size(); s++)
				{
					src = shadDetails.sources[s].c_str();
					gl.glShaderSource(shad, 1, &src, NULL);
				}
				gl.glCompileShader(shad);
				gl.glAttachShader(progDetails.colOutProg, shad);
				gl.glDeleteShader(shad);
			}
		}

		gl.glLinkProgram(progDetails.colOutProg);
	}
	
	auto &texDetails = m_pDriver->m_Textures[texid];
	
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

		gl.glTexStorage2D(eGL_TEXTURE_2D, 1, eGL_RGBA8, texDetails.width, texDetails.height); 
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
		gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
		gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, DebugData.overlayTex, 0);
		
		gl.glBindTexture(eGL_TEXTURE_2D, curTex);
	}
	
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, DebugData.overlayFBO);
	
	if(overlay == eTexOverlay_NaN || overlay == eTexOverlay_Clipping)
	{
		// just need the basic texture
		float black[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, black);
	}
	else if(overlay == eTexOverlay_Drawcall)
	{
		gl.glUseProgram(progDetails.colOutProg);
		
		{
			// copy across uniforms
			GLint numUniforms = 0;
			gl.glGetProgramiv(curProg, eGL_ACTIVE_UNIFORMS, &numUniforms);

			for(GLint i=0; i < numUniforms; i++)
			{
				char uniName[1024] = {};
				GLint uniSize = 0;
				GLenum uniType = eGL_NONE;
				gl.glGetActiveUniform(curProg, i, 1024, NULL, &uniSize, &uniType, uniName);

				GLint origloc = gl.glGetUniformLocation(curProg, uniName);
				GLint newloc = gl.glGetUniformLocation(progDetails.colOutProg, uniName);

				double dv[16];
				float *fv = (float *)dv;

				if(uniSize > 1)
				{
					RDCERR("Array elements beyond [0] not being copied to new program");
				}

				if(origloc != -1 && newloc != -1)
				{
					if(uniType == eGL_FLOAT_MAT4)
					{
						gl.glGetUniformfv(curProg, origloc, fv);
						gl.glUniformMatrix4fv(newloc, 1, false, fv);
					}
					else if(uniType == eGL_FLOAT_VEC3)
					{
						gl.glGetUniformfv(curProg, origloc, fv);
						gl.glUniform3fv(newloc, 1, fv);
					}
					else if(uniType == eGL_FLOAT_VEC4)
					{
						gl.glGetUniformfv(curProg, origloc, fv);
						gl.glUniform4fv(newloc, 1, fv);
					}
					else
					{
						RDCERR("Uniform type '%hs' not being copied to new program", ToStr::Get(uniType).c_str());
					}
				}
			}
		}
		
		float black[] = { 0.0f, 0.0f, 0.0f, 0.5f };
		gl.glClearBufferfv(eGL_COLOR, 0, black);

		GLint colLoc = gl.glGetUniformLocation(progDetails.colOutProg, "RENDERDOC_GenericFS_Color");
		float colVal[] = { 0.8f, 0.1f, 0.8f, 1.0f };
		gl.glUniform4fv(colLoc, 1, colVal);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
		
		gl.glUseProgram(curProg);
	}
	else if(overlay == eTexOverlay_Wireframe)
	{
		gl.glUseProgram(progDetails.colOutProg);
		
		{
			// copy across uniforms
			GLint numUniforms = 0;
			gl.glGetProgramiv(curProg, eGL_ACTIVE_UNIFORMS, &numUniforms);

			for(GLint i=0; i < numUniforms; i++)
			{
				char uniName[1024] = {};
				GLint uniSize = 0;
				GLenum uniType = eGL_NONE;
				gl.glGetActiveUniform(curProg, i, 1024, NULL, &uniSize, &uniType, uniName);

				GLint origloc = gl.glGetUniformLocation(curProg, uniName);
				GLint newloc = gl.glGetUniformLocation(progDetails.colOutProg, uniName);

				double dv[16];
				float *fv = (float *)dv;

				if(uniSize > 1)
				{
					RDCERR("Array elements beyond [0] not being copied to new program");
				}

				if(origloc != -1 && newloc != -1)
				{
					if(uniType == eGL_FLOAT_MAT4)
					{
						gl.glGetUniformfv(curProg, origloc, fv);
						gl.glUniformMatrix4fv(newloc, 1, false, fv);
					}
					else if(uniType == eGL_FLOAT_VEC3)
					{
						gl.glGetUniformfv(curProg, origloc, fv);
						gl.glUniform3fv(newloc, 1, fv);
					}
					else if(uniType == eGL_FLOAT_VEC4)
					{
						gl.glGetUniformfv(curProg, origloc, fv);
						gl.glUniform4fv(newloc, 1, fv);
					}
					else
					{
						RDCERR("Uniform type '%hs' not being copied to new program", ToStr::Get(uniType).c_str());
					}
				}
			}
		}
		
		float wireCol[] = { 200.0f/255.0f, 255.0f/255.0f, 0.0f/255.0f, 0.0f };
		gl.glClearBufferfv(eGL_COLOR, 0, wireCol);

		GLint colLoc = gl.glGetUniformLocation(progDetails.colOutProg, "RENDERDOC_GenericFS_Color");
		wireCol[3] = 1.0f;
		gl.glUniform4fv(colLoc, 1, wireCol);

		GLint depthTest = GL_FALSE;
		gl.glGetIntegerv(eGL_DEPTH_TEST, (GLint*)&depthTest);
		GLenum polyMode = eGL_FILL;
		gl.glGetIntegerv(eGL_POLYGON_MODE, (GLint*)&polyMode);

		gl.glDisable(eGL_DEPTH_TEST);
		gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

		ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);

		if(depthTest)
			gl.glEnable(eGL_DEPTH_TEST);
		if(polyMode != eGL_LINE)
			gl.glPolygonMode(eGL_FRONT_AND_BACK, polyMode);
		
		gl.glUseProgram(curProg);
	}
	
	gl.glBindFramebuffer(eGL_DRAW_FRAMEBUFFER, curDrawFBO);
	gl.glBindFramebuffer(eGL_READ_FRAMEBUFFER, curReadFBO);

	return m_pDriver->GetResourceManager()->GetID(TextureRes(ctx, DebugData.overlayTex));
}

void GLReplay::RenderMesh(uint32_t frameID, const vector<uint32_t> &events, MeshDisplay cfg)
{
	WrappedOpenGL &gl = *m_pDriver;
	
	MakeCurrentReplayContext(m_DebugCtx);
	
	GLuint curFBO = 0;
	gl.glGetIntegerv(eGL_FRAMEBUFFER_BINDING, (GLint*)&curFBO);

	OutputWindow *outw = NULL;
	for(auto it = m_OutputWindows.begin(); it != m_OutputWindows.end(); ++it)
	{
		if(it->second.BlitData.windowFBO == curFBO)
		{
			outw = &it->second;
			break;
		}
	}

	if(!outw) return;
	
	const auto &attr = m_CurPipelineState.m_VtxIn.attributes[0];
	const auto &vb = m_CurPipelineState.m_VtxIn.vbuffers[attr.BufferSlot];

	if(vb.Buffer == ResourceId())
		return;
	
	MakeCurrentReplayContext(&m_ReplayCtx);

	GLint viewport[4];
	gl.glGetIntegerv(eGL_VIEWPORT, viewport);
	
	gl.glGetIntegerv(eGL_FRAMEBUFFER_BINDING, (GLint*)&curFBO);

	if(outw->BlitData.replayFBO == 0)
	{
		gl.glGenFramebuffers(1, &outw->BlitData.replayFBO);
		gl.glBindFramebuffer(eGL_FRAMEBUFFER, outw->BlitData.replayFBO);

		gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, outw->BlitData.backbuffer, 0);
	}
	else
	{
		gl.glBindFramebuffer(eGL_FRAMEBUFFER, outw->BlitData.replayFBO);
	}
	
	gl.glViewport(0, 0, (GLsizei)DebugData.outWidth, (GLsizei)DebugData.outHeight);
	
	GLuint curProg = 0;
	gl.glGetIntegerv(eGL_CURRENT_PROGRAM, (GLint*)&curProg);
	
	gl.glUseProgram(DebugData.meshProg);
	
	float wireCol[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	GLint colLoc = gl.glGetUniformLocation(DebugData.meshProg, "RENDERDOC_GenericFS_Color");
	gl.glUniform4fv(colLoc, 1, wireCol);
	
	Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, DebugData.outWidth/DebugData.outHeight);

	Camera cam;
	if(cfg.arcballCamera)
		cam.Arcball(cfg.cameraPos.x, Vec3f(cfg.cameraRot.x, cfg.cameraRot.y, cfg.cameraRot.z));
	else
		cam.fpsLook(Vec3f(cfg.cameraPos.x, cfg.cameraPos.y, cfg.cameraPos.z), Vec3f(cfg.cameraRot.x, cfg.cameraRot.y, cfg.cameraRot.z));

	Matrix4f camMat = cam.GetMatrix();

	Matrix4f ModelViewProj = projMat.Mul(camMat);

	GLint mvpLoc = gl.glGetUniformLocation(DebugData.meshProg, "ModelViewProj");
	gl.glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, ModelViewProj.Data());
	
	GLuint curVAO = 0;
	gl.glGetIntegerv(eGL_VERTEX_ARRAY_BINDING, (GLint*)&curVAO);
	
	GLuint curArr = 0;
	gl.glGetIntegerv(eGL_ARRAY_BUFFER_BINDING, (GLint*)&curArr);

	gl.glBindVertexArray(DebugData.meshVAO);

	// TODO: we should probably use glBindVertexBuffer, glVertexAttribFormat, glVertexAttribBinding.
	// For now just assume things about the format and vbuffer.

	gl.glBindBuffer(eGL_ARRAY_BUFFER, m_pDriver->GetResourceManager()->GetLiveResource(vb.Buffer).name);

	if(attr.Format.compType == eCompType_Float && attr.Format.compByteWidth == 4)
	{
		gl.glVertexAttribPointer(0, attr.Format.compCount, eGL_FLOAT, GL_FALSE, 0, (void *)intptr_t(vb.Offset + attr.RelativeOffset));
	}
	else if(attr.Format.compType == eCompType_Float && attr.Format.compByteWidth == 2)
	{
		gl.glVertexAttribPointer(0, attr.Format.compCount, eGL_HALF_FLOAT, GL_FALSE, 0, (void *)intptr_t(vb.Offset + attr.RelativeOffset));
	}
	else
	{
		RDCERR("Not handling mesh display of unsupported format");
		return;
	}

	gl.glEnableVertexAttribArray(0);

	{
		GLint depthTest = GL_FALSE;
		gl.glGetIntegerv(eGL_DEPTH_TEST, (GLint*)&depthTest);
		GLenum polyMode = eGL_FILL;
		gl.glGetIntegerv(eGL_POLYGON_MODE, (GLint*)&polyMode);

		gl.glDisable(eGL_DEPTH_TEST);
		gl.glPolygonMode(eGL_FRONT_AND_BACK, eGL_LINE);

		ReplayLog(frameID, 0, events[0], eReplay_OnlyDraw);

		if(depthTest)
			gl.glEnable(eGL_DEPTH_TEST);
		if(polyMode != eGL_LINE)
			gl.glPolygonMode(eGL_FRONT_AND_BACK, polyMode);
	}

	gl.glBindVertexArray(curVAO);
	gl.glBindBuffer(eGL_ARRAY_BUFFER, curArr);
	
	gl.glUseProgram(curProg);
	gl.glViewport(viewport[0], viewport[1], (GLsizei)viewport[2], (GLsizei)viewport[3]);
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, curFBO);
}
