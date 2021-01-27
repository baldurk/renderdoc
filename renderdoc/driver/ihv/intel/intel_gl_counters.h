/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#pragma once

#include <map>

#include "api/replay/rdcarray.h"
#include "api/replay/replay_enums.h"

typedef unsigned int GLuint;

inline constexpr GPUCounter MakeIntelGlCounter(int index)
{
  return GPUCounter((int)GPUCounter::FirstIntel + index);
}

class IntelGlCounters
{
public:
  IntelGlCounters();

  bool Init();
  ~IntelGlCounters();

  rdcarray<GPUCounter> GetPublicCounterIds() const;
  CounterDescription GetCounterDescription(GPUCounter index) const;

  void EnableCounter(GPUCounter index);
  void DisableAllCounters();

  uint32_t GetPassCount();

  void BeginSession();
  void EndSession();

  void BeginPass(uint32_t passID);
  void EndPass();

  void BeginSample(uint32_t sampleID);
  void EndSample();

  rdcarray<CounterResult> GetCounterData(uint32_t maxSampleIndex, const rdcarray<uint32_t> &eventIDs,
                                         const rdcarray<GPUCounter> &counters);

private:
  static uint32_t GPUCounterToCounterIndex(GPUCounter counter)
  {
    return (uint32_t)(counter) - (uint32_t)(GPUCounter::FirstIntel);
  }

  struct IntelGlCounter
  {
    CounterDescription desc;
    GLuint queryId = 0;
    GLuint offset = 0;
    GLuint type = 0;
    GLuint dataType = 0;
    CompType originalType = CompType::Typeless;
    uint32_t originalByteWidth = 0;
  };
  rdcarray<IntelGlCounter> m_Counters;

  bool m_Paranoid = false;

  struct IntelGlQuery
  {
    GLuint queryId = 0;
    rdcstr name;
    GLuint size = 0;
  };
  std::map<GLuint, IntelGlQuery> m_Queries;

  void addCounter(const IntelGlQuery &query, GLuint counterId);
  void addQuery(GLuint queryId);
  uint32_t CounterPass(const IntelGlCounter &counter);
  void CopyData(void *dest, const IntelGlCounter &counter, uint32_t sample, uint32_t maxSampleIndex);

  rdcarray<uint32_t> m_EnabledQueries;

  uint32_t m_passIndex;

  rdcarray<GLuint> m_glQueries;
};
