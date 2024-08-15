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

#include "dxil_metadata.h"

#include "driver/shaders/dxbc/dxbc_container.h"
#include "serialise/streamio.h"

// serialise/encode helpers
namespace
{

struct RuntimePartHeader
{
  DXIL::RDATData::Part part;
  uint32_t size;
};

struct RuntimePartTableHeader
{
  uint32_t count;
  uint32_t stride;
};

// slightly type-safer way of returning an index/offset encoded as a uint
struct IndexReference
{
  uint32_t offset;
};

struct BytesReference
{
  uint32_t offset;
  uint32_t size;
};

static IndexReference MakeStringRef(rdcstr &stringblob, const rdcstr &str)
{
  // not efficient, we don't cache anything but do a straight linear search.
  uint32_t offs = 0;

  while(offs < stringblob.length())
  {
    const char *curstr = stringblob.c_str() + offs;
    uint32_t curlen = (uint32_t)strlen(curstr);

    if(curlen == str.length() && strcmp(curstr, str.c_str()) == 0)
      return {offs};

    // skip past the NULL terminator to the start of the next string
    offs += curlen + 1;
  }

  uint32_t ret = (uint32_t)stringblob.length();
  stringblob.append(str);
  // we need to explicitly include the NULL terminators
  stringblob.push_back('\0');
  return {ret};
}

static BytesReference MakeBytesRef(rdcarray<bytebuf> &bytesblobs, const bytebuf &bytes)
{
  // ~0U indicates empty bytes
  if(bytes.empty())
    return {~0U};

  // super inefficient but we don't expect there to be many bytes blobs (only root signatures)
  int32_t idx = bytesblobs.indexOf(bytes);
  if(idx < 0)
  {
    size_t offs = 0;
    for(size_t i = 0; i < bytesblobs.size(); i++)
      offs += bytesblobs[i].size();

    bytesblobs.push_back(bytes);
    return {(uint32_t)offs, (uint32_t)bytes.size()};
  }

  size_t offs = 0;
  for(int32_t i = 0; i < idx; i++)
    offs += bytesblobs[i].size();
  return {(uint32_t)offs, (uint32_t)bytes.size()};
}

static IndexReference MakeIndexArrayRef(rdcarray<uint32_t> &idxArrays,
                                        const rdcarray<uint32_t> &idxs, bool emptyIsNull)
{
  // ~0U indicates NULL, in some cases replaces an empty array
  if(emptyIsNull && idxs.empty())
    return {~0U};

  // not efficient, we don't cache anything but do a straight linear search.
  uint32_t offs = 0;

  while(offs < idxArrays.size())
  {
    const uint32_t *curarray = idxArrays.data() + offs;
    // length-prefix on array
    uint32_t curlen = *curarray;
    curarray++;

    if(curlen == idxs.size())
    {
      bool allsame = true;
      for(uint32_t i = 0; i < curlen && allsame; i++)
        allsame &= idxs[i] == curarray[i];

      if(allsame)
        return {offs};
    }

    // skip past the length and the current array
    offs += 1 + curlen;
  }

  uint32_t ret = (uint32_t)idxArrays.size();
  // idx arrays are length prefixed
  idxArrays.push_back((uint32_t)idxs.size());
  idxArrays.append(idxs);
  return {ret};
}

// serialised equivalent to RDAT::ResourceInfo
struct EncodedResourceInfo
{
  DXIL::ResourceClass nspace;
  DXIL::ResourceKind kind;
  uint32_t LinearID;
  uint32_t space;
  uint32_t regStart;
  uint32_t regEnd;
  IndexReference name;
  DXIL::RDATData::ResourceFlags flags;
};

// serialised equivalent to RDAT::FunctionInfo
struct EncodedFunctionInfo
{
  IndexReference name;
  IndexReference unmangledName;
  IndexReference globalResourcesIndexArrayRef;
  IndexReference functionDependenciesArrayRef;
  DXBC::ShaderType type;
  uint32_t payloadBytes;
  uint32_t attribBytes;
  // extremely annoyingly this is two 32-bit integers which is relevant since 64-bit alignment
  // causes extra packing in the struct
  uint32_t featureFlags[2];
  uint32_t shaderCompatMask;    // bitmask based on DXBC::ShaderType enum of stages this function
                                // could be used with.
  uint16_t minShaderModel;
  uint16_t minType;    // looks to always be equal to type above
};

// serialised equivalent to RDAT::FunctionInfo2
struct EncodedFunctionInfo2
{
  EncodedFunctionInfo info1;

