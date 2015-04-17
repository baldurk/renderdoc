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


#pragma once

#include "common/common.h"
#include "common/timing.h"

#include "core/core.h"

#include "replay/replay_driver.h"

#include "gl_common.h"
#include "gl_hookset.h"
#include "gl_resources.h"
#include "gl_manager.h"
#include "gl_renderstate.h"
#include "gl_replay.h"

#include <list>
using std::list;

struct GLInitParams : public RDCInitParams
{
	GLInitParams();
	ReplayCreateStatus Serialise();

	uint32_t colorBits;
	uint32_t depthBits;
	uint32_t stencilBits;
	uint32_t isSRGB;
	uint32_t multiSamples;
	uint32_t width;
	uint32_t height;
	
	static const uint32_t GL_SERIALISE_VERSION = 0x000000C;

	// version number internal to opengl stream
	uint32_t SerialiseVersion;
};

enum CaptureFailReason
{
	CaptureSucceeded = 0,
	CaptureFailed_UncappedUnmap,
};

struct DrawcallTreeNode
{
	DrawcallTreeNode() {}
	explicit DrawcallTreeNode(FetchDrawcall d) : draw(d) {}
	FetchDrawcall draw;
	vector<DrawcallTreeNode> children;

	DrawcallTreeNode &operator =(FetchDrawcall d) { *this = DrawcallTreeNode(d); return *this; }
	
	vector<FetchDrawcall> Bake()
	{
		vector<FetchDrawcall> ret;
		if(children.empty()) return ret;

		ret.resize(children.size());
		for(size_t i=0; i < children.size(); i++)
		{
			ret[i] = children[i].draw;
			ret[i].children = children[i].Bake();
		}

		return ret;
	}
};

struct Replacement
{
	Replacement(ResourceId i, GLResource r) : id(i), res(r) {}
	ResourceId id;
	GLResource res;
};

class WrappedOpenGL : public IFrameCapturer
{
	private:
		const GLHookSet &m_Real;

		friend class GLReplay;
		friend class GLResourceManager;

		const GLHookSet &GetHookset() { return m_Real; }

		vector<DebugMessage> m_DebugMessages;
		void Serialise_DebugMessages();
		vector<DebugMessage> GetDebugMessages();

		GLDEBUGPROC m_RealDebugFunc;
		const void *m_RealDebugFuncParam;
		string m_DebugMsgContext;

