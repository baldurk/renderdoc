/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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

#include <stdint.h>
#include <vector>
#include "api/replay/renderdoc_replay.h"

struct ID3D11Device;

class NVCounters
{
public:
  NVCounters();
  ~NVCounters();

  bool Init(ID3D11Device *pDevice);

  std::vector<GPUCounter> GetPublicCounterIds() const { return m_ExternalIds; }
  CounterDescription GetCounterDescription(GPUCounter counterID) const
  {
    const uint32_t LocalId = (uint32_t)counterID - (uint32_t)GPUCounter::FirstNvidia;
    return m_ExternalDescriptors[LocalId];
  }

  bool PrepareExperiment(const std::vector<GPUCounter> &counters, uint32_t objectsCount);

  // returns num passes
  uint32_t BeginExperiment() const;
  void EndExperiment(const std::vector<uint32_t> &eventIds, std::vector<CounterResult> &Result) const;

  void BeginPass(uint32_t passIdx) const;
  void EndPass(uint32_t passIdx) const;

  void BeginSample(uint32_t sampleIdx) const;
  void EndSample(uint32_t sampleIdx) const;

private:
  bool Init(void);

  void *m_NvPmLib;
  struct _NvPmApi *m_NvPmApi;
  uint64_t m_NvPmCtx;
  uint32_t m_ObjectsCount;

  std::vector<GPUCounter> m_ExternalIds;
  std::vector<uint32_t> m_InternalIds;
  std::vector<GPUCounter> m_SelectedExternalIds;
  std::vector<uint32_t> m_SelectedInternalIds;
  std::vector<CounterDescription> m_ExternalDescriptors;
  std::vector<uint32_t> m_InternalDescriptors;
};
