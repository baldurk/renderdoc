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

#include <map>
#include "api/replay/rdcarray.h"
#include "api/replay/rdcflatmap.h"
#include "api/replay/rdcpair.h"
#include "api/replay/rdcstr.h"
#include "common/common.h"
#include "driver/dx/official/d3dcommon.h"
#include "dxbc_common.h"

namespace DXBC
{
class IDebugInfo;
struct Reflection;
IDebugInfo *ProcessSDBGChunk(void *data);
IDebugInfo *ProcessSPDBChunk(void *data);
IDebugInfo *ProcessPDB(byte *data, uint32_t length);
bool IsPDBFile(void *data, size_t length);
void UnwrapEmbeddedPDBData(bytebuf &bytes);
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
    STATS_DX12,
  } version;
};

enum class GlobalShaderFlags : int64_t
{
  None = 0,
  DoublePrecision = 0x000001,
  RawStructured = 0x000002,
  UAVsEveryStage = 0x000004,
  UAVCount64 = 0x000008,
  MinPrecision = 0x000010,
  DoubleExtensions11_1 = 0x000020,
  ShaderExtensions11_1 = 0x000040,
  ComparisonFilter = 0x000080,
  TiledResources = 0x000100,
  PSOutStencilref = 0x000200,
  PSInnerCoverage = 0x000400,
  TypedUAVAdditional = 0x000800,
  RasterOrderViews = 0x001000,
  ArrayIndexFromVert = 0x002000,
  WaveOps = 0x004000,
  Int64 = 0x008000,
  ViewInstancing = 0x010000,
  Barycentrics = 0x020000,
  NativeLowPrecision = 0x040000,
  ShadingRate = 0x080000,
  Raytracing1_1 = 0x100000,
  SamplerFeedback = 0x200000,
};

BITMASK_OPERATORS(GlobalShaderFlags);

static const uint32_t FOURCC_DXBC = MAKE_FOURCC('D', 'X', 'B', 'C');
static const uint32_t FOURCC_RDEF = MAKE_FOURCC('R', 'D', 'E', 'F');
static const uint32_t FOURCC_RD11 = MAKE_FOURCC('R', 'D', '1', '1');
static const uint32_t FOURCC_STAT = MAKE_FOURCC('S', 'T', 'A', 'T');
static const uint32_t FOURCC_SHEX = MAKE_FOURCC('S', 'H', 'E', 'X');
static const uint32_t FOURCC_SHDR = MAKE_FOURCC('S', 'H', 'D', 'R');
static const uint32_t FOURCC_SDBG = MAKE_FOURCC('S', 'D', 'B', 'G');
static const uint32_t FOURCC_SPDB = MAKE_FOURCC('S', 'P', 'D', 'B');
static const uint32_t FOURCC_ISGN = MAKE_FOURCC('I', 'S', 'G', 'N');
static const uint32_t FOURCC_OSGN = MAKE_FOURCC('O', 'S', 'G', 'N');
static const uint32_t FOURCC_ISG1 = MAKE_FOURCC('I', 'S', 'G', '1');
static const uint32_t FOURCC_OSG1 = MAKE_FOURCC('O', 'S', 'G', '1');
static const uint32_t FOURCC_OSG5 = MAKE_FOURCC('O', 'S', 'G', '5');
static const uint32_t FOURCC_PCSG = MAKE_FOURCC('P', 'C', 'S', 'G');
static const uint32_t FOURCC_PSG1 = MAKE_FOURCC('P', 'S', 'G', '1');
static const uint32_t FOURCC_Aon9 = MAKE_FOURCC('A', 'o', 'n', '9');
static const uint32_t FOURCC_PRIV = MAKE_FOURCC('P', 'R', 'I', 'V');
static const uint32_t FOURCC_DXIL = MAKE_FOURCC('D', 'X', 'I', 'L');
static const uint32_t FOURCC_ILDB = MAKE_FOURCC('I', 'L', 'D', 'B');
static const uint32_t FOURCC_ILDN = MAKE_FOURCC('I', 'L', 'D', 'N');
static const uint32_t FOURCC_HASH = MAKE_FOURCC('H', 'A', 'S', 'H');
static const uint32_t FOURCC_SFI0 = MAKE_FOURCC('S', 'F', 'I', '0');
static const uint32_t FOURCC_PSV0 = MAKE_FOURCC('P', 'S', 'V', '0');
static const uint32_t FOURCC_RTS0 = MAKE_FOURCC('R', 'T', 'S', '0');
static const uint32_t FOURCC_RDAT = MAKE_FOURCC('R', 'D', 'A', 'T');

struct RDEFHeader;

uint32_t DecodeFlags(const ShaderCompileFlags &compileFlags);
rdcstr GetProfile(const ShaderCompileFlags &compileFlags);
ShaderCompileFlags EncodeFlags(const uint32_t flags, const rdcstr &profile);
void EncodeDXCFlags(uint32_t flags, rdcarray<rdcwstr> &args);

