/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include "common/formatting.h"
#include "dxil_bytecode.h"
#include "dxil_common.h"

namespace DXIL
{
enum class ShaderTag
{
  ShaderFlags = 0,
  Geometry = 1,
  Domain = 2,
  Hull = 3,
  Compute = 4,
};

enum class ResField
{
  ID = 0,
  VarDecl = 1,
  Name = 2,
  Space = 3,
  RegBase = 4,
  RegCount = 5,

  // SRV
  SRVShape = 6,
  SRVSampleCount = 7,
  SRVTags = 8,

  // UAV
  UAVShape = 6,
  UAVGloballyCoherent = 7,
  UAVHiddenCounter = 8,
  UAVRasterOrder = 9,
  UAVTags = 10,

  // CBuffer
  CBufferByteSize = 6,
  CBufferTags = 7,

  // Sampler
  SamplerType = 6,
  SamplerTags = 7,
};

enum class SRVUAVTag
{
  ElementType = 0,
  StructStride = 1,
};

enum class StructMemberAnnotation
{
  SNorm = 0,
  UNorm = 1,
  Matrix = 2,
  CBufferOffset = 3,
  SemanticString = 4,
  InterpolationMode = 5,
  FieldName = 6,
  CompType = 7,
};

template <typename T>
T getival(const Metadata *m)
{
  return T(m->constant->val.u32v[0]);
}

void Program::FetchComputeProperties(DXBC::Reflection *reflection)
{
  for(const Function &f : m_Functions)
  {
    if(f.name.beginsWith("dx.op.threadId"))
    {
      SigParameter param;
      param.systemValue = ShaderBuiltin::DispatchThreadIndex;
      param.compCount = 3;
      param.regChannelMask = param.channelUsedMask = 0x7;
      param.semanticIdxName = param.semanticName = "threadId";
      reflection->InputSig.push_back(param);
    }
    else if(f.name.beginsWith("dx.op.groupId"))
    {
      SigParameter param;
      param.systemValue = ShaderBuiltin::GroupIndex;
      param.compCount = 3;
      param.regChannelMask = param.channelUsedMask = 0x7;
      param.semanticIdxName = param.semanticName = "groupID";
      reflection->InputSig.push_back(param);
    }
    else if(f.name.beginsWith("dx.op.threadIdInGroup"))
    {
      SigParameter param;
      param.systemValue = ShaderBuiltin::GroupThreadIndex;
      param.compCount = 3;
      param.regChannelMask = param.channelUsedMask = 0x7;
      param.semanticIdxName = param.semanticName = "threadIdInGroup";
      reflection->InputSig.push_back(param);
    }
    else if(f.name.beginsWith("dx.op.flattenedThreadIdInGroup"))
    {
      SigParameter param;
      param.systemValue = ShaderBuiltin::GroupFlatIndex;
      param.compCount = 1;
      param.regChannelMask = param.channelUsedMask = 0x1;
      param.semanticIdxName = param.semanticName = "flattenedThreadIdInGroup";
      reflection->InputSig.push_back(param);
    }
  }

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    if(m_NamedMeta[i].name == "dx.entryPoints")
    {
      // expect only one child for this, DX doesn't support multiple entry points for compute
      // shaders
      RDCASSERTEQUAL(m_NamedMeta[i].children.size(), 1);
      Metadata &entry = *m_NamedMeta[i].children[0];
      RDCASSERTEQUAL(entry.children.size(), 5);
      Metadata &tags = *entry.children[4];

      for(size_t t = 0; t < tags.children.size(); t += 2)
      {
        RDCASSERT(tags.children[t]->isConstant);
        if(getival<ShaderTag>(tags.children[t]) == ShaderTag::Compute)
        {
          Metadata &threadDim = *tags.children[t + 1];
          RDCASSERTEQUAL(threadDim.children.size(), 3);
          reflection->DispatchThreadsDimension[0] = getival<uint32_t>(threadDim.children[0]);
          reflection->DispatchThreadsDimension[1] = getival<uint32_t>(threadDim.children[1]);
          reflection->DispatchThreadsDimension[2] = getival<uint32_t>(threadDim.children[2]);
          return;
        }
      }

      break;
    }
  }

  RDCERR("Couldn't find thread dimension tag in shader");

  reflection->DispatchThreadsDimension[0] = 1;
  reflection->DispatchThreadsDimension[1] = 1;
  reflection->DispatchThreadsDimension[2] = 1;
}

