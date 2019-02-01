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

#include "nv_counters.h"
#include "common/common.h"
#include "core/plugins.h"

#define NVPM_INITGUID
#include "official/PerfKit/include/NvPmApi.h"
#include "strings/string_utils.h"

struct EnumCountersCtx
{
  std::vector<GPUCounter> m_ExternalIds;
  std::vector<uint32_t> m_InternalIds;
  std::vector<CounterDescription> m_ExternalDescriptors;
  std::vector<uint32_t> m_InternalDescriptors;
  std::vector<char> m_TmpStr;

  NvPmApi *m_NvPmApi;

  uint32_t mCurrentCounterId;
};

static bool NvPmResultFails(NVPMRESULT actual, char const *failMsg)
{
  if(actual != NVPM_OK)
  {
    RDCWARN("NV GPU performance counters could not %s (code = %u)\n", failMsg, actual);
    return true;
  }
  return false;
}

int NvPmCountCounters(NVPMCounterID unCounterID, const char *pcCounterName, void *pUserData)
{
  uint32_t *pNumCounters = (uint32_t *)pUserData;
  *pNumCounters += 1;
  return NVPM_OK;
}

int NvPmGatherCounters(NVPMCounterID unCounterID, const char *pcCounterName, void *pUserData)
{
  EnumCountersCtx *pEnumCtx = (EnumCountersCtx *)pUserData;

  const uint32_t i = pEnumCtx->mCurrentCounterId;

  GPUCounter globalId = (GPUCounter)((uint32_t)GPUCounter::FirstNvidia + i);
  pEnumCtx->m_ExternalIds[i] = globalId;
  pEnumCtx->m_InternalIds[i] = unCounterID;

  CounterDescription &desc = pEnumCtx->m_ExternalDescriptors[i];

  NVPMUINT64 Attribute = 0;

  pEnumCtx->m_NvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_TYPE, &Attribute);
  NVPMCOUNTERTYPE Type = (NVPMCOUNTERTYPE)Attribute;

  switch(Type)
  {
    case NVPM_CT_GPU: desc.category = "GPU"; break;

    case NVPM_CT_OGL: desc.category = "D3D"; break;

    case NVPM_CT_D3D: desc.category = "D3D"; break;

    case NVPM_CT_SIMEXP: desc.category = "SIMEXP"; break;

    case NVPM_CT_AGGREGATE: desc.category = "AGGREGATE"; break;

    case NVPM_CT_USER: desc.category = "USER"; break;
  };

  pEnumCtx->m_NvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_DISPLAY, &Attribute);
  NVPMCOUNTERDISPLAY DisplayType = (NVPMCOUNTERDISPLAY)Attribute;

  pEnumCtx->m_NvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_DOMAIN, &Attribute);

  pEnumCtx->m_NvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_VALUE_TYPE, &Attribute);
  NVPMCOUNTERVALUETYPE ValueType = (NVPMCOUNTERVALUETYPE)Attribute;

  pEnumCtx->m_InternalDescriptors[i] = (DisplayType << 1) | ValueType;

  if(ValueType == NVPM_VALUE_TYPE_UINT64)
  {
    if(DisplayType == NVPM_CD_RATIO)
    {
      // Unfortunately, we can't classify exactly NV counters with display type `Ratio`
      // because sometimes they could be `percents` and sometimes 'ratios' (avg. instructions per
      // shader invocation, for example)
      desc.unit = CounterUnit::Ratio;
      desc.resultType = CompType::Double;
      desc.resultByteWidth = sizeof(double);
    }
    else
    {
      desc.unit = CounterUnit::Absolute;
      desc.resultType = CompType::UInt;
      desc.resultByteWidth = sizeof(uint64_t);
    }
  }
  else
  {
    if(DisplayType == NVPM_CD_RATIO)
    {
      RDCWARN(
          " normalization for counters with DisplayType == NVPM_CD_RATIO and ValueType == "
          "NVPM_VALUE_TYPE_FLOAT64 is unhandled");
    }
    // Same problem as for counters with 'ratio' display type but:
    // don't know for sure if a counter should be displayed as is or annotated with `%` symbol
    desc.unit = CounterUnit::Ratio;
    desc.resultType = CompType::Double;
    desc.resultByteWidth = sizeof(double);
  }

  char DummyChar = '\0';
  NVPMUINT DescriptionSize = 0;
  pEnumCtx->m_NvPmApi->GetCounterDescription(unCounterID, &DummyChar, &DescriptionSize);
  pEnumCtx->m_TmpStr.resize(DescriptionSize);
  pEnumCtx->m_NvPmApi->GetCounterDescription(unCounterID, pEnumCtx->m_TmpStr.data(),
                                             &DescriptionSize);

  desc.counter = globalId;
  desc.description = pEnumCtx->m_TmpStr.data();
  desc.name = pcCounterName;

  desc.uuid.words[0] = 0xC8958C90;
  desc.uuid.words[1] = 0xB7064F22;
  desc.uuid.words[2] = 0x8AF5E0A3 ^ strhash(desc.name.c_str());
  desc.uuid.words[3] = 0x831B2C39 ^ strhash(desc.description.c_str());

  pEnumCtx->mCurrentCounterId += 1;

  return NVPM_OK;
}

