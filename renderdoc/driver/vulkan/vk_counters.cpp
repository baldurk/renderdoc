/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "vk_replay.h"
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

vector<GPUCounter> VulkanReplay::EnumerateCounters()
{
  vector<GPUCounter> ret;

  VkPhysicalDeviceFeatures availableFeatures = m_pDriver->GetDeviceFeatures();

  ret.push_back(GPUCounter::EventGPUDuration);
  if(availableFeatures.pipelineStatisticsQuery)
  {
    ret.push_back(GPUCounter::InputVerticesRead);
    ret.push_back(GPUCounter::IAPrimitives);
    ret.push_back(GPUCounter::GSPrimitives);
    ret.push_back(GPUCounter::RasterizerInvocations);
    ret.push_back(GPUCounter::RasterizedPrimitives);
  }

  if(availableFeatures.occlusionQueryPrecise)
    ret.push_back(GPUCounter::SamplesWritten);

  if(availableFeatures.pipelineStatisticsQuery)
  {
    ret.push_back(GPUCounter::VSInvocations);
    ret.push_back(GPUCounter::TCSInvocations);
    ret.push_back(GPUCounter::TESInvocations);
    ret.push_back(GPUCounter::GSInvocations);
    ret.push_back(GPUCounter::PSInvocations);
    ret.push_back(GPUCounter::CSInvocations);
  }

  return ret;
}

void VulkanReplay::DescribeCounter(GPUCounter counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

  switch(counterID)
  {
    case GPUCounter::EventGPUDuration:
      desc.name = "GPU Duration";
      desc.description =
          "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::Double;
      desc.unit = CounterUnit::Seconds;
      break;
    case GPUCounter::InputVerticesRead:
      desc.name = "Input Vertices Read";
      desc.description = "Number of vertices read by input assembler.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::IAPrimitives:
      desc.name = "Input Primitives";
      desc.description = "Number of primitives read by the input assembler.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::GSPrimitives:
      desc.name = "GS Primitives";
      desc.description = "Number of primitives output by a geometry shader.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::RasterizerInvocations:
      desc.name = "Rasterizer Invocations";
      desc.description = "Number of primitives that were sent to the rasterizer.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::RasterizedPrimitives:
      desc.name = "Rasterized Primitives";
      desc.description = "Number of primitives that were rendered.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::SamplesWritten:
      desc.name = "Samples Written";
      desc.description = "Number of samples that passed depth/stencil test.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::VSInvocations:
      desc.name = "VS Invocations";
      desc.description = "Number of times a vertex shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::GSInvocations:
      desc.name = "GS Invocations";
      desc.description = "Number of times a geometry shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::TCSInvocations:
      desc.name = "TCS Invocations";
      desc.description = "Number of times a tesselation control shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::TESInvocations:
      desc.name = "TES Invocations";
      desc.description = "Number of times a tesselation evaluation shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::PSInvocations:
      desc.name = "PS Invocations";
      desc.description = "Number of times a pixel shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::CSInvocations:
      desc.name = "CS Invocations";
      desc.description = "Number of times a compute shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    default:
      desc.name = "Unknown";
      desc.description = "Unknown counter ID";
      desc.resultByteWidth = 0;
      desc.resultType = CompType::Typeless;
      desc.unit = CounterUnit::Absolute;
      break;
  }
}

