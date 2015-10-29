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

#include <vector>

#include "common/timing.h"
#include "serialise/serialiser.h"

#include "replay/replay_driver.h"

#include "vk_common.h"
#include "vk_info.h"
#include "vk_manager.h"
#include "vk_replay.h"

using std::vector;
using std::list;

struct VkInitParams : public RDCInitParams
{
	VkInitParams();
	ReplayCreateStatus Serialise();

	void Set(const VkInstanceCreateInfo* pCreateInfo, ResourceId inst);

	static const uint32_t VK_SERIALISE_VERSION = 0x0000002;

	// version number internal to vulkan stream
	uint32_t SerialiseVersion;

	string AppName, EngineName;
	uint32_t AppVersion, EngineVersion, APIVersion;

	vector<string> Layers;
	vector<string> Extensions;
	ResourceId InstanceID;
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

// use locally cached serialiser, per-thread
#undef GET_SERIALISER
#define GET_SERIALISER localSerialiser

// must be at the start of any function that serialises
#define CACHE_THREAD_SERIALISER() Serialiser *localSerialiser = GetThreadSerialiser();

// pass the cached serialiser into Serialised_ function
#undef SERIALISED_PARAMETER
#define SERIALISED_PARAMETER Serialiser *localSerialiser,

class WrappedVulkan : public IFrameCapturer
{
private:
	friend class VulkanReplay;
	friend class VulkanDebugManager;
	
	enum {
		eInitialContents_ClearColorImage = 1,
		eInitialContents_ClearDepthStencilImage,
	};

	Serialiser *m_pSerialiser;
	LogState m_State;
	bool m_AppControlledCapture;

	uint64_t threadSerialiserTLSSlot;

	Threading::CriticalSection m_ThreadSerialisersLock;
	vector<Serialiser *> m_ThreadSerialisers;

	uint64_t tempMemoryTLSSlot;
	struct TempMem
	{
		TempMem() : memory(NULL), size(0) {}
		byte *memory;
		size_t size;
	};
	Threading::CriticalSection m_ThreadTempMemLock;
	vector<TempMem*> m_ThreadTempMem;
	
	VulkanReplay m_Replay;

	VkInitParams m_InitParams;
		
	VkResourceRecord *m_FrameCaptureRecord;
	Chunk *m_HeaderChunk;

	// we record the command buffer records so we can insert them
	// individually, that means even if they were recorded locklessly
	// in parallel, on replay they are disjoint and it makes things
	// much easier to process (we will enforce/display ordering
	// by queue submit order anyway, so it's OK to lose the record
	// order).
	Threading::CriticalSection m_CmdBufferRecordsLock;
	vector<VkResourceRecord *> m_CmdBufferRecords;

	VulkanResourceManager *m_ResourceManager;
	VulkanDebugManager *m_DebugManager;

	Threading::CriticalSection m_CapTransitionLock;
	
	uint32_t m_FrameCounter;

	PerformanceTimer m_FrameTimer;
	vector<double> m_FrameTimes;
	double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;

	vector<FetchFrameRecord> m_FrameRecord;

	struct PhysicalDeviceData
	{
		PhysicalDeviceData()
			: readbackMemIndex(0), uploadMemIndex(0), GPULocalMemIndex(0)
		{
			RDCEraseEl(features);
			RDCEraseEl(memProps);
		}
		
		uint32_t GetMemoryIndex(uint32_t resourceRequiredBitmask, uint32_t allocRequiredProps, uint32_t allocUndesiredProps); 

		// store the three most common memory indices:
		//  - memory for copying into and reading back from the GPU
		//  - memory for copying into and uploading to the GPU
		//  - memory for sitting on the GPU and never being CPU accessed
		uint32_t readbackMemIndex;
		uint32_t uploadMemIndex;
		uint32_t GPULocalMemIndex;

		VkPhysicalDeviceFeatures features;
		VkPhysicalDeviceProperties props;
		VkPhysicalDeviceMemoryProperties memProps;
	};

	VkInstance m_Instance; // the instance corresponding to this WrappedVulkan
	VkDbgMsgCallback m_DbgMsgCallback; // the instance's dbg msg callback handle
	VkDevice m_Device; // the device used for our own command buffer work
	PhysicalDeviceData m_PhysicalDeviceData; // the data about the physical device used for the above device;
	uint32_t m_QueueFamilyIdx; // the family index that we've selected in CreateDevice for our queue
	VkQueue m_Queue; // the queue used for our own command buffer work

