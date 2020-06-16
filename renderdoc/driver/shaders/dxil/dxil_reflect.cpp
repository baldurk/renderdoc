/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

enum class ResShape
{
  Unknown = 0,
  Texture1D,
  Texture2D,
  Texture2DMS,
  Texture3D,
  TextureCube,
  Texture1DArray,
  Texture2DArray,
  Texture2DMSArray,
  TextureCubeArray,
  TypedBuffer,
  RawBuffer,
  StructuredBuffer,
  CBuffer,
  Sampler,
  TBuffer,
  RTAccelerationStructure,
  FeedbackTexture2D,
  FeedbackTexture2DArray,
  StructuredBufferWithCounter,
  SamplerComparison,
};

enum class ComponentType
{
  Invalid = 0,
  I1,
  I16,
  U16,
  I32,
  U32,
  I64,
  U64,
  F16,
  F32,
  F64,
  SNormF16,
  UNormF16,
  SNormF32,
  UNormF32,
  SNormF64,
  UNormF64,
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
  return T(m->val->val.uv[0]);
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
        RDCASSERT(tags.children[t]->value);
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
        RDCASSERT(tags.children[t]->value);
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

  DXMeta(const rdcarray<NamedMetadata> &namedMeta)
  {
    DXMeta &dx = *this;

    for(size_t i = 0; i < namedMeta.size(); i++)
    {
#define GRAB_META(metaname)          \
  if(namedMeta[i].name == #metaname) \
    metaname = &namedMeta[i];

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
    RDCASSERT(typeAnnotations->children.size() >= 2, typeAnnotations->children.size());
    const Metadata *structAnnotations = typeAnnotations->children[0];

    RDCASSERTEQUAL(getival<uint32_t>(structAnnotations->children[0]), 0);

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
    return ret;
  }
  else if(t->type == Type::Array)
  {
    ret = MakeCBufferVariableType(typeInfo, t->inner);
    ret.descriptor.elements = t->elemCount;
    // assume normal D3D array packing with each element on float4 boundary
    ret.descriptor.bytesize += (t->elemCount - 1) * 16;
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

  // if there are no members, return straight away
  if(t->members.empty())
    return ret;

  auto it = typeInfo.structData.find(t);

  if(it != typeInfo.structData.end())
  {
    ret.descriptor.bytesize = it->second.byteSize;
    ret.descriptor.name = t->name;
    if(ret.descriptor.name.beginsWith("struct."))
      ret.descriptor.name.erase(0, 7);
    if(ret.descriptor.name.beginsWith("class."))
      ret.descriptor.name.erase(0, 6);
    ret.descriptor.varType = VarType::Unknown;
    ret.descriptor.varClass = CLASS_STRUCT;
  }
  else
  {
    RDCERR("Don't have struct type annotations for %s", t->name.c_str());
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
      // TODO if we have to handle this case, we should try to calculate the offset
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

  const bool uav = !srv;

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

  for(size_t t = 0; tags && t < tags->children.size(); t += 2)
  {
    RDCASSERT(tags->children[t]->value);
    if(getival<SRVUAVTag>(tags->children[t]) == SRVUAVTag::ElementType)
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

  ResShape shape = srv ? getival<ResShape>(r->children[(size_t)ResField::SRVShape])
                       : getival<ResShape>(r->children[(size_t)ResField::UAVShape]);

  switch(shape)
  {
    case ResShape::Unknown:
    case ResShape::SamplerComparison:
    case ResShape::RTAccelerationStructure:
    case ResShape::CBuffer:
    case ResShape::Sampler:
    case ResShape::FeedbackTexture2D:
    case ResShape::FeedbackTexture2DArray:
      RDCERR("Unexpected %s shape %u", srv ? "SRV" : "UAV", shape);
      break;
    case ResShape::Texture1D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE1D;
      break;
    case ResShape::Texture2D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE2D;
      break;
    case ResShape::Texture2DMS:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE2DMS;
      break;
    case ResShape::Texture3D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE3D;
      break;
    case ResShape::TextureCube:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURECUBE;
      break;
    case ResShape::Texture1DArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE1DARRAY;
      break;
    case ResShape::Texture2DArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE2DARRAY;
      break;
    case ResShape::Texture2DMSArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURE2DMSARRAY;
      break;
    case ResShape::TextureCubeArray:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_TEXTURECUBEARRAY;
      break;
    case ResShape::TypedBuffer:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      break;
    case ResShape::TBuffer:
      bind.type = ShaderInputBind::TYPE_TBUFFER;
      bind.dimension = ShaderInputBind::DIM_UNKNOWN;
      bind.retType = RETURN_TYPE_UNKNOWN;
      break;
    case ResShape::RawBuffer:
      bind.type = ShaderInputBind::TYPE_BYTEADDRESS;
      bind.type = srv ? ShaderInputBind::TYPE_BYTEADDRESS : ShaderInputBind::TYPE_UAV_RWBYTEADDRESS;
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      bind.retType = RETURN_TYPE_MIXED;
      break;
    case ResShape::StructuredBuffer:
      bind.type = srv ? ShaderInputBind::TYPE_STRUCTURED : ShaderInputBind::TYPE_UAV_RWSTRUCTURED;
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      bind.retType = RETURN_TYPE_MIXED;
      break;
    case ResShape::StructuredBufferWithCounter:
      bind.type = srv ? ShaderInputBind::TYPE_STRUCTURED
                      : ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
      bind.dimension = ShaderInputBind::DIM_BUFFER;
      bind.retType = RETURN_TYPE_MIXED;
      break;
  }

  if(bind.type == ShaderInputBind::TYPE_UAV_RWSTRUCTURED)
  {
    if(getival<uint32_t>(r->children[(size_t)ResField::UAVHiddenCounter]) != 0)
      bind.type = ShaderInputBind::TYPE_UAV_RWSTRUCTURED_WITH_COUNTER;
  }

  switch(shape)
  {
    case ResShape::StructuredBuffer:
    case ResShape::StructuredBufferWithCounter:
      refl->ResourceBinds[bind.name] = MakeCBufferVariableType(typeInfo, baseType->inner);
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

        const Type *cbufType = r->children[(size_t)ResField::VarDecl]->type;

        // variable should be a pointer to the cbuffer type
        RDCASSERT(cbufType->type == Type::Pointer);
        cbufType = cbufType->inner;

        CBufferVariableType rootType = MakeCBufferVariableType(typeInfo, cbufType);

        bind.variables.swap(rootType.members);

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
        refl->Samplers.push_back(bind);
      }
    }
  }

  return refl;
}

};    // namespace DXIL
