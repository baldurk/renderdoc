/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "common/common.h"
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/intel/intel_counters.h"
#include "driver/ihv/nv/nv_counters.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"

std::vector<GPUCounter> D3D11Replay::EnumerateCounters()
{
  std::vector<GPUCounter> ret;

  ret.push_back(GPUCounter::EventGPUDuration);
  ret.push_back(GPUCounter::InputVerticesRead);
  ret.push_back(GPUCounter::IAPrimitives);
  ret.push_back(GPUCounter::GSPrimitives);
  ret.push_back(GPUCounter::RasterizerInvocations);
  ret.push_back(GPUCounter::RasterizedPrimitives);
  ret.push_back(GPUCounter::SamplesPassed);
  ret.push_back(GPUCounter::VSInvocations);
  ret.push_back(GPUCounter::HSInvocations);
  ret.push_back(GPUCounter::DSInvocations);
  ret.push_back(GPUCounter::GSInvocations);
  ret.push_back(GPUCounter::PSInvocations);
  ret.push_back(GPUCounter::CSInvocations);

  if(m_pAMDCounters)
  {
    std::vector<GPUCounter> amdCounters = m_pAMDCounters->GetPublicCounterIds();
    ret.insert(ret.end(), amdCounters.begin(), amdCounters.end());
  }

  if(m_pNVCounters)
  {
    std::vector<GPUCounter> nvCounters = m_pNVCounters->GetPublicCounterIds();
    ret.insert(ret.end(), nvCounters.begin(), nvCounters.end());
  }

  if(m_pIntelCounters)
  {
    std::vector<GPUCounter> intelCounters = m_pIntelCounters->GetPublicCounterIds();
    ret.insert(ret.end(), intelCounters.begin(), intelCounters.end());
  }

  return ret;
}

CounterDescription D3D11Replay::DescribeCounter(GPUCounter counterID)
{
  CounterDescription desc = {};
  desc.counter = counterID;

  /////AMD//////
  if(IsAMDCounter(counterID))
  {
    if(m_pAMDCounters)
    {
      return m_pAMDCounters->GetCounterDescription(counterID);
    }
  }

  /////NV//////
  if(IsNvidiaCounter(counterID))
  {
    if(m_pNVCounters)
    {
      return m_pNVCounters->GetCounterDescription(counterID);
    }
  }

  // Intel
  if(IsIntelCounter(counterID))
  {
    if(m_pIntelCounters)
    {
      return m_pIntelCounters->GetCounterDescription(counterID);
    }
  }

  // 448A0516-B50E-4312-A6DC-CFE7222FC1AC
  desc.uuid.words[0] = 0x448A0516;
  desc.uuid.words[1] = 0xB50E4312;
  desc.uuid.words[2] = 0xA6DCCFE7;
  desc.uuid.words[3] = 0x222FC1AC ^ (uint32_t)counterID;

  desc.category = "D3D11 Built-in";

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
    case GPUCounter::HSInvocations:
      desc.name = "HS Invocations";
      desc.description = "Number of times a hull shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultType = CompType::UInt;
      desc.unit = CounterUnit::Absolute;
      break;
    case GPUCounter::DSInvocations:
      desc.name = "DS Invocations";
      desc.description =
          "Number of times a domain shader (or tesselation evaluation shader in OpenGL) was "
          "invoked.";
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

struct GPUTimer
{
  ID3D11Query *before;
  ID3D11Query *after;
  ID3D11Query *stats;
  ID3D11Query *occlusion;
  uint32_t eventId;
};

struct D3D11CounterContext
{
  uint32_t eventStart;
  std::vector<GPUTimer> timers;
};

void D3D11Replay::FillTimers(D3D11CounterContext &ctx, const DrawcallDescription &drawnode)
{
  const D3D11_QUERY_DESC qtimedesc = {D3D11_QUERY_TIMESTAMP, 0};
  const D3D11_QUERY_DESC qstatsdesc = {D3D11_QUERY_PIPELINE_STATISTICS, 0};
  const D3D11_QUERY_DESC qoccldesc = {D3D11_QUERY_OCCLUSION, 0};

  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i];
    FillTimers(ctx, drawnode.children[i]);

    if(d.events.empty())
      continue;

    GPUTimer *timer = NULL;

    HRESULT hr = S_OK;

    {
      ctx.timers.push_back(GPUTimer());

      timer = &ctx.timers.back();
      timer->eventId = d.eventId;
      timer->before = timer->after = timer->stats = timer->occlusion = NULL;

      hr = m_pDevice->GetReal()->CreateQuery(&qtimedesc, &timer->before);
      RDCASSERTEQUAL(hr, S_OK);
      hr = m_pDevice->GetReal()->CreateQuery(&qtimedesc, &timer->after);
      RDCASSERTEQUAL(hr, S_OK);
      hr = m_pDevice->GetReal()->CreateQuery(&qstatsdesc, &timer->stats);
      RDCASSERTEQUAL(hr, S_OK);
      hr = m_pDevice->GetReal()->CreateQuery(&qoccldesc, &timer->occlusion);
      RDCASSERTEQUAL(hr, S_OK);
    }

    m_pDevice->ReplayLog(ctx.eventStart, d.eventId, eReplay_WithoutDraw);

    SerializeImmediateContext();

    if(timer->stats)
      m_pImmediateContext->GetReal()->Begin(timer->stats);
    if(timer->occlusion)
      m_pImmediateContext->GetReal()->Begin(timer->occlusion);
    if(timer->before && timer->after)
      m_pImmediateContext->GetReal()->End(timer->before);
    m_pDevice->ReplayLog(ctx.eventStart, d.eventId, eReplay_OnlyDraw);
    if(timer->before && timer->after)
      m_pImmediateContext->GetReal()->End(timer->after);
    if(timer->occlusion)
      m_pImmediateContext->GetReal()->End(timer->occlusion);
    if(timer->stats)
      m_pImmediateContext->GetReal()->End(timer->stats);

    ctx.eventStart = d.eventId + 1;
  }
}

