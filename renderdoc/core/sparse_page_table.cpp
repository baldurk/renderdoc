/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Baldur Karlsson
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

#include "sparse_page_table.h"
#include "common/globalconfig.h"
#include "serialise/serialiser.h"

namespace Sparse
{
void PageRangeMapping::createPages(uint32_t numPages, uint32_t pageSize)
{
  // don't do anything if the pages have already been populated
  if(!pages.empty())
    return;

  // otherwise allocate them. If we have a single page mapping we can just duplicate and it's easy
  if(singlePageReused || singleMapping.memory == ResourceId())
  {
    pages.fill(numPages, singleMapping);
  }
  else
  {
    // otherwise we need to allocate and increment
    pages.reserve(numPages);
    pages.clear();
    for(uint32_t i = 0; i < numPages; i++)
    {
      pages.push_back(singleMapping);
      singleMapping.offset += pageSize;
    }
  }

  // reset the single mapping to be super clear
  singleMapping = {};
  singlePageReused = false;
}

void PageTable::Initialise(uint64_t bufferByteSize, uint32_t pageByteSize)
{
  m_PageByteSize = pageByteSize;
  // set just in case the calling code calls getMipTailByteOffsetForSubresource
  m_ArraySize = 1;
  m_MipCount = 1;

  // initialise mip tail with buffer properties
  m_MipTail.byteStride = 0;
  m_MipTail.byteOffset = 0;
  m_MipTail.firstMip = 0;
  m_MipTail.totalPackedByteSize = bufferByteSize;
  m_MipTail.mappings.resize(1);
  m_MipTail.mappings[0].singlePageReused = false;
  m_MipTail.mappings[0].singleMapping = {};
}

void PageTable::Initialise(const Coord &overallTexelDim, uint32_t numMips, uint32_t numArraySlices,
                           uint32_t pageByteSize, const Sparse::Coord &pageTexelDim,
                           uint32_t firstTailMip, uint64_t mipTailOffset, uint64_t mipTailStride,
                           uint64_t mipTailTotalPackedSize)
{
  // sanitise inputs to be safe
  m_PageByteSize = RDCMAX(1U, pageByteSize);
  m_ArraySize = RDCMAX(1U, numArraySlices);
  m_MipCount = RDCMAX(1U, numMips);
  m_PageTexelSize.x = RDCMAX(1U, pageTexelDim.x);
  m_PageTexelSize.y = RDCMAX(1U, pageTexelDim.y);
  m_PageTexelSize.z = RDCMAX(1U, pageTexelDim.z);
  m_TextureDim.x = RDCMAX(1U, overallTexelDim.x);
  m_TextureDim.y = RDCMAX(1U, overallTexelDim.y);
  m_TextureDim.z = RDCMAX(1U, overallTexelDim.z);

  // initialise the subresources
  m_Subresources.resize(m_ArraySize * m_MipCount);

  // initialise mip tail if we have one
  if(firstTailMip < m_MipCount)
  {
    m_MipTail.byteStride = mipTailStride;
    m_MipTail.byteOffset = mipTailOffset;
    m_MipTail.firstMip = firstTailMip;
    m_MipTail.totalPackedByteSize = mipTailTotalPackedSize;

    if(m_MipTail.byteStride == 0)
    {
      m_MipTail.mappings.resize(1);
      m_MipTail.mappings[0].singlePageReused = false;
      m_MipTail.mappings[0].singleMapping = {};
    }
    else
    {
      m_MipTail.mappings.resize(m_ArraySize);
      for(size_t i = 0; i < m_MipTail.mappings.size(); i++)
      {
        m_MipTail.mappings[i].singlePageReused = false;
        m_MipTail.mappings[i].singleMapping = {};
      }
    }
  }
  else
  {
    m_MipTail.byteStride = 0;
    m_MipTail.byteOffset = 0;
    m_MipTail.firstMip = m_MipCount;
    m_MipTail.totalPackedByteSize = 0;
  }
}

uint64_t PageTable::setMipTailRange(uint64_t resourceByteOffset, ResourceId memory,
                                    uint64_t memoryByteOffset, uint64_t byteSize, bool useSinglePage)
{
  // offsets should be page aligned
  RDCASSERT((memoryByteOffset % m_PageByteSize) == 0, memoryByteOffset, m_PageByteSize);
  RDCASSERT((resourceByteOffset % m_PageByteSize) == 0, resourceByteOffset, m_PageByteSize);

  // size should either be page aligned, or should be the end of the mip tail region (for buffers
  // that don't have to fill the whole thing)
  RDCASSERT((byteSize % m_PageByteSize) == 0 ||
                (resourceByteOffset + byteSize == m_MipTail.totalPackedByteSize),
            resourceByteOffset, byteSize, m_PageByteSize, m_MipTail.totalPackedByteSize);

  RDCASSERT(m_MipTail.totalPackedByteSize > 0);

  // if we're setting to NULL, set the offset to 0
  if(memory == ResourceId())
    memoryByteOffset = 0;

  // rebase the byte offset from resource-relative to miptail-relative
  RDCASSERT(resourceByteOffset >= m_MipTail.byteOffset);
  resourceByteOffset -= m_MipTail.byteOffset;

  if(m_MipTail.mappings.empty())
  {
    RDCERR("Attempting to set mip tail on image with no mip region");
    return m_MipTail.byteOffset + m_MipTail.totalPackedByteSize;
  }

  // if we're setting the whole mip tail at once, store it as a single page mapping
  if(resourceByteOffset == 0 && byteSize == m_MipTail.totalPackedByteSize)
  {
    for(size_t i = 0; i < m_MipTail.mappings.size(); i++)
    {
      m_MipTail.mappings[i].pages.clear();
      m_MipTail.mappings[i].singleMapping.memory = memory;
      m_MipTail.mappings[i].singleMapping.offset = memoryByteOffset;
      m_MipTail.mappings[i].singlePageReused = useSinglePage;

      // if we're not using a single page and we have multiple mip tails separated by a stride,
      // update the memory offset for each single mapping. This implies wasted memory in between so
      // I don't think apps will do this, but it may be legal so handle it here.
      if(!useSinglePage)
        memoryByteOffset += i * m_MipTail.byteStride;
    }

    // we consumed the whole mip tail by definition
    return m_MipTail.byteOffset + m_MipTail.totalPackedByteSize;
  }
  else if(m_MipTail.mappings.size() == 1)
  {
    // if we only have one miptail region, this is simple. Create pages as needed and update the
    // referenced pages
    PageRangeMapping &mapping = m_MipTail.mappings[0];

    mapping.createPages(
        uint32_t((m_MipTail.totalPackedByteSize + m_PageByteSize - 1) / m_PageByteSize),
        m_PageByteSize);

    // iterate through each referenced resource page
    for(size_t
            page = size_t(resourceByteOffset / m_PageByteSize),
            endPage = size_t((resourceByteOffset + byteSize + m_PageByteSize - 1) / m_PageByteSize);
        page < mapping.pages.size() && page < endPage; page++)
    {
      mapping.pages[page].memory = memory;
      mapping.pages[page].offset = memoryByteOffset;

      // if we're not mapping all resource pages to a single memory page, advance the offset
      if(!useSinglePage && memory != ResourceId())
        memoryByteOffset += m_PageByteSize;
    }

    // return how much of the mip tail we consumed, clamped to the size. Note resourceByteOffset has
    // been remapped to be mip-tail relative here
    return m_MipTail.byteOffset +
           RDCMIN(m_MipTail.totalPackedByteSize, resourceByteOffset + byteSize);
  }
  else
  {
    // otherwise the hard case - separate mip tails for each subresource. Figure out which
    // subresource we're starting with
    RDCASSERTNOTEQUAL(m_MipTail.byteStride, 0);
    uint32_t sub = uint32_t(resourceByteOffset / m_MipTail.byteStride);
    resourceByteOffset -= sub * m_MipTail.byteStride;

    const uint64_t mipTailSubresourceByteSize =
        m_MipTail.totalPackedByteSize / m_MipTail.mappings.size();

    // while we have mapping bytes to consume and the subresource is in range
    while(byteSize > 0 && sub < m_MipTail.mappings.size())
    {
      PageRangeMapping &mapping = m_MipTail.mappings[sub];

      uint64_t consumedBytes = 0;

      // if we're setting the whole miptail, store that concisely
      if(resourceByteOffset == 0 && byteSize >= mipTailSubresourceByteSize)
      {
        mapping.pages.clear();
        mapping.singleMapping.memory = memory;
        mapping.singleMapping.offset = memoryByteOffset;
        mapping.singlePageReused = useSinglePage;

        if(!useSinglePage)
          memoryByteOffset += m_MipTail.byteStride;

        consumedBytes = mipTailSubresourceByteSize;
      }
      else
      {
        mapping.createPages(
            uint32_t((mipTailSubresourceByteSize + m_PageByteSize - 1) / m_PageByteSize),
            m_PageByteSize);

        // iterate through each referenced page in this subresource's mip tail. Note we only iterate
        // over as many pages as this mapping has, even if the bound region is larger.
        for(size_t page = size_t(resourceByteOffset / m_PageByteSize),
                   endPage =
                       size_t((resourceByteOffset + byteSize + m_PageByteSize - 1) / m_PageByteSize);
            page < mapping.pages.size() && page < endPage; page++)
        {
          mapping.pages[page].memory = memory;
          mapping.pages[page].offset = memoryByteOffset;

          // if we're not mapping all resource pages to a single memory page, advance the offset
          if(!useSinglePage && memory != ResourceId())
            memoryByteOffset += m_PageByteSize;

          consumedBytes += m_PageByteSize;
        }

        memoryByteOffset += m_MipTail.byteStride - mipTailSubresourceByteSize;
      }

      // if we have fully set this mip tail, move to the next subresource's mip tail.
      // note this covers the case where we set exactly all the bytes in the mip tail, where we set
      // more bytes than exist but don't overlap into the next (based on stride), as well as the
      // case where we have bytes to set in the next subresource too. In the first two cases we will
      // just return, but in the last case we 'consume' the stride and get ready to continue
      if(resourceByteOffset + consumedBytes >= mipTailSubresourceByteSize)
      {
        // we start from the first byte in the next miptail
        resourceByteOffset = 0;

        // advance over the consumed bytes
        byteSize -= consumedBytes;

        // advance over the padding bytes.
        // if we don't have enough remaining to hit the stride, we just zero-out the number of bytes
        // remaining
        byteSize -= RDCMIN(byteSize, m_MipTail.byteStride - mipTailSubresourceByteSize);
        sub++;
      }
      else
      {
        // and consume all bytes, even if that is more than we actually used above (consider if the
        // user specifies more than in the tail, but less than the stride)
        byteSize = 0;

        resourceByteOffset += consumedBytes;
      }
    }

    if(byteSize > 0)
      RDCERR("Unclaimed bytes being assigned to image after iterating over all subresources");

    return m_MipTail.byteOffset + sub * m_MipTail.byteStride + resourceByteOffset;
  }
}

void PageTable::setImageBoxRange(uint32_t subresource, const Sparse::Coord &coord,
                                 const Sparse::Coord &dim, ResourceId memory,
                                 uint64_t memoryByteOffset, bool useSinglePage)
{
  const Coord subresourcePageDim = calcSubresourcePageDim(subresource);

  RDCASSERT((coord.x % m_PageTexelSize.x) == 0);
  RDCASSERT((coord.y % m_PageTexelSize.y) == 0);
  RDCASSERT((coord.z % m_PageTexelSize.z) == 0);

  // dimension may be misaligned if it's referring to part of a page on a non-page-aligned texture
  // dimension
  RDCASSERT((dim.x % m_PageTexelSize.x) == 0 || (coord.x + dim.x == m_TextureDim.x));
  RDCASSERT((dim.y % m_PageTexelSize.y) == 0 || (coord.y + dim.y == m_TextureDim.y));
  RDCASSERT((dim.z % m_PageTexelSize.z) == 0 || (coord.z + dim.z == m_TextureDim.z));

  // convert coords and dim to pages for ease of calculation
  Sparse::Coord curCoord = coord;
  curCoord.x /= m_PageTexelSize.x;
  curCoord.y /= m_PageTexelSize.y;
  curCoord.z /= m_PageTexelSize.z;

  Sparse::Coord curDim = dim;
  curDim.x = RDCMAX(1U, (curDim.x + m_PageTexelSize.x - 1) / m_PageTexelSize.x);
  curDim.y = RDCMAX(1U, (curDim.y + m_PageTexelSize.y - 1) / m_PageTexelSize.y);
  curDim.z = RDCMAX(1U, (curDim.z + m_PageTexelSize.z - 1) / m_PageTexelSize.z);

  RDCASSERT(subresource < m_Subresources.size(), subresource, m_Subresources.size());
  RDCASSERT(curCoord.x < subresourcePageDim.x && curCoord.y < subresourcePageDim.y &&
            curCoord.z < subresourcePageDim.z);
  RDCASSERT(curCoord.x + curDim.x <= subresourcePageDim.x &&
            curCoord.y + curDim.y <= subresourcePageDim.y &&
            curCoord.z + curDim.z <= subresourcePageDim.z);

  // if we're setting to NULL, set the offset to 0
  if(memory == ResourceId())
    memoryByteOffset = 0;

  PageRangeMapping &sub = m_Subresources[subresource];

  // if we're setting the whole subresource, set it to use the optimal single mapping
  if(curCoord.x == 0 && curCoord.y == 0 && curCoord.z == 0 && curDim == subresourcePageDim)
  {
    sub.pages.clear();
    sub.singleMapping.memory = memory;
    sub.singleMapping.offset = memoryByteOffset;
    sub.singlePageReused = useSinglePage;
  }
  else
  {
    // either we're starting at a coord somewhere into the subresource, or we don't cover it all.
    // Split the subresource into pages if needed and update.
    const uint32_t numSubresourcePages =
        subresourcePageDim.x * subresourcePageDim.y * subresourcePageDim.z;
    sub.createPages(numSubresourcePages, m_PageByteSize);

    for(uint32_t z = curCoord.z; z < curCoord.z + curDim.z; z++)
    {
      for(uint32_t y = curCoord.y; y < curCoord.y + curDim.y; y++)
      {
        for(uint32_t x = curCoord.x; x < curCoord.x + curDim.x; x++)
        {
          // calculate the page
          const uint32_t page = (((z * subresourcePageDim.y) + y) * subresourcePageDim.x) + x;

          sub.pages[page].memory = memory;
          sub.pages[page].offset = memoryByteOffset;

          // if we're not mapping all resource pages to a single memory page, advance the offset
          if(!useSinglePage && memory != ResourceId())
            memoryByteOffset += m_PageByteSize;
        }
      }
    }
  }
}

rdcpair<uint32_t, Coord> PageTable::setImageWrappedRange(uint32_t subresource,
                                                         const Sparse::Coord &coord,
                                                         uint64_t byteSize, ResourceId memory,
                                                         uint64_t memoryByteOffset,
                                                         bool useSinglePage)
{
  RDCASSERT((byteSize % m_PageByteSize) == 0, byteSize, m_PageByteSize);

  RDCASSERT(subresource < m_Subresources.size(), subresource, m_Subresources.size());

  Sparse::Coord curCoord = coord;

  // if we're setting to NULL, set the offset to 0
  if(memory == ResourceId())
    memoryByteOffset = 0;

  // loop while we still have bytes left, to allow wrapping over subresources
  while(byteSize > 0 && subresource < m_Subresources.size())
  {
    const Coord subresourcePageDim = calcSubresourcePageDim(subresource);
    const uint32_t numSubresourcePages =
        subresourcePageDim.x * subresourcePageDim.y * subresourcePageDim.z;

    PageRangeMapping &sub = m_Subresources[subresource];

    const uint64_t subresourceByteSize = numSubresourcePages * m_PageByteSize;

    // if we're setting the whole subresource, set it to use the optimal single mapping
    if(curCoord.x == 0 && curCoord.y == 0 && curCoord.z == 0 && byteSize >= subresourceByteSize)
    {
      sub.pages.clear();
      sub.singleMapping.memory = memory;
      sub.singleMapping.offset = memoryByteOffset;
      sub.singlePageReused = useSinglePage;

      // if we're not mapping all resource pages to a single memory page, advance the offset
      if(!useSinglePage && memory != ResourceId())
        memoryByteOffset += numSubresourcePages * m_PageByteSize;

      byteSize -= subresourceByteSize;

      // continue on the next subresource at {0,0,0}. If there are bytes remaining we'll loop and
      // assign them
      subresource++;
      curCoord = {0, 0, 0};

      // since we know we consumed a whole subresource above, if we're done then we can return the
      // correct reference to the next subresource at 0,0,0 here
      if(byteSize == 0)
        return {subresource, curCoord};
    }
    else
    {
      // either we're starting at a coord somewhere into the subresource, or we don't cover it all.
      // Split the subresource into pages if needed and update.
      sub.createPages(numSubresourcePages, m_PageByteSize);

      // note that numPages could be more pages than in the subresource! hence below we check both
      // that we haven't exhausted the incoming pages and that we haven't exhausted the pages in the
      // subresource
      const uint32_t numPages = uint32_t(byteSize / m_PageByteSize);

      // convert current co-ord to pages for calculation. We don't have to worry about doing this
      // repeatedly because if we overlap into another subresource we start at 0,0,0
      curCoord.x /= m_PageTexelSize.x;
      curCoord.y /= m_PageTexelSize.y;
      curCoord.z /= m_PageTexelSize.z;

      // calculate the starting page
      uint32_t startingPage =
          (((curCoord.z * subresourcePageDim.y) + curCoord.y) * subresourcePageDim.x) + curCoord.x;

      for(uint32_t page = startingPage;
          page < startingPage + numPages && page < numSubresourcePages; page++)
      {
        sub.pages[page].memory = memory;
        sub.pages[page].offset = memoryByteOffset;

        // if we're not mapping all resource pages to a single memory page, advance the offset
        if(!useSinglePage && memory != ResourceId())
          memoryByteOffset += m_PageByteSize;
        byteSize -= m_PageByteSize;
      }

      // if we consumed all bytes and didn't get to the end of the subresource, calculate where we
      // ended up
      if(byteSize == 0 && startingPage + numPages < numSubresourcePages)
      {
        // X we just increment by however many pages, wrapping by the row length in pages
        curCoord.x = (curCoord.x + numPages) % subresourcePageDim.x;
        // for Y we increment by however many *rows*, again wrapping
        curCoord.y = (curCoord.y + numPages / subresourcePageDim.x) % subresourcePageDim.y;
        // similarly for Z
        curCoord.z = (curCoord.z + numPages / (subresourcePageDim.x * subresourcePageDim.y)) %
                     subresourcePageDim.z;

        return {subresource, curCoord};
      }

      // continue on the next subresource at {0,0,0}. If there are bytes remaining we'll loop and
      // assign them
      subresource++;
      curCoord = {0, 0, 0};

      // if we got here we consumed the whole subresource and ended, so return that
      if(byteSize == 0)
        return {subresource, curCoord};
    }
  }

  if(byteSize > 0)
    RDCERR("Unclaimed bytes being assigned to image after iterating over all subresources");

  return {0, {0, 0, 0}};
}

Coord PageTable::calcSubresourcePageDim(uint32_t subresource) const
{
  const uint32_t mipLevel = subresource % m_MipCount;

  const Sparse::Coord mipDim = {
      RDCMAX(1U, m_TextureDim.x >> mipLevel), RDCMAX(1U, m_TextureDim.y >> mipLevel),
      RDCMAX(1U, m_TextureDim.z >> mipLevel),
  };

  // for each page that is fully or partially used
  return {RDCMAX(1U, (mipDim.x + m_PageTexelSize.x - 1) / m_PageTexelSize.x),
          RDCMAX(1U, (mipDim.y + m_PageTexelSize.y - 1) / m_PageTexelSize.y),
          RDCMAX(1U, (mipDim.z + m_PageTexelSize.z - 1) / m_PageTexelSize.z)};
}

uint64_t PageTable::GetSerialiseSize() const
{
  uint64_t ret = 0;

  // size of the pair itself
  ret += sizeof(*this);

  // for each mip tail region
  for(uint32_t s = 0; s < getMipTail().mappings.size(); s++)
  {
    ret += sizeof(Sparse::PageRangeMapping);

    // and the size of each page if the range mapping is expanded
    if(!getMipTail().mappings[s].hasSingleMapping())
      ret += sizeof(Sparse::Page) * getMipTail().mappings[s].pages.size();
  }

  // for each subresource the size of it
  for(uint32_t s = 0; s < getNumSubresources(); s++)
  {
    ret += sizeof(Sparse::PageRangeMapping);

    // and the size of each page if the range mapping is expanded
    if(!getSubresource(s).hasSingleMapping())
      ret += sizeof(Sparse::Page) * getSubresource(s).pages.size();
  }

  return ret;
}
};    // namespace Sparse

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Sparse::Coord &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(z);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Sparse::Page &el)
{
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(offset);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Sparse::PageRangeMapping &el)
{
  SERIALISE_MEMBER(singleMapping);
  SERIALISE_MEMBER(pages);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Sparse::MipTail &el)
{
  SERIALISE_MEMBER(firstMip);
  SERIALISE_MEMBER(byteOffset);
  SERIALISE_MEMBER(byteStride);
  SERIALISE_MEMBER(totalPackedByteSize);
  SERIALISE_MEMBER(mappings);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, Sparse::PageTable &el)
{
  SERIALISE_MEMBER(m_TextureDim);
  SERIALISE_MEMBER(m_MipCount);
  SERIALISE_MEMBER(m_ArraySize);
  SERIALISE_MEMBER(m_PageByteSize);
  SERIALISE_MEMBER(m_PageTexelSize);
  SERIALISE_MEMBER(m_Subresources);
  SERIALISE_MEMBER(m_MipTail);
}

