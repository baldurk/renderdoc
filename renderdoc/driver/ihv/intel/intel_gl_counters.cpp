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

#include "common/common.h"
#include "driver/gl/gl_dispatch_table.h"
#include "strings/string_utils.h"

#include "intel_gl_counters.h"

const static rdcarray<rdcstr> metricSetBlacklist = {
    // Used for testing HW is programmed correctly.
    "TestOa",
    // Used to plumb raw data from the GL driver to metrics-discovery.
    "Intel_Raw_Hardware_Counters_Set_0_Query", "Intel_Raw_Pipeline_Statistics_Query"};

IntelGlCounters::IntelGlCounters() : m_passIndex(0)
{
}

IntelGlCounters::~IntelGlCounters()
{
}

rdcarray<GPUCounter> IntelGlCounters::GetPublicCounterIds() const
{
  rdcarray<GPUCounter> counters;

  for(const IntelGlCounter &c : m_Counters)
    counters.push_back(c.desc.counter);

  if(m_Paranoid)
    counters.push_back(GPUCounter((int)GPUCounter::FirstIntel + m_Counters.size()));

  return counters;
}

CounterDescription IntelGlCounters::GetCounterDescription(GPUCounter index) const
{
  uint32_t idx = GPUCounterToCounterIndex(index);
  if(idx >= m_Counters.size())
  {
    CounterDescription desc;

    desc.counter = index;
    desc.name = "Counters limited, see description";
    desc.category = "More counters are available";
    desc.description =
        "Not all counters available, run 'sudo sysctl dev.i915.perf_stream_paranoid=0' or "
        "'sudo sysctl dev.xe.perf_stream_paranoid=0' to enable more counters!";

    desc.resultType = CompType::UInt;
    desc.resultByteWidth = 8;
    desc.unit = CounterUnit::Absolute;
    desc.uuid = Uuid(0x8086, 0x1234, 0x5678, 0xABCD);

    return desc;
  }

  return m_Counters[GPUCounterToCounterIndex(index)].desc;
}

static CompType glToRdcCounterType(GLuint glDataType)
{
  switch(glDataType)
  {
    case GL_PERFQUERY_COUNTER_DATA_UINT32_INTEL: return CompType::UInt;
    case GL_PERFQUERY_COUNTER_DATA_UINT64_INTEL: return CompType::UInt;
    case GL_PERFQUERY_COUNTER_DATA_FLOAT_INTEL: return CompType::Float;
    case GL_PERFQUERY_COUNTER_DATA_DOUBLE_INTEL: return CompType::Float;
    case GL_PERFQUERY_COUNTER_DATA_BOOL32_INTEL: return CompType::UInt;
    default: RDCERR("Wrong counter data type: %u", glDataType);
  }
  return CompType::Typeless;
}

