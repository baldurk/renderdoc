/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "driver/ihv/amd/amd_counters.h"
#include "driver/ihv/arm/arm_counters.h"
#include "driver/ihv/intel/intel_gl_counters.h"
#include "driver/ihv/nv/nv_gl_counters.h"
#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

rdcarray<GPUCounter> GLReplay::EnumerateCounters()
{
  rdcarray<GPUCounter> ret;

  if(HasExt[ARB_timer_query])
    ret.push_back(GPUCounter::EventGPUDuration);

  if(HasExt[ARB_occlusion_query2])
    ret.push_back(GPUCounter::SamplesPassed);

  if(HasExt[ARB_pipeline_statistics_query])
  {
    ret.push_back(GPUCounter::InputVerticesRead);
    ret.push_back(GPUCounter::IAPrimitives);
    ret.push_back(GPUCounter::GSPrimitives);
    ret.push_back(GPUCounter::RasterizerInvocations);
    ret.push_back(GPUCounter::RasterizedPrimitives);
    ret.push_back(GPUCounter::VSInvocations);
    ret.push_back(GPUCounter::TCSInvocations);
    ret.push_back(GPUCounter::TESInvocations);
    ret.push_back(GPUCounter::GSInvocations);
    ret.push_back(GPUCounter::PSInvocations);
    ret.push_back(GPUCounter::CSInvocations);
  }

  if(m_pAMDCounters)
  {
    ret.append(m_pAMDCounters->GetPublicCounterIds());
  }

  if(m_pIntelCounters)
  {
    ret.append(m_pIntelCounters->GetPublicCounterIds());
  }

  if(m_pARMCounters)
  {
    ret.append(m_pARMCounters->GetPublicCounterIds());
  }

#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
  if(m_pNVCounters)
  {
    ret.append(m_pNVCounters->EnumerateCounters());
  }
#endif

  return ret;
}

CounterDescription GLReplay::DescribeCounter(GPUCounter counterID)
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

  /////Intel/////
  if(IsIntelCounter(counterID))
  {
    if(m_pIntelCounters)
    {
      desc = m_pIntelCounters->GetCounterDescription(counterID);

      return desc;
    }
  }

  if(IsARMCounter(counterID) && m_pARMCounters)
  {
    return m_pARMCounters->GetCounterDescription(counterID);
  }

#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
  /////NVIDIA/////
  if(m_pNVCounters && m_pNVCounters->HasCounter(counterID))
  {
    return m_pNVCounters->DescribeCounter(counterID);
  }
#endif

  // FFBA5548-FBF8-405D-BA18-F0329DA370A0
  desc.uuid.words[0] = 0xFFBA5548;
  desc.uuid.words[1] = 0xFBF8405D;
  desc.uuid.words[2] = 0xBA18F032;
  desc.uuid.words[3] = 0x9DA370A0 ^ (uint32_t)counterID;

  desc.category = "OpenGL Built-in";

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

struct GPUQueries
{
  GLuint obj[arraydim<GPUCounter>()];
  uint32_t eventId;
};

struct GLCounterContext
{
  uint32_t eventStart;
  rdcarray<GPUQueries> queries;
};

GLenum glCounters[] = {
    eGL_NONE,                                      // Undefined!!
    eGL_TIME_ELAPSED,                              // GPUCounter::EventGPUDuration
    eGL_VERTICES_SUBMITTED_ARB,                    // GPUCounter::InputVerticesRead
    eGL_PRIMITIVES_SUBMITTED_ARB,                  // GPUCounter::IAPrimitives
    eGL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB,    // GPUCounter::GSPrimitives
    eGL_CLIPPING_INPUT_PRIMITIVES_ARB,             // GPUCounter::RasterizerInvocations
    eGL_CLIPPING_OUTPUT_PRIMITIVES_ARB,            // GPUCounter::RasterizedPrimitives
    eGL_SAMPLES_PASSED,                            // GPUCounter::SamplesPassed
    eGL_VERTEX_SHADER_INVOCATIONS_ARB,             // GPUCounter::VSInvocations
    eGL_TESS_CONTROL_SHADER_PATCHES_ARB,           // GPUCounter::TCSInvocations
    eGL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB,    // GPUCounter::TESInvocations
    eGL_GEOMETRY_SHADER_INVOCATIONS,               // GPUCounter::GSInvocations
    eGL_FRAGMENT_SHADER_INVOCATIONS_ARB,           // GPUCounter::PSInvocations
    eGL_COMPUTE_SHADER_INVOCATIONS_ARB             // GPUCounter::CSInvocations
};

