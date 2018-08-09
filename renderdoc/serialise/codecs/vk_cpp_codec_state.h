/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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

#include "ext_object.h"

namespace vk_cpp_codec
{
struct InitResourceDesc
{
  ExtObject *sdobj;

  inline InitResourceDesc() : sdobj(NULL) {}
  inline InitResourceDesc(ExtObject *ext, bool reset) : sdobj(ext) {}
};

typedef std::map<uint64_t, InitResourceDesc> InitResourceIDMap;
typedef InitResourceIDMap::iterator InitResourceIDMapIter;
typedef std::pair<uint64_t, InitResourceDesc> InitResourceIDMapPair;

typedef std::map<uint64_t, ExtObjectVec> ExtObjectVecIDMap;
typedef ExtObjectVecIDMap::iterator ExtObjectVecIDMapIter;
typedef std::pair<uint64_t, ExtObjectVec> ExtObjectVecIDMapPair;

// Enum representing the reset requirement.
enum ResetRequirement
{
  RESET_REQUIREMENT_UNKNOWN = 0,    // reset requirement is unknown (possibly just not yet computed)
  RESET_REQUIREMENT_RESET = 1,      // reset is required before each frame
  RESET_REQUIREMENT_INIT = 2,       // initialization is required, but no reset between frames
  RESET_REQUIREMENT_NO_RESET = 3,    // no reset is required
};

// This structure describes a resource binding information.
struct BoundResource
{
  ExtObject *createSDObj;    // create call for the bound resource
  ExtObject *bindSDObj;      // binding call
  ExtObject *resource;       // resource ID
  ExtObject *requirement;    // serialized memory requirements
  ExtObject *offset;         // binding offset
  ResetRequirement reset;
};

typedef std::vector<BoundResource> BoundResources;
typedef BoundResources::iterator BoundResourcesIter;

// This structure describes a resource memory range.
struct MemRange
{
  uint64_t start = 0;
  uint64_t end = 0;
  MemRange MakeRange(ExtObject *offset, ExtObject *reqs);
  bool Intersect(MemRange &r);
};

/*************************************************************
State machine diagram for AccessState/AccessAction.
- The states are labeled in CAPS (INIT, READ, WRITE, CLEAR, RESET)
- The actions are labeled lower case (read, write, clear).
- All the actions that are not shown are loops
(e.g. a `read` action in the CLEAR state remains in the CLEAR state)

+--------INIT-----------+
|          |            |
read|          |write       |clear
|          |            |
V   read   V   clear    V
READ<------WRITE------->CLEAR
|
|write
|clear
V
RESET

*************************************************************/

// AccessState is used to store whether an image or memory range has been read, written, or both,
// and whether a reset is required.
enum AccessState
{
  // Resource has not been read or written
  ACCESS_STATE_INIT = 0,

  // Some regions of the resource may have been read; all reads occurred after all writes.
  ACCESS_STATE_READ = 1,

  // Some regions of the resource may have been written, but nothing has been read.
  ACCESS_STATE_WRITE = 2,

  // The entire resource was reset, without reading the initial contents
  ACCESS_STATE_CLEAR = 3,

  // Some piece of resource may have been read and later written, requiring a reset.
  ACCESS_STATE_RESET = 4,
};

// Memory action encodes the possible effects on a region of memory.
enum AccessAction
{
  ACCESS_ACTION_NONE = 0,

  // Write the some regions of the resource
  ACCESS_ACTION_WRITE = 1,

  // Read some regions of the resource
  ACCESS_ACTION_READ = 2,

  // Overwrite the entire resource, ignoring the previous contents
  ACCESS_ACTION_CLEAR = 4,

  // Write some regions of the memory after possible reading some regions of the resource.
  // Equivalent to a ACCESS_ACTION_READ followed by ACCESS_ACTION_WRITE.
  ACCESS_ACTION_READ_WRITE = ACCESS_ACTION_WRITE | ACCESS_ACTION_READ,
};

// Returns the new AccessState resulting from clearing the entire resource
AccessState AccessStateClearTransition(AccessState s);

// Returns the new AccessState resulting from writing to some regions of the resource
AccessState AccessStateWriteTransition(AccessState s);

// Returns the new AccessState resulting from reading the resource
AccessState AccessStateReadTransition(AccessState s);

// Returns the new AccessState resulting from reading some regions of the resource, and then
// writing some regions of the resource
AccessState AccessStateReadWriteTransition(AccessState s);

// Given an action, returns a function mapping the old state of a resource to the new state of that
// resource
std::function<AccessState(AccessState)> GetAccessStateTransition(AccessAction action);

struct MemoryState
{
  // The "current" access state (read/write) of the subresource.
  // Updated by the command analysis functions called from CodeTracker::AnalyzeInitResources.
  AccessState accessState = ACCESS_STATE_INIT;

