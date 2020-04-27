/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include <algorithm>
#include "api/app/renderdoc_app.h"
#include "common/common.h"
#include "driver/shaders/dxil/dxil_bytecode.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "dxbc_bytecode.h"

#include "driver/dx/official/d3dcompiler.h"

namespace DXBC
{
struct RDEFCBufferVariable
{
  uint32_t nameOffset;

  uint32_t startOffset;    // start offset in bytes of this variable in the cbuffer
  uint32_t size;           // size in bytes of this type
  uint32_t flags;

  uint32_t typeOffset;            // offset to a RDEFCBufferType
  uint32_t defaultValueOffset;    // offset to [size] bytes where the default value can be found, or
                                  // 0 for no default value

  uint32_t unknown[4];    // this is only present for RDEFHeader.targetVersion >= 0x500. In earlier
                          // versions, this is not in the file.
};

struct RDEFCBuffer
{
  uint32_t nameOffset;    // relative to the same offset base position as others in this chunk -
                          // after FourCC and chunk length.

  DXBC::CountOffset variables;
  uint32_t size;    // size in bytes of this cbuffer
  uint32_t flags;
  uint32_t type;

  // followed immediately by [variables.count] RDEFCBufferVariables
};

// mostly for nested structures
struct RDEFCBufferChildType
{
  uint32_t nameOffset;
  uint32_t typeOffset;      // offset to a RDEFCBufferType
  uint32_t memberOffset;    // byte offset in the parent structure - not a file offset
};

struct RDEFCBufferType
{
  uint16_t varClass;    // D3D_SHADER_VARIABLE_CLASS
  uint16_t varType;     // D3D_SHADER_VARIABLE_TYPE

  uint16_t rows;
  uint16_t cols;

  uint16_t numElems;
  uint16_t numMembers;

  uint32_t memberOffset;    // offset to [numMembers] RDEFCBufferChildTypes that point to the member
                            // types

  // my own guessing - not in wine structures
  // looks like these are only present for RD11 shaders
  uint32_t unknown[4];

  uint32_t nameOffset;    // offset to type name
};

// this isn't a proper chunk, it's the file header before all the chunks.
struct FileHeader
{
  uint32_t fourcc;          // "DXBC"
  uint32_t hashValue[4];    // unknown hash function and data
  uint32_t unknown;
  uint32_t fileLength;
  uint32_t numChunks;
  // uint32 chunkOffsets[numChunks]; follows
};

struct ILDNHeader
{
  uint16_t Flags;
  uint16_t NameLength;
  char Name[1];
};

struct RDEFHeader
{
  //////////////////////////////////////////////////////
  // offsets are relative to this position in the file.
  // NOT the end of this structure. Note this differs
  // from the SDBG chunk, but matches the SIGN chunks

  // note that these two actually come in the opposite order after
  // this header. So cbuffers offset will be higher than resources
  // offset
  CountOffset cbuffers;
  CountOffset resources;

  uint16_t targetVersion;        // 0x0501 is the latest.
  uint16_t targetShaderStage;    // 0xffff for pixel shaders, 0xfffe for vertex shaders

  uint32_t flags;
  uint32_t creatorOffset;    // null terminated ascii string

  uint32_t unknown[8];    // this is only present for targetVersion >= 0x500. In earlier versions,
                          // this is not in the file.
};

struct RDEFResource
{
  uint32_t nameOffset;    // relative to the same offset base position as others in this chunk -
                          // after FourCC and chunk length.

  uint32_t type;
  uint32_t retType;
  uint32_t dimension;
  int32_t sampleCount;
  uint32_t bindPoint;
  uint32_t bindCount;
  uint32_t flags;

  // this is only present for RDEFHeader.targetVersion >= 0x501.
  uint32_t space;
  // the ID seems to be a 0-based name fxc generates to refer to the object.
  // We don't use it, and it's easy enough to re-generate
  uint32_t ID;
};

struct SIGNHeader
{
  //////////////////////////////////////////////////////
  // offsets are relative to this position in the file.
  // NOT the end of this structure. Note this differs
  // from the SDBG chunk, but matches the RDEF chunk

  uint32_t numElems;
  uint32_t unknown;

  // followed by SIGNElement elements[numElems]; - note that SIGNElement's size depends on the type.
  // for OSG5 you should use SIGNElement7
};

struct PRIVHeader
{
  uint32_t fourcc;         // "PRIV"
  uint32_t chunkLength;    // length of this chunk

  GUID debugInfoGUID;    // GUID/magic number, since PRIV data could be used for something else.
                         // Set to the value of RENDERDOC_ShaderDebugMagicValue from
                         // renderdoc_app.h which can also be used as a GUID to set the path
                         // at runtime via SetPrivateData (see documentation)

  static const GUID RENDERDOC_ShaderDebugMagicValue;

  void *data;
};

const GUID PRIVHeader::RENDERDOC_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

struct SIGNElement
{
  uint32_t nameOffset;    // relative to the same offset base position as others in similar chunks -
                          // after FourCC and chunk length.

  uint32_t semanticIdx;
  SVSemantic systemType;
  uint32_t componentType;
  uint32_t registerNum;

  byte mask;
  byte rwMask;
  uint16_t unused;
};

struct SIGNElement7
{
  uint32_t stream;
  SIGNElement elem;
};

enum MinimumPrecision
{
  PRECISION_DEFAULT,
  PRECISION_FLOAT16,
  PRECISION_FLOAT10,
  PRECISION_UNUSED,
  PRECISION_SINT16,
  PRECISION_UINT16,
  PRECISION_ANY16,
  PRECISION_ANY10,

