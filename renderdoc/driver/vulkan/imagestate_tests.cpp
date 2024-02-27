/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "common/globalconfig.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

#include "vk_resources.h"

#include <stdint.h>

void CheckSubresourceRanges(const ImageState &state, bool expectAspectsSplit,
                            bool expectLevelsSplit, bool expectLayersSplit, bool expectDepthSplit)
{
  rdcarray<VkImageAspectFlags> splitAspects;
  if(expectAspectsSplit)
  {
    for(auto it = ImageAspectFlagIter::begin(state.GetImageInfo().Aspects());
        it != ImageAspectFlagIter::end(); ++it)
    {
      splitAspects.push_back(*it);
    }
  }
  else
  {
    splitAspects.push_back(state.GetImageInfo().Aspects());
  }
  uint32_t splitLevelCount = expectLevelsSplit ? state.GetImageInfo().levelCount : 1;
  uint32_t splitLayerCount = expectLayersSplit ? state.GetImageInfo().layerCount : 1;
  uint32_t splitSliceCount = expectDepthSplit ? state.GetImageInfo().extent.depth : 1;
  size_t splitSize = splitAspects.size() * (size_t)splitLevelCount * (size_t)splitLayerCount *
                     (size_t)splitSliceCount;
  CHECK(state.subresourceStates.size() == splitSize);

  auto substateIt = state.subresourceStates.begin();
  auto aspectIt = splitAspects.begin();
  uint32_t level = 0;
  uint32_t layer = 0;
  uint32_t slice = 0;
  uint32_t index = 0;
  for(; aspectIt != splitAspects.end() && substateIt != state.subresourceStates.end();
      ++substateIt, ++index)
  {
    const ImageSubresourceRange &range = substateIt->range();
    CHECK(range.aspectMask == *aspectIt);

    if(expectLevelsSplit)
    {
      CHECK(range.baseMipLevel == level);
      CHECK(range.levelCount == 1);
    }
    else
    {
      CHECK(range.baseMipLevel == 0);
      CHECK(range.levelCount == (uint32_t)state.GetImageInfo().levelCount);
    }

    if(expectLayersSplit)
    {
      CHECK(range.baseArrayLayer == layer);
      CHECK(range.layerCount == 1);
    }
    else
    {
      CHECK(range.baseArrayLayer == 0);
      CHECK(range.layerCount == (uint32_t)state.GetImageInfo().layerCount);
    }

    if(expectDepthSplit)
    {
      CHECK(range.baseDepthSlice == slice);
      CHECK(range.sliceCount == 1);
    }
    else
    {
      CHECK(range.baseDepthSlice == 0);
      CHECK(range.sliceCount == (uint32_t)state.GetImageInfo().extent.depth);
    }

    ++slice;
    if(slice < splitSliceCount)
      continue;
    slice = 0;

    ++layer;
    if(layer < splitLayerCount)
      continue;
    layer = 0;

    ++level;
    if(level < splitLevelCount)
      continue;
    level = 0;

    ++aspectIt;
  }
  CHECK(index == splitSize);
}

void CheckSubresourceState(const ImageSubresourceState &substate,
                           const ImageSubresourceState &expected)
{
  CHECK(substate.oldQueueFamilyIndex == expected.oldQueueFamilyIndex);
  CHECK(substate.newQueueFamilyIndex == expected.newQueueFamilyIndex);
  CHECK(substate.oldLayout == expected.oldLayout);
  CHECK(substate.newLayout == expected.newLayout);
  CHECK(substate.refType == expected.refType);
}

