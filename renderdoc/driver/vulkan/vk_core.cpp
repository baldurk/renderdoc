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

	VkResult ret = GetInstanceDispatchTable(NULL)->CreateInstance(&instinfo, &inst);

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

	m_SwapPhysDevice = -1;

	m_Replay.SetDriver(this);

	m_FrameCounter = 0;

	m_FrameTimer.Restart();

	m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;
	
	m_RootEventID = 1;
	m_RootDrawcallID = 1;
	m_FirstEventID = 0;
	m_LastEventID = ~0U;

	m_LastCmdBufferID = ResourceId();

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
	
	// VKTODOLOW shutdown order is really up in the air
	for(size_t i=0; i < m_PhysicalReplayData.size(); i++)
		if(m_PhysicalReplayData[i].dev != VK_NULL_HANDLE)
			vkDestroyDevice(m_PhysicalReplayData[i].dev);

	// VKTODOLOW only one instance
	if(m_PhysicalReplayData[0].inst != VK_NULL_HANDLE)
	{
		VkInstance instance = Unwrap(m_PhysicalReplayData[0].inst);

		ObjDisp(m_PhysicalReplayData[0].inst)->DestroyInstance(instance);
	}
}

const char * WrappedVulkan::GetChunkName(uint32_t idx)
{
	if(idx < FIRST_CHUNK_ID || idx >= NUM_VULKAN_CHUNKS)
		return "<unknown>";
	return VkChunkNames[idx-FIRST_CHUNK_ID];
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
		chunkinfo() : count(0), totalsize(0), total(0.0) {}
		int count;
		uint64_t totalsize;
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
		}

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

		GetResourceManager()->WrapResource(Unwrap(GetDev()), fakeBBImView);
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

		m_PartialReplayData.renderPassActive = false;
		RDCASSERT(m_PartialReplayData.resultPartialCmdBuffer == VK_NULL_HANDLE);
		m_PartialReplayData.partialParent = ResourceId();
		m_PartialReplayData.baseEvent = 0;
		m_PartialReplayData.state = PartialReplayData::StateVector();
	}
	else if(m_State == READING)
	{
		m_RootEventID = 1;
		m_RootDrawcallID = 1;
		m_FirstEventID = 0;
		m_LastEventID = ~0U;
	}

	// VKTODOMED I think this is a legacy concept that doesn't really mean anything anymore,
	// even on GL/D3D11. Creates are all shifted before the frame, only command bfufers remain
	// in vulkan
	//GetResourceManager()->MarkInFrame(true);

	while(1)
	{
		if(m_State == EXECUTING && m_RootEventID > endEventID)
		{
			// we can just break out if we've done all the events desired.
			// note that the command buffer events aren't 'real' and we just blaze through them
			break;
		}

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, 1, false);

		m_LastCmdBufferID = ResourceId();

		ContextProcessChunk(offset, context, false);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));
		
		// for now just abort after capture scope. Really we'd need to support multiple frames
		// but for now this will do.
		if(context == CONTEXT_CAPTURE_FOOTER)
			break;

		if(m_LastCmdBufferID != ResourceId())
			m_CmdBufferInfo[m_LastCmdBufferID].curEventID++;
		else
			m_RootEventID++;
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

	LogState state = m_State;

	if(forceExecute)
		m_State = EXECUTING;

	m_AddedDrawcall = false;

	ProcessChunk(offset, chunk);

	m_pSerialiser->PopContext(NULL, chunk);
	
	if(m_State == READING && chunk == SET_MARKER)
	{
		// no push/pop necessary
	}
	else if(m_State == READING && chunk == BEGIN_EVENT)
	{
		// push down the drawcallstack to the latest drawcall
		GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());
	}
	else if(m_State == READING && chunk == END_EVENT)
	{
		// refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
		RDCASSERT(GetDrawcallStack().size() > 1);
		if(GetDrawcallStack().size() > 1)
			GetDrawcallStack().pop_back();
	}
	else if(m_State == READING && (chunk == BEGIN_CMD_BUFFER || chunk == END_CMD_BUFFER))
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
	case FLUSH_MEM:
		Serialise_vkFlushMappedMemoryRanges(VK_NULL_HANDLE, 0, NULL);
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
		Serialise_vkCreateFence(VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_FENCE_STATUS:
		Serialise_vkGetFenceStatus(VK_NULL_HANDLE, VK_NULL_HANDLE);
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

			vkr = ObjDisp(q)->QueueSubmit(Unwrap(q), 1, UnwrapPtr(cmd), VK_NULL_HANDLE);
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

	FetchDrawcall draw = d;
	draw.eventID = m_LastCmdBufferID != ResourceId() ? m_CmdBufferInfo[m_LastCmdBufferID].curEventID : m_RootEventID;
	draw.drawcallID = m_LastCmdBufferID != ResourceId() ? m_CmdBufferInfo[m_LastCmdBufferID].drawCount : m_RootDrawcallID;

	for(int i=0; i < 8; i++)
		draw.outputs[i] = ResourceId();

	draw.depthOut = ResourceId();

	ResourceId pipe = m_PartialReplayData.state.graphics.pipeline;
	if(pipe != ResourceId())
		draw.topology = MakePrimitiveTopology(m_CreationInfo.m_Pipeline[pipe].topology, m_CreationInfo.m_Pipeline[pipe].patchControlPoints);
	else
		draw.topology = eTopology_Unknown;

	draw.indexByteWidth = m_PartialReplayData.state.ibuffer.bytewidth;

	if(m_LastCmdBufferID != ResourceId())
		m_CmdBufferInfo[m_LastCmdBufferID].drawCount++;
	else
		m_RootDrawcallID++;

	if(hasEvents)
	{
		vector<FetchAPIEvent> &srcEvents = m_LastCmdBufferID != ResourceId() ? m_CmdBufferInfo[m_LastCmdBufferID].curEvents : m_RootEvents;

		// VKTODOLOW the whole 'context' filter thing will go away so this will be
		// a straight copy
		vector<FetchAPIEvent> evs;
		evs.reserve(srcEvents.size());
		for(size_t i=0; i < srcEvents.size(); )
		{
			if(srcEvents[i].context == draw.context)
			{
				evs.push_back(srcEvents[i]);
				srcEvents.erase(srcEvents.begin()+i);
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
	if(!GetDrawcallStack().empty())
	{
		DrawcallTreeNode node(draw);
		node.children.insert(node.children.begin(), draw.children.elems, draw.children.elems+draw.children.count);
		GetDrawcallStack().back()->children.push_back(node);
	}
	else
		RDCERR("Somehow lost drawcall stack!");
}

void WrappedVulkan::AddEvent(VulkanChunkType type, string description)
{
	FetchAPIEvent apievent;

	apievent.context = ResourceId();
	apievent.fileOffset = m_CurChunkOffset;
	apievent.eventID = m_LastCmdBufferID != ResourceId() ? m_CmdBufferInfo[m_LastCmdBufferID].curEventID : m_RootEventID;

	apievent.eventDesc = description;

	Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
	if(stack)
	{
		create_array(apievent.callstack, stack->NumLevels());
		memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t)*stack->NumLevels());
	}

	if(m_LastCmdBufferID != ResourceId())
		m_CmdBufferInfo[m_LastCmdBufferID].curEvents.push_back(apievent);
	else
		m_RootEvents.push_back(apievent);

	if(m_State == READING && m_CmdBuffersInProgress == 0)
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
