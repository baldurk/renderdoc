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


#include "common/common.h"
#include "gl_driver.h"

#include "common/string_utils.h"

#include "replay/type_helpers.h"

#include "maths/vec.h"

#include "jpeg-compressor/jpge.h"

const char *GLChunkNames[] =
{
	"WrappedOpenGL::Initialisation",

	"glGenTextures",
	"glBindTexture",
	"glActiveTexture",
	"glTexStorage1D",
	"glTexStorage2D",
	"glTexStorage3D",
	"glTexSubImage1D",
	"glTexSubImage2D",
	"glTexSubImage3D",
	"glCompressedTexSubImage1D",
	"glCompressedTexSubImage2D",
	"glCompressedTexSubImage3D",
	"glTexBufferRange",
	"glPixelStore",
	"glTexParameterf",
	"glTexParameterfv",
	"glTexParameteri",
	"glTexParameteriv",
	"glGenerateMipmap",
	"glCopyImageSubData",
	"glTextureView",

	"glCreateShader",
	"glCreateProgram",
	"glCreateShaderProgramv",
	"glCompileShader",
	"glShaderSource",
	"glAttachShader",
	"glDetachShader",
	"glUseProgram",
	"glProgramParameter",
	"glBindAttribLocation",
	"glUniformBlockBinding",
	"glProgramUniformVector*",
	"glLinkProgram",
	
	"glGenProgramPipelines",
	"glUseProgramStages",
	"glBindProgramPipeline",

	"glFenceSync",
	"glClientWaitSync",
	"glWaitSync",

	"glGenQueries",
	"glBeginQuery",
	"glEndQuery",

	"glClearColor",
	"glClearDepth",
	"glClear",
	"glClearBufferfv",
	"glClearBufferiv",
	"glClearBufferuiv",
	"glClearBufferfi",
	"glPolygonMode",
	"glPolygonOffset",
	"glCullFace",
	"glHint",
	"glEnable",
	"glDisable",
	"glEnablei",
	"glDisablei",
	"glFrontFace",
	"glBlendFunc",
	"glBlendFunci",
	"glBlendColor",
	"glBlendFuncSeparate",
	"glBlendFuncSeparatei",
	"glBlendEquationSeparate",
	"glBlendEquationSeparatei",
	"glStencilOp",
	"glStencilOpSeparate",
	"glStencilFunc",
	"glStencilFuncSeparate",
	"glStencilMask",
	"glStencilMaskSeparate",
	"glColorMask",
	"glColorMaski",
	"glSampleMaski",
	"glDepthFunc",
	"glDepthMask",
	"glDepthRange",
	"glDepthRangef",
	"glDepthRangeArrayv",
	"glDepthBoundsEXT",
	"glPatchParameteri",
	"glPatchParameterfv",
	"glViewport",
	"glViewportArrayv",
	"glScissor",
	"glScissorArrayv",
	"glBindVertexArray",
	"glBindVertexBuffer",
	"glVertexBindingDivisor",
	"glUniformMatrix*",
	"glUniformVector*",
	"glDrawArrays",
	"glDrawArraysInstanced",
	"glDrawArraysInstancedBaseInstance",
	"glDrawElements",
	"glDrawRangeElements",
	"glDrawElementsInstanced",
	"glDrawElementsInstancedBaseInstance",
	"glDrawElementsBaseVertex",
	"glDrawElementsInstancedBaseVertex",
	"glDrawElementsInstancedBaseVertexBaseInstance",

	"glGenFramebuffers",
	"glFramebufferTexture",
	"glFramebufferTexture2D",
	"glFramebufferTextureLayer",
	"glReadBuffer",
	"glBindFramebuffer",
	"glDrawBuffer",
	"glDrawBuffers",
	"glBlitFramebuffer",

	"glGenSamplers",
	"glSamplerParameteri",
	"glSamplerParameterf",
	"glSamplerParameteriv",
	"glSamplerParameterfv",
	"glSamplerParameterIiv",
	"glSamplerParameterIuiv",
	"glBindSampler",

	"glGenBuffers",
	"glBindBuffer",
	"glBindBufferBase",
	"glBindBufferRange",
	"glBufferStorage",
	"glBufferData",
	"glBufferSubData",
	"glCopyBufferSubData",
	"glUnmapBuffer",
	"glGenVertexArrays",
	"glBindVertexArray",
	"glVertexAttribPointer",
	"glVertexAttribIPointer",
	"glEnableVertexAttribArray",
	"glDisableVertexAttribArray",
	"glVertexAttribFormat",
	"glVertexAttribIFormat",
	"glVertexAttribBinding",
	
	
	"glObjectLabel",
	"glPushDebugGroup",
	"glDebugMessageInsert",
	"glPopDebugGroup",

	"Capture",
	"BeginCapture",
	"EndCapture",
};

template<>
string ToStrHelper<false, WrappedOpenGL::UniformType>::Get(const WrappedOpenGL::UniformType &el)
{
	switch(el)
	{
		case WrappedOpenGL::UNIFORM_UNKNOWN: return "unk";
		case WrappedOpenGL::VEC1FV: return "1fv";
		case WrappedOpenGL::VEC1IV: return "1iv";
		case WrappedOpenGL::VEC1UIV: return "1uiv";
		case WrappedOpenGL::VEC2FV: return "2fv";
		case WrappedOpenGL::VEC3FV: return "3fv";
		case WrappedOpenGL::VEC4FV: return "4fv";
		case WrappedOpenGL::MAT4FV: return "4fv";
	}

	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "WrappedOpenGL::UniformType<%d>", el);

	return tostrBuf;
}

GLInitParams::GLInitParams()
{
	SerialiseVersion = GL_SERIALISE_VERSION;
	colorBits = 32;
	depthBits = 32;
	stencilBits = 8;
	width = 32;
	height = 32;
}

ReplayCreateStatus GLInitParams::Serialise()
{
	SERIALISE_ELEMENT(uint32_t, ver, GL_SERIALISE_VERSION); SerialiseVersion = ver;

	if(ver != GL_SERIALISE_VERSION)
	{
		RDCERR("Incompatible OpenGL serialise version, expected %d got %d", GL_SERIALISE_VERSION, ver);
		return eReplayCreate_APIIncompatibleVersion;
	}
	
	SERIALISE_ELEMENT(uint32_t, col, colorBits); colorBits = col;
	SERIALISE_ELEMENT(uint32_t, dpth, depthBits); depthBits = dpth;
	SERIALISE_ELEMENT(uint32_t, stenc, stencilBits); stencilBits = stenc;
	SERIALISE_ELEMENT(uint32_t, w, width); width = w;
	SERIALISE_ELEMENT(uint32_t, h, height); height = h;

	return eReplayCreate_Success;
}

WrappedOpenGL::WrappedOpenGL(const wchar_t *logfile, const GLHookSet &funcs)
	: m_Real(funcs)
{
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(WrappedOpenGL));

	glExts.push_back("GL_ARB_multitexture");
	glExts.push_back("GL_ARB_debug_output");
	glExts.push_back("GL_EXT_direct_state_access");
	glExts.push_back("GL_ARB_internalformat_query");
	glExts.push_back("GL_ARB_internalformat_query2");
	