void D3D11Replay::SerializeImmediateContext()
{
  ID3D11Query *query = 0;
  D3D11_QUERY_DESC desc = {D3D11_QUERY_EVENT};

  HRESULT hr = m_pDevice->GetReal()->CreateQuery(&desc, &query);
  if(FAILED(hr))
  {
    return;
  }

  BOOL completed = FALSE;

  m_pImmediateContext->GetReal()->End(query);

  m_pImmediateContext->Flush();

  do
  {
    hr = m_pImmediateContext->GetReal()->GetData(query, &completed, sizeof(BOOL), 0);
    if(hr == S_FALSE)
    {
      ::Sleep(0);
    }
    else if(SUCCEEDED(hr) && completed)
    {
      break;
    }
    else
    {
      // error
      break;
    }
  } while(!completed);

  query->Release();
}

void D3D11Replay::FillTimersAMD(uint32_t &eventStartID, uint32_t &sampleIndex,
                                std::vector<uint32_t> &eventIDs, const DrawcallDescription &drawnode)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i];

    FillTimersAMD(eventStartID, sampleIndex, eventIDs, drawnode.children[i]);

    if(d.events.empty())
      continue;

    eventIDs.push_back(d.eventId);

    m_pDevice->ReplayLog(eventStartID, d.eventId, eReplay_WithoutDraw);

    SerializeImmediateContext();

    m_pAMDCounters->BeginSample(sampleIndex);

    m_pDevice->ReplayLog(eventStartID, d.eventId, eReplay_OnlyDraw);

    m_pAMDCounters->EndSample();

    eventStartID = d.eventId + 1;
    sampleIndex++;
  }
}

