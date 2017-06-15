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

#include "amd_counters.h"
#include <Windows.h>
#include "common/common.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPI.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPIFunctionTypes.h"

typedef GPA_Status(__stdcall *PFN_GPA_INITIALIZE)();
typedef GPA_Status(__stdcall *PFN_GPA_OPENCONTEXT)(void *pContext);
typedef GPA_Status(__stdcall *PFN_GPA_GET_NUM_COUNTERS)(gpa_uint32 *pCount);
typedef GPA_Status(__stdcall *PFN_GPA_GET_COUNTER_NAME)(gpa_uint32 index, const char **ppName);
typedef GPA_Status(__stdcall *PFN_GPA_GET_COUNTER_INDEX)(const char *pCounter, gpa_uint32 *pIndex);
typedef GPA_Status(__stdcall *PFN_GPA_GET_COUNTER_DESCRIPTION)(gpa_uint32 index,
                                                               const char **ppDescription);
typedef GPA_Status(__stdcall *PFN_GPA_GET_COUNTER_DATA_TYPE)(gpa_uint32 index, GPA_Type *pDataType);
typedef GPA_Status(__stdcall *PFN_GPA_GET_COUNTER_USAGE_TYPE)(gpa_uint32 index,
                                                              GPA_Usage_Type *usageType);
typedef GPA_Status(__stdcall *PFN_GPA_ENABLE_COUNTER)(gpa_uint32 index);
typedef GPA_Status(__stdcall *PFN_GPA_ENABLE_COUNTER_STR)(const char *pCounter);
typedef GPA_Status(__stdcall *PFN_GPA_ENABLE_ALL_COUNTERS)();
typedef GPA_Status(__stdcall *PFN_GPA_DISABLE_COUNTER)(gpa_uint32 index);
typedef GPA_Status(__stdcall *PFN_GPA_DISABLE_COUNTER_STR)(const char *pCounter);
typedef GPA_Status(__stdcall *PFN_GPA_DISABLE_ALL_COUNTERS)();
typedef GPA_Status(__stdcall *PFN_GPA_GET_PASS_COUNT)(gpa_uint32 *pNumPasses);
typedef GPA_Status(__stdcall *PFN_GPA_BEGIN_SESSION)(gpa_uint32 *pSessionID);
typedef GPA_Status(__stdcall *PFN_GPA_END_SESSION)();
typedef GPA_Status(__stdcall *PFN_GPA_BEGIN_PASS)();
typedef GPA_Status(__stdcall *PFN_GPA_END_PASS)();
typedef GPA_Status(__stdcall *PFN_GPA_BEGIN_SAMPLE)(gpa_uint32 sampleID);
typedef GPA_Status(__stdcall *PFN_GPA_END_SAMPLE)();
typedef GPA_Status(__stdcall *PFN_GPA_IS_SESSION_READY)(bool *pReadyResult, gpa_uint32 sessionID);
typedef GPA_Status(__stdcall *PFN_GPA_IS_SAMPLE_READY)(bool *pReadyResult, gpa_uint32 sessionID,
                                                       gpa_uint32 sampleID);
typedef GPA_Status(__stdcall *PFN_GPA_GET_SAMPLE_UINT32)(gpa_uint32 sessionID, gpa_uint32 sampleID,
                                                         gpa_uint32 counterID, gpa_uint32 *pResult);
typedef GPA_Status(__stdcall *PFN_GPA_GET_SAMPLE_UINT64)(gpa_uint32 sessionID, gpa_uint32 sampleID,
                                                         gpa_uint32 counterID, gpa_uint64 *pResult);
typedef GPA_Status(__stdcall *PFN_GPA_GET_SAMPLE_FLOAT32)(gpa_uint32 sessionID, gpa_uint32 sampleID,
                                                          gpa_uint32 counterID, gpa_float32 *pResult);
typedef GPA_Status(__stdcall *PFN_GPA_GET_SAMPLE_FLOAT64)(gpa_uint32 sessionID, gpa_uint32 sampleID,
                                                          gpa_uint32 counterID, gpa_float64 *pResult);
typedef GPA_Status(__stdcall *PFN_GPA_CLOSE_CONTEXT)();
typedef GPA_Status(__stdcall *PFN_GPA_DESTROY)();

