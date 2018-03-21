#include "nv_counters.h"
#include "common/common.h"

#define NVPM_INITGUID
#include "official/PerfKit/include/NvPmApi.h"
#include "strings/string_utils.h"

#include <algorithm>

struct EnumCountersCtx
{
  std::vector<GPUCounter> mExternalIds;
  std::vector<uint32_t> mInternalIds;
  std::vector<CounterDescription> mExternalDescriptors;
  std::vector<uint32_t> mInternalDescriptors;

  NvPmApi *mNvPmApi;

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
  uint32_t *pNumCounters = static_cast<uint32_t *>(pUserData);
  *pNumCounters += 1;
  return NVPM_OK;
}

int NvPmGatherCounters(NVPMCounterID unCounterID, const char *pcCounterName, void *pUserData)
{
  EnumCountersCtx *pEnumCtx = static_cast<EnumCountersCtx *>(pUserData);

  const uint32_t i = pEnumCtx->mCurrentCounterId;

  GPUCounter globalId = static_cast<GPUCounter>(static_cast<uint32_t>(GPUCounter::FirstNvidia) + i);
  pEnumCtx->mExternalIds[i] = globalId;
  pEnumCtx->mInternalIds[i] = unCounterID;

  CounterDescription &desc = pEnumCtx->mExternalDescriptors[i];

  NVPMUINT64 Attribute = 0;

  pEnumCtx->mNvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_TYPE, &Attribute);
  NVPMCOUNTERTYPE Type = static_cast<NVPMCOUNTERTYPE>(Attribute);

  switch(Type)
  {
    case NVPM_CT_GPU: desc.category = "GPU"; break;

    case NVPM_CT_OGL: desc.category = "D3D"; break;

    case NVPM_CT_D3D: desc.category = "D3D"; break;

    case NVPM_CT_SIMEXP: desc.category = "SIMEXP"; break;

    case NVPM_CT_AGGREGATE: desc.category = "AGGREGATE"; break;

    case NVPM_CT_USER: desc.category = "USER"; break;
  };

  pEnumCtx->mNvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_DISPLAY, &Attribute);
  NVPMCOUNTERDISPLAY DisplayType = static_cast<NVPMCOUNTERDISPLAY>(Attribute);

  pEnumCtx->mNvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_DOMAIN, &Attribute);

  pEnumCtx->mNvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_VALUE_TYPE, &Attribute);
  NVPMCOUNTERVALUETYPE ValueType = static_cast<NVPMCOUNTERVALUETYPE>(Attribute);

  pEnumCtx->mInternalDescriptors[i] = (DisplayType << 1) | ValueType;

  if(ValueType == NVPM_VALUE_TYPE_UINT64)
  {
    if(DisplayType == NVPM_CD_RATIO)
    {
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
    desc.unit = CounterUnit::Ratio;
    desc.resultType = CompType::Double;
    desc.resultByteWidth = sizeof(double);
  }

  char Description[512];
  NVPMUINT DescriptionSize = sizeof(Description);
  pEnumCtx->mNvPmApi->GetCounterDescription(unCounterID, Description, &DescriptionSize);

  desc.counter = globalId;
  desc.description = Description;
  desc.name = pcCounterName;

  desc.uuid.words[0] = 0xC8958C90;
  desc.uuid.words[1] = 0xB7064F22;
  desc.uuid.words[2] = 0x8AF5E0A3 ^ strhash(desc.name.c_str());
  desc.uuid.words[3] = 0x831B2C39 ^ strhash(desc.description.c_str());

  pEnumCtx->mCurrentCounterId += 1;

  return NVPM_OK;
}

NVCounters::NVCounters()
    : mNvPmLib(NULL), mNvPmApi(NULL), mNvPmCtx(static_cast<uint64_t>(-1)), mObjectsCount(0)
{
}

NVCounters::~NVCounters()
{
  if(mObjectsCount != 0)
  {
    NvPmResultFails(mNvPmApi->DeleteObjects(mNvPmCtx), "call to 'NvPmApi::DeleteObjects'");
    mObjectsCount = 0;
  }

  if(mNvPmCtx != static_cast<uint64_t>(-1))
  {
    NvPmResultFails(mNvPmApi->DestroyContext(mNvPmCtx), "call to 'NvPmApi::DestroyContext'");
    mNvPmCtx = static_cast<uint64_t>(-1);
  }

  if(mNvPmApi != 0)
  {
    NvPmResultFails(mNvPmApi->Shutdown(), "call to 'NvPmApi::Shutdown'");
    mNvPmApi = NULL;
  }
  mNvPmLib = NULL;
}

