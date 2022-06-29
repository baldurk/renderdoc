/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "nv_d3d11_counters.h"

#include "nv_counter_enumerator.h"

#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_device.h"
#include "driver/d3d11/d3d11_replay.h"

#include "NvPerfD3D11.h"
#include "NvPerfRangeProfilerD3D11.h"
#include "NvPerfScopeExitGuard.h"

struct NVD3D11Counters::Impl
{
  NVCounterEnumerator *CounterEnumerator;
  bool LibraryNotFound = false;

  Impl() : CounterEnumerator(NULL) {}
  ~Impl()
  {
    delete CounterEnumerator;
    CounterEnumerator = NULL;
  }

  bool TryInitializePerfSDK(WrappedID3D11Device *device)
  {
    if(!NVCounterEnumerator::InitializeNvPerf())
    {
      RDCERR("NvPerf library failed to initialize");
      LibraryNotFound = true;

      // NOTE: Return success here so that we can later show a message
      //       directing the user to download the Nsight Perf SDK library.
      return true;
    }

    if(!nv::perf::D3D11LoadDriver())
    {
      RDCERR("NvPerf failed to load D3D11 driver");
      return false;
    }

    if(!nv::perf::profiler::D3D11IsGpuSupported(device->GetReal()))
    {
      RDCERR("NvPerf does not support profiling on this GPU");
      return false;
    }

    nv::perf::DeviceIdentifiers deviceIdentifiers =
        nv::perf::D3D11GetDeviceIdentifiers(device->GetReal());
    if(!deviceIdentifiers.pChipName)
    {
      RDCERR("NvPerf could not determine chip name");
      return false;
    }

    const size_t scratchBufferSize =
        nv::perf::D3D11CalculateMetricsEvaluatorScratchBufferSize(deviceIdentifiers.pChipName);
    if(!scratchBufferSize)
    {
      RDCERR("NvPerf could not determine scratch buffer size for metrics evaluation");
      return false;
    }

    std::vector<uint8_t> scratchBuffer;
    scratchBuffer.resize(scratchBufferSize);
    NVPW_MetricsEvaluator *pMetricsEvaluator = nv::perf::D3D11CreateMetricsEvaluator(
        scratchBuffer.data(), scratchBuffer.size(), deviceIdentifiers.pChipName);
    if(!pMetricsEvaluator)
    {
      RDCERR("NvPerf could not initialize metrics evaluator");
      return false;
    }

    nv::perf::MetricsEvaluator metricsEvaluator(pMetricsEvaluator, std::move(scratchBuffer));

    CounterEnumerator = new NVCounterEnumerator;
    if(!CounterEnumerator->Init(std::move(metricsEvaluator)))
    {
      RDCERR("NvPerf could not initialize metrics evaluator");
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

  static void RecurseProfileEvents(D3D11Replay *replay, WrappedID3D11Device *device,
                                   nv::perf::profiler::RangeProfilerD3D11 &rangeProfiler,
                                   uint32_t &eventStartID, const ActionDescription &actionnode)
  {
    for(size_t i = 0; i < actionnode.children.size(); i++)
    {
      RecurseProfileEvents(replay, device, rangeProfiler, eventStartID, actionnode.children[i]);
    }

    if(!Impl::CanProfileEvent(actionnode))
      return;

    device->ReplayLog(eventStartID, actionnode.eventId, eReplay_WithoutDraw);

    rdcstr eidName = StringFormat::Fmt("%d", actionnode.eventId);
    rangeProfiler.PushRange(eidName.c_str());

    device->ReplayLog(eventStartID, actionnode.eventId, eReplay_OnlyDraw);

    rangeProfiler.PopRange();

    eventStartID = actionnode.eventId + 1;
  }
};

NVD3D11Counters::NVD3D11Counters() : m_Impl(NULL)
{
}

NVD3D11Counters::~NVD3D11Counters()
{
  delete m_Impl;
  m_Impl = NULL;
}

bool NVD3D11Counters::Init(WrappedID3D11Device *device)
{
  m_Impl = new Impl;
  if(!m_Impl)
    return false;

  const bool initSuccess = m_Impl->TryInitializePerfSDK(device);
  if(!initSuccess)
  {
    delete m_Impl;
    m_Impl = NULL;
    return false;
  }

  return true;
}

rdcarray<GPUCounter> NVD3D11Counters::EnumerateCounters() const
{
  if(m_Impl->LibraryNotFound)
  {
    return {GPUCounter::FirstNvidia};
  }
  return m_Impl->CounterEnumerator->GetPublicCounterIds();
}

bool NVD3D11Counters::HasCounter(GPUCounter counterID) const
{
  if(m_Impl->LibraryNotFound)
  {
    return counterID == GPUCounter::FirstNvidia;
  }
  return m_Impl->CounterEnumerator->HasCounter(counterID);
}

CounterDescription NVD3D11Counters::DescribeCounter(GPUCounter counterID) const
{
  if(m_Impl->LibraryNotFound)
  {
    RDCASSERT(counterID == GPUCounter::FirstNvidia);
    // Dummy counter shows message directing user to download the Nsight Perf SDK library
    return NVCounterEnumerator::LibraryNotFoundMessage();
  }
  return m_Impl->CounterEnumerator->GetCounterDescription(counterID);
}

rdcarray<CounterResult> NVD3D11Counters::FetchCounters(const rdcarray<GPUCounter> &counters,
                                                       D3D11Replay *replay,
                                                       WrappedID3D11Device *device,
                                                       WrappedID3D11DeviceContext *immediateContext)
{
  if(m_Impl->LibraryNotFound)
  {
    return {};
  }

  ID3D11Device *d3dDevice = device->GetReal();
  ID3D11DeviceContext *d3dImmediateContext = immediateContext->GetReal();

  uint32_t maxNumRanges;
  {
    uint32_t numEvents = 0u;
    // replay the events to determine how many profile-able events there are
    Impl::RecurseDiscoverEvents(numEvents, immediateContext->GetRootDraw());
    maxNumRanges = numEvents;
  }

  nv::perf::profiler::SessionOptions sessionOptions = {};
  sessionOptions.maxNumRanges = maxNumRanges;
  sessionOptions.avgRangeNameLength = 16;
  sessionOptions.numTraceBuffers = 2;

  nv::perf::profiler::RangeProfilerD3D11 rangeProfiler;

  rdcarray<CounterResult> results;

  if(!rangeProfiler.BeginSession(d3dImmediateContext, sessionOptions))
  {
    RDCERR("NvPerf failed to start profiling session");
    return {};    // Failure
  }
  auto sessionGuard = nv::perf::ScopeExitGuard([&rangeProfiler]() { rangeProfiler.EndSession(); });

  // Create counter configuration, and set it.
  {
    nv::perf::DeviceIdentifiers deviceIdentifiers = nv::perf::D3D11GetDeviceIdentifiers(d3dDevice);
    NVPA_RawMetricsConfig *pRawMetricsConfig =
        nv::perf::profiler::D3D11CreateRawMetricsConfig(deviceIdentifiers.pChipName);
    m_Impl->CounterEnumerator->CreateConfig(deviceIdentifiers.pChipName, pRawMetricsConfig, counters);
  }

  nv::perf::profiler::SetConfigParams setConfigParams;
  setConfigParams.numNestingLevels = 1;
  setConfigParams.numStatisticalSamples = 1;
  m_Impl->CounterEnumerator->GetConfig(setConfigParams.pConfigImage, setConfigParams.configImageSize,
                                       setConfigParams.pCounterDataPrefix,
                                       setConfigParams.counterDataPrefixSize);

  size_t maxNumReplayPasses =
      m_Impl->CounterEnumerator->GetMaxNumReplayPasses(setConfigParams.numNestingLevels);
  RDCASSERT(maxNumReplayPasses > 0u);

  if(!rangeProfiler.EnqueueCounterCollection(setConfigParams))
  {
    RDCERR("NvPerf failed to schedule counter collection");
    return {};    // Failure
  }

  std::vector<uint8_t> counterDataImage;
  size_t replayPass;
  for(replayPass = 0;; ++replayPass)
  {
    if(!rangeProfiler.BeginPass())
    {
      RDCERR("NvPerf failed to start counter collection pass");
      break;    // Failure
    }

    uint32_t eventStartID = 0u;
    Impl::RecurseProfileEvents(replay, device, rangeProfiler, eventStartID,
                               immediateContext->GetRootDraw());

    if(!rangeProfiler.EndPass())
    {
      RDCERR("NvPerf failed to end counter collection pass!");
      break;    // Failure
    }

    nv::perf::profiler::DecodeResult decodeResult;
    if(!rangeProfiler.DecodeCounters(decodeResult))
    {
      RDCERR("NvPerf failed to decode counters in collection pass");
      break;    // Failure
    }

    if(decodeResult.allPassesDecoded)
    {
      counterDataImage = std::move(decodeResult.counterDataImage);
      break;    // Success!
    }

    if(replayPass >= maxNumReplayPasses - 1)
    {
      // FIXME: maxNumReplayPasses does not appear to be calculated correctly for d3d11!
      // RDCERR("NvPerf exceeded the maximum expected number of replay passes");
      // break;    // Failure
    }
  }

  if(counterDataImage.empty())
  {
    RDCERR("No data found in NvPerf counter data image");
    return {};    // Failure
  }

  if(!m_Impl->CounterEnumerator->EvaluateMetrics(counterDataImage.data(), counterDataImage.size(),
                                                 results))
  {
    RDCERR("NvPerf failed to evaluate metrics from counter data");
    return {};
  }

  return results;
}
