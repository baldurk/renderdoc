#include "nv_counters.h"
#include "common/common.h"

#define NVPM_INITGUID
#include "official/PerfKit/include/NvPmApi.h"
#include "strings/string_utils.h"

struct EnumCountersCtx
{
  std::vector<GPUCounter> mExternalIds;
  std::vector<uint32_t> mInternalIds;
  std::vector<CounterDescription> mDescriptors;

  NvPmApi *mNvPmApi;

  uint32_t mCurrentCounterId;
};

static bool NvPmResultFails(NVPMRESULT actual, char const *failMsg)
{
  return actual != NVPM_OK;
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

  CounterDescription &desc = pEnumCtx->mDescriptors[i];

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
  NVPMCOUNTERDISPLAY Display = static_cast<NVPMCOUNTERDISPLAY>(Attribute);

  switch(Display)
  {
    case NVPM_CD_RATIO: desc.unit = CounterUnit::Ratio; break;

    case NVPM_CD_RAW: desc.unit = CounterUnit::Cycles; break;
  }

  pEnumCtx->mNvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_DOMAIN, &Attribute);

  pEnumCtx->mNvPmApi->GetCounterAttribute(unCounterID, NVPMA_COUNTER_VALUE_TYPE, &Attribute);
  NVPMCOUNTERVALUETYPE ValueType = static_cast<NVPMCOUNTERVALUETYPE>(Attribute);

  switch(ValueType)
  {
    case NVPM_VALUE_TYPE_UINT64:
    {
      desc.resultType = CompType::UInt;
      desc.resultByteWidth = sizeof(uint64_t);
    }
    break;

    case NVPM_VALUE_TYPE_FLOAT64:
    {
      desc.resultType = CompType::Double;
      desc.resultByteWidth = sizeof(double);
    }
    break;
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

NVCounters::NVCounters() : mNvPmLib(NULL), mNvPmApi(NULL), mNvPmCtx(static_cast<uint64_t>(-1)) {}

NVCounters::~NVCounters()
{
  if(mNvPmCtx != static_cast<uint64_t>(-1))
  {
    mNvPmApi->DestroyContext(mNvPmCtx);
    mNvPmCtx = static_cast<uint64_t>(-1);
  }

  if(mNvPmApi != 0)
  {
    mNvPmApi->Shutdown();
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
    return false;
  }

  NVPMGetExportTable_Pfn pfnGetExportTable =
      (NVPMGetExportTable_Pfn)Process::GetFunctionAddress(mNvPmLib, "NVPMGetExportTable");
  if(pfnGetExportTable == NULL)
  {
    return false;
  }

  if(NvPmResultFails(pfnGetExportTable(&ETID_NvPmApi, (void **)&mNvPmApi), "Get 'NvPmApi' table"))
  {
    return false;
  }

  if(NvPmResultFails(mNvPmApi->Init(), "Init 'NvPmApi'"))
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
                     "Init 'NVPMContext' from ID3D11Device"))
  {
    return false;
  }

  uint32_t NumCounters = 0;
  mNvPmApi->EnumCountersByContextUserData(mNvPmCtx, NvPmCountCounters, &NumCounters);

  EnumCountersCtx ctx;
  ctx.mExternalIds.resize(NumCounters);
  ctx.mInternalIds.resize(NumCounters);
  ctx.mDescriptors.resize(NumCounters);
  ctx.mNvPmApi = mNvPmApi;
  ctx.mCurrentCounterId = 0;

  mNvPmApi->EnumCountersByContextUserData(mNvPmCtx, NvPmGatherCounters, &ctx);

  ctx.mExternalIds.swap(mExternalIds);
  ctx.mInternalIds.swap(mInternalIds);
  ctx.mDescriptors.swap(mDescriptors);

  return true;
}
