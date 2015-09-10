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

#include "vk_core.h"
#include "serialise/string_utils.h"

// VKTODOHIGH need to call vkResetCommandBuffer() before calling vkBeginCommandBuffer()

// VKTODOHIGH check final semantics for lifetimes - e.g. shaders & shader modules can be freed
// after shaders are used to create a pipeline, but for most things (views and resources,
// resources and memory, descriptor set bindings and the resources they point to) the app is
// responsible for correctly managing their lifetime, so we shouldn't have to ensure they are
// refcounted and kept around - just need to account for bugs and resources/memory/etc not
// being present on replay.
// We should ref these by ID so that we handle these records going
// away without crashing on dereference

// VKTODOLOW dirty buffers should propagate through to their memory somehow
// images can be separately dirty since we can't just copy their memory
// (tiling could be different)

static bool operator <(const VkExtensionProperties &a, const VkExtensionProperties &b)
{
	int cmp = strcmp(a.extName, b.extName);
	if(cmp == 0)
		return a.specVersion < b.specVersion;

	return cmp < 0;
}

// VKTODOLOW assertion of structure types, handling of pNext or assert == NULL

const char *VkChunkNames[] =
{
	"WrappedVulkan::Initialisation",
	"vkCreateInstance",
	"vkEnumeratePhysicalDevices",
	"vkCreateDevice",
	"vkGetDeviceQueue",
	
	"vkAllocMemory",
	"vkUnmapMemory",
	"vkFreeMemory",

	"vkCreateCommandPool",
	"vkResetCommandPool",

	"vkCreateCommandBuffer",
	"vkCreateFramebuffer",
	"vkCreateRenderPass",
	"vkCreateDescriptorPool",
	"vkCreateDescriptorSet",
	"vkCreateDescriptorSetLayout",
	"vkCreateBuffer",
	"vkCreateBufferView",
	"vkCreateImage",
	"vkCreateImageView",
	"vkCreateAttachmentView",
	"vkCreateDepthTargetView",
	"vkCreateDynamicViewportState",
	"vkCreateDynamicRasterState",
	"vkCreateDynamicBlendState",
	"vkCreateDynamicDepthStencilState",
	"vkCreateSampler",
	"vkCreateShader",
	"vkCreateShaderModule",
	"vkCreatePipelineLayout",
	"vkCreatePipelineCache",
	"vkCreateGraphicsPipelines",
	"vkCreateComputePipelines",
	"vkGetSwapChainInfoWSI",

	"vkCreateSemaphore",
	"vkCreateFence",
	"vkGetFenceStatus",
	"vkWaitForFences",

	"vkAllocDescriptorSets",
	"vkUpdateDescriptorSets",

	"vkResetCommandBuffer",
	"vkBeginCommandBuffer",
	"vkEndCommandBuffer",

	"vkQueueSignalSemaphore",
	"vkQueueWaitSemaphore",
	"vkQueueWaitIdle",
	"vkDeviceWaitIdle",

	"vkQueueSubmit",
	"vkBindBufferMemory",
	"vkBindImageMemory",

	"vkCmdBeginRenderPass",
	"vkCmdEndRenderPass",

	"vkCmdBindPipeline",
	"vkCmdBindDynamicViewportState",
	"vkCmdBindDynamicRasterState",
	"vkCmdBindDynamicColorBlendState",
	"vkCmdBindDynamicDepthStencilState",
	"vkCmdBindDescriptorSet",
	"vkCmdBindVertexBuffers",
	"vkCmdBindIndexBuffer",
	"vkCmdCopyBufferToImage",
	"vkCmdCopyImageToBuffer",
	"vkCmdCopyBuffer",
	"vkCmdCopyImage",
	"vkCmdBlitImage",
	"vkCmdClearColorImage",
	"vkCmdClearDepthStencilImage",
	"vkCmdClearColorAttachment",
	"vkCmdClearDepthStencilAttachment",
	"vkCmdPipelineBarrier",
	"vkCmdResolveImage",
	"vkCmdWriteTimestamp",
	"vkCmdDraw",
	"vkCmdDrawIndirect",
	"vkCmdDrawIndexed",
	"vkCmdDrawIndexedIndirect",
	"vkCmdDispatch",
	"vkCmdDispatchIndirect",
	
	"vkCmdDbgMarkerBegin",
	"vkCmdDbgMarker", // no equivalent function at the moment
	"vkCmdDbgMarkerEnd",

	"vkCreateSwapChainWSI",

	"Capture",
	"BeginCapture",
	"EndCapture",
};

VkInitParams::VkInitParams()
{
	SerialiseVersion = VK_SERIALISE_VERSION;
}

ReplayCreateStatus VkInitParams::Serialise()
{
	SERIALISE_ELEMENT(uint32_t, ver, VK_SERIALISE_VERSION); SerialiseVersion = ver;

	if(ver != VK_SERIALISE_VERSION)
	{
		RDCERR("Incompatible Vulkan serialise version, expected %d got %d", VK_SERIALISE_VERSION, ver);
		return eReplayCreate_APIIncompatibleVersion;
	}

	m_pSerialiser->Serialise("AppName", AppName);
	m_pSerialiser->Serialise("EngineName", EngineName);
	m_pSerialiser->Serialise("AppVersion", AppVersion);
	m_pSerialiser->Serialise("EngineVersion", EngineVersion);
	m_pSerialiser->Serialise("APIVersion", APIVersion);

	m_pSerialiser->Serialise("Layers", Layers);
	m_pSerialiser->Serialise("Extensions", Extensions);

	m_pSerialiser->Serialise("InstanceID", InstanceID);

	return eReplayCreate_Success;
}

void VkInitParams::Set(const VkInstanceCreateInfo* pCreateInfo, ResourceId inst)
{
	RDCASSERT(pCreateInfo);

	if(pCreateInfo->pAppInfo)
	{
		// VKTODOLOW handle app info next pointer
		RDCASSERT(pCreateInfo->pAppInfo->pNext == NULL);

		AppName = pCreateInfo->pAppInfo->pAppName ? pCreateInfo->pAppInfo->pAppName : "";
		EngineName = pCreateInfo->pAppInfo->pEngineName ? pCreateInfo->pAppInfo->pEngineName : "";

		AppVersion = pCreateInfo->pAppInfo->appVersion;
		EngineVersion = pCreateInfo->pAppInfo->engineVersion;
		APIVersion = pCreateInfo->pAppInfo->apiVersion;
	}
	else
	{
		AppName = "";
		EngineName = "";

		AppVersion = 0;
		EngineVersion = 0;
		APIVersion = 0;
	}

	Layers.resize(pCreateInfo->layerCount);
	Extensions.resize(pCreateInfo->extensionCount);

	for(uint32_t i=0; i < pCreateInfo->layerCount; i++)
		Layers[i] = pCreateInfo->ppEnabledLayerNames[i];

	for(uint32_t i=0; i < pCreateInfo->extensionCount; i++)
		Extensions[i] = pCreateInfo->ppEnabledExtensionNames[i];

	InstanceID = inst;
}

void WrappedVulkan::Initialise(VkInitParams &params)
{
	params.AppName = string("RenderDoc (") + params.AppName + ")";
	params.EngineName = string("RenderDoc (") + params.EngineName + ")";

	// VKTODOLOW verify that layers/extensions are available

	const char **layerscstr = new const char *[params.Layers.size()];
	for(size_t i=0; i < params.Layers.size(); i++)
		layerscstr[i] = params.Layers[i].c_str();

#if defined(FORCE_VALIDATION_LAYER)
	params.Extensions.push_back("DEBUG_REPORT");
#endif

	const char **extscstr = new const char *[params.Extensions.size()];
	for(size_t i=0; i < params.Extensions.size(); i++)
		extscstr[i] = params.Extensions[i].c_str();

	VkApplicationInfo appinfo = {
			/*.sType =*/ VK_STRUCTURE_TYPE_APPLICATION_INFO,
			/*.pNext =*/ NULL,
			/*.pAppName =*/ params.AppName.c_str(),
			/*.appVersion =*/ params.AppVersion,
			/*.pEngineName =*/ params.EngineName.c_str(),
			/*.engineVersion =*/ params.EngineVersion,
			/*.apiVersion =*/ VK_API_VERSION,
	};
	// VKTODOLOW handle other API version different to ours
	RDCASSERT(params.APIVersion == VK_API_VERSION);
	VkInstanceCreateInfo instinfo = {
			/*.sType =*/ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			/*.pNext =*/ NULL,
			/*.pAppInfo =*/ &appinfo,
			/*.pAllocCb =*/ NULL,
			/*.layerCount =*/ (uint32_t)params.Layers.size(),
			/*.ppEnabledLayerNames =*/ layerscstr,
			/*.extensionCount =*/ (uint32_t)params.Extensions.size(),
			/*.ppEnabledExtensionNames =*/ extscstr,
	};

	VkInstance inst;

	VkResult ret = m_Real.vkCreateInstance(&instinfo, &inst);

	GetResourceManager()->RegisterResource(MakeRes(inst));
	GetResourceManager()->AddLiveResource(params.InstanceID, MakeRes(inst));

	SAFE_DELETE_ARRAY(layerscstr);
	SAFE_DELETE_ARRAY(extscstr);
}

WrappedVulkan::WrappedVulkan(const VulkanFunctions &real, const char *logFilename)
	: m_Real(real)
{
#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	if(RenderDoc::Inst().IsReplayApp())
	{
		m_State = READING;
		if(logFilename)
		{
			m_pSerialiser = new Serialiser(logFilename, Serialiser::READING, debugSerialiser);
		}
		else
		{
			byte dummy[4];
			m_pSerialiser = new Serialiser(4, dummy, false);
		}
	}
	else
	{
		m_State = WRITING_IDLE;
		m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
	}

#define AddExtSupport(list, name, version) { VkExtensionProperties props = { name, version }; list.push_back(props); }
	
	AddExtSupport(globalExts.renderdoc, "VK_Renderdoc", 0);
	AddExtSupport(globalExts.renderdoc, "VK_WSI_swapchain", 0);

#undef AddExtSupport

	m_SwapPhysDevice = -1;

	uint32_t extCount = 0;
	m_Real.vkGetGlobalExtensionProperties(NULL, &extCount, NULL);

	globalExts.driver.resize(extCount);
	m_Real.vkGetGlobalExtensionProperties(NULL, &extCount, &globalExts.driver[0]);

	std::sort(globalExts.driver.begin(), globalExts.driver.end());

	for(size_t i=0; i < globalExts.driver.size(); i++)
		RDCDEBUG("Driver Ext %d: %s", i, globalExts.driver[i].extName);

	// intersection of extensions
	{
		size_t len = RDCMIN(globalExts.renderdoc.size(), globalExts.driver.size());
		for(size_t i=0, j=0; i < globalExts.renderdoc.size() && j < globalExts.driver.size(); )
		{
			string a = globalExts.renderdoc[i].extName;
			string b = globalExts.driver[j].extName;

			if(a == b)
			{
				globalExts.extensions.push_back(globalExts.renderdoc[i]);
				i++;
				j++;
			}
			else if(a < b)
			{
				i++;
			}
			else if(b < a)
			{
				j++;
			}
		}
	}

	m_Replay.SetDriver(this);

	m_FrameCounter = 0;

	m_FrameTimer.Restart();

	m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;
	
	m_CurEventID = 1;
	m_CurDrawcallID = 1;
	m_FirstEventID = 0;
	m_LastEventID = ~0U;

	m_CurCmdBufferID = ResourceId();

	m_PartialReplayData.renderPassActive = false;
	m_PartialReplayData.resultPartialCmdBuffer = VK_NULL_HANDLE;
	m_PartialReplayData.partialParent = ResourceId();
	m_PartialReplayData.baseEvent = 0;

	m_DrawcallStack.push_back(&m_ParentDrawcall);

	m_FakeBBImgId = ResourceId();
	m_FakeBBIm = VK_NULL_HANDLE;
	RDCEraseEl(m_FakeBBExtent);

	m_ResourceManager = new VulkanResourceManager(m_State, m_pSerialiser, this);
	
	m_ContextResourceID = GetResourceManager()->RegisterResource(VkResource(eResSpecial, VK_NULL_HANDLE));

	m_HeaderChunk = NULL;

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
		m_ContextRecord->DataInSerialiser = false;
		m_ContextRecord->Length = 0;
		m_ContextRecord->NumSubResources = 0;
		m_ContextRecord->SpecialResource = true;
		m_ContextRecord->SubResources = NULL;
	}
	else
	{
		m_ContextRecord = NULL;

		ResourceIDGen::SetReplayResourceIDs();
	}
		
	RDCDEBUG("Debug Text enabled - for development! remove before release!");
	m_pSerialiser->SetDebugText(true);
	
	m_pSerialiser->SetChunkNameLookup(&GetChunkName);

	//////////////////////////////////////////////////////////////////////////
	// Compile time asserts

	RDCCOMPILE_ASSERT(ARRAY_COUNT(VkChunkNames) == NUM_VULKAN_CHUNKS-FIRST_CHUNK_ID, "Not right number of chunk names");
}

WrappedVulkan::~WrappedVulkan()
{
#if defined(FORCE_VALIDATION_LAYER)
	if(m_MsgCallback != VK_NULL_HANDLE)
	{
		// VKTODOMED [0] isn't right..
		m_Real.vkDbgDestroyMsgCallback(m_PhysicalReplayData[0].inst, m_MsgCallback);
	}
#endif

	// VKTODOMED clean up replay resources here?
}

const char * WrappedVulkan::GetChunkName(uint32_t idx)
{
	if(idx < FIRST_CHUNK_ID || idx >= NUM_VULKAN_CHUNKS)
		return "<unknown>";
	return VkChunkNames[idx-FIRST_CHUNK_ID];
}

VkResult WrappedVulkan::vkCreateInstance(
		const VkInstanceCreateInfo*                 pCreateInfo,
		VkInstance*                                 pInstance)
{
	if(pCreateInfo == NULL)
		return VK_ERROR_INVALID_POINTER;

	RDCASSERT(pCreateInfo->pNext == NULL);

	VkInstance inst;

	VkResult ret = m_Real.vkCreateInstance(pCreateInfo, &inst);

	if(ret != VK_SUCCESS)
		return ret;
	
#if !defined(RELEASE)
	if(m_State >= WRITING)
	{
		CaptureOptions &opts = (CaptureOptions &)RenderDoc::Inst().GetCaptureOptions();
		opts.DebugDeviceMode = true;
	}
#endif

	// VKTODOLOW we should try and fetch vkDbgCreateMsgCallback ourselves if it isn't
	// already loaded
	if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode && m_Real.vkDbgCreateMsgCallback)
	{
		VkFlags flags = VK_DBG_REPORT_INFO_BIT |
										VK_DBG_REPORT_WARN_BIT |
										VK_DBG_REPORT_PERF_WARN_BIT |
										VK_DBG_REPORT_ERROR_BIT |
										VK_DBG_REPORT_DEBUG_BIT;
		m_Real.vkDbgCreateMsgCallback(inst, flags, &DebugCallbackStatic, this, &m_MsgCallback);
	}

	GetResourceManager()->RegisterResource(MakeRes(inst));

	if(m_State >= WRITING)
	{
		ResourceId instID = GetResourceManager()->GetID(MakeRes(inst));
		m_InitParams.Set(pCreateInfo, instID);
		m_InstanceRecord = GetResourceManager()->AddResourceRecord(instID);
	}

	*pInstance = inst;

	return VK_SUCCESS;
}

VkResult WrappedVulkan::vkDestroyInstance(
		VkInstance                                  instance)
{
	VkResult ret = m_Real.vkDestroyInstance(instance);

	if(ret != VK_SUCCESS)
		return ret;
	
	if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode && m_MsgCallback != VK_NULL_HANDLE)
	{
		m_Real.vkDbgDestroyMsgCallback(instance, m_MsgCallback);
	}

	GetResourceManager()->UnregisterResource(MakeRes(instance));

	return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
{
	SERIALISE_ELEMENT(ResourceId, inst, GetResourceManager()->GetID(MakeRes(instance)));
	SERIALISE_ELEMENT(uint32_t, physIndex, *pPhysicalDeviceCount);
	SERIALISE_ELEMENT(ResourceId, physId, GetResourceManager()->GetID(MakeRes(*pPhysicalDevices)));

	uint32_t count;
	VkPhysicalDevice devices[8]; // VKTODOLOW: dynamically allocate
	if(m_State < WRITING)
	{
		instance = (VkInstance)GetResourceManager()->GetLiveResource(inst).handle;
		VkResult ret = m_Real.vkEnumeratePhysicalDevices(instance, &count, devices);
		RDCASSERT(ret == VK_SUCCESS);

		// VKTODOLOW match up physical devices to those available on replay
	}

	VkPhysicalDevice pd = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		pd = *pPhysicalDevices;
	}
	else
	{
		pd = devices[physIndex];

		GetResourceManager()->RegisterResource(MakeRes(pd));
		GetResourceManager()->AddLiveResource(physId, MakeRes(pd));
	}

	ReplayData data;
	data.inst = instance;
	data.phys = pd;

	m_Real.vkGetPhysicalDeviceMemoryProperties(pd, &data.memProps);

	data.readbackMemIndex = data.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT);
	data.uploadMemIndex = data.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
	data.GPULocalMemIndex = data.GetMemoryIndex(~0U, VK_MEMORY_PROPERTY_DEVICE_ONLY, 0);

	m_PhysicalReplayData.push_back(data);

	return true;
}

VkResult WrappedVulkan::vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
{
	uint32_t count;
	VkPhysicalDevice devices[8]; // VKTODOLOW: dynamically allocate
	VkResult ret = m_Real.vkEnumeratePhysicalDevices(instance, &count, devices);

	if(ret != VK_SUCCESS)
		return ret;
	
	for(uint32_t i=0; i < count; i++)
	{
		GetResourceManager()->RegisterResource(MakeRes(devices[i]));

		if(m_State >= WRITING)
		{
			SCOPED_SERIALISE_CONTEXT(ENUM_PHYSICALS);
			Serialise_vkEnumeratePhysicalDevices(instance, &i, &devices[i]);

			m_InstanceRecord->AddChunk(scope.Get());
		}
	}

	if(pPhysicalDeviceCount) *pPhysicalDeviceCount = count;
	if(pPhysicalDevices) memcpy(pPhysicalDevices, devices, count*sizeof(VkPhysicalDevice));

	return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkCreateDevice(
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		VkDevice*                                   pDevice)
{
	SERIALISE_ELEMENT(ResourceId, physId, GetResourceManager()->GetID(MakeRes(physicalDevice)));
	SERIALISE_ELEMENT(VkDeviceCreateInfo, createInfo, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(*pDevice)));

	if(m_State == READING)
	{
		physicalDevice = (VkPhysicalDevice)GetResourceManager()->GetLiveResource(physId).handle;

		VkDevice device;
		
		// VKTODOHIGH: find a queue that supports graphics/compute/dma, and if one doesn't exist, add it.

		// VKTODOLOW: check that extensions supported in capture (from createInfo) are supported in replay

		VkResult ret = m_Real.vkCreateDevice(physicalDevice, &createInfo, &device);

		VkResource res = MakeRes(device);

		GetResourceManager()->RegisterResource(res);
		GetResourceManager()->AddLiveResource(devId, res);

		bool found = false;

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].phys == physicalDevice)
			{
				// VKTODOHIGH super ultra mega hyper hack of great justice
				void PopulateDeviceHooks(VkDevice d, VkInstance i);
				PopulateDeviceHooks(device, m_PhysicalReplayData[i].inst);

				m_PhysicalReplayData[i].dev = device;
				// VKTODOHIGH: shouldn't be 0, 0
				VkResult vkr = m_Real.vkGetDeviceQueue(device, 0, 0, &m_PhysicalReplayData[i].q);
				RDCASSERT(vkr == VK_SUCCESS);

				// VKTODOHIGH queueFamilyIndex
				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, 0, 0 };
				vkr = m_Real.vkCreateCommandPool(device, &poolInfo, &m_PhysicalReplayData[i].cmdpool);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, m_PhysicalReplayData[i].cmdpool, VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
				vkr = m_Real.vkCreateCommandBuffer(device, &cmdInfo, &m_PhysicalReplayData[i].cmd);
				RDCASSERT(vkr == VK_SUCCESS);

#if defined(FORCE_VALIDATION_LAYER)
				if(m_Real.vkDbgCreateMsgCallback)
				{
					VkFlags flags = VK_DBG_REPORT_INFO_BIT |
						VK_DBG_REPORT_WARN_BIT |
						VK_DBG_REPORT_PERF_WARN_BIT |
						VK_DBG_REPORT_ERROR_BIT |
						VK_DBG_REPORT_DEBUG_BIT;
					vkr = m_Real.vkDbgCreateMsgCallback(m_PhysicalReplayData[i].inst, flags, &DebugCallbackStatic, this, &m_MsgCallback);
					RDCASSERT(vkr == VK_SUCCESS);
					RDCLOG("Created dbg callback");
				}
				else
				{
					RDCLOG("No dbg callback");
				}
