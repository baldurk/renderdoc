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

// VKTODO drop m_DeviceRecord - move EVERYTHING to resource records.

static bool operator <(const VkExtensionProperties &a, const VkExtensionProperties &b)
{
	int cmp = strcmp(a.extName, b.extName);
	if(cmp == 0)
		return a.specVersion < b.specVersion;

	return cmp < 0;
}

// VKTODO assertion of structure types, handling of pNext or assert == NULL
// VKTODO bring back image state tracking :(

const char *VkChunkNames[] =
{
	"WrappedVulkan::Initialisation",
	"vkCreateInstance",
	"vkInitAndEnumerateGpus",
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

	"vkCreateFence",
	"vkGetFenceStatus",
	"vkWaitForFences",

	"vkAllocDescriptorSets",
	"vkUpdateDescriptorSets",

	"vkResetCommandBuffer",
	"vkBeginCommandBuffer",
	"vkEndCommandBuffer",

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

	return eReplayCreate_Success;
}

WrappedVulkan::WrappedVulkan(const VulkanFunctions &real, const char *logFilename)
	: m_Real(real)
{
#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif
	
#if !defined(_RELEASE)
	CaptureOptions &opts = (CaptureOptions &)RenderDoc::Inst().GetCaptureOptions();
	opts.RefAllResources = true;
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

	m_DrawcallStack.push_back(&m_ParentDrawcall);

	m_FakeBBImgId = ResourceId();
	m_FakeBBIm = VK_NULL_HANDLE;
	m_FakeBBMem = VK_NULL_HANDLE;

	m_ResourceManager = new VulkanResourceManager(m_State, m_pSerialiser, this);
	
	m_DeviceResourceID = GetResourceManager()->RegisterResource(VkResource(eResSpecial, VK_NULL_HANDLE));
	m_ContextResourceID = GetResourceManager()->RegisterResource(VkResource(eResSpecial, VK_NULL_HANDLE));

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_DeviceResourceID);
		m_DeviceRecord->DataInSerialiser = false;
		m_DeviceRecord->Length = 0;
		m_DeviceRecord->NumSubResources = 0;
		m_DeviceRecord->SpecialResource = true;
		m_DeviceRecord->SubResources = NULL;
		
		m_ContextRecord = GetResourceManager()->AddResourceRecord(m_ContextResourceID);
		m_ContextRecord->DataInSerialiser = false;
		m_ContextRecord->Length = 0;
		m_ContextRecord->NumSubResources = 0;
		m_ContextRecord->SpecialResource = true;
		m_ContextRecord->SubResources = NULL;
	}
	else
	{
		m_DeviceRecord = m_ContextRecord = NULL;

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
	if(m_MsgCallback)
	{
		m_Real.vkDbgDestroyMsgCallback(instance, m_MsgCallback);
	}
#endif

	// VKTODO clean up replay resources here?
}

const char * WrappedVulkan::GetChunkName(uint32_t idx)
{
	if(idx < FIRST_CHUNK_ID || idx >= NUM_VULKAN_CHUNKS)
		return "<unknown>";
	return VkChunkNames[idx-FIRST_CHUNK_ID];
}

bool WrappedVulkan::Serialise_vkCreateInstance(
		const VkInstanceCreateInfo*                 pCreateInfo,
		VkInstance*                                 pInstance)
{
	string app = "";
	string engine = "";
	vector<string> layers;
	vector<string> extensions;

	if(m_State >= WRITING)
	{
		if(pCreateInfo->pAppInfo)
		{
			// VKTODO
			RDCASSERT(pCreateInfo->pAppInfo->pNext == NULL);

			app = pCreateInfo->pAppInfo->pAppName ? pCreateInfo->pAppInfo->pAppName : "";
			engine = pCreateInfo->pAppInfo->pEngineName ? pCreateInfo->pAppInfo->pEngineName : "";
		}

		layers.resize(pCreateInfo->layerCount);
		extensions.resize(pCreateInfo->extensionCount);

		for(uint32_t i=0; i < pCreateInfo->layerCount; i++)
			layers[i] = pCreateInfo->ppEnabledLayerNames[i];

		for(uint32_t i=0; i < pCreateInfo->extensionCount; i++)
			extensions[i] = pCreateInfo->ppEnabledExtensionNames[i];
	}

	m_pSerialiser->Serialise("AppName", app);
	m_pSerialiser->Serialise("EngineName", engine);
	SERIALISE_ELEMENT(uint32_t, AppVersion, pCreateInfo->pAppInfo ? pCreateInfo->pAppInfo->appVersion : 0);
	SERIALISE_ELEMENT(uint32_t, EngineVersion, pCreateInfo->pAppInfo ? pCreateInfo->pAppInfo->engineVersion : 0);
	SERIALISE_ELEMENT(uint32_t, apiVersion, pCreateInfo->pAppInfo ? pCreateInfo->pAppInfo->apiVersion : 0);

	m_pSerialiser->Serialise("Layers", layers);
	m_pSerialiser->Serialise("Extensions", extensions);

	SERIALISE_ELEMENT(ResourceId, instId, GetResourceManager()->GetID(MakeRes(*pInstance)));

	if(m_State == READING)
	{
		// VKTODO verify that layers/extensions are available

		app = std::string("RenderDoc (") + app + ")";
		engine = std::string("RenderDoc (") + engine + ")";

		const char **layerscstr = new const char *[layers.size()];
		for(size_t i=0; i < layers.size(); i++)
			layerscstr[i] = layers[i].c_str();

		const char **extscstr = new const char *[extensions.size()];
		for(size_t i=0; i < extensions.size(); i++)
			extscstr[i] = extensions[i].c_str();

    VkApplicationInfo appinfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pAppName = app.c_str(),
        .appVersion = AppVersion,
        .pEngineName = engine.c_str(),
        .engineVersion = EngineVersion,
        .apiVersion = VK_API_VERSION,
    };
    VkInstanceCreateInfo instinfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .pAppInfo = &appinfo,
        .pAllocCb = NULL,
        .layerCount = (uint32_t)layers.size(),
        .ppEnabledLayerNames = layerscstr,
        .extensionCount = (uint32_t)extensions.size(),
        .ppEnabledExtensionNames = extscstr,
    };

		VkInstance inst;

		VkResult ret = m_Real.vkCreateInstance(&instinfo, &inst);