void IntelGlCounters::addCounter(const IntelGlQuery &query, GLuint counterId)
{
  IntelGlCounter counter;

  counter.queryId = query.queryId;
  counter.desc.counter = (GPUCounter)((int)GPUCounter::FirstIntel + m_Counters.size());
  counter.desc.category = query.name;

  GLint len = 0;
  GL.glGetIntegerv(eGL_PERFQUERY_COUNTER_NAME_LENGTH_MAX_INTEL, &len);
  counter.desc.name.resize(len);
  GL.glGetIntegerv(eGL_PERFQUERY_COUNTER_DESC_LENGTH_MAX_INTEL, &len);
  counter.desc.description.resize(len);

  GLuint64 rawCounterMaxValue = 0;
  GL.glGetPerfCounterInfoINTEL(
      query.queryId, counterId, (GLuint)counter.desc.name.size(), &counter.desc.name[0],
      (GLuint)counter.desc.description.size(), &counter.desc.description[0], &counter.offset,
      &counter.desc.resultByteWidth, &counter.type, &counter.dataType, &rawCounterMaxValue);

  counter.desc.name.resize(strlen(&counter.desc.name[0]));
  counter.desc.description.resize(strlen(&counter.desc.description[0]));

  uint32_t query_hash = strhash(query.name.c_str());
  uint32_t name_hash = strhash(counter.desc.name.c_str());
  uint32_t desc_hash = strhash(counter.desc.description.c_str());
  counter.desc.uuid = Uuid(0x8086, query_hash, name_hash, desc_hash);
  counter.desc.resultType = glToRdcCounterType(counter.dataType);
  counter.desc.unit = CounterUnit::Absolute;

  if(counter.desc.description.contains("Unit: cycles."))
  {
    counter.desc.unit = CounterUnit::Cycles;
  }
  else if(counter.desc.description.contains("Unit: bytes."))
  {
    counter.desc.unit = CounterUnit::Bytes;
  }
  else if(counter.desc.description.contains("Unit: percent."))
  {
    counter.desc.unit = CounterUnit::Percentage;
  }
  else if(counter.desc.description.contains("Unit: Hz."))
  {
    counter.desc.unit = CounterUnit::Hertz;
  }
  else if(counter.desc.description.contains("Unit: ns."))
  {
    counter.desc.unit = CounterUnit::Seconds;

    counter.originalType = counter.desc.resultType;
    counter.originalByteWidth = counter.desc.resultByteWidth;

    counter.desc.resultType = CompType::Float;
    counter.desc.resultByteWidth = sizeof(double);
  }

  m_Counters.push_back(counter);
}

void IntelGlCounters::addQuery(GLuint queryId)
{
  IntelGlQuery query;

  query.queryId = queryId;

  GLint len = 0;
  GL.glGetIntegerv(eGL_PERFQUERY_QUERY_NAME_LENGTH_MAX_INTEL, &len);
  query.name.resize(len);
  GLuint nCounters = 0;
  GLuint nInstances = 0;
  GLuint capsMask = 0;
  GL.glGetPerfQueryInfoINTEL(queryId, (GLuint)query.name.size(), &query.name[0], &query.size,
                             &nCounters, &nInstances, &capsMask);
  // Some drivers raise an error when we query some of its IDs because those
  // are used to plumb external library with raw counter data.
  if(GL.glGetError() != eGL_NONE)
    return;

  query.name.resize(strlen(&query.name[0]));
  if(metricSetBlacklist.contains(query.name))
    return;

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

  m_Paranoid = false;

#if defined(RENDERDOC_PLATFORM_ANDROID) || defined(RENDERDOC_PLATFORM_LINUX)
  rdcstr i915_contents, xe_contents;

  FileIO::ReadAll("/proc/sys/dev/i915/perf_stream_paranoid", i915_contents);
  i915_contents.trim();

  FileIO::ReadAll("/proc/sys/dev/xe/perf_stream_paranoid", xe_contents);
  xe_contents.trim();

  if(!i915_contents.empty())
  {
    if(atoi(i915_contents.c_str()))
    {
      RDCWARN(
          "Not all counters available, run "
          "'sudo sysctl dev.i915.perf_stream_paranoid=0' to enable more counters!");
      m_Paranoid = true;
    }
  }

  if(!xe_contents.empty())
  {
    if(atoi(xe_contents.c_str()))
    {
      RDCWARN(
          "Not all counters available, run "
          "'sudo sysctl dev.xe.perf_stream_paranoid=0' to enable more counters!");
      m_Paranoid = true;
    }
  }
#endif

  do
  {
    addQuery(queryId);

    GL.glGetNextPerfQueryIdINTEL(queryId, &queryId);
  } while(queryId != 0);

  return true;
}