#endif

				found = true;
				break;
			}
		}

		if(!found)
			RDCERR("Couldn't find VkPhysicalDevice for vkCreateDevice!");
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDevice(
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		VkDevice*                                   pDevice)
{
	VkDeviceCreateInfo createInfo = *pCreateInfo;

	// VKTODOHIGH: find a queue that supports all capabilities, and if one doesn't exist, add it.
	bool found = false;

	RDCDEBUG("Might want to fiddle with createinfo - e.g. to remove VK_RenderDoc from set of extensions or similar");

	VkResult ret = m_Real.vkCreateDevice(physicalDevice, &createInfo, pDevice);

	if(ret == VK_SUCCESS)
	{
		found = false;

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].phys == physicalDevice)
			{
				m_PhysicalReplayData[i].dev = *pDevice;
				// VKTODOHIGH: shouldn't be 0, 0
				VkResult vkr = m_Real.vkGetDeviceQueue(*pDevice, 0, 0, &m_PhysicalReplayData[i].q);
				RDCASSERT(vkr == VK_SUCCESS);

				// VKTODOHIGH queueFamilyIndex
				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, 0, 0 };
				vkr = m_Real.vkCreateCommandPool(*pDevice, &poolInfo, &m_PhysicalReplayData[i].cmdpool);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, m_PhysicalReplayData[i].cmdpool, VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
				vkr = m_Real.vkCreateCommandBuffer(*pDevice, &cmdInfo, &m_PhysicalReplayData[i].cmd);
				RDCASSERT(vkr == VK_SUCCESS);
				found = true;
				break;
			}
		}

		if(!found)
			RDCERR("Couldn't find VkPhysicalDevice for vkCreateDevice!");

		VkResource res = MakeRes(*pDevice);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DEVICE);
				Serialise_vkCreateDevice(physicalDevice, &createInfo, pDevice);

				chunk = scope.Get();
			}

			m_InstanceRecord->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkDestroyDevice(VkDevice device)
{
	// VKTODOHIGH this stuff should all be in vkDestroyInstance
	if(m_State >= WRITING)
	{
		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].dev == device)
			{
				if(i == (size_t)m_SwapPhysDevice)
				{
					// VKTODOHIGH m_InstanceRecord

					if(m_ContextRecord)
					{
						RDCASSERT(m_ContextRecord->GetRefCount() == 1);
						m_ContextRecord->Delete(GetResourceManager());
						m_ContextRecord = NULL;
					}

					m_ResourceManager->Shutdown();

					m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
					m_ContextRecord->DataInSerialiser = false;
					m_ContextRecord->Length = 0;
					m_ContextRecord->NumSubResources = 0;
					m_ContextRecord->SpecialResource = true;
					m_ContextRecord->SubResources = NULL;
				}

				if(m_PhysicalReplayData[i].cmd != VK_NULL_HANDLE)
					m_Real.vkDestroyCommandBuffer(device, m_PhysicalReplayData[i].cmd);

				m_PhysicalReplayData[i] = ReplayData();
				break;
			}
		}
	}

	VkResult ret = m_Real.vkDestroyDevice(device);

	GetResourceManager()->UnregisterResource(MakeRes(device));

	return ret;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
	return m_Real.vkGetPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
	return m_Real.vkGetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageFormatProperties*                    pImageFormatProperties)
{
	return m_Real.vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, pImageFormatProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceLimits(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceLimits*                     pLimits)
{
	return m_Real.vkGetPhysicalDeviceLimits(physicalDevice, pLimits);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
	return m_Real.vkGetPhysicalDeviceProperties(physicalDevice, pProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueCount(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount)
{
	return m_Real.vkGetPhysicalDeviceQueueCount(physicalDevice, pCount);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    count,
    VkPhysicalDeviceQueueProperties*            pQueueProperties)
{
	return m_Real.vkGetPhysicalDeviceQueueProperties(physicalDevice, count, pQueueProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
	return m_Real.vkGetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

VkResult WrappedVulkan::vkGetImageSubresourceLayout(
			VkDevice                                    device,
			VkImage                                     image,
			const VkImageSubresource*                   pSubresource,
			VkSubresourceLayout*                        pLayout)
{
	return m_Real.vkGetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

VkResult WrappedVulkan::vkGetBufferMemoryRequirements(
		VkDevice                                    device,
		VkBuffer                                    buffer,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	return m_Real.vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

VkResult WrappedVulkan::vkGetImageMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	return m_Real.vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
}

VkResult WrappedVulkan::vkGetGlobalExtensionProperties(
    const char*                                 pLayerName,
		uint32_t*                                   pCount,
		VkExtensionProperties*                      pProperties)
{
	if(pLayerName == NULL)
	{
		if(pCount) *pCount = uint32_t(globalExts.extensions.size());
		if(pProperties)
			memcpy(pProperties, &globalExts.extensions[0], sizeof(VkExtensionProperties)*globalExts.extensions.size());
		return VK_SUCCESS;
	}

	return m_Real.vkGetGlobalExtensionProperties(pLayerName, pCount, pProperties);
}

void WrappedVulkan::DestroyObject(VkResource res, ResourceId id)
{
	GetResourceManager()->MarkCleanResource(id);
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
	if(record)
		record->Delete(GetResourceManager());
	GetResourceManager()->UnregisterResource(res);

	if(m_ImageInfo.find(id) != m_ImageInfo.end())
		m_ImageInfo.erase(id);
}

#define DESTROY_IMPL(type, func) \
	VkResult WrappedVulkan::func(VkDevice device, type obj) \
	{ \
		DestroyObject(MakeRes(obj), GetResourceManager()->GetID(MakeRes(obj))); \
		return m_Real.func(device, obj); \
	}

DESTROY_IMPL(VkBuffer, vkDestroyBuffer)
DESTROY_IMPL(VkBufferView, vkDestroyBufferView)
DESTROY_IMPL(VkImage, vkDestroyImage)
DESTROY_IMPL(VkImageView, vkDestroyImageView)
DESTROY_IMPL(VkAttachmentView, vkDestroyAttachmentView)
DESTROY_IMPL(VkShader, vkDestroyShader)
DESTROY_IMPL(VkShaderModule, vkDestroyShaderModule)
DESTROY_IMPL(VkPipeline, vkDestroyPipeline)
DESTROY_IMPL(VkPipelineCache, vkDestroyPipelineCache)
DESTROY_IMPL(VkPipelineLayout, vkDestroyPipelineLayout)
DESTROY_IMPL(VkSampler, vkDestroySampler)
DESTROY_IMPL(VkDescriptorSetLayout, vkDestroyDescriptorSetLayout)
DESTROY_IMPL(VkDescriptorPool, vkDestroyDescriptorPool)
DESTROY_IMPL(VkDynamicViewportState, vkDestroyDynamicViewportState)
DESTROY_IMPL(VkDynamicRasterState, vkDestroyDynamicRasterState)
DESTROY_IMPL(VkDynamicColorBlendState, vkDestroyDynamicColorBlendState)
DESTROY_IMPL(VkDynamicDepthStencilState, vkDestroyDynamicDepthStencilState)
DESTROY_IMPL(VkSemaphore, vkDestroySemaphore)
DESTROY_IMPL(VkCmdPool, vkDestroyCommandPool)
DESTROY_IMPL(VkCmdBuffer, vkDestroyCommandBuffer)
DESTROY_IMPL(VkFramebuffer, vkDestroyFramebuffer)
DESTROY_IMPL(VkRenderPass, vkDestroyRenderPass)
DESTROY_IMPL(VkSwapChainWSI, vkDestroySwapChainWSI)

#undef DESTROY_IMPL

bool WrappedVulkan::Serialise_vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(uint32_t, nodeIdx, queueNodeIndex);
	SERIALISE_ELEMENT(uint32_t, idx, queueIndex);
	SERIALISE_ELEMENT(ResourceId, queueId, GetResourceManager()->GetID(MakeRes(*pQueue)));

	if(m_State == READING)
	{
		VkQueue queue;
		VkResult ret = m_Real.vkGetDeviceQueue((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, nodeIdx, idx, &queue);

		VkResource res = MakeRes(queue);

		GetResourceManager()->RegisterResource(res);
		GetResourceManager()->AddLiveResource(queueId, res);
	}

	return true;
}

VkResult WrappedVulkan::vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	VkResult ret = m_Real.vkGetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pQueue);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(GET_DEVICE_QUEUE);
				Serialise_vkGetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    cmdBufferCount,
    const VkCmdBuffer*                          pCmdBuffers,
    VkFence                                     fence)
{
	SERIALISE_ELEMENT(ResourceId, queueId, GetResourceManager()->GetID(MakeRes(queue)));
	SERIALISE_ELEMENT(ResourceId, fenceId, fence != VK_NULL_HANDLE ? GetResourceManager()->GetID(MakeRes(fence)) : ResourceId());
	
	SERIALISE_ELEMENT(uint32_t, numCmds, cmdBufferCount);

	vector<ResourceId> cmdIds;
	VkCmdBuffer *cmds = m_State >= WRITING ? NULL : new VkCmdBuffer[numCmds];
	for(uint32_t i=0; i < numCmds; i++)
	{
		ResourceId bakedId;

		if(m_State >= WRITING)
		{
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(pCmdBuffers[i]));
			RDCASSERT(record->bakedCommands);
			if(record->bakedCommands)
				bakedId = record->bakedCommands->GetResourceID();
		}

		SERIALISE_ELEMENT(ResourceId, id, bakedId);

		if(m_State < WRITING)
		{
			cmdIds.push_back(id);

			cmds[i] = id != ResourceId()
			          ? (VkCmdBuffer)GetResourceManager()->GetLiveResource(id).handle
			          : NULL;
		}
	}
	
	if(m_State < WRITING)
	{
		queue = (VkQueue)GetResourceManager()->GetLiveResource(queueId).handle;
		if(fenceId != ResourceId())
			fence = (VkFence)GetResourceManager()->GetLiveResource(fenceId).handle;
		else
			fence = VK_NULL_HANDLE;
	}

	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		m_SubmittedFences.insert(fenceId);

		m_Real.vkQueueSubmit(queue, numCmds, cmds, fence);

		for(uint32_t i=0; i < numCmds; i++)
		{
			ResourceId cmd = GetResourceManager()->GetID(MakeRes(cmds[i]));
			GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
		}

		AddEvent(QUEUE_SUBMIT, desc);
		string name = "vkQueueSubmit(" +
						ToStr::Get(numCmds) + ")";

		FetchDrawcall draw;
		draw.name = name;

		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, true);

		// add command buffer draws under here
		m_DrawcallStack.push_back(&m_DrawcallStack.back()->children.back());

		m_CurEventID++;

		for(uint32_t c=0; c < numCmds; c++)
		{
			AddEvent(QUEUE_SUBMIT, "");
			string name = "[" + ToStr::Get(cmdIds[c]) + "]";

			FetchDrawcall draw;
			draw.name = name;

			draw.flags |= eDraw_PushMarker;

			AddDrawcall(draw, true);

			DrawcallTreeNode &d = m_DrawcallStack.back()->children.back();

			// copy DrawcallTreeNode children
			d.children = m_CmdBufferInfo[cmdIds[c]].draw->children;

			// assign new event and drawIDs
			RefreshIDs(d.children, m_CurEventID, m_CurDrawcallID);

			m_PartialReplayData.cmdBufferSubmits[cmdIds[c]].push_back(m_CurEventID);

			// 1 extra for the [0] virtual event for the command buffer
			m_CurEventID += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			m_CurDrawcallID += m_CmdBufferInfo[cmdIds[c]].drawCount;
		}

		// the outer loop will increment the event ID but we've handled
		// it ourselves, so 'undo' that.
		m_CurEventID--;

		// done adding command buffers
		m_DrawcallStack.pop_back();
	}
	else if(m_State == EXECUTING)
	{
		m_CurEventID++;

		uint32_t startEID = m_CurEventID;

		// advance m_CurEventID to match the events added when reading
		for(uint32_t c=0; c < numCmds; c++)
		{
			// 1 extra for the [0] virtual event for the command buffer
			m_CurEventID += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			m_CurDrawcallID += m_CmdBufferInfo[cmdIds[c]].drawCount;
		}

		m_CurEventID--;

		if(m_LastEventID < m_CurEventID)
		{
			RDCDEBUG("Queue Submit partial replay %u < %u", m_LastEventID, m_CurEventID);

			uint32_t eid = startEID;

			vector<ResourceId> trimmedCmdIds;
			vector<VkCmdBuffer> trimmedCmds;

			for(uint32_t c=0; c < numCmds; c++)
			{
				uint32_t end = eid + m_CmdBufferInfo[cmdIds[c]].eventCount;

				if(eid == m_PartialReplayData.baseEvent)
				{
					ResourceId partial = GetResourceManager()->GetID(MakeRes(PartialCmdBuf()));
					RDCDEBUG("Queue Submit partial replay of %llu at %u, using %llu", cmdIds[c], eid, partial);
					trimmedCmdIds.push_back(partial);
					trimmedCmds.push_back(PartialCmdBuf());
				}
				else if(m_LastEventID >= end)
				{
					RDCDEBUG("Queue Submit full replay %llu", cmdIds[c]);
					trimmedCmdIds.push_back(cmdIds[c]);
					trimmedCmds.push_back((VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdIds[c]).handle);
				}
				else
				{
					RDCDEBUG("Queue not submitting %llu", cmdIds[c]);
				}

				eid += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			}

			RDCASSERT(trimmedCmds.size() > 0);

			m_SubmittedFences.insert(fenceId);

			m_Real.vkQueueSubmit(queue, (uint32_t)trimmedCmds.size(), &trimmedCmds[0], fence);

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = trimmedCmdIds[i];
				GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
			}
		}
		else
		{
			m_SubmittedFences.insert(fenceId);

			m_Real.vkQueueSubmit(queue, numCmds, cmds, fence);

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = GetResourceManager()->GetID(MakeRes(cmds[i]));
				GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
			}
		}
	}

	SAFE_DELETE_ARRAY(cmds);

	return true;
}

void WrappedVulkan::RefreshIDs(vector<DrawcallTreeNode> &nodes, uint32_t baseEventID, uint32_t baseDrawID)
{
	// assign new drawcall IDs
	for(size_t i=0; i < nodes.size(); i++)
	{
		nodes[i].draw.eventID += baseEventID;
		nodes[i].draw.drawcallID += baseDrawID;

		for(int32_t e=0; e < nodes[i].draw.events.count; e++)
		{
			nodes[i].draw.events[e].eventID += baseEventID;
			m_Events.push_back(nodes[i].draw.events[e]);
		}

		RefreshIDs(nodes[i].children, baseEventID, baseDrawID);
	}
}

VkResult WrappedVulkan::vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    cmdBufferCount,
    const VkCmdBuffer*                          pCmdBuffers,
    VkFence                                     fence)
{
	VkResult ret = m_Real.vkQueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_SUBMIT);
		Serialise_vkQueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);

		m_ContextRecord->AddChunk(scope.Get());
	}

	for(uint32_t i=0; i < cmdBufferCount; i++)
	{
		ResourceId cmd = GetResourceManager()->GetID(MakeRes(pCmdBuffers[i]));
		GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);

		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmd);
		for(auto it = record->bakedCommands->dirtied.begin(); it != record->bakedCommands->dirtied.end(); ++it)
			GetResourceManager()->MarkDirtyResource(*it);

		if(m_State == WRITING_CAPFRAME)
		{
			// pull in frame refs from this baked command buffer
			record->bakedCommands->AddResourceReferences(GetResourceManager());

			// ref the parent command buffer by itself, this will pull in the cmd buffer pool
			GetResourceManager()->MarkResourceFrameReferenced(record->GetResourceID(), eFrameRef_Read);

			m_CmdBufferRecords.push_back(record->bakedCommands);
			record->bakedCommands->AddRef();
		}

		record->dirtied.clear();
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResourceManager()->GetID(MakeRes(queue)));
	SERIALISE_ELEMENT(ResourceId, sid, GetResourceManager()->GetID(MakeRes(semaphore)));
	
	if(m_State < WRITING)
	{
		m_Real.vkQueueSignalSemaphore((VkQueue)GetResourceManager()->GetLiveResource(qid).handle,
				(VkSemaphore)GetResourceManager()->GetLiveResource(sid).handle);
	}

	return true;
}

VkResult WrappedVulkan::vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = m_Real.vkQueueSignalSemaphore(queue, semaphore);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_SIGNAL_SEMAPHORE);
		Serialise_vkQueueSignalSemaphore(queue, semaphore);

		m_ContextRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(MakeRes(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(MakeRes(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResourceManager()->GetID(MakeRes(queue)));
	SERIALISE_ELEMENT(ResourceId, sid, GetResourceManager()->GetID(MakeRes(semaphore)));
	
	if(m_State < WRITING)
	{
		m_Real.vkQueueWaitSemaphore((VkQueue)GetResourceManager()->GetLiveResource(qid).handle,
				(VkSemaphore)GetResourceManager()->GetLiveResource(sid).handle);
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = m_Real.vkQueueWaitSemaphore(queue, semaphore);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_SEMAPHORE);
		Serialise_vkQueueWaitSemaphore(queue, semaphore);

		m_ContextRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(MakeRes(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(MakeRes(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitIdle(VkQueue queue)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(queue)));
	
	if(m_State < WRITING_CAPFRAME)
	{
		m_Real.vkQueueWaitIdle((VkQueue)GetResourceManager()->GetLiveResource(id).handle);
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitIdle(VkQueue queue)
{
	VkResult ret = m_Real.vkQueueWaitIdle(queue);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_IDLE);
		Serialise_vkQueueWaitIdle(queue);

		m_ContextRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(MakeRes(queue), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkDeviceWaitIdle(VkDevice device)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(device)));
	
	if(m_State < WRITING)
	{
		m_Real.vkDeviceWaitIdle((VkDevice)GetResourceManager()->GetLiveResource(id).handle);
	}

	return true;
}

VkResult WrappedVulkan::vkDeviceWaitIdle(VkDevice device)
{
	VkResult ret = m_Real.vkDeviceWaitIdle(device);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DEVICE_WAIT_IDLE);
		Serialise_vkDeviceWaitIdle(device);

		m_ContextRecord->AddChunk(scope.Get());
	}

	return ret;
}

// Memory functions

bool WrappedVulkan::Serialise_vkAllocMemory(
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkMemoryAllocInfo, info, *pAllocInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pMem)));

	if(m_State == READING)
	{
		VkDeviceMemory mem = VK_NULL_HANDLE;

		// VKTODOLOW may need to re-write info to change memory type index to the
		// appropriate index on replay
		VkResult ret = m_Real.vkAllocMemory((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &mem);
		
		ResourceId live;

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			live = GetResourceManager()->RegisterResource(MakeRes(mem));
			GetResourceManager()->AddLiveResource(id, MakeRes(mem));

			m_MemoryInfo[live].size = info.allocationSize;
		}
	}

	return true;
}

VkResult WrappedVulkan::vkAllocMemory(
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem)
{
	VkResult ret = m_Real.vkAllocMemory(device, pAllocInfo, pMem);
	
	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pMem);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(ALLOC_MEM);
				Serialise_vkAllocMemory(device, pAllocInfo, pMem);

				chunk = scope.Get();
			}
			
			// create resource record for gpu memory
			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}

		m_MemoryInfo[id].size = pAllocInfo->allocationSize;
	}

	return ret;
}

VkResult WrappedVulkan::vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	// VKTODOMED I don't think I need to serialise this.
	// the resource record just stays around until there are
	// no references (which should be the same since lifetime
	// tracking is app responsibility)
	// we just need to clean up after ourselves on replay
	ResourceId id = GetResourceManager()->GetID(MakeRes(mem));
	m_MemoryInfo.erase(id);
	GetResourceManager()->MarkCleanResource(id);
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
	if(record)
		record->Delete(GetResourceManager());
	GetResourceManager()->UnregisterResource(MakeRes(mem));

	return m_Real.vkFreeMemory(device, mem);
}

