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
#include "vk_debug.h"

#include "serialise/string_utils.h"

#include "maths/formatpacking.h"

#include "jpeg-compressor/jpge.h"

device_table_map renderdoc_device_table_map;
instance_table_map renderdoc_instance_table_map;

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

	VkInstance inst = {0};

	VkResult ret = dummyInstanceTable->CreateInstance(&instinfo, &inst);

	GetResourceManager()->WrapResource(inst, inst);
	GetResourceManager()->AddLiveResource(params.InstanceID, inst);

	SAFE_DELETE_ARRAY(layerscstr);
	SAFE_DELETE_ARRAY(extscstr);
}

WrappedVulkan::WrappedVulkan(const char *logFilename)
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
	vkGetGlobalExtensionProperties(NULL, &extCount, NULL);

	globalExts.driver.resize(extCount);
	vkGetGlobalExtensionProperties(NULL, &extCount, &globalExts.driver[0]);

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

	m_HeaderChunk = NULL;

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
		m_FrameCaptureRecord->DataInSerialiser = false;
		m_FrameCaptureRecord->Length = 0;
		m_FrameCaptureRecord->NumSubResources = 0;
		m_FrameCaptureRecord->SpecialResource = true;
		m_FrameCaptureRecord->SubResources = NULL;
	}
	else
	{
		m_FrameCaptureRecord = NULL;

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
		// GSFTODO Fix this
		// m_Real.vkDbgDestroyMsgCallback(m_PhysicalReplayData[0].inst, m_MsgCallback);
		ObjDisp(m_PhysicalReplayData[0].inst)->DbgDestroyMsgCallback(Unwrap(m_PhysicalReplayData[0].inst), m_MsgCallback);
	}
#endif

	// VKTODOLOW should only have one swapchain, since we are only handling the simple case
	// of one device, etc for now.
	RDCASSERT(m_SwapChainInfo.size() == 1);
	for(auto it = m_SwapChainInfo.begin(); it != m_SwapChainInfo.end(); ++it)
	{
		for(size_t i=0; i < it->second.images.size(); i++)
		{
			// only in the replay app are these 'real' images to be destroyed
			if(RenderDoc::Inst().IsReplayApp())
			{
				// go through our wrapped functions, since the resources need to be deregistered
				vkDestroyImage(GetDev(), it->second.images[i].im);
				vkFreeMemory(GetDev(), it->second.images[i].mem);
			}
			
			// VKTODOHIGH this device has been destroyed already - need to kill these when
			// swapchain is destroyed?
			//if(it->second.images[i].fb != VK_NULL_HANDLE)
				//ObjDisp(GetDev())->DestroyFramebuffer(Unwrap(GetDev()), it->second.images[i].fb);
			
			//if(it->second.images[i].view != VK_NULL_HANDLE)
				//ObjDisp(GetDev())->DestroyAttachmentView(Unwrap(GetDev()), it->second.images[i].view);
		}

		//if(it->second.rp != VK_NULL_HANDLE)
			//ObjDisp(GetDev())->DestroyRenderPass(Unwrap(GetDev()), it->second.rp);
		//if(it->second.vp != VK_NULL_HANDLE)
			//ObjDisp(GetDev())->DestroyDynamicViewportState(Unwrap(GetDev()), it->second.vp);
	}
	m_SwapChainInfo.clear();

	m_ResourceManager->Shutdown();
	delete m_ResourceManager;
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

	RDCASSERT(pCreateInfo->pAppInfo == NULL || pCreateInfo->pAppInfo->pNext == NULL);
	RDCASSERT(pCreateInfo->pNext == NULL);

	VkInstance inst = *pInstance;

	VkResult ret = get_dispatch_table(renderdoc_instance_table_map, *pInstance)->CreateInstance(pCreateInfo, &inst);

	GetResourceManager()->WrapResource(inst, inst);

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
	PFN_vkDbgCreateMsgCallback dcmc_fn = ObjDisp(inst)->DbgCreateMsgCallback;
	if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode && dcmc_fn)
	{
		VkFlags flags = VK_DBG_REPORT_INFO_BIT |
										VK_DBG_REPORT_WARN_BIT |
										VK_DBG_REPORT_PERF_WARN_BIT |
										VK_DBG_REPORT_ERROR_BIT |
										VK_DBG_REPORT_DEBUG_BIT;
		dcmc_fn(Unwrap(inst), flags, &DebugCallbackStatic, this, &m_MsgCallback);
	}

	if(m_State >= WRITING)
	{
		m_InitParams.Set(pCreateInfo, GetResID(inst));
		m_InstanceRecord = GetResourceManager()->AddResourceRecord(inst);
	}

	*pInstance = inst;

	return VK_SUCCESS;
}

VkResult WrappedVulkan::vkDestroyInstance(
		VkInstance                                  instance)
{
	dispatch_key key = get_dispatch_key(instance);
	VkResult ret = ObjDisp(instance)->DestroyInstance(Unwrap(instance));

	if(ret != VK_SUCCESS)
		return ret;
	
	if(RenderDoc::Inst().GetCaptureOptions().DebugDeviceMode && m_MsgCallback != VK_NULL_HANDLE)
	{
		ObjDisp(instance)->DbgDestroyMsgCallback(Unwrap(instance), m_MsgCallback);
	}

	GetResourceManager()->ReleaseWrappedResource(instance);

	destroy_dispatch_table(renderdoc_instance_table_map, key);

	return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkEnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
{
	SERIALISE_ELEMENT(ResourceId, inst, GetResID(instance));
	SERIALISE_ELEMENT(uint32_t, physIndex, *pPhysicalDeviceCount);
	SERIALISE_ELEMENT(ResourceId, physId, GetResID(*pPhysicalDevices));

	VkPhysicalDevice pd = VK_NULL_HANDLE;

	if(m_State >= WRITING)
	{
		pd = *pPhysicalDevices;
	}
	else
	{
		uint32_t count;
		VkPhysicalDevice *devices;

		instance = GetResourceManager()->GetLiveHandle<VkInstance>(inst);
		VkResult vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		RDCASSERT(count > physIndex);
		devices = new VkPhysicalDevice[count];

		vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOLOW match up physical devices to those available on replay

		pd = devices[physIndex];

		SAFE_DELETE_ARRAY(devices);

		GetResourceManager()->WrapResource(inst, pd);
		GetResourceManager()->AddLiveResource(physId, pd);
	}

	ReplayData data;
	data.inst = instance;
	data.phys = pd;

	ObjDisp(pd)->GetPhysicalDeviceMemoryProperties(Unwrap(pd), &data.memProps);

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

	VkResult vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, NULL);

	if(vkr != VK_SUCCESS)
		return vkr;

	VkPhysicalDevice *devices = new VkPhysicalDevice[count];

	vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
	RDCASSERT(vkr == VK_SUCCESS);
	
	for(uint32_t i=0; i < count; i++)
	{
		// it's perfectly valid for enumerate type functions to return the same handle
		// each time. If that happens, we will already have a wrapper created so just
		// return the wrapped object to the user and do nothing else
		if(GetResourceManager()->HasWrapper(RealVkRes((void *)devices[i])))
		{
			devices[i] = (VkPhysicalDevice)GetResourceManager()->GetWrapper(RealVkRes((void *)devices[i]));
		}
		else
		{
			GetResourceManager()->WrapResource(instance, devices[i]);

			if(m_State >= WRITING)
			{
				SCOPED_SERIALISE_CONTEXT(ENUM_PHYSICALS);
				Serialise_vkEnumeratePhysicalDevices(instance, &i, &devices[i]);

				m_InstanceRecord->AddChunk(scope.Get());
			}
		}
	}

	if(pPhysicalDeviceCount) *pPhysicalDeviceCount = count;
	if(pPhysicalDevices) memcpy(pPhysicalDevices, devices, count*sizeof(VkPhysicalDevice));

	SAFE_DELETE_ARRAY(devices);

	return VK_SUCCESS;
}

bool WrappedVulkan::Serialise_vkCreateDevice(
		VkPhysicalDevice                            physicalDevice,
		const VkDeviceCreateInfo*                   pCreateInfo,
		VkDevice*                                   pDevice)
{
	SERIALISE_ELEMENT(ResourceId, physId, GetResID(physicalDevice));
	SERIALISE_ELEMENT(VkDeviceCreateInfo, createInfo, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(*pDevice));

	if(m_State == READING)
	{
		physicalDevice = GetResourceManager()->GetLiveHandle<VkPhysicalDevice>(physId);

		VkDevice device;

		uint32_t qCount = 0;
		VkResult vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueCount(Unwrap(physicalDevice), &qCount);
		RDCASSERT(vkr == VK_SUCCESS);

		VkPhysicalDeviceQueueProperties *props = new VkPhysicalDeviceQueueProperties[qCount];
		vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueProperties(Unwrap(physicalDevice), qCount, props);
		RDCASSERT(vkr == VK_SUCCESS);

		bool found = false;
		uint32_t qFamilyIdx = 0;
		VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_DMA_BIT);

		// if we need to change the requested queues, it will point to this
		VkDeviceQueueCreateInfo *modQueues = NULL;

		for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
		{
			uint32_t idx = createInfo.pRequestedQueues[i].queueFamilyIndex;
			RDCASSERT(idx < qCount);

			// this requested queue is one we can use too
			if((props[idx].queueFlags & search) == search && createInfo.pRequestedQueues[i].queueCount > 0)
			{
				qFamilyIdx = idx;
				found = true;
				break;
			}
		}

		// if we didn't find it, search for which queue family we should add a request for
		if(!found)
		{
			RDCDEBUG("App didn't request a queue family we can use - adding our own");

			for(uint32_t i=0; i < qCount; i++)
			{
				if((props[i].queueFlags & search) == search)
				{
					qFamilyIdx = i;
					found = true;
					break;
				}
			}

			if(!found)
			{
				SAFE_DELETE_ARRAY(props);
				RDCERR("Can't add a queue with required properties for RenderDoc! Unsupported configuration");
			}
			else
			{
				// we found the queue family, add it
				modQueues = new VkDeviceQueueCreateInfo[createInfo.queueRecordCount + 1];
				for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
					modQueues[i] = createInfo.pRequestedQueues[i];

				modQueues[createInfo.queueRecordCount].queueFamilyIndex = qFamilyIdx;
				modQueues[createInfo.queueRecordCount].queueCount = 1;

				createInfo.pRequestedQueues = modQueues;
				createInfo.queueRecordCount++;
			}
		}
		
		SAFE_DELETE_ARRAY(props);

		// VKTODOLOW: check that extensions and layers supported in capture (from createInfo) are supported in replay

		VkResult ret = ObjDisp(*pDevice)->CreateDevice(Unwrap(physicalDevice), &createInfo, &device);

		GetResourceManager()->WrapResource(device, device);
		GetResourceManager()->AddLiveResource(devId, device);

		found = false;

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].phys == physicalDevice)
			{
				// fill out replay functions. Maybe this should be somewhere else.
				// VKTODOLOW this won't work with multiple devices - will need a replay device table for each
				{
					RDCASSERT(dummyDeviceTable);

#define FETCH_DEVICE_FUNCPTR(func) dummyDeviceTable->func = (CONCAT(PFN_vk, func))dummyDeviceTable->GetDeviceProcAddr(device, STRINGIZE(CONCAT(vk, func)));
					FETCH_DEVICE_FUNCPTR(CreateSwapChainWSI)
					FETCH_DEVICE_FUNCPTR(DestroySwapChainWSI)
					FETCH_DEVICE_FUNCPTR(GetSurfaceInfoWSI)
					FETCH_DEVICE_FUNCPTR(GetSwapChainInfoWSI)
					FETCH_DEVICE_FUNCPTR(AcquireNextImageWSI)
					FETCH_DEVICE_FUNCPTR(QueuePresentWSI)
#undef FETCH_DEVICE_FUNCPTR
				}

				// VKTODOLOW not handling multiple devices per physical devices
				RDCASSERT(m_PhysicalReplayData[i].dev == VK_NULL_HANDLE);
				m_PhysicalReplayData[i].dev = device;

				m_PhysicalReplayData[i].qFamilyIdx = qFamilyIdx;

				VkResult vkr = ObjDisp(*pDevice)->GetDeviceQueue(Unwrap(device), qFamilyIdx, 0, &m_PhysicalReplayData[i].q);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, qFamilyIdx, VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
				vkr = ObjDisp(*pDevice)->CreateCommandPool(Unwrap(device), &poolInfo, &m_PhysicalReplayData[i].cmdpool);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, m_PhysicalReplayData[i].cmdpool, VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
				vkr = ObjDisp(*pDevice)->CreateCommandBuffer(Unwrap(device), &cmdInfo, &m_PhysicalReplayData[i].cmd);
				RDCASSERT(vkr == VK_SUCCESS);

#if defined(FORCE_VALIDATION_LAYER)
				if(ObjDisp(*pDevice)->DbgCreateMsgCallback)
				{
					VkFlags flags = VK_DBG_REPORT_INFO_BIT |
						VK_DBG_REPORT_WARN_BIT |
						VK_DBG_REPORT_PERF_WARN_BIT |
						VK_DBG_REPORT_ERROR_BIT |
						VK_DBG_REPORT_DEBUG_BIT;
					vkr = ObjDisp(*pDevice)->DbgCreateMsgCallback(Unwrap(m_PhysicalReplayData[i].inst), flags, &DebugCallbackStatic, this, &m_MsgCallback);
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

		SAFE_DELETE_ARRAY(modQueues);

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

	uint32_t qCount = 0;
	VkResult vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueCount(Unwrap(physicalDevice), &qCount);
	RDCASSERT(vkr == VK_SUCCESS);

	VkPhysicalDeviceQueueProperties *props = new VkPhysicalDeviceQueueProperties[qCount];
	vkr = ObjDisp(physicalDevice)->GetPhysicalDeviceQueueProperties(Unwrap(physicalDevice), qCount, props);
	RDCASSERT(vkr == VK_SUCCESS);

	// find a queue that supports all capabilities, and if one doesn't exist, add it.
	bool found = false;
	uint32_t qFamilyIdx = 0;
	VkQueueFlags search = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_DMA_BIT);

	// if we need to change the requested queues, it will point to this
	VkDeviceQueueCreateInfo *modQueues = NULL;

	for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
	{
		uint32_t idx = createInfo.pRequestedQueues[i].queueFamilyIndex;
		RDCASSERT(idx < qCount);

		// this requested queue is one we can use too
		if((props[idx].queueFlags & search) == search && createInfo.pRequestedQueues[i].queueCount > 0)
		{
			qFamilyIdx = idx;
			found = true;
			break;
		}
	}

	// if we didn't find it, search for which queue family we should add a request for
	if(!found)
	{
		RDCDEBUG("App didn't request a queue family we can use - adding our own");

		for(uint32_t i=0; i < qCount; i++)
		{
			if((props[i].queueFlags & search) == search)
			{
				qFamilyIdx = i;
				found = true;
				break;
			}
		}

		if(!found)
		{
			SAFE_DELETE_ARRAY(props);
			RDCERR("Can't add a queue with required properties for RenderDoc! Unsupported configuration");
			return VK_UNSUPPORTED;
		}

		// we found the queue family, add it
		modQueues = new VkDeviceQueueCreateInfo[createInfo.queueRecordCount + 1];
		for(uint32_t i=0; i < createInfo.queueRecordCount; i++)
			modQueues[i] = createInfo.pRequestedQueues[i];

		modQueues[createInfo.queueRecordCount].queueFamilyIndex = qFamilyIdx;
		modQueues[createInfo.queueRecordCount].queueCount = 1;

		createInfo.pRequestedQueues = modQueues;
		createInfo.queueRecordCount++;
	}

	SAFE_DELETE_ARRAY(props);

	RDCDEBUG("Might want to fiddle with createinfo - e.g. to remove VK_RenderDoc from set of extensions or similar");

	VkResult ret = get_dispatch_table(renderdoc_device_table_map, *pDevice)->CreateDevice(Unwrap(physicalDevice), &createInfo, pDevice);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(*pDevice, *pDevice);
		
		found = false;

		for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		{
			if(m_PhysicalReplayData[i].phys == physicalDevice)
			{
				m_PhysicalReplayData[i].dev = *pDevice;

				m_PhysicalReplayData[i].qFamilyIdx = qFamilyIdx;

				// we call our own vkGetDeviceQueue so that its initialisation is properly serialised in case when
				// the application fetches this queue it gets the same handle - the already wrapped one created
				// here will be returned.
				vkr = vkGetDeviceQueue(*pDevice, qFamilyIdx, 0, &m_PhysicalReplayData[i].q);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO, NULL, qFamilyIdx, VK_CMD_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
				vkr = ObjDisp(*pDevice)->CreateCommandPool(Unwrap(*pDevice), &poolInfo, &m_PhysicalReplayData[i].cmdpool);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(*pDevice), m_PhysicalReplayData[i].cmdpool);

				VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, Unwrap(m_PhysicalReplayData[i].cmdpool), VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
				vkr = ObjDisp(*pDevice)->CreateCommandBuffer(Unwrap(*pDevice), &cmdInfo, &m_PhysicalReplayData[i].cmd);
				RDCASSERT(vkr == VK_SUCCESS);
				found = true;

				GetResourceManager()->WrapResource(Unwrap(*pDevice), m_PhysicalReplayData[i].cmd);

				// VKTODOHIGH hack, need to properly handle multiple devices etc and
				// not have this 'current swap chain device' thing.
				m_SwapPhysDevice = (int)i;
				
				m_PhysicalReplayData[i].debugMan = new VulkanDebugManager(this, *pDevice, VK_NULL_HANDLE);
				break;
			}
		}

		if(!found)
			RDCERR("Couldn't find VkPhysicalDevice for vkCreateDevice!");

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
			GetResourceManager()->AddLiveResource(id, *pDevice);
		}
	}

	SAFE_DELETE_ARRAY(modQueues);

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

					if(m_FrameCaptureRecord)
					{
						RDCASSERT(m_FrameCaptureRecord->GetRefCount() == 1);
						m_FrameCaptureRecord->Delete(GetResourceManager());
						m_FrameCaptureRecord = NULL;
					}

					m_ResourceManager->Shutdown();

					m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
					m_FrameCaptureRecord->DataInSerialiser = false;
					m_FrameCaptureRecord->Length = 0;
					m_FrameCaptureRecord->NumSubResources = 0;
					m_FrameCaptureRecord->SpecialResource = true;
					m_FrameCaptureRecord->SubResources = NULL;
				}
				
				if(m_PhysicalReplayData[i].cmd != VK_NULL_HANDLE)
					ObjDisp(device)->DestroyCommandBuffer(Unwrap(device), Unwrap(m_PhysicalReplayData[i].cmd));

				if(m_PhysicalReplayData[i].cmdpool != VK_NULL_HANDLE)
					ObjDisp(device)->DestroyCommandPool(Unwrap(device), Unwrap(m_PhysicalReplayData[i].cmdpool));

				// VKTODOHIGH this data is needed in destructor for swapchains - order of shutdown needs to be revamped
				break;
			}
		}
	}

	dispatch_key key = get_dispatch_key(device);
	VkResult ret = ObjDisp(device)->DestroyDevice(device);
	destroy_dispatch_table(renderdoc_device_table_map, key);

	GetResourceManager()->ReleaseWrappedResource(device);

	return ret;
}

