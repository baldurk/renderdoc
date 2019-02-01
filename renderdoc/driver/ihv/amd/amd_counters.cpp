/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "core/core.h"
#include "core/plugins.h"
#include "official/GPUPerfAPI/Include/GPUPerfAPIFunctionTypes.h"
#include "strings/string_utils.h"

inline bool AMD_FAILED(GPA_Status status)
{
  return status < GPA_STATUS_OK;
}

inline bool AMD_SUCCEEDED(GPA_Status status)
{
  return status >= GPA_STATUS_OK;
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

#define GPA_WARNING(text, status) \
  RDCWARN(text ". %s", m_pGPUPerfAPI->GPA_GetStatusAsStr((GPA_Status)status));

AMDCounters::AMDCounters(bool dx12DebugLayerEnabled)
    : m_pGPUPerfAPI(NULL),
      m_gpaSessionCounter(0u),
      m_passCounter(-1),
      m_dx12DebugLayerEnabled(dx12DebugLayerEnabled)
{
}

bool AMDCounters::Init(ApiType apiType, void *pContext)
{
#if DISABLED(RDOC_WIN32) && DISABLED(RDOC_LINUX)
  (void)m_dx12DebugLayerEnabled;
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

  bool disableCounters = false;

  if(apiType == ApiType::Dx12 && m_dx12DebugLayerEnabled)
  {
    // Disable counters in DX12 Debug configuration
    void *versionFunc = Process::GetFunctionAddress(module, "GPA_GetVersion");

    if(NULL == versionFunc)
    {
      disableCounters = true;
    }
  }

  if(disableCounters)
  {
    RDCLOG("AMD counters are disabled in DX12 Debug version for GPA v3.0");
    return false;
  }

  GPA_GetFuncTablePtrType getFuncTable =
      (GPA_GetFuncTablePtrType)Process::GetFunctionAddress(module, "GPA_GetFuncTable");

  m_pGPUPerfAPI = new GPAFunctionTable();
  if(getFuncTable)
  {
    GPA_Status gpaStatus = getFuncTable((void *)m_pGPUPerfAPI);

    if(AMD_FAILED(gpaStatus))
    {
      RDCERR("Failed to load the GPA function entrypoint.");
      return false;
    }
  }
  else
  {
    SAFE_DELETE(m_pGPUPerfAPI);
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

  status = m_pGPUPerfAPI->GPA_Initialize(GPA_INITIALIZE_DEFAULT_BIT);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Initialization failed", status);
    SAFE_DELETE(m_pGPUPerfAPI);
    return false;
  }

  status = m_pGPUPerfAPI->GPA_OpenContext(
      pContext, GPA_OPENCONTEXT_HIDE_SOFTWARE_COUNTERS_BIT | GPA_OPENCONTEXT_CLOCK_MODE_NONE_BIT,
      &m_gpaContextId);
  if(AMD_FAILED(status))
  {
    GPA_WARNING("Open context for counters failed", status);
    m_pGPUPerfAPI->GPA_Destroy();
    SAFE_DELETE(m_pGPUPerfAPI);
    return false;
  }

  m_Counters = EnumerateCounters();
  m_apiType = apiType;

  status = m_pGPUPerfAPI->GPA_CloseContext(m_gpaContextId);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Close context failed", status);
  }

  m_gpaContextId = NULL;

  return true;
#endif
}

AMDCounters::~AMDCounters()
{
  if(m_pGPUPerfAPI)
  {
    if(m_gpaContextId)
    {
      GPA_Status status = m_pGPUPerfAPI->GPA_CloseContext(m_gpaContextId);
      if(AMD_FAILED(status))
      {
        GPA_ERROR("Close context failed", status);
      }
    }

    GPA_Status status = m_pGPUPerfAPI->GPA_Destroy();
    if(AMD_FAILED(status))
    {
      GPA_ERROR("Destroy failed", status);
    }

    SAFE_DELETE(m_pGPUPerfAPI);
  }
}

