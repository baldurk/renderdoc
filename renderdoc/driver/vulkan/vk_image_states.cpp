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

#include "vk_resources.h"

ImageSubresourceRange ImageInfo::FullRange() const
{
  return ImageSubresourceRange(
      /* aspectMask = */ Aspects(),
      /* baseMipLevel = */ 0u,
      /* levelCount = */ (uint32_t)levelCount,
      /* baseArrayLayer = */ 0u,
      /* layerCount = */ (uint32_t)layerCount,
      /* baseDepthSlice = */ 0u,
      /* sliceCount = */ extent.depth);
}

void ImageSubresourceState::Update(const ImageSubresourceState &other, FrameRefCompFunc compose)
{
  if(oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    oldQueueFamilyIndex = other.oldQueueFamilyIndex;

  if(other.newQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    newQueueFamilyIndex = other.newQueueFamilyIndex;

  if(oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
    oldLayout = other.oldLayout;

  if(other.newLayout != UNKNOWN_PREV_IMG_LAYOUT)
    newLayout = other.newLayout;

  refType = compose(refType, other.refType);
}

bool ImageSubresourceState::Update(const ImageSubresourceState &other,
                                   ImageSubresourceState &result, FrameRefCompFunc compose) const
{
  result = *this;
  result.Update(other, compose);

  return result != *this;
}

template <typename Map, typename Pair>
typename ImageSubresourceMap::SubresourceRangeIterTemplate<Map, Pair>
    &ImageSubresourceMap::SubresourceRangeIterTemplate<Map, Pair>::operator++()
{
  if(!IsValid())
    return *this;
  FixSubRange();

  ++m_slice;
  if(IsDepthSplit(m_splitFlags) && m_slice < m_range.baseDepthSlice + m_range.sliceCount)
  {
    m_value.m_range.baseDepthSlice = m_slice;
    return *this;
  }
  m_value.m_range.baseDepthSlice = m_slice = m_range.baseDepthSlice;

  ++m_layer;
  if(AreLayersSplit(m_splitFlags) && m_layer < m_range.baseArrayLayer + m_range.layerCount)
  {
    m_value.m_range.baseArrayLayer = m_layer;
    return *this;
  }
  m_value.m_range.baseArrayLayer = m_layer = m_range.baseArrayLayer;

  ++m_level;
  if(AreLevelsSplit(m_splitFlags) && m_level < m_range.baseMipLevel + m_range.levelCount)
  {
    m_value.m_range.baseMipLevel = m_level;
    return *this;
  }
  m_value.m_range.baseMipLevel = m_level = m_range.baseMipLevel;

  if(AreAspectsSplit(m_splitFlags))
  {
    auto aspectIt = ImageAspectFlagIter(m_map->GetImageInfo().Aspects(),
                                        (VkImageAspectFlagBits)m_value.m_range.aspectMask);
    while(true)
    {
      ++m_aspectIndex;
      ++aspectIt;
      if(aspectIt == ImageAspectFlagIter::end())
      {
        break;
      }
      else if(m_range.aspectMask & *aspectIt)
      {
        m_value.m_range.aspectMask = *aspectIt;
        return *this;
      }
    }
  }

  // iterator is at the end.
  // make `m_aspectIndex` out of range to mark this.
  m_aspectIndex = m_map->m_aspectCount;
  return *this;
}
template
    typename ImageSubresourceMap::SubresourceRangeIterTemplate<ImageSubresourceMap,
                                                               ImageSubresourceMap::SubresourcePairRef>
        &ImageSubresourceMap::SubresourceRangeIterTemplate<
            ImageSubresourceMap, ImageSubresourceMap::SubresourcePairRef>::operator++();
template typename ImageSubresourceMap::SubresourceRangeIterTemplate<
    const ImageSubresourceMap, ImageSubresourceMap::ConstSubresourcePairRef>
    &ImageSubresourceMap::SubresourceRangeIterTemplate<
        const ImageSubresourceMap, ImageSubresourceMap::ConstSubresourcePairRef>::operator++();

void ImageSubresourceMap::Split(bool splitAspects, bool splitLevels, bool splitLayers, bool splitDepth)
{
  uint16_t newFlags = m_flags;
  if(splitAspects)
    newFlags |= (uint16_t)FlagBits::AreAspectsSplit;
  else
    splitAspects = AreAspectsSplit();

  if(splitLevels)
    newFlags |= (uint16_t)FlagBits::AreLevelsSplit;
  else
    splitLevels = AreLevelsSplit();

  if(splitLayers)
    newFlags |= (uint16_t)FlagBits::AreLayersSplit;
  else
    splitLayers = AreLayersSplit();

  if(splitDepth)
    newFlags |= (uint16_t)FlagBits::IsDepthSplit;
  else
    splitDepth = IsDepthSplit();

  if(newFlags == m_flags)
    // not splitting anything new
    return;

  uint32_t oldSplitAspectCount = AreAspectsSplit() ? m_aspectCount : 1;
  uint32_t newSplitAspectCount = splitAspects ? m_aspectCount : oldSplitAspectCount;

  uint32_t oldSplitLevelCount = AreLevelsSplit() ? GetImageInfo().levelCount : 1;
  uint32_t newSplitLevelCount = splitLevels ? GetImageInfo().levelCount : oldSplitLevelCount;

  uint32_t oldSplitLayerCount = AreLayersSplit() ? GetImageInfo().layerCount : 1;
  uint32_t newSplitLayerCount = splitLayers ? GetImageInfo().layerCount : oldSplitLayerCount;

  uint32_t oldSplitSliceCount = IsDepthSplit() ? GetImageInfo().extent.depth : 1;
  uint32_t newSplitSliceCount = splitDepth ? GetImageInfo().extent.depth : oldSplitSliceCount;

  uint32_t oldSize = (uint32_t)m_values.size();

  uint32_t newSize =
      newSplitAspectCount * newSplitLevelCount * newSplitLayerCount * newSplitSliceCount;
  RDCASSERT(newSize > RDCMAX(oldSize, 1U));

  m_values.resize(newSize);
  // if m_values was empty before, copy the first value from our inline storage
  if(oldSize == 0)
    m_values[0] = m_value;

  uint32_t newAspectIndex = newSplitAspectCount - 1;
  uint32_t oldAspectIndex = AreAspectsSplit() ? newAspectIndex : 0;
  uint32_t newLevel = newSplitLevelCount - 1;
  uint32_t oldLevel = AreLevelsSplit() ? newLevel : 0;
  uint32_t newLayer = newSplitLayerCount - 1;
  uint32_t oldLayer = AreLayersSplit() ? newLayer : 0;
  uint32_t newSlice = newSplitSliceCount - 1;
  uint32_t oldSlice = IsDepthSplit() ? newSlice : 0;
  uint32_t newIndex = newSize - 1;
  while(true)
  {
    uint32_t oldIndex =
        ((oldAspectIndex * oldSplitLevelCount + oldLevel) * oldSplitLayerCount + oldLayer) *
            oldSplitSliceCount +
        oldSlice;
    m_values[newIndex] = m_values[oldIndex];

    if(newIndex == 0)
    {
      RDCASSERT(oldIndex == 0);
      break;
    }
    --newIndex;

    if(newSlice > 0)
    {
      --newSlice;
      oldSlice = IsDepthSplit() ? newSlice : 0;
      continue;
    }
    newSlice = newSplitSliceCount - 1;
    oldSlice = oldSplitSliceCount - 1;

    if(newLayer > 0)
    {
      --newLayer;
      oldLayer = AreLayersSplit() ? newLayer : 0;
      continue;
    }
    newLayer = newSplitLayerCount - 1;
    oldLayer = oldSplitLayerCount - 1;

    if(newLevel > 0)
    {
      --newLevel;
      oldLevel = AreLevelsSplit() ? newLevel : 0;
      continue;
    }
    newLevel = newSplitLevelCount - 1;
    oldLevel = oldSplitLevelCount - 1;

    if(newAspectIndex > 0)
    {
      --newAspectIndex;
      oldAspectIndex = AreAspectsSplit() ? newAspectIndex : 0;
      continue;
    }
    RDCERR("Too many subresources in ImageSubresourceMap::Split");
    break;
  }

  m_flags = newFlags;
}

void ImageSubresourceMap::Unsplit(bool unsplitAspects, bool unsplitLevels, bool unsplitLayers,
                                  bool unsplitDepth)
{
  uint16_t newFlags = m_flags;
  if(unsplitAspects)
    newFlags &= ~(uint16_t)FlagBits::AreAspectsSplit;

  if(unsplitLevels)
    newFlags &= ~(uint16_t)FlagBits::AreLevelsSplit;

  if(unsplitLayers)
    newFlags &= ~(uint16_t)FlagBits::AreLayersSplit;

  if(unsplitDepth)
    newFlags &= ~(uint16_t)FlagBits::IsDepthSplit;

  if(newFlags == m_flags)
    // not splitting anything new
    return;

  uint32_t oldSplitAspectCount = AreAspectsSplit() ? m_aspectCount : 1;
  uint32_t newSplitAspectCount = unsplitAspects ? 1 : oldSplitAspectCount;

  uint32_t oldSplitLevelCount = AreLevelsSplit() ? GetImageInfo().levelCount : 1;
  uint32_t newSplitLevelCount = unsplitLevels ? 1 : oldSplitLevelCount;

  uint32_t oldSplitLayerCount = AreLayersSplit() ? GetImageInfo().layerCount : 1;
  uint32_t newSplitLayerCount = unsplitLayers ? 1 : oldSplitLayerCount;

  uint32_t oldSplitSliceCount = IsDepthSplit() ? GetImageInfo().extent.depth : 1;
  uint32_t newSplitSliceCount = unsplitDepth ? 1 : oldSplitSliceCount;

  uint32_t oldSize = (uint32_t)m_values.size();
  RDCASSERT(oldSize > 0);

  uint32_t newSize =
      newSplitAspectCount * newSplitLevelCount * newSplitLayerCount * newSplitSliceCount;
  RDCASSERT(newSize < oldSize);

  rdcarray<ImageSubresourceState> newValues;
  newValues.resize(newSize);

  uint32_t aspectIndex = 0;
  uint32_t level = 0;
  uint32_t layer = 0;
  uint32_t slice = 0;
  uint32_t newIndex = 0;

  while(newIndex < newValues.size())
  {
    uint32_t oldIndex = ((aspectIndex * oldSplitLevelCount + level) * oldSplitLayerCount + layer) *
                            oldSplitSliceCount +
                        slice;
    newValues[newIndex] = m_values[oldIndex];

    ++newIndex;

    ++slice;
    if(slice < newSplitSliceCount)
      continue;
    slice = 0;

    ++layer;
    if(layer < newSplitLayerCount)
      continue;
    layer = 0;

    ++level;
    if(level < newSplitLevelCount)
      continue;
    level = 0;

    ++aspectIndex;
  }

  newValues.swap(m_values);
  m_flags = newFlags;
}

void ImageSubresourceMap::Unsplit()
{
  if(m_values.size() <= 1)
    return;

  uint32_t aspectCount = AreAspectsSplit() ? m_aspectCount : 1;
  uint32_t aspectIndex = 0;
  uint32_t levelCount = AreLevelsSplit() ? m_imageInfo.levelCount : 1;
  uint32_t level = 0;
  uint32_t layerCount = AreLayersSplit() ? m_imageInfo.layerCount : 1;
  uint32_t layer = 0;
  uint32_t sliceCount = IsDepthSplit() ? m_imageInfo.extent.depth : 1;
  uint32_t slice = 0;
  uint32_t index = 0;

  bool canUnsplitAspects = aspectCount > 1;
  bool canUnsplitLevels = levelCount > 1;
  bool canUnsplitLayers = layerCount > 1;
  bool canUnsplitDepth = sliceCount > 1;

  RDCASSERT(aspectCount * levelCount * layerCount * sliceCount == m_values.size());
#define UNSPLIT_INDEX(ASPECT, LEVEL, LAYER, SLICE) \
  ((((ASPECT)*levelCount + (LEVEL)) * layerCount + (LAYER)) * sliceCount + (SLICE))
  while(index < m_values.size() &&
        (canUnsplitAspects || canUnsplitLevels || canUnsplitLayers || canUnsplitDepth))
  {
    if(canUnsplitAspects && aspectIndex > 0)
    {
      uint32_t index0 = UNSPLIT_INDEX(0, level, layer, slice);
      if(m_values[index] != m_values[index0])
        canUnsplitAspects = false;
    }
    if(canUnsplitLevels && level > 0)
    {
      uint32_t index0 = UNSPLIT_INDEX(aspectIndex, 0, layer, slice);
      if(m_values[index] != m_values[index0])
        canUnsplitLevels = false;
    }
    if(canUnsplitLayers && layer > 0)
    {
      uint32_t index0 = UNSPLIT_INDEX(aspectIndex, level, 0, slice);
      if(m_values[index] != m_values[index0])
        canUnsplitLayers = false;
    }
    if(canUnsplitDepth && slice > 0)
    {
      uint32_t index0 = UNSPLIT_INDEX(aspectIndex, level, layer, 0);
      if(m_values[index] != m_values[index0])
        canUnsplitDepth = false;
    }

    ++index;

    ++slice;
    if(slice < sliceCount)
      continue;
    slice = 0;

    ++layer;
    if(layer < layerCount)
      continue;
    layer = 0;

    ++level;
    if(level < levelCount)
      continue;
    level = 0;

    ++aspectIndex;
    if(aspectIndex >= aspectCount)
      break;
  }
#undef UNSPLIT_INDEX

  Unsplit(canUnsplitAspects, canUnsplitLevels, canUnsplitLayers, canUnsplitDepth);
}

inline FrameRefType ImageSubresourceMap::Merge(const ImageSubresourceMap &other,
                                               FrameRefCompFunc compose)
{
  FrameRefType maxRefType = eFrameRef_None;
  bool didSplit = false;
  for(auto oIt = other.begin(); oIt != other.end(); ++oIt)
  {
    for(auto it = RangeBegin(oIt->range()); it != end(); ++it)
    {
      ImageSubresourceState subState;
      if(it->state().Update(oIt->state(), subState, compose))
      {
        if(!didSplit)
        {
          Split(oIt->range());
          didSplit = true;
        }
        RDCASSERT(it->range().ContainedIn(oIt->range()));
        it->SetState(subState);
        maxRefType = ComposeFrameRefsDisjoint(maxRefType, subState.refType);
      }
    }
  }
  return maxRefType;
}

size_t ImageSubresourceMap::SubresourceIndex(uint32_t aspectIndex, uint32_t level, uint32_t layer,
                                             uint32_t slice) const
{
  if(!AreAspectsSplit())
    aspectIndex = 0;
  int splitLevelCount = 1;
  if(AreLevelsSplit())
    splitLevelCount = GetImageInfo().levelCount;
  else
    level = 0;
  int splitLayerCount = 1;
  if(AreLayersSplit())
    splitLayerCount = GetImageInfo().layerCount;
  else
    layer = 0;
  int splitSliceCount = 1;
  if(IsDepthSplit())
    splitSliceCount = GetImageInfo().extent.depth;
  else
    slice = 0;
  return ((aspectIndex * splitLevelCount + level) * splitLayerCount + layer) * splitSliceCount +
         slice;
}

void ImageSubresourceMap::ToArray(rdcarray<ImageSubresourceStateForRange> &arr)
{
  arr.reserve(arr.size() + size());
  for(auto src = begin(); src != end(); ++src)
  {
    arr.push_back(*src);
  }
}

void ImageSubresourceMap::FromArray(const rdcarray<ImageSubresourceStateForRange> &arr)
{
  if(arr.empty())
  {
    RDCERR("No values for ImageSubresourceMap");
    return;
  }
  Split(arr.front().range);
  if(size() != arr.size())
  {
    RDCERR("Incorrect number of values for ImageSubresourceMap");
    return;
  }
  auto src = arr.begin();
  auto dst = begin();
  while(src != arr.end())
  {
    if(src->range != dst->range())
      RDCERR("Subresource range mismatch in ImageSubresourceMap");
    else
      dst->SetState(src->state);
    ++src;
    ++dst;
  }
}

void ImageSubresourceMap::FromImgRefs(const ImgRefs &imgRefs)
{
  bool splitLayers = imgRefs.areLayersSplit;
  bool splitDepth = false;
  if(GetImageInfo().extent.depth > 1)
  {
    RDCASSERT(GetImageInfo().layerCount == 1);
    splitDepth = splitLayers;
    splitLayers = false;
  }
  Split(imgRefs.areAspectsSplit, imgRefs.areLevelsSplit, splitLayers, splitDepth);
  RDCASSERT(!(AreLayersSplit() && IsDepthSplit()));

  for(auto dstIt = begin(); dstIt != end(); ++dstIt)
  {
    int aspectIndex = imgRefs.AspectIndex((VkImageAspectFlagBits)dstIt->range().aspectMask);
    int level = (int)dstIt->range().baseMipLevel;
    int layer = (int)(dstIt->range().baseArrayLayer + dstIt->range().baseDepthSlice);
    dstIt->state().refType = imgRefs.SubresourceRef(aspectIndex, level, layer);
  }
}

bool IntervalsOverlap(uint32_t base1, uint32_t count1, uint32_t base2, uint32_t count2)
{
  if((base1 + count1) < base1)
  {
    // integer overflow
    if(count1 != VK_REMAINING_MIP_LEVELS)
      RDCWARN("Integer overflow in interval: base=%u, count=%u", base1, count1);
    count1 = UINT32_MAX - base1;
  }
  if((base2 + count2) < base2)
  {
    // integer overflow
    if(count2 != VK_REMAINING_MIP_LEVELS)
      RDCWARN("Integer overflow in interval: base=%u, count=%u", base2, count2);
    count2 = UINT32_MAX - base2;
  }
  if(count1 == 0 || count2 == 0)
    return false;    // one of the intervals is empty, so no overlap
  if(base1 > base2)
  {
    std::swap(base1, base2);
    std::swap(count1, count2);
  }
  return base2 < base1 + count1;
}

bool IntervalContainedIn(uint32_t base1, uint32_t count1, uint32_t base2, uint32_t count2)
{
  if((base1 + count1) < base1)
  {
    // integer overflow
    if(count1 != VK_REMAINING_MIP_LEVELS)
      RDCWARN("Integer overflow in interval: base=%u, count=%u", base1, count1);
    count1 = UINT32_MAX - base1;
  }
  if((base2 + count2) < base2)
  {
    // integer overflow
    if(count2 != VK_REMAINING_MIP_LEVELS)
      RDCWARN("Integer overflow in interval: base=%u, count=%u", base2, count2);
    count2 = UINT32_MAX - base2;
  }
  return base1 >= base2 && base1 + count1 <= base2 + count2;
}

bool SanitiseLevelRange(uint32_t &baseMipLevel, uint32_t &levelCount, uint32_t imageLevelCount)
{
  bool res = true;
  if(baseMipLevel > imageLevelCount)
  {
    RDCWARN("baseMipLevel (%u) is greater than image levelCount (%u)", baseMipLevel, imageLevelCount);
    baseMipLevel = imageLevelCount;
    res = false;
  }
  if(levelCount == VK_REMAINING_MIP_LEVELS)
  {
    levelCount = imageLevelCount - baseMipLevel;
  }
  else if(levelCount > imageLevelCount - baseMipLevel)
  {
    RDCWARN("baseMipLevel (%u) + levelCount (%u) is greater than the image levelCount (%u)",
            baseMipLevel, levelCount, imageLevelCount);
    levelCount = imageLevelCount - baseMipLevel;
    res = false;
  }
  return res;
}

bool SanitiseLayerRange(uint32_t &baseArrayLayer, uint32_t &layerCount, uint32_t imageLayerCount)
{
  bool res = true;
  if(baseArrayLayer > imageLayerCount)
  {
    RDCWARN("baseArrayLayer (%u) is greater than image layerCount (%u)", baseArrayLayer,
            imageLayerCount);
    baseArrayLayer = imageLayerCount;
    res = false;
  }
  if(layerCount == VK_REMAINING_ARRAY_LAYERS)
  {
    layerCount = imageLayerCount - baseArrayLayer;
  }
  else if(layerCount > imageLayerCount - baseArrayLayer)
  {
    RDCWARN("baseArrayLayer (%u) + layerCount (%u) is greater than the image layerCount (%u)",
            baseArrayLayer, layerCount, imageLayerCount);
    layerCount = imageLayerCount - baseArrayLayer;
    res = false;
  }
  return res;
}

bool SanitiseSliceRange(uint32_t &baseSlice, uint32_t &sliceCount, uint32_t imageSliceCount)
{
  bool res = true;
  if(baseSlice > imageSliceCount)
  {
    RDCWARN("baseSlice (%u) is greater than image sliceCount (%u)", baseSlice, imageSliceCount);
    baseSlice = imageSliceCount;
    res = false;
  }
  if(sliceCount == VK_REMAINING_ARRAY_LAYERS)
  {
    sliceCount = imageSliceCount - baseSlice;
  }
  else if(sliceCount > imageSliceCount - baseSlice)
  {
    RDCWARN("baseSlice (%u) + sliceCount (%u) is greater than the image sliceCount (%u)", baseSlice,
            sliceCount, imageSliceCount);
    sliceCount = imageSliceCount - baseSlice;
    res = false;
  }
  return res;
}

template <typename Map, typename Pair>
ImageSubresourceMap::SubresourceRangeIterTemplate<Map, Pair>::SubresourceRangeIterTemplate(
    Map &map, const ImageSubresourceRange &range)
    : m_map(&map),
      m_range(range),
      m_level(range.baseMipLevel),
      m_layer(range.baseArrayLayer),
      m_slice(range.baseDepthSlice)
{
  m_range.Sanitise(m_map->GetImageInfo());
  m_splitFlags = (uint16_t)ImageSubresourceMap::FlagBits::IsUninitialized;
  FixSubRange();
}
template ImageSubresourceMap::SubresourceRangeIterTemplate<ImageSubresourceMap,
                                                           ImageSubresourceMap::SubresourcePairRef>::
    SubresourceRangeIterTemplate(ImageSubresourceMap &map, const ImageSubresourceRange &range);
template ImageSubresourceMap::SubresourceRangeIterTemplate<
    const ImageSubresourceMap, ImageSubresourceMap::ConstSubresourcePairRef>::
    SubresourceRangeIterTemplate(const ImageSubresourceMap &map, const ImageSubresourceRange &range);

template <typename Map, typename Pair>
void ImageSubresourceMap::SubresourceRangeIterTemplate<Map, Pair>::FixSubRange()
{
  if(m_splitFlags == m_map->m_flags)
    return;
  uint16_t oldFlags = m_splitFlags;
  m_splitFlags = m_map->m_flags;

  if(IsDepthSplit(m_splitFlags))
  {
    m_value.m_range.baseDepthSlice = m_slice;
    m_value.m_range.sliceCount = 1u;
  }
  else
  {
    m_value.m_range.baseDepthSlice = 0u;
    m_value.m_range.sliceCount = m_map->GetImageInfo().extent.depth;
  }

  if(AreLayersSplit(m_splitFlags))
  {
    m_value.m_range.baseArrayLayer = m_layer;
    m_value.m_range.layerCount = 1u;
  }
  else
  {
    m_value.m_range.baseArrayLayer = 0u;
    m_value.m_range.layerCount = m_map->GetImageInfo().layerCount;
  }

  if(AreLevelsSplit(m_splitFlags))
  {
    m_value.m_range.baseMipLevel = m_level;
    m_value.m_range.levelCount = 1u;
  }
  else
  {
    m_value.m_range.baseMipLevel = 0u;
    m_value.m_range.levelCount = m_map->GetImageInfo().levelCount;
  }

  if(!AreAspectsSplit(m_splitFlags))
  {
    m_value.m_range.aspectMask = m_map->GetImageInfo().Aspects();
  }
  else if(!AreAspectsSplit(oldFlags))
  {
    // aspects are split in the map, but are not yet split in this iterator.
    // We need to find the aspectMask.
    uint32_t i = 0;
    for(auto it = ImageAspectFlagIter::begin(m_map->GetImageInfo().Aspects());
        it != ImageAspectFlagIter::end(); ++it, ++i)
    {
      if(i >= m_aspectIndex && (((*it) & m_range.aspectMask) != 0))
      {
        m_value.m_range.aspectMask = *it;
        break;
      }
    }
    m_aspectIndex = i;
  }
}
template void ImageSubresourceMap::SubresourceRangeIterTemplate<
    ImageSubresourceMap, ImageSubresourceMap::SubresourcePairRef>::FixSubRange();
template void ImageSubresourceMap::SubresourceRangeIterTemplate<
    const ImageSubresourceMap, ImageSubresourceMap::ConstSubresourcePairRef>::FixSubRange();

template <typename Map, typename Pair>
Pair *ImageSubresourceMap::SubresourceRangeIterTemplate<Map, Pair>::operator->()
{
  FixSubRange();
  m_value.m_state = &m_map->SubresourceIndexValue(m_aspectIndex, m_level, m_layer, m_slice);
  return &m_value;
}
template ImageSubresourceMap::SubresourcePairRef *ImageSubresourceMap::SubresourceRangeIterTemplate<
    ImageSubresourceMap, ImageSubresourceMap::SubresourcePairRef>::operator->();
template ImageSubresourceMap::ConstSubresourcePairRef *ImageSubresourceMap::SubresourceRangeIterTemplate<
    const ImageSubresourceMap, ImageSubresourceMap::ConstSubresourcePairRef>::operator->();

template <typename Map, typename Pair>
Pair &ImageSubresourceMap::SubresourceRangeIterTemplate<Map, Pair>::operator*()
{
  FixSubRange();
  m_value.m_state = &m_map->SubresourceIndexValue(m_aspectIndex, m_level, m_layer, m_slice);
  return m_value;
}
template ImageSubresourceMap::SubresourcePairRef &ImageSubresourceMap::SubresourceRangeIterTemplate<
    ImageSubresourceMap, ImageSubresourceMap::SubresourcePairRef>::operator*();
template ImageSubresourceMap::ConstSubresourcePairRef &ImageSubresourceMap::SubresourceRangeIterTemplate<
    const ImageSubresourceMap, ImageSubresourceMap::ConstSubresourcePairRef>::operator*();

uint32_t ImageBarrierSequence::MaxQueueFamilyIndex = 4;

void ImageBarrierSequence::AddWrapped(uint32_t batchIndex, uint32_t queueFamilyIndex,
                                      const VkImageMemoryBarrier &barrier)
{
  RDCASSERT(batchIndex < MAX_BATCH_COUNT);
  RDCASSERT(queueFamilyIndex < batches[batchIndex].size(), queueFamilyIndex,
            batches[batchIndex].size());
  batches[batchIndex][queueFamilyIndex].push_back(barrier);
  ++barrierCount;
}

void ImageBarrierSequence::Merge(const ImageBarrierSequence &other)
{
  for(uint32_t batchIndex = 0; batchIndex < MAX_BATCH_COUNT; ++batchIndex)
  {
    rdcarray<Batch> &batch = batches[batchIndex];
    const rdcarray<Batch> &otherBatch = other.batches[batchIndex];
    for(size_t queueFamilyIndex = 0; queueFamilyIndex < batch.size(); ++queueFamilyIndex)
    {
      Batch &barriers = batch[queueFamilyIndex];
      const Batch &otherBarriers = otherBatch[queueFamilyIndex];
      barriers.insert(barriers.size(), otherBarriers.begin(), otherBarriers.size());
      barrierCount += otherBarriers.size();
    }
  }
}

bool ImageBarrierSequence::IsBatchEmpty(uint32_t batchIndex) const
{
  if(batchIndex >= MAX_BATCH_COUNT)
    return true;
  for(size_t queueFamilyIndex = 0; queueFamilyIndex < batches[batchIndex].size(); ++queueFamilyIndex)
  {
    if(!batches[batchIndex][queueFamilyIndex].empty())
      return false;
  }
  return true;
}

void ImageBarrierSequence::UnwrapBarriers(rdcarray<VkImageMemoryBarrier> &barriers)
{
  for(auto it = barriers.begin(); it != barriers.end(); ++it)
  {
    it->image = ::Unwrap(it->image);
  }
}

void ImageBarrierSequence::ExtractUnwrappedBatch(uint32_t batchIndex, uint32_t queueFamilyIndex,
                                                 Batch &result)
{
  if(batchIndex >= MAX_BATCH_COUNT || queueFamilyIndex >= batches[batchIndex].size())
    return;
  Batch &batch = batches[batchIndex][queueFamilyIndex];
  batch.swap(result);
  batch.clear();
  barrierCount -= result.size();
  UnwrapBarriers(result);
}

void ImageBarrierSequence::ExtractFirstUnwrappedBatchForQueue(uint32_t queueFamilyIndex, Batch &result)
{
  for(uint32_t batchIndex = 0; batchIndex < MAX_BATCH_COUNT; ++batchIndex)
  {
    if(!IsBatchEmpty(batchIndex))
    {
      batches[batchIndex][queueFamilyIndex].swap(result);
      batches[batchIndex][queueFamilyIndex].clear();
      barrierCount -= result.size();
      UnwrapBarriers(result);
      return;
    }
  }
}

void ImageBarrierSequence::ExtractLastUnwrappedBatchForQueue(uint32_t queueFamilyIndex, Batch &result)
{
  for(uint32_t batchIndex = MAX_BATCH_COUNT; batchIndex > 0;)
  {
    --batchIndex;
    if(!IsBatchEmpty(batchIndex))
    {
      batches[batchIndex][queueFamilyIndex].swap(result);
      batches[batchIndex][queueFamilyIndex].clear();
      barrierCount -= result.size();
      UnwrapBarriers(result);
      return;
    }
  }
}

ImageState ImageState::InitialState() const
{
  ImageState result(wrappedHandle, GetImageInfo(), eFrameRef_Unknown);
  InitialState(result);
  return result;
}

void ImageState::InitialState(ImageState &result) const
{
  result.subresourceStates = subresourceStates;
  for(auto it = result.subresourceStates.begin(); it != result.subresourceStates.end(); ++it)
  {
    ImageSubresourceState &sub = it->state();
    sub.newLayout = sub.oldLayout = GetImageInfo().initialLayout;
    sub.newQueueFamilyIndex = sub.oldQueueFamilyIndex;
    sub.refType = eFrameRef_Unknown;
  }
}

ImageState ImageState::CommandBufferInitialState() const
{
  ImageSubresourceState sub;
  sub.oldLayout = sub.newLayout = UNKNOWN_PREV_IMG_LAYOUT;
  return UniformState(sub);
}

ImageState ImageState::UniformState(const ImageSubresourceState &sub) const
{
  ImageState result(wrappedHandle, GetImageInfo(), eFrameRef_None);
  result.subresourceStates.begin()->SetState(sub);
  return result;
}

ImageState ImageState::ContentInitializationState(InitPolicy policy, bool initialized,
                                                  uint32_t queueFamilyIndex, VkImageLayout copyLayout,
                                                  VkImageLayout clearLayout) const
{
  ImageState result = *this;
  for(auto it = result.subresourceStates.begin(); it != result.subresourceStates.end(); ++it)
  {
    ImageSubresourceState &sub = it->state();
    InitReqType initReq = InitReq(sub.refType, policy, initialized);
    if(initReq == eInitReq_None)
      continue;
    sub.newQueueFamilyIndex = queueFamilyIndex;
    if(initReq == eInitReq_Copy)
      sub.newLayout = copyLayout;
    else if(initReq == eInitReq_Clear)
      sub.newLayout = clearLayout;
  }
  return result;
}

void ImageState::RemoveQueueFamilyTransfer(VkImageMemoryBarrier *it)
{
  if(it < newQueueFamilyTransfers.begin() || it >= newQueueFamilyTransfers.end())
    RDCERR("Attempting to remove queue family transfer at invalid address");
  std::swap(*it, newQueueFamilyTransfers.back());
  newQueueFamilyTransfers.erase(newQueueFamilyTransfers.size() - 1);
}

void ImageState::Update(ImageSubresourceRange range, const ImageSubresourceState &dst,
                        FrameRefCompFunc compose)
{
  range.Sanitise(GetImageInfo());

  bool didSplit = false;
  for(auto it = subresourceStates.RangeBegin(range); it != subresourceStates.end(); ++it)
  {
    ImageSubresourceState subState;
    if(it->state().Update(dst, subState, compose))
    {
      if(!didSplit)
      {
        subresourceStates.Split(range);
        didSplit = true;
      }
      RDCASSERT(it->range().ContainedIn(range));
      it->SetState(subState);
      maxRefType = ComposeFrameRefsDisjoint(maxRefType, subState.refType);
    }
  }
}

void ImageState::Merge(const ImageState &other, ImageTransitionInfo info)
{
  if(wrappedHandle == VK_NULL_HANDLE)
    wrappedHandle = other.wrappedHandle;
  for(auto it = other.oldQueueFamilyTransfers.begin(); it != other.oldQueueFamilyTransfers.end(); ++it)
  {
    RecordQueueFamilyAcquire(*it);
  }
  maxRefType = subresourceStates.Merge(other.subresourceStates, info.GetFrameRefCompFunc());
  for(auto it = other.newQueueFamilyTransfers.begin(); it != other.newQueueFamilyTransfers.end(); ++it)
  {
    RecordQueueFamilyRelease(*it);
  }
}

void ImageState::MergeCaptureBeginState(const ImageState &initialState)
{
  oldQueueFamilyTransfers = initialState.oldQueueFamilyTransfers;
  subresourceStates.Merge(initialState.subresourceStates, ComposeFrameRefsFirstKnown);
  maxRefType = initialState.maxRefType;
}

void ImageState::Merge(rdcflatmap<ResourceId, ImageState> &states,
                       const rdcflatmap<ResourceId, ImageState> &dstStates, ImageTransitionInfo info)
{
  auto it = states.begin();
  auto dstIt = dstStates.begin();
  while(dstIt != dstStates.end())
  {
    if(it == states.end() || dstIt->first < it->first)
    {
      it = states.insert(it, {dstIt->first, dstIt->second.InitialState()});
    }
    else if(it->first < dstIt->first)
    {
      ++it;
      continue;
    }

    it->second.Merge(dstIt->second, info);
    ++it;
    ++dstIt;
  }
}

void ImageState::DiscardContents(const ImageSubresourceRange &range)
{
  Update(range, ImageSubresourceState(VK_QUEUE_FAMILY_IGNORED, VK_IMAGE_LAYOUT_UNDEFINED),
         KeepOldFrameRef);
}

void ImageState::RecordQueueFamilyRelease(const VkImageMemoryBarrier &barrier)
{
  for(auto it = newQueueFamilyTransfers.begin(); it != newQueueFamilyTransfers.end(); ++it)
  {
    if(ImageSubresourceRange(barrier.subresourceRange).Overlaps(it->subresourceRange))
    {
#if ENABLED(RDOC_DEVEL)
      RDCWARN("Queue family release barriers overlap");
#endif
      RemoveQueueFamilyTransfer(it);
      --it;
    }
  }
  newQueueFamilyTransfers.push_back(barrier);
}

void ImageState::RecordQueueFamilyAcquire(const VkImageMemoryBarrier &barrier)
{
  bool foundRelease = false;
  ImageSubresourceRange acquireRange(barrier.subresourceRange);
  for(auto it = newQueueFamilyTransfers.begin(); it != newQueueFamilyTransfers.end(); ++it)
  {
    ImageSubresourceRange releaseRange(it->subresourceRange);
    if(acquireRange.Overlaps(releaseRange))
    {
      if(acquireRange != releaseRange)
        RDCWARN(
            "Overlapping queue family release and acquire barriers have different "
            "subresourceRange");
      if(barrier.srcQueueFamilyIndex != it->srcQueueFamilyIndex ||
         barrier.dstQueueFamilyIndex != it->dstQueueFamilyIndex)
        RDCWARN("Queue family mismatch between release and acquire barriers");
      if(barrier.oldLayout != it->oldLayout || barrier.newLayout != it->newLayout)
        RDCWARN("Image layouts mismatch between release and acquire barriers");
      if(foundRelease)
        RDCWARN("Found multiple release barriers for acquire barrier");
      RemoveQueueFamilyTransfer(it);
      --it;
      foundRelease = true;
    }
  }
  if(!foundRelease)
  {
    oldQueueFamilyTransfers.push_back(barrier);
  }
}

void ImageState::RecordBarrier(VkImageMemoryBarrier barrier, uint32_t queueFamilyIndex,
                               ImageTransitionInfo info)
{
  if(barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL ||
     barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT ||
     barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL ||
     barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT)
  {
    RDCDEBUG("External/foreign queue families are not supported");
    return;
  }
  if(GetImageInfo().sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    if(!(barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED &&
         barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED))
    {
      RDCWARN("Barrier contains invalid queue families for VK_SHARING_MODE_CONCURRENT (%u %u)",
              barrier.srcQueueFamilyIndex, barrier.dstQueueFamilyIndex);
    }
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = queueFamilyIndex;
  }
  else if(GetImageInfo().sharingMode == VK_SHARING_MODE_EXCLUSIVE)
  {
    if(barrier.srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED ||
       barrier.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
      if(barrier.srcQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED ||
         barrier.dstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
      {
        RDCERR("Barrier contains invalid queue families for VK_SHARING_MODE_EXCLUSIVE: (%s, %s)",
               ToStr(barrier.srcQueueFamilyIndex).c_str(),
               ToStr(barrier.dstQueueFamilyIndex).c_str());
        return;
      }
      barrier.srcQueueFamilyIndex = queueFamilyIndex;
      barrier.dstQueueFamilyIndex = queueFamilyIndex;
    }
    else if(barrier.srcQueueFamilyIndex == queueFamilyIndex)
    {
      if(barrier.dstQueueFamilyIndex != queueFamilyIndex)
      {
        RecordQueueFamilyRelease(barrier);
        // Skip the updates to the subresource states.
        // These will be updated by the acquire.
        // This allows us to restore a released-but-not-acquired state by first transitioning to the
        // subresource states (which will match the srcQueueFamilyIndex/oldLayout), and then
        // applying the release barrier.
        return;
      }
    }
    else if(barrier.dstQueueFamilyIndex == queueFamilyIndex)
    {
      RecordQueueFamilyAcquire(barrier);
    }
    else
    {
      RDCERR("Ownership transfer from queue family %u to %u submitted to queue family %u",
             barrier.srcQueueFamilyIndex, barrier.dstAccessMask, queueFamilyIndex);
    }
  }

  Update(barrier.subresourceRange, ImageSubresourceState(barrier), info.GetFrameRefCompFunc());
}

bool ImageState::CloseTransfers(uint32_t batchIndex, VkAccessFlags dstAccessMask,
                                ImageBarrierSequence &barriers, ImageTransitionInfo info)
{
  if(newQueueFamilyTransfers.empty())
    return false;
  FrameRefCompFunc compose = info.GetFrameRefCompFunc();
  for(auto it = newQueueFamilyTransfers.begin(); it != newQueueFamilyTransfers.end(); ++it)
  {
    Update(it->subresourceRange, ImageSubresourceState(it->dstQueueFamilyIndex, it->newLayout),
           compose);

    it->dstAccessMask = dstAccessMask;
    it->image = wrappedHandle;
    barriers.AddWrapped(batchIndex, it->dstQueueFamilyIndex, *it);
  }
  newQueueFamilyTransfers.clear();
  return true;
}

bool ImageState::RestoreTransfers(uint32_t batchIndex,
                                  const rdcarray<VkImageMemoryBarrier> &transfers,
                                  VkAccessFlags srcAccessMask, ImageBarrierSequence &barriers,
                                  ImageTransitionInfo info)
{
  // TODO: figure out why `transfers` has duplicate entries
  if(transfers.empty())
    return false;
  for(auto it = transfers.begin(); it != transfers.end(); ++it)
  {
    VkImageMemoryBarrier barrier = *it;
    barrier.srcAccessMask = srcAccessMask;
    barrier.image = wrappedHandle;
    barriers.AddWrapped(batchIndex, barrier.srcQueueFamilyIndex, barrier);
    RecordQueueFamilyRelease(barrier);
  }
  return true;
}

void ImageState::ResetToOldState(ImageBarrierSequence &barriers, ImageTransitionInfo info)
{
  VkAccessFlags srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
  VkAccessFlags dstAccessMask = VK_ACCESS_ALL_READ_BITS;
  const uint32_t CLOSE_TRANSFERS_BATCH_INDEX = 0;
  const uint32_t MAIN_BATCH_INDEX = 1;
  const uint32_t ACQUIRE_BATCH_INDEX = 2;
  const uint32_t RESTORE_TRANSFERS_BATCH_INDEX = 3;
  CloseTransfers(CLOSE_TRANSFERS_BATCH_INDEX, dstAccessMask, barriers, info);

  for(auto subIt = subresourceStates.begin(); subIt != subresourceStates.end(); ++subIt)
  {
    VkImageLayout oldLayout = subIt->state().newLayout;
    if(oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
      oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout newLayout = subIt->state().oldLayout;
    subIt->state().newLayout = subIt->state().oldLayout;
    if(newLayout == UNKNOWN_PREV_IMG_LAYOUT || newLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
      // contents discarded, no barrier necessary
      continue;
    }
    SanitiseReplayImageLayout(oldLayout);
    SanitiseReplayImageLayout(newLayout);
    if(oldLayout != VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
      // Transitioning back to PREINITIALIZED; this is impossible, so transition to GENERAL instead.
      newLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    uint32_t srcQueueFamilyIndex = subIt->state().newQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex = subIt->state().oldQueueFamilyIndex;

    if(srcQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL ||
       srcQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT)
    {
      srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }
    if(dstQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL ||
       dstQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT)
    {
      dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    uint32_t submitQueueFamilyIndex = srcQueueFamilyIndex;

    if(GetImageInfo().sharingMode == VK_SHARING_MODE_EXCLUSIVE)
    {
      if(srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
      {
        submitQueueFamilyIndex = dstQueueFamilyIndex;
        dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      }
      else if(dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
      {
        srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      }
    }
    else
    {
      if(submitQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
        submitQueueFamilyIndex = dstQueueFamilyIndex;
      srcQueueFamilyIndex = dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    if(srcQueueFamilyIndex == dstQueueFamilyIndex && oldLayout == newLayout)
    {
      subIt->state().newQueueFamilyIndex = subIt->state().oldQueueFamilyIndex;
      continue;
    }

    if(submitQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
    {
      RDCWARN(
          "ResetToOldState: barrier submitted to VK_QUEUE_FAMILY_IGNORED; defaulting to queue "
          "family %u",
          info.defaultQueueFamilyIndex);
      submitQueueFamilyIndex = info.defaultQueueFamilyIndex;
    }
    subIt->state().newQueueFamilyIndex = subIt->state().oldQueueFamilyIndex;

    ImageSubresourceRange subRange = subIt->range();

    if(subRange.baseDepthSlice != 0)
    {
      // We can't issue barriers per depth slice, so skip the barriers for non-zero depth slices.
      // The zero depth slice barrier will implicitly cover the non-zero depth slices.
      continue;
    }

    if((GetImageInfo().Aspects() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ==
           (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) &&
       !info.separateDepthStencil)
    {
      // This is a subresource of a depth and stencil image, and
      // VK_KHR_separate_depth_stencil_layouts is not enabled, so the barrier needs to include both
      // depth and stencil aspects. We skip the stencil-only aspect and expand the barrier for the
      // depth-only aspect to include both depth and stencil aspects.
      if(subRange.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
        continue;
      if(subRange.aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
        subRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageMemoryBarrier barrier = {
        /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /* pNext = */ NULL,
        /* srcAccessMask = */ srcAccessMask,
        /* dstAccessMask = */ dstAccessMask,
        /* oldLayout = */ oldLayout,
        /* newLayout = */ newLayout,
        /* srcQueueFamilyIndex = */ srcQueueFamilyIndex,
        /* dstQueueFamilyIndex = */ dstQueueFamilyIndex,
        /* image = */ wrappedHandle,
        /* subresourceRange = */ subRange,
    };
    barriers.AddWrapped(MAIN_BATCH_INDEX, submitQueueFamilyIndex, barrier);

    // acquire the subresource in the dstQueueFamily, if necessary
    if(barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex)
    {
      barriers.AddWrapped(ACQUIRE_BATCH_INDEX, barrier.dstQueueFamilyIndex, barrier);
    }
  }
  RestoreTransfers(RESTORE_TRANSFERS_BATCH_INDEX, oldQueueFamilyTransfers, srcAccessMask, barriers,
                   info);
}

void ImageState::Transition(const ImageState &dstState, VkAccessFlags srcAccessMask,
                            VkAccessFlags dstAccessMask, ImageBarrierSequence &barriers,
                            ImageTransitionInfo info)
{
  const uint32_t CLOSE_TRANSFERS_BATCH_INDEX = 0;
  const uint32_t MAIN_BATCH_INDEX = 1;
  const uint32_t ACQUIRE_BATCH_INDEX = 2;
  const uint32_t RESTORE_TRANSFERS_BATCH_INDEX = 3;
  CloseTransfers(CLOSE_TRANSFERS_BATCH_INDEX, dstAccessMask, barriers, info);

  for(auto dstIt = dstState.subresourceStates.begin(); dstIt != dstState.subresourceStates.end();
      ++dstIt)
  {
    const ImageSubresourceRange &dstRng = dstIt->range();
    const ImageSubresourceState &dstSub = dstIt->state();
    for(auto it = subresourceStates.RangeBegin(dstRng); it != subresourceStates.end(); ++it)
    {
      ImageSubresourceState srcSub;

      // ignore transitions of subresources that were untouched if this isn't the *canonical* image
      // state, but just an overlay tracking changes within a command buffer
      if(it->state() == ImageSubresourceState() && m_Overlay)
        continue;

      if(!it->state().Update(dstSub, srcSub, info.GetFrameRefCompFunc()))
        // subresource state did not change, so no need for a barrier
        continue;

      subresourceStates.Split(dstRng);
      std::swap(it->state(), srcSub);

      ImageSubresourceRange srcRng = it->range();

      VkImageLayout oldLayout = srcSub.newLayout;
      if(oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
        oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      VkImageLayout newLayout = dstSub.newLayout;
      if(newLayout == UNKNOWN_PREV_IMG_LAYOUT || newLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        // ignore transitions to undefined
        continue;
      uint32_t srcQueueFamilyIndex = srcSub.newQueueFamilyIndex;
      uint32_t dstQueueFamilyIndex = dstSub.newQueueFamilyIndex;

      if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        // transitions from undefined discard the contents anyway, so no queue family ownership
        // transfer is necessary
        srcQueueFamilyIndex = dstQueueFamilyIndex;

      if(newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && oldLayout != VK_IMAGE_LAYOUT_PREINITIALIZED)
      {
        // Transitioning to PREINITIALIZED, which is invalid. This happens when we are resetting to
        // an earlier image state.
        // Instead, we transition to GENERAL, and make the image owned by oldQueueFamilyIndex.
        newLayout = VK_IMAGE_LAYOUT_GENERAL;
        dstQueueFamilyIndex = srcSub.oldQueueFamilyIndex;
        RDCASSERT(dstQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED);
      }

      if(IsReplayMode(info.capState))
      {
        // Get rid of PRESENT layouts
        SanitiseReplayImageLayout(oldLayout);
        SanitiseReplayImageLayout(newLayout);
      }

      uint32_t submitQueueFamilyIndex = (srcQueueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
                                            ? srcQueueFamilyIndex
                                            : dstQueueFamilyIndex;
      if(submitQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED ||
         submitQueueFamilyIndex == VK_QUEUE_FAMILY_EXTERNAL ||
         submitQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT)
      {
        RDCERR("Ignoring state transition submitted to invalid queue family %u",
               submitQueueFamilyIndex);
        continue;
      }
      if(GetImageInfo().sharingMode == VK_SHARING_MODE_CONCURRENT)
      {
        srcQueueFamilyIndex = dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      }
      else
      {
        if(srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
        {
          RDCWARN("ImageState::Transition: src queue family == VK_QUEUE_FAMILY_IGNORED.");
          srcQueueFamilyIndex = dstQueueFamilyIndex;
        }
        if(dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
        {
          RDCWARN("ImageState::Transition: dst queue family == VK_QUEUE_FAMILY_IGNORED.");
          dstQueueFamilyIndex = srcQueueFamilyIndex;
        }
      }

      if(srcQueueFamilyIndex == dstQueueFamilyIndex && oldLayout == newLayout)
        // Skip the barriers, because it would do nothing
        continue;

      if(srcRng.baseDepthSlice != 0 || dstRng.baseDepthSlice != 0)
      {
        // We can't issue barriers per depth slice, so skip the barriers for non-zero depth slices.
        // The zero depth slice barrier will implicitly cover the non-zerp depth slices.
        continue;
      }

      VkImageAspectFlags aspectMask = srcRng.aspectMask & dstRng.aspectMask;
      if((GetImageInfo().Aspects() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ==
             (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) &&
         !info.separateDepthStencil)
      {
        // This is a subresource of a depth and stencil image, and
        // VK_KHR_separate_depth_stencil_layouts is not enabled, so the barrier needs to include
        // both depth and stencil aspects. We skip the stencil-only aspect and expand the barrier
        // for the depth-only aspect to include both depth and stencil aspects.
        if(aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
          continue;
        if(aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT)
          aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      uint32_t baseMipLevel = RDCMAX(dstRng.baseMipLevel, srcRng.baseMipLevel);
      uint32_t endMipLevel =
          RDCMIN(dstRng.baseMipLevel + dstRng.levelCount, srcRng.baseMipLevel + srcRng.levelCount);
      uint32_t baseArrayLayer = RDCMAX(dstRng.baseArrayLayer, srcRng.baseArrayLayer);
      uint32_t endArrayLayer = RDCMIN(dstRng.baseArrayLayer + dstRng.layerCount,
                                      srcRng.baseArrayLayer + srcRng.layerCount);
      VkImageMemoryBarrier barrier = {
          /* sType = */ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          /* pNext = */ NULL,
          /* srcAccessMask = */ srcAccessMask,
          /* dstAccessMask = */ dstAccessMask,
          /* oldLayout = */ oldLayout,
          /* newLayout = */ newLayout,
          /* srcQueueFamilyIndex = */ srcQueueFamilyIndex,
          /* dstQueueFamilyIndex = */ dstQueueFamilyIndex,
          /* image = */ wrappedHandle,
          /* subresourceRange = */
          {
              /* aspectMask = */ aspectMask,
              /* baseMipLevel = */ baseMipLevel,
              /* levelCount = */ endMipLevel - baseMipLevel,
              /* baseArrayLayer = */ baseArrayLayer,
              /* layerCount = */ endArrayLayer - baseArrayLayer,
          },
      };
      barriers.AddWrapped(MAIN_BATCH_INDEX, submitQueueFamilyIndex, barrier);

      // acquire the subresource in the dstQueueFamily, if necessary
      if(barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex)
      {
        barriers.AddWrapped(ACQUIRE_BATCH_INDEX, barrier.dstQueueFamilyIndex, barrier);
      }
    }
  }
  RestoreTransfers(RESTORE_TRANSFERS_BATCH_INDEX, dstState.newQueueFamilyTransfers, srcAccessMask,
                   barriers, info);
}

void ImageState::Transition(uint32_t queueFamilyIndex, VkImageLayout layout,
                            VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                            ImageBarrierSequence &barriers, ImageTransitionInfo info)
{
  Transition(UniformState(ImageSubresourceState(queueFamilyIndex, layout)), srcAccessMask,
             dstAccessMask, barriers, info);
}

void ImageState::TempTransition(const ImageState &dstState, VkAccessFlags preSrcAccessMask,
                                VkAccessFlags preDstAccessMask, VkAccessFlags postSrcAccessmask,
                                VkAccessFlags postDstAccessMask, ImageBarrierSequence &setupBarriers,
                                ImageBarrierSequence &cleanupBarriers, ImageTransitionInfo info) const
{
  ImageState temp(*this);
  temp.Transition(dstState, preSrcAccessMask, preDstAccessMask, setupBarriers, info);
  temp.Transition(*this, postSrcAccessmask, postDstAccessMask, cleanupBarriers, info);
}

void ImageState::TempTransition(uint32_t queueFamilyIndex, VkImageLayout layout,
                                VkAccessFlags accessMask, ImageBarrierSequence &setupBarriers,
                                ImageBarrierSequence &cleanupBarriers, ImageTransitionInfo info) const
{
  TempTransition(UniformState(ImageSubresourceState(queueFamilyIndex, layout)),
                 VK_ACCESS_ALL_WRITE_BITS, accessMask, accessMask, VK_ACCESS_ALL_READ_BITS,
                 setupBarriers, cleanupBarriers, info);
}

void ImageState::InlineTransition(VkCommandBuffer cmd, uint32_t queueFamilyIndex,
                                  const ImageState &dstState, VkAccessFlags srcAccessMask,
                                  VkAccessFlags dstAccessMask, ImageTransitionInfo info)
{
  ImageBarrierSequence barriers;
  Transition(dstState, srcAccessMask, dstAccessMask, barriers, info);
  if(barriers.empty())
    return;
  rdcarray<VkImageMemoryBarrier> barriersArray;
  barriers.ExtractFirstUnwrappedBatchForQueue(queueFamilyIndex, barriersArray);
  if(!barriersArray.empty())
    DoPipelineBarrier(cmd, (uint32_t)barriersArray.size(), barriersArray.data());
  if(!barriers.empty())
  {
    RDCERR("Could not inline all image state transition barriers");
  }
}

void ImageState::InlineTransition(VkCommandBuffer cmd, uint32_t queueFamilyIndex,
                                  VkImageLayout layout, VkAccessFlags srcAccessMask,
                                  VkAccessFlags dstAccessMask, ImageTransitionInfo info)
{
  InlineTransition(cmd, queueFamilyIndex,
                   UniformState(ImageSubresourceState(queueFamilyIndex, layout)), srcAccessMask,
                   dstAccessMask, info);
}

InitReqType ImageState::MaxInitReq(const ImageSubresourceRange &range, InitPolicy policy,
                                   bool initialized) const
{
  FrameRefType refType = eFrameRef_None;
  for(auto it = subresourceStates.RangeBegin(range); it != subresourceStates.end(); ++it)
  {
    refType = ComposeFrameRefsDisjoint(refType, it->state().refType);
  }
  return InitReq(refType, policy, initialized);
}

VkImageLayout ImageState::GetImageLayout(VkImageAspectFlagBits aspect, uint32_t mipLevel,
                                         uint32_t arrayLayer) const
{
  return subresourceStates.SubresourceAspectValue(aspect, mipLevel, arrayLayer, 0).newLayout;
}

void ImageState::BeginCapture()
{
  maxRefType = eFrameRef_None;

  // Forget any pending queue family release operations.
  // If the matching queue family acquire operation happens during the frame,
  // an implicit release operation will be put into `oldQueueFamilyTransfers`.
  newQueueFamilyTransfers.clear();

  // Also clear implicit queue family acquire operations because these correspond to release
  // operations already submitted (and therefore not part of the capture).
  oldQueueFamilyTransfers.clear();

  for(auto it = subresourceStates.begin(); it != subresourceStates.end(); ++it)
  {
    ImageSubresourceState state = it->state();
    state.oldLayout = state.newLayout;
    state.oldQueueFamilyIndex = state.newQueueFamilyIndex;
    state.refType = eFrameRef_None;
    it->SetState(state);
  }
}

void ImageState::FixupStorageReferences()
{
  if(m_Storage)
  {
    // storage images we don't track the reference to because they're in descriptor sets, so the
    // read/write state of them is unknown. We can't allow a 'completewrite' to be used as-is
    // because
    // there might be a read before then which we just didn't track at the time.
    maxRefType = ComposeFrameRefsUnordered(maxRefType, eFrameRef_ReadBeforeWrite);

    for(auto it = subresourceStates.begin(); it != subresourceStates.end(); ++it)
    {
      ImageSubresourceState state = it->state();
      state.refType = ComposeFrameRefsUnordered(state.refType, eFrameRef_ReadBeforeWrite);
      it->SetState(state);
    }
  }
}