  NUM_PRECISIONS,
};

struct SIGNElement1
{
  uint32_t stream;
  SIGNElement elem;
  MinimumPrecision precision;
};

static const uint32_t STATSizeDX10 = 29 * 4;    // either 29 uint32s
static const uint32_t STATSizeDX11 = 37 * 4;    // or 37 uint32s

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
static const uint32_t FOURCC_Aon9 = MAKE_FOURCC('A', 'o', 'n', '9');
static const uint32_t FOURCC_PRIV = MAKE_FOURCC('P', 'R', 'I', 'V');
static const uint32_t FOURCC_DXIL = MAKE_FOURCC('D', 'X', 'I', 'L');
static const uint32_t FOURCC_ILDB = MAKE_FOURCC('I', 'L', 'D', 'B');
static const uint32_t FOURCC_ILDN = MAKE_FOURCC('I', 'L', 'D', 'N');

int TypeByteSize(VariableType t)
{
  switch(t)
  {
    case VARTYPE_UINT8: return 1;
    case VARTYPE_BOOL:
    case VARTYPE_INT:
    case VARTYPE_FLOAT:
    case VARTYPE_UINT:
      return 4;
    // we pretend for our purposes that the 'min' formats round up to 4 bytes. For any external
    // interfaces they are treated as regular types, only using lower precision internally.
    case VARTYPE_MIN8FLOAT:
    case VARTYPE_MIN10FLOAT:
    case VARTYPE_MIN16FLOAT:
    case VARTYPE_MIN12INT:
    case VARTYPE_MIN16INT:
    case VARTYPE_MIN16UINT: return 4;
    case VARTYPE_DOUBLE:
      return 8;
    // 'virtual' type. Just return 1
    case VARTYPE_INTERFACE_POINTER: return 1;
    default: RDCERR("Trying to take size of undefined type %d", t); return 1;
  }
}

ShaderBuiltin GetSystemValue(SVSemantic systemValue)
{
  switch(systemValue)
  {
    case SVNAME_UNDEFINED: return ShaderBuiltin::Undefined;
    case SVNAME_POSITION: return ShaderBuiltin::Position;
    case SVNAME_CLIP_DISTANCE: return ShaderBuiltin::ClipDistance;
    case SVNAME_CULL_DISTANCE: return ShaderBuiltin::CullDistance;
    case SVNAME_RENDER_TARGET_ARRAY_INDEX: return ShaderBuiltin::RTIndex;
    case SVNAME_VIEWPORT_ARRAY_INDEX: return ShaderBuiltin::ViewportIndex;
    case SVNAME_VERTEX_ID: return ShaderBuiltin::VertexIndex;
    case SVNAME_PRIMITIVE_ID: return ShaderBuiltin::PrimitiveIndex;
    case SVNAME_INSTANCE_ID: return ShaderBuiltin::InstanceIndex;
    case SVNAME_IS_FRONT_FACE: return ShaderBuiltin::IsFrontFace;
    case SVNAME_SAMPLE_INDEX: return ShaderBuiltin::MSAASampleIndex;
    case SVNAME_FINAL_QUAD_EDGE_TESSFACTOR0:
    case SVNAME_FINAL_QUAD_EDGE_TESSFACTOR1:
    case SVNAME_FINAL_QUAD_EDGE_TESSFACTOR2:
    case SVNAME_FINAL_QUAD_EDGE_TESSFACTOR3: return ShaderBuiltin::OuterTessFactor;
    case SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR0:
    case SVNAME_FINAL_QUAD_INSIDE_TESSFACTOR1: return ShaderBuiltin::InsideTessFactor;
    case SVNAME_FINAL_TRI_EDGE_TESSFACTOR0:
    case SVNAME_FINAL_TRI_EDGE_TESSFACTOR1:
    case SVNAME_FINAL_TRI_EDGE_TESSFACTOR2: return ShaderBuiltin::OuterTessFactor;
    case SVNAME_FINAL_TRI_INSIDE_TESSFACTOR: return ShaderBuiltin::InsideTessFactor;
    case SVNAME_FINAL_LINE_DETAIL_TESSFACTOR: return ShaderBuiltin::OuterTessFactor;
    case SVNAME_FINAL_LINE_DENSITY_TESSFACTOR: return ShaderBuiltin::InsideTessFactor;
    case SVNAME_TARGET: return ShaderBuiltin::ColorOutput;
    case SVNAME_DEPTH: return ShaderBuiltin::DepthOutput;
    case SVNAME_COVERAGE: return ShaderBuiltin::MSAACoverage;
    case SVNAME_DEPTH_GREATER_EQUAL: return ShaderBuiltin::DepthOutputGreaterEqual;
    case SVNAME_DEPTH_LESS_EQUAL: return ShaderBuiltin::DepthOutputLessEqual;
  }

  return ShaderBuiltin::Undefined;
}

rdcstr TypeName(CBufferVariableType::Descriptor desc)
{
  rdcstr ret;

  char *type = "";
  switch(desc.type)
  {
    case VARTYPE_BOOL: type = "bool"; break;
    case VARTYPE_INT: type = "int"; break;
    case VARTYPE_FLOAT: type = "float"; break;
    case VARTYPE_DOUBLE: type = "double"; break;
    case VARTYPE_UINT: type = "uint"; break;
    case VARTYPE_UINT8: type = "ubyte"; break;
    case VARTYPE_VOID: type = "void"; break;
    case VARTYPE_INTERFACE_POINTER: type = "interface"; break;
    case VARTYPE_MIN8FLOAT: type = "min8float"; break;
    case VARTYPE_MIN10FLOAT: type = "min10float"; break;
    case VARTYPE_MIN16FLOAT: type = "min16float"; break;
    case VARTYPE_MIN12INT: type = "min12int"; break;
    case VARTYPE_MIN16INT: type = "min16int"; break;
    case VARTYPE_MIN16UINT: type = "min16uint"; break;
    default: RDCERR("Unexpected type in RDEF variable type %d", type);
  }

  if(desc.varClass == CLASS_OBJECT)
    RDCERR("Unexpected object in RDEF variable type");
  else if(desc.varClass == CLASS_INTERFACE_CLASS)
    RDCERR("Unexpected iface class in RDEF variable type");
  else if(desc.varClass == CLASS_INTERFACE_POINTER)
    ret = type;
  else if(desc.varClass == CLASS_STRUCT)
    ret = "<unnamed>";
  else
  {
    if(desc.rows > 1)
    {
      ret = StringFormat::Fmt("%s%dx%d", type, desc.rows, desc.cols);

      if(desc.varClass == CLASS_MATRIX_ROWS)
      {
        ret = "row_major " + ret;
      }
    }
    else if(desc.cols > 1)
    {
      ret = StringFormat::Fmt("%s%d", type, desc.cols);
    }
    else
    {
      ret = type;
    }
  }

  return ret;
}

CBufferVariableType DXBCContainer::ParseRDEFType(RDEFHeader *h, char *chunkContents,
                                                 uint32_t typeOffset)
{
  if(m_Variables.find(typeOffset) != m_Variables.end())
    return m_Variables[typeOffset];

  RDEFCBufferType *type = (RDEFCBufferType *)(chunkContents + typeOffset);

  CBufferVariableType ret;

  ret.descriptor.varClass = (VariableClass)type->varClass;
  ret.descriptor.cols = type->cols;
  ret.descriptor.elements = type->numElems;
  ret.descriptor.members = type->numMembers;
  ret.descriptor.rows = type->rows;
  ret.descriptor.type = (VariableType)type->varType;

  ret.descriptor.name = TypeName(ret.descriptor);

  if(ret.descriptor.name == "interface")
  {
    if(h->targetVersion >= 0x500 && type->nameOffset > 0)
    {
      ret.descriptor.name += " " + rdcstr(chunkContents + type->nameOffset);
    }
    else
    {
      ret.descriptor.name += StringFormat::Fmt(" unnamed_iface_0x%08x", typeOffset);
    }
  }

  // rename unnamed structs to have valid identifiers as type name
  if(ret.descriptor.name.contains("<unnamed>"))
  {
    if(h->targetVersion >= 0x500 && type->nameOffset > 0)
    {
      ret.descriptor.name = chunkContents + type->nameOffset;
    }
    else
    {
      ret.descriptor.name = StringFormat::Fmt("unnamed_struct_0x%08x", typeOffset);
    }
  }

  if(type->memberOffset)
  {
    RDEFCBufferChildType *members = (RDEFCBufferChildType *)(chunkContents + type->memberOffset);

    ret.members.reserve(type->numMembers);

    ret.descriptor.bytesize = 0;

    for(int32_t j = 0; j < type->numMembers; j++)
    {
      CBufferVariable v;

      v.name = chunkContents + members[j].nameOffset;
      v.type = ParseRDEFType(h, chunkContents, members[j].typeOffset);
      v.descriptor.offset = members[j].memberOffset;

      ret.descriptor.bytesize = v.descriptor.offset + v.type.descriptor.bytesize;

      // N/A
      v.descriptor.flags = 0;
      v.descriptor.startTexture = 0;
      v.descriptor.numTextures = 0;
      v.descriptor.startSampler = 0;
      v.descriptor.numSamplers = 0;
      v.descriptor.defaultValue.clear();

      ret.members.push_back(v);
    }

    ret.descriptor.bytesize *= RDCMAX(1U, ret.descriptor.elements);
  }
  else
  {
    // matrices take up a full vector for each column or row depending which is major, regardless of
    // the other dimension
    if(ret.descriptor.varClass == CLASS_MATRIX_COLUMNS)
    {
      ret.descriptor.bytesize = TypeByteSize(ret.descriptor.type) * ret.descriptor.cols * 4 *
                                RDCMAX(1U, ret.descriptor.elements);
    }
    else if(ret.descriptor.varClass == CLASS_MATRIX_ROWS)
    {
      ret.descriptor.bytesize = TypeByteSize(ret.descriptor.type) * ret.descriptor.rows * 4 *
                                RDCMAX(1U, ret.descriptor.elements);
    }
    else
    {
      // arrays also take up a full vector for each element
      if(ret.descriptor.elements > 1)
        ret.descriptor.bytesize =
            TypeByteSize(ret.descriptor.type) * 4 * RDCMAX(1U, ret.descriptor.elements);
      else
        ret.descriptor.bytesize =
            TypeByteSize(ret.descriptor.type) * ret.descriptor.rows * ret.descriptor.cols;
    }
  }

  m_Variables[typeOffset] = ret;
  return ret;
}

D3D_PRIMITIVE_TOPOLOGY DXBCContainer::GetOutputTopology()
{
  if(m_OutputTopology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
  {
    m_OutputTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    if(m_DXBCByteCode)
      m_OutputTopology = m_DXBCByteCode->GetOutputTopology();
    else if(m_DXILByteCode)
      m_OutputTopology = m_DXILByteCode->GetOutputTopology();
  }

  return m_OutputTopology;
}

const rdcstr &DXBCContainer::GetDisassembly()
{
  if(m_Disassembly.empty())
  {
    uint32_t *hash =
        (uint32_t *)&m_ShaderBlob[4];    // hash is 4 uints, starting after the FOURCC of 'DXBC'

    m_Disassembly =
        StringFormat::Fmt("Shader hash %08x-%08x-%08x-%08x\n\n", hash[0], hash[1], hash[2], hash[3]);

    if(m_DXBCByteCode)
      m_Disassembly += m_DXBCByteCode->GetDisassembly();
    else if(m_DXILByteCode)
      m_Disassembly += m_DXILByteCode->GetDisassembly();
  }

  return m_Disassembly;
}

void DXBCContainer::FillTraceLineInfo(ShaderDebugTrace &trace) const
{
  if(m_DXBCByteCode)
  {
    trace.lineInfo.resize(m_DXBCByteCode->GetNumInstructions());
    for(size_t i = 0; i < m_DXBCByteCode->GetNumInstructions(); i++)
    {
      const DXBCBytecode::Operation &op = m_DXBCByteCode->GetInstruction(i);

      if(m_DebugInfo)
        m_DebugInfo->GetLineInfo(i, op.offset, trace.lineInfo[i]);

      // we add two lines for the shader hash on top of what the bytecode disassembler did
      if(op.line > 0)
        trace.lineInfo[i].disassemblyLine = 2 + op.line;
    }
  }
}

void DXBCContainer::FillStateInstructionInfo(ShaderDebugState &state) const
{
  uint32_t instruction = state.nextInstruction;

  uintptr_t offset = 0;

  state.sourceVars.clear();

  if(m_DXBCByteCode)
  {
    if(instruction < m_DXBCByteCode->GetNumInstructions())
      offset = m_DXBCByteCode->GetInstruction(instruction).offset;

    if(m_DebugInfo)
      m_DebugInfo->GetLocals(m_DXBCByteCode, instruction, offset, state.sourceVars);
  }

  if(m_DebugInfo)
  {
    m_DebugInfo->GetCallstack(instruction, offset, state.callstack);
  }
  else
  {
    state.callstack.clear();
  }
}

void DXBCContainer::GetHash(uint32_t hash[4], const void *ByteCode, size_t BytecodeLength)
{
  if(BytecodeLength < sizeof(FileHeader))
  {
    memset(hash, 0, sizeof(uint32_t) * 4);
    return;
  }

  FileHeader *header = (FileHeader *)ByteCode;

  memcpy(hash, header->hashValue, sizeof(header->hashValue));
}

bool DXBCContainer::CheckForDebugInfo(const void *ByteCode, size_t ByteCodeLength)
{
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return false;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_SDBG)
    {
      return true;
    }
    else if(*fourcc == FOURCC_SPDB)
    {
      return true;
    }
  }