#if defined(FORCE_VALIDATION_LAYER)
		if(m_Real.vkDbgCreateMsgCallback)
		{
			VkFlags flags = VK_DBG_REPORT_INFO_BIT |
				VK_DBG_REPORT_WARN_BIT |
				VK_DBG_REPORT_PERF_WARN_BIT |
				VK_DBG_REPORT_ERROR_BIT |
				VK_DBG_REPORT_DEBUG_BIT;
			m_Real.vkDbgCreateMsgCallback(inst, flags, &DebugCallbackStatic, this, &m_MsgCallback);
		}
#endif

		GetResourceManager()->RegisterResource(MakeRes(inst));
		GetResourceManager()->AddLiveResource(instId, MakeRes(inst));

		SAFE_DELETE_ARRAY(layerscstr);
		SAFE_DELETE_ARRAY(extscstr);
	}

	return true;
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
		opts.RefAllResources = true;
		opts.DebugDeviceMode = true;
	}
#endif

	// VKTODO we should try and fetch vkDbgCreateMsgCallback ourselves if it isn't
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
		SCOPED_SERIALISE_CONTEXT(CREATE_INSTANCE);
		Serialise_vkCreateInstance(pCreateInfo, &inst);
				
		m_DeviceRecord->AddChunk(scope.Get());
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
	
	if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode && m_MsgCallback)
	{
		m_Real.vkDbgDestroyMsgCallback(instance, m_MsgCallback);
	}

	VkResource res = MakeRes(instance);
	if(GetResourceManager()->HasCurrentResource(res))
	{
		if(GetResourceManager()->HasResourceRecord(res))
				GetResourceManager()->GetResourceRecord(res)->Delete(GetResourceManager());
		GetResourceManager()->UnregisterResource(res);
	}

	return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
{
	SERIALISE_ELEMENT(ResourceId, inst, GetResourceManager()->GetID(MakeRes(instance)));
	
	SERIALISE_ELEMENT(uint32_t, physCount, *pPhysicalDeviceCount);

	uint32_t count;
	VkPhysicalDevice devices[8]; // VKTODO: dynamically allocate
	if(m_State < WRITING)
	{
		instance = (VkInstance)GetResourceManager()->GetLiveResource(inst).handle;
		VkResult ret = m_Real.vkEnumeratePhysicalDevices(instance, &count, devices);

		// VKTODO
		RDCASSERT(ret == VK_SUCCESS);

		// VKTODO match up physical devices to those available on replay
	}
	
	for(uint32_t i=0; i < physCount; i++)
	{
		SERIALISE_ELEMENT(ResourceId, physId, GetResourceManager()->GetID(MakeRes(pPhysicalDevices[i])));

		VkPhysicalDevice pd = VK_NULL_HANDLE;

		if(m_State >= WRITING)
		{
			pd = pPhysicalDevices[i];
		}
		else
		{
			pd = devices[i];

			GetResourceManager()->RegisterResource(MakeRes(devices[i]));
			GetResourceManager()->AddLiveResource(physId, MakeRes(devices[i]));
		}

		ReplayData data;
		data.phys = pd;
		m_PhysicalReplayData.push_back(data);
	}

	return true;
}

