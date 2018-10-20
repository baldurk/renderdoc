/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include "driver/gl/gl_dispatch_table.h"
#include "driver/gl/gl_driver.h"

#include "intel_gl_counters.h"

const static std::vector<std::string> metricSetBlacklist = {"TestOa"};

IntelGlCounters::IntelGlCounters() : m_passIndex(0), m_sampleIndex(0)
{
}

IntelGlCounters::~IntelGlCounters()
{
}

std::vector<GPUCounter> IntelGlCounters::GetPublicCounterIds() const
{
  vector<GPUCounter> counters;

  for(const auto &c : m_Counters)
    counters.push_back(c.desc.counter);

  return counters;
}

CounterDescription IntelGlCounters::GetCounterDescription(GPUCounter index) const
{
  return m_Counters[GPUCounterToCounterIndex(index)].desc;
}

static CompType glToRdcCounterType(GLuint glDataType)
{
  switch(glDataType)
  {
    case GL_PERFQUERY_COUNTER_DATA_UINT32_INTEL: return CompType::UInt;
    case GL_PERFQUERY_COUNTER_DATA_UINT64_INTEL: return CompType::UInt;
    case GL_PERFQUERY_COUNTER_DATA_FLOAT_INTEL: return CompType::Float;
    case GL_PERFQUERY_COUNTER_DATA_DOUBLE_INTEL: return CompType::Double;
    case GL_PERFQUERY_COUNTER_DATA_BOOL32_INTEL: return CompType::UInt;
    default:
      RDCASSERT(false && "Wrong type");    // unreachable
  }
  return CompType::Typeless;
}

void IntelGlCounters::addCounter(const IntelGlQuery &query, GLuint counterId)
{
  IntelGlCounter counter;

  counter.queryId = query.queryId;
  counter.desc.counter = (GPUCounter)((int)GPUCounter::FirstIntel + m_Counters.size());
  counter.desc.category = query.name;

  GLint len;
  GL.glGetIntegerv(eGL_PERFQUERY_COUNTER_NAME_LENGTH_MAX_INTEL, &len);
  counter.desc.name.resize(len);
  GL.glGetIntegerv(eGL_PERFQUERY_COUNTER_DESC_LENGTH_MAX_INTEL, &len);
  counter.desc.description.resize(100);

  GL.glGetPerfCounterInfoINTEL(
      query.queryId, counterId, counter.desc.name.size(), &counter.desc.name[0],
      counter.desc.description.size(), &counter.desc.description[0], &counter.offset,
      &counter.desc.resultByteWidth, &counter.type, &counter.dataType, NULL);

  if(m_CounterNames.find(counter.desc.name) != m_CounterNames.end())
    return;

  counter.desc.resultType = glToRdcCounterType(counter.dataType);

  m_Counters.push_back(counter);
  m_CounterNames[counter.desc.name] = counter;
}

void IntelGlCounters::addQuery(GLuint queryId)
{
  IntelGlQuery query;

  query.queryId = queryId;

  GLint len;
  GL.glGetIntegerv(eGL_PERFQUERY_QUERY_NAME_LENGTH_MAX_INTEL, &len);
  query.name.resize(len);
  GLuint nCounters;
  GL.glGetPerfQueryInfoINTEL(queryId, query.name.size(), &query.name[0], &query.size, &nCounters,
                             NULL, NULL);
  m_Queries[query.queryId] = query;

  for(GLuint c = 1; c <= nCounters; c++)
    addCounter(query, c);
}

bool IntelGlCounters::Init()
{
  if(!HasExt[INTEL_performance_query])
    return false;

  GLuint queryId;
  GL.glGetFirstPerfQueryIdINTEL(&queryId);
  GLenum err = GL.glGetError();
  if(err != eGL_NONE)
    return false;

  do
  {
    addQuery(queryId);

    GL.glGetNextPerfQueryIdINTEL(queryId, &queryId);
  } while(queryId != 0);

  return true;
}

void IntelGlCounters::EnableCounter(GPUCounter index)
{
  const IntelGlCounter &counter = m_Counters[GPUCounterToCounterIndex(index)];

  for(uint32_t p = 0; p < m_EnabledQueries.size(); p++)
  {
    if(m_EnabledQueries[p] == counter.queryId)
      return;
  }
  m_EnabledQueries.push_back(counter.queryId);
}

