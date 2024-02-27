/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "nv_d3d12_counters.h"

#include "nv_counter_enumerator.h"

#include "api/replay/shader_types.h"
#include "driver/d3d12/d3d12_command_list.h"
#include "driver/d3d12/d3d12_command_queue.h"
#include "driver/d3d12/d3d12_commands.h"
#include "driver/d3d12/d3d12_device.h"
#include "driver/d3d12/d3d12_replay.h"

#include "NvPerfD3D12.h"
#include "NvPerfRangeProfilerD3D12.h"
#include "NvPerfScopeExitGuard.h"

struct NVD3D12Counters::Impl
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
    WrappedID3D12Device *device = (WrappedID3D12Device *)pData;
    rdcstr message =
        StringFormat::Fmt("NVIDIA Nsight Perf SDK\n%s%s\n%s", pPrefix, pFunctionName, pMessage);
    device->AddDebugMessage(MessageCategory::Miscellaneous, MessageSeverity::High,
                            MessageSource::RuntimeWarning, message);
  }

  static void LogDebugMessage(const char *pFunctionName, const char *pMessage,
                              WrappedID3D12Device &device)
  {
    rdcstr message = StringFormat::Fmt("NVIDIA Nsight Perf SDK\n%s\n%s", pFunctionName, pMessage);
    device.AddDebugMessage(MessageCategory::Miscellaneous, MessageSeverity::High,
                           MessageSource::RuntimeWarning, message);
  }

  bool TryInitializePerfSDK(WrappedID3D12Device &device)
  {
    if(!NVCounterEnumerator::InitializeNvPerf())
    {
      RDCWARN("NvPerf library failed to initialize");
      LibraryNotFound = true;

      // NOTE: Return success here so that we can later show a message
      //       directing the user to download the Nsight Perf SDK library.
      return true;
    }

    nv::perf::UserLogEnableCustom(NVD3D12Counters::Impl::LogNvPerfAsDebugMessage, (void *)&device);
    auto logGuard = nv::perf::ScopeExitGuard([]() { nv::perf::UserLogDisableCustom(); });

    if(!nv::perf::D3D12LoadDriver())
    {
      Impl::LogDebugMessage("NVD3D12Counters::Impl::TryInitializePerfSDK",
                            "NvPerf failed to load D3D12 driver", device);
      return false;
    }

    if(!nv::perf::profiler::D3D12IsGpuSupported(device.GetReal()))
    {
      Impl::LogDebugMessage("NVD3D12Counters::Impl::TryInitializePerfSDK",
                            "NvPerf does not support profiling on this GPU", device);
      return false;
    }

    nv::perf::DeviceIdentifiers deviceIdentifiers =
        nv::perf::D3D12GetDeviceIdentifiers(device.GetReal());
    if(!deviceIdentifiers.pChipName)
    {
      Impl::LogDebugMessage("NVD3D12Counters::Impl::TryInitializePerfSDK",
                            "NvPerf could not determine chip name", device);
      return false;
    }

    size_t scratchBufferSize =
        nv::perf::D3D12CalculateMetricsEvaluatorScratchBufferSize(deviceIdentifiers.pChipName);
    if(!scratchBufferSize)
    {
      Impl::LogDebugMessage(
          "NVD3D12Counters::Impl::TryInitializePerfSDK",
          "NvPerf could not determine the scratch buffer size for metrics evaluation", device);
      return false;
    }

    std::vector<uint8_t> scratchBuffer;
    scratchBuffer.resize(scratchBufferSize);
    NVPW_MetricsEvaluator *pMetricsEvaluator = nv::perf::D3D12CreateMetricsEvaluator(
        scratchBuffer.data(), scratchBuffer.size(), deviceIdentifiers.pChipName);
    if(!pMetricsEvaluator)
    {
      Impl::LogDebugMessage("NVD3D12Counters::Impl::TryInitializePerfSDK",
                            "NvPerf could not initialize metrics evaluator", device);
      return false;
    }

    nv::perf::MetricsEvaluator metricsEvaluator(pMetricsEvaluator, std::move(scratchBuffer));

    CounterEnumerator = new NVCounterEnumerator;
    if(!CounterEnumerator->Init(std::move(metricsEvaluator)))
    {
      Impl::LogDebugMessage("NVD3D12Counters::Impl::TryInitializePerfSDK",
                            "NvPerf could not initialize metrics evaluator", device);
      delete CounterEnumerator;
      return false;
    }
    return true;
  };

  static bool CanProfileEvent(const ActionDescription &actionnode)
  {
    if(!actionnode.children.empty())
      return false;    // Only profile events for leaf nodes

    if(actionnode.events.empty())
      return false;    // Skip nodes with no events

    if(!(actionnode.flags & (ActionFlags::Clear | ActionFlags::Drawcall | ActionFlags::Dispatch |
                             ActionFlags::Present | ActionFlags::Copy | ActionFlags::Resolve)))
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

NVD3D12Counters::NVD3D12Counters() : m_Impl(NULL)
{
}

NVD3D12Counters::~NVD3D12Counters()
{
  delete m_Impl;
  m_Impl = NULL;
}

bool NVD3D12Counters::Init(WrappedID3D12Device &device)
{
  m_Impl = new Impl;

  if(!m_Impl)
    return false;

  bool initSuccess = m_Impl->TryInitializePerfSDK(device);
  if(!initSuccess)
  {
    delete m_Impl;
    m_Impl = NULL;
    return false;
  }

  return true;
}

rdcarray<GPUCounter> NVD3D12Counters::EnumerateCounters() const
{
  if(m_Impl->LibraryNotFound)
  {
    return {GPUCounter::FirstNvidia};
  }
  return m_Impl->CounterEnumerator->GetPublicCounterIds();
}

bool NVD3D12Counters::HasCounter(GPUCounter counterID) const
{
  if(m_Impl->LibraryNotFound)
  {
    return counterID == GPUCounter::FirstNvidia;
  }
  return m_Impl->CounterEnumerator->HasCounter(counterID);
}

CounterDescription NVD3D12Counters::DescribeCounter(GPUCounter counterID) const
{
  if(m_Impl->LibraryNotFound)
  {
    RDCASSERT(counterID == GPUCounter::FirstNvidia);
    // Dummy counter shows message directing user to download the Nsight Perf SDK library
    return NVCounterEnumerator::LibraryNotFoundMessage();
  }
  return m_Impl->CounterEnumerator->GetCounterDescription(counterID);
}

struct D3D12NvidiaActionCallback final : public D3D12ActionCallback
{
  D3D12NvidiaActionCallback(WrappedID3D12Device *dev,
                            nv::perf::profiler::D3D12RangeCommands *pRangeCommands)
      : m_pDevice(dev), m_pRangeCommands(pRangeCommands)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = this;
  }

  virtual ~D3D12NvidiaActionCallback()
  {
    m_pDevice->GetQueue()->GetCommandData()->m_ActionCallback = NULL;
  }

  void PreDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) final
  {
    rdcstr eidName = StringFormat::Fmt("%d", eid);

    WrappedID3D12GraphicsCommandList *pWrappedCmdList = (WrappedID3D12GraphicsCommandList *)cmd;
    m_pRangeCommands->PushRange(pWrappedCmdList->GetReal(), eidName.c_str());
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) final
  {
    WrappedID3D12GraphicsCommandList *pWrappedCmdList = (WrappedID3D12GraphicsCommandList *)cmd;
    m_pRangeCommands->PopRange(pWrappedCmdList->GetReal());
    return false;
  }

  void PreCloseCommandList(ID3D12GraphicsCommandListX *cmd) final {}
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandListX *cmd) final {}
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) final { PreDraw(eid, cmd); }
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) final
  {
    return PostDraw(eid, cmd);
  }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandListX *cmd) final { PostRedraw(eid, cmd); }
  void PreMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) final
  {
    if(flags & ActionFlags::PassBoundary)
      return;
    PreDraw(eid, cmd);
  }
  bool PostMisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) final
  {
    if(flags & ActionFlags::PassBoundary)
      return false;
    return PostDraw(eid, cmd);
  }
  void PostRemisc(uint32_t eid, ActionFlags flags, ID3D12GraphicsCommandListX *cmd) final
  {
    if(flags & ActionFlags::PassBoundary)
      return;
    PostRedraw(eid, cmd);
  }

  void AliasEvent(uint32_t primary, uint32_t alias) final {}
  WrappedID3D12Device *m_pDevice;
  nv::perf::profiler::D3D12RangeCommands *m_pRangeCommands;
};

