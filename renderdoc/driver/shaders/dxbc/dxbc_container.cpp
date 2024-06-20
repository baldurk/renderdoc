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

#include "dxbc_container.h"
#include <algorithm>
#include "api/app/renderdoc_app.h"
#include "common/common.h"
#include "core/settings.h"
#include "driver/shaders/dxil/dxil_bytecode.h"
#include "lz4/lz4.h"
#include "md5/md5.h"
#include "replay/replay_driver.h"
#include "serialise/serialiser.h"
#include "dxbc_bytecode.h"

#include "driver/dx/official/d3dcompiler.h"

// this is extern so that it can be shared with vulkan
RDOC_EXTERN_CONFIG(rdcarray<rdcstr>, DXBC_Debug_SearchDirPaths);

namespace DXBC
{
rdcstr BasicDemangle(const rdcstr &possiblyMangledName)
{
  if(possiblyMangledName.size() > 2 && possiblyMangledName[0] == '\x1' &&
     possiblyMangledName[1] == '?')
  {
    int idx = possiblyMangledName.indexOf('@');
    if(idx > 2)
      return possiblyMangledName.substr(2, idx - 2);
  }

  return possiblyMangledName;
}

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
  uint32_t containerVersion;
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

enum class HASHFlags : uint32_t
{
  INCLUDES_SOURCE = 0x1,
};

struct HASHHeader
{
  HASHFlags Flags;
  uint32_t hashValue[4];
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
    case SVNAME_BARYCENTRICS: return ShaderBuiltin::Barycentrics;
    case SVNAME_SHADINGRATE: return ShaderBuiltin::PackedFragRate;
    case SVNAME_CULLPRIMITIVE: return ShaderBuiltin::CullPrimitive;
    case SVNAME_TARGET: return ShaderBuiltin::ColorOutput;
    case SVNAME_DEPTH: return ShaderBuiltin::DepthOutput;
    case SVNAME_COVERAGE: return ShaderBuiltin::MSAACoverage;
    case SVNAME_DEPTH_GREATER_EQUAL: return ShaderBuiltin::DepthOutputGreaterEqual;
    case SVNAME_DEPTH_LESS_EQUAL: return ShaderBuiltin::DepthOutputLessEqual;
    case SVNAME_STENCIL_REF: return ShaderBuiltin::StencilReference;
    case SVNAME_INNER_COVERAGE: return ShaderBuiltin::IsFullyCovered;
  }

  return ShaderBuiltin::Undefined;
}

ShaderStage GetShaderStage(ShaderType type)
{
  switch(type)
  {
    case DXBC::ShaderType::Pixel: return ShaderStage::Pixel;
    case DXBC::ShaderType::Vertex: return ShaderStage::Vertex;
    case DXBC::ShaderType::Geometry: return ShaderStage::Geometry;
    case DXBC::ShaderType::Hull: return ShaderStage::Hull;
    case DXBC::ShaderType::Domain: return ShaderStage::Domain;
    case DXBC::ShaderType::Compute: return ShaderStage::Compute;
    case DXBC::ShaderType::Amplification: return ShaderStage::Amplification;
    case DXBC::ShaderType::Mesh: return ShaderStage::Mesh;
    case DXBC::ShaderType::RayGeneration: return ShaderStage::RayGen;
    case DXBC::ShaderType::Intersection: return ShaderStage::Intersection;
    case DXBC::ShaderType::AnyHit: return ShaderStage::AnyHit;
    case DXBC::ShaderType::ClosestHit: return ShaderStage::ClosestHit;
    case DXBC::ShaderType::Miss: return ShaderStage::Miss;
    case DXBC::ShaderType::Callable: return ShaderStage::Callable;
    default: RDCERR("Unexpected DXBC shader type %u", type); return ShaderStage::Vertex;
  }
}

