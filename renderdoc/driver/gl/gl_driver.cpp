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

#include "3rdparty/jpeg-compressor/jpge.h"

const char *GLChunkNames[] =
{
	"WrappedOpenGL::Initialisation",
	"glGenTextures",
	"glBindTexture",
	"glActiveTexture",
	"glTexStorage2D",
	"glTexSubImage2D",
	"glPixelStore",
	"glTexParameteri",
	"glGenerateMipmap",

	"glCreateShader",
	"glCreateProgram",
	"glCompileShader",
	"glShaderSource",
	"glAttachShader",
	"glLinkProgram",

	// legacy/immediate mode chunks
	"glLightfv",
	"glMaterialfv",
	"glGenLists",
	"glNewList",
	"glEndList",
	"glCallList",
	"glShadeModel",
	"glBegin",
	"glEnd",
	"glVertex3f",
	"glNormal3f",
	"glPushMatrix",
	"glPopMatrix",
	"glMatrixMode",
	"glLoadIdentity",
	"glFrustum",
	"glTranslatef",
	"glRotatef",
	//

	"glClearColor",
	"glClearDepth",
	"glClear",
	"glClearBufferfv",
	"glClearBufferiv",
	"glClearBufferuiv",
	"glClearBufferfi",
	"glCullFace",
	"glEnable",
	"glDisable",
	"glFrontFace",
	"glBlendFunc",
	"glBlendColor",
	"glBlendFuncSeparate",
	"glBlendFuncSeparatei",
	"glBlendEquationSeparate",
	"glBlendEquationSeparatei",
	"glColorMask",
	"glColorMaski",
	"glDepthFunc",
	"glViewport",
	"glViewportArrayv",
	"glUseProgram",
	"glBindVertexArray",
	"glUniformMatrix*",
	"glUniformVector*",
	"glDrawArrays",
	"glDrawArraysInstancedBasedInstance",

	"glGenFramebuffers",
	"glFramebufferTexture",
	"glBindFramebuffer",
	"glBlitFramebuffer",

	"glBindSampler",

	"glGenBuffers",
	"glBindBuffer",
	"glBindBufferBase",
	"glBindBufferRange",
	"glBufferData",
	"glGenVertexArrays",
	"glBindVertexArray",
	"glVertexAttribPointer",
	"glEnableVertexAttribArray",
	"glDeleteVertexArray",
	"glDeleteBuffer",
	
	"glObjectLabel",
	"PushMarker",
	"SetMarker",
	"PopMarker",

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
	m_TextureUnit = 0;
	
	m_LastIndexSize = eGL_UNKNOWN_ENUM;
	m_LastIndexOffset = 0;
	m_LastDrawMode = eGL_UNKNOWN_ENUM;

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

	m_ResourceManager = new GLResourceManager(this);

	m_DeviceResourceID = GetResourceManager()->RegisterResource(GLResource(eResSpecial, eSpecialResDevice));
	m_ContextResourceID = GetResourceManager()->RegisterResource(GLResource(eResSpecial, eSpecialResContext));

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
				GLint unpackBufBind = 0;
				m_Real.glGetIntegerv(eGL_READ_BUFFER, (GLint *)&prevReadBuf);
				m_Real.glGetIntegerv(eGL_READ_FRAMEBUFFER_BINDING, &prevBuf);
				m_Real.glGetIntegerv(eGL_PIXEL_PACK_BUFFER_BINDING, &unpackBufBind);

				m_Real.glReadBuffer(eGL_BACK);
				m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, 0);
				m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, 0);

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

				m_Real.glBindBuffer(eGL_PIXEL_UNPACK_BUFFER, unpackBufBind);
				m_Real.glBindFramebuffer(eGL_READ_FRAMEBUFFER, prevBuf);
				m_Real.glReadBuffer(prevReadBuf);
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
			
			GetResourceManager()->InsertInitialContentsChunks(m_pSerialiser, m_pFileSerialiser);

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
		GetResourceManager()->Serialise_InitialContentsNeeded(m_pSerialiser);
	}
	else
	{
		FetchFrameRecord record;
		record.frameInfo.fileOffset = offset;
		record.frameInfo.firstEvent = 1;//m_pImmediateContext->GetEventID();
		record.frameInfo.frameNumber = FrameNumber;
		record.frameInfo.immContextId = GetResourceManager()->GetOriginalID(m_ContextResourceID);
		m_FrameRecord.push_back(record);

		GetResourceManager()->CreateInitialContents(m_pSerialiser);
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

	state.Serialise(m_State, GetResourceManager());

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

void WrappedOpenGL::glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam)
{
	m_RealDebugFunc = callback;
	m_RealDebugFuncParam = userParam;

	m_Real.glDebugMessageCallback(&DebugSnoopStatic, this);
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

			m_ResourceManager->AddLiveResource(immContextId, GLResource(eResSpecial, eSpecialResContext));
			break;
		}
	case GEN_TEXTURE:
		Serialise_glGenTextures(0, NULL);
		break;
	case ACTIVE_TEXTURE:
		Serialise_glActiveTexture(eGL_UNKNOWN_ENUM);
		break;
	case BIND_TEXTURE:
		Serialise_glBindTexture(eGL_UNKNOWN_ENUM, 0);
		break;
	case TEXSTORAGE2D:
		Serialise_glTexStorage2D(eGL_UNKNOWN_ENUM, 0, eGL_UNKNOWN_ENUM, 0, 0);
		break;
	case TEXSUBIMAGE2D:
		Serialise_glTexSubImage2D(eGL_UNKNOWN_ENUM, 0, 0, 0, 0, 0, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, NULL);
		break;
	case PIXELSTORE:
		Serialise_glPixelStorei(eGL_UNKNOWN_ENUM, 0);
		break;
	case TEXPARAMETERI:
		Serialise_glTexParameteri(eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, 0);
		break;
	case GENERATE_MIPMAP:
		Serialise_glGenerateMipmap(eGL_UNKNOWN_ENUM);
		break;

	case CREATE_SHADER:
		Serialise_glCreateShader(0, eGL_UNKNOWN_ENUM);
		break;
	case CREATE_PROGRAM:
		Serialise_glCreateProgram(0);
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
	case LINKPROGRAM:
		Serialise_glLinkProgram(0);
		break;
		

	// legacy/immediate mode chunks
	case LIGHTFV:
		Serialise_glLightfv(eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, NULL);
		break;
	case MATERIALFV:
		Serialise_glMaterialfv(eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, NULL);
		break;
	case GENLISTS:
		Serialise_glGenLists(0);
		break;
	case NEWLIST:
		Serialise_glNewList(0, eGL_UNKNOWN_ENUM);
		break;
	case ENDLIST:
		Serialise_glEndList();
		break;
	case CALLLIST:
		Serialise_glCallList(0);
		break;
	case SHADEMODEL:
		Serialise_glShadeModel(eGL_UNKNOWN_ENUM);
		break;
	case BEGIN:
		Serialise_glBegin(eGL_UNKNOWN_ENUM);
		break;
	case END:
		Serialise_glEnd();
		break;
	case VERTEX3F:
		Serialise_glVertex3f(0, 0, 0);
		break;
	case NORMAL3F:
		Serialise_glNormal3f(0, 0, 0);
		break;
	case PUSHMATRIX:
		Serialise_glPushMatrix();
		break;
	case POPMATRIX:
		Serialise_glPopMatrix();
		break;
	case MATRIXMODE:
		Serialise_glMatrixMode(eGL_UNKNOWN_ENUM);
		break;
	case LOADIDENTITY:
		Serialise_glLoadIdentity();
		break;
	case FRUSTUM:
		Serialise_glFrustum(0, 0, 0, 0, 0, 0);
		break;
	case TRANSLATEF:
		Serialise_glTranslatef(0, 0, 0);
		break;
	case ROTATEF:
		Serialise_glRotatef(0, 0, 0, 0);
		break;
	//

	case BIND_FRAMEBUFFER:
		Serialise_glBindFramebuffer(eGL_UNKNOWN_ENUM, 0);
		break;
	case BLIT_FRAMEBUFFER:
		Serialise_glBlitFramebuffer(0, 0, 0, 0, 0, 0, 0, 0, 0, eGL_UNKNOWN_ENUM);
		break;

	case BIND_SAMPLER:
		Serialise_glBindSampler(0, 0);
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
	case CULL_FACE:
		Serialise_glCullFace(eGL_UNKNOWN_ENUM);
		break;
	case CLEARBUFFERF:
		Serialise_glClearBufferfv(eGL_UNKNOWN_ENUM, 0, NULL);
		break;
	case CLEARBUFFERI:
		Serialise_glClearBufferiv(eGL_UNKNOWN_ENUM, 0, NULL);
		break;
	case CLEARBUFFERUI:
		Serialise_glClearBufferuiv(eGL_UNKNOWN_ENUM, 0, NULL);
		break;
	case CLEARBUFFERFI:
		Serialise_glClearBufferfi(eGL_UNKNOWN_ENUM, 0, 0, 0);
		break;
	case DISABLE:
		Serialise_glDisable(eGL_UNKNOWN_ENUM);
		break;
	case ENABLE:
		Serialise_glEnable(eGL_UNKNOWN_ENUM);
		break;
	case FRONT_FACE:
		Serialise_glFrontFace(eGL_UNKNOWN_ENUM);
		break;
	case BLEND_FUNC:
		glBlendFunc(eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM);
		break;
	case BLEND_COLOR:
		glBlendColor(0, 0, 0, 0);
		break;
	case BLEND_FUNC_SEP:
		glBlendFuncSeparate(eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM);
		break;
	case BLEND_FUNC_SEPI:
		glBlendFuncSeparatei(0, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM);
		break;
	case BLEND_EQ_SEP:
		glBlendEquationSeparate(eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM);
		break;
	case BLEND_EQ_SEPI:
		glBlendEquationSeparatei(0, eGL_UNKNOWN_ENUM, eGL_UNKNOWN_ENUM);
		break;
	case COLOR_MASK:
		glColorMask(0, 0, 0, 0);
		break;
	case COLOR_MASKI:
		glColorMaski(0, 0, 0, 0, 0);
		break;
	case DEPTH_FUNC:
		Serialise_glDepthFunc(eGL_UNKNOWN_ENUM);
		break;
	case VIEWPORT:
		Serialise_glViewport(0, 0, 0, 0);
		break;
	case VIEWPORT_ARRAY:
		Serialise_glViewportArrayv(0, 0, 0);
		break;
	case USEPROGRAM:
		Serialise_glUseProgram(0);
		break;
	case BINDVERTEXARRAY:
		Serialise_glBindVertexArray(0);
		break;
	case UNIFORM_MATRIX:
		Serialise_glUniformMatrix(0, 0, 0, NULL, UNIFORM_UNKNOWN);
		break;
	case UNIFORM_VECTOR:
		Serialise_glUniformVector(0, 0, NULL, UNIFORM_UNKNOWN);
		break;
	case DRAWARRAYS_INSTANCEDBASEDINSTANCE:
		Serialise_glDrawArraysInstancedBaseInstance(eGL_UNKNOWN_ENUM, 0, 0, 0, 0);
		break;

	case GEN_BUFFER:
		Serialise_glGenBuffers(0, NULL);
		break;
	case BIND_BUFFER:
		Serialise_glBindBuffer(eGL_UNKNOWN_ENUM, 0);
		break;
	case BIND_BUFFER_BASE:
		Serialise_glBindBufferBase(eGL_UNKNOWN_ENUM, 0, 0);
		break;
	case BIND_BUFFER_RANGE:
		Serialise_glBindBufferRange(eGL_UNKNOWN_ENUM, 0, 0, 0, 0);
		break;
	case BUFFERDATA:
		Serialise_glBufferData(eGL_UNKNOWN_ENUM, 0, NULL, eGL_UNKNOWN_ENUM);
		break;
	case GEN_VERTEXARRAY:
		Serialise_glGenVertexArrays(0, NULL);
		break;
	case BIND_VERTEXARRAY:
		Serialise_glBindVertexArray(0);
		break;
	case VERTEXATTRIBPOINTER:
		Serialise_glVertexAttribPointer(0, 0, eGL_UNKNOWN_ENUM, 0, 0, NULL);
		break;
	case ENABLEVERTEXATTRIBARRAY:
		Serialise_glEnableVertexAttribArray(0);
		break;

	case DELETE_VERTEXARRAY:
	case DELETE_BUFFER:
		RDCFATAL("Not impl");
		break;
		
	case OBJECT_LABEL:
		Serialise_glObjectLabel(eGL_UNKNOWN_ENUM, 0, 0, NULL);
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
			RDCERR("Initial contents not implemented yet");
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

	for(int i=0; i < 8; i++)
		draw.outputs[i] = ResourceId();

	draw.depthOut = ResourceId();

	GLNOTIMP("Hack, not getting current pipeline state framebufer binding");
	draw.outputs[0] = GetResourceManager()->GetID(TextureRes(m_FakeBB_Color));
	draw.depthOut = GetResourceManager()->GetID(TextureRes(m_FakeBB_DepthStencil));

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

