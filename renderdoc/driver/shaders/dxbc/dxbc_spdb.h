/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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
#include <string>
#include <utility>
#include <vector>
#include "dxbc_disassemble.h"

using std::vector;
using std::pair;
using std::string;
using std::map;

namespace DXBC
{
class PageMapping
{
public:
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
  vector<byte> contiguous;
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
  string moduleName;
  string objectName;
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
  string CompilerSig;
};

struct LineNumbersHeader
{
  uint32_t offset;
  uint16_t sec;
  uint16_t flags;
  uint32_t cod;
};

struct FileLineNumbers
{
  uint32_t fileIdx;    // index = byte offset in hash chunk
  uint32_t numLines;
  uint32_t size;
};

#pragma pack(push, 1)
struct ProcHeader
{
  uint32_t Parent;
  uint32_t End;
  uint32_t Next;
  uint32_t Length;
  uint32_t DebugStart;
  uint32_t DebugEnd;
  uint32_t Type;
  uint32_t Offset;
  byte Unknown[3];
};
#pragma pack(pop)

struct InstructionLocation
{
  InstructionLocation() : funcEnd(false), offset(0), line(0), colStart(0), colEnd(0) {}
  bool funcEnd;
  uint32_t offset;
  uint32_t line;
  uint32_t colStart;
  uint32_t colEnd;
};

struct FuncCallLineNumbers
{
  uint32_t fileOffs;
  uint32_t baseLineNum;
  vector<InstructionLocation> locations;
};

struct Function
{
  uint8_t unkA;
  uint32_t unkB;
  uint16_t unkC;
  uint16_t unkD;
  uint16_t unkE;
  string funcName;
  vector<int8_t> things;
};

enum FuncCallBytestreamOpcodes
{
  EndStream = 0x0,

  SetByteOffset = 0x1,
  // 0x2
  AdvanceBytes = 0x3,

  FunctionEndNoAdvance = 0x4,
  // 0x5
  AdvanceLines = 0x6,

  PrologueEnd = 0x7,
  EpilogueBegin = 0x8,

  ColumnStart = 0x9,
  // 0xa

  AdvanceBytesAndLines = 0xb,
  EndOfFunction = 0xc,

  ColumnEnd = 0xd,
};

#pragma pack(push, 1)
struct RegisterVariableAssign
{
  uint32_t func;
  uint16_t unkflags;
  char name[1];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RegisterVariableAssignComponent
{
  uint16_t type;
  OperandType Type() { return (OperandType)type; }
  uint16_t unkA;
  uint16_t srcComp;
  uint16_t unkB;
  uint32_t instrOffset;
  uint16_t unkC;
  uint16_t unkD;
  uint16_t destComp;
  uint16_t unkE;    // destComp for len == 24
};
#pragma pack(pop)

struct PDBStream
{
  uint32_t byteLength;
  vector<uint32_t> pageIndices;
};

class SPDBChunk : public DXBCDebugChunk
{
public:
  SPDBChunk(void *data);

  string GetCompilerSig() const { return m_CompilandDetails.CompilerSig; }
  string GetEntryFunction() const { return m_Entry; }
  string GetShaderProfile() const { return m_Profile; }
  uint32_t GetShaderCompileFlags() const { return m_ShaderFlags; }
  void GetFileLine(size_t instruction, uintptr_t offset, int32_t &fileIdx, int32_t &lineNum) const;

private:
  SPDBChunk(const SPDBChunk &);
  SPDBChunk &operator=(const SPDBChunk &o);

  bool m_HasDebugInfo;

  CompilandDetails m_CompilandDetails;

  string m_Entry;
  string m_Profile;

  uint32_t m_ShaderFlags;

  map<uint32_t, pair<int32_t, uint32_t> > m_LineNumbers;
};
};