  uint8_t minWaveCount;
  uint8_t maxWaveCount;
  DXIL::RDATData::ShaderBehaviourFlags shaderBehaviourFlags;

  // below here is a stage-specific set of data containing e.g. signature elements. Currently
  // DXC does not emit RDAT except for in library targets, so this will be unused. It would be an
  // index into a table elsewhere of VSInfo, PSInfo, etc.
  IndexReference extraInfoRef;
};

// serialised equivalent to RDAT::FunctionInfo2
struct EncodedSubobjectInfo
{
  DXIL::RDATData::SubobjectInfo::SubobjectType type;
  IndexReference name;

  // we union members where possible but several contain arrays/strings which can't be unioned.

  union
  {
    DXIL::RDATData::SubobjectInfo::StateConfig config;
    DXIL::RDATData::SubobjectInfo::RTShaderConfig rtshaderconfig;
    DXIL::RDATData::SubobjectInfo::RTPipeConfig1 rtpipeconfig;

    struct
    {
      BytesReference data;
    } rs;

    struct
    {
      IndexReference subobject;
      IndexReference exports;
    } assoc;

    struct
    {
      DXIL::RDATData::HitGroupType type;
      IndexReference anyHit;
      IndexReference closestHit;
      IndexReference intersection;
    } hitgroup;
  };
};

static void BakeRuntimePart(rdcarray<bytebuf> &parts, DXIL::RDATData::Part part, void *data,
                            uint32_t byteSize)
{
  // empty parts are skipped
  if(byteSize == 0)
    return;

  const uint32_t alignedDataSize = (uint32_t)AlignUp4(byteSize);
  const RuntimePartHeader header = {part, alignedDataSize};

  bytebuf b;
  b.resize(alignedDataSize + sizeof(header));
  memcpy(b.data(), &header, sizeof(header));
  memcpy(b.data() + sizeof(header), data, byteSize);
  parts.push_back(std::move(b));
}

template <typename TableType>
static void BakeRuntimeTablePart(rdcarray<bytebuf> &parts, DXIL::RDATData::Part part,
                                 const rdcarray<TableType> &entries)
{
  // empty parts are skipped
  if(entries.size() == 0)
    return;

  const uint32_t alignedEntriesSize = (uint32_t)AlignUp4(entries.byteSize());
  const RuntimePartTableHeader tableHeader = {(uint32_t)entries.count(),
                                              (uint32_t)AlignUp4(sizeof(TableType))};
  const RuntimePartHeader header = {part, alignedEntriesSize + sizeof(tableHeader)};

  bytebuf b;
  b.resize(alignedEntriesSize + sizeof(header) + sizeof(tableHeader));
  memcpy(b.data(), &header, sizeof(header));
  memcpy(b.data() + sizeof(header), &tableHeader, sizeof(tableHeader));
  memcpy(b.data() + sizeof(header) + sizeof(tableHeader), entries.data(), entries.byteSize());
  parts.push_back(std::move(b));
}

};

namespace DXBC
{
bool DXBCContainer::GetPipelineValidation(DXIL::PSVData &psv) const
{
  using namespace DXIL;

  if(m_PSVOffset == 0)
    return false;

  return true;
}

void DXBCContainer::SetPipelineValidation(bytebuf &ByteCode, const DXIL::PSVData &psv)
{
}

bool DXBCContainer::GetRuntimeData(DXIL::RDATData &rdat) const
{
  using namespace DXIL;

  if(m_RDATOffset == 0)
    return false;

  return true;
}

void DXBCContainer::SetRuntimeData(bytebuf &ByteCode, const DXIL::RDATData &rdat)
{
}

};    // namespace DXBC
