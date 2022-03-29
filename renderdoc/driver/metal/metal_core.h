/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_common.h"

struct MetalInitParams
{
  MetalInitParams();
  void Set(MTL::Device *pDevice, ResourceId inst);

  // update this when adding/removing members
  uint64_t GetSerialiseSize();

  // check if a frame capture section version is supported
  static const uint64_t CurrentVersion = 0x1;

  // device information
  NS::String *name;
  uint64_t recommendedMaxWorkingSetSize;
  uint64_t maxTransferRate;
  uint64_t registryID;
  uint64_t peerGroupID;
  uint32_t peerCount;
  uint32_t peerIndex;
  MTL::DeviceLocation location;
  NS::UInteger locationNumber;
  bool hasUnifiedMemory;
  bool headless;
  bool lowPower;
  bool removable;

  // device capabilities
  bool supportsMTLGPUFamilyCommon1;
  bool supportsMTLGPUFamilyCommon2;
  bool supportsMTLGPUFamilyCommon3;

  bool supportsMTLGPUFamilyApple1;
  bool supportsMTLGPUFamilyApple2;
  bool supportsMTLGPUFamilyApple3;
  bool supportsMTLGPUFamilyApple4;
  bool supportsMTLGPUFamilyApple5;
  bool supportsMTLGPUFamilyApple6;
  bool supportsMTLGPUFamilyApple7;
  bool supportsMTLGPUFamilyApple8;

  bool supportsMTLGPUFamilyMac1;
  bool supportsMTLGPUFamilyMac2;

  bool supportsMTLGPUFamilyMacCatalyst1;
  bool supportsMTLGPUFamilyMacCatalyst2;

  MTL::ArgumentBuffersTier argumentBuffersSupport;

  ResourceId InstanceID;
};

DECLARE_REFLECTION_STRUCT(MetalInitParams);