void IntelGlCounters::DisableAllCounters()
{
  m_EnabledQueries.clear();
}

uint32_t IntelGlCounters::GetPassCount()
{
  return m_EnabledQueries.size();
}

void IntelGlCounters::BeginSession()
{
  RDCASSERT(m_glQueries.empty());
}

void IntelGlCounters::EndSession()
{
  for(uint32_t queryHandle : m_glQueries)
    GL.glDeletePerfQueryINTEL(queryHandle);
  m_glQueries.clear();
}

void IntelGlCounters::BeginPass(uint32_t passID)
{
  m_passIndex = passID;
}

void IntelGlCounters::EndPass()
{
  // Flush all of the pass' queries to ensure we can begin further samples
  // with a different pass.
  std::vector<uint8_t> data(m_Queries[m_EnabledQueries[m_passIndex]].size);
  GLuint len;
  uint32_t nSamples = m_glQueries.size() / (m_passIndex + 1);

  for(uint32_t q = nSamples * m_passIndex; q < m_glQueries.size(); q++)
  {
    GL.glGetPerfQueryDataINTEL(m_glQueries[q], GL_PERFQUERY_WAIT_INTEL, data.size(), &data[0], &len);
  }
}

void IntelGlCounters::BeginSample(uint32_t sampleID)
{
  GLuint queryId = m_EnabledQueries[m_passIndex];
  GLuint queryHandle = 0;

  GL.glCreatePerfQueryINTEL(queryId, &queryHandle);
  m_glQueries.push_back(queryHandle);

  GLenum err = GL.glGetError();
  if(err != eGL_NONE)
    return;

  GL.glBeginPerfQueryINTEL(m_glQueries.back());
}

void IntelGlCounters::EndSample()
{
  GLuint queryHandle = m_glQueries.back();

  if(queryHandle == 0)
    return;

  GL.glEndPerfQueryINTEL(queryHandle);
}

uint32_t IntelGlCounters::CounterPass(const IntelGlCounter &counter)
{
  for(uint32_t p = 0; p < m_EnabledQueries.size(); p++)
    if(m_EnabledQueries[p] == counter.queryId)
      return p;

  RDCASSERT(false && "Counter not enabled");
  return 0;
}

void IntelGlCounters::CopyData(void *dest, const IntelGlCounter &counter, uint32_t sample,
                               uint32_t maxSampleIndex)
{
  uint32_t pass = CounterPass(counter);
  uint32_t queryHandle = m_glQueries[maxSampleIndex * pass + sample];

  std::vector<uint8_t> data(m_Queries[m_EnabledQueries[pass]].size);
  GLuint len;
  GL.glGetPerfQueryDataINTEL(queryHandle, 0, data.size(), &data[0], &len);

  memcpy(dest, &data[counter.offset], counter.desc.resultByteWidth);
}

std::vector<CounterResult> IntelGlCounters::GetCounterData(uint32_t maxSampleIndex,
                                                           const std::vector<uint32_t> &eventIDs,
                                                           const std::vector<GPUCounter> &counters)
{
  std::vector<CounterResult> ret;

  RDCASSERT((maxSampleIndex * m_EnabledQueries.size()) == m_glQueries.size());

  for(uint32_t s = 0; s < maxSampleIndex; s++)
  {
    for(const auto &c : counters)
    {
      const auto &counter = m_Counters[GPUCounterToCounterIndex(c)];
      switch(counter.desc.resultType)
      {
        case CompType::Double:
        {
          double r;
          CopyData(&r, counter, s, maxSampleIndex);
          ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, r));
          break;
        }
        case CompType::Float:
        {
          float r;
          CopyData(&r, counter, s, maxSampleIndex);
          ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, r));
          break;
        }
        case CompType::UInt:
        {
          uint64_t r;
          CopyData(&r, counter, s, maxSampleIndex);
          ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, r));
          break;
        }
        default:
          RDCASSERT(false && "Wrong type");    // unreachable
      }
    }
  }

  return ret;
}
