/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <algorithm>
#include <iterator>
#include "vk_core.h"
#include "vk_replay.h"
#include "vk_resources.h"

#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/amd/official/GPUPerfAPI/Include/GPUPerfAPI-VK.h"
#include "strings/string_utils.h"

static uint32_t FromKHRCounter(GPUCounter counterID)
{
  return (uint32_t)counterID - (uint32_t)GPUCounter::FirstVulkanExtended;
}

static GPUCounter ToKHRCounter(uint32_t idx)
{
  return (GPUCounter)((uint32_t)GPUCounter::FirstVulkanExtended + idx);
}

static bool isFloatKhrStorage(const VkPerformanceCounterStorageKHR khrStorage)
{
  return khrStorage == VK_PERFORMANCE_COUNTER_STORAGE_FLOAT32_KHR ||
         khrStorage == VK_PERFORMANCE_COUNTER_STORAGE_FLOAT64_KHR;
}

static void GetKHRUnitDescription(const VkPerformanceCounterUnitKHR khrUnit,
                                  const VkPerformanceCounterStorageKHR khrStorage,
                                  CounterUnit &unit, CompType &type, uint32_t &byteWidth)
{
  type = isFloatKhrStorage(khrStorage) ? CompType::Float : CompType::UInt;
  byteWidth = 8;

  switch(khrUnit)
  {
    case VK_PERFORMANCE_COUNTER_UNIT_GENERIC_KHR: unit = CounterUnit::Absolute; return;
    case VK_PERFORMANCE_COUNTER_UNIT_PERCENTAGE_KHR: unit = CounterUnit::Percentage; return;
    case VK_PERFORMANCE_COUNTER_UNIT_NANOSECONDS_KHR: unit = CounterUnit::Seconds; return;
    case VK_PERFORMANCE_COUNTER_UNIT_BYTES_KHR: unit = CounterUnit::Bytes; return;
    case VK_PERFORMANCE_COUNTER_UNIT_BYTES_PER_SECOND_KHR: unit = CounterUnit::Ratio; return;
    case VK_PERFORMANCE_COUNTER_UNIT_KELVIN_KHR: unit = CounterUnit::Absolute; return;
    case VK_PERFORMANCE_COUNTER_UNIT_WATTS_KHR: unit = CounterUnit::Absolute; return;
    case VK_PERFORMANCE_COUNTER_UNIT_VOLTS_KHR: unit = CounterUnit::Absolute; return;
    case VK_PERFORMANCE_COUNTER_UNIT_AMPS_KHR: unit = CounterUnit::Absolute; return;
    case VK_PERFORMANCE_COUNTER_UNIT_HERTZ_KHR: unit = CounterUnit::Absolute; return;
    case VK_PERFORMANCE_COUNTER_UNIT_CYCLES_KHR: unit = CounterUnit::Cycles; return;
    default: RDCERR("Invalid performance counter unit %d", khrUnit);
  }
}