  return false;
}

bool DXBCContainer::CheckForShaderCode(const void *ByteCode, size_t ByteCodeLength)
{
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return false;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_SHEX || *fourcc == FOURCC_SHDR)
      return true;
  }

  return false;
}

rdcstr DXBCContainer::GetDebugBinaryPath(const void *ByteCode, size_t ByteCodeLength)
{
  rdcstr debugPath;
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(header->fourcc != FOURCC_DXBC)
    return debugPath;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return debugPath;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_PRIV)
    {
      PRIVHeader *privHeader = (PRIVHeader *)fourcc;
      if(privHeader->debugInfoGUID == PRIVHeader::RENDERDOC_ShaderDebugMagicValue)
      {
        const char *pathData = (char *)&privHeader->data;
        size_t pathLength = strnlen(pathData, privHeader->chunkLength);

        if(privHeader->chunkLength == (sizeof(GUID) + pathLength + 1))
        {
          debugPath.append(pathData, pathLength);
          return debugPath;
        }
      }
    }
  }

  return debugPath;
}

DXBCContainer::DXBCContainer(const void *ByteCode, size_t ByteCodeLength)
{
  RDCASSERT(ByteCodeLength < UINT32_MAX);

  RDCEraseEl(m_ShaderStats);

  m_ShaderBlob.resize(ByteCodeLength);
  memcpy(&m_ShaderBlob[0], ByteCode, m_ShaderBlob.size());

  char *data = (char *)&m_ShaderBlob[0];    // just for convenience

  FileHeader *header = (FileHeader *)&m_ShaderBlob[0];

  if(header->fourcc != FOURCC_DXBC)
    return;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return;

  // default to vertex shader to support blobs without RDEF chunks (e.g. used with
  // input layouts if they're super stripped down)
  m_Type = DXBC::ShaderType::Vertex;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
    uint32_t *chunkSize = (uint32_t *)(fourcc + 1);

    char *chunkContents = (char *)(chunkSize + 1);

    if(*fourcc == FOURCC_RDEF)
    {
      RDEFHeader *h = (RDEFHeader *)chunkContents;

      // for target version 0x500, unknown[0] is FOURCC_RD11.
      // for 0x501 it's "\x13\x13\D%"

      m_Reflection = new Reflection;

      if(h->targetShaderStage == 0xffff)
        m_Type = DXBC::ShaderType::Pixel;
      else if(h->targetShaderStage == 0xfffe)
        m_Type = DXBC::ShaderType::Vertex;

      else if(h->targetShaderStage == 0x4753)    // 'GS'
        m_Type = DXBC::ShaderType::Geometry;

      else if(h->targetShaderStage == 0x4853)    // 'HS'
        m_Type = DXBC::ShaderType::Hull;
      else if(h->targetShaderStage == 0x4453)    // 'DS'
        m_Type = DXBC::ShaderType::Domain;
      else if(h->targetShaderStage == 0x4353)    // 'CS'
        m_Type = DXBC::ShaderType::Compute;

      m_Reflection->SRVs.reserve(h->resources.count);
      m_Reflection->UAVs.reserve(h->resources.count);
      m_Reflection->Samplers.reserve(h->resources.count);

      struct CBufferBind
      {
        uint32_t reg, space, bindCount, identifier;
      };

      std::map<rdcstr, CBufferBind> cbufferbinds;

      uint32_t resourceStride = sizeof(RDEFResource);

      // versions before 5.1 don't have the space and ID
      if(h->targetVersion < 0x501)
      {
        resourceStride -= sizeof(RDEFResource) - offsetof(RDEFResource, space);
      }

      for(int32_t i = 0; i < h->resources.count; i++)
      {
        RDEFResource *res =
            (RDEFResource *)(chunkContents + h->resources.offset + i * resourceStride);

        ShaderInputBind desc;

        desc.name = chunkContents + res->nameOffset;
        desc.type = (ShaderInputBind::InputType)res->type;
        desc.space = h->targetVersion >= 0x501 ? res->space : 0;
        desc.reg = res->bindPoint;
        desc.bindCount = res->bindCount;
        desc.flags = res->flags;
        desc.retType = (DXBC::ResourceRetType)res->retType;
        desc.dimension = (ShaderInputBind::Dimension)res->dimension;
        desc.numSamples = res->sampleCount;

        // Bindless resources report a bind count of 0 from the shader bytecode, but many other
        // places in this codebase assume ~0U means bindless. Patch it up now.
        if(h->targetVersion >= 0x501 && desc.bindCount == 0)
          desc.bindCount = ~0U;

        if(desc.numSamples == ~0 && desc.retType != RETURN_TYPE_MIXED &&
           desc.retType != RETURN_TYPE_UNKNOWN && desc.retType != RETURN_TYPE_CONTINUED)
        {
          // uint, uint2, uint3, uint4 seem to be in these bits of flags.
          desc.numSamples = 1 + ((desc.flags & 0xC) >> 2);
        }

        // for cbuffers the names can be duplicated, so handle this by assuming
        // the order will match between binding declaration and cbuffer declaration
        // and append _s onto each subsequent buffer name
        if(desc.IsCBuffer())
        {
          rdcstr cname = desc.name;

          while(cbufferbinds.find(cname) != cbufferbinds.end())
            cname += "_";

          CBufferBind cb;
          cb.space = desc.space;
          cb.reg = desc.reg;
          cb.bindCount = desc.bindCount;
          cb.identifier = h->targetVersion >= 0x501 ? res->ID : desc.reg;
          cbufferbinds[cname] = cb;
        }
        else if(desc.IsSampler())
        {
          m_Reflection->Samplers.push_back(desc);
        }
        else if(desc.IsSRV())
        {
          m_Reflection->SRVs.push_back(desc);
        }
        else if(desc.IsUAV())
        {
          m_Reflection->UAVs.push_back(desc);
        }
        else
        {
          RDCERR("Unexpected type of resource: %u", desc.type);
        }
      }

      // Expand out any array resources. We deliberately place these at the end of the resources
      // array, so that any non-array resources can be picked up first before any arrays.
      //
      // The reason for this is that an array element could refer to an un-used alias in a bind
      // point, and an individual non-array resoruce will always refer to the used alias (an
      // un-used individual resource will be omitted entirely from the reflection
      //
      // Note we preserve the arrays in SM5.1
      if(h->targetVersion < 0x501)
      {
        for(rdcarray<ShaderInputBind> *arr :
            {&m_Reflection->SRVs, &m_Reflection->UAVs, &m_Reflection->Samplers})
        {
          rdcarray<ShaderInputBind> &resArray = *arr;
          for(size_t i = 0; i < resArray.size();)
          {
            if(resArray[i].bindCount > 1)
            {
              // remove the item from the array at this location
              ShaderInputBind desc = resArray.takeAt(i);

              rdcstr rname = desc.name;
              uint32_t arraySize = desc.bindCount;

              desc.bindCount = 1;

              for(uint32_t a = 0; a < arraySize; a++)
              {
                desc.name = StringFormat::Fmt("%s[%u]", rname.c_str(), a);
                resArray.push_back(desc);
                desc.reg++;
              }

              continue;
            }

            // just move on if this item wasn't arrayed
            i++;
          }
        }
      }

      std::set<rdcstr> cbuffernames;

      for(int32_t i = 0; i < h->cbuffers.count; i++)
      {
        RDEFCBuffer *cbuf =
            (RDEFCBuffer *)(chunkContents + h->cbuffers.offset + i * sizeof(RDEFCBuffer));

        CBuffer cb;

        // I have no real justification for this, it seems some cbuffers are included that are
        // empty and have nameOffset = 0, fxc seems to skip them so I'll do the same.
        // See github issue #122
        if(cbuf->nameOffset == 0)
          continue;

        cb.name = chunkContents + cbuf->nameOffset;

        cb.descriptor.name = chunkContents + cbuf->nameOffset;
        cb.descriptor.byteSize = cbuf->size;
        cb.descriptor.type = (CBuffer::Descriptor::Type)cbuf->type;
        cb.descriptor.flags = cbuf->flags;
        cb.descriptor.numVars = cbuf->variables.count;

        cb.variables.reserve(cbuf->variables.count);

        size_t varStride = sizeof(RDEFCBufferVariable);

        if(h->targetVersion < 0x500)
        {
          size_t extraData = sizeof(RDEFCBufferVariable) - offsetof(RDEFCBufferVariable, unknown);

          varStride -= extraData;

          // it seems in rare circumstances, this data is present even for targetVersion < 0x500.
          // use a heuristic to check if the lower stride would cause invalid-looking data
          // for variables. See github issue #122
          if(cbuf->variables.count > 1)
          {
            RDEFCBufferVariable *var =
                (RDEFCBufferVariable *)(chunkContents + cbuf->variables.offset + varStride);

            if(var->nameOffset > ByteCodeLength)
            {
              varStride += extraData;
            }
          }
        }

        for(int32_t vi = 0; vi < cbuf->variables.count; vi++)
        {
          RDEFCBufferVariable *var =
              (RDEFCBufferVariable *)(chunkContents + cbuf->variables.offset + vi * varStride);

          RDCASSERT(var->nameOffset < ByteCodeLength);

          CBufferVariable v;

          v.name = chunkContents + var->nameOffset;

          v.descriptor.defaultValue.resize(var->size);

          if(var->defaultValueOffset && var->defaultValueOffset != ~0U)
          {
            memcpy(&v.descriptor.defaultValue[0], chunkContents + var->defaultValueOffset, var->size);
          }

          v.descriptor.name = v.name;
          // v.descriptor.bytesize = var->size; // size with cbuffer padding
          v.descriptor.offset = var->startOffset;
          v.descriptor.flags = var->flags;

          v.descriptor.startTexture = (uint32_t)-1;
          v.descriptor.startSampler = (uint32_t)-1;
          v.descriptor.numSamplers = 0;
          v.descriptor.numTextures = 0;

          v.type = ParseRDEFType(h, chunkContents, var->typeOffset);

          cb.variables.push_back(v);
        }

        rdcstr cname = cb.name;

        while(cbuffernames.find(cname) != cbuffernames.end())
          cname += "_";

        cbuffernames.insert(cname);

        cb.identifier = cbufferbinds[cname].identifier;
        cb.space = cbufferbinds[cname].space;
        cb.reg = cbufferbinds[cname].reg;
        cb.bindCount = cbufferbinds[cname].bindCount;

        if(cb.descriptor.type == CBuffer::Descriptor::TYPE_CBUFFER)
        {
          m_Reflection->CBuffers.push_back(cb);
        }
        else if(cb.descriptor.type == CBuffer::Descriptor::TYPE_RESOURCE_BIND_INFO)
        {
          RDCASSERT(cb.variables.size() == 1 && cb.variables[0].name == "$Element");
          m_Reflection->ResourceBinds[cb.name] = cb.variables[0].type;
        }
        else if(cb.descriptor.type == CBuffer::Descriptor::TYPE_INTERFACE_POINTERS)
        {
          m_Reflection->Interfaces = cb;
        }
        else
        {
          RDCDEBUG("Unused information, buffer %d: %s", cb.descriptor.type,
                   cb.descriptor.name.c_str());
        }
      }
    }
    else if(*fourcc == FOURCC_STAT)
    {
      if(*chunkSize == STATSizeDX10)
      {
        memcpy(&m_ShaderStats, chunkContents, STATSizeDX10);
        m_ShaderStats.version = ShaderStatistics::STATS_DX10;
      }
      else if(*chunkSize == STATSizeDX11)
      {
        memcpy(&m_ShaderStats, chunkContents, STATSizeDX11);
        m_ShaderStats.version = ShaderStatistics::STATS_DX11;
      }
      else
      {
        RDCERR("Unexpected Unexpected STAT chunk version");
      }
    }
    else if(*fourcc == FOURCC_SHEX || *fourcc == FOURCC_SHDR)
    {
      m_DXBCByteCode = new DXBCBytecode::Program((const byte *)chunkContents, *chunkSize);
    }
    else if(*fourcc == FOURCC_ILDN)
    {
      ILDNHeader *h = (ILDNHeader *)chunkContents;

      m_DebugFileName = rdcstr(h->Name, h->NameLength);
    }
  }

  if(m_DXBCByteCode == NULL)
  {
    // prefer ILDB if present
    for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
    {
      uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
      uint32_t *chunkSize = (uint32_t *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t));

      char *chunkContents = (char *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t) * 2);

      if(*fourcc == FOURCC_ILDB)
      {
        m_DXILByteCode = new DXIL::Program((const byte *)chunkContents, *chunkSize);
      }
    }

    // if we didn't find it, look for DXIL
    if(m_DXILByteCode == NULL)
    {
      for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
      {
        uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
        uint32_t *chunkSize = (uint32_t *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t));

        char *chunkContents = (char *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t) * 2);

        if(*fourcc == FOURCC_DXIL)
        {
          m_DXILByteCode = new DXIL::Program((const byte *)chunkContents, *chunkSize);
        }
      }
    }
  }

  // get type/version that's used regularly and cheap to fetch
  if(m_DXBCByteCode)
  {
    m_Type = m_DXBCByteCode->GetShaderType();
    m_Version.Major = m_DXBCByteCode->GetMajorVersion();
    m_Version.Minor = m_DXBCByteCode->GetMinorVersion();

    m_DXBCByteCode->SetReflection(m_Reflection);
  }
  else if(m_DXILByteCode)
  {
    m_Type = m_DXILByteCode->GetShaderType();
    m_Version.Major = m_DXILByteCode->GetMajorVersion();
    m_Version.Minor = m_DXILByteCode->GetMinorVersion();

    // m_DXILByteCode->SetReflection(m_Reflection);
  }

  // if reflection information was stripped, attempt to reverse engineer basic info from
  // declarations
  if(m_Reflection == NULL)
  {
    // need to disassemble now to guess resources
    if(m_DXBCByteCode)
      m_Reflection = m_DXBCByteCode->GuessReflection();
    else if(m_DXILByteCode)
      m_Reflection = m_DXILByteCode->GetReflection();
    else
      m_Reflection = new Reflection;
  }

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
    // uint32_t *chunkSize = (uint32_t *)(fourcc + 1);

    char *chunkContents = (char *)(fourcc + 2);

    if(*fourcc == FOURCC_ISGN || *fourcc == FOURCC_OSGN || *fourcc == FOURCC_ISG1 ||
       *fourcc == FOURCC_OSG1 || *fourcc == FOURCC_OSG5 || *fourcc == FOURCC_PCSG)
    {
      SIGNHeader *sign = (SIGNHeader *)chunkContents;

      rdcarray<SigParameter> *sig = NULL;

      bool input = false;
      bool output = false;
      bool patch = false;

      if(*fourcc == FOURCC_ISGN || *fourcc == FOURCC_ISG1)
      {
        sig = &m_Reflection->InputSig;
        input = true;
      }
      if(*fourcc == FOURCC_OSGN || *fourcc == FOURCC_OSG1 || *fourcc == FOURCC_OSG5)
      {
        sig = &m_Reflection->OutputSig;
        output = true;
      }
      if(*fourcc == FOURCC_PCSG)
      {
        sig = &m_Reflection->PatchConstantSig;
        patch = true;
      }

      RDCASSERT(sig && sig->empty());

      SIGNElement *el0 = (SIGNElement *)(sign + 1);
      SIGNElement7 *el7 = (SIGNElement7 *)el0;
      SIGNElement1 *el1 = (SIGNElement1 *)el0;

      for(uint32_t signIdx = 0; signIdx < sign->numElems; signIdx++)
      {
        SigParameter desc;

        const SIGNElement *el = el0;

        if(*fourcc == FOURCC_ISG1 || *fourcc == FOURCC_OSG1)
        {
          desc.stream = el1->stream;

          // discard el1->precision as we don't use it and don't want to pollute the common API
          // structures

          el = &el1->elem;
        }

        if(*fourcc == FOURCC_OSG5)
        {
          desc.stream = el7->stream;

          el = &el7->elem;
        }

        ComponentType compType = (ComponentType)el->componentType;
        desc.compType = CompType::Float;
        if(compType == COMPONENT_TYPE_UINT32)
          desc.compType = CompType::UInt;
        else if(compType == COMPONENT_TYPE_SINT32)
          desc.compType = CompType::SInt;
        else if(compType != COMPONENT_TYPE_FLOAT32)
          RDCERR("Unexpected component type in signature");

        desc.regChannelMask = (uint8_t)(el->mask & 0xff);
        desc.channelUsedMask = (uint8_t)(el->rwMask & 0xff);
        desc.regIndex = el->registerNum;
        desc.semanticIndex = el->semanticIdx;
        desc.semanticName = chunkContents + el->nameOffset;
        desc.systemValue = GetSystemValue(el->systemType);
        desc.compCount = (desc.regChannelMask & 0x1 ? 1 : 0) + (desc.regChannelMask & 0x2 ? 1 : 0) +
                         (desc.regChannelMask & 0x4 ? 1 : 0) + (desc.regChannelMask & 0x8 ? 1 : 0);

        RDCASSERT(m_Type != DXBC::ShaderType::Max);

        // pixel shader outputs with registers are always targets
        if(m_Type == DXBC::ShaderType::Pixel && output &&
           desc.systemValue == ShaderBuiltin::Undefined && desc.regIndex <= 16)
          desc.systemValue = ShaderBuiltin::ColorOutput;

        // check system value semantics
        if(desc.systemValue == ShaderBuiltin::Undefined)
        {
          if(!_stricmp(desc.semanticName.c_str(), "SV_Position"))
            desc.systemValue = ShaderBuiltin::Position;
          if(!_stricmp(desc.semanticName.c_str(), "SV_ClipDistance"))
            desc.systemValue = ShaderBuiltin::ClipDistance;
          if(!_stricmp(desc.semanticName.c_str(), "SV_CullDistance"))
            desc.systemValue = ShaderBuiltin::CullDistance;
          if(!_stricmp(desc.semanticName.c_str(), "SV_RenderTargetArrayIndex"))
            desc.systemValue = ShaderBuiltin::RTIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_ViewportArrayIndex"))
            desc.systemValue = ShaderBuiltin::ViewportIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_VertexID"))
            desc.systemValue = ShaderBuiltin::VertexIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_PrimitiveID"))
            desc.systemValue = ShaderBuiltin::PrimitiveIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_InstanceID"))
            desc.systemValue = ShaderBuiltin::InstanceIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_DispatchThreadID"))
            desc.systemValue = ShaderBuiltin::DispatchThreadIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_GroupID"))
            desc.systemValue = ShaderBuiltin::GroupIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_GroupIndex"))
            desc.systemValue = ShaderBuiltin::GroupFlatIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_GroupThreadID"))
            desc.systemValue = ShaderBuiltin::GroupThreadIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_GSInstanceID"))
            desc.systemValue = ShaderBuiltin::GSInstanceIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_OutputControlPointID"))
            desc.systemValue = ShaderBuiltin::OutputControlPointIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_DomainLocation"))
            desc.systemValue = ShaderBuiltin::DomainLocation;
          if(!_stricmp(desc.semanticName.c_str(), "SV_IsFrontFace"))
            desc.systemValue = ShaderBuiltin::IsFrontFace;
          if(!_stricmp(desc.semanticName.c_str(), "SV_SampleIndex"))
            desc.systemValue = ShaderBuiltin::MSAASampleIndex;
          if(!_stricmp(desc.semanticName.c_str(), "SV_TessFactor"))
            desc.systemValue = ShaderBuiltin::OuterTessFactor;
          if(!_stricmp(desc.semanticName.c_str(), "SV_InsideTessFactor"))
            desc.systemValue = ShaderBuiltin::InsideTessFactor;
          if(!_stricmp(desc.semanticName.c_str(), "SV_Target"))
            desc.systemValue = ShaderBuiltin::ColorOutput;
          if(!_stricmp(desc.semanticName.c_str(), "SV_Depth"))
            desc.systemValue = ShaderBuiltin::DepthOutput;
          if(!_stricmp(desc.semanticName.c_str(), "SV_Coverage"))
            desc.systemValue = ShaderBuiltin::MSAACoverage;
          if(!_stricmp(desc.semanticName.c_str(), "SV_DepthGreaterEqual"))
            desc.systemValue = ShaderBuiltin::DepthOutputGreaterEqual;
          if(!_stricmp(desc.semanticName.c_str(), "SV_DepthLessEqual"))
            desc.systemValue = ShaderBuiltin::DepthOutputLessEqual;
        }

        RDCASSERT(desc.systemValue != ShaderBuiltin::Undefined || desc.regIndex >= 0);

        sig->push_back(desc);

        el0++;
        el1++;
        el7++;
      }

      for(uint32_t i = 0; i < sign->numElems; i++)
      {
        SigParameter &a = (*sig)[i];

        for(uint32_t j = 0; j < sign->numElems; j++)
        {
          SigParameter &b = (*sig)[j];
          if(i != j && a.semanticName == b.semanticName)
          {
            a.needSemanticIndex = true;
            break;
          }
        }

        rdcstr semanticIdxName = a.semanticName;
        if(a.needSemanticIndex)
          semanticIdxName += ToStr(a.semanticIndex);

        a.semanticIdxName = semanticIdxName;
      }
    }
    else if(*fourcc == FOURCC_Aon9)    // 10Level9 most likely
    {
      char *c = (char *)fourcc;
      RDCWARN("Unknown chunk: %c%c%c%c", c[0], c[1], c[2], c[3]);
    }
  }

  // make sure to fetch the dispatch threads dimension from disassembly
  if(m_Type == DXBC::ShaderType::Compute)
  {
    if(m_DXBCByteCode)
      m_DXBCByteCode->FetchComputeProperties(m_Reflection);
    else if(m_DXILByteCode)
      m_DXILByteCode->FetchComputeProperties(m_Reflection);
  }

  // initialise debug chunks last
  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_SDBG)
    {
      m_DebugInfo = MakeSDBGChunk(fourcc);
    }
    else if(*fourcc == FOURCC_SPDB)
    {
      m_DebugInfo = MakeSPDBChunk(fourcc);
    }
  }

  // we do a mini-preprocess of the files from the debug info to handle #line directives.
  // This means that any lines that our source file declares to be in another filename via a #line
  // get put in the right place for what the debug information hopefully matches.
  // We also concatenate duplicate lines and display them all, to handle edge cases where #lines
  // declare duplicates.

  if(m_DebugInfo)
  {
    m_DXBCByteCode->SetDebugInfo(m_DebugInfo);

    struct SplitFile
    {
      rdcstr filename;
      rdcarray<rdcstr> lines;
      bool modified = false;
    };

    rdcarray<SplitFile> splitFiles;

    splitFiles.resize(m_DebugInfo->Files.size());

    for(size_t i = 0; i < m_DebugInfo->Files.size(); i++)
      splitFiles[i].filename = m_DebugInfo->Files[i].first;

    for(size_t i = 0; i < m_DebugInfo->Files.size(); i++)
    {
      rdcarray<rdcstr> srclines;

      // start off writing to the corresponding output file.
      SplitFile *dstFile = &splitFiles[i];
      bool changedFile = false;

      size_t dstLine = 0;

      // skip this file if it doesn't contain #line
      if(!m_DebugInfo->Files[i].second.contains("#line"))
        continue;

      split(m_DebugInfo->Files[i].second, srclines, '\n');
      srclines.push_back("");

      // handle #line directives by inserting empty lines or erasing as necessary
      bool seenLine = false;

      for(size_t srcLine = 0; srcLine < srclines.size(); srcLine++)
      {
        if(srclines[srcLine].empty())
        {
          dstLine++;
          continue;
        }

        char *c = &srclines[srcLine][0];
        char *end = c + srclines[srcLine].size();

        while(*c == '\t' || *c == ' ' || *c == '\r')
          c++;

        if(c == end)
        {
          // blank line, just advance line counter
          dstLine++;
          continue;
        }

        if(c + 5 > end || strncmp(c, "#line", 5) != 0)
        {
          // only actually insert the line if we've seen a #line statement. Otherwise we're just
          // doing an identity copy. This can lead to problems e.g. if there are a few statements in
          // a file before the #line we then create a truncated list of lines with only those and
          // then start spitting the #line directives into other files. We still want to have the
          // original file as-is.
          if(seenLine)
          {
            // resize up to account for the current line, if necessary
            dstFile->lines.resize(RDCMAX(dstLine + 1, dstFile->lines.size()));

            // if non-empty, append this line (to allow multiple lines on the same line
            // number to be concatenated). To avoid screwing up line numbers we have to append with
            // a comment and not a newline.
            if(dstFile->lines[dstLine].empty())
              dstFile->lines[dstLine] = srclines[srcLine];
            else
              dstFile->lines[dstLine] += " /* multiple #lines overlapping */ " + srclines[srcLine];

            dstFile->modified = true;
          }

          // advance line counter
          dstLine++;

          continue;
        }

        seenLine = true;

        // we have a #line directive
        c += 5;

        if(c >= end)
        {
          // invalid #line, just advance line counter
          dstLine++;
          continue;
        }

        while(*c == '\t' || *c == ' ')
          c++;

        if(c >= end)
        {
          // invalid #line, just advance line counter
          dstLine++;
          continue;
        }

        // invalid #line, no line number. Skip/ignore and just advance line counter
        if(*c < '0' || *c > '9')
        {
          dstLine++;
          continue;
        }

        size_t newLineNum = 0;
        while(*c >= '0' && *c <= '9')
        {
          newLineNum *= 10;
          newLineNum += int((*c) - '0');
          c++;
        }

        // convert to 0-indexed line number
        if(newLineNum > 0)
          newLineNum--;

        while(*c == '\t' || *c == ' ')
          c++;

        if(*c == '"')
        {
          // filename
          c++;

          char *filename = c;

          // parse out filename
          while(*c != '"' && *c != 0)
          {
            if(*c == '\\')
            {
              // skip escaped characters
              c += 2;
            }
            else
            {
              c++;
            }
          }

          // parsed filename successfully
          if(*c == '"')
          {
            *c = 0;

            // find the new destination file
            bool found = false;
            size_t dstFileIdx = 0;

            for(size_t f = 0; f < splitFiles.size(); f++)
            {
              if(splitFiles[f].filename == filename)
              {
                found = true;
                dstFileIdx = f;
                break;
              }
            }

            if(found)
            {
              changedFile = (dstFile != &splitFiles[dstFileIdx]);
              dstFile = &splitFiles[dstFileIdx];
            }
            else
            {
              RDCWARN("Couldn't find filename '%s' in #line directive in debug info", filename);

              // make a dummy file to write into that won't be used.
              splitFiles.push_back(SplitFile());
              splitFiles.back().filename = filename;
              splitFiles.back().modified = true;

              changedFile = true;
              dstFile = &splitFiles.back();
            }

            // set the next line number, and continue processing
            dstLine = newLineNum;

            continue;
          }
          else
          {
            // invalid #line, ignore
            RDCERR("Couldn't parse #line directive: '%s'", srclines[srcLine].c_str());
            continue;
          }
        }
        else
        {
          // No filename. Set the next line number, and continue processing
          dstLine = newLineNum;
          continue;
        }
      }
    }

    for(size_t i = 0; i < m_DebugInfo->Files.size(); i++)
    {
      if(m_DebugInfo->Files[i].second.empty() || splitFiles[i].modified)
      {
        merge(splitFiles[i].lines, m_DebugInfo->Files[i].second, '\n');
      }
    }
  }

  // if we had bytecode in this container, ensure we had reflection. If it's a blob with only an
  // input signature then we can do without reflection.
  if(m_DXBCByteCode || m_DXILByteCode)
  {
    RDCASSERT(m_Reflection);
  }
}

