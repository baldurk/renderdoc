/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 Baldur Karlsson
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

#include "nv_vk_counters.h"

#include "nv_counter_enumerator.h"

#include "driver/vulkan/vk_core.h"
#include "driver/vulkan/vk_replay.h"

#define NV_PERF_UTILITY_HIDE_VULKAN_SYMBOLS
#include "NvPerfRangeProfilerVulkan.h"
#include "NvPerfScopeExitGuard.h"
#include "NvPerfVulkan.h"

struct NVVulkanCounters::Impl
{
  NVCounterEnumerator *CounterEnumerator;
  bool LibraryNotFound = false;

  Impl() : CounterEnumerator(NULL) {}
  ~Impl()
  {
    delete CounterEnumerator;
    CounterEnumerator = NULL;
  }

  static void LogNvPerfAsDebugMessage(const char *pPrefix, const char *pDate, const char *pTime,
                                      const char *pFunctionName, const char *pMessage, void *pData)
  {
    WrappedVulkan *driver = (WrappedVulkan *)pData;
    rdcstr message =
        StringFormat::Fmt("NVIDIA Nsight Perf SDK\n%s%s\n%s", pPrefix, pFunctionName, pMessage);
    driver->AddDebugMessage(MessageCategory::Miscellaneous, MessageSeverity::High,
                            MessageSource::RuntimeWarning, message);
  }

  static void LogDebugMessage(const char *pFunctionName, const char *pMessage, WrappedVulkan *driver)
  {
    rdcstr message = StringFormat::Fmt("NVIDIA Nsight Perf SDK\n%s\n%s", pFunctionName, pMessage);
    driver->AddDebugMessage(MessageCategory::Miscellaneous, MessageSeverity::High,
                            MessageSource::RuntimeWarning, message);
  }

  bool TryInitializePerfSDK(WrappedVulkan *driver)
  {
    if(!NVCounterEnumerator::InitializeNvPerf())
    {
      RDCWARN("NvPerf library failed to initialize");
      LibraryNotFound = true;

      // NOTE: Return success here so that we can later show a message
      //       directing the user to download the Nsight Perf SDK library.
      return true;
    }

    nv::perf::UserLogEnableCustom(NVVulkanCounters::Impl::LogNvPerfAsDebugMessage, (void *)driver);
    auto logGuard = nv::perf::ScopeExitGuard([]() { nv::perf::UserLogDisableCustom(); });

    if(!nv::perf::VulkanLoadDriver(Unwrap(driver->GetInstance())))
    {
      Impl::LogDebugMessage("NVVulkanCounters::Impl::TryInitializePerfSDK",
                            "NvPerf failed to load Vulkan driver", driver);
      return false;
    }

    if(!nv::perf::profiler::VulkanIsGpuSupported(
           Unwrap(driver->GetInstance()), Unwrap(driver->GetPhysDev()), Unwrap(driver->GetDev()),
           ObjDisp(driver->GetInstance())->GetInstanceProcAddr,
           ObjDisp(driver->GetDev())->GetDeviceProcAddr))
    {
      Impl::LogDebugMessage("NVVulkanCounters::Impl::TryInitializePerfSDK",
                            "NvPerf does not support profiling on this GPU", driver);
      return false;
    }

    nv::perf::DeviceIdentifiers deviceIdentifiers = nv::perf::VulkanGetDeviceIdentifiers(
        Unwrap(driver->GetInstance()), Unwrap(driver->GetPhysDev()), Unwrap(driver->GetDev()),
        ObjDisp(driver->GetInstance())->GetInstanceProcAddr,
        ObjDisp(driver->GetDev())->GetDeviceProcAddr);
    if(!deviceIdentifiers.pChipName)
    {
      Impl::LogDebugMessage("NVVulkanCounters::Impl::TryInitializePerfSDK",
                            "NvPerf could not determine chip name", driver);
      return false;
    }

    const size_t scratchBufferSize =
        nv::perf::VulkanCalculateMetricsEvaluatorScratchBufferSize(deviceIdentifiers.pChipName);
    if(!scratchBufferSize)
    {
      Impl::LogDebugMessage("NVVulkanCounters::Impl::TryInitializePerfSDK",
                            "NvPerf could not determine scratch buffer size for metrics evaluation",
                            driver);
      return false;
    }

    std::vector<uint8_t> scratchBuffer;
    scratchBuffer.resize(scratchBufferSize);
    NVPW_MetricsEvaluator *pMetricsEvaluator = nv::perf::VulkanCreateMetricsEvaluator(
        scratchBuffer.data(), scratchBuffer.size(), deviceIdentifiers.pChipName);
    if(!pMetricsEvaluator)
    {
      Impl::LogDebugMessage("NVVulkanCounters::Impl::TryInitializePerfSDK",
                            "NvPerf could not initialize metrics evaluator", driver);
      return false;
    }

    nv::perf::MetricsEvaluator metricsEvaluator(pMetricsEvaluator, std::move(scratchBuffer));

    CounterEnumerator = new NVCounterEnumerator;
    if(!CounterEnumerator->Init(std::move(metricsEvaluator)))
    {
      Impl::LogDebugMessage("NVVulkanCounters::Impl::TryInitializePerfSDK",
                            "NvPerf could not initialize metrics evaluator", driver);
      delete CounterEnumerator;
      return false;
    }
    return true;
  }