VkResult WrappedVulkan::vkMapMemory(
			VkDevice                                    device,
			VkDeviceMemory                              mem,
			VkDeviceSize                                offset,
			VkDeviceSize                                size,
			VkMemoryMapFlags                            flags,
			void**                                      ppData)
{
	VkResult ret = m_Real.vkMapMemory(device, mem, offset, size, flags, ppData);

	if(ret == VK_SUCCESS && ppData)
	{
		ResourceId id = GetResourceManager()->GetID(MakeRes(mem));

		if(m_State >= WRITING)
		{
			auto it = m_MemoryInfo.find(id);
			if(it == m_MemoryInfo.end())
			{
				RDCERR("vkMapMemory for unknown memory handle");
			}
			else
			{
				it->second.mappedPtr = *ppData;
				it->second.mapOffset = offset;
				it->second.mapSize = size == 0 ? it->second.size : size;
				it->second.mapFlags = flags;
			}
		}
		else if(m_State >= WRITING)
		{
			GetResourceManager()->MarkDirtyResource(id);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(mem)));

	auto it = m_MemoryInfo.find(id);

	SERIALISE_ELEMENT(VkMemoryMapFlags, flags, it->second.mapFlags);
	SERIALISE_ELEMENT(uint64_t, memOffset, it->second.mapOffset);
	SERIALISE_ELEMENT(uint64_t, memSize, it->second.mapSize);

	// VKTODOHIGH: this is really horrible - this could be write-combined memory that we're
	// reading from to get the latest data. This saves on having to fetch the data some
	// other way and provide an interception buffer to the app, but is awful.
	// we're also not doing any diff range checks, just serialising the whole memory region.
	// In vulkan the common case will be one memory region for a large number of distinct
	// bits of data so most maps will not change the whole region.
	SERIALISE_ELEMENT_BUF(byte*, data, (byte *)it->second.mappedPtr + it->second.mapOffset, (size_t)memSize);

	if(m_State < WRITING)
	{
		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		mem = (VkDeviceMemory)GetResourceManager()->GetLiveResource(id).handle;

		void *mapPtr = NULL;
		VkResult ret = m_Real.vkMapMemory(device, mem, memOffset, memSize, flags, &mapPtr);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Error mapping memory on replay: 0x%08x", ret);
		}
		else
		{
			memcpy((byte *)mapPtr+memOffset, data, (size_t)memSize);

			ret = m_Real.vkUnmapMemory(device, mem);
			
			if(ret != VK_SUCCESS)
				RDCERR("Error unmapping memory on replay: 0x%08x", ret);
		}

		SAFE_DELETE_ARRAY(data);
	}

	return true;
}

VkResult WrappedVulkan::vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	VkResult ret = m_Real.vkUnmapMemory(device, mem);
	
	if(m_State >= WRITING)
	{
		ResourceId id = GetResourceManager()->GetID(MakeRes(mem));

		if(m_State >= WRITING)
		{
			auto it = m_MemoryInfo.find(id);
			if(it == m_MemoryInfo.end())
			{
				RDCERR("vkMapMemory for unknown memory handle");
			}
			else
			{
				if(ret == VK_SUCCESS && m_State >= WRITING_CAPFRAME)
				{
					SCOPED_SERIALISE_CONTEXT(UNMAP_MEM);
					Serialise_vkUnmapMemory(device, mem);

					VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(mem));

					if(m_State == WRITING_IDLE)
					{
						record->AddChunk(scope.Get());
					}
					else
					{
						m_ContextRecord->AddChunk(scope.Get());
						GetResourceManager()->MarkResourceFrameReferenced(MakeRes(mem), eFrameRef_Write);
					}
				}
				else
				{
					GetResourceManager()->MarkDirtyResource(MakeRes(mem));
				}

				it->second.mappedPtr = NULL;
			}
		}
	}

	return ret;
}

// Generic API object functions

bool WrappedVulkan::Serialise_vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, bufId, GetResourceManager()->GetID(MakeRes(buffer)));
	SERIALISE_ELEMENT(ResourceId, memId, GetResourceManager()->GetID(MakeRes(mem)));
	SERIALISE_ELEMENT(uint64_t, offs, memOffset);

	if(m_State < WRITING)
	{
		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufId).handle;
		mem = (VkDeviceMemory)GetResourceManager()->GetLiveResource(memId).handle;

		m_Real.vkBindBufferMemory(device, buffer, mem, offs);
	}

	return true;
}

VkResult WrappedVulkan::vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(buffer));

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_MEM);
			Serialise_vkBindBufferMemory(device, buffer, mem, memOffset);

			chunk = scope.Get();
		}

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);

			GetResourceManager()->MarkResourceFrameReferenced(MakeRes(buffer), eFrameRef_Write);
			GetResourceManager()->MarkResourceFrameReferenced(MakeRes(mem), eFrameRef_Read);
		}
		else
		{
			record->AddChunk(chunk);
		}

		record->SetMemoryRecord(GetResourceManager()->GetResourceRecord(MakeRes(mem)));
	}

	return m_Real.vkBindBufferMemory(device, buffer, mem, memOffset);
}

bool WrappedVulkan::Serialise_vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, imgId, GetResourceManager()->GetID(MakeRes(image)));
	SERIALISE_ELEMENT(ResourceId, memId, GetResourceManager()->GetID(MakeRes(mem)));
	SERIALISE_ELEMENT(uint64_t, offs, memOffset);

	if(m_State < WRITING)
	{
		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		image = (VkImage)GetResourceManager()->GetLiveResource(imgId).handle;
		mem = (VkDeviceMemory)GetResourceManager()->GetLiveResource(memId).handle;

		m_Real.vkBindImageMemory(device, image, mem, offs);
	}

	return true;
}

VkResult WrappedVulkan::vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(image));

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_IMAGE_MEM);
			Serialise_vkBindImageMemory(device, image, mem, memOffset);

			chunk = scope.Get();
		}

		if(m_State == WRITING_CAPFRAME)
		{
			m_ContextRecord->AddChunk(chunk);

			GetResourceManager()->MarkResourceFrameReferenced(MakeRes(image), eFrameRef_Write);
			GetResourceManager()->MarkResourceFrameReferenced(MakeRes(mem), eFrameRef_Read);
		}
		else
		{
			record->AddChunk(chunk);
		}

		record->SetMemoryRecord(GetResourceManager()->GetResourceRecord(MakeRes(mem)));
	}

	return m_Real.vkBindImageMemory(device, image, mem, memOffset);
}

bool WrappedVulkan::Serialise_vkCreateBuffer(
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkBufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pBuffer)));

	if(m_State == READING)
	{
		VkBuffer buf = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateBuffer((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &buf);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(buf));
			GetResourceManager()->AddLiveResource(id, MakeRes(buf));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateBuffer(
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer)
{
	VkResult ret = m_Real.vkCreateBuffer(device, pCreateInfo, pBuffer);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pBuffer);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER);
				Serialise_vkCreateBuffer(device, pCreateInfo, pBuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateBufferView(
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkBufferViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pView)));

	if(m_State == READING)
	{
		VkBufferView view = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateBufferView((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(view));
			GetResourceManager()->AddLiveResource(id, MakeRes(view));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateBufferView(
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView)
{
	VkResult ret = m_Real.vkCreateBufferView(device, pCreateInfo, pView);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pView);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER_VIEW);
				Serialise_vkCreateBufferView(device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
			record->AddParent(GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->buffer)));
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateImage(
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkImageCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pImage)));

	if(m_State == READING)
	{
		VkImage img = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateImage((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &img);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(img));
			GetResourceManager()->AddLiveResource(id, MakeRes(img));
			
			m_ImageInfo[live].type = info.imageType;
			m_ImageInfo[live].format = info.format;
			m_ImageInfo[live].extent = info.extent;
			m_ImageInfo[live].mipLevels = info.mipLevels;
			m_ImageInfo[live].arraySize = info.arraySize;
			
			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArraySlice = 0;
			range.mipLevels = info.mipLevels;
			range.arraySize = info.arraySize;
			if(info.imageType == VK_IMAGE_TYPE_3D)
				range.arraySize = info.extent.depth;

			m_ImageInfo[live].subresourceStates.clear();
			
			if(!IsDepthStencilFormat(info.format))
			{
				range.aspect = VK_IMAGE_ASPECT_COLOR;  m_ImageInfo[live].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			}
			else
			{
				range.aspect = VK_IMAGE_ASPECT_DEPTH;  m_ImageInfo[live].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
				range.aspect = VK_IMAGE_ASPECT_STENCIL;m_ImageInfo[live].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			}
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateImage(
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage)
{
	VkResult ret = m_Real.vkCreateImage(device, pCreateInfo, pImage);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pImage);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_IMAGE);
				Serialise_vkCreateImage(device, pCreateInfo, pImage);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
		
		m_ImageInfo[id].type = pCreateInfo->imageType;
		m_ImageInfo[id].format = pCreateInfo->format;
		m_ImageInfo[id].extent = pCreateInfo->extent;
		m_ImageInfo[id].mipLevels = pCreateInfo->mipLevels;
		m_ImageInfo[id].arraySize = pCreateInfo->arraySize;

		VkImageSubresourceRange range;
		range.baseMipLevel = range.baseArraySlice = 0;
		range.mipLevels = pCreateInfo->mipLevels;
		range.arraySize = pCreateInfo->arraySize;
		if(pCreateInfo->imageType == VK_IMAGE_TYPE_3D)
			range.arraySize = pCreateInfo->extent.depth;

		m_ImageInfo[id].subresourceStates.clear();

		if(!IsDepthStencilFormat(pCreateInfo->format))
		{
			range.aspect = VK_IMAGE_ASPECT_COLOR;  m_ImageInfo[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
		}
		else
		{
			range.aspect = VK_IMAGE_ASPECT_DEPTH;  m_ImageInfo[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
			range.aspect = VK_IMAGE_ASPECT_STENCIL;m_ImageInfo[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
		}
	}

	return ret;
}

// Image view functions

bool WrappedVulkan::Serialise_vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    VkImageView*                                pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkImageViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pView)));

	if(m_State == READING)
	{
		VkImageView view = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateImageView((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(view));
			GetResourceManager()->AddLiveResource(id, MakeRes(view));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    VkImageView*                                pView)
{
	VkResult ret = m_Real.vkCreateImageView(device, pCreateInfo, pView);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pView);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_IMAGE_VIEW);
				Serialise_vkCreateImageView(device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
			record->AddParent(GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->image)));
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateAttachmentView(
    VkDevice                                    device,
    const VkAttachmentViewCreateInfo*           pCreateInfo,
    VkAttachmentView*                           pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkAttachmentViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pView)));

	if(m_State == READING)
	{
		VkAttachmentView view = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateAttachmentView((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(view));
			GetResourceManager()->AddLiveResource(id, MakeRes(view));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateAttachmentView(
    VkDevice                                    device,
    const VkAttachmentViewCreateInfo*           pCreateInfo,
    VkAttachmentView*                           pView)
{
	VkResult ret = m_Real.vkCreateAttachmentView(device, pCreateInfo, pView);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pView);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_ATTACHMENT_VIEW);
				Serialise_vkCreateAttachmentView(device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
			record->AddParent(GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->image)));
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

// Shader functions

bool WrappedVulkan::Serialise_vkCreateShaderModule(
		VkDevice                                    device,
		const VkShaderModuleCreateInfo*             pCreateInfo,
		VkShaderModule*                             pShaderModule)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkShaderModuleCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pShaderModule)));

	if(m_State == READING)
	{
		VkShaderModule sh = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateShaderModule((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &sh);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(sh));
			GetResourceManager()->AddLiveResource(id, MakeRes(sh));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateShaderModule(
		VkDevice                                    device,
		const VkShaderModuleCreateInfo*             pCreateInfo,
		VkShaderModule*                             pShaderModule)
{
	VkResult ret = m_Real.vkCreateShaderModule(device, pCreateInfo, pShaderModule);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pShaderModule);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SHADER_MODULE);
				Serialise_vkCreateShaderModule(device, pCreateInfo, pShaderModule);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateShader(
    VkDevice                                    device,
    const VkShaderCreateInfo*                   pCreateInfo,
    VkShader*                                   pShader)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkShaderCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pShader)));

	if(m_State == READING)
	{
		VkShader sh = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateShader((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &sh);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(sh));
			GetResourceManager()->AddLiveResource(id, MakeRes(sh));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateShader(
    VkDevice                                    device,
    const VkShaderCreateInfo*                   pCreateInfo,
    VkShader*                                   pShader)
{
	VkResult ret = m_Real.vkCreateShader(device, pCreateInfo, pShader);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pShader);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SHADER);
				Serialise_vkCreateShader(device, pCreateInfo, pShader);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);

			VkResourceRecord *modulerecord = GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->module));
			record->AddParent(modulerecord);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

// Pipeline functions

bool WrappedVulkan::Serialise_vkCreatePipelineCache(
		VkDevice                                    device,
		const VkPipelineCacheCreateInfo*            pCreateInfo,
		VkPipelineCache*                            pPipelineCache)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkPipelineCacheCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pPipelineCache)));

	if(m_State == READING)
	{
		VkPipelineCache cache = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreatePipelineCache((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &cache);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(cache));
			GetResourceManager()->AddLiveResource(id, MakeRes(cache));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreatePipelineCache(
		VkDevice                                    device,
		const VkPipelineCacheCreateInfo*            pCreateInfo,
		VkPipelineCache*                            pPipelineCache)
{
	VkResult ret = m_Real.vkCreatePipelineCache(device, pCreateInfo, pPipelineCache);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pPipelineCache);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_PIPE_CACHE);
				Serialise_vkCreatePipelineCache(device, pCreateInfo, pPipelineCache);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateGraphicsPipelines(
		VkDevice                                    device,
		VkPipelineCache                             pipelineCache,
		uint32_t                                    count,
		const VkGraphicsPipelineCreateInfo*         pCreateInfos,
		VkPipeline*                                 pPipelines)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, cacheId, GetResourceManager()->GetID(MakeRes(pipelineCache)));
	SERIALISE_ELEMENT(VkGraphicsPipelineCreateInfo, info, *pCreateInfos);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pPipelines)));

	if(m_State == READING)
	{
		VkPipeline pipe = VK_NULL_HANDLE;
		
		// use original ID
		m_CreationInfo.m_Pipeline[id].Init(GetResourceManager(), &info);

		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		pipelineCache = (VkPipelineCache)GetResourceManager()->GetLiveResource(cacheId).handle;

		VkResult ret = m_Real.vkCreateGraphicsPipelines(device, pipelineCache, 1, &info, &pipe);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(pipe));
			GetResourceManager()->AddLiveResource(id, MakeRes(pipe));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateGraphicsPipelines(
			VkDevice                                    device,
			VkPipelineCache                             pipelineCache,
			uint32_t                                    count,
			const VkGraphicsPipelineCreateInfo*         pCreateInfos,
			VkPipeline*                                 pPipelines)
{
	VkResult ret = m_Real.vkCreateGraphicsPipelines(device, pipelineCache, count, pCreateInfos, pPipelines);

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			VkResource res = MakeRes(pPipelines[i]);
			ResourceId id = GetResourceManager()->RegisterResource(res);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(CREATE_GRAPHICS_PIPE);
					Serialise_vkCreateGraphicsPipelines(device, pipelineCache, 1, &pCreateInfos[i], &pPipelines[i]);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
				record->AddChunk(chunk);

				VkResourceRecord *cacherecord = GetResourceManager()->GetResourceRecord(MakeRes(pipelineCache));
				record->AddParent(cacherecord);

				VkResourceRecord *layoutrecord = GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfos->layout));
				record->AddParent(layoutrecord);

				for(uint32_t i=0; i < pCreateInfos->stageCount; i++)
				{
					VkResourceRecord *shaderrecord = GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfos->pStages[i].shader));
					record->AddParent(shaderrecord);
				}
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, res);
			}
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDescriptorPool(
			VkDevice                                    device,
			VkDescriptorPoolUsage                       poolUsage,
			uint32_t                                    maxSets,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkDescriptorPoolUsage, pooluse, poolUsage);
	SERIALISE_ELEMENT(uint32_t, maxs, maxSets);
	SERIALISE_ELEMENT(VkDescriptorPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pDescriptorPool)));

	if(m_State == READING)
	{
		VkDescriptorPool pool = VK_NULL_HANDLE;

		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;

		VkResult ret = m_Real.vkCreateDescriptorPool(device, pooluse, maxs, &info, &pool);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(pool));
			GetResourceManager()->AddLiveResource(id, MakeRes(pool));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDescriptorPool(
			VkDevice                                    device,
			VkDescriptorPoolUsage                       poolUsage,
			uint32_t                                    maxSets,
			const VkDescriptorPoolCreateInfo*           pCreateInfo,
			VkDescriptorPool*                           pDescriptorPool)
{
	VkResult ret = m_Real.vkCreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pDescriptorPool);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_POOL);
				Serialise_vkCreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDescriptorSetLayout(
		VkDevice                                    device,
		const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
		VkDescriptorSetLayout*                      pSetLayout)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkDescriptorSetLayoutCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pSetLayout)));

	// this creation info is needed at capture time (for creating/updating descriptor set bindings)
	// uses original ID in replay
	m_CreationInfo.m_DescSetLayout[id].Init(GetResourceManager(), &info);

	if(m_State == READING)
	{
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;

		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;

		VkResult ret = m_Real.vkCreateDescriptorSetLayout(device, &info, &layout);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(layout));
			GetResourceManager()->AddLiveResource(id, MakeRes(layout));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDescriptorSetLayout(
		VkDevice                                    device,
		const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
		VkDescriptorSetLayout*                      pSetLayout)
{
	VkResult ret = m_Real.vkCreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pSetLayout);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_SET_LAYOUT);
				Serialise_vkCreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreatePipelineLayout(
		VkDevice                                    device,
		const VkPipelineLayoutCreateInfo*           pCreateInfo,
		VkPipelineLayout*                           pPipelineLayout)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkPipelineLayoutCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pPipelineLayout)));

	if(m_State == READING)
	{
		VkPipelineLayout layout = VK_NULL_HANDLE;

		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;

		VkResult ret = m_Real.vkCreatePipelineLayout(device, &info, &layout);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(layout));
			GetResourceManager()->AddLiveResource(id, MakeRes(layout));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreatePipelineLayout(
		VkDevice                                    device,
		const VkPipelineLayoutCreateInfo*           pCreateInfo,
		VkPipelineLayout*                           pPipelineLayout)
{
	VkResult ret = m_Real.vkCreatePipelineLayout(device, pCreateInfo, pPipelineLayout);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pPipelineLayout);
		ResourceId id = GetResourceManager()->RegisterResource(res);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_PIPE_LAYOUT);
				Serialise_vkCreatePipelineLayout(device, pCreateInfo, pPipelineLayout);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

// Sampler functions

bool WrappedVulkan::Serialise_vkCreateSampler(
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkSamplerCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pSampler)));

	if(m_State == READING)
	{
		VkSampler samp = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateSampler((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &samp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(samp));
			GetResourceManager()->AddLiveResource(id, MakeRes(samp));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSampler(
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler)
{
	VkResult ret = m_Real.vkCreateSampler(device, pCreateInfo, pSampler);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pSampler);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLER);
				Serialise_vkCreateSampler(device, pCreateInfo, pSampler);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkSemaphoreCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pSemaphore)));

	if(m_State == READING)
	{
		VkSemaphore sem = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateSemaphore((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &sem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(sem));
			GetResourceManager()->AddLiveResource(id, MakeRes(sem));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore)
{
	VkResult ret = m_Real.vkCreateSemaphore(device, pCreateInfo, pSemaphore);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pSemaphore);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SEMAPHORE);
				Serialise_vkCreateSemaphore(device, pCreateInfo, pSemaphore);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkFramebufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pFramebuffer)));

	if(m_State == READING)
	{
		VkFramebuffer fb = VK_NULL_HANDLE;
		
		// use original ID
		m_CreationInfo.m_Framebuffer[id].Init(GetResourceManager(), &info);

		VkResult ret = m_Real.vkCreateFramebuffer((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &fb);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(fb));
			GetResourceManager()->AddLiveResource(id, MakeRes(fb));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	VkResult ret = m_Real.vkCreateFramebuffer(device, pCreateInfo, pFramebuffer);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pFramebuffer);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_FRAMEBUFFER);
				Serialise_vkCreateFramebuffer(device, pCreateInfo, pFramebuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);

			for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
			{
				record->AddParent(GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->pAttachments[i].view)));
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkRenderPassCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pRenderPass)));

	if(m_State == READING)
	{
		VkRenderPass rp = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateRenderPass((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &rp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(rp));
			GetResourceManager()->AddLiveResource(id, MakeRes(rp));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass)
{
	VkResult ret = m_Real.vkCreateRenderPass(device, pCreateInfo, pRenderPass);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pRenderPass);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_RENDERPASS);
				Serialise_vkCreateRenderPass(device, pCreateInfo, pRenderPass);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

// State object functions

