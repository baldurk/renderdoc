/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "nv_counter_enumerator.h"

#include "common/common.h"
#include "common/formatting.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

#if ENABLED(RDOC_WIN32)
#include "windows-desktop-x64/nvperf_host_impl.h"
#elif ENABLED(RDOC_LINUX)
#include "linux-desktop-x64/nvperf_host_impl.h"
#endif

#include "NvPerfCounterConfiguration.h"
#include "NvPerfCounterData.h"
#include "NvPerfMetricsEvaluator.h"

struct NVCounterEnumerator::Impl
{
public:
  nv::perf::MetricsEvaluator Evaluator;

  nv::perf::CounterConfiguration SelectedConfiguration;    // configImage etc. for the current selection
  rdcarray<GPUCounter> SelectedExternalIds;
  rdcarray<NVPW_MetricEvalRequest> SelectedEvalRequests;
  size_t SelectedNumPasses;

  const rdcarray<GPUCounter> &ExternalIds()
  {
    InitEnumerateCounters();
    return m_ExternalIds;
  }
  const rdcarray<CounterDescription> &ExternalDescriptions()
  {
    InitEnumerateCounters();
    return m_ExternalDescriptions;
  }
  const rdcarray<NVPW_MetricEvalRequest> &AllEvalRequests()
  {
    InitEnumerateCounters();
    return m_AllEvalRequests;
  }

private:
  void InitEnumerateCounters();
  bool m_EnumerationDone = false;

  rdcarray<GPUCounter> m_ExternalIds;
  rdcarray<CounterDescription> m_ExternalDescriptions;
  rdcarray<NVPW_MetricEvalRequest> m_AllEvalRequests;
};

NVCounterEnumerator::NVCounterEnumerator()
{
  m_Impl = new NVCounterEnumerator::Impl();
}

NVCounterEnumerator::~NVCounterEnumerator()
{
  delete m_Impl;
}

static CounterUnit ToCounterUnit(const std::vector<NVPW_DimUnitFactor> &dimUnits)
{
  if(dimUnits.size() == 0)
  {
    return CounterUnit::Ratio;
  }
  if(dimUnits.size() == 1 && dimUnits[0].exponent == 1)
  {
    switch(dimUnits[0].dimUnit)
    {
      case NVPW_DIM_UNIT_BYTES: return CounterUnit::Bytes;
      case NVPW_DIM_UNIT_SECONDS: return CounterUnit::Seconds;
      case NVPW_DIM_UNIT_PERCENT: return CounterUnit::Percentage;
      case NVPW_DIM_UNIT_FBP_CYCLES: return CounterUnit::Cycles;
      case NVPW_DIM_UNIT_GPC_CYCLES: return CounterUnit::Cycles;
      case NVPW_DIM_UNIT_SYS_CYCLES: return CounterUnit::Cycles;
      case NVPW_DIM_UNIT_DRAM_CYCLES: return CounterUnit::Cycles;
      case NVPW_DIM_UNIT_PCIE_CYCLES:
        return CounterUnit::Cycles;
        // fallthrough...
    }
  }

  // catch-all
  return CounterUnit::Absolute;
}

bool NVCounterEnumerator::Init(nv::perf::MetricsEvaluator &&metricsEvaluator)
{
  m_Impl->Evaluator = std::move(metricsEvaluator);

  return true;
}

