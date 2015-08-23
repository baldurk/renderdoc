/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include "vk_common.h"
#include "api/replay/renderdoc_replay.h"
#include "replay/replay_driver.h"
#include "core/core.h"

#if defined(WIN32)

#include <windows.h>
#define WINDOW_HANDLE_DECL HWND wnd;
#define NULL_WND_HANDLE NULL

#elif defined(__linux__)

#include <X11/Xlib.h>
#define WINDOW_HANDLE_DECL Display *display; Drawable wnd;
#define NULL_WND_HANDLE 0

#endif

#include <map>
using std::map;

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the debugbreak.
#define VULKANNOTIMP(...) RDCDEBUG("Vulkan not implemented - " __VA_ARGS__)

class WrappedVulkan;
struct VulkanFunctions;

class VulkanReplay : public IReplayDriver
{
	public:
		VulkanReplay();

		void SetProxy(bool p) { m_Proxy = p; }
		bool IsRemoteProxy() { return m_Proxy; }

		void Shutdown();

		void SetDriver(WrappedVulkan *d) { m_pDriver = d; }
	
		APIProperties GetAPIProperties();

		vector<ResourceId> GetBuffers();
		FetchBuffer GetBuffer(ResourceId id);

		vector<ResourceId> GetTextures();
		FetchTexture GetTexture(ResourceId id);

		ShaderReflection *GetShader(ResourceId id);
		
		vector<EventUsage> GetUsage(ResourceId id);

		vector<FetchFrameRecord> GetFrameRecord();
		vector<DebugMessage> GetDebugMessages();

		void SavePipelineState();
		D3D11PipelineState GetD3D11PipelineState() { return D3D11PipelineState(); }
		GLPipelineState GetGLPipelineState() { return GLPipelineState(); }

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
				
		vector<uint32_t> EnumerateCounters();
		void DescribeCounter(uint32_t counterID, CounterDescription &desc);
		vector<CounterResult> FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counters);

		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval);
		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram);
		
		MeshFormat GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage);
		
		vector<byte> GetBufferData(ResourceId buff, uint32_t offset, uint32_t len);
		byte *GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool resolve, bool forceRGBA8unorm, float blackPoint, float whitePoint, size_t &dataSize);
		
		void ReplaceResource(ResourceId from, ResourceId to);
		void RemoveReplacement(ResourceId id);

		void RenderMesh(uint32_t frameID, uint32_t eventID, const vector<MeshFormat> &secondaryDraws, MeshDisplay cfg);
		
		void BuildTargetShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		void BuildCustomShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors);
		void FreeCustomShader(ResourceId id);

		bool RenderTexture(TextureDisplay cfg);

		void RenderCheckerboard(Vec3f light, Vec3f dark);

		void RenderHighlightBox(float w, float h, float scale);
		
		void FillCBufferVariables(ResourceId shader, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data);
		
		vector<PixelModification> PixelHistory(uint32_t frameID, vector<EventUsage> events, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip, uint32_t sampleIdx);
		ShaderDebugTrace DebugVertex(uint32_t frameID, uint32_t eventID, uint32_t vertid, uint32_t instid, uint32_t idx, uint32_t instOffset, uint32_t vertOffset);
		ShaderDebugTrace DebugPixel(uint32_t frameID, uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive);
		ShaderDebugTrace DebugThread(uint32_t frameID, uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3]);
		void PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace, uint32_t mip, uint32_t sample, float pixel[4]);
		uint32_t PickVertex(uint32_t frameID, uint32_t eventID, MeshDisplay cfg, uint32_t x, uint32_t y);
			
		ResourceId RenderOverlay(ResourceId cfg, TextureDisplayOverlay overlay, uint32_t frameID, uint32_t eventID, const vector<uint32_t> &passEvents);
		ResourceId ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip);
			
		ResourceId CreateProxyTexture(FetchTexture templateTex);
		void SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data, size_t dataSize);
		
		ResourceId CreateProxyBuffer(FetchBuffer templateBuf);
		void SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize);

		bool IsRenderOutput(ResourceId id);
		
		void FileChanged();
	
		void InitCallstackResolver();
		bool HasCallstacks();
		Callstack::StackResolver *GetCallstackResolver();
	private:
		struct OutputWindow
		{
			OutputWindow();

			void SetCol(VkDeviceMemory mem, VkImage img);
			void SetDS(VkDeviceMemory mem, VkImage img);
			void MakeTargets(const VulkanFunctions &vk, VkDevice device, bool depth);

			void SetWindowHandle(void *wn);

			WINDOW_HANDLE_DECL

			int32_t width, height;

			VkImage colimg;
			VkDeviceMemory colmem;
			VkAttachmentView colview;
			VkImageMemoryBarrier coltrans;
			VkImage dsimg;
			VkDeviceMemory dsmem;
			VkAttachmentView dsview;
			VkImageMemoryBarrier depthtrans;
			VkImageMemoryBarrier stenciltrans;
		};

		map<uint64_t, OutputWindow> m_OutputWindows;
		uint64_t m_OutputWinID;
		uint64_t m_ActiveWinID;
		bool m_BindDepth;

		bool m_Proxy;
		
		WrappedVulkan *m_pDriver;
};
