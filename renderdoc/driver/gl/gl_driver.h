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
		GLResourceRecord *m_TextureRecord[128];
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
			ProgramData() : colOutProg(0) {}
			vector<ResourceId> shaders;

			GLuint colOutProg;
		};

		map<ResourceId, ShaderData> m_Shaders;
		map<ResourceId, ProgramData> m_Programs;

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

		// legacy/immediate mode
		IMPLEMENT_FUNCTION_SERIALISED(void, glLightfv(GLenum light, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glMaterialfv(GLenum face, GLenum pname, const GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(GLuint, glGenLists(GLsizei range));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNewList(GLuint list, GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEndList());
		IMPLEMENT_FUNCTION_SERIALISED(void, glCallList(GLuint list));
		IMPLEMENT_FUNCTION_SERIALISED(void, glShadeModel(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBegin(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnd());
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertex3f(GLfloat x, GLfloat y, GLfloat z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz));
		IMPLEMENT_FUNCTION_SERIALISED(void, glPushMatrix());
		IMPLEMENT_FUNCTION_SERIALISED(void, glPopMatrix());
		IMPLEMENT_FUNCTION_SERIALISED(void, glMatrixMode(GLenum mode));
		IMPLEMENT_FUNCTION_SERIALISED(void, glLoadIdentity());
		IMPLEMENT_FUNCTION_SERIALISED(void, glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTranslatef(GLfloat x, GLfloat y, GLfloat z));
		IMPLEMENT_FUNCTION_SERIALISED(void, glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z));
		//

		IMPLEMENT_FUNCTION_SERIALISED(void, glBindTexture(GLenum target, GLuint texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFunc(GLenum sfactor, GLenum dfactor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glColorMaski(GLuint buf, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClear(GLbitfield mask));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearDepth(GLclampd depth));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCullFace(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthFunc(GLenum func));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthMask(GLboolean flag));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDepthRangeArrayv(GLuint first, GLsizei count, const GLdouble *v));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisable(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnable(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDisablei(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glEnablei(GLenum cap, GLuint index));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFrontFace(GLenum cap));
		IMPLEMENT_FUNCTION_SERIALISED(GLenum, glGetError());
		IMPLEMENT_FUNCTION_SERIALISED(void, glFinish());
		IMPLEMENT_FUNCTION_SERIALISED(void, glFlush());
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloatv(GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegerv(GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBooleanv(GLenum pname, GLboolean *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetIntegeri_v(GLenum pname, GLuint index, GLint *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFloati_v(GLenum pname, GLuint index, GLfloat *data));
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
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexParameteri(GLenum target, GLenum pname, GLint param));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenSamplers(GLsizei count, GLuint *samplers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindSampler(GLuint unit, GLuint sampler));
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
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindFramebuffer(GLenum target, GLuint framebuffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params));

		GLenum glCheckFramebufferStatus(GLenum target);

		IMPLEMENT_FUNCTION_SERIALISED(void, glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label));
		IMPLEMENT_FUNCTION_SERIALISED(void, glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label));

		IMPLEMENT_FUNCTION_SERIALISED(void, glDebugMessageCallback(GLDEBUGPROC callback, const void *userParam));
		IMPLEMENT_FUNCTION_SERIALISED(void, glActiveTexture(GLenum texture));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height));
		IMPLEMENT_FUNCTION_SERIALISED(void, glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenerateMipmap(GLenum target));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter));

		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexParameteriv(GLenum target, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void *pixels));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetInternalformati64v(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint64 *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params));

		bool Serialise_glCreateShader(GLuint real, GLenum type);
		GLuint glCreateShader(GLenum type);

		bool Serialise_glCreateProgram(GLuint real);
		GLuint glCreateProgram();

		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glShaderSource(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length));
		IMPLEMENT_FUNCTION_SERIALISED(void, glCompileShader(GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderiv(GLuint shader, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glAttachShader(GLuint program, GLuint shader));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glLinkProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUseProgram(GLuint program));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramiv(GLuint program, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGenBuffers(GLsizei n, GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBuffer(GLenum target, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBufferBase(GLenum target, GLuint index, GLuint buffer));
		IMPLEMENT_FUNCTION_SERIALISED(void, glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size));
		IMPLEMENT_FUNCTION_SERIALISED(void *, glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access));
		IMPLEMENT_FUNCTION_SERIALISED(GLboolean, glUnmapBuffer(GLenum target));

		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil));
		IMPLEMENT_FUNCTION_SERIALISED(void, glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer));
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

			VEC3FV,
			VEC4FV,

			MAT4FV,
		};

		bool Serialise_glUniformMatrix(GLint location, GLsizei count, GLboolean transpose, const void *value, UniformType type);
		bool Serialise_glUniformVector(GLint location, GLsizei count, const void *value, UniformType type);

		IMPLEMENT_FUNCTION_SERIALISED(void, glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform3fv(GLint location, GLsizei count, const GLfloat *value));
		IMPLEMENT_FUNCTION_SERIALISED(void, glUniform4fv(GLint location, GLsizei count, const GLfloat *value));

		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArrays(GLenum mode, GLint first, GLsizei count));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteBuffers(GLsizei n, const GLuint *buffers));
		IMPLEMENT_FUNCTION_SERIALISED(void, glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data));
		IMPLEMENT_FUNCTION_SERIALISED(void, glDeleteVertexArrays(GLsizei n, const GLuint *arrays));
};
