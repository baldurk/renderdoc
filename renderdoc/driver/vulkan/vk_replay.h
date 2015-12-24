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
#include "vk_info.h"
#include "api/replay/renderdoc_replay.h"
#include "replay/replay_driver.h"
#include "core/core.h"

#ifdef WIN32
// undefined clashing windows #defines
#undef CreateEvent
#undef CreateSemaphore
#endif

#include <vulkan/vk_layer.h>

#if defined(WIN32)

#include <windows.h>
#define WINDOW_HANDLE_DECL HWND wnd;
#define NULL_WND_HANDLE NULL

#elif defined(__linux__)

#include <xcb/xcb.h>
#define WINDOW_HANDLE_DECL xcb_connection_t *connection; xcb_screen_t *screen; xcb_window_t wnd;
#define NULL_WND_HANDLE xcb_window_t(0)

#endif

#include <map>
using std::map;

// similar to RDCUNIMPLEMENTED but for things that are hit often so we don't want to fire the debugbreak.
#define VULKANNOTIMP(...) do { static bool msgprinted = false; if(!msgprinted) RDCDEBUG("Vulkan not implemented - " __VA_ARGS__); msgprinted = true; } while(0)

// allows easy disabling of MSAA
#define VULKAN_MESH_VIEW_SAMPLES VK_SAMPLE_COUNT_1_BIT

class WrappedVulkan;
class VulkanDebugManager;
class VulkanResourceManager;

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

		ShaderReflection *GetShader(ResourceId shader, string entryPoint);
		
		vector<EventUsage> GetUsage(ResourceId id);

		vector<FetchFrameRecord> GetFrameRecord();
		vector<DebugMessage> GetDebugMessages();

		void SavePipelineState();
		D3D11PipelineState GetD3D11PipelineState() { return m_D3D11PipelineState; }
		GLPipelineState GetGLPipelineState() { return GLPipelineState(); }
		VulkanPipelineState GetVulkanPipelineState() { return m_VulkanPipelineState; }

		void FreeTargetResource(ResourceId id);

		void ReadLogInitialisation();
		void SetContextFilter(ResourceId id, uint32_t firstDefEv, uint32_t lastDefEv);
		void ReplayLog(uint32_t frameID, uint32_t endEventID, ReplayLogType replayType);

		vector<uint32_t> GetPassEvents(uint32_t frameID, uint32_t eventID);

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
		void InitPostVSBuffers(uint32_t frameID, const vector<uint32_t> &passEvents);

		ResourceId GetLiveID(ResourceId id);
				
		vector<uint32_t> EnumerateCounters();
		void DescribeCounter(uint32_t counterID, CounterDescription &desc);
		vector<CounterResult> FetchCounters(uint32_t frameID, uint32_t minEventID, uint32_t maxEventID, const vector<uint32_t> &counters);

		bool GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval);
		bool GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram);
		
		MeshFormat GetPostVSBuffers(uint32_t frameID, uint32_t eventID, uint32_t instID, MeshDataStage stage);
		
		void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData);
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
		
		void FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot, vector<ShaderVariable> &outvars, const vector<byte> &data);
		
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

		// called before any VkDevice is created, to init any counters
		static void PreDeviceInitCounters();
		// called after any VkDevice is destroyed, to do corresponding shutdown of counters
		static void PostDeviceShutdownCounters();
		// called after the VkDevice is created, to init any counters
		void PostDeviceInitCounters();
	private:
		struct OutputWindow
		{
			OutputWindow();

			void SetCol(VkDeviceMemory mem, VkImage img);
			void SetDS(VkDeviceMemory mem, VkImage img);
			void Create(WrappedVulkan *driver, VkDevice device, bool depth);
			void Destroy(WrappedVulkan *driver, VkDevice device);

			// implemented in vk_replay_platform.cpp
			void CreateSurface(VkInstance inst);
			void SetWindowHandle(void *wn);

			WINDOW_HANDLE_DECL

			int32_t width, height;

			VkSurfaceKHR surface;
			VkSwapchainKHR swap;
			uint32_t numImgs;
			VkImage colimg[8];
			VkImageMemoryBarrier colBarrier[8];

			VkImage bb;
			VkImageView bbview;
			VkDeviceMemory bbmem;
			VkImageMemoryBarrier bbBarrier;
			VkFramebuffer fb, fbdepth;
			VkRenderPass rp, rpdepth;
			uint32_t curidx;

			VkImage dsimg;
			VkDeviceMemory dsmem;
			VkImageView dsview;
			VkImageMemoryBarrier depthBarrier;

			VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
			VulkanResourceManager *m_ResourceManager;
		};

		VulkanPipelineState m_VulkanPipelineState;
		D3D11PipelineState m_D3D11PipelineState;

		map<uint64_t, OutputWindow> m_OutputWindows;
		uint64_t m_OutputWinID;
		uint64_t m_ActiveWinID;
		bool m_BindDepth;
		int m_DebugWidth, m_DebugHeight;
		
		// simple cache for when we need buffer data for highlighting
		// vertices, typical use will be lots of vertices in the same
		// mesh, not jumping back and forth much between meshes.
		struct HighlightCache
		{
			HighlightCache() : EID(0), buf(), offs(0), stage(eMeshDataStage_Unknown), useidx(false) {}
			uint32_t EID;
			ResourceId buf;
			uint64_t offs;
			MeshDataStage stage;
			bool useidx;

			vector<byte> data;
			vector<uint32_t> indices;
		} m_HighlightCache;

		FloatVector InterpretVertex(byte *data, uint32_t vert, MeshDisplay cfg, byte *end, bool useidx, bool &valid);

		bool m_Proxy;
		
		WrappedVulkan *m_pDriver;

		bool RenderTextureInternal(TextureDisplay cfg, VkRenderPassBeginInfo rpbegin, bool f32render);

		void CreateTexImageView(VkImageAspectFlags aspectFlags, VkImage liveIm, VulkanCreationInfo::Image &iminfo);

		void FillCBufferVariables(rdctype::array<ShaderConstant>, vector<ShaderVariable> &outvars, const vector<byte> &data, size_t &offset);
		
		// called before the VkDevice is destroyed, to shutdown any counters
		void PreDeviceShutdownCounters();
		
		VulkanDebugManager *GetDebugManager();
		VulkanResourceManager *GetResourceManager();
};
