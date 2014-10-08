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

#include "api/replay/renderdoc_replay.h"
#include "replay/replay_driver.h"
#include "core/core.h"

class WrappedID3D11Device;

class D3D11Replay : public IReplayDriver
{
	public:
		D3D11Replay();

		void SetProxy(bool p) { m_Proxy = p; }
		bool IsRemoteProxy() { return m_Proxy; }

		void Shutdown();

		void SetDevice(WrappedID3D11Device *d) { m_pDevice = d; }

		APIProperties GetAPIProperties();
			
		vector<ResourceId> GetBuffers();
		FetchBuffer GetBuffer(ResourceId id);

		vector<ResourceId> GetTextures();
		FetchTexture GetTexture(ResourceId id);

		ShaderReflection *GetShader(ResourceId id);
		
		vector<EventUsage> GetUsage(ResourceId id);

		vector<FetchFrameRecord> GetFrameRecord();

		void SavePipelineState() { m_CurPipelineState = MakePipelineState(); }
		D3D11PipelineState GetD3D11PipelineState() { return m_CurPipelineState; }
		GLPipelineState GetGLPipelineState() { return GLPipelineState(); }

		void FreeTargetResource(ResourceId id);
		void FreeCustomShader(ResourceId id);

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
		
		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval);
		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram);
		
		PostVSMeshData GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage);
		
		vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len);
		byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize);
		
		void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		void ReplaceResource(ResourceId from, ResourceId to);
		void RemoveReplacement(ResourceId id);

		void TimeDrawcalls(rdctype::array<FetchDrawcall> &arr);

		ResourceId CreateProxyTexture(FetchTexture templateTex);
		void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize);

		void RenderMesh(uint32_t frameID, const vector<uint32_t> &events, MeshDisplay cfg);
		
		bool RenderTexture(TextureDisplay cfg);

		void RenderCheckerboard(Vec3f light, Vec3f dark);

		void RenderHighlightBox(float w, float h, float scale);
		
		void FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data);
		
		vector<PixelModification> PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t sampleIdx);
		ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset);
		ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive);
		ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
		void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4]);
			
		ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents);

		void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip);
			
		bool IsRenderOutput(ResourceId id);
		
		void InitCallstackResolver();
		bool HasCallstacks();
		Callstack::StackResolver *GetCallstackResolver();
	private:
		D3D11PipelineState MakePipelineState();

		bool m_Proxy;

		WrappedID3D11Device *m_pDevice;

		D3D11PipelineState m_CurPipelineState;
};


