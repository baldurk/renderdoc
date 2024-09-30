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

struct StringBuffer
{
  StringBuffer(bool deduplicating) : dedup(deduplicating)
  {
    stringblob.push_back('\0');    // starts with an empty string
  }

  void Reset()
  {
    stringblob.clear();
    stringblob.push_back('\0');
  }
  void Load(const void *data, size_t size) { stringblob.assign((const char *)data, size); }
  const char *GetString(IndexReference offs) { return stringblob.c_str() + offs.offset; }

  const rdcstr &GetBlob() { return stringblob; }

  IndexReference MakeRef(const rdcstr &str)
  {
    if(dedup)
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
    }
    else
    {
      // note: empty strings are deduplicated (unlike full strings) to offset 0 even in PSV...
      if(str.empty())
        return {0};
    }

    uint32_t ret = (uint32_t)stringblob.length();
    stringblob.append(str);
    // we need to explicitly include the NULL terminators
    stringblob.push_back('\0');
    return {ret};
  }

private:
  bool dedup;
  rdcstr stringblob;
};

struct IndexArrays
{
  IndexArrays(bool deduplicating, bool lengthprefixing)
      : dedup(deduplicating), prefix(lengthprefixing)
  {
  }

  void Load(const void *data, size_t size)
  {
    idxArrays.assign((const uint32_t *)data, size / sizeof(uint32_t));
  }

  template <typename T>
  struct Span
  {
    uint32_t length;
    T *idxs;

    size_t size() const { return length; }
    T &operator[](size_t i) { return idxs[i]; }

    operator rdcarray<T>() { return rdcarray<T>(idxs, length); }
  };

  template <typename T>
  Span<T> GetSpan(IndexReference offs)
  {
    if(prefix)
      return {idxArrays[offs.offset], (T *)&idxArrays[offs.offset + 1]};
    else
      return {idxArrays.count() - offs.offset, (T *)&idxArrays[offs.offset]};
  }

  const rdcarray<uint32_t> &GetBlob() { return idxArrays; }

  IndexReference MakeRef(const rdcarray<uint32_t> &idxs, bool emptyIsNull)
  {
    // ~0U indicates NULL, in some cases replaces an empty array
    if(emptyIsNull && idxs.empty())
      return {~0U};

    if(dedup)
    {
      // not efficient, we don't cache anything but do a straight linear search.
      uint32_t offs = 0;

      while(offs < idxArrays.size())
      {
        const uint32_t *curarray = idxArrays.data() + offs;
        uint32_t curlen;

        if(prefix)
        {
          // length-prefix on array
          curlen = *curarray;
          curarray++;
        }
        else
        {
          // no length, consider everything else feasible and look for a subset match
          curlen = idxArrays.count() - offs;
        }

        if((prefix && curlen == idxs.size()) || (!prefix && curlen >= idxs.size()))
        {
          bool allsame = true;
          for(uint32_t i = 0; i < idxs.size() && allsame; i++)
            allsame &= idxs[i] == curarray[i];

          if(allsame)
            return {offs};
        }

        // if length prefixing, skip past the length and the current array
        // otherwise if not just try at the next possible offset
        if(prefix)
        {
          offs += 1 + curlen;
        }
        else
        {
          offs++;
        }
      }
    }

    uint32_t ret = (uint32_t)idxArrays.size();
    // idx arrays are length prefixed
    if(prefix)
      idxArrays.push_back((uint32_t)idxs.size());
    idxArrays.append(idxs);
    return {ret};
  }

private:
  bool dedup;
  bool prefix;
  rdcarray<uint32_t> idxArrays;
};

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
  union
  {
    DXBC::ShaderType type;
    uint32_t type_padding_;    // pad to 32-bit so the enum can be 8-bit
  };
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

