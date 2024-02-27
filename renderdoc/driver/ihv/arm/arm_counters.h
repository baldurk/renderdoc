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

#pragma once

#include <map>
#include "api/replay/data_types.h"
#include "api/replay/renderdoc_replay.h"
#include "api/replay/replay_enums.h"
#include "common/common.h"

struct LizardApi;
typedef void *LizardInstance;

class ARMCounters
{
public:
  ARMCounters();
  ~ARMCounters();

  bool Init();
  rdcarray<GPUCounter> GetPublicCounterIds();
  CounterDescription GetCounterDescription(GPUCounter index);

  void EnableCounter(GPUCounter counter);
  void DisableAllCounters();

  uint32_t GetPassCount();

  void BeginPass(uint32_t passID);
  void EndPass();

  void BeginSample(uint32_t eventId);
  void EndSample();

  rdcarray<CounterResult> GetCounterData(const rdcarray<uint32_t> &eventIDs,
                                         const rdcarray<GPUCounter> &counters);

private:
#if ENABLED(RDOC_ANDROID)
  struct LizardApi *m_Api;
  LizardInstance m_Ctx;

  uint32_t m_EventId;
  uint32_t m_passIndex;

  rdcarray<uint32_t> m_EnabledCounters;
  rdcarray<CounterDescription> m_CounterDescriptions;
  rdcarray<GPUCounter> m_CounterIds;
  std::map<uint32_t, std::map<uint32_t, CounterValue>> m_CounterData;
#endif
};