bool NVCounters::Init()
{
  if(mNvPmLib != NULL)
  {
    return false;
  }

  mNvPmLib = Process::LoadModule("NvPmApi.Core.dll");
  if(mNvPmLib == NULL)
  {
    RDCWARN("NV GPU performance counters could not locate 'NvPmApi.Core.dll'");
    return false;
  }

  NVPMGetExportTable_Pfn pfnGetExportTable =
      (NVPMGetExportTable_Pfn)Process::GetFunctionAddress(mNvPmLib, "NVPMGetExportTable");
  if(pfnGetExportTable == NULL)
  {
    return false;
  }

  if(NvPmResultFails(pfnGetExportTable(&ETID_NvPmApi, (void **)&mNvPmApi), "get 'NvPmApi' table"))
  {
    return false;
  }

  if(NvPmResultFails(mNvPmApi->Init(), "init 'NvPmApi'"))
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

  if(NvPmResultFails(mNvPmApi->CreateContextFromD3D11Device(pDevice, &mNvPmCtx),
                     "init 'NVPMContext' from ID3D11Device"))
  {
    return false;
  }

  uint32_t NumCounters = 0;
  mNvPmApi->EnumCountersByContextUserData(mNvPmCtx, NvPmCountCounters, &NumCounters);

  EnumCountersCtx ctx;
  ctx.mExternalIds.resize(NumCounters);
  ctx.mInternalIds.resize(NumCounters);
  ctx.mExternalDescriptors.resize(NumCounters);
  ctx.mInternalDescriptors.resize(NumCounters);
  ctx.mNvPmApi = mNvPmApi;
  ctx.mCurrentCounterId = 0;

  mNvPmApi->EnumCountersByContextUserData(mNvPmCtx, NvPmGatherCounters, &ctx);

  ctx.mExternalIds.swap(mExternalIds);
  ctx.mInternalIds.swap(mInternalIds);
  ctx.mExternalDescriptors.swap(mExternalDescriptors);
  ctx.mInternalDescriptors.swap(mInternalDescriptors);

  mSelectedExternalIds.reserve(NumCounters);
  mSelectedInternalIds.reserve(NumCounters);

  return true;
}

bool NVCounters::PrepareExperiment(const std::vector<GPUCounter> &counters, uint32_t objectsCount)
{
  if(NvPmResultFails(mNvPmApi->RemoveAllCounters(mNvPmCtx), "call to 'NvPmApi::RemoveAllCounters'"))
  {
    return false;
  }

  mSelectedExternalIds.clear();
  mSelectedInternalIds.clear();

  std::for_each(counters.begin(), counters.end(),
                [&selExternalIds = mSelectedExternalIds, &selInternalIds = mSelectedInternalIds,
                 &internalIds = mInternalIds](GPUCounter counter) {
                  const uint32_t externalId = static_cast<uint32_t>(counter) -
                                              static_cast<uint32_t>(GPUCounter::FirstNvidia);
                  selExternalIds.push_back(counter);
                  selInternalIds.push_back(internalIds[externalId]);
                });

  if(NvPmResultFails(
         mNvPmApi->AddCounters(mNvPmCtx, static_cast<NVPMUINT>(mSelectedInternalIds.size()),
                               mSelectedInternalIds.data()),
         "call to 'NvPmApi::AddCounters'"))
  {
    return false;
  }

  if(mObjectsCount != objectsCount)
  {
    if(mObjectsCount != 0)
    {
      NvPmResultFails(mNvPmApi->DeleteObjects(mNvPmCtx), "call to 'NvPmApi::DeleteObjects'");
      mObjectsCount = 0;
    }

    if(NvPmResultFails(mNvPmApi->ReserveObjects(mNvPmCtx, objectsCount),
                       "call to 'NvPmApi::ReserveObjects'"))
    {
      return false;
    }
    mObjectsCount = objectsCount;
  }
  return true;
}