rdcstr TypeName(CBufferVariableType desc)
{
  rdcstr ret;

  char *type = "";
  switch(desc.varType)
  {
    case VarType::Bool: type = "bool"; break;
    case VarType::SInt: type = "int"; break;
    case VarType::Float: type = "float"; break;
    case VarType::Double: type = "double"; break;
    case VarType::UInt: type = "uint"; break;
    case VarType::UByte: type = "ubyte"; break;
    case VarType::Unknown: type = "void"; break;
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

CBufferVariableType DXBCContainer::ParseRDEFType(const RDEFHeader *h, const byte *chunkContents,
                                                 uint32_t typeOffset)
{
  if(m_Variables.find(typeOffset) != m_Variables.end())
    return m_Variables[typeOffset];

  const RDEFCBufferType *type = (const RDEFCBufferType *)(chunkContents + typeOffset);

  CBufferVariableType ret;

  ret.varClass = (VariableClass)type->varClass;
  ret.cols = RDCMAX(1U, (uint32_t)type->cols);
  ret.elements = RDCMAX(1U, (uint32_t)type->numElems);
  ret.rows = RDCMAX(1U, (uint32_t)type->rows);

  switch((VariableType)type->varType)
  {
    // DXBC treats all cbuffer variables as 32-bit regardless of declaration
    case DXBC::VARTYPE_MIN12INT:
    case DXBC::VARTYPE_MIN16INT:
    case DXBC::VARTYPE_INT: ret.varType = VarType::SInt; break;
    case DXBC::VARTYPE_BOOL: ret.varType = VarType::Bool; break;
    case DXBC::VARTYPE_MIN16UINT:
    case DXBC::VARTYPE_UINT: ret.varType = VarType::UInt; break;
    case DXBC::VARTYPE_INT64:
    case DXBC::VARTYPE_UINT64:
    case DXBC::VARTYPE_DOUBLE: ret.varType = VarType::Double; break;
    case DXBC::VARTYPE_FLOAT:
    case DXBC::VARTYPE_MIN8FLOAT:
    case DXBC::VARTYPE_MIN10FLOAT:
    case DXBC::VARTYPE_MIN16FLOAT: ret.varType = VarType::Float; break;
    // new types are actually 16-bit, though alignment is still the same as 32-bit
    case DXBC::VARTYPE_INT16: ret.varType = VarType::SShort; break;
    case DXBC::VARTYPE_UINT16: ret.varType = VarType::UShort; break;
    case DXBC::VARTYPE_FLOAT16: ret.varType = VarType::Half; break;
    default: ret.varType = VarType::Float; break;
  }

  ret.name = TypeName(ret);

  if(ret.name == "interface")
  {
    if(h->targetVersion >= 0x500 && type->nameOffset > 0)
    {
      ret.name += " " + rdcstr((const char *)chunkContents + type->nameOffset);
    }
    else
    {
      ret.name += StringFormat::Fmt(" unnamed_iface_0x%08x", typeOffset);
    }
  }

  // rename unnamed structs to have valid identifiers as type name
  if(ret.name.contains("<unnamed>"))
  {
    if(h->targetVersion >= 0x500 && type->nameOffset > 0)
    {
      ret.name = (const char *)chunkContents + type->nameOffset;
    }
    else
    {
      ret.name = StringFormat::Fmt("unnamed_struct_0x%08x", typeOffset);
    }
  }

  if(type->memberOffset)
  {
    const RDEFCBufferChildType *members =
        (const RDEFCBufferChildType *)(chunkContents + type->memberOffset);

    ret.members.reserve(type->numMembers);

    ret.bytesize = 0;

    for(int32_t j = 0; j < type->numMembers; j++)
    {
      CBufferVariable v;

      v.name = (const char *)(chunkContents + members[j].nameOffset);
      v.type = ParseRDEFType(h, chunkContents, members[j].typeOffset);
      v.offset = members[j].memberOffset;

      ret.bytesize = v.offset + v.type.bytesize;

      ret.members.push_back(v);
    }

    ret.bytesize *= RDCMAX(1U, ret.elements);
  }
  else
  {
    // matrices take up a full vector for each column or row depending which is major, regardless of
    // the other dimension
    if(ret.varClass == CLASS_MATRIX_COLUMNS)
    {
      ret.bytesize = VarTypeByteSize(ret.varType) * ret.cols * 4 * RDCMAX(1U, ret.elements);
    }
    else if(ret.varClass == CLASS_MATRIX_ROWS)
    {
      ret.bytesize = VarTypeByteSize(ret.varType) * ret.rows * 4 * RDCMAX(1U, ret.elements);
    }
    else
    {
      // arrays also take up a full vector for each element
      if(ret.elements > 1)
        ret.bytesize = VarTypeByteSize(ret.varType) * 4 * RDCMAX(1U, ret.elements);
      else
        ret.bytesize = VarTypeByteSize(ret.varType) * ret.rows * ret.cols;
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

D3D_PRIMITIVE_TOPOLOGY DXBCContainer::GetOutputTopology(const void *ByteCode, size_t ByteCodeLength)
{
  const FileHeader *header = (const FileHeader *)ByteCode;

  const byte *data = (const byte *)ByteCode;    // just for convenience

  if(ByteCode == NULL || ByteCodeLength == 0)
    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

  if(header->fourcc != FOURCC_DXBC)
    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

  const uint32_t *chunkOffsets = (const uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    const uint32_t *fourcc = (const uint32_t *)(data + chunkOffsets[chunkIdx]);
    const uint32_t *chunkSize = (const uint32_t *)(fourcc + 1);

    const byte *chunkContents = (const byte *)(chunkSize + 1);

    if(*fourcc == FOURCC_SHEX || *fourcc == FOURCC_SHDR)
      return DXBCBytecode::Program::GetOutputTopology(chunkContents, *chunkSize);
  }

  return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

const rdcstr &DXBCContainer::GetDisassembly(bool dxcStyle)
{
  if(m_Disassembly.empty() || (dxcStyle != m_DXCStyle))
  {
    m_DXCStyle = dxcStyle;
    rdcstr globalFlagsString;

    const rdcstr commentString = m_DXBCByteCode ? "//" : ";";

    if(m_GlobalFlags != GlobalShaderFlags::None)
    {
      globalFlagsString += commentString + " Note: shader requires additional functionality:\n";

      if(m_GlobalFlags & GlobalShaderFlags::DoublePrecision)
        globalFlagsString += commentString + "       Double-precision floating point\n";
      if(m_GlobalFlags & GlobalShaderFlags::RawStructured)
        globalFlagsString += commentString + "       Raw and Structured buffers\n";
      if(m_GlobalFlags & GlobalShaderFlags::UAVsEveryStage)
        globalFlagsString += commentString + "       UAVs at every shader stage\n";
      if(m_GlobalFlags & GlobalShaderFlags::UAVCount64)
        globalFlagsString += commentString + "       64 UAV slots\n";
      if(m_GlobalFlags & GlobalShaderFlags::MinPrecision)
        globalFlagsString += commentString + "       Minimum-precision data types\n";
      if(m_GlobalFlags & GlobalShaderFlags::DoubleExtensions11_1)
        globalFlagsString += commentString + "       Double-precision extensions for 11.1\n";
      if(m_GlobalFlags & GlobalShaderFlags::ShaderExtensions11_1)
        globalFlagsString += commentString + "       Shader extensions for 11.1\n";
      if(m_GlobalFlags & GlobalShaderFlags::ComparisonFilter)
        globalFlagsString += commentString + "       Comparison filtering for feature level 9\n";
      if(m_GlobalFlags & GlobalShaderFlags::TiledResources)
        globalFlagsString += commentString + "       Tiled resources\n";
      if(m_GlobalFlags & GlobalShaderFlags::PSOutStencilref)
        globalFlagsString += commentString + "       PS Output Stencil Ref\n";
      if(m_GlobalFlags & GlobalShaderFlags::PSInnerCoverage)
        globalFlagsString += commentString + "       PS Inner Coverage\n";
      if(m_GlobalFlags & GlobalShaderFlags::TypedUAVAdditional)
        globalFlagsString += commentString + "       Typed UAV Load Additional Formats\n";
      if(m_GlobalFlags & GlobalShaderFlags::RasterOrderViews)
        globalFlagsString += commentString + "       Raster Ordered UAVs\n";
      if(m_GlobalFlags & GlobalShaderFlags::ArrayIndexFromVert)
        globalFlagsString += commentString +
                             "       SV_RenderTargetArrayIndex or SV_ViewportArrayIndex from any "
                             "shader feeding rasterizer\n";
      if(m_GlobalFlags & GlobalShaderFlags::WaveOps)
        globalFlagsString += commentString + "       Wave level operations\n";
      if(m_GlobalFlags & GlobalShaderFlags::Int64)
        globalFlagsString += commentString + "       64-Bit integer\n";
      if(m_GlobalFlags & GlobalShaderFlags::ViewInstancing)
        globalFlagsString += commentString + "       View Instancing\n";
      if(m_GlobalFlags & GlobalShaderFlags::Barycentrics)
        globalFlagsString += commentString + "       Barycentrics\n";
      if(m_GlobalFlags & GlobalShaderFlags::NativeLowPrecision)
        globalFlagsString += commentString + "       Use native low precision\n";
      if(m_GlobalFlags & GlobalShaderFlags::ShadingRate)
        globalFlagsString += commentString + "       Shading Rate\n";
      if(m_GlobalFlags & GlobalShaderFlags::Raytracing1_1)
        globalFlagsString += commentString + "       Raytracing tier 1.1 features\n";
      if(m_GlobalFlags & GlobalShaderFlags::SamplerFeedback)
        globalFlagsString += commentString + "       Sampler feedback\n";
      globalFlagsString += commentString + "\n";
    }

    if(m_DXBCByteCode)
    {
      m_Disassembly = StringFormat::Fmt("Shader hash %08x-%08x-%08x-%08x\n\n", m_Hash[0], m_Hash[1],
                                        m_Hash[2], m_Hash[3]);

      if(m_GlobalFlags != GlobalShaderFlags::None)
        m_Disassembly += globalFlagsString;

      if(!m_DebugFileName.empty())
        m_Disassembly += StringFormat::Fmt("// Debug name: %s\n", m_DebugFileName.c_str());

      if(m_ShaderExt.second != ~0U)
        m_Disassembly += "// Vendor shader extensions in use\n";

      m_Disassembly += m_DXBCByteCode->GetDisassembly();
    }
    else if(m_DXILByteCode)
    {
      m_Disassembly.clear();

#if DISABLED(DXC_COMPATIBLE_DISASM)
      if(m_GlobalFlags != GlobalShaderFlags::None)
        m_Disassembly += globalFlagsString;

      if(!m_DebugFileName.empty())
        m_Disassembly += StringFormat::Fmt("; shader debug name: %s\n", m_DebugFileName.c_str());

      if(m_ShaderExt.second != ~0U)
        m_Disassembly += "; Vendor shader extensions in use\n";

      m_Disassembly += "; shader hash: ";
      byte *hashBytes = (byte *)m_Hash;
      for(size_t i = 0; i < sizeof(m_Hash); i++)
        m_Disassembly += StringFormat::Fmt("%02x", hashBytes[i]);
      m_Disassembly += "\n\n";
#endif

      m_Disassembly += m_DXILByteCode->GetDisassembly(dxcStyle, m_Reflection);
    }
  }

  return m_Disassembly;
}

void DXBCContainer::FillTraceLineInfo(ShaderDebugTrace &trace) const
{
  // we add some number of lines for the header we added with shader hash, debug name, etc on
  // top of what the bytecode disassembler did

  // 2 minimum for the shader hash we always print
  uint32_t extraLines = 2;
  if(!m_DebugFileName.empty())
    extraLines++;
  if(m_ShaderExt.second != ~0U)
    extraLines++;

  if(m_GlobalFlags != GlobalShaderFlags::None)
    extraLines += (uint32_t)Bits::CountOnes((uint32_t)m_GlobalFlags) + 2;

  if(m_DXBCByteCode)
  {
    trace.instInfo.resize(m_DXBCByteCode->GetNumInstructions());
    for(size_t i = 0; i < m_DXBCByteCode->GetNumInstructions(); i++)
    {
      const DXBCBytecode::Operation &op = m_DXBCByteCode->GetInstruction(i);

      trace.instInfo[i].instruction = (uint32_t)i;

      if(m_DebugInfo)
        m_DebugInfo->GetLineInfo(i, op.offset, trace.instInfo[i].lineInfo);

      if(op.line > 0)
        trace.instInfo[i].lineInfo.disassemblyLine = extraLines + op.line;

      if(m_DebugInfo)
        m_DebugInfo->GetLocals(this, i, op.offset, trace.instInfo[i].sourceVars);
    }
  }
  else if(m_DXILByteCode)
  {
#if ENABLED(DXC_COMPATIBLE_DISASM)
    extraLines = 0;
#endif
    size_t instrCount = m_DXILByteCode->GetInstructionCount();
    trace.instInfo.resize(instrCount);
    for(size_t i = 0; i < instrCount; i++)
    {
      trace.instInfo[i].instruction = (uint32_t)i;

      if(m_DebugInfo)
        m_DebugInfo->GetLineInfo(i, 0, trace.instInfo[i].lineInfo);
      else
        m_DXILByteCode->GetLineInfo(i, 0, trace.instInfo[i].lineInfo);

      trace.instInfo[i].lineInfo.disassemblyLine += extraLines;

      if(m_DebugInfo)
        m_DebugInfo->GetLocals(this, i, 0, trace.instInfo[i].sourceVars);
    }
  }
}

void DXBCContainer::StripChunk(bytebuf &ByteCode, uint32_t fourcc)
{
  FileHeader *header = (FileHeader *)ByteCode.data();

  if(header->fourcc != FOURCC_DXBC)
    return;

  if(header->fileLength != (uint32_t)ByteCode.size())
    return;

  uint32_t *chunkOffsets =
      (uint32_t *)(ByteCode.data() + sizeof(FileHeader));    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t offs = chunkOffsets[chunkIdx];

    uint32_t *chunkFourcc = (uint32_t *)(ByteCode.data() + offs);
    uint32_t *chunkSize = (uint32_t *)(chunkFourcc + 1);

    if(*chunkFourcc == fourcc)
    {
      // the size of the whole chunk that we're erasing is the chunk's size itself, plus 8 bytes for
      // fourcc+size
      uint32_t size = 8 + *chunkSize;
      for(uint32_t c = chunkIdx; c < header->numChunks; c++)
        chunkOffsets[c] = chunkOffsets[c + 1] - size;

      header->numChunks--;
      header->fileLength -= size;

      // all chunk offsets (before and after) and file size decrement by a uint32, because we're
      // going to remove a chunkoffset as well which is before them all
      for(uint32_t c = 0; c < header->numChunks; c++)
        chunkOffsets[c] -= sizeof(uint32_t);
      header->fileLength -= sizeof(uint32_t);

      // erase the chunk itself
      ByteCode.erase(offs, size);
      // remove the chunk offset
      ByteCode.erase(sizeof(FileHeader) + header->numChunks * sizeof(uint32_t), 4);

      break;
    }
  }

  HashContainer(ByteCode.data(), ByteCode.size());
}

void DXBCContainer::ReplaceChunk(bytebuf &ByteCode, uint32_t fourcc, const byte *replacement,
                                 size_t size)
{
  FileHeader *header = (FileHeader *)ByteCode.data();

  if(header->fourcc != FOURCC_DXBC)
    return;

  if(header->fileLength != (uint32_t)ByteCode.size())
    return;

  uint32_t *chunkOffsets =
      (uint32_t *)(ByteCode.data() + sizeof(FileHeader));    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t offs = chunkOffsets[chunkIdx];

    uint32_t *chunkFourcc = (uint32_t *)(ByteCode.data() + offs);
    uint32_t *chunkSize = (uint32_t *)(chunkFourcc + 1);

    if(*chunkFourcc == fourcc)
    {
      int32_t diff = int32_t(size) - int32_t(*chunkSize);

      *chunkSize = (uint32_t)size;

      if(diff == 0)
      {
        memcpy(ByteCode.data() + offs + 8, replacement, size);
      }
      else if(diff > 0)
      {
        const byte *replaceBytes = (const byte *)replacement;
        ByteCode.insert(offs + 8, replaceBytes, diff);
        memcpy(ByteCode.data() + offs + 8 + diff, replaceBytes + diff, size - diff);
      }
      else if(diff < 0)
      {
        ByteCode.erase(offs + 8, -diff);
        memcpy(ByteCode.data() + offs + 8, replacement, size);
      }

      // fixup offsets of chunks after this point
      header = (FileHeader *)ByteCode.data();
      chunkOffsets = (uint32_t *)(ByteCode.data() + sizeof(FileHeader));

      header->fileLength += diff;

      chunkIdx++;

      for(; chunkIdx < header->numChunks; chunkIdx++)
        chunkOffsets[chunkIdx] += diff;

      HashContainer(ByteCode.data(), ByteCode.size());
      return;
    }
  }

  uint32_t newOffs = uint32_t(ByteCode.size() + sizeof(uint32_t));
  ByteCode.insert(sizeof(FileHeader) + header->numChunks * sizeof(uint32_t), (byte *)&newOffs,
                  sizeof(newOffs));

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
    chunkOffsets[chunkIdx] += sizeof(uint32_t);

  ByteCode.append((byte *)&fourcc, sizeof(fourcc));
  uint32_t chunkSize = (uint32_t)size;
  ByteCode.append((byte *)&chunkSize, sizeof(chunkSize));
  ByteCode.append(replacement, size);

  header->numChunks++;
  header->fileLength = (uint32_t)ByteCode.size();

  HashContainer(ByteCode.data(), ByteCode.size());
}

const byte *DXBCContainer::FindChunk(const bytebuf &ByteCode, uint32_t fourcc, size_t &size)
{
  const FileHeader *header = (const FileHeader *)ByteCode.data();

  size = 0;

  if(header->fourcc != FOURCC_DXBC)
    return NULL;

  if(header->fileLength != (uint32_t)ByteCode.size())
    return NULL;

  const uint32_t *chunkOffsets =
      (const uint32_t *)(ByteCode.data() + sizeof(FileHeader));    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t offs = chunkOffsets[chunkIdx];

    const uint32_t *chunkFourcc = (uint32_t *)(ByteCode.data() + offs);
    const uint32_t *chunkSize = (uint32_t *)(chunkFourcc + 1);

    if(*chunkFourcc == fourcc)
    {
      size = *chunkSize;
      return ByteCode.data() + offs + 8;
    }
  }

  return NULL;
}

void DXBCContainer::GetHash(uint32_t hash[4], const void *ByteCode, size_t BytecodeLength)
{
  if(BytecodeLength < sizeof(FileHeader) || ByteCode == NULL)
  {
    memset(hash, 0, sizeof(uint32_t) * 4);
    return;
  }

  const byte *data = (byte *)ByteCode;    // just for convenience

  FileHeader *header = (FileHeader *)ByteCode;

  memset(hash, 0, sizeof(uint32_t) * 4);

  if(header->fourcc != FOURCC_DXBC)
    return;

  if(header->fileLength != (uint32_t)BytecodeLength)
    return;

  memcpy(hash, header->hashValue, sizeof(header->hashValue));

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
    uint32_t *chunkSize = (uint32_t *)(fourcc + 1);

    char *chunkContents = (char *)(chunkSize + 1);

    if(*fourcc == FOURCC_HASH)
    {
      HASHHeader *hashHeader = (HASHHeader *)chunkContents;

      memcpy(hash, hashHeader->hashValue, sizeof(hashHeader->hashValue));
    }
  }
}

bool DXBCContainer::IsHashedContainer(const void *ByteCode, size_t BytecodeLength)
{
  if(BytecodeLength < sizeof(FileHeader) || ByteCode == NULL)
    return false;

  const FileHeader *header = (const FileHeader *)ByteCode;

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)BytecodeLength)
    return false;

  if(header->hashValue[0] != 0 || header->hashValue[1] != 0 || header->hashValue[2] != 0 ||
     header->hashValue[3] != 0)
    return true;

  return false;
}