std::map<uint32_t, CounterDescription> AMDCounters::EnumerateCounters()
{
  std::map<uint32_t, CounterDescription> counters;

  gpa_uint32 num;
  GPA_Status status = m_pGPUPerfAPI->GPA_GetNumCounters(m_gpaContextId, &num);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get number of counters", status);
    return counters;
  }

  for(uint32_t i = 0; i < num; ++i)
  {
    GPA_Usage_Type usageType;

    status = m_pGPUPerfAPI->GPA_GetCounterUsageType(m_gpaContextId, i, &usageType);
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
  GPA_Status status = m_pGPUPerfAPI->GPA_GetCounterName(m_gpaContextId, internalIndex, &tmp);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter name.", status);
    return desc;
  }

  desc.name = tmp;
  status = m_pGPUPerfAPI->GPA_GetCounterDescription(m_gpaContextId, internalIndex, &tmp);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter description.", status);
    return desc;
  }

  desc.description = tmp;
  status = m_pGPUPerfAPI->GPA_GetCounterGroup(m_gpaContextId, internalIndex, &tmp);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter category.", status);
    return desc;
  }

  desc.category = tmp;

  GPA_Usage_Type usageType;
  status = m_pGPUPerfAPI->GPA_GetCounterUsageType(m_gpaContextId, internalIndex, &usageType);
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
    case GPA_USAGE_TYPE_NANOSECONDS:     ///< Result is in nanoseconds
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

  GPA_Data_Type type;
  status = m_pGPUPerfAPI->GPA_GetCounterDataType(m_gpaContextId, internalIndex, &type);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter data type.", status);
    return desc;
  }

  // results should either be float32/64 or uint32/64 as the GetSample functions only support those
  switch(type)
  {
    case GPA_DATA_TYPE_FLOAT64:    ///< Result will be a 64-bit float
      desc.resultType = CompType::Double;
      desc.resultByteWidth = sizeof(double);
      break;
    case GPA_DATA_TYPE_UINT64:    ///< Result will be a 64-bit unsigned int
      desc.resultType = CompType::UInt;
      desc.resultByteWidth = sizeof(uint64_t);
      break;
    default: desc.resultType = CompType::UInt; desc.resultByteWidth = sizeof(uint32_t);
  }

  status = m_pGPUPerfAPI->GPA_GetCounterUuid(m_gpaContextId, internalIndex, (GPA_UUID *)&desc.uuid);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get counter UUID.", status);
    return desc;
  }

  return desc;
}

void AMDCounters::EnableCounter(GPUCounter counter)
{
  const uint32_t internalIndex = m_PublicToInternalCounter[counter];

  GPA_Status status = m_pGPUPerfAPI->GPA_EnableCounter(m_gpaSessionInfo.back(), internalIndex);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Enable counter.", status);
  }
}

void AMDCounters::EnableAllCounters()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EnableAllCounters(m_gpaSessionInfo.back());
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Enable all counters.", status);
  }
}

void AMDCounters::DisableAllCounters()
{
  GPA_Status status = m_pGPUPerfAPI->GPA_DisableAllCounters(m_gpaSessionInfo.back());
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Disable all counters.", status);
  }
}

bool AMDCounters::BeginMeasurementMode(ApiType apiType, void *pContext)
{
  RDCASSERT(apiType == m_apiType);
  RDCASSERT(pContext);
  RDCASSERT(!m_gpaContextId);

  GPA_Status status = m_pGPUPerfAPI->GPA_OpenContext(
      pContext, GPA_OPENCONTEXT_HIDE_SOFTWARE_COUNTERS_BIT | GPA_OPENCONTEXT_CLOCK_MODE_PEAK_BIT,
      &m_gpaContextId);
  if(AMD_FAILED(status))
  {
    GPA_WARNING("Creating context for analysis failed", status);
    return false;
  }

  return true;
}

void AMDCounters::EndMeasurementMode()
{
  if(m_gpaContextId)
  {
    GPA_Status status = m_pGPUPerfAPI->GPA_CloseContext(m_gpaContextId);
    if(AMD_FAILED(status))
    {
      GPA_ERROR("Close context failed", status);
    }

    m_gpaContextId = NULL;
  }
}

uint32_t AMDCounters::GetPassCount()
{
  gpa_uint32 numRequiredPasses = 0;
  GPA_Status status = m_pGPUPerfAPI->GPA_GetPassCount(m_gpaSessionInfo.back(), &numRequiredPasses);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get pass count.", status);
  }

  return (uint32_t)numRequiredPasses;
}

uint32_t AMDCounters::CreateSession()
{
  uint32_t sessionID = m_gpaSessionCounter;
  GPA_SessionId gpaSessionId = NULL;
  GPA_Status status = m_pGPUPerfAPI->GPA_CreateSession(
      m_gpaContextId, GPA_SESSION_SAMPLE_TYPE_DISCRETE_COUNTER, &gpaSessionId);

  if(AMD_FAILED(status))
  {
    GPA_ERROR("Create session.", status);
  }
  else
  {
    InitializeCmdInfo();
    m_gpaSessionInfo.push_back(gpaSessionId);
    ++m_gpaSessionCounter;
  }

  return sessionID;
}