void NVCounterEnumerator::Impl::InitEnumerateCounters()
{
  // Defer counter enumeration until the first time this is called
  if(m_EnumerationDone)
    return;

  m_EnumerationDone = true;

  struct MetricAttribute
  {
    NVPW_MetricType metricType;
    NVPW_RollupOp rollupOp;
    NVPW_Submetric submetric;
  };
  const MetricAttribute metricAttributes[] = {
      {NVPW_METRIC_TYPE_COUNTER, NVPW_ROLLUP_OP_SUM, NVPW_SUBMETRIC_NONE},
      {NVPW_METRIC_TYPE_COUNTER, NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_NONE},
      {NVPW_METRIC_TYPE_COUNTER, NVPW_ROLLUP_OP_MAX, NVPW_SUBMETRIC_NONE},
      {NVPW_METRIC_TYPE_COUNTER, NVPW_ROLLUP_OP_MIN, NVPW_SUBMETRIC_NONE},
      {NVPW_METRIC_TYPE_RATIO, NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_RATIO},
      {NVPW_METRIC_TYPE_RATIO, NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_MAX_RATE},
      {NVPW_METRIC_TYPE_RATIO, NVPW_ROLLUP_OP_AVG, NVPW_SUBMETRIC_PCT},
  };
  for(size_t i = 0; i < sizeof(metricAttributes) / sizeof(metricAttributes[0]); i++)
  {
    const auto &attributes = metricAttributes[i];
    NVPW_MetricType metricType = attributes.metricType;
    NVPW_RollupOp rollupOp = attributes.rollupOp;
    NVPW_Submetric submetric = attributes.submetric;

    for(const char *counterName : nv::perf::EnumerateMetrics(Evaluator, metricType))
    {
      if(strstr(counterName, "Triage") != NULL)
        continue;    // filter out Triage counters (they are all duplicates)

      size_t metricIndex;
      if(!nv::perf::GetMetricTypeAndIndex(Evaluator, counterName, metricType, metricIndex))
        continue;
      RDCASSERT(metricType == attributes.metricType);

      NVPW_MetricEvalRequest evalReq = {};
      evalReq.metricIndex = metricIndex;
      evalReq.metricType = (uint8_t)metricType;
      evalReq.rollupOp = (uint8_t)rollupOp;
      evalReq.submetric = (uint16_t)submetric;

      std::vector<NVPW_DimUnitFactor> dimUnits;
      GetMetricDimUnits(Evaluator, evalReq, dimUnits);

      {
        //-----------------
        // Filter out metrics that count "cycles".
        // The RenderDoc replay loop is not designed for reproducing representative cycle counts.
        auto itr =
            std::find_if(dimUnits.begin(), dimUnits.end(), [](const NVPW_DimUnitFactor &factor) {
              switch(factor.dimUnit)
              {
                case NVPW_DIM_UNIT_DRAM_CYCLES:
                case NVPW_DIM_UNIT_FBP_CYCLES:
                case NVPW_DIM_UNIT_GPC_CYCLES:
                case NVPW_DIM_UNIT_NVLRX_CYCLES:
                case NVPW_DIM_UNIT_NVLTX_CYCLES:
                case NVPW_DIM_UNIT_PCIE_CYCLES:
                case NVPW_DIM_UNIT_SYS_CYCLES: return true;
                default: break;
              }
              return false;
            });
        if(itr != dimUnits.end())
          continue;
      }

      CounterDescription desc = {};
      desc.resultType = CompType::Float;
      desc.resultByteWidth = 8;

      //-----------------
      // Counter name, including rollup and submetric qualifiers
      desc.name = counterName;
      desc.name.append(nv::perf::ToCString((NVPW_RollupOp)evalReq.rollupOp));
      desc.name.append(nv::perf::ToCString((NVPW_Submetric)evalReq.submetric));

      //-----------------
      // Counter description, including metric type and dim unit
      desc.description = rdcstr(GetMetricDescription(Evaluator, metricType, metricIndex));
      desc.description.append("<br/>HW Unit: <em>");
      NVPW_HwUnit hwunit = nv::perf::GetMetricHwUnit(Evaluator, metricType, metricIndex);
      desc.description.append(nv::perf::ToCString(Evaluator, hwunit));
      desc.description.append("</em>");
      desc.description.append("<br/>MetricType: <em>");
      desc.description.append(nv::perf::ToCString(metricType));
      desc.description.append("</em>");
      desc.description.append("<br/>RollupOp: <em>");
      desc.description.append(nv::perf::ToCString(rollupOp));
      desc.description.append("</em>");
      desc.description.append("<br/>Submetric: <em>");
      desc.description.append(nv::perf::ToCString(submetric));
      desc.description.append("</em>");
      desc.description.append("<br/>DimUnit: <em>");
      desc.description.append(
          nv::perf::ToString(dimUnits, [this](NVPW_DimUnitName dimUnit, bool plural) {
            return ToCString(Evaluator, dimUnit, plural);
          }).c_str());
      desc.description.append("</em>");

      //-----------------
      // Categorize counter by DimUnit
      desc.category =
          rdcstr(nv::perf::ToString(dimUnits, [this](NVPW_DimUnitName dimUnit, bool plural) {
                   return ToCString(Evaluator, dimUnit, plural);
                 }).c_str());

      //-----------------
      // Convert Perf SDK units to Renderdoc units (only works for limited subset of units)
      desc.unit = ToCounterUnit(dimUnits);

      //-----------------
      // Assign external counter ID and UUID
      GPUCounter counterID =
          GPUCounter((uint32_t)GPUCounter::FirstNvidia + (uint32_t)m_AllEvalRequests.size());
      desc.counter = counterID;
      desc.uuid.words[0] = 0x25B624D0;
      desc.uuid.words[1] = 0x33244527;
      desc.uuid.words[2] = 0x9F71CD67;
      desc.uuid.words[3] = 0x61B37980 ^ strhash(desc.name.c_str());

      m_ExternalIds.push_back(counterID);
      m_ExternalDescriptions.push_back(desc);
      m_AllEvalRequests.push_back(evalReq);
    }
  }

  //-----------------
  // Sort counter IDs by category and name so that counters appear sorted in the selection UI
  std::sort(m_ExternalIds.begin(), m_ExternalIds.end(),
            [this](const GPUCounter &a, const GPUCounter &b) {
              uint32_t a_localId = (uint32_t)a - (uint32_t)GPUCounter::FirstNvidia;
              uint32_t b_localId = (uint32_t)b - (uint32_t)GPUCounter::FirstNvidia;
              const CounterDescription &a_desc = m_ExternalDescriptions[a_localId];
              const CounterDescription &b_desc = m_ExternalDescriptions[b_localId];
              int result = strcmp(a_desc.category.c_str(), b_desc.category.c_str());
              if(result < 0)
                return true;
              if(result > 0)
                return false;
              result = strcmp(a_desc.name.c_str(), b_desc.name.c_str());
              if(result < 0)
                return true;
              return false;
            });
}