rdcarray<CounterResult> NVD3D12Counters::FetchCounters(const rdcarray<GPUCounter> &counters,
                                                       WrappedID3D12Device &device)
{
  if(m_Impl->LibraryNotFound)
  {
    return {};
  }

  nv::perf::UserLogEnableCustom(NVD3D12Counters::Impl::LogNvPerfAsDebugMessage, (void *)&device);
  auto logGuard = nv::perf::ScopeExitGuard([]() { nv::perf::UserLogDisableCustom(); });

  uint32_t maxEID = device.GetQueue()->GetMaxEID();
  ID3D12Device *d3dDevice = device.GetReal();

  nv::perf::profiler::D3D12RangeCommands rangeCommands;
  rangeCommands.Initialize(d3dDevice);
  RDCASSERT(rangeCommands.isNvidiaDevice);
  if(!rangeCommands.isNvidiaDevice)
  {
    return {};
  }

  uint32_t maxNumRanges = 0;
  {
    // replay the events to determine how many profile-able events there are
    FrameRecord frameRecord = device.GetReplay()->GetFrameRecord();
    for(size_t i = 0; i < frameRecord.actionList.size(); i++)
    {
      Impl::RecurseDiscoverEvents(maxNumRanges, frameRecord.actionList[i]);
    }
  }

  nv::perf::profiler::SessionOptions sessionOptions = {};
  sessionOptions.maxNumRanges = maxNumRanges;
  sessionOptions.avgRangeNameLength = 16;
  sessionOptions.numTraceBuffers = 1;

  nv::perf::profiler::RangeProfilerD3D12 rangeProfiler;

  rdcarray<CounterResult> results;
  const rdcarray<WrappedID3D12CommandQueue *> &commandQueues = device.GetQueues();
  for(WrappedID3D12CommandQueue *pWrappedQueue : commandQueues)
  {
    ID3D12CommandQueue *d3dQueue = pWrappedQueue->GetReal();

    switch(d3dQueue->GetDesc().Type)
    {
      case D3D12_COMMAND_LIST_TYPE_DIRECT:
      case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        // Profiling is supported for 3D and compute queues.
        break;
      case D3D12_COMMAND_LIST_TYPE_BUNDLE:
      case D3D12_COMMAND_LIST_TYPE_COPY:
      case D3D12_COMMAND_LIST_TYPE_NONE:
      case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:
      case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS:
      case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:
        // Profiling is not supported for copy or video queues.
        continue;
    }

    if(!rangeProfiler.BeginSession(d3dQueue, sessionOptions))
    {
      Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                            "NvPerf failed to start profiling session", device);
      continue;    // Try the next command queue
    }
    auto sessionGuard = nv::perf::ScopeExitGuard([&rangeProfiler]() { rangeProfiler.EndSession(); });

    // Create counter configuration, and set it.
    {
      nv::perf::DeviceIdentifiers deviceIdentifiers = nv::perf::D3D12GetDeviceIdentifiers(d3dDevice);
      NVPA_RawMetricsConfig *pRawMetricsConfig =
          nv::perf::profiler::D3D12CreateRawMetricsConfig(deviceIdentifiers.pChipName);
      m_Impl->CounterEnumerator->CreateConfig(deviceIdentifiers.pChipName, pRawMetricsConfig,
                                              counters);
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
      Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                            "NvPerf failed to schedule counter collection", device);
      continue;    // Try the next command queue
    }

    D3D12NvidiaActionCallback actionCallback(&device, &rangeCommands);

    std::vector<uint8_t> counterDataImage;
    for(size_t replayPass = 0;; ++replayPass)
    {
      if(!rangeProfiler.BeginPass())
      {
        Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                              "NvPerf failed to start counter collection pass", device);
        break;
      }

      // replay the events to perform all the queries
      uint32_t eventStartID = 0;
      device.ReplayLog(eventStartID, maxEID, eReplay_Full);

      if(!rangeProfiler.EndPass())
      {
        Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                              "NvPerf failed to end counter collection pass!", device);
        break;
      }

      // device->GPUSync(d3dQueue);

      nv::perf::profiler::DecodeResult decodeResult;
      if(!rangeProfiler.DecodeCounters(decodeResult))
      {
        Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                              "NvPerf failed to decode counters in collection pass", device);
        break;
      }

      if(decodeResult.allPassesDecoded)
      {
        counterDataImage = std::move(decodeResult.counterDataImage);
        break;    // Success!
      }

      if(replayPass >= maxNumReplayPasses - 1)
      {
        Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                              "NvPerf exceeded the maximum expected number of replay passes", device);
        break;    // Failure
      }
    }

    if(counterDataImage.empty())
    {
      Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                            "No data found in NvPerf counter data image", device);
      return {};
    }

    if(!m_Impl->CounterEnumerator->EvaluateMetrics(counterDataImage.data(), counterDataImage.size(),
                                                   results))
    {
      Impl::LogDebugMessage("NVD3D12Counters::FetchCounters",
                            "NvPerf failed to evaluate metrics from counter data", device);
      return {};
    }
  }

  return results;
}
