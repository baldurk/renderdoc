/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

const char *VkChunkNames[] =
{
	"WrappedVulkan::Initialisation",
	"vkCreateInstance",
	"vkEnumeratePhysicalDevices",
	"vkCreateDevice",
	"vkGetDeviceQueue",
	
	"vkAllocMemory",
	"vkUnmapMemory",
	"vkFlushMappedMemoryRanges",
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
	"vkCreateDepthTargetView",
	"vkCreateSampler",
	"vkCreateShaderModule",
	"vkCreatePipelineLayout",
	"vkCreatePipelineCache",
	"vkCreateGraphicsPipelines",
	"vkCreateComputePipelines",
	"vkGetSwapchainImagesKHR",

	"vkCreateSemaphore",
	"vkCreateFence",
	"vkGetFenceStatus",
	"vkResetFences",
	"vkWaitForFences",
	
	"vkCreateEvent",
	"vkGetEventStatus",
	"vkSetEvent",
	"vkResetEvent",

	"vkCreateQueryPool",

	"vkAllocDescriptorSets",
	"vkUpdateDescriptorSets",

	"vkBeginCommandBuffer",
	"vkEndCommandBuffer",

	"vkQueueWaitIdle",
	"vkDeviceWaitIdle",

	"vkQueueSubmit",
	"vkBindBufferMemory",
	"vkBindImageMemory",
	
	"vkQueueBindSparse",

	"vkCmdBeginRenderPass",
	"vkCmdNextSubpass",
	"vkCmdExecuteCommands",
	"vkCmdEndRenderPass",

	"vkCmdBindPipeline",

	"vkCmdSetViewport",
	"vkCmdSetScissor",
	"vkCmdSetLineWidth",
	"vkCmdSetDepthBias",
	"vkCmdSetBlendConstants",
	"vkCmdSetDepthBounds",
	"vkCmdSetStencilCompareMask",
	"vkCmdSetStencilWriteMask",
	"vkCmdSetStencilReference",

	"vkCmdBindDescriptorSet",
	"vkCmdBindVertexBuffers",
	"vkCmdBindIndexBuffer",
	"vkCmdCopyBufferToImage",
	"vkCmdCopyImageToBuffer",
	"vkCmdCopyBuffer",
	"vkCmdCopyImage",
	"vkCmdBlitImage",
	"vkCmdResolveImage",
	"vkCmdUpdateBuffer",
	"vkCmdFillBuffer",
	"vkCmdPushConstants",

	"vkCmdClearColorImage",
	"vkCmdClearDepthStencilImage",
	"vkCmdClearAttachments",
	"vkCmdPipelineBarrier",

	"vkCmdWriteTimestamp",
	"vkCmdCopyQueryPoolResults",
	"vkCmdBeginQuery",
	"vkCmdEndQuery",
	"vkCmdResetQueryPool",

	"vkCmdSetEvent",
	"vkCmdResetEvent",
	"vkCmdWaitEvents",

	"vkCmdDraw",
	"vkCmdDrawIndirect",
	"vkCmdDrawIndexed",
	"vkCmdDrawIndexedIndirect",
	"vkCmdDispatch",
	"vkCmdDispatchIndirect",
	
	"vkCmdDbgMarkerBegin",
	"vkCmdDbgMarker", // no equivalent function at the moment
	"vkCmdDbgMarkerEnd",

	"vkDbgSetObjectName",

	"vkCreateSwapchainKHR",

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
	Serialiser *localSerialiser = GetSerialiser();

	SERIALISE_ELEMENT(uint32_t, ver, VK_SERIALISE_VERSION); SerialiseVersion = ver;

	if(ver != VK_SERIALISE_VERSION)
	{
		RDCERR("Incompatible Vulkan serialise version, expected %d got %d", VK_SERIALISE_VERSION, ver);
		return eReplayCreate_APIIncompatibleVersion;
	}

	localSerialiser->Serialise("AppName", AppName);
	localSerialiser->Serialise("EngineName", EngineName);
	localSerialiser->Serialise("AppVersion", AppVersion);
	localSerialiser->Serialise("EngineVersion", EngineVersion);
	localSerialiser->Serialise("APIVersion", APIVersion);

	localSerialiser->Serialise("Layers", Layers);
	localSerialiser->Serialise("Extensions", Extensions);

	localSerialiser->Serialise("InstanceID", InstanceID);

	return eReplayCreate_Success;
}

void VkInitParams::Set(const VkInstanceCreateInfo* pCreateInfo, ResourceId inst)
{
	RDCASSERT(pCreateInfo);

	if(pCreateInfo->pApplicationInfo)
	{
		// we don't support any extensions on appinfo structure
		RDCASSERT(pCreateInfo->pApplicationInfo->pNext == NULL);

		AppName = pCreateInfo->pApplicationInfo->pApplicationName ? pCreateInfo->pApplicationInfo->pApplicationName : "";
		EngineName = pCreateInfo->pApplicationInfo->pEngineName ? pCreateInfo->pApplicationInfo->pEngineName : "";

		AppVersion = pCreateInfo->pApplicationInfo->applicationVersion;
		EngineVersion = pCreateInfo->pApplicationInfo->engineVersion;
		APIVersion = pCreateInfo->pApplicationInfo->apiVersion;
	}
	else
	{
		AppName = "";
		EngineName = "";

		AppVersion = 0;
		EngineVersion = 0;
		APIVersion = 0;
	}

	Layers.resize(pCreateInfo->enabledLayerCount);
	Extensions.resize(pCreateInfo->enabledExtensionCount);

	for(uint32_t i=0; i < pCreateInfo->enabledLayerCount; i++)
		Layers[i] = pCreateInfo->ppEnabledLayerNames[i];

	for(uint32_t i=0; i < pCreateInfo->enabledExtensionCount; i++)
		Extensions[i] = pCreateInfo->ppEnabledExtensionNames[i];

	InstanceID = inst;
}

WrappedVulkan::WrappedVulkan(const char *logFilename)
	: m_RenderState(&m_CreationInfo)
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

	InitSPIRVCompiler();
	RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);

	m_Replay.SetDriver(this);

	m_FrameCounter = 0;

	m_AppControlledCapture = false;

	m_FrameTimer.Restart();

	threadSerialiserTLSSlot = Threading::AllocateTLSSlot();
	tempMemoryTLSSlot = Threading::AllocateTLSSlot();

	m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;
	
	m_RootEventID = 1;
	m_RootDrawcallID = 1;
	m_FirstEventID = 0;
	m_LastEventID = ~0U;

	m_DrawcallCallback = NULL;

	m_LastCmdBufferID = ResourceId();

	m_PartialReplayData.renderPassActive = false;
	m_PartialReplayData.resultPartialCmdBuffer = VK_NULL_HANDLE;
	m_PartialReplayData.outsideCmdBuffer = VK_NULL_HANDLE;
	m_PartialReplayData.partialParent = ResourceId();
	m_PartialReplayData.baseEvent = 0;

	m_DrawcallStack.push_back(&m_ParentDrawcall);

	m_ResourceManager = new VulkanResourceManager(m_State, m_pSerialiser, this);

	m_pSerialiser->SetUserData(m_ResourceManager);
	m_RenderState.m_ResourceManager = GetResourceManager();

	m_HeaderChunk = NULL;

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
		m_FrameCaptureRecord->DataInSerialiser = false;
		m_FrameCaptureRecord->Length = 0;
		m_FrameCaptureRecord->SpecialResource = true;
	}
	else
	{
		m_FrameCaptureRecord = NULL;

		ResourceIDGen::SetReplayResourceIDs();
	}
	
	m_pSerialiser->SetChunkNameLookup(&GetChunkName);

	//////////////////////////////////////////////////////////////////////////
	// Compile time asserts

	RDCCOMPILE_ASSERT(ARRAY_COUNT(VkChunkNames) == NUM_VULKAN_CHUNKS-FIRST_CHUNK_ID, "Not right number of chunk names");
}

WrappedVulkan::~WrappedVulkan()
{
	// records must be deleted before resource manager shutdown
	if(m_FrameCaptureRecord)
	{
		RDCASSERT(m_FrameCaptureRecord->GetRefCount() == 1);
		m_FrameCaptureRecord->Delete(GetResourceManager());
		m_FrameCaptureRecord = NULL;
	}

	// in case the application leaked some objects, avoid crashing trying
	// to release them ourselves by clearing the resource manager.
	// In a well-behaved application, this should be a no-op.
	m_ResourceManager->ClearWithoutReleasing();
	SAFE_DELETE(m_ResourceManager);
		
	SAFE_DELETE(m_pSerialiser);

	for(size_t i=0; i < m_MemIdxMaps.size(); i++)
		delete[] m_MemIdxMaps[i];

	for(size_t i=0; i < m_ThreadSerialisers.size(); i++)
		delete m_ThreadSerialisers[i];

	for(size_t i=0; i < m_ThreadTempMem.size(); i++)
	{
		delete[] m_ThreadTempMem[i]->memory;
		delete m_ThreadTempMem[i];
	}
}