void IntelGlCounters::EnableCounter(GPUCounter index)
{
  uint32_t idx = GPUCounterToCounterIndex(index);
  if(idx >= m_Counters.size())
    return;
  const IntelGlCounter &counter = m_Counters[idx];

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
  return (uint32_t)m_EnabledQueries.size();
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
  rdcarray<uint8_t> data;
  data.resize(m_Queries[m_EnabledQueries[m_passIndex]].size);
  GLuint len;
  uint32_t nSamples = (uint32_t)m_glQueries.size() / (m_passIndex + 1);

  for(uint32_t q = nSamples * m_passIndex; q < m_glQueries.size(); q++)
  {
    GL.glGetPerfQueryDataINTEL(m_glQueries[q], GL_PERFQUERY_WAIT_INTEL, (GLsizei)data.size(),
                               &data[0], &len);
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

  RDCERR("Counters not enabled");
  return 0;
}

void IntelGlCounters::CopyData(void *dest, const IntelGlCounter &counter, uint32_t sample,
                               uint32_t maxSampleIndex)
{
  uint32_t pass = CounterPass(counter);
  uint32_t queryHandle = m_glQueries[maxSampleIndex * pass + sample];

  rdcarray<uint8_t> data;
  data.resize(m_Queries[m_EnabledQueries[pass]].size);
  GLuint len;
  GL.glGetPerfQueryDataINTEL(queryHandle, 0, (GLsizei)data.size(), &data[0], &len);

  memcpy(dest, &data[counter.offset], counter.desc.resultByteWidth);
}

rdcarray<CounterResult> IntelGlCounters::GetCounterData(uint32_t maxSampleIndex,
                                                        const rdcarray<uint32_t> &eventIDs,
                                                        const rdcarray<GPUCounter> &counters)
{
  rdcarray<CounterResult> ret;

  RDCASSERT((maxSampleIndex * m_EnabledQueries.size()) == m_glQueries.size());

  for(uint32_t s = 0; s < maxSampleIndex; s++)
  {
    for(const GPUCounter &c : counters)
    {
      uint32_t idx = GPUCounterToCounterIndex(c);
      if(idx >= m_Counters.size())
      {
        ret.push_back(CounterResult(eventIDs[s], c, uint64_t(0)));
        continue;
      }

      const IntelGlCounter &counter = m_Counters[idx];

      if(counter.desc.unit == CounterUnit::Seconds)
      {
        double nanoseconds = 0.0;
        if(counter.originalType == CompType::UInt)
        {
          if(counter.desc.resultByteWidth == 8)
          {
            uint64_t r;
            CopyData(&r, counter, s, maxSampleIndex);
            nanoseconds = (double)r;
          }
          else
          {
            uint32_t r;
            CopyData(&r, counter, s, maxSampleIndex);
            nanoseconds = (double)r;
          }
        }
        else if(counter.originalType == CompType::Float)
        {
          if(counter.desc.resultByteWidth == 8)
          {
            double r;
            CopyData(&r, counter, s, maxSampleIndex);
            nanoseconds = r;
          }
          else
          {
            float r;
            CopyData(&r, counter, s, maxSampleIndex);
            nanoseconds = r;
          }
        }
        else
        {
          RDCERR("Wrong counter result type: %u", counter.originalType);
        }

        double seconds = nanoseconds / 1e9f;
        ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, seconds));

        continue;
      }

      switch(counter.desc.resultType)
      {
        case CompType::Float:
        {
          if(counter.desc.resultByteWidth == 8)
          {
            double r;
            CopyData(&r, counter, s, maxSampleIndex);
            ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, r));
            break;
          }
          else
          {
            float r;
            CopyData(&r, counter, s, maxSampleIndex);
            ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, r));
          }
          break;
        }
        case CompType::UInt:
        {
          uint64_t r;
          CopyData(&r, counter, s, maxSampleIndex);
          ret.push_back(CounterResult(eventIDs[s], counter.desc.counter, r));
          break;
        }
        default: RDCERR("Wrong counter result type: %u", counter.desc.resultType);
      }
    }
  }

  return ret;
}