struct VulkanGPUTimerCallback : public VulkanDrawcallCallback
{
  VulkanGPUTimerCallback(WrappedVulkan *vk, VulkanReplay *rp, VkQueryPool tsqp, VkQueryPool occqp,
                         VkQueryPool psqp)
      : m_pDriver(vk),
        m_pReplay(rp),
        m_TimeStampQueryPool(tsqp),
        m_OcclusionQueryPool(occqp),
        m_PipeStatsQueryPool(psqp)
  {
    m_pDriver->SetDrawcallCB(this);
  }
  ~VulkanGPUTimerCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_OcclusionQueryPool != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionQueryPool, (uint32_t)m_Results.size(),
                                  VK_QUERY_CONTROL_PRECISE_BIT);
    if(m_PipeStatsQueryPool != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_PipeStatsQueryPool, (uint32_t)m_Results.size(), 0);
    ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    m_TimeStampQueryPool, (uint32_t)(m_Results.size() * 2 + 0));
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    m_TimeStampQueryPool, (uint32_t)(m_Results.size() * 2 + 1));
    if(m_OcclusionQueryPool != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_OcclusionQueryPool, (uint32_t)m_Results.size());
    if(m_PipeStatsQueryPool != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_PipeStatsQueryPool, (uint32_t)m_Results.size());
    m_Results.push_back(eid);
    return false;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) {}
  // we don't need to distinguish, call the Draw functions
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return PostDraw(eid, cmd); }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) { PostRedraw(eid, cmd); }
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { PreDraw(eid, cmd); }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return PostDraw(eid, cmd); }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { PostRedraw(eid, cmd); }
  bool RecordAllCmds() { return true; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    m_AliasEvents.push_back(std::make_pair(primary, alias));
  }

  WrappedVulkan *m_pDriver;
  VulkanReplay *m_pReplay;
  VkQueryPool m_TimeStampQueryPool;
  VkQueryPool m_OcclusionQueryPool;
  VkQueryPool m_PipeStatsQueryPool;
  vector<uint32_t> m_Results;
  // events which are the 'same' from being the same command buffer resubmitted
  // multiple times in the frame. We will only get the full callback when we're
  // recording the command buffer, and will be given the first EID. After that
  // we'll just be told which other EIDs alias this event.
  vector<pair<uint32_t, uint32_t> > m_AliasEvents;
};