#if !defined(_RELEASE)
	CaptureOptions &opts = (CaptureOptions &)RenderDoc::Inst().GetCaptureOptions();
	opts.RefAllResources = true;
#endif

	m_Replay.SetDriver(this);

	// TODO: need to check against implementation to ensure we don't claim to support
	// an extension that it doesn't!
	merge(glExts, glExtsString, ' ');
	
	m_FrameCounter = 0;

	m_FrameTimer.Restart();

	m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;

	m_CurFileSize = 0;

	m_RealDebugFunc = NULL;
	m_RealDebugFuncParam = NULL;
	
	m_DrawcallStack.push_back(&m_ParentDrawcall);

	m_CurEventID = 1;
	m_CurDrawcallID = 1;

	RDCEraseEl(m_TextureRecord);
	RDCEraseEl(m_BufferRecord);
	m_VertexArrayRecord = NULL;
	m_DrawFramebufferRecord = NULL;
	m_ReadFramebufferRecord = NULL;
	m_TextureUnit = 0;
	
	m_LastIndexSize = eGL_NONE;
	m_LastIndexOffset = 0;
	m_LastDrawMode = eGL_NONE;

	m_DisplayListRecord = NULL;
	
#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	if(RenderDoc::Inst().IsReplayApp())
	{
		m_State = READING;
		if(logfile)
		{
			m_pSerialiser = new Serialiser(logfile, Serialiser::READING, debugSerialiser);
		}
		else
		{
			byte dummy[4];
			m_pSerialiser = new Serialiser(4, dummy, false);
		}

		if(m_Real.glDebugMessageCallback)
		{
			m_Real.glDebugMessageCallback(&DebugSnoopStatic, this);
			m_Real.glEnable(eGL_DEBUG_OUTPUT_SYNCHRONOUS);
		}
	}
	else
	{
		m_State = WRITING_IDLE;
		m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
	}

	m_DeviceRecord = NULL;

	m_ResourceManager = new GLResourceManager(m_State, m_pSerialiser, this);

	m_DeviceResourceID = GetResourceManager()->RegisterResource(GLResource(NULL, eResSpecial, eSpecialResDevice));
	m_ContextResourceID = GetResourceManager()->RegisterResource(GLResource(NULL, eResSpecial, eSpecialResContext));

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_DeviceResourceID);
		m_DeviceRecord->DataInSerialiser = false;
		m_DeviceRecord->Length = 0;
		m_DeviceRecord->NumSubResources = 0;
		m_DeviceRecord->SpecialResource = true;
		m_DeviceRecord->SubResources = NULL;
		
		m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
		m_ContextRecord->DataInSerialiser = false;
		m_ContextRecord->Length = 0;
		m_ContextRecord->NumSubResources = 0;
		m_ContextRecord->SpecialResource = true;
		m_ContextRecord->SubResources = NULL;
	}
	else
	{
		m_DeviceRecord = m_ContextRecord = NULL;

		TrackedResource::SetReplayResourceIDs();
	}

	m_FakeBB_FBO = 0;
	m_FakeBB_Color = 0;
	m_FakeBB_DepthStencil = 0;
		
	RDCDEBUG("Debug Text enabled - for development! remove before release!");
	m_pSerialiser->SetDebugText(true);
	
	m_pSerialiser->SetChunkNameLookup(&GetChunkName);

	//////////////////////////////////////////////////////////////////////////
	// Compile time asserts

	RDCCOMPILE_ASSERT(ARRAY_COUNT(GLChunkNames) == NUM_OPENGL_CHUNKS-FIRST_CHUNK_ID, "Not right number of chunk names");
}

void WrappedOpenGL::Initialise(GLInitParams &params)
{
	// deliberately want to go through our own wrappers to set up e.g. m_Textures members
	WrappedOpenGL &gl = *this;

	gl.glGenFramebuffers(1, &m_FakeBB_FBO);
	gl.glBindFramebuffer(eGL_FRAMEBUFFER, m_FakeBB_FBO);

	gl.glGenTextures(1, &m_FakeBB_Color);
	gl.glBindTexture(eGL_TEXTURE_2D, m_FakeBB_Color);

	gl.glObjectLabel(eGL_TEXTURE, m_FakeBB_Color, -1, "Backbuffer Color");

	GLNOTIMP("backbuffer needs to resize if the size is exceeded");

	GLenum colfmt = eGL_RGBA8;

	if(params.colorBits == 32)
		colfmt = eGL_RGBA8;
	else if(params.colorBits == 24)
		colfmt = eGL_RGB8;
	else
		RDCERR("Unexpected # colour bits: %d", params.colorBits);

	gl.glTexStorage2D(eGL_TEXTURE_2D, 1, colfmt, params.width, params.height); 
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MIN_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_MAG_FILTER, eGL_NEAREST);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_S, eGL_CLAMP_TO_EDGE);
	gl.glTexParameteri(eGL_TEXTURE_2D, eGL_TEXTURE_WRAP_T, eGL_CLAMP_TO_EDGE);
	gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_COLOR_ATTACHMENT0, m_FakeBB_Color, 0);

	gl.glViewport(0, 0, params.width, params.height);

	if(params.depthBits > 0 || params.stencilBits > 0)
	{
		gl.glGenTextures(1, &m_FakeBB_DepthStencil);
		gl.glBindTexture(eGL_TEXTURE_2D, m_FakeBB_DepthStencil);

		GLenum depthfmt = eGL_DEPTH32F_STENCIL8;
		bool stencil = false;

		if(params.stencilBits == 8)
		{
			stencil = true;

			if(params.depthBits == 32)
				depthfmt = eGL_DEPTH32F_STENCIL8;
			else if(params.depthBits == 24)
				depthfmt = eGL_DEPTH24_STENCIL8;
			else
				RDCERR("Unexpected combination of depth & stencil bits: %d & %d", params.depthBits, params.stencilBits);
		}
		else if(params.stencilBits == 0)
		{
			if(params.depthBits == 32)
				depthfmt = eGL_DEPTH_COMPONENT32F;
			else if(params.depthBits == 24)
				depthfmt = eGL_DEPTH_COMPONENT24;
			else if(params.depthBits == 16)
				depthfmt = eGL_DEPTH_COMPONENT16;
			else
				RDCERR("Unexpected # depth bits: %d", params.depthBits);
		}
		else
			RDCERR("Unexpected # stencil bits: %d", params.stencilBits);
		
		if(stencil)
			gl.glObjectLabel(eGL_TEXTURE, m_FakeBB_DepthStencil, -1, "Backbuffer Depth-stencil");
		else
			gl.glObjectLabel(eGL_TEXTURE, m_FakeBB_DepthStencil, -1, "Backbuffer Depth");

		gl.glTexStorage2D(eGL_TEXTURE_2D, 1, depthfmt, params.width, params.height); 

		if(stencil)
			gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_STENCIL_ATTACHMENT, m_FakeBB_DepthStencil, 0);
		else
			gl.glFramebufferTexture(eGL_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, m_FakeBB_DepthStencil, 0);
	}
}

