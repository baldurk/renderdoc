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
#include "core/core.h"
#include "replay/renderdoc.h"
#include "replay/replay_driver.h"

#include <vector>
#include <set>

#include "type_helpers.h"

struct ReplayRenderer;

struct ReplayOutput
{
public:
	bool SetOutputConfig(const OutputConfig &o);
	bool SetTextureDisplay(const TextureDisplay &o);
	bool SetMeshDisplay(const MeshDisplay &o);
	
	bool ClearThumbnails();
	bool AddThumbnail(void *wnd, ResourceId texID);

	bool Display();

	OutputType GetType() { return m_Config.m_Type; }
	
	bool SetPixelContext(void *wnd);
	bool SetPixelContextLocation(uint32_t x, uint32_t y);
	void DisablePixelContext();

	bool PickPixel(ResourceId texID, bool customShader, 
					uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample,
					PixelValue *val);
private:
	ReplayOutput(ReplayRenderer *parent, void *w);
	~ReplayOutput();
	
	void SetFrameEvent(int frameID, int eventID);
	void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
	
	void RefreshOverlay();


	void DisplayContext();
	void DisplayTex();


	void DisplayMesh();

	ReplayRenderer *m_pRenderer;
	size_t m_ID;

	bool m_OverlayDirty;

	IReplayDriver *m_pDevice;

	uint32_t m_TexWidth, m_TexHeight;

	struct OutputPair 
	{
		ResourceId texture;
		bool depthMode;
		void *wndHandle;
		uint64_t outputID;

		bool dirty;
	} m_MainOutput;

	ResourceId m_OverlayResourceId;
	ResourceId m_CustomShaderResourceId;

	std::vector<OutputPair> m_Thumbnails;

	float m_ContextX;
	float m_ContextY;
	OutputPair m_PixelContext;

	uint32_t m_FrameID;
	uint32_t m_EventID;
	uint32_t m_FirstDeferredEvent;
	uint32_t m_LastDeferredEvent;
	OutputConfig m_Config;
	
	vector<uint32_t> passEvents;

	int32_t m_Width;
	int32_t m_Height;

	struct
	{
		TextureDisplay texDisplay;
		MeshDisplay meshDisplay;
	} m_RenderData;

	friend struct ReplayRenderer;
};

struct ReplayRenderer
{
	public:
		ReplayRenderer();
		~ReplayRenderer();

		APIProperties GetAPIProperties();

		ReplayCreateStatus CreateDevice(const wchar_t *logfile);
		ReplayCreateStatus SetDevice(IReplayDriver *device);
		
		bool HasCallstacks();
		bool InitResolver();
		
		bool SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
		bool SetFrameEvent(uint32_t frameID, uint32_t eventID);
		bool SetFrameEvent(uint32_t frameID, uint32_t eventID, bool force);

		void FetchPipelineState();

		bool GetD3D11PipelineState(D3D11PipelineState *state);
		bool GetGLPipelineState(GLPipelineState *state);
		
		ResourceId BuildCustomShader(const wchar_t *entry, const wchar_t *source, const uint32_t compileFlags, ShaderStageType type, rdctype::wstr *errors);
		bool FreeCustomShader(ResourceId id);
		
		ResourceId BuildTargetShader(const wchar_t *entry, const wchar_t *source, const uint32_t compileFlags, ShaderStageType type, rdctype::wstr *errors);
		bool ReplaceResource(ResourceId from, ResourceId to);
		bool RemoveReplacement(ResourceId id);
		bool FreeTargetResource(ResourceId id);
		
		bool GetFrameInfo(rdctype::array<FetchFrameInfo> *frame);
		bool GetDrawcalls(uint32_t frameID, bool includeTimes, rdctype::array<FetchDrawcall> *draws);
		bool GetTextures(rdctype::array<FetchTexture> *texs);
		bool GetBuffers(rdctype::array<FetchBuffer> *bufs);
		bool GetResolve(uint64_t *callstack, uint32_t callstackLen, rdctype::array<rdctype::wstr> *trace);
		ShaderReflection *GetShaderDetails(ResourceId shader);
		
		bool PixelHistory(ResourceId target, uint32_t x, uint32_t y, rdctype::array<PixelModification> *history);
		bool VSGetDebugStates(uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace);
		bool PSGetDebugStates(uint32_t x, uint32_t y, ShaderDebugTrace *trace);
		bool CSGetDebugStates(uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace);

		bool GetPostVSData(MeshDataStage stage, PostVSMeshData *data);
		
		bool GetMinMax(ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, PixelValue *minval, PixelValue *maxval);
		bool GetHistogram(ResourceId tex, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], rdctype::array<uint32_t> *histogram);
		
		bool GetUsage(ResourceId id, rdctype::array<EventUsage> *usage);
		
		bool GetBufferData(ResourceId buff, uint32_t offset, uint32_t len, rdctype::array<byte> *data);
		
		bool SaveTexture(ResourceId tex, uint32_t saveMip, const wchar_t *path);

		bool GetCBufferVariableContents(ResourceId shader, uint32_t cbufslot, ResourceId buffer, uint32_t offs, rdctype::array<ShaderVariable> *vars);
	
		ReplayOutput *CreateOutput(void *handle);
	private:
		ReplayCreateStatus PostCreateInit(IReplayDriver *device);
		
		FetchDrawcall *GetDrawcallByEID(uint32_t eventID, uint32_t defEventID);
		FetchDrawcall *SetupDrawcallPointers(FetchFrameInfo frame, rdctype::array<FetchDrawcall> &draws, FetchDrawcall *parent, FetchDrawcall *previous);
	
		IReplayDriver *GetDevice() { return m_pDevice; }
		
		struct FrameRecord
		{
			FetchFrameInfo frameInfo;

			rdctype::array<FetchDrawcall> m_DrawCallList;
		};
		vector<FrameRecord> m_FrameRecord;
		vector<FetchDrawcall*> m_Drawcalls;

		uint32_t m_FrameID;
		uint32_t m_EventID;
		ResourceId m_DeferredCtx;
		uint32_t m_FirstDeferredEvent;
		uint32_t m_LastDeferredEvent;
		
		D3D11PipelineState m_D3D11PipelineState;
		GLPipelineState m_GLPipelineState;

		std::vector<ReplayOutput *> m_Outputs;

		std::vector<FetchBuffer> m_Buffers;
		std::vector<FetchTexture> m_Textures;

		IReplayDriver *m_pDevice;

		std::set<ResourceId> m_TargetResources;
		std::set<ResourceId> m_CustomShaders;

		friend struct ReplayOutput;
};
