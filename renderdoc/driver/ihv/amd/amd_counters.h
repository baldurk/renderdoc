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

#pragma once

#include <vector>
#include "api/replay/renderdoc_replay.h"

class GPUPerfAPI;

inline constexpr GPUCounter MakeAMDCounter(int index)
{
  return GPUCounter((int)GPUCounter::FirstAMD + index);
}

class AMDCounters
{
public:
  AMDCounters();
  bool Init(void *pContext);
  ~AMDCounters();

  uint32_t GetNumCounters();

  CounterDescription GetCounterDescription(GPUCounter index);

  void EnableCounter(GPUCounter index);
  void EnableAllCounters();
  void DisableAllCounters();

  uint32_t GetPassCount();

  uint32_t BeginSession();
  void EndSesssion();

  void BeginPass();
  void EndPass();

  void BeginSample(uint32_t index);
  void EndSample();

  bool IsSessionReady(uint32_t sessionIndex);

  uint32_t GetSampleUint32(uint32_t session, uint32_t sample, GPUCounter counter);

  uint64_t GetSampleUint64(uint32_t session, uint32_t sample, GPUCounter counter);

  float GetSampleFloat32(uint32_t session, uint32_t sample, GPUCounter counter);

  double GetSampleFloat64(uint32_t session, uint32_t sample, GPUCounter counter);

private:
  GPUPerfAPI *m_pGPUPerfAPI;
  static uint32_t GPUCounterToCounterIndex(GPUCounter counter)
  {
    return (uint32_t)(counter) - (uint32_t)(GPUCounter::FirstAMD);
  }

  CounterDescription InternalGetCounterDescription(uint32_t index);

  struct InternalCounterDescription
  {
    CounterDescription desc;
    uint32_t internalIndex;
  };

  std::vector<InternalCounterDescription> EnumerateCounters();
  std::vector<InternalCounterDescription> m_Counters;
};