VkResult WrappedVulkan::vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceFeatures(Unwrap(physicalDevice), pFeatures);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceFormatProperties(Unwrap(physicalDevice), format, pFormatProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageFormatProperties*                    pImageFormatProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceImageFormatProperties(Unwrap(physicalDevice), format, type, tiling, usage, pImageFormatProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceLimits(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceLimits*                     pLimits)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceLimits(Unwrap(physicalDevice), pLimits);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceProperties(Unwrap(physicalDevice), pProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueCount(
    VkPhysicalDevice                            physicalDevice,
    uint32_t*                                   pCount)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceQueueCount(Unwrap(physicalDevice), pCount);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceQueueProperties(
    VkPhysicalDevice                            physicalDevice,
    uint32_t                                    count,
    VkPhysicalDeviceQueueProperties*            pQueueProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceQueueProperties(Unwrap(physicalDevice), count, pQueueProperties);
}

VkResult WrappedVulkan::vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
	return ObjDisp(physicalDevice)->GetPhysicalDeviceMemoryProperties(Unwrap(physicalDevice), pMemoryProperties);
}

VkResult WrappedVulkan::vkGetImageSubresourceLayout(
			VkDevice                                    device,
			VkImage                                     image,
			const VkImageSubresource*                   pSubresource,
			VkSubresourceLayout*                        pLayout)
{
	return ObjDisp(device)->GetImageSubresourceLayout(Unwrap(device), Unwrap(image), pSubresource, pLayout);
}

VkResult WrappedVulkan::vkGetBufferMemoryRequirements(
		VkDevice                                    device,
		VkBuffer                                    buffer,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	return ObjDisp(device)->GetBufferMemoryRequirements(Unwrap(device), Unwrap(buffer), pMemoryRequirements);
}

VkResult WrappedVulkan::vkGetImageMemoryRequirements(
		VkDevice                                    device,
		VkImage                                     image,
		VkMemoryRequirements*                       pMemoryRequirements)
{
	return ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(image), pMemoryRequirements);
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

	return util_GetExtensionProperties(0, NULL, pCount, pProperties);
}

#define DESTROY_IMPL(type, func) \
	VkResult WrappedVulkan::vk ## func(VkDevice device, type obj) \
	{ \
		if(m_ImageInfo.find(GetResID(obj)) != m_ImageInfo.end()) m_ImageInfo.erase(GetResID(obj)); \
		VkResult ret = ObjDisp(device)->func(Unwrap(device), Unwrap(obj)); \
		GetResourceManager()->ReleaseWrappedResource(obj); \
		return ret; \
	}

DESTROY_IMPL(VkBuffer, DestroyBuffer)
DESTROY_IMPL(VkBufferView, DestroyBufferView)
DESTROY_IMPL(VkImage, DestroyImage)
DESTROY_IMPL(VkImageView, DestroyImageView)
DESTROY_IMPL(VkAttachmentView, DestroyAttachmentView)
DESTROY_IMPL(VkShader, DestroyShader)
DESTROY_IMPL(VkShaderModule, DestroyShaderModule)
DESTROY_IMPL(VkPipeline, DestroyPipeline)
DESTROY_IMPL(VkPipelineCache, DestroyPipelineCache)
DESTROY_IMPL(VkPipelineLayout, DestroyPipelineLayout)
DESTROY_IMPL(VkSampler, DestroySampler)
DESTROY_IMPL(VkDescriptorSetLayout, DestroyDescriptorSetLayout)
DESTROY_IMPL(VkDescriptorPool, DestroyDescriptorPool)
DESTROY_IMPL(VkDynamicViewportState, DestroyDynamicViewportState)
DESTROY_IMPL(VkDynamicRasterState, DestroyDynamicRasterState)
DESTROY_IMPL(VkDynamicColorBlendState, DestroyDynamicColorBlendState)
DESTROY_IMPL(VkDynamicDepthStencilState, DestroyDynamicDepthStencilState)
DESTROY_IMPL(VkSemaphore, DestroySemaphore)
DESTROY_IMPL(VkCmdPool, DestroyCommandPool)
DESTROY_IMPL(VkFramebuffer, DestroyFramebuffer)
DESTROY_IMPL(VkRenderPass, DestroyRenderPass)
DESTROY_IMPL(VkSwapChainWSI, DestroySwapChainWSI)

#undef DESTROY_IMPL

// needs to be separate since it's dispatchable
VkResult WrappedVulkan::vkDestroyCommandBuffer(VkDevice device, VkCmdBuffer obj)
{
	WrappedVkDispRes *wrapped = (WrappedVkDispRes *)GetWrapped(obj);
	GetResourceManager()->MarkCleanResource(wrapped->id);
	if(wrapped->record) wrapped->record->Delete(GetResourceManager());
	return ObjDisp(device)->DestroyCommandBuffer(Unwrap(device), wrapped->real.As<VkCmdBuffer>());
}


bool WrappedVulkan::Serialise_vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(uint32_t, nodeIdx, queueNodeIndex);
	SERIALISE_ELEMENT(uint32_t, idx, queueIndex);
	SERIALISE_ELEMENT(ResourceId, queueId, GetResID(*pQueue));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkQueue queue;
		VkResult ret = ObjDisp(device)->GetDeviceQueue(Unwrap(device), nodeIdx, idx, &queue);

		GetResourceManager()->WrapResource(Unwrap(device), queue);
		GetResourceManager()->AddLiveResource(queueId, queue);
	}

	return true;
}

VkResult WrappedVulkan::vkGetDeviceQueue(
    VkDevice                                    device,
    uint32_t                                    queueNodeIndex,
    uint32_t                                    queueIndex,
    VkQueue*                                    pQueue)
{
	VkResult ret = ObjDisp(device)->GetDeviceQueue(Unwrap(device), queueNodeIndex, queueIndex, pQueue);

	if(ret == VK_SUCCESS)
	{
		// it's perfectly valid for enumerate type functions to return the same handle
		// each time. If that happens, we will already have a wrapper created so just
		// return the wrapped object to the user and do nothing else
		if(GetResourceManager()->HasWrapper(RealVkRes((void *)*pQueue)))
		{
			*pQueue = (VkQueue)GetResourceManager()->GetWrapper(RealVkRes((void *)*pQueue));
		}
		else
		{
			ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pQueue);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(GET_DEVICE_QUEUE);
					Serialise_vkGetDeviceQueue(device, queueNodeIndex, queueIndex, pQueue);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pQueue);
				RDCASSERT(record);

				record->AddChunk(chunk);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, *pQueue);
			}
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
	SERIALISE_ELEMENT(ResourceId, queueId, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, fenceId, fence != VK_NULL_HANDLE ? GetResID(fence) : ResourceId());
	
	SERIALISE_ELEMENT(uint32_t, numCmds, cmdBufferCount);

	vector<ResourceId> cmdIds;
	VkCmdBuffer *cmds = m_State >= WRITING ? NULL : new VkCmdBuffer[numCmds];
	for(uint32_t i=0; i < numCmds; i++)
	{
		ResourceId bakedId;

		if(m_State >= WRITING)
		{
			VkResourceRecord *record = GetRecord(pCmdBuffers[i]);
			RDCASSERT(record->bakedCommands);
			if(record->bakedCommands)
				bakedId = record->bakedCommands->GetResourceID();
		}

		SERIALISE_ELEMENT(ResourceId, id, bakedId);

		if(m_State < WRITING)
		{
			cmdIds.push_back(id);

			cmds[i] = id != ResourceId()
			          ? Unwrap(GetResourceManager()->GetLiveHandle<VkCmdBuffer>(id))
			          : NULL;
		}
	}
	
	if(m_State < WRITING)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(queueId);
		if(fenceId != ResourceId())
			fence = GetResourceManager()->GetLiveHandle<VkFence>(fenceId);
		else
			fence = VK_NULL_HANDLE;
	}

	const string desc = m_pSerialiser->GetDebugStr();

	if(m_State == READING)
	{
		m_SubmittedFences.insert(fenceId);

		ObjDisp(queue)->QueueSubmit(Unwrap(queue), numCmds, cmds, Unwrap(fence));

		for(uint32_t i=0; i < numCmds; i++)
		{
			ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
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
					ResourceId partial = GetResID(PartialCmdBuf());
					RDCDEBUG("Queue Submit partial replay of %llu at %u, using %llu", cmdIds[c], eid, partial);
					trimmedCmdIds.push_back(partial);
					trimmedCmds.push_back(PartialCmdBuf());
				}
				else if(m_LastEventID >= end)
				{
					RDCDEBUG("Queue Submit full replay %llu", cmdIds[c]);
					trimmedCmdIds.push_back(cmdIds[c]);
					trimmedCmds.push_back(GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdIds[c]));
				}
				else
				{
					RDCDEBUG("Queue not submitting %llu", cmdIds[c]);
				}

				eid += 1+m_CmdBufferInfo[cmdIds[c]].eventCount;
			}

			RDCASSERT(trimmedCmds.size() > 0);

			m_SubmittedFences.insert(fenceId);

			ObjDisp(queue)->QueueSubmit(Unwrap(queue), (uint32_t)trimmedCmds.size(), &trimmedCmds[0], Unwrap(fence));

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = trimmedCmdIds[i];
				GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);
			}
		}
		else
		{
			m_SubmittedFences.insert(fenceId);

			ObjDisp(queue)->QueueSubmit(Unwrap(queue), numCmds, cmds, Unwrap(fence));

			for(uint32_t i=0; i < numCmds; i++)
			{
				ResourceId cmd = GetResourceManager()->GetLiveID(cmdIds[i]);
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkCmdBuffer *unwrapped = new VkCmdBuffer[cmdBufferCount];
	for(uint32_t i=0; i < cmdBufferCount; i++)
		unwrapped[i] = Unwrap(pCmdBuffers[i]);

	VkResult ret = ObjDisp(queue)->QueueSubmit(Unwrap(queue), cmdBufferCount, unwrapped, Unwrap(fence));

	SAFE_DELETE_ARRAY(unwrapped);

	if(m_State == WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_SUBMIT);
		Serialise_vkQueueSubmit(queue, cmdBufferCount, pCmdBuffers, fence);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	for(uint32_t i=0; i < cmdBufferCount; i++)
	{
		ResourceId cmd = GetResID(pCmdBuffers[i]);
		GetResourceManager()->ApplyTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo);

		VkResourceRecord *record = GetRecord(pCmdBuffers[i]);
		for(auto it = record->bakedCommands->dirtied.begin(); it != record->bakedCommands->dirtied.end(); ++it)
			GetResourceManager()->MarkDirtyResource(*it);

		// for each bound descriptor set, mark it referenced as well as all resources currently bound to it
		for(auto it = record->bakedCommands->boundDescSets.begin(); it != record->bakedCommands->boundDescSets.end(); ++it)
		{
			GetResourceManager()->MarkResourceFrameReferenced(GetResID(*it), eFrameRef_Read);

			VkResourceRecord *setrecord = GetRecord(*it);

			for(auto refit = setrecord->bindFrameRefs.begin(); refit != setrecord->bindFrameRefs.end(); ++refit)
				GetResourceManager()->MarkResourceFrameReferenced(refit->first, refit->second.second);
		}

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
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, sid, GetResID(semaphore));
	
	if(m_State < WRITING)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		ObjDisp(queue)->QueueSignalSemaphore(Unwrap(queue), Unwrap(GetResourceManager()->GetLiveHandle<VkSemaphore>(sid)));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueSignalSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = ObjDisp(queue)->QueueSignalSemaphore(Unwrap(queue), Unwrap(semaphore));
	
	if(m_State >= WRITING)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_SIGNAL_SEMAPHORE);
		Serialise_vkQueueSignalSemaphore(queue, semaphore);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	SERIALISE_ELEMENT(ResourceId, qid, GetResID(queue));
	SERIALISE_ELEMENT(ResourceId, sid, GetResID(semaphore));
	
	if(m_State < WRITING)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(qid);
		ObjDisp(queue)->QueueWaitSemaphore(Unwrap(queue), Unwrap(GetResourceManager()->GetLiveHandle<VkSemaphore>(sid)));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitSemaphore(VkQueue queue, VkSemaphore semaphore)
{
	VkResult ret = ObjDisp(queue)->QueueWaitSemaphore(Unwrap(queue), Unwrap(semaphore));
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_SEMAPHORE);
		Serialise_vkQueueWaitSemaphore(queue, semaphore);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(semaphore), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkQueueWaitIdle(VkQueue queue)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(queue));
	
	if(m_State < WRITING_CAPFRAME)
	{
		queue = GetResourceManager()->GetLiveHandle<VkQueue>(id);
		ObjDisp(queue)->QueueWaitIdle(Unwrap(queue));
	}

	return true;
}

VkResult WrappedVulkan::vkQueueWaitIdle(VkQueue queue)
{
	VkResult ret = ObjDisp(queue)->QueueWaitIdle(queue);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(QUEUE_WAIT_IDLE);
		Serialise_vkQueueWaitIdle(queue);

		m_FrameCaptureRecord->AddChunk(scope.Get());
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(queue), eFrameRef_Read);
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkDeviceWaitIdle(VkDevice device)
{
	SERIALISE_ELEMENT(ResourceId, id, GetResID(device));
	
	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(id);
		ObjDisp(device)->DeviceWaitIdle(Unwrap(device));
	}

	return true;
}

VkResult WrappedVulkan::vkDeviceWaitIdle(VkDevice device)
{
	VkResult ret = ObjDisp(device)->DeviceWaitIdle(device);
	
	if(m_State >= WRITING_CAPFRAME)
	{
		SCOPED_SERIALISE_CONTEXT(DEVICE_WAIT_IDLE);
		Serialise_vkDeviceWaitIdle(device);

		m_FrameCaptureRecord->AddChunk(scope.Get());
	}

	return ret;
}

// Memory functions

bool WrappedVulkan::Serialise_vkAllocMemory(
			VkDevice                                    device,
			const VkMemoryAllocInfo*                    pAllocInfo,
			VkDeviceMemory*                             pMem)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkMemoryAllocInfo, info, *pAllocInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pMem));

	if(m_State == READING)
	{
		VkDeviceMemory mem = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		// VKTODOLOW may need to re-write info to change memory type index to the
		// appropriate index on replay
		VkResult ret = ObjDisp(device)->AllocMemory(Unwrap(device), &info, &mem);
		
		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), mem);
			GetResourceManager()->AddLiveResource(id, mem);

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
	VkResult ret = ObjDisp(device)->AllocMemory(Unwrap(device), pAllocInfo, pMem);
	
	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pMem);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(ALLOC_MEM);
				Serialise_vkAllocMemory(device, pAllocInfo, pMem);

				chunk = scope.Get();
			}
			
			// create resource record for gpu memory
			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pMem);
			RDCASSERT(record);

			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pMem);
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
	WrappedVkNonDispRes *wrapped = (WrappedVkNonDispRes *)GetWrapped(mem);
	m_MemoryInfo.erase(wrapped->id);
	VkResult res = ObjDisp(device)->FreeMemory(Unwrap(device), wrapped->real.As<VkDeviceMemory>());

	GetResourceManager()->ReleaseWrappedResource(mem);

	return res;
}

VkResult WrappedVulkan::vkMapMemory(
			VkDevice                                    device,
			VkDeviceMemory                              mem,
			VkDeviceSize                                offset,
			VkDeviceSize                                size,
			VkMemoryMapFlags                            flags,
			void**                                      ppData)
{
	VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), offset, size, flags, ppData);

	if(ret == VK_SUCCESS && ppData)
	{
		ResourceId id = GetResID(mem);

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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, id, GetResID(mem));

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
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(id);

		void *mapPtr = NULL;
		VkResult ret = ObjDisp(device)->MapMemory(Unwrap(device), Unwrap(mem), memOffset, memSize, flags, &mapPtr);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Error mapping memory on replay: 0x%08x", ret);
		}
		else
		{
			memcpy((byte *)mapPtr+memOffset, data, (size_t)memSize);

			ret = ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
			
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
	VkResult ret = ObjDisp(device)->UnmapMemory(Unwrap(device), Unwrap(mem));
	
	if(m_State >= WRITING)
	{
		ResourceId id = GetResID(mem);

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

					VkResourceRecord *record = GetRecord(mem);

					if(m_State == WRITING_IDLE)
					{
						record->AddChunk(scope.Get());
					}
					else
					{
						m_FrameCaptureRecord->AddChunk(scope.Get());
						GetResourceManager()->MarkResourceFrameReferenced(GetResID(mem), eFrameRef_Write);
					}
				}
				else
				{
					GetResourceManager()->MarkDirtyResource(GetResID(mem));
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, bufId, GetResID(buffer));
	SERIALISE_ELEMENT(ResourceId, memId, GetResID(mem));
	SERIALISE_ELEMENT(uint64_t, offs, memOffset);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(memId);

		ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(mem), offs);
	}

	return true;
}