DXBCContainer::~DXBCContainer()
{
  SAFE_DELETE(m_DebugInfo);
  SAFE_DELETE(m_DXBCByteCode);
  SAFE_DELETE(m_DXILByteCode);
  SAFE_DELETE(m_Reflection);
}

struct FxcArg
{
  uint32_t bit;
  const char *arg;
} fxc_flags[] = {
    {D3DCOMPILE_DEBUG, " /Zi "},
    {D3DCOMPILE_SKIP_VALIDATION, " /Vd "},
    {D3DCOMPILE_SKIP_OPTIMIZATION, " /Od "},
    {D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, " /Zpr "},
    {D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, " /Zpc "},
    {D3DCOMPILE_PARTIAL_PRECISION, " /Gpp "},
    //{D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT, " /XX "},
    //{D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT, " /XX "},
    {D3DCOMPILE_NO_PRESHADER, " /Op "},
    {D3DCOMPILE_AVOID_FLOW_CONTROL, " /Gfa "},
    {D3DCOMPILE_PREFER_FLOW_CONTROL, " /Gfp "},
    {D3DCOMPILE_ENABLE_STRICTNESS, " /Ges "},
    {D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, " /Gec "},
    {D3DCOMPILE_IEEE_STRICTNESS, " /Gis "},
    {D3DCOMPILE_WARNINGS_ARE_ERRORS, " /WX "},
    {D3DCOMPILE_RESOURCES_MAY_ALIAS, " /res_may_alias "},
    {D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, " /enable_unbounded_descriptor_tables "},
    {D3DCOMPILE_ALL_RESOURCES_BOUND, " /all_resources_bound "},
    {D3DCOMPILE_DEBUG_NAME_FOR_SOURCE, " /Zss "},
    {D3DCOMPILE_DEBUG_NAME_FOR_BINARY, " /Zsb "},
};