  static bool CanProfileEvent(const ActionDescription &actionnode)
  {
    if(!actionnode.children.empty())
      return false;    // Only profile events for leaf nodes

    if(actionnode.events.empty())
      return false;    // Skip nodes with no events

    if(!(actionnode.flags &
         (ActionFlags::Clear | ActionFlags::MeshDispatch | ActionFlags::Drawcall |
          ActionFlags::Dispatch | ActionFlags::Present | ActionFlags::Copy | ActionFlags::Resolve)))
      return false;    // Filter out events we cannot profile

    return true;
  }

  static void RecurseDiscoverEvents(uint32_t &numEvents, const ActionDescription &actionnode)
  {
    for(size_t i = 0; i < actionnode.children.size(); i++)
    {
      RecurseDiscoverEvents(numEvents, actionnode.children[i]);
    }

    if(!Impl::CanProfileEvent(actionnode))
      return;

    numEvents++;
  }
};

NVVulkanCounters::NVVulkanCounters() : m_Impl(NULL)
{
}

NVVulkanCounters::~NVVulkanCounters()
{
  delete m_Impl;
  m_Impl = NULL;
}

bool NVVulkanCounters::Init(WrappedVulkan *driver)
{
  m_Impl = new Impl;
  if(!m_Impl)
    return false;

  const bool initSuccess = m_Impl->TryInitializePerfSDK(driver);
  if(!initSuccess)
  {
    delete m_Impl;
    m_Impl = NULL;
    return false;
  }

  return true;
}

rdcarray<GPUCounter> NVVulkanCounters::EnumerateCounters() const
{
  if(m_Impl->LibraryNotFound)
  {
    return {GPUCounter::FirstNvidia};
  }
  return m_Impl->CounterEnumerator->GetPublicCounterIds();
}

bool NVVulkanCounters::HasCounter(GPUCounter counterID) const
{
  if(m_Impl->LibraryNotFound)
  {
    return counterID == GPUCounter::FirstNvidia;
  }
  return m_Impl->CounterEnumerator->HasCounter(counterID);
}

CounterDescription NVVulkanCounters::DescribeCounter(GPUCounter counterID) const
{
  if(m_Impl->LibraryNotFound)
  {
    RDCASSERT(counterID == GPUCounter::FirstNvidia);
    // Dummy counter shows message directing user to download the Nsight Perf SDK library
    return NVCounterEnumerator::LibraryNotFoundMessage();
  }
  return m_Impl->CounterEnumerator->GetCounterDescription(counterID);
}

struct VulkanNvidiaActionCallback final : public VulkanActionCallback
{
  VulkanNvidiaActionCallback(WrappedVulkan *driver) : m_driver(driver)
  {
    m_driver->SetActionCB(this);
  }
  ~VulkanNvidiaActionCallback() { m_driver->SetActionCB(NULL); }
  void PreDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final
  {
    rdcstr eidName = StringFormat::Fmt("%d", eid);
    nv::perf::profiler::VulkanPushRange(Unwrap(cmd), eidName.c_str());
  }

  bool PostDraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final
  {
    nv::perf::profiler::VulkanPopRange(Unwrap(cmd));
    return false;
  }

