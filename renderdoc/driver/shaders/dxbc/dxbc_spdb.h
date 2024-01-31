/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <guiddef.h>
#include <map>
#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"
#include "dxbc_bytecode.h"

namespace DXBC
{
class PageMapping
{
public:
  PageMapping() = default;

  PageMapping(const byte **pages, uint32_t PageSize, uint32_t *indices, uint32_t numIndices)
  {
    direct = NULL;

    if(numIndices == 1)
    {
      direct = pages[indices[0]];
    }
    else
    {
      contiguous.resize(numIndices * PageSize);
      for(uint32_t i = 0; i < numIndices; i++)
        memcpy(&contiguous[i * PageSize], pages[indices[i]], PageSize);
    }
  }

  const byte *Data()
  {
    if(direct)
      return direct;
    return &contiguous[0];
  }

private:
  const byte *direct;
  bytebuf contiguous;
};

struct FileHeaderPage
{
  char identifier[32];
  uint32_t PageSize;
  uint32_t FreePageMapIdx;
  uint32_t PageCount;
  uint32_t RootDirSize;
  uint32_t zero;
  uint32_t RootDirectory[73];

  uint32_t PagesForByteSize(uint32_t ByteSize)
  {
    // align up ByteSize to PageSize
    return (ByteSize + PageSize - 1) / PageSize;
  }
};

struct GuidPageHeader
{
  uint32_t Version;
  uint32_t Signature;
  uint32_t Age;
  GUID Guid;
  uint32_t StringBytes;
  char Strings[1];
};

struct OffsetLength
{
  uint32_t offset;
  uint32_t byteLength;
};

struct TPIHash
{
  uint16_t streamNumber;
  uint16_t pad;
  uint32_t hashSize;
  uint32_t numBucket;
  OffsetLength hashVals;
  OffsetLength tiOffs;
  OffsetLength hashAdj;
};

struct TPIHeader
{
  uint32_t version;
  uint32_t headerSize;
  uint32_t typeMin;
  uint32_t typeMax;
  uint32_t dataSize;
  TPIHash hash;
};

struct DBIHeader
{
  uint32_t sig;
  int32_t ver;
  int32_t age;
  int16_t gssymStream;
  uint16_t vers;
  int16_t pssymStream;
  uint16_t pdbver;
  int16_t symrecStream;
  uint16_t pdbver2;
  int32_t gpmodiSize;
  int32_t secconSize;
  int32_t secmapSize;
  int32_t filinfSize;
  int32_t tsmapSize;
  int32_t mfcIndex;
  int32_t dbghdrSize;
  int32_t ecinfoSize;
  uint16_t flags;
  uint16_t machine;
  int32_t reserved;
};

struct DBIModule
{
  int32_t opened;

  // seccon
  int16_t section;
  int16_t pad1;
  int32_t offset;
  int32_t size;
  uint32_t flags_;
  int16_t module;
  int16_t pad2;
  uint32_t dataCrc;
  uint32_t relocCrc;

  uint16_t flags;
  int16_t stream;
  int32_t cbSyms;
  int32_t cbOldLines;
  int32_t cbLines;
  int16_t files;
  int16_t pad;
  uint32_t offsets;
  int32_t niSource;
  int32_t niCompiler;

  // invalid when this is read in-place!
  rdcstr moduleName;
  rdcstr objectName;
};

struct CompilandDetails
{
  uint8_t Language;
  uint8_t Flags;
  uint16_t Unknown;
  uint16_t Platform;

  struct
  {
    uint16_t Major, Minor, Build, QFE;
  } FrontendVersion, BackendVersion;

  // invalid when this is read in-place!
  rdcstr CompilerSig;
};

struct FileChecksum
{
  uint32_t nameIndex;
  uint8_t hashLength;
  uint8_t hashType;
  uint8_t hashData[1];
};

struct InstructionLocation
{
  bool statement = true;
  uint32_t offsetStart = 0;
  uint32_t offsetEnd = 0;
  uint32_t lineStart = 0;
  uint32_t lineEnd = 0;
  uint32_t colStart = 0;
  uint32_t colEnd = 0;
};

struct Inlinee
{
  uint32_t id;
  uint64_t ptr;
  uint64_t parentPtr;
  uint32_t fileOffs;
  uint32_t baseLineNum;
  rdcarray<InstructionLocation> locations;
};

struct Function
{
  uint32_t type = 0;
  rdcstr name;
};

struct PDBStream
{
  uint32_t byteLength;
  rdcarray<uint32_t> pageIndices;
};

struct LocalRange
{
  uint32_t startRange;
  uint32_t endRange;

  bool operator==(const LocalRange &o) const
  {
    return startRange == o.startRange && endRange == o.endRange;
  }
};

struct LocalMapping
{
  bool operator<(const LocalMapping &o) const { return range.startRange < o.range.startRange; }
  LocalRange range;
  uint8_t regFirstComp;
  uint32_t varFirstComp;
  uint32_t varOffset;
  uint32_t numComps;
  rdcarray<LocalRange> gaps;

  ShaderConstantType var;

  // stored here so that we don't need to have the register mapping at the time we parse the SPDB
  // chunk
  DXBCBytecode::OperandType regType;
  uint32_t regIndex;
};

class SPDBChunk : public IDebugInfo
{
public:
  SPDBChunk(byte *data, uint32_t length);
  SPDBChunk(const SPDBChunk &) = delete;
  SPDBChunk &operator=(const SPDBChunk &o) = delete;

  rdcstr GetCompilerSig() const { return m_CompilerSig; }
  rdcstr GetEntryFunction() const { return m_Entry; }
  rdcstr GetShaderProfile() const { return m_Profile; }
  ShaderCompileFlags GetShaderCompileFlags() const { return EncodeFlags(m_ShaderFlags, m_Profile); }
  void GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const;
  void GetCallstack(size_t instruction, uintptr_t offset, rdcarray<rdcstr> &callstack) const;

  bool HasSourceMapping() const;
  void GetLocals(const DXBC::DXBCContainer *dxbc, size_t instruction, uintptr_t offset,
                 rdcarray<SourceVariableMapping> &locals) const;

private:
  bool m_HasDebugInfo;

  rdcstr m_CompilerSig;

  rdcstr m_Entry;
  rdcstr m_Profile;

  uint32_t m_ShaderFlags;

  rdcarray<LocalMapping> m_Locals;

  struct InstInfo
  {
    LineColumnInfo lineInfo;
    rdcarray<rdcstr> callstack;
  };

  std::map<uint32_t, Function> m_Functions;
  std::map<uint32_t, InstInfo> m_InstructionInfo;
};
};
