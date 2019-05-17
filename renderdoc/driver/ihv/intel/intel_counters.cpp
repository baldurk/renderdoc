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

#include "common/common.h"

#include "official/DriverStorePath.h"
#include "intel_counters.h"

#include <set>

const static std::vector<std::string> metricSetBlacklist = {"TestOa"};

HMODULE IntelCounters::m_MDLibraryHandle = 0;
IMetricsDevice_1_5 *IntelCounters::m_metricsDevice = NULL;
OpenMetricsDevice_fn IntelCounters::OpenMetricsDevice = NULL;
CloseMetricsDevice_fn IntelCounters::CloseMetricsDevice = NULL;

IntelCounters::IntelCounters()
    : m_device(NULL), m_deviceContext(NULL), m_counter(NULL), m_passIndex(0), m_sampleIndex(0)
{
}

IntelCounters::~IntelCounters()
{
  SAFE_RELEASE(m_deviceContext);

  if(CloseMetricsDevice && m_metricsDevice)
    CloseMetricsDevice(m_metricsDevice);

  m_metricsDevice = NULL;

  if(m_MDLibraryHandle)
    FreeLibrary(m_MDLibraryHandle);

  m_MDLibraryHandle = (HMODULE)0;
  OpenMetricsDevice = NULL;
  CloseMetricsDevice = NULL;
}

void IntelCounters::Load()
{
  if(m_metricsDevice)
    return;

  m_MDLibraryHandle = LoadDynamicLibrary(L"igdmd64.dll", NULL, 0);
  if(!m_MDLibraryHandle)
    return;

  OpenMetricsDevice = (OpenMetricsDevice_fn)GetProcAddress(m_MDLibraryHandle, "OpenMetricsDevice");
  if(!OpenMetricsDevice)
    return;

  CloseMetricsDevice =
      (CloseMetricsDevice_fn)GetProcAddress(m_MDLibraryHandle, "CloseMetricsDevice");
  if(!CloseMetricsDevice)
    return;

  OpenMetricsDevice(&m_metricsDevice);
}

bool IntelCounters::Init(void *pContext)
{
  if(!pContext)
    return false;

  if(!m_metricsDevice)
    return false;

  m_device = (ID3D11Device *)pContext;
  m_device->GetImmediateContext(&m_deviceContext);
  if(!m_deviceContext)
    return false;

  TMetricsDeviceParams_1_2 *deviceParams = m_metricsDevice->GetParams();
  if(deviceParams->Version.MajorNumber < 1 ||
     (deviceParams->Version.MajorNumber == 1 && deviceParams->Version.MinorNumber < 1))
  {
    CloseMetricsDevice(m_metricsDevice);
    FreeLibrary(m_MDLibraryHandle);

    m_metricsDevice = NULL;
    m_MDLibraryHandle = (HMODULE)0;
    OpenMetricsDevice = NULL;
    CloseMetricsDevice = NULL;
    return false;
  }

  m_Counters = EnumerateCounters();

  return true;
}

CounterDescription IntelCounters::GetCounterDescription(GPUCounter counter)
{
  return m_Counters[GPUCounterToCounterIndex(counter)];
}