	struct
	{
		void Reset()
		{
			m_CmdPool = VK_NULL_HANDLE;
			freecmds.clear();
			pendingcmds.clear();
			submittedcmds.clear();
		}

		VkCmdPool m_CmdPool; // the command pool used for allocating our own command buffers
	
		vector<VkCmdBuffer> freecmds;
		// -> record ->
		vector<VkCmdBuffer> pendingcmds;
		// -> submit ->
		vector<VkCmdBuffer> submittedcmds;
		// -> flush/waitidle -> freecmds
	} m_InternalCmds;

	vector<VkDeviceMemory> m_FreeMems;

	// return the pre-selected device and queue
	VkDevice GetDev()    { RDCASSERT(m_Device != VK_NULL_HANDLE); return m_Device; }
	VkQueue  GetQ()      { RDCASSERT(m_Device != VK_NULL_HANDLE); return m_Queue; }
	VkCmdBuffer GetNextCmd();
	void SubmitCmds();
	void FlushQ();

	const VkPhysicalDeviceFeatures &GetDeviceFeatures()
	{ return m_PhysicalDeviceData.features; }
	const VkPhysicalDeviceProperties &GetDeviceProps()
	{ return m_PhysicalDeviceData.props; }

	uint32_t GetReadbackMemoryIndex(uint32_t resourceRequiredBitmask);
	uint32_t GetUploadMemoryIndex(uint32_t resourceRequiredBitmask);
	uint32_t GetGPULocalMemoryIndex(uint32_t resourceRequiredBitmask);

	ResourceId m_FakeBBImgId;
		
	struct BakedCmdBufferInfo
	{
		vector<FetchAPIEvent> curEvents;
		list<DrawcallTreeNode *> drawStack;

		vector< pair<ResourceId, ImageRegionState> > imgtransitions;

		DrawcallTreeNode *draw; // the root draw to copy from when submitting
		uint32_t eventCount; // how many events are in this cmd buffer, for quick skipping
		uint32_t curEventID; // current event ID while reading or executing
		uint32_t drawCount; // similar to above
	};

	// on replay, the current command buffer for the last chunk we
	// handled.
	ResourceId m_LastCmdBufferID;
	int m_CmdBuffersInProgress;

	struct PartialReplayData
	{
		// if we're doing a partial replay, by definition only one command
		// buffer will be partial at any one time. While replaying through
		// the command buffer chunks, the partial command buffer will be
		// created as a temporary new command buffer and when it comes to
		// the queue that should submit it, it can submit this instead.
		VkCmdBuffer resultPartialCmdBuffer;
		VkDevice partialDevice; // device for above cmd buffer

		// if we're replaying just a single draw we don't go through the
		// whole original command buffers to set up the partial replay,
		// so we just set this command buffer
		VkCmdBuffer singleDrawCmdBuffer;

		// this records where in the frame a command buffer was submitted,
		// so that we know if our replay range ends in one of these ranges
		// we need to construct a partial command buffer for future
		// replaying. Note that we always have the complete command buffer
		// around - it's the bakeID itself.
		// Since we only ever record a bakeID once the key is unique - note
		// that the same command buffer could be recorded multiple times
		// a frame, so the parent command buffer ID (the one recorded in
		// vkCmd chunks) is NOT unique.
		// However, a single baked command list can be submitted multiple
		// times - so we have to have a list of base events
		// VKTODOLOW change this to a sorted vector similar to the image
		// states
		// Map from bakeID -> vector<baseEventID>
		map<ResourceId, vector<uint32_t> > cmdBufferSubmits;

		// This is just the ResourceId of the original parent command buffer
		// and it's baked id.
		// If we are in the middle of a partial replay - allows fast checking
		// in all vkCmd chunks, with the iteration through the above list
		// only in vkBegin.
		// partialParent gets reset to ResourceId() in the vkEnd so that
		// other baked command buffers from the same parent don't pick it up
		// Also reset each overall replay
		ResourceId partialParent;

		// If a partial replay is detected, this records the base of the
		// range. This both allows easily and uniquely identifying it in the
		// queuesubmit, but also allows the recording to 'rebase' the last
		// event ID by subtracting this, to know how far to record
		uint32_t baseEvent;

		// If we're doing a partial record this bool tells us when we
		// reach the vkEndCommandBuffer that we also need to end a render
		// pass.
		bool renderPassActive;