bool WrappedVulkan::Serialise_vkCreateDynamicViewportState(
			VkDevice                                         device,
			const VkDynamicViewportStateCreateInfo*          pCreateInfo,
			VkDynamicViewportState*                          pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkDynamicViewportStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pState)));

	if(m_State == READING)
	{
		VkDynamicViewportState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_VPScissor[id].Init(GetResourceManager(), &info);

		VkResult ret = m_Real.vkCreateDynamicViewportState((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(state));
			GetResourceManager()->AddLiveResource(id, MakeRes(state));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicViewportState(
			VkDevice                                    device,
			const VkDynamicViewportStateCreateInfo*           pCreateInfo,
			VkDynamicViewportState*                           pState)
{
	VkResult ret = m_Real.vkCreateDynamicViewportState(device, pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pState);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_VIEWPORT_STATE);
				Serialise_vkCreateDynamicViewportState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicRasterState(
			VkDevice                                        device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkDynamicRasterStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pState)));

	if(m_State == READING)
	{
		VkDynamicRasterState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_Raster[id].Init(GetResourceManager(), &info);

		VkResult ret = m_Real.vkCreateDynamicRasterState((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(state));
			GetResourceManager()->AddLiveResource(id, MakeRes(state));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicRasterState(
			VkDevice                                        device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState)
{
	VkResult ret = m_Real.vkCreateDynamicRasterState(device, pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pState);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE);
				Serialise_vkCreateDynamicRasterState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicColorBlendState(
			VkDevice                                            device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkDynamicColorBlendStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pState)));

	if(m_State == READING)
	{
		VkDynamicColorBlendState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_Blend[id].Init(GetResourceManager(), &info);

		VkResult ret = m_Real.vkCreateDynamicColorBlendState((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(state));
			GetResourceManager()->AddLiveResource(id, MakeRes(state));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicColorBlendState(
			VkDevice                                    device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState)
{
	VkResult ret = m_Real.vkCreateDynamicColorBlendState(device, pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pState);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_BLEND_STATE);
				Serialise_vkCreateDynamicColorBlendState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicDepthStencilState(
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkDynamicDepthStencilStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pState)));

	if(m_State == READING)
	{
		VkDynamicDepthStencilState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_DepthStencil[id].Init(GetResourceManager(), &info);

		VkResult ret = m_Real.vkCreateDynamicDepthStencilState((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(state));
			GetResourceManager()->AddLiveResource(id, MakeRes(state));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicDepthStencilState(
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState)
{
	VkResult ret = m_Real.vkCreateDynamicDepthStencilState(device, pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pState);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DEPTH_STATE);
				Serialise_vkCreateDynamicDepthStencilState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

// Command pool functions

bool WrappedVulkan::Serialise_vkCreateCommandPool(
			VkDevice                                    device,
			const VkCmdPoolCreateInfo*                  pCreateInfo,
			VkCmdPool*                                  pCmdPool)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkCmdPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pCmdPool)));

	if(m_State == READING)
	{
		VkCmdPool pool = VK_NULL_HANDLE;

		VkResult ret = m_Real.vkCreateCommandPool((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &pool);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(pool));
			GetResourceManager()->AddLiveResource(id, MakeRes(pool));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateCommandPool(
			VkDevice                                    device,
			const VkCmdPoolCreateInfo*                  pCreateInfo,
			VkCmdPool*                                  pCmdPool)
{
	VkResult ret = m_Real.vkCreateCommandPool(device, pCreateInfo, pCmdPool);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pCmdPool);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_CMD_POOL);
				Serialise_vkCreateCommandPool(device, pCreateInfo, pCmdPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkResetCommandPool(
			VkDevice                                    device,
			VkCmdPool                                   cmdPool,
			VkCmdPoolResetFlags                         flags)
{
	// VKTODOMED do I need to serialise this? just a driver hint..
	return m_Real.vkResetCommandPool(device, cmdPool, flags);
}


// Command buffer functions

VkResult WrappedVulkan::vkCreateCommandBuffer(
	VkDevice                        device,
	const VkCmdBufferCreateInfo* pCreateInfo,
	VkCmdBuffer*                   pCmdBuffer)
{
	VkResult ret = m_Real.vkCreateCommandBuffer(device, pCreateInfo, pCmdBuffer);

	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pCmdBuffer);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);

			record->bakedCommands = NULL;

			record->AddParent(GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->cmdPool)));

			// we don't serialise this as we never create this command buffer directly.
			// Instead we create a command buffer for each baked list that we find.
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}

		m_CmdBufferInfo[id].device = device;
		m_CmdBufferInfo[id].createInfo = *pCreateInfo;
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkAllocDescriptorSets(
		VkDevice                                    device,
		VkDescriptorPool                            descriptorPool,
		VkDescriptorSetUsage                        setUsage,
		uint32_t                                    count,
		const VkDescriptorSetLayout*                pSetLayouts,
		VkDescriptorSet*                            pDescriptorSets,
		uint32_t*                                   pCount)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, poolId, GetResourceManager()->GetID(MakeRes(descriptorPool)));
	SERIALISE_ELEMENT(VkDescriptorSetUsage, usage, setUsage);
	SERIALISE_ELEMENT(ResourceId, layoutId, GetResourceManager()->GetID(MakeRes(*pSetLayouts)));
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pDescriptorSets)));

	if(m_State == READING)
	{
		VkDescriptorSet descset = VK_NULL_HANDLE;

		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		descriptorPool = (VkDescriptorPool)GetResourceManager()->GetLiveResource(poolId).handle;
		VkDescriptorSetLayout layout = (VkDescriptorSetLayout)GetResourceManager()->GetLiveResource(layoutId).handle;

		uint32_t cnt = 0;
		VkResult ret = m_Real.vkAllocDescriptorSets(device, descriptorPool, usage, 1, &layout, &descset, &cnt);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->RegisterResource(MakeRes(descset));
			GetResourceManager()->AddLiveResource(id, MakeRes(descset));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkAllocDescriptorSets(
		VkDevice                                    device,
		VkDescriptorPool                            descriptorPool,
		VkDescriptorSetUsage                        setUsage,
		uint32_t                                    count,
		const VkDescriptorSetLayout*                pSetLayouts,
		VkDescriptorSet*                            pDescriptorSets,
		uint32_t*                                   pCount)
{
	VkResult ret = m_Real.vkAllocDescriptorSets(device, descriptorPool, setUsage, count, pSetLayouts, pDescriptorSets, pCount);
	
	RDCASSERT(pCount == NULL || *pCount == count); // VKTODOMED: find out what *pCount < count means

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			VkResource res = MakeRes(pDescriptorSets[i]);
			ResourceId id = GetResourceManager()->RegisterResource(res);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(ALLOC_DESC_SET);
					Serialise_vkAllocDescriptorSets(device, descriptorPool, setUsage, 1, &pSetLayouts[i], &pDescriptorSets[i], NULL);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
				record->AddChunk(chunk);

				ResourceId layoutID = GetResourceManager()->GetID(MakeRes(pSetLayouts[i]));

				record->AddParent(GetResourceManager()->GetResourceRecord(MakeRes(descriptorPool)));
				record->AddParent(GetResourceManager()->GetResourceRecord(layoutID));

				m_CreationInfo.m_DescSetLayout[layoutID].CreateBindingsArray(record->descBindings);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, res);
			}
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkFreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    count,
    const VkDescriptorSet*                      pDescriptorSets)
{
	VkResult ret = m_Real.vkFreeDescriptorSets(device, descriptorPool, count, pDescriptorSets);

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			VkResource res = MakeRes(pDescriptorSets[i]);
			ResourceId id = GetResourceManager()->GetID(res);

			GetResourceManager()->MarkCleanResource(id);
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
			if(record)
				record->Delete(GetResourceManager());
			GetResourceManager()->UnregisterResource(res);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkUpdateDescriptorSets(
		VkDevice                                    device,
		uint32_t                                    writeCount,
		const VkWriteDescriptorSet*                 pDescriptorWrites,
		uint32_t                                    copyCount,
		const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(bool, writes, writeCount == 1);

	VkWriteDescriptorSet writeDesc;
	VkCopyDescriptorSet copyDesc;
	if(writes)
	{
		SERIALISE_ELEMENT(VkWriteDescriptorSet, w, *pDescriptorWrites);
		writeDesc = w;
	}
	else
	{
		SERIALISE_ELEMENT(VkCopyDescriptorSet, c, *pDescriptorCopies);
		copyDesc = c;
	}

	if(m_State < WRITING)
	{
		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;

		if(writes)
			m_Real.vkUpdateDescriptorSets(device, 1, &writeDesc, 0, NULL);
		else
			m_Real.vkUpdateDescriptorSets(device, 0, NULL, 1, &copyDesc);
	}

	return true;
}

VkResult WrappedVulkan::vkUpdateDescriptorSets(
		VkDevice                                    device,
		uint32_t                                    writeCount,
		const VkWriteDescriptorSet*                 pDescriptorWrites,
		uint32_t                                    copyCount,
		const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	VkResult ret = m_Real.vkUpdateDescriptorSets(device, writeCount, pDescriptorWrites, copyCount, pDescriptorCopies);
	
	if(ret == VK_SUCCESS)
	{
		if(m_State == WRITING_CAPFRAME)
		{
			for(uint32_t i=0; i < writeCount; i++)
			{
				{
					SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
					Serialise_vkUpdateDescriptorSets(device, 1, &pDescriptorWrites[i], 0, NULL);

					m_ContextRecord->AddChunk(scope.Get());
				}

				// VKTODOMED marking conservatively as write here - need to correspond with the layout
				// to mark read or write depending on binding type
				for(uint32_t d=0; d < pDescriptorWrites[i].count; d++)
				{
					if(pDescriptorWrites[i].pDescriptors[d].bufferView != VK_NULL_HANDLE)
						GetResourceManager()->MarkResourceFrameReferenced(MakeRes(pDescriptorWrites[i].pDescriptors[d].bufferView), eFrameRef_Write);
					if(pDescriptorWrites[i].pDescriptors[d].sampler != VK_NULL_HANDLE)
						GetResourceManager()->MarkResourceFrameReferenced(MakeRes(pDescriptorWrites[i].pDescriptors[d].sampler), eFrameRef_Read);
					if(pDescriptorWrites[i].pDescriptors[d].imageView != VK_NULL_HANDLE)
						GetResourceManager()->MarkResourceFrameReferenced(MakeRes(pDescriptorWrites[i].pDescriptors[d].imageView), eFrameRef_Write);
					if(pDescriptorWrites[i].pDescriptors[d].attachmentView != VK_NULL_HANDLE)
						GetResourceManager()->MarkResourceFrameReferenced(MakeRes(pDescriptorWrites[i].pDescriptors[d].attachmentView), eFrameRef_Write);
				}
			}

			for(uint32_t i=0; i < copyCount; i++)
			{
				{
					SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
					Serialise_vkUpdateDescriptorSets(device, 0, NULL, 1, &pDescriptorCopies[i]);

					m_ContextRecord->AddChunk(scope.Get());
				}
				
				// VKTODOHIGH need to MarkResourceFrameReferenced the source descriptors
			}
		}

		// need to track descriptor set contents whether capframing or idle
		if(m_State >= WRITING)
		{
			for(uint32_t i=0; i < writeCount; i++)
			{
				VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(pDescriptorWrites[i].destSet));

				// VKTODOHIGH need to add some kind of parent ref on copied elements here

				RDCASSERT(pDescriptorWrites[i].destBinding < record->descBindings.size());
				
				VkDescriptorInfo *binding = record->descBindings[pDescriptorWrites[i].destBinding];

				for(uint32_t d=0; d < pDescriptorWrites[i].count; d++)
					record->descBindings[pDescriptorWrites[i].destBinding][pDescriptorWrites[i].destArrayElement + d] = pDescriptorWrites[i].pDescriptors[d];
			}

			if(copyCount > 0)
			{
				// don't want to implement this blindly
				RDCUNIMPLEMENTED("Copying descriptors not implemented");
			}
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkBeginCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResourceManager()->GetID(MakeRes(cmdBuffer)));

	ResourceId bakedCmdId;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
	}

	SERIALISE_ELEMENT(VkCmdBufferBeginInfo, info, *pBeginInfo);
	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);
	
	VkCmdBufferCreateInfo createInfo;
	VkDevice device = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		device = m_CmdBufferInfo[cmdId].device;
		createInfo = m_CmdBufferInfo[cmdId].createInfo;
	}
	else
	{
		m_CurCmdBufferID = bakeId;
	}
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	m_pSerialiser->Serialise("createInfo", createInfo);

	if(m_State < WRITING)
		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;

	if(m_State == EXECUTING)
	{
		const vector<uint32_t> &baseEvents = m_PartialReplayData.cmdBufferSubmits[bakeId];
		uint32_t length = m_CmdBufferInfo[bakeId].eventCount;

		for(auto it=baseEvents.begin(); it != baseEvents.end(); ++it)
		{
			if(*it < m_LastEventID && m_LastEventID < (*it + length))
			{
				RDCDEBUG("vkBegin - partial detected %u < %u < %u, %llu -> %llu", *it, m_LastEventID, *it + length, cmdId, bakeId);

				m_PartialReplayData.partialParent = cmdId;
				m_PartialReplayData.baseEvent = *it;
				m_PartialReplayData.renderPassActive = false;

				VkCmdBuffer cmd = VK_NULL_HANDLE;
				VkResult ret = m_Real.vkCreateCommandBuffer(device, &createInfo, &cmd);

				if(ret != VK_SUCCESS)
				{
					RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
				}
				else
				{
					GetResourceManager()->RegisterResource(MakeRes(cmd));
				}

				m_PartialReplayData.resultPartialCmdBuffer = cmd;
				m_PartialReplayData.partialDevice = device;

				// add one-time submit flag as this partial cmd buffer will only be submitted once
				info.flags |= VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT;

				m_Real.vkBeginCommandBuffer(cmd, &info);
			}
		}
	}
	else if(m_State == READING)
	{
		// remove one-time submit flag as we will want to submit many
		info.flags &= ~VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT;

		VkCmdBuffer cmd = VK_NULL_HANDLE;

		if(!GetResourceManager()->HasLiveResource(bakeId))
		{
			VkResult ret = m_Real.vkCreateCommandBuffer(device, &createInfo, &cmd);

			if(ret != VK_SUCCESS)
			{
				RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
			}
			else
			{
				ResourceId live = GetResourceManager()->RegisterResource(MakeRes(cmd));
				GetResourceManager()->AddLiveResource(bakeId, MakeRes(cmd));
			}

			// whenever a vkCmd command-building chunk asks for the command buffer, it
			// will get our baked version.
			GetResourceManager()->ReplaceResource(cmdId, bakeId);
		}
		else
		{
			cmd = (VkCmdBuffer)GetResourceManager()->GetLiveResource(bakeId).handle;
		}

		{
			ResourceId liveBaked = GetResourceManager()->GetLiveID(bakeId);
			m_CmdBufferInfo[liveBaked].device = VK_NULL_HANDLE;
		}

		m_Real.vkBeginCommandBuffer(cmd, &info);
	}

	return true;
}

VkResult WrappedVulkan::vkBeginCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo)
{
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));
	RDCASSERT(record);

	if(record)
	{
		if(record->bakedCommands)
			record->bakedCommands->Delete(GetResourceManager());

		ResourceId bakedId = ResourceIDGen::GetNewUniqueID();

		// VKTODOLOW: shouldn't depend on 'id' being a uint64_t member
		VkResource res(eResCmdBufferBake, bakedId.id);

		record->bakedCommands = GetResourceManager()->AddResourceRecord(res, bakedId);

		{
			SCOPED_SERIALISE_CONTEXT(BEGIN_CMD_BUFFER);
			Serialise_vkBeginCommandBuffer(cmdBuffer, pBeginInfo);
			
			record->AddChunk(scope.Get());
		}
	}

	return m_Real.vkBeginCommandBuffer(cmdBuffer, pBeginInfo);
}

bool WrappedVulkan::Serialise_vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResourceManager()->GetID(MakeRes(cmdBuffer)));

	ResourceId bakedCmdId;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
	}

	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdId))
		{
			RDCDEBUG("Ending partial command buffer for %llu baked to %llu", cmdId, bakeId);

			if(m_PartialReplayData.renderPassActive)
				m_Real.vkCmdEndRenderPass(PartialCmdBuf());

			m_Real.vkEndCommandBuffer(PartialCmdBuf());

			m_PartialReplayData.partialParent = ResourceId();
		}

		m_CurEventID--;
	}
	else if(m_State == READING)
	{
		VkCmdBuffer cmd = (VkCmdBuffer)GetResourceManager()->GetLiveResource(bakeId).handle;

		GetResourceManager()->RemoveReplacement(cmdId);

		m_Real.vkEndCommandBuffer(cmd);

		if(!m_CurEvents.empty())
		{
			FetchDrawcall draw;
			draw.name = "API Calls";
			draw.flags |= eDraw_SetMarker;

			// the outer loop will increment the event ID but we've not
			// actually added anything just wrapped up the existing EIDs.
			m_CurEventID--;

			AddDrawcall(draw, true);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));
	RDCASSERT(record);

	if(record)
	{
		RDCASSERT(record->bakedCommands);

		{
			SCOPED_SERIALISE_CONTEXT(END_CMD_BUFFER);
			Serialise_vkEndCommandBuffer(cmdBuffer);
			
			record->AddChunk(scope.Get());
		}

		record->Bake();
	}

	return m_Real.vkEndCommandBuffer(cmdBuffer);
}

bool WrappedVulkan::Serialise_vkResetCommandBuffer(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(VkCmdBufferResetFlags, fl, flags);

	ResourceId bakedCmdId;

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(cmdId);
		RDCASSERT(record->bakedCommands);
		if(record->bakedCommands)
			bakedCmdId = record->bakedCommands->GetResourceID();
	}

	SERIALISE_ELEMENT(ResourceId, bakeId, bakedCmdId);
	
	VkCmdBufferCreateInfo info;
	VkDevice device = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		device = m_CmdBufferInfo[cmdId].device;
		info = m_CmdBufferInfo[cmdId].createInfo;
	}
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	m_pSerialiser->Serialise("createInfo", info);

	if(m_State == EXECUTING)
	{
		// VKTODOHIGH check how vkResetCommandBuffer interacts with partial replays
	}
	else if(m_State == READING)
	{
		VkCmdBuffer cmd = VK_NULL_HANDLE;

		if(!GetResourceManager()->HasLiveResource(bakeId))
		{
			VkResult ret = m_Real.vkCreateCommandBuffer((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &info, &cmd);

			if(ret != VK_SUCCESS)
			{
				RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
			}
			else
			{
				ResourceId live = GetResourceManager()->RegisterResource(MakeRes(cmd));
				GetResourceManager()->AddLiveResource(bakeId, MakeRes(cmd));
			}

			// whenever a vkCmd command-building chunk asks for the command buffer, it
			// will get our baked version.
			GetResourceManager()->ReplaceResource(cmdId, bakeId);
		}
		else
		{
			cmd = (VkCmdBuffer)GetResourceManager()->GetLiveResource(bakeId).handle;
		}
		
		{
			ResourceId liveBaked = GetResourceManager()->GetLiveID(bakeId);
			m_CmdBufferInfo[liveBaked].device = VK_NULL_HANDLE;
		}

		m_Real.vkResetCommandBuffer(cmd, fl);
	}

	return true;
}

VkResult WrappedVulkan::vkResetCommandBuffer(
	  VkCmdBuffer                                 cmdBuffer,
    VkCmdBufferResetFlags                       flags)
{
	VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));
	RDCASSERT(record);

	if(record)
	{
		if(record->bakedCommands)
			record->bakedCommands->Delete(GetResourceManager());
		
		ResourceId bakedId = ResourceIDGen::GetNewUniqueID();

		// VKTODOLOW: shouldn't depend on 'id' being a uint64_t member
		VkResource res(eResCmdBufferBake, bakedId.id);

		record->bakedCommands = GetResourceManager()->AddResourceRecord(res, bakedId);

		// VKTODOHIGH do we need to actually serialise this at all? all it does is
		// reset a command buffer to be able to begin again. We could just move the
		// logic to create new baked commands from begin to here, and skip 
		// serialising this (as we never re-begin a cmd buffer, we make a new copy
		// for each bake).
		{
			SCOPED_SERIALISE_CONTEXT(RESET_CMD_BUFFER);
			Serialise_vkResetCommandBuffer(cmdBuffer, flags);
			
			record->AddChunk(scope.Get());
		}
	}

	return m_Real.vkResetCommandBuffer(cmdBuffer, flags);
}

// Command buffer building functions

bool WrappedVulkan::Serialise_vkCmdBeginRenderPass(
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(VkRenderPassBeginInfo, beginInfo, *pRenderPassBegin);
	SERIALISE_ELEMENT(VkRenderPassContents, cont, contents);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_PartialReplayData.renderPassActive = true;
			m_Real.vkCmdBeginRenderPass(PartialCmdBuf(), &beginInfo, cont);

			m_PartialReplayData.state.renderPass = GetResourceManager()->GetOriginalID(GetResourceManager()->GetID(MakeRes(beginInfo.renderPass)));
			m_PartialReplayData.state.framebuffer = GetResourceManager()->GetOriginalID(GetResourceManager()->GetID(MakeRes(beginInfo.framebuffer)));
			m_PartialReplayData.state.renderArea = beginInfo.renderArea;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdBeginRenderPass(cmdBuffer, &beginInfo, cont);

		const string desc = m_pSerialiser->GetDebugStr();

		// VKTODOMED change the name to show render pass load-op
		AddEvent(BEGIN_RENDERPASS, desc);
		FetchDrawcall draw;
		draw.name = "Command Buffer Start";
		draw.flags |= eDraw_Clear;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedVulkan::vkCmdBeginRenderPass(
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents)
{
	m_Real.vkCmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BEGIN_RENDERPASS);
		Serialise_vkCmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(pRenderPassBegin->renderPass)), eFrameRef_Read);
		// VKTODOMED should mark framebuffer read and attachments write
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(pRenderPassBegin->framebuffer)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_PartialReplayData.renderPassActive = false;
			m_Real.vkCmdEndRenderPass(PartialCmdBuf());

			m_PartialReplayData.state.renderPass = ResourceId();
			m_PartialReplayData.state.framebuffer = ResourceId();
			RDCEraseEl(m_PartialReplayData.state.renderArea);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdEndRenderPass(cmdBuffer);
	}

	return true;
}