bool DXBCContainer::HashContainer(void *ByteCode, size_t BytecodeLength)
{
  if(BytecodeLength < sizeof(FileHeader) || ByteCode == NULL)
    return false;

  FileHeader *header = (FileHeader *)ByteCode;

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)BytecodeLength)
    return false;

  MD5_CTX md5ctx = {};
  MD5_Init(&md5ctx);

  // the hashable data starts immediately after the hash.
  byte *data = (byte *)&header->containerVersion;
  uint32_t length = uint32_t(BytecodeLength - offsetof(FileHeader, containerVersion));

  // we need to know the number of bits for putting in the trailing padding.
  uint32_t numBits = length * 8;
  uint32_t numBitsPart2 = (numBits >> 2) | 1;

  // MD5 works on 64-byte chunks, process the first set of whole chunks, leaving 0-63 bytes left
  // over
  uint32_t leftoverLength = length % 64;
  MD5_Update(&md5ctx, data, length - leftoverLength);

  data += length - leftoverLength;

  uint32_t block[16] = {};
  RDCCOMPILE_ASSERT(sizeof(block) == 64, "Block is not properly sized for MD5 round");

  // normally MD5 finishes by appending a 1 bit to the bitstring. Since we are only appending bytes
  // this would be an 0x80 byte (the first bit is considered to be the MSB). Then it pads out with
  // zeroes until it has 56 bytes in the last block and appends appends the message length as a
  // 64-bit integer as the final part of that block.
  // in other words, normally whatever is leftover from the actual message gets one byte appended,
  // then if there's at least 8 bytes left we'll append the length. Otherwise we pad that block with
  // 0s and create a new block with the length at the end.
  // Or as the original RFC/spec says: padding is always performed regardless of whether the
  // original buffer already ended in exactly a 56 byte block.
  //
  // The DXBC finalisation is slightly different (previous work suggests this is due to a bug in the
  // original implementation and it was maybe intended to be exactly MD5?):
  //
  // The length provided in the padding block is not 64-bit properly: the second dword with the high
  // bits is instead the number of nybbles(?) with 1 OR'd on. The length is also split, so if it's
  // in
  // a padding block the low bits are in the first dword and the upper bits in the last. If there's
  // no padding block the low dword is passed in first before the leftovers of the message and then
  // the upper bits at the end.

  // if the leftovers uses at least 56, we can't fit both the trailing 1 and the 64-bit length, so
  // we need a padding block and then our own block for the length.
  if(leftoverLength >= 56)
  {
    // pass in the leftover data padded out to 64 bytes with zeroes
    MD5_Update(&md5ctx, data, leftoverLength);

    block[0] = 0x80;    // first padding bit is 1
    MD5_Update(&md5ctx, block, 64 - leftoverLength);

    // the final block contains the number of bits in the first dword, and the weird upper bits
    block[0] = numBits;
    block[15] = numBitsPart2;

    // process this block directly, we're replacing the call to MD5_Final here manually
    MD5_Update(&md5ctx, block, 64);
  }
  else
  {
    // the leftovers mean we can put the padding inside the final block. But first we pass the "low"
    // number of bits:
    MD5_Update(&md5ctx, &numBits, sizeof(numBits));

    if(leftoverLength)
      MD5_Update(&md5ctx, data, leftoverLength);

    uint32_t paddingBytes = 64 - leftoverLength - 4;

    // prepare the remainder of this block, starting with the 0x80 padding start right after the
    // leftovers and the first part of the bit length above.
    block[0] = 0x80;
    // then add the remainder of the 'length' here in the final part of the block
    memcpy(((byte *)block) + paddingBytes - 4, &numBitsPart2, 4);

    MD5_Update(&md5ctx, block, paddingBytes);
  }

  header->hashValue[0] = md5ctx.a;
  header->hashValue[1] = md5ctx.b;
  header->hashValue[2] = md5ctx.c;
  header->hashValue[3] = md5ctx.d;

  return true;
}

bool DXBCContainer::UsesExtensionUAV(uint32_t slot, uint32_t space, const void *ByteCode,
                                     size_t BytecodeLength)
{
  if(slot == ~0U && space == ~0U)
    return false;

  const FileHeader *header = (const FileHeader *)ByteCode;

  const byte *data = (const byte *)ByteCode;    // just for convenience

  if(ByteCode == NULL || BytecodeLength == 0)
    return false;

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)BytecodeLength)
    return false;

  const uint32_t *chunkOffsets = (const uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    const uint32_t *fourcc = (const uint32_t *)(data + chunkOffsets[chunkIdx]);
    const uint32_t *chunkSize = (const uint32_t *)(fourcc + 1);

    const byte *chunkContents = (const byte *)(chunkSize + 1);

    if(*fourcc == FOURCC_SHEX || *fourcc == FOURCC_SHDR)
      return DXBCBytecode::Program::UsesExtensionUAV(slot, space, chunkContents, *chunkSize);

    // far too expensive to figure out if a DXIL blob references the shader UAV. Just assume it does
    // - this is only as an opportunistic thing to avoid requiring vendor extensions on programs
    // that initialise but don't use them. If a user is bothering with DXIL they deserve what they
    // get.
    if(*fourcc == FOURCC_DXIL || *fourcc == FOURCC_ILDB)
      return true;
  }

  return false;
}