uint32_t DecodeFlags(const ShaderCompileFlags &compileFlags)
{
  uint32_t ret = 0;

  for(const ShaderCompileFlag flag : compileFlags.flags)
  {
    if(flag.name == "@cmdline")
    {
      rdcstr cmdline = flag.value;

      // ensure cmdline is surrounded by spaces and all whitespace is spaces. This means we can
      // search for our flags surrounded by space and ensure we get exact matches.
      for(char &c : cmdline)
        if(isspace(c))
          c = ' ';

      cmdline = " " + cmdline + " ";

      for(const FxcArg &arg : fxc_flags)
      {
        if(strstr(cmdline.c_str(), arg.arg))
          ret |= arg.bit;
      }

      // check optimisation special case
      if(strstr(cmdline.c_str(), " /O0 "))
        ret |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
      else if(strstr(cmdline.c_str(), " /O1 "))
        ret |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
      else if(strstr(cmdline.c_str(), " /O2 "))
        ret |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
      else if(strstr(cmdline.c_str(), " /O3 "))
        ret |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

      // ignore any other flags we might not understand

      break;
    }
  }

  return ret;
}

ShaderCompileFlags EncodeFlags(const uint32_t flags)
{
  ShaderCompileFlags ret;

  rdcstr cmdline;

  for(const FxcArg &arg : fxc_flags)
  {
    if(flags & arg.bit)
      cmdline += arg.arg;
  }

  // optimization flags are a special case.
  //
  // D3DCOMPILE_OPTIMIZATION_LEVEL0 = (1 << 14)
  // D3DCOMPILE_OPTIMIZATION_LEVEL1 = 0
  // D3DCOMPILE_OPTIMIZATION_LEVEL2 = ((1 << 14) | (1 << 15))
  // D3DCOMPILE_OPTIMIZATION_LEVEL3 = (1 << 15)

  uint32_t opt = (flags & D3DCOMPILE_OPTIMIZATION_LEVEL2);
  if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL0)
    cmdline += " /O0";
  else if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL1)
    cmdline += " /O1";
  else if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL2)
    cmdline += " /O2";
  else if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL3)
    cmdline += " /O3";

  ret.flags = {{"@cmdline", cmdline}};

  // If D3DCOMPILE_SKIP_OPTIMIZATION is set, then prefer source-level debugging as it should be
  // accurate enough to work with.
  if(flags & D3DCOMPILE_SKIP_OPTIMIZATION)
    ret.flags.push_back({"preferSourceDebug", "1"});

  return ret;
}