		void DebugSnoop(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);
		static void APIENTRY DebugSnoopStatic(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
		{ ((WrappedOpenGL *)userParam)->DebugSnoop(source, type, id, severity, length, message); }

		// checks if the given object has tons of updates. If so it's probably
		// in the vein of "one global object, updated per-draw as necessary", or it's just
		// really high traffic, in which case we just want to save the state of it at frame
		// start, then track changes while frame capturing
		bool RecordUpdateCheck(GLResourceRecord *record);

		// internals
		Serialiser *m_pSerialiser;
		LogState m_State;
		bool m_AppControlledCapture;
		
		GLReplay m_Replay;

		GLInitParams m_InitParams;

		map<uint64_t, GLWindowingData> m_ActiveContexts;

		// we use this context if we need one e.g. for fetching initial states
		// if no context is current
		map<uint64_t, GLWindowingData> m_DefaultContexts;

		bool m_ActiveQueries[8][8]; // first index type, second index (for some, always 0)
		bool m_ActiveConditional;
		bool m_ActiveFeedback;

		ResourceId m_DeviceResourceID;
		GLResourceRecord *m_DeviceRecord;
		
		ResourceId m_ContextResourceID;
		GLResourceRecord *m_ContextRecord;

		GLResourceRecord *m_DisplayListRecord;

		GLResourceManager *m_ResourceManager;

		uint32_t m_FrameCounter;

		uint64_t m_CurFileSize;

		PerformanceTimer m_FrameTimer;
		vector<double> m_FrameTimes;
		double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;
		
		set<ResourceId> m_HighTrafficResources;

		// we store two separate sets of maps, since for an explicit glMemoryBarrier
		// we need to flush both types of maps, but for implicit sync points we only
		// want to consider coherent maps, and since that happens often we want it to
		// be as efficient as possible.
		set<GLResourceRecord*> m_CoherentMaps;
		set<GLResourceRecord*> m_PersistentMaps;

		// this function iterates over all the maps, checking for any changes between
		// the shadow pointers, and propogates that to 'real' GL
		void PersistentMapMemoryBarrier(const set<GLResourceRecord *> &maps);

		// this function is called at any point that could possibly pick up a change
		// in a coherent persistent mapped buffer, to propogate changes across. In most
		// cases hopefully m_CoherentMaps will be empty so this will amount to an inlined
		// check and jump
		inline void CoherentMapImplicitBarrier()
		{ if(!m_CoherentMaps.empty()) PersistentMapMemoryBarrier(m_CoherentMaps); }

		vector<FetchFrameRecord> m_FrameRecord;
		
		const FetchDrawcall *GetDrawcall(const FetchDrawcall *draw, uint32_t eventID);
		
		static const char *GetChunkName(uint32_t idx);
		
		// replay
		
		vector<FetchAPIEvent> m_CurEvents, m_Events;
		bool m_AddedDrawcall;

		uint64_t m_CurChunkOffset;
		uint32_t m_CurEventID, m_CurDrawcallID;
		uint32_t m_FirstEventID;
		uint32_t m_LastEventID;
		
		DrawcallTreeNode m_ParentDrawcall;

		list<DrawcallTreeNode *> m_DrawcallStack;

		map<ResourceId, vector<EventUsage> > m_ResourceUses;

		// buffer used
		vector<byte> m_ScratchBuf;

		struct BufferData
		{
			GLResource resource;
			GLenum curType;
			uint64_t size;
		};

		map<ResourceId, BufferData> m_Buffers;
		
		struct TextureData
		{
			TextureData()
				: dimension(0), width(0), height(0), depth(0), samples(0), creationFlags(0), internalFormat(eGL_NONE),
				renderbufferReadTex(0)
			{
				renderbufferFBOs[0] = renderbufferFBOs[1] = 0;
			}
			GLResource resource;
			GLenum curType;
			GLint dimension;
			GLint width, height, depth, samples;
			uint32_t creationFlags;
			GLenum internalFormat;

			// since renderbuffers cannot be read from, we have to create a texture of identical size/format,
			// and define FBOs for blitting to it - the renderbuffer is attached to the first FBO and the texture is
			// bound to the second.
			GLuint renderbufferReadTex;
			GLuint renderbufferFBOs[2];
		};

		map<ResourceId, TextureData> m_Textures;

		struct ShaderData
		{
			GLenum type;
			vector<string> sources;
			vector<string> includepaths;
			ShaderReflection reflection;
			GLuint prog;
		};

		struct ProgramData
		{
			ProgramData() : linked(false)
			{
				RDCEraseEl(stageShaders);
			}
			vector<ResourceId> shaders;

			map<GLint, GLint> locationTranslate;

			bool linked;
			ResourceId stageShaders[6];
		};
		
		struct PipelineData
		{
			PipelineData()
			{
				RDCEraseEl(stagePrograms);
				RDCEraseEl(stageShaders);
			}

			struct ProgramUse
			{
				ProgramUse(ResourceId id_, GLbitfield use_) : id(id_), use(use_) {}

				ResourceId id;
				GLbitfield use;
			};

			vector<ProgramUse> programs;
			ResourceId stagePrograms[6];
			ResourceId stageShaders[6];
		};

		map<ResourceId, ShaderData> m_Shaders;
		map<ResourceId, ProgramData> m_Programs;
		map<ResourceId, PipelineData> m_Pipelines;
		vector< pair<ResourceId, Replacement> > m_DependentReplacements;

		GLuint m_FakeBB_FBO;
		GLuint m_FakeBB_Color;
		GLuint m_FakeBB_DepthStencil;
		GLuint m_FakeVAO;
		GLuint m_FakeIdxBuf;
		GLsizeiptr m_FakeIdxSize;

		ResourceId m_FakeVAOID;
		
		bool m_DoStateVerify;
		//GLRenderState *m_CurrentPipelineState;
		
		Serialiser *GetSerialiser() { return m_pSerialiser; }

		void ProcessChunk(uint64_t offset, GLChunkType context);
		void ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
		void ContextProcessChunk(uint64_t offset, GLChunkType chunk, bool forceExecute);
		void AddUsage(FetchDrawcall draw);
		void AddDrawcall(FetchDrawcall d, bool hasEvents);
		void AddEvent(GLChunkType type, string description, ResourceId ctx = ResourceId());
		
		void Serialise_CaptureScope(uint64_t offset);
		bool HasSuccessfulCapture();
		void AttemptCapture();
		bool Serialise_BeginCaptureFrame(bool applyInitialState);
		void BeginCaptureFrame();
		void FinishCapture();
		void ContextEndFrame();

		struct ContextData
		{
			ContextData()
			{
				built = ready = false;
				attribsCreate = false;
				version = 0;
				isCore = false;
				Program = GeneralUBO = StringUBO = GlyphUBO = 0;
				GlyphTexture = DummyVAO = 0;
				CharSize = CharAspect = 0.0f;
				RDCEraseEl(m_TextureRecord);
				RDCEraseEl(m_BufferRecord);
				m_VertexArrayRecord = m_FeedbackRecord = m_DrawFramebufferRecord = NULL;
				m_ReadFramebufferRecord = NULL;
				m_Renderbuffer = ResourceId();
				m_TextureUnit = 0;
				m_ProgramPipeline = m_Program = 0;
			}

			void *ctx;

			bool built;
			bool ready;

			int version;
			bool attribsCreate;
			bool isCore;

			// map from window handle void* to uint64_t unix timestamp with
			// the last time a window was seen/associated with this context.
			// Decays after a few seconds since there's no good explicit
			// 'remove' type call for GL, only wglCreateContext/wglMakeCurrent
			map<void *, uint64_t> windows;

			// a window is only associated with one context at once, so any
			// time we associate a window, it broadcasts to all other
			// contexts to let them know to remove it
			void UnassociateWindow(void *wndHandle);
			void AssociateWindow(WrappedOpenGL *gl, void *wndHandle);

			bool Legacy() { return !attribsCreate || version < 32; }
			bool Modern() { return !Legacy(); }

			GLuint Program;
			GLuint GeneralUBO, StringUBO, GlyphUBO;
			GLuint GlyphTexture;
			GLuint DummyVAO;

			float CharSize;
			float CharAspect;

			// extensions
			vector<string> glExts;
			string glExtsString;

			// state
			GLResourceRecord *m_TextureRecord[256]; // TODO this needs on per texture type :(
			GLResourceRecord *m_BufferRecord[16];
			GLResourceRecord *m_VertexArrayRecord;
			GLResourceRecord *m_FeedbackRecord;
			GLResourceRecord *m_DrawFramebufferRecord;
			GLResourceRecord *m_ReadFramebufferRecord;
			ResourceId m_Renderbuffer;
			GLint m_TextureUnit;
			GLuint m_ProgramPipeline;
			GLuint m_Program;

			GLResourceRecord *GetActiveTexRecord() { return m_TextureRecord[m_TextureUnit]; }
		};

		map<void*, ContextData> m_ContextData;
		
		ContextData &GetCtxData();
		GLuint GetUniformProgram();
		
		void ReplaceResource(ResourceId from, ResourceId to);
		void RemoveReplacement(ResourceId id);
		void FreeTargetResource(ResourceId id);

		struct QueuedInitialStateFetch
		{
			GLResource res;
			byte *blob;

			bool operator <(const QueuedInitialStateFetch &o) const
			{
				return res.Context < o.res.Context;
			}
		};

		vector<QueuedInitialStateFetch> m_QueuedInitialFetches;

		void QueuePrepareInitialState(GLResource res, byte *blob);

		static const int FONT_TEX_WIDTH = 256;
		static const int FONT_TEX_HEIGHT = 128;
		static const int FONT_MAX_CHARS = 256;

		void RenderOverlayText(float x, float y, const char *fmt, ...);
		void RenderOverlayStr(float x, float y, const char *str);

		vector<string> globalExts;

		// no copy semantics
		WrappedOpenGL(const WrappedOpenGL &);
		WrappedOpenGL &operator =(const WrappedOpenGL &);

	public:
		WrappedOpenGL(const char *logfile, const GLHookSet &funcs);
		virtual ~WrappedOpenGL();

		GLResourceManager *GetResourceManager() { return m_ResourceManager; }

		ResourceId GetDeviceResourceID() { return m_DeviceResourceID; }
		ResourceId GetContextResourceID() { return m_ContextResourceID; }

		GLReplay *GetReplay() { return &m_Replay; }
		void *GetCtx();

		void SetDebugMsgContext(const char *context) { m_DebugMsgContext = context; }
		
		void AddDebugMessage(DebugMessage msg) { if(m_State < WRITING) m_DebugMessages.push_back(msg); }
		void AddDebugMessage(DebugMessageCategory c, DebugMessageSeverity sv, DebugMessageSource src, std::string d);

		// replay interface
		void Initialise(GLInitParams &params);
		void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
		void ReadLogInitialisation();

		GLuint GetFakeBBFBO() { return m_FakeBB_FBO; }
		GLuint GetFakeVAO() { return m_FakeVAO; }

		vector<FetchFrameRecord> &GetFrameRecord() { return m_FrameRecord; }
		FetchAPIEvent GetEvent(uint32_t eventID);

		const DrawcallTreeNode &GetRootDraw() { return m_ParentDrawcall; }

		const FetchDrawcall *GetDrawcall(uint32_t frameID, uint32_t eventID);

		vector<EventUsage> GetUsage(ResourceId id) { return m_ResourceUses[id]; }

		void CreateContext(GLWindowingData winData, void *shareContext, GLInitParams initParams, bool core, bool attribsCreate);
		void DeleteContext(void *contextHandle);
		void ActivateContext(GLWindowingData winData);
		void WindowSize(void *windowHandle, uint32_t w, uint32_t h);
		void SwapBuffers(void *windowHandle);

		void StartFrameCapture();

		void EndFrameCapture();


		void StartFrameCapture(void *dev, void *wnd);
		bool EndFrameCapture(void *dev, void *wnd);

		IMPLEMENT_FUNCTION_SERIALISED(void, glBindTexture(GLenum target, GLuint texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindTextures(GLuint first, GLsizei count, const GLuint *textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindImageTextures(GLuint first, GLsizei count, const GLuint *textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFunc(GLenum sfactor, GLenum dfactor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquation(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationi(GLuint buf, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendBarrierKHR());
		IMPLEMENT_FUNCTION_SERIALISED(void, glLogicOp(GLenum opcode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilFunc(GLenum func, GLint ref, GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilMask(GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilOp(GLenum fail, GLenum zfail, GLenum zpass));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilMaskSeparate(GLenum face, GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass));
		IMPLEMENT_FUNCTION_SERIALISED(void, glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSampleMaski(GLuint maskNumber, GLbitfield mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSampleCoverage(GLfloat value, GLboolean invert));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMinSampleShading(GLfloat value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glRasterSamplesEXT(GLuint samples, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClear(GLbitfield mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearDepth(GLdouble depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearDepthf(GLfloat depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearStencil(GLint stencil));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCullFace(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthFunc(GLenum func));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthMask(GLboolean flag));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRange(GLdouble nearVal, GLdouble farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRangef(GLfloat nearVal, GLfloat farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRangeIndexed(GLuint index, GLdouble nearVal, GLdouble farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthBoundsEXT(GLclampd nearVal, GLclampd farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClipControl(GLenum origin, GLenum depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProvokingVertex(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPrimitiveRestartIndex(GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisable(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnable(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisablei(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnablei(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFrontFace(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFinish());
		IMPLEMENT_FUNCTION_SERIALISED(void, glFlush());
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenTextures(GLsizei n, GLuint* textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteTextures(GLsizei n, const GLuint *textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glHint(GLenum target, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPixelStorei(GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPixelStoref(GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPolygonMode(GLenum face, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPolygonOffset(GLfloat factor, GLfloat units));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPatchParameteri(GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPatchParameterfv(GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPointParameterf(GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPointParameterfv(GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPointParameteri(GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPointParameteriv(GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPointSize(GLfloat size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glLineWidth(GLfloat width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameterf(GLenum target, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameteri(GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameteriv(GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameterIiv(GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameterIuiv(GLenum target, GLenum pname, const GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenSamplers(GLsizei count, GLuint *samplers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindSampler(GLuint unit, GLuint sampler));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindSamplers(GLuint first, GLsizei count, const GLuint *samplers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteSamplers(GLsizei n, const GLuint *ids));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSamplerParameteri(GLuint sampler, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glViewport(GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glViewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h));
		IMPLEMENT_FUNCTION_SERIALISED(void, glViewportIndexedfv(GLuint index, const GLfloat *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glViewportArrayv(GLuint first, GLuint count, const GLfloat *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glScissor(GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glScissorArrayv(GLuint first, GLsizei count, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glScissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glScissorIndexedv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClampColor(GLenum target, GLenum clamp));
		IMPLEMENT_FUNCTION_SERIALISED(void, glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glReadBuffer(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenFramebuffers(GLsizei n, GLuint *framebuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawBuffer(GLenum buf));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawBuffers(GLsizei n, const GLenum *bufs));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindFramebuffer(GLenum target, GLuint framebuffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferParameteri(GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenRenderbuffers(GLsizei n, GLuint *renderbuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindRenderbuffer(GLenum target, GLuint renderbuffer));

		GLenum glCheckFramebufferStatus(GLenum target);

		IMPLEMENT_FUNCTION_SERIALISED(void, glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label));
		IMPLEMENT_FUNCTION_SERIALISED(void, glObjectPtrLabel(const void *ptr, GLsizei length, const GLchar *label));

		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPopDebugGroup());

		IMPLEMENT_FUNCTION_SERIALISED(void, glPushGroupMarkerEXT(GLsizei length, const GLchar *marker));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPopGroupMarkerEXT());
		IMPLEMENT_FUNCTION_SERIALISED(void, glInsertEventMarkerEXT(GLsizei length, const GLchar *marker));

		IMPLEMENT_FUNCTION_SERIALISED(void, glFrameTerminatorGREMEDY());
		IMPLEMENT_FUNCTION_SERIALISED(void, glStringMarkerGREMEDY(GLsizei len, const void *string));
		
		bool Serialise_glFenceSync(GLsync real, GLenum condition, GLbitfield flags);
		GLsync glFenceSync(GLenum condition, GLbitfield flags);

		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout));
		IMPLEMENT_FUNCTION_SERIALISED(void, glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteSync(GLsync sync));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenQueries(GLsizei n, GLuint *ids));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBeginQuery(GLenum target, GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBeginQueryIndexed(GLenum target, GLuint index, GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEndQuery(GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEndQueryIndexed(GLenum target, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBeginConditionalRender(GLuint id, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEndConditionalRender());
		IMPLEMENT_FUNCTION_SERIALISED(void, glQueryCounter(GLuint id, GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteQueries(GLsizei n, const GLuint *ids));

		IMPLEMENT_FUNCTION_SERIALISED(void, glActiveTexture(GLenum texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateMipmap(GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTexImage1D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height));

		IMPLEMENT_FUNCTION_SERIALISED(void, glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter));

		bool Serialise_glCreateShader(GLuint real, GLenum type);
		GLuint glCreateShader(GLenum type);

		bool Serialise_glCreateShaderProgramv(GLuint real, GLenum type, GLsizei count, const GLchar *const*strings);
		GLuint glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const*strings);

		bool Serialise_glCreateProgram(GLuint real);
		GLuint glCreateProgram();
		
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompileShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glAttachShader(GLuint program, GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDetachShader(GLuint program, GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glReleaseShaderCompiler());
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glLinkProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramParameteri(GLuint program, GLenum pname, GLint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedStringARB(GLenum type, GLint namelen, const GLchar *name, GLint stringlen, const GLchar *str));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteNamedStringARB(GLint namelen, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompileShaderIncludeARB(GLuint shader, GLsizei count, const GLchar *const*path, const GLint *length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding));
		IMPLEMENT_FUNCTION_SERIALISED(void, glShaderStorageBlockBinding(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformSubroutinesuiv(GLenum shadertype, GLsizei count, const GLuint *indices));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindAttribLocation(GLuint program, GLuint index, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindFragDataLocation(GLuint program, GLuint color, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindFragDataLocationIndexed(GLuint program, GLuint colorNumber, GLuint index, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUseProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glValidateProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenProgramPipelines(GLsizei n, GLuint *pipelines));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindProgramPipeline(GLuint pipeline));
		IMPLEMENT_FUNCTION_SERIALISED(void, glActiveShaderProgram(GLuint pipeline, GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines));
		IMPLEMENT_FUNCTION_SERIALISED(void, glValidateProgramPipeline(GLuint pipeline));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenBuffers(GLsizei n, GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBuffer(GLenum target, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBufferBase(GLenum target, GLuint index, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBuffersBase(GLenum target, GLuint first, GLsizei count, const GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBuffersRange(GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapBuffer(GLenum target, GLenum access));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glUnmapBuffer(GLenum target));

		IMPLEMENT_FUNCTION_SERIALISED(void, glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenTransformFeedbacks(GLsizei n, GLuint *ids));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindTransformFeedback(GLenum target, GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBeginTransformFeedback(GLenum primitiveMode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPauseTransformFeedback());
		IMPLEMENT_FUNCTION_SERIALISED(void, glResumeTransformFeedback());
		IMPLEMENT_FUNCTION_SERIALISED(void, glEndTransformFeedback());
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawTransformFeedback(GLenum mode, GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawTransformFeedbackInstanced(GLenum mode, GLuint id, GLsizei instancecount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawTransformFeedbackStream(GLenum mode, GLuint id, GLuint stream));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawTransformFeedbackStreamInstanced(GLenum mode, GLuint id, GLuint stream, GLsizei instancecount));

		IMPLEMENT_FUNCTION_SERIALISED(void, glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDispatchComputeGroupSizeARB(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z, GLuint group_size_x, GLuint group_size_y, GLuint group_size_z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDispatchComputeIndirect(GLintptr indirect));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMemoryBarrier(GLbitfield barriers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMemoryBarrierByRegion(GLbitfield barriers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureBarrier());
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferData(GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferSubData(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateBufferData(GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateTexImage(GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateTexSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth));

		enum AttribType
		{
			Attrib_GLdouble = 0x01,
			Attrib_GLfloat  = 0x02,
			Attrib_GLshort  = 0x03,
			Attrib_GLushort = 0x04,
			Attrib_GLbyte   = 0x05,
			Attrib_GLubyte  = 0x06,
			Attrib_GLint    = 0x07,
			Attrib_GLuint   = 0x08,
			Attrib_packed   = 0x09,
			Attrib_typemask = 0x0f,

			Attrib_L        = 0x10,
			Attrib_I        = 0x20,
			Attrib_N        = 0x40,
		};

		bool Serialise_glVertexAttrib(GLuint index, int count, GLenum type, GLboolean normalized, const void *value, int attribtype);

		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib1d(GLuint index, GLdouble x));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib1dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib1f(GLuint index, GLfloat x));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib1fv(GLuint index, const GLfloat *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib1s(GLuint index, GLshort x));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib1sv(GLuint index, const GLshort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib2d(GLuint index, GLdouble x, GLdouble y));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib2dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib2fv(GLuint index, const GLfloat *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib2s(GLuint index, GLshort x, GLshort y));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib2sv(GLuint index, const GLshort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib3d(GLuint index, GLdouble x, GLdouble y, GLdouble z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib3dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib3fv(GLuint index, const GLfloat *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib3s(GLuint index, GLshort x, GLshort y, GLshort z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib3sv(GLuint index, const GLshort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Nbv(GLuint index, const GLbyte *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Niv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Nsv(GLuint index, const GLshort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Nub(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Nubv(GLuint index, const GLubyte *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Nuiv(GLuint index, const GLuint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4Nusv(GLuint index, const GLushort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4bv(GLuint index, const GLbyte *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4fv(GLuint index, const GLfloat *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4iv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4s(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4sv(GLuint index, const GLshort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4ubv(GLuint index, const GLubyte *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4uiv(GLuint index, const GLuint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttrib4usv(GLuint index, const GLushort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI1i(GLuint index, GLint x));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI1iv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI1ui(GLuint index, GLuint x));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI1uiv(GLuint index, const GLuint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI2i(GLuint index, GLint x, GLint y));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI2iv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI2ui(GLuint index, GLuint x, GLuint y));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI2uiv(GLuint index, const GLuint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI3i(GLuint index, GLint x, GLint y, GLint z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI3iv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI3uiv(GLuint index, const GLuint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4bv(GLuint index, const GLbyte *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4iv(GLuint index, const GLint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4sv(GLuint index, const GLshort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4ubv(GLuint index, const GLubyte *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4uiv(GLuint index, const GLuint *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribI4usv(GLuint index, const GLushort *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL1d(GLuint index, GLdouble x));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL1dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL2dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL3dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribL4dv(GLuint index, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP1ui(GLuint index, GLenum type, GLboolean normalized, GLuint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP1uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP2ui(GLuint index, GLenum type, GLboolean normalized, GLuint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP2uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP3ui(GLuint index, GLenum type, GLboolean normalized, GLuint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP3uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP4ui(GLuint index, GLenum type, GLboolean normalized, GLuint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribP4uiv(GLuint index, GLenum type, GLboolean normalized, const GLuint *value));

		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribBinding(GLuint attribindex, GLuint bindingindex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribDivisor(GLuint index, GLuint divisor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnableVertexAttribArray(GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisableVertexAttribArray(GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenVertexArrays(GLsizei n, GLuint *arrays));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindVertexArray(GLuint array));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexBindingDivisor(GLuint bindingindex, GLuint divisor));

		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsEnabled(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsTexture(GLuint texture));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsEnabledi(GLenum target, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsBuffer(GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsFramebuffer(GLuint framebuffer));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsProgramPipeline(GLuint pipeline));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsQuery(GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsRenderbuffer(GLuint renderbuffer));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsSampler(GLuint sampler));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsSync(GLsync sync));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsTransformFeedback(GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsVertexArray(GLuint array));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsNamedStringARB(GLint namelen, const GLchar *name));

		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glGetError());
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameteriv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBooleanv(GLenum pname, GLboolean *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloatv(GLenum pname, GLfloat *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetDoublev(GLenum pname, GLdouble *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegerv(GLenum pname, GLint *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetPointerv(GLenum pname, void **params));
		IMPLEMENT_FUNCTION_SERIALISED(const GLubyte *, glGetString(GLenum name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferPointerv(GLenum target, GLenum pname, void **params));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetFragDataIndex(GLuint program, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetFragDataLocation(GLuint program, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(const GLubyte *, glGetStringi(GLenum name, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBooleani_v(GLenum target, GLuint index, GLboolean *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegeri_v(GLenum target, GLuint index, GLint *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloati_v(GLenum target, GLuint index, GLfloat *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetDoublei_v(GLenum target, GLuint index, GLdouble *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInteger64v(GLenum pname, GLint64 *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderiv(GLuint shader, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramiv(GLuint program, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(GLuint, glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetProgramResourceLocationIndex(GLuint program, GLenum programInterface, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramStageiv(GLuint program, GLenum shadertype, GLenum pname, GLint *values));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedStringARB(GLint namelen, const GLchar *name, GLsizei bufSize, GLint *stringlen, GLchar *string));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedStringivARB(GLint namelen, const GLchar *name, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glGetGraphicsResetStatus());
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetObjectPtrLabel(const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label));
		IMPLEMENT_FUNCTION_SERIALISED(GLuint, glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultisamplefv(GLenum pname, GLuint index, GLfloat *val));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryiv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedTexImage(GLenum target, GLint level, void *img));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetnCompressedTexImage(GLenum target, GLint lod, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameteriv(GLuint texture, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureSubImage(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameterIiv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64 *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(GLuint, glGetSubroutineIndex(GLuint program, GLenum shadertype, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetSubroutineUniformLocation(GLuint program, GLenum shadertype, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveAtomicCounterBufferiv(GLuint program, GLuint bufferIndex, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveSubroutineName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveSubroutineUniformName(GLuint program, GLenum shadertype, GLuint index, GLsizei bufsize, GLsizei *length, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveSubroutineUniformiv(GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetUniformLocation(GLuint program, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformSubroutineuiv(GLenum shadertype, GLint location, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(GLuint, glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetAttribLocation(GLuint program, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniformName(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformName));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformfv(GLuint program, GLint location, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformiv(GLuint program, GLint location, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformuiv(GLuint program, GLint location, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformdv(GLuint program, GLint location, GLdouble *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetnUniformdv(GLuint program, GLint location, GLsizei bufSize, GLdouble *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64 *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizei size, void *data));

		enum UniformType
		{
			UNIFORM_UNKNOWN,

			VEC1fv,
			VEC1iv,
			VEC1uiv,
			VEC1dv,

			VEC2fv,
			VEC2iv,
			VEC2uiv,
			VEC2dv,

			VEC3fv,
			VEC3iv,
			VEC3uiv,
			VEC3dv,

			VEC4fv,
			VEC4iv,
			VEC4uiv,
			VEC4dv,

			MAT2fv,
			MAT2x3fv,
			MAT2x4fv,
			MAT3fv,
			MAT3x2fv,
			MAT3x4fv,
			MAT4fv,
			MAT4x2fv,
			MAT4x3fv,
			
			MAT2dv,
			MAT2x3dv,
			MAT2x4dv,
			MAT3dv,
			MAT3x2dv,
			MAT3x4dv,
			MAT4dv,
			MAT4x2dv,
			MAT4x3dv,
		};

		bool Serialise_glUniformMatrix(GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type);
		bool Serialise_glUniformVector(GLint location, GLsizei count, const void *value, UniformType type);
		
		bool Serialise_glProgramUniformMatrix(GLuint program, GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type);
		bool Serialise_glProgramUniformVector(GLuint program, GLint location, GLsizei count, const void *value, UniformType type);

		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1f(GLint location, GLfloat v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1i(GLint location, GLint v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1ui(GLint location, GLuint v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1d(GLint location, GLdouble v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2f(GLint location, GLfloat v0, GLfloat v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2i(GLint location, GLint v0, GLint v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2ui(GLint location, GLuint v0, GLuint v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2d(GLint location, GLdouble v0, GLdouble v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3i(GLint location, GLint v0, GLint v1, GLint v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3d(GLint location, GLdouble v0, GLdouble v1, GLdouble v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4d(GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3));

		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1iv(GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1uiv(GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1dv(GLint location, GLsizei count, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2iv(GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2uiv(GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2dv(GLint location, GLsizei count, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3iv(GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3uiv(GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3dv(GLint location, GLsizei count, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4iv(GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4uiv(GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4dv(GLint location, GLsizei count, const GLdouble *value));

		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1f(GLuint program, GLint location, GLfloat v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1i(GLuint program, GLint location, GLint v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1ui(GLuint program, GLint location, GLuint v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1d(GLuint program, GLint location, GLdouble v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2d(GLuint program, GLint location, GLdouble v0, GLdouble v1));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4d(GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3));

		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1dv(GLuint program, GLint location, GLsizei count, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2dv(GLuint program, GLint location, GLsizei count, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3dv(GLuint program, GLint location, GLsizei count, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4dv(GLuint program, GLint location, GLsizei count, const GLdouble *value));
		
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix2x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix2x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix3x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix3x4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4x2dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4x3dv(GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix2x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix2x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix3x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix3x4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix4dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix4x2dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniformMatrix4x3dv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value));

		// utility handling functions for glDraw*Elements* to handle pointers to indices being
		// passed directly, with no index buffer bound. It's not allowed in core profile but
		// it's fairly common and not too hard to support
		byte *Common_preElements(GLsizei Count, GLenum Type, uint64_t &IdxOffset);
		void Common_postElements(byte *idxDelete);

		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArrays(GLenum mode, GLint first, GLsizei count));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawArrays(GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride))
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawArraysIndirectCountARB(GLenum mode, GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride))
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArraysIndirect(GLenum mode, const void *indirect));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteBuffers(GLsizei n, const GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteVertexArrays(GLsizei n, const GLuint *arrays));

		// EXT_direct_state_access

		// there's a lot of duplicated code in some of these variants, between
		// EXT_dsa, ARB_dsa, non-dsa and for textures the MultiTex variants etc.
		// So we make a Common_ function similar to the Serialise_ function based
		// on the EXT_dsa interface, which takes the function parameters and a
		// GLResourceRecord* which does all the common tasks between all of these
		// functions.
		
		void Common_glGenerateTextureMipmapEXT(GLResourceRecord *record, GLenum target);

		void Common_glCopyTextureImage1DEXT(GLResourceRecord *record, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
		void Common_glCopyTextureImage2DEXT(GLResourceRecord *record, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
		void Common_glCopyTextureSubImage1DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
		void Common_glCopyTextureSubImage2DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
		void Common_glCopyTextureSubImage3DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

		void Common_glTextureBufferEXT(ResourceId id, GLenum target, GLenum internalformat, GLuint buffer);
		void Common_glTextureBufferRangeEXT(ResourceId id, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);

		void Common_glTextureImage1DEXT(ResourceId id, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels);
		void Common_glTextureImage2DEXT(ResourceId id, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
		void Common_glTextureImage3DEXT(ResourceId id, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
		void Common_glCompressedTextureImage1DEXT(ResourceId id, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits);
		void Common_glCompressedTextureImage2DEXT(ResourceId id, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits);
		void Common_glCompressedTextureImage3DEXT(ResourceId id, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits);

		void Common_glTextureStorage1DEXT(ResourceId id, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
		void Common_glTextureStorage2DEXT(ResourceId id, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
		void Common_glTextureStorage3DEXT(ResourceId id, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
		void Common_glTextureStorage2DMultisampleEXT(ResourceId id, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
		void Common_glTextureStorage3DMultisampleEXT(ResourceId id, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);

		void Common_glTextureSubImage1DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels);
		void Common_glTextureSubImage2DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
		void Common_glTextureSubImage3DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
		void Common_glCompressedTextureSubImage1DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits);
		void Common_glCompressedTextureSubImage2DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits);
		void Common_glCompressedTextureSubImage3DEXT(GLResourceRecord *record, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits);

		void Common_glTextureParameterfEXT(GLResourceRecord *record, GLenum target, GLenum pname, GLfloat param);
		void Common_glTextureParameterfvEXT(GLResourceRecord *record, GLenum target, GLenum pname, const GLfloat *params);
		void Common_glTextureParameteriEXT(GLResourceRecord *record, GLenum target, GLenum pname, GLint param);
		void Common_glTextureParameterivEXT(GLResourceRecord *record, GLenum target, GLenum pname, const GLint *params);
		void Common_glTextureParameterIivEXT(GLResourceRecord *record, GLenum target, GLenum pname, const GLint *params);
		void Common_glTextureParameterIuivEXT(GLResourceRecord *record, GLenum target, GLenum pname, const GLuint *params);

		void Common_glNamedBufferStorageEXT(ResourceId id, GLsizeiptr size, const void *data, GLbitfield flags);

		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateTextureMipmapEXT(GLuint texture, GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetPointeri_vEXT(GLenum pname, GLuint index, void **params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetDoubleIndexedvEXT(GLenum target, GLuint index, GLdouble *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetPointerIndexedvEXT(GLenum target, GLuint index, void **data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegerIndexedvEXT(GLenum target, GLuint index, GLint *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBooleanIndexedvEXT(GLenum target, GLuint index, GLboolean *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloatIndexedvEXT(GLenum target, GLuint index, GLfloat *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexImageEXT(GLenum texunit, GLenum target, GLint level, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexLevelParameterfvEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetMultiTexLevelParameterivEXT(GLenum texunit, GLenum target, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedMultiTexImageEXT(GLenum texunit, GLenum target, GLint lod, void *img));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferPointervEXT(GLuint buffer, GLenum pname, void **params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedProgramivEXT(GLuint program, GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedFramebufferParameterivEXT(GLuint framebuffer, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedRenderbufferParameterivEXT(GLuint renderbuffer, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayIntegervEXT(GLuint vaobj, GLenum pname, GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayPointervEXT(GLuint vaobj, GLenum pname, void **param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayIntegeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexArrayPointeri_vEXT(GLuint vaobj, GLuint index, GLenum pname, void **param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void *img));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureLevelParameterfvEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapNamedBufferEXT(GLuint buffer, GLenum access));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFlushMappedNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glUnmapNamedBufferEXT(GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedBufferDataEXT(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedBufferSubDataEXT(GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTexture1DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTexture3DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferRenderbufferEXT(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferParameteriEXT(GLuint framebuffer, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferDrawBufferEXT(GLuint framebuffer, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferReadBufferEXT(GLuint framebuffer, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedRenderbufferStorageEXT(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexParameteriEXT(GLenum texunit, GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexParameterivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexParameterfEXT(GLenum texunit, GLenum target, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexParameterfvEXT(GLenum texunit, GLenum target, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedMultiTexImage3DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedMultiTexImage2DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedMultiTexImage1DEXT(GLenum texunit, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedMultiTexSubImage3DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedMultiTexSubImage2DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedMultiTexSubImage1DEXT(GLenum texunit, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexBufferEXT(GLenum texunit, GLenum target, GLenum internalformat, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexParameterIivEXT(GLenum texunit, GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMultiTexParameterIuivEXT(GLenum texunit, GLenum target, GLenum pname, const GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateMultiTexMipmapEXT(GLenum texunit, GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureBufferEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterIivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterIuivEXT(GLuint texture, GLenum target, GLenum pname, const GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage2DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage3DMultisampleEXT(GLuint texture, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribIOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnableVertexArrayEXT(GLuint vaobj, GLenum array));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisableVertexArrayEXT(GLuint vaobj, GLenum array));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnableVertexArrayAttribEXT(GLuint vaobj, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisableVertexArrayAttribEXT(GLuint vaobj, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayBindVertexBufferEXT(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribIFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribLFormatEXT(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribBindingEXT(GLuint vaobj, GLuint attribindex, GLuint bindingindex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexBindingDivisorEXT(GLuint vaobj, GLuint bindingindex, GLuint divisor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribLOffsetEXT(GLuint vaobj, GLuint buffer, GLuint index, GLint size, GLenum type, GLsizei stride, GLintptr offset));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexAttribDivisorEXT(GLuint vaobj, GLuint index, GLuint divisor));

		// ARB_direct_state_access
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateTransformFeedbacks(GLsizei n, GLuint *ids));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateBuffers(GLsizei n, GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateFramebuffers(GLsizei n, GLuint *framebuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateTextures(GLenum target, GLsizei n, GLuint *textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateVertexArrays(GLsizei n, GLuint *arrays));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateSamplers(GLsizei n, GLuint *samplers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateProgramPipelines(GLsizei n, GLuint *pipelines));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCreateQueries(GLenum target, GLsizei n, GLuint *ids));

		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferData(GLuint buffer, GLsizei size, const void *data, GLenum usage));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferStorage(GLuint buffer, GLsizei size, const void *data, GLbitfield flags));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizei size, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizei size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLsizeiptr offset, GLsizei size, GLenum format, GLenum type, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizei length, GLbitfield access));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizei length));

		IMPLEMENT_FUNCTION_SERIALISED(void, glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizei size));

		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments));
		IMPLEMENT_FUNCTION_SERIALISED(void, glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, const GLfloat depth, GLint stencil));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter));

		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizei size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data));

		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height));

		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterf(GLuint texture, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameteri(GLuint texture, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterIiv(GLuint texture, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameteriv(GLuint texture, GLenum pname, const GLint *param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateTextureMipmap(GLuint texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindTextureUnit(GLuint unit, GLuint texture));

		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizei *strides));

};

class ScopedDebugContext
{
	public:
		ScopedDebugContext(WrappedOpenGL *gl, const char *fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			char buf[1024]; buf[1023] = 0;
			StringFormat::vsnprintf(buf, 1023, fmt, args);
			va_end(args);

			m_GL = gl;
			m_GL->SetDebugMsgContext(buf);
		}

		~ScopedDebugContext() { m_GL->SetDebugMsgContext(""); }
	private:
		WrappedOpenGL *m_GL;
};