void AMDCounters::BeginSession(uint32_t sessionId)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_BeginSession(m_gpaSessionInfo.at(sessionId));
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Begin session.", status);
  }
  else
  {
    m_passCounter = -1;
  }
}

void AMDCounters::EndSesssion(uint32_t sessionId)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_EndSession(m_gpaSessionInfo.at(sessionId));
  if(AMD_FAILED(status))
  {
    GPA_ERROR("End session.", status);
  }

  m_passCounter = 0u;
}

void AMDCounters::InitializeCmdInfo()
{
  switch(m_apiType)
  {
    case ApiType::Dx11:
    case ApiType::Ogl: break;
    case ApiType::Vk:
    case ApiType::Dx12:
      if(NULL == m_gpaCmdListInfo.m_pCommandListMap)
      {
        m_gpaCmdListInfo.m_pCommandListMap = new std::map<void *, GPA_CommandListId>();
      }
      break;
  }
}

void AMDCounters::DeInitializeCmdInfo()
{
  switch(m_apiType)
  {
    case ApiType::Dx11:
    case ApiType::Ogl: break;
    case ApiType::Vk:
    case ApiType::Dx12: SAFE_DELETE(m_gpaCmdListInfo.m_pCommandListMap); break;
  }
}

void AMDCounters::DeleteSession(uint32_t sessionId)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_DeleteSession(m_gpaSessionInfo.at(sessionId));
  DeInitializeCmdInfo();
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Create session.", status);
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

      if(timeout.GetMilliseconds() > timeoutPeriod)
      {
        GPA_LoggingCallback(GPA_LOGGING_ERROR, "GetCounterData failed due to elapsed timeout.");
        return ret;
      }
    }
  } while(!isReady);

  GPA_SessionId gpaSessionId = m_gpaSessionInfo.at(sessionID);
  size_t sampleResultSize = 0u;
  GPA_Status status = m_pGPUPerfAPI->GPA_GetSampleResultSize(gpaSessionId, 0, &sampleResultSize);

  if(AMD_FAILED(status))
  {
    GPA_ERROR("Get Sample Result Size", status);
  }

  void *pSampleResult = malloc(sampleResultSize);

  for(uint32_t s = 0; s < maxSampleIndex; s++)
  {
    status = m_pGPUPerfAPI->GPA_GetSampleResult(gpaSessionId, s, sampleResultSize, pSampleResult);

    if(AMD_FAILED(status))
    {
      GPA_ERROR("Get Sample Result ", status);
    }

    for(size_t c = 0; c < counters.size(); c++)
    {
      const CounterDescription desc = GetCounterDescription(counters[c]);
      const uint32_t internalIndex = m_PublicToInternalCounter[counters[c]];

      GPA_Usage_Type usageType;
      status = m_pGPUPerfAPI->GPA_GetCounterUsageType(m_gpaContextId, internalIndex, &usageType);

      if(AMD_FAILED(status))
      {
        GPA_ERROR("Get counter usage type.", status);
      }

      switch(desc.resultType)
      {
        case CompType::UInt:
        {
          uint64_t value = 0;

          memcpy(&value, (uint64_t *)(pSampleResult) + c, sizeof(uint64_t));
          // normalise units as expected
          if(usageType == GPA_USAGE_TYPE_KILOBYTES)
          {
            value *= 1000;
          }

          if(desc.unit == CounterUnit::Percentage)
          {
            value = RDCCLAMP(value, (uint64_t)0ULL, (uint64_t)100ULL);
          }
          ret.push_back(CounterResult(eventIDs[s], counters[c], (uint64_t)value));
        }
        break;
        case CompType::Double:
        {
          double value = 0.0;
          memcpy(&value, (double *)(pSampleResult) + c, sizeof(double));

          // normalise units as expected
          if(usageType == GPA_USAGE_TYPE_KILOBYTES)
          {
            value *= 1000.0;
          }
          else if(usageType == GPA_USAGE_TYPE_MILLISECONDS)
          {
            value /= 1000.0;
          }
          else if(usageType == GPA_USAGE_TYPE_NANOSECONDS)
          {
            value /= 1.0e+9;
          }

          ret.push_back(CounterResult(eventIDs[s], counters[c], value));
        }
        break;
        default: RDCASSERT(0); break;
      };
    }
  }

  free(pSampleResult);
  DeleteSession(sessionID);

  return ret;
}

bool AMDCounters::IsSessionReady(uint32_t sessionIndex)
{
  GPA_Status status = m_pGPUPerfAPI->GPA_IsSessionComplete(m_gpaSessionInfo.at(sessionIndex));
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Is session ready", status);
  }

  return status == GPA_STATUS_OK;
}