const char * WrappedOpenGL::GetChunkName(uint32_t idx)
{
	if(idx < FIRST_CHUNK_ID || idx >= NUM_OPENGL_CHUNKS)
		return "<unknown>";
	return GLChunkNames[idx-FIRST_CHUNK_ID];
}

WrappedOpenGL::~WrappedOpenGL()
{
	SAFE_DELETE(m_pSerialiser);

	GetResourceManager()->ReleaseCurrentResource(m_DeviceResourceID);
	GetResourceManager()->ReleaseCurrentResource(m_ContextResourceID);
	
	if(m_ContextRecord)
	{
		RDCASSERT(m_ContextRecord->GetRefCount() == 1);
		m_ContextRecord->Delete(GetResourceManager());
	}

	if(m_DeviceRecord)
	{
		RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
		m_DeviceRecord->Delete(GetResourceManager());
	}
	
	m_ResourceManager->Shutdown();

	SAFE_DELETE(m_ResourceManager);

	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

void *WrappedOpenGL::GetCtx()
{
	return m_ActiveContexts[Threading::GetCurrentID()];
}

////////////////////////////////////////////////////////////////
// Windowing/setup/etc
////////////////////////////////////////////////////////////////

void WrappedOpenGL::CreateContext(void *windowHandle, void *contextHandle, void *shareContext, GLInitParams initParams)
{
	// TODO: support multiple GL contexts more explicitly
	m_InitParams = initParams;
}

void WrappedOpenGL::ActivateContext(void *windowHandle, void *contextHandle)
{
	m_ActiveContexts[Threading::GetCurrentID()] = contextHandle;
	// TODO: support multiple GL contexts more explicitly
	Keyboard::AddInputWindow(windowHandle);
}

void WrappedOpenGL::WindowSize(void *windowHandle, uint32_t w, uint32_t h)
{
	// TODO: support multiple window handles
	m_InitParams.width = w;
	m_InitParams.height = h;
}

void WrappedOpenGL::Present(void *windowHandle)
{
	RenderDoc::Inst().SetCurrentDriver(RDC_OpenGL);

	if(m_State == WRITING_IDLE)
		RenderDoc::Inst().Tick();
	
	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame
	
	if(m_State == WRITING_IDLE)
	{
		m_FrameTimes.push_back(m_FrameTimer.GetMilliseconds());
		m_TotalTime += m_FrameTimes.back();
		m_FrameTimer.Restart();

		// update every second
		if(m_TotalTime > 1000.0)
		{
			m_MinFrametime = 10000.0;
			m_MaxFrametime = 0.0;
			m_AvgFrametime = 0.0;

			m_TotalTime = 0.0;

			for(size_t i=0; i < m_FrameTimes.size(); i++)
			{
				m_AvgFrametime += m_FrameTimes[i];
				if(m_FrameTimes[i] < m_MinFrametime)
					m_MinFrametime = m_FrameTimes[i];
				if(m_FrameTimes[i] > m_MaxFrametime)
					m_MaxFrametime = m_FrameTimes[i];
			}

			m_AvgFrametime /= double(m_FrameTimes.size());

			m_FrameTimes.clear();

			RDCLOG("Frame: %d. F12/PrtScr to capture. %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)",
				m_FrameCounter, m_AvgFrametime, m_MinFrametime, m_MaxFrametime, 1000.0f/m_AvgFrametime);
			for(size_t i=0; i < m_FrameRecord.size(); i++)
				RDCLOG("Captured Frame %d. Multiple frame capture not supported.\n", m_FrameRecord[i].frameInfo.frameNumber);
#if !defined(RELEASE)
			RDCLOG("%llu chunks - %.2f MB", Chunk::NumLiveChunks(), float(Chunk::TotalMem())/1024.0f/1024.0f);
#endif
		}
	}

	// kill any current capture
	if(m_State == WRITING_CAPFRAME)
	{
		//if(HasSuccessfulCapture())
		{
			RDCLOG("Finished capture, Frame %u", m_FrameCounter);

			EndCaptureFrame();
			FinishCapture();
			
			const uint32_t maxSize = 1024;

			byte *thpixels = NULL;
			uint32_t thwidth = 0;
			uint32_t thheight = 0;

			if(m_Real.glGetIntegerv && m_Real.glReadBuffer && m_Real.glBindFramebuffer && m_Real.glBindBuffer && m_Real.glReadPixels)
			{
				RDCGLenum prevReadBuf = eGL_BACK;
				GLint prevBuf = 0;
				GLint packBufBind = 0;
				GLint prevPackRowLen = 0;
				GLint prevPackSkipRows = 0;
				GLint prevPackSkipPixels = 0;
				GLint prevPackAlignment = 0;
				m_Real.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&prevReadBuf);
				m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &prevBuf);
				m_Real.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, &packBufBind);
				m_Real.glGetIntegerv(eGL_PACK_ROW_LENGTH, &prevPackRowLen);
				m_Real.glGetIntegerv(eGL_PACK_SKIP_ROWS, &prevPackSkipRows);
				m_Real.glGetIntegerv(eGL_PACK_SKIP_PIXELS, &prevPackSkipPixels);
				m_Real.glGetIntegerv(eGL_PACK_ALIGNMENT, &prevPackAlignment);

				m_Real.glReadBuffer(eGL_BACK);
				m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, 0);
				m_Real.glBindBuffer(eGL_PIXEL_PACK_BUFFER, 0);
				m_Real.glPixelStorei(eGL_PACK_ROW_LENGTH, 0);
				m_Real.glPixelStorei(eGL_PACK_SKIP_ROWS, 0);
				m_Real.glPixelStorei(eGL_PACK_SKIP_PIXELS, 0);
				m_Real.glPixelStorei(eGL_PACK_ALIGNMENT, 1);

				thwidth = m_InitParams.width;
				thheight = m_InitParams.height;

				thpixels = new byte[thwidth*thheight*3];

				m_Real.glReadPixels(0, 0, thwidth, thheight, eGL_RGB, eGL_UNSIGNED_BYTE, thpixels);

				for(uint32_t y=0; y <= thheight/2; y++)
				{
					for(uint32_t x=0; x < thwidth; x++)
					{
						byte save[3];
						save[0] = thpixels[y*(thwidth*3) + x*3 + 0];
						save[1] = thpixels[y*(thwidth*3) + x*3 + 1];
						save[2] = thpixels[y*(thwidth*3) + x*3 + 2];
						
						thpixels[y*(thwidth*3) + x*3 + 0] = thpixels[(thheight-1-y)*(thwidth*3) + x*3 + 0];
						thpixels[y*(thwidth*3) + x*3 + 1] = thpixels[(thheight-1-y)*(thwidth*3) + x*3 + 1];
						thpixels[y*(thwidth*3) + x*3 + 2] = thpixels[(thheight-1-y)*(thwidth*3) + x*3 + 2];
						
						thpixels[(thheight-1-y)*(thwidth*3) + x*3 + 0] = save[0];
						thpixels[(thheight-1-y)*(thwidth*3) + x*3 + 1] = save[1];
						thpixels[(thheight-1-y)*(thwidth*3) + x*3 + 2] = save[2];
					}
				}

				m_Real.glBindBuffer(eGL_PIXEL_PACK_BUFFER, packBufBind);
				m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevBuf);
				m_Real.glReadBuffer(prevReadBuf);
				m_Real.glPixelStorei(eGL_PACK_ROW_LENGTH, prevPackRowLen);
				m_Real.glPixelStorei(eGL_PACK_SKIP_ROWS, prevPackSkipRows);
				m_Real.glPixelStorei(eGL_PACK_SKIP_PIXELS, prevPackSkipPixels);
				m_Real.glPixelStorei(eGL_PACK_ALIGNMENT, prevPackAlignment);
			}
			
			byte *jpgbuf = NULL;
			int len = thwidth*thheight;

			if(len > 0)
			{
				jpgbuf = new byte[len];

				jpge::params p;

				p.m_quality = 40;

				bool success = jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

				if(!success)
				{
					RDCERR("Failed to compress to jpg");
					SAFE_DELETE_ARRAY(jpgbuf);
					thwidth = 0;
					thheight = 0;
				}
			}

			Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

			{
				SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

				SERIALISE_ELEMENT(ResourceId, immContextId, m_ContextResourceID);

				m_pFileSerialiser->Insert(scope.Get(true));
			}

			RDCDEBUG("Inserting Resource Serialisers");	

			GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);
			
			GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

			RDCDEBUG("Creating Capture Scope");	

			{
				SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

				Serialise_CaptureScope(0);

				m_pFileSerialiser->Insert(scope.Get(true));
			}

			{
				RDCDEBUG("Getting Resource Record");	

				GLResourceRecord *record = m_ResourceManager->GetResourceRecord(m_ContextResourceID);

				RDCDEBUG("Accumulating context resource list");	

				map<int32_t, Chunk *> recordlist;
				record->Insert(recordlist);

				RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());	

				for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
					m_pFileSerialiser->Insert(it->second);

				RDCDEBUG("Done");	
			}

			m_CurFileSize += m_pFileSerialiser->FlushToDisk();

			RenderDoc::Inst().SuccessfullyWrittenLog();

			SAFE_DELETE(m_pFileSerialiser);

			m_State = WRITING_IDLE;
			
			GetResourceManager()->MarkUnwrittenResources();

			GetResourceManager()->ClearReferencedResources();
		}
	}

	if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE && m_FrameRecord.empty())
	{
		m_State = WRITING_CAPFRAME;

		FetchFrameRecord record;
		record.frameInfo.frameNumber = m_FrameCounter+1;
		m_FrameRecord.push_back(record);

		GetResourceManager()->ClearReferencedResources();

		GetResourceManager()->MarkResourceFrameReferenced(m_DeviceResourceID, eFrameRef_Write);
		GetResourceManager()->PrepareInitialContents();
		
		AttemptCapture();
		BeginCaptureFrame();

		RDCLOG("Starting capture, frame %u", m_FrameCounter);
	}
}