void GLReplay::FillTimers(GLCounterContext &ctx, const ActionDescription &actionnode,
                          const rdcarray<GPUCounter> &counters)
{
  if(actionnode.children.empty())
    return;

  for(size_t i = 0; i < actionnode.children.size(); i++)
  {
    const ActionDescription &a = actionnode.children[i];
    FillTimers(ctx, a, counters);

    // for marker regions and multi-actions, don't fetch counters for them, only their children
    if(!a.children.empty())
      continue;

    GPUQueries *queries = NULL;

    {
      ctx.queries.push_back(GPUQueries());

      queries = &ctx.queries.back();
      queries->eventId = a.eventId;
      for(auto q : indices<GPUCounter>())
        queries->obj[q] = 0;

      for(uint32_t c = 0; c < counters.size(); c++)
      {
        m_pDriver->glGenQueries(1, &queries->obj[(uint32_t)counters[c]]);
        if(m_pDriver->glGetError() != eGL_NONE)
          queries->obj[(uint32_t)counters[c]] = 0;
      }
    }

    m_pDriver->ReplayLog(ctx.eventStart, a.eventId, eReplay_WithoutDraw);

    ClearGLErrors();

    // Reverse order so that Timer counter is queried the last.
    for(int32_t q = uint32_t(GPUCounter::Count) - 1; q >= 0; q--)
    {
      if(queries->obj[q])
      {
        m_pDriver->glBeginQuery(glCounters[q], queries->obj[q]);
        if(m_pDriver->glGetError() != eGL_NONE)
        {
          m_pDriver->glDeleteQueries(1, &queries->obj[q]);
          queries->obj[q] = 0;
        }
      }
    }

    m_pDriver->ReplayLog(ctx.eventStart, a.eventId, eReplay_OnlyDraw);

    for(auto q : indices<GPUCounter>())
      if(queries->obj[q])
        m_pDriver->glEndQuery(glCounters[q]);

    ctx.eventStart = a.eventId + 1;
  }
}

void GLReplay::FillTimersAMD(uint32_t *eventStartID, uint32_t *sampleIndex,
                             rdcarray<uint32_t> *eventIDs, const ActionDescription &actionnode)
{
  if(actionnode.children.empty())
    return;

  for(size_t i = 0; i < actionnode.children.size(); i++)
  {
    const ActionDescription &a = actionnode.children[i];

    FillTimersAMD(eventStartID, sampleIndex, eventIDs, actionnode.children[i]);

    if(a.events.empty() ||
       (a.flags & (ActionFlags::PushMarker | ActionFlags::SetMarker | ActionFlags::PopMarker)))
      continue;

    eventIDs->push_back(a.eventId);

    m_pDriver->ReplayLog(*eventStartID, a.eventId, eReplay_WithoutDraw);

    m_pAMDCounters->BeginSample(*sampleIndex);

    m_pDriver->ReplayLog(*eventStartID, a.eventId, eReplay_OnlyDraw);

    m_pAMDCounters->EndSample();

    *eventStartID = a.eventId + 1;
    ++*sampleIndex;
  }
}