  // The queue family owning the subresource at the beginning of the frame.
  uint64_t startQueueFamily = VK_QUEUE_FAMILY_IGNORED;

  // The "current" queue family owning the subresource
  // Updated by the command analysis functions called from CodeTracker::AnalyzeInitResources.
  uint64_t queueFamily = VK_QUEUE_FAMILY_IGNORED;

  // Indicates whether this memory region is currently acquired by a queue family.
  bool isAcquired = false;

  inline bool operator==(const MemoryState &rhs) const
  {
    return accessState == rhs.accessState && startQueueFamily == rhs.startQueueFamily &&
           queueFamily == rhs.queueFamily && isAcquired == rhs.isAcquired;
  }
  inline bool operator!=(const MemoryState &rhs) const
  {
    return accessState != rhs.accessState || startQueueFamily != rhs.startQueueFamily ||
           queueFamily != rhs.queueFamily || isAcquired != rhs.isAcquired;
  }
};

// This structure describes a memory allocation (described through RenderDoc's
// SDObject) and the list of all resources that are bound to that allocation.
// It stores the list of memory ranges, which is used to keep track of
// overlapping resources and detect resource aliasing.
struct MemoryAllocationWithBoundResources
{
  enum HasAliasedResources
  {
    HasAliasedResourcesFalse,
    HasAliasedResourcesTrue,
    HasAliasedResourcesUnknown
  };
  ExtObject *allocateSDObj = NULL;
  BoundResources boundResources;
  std::vector<MemRange> ranges;
  Intervals<MemoryState> memoryState;
  uint64_t hasAliasedResources = HasAliasedResourcesUnknown;

  inline MemoryAllocationWithBoundResources(ExtObject *allocateExt = NULL)
      : allocateSDObj(allocateExt)
  {
  }
  inline size_t BoundResourceCount() { return boundResources.size(); }
  inline BoundResourcesIter FirstBoundResource() { return boundResources.begin(); }
  inline BoundResourcesIter EndOfBoundResources() { return boundResources.end(); }
  inline void Add(BoundResource &r) { boundResources.push_back(r); }
  bool HasAliasedResources();
  bool NeedsReset();
  bool NeedsInit();
  std::vector<size_t> BoundResourcesOrderByResetRequiremnet();
  bool CheckAliasedResources(MemRange r);
  void Access(uint64_t cmdQueueFamily, VkSharingMode sharingMode, AccessAction action,
              uint64_t offset, uint64_t size);
  void TransitionQueueFamily(uint64_t cmdQueueFamily, VkSharingMode sharingMode,
                             uint64_t srcQueueFamily, uint64_t dstQueueFamily, uint64_t offset,
                             uint64_t size);
};

// For each memory allocation ID, the map type below store allocation
// create info structure along with the list of bound resource.
typedef std::map<uint64_t, MemoryAllocationWithBoundResources> MemAllocWithResourcesMap;
typedef MemAllocWithResourcesMap::iterator MemAllocWithResourcesMapIter;
typedef std::pair<uint64_t, MemoryAllocationWithBoundResources> MemAllocWithResourcesMapPair;

// Keep this as a wrapper around create RenderDoc's SDObject
// because this will help keep track of all the views
// and operations associated with a resource. This is needed to
// find the proper initial state and to determine if a resource
// needs an expensive memory reset before each frame render.
struct ResourceWithViews
{
  ExtObject *sdobj;
  ExtObjectIDMap views;
};

typedef std::map<uint64_t, ResourceWithViews> ResourceWithViewsMap;
typedef ResourceWithViewsMap::iterator ResourceWithViewsMapIter;
typedef std::pair<uint64_t, ResourceWithViews> ResourceWithViewsMapPair;

typedef std::map<uint64_t, int64_t> U64Map;
typedef U64Map::iterator U64MapIter;
typedef std::pair<uint64_t, uint64_t> U64MapPair;

struct MemStateUpdates
{
  ExtObjectVec descset;
  ExtObjectVec memory;
};

struct CmdBufferRecord
{
  ExtObject *sdobject;    // command buffer begin sdobject
  ExtObject *cb;
  ExtObjectVec cmds;    // commands
};

struct QueueSubmit
{
  ExtObject *sdobject;         // queue submit sdobject
  ExtObject *q;                // queue
  uint64_t memory_updates;     // # of completed updates
  uint64_t descset_updates;    // # of completed updates
};

typedef std::vector<QueueSubmit> QueueSubmits;
typedef std::vector<QueueSubmit>::iterator QueueSubmitsIter;

struct FrameGraph
{
  QueueSubmits submits;
  MemStateUpdates updates;
  std::vector<CmdBufferRecord> records;