void WrappedVulkan::vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	m_Real.vkCmdEndRenderPass(cmdBuffer);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(END_RENDERPASS);
		Serialise_vkCmdEndRenderPass(cmdBuffer);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdBindPipeline(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(VkPipelineBindPoint, bind, pipelineBindPoint);
	SERIALISE_ELEMENT(ResourceId, pipeid, GetResourceManager()->GetID(MakeRes(pipeline)));

	if(m_State == EXECUTING)
	{
		pipeline = (VkPipeline)GetResourceManager()->GetLiveResource(pipeid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindPipeline(PartialCmdBuf(), bind, pipeline);
			if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				m_PartialReplayData.state.graphics.pipeline = pipeid;
			else
				m_PartialReplayData.state.compute.pipeline = pipeid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		pipeline = (VkPipeline)GetResourceManager()->GetLiveResource(pipeid).handle;

		m_Real.vkCmdBindPipeline(cmdBuffer, bind, pipeline);
	}

	return true;
}

void WrappedVulkan::vkCmdBindPipeline(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline)
{
	m_Real.vkCmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_PIPELINE);
		Serialise_vkCmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(pipeline)), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDescriptorSets(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipelineLayout                            layout,
			uint32_t                                    firstSet,
			uint32_t                                    setCount,
			const VkDescriptorSet*                      pDescriptorSets,
			uint32_t                                    dynamicOffsetCount,
			const uint32_t*                             pDynamicOffsets)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, layoutid, GetResourceManager()->GetID(MakeRes(layout)));
	SERIALISE_ELEMENT(VkPipelineBindPoint, bind, pipelineBindPoint);
	SERIALISE_ELEMENT(uint32_t, first, firstSet);

	SERIALISE_ELEMENT(uint32_t, numSets, setCount);

	ResourceId *descriptorIDs = new ResourceId[numSets];

	VkDescriptorSet *sets = (VkDescriptorSet *)pDescriptorSets;
	if(m_State < WRITING)
		sets = new VkDescriptorSet[numSets];

	for(uint32_t i=0; i < numSets; i++)
	{
		if(m_State >= WRITING) descriptorIDs[i] = GetResourceManager()->GetID(MakeRes(sets[i]));
		m_pSerialiser->Serialise("DescriptorSet", descriptorIDs[i]);
		if(m_State < WRITING)  sets[i] = (VkDescriptorSet)GetResourceManager()->GetLiveResource(descriptorIDs[i]).handle;
	}

	SERIALISE_ELEMENT(uint32_t, offsCount, dynamicOffsetCount);
	SERIALISE_ELEMENT_ARR_OPT(uint32_t, offs, pDynamicOffsets, offsCount, offsCount > 0);

	if(m_State == EXECUTING)
	{
		layout = (VkPipelineLayout)GetResourceManager()->GetLiveResource(layoutid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindDescriptorSets(PartialCmdBuf(), bind, layout, first, numSets, sets, offsCount, offs);

			vector<ResourceId> &descsets =
				(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				? m_PartialReplayData.state.graphics.descSets
				: m_PartialReplayData.state.compute.descSets;

			// expand as necessary
			if(descsets.size() < first + numSets)
				descsets.resize(first + numSets);

			// VKTODOMED use layout to bake in dynamic offsets?
			for(uint32_t i=0; i < numSets; i++)
				descsets[first+i] = descriptorIDs[i];
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		layout = (VkPipelineLayout)GetResourceManager()->GetLiveResource(layoutid).handle;

		m_Real.vkCmdBindDescriptorSets(cmdBuffer, bind, layout, first, numSets, sets, offsCount, offs);
	}

	if(m_State < WRITING)
		SAFE_DELETE_ARRAY(sets);

	SAFE_DELETE_ARRAY(descriptorIDs);
	SAFE_DELETE_ARRAY(offs);

	return true;
}

void WrappedVulkan::vkCmdBindDescriptorSets(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipelineLayout                            layout,
			uint32_t                                    firstSet,
			uint32_t                                    setCount,
			const VkDescriptorSet*                      pDescriptorSets,
			uint32_t                                    dynamicOffsetCount,
			const uint32_t*                             pDynamicOffsets)
{
	m_Real.vkCmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_DESCRIPTOR_SET);
		Serialise_vkCmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(layout)), eFrameRef_Read);
		for(uint32_t i=0; i < setCount; i++)
			record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(pDescriptorSets[i])), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicViewportState)));

	if(m_State == EXECUTING)
	{
		dynamicViewportState = (VkDynamicViewportState)GetResourceManager()->GetLiveResource(stateid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindDynamicViewportState(PartialCmdBuf(), dynamicViewportState);
			m_PartialReplayData.state.dynamicVP = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		dynamicViewportState = (VkDynamicViewportState)GetResourceManager()->GetLiveResource(stateid).handle;

		m_Real.vkCmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	m_Real.vkCmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_VP_STATE);
		Serialise_vkCmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(dynamicViewportState)), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                        dynamicRasterState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicRasterState)));

	if(m_State == EXECUTING)
	{
		dynamicRasterState = (VkDynamicRasterState)GetResourceManager()->GetLiveResource(stateid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindDynamicRasterState(PartialCmdBuf(), dynamicRasterState);
			m_PartialReplayData.state.dynamicRS = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		dynamicRasterState = (VkDynamicRasterState)GetResourceManager()->GetLiveResource(stateid).handle;

		m_Real.vkCmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                      dynamicRasterState)
{
	m_Real.vkCmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_RS_STATE);
		Serialise_vkCmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(dynamicRasterState)), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicColorBlendState)));

	if(m_State == EXECUTING)
	{
		dynamicColorBlendState = (VkDynamicColorBlendState)GetResourceManager()->GetLiveResource(stateid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindDynamicColorBlendState(PartialCmdBuf(), dynamicColorBlendState);
			m_PartialReplayData.state.dynamicCB = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		dynamicColorBlendState = (VkDynamicColorBlendState)GetResourceManager()->GetLiveResource(stateid).handle;

		m_Real.vkCmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	m_Real.vkCmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_CB_STATE);
		Serialise_vkCmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(dynamicColorBlendState)), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicDepthStencilState)));

	if(m_State == EXECUTING)
	{
		dynamicDepthStencilState = (VkDynamicDepthStencilState)GetResourceManager()->GetLiveResource(stateid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindDynamicDepthStencilState(PartialCmdBuf(), dynamicDepthStencilState);
			m_PartialReplayData.state.dynamicDS = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		dynamicDepthStencilState = (VkDynamicDepthStencilState)GetResourceManager()->GetLiveResource(stateid).handle;

		m_Real.vkCmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	m_Real.vkCmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_DS_STATE);
		Serialise_vkCmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(dynamicDepthStencilState)), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, start, startBinding);
	SERIALISE_ELEMENT(uint32_t, count, bindingCount);

	vector<ResourceId> bufids;
	vector<VkBuffer> bufs;
	vector<VkDeviceSize> offs;

	for(uint32_t i=0; i < count; i++)
	{
		ResourceId id;
		VkDeviceSize o;
		if(m_State >= WRITING)
		{
			id = GetResourceManager()->GetID(MakeRes(pBuffers[i]));
			o = pOffsets[i];
		}

		m_pSerialiser->Serialise("pBuffers[]", id);
		m_pSerialiser->Serialise("pOffsets[]", o);

		if(m_State < WRITING)
		{
			bufids.push_back(id);
			bufs.push_back((VkBuffer)GetResourceManager()->GetLiveResource(id).handle);
			offs.push_back(o);
		}
	}

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindVertexBuffers(PartialCmdBuf(), start, count, &bufs[0], &offs[0]);

			if(m_PartialReplayData.state.vbuffers.size() < start + count)
				m_PartialReplayData.state.vbuffers.resize(start + count);

			for(uint32_t i=0; i < count; i++)
			{
				m_PartialReplayData.state.vbuffers[start + i].buf = bufids[i];
				m_PartialReplayData.state.vbuffers[start + i].offs = offs[i];
			}
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		
		m_Real.vkCmdBindVertexBuffers(cmdBuffer, start, count, &bufs[0], &offs[0]);
	}

	return true;
}

void WrappedVulkan::vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	m_Real.vkCmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_VERTEX_BUFFERS);
		Serialise_vkCmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);

		record->AddChunk(scope.Get());
		for(uint32_t i=0; i < bindingCount; i++)
			record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(pBuffers[i])), eFrameRef_Read);
	}
}


bool WrappedVulkan::Serialise_vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(MakeRes(buffer)));
	SERIALISE_ELEMENT(uint64_t, offs, offset);
	SERIALISE_ELEMENT(VkIndexType, idxType, indexType);

	if(m_State == EXECUTING)
	{
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdBindIndexBuffer(PartialCmdBuf(), buffer, offs, idxType);

			m_PartialReplayData.state.ibuffer.buf = bufid;
			m_PartialReplayData.state.ibuffer.offs = offs;
			m_PartialReplayData.state.ibuffer.bytewidth = idxType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;
		
		m_Real.vkCmdBindIndexBuffer(cmdBuffer, buffer, offs, idxType);
	}

	return true;
}

void WrappedVulkan::vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	m_Real.vkCmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BIND_INDEX_BUFFER);
		Serialise_vkCmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(buffer)), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdDraw(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstVertex,
	uint32_t       vertexCount,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, firstVtx, firstVertex);
	SERIALISE_ELEMENT(uint32_t, vtxCount, vertexCount);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdDraw(PartialCmdBuf(), firstVtx, vtxCount, firstInst, instCount);
	}
	else if(m_State == READING)
	{
		VkCmdBuffer buf = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdDraw(buf, firstVtx, vtxCount, firstInst, instCount);

		const string desc = m_pSerialiser->GetDebugStr();

		{
			AddEvent(DRAW, desc);
			string name = "vkCmdDraw(" +
				ToStr::Get(vtxCount) + "," +
				ToStr::Get(instCount) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.numIndices = vtxCount;
			draw.numInstances = instCount;
			draw.indexOffset = 0;
			draw.vertexOffset = firstVtx;
			draw.instanceOffset = firstInst;

			draw.flags |= eDraw_Drawcall;

			AddDrawcall(draw, true);
		}
	}

	return true;
}

void WrappedVulkan::vkCmdDraw(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstVertex,
	uint32_t       vertexCount,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	m_Real.vkCmdDraw(cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(DRAW);
		Serialise_vkCmdDraw(cmdBuffer, firstVertex, vertexCount, firstInstance, instanceCount);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdBlitImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkTexFilter                                 filter)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResourceManager()->GetID(MakeRes(srcImage)));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResourceManager()->GetID(MakeRes(destImage)));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

	SERIALISE_ELEMENT(VkTexFilter, f, filter);
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageBlit, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(srcid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(dstid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdBlitImage(PartialCmdBuf(), srcImage, srclayout, destImage, dstlayout, count, regions, f);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(srcid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(dstid).handle;

		m_Real.vkCmdBlitImage(cmdBuffer, srcImage, srclayout, destImage, dstlayout, count, regions, f);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdBlitImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageBlit*                          pRegions,
			VkTexFilter                                 filter)
{
	m_Real.vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BLIT_IMG);
		Serialise_vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResourceManager()->GetID(MakeRes(destImage)));
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(srcImage)), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(destImage)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResourceManager()->GetID(MakeRes(srcImage)));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResourceManager()->GetID(MakeRes(destImage)));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(srcid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(dstid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdCopyImage(PartialCmdBuf(), srcImage, srclayout, destImage, dstlayout, count, regions);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(srcid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(dstid).handle;

		m_Real.vkCmdCopyImage(cmdBuffer, srcImage, srclayout, destImage, dstlayout, count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     srcImage,
			VkImageLayout                               srcImageLayout,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkImageCopy*                          pRegions)
{
	m_Real.vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(COPY_IMG);
		Serialise_vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(srcImage)), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(destImage)), eFrameRef_Write);

		// VKTODOHIGH init states not implemented yet...
		//record->dirtied.insert(GetResourceManager()->GetID(MakeRes(destImage)));
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBufferToImage(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(MakeRes(srcBuffer)));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResourceManager()->GetID(MakeRes(destImage)));
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdCopyBufferToImage(PartialCmdBuf(), srcBuffer, destImage, destImageLayout, count, regions);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		srcBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;

		m_Real.vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyBufferToImage(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkImage                                     destImage,
			VkImageLayout                               destImageLayout,
			uint32_t                                    regionCount,
			const VkBufferImageCopy*                    pRegions)
{
	m_Real.vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(COPY_BUF2IMG);
		Serialise_vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResourceManager()->GetID(MakeRes(destImage)));
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(srcBuffer)), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(destImage)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyImageToBuffer(
    VkCmdBuffer                                 cmdBuffer,
		VkImage                                     srcImage,
		VkImageLayout                               srcImageLayout,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferImageCopy*                    pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(MakeRes(destBuffer)));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResourceManager()->GetID(MakeRes(srcImage)));

	SERIALISE_ELEMENT(VkImageLayout, layout, srcImageLayout);

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;
		destBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdCopyImageToBuffer(PartialCmdBuf(), srcImage, layout, destBuffer, count, regions);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;
		destBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		m_Real.vkCmdCopyImageToBuffer(cmdBuffer, srcImage, layout, destBuffer, count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyImageToBuffer(
    VkCmdBuffer                                 cmdBuffer,
		VkImage                                     srcImage,
		VkImageLayout                               srcImageLayout,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferImageCopy*                    pRegions)
{
	m_Real.vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(COPY_IMG2BUF);
		Serialise_vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(srcImage)), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(destBuffer)), eFrameRef_Write);

		// VKTODOMED: need to dirty the memory bound to the buffer?
		record->dirtied.insert(GetResourceManager()->GetID(MakeRes(destBuffer)));
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBuffer(
    VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    srcBuffer,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferCopy*                         pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResourceManager()->GetID(MakeRes(srcBuffer)));
	SERIALISE_ELEMENT(ResourceId, dstid, GetResourceManager()->GetID(MakeRes(destBuffer)));
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(srcid).handle;
		destBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(dstid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdCopyBuffer(PartialCmdBuf(), srcBuffer, destBuffer, count, regions);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		srcBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(srcid).handle;
		destBuffer = (VkBuffer)GetResourceManager()->GetLiveResource(dstid).handle;

		m_Real.vkCmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, count, regions);
	}

	SAFE_DELETE_ARRAY(regions);

	return true;
}

void WrappedVulkan::vkCmdCopyBuffer(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    srcBuffer,
			VkBuffer                                    destBuffer,
			uint32_t                                    regionCount,
			const VkBufferCopy*                         pRegions)
{
	m_Real.vkCmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(COPY_BUF);
		Serialise_vkCmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(srcBuffer)), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(destBuffer)), eFrameRef_Write);
		
		// VKTODOMED: need to dirty the memory bound to the buffer?
		record->dirtied.insert(GetResourceManager()->GetID(MakeRes(destBuffer)));
	}
}

bool WrappedVulkan::Serialise_vkCmdClearColorImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResourceManager()->GetID(MakeRes(image)));
	SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
	SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State == EXECUTING)
	{
		image = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdClearColorImage(PartialCmdBuf(), image, layout, &col, count, ranges);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		image = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;

		m_Real.vkCmdClearColorImage(cmdBuffer, image, layout, &col, count, ranges);
	}

	SAFE_DELETE_ARRAY(ranges);

	return true;
}

void WrappedVulkan::vkCmdClearColorImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	m_Real.vkCmdClearColorImage(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
		Serialise_vkCmdClearColorImage(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(image)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearDepthStencilImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResourceManager()->GetID(MakeRes(image)));
	SERIALISE_ELEMENT(VkImageLayout, l, imageLayout);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(byte, s, stencil);
	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State == EXECUTING)
	{
		image = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdClearDepthStencilImage(PartialCmdBuf(), image, l, d, s, count, ranges);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		image = (VkImage)GetResourceManager()->GetLiveResource(imgid).handle;

		m_Real.vkCmdClearDepthStencilImage(cmdBuffer, image, l, d, s, count, ranges);
	}

	SAFE_DELETE_ARRAY(ranges);

	return true;
}

void WrappedVulkan::vkCmdClearDepthStencilImage(
			VkCmdBuffer                                 cmdBuffer,
			VkImage                                     image,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rangeCount,
			const VkImageSubresourceRange*              pRanges)
{
	m_Real.vkCmdClearDepthStencilImage(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL);
		Serialise_vkCmdClearDepthStencilImage(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(image)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearColorAttachment(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    colorAttachment,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, att, colorAttachment);
	SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
	SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkRect3D, rects, pRects, count);
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdClearColorAttachment(PartialCmdBuf(), att, layout, &col, count, rects);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdClearColorAttachment(cmdBuffer, att, layout, &col, count, rects);

		const string desc = m_pSerialiser->GetDebugStr();

		{
			AddEvent(CLEAR_COLOR_ATTACH, desc);
			string name = "vkCmdClearColorAttachment(" +
				ToStr::Get(att) + "," +
				ToStr::Get(col) + ")";

			FetchDrawcall draw;
			draw.name = name;
			draw.flags |= eDraw_Clear|eDraw_ClearColour;

			AddDrawcall(draw, true);
		}
	}

	SAFE_DELETE_ARRAY(rects);

	return true;
}

void WrappedVulkan::vkCmdClearColorAttachment(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    colorAttachment,
			VkImageLayout                               imageLayout,
			const VkClearColorValue*                    pColor,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	m_Real.vkCmdClearColorAttachment(cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR_ATTACH);
		Serialise_vkCmdClearColorAttachment(cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);

		record->AddChunk(scope.Get());
		// VKTODOHIGH mark referenced the image under the attachment
		//record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(image)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdClearDepthStencilAttachment(
			VkCmdBuffer                                 cmdBuffer,
			VkImageAspectFlags                          imageAspectMask,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(VkImageAspectFlags, asp, imageAspectMask);
	SERIALISE_ELEMENT(VkImageLayout, lay, imageLayout);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(byte, s, stencil);
	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkRect3D, rects, pRects, count);
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdClearDepthStencilAttachment(PartialCmdBuf(), asp, lay, d, s, count, rects);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdClearDepthStencilAttachment(cmdBuffer, asp, lay, d, s, count, rects);
	}

	SAFE_DELETE_ARRAY(rects);

	return true;
}

void WrappedVulkan::vkCmdClearDepthStencilAttachment(
			VkCmdBuffer                                 cmdBuffer,
			VkImageAspectFlags                          imageAspectMask,
			VkImageLayout                               imageLayout,
			float                                       depth,
			uint32_t                                    stencil,
			uint32_t                                    rectCount,
			const VkRect3D*                             pRects)
{
	m_Real.vkCmdClearDepthStencilAttachment(cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL_ATTACH);
		Serialise_vkCmdClearDepthStencilAttachment(cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);

		record->AddChunk(scope.Get());
		// VKTODOHIGH mark referenced the image under the attachment
		//record->MarkResourceFrameReferenced(GetResourceManager()->GetID(MakeRes(image)), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdPipelineBarrier(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			VkBool32                                    byRegion,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(VkPipelineStageFlags, src, srcStageMask);
	SERIALISE_ELEMENT(VkPipelineStageFlags, dest, destStageMask);
	
	SERIALISE_ELEMENT(VkBool32, region, byRegion);

	SERIALISE_ELEMENT(uint32_t, memCount, memBarrierCount);

	vector<VkGenericStruct*> mems;
	vector<VkImageMemoryBarrier> imTrans;

	for(uint32_t i=0; i < memCount; i++)
	{
		SERIALISE_ELEMENT(VkStructureType, stype, ((VkGenericStruct *)ppMemBarriers[i])->type);

		if(stype == VK_STRUCTURE_TYPE_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkMemoryBarrier, barrier, *((VkMemoryBarrier *)ppMemBarriers[i]));

			if(m_State < WRITING)
				mems.push_back((VkGenericStruct *)new VkMemoryBarrier(barrier));
		}
		else if(stype == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkBufferMemoryBarrier, barrier, *((VkBufferMemoryBarrier *)ppMemBarriers[i]));

			if(m_State < WRITING)
				mems.push_back((VkGenericStruct *)new VkBufferMemoryBarrier(barrier));
		}
		else if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
		{
			SERIALISE_ELEMENT(VkImageMemoryBarrier, barrier, *((VkImageMemoryBarrier *)ppMemBarriers[i]));

			if(m_State < WRITING)
			{
				mems.push_back((VkGenericStruct *)new VkImageMemoryBarrier(barrier));
				imTrans.push_back(barrier);
			}
		}
	}
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			m_Real.vkCmdPipelineBarrier(PartialCmdBuf(), src, dest, region, memCount, (const void **)&mems[0]);

			ResourceId cmd = GetResourceManager()->GetID(MakeRes(PartialCmdBuf()));
			GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdPipelineBarrier(cmdBuffer, src, dest, region, memCount, (const void **)&mems[0]);
		
		ResourceId cmd = GetResourceManager()->GetID(MakeRes(cmdBuffer));
		GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);
	}

	for(size_t i=0; i < mems.size(); i++)
		delete mems[i];

	return true;
}