VkCommandBuffer WrappedVulkan::GetNextCmd()
{
	VkCommandBuffer ret;

	if(!m_InternalCmds.freecmds.empty())
	{
		ret = m_InternalCmds.freecmds.back();
		m_InternalCmds.freecmds.pop_back();

		ObjDisp(ret)->ResetCommandBuffer(Unwrap(ret), 0);
	}
	else
	{	
		VkCommandBufferAllocateInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, Unwrap(m_InternalCmds.cmdpool), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
		VkResult vkr = ObjDisp(m_Device)->AllocateCommandBuffers(Unwrap(m_Device), &cmdInfo, &ret);

		SetDispatchTableOverMagicNumber(m_Device, ret);

		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(m_Device), ret);
	}

	m_InternalCmds.pendingcmds.push_back(ret);

	return ret;
}

void WrappedVulkan::SubmitCmds()
{
	// nothing to do
	if(m_InternalCmds.pendingcmds.empty())
		return;

	vector<VkCommandBuffer> cmds = m_InternalCmds.pendingcmds;
	for(size_t i=0; i < cmds.size(); i++) cmds[i] = Unwrap(cmds[i]);

	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		0, NULL, NULL, // wait semaphores
		(uint32_t)cmds.size(), &cmds[0], // command buffers
		0, NULL, // signal semaphores
	};

	// we might have work to do (e.g. debug manager creation command buffer) but
	// no queue, if the device is destroyed immediately. In this case we can just
	// skip the submit
	if(m_Queue != VK_NULL_HANDLE)
	{
		VkResult vkr = ObjDisp(m_Queue)->QueueSubmit(Unwrap(m_Queue), 1, &submitInfo, VK_NULL_HANDLE);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);
	}

	m_InternalCmds.submittedcmds.insert(m_InternalCmds.submittedcmds.end(), m_InternalCmds.pendingcmds.begin(), m_InternalCmds.pendingcmds.end());
	m_InternalCmds.pendingcmds.clear();
}

VkSemaphore WrappedVulkan::GetNextSemaphore()
{
	VkSemaphore ret;

	if(!m_InternalCmds.freesems.empty())
	{
		ret = m_InternalCmds.freesems.back();
		m_InternalCmds.freesems.pop_back();

		// assume semaphore is back to unsignaled state after being waited on
	}
	else
	{	
		VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkResult vkr = ObjDisp(m_Device)->CreateSemaphore(Unwrap(m_Device), &semInfo, NULL, &ret);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(m_Device), ret);
	}

	m_InternalCmds.pendingsems.push_back(ret);

	return ret;
}

void WrappedVulkan::SubmitSemaphores()
{
	// nothing to do
	if(m_InternalCmds.pendingsems.empty())
		return;

	// no actual submission, just mark them as 'done with' so they will be
	// recycled on next flush
	m_InternalCmds.submittedsems.insert(m_InternalCmds.submittedsems.end(), m_InternalCmds.pendingsems.begin(), m_InternalCmds.pendingsems.end());
	m_InternalCmds.pendingsems.clear();
}

void WrappedVulkan::FlushQ()
{
	// VKTODOLOW could do away with the need for this function by keeping
	// commands until N presents later, or something, or checking on fences.
	// If we do so, then check each use for FlushQ to see if it needs a
	// CPU-GPU sync or whether it is just looking to recycle command buffers
	// (Particularly the one in vkQueuePresentKHR drawing the overlay)
	

	// see comment in SubmitQ()
	if(m_Queue != VK_NULL_HANDLE)
	{
		ObjDisp(m_Queue)->QueueWaitIdle(Unwrap(m_Queue));
	}

	if(!m_InternalCmds.submittedcmds.empty())
	{
		m_InternalCmds.freecmds.insert(m_InternalCmds.freecmds.end(), m_InternalCmds.submittedcmds.begin(), m_InternalCmds.submittedcmds.end());
		m_InternalCmds.submittedcmds.clear();
	}
}

uint32_t WrappedVulkan::HandlePreCallback(VkCommandBuffer commandBuffer, bool dispatch)
{
	if(!m_DrawcallCallback) return 0;

	// look up the EID this drawcall came from
	DrawcallUse use(m_CurChunkOffset, 0);
	auto it = std::lower_bound(m_DrawcallUses.begin(), m_DrawcallUses.end(), use);
	RDCASSERT(it != m_DrawcallUses.end());

	uint32_t eventID = it->eventID;

	RDCASSERT(eventID != 0);

	// handle all aliases of this drawcall
	++it;
	while(it != m_DrawcallUses.end() && it->fileOffset == m_CurChunkOffset)
	{
		m_DrawcallCallback->AliasEvent(eventID, it->eventID);
		++it;
	}

	if(dispatch)
		m_DrawcallCallback->PreDispatch(eventID, commandBuffer);
	else
		m_DrawcallCallback->PreDraw(eventID, commandBuffer);

	return eventID;
}

const char *WrappedVulkan::GetChunkName(uint32_t idx)
{
	if(idx == CREATE_PARAMS) return "Create Params";
	if(idx == THUMBNAIL_DATA) return "Thumbnail Data";
	if(idx == DRIVER_INIT_PARAMS) return "Driver Init Params";
	if(idx == INITIAL_CONTENTS) return "Initial Contents";
	if(idx < FIRST_CHUNK_ID || idx >= NUM_VULKAN_CHUNKS)
		return "<unknown>";
	return VkChunkNames[idx-FIRST_CHUNK_ID];
}

template<>
string ToStrHelper<false, VulkanChunkType>::Get(const VulkanChunkType &el)
{
	return WrappedVulkan::GetChunkName(el);
}

byte *WrappedVulkan::GetTempMemory(size_t s)
{
	TempMem *mem = (TempMem *)Threading::GetTLSValue(tempMemoryTLSSlot);
	if(mem && mem->size >= s) return mem->memory;

	// alloc or grow alloc
	TempMem *newmem = mem;

	if(!newmem) newmem = new TempMem();

	// free old memory, don't need to keep contents
	if(newmem->memory) delete[] newmem->memory;

	// alloc new memory
	newmem->size = s;
	newmem->memory = new byte[s];

	Threading::SetTLSValue(tempMemoryTLSSlot, (void *)newmem);

	// if this is entirely new, save it for deletion on shutdown
	if(!mem)
	{
		SCOPED_LOCK(m_ThreadTempMemLock);
		m_ThreadTempMem.push_back(newmem);
	}

	return newmem->memory;
}

Serialiser *WrappedVulkan::GetThreadSerialiser()
{
	Serialiser *ser = (Serialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
	if(ser) return ser;

	// slow path, but rare

#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	ser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
	ser->SetUserData(m_ResourceManager);
	
	ser->SetChunkNameLookup(&GetChunkName);

	Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

	{
		SCOPED_LOCK(m_ThreadSerialisersLock);
		m_ThreadSerialisers.push_back(ser);
	}
	
	return ser;
}

static VkResult FillPropertyCountAndList(const VkExtensionProperties *src, uint32_t numExts, uint32_t *dstCount, VkExtensionProperties *dstProps)
{
	if(dstCount && !dstProps)
	{
		// just returning the number of extensions
		*dstCount = numExts;
		return VK_SUCCESS;
	}
	else if(dstCount && dstProps)
	{
		uint32_t dstSpace = *dstCount;

		// copy as much as there's space for, up to how many there are
		memcpy(dstProps, src, sizeof(VkExtensionProperties)*RDCMIN(numExts, dstSpace));

		// if there was enough space, return success, else incomplete
		if(dstSpace >= numExts)
			return VK_SUCCESS;
		else
			return VK_INCOMPLETE;
	}

	// both parameters were NULL, return incomplete
	return VK_INCOMPLETE;
}

bool operator <(const VkExtensionProperties &a, const VkExtensionProperties &b)
{
	// assume a given extension name is unique, ie. an implementation won't report the
	// same extension with two different spec versions.
	return strcmp(a.extensionName, b.extensionName) < 0;
}

VkResult WrappedVulkan::FilterDeviceExtensionProperties(VkPhysicalDevice physDev, uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
	VkResult vkr;

	// first fetch the list of extensions ourselves
	uint32_t numExts;
	vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), NULL, &numExts, NULL);

	if(vkr != VK_SUCCESS)
		return vkr;

	vector<VkExtensionProperties> exts(numExts);
	vkr = ObjDisp(physDev)->EnumerateDeviceExtensionProperties(Unwrap(physDev), NULL, &numExts, &exts[0]);
	
	if(vkr != VK_SUCCESS)
		return vkr;

	// filter the list of extensions to only the ones we support. Note it's important that
	// this list is kept sorted according to the above sort operator!
	const VkExtensionProperties supportedExtensions[] = {
		{
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
			VK_EXT_DEBUG_REPORT_SPEC_VERSION,
		},
		{
			VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,
			VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_SPEC_VERSION,
		},
		{
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_SURFACE_SPEC_VERSION,
		},
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SWAPCHAIN_SPEC_VERSION,
		},
