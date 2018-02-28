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

#include "amd_counters.h"
#include "common/common.h"
#include "common/timing.h"
#include "core/plugins.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPI.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPIFunctionTypes.h"
#include "strings/string_utils.h"

inline bool AMD_FAILED(GPA_Status status)
{
  return status != GPA_STATUS_OK;
}

inline bool AMD_SUCCEEDED(GPA_Status status)
{
  return status == GPA_STATUS_OK;
}

static void GPA_LoggingCallback(GPA_Logging_Type messageType, const char *pMessage)
{
  if(messageType == GPA_LOGGING_ERROR)
  {
    RDCWARN(pMessage);
  }
  else
  {
    RDCLOG(pMessage);
  }
}

#define GPA_ERROR(text, status) \
  RDCERR(text ". %s", m_pGPUPerfAPI->GPA_GetStatusAsStr((GPA_Status)status));

AMDCounters::AMDCounters() : m_pGPUPerfAPI(NULL)
{
}

bool AMDCounters::Init(ApiType apiType, void *pContext)
{
#if DISABLED(RDOC_WIN32) && DISABLED(RDOC_LINUX)
  return false;
#else

  std::string dllName("GPUPerfAPI");

  switch(apiType)
  {
    case ApiType::Dx11: dllName += "DX11"; break;
    case ApiType::Dx12: dllName += "DX12"; break;
    case ApiType::Ogl: dllName += "GL"; break;
    case ApiType::Vk: dllName += "VK"; break;
    default:
      RDCWARN(
          "AMD GPU performance counters could not be initialized successfully. "
          "Unsupported API type specified");
      return false;
  }

#if ENABLED(RDOC_WIN32)
#if ENABLED(RDOC_X64)
  dllName += "-x64";
#endif
  dllName += ".dll";
#else
  dllName = "lib" + dllName;
  dllName += ".so";
#endif

  // first try in the plugin location it will be in distributed builds
  std::string dllPath = LocatePluginFile("amd/counters", dllName.c_str());

  void *module = Process::LoadModule(dllPath.c_str());
  if(module == NULL)
  {
    module = Process::LoadModule(dllName.c_str());
  }

  if(module == NULL)
  {
    RDCWARN(
        "AMD GPU performance counters could not be initialized successfully. "
        "Are you missing the DLLs?");
    return false;
  }

  GPA_GetFuncTablePtrType getFuncTable =
      (GPA_GetFuncTablePtrType)Process::GetFunctionAddress(module, "GPA_GetFuncTable");

  m_pGPUPerfAPI = new GPAApi();
  if(getFuncTable)
  {
    getFuncTable((void **)&m_pGPUPerfAPI);
  }
  else
  {
    delete m_pGPUPerfAPI;
    RDCERR("Failed to get GPA function table. Invalid dynamic library?");
    return false;
  }

  GPA_Logging_Type loggingType = GPA_LOGGING_ERROR;
#if ENABLED(RDOC_DEVEL)
  loggingType = GPA_LOGGING_ERROR_AND_MESSAGE;
#endif
  GPA_Status status = m_pGPUPerfAPI->GPA_RegisterLoggingCallback(loggingType, GPA_LoggingCallback);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Failed to initialize logging", status);
    return false;
  }

  status = m_pGPUPerfAPI->GPA_Initialize();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Initialization failed", status);
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  status = m_pGPUPerfAPI->GPA_OpenContext(
      pContext, GPA_OPENCONTEXT_HIDE_SOFTWARE_COUNTERS_BIT | GPA_OPENCONTEXT_CLOCK_MODE_PEAK_BIT);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Open context for counters failed", status);
    m_pGPUPerfAPI->GPA_Destroy();
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  m_Counters = EnumerateCounters();

  return true;
#endif
}

AMDCounters::~AMDCounters()
{
  if(m_pGPUPerfAPI)
  {
    GPA_Status status = m_pGPUPerfAPI->GPA_CloseContext();
    if(AMD_FAILED(status))
    {
      GPA_ERROR("Close context failed", status);
    }

    status = m_pGPUPerfAPI->GPA_Destroy();
    if(AMD_FAILED(status))
    {
      GPA_ERROR("Destroy failed", status);
    }

    delete m_pGPUPerfAPI;
  }
}