INSTANTIATE_SERIALISE_TYPE(Sparse::Coord);
INSTANTIATE_SERIALISE_TYPE(Sparse::Page);
INSTANTIATE_SERIALISE_TYPE(Sparse::PageRangeMapping);
INSTANTIATE_SERIALISE_TYPE(Sparse::MipTail);
INSTANTIATE_SERIALISE_TYPE(Sparse::PageTable);

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

template <>
rdcstr DoStringise(const Sparse::Coord &el)
{
  return StringFormat::Fmt("{%u, %u, %u}", el.x, el.y, el.z);
}

template <>
rdcstr DoStringise(const Sparse::Page &el)
{
  return StringFormat::Fmt("%s:%llu", ToStr(el.memory).c_str(), el.offset);
}

TEST_CASE("Test sparse page table mapping", "[sparse]")
{
  Sparse::PageTable pageTable;

  SECTION("normal buffer")
  {
    pageTable.Initialise(256, 64);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getMipTail().byteOffset == 0);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 256);
    CHECK(pageTable.getMipTail().firstMip == 0);
    REQUIRE(pageTable.getMipTail().mappings.size() == 1);
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({ResourceId(), 0}));

    uint64_t nextTailOffset;

    SECTION("Set all pages")
    {
      ResourceId mem = ResourceIDGen::GetNewUniqueID();

      nextTailOffset = pageTable.setBufferRange(0, mem, 512, 256, false);

      CHECK(nextTailOffset == 256);

      CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
      CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem, 512}));
      CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);
    };

    SECTION("Set repeated page")
    {
      ResourceId mem = ResourceIDGen::GetNewUniqueID();

      nextTailOffset = pageTable.setBufferRange(0, mem, 512, 256, true);

      CHECK(nextTailOffset == 256);

      CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
      CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem, 512}));
      CHECK(pageTable.getMipTail().mappings[0].singlePageReused);
    };

    SECTION("Set page subsets")
    {
      ResourceId mem = ResourceIDGen::GetNewUniqueID();

      nextTailOffset = pageTable.setBufferRange(128, mem, 512, 64, false);

      CHECK(nextTailOffset == 128 + 64);

      CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
      REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 4);
      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem, 512}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({ResourceId(), 0}));

      nextTailOffset = pageTable.setBufferRange(0, mem, 1024, 64, false);

      CHECK(nextTailOffset == 64);

      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mem, 1024}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem, 512}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({ResourceId(), 0}));

      nextTailOffset = pageTable.setBufferRange(64, mem, 128, 128, false);

      CHECK(nextTailOffset == 64 + 128);

      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mem, 1024}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mem, 128}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem, 128 + 64}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({ResourceId(), 0}));

      nextTailOffset = pageTable.setBufferRange(64, mem, 256, 128, true);

      CHECK(nextTailOffset == 64 + 128);

      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mem, 1024}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mem, 256}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem, 256}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({ResourceId(), 0}));

      nextTailOffset = pageTable.setBufferRange(64, ResourceId(), 256, 64, true);

      CHECK(nextTailOffset == 64 + 64);

      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mem, 1024}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem, 256}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({ResourceId(), 0}));
    };
  };

  SECTION("one-page buffer")
  {
    pageTable.Initialise(64, 64);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getMipTail().byteOffset == 0);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 64);
    CHECK(pageTable.getMipTail().firstMip == 0);
    REQUIRE(pageTable.getMipTail().mappings.size() == 1);

    ResourceId mem = ResourceIDGen::GetNewUniqueID();

    uint64_t nextTailOffset;

    nextTailOffset = pageTable.setBufferRange(0, mem, 1024, 64, false);

    CHECK(nextTailOffset == 64);

    CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem, 1024}));

    nextTailOffset = pageTable.setBufferRange(0, ResourceId(), 1024, 64, false);

    CHECK(nextTailOffset == 64);

    CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({ResourceId(), 0}));
  };

  SECTION("non-page-aligned buffer")
  {
    pageTable.Initialise(100, 64);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getMipTail().byteOffset == 0);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 100);
    CHECK(pageTable.getMipTail().firstMip == 0);
    REQUIRE(pageTable.getMipTail().mappings.size() == 1);

    uint64_t nextTailOffset;

    ResourceId mem = ResourceIDGen::GetNewUniqueID();

    nextTailOffset = pageTable.setBufferRange(0, mem, 1024, 100, false);

    CHECK(nextTailOffset == 100);

    CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem, 1024}));

    nextTailOffset = pageTable.setBufferRange(0, ResourceId(), 1024, 64, false);

    CHECK(nextTailOffset == 64);

    CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
    REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 2);
    CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mem, 1024 + 64}));
  };

  SECTION("sub-page sized buffer")
  {
    pageTable.Initialise(10, 64);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getMipTail().byteOffset == 0);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 10);
    CHECK(pageTable.getMipTail().firstMip == 0);
    REQUIRE(pageTable.getMipTail().mappings.size() == 1);

    uint64_t nextTailOffset;

    ResourceId mem = ResourceIDGen::GetNewUniqueID();

    nextTailOffset = pageTable.setBufferRange(0, mem, 1024, 10, false);

    CHECK(nextTailOffset == 10);

    CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem, 1024}));

    nextTailOffset = pageTable.setBufferRange(0, ResourceId(), 1024, 10, false);

    CHECK(nextTailOffset == 10);

    CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({ResourceId(), 0}));
  };

  SECTION("2D texture")
  {
    // create a 256x256 texture with 32x32 pages, 6 mips (the last two are in the mip tail)
    pageTable.Initialise({256, 256, 1}, 6, 1, 64, {32, 32, 1}, 4, 0x10000, 0, 64);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getPageTexelSize() == Sparse::Coord({32, 32, 1}));
    CHECK(pageTable.getMipTail().byteOffset == 0x10000);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 64);
    CHECK(pageTable.getMipTail().firstMip == 4);
    // only expect one mapping because we specified stride of 0, so packed mip tail
    REQUIRE(pageTable.getMipTail().mappings.size() == 1);

    REQUIRE(pageTable.getNumSubresources() == 6);

    CHECK_FALSE(pageTable.isSubresourceInMipTail(0));
    CHECK_FALSE(pageTable.isSubresourceInMipTail(1));
    CHECK_FALSE(pageTable.isSubresourceInMipTail(2));
    CHECK_FALSE(pageTable.isSubresourceInMipTail(3));
    CHECK(pageTable.isSubresourceInMipTail(4));
    CHECK(pageTable.isSubresourceInMipTail(5));

    CHECK_FALSE(pageTable.isByteOffsetInResource(0));
    CHECK_FALSE(pageTable.isByteOffsetInResource(0x1000));
    CHECK(pageTable.isByteOffsetInResource(0x10000));
    CHECK(pageTable.isByteOffsetInResource(0x10000 + 32));
    CHECK(pageTable.isByteOffsetInResource(0x10000 + 63));
    CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000 + 64));

    // they should all be a single mapping to NULL
    CHECK(pageTable.getSubresource(0).hasSingleMapping());
    CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(1).hasSingleMapping());
    CHECK(pageTable.getSubresource(1).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(2).hasSingleMapping());
    CHECK(pageTable.getSubresource(2).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(3).hasSingleMapping());
    CHECK(pageTable.getSubresource(3).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(4).hasSingleMapping());
    CHECK(pageTable.getSubresource(4).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(5).hasSingleMapping());
    CHECK(pageTable.getSubresource(5).singleMapping == Sparse::Page({ResourceId(), 0}));

    ResourceId mip = ResourceIDGen::GetNewUniqueID();

    uint64_t nextTailOffset;

    // this is tested above more robustly as buffers. Here we just check that setting the mip tail
    // offset doesn't break anything
    nextTailOffset = pageTable.setMipTailRange(0x10000, mip, 128, 64, false);

    CHECK(nextTailOffset == 0x10000 + 64);

    CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
    CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mip, 128}));

    SECTION("whole-subresource bindings")
    {
      ResourceId sub0 = ResourceIDGen::GetNewUniqueID();
      ResourceId sub1 = ResourceIDGen::GetNewUniqueID();
      ResourceId sub2 = ResourceIDGen::GetNewUniqueID();
      ResourceId sub3 = ResourceIDGen::GetNewUniqueID();

      pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 256, 1}, sub0, 0, false);

      CHECK(pageTable.getSubresource(0).hasSingleMapping());
      CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({sub0, 0}));
      CHECK_FALSE(pageTable.getSubresource(0).singlePageReused);

      pageTable.setImageBoxRange(1, {0, 0, 0}, {128, 128, 1}, sub1, 128, true);

      CHECK(pageTable.getSubresource(1).hasSingleMapping());
      CHECK(pageTable.getSubresource(1).singleMapping == Sparse::Page({sub1, 128}));
      CHECK(pageTable.getSubresource(1).singlePageReused);

      rdcpair<uint32_t, Sparse::Coord> nextCoord;

      // this mip 2 is 64x64 which is 2x2 tiles, each tiles is 64 bytes
      nextCoord = pageTable.setImageWrappedRange(2, {0, 0, 0}, 2 * 2 * 64, sub2, 256, false);

      CHECK(pageTable.getSubresource(2).hasSingleMapping());
      CHECK(pageTable.getSubresource(2).singleMapping == Sparse::Page({sub2, 256}));
      CHECK_FALSE(pageTable.getSubresource(2).singlePageReused);

      CHECK(nextCoord.first == 3);
      CHECK(nextCoord.second == Sparse::Coord({0, 0, 0}));

      nextCoord = pageTable.setImageWrappedRange(3, {0, 0, 0}, 1 * 1 * 64, sub3, 512, true);

      CHECK(pageTable.getSubresource(3).hasSingleMapping());
      CHECK(pageTable.getSubresource(3).singleMapping == Sparse::Page({sub3, 512}));
      // this is redundant because there's only one page, but let's check it anyway
      CHECK(pageTable.getSubresource(3).singlePageReused);

      CHECK(nextCoord.first == 4);
      CHECK(nextCoord.second == Sparse::Coord({0, 0, 0}));
    };

    SECTION("Partial-subresource bindings")
    {
      ResourceId sub0a = ResourceIDGen::GetNewUniqueID();
      ResourceId sub0b = ResourceIDGen::GetNewUniqueID();
      ResourceId sub0c = ResourceIDGen::GetNewUniqueID();

      // make sure that we detect this as a sub-update even though it starts at 0 and has full width
      pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 192, 1}, sub0a, 0, false);

      CHECK_FALSE(pageTable.getSubresource(0).hasSingleMapping());
      // 8x8 pages in top mip
      REQUIRE(pageTable.getSubresource(0).pages.size() == 64);