void WrappedVulkan::vkCmdPipelineBarrier(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        destStageMask,
			VkBool32                                    byRegion,
			uint32_t                                    memBarrierCount,
			const void* const*                          ppMemBarriers)
{
	m_Real.vkCmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(PIPELINE_BARRIER);
		Serialise_vkCmdPipelineBarrier(cmdBuffer, srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);

		record->AddChunk(scope.Get());

		vector<VkImageMemoryBarrier> imTrans;

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkStructureType stype = ((VkGenericStruct *)ppMemBarriers[i])->type;

			if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
				imTrans.push_back(*((VkImageMemoryBarrier *)ppMemBarriers[i]));
		}
		
		ResourceId cmd = GetResourceManager()->GetID(MakeRes(cmdBuffer));
		GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);

		// VKTODOMED do we need to mark frame referenced the resources in the barrier? if they're not referenced
		// elsewhere, perhaps they can be dropped
	}
}

VkResult WrappedVulkan::vkDbgCreateMsgCallback(
	VkInstance                          instance,
	VkFlags                             msgFlags,
	const PFN_vkDbgMsgCallback          pfnMsgCallback,
	void*                               pUserData,
	VkDbgMsgCallback*                   pMsgCallback)
{
	return m_Real.vkDbgCreateMsgCallback(instance, msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
}

VkResult WrappedVulkan::vkDbgDestroyMsgCallback(
	VkInstance                          instance,
	VkDbgMsgCallback                    msgCallback)
{
	return m_Real.vkDbgDestroyMsgCallback(instance, msgCallback);
}
	
bool WrappedVulkan::Serialise_vkCmdDbgMarkerBegin(
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker)
{
	string name = pMarker ? string(pMarker) : "";

	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	m_pSerialiser->Serialise("Name", name);
	
	if(m_State == READING)
	{
		FetchDrawcall draw;
		draw.name = name;
		draw.flags |= eDraw_PushMarker;

		AddDrawcall(draw, false);
	}

	return true;
}

void WrappedVulkan::vkCmdDbgMarkerBegin(
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker)
{
	if(m_State == WRITING_CAPFRAME)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
		Serialise_vkCmdDbgMarkerBegin(cmdBuffer, pMarker);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDbgMarkerEnd(VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	
	if(m_State == READING && !m_CurEvents.empty())
	{
		FetchDrawcall draw;
		draw.name = "API Calls";
		draw.flags |= eDraw_SetMarker;

		AddDrawcall(draw, true);
	}

	return true;
}

void WrappedVulkan::vkCmdDbgMarkerEnd(
	VkCmdBuffer  cmdBuffer)
{
	if(m_State == WRITING_CAPFRAME)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(END_EVENT);
		Serialise_vkCmdDbgMarkerEnd(cmdBuffer);

		record->AddChunk(scope.Get());
	}
}

uint32_t WrappedVulkan::GetReadbackMemoryIndex(uint32_t resourceRequiredBitmask)
{
	if(resourceRequiredBitmask & (1 << m_PhysicalReplayData[m_SwapPhysDevice].readbackMemIndex))
		return m_PhysicalReplayData[m_SwapPhysDevice].readbackMemIndex;

	return m_PhysicalReplayData[m_SwapPhysDevice].GetMemoryIndex(
		resourceRequiredBitmask,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_WRITE_COMBINED_BIT);
}

uint32_t WrappedVulkan::GetUploadMemoryIndex(uint32_t resourceRequiredBitmask)
{
	if(resourceRequiredBitmask & (1 << m_PhysicalReplayData[m_SwapPhysDevice].uploadMemIndex))
		return m_PhysicalReplayData[m_SwapPhysDevice].uploadMemIndex;

	return m_PhysicalReplayData[m_SwapPhysDevice].GetMemoryIndex(
		resourceRequiredBitmask,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
}

uint32_t WrappedVulkan::GetGPULocalMemoryIndex(uint32_t resourceRequiredBitmask)
{
	if(resourceRequiredBitmask & (1 << m_PhysicalReplayData[m_SwapPhysDevice].GPULocalMemIndex))
		return m_PhysicalReplayData[m_SwapPhysDevice].GPULocalMemIndex;

	return m_PhysicalReplayData[m_SwapPhysDevice].GetMemoryIndex(
		resourceRequiredBitmask,
		VK_MEMORY_PROPERTY_DEVICE_ONLY, 0);
}

uint32_t WrappedVulkan::ReplayData::GetMemoryIndex(uint32_t resourceRequiredBitmask, uint32_t allocRequiredProps, uint32_t allocUndesiredProps)
{
	uint32_t best = memProps.memoryTypeCount;
	
	for(uint32_t memIndex = 0; memIndex < memProps.memoryTypeCount; memIndex++)
	{
		if(resourceRequiredBitmask & (1 << memIndex))
		{
			uint32_t memTypeFlags = memProps.memoryTypes[memIndex].propertyFlags;

			if((memTypeFlags & allocRequiredProps) == allocRequiredProps)
			{
				if(memTypeFlags & allocUndesiredProps)
					best = memIndex;
				else
					return memIndex;
			}
		}
	}

	if(best == memProps.memoryTypeCount)
	{
		RDCERR("Couldn't find any matching heap! requirements %x / %x too strict", resourceRequiredBitmask, allocRequiredProps);
		return 0;
	}
	return best;
}

bool WrappedVulkan::ReleaseResource(VkResource res)
{
	// VKTODOHIGH: release resource with device from resource record

	return true;
}

void WrappedVulkan::Serialise_CaptureScope(uint64_t offset)
{
	SERIALISE_ELEMENT(uint32_t, FrameNumber, m_FrameCounter);

	if(m_State >= WRITING)
	{
		GetResourceManager()->Serialise_InitialContentsNeeded();
	}
	else
	{
		FetchFrameRecord record;
		record.frameInfo.fileOffset = offset;
		record.frameInfo.firstEvent = 1;//m_pImmediateContext->GetEventID();
		record.frameInfo.frameNumber = FrameNumber;
		record.frameInfo.immContextId = GetResourceManager()->GetOriginalID(m_ContextResourceID);
		m_FrameRecord.push_back(record);

		GetResourceManager()->CreateInitialContents();
	}
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);
	
	SERIALISE_ELEMENT(ResourceId, bbid, GetResourceManager()->GetID(MakeRes(presentImage)));

	RDCASSERT(presentImage != VK_NULL_HANDLE);

	bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
	m_pSerialiser->Serialise("HasCallstack", HasCallstack);	

	if(HasCallstack)
	{
		Callstack::Stackwalk *call = Callstack::Collect();

		RDCASSERT(call->NumLevels() < 0xff);

		size_t numLevels = call->NumLevels();
		uint64_t *stack = (uint64_t *)call->GetAddrs();

		m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

		delete call;
	}

	m_ContextRecord->AddChunk(scope.Get());
}

void WrappedVulkan::AttemptCapture()
{
	m_State = WRITING_CAPFRAME;

	{
		RDCDEBUG("Immediate Context %llu Attempting capture", GetContextResourceID());

		//m_SuccessfulCapture = true;

		m_ContextRecord->LockChunks();
		while(m_ContextRecord->HasChunks())
		{
			Chunk *chunk = m_ContextRecord->GetLastChunk();

			SAFE_DELETE(chunk);
			m_ContextRecord->PopChunk();
		}
		m_ContextRecord->UnlockChunks();
	}
}

bool WrappedVulkan::Serialise_BeginCaptureFrame(bool applyInitialState)
{
	if(m_State < WRITING && !applyInitialState)
	{
		m_pSerialiser->SkipCurrentChunk();
		return true;
	}

	vector<VkImageMemoryBarrier> imgTransitions;
	
	GetResourceManager()->SerialiseImageStates(m_pSerialiser, m_ImageInfo, imgTransitions);

	if(applyInitialState && !imgTransitions.empty())
	{
		VkCmdBuffer cmd = GetCmd();

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		m_Real.vkBeginCommandBuffer(cmd, &beginInfo);
		
    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		if(!imgTransitions.empty())
		{
			vector<void *> barriers;
			for(size_t i=0; i < imgTransitions.size(); i++)
				barriers.push_back(&imgTransitions[i]);
			m_Real.vkCmdPipelineBarrier(cmd, src_stages, dest_stages, false, (uint32_t)imgTransitions.size(), (const void *const *)&barriers[0]);
		}

		m_Real.vkEndCommandBuffer(cmd);
		m_Real.vkQueueSubmit(GetQ(), 1, &cmd, VK_NULL_HANDLE);
	}

	return true;
}
	
void WrappedVulkan::BeginCaptureFrame()
{
	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);

	Serialise_BeginCaptureFrame(false);

	// need to hold onto this as it must come right after the capture chunk,
	// before any command buffers
	m_HeaderChunk = scope.Get();
}

void WrappedVulkan::FinishCapture()
{
	m_State = WRITING_IDLE;

	//m_SuccessfulCapture = false;

	m_Real.vkDeviceWaitIdle(GetDev());
}

void WrappedVulkan::ReadLogInitialisation()
{
	uint64_t lastFrame = 0;
	uint64_t firstFrame = 0;

	m_pSerialiser->SetDebugText(true);

	m_pSerialiser->Rewind();

	while(!m_pSerialiser->AtEnd())
	{
		m_pSerialiser->SkipToChunk(CAPTURE_SCOPE);

		// found a capture chunk
		if(!m_pSerialiser->AtEnd())
		{
			lastFrame = m_pSerialiser->GetOffset();
			if(firstFrame == 0)
				firstFrame = m_pSerialiser->GetOffset();

			// skip this chunk
			m_pSerialiser->PushContext(NULL, CAPTURE_SCOPE, false);
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(NULL, CAPTURE_SCOPE);
		}
	}

	m_pSerialiser->Rewind();

	int chunkIdx = 0;

	struct chunkinfo
	{
		chunkinfo() : count(0), total(0.0) {}
		int count;
		double total;
	};

	map<VulkanChunkType,chunkinfo> chunkInfos;

	SCOPED_TIMER("chunk initialisation");

	while(1)
	{
		PerformanceTimer timer;

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, 1, false);

		if(context == CAPTURE_SCOPE)
		{
			// immediately read rest of log into memory
			m_pSerialiser->SetPersistentBlock(offset);
		}

		chunkIdx++;

		ProcessChunk(offset, context);

		m_pSerialiser->PopContext(NULL, context);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(m_pSerialiser->GetOffset())/float(m_pSerialiser->GetSize()));

		if(context == CAPTURE_SCOPE)
		{
			GetResourceManager()->ApplyInitialContents();

			ContextReplayLog(READING, 0, 0, false);

			if(m_pSerialiser->GetOffset() > lastFrame)
				break;
		}

		chunkInfos[context].total += timer.GetMilliseconds();
		chunkInfos[context].count++;

		if(m_pSerialiser->AtEnd())
		{
			break;
		}
	}

	for(auto it=chunkInfos.begin(); it != chunkInfos.end(); ++it)
	{
		RDCDEBUG("%hs: %.3f total time in %d chunks - %.3f average",
				GetChunkName(it->first), it->second.total, it->second.count,
				it->second.total/double(it->second.count));
	}

	RDCDEBUG("Allocating %llu persistant bytes of memory for the log.", m_pSerialiser->GetSize() - firstFrame);
	
	m_pSerialiser->SetDebugText(false);

	RDCASSERT(m_SwapPhysDevice >= 0 &&
	            m_PhysicalReplayData[m_SwapPhysDevice].dev != VK_NULL_HANDLE &&
	            m_PhysicalReplayData[m_SwapPhysDevice].q != VK_NULL_HANDLE &&
	            m_PhysicalReplayData[m_SwapPhysDevice].cmd != VK_NULL_HANDLE &&
	            m_PhysicalReplayData[m_SwapPhysDevice].cmdpool != VK_NULL_HANDLE);
}

void WrappedVulkan::ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial)
{
	m_State = readType;

	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, 1, false);
	RDCASSERT(header == CONTEXT_CAPTURE_HEADER);

	WrappedVulkan *context = this;

	Serialise_BeginCaptureFrame(!partial);
	
	m_Real.vkDeviceWaitIdle(GetDev());

	m_pSerialiser->PopContext(NULL, header);

	m_CurEvents.clear();
	
	if(m_State == EXECUTING)
	{
		FetchAPIEvent ev = GetEvent(startEventID);
		m_CurEventID = ev.eventID;

		// if not partial, we need to be sure to replay
		// past the command buffer records, so can't
		// skip to the file offset of the first event
		if(partial)
			m_pSerialiser->SetOffset(ev.fileOffset);

		m_FirstEventID = startEventID;
		m_LastEventID = endEventID;

		m_PartialReplayData.renderPassActive = false;
		RDCASSERT(m_PartialReplayData.resultPartialCmdBuffer == VK_NULL_HANDLE);
		m_PartialReplayData.partialParent = ResourceId();
		m_PartialReplayData.baseEvent = 0;
		m_PartialReplayData.state = PartialReplayData::StateVector();
	}
	else if(m_State == READING)
	{
		m_CurEventID = 1;
		m_CurDrawcallID = 1;
		m_FirstEventID = 0;
		m_LastEventID = ~0U;
	}

	// VKTODOMED I think this is a legacy concept that doesn't really mean anything anymore,
	// even on GL/D3D11. Creates are all shifted before the frame, only command bfufers remain
	// in vulkan
	//GetResourceManager()->MarkInFrame(true);

	while(1)
	{
		if(m_State == EXECUTING && m_CurEventID > endEventID && m_CurCmdBufferID == ResourceId())
		{
			// we can just break out if we've done all the events desired.
			// note that the command buffer events aren't 'real' and we just blaze through them
			break;
		}

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, 1, false);

		ContextProcessChunk(offset, context, false);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));
		
		// for now just abort after capture scope. Really we'd need to support multiple frames
		// but for now this will do.
		if(context == CONTEXT_CAPTURE_FOOTER)
			break;
		
		m_CurEventID++;
	}

	if(m_State == READING)
	{
		GetFrameRecord().back().drawcallList = m_ParentDrawcall.Bake();

		struct SortEID
		{
			bool operator() (const FetchAPIEvent &a, const FetchAPIEvent &b) { return a.eventID < b.eventID; }
		};

		std::sort(m_Events.begin(), m_Events.end(), SortEID());
		m_ParentDrawcall.children.clear();
	}

	// VKTODOMED See above
	//GetResourceManager()->MarkInFrame(false);

	if(m_PartialReplayData.resultPartialCmdBuffer != VK_NULL_HANDLE)
	{
		m_Real.vkDeviceWaitIdle(m_PartialReplayData.partialDevice);
		m_Real.vkDestroyCommandBuffer(m_PartialReplayData.partialDevice, m_PartialReplayData.resultPartialCmdBuffer);
		m_PartialReplayData.resultPartialCmdBuffer = VK_NULL_HANDLE;
	}

	m_State = READING;
}

