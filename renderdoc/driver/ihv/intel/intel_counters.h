/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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

#include <d3d11.h>
#include <windows.h>
#include <map>
#include <vector>

#include "api/replay/renderdoc_replay.h"
#include "official/metrics_discovery_api.h"

using namespace MetricsDiscovery;

inline constexpr GPUCounter MakeIntelCounter(int index)
{
  return GPUCounter((int)GPUCounter::FirstIntel + index);
}

class IntelCounters
{
public:
  IntelCounters();

  static void Load();

  bool Init(void *pDevice);
  ~IntelCounters();

  std::vector<GPUCounter> GetPublicCounterIds() const { return m_counterIds; }
  CounterDescription GetCounterDescription(GPUCounter index);

  void EnableCounter(GPUCounter index);
  void DisableAllCounters();

  uint32_t GetPassCount();

  uint32_t BeginSession();
  void EndSession();

  void BeginPass();
  void EndPass();

  void BeginSample();
  void EndSample();

  std::vector<CounterResult> GetCounterData(const std::vector<uint32_t> &eventIDs,
                                            const std::vector<GPUCounter> &counters);

private:
  static uint32_t GPUCounterToCounterIndex(GPUCounter counter)
  {
    return (uint32_t)(counter) - (uint32_t)(GPUCounter::FirstIntel);
  }

  std::vector<CounterDescription> EnumerateCounters();
  std::vector<GPUCounter> m_counterIds;
  std::vector<CounterDescription> m_Counters;
  std::vector<IMetricSet_1_1 *> m_allMetricSets;
  std::vector<IMetricSet_1_1 *> m_subscribedMetricSets;
  std::map<GPUCounter, rdcpair<uint32_t, uint32_t>> m_metricLocation;
  std::vector<std::vector<GPUCounter>> m_subscribedMetricsByCounterSet;
  ID3D11Device *m_device;
  ID3D11DeviceContext *m_deviceContext;
  ID3D11Counter *m_counter;
  std::vector<TTypedValue_1_0> m_queryResult;
  uint32_t m_passIndex;
  uint32_t m_sampleIndex;
  std::map<rdcpair<GPUCounter, uint32_t>, TTypedValue_1_0> m_results;

  static HMODULE m_MDLibraryHandle;
  static IMetricsDevice_1_5 *m_metricsDevice;
  static OpenMetricsDevice_fn OpenMetricsDevice;
  static CloseMetricsDevice_fn CloseMetricsDevice;
};