void WrappedOpenGL::Serialise_CaptureScope(uint64_t offset)
{
	SERIALISE_ELEMENT(uint32_t, FrameNumber, m_FrameCounter);

	if(m_State >= WRITING)
	{
		GetResourceManager()->Serialise_InitialContentsNeeded();
	}
	else
	{
		FetchFrameRecord record;
		record.frameInfo.fileOffset = offset;
		record.frameInfo.firstEvent = 1;//m_pImmediateContext->GetEventID();
		record.frameInfo.frameNumber = FrameNumber;
		record.frameInfo.immContextId = GetResourceManager()->GetOriginalID(m_ContextResourceID);
		m_FrameRecord.push_back(record);

		GetResourceManager()->CreateInitialContents();
	}
}

void WrappedOpenGL::EndCaptureFrame()
{
	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);
	
	bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
	m_pSerialiser->Serialise("HasCallstack", HasCallstack);	

	if(HasCallstack)
	{
		Callstack::Stackwalk *call = Callstack::Collect();

		RDCASSERT(call->NumLevels() < 0xff);

		size_t numLevels = call->NumLevels();
		uint64_t *stack = call->GetAddrs();

		m_pSerialiser->Serialise("callstack", stack, numLevels);

		delete call;
	}

	m_ContextRecord->AddChunk(scope.Get());
}

void WrappedOpenGL::AttemptCapture()
{
	m_State = WRITING_CAPFRAME;

	{
		RDCDEBUG("Immediate Context %llu Attempting capture", GetContextResourceID());

		//m_SuccessfulCapture = true;

		m_ContextRecord->LockChunks();
		while(m_ContextRecord->HasChunks())
		{
			Chunk *chunk = m_ContextRecord->GetLastChunk();

			SAFE_DELETE(chunk);
			m_ContextRecord->PopChunk();
		}
		m_ContextRecord->UnlockChunks();
	}
}

bool WrappedOpenGL::Serialise_BeginCaptureFrame(bool applyInitialState)
{
	GLRenderState state(&m_Real, m_pSerialiser);

	if(m_State >= WRITING)
	{
		state.FetchState();
	}

	state.Serialise(m_State, GetCtx(), this);

	if(m_State <= EXECUTING && applyInitialState)
	{
		m_DoStateVerify = false;
		state.ApplyState();
		m_DoStateVerify = true;
	}

	return true;
}
	
void WrappedOpenGL::BeginCaptureFrame()
{
	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);

	Serialise_BeginCaptureFrame(false);

	m_ContextRecord->AddChunk(scope.Get(), 1);
}

void WrappedOpenGL::FinishCapture()
{
	m_State = WRITING_IDLE;

	//m_SuccessfulCapture = false;
}

void WrappedOpenGL::DebugSnoop(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message)
{
	RDCLOG("Got a Debug message from %hs, type %hs, ID %d, severity %hs:\n'%hs'",
				ToStr::Get(source).c_str(), ToStr::Get(type).c_str(), id, ToStr::Get(severity).c_str(), message);

	if(m_RealDebugFunc)
		m_RealDebugFunc(source, type, id, severity, length, message, m_RealDebugFuncParam);
}

void WrappedOpenGL::ReadLogInitialisation()
{
	uint64_t lastFrame = 0;
	uint64_t firstFrame = 0;

	m_pSerialiser->SetDebugText(true);

	m_pSerialiser->Rewind();

	while(!m_pSerialiser->AtEnd())
	{
		m_pSerialiser->SkipToChunk(CAPTURE_SCOPE);

		// found a capture chunk
		if(!m_pSerialiser->AtEnd())
		{
			lastFrame = m_pSerialiser->GetOffset();
			if(firstFrame == 0)
				firstFrame = m_pSerialiser->GetOffset();

			// skip this chunk
			m_pSerialiser->PushContext(NULL, CAPTURE_SCOPE, false);
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(NULL, CAPTURE_SCOPE);
		}
	}

	m_pSerialiser->Rewind();

	int chunkIdx = 0;

	struct chunkinfo
	{
		chunkinfo() : count(0), total(0.0) {}
		int count;
		double total;
	};

	map<GLChunkType,chunkinfo> chunkInfos;

	SCOPED_TIMER("chunk initialisation");

	while(1)
	{
		PerformanceTimer timer;

		uint64_t offset = m_pSerialiser->GetOffset();

		GLChunkType context = (GLChunkType)m_pSerialiser->PushContext(NULL, 1, false);

		chunkIdx++;

		ProcessChunk(offset, context);

		m_pSerialiser->PopContext(NULL, context);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(m_pSerialiser->GetOffset())/float(m_pSerialiser->GetSize()));

		if(context == CAPTURE_SCOPE)
		{
			ContextReplayLog(READING, 0, 0, false);

			if(m_pSerialiser->GetOffset() > lastFrame)
				break;
		}

		chunkInfos[context].total += timer.GetMilliseconds();
		chunkInfos[context].count++;

		if(m_pSerialiser->AtEnd())
		{
			break;
		}
	}

	for(auto it=chunkInfos.begin(); it != chunkInfos.end(); ++it)
	{
		RDCDEBUG("%hs: %.3f total time in %d chunks - %.3f average",
				GetChunkName(it->first), it->second.total, it->second.count,
				it->second.total/double(it->second.count));
	}

	RDCDEBUG("Allocating %llu persistant bytes of memory for the log.", m_pSerialiser->GetSize() - firstFrame);
	
	m_pSerialiser->SetDebugText(false);
	
	m_pSerialiser->SetBase(firstFrame);
}