NVCounters::NVCounters()
    : m_NvPmLib(NULL), m_NvPmApi(NULL), m_NvPmCtx((uint64_t)-1), m_ObjectsCount(0)
{
}

NVCounters::~NVCounters()
{
  if(m_ObjectsCount != 0)
  {
    NvPmResultFails(m_NvPmApi->DeleteObjects(m_NvPmCtx), "call to 'NvPmApi::DeleteObjects'");
    m_ObjectsCount = 0;
  }

  if(m_NvPmCtx != (uint64_t)-1)
  {
    NvPmResultFails(m_NvPmApi->DestroyContext(m_NvPmCtx), "call to 'NvPmApi::DestroyContext'");
    m_NvPmCtx = (uint64_t)-1;
  }

  if(m_NvPmApi != NULL)
  {
    NvPmResultFails(m_NvPmApi->Shutdown(), "call to 'NvPmApi::Shutdown'");
    m_NvPmApi = NULL;
  }
  m_NvPmLib = NULL;
}

bool NVCounters::Init()
{
  if(m_NvPmLib != NULL)
  {
    return false;
  }
#if ENABLED(RDOC_WIN32)

#if ENABLED(RDOC_X64)
  std::string dllPath = LocatePluginFile("nv/counters/x64", "NvPmApi.Core.dll");
#else
  std::string dllPath = LocatePluginFile("nv/counters/x86", "NvPmApi.Core.dll");
#endif

#endif

  m_NvPmLib = Process::LoadModule(dllPath.c_str());
  if(m_NvPmLib == NULL)
  {
    RDCWARN("NV GPU performance counters could not locate 'NvPmApi.Core.dll'");
    return false;
  }

  NVPMGetExportTable_Pfn pfnGetExportTable =
      (NVPMGetExportTable_Pfn)Process::GetFunctionAddress(m_NvPmLib, "NVPMGetExportTable");
  if(pfnGetExportTable == NULL)
  {
    return false;
  }

  if(NvPmResultFails(pfnGetExportTable(&ETID_NvPmApi, (void **)&m_NvPmApi), "get 'NvPmApi' table"))
  {
    return false;
  }

  if(NvPmResultFails(m_NvPmApi->Init(), "init 'NvPmApi'"))
  {
    return false;
  }
  return true;
}

bool NVCounters::Init(ID3D11Device *pDevice)
{
  if(Init() == false)
  {
    return false;
  }

  if(NvPmResultFails(m_NvPmApi->CreateContextFromD3D11Device(pDevice, &m_NvPmCtx),
                     "init 'NVPMContext' from ID3D11Device"))
  {
    return false;
  }

  uint32_t NumCounters = 0;
  m_NvPmApi->EnumCountersByContextUserData(m_NvPmCtx, NvPmCountCounters, &NumCounters);

  EnumCountersCtx ctx;
  ctx.m_ExternalIds.resize(NumCounters);
  ctx.m_InternalIds.resize(NumCounters);
  ctx.m_ExternalDescriptors.resize(NumCounters);
  ctx.m_InternalDescriptors.resize(NumCounters);
  ctx.m_NvPmApi = m_NvPmApi;
  ctx.mCurrentCounterId = 0;

  m_NvPmApi->EnumCountersByContextUserData(m_NvPmCtx, NvPmGatherCounters, &ctx);

  ctx.m_ExternalIds.swap(m_ExternalIds);
  ctx.m_InternalIds.swap(m_InternalIds);
  ctx.m_ExternalDescriptors.swap(m_ExternalDescriptors);
  ctx.m_InternalDescriptors.swap(m_InternalDescriptors);

  m_SelectedExternalIds.reserve(NumCounters);
  m_SelectedInternalIds.reserve(NumCounters);

  return true;
}

bool NVCounters::PrepareExperiment(const std::vector<GPUCounter> &counters, uint32_t objectsCount)
{
  if(NvPmResultFails(m_NvPmApi->RemoveAllCounters(m_NvPmCtx),
                     "call to 'NvPmApi::RemoveAllCounters'"))
  {
    return false;
  }

  m_SelectedExternalIds.clear();
  m_SelectedInternalIds.clear();

  const size_t numCounters = counters.size();

  for(size_t i = 0; i < numCounters; ++i)
  {
    const uint32_t externalId = (uint64_t)counters[i] - (uint64_t)GPUCounter::FirstNvidia;
    m_SelectedExternalIds.push_back(counters[i]);
    m_SelectedInternalIds.push_back(m_InternalIds[externalId]);
  }

  if(NvPmResultFails(m_NvPmApi->AddCounters(m_NvPmCtx, (NVPMUINT)m_SelectedInternalIds.size(),
                                            m_SelectedInternalIds.data()),
                     "call to 'NvPmApi::AddCounters'"))
  {
    return false;
  }

  if(m_ObjectsCount != objectsCount)
  {
    if(m_ObjectsCount != 0)
    {
      NvPmResultFails(m_NvPmApi->DeleteObjects(m_NvPmCtx), "call to 'NvPmApi::DeleteObjects'");
      m_ObjectsCount = 0;
    }

    if(NvPmResultFails(m_NvPmApi->ReserveObjects(m_NvPmCtx, objectsCount),
                       "call to 'NvPmApi::ReserveObjects'"))
    {
      return false;
    }
    m_ObjectsCount = objectsCount;
  }
  return true;
}

