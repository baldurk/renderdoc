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

#include "maths/vec.h"
#include "api/replay/renderdoc_replay.h"
#include "replay/replay_driver.h"
#include "core/core.h"

struct FetchFrameRecord
{
	FetchFrameInfo frameInfo;

	vector<FetchDrawcall> drawcallList;
};

// these two interfaces define what an API driver implementation must provide
// to the replay. At minimum it must implement IRemoteDriver which contains
// all of the functionality that cannot be achieved elsewhere. An IReplayDriver
// is more powerful and can be used as a local replay (with an IRemoteDriver
// proxied elsewhere if necessary).
//
// In this sense, IRemoteDriver is a strict subset of IReplayDriver functionality.
// Wherever at all possible functionality should be added as part of IReplayDriver,
// *not* as part of IRemoteDriver, to keep the burden on remote drivers to a minimum.

class IRemoteDriver
{
	public:
		virtual void Shutdown() = 0;
		
		virtual APIProperties GetAPIProperties() = 0;

		virtual vector<ResourceId> GetBuffers() = 0;
		virtual FetchBuffer GetBuffer(ResourceId id) = 0;

		virtual vector<ResourceId> GetTextures() = 0;
		virtual FetchTexture GetTexture(ResourceId id) = 0;

		virtual vector<DebugMessage> GetDebugMessages() = 0;

		virtual ShaderReflection *GetShader(ResourceId id) = 0;
		
		virtual vector<EventUsage> GetUsage(ResourceId id) = 0;

		virtual void SavePipelineState() = 0;
		virtual D3D11PipelineState GetD3D11PipelineState() = 0;
		virtual GLPipelineState GetGLPipelineState() = 0;

		virtual vector<FetchFrameRecord> GetFrameRecord() = 0;


		virtual void ReadLogInitialisation() = 0;
		virtual void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv) = 0;
		virtual void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType) = 0;

		virtual void InitPostVSBuffers(uint32_t frameID, uint32_t eventID) = 0;

		virtual ResourceId GetLiveID(ResourceId id) = 0;
		
		virtual MeshFormat GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage) = 0;
		
		virtual vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len) = 0;
		virtual byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize) = 0;
		
		virtual void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors) = 0;
		virtual void ReplaceResource(ResourceId from, ResourceId to) = 0;
		virtual void RemoveReplacement(ResourceId id) = 0;
		virtual void FreeTargetResource(ResourceId id) = 0;
		
		virtual vector<uint32_t> EnumerateCounters() = 0;
		virtual void DescribeCounter(uint32_t counterID, CounterDescription &desc) = 0;
		virtual vector<CounterResult> FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counterID) = 0;
		
		virtual void FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data) = 0;

		virtual vector<PixelModification> PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx) = 0;
		virtual ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset) = 0;
		virtual ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive) = 0;
		virtual ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]) = 0;

		virtual ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents) = 0;
			
		virtual bool IsRenderOutput(ResourceId id) = 0;

		virtual void FileChanged() = 0;
	
		virtual void InitCallstackResolver() = 0;
		virtual bool HasCallstacks() = 0;
		virtual Callstack::StackResolver *GetCallstackResolver() = 0;
};

class IReplayDriver : public IRemoteDriver
{
	public:
		virtual bool IsRemoteProxy() = 0;

		virtual uint64_t MakeOutputWindow(void *w, bool depth) = 0;
		virtual void DestroyOutputWindow(uint64_t id) = 0;
		virtual bool CheckResizeOutputWindow(uint64_t id) = 0;
		virtual void GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h) = 0;
		virtual void ClearOutputWindowColour(uint64_t id, float col[4]) = 0;
		virtual void ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil) = 0;
		virtual void BindOutputWindow(uint64_t id, bool depth) = 0;
		virtual bool IsOutputWindowVisible(uint64_t id) = 0;
		virtual void FlipOutputWindow(uint64_t id) = 0;

		virtual bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval) = 0;
		virtual bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram) = 0;

		virtual ResourceId CreateProxyTexture(FetchTexture templateTex) = 0;
		virtual void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize) = 0;
		
		virtual ResourceId CreateProxyBuffer(FetchBuffer templateBuf) = 0;
		virtual void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize) = 0;

		virtual void RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg) = 0;
		virtual bool RenderTexture(TextureDisplay cfg) = 0;

		virtual void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors) = 0;
		virtual ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip) = 0;
		virtual void FreeCustomShader(ResourceId id) = 0;

		virtual void RenderCheckerboard(Vec3f light, Vec3f dark) = 0;

		virtual void RenderHighlightBox(float w, float h, float scale) = 0;
		
		virtual void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4]) = 0;
		virtual uint32_t PickVertex(uint32_t frameID, uint32_t eventID, MeshDisplay cfg, uint32_t x, uint32_t y) = 0;
};
