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
#include "vk_hookset.h"
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

class WrappedVulkan
{
private:
	const VulkanFunctions &m_Real;
	
	friend class VulkanReplay;
	
	enum {
		eInitialContents_Copy = 0,
		eInitialContents_ClearColorImage,
		eInitialContents_ClearDepthStencilImage,
	};

	Serialiser *m_pSerialiser;
	LogState m_State;
	
	VulkanReplay m_Replay;

	VkInitParams m_InitParams;

	VkResourceRecord *m_InstanceRecord;
		
	ResourceId m_ContextResourceID;
	VkResourceRecord *m_ContextRecord;
	Chunk *m_HeaderChunk;

	// we record the command buffer records so we can insert them
	// individually, that means even if they were recorded locklessly
	// in parallel, on replay they are disjoint and it makes things
	// much easier to process (we will enforce/display ordering
	// by queue submit order anyway, so it's OK to lose the record
	// order).
	vector<VkResourceRecord *> m_CmdBufferRecords;

	VulkanResourceManager *m_ResourceManager;
	
	uint32_t m_FrameCounter;

	uint64_t m_CurFileSize;

	PerformanceTimer m_FrameTimer;
	vector<double> m_FrameTimes;
	double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;

	vector<FetchFrameRecord> m_FrameRecord;

	struct ReplayData
	{
		// VKTODOHIGH need to release/destroy these somewhere
		ReplayData() : inst(VK_NULL_HANDLE), phys(VK_NULL_HANDLE), dev(VK_NULL_HANDLE), q(VK_NULL_HANDLE), cmd(VK_NULL_HANDLE), cmdpool(VK_NULL_HANDLE) {}
		VkInstance inst;
		VkPhysicalDevice phys;
		VkDevice dev;
		VkQueue q;
		VkCmdBuffer cmd;
		VkCmdPool cmdpool;
	};
	vector<ReplayData> m_PhysicalReplayData;
	int m_SwapPhysDevice;

	VkDbgMsgCallback m_MsgCallback;
	
	struct ExtensionSupport
	{
		vector<VkExtensionProperties> renderdoc;
		vector<VkExtensionProperties> driver;
		vector<VkExtensionProperties> extensions;
	};
	ExtensionSupport globalExts;
	map<ResourceId, ExtensionSupport> deviceExts;

	VkDevice GetDev()    { RDCASSERT(m_SwapPhysDevice >= 0); return m_PhysicalReplayData[m_SwapPhysDevice].dev; }
	VkQueue GetQ()       { RDCASSERT(m_SwapPhysDevice >= 0); return m_PhysicalReplayData[m_SwapPhysDevice].q;   }
	VkCmdBuffer GetCmd(){ RDCASSERT(m_SwapPhysDevice >= 0); return m_PhysicalReplayData[m_SwapPhysDevice].cmd; }

	ResourceId m_FakeBBImgId;
	VkImage m_FakeBBIm;
	VkExtent3D m_FakeBBExtent;
	void GetFakeBB(ResourceId &id, VkImage &im, VkExtent3D &extent)
	{ id = m_FakeBBImgId; im = m_FakeBBIm; extent = m_FakeBBExtent; }
	
	map<ResourceId, MemState> m_MemoryInfo;
	map<ResourceId, ImgState> m_ImageInfo;

	struct CmdBufferInfo
	{
		VkDevice device;
		VkCmdBufferCreateInfo createInfo;
		
		vector< pair<ResourceId, ImageRegionState> > imgtransitions;

		// used on replay
		DrawcallTreeNode *draw; // the root draw to copy from when submitting
		uint32_t eventCount; // how many events are in this cmd buffer, for quick skipping
		uint32_t drawCount; // similar to above
	};
	map<ResourceId, CmdBufferInfo> m_CmdBufferInfo;

	// on replay, the current command buffer we're handling (we know
	// that these don't overlap as that disjoint ordering is guaranteed
	// on capture).
	ResourceId m_CurCmdBufferID;