VkResult WrappedVulkan::vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
{
	uint32_t count;
	VkPhysicalDevice devices[8]; // VKTODO: dynamically allocate
	VkResult ret = m_Real.vkEnumeratePhysicalDevices(instance, &count, devices);

	if(ret != VK_SUCCESS)
		return ret;
	
	for(uint32_t i=0; i < count; i++)
	{
		GetResourceManager()->RegisterResource(MakeRes(devices[i]));

		if(m_State >= WRITING)
		{
			SCOPED_SERIALISE_CONTEXT(ENUM_PHYSICALS);
			Serialise_vkEnumeratePhysicalDevices(instance, &count, &devices[i]);

			m_DeviceRecord->AddChunk(scope.Get());
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
		
		// VKTODO: find a queue that supports graphics/compute/dma, and if one doesn't exist, add it.

		// VKTODO: check that extensions supported in capture (from createInfo) are supported in replay

		VkResult ret = m_Real.vkCreateDevice(physicalDevice, &createInfo, &device);

		VkResource res = MakeRes(device);

		GetResourceManager()->RegisterResource(res);
		GetResourceManager()->AddLiveResource(devId, res);

		bool found = false;

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].phys == physicalDevice)
			{
				m_PhysicalReplayData[i].dev = device;
				// VKTODO: shouldn't be 0, 0
				m_Real.vkGetDeviceQueue(device, 0, 0, &m_PhysicalReplayData[i].q);

				// VKTODO queueFamilyIndex
				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, 0, 0 };
				m_Real.vkCreateCommandPool(device, &poolInfo, &m_PhysicalReplayData[i].cmdpool);

				VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, m_PhysicalReplayData[i].cmdpool, VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
				m_Real.vkCreateCommandBuffer(device, &cmdInfo, &m_PhysicalReplayData[i].cmd);
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

	// VKTODO: find a queue that supports all capabilities, and if one doesn't exist, add it.
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
				// VKTODO: shouldn't be 0, 0
				m_Real.vkGetDeviceQueue(*pDevice, 0, 0, &m_PhysicalReplayData[i].q);

				// VKTODO queueFamilyIndex
				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, 0, 0 };
				m_Real.vkCreateCommandPool(*pDevice, &poolInfo, &m_PhysicalReplayData[i].cmdpool);

				VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, m_PhysicalReplayData[i].cmdpool, VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
				m_Real.vkCreateCommandBuffer(*pDevice, &cmdInfo, &m_PhysicalReplayData[i].cmd);
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

			m_DeviceRecord->AddChunk(chunk);
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
	if(m_State >= WRITING)
	{
		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].dev == device)
			{
				if(i == (size_t)m_SwapPhysDevice)
				{
					if(m_DeviceRecord)
					{
						RDCASSERT(m_DeviceRecord->GetRefCount() == 1);
						m_DeviceRecord->Delete(GetResourceManager());
						m_DeviceRecord = NULL;
					}

					if(m_ContextRecord)
					{
						RDCASSERT(m_ContextRecord->GetRefCount() == 1);
						m_ContextRecord->Delete(GetResourceManager());
						m_ContextRecord = NULL;
					}

					m_ResourceManager->Shutdown();

					m_DeviceRecord = GetResourceManager()->AddResourceRecord(m_DeviceResourceID);
					m_DeviceRecord->DataInSerialiser = false;
					m_DeviceRecord->Length = 0;
					m_DeviceRecord->NumSubResources = 0;
					m_DeviceRecord->SpecialResource = true;
					m_DeviceRecord->SubResources = NULL;

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

	// VKTODO: serialise mem refs, on replay add/remove as necessary
	
	SERIALISE_ELEMENT(uint32_t, numCmds, cmdBufferCount);

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

		m_SubmittedFences.insert(fenceId);

		m_Real.vkQueueSubmit(queue, numCmds, cmds, fence);

		for(uint32_t i=0; i < numCmds; i++)
		{
			ResourceId cmd = GetResourceManager()->GetID(MakeRes(cmds[i]));
			GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
		}

		delete[] cmds;
	}
	
	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		AddEvent(QUEUE_SUBMIT, desc);
		string name = "vkQueueSubmit(" +
						ToStr::Get(numCmds) + ", ...)";

		FetchDrawcall draw;
		draw.name = name;
		draw.numIndices = 3;
		draw.numInstances = 1;
		draw.indexOffset = 0;
		draw.vertexOffset = 0;
		draw.instanceOffset = 0;

		draw.flags |= eDraw_Drawcall;

		AddDrawcall(draw, true);
	}

	return true;
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
			m_DeviceRecord->AddParent(record);
			m_DeviceRecord->AddParent(record->bakedCommands);
		}

		record->dirtied.clear();
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitIdle(VkQueue queue)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(queue)));
	
	if(m_State < WRITING)
	{
		m_Real.vkQueueWaitIdle((VkQueue)GetResourceManager()->GetLiveResource(id).handle);
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitIdle(VkQueue queue)
{
	VkResult ret = m_Real.vkQueueWaitIdle(queue);
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_IDLE);
		Serialise_vkQueueWaitIdle(queue);

		m_ContextRecord->AddChunk(scope.Get());
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

			m_DeviceRecord->AddChunk(chunk);
			
			// create resource record for gpu memory, although we won't use it for chunk tracking
			GetResourceManager()->AddResourceRecord(id);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, res);
		}

		m_MemoryInfo[id].size = pAllocInfo->allocationSize;
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(mem)));

	if(m_State == READING)
	{
		VkResource res = GetResourceManager()->GetLiveResource(id);
		device = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		mem = (VkDeviceMemory)res.handle;

		VkResult ret = m_Real.vkFreeMemory(device, mem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on freeing memory, VkResult: 0x%08x", ret);
		}
		
		ResourceId liveid = GetResourceManager()->GetLiveID(id);
		m_MemoryInfo.erase(liveid);
		GetResourceManager()->EraseLiveResource(id);
		GetResourceManager()->MarkCleanResource(liveid);
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(liveid);
		if(record)
			record->Delete(GetResourceManager());
		GetResourceManager()->UnregisterResource(res);
	}

	return true;
}