void D3D11Replay::FillTimersNV(uint32_t &eventStartID, uint32_t &sampleIndex,
                               std::vector<uint32_t> &eventIDs, const DrawcallDescription &drawnode)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i];

    FillTimersNV(eventStartID, sampleIndex, eventIDs, drawnode.children[i]);

    if(d.events.empty() || (!(drawnode.children[i].flags & DrawFlags::Drawcall) &&
                            !(drawnode.children[i].flags & DrawFlags::Dispatch)))
      continue;

    eventIDs.push_back(d.eventId);

    m_pDevice->ReplayLog(eventStartID, d.eventId, eReplay_WithoutDraw);

    SerializeImmediateContext();

    m_pNVCounters->BeginSample(sampleIndex);

    m_pDevice->ReplayLog(eventStartID, d.eventId, eReplay_OnlyDraw);

    SerializeImmediateContext();

    m_pNVCounters->EndSample(sampleIndex);

    eventStartID = d.eventId + 1;
    sampleIndex++;
  }
}

void D3D11Replay::FillTimersIntel(uint32_t &eventStartID, uint32_t &sampleIndex,
                                  std::vector<uint32_t> &eventIDs,
                                  const DrawcallDescription &drawnode)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i];

    FillTimersIntel(eventStartID, sampleIndex, eventIDs, drawnode.children[i]);

    if(d.events.empty() || (!(drawnode.children[i].flags & DrawFlags::Drawcall) &&
                            !(drawnode.children[i].flags & DrawFlags::Dispatch)))
      continue;

    eventIDs.push_back(d.eventId);

    m_pDevice->ReplayLog(eventStartID, d.eventId, eReplay_WithoutDraw);

    SerializeImmediateContext();

    m_pIntelCounters->BeginSample();

    m_pDevice->ReplayLog(eventStartID, d.eventId, eReplay_OnlyDraw);

    SerializeImmediateContext();

    m_pIntelCounters->EndSample();

    eventStartID = d.eventId + 1;
    sampleIndex++;
  }
}