	struct PartialReplayData
	{
		// if we're doing a partial replay, by definition only one command
		// buffer will be partial at any one time. While replaying through
		// the command buffer chunks, the partial command buffer will be
		// created as a temporary new command buffer and when it comes to
		// the queue that should submit it, it can submit this instead.
		VkCmdBuffer resultPartialCmdBuffer;
		VkDevice partialDevice; // device for above cmd buffer

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
		// VKTODO change this to a sorted vector similar to the image
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
				compute.pipeline = graphics.pipeline = renderPass = framebuffer = 
					dynamicVP = dynamicRS = dynamicCB = dynamicDS = ResourceId();
				compute.descSets.clear();
				graphics.descSets.clear();

				RDCEraseEl(renderArea);

				RDCEraseEl(ibuffer);
				vbuffers.clear();
			}

			ResourceId dynamicVP;
			ResourceId dynamicRS;
			ResourceId dynamicCB;
			ResourceId dynamicDS;

			ResourceId renderPass;
			ResourceId framebuffer;
			VkRect2D renderArea;

			struct
			{
				ResourceId pipeline;
				// VKTODOMED might need something more sophisticated here, for
				// dynamic offsets
				vector<ResourceId> descSets;
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

	bool IsPartialCmd(ResourceId cmdid) { return cmdid == m_PartialReplayData.partialParent; }
	bool InPartialRange() { return m_CurEventID <= m_LastEventID - m_PartialReplayData.baseEvent; }
	VkCmdBuffer PartialCmdBuf() { return m_PartialReplayData.resultPartialCmdBuffer; }

	struct SwapInfo
	{
		VkFormat format;
		VkExtent2D extent;
		int arraySize;

		struct SwapImage
		{
			VkDeviceMemory mem;
			VkImage im;
			ImageRegionState state;
		};
		vector<SwapImage> images;
	};
	map<ResourceId, SwapInfo> m_SwapChainInfo;

	struct DescriptorSetSlot
	{
		DescriptorSetSlot() : type(DescSetSlot_None)
		{
			RDCEraseEl(a);
		}
		DescriptorSlotType type;

		union
		{
			uint64_t a;
		};
	};

	struct DescriptorSetInfo
	{
		DescriptorSetSlot *slots;
		uint32_t slotCount;
	};
	map<ResourceId, DescriptorSetInfo> m_DescSetInfo;

	VulkanCreationInfo m_CreationInfo;

	set<ResourceId> m_SubmittedFences;
		
	static const char *GetChunkName(uint32_t idx);
	
	Serialiser *GetSerialiser() { return m_pSerialiser; }

	void Serialise_CaptureScope(uint64_t offset);
	bool HasSuccessfulCapture();
	void AttemptCapture();
	bool Serialise_BeginCaptureFrame(bool applyInitialState);
	void BeginCaptureFrame();
	void FinishCapture();
	void EndCaptureFrame(VkImage presentImage);

	void SerialiseDescriptorSlot(DescriptorSetSlot *slot);
	
	// replay
		
	vector<FetchAPIEvent> m_CurEvents, m_Events;
	bool m_AddedDrawcall;

	uint64_t m_CurChunkOffset;
	uint32_t m_CurEventID, m_CurDrawcallID;
	uint32_t m_FirstEventID, m_LastEventID;
		
	DrawcallTreeNode m_ParentDrawcall;

	void RefreshIDs(vector<DrawcallTreeNode> &nodes, uint32_t baseEventID, uint32_t baseDrawID);

	list<DrawcallTreeNode *> m_DrawcallStack;
	
	void ProcessChunk(uint64_t offset, VulkanChunkType context);
	void ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial);
	void ContextProcessChunk(uint64_t offset, VulkanChunkType chunk, bool forceExecute);
	void AddDrawcall(FetchDrawcall d, bool hasEvents);
	void AddEvent(VulkanChunkType type, string description, ResourceId ctx = ResourceId());
		
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
	
	static void VKAPI DebugCallbackStatic(
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
	WrappedVulkan(const VulkanFunctions &real, const char *logFilename);
	~WrappedVulkan();

	ResourceId GetContextResourceID() { return m_ContextResourceID; }

	VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
	
	VulkanReplay *GetReplay() { return &m_Replay; }
	
	// replay interface
	bool Prepare_InitialState(VkResource res);
	bool Serialise_InitialState(VkResource res);
	void Create_InitialState(ResourceId id, VkResource live, bool hasData);
	void Apply_InitialState(VkResource live, VulkanResourceManager::InitialContentData initial);

	bool ReleaseResource(VkResource res);

	void Initialise(VkInitParams &params);
	void ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType);
	void ReadLogInitialisation();