#ifdef VK_KHR_win32_surface
		{
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_SPEC_VERSION,
		},
#endif
#ifdef VK_KHR_xcb_surface
		{
			VK_KHR_XCB_SURFACE_EXTENSION_NAME,
			VK_KHR_XCB_SURFACE_SPEC_VERSION,
		},
#endif
#ifdef VK_KHR_xlib_surface
		{
			VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
			VK_KHR_XLIB_SURFACE_SPEC_VERSION,
		},
#endif
	};

	// sort the reported extensions
	std::sort(exts.begin(), exts.end());

	std::vector<VkExtensionProperties> filtered;
	filtered.reserve(exts.size());

	// now we can step through both lists with two pointers,
	// instead of doing an O(N*M) lookup searching through each
	// supported extension for each reported extension.
	size_t i = 0;
	for(auto it=exts.begin(); it != exts.end() && i < ARRAY_COUNT(supportedExtensions); )
	{
		int nameCompare = strcmp(it->extensionName, supportedExtensions[i].extensionName);
		// if neither is less than the other, the extensions are equal
		if(nameCompare == 0)
		{
			// warn on spec version mismatch, but allow it.
			if(supportedExtensions[i].specVersion != it->specVersion)
				RDCWARN("Spec versions of %s are different between supported extension (%d) and reported (%d)!", it->extensionName, supportedExtensions[i].specVersion, it->specVersion);

			filtered.push_back(*it);
			++it;
			++i;
		}
		else if(nameCompare < 0)
		{
			// reported extension was less. It's not supported - skip past it and continue
			++it;
		}
		else if(nameCompare > 0)
		{
			// supported extension was less. Check the next supported extension
			++i;
		}
	}

	return FillPropertyCountAndList(&filtered[0], (uint32_t)filtered.size(), pPropertyCount, pProperties);
}

VkResult WrappedVulkan::GetProvidedExtensionProperties(uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
	// this is the list of extensions we provide - regardless of whether the ICD supports them
	const VkExtensionProperties providedExtensions[] = {
		{
			DEBUG_MARKER_EXTENSION_NAME,
			VK_DEBUG_MARKER_EXTENSION_REVISION
		},
	};

	return FillPropertyCountAndList(providedExtensions, (uint32_t)ARRAY_COUNT(providedExtensions), pPropertyCount, pProperties);
}

void WrappedVulkan::Serialise_CaptureScope(uint64_t offset)
{
	uint32_t FrameNumber = m_FrameCounter;
	GetMainSerialiser()->Serialise("FrameNumber", FrameNumber); // must use main serialiser here to match resource manager below

	if(m_State >= WRITING)
	{
		GetResourceManager()->Serialise_InitialContentsNeeded();
	}
	else
	{
		m_FrameRecord.frameInfo.fileOffset = offset;
		m_FrameRecord.frameInfo.firstEvent = 1;//m_pImmediateContext->GetEventID();
		m_FrameRecord.frameInfo.frameNumber = FrameNumber;
		m_FrameRecord.frameInfo.immContextId = ResourceId();
		RDCEraseEl(m_FrameRecord.frameInfo.stats);

		GetResourceManager()->CreateInitialContents();
	}
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
	// must use main serialiser here to match resource manager
	Serialiser *localSerialiser = GetMainSerialiser();

	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);
	
	SERIALISE_ELEMENT(ResourceId, bbid, GetResID(presentImage));

	bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
	localSerialiser->Serialise("HasCallstack", HasCallstack);	

	if(HasCallstack)
	{
		Callstack::Stackwalk *call = Callstack::Collect();

		RDCASSERT(call->NumLevels() < 0xff);

		size_t numLevels = call->NumLevels();
		uint64_t *stack = (uint64_t *)call->GetAddrs();

		localSerialiser->SerialisePODArray("callstack", stack, numLevels);

		delete call;
	}

	m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedVulkan::AttemptCapture()
{
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

	vector<VkImageMemoryBarrier> imgBarriers;
	
	{
		SCOPED_LOCK(m_ImageLayoutsLock); // not needed on replay, but harmless also
		GetResourceManager()->SerialiseImageStates(m_ImageLayouts, imgBarriers);
	}

	if(applyInitialState && !imgBarriers.empty())
	{
		VkCommandBuffer cmd = GetNextCmd();

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

		VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		
		VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		if(!imgBarriers.empty())
		{
			for(size_t i=0; i < imgBarriers.size(); i++)
			{
				imgBarriers[i].srcAccessMask = MakeAccessMask(imgBarriers[i].oldLayout);
				imgBarriers[i].dstAccessMask = MakeAccessMask(imgBarriers[i].newLayout);
			}
			ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), src_stages, dest_stages, false, 0, NULL, 0, NULL, (uint32_t)imgBarriers.size(), &imgBarriers[0]);
		}

		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		SubmitCmds();
		// don't need to flush here
	}

	return true;
}
	
void WrappedVulkan::BeginCaptureFrame()
{
	// must use main serialiser here to match resource manager
	Serialiser *localSerialiser = GetMainSerialiser();

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

	{
		SCOPED_LOCK(m_CoherentMapsLock);
		for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
		{
			Serialiser::FreeAlignedBuffer((*it)->memMapState->refData);
			(*it)->memMapState->refData = NULL;
			(*it)->memMapState->needRefData = false;
		}
	}
}