VkResult WrappedVulkan::vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              mem)
{
	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(FREE_MEM);
			Serialise_vkFreeMemory(device, mem);

			chunk = scope.Get();
		}

		m_DeviceRecord->AddChunk(chunk);
	}
	
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

		// VKTODO shouldn't track maps unless capframe, dirty otherwise
		if(m_State >= WRITING)
		{
			auto it = m_MemoryInfo.find(id);
			if(it == m_MemoryInfo.end())
			{
				RDCERR("vkMapMemory for unknown memory handle");
			}
			else
			{
				// VKTODO handle multiple maps
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

	// VKTODO: this is really horrible - this could be write-combined memory that we're
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
				if(ret == VK_SUCCESS)
				{
					SCOPED_SERIALISE_CONTEXT(UNMAP_MEM);
					Serialise_vkUnmapMemory(device, mem);

					if(m_State == WRITING_IDLE)
						m_DeviceRecord->AddChunk(scope.Get());
					else
						m_ContextRecord->AddChunk(scope.Get());
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
	if(record == NULL) record = m_DeviceRecord;

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_BUFFER_MEM);
			Serialise_vkBindBufferMemory(device, buffer, mem, memOffset);

			chunk = scope.Get();
		}

		record->AddChunk(chunk);
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
	if(record == NULL) record = m_DeviceRecord;

	if(m_State >= WRITING)
	{
		Chunk *chunk = NULL;

		{
			SCOPED_SERIALISE_CONTEXT(BIND_IMAGE_MEM);
			Serialise_vkBindImageMemory(device, image, mem, memOffset);

			chunk = scope.Get();
		}

		record->AddChunk(chunk);
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

			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->image));
			record->AddChunk(chunk);
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

			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(pCreateInfo->image));
			record->AddChunk(chunk);
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

			m_DeviceRecord->AddChunk(chunk);
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

			m_DeviceRecord->AddChunk(chunk);
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

			m_DeviceRecord->AddChunk(chunk);
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

			m_DeviceRecord->AddChunk(chunk);
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

			m_DeviceRecord->AddChunk(chunk);
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
	// VKTODO do I need to serialise this? just a driver hint..
	return vkResetCommandPool(device, cmdPool, flags);
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
	
	RDCASSERT(pCount == NULL || *pCount == count); // VKTODO: find out what *pCount < count means

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

				// VKTODO: add parent descriptor pool record
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, res);
			}
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
	
	if(ret == VK_SUCCESS && m_State >= WRITING)
	{
		for(uint32_t i=0; i < writeCount; i++)
		{
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(pDescriptorWrites[i].destSet));
			
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
				Serialise_vkUpdateDescriptorSets(device, 1, &pDescriptorWrites[i], 0, NULL);

				chunk = scope.Get();
			}

			record->AddChunk(chunk);
		}

		for(uint32_t i=0; i < copyCount; i++)
		{
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(pDescriptorCopies[i].destSet));
			
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
				Serialise_vkUpdateDescriptorSets(device, 0, NULL, 1, &pDescriptorCopies[i]);

				chunk = scope.Get();
			}

			record->AddChunk(chunk);
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
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResourceManager()->GetID(MakeRes(device)));
	m_pSerialiser->Serialise("createInfo", createInfo);

	if(m_State == READING)
	{
		// remove one-time submit flag as we will want to submit many
		info.flags &= ~VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT;

		VkCmdBuffer cmd = VK_NULL_HANDLE;

		if(!GetResourceManager()->HasLiveResource(bakeId))
		{
			VkResult ret = m_Real.vkCreateCommandBuffer((VkDevice)GetResourceManager()->GetLiveResource(devId).handle, &createInfo, &cmd);

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

		// VKTODO: shouldn't depend on 'id' being a uint64_t member
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

	if(m_State == READING)
	{
		VkCmdBuffer cmd = (VkCmdBuffer)GetResourceManager()->GetLiveResource(bakeId).handle;

		GetResourceManager()->RemoveReplacement(cmdId);

		m_Real.vkEndCommandBuffer(cmd);
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

	if(m_State == READING)
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

		// VKTODO: shouldn't depend on 'id' being a uint64_t member
		VkResource res(eResCmdBufferBake, bakedId.id);

		record->bakedCommands = GetResourceManager()->AddResourceRecord(res, bakedId);

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

	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdBeginRenderPass(cmdBuffer, &beginInfo, cont);
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
	}
}

bool WrappedVulkan::Serialise_vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));

	if(m_State < WRITING)
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

	if(m_State < WRITING)
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

	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		layout = (VkPipelineLayout)GetResourceManager()->GetLiveResource(layoutid).handle;

		m_Real.vkCmdBindDescriptorSets(cmdBuffer, bind, layout, first, numSets, sets, offsCount, offs);

		SAFE_DELETE_ARRAY(sets);
	}

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
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicViewportState)));

	if(m_State < WRITING)
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
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                        dynamicRasterState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicRasterState)));

	if(m_State < WRITING)
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
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicColorBlendState)));

	if(m_State < WRITING)
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
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResourceManager()->GetID(MakeRes(dynamicDepthStencilState)));

	if(m_State < WRITING)
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

	if(m_State < WRITING)
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
	}
}