class GPUPerfAPI
{
public:
  PFN_GPA_INITIALIZE init;
  PFN_GPA_OPENCONTEXT openContext;
  PFN_GPA_GET_NUM_COUNTERS getNumCounters;
  PFN_GPA_GET_COUNTER_NAME getCounterName;
  PFN_GPA_GET_COUNTER_INDEX getCounterIndex;
  PFN_GPA_GET_COUNTER_DESCRIPTION getCounterDescription;
  PFN_GPA_GET_COUNTER_DATA_TYPE getCounterDataType;
  PFN_GPA_GET_COUNTER_USAGE_TYPE getCounterUsageType;
  PFN_GPA_ENABLE_COUNTER enableCounter;
  PFN_GPA_ENABLE_COUNTER_STR enableCounterStr;
  PFN_GPA_ENABLE_ALL_COUNTERS enableAllCounters;
  PFN_GPA_DISABLE_COUNTER disableCounter;
  PFN_GPA_DISABLE_COUNTER_STR disableCounterStr;
  PFN_GPA_DISABLE_ALL_COUNTERS disableAllCounters;
  PFN_GPA_GET_PASS_COUNT getPassCount;
  PFN_GPA_BEGIN_SESSION beginSession;
  PFN_GPA_END_SESSION endSession;
  PFN_GPA_BEGIN_PASS beginPass;
  PFN_GPA_END_PASS endPass;
  PFN_GPA_BEGIN_SAMPLE beginSample;
  PFN_GPA_END_SAMPLE endSample;
  PFN_GPA_IS_SESSION_READY isSessionReady;
  PFN_GPA_IS_SAMPLE_READY isSampleReady;
  PFN_GPA_GET_SAMPLE_UINT32 getSampleUInt32;
  PFN_GPA_GET_SAMPLE_UINT64 getSampleUInt64;
  PFN_GPA_GET_SAMPLE_FLOAT32 getSampleFloat32;
  PFN_GPA_GET_SAMPLE_FLOAT64 getSampleFloat64;
  PFN_GPA_CLOSE_CONTEXT closeContext;
  PFN_GPA_DESTROY destroy;
};

AMDCounters::AMDCounters() : m_pGPUPerfAPI(NULL)
{
}

