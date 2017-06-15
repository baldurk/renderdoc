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

#include <algorithm>
#include <iterator>
#include "common/common.h"
#include "driver/ihv/amd/amd_counters.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"

void D3D11DebugManager::PreDeviceInitCounters()
{
}

void D3D11DebugManager::PostDeviceInitCounters()
{
}

void D3D11DebugManager::PreDeviceShutdownCounters()
{
}

void D3D11DebugManager::PostDeviceShutdownCounters()
{
}

vector<GPUCounter> D3D11DebugManager::EnumerateCounters()
{
  vector<GPUCounter> ret;

  ret.push_back(GPUCounter::EventGPUDuration);
  ret.push_back(GPUCounter::InputVerticesRead);
  ret.push_back(GPUCounter::IAPrimitives);
  ret.push_back(GPUCounter::GSPrimitives);
  ret.push_back(GPUCounter::RasterizerInvocations);
  ret.push_back(GPUCounter::RasterizedPrimitives);
  ret.push_back(GPUCounter::SamplesWritten);
  ret.push_back(GPUCounter::VSInvocations);
  ret.push_back(GPUCounter::HSInvocations);
  ret.push_back(GPUCounter::DSInvocations);
  ret.push_back(GPUCounter::GSInvocations);
  ret.push_back(GPUCounter::PSInvocations);
  ret.push_back(GPUCounter::CSInvocations);

  if(m_pAMDCounters)
  {
    for(uint32_t i = 0; i < m_pAMDCounters->GetNumCounters(); i++)
    {
      ret.push_back(MakeAMDCounter(i));
    }
  }

  return ret;
}

void D3D11DebugManager::DescribeCounter(GPUCounter counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

  /////AMD//////
  if(counterID >= GPUCounter::FirstAMD && counterID < GPUCounter::FirstIntel)
  {
    if(m_pAMDCounters)
    {
      desc = m_pAMDCounters->GetCounterDescription(counterID);

      return;
    }
  }

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
}

struct GPUTimer
{
  ID3D11Query *before;
  ID3D11Query *after;
  ID3D11Query *stats;
  ID3D11Query *occlusion;
  uint32_t eventID;
};

struct D3D11CounterContext
{
  uint32_t eventStart;
  vector<GPUTimer> timers;
  int reuseIdx;
};

void D3D11DebugManager::FillTimers(D3D11CounterContext &ctx, const DrawcallTreeNode &drawnode)
{
  const D3D11_QUERY_DESC qtimedesc = {D3D11_QUERY_TIMESTAMP, 0};
  const D3D11_QUERY_DESC qstatsdesc = {D3D11_QUERY_PIPELINE_STATISTICS, 0};
  const D3D11_QUERY_DESC qoccldesc = {D3D11_QUERY_OCCLUSION, 0};

  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i].draw;
    FillTimers(ctx, drawnode.children[i]);

    if(d.events.count == 0)
      continue;

    GPUTimer *timer = NULL;

    HRESULT hr = S_OK;

    {
      if(ctx.reuseIdx == -1)
      {
        ctx.timers.push_back(GPUTimer());

        timer = &ctx.timers.back();
        timer->eventID = d.eventID;
        timer->before = timer->after = timer->stats = timer->occlusion = NULL;

        hr = m_pDevice->CreateQuery(&qtimedesc, &timer->before);
        RDCASSERTEQUAL(hr, S_OK);
        hr = m_pDevice->CreateQuery(&qtimedesc, &timer->after);
        RDCASSERTEQUAL(hr, S_OK);
        hr = m_pDevice->CreateQuery(&qstatsdesc, &timer->stats);
        RDCASSERTEQUAL(hr, S_OK);
        hr = m_pDevice->CreateQuery(&qoccldesc, &timer->occlusion);
        RDCASSERTEQUAL(hr, S_OK);
      }
      else
      {
        timer = &ctx.timers[ctx.reuseIdx++];
      }
    }

    m_WrappedDevice->ReplayLog(ctx.eventStart, d.eventID, eReplay_WithoutDraw);

    m_pImmediateContext->Flush();

    if(timer->stats)
      m_pImmediateContext->Begin(timer->stats);
    if(timer->occlusion)
      m_pImmediateContext->Begin(timer->occlusion);
    if(timer->before && timer->after)
      m_pImmediateContext->End(timer->before);
    m_WrappedDevice->ReplayLog(ctx.eventStart, d.eventID, eReplay_OnlyDraw);
    if(timer->before && timer->after)
      m_pImmediateContext->End(timer->after);
    if(timer->occlusion)
      m_pImmediateContext->End(timer->occlusion);
    if(timer->stats)
      m_pImmediateContext->End(timer->stats);

    ctx.eventStart = d.eventID + 1;
  }
}