bool WrappedVulkan::Serialise_vkCmdDraw(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstVertex,
	uint32_t       vertexCount,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, firstVtx, firstVertex);
	SERIALISE_ELEMENT(uint32_t, vtxCount, vertexCount);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State < WRITING)
	{
		VkCmdBuffer buf = (VkCmdBuffer)GetResourceManager()->GetLiveResource(id).handle;

		m_Real.vkCmdDraw(buf, firstVtx, vtxCount, firstInst, instCount);
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
	
	if(m_State < WRITING)
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
	
	if(m_State < WRITING)
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

		// VKTODO init states not implemented yet...
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
	
	if(m_State < WRITING)
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
	
	if(m_State < WRITING)
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

		// VKTODO: need to dirty the memory bound to the buffer?
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
	
	if(m_State < WRITING)
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
		
		// VKTODO: need to dirty the memory bound to the buffer?
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
	
	if(m_State < WRITING)
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
	
	if(m_State < WRITING)
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
	
	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;

		m_Real.vkCmdClearColorAttachment(cmdBuffer, att, layout, &col, count, rects);
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
	
	if(m_State < WRITING)
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
	
	if(m_State < WRITING)
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
	
	if(m_State == READING) // && !m_CurEvents.empty()
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

bool WrappedVulkan::ReleaseResource(VkResource res)
{
	// VKTODO: release resource with device from resource record

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

		m_pSerialiser->Serialise("callstack", stack, numLevels);

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

	m_ContextRecord->AddChunk(scope.Get(), 1);
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
		m_pSerialiser->SetOffset(ev.fileOffset);
	}
	else if(m_State == READING)
	{
		m_CurEventID = 1;
	}

	if(m_State == EXECUTING)
	{
	}

	GetResourceManager()->MarkInFrame(true);

	while(1)
	{
		if(m_State == EXECUTING && m_CurEventID > endEventID)
		{
			// we can just break out if we've done all the events desired.
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

		m_ParentDrawcall.children.clear();
	}

	GetResourceManager()->MarkInFrame(false);

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
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(uint32_t, firstIdx, firstIndex);
	SERIALISE_ELEMENT(uint32_t, idxCount, indexCount);
	SERIALISE_ELEMENT(int32_t, vtxOffs, vertexOffset);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State < WRITING)
	{
		VkCmdBuffer buf = (VkCmdBuffer)GetResourceManager()->GetLiveResource(id).handle;

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

	if(m_State < WRITING)
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

	if(m_State < WRITING)
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

	if(m_State < WRITING)
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

	if(m_State < WRITING)
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


#if 0

bool WrappedVulkan::Serialise_vkCmdUpdateMemory(
	VkCmdBuffer    cmdBuffer,
	VkDeviceMemory    destMem,
	VkDeviceSize      destOffset,
	VkDeviceSize      dataSize,
	const uint32_t32* pData)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, memid, GetResourceManager()->GetID(MakeRes(destMem)));
	SERIALISE_ELEMENT(uint64_t, offs, destOffset);
	SERIALISE_ELEMENT(uint64_t, size, dataSize);
	SERIALISE_ELEMENT_BUF(byte *, data, (byte *)pData, (size_t)size);

	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		destMem = (VkDeviceMemory)GetResourceManager()->GetLiveResource(memid).handle;

		m_Real.vkCmdUpdateMemory(cmdBuffer, destMem, offs, size, (uint32_t32*)data);

		SAFE_DELETE_ARRAY(data);
	}

	return true;
}

