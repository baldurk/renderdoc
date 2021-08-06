/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "dxbc_container.h"

#include "os/os_specific.h"
#include "dxbc_sdbg.h"

namespace DXBC
{
static const uint32_t FOURCC_SDBG = MAKE_FOURCC('S', 'D', 'B', 'G');

SDBGChunk::SDBGChunk(void *data)
{
  m_HasDebugInfo = false;

  m_ShaderFlags = 0;
  RDCEraseEl(m_Header);

  {
    uint32_t *raw = (uint32_t *)data;

    if(raw[0] != FOURCC_SDBG)
      return;

    m_RawData.resize(raw[1]);
    memcpy(&m_RawData[0], &raw[2], m_RawData.size());
  }

  char *dbgData = (char *)&m_RawData[0];

  m_Header = *((SDBGHeader *)dbgData);

  m_ShaderFlags = m_Header.shaderFlags;

  char *dbgPostHeader = dbgData + sizeof(SDBGHeader);

  SDBGFileHeader *FileHeaders = (SDBGFileHeader *)(dbgPostHeader + m_Header.files.offset);
  SDBGAsmInstruction *Instructions =
      (SDBGAsmInstruction *)(dbgPostHeader + m_Header.instructions.offset);
  SDBGVariable *Variables = (SDBGVariable *)(dbgPostHeader + m_Header.variables.offset);
  SDBGInputRegister *Inputs = (SDBGInputRegister *)(dbgPostHeader + m_Header.inputRegisters.offset);
  SDBGSymbol *SymbolTable = (SDBGSymbol *)(dbgPostHeader + m_Header.symbolTable.offset);
  SDBGScope *Scopes = (SDBGScope *)(dbgPostHeader + m_Header.scopes.offset);
  SDBGType *Types = (SDBGType *)(dbgPostHeader + m_Header.types.offset);
  int32_t *Int32DB = (int32_t *)(dbgPostHeader + m_Header.int32DBOffset);

  m_FileHeaders = rdcarray<SDBGFileHeader>(FileHeaders, m_Header.files.count);
  m_Instructions = rdcarray<SDBGAsmInstruction>(Instructions, m_Header.instructions.count);
  m_Variables = rdcarray<SDBGVariable>(Variables, m_Header.variables.count);
  m_Inputs = rdcarray<SDBGInputRegister>(Inputs, m_Header.inputRegisters.count);
  m_SymbolTable = rdcarray<SDBGSymbol>(SymbolTable, m_Header.symbolTable.count);
  m_Scopes = rdcarray<SDBGScope>(Scopes, m_Header.scopes.count);
  m_Types = rdcarray<SDBGType>(Types, m_Header.types.count);
  m_Int32Database = rdcarray<int32_t>(
      Int32DB, (m_Header.asciiDBOffset - m_Header.int32DBOffset) / sizeof(int32_t));

  char *asciiDatabase = dbgPostHeader + m_Header.asciiDBOffset;

  m_CompilerSig = asciiDatabase + m_Header.compilerSigOffset;
  m_Profile = asciiDatabase + m_Header.profileOffset;
  m_Entry = asciiDatabase + m_Header.entryFuncOffset;

  for(size_t i = 0; i < m_FileHeaders.size(); i++)
  {
    rdcstr filename =
        rdcstr(asciiDatabase + m_FileHeaders[i].filenameOffset, m_FileHeaders[i].filenameLen);
    rdcstr source = rdcstr(asciiDatabase + m_FileHeaders[i].sourceOffset, m_FileHeaders[i].sourceLen);

    this->Files.push_back(make_rdcpair(filename, source));
  }

  // successful grab of info
  m_HasDebugInfo = true;
}

void SDBGChunk::GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const
{
  if(instruction < m_Instructions.size())
  {
    int32_t symID = m_Instructions[instruction].symbol;
    if(symID > 0 && symID < (int32_t)m_SymbolTable.size())
    {
      const SDBGSymbol &sym = m_SymbolTable[symID];

      lineInfo.fileIndex = sym.fileID;
      lineInfo.lineStart = sym.lineNum;
      lineInfo.lineEnd = sym.lineNum;
      lineInfo.colStart = 0;
      lineInfo.colEnd = 0;
    }
  }
}

void SDBGChunk::GetCallstack(size_t instruction, uintptr_t offset, rdcarray<rdcstr> &callstack) const
{
  if(instruction < m_Instructions.size())
  {
    callstack = {m_Entry};
  }
}

bool SDBGChunk::HasSourceMapping() const
{
  return false;
}

void SDBGChunk::GetLocals(const DXBC::DXBCContainer *dxbc, size_t instruction, uintptr_t offset,
                          rdcarray<SourceVariableMapping> &locals) const
{
}

rdcstr SDBGChunk::GetSymbolName(int symbolID)
{
  RDCASSERT(symbolID >= 0 && symbolID < (int)m_SymbolTable.size());

  SDBGSymbol &sym = m_SymbolTable[symbolID];

  return GetSymbolName(sym.symbol.offset, sym.symbol.count);
}

rdcstr SDBGChunk::GetSymbolName(int32_t symbolOffset, int32_t symbolLength)
{
  RDCASSERT(symbolOffset < m_Header.compilerSigOffset);
  RDCASSERT(symbolOffset + symbolLength <= m_Header.compilerSigOffset);

  int32_t offset = sizeof(m_Header) + m_Header.asciiDBOffset + symbolOffset;

  return rdcstr(&m_RawData[offset], symbolLength);
}

IDebugInfo *ProcessSDBGChunk(void *data)
{
  return new SDBGChunk(data);
}

};    // namespace DXBC
