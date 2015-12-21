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

#include "vk_state.h"
#include "vk_info.h"
#include "vk_resources.h"

VulkanRenderState::VulkanRenderState(VulkanCreationInfo &createInfo)
	: m_CreationInfo(createInfo)
{
	compute.pipeline = graphics.pipeline = renderPass = framebuffer = ResourceId();
	compute.descSets.clear();
	graphics.descSets.clear();
	compute.offsets.clear();
	graphics.offsets.clear();

	views.clear();
	scissors.clear();
	lineWidth = 1.0f;
	RDCEraseEl(bias);
	RDCEraseEl(blendConst);
	mindepth = 0.0f; maxdepth = 1.0f;
	RDCEraseEl(front);
	RDCEraseEl(back);
	RDCEraseEl(pushconsts);

	renderPass = ResourceId();
	subpass = 0;

	RDCEraseEl(renderArea);

	RDCEraseEl(ibuffer);
	vbuffers.clear();
}

VulkanRenderState & VulkanRenderState::operator =(const VulkanRenderState &o)
{
	views = o.views;
	scissors = o.scissors;
	lineWidth = o.lineWidth;
	bias = o.bias;
	memcpy(blendConst, o.blendConst, sizeof(blendConst));
	mindepth = o.mindepth;
	maxdepth = o.maxdepth;
	front = o.front;
	back = o.back;
	memcpy(pushconsts, o.pushconsts, sizeof(pushconsts));
	renderPass = o.renderPass;
	subpass = o.subpass;
	framebuffer = o.framebuffer;
	renderArea = o.renderArea;

	compute.pipeline = o.compute.pipeline;
	compute.descSets = o.compute.descSets;
	compute.offsets = o.compute.offsets;

	graphics.pipeline = o.graphics.pipeline;
	graphics.descSets = o.graphics.descSets;
	graphics.offsets = o.graphics.offsets;

	ibuffer = o.ibuffer;
	vbuffers = o.vbuffers;

	return *this;
}

void VulkanRenderState::BeginRenderPassAndApplyState(VkCommandBuffer cmd)
{
	RDCASSERT(renderPass != ResourceId());

	// clear values don't matter as we're using the load renderpass here, that
	// has all load ops set to load (as we're doing a partial replay - can't
	// just clear the targets that are partially written to).

	VkClearValue empty[16] = {0};

	RDCASSERT(ARRAY_COUNT(empty) >= m_CreationInfo.m_RenderPass[renderPass].attachments.size());

	VkRenderPassBeginInfo rpbegin = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		Unwrap(m_CreationInfo.m_RenderPass[renderPass].loadRP),
		Unwrap(GetResourceManager()->GetCurrentHandle<VkFramebuffer>(framebuffer)),
		renderArea,
		(uint32_t)m_CreationInfo.m_RenderPass[renderPass].attachments.size(), empty,
	};
	ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

	for(uint32_t i=0; i < subpass; i++)
		ObjDisp(cmd)->CmdNextSubpass(Unwrap(cmd), VK_SUBPASS_CONTENTS_INLINE);

	BindPipeline(cmd);

	if(!views.empty())
		ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), (uint32_t)views.size(), &views[0]);
	if(!scissors.empty())
		ObjDisp(cmd)->CmdSetScissor(Unwrap(cmd), (uint32_t)scissors.size(), &scissors[0]);

	ObjDisp(cmd)->CmdSetBlendConstants(Unwrap(cmd), blendConst);
	ObjDisp(cmd)->CmdSetDepthBounds(Unwrap(cmd), mindepth, maxdepth);
	ObjDisp(cmd)->CmdSetLineWidth(Unwrap(cmd), lineWidth);
	ObjDisp(cmd)->CmdSetDepthBias(Unwrap(cmd), bias.depth, bias.biasclamp, bias.slope);

	ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.ref);
	ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.compare);
	ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, back.write);

	ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.ref);
	ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.compare);
	ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, front.write);

	if(ibuffer.buf != ResourceId())
		ObjDisp(cmd)->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(GetResourceManager()->GetCurrentHandle<VkBuffer>(ibuffer.buf)), ibuffer.offs, ibuffer.bytewidth == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);

	for(size_t i=0; i < vbuffers.size(); i++)
		ObjDisp(cmd)->CmdBindVertexBuffers(Unwrap(cmd), (uint32_t)i, 1, UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkBuffer>(vbuffers[i].buf)), &vbuffers[i].offs);
}

void VulkanRenderState::BindPipeline(VkCommandBuffer cmd)
{
	if(graphics.pipeline != ResourceId())
	{
		ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetResourceManager()->GetCurrentHandle<VkPipeline>(graphics.pipeline)));

		ResourceId pipeLayoutId = m_CreationInfo.m_Pipeline[graphics.pipeline].layout;
		VkPipelineLayout layout = GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

		const vector<VkPushConstantRange> &pushRanges = m_CreationInfo.m_PipelineLayout[pipeLayoutId].pushRanges;

		// only set push constant ranges that the layout uses
		for(size_t i=0; i < pushRanges.size(); i++)
			ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags, pushRanges[i].offset, pushRanges[i].size, pushconsts+pushRanges[i].offset);

		const vector<ResourceId> &descSetLayouts = m_CreationInfo.m_PipelineLayout[pipeLayoutId].descSetLayouts;

		// only iterate over the desc sets that this layout actually uses, not all that were bound
		for(size_t i=0; i < descSetLayouts.size(); i++)
		{
			const DescSetLayout &descLayout = m_CreationInfo.m_DescSetLayout[ descSetLayouts[i] ];

			if(i < graphics.descSets.size() && graphics.descSets[i] != ResourceId())
			{
				// if there are dynamic buffers, pass along the offsets
				ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(layout), (uint32_t)i,
					1, UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkDescriptorSet>(graphics.descSets[i])),
					descLayout.dynamicCount, descLayout.dynamicCount == 0 ? NULL : &graphics.offsets[i][0]);
			}
			else
			{
				RDCWARN("Descriptor set is not bound but pipeline layout expects one");
			}
		}
	}

	if(compute.pipeline != ResourceId())
	{
		ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetResourceManager()->GetCurrentHandle<VkPipeline>(compute.pipeline)));

		ResourceId pipeLayoutId = m_CreationInfo.m_Pipeline[compute.pipeline].layout;
		VkPipelineLayout layout = GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

		const vector<VkPushConstantRange> &pushRanges = m_CreationInfo.m_PipelineLayout[pipeLayoutId].pushRanges;

		// only set push constant ranges that the layout uses
		for(size_t i=0; i < pushRanges.size(); i++)
			ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(layout), pushRanges[i].stageFlags, pushRanges[i].offset, pushRanges[i].size, pushconsts+pushRanges[i].offset);

		const vector<ResourceId> &descSetLayouts = m_CreationInfo.m_PipelineLayout[pipeLayoutId].descSetLayouts;

		for(size_t i=0; i < descSetLayouts.size(); i++)
		{
			const DescSetLayout &descLayout = m_CreationInfo.m_DescSetLayout[ descSetLayouts[i] ];

			if(compute.descSets[i] != ResourceId())
			{
				ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(layout), (uint32_t)i,
					1, UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkDescriptorSet>(compute.descSets[i])),
					descLayout.dynamicCount, descLayout.dynamicCount == 0 ? NULL : &compute.offsets[i][0]);
			}
		}
	}
}