void WrappedVulkan::vkCmdUpdateMemory(
	VkCmdBuffer    cmdBuffer,
	VkDeviceMemory    destMem,
	VkDeviceSize      destOffset,
	VkDeviceSize      dataSize,
	const uint32_t32* pData)
{
	m_Real.vkCmdUpdateMemory(cmdBuffer, destMem, destOffset, dataSize, pData);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(UPDATE_MEM);
		Serialise_vkCmdUpdateMemory(cmdBuffer, destMem, destOffset, dataSize, pData);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdFillMemory(
	VkCmdBuffer cmdBuffer,
    VkDeviceMemory destMem,
    VkDeviceSize   destOffset,
    VkDeviceSize   fillSize,
    uint32_t32     data)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, memid, GetResourceManager()->GetID(MakeRes(destMem)));
	SERIALISE_ELEMENT(uint64_t, offs, destOffset);
	SERIALISE_ELEMENT(uint64_t, size, fillSize);
	SERIALISE_ELEMENT(uint32_t, val, data);
	
	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		destMem = (VkDeviceMemory)GetResourceManager()->GetLiveResource(memid).handle;

		m_Real.vkCmdFillMemory(cmdBuffer, destMem, (VkDeviceSize)offs, (VkDeviceSize)size, (uint32_t32)val);
	}

	return true;
}

void WrappedVulkan::vkCmdFillMemory(
	VkCmdBuffer cmdBuffer,
    VkDeviceMemory destMem,
    VkDeviceSize   destOffset,
    VkDeviceSize   fillSize,
    uint32_t32     data)
{
	m_Real.vkCmdFillMemory(cmdBuffer, destMem, destOffset, fillSize, data);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(FILL_MEM);
		Serialise_vkCmdFillMemory(cmdBuffer, destMem, destOffset, fillSize, data);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdResolveImage(
	VkCmdBuffer           cmdBuffer,
	VkImage                srcImage,
	VkImage                destImage,
	uint32_t                 rectCount,
	const VkImageResolve* pRects)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResourceManager()->GetID(MakeRes(srcImage)));
	SERIALISE_ELEMENT(ResourceId, dstid, GetResourceManager()->GetID(MakeRes(destImage)));
	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkImageResolve, rects, pRects, count);
	
	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		srcImage = (VkImage)GetResourceManager()->GetLiveResource(srcid).handle;
		destImage = (VkImage)GetResourceManager()->GetLiveResource(dstid).handle;

		m_Real.vkCmdResolveImage(cmdBuffer, srcImage, destImage, count, rects);
	}

	SAFE_DELETE_ARRAY(rects);

	return true;
}

void WrappedVulkan::vkCmdResolveImage(
	VkCmdBuffer           cmdBuffer,
	VkImage                srcImage,
	VkImage                destImage,
	uint32_t                 rectCount,
	const VkImageResolve* pRects)
{
	m_Real.vkCmdResolveImage(cmdBuffer, srcImage, destImage, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(RESOLVE_IMAGE);
		Serialise_vkCmdResolveImage(cmdBuffer, srcImage, destImage, rectCount, pRects);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdWriteTimestamp(
	VkCmdBuffer cmdBuffer,
	VkTimestampType       timestampType,
	VkDeviceMemory destMem,
	VkDeviceSize   destOffset)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResourceManager()->GetID(MakeRes(cmdBuffer)));
	SERIALISE_ELEMENT(ResourceId, memid, GetResourceManager()->GetID(MakeRes(destMem)));
	SERIALISE_ELEMENT(VkTimestampType, type, timestampType);
	SERIALISE_ELEMENT(uint64_t, offs, destOffset);
	
	if(m_State < WRITING)
	{
		cmdBuffer = (VkCmdBuffer)GetResourceManager()->GetLiveResource(cmdid).handle;
		destMem = (VkDeviceMemory)GetResourceManager()->GetLiveResource(memid).handle;

		m_Real.vkCmdWriteTimestamp(cmdBuffer, type, destMem, offs);
	}

	return true;
}