void WrappedVulkan::StartFrameCapture(void *dev, void *wnd)
{
	if(m_State != WRITING_IDLE) return;
	
	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);

	m_AppControlledCapture = true;

	m_FrameCounter = RDCMAX(1+(uint32_t)m_CapturedFrames.size(), m_FrameCounter);
	
	FetchFrameInfo frame;
	frame.frameNumber = m_FrameCounter+1;
	frame.captureTime = Timing::GetUnixTimestamp();
	RDCEraseEl(frame.stats);
	m_CapturedFrames.push_back(frame);

	GetResourceManager()->ClearReferencedResources();

	GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Instance), eFrameRef_Read);
	GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Device), eFrameRef_Read);
	GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Queue), eFrameRef_Read);

	// need to do all this atomically so that no other commands
	// will check to see if they need to markdirty or markpendingdirty
	// and go into the frame record.
	{
		SCOPED_LOCK(m_CapTransitionLock);
		GetResourceManager()->PrepareInitialContents();

		AttemptCapture();
		BeginCaptureFrame();

		m_State = WRITING_CAPFRAME;
	}

	RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedVulkan::EndFrameCapture(void *dev, void *wnd)
{
	if(m_State != WRITING_CAPFRAME) return true;
	
	VkSwapchainKHR swap = VK_NULL_HANDLE;
	
	if(wnd)
	{
		{
			SCOPED_LOCK(m_SwapLookupLock);
			auto it = m_SwapLookup.find(wnd);
			if(it != m_SwapLookup.end())
				swap = it->second;
		}

		if(swap == VK_NULL_HANDLE)
		{
			RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
			return false;
		}
	}

	RDCLOG("Finished capture, Frame %u", m_FrameCounter);

	VkImage backbuffer = VK_NULL_HANDLE;
	VkResourceRecord *swaprecord = NULL;

	if(swap != VK_NULL_HANDLE)
	{
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(swap), eFrameRef_Read);
	
		swaprecord = GetRecord(swap);
		RDCASSERT(swaprecord->swapInfo);

		const SwapchainInfo &swapInfo = *swaprecord->swapInfo;
		
		backbuffer = swapInfo.images[swapInfo.lastPresent].im;

		// mark all images referenced as well
		for(size_t i=0; i < swapInfo.images.size(); i++)
			GetResourceManager()->MarkResourceFrameReferenced(GetResID(swapInfo.images[i].im), eFrameRef_Read);
	}
	else
	{
		// if a swapchain wasn't specified or found, use the last one presented
		swaprecord = GetResourceManager()->GetResourceRecord(m_LastSwap);

		if(swaprecord)
		{
			GetResourceManager()->MarkResourceFrameReferenced(swaprecord->GetResourceID(), eFrameRef_Read);
			RDCASSERT(swaprecord->swapInfo);

			const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

			backbuffer = swapInfo.images[swapInfo.lastPresent].im;

			// mark all images referenced as well
			for(size_t i=0; i < swapInfo.images.size(); i++)
				GetResourceManager()->MarkResourceFrameReferenced(GetResID(swapInfo.images[i].im), eFrameRef_Read);
		}
	}
	
	// transition back to IDLE atomically
	{
		SCOPED_LOCK(m_CapTransitionLock);
		EndCaptureFrame(backbuffer);
		FinishCapture();
	}

	byte *thpixels = NULL;
	uint32_t thwidth = 0;
	uint32_t thheight = 0;

	// gather backbuffer screenshot
	const uint32_t maxSize = 1024;

	if(swap != VK_NULL_HANDLE)
	{
		VkDevice device = GetDev();
		VkCommandBuffer cmd = GetNextCmd();

		const VkLayerDispatchTable *vt = ObjDisp(device);

		vt->DeviceWaitIdle(Unwrap(device));
		
		const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

		// since these objects are very short lived (only this scope), we
		// don't wrap them.
		VkImage readbackIm = VK_NULL_HANDLE;
		VkDeviceMemory readbackMem = VK_NULL_HANDLE;

		VkResult vkr = VK_SUCCESS;

		// create identical image
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D, swapInfo.format,
			{ swapInfo.extent.width, swapInfo.extent.height, 1 }, 1, 1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};
		vt->CreateImage(Unwrap(device), &imInfo, NULL, &readbackIm);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		VkMemoryRequirements mrq;
		vt->GetImageMemoryRequirements(Unwrap(device), readbackIm, &mrq);

		VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
		VkSubresourceLayout layout = { 0 };
		vt->GetImageSubresourceLayout(Unwrap(device), readbackIm, &subr, &layout);

		// allocate readback memory
		VkMemoryAllocateInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
			mrq.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocateMemory(Unwrap(device), &allocInfo, NULL, &readbackMem);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);
		vkr = vt->BindImageMemory(Unwrap(device), readbackIm, readbackMem, 0);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

		// do image copy
		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		VkImageCopy cpy = {
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			{ 0, 0, 0 },
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
			{ 0, 0, 0 },
			{ imInfo.extent.width, imInfo.extent.height, 1 },
		};

		VkImageMemoryBarrier bbBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			0, 0, // MULTIDEVICE - need to actually pick the right queue family here maybe?
			Unwrap(backbuffer),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		VkImageMemoryBarrier readBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			readbackIm, // was never wrapped
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		DoPipelineBarrier(cmd, 1, &bbBarrier);
		DoPipelineBarrier(cmd, 1, &readBarrier);

		vt->CmdCopyImage(Unwrap(cmd), Unwrap(backbuffer), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackIm, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

		// barrier to switch backbuffer back to present layout
		std::swap(bbBarrier.oldLayout, bbBarrier.newLayout);
		std::swap(bbBarrier.srcAccessMask, bbBarrier.dstAccessMask);

		readBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		readBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		readBarrier.oldLayout = readBarrier.newLayout;
		readBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		
		DoPipelineBarrier(cmd, 1, &bbBarrier);
		DoPipelineBarrier(cmd, 1, &readBarrier);

		vkr = vt->EndCommandBuffer(Unwrap(cmd));
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

		SubmitCmds();
		FlushQ(); // need to wait so we can readback

		// map memory and readback
		byte *pData = NULL;
		vkr = vt->MapMemory(Unwrap(device), readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&pData);
		RDCASSERTEQUAL(vkr, VK_SUCCESS);

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
			bool bufBGRA = (fmt.bgraOrder != false);

			if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
			{
				stride = 4;
				buf1010102 = true;
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

		vt->UnmapMemory(Unwrap(device), readbackMem);

		// delete all
		vt->DestroyImage(Unwrap(device), readbackIm, NULL);
		vt->FreeMemory(Unwrap(device), readbackMem, NULL);
	}

	byte *jpgbuf = NULL;
	int len = thwidth*thheight;

	if(wnd)
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
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

		m_pFileSerialiser->Insert(scope.Get(true));
	}

	RDCDEBUG("Inserting Resource Serialisers");	

	GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);

	GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

	RDCDEBUG("Creating Capture Scope");	

	{
		Serialiser *localSerialiser = GetMainSerialiser();

		SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

		Serialise_CaptureScope(0);

		m_pFileSerialiser->Insert(scope.Get(true));

		m_pFileSerialiser->Insert(m_HeaderChunk);
	}

	// don't need to lock access to m_CmdBufferRecords as we are no longer 
	// in capframe (the transition is thread-protected) so nothing will be
	// pushed to the vector

	{
		RDCDEBUG("Flushing %u command buffer records to file serialiser", (uint32_t)m_CmdBufferRecords.size());	

		map<int32_t, Chunk *> recordlist;

		// ensure all command buffer records within the frame evne if recorded before, but
		// otherwise order must be preserved (vs. queue submits and desc set updates)
		for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
		{
			m_CmdBufferRecords[i]->Insert(recordlist);

			RDCDEBUG("Adding %u chunks to file serialiser from command buffer %llu", (uint32_t)recordlist.size(), m_CmdBufferRecords[i]->GetResourceID());	
		}

		m_FrameCaptureRecord->Insert(recordlist);

		RDCDEBUG("Flushing %u chunks to file serialiser from context record", (uint32_t)recordlist.size());	

		for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
			m_pFileSerialiser->Insert(it->second);

		RDCDEBUG("Done");	
	}

	m_pFileSerialiser->FlushToDisk();

	RenderDoc::Inst().SuccessfullyWrittenLog();

	SAFE_DELETE(m_pFileSerialiser);
	SAFE_DELETE(m_HeaderChunk);

	m_State = WRITING_IDLE;

	// delete cmd buffers now - had to keep them alive until after serialiser flush.
	for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
		m_CmdBufferRecords[i]->Delete(GetResourceManager());

	m_CmdBufferRecords.clear();

	GetResourceManager()->MarkUnwrittenResources();

	GetResourceManager()->ClearReferencedResources();

	GetResourceManager()->FreeInitialContents();

	GetResourceManager()->FlushPendingDirty();

	return true;
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
			m_pSerialiser->PushContext(NULL, NULL, CAPTURE_SCOPE, false);
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(CAPTURE_SCOPE);
		}
	}

	m_pSerialiser->Rewind();

	int chunkIdx = 0;

	struct chunkinfo
	{
		chunkinfo() : count(0), totalsize(0), total(0.0) {}
		int count;
		uint64_t totalsize;
		double total;
	};

	map<VulkanChunkType,chunkinfo> chunkInfos;

	SCOPED_TIMER("chunk initialisation");

	for(;;)
	{
		PerformanceTimer timer;

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

		if(context == CAPTURE_SCOPE)
		{
			// immediately read rest of log into memory
			m_pSerialiser->SetPersistentBlock(offset);
		}

		chunkIdx++;

		ProcessChunk(offset, context);

		m_pSerialiser->PopContext(context);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(m_pSerialiser->GetOffset())/float(m_pSerialiser->GetSize()));

		if(context == CAPTURE_SCOPE)
			ContextReplayLog(READING, 0, 0, false);

		uint64_t offset2 = m_pSerialiser->GetOffset();

		chunkInfos[context].total += timer.GetMilliseconds();
		chunkInfos[context].totalsize += offset2 - offset;
		chunkInfos[context].count++;

		if(context == CAPTURE_SCOPE)
		{
			if(m_pSerialiser->GetOffset() > lastFrame)
				break;
		}

		if(m_pSerialiser->AtEnd())
		{
			break;
		}
	}
	
#if !defined(RELEASE)
	for(auto it=chunkInfos.begin(); it != chunkInfos.end(); ++it)
	{
		double dcount = double(it->second.count);

		RDCDEBUG("% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
				it->second.count,
				it->second.total, it->second.total/dcount,
				double(it->second.totalsize)/(1024.0*1024.0),
				double(it->second.totalsize)/(dcount*1024.0*1024.0),
				GetChunkName(it->first), uint32_t(it->first)
				);
	}