rdcarray<CounterResult> GLReplay::FetchCountersAMD(const rdcarray<GPUCounter> &counters)
{
  if(!m_pAMDCounters->BeginMeasurementMode(AMDCounters::ApiType::Ogl, m_ReplayCtx.ctx))
  {
    return rdcarray<CounterResult>();
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

  m_pDriver->ReplayMarkers(false);

  for(uint32_t p = 0; p < passCount; p++)
  {
    m_pAMDCounters->BeginPass();
    m_pAMDCounters->BeginCommandList();
    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersAMD(&eventStartID, &sampleIndex, &eventIDs, m_pDriver->GetRootAction());
    m_pAMDCounters->EndCommandList();
    m_pAMDCounters->EndPass();
  }

  m_pDriver->ReplayMarkers(true);

  m_pAMDCounters->EndSesssion(sessionID);

  rdcarray<CounterResult> ret =
      m_pAMDCounters->GetCounterData(sessionID, sampleIndex, eventIDs, counters);

  m_pAMDCounters->EndMeasurementMode();

  return ret;
}

void GLReplay::FillTimersIntel(uint32_t *eventStartID, uint32_t *sampleIndex,
                               rdcarray<uint32_t> *eventIDs, const ActionDescription &actionnode)
{
  if(actionnode.children.empty())
    return;

  for(size_t i = 0; i < actionnode.children.size(); i++)
  {
    const ActionDescription &a = actionnode.children[i];

    FillTimersIntel(eventStartID, sampleIndex, eventIDs, actionnode.children[i]);

    if(a.events.empty())
      continue;

    eventIDs->push_back(a.eventId);

    m_pDriver->ReplayLog(*eventStartID, a.eventId, eReplay_WithoutDraw);

    m_pIntelCounters->BeginSample(*sampleIndex);

    m_pDriver->ReplayLog(*eventStartID, a.eventId, eReplay_OnlyDraw);

    m_pIntelCounters->EndSample();

    *eventStartID = a.eventId + 1;
    ++*sampleIndex;
  }
}

rdcarray<CounterResult> GLReplay::FetchCountersIntel(const rdcarray<GPUCounter> &counters)
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

  rdcarray<uint32_t> eventIDs;

  m_pDriver->ReplayMarkers(false);

  for(uint32_t p = 0; p < passCount; p++)
  {
    m_pIntelCounters->BeginPass(p);

    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersIntel(&eventStartID, &sampleIndex, &eventIDs, m_pDriver->GetRootAction());
    m_pIntelCounters->EndPass();
  }

  m_pDriver->ReplayMarkers(true);

  rdcarray<CounterResult> ret = m_pIntelCounters->GetCounterData(sampleIndex, eventIDs, counters);

  m_pIntelCounters->EndSession();

  return ret;
}