void D3D11DebugManager::FillTimersAMD(uint32_t &eventStartID, uint32_t &sampleIndex,
                                      vector<uint32_t> &eventIDs, const DrawcallTreeNode &drawnode)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i].draw;

    FillTimersAMD(eventStartID, sampleIndex, eventIDs, drawnode.children[i]);

    if(d.events.count == 0)
      continue;

    eventIDs.push_back(d.eventID);

    m_WrappedDevice->ReplayLog(eventStartID, d.eventID, eReplay_WithoutDraw);

    m_pImmediateContext->Flush();

    m_pAMDCounters->BeginSample(sampleIndex);

    m_WrappedDevice->ReplayLog(eventStartID, d.eventID, eReplay_OnlyDraw);

    m_pAMDCounters->EndSample();

    eventStartID = d.eventID + 1;
    sampleIndex++;
  }
}

vector<CounterResult> D3D11DebugManager::FetchCountersAMD(const vector<GPUCounter> &counters)
{
  vector<CounterResult> ret;

  m_pAMDCounters->DisableAllCounters();

  // enable counters it needs
  for(size_t i = 0; i < counters.size(); i++)
  {
    // This function is only called internally, and violating this assertion means our
    // caller has invoked this method incorrectly
    RDCASSERT((counters[i] >= (GPUCounter::FirstAMD)) && (counters[i] < (GPUCounter::FirstIntel)));
    m_pAMDCounters->EnableCounter(counters[i]);
  }

  uint32_t sessionID = m_pAMDCounters->BeginSession();

  uint32_t passCount = m_pAMDCounters->GetPassCount();

  uint32_t sampleIndex = 0;

  vector<uint32_t> eventIDs;

  for(uint32_t p = 0; p < passCount; p++)
  {
    m_pAMDCounters->BeginPass();

    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersAMD(eventStartID, sampleIndex, eventIDs, m_WrappedContext->GetRootDraw());

    m_pAMDCounters->EndPass();
  }

  m_pAMDCounters->EndSesssion();

  bool isReady = false;

  do
  {
    isReady = m_pAMDCounters->IsSessionReady(sessionID);
  } while(!isReady);

  for(uint32_t s = 0; s < sampleIndex; s++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      const CounterDescription desc = m_pAMDCounters->GetCounterDescription(counters[c]);

      switch(desc.resultType)
      {
        case CompType::UInt:
        {
          if(desc.resultByteWidth == sizeof(uint32_t))
          {
            uint32_t value = m_pAMDCounters->GetSampleUint32(sessionID, s, counters[c]);

            if(desc.unit == CounterUnit::Percentage)
            {
              value = RDCCLAMP(value, 0U, 100U);
            }

            ret.push_back(CounterResult(eventIDs[s], counters[c], value));
          }
          else if(desc.resultByteWidth == sizeof(uint64_t))
          {
            uint64_t value = m_pAMDCounters->GetSampleUint64(sessionID, s, counters[c]);

            if(desc.unit == CounterUnit::Percentage)
            {
              value = RDCCLAMP(value, 0ULL, 100ULL);
            }

            ret.push_back(

                CounterResult(eventIDs[s], counters[c], value));
          }
          else
          {
            RDCERR("Unexpected byte width %u", desc.resultByteWidth);
          }
        }
        break;
        case CompType::Float:
        {
          float value = m_pAMDCounters->GetSampleFloat32(sessionID, s, counters[c]);

          if(desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, 0.0f, 100.0f);
          }

          ret.push_back(CounterResult(eventIDs[s], counters[c], value));
        }
        break;
        case CompType::Double:
        {
          double value = m_pAMDCounters->GetSampleFloat64(sessionID, s, counters[c]);

          if(desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, 0.0, 100.0);
          }

          ret.push_back(CounterResult(eventIDs[s], counters[c], value));
        }
        break;
        default: RDCASSERT(0); break;
      };
    }
  }

  return ret;
}