D3D_PRIMITIVE_TOPOLOGY Program::GetOutputTopology()
{
  if(m_Type != DXBC::ShaderType::Geometry && m_Type != DXBC::ShaderType::Domain)
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    if(m_NamedMeta[i].name == "dx.entryPoints")
    {
      // expect only one child for this, DX doesn't support multiple entry points for compute
      // shaders
      RDCASSERTEQUAL(m_NamedMeta[i].children.size(), 1);
      Metadata &entry = *m_NamedMeta[i].children[0];
      RDCASSERTEQUAL(entry.children.size(), 5);
      Metadata &tags = *entry.children[4];

      for(size_t t = 0; t < tags.children.size(); t += 2)
      {
        RDCASSERT(tags.children[t]->isConstant);
        if(getival<ShaderTag>(tags.children[t]) == ShaderTag::Geometry)
        {
          Metadata &geomData = *tags.children[t + 1];
          RDCASSERTEQUAL(geomData.children.size(), 5);
          return getival<D3D_PRIMITIVE_TOPOLOGY>(geomData.children[3]);
        }
      }

      break;
    }
  }

  RDCERR("Couldn't find topology tag in shader");

  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

struct DXMeta
{
  struct
  {
    const Metadata *contents = NULL;
    const Metadata *defines = NULL;
    const Metadata *mainFileName = NULL;
    const Metadata *args = NULL;
  } source;

  const Metadata *version = NULL;
  const Metadata *valver = NULL;
  const Metadata *shaderModel = NULL;
  const Metadata *resources = NULL;
  const Metadata *typeAnnotations = NULL;
  const Metadata *viewIdState = NULL;
  const Metadata *entryPoints = NULL;

  // technically llvm.ident
  const Metadata *ident = NULL;

  DXMeta(const rdcarray<NamedMetadata> &namedMeta)
  {
    DXMeta &dx = *this;
    DXMeta &llvm = *this;

    for(size_t i = 0; i < namedMeta.size(); i++)
    {
#define GRAB_META(metaname)          \
  if(namedMeta[i].name == #metaname) \
    metaname = &namedMeta[i];

      GRAB_META(llvm.ident);
      GRAB_META(dx.source.contents);
      GRAB_META(dx.source.defines);
      GRAB_META(dx.source.mainFileName);
      GRAB_META(dx.source.args);
      GRAB_META(dx.version);
      GRAB_META(dx.valver);
      GRAB_META(dx.shaderModel);
      GRAB_META(dx.resources);
      GRAB_META(dx.typeAnnotations);
      GRAB_META(dx.viewIdState);
      GRAB_META(dx.entryPoints);

#undef GRAB_META
    }
  }
};

struct TypeInfo
{
  struct MemberData
  {
    enum Flags : uint8_t
    {
      None = 0,
      UNorm = 0x1,
      SNorm = 0x2,
      RowMajor = 0x4,
      Matrix = 0x8,
    } flags = None;
    uint8_t rows = 0, cols = 0;
    uint32_t offset;
    rdcstr name;
    ComponentType type;
  };

  struct StructData
  {
    uint32_t byteSize;
    rdcarray<MemberData> members;
  };

  std::map<const Type *, StructData> structData;

