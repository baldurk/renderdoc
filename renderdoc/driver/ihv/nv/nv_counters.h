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
    return mDescriptors[LocalId];
  }

private:
  bool Init(void);

  void *mNvPmLib;
  struct _NvPmApi *mNvPmApi;
  uint64_t mNvPmCtx;

  std::vector<GPUCounter> mExternalIds;
  std::vector<uint32_t> mInternalIds;
  std::vector<CounterDescription> mDescriptors;
};
