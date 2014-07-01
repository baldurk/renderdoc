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
	uint32_t width;
	uint32_t height;
	
	static const uint32_t GL_SERIALISE_VERSION = 0x0000002;

	// version number internal to opengl stream
	uint32_t SerialiseVersion;
};

struct DrawcallTreeNode
{
	DrawcallTreeNode() {}
	explicit DrawcallTreeNode(FetchDrawcall d) : draw(d) {}
	FetchDrawcall draw;
	vector<DrawcallTreeNode> children;

	DrawcallTreeNode &operator =(FetchDrawcall d) { *this = DrawcallTreeNode(d); return *this; }

	rdctype::array<FetchDrawcall> Bake()
	{
		rdctype::array<FetchDrawcall> ret;
		if(children.empty()) return ret;

		create_array_uninit(ret, children.size());
		for(size_t i=0; i < children.size(); i++)
		{
			ret.elems[i] = children[i].draw;
			ret.elems[i].children = children[i].Bake();
		}

		return ret;
	}
};

class WrappedOpenGL
{
	private:
		const GLHookSet &m_Real;

		friend class GLReplay;

		GLDEBUGPROC m_RealDebugFunc;
		const void *m_RealDebugFuncParam;

		void DebugSnoop(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message);
		static void APIENTRY DebugSnoopStatic(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
		{ ((WrappedOpenGL *)userParam)->DebugSnoop(source, type, id, severity, length, message); }

		// state
		GLResourceRecord *m_TextureRecord[256]; // TODO this needs on per texture type :(
		GLResourceRecord *m_BufferRecord[16];
		GLResourceRecord *m_VertexArrayRecord;
		GLResourceRecord *m_DrawFramebufferRecord;
		GLResourceRecord *m_ReadFramebufferRecord;
		GLint m_TextureUnit;

		size_t BufferIdx(GLenum buf);

		// internals
		Serialiser *m_pSerialiser;
		LogState m_State;
		
		GLReplay m_Replay;

		GLInitParams m_InitParams;

		map<uint64_t, void *> m_ActiveContexts;

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
		
		set<GLResource> m_HighTrafficResources;

		vector<FetchFrameRecord> m_FrameRecord;
		
		static const char *GetChunkName(uint32_t idx);
		
		// replay
		
		vector<FetchAPIEvent> m_CurEvents, m_Events;
		bool m_AddedDrawcall;

		uint64_t m_CurChunkOffset;
		uint32_t m_CurEventID, m_CurDrawcallID;
		
		DrawcallTreeNode m_ParentDrawcall;

		list<DrawcallTreeNode *> m_DrawcallStack;

		GLenum m_LastIndexSize;
		GLuint m_LastIndexOffset;
		GLenum m_LastDrawMode;

		struct BufferData
		{
			GLResource resource;
			GLenum curType;
			uint64_t size;
		};

		map<ResourceId, BufferData> m_Buffers;
		
		struct TextureData
		{
			TextureData() : width(0), height(0), depth(0) {}
			GLResource resource;
			GLenum curType;
			GLint width, height, depth;
		};

		map<ResourceId, TextureData> m_Textures;

		struct ShaderData
		{
			GLenum type;
			vector<string> sources;
			ShaderReflection reflection;
		};

		struct ProgramData
		{
			ProgramData() : colOutProg(0), linked(false) {}
			vector<ResourceId> shaders;

			GLuint colOutProg;
			bool linked;
		};
		
		struct PipelineData
		{
			PipelineData() {}

			struct ProgramUse
			{
				ProgramUse(ResourceId id_, GLbitfield use_) : id(id_), use(use_) {}

				ResourceId id;
				GLbitfield use;
			};

			vector<ProgramUse> programs;
		};

		map<ResourceId, ShaderData> m_Shaders;
		map<ResourceId, ProgramData> m_Programs;
		map<ResourceId, PipelineData> m_Pipelines;

		GLuint m_FakeBB_FBO;
		GLuint m_FakeBB_Color;
		GLuint m_FakeBB_DepthStencil;
		
		bool m_DoStateVerify;
		//GLRenderState *m_CurrentPipelineState;
		
		Serialiser *GetSerialiser() { return m_pSerialiser; }

		void ProcessChunk(uint64_t offset, GLChunkType context);
		void ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
		void ContextProcessChunk(uint64_t offset, GLChunkType chunk, bool forceExecute);
		void AddDrawcall(FetchDrawcall d, bool hasEvents);
		void AddEvent(GLChunkType type, string description, ResourceId ctx = ResourceId());
		
		void Serialise_CaptureScope(uint64_t offset);
		bool HasSuccessfulCapture();
		void AttemptCapture();
		bool Serialise_BeginCaptureFrame(bool applyInitialState);
		void BeginCaptureFrame();
		void FinishCapture();
		void EndCaptureFrame();

		vector<string> glExts;
		string glExtsString;

		// no copy semantics
		WrappedOpenGL(const WrappedOpenGL &);
		WrappedOpenGL &operator =(const WrappedOpenGL &);

	public:
		WrappedOpenGL(const wchar_t *logfile, const GLHookSet &funcs);
		~WrappedOpenGL();

		GLResourceManager *GetResourceManager() { return m_ResourceManager; }

		ResourceId GetDeviceResourceID() { return m_DeviceResourceID; }
		ResourceId GetContextResourceID() { return m_ContextResourceID; }

		GLReplay *GetReplay() { return &m_Replay; }
		void *GetCtx();
		
		// replay interface
		void Initialise(GLInitParams &params);
		void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
		void ReadLogInitialisation();

		vector<FetchFrameRecord> &GetFrameRecord() { return m_FrameRecord; }
		FetchAPIEvent GetEvent(uint32_t eventID);

		void CreateContext(void *windowHandle, void *contextHandle, void *shareContext, GLInitParams initParams);
		void ActivateContext(void *windowHandle, void *contextHandle);
		void WindowSize(void *windowHandle, uint32_t w, uint32_t h);
		void Present(void *windowHandle);

		IMPLEMENT_FUNCTION_SERIALISED(void, glBindTexture(GLenum target, GLuint texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFunc(GLenum sfactor, GLenum dfactor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilFunc(GLenum func, GLint ref, GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilMask(GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilOp(GLenum fail, GLenum zfail, GLenum zpass));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilMaskSeparate(GLenum face, GLuint mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass));
		IMPLEMENT_FUNCTION_SERIALISED(void, glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glSampleMaski(GLuint maskNumber, GLbitfield mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClear(GLbitfield mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearDepth(GLclampd depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCullFace(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthFunc(GLenum func));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthMask(GLboolean flag));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRange(GLdouble nearVal, GLdouble farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRangef(GLfloat nearVal, GLfloat farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthBoundsEXT(GLclampd nearVal, GLclampd farVal));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisable(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnable(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsEnabled(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisablei(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnablei(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glIsEnabledi(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFrontFace(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glGetError());
		IMPLEMENT_FUNCTION_SERIALISED(void, glFinish());
		IMPLEMENT_FUNCTION_SERIALISED(void, glFlush());
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloatv(GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetDoublev(GLenum pname, GLdouble *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegerv(GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBooleanv(GLenum pname, GLboolean *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegeri_v(GLenum pname, GLuint index, GLint *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloati_v(GLenum pname, GLuint index, GLfloat *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetDoublei_v(GLenum pname, GLuint index, GLdouble *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBooleani_v(GLenum pname, GLuint index, GLboolean *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data));
		IMPLEMENT_FUNCTION_SERIALISED(const GLubyte *, glGetStringi(GLenum name, GLuint i));
		IMPLEMENT_FUNCTION_SERIALISED(const GLubyte *, glGetString(GLenum name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenTextures(GLsizei n, GLuint* textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteTextures(GLsizei n, const GLuint *textures));
		IMPLEMENT_FUNCTION_SERIALISED(void, glHint(GLenum target, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPixelStorei(GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPixelStoref(GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPolygonMode(GLenum face, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPolygonOffset(GLfloat factor, GLfloat units));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPatchParameteri(GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPatchParameterfv(GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexImage1D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameterf(GLenum target, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameteri(GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameteriv(GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenSamplers(GLsizei count, GLuint *samplers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindSampler(GLuint unit, GLuint sampler));
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
		IMPLEMENT_FUNCTION_SERIALISED(void, glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glReadBuffer(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenFramebuffers(GLsizei n, GLuint *framebuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawBuffer(GLenum buf));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawBuffers(GLsizei n, const GLenum *bufs));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindFramebuffer(GLenum target, GLuint framebuffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params));

		GLenum glCheckFramebufferStatus(GLenum target);

		IMPLEMENT_FUNCTION_SERIALISED(void, glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label));
		IMPLEMENT_FUNCTION_SERIALISED(void, glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label));

		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPopDebugGroup());
		
		bool Serialise_glFenceSync(GLsync real, GLenum condition, GLbitfield flags);
		GLsync glFenceSync(GLenum condition, GLbitfield flags);

		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout));
		IMPLEMENT_FUNCTION_SERIALISED(void, glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteSync(GLsync sync));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenQueries(GLsizei n, GLuint *ids));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBeginQuery(GLenum target, GLuint id));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEndQuery(GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteQueries(GLsizei n, const GLuint *ids));

		IMPLEMENT_FUNCTION_SERIALISED(void, glActiveTexture(GLenum texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexSubImage1D(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureView(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateMipmap(GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth));

		IMPLEMENT_FUNCTION_SERIALISED(void, glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter));

		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameteriv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedTexImage(GLenum target, GLint level, void *img));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params));

		bool Serialise_glCreateShader(GLuint real, GLenum type);
		GLuint glCreateShader(GLenum type);

		bool Serialise_glCreateShaderProgramv(GLuint real, GLenum type, GLsizei count, const GLchar *const*strings);
		GLuint glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const*strings);

		bool Serialise_glCreateProgram(GLuint real);
		GLuint glCreateProgram();
		
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompileShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderiv(GLuint shader, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glAttachShader(GLuint program, GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDetachShader(GLuint program, GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glReleaseShaderCompiler());
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glLinkProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramParameteri(GLuint program, GLenum pname, GLint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindAttribLocation(GLuint program, GLuint index, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUseProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glValidateProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramiv(GLuint program, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenProgramPipelines(GLsizei n, GLuint *pipelines));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindProgramPipeline(GLuint pipeline));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteProgramPipelines(GLsizei n, const GLuint *pipelines));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glValidateProgramPipeline(GLuint pipeline));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenBuffers(GLsizei n, GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBuffer(GLenum target, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBufferBase(GLenum target, GLuint index, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapBuffer(GLenum target, GLenum access));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glUnmapBuffer(GLenum target));

		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnableVertexAttribArray(GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisableVertexAttribArray(GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenVertexArrays(GLsizei n, GLuint *arrays));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindVertexArray(GLuint array));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetUniformLocation(GLuint program, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices));
		IMPLEMENT_FUNCTION_SERIALISED(GLuint, glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(GLint, glGetAttribLocation(GLuint program, const GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformfv(GLuint program, GLint location, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetUniformiv(GLuint program, GLint location, GLint *params));

		enum UniformType
		{
			UNIFORM_UNKNOWN,

			VEC1FV,
			VEC1IV,
			VEC1UIV,
			VEC2FV,
			VEC3FV,
			VEC4FV,

			MAT4FV,
		};

		bool Serialise_glUniformMatrix(GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type);
		bool Serialise_glUniformVector(GLint location, GLsizei count, const void *value, UniformType type);
		
		bool Serialise_glProgramUniformVector(GLuint program, GLint location, GLsizei count, const void *value, UniformType type);

		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1f(GLint location, GLfloat value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1i(GLint location, GLint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1ui(GLint location, GLuint value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1iv(GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1uiv(GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform1fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform2fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4fv(GLint location, GLsizei count, const GLfloat *value));
		
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1i(GLuint program, GLint location, GLint v0));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value));

		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArrays(GLenum mode, GLint first, GLsizei count));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteBuffers(GLsizei n, const GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteVertexArrays(GLsizei n, const GLuint *arrays));

		// EXT_direct_state_access
		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glCheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompressedTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *bits));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferDrawBuffersEXT(GLuint framebuffer, GLsizei n, const GLenum *bufs));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateTextureMipmapEXT(GLuint texture, GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetCompressedTextureImageEXT(GLuint texture, GLenum target, GLint lod, void *img));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedBufferParameterivEXT(GLuint buffer, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetNamedFramebufferAttachmentParameterivEXT(GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureImageEXT(GLuint texture, GLenum target, GLint level, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTextureLevelParameterivEXT(GLuint texture, GLenum target, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapNamedBufferEXT(GLuint buffer, GLenum access));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapNamedBufferRangeEXT(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferDataEXT(GLuint buffer, GLsizeiptr size, const void *data, GLenum usage));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferStorageEXT(GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedBufferSubDataEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedCopyBufferSubDataEXT(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTextureEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTexture2DEXT(GLuint framebuffer, GLenum attachment, GLenum textarget, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNamedFramebufferTextureLayerEXT(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureBufferRangeEXT(GLuint texture, GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureImage1DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureImage3DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterfvEXT(GLuint texture, GLenum target, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureParameterivEXT(GLuint texture, GLenum target, GLenum pname, const GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage1DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTextureSubImage3DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glUnmapNamedBufferEXT(GLuint buffer));
};