		// There is only a state while currently partially replaying, it's
		// undefined/empty otherwise.
		// All IDs are original IDs, not live.
		struct StateVector
		{
			StateVector()
			{
				compute.pipeline = graphics.pipeline = renderPass = framebuffer = ResourceId();
				compute.descSets.clear();
				graphics.descSets.clear();
				compute.offsets.clear();
				graphics.offsets.clear();

				lineWidth = 1.0f;
				RDCEraseEl(bias);
				RDCEraseEl(blendConst);
				mindepth = 0.0f; maxdepth = 1.0f;
				RDCEraseEl(front);
				RDCEraseEl(back);

				RDCEraseEl(renderArea);

				RDCEraseEl(ibuffer);
				vbuffers.clear();
			}

			// dynamic state
			vector<VkViewport> views;
			vector<VkRect2D> scissors;
			float lineWidth;
			struct { float depth, biasclamp, slope; } bias;
			float blendConst[4];
			float mindepth, maxdepth;
			struct { uint32_t compare, write, ref; } front, back;

			ResourceId renderPass;
			ResourceId framebuffer;
			VkRect2D renderArea;

			struct
			{
				ResourceId pipeline;
				vector<ResourceId> descSets;
				vector< vector<uint32_t> > offsets;
			} compute, graphics;

			struct IdxBuffer
			{
				ResourceId buf;
				VkDeviceSize offs;
				int bytewidth;
			} ibuffer;

			struct VertBuffer
			{
				ResourceId buf;
				VkDeviceSize offs;
			};
			vector<VertBuffer> vbuffers;
		} state;
	} m_PartialReplayData;

	bool IsPartialCmd(ResourceId cmdid)
	{
		return m_PartialReplayData.singleDrawCmdBuffer != VK_NULL_HANDLE ||
			cmdid == m_PartialReplayData.partialParent;
	}
	bool InPartialRange()
	{
		return m_PartialReplayData.singleDrawCmdBuffer != VK_NULL_HANDLE ||
			m_BakedCmdBufferInfo[m_PartialReplayData.partialParent].curEventID <= m_LastEventID - m_PartialReplayData.baseEvent;
	}
	VkCmdBuffer PartialCmdBuf()
	{
		if(m_PartialReplayData.singleDrawCmdBuffer != VK_NULL_HANDLE)
			return m_PartialReplayData.singleDrawCmdBuffer;
		return m_PartialReplayData.resultPartialCmdBuffer;
	}


	// this info is stored in the record on capture, but we
	// need it on replay too
	struct DescriptorSetInfo
	{
		ResourceId layout;
		vector<VkDescriptorInfo *> currentBindings;
	};

	struct MapState
	{
		MapState()
			: device(VK_NULL_HANDLE), mapOffset(0), mapSize(0), mapFlags(0)
			, mapFrame(0), mapFlushed(false), mappedPtr(NULL), refData(NULL)
		{ }
		VkDevice device;
		VkDeviceSize mapOffset, mapSize;
		VkMemoryMapFlags mapFlags;
		uint32_t mapFrame;
		bool mapFlushed;
		void *mappedPtr;
		byte *refData;
	};

	// capture-side data

	// holds the current list of mapped memory. Locked against concurrent use
	// VKTODOLOW once maps are handled properly we will know which maps must be
	// treated as coherent and this will be a vector of VkResourceRecords to
	// iterate over and flush changes out at any queuesubmit. Then the main
	// MapState can be moved into the resource record
	map<ResourceId, MapState> m_CurrentMaps;
	Threading::CriticalSection m_CurrentMapsLock;

	// used both on capture and replay side to track image layouts. Only locked
	// in capture
	map<ResourceId, ImageLayouts> m_ImageLayouts;
	Threading::CriticalSection m_ImageLayoutsLock;

	// find swapchain for an image
	map<RENDERDOC_WindowHandle, VkSwapchainKHR> m_SwapLookup;
	Threading::CriticalSection m_SwapLookupLock;

	// below are replay-side data only, doesn't have to be thread protected

	// current descriptor set contents
	map<ResourceId, DescriptorSetInfo> m_DescriptorSetState;
	// data for a baked command buffer - its drawcalls and events, ready to submit
	map<ResourceId, BakedCmdBufferInfo> m_BakedCmdBufferInfo;
	// immutable creation data
	VulkanCreationInfo m_CreationInfo;
		
	static const char *GetChunkName(uint32_t idx);
	