#endif
	
	m_FrameRecord.frameInfo.fileSize = m_pSerialiser->GetSize();
	m_FrameRecord.frameInfo.persistentSize = m_pSerialiser->GetSize() - firstFrame;
	m_FrameRecord.frameInfo.initDataSize = chunkInfos[(VulkanChunkType)INITIAL_CONTENTS].totalsize;

	RDCDEBUG("Allocating %llu persistant bytes of memory for the log.", m_pSerialiser->GetSize() - firstFrame);
	
	m_pSerialiser->SetDebugText(false);

	// ensure the capture at least created a device and fetched a queue.
	RDCASSERT(m_Device != VK_NULL_HANDLE && m_Queue != VK_NULL_HANDLE && m_InternalCmds.cmdpool != VK_NULL_HANDLE);
}

void WrappedVulkan::ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial)
{
	m_State = readType;

	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
	RDCASSERTEQUAL(header, CONTEXT_CAPTURE_HEADER);

	Serialise_BeginCaptureFrame(!partial);
	
	ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

	// apply initial contents here so that images are in the right layout
	// (not undefined)
	if(readType == READING)
	{
		ApplyInitialContents();

		SubmitCmds();
		FlushQ();
	}

	m_pSerialiser->PopContext(header);

	m_RootEvents.clear();

	m_CmdBuffersInProgress = 0;
	
	if(m_State == EXECUTING)
	{
		FetchAPIEvent ev = GetEvent(startEventID);
		m_RootEventID = ev.eventID;

		// if not partial, we need to be sure to replay
		// past the command buffer records, so can't
		// skip to the file offset of the first event
		if(partial)
			m_pSerialiser->SetOffset(ev.fileOffset);

		m_FirstEventID = startEventID;
		m_LastEventID = endEventID;
	}
	else if(m_State == READING)
	{
		m_RootEventID = 1;
		m_RootDrawcallID = 1;
		m_FirstEventID = 0;
		m_LastEventID = ~0U;
	}

	for(;;)
	{
		if(m_State == EXECUTING && m_RootEventID > endEventID)
		{
			// we can just break out if we've done all the events desired.
			// note that the command buffer events aren't 'real' and we just blaze through them
			break;
		}

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

		m_LastCmdBufferID = ResourceId();

		ContextProcessChunk(offset, context, false);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));
		
		// for now just abort after capture scope. Really we'd need to support multiple frames
		// but for now this will do.
		if(context == CONTEXT_CAPTURE_FOOTER)
			break;
		
		// break out if we were only executing one event
		if(m_State == EXECUTING && startEventID == endEventID)
			break;

		// increment root event ID either if we didn't just replay a cmd
		// buffer event, OR if we are doing a frame sub-section replay,
		// in which case it's up to the calling code to make sure we only
		// replay inside a command buffer (if we crossed command buffer
		// boundaries, the event IDs would no longer match up).
		if(m_LastCmdBufferID == ResourceId() || startEventID > 1)
		{
			m_RootEventID++;

			if(startEventID > 1)
				m_pSerialiser->SetOffset(GetEvent(m_RootEventID).fileOffset);
		}
		else
		{
			// these events are completely omitted, so don't increment the curEventID
			if(context != BEGIN_CMD_BUFFER && context != END_CMD_BUFFER)
				m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
		}
	}

	if(m_State == READING)
	{
		GetFrameRecord().drawcallList = m_ParentDrawcall.Bake();

		SetupDrawcallPointers(&m_Drawcalls, GetFrameRecord().frameInfo.immContextId, GetFrameRecord().drawcallList, NULL, NULL);
		
		struct SortEID
		{
			bool operator() (const FetchAPIEvent &a, const FetchAPIEvent &b) { return a.eventID < b.eventID; }
		};

		std::sort(m_Events.begin(), m_Events.end(), SortEID());
		m_ParentDrawcall.children.clear();
	}
	
	ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

	// destroy any events we created for waiting on
	for(size_t i=0; i < m_CleanupEvents.size(); i++)
		ObjDisp(GetDev())->DestroyEvent(Unwrap(GetDev()), m_CleanupEvents[i], NULL);

	m_CleanupEvents.clear();

	if(m_PartialReplayData.resultPartialCmdBuffer != VK_NULL_HANDLE)
	{
		// deliberately call our own function, so this is destroyed as a wrapped object
		vkFreeCommandBuffers(m_PartialReplayData.partialDevice, m_PartialReplayData.resultPartialCmdPool, 1, &m_PartialReplayData.resultPartialCmdBuffer);
		m_PartialReplayData.resultPartialCmdBuffer = VK_NULL_HANDLE;
	}

	for(auto it = m_RerecordCmds.begin(); it != m_RerecordCmds.end(); ++it)
	{
		VkCommandBuffer cmd = it->second;

		// same as above (these are created in an identical way)
		vkFreeCommandBuffers(GetDev(), m_InternalCmds.cmdpool, 1, &cmd);
	}

	m_RerecordCmds.clear();

	m_State = READING;
}

void WrappedVulkan::ApplyInitialContents()
{
	// add a global memory barrier to ensure all writes have finished and are synchronised
	// add memory barrier to ensure this copy completes before any subsequent work
	// this is a very blunt instrument but it ensures we don't get random artifacts around
	// frame restart where we may be skipping a lot of important synchronisation
	VkMemoryBarrier memBarrier = {
		VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
		VK_ACCESS_ALL_WRITE_BITS,
		VK_ACCESS_ALL_READ_BITS,
	};

	VkCommandBuffer cmd = GetNextCmd();

	VkResult vkr = VK_SUCCESS;
	
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

	vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);

	DoPipelineBarrier(cmd, 1, &memBarrier);
	
	vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
	
	// sync all GPU work so we can also apply descriptor set initial contents
	SubmitCmds();
	FlushQ();

	// actually apply the initial contents here
	GetResourceManager()->ApplyInitialContents();
	
	// likewise again to make sure the initial states are all applied
	cmd = GetNextCmd();
	
	vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
	
	DoPipelineBarrier(cmd, 1, &memBarrier);
	
	vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