VkResult WrappedVulkan::vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	VkResourceRecord *record = GetRecord(buffer);

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
			m_FrameCaptureRecord->AddChunk(chunk);

			GetResourceManager()->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Write);
			GetResourceManager()->MarkResourceFrameReferenced(GetResID(mem), eFrameRef_Read);
		}
		else
		{
			record->AddChunk(chunk);
		}

		record->SetMemoryRecord(GetRecord(mem));
	}

	return ObjDisp(device)->BindBufferMemory(Unwrap(device), Unwrap(buffer), Unwrap(mem), memOffset);
}

bool WrappedVulkan::Serialise_vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, imgId, GetResID(image));
	SERIALISE_ELEMENT(ResourceId, memId, GetResID(mem));
	SERIALISE_ELEMENT(uint64_t, offs, memOffset);

	if(m_State < WRITING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgId);
		mem = GetResourceManager()->GetLiveHandle<VkDeviceMemory>(memId);

		ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(mem), offs);
	}

	return true;
}

VkResult WrappedVulkan::vkBindImageMemory(
    VkDevice                                    device,
    VkImage                                     image,
    VkDeviceMemory                              mem,
    VkDeviceSize                                memOffset)
{
	VkResourceRecord *record = GetRecord(image);

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
			m_FrameCaptureRecord->AddChunk(chunk);

			GetResourceManager()->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
			GetResourceManager()->MarkResourceFrameReferenced(GetResID(mem), eFrameRef_Read);
		}
		else
		{
			record->AddChunk(chunk);
		}

		record->SetMemoryRecord(GetRecord(mem));
	}

	return ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(image), Unwrap(mem), memOffset);
}

bool WrappedVulkan::Serialise_vkCreateBuffer(
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkBufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pBuffer));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkBuffer buf = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), &info, &buf);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), buf);
			GetResourceManager()->AddLiveResource(id, buf);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateBuffer(
			VkDevice                                    device,
			const VkBufferCreateInfo*                   pCreateInfo,
			VkBuffer*                                   pBuffer)
{
	VkResult ret = ObjDisp(device)->CreateBuffer(Unwrap(device), pCreateInfo, pBuffer);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pBuffer);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER);
				Serialise_vkCreateBuffer(device, pCreateInfo, pBuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pBuffer);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pBuffer);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateBufferView(
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkBufferViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pView));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkBufferView view = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), view);
			GetResourceManager()->AddLiveResource(id, view);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateBufferView(
			VkDevice                                    device,
			const VkBufferViewCreateInfo*               pCreateInfo,
			VkBufferView*                               pView)
{
	VkBufferViewCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.buffer = Unwrap(unwrappedInfo.buffer);
	VkResult ret = ObjDisp(device)->CreateBufferView(Unwrap(device), &unwrappedInfo, pView);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_BUFFER_VIEW);
				Serialise_vkCreateBufferView(device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pView);
			record->AddChunk(chunk);
			record->AddParent(GetRecord(pCreateInfo->buffer));
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pView);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateImage(
			VkDevice                                    device,
			const VkImageCreateInfo*                    pCreateInfo,
			VkImage*                                    pImage)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkImageCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pImage));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkImage img = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), &info, &img);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), img);
			GetResourceManager()->AddLiveResource(id, img);
			
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
	VkResult ret = ObjDisp(device)->CreateImage(Unwrap(device), pCreateInfo, pImage);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pImage);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_IMAGE);
				Serialise_vkCreateImage(device, pCreateInfo, pImage);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pImage);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pImage);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkImageViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pView));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkImageView view = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), view);
			GetResourceManager()->AddLiveResource(id, view);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateImageView(
    VkDevice                                    device,
    const VkImageViewCreateInfo*                pCreateInfo,
    VkImageView*                                pView)
{
	VkImageViewCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.image = Unwrap(unwrappedInfo.image);
	VkResult ret = ObjDisp(device)->CreateImageView(Unwrap(device), &unwrappedInfo, pView);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_IMAGE_VIEW);
				Serialise_vkCreateImageView(device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pView);
			record->AddChunk(chunk);
			record->AddParent(GetRecord(pCreateInfo->image));
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pView);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateAttachmentView(
    VkDevice                                    device,
    const VkAttachmentViewCreateInfo*           pCreateInfo,
    VkAttachmentView*                           pView)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkAttachmentViewCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pView));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkAttachmentView view = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateAttachmentView(Unwrap(device), &info, &view);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), view);
			GetResourceManager()->AddLiveResource(id, view);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateAttachmentView(
    VkDevice                                    device,
    const VkAttachmentViewCreateInfo*           pCreateInfo,
    VkAttachmentView*                           pView)
{
	VkAttachmentViewCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.image = Unwrap(unwrappedInfo.image);
	VkResult ret = ObjDisp(device)->CreateAttachmentView(Unwrap(device), &unwrappedInfo, pView);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pView);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_ATTACHMENT_VIEW);
				Serialise_vkCreateAttachmentView(device, pCreateInfo, pView);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pView);
			record->AddChunk(chunk);
			record->AddParent(GetRecord(pCreateInfo->image));
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pView);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkShaderModuleCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pShaderModule));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkShaderModule sh = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateShaderModule(Unwrap(device), &info, &sh);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sh);
			GetResourceManager()->AddLiveResource(id, sh);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateShaderModule(
		VkDevice                                    device,
		const VkShaderModuleCreateInfo*             pCreateInfo,
		VkShaderModule*                             pShaderModule)
{
	VkResult ret = ObjDisp(device)->CreateShaderModule(Unwrap(device), pCreateInfo, pShaderModule);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pShaderModule);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SHADER_MODULE);
				Serialise_vkCreateShaderModule(device, pCreateInfo, pShaderModule);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pShaderModule);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pShaderModule);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateShader(
    VkDevice                                    device,
    const VkShaderCreateInfo*                   pCreateInfo,
    VkShader*                                   pShader)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkShaderCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pShader));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkShader sh = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateShader(Unwrap(device), &info, &sh);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sh);
			GetResourceManager()->AddLiveResource(id, sh);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateShader(
    VkDevice                                    device,
    const VkShaderCreateInfo*                   pCreateInfo,
    VkShader*                                   pShader)
{
	VkShaderCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.module = Unwrap(unwrappedInfo.module);
	VkResult ret = ObjDisp(device)->CreateShader(Unwrap(device), &unwrappedInfo, pShader);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pShader);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SHADER);
				Serialise_vkCreateShader(device, pCreateInfo, pShader);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pShader);
			record->AddChunk(chunk);

			VkResourceRecord *modulerecord = GetRecord(pCreateInfo->module);
			record->AddParent(modulerecord);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pShader);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkPipelineCacheCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelineCache));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkPipelineCache cache = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), &info, &cache);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cache);
			GetResourceManager()->AddLiveResource(id, cache);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreatePipelineCache(
		VkDevice                                    device,
		const VkPipelineCacheCreateInfo*            pCreateInfo,
		VkPipelineCache*                            pPipelineCache)
{
	VkResult ret = ObjDisp(device)->CreatePipelineCache(Unwrap(device), pCreateInfo, pPipelineCache);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pPipelineCache);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_PIPE_CACHE);
				Serialise_vkCreatePipelineCache(device, pCreateInfo, pPipelineCache);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pPipelineCache);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pPipelineCache);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, cacheId, GetResID(pipelineCache));
	SERIALISE_ELEMENT(VkGraphicsPipelineCreateInfo, info, *pCreateInfos);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelines));

	if(m_State == READING)
	{
		VkPipeline pipe = VK_NULL_HANDLE;
		
		// use original ID
		m_CreationInfo.m_Pipeline[id].Init(&info);

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		pipelineCache = GetResourceManager()->GetLiveHandle<VkPipelineCache>(cacheId);

		VkResult ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache), 1, &info, &pipe);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pipe);
			GetResourceManager()->AddLiveResource(id, pipe);
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkGraphicsPipelineCreateInfo *unwrappedInfos = new VkGraphicsPipelineCreateInfo[count];
	for(uint32_t i=0; i < count; i++)
	{
		VkPipelineShaderStageCreateInfo *unwrappedStages = new VkPipelineShaderStageCreateInfo[pCreateInfos[i].stageCount];
		for(uint32_t j=0; j < pCreateInfos[i].stageCount; j++)
		{
			unwrappedStages[j] = pCreateInfos[i].pStages[j];
			unwrappedStages[j].shader = Unwrap(unwrappedStages[j].shader);
		}

		unwrappedInfos[i] = pCreateInfos[i];
		unwrappedInfos[i].pStages = unwrappedStages;
		unwrappedInfos[i].layout = Unwrap(unwrappedInfos[i].layout);
		unwrappedInfos[i].renderPass = Unwrap(unwrappedInfos[i].renderPass);
		unwrappedInfos[i].basePipelineHandle = Unwrap(unwrappedInfos[i].basePipelineHandle);
	}

	VkResult ret = ObjDisp(device)->CreateGraphicsPipelines(Unwrap(device), Unwrap(pipelineCache), count, unwrappedInfos, pPipelines);
	
	for(uint32_t i=0; i < count; i++)
		delete[] unwrappedInfos[i].pStages;
	SAFE_DELETE_ARRAY(unwrappedInfos);

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pPipelines[i]);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(CREATE_GRAPHICS_PIPE);
					Serialise_vkCreateGraphicsPipelines(device, pipelineCache, 1, &pCreateInfos[i], &pPipelines[i]);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pPipelines[i]);
				record->AddChunk(chunk);

				VkResourceRecord *cacherecord = GetRecord(pipelineCache);
				record->AddParent(cacherecord);

				VkResourceRecord *layoutrecord = GetRecord(pCreateInfos->layout);
				record->AddParent(layoutrecord);

				for(uint32_t i=0; i < pCreateInfos->stageCount; i++)
				{
					VkResourceRecord *shaderrecord = GetRecord(pCreateInfos->pStages[i].shader);
					record->AddParent(shaderrecord);
				}
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, pPipelines[i]);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDescriptorPoolUsage, pooluse, poolUsage);
	SERIALISE_ELEMENT(uint32_t, maxs, maxSets);
	SERIALISE_ELEMENT(VkDescriptorPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorPool));

	if(m_State == READING)
	{
		VkDescriptorPool pool = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), pooluse, maxs, &info, &pool);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
			GetResourceManager()->AddLiveResource(id, pool);
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
	VkResult ret = ObjDisp(device)->CreateDescriptorPool(Unwrap(device), poolUsage, maxSets, pCreateInfo, pDescriptorPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pDescriptorPool);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_POOL);
				Serialise_vkCreateDescriptorPool(device, poolUsage, maxSets, pCreateInfo, pDescriptorPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pDescriptorPool);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pDescriptorPool);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDescriptorSetLayout(
		VkDevice                                    device,
		const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
		VkDescriptorSetLayout*                      pSetLayout)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDescriptorSetLayoutCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSetLayout));

	// this creation info is needed at capture time (for creating/updating descriptor set bindings)
	// uses original ID in replay
	m_CreationInfo.m_DescSetLayout[id].Init(&info);

	if(m_State == READING)
	{
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &info, &layout);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), layout);
			GetResourceManager()->AddLiveResource(id, layout);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDescriptorSetLayout(
		VkDevice                                    device,
		const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
		VkDescriptorSetLayout*                      pSetLayout)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSetLayoutBinding *unwrapped = new VkDescriptorSetLayoutBinding[pCreateInfo->count];
	for(uint32_t i=0; i < pCreateInfo->count; i++)
	{
		unwrapped[i] = pCreateInfo->pBinding[i];

		if(unwrapped[i].pImmutableSamplers)
		{
			VkSampler *unwrappedSamplers = new VkSampler[unwrapped[i].arraySize];
			for(uint32_t j=0; j < unwrapped[i].arraySize; j++)
				unwrappedSamplers[j] = Unwrap(unwrapped[i].pImmutableSamplers[j]);
			unwrapped[i].pImmutableSamplers = unwrappedSamplers;
		}
	}

	VkDescriptorSetLayoutCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.pBinding = unwrapped;
	VkResult ret = ObjDisp(device)->CreateDescriptorSetLayout(Unwrap(device), &unwrappedInfo, pSetLayout);
	
	for(uint32_t i=0; i < pCreateInfo->count; i++)
		delete[] unwrapped[i].pImmutableSamplers;
	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSetLayout);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DESCRIPTOR_SET_LAYOUT);
				Serialise_vkCreateDescriptorSetLayout(device, pCreateInfo, pSetLayout);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSetLayout);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSetLayout);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreatePipelineLayout(
		VkDevice                                    device,
		const VkPipelineLayoutCreateInfo*           pCreateInfo,
		VkPipelineLayout*                           pPipelineLayout)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkPipelineLayoutCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pPipelineLayout));

	if(m_State == READING)
	{
		VkPipelineLayout layout = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		VkResult ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &info, &layout);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), layout);
			GetResourceManager()->AddLiveResource(id, layout);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreatePipelineLayout(
		VkDevice                                    device,
		const VkPipelineLayoutCreateInfo*           pCreateInfo,
		VkPipelineLayout*                           pPipelineLayout)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSetLayout *unwrapped = new VkDescriptorSetLayout[pCreateInfo->descriptorSetCount];
	for(uint32_t i=0; i < pCreateInfo->descriptorSetCount; i++)
		unwrapped[i] = Unwrap(pCreateInfo->pSetLayouts[i]);

	VkPipelineLayoutCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.pSetLayouts = unwrapped;

	VkResult ret = ObjDisp(device)->CreatePipelineLayout(Unwrap(device), &unwrappedInfo, pPipelineLayout);

	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pPipelineLayout);

		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_PIPE_LAYOUT);
				Serialise_vkCreatePipelineLayout(device, pCreateInfo, pPipelineLayout);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pPipelineLayout);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pPipelineLayout);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSamplerCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSampler));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkSampler samp = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), &info, &samp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), samp);
			GetResourceManager()->AddLiveResource(id, samp);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSampler(
			VkDevice                                    device,
			const VkSamplerCreateInfo*                  pCreateInfo,
			VkSampler*                                  pSampler)
{
	VkResult ret = ObjDisp(device)->CreateSampler(Unwrap(device), pCreateInfo, pSampler);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSampler);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SAMPLER);
				Serialise_vkCreateSampler(device, pCreateInfo, pSampler);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSampler);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSampler);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSemaphoreCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSemaphore));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkSemaphore sem = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), &info, &sem);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), sem);
			GetResourceManager()->AddLiveResource(id, sem);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			VkSemaphore*                                pSemaphore)
{
	VkResult ret = ObjDisp(device)->CreateSemaphore(Unwrap(device), pCreateInfo, pSemaphore);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSemaphore);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SEMAPHORE);
				Serialise_vkCreateSemaphore(device, pCreateInfo, pSemaphore);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSemaphore);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSemaphore);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkFramebufferCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pFramebuffer));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkFramebuffer fb = VK_NULL_HANDLE;
		
		// use original ID
		m_CreationInfo.m_Framebuffer[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &info, &fb);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), fb);
			GetResourceManager()->AddLiveResource(id, fb);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateFramebuffer(
			VkDevice                                    device,
			const VkFramebufferCreateInfo*              pCreateInfo,
			VkFramebuffer*                              pFramebuffer)
{
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkAttachmentBindInfo *unwrapped = new VkAttachmentBindInfo[pCreateInfo->attachmentCount];
	for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
	{
		unwrapped[i] = pCreateInfo->pAttachments[i];
		unwrapped[i].view = Unwrap(unwrapped[i].view);
	}

	VkFramebufferCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
	unwrappedInfo.pAttachments = unwrapped;

	VkResult ret = ObjDisp(device)->CreateFramebuffer(Unwrap(device), &unwrappedInfo, pFramebuffer);

	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pFramebuffer);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_FRAMEBUFFER);
				Serialise_vkCreateFramebuffer(device, pCreateInfo, pFramebuffer);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pFramebuffer);
			record->AddChunk(chunk);

			for(uint32_t i=0; i < pCreateInfo->attachmentCount; i++)
			{
				record->AddParent(GetRecord(pCreateInfo->pAttachments[i].view));
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pFramebuffer);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkRenderPassCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pRenderPass));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkRenderPass rp = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), &info, &rp);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), rp);
			GetResourceManager()->AddLiveResource(id, rp);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateRenderPass(
			VkDevice                                    device,
			const VkRenderPassCreateInfo*               pCreateInfo,
			VkRenderPass*                               pRenderPass)
{
	VkResult ret = ObjDisp(device)->CreateRenderPass(Unwrap(device), pCreateInfo, pRenderPass);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pRenderPass);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_RENDERPASS);
				Serialise_vkCreateRenderPass(device, pCreateInfo, pRenderPass);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pRenderPass);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pRenderPass);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicViewportStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicViewportState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_VPScissor[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicViewportState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicViewportState(
			VkDevice                                    device,
			const VkDynamicViewportStateCreateInfo*           pCreateInfo,
			VkDynamicViewportState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicViewportState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_VIEWPORT_STATE);
				Serialise_vkCreateDynamicViewportState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicRasterState(
			VkDevice                                        device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicRasterStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicRasterState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_Raster[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicRasterState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicRasterState(
			VkDevice                                        device,
			const VkDynamicRasterStateCreateInfo*           pCreateInfo,
			VkDynamicRasterState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicRasterState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_RASTER_STATE);
				Serialise_vkCreateDynamicRasterState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicColorBlendState(
			VkDevice                                            device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicColorBlendStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicColorBlendState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_Blend[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicColorBlendState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicColorBlendState(
			VkDevice                                    device,
			const VkDynamicColorBlendStateCreateInfo*           pCreateInfo,
			VkDynamicColorBlendState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicColorBlendState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_BLEND_STATE);
				Serialise_vkCreateDynamicColorBlendState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
		}
	}

	return ret;
}

bool WrappedVulkan::Serialise_vkCreateDynamicDepthStencilState(
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkDynamicDepthStencilStateCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pState));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkDynamicDepthStencilState state = VK_NULL_HANDLE;

		// use original ID
		m_CreationInfo.m_DepthStencil[id].Init(&info);

		VkResult ret = ObjDisp(device)->CreateDynamicDepthStencilState(Unwrap(device), &info, &state);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), state);
			GetResourceManager()->AddLiveResource(id, state);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateDynamicDepthStencilState(
			VkDevice                                    device,
			const VkDynamicDepthStencilStateCreateInfo*           pCreateInfo,
			VkDynamicDepthStencilState*                           pState)
{
	VkResult ret = ObjDisp(device)->CreateDynamicDepthStencilState(Unwrap(device), pCreateInfo, pState);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pState);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_DEPTH_STATE);
				Serialise_vkCreateDynamicDepthStencilState(device, pCreateInfo, pState);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pState);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pState);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkCmdPoolCreateInfo, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pCmdPool));

	if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkCmdPool pool = VK_NULL_HANDLE;

		VkResult ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), &info, &pool);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), pool);
			GetResourceManager()->AddLiveResource(id, pool);
		}
	}

	return true;
}

