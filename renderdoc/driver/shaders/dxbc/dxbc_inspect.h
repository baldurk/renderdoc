/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "dxbc_disassemble.h"

using std::vector;
using std::pair;
using std::string;
using std::map;

// matches D3D11_SHADER_VERSION_TYPE from d3d11shader.h
enum D3D11_ShaderType
{
  D3D11_ShaderType_Pixel = 0,
  D3D11_ShaderType_Vertex = 1,
  D3D11_ShaderType_Geometry = 2,

  // D3D11 Shaders
  D3D11_ShaderType_Hull = 3,
  D3D11_ShaderType_Domain = 4,
  D3D11_ShaderType_Compute = 5,
};

// many thanks to winehq for information of format of RDEF, STAT and SIGN chunks:
// http://source.winehq.org/git/wine.git/blob/HEAD:/dlls/d3dcompiler_43/reflection.c
namespace DXBC
{
enum VariableType
{
  VARTYPE_VOID = 0,
  VARTYPE_BOOL,
  VARTYPE_INT,
  VARTYPE_FLOAT,
  VARTYPE_STRING,
  VARTYPE_TEXTURE,
  VARTYPE_TEXTURE1D,
  VARTYPE_TEXTURE2D,
  VARTYPE_TEXTURE3D,
  VARTYPE_TEXTURECUBE,
  VARTYPE_SAMPLER,
  VARTYPE_SAMPLER1D,
  VARTYPE_SAMPLER2D,
  VARTYPE_SAMPLER3D,
  VARTYPE_SAMPLERCUBE,
  VARTYPE_PIXELSHADER,
  VARTYPE_VERTEXSHADER,
  VARTYPE_PIXELFRAGMENT,
  VARTYPE_VERTEXFRAGMENT,
  VARTYPE_UINT,
  VARTYPE_UINT8,
  VARTYPE_GEOMETRYSHADER,
  VARTYPE_RASTERIZER,
  VARTYPE_DEPTHSTENCIL,
  VARTYPE_BLEND,
  VARTYPE_BUFFER,
  VARTYPE_CBUFFER,
  VARTYPE_TBUFFER,
  VARTYPE_TEXTURE1DARRAY,
  VARTYPE_TEXTURE2DARRAY,
  VARTYPE_RENDERTARGETVIEW,
  VARTYPE_DEPTHSTENCILVIEW,
  VARTYPE_TEXTURE2DMS,
  VARTYPE_TEXTURE2DMSARRAY,
  VARTYPE_TEXTURECUBEARRAY,
  VARTYPE_HULLSHADER,
  VARTYPE_DOMAINSHADER,
  VARTYPE_INTERFACE_POINTER,
  VARTYPE_COMPUTESHADER,
  VARTYPE_DOUBLE,
  VARTYPE_RWTEXTURE1D,
  VARTYPE_RWTEXTURE1DARRAY,
  VARTYPE_RWTEXTURE2D,
  VARTYPE_RWTEXTURE2DARRAY,
  VARTYPE_RWTEXTURE3D,
  VARTYPE_RWBUFFER,
  VARTYPE_BYTEADDRESS_BUFFER,
  VARTYPE_RWBYTEADDRESS_BUFFER,
  VARTYPE_STRUCTURED_BUFFER,
  VARTYPE_RWSTRUCTURED_BUFFER,
  VARTYPE_APPEND_STRUCTURED_BUFFER,
  VARTYPE_CONSUME_STRUCTURED_BUFFER,
};

struct ShaderInputBind
{
  string name;

  enum InputType
  {
    TYPE_CBUFFER = 0,
    TYPE_TBUFFER,
    TYPE_TEXTURE,
    TYPE_SAMPLER,
    TYPE_UAV_RWTYPED,
    TYPE_STRUCTURED,
    TYPE_UAV_RWSTRUCTURED,
    TYPE_BYTEADDRESS,
    TYPE_UAV_RWBYTEADDRESS,
    TYPE_UAV_APPEND_STRUCTURED,
    TYPE_UAV_CONSUME_STRUCTURED,
    TYPE_UAV_RWSTRUCTURED_WITH_COUNTER,
  } type;