	vector<FetchFrameRecord> &GetFrameRecord() { return m_FrameRecord; }
	FetchAPIEvent GetEvent(uint32_t eventID);

	void DestroyObject(VkResource res, ResourceId id);
	
	// Device initialization

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateInstance(
		const VkInstanceCreateInfo*                 pCreateInfo,
		VkInstance*                                 pInstance));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyInstance(
		VkInstance                                  instance));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageFormatProperties*                    pImageFormatProperties));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceLimits(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceLimits*                     pLimits));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceQueueCount(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceQueueProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    count,
    VkPhysicalDeviceQueueProperties*            pQueueProperties));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties));

	// Device functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDevice(
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		VkDevice*                                   pDevice));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDevice(
		VkDevice                                    device));
	
	// Extension discovery functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetGlobalExtensionProperties(
			const char*                                 pLayerName,
			uint32_t*                                   pCount,
			VkExtensionProperties*                      pProperties));

	// Queue functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetDeviceQueue(
			VkDevice                                    device,
			uint32_t                                    queueNodeIndex,
			uint32_t                                    queueIndex,
			VkQueue*                                    pQueue));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueSubmit(
			VkQueue                                     queue,
			uint32_t                                    cmdBufferCount,
			const VkCmdBuffer*                          pCmdBuffers,
			VkFence                                     fence));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueAddMemReferences(
			VkQueue                                     queue,
			uint32_t                                    count,
			const VkDeviceMemory*                       pMems));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueRemoveMemReferences(
			VkQueue                                     queue,
			uint32_t                                    count,
			const VkDeviceMemory*                       pMems));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueWaitIdle(
			VkQueue                                     queue));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDeviceWaitIdle(
			VkDevice                                    device));

	// Semaphore functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroySemaphore(
			VkDevice                                    device,
			VkSemaphore                                 semaphore));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueSignalSemaphore(
			VkQueue                                     queue,
			VkSemaphore                                 semaphore));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueueWaitSemaphore(
			VkQueue                                     queue,
			VkSemaphore                                 semaphore));

	// Memory functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocMemory(
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkFreeMemory(
			VkDevice                                    device,
			VkDeviceMemory                              mem));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkMapMemory(
			VkDevice                                    device,
			VkDeviceMemory                              mem,
			VkDeviceSize                                offset,
			VkDeviceSize                                size,
			VkMemoryMapFlags                            flags,
			void**                                      ppData));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkUnmapMemory(
			VkDevice                                    device,
			VkDeviceMemory                              mem));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetBufferMemoryRequirements(
			VkDevice                                    device,
			VkBuffer                                    buffer,
			VkMemoryRequirements*                       pMemoryRequirements));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetImageMemoryRequirements(
			VkDevice                                    device,
			VkImage                                     image,
			VkMemoryRequirements*                       pMemoryRequirements));

	// Memory management API functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset));

	// Buffer functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateBuffer(
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyBuffer(
			VkDevice                                    device,
			VkBuffer                                    buffer));

	// Buffer view functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateBufferView(
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyBufferView(
			VkDevice                                    device,
			VkBufferView                                view));

	// Image functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateImage(
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyImage(
			VkDevice                                    device,
			VkImage                                     image));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetImageSubresourceLayout(
			VkDevice                                    device,
			VkImage                                     image,
			const VkImageSubresource*                   pSubresource,
			VkSubresourceLayout*                        pLayout));

	// Image view functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateImageView(
			VkDevice                                    device,
			const VkImageViewCreateInfo*                pCreateInfo,
			VkImageView*                                pView));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyImageView(
			VkDevice                                    device,
			VkImageView                                 view));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateAttachmentView(
			VkDevice                                    device,
			const VkAttachmentViewCreateInfo*           pCreateInfo,
			VkAttachmentView*                           pView));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyAttachmentView(
			VkDevice                                    device,
			VkAttachmentView                            view));

	// Shader functions
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateShaderModule(
			VkDevice                                    device,
			const VkShaderModuleCreateInfo*             pCreateInfo,
			VkShaderModule*                             pShaderModule));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyShaderModule(
			VkDevice                                    device,
			VkShaderModule                              shaderModule));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateShader(
			VkDevice                                    device,
			const VkShaderCreateInfo*                   pCreateInfo,
			VkShader*                                   pShader));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyShader(
			VkDevice                                    device,
			VkShader                                    shader));

	// Pipeline functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateGraphicsPipelines(
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache,
			uint32_t                                    count,
			const VkGraphicsPipelineCreateInfo*         pCreateInfos,
			VkPipeline*                                 pPipelines));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyPipeline(
			VkDevice                                    device,
			VkPipeline                                  pipeline));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreatePipelineCache(
			VkDevice                                    device,
			const VkPipelineCacheCreateInfo*            pCreateInfo,
			VkPipelineCache*                            pPipelineCache));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyPipelineCache(
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache));

	// Pipeline layout functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreatePipelineLayout(
			VkDevice                                    device,
			const VkPipelineLayoutCreateInfo*           pCreateInfo,
			VkPipelineLayout*                           pPipelineLayout));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyPipelineLayout(
			VkDevice                                    device,
			VkPipelineLayout                            pipelineLayout));

	// Sampler functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSampler(
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroySampler(
			VkDevice                                    device,
			VkSampler                                   sampler));

	// Descriptor set functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorSetLayout(
			VkDevice                                    device,
			const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
			VkDescriptorSetLayout*                      pSetLayout));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDescriptorSetLayout(
			VkDevice                                    device,
			VkDescriptorSetLayout                       setLayout));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDescriptorPool(
			VkDevice                                    device,
			VkDescriptorPoolUsage                       poolUsage,
			uint32_t                                    maxSets,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDescriptorPool(
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAllocDescriptorSets(
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool,
			VkDescriptorSetUsage                        setUsage,
			uint32_t                                    count,
			const VkDescriptorSetLayout*                pSetLayouts,
			VkDescriptorSet*                            pDescriptorSets,
			uint32_t*                                   pCount));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkUpdateDescriptorSets(
			VkDevice                                    device,
			uint32_t                                    writeCount,
			const VkWriteDescriptorSet*                 pDescriptorWrites,
			uint32_t                                    copyCount,
			const VkCopyDescriptorSet*                  pDescriptorCopies));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkFreeDescriptorSets(
			VkDevice                                    device,
			VkDescriptorPool                            descriptorPool,
			uint32_t                                    count,
			const VkDescriptorSet*                      pDescriptorSets));

	// State object functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDynamicViewportState(
			VkDevice                                    device,
			const VkDynamicViewportStateCreateInfo*           pCreateInfo,
			VkDynamicViewportState*                           pState));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDynamicViewportState(
			VkDevice                                    device,
			VkDynamicViewportState                      state));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDynamicRasterState(
			VkDevice                                    device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDynamicRasterState(
			VkDevice                                    device,
			VkDynamicRasterState                        state));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDynamicColorBlendState(
			VkDevice                                    device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDynamicColorBlendState(
			VkDevice                                    device,
			VkDynamicColorBlendState                    state));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateDynamicDepthStencilState(
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyDynamicDepthStencilState(
			VkDevice                                    device,
			VkDynamicDepthStencilState                  state));

	// Command pool functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateCommandPool(
			VkDevice                                  device,
			const VkCmdPoolCreateInfo*                pCreateInfo,
			VkCmdPool*                                pCmdPool));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyCommandPool(
			VkDevice                                  device,
			VkCmdPool                                 VkCmdPool));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetCommandPool(
			VkDevice                                  device,
			VkCmdPool                                 VkCmdPool,
    	VkCmdPoolResetFlags                       flags));

	// Command buffer functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateCommandBuffer(
			VkDevice                                    device,
			const VkCmdBufferCreateInfo*                pCreateInfo,
			VkCmdBuffer*                                pCmdBuffer));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyCommandBuffer(
			VkDevice                                    device,
			VkCmdBuffer                                 cmdBuffer));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkBeginCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkEndCommandBuffer(
			VkCmdBuffer                                 cmdBuffer));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkResetCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
    	VkCmdBufferResetFlags                       flags));

	// Command buffer building functions

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindPipeline(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                        dynamicRasterState));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindDescriptorSets(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipelineLayout                            layout,
			uint32_t                                    firstSet,
			uint32_t                                    setCount,
			const VkDescriptorSet*                      pDescriptorSets,
			uint32_t                                    dynamicOffsetCount,
			const uint32_t*                             pDynamicOffsets));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindIndexBuffer(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset,
			VkIndexType                                 indexType));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBindVertexBuffers(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    startBinding,
			uint32_t                                    bindingCount,
			const VkBuffer*                             pBuffers,
			const VkDeviceSize*                         pOffsets));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDraw(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    firstVertex,
			uint32_t                                    vertexCount,
			uint32_t                                    firstInstance,
			uint32_t                                    instanceCount));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexed(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    firstIndex,
			uint32_t                                    indexCount,
			int32_t                                     vertexOffset,
			uint32_t                                    firstInstance,
			uint32_t                                    instanceCount));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset,
			uint32_t                                    count,
			uint32_t                                    stride));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDrawIndexedIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset,
			uint32_t                                    count,
			uint32_t                                    stride));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatch(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    x,
			uint32_t                                    y,
			uint32_t                                    z));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDispatchIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBuffer(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferCopy*                         pRegions));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBlitImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkTexFilter                                 filter));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyBufferToImage(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdCopyImageToBuffer(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearColorImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearDepthStencilImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearColorAttachment(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    colorAttachment,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdClearDepthStencilAttachment(
			VkCmdBuffer                                 cmdBuffer,
			VkImageAspectFlags                          imageAspectMask,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdPipelineBarrier(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			VkBool32                                    byRegion,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyFramebuffer(
			VkDevice                                    device,
			VkFramebuffer                               framebuffer));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroyRenderPass(
			VkDevice                                    device,
			VkRenderPass                                renderPass));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdBeginRenderPass(
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer));

	// Debug functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDbgCreateMsgCallback(
    VkInstance                          instance,
    VkFlags                             msgFlags,
    const PFN_vkDbgMsgCallback          pfnMsgCallback,
    void*                               pUserData,
    VkDbgMsgCallback*                   pMsgCallback));
	
	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDbgDestroyMsgCallback(
    VkInstance                          instance,
    VkDbgMsgCallback                    msgCallback));
	
	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDbgMarkerBegin(
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker));

	IMPLEMENT_FUNCTION_SERIALISED(void, vkCmdDbgMarkerEnd(
			VkCmdBuffer  cmdBuffer));

	// WSI functions

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetPhysicalDeviceSurfaceSupportWSI(
			VkPhysicalDevice                        physicalDevice,
			uint32_t                                queueFamilyIndex,
			const VkSurfaceDescriptionWSI*          pSurfaceDescription,
			VkBool32*                               pSupported));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkCreateSwapChainWSI(
			VkDevice                                device,
			const VkSwapChainCreateInfoWSI*         pCreateInfo,
			VkSwapChainWSI*                         pSwapChain));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkDestroySwapChainWSI(
			VkDevice                                 device,
			VkSwapChainWSI                           swapChain));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSurfaceInfoWSI(
			VkDevice                                 device,
			const VkSurfaceDescriptionWSI*           pSurfaceDescription,
			VkSurfaceInfoTypeWSI                     infoType,
			size_t*                                  pDataSize,
			void*                                    pData));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkGetSwapChainInfoWSI(
			VkDevice                                 device,
			VkSwapChainWSI                           swapChain,
			VkSwapChainInfoTypeWSI                   infoType,
			size_t*                                  pDataSize,
			void*                                    pData));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkAcquireNextImageWSI(
			VkDevice                                 device,
			VkSwapChainWSI                           swapChain,
			uint64_t                                 timeout,
			VkSemaphore                              semaphore,
			uint32_t*                                pImageIndex));

	IMPLEMENT_FUNCTION_SERIALISED(VkResult, vkQueuePresentWSI(
			VkQueue                                 queue,
			VkPresentInfoWSI*                       pPresentInfo));
};