#pragma region Get functions

GLenum WrappedOpenGL::glGetError()
{
	return m_Real.glGetError();
}

void WrappedOpenGL::glGetFloatv(GLenum pname, GLfloat *params)
{
	m_Real.glGetFloatv(pname, params);
}

void WrappedOpenGL::glGetIntegerv(GLenum pname, GLint *params)
{
	if(pname == GL_NUM_EXTENSIONS)
	{
		if(params)
			*params = (GLint)glExts.size();
		return;
	}

	m_Real.glGetIntegerv(pname, params);
}

void WrappedOpenGL::glGetIntegeri_v(GLenum pname, GLuint index, GLint *data)
{
	m_Real.glGetIntegeri_v(pname, index, data);
}

void WrappedOpenGL::glGetFloati_v(GLenum pname, GLuint index, GLfloat *data)
{
	m_Real.glGetFloati_v(pname, index, data);
}

void WrappedOpenGL::glGetInteger64i_v(GLenum pname, GLuint index, GLint64 *data)
{
	m_Real.glGetInteger64i_v(pname, index, data);
}

void WrappedOpenGL::glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params)
{
	m_Real.glGetTexLevelParameteriv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params)
{
	m_Real.glGetTexLevelParameterfv(target, level, pname, params);
}

void WrappedOpenGL::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	m_Real.glGetTexParameterfv(target, pname, params);
}