  constexpr bool IsCBuffer() const { return type == TYPE_CBUFFER; }
  constexpr bool IsSampler() const { return type == TYPE_SAMPLER; }
  constexpr bool IsROResource() const
  {
    return type == TYPE_TBUFFER || type == TYPE_TEXTURE || type == TYPE_STRUCTURED ||
           type == TYPE_BYTEADDRESS;
  }
  constexpr bool IsUAV() const
  {
    return type == TYPE_UAV_RWTYPED || type == TYPE_UAV_RWSTRUCTURED ||
           type == TYPE_UAV_RWBYTEADDRESS || type == TYPE_UAV_APPEND_STRUCTURED ||
           type == TYPE_UAV_CONSUME_STRUCTURED || type == TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
  }

  uint32_t space;
  uint32_t reg;
  uint32_t bindCount;

  uint32_t flags;

  enum RetType
  {
    RETTYPE_UNKNOWN = 0,
    RETTYPE_UNORM = 1,
    RETTYPE_SNORM,
    RETTYPE_SINT,
    RETTYPE_UINT,
    RETTYPE_FLOAT,
    RETTYPE_MIXED,
    RETTYPE_DOUBLE,
    RETTYPE_CONTINUED,
  } retType;

  enum Dimension
  {
    DIM_UNKNOWN = 0,
    DIM_BUFFER,
    DIM_TEXTURE1D,
    DIM_TEXTURE1DARRAY,
    DIM_TEXTURE2D,
    DIM_TEXTURE2DARRAY,
    DIM_TEXTURE2DMS,
    DIM_TEXTURE2DMSARRAY,
    DIM_TEXTURE3D,
    DIM_TEXTURECUBE,
    DIM_TEXTURECUBEARRAY,
    DIM_BUFFEREX,
  } dimension;

  uint32_t numSamples;
};

/////////////////////////////////////////////////////////////////////////
// the below classes basically mimics the existing reflection interface.
//
// essentially all information comes from the wine project.
/////////////////////////////////////////////////////////////////////////

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

struct CBufferVariable;

enum VariableClass
{
  CLASS_SCALAR = 0,
  CLASS_VECTOR,
  CLASS_MATRIX_ROWS,
  CLASS_MATRIX_COLUMNS,
  CLASS_OBJECT,
  CLASS_STRUCT,
  CLASS_INTERFACE_CLASS,
  CLASS_INTERFACE_POINTER,
};

struct CBufferVariableType
{
  struct Descriptor
  {
    VariableClass varClass;
    VariableType type;
    uint32_t rows;
    uint32_t cols;
    uint32_t elements;
    uint32_t members;
    uint32_t bytesize;
    string name;
  } descriptor;

  // if a struct, these are variables for each member (this can obviously nest). Not all
  // elements of the nested member descriptor are valid, as this might not be in a cbuffer,
  // but might be a loose structure
  vector<CBufferVariable> members;
};

struct CBufferVariable
{
  string name;

  struct
  {
    string name;
    uint32_t offset;    // offset in parent (cbuffer or nested struct)
    uint32_t flags;
    std::vector<uint8_t> defaultValue;
    uint32_t startTexture;    // first texture
    uint32_t numTextures;
    uint32_t startSampler;    // first sampler
    uint32_t numSamplers;
  } descriptor;

  // type details of this variable
  CBufferVariableType type;
};

struct CBuffer
{
  string name;

  uint32_t space;
  uint32_t reg;
  uint32_t bindCount;

  struct Descriptor
  {
    string name;

