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

#include "gl_driver.h"
#include "gl_replay.h"
#include "gl_resources.h"

void GLReplay::PreContextInitCounters()
{
}

void GLReplay::PostContextInitCounters()
{
}

void GLReplay::PreContextShutdownCounters()
{
}

void GLReplay::PostContextShutdownCounters()
{
}

vector<GPUCounter> GLReplay::EnumerateCounters()
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
  ret.push_back(GPUCounter::TCSInvocations);
  ret.push_back(GPUCounter::TESInvocations);
  ret.push_back(GPUCounter::GSInvocations);
  ret.push_back(GPUCounter::PSInvocations);
  ret.push_back(GPUCounter::CSInvocations);

  return ret;
}

void GLReplay::DescribeCounter(GPUCounter counterID, CounterDescription &desc)
{
  desc.counterID = counterID;

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
}

struct GPUQueries
{
  GLuint obj[ENUM_ARRAY_SIZE(GPUCounter)];
  uint32_t eventID;
};

struct GLCounterContext
{
  uint32_t eventStart;
  vector<GPUQueries> queries;
  int reuseIdx;
};

GLenum glCounters[] = {
    eGL_NONE,                                      // Undefined!!
    eGL_TIME_ELAPSED,                              // GPUCounter::EventGPUDuration
    eGL_VERTICES_SUBMITTED_ARB,                    // GPUCounter::InputVerticesRead
    eGL_PRIMITIVES_SUBMITTED_ARB,                  // GPUCounter::IAPrimitives
    eGL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB,    // GPUCounter::GSPrimitives
    eGL_CLIPPING_INPUT_PRIMITIVES_ARB,             // GPUCounter::RasterizerInvocations
    eGL_CLIPPING_OUTPUT_PRIMITIVES_ARB,            // GPUCounter::RasterizedPrimitives
    eGL_SAMPLES_PASSED,                            // GPUCounter::SamplesWritten
    eGL_VERTEX_SHADER_INVOCATIONS_ARB,             // GPUCounter::VSInvocations
    eGL_TESS_CONTROL_SHADER_PATCHES_ARB,           // GPUCounter::TCSInvocations
    eGL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB,    // GPUCounter::TESInvocations
    eGL_GEOMETRY_SHADER_INVOCATIONS,               // GPUCounter::GSInvocations
    eGL_FRAGMENT_SHADER_INVOCATIONS_ARB,           // GPUCounter::PSInvocations
    eGL_COMPUTE_SHADER_INVOCATIONS_ARB             // GPUCounter::CSInvocations
};

void GLReplay::FillTimers(GLCounterContext &ctx, const DrawcallTreeNode &drawnode,
                          const vector<GPUCounter> &counters)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const DrawcallDescription &d = drawnode.children[i].draw;
    FillTimers(ctx, drawnode.children[i], counters);

    if(d.events.count == 0)
      continue;

    GPUQueries *queries = NULL;

    {
      if(ctx.reuseIdx == -1)
      {
        ctx.queries.push_back(GPUQueries());

        queries = &ctx.queries.back();
        queries->eventID = d.eventID;
        for(auto q : indices<GPUCounter>())
          queries->obj[q] = 0;

        for(uint32_t c = 0; c < counters.size(); c++)
        {
          m_pDriver->glGenQueries(1, &queries->obj[(uint32_t)counters[c]]);
          if(m_pDriver->glGetError())
            queries->obj[(uint32_t)counters[c]] = 0;
        }
      }
      else
      {
        queries = &ctx.queries[ctx.reuseIdx++];
      }
    }

    m_pDriver->ReplayLog(ctx.eventStart, d.eventID, eReplay_WithoutDraw);

    // Reverse order so that Timer counter is queried the last.
    for(int32_t q = uint32_t(GPUCounter::Count) - 1; q >= 0; q--)
      if(queries->obj[q])
      {
        m_pDriver->glBeginQuery(glCounters[q], queries->obj[q]);
        if(m_pDriver->glGetError())
        {
          m_pDriver->glDeleteQueries(1, &queries->obj[q]);
          queries->obj[q] = 0;
        }
      }

    m_pDriver->ReplayLog(ctx.eventStart, d.eventID, eReplay_OnlyDraw);

    for(auto q : indices<GPUCounter>())
      if(queries->obj[q])
        m_pDriver->glEndQuery(glCounters[q]);

    ctx.eventStart = d.eventID + 1;
  }
}

vector<CounterResult> GLReplay::FetchCounters(const vector<GPUCounter> &counters)
{
  vector<CounterResult> ret;

  if(counters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);

  GLCounterContext ctx;

  for(int loop = 0; loop < 1; loop++)
  {
    ctx.eventStart = 0;
    ctx.reuseIdx = loop == 0 ? -1 : 0;
    m_pDriver->SetFetchCounters(true);
    FillTimers(ctx, m_pDriver->GetRootDraw(), counters);
    m_pDriver->SetFetchCounters(false);

    double nanosToSecs = 1.0 / 1000000000.0;

    GLuint prevbind = 0;
    m_pDriver->glGetIntegerv(eGL_QUERY_BUFFER_BINDING, (GLint *)&prevbind);
    m_pDriver->glBindBuffer(eGL_QUERY_BUFFER, 0);

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

          if(m_pDriver->glGetError())
          {
            data = (uint64_t)-1;
            duration = -1;
          }

          if(counters[c] == GPUCounter::EventGPUDuration)
          {
            ret.push_back(
                CounterResult(ctx.queries[i].eventID, GPUCounter::EventGPUDuration, duration));
          }
          else
            ret.push_back(CounterResult(ctx.queries[i].eventID, counters[c], data));
        }
        else
          ret.push_back(CounterResult(ctx.queries[i].eventID, counters[c], (uint64_t)-1));
      }
    }

    m_pDriver->glBindBuffer(eGL_QUERY_BUFFER, prevbind);
  }

  for(size_t i = 0; i < ctx.queries.size(); i++)
    for(uint32_t c = 0; c < counters.size(); c++)
      if(ctx.queries[i].obj[(uint32_t)counters[c]])
        m_pDriver->glDeleteQueries(1, &ctx.queries[i].obj[(uint32_t)counters[c]]);

  return ret;
}