ShaderCompileFlags EncodeFlags(const IDebugInfo *dbg)
{
  return EncodeFlags(dbg ? dbg->GetShaderCompileFlags() : 0);
}

};    // namespace DXBC

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

#if 0

TEST_CASE("DO NOT COMMIT - convenience test", "[dxbc]")
{
  // this test loads a file from disk and passes it through DXBC::DXBCContainer. Useful for when you
  // are iterating on a shader and don't want to have to load a whole capture.
  bytebuf buf;
  FileIO::ReadAll("/path/to/container_file.dxbc", buf);

  DXBC::DXBCContainer container(buf.data(), buf.size());

  // the only thing fetched lazily is the disassembly, so grab that here

  rdcstr disasm = container.GetDisassembly();

  RDCLOG("%s", disasm.c_str());
}

#endif

TEST_CASE("Check DXBC flags are non-overlapping", "[dxbc]")
{
  for(const DXBC::FxcArg &a : DXBC::fxc_flags)
  {
    for(const DXBC::FxcArg &b : DXBC::fxc_flags)
    {
      if(a.arg == b.arg)
        continue;

      // no argument should be a subset of another argument
      rdcstr arga = a.arg;
      rdcstr argb = b.arg;
      arga.trim();
      argb.trim();
      INFO("a: '" << arga << "' b: '" << argb << "'");
      CHECK(strstr(arga.c_str(), argb.c_str()) == NULL);
      CHECK(strstr(argb.c_str(), arga.c_str()) == NULL);
    }
  }
}