void WrappedVulkan::ContextProcessChunk(uint64_t offset, VulkanChunkType chunk, bool forceExecute)
{
	m_CurChunkOffset = offset;

	LogState state = m_State;

	if(forceExecute)
		m_State = EXECUTING;

	m_AddedDrawcall = false;

	ProcessChunk(offset, chunk);

	m_pSerialiser->PopContext(chunk);
	
	if(m_State == READING && chunk == SET_MARKER)
	{
		// no push/pop necessary
	}
	else if(m_State == READING && (chunk == BEGIN_CMD_BUFFER || chunk == END_CMD_BUFFER || chunk == BEGIN_EVENT || chunk == END_EVENT))
	{
		// don't add these events - they will be handled when inserted in-line into queue submit
	}
	else if(m_State == READING)
	{
		if(!m_AddedDrawcall)
			AddEvent(chunk, m_pSerialiser->GetDebugStr());
	}

	m_AddedDrawcall = false;
	
	if(forceExecute)
		m_State = state;
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
		Serialise_vkEnumeratePhysicalDevices(GetMainSerialiser(), NULL, NULL, NULL);
		break;
	case CREATE_DEVICE:
		Serialise_vkCreateDevice(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case GET_DEVICE_QUEUE:
		Serialise_vkGetDeviceQueue(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, NULL);
		break;

	case ALLOC_MEM:
		Serialise_vkAllocateMemory(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case UNMAP_MEM:
		Serialise_vkUnmapMemory(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case FLUSH_MEM:
		Serialise_vkFlushMappedMemoryRanges(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case FREE_MEM:
		RDCERR("vkFreeMemory should not be serialised directly");
		break;
	case CREATE_CMD_POOL:
		Serialise_vkCreateCommandPool(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_CMD_BUFFER:
		RDCERR("vkCreateCommandBuffer should not be serialised directly");
		break;
	case CREATE_FRAMEBUFFER:
		Serialise_vkCreateFramebuffer(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_RENDERPASS:
		Serialise_vkCreateRenderPass(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_POOL:
		Serialise_vkCreateDescriptorPool(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_SET_LAYOUT:
		Serialise_vkCreateDescriptorSetLayout(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_BUFFER:
		Serialise_vkCreateBuffer(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_BUFFER_VIEW:
		Serialise_vkCreateBufferView(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_IMAGE:
		Serialise_vkCreateImage(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_IMAGE_VIEW:
		Serialise_vkCreateImageView(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_SAMPLER:
		Serialise_vkCreateSampler(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_SHADER_MODULE:
		Serialise_vkCreateShaderModule(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_PIPE_LAYOUT:
		Serialise_vkCreatePipelineLayout(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_PIPE_CACHE:
		Serialise_vkCreatePipelineCache(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_GRAPHICS_PIPE:
		Serialise_vkCreateGraphicsPipelines(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL, NULL);
		break;
	case CREATE_COMPUTE_PIPE:
		Serialise_vkCreateComputePipelines(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL, NULL);
		break;
	case GET_SWAPCHAIN_IMAGE:
		Serialise_vkGetSwapchainImagesKHR(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, NULL);
		break;

	case CREATE_SEMAPHORE:
		Serialise_vkCreateSemaphore(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case CREATE_FENCE:
		Serialise_vkCreateFence(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case GET_FENCE_STATUS:
		Serialise_vkGetFenceStatus(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case RESET_FENCE:
		Serialise_vkResetFences(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case WAIT_FENCES:
		Serialise_vkWaitForFences(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_FALSE, 0);
		break;
		
	case CREATE_EVENT:
		Serialise_vkCreateEvent(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;
	case GET_EVENT_STATUS:
		Serialise_vkGetEventStatus(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case SET_EVENT:
		Serialise_vkSetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case RESET_EVENT:
		Serialise_vkResetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;

	case CREATE_QUERY_POOL:
		Serialise_vkCreateQueryPool(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;

	case ALLOC_DESC_SET:
		Serialise_vkAllocateDescriptorSets(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case UPDATE_DESC_SET:
		Serialise_vkUpdateDescriptorSets(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, 0, NULL);
		break;

	case BEGIN_CMD_BUFFER:
		Serialise_vkBeginCommandBuffer(GetMainSerialiser(), VK_NULL_HANDLE, NULL);
		break;
	case END_CMD_BUFFER:
		Serialise_vkEndCommandBuffer(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case QUEUE_WAIT_IDLE:
		Serialise_vkQueueWaitIdle(GetMainSerialiser(), VK_NULL_HANDLE);
		break;
	case DEVICE_WAIT_IDLE:
		Serialise_vkDeviceWaitIdle(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case QUEUE_SUBMIT:
		Serialise_vkQueueSubmit(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
		break;
	case BIND_BUFFER_MEM:
		Serialise_vkBindBufferMemory(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;
	case BIND_IMAGE_MEM:
		Serialise_vkBindImageMemory(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;

	case BIND_SPARSE:
		Serialise_vkQueueBindSparse(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
		break;

	case BEGIN_RENDERPASS:
		Serialise_vkCmdBeginRenderPass(GetMainSerialiser(), VK_NULL_HANDLE, NULL, VK_SUBPASS_CONTENTS_MAX_ENUM);
		break;
	case NEXT_SUBPASS:
		Serialise_vkCmdNextSubpass(GetMainSerialiser(), VK_NULL_HANDLE, VK_SUBPASS_CONTENTS_MAX_ENUM);
		break;
	case EXEC_CMDS:
		Serialise_vkCmdExecuteCommands(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case END_RENDERPASS:
		Serialise_vkCmdEndRenderPass(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case BIND_PIPELINE:
		Serialise_vkCmdBindPipeline(GetMainSerialiser(), VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_NULL_HANDLE);
		break;
	case SET_VP:
		Serialise_vkCmdSetViewport(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, NULL);
		break;
	case SET_SCISSOR:
		Serialise_vkCmdSetScissor(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, NULL);
		break;
	case SET_LINE_WIDTH:
		Serialise_vkCmdSetLineWidth(GetMainSerialiser(), VK_NULL_HANDLE, 0);
		break;
	case SET_DEPTH_BIAS:
		Serialise_vkCmdSetDepthBias(GetMainSerialiser(), VK_NULL_HANDLE, 0.0f, 0.0f, 0.0f);
		break;
	case SET_BLEND_CONST:
		Serialise_vkCmdSetBlendConstants(GetMainSerialiser(), VK_NULL_HANDLE, NULL);
		break;
	case SET_DEPTH_BOUNDS:
		Serialise_vkCmdSetDepthBounds(GetMainSerialiser(), VK_NULL_HANDLE, 0.0f, 0.0f);
		break;
	case SET_STENCIL_COMP_MASK:
		Serialise_vkCmdSetStencilCompareMask(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0);
		break;
	case SET_STENCIL_WRITE_MASK:
		Serialise_vkCmdSetStencilWriteMask(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0);
		break;
	case SET_STENCIL_REF:
		Serialise_vkCmdSetStencilReference(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0);
		break;
	case BIND_DESCRIPTOR_SET:
		Serialise_vkCmdBindDescriptorSets(GetMainSerialiser(), VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_NULL_HANDLE, 0, 0, NULL, 0, NULL);
		break;
	case BIND_INDEX_BUFFER:
		Serialise_vkCmdBindIndexBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, VK_INDEX_TYPE_MAX_ENUM);
		break;
	case BIND_VERTEX_BUFFERS:
		Serialise_vkCmdBindVertexBuffers(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, NULL, NULL);
		break;
	case COPY_BUF2IMG:
		Serialise_vkCmdCopyBufferToImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case COPY_IMG2BUF:
		Serialise_vkCmdCopyImageToBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, 0, NULL);
		break;
	case COPY_IMG:
		Serialise_vkCmdCopyImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case BLIT_IMG:
		Serialise_vkCmdBlitImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL, VK_FILTER_MAX_ENUM);
		break;
	case RESOLVE_IMG:
		Serialise_vkCmdResolveImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case COPY_BUF:
		Serialise_vkCmdCopyBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
		break;
	case UPDATE_BUF:
		Serialise_vkCmdUpdateBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, NULL);
		break;
	case FILL_BUF:
		Serialise_vkCmdFillBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case PUSH_CONST:
		Serialise_vkCmdPushConstants(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SHADER_STAGE_ALL, 0, 0, NULL);
		break;
	case CLEAR_COLOR:
		Serialise_vkCmdClearColorImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_DEPTHSTENCIL:
		Serialise_vkCmdClearDepthStencilImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_ATTACH:
		Serialise_vkCmdClearAttachments(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, 0, NULL);
		break;
	case PIPELINE_BARRIER:
		Serialise_vkCmdPipelineBarrier(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, VK_FALSE, 0, NULL, 0, NULL, 0, NULL);
		break;
	case WRITE_TIMESTAMP:
		Serialise_vkCmdWriteTimestamp(GetMainSerialiser(), VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_NULL_HANDLE, 0);
		break;
	case COPY_QUERY_RESULTS:
		Serialise_vkCmdCopyQueryPoolResults(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case BEGIN_QUERY:
		Serialise_vkCmdBeginQuery(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
		break;
	case END_QUERY:
		Serialise_vkCmdEndQuery(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;
	case RESET_QUERY_POOL:
		Serialise_vkCmdResetQueryPool(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
		break;
		
	case CMD_SET_EVENT:
		Serialise_vkCmdSetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		break;
	case CMD_RESET_EVENT:
		Serialise_vkCmdResetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
		break;
	case CMD_WAIT_EVENTS:
		Serialise_vkCmdWaitEvents(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, NULL, 0, NULL, 0, NULL);
		break;

	case DRAW:
		Serialise_vkCmdDraw(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, 0, 0);
		break;
	case DRAW_INDIRECT:
		Serialise_vkCmdDrawIndirect(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DRAW_INDEXED:
		Serialise_vkCmdDrawIndexed(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, 0, 0, 0);
		break;
	case DRAW_INDEXED_INDIRECT:
		Serialise_vkCmdDrawIndexedIndirect(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DISPATCH:
		Serialise_vkCmdDispatch(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DISPATCH_INDIRECT:
		Serialise_vkCmdDispatchIndirect(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;

	case BEGIN_EVENT:
		Serialise_vkCmdDbgMarkerBegin(GetMainSerialiser(), VK_NULL_HANDLE, NULL);
		break;
	case SET_MARKER:
		RDCFATAL("No such function vkCmdDbgMarker");
		break;
	case END_EVENT:
		Serialise_vkCmdDbgMarkerEnd(GetMainSerialiser(), VK_NULL_HANDLE);
		break;
	case SET_NAME:
		Serialise_vkDbgSetObjectName(GetMainSerialiser(), VK_NULL_HANDLE, VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT, 0, 0, NULL);
		break;

	case CREATE_SWAP_BUFFER:
		Serialise_vkCreateSwapchainKHR(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL, NULL);
		break;

	case CAPTURE_SCOPE:
		Serialise_CaptureScope(offset);
		break;
	case CONTEXT_CAPTURE_FOOTER:
		{
			Serialiser *localSerialiser = GetMainSerialiser();

			SERIALISE_ELEMENT(ResourceId, bbid, ResourceId());

			bool HasCallstack = false;
			localSerialiser->Serialise("HasCallstack", HasCallstack);	

			if(HasCallstack)
			{
				size_t numLevels = 0;
				uint64_t *stack = NULL;

				localSerialiser->SerialisePODArray("callstack", stack, numLevels);

				localSerialiser->SetCallstack(stack, numLevels);

				SAFE_DELETE_ARRAY(stack);
			}

			if(m_State == READING)
			{
				AddEvent(CONTEXT_CAPTURE_FOOTER, "vkQueuePresentKHR()");

				FetchDrawcall draw;
				draw.name = "vkQueuePresentKHR()";
				draw.flags |= eDraw_Present;

				draw.copyDestination = bbid;

				AddDrawcall(draw, true);
			}
		}
		break;
	default:
		// ignore system chunks
		if((int)context == (int)INITIAL_CONTENTS)
			Serialise_InitialState(ResourceId(), NULL);
		else if((int)context < (int)FIRST_CHUNK_ID)
			m_pSerialiser->SkipCurrentChunk();
		else
			RDCERR("Unrecognised Chunk type %d", context);
		break;
	}
}

void WrappedVulkan::ReplayLog(uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	uint64_t offs = m_FrameRecord.frameInfo.fileOffset;

	m_pSerialiser->SetOffset(offs);

	bool partial = true;

	if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
	{
		startEventID = m_FrameRecord.frameInfo.firstEvent;
		partial = false;
	}
	
	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

	RDCASSERTEQUAL(header, CAPTURE_SCOPE);

	m_pSerialiser->SkipCurrentChunk();

	m_pSerialiser->PopContext(header);
	
	if(!partial)
	{
		ApplyInitialContents();

		SubmitCmds();
		FlushQ();

		GetResourceManager()->ReleaseInFrameResources();
	}
	
	{
		if(!partial)
		{
			m_PartialReplayData.renderPassActive = false;
			RDCASSERT(m_PartialReplayData.resultPartialCmdBuffer == VK_NULL_HANDLE);
			m_PartialReplayData.partialParent = ResourceId();
			m_PartialReplayData.baseEvent = 0;
			m_RenderState = VulkanRenderState(&m_CreationInfo);
			m_RenderState.m_ResourceManager = GetResourceManager();
		}

		VkResult vkr = VK_SUCCESS;

		bool rpWasActive = false;
		
		// we'll need our own command buffer if we're replaying just a subsection
		// of events within a single command buffer record - always if it's only
		// one drawcall, or if start event ID is > 0 we assume the outside code
		// has chosen a subsection that lies within a command buffer
		if(partial)
		{
			VkCommandBuffer cmd = m_PartialReplayData.outsideCmdBuffer = GetNextCmd();
			
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };

			vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
			RDCASSERTEQUAL(vkr, VK_SUCCESS);

			rpWasActive = m_PartialReplayData.renderPassActive;

			// if a render pass was active, begin it and set up the partial replay state
			if(m_PartialReplayData.renderPassActive)
				m_RenderState.BeginRenderPassAndApplyState(cmd);
			// if we had a compute pipeline, need to bind that
			else if(m_RenderState.compute.pipeline != ResourceId())
				m_RenderState.BindPipeline(cmd);
		}

		if(replayType == eReplay_Full)
		{
			ContextReplayLog(EXECUTING, startEventID, endEventID, partial);
		}
		else if(replayType == eReplay_WithoutDraw)
		{
			ContextReplayLog(EXECUTING, startEventID, RDCMAX(1U,endEventID)-1, partial);
		}
		else if(replayType == eReplay_OnlyDraw)
		{
			ContextReplayLog(EXECUTING, endEventID, endEventID, partial);
		}
		else
			RDCFATAL("Unexpected replay type");

		if(m_PartialReplayData.outsideCmdBuffer != VK_NULL_HANDLE)
		{
			VkCommandBuffer cmd = m_PartialReplayData.outsideCmdBuffer;

			// check if the render pass is active - it could have become active
			// even if it wasn't before (if the above event was a CmdBeginRenderPass)
			if(m_PartialReplayData.renderPassActive)
				m_RenderState.EndRenderPass(cmd);

			// we might have replayed a CmdBeginRenderPass or CmdEndRenderPass,
			// but we want to keep the partial replay data state intact, so restore
			// whether or not a render pass was active.
			m_PartialReplayData.renderPassActive = rpWasActive;

			ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

			SubmitCmds();

			m_PartialReplayData.outsideCmdBuffer = VK_NULL_HANDLE;
		}
	}
}

VkBool32 WrappedVulkan::DebugCallback(
				VkDebugReportFlagsEXT                       flags,
				VkDebugReportObjectTypeEXT                  objectType,
				uint64_t                                    object,
				size_t                                      location,
				int32_t                                     messageCode,
				const char*                                 pLayerPrefix,
				const char*                                 pMessage)
{
	if(m_State < WRITING)
	{
		bool isDS = !strcmp(pLayerPrefix, "DS");

		// All access mask/barrier messages.
		// These are just too spammy/false positive/unreliable to keep
		if(isDS && messageCode == 12)
			return false;

		bool isMEM = !strcmp(pLayerPrefix, "MEM");

		// Memory is aliased between image and buffer
		// ignore memory aliasing warning - we make use of the memory in disjoint ways
		// and copy image data over separately, so our use is safe
		// no location set for this one, so ignore by code (maybe too coarse)
		if(isMEM && messageCode == 3)
			return false;

		RDCWARN("[%s:%u/%d] %s", pLayerPrefix, (uint32_t)location, messageCode, pMessage);
	}
	return false;
}

bool WrappedVulkan::ShouldRerecordCmd(ResourceId cmdid)
{
	if(m_PartialReplayData.outsideCmdBuffer != VK_NULL_HANDLE)
		return true;

	if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
		return true;

	return cmdid == m_PartialReplayData.partialParent;
}

bool WrappedVulkan::InRerecordRange()
{
	if(m_PartialReplayData.outsideCmdBuffer != VK_NULL_HANDLE)
		return true;
	
	if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
		return true;

	return m_BakedCmdBufferInfo[m_PartialReplayData.partialParent].curEventID <= m_LastEventID - m_PartialReplayData.baseEvent;
}

VkCommandBuffer WrappedVulkan::RerecordCmdBuf(ResourceId cmdid)
{
	if(m_PartialReplayData.outsideCmdBuffer != VK_NULL_HANDLE)
		return m_PartialReplayData.outsideCmdBuffer;
	
	if(m_DrawcallCallback && m_DrawcallCallback->RecordAllCmds())
	{
		auto it = m_RerecordCmds.find(cmdid);

		RDCASSERT(it != m_RerecordCmds.end());

		return it->second;
	}

	return m_PartialReplayData.resultPartialCmdBuffer;
}

void WrappedVulkan::AddDrawcall(FetchDrawcall d, bool hasEvents)
{
	m_AddedDrawcall = true;

	FetchDrawcall draw = d;
	draw.eventID = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID : m_RootEventID;
	draw.drawcallID = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount : m_RootDrawcallID;

	for(int i=0; i < 8; i++)
		draw.outputs[i] = ResourceId();

	draw.depthOut = ResourceId();

	draw.indexByteWidth = 0;
	draw.topology = eTopology_Unknown;

	if(m_LastCmdBufferID != ResourceId())
	{
		ResourceId pipe = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.pipeline;
		if(pipe != ResourceId())
			draw.topology = MakePrimitiveTopology(m_CreationInfo.m_Pipeline[pipe].topology, m_CreationInfo.m_Pipeline[pipe].patchControlPoints);

		draw.indexByteWidth = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.idxWidth;

		ResourceId fb = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.framebuffer;
		ResourceId rp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.renderPass;
		uint32_t sp = m_BakedCmdBufferInfo[m_LastCmdBufferID].state.subpass;

		if(fb != ResourceId() && rp != ResourceId())
		{
			vector<VulkanCreationInfo::Framebuffer::Attachment> &atts = m_CreationInfo.m_Framebuffer[fb].attachments;

			RDCASSERT(sp < m_CreationInfo.m_RenderPass[rp].subpasses.size());

			vector<uint32_t> &colAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].colorAttachments;
			int32_t dsAtt = m_CreationInfo.m_RenderPass[rp].subpasses[sp].depthstencilAttachment;

			RDCASSERT(colAtt.size() < 8);
			
			for(int i=0; i < 8 && i < (int)colAtt.size(); i++)
			{
				RDCASSERT(colAtt[i] < atts.size());
				draw.outputs[i] = atts[ colAtt[i] ].view;
			}

			if(dsAtt != -1)
			{
				RDCASSERT(dsAtt < (int32_t)atts.size());
				draw.depthOut = atts[dsAtt].view;
			}
		}
	}

	if(m_LastCmdBufferID != ResourceId())
		m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount++;
	else
		m_RootDrawcallID++;

	if(hasEvents)
	{
		vector<FetchAPIEvent> &srcEvents = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents : m_RootEvents;

		draw.events = srcEvents; srcEvents.clear();
	}
	
	// should have at least the root drawcall here, push this drawcall
	// onto the back's children list.
	if(!GetDrawcallStack().empty())
	{
		VulkanDrawcallTreeNode node(draw);

		if(m_LastCmdBufferID != ResourceId())
			AddUsage(node);

		node.children.insert(node.children.begin(), draw.children.elems, draw.children.elems+draw.children.count);
		GetDrawcallStack().back()->children.push_back(node);
	}
	else
		RDCERR("Somehow lost drawcall stack!");
}

void WrappedVulkan::AddUsage(VulkanDrawcallTreeNode &drawNode)
{
	FetchDrawcall &d = drawNode.draw;

	const BakedCmdBufferInfo::CmdBufferState &state = m_BakedCmdBufferInfo[m_LastCmdBufferID].state;
	VulkanCreationInfo &c = m_CreationInfo;
	uint32_t e = d.eventID;

	if((d.flags & (eDraw_Drawcall|eDraw_Dispatch)) == 0)
		return;
	
	//////////////////////////////
	// Vertex input

	if(d.flags & eDraw_UseIBuffer && state.ibuffer != ResourceId())
		drawNode.resourceUsage.push_back(std::make_pair(state.ibuffer, EventUsage(e, eUsage_IndexBuffer)));

	for(size_t i=0; i < state.vbuffers.size(); i++)
		drawNode.resourceUsage.push_back(std::make_pair(state.vbuffers[i], EventUsage(e, eUsage_VertexBuffer)));
	
	//////////////////////////////
	// Shaders

	for(int shad=0; shad < 6; shad++)
	{
		VulkanCreationInfo::Pipeline::Shader &sh = c.m_Pipeline[state.pipeline].shaders[shad];
		if(sh.module == ResourceId()) continue;

		// 5 is the compute shader's index (VS, TCS, TES, GS, FS, CS)
		const vector<ResourceId> &descSets = (shad == 5 ? state.computeDescSets : state.graphicsDescSets);

		RDCASSERT(sh.mapping);

		struct ResUsageType
		{
			ResUsageType(rdctype::array<BindpointMap> &a, ResourceUsage u)
				: bindmap(a), usage(u) {}
			rdctype::array<BindpointMap> &bindmap;
			ResourceUsage usage;
		};

		ResUsageType types[] = {
			ResUsageType(sh.mapping->ReadOnlyResources, eUsage_VS_Resource),
			ResUsageType(sh.mapping->ReadWriteResources, eUsage_VS_RWResource),
			ResUsageType(sh.mapping->ConstantBlocks, eUsage_VS_Constants),
		};

		for(size_t t=0; t < ARRAY_COUNT(types); t++)
		{
			for(int32_t i=0; i < types[t].bindmap.count; i++)
			{
				if(!types[t].bindmap[i].used) continue;

				// ignore push constants
				if(t == 2 && !sh.refl->ConstantBlocks[i].bufferBacked) continue;

				int32_t bindset = types[t].bindmap[i].bindset;
				int32_t bind = types[t].bindmap[i].bind;

				if(bindset >= (int32_t)descSets.size())
				{
					RDCWARN("At draw %u, shader referenced a descriptor set %i that was not bound", drawNode.draw.eventID, bindset);
					continue;
				}
				
				DescriptorSetInfo &descset = m_DescriptorSetState[ descSets[bindset] ];
				DescSetLayout &layout = c.m_DescSetLayout[ descset.layout ];

				if(layout.bindings.empty())
				{
					RDCWARN("At draw %u, shader referenced a descriptor set %i that was not bound", drawNode.draw.eventID, bindset);
					continue;
				}

				if(bind >= (int32_t)layout.bindings.size())
				{
					RDCWARN("At draw %u, shader referenced a bind %i in descriptor set %i that does not exist. Mismatched descriptor set?", drawNode.draw.eventID, bind, bindset);
					continue;
				}
				
				// handled as part of the framebuffer attachments
				if(layout.bindings[bind].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
					continue;
				
				// we don't mark samplers with usage
				if(layout.bindings[bind].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
					continue;

				ResourceUsage usage = ResourceUsage(types[t].usage + shad);

				if(bind >= (int32_t)descset.currentBindings.size())
				{
					RDCWARN("At draw %u, shader referenced a bind %i in descriptor set %i that does not exist. Mismatched descriptor set?", drawNode.draw.eventID, bind, bindset);
					continue;
				}

				for(uint32_t a=0; a < layout.bindings[bind].descriptorCount; a++)
				{
					DescriptorSetSlot &slot = descset.currentBindings[bind][a];
					
					ResourceId id;

					switch(layout.bindings[bind].descriptorType)
					{
						case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
						case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
						case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
							if(slot.imageInfo.imageView != VK_NULL_HANDLE)
								id = c.m_ImageView[GetResourceManager()->GetNonDispWrapper(slot.imageInfo.imageView)->id].image;
							break;
						case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
							if(slot.texelBufferView != VK_NULL_HANDLE)
								id = c.m_BufferView[GetResourceManager()->GetNonDispWrapper(slot.texelBufferView)->id].buffer;
							break;
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
							if(slot.bufferInfo.buffer != VK_NULL_HANDLE)
								id = GetResourceManager()->GetNonDispWrapper(slot.bufferInfo.buffer)->id;
							break;
						default:
							RDCERR("Unexpected type %d", layout.bindings[bind].descriptorType);
							break;
					}

					drawNode.resourceUsage.push_back(std::make_pair(id, EventUsage(e, usage)));
				}
			}
		}
	}

	//////////////////////////////
	// Framebuffer/renderpass

	if(state.renderPass != ResourceId() && state.framebuffer != ResourceId())
	{
		VulkanCreationInfo::RenderPass &rp = c.m_RenderPass[state.renderPass];
		VulkanCreationInfo::Framebuffer &fb = c.m_Framebuffer[state.framebuffer];

		RDCASSERT(state.subpass < rp.subpasses.size());

		for(size_t i=0; i < rp.subpasses[state.subpass].inputAttachments.size(); i++)
		{
			uint32_t att = rp.subpasses[state.subpass].inputAttachments[i];
			drawNode.resourceUsage.push_back(std::make_pair(c.m_ImageView[fb.attachments[att].view].image, EventUsage(e, eUsage_InputTarget)));
		}

		for(size_t i=0; i < rp.subpasses[state.subpass].colorAttachments.size(); i++)
		{
			uint32_t att = rp.subpasses[state.subpass].colorAttachments[i];
			drawNode.resourceUsage.push_back(std::make_pair(c.m_ImageView[fb.attachments[att].view].image, EventUsage(e, eUsage_ColourTarget)));
		}

		if(rp.subpasses[state.subpass].depthstencilAttachment >= 0)
		{
			int32_t att = rp.subpasses[state.subpass].depthstencilAttachment;
			drawNode.resourceUsage.push_back(std::make_pair(c.m_ImageView[fb.attachments[att].view].image, EventUsage(e, eUsage_DepthStencilTarget)));
		}
	}
}

void WrappedVulkan::AddEvent(VulkanChunkType type, string description)
{
	FetchAPIEvent apievent;

	apievent.context = ResourceId();
	apievent.fileOffset = m_CurChunkOffset;
	apievent.eventID = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID : m_RootEventID;

	apievent.eventDesc = description;

	Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
	if(stack)
	{
		create_array(apievent.callstack, stack->NumLevels());
		memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t)*stack->NumLevels());
	}

	if(m_LastCmdBufferID != ResourceId())
	{
		m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.push_back(apievent);
	}
	else
	{
		m_RootEvents.push_back(apievent);
		m_Events.push_back(apievent);
	}
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

const FetchDrawcall *WrappedVulkan::GetDrawcall(uint32_t eventID)
{
	if(eventID >= m_Drawcalls.size())
		return NULL;

	return m_Drawcalls[eventID];
}
