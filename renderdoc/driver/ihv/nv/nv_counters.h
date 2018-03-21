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

  std::vector<GPUCounter> GetPublicCounterIds() const { return mExternalIds; }

  CounterDescription GetCounterDescription(GPUCounter counterID) const
  {
    const uint32_t LocalId =
        static_cast<uint32_t>(counterID) - static_cast<uint32_t>(GPUCounter::FirstNvidia);
    return mExternalDescriptors[LocalId];
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

  void *mNvPmLib;
  struct _NvPmApi *mNvPmApi;
  uint64_t mNvPmCtx;
  uint32_t mObjectsCount;

  std::vector<GPUCounter> mExternalIds;
  std::vector<uint32_t> mInternalIds;
  std::vector<GPUCounter> mSelectedExternalIds;
  std::vector<uint32_t> mSelectedInternalIds;
  std::vector<CounterDescription> mExternalDescriptors;
  std::vector<uint32_t> mInternalDescriptors;
};
