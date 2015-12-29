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

#include "vk_replay.h"
#include "vk_core.h"
#include "vk_resources.h"

void VulkanReplay::PreDeviceInitCounters()
{
}

void VulkanReplay::PostDeviceInitCounters()
{
}

void VulkanReplay::PreDeviceShutdownCounters()
{
}

void VulkanReplay::PostDeviceShutdownCounters()
{
}

vector<uint32_t> VulkanReplay::EnumerateCounters()
{
	vector<uint32_t> ret;

	ret.push_back(eCounter_EventGPUDuration);

	return ret;
}

void VulkanReplay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
	desc.counterID = counterID;

	if(counterID == eCounter_EventGPUDuration)
	{
		desc.name = "GPU Duration";
		desc.description = "Time taken for this event on the GPU, as measured by delta between two GPU timestamps, top to bottom of the pipe.";
		desc.resultByteWidth = 8;
		desc.resultCompType = eCompType_Double;
		desc.units = eUnits_Seconds;
	}
	else
	{
		desc.name = "Unknown";
		desc.description = "Unknown counter ID";
		desc.resultByteWidth = 0;
		desc.resultCompType = eCompType_None;
		desc.units = eUnits_Absolute;
	}
}
struct GPUTimerCallback : public DrawcallCallback
{
	GPUTimerCallback(WrappedVulkan *vk, VulkanReplay *rp, VkQueryPool qp)
		: m_pDriver(vk)
		, m_pReplay(rp)
		, m_QueryPool(qp)
	{ m_pDriver->SetDrawcallCB(this); }
	~GPUTimerCallback()
	{ m_pDriver->SetDrawcallCB(NULL); }

	void PreDraw(uint32_t eid, VkCommandBuffer cmd)
	{
		ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_QueryPool, (uint32_t)(m_Results.size()*2 + 0));
	}

	bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
	{
		ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, m_QueryPool, (uint32_t)(m_Results.size()*2 + 1));
		m_Results.push_back(eid);
		return false;
	}

	void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
	{
	}

	bool RecordAllCmds()
	{
		return true;
	}

	WrappedVulkan *m_pDriver;
	VulkanReplay *m_pReplay;
	VkQueryPool m_QueryPool;
	vector<uint32_t> m_Results;
};

vector<CounterResult> VulkanReplay::FetchCounters(uint32_t frameID, const vector<uint32_t> &counters)
{
	uint32_t maxEID = m_pDriver->GetMaxEID();

	VkDevice dev = m_pDriver->GetDev();

	VkQueryPoolCreateInfo poolCreateInfo = {
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, NULL,
		0, VK_QUERY_TYPE_TIMESTAMP, maxEID*2, 0
	};

	VkQueryPool pool;
	VkResult vkr = ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &poolCreateInfo, NULL, &pool);
	RDCASSERT(vkr == VK_SUCCESS);

	VkCommandBuffer cmd = m_pDriver->GetNextCmd();

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
	
	vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
	RDCASSERT(vkr == VK_SUCCESS);

	ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), pool, 0, maxEID*2);
	
	vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
	RDCASSERT(vkr == VK_SUCCESS);

	GPUTimerCallback cb(m_pDriver, this, pool);

	// replay the events to perform all the queries
	m_pDriver->ReplayLog(frameID, 0, maxEID, eReplay_Full);

	vector<uint64_t> m_Data;
	m_Data.resize(cb.m_Results.size()*2);

	vkr = ObjDisp(dev)->GetQueryPoolResults(Unwrap(dev), pool, 0, (uint32_t)m_Data.size(),
	           sizeof(uint64_t)*m_Data.size(), &m_Data[0], sizeof(uint64_t),
	           VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT);
	RDCASSERT(vkr == VK_SUCCESS);

	ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), pool, NULL);

	vector<CounterResult> ret;

	for(size_t i=0; i < cb.m_Results.size(); i++)
	{
		CounterResult result;

		uint64_t delta = m_Data[i*2 + 1] - m_Data[i*2 + 0];
		
		result.eventID = cb.m_Results[i];
		result.counterID = eCounter_EventGPUDuration;
		result.value.d =
			(double(m_pDriver->GetDeviceProps().limits.timestampPeriod) * double(delta)) // nanoseconds
			/
			(1000.0*1000.0*1000.0);  // to seconds

		ret.push_back(result);
	}

	return ret;
}