  inline void AddUnorderedSubmit(QueueSubmit qs) { submits.push_back(qs); }
  uint32_t FindCmdBufferIndex(ExtObject *o);
};

struct BoundBuffer
{
  uint64_t buffer = 0;
  uint64_t offset = 0;
  uint64_t size = 0;
  uint64_t dynamicOffset = 0;
  bool bound = false;

  inline BoundBuffer() {}
  inline BoundBuffer(uint64_t aBuffer, uint64_t aOffset, uint64_t aSize, uint64_t aDynamicOffset)
      : buffer(aBuffer), offset(aOffset), size(aSize), dynamicOffset(aDynamicOffset), bound(true)
  {
  }
};

struct BoundImage
{
  uint64_t sampler = 0;
  uint64_t imageView = 0;
  VkImageLayout imageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  bool bound = false;

  inline BoundImage() {}
  inline BoundImage(uint64_t aSampler, uint64_t aImageView, VkImageLayout aImageLayout)
      : sampler(aSampler), imageView(aImageView), imageLayout(aImageLayout), bound(true)
  {
  }
};

struct BoundTexelView
{
  uint64_t texelBufferView = 0;
  bool bound = false;

  inline BoundTexelView() {}
  inline BoundTexelView(uint64_t aTexelBufferView) : texelBufferView(aTexelBufferView), bound(true)
  {
  }
};

struct DescriptorBinding
{
  VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  std::vector<BoundImage> imageBindings;
  std::vector<BoundBuffer> bufferBindings;
  std::vector<BoundTexelView> texelViewBindings;
  std::vector<bool> updated;

  inline DescriptorBinding() {}
  inline DescriptorBinding(uint64_t type, uint64_t elementCount) { Resize(type, elementCount); }
  uint64_t Size();

  void SetBindingObj(uint64_t index, ExtObject *o, bool initialization);
  void CopyBinding(uint64_t index, const DescriptorBinding &other, uint64_t otherIndex);
  void Resize(uint64_t aType, uint64_t elementCount);
  bool NeedsReset(uint64_t element);
};

typedef std::map<uint64_t, DescriptorBinding> DescriptorBindingMap;
typedef std::map<uint64_t, DescriptorBinding>::iterator DescriptorBindingMapIter;
typedef std::pair<uint64_t, DescriptorBinding> DescriptorBindingMapPair;

struct DescriptorSetInfo
{
  uint64_t layout;    // ID of a parent vkDescriptorSetLayout object
  DescriptorBindingMap bindings;

  bool NeedsReset(uint64_t binding, uint64_t element);
};

typedef std::map<uint64_t, DescriptorSetInfo> DescriptorSetInfoMap;
typedef std::map<uint64_t, DescriptorSetInfo>::iterator DescriptorSetInfoMapIter;
typedef std::pair<uint64_t, DescriptorSetInfo> DescriptorSetInfoMapPair;

struct BoundPipeline
{
  // Identifier of the pipeline
  uint64_t pipeline = 0;

  // Map from the descriptor set number to the id of the bound descriptor set at that number
  U64Map descriptorSets;

  // Indicates whether a draw command has been found within the current subpass
  bool subpassHasDraw = false;

  inline BoundPipeline() {}
};

struct BindingState
{
  BoundPipeline graphicsPipeline;
  BoundPipeline computePipeline;
  std::map<uint64_t, BoundBuffer> vertexBuffers;    // key = binding number
  BoundBuffer indexBuffer;
  uint64_t indexBufferType = 0;
  ExtObject *renderPass = NULL;
  ExtObject *framebuffer = NULL;
  bool isFullRenderArea = false;
  std::vector<VkImageLayout> attachmentLayout;
  std::vector<uint64_t> attachmentFirstUse;
  std::vector<uint64_t> attachmentLastUse;
  uint64_t subpassIndex = 0;