  void PostRedraw(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final {}
  void PreDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final
  {
    PreDraw(eid, flags, cmd);
  }
  bool PostDispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final
  {
    return PostDraw(eid, flags, cmd);
  }
  void PostRedispatch(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final {}
  void PreMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final
  {
    if(flags & ActionFlags::PassBoundary)
      return;
    PreDraw(eid, flags, cmd);
  }

  bool PostMisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final
  {
    if(flags & ActionFlags::PassBoundary)
      return false;
    return PostDraw(eid, flags, cmd);
  }

  void PostRemisc(uint32_t eid, ActionFlags flags, VkCommandBuffer cmd) final {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) final {}
  void AliasEvent(uint32_t primary, uint32_t alias) final {}
  bool SplitSecondary() final { return false; }
  bool ForceLoadRPs() final { return false; }
  void PreCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                     VkCommandBuffer cmd) final
  {
  }
  void PostCmdExecute(uint32_t baseEid, uint32_t secondaryFirst, uint32_t secondaryLast,
                      VkCommandBuffer cmd) final
  {
  }
  WrappedVulkan *m_driver;
};

rdcarray<CounterResult> NVVulkanCounters::FetchCounters(const rdcarray<GPUCounter> &counters,
                                                        WrappedVulkan *driver)
{
  if(m_Impl->LibraryNotFound)
  {
    return {};
  }

  nv::perf::UserLogEnableCustom(NVVulkanCounters::Impl::LogNvPerfAsDebugMessage, (void *)driver);
  auto logGuard = nv::perf::ScopeExitGuard([]() { nv::perf::UserLogDisableCustom(); });

  uint32_t maxEID = driver->GetMaxEID();

  uint32_t maxNumRanges = 0;
  {
    // replay the events to determine how many profile-able events there are
    FrameRecord frameRecord = driver->GetReplay()->GetFrameRecord();
    for(size_t i = 0; i < frameRecord.actionList.size(); i++)
    {
      Impl::RecurseDiscoverEvents(maxNumRanges, frameRecord.actionList[i]);
    }
  }

  nv::perf::profiler::SessionOptions sessionOptions = {};
  sessionOptions.maxNumRanges = maxNumRanges;
  sessionOptions.avgRangeNameLength = 16;
  sessionOptions.numTraceBuffers = 1;

  nv::perf::profiler::RangeProfilerVulkan rangeProfiler;

  rdcarray<CounterResult> results;
  // TODO: For each Vulkan queue
  {
    if(!rangeProfiler.BeginSession(Unwrap(driver->GetInstance()), Unwrap(driver->GetPhysDev()),
                                   Unwrap(driver->GetDev()), Unwrap(driver->GetQ()),
                                   driver->GetQueueFamilyIndex(), sessionOptions,
                                   ObjDisp(driver->GetInstance())->GetInstanceProcAddr,
                                   ObjDisp(driver->GetDev())->GetDeviceProcAddr))
    {
      Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                            "NvPerf failed to start profiling session", driver);
      return {};    // Failure
    }
    auto sessionGuard = nv::perf::ScopeExitGuard([&rangeProfiler]() { rangeProfiler.EndSession(); });

    // Create counter configuration, and set it.
    {
      nv::perf::DeviceIdentifiers deviceIdentifiers = nv::perf::VulkanGetDeviceIdentifiers(
          Unwrap(driver->GetInstance()), Unwrap(driver->GetPhysDev()), Unwrap(driver->GetDev()),
          ObjDisp(driver->GetInstance())->GetInstanceProcAddr,
          ObjDisp(driver->GetDev())->GetDeviceProcAddr);
      NVPA_RawMetricsConfig *pRawMetricsConfig =
          nv::perf::profiler::VulkanCreateRawMetricsConfig(deviceIdentifiers.pChipName);
      if(!m_Impl->CounterEnumerator->CreateConfig(deviceIdentifiers.pChipName, pRawMetricsConfig,
                                                  counters))
        return {};    // Failure
    }

    nv::perf::profiler::SetConfigParams setConfigParams;
    setConfigParams.numNestingLevels = 1;
    setConfigParams.numStatisticalSamples = 1;
    m_Impl->CounterEnumerator->GetConfig(
        setConfigParams.pConfigImage, setConfigParams.configImageSize,
        setConfigParams.pCounterDataPrefix, setConfigParams.counterDataPrefixSize);

    size_t maxNumReplayPasses =
        m_Impl->CounterEnumerator->GetMaxNumReplayPasses(setConfigParams.numNestingLevels);
    RDCASSERT(maxNumReplayPasses > 0u);

    if(!rangeProfiler.EnqueueCounterCollection(setConfigParams))
    {
      Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                            "NvPerf failed to schedule counter collection", driver);
      return {};    // Failure
    }

    VulkanNvidiaActionCallback actionCallback(driver);

    std::vector<uint8_t> counterDataImage;
    for(size_t replayPass = 0;; ++replayPass)
    {
      if(!rangeProfiler.BeginPass())
      {
        Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                              "NvPerf failed to start counter collection pass", driver);
        break;
      }

      // replay the events to perform all the queries
      uint32_t eventStartID = 0;
      driver->ReplayLog(eventStartID, maxEID, eReplay_Full);

      if(!rangeProfiler.EndPass())
      {
        Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                              "NvPerf failed to end counter collection pass!", driver);
        break;
      }

      ObjDisp(driver->GetQ())->QueueWaitIdle(Unwrap(driver->GetQ()));

      nv::perf::profiler::DecodeResult decodeResult;
      if(!rangeProfiler.DecodeCounters(decodeResult))
      {
        Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                              "NvPerf failed to decode counters in collection pass", driver);
        break;
      }

      if(decodeResult.allPassesDecoded)
      {
        counterDataImage = std::move(decodeResult.counterDataImage);
        break;    // success!
      }

      if(replayPass >= maxNumReplayPasses - 1)
      {
        Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                              "NvPerf exceeded the maximum expected number of replay passes", driver);
        break;    // Failure
      }
    }

    if(counterDataImage.empty())
    {
      Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                            "No data found in NvPerf counter data image", driver);
      return {};
    }

    if(!m_Impl->CounterEnumerator->EvaluateMetrics(counterDataImage.data(), counterDataImage.size(),
                                                   results))
    {
      Impl::LogDebugMessage("NVVulkanCounters::FetchCounters",
                            "NvPerf failed to evaluate metrics from counter data", driver);
      return {};
    }
  }

  return results;
}