std::map<uint32_t, CounterDescription> AMDCounters::EnumerateCounters()
{
  std::map<uint32_t, CounterDescription> counters;

  gpa_uint32 num;

  GPA_Status status = m_pGPUPerfAPI->GPA_GetNumCounters(&num);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get number of counters", status);
    return counters;
  }

  for(uint32_t i = 0; i < num; ++i)
  {
    GPA_Usage_Type usageType;

    status = m_pGPUPerfAPI->GPA_GetCounterUsageType(i, &usageType);
    if(AMD_FAILED(status))
    {
      GPA_ERROR("Get counter usage type.", status);
      return counters;
    }

    // Ignore percentage counters due to aggregate roll-up support
    if(usageType == GPA_USAGE_TYPE_PERCENTAGE)
    {
      continue;
    }

    CounterDescription desc = InternalGetCounterDescription(i);

    desc.counter = MakeAMDCounter(i);
    counters[i] = desc;

    m_PublicToInternalCounter[desc.counter] = i;
  }

  return counters;
}

std::vector<GPUCounter> AMDCounters::GetPublicCounterIds() const
{
  std::vector<GPUCounter> ret;

  for(const std::pair<GPUCounter, uint32_t> &entry : m_PublicToInternalCounter)
    ret.push_back(entry.first);

  return ret;
}

CounterDescription AMDCounters::GetCounterDescription(GPUCounter counter)
{
  return m_Counters[m_PublicToInternalCounter[counter]];
}

CounterDescription AMDCounters::InternalGetCounterDescription(uint32_t internalIndex)
{
  CounterDescription desc = {};
  const char *tmp = NULL;
  GPA_Status status = m_pGPUPerfAPI->GPA_GetCounterName(internalIndex, &tmp);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter name.", status);
    return desc;
  }

  desc.name = tmp;
  status = m_pGPUPerfAPI->GPA_GetCounterDescription(internalIndex, &tmp);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter description.", status);
    return desc;
  }

  desc.description = tmp;
  status = m_pGPUPerfAPI->GPA_GetCounterCategory(internalIndex, &tmp);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter category.", status);
    return desc;
  }

  desc.category = tmp;

  GPA_Usage_Type usageType;

  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter usage type.", status);
    return desc;
  }

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

  status = m_pGPUPerfAPI->GPA_GetCounterDataType(internalIndex, &type);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter data type.", status);
    return desc;
  }

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
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];

  GPA_Status status = m_pGPUPerfAPI->GPA_EnableCounter(internalIndex);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Enable counter.", status);
  }
}

void AMDCounters::EnableAllCounters()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EnableAllCounters();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Enable all counters.", status);
  }
}

void AMDCounters::DisableAllCounters()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_DisableAllCounters();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Disable all counters.", status);
  }
}

uint32_t AMDCounters::GetPassCount()
{
  gpa_uint32 numRequiredPasses = 0;
  GPA_Status status = m_pGPUPerfAPI->GPA_GetPassCount(&numRequiredPasses);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get pass count.", status);
  }

  return (uint32_t)numRequiredPasses;
}

uint32_t AMDCounters::BeginSession()
{
  gpa_uint32 sessionID = 0;

  GPA_Status status = m_pGPUPerfAPI->GPA_BeginSession(&sessionID);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Begin session.", status);
  }

  return (uint32_t)sessionID;
}

void AMDCounters::EndSesssion()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EndSession();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("End session.", status);
  }
}