bool DXBCContainer::CheckForDebugInfo(const void *ByteCode, size_t ByteCodeLength)
{
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(ByteCode == NULL || ByteCodeLength == 0)
    return false;

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return false;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_SDBG || *fourcc == FOURCC_SPDB || *fourcc == FOURCC_ILDB)
      return true;
  }

  return false;
}

bool DXBCContainer::CheckForDXIL(const void *ByteCode, size_t ByteCodeLength)
{
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(ByteCode == NULL || ByteCodeLength == 0)
    return false;

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return false;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_ILDB || *fourcc == FOURCC_DXIL)
      return true;
  }

  return false;
}

bool DXBCContainer::CheckForRootSig(const void *ByteCode, size_t ByteCodeLength)
{
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(ByteCode == NULL || ByteCodeLength == 0)
    return false;

  if(header->fourcc != FOURCC_DXBC)
    return false;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return false;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_RTS0)
      return true;
  }

  return false;
}

rdcstr DXBCContainer::GetDebugBinaryPath(const void *ByteCode, size_t ByteCodeLength)
{
  rdcstr debugPath;
  FileHeader *header = (FileHeader *)ByteCode;

  char *data = (char *)ByteCode;    // just for convenience

  if(ByteCode == NULL || ByteCodeLength == 0)
    return debugPath;

  if(header->fourcc != FOURCC_DXBC)
    return debugPath;

  if(header->fileLength != (uint32_t)ByteCodeLength)
    return debugPath;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header

  // prefer RenderDoc's magic value which pre-dated D3D's support
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

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
    if(*fourcc == FOURCC_ILDN)
    {
      const ILDNHeader *h = (const ILDNHeader *)(fourcc + 2);

      debugPath.append(h->Name, h->NameLength);
      return debugPath;
    }
  }

  return debugPath;
}

void DXBCContainer::TryFetchSeparateDebugInfo(bytebuf &byteCode, const rdcstr &debugInfoPath)
{
  if(!CheckForDebugInfo((const void *)&byteCode[0], byteCode.size()))
  {
    rdcstr originalPath = debugInfoPath;

    if(originalPath.empty())
      originalPath = GetDebugBinaryPath((const void *)&byteCode[0], byteCode.size());

    if(!originalPath.empty())
    {
      bool lz4 = false;

      if(!strncmp(originalPath.c_str(), "lz4#", 4))
      {
        originalPath = originalPath.substr(4);
        lz4 = true;
      }
      // could support more if we're willing to compile in the decompressor

      FILE *originalShaderFile = NULL;

      const rdcarray<rdcstr> &searchPaths = DXBC_Debug_SearchDirPaths();

      size_t numSearchPaths = searchPaths.size();

      rdcstr foundPath;

      // keep searching until we've exhausted all possible path options, or we've found a file that
      // opens
      while(originalShaderFile == NULL && !originalPath.empty())
      {
        // while we haven't found a file, keep trying through the search paths. For i==0
        // check the path on its own, in case it's an absolute path.
        for(size_t i = 0; originalShaderFile == NULL && i <= numSearchPaths; i++)
        {
          if(i == 0)
          {
            originalShaderFile = FileIO::fopen(originalPath, FileIO::ReadBinary);
            foundPath = originalPath;
            continue;
          }
          else
          {
            const rdcstr &searchPath = searchPaths[i - 1];
            foundPath = searchPath + "/" + originalPath;
            originalShaderFile = FileIO::fopen(foundPath, FileIO::ReadBinary);
          }
        }

        if(originalShaderFile == NULL)
        {
          // the "documented" behaviour for D3D debug info names is that when presented with a
          // relative path containing subfolders like foo/bar/blah.pdb then we should first try to
          // append it to all search paths as-is, then strip off the top-level subdirectory to get
          // bar/blah.pdb and try that in all search directories, and keep going. So if we got here
          // and didn't open a file, try to strip off the the top directory and continue.
          int32_t offs = originalPath.find_first_of("\\/");

          // if we couldn't find a directory separator there's nothing to do, stop looking
          if(offs == -1)
            break;

          // otherwise strip up to there and keep going
          originalPath.erase(0, offs + 1);
        }
      }

      if(originalShaderFile == NULL)
        return;

      FileIO::fseek64(originalShaderFile, 0L, SEEK_END);
      uint64_t originalShaderSize = FileIO::ftell64(originalShaderFile);
      FileIO::fseek64(originalShaderFile, 0, SEEK_SET);

      if(lz4 || originalShaderSize >= byteCode.size())
      {
        bytebuf debugBytecode;

        debugBytecode.resize((size_t)originalShaderSize);
        FileIO::fread(&debugBytecode[0], sizeof(byte), (size_t)originalShaderSize,
                      originalShaderFile);

        if(lz4)
        {
          rdcarray<byte> decompressed;

          // first try decompressing to 1MB flat
          decompressed.resize(100 * 1024);

          int ret = LZ4_decompress_safe((const char *)&debugBytecode[0], (char *)&decompressed[0],
                                        (int)debugBytecode.size(), (int)decompressed.size());

          if(ret < 0)
          {
            // if it failed, either source is corrupt or we didn't allocate enough space.
            // Just allocate 255x compressed size since it can't need any more than that.
            decompressed.resize(255 * debugBytecode.size());

            ret = LZ4_decompress_safe((const char *)&debugBytecode[0], (char *)&decompressed[0],
                                      (int)debugBytecode.size(), (int)decompressed.size());

            if(ret < 0)
            {
              RDCERR("Failed to decompress LZ4 data from %s", foundPath.c_str());
              return;
            }
          }

          RDCASSERT(ret > 0, ret);

          // we resize and memcpy instead of just doing .swap() because that would
          // transfer over the over-large pessimistic capacity needed for decompression
          debugBytecode.resize(ret);
          memcpy(&debugBytecode[0], &decompressed[0], debugBytecode.size());
        }

        if(IsPDBFile(&debugBytecode[0], debugBytecode.size()))
        {
          UnwrapEmbeddedPDBData(debugBytecode);
          m_DebugShaderBlob = debugBytecode;
        }
        else if(CheckForDebugInfo((const void *)&debugBytecode[0], debugBytecode.size()))
        {
          byteCode.swap(debugBytecode);
        }
      }

      FileIO::fclose(originalShaderFile);
    }
  }
}