void WrappedVulkan::vkCmdWriteTimestamp(
	VkCmdBuffer cmdBuffer,
	VkTimestampType       timestampType,
	VkDeviceMemory destMem,
	VkDeviceSize   destOffset)
{
	m_Real.vkCmdWriteTimestamp(cmdBuffer, timestampType, destMem, destOffset);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(MakeRes(cmdBuffer));

		SCOPED_SERIALISE_CONTEXT(WRITE_TIMESTAMP);
		Serialise_vkCmdWriteTimestamp(cmdBuffer, timestampType, destMem, destOffset);

		record->AddChunk(scope.Get());
	}
}

#endif

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
		// VKTODO what if num images is less than on capture?
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

	// VKTODO: intercept VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI and wrap/serialise the images
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
	// VKTODO: does this need to be intercepted/serialised?
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

	VkFormat fmt = VK_FORMAT_UNDEFINED;
	uint32_t numIms = 0;

	if(m_State >= WRITING)
	{
		VkResult res = VK_SUCCESS;

    size_t swapChainImagesSize;
    res = m_Real.vkGetSwapChainInfoWSI(device, *pSwapChain, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
    RDCASSERT(res == VK_SUCCESS);

		numIms = uint32_t(swapChainImagesSize/sizeof(VkSwapChainImagePropertiesWSI));

		fmt = VK_FORMAT_B8G8R8A8_UNORM;

		// VKTODO: need to get proper formats - needs surface description
		/*
    size_t formatsSize;
    res = m_Real.vkGetSurfaceInfoWSI(device, (VkSurfaceDescriptionWSI *), VK_SURFACE_INFO_TYPE_FORMATS_WSI, &formatsSize, NULL);
    RDCASSERT(res == VK_SUCCESS);
    VkSurfaceFormatPropertiesWSI *surfFormats = (VkSurfaceFormatPropertiesWSI *)malloc(formatsSize);
    res = m_Real.vkGetSurfaceInfoWSI(demo->device, (VkSurfaceDescriptionWSI *) , VK_SURFACE_INFO_TYPE_FORMATS_WSI, &formatsSize, surfFormats);
    RDCASSERT(res == VK_SUCCESS);
		*/
	}

	SERIALISE_ELEMENT(VkFormat, imFormat, fmt);
	SERIALISE_ELEMENT(uint32_t, numSwapImages, numIms);

	m_SwapChainInfo[id].format = imFormat;
	m_SwapChainInfo[id].extent = info.imageExtent;
	m_SwapChainInfo[id].arraySize = info.imageArraySize;

	m_SwapChainInfo[id].images.resize(numSwapImages);

	if(m_State == READING)
	{
		RDCASSERT(imFormat == info.imageFormat);

		VkDevice dev = (VkDevice)GetResourceManager()->GetLiveResource(devId).handle;
		
    const VkImageCreateInfo imInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = imFormat,
        .extent = { info.imageExtent.width, info.imageExtent.height, 1 },
        .mipLevels = 1,
        .arraySize = info.imageArraySize,
        .samples = 1,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
					VK_IMAGE_USAGE_TRANSFER_SOURCE_BIT|
					VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT|
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
					VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        .flags = 0,
    };

		for(uint32_t i=0; i < numSwapImages; i++)
		{
			VkDeviceMemory mem = VK_NULL_HANDLE;
			VkImage im = VK_NULL_HANDLE;

			VkResult res = m_Real.vkCreateImage(dev, &imInfo, &im);
			RDCASSERT(res == VK_SUCCESS);
			
			VkMemoryRequirements mrq = {0};

			res = m_Real.vkGetImageMemoryRequirements(dev, im, &mrq);
			RDCASSERT(res == VK_SUCCESS);
			
			VkMemoryAllocInfo allocInfo = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO,
				.pNext = NULL,
				.allocationSize = mrq.size,
				.memoryTypeIndex = 0, // VKTODO find appropriate memory type index
			};

			res = m_Real.vkAllocMemory(dev, &allocInfo, &mem);
			RDCASSERT(res == VK_SUCCESS);

			res = m_Real.vkBindImageMemory(dev, im, mem, 0);
			RDCASSERT(res == VK_SUCCESS);

			GetResourceManager()->RegisterResource(MakeRes(mem));
			GetResourceManager()->RegisterResource(MakeRes(im));

			// image live ID will be assigned separately in Serialise_vkGetSwapChainInfoWSI
			// memory doesn't have a live ID

			m_SwapChainInfo[id].images[i].mem = mem;
			m_SwapChainInfo[id].images[i].im = im;

			// fill out image info so we track resource state transitions
			m_ImageInfo[id].format = imFormat;
			m_ImageInfo[id].extent.width = info.imageExtent.width;
			m_ImageInfo[id].extent.height = info.imageExtent.height;
			m_ImageInfo[id].extent.depth = 1;
			m_ImageInfo[id].mipLevels = 1;
			m_ImageInfo[id].arraySize = info.imageArraySize;

			VkImageSubresourceRange range;
			range.baseMipLevel = range.baseArraySlice = 0;
			range.mipLevels = 1;
			range.arraySize = info.imageArraySize;
			range.aspect = VK_IMAGE_ASPECT_COLOR;

			m_ImageInfo[id].subresourceStates.clear();
			m_ImageInfo[id].subresourceStates.push_back(ImageRegionState(range, UNTRANSITIONED_IMG_STATE, VK_IMAGE_LAYOUT_UNDEFINED));
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

			// VKTODO
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
			}

			{
				RDCDEBUG("Getting Resource Record");	

				VkResourceRecord *record = GetResourceManager()->GetResourceRecord(m_ContextResourceID);

				RDCDEBUG("Accumulating context resource list");	

				map<int32_t, Chunk *> recordlist;
				record->Insert(recordlist);

				RDCDEBUG("Flushing %u records to file serialiser", (uint32_t)recordlist.size());	

				for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
					m_pFileSerialiser->Insert(it->second);

				RDCDEBUG("Done");	
			}

			m_CurFileSize += m_pFileSerialiser->FlushToDisk();

			RenderDoc::Inst().SuccessfullyWrittenLog();

			SAFE_DELETE(m_pFileSerialiser);

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

		GetResourceManager()->MarkResourceFrameReferenced(m_DeviceResourceID, eFrameRef_Write);
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
	
	RDCUNIMPLEMENTED("initial states not implemented");

	if(res.Namespace == eResDescriptorSet)
	{
		// VKTODO: need to figure out the format/serialisation of these
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

		// VKTODO: need to copy off memory somewhere else

		return true;
	}
	else if(res.Namespace == eResImage)
	{
		if(m_ImageInfo.find(id) == m_ImageInfo.end())
		{
			RDCERR("Couldn't find image info");
			return false;
		}

		// VKTODO: need to copy off contents to memory somewhere else

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
	
	RDCUNIMPLEMENTED("initial states not implemented");

	if(m_State >= WRITING)
	{
		VulkanResourceManager::InitialContentData initContents = GetResourceManager()->GetInitialContents(id);

		if(res.Namespace == eResDescriptorSet)
		{
			// VKTODO: need to figure out the format/serialisation of these
		}
		else if(res.Namespace == eResImage || res.Namespace == eResDeviceMemory)
		{
			// VKTODO: need to upload the buffer to memory
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
			
			// VKTODO: need to copy buffer to memory
		}
	}

	return true;
}