// declare one of these and pass in your shader bytecode, then inspect
// the members that are populated with the shader information.
class DXBCContainer
{
public:
  DXBCContainer(const bytebuf &ByteCode, const rdcstr &debugInfoPath, GraphicsAPI api,
                uint32_t shaderExtReg, uint32_t shaderExtSpace);
  ~DXBCContainer();
  DXBCContainer(const DXBCContainer &o) = delete;
  DXBCContainer(DXBCContainer &&o) = delete;
  DXBCContainer &operator=(const DXBCContainer &o) = delete;

  DXBC::ShaderType m_Type = DXBC::ShaderType::Max;
  struct
  {
    uint32_t Major = 0, Minor = 0;
  } m_Version;

  const bytebuf &GetShaderBlob() const { return m_ShaderBlob; }
  const IDebugInfo *GetDebugInfo() const { return m_DebugInfo; }
  const Reflection *GetReflection() const { return m_Reflection; }
  D3D_PRIMITIVE_TOPOLOGY GetOutputTopology();

  CBufferVariableType GetRayPayload(const ShaderEntryPoint &entry)
  {
    if(m_RayPayloads.empty())
      return {};
    return m_RayPayloads[entry].first;
  }
  CBufferVariableType GetRayAttributes(const ShaderEntryPoint &entry)
  {
    if(m_RayPayloads.empty())
      return {};
    return m_RayPayloads[entry].second;
  }

  rdcarray<ShaderEntryPoint> GetEntryPoints() const { return m_EntryPoints; }

  const rdcstr &GetDisassembly(bool dxcStyle);
  void FillTraceLineInfo(ShaderDebugTrace &trace) const;

  static void StripChunk(bytebuf &ByteCode, uint32_t fourcc);
  static void ReplaceChunk(bytebuf &ByteCode, uint32_t fourcc, const byte *replacement, size_t size);

  template <typename T>
  static void ReplaceChunk(bytebuf &ByteCode, uint32_t fourcc, const rdcarray<T> &replacement)
  {
    ReplaceChunk(ByteCode, fourcc, (byte *)replacement.data(), replacement.byteSize());
  }

  static const byte *FindChunk(const bytebuf &ByteCode, uint32_t fourcc, size_t &size);

  const DXBCBytecode::Program *GetDXBCByteCode() const { return m_DXBCByteCode; }
  DXBCBytecode::Program *GetDXBCByteCode() { return m_DXBCByteCode; }
  const DXIL::Program *GetDXILByteCode() const { return m_DXILByteCode; }
  DXIL::Program *GetDXILByteCode() { return m_DXILByteCode; }
  static void GetHash(uint32_t hash[4], const void *ByteCode, size_t BytecodeLength);

  const byte *GetNonDebugDXILByteCode() const
  {
    return m_ShaderBlob.data() + m_NonDebugDXILByteCodeOffset;
  }
  size_t GetNonDebugDXILByteCodeSize() const { return m_NonDebugDXILByteCodeSize; }
  static bool IsHashedContainer(const void *ByteCode, size_t BytecodeLength);
  static bool HashContainer(void *ByteCode, size_t BytecodeLength);

  static bool UsesExtensionUAV(uint32_t slot, uint32_t space, const void *ByteCode,
                               size_t BytecodeLength);

  static bool CheckForDebugInfo(const void *ByteCode, size_t ByteCodeLength);
  static bool CheckForDXIL(const void *ByteCode, size_t ByteCodeLength);
  static bool CheckForRootSig(const void *ByteCode, size_t ByteCodeLength);
  static rdcstr GetDebugBinaryPath(const void *ByteCode, size_t ByteCodeLength);
  static D3D_PRIMITIVE_TOPOLOGY GetOutputTopology(const void *ByteCode, size_t ByteCodeLength);

private:
  void TryFetchSeparateDebugInfo(bytebuf &byteCode, const rdcstr &debugInfoPath);

  bytebuf m_DebugShaderBlob;
  bytebuf m_ShaderBlob;

  rdcstr m_Disassembly;
  bool m_DXCStyle = false;

  D3D_PRIMITIVE_TOPOLOGY m_OutputTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

  CBufferVariableType ParseRDEFType(const RDEFHeader *h, const byte *chunk, uint32_t offset);
  std::map<uint32_t, CBufferVariableType> m_Variables;

  uint32_t m_Hash[4];

  rdcpair<uint32_t, uint32_t> m_ShaderExt = {~0U, ~0U};

  rdcstr m_DebugFileName;
  GlobalShaderFlags m_GlobalFlags = GlobalShaderFlags::None;

  size_t m_NonDebugDXILByteCodeOffset = 0;
  size_t m_NonDebugDXILByteCodeSize = 0;

  rdcflatmap<ShaderEntryPoint, rdcpair<CBufferVariableType, CBufferVariableType>> m_RayPayloads;

  ShaderStatistics m_ShaderStats;
  DXBCBytecode::Program *m_DXBCByteCode = NULL;
  DXIL::Program *m_DXILByteCode = NULL;
  IDebugInfo *m_DebugInfo = NULL;
  Reflection *m_Reflection = NULL;
  rdcarray<ShaderEntryPoint> m_EntryPoints;
};

};    // namespace DXBC