TEST_CASE("Check DXBC flag encoding/decoding", "[dxbc]")
{
  SECTION("encode/decode identity")
  {
    uint32_t flags = D3DCOMPILE_PARTIAL_PRECISION | D3DCOMPILE_SKIP_OPTIMIZATION |
                     D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_OPTIMIZATION_LEVEL2;
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);

    flags = 0;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_DEBUG;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);
  };

  SECTION("encode/decode discards unrecognised parameters")
  {
    uint32_t flags = D3DCOMPILE_PARTIAL_PRECISION | (1 << 30);
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags2 == D3DCOMPILE_PARTIAL_PRECISION);

    ShaderCompileFlags compileflags;

    compileflags.flags = {
        {"@cmdline", "/Zi /Z8 /JJ /WX /K other words embed/Odparam /DFoo=\"bar\""}};

    flags2 = DXBC::DecodeFlags(compileflags);

    CHECK(flags2 == (D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS));

    flags = ~0U;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    uint32_t allflags = 0;
    for(const DXBC::FxcArg &a : DXBC::fxc_flags)
      allflags |= a.bit;

    allflags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;

    CHECK(flags2 == allflags);
  };

  SECTION("optimisation flags are properly decoded and encoded")
  {
    uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL0;
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL1;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL2;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags));

    CHECK(flags == flags2);
  };
}
#endif