vector<CounterResult> VulkanReplay::FetchCounters(const vector<GPUCounter> &counters)
{
  uint32_t maxEID = m_pDriver->GetMaxEID();

  VkPhysicalDeviceFeatures availableFeatures = m_pDriver->GetDeviceFeatures();

  VkDevice dev = m_pDriver->GetDev();

  VkQueryPoolCreateInfo timeStampPoolCreateInfo = {
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, NULL, 0, VK_QUERY_TYPE_TIMESTAMP, maxEID * 2, 0};

  VkQueryPoolCreateInfo occlusionPoolCreateInfo = {
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, NULL, 0, VK_QUERY_TYPE_OCCLUSION, maxEID, 0};

  VkQueryPipelineStatisticFlags pipeStatsFlags =
      VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
      VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
      VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;

  VkQueryPoolCreateInfo pipeStatsPoolCreateInfo = {
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, NULL,   0,
      VK_QUERY_TYPE_PIPELINE_STATISTICS,        maxEID, pipeStatsFlags};

  VkQueryPool timeStampPool;
  VkResult vkr =
      ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &timeStampPoolCreateInfo, NULL, &timeStampPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkQueryPool occlusionPool = VK_NULL_HANDLE;
  if(availableFeatures.occlusionQueryPrecise)
  {
    vkr = ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &occlusionPoolCreateInfo, NULL, &occlusionPool);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkQueryPool pipeStatsPool = VK_NULL_HANDLE;
  if(availableFeatures.pipelineStatisticsQuery)
  {
    vkr = ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &pipeStatsPoolCreateInfo, NULL, &pipeStatsPool);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), timeStampPool, 0, maxEID * 2);
  if(occlusionPool != VK_NULL_HANDLE)
    ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), occlusionPool, 0, maxEID);
  if(pipeStatsPool != VK_NULL_HANDLE)
    ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), pipeStatsPool, 0, maxEID);

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif

  VulkanGPUTimerCallback cb(m_pDriver, this, timeStampPool, occlusionPool, pipeStatsPool);

  // replay the events to perform all the queries
  m_pDriver->ReplayLog(0, maxEID, eReplay_Full);

  vector<uint64_t> m_TimeStampData;
  m_TimeStampData.resize(cb.m_Results.size() * 2);

  vkr = ObjDisp(dev)->GetQueryPoolResults(
      Unwrap(dev), timeStampPool, 0, (uint32_t)m_TimeStampData.size(),
      sizeof(uint64_t) * m_TimeStampData.size(), &m_TimeStampData[0], sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), timeStampPool, NULL);

  vector<uint64_t> m_OcclusionData;
  m_OcclusionData.resize(cb.m_Results.size());
  if(occlusionPool != VK_NULL_HANDLE)
  {
    vkr = ObjDisp(dev)->GetQueryPoolResults(
        Unwrap(dev), occlusionPool, 0, (uint32_t)m_OcclusionData.size(),
        sizeof(uint64_t) * m_OcclusionData.size(), &m_OcclusionData[0], sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), occlusionPool, NULL);
  }

  vector<uint64_t> m_PipeStatsData;
  m_PipeStatsData.resize(cb.m_Results.size() * 11);
  if(pipeStatsPool != VK_NULL_HANDLE)
  {
    vkr = ObjDisp(dev)->GetQueryPoolResults(
        Unwrap(dev), pipeStatsPool, 0, (uint32_t)cb.m_Results.size(),
        sizeof(uint64_t) * m_PipeStatsData.size(), &m_PipeStatsData[0], sizeof(uint64_t) * 11,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), pipeStatsPool, NULL);
  }

  vector<CounterResult> ret;

  for(size_t i = 0; i < cb.m_Results.size(); i++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult result;

      result.eventID = cb.m_Results[i];
      result.counterID = counters[c];

      switch(counters[c])
      {
        case GPUCounter::EventGPUDuration:
        {
          uint64_t delta = m_TimeStampData[i * 2 + 1] - m_TimeStampData[i * 2 + 0];
          result.value.d = (double(m_pDriver->GetDeviceProps().limits.timestampPeriod) *
                            double(delta))                  // nanoseconds
                           / (1000.0 * 1000.0 * 1000.0);    // to seconds
        }
        break;
        case GPUCounter::InputVerticesRead: result.value.u64 = m_PipeStatsData[i * 11 + 0]; break;
        case GPUCounter::IAPrimitives: result.value.u64 = m_PipeStatsData[i * 11 + 1]; break;
        case GPUCounter::GSPrimitives: result.value.u64 = m_PipeStatsData[i * 11 + 4]; break;
        case GPUCounter::RasterizerInvocations:
          result.value.u64 = m_PipeStatsData[i * 11 + 5];
          break;
        case GPUCounter::RasterizedPrimitives:
          result.value.u64 = m_PipeStatsData[i * 11 + 6];
          break;
        case GPUCounter::SamplesWritten: result.value.u64 = m_OcclusionData[i]; break;
        case GPUCounter::VSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 2]; break;
        case GPUCounter::TCSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 8]; break;
        case GPUCounter::TESInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 9]; break;
        case GPUCounter::GSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 3]; break;
        case GPUCounter::PSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 9]; break;
        case GPUCounter::CSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 10]; break;
        default: break;
      }
      ret.push_back(result);
    }
  }

  for(size_t i = 0; i < cb.m_AliasEvents.size(); i++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult search;
      search.counterID = counters[c];
      search.eventID = cb.m_AliasEvents[i].first;

      // find the result we're aliasing
      auto it = std::find(ret.begin(), ret.end(), search);
      RDCASSERT(it != ret.end());

      // duplicate the result and append
      CounterResult aliased = *it;
      aliased.eventID = cb.m_AliasEvents[i].second;
      ret.push_back(aliased);
    }
  }

  // sort so that the alias results appear in the right places
  std::sort(ret.begin(), ret.end());

  return ret;
}
