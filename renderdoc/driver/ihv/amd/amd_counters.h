/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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
#include <vector>
#include "api/replay/renderdoc_replay.h"

struct _GPAApi;

inline constexpr GPUCounter MakeAMDCounter(int index)
{
  return GPUCounter((int)GPUCounter::FirstAMD + index);
}
class AMDCounters
{
public:
  enum ApiType
  {
    eApiType_Dx11 = 0,
    eApiType_Dx12 = 1,
    eApiType_Ogl = 2,
    eApiType_Vk = 3
  };

  AMDCounters();
  bool Init(ApiType apiType, void *pContext);
  ~AMDCounters();

  uint32_t GetNumCounters();

  CounterDescription GetCounterDescription(GPUCounter index);

  void EnableCounter(GPUCounter index);
  void EnableAllCounters();
  void DisableAllCounters();

  uint32_t GetPassCount();

  uint32_t BeginSession();
  void EndSesssion();

  // DX11 and OGL entry points
  void BeginPass();
  void EndPass();

  void BeginSample(uint32_t index);
  void EndSample();

  // DX12 and VK entry points
  void BeginSampleList(void *pSampleList);

  void EndSampleList(void *pSampleList);

  void BeginSampleInSampleList(uint32_t sampleID, void *pSampleList);

  void EndSampleInSampleList(void *pSampleList);

  // Session data retrieval
  std::vector<CounterResult> GetCounterData(uint32_t sessionID, uint32_t maxSampleIndex,
                                            const std::vector<uint32_t> &eventIDs,
                                            const std::vector<GPUCounter> &counters);

private:
  _GPAApi *m_pGPUPerfAPI;
  std::string FormatErrMessage(const char *operation, uint32_t status);
  bool IsSessionReady(uint32_t sessionIndex);

  uint32_t GetSampleUint32(uint32_t session, uint32_t sample, GPUCounter counter);

  uint64_t GetSampleUint64(uint32_t session, uint32_t sample, GPUCounter counter);

  float GetSampleFloat32(uint32_t session, uint32_t sample, GPUCounter counter);

  double GetSampleFloat64(uint32_t session, uint32_t sample, GPUCounter counter);

  CounterDescription InternalGetCounterDescription(uint32_t index);

  std::map<uint32_t, CounterDescription> EnumerateCounters();
  std::map<uint32_t, CounterDescription> m_Counters;

  std::map<GPUCounter, uint32_t> m_PublicToInternalCounter;
};