std::vector<CounterResult> D3D11Replay::FetchCountersAMD(const std::vector<GPUCounter> &counters)
{
  ID3D11Device *d3dDevice = m_pDevice->GetReal();

  if(!m_pAMDCounters->BeginMeasurementMode(AMDCounters::ApiType::Dx11, (void *)d3dDevice))
  {
    return std::vector<CounterResult>();
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

  std::vector<uint32_t> eventIDs;

  for(uint32_t p = 0; p < passCount; p++)
  {
    m_pAMDCounters->BeginPass();
    m_pAMDCounters->BeginCommandList();
    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersAMD(eventStartID, sampleIndex, eventIDs, m_pImmediateContext->GetRootDraw());
    m_pAMDCounters->EndCommandList();
    m_pAMDCounters->EndPass();
  }

  m_pAMDCounters->EndSesssion(sessionID);

  std::vector<CounterResult> ret =
      m_pAMDCounters->GetCounterData(sessionID, sampleIndex, eventIDs, counters);

  m_pAMDCounters->EndMeasurementMode();

  return ret;
}

std::vector<CounterResult> D3D11Replay::FetchCountersNV(const std::vector<GPUCounter> &counters)
{
  const FrameRecord &frameRecord = m_pDevice->GetFrameRecord();
  const FrameStatistics &frameStats = frameRecord.frameInfo.stats;

  const uint32_t objectsCount = frameStats.draws.calls + frameStats.dispatches.calls + 1;

  std::vector<CounterResult> ret;

  if(m_pNVCounters->PrepareExperiment(counters, objectsCount))
  {
    SerializeImmediateContext();

    uint32_t passCount = m_pNVCounters->BeginExperiment();
    uint32_t sampleIndex = 0;
    std::vector<uint32_t> eventIDs;

    for(uint32_t passIdx = 0; passIdx < passCount; ++passIdx)
    {
      m_pNVCounters->BeginPass(passIdx);

      uint32_t eventStartID = 0;
      sampleIndex = 0;
      eventIDs.clear();

      FillTimersNV(eventStartID, sampleIndex, eventIDs, m_pImmediateContext->GetRootDraw());

      m_pNVCounters->EndPass(passIdx);
    }

    m_pNVCounters->EndExperiment(eventIDs, ret);
  }
  return ret;
}

std::vector<CounterResult> D3D11Replay::FetchCountersIntel(const std::vector<GPUCounter> &counters)
{
  m_pIntelCounters->DisableAllCounters();

  // enable counters it needs
  for(size_t i = 0; i < counters.size(); i++)
  {
    // This function is only called internally, and violating this assertion means our
    // caller has invoked this method incorrectly
    RDCASSERT(IsIntelCounter(counters[i]));
    m_pIntelCounters->EnableCounter(counters[i]);
  }

  m_pIntelCounters->BeginSession();

  uint32_t passCount = m_pIntelCounters->GetPassCount();

  uint32_t sampleIndex = 0;

  std::vector<uint32_t> eventIDs;

  for(uint32_t p = 0; p < passCount; p++)
  {
    m_pIntelCounters->BeginPass();
    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersIntel(eventStartID, sampleIndex, eventIDs, m_pImmediateContext->GetRootDraw());
    m_pIntelCounters->EndPass();
  }

  m_pIntelCounters->EndSession();

  return m_pIntelCounters->GetCounterData(eventIDs, counters);
}

std::vector<CounterResult> D3D11Replay::FetchCounters(const std::vector<GPUCounter> &counters)
{
  std::vector<CounterResult> ret;

  if(counters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  SCOPED_TIMER("Fetch Counters, counters to fetch %u", counters.size());

  std::vector<GPUCounter> d3dCounters;
  std::copy_if(counters.begin(), counters.end(), std::back_inserter(d3dCounters),
               [](const GPUCounter &c) { return IsGenericCounter(c); });

  if(m_pAMDCounters)
  {
    // Filter out the AMD counters
    std::vector<GPUCounter> amdCounters;
    std::copy_if(counters.begin(), counters.end(), std::back_inserter(amdCounters),
                 [](const GPUCounter &c) { return IsAMDCounter(c); });

    if(!amdCounters.empty())
    {
      ret = FetchCountersAMD(amdCounters);
    }
  }

  if(m_pNVCounters)
  {
    // Filter out the AMD counters
    std::vector<GPUCounter> nvCounters;
    std::copy_if(counters.begin(), counters.end(), std::back_inserter(nvCounters),
                 [](const GPUCounter &c) { return IsNvidiaCounter(c); });

    if(!nvCounters.empty())
    {
      ret = FetchCountersNV(nvCounters);
    }
  }

  if(m_pIntelCounters)
  {
    std::vector<GPUCounter> intelCounters;
    std::copy_if(counters.begin(), counters.end(), std::back_inserter(intelCounters),
                 [](const GPUCounter &c) { return IsIntelCounter(c); });

    if(!intelCounters.empty())
    {
      ret = FetchCountersIntel(intelCounters);
    }
  }

  if(d3dCounters.empty())
  {
    return ret;
  }

  D3D11_QUERY_DESC disjointdesc = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
  ID3D11Query *disjoint = NULL;

  D3D11_QUERY_DESC qdesc = {D3D11_QUERY_TIMESTAMP, 0};
  ID3D11Query *start = NULL;

  HRESULT hr = S_OK;

  hr = m_pDevice->CreateQuery(&disjointdesc, &disjoint);
  if(FAILED(hr))
  {
    RDCERR("Failed to create disjoint query HRESULT: %s", ToStr(hr).c_str());
    return ret;
  }

  hr = m_pDevice->CreateQuery(&qdesc, &start);
  if(FAILED(hr))
  {
    RDCERR("Failed to create start query HRESULT: %s", ToStr(hr).c_str());
    return ret;
  }

  D3D11CounterContext ctx;

  {
    {
      m_pImmediateContext->Begin(disjoint);

      m_pImmediateContext->End(start);

      ctx.eventStart = 0;
      FillTimers(ctx, m_pImmediateContext->GetRootDraw());

      m_pImmediateContext->End(disjoint);
    }

    {
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
      do
      {
        hr = m_pImmediateContext->GetData(disjoint, &disjointData,
                                          sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
      } while(hr == S_FALSE);
      RDCASSERTEQUAL(hr, S_OK);

      RDCASSERT(!disjointData.Disjoint);

      double ticksToSecs = double(disjointData.Frequency);

      UINT64 a = 0;
      hr = m_pImmediateContext->GetData(start, &a, sizeof(UINT64), 0);
      RDCASSERTEQUAL(hr, S_OK);

      for(size_t i = 0; i < ctx.timers.size(); i++)
      {
        if(ctx.timers[i].before && ctx.timers[i].after && ctx.timers[i].stats &&
           ctx.timers[i].occlusion)
        {
          hr = m_pImmediateContext->GetReal()->GetData(ctx.timers[i].before, &a, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          UINT64 b = 0;
          hr = m_pImmediateContext->GetReal()->GetData(ctx.timers[i].after, &b, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          double duration = (double(b - a) / ticksToSecs);

          if(b == 0)
            duration = 0.0;

          a = b;

          D3D11_QUERY_DATA_PIPELINE_STATISTICS pipelineStats;
          hr = m_pImmediateContext->GetReal()->GetData(
              ctx.timers[i].stats, &pipelineStats, sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS), 0);
          RDCASSERTEQUAL(hr, S_OK);

          UINT64 occlusion = 0;
          hr = m_pImmediateContext->GetReal()->GetData(ctx.timers[i].occlusion, &occlusion,
                                                       sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          for(size_t c = 0; c < d3dCounters.size(); c++)
          {
            switch(d3dCounters[c])
            {
              case GPUCounter::EventGPUDuration:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventId, GPUCounter::EventGPUDuration, duration));
                break;
              case GPUCounter::InputVerticesRead:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::InputVerticesRead,
                                            pipelineStats.IAVertices));
                break;
              case GPUCounter::IAPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::IAPrimitives,
                                            pipelineStats.IAPrimitives));
                break;
              case GPUCounter::VSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::VSInvocations,
                                            pipelineStats.VSInvocations));
                break;
              case GPUCounter::GSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::GSInvocations,
                                            pipelineStats.GSInvocations));
                break;
              case GPUCounter::GSPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::GSPrimitives,
                                            pipelineStats.GSPrimitives));
                break;
              case GPUCounter::RasterizerInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::RasterizerInvocations,
                                            pipelineStats.CInvocations));
                break;
              case GPUCounter::RasterizedPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::RasterizedPrimitives,
                                            pipelineStats.CPrimitives));
                break;
              case GPUCounter::PSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::PSInvocations,
                                            pipelineStats.PSInvocations));
                break;
              case GPUCounter::HSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::HSInvocations,
                                            pipelineStats.HSInvocations));
                break;
              case GPUCounter::DSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::DSInvocations,
                                            pipelineStats.DSInvocations));
                break;
              case GPUCounter::CSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventId, GPUCounter::CSInvocations,
                                            pipelineStats.CSInvocations));
                break;
              case GPUCounter::SamplesPassed:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventId, GPUCounter::SamplesPassed, occlusion));
                break;
            }
          }
        }
        else
        {
          for(size_t c = 0; c < d3dCounters.size(); c++)
          {
            switch(d3dCounters[c])
            {
              case GPUCounter::EventGPUDuration:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventId, GPUCounter::EventGPUDuration, -1.0));
                break;
              case GPUCounter::InputVerticesRead:
              case GPUCounter::IAPrimitives:
              case GPUCounter::GSPrimitives:
              case GPUCounter::RasterizerInvocations:
              case GPUCounter::RasterizedPrimitives:
              case GPUCounter::VSInvocations:
              case GPUCounter::HSInvocations:
              case GPUCounter::DSInvocations:
              case GPUCounter::GSInvocations:
              case GPUCounter::PSInvocations:
              case GPUCounter::CSInvocations:
              case GPUCounter::SamplesPassed:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventId, d3dCounters[c], 0xFFFFFFFFFFFFFFFF));
                break;
            }
          }
        }
      }
    }
  }

  for(size_t i = 0; i < ctx.timers.size(); i++)
  {
    SAFE_RELEASE(ctx.timers[i].before);
    SAFE_RELEASE(ctx.timers[i].after);
    SAFE_RELEASE(ctx.timers[i].stats);
    SAFE_RELEASE(ctx.timers[i].occlusion);
  }

  SAFE_RELEASE(disjoint);
  SAFE_RELEASE(start);

  return ret;
}