TEST_CASE("Test ImageState type", "[imagestate]")
{
  ImageTransitionInfo transitionInfo(CaptureState::ActiveCapturing, 0, true);
  VkImage image = (VkImage)123;
  VkFormat format = VK_FORMAT_D16_UNORM_S8_UINT;
  VkExtent3D extent = {100, 100, 13};
  uint16_t levelCount = 11;
  uint32_t layerCount = 17;
  uint16_t sampleCount = 1;
  ImageInfo imageInfo(format, extent, levelCount, layerCount, sampleCount,
                      VK_IMAGE_LAYOUT_UNDEFINED, VK_SHARING_MODE_EXCLUSIVE);

  ImageSubresourceState initSubstate(VK_QUEUE_FAMILY_IGNORED, UNKNOWN_PREV_IMG_LAYOUT,
                                     eFrameRef_None);

  ImageSubresourceState readSubstate(initSubstate);
  readSubstate.oldQueueFamilyIndex = readSubstate.newQueueFamilyIndex = 0;
  readSubstate.refType = eFrameRef_Read;

  SECTION("Initial state")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    CheckSubresourceRanges(state, false, false, false, false);
    CheckSubresourceState(state.subresourceStates.begin()->state(), initSubstate);
  };

  SECTION("Split aspects")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range = imageInfo.FullRange();
    range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    state.RecordUse(range, eFrameRef_Read, 0);

    CheckSubresourceRanges(state, true, false, false, false);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Split mip levels")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range = imageInfo.FullRange();
    range.baseMipLevel = 1;
    range.levelCount = 3;
    state.RecordUse(range, eFrameRef_Read, 0);

    CheckSubresourceRanges(state, false, true, false, false);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().baseMipLevel >= range.baseMipLevel &&
         it->range().baseMipLevel - range.baseMipLevel < range.levelCount)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Split array layers")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range = imageInfo.FullRange();
    range.baseArrayLayer = 3;
    range.layerCount = 5;
    state.RecordUse(range, eFrameRef_Read, 0);

    CheckSubresourceRanges(state, false, false, true, false);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().baseArrayLayer >= range.baseArrayLayer &&
         it->range().baseArrayLayer - range.baseArrayLayer < range.layerCount)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Split depth slices")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range = imageInfo.FullRange();
    range.baseDepthSlice = 1;
    range.sliceCount = 1;
    state.RecordUse(range, eFrameRef_Read, 0);

    CheckSubresourceRanges(state, false, false, false, true);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().baseDepthSlice >= range.baseDepthSlice &&
         it->range().baseDepthSlice - range.baseDepthSlice < range.sliceCount)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Split aspect to depth")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    ImageSubresourceRange aspectRange(imageInfo.FullRange());
    aspectRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    state.RecordUse(aspectRange, eFrameRef_Read, 0);
    CheckSubresourceRanges(state, true, false, false, false);

    ImageSubresourceRange levelRange(imageInfo.FullRange());
    levelRange.baseMipLevel = 0;
    levelRange.levelCount = 1;
    state.RecordUse(levelRange, eFrameRef_PartialWrite, 1);
    CheckSubresourceRanges(state, true, true, false, false);

    ImageSubresourceRange layerRange(imageInfo.FullRange());
    layerRange.baseArrayLayer = 0;
    layerRange.layerCount = 1;
    state.RecordUse(layerRange, eFrameRef_Read, 2);
    CheckSubresourceRanges(state, true, true, true, false);

    ImageSubresourceRange sliceRange(imageInfo.FullRange());
    sliceRange.baseDepthSlice = 0;
    sliceRange.sliceCount = 1;
    state.RecordUse(sliceRange, eFrameRef_CompleteWrite, 3);
    CheckSubresourceRanges(state, true, true, true, true);

    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      ImageSubresourceState substate(initSubstate);
      if(it->range().aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_Read);
        substate.newQueueFamilyIndex = 0;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }
      if(levelRange.baseMipLevel <= it->range().baseMipLevel &&
         it->range().baseMipLevel - levelRange.baseMipLevel < levelRange.levelCount)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_PartialWrite);
        substate.newQueueFamilyIndex = 1;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }
      if(layerRange.baseArrayLayer <= it->range().baseArrayLayer &&
         it->range().baseArrayLayer - layerRange.baseArrayLayer < layerRange.layerCount)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_Read);
        substate.newQueueFamilyIndex = 2;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }
      if(sliceRange.baseDepthSlice <= it->range().baseDepthSlice &&
         it->range().baseDepthSlice - sliceRange.baseDepthSlice < sliceRange.sliceCount)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_CompleteWrite);
        substate.newQueueFamilyIndex = 3;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }

      CheckSubresourceState(it->state(), substate);
    }
  };

  SECTION("Split depth to aspect")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    ImageSubresourceRange sliceRange(imageInfo.FullRange());
    sliceRange.baseDepthSlice = 0;
    sliceRange.sliceCount = 1;
    state.RecordUse(sliceRange, eFrameRef_CompleteWrite, 3);
    CheckSubresourceRanges(state, false, false, false, true);

    ImageSubresourceRange layerRange(imageInfo.FullRange());
    layerRange.baseArrayLayer = 0;
    layerRange.layerCount = 1;
    state.RecordUse(layerRange, eFrameRef_Read, 2);
    CheckSubresourceRanges(state, false, false, true, true);

    ImageSubresourceRange levelRange(imageInfo.FullRange());
    levelRange.baseMipLevel = 0;
    levelRange.levelCount = 1;
    state.RecordUse(levelRange, eFrameRef_PartialWrite, 1);
    CheckSubresourceRanges(state, false, true, true, true);

    ImageSubresourceRange aspectRange(imageInfo.FullRange());
    aspectRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    state.RecordUse(aspectRange, eFrameRef_Read, 0);
    CheckSubresourceRanges(state, true, true, true, true);

    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      ImageSubresourceState substate(initSubstate);
      if(sliceRange.baseDepthSlice <= it->range().baseDepthSlice &&
         it->range().baseDepthSlice - sliceRange.baseDepthSlice < sliceRange.sliceCount)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_CompleteWrite);
        substate.newQueueFamilyIndex = 3;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }
      if(layerRange.baseArrayLayer <= it->range().baseArrayLayer &&
         it->range().baseArrayLayer - layerRange.baseArrayLayer < layerRange.layerCount)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_Read);
        substate.newQueueFamilyIndex = 2;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }
      if(levelRange.baseMipLevel <= it->range().baseMipLevel &&
         it->range().baseMipLevel - levelRange.baseMipLevel < levelRange.levelCount)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_PartialWrite);
        substate.newQueueFamilyIndex = 1;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }
      if(it->range().aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
      {
        substate.refType = ComposeFrameRefs(substate.refType, eFrameRef_Read);
        substate.newQueueFamilyIndex = 0;
        if(substate.oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
          substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex;
      }

      CheckSubresourceState(it->state(), substate);
    }
  };

  SECTION("Single barrier")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range(imageInfo.FullRange());
    range.baseArrayLayer = 1;
    range.layerCount = 1;

    VkImageMemoryBarrier barrier = {
        /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /* pNext = */ NULL,
        /* srcAccessMask = */ 0,
        /* dstAccessMask = */ 0,
        /* oldLayout = */ VK_IMAGE_LAYOUT_UNDEFINED,
        /* newLayout = */ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        /* srcQueueFamilyIndex = */ 0,
        /* dstQueueFamilyIndex = */ 0,
        /* image = */ image,
        /* subresourceRange = */ range,
    };
    state.RecordBarrier(barrier, 0, transitionInfo);
    CheckSubresourceRanges(state, false, false, true, false);

    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      ImageSubresourceState substate(initSubstate);
      if(range.baseArrayLayer <= it->range().baseArrayLayer &&
         it->range().baseArrayLayer - range.baseArrayLayer < range.layerCount)
      {
        substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex = 0;
        substate.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        substate.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }

      CheckSubresourceState(it->state(), substate);
    }
  };

  SECTION("Layout barriers")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range(imageInfo.FullRange());
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier barrier = {
        /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /* pNext = */ NULL,
        /* srcAccessMask = */ 0,
        /* dstAccessMask = */ 0,
        /* oldLayout = */ VK_IMAGE_LAYOUT_UNDEFINED,
        /* newLayout = */ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        /* srcQueueFamilyIndex = */ 0,
        /* dstQueueFamilyIndex = */ 0,
        /* image = */ image,
        /* subresourceRange = */ range,
    };
    state.RecordBarrier(barrier, 0, transitionInfo);
    CheckSubresourceRanges(state, false, false, true, false);

    barrier.subresourceRange.baseArrayLayer = 1;
    state.RecordBarrier(barrier, 0, transitionInfo);
    CheckSubresourceRanges(state, false, false, true, false);

    barrier.subresourceRange.baseArrayLayer = range.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = range.layerCount = 2;
    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    state.RecordBarrier(barrier, 0, transitionInfo);
    CheckSubresourceRanges(state, false, false, true, false);

    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      ImageSubresourceState substate(initSubstate);
      if(range.baseArrayLayer <= it->range().baseArrayLayer &&
         it->range().baseArrayLayer - range.baseArrayLayer < range.layerCount)
      {
        substate.oldQueueFamilyIndex = substate.newQueueFamilyIndex = 0;
        substate.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        substate.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      }

      CheckSubresourceState(it->state(), substate);
    }
  };

  SECTION("Unmatched queue family acquire")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range(imageInfo.FullRange());
    range.baseArrayLayer = 1;
    range.layerCount = 2;

    VkImageMemoryBarrier barrier = {
        /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /* pNext = */ NULL,
        /* srcAccessMask = */ 0,
        /* dstAccessMask = */ 0,
        /* oldLayout = */ VK_IMAGE_LAYOUT_GENERAL,
        /* newLayout = */ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        /* srcQueueFamilyIndex = */ 0,
        /* dstQueueFamilyIndex = */ 1,
        /* image = */ image,
        /* subresourceRange = */ range,
    };
    state.RecordBarrier(barrier, 1, transitionInfo);
    CheckSubresourceRanges(state, false, false, true, false);

    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      ImageSubresourceState substate(initSubstate);
      if(range.baseArrayLayer <= it->range().baseArrayLayer &&
         it->range().baseArrayLayer - range.baseArrayLayer < range.layerCount)
      {
        substate.oldQueueFamilyIndex = 0;
        substate.newQueueFamilyIndex = 1;
        substate.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        substate.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }

      CheckSubresourceState(it->state(), substate);
    }

    REQUIRE(state.oldQueueFamilyTransfers.size() == 1);
    CHECK(state.oldQueueFamilyTransfers[0].oldLayout == VK_IMAGE_LAYOUT_GENERAL);
    CHECK(state.oldQueueFamilyTransfers[0].newLayout ==
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    CHECK(state.oldQueueFamilyTransfers[0].srcQueueFamilyIndex == 0);
    CHECK(state.oldQueueFamilyTransfers[0].dstQueueFamilyIndex == 1);

    CHECK(state.newQueueFamilyTransfers.size() == 0);
  };

  SECTION("Unmatched queue family release")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range(imageInfo.FullRange());
    range.baseArrayLayer = 1;
    range.layerCount = 2;

    VkImageMemoryBarrier barrier = {
        /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /* pNext = */ NULL,
        /* srcAccessMask = */ 0,
        /* dstAccessMask = */ 0,
        /* oldLayout = */ VK_IMAGE_LAYOUT_GENERAL,
        /* newLayout = */ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        /* srcQueueFamilyIndex = */ 0,
        /* dstQueueFamilyIndex = */ 1,
        /* image = */ image,
        /* subresourceRange = */ range,
    };
    state.RecordBarrier(barrier, 0, transitionInfo);
    CheckSubresourceRanges(state, false, false, false, false);
    CheckSubresourceState(state.subresourceStates.begin()->state(), initSubstate);

    REQUIRE(state.newQueueFamilyTransfers.size() == 1);
    CHECK(state.newQueueFamilyTransfers[0].oldLayout == VK_IMAGE_LAYOUT_GENERAL);
    CHECK(state.newQueueFamilyTransfers[0].newLayout ==
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    CHECK(state.newQueueFamilyTransfers[0].srcQueueFamilyIndex == 0);
    CHECK(state.newQueueFamilyTransfers[0].dstQueueFamilyIndex == 1);

    CHECK(state.oldQueueFamilyTransfers.size() == 0);
  };

  SECTION("Matched queue family transfer")
  {
    ImageState state(image, imageInfo, eFrameRef_None);
    ImageSubresourceRange range(imageInfo.FullRange());
    range.baseArrayLayer = 1;
    range.layerCount = 2;

    VkImageMemoryBarrier barrier = {
        /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /* pNext = */ NULL,
        /* srcAccessMask = */ 0,
        /* dstAccessMask = */ 0,
        /* oldLayout = */ VK_IMAGE_LAYOUT_GENERAL,
        /* newLayout = */ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        /* srcQueueFamilyIndex = */ 0,
        /* dstQueueFamilyIndex = */ 1,
        /* image = */ image,
        /* subresourceRange = */ range,
    };
    state.RecordBarrier(barrier, 0, transitionInfo);
    CheckSubresourceRanges(state, false, false, false, false);
    state.RecordBarrier(barrier, 1, transitionInfo);
    CheckSubresourceRanges(state, false, false, true, false);

    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      ImageSubresourceState substate(initSubstate);
      if(range.baseArrayLayer <= it->range().baseArrayLayer &&
         it->range().baseArrayLayer - range.baseArrayLayer < range.layerCount)
      {
        substate.oldQueueFamilyIndex = 0;
        substate.newQueueFamilyIndex = 1;
        substate.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        substate.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }

      CheckSubresourceState(it->state(), substate);
    }

    CHECK(state.oldQueueFamilyTransfers.size() == 0);

    CHECK(state.newQueueFamilyTransfers.size() == 0);
  };

  SECTION("Unsplit aspects")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    // read subresource, triggering a split in every dimension except depth
    ImageSubresourceRange range0 = imageInfo.FullRange();
    range0.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    range0.baseMipLevel = 1;
    range0.levelCount = imageInfo.levelCount - 1;
    range0.baseArrayLayer = 1;
    range0.layerCount = imageInfo.layerCount - 1;
    range0.baseDepthSlice = 0;
    range0.sliceCount = imageInfo.extent.depth;
    state.RecordUse(range0, eFrameRef_Read, 0);

    // read all aspects
    ImageSubresourceRange range1 = range0;
    range1.aspectMask = imageInfo.Aspects();
    state.RecordUse(range1, eFrameRef_Read, 0);

    state.subresourceStates.Unsplit();

    CheckSubresourceRanges(state, false, true, true, false);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().baseMipLevel > 0 && it->range().baseArrayLayer > 0)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Unsplit mip levels")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    // read subresource, triggering a split in every dimension except aspect
    ImageSubresourceRange range0 = imageInfo.FullRange();
    range0.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    range0.baseMipLevel = 1;
    range0.levelCount = imageInfo.levelCount - 1;
    range0.baseArrayLayer = 1;
    range0.layerCount = imageInfo.layerCount - 1;
    range0.baseDepthSlice = 1;
    range0.sliceCount = imageInfo.extent.depth - 1;
    state.RecordUse(range0, eFrameRef_Read, 0);

    // read all mip levels
    ImageSubresourceRange range1 = range0;
    range1.baseMipLevel = 0;
    range1.levelCount = imageInfo.levelCount;
    state.RecordUse(range1, eFrameRef_Read, 0);

    state.subresourceStates.Unsplit();

    CheckSubresourceRanges(state, false, false, true, true);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().baseArrayLayer > 0 && it->range().baseDepthSlice > 0)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Unsplit array layers")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    // read subresource, triggering a split in every dimension except mip levels
    ImageSubresourceRange range0 = imageInfo.FullRange();
    range0.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    range0.baseMipLevel = 0;
    range0.levelCount = imageInfo.levelCount;
    range0.baseArrayLayer = 1;
    range0.layerCount = imageInfo.layerCount - 1;
    range0.baseDepthSlice = 1;
    range0.sliceCount = imageInfo.extent.depth - 1;
    state.RecordUse(range0, eFrameRef_Read, 0);

    // read all array layers
    ImageSubresourceRange range1 = range0;
    range1.baseArrayLayer = 0;
    range1.layerCount = imageInfo.layerCount;
    state.RecordUse(range1, eFrameRef_Read, 0);

    state.subresourceStates.Unsplit();

    CheckSubresourceRanges(state, true, false, false, true);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT && it->range().baseDepthSlice > 0)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Unsplit depth slices")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    // read subresource, triggering a split in every dimension except array layers
    ImageSubresourceRange range0 = imageInfo.FullRange();
    range0.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    range0.baseMipLevel = 1;
    range0.levelCount = imageInfo.levelCount - 1;
    range0.baseArrayLayer = 0;
    range0.layerCount = imageInfo.layerCount;
    range0.baseDepthSlice = 1;
    range0.sliceCount = imageInfo.extent.depth - 1;
    state.RecordUse(range0, eFrameRef_Read, 0);

    // read all depth slices
    ImageSubresourceRange range1 = range0;
    range1.baseDepthSlice = 0;
    range1.sliceCount = imageInfo.extent.depth;
    state.RecordUse(range1, eFrameRef_Read, 0);

    state.subresourceStates.Unsplit();

    CheckSubresourceRanges(state, true, true, false, false);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      if(it->range().aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT && it->range().baseMipLevel > 0)
        CheckSubresourceState(it->state(), readSubstate);
      else
        CheckSubresourceState(it->state(), initSubstate);
    }
  };

  SECTION("Unsplit all")
  {
    ImageState state(image, imageInfo, eFrameRef_None);

    // read subresource, triggering a split in every dimension
    ImageSubresourceRange range0 = imageInfo.FullRange();
    range0.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    range0.baseMipLevel = 1;
    range0.levelCount = imageInfo.levelCount - 1;
    range0.baseArrayLayer = 1;
    range0.layerCount = imageInfo.layerCount - 1;
    range0.baseDepthSlice = 1;
    range0.sliceCount = imageInfo.extent.depth - 1;
    state.RecordUse(range0, eFrameRef_Read, 0);

    // read all subresources
    ImageSubresourceRange range1 = imageInfo.FullRange();
    state.RecordUse(range1, eFrameRef_Read, 0);

    state.subresourceStates.Unsplit();

    CheckSubresourceRanges(state, false, false, false, false);
    for(auto it = state.subresourceStates.begin(); it != state.subresourceStates.end(); ++it)
    {
      CheckSubresourceState(it->state(), readSubstate);
    }
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