#define _idx(x, y) y * 8 + x

      // don't check every one, spot-check
      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({sub0a, 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0a, 128}));

      CHECK(pageTable.getSubresource(0).pages[_idx(1, 2)] == Sparse::Page({sub0a, (2 * 8 + 1) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0a, (2 * 8 + 2) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({ResourceId(), 0}));

      // update only a sub-box
      pageTable.setImageBoxRange(0, {64, 0, 0}, {32, 256, 1}, sub0b, 0, false);

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({sub0a, 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0b, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0b, 128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({sub0b, 6 * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({sub0b, 7 * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({ResourceId(), 0}));

      rdcpair<uint32_t, Sparse::Coord> nextCoord;

      // update a wrapped region
      nextCoord = pageTable.setImageWrappedRange(0, {96, 192, 0}, 8 * 64, sub0c, 640, true);

      CHECK(nextCoord.first == 0);
      CHECK(nextCoord.second == Sparse::Coord({3, 7, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({sub0a, 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0b, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0b, 128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({sub0b, 6 * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({ResourceId(), 0}));

      nextCoord = pageTable.setImageWrappedRange(0, {64, 224, 0}, 11 * 64, sub0c, 6400, false);

      CHECK(nextCoord.first == 1);
      CHECK(nextCoord.second == Sparse::Coord({1, 1, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({sub0a, 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0b, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0b, 128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({sub0b, 6 * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({sub0c, 6400}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({sub0c, 6464}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({sub0c, 6528}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({sub0c, 6720}));

      CHECK_FALSE(pageTable.getSubresource(1).hasSingleMapping());
      // 4x4 pages in second mip
      REQUIRE(pageTable.getSubresource(1).pages.size() == 16);
      CHECK(pageTable.getSubresource(1).pages[0] == Sparse::Page({sub0c, 6784}));

      nextCoord = pageTable.setImageWrappedRange(0, {32, 0, 0}, 64, ResourceId(), 640, false);

      CHECK(nextCoord.first == 0);
      CHECK(nextCoord.second == Sparse::Coord({2, 0, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0b, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0b, 128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({sub0b, 6 * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({sub0c, 6400}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({sub0c, 6464}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({sub0c, 6528}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({sub0c, 6720}));

      pageTable.setImageBoxRange(0, {32, 192, 0}, {64, 64, 1}, ResourceId(), 640, false);

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0b, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0b, 128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({sub0c, 6464}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({sub0c, 6528}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({sub0c, 6720}));

      nextCoord = pageTable.setImageWrappedRange(0, {128, 224, 0}, 64 * 4, sub0a, 512, true);

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({sub0a, 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({sub0b, 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({sub0b, 128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 2)] == Sparse::Page({sub0a, (2 * 8 + 3) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({sub0c, 640}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({sub0c, 6464}));
      CHECK(pageTable.getSubresource(0).pages[_idx(4, 7)] == Sparse::Page({sub0a, 512}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7)] == Sparse::Page({sub0a, 512}));

      CHECK(pageTable.getSubresource(1).pages[0] == Sparse::Page({sub0c, 6784}));

      CHECK(nextCoord.first == 1);
      CHECK(nextCoord.second == Sparse::Coord({0, 0, 0}));
    };
  };

  SECTION("2D rectangular texture")
  {
    pageTable.Initialise({512, 128, 1}, 6, 1, 64, {32, 32, 1}, 4, 0x10000, 0, 64);

    ResourceId mem0 = ResourceIDGen::GetNewUniqueID();
    ResourceId mem1 = ResourceIDGen::GetNewUniqueID();
    ResourceId mem2 = ResourceIDGen::GetNewUniqueID();

    pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 64, 1}, mem0, 0, true);

    CHECK_FALSE(pageTable.getSubresource(0).hasSingleMapping());
    // 16x4 pages in top mip
    REQUIRE(pageTable.getSubresource(0).pages.size() == 64);

#undef _idx
#define _idx(x, y) y * 16 + x

    CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(1, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(3, 1)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 2)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 2)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 2)] == Sparse::Page({ResourceId(), 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 3)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 3)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 3)] == Sparse::Page({ResourceId(), 0}));

    pageTable.setImageBoxRange(0, {256, 64, 0}, {256, 64, 1}, mem1, 0, true);

    CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(1, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(3, 1)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 2)] == Sparse::Page({mem1, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 2)] == Sparse::Page({mem1, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 2)] == Sparse::Page({mem1, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 3)] == Sparse::Page({mem1, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 3)] == Sparse::Page({mem1, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 3)] == Sparse::Page({mem1, 0}));

    rdcpair<uint32_t, Sparse::Coord> nextCoord;

    // update from 11,2 for 17 tiles, which should overlap correctly to 11,3 and no more
    nextCoord = pageTable.setImageWrappedRange(0, {11 * 32, 64, 0}, 64 * 17, mem2, 0, true);

    CHECK(nextCoord.first == 0);
    CHECK(nextCoord.second == Sparse::Coord({12, 3, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(1, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(3, 1)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 2)] == Sparse::Page({mem2, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 2)] == Sparse::Page({mem2, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 2)] == Sparse::Page({mem2, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 3)] == Sparse::Page({mem2, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 3)] == Sparse::Page({mem1, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 3)] == Sparse::Page({mem1, 0}));
  };

  SECTION("2D non-aligned texture")
  {
    pageTable.Initialise({500, 116, 1}, 6, 1, 64, {32, 32, 1}, 4, 0x10000, 0, 64);

    ResourceId mem0 = ResourceIDGen::GetNewUniqueID();
    ResourceId mem1 = ResourceIDGen::GetNewUniqueID();
    ResourceId mem2 = ResourceIDGen::GetNewUniqueID();

#undef _idx
#define _idx(x, y) y * 16 + x

    pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 64, 1}, mem0, 0, true);
    pageTable.setImageBoxRange(0, {256, 64, 0}, {500 - 256, 116 - 64, 1}, mem1, 0, true);
    pageTable.setImageWrappedRange(0, {11 * 32, 64, 0}, 64 * 17, mem2, 0, true);

    // still 16x4 pages in top mip
    REQUIRE(pageTable.getSubresource(0).pages.size() == 64);

    CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(1, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(2, 1)] == Sparse::Page({mem0, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(3, 1)] == Sparse::Page({mem0, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 2)] == Sparse::Page({mem2, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 2)] == Sparse::Page({mem2, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 2)] == Sparse::Page({mem2, 0}));

    CHECK(pageTable.getSubresource(0).pages[_idx(11, 3)] == Sparse::Page({mem2, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(12, 3)] == Sparse::Page({mem1, 0}));
    CHECK(pageTable.getSubresource(0).pages[_idx(13, 3)] == Sparse::Page({mem1, 0}));
  };

  SECTION("2D texture that's all mip tail")
  {
    // create a 256x256 texture with 32x32 pages, 6 mips (the last two are in the mip tail)
    pageTable.Initialise({256, 256, 1}, 6, 1, 64, {32, 32, 1}, 0, 0, 0, 8192);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getPageTexelSize() == Sparse::Coord({32, 32, 1}));
    CHECK(pageTable.getMipTail().byteOffset == 0);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 8192);
    CHECK(pageTable.getMipTail().firstMip == 0);
    REQUIRE(pageTable.getMipTail().mappings.size() == 1);

    REQUIRE(pageTable.getNumSubresources() == 6);

    CHECK(pageTable.isSubresourceInMipTail(0));
    CHECK(pageTable.isSubresourceInMipTail(1));
    CHECK(pageTable.isSubresourceInMipTail(2));
    CHECK(pageTable.isSubresourceInMipTail(3));
    CHECK(pageTable.isSubresourceInMipTail(4));
    CHECK(pageTable.isSubresourceInMipTail(5));

    ResourceId mip = ResourceIDGen::GetNewUniqueID();

    uint64_t nextTailOffset;

    // this is tested above more robustly as buffers. Here we just check that setting the mip tail
    // offset doesn't break anything
    nextTailOffset = pageTable.setMipTailRange(0, mip, 512, 256, false);

    CHECK(nextTailOffset == 256);

    CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
    REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 8192 / 64);
    CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mip, 512}));
    CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mip, 576}));
    CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mip, 640}));
    CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({mip, 704}));
    CHECK(pageTable.getMipTail().mappings[0].pages[4] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getMipTail().mappings[0].pages[5] == Sparse::Page({ResourceId(), 0}));
  };

  SECTION("3D texture tests")
  {
    // create a 256x256x64 texture with 32x32x4 pages, 6 mips (the last two are in the mip tail)
    pageTable.Initialise({256, 256, 64}, 6, 1, 64, {32, 32, 4}, 4, 0x10000, 0, 64);

    CHECK(pageTable.getPageByteSize() == 64);
    CHECK(pageTable.getPageTexelSize() == Sparse::Coord({32, 32, 4}));
    CHECK(pageTable.getMipTail().byteOffset == 0x10000);
    CHECK(pageTable.getMipTail().byteStride == 0);
    CHECK(pageTable.getMipTail().totalPackedByteSize == 64);
    CHECK(pageTable.getMipTail().firstMip == 4);

    REQUIRE(pageTable.getNumSubresources() == 6);

    CHECK_FALSE(pageTable.isSubresourceInMipTail(0));
    CHECK_FALSE(pageTable.isSubresourceInMipTail(1));
    CHECK_FALSE(pageTable.isSubresourceInMipTail(2));
    CHECK_FALSE(pageTable.isSubresourceInMipTail(3));
    CHECK(pageTable.isSubresourceInMipTail(4));
    CHECK(pageTable.isSubresourceInMipTail(5));

    // they should all be a single mapping to NULL
    CHECK(pageTable.getSubresource(0).hasSingleMapping());
    CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(1).hasSingleMapping());
    CHECK(pageTable.getSubresource(1).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(2).hasSingleMapping());
    CHECK(pageTable.getSubresource(2).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(3).hasSingleMapping());
    CHECK(pageTable.getSubresource(3).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(4).hasSingleMapping());
    CHECK(pageTable.getSubresource(4).singleMapping == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(5).hasSingleMapping());
    CHECK(pageTable.getSubresource(5).singleMapping == Sparse::Page({ResourceId(), 0}));

    SECTION("whole-subresource bindings")
    {
      ResourceId sub0 = ResourceIDGen::GetNewUniqueID();
      ResourceId sub1 = ResourceIDGen::GetNewUniqueID();

      pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 256, 64}, sub0, 0, false);

      CHECK(pageTable.getSubresource(0).hasSingleMapping());
      CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({sub0, 0}));
      CHECK_FALSE(pageTable.getSubresource(0).singlePageReused);

      pageTable.setImageBoxRange(1, {0, 0, 0}, {128, 128, 32}, sub1, 128, true);

      CHECK(pageTable.getSubresource(1).hasSingleMapping());
      CHECK(pageTable.getSubresource(1).singleMapping == Sparse::Page({sub1, 128}));
      CHECK(pageTable.getSubresource(1).singlePageReused);
    };

    SECTION("Partial-subresource bindings")
    {
      ResourceId sub0a = ResourceIDGen::GetNewUniqueID();
      ResourceId sub0b = ResourceIDGen::GetNewUniqueID();
      ResourceId sub0c = ResourceIDGen::GetNewUniqueID();

      // make sure that we detect this as a sub-update even though it covers full width/height
      pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 256, 16}, sub0a, 0, false);

      CHECK_FALSE(pageTable.getSubresource(0).hasSingleMapping());
      // 8x8x16 pages in top mip
      REQUIRE(pageTable.getSubresource(0).pages.size() == 8 * 8 * 16);

#undef _idx
#define _idx(x, y, z) ((z * 8 + y) * 8 + x)

      // don't check every one, spot-check
      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0, 0)] ==
            Sparse::Page({sub0a, _idx(0, 0, 0) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 4, 0)] ==
            Sparse::Page({sub0a, _idx(3, 4, 0) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7, 0)] ==
            Sparse::Page({sub0a, _idx(7, 7, 0) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0, 1)] ==
            Sparse::Page({sub0a, _idx(0, 0, 1) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 4, 1)] ==
            Sparse::Page({sub0a, _idx(3, 4, 1) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7, 1)] ==
            Sparse::Page({sub0a, _idx(7, 7, 1) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0, 10)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 4, 10)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7, 10)] == Sparse::Page({ResourceId(), 0}));

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0, 11)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 4, 11)] == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(0).pages[_idx(7, 7, 11)] == Sparse::Page({ResourceId(), 0}));
    };
  };

  SECTION("2D texture array tests")
  {
    ResourceId mip0 = ResourceIDGen::GetNewUniqueID();
    ResourceId mip1 = ResourceIDGen::GetNewUniqueID();
    ResourceId mip2 = ResourceIDGen::GetNewUniqueID();

    // create a 256x256 texture with 32x32 pages, 6 mips (the last two are in the mip tail), and 5
    // array slices. The mip tail in this case we make two pages to better show the effect of the
    // mip tail
    SECTION("separate mip tail")
    {
      // in the event that we have separate mip tails the stride may be huge as otherwise it's just
      // a single mip tail storage. In this event we don't want to overallocate and waste pages
      pageTable.Initialise({256, 256, 1}, 6, 5, 64, {32, 32, 1}, 4, 0x10000, 32768, 128 * 5);

      SECTION("property accessors")
      {
        CHECK(pageTable.getMipCount() == 6);
        CHECK(pageTable.getArraySize() == 5);

        CHECK(pageTable.calcSubresource(0, 0) == 0);
        CHECK(pageTable.calcSubresource(0, 1) == 1);
        CHECK(pageTable.calcSubresource(0, 2) == 2);
        CHECK(pageTable.calcSubresource(0, 3) == 3);
        CHECK(pageTable.calcSubresource(0, 4) == 4);
        CHECK(pageTable.calcSubresource(0, 5) == 5);
        CHECK(pageTable.calcSubresource(1, 0) == 6);
        CHECK(pageTable.calcSubresource(2, 2) == 14);
        CHECK(pageTable.calcSubresource(4, 5) == 29);

        // 64 bytes per page, 8x8 pages in top mip
        CHECK(pageTable.getSubresourceByteSize(0) == 64 * 8 * 8);
        CHECK(pageTable.getSubresourceByteSize(6) == 64 * 8 * 8);
        CHECK(pageTable.getSubresourceByteSize(12) == 64 * 8 * 8);
        CHECK(pageTable.getSubresourceByteSize(1) == 64 * 4 * 4);
        CHECK(pageTable.getSubresourceByteSize(2) == 64 * 2 * 2);
        CHECK(pageTable.getSubresourceByteSize(3) == 64 * 1 * 1);
      }

      CHECK(pageTable.getPageByteSize() == 64);
      CHECK(pageTable.getPageTexelSize() == Sparse::Coord({32, 32, 1}));
      CHECK(pageTable.getMipTail().byteOffset == 0x10000);
      CHECK(pageTable.getMipTail().byteStride == 32768);
      CHECK(pageTable.getMipTail().totalPackedByteSize == 128 * 5);
      CHECK(pageTable.getMipTail().firstMip == 4);
      REQUIRE(pageTable.getMipTail().mappings.size() == 5);

      REQUIRE(pageTable.getNumSubresources() == 6 * 5);

      CHECK_FALSE(pageTable.isByteOffsetInResource(0));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x1000));
      CHECK(pageTable.isByteOffsetInResource(0x10000));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 32));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 1280));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 32768));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 128000));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 32768 * 5 - 1));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000 + 32768 * 5));

      // all mips in the same array slice should have the same miptail offset
      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) ==
            pageTable.getMipTailByteOffsetForSubresource(1));
      CHECK(pageTable.getMipTailByteOffsetForSubresource(6) ==
            pageTable.getMipTailByteOffsetForSubresource(8));
      CHECK(pageTable.getMipTailByteOffsetForSubresource(18) ==
            pageTable.getMipTailByteOffsetForSubresource(20));

      // but mips in different slices should have a different one
      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) !=
            pageTable.getMipTailByteOffsetForSubresource(6));
      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) !=
            pageTable.getMipTailByteOffsetForSubresource(20));

      // the calculated offset should be relative to the stride, not relative to the packing
      CHECK(pageTable.getMipTailByteOffsetForSubresource(6) == 0x10000 + 32768);

      uint64_t nextTailOffset;

      SECTION("separate whole-mip sets")
      {
        nextTailOffset = pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(0),
                                                   mip0, 0, 128, false);

        CHECK(nextTailOffset == pageTable.getMipTailByteOffsetForSubresource(6));

        nextTailOffset = pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(6),
                                                   mip1, 640, 128, true);

        CHECK(nextTailOffset == pageTable.getMipTailByteOffsetForSubresource(12));

        nextTailOffset = pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(18),
                                                   mip2, 6400, 128, false);

        CHECK(nextTailOffset == pageTable.getMipTailByteOffsetForSubresource(24));

        // each of these sets should have been detected as a single page mapping
        CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mip0, 0}));
        CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[1].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[1].singleMapping == Sparse::Page({mip1, 640}));
        CHECK(pageTable.getMipTail().mappings[1].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[2].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[2].singleMapping == Sparse::Page({ResourceId(), 0}));
        CHECK_FALSE(pageTable.getMipTail().mappings[2].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[3].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[3].singleMapping == Sparse::Page({mip2, 6400}));
        CHECK_FALSE(pageTable.getMipTail().mappings[3].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[4].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[4].singleMapping == Sparse::Page({ResourceId(), 0}));
        CHECK_FALSE(pageTable.getMipTail().mappings[4].singlePageReused);
      };

      SECTION("single set large enough for all mips")
      {
        nextTailOffset = pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(0),
                                                   mip0, 0, 32768 * 4 + 128, false);

        CHECK(nextTailOffset >= pageTable.getMipTailByteOffsetForSubresource(29) + 128);

        CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mip0, 32768 * 0}));
        CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[1].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[1].singleMapping == Sparse::Page({mip0, 32768 * 1}));
        CHECK_FALSE(pageTable.getMipTail().mappings[1].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[2].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[2].singleMapping == Sparse::Page({mip0, 32768 * 2}));
        CHECK_FALSE(pageTable.getMipTail().mappings[2].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[3].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[3].singleMapping == Sparse::Page({mip0, 32768 * 3}));
        CHECK_FALSE(pageTable.getMipTail().mappings[3].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[4].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[4].singleMapping == Sparse::Page({mip0, 32768 * 4}));
        CHECK_FALSE(pageTable.getMipTail().mappings[4].singlePageReused);
      };

      SECTION("Partial and overlapping memory sets")
      {
        nextTailOffset = pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(0),
                                                   mip0, 0, 64, false);

        CHECK(nextTailOffset == pageTable.getMipTailByteOffsetForSubresource(0) + 64);

        nextTailOffset = pageTable.setMipTailRange(
            pageTable.getMipTailByteOffsetForSubresource(6) + 64, mip1, 256, 64, false);

        CHECK(nextTailOffset == pageTable.getMipTailByteOffsetForSubresource(12));

        CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
        REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 2);
        CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mip0, 0}));
        CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({ResourceId(), 0}));

        CHECK_FALSE(pageTable.getMipTail().mappings[1].hasSingleMapping());
        REQUIRE(pageTable.getMipTail().mappings[1].pages.size() == 2);
        CHECK(pageTable.getMipTail().mappings[1].pages[0] == Sparse::Page({ResourceId(), 0}));
        CHECK(pageTable.getMipTail().mappings[1].pages[1] == Sparse::Page({mip1, 256}));

        // this set is dubiously legal in client APIs but we ensure it works. We set part of one mip
        // tail, then the whole stride (which overwrites the real non-tail subresources?) then part
        // of the mip tail of the next
        // we set 64 bytes in one, 'set' (skip) the padding bytes (stride - miptail size) then 64
        // more bytes
        nextTailOffset =
            pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(0) + 64, mip2,
                                      64, 64 + (32768 - 128) + 64, false);

        CHECK(nextTailOffset == pageTable.getMipTailByteOffsetForSubresource(6) + 64);

        CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
        REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 2);
        CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mip0, 0}));
        CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mip2, 64}));

        CHECK_FALSE(pageTable.getMipTail().mappings[1].hasSingleMapping());
        REQUIRE(pageTable.getMipTail().mappings[1].pages.size() == 2);
        CHECK(pageTable.getMipTail().mappings[1].pages[0] == Sparse::Page({mip2, 32768}));
        CHECK(pageTable.getMipTail().mappings[1].pages[1] == Sparse::Page({mip1, 256}));

        nextTailOffset =
            pageTable.setMipTailRange(pageTable.getMipTailByteOffsetForSubresource(18) + 64, mip2,
                                      0, 64 + (32768 - 128) + 128, false);

        CHECK(nextTailOffset >= pageTable.getMipTailByteOffsetForSubresource(29) + 128);

        CHECK_FALSE(pageTable.getMipTail().mappings[3].hasSingleMapping());
        REQUIRE(pageTable.getMipTail().mappings[3].pages.size() == 2);
        CHECK(pageTable.getMipTail().mappings[3].pages[0] == Sparse::Page({ResourceId(), 0}));
        CHECK(pageTable.getMipTail().mappings[3].pages[1] == Sparse::Page({mip2, 0}));

        CHECK(pageTable.getMipTail().mappings[4].hasSingleMapping());
        CHECK_FALSE(pageTable.getMipTail().mappings[4].singlePageReused);
        CHECK(pageTable.getMipTail().mappings[4].singleMapping ==
              Sparse::Page({mip2, 64 + (32768 - 128)}));
      };
    };

    SECTION("combined mip tail")
    {
      pageTable.Initialise({256, 256, 1}, 6, 5, 64, {32, 32, 1}, 4, 0x10000, 0, 128 * 5);

      CHECK(pageTable.getPageByteSize() == 64);
      CHECK(pageTable.getPageTexelSize() == Sparse::Coord({32, 32, 1}));
      CHECK(pageTable.getMipTail().byteOffset == 0x10000);
      CHECK(pageTable.getMipTail().byteStride == 0);
      CHECK(pageTable.getMipTail().totalPackedByteSize == 128 * 5);
      CHECK(pageTable.getMipTail().firstMip == 4);
      REQUIRE(pageTable.getMipTail().mappings.size() == 1);

      REQUIRE(pageTable.getNumSubresources() == 6 * 5);

      CHECK_FALSE(pageTable.isByteOffsetInResource(0));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x1000));
      CHECK(pageTable.isByteOffsetInResource(0x10000));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 32));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 128));
      CHECK(pageTable.isByteOffsetInResource(0x10000 + 128 * 5 - 1));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000 + 128 * 5));

      // all mips in all array slices should have the same miptail offset we specified
      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) == 0x10000);
      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) ==
            pageTable.getMipTailByteOffsetForSubresource(1));

      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) ==
            pageTable.getMipTailByteOffsetForSubresource(6));

      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) ==
            pageTable.getMipTailByteOffsetForSubresource(8));

      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) ==
            pageTable.getMipTailByteOffsetForSubresource(16));

      CHECK(pageTable.getMipTailByteOffsetForSubresource(0) ==
            pageTable.getMipTailByteOffsetForSubresource(20));

      uint64_t nextTailOffset;

      SECTION("whole-tail set")
      {
        nextTailOffset = pageTable.setMipTailRange(0x10000, mip0, 0, 128 * 5, false);

        CHECK(nextTailOffset == 0x10000 + 128 * 5);
        CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
        CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mip0, 0}));
        CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);
      };

      SECTION("separate mip sets")
      {
        // we don't use getMipTailByteOffset.. to calculate the offset because the mip tail is a
        // single one for all subresources
        nextTailOffset = pageTable.setMipTailRange(0x10000, mip0, 0, 128, false);

        CHECK(nextTailOffset == 0x10000 + 128);

        nextTailOffset = pageTable.setMipTailRange(0x10000 + 128, mip1, 640, 128, false);

        CHECK(nextTailOffset == 0x10000 + 256);

        nextTailOffset = pageTable.setMipTailRange(0x10000 + 384, mip2, 6400, 128, false);

        CHECK(nextTailOffset == 0x10000 + 512);

        // we should only allocate the minimum number of pages - total size divided by page size
        CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
        REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 5 * 2);

        CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mip0, 0}));
        CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mip0, 64}));
        CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mip1, 640}));
        CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({mip1, 704}));
        CHECK(pageTable.getMipTail().mappings[0].pages[4] == Sparse::Page({ResourceId(), 0}));
        CHECK(pageTable.getMipTail().mappings[0].pages[5] == Sparse::Page({ResourceId(), 0}));
        CHECK(pageTable.getMipTail().mappings[0].pages[6] == Sparse::Page({mip2, 6400}));
        CHECK(pageTable.getMipTail().mappings[0].pages[7] == Sparse::Page({mip2, 6464}));
      };
    };

    SECTION("no mip tail")
    {
      pageTable.Initialise({256, 256, 1}, 6, 5, 64, {32, 32, 1}, 8, 0x10000, 0, 128 * 5);

      CHECK(pageTable.getPageByteSize() == 64);
      CHECK(pageTable.getPageTexelSize() == Sparse::Coord({32, 32, 1}));
      CHECK(pageTable.getMipTail().byteOffset == 0);
      CHECK(pageTable.getMipTail().byteStride == 0);
      CHECK(pageTable.getMipTail().totalPackedByteSize == 0);
      CHECK(pageTable.getMipTail().firstMip == 6);

      REQUIRE(pageTable.getNumSubresources() == 6 * 5);

      CHECK_FALSE(pageTable.isByteOffsetInResource(0));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x1000));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000 + 32));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000 + 63));
      CHECK_FALSE(pageTable.isByteOffsetInResource(0x10000 + 64));
    };

    if(pageTable.getMipTail().totalPackedByteSize > 0)
    {
      for(uint32_t slice = 0; slice < 5; slice++)
      {
        for(uint32_t mip = 0; mip < 6; mip++)
        {
          uint32_t sub = slice * 6 + mip;
          if(mip < 4)
          {
            CHECK_FALSE(pageTable.isSubresourceInMipTail(sub));
          }
          else
          {
            CHECK(pageTable.isSubresourceInMipTail(sub));
          }
        }
      }
    }

    ResourceId sub0 = ResourceIDGen::GetNewUniqueID();
    ResourceId sub1_2 = ResourceIDGen::GetNewUniqueID();
    ResourceId sub7 = ResourceIDGen::GetNewUniqueID();
    ResourceId sub8 = ResourceIDGen::GetNewUniqueID();
    ResourceId sub18_19_20 = ResourceIDGen::GetNewUniqueID();

    pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 256, 1}, sub0, 0, false);

    CHECK(pageTable.getSubresource(0).hasSingleMapping());
    CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({sub0, 0}));
    CHECK_FALSE(pageTable.getSubresource(0).singlePageReused);

    rdcpair<uint32_t, Sparse::Coord> nextCoord;

    // this will set all of subresource 1 (4x4 tiles), wrap into subresource 2 (2x2 tiles) and set
    // all of that
    nextCoord = pageTable.setImageWrappedRange(1, {0, 0, 0}, (16 + 4) * 64, sub1_2, 0x200000, false);

    CHECK(nextCoord.first == 3);
    CHECK(nextCoord.second == Sparse::Coord({0, 0, 0}));

    CHECK(pageTable.getSubresource(1).hasSingleMapping());
    CHECK(pageTable.getSubresource(1).singleMapping == Sparse::Page({sub1_2, 0x200000}));
    CHECK_FALSE(pageTable.getSubresource(1).singlePageReused);

    CHECK(pageTable.getSubresource(2).hasSingleMapping());
    CHECK(pageTable.getSubresource(2).singleMapping == Sparse::Page({sub1_2, 0x200000 + 16 * 64}));
    CHECK_FALSE(pageTable.getSubresource(2).singlePageReused);

    CHECK(pageTable.getSubresource(3).hasSingleMapping());
    CHECK(pageTable.getSubresource(3).singleMapping == Sparse::Page({ResourceId(), 0}));

    pageTable.setImageBoxRange(7, {0, 0, 0}, {128, 128, 1}, sub7, 128, true);

    CHECK(pageTable.getSubresource(7).hasSingleMapping());
    CHECK(pageTable.getSubresource(7).singleMapping == Sparse::Page({sub7, 128}));
    CHECK(pageTable.getSubresource(7).singlePageReused);