void VulkanReplay::convertKhrCounterResult(CounterResult &rdcResult,
                                           const VkPerformanceCounterResultKHR &khrResult,
                                           VkPerformanceCounterUnitKHR khrUnit,
                                           VkPerformanceCounterStorageKHR khrStorage)
{
  CounterUnit unit;
  CompType type;
  uint32_t byteWidth;
  GetKHRUnitDescription(khrUnit, khrStorage, unit, type, byteWidth);

  double value;

  // Convert everything to doubles.
  switch(khrStorage)
  {
    case VK_PERFORMANCE_COUNTER_STORAGE_INT32_KHR: rdcResult.value.u64 = khrResult.int32; break;
    case VK_PERFORMANCE_COUNTER_STORAGE_UINT32_KHR: rdcResult.value.u64 = khrResult.uint32; break;
    case VK_PERFORMANCE_COUNTER_STORAGE_INT64_KHR: rdcResult.value.u64 = khrResult.int64; break;
    case VK_PERFORMANCE_COUNTER_STORAGE_UINT64_KHR: rdcResult.value.u64 = khrResult.uint64; break;
    case VK_PERFORMANCE_COUNTER_STORAGE_FLOAT32_KHR: rdcResult.value.d = khrResult.float32; break;
    case VK_PERFORMANCE_COUNTER_STORAGE_FLOAT64_KHR: rdcResult.value.d = khrResult.float64; break;
    default: value = 0; RDCERR("Wrong counter storage type %d", khrStorage);
  }

  // Special case for time units, renderdoc only has a Seconds type.
  if(khrUnit == VK_PERFORMANCE_COUNTER_UNIT_NANOSECONDS_KHR)
  {
    if((khrStorage == VK_PERFORMANCE_COUNTER_STORAGE_FLOAT64_KHR) ||
       (khrStorage == VK_PERFORMANCE_COUNTER_STORAGE_FLOAT32_KHR))
    {
      rdcResult.value.d = rdcResult.value.d / (1000.0 * 1000.0 * 1000.0);
    }
    else
    {
      rdcResult.value.d = (double)(rdcResult.value.u64) / (1000.0 * 1000.0 * 1000.0);
    }
  }
}

rdcarray<GPUCounter> VulkanReplay::EnumerateCounters()
{
  rdcarray<GPUCounter> ret;

  VkPhysicalDeviceFeatures availableFeatures = m_pDriver->GetDeviceEnabledFeatures();

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
    ret.push_back(GPUCounter::SamplesPassed);

  if(availableFeatures.pipelineStatisticsQuery)
  {
    ret.push_back(GPUCounter::VSInvocations);
    ret.push_back(GPUCounter::TCSInvocations);
    ret.push_back(GPUCounter::TESInvocations);
    ret.push_back(GPUCounter::GSInvocations);
    ret.push_back(GPUCounter::PSInvocations);
    ret.push_back(GPUCounter::CSInvocations);
  }

  if(m_pDriver->GetPhysicalDevicePerformanceQueryFeatures().performanceCounterQueryPools)
  {
    VkPhysicalDevice physDev = m_pDriver->GetPhysDev();
    uint32_t khrCounters = 0;
    ObjDisp(physDev)->EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
        Unwrap(physDev), 0, &khrCounters, NULL, NULL);

    m_KHRCounters.resize(khrCounters);
    m_KHRCountersDescriptions.resize(khrCounters);

    ObjDisp(physDev)->EnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
        Unwrap(physDev), 0, &khrCounters, &m_KHRCounters[0], &m_KHRCountersDescriptions[0]);

    for(uint32_t c = 0; c < khrCounters; c++)
    {
      // Only report counters with command scope. We currently don't
      // have use for renderpass ones.
      if(m_KHRCounters[c].scope == VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_KHR)
        ret.push_back(ToKHRCounter(c));
    }
  }

  if(m_pAMDCounters)
  {
    ret.append(m_pAMDCounters->GetPublicCounterIds());
  }

  return ret;
}