DXBCContainer::DXBCContainer(const bytebuf &ByteCode, const rdcstr &debugInfoPath, GraphicsAPI api,
                             uint32_t shaderExtReg, uint32_t shaderExtSpace)
{
  RDCEraseEl(m_ShaderStats);

  m_ShaderBlob = ByteCode;

  TryFetchSeparateDebugInfo(m_ShaderBlob, debugInfoPath);

  // just for convenience
  byte *data = (byte *)m_ShaderBlob.data();
  byte *debugData = (byte *)m_DebugShaderBlob.data();

  FileHeader *header = (FileHeader *)data;
  FileHeader *debugHeader = (FileHeader *)debugData;

  if(header->fourcc != FOURCC_DXBC)
    return;

  if(header->fileLength != (uint32_t)m_ShaderBlob.size())
    return;

  if(debugHeader && debugHeader->fourcc != FOURCC_DXBC)
    debugHeader = NULL;

  if(debugHeader && debugHeader->fileLength != m_DebugShaderBlob.size())
    debugHeader = NULL;

  memcpy(m_Hash, header->hashValue, sizeof(m_Hash));

  // default to vertex shader to support blobs without RDEF chunks (e.g. used with
  // input layouts if they're super stripped down)
  m_Type = DXBC::ShaderType::Vertex;

  uint32_t *chunkOffsets = (uint32_t *)(header + 1);    // right after the header
  uint32_t *debugChunkOffsets = debugHeader ? (uint32_t *)(debugHeader + 1) : NULL;

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    const uint32_t *fourcc = (const uint32_t *)(data + chunkOffsets[chunkIdx]);
    const uint32_t *chunkSize = (const uint32_t *)(fourcc + 1);

    const byte *chunkContents = (const byte *)(chunkSize + 1);

    if(*fourcc == FOURCC_RDEF)
    {
      if(*chunkSize < offsetof(RDEFHeader, unknown))
      {
        RDCERR("Invalid RDEF chunk encountered: size %u", *chunkSize);
        continue;
      }

      const RDEFHeader *h = (const RDEFHeader *)chunkContents;

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

        desc.name = (const char *)(chunkContents + res->nameOffset);
        desc.type = (ShaderInputBind::InputType)res->type;
        desc.space = h->targetVersion >= 0x501 ? res->space : 0;
        desc.reg = res->bindPoint;
        desc.bindCount = res->bindCount;
        desc.retType = (DXBC::ResourceRetType)res->retType;
        desc.dimension = (ShaderInputBind::Dimension)res->dimension;

        // Bindless resources report a bind count of 0 from the shader bytecode, but many other
        // places in this codebase assume ~0U means bindless. Patch it up now.
        if(h->targetVersion >= 0x501 && desc.bindCount == 0)
          desc.bindCount = ~0U;

        // component count seem to be in these lower bits of flags.
        desc.numComps = 1 + ((res->flags & 0xC) >> 2);

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

        cb.name = (const char *)(chunkContents + cbuf->nameOffset);

        cb.descriptor.byteSize = cbuf->size;
        cb.descriptor.type = (CBuffer::Descriptor::Type)cbuf->type;

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

            if(var->nameOffset > m_ShaderBlob.size())
            {
              varStride += extraData;
            }
          }
        }

        for(int32_t vi = 0; vi < cbuf->variables.count; vi++)
        {
          RDEFCBufferVariable *var =
              (RDEFCBufferVariable *)(chunkContents + cbuf->variables.offset + vi * varStride);

          RDCASSERT(var->nameOffset < m_ShaderBlob.size());

          CBufferVariable v;

          v.name = (const char *)(chunkContents + var->nameOffset);

          // var->size; // size with cbuffer padding
          v.offset = var->startOffset;

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
                   (const char *)(chunkContents + cbuf->nameOffset));
        }
      }
    }
    else if(*fourcc == FOURCC_STAT)
    {
      if(DXIL::Program::Valid(chunkContents, *chunkSize))
      {
        RDCEraseEl(m_ShaderStats);
        m_ShaderStats.version = ShaderStatistics::STATS_DX12;

        // this stats chunk is a whole program, just with the actual function definition removed
        // (and any related debug metadata). We have to handle this later with the bytecode.
        /* DXIL::Program prog(chunkContents, *chunkSize); */
      }
      else if(*chunkSize == STATSizeDX10)
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
      m_DXBCByteCode = new DXBCBytecode::Program(chunkContents, *chunkSize);
    }
    else if(*fourcc == FOURCC_SPDB || *fourcc == FOURCC_SDBG)
    {
      // debug info is processed afterwards
    }
    else if(*fourcc == FOURCC_ILDB || *fourcc == FOURCC_DXIL)
    {
      // we avoiding parsing these immediately because you can get both in a dxbc, so we prefer the
      // debug version.
      // we do remember where the non-debug DXIL is though so we can return it for editing (we only
      // edit non-debug DXIL)

      if(*fourcc == FOURCC_DXIL)
      {
        m_NonDebugDXILByteCodeOffset = chunkContents - data;
        m_NonDebugDXILByteCodeSize = *chunkSize;
      }
    }
    else if(*fourcc == FOURCC_ILDN)
    {
      if(*chunkSize < sizeof(ILDNHeader))
      {
        RDCERR("Invalid ILDN chunk encountered: size %u", *chunkSize);
        continue;
      }

      const ILDNHeader *h = (const ILDNHeader *)chunkContents;

      m_DebugFileName = rdcstr(h->Name, h->NameLength);
    }
    else if(*fourcc == FOURCC_HASH)
    {
      if(*chunkSize < sizeof(HASHHeader))
      {
        RDCERR("Invalid HASH chunk encountered: size %u", *chunkSize);
        continue;
      }

      const HASHHeader *h = (const HASHHeader *)chunkContents;

      memcpy(m_Hash, h->hashValue, sizeof(h->hashValue));
    }
    else if(*fourcc == FOURCC_SFI0)
    {
      if(*chunkSize < sizeof(GlobalShaderFlags))
      {
        RDCERR("Invalid SFI0 chunk encountered: size %u", *chunkSize);
        continue;
      }

      m_GlobalFlags = *(const GlobalShaderFlags *)chunkContents;
    }
    else if(*fourcc == FOURCC_RTS0)
    {
      // root signature
    }
    else if(*fourcc == FOURCC_RDAT)
    {
      // runtime data
    }
    else if(*fourcc == FOURCC_PSV0)
    {
      // this chunk contains some information we could use for reflection but it doesn't contain
      // enough, and doesn't have anything else interesting so we skip it
    }
    else if(*fourcc == FOURCC_ISGN || *fourcc == FOURCC_OSGN || *fourcc == FOURCC_ISG1 ||
            *fourcc == FOURCC_OSG1 || *fourcc == FOURCC_OSG5 || *fourcc == FOURCC_PCSG ||
            *fourcc == FOURCC_PSG1)
    {
      // processed later
    }
    else
    {
      RDCWARN("Unknown chunk %c%c%c%c", ((const char *)fourcc)[0], ((const char *)fourcc)[1],
              ((const char *)fourcc)[2], ((const char *)fourcc)[3]);
    }
  }

  // DXIL can have three(!) different programs in different chunks.
  // ILDB is the best, it contains everything
  // STAT is better for reflection only
  // DXIL is the executable code and most stripped version
  //
  // Since decoding DXIL is expensive we want to do it as few times as possible. If we can get ILDB
  // we do and don't get anything else. Otherwise we grab both STAT (for reflection) and DXIL (for disassembly)

  // only one of these will be allocated
  DXIL::Program *dxilILDBProgram = NULL;
  DXIL::Program *dxilDXILProgram = NULL;

  // this will be allocated if DXIL is, and will be deleted below after reflection is fetched from it
  DXIL::Program *dxilSTATProgram = NULL;

  DXIL::Program *dxilReflectProgram = NULL;

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
        dxilILDBProgram = new DXIL::Program((const byte *)chunkContents, *chunkSize);
      }
    }

    // next search the debug file if it exists
    for(uint32_t chunkIdx = 0;
        debugHeader && dxilILDBProgram == NULL && chunkIdx < debugHeader->numChunks; chunkIdx++)
    {
      uint32_t *fourcc = (uint32_t *)(debugData + debugChunkOffsets[chunkIdx]);
      uint32_t *chunkSize = (uint32_t *)(debugData + debugChunkOffsets[chunkIdx] + sizeof(uint32_t));

      char *chunkContents = (char *)(debugData + debugChunkOffsets[chunkIdx] + sizeof(uint32_t) * 2);

      if(*fourcc == FOURCC_ILDB)
      {
        dxilILDBProgram = new DXIL::Program((const byte *)chunkContents, *chunkSize);
      }
    }

    // if we didn't find ILDB then we have to get the bytecode from DXIL. However we look for the
    // STAT chunk and if we find it get reflection from there, since it will have better
    // information. What a mess.
    if(dxilILDBProgram == NULL)
    {
      for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
      {
        uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
        uint32_t *chunkSize = (uint32_t *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t));

        const byte *chunkContents =
            (const byte *)(data + chunkOffsets[chunkIdx] + sizeof(uint32_t) * 2);

        if(*fourcc == FOURCC_DXIL)
        {
          dxilDXILProgram = new DXIL::Program(chunkContents, *chunkSize);
        }
        else if(*fourcc == FOURCC_STAT)
        {
          dxilSTATProgram = new DXIL::Program((const byte *)chunkContents, *chunkSize);
        }
      }

      // if there's a debug file we'd have expected to find an ILDB but just in case look for a STAT
      // if we didn't get it
      for(uint32_t chunkIdx = 0;
          debugHeader && dxilSTATProgram == NULL && chunkIdx < debugHeader->numChunks; chunkIdx++)
      {
        uint32_t *fourcc = (uint32_t *)(debugData + debugChunkOffsets[chunkIdx]);
        uint32_t *chunkSize =
            (uint32_t *)(debugData + debugChunkOffsets[chunkIdx] + sizeof(uint32_t));

        char *chunkContents =
            (char *)(debugData + debugChunkOffsets[chunkIdx] + sizeof(uint32_t) * 2);

        if(*fourcc == FOURCC_STAT)
        {
          dxilSTATProgram = new DXIL::Program((const byte *)chunkContents, *chunkSize);
        }
      }
    }

    // if we got the full debug program we don't need the stat program
    if(dxilILDBProgram)
    {
      SAFE_DELETE(dxilSTATProgram);
      SAFE_DELETE(dxilDXILProgram);
      dxilReflectProgram = m_DXILByteCode = dxilILDBProgram;
    }
    else if(dxilDXILProgram)
    {
      // prefer STAT for reflection, but otherwise use DXIL
      dxilReflectProgram = dxilSTATProgram ? dxilSTATProgram : dxilDXILProgram;
      m_DXILByteCode = dxilDXILProgram;
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
  }

  // if reflection information was stripped (or never emitted with DXIL), attempt to reverse
  // engineer basic info from declarations or read it from the DXIL
  if(m_Reflection == NULL)
  {
    // need to disassemble now to guess resources
    if(m_DXBCByteCode)
      m_Reflection = m_DXBCByteCode->GuessReflection();
    else if(dxilReflectProgram)
      m_Reflection = dxilReflectProgram->GetReflection();
    else
      m_Reflection = new Reflection;
  }

  if(dxilReflectProgram)
  {
    m_EntryPoints = dxilReflectProgram->GetEntryPoints();
  }
  else if(m_EntryPoints.empty())
  {
    rdcstr entry;
    if(GetDebugInfo())
      entry = GetDebugInfo()->GetEntryFunction();
    if(entry.empty())
      entry = "main";
    m_EntryPoints = {ShaderEntryPoint(entry, GetShaderStage(m_Type))};
  }

  SAFE_DELETE(dxilSTATProgram);

  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);
    uint32_t *chunkSize = (uint32_t *)(fourcc + 1);

    char *chunkContents = (char *)(fourcc + 2);

    if(*fourcc == FOURCC_ISGN || *fourcc == FOURCC_OSGN || *fourcc == FOURCC_ISG1 ||
       *fourcc == FOURCC_OSG1 || *fourcc == FOURCC_OSG5 || *fourcc == FOURCC_PCSG ||
       *fourcc == FOURCC_PSG1)
    {
      if(*chunkSize < sizeof(SIGNHeader))
      {
        RDCERR("Invalid SIGN chunk encountered: size %u", *chunkSize);
        continue;
      }

      SIGNHeader *sign = (SIGNHeader *)chunkContents;

      rdcarray<SigParameter> *sig = NULL;

      bool input = false;
      bool output = false;
      bool patchOrPerPrim = false;

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
      if(*fourcc == FOURCC_PCSG || *fourcc == FOURCC_PSG1)
      {
        sig = &m_Reflection->PatchConstantSig;

        // for mesh shaders put everything in the output signature
        if(m_Type == DXBC::ShaderType::Mesh)
          sig = &m_Reflection->OutputSig;

        patchOrPerPrim = true;
      }

      RDCASSERT(sig && (sig->empty() || m_Type == DXBC::ShaderType::Mesh));

      SIGNElement *el0 = (SIGNElement *)(sign + 1);
      SIGNElement7 *el7 = (SIGNElement7 *)el0;
      SIGNElement1 *el1 = (SIGNElement1 *)el0;

      for(uint32_t signIdx = 0; signIdx < sign->numElems; signIdx++)
      {
        SigParameter desc;

        const SIGNElement *el = el0;

        if(*fourcc == FOURCC_ISG1 || *fourcc == FOURCC_OSG1 || *fourcc == FOURCC_PSG1)
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

        SigCompType compType = (SigCompType)el->componentType;
        desc.varType = VarType::Float;
        if(compType == COMPONENT_TYPE_UINT32)
          desc.varType = VarType::UInt;
        else if(compType == COMPONENT_TYPE_SINT32)
          desc.varType = VarType::SInt;
        else if(compType == COMPONENT_TYPE_FLOAT32)
          desc.varType = VarType::Float;
        else if(compType == COMPONENT_TYPE_UINT16)
          desc.varType = VarType::UShort;
        else if(compType == COMPONENT_TYPE_SINT16)
          desc.varType = VarType::SShort;
        else if(compType == COMPONENT_TYPE_FLOAT16)
          desc.varType = VarType::Half;
        else if(compType == COMPONENT_TYPE_UINT64)
          desc.varType = VarType::ULong;
        else if(compType == COMPONENT_TYPE_SINT64)
          desc.varType = VarType::SLong;
        else if(compType == COMPONENT_TYPE_FLOAT64)
          desc.varType = VarType::Double;

        desc.regChannelMask = (uint8_t)(el->mask & 0xff);
        desc.channelUsedMask = (uint8_t)(el->rwMask & 0xff);
        desc.regIndex = el->registerNum;
        desc.semanticIndex = (uint16_t)el->semanticIdx;
        desc.semanticName = chunkContents + el->nameOffset;
        desc.systemValue = GetSystemValue(el->systemType);
        desc.compCount = (desc.regChannelMask & 0x1 ? 1 : 0) + (desc.regChannelMask & 0x2 ? 1 : 0) +
                         (desc.regChannelMask & 0x4 ? 1 : 0) + (desc.regChannelMask & 0x8 ? 1 : 0);

        // this is the per-primitive signature for mesh shaders
        if(m_Type == DXBC::ShaderType::Mesh && patchOrPerPrim)
          desc.perPrimitiveRate = true;

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
          if(!_stricmp(desc.semanticName.c_str(), "SV_Barycentrics"))
            desc.systemValue = ShaderBuiltin::Barycentrics;
          if(!_stricmp(desc.semanticName.c_str(), "SV_ShadingRate"))
            desc.systemValue = ShaderBuiltin::PackedFragRate;
          if(!_stricmp(desc.semanticName.c_str(), "SV_CullPrimitive"))
            desc.systemValue = ShaderBuiltin::CullPrimitive;
          if(!_stricmp(desc.semanticName.c_str(), "SV_StencilRef"))
            desc.systemValue = ShaderBuiltin::StencilReference;
          if(!_stricmp(desc.semanticName.c_str(), "SV_InnerCoverage"))
            desc.systemValue = ShaderBuiltin::IsFullyCovered;
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
          if(i != j && a.semanticName == b.semanticName || a.semanticIndex != 0)
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

  // sort per-primitive outputs to the end
  if(m_Type == DXBC::ShaderType::Mesh)
  {
    std::stable_sort(m_Reflection->OutputSig.begin(), m_Reflection->OutputSig.end(),
                     [](const SigParameter &a, const SigParameter &b) {
                       return a.perPrimitiveRate < b.perPrimitiveRate;
                     });
  }

  // make sure to fetch the dispatch threads dimension from disassembly
  if(m_Type == DXBC::ShaderType::Compute && m_DXBCByteCode)
    m_DXBCByteCode->FetchComputeProperties(m_Reflection);
  if((m_Type == DXBC::ShaderType::Compute || m_Type == DXBC::ShaderType::Amplification ||
      m_Type == DXBC::ShaderType::Mesh) &&
     m_DXILByteCode)
    m_DXILByteCode->FetchComputeProperties(m_Reflection);

  // initialise debug chunks last
  for(uint32_t chunkIdx = 0; chunkIdx < header->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(data + chunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_SDBG)
    {
      m_DebugInfo = ProcessSDBGChunk(fourcc);
    }
    else if(*fourcc == FOURCC_SPDB)
    {
      m_DebugInfo = ProcessSPDBChunk(fourcc);
    }
  }

  // try to find SPDB in the separate debug info pdb now
  for(uint32_t chunkIdx = 0;
      debugHeader && m_DebugInfo == NULL && chunkIdx < debugHeader->numChunks; chunkIdx++)
  {
    uint32_t *fourcc = (uint32_t *)(debugData + debugChunkOffsets[chunkIdx]);

    if(*fourcc == FOURCC_SPDB)
    {
      m_DebugInfo = ProcessSPDBChunk(fourcc);
    }
  }

  if(m_DXBCByteCode && m_DebugInfo == NULL && !m_DebugShaderBlob.empty())
    m_DebugInfo = ProcessPDB(m_DebugShaderBlob.data(), (uint32_t)m_DebugShaderBlob.size());

  if(m_DXILByteCode)
    m_DebugInfo = m_DXILByteCode;

  // we do a mini-preprocess of the files from the debug info to handle #line directives.
  // This means that any lines that our source file declares to be in another filename via a #line
  // get put in the right place for what the debug information hopefully matches.
  // We also concatenate duplicate lines and display them all, to handle edge cases where #lines
  // declare duplicates.

  if(m_DebugInfo)
  {
    if(m_DXBCByteCode)
      m_DXBCByteCode->SetDebugInfo(m_DebugInfo);

    PreprocessLineDirectives(m_DebugInfo->Files);
  }

  // if we had bytecode in this container, ensure we had reflection. If it's a blob with only an
  // input signature then we can do without reflection.
  if(m_DXBCByteCode || m_DXILByteCode)
  {
    RDCASSERT(m_Reflection);

    if(shaderExtReg != ~0U)
    {
      bool found = false;
      const bool pre_sm51 = (m_Version.Major * 10 + m_Version.Minor) < 51;

      // see if we can find the magic UAV. If so remove it from the reflection
      for(size_t i = 0; i < m_Reflection->UAVs.size(); i++)
      {
        const ShaderInputBind &uav = m_Reflection->UAVs[i];
        if(uav.reg == shaderExtReg && (pre_sm51 || shaderExtSpace == uav.space))
        {
          found = true;
          m_Reflection->UAVs.erase(i);
          if(m_DXBCByteCode)
            m_DXBCByteCode->SetShaderEXTUAV(api, shaderExtSpace, shaderExtReg);
          m_ShaderExt = {shaderExtSpace, shaderExtReg};
          break;
        }
      }
    }
  }
}

DXBCContainer::~DXBCContainer()
{
  // DXIL bytecode doubles as debug info, don't delete it twice
  if(m_DXILByteCode)
    m_DebugInfo = NULL;

  SAFE_DELETE(m_DebugInfo);

  SAFE_DELETE(m_DXBCByteCode);
  SAFE_DELETE(m_DXILByteCode);

  SAFE_DELETE(m_Reflection);
}

struct DxcArg
{
  uint32_t bit;
  const wchar_t *arg;
} dxc_flags[] = {
    {D3DCOMPILE_DEBUG, L"-Zi"},
    {D3DCOMPILE_SKIP_VALIDATION, L"-Vd"},
    {D3DCOMPILE_SKIP_OPTIMIZATION, L"-Od"},
    {D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, L"-Zpr"},
    {D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR, L"-Zpc "},
    {D3DCOMPILE_PARTIAL_PRECISION, L"-Gpp"},
    {D3DCOMPILE_NO_PRESHADER, L"-Op"},
    {D3DCOMPILE_AVOID_FLOW_CONTROL, L"-Gfa"},
    {D3DCOMPILE_PREFER_FLOW_CONTROL, L"-Gfp"},
    {D3DCOMPILE_ENABLE_STRICTNESS, L"-Ges"},
    {D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, L"-Gec"},
    {D3DCOMPILE_IEEE_STRICTNESS, L"-Gis"},
    {D3DCOMPILE_WARNINGS_ARE_ERRORS, L"-WX"},
    {D3DCOMPILE_RESOURCES_MAY_ALIAS, L"-res_may_alias"},
    {D3DCOMPILE_ALL_RESOURCES_BOUND, L"-all_resources_bound"},
    {D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, L"-enable_unbounded_descriptor_tables"},
    {D3DCOMPILE_DEBUG_NAME_FOR_SOURCE, L"-Zss"},
    {D3DCOMPILE_DEBUG_NAME_FOR_BINARY, L"-Zsb"},
};

void EncodeDXCFlags(uint32_t flags, rdcarray<rdcwstr> &args)
{
  for(const DxcArg &arg : dxc_flags)
  {
    if(flags & arg.bit)
      args.push_back(arg.arg);
  }

  // Can't make this match DXC defaults
  // DXC by default uses /O3 and FXC uses /O1

  // Optimization flags are a special case.
  // D3DCOMPILE_OPTIMIZATION_LEVEL0 = (1 << 14)
  // D3DCOMPILE_OPTIMIZATION_LEVEL1 = 0
  // D3DCOMPILE_OPTIMIZATION_LEVEL2 = ((1 << 14) | (1 << 15))
  // D3DCOMPILE_OPTIMIZATION_LEVEL3 = (1 << 15)

  uint32_t opt = (flags & D3DCOMPILE_OPTIMIZATION_LEVEL2);
  if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL0)
    args.push_back(L"-O0");
  else if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL1)
    args.push_back(L"-O1");
  else if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL2)
    args.push_back(L"-O2");
  else if(opt == D3DCOMPILE_OPTIMIZATION_LEVEL3)
    args.push_back(L"-O3");
};

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

