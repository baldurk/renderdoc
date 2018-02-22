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

#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include "amd_counters.h"
#include "common/common.h"
#include "core/plugins.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPI.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPIFunctionTypes.h"
#include "strings/string_utils.h"

#if ENABLED(RDOC_WIN32)
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

inline bool AMD_FAILED(GPA_Status status)
{
  return status != GPA_STATUS_OK;
}

inline bool AMD_SUCCEEDED(GPA_Status status)
{
  return status == GPA_STATUS_OK;
}

static void GPA_LoggingCallback(GPA_Logging_Type messageType, const char* pMessage)
{
  std::string message;

  switch (messageType)
  {
  case GPA_LOGGING_ERROR:
    message = "Error: ";
    break;
  case GPA_LOGGING_MESSAGE:
    message = "Info: ";
    break;
  case GPA_LOGGING_TRACE:
    message = "Trace: ";
    break;
  default:
    break;
  }

  message += "AMD: ";

  message.append(pMessage);

  if (messageType == GPA_LOGGING_ERROR)
  {
    RDCWARN(message.c_str());
  }
  else
  {
    RDCLOG(message.c_str());
  }

#if ENABLED(RDOC_DEVEL)
#if ENABLED(RDOC_WIN32)
  ::OutputDebugStringA(message.c_str());
  ::OutputDebugStringA("\n");
#else
  std::cerr << message.c_str() << "\n";
#endif
#endif
}

AMDCounters::AMDCounters() : m_pGPUPerfAPI(NULL)
{
}

std::string AMDCounters::FormatErrMessage(const char* operation, uint32_t status)
{
  std::ostringstream o;
  o << operation << ". " << m_pGPUPerfAPI->GPA_GetStatusAsStr((GPA_Status)status);
  return o.str();
}

bool AMDCounters::Init(ApiType apiType, void *pContext)
{
  std::string dllName("GPUPerfAPI");

  switch (apiType)
  {
  case AMDCounters::eApiType_Dx11:
    dllName += "DX11";
    break;
  case AMDCounters::eApiType_Dx12:
    dllName += "DX12";
    break;
  case AMDCounters::eApiType_Ogl:
    dllName += "GL";
    break;
  case AMDCounters::eApiType_Vk:
    dllName += "VK";
    break;
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



  GPA_GetFuncTablePtrType getFuncTable = NULL;

#if ENABLED(RDOC_WIN32)
  // first try in the plugin location it will be in distributed builds
  HMODULE module = LoadLibraryA(LocatePluginFile("amd/counters", dllName.c_str()).c_str());

  // if that failed then try checking for it just in the default search path
  if(module == NULL)
  {
    module = LoadLibraryA(dllName.c_str());
  }

  if(module == NULL)
  {
    RDCWARN(
        "AMD GPU performance counters could not be initialized successfully. "
        "Are you missing the DLLs?");
    return false;
  }

  getFuncTable = (GPA_GetFuncTablePtrType)GetProcAddress(module, "GPA_GetFuncTable");
#else
  // first try in the plugin location it will be in distributed builds
  void* module = dlopen(LocatePluginFile("amd/counters", dllName.c_str()).c_str(), RTLD_LAZY);

    // if that failed then try checking for it just in the default search path
    if(module == NULL)
    {
      module = dlopen(dllName.c_str(), RTLD_LAZY);
    }

    if(module == NULL)
    {
      RDCWARN(
          "AMD GPU performance counters could not be initialized successfully. "
          "Are you missing the DLLs?");
      return false;
    }

    getFuncTable = (GPA_GetFuncTablePtrType)dlsym(module, "GPA_GetFuncTable");
#endif

  m_pGPUPerfAPI = new GPAApi();
  if(getFuncTable)
  {
    getFuncTable((void **)&m_pGPUPerfAPI);
  }
  else
  {
    delete m_pGPUPerfAPI;
    GPA_LoggingCallback(GPA_LOGGING_ERROR, "Failed to get GPA function table. Invalid dynamic library?");
    return false;
  }

  GPA_Logging_Type loggingType = GPA_LOGGING_ERROR;
#if ENABLED(RDOC_DEVEL)
  loggingType = GPA_LOGGING_ERROR_AND_MESSAGE;
#endif
  auto status = m_pGPUPerfAPI->GPA_RegisterLoggingCallback(loggingType, GPA_LoggingCallback);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Failed to initialize logging", status).c_str());
    return false;
  }

  status = m_pGPUPerfAPI->GPA_Initialize();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Initialization failed", status).c_str());
    delete m_pGPUPerfAPI;
    m_pGPUPerfAPI = NULL;
    return false;
  }

  status = m_pGPUPerfAPI->GPA_OpenContext(pContext, GPA_OPENCONTEXT_HIDE_SOFTWARE_COUNTERS_BIT | GPA_OPENCONTEXT_CLOCK_MODE_PEAK_BIT);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Open context for counters failed", status).c_str());
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
    auto status = m_pGPUPerfAPI->GPA_CloseContext();
    if (AMD_FAILED(status))
    {
      GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Close context failed", status).c_str());
    }

    status = m_pGPUPerfAPI->GPA_Destroy();
    if (AMD_FAILED(status))
    {
      GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Destroy failed", status).c_str());
    }

    delete m_pGPUPerfAPI;
  }
}

