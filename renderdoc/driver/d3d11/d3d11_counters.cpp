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

#include "common/common.h"
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

vector<uint32_t> D3D11DebugManager::EnumerateCounters()
{
  vector<uint32_t> ret;

  ret.push_back(eCounter_EventGPUDuration);
  ret.push_back(eCounter_InputVerticesRead);
  ret.push_back(eCounter_IAPrimitives);
  ret.push_back(eCounter_GSPrimitives);
  ret.push_back(eCounter_RasterizerInvocations);
  ret.push_back(eCounter_RasterizedPrimitives);
  ret.push_back(eCounter_SamplesWritten);
  ret.push_back(eCounter_VSInvocations);
  ret.push_back(eCounter_HSInvocations);
  ret.push_back(eCounter_DSInvocations);
  ret.push_back(eCounter_GSInvocations);
  ret.push_back(eCounter_PSInvocations);
  ret.push_back(eCounter_CSInvocations);

  return ret;
}

void D3D11DebugManager::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

  switch(counterID)
  {
    case eCounter_EventGPUDuration:
      desc.name = "GPU Duration";
      desc.description =
          "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_Double;
      desc.units = eUnits_Seconds;
      break;
    case eCounter_InputVerticesRead:
      desc.name = "Input Vertices Read";
      desc.description = "Number of vertices read by input assembler.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_IAPrimitives:
      desc.name = "Input Primitives";
      desc.description = "Number of primitives read by the input assembler.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_GSPrimitives:
      desc.name = "GS Primitives";
      desc.description = "Number of primitives output by a geometry shader.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_RasterizerInvocations:
      desc.name = "Rasterizer Invocations";
      desc.description = "Number of primitives that were sent to the rasterizer.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_RasterizedPrimitives:
      desc.name = "Rasterized Primitives";
      desc.description = "Number of primitives that were rendered.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_SamplesWritten:
      desc.name = "Samples Written";
      desc.description = "Number of samples that passed depth/stencil test.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_VSInvocations:
      desc.name = "VS Invocations";
      desc.description = "Number of times a vertex shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_GSInvocations:
      desc.name = "GS Invocations";
      desc.description = "Number of times a geometry shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_HSInvocations:
      desc.name = "HS Invocations";
      desc.description = "Number of times a hull shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_DSInvocations:
      desc.name = "DS Invocations";
      desc.description =
          "Number of times a domain shader (or tesselation evaluation shader in OpenGL) was "
          "invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_PSInvocations:
      desc.name = "PS Invocations";
      desc.description = "Number of times a pixel shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_CSInvocations:
      desc.name = "CS Invocations";
      desc.description = "Number of times a compute shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    default:
      desc.name = "Unknown";
      desc.description = "Unknown counter ID";
      desc.resultByteWidth = 0;
      desc.resultCompType = eCompType_None;
      desc.units = eUnits_Absolute;
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

struct CounterContext
{
  uint32_t eventStart;
  vector<GPUTimer> timers;
  int reuseIdx;
};

void D3D11DebugManager::FillTimers(CounterContext &ctx, const DrawcallTreeNode &drawnode)
{
  const D3D11_QUERY_DESC qtimedesc = {D3D11_QUERY_TIMESTAMP, 0};
  const D3D11_QUERY_DESC qstatsdesc = {D3D11_QUERY_PIPELINE_STATISTICS, 0};
  const D3D11_QUERY_DESC qoccldesc = {D3D11_QUERY_OCCLUSION, 0};

  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const FetchDrawcall &d = drawnode.children[i].draw;
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

vector<CounterResult> D3D11DebugManager::FetchCounters(const vector<uint32_t> &counters)
{
  vector<CounterResult> ret;

  if(counters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  SCOPED_TIMER("Fetch Counters, counters to fetch %u", counters.size());

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

  CounterContext ctx;

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

          for(size_t c = 0; c < counters.size(); c++)
          {
            switch(counters[c])
            {
              case eCounter_EventGPUDuration:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventID, eCounter_EventGPUDuration, duration));
                break;
              case eCounter_InputVerticesRead:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_InputVerticesRead,
                                            pipelineStats.IAVertices));
                break;
              case eCounter_IAPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_IAPrimitives,
                                            pipelineStats.IAPrimitives));
                break;
              case eCounter_VSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_VSInvocations,
                                            pipelineStats.VSInvocations));
                break;
              case eCounter_GSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_GSInvocations,
                                            pipelineStats.GSInvocations));
                break;
              case eCounter_GSPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_GSPrimitives,
                                            pipelineStats.GSPrimitives));
                break;
              case eCounter_RasterizerInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_RasterizerInvocations,
                                            pipelineStats.CInvocations));
                break;
              case eCounter_RasterizedPrimitives:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_RasterizedPrimitives,
                                            pipelineStats.CPrimitives));
                break;
              case eCounter_PSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_PSInvocations,
                                            pipelineStats.PSInvocations));
                break;
              case eCounter_HSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_HSInvocations,
                                            pipelineStats.HSInvocations));
                break;
              case eCounter_DSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_DSInvocations,
                                            pipelineStats.DSInvocations));
                break;
              case eCounter_CSInvocations:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_CSInvocations,
                                            pipelineStats.CSInvocations));
                break;
              case eCounter_SamplesWritten:
                ret.push_back(
                    CounterResult(ctx.timers[i].eventID, eCounter_SamplesWritten, occlusion));
                break;
            }
          }
        }
        else
        {
          for(size_t c = 0; c < counters.size(); c++)
          {
            switch(counters[c])
            {
              case eCounter_EventGPUDuration:
                ret.push_back(CounterResult(ctx.timers[i].eventID, eCounter_EventGPUDuration, -1.0));
                break;
              case eCounter_InputVerticesRead:
              case eCounter_IAPrimitives:
              case eCounter_GSPrimitives:
              case eCounter_RasterizerInvocations:
              case eCounter_RasterizedPrimitives:
              case eCounter_VSInvocations:
              case eCounter_HSInvocations:
              case eCounter_DSInvocations:
              case eCounter_GSInvocations:
              case eCounter_PSInvocations:
              case eCounter_CSInvocations:
              case eCounter_SamplesWritten:
                ret.push_back(CounterResult(ctx.timers[i].eventID, counters[c], 0xFFFFFFFFFFFFFFFF));
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