	// returns thread-local temporary memory
	byte *GetTempMemory(size_t s);
	template<class T> T *GetTempArray(uint32_t arraycount) { return (T*)GetTempMemory(sizeof(T)*arraycount); }
	
	Serialiser *GetThreadSerialiser();
	Serialiser *GetMainSerialiser() { return m_pSerialiser; }

	void Serialise_CaptureScope(uint64_t offset);
	bool HasSuccessfulCapture();
	void AttemptCapture();
	bool Serialise_BeginCaptureFrame(bool applyInitialState);
	void BeginCaptureFrame();
	void FinishCapture();
	void EndCaptureFrame(VkImage presentImage);

	RENDERDOC_WindowHandle GetHandleForSurface(const VkSurfaceDescriptionKHR* surf);

	void StartFrameCapture(void *dev, void *wnd);
	bool EndFrameCapture(void *dev, void *wnd);
	
	// replay
		
	vector<FetchAPIEvent> m_RootEvents, m_Events;
	bool m_AddedDrawcall;

	uint64_t m_CurChunkOffset;
	uint32_t m_RootEventID, m_RootDrawcallID;
	uint32_t m_FirstEventID, m_LastEventID;
		
	DrawcallTreeNode m_ParentDrawcall;

	void RefreshIDs(vector<DrawcallTreeNode> &nodes, uint32_t baseEventID, uint32_t baseDrawID);

	list<DrawcallTreeNode *> m_DrawcallStack;

	list<DrawcallTreeNode *> &GetDrawcallStack()
	{
		if(m_LastCmdBufferID != ResourceId())
			return m_BakedCmdBufferInfo[m_LastCmdBufferID].drawStack;

		return m_DrawcallStack;
	}
	
	void ProcessChunk(uint64_t offset, VulkanChunkType context);
	void ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
	void ContextProcessChunk(uint64_t offset, VulkanChunkType chunk, bool forceExecute);
	void AddDrawcall(FetchDrawcall d, bool hasEvents);
	void AddEvent(VulkanChunkType type, string description);
		
	// no copy semantics
	WrappedVulkan(const WrappedVulkan &);
	WrappedVulkan &operator =(const WrappedVulkan &);

	void DebugCallback(
				VkFlags             msgFlags,
				VkDbgObjectType     objType,
				uint64_t            srcObject,
				size_t              location,
				int32_t             msgCode,
				const char*         pLayerPrefix,
				const char*         pMsg);
	
	static void DebugCallbackStatic(
				VkFlags             msgFlags,
				VkDbgObjectType     objType,
				uint64_t            srcObject,
				size_t              location,
				int32_t             msgCode,
				const char*         pLayerPrefix,
				const char*         pMsg,
				void*               pUserData)
	{
		((WrappedVulkan *)pUserData)->DebugCallback(msgFlags, objType, srcObject, location, msgCode, pLayerPrefix, pMsg);
	}

public:
	WrappedVulkan(const char *logFilename);
	~WrappedVulkan();

	ResourceId GetContextResourceID() { return m_FrameCaptureRecord->GetResourceID(); }

	VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
	VulkanDebugManager *GetDebugManager() { return m_DebugManager; }
	
	VulkanReplay *GetReplay() { return &m_Replay; }
	
	// replay interface
	bool Prepare_InitialState(WrappedVkRes *res);
	bool Serialise_InitialState(WrappedVkRes *res);
	void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
	void Apply_InitialState(WrappedVkRes *live, VulkanResourceManager::InitialContentData initial);

	bool ReleaseResource(WrappedVkRes *res);

	void Initialise(VkInitParams &params);
	void Shutdown();
	void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
	void ReadLogInitialisation();

	vector<FetchFrameRecord> &GetFrameRecord() { return m_FrameRecord; }
	FetchAPIEvent GetEvent(uint32_t eventID);