std::map<uint32_t, CounterDescription> AMDCounters::EnumerateCounters()
{
  std::map<uint32_t, CounterDescription> counters;

  gpa_uint32 num;

  auto status = m_pGPUPerfAPI->GPA_GetNumCounters(&num);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get number of counters", status).c_str());
    return counters;
  }

  for(uint32_t i = 0; i < num; ++i)
  {
    GPA_Usage_Type usageType;

    status = m_pGPUPerfAPI->GPA_GetCounterUsageType(i, &usageType);
    if (AMD_FAILED(status))
    {
      GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter usage type.", status).c_str());
      return counters;
    }

    // Ignore percentage counters due to aggregate roll-up support
    if (usageType == GPA_USAGE_TYPE_PERCENTAGE)
    {
      continue;
    }

    CounterDescription desc = InternalGetCounterDescription(i);

    desc.counter = MakeAMDCounter(i);
    counters[i] = desc;

    m_PublicToInternalCounter[desc.counter] = i;
  }

  return std::move(counters);
}

uint32_t AMDCounters::GetNumCounters()
{
  return (uint32_t)m_Counters.size();
}

CounterDescription AMDCounters::GetCounterDescription(GPUCounter counter)
{
    return m_Counters[m_PublicToInternalCounter[counter]];
}

CounterDescription AMDCounters::InternalGetCounterDescription(uint32_t internalIndex)
{
  CounterDescription desc = {};
  const char *tmp = NULL;
  auto status = m_pGPUPerfAPI->GPA_GetCounterName(internalIndex, &tmp);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter name.", status).c_str());
    return desc;
  }

  desc.name = tmp;
  status = m_pGPUPerfAPI->GPA_GetCounterDescription(internalIndex, &tmp);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter description.", status).c_str());
    return desc;
  }

  desc.description = tmp;
  status = m_pGPUPerfAPI->GPA_GetCounterCategory(internalIndex, &tmp);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter category.", status).c_str());
    return desc;
  }

  desc.category = tmp;

  GPA_Usage_Type usageType;

  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter usage type.", status).c_str());
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
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter data type.", status).c_str());
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

  if (m_pGPUPerfAPI->GPA_GetCounterUuid)
  {
      status = m_pGPUPerfAPI->GPA_GetCounterUuid(internalIndex, (GPA_UUID*)&desc.uuid);
      if (AMD_FAILED(status))
      {
          GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter UUID.", status).c_str());
          return desc;
      }
  }
  else
  {
      // C8958C90-B706-4F22-8AF5-E0A3831B2C39
      desc.uuid.words[0] = 0xC8958C90;
      desc.uuid.words[1] = 0xB7064F22;
      desc.uuid.words[2] = 0x8AF5E0A3 ^ strhash(desc.name.c_str());
      desc.uuid.words[3] = 0x831B2C39 ^ strhash(desc.description.c_str());
  }

  return desc;
}

void AMDCounters::EnableCounter(GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];

  auto status = m_pGPUPerfAPI->GPA_EnableCounter(internalIndex);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Enable counter.", status).c_str());
  }
}

void AMDCounters::EnableAllCounters()
{
  auto status = m_pGPUPerfAPI->GPA_EnableAllCounters();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Enable all counters.", status).c_str());
  }
}

void AMDCounters::DisableAllCounters()
{
  auto status = m_pGPUPerfAPI->GPA_DisableAllCounters();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Disable all counters.", status).c_str());
  }
}

uint32_t AMDCounters::GetPassCount()
{
  gpa_uint32 numRequiredPasses = 0;
  auto status = m_pGPUPerfAPI->GPA_GetPassCount(&numRequiredPasses);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get pass count.", status).c_str());
  }

  return (uint32_t)numRequiredPasses;
}

uint32_t AMDCounters::BeginSession()
{
  gpa_uint32 sessionID = 0;

  auto status = m_pGPUPerfAPI->GPA_BeginSession(&sessionID);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Begin session.", status).c_str());
  }

  return (uint32_t)sessionID;
}

void AMDCounters::EndSesssion()
{
  auto status = m_pGPUPerfAPI->GPA_EndSession();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("End session.", status).c_str());
  }
}