  TypeInfo(const Metadata *typeAnnotations)
  {
    if(!typeAnnotations)
      return;

    const Metadata *structAnnotations = NULL;

    for(size_t i = 0; i < typeAnnotations->children.size(); i++)
    {
      if(getival<uint32_t>(typeAnnotations->children[i]->children[0]) == 0)
      {
        structAnnotations = typeAnnotations->children[i];
        break;
      }
    }

    if(!structAnnotations)
      return;

    for(size_t c = 1; c < structAnnotations->children.size(); c += 2)
    {
      const Type *type = structAnnotations->children[c]->type;
      const Metadata *structMembers = structAnnotations->children[c + 1];

      RDCASSERT(structMembers->children.size() - 1 >= type->members.size(),
                structMembers->children.size(), type->members.size());

      StructData &data = structData[type];
      data.byteSize = getival<uint32_t>(structMembers->children[0]);
      data.members.resize(type->members.size());

      for(size_t m = 0; m < type->members.size(); m++)
      {
        const Metadata *memberIn = structMembers->children[m + 1];
        MemberData &memberOut = data.members[m];

        for(size_t tag = 0; tag < memberIn->children.size(); tag += 2)
        {
          StructMemberAnnotation fieldTag = getival<StructMemberAnnotation>(memberIn->children[tag]);
          switch(fieldTag)
          {
            case StructMemberAnnotation::SNorm:
            {
              if(getival<uint32_t>(memberIn->children[tag + 1]) != 0)
                memberOut.flags = MemberData::Flags(memberOut.flags | MemberData::SNorm);
              break;
            }
            case StructMemberAnnotation::UNorm:
            {
              if(getival<uint32_t>(memberIn->children[tag + 1]) != 0)
                memberOut.flags = MemberData::Flags(memberOut.flags | MemberData::UNorm);
              break;
            }
            case StructMemberAnnotation::Matrix:
            {
              const Metadata *matrixData = memberIn->children[tag + 1];
              memberOut.rows = getival<uint8_t>(matrixData->children[0]);
              memberOut.cols = getival<uint8_t>(matrixData->children[1]);
              bool rowmajor = (getival<uint32_t>(matrixData->children[2]) == 1);
              if(rowmajor)
                memberOut.flags =
                    MemberData::Flags(memberOut.flags | MemberData::RowMajor | MemberData::Matrix);
              else
                memberOut.flags = MemberData::Flags(memberOut.flags | MemberData::Matrix);
              break;
            }
            case StructMemberAnnotation::CBufferOffset:
              memberOut.offset = getival<uint32_t>(memberIn->children[tag + 1]);
              break;
            case StructMemberAnnotation::SemanticString: break;
            case StructMemberAnnotation::InterpolationMode: break;
            case StructMemberAnnotation::FieldName:
              memberOut.name = memberIn->children[tag + 1]->str;
              break;
            case StructMemberAnnotation::CompType:
              memberOut.type = getival<ComponentType>(memberIn->children[tag + 1]);
              break;
            default: RDCWARN("Unexpected field tag %u", fieldTag); break;
          }
        }
      }
    }
  }
};

static DXBC::CBufferVariableType MakeCBufferVariableType(const TypeInfo &typeInfo, const Type *t)
{
  using namespace DXBC;

  CBufferVariableType ret = {};

  ret.descriptor.elements = 1;

  if(t->type == Type::Scalar || t->type == Type::Vector)
  {
    ret.descriptor.rows = ret.descriptor.cols = 1;
    if(t->type == Type::Vector)
      ret.descriptor.cols = t->elemCount;
    ret.descriptor.bytesize = (t->bitWidth / 8) * ret.descriptor.cols;
    ret.descriptor.varClass = CLASS_SCALAR;

    if(t->scalarType == Type::Float)
    {
      if(t->bitWidth > 32)
        ret.descriptor.varType = VarType::Double;
      else if(t->bitWidth == 16)
        ret.descriptor.varType = VarType::Half;
      else
        ret.descriptor.varType = VarType::Float;
    }
    else
    {
      // can't distinguish int/uint here, default to signed
      if(t->bitWidth > 32)
        ret.descriptor.varType = VarType::SLong;
      else if(t->bitWidth == 32)
        ret.descriptor.varType = VarType::SInt;
      else if(t->bitWidth == 16)
        ret.descriptor.varType = VarType::SShort;
      else if(t->bitWidth == 8)
        ret.descriptor.varType = VarType::SByte;
      else if(t->bitWidth == 1)
        ret.descriptor.varType = VarType::Bool;
    }

    ret.descriptor.name = ToStr(ret.descriptor.varType);
    if(t->type == Type::Vector)
      ret.descriptor.name += ToStr(ret.descriptor.cols);

    return ret;
  }
  else if(t->type == Type::Array)
  {
    ret = MakeCBufferVariableType(typeInfo, t->inner);
    ret.descriptor.elements *= RDCMAX(1U, t->elemCount);
    // assume normal D3D array packing with each element on float4 boundary
    ret.descriptor.bytesize += (ret.descriptor.elements - 1) * AlignUp16(ret.descriptor.bytesize);
    return ret;
  }
  else if(t->type == Type::Struct)
  {
    // processing below
  }
  else
  {
    RDCERR("Unexpected type %u iterating cbuffer variable type %s", t->type, t->name.c_str());
    return ret;
  }

  ret.descriptor.name = t->name;
  ret.descriptor.varType = VarType::Unknown;
  ret.descriptor.varClass = CLASS_STRUCT;

  char alignmentPrefix[] = "dx.alignment.legacy.";
  if(ret.descriptor.name.beginsWith(alignmentPrefix))
    ret.descriptor.name.erase(0, sizeof(alignmentPrefix) - 1);

  char structPrefix[] = "struct.";
  if(ret.descriptor.name.beginsWith(structPrefix))
    ret.descriptor.name.erase(0, sizeof(structPrefix) - 1);

  char classPrefix[] = "class.";
  if(ret.descriptor.name.beginsWith(classPrefix))
    ret.descriptor.name.erase(0, sizeof(classPrefix) - 1);

  // if there are no members, return straight away
  if(t->members.empty())
    return ret;

  auto it = typeInfo.structData.find(t);

  if(it != typeInfo.structData.end())
  {
    ret.descriptor.bytesize = it->second.byteSize;
  }
  else
  {
    // shouldn't get here if we don't have type information at all
    RDCERR("Couldn't find type information for struct '%s'!", t->name.c_str());
    return ret;
  }

  if(ret.descriptor.name.contains("StructuredBuffer<"))
  {
    // silently go into the inner member that's declared in this type as we only care about
    // reflecting that actual structure
    if(t->members.size() == 1 && it->second.members.size() == 1 && it->second.members[0].name == "h")
      return MakeCBufferVariableType(typeInfo, t->members[0]);

    RDCWARN("Structured buffer declaration found but expected single inner handle");

    // otherwise use it as-is and trim off the name in an attempt to make it look normal

    ret.descriptor.name.trim();

    // remove any outer definition of the type
    if(ret.descriptor.name.back() == '>')
      ret.descriptor.name.pop_back();
    else
      RDCERR("Expected closing > in StructuredBuffer type name");

    int idx = ret.descriptor.name.indexOf('<');
    ret.descriptor.name.erase(0, idx + 1);
  }

  for(size_t i = 0; i < t->members.size(); i++)
  {
    CBufferVariable var;
    var.type = MakeCBufferVariableType(typeInfo, t->members[i]);
    if(it != typeInfo.structData.end())
    {
      var.name = it->second.members[i].name;
      var.offset = it->second.members[i].offset;

      if(it->second.members[i].flags & TypeInfo::MemberData::Matrix)
      {
        var.type.descriptor.rows = it->second.members[i].rows;
        var.type.descriptor.cols = it->second.members[i].cols;
        var.type.descriptor.varClass = (it->second.members[i].flags & TypeInfo::MemberData::RowMajor)
                                           ? CLASS_MATRIX_ROWS
                                           : CLASS_MATRIX_COLUMNS;

        // the array was expanded out like float[4][3] would be, so divide by the matrix dimension
        // to get the real array size
        var.type.descriptor.elements /= (it->second.members[i].flags & TypeInfo::MemberData::RowMajor)
                                            ? var.type.descriptor.rows
                                            : var.type.descriptor.cols;
      }

      if(var.type.members.empty() && t->members[i]->type != Type::Struct)
      {
        switch(it->second.members[i].type)
        {
          case ComponentType::Invalid:
            var.type.descriptor.varType = VarType::Unknown;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
          case ComponentType::I1: var.type.descriptor.varType = VarType::Bool; break;
          case ComponentType::I16: var.type.descriptor.varType = VarType::SShort; break;
          case ComponentType::U16: var.type.descriptor.varType = VarType::UShort; break;
          case ComponentType::I32: var.type.descriptor.varType = VarType::SInt; break;
          case ComponentType::U32: var.type.descriptor.varType = VarType::UInt; break;
          case ComponentType::I64: var.type.descriptor.varType = VarType::SLong; break;
          case ComponentType::U64: var.type.descriptor.varType = VarType::ULong; break;
          case ComponentType::F16: var.type.descriptor.varType = VarType::Half; break;
          case ComponentType::F32: var.type.descriptor.varType = VarType::Float; break;
          case ComponentType::F64: var.type.descriptor.varType = VarType::Double; break;
          case ComponentType::SNormF16:
            var.type.descriptor.varType = VarType::Half;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
          case ComponentType::UNormF16:
            var.type.descriptor.varType = VarType::Half;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
          case ComponentType::SNormF32:
            var.type.descriptor.varType = VarType::Float;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
          case ComponentType::UNormF32:
            var.type.descriptor.varType = VarType::Float;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
          case ComponentType::SNormF64:
            var.type.descriptor.varType = VarType::Double;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
          case ComponentType::UNormF64:
            var.type.descriptor.varType = VarType::Double;
            RDCERR("Unexpected type in cbuffer annotations");
            break;
        }
      }
    }
    else
    {
      var.name = StringFormat::Fmt("_child%zu", i);
      var.offset = 0;
    }
    ret.members.push_back(var);
  }

  return ret;
}

static void AddResourceBind(DXBC::Reflection *refl, const TypeInfo &typeInfo, const Metadata *r,
                            const bool srv)
{
  using namespace DXBC;

  ShaderInputBind bind;
  bind.name = r->children[(size_t)ResField::Name]->str;
  bind.type = ShaderInputBind::TYPE_TEXTURE;
  bind.space = getival<uint32_t>(r->children[(size_t)ResField::Space]);
  bind.reg = getival<uint32_t>(r->children[(size_t)ResField::RegBase]);
  bind.bindCount = getival<uint32_t>(r->children[(size_t)ResField::RegCount]);

  bind.retType = RETURN_TYPE_UNKNOWN;
  bind.numComps = 1;

  const Type *resType = r->children[(size_t)ResField::VarDecl]->type;
  const Type *baseType = resType;

  // variable should be a pointer to the underlying type
  RDCASSERT(resType->type == Type::Pointer);
  resType = resType->inner;

  // textures are a struct containing the inner type and a mips type
  if(resType->type == Type::Struct && !resType->members.empty())
    resType = resType->members[0];

  // if we found a vector go further to get the underlying type
  if(resType->type == Type::Vector)
  {
    bind.numComps = resType->elemCount;
    resType = resType->inner;
  }

  if(resType->type == Type::Scalar)
  {
    if(resType->scalarType == Type::Float)
      bind.retType = resType->bitWidth > 32 ? RETURN_TYPE_DOUBLE : RETURN_TYPE_FLOAT;
    // can't distinguish sign bit here, hope we have metadata to set this exactly
    else if(resType->scalarType == Type::Int)
      bind.retType = RETURN_TYPE_SINT;
  }

  const Metadata *tags =
      srv ? r->children[(size_t)ResField::SRVTags] : r->children[(size_t)ResField::UAVTags];

  uint32_t structStride = 0;
  for(size_t t = 0; tags && t < tags->children.size(); t += 2)
  {
    RDCASSERT(tags->children[t]->isConstant);
    if(getival<SRVUAVTag>(tags->children[t]) == SRVUAVTag::StructStride)
    {
      structStride = getival<uint32_t>(tags->children[t + 1]);
    }
    else if(getival<SRVUAVTag>(tags->children[t]) == SRVUAVTag::ElementType)
    {
      switch(getival<ComponentType>(tags->children[t + 1]))
      {
        default:
        case ComponentType::Invalid:
        case ComponentType::I1: bind.retType = RETURN_TYPE_UNKNOWN; break;
        case ComponentType::I16:
        case ComponentType::I32:
        case ComponentType::I64: bind.retType = RETURN_TYPE_SINT; break;
        case ComponentType::U16:
        case ComponentType::U32:
        case ComponentType::U64: bind.retType = RETURN_TYPE_UINT; break;
        case ComponentType::F16:
        case ComponentType::F32: bind.retType = RETURN_TYPE_FLOAT; break;
        case ComponentType::F64: bind.retType = RETURN_TYPE_DOUBLE; break;
        case ComponentType::SNormF16:
        case ComponentType::SNormF32:
        case ComponentType::SNormF64: bind.retType = RETURN_TYPE_SNORM; break;
        case ComponentType::UNormF16:
        case ComponentType::UNormF32:
        case ComponentType::UNormF64: bind.retType = RETURN_TYPE_UNORM; break;
      }
    }
  }

  ResourceKind shape = srv ? getival<ResourceKind>(r->children[(size_t)ResField::SRVShape])
                           : getival<ResourceKind>(r->children[(size_t)ResField::UAVShape]);

  rdcstr defName;

  switch(shape)
  {
    case ResourceKind::Unknown:
    case ResourceKind::SamplerComparison:
    case ResourceKind::RTAccelerationStructure:
    case ResourceKind::CBuffer:
    case ResourceKind::Sampler:
    case ResourceKind::FeedbackTexture2D:
    case ResourceKind::FeedbackTexture2DArray:
      RDCERR("Unexpected %s shape %u", srv ? "SRV" : "UAV", shape);
      defName = srv ? "SRV" : "UAV";
      break;
    case ResourceKind::Texture1D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture1D" : "RWTexture1D";
      bind.dimension = ShaderInputBind::DIM_TEXTURE1D;
      break;
    case ResourceKind::Texture2D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture2D" : "RWTexture2D";
      bind.dimension = ShaderInputBind::DIM_TEXTURE2D;
      break;
    case ResourceKind::Texture2DMS:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture2DMS" : "RWTexture2DMS";
      bind.dimension = ShaderInputBind::DIM_TEXTURE2DMS;
      break;
    case ResourceKind::Texture3D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture3D" : "RWTexture3D";
      bind.dimension = ShaderInputBind::DIM_TEXTURE3D;
      break;
    case ResourceKind::TextureCube:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "TextureCube" : "RWTextureCube";
      bind.dimension = ShaderInputBind::DIM_TEXTURECUBE;
      break;
    case ResourceKind::Texture1DArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture1DArray" : "RWTexture1DArray";
      bind.dimension = ShaderInputBind::DIM_TEXTURE1DARRAY;
      break;
    case ResourceKind::Texture2DArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture2DArray" : "RWTexture2DArray";
      bind.dimension = ShaderInputBind::DIM_TEXTURE2DARRAY;
      break;
    case ResourceKind::Texture2DMSArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture2DMSArray" : "RWTexture2DMSArray";
      bind.dimension = ShaderInputBind::DIM_TEXTURE2DMSARRAY;
      break;
    case ResourceKind::TextureCubeArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "TextureCubeArray" : "RWTextureCubeArray";
      bind.dimension = ShaderInputBind::DIM_TEXTURECUBEARRAY;
      break;
    case ResourceKind::TypedBuffer:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Buffer" : "RWBuffer";
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      break;
    case ResourceKind::TBuffer:
      bind.type = ShaderInputBind::TYPE_TBUFFER;
      defName = "TBuffer";
      bind.dimension = ShaderInputBind::DIM_UNKNOWN;
      bind.retType = RETURN_TYPE_UNKNOWN;
      break;
    case ResourceKind::RawBuffer:
      bind.type = srv ? ShaderInputBind::TYPE_BYTEADDRESS : ShaderInputBind::TYPE_UAV_RWBYTEADDRESS;
      defName = srv ? "ByteAddressBuffer" : "RWByteAddressBuffer";
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      bind.retType = RETURN_TYPE_MIXED;
      break;
    case ResourceKind::StructuredBuffer:
      bind.type = srv ? ShaderInputBind::TYPE_STRUCTURED : ShaderInputBind::TYPE_UAV_RWSTRUCTURED;
      defName = srv ? "StructuredBuffer" : "RWStructuredBuffer";
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      bind.retType = RETURN_TYPE_MIXED;
      break;
    case ResourceKind::StructuredBufferWithCounter:
      bind.type = srv ? ShaderInputBind::TYPE_STRUCTURED
                      : ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
      defName = srv ? "StructuredBufferWithCounter" : "RWStructuredBufferWithCounter";
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      bind.retType = RETURN_TYPE_MIXED;
      break;
  }

  if(bind.type == ShaderInputBind::TYPE_UAV_RWSTRUCTURED)
  {
    if(getival<uint32_t>(r->children[(size_t)ResField::UAVHiddenCounter]) != 0)
      bind.type = ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
  }

  if(bind.name.empty())
    bind.name = StringFormat::Fmt("%s%u", defName.c_str(),
                                  getival<uint32_t>(r->children[(size_t)ResField::ID]));

  switch(shape)
  {
    case ResourceKind::StructuredBuffer:
    case ResourceKind::StructuredBufferWithCounter:
    {
      if(!typeInfo.structData.empty())
      {
        refl->ResourceBinds[bind.name] = MakeCBufferVariableType(typeInfo, baseType->inner);
      }
      else
      {
        // if we don't have type annotations, create a dummy byte-array struct member
        refl->ResourceBinds[bind.name].descriptor.bytesize = structStride;
        refl->ResourceBinds[bind.name].descriptor.cols = 1;
        refl->ResourceBinds[bind.name].descriptor.rows = 1;
        refl->ResourceBinds[bind.name].descriptor.elements = structStride;
        refl->ResourceBinds[bind.name].descriptor.varClass = DXBC::CLASS_SCALAR;
        refl->ResourceBinds[bind.name].descriptor.varType = VarType::UByte;
      }
    }
    default: break;
  }

  if(srv)
    refl->SRVs.push_back(bind);
  else
    refl->UAVs.push_back(bind);
}

DXBC::Reflection *Program::GetReflection()
{
  using namespace DXBC;

  Reflection *refl = new Reflection;

  DXMeta dx(m_NamedMeta);

  TypeInfo typeInfo(dx.typeAnnotations);

  if(dx.ident && dx.ident->children.size() == 1 && dx.ident->children[0]->children.size() == 1)
  {
    m_CompilerSig = "dxc - " + dx.ident->children[0]->children[0]->str;
  }
  else
  {
    m_CompilerSig = "dxc - unknown version";
  }

  if(dx.valver && dx.valver->children.size() == 1 && dx.valver->children[0]->children.size() == 2)
  {
    m_CompilerSig +=
        StringFormat::Fmt(" (Validation version %s.%s)",
                          dx.valver->children[0]->children[0]->constant->toString().c_str(),
                          dx.valver->children[0]->children[1]->constant->toString().c_str());
  }

  if(dx.entryPoints && dx.entryPoints->children.size() > 0 &&
     dx.entryPoints->children[0]->children.size() > 2)
  {
    m_EntryPoint = dx.entryPoints->children[0]->children[1]->str;
  }
  else
  {
    RDCERR("Didn't find dx.entryPoints");
    m_EntryPoint = "main";
  }

  if(dx.shaderModel && dx.shaderModel->children.size() == 1 &&
     dx.shaderModel->children[0]->children.size() == 3)
  {
    m_Profile =
        StringFormat::Fmt("%s_%s_%s", dx.shaderModel->children[0]->children[0]->str.c_str(),
                          dx.shaderModel->children[0]->children[1]->constant->toString().c_str(),
                          dx.shaderModel->children[0]->children[2]->constant->toString().c_str());
  }
  else
  {
    switch(m_Type)
    {
      case DXBC::ShaderType::Pixel: m_Profile = "ps"; break;
      case DXBC::ShaderType::Vertex: m_Profile = "vs"; break;
      case DXBC::ShaderType::Geometry: m_Profile = "gs"; break;
      case DXBC::ShaderType::Hull: m_Profile = "hs"; break;
      case DXBC::ShaderType::Domain: m_Profile = "ds"; break;
      case DXBC::ShaderType::Compute: m_Profile = "cs"; break;
      default: m_Profile = "xx"; break;
    }
    m_Profile += StringFormat::Fmt("_%u_%u", m_Major, m_Minor);
  }

  if(dx.source.contents)
  {
    for(const Metadata *f : dx.source.contents->children)
    {
      if(f->children.size() != 2)
        continue;
      Files.push_back({f->children[0]->str, f->children[1]->str});
    }

    // push the main filename to the front
    if(dx.source.mainFileName && !dx.source.mainFileName->children.empty())
    {
      rdcstr mainFile = dx.source.mainFileName->children[0]->str;

      if(!mainFile.empty())
      {
        for(size_t i = 1; i < Files.size(); i++)
        {
          if(Files[i].first == mainFile)
          {
            std::swap(Files[0], Files[i]);
            break;
          }
        }
      }
    }
  }

  if(dx.source.args && dx.source.args->children.size() == 1)
  {
    rdcstr cmdline;
    for(const Metadata *f : dx.source.args->children[0]->children)
    {
      rdcstr param = f->str;
      param.trim();
      if(param.find_first_of(" \t\r\n") >= 0)
      {
        cmdline += " \"";
        for(char c : param)
        {
          if(c == '"')
            cmdline.push_back('\\');
          cmdline.push_back(c);
        }
        cmdline += "\"";
      }
      else
      {
        cmdline += " " + param;
      }
    }

    m_CompileFlags.flags.push_back({"@cmdline", cmdline});
  }
  else
  {
    m_CompileFlags.flags.push_back({"@cmdline", "-T " + m_Profile});
  }

  if(dx.resources)
  {
    RDCASSERTEQUAL(dx.resources->children.size(), 1);

    const Metadata *resList = dx.resources->children[0];
    RDCASSERTEQUAL(resList->children.size(), 4);

    const Metadata *SRVs = resList->children[0];
    if(SRVs)
    {
      for(const Metadata *r : SRVs->children)
      {
        AddResourceBind(refl, typeInfo, r, true);
      }
    }

    const Metadata *UAVs = resList->children[1];
    if(UAVs)
    {
      for(const Metadata *r : UAVs->children)
      {
        AddResourceBind(refl, typeInfo, r, false);
      }
    }

    const Metadata *CBVs = resList->children[2];
    if(CBVs)
    {
      for(const Metadata *r : CBVs->children)
      {
        CBuffer bind;
        bind.name = r->children[(size_t)ResField::Name]->str;
        bind.identifier = getival<uint32_t>(r->children[(size_t)ResField::ID]);
        bind.space = getival<uint32_t>(r->children[(size_t)ResField::Space]);
        bind.reg = getival<uint32_t>(r->children[(size_t)ResField::RegBase]);
        bind.bindCount = getival<uint32_t>(r->children[(size_t)ResField::RegCount]);

        bind.descriptor.type = CBuffer::Descriptor::TYPE_CBUFFER;
        bind.descriptor.byteSize = getival<uint32_t>(r->children[(size_t)ResField::CBufferByteSize]);

        if(bind.name.empty())
          bind.name = StringFormat::Fmt("cbuffer%u", bind.identifier);

        const Type *cbufType = r->children[(size_t)ResField::VarDecl]->type;

        // variable should be a pointer to the cbuffer type
        RDCASSERT(cbufType->type == Type::Pointer);
        cbufType = cbufType->inner;

        if(!typeInfo.structData.empty())
        {
          CBufferVariableType rootType = MakeCBufferVariableType(typeInfo, cbufType);

          bind.variables.swap(rootType.members);
        }
        else
        {
          CBufferVariable var;

          var.name = "unknown";
          var.offset = 0;

          // if we don't have type annotations, create a dummy struct member
          var.type.descriptor.bytesize = bind.descriptor.byteSize / 16;
          var.type.descriptor.cols = 4;
          var.type.descriptor.rows = 1;
          var.type.descriptor.elements = bind.descriptor.byteSize / 16;
          var.type.descriptor.varClass = DXBC::CLASS_SCALAR;
          var.type.descriptor.varType = VarType::UInt;

          uint32_t remainingBytes = var.type.descriptor.bytesize * 16;

          bind.variables.push_back(var);

          // add any remaining bytes if the struct isn't a multiple of float4 size
          var.type.descriptor.cols = 1;
          var.type.descriptor.bytesize = var.type.descriptor.elements = 1;
          var.type.descriptor.varType = VarType::UByte;

          for(; remainingBytes < bind.descriptor.byteSize; remainingBytes++)
          {
            bind.variables.push_back(var);
          }
        }

        refl->CBuffers.push_back(bind);
      }
    }

    const Metadata *Samplers = resList->children[3];
    if(Samplers)
    {
      for(const Metadata *r : Samplers->children)
      {
        ShaderInputBind bind;
        bind.name = r->children[(size_t)ResField::Name]->str;
        bind.space = getival<uint32_t>(r->children[(size_t)ResField::Space]);
        bind.reg = getival<uint32_t>(r->children[(size_t)ResField::RegBase]);
        bind.bindCount = getival<uint32_t>(r->children[(size_t)ResField::RegCount]);
        bind.type = ShaderInputBind::TYPE_SAMPLER;
        bind.dimension = ShaderInputBind::DIM_UNKNOWN;
        bind.numComps = 0;
        bind.retType = RETURN_TYPE_UNKNOWN;

        if(bind.name.empty())
          bind.name =
              StringFormat::Fmt("sampler%u", getival<uint32_t>(r->children[(size_t)ResField::ID]));

        refl->Samplers.push_back(bind);
      }
    }
  }

  RDCEraseEl(refl->Interfaces);
  RDCEraseEl(refl->DispatchThreadsDimension);

  return refl;
}

void Program::GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const
{
  lineInfo = LineColumnInfo();

  for(const Function &f : m_Functions)
  {
    if(instruction < f.instructions.size())
    {
      lineInfo.disassemblyLine = f.instructions[instruction].disassemblyLine;
      break;
    }
    instruction -= f.instructions.size();
  }
}

void Program::GetCallstack(size_t instruction, uintptr_t offset, rdcarray<rdcstr> &callstack) const
{
  callstack.clear();
}

bool Program::HasSourceMapping() const
{
  // not yet implemented and only relevant for debugging
  return false;
}

void Program::GetLocals(const DXBC::DXBCContainer *dxbc, size_t instruction, uintptr_t offset,
                        rdcarray<SourceVariableMapping> &locals) const
{
  locals.clear();
}

};    // namespace DXIL