void WrappedVulkan::ContextProcessChunk(uint64_t offset, VulkanChunkType chunk, bool forceExecute)
{
	m_CurChunkOffset = offset;

	uint64_t cOffs = m_pSerialiser->GetOffset();

	WrappedVulkan *context = this;

	LogState state = context->m_State;

	if(forceExecute)
		context->m_State = EXECUTING;
	else
		context->m_State = m_State;

	m_AddedDrawcall = false;

	ProcessChunk(offset, chunk);

	m_pSerialiser->PopContext(NULL, chunk);
	
	if(context->m_State == READING && chunk == SET_MARKER)
	{
		// no push/pop necessary
	}
	else if(context->m_State == READING && chunk == BEGIN_EVENT)
	{
		// push down the drawcallstack to the latest drawcall
		context->m_DrawcallStack.push_back(&context->m_DrawcallStack.back()->children.back());
	}
	else if(context->m_State == READING && chunk == END_EVENT)
	{
		// refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
		RDCASSERT(context->m_DrawcallStack.size() > 1);
		if(context->m_DrawcallStack.size() > 1)
			context->m_DrawcallStack.pop_back();
	}
	else if(chunk == BEGIN_CMD_BUFFER)
	{
		if(context->m_State == READING)
		{
			DrawcallTreeNode *draw = new DrawcallTreeNode;

			RDCASSERT(m_CurCmdBufferID != ResourceId());
			m_CmdBufferInfo[m_CurCmdBufferID].draw = draw;

			context->m_DrawcallStack.push_back(draw);
		}

		// we know that command buffers always come before any other events,
		// so we aren't trashing useful data here.
		// We restart the count from 1 to account for a fake marker at the
		// start of the command buffer, but the events and drawcalls recorded
		// locally into the command buffers drawcall in m_CmdBufferInfo are
		// 0-based. Then on queue submit we just increment all child
		// events/drawcalls by the current 'next' ID and insert them into
		// the tree.
		// this happens on reading AND executing to make sure event IDs stay
		// consistent
		m_CurEventID = 1;
		m_CurDrawcallID = 1;
	}
	else if(chunk == END_CMD_BUFFER)
	{
		if(context->m_State == READING)
		{
			RDCASSERT(m_CurCmdBufferID != ResourceId());
			m_CmdBufferInfo[m_CurCmdBufferID].eventCount = m_CurEventID;
			m_CmdBufferInfo[m_CurCmdBufferID].drawCount = m_CurDrawcallID;

			if(context->m_DrawcallStack.size() > 1)
				context->m_DrawcallStack.pop_back();
		}

		m_CurCmdBufferID = ResourceId();

		// reset to starting event/drawcall IDs as we might be doing the actual
		// frame events now
		m_CurEventID = 1;
		m_CurDrawcallID = 1;
	}
	else if(context->m_State == READING)
	{
		if(!m_AddedDrawcall)
			context->AddEvent(chunk, m_pSerialiser->GetDebugStr());
	}

	m_AddedDrawcall = false;
	
	if(forceExecute)
		context->m_State = state;
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexed(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstIndex,
	uint32_t       indexCount,
	int32_t        vertexOffset,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, firstIdx, firstIndex);
	SERIALISE_ELEMENT(uint32_t, idxCount, indexCount);
	SERIALISE_ELEMENT(int32_t, vtxOffs, vertexOffset);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdDrawIndexed(PartialCmdBuf(), firstIdx, idxCount, vtxOffs, firstInst, instCount);
	}
	else if(m_State == READING)
	{
		VkCmdBuffer buf = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdDrawIndexed(buf, firstIdx, idxCount, vtxOffs, firstInst, instCount);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndexed(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstIndex,
	uint32_t       indexCount,
	int32_t        vertexOffset,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	m_Real.vkCmdDrawIndexed(cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED);
		Serialise_vkCmdDrawIndexed(cmdBuffer, firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndirect(
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(MakeRes(buffer)));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT(uint32_t, strd, stride);

	if(m_State == EXECUTING)
	{
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdDrawIndirect(PartialCmdBuf(), buffer, offs, cnt, strd);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		m_Real.vkCmdDrawIndirect(cmdBuffer, buffer, offs, cnt, strd);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndirect(
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	m_Real.vkCmdDrawIndirect(cmdBuffer, buffer, offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(DRAW_INDIRECT);
		Serialise_vkCmdDrawIndirect(cmdBuffer, buffer, offset, count, stride);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDrawIndexedIndirect(
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(MakeRes(buffer)));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT(uint32_t, strd, stride);

	if(m_State == EXECUTING)
	{
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdDrawIndexedIndirect(PartialCmdBuf(), buffer, offs, cnt, strd);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		m_Real.vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offs, cnt, strd);
	}

	return true;
}

void WrappedVulkan::vkCmdDrawIndexedIndirect(
		VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    buffer,
		VkDeviceSize                                offset,
		uint32_t                                    count,
		uint32_t                                    stride)
{
	m_Real.vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(DRAW_INDEXED_INDIRECT);
		Serialise_vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offset, count, stride);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDispatch(
	VkCmdBuffer cmdBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, X, x);
	SERIALISE_ELEMENT(uint32_t, Y, y);
	SERIALISE_ELEMENT(uint32_t, Z, z);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdDispatch(PartialCmdBuf(), x, y, z);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdDispatch(cmdBuffer, X, Y, Z);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatch(
	VkCmdBuffer cmdBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	m_Real.vkCmdDispatch(cmdBuffer, x, y, z);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(DISPATCH);
		Serialise_vkCmdDispatch(cmdBuffer, x, y, z);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDispatchIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResourceManager()->GetID(MakeRes(buffer)));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	if(m_State == EXECUTING)
	{
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		if(IsPartialCmd(cmdid) && InPartialRange())
			m_Real.vkCmdDispatchIndirect(PartialCmdBuf(), buffer, offs);
	}
	else if(m_State == READING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		buffer = (VkBuffer)GetResourceManager()->GetLiveResource(bufid).handle;

		m_Real.vkCmdDispatchIndirect(cmdBuffer, buffer, offs);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	m_Real.vkCmdDispatchIndirect(cmdBuffer, buffer, offset);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(DISPATCH_INDIRECT);
		Serialise_vkCmdDispatchIndirect(cmdBuffer, buffer, offset);

		record->AddChunk(scope.Get());
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// WSI extension

VkResult WrappedVulkan::vkGetPhysicalDeviceSurfaceSupportWSI(
		VkPhysicalDevice                        physicalDevice,
		uint32_t                                queueFamilyIndex,
		const VkSurfaceDescriptionWSI*          pSurfaceDescription,
		VkBool32*                               pSupported)
{
	return m_Real.vkGetPhysicalDeviceSurfaceSupportWSI(physicalDevice, queueFamilyIndex, pSurfaceDescription, pSupported);
}

VkResult WrappedVulkan::vkGetSurfaceInfoWSI(
		VkDevice                                 device,
		const VkSurfaceDescriptionWSI*           pSurfaceDescription,
		VkSurfaceInfoTypeWSI                     infoType,
		size_t*                                  pDataSize,
		void*                                    pData)
{
	return m_Real.vkGetSurfaceInfoWSI(device, pSurfaceDescription, infoType, pDataSize, pData);
}

bool WrappedVulkan::Serialise_vkGetSwapChainInfoWSI(
		VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    VkSwapChainInfoTypeWSI                   infoType,
    size_t*                                  pDataSize,
    void*                                    pData)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, swapId, GetResourceManager()->GetID(MakeRes(swapChain)));
	VkSwapChainImagePropertiesWSI *image = (VkSwapChainImagePropertiesWSI *)pData;
	SERIALISE_ELEMENT(size_t, idx, *pDataSize);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(image->image)));

	if(m_State >= WRITING)
	{
		RDCASSERT(infoType == VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI);
	}

	if(m_State == READING)
	{
		// VKTODOLOW what if num images is less than on capture?
		RDCASSERT(idx < m_SwapChainInfo[swapId].images.size());
		VkImage im = m_SwapChainInfo[swapId].images[idx].im;

		GetResourceManager()->AddLiveResource(id, MakeRes(im));
	}

	return true;
}

VkResult WrappedVulkan::vkGetSwapChainInfoWSI(
		VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    VkSwapChainInfoTypeWSI                   infoType,
    size_t*                                  pDataSize,
    void*                                    pData)
{
	// make sure we always get the size
	size_t dummySize = 0;
	if(pDataSize == NULL)
		pDataSize = &dummySize;

	VkResult ret = m_Real.vkGetSwapChainInfoWSI(device, swapChain, infoType, pDataSize, pData);

	if(infoType == VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI && pData && m_State >= WRITING)
	{
		VkSwapChainImagePropertiesWSI *images = (VkSwapChainImagePropertiesWSI *)pData;
		size_t numImages = (*pDataSize)/sizeof(VkSwapChainImagePropertiesWSI);

		for(size_t i=0; i < numImages; i++)
		{
			VkResource res = MakeRes(images[i].image);

			// already registered
			if(GetResourceManager()->GetID(res) != ResourceId())
				continue;

			ResourceId id = GetResourceManager()->RegisterResource(res);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(PRESENT_IMAGE);
					Serialise_vkGetSwapChainInfoWSI(device, swapChain, infoType, &i, (void *)&images[i]);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
				record->AddChunk(chunk);

				// we invert the usual scheme - we make the swapchain record take parent refs
				// on these images, so that we can just ref the swapchain on present and pull
				// in all the images
				GetResourceManager()->GetResourceRecord(MakeRes(swapChain))->AddParent(record);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, res);
			}
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkAcquireNextImageWSI(
		VkDevice                                 device,
		VkSwapChainWSI                           swapChain,
		uint64_t                                 timeout,
		VkSemaphore                              semaphore,
		uint32_t*                                pImageIndex)
{
	// VKTODOLOW: does this need to be intercepted/serialised?
	return m_Real.vkAcquireNextImageWSI(device, swapChain, timeout, semaphore, pImageIndex);
}

bool WrappedVulkan::Serialise_vkCreateSwapChainWSI(
		VkDevice                                device,
		const VkSwapChainCreateInfoWSI*         pCreateInfo,
		VkSwapChainWSI*                         pSwapChain)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(VkSwapChainCreateInfoWSI, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(*pSwapChain)));

	uint32_t numIms = 0;

	if(m_State >= WRITING)
	{
		VkResult vkr = VK_SUCCESS;

    size_t swapChainImagesSize;
    vkr = m_Real.vkGetSwapChainInfoWSI(device, *pSwapChain, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
    RDCASSERT(vkr == VK_SUCCESS);

		numIms = uint32_t(swapChainImagesSize/sizeof(VkSwapChainImagePropertiesWSI));
	}

	SERIALISE_ELEMENT(uint32_t, numSwapImages, numIms);

	m_SwapChainInfo[id].format = info.imageFormat;
	m_SwapChainInfo[id].extent = info.imageExtent;
	m_SwapChainInfo[id].arraySize = info.imageArraySize;

	m_SwapChainInfo[id].images.resize(numSwapImages);

	if(m_State == READING)
	{
		VkDevice dev = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;

		const VkImageCreateInfo imInfo = {
			/*.sType =*/ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			/*.pNext =*/ NULL,
			/*.imageType =*/ VK_IMAGE_TYPE_2D,
			/*.format =*/ info.imageFormat,
			/*.extent =*/ { info.imageExtent.width, info.imageExtent.height, 1 },
			/*.mipLevels =*/ 1,
			/*.arraySize =*/ info.imageArraySize,
			/*.samples =*/ 1,
			/*.tiling =*/ VK_IMAGE_TILING_OPTIMAL,
			/*.usage =*/
			VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|
			VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
			VK_IMAGE_USAGE_SAMPLED_BIT,
			/*.flags =*/ 0,
		};

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].dev == dev)
				m_SwapPhysDevice = (int)i;
		}

		for(uint32_t i=0; i < numSwapImages; i++)
		{
			VkDeviceMemory mem = VK_NULL_HANDLE;
			VkImage im = VK_NULL_HANDLE;

			VkResult vkr = m_Real.vkCreateImage(dev, &imInfo, &im);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkMemoryRequirements mrq = {0};

			vkr = m_Real.vkGetImageMemoryRequirements(dev, im, &mrq);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				mrq.size, GetGPULocalMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = m_Real.vkAllocMemory(dev, &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);

			vkr = m_Real.vkBindImageMemory(dev, im, mem, 0);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->RegisterResource(MakeRes(mem));
			ResourceId liveId = GetResourceManager()->RegisterResource(MakeRes(im));

			// image live ID will be assigned separately in Serialise_vkGetSwapChainInfoWSI
			// memory doesn't have a live ID

			m_SwapChainInfo[id].images[i].mem = mem;
			m_SwapChainInfo[id].images[i].im = im;

			// fill out image info so we track resource state transitions
			m_ImageInfo[liveId].mem = mem;
			m_ImageInfo[liveId].type = VK_IMAGE_TYPE_2D;
			m_ImageInfo[liveId].format = info.imageFormat;
			m_ImageInfo[liveId].extent.width = info.imageExtent.width;
			m_ImageInfo[liveId].extent.height = info.imageExtent.height;
			m_ImageInfo[liveId].extent.depth = 1;
			m_ImageInfo[liveId].mipLevels = 1;
			m_ImageInfo[liveId].arraySize = info.imageArraySize;

			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArraySlice = 0;
			range.mipLevels = 1;
			range.arraySize = info.imageArraySize;
			range.aspect = VK_IMAGE_ASPECT_COLOR;

			m_ImageInfo[liveId].subresourceStates.clear();
			m_ImageInfo[liveId].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSwapChainWSI(
		VkDevice                                device,
		const VkSwapChainCreateInfoWSI*         pCreateInfo,
		VkSwapChainWSI*                         pSwapChain)
{
	VkResult ret = m_Real.vkCreateSwapChainWSI(device, pCreateInfo, pSwapChain);
	
	if(ret == VK_SUCCESS)
	{
		VkResource res = MakeRes(*pSwapChain);
		ResourceId id = GetResourceManager()->RegisterResource(res);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);
				Serialise_vkCreateSwapChainWSI(device, pCreateInfo, pSwapChain);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(id);
			record->AddChunk(chunk);

			for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
			{
				if(m_PhysicalReplayData[i].dev == device)
					m_SwapPhysDevice = (int)i;
			}

			// serialise out the swap chain images
			{
				size_t swapChainImagesSize;
				VkResult ret = m_Real.vkGetSwapChainInfoWSI(device, *pSwapChain, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
				RDCASSERT(ret == VK_SUCCESS);

				uint32_t numSwapImages = uint32_t(swapChainImagesSize)/sizeof(VkSwapChainImagePropertiesWSI);

				VkSwapChainImagePropertiesWSI* images = new VkSwapChainImagePropertiesWSI[numSwapImages];

				// go through our own function so we assign these images IDs
				ret = vkGetSwapChainInfoWSI(device, *pSwapChain, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, images);
				RDCASSERT(ret == VK_SUCCESS);

				for(uint32_t i=0; i < numSwapImages; i++)
				{
					// memory doesn't exist for genuine WSI created images
					m_SwapChainInfo[id].images[i].mem = VK_NULL_HANDLE;
					m_SwapChainInfo[id].images[i].im = images[i].image;

					ResourceId imid = GetResourceManager()->GetID(MakeRes(images[i].image));

					// fill out image info so we track resource state transitions
					m_ImageInfo[imid].type = VK_IMAGE_TYPE_2D;
					m_ImageInfo[imid].format = pCreateInfo->imageFormat;
					m_ImageInfo[imid].extent.width = pCreateInfo->imageExtent.width;
					m_ImageInfo[imid].extent.height = pCreateInfo->imageExtent.height;
					m_ImageInfo[imid].extent.depth = 1;
					m_ImageInfo[imid].mipLevels = 1;
					m_ImageInfo[imid].arraySize = pCreateInfo->imageArraySize;

					VkImageSubresourceRange range;
					range.baseMipLevel = range.baseArraySlice = 0;
					range.mipLevels = 1;
					range.arraySize = pCreateInfo->imageArraySize;
					range.aspect = VK_IMAGE_ASPECT_COLOR;

					m_ImageInfo[imid].subresourceStates.clear();
					m_ImageInfo[imid].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
				}

				SAFE_DELETE_ARRAY(images);
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkQueuePresentWSI(
			VkQueue                                 queue,
			VkPresentInfoWSI*                       pPresentInfo)
{
	VkResult ret = m_Real.vkQueuePresentWSI(queue, pPresentInfo);

	if(ret != VK_SUCCESS || pPresentInfo->swapChainCount == 0)
		return ret;

	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);
	
	if(m_State == WRITING_IDLE)
		RenderDoc::Inst().Tick();
	
	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame

	if(pPresentInfo->swapChainCount > 1 && (m_FrameCounter % 100) == 0)
	{
		RDCWARN("Presenting multiple swapchains at once - only first will be processed");
	}
	
	if(m_State == WRITING_IDLE)
	{
		m_FrameTimes.push_back(m_FrameTimer.GetMilliseconds());
		m_TotalTime += m_FrameTimes.back();
		m_FrameTimer.Restart();

		// update every second
		if(m_TotalTime > 1000.0)
		{
			m_MinFrametime = 10000.0;
			m_MaxFrametime = 0.0;
			m_AvgFrametime = 0.0;

			m_TotalTime = 0.0;

			for(size_t i=0; i < m_FrameTimes.size(); i++)
			{
				m_AvgFrametime += m_FrameTimes[i];
				if(m_FrameTimes[i] < m_MinFrametime)
					m_MinFrametime = m_FrameTimes[i];
				if(m_FrameTimes[i] > m_MaxFrametime)
					m_MaxFrametime = m_FrameTimes[i];
			}

			m_AvgFrametime /= double(m_FrameTimes.size());

			m_FrameTimes.clear();

			RDCLOG("Frame: %d. F12/PrtScr to capture. %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)",
				m_FrameCounter, m_AvgFrametime, m_MinFrametime, m_MaxFrametime, 1000.0f/m_AvgFrametime);
			for(size_t i=0; i < m_FrameRecord.size(); i++)
				RDCLOG("Captured Frame %d. Multiple frame capture not supported.\n", m_FrameRecord[i].frameInfo.frameNumber);
#if !defined(RELEASE)
			RDCLOG("%llu chunks - %.2f MB", Chunk::NumLiveChunks(), float(Chunk::TotalMem())/1024.0f/1024.0f);
#endif
		}
	}
	
	// kill any current capture
	if(m_State == WRITING_CAPFRAME)
	{
		//if(HasSuccessfulCapture())
		{
			RDCLOG("Finished capture, Frame %u", m_FrameCounter);

			ResourceId swapid = GetResourceManager()->GetID(MakeRes(pPresentInfo->swapChains[0]));

			GetResourceManager()->MarkResourceFrameReferenced(swapid, eFrameRef_Read);

			// VKTODOLOW handle present info pNext
			RDCASSERT(pPresentInfo->pNext == NULL);

			VkImage backbuffer = m_SwapChainInfo[swapid].images[pPresentInfo->imageIndices[0]].im;

			EndCaptureFrame(backbuffer);
			FinishCapture();

			Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(m_FrameCounter, &m_InitParams, NULL, 0, 0, 0);

			{
				SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

				SERIALISE_ELEMENT(ResourceId, immContextId, m_ContextResourceID);

				m_pFileSerialiser->Insert(scope.Get(true));
			}

			RDCDEBUG("Inserting Resource Serialisers");	

			GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);
			
			GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

			RDCDEBUG("Creating Capture Scope");	

			{
				SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

				Serialise_CaptureScope(0);

				m_pFileSerialiser->Insert(scope.Get(true));

				m_pFileSerialiser->Insert(m_HeaderChunk);
			}

			{
				RDCDEBUG("Flushing %u command buffer records to file serialiser", (uint32_t)m_CmdBufferRecords.size());	

				map<int32_t, Chunk *> recordlist;

				// ensure all command buffer records are disjoint and all present before queue submits
				for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
				{
					recordlist.clear();
					m_CmdBufferRecords[i]->Insert(recordlist);

					RDCDEBUG("Adding %u chunks to file serialiser from command buffer %llu", (uint32_t)recordlist.size(), m_CmdBufferRecords[i]->GetResourceID());	

					for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
						m_pFileSerialiser->Insert(it->second);

					m_CmdBufferRecords[i]->Delete(GetResourceManager());
				}

				recordlist.clear();
				m_ContextRecord->Insert(recordlist);

				RDCDEBUG("Flushing %u chunks to file serialiser from context record", (uint32_t)recordlist.size());	

				for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
					m_pFileSerialiser->Insert(it->second);

				RDCDEBUG("Done");	
			}

			m_CurFileSize += m_pFileSerialiser->FlushToDisk();

			RenderDoc::Inst().SuccessfullyWrittenLog();

			SAFE_DELETE(m_pFileSerialiser);
			SAFE_DELETE(m_HeaderChunk);

			m_State = WRITING_IDLE;
			
			GetResourceManager()->MarkUnwrittenResources();

			GetResourceManager()->ClearReferencedResources();
		}
	}

	if(RenderDoc::Inst().ShouldTriggerCapture(m_FrameCounter) && m_State == WRITING_IDLE && m_FrameRecord.empty())
	{
		m_State = WRITING_CAPFRAME;

		FetchFrameRecord record;
		record.frameInfo.frameNumber = m_FrameCounter+1;
		m_FrameRecord.push_back(record);

		GetResourceManager()->ClearReferencedResources();

		GetResourceManager()->MarkResourceFrameReferenced(m_InstanceRecord->GetResourceID(), eFrameRef_Read);
		GetResourceManager()->PrepareInitialContents();
		
		AttemptCapture();
		BeginCaptureFrame();

		RDCLOG("Starting capture, frame %u", m_FrameCounter);
	}

	return ret;
}

bool WrappedVulkan::Prepare_InitialState(VkResource res)
{
	ResourceId id = GetResourceManager()->GetID(res);

	RDCDEBUG("Prepare_InitialState %llu", id);
	
	if(res.Namespace == eResDescriptorSet)
	{
		RDCUNIMPLEMENTED("descriptor set initial states not implemented");

		// VKTODOHIGH: need to figure out the format/serialisation of these
		return true;
	}
	else if(res.Namespace == eResDeviceMemory)
	{
		if(m_MemoryInfo.find(id) == m_MemoryInfo.end())
		{
			RDCERR("Couldn't find memory info");
			return false;
		}

		MemState &meminfo = m_MemoryInfo[id];

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		VkQueue q = GetQ();
		VkCmdBuffer cmd = GetCmd();

		VkDeviceMemory mem = VK_NULL_HANDLE;
		
		// VKTODOMED should get mem requirements for buffer - copy might enforce
		// some restrictions?
		VkMemoryRequirements mrq = { meminfo.size, 16, ~0U };

		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			meminfo.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = m_Real.vkAllocMemory(d, &allocInfo, &mem);
		RDCASSERT(vkr == VK_SUCCESS);

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = m_Real.vkBeginCommandBuffer(cmd, &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			meminfo.size, VK_BUFFER_USAGE_GENERAL, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		VkBuffer srcBuf, dstBuf;

		vkr = m_Real.vkCreateBuffer(d, &bufInfo, &srcBuf);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = m_Real.vkCreateBuffer(d, &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = m_Real.vkBindBufferMemory(d, srcBuf, (VkDeviceMemory)res.handle, 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = m_Real.vkBindBufferMemory(d, dstBuf, mem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { 0, 0, meminfo.size };

		m_Real.vkCmdCopyBuffer(cmd, srcBuf, dstBuf, 1, &region);
	
		vkr = m_Real.vkEndCommandBuffer(cmd);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = m_Real.vkQueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOMED would be nice to store a fence too at this point
		// so we can sync on that on serialise rather than syncing
		// every time.
		m_Real.vkQueueWaitIdle(q);

		m_Real.vkDestroyBuffer(d, srcBuf);
		m_Real.vkDestroyBuffer(d, dstBuf);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(MakeRes(mem), (uint32_t)meminfo.size, NULL));

		return true;
	}
	else if(res.Namespace == eResImage)
	{
		RDCUNIMPLEMENTED("image initial states not implemented");

		if(m_ImageInfo.find(id) == m_ImageInfo.end())
		{
			RDCERR("Couldn't find image info");
			return false;
		}

		// VKTODOHIGH: need to copy off contents to memory somewhere else

		return true;
	}
	else
	{
		RDCERR("Unhandled resource type %d", res.Namespace);
	}

	return false;
}

bool WrappedVulkan::Serialise_InitialState(VkResource res)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(res));

	if(m_State < WRITING) res = GetResourceManager()->GetLiveResource(id);
	
	if(m_State >= WRITING)
	{
		VulkanResourceManager::InitialContentData initContents = GetResourceManager()->GetInitialContents(id);

		if(res.Namespace == eResDescriptorSet)
		{
			RDCUNIMPLEMENTED("descriptor set initial states not implemented");

			// VKTODOHIGH: need to figure out the format/serialisation of these
		}
		else if(res.Namespace == eResImage || res.Namespace == eResDeviceMemory)
		{
			VkDevice d = GetDev();

			byte *ptr = NULL;
			m_Real.vkMapMemory(d, (VkDeviceMemory)initContents.resource.handle, 0, 0, 0, (void **)&ptr);

			size_t dataSize = (size_t)initContents.num;

			m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

			m_Real.vkUnmapMemory(d, (VkDeviceMemory)initContents.resource.handle);
		}
	}
	else
	{
		if(res.Namespace == eResDescriptorSet)
		{
			RDCUNIMPLEMENTED("Descriptor set initial states not implemented");
		}
		else if(res.Namespace == eResImage || res.Namespace == eResDeviceMemory)
		{
			byte *data = NULL;
			size_t dataSize = 0;
			m_pSerialiser->SerialiseBuffer("data", data, dataSize);

			VkResult vkr = VK_SUCCESS;

			VkDevice d = GetDev();

			VkDeviceMemory mem = VK_NULL_HANDLE;

			// VKTODOMED should get mem requirements for buffer - copy might enforce
			// some restrictions?
			VkMemoryRequirements mrq = { dataSize, 16, ~0U };

			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				dataSize, GetUploadMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = m_Real.vkAllocMemory(d, &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);

			VkBufferCreateInfo bufInfo = {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
				dataSize, VK_BUFFER_USAGE_GENERAL, 0,
				VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			};

			VkBuffer buf;

			vkr = m_Real.vkCreateBuffer(d, &bufInfo, &buf);
			RDCASSERT(vkr == VK_SUCCESS);

			vkr = m_Real.vkBindBufferMemory(d, buf, mem, 0);
			RDCASSERT(vkr == VK_SUCCESS);

			byte *ptr = NULL;
			m_Real.vkMapMemory(d, mem, 0, 0, 0, (void **)&ptr);

			// VKTODOLOW could deserialise directly into this ptr if we serialised
			// size separately.
			memcpy(ptr, data, dataSize);

			m_Real.vkUnmapMemory(d, mem);

			// VKTODOMED leaking the memory here! needs to be cleaned up with the buffer
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(MakeRes(buf), eInitialContents_Copy, NULL));
		}
	}

	return true;
}

void WrappedVulkan::Create_InitialState(ResourceId id, VkResource live, bool hasData)
{
	if(live.Namespace == eResDescriptorSet)
	{
		RDCERR("Unexpected attempt to create initial state for descriptor set");
	}
	else if(live.Namespace == eResImage)
	{
		RDCUNIMPLEMENTED("image initial states not implemented");

		if(m_ImageInfo.find(id) == m_ImageInfo.end())
		{
			RDCERR("Couldn't find image info");
			return;
		}

		VkResource z(MakeNullResource);

		ImgState &img = m_ImageInfo[id];

		if(img.subresourceStates[0].range.aspect == VK_IMAGE_ASPECT_COLOR)
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(z, eInitialContents_ClearColorImage, NULL));
		else
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(z, eInitialContents_ClearDepthStencilImage, NULL));
	}
	else if(live.Namespace == eResDeviceMemory)
	{
		RDCERR("Unexpected attempt to create initial state for memory");
	}
	else if(live.Namespace == eResFramebuffer)
	{
		RDCWARN("Framebuffer without initial state! should clear all attachments");
	}
	else
	{
		RDCERR("Unhandled resource type %d", live.Namespace);
	}
}