std::vector<CounterResult> AMDCounters::GetCounterData(
  uint32_t sessionID,
  uint32_t maxSampleIndex,
  const std::vector<uint32_t> &eventIDs,
  const std::vector<GPUCounter> &counters
)
{
  std::vector<CounterResult> ret;

  bool isReady = false;

  const uint32_t timeout = 10000; // ms

  auto startTime = std::chrono::high_resolution_clock::now();

  do
  {
    isReady = IsSessionReady(sessionID);
    if (!isReady)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(0));

      auto endTime = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> elapsedTime = endTime - startTime;
      if (elapsedTime.count() > timeout)
      {
        GPA_LoggingCallback(GPA_LOGGING_ERROR, "GetCounterData failed due to elapsed timeout.");
        return std::move(ret);
      }
    }
  } while (!isReady);

  for (uint32_t s = 0; s < maxSampleIndex; s++)
  {
    for (size_t c = 0; c < counters.size(); c++)
    {
      const CounterDescription desc = GetCounterDescription(counters[c]);

      switch (desc.resultType)
      {
      case CompType::UInt:
      {
        if (desc.resultByteWidth == sizeof(uint32_t))
        {
          uint32_t value = GetSampleUint32(sessionID, s, counters[c]);

          if (desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, 0U, 100U);
          }

          ret.push_back(CounterResult(eventIDs[s], counters[c], value));
        }
        else if (desc.resultByteWidth == sizeof(uint64_t))
        {
          uint64_t value = GetSampleUint64(sessionID, s, counters[c]);

          if (desc.unit == CounterUnit::Percentage)
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

        if (desc.unit == CounterUnit::Percentage)
        {
          value = RDCCLAMP(value, 0.0f, 100.0f);
        }

        ret.push_back(CounterResult(eventIDs[s], counters[c], value));
      }
      break;
      case CompType::Double:
      {
        double value = GetSampleFloat64(sessionID, s, counters[c]);

        if (desc.unit == CounterUnit::Percentage)
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

  return std::move(ret);
}

bool AMDCounters::IsSessionReady(uint32_t sessionIndex)
{
  gpa_uint8 readyResult = 0;

  auto status = m_pGPUPerfAPI->GPA_IsSessionReady(&readyResult, sessionIndex);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Is session ready", status).c_str());
  }

  return readyResult && status == GPA_STATUS_OK;
}

void AMDCounters::BeginPass()
{
  auto status = m_pGPUPerfAPI->GPA_BeginPass();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Begin pass.", status).c_str());
  }
}

void AMDCounters::EndPass()
{
  auto status = m_pGPUPerfAPI->GPA_EndPass();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("End pass.", status).c_str());
  }
}

void AMDCounters::BeginSample(uint32_t index)
{
  auto status = m_pGPUPerfAPI->GPA_BeginSample(index);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Begin sample.", status).c_str());
  }
}

void AMDCounters::EndSample()
{
  auto status = m_pGPUPerfAPI->GPA_EndSample();
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("End sample.", status).c_str());
  }
}

void AMDCounters::BeginSampleList(void * pSampleList)
{
  auto status = m_pGPUPerfAPI->GPA_BeginSampleList(pSampleList);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("BeginSampleList.", status).c_str());
  }
}

void AMDCounters::EndSampleList(void * pSampleList)
{
  auto status = m_pGPUPerfAPI->GPA_EndSampleList(pSampleList);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("EndSampleList.", status).c_str());
  }
}

void AMDCounters::BeginSampleInSampleList(uint32_t sampleID, void * pSampleList)
{
  auto status = m_pGPUPerfAPI->GPA_BeginSampleInSampleList(sampleID, pSampleList);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("BeginSampleInSampleList.", status).c_str());
  }
}

void AMDCounters::EndSampleInSampleList(void * pSampleList)
{
  auto status = m_pGPUPerfAPI->GPA_EndSampleInSampleList(pSampleList);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("EndSampleInSampleList.", status).c_str());
  }
}

uint32_t AMDCounters::GetSampleUint32(uint32_t session, uint32_t sample, GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];
  uint32_t value = 0;

  auto status = m_pGPUPerfAPI->GPA_GetSampleUInt32(session, sample, internalIndex, &value);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get sample uint32.", status).c_str());
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter usage type.", status).c_str());
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

  auto status = m_pGPUPerfAPI->GPA_GetSampleUInt64(session, sample, internalIndex, &value);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get sample uint64.", status).c_str());
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter usage type.", status).c_str());
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

  auto status = m_pGPUPerfAPI->GPA_GetSampleFloat32(session, sample, internalIndex, &value);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get sample float32.", status).c_str());
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter usage type.", status).c_str());
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

  auto status = m_pGPUPerfAPI->GPA_GetSampleFloat64(session, sample, internalIndex, &value);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get sample float64.", status).c_str());
    return value;
  }

  // normalise units as expected
  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(internalIndex, &usageType);
  if (AMD_FAILED(status))
  {
    GPA_LoggingCallback(GPA_LOGGING_ERROR, FormatErrMessage("Get counter usage type.", status).c_str());
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