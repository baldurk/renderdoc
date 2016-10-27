/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
 * Copyright (c) 2016 University of Szeged
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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

#include "gles_driver.h"
#include "gles_replay.h"
#include "gles_resources.h"

void GLESReplay::PreContextInitCounters()
{
}

void GLESReplay::PostContextInitCounters()
{
}

void GLESReplay::PreContextShutdownCounters()
{
}

void GLESReplay::PostContextShutdownCounters()
{
}

vector<uint32_t> GLESReplay::EnumerateCounters()
{
  vector<uint32_t> ret;

  ret.push_back(eCounter_EventGPUDuration);

  return ret;
}

void GLESReplay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
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
  GLuint obj;
  uint32_t eventID;
};

struct CounterContext
{
  uint32_t eventStart;
  vector<GPUTimer> timers;
  int reuseIdx;
};

void GLESReplay::FillTimers(CounterContext &ctx, const DrawcallTreeNode &drawnode)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const FetchDrawcall &d = drawnode.children[i].draw;
    FillTimers(ctx, drawnode.children[i]);

    if(d.events.count == 0)
      continue;

    GPUTimer *timer = NULL;

    {
      if(ctx.reuseIdx == -1)
      {
        ctx.timers.push_back(GPUTimer());

        timer = &ctx.timers.back();
        timer->eventID = d.eventID;
        timer->obj = 0;

        m_pDriver->glGenQueries(1, &timer->obj);
      }
      else
      {
        timer = &ctx.timers[ctx.reuseIdx++];
      }
    }

    m_pDriver->ReplayLog(ctx.eventStart, d.eventID, eReplay_WithoutDraw);

    if(timer->obj)
    {
      m_pDriver->glBeginQuery(eGL_TIME_ELAPSED_EXT, timer->obj);
      m_pDriver->ReplayLog(ctx.eventStart, d.eventID, eReplay_OnlyDraw);
      m_pDriver->glEndQuery(eGL_TIME_ELAPSED_EXT);
    }
    else
    {
      m_pDriver->ReplayLog(ctx.eventStart, d.eventID, eReplay_OnlyDraw);
    }

    ctx.eventStart = d.eventID + 1;
  }
}

vector<CounterResult> GLESReplay::FetchCounters(const vector<uint32_t> &counters)
{
  vector<CounterResult> ret;

  if(counters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);

  uint32_t counterID = counters[0];
  RDCASSERT(counters.size() == 1);
  RDCASSERT(counterID == eCounter_EventGPUDuration);

  SCOPED_TIMER("Fetch Counters for %u", counterID);

  CounterContext ctx;

  for(int loop = 0; loop < 1; loop++)
  {
    ctx.eventStart = 0;
    ctx.reuseIdx = loop == 0 ? -1 : 0;
    FillTimers(ctx, m_pDriver->GetRootDraw());

    double nanosToSecs = 1.0 / 1000000000.0;

    for(size_t i = 0; i < ctx.timers.size(); i++)
    {
      if(ctx.timers[i].obj)
      {
        GLuint elapsed = 0;
        m_pDriver->glGetQueryObjectuiv(ctx.timers[i].obj, eGL_QUERY_RESULT, &elapsed);

        double duration = double(elapsed) * nanosToSecs;

        ret.push_back(CounterResult(ctx.timers[i].eventID, counterID, duration));
      }
      else
      {
        ret.push_back(CounterResult(ctx.timers[i].eventID, counterID, 0.0));
      }
    }

  }

  for(size_t i = 0; i < ctx.timers.size(); i++)
    m_pDriver->glDeleteQueries(1, &ctx.timers[i].obj);

  return ret;
}