CounterDescription VulkanReplay::DescribeCounter(GPUCounter counterID)
{
  CounterDescription desc = {};
  desc.counter = counterID;

  /////AMD//////
  if(IsAMDCounter(counterID))
  {
    if(m_pAMDCounters)
    {
      desc = m_pAMDCounters->GetCounterDescription(counterID);

      return desc;
    }
  }

  if(IsVulkanExtendedCounter(counterID))
  {
    const VkPerformanceCounterKHR &khrCounter = m_KHRCounters[FromKHRCounter(counterID)];
    const VkPerformanceCounterDescriptionKHR &khrCounterDesc =
        m_KHRCountersDescriptions[FromKHRCounter(counterID)];

    CounterDescription rdcDesc;
    rdcDesc.counter = counterID;
    rdcDesc.name = khrCounterDesc.name;
    rdcDesc.category = khrCounterDesc.category;
    rdcDesc.description = khrCounterDesc.description;

    const uint32_t *uuid_dwords = (const uint32_t *)khrCounter.uuid;
    desc.uuid.words[0] = uuid_dwords[0];
    desc.uuid.words[1] = uuid_dwords[1];
    desc.uuid.words[2] = uuid_dwords[2];
    desc.uuid.words[3] = uuid_dwords[3];

    GetKHRUnitDescription(khrCounter.unit, khrCounter.storage, rdcDesc.unit, rdcDesc.resultType,
                          rdcDesc.resultByteWidth);

    // Special chase for time units.
    if(khrCounter.unit == VK_PERFORMANCE_COUNTER_UNIT_NANOSECONDS_KHR)
      rdcDesc.resultType = CompType::Float;

    return rdcDesc;
  }

  // 6839CB5B-FBD2-4550-B606-8C65157C684C
  desc.uuid.words[0] = 0x6839CB5B;
  desc.uuid.words[1] = 0xFBD24550;
  desc.uuid.words[2] = 0xB6068C65;
  desc.uuid.words[3] = 0x157C684C ^ (uint32_t)counterID;

  desc.category = "Vulkan Built-in";

  switch(counterID)
  {
    case GPUCounter::EventGPUDuration:
      desc.name = "GPU Duration";
      desc.description =
          "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::Float;
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
    case GPUCounter::SamplesPassed:
      desc.name = "Samples Passed";
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

  return desc;
}

struct VulkanAMDDrawCallback : public VulkanDrawcallCallback
{
  VulkanAMDDrawCallback(WrappedVulkan *dev, VulkanReplay *rp, uint32_t &sampleIndex,
                        rdcarray<uint32_t> &eventIDs)
      : m_pDriver(dev), m_pReplay(rp), m_pSampleId(&sampleIndex), m_pEventIds(&eventIDs)
  {
    m_pDriver->SetDrawcallCB(this);
  }

  virtual ~VulkanAMDDrawCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd) override
  {
    m_pEventIds->push_back(eid);

    VkCommandBuffer realCmdBuffer = Unwrap(cmd);

    if(m_begunCommandBuffers.find(realCmdBuffer) == m_begunCommandBuffers.end())
    {
      m_begunCommandBuffers.insert(realCmdBuffer);

      m_pReplay->GetAMDCounters()->BeginCommandList(realCmdBuffer);
    }

    m_pReplay->GetAMDCounters()->BeginSample(*m_pSampleId, realCmdBuffer);

    ++*m_pSampleId;
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) override
  {
    VkCommandBuffer realCmdBuffer = Unwrap(cmd);

    m_pReplay->GetAMDCounters()->EndSample(realCmdBuffer);
    return false;
  }

  void PreEndCommandBuffer(VkCommandBuffer cmd) override
  {
    VkCommandBuffer realCmdBuffer = Unwrap(cmd);

    auto iter = m_begunCommandBuffers.find(realCmdBuffer);

    if(iter != m_begunCommandBuffers.end())
    {
      m_pReplay->GetAMDCounters()->EndCommandList(*iter);
      m_begunCommandBuffers.erase(iter);
    }
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) override {}
  // we don't need to distinguish, call the Draw functions
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) override { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) override { return PostDraw(eid, cmd); }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) override { PostRedraw(eid, cmd); }
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    if(flags & DrawFlags::PassBoundary)
      return;
    PreDraw(eid, cmd);
  }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    if(flags & DrawFlags::PassBoundary)
      return false;
    return PostDraw(eid, cmd);
  }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    if(flags & DrawFlags::PassBoundary)
      return;
    PostRedraw(eid, cmd);
  }

  void AliasEvent(uint32_t primary, uint32_t alias) override
  {
    m_AliasEvents.push_back(make_rdcpair(primary, alias));
  }
  bool SplitSecondary() override { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd) override
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd) override
  {
  }

  uint32_t *m_pSampleId;
  WrappedVulkan *m_pDriver;
  VulkanReplay *m_pReplay;
  rdcarray<uint32_t> *m_pEventIds;
  std::set<VkCommandBuffer> m_begunCommandBuffers;
  // events which are the 'same' from being the same command buffer resubmitted
  // multiple times in the frame. We will only get the full callback when we're
  // recording the command buffer, and will be given the first EID. After that
  // we'll just be told which other EIDs alias this event.
  rdcarray<rdcpair<uint32_t, uint32_t> > m_AliasEvents;
};

