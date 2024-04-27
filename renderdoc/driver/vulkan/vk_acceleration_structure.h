/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include "vk_manager.h"

class WrappedVulkan;

class VulkanAccelerationStructureManager
{
public:
  struct ASMemory
  {
    MemoryAllocation alloc;
    bool isTLAS;
  };

  VulkanAccelerationStructureManager(WrappedVulkan *driver) : m_pDriver(driver) {}

  // Called when the initial state is prepared.  Any TLAS and BLAS data is copied into temporary
  // buffers and the handles for that memory and the buffers is stored in the init state
  bool Prepare(VkAccelerationStructureKHR unwrappedAs, const rdcarray<uint32_t> &queueFamilyIndices,
               ASMemory &result);

  template <typename SerialiserType>
  bool Serialise(SerialiserType &ser, ResourceId id, const VkInitialContents *initial,
                 CaptureState state);

  // Called when the initial state is applied.  The AS data is deserialised from the upload buffer
  // into the acceleration structure
  void Apply(ResourceId id, const VkInitialContents &initial);

private:
  VkDeviceSize SerialisedASSize(VkAccelerationStructureKHR unwrappedAs);

  WrappedVulkan *m_pDriver;
};
