/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "arm_counters.h"

#include "android/android.h"
#include "common/common.h"
#include "common/formatting.h"
#include "core/plugins.h"
#include "os/os_specific.h"

#include <dlfcn.h>

#include "official/lizard/include/lizard/lizard_api.h"

static CounterDescription ARMCreateCounterDescription(GPUCounter index,
                                                      LizardCounterDescription &lzdDesc)
{
  CounterDescription desc;
  desc.name = StringFormat::Fmt("%s %s", lzdDesc.title, lzdDesc.name);
  desc.counter = GPUCounter(index);
  desc.category = lzdDesc.category;

  if(strlen(lzdDesc.short_name) > 0)
    desc.description = StringFormat::Fmt("%s (%s)", lzdDesc.description, lzdDesc.short_name);
  else
    desc.description = lzdDesc.description;

  desc.resultByteWidth = 8;

  switch(lzdDesc.result_type)
  {
    case LZD_TYPE_INT: desc.resultType = CompType::UInt; break;
    case LZD_TYPE_DOUBLE: desc.resultType = CompType::Float; break;
    default: desc.resultType = CompType::UInt; break;
  }

  switch(lzdDesc.units)
  {
    case LZD_UNITS_BYTE: desc.unit = CounterUnit::Bytes; break;
    case LZD_UNITS_CELSIUS: desc.unit = CounterUnit::Celsius; break;
    case LZD_UNITS_HZ: desc.unit = CounterUnit::Hertz; break;
    case LZD_UNITS_S: desc.unit = CounterUnit::Seconds; break;
    case LZD_UNITS_V: desc.unit = CounterUnit::Volt; break;
    default: desc.unit = CounterUnit::Absolute; break;
  }

  return desc;
}

ARMCounters::ARMCounters() : m_Api(NULL), m_Ctx(0), m_EventId(0), m_passIndex(0)
{
}

ARMCounters::~ARMCounters()
{
  if(m_Ctx)
    m_Api->Destroy(m_Ctx);
}

bool ARMCounters::Init()
{
  if(LoadApi(&m_Api) != LZD_OK)
  {
    RDCLOG("Failed to load Lizard api.");
    return false;
  }

  if(m_Api->version != LIZARD_VERSION_0_1)
  {
    RDCLOG("Lizard version is not supported.");
    return false;
  }

  m_Ctx = m_Api->Init("127.0.0.1", 8080);
  if(!m_Ctx)
  {
    RDCLOG("Failed to initialize Lizard.");
    return false;
  }

  uint32_t count = m_Api->GetAvailableCountersCount(m_Ctx);

  if(count == 0)
  {
    RDCLOG("Couldn't find available ARM counters.");
    m_Api->Destroy(m_Ctx);
    return false;
  }

  for(LizardCounterId idx = 1; idx <= count; idx++)
  {
    struct LizardCounterDescription lzdDesc;

    LZD_Result result = m_Api->GetCounterDescription(m_Ctx, idx, &lzdDesc);

    if(result == LZD_OK)
    {
      CounterDescription desc =
          ARMCreateCounterDescription(GPUCounter((int)GPUCounter::FirstARM + idx), lzdDesc);
      m_CounterDescriptions.push_back(desc);
      m_CounterIds.push_back(desc.counter);
    }
    else
    {
      RDCLOG("Failed to get ARM counter information.");
      m_Api->Destroy(m_Ctx);
      return false;
    }
  }
  return true;
}

rdcarray<GPUCounter> ARMCounters::GetPublicCounterIds()
{
  return m_CounterIds;
}

CounterDescription ARMCounters::GetCounterDescription(GPUCounter index)
{
  return m_CounterDescriptions[(int)index - (int)GPUCounter::FirstARM - 1];
}

void ARMCounters::EnableCounter(GPUCounter counter)
{
  uint32_t id = (uint32_t)counter - (uint32_t)GPUCounter::FirstARM;
  m_EnabledCounters.push_back(id);
}

void ARMCounters::DisableAllCounters()
{
  m_EnabledCounters.clear();
}

uint32_t ARMCounters::GetPassCount()
{
  return 1;
}

void ARMCounters::BeginPass(uint32_t passID)
{
  m_passIndex = passID;
  for(size_t i = 0; i < m_EnabledCounters.size(); i++)
  {
    m_Api->EnableCounter(m_Ctx, m_EnabledCounters[i]);
  }
}

void ARMCounters::EndPass()
{
  for(size_t i = 0; i < m_EnabledCounters.size(); i++)
  {
    m_Api->DisableCounter(m_Ctx, m_EnabledCounters[i]);
  }
}

void ARMCounters::BeginSample(uint32_t eventId)
{
  m_EventId = eventId;
  m_Api->StartCapture(m_Ctx);
}

void ARMCounters::EndSample()
{
  m_Api->StopCapture(m_Ctx);

  for(uint32_t counterId : m_EnabledCounters)
  {
    const CounterDescription &desc = m_CounterDescriptions[counterId - 1];
    CounterValue data;
    data.u64 = 0;
    if(desc.resultType == CompType::UInt)
    {
      data.u64 = m_Api->ReadCounterInt(m_Ctx, counterId);
    }
    else if(desc.resultType == CompType::Float)
    {
      data.d = m_Api->ReadCounterDouble(m_Ctx, counterId);
    }
    m_CounterData[m_EventId][counterId] = data;
  }
}

rdcarray<CounterResult> ARMCounters::GetCounterData(const rdcarray<uint32_t> &eventIDs,
                                                    const rdcarray<GPUCounter> &counters)
{
  rdcarray<CounterResult> result;
  for(size_t i = 0; i < eventIDs.size(); i++)
  {
    uint32_t eventId = eventIDs[i];
    for(size_t j = 0; j < counters.size(); j++)
    {
      GPUCounter counter = counters[j];
      uint32_t counterId = (uint32_t)counter - (uint32_t)GPUCounter::FirstARM;
      const CounterDescription &desc = GetCounterDescription(counter);
      const CounterValue &data = m_CounterData[eventId][counterId];
      if(desc.resultType == CompType::UInt)
      {
        result.push_back(CounterResult(eventId, counter, data.u64));
      }
      else if(desc.resultType == CompType::Float)
      {
        result.push_back(CounterResult(eventId, counter, data.d));
      }
    }
  }
  return result;
}