#undef _idx
#define _idx(x, y) y * 2 + x

    pageTable.setImageBoxRange(8, {32, 0, 0}, {32, 64, 1}, sub8, 12800, false);

    CHECK(pageTable.getSubresource(8).pages[_idx(0, 0)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(8).pages[_idx(1, 0)] == Sparse::Page({sub8, 12800}));

    CHECK(pageTable.getSubresource(8).pages[_idx(0, 1)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(8).pages[_idx(1, 1)] == Sparse::Page({sub8, 12864}));

    // this sets some of subresource 18 (8x8 tiles), all of subresource 19 (4x4 tiles) and some of
    // 20 (2x2 tiles)
    nextCoord =
        pageTable.setImageWrappedRange(18, {128, 128, 0}, (28 + 16 + 1) * 64, sub18_19_20, 0, false);

    CHECK(nextCoord.first == 20);
    CHECK(nextCoord.second == Sparse::Coord({1, 0, 0}));

#undef _idx
#define _idx(x, y) y * 8 + x

    CHECK(pageTable.getSubresource(18).pages[_idx(0, 0)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(18).pages[_idx(3, 3)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(18).pages[_idx(4, 3)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(18).pages[_idx(5, 3)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(18).pages[_idx(3, 4)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(18).pages[_idx(4, 4)] == Sparse::Page({sub18_19_20, 0 * 64}));
    CHECK(pageTable.getSubresource(18).pages[_idx(5, 4)] == Sparse::Page({sub18_19_20, 1 * 64}));
    CHECK(pageTable.getSubresource(18).pages[_idx(7, 7)] == Sparse::Page({sub18_19_20, 27 * 64}));

    CHECK(pageTable.getSubresource(19).hasSingleMapping());
    CHECK(pageTable.getSubresource(19).singleMapping == Sparse::Page({sub18_19_20, 28 * 64}));
    CHECK_FALSE(pageTable.getSubresource(19).singlePageReused);

#undef _idx
#define _idx(x, y) y * 2 + x

    CHECK(pageTable.getSubresource(20).pages[_idx(0, 0)] ==
          Sparse::Page({sub18_19_20, (28 + 16) * 64}));
    CHECK(pageTable.getSubresource(20).pages[_idx(1, 0)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(20).pages[_idx(0, 1)] == Sparse::Page({ResourceId(), 0}));
    CHECK(pageTable.getSubresource(20).pages[_idx(1, 1)] == Sparse::Page({ResourceId(), 0}));
  };

  SECTION("Updates from whole-subresource to split pages")
  {
    ResourceId mem0 = ResourceIDGen::GetNewUniqueID();
    ResourceId mem1 = ResourceIDGen::GetNewUniqueID();
    ResourceId mem2 = ResourceIDGen::GetNewUniqueID();

    SECTION("Buffers/mip-tail")
    {
      pageTable.Initialise(320, 64);

      CHECK(pageTable.getPageByteSize() == 64);
      CHECK(pageTable.getMipTail().byteOffset == 0);
      CHECK(pageTable.getMipTail().byteStride == 0);
      CHECK(pageTable.getMipTail().totalPackedByteSize == 320);
      CHECK(pageTable.getMipTail().firstMip == 0);
      REQUIRE(pageTable.getMipTail().mappings.size() == 1);

      uint64_t nextTailOffset;

      nextTailOffset = pageTable.setBufferRange(0, mem0, 0, 320, false);

      CHECK(nextTailOffset == 320);

      CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
      CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem0, 0}));
      CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);

      nextTailOffset = pageTable.setBufferRange(128, mem1, 0, 64, false);

      CHECK(nextTailOffset == 128 + 64);

      CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
      REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 5);
      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mem0, 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mem0, 64}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem1, 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({mem0, 192}));
      CHECK(pageTable.getMipTail().mappings[0].pages[4] == Sparse::Page({mem0, 256}));

      nextTailOffset = pageTable.setBufferRange(0, mem2, 1024, 64, false);

      CHECK(nextTailOffset == 0 + 64);

      CHECK_FALSE(pageTable.getMipTail().mappings[0].hasSingleMapping());
      REQUIRE(pageTable.getMipTail().mappings[0].pages.size() == 5);
      CHECK(pageTable.getMipTail().mappings[0].pages[0] == Sparse::Page({mem2, 1024}));
      CHECK(pageTable.getMipTail().mappings[0].pages[1] == Sparse::Page({mem0, 64}));
      CHECK(pageTable.getMipTail().mappings[0].pages[2] == Sparse::Page({mem1, 0}));
      CHECK(pageTable.getMipTail().mappings[0].pages[3] == Sparse::Page({mem0, 192}));
      CHECK(pageTable.getMipTail().mappings[0].pages[4] == Sparse::Page({mem0, 256}));

      nextTailOffset = pageTable.setBufferRange(0, mem2, 0, 320, false);

      CHECK(nextTailOffset == 0 + 320);

      CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
      CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem2, 0}));
      CHECK_FALSE(pageTable.getMipTail().mappings[0].singlePageReused);

      nextTailOffset = pageTable.setBufferRange(0, mem1, 0, 320, true);

      CHECK(nextTailOffset == 0 + 320);

      CHECK(pageTable.getMipTail().mappings[0].hasSingleMapping());
      CHECK(pageTable.getMipTail().mappings[0].singleMapping == Sparse::Page({mem1, 0}));
      CHECK(pageTable.getMipTail().mappings[0].singlePageReused);
    };

    SECTION("2D texture")
    {
      // create a 256x256 texture with 32x32 pages, 6 mips (the last two are in the mip tail)
      pageTable.Initialise({256, 256, 1}, 6, 1, 64, {32, 32, 1}, 4, 0x10000, 0, 64);

      // they should all be a single mapping to NULL
      CHECK(pageTable.getSubresource(0).hasSingleMapping());
      CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(1).hasSingleMapping());
      CHECK(pageTable.getSubresource(1).singleMapping == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(2).hasSingleMapping());
      CHECK(pageTable.getSubresource(2).singleMapping == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(3).hasSingleMapping());
      CHECK(pageTable.getSubresource(3).singleMapping == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(4).hasSingleMapping());
      CHECK(pageTable.getSubresource(4).singleMapping == Sparse::Page({ResourceId(), 0}));
      CHECK(pageTable.getSubresource(5).hasSingleMapping());
      CHECK(pageTable.getSubresource(5).singleMapping == Sparse::Page({ResourceId(), 0}));

      pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 256, 1}, mem0, 0, false);

      CHECK(pageTable.getSubresource(0).hasSingleMapping());
      CHECK(pageTable.getSubresource(0).singleMapping == Sparse::Page({mem0, 0}));
      CHECK_FALSE(pageTable.getSubresource(0).singlePageReused);

      pageTable.setImageBoxRange(0, {32, 32, 0}, {64, 64, 1}, mem1, 10240, true);