	// Device initialization

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateInstance,
		const VkInstanceCreateInfo*                 pCreateInfo,
		VkInstance*                                 pInstance);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyInstance,
		VkInstance                                  instance);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkEnumeratePhysicalDevices,
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceFeatures,
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceFormatProperties,
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceImageFormatProperties,
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          flags,
    VkImageFormatProperties*                    pImageFormatProperties);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSparseImageFormatProperties,
			VkPhysicalDevice                            physicalDevice,
			VkFormat                                    format,
			VkImageType                                 type,
			uint32_t                                    samples,
			VkImageUsageFlags                           usage,
			VkImageTiling                               tiling,
			uint32_t*                                   pNumProperties,
			VkSparseImageFormatProperties*              pProperties);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceProperties,
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceQueueFamilyProperties,
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount,
    VkQueueFamilyProperties*                    pQueueFamilyProperties);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceMemoryProperties,
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties);

	// Device functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDevice,
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		VkDevice*                                   pDevice);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDevice,
		VkDevice                                    device);
	
	// Queue functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetDeviceQueue,
			VkDevice                                    device,
			uint32_t                                    queueFamilyIndex,
			uint32_t                                    queueIndex,
			VkQueue*                                    pQueue);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueSubmit,
			VkQueue                                     queue,
			uint32_t                                    cmdBufferCount,
			const VkCmdBuffer*                          pCmdBuffers,
			VkFence                                     fence);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueAddMemReferences,
			VkQueue                                     queue,
			uint32_t                                    count,
			const VkDeviceMemory*                       pMems);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueRemoveMemReferences,
			VkQueue                                     queue,
			uint32_t                                    count,
			const VkDeviceMemory*                       pMems);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueWaitIdle,
			VkQueue                                     queue);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDeviceWaitIdle,
			VkDevice                                    device);

	// Query pool functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateQueryPool,
			VkDevice                                    device,
			const VkQueryPoolCreateInfo*                pCreateInfo,
			VkQueryPool*                                pQueryPool);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyQueryPool,
			VkDevice                                    device,
			VkQueryPool                                 queryPool);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetQueryPoolResults,
			VkDevice                                    device,
			VkQueryPool                                 queryPool,
			uint32_t                                    startQuery,
			uint32_t                                    queryCount,
			size_t*                                     pDataSize,
			void*                                       pData,
			VkQueryResultFlags                          flags);

	// Semaphore functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSemaphore,
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroySemaphore,
			VkDevice                                    device,
			VkSemaphore                                 semaphore);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueSignalSemaphore,
			VkQueue                                     queue,
			VkSemaphore                                 semaphore);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueWaitSemaphore,
			VkQueue                                     queue,
			VkSemaphore                                 semaphore);

	// Fence functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateFence,
			VkDevice                                    device,
			const VkFenceCreateInfo*                    pCreateInfo,
			VkFence*                                    pFence);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyFence,
			VkDevice                                    device,
			VkFence                                     fence);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetFenceStatus,
			VkDevice                                    device,
			VkFence                                     fence);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetFences,
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkWaitForFences,
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences,
			VkBool32                                    waitAll,
			uint64_t                                    timeout);

	// Event functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateEvent,
			VkDevice                                    device,
			const VkEventCreateInfo*                    pCreateInfo,
			VkEvent*                                    pEvent);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyEvent,
			VkDevice                                    device,
			VkEvent                                     event);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetEventStatus,
			VkDevice                                    device,
			VkEvent                                     event);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkSetEvent,
			VkDevice                                    device,
			VkEvent                                     event);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetEvent,
			VkDevice                                    device,
			VkEvent                                     event);

	// Memory functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocMemory,
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkFreeMemory,
			VkDevice                                    device,
			VkDeviceMemory                              mem);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkMapMemory,
			VkDevice                                    device,
			VkDeviceMemory                              mem,
			VkDeviceSize                                offset,
			VkDeviceSize                                size,
			VkMemoryMapFlags                            flags,
			void**                                      ppData);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkUnmapMemory,
			VkDevice                                    device,
			VkDeviceMemory                              mem);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkFlushMappedMemoryRanges,
			VkDevice                                    device,
			uint32_t                                    memRangeCount,
			const VkMappedMemoryRange*                  pMemRanges);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkInvalidateMappedMemoryRanges,
			VkDevice                                    device,
			uint32_t                                    memRangeCount,
			const VkMappedMemoryRange*                  pMemRanges);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetDeviceMemoryCommitment,
			VkDevice                                    device,
			VkDeviceMemory                              memory,
			VkDeviceSize*                               pCommittedMemoryInBytes);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetBufferMemoryRequirements,
			VkDevice                                    device,
			VkBuffer                                    buffer,
			VkMemoryRequirements*                       pMemoryRequirements);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetImageMemoryRequirements,
			VkDevice                                    device,
			VkImage                                     image,
			VkMemoryRequirements*                       pMemoryRequirements);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetImageSparseMemoryRequirements,
			VkDevice                                    device,
			VkImage                                     image,
			uint32_t*                                   pNumRequirements,
			VkSparseImageMemoryRequirements*            pSparseMemoryRequirements);

	// Memory management API functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory,
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindImageMemory,
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueBindSparseBufferMemory,
		VkQueue                                     queue,
		VkBuffer                                    buffer,
		uint32_t                                    numBindings,
		const VkSparseMemoryBindInfo*               pBindInfo);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueBindSparseImageOpaqueMemory,
		VkQueue                                     queue,
		VkImage                                     image,
		uint32_t                                    numBindings,
		const VkSparseMemoryBindInfo*               pBindInfo);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueBindSparseImageMemory,
		VkQueue                                     queue,
		VkImage                                     image,
		uint32_t                                    numBindings,
		const VkSparseImageMemoryBindInfo*          pBindInfo);

	// Buffer functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateBuffer,
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyBuffer,
			VkDevice                                    device,
			VkBuffer                                    buffer);

	// Buffer view functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateBufferView,
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyBufferView,
			VkDevice                                    device,
			VkBufferView                                view);

	// Image functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateImage,
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyImage,
			VkDevice                                    device,
			VkImage                                     image);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetImageSubresourceLayout,
			VkDevice                                    device,
			VkImage                                     image,
			const VkImageSubresource*                   pSubresource,
			VkSubresourceLayout*                        pLayout);

	// Image view functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateImageView,
			VkDevice                                    device,
			const VkImageViewCreateInfo*                pCreateInfo,
			VkImageView*                                pView);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyImageView,
			VkDevice                                    device,
			VkImageView                                 view);

	// Shader functions
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateShaderModule,
			VkDevice                                    device,
			const VkShaderModuleCreateInfo*             pCreateInfo,
			VkShaderModule*                             pShaderModule);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyShaderModule,
			VkDevice                                    device,
			VkShaderModule                              shaderModule);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateShader,
			VkDevice                                    device,
			const VkShaderCreateInfo*                   pCreateInfo,
			VkShader*                                   pShader);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyShader,
			VkDevice                                    device,
			VkShader                                    shader);

	// Pipeline functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateGraphicsPipelines,
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache,
			uint32_t                                    count,
			const VkGraphicsPipelineCreateInfo*         pCreateInfos,
			VkPipeline*                                 pPipelines);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateComputePipelines,
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache,
			uint32_t                                    count,
			const VkComputePipelineCreateInfo*          pCreateInfos,
			VkPipeline*                                 pPipelines);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyPipeline,
			VkDevice                                    device,
			VkPipeline                                  pipeline);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreatePipelineCache,
			VkDevice                                    device,
			const VkPipelineCacheCreateInfo*            pCreateInfo,
			VkPipelineCache*                            pPipelineCache);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyPipelineCache,
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache);
		
	IMPLEMENT_FUNCTION_SERIALISED(size_t, vkGetPipelineCacheSize,
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPipelineCacheData,
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache,
			void*                                       pData);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkMergePipelineCaches,
			VkDevice                                    device,
			VkPipelineCache                             destCache,
			uint32_t                                    srcCacheCount,
			const VkPipelineCache*                      pSrcCaches);

	// Pipeline layout functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreatePipelineLayout,
			VkDevice                                    device,
			const VkPipelineLayoutCreateInfo*           pCreateInfo,
			VkPipelineLayout*                           pPipelineLayout);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyPipelineLayout,
			VkDevice                                    device,
			VkPipelineLayout                            pipelineLayout);

	// Sampler functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSampler,
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroySampler,
			VkDevice                                    device,
			VkSampler                                   sampler);

	// Descriptor set functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorSetLayout,
			VkDevice                                    device,
			const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
			VkDescriptorSetLayout*                      pSetLayout);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDescriptorSetLayout,
			VkDevice                                    device,
			VkDescriptorSetLayout                       setLayout);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorPool,
			VkDevice                                    device,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyDescriptorPool,
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocDescriptorSets,
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool,
			VkDescriptorSetUsage                        setUsage,
			uint32_t                                    count,
			const VkDescriptorSetLayout*                pSetLayouts,
			VkDescriptorSet*                            pDescriptorSets);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetDescriptorPool,
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkUpdateDescriptorSets,
			VkDevice                                    device,
			uint32_t                                    writeCount,
			const VkWriteDescriptorSet*                 pDescriptorWrites,
			uint32_t                                    copyCount,
			const VkCopyDescriptorSet*                  pDescriptorCopies);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkFreeDescriptorSets,
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool,
			uint32_t                                    count,
			const VkDescriptorSet*                      pDescriptorSets);

	// Command pool functions
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetRenderAreaGranularity,
			VkDevice                                    device,
			VkRenderPass                                renderPass,
			VkExtent2D*                                 pGranularity);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateCommandPool,
			VkDevice                                  device,
			const VkCmdPoolCreateInfo*                pCreateInfo,
			VkCmdPool*                                pCmdPool);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyCommandPool,
			VkDevice                                  device,
			VkCmdPool                                 VkCmdPool);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetCommandPool,
			VkDevice                                  device,
			VkCmdPool                                 VkCmdPool,
    	VkCmdPoolResetFlags                       flags);

	// Command buffer functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateCommandBuffer,
			VkDevice                                    device,
			const VkCmdBufferCreateInfo*                pCreateInfo,
			VkCmdBuffer*                                pCmdBuffer);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyCommandBuffer,
			VkDevice                                    device,
			VkCmdBuffer                                 cmdBuffer);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBeginCommandBuffer,
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkEndCommandBuffer,
			VkCmdBuffer                                 cmdBuffer);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetCommandBuffer,
			VkCmdBuffer                                 cmdBuffer,
    	VkCmdBufferResetFlags                       flags);

	// Command buffer building functions

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindPipeline,
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetViewport,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    viewportCount,
			const VkViewport*                           pViewports);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetScissor,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    scissorCount,
			const VkRect2D*                             pScissors);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetLineWidth,
			VkCmdBuffer                                 cmdBuffer,
			float                                       lineWidth);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthBias,
			VkCmdBuffer                                 cmdBuffer,
			float                                       depthBias,
			float                                       depthBiasClamp,
			float                                       slopeScaledDepthBias);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetBlendConstants,
			VkCmdBuffer                                 cmdBuffer,
			const float                                 blendConst[4]);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetDepthBounds,
			VkCmdBuffer                                 cmdBuffer,
			float                                       minDepthBounds,
			float                                       maxDepthBounds);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilCompareMask,
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilCompareMask);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilWriteMask,
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilWriteMask);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetStencilReference,
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilReference);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDescriptorSets,
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipelineLayout                            layout,
			uint32_t                                    firstSet,
			uint32_t                                    setCount,
			const VkDescriptorSet*                      pDescriptorSets,
			uint32_t                                    dynamicOffsetCount,
			const uint32_t*                             pDynamicOffsets);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindIndexBuffer,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset,
			VkIndexType                                 indexType);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindVertexBuffers,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    startBinding,
			uint32_t                                    bindingCount,
			const VkBuffer*                             pBuffers,
			const VkDeviceSize*                         pOffsets);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDraw,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    vertexCount,
			uint32_t                                    instanceCount,
			uint32_t                                    firstVertex,
			uint32_t                                    firstInstance);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexed,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    indexCount,
			uint32_t                                    instanceCount,
			uint32_t                                    firstIndex,
			int32_t                                     vertexOffset,
			uint32_t                                    firstInstance);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndirect,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset,
			uint32_t                                    count,
			uint32_t                                    stride);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirect,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset,
			uint32_t                                    count,
			uint32_t                                    stride);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatch,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    x,
			uint32_t                                    y,
			uint32_t                                    z);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatchIndirect,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBuffer,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferCopy*                         pRegions);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImage,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBlitImage,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkTexFilter                                 filter);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResolveImage,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageResolve*                       pRegions);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBufferToImage,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImageToBuffer,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdUpdateBuffer,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    destBuffer,
			VkDeviceSize                                destOffset,
			VkDeviceSize                                dataSize,
			const uint32_t*                             pData);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdFillBuffer,
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    destBuffer,
			VkDeviceSize                                destOffset,
			VkDeviceSize                                fillSize,
			uint32_t                                    data);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPushConstants,
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineLayout                            layout,
			VkShaderStageFlags                          stageFlags,
			uint32_t                                    start,
			uint32_t                                    length,
			const void*                                 values);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearColorImage,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearDepthStencilImage,
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearDepthStencilValue*             pDepthStencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearColorAttachment,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    colorAttachment,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearDepthStencilAttachment,
			VkCmdBuffer                                 cmdBuffer,
			VkImageAspectFlags                          imageAspectMask,
			VkImageLayout                               imageLayout,
			const VkClearDepthStencilValue*             pDepthStencil,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPipelineBarrier,
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			VkBool32                                    byRegion,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWriteTimestamp,
			VkCmdBuffer                                 cmdBuffer,
			VkTimestampType                             timestampType,
			VkBuffer                                    destBuffer,
			VkDeviceSize                                destOffset);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyQueryPoolResults,
			VkCmdBuffer                                 cmdBuffer,
			VkQueryPool                                 queryPool,
			uint32_t                                    startQuery,
			uint32_t                                    queryCount,
			VkBuffer                                    destBuffer,
			VkDeviceSize                                destOffset,
			VkDeviceSize                                destStride,
			VkQueryResultFlags                          flags);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginQuery,
			VkCmdBuffer                                 cmdBuffer,
			VkQueryPool                                 queryPool,
			uint32_t                                    slot,
			VkQueryControlFlags                         flags);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndQuery,
			VkCmdBuffer                                 cmdBuffer,
			VkQueryPool                                 queryPool,
			uint32_t                                    slot);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResetQueryPool,
			VkCmdBuffer                                 cmdBuffer,
			VkQueryPool                                 queryPool,
			uint32_t                                    startQuery,
			uint32_t                                    queryCount);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdSetEvent,
			VkCmdBuffer                                 cmdBuffer,
			VkEvent                                     event,
			VkPipelineStageFlags                        stageMask);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdResetEvent,
			VkCmdBuffer                                 cmdBuffer,
			VkEvent                                     event,
			VkPipelineStageFlags                        stageMask);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdWaitEvents,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    eventCount,
			const VkEvent*                              pEvents,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateFramebuffer,
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyFramebuffer,
			VkDevice                                    device,
			VkFramebuffer                               framebuffer);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateRenderPass,
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkDestroyRenderPass,
			VkDevice                                    device,
			VkRenderPass                                renderPass);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass,
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdNextSubpass,
			VkCmdBuffer                                 cmdBuffer,
			VkRenderPassContents                        contents);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdExecuteCommands,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    cmdBuffersCount,
			const VkCmdBuffer*                          pCmdBuffers);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndRenderPass,
			VkCmdBuffer                                 cmdBuffer);

	// Debug functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDbgCreateMsgCallback,
    VkInstance                          instance,
    VkFlags                             msgFlags,
    const PFN_vkDbgMsgCallback          pfnMsgCallback,
    void*                               pUserData,
    VkDbgMsgCallback*                   pMsgCallback);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDbgDestroyMsgCallback,
    VkInstance                          instance,
    VkDbgMsgCallback                    msgCallback);
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDbgMarkerBegin,
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker);

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDbgMarkerEnd,
			VkCmdBuffer  cmdBuffer);

	// Windowing extension functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfaceSupportKHR,
			VkPhysicalDevice                        physicalDevice,
			uint32_t                                queueFamilyIndex,
			const VkSurfaceDescriptionKHR*          pSurfaceDescription,
			VkBool32*                               pSupported);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSurfacePropertiesKHR,
			VkDevice                                 device,
			const VkSurfaceDescriptionKHR*           pSurfaceDescription,
			VkSurfacePropertiesKHR*                  pSurfaceProperties);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSurfaceFormatsKHR,
			VkDevice                                 device,
			const VkSurfaceDescriptionKHR*           pSurfaceDescription,
			uint32_t*                                pCount,
			VkSurfaceFormatKHR*                      pSurfaceFormats);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSurfacePresentModesKHR,
			VkDevice                                 device,
			const VkSurfaceDescriptionKHR*           pSurfaceDescription,
			uint32_t*                                pCount,
			VkPresentModeKHR*                        pPresentModes);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSwapchainKHR,
			VkDevice                                 device,
			const VkSwapchainCreateInfoKHR*          pCreateInfo,
			VkSwapchainKHR*                          pSwapchain);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroySwapchainKHR,
			VkDevice                                 device,
			VkSwapchainKHR                           swapchain);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSwapchainImagesKHR,
			VkDevice                                 device,
			VkSwapchainKHR                           swapchain,
			uint32_t*                                pCount,
			VkImage*                                 pSwapchainImages);
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAcquireNextImageKHR,
			VkDevice                                 device,
			VkSwapchainKHR                           swapChain,
			uint64_t                                 timeout,
			VkSemaphore                              semaphore,
			uint32_t*                                pImageIndex);

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueuePresentKHR,
			VkQueue                                 queue,
			VkPresentInfoKHR*                       pPresentInfo);
};