  BindingState() {}
private:
  void attachmentUse(uint64_t subpassId, uint64_t attachmentId);

public:
  void BeginRenderPass(ExtObject *aRenderPass, ExtObject *aFramebuffer, ExtObject *aRenderArea);
};

struct ImageSubresource
{
  uint64_t image;
  VkImageAspectFlagBits aspect;
  uint64_t layer;
  uint64_t level;

  inline bool operator==(const ImageSubresource &rhs) const
  {
    return image == rhs.image && aspect == rhs.aspect && layer == rhs.layer && level == rhs.level;
  }

  inline bool operator!=(const ImageSubresource &rhs) const { return !operator==(rhs); }
  inline bool operator<(const ImageSubresource &rhs) const
  {
    return std::tie(image, aspect, layer, level) <
           std::tie(rhs.image, rhs.aspect, rhs.layer, rhs.level);
  }
};

class ImageSubresourceRangeIter;

struct ImageSubresourceRange
{
  uint64_t image;
  VkImageAspectFlags aspectMask;
  uint64_t baseMipLevel;
  uint64_t levelCount;
  uint64_t baseArrayLayer;
  uint64_t layerCount;
  ImageSubresourceRangeIter begin() const;
  ImageSubresourceRangeIter end() const;
};

// ImageSubresourceRangeIter iterates through an image subresource range (aspect, mip level, array
// layer).
// The iteration order is:
//   - For each aspect bit in aspectMask, in increasing order
//     - For each level in range (baseMipLevel .. baseMipLevel + levelCount)
//       - For each layer in  range (baseArrayLayer .. baseArrayLayer + layerCout)
//         - yield (aspect, level, layer)
class ImageSubresourceRangeIter
{
  static const VkImageAspectFlags VK_IMAGE_ASPECT_END_BIT = 0x00000080;
  ImageSubresource res;
  ImageSubresourceRange range;

  // Set this iterator into a common 'end' state.
  inline void setEnd()
  {
    res.level = (~0ULL) - 1;
    res.layer = (~0ULL) - 1;
    res.aspect = (VkImageAspectFlagBits)VK_IMAGE_ASPECT_END_BIT;
  }

public:
  ImageSubresourceRangeIter &operator++();
  inline ImageSubresourceRangeIter operator++(int)
  {
    ImageSubresourceRangeIter tmp(*this);
    operator++();
    return tmp;
  }
  inline bool operator==(const ImageSubresourceRangeIter &rhs) const { return res == rhs.res; }
  inline bool operator!=(const ImageSubresourceRangeIter &rhs) const { return res != rhs.res; }
  inline const ImageSubresource &operator*() { return res; }
  static ImageSubresourceRangeIter end(const ImageSubresourceRange &range);
  static ImageSubresourceRangeIter begin(const ImageSubresourceRange &range);
};

inline ImageSubresourceRangeIter ImageSubresourceRange::begin() const
{
  return ImageSubresourceRangeIter::begin(*this);
}
inline ImageSubresourceRangeIter ImageSubresourceRange::end() const
{
  return ImageSubresourceRangeIter::end(*this);
}

class ImageSubresourceState
{
  uint64_t image;
  VkImageAspectFlagBits aspect;
  uint64_t mipLevel;
  uint64_t layer;
  VkSharingMode sharingMode;

  // The "current" access state (read/write) of the subresource.
  // Updated by the command analysis functions called from CodeTracker::AnalyzeInitResources.
  AccessState accessState = ACCESS_STATE_INIT;

  // The layout of the subresource at the beginning of the frame.
  VkImageLayout startLayout = VK_IMAGE_LAYOUT_MAX_ENUM;

  // The "current" layout of the subresource.
  // Updated by the command analysis functions called from CodeTracker::AnalyzeInitResources.
  VkImageLayout layout = VK_IMAGE_LAYOUT_MAX_ENUM;

  // The queue family owning the subresource at the beginning of the frame.
  uint64_t startQueueFamily = VK_QUEUE_FAMILY_IGNORED;

  // The "current" queue family owning the subresource
  // Updated by the command analysis functions called from CodeTracker::AnalyzeInitResources.
  uint64_t queueFamily = VK_QUEUE_FAMILY_IGNORED;

  bool isInitialized = false;
  bool isTransitioned = false;
  bool isAcquiredByQueue = false;

  void CheckLayout(VkImageLayout requestedLayout);

