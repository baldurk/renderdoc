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
#include "vk_manager.h"
#include "vk_replay.h"

using std::vector;
using std::list;

struct VkInitParams : public RDCInitParams
{
	VkInitParams();
	ReplayCreateStatus Serialise();

	static const uint32_t VK_SERIALISE_VERSION = 0x0000001;

	// version number internal to vulkan stream
	uint32_t SerialiseVersion;
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

	ResourceId m_DeviceResourceID;
	VkResourceRecord *m_DeviceRecord;
		
	ResourceId m_ContextResourceID;
	VkResourceRecord *m_ContextRecord;

	VulkanResourceManager *m_ResourceManager;
	
	uint32_t m_FrameCounter;

	uint64_t m_CurFileSize;

	PerformanceTimer m_FrameTimer;
	vector<double> m_FrameTimes;
	double m_TotalTime, m_AvgFrametime, m_MinFrametime, m_MaxFrametime;

	vector<FetchFrameRecord> m_FrameRecord;

	struct ReplayData
	{
		ReplayData() : phys(VK_NULL_HANDLE), dev(VK_NULL_HANDLE), q(VK_NULL_HANDLE), cmd(VK_NULL_HANDLE) {}
		VkPhysicalDevice phys;
		VkDevice dev;
		VkQueue q;
		VkCmdBuffer cmd;
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

	VkDevice GetDev()    { return m_PhysicalReplayData[m_SwapPhysDevice].dev; }
	VkQueue GetQ()       { return m_PhysicalReplayData[m_SwapPhysDevice].q;   }
	VkCmdBuffer GetCmd(){ return m_PhysicalReplayData[m_SwapPhysDevice].cmd; }

	ResourceId m_FakeBBImgId;
	VkImage m_FakeBBIm;
	VkDeviceMemory m_FakeBBMem;
	void GetFakeBB(ResourceId &id, VkImage &im, VkDeviceMemory &mem)
	{ id = m_FakeBBImgId; im = m_FakeBBIm; mem = m_FakeBBMem; }
	
	map<ResourceId, MemState> m_MemoryInfo;
	map<ResourceId, ImgState> m_ImageInfo;

	struct CmdBufferInfo
	{
		VkDevice device;
		VkCmdBufferCreateInfo createInfo;
	};
	map<ResourceId, CmdBufferInfo> m_CmdBufferInfo;

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
		
	DrawcallTreeNode m_ParentDrawcall;

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

	ResourceId GetDeviceResourceID() { return m_DeviceResourceID; }
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
