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

#include "../vk_core.h"

bool WrappedVulkan::Serialise_vkCmdSetViewport(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    viewportCount,
			const VkViewport*                           pViewports)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, count, viewportCount);
	SERIALISE_ELEMENT_ARR(VkViewport, views, pViewports, count);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetViewport(Unwrap(cmdBuffer), count, views);
			m_PartialReplayData.state.views.assign(views, views + count);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetViewport(Unwrap(cmdBuffer), count, views);
	}

	SAFE_DELETE_ARRAY(views);

	return true;
}

void WrappedVulkan::vkCmdSetViewport(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    viewportCount,
			const VkViewport*                           pViewports)
{
	ObjDisp(cmdBuffer)->CmdSetViewport(Unwrap(cmdBuffer), viewportCount, pViewports);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_VP);
		Serialise_vkCmdSetViewport(localSerialiser, cmdBuffer, viewportCount, pViewports);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetScissor(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    scissorCount,
			const VkRect2D*                             pScissors)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(uint32_t, count, scissorCount);
	SERIALISE_ELEMENT_ARR(VkRect2D, scissors, pScissors, count);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetScissor(Unwrap(cmdBuffer), count, scissors);
			m_PartialReplayData.state.scissors.assign(scissors, scissors + count);
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetScissor(Unwrap(cmdBuffer), count, scissors);
	}

	SAFE_DELETE_ARRAY(scissors);

	return true;
}

void WrappedVulkan::vkCmdSetScissor(
			VkCmdBuffer                                 cmdBuffer,
			uint32_t                                    scissorCount,
			const VkRect2D*                             pScissors)
{
	ObjDisp(cmdBuffer)->CmdSetScissor(Unwrap(cmdBuffer), scissorCount, pScissors);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_SCISSOR);
		Serialise_vkCmdSetScissor(localSerialiser, cmdBuffer, scissorCount, pScissors);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetLineWidth(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			float                                       lineWidth)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(float, width, lineWidth);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetLineWidth(Unwrap(cmdBuffer), width);
			m_PartialReplayData.state.lineWidth = width;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetLineWidth(Unwrap(cmdBuffer), width);
	}

	return true;
}

void WrappedVulkan::vkCmdSetLineWidth(
			VkCmdBuffer                                 cmdBuffer,
			float                                       lineWidth)
{
	ObjDisp(cmdBuffer)->CmdSetLineWidth(Unwrap(cmdBuffer), lineWidth);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_LINE_WIDTH);
		Serialise_vkCmdSetLineWidth(localSerialiser, cmdBuffer, lineWidth);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetDepthBias(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			float                                       depthBias,
			float                                       depthBiasClamp,
			float                                       slopeScaledDepthBias)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(float, bias, depthBias);
	SERIALISE_ELEMENT(float, biasclamp, depthBiasClamp);
	SERIALISE_ELEMENT(float, slope, slopeScaledDepthBias);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetDepthBias(Unwrap(cmdBuffer), bias, biasclamp, slope);
			m_PartialReplayData.state.bias.depth = bias;
			m_PartialReplayData.state.bias.biasclamp = biasclamp;
			m_PartialReplayData.state.bias.slope = slope;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetDepthBias(Unwrap(cmdBuffer), bias, biasclamp, slope);
	}

	return true;
}

void WrappedVulkan::vkCmdSetDepthBias(
			VkCmdBuffer                                 cmdBuffer,
			float                                       depthBias,
			float                                       depthBiasClamp,
			float                                       slopeScaledDepthBias)
{
	ObjDisp(cmdBuffer)->CmdSetDepthBias(Unwrap(cmdBuffer), depthBias, depthBiasClamp, slopeScaledDepthBias);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_DEPTH_BIAS);
		Serialise_vkCmdSetDepthBias(localSerialiser, cmdBuffer, depthBias, depthBiasClamp, slopeScaledDepthBias);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetBlendConstants(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			const float*                                blendConst)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	float blendFactor[4];
	if(m_State >= WRITING)
	{
		blendFactor[0] = blendConst[0];
		blendFactor[1] = blendConst[1];
		blendFactor[2] = blendConst[2];
		blendFactor[3] = blendConst[3];
	}
	localSerialiser->SerialisePODArray<4>("blendConst", blendFactor);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetBlendConstants(Unwrap(cmdBuffer), blendFactor);
			memcpy(m_PartialReplayData.state.blendConst, blendFactor, sizeof(blendFactor));
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetBlendConstants(Unwrap(cmdBuffer), blendFactor);
	}

	return true;
}

void WrappedVulkan::vkCmdSetBlendConstants(
			VkCmdBuffer                                 cmdBuffer,
			const float*                                blendConst)
{
	ObjDisp(cmdBuffer)->CmdSetBlendConstants(Unwrap(cmdBuffer), blendConst);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_BLEND_CONST);
		Serialise_vkCmdSetBlendConstants(localSerialiser, cmdBuffer, blendConst);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetDepthBounds(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			float                                       minDepthBounds,
			float                                       maxDepthBounds)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(float, mind, minDepthBounds);
	SERIALISE_ELEMENT(float, maxd, maxDepthBounds);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetDepthBounds(Unwrap(cmdBuffer), mind, maxd);
			m_PartialReplayData.state.mindepth = mind;
			m_PartialReplayData.state.maxdepth = maxd;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetDepthBounds(Unwrap(cmdBuffer), mind, maxd);
	}

	return true;
}