void WrappedOpenGL::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	m_Real.glGetTexParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params)
{
	m_Real.glGetInternalformativ(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params)
{
	m_Real.glGetInternalformati64v(target, internalformat, pname, bufSize, params);
}

void WrappedOpenGL::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
	m_Real.glGetBufferParameteriv(target, pname, params);
}

void WrappedOpenGL::glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
	m_Real.glGetBufferSubData(target, offset, size, data);
}

const GLubyte *WrappedOpenGL::glGetString(GLenum name)
{
	if(name == GL_EXTENSIONS)
	{
		return (const GLubyte *)glExtsString.c_str();
	}
	return m_Real.glGetString(name);
}

const GLubyte *WrappedOpenGL::glGetStringi(GLenum name, GLuint i)
{
	if(name == GL_EXTENSIONS)
	{
		if((size_t)i < glExts.size())
			return (const GLubyte *)glExts[i].c_str();

		return (const GLubyte *)"";
	}
	return m_Real.glGetStringi(name, i);
}

void WrappedOpenGL::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
	m_Real.glGetVertexAttribiv(index, pname, params);
}

void WrappedOpenGL::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
	m_Real.glGetVertexAttribPointerv(index, pname, pointer);
}

void WrappedOpenGL::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
	m_Real.glGetShaderiv(shader, pname, params);
}