bool AMDCounters::Init(void *pContext)
{
  // first try in the location it will be in distributed builds
  HMODULE module = LoadLibraryA("plugins/amd/counters/GPUPerfAPIDX11-x64.dll");

  // if that failed then try checking for it just in the default search path
  if(module == NULL)
  {
    module = LoadLibraryA("GPUPerfAPIDX11-x64.dll");
  }

  if(module == NULL)
  {
    RDCWARN(
        "AMD GPU performance counters could not be initialized successfully. "
        "Are you missing the DLLs?");

    return false;
  }

  m_pGPUPerfAPI = new GPUPerfAPI();
  m_pGPUPerfAPI->init = (PFN_GPA_INITIALIZE)GetProcAddress(module, "GPA_Initialize");
  m_pGPUPerfAPI->openContext = (PFN_GPA_OPENCONTEXT)GetProcAddress(module, "GPA_OpenContext");
  m_pGPUPerfAPI->getNumCounters =
      (PFN_GPA_GET_NUM_COUNTERS)GetProcAddress(module, "GPA_GetNumCounters");
  m_pGPUPerfAPI->getCounterName =
      (PFN_GPA_GET_COUNTER_NAME)GetProcAddress(module, "GPA_GetCounterName");
  m_pGPUPerfAPI->getCounterIndex =
      (PFN_GPA_GET_COUNTER_INDEX)GetProcAddress(module, "GPA_GetCounterIndex");
  m_pGPUPerfAPI->getCounterDescription =
      (PFN_GPA_GET_COUNTER_DESCRIPTION)GetProcAddress(module, "GPA_GetCounterDescription");
  m_pGPUPerfAPI->getCounterDataType =
      (PFN_GPA_GET_COUNTER_DATA_TYPE)GetProcAddress(module, "GPA_GetCounterDataType");
  m_pGPUPerfAPI->getCounterUsageType =
      (PFN_GPA_GET_COUNTER_USAGE_TYPE)GetProcAddress(module, "GPA_GetCounterUsageType");
  m_pGPUPerfAPI->enableCounter =
      (PFN_GPA_ENABLE_COUNTER)GetProcAddress(module, "GPA_EnableCounter");
  m_pGPUPerfAPI->enableCounterStr =
      (PFN_GPA_ENABLE_COUNTER_STR)GetProcAddress(module, "GPA_EnableCounterStr");
  m_pGPUPerfAPI->enableAllCounters =
      (PFN_GPA_ENABLE_ALL_COUNTERS)GetProcAddress(module, "GPA_EnableAllCounters");
  m_pGPUPerfAPI->disableCounter =
      (PFN_GPA_DISABLE_COUNTER)GetProcAddress(module, "GPA_DisableCounter");
  m_pGPUPerfAPI->disableCounterStr =
      (PFN_GPA_DISABLE_COUNTER_STR)GetProcAddress(module, "GPA_DisableCounterStr");
  m_pGPUPerfAPI->disableAllCounters =
      (PFN_GPA_DISABLE_ALL_COUNTERS)GetProcAddress(module, "GPA_DisableAllCounters");
  m_pGPUPerfAPI->getPassCount = (PFN_GPA_GET_PASS_COUNT)GetProcAddress(module, "GPA_GetPassCount");
  m_pGPUPerfAPI->beginSession = (PFN_GPA_BEGIN_SESSION)GetProcAddress(module, "GPA_BeginSession");
  m_pGPUPerfAPI->endSession = (PFN_GPA_END_SESSION)GetProcAddress(module, "GPA_EndSession");
  m_pGPUPerfAPI->beginPass = (PFN_GPA_BEGIN_PASS)GetProcAddress(module, "GPA_BeginPass");
  m_pGPUPerfAPI->endPass = (PFN_GPA_END_PASS)GetProcAddress(module, "GPA_EndPass");
  m_pGPUPerfAPI->beginSample = (PFN_GPA_BEGIN_SAMPLE)GetProcAddress(module, "GPA_BeginSample");
  m_pGPUPerfAPI->endSample = (PFN_GPA_END_SAMPLE)GetProcAddress(module, "GPA_EndSample");
  m_pGPUPerfAPI->isSessionReady =
      (PFN_GPA_IS_SESSION_READY)GetProcAddress(module, "GPA_IsSessionReady");
  m_pGPUPerfAPI->isSampleReady =
      (PFN_GPA_IS_SAMPLE_READY)GetProcAddress(module, "GPA_IsSampleReady");
  m_pGPUPerfAPI->getSampleUInt32 =
      (PFN_GPA_GET_SAMPLE_UINT32)GetProcAddress(module, "GPA_GetSampleUInt32");
  m_pGPUPerfAPI->getSampleUInt64 =
      (PFN_GPA_GET_SAMPLE_UINT64)GetProcAddress(module, "GPA_GetSampleUInt64");
  m_pGPUPerfAPI->getSampleFloat32 =
      (PFN_GPA_GET_SAMPLE_FLOAT32)GetProcAddress(module, "GPA_GetSampleFloat32");
  m_pGPUPerfAPI->getSampleFloat64 =
      (PFN_GPA_GET_SAMPLE_FLOAT64)GetProcAddress(module, "GPA_GetSampleFloat64");
  m_pGPUPerfAPI->closeContext = (PFN_GPA_CLOSE_CONTEXT)GetProcAddress(module, "GPA_CloseContext");
  m_pGPUPerfAPI->destroy = (PFN_GPA_DESTROY)GetProcAddress(module, "GPA_Destroy");

  if(m_pGPUPerfAPI->init() != GPA_STATUS_OK)
  {
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  if(m_pGPUPerfAPI->openContext(pContext) != GPA_STATUS_OK)
  {
    m_pGPUPerfAPI->destroy();
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  m_Counters = EnumerateCounters();

  return true;
}

AMDCounters::~AMDCounters()
{
  if(m_pGPUPerfAPI)
  {
    GPA_Status status = GPA_STATUS_OK;

    status = m_pGPUPerfAPI->closeContext();
    status = m_pGPUPerfAPI->destroy();

    delete m_pGPUPerfAPI;
  }
}

vector<AMDCounters::InternalCounterDescription> AMDCounters::EnumerateCounters()
{
  gpa_uint32 num;

  m_pGPUPerfAPI->getNumCounters(&num);

  vector<InternalCounterDescription> counters;

  for(uint32_t i = 0; i < num; ++i)
  {
    InternalCounterDescription internalDesc;
    internalDesc.desc = InternalGetCounterDescription(i);

    // We ignore any D3D11 counters, as those are handled elsewhere
    if(strncmp(internalDesc.desc.description, "#D3D11#", 7) == 0)
    {
      continue;
    }

    internalDesc.internalIndex = i;
    internalDesc.desc.counterID = MakeAMDCounter(i);
    counters.push_back(internalDesc);
  }

  return counters;
}

uint32_t AMDCounters::GetNumCounters()
{
  return (uint32_t)m_Counters.size();
}

CounterDescription AMDCounters::GetCounterDescription(GPUCounter counter)
{
  return m_Counters[GPUCounterToCounterIndex(counter)].desc;
}

CounterDescription AMDCounters::InternalGetCounterDescription(uint32_t internalIndex)
{
  CounterDescription desc = {};
  const char *tmp = NULL;
  m_pGPUPerfAPI->getCounterName(internalIndex, &tmp);
  desc.name = tmp;
  m_pGPUPerfAPI->getCounterDescription(internalIndex, &tmp);
  desc.description = tmp;

  GPA_Usage_Type usageType;

  m_pGPUPerfAPI->getCounterUsageType(internalIndex, &usageType);

  switch(usageType)
  {
    case GPA_USAGE_TYPE_RATIO:    ///< Result is a ratio of two different values or types
      desc.unit = CounterUnit::Ratio;
      break;
    case GPA_USAGE_TYPE_PERCENTAGE:    ///< Result is a percentage, typically within [0,100] range,
                                       /// but may be higher for certain counters
      desc.unit = CounterUnit::Percentage;
      break;
    case GPA_USAGE_TYPE_CYCLES:    ///< Result is in clock cycles
      desc.unit = CounterUnit::Cycles;
      break;
    case GPA_USAGE_TYPE_MILLISECONDS:    ///< Result is in milliseconds
      desc.unit = CounterUnit::Seconds;
      break;
    case GPA_USAGE_TYPE_KILOBYTES:    ///< Result is in kilobytes
    case GPA_USAGE_TYPE_BYTES:        ///< Result is in bytes
      desc.unit = CounterUnit::Bytes;
      break;
    case GPA_USAGE_TYPE_ITEMS:    ///< Result is a count of items or objects (ie, vertices,
                                  /// triangles, threads, pixels, texels, etc)
      desc.unit = CounterUnit::Absolute;
      break;
    default: desc.unit = CounterUnit::Absolute;
  }

  GPA_Type type;

  m_pGPUPerfAPI->getCounterDataType(internalIndex, &type);

  // results should either be float32/64 or uint32/64 as the GetSample functions only support those
  switch(type)
  {
    case GPA_TYPE_FLOAT32:    ///< Result will be a 32-bit float
      desc.resultType = CompType::Float;
      desc.resultByteWidth = sizeof(float);
      break;
    case GPA_TYPE_FLOAT64:    ///< Result will be a 64-bit float
      desc.resultType = CompType::Double;
      desc.resultByteWidth = sizeof(double);
      break;
    case GPA_TYPE_UINT32:    ///< Result will be a 32-bit unsigned int
      desc.resultType = CompType::UInt;
      desc.resultByteWidth = sizeof(uint32_t);
      break;
    case GPA_TYPE_UINT64:    ///< Result will be a 64-bit unsigned int
      desc.resultType = CompType::UInt;
      desc.resultByteWidth = sizeof(uint64_t);
      break;
    case GPA_TYPE_INT32:    ///< Result will be a 32-bit int
      desc.resultType = CompType::SInt;
      desc.resultByteWidth = sizeof(int32_t);
      break;
    case GPA_TYPE_INT64:    ///< Result will be a 64-bit int
      desc.resultType = CompType::SInt;
      desc.resultByteWidth = sizeof(int64_t);
      break;
    default: desc.resultType = CompType::UInt; desc.resultByteWidth = sizeof(uint32_t);
  }

  return desc;
}

void AMDCounters::EnableCounter(GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;

  m_pGPUPerfAPI->enableCounter(internalIndex);
}

void AMDCounters::EnableAllCounters()
{
  m_pGPUPerfAPI->enableAllCounters();
}

void AMDCounters::DisableAllCounters()
{
  m_pGPUPerfAPI->disableAllCounters();
}

uint32_t AMDCounters::GetPassCount()
{
  gpa_uint32 numRequiredPasses;
  m_pGPUPerfAPI->getPassCount(&numRequiredPasses);

  return (uint32_t)numRequiredPasses;
}

uint32_t AMDCounters::BeginSession()
{
  gpa_uint32 sessionID;

  m_pGPUPerfAPI->beginSession(&sessionID);

  return (uint32_t)sessionID;
}

void AMDCounters::EndSesssion()
{
  m_pGPUPerfAPI->endSession();
}

bool AMDCounters::IsSessionReady(uint32_t sessionIndex)
{
  bool readyResult = false;

  GPA_Status status = m_pGPUPerfAPI->isSessionReady(&readyResult, sessionIndex);

  return readyResult && status == GPA_STATUS_OK;
}

void AMDCounters::BeginPass()
{
  m_pGPUPerfAPI->beginPass();
}

void AMDCounters::EndPass()
{
  m_pGPUPerfAPI->endPass();
}

void AMDCounters::BeginSample(uint32_t index)
{
  m_pGPUPerfAPI->beginSample(index);
}

void AMDCounters::EndSample()
{
  m_pGPUPerfAPI->endSample();
}

uint32_t AMDCounters::GetSampleUint32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  uint32_t value;

  m_pGPUPerfAPI->getSampleUInt32(session, sample, internalIndex, &value);

  return value;
}

uint64_t AMDCounters::GetSampleUint64(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  gpa_uint64 value;

  m_pGPUPerfAPI->getSampleUInt64(session, sample, internalIndex, &value);

  return value;
}

float AMDCounters::GetSampleFloat32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  float value;

  m_pGPUPerfAPI->getSampleFloat32(session, sample, internalIndex, &value);

  return value;
}

double AMDCounters::GetSampleFloat64(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  double value;

  m_pGPUPerfAPI->getSampleFloat64(session, sample, internalIndex, &value);

  return value;
}