    enum Type
    {
      TYPE_CBUFFER = 0,
      TYPE_TBUFFER,
      TYPE_INTERFACE_POINTERS,
      TYPE_RESOURCE_BIND_INFO,
    } type;

    uint32_t numVars;
    uint32_t byteSize;
    uint32_t flags;
  } descriptor;

  std::vector<CBufferVariable> variables;
};

struct RDEFHeader;

class DXBCDebugChunk
{
public:
  virtual ~DXBCDebugChunk() {}
  virtual string GetCompilerSig() const = 0;
  virtual string GetEntryFunction() const = 0;
  virtual string GetShaderProfile() const = 0;

  virtual uint32_t GetShaderCompileFlags() const = 0;

  vector<pair<string, string> > Files;    // <filename, source>

  virtual void GetFileLine(size_t instruction, uintptr_t offset, int32_t &fileIdx,
                           int32_t &lineNum) const = 0;
};

// declare one of these and pass in your shader bytecode, then inspect
// the members that are populated with the shader information.
class DXBCFile
{
public:
  DXBCFile(const void *ByteCode, size_t ByteCodeLength);
  ~DXBCFile() { SAFE_DELETE(m_DebugInfo); }
  D3D11_ShaderType m_Type;
  struct
  {
    uint32_t Major, Minor;
  } m_Version;

  ShaderStatistics m_ShaderStats;
  DXBCDebugChunk *m_DebugInfo;

  vector<uint32_t> m_Immediate;

  bool m_GuessedResources;
  vector<ShaderInputBind> m_Resources;

  vector<CBuffer> m_CBuffers;

  CBuffer m_Interfaces;

  map<string, CBufferVariableType> m_ResourceBinds;

  vector<SigParameter> m_InputSig;
  vector<SigParameter> m_OutputSig;
  vector<SigParameter> m_PatchConstantSig;

  uint32_t DispatchThreadsDimension[3];

  vector<uint32_t> m_HexDump;

  vector<byte> m_ShaderBlob;

  const string &GetDisassembly()
  {
    if(m_Disassembly.empty())
      MakeDisassemblyString();
    return m_Disassembly;
  }

  size_t GetNumDeclarations() { return m_Declarations.size(); }
  const ASMDecl &GetDeclaration(size_t i) { return m_Declarations[i]; }
  size_t GetNumInstructions() { return m_Instructions.size(); }
  const ASMOperation &GetInstruction(size_t i) { return m_Instructions[i]; }
  size_t NumOperands(OpcodeType op);

  static void GetHash(uint32_t hash[4], const void *ByteCode, size_t BytecodeLength);

  static bool CheckForDebugInfo(const void *ByteCode, size_t ByteCodeLength);
  static string GetDebugBinaryPath(const void *ByteCode, size_t ByteCodeLength);

private:
  DXBCFile(const DXBCFile &o);
  DXBCFile &operator=(const DXBCFile &o);

  void FetchTypeVersion();
  void DisassembleHexDump();
  void MakeDisassemblyString();
  void GuessResources();

  // these functions modify tokenStream pointer to point after the item
  // ExtractOperation/ExtractDecl returns false if not an operation (ie. it's a declaration)
  bool ExtractOperation(uint32_t *&tokenStream, ASMOperation &op, bool friendlyName);
  bool ExtractDecl(uint32_t *&tokenStream, ASMDecl &decl, bool friendlyName);
  bool ExtractOperand(uint32_t *&tokenStream, ToString flags, ASMOperand &oper);

  bool IsDeclaration(OpcodeType op);

  CBufferVariableType ParseRDEFType(RDEFHeader *h, char *chunk, uint32_t offset);
  map<uint32_t, CBufferVariableType> m_Variables;

  bool m_Disassembled;

  vector<ASMDecl>
      m_Declarations;    // declarations of inputs, outputs, constant buffers, temp registers etc.
  vector<ASMOperation> m_Instructions;

  string m_Disassembly;
};

};    // namespace DXBC