void VulkanReplay::FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex,
                                 rdcarray<uint32_t> *eventIDs)
{
  uint32_t maxEID = m_pDriver->GetMaxEID();

  m_pAMDDrawCallback = new VulkanAMDDrawCallback(m_pDriver, this, *sampleIndex, *eventIDs);

  // replay the events to perform all the queries
  m_pDriver->ReplayLog(*eventStartID, maxEID, eReplay_Full);
}

rdcarray<CounterResult> VulkanReplay::FetchCountersAMD(const rdcarray<GPUCounter> &counters)
{
  GPA_vkContextOpenInfo context = {Unwrap(m_pDriver->GetInstance()),
                                   Unwrap(m_pDriver->GetPhysDev()), Unwrap(m_pDriver->GetDev())};

  if(!m_pAMDCounters->BeginMeasurementMode(AMDCounters::ApiType::Vk, (void *)&context))
  {
    return {};
  }

  uint32_t sessionID = m_pAMDCounters->CreateSession();
  m_pAMDCounters->DisableAllCounters();

  // enable counters it needs
  for(size_t i = 0; i < counters.size(); i++)
  {
    // This function is only called internally, and violating this assertion means our
    // caller has invoked this method incorrectly
    RDCASSERT(IsAMDCounter(counters[i]));
    m_pAMDCounters->EnableCounter(counters[i]);
  }

  m_pAMDCounters->BeginSession(sessionID);

  uint32_t passCount = m_pAMDCounters->GetPassCount();

  uint32_t sampleIndex = 0;

  rdcarray<uint32_t> eventIDs;

  for(uint32_t i = 0; i < passCount; i++)
  {
    m_pAMDCounters->BeginPass();

    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersAMD(&eventStartID, &sampleIndex, &eventIDs);

    m_pAMDCounters->EndPass();
  }

  m_pAMDCounters->EndSesssion(sessionID);

  rdcarray<CounterResult> ret =
      m_pAMDCounters->GetCounterData(sessionID, sampleIndex, eventIDs, counters);

  for(size_t i = 0; i < m_pAMDDrawCallback->m_AliasEvents.size(); i++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult search;
      search.counter = counters[c];
      search.eventId = m_pAMDDrawCallback->m_AliasEvents[i].first;

      // find the result we're aliasing
      int32_t idx = ret.indexOf(search);
      if(idx >= 0)
      {
        // duplicate the result and append
        CounterResult aliased = ret[idx];
        aliased.eventId = m_pAMDDrawCallback->m_AliasEvents[i].second;
        ret.push_back(aliased);
      }
      else
      {
        RDCERR("Expected to find alias-target result for EID %u counter %u, but didn't",
               search.eventId, search.counter);
      }
    }
  }

  SAFE_DELETE(m_pAMDDrawCallback);

  // sort so that the alias results appear in the right places
  std::sort(ret.begin(), ret.end());

  m_pAMDCounters->EndMeasurementMode();

  return ret;
}