void WrappedOpenGL::ProcessChunk(uint64_t offset, GLChunkType context)
{
	switch(context)
	{
	case DEVICE_INIT:
		{
			SERIALISE_ELEMENT(ResourceId, immContextId, ResourceId());

			m_ResourceManager->AddLiveResource(immContextId, GLResource(NULL, eResSpecial, eSpecialResContext));
			break;
		}
	case GEN_TEXTURE:
		Serialise_glGenTextures(0, NULL);
		break;
	case ACTIVE_TEXTURE:
		Serialise_glActiveTexture(eGL_NONE);
		break;
	case BIND_TEXTURE:
		Serialise_glBindTexture(eGL_NONE, 0);
		break;
	case TEXSTORAGE1D:
		Serialise_glTextureStorage1DEXT(0, eGL_NONE, 0, eGL_NONE, 0);
		break;
	case TEXSTORAGE2D:
		Serialise_glTextureStorage2DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0);
		break;
	case TEXSTORAGE3D:
		Serialise_glTextureStorage3DEXT(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0);
		break;
	case TEXSUBIMAGE1D:
		Serialise_glTextureSubImage1DEXT(0, eGL_NONE, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
		break;
	case TEXSUBIMAGE2D:
		Serialise_glTextureSubImage2DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
		break;
	case TEXSUBIMAGE3D:
		Serialise_glTextureSubImage3DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, eGL_NONE, NULL);
		break;
	case TEXSUBIMAGE1D_COMPRESSED:
		Serialise_glCompressedTextureSubImage1DEXT(0, eGL_NONE, 0, 0, 0, eGL_NONE, 0, NULL);
		break;
	case TEXSUBIMAGE2D_COMPRESSED:
		Serialise_glCompressedTextureSubImage2DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, NULL);
		break;
	case TEXSUBIMAGE3D_COMPRESSED:
		Serialise_glCompressedTextureSubImage3DEXT(0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0, eGL_NONE, 0, NULL);
		break;
	case TEXBUFFER_RANGE:
		Serialise_glTextureBufferRangeEXT(0, eGL_NONE, eGL_NONE, 0, 0, 0);
		break;
	case PIXELSTORE:
		Serialise_glPixelStorei(eGL_NONE, 0);
		break;
	case TEXPARAMETERF:
		Serialise_glTextureParameterfEXT(0, eGL_NONE, eGL_NONE, 0);
		break;
	case TEXPARAMETERFV:
		Serialise_glTextureParameterfvEXT(0, eGL_NONE, eGL_NONE, NULL);
		break;
	case TEXPARAMETERI:
		Serialise_glTextureParameteriEXT(0, eGL_NONE, eGL_NONE, 0);
		break;
	case TEXPARAMETERIV:
		Serialise_glTextureParameterivEXT(0, eGL_NONE, eGL_NONE, NULL);
		break;
	case GENERATE_MIPMAP:
		Serialise_glGenerateTextureMipmapEXT(0, eGL_NONE);
		break;
	case COPY_SUBIMAGE:
		Serialise_glCopyImageSubData(0, eGL_NONE, 0, 0, 0, 0, 0, eGL_NONE, 0, 0, 0, 0, 0, 0, 0);
		break;
	case TEXTURE_VIEW:
		Serialise_glTextureView(0, eGL_NONE, 0, eGL_NONE, 0, 0, 0, 0);
		break;

	case CREATE_SHADER:
		Serialise_glCreateShader(0, eGL_NONE);
		break;
	case CREATE_PROGRAM:
		Serialise_glCreateProgram(0);
		break;
	case CREATE_SHADERPROGRAM:
		Serialise_glCreateShaderProgramv(0, eGL_NONE, 0, NULL);
		break;
	case COMPILESHADER:
		Serialise_glCompileShader(0);
		break;
	case SHADERSOURCE:
		Serialise_glShaderSource(0, 0, NULL, NULL);
		break;
	case ATTACHSHADER:
		Serialise_glAttachShader(0, 0);
		break;
	case DETACHSHADER:
		Serialise_glDetachShader(0, 0);
		break;
	case USEPROGRAM:
		Serialise_glUseProgram(0);
		break;
	case PROGRAMPARAMETER:
		Serialise_glProgramParameteri(0, eGL_NONE, 0);
		break;
	case BINDATTRIB_LOCATION:
		Serialise_glBindAttribLocation(0, 0, NULL);
		break;
	case UNIFORM_BLOCKBIND:
		Serialise_glUniformBlockBinding(0, 0, 0);
		break;
	case PROGRAMUNIFORM_VECTOR:
		Serialise_glProgramUniformVector(0, eGL_NONE, 0, 0, UNIFORM_UNKNOWN);
		break;
	case LINKPROGRAM:
		Serialise_glLinkProgram(0);
		break;
		
	case GEN_PROGRAMPIPE:
		Serialise_glGenProgramPipelines(0, NULL);
		break;
	case USE_PROGRAMSTAGES:
		Serialise_glUseProgramStages(0, 0, 0);
		break;
	case BIND_PROGRAMPIPE:
		Serialise_glBindProgramPipeline(0);
		break;
		
	case FENCE_SYNC:
		Serialise_glFenceSync(NULL, eGL_NONE, 0);
		break;
	case CLIENTWAIT_SYNC:
		Serialise_glClientWaitSync(NULL, 0, 0);
		break;
	case WAIT_SYNC:
		Serialise_glWaitSync(NULL, 0, 0);
		break;
		
	case GEN_QUERIES:
		Serialise_glGenQueries(0, NULL);
		break;
	case BEGIN_QUERY:
		Serialise_glBeginQuery(eGL_NONE, 0);
		break;
	case END_QUERY:
		Serialise_glEndQuery(eGL_NONE);
		break;

	case CLEAR_COLOR:
		Serialise_glClearColor(0, 0, 0, 0);
		break;
	case CLEAR_DEPTH:
		Serialise_glClearDepth(0);
		break;
	case CLEAR:
		Serialise_glClear(0);
		break;
	case CLEARBUFFERF:
		Serialise_glClearBufferfv(eGL_NONE, 0, NULL);
		break;
	case CLEARBUFFERI:
		Serialise_glClearBufferiv(eGL_NONE, 0, NULL);
		break;
	case CLEARBUFFERUI:
		Serialise_glClearBufferuiv(eGL_NONE, 0, NULL);
		break;
	case CLEARBUFFERFI:
		Serialise_glClearBufferfi(eGL_NONE, 0, 0, 0);
		break;
	case POLYGON_MODE:
		Serialise_glPolygonMode(eGL_NONE, eGL_NONE);
		break;
	case POLYGON_OFFSET:
		Serialise_glPolygonOffset(0, 0);
		break;
	case CULL_FACE:
		Serialise_glCullFace(eGL_NONE);
		break;
	case HINT:
		Serialise_glHint(eGL_NONE, eGL_NONE);
		break;
	case ENABLE:
		Serialise_glEnable(eGL_NONE);
		break;
	case DISABLE:
		Serialise_glDisable(eGL_NONE);
		break;
	case ENABLEI:
		Serialise_glEnablei(eGL_NONE, 0);
		break;
	case DISABLEI:
		Serialise_glDisablei(eGL_NONE, 0);
		break;
	case FRONT_FACE:
		Serialise_glFrontFace(eGL_NONE);
		break;
	case BLEND_FUNC:
		Serialise_glBlendFunc(eGL_NONE, eGL_NONE);
		break;
	case BLEND_FUNCI:
		Serialise_glBlendFunci(0, eGL_NONE, eGL_NONE);
		break;
	case BLEND_COLOR:
		Serialise_glBlendColor(0, 0, 0, 0);
		break;
	case BLEND_FUNC_SEP:
		Serialise_glBlendFuncSeparate(eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
		break;
	case BLEND_FUNC_SEPI:
		Serialise_glBlendFuncSeparatei(0, eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
		break;
	case BLEND_EQ_SEP:
		Serialise_glBlendEquationSeparate(eGL_NONE, eGL_NONE);
		break;
	case BLEND_EQ_SEPI:
		Serialise_glBlendEquationSeparatei(0, eGL_NONE, eGL_NONE);
		break;

	case STENCIL_OP:
		Serialise_glStencilOp(eGL_NONE, eGL_NONE, eGL_NONE);
		break;
	case STENCIL_OP_SEP:
		Serialise_glStencilOpSeparate(eGL_NONE, eGL_NONE, eGL_NONE, eGL_NONE);
		break;
	case STENCIL_FUNC:
		Serialise_glStencilFunc(eGL_NONE, 0, 0);
		break;
	case STENCIL_FUNC_SEP:
		Serialise_glStencilFuncSeparate(eGL_NONE, eGL_NONE, 0, 0);
		break;
	case STENCIL_MASK:
		Serialise_glStencilMask(0);
		break;
	case STENCIL_MASK_SEP:
		Serialise_glStencilMaskSeparate(eGL_NONE, 0);
		break;

	case COLOR_MASK:
		Serialise_glColorMask(0, 0, 0, 0);
		break;
	case COLOR_MASKI:
		Serialise_glColorMaski(0, 0, 0, 0, 0);
		break;
	case SAMPLE_MASK:
		Serialise_glSampleMaski(0, 0);
		break;
	case DEPTH_FUNC:
		Serialise_glDepthFunc(eGL_NONE);
		break;
	case DEPTH_MASK:
		Serialise_glDepthMask(0);
		break;
	case DEPTH_RANGE:
		Serialise_glDepthRange(0, 0);
		break;
	case DEPTH_RANGEF:
		Serialise_glDepthRangef(0, 0);
		break;
	case DEPTH_RANGEARRAY:
		Serialise_glDepthRangeArrayv(0, 0, NULL);
		break;
	case DEPTH_BOUNDS:
		Serialise_glDepthBoundsEXT(0, 0);
		break;
	case PATCH_PARAMI:
		Serialise_glPatchParameteri(eGL_NONE, 0);
		break;
	case PATCH_PARAMFV:
		Serialise_glPatchParameterfv(eGL_NONE, NULL);
		break;
	case VIEWPORT:
		Serialise_glViewport(0, 0, 0, 0);
		break;
	case VIEWPORT_ARRAY:
		Serialise_glViewportArrayv(0, 0, 0);
		break;
	case SCISSOR:
		Serialise_glScissor(0, 0, 0, 0);
		break;
	case SCISSOR_ARRAY:
		Serialise_glScissorArrayv(0, 0, 0);
		break;
	case BINDVERTEXARRAY:
		Serialise_glBindVertexArray(0);
		break;
	case BINDVERTEXBUFFER:
		Serialise_glBindVertexBuffer(0, 0, 0, 0);
		break;
	case VERTEXDIVISOR:
		Serialise_glVertexBindingDivisor(0, 0);
		break;
	case UNIFORM_MATRIX:
		Serialise_glUniformMatrix(0, 0, 0, NULL, UNIFORM_UNKNOWN);
		break;
	case UNIFORM_VECTOR:
		Serialise_glUniformVector(0, 0, NULL, UNIFORM_UNKNOWN);
		break;
	case DRAWARRAYS:
		Serialise_glDrawArrays(eGL_NONE, 0, 0);
		break;
	case DRAWARRAYS_INSTANCED:
		Serialise_glDrawArraysInstanced(eGL_NONE, 0, 0, 0);
		break;
	case DRAWARRAYS_INSTANCEDBASEINSTANCE:
		Serialise_glDrawArraysInstancedBaseInstance(eGL_NONE, 0, 0, 0, 0);
		break;
	case DRAWELEMENTS:
		Serialise_glDrawElements(eGL_NONE, 0, eGL_NONE, NULL);
		break;
	case DRAWRANGEELEMENTS:
		Serialise_glDrawRangeElements(eGL_NONE, 0, 0, 0, eGL_NONE, NULL);
		break;
	case DRAWELEMENTS_INSTANCED:
		Serialise_glDrawElementsInstanced(eGL_NONE, 0, eGL_NONE, NULL, 0);
		break;
	case DRAWELEMENTS_INSTANCEDBASEINSTANCE:
		Serialise_glDrawElementsInstancedBaseInstance(eGL_NONE, 0, eGL_NONE, NULL, 0, 0);
		break;
	case DRAWELEMENTS_BASEVERTEX:
		Serialise_glDrawElementsBaseVertex(eGL_NONE, 0, eGL_NONE, NULL, 0);
		break;
	case DRAWELEMENTS_INSTANCEDBASEVERTEX:
		Serialise_glDrawElementsInstancedBaseVertex(eGL_NONE, 0, eGL_NONE, NULL, 0, 0);
		break;
	case DRAWELEMENTS_INSTANCEDBASEVERTEXBASEINSTANCE:
		Serialise_glDrawElementsInstancedBaseVertexBaseInstance(eGL_NONE, 0, eGL_NONE, NULL, 0, 0, 0);
		break;
		
	case GEN_FRAMEBUFFERS:
		Serialise_glGenFramebuffers(0, NULL);
		break;
	case FRAMEBUFFER_TEX:
		Serialise_glNamedFramebufferTextureEXT(0, eGL_NONE, 0, 0);
		break;
	case FRAMEBUFFER_TEX2D:
		Serialise_glNamedFramebufferTexture2DEXT(0, eGL_NONE, eGL_NONE, 0, 0);
		break;
	case FRAMEBUFFER_TEXLAYER:
		Serialise_glNamedFramebufferTextureLayerEXT(0, eGL_NONE, 0, 0, 0);
		break;
	case READ_BUFFER:
		Serialise_glReadBuffer(eGL_NONE);
		break;
	case BIND_FRAMEBUFFER:
		Serialise_glBindFramebuffer(eGL_NONE, 0);
		break;
	case DRAW_BUFFER:
		Serialise_glDrawBuffer(eGL_NONE);
		break;
	case DRAW_BUFFERS:
		Serialise_glFramebufferDrawBuffersEXT(0, 0, NULL);
		break;
	case BLIT_FRAMEBUFFER:
		Serialise_glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, 0, eGL_NONE);
		break;
		
	case GEN_SAMPLERS:
		Serialise_glGenSamplers(0, NULL);
		break;
	case SAMPLER_PARAMETERI:
		Serialise_glSamplerParameteri(0, eGL_NONE, 0);
		break;
	case SAMPLER_PARAMETERF:
		Serialise_glSamplerParameterf(0, eGL_NONE, 0);
		break;
	case SAMPLER_PARAMETERIV:
		Serialise_glSamplerParameteriv(0, eGL_NONE, NULL);
		break;
	case SAMPLER_PARAMETERFV:
		Serialise_glSamplerParameterfv(0, eGL_NONE, NULL);
		break;
	case SAMPLER_PARAMETERIIV:
		Serialise_glSamplerParameterIiv(0, eGL_NONE, NULL);
		break;
	case SAMPLER_PARAMETERIUIV:
		Serialise_glSamplerParameterIuiv(0, eGL_NONE, NULL);
		break;
	case BIND_SAMPLER:
		Serialise_glBindSampler(0, 0);
		break;
		
	case GEN_BUFFER:
		Serialise_glGenBuffers(0, NULL);
		break;
	case BIND_BUFFER:
		Serialise_glBindBuffer(eGL_NONE, 0);
		break;
	case BIND_BUFFER_BASE:
		Serialise_glBindBufferBase(eGL_NONE, 0, 0);
		break;
	case BIND_BUFFER_RANGE:
		Serialise_glBindBufferRange(eGL_NONE, 0, 0, 0, 0);
		break;
	case BUFFERSTORAGE:
		Serialise_glNamedBufferStorageEXT(0, 0, NULL, 0);
		break;
	case BUFFERDATA:
		Serialise_glNamedBufferDataEXT(eGL_NONE, 0, NULL, eGL_NONE);
		break;
	case BUFFERSUBDATA:
		Serialise_glNamedBufferSubDataEXT(0, 0, 0, NULL);
		break;
	case COPYBUFFERSUBDATA:
		Serialise_glNamedCopyBufferSubDataEXT(0, 0, 0, 0, 0);
		break;
	case UNMAP:
		Serialise_glUnmapNamedBufferEXT(eGL_NONE);
		break;
	case GEN_VERTEXARRAY:
		Serialise_glGenVertexArrays(0, NULL);
		break;
	case BIND_VERTEXARRAY:
		Serialise_glBindVertexArray(0);
		break;
	case VERTEXATTRIBPOINTER:
		Serialise_glVertexAttribPointer(0, 0, eGL_NONE, 0, 0, NULL);
		break;
	case VERTEXATTRIBIPOINTER:
		Serialise_glVertexAttribIPointer(0, 0, eGL_NONE, 0, NULL);
		break;
	case ENABLEVERTEXATTRIBARRAY:
		Serialise_glEnableVertexAttribArray(0);
		break;
	case DISABLEVERTEXATTRIBARRAY:
		Serialise_glDisableVertexAttribArray(0);
		break;
	case VERTEXATTRIBFORMAT:
		Serialise_glVertexAttribFormat(0, 0, eGL_NONE, 0, 0);
		break;
	case VERTEXATTRIBIFORMAT:
		Serialise_glVertexAttribIFormat(0, 0, eGL_NONE, 0);
		break;
	case VERTEXATTRIBBINDING:
		Serialise_glVertexAttribBinding(0, 0);
		break;

	case OBJECT_LABEL:
		Serialise_glObjectLabel(eGL_NONE, 0, 0, NULL);
		break;
	case BEGIN_EVENT:
		Serialise_glPushDebugGroup(eGL_NONE, 0, 0, NULL);
		break;
	case SET_MARKER:
		Serialise_glDebugMessageInsert(eGL_NONE, eGL_NONE, 0, eGL_NONE, 0, NULL);
		break;
	case END_EVENT:
		Serialise_glPopDebugGroup();
		break;

	case CAPTURE_SCOPE:
		Serialise_CaptureScope(offset);
		break;
	case CONTEXT_CAPTURE_FOOTER:
		{
			bool HasCallstack = false;
			m_pSerialiser->Serialise("HasCallstack", HasCallstack);	

			if(HasCallstack)
			{
				size_t numLevels = 0;
				uint64_t *stack = NULL;

				m_pSerialiser->Serialise("callstack", stack, numLevels);

				m_pSerialiser->SetCallstack(stack, numLevels);

				SAFE_DELETE_ARRAY(stack);
			}

			if(m_State == READING)
			{
				AddEvent(CONTEXT_CAPTURE_FOOTER, "SwapBuffers()");

				FetchDrawcall draw;
				draw.name = L"SwapBuffers()";
				draw.flags |= eDraw_Present;

				AddDrawcall(draw, true);
			}
		}
		break;
	default:
		// ignore system chunks
		if((int)context == (int)INITIAL_CONTENTS)
			GetResourceManager()->Serialise_InitialState(GLResource(MakeNullResource));
		else if((int)context < (int)FIRST_CHUNK_ID)
			m_pSerialiser->SkipCurrentChunk();
		else
			RDCERR("Unrecognised Chunk type %d", context);
		break;
	}
}