uint32_t NVCounters::BeginExperiment() const
{
  NVPMUINT NumPasses = 0;
  if(NvPmResultFails(m_NvPmApi->BeginExperiment(m_NvPmCtx, &NumPasses),
                     "call to 'NvPmApi::BeginExperiment'"))
  {
    return 0;
  }
  return NumPasses;
}

void NVCounters::EndExperiment(const std::vector<uint32_t> &eventIds,
                               std::vector<CounterResult> &Result) const
{
  NvPmResultFails(m_NvPmApi->EndExperiment(m_NvPmCtx), "call to 'NvPmApi::EndExperiment'");

  Result.reserve(m_SelectedExternalIds.size() * m_ObjectsCount);

  for(uint32_t counterIdx = 0; counterIdx < m_SelectedExternalIds.size(); ++counterIdx)
  {
    const GPUCounter counter = m_SelectedExternalIds[counterIdx];

    const uint32_t externalId = (uint32_t)counter - (uint32_t)GPUCounter::FirstNvidia;

    const NVPMCounterID internalId = m_InternalIds[externalId];

    const uint32_t internalDesc = m_InternalDescriptors[externalId];

    const NVPMCOUNTERDISPLAY displayType = (NVPMCOUNTERDISPLAY)(internalDesc >> 1);
    const NVPMCOUNTERVALUETYPE counterType = (NVPMCOUNTERVALUETYPE)(internalDesc & 1);

    if(counterType == NVPM_VALUE_TYPE_UINT64)
    {
      if(displayType == NVPM_CD_RATIO)
      {
        for(uint32_t i = 0; i < m_ObjectsCount; ++i)
        {
          NVPMUINT64 Value;
          NVPMUINT64 Cycles;
          NVPMUINT8 Overflow;
          NVPMRESULT result =
              m_NvPmApi->GetCounterValueUint64(m_NvPmCtx, internalId, i, &Value, &Cycles, &Overflow);

          double Ratio = (double)Value / (double)Cycles;
          Result.push_back(CounterResult(eventIds[i], counter, Ratio));

          (void)result;
        }
      }
      else
      {
        for(uint32_t i = 0; i < m_ObjectsCount; ++i)
        {
          NVPMUINT64 Value;
          NVPMUINT64 Cycles;
          NVPMUINT8 Overflow;
          NVPMRESULT result =
              m_NvPmApi->GetCounterValueUint64(m_NvPmCtx, internalId, i, &Value, &Cycles, &Overflow);

          Result.push_back(CounterResult(eventIds[i], counter, Value));

          (void)result;
        }
      }
    }
    else
    {
      for(uint32_t i = 0; i < m_ObjectsCount; ++i)
      {
        NVPMFLOAT64 Value;
        NVPMUINT64 Cycles;
        NVPMUINT8 Overflow;
        NVPMRESULT result =
            m_NvPmApi->GetCounterValueFloat64(m_NvPmCtx, internalId, i, &Value, &Cycles, &Overflow);

        Result.push_back(CounterResult(eventIds[i], counter, Value));

        (void)result;
      }
    }
  }
}

void NVCounters::BeginPass(uint32_t passIdx) const
{
  NvPmResultFails(m_NvPmApi->BeginPass(m_NvPmCtx, passIdx), "call to 'NvPmApi::BeginPass'");
}

void NVCounters::EndPass(uint32_t passIdx) const
{
  NvPmResultFails(m_NvPmApi->EndPass(m_NvPmCtx, passIdx), "call to 'NvPmApi::EndPass'");
}

void NVCounters::BeginSample(uint32_t sampleIdx) const
{
  RDCASSERT(sampleIdx < m_ObjectsCount);
  NvPmResultFails(m_NvPmApi->BeginObject(m_NvPmCtx, sampleIdx), "call to 'NvPmApi::BeginObject'");
}

void NVCounters::EndSample(uint32_t sampleIdx) const
{
  RDCASSERT(sampleIdx < m_ObjectsCount);
  NvPmResultFails(m_NvPmApi->EndObject(m_NvPmCtx, sampleIdx), "call to 'NvPmApi::EndObject'");
}