VkResult WrappedVulkan::vkCreateCommandPool(
			VkDevice                                    device,
			const VkCmdPoolCreateInfo*                  pCreateInfo,
			VkCmdPool*                                  pCmdPool)
{
	VkResult ret = ObjDisp(device)->CreateCommandPool(Unwrap(device), pCreateInfo, pCmdPool);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pCmdPool);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_CMD_POOL);
				Serialise_vkCreateCommandPool(device, pCreateInfo, pCmdPool);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pCmdPool);
			record->AddChunk(chunk);
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pCmdPool);
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
	return ObjDisp(device)->ResetCommandPool(device, cmdPool, flags);
}


// Command buffer functions

VkResult WrappedVulkan::vkCreateCommandBuffer(
	VkDevice                        device,
	const VkCmdBufferCreateInfo* pCreateInfo,
	VkCmdBuffer*                   pCmdBuffer)
{
	VkCmdBufferCreateInfo unwrappedInfo = *pCreateInfo;
	unwrappedInfo.cmdPool = Unwrap(unwrappedInfo.cmdPool);
	VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &unwrappedInfo, pCmdBuffer);

	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pCmdBuffer);
		
		if(m_State >= WRITING)
		{
			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pCmdBuffer);

			record->bakedCommands = NULL;

			record->AddParent(GetRecord(pCreateInfo->cmdPool));

			// we don't serialise this as we never create this command buffer directly.
			// Instead we create a command buffer for each baked list that we find.
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pCmdBuffer);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, poolId, GetResID(descriptorPool));
	SERIALISE_ELEMENT(VkDescriptorSetUsage, usage, setUsage);
	SERIALISE_ELEMENT(ResourceId, layoutId, GetResID(*pSetLayouts));
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pDescriptorSets));

	if(m_State == READING)
	{
		VkDescriptorSet descset = VK_NULL_HANDLE;

		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		descriptorPool = GetResourceManager()->GetLiveHandle<VkDescriptorPool>(poolId);
		VkDescriptorSetLayout layout = GetResourceManager()->GetLiveHandle<VkDescriptorSetLayout>(layoutId);

		uint32_t cnt = 0;
		VkResult ret = ObjDisp(device)->AllocDescriptorSets(Unwrap(device), descriptorPool, usage, 1, &layout, &descset, &cnt);

		if(ret != VK_SUCCESS)
		{
			RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
		}
		else
		{
			ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), descset);
			GetResourceManager()->AddLiveResource(id, descset);

			// this is stored in the resource record on capture, we need to be able to look to up
			m_DescriptorSetInfo[id].layout = layoutId;
			m_CreationInfo.m_DescSetLayout[layoutId].CreateBindingsArray(m_DescriptorSetInfo[id].currentBindings);
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSetLayout *unwrapped = new VkDescriptorSetLayout[count];
	for(uint32_t i=0; i < count; i++)
		unwrapped[i] = Unwrap(pSetLayouts[i]);

	VkResult ret = ObjDisp(device)->AllocDescriptorSets(Unwrap(device), Unwrap(descriptorPool), setUsage, count, unwrapped, pDescriptorSets, pCount);

	SAFE_DELETE_ARRAY(unwrapped);
	
	RDCASSERT(pCount == NULL || *pCount == count); // VKTODOMED: find out what *pCount < count means

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), pDescriptorSets[i]);

			if(m_State >= WRITING)
			{
				Chunk *chunk = NULL;

				{
					SCOPED_SERIALISE_CONTEXT(ALLOC_DESC_SET);
					Serialise_vkAllocDescriptorSets(device, descriptorPool, setUsage, 1, &pSetLayouts[i], &pDescriptorSets[i], NULL);

					chunk = scope.Get();
				}

				VkResourceRecord *record = GetResourceManager()->AddResourceRecord(pDescriptorSets[i]);
				record->AddChunk(chunk);

				ResourceId layoutID = GetResID(pSetLayouts[i]);

				record->AddParent(GetRecord(descriptorPool));
				record->AddParent(GetResourceManager()->GetResourceRecord(layoutID));

				// just always treat descriptor sets as dirty
				GetResourceManager()->MarkDirtyResource(id);

				record->layout = layoutID;
				m_CreationInfo.m_DescSetLayout[layoutID].CreateBindingsArray(record->descBindings);
			}
			else
			{
				GetResourceManager()->AddLiveResource(id, pDescriptorSets[i]);
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSet *unwrapped = new VkDescriptorSet[count];
	for(uint32_t i=0; i < count; i++)
		unwrapped[i] = Unwrap(pDescriptorSets[i]);

	VkResult ret = ObjDisp(device)->FreeDescriptorSets(Unwrap(device), Unwrap(descriptorPool), count, unwrapped);

	SAFE_DELETE_ARRAY(unwrapped);

	if(ret == VK_SUCCESS)
	{
		for(uint32_t i=0; i < count; i++)
		{
			ResourceId id = GetResID(pDescriptorSets[i]);

			GetResourceManager()->MarkCleanResource(id);
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
			if(record)
				record->Delete(GetResourceManager());
			GetResourceManager()->ReleaseWrappedResource(pDescriptorSets[i]);
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
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
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
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

		if(writes)
			ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 1, &writeDesc, 0, NULL);
		else
			ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), 0, NULL, 1, &copyDesc);
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
	VkResult ret = VK_SUCCESS;
	
	{
		// VKTODOLOW this should be a persistent per-thread array that resizes up
		// to a high water mark, so we don't have to allocate
		vector<VkDescriptorInfo> desc;

		uint32_t numInfos = 0;
		for(uint32_t i=0; i < writeCount; i++) numInfos += pDescriptorWrites[i].count;

		// ensure we don't resize while looping so we can take pointers
		desc.resize(numInfos);

		VkWriteDescriptorSet *unwrappedWrites = new VkWriteDescriptorSet[writeCount];
		VkCopyDescriptorSet *unwrappedCopies = new VkCopyDescriptorSet[copyCount];
		
		uint32_t curInfo = 0;
		for(uint32_t i=0; i < writeCount; i++)
		{
			unwrappedWrites[i] = pDescriptorWrites[i];
			unwrappedWrites[i].destSet = Unwrap(unwrappedWrites[i].destSet);

			VkDescriptorInfo *unwrappedInfos = &desc[curInfo];
			curInfo += pDescriptorWrites[i].count;

			for(uint32_t j=0; j < pDescriptorWrites[i].count; j++)
			{
				unwrappedInfos[j] = unwrappedWrites[i].pDescriptors[j];
				unwrappedInfos[j].bufferView = Unwrap(unwrappedInfos[j].bufferView);
				unwrappedInfos[j].sampler = Unwrap(unwrappedInfos[j].sampler);
				unwrappedInfos[j].imageView = Unwrap(unwrappedInfos[j].imageView);
				unwrappedInfos[j].attachmentView = Unwrap(unwrappedInfos[j].attachmentView);
			}
			
			unwrappedWrites[i].pDescriptors = unwrappedInfos;
		}

		for(uint32_t i=0; i < copyCount; i++)
		{
			unwrappedCopies[i] = pDescriptorCopies[i];
			unwrappedCopies[i].destSet = Unwrap(unwrappedCopies[i].destSet);
			unwrappedCopies[i].srcSet = Unwrap(unwrappedCopies[i].srcSet);
		}

		ret = ObjDisp(device)->UpdateDescriptorSets(Unwrap(device), writeCount, unwrappedWrites, copyCount, unwrappedCopies);
		
		for(uint32_t i=0; i < writeCount; i++) delete[] unwrappedWrites[i].pDescriptors;
		SAFE_DELETE_ARRAY(unwrappedWrites);
		SAFE_DELETE_ARRAY(unwrappedCopies);
	}

	if(ret == VK_SUCCESS)
	{
		if(m_State == WRITING_CAPFRAME)
		{
			for(uint32_t i=0; i < writeCount; i++)
			{
				{
					SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
					Serialise_vkUpdateDescriptorSets(device, 1, &pDescriptorWrites[i], 0, NULL);

					m_FrameCaptureRecord->AddChunk(scope.Get());
				}

				// don't have to mark referenced any of the resources pointed to by the descriptor set - that's handled
				// on queue submission by marking ref'd all the current bindings of the sets referenced by the cmd buffer
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorWrites[i].destSet), eFrameRef_Write);
			}

			for(uint32_t i=0; i < copyCount; i++)
			{
				{
					SCOPED_SERIALISE_CONTEXT(UPDATE_DESC_SET);
					Serialise_vkUpdateDescriptorSets(device, 0, NULL, 1, &pDescriptorCopies[i]);

					m_FrameCaptureRecord->AddChunk(scope.Get());
				}
				
				// don't have to mark referenced any of the resources pointed to by the descriptor sets - that's handled
				// on queue submission by marking ref'd all the current bindings of the sets referenced by the cmd buffer
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].destSet), eFrameRef_Write);
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(pDescriptorCopies[i].srcSet), eFrameRef_Read);
			}
		}

		// need to track descriptor set contents whether capframing or idle
		if(m_State >= WRITING)
		{
			for(uint32_t i=0; i < writeCount; i++)
			{
				VkResourceRecord *record = GetRecord(pDescriptorWrites[i].destSet);
				const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[record->layout];

				RDCASSERT(pDescriptorWrites[i].destBinding < record->descBindings.size());
				
				VkDescriptorInfo *binding = record->descBindings[pDescriptorWrites[i].destBinding];

				FrameRefType ref = eFrameRef_Write;

				switch(layout.bindings[pDescriptorWrites[i].destBinding].descriptorType)
				{
					case VK_DESCRIPTOR_TYPE_SAMPLER:
					case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
					case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
						ref = eFrameRef_Read;
						break;
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
						ref = eFrameRef_Write;
						break;
					default:
						RDCERR("Unexpected descriptor type");
				}

				// We need to handle the cases where these bindings are stale:
				// ie. image handle 0xf00baa is allocated
				// bound into a descriptor set
				// image is released
				// descriptor set is bound but this image is never used by shader etc.
				//
				// worst case, a new image or something has been added with this handle -
				// in this case we end up ref'ing an image that isn't actually used.
				// Worst worst case, we ref an image as write when actually it's not, but
				// this is likewise not a serious problem, and rather difficult to solve
				// (would need to version handles somehow, but don't have enough bits
				// to do that reliably).
				//
				// This is handled by RemoveBindFrameRef silently dropping id == ResourceId()

				for(uint32_t d=0; d < pDescriptorWrites[i].count; d++)
				{
					VkDescriptorInfo &bind = binding[pDescriptorWrites[i].destArrayElement + d];

					if(bind.attachmentView != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.attachmentView));
					if(bind.bufferView != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.bufferView));
					if(bind.imageView != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.imageView));
					if(bind.sampler != VK_NULL_HANDLE)
						record->RemoveBindFrameRef(GetResID(bind.sampler));

					bind = pDescriptorWrites[i].pDescriptors[d];

					if(bind.attachmentView != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.attachmentView), ref);
					if(bind.bufferView != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.bufferView), ref);
					if(bind.imageView != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.imageView), ref);
					if(bind.sampler != VK_NULL_HANDLE)
						record->AddBindFrameRef(GetResID(bind.sampler), ref);
				}
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
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));

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
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	m_pSerialiser->Serialise("createInfo", createInfo);

	if(m_State < WRITING)
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

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
				VkResult ret = ObjDisp(cmdBuffer)->CreateCommandBuffer(Unwrap(device), &createInfo, &cmd);

				if(ret != VK_SUCCESS)
				{
					RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
				}
				else
				{
					GetResourceManager()->WrapResource(Unwrap(device), cmd);
				}

				m_PartialReplayData.resultPartialCmdBuffer = cmd;
				m_PartialReplayData.partialDevice = device;

				// add one-time submit flag as this partial cmd buffer will only be submitted once
				info.flags |= VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT;

				ObjDisp(cmdBuffer)->BeginCommandBuffer(Unwrap(cmd), &info);
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
			VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &createInfo, &cmd);

			if(ret != VK_SUCCESS)
			{
				RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
			}
			else
			{
				ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
				GetResourceManager()->AddLiveResource(bakeId, cmd);
			}

			// whenever a vkCmd command-building chunk asks for the command buffer, it
			// will get our baked version.
			GetResourceManager()->ReplaceResource(cmdId, bakeId);
		}
		else
		{
			cmd = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);
		}

		{
			ResourceId liveBaked = GetResourceManager()->GetLiveID(bakeId);
			m_CmdBufferInfo[liveBaked].device = VK_NULL_HANDLE;
		}

		ObjDisp(device)->BeginCommandBuffer(Unwrap(cmd), &info);
	}

	return true;
}

VkResult WrappedVulkan::vkBeginCommandBuffer(
			VkCmdBuffer                                 cmdBuffer,
			const VkCmdBufferBeginInfo*                 pBeginInfo)
{
	VkResourceRecord *record = GetRecord(cmdBuffer);
	RDCASSERT(record);

	if(record)
	{
		if(record->bakedCommands)
			record->bakedCommands->Delete(GetResourceManager());

		record->bakedCommands = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());

		{
			SCOPED_SERIALISE_CONTEXT(BEGIN_CMD_BUFFER);
			Serialise_vkBeginCommandBuffer(cmdBuffer, pBeginInfo);
			
			record->AddChunk(scope.Get());
		}
	}

	VkCmdBufferBeginInfo unwrappedInfo = *pBeginInfo;
	unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);

	return ObjDisp(cmdBuffer)->BeginCommandBuffer(Unwrap(cmdBuffer), &unwrappedInfo);
}

bool WrappedVulkan::Serialise_vkEndCommandBuffer(VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));

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
			cmdBuffer = PartialCmdBuf();
			RDCDEBUG("Ending partial command buffer for %llu baked to %llu", cmdId, bakeId);

			if(m_PartialReplayData.renderPassActive)
				ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));

			ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));

			m_PartialReplayData.partialParent = ResourceId();
		}

		m_CurEventID--;
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);

		GetResourceManager()->RemoveReplacement(cmdId);

		ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));

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
	VkResourceRecord *record = GetRecord(cmdBuffer);
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

	return ObjDisp(cmdBuffer)->EndCommandBuffer(Unwrap(cmdBuffer));
}

bool WrappedVulkan::Serialise_vkResetCommandBuffer(VkCmdBuffer cmdBuffer, VkCmdBufferResetFlags flags)
{
	SERIALISE_ELEMENT(ResourceId, cmdId, GetResID(cmdBuffer));
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
	
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	m_pSerialiser->Serialise("createInfo", info);

	if(m_State == EXECUTING)
	{
		// VKTODOHIGH check how vkResetCommandBuffer interacts with partial replays
	}
	else if(m_State == READING)
	{
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);
		VkCmdBuffer cmd = VK_NULL_HANDLE;

		if(!GetResourceManager()->HasLiveResource(bakeId))
		{
			VkResult ret = ObjDisp(device)->CreateCommandBuffer(Unwrap(device), &info, &cmd);

			if(ret != VK_SUCCESS)
			{
				RDCERR("Failed on resource serialise-creation, VkResult: 0x%08x", ret);
			}
			else
			{
				ResourceId live = GetResourceManager()->WrapResource(Unwrap(device), cmd);
				GetResourceManager()->AddLiveResource(bakeId, cmd);
			}

			// whenever a vkCmd command-building chunk asks for the command buffer, it
			// will get our baked version.
			GetResourceManager()->ReplaceResource(cmdId, bakeId);
		}
		else
		{
			cmd = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(bakeId);
		}
		
		{
			ResourceId liveBaked = GetResourceManager()->GetLiveID(bakeId);
			m_CmdBufferInfo[liveBaked].device = VK_NULL_HANDLE;
		}

		ObjDisp(device)->ResetCommandBuffer(Unwrap(cmd), fl);
	}

	return true;
}