#undef _idx
#define _idx(x, y) (y * 8 + x)

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({mem0, _idx(0, 0) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({mem0, _idx(1, 0) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({mem0, _idx(2, 0) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(1, 1)] == Sparse::Page({mem1, 10240}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 1)] == Sparse::Page({mem1, 10240}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 2)] == Sparse::Page({mem1, 10240}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({mem1, 10240}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({mem0, _idx(2, 6) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({mem0, _idx(3, 6) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({mem0, _idx(1, 7) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({mem0, _idx(2, 7) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({mem0, _idx(3, 7) * 64}));

      pageTable.setImageBoxRange(0, {0, 0, 0}, {256, 256, 1}, mem0, 0, false);
      pageTable.setImageBoxRange(0, {32, 32, 0}, {64, 64, 1}, mem1, 1024000, false);

      CHECK(pageTable.getSubresource(0).pages[_idx(0, 0)] == Sparse::Page({mem0, _idx(0, 0) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 0)] == Sparse::Page({mem0, _idx(1, 0) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 0)] == Sparse::Page({mem0, _idx(2, 0) * 64}));

      CHECK(pageTable.getSubresource(0).pages[_idx(1, 1)] == Sparse::Page({mem1, 1024000}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 1)] == Sparse::Page({mem1, 1024064}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 2)] == Sparse::Page({mem1, 1024128}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 2)] == Sparse::Page({mem1, 1024192}));

      CHECK(pageTable.getSubresource(0).pages[_idx(2, 6)] == Sparse::Page({mem0, _idx(2, 6) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 6)] == Sparse::Page({mem0, _idx(3, 6) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(1, 7)] == Sparse::Page({mem0, _idx(1, 7) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(2, 7)] == Sparse::Page({mem0, _idx(2, 7) * 64}));
      CHECK(pageTable.getSubresource(0).pages[_idx(3, 7)] == Sparse::Page({mem0, _idx(3, 7) * 64}));
    };
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