void WrappedVulkan::vkCmdSetDepthBounds(
			VkCmdBuffer                                 cmdBuffer,
			float                                       minDepthBounds,
			float                                       maxDepthBounds)
{
	ObjDisp(cmdBuffer)->CmdSetDepthBounds(Unwrap(cmdBuffer), minDepthBounds, maxDepthBounds);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_DEPTH_BOUNDS);
		Serialise_vkCmdSetDepthBounds(localSerialiser, cmdBuffer, minDepthBounds, maxDepthBounds);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetStencilCompareMask(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilCompareMask)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkStencilFaceFlagBits, face, (VkStencilFaceFlagBits)faceMask);
	SERIALISE_ELEMENT(uint32_t, mask, stencilCompareMask);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetStencilCompareMask(Unwrap(cmdBuffer), face, mask);

			if(face & VK_STENCIL_FACE_FRONT_BIT)
				m_PartialReplayData.state.front.compare = mask;
			if(face & VK_STENCIL_FACE_BACK_BIT)
				m_PartialReplayData.state.back.compare = mask;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetStencilCompareMask(Unwrap(cmdBuffer), face, mask);
	}

	return true;
}

void WrappedVulkan::vkCmdSetStencilCompareMask(
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilCompareMask)
{
	ObjDisp(cmdBuffer)->CmdSetStencilCompareMask(Unwrap(cmdBuffer), faceMask, stencilCompareMask);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_STENCIL_COMP_MASK);
		Serialise_vkCmdSetStencilCompareMask(localSerialiser, cmdBuffer, faceMask, stencilCompareMask);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetStencilWriteMask(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilWriteMask)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkStencilFaceFlagBits, face, (VkStencilFaceFlagBits)faceMask);
	SERIALISE_ELEMENT(uint32_t, mask, stencilWriteMask);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetStencilWriteMask(Unwrap(cmdBuffer), face, mask);

			if(face & VK_STENCIL_FACE_FRONT_BIT)
				m_PartialReplayData.state.front.write = mask;
			if(face & VK_STENCIL_FACE_BACK_BIT)
				m_PartialReplayData.state.back.write = mask;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetStencilWriteMask(Unwrap(cmdBuffer), face, mask);
	}

	return true;
}

void WrappedVulkan::vkCmdSetStencilWriteMask(
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilWriteMask)
{
	ObjDisp(cmdBuffer)->CmdSetStencilWriteMask(Unwrap(cmdBuffer), faceMask, stencilWriteMask);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_STENCIL_WRITE_MASK);
		Serialise_vkCmdSetStencilWriteMask(localSerialiser, cmdBuffer, faceMask, stencilWriteMask);

		record->AddChunk(scope.Get());
	}
}

bool WrappedVulkan::Serialise_vkCmdSetStencilReference(
			Serialiser*                                 localSerialiser,
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilReference)
{
	SERIALISE_ELEMENT(ResourceId, cmdid, GetResID(cmdBuffer));
	SERIALISE_ELEMENT(VkStencilFaceFlagBits, face, (VkStencilFaceFlagBits)faceMask);
	SERIALISE_ELEMENT(uint32_t, mask, stencilReference);

	if(m_State < WRITING)
		m_LastCmdBufferID = cmdid;
	
	if(m_State == EXECUTING)
	{
		if(IsPartialCmd(cmdid) && InPartialRange())
		{
			cmdBuffer = PartialCmdBuf();
			ObjDisp(cmdBuffer)->CmdSetStencilReference(Unwrap(cmdBuffer), face, mask);

			if(face & VK_STENCIL_FACE_FRONT_BIT)
				m_PartialReplayData.state.front.ref = mask;
			if(face & VK_STENCIL_FACE_BACK_BIT)
				m_PartialReplayData.state.back.ref = mask;
		}
	}
	else if(m_State == READING)
	{
		cmdBuffer = GetResourceManager()->GetLiveHandle<VkCmdBuffer>(cmdid);

		ObjDisp(cmdBuffer)->CmdSetStencilReference(Unwrap(cmdBuffer), face, mask);
	}

	return true;
}

void WrappedVulkan::vkCmdSetStencilReference(
			VkCmdBuffer                                 cmdBuffer,
			VkStencilFaceFlags                          faceMask,
			uint32_t                                    stencilReference)
{
	ObjDisp(cmdBuffer)->CmdSetStencilReference(Unwrap(cmdBuffer), faceMask, stencilReference);

	if(m_State >= WRITING)
	{
		VkResourceRecord *record = GetRecord(cmdBuffer);

		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(SET_STENCIL_REF);
		Serialise_vkCmdSetStencilReference(localSerialiser, cmdBuffer, faceMask, stencilReference);

		record->AddChunk(scope.Get());
	}
}