uint32_t NVCounters::BeginExperiment() const
{
  NVPMUINT NumPasses = 0;
  if(NvPmResultFails(mNvPmApi->BeginExperiment(mNvPmCtx, &NumPasses),
                     "call to 'NvPmApi::BeginExperiment'"))
  {
    return 0;
  }
  return NumPasses;
}

void NVCounters::EndExperiment(const std::vector<uint32_t> &eventIds,
                               std::vector<CounterResult> &Result) const
{
  NvPmResultFails(mNvPmApi->EndExperiment(mNvPmCtx), "call to 'NvPmApi::EndExperiment'");

  // NVPMUINT NumCounters = mObjectsCount;
  // NVPMRESULT result = mNvPmApi->SampleEx(mNvPmCtx, Samples.data(), &NumCounters);
  // NvPmResultFails(result, "call to 'NvPmApi::SampleEx'");
  // mNvPmApi->GetCounterValue();

  Result.reserve(mSelectedExternalIds.size() * mObjectsCount);

  for(uint32_t counterIdx = 0; counterIdx < mSelectedExternalIds.size(); ++counterIdx)
  {
    const GPUCounter counter = mSelectedExternalIds[counterIdx];

    const uint32_t externalId =
        static_cast<uint32_t>(counter) - static_cast<uint32_t>(GPUCounter::FirstNvidia);

    const NVPMCounterID internalId = mInternalIds[externalId];

    const uint32_t internalDesc = mInternalDescriptors[externalId];

    const NVPMCOUNTERDISPLAY displayType = static_cast<NVPMCOUNTERDISPLAY>(internalDesc >> 1);
    const NVPMCOUNTERTYPE counterType = static_cast<NVPMCOUNTERTYPE>(internalDesc & 1);

    if(counterType == NVPM_VALUE_TYPE_UINT64)
    {
      if(displayType == NVPM_CD_RATIO)
      {
        for(uint32_t i = 0; i < mObjectsCount; ++i)
        {
          NVPMUINT64 Value;
          NVPMUINT64 Cycles;
          NVPMUINT8 Overflow;
          NVPMRESULT result =
              mNvPmApi->GetCounterValueUint64(mNvPmCtx, internalId, i, &Value, &Cycles, &Overflow);

          double Ratio = static_cast<double>(Value) / static_cast<double>(Cycles);
          Result.push_back(CounterResult(eventIds[i], counter, Ratio));

          (void)result;
        }
      }
      else
      {
        for(uint32_t i = 0; i < mObjectsCount; ++i)
        {
          NVPMUINT64 Value;
          NVPMUINT64 Cycles;
          NVPMUINT8 Overflow;
          NVPMRESULT result =
              mNvPmApi->GetCounterValueUint64(mNvPmCtx, internalId, i, &Value, &Cycles, &Overflow);

          Result.push_back(CounterResult(eventIds[i], counter, Value));

          (void)result;
        }
      }
    }
    else
    {
      for(uint32_t i = 0; i < mObjectsCount; ++i)
      {
        NVPMFLOAT64 Value;
        NVPMUINT64 Cycles;
        NVPMUINT8 Overflow;
        NVPMRESULT result =
            mNvPmApi->GetCounterValueFloat64(mNvPmCtx, internalId, i, &Value, &Cycles, &Overflow);

        Result.push_back(CounterResult(eventIds[i], counter, Value));

        (void)result;
      }
    }
  }
}

void NVCounters::BeginPass(uint32_t passIdx) const
{
  NvPmResultFails(mNvPmApi->BeginPass(mNvPmCtx, passIdx), "call to 'NvPmApi::BeginPass'");
}

void NVCounters::EndPass(uint32_t passIdx) const
{
  NvPmResultFails(mNvPmApi->EndPass(mNvPmCtx, passIdx), "call to 'NvPmApi::EndPass'");
}

void NVCounters::BeginSample(uint32_t sampleIdx) const
{
  RDCASSERT(sampleIdx < mObjectsCount);
  NvPmResultFails(mNvPmApi->BeginObject(mNvPmCtx, sampleIdx), "call to 'NvPmApi::BeginObject'");
}

void NVCounters::EndSample(uint32_t sampleIdx) const
{
  RDCASSERT(sampleIdx < mObjectsCount);
  NvPmResultFails(mNvPmApi->EndObject(mNvPmCtx, sampleIdx), "call to 'NvPmApi::EndObject'");
}