struct VulkanKHRCallback : public VulkanDrawcallCallback
{
  VulkanKHRCallback(WrappedVulkan *vk, VulkanReplay *rp, VkQueryPool qp)
      : m_pDriver(vk), m_pReplay(rp), m_QueryPool(qp)
  {
    m_pDriver->SetDrawcallCB(this);
  }
  ~VulkanKHRCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd) override
  {
    ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_QueryPool, (uint32_t)m_Results.size(), 0);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) override
  {
    ObjDisp(cmd)->CmdEndQuery(Unwrap(cmd), m_QueryPool, (uint32_t)m_Results.size());
    m_Results.push_back(eid);
    return false;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) override {}
  // we don't need to distinguish, call the Draw functions
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) override { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) override { return PostDraw(eid, cmd); }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) override { PostRedraw(eid, cmd); }
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override { PreDraw(eid, cmd); }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    return PostDraw(eid, cmd);
  }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    PostRedraw(eid, cmd);
  }
  void AliasEvent(uint32_t primary, uint32_t alias) override
  {
    m_AliasEvents.push_back(std::make_pair(primary, alias));
  }
  bool SplitSecondary() override { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd) override
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd) override
  {
  }

  void PreEndCommandBuffer(VkCommandBuffer cmd) override {}
  WrappedVulkan *m_pDriver;
  VulkanReplay *m_pReplay;
  VkQueryPool m_QueryPool;
  rdcarray<uint32_t> m_Results;
  // events which are the 'same' from being the same command buffer resubmitted
  // multiple times in the frame. We will only get the full callback when we're
  // recording the command buffer, and will be given the first EID. After that
  // we'll just be told which other EIDs alias this event.
  rdcarray<std::pair<uint32_t, uint32_t> > m_AliasEvents;
};