std::vector<CounterDescription> IntelCounters::EnumerateCounters()
{
  m_Counters.clear();
  m_allMetricSets.clear();
  std::set<std::string> addedMetrics;
  TMetricsDeviceParams_1_2 *deviceParams = m_metricsDevice->GetParams();
  for(unsigned int i = 0; i < deviceParams->ConcurrentGroupsCount; ++i)
  {
    IConcurrentGroup_1_1 *concurrentGroup = m_metricsDevice->GetConcurrentGroup(i);
    TConcurrentGroupParams_1_0 *groupParams = concurrentGroup->GetParams();
    if(strcmp(groupParams->SymbolName, "OA") != 0)
      continue;

    m_subscribedMetricsByCounterSet.resize(groupParams->MetricSetsCount);

    for(unsigned int j = 0; j < groupParams->MetricSetsCount; ++j)
    {
      IMetricSet_1_1 *metricSet = concurrentGroup->GetMetricSet(j);
      metricSet->SetApiFiltering(API_TYPE_DX11);
      m_allMetricSets.push_back(metricSet);
      TMetricSetParams_1_0 *setParams = metricSet->GetParams();

      if(std::find(metricSetBlacklist.begin(), metricSetBlacklist.end(), setParams->SymbolName) !=
         metricSetBlacklist.end())
        continue;

      for(unsigned int k = 0; k < setParams->MetricsCount; ++k)
      {
        IMetric_1_0 *metric = metricSet->GetMetric(k);
        TMetricParams_1_0 *metricParams = metric->GetParams();

        if((metricParams->UsageFlagsMask & EMetricUsageFlag::USAGE_FLAG_OVERVIEW) == 0)
          continue;

        if(metricParams->ResultType == TMetricResultType::RESULT_BOOL)
          continue;

        if(addedMetrics.count(metricParams->ShortName) > 0)
          continue;

        CounterDescription counterDesc;
        counterDesc.name = metricParams->ShortName;
        counterDesc.description = metricParams->LongName;
        counterDesc.category = metricParams->GroupName;
        counterDesc.resultByteWidth = 8;
        counterDesc.resultType = CompType::Double;
        switch(metricParams->ResultType)
        {
          case RESULT_UINT32:
            counterDesc.resultType = CompType::UInt;
            counterDesc.resultByteWidth = sizeof(uint32_t);
            break;
          case RESULT_UINT64:
            counterDesc.resultType = CompType::UInt;
            counterDesc.resultByteWidth = sizeof(uint64_t);
            break;
          case RESULT_FLOAT:
            counterDesc.resultType = CompType::Float;
            counterDesc.resultByteWidth = sizeof(float);
            break;
          default: break;
        }

        counterDesc.unit = CounterUnit::Absolute;
        if(strcmp(metricParams->MetricResultUnits, "cycles") == 0)
          counterDesc.unit = CounterUnit::Cycles;
        else if(strcmp(metricParams->MetricResultUnits, "bytes") == 0)
          counterDesc.unit = CounterUnit::Bytes;
        else if(strcmp(metricParams->MetricResultUnits, "percent") == 0)
          counterDesc.unit = CounterUnit::Percentage;
        else if(strcmp(metricParams->MetricResultUnits, "ns") == 0)
        {
          counterDesc.unit = CounterUnit::Seconds;
          counterDesc.resultType = CompType::Float;
          counterDesc.resultByteWidth = sizeof(float);
        }

        uint32_t counterId = (uint32_t)GPUCounter::FirstIntel + (uint32_t)m_Counters.size();
        counterDesc.counter = (GPUCounter)counterId;
        m_Counters.push_back(counterDesc);
        addedMetrics.insert(counterDesc.name);
        m_metricLocation[(GPUCounter)counterId] = rdcpair<uint32_t, uint32_t>(j, k);
        m_counterIds.push_back((GPUCounter)counterId);
      }
    }
  }

  return m_Counters;
}

uint32_t IntelCounters::GetPassCount()
{
  return (uint32_t)m_subscribedMetricSets.size();
}

uint32_t IntelCounters::BeginSession()
{
  if(!m_metricsDevice)
    return 0;

  m_passIndex = 0;
  TMetricsDeviceParams_1_2 *deviceParams = m_metricsDevice->GetParams();
  if(deviceParams->Version.MajorNumber < 1 ||
     (deviceParams->Version.MajorNumber == 1 && deviceParams->Version.MinorNumber < 2))
    return 0;

  IOverride_1_2 *frequencyOverride = m_metricsDevice->GetOverrideByName("FrequencyOverride");
  if(!frequencyOverride)
    return 0;

  TTypedValue_1_0 *maxFreqSymbol =
      m_metricsDevice->GetGlobalSymbolValueByName("GpuMaxFrequencyMHz");
  if(!maxFreqSymbol)
    return 0;

  TSetFrequencyOverrideParams_1_2 params;
  params.Enable = true;
  params.FrequencyMhz = maxFreqSymbol->ValueUInt32;
  params.Pid = 0;
  frequencyOverride->SetOverride(&params, sizeof(params));
  Sleep(500);

  return 0;
}

void IntelCounters::EndSession()
{
  if(!m_metricsDevice)
    return;

  TMetricsDeviceParams_1_2 *deviceParams = m_metricsDevice->GetParams();
  if(deviceParams->Version.MajorNumber > 1 ||
     (deviceParams->Version.MajorNumber == 1 && deviceParams->Version.MinorNumber >= 2))
  {
    IOverride_1_2 *frequencyOverride = m_metricsDevice->GetOverrideByName("FrequencyOverride");

    if(frequencyOverride)
    {
      TSetFrequencyOverrideParams_1_2 params;
      params.Enable = false;
      params.Pid = 0;
      frequencyOverride->SetOverride(&params, sizeof(params));
    }
  }
}

void IntelCounters::BeginPass()
{
  m_sampleIndex = 0;
  TMetricSetParams_1_0 *metricSetParams = m_subscribedMetricSets[m_passIndex]->GetParams();
  m_queryResult.resize(metricSetParams->MetricsCount + metricSetParams->InformationCount);
}

void IntelCounters::EndPass()
{
  m_passIndex++;
}

void IntelCounters::EnableCounter(GPUCounter index)
{
  uint32_t metricSetIdx = m_metricLocation[index].first;
  IMetricSet_1_1 *metricSet = m_allMetricSets[metricSetIdx];
  auto iter = std::find(m_subscribedMetricSets.begin(), m_subscribedMetricSets.end(), metricSet);
  if(iter == m_subscribedMetricSets.end())
  {
    m_subscribedMetricSets.push_back(metricSet);
    metricSetIdx = (uint32_t)m_subscribedMetricSets.size() - 1;
  }
  else
  {
    metricSetIdx = (uint32_t)(iter - m_subscribedMetricSets.begin());
  }
  m_subscribedMetricsByCounterSet[metricSetIdx].push_back(index);
}