  void CheckQueueFamily(uint64_t cmdQueueFamily);

public:
  inline ImageSubresourceState(uint64_t image, VkImageLayout initialLayout,
                               VkSharingMode aSharingMode, const ImageSubresource &res)
      : image(image),
        startLayout(initialLayout),
        layout(initialLayout),
        sharingMode(aSharingMode),
        aspect(res.aspect),
        mipLevel(res.level),
        layer(res.layer)
  {
  }
  void Initialize(VkImageLayout aStartLayout, uint64_t aStartQueueFamily);
  void Access(uint64_t cmdQueueFamily, VkImageLayout requestedLayout,
              const std::function<AccessState(AccessState)> &transition);
  void Transition(uint64_t cmdQueueFamily, VkImageLayout oldLayout, VkImageLayout newLayout,
                  uint64_t srcQueueFamily, uint64_t dstQueueFamily);

  inline AccessState AccessState() const { return accessState; }
  inline VkImageLayout StartLayout() const { return startLayout; }
  inline VkImageLayout Layout() const { return layout; }
  inline uint64_t StartQueueFamily() const { return startQueueFamily; }
  inline uint64_t QueueFamily() const { return queueFamily; }
  inline VkSharingMode SharingMode() const { return sharingMode; }
};

typedef std::map<ImageSubresource, ImageSubresourceState> ImageSubresourceStateMap;
typedef std::pair<ImageSubresource, ImageSubresourceState> ImageSubresourceStateMapPair;
typedef std::map<ImageSubresource, ImageSubresourceState>::iterator ImageSubresourceStateMapIter;
typedef std::map<ImageSubresource, ImageSubresourceState>::const_iterator ImageSubresourceStateMapConstIter;

struct ImageSubresourceRangeStateChanges
{
  VkImageLayout startLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  VkImageLayout endLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  bool sameStartLayout = true;
  bool sameEndLayout = true;

  // layoutChanged indicates whether any subresource in the range had a non-trivial layout change
  // between the start and end of the frame.
  // A layout change is "trivial" if either:
  //   - the start layout is VK_IMAGE_LAYOUT_UNDEFINED (no need to transition to UNDEFINED),
  //   - the start layout is VK_IMAGE_LAYOUT_MAX_ENUM (indicating no start layout was recorded while
  //   capturing), or
  //   - the end layout is VK_IMAGE_LAYOUT_MAX_ENUM (indicating the subresource was never used).
  bool layoutChanged = false;
  uint64_t startQueueFamily = VK_QUEUE_FAMILY_IGNORED;
  uint64_t endQueueFamily = VK_QUEUE_FAMILY_IGNORED;
  bool sameStartQueueFamily = true;
  bool sameEndQueueFamily = true;
  bool queueFamilyChanged = false;
};

class ImageState
{
  uint64_t image;
  ImageSubresourceStateMap subresourceStates;
  VkImageType type = VK_IMAGE_TYPE_2D;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkImageAspectFlags availableAspects = 0;
  uint64_t mipLevels = 0;
  uint64_t arrayLayers = 0;
  uint64_t width = 0;
  uint64_t height = 0;
  uint64_t depth = 0;
  VkImageLayout initialLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  VkSharingMode sharingMode = VK_SHARING_MODE_MAX_ENUM;

public:
  ImageSubresourceRange FullRange();

  inline ImageState() { RDCASSERT(0); }
  ImageState(uint64_t aImage, ExtObject *ci);

  VkImageAspectFlags NormalizeAspectMask(VkImageAspectFlags aspectMask) const;

  ImageSubresourceRange Range(VkImageAspectFlags aspectMask, uint64_t baseMipLevel,
                              uint64_t levelCount, uint64_t baseArrayLayer, uint64_t layerCount,
                              bool is2DView = false);

  ImageSubresourceRangeStateChanges RangeChanges(ImageSubresourceRange range) const;

  inline ImageSubresourceState &At(const ImageSubresource &res)
  {
    return subresourceStates.at(res);
  }
  inline const ImageSubresourceState &At(const ImageSubresource &res) const
  {
    return subresourceStates.at(res);
  }
  inline ImageSubresourceStateMapIter begin() { return subresourceStates.begin(); }
  inline ImageSubresourceStateMapConstIter begin() const { return subresourceStates.begin(); }
  inline ImageSubresourceStateMapIter end() { return subresourceStates.end(); }
  inline ImageSubresourceStateMapConstIter end() const { return subresourceStates.end(); }
  inline VkImageLayout InitialLayout() { return initialLayout; }
};

typedef std::map<uint64_t, ImageState> ImageStateMap;
typedef std::pair<uint64_t, ImageState> ImageStateMapPair;
typedef std::map<uint64_t, ImageState>::iterator ImageStateMapIter;

}    // namespace vk_cpp_codec