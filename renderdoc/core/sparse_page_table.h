/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "api/replay/data_types.h"
#include "api/replay/rdcarray.h"
#include "api/replay/rdcpair.h"
#include "api/replay/resourceid.h"
#include "api/replay/stringise.h"

namespace Sparse
{
class PageTable;
};    // namespace Sparse

// we pre-declare this function so we can make it a friend inside the PageTable implementation
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, Sparse::PageTable &el);

namespace Sparse
{
// used for co-ordinates as well as dimensions
struct Coord
{
  uint32_t x, y, z;

  bool operator==(const Coord &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct Page
{
  ResourceId memory;
  uint64_t offset;

  bool operator==(const Page &o) const { return memory == o.memory && offset == o.offset; }
};

struct PageRangeMapping
{
  bool hasSingleMapping() const { return pages.empty(); }
  // the memory mapping if there's a single mapping. Only valid if pages below is empty
  Page singleMapping;

  // since with a single mapping we only store the 'base' page, we need an additional bool to
  // indicate if it's a single page re-used or if it's all subsequent pages used.
  bool singlePageReused = false;

  // the memory mappings per-page if there are different mappings per-page
  rdcarray<Page> pages;

  bool isMapped() const { return !pages.empty() || singleMapping.memory != ResourceId(); }
  void simplifyUnmapped()
  {
    // if we're already using singleMapping, don't check anything
    if(pages.empty())
      return;

    // if we find a single page with memory mapped, we're not entirely unmapped
    for(size_t i = 0; i < pages.size(); i++)
      if(pages[i].memory != ResourceId())
        return;

    // we're entirely unmapped - revert back to a single page mapping
    pages.clear();
    singleMapping = Page();
    singlePageReused = false;
  }
  Page getPage(uint32_t idx, uint32_t pageSize) const
  {
    if(pages.empty())
    {
      if(singlePageReused)
        return singleMapping;

      Page ret = singleMapping;
      ret.offset += pageSize * idx;
      return ret;
    }

    return pages[idx];
  }

  void createPages(uint32_t numPages, uint32_t pageSize);
};

struct MipTail
{
  // the first mip that is in the mip tail
  uint32_t firstMip = 0;

  // the offset in bytes for the mip tail.
  uint64_t byteOffset = 0;

  // the stride in bytes for the mip tail between each array slice's mip stride. This is set to 0
  // if there is only one mip tail
  uint64_t byteStride = 0;

  // the size in bytes for the mip tail
  uint64_t totalPackedByteSize = 0;

  // the pages for the mip tail (or buffer pages for buffers). If byteStride is 0 this is the
  // single set of pages for all array slices. If byteStride is non-zero then the first N pages
  // (with N = byteStride / pageByteSize) are for slice 0, the next N for slice 1, etc.
  rdcarray<PageRangeMapping> mappings;
};

// shadows the page table for a sparse resource - buffer or texture - and handles updates and
// retrieval. Currently the system is simple - we first store a page mapping per subresource. This
// should hopefully cover most applications and avoids needing to allocate, update, and store/apply
// a whole page table. If we detect a partial update we allocate a full page table and store the
// mapping for each page.
class PageTable
{
public:
  // initialise the page table for a buffer. We just need to know its size and the page size
  void Initialise(uint64_t bufferByteSize, uint32_t pageByteSize);
  // initialise the page table for a texture. We specify various properties about the texture and
  // its page shape/size and mip tail.
  // The mip tail starts at firstTailMip - if this is >= numMips then there is no mip tail.
  // mipTailOffset is an arbitrary offset which all mip tail bindings are relative to. It does
  // nothing but rebase the offset provided setMipTailRange.
  // mipTailStride is the byte stride between mip tails if they are stored separately per slice in
  // an array texture. Again this is arbitrary to offset the binding resource offset. If the stride
  // is 0 this means all slices have their mip tails consecutively packed.
  // mipTailTotalPackedSize is the *total packed size of the mip tails*. This can be calculated with
  // the size of one array slice's mip tail multiplied by the number of array slices. If the stride
  // is 0 this is the size of the whole mip tail of the resource.
  void Initialise(const Coord &overallTexelDim, uint32_t numMips, uint32_t numArraySlices,
                  uint32_t pageByteSize, const Coord &pageTexelDim, uint32_t firstTailMip,
                  uint64_t mipTailOffset, uint64_t mipTailStride, uint64_t mipTailTotalPackedSize);

  uint64_t GetSerialiseSize() const;

  inline uint32_t getPageByteSize() const { return m_PageByteSize; }
  inline Coord getPageTexelSize() const { return m_PageTexelSize; }
  inline Coord getResourceSize() const { return m_TextureDim; }
  // useful for D3D where the mip tail is indexed by subresource/array slice even if we treat it all
  // as one
  inline uint64_t getMipTailByteOffsetForSubresource(uint32_t subresource) const
  {
    const uint32_t arraySlice = (subresource / m_MipCount) % m_ArraySize;
    return m_MipTail.byteOffset + m_MipTail.byteStride * arraySlice;
  }

  inline uint32_t calcSubresource(uint32_t arraySlice, uint32_t mipLevel) const
  {
    return arraySlice * m_MipCount + mipLevel;
  }
  Coord calcSubresourcePageDim(uint32_t subresource) const;

  // is this subresource in the mip tail
  inline bool isSubresourceInMipTail(uint32_t subresource) const
  {
    const uint32_t mipLevel = subresource % m_MipCount;
    return mipLevel >= m_MipTail.firstMip;
  }
  // is this byte offset in the resource (according to the mip tail - no other offsets are known)
  inline bool isByteOffsetInResource(uint64_t byteOffset) const
  {
    const uint64_t mipTailSize = m_MipTail.byteStride == 0 ? m_MipTail.totalPackedByteSize
                                                           : m_MipTail.byteStride * m_ArraySize;
    return byteOffset >= m_MipTail.byteOffset && byteOffset < m_MipTail.byteOffset + mipTailSize;
  }

  // read-only accessors to get current state
  uint32_t getNumSubresources() const { return (uint32_t)m_Subresources.size(); }
  uint32_t getArraySize() const { return m_ArraySize; }
  uint32_t getMipCount() const { return m_MipCount; }
  const PageRangeMapping &getSubresource(uint32_t subresource) const
  {
    return m_Subresources[subresource];
  }
  const PageRangeMapping &getPageRangeMapping(uint32_t subresource) const
  {
    if(isSubresourceInMipTail(subresource))
      return getMipTailMapping(subresource);
    else
      return getSubresource(subresource);
  }
  const MipTail &getMipTail() const { return m_MipTail; }
  const PageRangeMapping &getMipTailMapping(uint32_t subresource) const
  {
    const uint32_t arraySlice = (subresource / m_MipCount) % m_ArraySize;
    return m_MipTail.mappings[arraySlice];
  }
  uint64_t getMipTailSliceSize() const { return m_MipTail.totalPackedByteSize / m_ArraySize; }
  uint64_t getSubresourceByteSize(uint32_t subresource) const
  {
    const Coord subresourcePageDim = calcSubresourcePageDim(subresource);

    return subresourcePageDim.x * subresourcePageDim.y * subresourcePageDim.z * m_PageByteSize;
  }

  // set a contiguous range of pages, with offsets and sizes applied in bytes.
  // This is when you are setting XYZ resource pages to point to ABC memory pages.
  // useSinglePage means only one page of memory will be used for all pages in the resource. Think
  // of the case of mapping a single 'black' page to large areas of the resource, or NULL'ing out
  // mappings.
  // as a convenience it returns the resource offset where it finishes, since D3D allows
  // mismatched boundaries for tile ranges and memory regions
  uint64_t setMipTailRange(uint64_t resourceByteOffset, ResourceId memory,
                           uint64_t memoryByteOffset, uint64_t byteSize, bool useSinglePage);
  inline uint64_t setBufferRange(uint64_t resourceByteOffset, ResourceId memory,
                                 uint64_t memoryByteOffset, uint64_t byteSize, bool useSinglePage)
  {
    return setMipTailRange(resourceByteOffset, memory, memoryByteOffset, byteSize, useSinglePage);
  }

  // set a 3D box of texel pages to a range of memory.
  // useSinglePage means only one page of memory will be used for all pages in the resource. Think
  // of the case of mapping a single 'black' page to large areas of the resource, or NULL'ing out
  // mappings.
  void setImageBoxRange(uint32_t subresource, const Coord &coord, const Coord &dim,
                        ResourceId memory, uint64_t memoryByteOffset, bool useSinglePage);

  // set a series of tiles in x, y, z order starting at a given point and wrapping around the
  // overall image dimensions. In theory you could e.g. map 10 pages starting at page 6, when there
  // are only 8 pages to a row. It would update pages 6 and 7 in the first row then all pages in the
  // next row. This also allows wrapping from one subresource to the next
  // we accept a byte size for consistency with all other calls that work in bytes not pages, even
  // though this is only expected to be used by D3D which has this wrapping update operation and it
  // operates in pages - the calling code can mutiply by getPageByteSize().
  // useSinglePage means only one page of memory will be used for all pages in the resource. Think
  // of the case of mapping a single 'black' page to large areas of the resource, or NULL'ing out
  // mappings
  // as a convenience it returns the co-ordinate and subresource where it finishes, since D3D allows
  // mismatched boundaries for tile ranges and memory regions. This interacts with the
  // updateMappings parameter which can skip applying any bindings and only advances the coord
  // 'cursor'
  rdcpair<uint32_t, Coord> setImageWrappedRange(uint32_t dstSubresource, const Coord &coord,
                                                uint64_t byteSize, ResourceId memory,
                                                uint64_t memoryByteOffset, bool useSinglePage,
                                                bool updateMappings = true);

  // copy pages from another page table, in D3D fashion. These take co-ordinates and dimensions in
  // tiles because copying between textures with different tile shapes seems possible and if we
  // expect inputs in texels we have to specify which page table's texel dimensions we're using,
  // which is probably more error prone than breaking the convention of not accepting parameters in
  // tiles.
  void copyImageBoxRange(uint32_t dstSubresource, const Coord &coordInTiles,
                         const Coord &dimInTiles, const PageTable &srcPageTable,
                         uint32_t srcSubresource, const Coord &srcCoordInTiles);

  void copyImageWrappedRange(uint32_t subresource, const Coord &coordInTiles, uint64_t numTiles,
                             const PageTable &srcPageTable, uint32_t srcSubresource,
                             const Coord &srcCoordInTiles);

private:
  PageRangeMapping &getMipTailMapping(uint32_t subresource)
  {
    const uint32_t arraySlice = (subresource / m_MipCount) % m_ArraySize;
    return m_MipTail.mappings[arraySlice];
  }

  // The image dimensions that we need. We don't care about the format or anything, we just need to
  // know the size in pages and the subresource setup
  Coord m_TextureDim = {};
  uint32_t m_MipCount = 1;
  uint32_t m_ArraySize = 1;

  // the byte size of a page, constant over a resource
  uint32_t m_PageByteSize = 0;

  // the size of a page in texels
  Coord m_PageTexelSize = {};

  // the page tables for each subresource, if this is an image. Note for buffers everything goes in
  // the "mipTail".
  // For simplicity and robustness of access every subresource has an entry here, even those
  // corresponding to mips that are in mip tails - the overhead is nominal with each entry being
  // 5*ptrsize = 40 bytes.
  rdcarray<PageRangeMapping> m_Subresources;

  MipTail m_MipTail;

  template <typename SerialiserType>
  friend void ::DoSerialise(SerialiserType &ser, PageTable &el);
};

};    // namespace Sparse

DECLARE_REFLECTION_STRUCT(Sparse::Coord);
DECLARE_REFLECTION_STRUCT(Sparse::Page);
DECLARE_REFLECTION_STRUCT(Sparse::PageRangeMapping);
DECLARE_REFLECTION_STRUCT(Sparse::MipTail);
DECLARE_REFLECTION_STRUCT(Sparse::PageTable);