void IntelCounters::DisableAllCounters()
{
  m_subscribedMetricSets.clear();
  for(size_t i = 0; i < m_subscribedMetricsByCounterSet.size(); i++)
    m_subscribedMetricsByCounterSet[i].clear();
}

void IntelCounters::BeginSample()
{
  if(!m_metricsDevice)
    return;

  D3D11_COUNTER_DESC counter_desc;
  counter_desc.MiscFlags = 0;
  counter_desc.Counter =
      (D3D11_COUNTER)m_subscribedMetricSets[m_passIndex]->GetParams()->ApiSpecificId.D3D1XDevDependentId;

  if(counter_desc.Counter == 0)
    return;

  TCompletionCode res = m_subscribedMetricSets[m_passIndex]->Activate();
  if(res != TCompletionCode::CC_OK)
    return;

  HRESULT hr = m_device->CreateCounter(&counter_desc, &m_counter);
  if(FAILED(hr))
  {
    m_subscribedMetricSets[m_passIndex]->Deactivate();
    return;
  }

  res = m_subscribedMetricSets[m_passIndex]->Deactivate();
  if(res != TCompletionCode::CC_OK)
    return;

  m_deviceContext->Begin(m_counter);
}

void IntelCounters::EndSample()
{
  if(!m_metricsDevice)
    return;

  m_deviceContext->End(m_counter);
  HRESULT hr = S_OK;
  uint32_t iteration = 0;
  const uint32_t max_attempts = 0xFFFF;
  void *counter_data = NULL;

  do
  {
    uint32_t flags = (iteration == 0) ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH;
    hr = m_deviceContext->GetData(m_counter, &counter_data, m_counter->GetDataSize(), flags);
    iteration++;
  } while(hr != S_OK && iteration < max_attempts);
  m_counter->Release();

  if(hr != S_OK)
    return;

  IMetricSet_1_1 *metricSet = m_subscribedMetricSets[m_passIndex];
  TMetricSetParams_1_0 *metricSetParams = metricSet->GetParams();

  uint32_t calculatedReportCount = 0;
  TCompletionCode res = m_subscribedMetricSets[m_passIndex]->CalculateMetrics(
      (const unsigned char *)counter_data, metricSetParams->QueryReportSize, m_queryResult.data(),
      (uint32_t)m_queryResult.size() * sizeof(TTypedValue_1_0), &calculatedReportCount, false);

  if(res != TCompletionCode::CC_OK)
    return;

  for(size_t i = 0; i < m_subscribedMetricsByCounterSet[m_passIndex].size(); i++)
  {
    GPUCounter counterId = m_subscribedMetricsByCounterSet[m_passIndex][i];
    uint32_t counterIndex = m_metricLocation[counterId].second;

    m_results[rdcpair<GPUCounter, uint32_t>(counterId, m_sampleIndex)] = m_queryResult[counterIndex];
  }
  m_sampleIndex++;
}

std::vector<CounterResult> IntelCounters::GetCounterData(const std::vector<uint32_t> &eventIDs,
                                                         const std::vector<GPUCounter> &counters)
{
  std::vector<CounterResult> ret;
  for(uint32_t sample = 0; sample < (uint32_t)eventIDs.size(); sample++)
  {
    for(size_t counter = 0; counter < counters.size(); counter++)
    {
      const CounterDescription desc = GetCounterDescription(counters[counter]);

      switch(desc.resultType)
      {
        case CompType::UInt:
        {
          if(desc.resultByteWidth == sizeof(uint32_t))
          {
            uint32_t value =
                m_results[rdcpair<GPUCounter, uint32_t>(desc.counter, sample)].ValueUInt32;
            if(desc.unit == CounterUnit::Percentage)
            {
              value = RDCCLAMP(value, 0U, 100U);
            }

            ret.push_back(CounterResult(eventIDs[sample], counters[counter], value));
          }
          else if(desc.resultByteWidth == sizeof(uint64_t))
          {
            uint64_t value =
                m_results[rdcpair<GPUCounter, uint32_t>(desc.counter, sample)].ValueUInt64;

            if(desc.unit == CounterUnit::Percentage)
            {
              value = RDCCLAMP(value, 0ULL, 100ULL);
            }

            ret.push_back(CounterResult(eventIDs[sample], counters[counter], value));
          }
          else
          {
            RDCERR("Unexpected byte width %u", desc.resultByteWidth);
          }
        }
        break;
        case CompType::Float:
        {
          float value = m_results[rdcpair<GPUCounter, uint32_t>(desc.counter, sample)].ValueFloat;
          if(desc.unit == CounterUnit::Seconds)
          {
            float nanoseconds =
                (float)m_results[rdcpair<GPUCounter, uint32_t>(desc.counter, sample)].ValueUInt64;
            value = nanoseconds / 1e9f;
          }

          if(fabs(value) < 1e-9)
          {
            value = 0.0f;
          }

          if(desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, 0.0f, 100.0f);
          }

          ret.push_back(CounterResult(eventIDs[sample], counters[counter], value));
        }
        break;
        default: RDCASSERT(0); break;
      };
    }
  }

  return ret;
}