rdcarray<CounterResult> VulkanReplay::FetchCountersKHR(const rdcarray<GPUCounter> &counters)
{
  rdcarray<uint32_t> counterIndices;
  for(const GPUCounter &c : counters)
    counterIndices.push_back(FromKHRCounter(c));

  VkQueryPoolPerformanceCreateInfoKHR perfCreateInfo = {
      VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR, NULL, 0,
      (uint32_t)counterIndices.size(), &counterIndices[0]};
  uint32_t passCount = 0;
  ObjDisp(m_pDriver->GetInstance())
      ->GetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(Unwrap(m_pDriver->GetPhysDev()),
                                                              &perfCreateInfo, &passCount);

  VkDevice dev = m_pDriver->GetDev();
  VkAcquireProfilingLockInfoKHR acquireLockInfo = {
      VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR, NULL, 0, 50 * 1000 * 1000 /* 50ms */};
  VkResult vkr = ObjDisp(dev)->AcquireProfilingLockKHR(Unwrap(dev), &acquireLockInfo);
  if(vkr != VK_SUCCESS)
  {
    RDCWARN("Unable to acquire profiling lock: %s", ToStr(vkr).c_str());
    return {};
  }

  uint32_t maxEID = m_pDriver->GetMaxEID();
  VkQueryPoolCreateInfo queryPoolCreateInfo = {
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, &perfCreateInfo, 0,
      VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR,      maxEID,          0};

  VkQueryPool queryPool;
  vkr = ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &queryPoolCreateInfo, NULL, &queryPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // Reset query pool
  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ObjDisp(dev)->CmdResetQueryPool(Unwrap(cmd), queryPool, 0, maxEID);

  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  m_pDriver->SubmitCmds();

  VulkanKHRCallback cb(m_pDriver, this, queryPool);

  // replay the events to perform all the queries
  for(uint32_t i = 0; i < passCount; i++)
  {
    VkPerformanceQuerySubmitInfoKHR perfSubmitInfo = {
        VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR, NULL, i};

    cb.m_Results.clear();

    m_pDriver->SetSubmitChain(&perfSubmitInfo);
    m_pDriver->ReplayLog(0, maxEID, eReplay_Full);
    m_pDriver->SetSubmitChain(NULL);
  }

  rdcarray<VkPerformanceCounterResultKHR> perfResults;
  perfResults.resize(cb.m_Results.size() * counters.size());

  vkr = ObjDisp(dev)->GetQueryPoolResults(
      Unwrap(dev), queryPool, 0, (uint32_t)cb.m_Results.size(),
      sizeof(VkPerformanceCounterResultKHR) * perfResults.size(), &perfResults[0],
      sizeof(VkPerformanceCounterResultKHR) * counters.size(), VK_QUERY_RESULT_WAIT_BIT);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), queryPool, NULL);

  ObjDisp(dev)->ReleaseProfilingLockKHR(Unwrap(dev));

  rdcarray<CounterResult> ret;
  for(size_t i = 0; i < cb.m_Results.size(); i++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult result;

      result.eventId = cb.m_Results[i];
      result.counter = counters[c];

      const VkPerformanceCounterKHR &khrCounter = m_KHRCounters[counterIndices[c]];

      convertKhrCounterResult(result, perfResults[counters.size() * i + c], khrCounter.unit,
                              khrCounter.storage);
      ret.push_back(result);
    }
  }

  for(size_t i = 0; i < cb.m_AliasEvents.size(); i++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      CounterResult search;
      search.counter = counters[c];
      search.eventId = cb.m_AliasEvents[i].first;

      // find the result we're aliasing
      int32_t idx = ret.indexOf(search);
      if(idx >= 0)
      {
        // duplicate the result and append
        CounterResult aliased = ret[idx];
        aliased.eventId = cb.m_AliasEvents[i].second;
        ret.push_back(aliased);
      }
      else
      {
        RDCERR("Expected to find alias-target result for EID %u counter %u, but didn't",
               search.eventId, search.counter);
      }
    }
  }

  // sort so that the alias results appear in the right places
  std::sort(ret.begin(), ret.end());

  return ret;
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
  void PreDraw(uint32_t eid, VkCommandBuffer cmd) override
  {
    if(m_OcclusionQueryPool != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_OcclusionQueryPool, (uint32_t)m_Results.size(),
                                  VK_QUERY_CONTROL_PRECISE_BIT);
    if(m_PipeStatsQueryPool != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdBeginQuery(Unwrap(cmd), m_PipeStatsQueryPool, (uint32_t)m_Results.size(), 0);
    ObjDisp(cmd)->CmdWriteTimestamp(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    m_TimeStampQueryPool, (uint32_t)(m_Results.size() * 2 + 0));
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) override
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

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) override {}
  // we don't need to distinguish, call the Draw functions
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) override { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) override { return PostDraw(eid, cmd); }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) override { PostRedraw(eid, cmd); }
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    if(flags & DrawFlags::PassBoundary)
      return;
    PreDraw(eid, cmd);
  }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    if(flags & DrawFlags::PassBoundary)
      return false;
    return PostDraw(eid, cmd);
  }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) override
  {
    if(flags & DrawFlags::PassBoundary)
      return;
    PostRedraw(eid, cmd);
  }
  void AliasEvent(uint32_t primary, uint32_t alias) override
  {
    m_AliasEvents.push_back(make_rdcpair(primary, alias));
  }
  bool SplitSecondary() override { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd) override
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd) override
  {
  }

  void PreEndCommandBuffer(VkCommandBuffer cmd) override {}
  WrappedVulkan *m_pDriver;
  VulkanReplay *m_pReplay;
  VkQueryPool m_TimeStampQueryPool;
  VkQueryPool m_OcclusionQueryPool;
  VkQueryPool m_PipeStatsQueryPool;
  rdcarray<uint32_t> m_Results;
  // events which are the 'same' from being the same command buffer resubmitted
  // multiple times in the frame. We will only get the full callback when we're
  // recording the command buffer, and will be given the first EID. After that
  // we'll just be told which other EIDs alias this event.
  rdcarray<rdcpair<uint32_t, uint32_t> > m_AliasEvents;
};