VkResult WrappedVulkan::vkResetCommandBuffer(
	  VkCmdBuffer                                 cmdBuffer,
    VkCmdBufferResetFlags                       flags)
{
	VkResourceRecord *record = GetRecord(cmdBuffer);
	RDCASSERT(record);

	if(record)
	{
		if(record->bakedCommands)
			record->bakedCommands->Delete(GetResourceManager());
		
		record->bakedCommands = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());

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

	return ObjDisp(cmdBuffer)->ResetCommandBuffer(Unwrap(cmdBuffer), flags);
}

// Command buffer building functions

bool WrappedVulkan::Serialise_vkCmdBeginRenderPass(
			VkCmdBuffer                                 cmdBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkRenderPassContents                        contents)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkRenderPassBeginInfo, beginInfo, *pRenderPassBegin);
	SERIALISE_ELEMENT(VkRenderPassContents, cont, contents);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			m_PartialReplayData.renderPassActive = true;
			ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &beginInfo, cont);

			m_PartialReplayData.state.renderPass = GetResourceManager()->GetOriginalID(GetResID(beginInfo.renderPass));
			m_PartialReplayData.state.framebuffer = GetResourceManager()->GetOriginalID(GetResID(beginInfo.framebuffer));
			m_PartialReplayData.state.renderArea = beginInfo.renderArea;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &beginInfo, cont);

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
	VkRenderPassBeginInfo unwrappedInfo = *pRenderPassBegin;
	unwrappedInfo.renderPass = Unwrap(unwrappedInfo.renderPass);
	unwrappedInfo.framebuffer = Unwrap(unwrappedInfo.framebuffer);
	ObjDisp(cmdBuffer)->CmdBeginRenderPass(Unwrap(cmdBuffer), &unwrappedInfo, contents);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BEGIN_RENDERPASS);
		Serialise_vkCmdBeginRenderPass(cmdBuffer, pRenderPassBegin, contents);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->renderPass), eFrameRef_Read);
		// VKTODOMED should mark framebuffer read and attachments write
		record->MarkResourceFrameReferenced(GetResID(pRenderPassBegin->framebuffer), eFrameRef_Write);
	}
}

bool WrappedVulkan::Serialise_vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			m_PartialReplayData.renderPassActive = false;
			ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));

			m_PartialReplayData.state.renderPass = ResourceId();
			m_PartialReplayData.state.framebuffer = ResourceId();
			RDCEraseEl(m_PartialReplayData.state.renderArea);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));
	}

	return true;
}

void WrappedVulkan::vkCmdEndRenderPass(
			VkCmdBuffer                                 cmdBuffer)
{
	ObjDisp(cmdBuffer)->CmdEndRenderPass(Unwrap(cmdBuffer));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkPipelineBindPoint, bind, pipelineBindPoint);
	SERIALISE_ELEMENT(ResourceId, pipeid, GetResID(pipeline));

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			pipeline = GetResourceManager()->GetLiveHandle<VkPipeline>(pipeid);
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindPipeline(Unwrap(cmdBuffer), bind, Unwrap(pipeline));
			if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				m_PartialReplayData.state.graphics.pipeline = pipeid;
			else
				m_PartialReplayData.state.compute.pipeline = pipeid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		pipeline = GetResourceManager()->GetLiveHandle<VkPipeline>(pipeid);

		// track this while reading, as we need to bind current topology & index byte width to draws
		if(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
			m_PartialReplayData.state.graphics.pipeline = pipeid;
		else
			m_PartialReplayData.state.compute.pipeline = pipeid;

		ObjDisp(cmdBuffer)->CmdBindPipeline(Unwrap(cmdBuffer), bind, Unwrap(pipeline));
	}

	return true;
}

void WrappedVulkan::vkCmdBindPipeline(
			VkCmdBuffer                                 cmdBuffer,
			VkPipelineBindPoint                         pipelineBindPoint,
			VkPipeline                                  pipeline)
{
	ObjDisp(cmdBuffer)->CmdBindPipeline(Unwrap(cmdBuffer), pipelineBindPoint, Unwrap(pipeline));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_PIPELINE);
		Serialise_vkCmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(pipeline), eFrameRef_Read);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, layoutid, GetResID(layout));
	SERIALISE_ELEMENT(VkPipelineBindPoint, bind, pipelineBindPoint);
	SERIALISE_ELEMENT(uint32_t, first, firstSet);

	SERIALISE_ELEMENT(uint32_t, numSets, setCount);

	ResourceId *descriptorIDs = new ResourceId[numSets];

	VkDescriptorSet *sets = (VkDescriptorSet *)pDescriptorSets;
	if(m_State < WRITING)
		sets = new VkDescriptorSet[numSets];

	for(uint32_t i=0; i < numSets; i++)
	{
		if(m_State >= WRITING) descriptorIDs[i] = GetResID(sets[i]);
		m_pSerialiser->Serialise("DescriptorSet", descriptorIDs[i]);
		if(m_State < WRITING)  sets[i] = Unwrap(GetResourceManager()->GetLiveHandle<VkDescriptorSet>(descriptorIDs[i]));
	}

	SERIALISE_ELEMENT(uint32_t, offsCount, dynamicOffsetCount);
	SERIALISE_ELEMENT_ARR_OPT(uint32_t, offs, pDynamicOffsets, offsCount, offsCount > 0);

	if(m_State == EXECUTING)
	{
		layout = GetResourceManager()->GetLiveHandle<VkPipelineLayout>(layoutid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), bind, Unwrap(layout), first, numSets, sets, offsCount, offs);

			vector<ResourceId> &descsets =
				(bind == VK_PIPELINE_BIND_POINT_GRAPHICS)
				? m_PartialReplayData.state.graphics.descSets
				: m_PartialReplayData.state.compute.descSets;

			// expand as necessary
			if(descsets.size() < first + numSets)
				descsets.resize(first + numSets);

			for(uint32_t i=0; i < numSets; i++)
				descsets[first+i] = descriptorIDs[i];

			// if there are dynamic offsets, bake them into the current bindings by alias'ing
			// the image layout member (which is never used for buffer views).
			// This lets us look it up easily when we want to show the current pipeline state
			RDCCOMPILE_ASSERT(sizeof(VkImageLayout) >= sizeof(uint32_t), "Can't alias image layout for dynamic offset!");
			if(offsCount > 0)
			{
				uint32_t o = 0;

				// spec states that dynamic offsets precisely match all the offsets needed for these
				// sets, in order of set N before set N+1, binding X before binding X+1 within a set,
				// and in array element order within a binding
				for(uint32_t i=0; i < numSets; i++)
				{
					const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[descriptorIDs[i]];

					for(size_t b=0; b < layout.bindings.size(); b++)
					{
						// not dynamic, doesn't need an offset
						if(layout.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC &&
							 layout.bindings[b].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
							 continue;

						// assign every array element an offset according to array size
						for(uint32_t a=0; a < layout.bindings[b].arraySize; a++)
						{
							RDCASSERT(o < offsCount);
							uint32_t *alias = (uint32_t *)&m_DescriptorSetInfo[descriptorIDs[i]].currentBindings[b][a].imageLayout;
							*alias = offs[o++];
						}
					}
				}
			}
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		layout = GetResourceManager()->GetLiveHandle<VkPipelineLayout>(layoutid);

		ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), bind, Unwrap(layout), first, numSets, sets, offsCount, offs);
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkDescriptorSet *unwrapped = new VkDescriptorSet[setCount];
	for(uint32_t i=0; i < setCount; i++)
		unwrapped[i] = Unwrap(pDescriptorSets[i]);

	ObjDisp(cmdBuffer)->CmdBindDescriptorSets(Unwrap(cmdBuffer), pipelineBindPoint, Unwrap(layout), firstSet, setCount, unwrapped, dynamicOffsetCount, pDynamicOffsets);

	SAFE_DELETE_ARRAY(unwrapped);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_DESCRIPTOR_SET);
		Serialise_vkCmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, layout, firstSet, setCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(layout), eFrameRef_Read);
		record->boundDescSets.insert(pDescriptorSets, pDescriptorSets + setCount);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicViewportState));

	if(m_State == EXECUTING)
	{
		dynamicViewportState = GetResourceManager()->GetLiveHandle<VkDynamicViewportState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindDynamicViewportState(Unwrap(cmdBuffer), Unwrap(dynamicViewportState));
			m_PartialReplayData.state.dynamicVP = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicViewportState = GetResourceManager()->GetLiveHandle<VkDynamicViewportState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicViewportState(Unwrap(cmdBuffer), Unwrap(dynamicViewportState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicViewportState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicViewportState                      dynamicViewportState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicViewportState(Unwrap(cmdBuffer), Unwrap(dynamicViewportState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_VP_STATE);
		Serialise_vkCmdBindDynamicViewportState(cmdBuffer, dynamicViewportState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicViewportState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                        dynamicRasterState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicRasterState));

	if(m_State == EXECUTING)
	{
		dynamicRasterState = GetResourceManager()->GetLiveHandle<VkDynamicRasterState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();

			ObjDisp(cmdBuffer)->CmdBindDynamicRasterState(Unwrap(cmdBuffer), Unwrap(dynamicRasterState));
			m_PartialReplayData.state.dynamicRS = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicRasterState = GetResourceManager()->GetLiveHandle<VkDynamicRasterState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicRasterState(Unwrap(cmdBuffer), Unwrap(dynamicRasterState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicRasterState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicRasterState                      dynamicRasterState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicRasterState(Unwrap(cmdBuffer), Unwrap(dynamicRasterState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_RS_STATE);
		Serialise_vkCmdBindDynamicRasterState(cmdBuffer, dynamicRasterState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicRasterState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicColorBlendState));

	if(m_State == EXECUTING)
	{
		dynamicColorBlendState = GetResourceManager()->GetLiveHandle<VkDynamicColorBlendState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindDynamicColorBlendState(Unwrap(cmdBuffer), Unwrap(dynamicColorBlendState));
			m_PartialReplayData.state.dynamicCB = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicColorBlendState = GetResourceManager()->GetLiveHandle<VkDynamicColorBlendState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicColorBlendState(Unwrap(cmdBuffer), Unwrap(dynamicColorBlendState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicColorBlendState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicColorBlendState                    dynamicColorBlendState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicColorBlendState(Unwrap(cmdBuffer), Unwrap(dynamicColorBlendState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_CB_STATE);
		Serialise_vkCmdBindDynamicColorBlendState(cmdBuffer, dynamicColorBlendState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicColorBlendState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, stateid, GetResID(dynamicDepthStencilState));

	if(m_State == EXECUTING)
	{
		dynamicDepthStencilState = GetResourceManager()->GetLiveHandle<VkDynamicDepthStencilState>(stateid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindDynamicDepthStencilState(Unwrap(cmdBuffer), Unwrap(dynamicDepthStencilState));
			m_PartialReplayData.state.dynamicDS = stateid;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		dynamicDepthStencilState = GetResourceManager()->GetLiveHandle<VkDynamicDepthStencilState>(stateid);

		ObjDisp(cmdBuffer)->CmdBindDynamicDepthStencilState(Unwrap(cmdBuffer), Unwrap(dynamicDepthStencilState));
	}

	return true;
}

void WrappedVulkan::vkCmdBindDynamicDepthStencilState(
			VkCmdBuffer                                 cmdBuffer,
			VkDynamicDepthStencilState                  dynamicDepthStencilState)
{
	ObjDisp(cmdBuffer)->CmdBindDynamicDepthStencilState(Unwrap(cmdBuffer), Unwrap(dynamicDepthStencilState));

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_DS_STATE);
		Serialise_vkCmdBindDynamicDepthStencilState(cmdBuffer, dynamicDepthStencilState);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(dynamicDepthStencilState), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdBindVertexBuffers(
    VkCmdBuffer                                 cmdBuffer,
    uint32_t                                    startBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			id = GetResID(pBuffers[i]);
			o = pOffsets[i];
		}

		m_pSerialiser->Serialise("pBuffers[]", id);
		m_pSerialiser->Serialise("pOffsets[]", o);

		if(m_State < WRITING)
		{
			bufids.push_back(id);
			bufs.push_back(Unwrap(GetResourceManager()->GetLiveHandle<VkBuffer>(id)));
			offs.push_back(o);
		}
	}

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), start, count, &bufs[0], &offs[0]);

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
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		
		ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), start, count, &bufs[0], &offs[0]);
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
	// VKTODOLOW this should be a persistent per-thread array that resizes up
	// to a high water mark, so we don't have to allocate
	VkBuffer *unwrapped = new VkBuffer[bindingCount];
	for(uint32_t i=0; i < bindingCount; i++)
		unwrapped[i] = Unwrap(pBuffers[i]);

	ObjDisp(cmdBuffer)->CmdBindVertexBuffers(Unwrap(cmdBuffer), startBinding, bindingCount, unwrapped, pOffsets);

	SAFE_DELETE_ARRAY(unwrapped);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_VERTEX_BUFFERS);
		Serialise_vkCmdBindVertexBuffers(cmdBuffer, startBinding, bindingCount, pBuffers, pOffsets);

		record->AddChunk(scope.Get());
		for(uint32_t i=0; i < bindingCount; i++)
			record->MarkResourceFrameReferenced(GetResID(pBuffers[i]), eFrameRef_Read);
	}
}


bool WrappedVulkan::Serialise_vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);
	SERIALISE_ELEMENT(VkIndexType, idxType, indexType);

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBindIndexBuffer(Unwrap(cmdBuffer), Unwrap(buffer), offs, idxType);

			m_PartialReplayData.state.ibuffer.buf = bufid;
			m_PartialReplayData.state.ibuffer.offs = offs;
			m_PartialReplayData.state.ibuffer.bytewidth = idxType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		// track this while reading, as we need to bind current topology & index byte width to draws
		m_PartialReplayData.state.ibuffer.bytewidth = idxType == VK_INDEX_TYPE_UINT32 ? 4 : 2;
		
		ObjDisp(cmdBuffer)->CmdBindIndexBuffer(Unwrap(cmdBuffer), Unwrap(buffer), offs, idxType);
	}

	return true;
}

void WrappedVulkan::vkCmdBindIndexBuffer(
    VkCmdBuffer                                 cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
	ObjDisp(cmdBuffer)->CmdBindIndexBuffer(Unwrap(cmdBuffer), Unwrap(buffer), offset, indexType);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BIND_INDEX_BUFFER);
		Serialise_vkCmdBindIndexBuffer(cmdBuffer, buffer, offset, indexType);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(buffer), eFrameRef_Read);
	}
}

bool WrappedVulkan::Serialise_vkCmdDraw(
	VkCmdBuffer cmdBuffer,
	uint32_t       firstVertex,
	uint32_t       vertexCount,
	uint32_t       firstInstance,
	uint32_t       instanceCount)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, firstVtx, firstVertex);
	SERIALISE_ELEMENT(uint32_t, vtxCount, vertexCount);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDraw(Unwrap(cmdBuffer), firstVtx, vtxCount, firstInst, instCount);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdDraw(Unwrap(cmdBuffer), firstVtx, vtxCount, firstInst, instCount);

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
	ObjDisp(cmdBuffer)->CmdDraw(Unwrap(cmdBuffer), firstVertex, vertexCount, firstInstance, instanceCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);

	SERIALISE_ELEMENT(VkTexFilter, f, filter);
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageBlit, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdBlitImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions, f);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(cmdBuffer)->CmdBlitImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions, f);
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
	ObjDisp(cmdBuffer)->CmdBlitImage(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions, filter);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BLIT_IMG);
		Serialise_vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions, filter);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResID(destImage));
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcImage));
	SERIALISE_ELEMENT(VkImageLayout, srclayout, srcImageLayout);
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destImage));
	SERIALISE_ELEMENT(VkImageLayout, dstlayout, destImageLayout);
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(srcid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(dstid);

		ObjDisp(cmdBuffer)->CmdCopyImage(Unwrap(cmdBuffer), Unwrap(srcImage), srclayout, Unwrap(destImage), dstlayout, count, regions);
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
	ObjDisp(cmdBuffer)->CmdCopyImage(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(COPY_IMG);
		Serialise_vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, destImage, destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);

		// VKTODOHIGH init states not implemented yet...
		//record->dirtied.insert(GetResID(destImage));
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(srcBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(destImage));
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);
		destImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(cmdBuffer)->CmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, count, regions);
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
	ObjDisp(cmdBuffer)->CmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(COPY_BUF2IMG);
		Serialise_vkCmdCopyBufferToImage(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destImage), destImageLayout, regionCount, pRegions);

		record->AddChunk(scope.Get());

		record->dirtied.insert(GetResID(destImage));
		record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destImage), eFrameRef_Write);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(destBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(srcImage));

	SERIALISE_ELEMENT(VkImageLayout, layout, srcImageLayout);

	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferImageCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyImageToBuffer(Unwrap(cmdBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer), count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcImage = GetResourceManager()->GetLiveHandle<VkImage>(imgid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdCopyImageToBuffer(Unwrap(cmdBuffer), Unwrap(srcImage), layout, Unwrap(destBuffer), count, regions);
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
	ObjDisp(cmdBuffer)->CmdCopyImageToBuffer(Unwrap(cmdBuffer), Unwrap(srcImage), srcImageLayout, Unwrap(destBuffer), regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(COPY_IMG2BUF);
		Serialise_vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcImage), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destBuffer), eFrameRef_Write);

		// VKTODOMED: need to dirty the memory bound to the buffer?
		record->dirtied.insert(GetResID(destBuffer));
	}
}

