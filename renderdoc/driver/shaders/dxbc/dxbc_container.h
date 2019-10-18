/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "driver/dx/official/d3dcommon.h"
#include "dxbc_common.h"

namespace DXBCBytecode
{
class Program;
};

namespace DXIL
{
class Program;
};

namespace DXBC
{
class IDebugInfo;
struct Reflection;
IDebugInfo *MakeSDBGChunk(void *data);
IDebugInfo *MakeSPDBChunk(Reflection *reflection, void *data);
};

// many thanks to winehq for information of format of RDEF, STAT and SIGN chunks:
// http://source.winehq.org/git/wine.git/blob/HEAD:/dlls/d3dcompiler_43/reflection.c
namespace DXBC
{
// this struct is the whole STAT chunk, since it's just a series of fixed numbers
// preceded by FourCC and chunk length as usual
//
// This should correspond to D3D11_SHADER_DESC, some elements aren't identified yet.
struct ShaderStatistics
{
  uint32_t instructionCount;
  uint32_t tempRegisterCount;
  uint32_t unknown_a;
  uint32_t dclCount;
  uint32_t fltInstructionCount;
  uint32_t intInstructionCount;
  uint32_t uintInstructionCount;
  uint32_t staticFlowControlCount;
  uint32_t dynamicFlowControlCount;
  uint32_t unknown_b;
  uint32_t tempArrayCount;
  uint32_t arrayInstructionCount;
  uint32_t cutInstructionCount;
  uint32_t emitInstructionCount;
  uint32_t sampleTexCount;
  uint32_t loadTexCount;
  uint32_t cmpTexCount;
  uint32_t sampleBiasTexCount;
  uint32_t sampleGradTexCount;
  uint32_t movInstructionCount;
  uint32_t unknown_c;
  uint32_t convInstructionCount;
  uint32_t unknown_d;
  uint32_t inputPrimCount;
  uint32_t gsOutputTopology;
  uint32_t gsMaxOutputVtxCount;
  uint32_t unknown_e[3];

  // below won't exist for dx10 shaders. They'll be filled with 0

  uint32_t unknown_f;
  uint32_t cControlPoints;
  uint32_t hsOutputPrim;
  uint32_t hsPartitioning;
  uint32_t tessellatorDomain;
  uint32_t unknown_g[3];

  enum Version
  {
    STATS_UNKNOWN = 0,
    STATS_DX10,
    STATS_DX11,
  } version;
};

std::string TypeName(CBufferVariableType::Descriptor desc);

struct RDEFHeader;

class IDebugInfo
{
public:
  virtual ~IDebugInfo() {}
  virtual std::string GetCompilerSig() const = 0;
  virtual std::string GetEntryFunction() const = 0;
  virtual std::string GetShaderProfile() const = 0;

  virtual uint32_t GetShaderCompileFlags() const = 0;

  std::vector<rdcpair<std::string, std::string> > Files;    // <filename, source>

  virtual void GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const = 0;

  virtual bool HasLocals() const = 0;
  virtual void GetLocals(size_t instruction, uintptr_t offset,
                         rdcarray<LocalVariableMapping> &locals) const = 0;
};

uint32_t DecodeFlags(const ShaderCompileFlags &compileFlags);
ShaderCompileFlags EncodeFlags(const IDebugInfo *dbg);
ShaderCompileFlags EncodeFlags(const uint32_t flags);

// declare one of these and pass in your shader bytecode, then inspect
// the members that are populated with the shader information.
class DXBCContainer
{
public:
  DXBCContainer(const void *ByteCode, size_t ByteCodeLength);
  ~DXBCContainer();
  DXBC::ShaderType m_Type = DXBC::ShaderType::Max;
  struct
  {
    uint32_t Major = 0, Minor = 0;
  } m_Version;

  std::vector<byte> m_ShaderBlob;

  const IDebugInfo *GetDebugInfo() const { return m_DebugInfo; }
  const Reflection *GetReflection() const { return m_Reflection; }
  D3D_PRIMITIVE_TOPOLOGY GetOutputTopology();

  const std::string &GetDisassembly();

  const DXBCBytecode::Program *GetDXBCByteCode() { return m_DXBCByteCode; }
  const DXIL::Program *GetDXILByteCode() { return m_DXILByteCode; }
  static void GetHash(uint32_t hash[4], const void *ByteCode, size_t BytecodeLength);

  static bool CheckForDebugInfo(const void *ByteCode, size_t ByteCodeLength);
  static bool CheckForShaderCode(const void *ByteCode, size_t ByteCodeLength);
  static std::string GetDebugBinaryPath(const void *ByteCode, size_t ByteCodeLength);

private:
  DXBCContainer(const DXBCContainer &o);
  DXBCContainer &operator=(const DXBCContainer &o);

  std::string m_Disassembly;

  D3D_PRIMITIVE_TOPOLOGY m_OutputTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

  CBufferVariableType ParseRDEFType(RDEFHeader *h, char *chunk, uint32_t offset);
  std::map<uint32_t, CBufferVariableType> m_Variables;

  rdcstr m_DebugFileName;

  ShaderStatistics m_ShaderStats;
  DXBCBytecode::Program *m_DXBCByteCode = NULL;
  DXIL::Program *m_DXILByteCode = NULL;
  IDebugInfo *m_DebugInfo = NULL;
  Reflection *m_Reflection = NULL;
};

};    // namespace DXBC