void WrappedVulkan::Create_InitialState(ResourceId id, VkResource live, bool hasData)
{
	RDCUNIMPLEMENTED("initial states not implemented");

	if(live.Namespace == eResDescriptorSet)
	{
		RDCERR("Unexpected attempt to create initial state for descriptor set");
	}
	else if(live.Namespace == eResImage)
	{
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
	else
	{
		RDCERR("Unhandled resource type %d", live.Namespace);
	}
}

void WrappedVulkan::Apply_InitialState(VkResource live, VulkanResourceManager::InitialContentData initial)
{
	RDCUNIMPLEMENTED("initial states not implemented");

	if(live.Namespace == eResDescriptorSet)
	{
		// VKTODO: need to figure out the format/serialisation of these
	}
	else if(live.Namespace == eResDeviceMemory)
	{
		// VKTODO: need to copy initial copy to live
	}
	else if(live.Namespace == eResImage)
	{
		// VKTODO: need to copy initial copy to live
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
	case CREATE_INSTANCE:
		Serialise_vkCreateInstance(NULL, NULL);
		break;
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
		Serialise_vkFreeMemory(VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
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
		//VKTODO:
		//Serialise_vkCreateComputePipelines(VK_NULL_HANDLE, NULL, NULL);
		break;
	case PRESENT_IMAGE:
		Serialise_vkGetSwapChainInfoWSI(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SWAP_CHAIN_INFO_TYPE_MAX_ENUM_WSI, NULL, NULL);
		break;

	case CREATE_FENCE:
		//VKTODO:
		//Serialise_vkCreateFence(VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_FENCE_STATUS:
		//VKTODO:
		//Serialise_vkGetFenceStatus(VK_NULL_HANDLE);
		break;
	case WAIT_FENCES:
		//VKTODO:
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
		//VKTODO:
		//Serialise_vkCmdResolveImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
		break;
	case WRITE_TIMESTAMP:
		//VKTODO:
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
			m_FakeBBMem = m_ImageInfo[liveBBid].mem;

			bool HasCallstack = false;
			m_pSerialiser->Serialise("HasCallstack", HasCallstack);	

			if(HasCallstack)
			{
				size_t numLevels = 0;
				uint64_t *stack = NULL;

				m_pSerialiser->Serialise("callstack", stack, numLevels);

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

void WrappedVulkan::Initialise(VkInitParams &params)
{
	// maybe instance params should be fetched here?
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

	if(m_State == READING)
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
