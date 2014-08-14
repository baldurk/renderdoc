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

#include "gl_common.h"
#include "replay/renderdoc.h"
#include "replay/replay_driver.h"
#include "core/core.h"

class WrappedOpenGL;

class GLReplay : public IReplayDriver
{
	public:
		GLReplay();

		void SetProxy(bool p) { m_Proxy = p; }
		bool IsRemoteProxy() { return m_Proxy; }

		void Shutdown();

		void SetDriver(WrappedOpenGL *d) { m_pDriver = d; }
	
		APIProperties GetAPIProperties();

		vector<ResourceId> GetBuffers();
		FetchBuffer GetBuffer(ResourceId id);

		vector<ResourceId> GetTextures();
		FetchTexture GetTexture(ResourceId id);

		ShaderReflection *GetShader(ResourceId id);
		
		vector<EventUsage> GetUsage(ResourceId id);

		vector<FetchFrameRecord> GetFrameRecord();

		void SavePipelineState();
		D3D11PipelineState GetD3D11PipelineState() { return D3D11PipelineState(); }
		GLPipelineState GetGLPipelineState() { return m_CurPipelineState; }

		void FreeTargetResource(ResourceId id);

		void ReadLogInitialisation();
		void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
		void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);

		uint64_t MakeOutputWindow(void *w, bool depth);
		void DestroyOutputWindow(uint64_t id);
		bool CheckResizeOutputWindow(uint64_t id);
		void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h);
		void ClearOutputWindowColour(uint64_t id, float col[4]);
		void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil);
		void BindOutputWindow(uint64_t id, bool depth);
		bool IsOutputWindowVisible(uint64_t id);
		void FlipOutputWindow(uint64_t id);

		void InitPostVSBuffers(uint32_t frameID, uint32_t eventID);

		ResourceId GetLiveID(ResourceId id);
		
		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, float *minval, float *maxval);
		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram);
		
		PostVSMeshData GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage);
		
		vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len);
		byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, size_t &dataSize);
		
		void ReplaceResource(ResourceId from, ResourceId to);
		void RemoveReplacement(ResourceId id);

		void TimeDrawcalls(rdctype::array<FetchDrawcall> &arr);

		bool SaveTexture(ResourceId tex, uint32_t saveMip, wstring path);

		void RenderMesh(int frameID, vector<int> eventID, MeshDisplay cfg);
		
		void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		void FreeCustomShader(ResourceId id);

		bool RenderTexture(TextureDisplay cfg);

		void RenderCheckerboard(Vec3f light, Vec3f dark);

		void RenderHighlightBox(float w, float h, float scale);
		
		void FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data);
		
		vector<PixelModification> PixelHistory(uint32_t frameID, vector<uint32_t> events, ResourceId target, uint32_t x, uint32_t y);
		ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset);
		ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y);
		ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
		void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, float pixel[4]);
			
		ResourceId RenderOverlay(ResourceId cfg, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID);
		ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip);
			
		ResourceId CreateProxyTexture(FetchTexture templateTex);
		void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize);

		bool IsRenderOutput(ResourceId id);
		
		void InitCallstackResolver();
		bool HasCallstacks();
		Callstack::StackResolver *GetCallstackResolver();

		void SetReplayData(GLWindowingData data);
	private:
		void FillCBufferValue(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, bool rowMajor, uint32_t offs, uint32_t matStride,
		                      const vector<byte> &data, ShaderVariable &outVar);
		void FillCBufferVariables(WrappedOpenGL &gl, GLuint prog, bool bufferBacked, string prefix,
								  const rdctype::array<ShaderConstant> &variables, vector<ShaderVariable> &outvars,
								  const vector<byte> &data);

		void GetMapping(WrappedOpenGL &gl, GLuint curProg, int shadIdx, ShaderReflection *refl, ShaderBindpointMapping &mapping);

		struct OutputWindow : public GLWindowingData
		{
			struct
			{
				// used to blit from defined FBO (VAOs not shared)
				GLuint emptyVAO;

				// texture for the below FBO. Resizes with the window
				GLuint backbuffer;

				// this FBO is on the debug GL context, not the window's GL context
				// when rendering a texture or mesh etc, we render onto this FBO on
				// the debug GL context, then blit onto the default framebuffer
				// on the window's GL context.
				// This is so we don't have to re-create any non-shared resource we
				// need for debug rendering on the window's GL context.
				GLuint windowFBO;

				// this FBO is the same as the above, but on the replay context,
				// for any cases where we need to use the replay context (like
				// re-rendering a draw).
				GLuint replayFBO;
			} BlitData;

			int width, height;
		};

		// any objects that are shared between contexts, we just initialise
		// once
		struct
		{
			float outWidth, outHeight;
			
			string blitvsSource;
			string blitfsSource;
			string genericvsSource;
			string genericfsSource;

			// program that does a blit of texture from input to output,
			// no transformation or scaling
			GLuint blitProg;

			GLuint texDisplayProg;

			GLuint pointSampler;
			GLuint linearSampler;

			GLuint checkerProg;

			GLuint genericProg;

			GLuint meshProg;
			GLuint meshVAO;

			GLuint outlineStripVB;
			GLuint outlineStripVAO;

			GLuint pickPixelTex;
			GLuint pickPixelFBO;

			GLuint overlayTex;
			GLuint overlayFBO;
			GLint overlayTexWidth, overlayTexHeight;

			GLuint UBOs[2];
			static const size_t UBOSize = 64 * sizeof(Vec4f);

			GLuint emptyVAO;
		} DebugData;

		void InitDebugData();
		
		GLuint CreateShaderProgram(const char *vs, const char *ps);

		void InitOutputWindow(OutputWindow &outwin);
		void CreateOutputWindowBackbuffer(OutputWindow &outwin);

		GLWindowingData m_ReplayCtx;
		OutputWindow *m_DebugCtx;

		void MakeCurrentReplayContext(GLWindowingData *ctx);
		void SwapBuffers(GLWindowingData *ctx);
		void CloseReplayContext();

		uint64_t m_OutputWindowID;
		map<uint64_t, OutputWindow> m_OutputWindows;

		bool m_Proxy;

		WrappedOpenGL *m_pDriver;

		GLPipelineState m_CurPipelineState;
};