bool WrappedVulkan::Serialise_vkCmdCopyBuffer(
    VkCmdBuffer                                 cmdBuffer,
		VkBuffer                                    srcBuffer,
		VkBuffer                                    destBuffer,
		uint32_t                                    regionCount,
		const VkBufferCopy*                         pRegions)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, srcid, GetResID(srcBuffer));
	SERIALISE_ELEMENT(ResourceId, dstid, GetResID(destBuffer));
	
	SERIALISE_ELEMENT(uint32_t, count, regionCount);
	SERIALISE_ELEMENT_ARR(VkBufferCopy, regions, pRegions, count);
	
	if(m_State == EXECUTING)
	{
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdCopyBuffer(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		srcBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(srcid);
		destBuffer = GetResourceManager()->GetLiveHandle<VkBuffer>(dstid);

		ObjDisp(cmdBuffer)->CmdCopyBuffer(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), count, regions);
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
	ObjDisp(cmdBuffer)->CmdCopyBuffer(Unwrap(cmdBuffer), Unwrap(srcBuffer), Unwrap(destBuffer), regionCount, pRegions);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(COPY_BUF);
		Serialise_vkCmdCopyBuffer(cmdBuffer, srcBuffer, destBuffer, regionCount, pRegions);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(srcBuffer), eFrameRef_Read);
		record->MarkResourceFrameReferenced(GetResID(destBuffer), eFrameRef_Write);
		
		// VKTODOMED: need to dirty the memory bound to the buffer?
		record->dirtied.insert(GetResID(destBuffer));
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
	SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
	SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State == EXECUTING)
	{
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), layout, &col, count, ranges);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(cmdBuffer)->CmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), layout, &col, count, ranges);
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
	ObjDisp(cmdBuffer)->CmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), imageLayout, pColor, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR);
		Serialise_vkCmdClearColorImage(Unwrap(cmdBuffer), Unwrap(image), imageLayout, pColor, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, imgid, GetResID(image));
	SERIALISE_ELEMENT(VkImageLayout, l, imageLayout);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(byte, s, stencil);
	SERIALISE_ELEMENT(uint32_t, count, rangeCount);
	SERIALISE_ELEMENT_ARR(VkImageSubresourceRange, ranges, pRanges, count);
	
	if(m_State == EXECUTING)
	{
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearDepthStencilImage(Unwrap(cmdBuffer), Unwrap(image), l, d, s, count, ranges);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		image = GetResourceManager()->GetLiveHandle<VkImage>(imgid);

		ObjDisp(cmdBuffer)->CmdClearDepthStencilImage(Unwrap(cmdBuffer), Unwrap(image), l, d, s, count, ranges);
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
	ObjDisp(cmdBuffer)->CmdClearDepthStencilImage(Unwrap(cmdBuffer), Unwrap(image), imageLayout, depth, stencil, rangeCount, pRanges);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL);
		Serialise_vkCmdClearDepthStencilImage(cmdBuffer, image, imageLayout, depth, stencil, rangeCount, pRanges);

		record->AddChunk(scope.Get());
		record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, att, colorAttachment);
	SERIALISE_ELEMENT(VkImageLayout, layout, imageLayout);
	SERIALISE_ELEMENT(VkClearColorValue, col, *pColor);

	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkRect3D, rects, pRects, count);
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearColorAttachment(Unwrap(cmdBuffer), att, layout, &col, count, rects);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdClearColorAttachment(Unwrap(cmdBuffer), att, layout, &col, count, rects);

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
	ObjDisp(cmdBuffer)->CmdClearColorAttachment(Unwrap(cmdBuffer), colorAttachment, imageLayout, pColor, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(CLEAR_COLOR_ATTACH);
		Serialise_vkCmdClearColorAttachment(cmdBuffer, colorAttachment, imageLayout, pColor, rectCount, pRects);

		record->AddChunk(scope.Get());
		// VKTODOHIGH mark referenced the image under the attachment
		//record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkImageAspectFlags, asp, imageAspectMask);
	SERIALISE_ELEMENT(VkImageLayout, lay, imageLayout);
	SERIALISE_ELEMENT(float, d, depth);
	SERIALISE_ELEMENT(byte, s, stencil);
	SERIALISE_ELEMENT(uint32_t, count, rectCount);
	SERIALISE_ELEMENT_ARR(VkRect3D, rects, pRects, count);
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdClearDepthStencilAttachment(Unwrap(cmdBuffer), asp, lay, d, s, count, rects);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdClearDepthStencilAttachment(Unwrap(cmdBuffer), asp, lay, d, s, count, rects);
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
	ObjDisp(cmdBuffer)->CmdClearDepthStencilAttachment(Unwrap(cmdBuffer), imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(CLEAR_DEPTHSTENCIL_ATTACH);
		Serialise_vkCmdClearDepthStencilAttachment(cmdBuffer, imageAspectMask, imageLayout, depth, stencil, rectCount, pRects);

		record->AddChunk(scope.Get());
		// VKTODOHIGH mark referenced the image under the attachment
		//record->MarkResourceFrameReferenced(GetResID(image), eFrameRef_Write);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), src, dest, region, memCount, (const void **)&mems[0]);

			ResourceId cmd = GetResID(PartialCmdBuf());
			GetResourceManager()->RecordTransitions(m_CmdBufferInfo[cmd].imgtransitions, m_ImageInfo, (uint32_t)imTrans.size(), &imTrans[0]);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), src, dest, region, memCount, (const void **)&mems[0]);
		
		ResourceId cmd = GetResID(cmdBuffer);
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

	{
		// VKTODOLOW this should be a persistent per-thread array that resizes up
		// to a high water mark, so we don't have to allocate
		vector<VkImageMemoryBarrier> im;
		vector<VkBufferMemoryBarrier> buf;

		// ensure we don't resize while looping so we can take pointers
		im.reserve(memBarrierCount);
		buf.reserve(memBarrierCount);

		void **unwrappedBarriers = new void*[memBarrierCount];

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkGenericStruct *header = (VkGenericStruct *)ppMemBarriers[i];

			if(header->type == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
			{
				VkImageMemoryBarrier barrier = *(VkImageMemoryBarrier *)header;
				barrier.image = Unwrap(barrier.image);
				im.push_back(barrier);
				unwrappedBarriers[i] = &im.back();
			}
			else if(header->type == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
			{
				VkBufferMemoryBarrier barrier = *(VkBufferMemoryBarrier *)header;
				barrier.buffer = Unwrap(barrier.buffer);
				buf.push_back(barrier);
				unwrappedBarriers[i] = &buf.back();
			}
			else
			{
				unwrappedBarriers[i] = (void *)ppMemBarriers[i];
			}
		}

		ObjDisp(cmdBuffer)->CmdPipelineBarrier(Unwrap(cmdBuffer), srcStageMask, destStageMask, byRegion, memBarrierCount, unwrappedBarriers);

		SAFE_DELETE_ARRAY(unwrappedBarriers);
	}

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(PIPELINE_BARRIER);
		Serialise_vkCmdPipelineBarrier(Unwrap(cmdBuffer), srcStageMask, destStageMask, byRegion, memBarrierCount, ppMemBarriers);

		record->AddChunk(scope.Get());

		vector<VkImageMemoryBarrier> imTrans;

		for(uint32_t i=0; i < memBarrierCount; i++)
		{
			VkStructureType stype = ((VkGenericStruct *)ppMemBarriers[i])->type;

			if(stype == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
				imTrans.push_back(*((VkImageMemoryBarrier *)ppMemBarriers[i]));
		}
		
		ResourceId cmd = GetResID(cmdBuffer);
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
	return ObjDisp(instance)->DbgCreateMsgCallback(Unwrap(instance), msgFlags, pfnMsgCallback, pUserData, pMsgCallback);
}

VkResult WrappedVulkan::vkDbgDestroyMsgCallback(
	VkInstance                          instance,
	VkDbgMsgCallback                    msgCallback)
{
	return ObjDisp(instance)->DbgDestroyMsgCallback(Unwrap(instance), msgCallback);
}
	
bool WrappedVulkan::Serialise_vkCmdDbgMarkerBegin(
			VkCmdBuffer  cmdBuffer,
			const char*     pMarker)
{
	string name = pMarker ? string(pMarker) : "";

	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
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
	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		SCOPED_SERIALISE_CONTEXT(BEGIN_EVENT);
		Serialise_vkCmdDbgMarkerBegin(cmdBuffer, pMarker);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdDbgMarkerEnd(VkCmdBuffer cmdBuffer)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	
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
	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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

bool WrappedVulkan::ReleaseResource(WrappedVkRes *res)
{
	// VKTODOHIGH: release resource with device from resource record

	// VKTODOLOW - this will break if we have multiple devices and resources from each,
	// but that will likely break other things too.
	VkDevice dev = GetDev();
	const VkLayerDispatchTable *vt = ObjDisp(dev);

	WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
	WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;

	switch(IdentifyTypeByPtr(res))
	{
		case eResWSISwapChain:
			RDCERR("Should be no swapchain objects created on replay");
			break;

		case eResUnknown:
			RDCBREAK();
			// virtual object - nothing to do
			break;

		case eResPhysicalDevice:
		case eResQueue:
		case eResDescriptorSet:
			// nothing to do - destroyed with parent object
			break;

		case eResInstance:
		{
			VkInstance instance = disp->real.As<VkInstance>();
			dispatch_key key = get_dispatch_key(instance);
			ObjDisp(instance)->DestroyInstance(instance);
			destroy_dispatch_table(renderdoc_instance_table_map, key);
			break;
		}
		case eResDevice:
			vt->DestroyDevice(disp->real.As<VkDevice>());
			break;
		case eResDeviceMemory:
			vt->FreeMemory(dev, nondisp->real.As<VkDeviceMemory>());
			break;
		case eResBuffer:
			vt->DestroyBuffer(dev, nondisp->real.As<VkBuffer>());
			break;
		case eResBufferView:
			vt->DestroyBufferView(dev, nondisp->real.As<VkBufferView>());
			break;
		case eResImage:
			vt->DestroyImage(dev, nondisp->real.As<VkImage>());
			break;
		case eResImageView:
			vt->DestroyImageView(dev, nondisp->real.As<VkImageView>());
			break;
		case eResAttachmentView:
			vt->DestroyAttachmentView(dev, nondisp->real.As<VkAttachmentView>());
			break;
		case eResFramebuffer:
			vt->DestroyFramebuffer(dev, nondisp->real.As<VkFramebuffer>());
			break;
		case eResRenderPass:
			vt->DestroyRenderPass(dev, nondisp->real.As<VkRenderPass>());
			break;
		case eResShaderModule:
			vt->DestroyShaderModule(dev, nondisp->real.As<VkShaderModule>());
			break;
		case eResShader:
			vt->DestroyShader(dev, nondisp->real.As<VkShader>());
			break;
		case eResPipelineCache:
			vt->DestroyPipelineCache(dev, nondisp->real.As<VkPipelineCache>());
			break;
		case eResPipelineLayout:
			vt->DestroyPipelineLayout(dev, nondisp->real.As<VkPipelineLayout>());
			break;
		case eResPipeline:
			vt->DestroyPipeline(dev, nondisp->real.As<VkPipeline>());
			break;
		case eResSampler:
			vt->DestroySampler(dev, nondisp->real.As<VkSampler>());
			break;
		case eResDescriptorPool:
			vt->DestroyDescriptorPool(dev, nondisp->real.As<VkDescriptorPool>());
			break;
		case eResDescriptorSetLayout:
			vt->DestroyDescriptorSetLayout(dev, nondisp->real.As<VkDescriptorSetLayout>());
			break;
		case eResViewportState:
			vt->DestroyDynamicViewportState(dev, nondisp->real.As<VkDynamicViewportState>());
			break;
		case eResRasterState:
			vt->DestroyDynamicViewportState(dev, nondisp->real.As<VkDynamicViewportState>());
			break;
		case eResColorBlendState:
			vt->DestroyDynamicColorBlendState(dev, nondisp->real.As<VkDynamicColorBlendState>());
			break;
		case eResDepthStencilState:
			vt->DestroyDynamicDepthStencilState(dev, nondisp->real.As<VkDynamicDepthStencilState>());
			break;
		case eResCmdPool:
			vt->DestroyCommandPool(dev, nondisp->real.As<VkCmdPool>());
			break;
		case eResCmdBuffer:
			vt->DestroyCommandBuffer(dev, disp->real.As<VkCmdBuffer>());
			break;
		case eResFence:
			// VKTODOLOW
			//vt->DestroyFence(dev, nondisp->real.As<VkFence>());
			break;
		case eResSemaphore:
			vt->DestroySemaphore(dev, nondisp->real.As<VkSemaphore>());
			break;
	}

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
		record.frameInfo.immContextId = ResourceId();
		m_FrameRecord.push_back(record);

		GetResourceManager()->CreateInitialContents();
	}
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);
	
	SERIALISE_ELEMENT(ResourceId, bbid, GetResID(presentImage));

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

	m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedVulkan::AttemptCapture()
{
	m_State = WRITING_CAPFRAME;

	{
		RDCDEBUG("Attempting capture");

		//m_SuccessfulCapture = true;

		m_FrameCaptureRecord->LockChunks();
		while(m_FrameCaptureRecord->HasChunks())
		{
			Chunk *chunk = m_FrameCaptureRecord->GetLastChunk();

			SAFE_DELETE(chunk);
			m_FrameCaptureRecord->PopChunk();
		}
		m_FrameCaptureRecord->UnlockChunks();
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
		VkQueue q = GetQ();

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		VkResult vkr = ObjDisp(cmd)->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);
		
		VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		if(!imgTransitions.empty())
		{
			vector<void *> barriers;
			for(size_t i=0; i < imgTransitions.size(); i++)
				barriers.push_back(&imgTransitions[i]);
			ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), src_stages, dest_stages, false, (uint32_t)imgTransitions.size(), (const void *const *)&barriers[0]);
		}

		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(q)->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);
		// VKTODOMED while we're reusing cmd buffer, we have to ensure this one
		// is done before continuing
		vkr = ObjDisp(q)->QueueWaitIdle(Unwrap(q));
		RDCASSERT(vkr == VK_SUCCESS);
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

	ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));
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

	VkImageView fakeBBImView;

	{
		VkImageViewCreateInfo bbviewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL,
			Unwrap(m_FakeBBIm), VK_IMAGE_VIEW_TYPE_2D,
			(VkFormat)m_FakeBBFmt.rawType,
			{ VK_CHANNEL_SWIZZLE_R, VK_CHANNEL_SWIZZLE_G, VK_CHANNEL_SWIZZLE_B, VK_CHANNEL_SWIZZLE_A },
			{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1, }
		};

		// VKTODOMED used for texture display, but eventually will have to be created on the fly
		// for whichever image we're viewing (and cached), not specifically created here.
		VkResult vkr = ObjDisp(GetDev())->CreateImageView(Unwrap(GetDev()), &bbviewInfo, &fakeBBImView);
		RDCASSERT(vkr == VK_SUCCESS);
	}
	
	// VKTODOLOW maybe better place to put this?
	// VKTODOLOW leaking debug manager
	m_PhysicalReplayData[m_SwapPhysDevice].debugMan = new VulkanDebugManager(this, GetDev(), fakeBBImView);
}

void WrappedVulkan::ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial)
{
	m_State = readType;

	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, 1, false);
	RDCASSERT(header == CONTEXT_CAPTURE_HEADER);

	WrappedVulkan *context = this;

	Serialise_BeginCaptureFrame(!partial);
	
	ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

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
		ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(m_PartialReplayData.partialDevice));

		// deliberately call our own function, so this is destroyed as a wrapped object
		vkDestroyCommandBuffer(m_PartialReplayData.partialDevice, m_PartialReplayData.resultPartialCmdBuffer);
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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, firstIdx, firstIndex);
	SERIALISE_ELEMENT(uint32_t, idxCount, indexCount);
	SERIALISE_ELEMENT(int32_t, vtxOffs, vertexOffset);
	SERIALISE_ELEMENT(uint32_t, firstInst, firstInstance);
	SERIALISE_ELEMENT(uint32_t, instCount, instanceCount);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDrawIndexed(Unwrap(cmdBuffer), firstIdx, idxCount, vtxOffs, firstInst, instCount);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdDrawIndexed(Unwrap(cmdBuffer), firstIdx, idxCount, vtxOffs, firstInst, instCount);
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
	ObjDisp(cmdBuffer)->CmdDrawIndexed(Unwrap(cmdBuffer), firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT(uint32_t, strd, stride);

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDrawIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdDrawIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
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
	ObjDisp(cmdBuffer)->CmdDrawIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	SERIALISE_ELEMENT(uint32_t, cnt, count);
	SERIALISE_ELEMENT(uint32_t, strd, stride);

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDrawIndexedIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdDrawIndexedIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs, cnt, strd);
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
	ObjDisp(cmdBuffer)->CmdDrawIndexedIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offset, count, stride);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, X, x);
	SERIALISE_ELEMENT(uint32_t, Y, y);
	SERIALISE_ELEMENT(uint32_t, Z, z);

	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDispatch(Unwrap(cmdBuffer), x, y, z);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdDispatch(Unwrap(cmdBuffer), X, Y, Z);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatch(
	VkCmdBuffer cmdBuffer,
	uint32_t       x,
	uint32_t       y,
	uint32_t       z)
{
	ObjDisp(cmdBuffer)->CmdDispatch(Unwrap(cmdBuffer), x, y, z);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(ResourceId, bufid, GetResID(buffer));
	SERIALISE_ELEMENT(uint64_t, offs, offset);

	if(m_State == EXECUTING)
	{
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdDispatchIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);
		buffer = GetResourceManager()->GetLiveHandle<VkBuffer>(bufid);

		ObjDisp(cmdBuffer)->CmdDispatchIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offs);
	}

	return true;
}

void WrappedVulkan::vkCmdDispatchIndirect(
			VkCmdBuffer                                 cmdBuffer,
			VkBuffer                                    buffer,
			VkDeviceSize                                offset)
{
	ObjDisp(cmdBuffer)->CmdDispatchIndirect(Unwrap(cmdBuffer), Unwrap(buffer), offset);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

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
	return ObjDisp(physicalDevice)->GetPhysicalDeviceSurfaceSupportWSI(Unwrap(physicalDevice), queueFamilyIndex, pSurfaceDescription, pSupported);
}