std::vector<CounterResult> AMDCounters::GetCounterData(uint32_t sessionID, uint32_t maxSampleIndex,
                                                       const std::vector<uint32_t> &eventIDs,
                                                       const std::vector<GPUCounter> &counters)
{
  std::vector<CounterResult> ret;

  bool isReady = false;

  const uint32_t timeoutPeriod = 10000;    // ms

  PerformanceTimer timeout;

  do
  {
    isReady = IsSessionReady(sessionID);
    if(!isReady)
    {
      Threading::Sleep(0);

      PerformanceTimer endTime;

      if(timeout.GetMilliseconds() > timeoutPeriod)
      {
        GPA_LoggingCallback(GPA_LOGGING_ERROR, "GetCounterData failed due to elapsed timeout.");
        return ret;
      }
    }
  } while(!isReady);

  for(uint32_t s = 0; s < maxSampleIndex; s++)
  {
    for(size_t c = 0; c < counters.size(); c++)
    {
      const CounterDescription desc = GetCounterDescription(counters[c]);

      switch(desc.resultType)
      {
        case CompType::UInt:
        {
          if(desc.resultByteWidth == sizeof(uint32_t))
          {
            uint32_t value = GetSampleUint32(sessionID, s, counters[c]);

            if(desc.unit == CounterUnit::Percentage)
            {
              value = RDCCLAMP(value, 0U, 100U);
            }

            ret.push_back(CounterResult(eventIDs[s], counters[c], value));
          }
          else if(desc.resultByteWidth == sizeof(uint64_t))
          {
            uint64_t value = GetSampleUint64(sessionID, s, counters[c]);

            if(desc.unit == CounterUnit::Percentage)
            {
              value = RDCCLAMP(value, (uint64_t)0, (uint64_t)100);
            }

            ret.push_back(CounterResult(eventIDs[s], counters[c], value));
          }
          else
          {
            RDCERR("Unexpected byte width %u", desc.resultByteWidth);
          }
        }
        break;
        case CompType::Float:
        {
          float value = GetSampleFloat32(sessionID, s, counters[c]);

          if(desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, 0.0f, 100.0f);
          }

          ret.push_back(CounterResult(eventIDs[s], counters[c], value));
        }
        break;
        case CompType::Double:
        {
          double value = GetSampleFloat64(sessionID, s, counters[c]);

          if(desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, 0.0, 100.0);
          }

          ret.push_back(CounterResult(eventIDs[s], counters[c], value));
        }
        break;
        default: RDCASSERT(0); break;
      };
    }
  }

  return ret;
}

bool AMDCounters::IsSessionReady(uint32_t sessionIndex)
{
  gpa_uint8 readyResult = 0;

  GPA_Status status = m_pGPUPerfAPI->GPA_IsSessionReady(&readyResult, sessionIndex);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Is session ready", status);
  }

  return readyResult && status == GPA_STATUS_OK;
}

void AMDCounters::BeginPass()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_BeginPass();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Begin pass.", status);
  }
}

void AMDCounters::EndPass()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EndPass();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("End pass.", status);
  }
}

void AMDCounters::BeginSample(uint32_t index)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_BeginSample(index);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Begin sample.", status);
  }
}

void AMDCounters::EndSample()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EndSample();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("End sample.", status);
  }
}

void AMDCounters::BeginSampleList(void *pSampleList)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_BeginSampleList(pSampleList);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("BeginSampleList.", status);
  }
}

void AMDCounters::EndSampleList(void *pSampleList)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EndSampleList(pSampleList);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("EndSampleList.", status);
  }
}

void AMDCounters::BeginSampleInSampleList(uint32_t sampleID, void *pSampleList)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_BeginSampleInSampleList(sampleID, pSampleList);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("BeginSampleInSampleList.", status);
  }
}

void AMDCounters::EndSampleInSampleList(void *pSampleList)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EndSampleInSampleList(pSampleList);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("EndSampleInSampleList.", status);
  }
}

uint32_t AMDCounters::GetSampleUint32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];
  uint32_t value = 0;

  GPA_Status status = m_pGPUPerfAPI->GPA_GetSampleUInt32(session, sample, internalIndex, &value);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get sample uint32.", status);
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter usage type.", status);
    return value;
  }

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
  {
    value *= 1000;
  }

  return value;
}

uint64_t AMDCounters::GetSampleUint64(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];
  gpa_uint64 value = 0;

  GPA_Status status = m_pGPUPerfAPI->GPA_GetSampleUInt64(session, sample, internalIndex, &value);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get sample uint64.", status);
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter usage type.", status);
    return value;
  }

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
  {
    value *= 1000;
  }

  return value;
}

float AMDCounters::GetSampleFloat32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];
  float value = 0;

  GPA_Status status = m_pGPUPerfAPI->GPA_GetSampleFloat32(session, sample, internalIndex, &value);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get sample float32.", status);
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter usage type.", status);
    return value;
  }

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
  {
    value *= 1000.0f;
  }
  else if(usageType == GPA_USAGE_TYPE_MILLISECONDS)
  {
    value /= 1000.0f;
  }

  return value;
}

double AMDCounters::GetSampleFloat64(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];
  double value;

  GPA_Status status = m_pGPUPerfAPI->GPA_GetSampleFloat64(session, sample, internalIndex, &value);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get sample float64.", status);
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter usage type.", status);
    return value;
  }

  if(usageType == GPA_USAGE_TYPE_KILOBYTES)
  {
    value *= 1000.0;
  }
  else if(usageType == GPA_USAGE_TYPE_MILLISECONDS)
  {
    value /= 1000.0;
  }

  return value;
}