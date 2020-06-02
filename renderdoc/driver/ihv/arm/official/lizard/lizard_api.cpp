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

#include <lizard_api.h>

#include <cstring>
#include <map>

#include <gator_constants.hpp>
#include <lizard.hpp>

static LizardInstance LZD_Init(const char *host, int port)
{
  lizard::Lizard *lzd = new lizard::Lizard();

  bool configured = lzd->configure(host, port);

  if(!configured)
  {
    delete lzd;
    return NULL;
  }

  return static_cast<LizardInstance>(lzd);
}

static void LZD_Destroy(LizardInstance ctx)
{
  delete static_cast<lizard::Lizard *>(ctx);
}

static uint32_t LZD_GetAvailableCountersCount(LizardInstance ctx)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);

  if(lzd == NULL)
  {
    return 0;
  }

  return lzd->availableCountersCount();
}

static LZD_Result LZD_GetCounterDescription(LizardInstance ctx, LizardCounterId id,
                                            LizardCounterDescription *desc)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);

  if(lzd == NULL || desc == NULL)
  {
    return LZD_FAILURE;
  }

  uint32_t count = lzd->availableCountersCount();

  if(id == 0 || id > count)
  {
    return LZD_FAILURE;
  }

  const lizard::LizardCounter *lzdCounter = lzd->availableCounters() + (id - 1);

  desc->id = lzdCounter->id();
  desc->short_name = lzdCounter->key();
  desc->title = lzdCounter->title();
  desc->name = lzdCounter->name();
  desc->category = lzdCounter->category();
  desc->description = lzdCounter->description();
  desc->multiplier = lzdCounter->multiplier();

  switch(lzdCounter->classType())
  {
    case lizard::LizardCounter::CLASS_ABSOLUTE: desc->class_type = LZD_ABSOLUTE; break;
    case lizard::LizardCounter::CLASS_DELTA: desc->class_type = LZD_DELTA; break;
    default: break;
  }

  switch(lzdCounter->units())
  {
    case lizard::LizardCounter::UNITS_BYTE: desc->units = LZD_UNITS_BYTE; break;
    case lizard::LizardCounter::UNITS_CELSIUS: desc->units = LZD_UNITS_CELSIUS; break;
    case lizard::LizardCounter::UNITS_MHZ:
    case lizard::LizardCounter::UNITS_HZ: desc->units = LZD_UNITS_HZ; break;
    case lizard::LizardCounter::UNITS_PAGES: desc->units = LZD_UNITS_PAGES; break;
    case lizard::LizardCounter::UNITS_RPM: desc->units = LZD_UNITS_RPM; break;
    case lizard::LizardCounter::UNITS_S: desc->units = LZD_UNITS_S; break;
    case lizard::LizardCounter::UNITS_V: desc->units = LZD_UNITS_V; break;
    default: desc->units = LZD_UNITS_UNKNOWN; break;
  }

  if(lzdCounter->multiplier() == 1 && lzdCounter->sourceType() == lizard::LizardCounter::SOURCE_GATORD)
  {
    desc->result_type = LZD_TYPE_INT;
  }
  else
  {
    desc->result_type = LZD_TYPE_DOUBLE;
  }

  return LZD_OK;
}

static void LZD_EnableCounter(LizardInstance ctx, LizardCounterId id)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);
  if(lzd != NULL)
  {
    lzd->enableCounters(&id, 1);
  }
}

static void LZD_DisableCounter(LizardInstance ctx, LizardCounterId id)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);
  if(lzd != NULL)
  {
    lzd->disableCounters(&id, 1);
  }
}

static void LZD_DisableAllCounters(LizardInstance ctx)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);
  if(lzd != NULL)
  {
    uint32_t count = lzd->availableCountersCount();
    const lizard::LizardCounter *counters = lzd->availableCounters();

    for(uint32_t idx = 0; idx < count; idx++)
    {
      lizard::LizardCounterId id = counters[idx].id();
      lzd->disableCounters(&id, 1);
    }
  }
}

static LZD_Result LZD_StartCapture(LizardInstance ctx)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);
  if(lzd == NULL)
  {
    return LZD_FAILURE;
  }
  bool result = lzd->startCapture();
  return result ? LZD_OK : LZD_FAILURE;
}

static LZD_Result LZD_StopCapture(LizardInstance ctx)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);
  if(lzd == NULL)
  {
    return LZD_FAILURE;
  }
  lzd->endCapture();
  return LZD_OK;
}

static int64_t LZD_ReadCounterInt(LizardInstance ctx, LizardCounterId id)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);

  if(lzd == NULL)
  {
    return 0;
  }

  size_t size = lzd->readCounterInt(id, nullptr);

  std::vector<int64_t> values(size);

  size = lzd->readCounterInt(id, values.data());

  int64_t result = 0;

  for(size_t idx = 0; idx < size; idx++)
  {
    result += values[idx];
  }

  if(lzd->getCounterInfo(id)->classType() == lizard::LizardCounter::CLASS_ABSOLUTE && size != 0)
  {
    result /= size;
  }

  if(lzd->getCounterInfo(id)->units() == lizard::LizardCounter::UNITS_MHZ)
  {
    result *= 1000000;
  }

  return result * lzd->getCounterInfo(id)->multiplier();
}

static double LZD_ReadCounterDouble(LizardInstance ctx, LizardCounterId id)
{
  lizard::Lizard *lzd = static_cast<lizard::Lizard *>(ctx);

  if(lzd == NULL)
  {
    return 0;
  }

  size_t size = lzd->readCounterDouble(id, nullptr);

  std::vector<double> values(size);

  size = lzd->readCounterDouble(id, values.data());

  double result = 0;

  for(size_t idx = 0; idx < size; idx++)
  {
    result += values[idx];
  }

  if(lzd->getCounterInfo(id)->classType() == lizard::LizardCounter::CLASS_ABSOLUTE && size != 0)
  {
    result /= size;
  }

  return result * lzd->getCounterInfo(id)->multiplier();
}

static struct LizardApi s_ApiInstance;

static void InitApi(void)
{
  s_ApiInstance.struct_size = sizeof(struct LizardApi);
  s_ApiInstance.version = LIZARD_VERSION_0_1;
  s_ApiInstance.Init = &LZD_Init;
  s_ApiInstance.Destroy = &LZD_Destroy;
  s_ApiInstance.GetAvailableCountersCount = &LZD_GetAvailableCountersCount;
  s_ApiInstance.GetCounterDescription = &LZD_GetCounterDescription;
  s_ApiInstance.EnableCounter = &LZD_EnableCounter;
  s_ApiInstance.DisableCounter = &LZD_DisableCounter;
  s_ApiInstance.DisableAllCounters = &LZD_DisableAllCounters;
  s_ApiInstance.StartCapture = &LZD_StartCapture;
  s_ApiInstance.StopCapture = &LZD_StopCapture;
  s_ApiInstance.ReadCounterInt = &LZD_ReadCounterInt;
  s_ApiInstance.ReadCounterDouble = &LZD_ReadCounterDouble;
}

extern "C" LZD_Result LoadApi(struct LizardApi **api)
{
  static bool initialized = false;
  if(!initialized)
  {
    InitApi();
    initialized = true;
  }

  if(api == NULL)
  {
    return LZD_FAILURE;
  }

  *api = &s_ApiInstance;
  return LZD_OK;
}