VkResult WrappedVulkan::vkGetSurfaceInfoWSI(
		VkDevice                                 device,
		const VkSurfaceDescriptionWSI*           pSurfaceDescription,
		VkSurfaceInfoTypeWSI                     infoType,
		size_t*                                  pDataSize,
		void*                                    pData)
{
	return ObjDisp(device)->GetSurfaceInfoWSI(Unwrap(device), pSurfaceDescription, infoType, pDataSize, pData);
}

bool WrappedVulkan::Serialise_vkGetSwapChainInfoWSI(
		VkDevice                                 device,
    VkSwapChainWSI                           swapChain,
    VkSwapChainInfoTypeWSI                   infoType,
    size_t*                                  pDataSize,
    void*                                    pData)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(ResourceId, swapId, GetResID(swapChain));
	VkSwapChainImagePropertiesWSI *image = (VkSwapChainImagePropertiesWSI *)pData;
	SERIALISE_ELEMENT(size_t, idx, *pDataSize);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(image->image));

	if(m_State >= WRITING)
	{
		RDCASSERT(infoType == VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI);
	}

	if(m_State == READING)
	{
		// VKTODOLOW what if num images is less than on capture?
		RDCASSERT(idx < m_SwapChainInfo[swapId].images.size());
		GetResourceManager()->AddLiveResource(id, m_SwapChainInfo[swapId].images[idx].im);
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

	VkResult ret = ObjDisp(device)->GetSwapChainInfoWSI(Unwrap(device), Unwrap(swapChain), infoType, pDataSize, pData);

	if(infoType == VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI && pData && m_State >= WRITING)
	{
		VkSwapChainImagePropertiesWSI *images = (VkSwapChainImagePropertiesWSI *)pData;
		size_t numImages = (*pDataSize)/sizeof(VkSwapChainImagePropertiesWSI);

		for(size_t i=0; i < numImages; i++)
		{
			// these were all wrapped and serialised on swapchain create - we just have to
			// return the wrapped image in that case
			if(GetResourceManager()->HasWrapper(RealVkRes(images[i].image.handle)))
			{
				images[i].image = (VkImage)(uint64_t)GetResourceManager()->GetWrapper(RealVkRes(images[i].image.handle));
			}
			else
			{
				ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), images[i].image);

				if(m_State >= WRITING)
				{
					Chunk *chunk = NULL;

					{
						SCOPED_SERIALISE_CONTEXT(PRESENT_IMAGE);
						Serialise_vkGetSwapChainInfoWSI(device, swapChain, infoType, &i, (void *)&images[i]);

						chunk = scope.Get();
					}

					VkResourceRecord *record = GetResourceManager()->AddResourceRecord(images[i].image);
					record->AddChunk(chunk);

					// we invert the usual scheme - we make the swapchain record take parent refs
					// on these images, so that we can just ref the swapchain on present and pull
					// in all the images
					VkResourceRecord *swaprecord = GetRecord(swapChain);

					swaprecord->AddParent(record);
					// decrement refcount on swap images, so that they are only ref'd from the swapchain
					// (and will be deleted when it is deleted)
					record->Delete(GetResourceManager());
				}
				else
				{
					GetResourceManager()->AddLiveResource(id, images[i].image);
				}
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
	return ObjDisp(device)->AcquireNextImageWSI(Unwrap(device), Unwrap(swapChain), timeout, Unwrap(semaphore), pImageIndex);
}

bool WrappedVulkan::Serialise_vkCreateSwapChainWSI(
		VkDevice                                device,
		const VkSwapChainCreateInfoWSI*         pCreateInfo,
		VkSwapChainWSI*                         pSwapChain)
{
	SERIALISE_ELEMENT(ResourceId, devId, GetResID(device));
	SERIALISE_ELEMENT(VkSwapChainCreateInfoWSI, info, *pCreateInfo);
	SERIALISE_ELEMENT(ResourceId, id, GetResID(*pSwapChain));

	uint32_t numIms = 0;

	if(m_State >= WRITING)
	{
		VkResult vkr = VK_SUCCESS;

		size_t swapChainImagesSize;
		vkr = ObjDisp(device)->GetSwapChainInfoWSI(Unwrap(device), Unwrap(*pSwapChain), VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
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
		device = GetResourceManager()->GetLiveHandle<VkDevice>(devId);

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
			if(m_PhysicalReplayData[i].dev == device)
				m_SwapPhysDevice = (int)i;
		}

		for(uint32_t i=0; i < numSwapImages; i++)
		{
			VkDeviceMemory mem = VK_NULL_HANDLE;
			VkImage im = VK_NULL_HANDLE;

			VkResult vkr = ObjDisp(device)->CreateImage(Unwrap(device), &imInfo, &im);
			RDCASSERT(vkr == VK_SUCCESS);

			ResourceId liveId = GetResourceManager()->WrapResource(Unwrap(device), im);
			
			VkMemoryRequirements mrq = {0};

			vkr = ObjDisp(device)->GetImageMemoryRequirements(Unwrap(device), Unwrap(im), &mrq);
			RDCASSERT(vkr == VK_SUCCESS);
			
			VkMemoryAllocInfo allocInfo = {
				VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
				mrq.size, GetGPULocalMemoryIndex(mrq.memoryTypeBits),
			};

			vkr = ObjDisp(device)->AllocMemory(Unwrap(device), &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);
			
			GetResourceManager()->WrapResource(Unwrap(device), mem);

			vkr = ObjDisp(device)->BindImageMemory(Unwrap(device), Unwrap(im), Unwrap(mem), 0);
			RDCASSERT(vkr == VK_SUCCESS);

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
	VkResult ret = ObjDisp(device)->CreateSwapChainWSI(Unwrap(device), pCreateInfo, pSwapChain);
	
	if(ret == VK_SUCCESS)
	{
		ResourceId id = GetResourceManager()->WrapResource(Unwrap(device), *pSwapChain);
		
		if(m_State >= WRITING)
		{
			Chunk *chunk = NULL;

			{
				SCOPED_SERIALISE_CONTEXT(CREATE_SWAP_BUFFER);
				Serialise_vkCreateSwapChainWSI(device, pCreateInfo, pSwapChain);

				chunk = scope.Get();
			}

			VkResourceRecord *record = GetResourceManager()->AddResourceRecord(*pSwapChain);
			record->AddChunk(chunk);

			for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
			{
				if(m_PhysicalReplayData[i].dev == device)
					m_SwapPhysDevice = (int)i;
			}
			
			SwapInfo &swapInfo = m_SwapChainInfo[id];

			VkResult vkr = VK_SUCCESS;

			const VkLayerDispatchTable *vt = ObjDisp(device);

			{
				VkAttachmentDescription attDesc = {
					VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION, NULL,
					pCreateInfo->imageFormat, 1,
					VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				};

				VkAttachmentReference attRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

				VkSubpassDescription sub = {
					VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION, NULL,
					VK_PIPELINE_BIND_POINT_GRAPHICS, 0,
					0, NULL, // inputs
					1, &attRef, // color
					NULL, // resolve
					{ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED }, // depth-stencil
					0, NULL, // preserve
				};

				VkRenderPassCreateInfo rpinfo = {
					VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL,
					1, &attDesc,
					1, &sub,
					0, NULL, // dependencies
				};

				vkr = vt->CreateRenderPass(Unwrap(device), &rpinfo, &swapInfo.rp);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), swapInfo.rp);
			}

			{
				VkViewport vp = { 0.0f, 0.0f, (float)pCreateInfo->imageExtent.width, (float)pCreateInfo->imageExtent.height, 0.0f, 1.0f, };
				VkRect2D sc = { { 0, 0 }, { pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height } };

				VkDynamicViewportStateCreateInfo vpInfo = {
					VK_STRUCTURE_TYPE_DYNAMIC_VIEWPORT_STATE_CREATE_INFO, NULL,
					1, &vp, &sc
				};

				vkr = vt->CreateDynamicViewportState(Unwrap(device), &vpInfo, &swapInfo.vp);
				RDCASSERT(vkr == VK_SUCCESS);

				GetResourceManager()->WrapResource(Unwrap(device), swapInfo.vp);
			}

			// serialise out the swap chain images
			{
				size_t swapChainImagesSize;
				VkResult ret = vt->GetSwapChainInfoWSI(Unwrap(device), Unwrap(*pSwapChain), VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, NULL);
				RDCASSERT(ret == VK_SUCCESS);

				uint32_t numSwapImages = uint32_t(swapChainImagesSize)/sizeof(VkSwapChainImagePropertiesWSI);

				VkSwapChainImagePropertiesWSI* images = new VkSwapChainImagePropertiesWSI[numSwapImages];

				// go through our own function so we assign these images IDs
				ret = vkGetSwapChainInfoWSI(device, *pSwapChain, VK_SWAP_CHAIN_INFO_TYPE_IMAGES_WSI, &swapChainImagesSize, images);
				RDCASSERT(ret == VK_SUCCESS);

				for(uint32_t i=0; i < numSwapImages; i++)
				{
					SwapInfo::SwapImage &swapImInfo = swapInfo.images[i];

					// memory doesn't exist for genuine WSI created images
					swapImInfo.mem = VK_NULL_HANDLE;
					swapImInfo.im = images[i].image;

					ResourceId imid = GetResID(images[i].image);

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

					{
						VkAttachmentViewCreateInfo info = {
							VK_STRUCTURE_TYPE_ATTACHMENT_VIEW_CREATE_INFO, NULL,
							Unwrap(images[i].image), pCreateInfo->imageFormat, 0, 0, 1,
							0
						};

						vkr = vt->CreateAttachmentView(Unwrap(device), &info, &swapImInfo.view);
						RDCASSERT(vkr == VK_SUCCESS);

						GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.view);

						VkAttachmentBindInfo attBind = { swapImInfo.view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

						VkFramebufferCreateInfo fbinfo = {
							VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL,
							swapInfo.rp,
							1, &attBind,
							(uint32_t)pCreateInfo->imageExtent.width, (uint32_t)pCreateInfo->imageExtent.height, 1,
						};

						vkr = vt->CreateFramebuffer(Unwrap(device), &fbinfo, &swapImInfo.fb);
						RDCASSERT(vkr == VK_SUCCESS);

						GetResourceManager()->WrapResource(Unwrap(device), swapImInfo.fb);
					}
				}

				SAFE_DELETE_ARRAY(images);
			}
		}
		else
		{
			GetResourceManager()->AddLiveResource(id, *pSwapChain);
		}
	}

	return ret;
}

VkResult WrappedVulkan::vkQueuePresentWSI(
			VkQueue                                 queue,
			VkPresentInfoWSI*                       pPresentInfo)
{
	if(pPresentInfo->swapChainCount == 0)
		return VK_ERROR_INVALID_VALUE;

	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);
	
	if(m_State == WRITING_IDLE)
		RenderDoc::Inst().Tick();
	
	m_FrameCounter++; // first present becomes frame #1, this function is at the end of the frame

	if(pPresentInfo->swapChainCount > 1 && (m_FrameCounter % 100) == 0)
	{
		RDCWARN("Presenting multiple swapchains at once - only first will be processed");
	}
	
	// VKTODOLOW handle present info pNext
	RDCASSERT(pPresentInfo->pNext == NULL);
	
	ResourceId swapid = GetResID(pPresentInfo->swapChains[0]);

	const SwapInfo &swapInfo = m_SwapChainInfo[swapid];

	VkImage backbuffer = swapInfo.images[pPresentInfo->imageIndices[0]].im;
	
	// VKTODOLOW multiple windows/captures etc
	bool activeWindow = true; //RenderDoc::Inst().IsActiveWindow((ID3D11Device *)this, swapdesc.OutputWindow);

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
		}
		
		uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

		if(overlay & eRENDERDOC_Overlay_Enabled)
		{
			VkRenderPass rp = swapInfo.rp;
			VkDynamicViewportState vp = swapInfo.vp;
			VkFramebuffer fb = swapInfo.images[pPresentInfo->imageIndices[0]].fb;

			// VKTODOLOW only handling queue == GetQ()
			RDCASSERT(GetQ() == queue);
			VkQueue q = GetQ();

			VkLayerDispatchTable *vt = ObjDisp(GetDev());

			vt->QueueWaitIdle(Unwrap(q));

			TextPrintState textstate = { q, GetCmd(), rp, fb, vp, swapInfo.extent.width, swapInfo.extent.height };

			if(activeWindow)
			{
				vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetCaptureKeys();

				string overlayText = "Vulkan. ";

				for(size_t i=0; i < keys.size(); i++)
				{
					if(i > 0)
						overlayText += ", ";

					overlayText += ToStr::Get(keys[i]);
				}

				if(!keys.empty())
					overlayText += " to capture.";

				if(overlay & eRENDERDOC_Overlay_FrameNumber)
				{
					overlayText += StringFormat::Fmt(" Frame: %d.", m_FrameCounter);
				}
				if(overlay & eRENDERDOC_Overlay_FrameRate)
				{
					overlayText += StringFormat::Fmt(" %.2lf ms (%.2lf .. %.2lf) (%.0lf FPS)",
																					m_AvgFrametime, m_MinFrametime, m_MaxFrametime, 1000.0f/m_AvgFrametime);
				}

				float y=0.0f;

				if(!overlayText.empty())
				{
					GetDebugManager()->RenderText(textstate, 0.0f, y, overlayText.c_str());
					y += 1.0f;
				}

				if(overlay & eRENDERDOC_Overlay_CaptureList)
				{
					GetDebugManager()->RenderText(textstate, 0.0f, y, "%d Captures saved.\n", (uint32_t)m_FrameRecord.size());
					y += 1.0f;

					uint64_t now = Timing::GetUnixTimestamp();
					for(size_t i=0; i < m_FrameRecord.size(); i++)
					{
						if(now - m_FrameRecord[i].frameInfo.captureTime < 20)
						{
							GetDebugManager()->RenderText(textstate, 0.0f, y, "Captured frame %d.\n", m_FrameRecord[i].frameInfo.frameNumber);
							y += 1.0f;
						}
					}
				}

				// VKTODOLOW failed frames

#if !defined(RELEASE)
				GetDebugManager()->RenderText(textstate, 0.0f, y, "%llu chunks - %.2f MB", Chunk::NumLiveChunks(), float(Chunk::TotalMem())/1024.0f/1024.0f);
				y += 1.0f;
#endif
			}
			else
			{
				vector<RENDERDOC_InputButton> keys = RenderDoc::Inst().GetFocusKeys();

				string str = "Vulkan. Inactive swapchain.";

				for(size_t i=0; i < keys.size(); i++)
				{
					if(i == 0)
						str += " ";
					else
						str += ", ";

					str += ToStr::Get(keys[i]);
				}

				if(!keys.empty())
					str += " to cycle between swapchains";

				GetDebugManager()->RenderText(textstate, 0.0f, 0.0f, str.c_str());
			}
		}
	}
	
	// kill any current capture
	if(m_State == WRITING_CAPFRAME)
	{
		//if(HasSuccessfulCapture())
		{
			RDCLOG("Finished capture, Frame %u", m_FrameCounter);

			GetResourceManager()->MarkResourceFrameReferenced(swapid, eFrameRef_Read);

			EndCaptureFrame(backbuffer);
			FinishCapture();

			byte *thpixels = NULL;
			uint32_t thwidth = 0;
			uint32_t thheight = 0;

			// gather backbuffer screenshot
			const int32_t maxSize = 1024;

			// VKTODOLOW split this out properly into begin/end frame capture
			if(1)//if(wnd)
			{
				VkDevice dev = GetDev();
				VkQueue q = GetQ();
				VkCmdBuffer cmd = GetCmd();

				const VkLayerDispatchTable *vt = ObjDisp(dev);

				// VKTODOLOW idle all devices? or just the device for this queue?
				vt->DeviceWaitIdle(Unwrap(dev));

				// since these objects are very short lived (only this scope), we
				// don't wrap them.
				VkImage readbackIm = VK_NULL_HANDLE;
				VkDeviceMemory readbackMem = VK_NULL_HANDLE;

				VkResult vkr = VK_SUCCESS;

				// create identical image
				VkImageCreateInfo imInfo = {
					/*.sType =*/ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					/*.pNext =*/ NULL,
					/*.imageType =*/ VK_IMAGE_TYPE_2D,
					/*.format =*/ swapInfo.format,
					/*.extent =*/ { swapInfo.extent.width, swapInfo.extent.height, 1 },
					/*.mipLevels =*/ 1,
					/*.arraySize =*/ 1,
					/*.samples =*/ 1,
					/*.tiling =*/ VK_IMAGE_TILING_LINEAR,
					/*.usage =*/
					VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
					/*.flags =*/ 0,
				};
				vt->CreateImage(Unwrap(dev), &imInfo, &readbackIm);
				RDCASSERT(vkr == VK_SUCCESS);

				VkMemoryRequirements mrq;
				vkr = vt->GetImageMemoryRequirements(Unwrap(dev), readbackIm, &mrq);
				RDCASSERT(vkr == VK_SUCCESS);

				VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR, 0, 0 };
				VkSubresourceLayout layout = { 0 };
				vt->GetImageSubresourceLayout(Unwrap(dev), readbackIm, &subr, &layout);

				// allocate readback memory
				VkMemoryAllocInfo allocInfo = {
					VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
					mrq.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
				};

				vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &readbackMem);
				RDCASSERT(vkr == VK_SUCCESS);
				vkr = vt->BindImageMemory(Unwrap(dev), readbackIm, readbackMem, 0);
				RDCASSERT(vkr == VK_SUCCESS);

				VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

				// do image copy
				vkr = vt->ResetCommandBuffer(Unwrap(cmd), 0);
				RDCASSERT(vkr == VK_SUCCESS);
				vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
				RDCASSERT(vkr == VK_SUCCESS);

				VkImageCopy cpy = {
					subr,	{ 0, 0, 0 },
					subr,	{ 0, 0, 0 },
					{ imInfo.extent.width, imInfo.extent.height, 1 },
				};

				// VKTODOLOW back buffer must be in this layout right?
				VkImageMemoryBarrier bbTrans = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0, VK_IMAGE_LAYOUT_PRESENT_SOURCE_WSI, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
					0, 0, Unwrap(backbuffer),
					{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

				VkImageMemoryBarrier readTrans = {
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
					0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
					0, 0, readbackIm, // was never wrapped
					{ VK_IMAGE_ASPECT_COLOR, 0, 1, 0, 1 } };

				VkImageMemoryBarrier *barriers[] = {
					&bbTrans,
					&readTrans,
				};

				vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, (void **)barriers);

				vt->CmdCopyImage(Unwrap(cmd), Unwrap(backbuffer), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, readbackIm, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &cpy);

				// transition backbuffer back
				std::swap(bbTrans.oldLayout, bbTrans.newLayout);

				// VKTODOLOW find out correct image layout for reading back
				readTrans.oldLayout = readTrans.newLayout;
				readTrans.newLayout = VK_IMAGE_LAYOUT_GENERAL;

				vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, (void **)barriers);

				vkr = vt->EndCommandBuffer(Unwrap(cmd));
				RDCASSERT(vkr == VK_SUCCESS);

				vkr = vt->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
				RDCASSERT(vkr == VK_SUCCESS);

				// wait queue idle
				vt->QueueWaitIdle(Unwrap(q));

				// map memory and readback
				byte *pData = NULL;
				vkr = vt->MapMemory(Unwrap(dev), readbackMem, 0, 0, 0, (void **)&pData);
				RDCASSERT(vkr == VK_SUCCESS);

				RDCASSERT(pData != NULL);

				// point sample info into raw buffer
				{
					ResourceFormat fmt = MakeResourceFormat(imInfo.format);

					byte *data = (byte *)pData;

					data += layout.offset;

					float widthf = float(imInfo.extent.width);
					float heightf = float(imInfo.extent.height);

					float aspect = widthf/heightf;

					thwidth = RDCMIN(maxSize, imInfo.extent.width);
					thwidth &= ~0x7; // align down to multiple of 8
					thheight = uint32_t(float(thwidth)/aspect);

					thpixels = new byte[3*thwidth*thheight];

					uint32_t stride = fmt.compByteWidth*fmt.compCount;

					bool buf1010102 = false;
					bool bufBGRA = false;

					if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
					{
						stride = 4;
						buf1010102 = true;
					}
					if(fmt.special && fmt.specialFormat == eSpecial_B8G8R8A8)
					{
						stride = 4;
						bufBGRA = true;
					}

					byte *dst = thpixels;

					for(uint32_t y=0; y < thheight; y++)
					{
						for(uint32_t x=0; x < thwidth; x++)
						{
							float xf = float(x)/float(thwidth);
							float yf = float(y)/float(thheight);

							byte *src = &data[ stride*uint32_t(xf*widthf) + layout.rowPitch*uint32_t(yf*heightf) ];

							if(buf1010102)
							{
								uint32_t *src1010102 = (uint32_t *)src;
								Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
								dst[0] = (byte)(unorm.x*255.0f);
								dst[1] = (byte)(unorm.y*255.0f);
								dst[2] = (byte)(unorm.z*255.0f);
							}
							else if(bufBGRA)
							{
								dst[0] = src[2];
								dst[1] = src[1];
								dst[2] = src[0];
							}
							else if(fmt.compByteWidth == 2) // R16G16B16A16 backbuffer
							{
								uint16_t *src16 = (uint16_t *)src;

								float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
								float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
								float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

								if(linearR < 0.0031308f) dst[0] = byte(255.0f*(12.92f * linearR));
								else                     dst[0] = byte(255.0f*(1.055f * powf(linearR, 1.0f/2.4f) - 0.055f));

								if(linearG < 0.0031308f) dst[1] = byte(255.0f*(12.92f * linearG));
								else                     dst[1] = byte(255.0f*(1.055f * powf(linearG, 1.0f/2.4f) - 0.055f));

								if(linearB < 0.0031308f) dst[2] = byte(255.0f*(12.92f * linearB));
								else                     dst[2] = byte(255.0f*(1.055f * powf(linearB, 1.0f/2.4f) - 0.055f));
							}
							else
							{
								dst[0] = src[0];
								dst[1] = src[1];
								dst[2] = src[2];
							}

							dst += 3;
						}
					}
				}

				vkr = vt->UnmapMemory(Unwrap(dev), readbackMem);
				RDCASSERT(vkr == VK_SUCCESS);

				// delete all
				vkr = vt->DestroyImage(Unwrap(dev), readbackIm);
				RDCASSERT(vkr == VK_SUCCESS);
				vkr = vt->FreeMemory(Unwrap(dev), readbackMem);
				RDCASSERT(vkr == VK_SUCCESS);
			}

			byte *jpgbuf = NULL;
			int len = thwidth*thheight;

			// VKTODOLOW split this out properly into begin/end frame capture
			if(1)//if(wnd)
			{
				jpgbuf = new byte[len];

				jpge::params p;

				p.m_quality = 40;

				bool success = jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

				if(!success)
				{
					RDCERR("Failed to compress to jpg");
					SAFE_DELETE_ARRAY(jpgbuf);
					thwidth = 0;
					thheight = 0;
				}
			}

			Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

			{
				SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

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
				m_FrameCaptureRecord->Insert(recordlist);

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
		record.frameInfo.captureTime = Timing::GetUnixTimestamp();
		m_FrameRecord.push_back(record);

		GetResourceManager()->ClearReferencedResources();

		GetResourceManager()->MarkResourceFrameReferenced(m_InstanceRecord->GetResourceID(), eFrameRef_Read);
		GetResourceManager()->PrepareInitialContents();
		
		AttemptCapture();
		BeginCaptureFrame();

		RDCLOG("Starting capture, frame %u", m_FrameCounter);
	}
	
	return ObjDisp(queue)->QueuePresentWSI(Unwrap(queue), pPresentInfo);
}