void WrappedOpenGL::ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial)
{
	m_State = readType;

	m_DoStateVerify = true;

	GLChunkType header = (GLChunkType)m_pSerialiser->PushContext(NULL, 1, false);
	RDCASSERT(header == CONTEXT_CAPTURE_HEADER);

	WrappedOpenGL *context = this;

	Serialise_BeginCaptureFrame(!partial);

	m_pSerialiser->PopContext(NULL, header);

	m_CurEvents.clear();
	
	if(m_State == EXECUTING)
	{
		FetchAPIEvent ev = GetEvent(startEventID);
		m_CurEventID = ev.eventID;
		m_pSerialiser->SetOffset(ev.fileOffset);
	}
	else if(m_State == READING)
	{
		m_CurEventID = 1;
	}

	if(m_State == EXECUTING)
	{
	}

	GetResourceManager()->MarkInFrame(true);

	while(1)
	{
		if(m_State == EXECUTING && m_CurEventID > endEventID)
		{
			// we can just break out if we've done all the events desired.
			break;
		}

		uint64_t offset = m_pSerialiser->GetOffset();

		GLChunkType context = (GLChunkType)m_pSerialiser->PushContext(NULL, 1, false);

		ContextProcessChunk(offset, context, false);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));
		
		// for now just abort after capture scope. Really we'd need to support multiple frames
		// but for now this will do.
		if(context == CONTEXT_CAPTURE_FOOTER)
			break;
		
		m_CurEventID++;
	}

	if(m_State == READING)
	{
		GetFrameRecord().back().drawcallList = m_ParentDrawcall.Bake();

		m_ParentDrawcall.children.clear();
	}

	GetResourceManager()->MarkInFrame(false);

	m_State = READING;

	m_DoStateVerify = false;
}