void WrappedVulkan::Apply_InitialState(VkResource live, VulkanResourceManager::InitialContentData initial)
{
	if(live.Namespace == eResDescriptorSet)
	{
		// VKTODOHIGH: need to figure out the format/serialisation of these
		RDCUNIMPLEMENTED("descriptor set initial states not implemented");
	}
	else if(live.Namespace == eResDeviceMemory)
	{
		ResourceId id = GetResourceManager()->GetID(live);

		if(m_MemoryInfo.find(id) == m_MemoryInfo.end())
		{
			RDCERR("Couldn't find memory info");
			return;
		}

		MemState &meminfo = m_MemoryInfo[id];
		
		VkBuffer srcBuf = (VkBuffer)initial.resource.handle;
		VkDeviceMemory dstMem = (VkDeviceMemory)live.handle;

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		VkQueue q = GetQ();
		VkCmdBuffer cmd = GetCmd();

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = m_Real.vkBeginCommandBuffer(cmd, &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			meminfo.size, VK_BUFFER_USAGE_GENERAL, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		VkBuffer dstBuf;
		
		// VKTODOMED this should be created once up front, not every time
		vkr = m_Real.vkCreateBuffer(d, &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = m_Real.vkBindBufferMemory(d, dstBuf, dstMem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { 0, 0, meminfo.size };

		m_Real.vkCmdCopyBuffer(cmd, srcBuf, dstBuf, 1, &region);
	
		vkr = m_Real.vkEndCommandBuffer(cmd);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = m_Real.vkQueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOMED would be nice to store a fence too at this point
		// so we can sync on that on serialise rather than syncing
		// every time.
		m_Real.vkQueueWaitIdle(q);

		m_Real.vkDestroyBuffer(d, dstBuf);
	}
	else if(live.Namespace == eResImage)
	{
		// VKTODOHIGH: need to copy initial copy to live
		RDCUNIMPLEMENTED("image initial states not implemented");
	}
	else
	{
		RDCERR("Unhandled resource type %d", live.Namespace);
	}
}

void WrappedVulkan::ProcessChunk(uint64_t offset, VulkanChunkType context)
{
	switch(context)
	{
	case DEVICE_INIT:
		{
			SERIALISE_ELEMENT(ResourceId, immContextId, ResourceId());

			GetResourceManager()->AddLiveResource(immContextId, VkResource(eResSpecial, VK_NULL_HANDLE));
			break;
		}
	case ENUM_PHYSICALS:
		Serialise_vkEnumeratePhysicalDevices(NULL, NULL, NULL);
		break;
	case CREATE_DEVICE:
		Serialise_vkCreateDevice(VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_DEVICE_QUEUE:
		Serialise_vkGetDeviceQueue(VK_NULL_HANDLE, 0, 0, NULL);
		break;

	case ALLOC_MEM:
		Serialise_vkAllocMemory(VK_NULL_HANDLE, NULL, NULL);
		break;
	case UNMAP_MEM:
		Serialise_vkUnmapMemory(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case FREE_MEM:
		// VKTODOMED see vkFreeMemory
		//Serialise_vkFreeMemory(VK_NULL_HANDLE, VK_NULL_HANDLE);
		//break;
	case CREATE_CMD_POOL:
		Serialise_vkCreateCommandPool(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_CMD_BUFFER:
		RDCERR("vkCreateCommandBuffer should not be serialised directly");
		break;
	case CREATE_FRAMEBUFFER:
		Serialise_vkCreateFramebuffer(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_RENDERPASS:
		Serialise_vkCreateRenderPass(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_POOL:
		Serialise_vkCreateDescriptorPool(VK_NULL_HANDLE, VK_DESCRIPTOR_POOL_USAGE_MAX_ENUM, 0, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_SET:
		Serialise_vkAllocDescriptorSets(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_DESCRIPTOR_SET_USAGE_MAX_ENUM, 0, NULL, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_SET_LAYOUT:
		Serialise_vkCreateDescriptorSetLayout(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_BUFFER:
		Serialise_vkCreateBuffer(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_BUFFER_VIEW:
		Serialise_vkCreateBufferView(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_IMAGE:
		Serialise_vkCreateImage(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_IMAGE_VIEW:
		Serialise_vkCreateImageView(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_ATTACHMENT_VIEW:
		Serialise_vkCreateAttachmentView(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_VIEWPORT_STATE:
		Serialise_vkCreateDynamicViewportState(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_RASTER_STATE:
		Serialise_vkCreateDynamicRasterState(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_BLEND_STATE:
		Serialise_vkCreateDynamicColorBlendState(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_DEPTH_STATE:
		Serialise_vkCreateDynamicDepthStencilState(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_SAMPLER:
		Serialise_vkCreateSampler(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_SHADER:
		Serialise_vkCreateShader(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_SHADER_MODULE:
		Serialise_vkCreateShaderModule(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_PIPE_LAYOUT:
		Serialise_vkCreatePipelineLayout(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_PIPE_CACHE:
		Serialise_vkCreatePipelineCache(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_GRAPHICS_PIPE:
		Serialise_vkCreateGraphicsPipelines(VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL);
		break;
	case CREATE_COMPUTE_PIPE:
		//VKTODOMED:
		//Serialise_vkCreateComputePipelines(VK_NULL_HANDLE, NULL, NULL);
		break;
	case PRESENT_IMAGE:
		Serialise_vkGetSwapChainInfoWSI(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SWAP_CHAIN_INFO_TYPE_MAX_ENUM_WSI, NULL, NULL);
		break;

	case CREATE_SEMAPHORE:
		Serialise_vkCreateSemaphore(VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_FENCE:
		//VKTODOMED:
		//Serialise_vkCreateFence(VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_FENCE_STATUS:
		//VKTODOMED:
		//Serialise_vkGetFenceStatus(VK_NULL_HANDLE);
		break;
	case WAIT_FENCES:
		//VKTODOMED:
		//Serialise_vkWaitForFences(VK_NULL_HANDLE, 0, NULL, VK_FALSE, 0.0f);
		break;

	case ALLOC_DESC_SET:
		Serialise_vkAllocDescriptorSets(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_DESCRIPTOR_SET_USAGE_MAX_ENUM, 0, NULL, NULL, NULL);
		break;
	case UPDATE_DESC_SET:
		Serialise_vkUpdateDescriptorSets(VK_NULL_HANDLE, 0, NULL, 0, NULL);
		break;

	case RESET_CMD_BUFFER:
		Serialise_vkResetCommandBuffer(VK_NULL_HANDLE, 0);
		break;
	case BEGIN_CMD_BUFFER:
		Serialise_vkBeginCommandBuffer(VK_NULL_HANDLE, NULL);
		break;
	case END_CMD_BUFFER:
		Serialise_vkEndCommandBuffer(VK_NULL_HANDLE);
		break;

	case QUEUE_SIGNAL_SEMAPHORE:
		Serialise_vkQueueSignalSemaphore(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case QUEUE_WAIT_SEMAPHORE:
		Serialise_vkQueueWaitSemaphore(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case QUEUE_WAIT_IDLE:
		Serialise_vkQueueWaitIdle(VK_NULL_HANDLE);
		break;
	case DEVICE_WAIT_IDLE:
		Serialise_vkDeviceWaitIdle(VK_NULL_HANDLE);
		break;

	case QUEUE_SUBMIT:
		Serialise_vkQueueSubmit(VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
		break;
	case BIND_BUFFER_MEM:
		Serialise_vkBindBufferMemory(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;
	case BIND_IMAGE_MEM:
		Serialise_vkBindImageMemory(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;

	case BEGIN_RENDERPASS:
		Serialise_vkCmdBeginRenderPass(VK_NULL_HANDLE, NULL, VK_RENDER_PASS_CONTENTS_MAX_ENUM);
		break;
	case END_RENDERPASS:
		Serialise_vkCmdEndRenderPass(VK_NULL_HANDLE);
		break;

	case BIND_PIPELINE:
		Serialise_vkCmdBindPipeline(VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_NULL_HANDLE);
		break;
	case BIND_VP_STATE:
		Serialise_vkCmdBindDynamicViewportState(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case BIND_RS_STATE:
		Serialise_vkCmdBindDynamicRasterState(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case BIND_CB_STATE:
		Serialise_vkCmdBindDynamicColorBlendState(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case BIND_DS_STATE:
		Serialise_vkCmdBindDynamicDepthStencilState(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case BIND_DESCRIPTOR_SET:
		Serialise_vkCmdBindDescriptorSets(VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_NULL_HANDLE, 0, 0, NULL, 0, NULL);
		break;
	case BIND_INDEX_BUFFER:
		Serialise_vkCmdBindIndexBuffer(VK_NULL_HANDLE, VK_NULL_HANDLE, 0, VK_INDEX_TYPE_MAX_ENUM);
		break;
	case BIND_VERTEX_BUFFERS:
		Serialise_vkCmdBindVertexBuffers(VK_NULL_HANDLE, 0, 0, NULL, NULL);
		break;
	case COPY_BUF2IMG:
		Serialise_vkCmdCopyBufferToImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case COPY_IMG2BUF:
		Serialise_vkCmdCopyImageToBuffer(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, 0, NULL);
		break;
	case COPY_IMG:
		Serialise_vkCmdCopyImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case COPY_BUF:
		Serialise_vkCmdCopyBuffer(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
		break;
	case CLEAR_COLOR:
		Serialise_vkCmdClearColorImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_DEPTHSTENCIL:
		Serialise_vkCmdClearDepthStencilImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0.0f, 0, 0, NULL);
		break;
	case CLEAR_COLOR_ATTACH:
		Serialise_vkCmdClearColorAttachment(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_DEPTHSTENCIL_ATTACH:
		Serialise_vkCmdClearDepthStencilAttachment(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0.0f, 0, 0, NULL);
		break;
	case PIPELINE_BARRIER:
		Serialise_vkCmdPipelineBarrier(VK_NULL_HANDLE, 0, 0, VK_FALSE, 0, NULL);
		break;
	case RESOLVE_IMAGE:
		//VKTODOMED:
		//Serialise_vkCmdResolveImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
		break;
	case WRITE_TIMESTAMP:
		//VKTODOMED:
		//Serialise_vkCmdWriteTimestamp(VK_NULL_HANDLE, VK_TIMESTAMP_TYPE_MAX_ENUM, VK_NULL_HANDLE, 0);
		break;
	case DRAW:
		Serialise_vkCmdDraw(VK_NULL_HANDLE, 0, 0, 0, 0);
		break;
	case DRAW_INDIRECT:
		Serialise_vkCmdDrawIndirect(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
		break;
	case DRAW_INDEXED:
		Serialise_vkCmdDrawIndexed(VK_NULL_HANDLE, 0, 0, 0, 0, 0);
		break;
	case DRAW_INDEXED_INDIRECT:
		Serialise_vkCmdDrawIndexedIndirect(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
		break;
	case DISPATCH:
		Serialise_vkCmdDispatch(VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DISPATCH_INDIRECT:
		Serialise_vkCmdDispatchIndirect(VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;

	case BEGIN_EVENT:
		Serialise_vkCmdDbgMarkerBegin(VK_NULL_HANDLE, NULL);
		break;
	case SET_MARKER:
		RDCFATAL("No such function vkCmdDbgMarker");
		break;
	case END_EVENT:
		Serialise_vkCmdDbgMarkerEnd(VK_NULL_HANDLE);
		break;

	case CREATE_SWAP_BUFFER:
		Serialise_vkCreateSwapChainWSI(VK_NULL_HANDLE, NULL, NULL);
		break;

	case CAPTURE_SCOPE:
		Serialise_CaptureScope(offset);
		break;
	case CONTEXT_CAPTURE_FOOTER:
		{
			SERIALISE_ELEMENT(ResourceId, bbid, ResourceId());

			ResourceId liveBBid = GetResourceManager()->GetLiveID(bbid);

			m_FakeBBImgId = bbid;
			m_FakeBBIm = (VkImage)GetResourceManager()->GetLiveResource(bbid).handle;
			m_FakeBBExtent = m_ImageInfo[liveBBid].extent;
			m_FakeBBFmt = MakeResourceFormat(m_ImageInfo[liveBBid].format);

			bool HasCallstack = false;
			m_pSerialiser->Serialise("HasCallstack", HasCallstack);	

			if(HasCallstack)
			{
				size_t numLevels = 0;
				uint64_t *stack = NULL;

				m_pSerialiser->SerialisePODArray("callstack", stack, numLevels);

				m_pSerialiser->SetCallstack(stack, numLevels);

				SAFE_DELETE_ARRAY(stack);
			}

			if(m_State == READING)
			{
				AddEvent(CONTEXT_CAPTURE_FOOTER, "vkQueuePresentWSI()");

				FetchDrawcall draw;
				draw.name = "vkQueuePresentWSI()";
				draw.flags |= eDraw_Present;

				AddDrawcall(draw, true);
			}
		}
		break;
	default:
		// ignore system chunks
		if((int)context == (int)INITIAL_CONTENTS)
			Serialise_InitialState(VkResource(MakeNullResource));
		else if((int)context < (int)FIRST_CHUNK_ID)
			m_pSerialiser->SkipCurrentChunk();
		else
			RDCERR("Unrecognised Chunk type %d", context);
		break;
	}
}

void WrappedVulkan::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	RDCASSERT(frameID < (uint32_t)m_FrameRecord.size());

	// VKTODOHIGH figure out how replaying only a draw will work.
	if(replayType == eReplay_OnlyDraw) return;
	if(replayType == eReplay_WithoutDraw) { replayType = eReplay_Full; }

	uint64_t offs = m_FrameRecord[frameID].frameInfo.fileOffset;

	m_pSerialiser->SetOffset(offs);

	bool partial = true;

	if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
	{
		startEventID = m_FrameRecord[frameID].frameInfo.firstEvent;
		partial = false;
	}
	
	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, 1, false);

	RDCASSERT(header == CAPTURE_SCOPE);

	m_pSerialiser->SkipCurrentChunk();

	m_pSerialiser->PopContext(NULL, header);
	
	if(!partial)
	{
		GetResourceManager()->ApplyInitialContents();
		GetResourceManager()->ReleaseInFrameResources();

		// VKTODOLOW temp hack - clear backbuffer to black
		if(m_FakeBBImgId != ResourceId())
		{
			VkDevice dev = GetDev();
			VkCmdBuffer cmd = GetCmd();
			VkQueue q = GetQ();
			const VulkanFunctions &vk = m_Real;

			VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

			VkResult vkr = vk.vkBeginCommandBuffer(cmd, &beginInfo);
			RDCASSERT(vkr == VK_SUCCESS);

			ImgState &st = m_ImageInfo[GetResourceManager()->GetLiveID(m_FakeBBImgId)];
			RDCASSERT(st.subresourceStates.size() == 1);

			VkImageMemoryBarrier t;
			t.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			t.pNext = NULL;
			t.inputMask = 0;
			t.outputMask = 0;
			t.srcQueueFamilyIndex = 0;
			t.destQueueFamilyIndex = 0;
			t.image = m_FakeBBIm;
			t.oldLayout = st.subresourceStates[0].state;
			t.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			t.subresourceRange = st.subresourceStates[0].range;

			void *barrier = (void *)&t;

			st.subresourceStates[0].state = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, (void **)&barrier);

			VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f, } };
			vk.vkCmdClearColorImage(cmd, m_FakeBBIm, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &clearColor, 1, &t.subresourceRange);

			vkr = vk.vkEndCommandBuffer(cmd);
			RDCASSERT(vkr == VK_SUCCESS);

			vkr = vk.vkQueueSubmit(q, 1, &cmd, VK_NULL_HANDLE);
			RDCASSERT(vkr == VK_SUCCESS);
		}
	}
	
	{
		if(replayType == eReplay_Full)
			ContextReplayLog(EXECUTING, startEventID, endEventID, partial);
		else if(replayType == eReplay_WithoutDraw)
			ContextReplayLog(EXECUTING, startEventID, RDCMAX(1U,endEventID)-1, partial);
		else if(replayType == eReplay_OnlyDraw)
			ContextReplayLog(EXECUTING, endEventID, endEventID, partial);
		else
			RDCFATAL("Unexpected replay type");
	}
}

void WrappedVulkan::DebugCallback(
				VkFlags             msgFlags,
				VkDbgObjectType     objType,
				uint64_t            srcObject,
				size_t              location,
				int32_t             msgCode,
				const char*         pLayerPrefix,
				const char*         pMsg)
{
	RDCWARN("debug message:\n%s", pMsg);
}

void WrappedVulkan::AddDrawcall(FetchDrawcall d, bool hasEvents)
{
	if(d.context == ResourceId()) d.context = GetResourceManager()->GetOriginalID(m_ContextResourceID);

	m_AddedDrawcall = true;

	WrappedVulkan *context = this;

	FetchDrawcall draw = d;
	draw.eventID = m_CurEventID;
	draw.drawcallID = m_CurDrawcallID;

	for(int i=0; i < 8; i++)
		draw.outputs[i] = ResourceId();

	draw.depthOut = ResourceId();

	// VKTODOHIGH set index byte width here
	// VKTODOHIGH set topology here

	m_CurDrawcallID++;
	if(hasEvents)
	{
		vector<FetchAPIEvent> evs;
		evs.reserve(m_CurEvents.size());
		for(size_t i=0; i < m_CurEvents.size(); )
		{
			if(m_CurEvents[i].context == draw.context)
			{
				evs.push_back(m_CurEvents[i]);
				m_CurEvents.erase(m_CurEvents.begin()+i);
			}
			else
			{
				i++;
			}
		}

		draw.events = evs;
	}

	//AddUsage(draw);
	
	// should have at least the root drawcall here, push this drawcall
	// onto the back's children list.
	if(!context->m_DrawcallStack.empty())
	{
		DrawcallTreeNode node(draw);
		node.children.insert(node.children.begin(), draw.children.elems, draw.children.elems+draw.children.count);
		context->m_DrawcallStack.back()->children.push_back(node);
	}
	else
		RDCERR("Somehow lost drawcall stack!");
}

void WrappedVulkan::AddEvent(VulkanChunkType type, string description, ResourceId ctx)
{
	if(ctx == ResourceId()) ctx = GetResourceManager()->GetOriginalID(m_ContextResourceID);

	FetchAPIEvent apievent;

	apievent.context = ctx;
	apievent.fileOffset = m_CurChunkOffset;
	apievent.eventID = m_CurEventID;

	apievent.eventDesc = description;

	Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
	if(stack)
	{
		create_array(apievent.callstack, stack->NumLevels());
		memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t)*stack->NumLevels());
	}

	m_CurEvents.push_back(apievent);

	if(m_State == READING && m_CurCmdBufferID == ResourceId())
		m_Events.push_back(apievent);
}

FetchAPIEvent WrappedVulkan::GetEvent(uint32_t eventID)
{
	for(size_t i=m_Events.size()-1; i > 0; i--)
	{
		if(m_Events[i].eventID <= eventID)
			return m_Events[i];
	}

	return m_Events[0];
}