rdcarray<CounterResult> VulkanReplay::FetchCounters(const rdcarray<GPUCounter> &counters)
{
  uint32_t maxEID = m_pDriver->GetMaxEID();

  rdcarray<GPUCounter> vkCounters;
  std::copy_if(counters.begin(), counters.end(), std::back_inserter(vkCounters),
               [](const GPUCounter &c) { return IsGenericCounter(c); });

  rdcarray<CounterResult> ret;

  if(m_pAMDCounters)
  {
    // Filter out the AMD counters
    rdcarray<GPUCounter> amdCounters;
    std::copy_if(counters.begin(), counters.end(), std::back_inserter(amdCounters),
                 [](const GPUCounter &c) { return IsAMDCounter(c); });

    if(!amdCounters.empty())
    {
      ret = FetchCountersAMD(amdCounters);
    }
  }

  rdcarray<GPUCounter> vkKHRCounters;
  std::copy_if(counters.begin(), counters.end(), std::back_inserter(vkKHRCounters),
               [](const GPUCounter &c) { return IsVulkanExtendedCounter(c); });
  if(!vkKHRCounters.empty())
  {
    ret.append(FetchCountersKHR(vkKHRCounters));
  }

  VkPhysicalDeviceFeatures availableFeatures = m_pDriver->GetDeviceEnabledFeatures();

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

  bool occlNeeded = false;
  bool statsNeeded = false;

  for(size_t c = 0; c < vkCounters.size(); c++)
  {
    switch(vkCounters[c])
    {
      case GPUCounter::InputVerticesRead:
      case GPUCounter::IAPrimitives:
      case GPUCounter::GSPrimitives:
      case GPUCounter::RasterizerInvocations:
      case GPUCounter::RasterizedPrimitives:
      case GPUCounter::VSInvocations:
      case GPUCounter::TCSInvocations:
      case GPUCounter::TESInvocations:
      case GPUCounter::GSInvocations:
      case GPUCounter::PSInvocations:
      case GPUCounter::CSInvocations: statsNeeded = true; break;
      case GPUCounter::SamplesPassed: occlNeeded = true; break;
      default: break;
    }
  }

  VkQueryPool occlusionPool = VK_NULL_HANDLE;
  if(availableFeatures.occlusionQueryPrecise && occlNeeded)
  {
    vkr = ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &occlusionPoolCreateInfo, NULL, &occlusionPool);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  VkQueryPool pipeStatsPool = VK_NULL_HANDLE;
  if(availableFeatures.pipelineStatisticsQuery && statsNeeded)
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

  rdcarray<uint64_t> m_TimeStampData;
  m_TimeStampData.resize(cb.m_Results.size() * 2);

  vkr = ObjDisp(dev)->GetQueryPoolResults(
      Unwrap(dev), timeStampPool, 0, (uint32_t)m_TimeStampData.size(),
      sizeof(uint64_t) * m_TimeStampData.size(), &m_TimeStampData[0], sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), timeStampPool, NULL);

  rdcarray<uint64_t> m_OcclusionData;
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

  rdcarray<uint64_t> m_PipeStatsData;
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

  for(size_t i = 0; i < cb.m_Results.size(); i++)
  {
    for(size_t c = 0; c < vkCounters.size(); c++)
    {
      CounterResult result;

      result.eventId = cb.m_Results[i];
      result.counter = vkCounters[c];

      switch(vkCounters[c])
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
        case GPUCounter::SamplesPassed: result.value.u64 = m_OcclusionData[i]; break;
        case GPUCounter::VSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 2]; break;
        case GPUCounter::TCSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 8]; break;
        case GPUCounter::TESInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 9]; break;
        case GPUCounter::GSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 3]; break;
        case GPUCounter::PSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 7]; break;
        case GPUCounter::CSInvocations: result.value.u64 = m_PipeStatsData[i * 11 + 10]; break;
        default: break;
      }
      ret.push_back(result);
    }
  }

  for(size_t i = 0; i < cb.m_AliasEvents.size(); i++)
  {
    for(size_t c = 0; c < vkCounters.size(); c++)
    {
      CounterResult search;
      search.counter = vkCounters[c];
      search.eventId = cb.m_AliasEvents[i].first;

      // find the result we're aliasing
      int32_t idx = ret.indexOf(search);
      if(idx >= 0)
      {
        // duplicate the result and append
        CounterResult aliased = ret[idx];
        aliased.eventId = cb.m_AliasEvents[i].second;
        ret.push_back(aliased);
      }
      else
      {
        RDCERR("Expected to find alias-target result for EID %u counter %u, but didn't",
               search.eventId, search.counter);
      }
    }
  }

  // sort so that the alias results appear in the right places
  std::sort(ret.begin(), ret.end());

  return ret;
}
