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

#pragma once

#include <map>
#include "api/replay/data_types.h"
#include "api/replay/rdcarray.h"
#include "api/replay/replay_enums.h"
#include "driver/vulkan/official/vulkan.h"
#include "official/GPUPerfAPI/Include/gpu_perf_api.h"

inline constexpr GPUCounter MakeAMDCounter(int index)
{
  return GPUCounter((int)GPUCounter::FirstAMD + index);
}
class AMDCounters
{
public:
  enum class ApiType : uint32_t
  {
    Dx11 = 0,
    Dx12 = 1,
    Ogl = 2,
    Vk = 3
  };

  AMDCounters(bool dx12DebugLayerEnabled = false);
  ~AMDCounters();

  bool Init(ApiType apiType, void *pContext);
  rdcarray<GPUCounter> GetPublicCounterIds() const;

  CounterDescription GetCounterDescription(GPUCounter counter);

  void EnableCounter(GPUCounter counter);
  void EnableAllCounters();
  void DisableAllCounters();

  bool BeginMeasurementMode(ApiType apiType, void *pContext);
  void EndMeasurementMode();

  uint32_t GetPassCount();

  uint32_t CreateSession();

  void BeginSession(uint32_t sessionId);
  void EndSesssion(uint32_t sessionId);

  void BeginPass();
  void EndPass();

  void BeginCommandList(void *pCommandList = NULL);
  void EndCommandList(void *pCommandList = NULL);

  void BeginSample(uint32_t sampleID, void *pCommandList = NULL);
  void EndSample(void *pCommandList = NULL);

  // Session data retrieval
  rdcarray<CounterResult> GetCounterData(uint32_t sessionID, uint32_t maxSampleIndex,
                                         const rdcarray<uint32_t> &eventIDs,
                                         const rdcarray<GPUCounter> &counters);

private:
  bool IsSessionReady(uint32_t sessionIndex);

  GpaFunctionTable *m_pGPUPerfAPI;
  GpaContextId m_gpaContextId;

  union GPACmdListInfo
  {
    GpaCommandListId m_gpaCommandListId;
    std::map<void *, GpaCommandListId> *m_pCommandListMap;

    GPACmdListInfo() : m_pCommandListMap(NULL) {}
    ~GPACmdListInfo() {}
  };

  GPACmdListInfo m_gpaCmdListInfo;
  rdcarray<GpaSessionId> m_gpaSessionInfo;

  ApiType m_apiType;
  uint32_t m_gpaSessionCounter;
  int m_passCounter;
  bool m_dx12DebugLayerEnabled;

  void InitializeCmdInfo();
  void DeInitializeCmdInfo();
  void DeleteSession(uint32_t sessionId);
  CounterDescription InternalGetCounterDescription(uint32_t internalIndex);

  std::map<uint32_t, CounterDescription> EnumerateCounters();
  std::map<uint32_t, CounterDescription> m_Counters;
  std::map<GPUCounter, uint32_t> m_PublicToInternalCounter;
};