static void BakeRuntimePart(rdcarray<bytebuf> &parts, DXIL::RDATData::Part part, const void *data,
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

void DXIL::PSVData::Bitmask::WriteTable(StreamWriter &writer, uint32_t numVectors) const
{
  writer.Write(bitmask, TableByteSize(numVectors));
}

namespace DXBC
{
bool DXBCContainer::GetPipelineValidation(DXIL::PSVData &psv) const
{
  RDCCOMPILE_ASSERT(sizeof(DXIL::PSVData0) == DXIL::PSVData0::ExpectedSize,
                    "PSVData0 is not sized/packed correctly");
  RDCCOMPILE_ASSERT(sizeof(DXIL::PSVData1) == DXIL::PSVData1::ExpectedSize,
                    "PSVData1 is not sized/packed correctly");
  RDCCOMPILE_ASSERT(sizeof(DXIL::PSVData2) == DXIL::PSVData2::ExpectedSize,
                    "PSVData2 is not sized/packed correctly");
  RDCCOMPILE_ASSERT(offsetof(DXIL::PSVData1, inputSigElems) == sizeof(DXIL::PSVData0) + 4, "!!!");

  using namespace DXIL;

  if(m_PSVOffset == 0)
    return false;

  const byte *in = m_ShaderBlob.data() + m_PSVOffset;
  const byte *end = in + m_PSVSize;

  const uint32_t headerSize = *(uint32_t *)in;
  in += sizeof(uint32_t);
  DXIL::PSVData0 *header = (DXIL::PSVData0 *)in;
  in += headerSize;

  if(headerSize == sizeof(DXIL::PSVData0))
  {
    memcpy(&psv, header, headerSize);
    psv.version = PSVData::Version::Version0;

    psv.shaderType = m_Type;
  }
  else if(headerSize == sizeof(DXIL::PSVData1))
  {
    memcpy(&psv, header, headerSize);
    psv.version = PSVData::Version::Version1;
  }
  else if(headerSize == sizeof(DXIL::PSVData2))
  {
    memcpy(&psv, header, headerSize);
    psv.version = PSVData::Version::Version2;
  }
  else if(headerSize == DXIL::PSVData3::ExpectedSize)
  {
    memcpy(&psv, header, sizeof(DXIL::PSVData2));
    psv.version = PSVData::Version::Version3;
  }
  else if(headerSize > DXIL::PSVData3::ExpectedSize)
  {
    RDCWARN("Unexpected PSV header size %u, only reading ver3", headerSize);
    memcpy(&psv, header, sizeof(DXIL::PSVData2));
    psv.version = PSVData::Version::Version3;
  }
  else
  {
    // size is not larger than ver2, which means it's invalid
    RDCERR("Invalid PSV header size %u", headerSize);
    return false;
  }

  // resources are always present
  const uint32_t resourceCount = *(uint32_t *)in;
  in += sizeof(uint32_t);

  if(resourceCount)
  {
    const uint32_t resourceStride = *(uint32_t *)in;
    in += sizeof(uint32_t);

    if(resourceStride > sizeof(PSVResource1))
    {
      RDCWARN("Unexpected PSV resource stride %u, only reading ver1", resourceStride);
    }
    else if(resourceStride != sizeof(PSVResource0) && resourceStride != sizeof(PSVResource1))
    {
      RDCERR("Invalid PSV resource stride %u", resourceStride);
      return false;
    }

    psv.resourceVersion = PSVData::ResourceVersion::Version0;
    if(resourceStride >= sizeof(PSVResource1))
      psv.resourceVersion = PSVData::ResourceVersion::Version1;

    const byte *resources = in;
    in += resourceStride * resourceCount;

    psv.resources.reserve(resourceCount);
    for(uint32_t i = 0; i < resourceCount; i++)
    {
      PSVResource res;
      memcpy(&res, resources, resourceStride);

      psv.resources.push_back(res);

      resources += resourceStride;
    }
  }

  if(psv.version >= PSVData::Version::Version1)
  {
    const uint32_t stringBufSize = *(uint32_t *)in;
    in += sizeof(uint32_t);
    StringBuffer stringbuf(false);    // not deduplicated, though it doesn't matter on read
    stringbuf.Load(in, stringBufSize);
    in += AlignUp4(stringBufSize);    // should already be aligned but let's be safe

    const uint32_t idxArraySize = *(uint32_t *)in;
    in += sizeof(uint32_t);
    IndexArrays idxArrays(true, false);    // deduplicated and not length-prefixed
    idxArrays.Load(
        in, idxArraySize * sizeof(uint32_t));    // length is given as number of dwords not bytes
    in += idxArraySize * sizeof(uint32_t);

    if(psv.inputSigElems || psv.outputSigElems || psv.patchConstPrimSigElems)
    {
      const uint32_t sigStride = *(uint32_t *)in;
      in += sizeof(uint32_t);

      if(sigStride > PSVSignature0::SigStride)
      {
        RDCWARN("Unexpected PSV signature element stride %u, only reading ver1", sigStride);
      }
      else if(sigStride != PSVSignature0::SigStride)
      {
        RDCERR("Invalid PSV signature element stride %u", sigStride);
        return false;
      }

      psv.signatureVersion = PSVData::SignatureVersion::Version0;

      psv.inputSig.reserve(psv.inputSigElems);
      for(uint8_t i = 0; i < psv.inputSigElems; i++)
      {
        IndexReference name, semIndices;
        memcpy(&name, in, sizeof(IndexReference));
        in += sizeof(IndexReference);
        memcpy(&semIndices, in, sizeof(IndexReference));
        in += sizeof(IndexReference);

        PSVSignature sig;
        memcpy(&sig.properties, in, sizeof(sig.properties));
        in += sizeof(sig.properties);

        sig.name = stringbuf.GetString(name);
        sig.semIndices = idxArrays.GetSpan<uint32_t>(semIndices);
        sig.semIndices.resize(sig.properties.rows);

        psv.inputSig.push_back(sig);
      }

      psv.outputSig.reserve(psv.outputSigElems);
      for(uint8_t i = 0; i < psv.outputSigElems; i++)
      {
        IndexReference name, semIndices;
        memcpy(&name, in, sizeof(IndexReference));
        in += sizeof(IndexReference);
        memcpy(&semIndices, in, sizeof(IndexReference));
        in += sizeof(IndexReference);

        PSVSignature sig;
        memcpy(&sig.properties, in, sizeof(sig.properties));
        in += sizeof(sig.properties);

        sig.name = stringbuf.GetString(name);
        sig.semIndices = idxArrays.GetSpan<uint32_t>(semIndices);
        sig.semIndices.resize(sig.properties.rows);

        psv.outputSig.push_back(sig);
      }

      psv.patchConstPrimSig.reserve(psv.patchConstPrimSigElems);
      for(uint8_t i = 0; i < psv.patchConstPrimSigElems; i++)
      {
        IndexReference name, semIndices;
        memcpy(&name, in, sizeof(IndexReference));
        in += sizeof(IndexReference);
        memcpy(&semIndices, in, sizeof(IndexReference));
        in += sizeof(IndexReference);

        PSVSignature sig;
        memcpy(&sig.properties, in, sizeof(sig.properties));
        in += sizeof(sig.properties);

        sig.name = stringbuf.GetString(name);
        sig.semIndices = idxArrays.GetSpan<uint32_t>(semIndices);
        sig.semIndices.resize(sig.properties.rows);

        psv.patchConstPrimSig.push_back(sig);
      }
    }

    // view ID dependence table
    if(psv.useViewID)
    {
      for(uint32_t stream = 0; stream < PSVData::NumOutputStreams; stream++)
      {
        if(psv.outputSigVectors[stream])
        {
          psv.viewIDAffects.outputMask[stream].ReadTable(in, psv.outputSigVectors[stream]);
        }
      }

      // same union member
      RDCCOMPILE_ASSERT(offsetof(DXIL::PSVData1, hs1.sigPatchConstVectors) ==
                            offsetof(DXIL::PSVData1, ms1.sigPrimVectors),
                        "sigPatchConstVectors is not at the same offset as sigPrimVectors");
      if((m_Type == DXBC::ShaderType::Hull || m_Type == DXBC::ShaderType::Mesh) &&
         psv.hs1.sigPatchConstVectors)
      {
        psv.viewIDAffects.patchConstOrPrimMask.ReadTable(in, psv.hs1.sigPatchConstVectors);
      }
    }

    // IO dependence table
    for(uint32_t stream = 0; stream < PSVData::NumOutputStreams; stream++)
    {
      if(psv.inputSigVectors && psv.outputSigVectors[stream])
      {
        rdcarray<PSVData::Bitmask> &bitmask = psv.IODependencies[stream].dependentOutputsForInput;
        bitmask.resize(psv.inputSigVectors * 4);
        for(size_t i = 0; i < bitmask.size(); i++)
        {
          bitmask[i].ReadTable(in, psv.outputSigVectors[stream]);
        }
      }
    }

    // patch constant output on input dependence table
    if(m_Type == DXBC::ShaderType::Hull && psv.hs1.sigPatchConstVectors && psv.inputSigVectors)
    {
      rdcarray<PSVData::Bitmask> &bitmask = psv.PCIODependencies.dependentPCOutputsForInput;
      bitmask.resize(psv.inputSigVectors * 4);
      for(size_t i = 0; i < bitmask.size(); i++)
      {
        bitmask[i].ReadTable(in, psv.hs1.sigPatchConstVectors);
      }
    }

    // output on patch constant input dependence table
    if(m_Type == DXBC::ShaderType::Domain && psv.outputSigVectors[0] && psv.ds1.sigPatchConstVectors)
    {
      rdcarray<PSVData::Bitmask> &bitmask = psv.PCIODependencies.dependentPCOutputsForInput;
      bitmask.resize(psv.ds1.sigPatchConstVectors * 4);
      for(size_t i = 0; i < bitmask.size(); i++)
      {
        bitmask[i].ReadTable(in, psv.outputSigVectors[0]);
      }
    }

    if(psv.version >= PSVData::Version::Version3)
    {
      // annoyingly the entry name sits at the end of the header but BEFORE the string buffer...
      IndexReference *name = (IndexReference *)((byte *)header + sizeof(DXIL::PSVData2));

      psv.entryName = stringbuf.GetString(*name);
    }
  }

  RDCASSERT(in == end);

  return true;
}

void DXBCContainer::SetPipelineValidation(bytebuf &ByteCode, const DXIL::PSVData &psv)
{
  using namespace DXIL;

  StreamWriter writer(256);

  uint32_t headerSize = sizeof(PSVData::PSVData0);
  switch(psv.version)
  {
    default:
    case PSVData::Version::Version0: headerSize = sizeof(PSVData::PSVData0); break;
    case PSVData::Version::Version1: headerSize = sizeof(PSVData::PSVData1); break;
    case PSVData::Version::Version2: headerSize = sizeof(PSVData::PSVData2); break;
    case PSVData::Version::Version3: headerSize = PSVData::PSVData3::ExpectedSize; break;
  }

  // PSV does not deduplicate
  StringBuffer stringbuf(false);

  // PSV does deduplicate index arrays, but does not length prefix them
  IndexArrays idxArrays(true, false);

  // write header
  writer.Write<uint32_t>(headerSize);
  writer.Write(&psv, RDCMIN((uint32_t)sizeof(PSVData::PSVData2), headerSize));
  uint64_t entryNameOffset = writer.GetOffset();

  if(psv.version >= PSVData::Version::Version3)
    writer.Write<uint32_t>(0);

  // write resources
  const uint32_t resourceCount = psv.resources.count();
  writer.Write(resourceCount);

  if(resourceCount)
  {
    uint32_t resourceStride = sizeof(PSVResource1);

    if(psv.resourceVersion == PSVData::ResourceVersion::Version0)
      resourceStride = sizeof(PSVResource0);

    writer.Write(resourceStride);

    for(uint32_t i = 0; i < resourceCount; i++)
      writer.Write(&psv.resources[i], resourceStride);
  }

  if(psv.version >= PSVData::Version::Version1)
  {
    // gather string buffer and index arrays first so we can write them
    RDCASSERT(psv.inputSigElems == psv.inputSig.size());
    for(uint8_t i = 0; i < psv.inputSigElems; i++)
    {
      stringbuf.MakeRef(psv.inputSig[i].name);
      idxArrays.MakeRef(psv.inputSig[i].semIndices, false);
    }

    RDCASSERT(psv.outputSigElems == psv.outputSig.size());
    for(uint8_t i = 0; i < psv.outputSigElems; i++)
    {
      stringbuf.MakeRef(psv.outputSig[i].name);
      idxArrays.MakeRef(psv.outputSig[i].semIndices, false);
    }

    RDCASSERT(psv.patchConstPrimSigElems == psv.patchConstPrimSig.size());
    for(uint8_t i = 0; i < psv.patchConstPrimSigElems; i++)
    {
      stringbuf.MakeRef(psv.patchConstPrimSig[i].name);
      idxArrays.MakeRef(psv.patchConstPrimSig[i].semIndices, false);
    }

    if(psv.version >= PSVData::Version::Version3)
      writer.WriteAt(entryNameOffset, stringbuf.MakeRef(psv.entryName));

    const uint32_t stringBufSize = AlignUp4(stringbuf.GetBlob().count());
    writer.Write(stringBufSize);
    writer.Write(stringbuf.GetBlob().data(), stringbuf.GetBlob().size());
    writer.AlignTo<sizeof(uint32_t)>();

    // length is given as number of dwords not bytes
    const uint32_t idxArraySize = idxArrays.GetBlob().count();
    writer.Write(idxArraySize);
    writer.Write(idxArrays.GetBlob().data(), idxArraySize * sizeof(uint32_t));

    // since it's not deduplicated, reset the string buffer and we'll "recreate" it the same, to not
    // have to store all the references above. The index arrays will naturally deduplicate to be the same.
    stringbuf.Reset();

    // string buffer and index array are unconditionally written but we only write the signature
    // data with stride if there is some data to write
    if(psv.inputSigElems || psv.outputSigElems || psv.patchConstPrimSigElems)
    {
      uint32_t sigStride = PSVSignature0::SigStride;

      writer.Write(sigStride);

      for(uint8_t i = 0; i < psv.inputSigElems; i++)
      {
        writer.Write(stringbuf.MakeRef(psv.inputSig[i].name).offset);
        writer.Write(idxArrays.MakeRef(psv.inputSig[i].semIndices, false).offset);
        writer.Write(&psv.inputSig[i].properties, sizeof(PSVSignature0::Properties));
      }

      for(uint8_t i = 0; i < psv.outputSigElems; i++)
      {
        writer.Write(stringbuf.MakeRef(psv.outputSig[i].name).offset);
        writer.Write(idxArrays.MakeRef(psv.outputSig[i].semIndices, false).offset);
        writer.Write(&psv.outputSig[i].properties, sizeof(PSVSignature0::Properties));
      }

      for(uint8_t i = 0; i < psv.patchConstPrimSigElems; i++)
      {
        writer.Write(stringbuf.MakeRef(psv.patchConstPrimSig[i].name).offset);
        writer.Write(idxArrays.MakeRef(psv.patchConstPrimSig[i].semIndices, false).offset);
        writer.Write(&psv.patchConstPrimSig[i].properties, sizeof(PSVSignature0::Properties));
      }
    }

    // view ID dependence table
    if(psv.useViewID)
    {
      for(uint32_t stream = 0; stream < PSVData::NumOutputStreams; stream++)
      {
        if(psv.outputSigVectors[stream])
        {
          psv.viewIDAffects.outputMask[stream].WriteTable(writer, psv.outputSigVectors[stream]);
        }
      }

      // same union member
      RDCCOMPILE_ASSERT(offsetof(DXIL::PSVData1, hs1.sigPatchConstVectors) ==
                            offsetof(DXIL::PSVData1, ms1.sigPrimVectors),
                        "sigPatchConstVectors is not at the same offset as sigPrimVectors");
      if((psv.shaderType == DXBC::ShaderType::Hull || psv.shaderType == DXBC::ShaderType::Mesh) &&
         psv.hs1.sigPatchConstVectors)
      {
        psv.viewIDAffects.patchConstOrPrimMask.WriteTable(writer, psv.hs1.sigPatchConstVectors);
      }
    }

    // IO dependence table
    for(uint32_t stream = 0; stream < PSVData::NumOutputStreams; stream++)
    {
      if(psv.inputSigVectors && psv.outputSigVectors[stream])
      {
        const rdcarray<PSVData::Bitmask> &bitmask =
            psv.IODependencies[stream].dependentOutputsForInput;
        for(size_t i = 0; i < bitmask.size(); i++)
        {
          bitmask[i].WriteTable(writer, psv.outputSigVectors[stream]);
        }
      }
    }

    // patch constant output on input dependence table
    if(psv.shaderType == DXBC::ShaderType::Hull && psv.hs1.sigPatchConstVectors && psv.inputSigVectors)
    {
      const rdcarray<PSVData::Bitmask> &bitmask = psv.PCIODependencies.dependentPCOutputsForInput;
      for(size_t i = 0; i < bitmask.size(); i++)
      {
        bitmask[i].WriteTable(writer, psv.hs1.sigPatchConstVectors);
      }
    }

    // output on patch constant input dependence table
    if(psv.shaderType == DXBC::ShaderType::Domain && psv.outputSigVectors[0] &&
       psv.ds1.sigPatchConstVectors)
    {
      const rdcarray<PSVData::Bitmask> &bitmask = psv.PCIODependencies.dependentPCOutputsForInput;
      for(size_t i = 0; i < bitmask.size(); i++)
      {
        bitmask[i].WriteTable(writer, psv.outputSigVectors[0]);
      }
    }
  }

  DXBC::DXBCContainer::ReplaceChunk(ByteCode, DXBC::FOURCC_PSV0, writer.GetData(),
                                    (size_t)writer.GetOffset());
}

bool DXBCContainer::GetRuntimeData(DXIL::RDATData &rdat) const
{
  if(m_RDATOffset == 0)
    return false;

  return GetRuntimeData(m_ShaderBlob.data() + m_RDATOffset, m_RDATSize, rdat);
}

bool DXBCContainer::GetRuntimeData(const byte *RDATChunk, size_t RDATLength, DXIL::RDATData &rdat)
{
  using namespace DXIL;

  const byte *in = RDATChunk;

  // RDAT Header
  uint32_t *ver = (uint32_t *)in;
  if(ver[0] != RDATData::Version1_0)
    return false;

  uint32_t numParts = ver[1];
  rdcarray<uint32_t> partOffsets;
  partOffsets.resize(numParts);
  for(uint32_t i = 0; i < numParts; i++)
    partOffsets[i] = ver[2 + i];

  StringBuffer stringbuffer(true);
  IndexArrays indexArrays(true, true);
  bytebuf rawbytes;

  // we need to do this in two passes to first find the index arrays etc which can be referenced
  // before they have appeared :(
  for(uint32_t partOffset : partOffsets)
  {
    RuntimePartHeader *part = (RuntimePartHeader *)(in + partOffset);
    byte *data = (byte *)(part + 1);

    switch(part->part)
    {
      case RDATData::Part::StringBuffer: stringbuffer.Load(data, part->size); break;
      case RDATData::Part::IndexArrays: indexArrays.Load(data, part->size); break;
      case RDATData::Part::RawBytes: rawbytes.assign(data, part->size); break;
      default: break;    // ignore others for now
    }
  }

  for(uint32_t partOffset : partOffsets)
  {
    RuntimePartHeader *part = (RuntimePartHeader *)(in + partOffset);
    byte *data = (byte *)(part + 1);
    RuntimePartTableHeader *tableHeader = (RuntimePartTableHeader *)data;

    switch(part->part)
    {
      case RDATData::Part::StringBuffer: break;
      case RDATData::Part::IndexArrays: break;
      case RDATData::Part::RawBytes: break;
      case RDATData::Part::ResourceTable:
      {
        EncodedResourceInfo *infos = (EncodedResourceInfo *)(tableHeader + 1);

        RDCASSERT(tableHeader->stride == sizeof(EncodedResourceInfo));

        rdat.resourceInfo.reserve(tableHeader->count);
        for(uint32_t i = 0; i < tableHeader->count; i++)
        {
          EncodedResourceInfo &info = infos[i];
          rdat.resourceInfo.push_back({
              info.nspace,
              info.kind,
              info.LinearID,
              info.space,
              info.regStart,
              info.regEnd,
              stringbuffer.GetString(info.name),
              info.flags,
          });
        }

        break;
      }
      case RDATData::Part::FunctionTable:
      {
        data = (byte *)(tableHeader + 1);

        RDCASSERT(tableHeader->stride == sizeof(EncodedFunctionInfo2) ||
                  tableHeader->stride == sizeof(EncodedFunctionInfo));

        rdat.functionVersion = RDATData::FunctionInfoVersion::Version1;
        if(tableHeader->stride == sizeof(EncodedFunctionInfo2))
          rdat.functionVersion = RDATData::FunctionInfoVersion::Version2;

        rdat.functionInfo.reserve(tableHeader->count);
        for(uint32_t i = 0; i < tableHeader->count; i++)
        {
          EncodedFunctionInfo &info = (EncodedFunctionInfo &)*data;
          EncodedFunctionInfo2 &info2 = (EncodedFunctionInfo2 &)*data;

          RDATData::FunctionInfo out = {
              stringbuffer.GetString(info.name),
              stringbuffer.GetString(info.unmangledName),
              {},
              {},
              info.type,
              info.payloadBytes,
              info.attribBytes,
              DXBC::GlobalShaderFlags(uint64_t(info.featureFlags[0]) |
                                      uint64_t(info.featureFlags[1]) << 32),
              info.shaderCompatMask,
              info.minShaderModel,
              info.minType,
          };

          rdat.functionInfo.push_back(out);

          RDATData::FunctionInfo2 &func = rdat.functionInfo.back();

          if(info.globalResourcesIndexArrayRef.offset != ~0U)
          {
            IndexArrays::Span<uint32_t> resources =
                indexArrays.GetSpan<uint32_t>(info.globalResourcesIndexArrayRef);
            func.globalResources.reserve(resources.size());
            for(uint32_t j = 0; j < resources.size(); j++)
              func.globalResources.push_back({rdat.resourceInfo[resources[j]].nspace,
                                              rdat.resourceInfo[resources[j]].resourceIndex});
          }

          if(info.functionDependenciesArrayRef.offset != ~0U)
          {
            IndexArrays::Span<IndexReference> deps =
                indexArrays.GetSpan<IndexReference>(info.functionDependenciesArrayRef);
            func.functionDependencies.reserve(deps.size());
            for(uint32_t j = 0; j < deps.size(); j++)
              func.functionDependencies.push_back(stringbuffer.GetString(deps[j]));
          }

          if(rdat.functionVersion == RDATData::FunctionInfoVersion::Version2)
          {
            func.minWaveCount = info2.minWaveCount;
            func.maxWaveCount = info2.maxWaveCount;
            func.shaderBehaviourFlags = info2.shaderBehaviourFlags;

            // below here is a stage-specific set of data containing e.g. signature elements.
            // Currently DXC does not emit RDAT except for in library targets, so this will be
            // unused. It would be an index into a table elsewhere of VSInfo, PSInfo, etc.
            RDCASSERT(info2.extraInfoRef.offset == ~0U);
            func.extraInfoRef = ~0U;
          }

          data += tableHeader->stride;
        }

        break;
      }
      case RDATData::Part::SubobjectTable:
      {
        data = (byte *)(tableHeader + 1);

        EncodedSubobjectInfo *subobjects = (EncodedSubobjectInfo *)data;

        RDCASSERT(tableHeader->stride == sizeof(EncodedSubobjectInfo));

        rdat.subobjectsInfo.reserve(tableHeader->count);
        for(uint32_t i = 0; i < tableHeader->count; i++)
        {
          EncodedSubobjectInfo &info = subobjects[i];

          rdat.subobjectsInfo.push_back({
              info.type,
              stringbuffer.GetString(info.name),
          });

          RDATData::SubobjectInfo &sub = rdat.subobjectsInfo.back();

          switch(info.type)
          {
            case RDATData::SubobjectInfo::SubobjectType::StateConfig:
            {
              sub.config = info.config;
              break;
            }
              // these are only differentiated by the enum, the data is the same
            case RDATData::SubobjectInfo::SubobjectType::GlobalRS:
            case RDATData::SubobjectInfo::SubobjectType::LocalRS:
              sub.rs.data = bytebuf(rawbytes.data() + info.rs.data.offset, info.rs.data.size);
              break;
            case RDATData::SubobjectInfo::SubobjectType::SubobjectToExportsAssoc:
            {
              sub.assoc.subobject = stringbuffer.GetString(info.assoc.subobject);

              if(info.assoc.exports.offset != ~0U)
              {
                IndexArrays::Span<IndexReference> exports =
                    indexArrays.GetSpan<IndexReference>(info.assoc.exports);
                sub.assoc.exports.reserve(exports.size());
                for(uint32_t j = 0; j < exports.size(); j++)
                  sub.assoc.exports.push_back(stringbuffer.GetString(exports[j]));
              }

              break;
            }
            case RDATData::SubobjectInfo::SubobjectType::RTShaderConfig:
            {
              sub.rtshaderconfig = info.rtshaderconfig;
              break;
            }
              // we can treat these unions identically - in the old config case the flags will be
              // ignored and should be 0 but the struct is effective padded to the largest union
              // size because of the fixed stride anyway
            case RDATData::SubobjectInfo::SubobjectType::RTPipeConfig:
            {
              RDCASSERT(info.rtpipeconfig.flags == RDATData::RTPipeFlags::None);
              DELIBERATE_FALLTHROUGH();
            }
            case RDATData::SubobjectInfo::SubobjectType::RTPipeConfig1:
            {
              sub.rtpipeconfig = info.rtpipeconfig;
              break;
            }
            case RDATData::SubobjectInfo::SubobjectType::Hitgroup:
            {
              sub.hitgroup.type = info.hitgroup.type;
              sub.hitgroup.anyHit = stringbuffer.GetString(info.hitgroup.anyHit);
              sub.hitgroup.closestHit = stringbuffer.GetString(info.hitgroup.closestHit);
              sub.hitgroup.intersection = stringbuffer.GetString(info.hitgroup.intersection);
              break;
            }
            default:
            {
              RDCWARN("Unhandled subobject type %d", info.type);
              break;
            }
          }
        }

        break;
      }
      default: RDCWARN("Unhandled RDAT part %d, will not round-trip", part->part);
    }
  }

  return true;
}

void DXBCContainer::SetRuntimeData(bytebuf &ByteCode, const DXIL::RDATData &rdat)
{
  using namespace DXIL;

  StringBuffer stringblob(true);

  IndexArrays indexarrays(true, true);
  // due to how these are stored and deduplicated (and we have to deduplicate because DXC does so we
  // don't know if it's necessary) we have to store byte buffers individually or have some kind of
  // lookup which amounts to the same thing. This will get baked into rawbytes at the end
  rdcarray<bytebuf> rawbyteLookups;
  bytebuf rawbytes;

  rdcarray<EncodedResourceInfo> resourceInfo;
  rdcarray<EncodedFunctionInfo> functionInfo;
  rdcarray<EncodedFunctionInfo2> functionInfo2;
  rdcarray<EncodedSubobjectInfo> subobjectsInfo;

  resourceInfo.reserve(rdat.resourceInfo.size());
  for(const RDATData::ResourceInfo &info : rdat.resourceInfo)
  {
    resourceInfo.push_back({
        info.nspace,
        info.kind,
        info.resourceIndex,
        info.space,
        info.regStart,
        info.regEnd,
        stringblob.MakeRef(info.name),
        info.flags,
    });
  }

  // LLVM processes function dependencies first here which puts them into the string buffer in a different
  // order than if we just process all functions as we encode them.
  // That means we need to iterate function dependencies first too, to solidify string buffer
  // offsets in order to exactly match RDAT contents to what dxc produces
  for(const RDATData::FunctionInfo2 &info : rdat.functionInfo)
    for(const rdcstr &f : info.functionDependencies)
      stringblob.MakeRef(f);

  if(rdat.functionVersion == RDATData::FunctionInfoVersion::Version1)
  {
    rdcarray<uint32_t> functionDependenciesArray;
    rdcarray<uint32_t> globalResourcesIndexArray;

    functionInfo.reserve(rdat.functionInfo.size());
    for(const RDATData::FunctionInfo2 &info : rdat.functionInfo)
    {
      globalResourcesIndexArray.clear();
      functionDependenciesArray.clear();

      globalResourcesIndexArray.reserve(info.globalResources.size());
      for(const rdcpair<DXIL::ResourceClass, uint32_t> &res : info.globalResources)
      {
        int32_t idx = rdat.resourceInfo.indexOf(res);
        RDCASSERT(idx >= 0);
        globalResourcesIndexArray.push_back(idx);
      }

      functionDependenciesArray.reserve(info.functionDependencies.size());
      for(const rdcstr &f : info.functionDependencies)
        functionDependenciesArray.push_back(stringblob.MakeRef(f).offset);

      functionInfo.push_back({
          stringblob.MakeRef(info.name),
          stringblob.MakeRef(info.unmangledName),
          indexarrays.MakeRef(globalResourcesIndexArray, true),
          indexarrays.MakeRef(functionDependenciesArray, true),
          info.type,
          info.payloadBytes,
          info.attribBytes,
          {uint32_t(info.featureFlags) & 0xffffffff, uint64_t(info.featureFlags) >> 32},
          info.shaderCompatMask,
          info.minShaderModel,
          info.minType,
      });
    }
  }
  else if(rdat.functionVersion == RDATData::FunctionInfoVersion::Version2)
  {
    rdcarray<uint32_t> functionDependenciesArray;
    rdcarray<uint32_t> globalResourcesIndexArray;

    functionInfo2.reserve(rdat.functionInfo.size());
    for(const RDATData::FunctionInfo2 &info : rdat.functionInfo)
    {
      globalResourcesIndexArray.clear();
      functionDependenciesArray.clear();

      globalResourcesIndexArray.reserve(info.globalResources.size());
      for(const rdcpair<DXIL::ResourceClass, uint32_t> &res : info.globalResources)
      {
        int32_t idx = rdat.resourceInfo.indexOf(res);
        RDCASSERT(idx >= 0);
        globalResourcesIndexArray.push_back(idx);
      }

      functionDependenciesArray.reserve(info.functionDependencies.size());
      for(const rdcstr &f : info.functionDependencies)
        functionDependenciesArray.push_back(stringblob.MakeRef(f).offset);

      // don't expect any extra info currently
      RDCASSERT(info.extraInfoRef == ~0U);
      functionInfo2.push_back({
          {
              stringblob.MakeRef(info.name),
              stringblob.MakeRef(info.unmangledName),
              indexarrays.MakeRef(globalResourcesIndexArray, true),
              indexarrays.MakeRef(functionDependenciesArray, true),
              info.type,
              info.payloadBytes,
              info.attribBytes,
              {uint32_t(info.featureFlags) & 0xffffffff, uint64_t(info.featureFlags) >> 32},
              info.shaderCompatMask,
              info.minShaderModel,
              info.minType,
          },
          info.minWaveCount,
          info.maxWaveCount,
          info.shaderBehaviourFlags,

          // below here is a stage-specific set of data containing e.g. signature elements.
          // Currently DXC does not emit RDAT except for in library targets, so this will be
          // unused. It would be an index into a table elsewhere of VSInfo, PSInfo, etc.
          {~0U},
      });
    }
  }

  rdcarray<uint32_t> tmpIdxArray;
  subobjectsInfo.reserve(rdat.subobjectsInfo.size());
  for(const RDATData::SubobjectInfo &info : rdat.subobjectsInfo)
  {
    EncodedSubobjectInfo sub = {
        info.type,
        stringblob.MakeRef(info.name),
    };

    switch(info.type)
    {
      case RDATData::SubobjectInfo::SubobjectType::StateConfig:
      {
        sub.config = info.config;
        break;
      }
        // these are only differentiated by the enum, the data is the same
      case RDATData::SubobjectInfo::SubobjectType::GlobalRS:
      case RDATData::SubobjectInfo::SubobjectType::LocalRS:
        sub.rs.data = MakeBytesRef(rawbyteLookups, info.rs.data);
        break;
      case RDATData::SubobjectInfo::SubobjectType::SubobjectToExportsAssoc:
      {
        sub.assoc.subobject = stringblob.MakeRef(info.assoc.subobject);

        tmpIdxArray.clear();
        tmpIdxArray.reserve(info.assoc.exports.size());
        for(const rdcstr &f : info.assoc.exports)
          tmpIdxArray.push_back(stringblob.MakeRef(f).offset);

        sub.assoc.exports = indexarrays.MakeRef(tmpIdxArray, false);
        break;
      }
      case RDATData::SubobjectInfo::SubobjectType::RTShaderConfig:
      {
        sub.rtshaderconfig = info.rtshaderconfig;
        break;
      }
        // we can treat these unions identically - in the old config case the flags will be ignored
        // and should be 0 but the struct is effective padded to the largest union size because of
        // the fixed stride anyway
      case RDATData::SubobjectInfo::SubobjectType::RTPipeConfig:
      {
        RDCASSERT(info.rtpipeconfig.flags == RDATData::RTPipeFlags::None);
        DELIBERATE_FALLTHROUGH();
      }
      case RDATData::SubobjectInfo::SubobjectType::RTPipeConfig1:
      {
        sub.rtpipeconfig = info.rtpipeconfig;
        break;
      }
      case RDATData::SubobjectInfo::SubobjectType::Hitgroup:
      {
        sub.hitgroup.type = info.hitgroup.type;
        sub.hitgroup.anyHit = stringblob.MakeRef(info.hitgroup.anyHit);
        sub.hitgroup.closestHit = stringblob.MakeRef(info.hitgroup.closestHit);
        sub.hitgroup.intersection = stringblob.MakeRef(info.hitgroup.intersection);
        break;
      }
      default:
      {
        RDCWARN("Unhandled subobject type %d", info.type);
        break;
      }
    }

    subobjectsInfo.push_back(sub);
  }

  // concatenate bytes together now
  for(bytebuf &b : rawbyteLookups)
    rawbytes.append(b);

  // the order of these parts is important and matches dxc

  rdcarray<bytebuf> parts;

  BakeRuntimePart(parts, RDATData::Part::StringBuffer, stringblob.GetBlob().data(),
                  stringblob.GetBlob().count());
  BakeRuntimeTablePart(parts, RDATData::Part::ResourceTable, resourceInfo);
  if(!functionInfo.empty())
    BakeRuntimeTablePart(parts, RDATData::Part::FunctionTable, functionInfo);
  else
    BakeRuntimeTablePart(parts, RDATData::Part::FunctionTable, functionInfo2);
  BakeRuntimePart(parts, RDATData::Part::IndexArrays, indexarrays.GetBlob().data(),
                  (uint32_t)indexarrays.GetBlob().byteSize());
  BakeRuntimePart(parts, RDATData::Part::RawBytes, rawbytes.data(), (uint32_t)rawbytes.byteSize());
  BakeRuntimeTablePart(parts, RDATData::Part::SubobjectTable, subobjectsInfo);

  // write the header last now that the parts are complete

  // part offsets start immediately after the header which includes the part offsets themselves
  uint32_t offset = sizeof(RDATData::Version1_0) + sizeof(uint32_t) * (1 + parts.count());

  StreamWriter total(256);
  total.Write(RDATData::Version1_0);
  total.Write<uint32_t>(parts.count());
  for(size_t i = 0; i < parts.size(); i++)
  {
    total.Write(offset);
    // parts should already be uint32 aligned
    offset += (uint32_t)parts[i].byteSize();
  }
  // now write the parts themselves
  for(size_t i = 0; i < parts.size(); i++)
  {
    total.Write(parts[i].data(), parts[i].byteSize());
  }

  DXBC::DXBCContainer::ReplaceChunk(ByteCode, DXBC::FOURCC_RDAT, total.GetData(),
                                    (size_t)total.GetOffset());
}

};    // namespace DXBC
