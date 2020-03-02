/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Samsung Electronics (UK) Limited
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

#include "hwcpipe_api.hpp"

namespace lizard
{
HwcPipeApi::HwcPipeApi(std::vector<LizardCounter> &availableCounters,
                       LizardCounterDataStore &dataStore)
    : m_availableCounters(availableCounters), m_data(dataStore), m_HwcPipe(NULL)
{
}

HwcPipeApi::~HwcPipeApi()
{
  if(m_HwcPipe)
  {
    delete m_HwcPipe;
  }
}

template <typename TID, typename TINFO, typename THASH>
LizardCounter createCounter(uint64_t counterId, TID hwcpipeId,
                            const std::unordered_map<std::string, TID> &names,
                            const std::unordered_map<TID, TINFO, THASH> &info,
                            const std::string &category, const LizardCounter::SourceType sourceType)
{
  std::string title;
  for(auto &i : names)
  {
    if(i.second == hwcpipeId)
    {
      title = i.first;
      break;
    }
  }
  std::string desc = (info.at(hwcpipeId)).desc;
  std::string unit = (info.at(hwcpipeId)).unit;
  std::string key = "";
  std::string name = "";
  LizardCounter cnt = LizardCounter(
      ++counterId, key.c_str(), name.c_str(), title.c_str(), desc.c_str(), category.c_str(), 1,
      unit.compare("B") == 0 ? LizardCounter::UNITS_BYTE : LizardCounter::UNITS_UNKNOWN,
      LizardCounter::CLASS_ABSOLUTE, sourceType);
  cnt.setInternalKey((uint64_t)hwcpipeId);
  return cnt;
}

bool HwcPipeApi::init(uint32_t &counterId)
{
  m_HwcPipe = new hwcpipe::HWCPipe();

  uint64_t counterNum = counterId;

  if(m_HwcPipe->cpu_profiler())
  {
    for(hwcpipe::CpuCounter hwcpipeId : m_HwcPipe->cpu_profiler()->supported_counters())
    {
      m_availableCounters.push_back(createCounter(++counterId, hwcpipeId, hwcpipe::cpu_counter_names,
                                                  hwcpipe::cpu_counter_info, "HWCPipe CPU Counter",
                                                  LizardCounter::SOURCE_HWCPIPE_CPU));
    }
  }

  if(m_HwcPipe->gpu_profiler())
  {
    for(hwcpipe::GpuCounter hwcpipeId : m_HwcPipe->gpu_profiler()->supported_counters())
    {
      m_availableCounters.push_back(createCounter(++counterId, hwcpipeId, hwcpipe::gpu_counter_names,
                                                  hwcpipe::gpu_counter_info, "HWCPipe GPU Counter",
                                                  LizardCounter::SOURCE_HWCPIPE_GPU));
    }
  }

  return counterId > counterNum;
}

void HwcPipeApi::enableCounters(const std::vector<LizardCounter> &counters)
{
  hwcpipe::CpuCounterSet cpuCounterSet;
  hwcpipe::GpuCounterSet gpuCounterSet;

  for(LizardCounter counter : counters)
  {
    switch(counter.sourceType())
    {
      case LizardCounter::SourceType::SOURCE_HWCPIPE_CPU:
        cpuCounterSet.insert((hwcpipe::CpuCounter)counter.internalKey());
        break;
      case LizardCounter::SourceType::SOURCE_HWCPIPE_GPU:
        gpuCounterSet.insert((hwcpipe::GpuCounter)counter.internalKey());
        break;
      default: break;
    }
  }

  m_HwcPipe->set_enabled_cpu_counters(cpuCounterSet);
  m_HwcPipe->set_enabled_gpu_counters(gpuCounterSet);
}

void HwcPipeApi::startCapture()
{
  m_HwcPipe->run();
  m_HwcPipe->sample();
}

void HwcPipeApi::stopCapture()
{
  m_HwcPipe->stop();
}

void HwcPipeApi::readMessage()
{
  hwcpipe::Measurements measurements = m_HwcPipe->sample();
  if(measurements.cpu)
  {
    for(const std::pair<hwcpipe::CpuCounter, hwcpipe::Value> &data : *measurements.cpu)
    {
      for(LizardCounter &counter : m_availableCounters)
      {
        if(counter.sourceType() == LizardCounter::SourceType::SOURCE_HWCPIPE_CPU &&
           counter.internalKey() == (uint64_t)data.first)
        {
          Value value;
          value.as_double = data.second.get<long long>();
          m_data.addValue(counter.id(), value);
          break;
        }
      }
    }
  }

  if(measurements.gpu)
  {
    for(const std::pair<hwcpipe::GpuCounter, hwcpipe::Value> &data : *measurements.gpu)
    {
      for(LizardCounter &counter : m_availableCounters)
      {
        if(counter.sourceType() == LizardCounter::SourceType::SOURCE_HWCPIPE_GPU &&
           counter.internalKey() == (uint64_t)data.first)
        {
          Value value;
          value.as_double = data.second.get<double>();
          m_data.addValue(counter.id(), value);
          break;
        }
      }
    }
  }
}
}