void AMDCounters::BeginPass()
{
  m_passCounter++;

  if(m_apiType == ApiType::Dx12 || m_apiType == ApiType::Vk)
  {
    if(NULL != m_gpaCmdListInfo.m_pCommandListMap)
    {
      m_gpaCmdListInfo.m_pCommandListMap->clear();
    }
  }
}

void AMDCounters::EndPass()
{
  bool isReady = false;

  const uint32_t timeoutPeriod = 10000;    // ms

  PerformanceTimer timeout;

  do
  {
    isReady =
        GPA_STATUS_OK == m_pGPUPerfAPI->GPA_IsPassComplete(m_gpaSessionInfo.back(), m_passCounter);
    if(!isReady)
    {
      Threading::Sleep(0);

      PerformanceTimer endTime;

      if(timeout.GetMilliseconds() > timeoutPeriod)
      {
        GPA_LoggingCallback(GPA_LOGGING_ERROR, "GPA_IsPassComplete failed due to elapsed timeout.");
        break;
      }
    }
  } while(!isReady);
}

void AMDCounters::BeginSample(uint32_t sampleID, void *pCommandList)
{
  GPA_CommandListId startingSampleCmd = GPA_NULL_COMMAND_LIST;

  switch(m_apiType)
  {
    case ApiType::Dx11:
    case ApiType::Ogl: startingSampleCmd = m_gpaCmdListInfo.m_gpaCommandListId; break;
    case ApiType::Vk:
    case ApiType::Dx12:
      startingSampleCmd = m_gpaCmdListInfo.m_pCommandListMap->at(pCommandList);
      break;
  }

  GPA_Status status = m_pGPUPerfAPI->GPA_BeginSample(sampleID, startingSampleCmd);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("Begin sample.", status);
  }
}

void AMDCounters::EndSample(void *pCommandList)
{
  GPA_CommandListId endingSampleCmd = GPA_NULL_COMMAND_LIST;

  switch(m_apiType)
  {
    case ApiType::Dx11:
    case ApiType::Ogl: endingSampleCmd = m_gpaCmdListInfo.m_gpaCommandListId; break;
    case ApiType::Vk:
    case ApiType::Dx12:
      endingSampleCmd = m_gpaCmdListInfo.m_pCommandListMap->at(pCommandList);
      break;
  }

  GPA_Status status = m_pGPUPerfAPI->GPA_EndSample(endingSampleCmd);

  if(AMD_FAILED(status))
  {
    GPA_ERROR("End sample.", status);
  }
}

void AMDCounters::BeginCommandList(void *pCommandList)
{
  void *cmdList = NULL;
  GPA_Command_List_Type cmdType = GPA_COMMAND_LIST_NONE;
  switch(m_apiType)
  {
    case ApiType::Dx11:
    case ApiType::Ogl: cmdList = GPA_NULL_COMMAND_LIST; break;
    case ApiType::Vk:
    case ApiType::Dx12:
      cmdList = pCommandList;
      cmdType = GPA_COMMAND_LIST_PRIMARY;
      break;
  }

  GPA_CommandListId gpaCmdId = NULL;
  GPA_Status status = m_pGPUPerfAPI->GPA_BeginCommandList(m_gpaSessionInfo.back(), m_passCounter,
                                                          cmdList, cmdType, &gpaCmdId);

  if(AMD_FAILED(status))
  {
    GPA_ERROR("BeginCommandList.", status);
  }
  else
  {
    switch(m_apiType)
    {
      case ApiType::Dx11:
      case ApiType::Ogl: m_gpaCmdListInfo.m_gpaCommandListId = gpaCmdId; break;
      case ApiType::Vk:
      case ApiType::Dx12:
        m_gpaCmdListInfo.m_pCommandListMap->insert(
            std::pair<void *, GPA_CommandListId>(cmdList, gpaCmdId));
        break;
    }
  }
}

void AMDCounters::EndCommandList(void *pCommandList)
{
  GPA_CommandListId endingCmd = GPA_NULL_COMMAND_LIST;

  switch(m_apiType)
  {
    case ApiType::Dx11:
    case ApiType::Ogl: endingCmd = m_gpaCmdListInfo.m_gpaCommandListId; break;
    case ApiType::Vk:
    case ApiType::Dx12: endingCmd = m_gpaCmdListInfo.m_pCommandListMap->at(pCommandList); break;
  }

  GPA_Status status = m_pGPUPerfAPI->GPA_EndCommandList(endingCmd);
  if(AMD_FAILED(status))
  {
    GPA_ERROR("EndCommandList.", status);
  }
}