rdcarray<GPUCounter> NVCounterEnumerator::GetPublicCounterIds()
{
  return m_Impl->ExternalIds();
}

CounterDescription NVCounterEnumerator::GetCounterDescription(GPUCounter counterID)
{
  uint32_t LocalId = (uint32_t)counterID - (uint32_t)GPUCounter::FirstNvidia;
  return m_Impl->ExternalDescriptions()[LocalId];
}

bool NVCounterEnumerator::HasCounter(GPUCounter counterID)
{
  uint32_t LocalId = (uint32_t)counterID - (uint32_t)GPUCounter::FirstNvidia;
  return LocalId < m_Impl->ExternalDescriptions().size();
}

bool NVCounterEnumerator::CreateConfig(const char *pChipName,
                                       NVPA_RawMetricsConfig *pRawMetricsConfig,
                                       const rdcarray<GPUCounter> &counters)
{
  nv::perf::MetricsConfigBuilder metricsConfigBuilder;
  if(!metricsConfigBuilder.Initialize(m_Impl->Evaluator, pRawMetricsConfig, pChipName))
  {
    RDCERR("NvPerf failed to initialize config builder");
    return false;
  }

  for(GPUCounter counterID : counters)
  {
    RDCASSERT(IsNvidiaCounter(counterID));
    if(!IsNvidiaCounter(counterID))
    {
      continue;
    }
    size_t counterIndex = (uint32_t)counterID - (uint32_t)GPUCounter::FirstNvidia;
    const NVPW_MetricEvalRequest &evalReq = m_Impl->AllEvalRequests()[counterIndex];

    m_Impl->SelectedExternalIds.push_back(counterID);
    m_Impl->SelectedEvalRequests.push_back(m_Impl->AllEvalRequests()[counterIndex]);
    if(!metricsConfigBuilder.AddMetrics(&evalReq, 1))
    {
      // std::string metricName = nv::perf::ToString(m_Impl->Evaluator, evalReq);
      const char *metricName = nv::perf::ToCString(
          m_Impl->Evaluator, (NVPW_MetricType)evalReq.metricType, evalReq.metricIndex);
      RDCERR("NvPerf failed to configure metric: %s", metricName);
    }
  }

  if(!metricsConfigBuilder.PrepareConfigImage())
  {
    RDCERR("NvPerf failed to prepare config image");
    return false;
  }

  size_t configImageSize = metricsConfigBuilder.GetConfigImageSize();
  size_t counterDataPrefixSize = metricsConfigBuilder.GetCounterDataPrefixSize();
  m_Impl->SelectedConfiguration.configImage.resize(configImageSize);
  m_Impl->SelectedConfiguration.counterDataPrefix.resize(counterDataPrefixSize);
  metricsConfigBuilder.GetConfigImage(m_Impl->SelectedConfiguration.configImage.size(),
                                      m_Impl->SelectedConfiguration.configImage.data());
  metricsConfigBuilder.GetCounterDataPrefix(m_Impl->SelectedConfiguration.counterDataPrefix.size(),
                                            m_Impl->SelectedConfiguration.counterDataPrefix.data());
  m_Impl->SelectedNumPasses = metricsConfigBuilder.GetNumPasses();
  return true;
}