bool WrappedVulkan::Prepare_InitialState(WrappedVkRes *res)
{
	ResourceId id = GetResourceManager()->GetID(res);

	RDCDEBUG("Prepare_InitialState %llu", id);

	VkResourceType type = IdentifyTypeByPtr(res);
	
	if(type == eResDescriptorSet)
	{
		VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
		const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[record->layout];

		uint32_t numElems = 0;
		for(size_t i=0; i < layout.bindings.size(); i++)
			numElems += layout.bindings[i].arraySize;

		VkDescriptorInfo *info = (VkDescriptorInfo *)Serialiser::AllocAlignedBuffer(sizeof(VkDescriptorInfo)*numElems);
		RDCEraseMem(info, sizeof(VkDescriptorInfo)*numElems);

		uint32_t e=0;
		for(size_t i=0; i < layout.bindings.size(); i++)
			for(uint32_t b=0; b < layout.bindings[i].arraySize; b++)
				info[e++] = record->descBindings[i][b];

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, 0, (byte *)info));
		return true;
	}
	else if(type == eResDeviceMemory)
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

		vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(d), mem);

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		vkr = ObjDisp(d)->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			meminfo.size, VK_BUFFER_USAGE_GENERAL, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		// since these are very short lived, they are not wrapped
		VkBuffer srcBuf, dstBuf;

		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &srcBuf);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, ToHandle<VkDeviceMemory>(res), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, mem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { 0, 0, meminfo.size };

		ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

		vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOMED would be nice to store a fence too at this point
		// so we can sync on that on serialise rather than syncing
		// every time.
		ObjDisp(d)->QueueWaitIdle(Unwrap(q));

		ObjDisp(d)->DestroyBuffer(Unwrap(d), srcBuf);
		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);

		GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(mem), (uint32_t)meminfo.size, NULL));

		return true;
	}
	else if(type == eResImage)
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
		RDCERR("Unhandled resource type %d", type);
	}

	return false;
}

bool WrappedVulkan::Serialise_InitialState(WrappedVkRes *res)
{
	SERIALISE_ELEMENT(VkResourceType, type, IdentifyTypeByPtr(res));
	SERIALISE_ELEMENT(ResourceId, id, GetResourceManager()->GetID(res));

	if(m_State < WRITING) res = GetResourceManager()->GetLiveResource(id);
	
	if(m_State >= WRITING)
	{
		VulkanResourceManager::InitialContentData initContents = GetResourceManager()->GetInitialContents(id);

		if(type == eResDescriptorSet)
		{
			VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
			const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[record->layout];

			VkDescriptorInfo *info = (VkDescriptorInfo *)initContents.blob;

			uint32_t numElems = 0;
			for(size_t i=0; i < layout.bindings.size(); i++)
				numElems += layout.bindings[i].arraySize;

			m_pSerialiser->SerialiseComplexArray("Bindings", info, numElems);
		}
		else if(type == eResImage || type == eResDeviceMemory)
		{
			VkDevice d = GetDev();

			byte *ptr = NULL;
			ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource), 0, 0, 0, (void **)&ptr);

			size_t dataSize = (size_t)initContents.num;

			m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

			ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource));
		}
	}
	else
	{
		if(type == eResDescriptorSet)
		{
			const VulkanCreationInfo::DescSetLayout &layout = m_CreationInfo.m_DescSetLayout[ m_DescriptorSetInfo[id].layout ];

			uint32_t numElems;
			VkDescriptorInfo *bindings = NULL;

			m_pSerialiser->SerialiseComplexArray("Bindings", bindings, numElems);

			uint32_t numBinds = (uint32_t)layout.bindings.size();

			// allocate memory to keep the descriptorinfo structures around, as well as a WriteDescriptorSet array
			byte *blob = Serialiser::AllocAlignedBuffer(sizeof(VkDescriptorInfo)*numElems + sizeof(VkWriteDescriptorSet)*numBinds);

			VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)blob;
			VkDescriptorInfo *info = (VkDescriptorInfo *)(writes + numBinds);
			memcpy(info, bindings, sizeof(VkDescriptorInfo)*numElems);

			for(uint32_t i=0; i < numBinds; i++)
			{
				writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[i].pNext = NULL;

				// update whole element (array or single)
				writes[i].destSet = ToHandle<VkDescriptorSet>(res);
				writes[i].destBinding = i;
				writes[i].destArrayElement = 0;
				writes[i].count = layout.bindings[i].arraySize;
				writes[i].descriptorType = layout.bindings[i].descriptorType;
				writes[i].pDescriptors = info;

				info += layout.bindings[i].arraySize;
			}

			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, numBinds, blob));
		}
		else if(type == eResImage || type == eResDeviceMemory)
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

			vkr = ObjDisp(d)->AllocMemory(Unwrap(d), &allocInfo, &mem);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(d), mem);

			VkBufferCreateInfo bufInfo = {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
				dataSize, VK_BUFFER_USAGE_GENERAL, 0,
				VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			};

			VkBuffer buf;

			vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &buf);
			RDCASSERT(vkr == VK_SUCCESS);

			GetResourceManager()->WrapResource(Unwrap(d), buf);

			vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
			RDCASSERT(vkr == VK_SUCCESS);

			byte *ptr = NULL;
			ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, 0, 0, (void **)&ptr);

			// VKTODOLOW could deserialise directly into this ptr if we serialised
			// size separately.
			memcpy(ptr, data, dataSize);

			ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

			// VKTODOMED leaking the memory here! needs to be cleaned up with the buffer
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(GetWrapped(buf), eInitialContents_Copy, NULL));
		}
	}

	return true;
}

void WrappedVulkan::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
{
	VkResourceType type = IdentifyTypeByPtr(live);
	
	if(type == eResDescriptorSet)
	{
		RDCERR("Unexpected attempt to create initial state for descriptor set");
	}
	else if(type == eResImage)
	{
		RDCUNIMPLEMENTED("image initial states not implemented");

		if(m_ImageInfo.find(id) == m_ImageInfo.end())
		{
			RDCERR("Couldn't find image info");
			return;
		}

		ImgState &img = m_ImageInfo[id];

		if(img.subresourceStates[0].range.aspect == VK_IMAGE_ASPECT_COLOR)
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearColorImage, NULL));
		else
			GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(NULL, eInitialContents_ClearDepthStencilImage, NULL));
	}
	else if(type == eResDeviceMemory)
	{
		RDCERR("Unexpected attempt to create initial state for memory");
	}
	else if(type == eResFramebuffer)
	{
		RDCWARN("Framebuffer without initial state! should clear all attachments");
	}
	else
	{
		RDCERR("Unhandled resource type %d", type);
	}
}

void WrappedVulkan::Apply_InitialState(WrappedVkRes *live, VulkanResourceManager::InitialContentData initial)
{
	VkResourceType type = IdentifyTypeByPtr(live);
	
	ResourceId id = GetResourceManager()->GetID(live);

	if(type == eResDescriptorSet)
	{
		VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)initial.blob;

		VkResult vkr = ObjDisp(GetDev())->UpdateDescriptorSets(Unwrap(GetDev()), initial.num, writes, 0, NULL);
		RDCASSERT(vkr == VK_SUCCESS);

		// need to blat over the current descriptor set contents, so these are available
		// when we want to fetch pipeline state
		vector<VkDescriptorInfo *> &bindings = m_DescriptorSetInfo[GetResourceManager()->GetOriginalID(id)].currentBindings;

		for(uint32_t i=0; i < initial.num; i++)
		{
			RDCASSERT(writes[i].destBinding < bindings.size());
			RDCASSERT(writes[i].destArrayElement == 0);

			VkDescriptorInfo *bind = bindings[writes[i].destBinding];

			for(uint32_t d=0; d < writes[i].count; d++)
				bind[d] = writes[i].pDescriptors[d];
		}
	}
	else if(type == eResDeviceMemory)
	{
		if(m_MemoryInfo.find(id) == m_MemoryInfo.end())
		{
			RDCERR("Couldn't find memory info");
			return;
		}

		MemState &meminfo = m_MemoryInfo[id];
		
		VkBuffer srcBuf = (VkBuffer)(uint64_t)initial.resource;
		VkDeviceMemory dstMem = (VkDeviceMemory)(uint64_t)live; // maintain the wrapping, for consistency

		VkResult vkr = VK_SUCCESS;

		VkDevice d = GetDev();
		VkQueue q = GetQ();
		VkCmdBuffer cmd = GetCmd();

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };
		
		vkr = ObjDisp(cmd)->ResetCommandBuffer(Unwrap(cmd), 0);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCreateInfo bufInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL,
			meminfo.size, VK_BUFFER_USAGE_GENERAL, 0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
		};

		// since this is short lived it isn't wrapped. Note that we want
		// to cache this up front, so it will then be wrapped
		VkBuffer dstBuf;
		
		// VKTODOMED this should be created once up front, not every time
		vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, &dstBuf);
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, dstMem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkBufferCopy region = { 0, 0, meminfo.size };

		ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), dstBuf, 1, &region);
	
		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		vkr = ObjDisp(q)->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
		RDCASSERT(vkr == VK_SUCCESS);

		// VKTODOMED would be nice to store a fence too at this point
		// so we can sync on that on serialise rather than syncing
		// every time.
		ObjDisp(q)->QueueWaitIdle(Unwrap(q));

		ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf);
	}
	else if(type == eResImage)
	{
		// VKTODOHIGH: need to copy initial copy to live
		RDCUNIMPLEMENTED("image initial states not implemented");
	}
	else
	{
		RDCERR("Unhandled resource type %d", type);
	}
}

void WrappedVulkan::ProcessChunk(uint64_t offset, VulkanChunkType context)
{
	switch(context)
	{
	case DEVICE_INIT:
		{
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
			m_FakeBBIm = GetResourceManager()->GetLiveHandle<VkImage>(bbid);
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
			Serialise_InitialState(NULL);
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

			VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

			VkResult vkr = ObjDisp(cmd)->ResetCommandBuffer(Unwrap(cmd), 0);
			RDCASSERT(vkr == VK_SUCCESS);
			vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
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
			t.image = Unwrap(m_FakeBBIm);
			t.oldLayout = st.subresourceStates[0].state;
			t.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			t.subresourceRange = st.subresourceStates[0].range;

			void *barrier = (void *)&t;

			st.subresourceStates[0].state = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 1, (void **)&barrier);

			VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f, } };
			ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), Unwrap(m_FakeBBIm), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &clearColor, 1, &t.subresourceRange);

			vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
			RDCASSERT(vkr == VK_SUCCESS);

			vkr = ObjDisp(q)->QueueSubmit(q, 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
			RDCASSERT(vkr == VK_SUCCESS);
			// VKTODOMED while we're reusing cmd buffer, we have to ensure this one
			// is done before continuing
			vkr = ObjDisp(q)->QueueWaitIdle(Unwrap(q));
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
	m_AddedDrawcall = true;

	WrappedVulkan *context = this;

	FetchDrawcall draw = d;
	draw.eventID = m_CurEventID;
	draw.drawcallID = m_CurDrawcallID;

	for(int i=0; i < 8; i++)
		draw.outputs[i] = ResourceId();

	draw.depthOut = ResourceId();

	ResourceId pipe = m_PartialReplayData.state.graphics.pipeline;
	if(pipe != ResourceId())
		draw.topology = MakePrimitiveTopology(m_CreationInfo.m_Pipeline[pipe].topology, m_CreationInfo.m_Pipeline[pipe].patchControlPoints);
	else
		draw.topology = eTopology_Unknown;

	draw.indexByteWidth = m_PartialReplayData.state.ibuffer.bytewidth;

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

void WrappedVulkan::AddEvent(VulkanChunkType type, string description)
{
	FetchAPIEvent apievent;

	apievent.context = ResourceId();
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
