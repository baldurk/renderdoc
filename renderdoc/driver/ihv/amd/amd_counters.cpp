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
#include "core/plugins.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPI.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPIFunctionTypes.h"

#include "strings/string_utils.h"

AMDCounters::AMDCounters() : m_pGPUPerfAPI(NULL)
{
}

bool AMDCounters::Init(void *pContext)
{
  const char *dllName = "GPUPerfAPIDX11-x64.dll";

  // first try in the plugin location it will be in distributed builds
  HMODULE module = LoadLibraryA(LocatePluginFile("amd/counters", dllName).c_str());

  // if that failed then try checking for it just in the default search path
  if(module == NULL)
  {
    module = LoadLibraryA(dllName);
  }

  if(module == NULL)
  {
    RDCWARN(
        "AMD GPU performance counters could not be initialized successfully. "
        "Are you missing the DLLs?");

    return false;
  }

  m_pGPUPerfAPI = new GPAApi();
  GPA_GetFuncTablePtrType getFuncTable =
      (GPA_GetFuncTablePtrType)GetProcAddress(module, "GPA_GetFuncTable");

  if(getFuncTable)
  {
    getFuncTable((void **)&m_pGPUPerfAPI);
  }
  else
  {
    RDCWARN(
        "GPA version is out of date, doesn't expose GPA_GetFuncTable. Make sure you have 3.0 or "
        "above.");
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  if(m_pGPUPerfAPI->GPA_Initialize() != GPA_STATUS_OK)
  {
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  if(m_pGPUPerfAPI->GPA_OpenContext(pContext, GPA_OPENCONTEXT_HIDE_PUBLIC_COUNTERS_BIT |
                                                  GPA_OPENCONTEXT_CLOCK_MODE_PEAK_BIT) !=
     GPA_STATUS_OK)
  {
    m_pGPUPerfAPI->GPA_Destroy();
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

    status = m_pGPUPerfAPI->GPA_CloseContext();
    status = m_pGPUPerfAPI->GPA_Destroy();

    delete m_pGPUPerfAPI;
  }
}

vector<AMDCounters::InternalCounterDescription> AMDCounters::EnumerateCounters()
{
  gpa_uint32 num;

  m_pGPUPerfAPI->GPA_GetNumCounters(&num);

  vector<InternalCounterDescription> counters;

  for(uint32_t i = 0; i < num; ++i)
  {
    InternalCounterDescription internalDesc;
    internalDesc.desc = InternalGetCounterDescription(i);

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
  m_pGPUPerfAPI->GPA_GetCounterName(internalIndex, &tmp);
  desc.name = tmp;
  m_pGPUPerfAPI->GPA_GetCounterDescription(internalIndex, &tmp);
  desc.description = tmp;
  m_pGPUPerfAPI->GPA_GetCounterCategory(internalIndex, &tmp);
  desc.category = tmp;

  GPA_Usage_Type usageType;

  m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);

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

  m_pGPUPerfAPI->GPA_GetCounterDataType(internalIndex, &type);

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

  // C8958C90-B706-4F22-8AF5-E0A3831B2C39
  desc.uuid.words[0] = 0xC8958C90;
  desc.uuid.words[1] = 0xB7064F22;
  desc.uuid.words[2] = 0x8AF5E0A3 ^ strhash(desc.name.c_str());
  desc.uuid.words[3] = 0x831B2C39 ^ strhash(desc.description.c_str());

  return desc;
}

void AMDCounters::EnableCounter(GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;

  m_pGPUPerfAPI->GPA_EnableCounter(internalIndex);
}

void AMDCounters::EnableAllCounters()
{
  m_pGPUPerfAPI->GPA_EnableAllCounters();
}

void AMDCounters::DisableAllCounters()
{
  m_pGPUPerfAPI->GPA_DisableAllCounters();
}

uint32_t AMDCounters::GetPassCount()
{
  gpa_uint32 numRequiredPasses;
  m_pGPUPerfAPI->GPA_GetPassCount(&numRequiredPasses);

  return (uint32_t)numRequiredPasses;
}

uint32_t AMDCounters::BeginSession()
{
  gpa_uint32 sessionID;

  m_pGPUPerfAPI->GPA_BeginSession(&sessionID);

  return (uint32_t)sessionID;
}

void AMDCounters::EndSesssion()
{
  m_pGPUPerfAPI->GPA_EndSession();
}

bool AMDCounters::IsSessionReady(uint32_t sessionIndex)
{
  gpa_uint8 readyResult = 0;

  GPA_Status status = m_pGPUPerfAPI->GPA_IsSessionReady(&readyResult, sessionIndex);

  return readyResult && status == GPA_STATUS_OK;
}

void AMDCounters::BeginPass()
{
  m_pGPUPerfAPI->GPA_BeginPass();
}

void AMDCounters::EndPass()
{
  m_pGPUPerfAPI->GPA_EndPass();
}

void AMDCounters::BeginSample(uint32_t index)
{
  m_pGPUPerfAPI->GPA_BeginSample(index);
}

void AMDCounters::EndSample()
{
  m_pGPUPerfAPI->GPA_EndSample();
}

uint32_t AMDCounters::GetSampleUint32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  uint32_t value;

  m_pGPUPerfAPI->GPA_GetSampleUInt32(session, sample, internalIndex, &value);

  // normalise units as expected
  GPA_Usage_Type usageType;
  m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
    value *= 1000;

  return value;
}

uint64_t AMDCounters::GetSampleUint64(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  gpa_uint64 value;

  m_pGPUPerfAPI->GPA_GetSampleUInt64(session, sample, internalIndex, &value);

  // normalise units as expected
  GPA_Usage_Type usageType;
  m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
    value *= 1000;

  return value;
}

float AMDCounters::GetSampleFloat32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  float value;

  m_pGPUPerfAPI->GPA_GetSampleFloat32(session, sample, internalIndex, &value);

  // normalise units as expected
  GPA_Usage_Type usageType;
  m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
    value *= 1000.0f;
  else if(usageType == GPA_USAGE_TYPE_MILLISECONDS)
    value /= 1000.0f;

  return value;
}

double AMDCounters::GetSampleFloat64(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_Counters[GPUCounterToCounterIndex(counter)].internalIndex;
  double value;

  m_pGPUPerfAPI->GPA_GetSampleFloat64(session, sample, internalIndex, &value);

  // normalise units as expected
  GPA_Usage_Type usageType;
  m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
    value *= 1000.0;
  else if(usageType == GPA_USAGE_TYPE_MILLISECONDS)
    value /= 1000.0;

  return value;
}