void NVCounterEnumerator::GetConfig(const uint8_t *&pConfigImage, size_t &configImageSize,
                                    const uint8_t *&pCounterDataPrefix, size_t &counterDataPrefixSize)
{
  pConfigImage = m_Impl->SelectedConfiguration.configImage.data();
  configImageSize = m_Impl->SelectedConfiguration.configImage.size();
  pCounterDataPrefix = m_Impl->SelectedConfiguration.counterDataPrefix.data();
  counterDataPrefixSize = m_Impl->SelectedConfiguration.counterDataPrefix.size();
}

void NVCounterEnumerator::ClearConfig()
{
  m_Impl->SelectedExternalIds.clear();
  m_Impl->SelectedEvalRequests.clear();
  m_Impl->SelectedConfiguration = {};    // clear the byte vectors
  m_Impl->SelectedNumPasses = 0u;
}

size_t NVCounterEnumerator::GetMaxNumReplayPasses(uint16_t numNestingLevels)
{
  // Calculate max number of replay passes
  RDCASSERT(m_Impl->SelectedNumPasses > 0u);
  return (size_t)numNestingLevels * m_Impl->SelectedNumPasses + 1u;
}

bool NVCounterEnumerator::EvaluateMetrics(const uint8_t *counterDataImage,
                                          size_t counterDataImageSize,
                                          rdcarray<CounterResult> &values)
{
  bool setDeviceSuccess = nv::perf::MetricsEvaluatorSetDeviceAttributes(
      m_Impl->Evaluator, counterDataImage, counterDataImageSize);
  if(!setDeviceSuccess)
  {
    RDCERR("NvPerf failed to determine device attributes from counter data");
    return false;
  }

  size_t numRanges = nv::perf::CounterDataGetNumRanges(counterDataImage);

  std::vector<double> doubleValues;
  doubleValues.resize(m_Impl->SelectedEvalRequests.size());
  for(uint32_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
  {
    const char *leafRangeName = NULL;
    std::string rangeName = nv::perf::profiler::CounterDataGetRangeName(
        counterDataImage, rangeIndex, '/', &leafRangeName);
    if(!leafRangeName)
    {
      RDCERR("Failed to access NvPerf range name");
      continue;
    }
    errno = 0;
    uint32_t eid = (uint32_t)strtoul(leafRangeName, NULL, 10);
    if(errno != 0)
    {
      RDCERR("Failed to parse NvPerf range name: %s", leafRangeName);
      continue;
    }

    bool evalSuccess =
        nv::perf::EvaluateToGpuValues(m_Impl->Evaluator, counterDataImage, counterDataImageSize,
                                      rangeIndex, m_Impl->SelectedEvalRequests.size(),
                                      m_Impl->SelectedEvalRequests.data(), doubleValues.data());
    if(!evalSuccess)
    {
      RDCERR("NvPerf failed to evaluate GPU metrics for range: %s", leafRangeName);
      continue;
    }
    for(size_t counterIndex = 0; counterIndex < m_Impl->SelectedExternalIds.size(); ++counterIndex)
    {
      CounterResult counterResult(eid, m_Impl->SelectedExternalIds[counterIndex],
                                  doubleValues[counterIndex]);
      values.push_back(counterResult);
    }
  }

  return true;
}

bool NVCounterEnumerator::InitializeNvPerf()
{
  rdcstr pluginsFolder = FileIO::GetAppFolderFilename("plugins/nv");
  const char *paths[] = {
      pluginsFolder.c_str(),
      "./plugins/nv",
      ".",
  };
  NVPW_SetLibraryLoadPaths_Params params{NVPW_SetLibraryLoadPaths_Params_STRUCT_SIZE};
  params.numPaths = sizeof(paths) / sizeof(paths[0]);
  params.ppPaths = paths;
  NVPA_Status result = NVPW_SetLibraryLoadPaths(&params);
  if(result != NVPA_STATUS_SUCCESS)
  {
    RDCWARN("NvPerf could not set library search path");
  }
  nv::perf::UserLogEnableStderr(false);
  return nv::perf::InitializeNvPerf();
}

CounterDescription NVCounterEnumerator::LibraryNotFoundMessage()
{
  rdcstr pluginPath = FileIO::GetAppFolderFilename(
#if ENABLED(RDOC_WIN32)
      "plugins\\nv\\nvperf_grfx_host.dll"
#elif ENABLED(RDOC_LINUX)
      "plugins/nv/libnvperf_grfx_host.so"
#endif
  );
  if(pluginPath.empty())
  {
    pluginPath =
#if ENABLED(RDOC_WIN32)
        ".\\plugins\\nv\\nvperf_grfx_host.dll"
#elif ENABLED(RDOC_LINUX)
        "./plugins/nv/libnvperf_grfx_host.so"
#endif
        ;
  }

  CounterDescription desc = {};
  desc.resultType = CompType::Typeless;
  desc.resultByteWidth = 0;
  desc.name = "ERROR: Could not find Nsight Perf SDK library";
  desc.description = StringFormat::Fmt(
      "To use these counters, please:"
      "<ol>"
      "<li>download the Nsight Perf SDK from:<br/><a "
      "href=\"https://developer.nvidia.com/nsight-perf-sdk\">https://developer.nvidia.com/"
      "nsight-perf-sdk</a></li>"
      "<li>extract the SDK contents</li>"
      "<li>copy "
#if ENABLED(RDOC_WIN32)
      "the <strong>nvperf_grfx_host.dll</strong> file "
#elif ENABLED(RDOC_LINUX)
      "all the <strong>libnvperf_grfx_host.*</strong> files "
#endif
      "to:<br/><strong>%s</strong></li>"
      "<li>reopen this capture</li>"
      "</ol>",
      pluginPath.c_str());
  desc.unit = CounterUnit::Absolute;
  desc.counter = GPUCounter::FirstNvidia;

  // Create the plugin directory, so user will have somewhere to place the plugin file
  FileIO::CreateParentDirectory(pluginPath);

  return desc;
}