void WrappedOpenGL::ContextProcessChunk(uint64_t offset, GLChunkType chunk, bool forceExecute)
{
	/*
	if(chunk < FIRST_CONTEXT_CHUNK && !forceExecute)
	{
		if(m_State == READING)
		{
			GetResourceManager()->MarkInFrame(false);

			ProcessChunk(offset, chunk);
			m_pSerialiser->PopContext(NULL, chunk);

			GetResourceManager()->MarkInFrame(true);
		}
		else if(m_State == EXECUTING)
		{
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(NULL, chunk);
		}
		return;
	}*/

	m_CurChunkOffset = offset;

	uint64_t cOffs = m_pSerialiser->GetOffset();

	WrappedOpenGL *context = this;

	LogState state = context->m_State;

	if(forceExecute)
		context->m_State = EXECUTING;
	else
		context->m_State = m_State;

	m_AddedDrawcall = false;

	ProcessChunk(offset, chunk);

	m_pSerialiser->PopContext(NULL, chunk);
	
	if(context->m_State == READING && chunk == SET_MARKER)
	{
		// no push/pop necessary
	}
	else if(context->m_State == READING && chunk == BEGIN_EVENT)
	{
		// push down the drawcallstack to the latest drawcall
		context->m_DrawcallStack.push_back(&context->m_DrawcallStack.back()->children.back());
	}
	else if(context->m_State == READING && chunk == END_EVENT)
	{
		// refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
		RDCASSERT(context->m_DrawcallStack.size() > 1);
		if(context->m_DrawcallStack.size() > 1)
			context->m_DrawcallStack.pop_back();
	}
	else if(context->m_State == READING)
	{
		if(!m_AddedDrawcall)
			context->AddEvent(chunk, m_pSerialiser->GetDebugStr());
	}

	m_AddedDrawcall = false;
	
	if(forceExecute)
		context->m_State = state;
}