void GLReplay::FillTimersARM(uint32_t *eventStartID, uint32_t *sampleIndex,
                             rdcarray<uint32_t> *eventIDs, const ActionDescription &actionnode)
{
  if(actionnode.children.empty())
    return;

  for(size_t i = 0; i < actionnode.children.size(); i++)
  {
    const ActionDescription &a = actionnode.children[i];

    FillTimersARM(eventStartID, sampleIndex, eventIDs, actionnode.children[i]);

    if(a.events.empty())
      continue;

    eventIDs->push_back(a.eventId);

    m_pDriver->ReplayLog(*eventStartID, a.eventId, eReplay_WithoutDraw);

    m_pARMCounters->BeginSample(a.eventId);

    m_pDriver->ReplayLog(*eventStartID, a.eventId, eReplay_OnlyDraw);

    // wait for the GPU to process all commands
    GLsync sync = GL.glFenceSync(eGL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GL.glClientWaitSync(sync, eGL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);

    m_pARMCounters->EndSample();

    GL.glDeleteSync(sync);

    *eventStartID = a.eventId + 1;
    ++*sampleIndex;
  }
}

rdcarray<CounterResult> GLReplay::FetchCountersARM(const rdcarray<GPUCounter> &counters)
{
  m_pARMCounters->DisableAllCounters();

  // enable counters it needs
  for(size_t i = 0; i < counters.size(); i++)
  {
    // This function is only called internally, and violating this assertion means our
    // caller has invoked this method incorrectly
    RDCASSERT(IsARMCounter(counters[i]));
    m_pARMCounters->EnableCounter(counters[i]);
  }

  uint32_t passCount = m_pARMCounters->GetPassCount();

  uint32_t sampleIndex = 0;

  rdcarray<uint32_t> eventIDs;

  m_pDriver->ReplayMarkers(false);

  for(uint32_t p = 0; p < passCount; p++)
  {
    m_pARMCounters->BeginPass(p);

    uint32_t eventStartID = 0;

    sampleIndex = 0;

    eventIDs.clear();

    FillTimersARM(&eventStartID, &sampleIndex, &eventIDs, m_pDriver->GetRootAction());

    m_pARMCounters->EndPass();
  }
  m_pDriver->ReplayMarkers(true);

  rdcarray<CounterResult> ret = m_pARMCounters->GetCounterData(eventIDs, counters);

  return ret;
}

rdcarray<CounterResult> GLReplay::FetchCounters(const rdcarray<GPUCounter> &allCounters)
{
  rdcarray<CounterResult> ret;

  if(allCounters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);

  rdcarray<GPUCounter> counters;
  std::copy_if(allCounters.begin(), allCounters.end(), std::back_inserter(counters),
               [](const GPUCounter &c) { return IsGenericCounter(c); });

  m_pDriver->SetFetchCounters(true);

  if(m_pAMDCounters)
  {
    // Filter out the AMD counters
    rdcarray<GPUCounter> amdCounters;
    std::copy_if(allCounters.begin(), allCounters.end(), std::back_inserter(amdCounters),
                 [](const GPUCounter &c) { return IsAMDCounter(c); });

    if(!amdCounters.empty())
    {
      ret = FetchCountersAMD(amdCounters);
    }
  }

  if(m_pIntelCounters)
  {
    // Filter out the Intel counters
    rdcarray<GPUCounter> intelCounters;
    std::copy_if(allCounters.begin(), allCounters.end(), std::back_inserter(intelCounters),
                 [](const GPUCounter &c) { return IsIntelCounter(c); });

    if(!intelCounters.empty())
    {
      ret = FetchCountersIntel(intelCounters);
    }
  }

  if(m_pARMCounters)
  {
    rdcarray<GPUCounter> armCounters;
    std::copy_if(allCounters.begin(), allCounters.end(), std::back_inserter(armCounters),
                 [](const GPUCounter &c) { return IsARMCounter(c); });

    if(!armCounters.empty())
      ret = FetchCountersARM(armCounters);
  }

#if DISABLED(RDOC_ANDROID) && DISABLED(RDOC_APPLE)
  if(m_pNVCounters)
  {
    // Filter out the NVIDIA counters
    rdcarray<GPUCounter> nvCounters;
    std::copy_if(allCounters.begin(), allCounters.end(), std::back_inserter(nvCounters),
                 [=](const GPUCounter &c) { return m_pNVCounters->HasCounter(c); });
    if(!nvCounters.empty())
    {
      rdcarray<CounterResult> results = m_pNVCounters->FetchCounters(nvCounters, m_pDriver);
      ret.append(results);
    }
  }
#endif

  m_pDriver->SetFetchCounters(false);

  if(counters.empty())
  {
    return ret;
  }

  GLCounterContext ctx;
  ctx.eventStart = 0;

  m_pDriver->ReplayMarkers(false);

  m_pDriver->SetFetchCounters(true);
  FillTimers(ctx, m_pDriver->GetRootAction(), counters);
  m_pDriver->SetFetchCounters(false);

  m_pDriver->ReplayMarkers(true);

  double nanosToSecs = 1.0 / 1000000000.0;

  GLuint prevbind = 0;
  if(HasExt[ARB_query_buffer_object])
  {
    m_pDriver->glGetIntegerv(eGL_QUERY_BUFFER_BINDING, (GLint *)&prevbind);
    m_pDriver->glBindBuffer(eGL_QUERY_BUFFER, 0);
  }

  for(size_t i = 0; i < ctx.queries.size(); i++)
  {
    for(uint32_t c = 0; c < counters.size(); c++)
    {
      if(ctx.queries[i].obj[(uint32_t)counters[c]])
      {
        GLuint64 data = 0;
        m_pDriver->glGetQueryObjectui64v(ctx.queries[i].obj[(uint32_t)counters[c]],
                                         eGL_QUERY_RESULT, &data);

        double duration = double(data) * nanosToSecs;

        if(m_pDriver->glGetError() != eGL_NONE)
        {
          data = (uint64_t)-1;
          duration = -1;
        }

        if(counters[c] == GPUCounter::EventGPUDuration)
        {
          ret.push_back(CounterResult(ctx.queries[i].eventId, GPUCounter::EventGPUDuration, duration));
        }
        else
          ret.push_back(CounterResult(ctx.queries[i].eventId, counters[c], data));
      }
      else
        ret.push_back(CounterResult(ctx.queries[i].eventId, counters[c], (uint64_t)-1));
    }
  }

  if(HasExt[ARB_query_buffer_object])
    m_pDriver->glBindBuffer(eGL_QUERY_BUFFER, prevbind);

  for(size_t i = 0; i < ctx.queries.size(); i++)
    for(uint32_t c = 0; c < counters.size(); c++)
      if(ctx.queries[i].obj[(uint32_t)counters[c]])
        m_pDriver->glDeleteQueries(1, &ctx.queries[i].obj[(uint32_t)counters[c]]);

  return ret;
}