rdcstr GetProfile(const ShaderCompileFlags &compileFlags)
{
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

      const char *prof = strstr(cmdline.c_str(), " /T ");
      if(!prof)
        prof = strstr(cmdline.c_str(), " -T ");

      if(!prof)
        return "";

      prof += 4;

      return rdcstr(prof, strchr(prof, ' ') - prof);
    }
  }

  return "";
}

ShaderCompileFlags EncodeFlags(const uint32_t flags, const rdcstr &profile)
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

  if(!profile.empty())
    cmdline += " /T " + profile;

  ret.flags = {{"@cmdline", cmdline.trimmed()}};

  // If D3DCOMPILE_SKIP_OPTIMIZATION is set, then prefer source-level debugging as it should be
  // accurate enough to work with.
  if(flags & D3DCOMPILE_SKIP_OPTIMIZATION)
    ret.flags.push_back({"preferSourceDebug", "1"});

  return ret;
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

  DXBC::DXBCContainer container(buf, rdcstr(), GraphicsAPI::D3D11, ~0U, ~0U);

  // the only thing fetched lazily is the disassembly, so grab that here

  rdcstr disasm = container.GetDisassembly(false);

  RDCLOG("%s", disasm.c_str());
}

#endif

#include "dxbc_compile.h"

TEST_CASE("Check DXBC hash algorithm", "[dxbc]")
{
  SECTION("Test live compiles against fxc")
  {
    HMODULE d3dcompiler = GetD3DCompiler();

    if(!d3dcompiler)
      return;

    pD3DCompile compileFunc = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");

    if(compileFunc == NULL)
    {
      RDCFATAL("Can't get D3DCompile from d3dcompiler_??.dll");
    }

    HRESULT hr = S_OK;

    ID3DBlob *byteBlob = NULL;

    // don't include debug info
    uint32_t flags = D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_SKIP_OPTIMIZATION;

    // create snippets that affect the compilation since we don't have embedded source
    rdcarray<rdcstr> snippets = {
        R"(
)",
        R"(
ret.x = sin(ret.x);
)",
        R"(
ret.xy = cos(ret.zw * ret.xy);
)",
        R"(
