/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

  return ret;
}

void D3D11DebugManager::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

  if(counterID == eCounter_EventGPUDuration)
  {
    desc.name = "GPU Duration";
    desc.description =
        "Time taken for this event on the GPU, as measured by delta between two GPU timestamps.";
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

struct GPUTimer
{
  ID3D11Query *before;
  ID3D11Query *after;
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
  const D3D11_QUERY_DESC qdesc = {D3D11_QUERY_TIMESTAMP, 0};

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
        timer->before = timer->after = NULL;

        hr = m_pDevice->CreateQuery(&qdesc, &timer->before);
        RDCASSERTEQUAL(hr, S_OK);
        hr = m_pDevice->CreateQuery(&qdesc, &timer->after);
        RDCASSERTEQUAL(hr, S_OK);
      }
      else
      {
        timer = &ctx.timers[ctx.reuseIdx++];
      }
    }

    m_WrappedDevice->ReplayLog(ctx.eventStart, d.eventID, eReplay_WithoutDraw);

    m_pImmediateContext->Flush();

    if(timer->before && timer->after)
    {
      m_pImmediateContext->End(timer->before);
      m_WrappedDevice->ReplayLog(ctx.eventStart, d.eventID, eReplay_OnlyDraw);
      m_pImmediateContext->End(timer->after);
    }
    else
    {
      m_WrappedDevice->ReplayLog(ctx.eventStart, d.eventID, eReplay_OnlyDraw);
    }

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

  uint32_t counterID = counters[0];
  RDCASSERT(counters.size() == 1);
  RDCASSERT(counterID == eCounter_EventGPUDuration);

  SCOPED_TIMER("Fetch Counters for %u", counterID);

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
        if(ctx.timers[i].before && ctx.timers[i].after)
        {
          hr = m_pImmediateContext->GetData(ctx.timers[i].before, &a, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          UINT64 b = 0;
          hr = m_pImmediateContext->GetData(ctx.timers[i].after, &b, sizeof(UINT64), 0);
          RDCASSERTEQUAL(hr, S_OK);

          double duration = (double(b - a) / ticksToSecs);

          ret.push_back(CounterResult(ctx.timers[i].eventID, counterID, duration));

          a = b;
        }
        else
        {
          ret.push_back(CounterResult(ctx.timers[i].eventID, counterID, 0.0));
        }
      }
    }
  }

  for(size_t i = 0; i < ctx.timers.size(); i++)
  {
    SAFE_RELEASE(ctx.timers[i].before);
    SAFE_RELEASE(ctx.timers[i].after);
  }

  SAFE_RELEASE(disjoint);
  SAFE_RELEASE(start);

  return ret;
}
