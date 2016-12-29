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

vector<uint32_t> GLReplay::EnumerateCounters()
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
  ret.push_back(eCounter_TCSInvocations);
  ret.push_back(eCounter_TESInvocations);
  ret.push_back(eCounter_GSInvocations);
  ret.push_back(eCounter_PSInvocations);
  ret.push_back(eCounter_CSInvocations);

  return ret;
}

void GLReplay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
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
    case eCounter_TCSInvocations:
      desc.name = "TCS Invocations";
      desc.description = "Number of times a tesselation control shader was invoked.";
      desc.resultByteWidth = 8;
      desc.resultCompType = eCompType_UInt;
      desc.units = eUnits_Absolute;
      break;
    case eCounter_TESInvocations:
      desc.name = "TES Invocations";
      desc.description = "Number of times a tesselation evaluation shader was invoked.";
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

struct GPUQueries
{
  GLuint obj[eCounter_GLMaxCounters];
  uint32_t eventID;
};

struct CounterContext
{
  uint32_t eventStart;
  vector<GPUQueries> queries;
  int reuseIdx;
};

GLenum glCounters[] = {
    eGL_NONE,                                      // Undefined!!
    eGL_TIME_ELAPSED,                              // eCounter_EventGPUDuration
    eGL_VERTICES_SUBMITTED_ARB,                    // eCounter_InputVerticesRead
    eGL_PRIMITIVES_SUBMITTED_ARB,                  // eCounter_IAPrimitives
    eGL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB,    // eCounter_GSPrimitives
    eGL_CLIPPING_INPUT_PRIMITIVES_ARB,             // eCounter_RasterizerInvocations
    eGL_CLIPPING_OUTPUT_PRIMITIVES_ARB,            // eCounter_RasterizedPrimitives
    eGL_SAMPLES_PASSED,                            // eCounter_SamplesWritten
    eGL_VERTEX_SHADER_INVOCATIONS_ARB,             // eCounter_VSInvocations
    eGL_TESS_CONTROL_SHADER_PATCHES_ARB,           // eCounter_TCSInvocations
    eGL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB,    // eCounter_TESInvocations
    eGL_GEOMETRY_SHADER_INVOCATIONS,               // eCounter_GSInvocations
    eGL_FRAGMENT_SHADER_INVOCATIONS_ARB,           // eCounter_PSInvocations
    eGL_COMPUTE_SHADER_INVOCATIONS_ARB             // eCounter_CSInvocations
};

void GLReplay::FillTimers(CounterContext &ctx, const DrawcallTreeNode &drawnode,
                          const vector<uint32_t> &counters)
{
  if(drawnode.children.empty())
    return;

  for(size_t i = 0; i < drawnode.children.size(); i++)
  {
    const FetchDrawcall &d = drawnode.children[i].draw;
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
        for(uint32_t q = 0; q < eCounter_GLMaxCounters; q++)
          queries->obj[q] = 0;

        for(uint32_t c = 0; c < counters.size(); c++)
        {
          m_pDriver->glGenQueries(1, &queries->obj[counters[c]]);
          if(m_pDriver->glGetError())
            queries->obj[counters[c]] = 0;
        }
      }
      else
      {
        queries = &ctx.queries[ctx.reuseIdx++];
      }
    }

    m_pDriver->ReplayLog(ctx.eventStart, d.eventID, eReplay_WithoutDraw);

    // Reverse order so that Timer counter is queried the last.
    for(int32_t q = (eCounter_GLMaxCounters - 1); q >= 0; q--)
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

    for(uint32_t q = 0; q < eCounter_GLMaxCounters; q++)
      if(queries->obj[q])
        m_pDriver->glEndQuery(glCounters[q]);

    ctx.eventStart = d.eventID + 1;
  }
}

vector<CounterResult> GLReplay::FetchCounters(const vector<uint32_t> &counters)
{
  vector<CounterResult> ret;

  if(counters.empty())
  {
    RDCERR("No counters specified to FetchCounters");
    return ret;
  }

  MakeCurrentReplayContext(&m_ReplayCtx);

  CounterContext ctx;

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
        if(ctx.queries[i].obj[counters[c]])
        {
          GLuint64 data = 0;
          m_pDriver->glGetQueryObjectui64v(ctx.queries[i].obj[counters[c]], eGL_QUERY_RESULT, &data);

          double duration = double(data) * nanosToSecs;

          if(m_pDriver->glGetError())
          {
            data = (uint64_t)-1;
            duration = -1;
          }

          if(counters[c] == eCounter_EventGPUDuration)
          {
            ret.push_back(CounterResult(ctx.queries[i].eventID, eCounter_EventGPUDuration, duration));
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
      if(ctx.queries[i].obj[counters[c]])
        m_pDriver->glDeleteQueries(1, &ctx.queries[i].obj[counters[c]]);

  return ret;
}