vector<CounterResult> D3D11DebugManager::FetchCounters(const vector<GPUCounter> &counters)
{
  vector<CounterResult> ret;

  if(counters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  SCOPED_TIMER("Fetch Counters, counters to fetch %u", counters.size());

  vector<GPUCounter> d3dCounters;
  std::copy_if(counters.begin(), counters.end(), std::back_inserter(d3dCounters),
               [](const GPUCounter &c) { return c <= GPUCounter::FirstAMD; });

  if(m_pAMDCounters)
  {
    // Filter out the AMD counters
    vector<GPUCounter> amdCounters;
    std::copy_if(
        counters.begin(), counters.end(), std::back_inserter(amdCounters),
        [](const GPUCounter &c) { return c >= GPUCounter::FirstAMD && c < GPUCounter::FirstIntel; });

    ret = FetchCountersAMD(amdCounters);
  }

  D3D11_QUERY_DESC disjointdesc = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
  ID3D11Query *disjoint = NULL;

  D3D11_QUERY_DESC qdesc = {D3D11_QUERY_TIMESTAMP, 0};
  ID3D11Query *start = NULL;

  HRESULT hr = S_OK;

  hr = m_pDevice->CreateQuery(&disjointdesc, &disjoint);
  if(FAILED(hr))
  {
    RDCERR("Failed to create disjoint query %08x", hr);
    return ret;
  }

  hr = m_pDevice->CreateQuery(&qdesc, &start);
  if(FAILED(hr))
  {
    RDCERR("Failed to create start query %08x", hr);
    return ret;
  }

  D3D11CounterContext ctx;

  for(int loop = 0; loop < 1; loop++)
  {
    {
      m_pImmediateContext->Begin(disjoint);

      m_pImmediateContext->End(start);

      ctx.eventStart = 0;
      ctx.reuseIdx = loop == 0 ? -1 : 0;
      FillTimers(ctx, m_WrappedContext->GetRootDraw());

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
          hr = m_pImmediateContext->GetData(ctx.timers[i].before, &a, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          UINT64 b = 0;
          hr = m_pImmediateContext->GetData(ctx.timers[i].after, &b, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          double duration = (double(b - a) / ticksToSecs);

          a = b;

          D3D11_QUERY_DATA_PIPELINE_STATISTICS pipelineStats;
          hr = m_pImmediateContext->GetData(ctx.timers[i].stats, &pipelineStats,
                                            sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS), 0);
          RDCASSERTEQUAL(hr, S_OK);

          UINT64 occlusion = 0;
          hr = m_pImmediateContext->GetData(ctx.timers[i].occlusion, &occlusion, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          for(size_t c = 0; c < d3dCounters.size(); c++)
          {
            switch(d3dCounters[c])
            {
              case GPUCounter::EventGPUDuration:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventID, GPUCounter::EventGPUDuration, duration));
                break;
              case GPUCounter::InputVerticesRead:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::InputVerticesRead,
                                            pipelineStats.IAVertices));
                break;
              case GPUCounter::IAPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::IAPrimitives,
                                            pipelineStats.IAPrimitives));
                break;
              case GPUCounter::VSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::VSInvocations,
                                            pipelineStats.VSInvocations));
                break;
              case GPUCounter::GSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::GSInvocations,
                                            pipelineStats.GSInvocations));
                break;
              case GPUCounter::GSPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::GSPrimitives,
                                            pipelineStats.GSPrimitives));
                break;
              case GPUCounter::RasterizerInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::RasterizerInvocations,
                                            pipelineStats.CInvocations));
                break;
              case GPUCounter::RasterizedPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::RasterizedPrimitives,
                                            pipelineStats.CPrimitives));
                break;
              case GPUCounter::PSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::PSInvocations,
                                            pipelineStats.PSInvocations));
                break;
              case GPUCounter::HSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::HSInvocations,
                                            pipelineStats.HSInvocations));
                break;
              case GPUCounter::DSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::DSInvocations,
                                            pipelineStats.DSInvocations));
                break;
              case GPUCounter::CSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, GPUCounter::CSInvocations,
                                            pipelineStats.CSInvocations));
                break;
              case GPUCounter::SamplesWritten:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventID, GPUCounter::SamplesWritten, occlusion));
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
                    CounterResult(ctx.timers[i].eventID, GPUCounter::EventGPUDuration, -1.0));
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
              case GPUCounter::SamplesWritten:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventID, d3dCounters[c], 0xFFFFFFFFFFFFFFFF));
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