ret.xy += sqrt(ret.z).xx;
)",
        R"(
ret.zw += tex.Load(ret.xyz).yz;
)",
    };

    // add 128 snippets, each with a one character long input to slowly inflate the reflection
    // data.
    // The intent is to produce bytecodes of length 0 through 60 mod 64 (since bytecode is always
    // dword aligned)
    {
      const char *snippet = R"(
#define TEX_NAME tex%s
Texture2D<float> TEX_NAME : register(t0);
float4 main(float3 input : INPUT) : SV_Target0
{
  return TEX_NAME.Load(input);
}
)";
      rdcstr extra;
      for(int i = 0; i < 128; i++)
      {
        snippets.push_back(StringFormat::Fmt(snippet, extra.c_str()));
        extra += 'A';
      }
    }

    bool dwordLength[15] = {};

    for(rdcstr snippet : snippets)
    {
      rdcstr source;

      if(snippet.contains("main("))
        source = snippet;
      else
        source = R"(
Texture2D<float4> tex : register(t0);

float4 main(float input : INPUT) : SV_Target0
{
  float4 ret = input.xxxx;
)" + snippet +
                 R"(
  return ret;
}

)";

      ID3DBlob *errBlob;
      hr = compileFunc(source.c_str(), source.size(), "main", NULL, NULL, "main", "ps_5_0", flags,
                       0, &byteBlob, &errBlob);

      if(errBlob)
        RDCLOG("%s", (char *)errBlob->GetBufferPointer());

      REQUIRE(SUCCEEDED(hr));
      if(SUCCEEDED(hr))
      {
        bytebuf bytecode;
        bytecode.assign((const byte *)byteBlob->GetBufferPointer(), byteBlob->GetBufferSize());

        REQUIRE(bytecode.size() % 4 == 0);
        dwordLength[(bytecode.size() % 64) / 4] = true;

        bytebuf hashed = bytecode;
        DXBC::FileHeader *header = (DXBC::FileHeader *)hashed.data();
        RDCEraseEl(header->hashValue);

        DXBC::DXBCContainer::HashContainer(hashed.data(), hashed.size());

        bool same = (bytecode == hashed);
        CHECK(same);
      }

      SAFE_RELEASE(byteBlob);
    }

    // check that we've tested every length, mod 64.
    for(int i = 0; i < ARRAY_COUNT(dwordLength); i++)
      CHECK(dwordLength[i]);
  }

  SECTION("Test odd-sized buffer")
  {
    // dxc produces non-dword sized containers, but we don't want to pull dxc into our tests so we
    // instead test a fixed known shader

    bytebuf dxil = {
        0x44, 0x58, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xef, 0x05, 0x00, 0x00, 0x06, 0x00,
        0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0xbb,
        0x00, 0x00, 0x00, 0x37, 0x01, 0x00, 0x00, 0x53, 0x01, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47,
        0x31, 0x2f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x4e, 0x50, 0x55, 0x54, 0x41, 0x00, 0x4f, 0x53, 0x47, 0x31, 0x34, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x56, 0x5f, 0x50, 0x6f,
        0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x50, 0x53, 0x56, 0x30, 0x74, 0x00, 0x00, 0x00,
        0x24, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00,
        0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x4e, 0x50, 0x55, 0x54, 0x41, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x41, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x44, 0x03, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x41, 0x53, 0x48,
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x28, 0x08, 0x8c, 0xa0, 0xf5, 0x45,
        0x32, 0x63, 0x6a, 0x19, 0x1b, 0xa0, 0xf6, 0xc4, 0x76, 0x44, 0x58, 0x49, 0x4c, 0x94, 0x04,
        0x00, 0x00, 0x60, 0x00, 0x01, 0x00, 0x25, 0x01, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c, 0x00,
        0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x7c, 0x04, 0x00, 0x00, 0x42, 0x43, 0xc0, 0xde,
        0x21, 0x0c, 0x00, 0x00, 0x1c, 0x01, 0x00, 0x00, 0x0b, 0x82, 0x20, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x13, 0x00, 0x00, 0x00, 0x07, 0x81, 0x23, 0x91, 0x41, 0xc8, 0x04, 0x49, 0x06, 0x10,
        0x32, 0x39, 0x92, 0x01, 0x84, 0x0c, 0x25, 0x05, 0x08, 0x19, 0x1e, 0x04, 0x8b, 0x62, 0x80,
        0x10, 0x45, 0x02, 0x42, 0x92, 0x0b, 0x42, 0x84, 0x10, 0x32, 0x14, 0x38, 0x08, 0x18, 0x4b,
        0x0a, 0x32, 0x42, 0x88, 0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5, 0x00, 0x19, 0x32,
        0x42, 0xe4, 0x48, 0x0e, 0x90, 0x11, 0x22, 0xc4, 0x50, 0x41, 0x51, 0x81, 0x8c, 0xe1, 0x83,
        0xe5, 0x8a, 0x04, 0x21, 0x46, 0x06, 0x51, 0x18, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x1b,
        0x88, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x07, 0x40, 0x02, 0x00, 0x00, 0x49, 0x18, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x13, 0x82, 0x00, 0x00, 0x89, 0x20, 0x00, 0x00, 0x0e, 0x00, 0x00,
        0x00, 0x32, 0x22, 0x08, 0x09, 0x20, 0x64, 0x85, 0x04, 0x13, 0x22, 0xa4, 0x84, 0x04, 0x13,
        0x22, 0xe3, 0x84, 0xa1, 0x90, 0x14, 0x12, 0x4c, 0x88, 0x8c, 0x0b, 0x84, 0x84, 0x4c, 0x10,
        0x28, 0x23, 0x00, 0x25, 0x00, 0x8a, 0x39, 0x02, 0x30, 0x98, 0x23, 0x40, 0x66, 0x00, 0x8a,
        0x01, 0x33, 0x43, 0x45, 0x36, 0x10, 0x90, 0x03, 0x03, 0x00, 0x00, 0x00, 0x13, 0x14, 0x72,
        0xc0, 0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79, 0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d,
        0xaf, 0x50, 0x0e, 0x6d, 0xd0, 0x0e, 0x7a, 0x50, 0x0e, 0x6d, 0x00, 0x0f, 0x7a, 0x30, 0x07,
        0x72, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0, 0x07, 0x73, 0x20, 0x07,
        0x6d, 0x90, 0x0e, 0x78, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0x60, 0x07,
        0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe9, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07,
        0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07, 0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10, 0x07,
        0x76, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07,
        0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07, 0x74, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x6d, 0xe0, 0x0e,
        0x78, 0xa0, 0x07, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x76, 0x40, 0x07,
        0x43, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x3c,
        0x06, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x81, 0x00, 0x00,
        0x0b, 0x00, 0x00, 0x00, 0x32, 0x1e, 0x98, 0x10, 0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26,
        0x47, 0xc6, 0x04, 0x43, 0x9a, 0x12, 0x18, 0x01, 0x28, 0x85, 0x62, 0x28, 0x83, 0xf2, 0x20,
        0x2a, 0x89, 0x11, 0x80, 0x12, 0x28, 0x83, 0x42, 0xa0, 0x1c, 0x6b, 0x08, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x1a, 0x03, 0x4c, 0x90,
        0x46, 0x02, 0x13, 0x44, 0x35, 0x18, 0x63, 0x0b, 0x73, 0x3b, 0x03, 0xb1, 0x2b, 0x93, 0x9b,
        0x4b, 0x7b, 0x73, 0x03, 0x99, 0x71, 0xb9, 0x01, 0x41, 0xa1, 0x0b, 0x3b, 0x9b, 0x7b, 0x91,
        0x2a, 0x62, 0x2a, 0x0a, 0x9a, 0x2a, 0xfa, 0x9a, 0xb9, 0x81, 0x79, 0x31, 0x4b, 0x73, 0x0b,
        0x63, 0x4b, 0xd9, 0x10, 0x04, 0x13, 0x84, 0x41, 0x98, 0x20, 0x0c, 0xc3, 0x06, 0x61, 0x20,
        0x26, 0x08, 0x03, 0xb1, 0x41, 0x18, 0x0c, 0x0a, 0x76, 0x73, 0x13, 0x84, 0xa1, 0xd8, 0x30,
        0x20, 0x09, 0x31, 0x41, 0x48, 0x9a, 0x0d, 0xc1, 0x32, 0x41, 0x10, 0x00, 0x12, 0x6d, 0x61,
        0x69, 0x6e, 0x34, 0x92, 0x9c, 0xa0, 0xaa, 0xa8, 0x82, 0x26, 0x08, 0x04, 0x32, 0x41, 0x20,
        0x92, 0x0d, 0x01, 0x31, 0x41, 0x20, 0x94, 0x0d, 0x0b, 0xf1, 0x40, 0x91, 0x14, 0x0d, 0x13,
        0x11, 0x01, 0x1b, 0x02, 0x8a, 0xcb, 0x94, 0xd5, 0x17, 0xd4, 0xdb, 0x5c, 0x1a, 0x5d, 0xda,
        0x9b, 0xdb, 0x04, 0x81, 0x58, 0x26, 0x08, 0x04, 0x33, 0x41, 0x18, 0x8c, 0x09, 0xc2, 0x70,
        0x6c, 0x10, 0x32, 0x6d, 0xc3, 0x42, 0x58, 0xd0, 0x25, 0x61, 0x03, 0x46, 0x44, 0xdb, 0x86,
        0x80, 0xdb, 0x30, 0x54, 0x1d, 0xb0, 0xa1, 0x68, 0x1c, 0x0f, 0x00, 0xaa, 0xb0, 0xb1, 0xd9,
        0xb5, 0xb9, 0xa4, 0x91, 0x95, 0xb9, 0xd1, 0x4d, 0x09, 0x82, 0x2a, 0x64, 0x78, 0x2e, 0x76,
        0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x53, 0x02, 0xa2, 0x09, 0x19, 0x9e, 0x8b, 0x5d, 0x18,
        0x9b, 0x5d, 0x99, 0xdc, 0x94, 0xc0, 0xa8, 0x43, 0x86, 0xe7, 0x32, 0x87, 0x16, 0x46, 0x56,
        0x26, 0xd7, 0xf4, 0x46, 0x56, 0xc6, 0x36, 0x25, 0x48, 0xea, 0x90, 0xe1, 0xb9, 0xd8, 0xa5,
        0x95, 0xdd, 0x25, 0x91, 0x4d, 0xd1, 0x85, 0xd1, 0x95, 0x4d, 0x09, 0x96, 0x3a, 0x64, 0x78,
        0x2e, 0x65, 0x6e, 0x74, 0x72, 0x79, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x73, 0x53, 0x02, 0x0f,
        0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x33, 0x08, 0x80, 0x1c, 0xc4,
        0xe1, 0x1c, 0x66, 0x14, 0x01, 0x3d, 0x88, 0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07,
        0x79, 0x78, 0x07, 0x73, 0x98, 0x71, 0x0c, 0xe6, 0x00, 0x0f, 0xed, 0x10, 0x0e, 0xf4, 0x80,
        0x0e, 0x33, 0x0c, 0x42, 0x1e, 0xc2, 0xc1, 0x1d, 0xce, 0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d,
        0x88, 0x43, 0x38, 0x84, 0x83, 0x1b, 0xcc, 0x03, 0x3d, 0xc8, 0x43, 0x3d, 0x8c, 0x03, 0x3d,
        0xcc, 0x78, 0x8c, 0x74, 0x70, 0x07, 0x7b, 0x08, 0x07, 0x79, 0x48, 0x87, 0x70, 0x70, 0x07,
        0x7a, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20, 0x87, 0x19, 0xcc, 0x11, 0x0e, 0xec, 0x90,
        0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0, 0x0e, 0xf0, 0x50, 0x0e, 0x33, 0x10,
        0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8, 0x21, 0x1d, 0xc2, 0x61, 0x1e, 0x66, 0x30, 0x89, 0x3b,
        0xbc, 0x83, 0x3b, 0xd0, 0x43, 0x39, 0xb4, 0x03, 0x3c, 0xbc, 0x83, 0x3c, 0x84, 0x03, 0x3b,
        0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87, 0x72, 0x68, 0x07,
        0x37, 0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76, 0xf8, 0x05,
        0x76, 0x78, 0x87, 0x77, 0x80, 0x87, 0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87,
        0x79, 0x98, 0x81, 0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5, 0xc0, 0x0e, 0xec, 0x30,
        0x03, 0x62, 0xc8, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xdc,
        0x61, 0x1c, 0xca, 0x21, 0x1c, 0xc4, 0x81, 0x1d, 0xca, 0x61, 0x06, 0xd6, 0x90, 0x43, 0x39,
        0xc8, 0x43, 0x39, 0x98, 0x43, 0x39, 0xc8, 0x43, 0x39, 0xb8, 0xc3, 0x38, 0x94, 0x43, 0x38,
        0x88, 0x03, 0x3b, 0x94, 0xc3, 0x2f, 0xbc, 0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b,
        0xb0, 0xc3, 0x0c, 0xc4, 0x21, 0x07, 0x7c, 0x70, 0x03, 0x7a, 0x28, 0x87, 0x76, 0x80, 0x87,
        0x19, 0xd1, 0x43, 0x0e, 0xf8, 0xe0, 0x06, 0xe4, 0x20, 0x0e, 0xe7, 0xe0, 0x06, 0xf6, 0x10,
        0x0e, 0xf2, 0xc0, 0x0e, 0xe1, 0x90, 0x0f, 0xef, 0x50, 0x0f, 0xf4, 0x00, 0x00, 0x00, 0x71,
        0x20, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x16, 0x50, 0x0d, 0x97, 0xef, 0x3c, 0xbe, 0x34,
        0x39, 0x11, 0x81, 0x52, 0xd3, 0x43, 0x4d, 0x7e, 0x71, 0xdb, 0x06, 0x40, 0x30, 0x00, 0xd2,
        0x00, 0x61, 0x20, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x13, 0x04, 0x41, 0x2c, 0x10, 0x00,
        0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x44, 0x45, 0x40, 0x35, 0x46, 0x00, 0x82, 0x20, 0x88,
        0x7f, 0x63, 0x04, 0x20, 0x08, 0x82, 0x20, 0x18, 0x8c, 0x11, 0x80, 0x20, 0x08, 0x92, 0x60,
        0x30, 0x46, 0x00, 0x82, 0x20, 0x88, 0x82, 0x01, 0x00, 0x00, 0x00, 0x00, 0x23, 0x06, 0x09,
        0x00, 0x82, 0x60, 0x60, 0x48, 0x0f, 0x04, 0x29, 0xc4, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18,
        0x18, 0xd2, 0x03, 0x41, 0xc9, 0x30, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x86, 0xf4, 0x40,
        0x50, 0x21, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x81, 0x21, 0x3d, 0x10, 0x84, 0x04, 0x08,
        0x00, 0x00, 0x00, 0x00,
    };

    DXBC::DXBCContainer::HashContainer(dxil.data(), dxil.size());

    DXBC::FileHeader *header = (DXBC::FileHeader *)dxil.data();

    CHECK(header->hashValue[0] == 3739765114);
    CHECK(header->hashValue[1] == 3689508432);
    CHECK(header->hashValue[2] == 2832704775);
    CHECK(header->hashValue[3] == 3632933760);
  }
}

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
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);

    flags = 0;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_DEBUG;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);
  };

  SECTION("encode/decode discards unrecognised parameters")
  {
    uint32_t flags = D3DCOMPILE_PARTIAL_PRECISION | (1 << 30);
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags2 == D3DCOMPILE_PARTIAL_PRECISION);

    ShaderCompileFlags compileflags;

    compileflags.flags = {
        {"@cmdline", "/Zi /Z8 /JJ /WX /K other words embed/Odparam /DFoo=\"bar\""}};

    flags2 = DXBC::DecodeFlags(compileflags);

    CHECK(flags2 == (D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS));

    flags = ~0U;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    uint32_t allflags = 0;
    for(const DXBC::FxcArg &a : DXBC::fxc_flags)
      allflags |= a.bit;

    allflags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;

    CHECK(flags2 == allflags);
  };

  SECTION("optimisation flags are properly decoded and encoded")
  {
    uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL0;
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL1;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL2;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);
  };

  SECTION("Profile is correctly encoded and decoded")
  {
    const uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;

    rdcstr profile = "ps_5_0";
    rdcstr profile2 = DXBC::GetProfile(DXBC::EncodeFlags(flags, profile));

    CHECK(profile == profile2);

    profile = "ps_4_0";
    profile2 = DXBC::GetProfile(DXBC::EncodeFlags(flags, profile));

    CHECK(profile == profile2);

    profile = "";
    profile2 = DXBC::GetProfile(DXBC::EncodeFlags(flags, profile));

    CHECK(profile == profile2);

    profile = "cs_5_0";
    profile2 = DXBC::GetProfile(DXBC::EncodeFlags(flags, profile));

    CHECK(profile == profile2);

    profile = "??_9_9";
    profile2 = DXBC::GetProfile(DXBC::EncodeFlags(flags, profile));

    CHECK(profile == profile2);
  };

  SECTION("Profile does not affect flag encoding")
  {
    uint32_t flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    uint32_t flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, ""));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, "ps_5_0"));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, "ps_4_0"));

    CHECK(flags == flags2);

    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    flags2 = DXBC::DecodeFlags(DXBC::EncodeFlags(flags, "??_9_9"));

    CHECK(flags == flags2);
  };
}
#endif