void WrappedOpenGL::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	m_Real.glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
	m_Real.glGetProgramiv(program, pname, params);
}

void WrappedOpenGL::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
	m_Real.glGetProgramInfoLog(program, bufSize, length, infoLog);
}

void WrappedOpenGL::glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params)
{
	m_Real.glGetProgramInterfaceiv(program, programInterface, pname, params);
}

void WrappedOpenGL::glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params)
{
	m_Real.glGetProgramResourceiv(program, programInterface, index, propCount, props, bufSize, length, params);
}

void WrappedOpenGL::glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
	m_Real.glGetProgramResourceName(program, programInterface, index, bufSize, length, name);
}

GLint WrappedOpenGL::glGetUniformLocation(GLuint program, const GLchar *name)
{
	return m_Real.glGetUniformLocation(program, name);
}

void WrappedOpenGL::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
	m_Real.glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void WrappedOpenGL::glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
	m_Real.glGetUniformfv(program, location, params);
}

void WrappedOpenGL::glGetUniformiv(GLuint program, GLint location, GLint *params)
{
	m_Real.glGetUniformiv(program, location, params);
}

void WrappedOpenGL::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
	m_Real.glReadPixels(x, y, width, height, format, type, pixels);
}

void WrappedOpenGL::glReadBuffer(GLenum mode)
{
	m_Real.glReadBuffer(mode);
}

#pragma endregion