void WrappedOpenGL::AddDrawcall(FetchDrawcall d, bool hasEvents)
{
	if(d.context == ResourceId()) d.context = GetResourceManager()->GetOriginalID(m_ContextResourceID);

	m_AddedDrawcall = true;

	WrappedOpenGL *context = this;

	FetchDrawcall draw = d;
	draw.eventID = m_CurEventID;
	draw.drawcallID = m_CurDrawcallID;
	
	GLuint curCol[8] = { 0 };
	GLuint curDepth = 0;

	{
		GLint numCols = 8;
		m_Real.glGetIntegerv(eGL_MAX_COLOR_ATTACHMENTS, &numCols);
		
		RDCEraseEl(draw.outputs);

		for(GLint i=0; i < RDCMIN(numCols, 8); i++)
		{
			m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, GLenum(eGL_COLOR_ATTACHMENT0+i), eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curCol[i]);
			draw.outputs[i] = GetResourceManager()->GetID(TextureRes(GetCtx(), curCol[i]));
		}

		m_Real.glGetFramebufferAttachmentParameteriv(eGL_DRAW_FRAMEBUFFER, eGL_DEPTH_ATTACHMENT, eGL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, (GLint*)&curDepth);
		draw.depthOut = GetResourceManager()->GetID(TextureRes(GetCtx(), curDepth));
	}
	
	// markers don't increment drawcall ID
	if((draw.flags & (eDraw_SetMarker|eDraw_PushMarker)) == 0)
		m_CurDrawcallID++;

	if(hasEvents)
	{
		vector<FetchAPIEvent> evs;
		evs.reserve(m_CurEvents.size());
		for(size_t i=0; i < m_CurEvents.size(); )
		{
			if(m_CurEvents[i].context == draw.context)
			{
				evs.push_back(m_CurEvents[i]);
				m_CurEvents.erase(m_CurEvents.begin()+i);
			}
			else
			{
				i++;
			}
		}

		draw.events = evs;
	}

	//AddUsage(draw);
	
	// should have at least the root drawcall here, push this drawcall
	// onto the back's children list.
	if(!context->m_DrawcallStack.empty())
	{
		DrawcallTreeNode node(draw);
		node.children.insert(node.children.begin(), draw.children.elems, draw.children.elems+draw.children.count);
		context->m_DrawcallStack.back()->children.push_back(node);
	}
	else
		RDCERR("Somehow lost drawcall stack!");
}

void WrappedOpenGL::AddEvent(GLChunkType type, string description, ResourceId ctx)
{
	if(ctx == ResourceId()) ctx = GetResourceManager()->GetOriginalID(m_ContextResourceID);

	FetchAPIEvent apievent;

	apievent.context = ctx;
	apievent.fileOffset = m_CurChunkOffset;
	apievent.eventID = m_CurEventID;

	apievent.eventDesc = widen(description);

	Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
	if(stack)
	{
		create_array(apievent.callstack, stack->NumLevels());
		memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t)*stack->NumLevels());
	}

	m_CurEvents.push_back(apievent);

	if(m_State == READING)
		m_Events.push_back(apievent);
}

FetchAPIEvent WrappedOpenGL::GetEvent(uint32_t eventID)
{
	for(size_t i=m_Events.size()-1; i > 0; i--)
	{
		if(m_Events[i].eventID <= eventID)
			return m_Events[i];
	}

	return m_Events[0];
}

void WrappedOpenGL::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	RDCASSERT(frameID < (uint32_t)m_FrameRecord.size());

	uint64_t offs = m_FrameRecord[frameID].frameInfo.fileOffset;

	m_pSerialiser->SetOffset(offs);

	bool partial = true;

	if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
	{
		startEventID = m_FrameRecord[frameID].frameInfo.firstEvent;
		partial = false;
	}
	
	GLChunkType header = (GLChunkType)m_pSerialiser->PushContext(NULL, 1, false);

	RDCASSERT(header == CAPTURE_SCOPE);

	m_pSerialiser->SkipCurrentChunk();

	m_pSerialiser->PopContext(NULL, header);
	
	if(!partial)
	{
		GetResourceManager()->ApplyInitialContents();
		GetResourceManager()->ReleaseInFrameResources();
	}
	
	{
		if(replayType == eReplay_Full)
			ContextReplayLog(EXECUTING, startEventID, endEventID, partial);
		else if(replayType == eReplay_WithoutDraw)
			ContextReplayLog(EXECUTING, startEventID, RDCMAX(1U,endEventID)-1, partial);
		else if(replayType == eReplay_OnlyDraw)
			ContextReplayLog(EXECUTING, endEventID, endEventID, partial);
		else
			RDCFATAL("Unexpected replay type");
	}
}
