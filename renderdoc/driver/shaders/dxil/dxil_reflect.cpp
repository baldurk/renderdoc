/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
enum class ResourcesTag
{
  // SRV & UAV
  ElementType = 0,
  StructStride = 1,
  // UAV
  SamplerFeedbackKind = 2,
  Atomic64Use = 3,

  // CBuffer
  IsTBufferTag = 0,
  // Sampler
};

namespace SignatureElement
{
const uint32_t ID = 0;
const uint32_t Name = 1;
const uint32_t Type = 2;
const uint32_t SystemValue = 3;
const uint32_t IndexVector = 4;
const uint32_t InterpMode = 5;
const uint32_t Rows = 6;
const uint32_t Cols = 7;
const uint32_t StartRow = 8;
const uint32_t StartCol = 9;
const uint32_t NameValueList = 10;
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
  Precise = 8,
  CBUsed = 9,
  // ResourceProperties = 10,
  // BitFields = 11,
  FieldWidth = 12,
  VectorSize = 13,
};

template <typename T>
T getival(const Metadata *m)
{
  Constant *c = cast<Constant>(m->value);
  if(c && c->isLiteral())
    return T(c->getU32());
  return T();
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

  DXMeta(const rdcarray<NamedMetadata *> &namedMeta)
  {
    DXMeta &dx = *this;
    DXMeta &llvm = *this;

    for(size_t i = 0; i < namedMeta.size(); i++)
    {
#define GRAB_META(metaname)           \
  if(namedMeta[i]->name == #metaname) \
    metaname = namedMeta[i];

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
      Precise = 0x10,
      CBUsed = 0x20,
    } flags = None;
    uint8_t rows = 0, cols = 0;
    uint32_t offset;
    rdcstr name;
    rdcstr semantic;
    ComponentType type;
    uint32_t fieldWidth;
    uint32_t vectorSize;
  };

  struct StructData
  {
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
            case StructMemberAnnotation::SemanticString:
              memberOut.semantic = memberIn->children[tag + 1]->str;
              break;
            case StructMemberAnnotation::InterpolationMode: break;
            case StructMemberAnnotation::FieldName:
              memberOut.name = memberIn->children[tag + 1]->str;
              break;
            case StructMemberAnnotation::CompType:
              memberOut.type = getival<ComponentType>(memberIn->children[tag + 1]);
              break;
            case StructMemberAnnotation::Precise:
            {
              if(getival<uint32_t>(memberIn->children[tag + 1]) != 0)
                memberOut.flags = MemberData::Flags(memberOut.flags | MemberData::Precise);
              break;
            }
            case StructMemberAnnotation::CBUsed:
            {
              if(getival<uint32_t>(memberIn->children[tag + 1]) != 0)
                memberOut.flags = MemberData::Flags(memberOut.flags | MemberData::CBUsed);
              break;
            }
            case StructMemberAnnotation::FieldWidth:
              memberOut.fieldWidth = getival<uint32_t>(memberIn->children[tag + 1]);
              break;
            case StructMemberAnnotation::VectorSize:
              memberOut.vectorSize = getival<uint32_t>(memberIn->children[tag + 1]);
              break;
            default: RDCWARN("Unexpected field tag %u", fieldTag); break;
          }
        }
      }
    }
  }
};

EntryPointInterface::Signature::Signature(const Metadata *signature)
{
  /*
  // Extended properties
  static const unsigned kDxilSignatureElementOutputStreamTag = 0;
  static const unsigned kHLSignatureElementGlobalSymbolTag = 1;
  static const unsigned kDxilSignatureElementDynIdxCompMaskTag = 2;
  static const unsigned kDxilSignatureElementUsageCompMaskTag = 3;
  */

  name = signature->children[SignatureElement::Name]->str;
  type = getival<ComponentType>(signature->children[SignatureElement::Type]);
  interpolation = getival<D3D_INTERPOLATION_MODE>(signature->children[SignatureElement::InterpMode]);
  rows = getival<uint32_t>(signature->children[SignatureElement::Rows]);
  cols = getival<uint8_t>(signature->children[SignatureElement::Cols]);
  startRow = getival<int32_t>(signature->children[SignatureElement::StartRow]);
  startCol = getival<int8_t>(signature->children[SignatureElement::StartCol]);
}

EntryPointInterface::ResourceBase::ResourceBase(ResourceClass resourceClass,
                                                const Metadata *resourceBase)
    : resClass(resourceClass)
{
  id = getival<uint32_t>(resourceBase->children[(size_t)ResField::ID]);
  type = resourceBase->children[(size_t)ResField::VarDecl]->type;
  name = resourceBase->children[(size_t)ResField::Name]->str;
  space = getival<uint32_t>(resourceBase->children[(size_t)ResField::Space]);
  regBase = getival<uint32_t>(resourceBase->children[(size_t)ResField::RegBase]);
  regCount = getival<uint32_t>(resourceBase->children[(size_t)ResField::RegCount]);
}

EntryPointInterface::SRV::SRV(const Metadata *srv) : ResourceBase(ResourceClass::SRV, srv)
{
  shape = getival<ResourceKind>(srv->children[(size_t)ResField::SRVShape]);
  sampleCount = getival<uint32_t>(srv->children[(size_t)ResField::SRVSampleCount]);
  const Metadata *tags = srv->children[(size_t)ResField::SRVTags];
  for(size_t t = 0; tags && t < tags->children.size(); t += 2)
  {
    RDCASSERT(tags->children[t]->isConstant);
    ResourcesTag tag = getival<ResourcesTag>(tags->children[t]);
    switch(tag)
    {
      case ResourcesTag::ElementType:
        compType = getival<ComponentType>(tags->children[t + 1]);
        break;
      case ResourcesTag::StructStride:
        elementStride = getival<uint32_t>(tags->children[t + 1]);
        break;
      default: break;
    }
  }
}

EntryPointInterface::UAV::UAV(const Metadata *uav) : ResourceBase(ResourceClass::UAV, uav)
{
  shape = getival<ResourceKind>(uav->children[(size_t)ResField::UAVShape]);
  globallCoherent = (getival<uint32_t>(uav->children[(size_t)ResField::UAVGloballyCoherent]) == 1);
  hasCounter = (getival<uint32_t>(uav->children[(size_t)ResField::UAVHiddenCounter]) == 1);
  rasterizerOrderedView = (getival<uint32_t>(uav->children[(size_t)ResField::UAVRasterOrder]) == 1);
  const Metadata *tags = uav->children[(size_t)ResField::UAVTags];
  for(size_t t = 0; tags && t < tags->children.size(); t += 2)
  {
    RDCASSERT(tags->children[t]->isConstant);
    ResourcesTag tag = getival<ResourcesTag>(tags->children[t]);
    switch(tag)
    {
      case ResourcesTag::ElementType:
        compType = getival<ComponentType>(tags->children[t + 1]);
        break;
      case ResourcesTag::StructStride:
        elementStride = getival<uint32_t>(tags->children[t + 1]);
        break;
      case ResourcesTag::SamplerFeedbackKind:
        samplerFeedback = getival<SamplerFeedbackType>(tags->children[t + 1]);
        break;
      case ResourcesTag::Atomic64Use:
        atomic64Use = (getival<uint32_t>(tags->children[t + 1]) == 1);
        break;
      default: break;
    }
  }
}

EntryPointInterface::CBuffer::CBuffer(const Metadata *cbuffer)
    : ResourceBase(ResourceClass::CBuffer, cbuffer)
{
  sizeInBytes = getival<uint32_t>(cbuffer->children[(size_t)ResField::CBufferByteSize]);
  const Metadata *tags = cbuffer->children[(size_t)ResField::CBufferTags];
  for(size_t t = 0; tags && t < tags->children.size(); t += 2)
  {
    RDCASSERT(tags->children[t]->isConstant);
    ResourcesTag tag = getival<ResourcesTag>(tags->children[t]);
    if(tag == ResourcesTag::IsTBufferTag)
      isTBuffer = (getival<uint32_t>(tags->children[t + 1]) == 1);
  }
  cbufferRefl = NULL;
}

EntryPointInterface::Sampler::Sampler(const Metadata *sampler)
    : ResourceBase(ResourceClass::Sampler, sampler)
{
  samplerType = getival<SamplerKind>(sampler->children[(size_t)ResField::SamplerType]);
}

EntryPointInterface::EntryPointInterface(const Metadata *entryPoint)
{
  if(entryPoint->children[0] == NULL)
    return;

  function = entryPoint->children[0]->type;
  name = entryPoint->children[1]->str;

  const Metadata *signatures = entryPoint->children[2];
  if(signatures)
  {
    const Metadata *ins = signatures->children[0];
    if(ins)
    {
      for(size_t i = 0; i < ins->children.size(); ++i)
        inputs.push_back(ins->children[i]);
    }
    const Metadata *outs = signatures->children[1];
    if(outs)
    {
      for(size_t i = 0; i < outs->children.size(); ++i)
        outputs.push_back(outs->children[i]);
    }
    const Metadata *patchCons = signatures->children[2];
    if(patchCons)
    {
      for(size_t i = 0; i < patchCons->children.size(); ++i)
        patchConstants.push_back(patchCons->children[i]);
    }
  }

  // SRVs, UAVs, CBs, Samplers
  const Metadata *resources = entryPoint->children[3];
  if(resources)
  {
    const Metadata *srvsMeta = resources->children[0];
    if(srvsMeta)
    {
      for(size_t i = 0; i < srvsMeta->children.size(); ++i)
        srvs.push_back(srvsMeta->children[i]);
    }

    const Metadata *uavsMeta = resources->children[1];
    if(uavsMeta)
    {
      for(size_t i = 0; i < uavsMeta->children.size(); ++i)
        uavs.push_back(uavsMeta->children[i]);
    }
    const Metadata *cbuffersMeta = resources->children[2];
    if(cbuffersMeta)
    {
      for(size_t i = 0; i < cbuffersMeta->children.size(); ++i)
        cbuffers.push_back(cbuffersMeta->children[i]);
    }
    const Metadata *samplersMeta = resources->children[3];
    if(samplersMeta)
    {
      for(size_t i = 0; i < samplersMeta->children.size(); ++i)
        samplers.push_back(samplersMeta->children[i]);
    }
  }
  /*
  static const unsigned kDxilShaderFlagsTag = 0;
  static const unsigned kDxilGSStateTag = 1;
  static const unsigned kDxilDSStateTag = 2;
  static const unsigned kDxilHSStateTag = 3;
  static const unsigned kDxilNumThreadsTag = 4;
  static const unsigned kDxilAutoBindingSpaceTag = 5;
  static const unsigned kDxilRayPayloadSizeTag = 6;
  static const unsigned kDxilRayAttribSizeTag = 7;
  static const unsigned kDxilShaderKindTag = 8;
  static const unsigned kDxilMSStateTag = 9;
  static const unsigned kDxilASStateTag = 10;
  static const unsigned kDxilWaveSizeTag = 11;
  static const unsigned kDxilEntryRootSigTag = 12;
  static const unsigned kDxilNodeLaunchTypeTag = 13;
  static const unsigned kDxilNodeIsProgramEntryTag = 14;
  static const unsigned kDxilNodeIdTag = 15;
  static const unsigned kDxilNodeLocalRootArgumentsTableIndexTag = 16;
  static const unsigned kDxilShareInputOfTag = 17;
  static const unsigned kDxilNodeDispatchGridTag = 18;
  static const unsigned kDxilNodeMaxRecursionDepthTag = 19;
  static const unsigned kDxilNodeInputsTag = 20;
  static const unsigned kDxilNodeOutputsTag = 21;
  static const unsigned kDxilNodeMaxDispatchGridTag = 22;
  static const unsigned kDxilRangedWaveSizeTag = 23;
  */
  const Metadata *properties = entryPoint->children[4];
  if(properties)
  {
  }
}

static DXBC::CBufferVariableType MakePayloadType(const TypeInfo &typeInfo, const Type *t)
{
  using namespace DXBC;

  CBufferVariableType ret = {};

  ret.elements = 1;

  if(t->type == Type::Scalar || t->type == Type::Vector)
  {
    ret.rows = ret.cols = 1;
    if(t->type == Type::Vector)
      ret.cols = t->elemCount;
    ret.bytesize = (t->bitWidth / 8) * ret.cols;
    ret.varClass = CLASS_SCALAR;

    if(t->scalarType == Type::Float)
    {
      if(t->bitWidth > 32)
        ret.varType = VarType::Double;
      else if(t->bitWidth == 16)
        ret.varType = VarType::Half;
      else
        ret.varType = VarType::Float;
    }
    else
    {
      // can't distinguish int/uint here, default to signed
      if(t->bitWidth > 32)
        ret.varType = VarType::SLong;
      else if(t->bitWidth == 32)
        ret.varType = VarType::SInt;
      else if(t->bitWidth == 16)
        ret.varType = VarType::SShort;
      else if(t->bitWidth == 8)
        ret.varType = VarType::SByte;
      else if(t->bitWidth == 1)
        ret.varType = VarType::Bool;
    }

    ret.name = ToStr(ret.varType);
    if(t->type == Type::Vector)
      ret.name += ToStr(ret.cols);
  }
  else if(t->type == Type::Array)
  {
    ret = MakePayloadType(typeInfo, t->inner);
    ret.elements *= RDCMAX(1U, t->elemCount);
    ret.bytesize += (ret.elements - 1) * ret.bytesize;
  }
  else if(t->type == Type::Struct)
  {
    ret.name = t->name;
    ret.varType = VarType::Unknown;
    ret.varClass = CLASS_STRUCT;

    auto it = typeInfo.structData.find(t);

    char structPrefix[] = "struct.";
    if(ret.name.beginsWith(structPrefix))
      ret.name.erase(0, sizeof(structPrefix) - 1);

    char classPrefix[] = "class.";
    if(ret.name.beginsWith(classPrefix))
      ret.name.erase(0, sizeof(classPrefix) - 1);

    for(size_t i = 0; i < t->members.size(); i++)
    {
      ret.members.push_back({});
      ret.members.back().type = MakePayloadType(typeInfo, t->members[i]);

      if(it != typeInfo.structData.end())
        ret.members.back().name = it->second.members[i].name;
      else
        ret.members.back().name = StringFormat::Fmt("member%zu", i);

      ret.bytesize += ret.members.back().type.bytesize;
    }
  }
  else
  {
    RDCERR("Unexpected type %u iterating cbuffer variable type %s", t->type, t->name.c_str());
  }
  return ret;
}

void Program::FillEntryPointInterfaces()
{
  if(!m_EntryPointInterfaces.isEmpty())
    return;

  DXMeta dx(m_NamedMeta);

  m_EntryPointInterfaces.clear();
  if(!dx.entryPoints)
    return;

  for(size_t c = 0; c < dx.entryPoints->children.size(); ++c)
    m_EntryPointInterfaces.emplace_back(dx.entryPoints->children[c]);
}

void Program::FetchComputeProperties(DXBC::Reflection *reflection)
{
  DXMeta dx(m_NamedMeta);

  TypeInfo typeInfo(dx.typeAnnotations);

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &f = *m_Functions[i];

    // Match "dx.op.threadIdGroup" before "dx.op.threadId"
    if(f.name.beginsWith("dx.op.threadIdInGroup"))
    {
      SigParameter param;
      param.systemValue = ShaderBuiltin::GroupThreadIndex;
      param.compCount = 3;
      param.regChannelMask = param.channelUsedMask = 0x7;
      param.semanticIdxName = param.semanticName = "threadIdInGroup";
      reflection->InputSig.push_back(param);
    }
    else if(f.name.beginsWith("dx.op.threadId"))
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
    else if(f.name.beginsWith("dx.op.flattenedThreadIdInGroup"))
    {
      SigParameter param;
      param.systemValue = ShaderBuiltin::GroupFlatIndex;
      param.compCount = 1;
      param.regChannelMask = param.channelUsedMask = 0x1;
      param.semanticIdxName = param.semanticName = "flattenedThreadIdInGroup";
      reflection->InputSig.push_back(param);
    }

    if(m_Type == DXBC::ShaderType::Amplification)
    {
      for(const Instruction *in : f.instructions)
      {
        const Instruction &inst = *in;

        if(inst.op == Operation::Call && inst.getFuncCall()->name.beginsWith("dx.op.dispatchMesh"))
        {
          if(inst.args.size() != 5)
          {
            RDCERR("Unexpected number of arguments to dispatchMesh");
            continue;
          }

          Type *payloadType = NULL;

          GlobalVar *payloadVariable = cast<GlobalVar>(inst.args[4]);
          if(payloadVariable)
          {
            payloadType = (Type *)payloadVariable->type;
          }
          else
          {
            Instruction *payloadAlloc = cast<Instruction>(inst.args[4]);

            if(payloadAlloc->op == Operation::Alloca || payloadAlloc->op == Operation::GetElementPtr)
            {
              payloadType = (Type *)payloadAlloc->type;
            }
            else
            {
              RDCERR("Unexpected non-variable payload argument to dispatchMesh");
              continue;
            }
          }

          RDCASSERT(payloadType->type == Type::Pointer);
          payloadType = (Type *)payloadType->inner;

          reflection->TaskPayload = MakePayloadType(typeInfo, payloadType);

          break;
        }
      }
    }
  }

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    const NamedMetadata &m = *m_NamedMeta[i];
    if(m.name == "dx.entryPoints")
    {
      // expect only one child for this, DX doesn't support multiple entry points for compute
      // shaders
      RDCASSERTEQUAL(m.children.size(), 1);
      Metadata &entry = *m.children[0];
      RDCASSERTEQUAL(entry.children.size(), 5);
      Metadata &tags = *entry.children[4];

      for(size_t t = 0; t < tags.children.size(); t += 2)
      {
        RDCASSERT(tags.children[t]->isConstant);
        ShaderEntryTag shaderTypeTag = getival<ShaderEntryTag>(tags.children[t]);
        if(shaderTypeTag == ShaderEntryTag::Compute)
        {
          Metadata &threadDim = *tags.children[t + 1];
          RDCASSERTEQUAL(threadDim.children.size(), 3);
          reflection->DispatchThreadsDimension[0] = getival<uint32_t>(threadDim.children[0]);
          reflection->DispatchThreadsDimension[1] = getival<uint32_t>(threadDim.children[1]);
          reflection->DispatchThreadsDimension[2] = getival<uint32_t>(threadDim.children[2]);
          return;
        }
        else if(shaderTypeTag == ShaderEntryTag::Amplification)
        {
          Metadata &ampData = *tags.children[t + 1];
          Metadata &threadDim = *ampData.children[0];
          RDCASSERTEQUAL(threadDim.children.size(), 3);
          reflection->DispatchThreadsDimension[0] = getival<uint32_t>(threadDim.children[0]);
          reflection->DispatchThreadsDimension[1] = getival<uint32_t>(threadDim.children[1]);
          reflection->DispatchThreadsDimension[2] = getival<uint32_t>(threadDim.children[2]);
          return;
        }
        else if(shaderTypeTag == ShaderEntryTag::Mesh)
        {
          Metadata &meshData = *tags.children[t + 1];
          Metadata &threadDim = *meshData.children[0];
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

void Program::FillRayPayloads(
    Program *executable,
    rdcflatmap<ShaderEntryPoint, rdcpair<DXBC::CBufferVariableType, DXBC::CBufferVariableType>> &rayPayloads)
{
  if(m_Type != DXBC::ShaderType::Library)
    return;

  DXMeta dx(m_NamedMeta);

  TypeInfo typeInfo(dx.typeAnnotations);

  if(dx.entryPoints)
  {
    for(Metadata *entry : dx.entryPoints->children)
    {
      if(entry->children.size() > 2 && entry->children[0] != NULL)
      {
        ShaderEntryPoint entryPoint;
        entryPoint.name = entry->children[1]->str;

        Metadata *tags = entry->children[4];

        for(size_t i = 0; i < tags->children.size(); i += 2)
        {
          // 8 is the type tag
          if(getival<uint32_t>(tags->children[i]) == 8U)
          {
            entryPoint.stage =
                GetShaderStage((DXBC::ShaderType)getival<uint32_t>(tags->children[i + 1]));
            break;
          }
        }

        Function *ownFunc = cast<Function>(entry->children[0]->value);
        Function *executableFunc = NULL;

        // locate the function in the executable program so we can iterate instructions.
        for(Function *f : executable->m_Functions)
        {
          // assume names will match
          if(f->name == ownFunc->name)
          {
            executableFunc = f;
            break;
          }
        }

        // intersection shaders only report attributes, they do not access the ray payload
        if(entryPoint.stage == ShaderStage::Intersection)
        {
          // find the reportHit and grab the type from that
          for(const Instruction *in : executableFunc->instructions)
          {
            const Instruction &inst = *in;

            if(inst.op == Operation::Call && inst.getFuncCall()->name.beginsWith("dx.op.reportHit"))
            {
              if(inst.args.size() != 4)
              {
                RDCERR("Unexpected number of arguments to reportHit");
                continue;
              }
              const Type *executableAttrType = inst.args[3]->type;
              if(!executableAttrType)
              {
                RDCERR("Unexpected untyped payload argument to reportHit");
                continue;
              }

              RDCASSERT(executableAttrType->type == Type::Pointer);
              executableAttrType = (Type *)executableAttrType->inner;

              Type *ownAttrType = NULL;

              // we have the executable type but we can't use that to look up our type info. Try to
              // go back by name
              for(Type *t : m_Types)
              {
                if(t->type == executableAttrType->type && t->name == executableAttrType->name)
                {
                  ownAttrType = t;
                  break;
                }
              }

              if(ownAttrType)
                rayPayloads[entryPoint].second = MakePayloadType(typeInfo, ownAttrType);
              else
                RDCERR("Couldn't find matching attribute type for '%s' by name",
                       executableAttrType->name.c_str());
              break;
            }
          }
        }
        // raygen shaders only use the ray payload, not attributes
        else if(entryPoint.stage == ShaderStage::RayGen)
        {
          // find the reportHit and grab the type from that
          for(const Instruction *in : executableFunc->instructions)
          {
            const Instruction &inst = *in;

            if(inst.op == Operation::Call && inst.getFuncCall()->name.beginsWith("dx.op.traceRay"))
            {
              if(inst.args.size() != 16)
              {
                RDCERR("Unexpected number of arguments to traceRay");
                continue;
              }
              const Type *executablePayloadType = inst.args[15]->type;
              if(!executablePayloadType)
              {
                RDCERR("Unexpected untyped payload argument to traceRay");
                continue;
              }

              RDCASSERT(executablePayloadType->type == Type::Pointer);
              executablePayloadType = (Type *)executablePayloadType->inner;

              Type *ownPayloadType = NULL;

              // we have the executable type but we can't use that to look up our type info. Try to
              // go back by name
              for(Type *t : m_Types)
              {
                if(t->type == executablePayloadType->type && t->name == executablePayloadType->name)
                {
                  ownPayloadType = t;
                  break;
                }
              }

              if(ownPayloadType)
                rayPayloads[entryPoint].first = MakePayloadType(typeInfo, ownPayloadType);
              else
                RDCERR("Couldn't find matching payload type for '%s' by name",
                       executablePayloadType->name.c_str());

              break;
            }
          }
        }
        else if(entryPoint.stage == ShaderStage::Miss || entryPoint.stage == ShaderStage::AnyHit ||
                entryPoint.stage == ShaderStage::ClosestHit)
        {
          const Type *payloadType = ownFunc->type->members[0];
          RDCASSERT(payloadType->type == Type::Pointer);
          payloadType = (Type *)payloadType->inner;

          rdcpair<DXBC::CBufferVariableType, DXBC::CBufferVariableType> &dst =
              rayPayloads[entryPoint];

          // miss shaders only use the payload, any-hit and closest-hit use both. The first
          // parameter is the payload, the second is the attributes
          dst.first = MakePayloadType(typeInfo, payloadType);
          if(entryPoint.stage != ShaderStage::Miss)
          {
            const Type *attrType = ownFunc->type->members[1];
            RDCASSERT(attrType->type == Type::Pointer);
            attrType = (Type *)attrType->inner;

            dst.second = MakePayloadType(typeInfo, attrType);
          }
        }
      }
    }
  }
}

D3D_PRIMITIVE_TOPOLOGY Program::GetOutputTopology()
{
  if(m_Type != DXBC::ShaderType::Geometry && m_Type != DXBC::ShaderType::Domain &&
     m_Type != DXBC::ShaderType::Mesh)
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    const NamedMetadata &m = *m_NamedMeta[i];
    if(m.name == "dx.entryPoints")
    {
      // expect only one child for this, DX doesn't support multiple entry points for compute
      // shaders
      RDCASSERTEQUAL(m.children.size(), 1);
      Metadata &entry = *m.children[0];
      RDCASSERTEQUAL(entry.children.size(), 5);
      Metadata &tags = *entry.children[4];

      for(size_t t = 0; t < tags.children.size(); t += 2)
      {
        RDCASSERT(tags.children[t]->isConstant);
        if(getival<ShaderEntryTag>(tags.children[t]) == ShaderEntryTag::Geometry)
        {
          Metadata &geomData = *tags.children[t + 1];
          RDCASSERTEQUAL(geomData.children.size(), 5);
          return getival<D3D_PRIMITIVE_TOPOLOGY>(geomData.children[3]);
        }
        else if(getival<ShaderEntryTag>(tags.children[t]) == ShaderEntryTag::Domain)
        {
          Metadata &domainData = *tags.children[t + 1];
          RDCASSERTEQUAL(domainData.children.size(), 2);
          // 1 for isoline, 2 for tri and 3 for quad (which outputs tris)
          return getival<uint32_t>(domainData.children[0]) == 1
                     ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
                     : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
        else if(getival<ShaderEntryTag>(tags.children[t]) == ShaderEntryTag::Mesh)
        {
          Metadata &meshData = *tags.children[t + 1];
          RDCASSERTEQUAL(meshData.children.size(), 5);
          // 1 for lines, 2 for tris
          return getival<uint32_t>(meshData.children[3]) == 1 ? D3D_PRIMITIVE_TOPOLOGY_LINELIST
                                                              : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
      }

      break;
    }
  }

  RDCERR("Couldn't find topology tag in shader");

  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

// a struct is empty if it has no members, or all members are empty structs
static bool IsEmptyStruct(const Type *t)
{
  // base case - a non-struct is defined as 'non-empty' to propagate up
  if(t->type != Type::Struct)
    return false;

  // for structs, no members is a trivial empty struct
  if(t->members.empty())
    return true;

  // now recurse.
  // is any member a non-empty struct? if so this is also a non-empty struct
  for(const Type *m : t->members)
    if(!IsEmptyStruct(m))
      return false;

  // no members are non-empty => all members are empty => this is empty
  return true;
}

VarType VarTypeForComponentType(ComponentType compType)
{
  VarType varType;
  switch(compType)
  {
    default:
    case ComponentType::Invalid:
      varType = VarType::Unknown;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
    case ComponentType::I1: varType = VarType::Bool; break;
    case ComponentType::I16: varType = VarType::SShort; break;
    case ComponentType::U16: varType = VarType::UShort; break;
    case ComponentType::I32: varType = VarType::SInt; break;
    case ComponentType::U32: varType = VarType::UInt; break;
    case ComponentType::I64: varType = VarType::SLong; break;
    case ComponentType::U64: varType = VarType::ULong; break;
    case ComponentType::F16: varType = VarType::Half; break;
    case ComponentType::F32: varType = VarType::Float; break;
    case ComponentType::F64: varType = VarType::Double; break;
    case ComponentType::SNormF16:
      varType = VarType::Half;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
    case ComponentType::UNormF16:
      varType = VarType::Half;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
    case ComponentType::SNormF32:
      varType = VarType::Float;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
    case ComponentType::UNormF32:
      varType = VarType::Float;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
    case ComponentType::SNormF64:
      varType = VarType::Double;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
    case ComponentType::UNormF64:
      varType = VarType::Double;
      RDCERR("Unexpected type in cbuffer annotations");
      break;
  }
  return varType;
}

static DXBC::CBufferVariableType MakeCBufferVariableType(const TypeInfo &typeInfo, const Type *t)
{
  using namespace DXBC;

  CBufferVariableType ret = {};

  ret.elements = 1;

  if(t->type == Type::Scalar || t->type == Type::Vector)
  {
    ret.rows = ret.cols = 1;
    if(t->type == Type::Vector)
      ret.cols = t->elemCount;
    ret.bytesize = (t->bitWidth / 8) * ret.cols;
    ret.varClass = CLASS_SCALAR;

    if(t->scalarType == Type::Float)
    {
      if(t->bitWidth > 32)
        ret.varType = VarType::Double;
      else if(t->bitWidth == 16)
        ret.varType = VarType::Half;
      else
        ret.varType = VarType::Float;
    }
    else
    {
      // can't distinguish int/uint here, default to signed
      if(t->bitWidth > 32)
        ret.varType = VarType::SLong;
      else if(t->bitWidth == 32)
        ret.varType = VarType::SInt;
      else if(t->bitWidth == 16)
        ret.varType = VarType::SShort;
      else if(t->bitWidth == 8)
        ret.varType = VarType::SByte;
      else if(t->bitWidth == 1)
        ret.varType = VarType::Bool;
    }

    ret.name = ToStr(ret.varType);
    if(t->type == Type::Vector)
      ret.name += ToStr(ret.cols);

    return ret;
  }
  else if(t->type == Type::Array)
  {
    ret = MakeCBufferVariableType(typeInfo, t->inner);
    ret.elements *= RDCMAX(1U, t->elemCount);
    // assume normal D3D array packing with each element on float4 boundary
    ret.bytesize += (ret.elements - 1) * AlignUp16(ret.bytesize);
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

  ret.name = t->name;
  ret.varType = VarType::Unknown;
  ret.varClass = CLASS_STRUCT;

  char alignmentPrefix[] = "dx.alignment.legacy.";
  if(ret.name.beginsWith(alignmentPrefix))
    ret.name.erase(0, sizeof(alignmentPrefix) - 1);

  char hostlayoutPrefix[] = "hostlayout.";
  if(ret.name.beginsWith(hostlayoutPrefix))
    ret.name.erase(0, sizeof(hostlayoutPrefix) - 1);

  char structPrefix[] = "struct.";
  if(ret.name.beginsWith(structPrefix))
    ret.name.erase(0, sizeof(structPrefix) - 1);

  char classPrefix[] = "class.";
  if(ret.name.beginsWith(classPrefix))
    ret.name.erase(0, sizeof(classPrefix) - 1);

  // if this is an empty struct (including recursion), return straight away
  if(IsEmptyStruct(t))
    return ret;

  // textures declared in a struct that becomes a global uniform could end up here, treat it as an empty struct.
  if(ret.name.beginsWith("Texture2D<"))
    return ret;

  auto it = typeInfo.structData.find(t);

  if(it == typeInfo.structData.end())
  {
    // shouldn't get here if we don't have type information at all
    RDCERR("Couldn't find type information for struct '%s'!", t->name.c_str());
    return ret;
  }

  bool structured = false;
  if(ret.name.contains("StructuredBuffer<"))
  {
    structured = true;

    if(t->members.size() != 1 || it->second.members.size() != 1 || it->second.members[0].name != "h")
    {
      RDCWARN("Structured buffer declaration found but expected single inner handle");

      // otherwise use it as-is and trim off the name in an attempt to make it look normal

      ret.name.trim();

      // remove any outer definition of the type
      if(ret.name.back() == '>')
        ret.name.pop_back();
      else
        RDCERR("Expected closing > in StructuredBuffer type name");

      int idx = ret.name.indexOf('<');
      ret.name.erase(0, idx + 1);
    }
  }

  for(size_t i = 0; i < t->members.size(); i++)
  {
    CBufferVariable var;

    var.name = it->second.members[i].name;
    var.offset = it->second.members[i].offset;

    const Type *inner = t->members[i];
    // unpeel any arrays
    while(inner->type == Type::Array)
      inner = inner->inner;

    if(var.type.members.empty() &&
       (inner->type != Type::Struct || (it->second.members[i].flags & TypeInfo::MemberData::Matrix)))
    {
      switch(it->second.members[i].type)
      {
        case ComponentType::Invalid:
        case ComponentType::SNormF16:
        case ComponentType::UNormF16:
        case ComponentType::SNormF32:
        case ComponentType::UNormF32:
        case ComponentType::SNormF64:
        case ComponentType::UNormF64: RDCERR("Unexpected type in cbuffer annotations"); break;
        default: break;
      }

      var.type.varType = VarTypeForComponentType(it->second.members[i].type);
    }

    if(it->second.members[i].flags & TypeInfo::MemberData::Matrix)
    {
      var.type.elements = 1;
      const Type *matType = t->members[i];
      // unpeel any arrays that aren't the last one (that's the matrix array)
      while(matType->type == Type::Array && matType->inner->type == Type::Array)
      {
        var.type.elements *= matType->elemCount;
        matType = matType->inner;
      }

      RDCASSERT(var.type.varType != VarType::Unknown);

      var.type.rows = it->second.members[i].rows;
      var.type.cols = it->second.members[i].cols;
      var.type.varClass = (it->second.members[i].flags & TypeInfo::MemberData::RowMajor)
                              ? CLASS_MATRIX_ROWS
                              : CLASS_MATRIX_COLUMNS;

      var.type.name = ToStr(var.type.varType);
      var.type.name += ToStr(var.type.rows);
      var.type.name += "x";
      var.type.name += ToStr(var.type.cols);

      // D3D matrices in cbuffers always take up a float4 per row/column.
      uint32_t matrixByteStride = AlignUp16(VarTypeByteSize(var.type.varType));
      if(var.type.varClass == CLASS_MATRIX_ROWS)
        matrixByteStride *= var.type.rows;
      else
        matrixByteStride *= var.type.cols;

      var.type.bytesize = matrixByteStride * var.type.elements;
    }
    else
    {
      var.type = MakeCBufferVariableType(typeInfo, t->members[i]);
    }

    ret.bytesize = var.offset + var.type.bytesize;

    ret.members.push_back(var);
  }

  // silently go into the inner member that's declared in this type as we only care about
  // reflecting that actual structure
  if(structured && t->members.size() == 1 && it->second.members.size() == 1 &&
     it->second.members[0].name == "h")
    return ret.members[0].type;

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
    if(getival<ResourcesTag>(tags->children[t]) == ResourcesTag::StructStride)
    {
      structStride = getival<uint32_t>(tags->children[t + 1]);
    }
    else if(getival<ResourcesTag>(tags->children[t]) == ResourcesTag::ElementType)
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
    case ResourceKind::CBuffer:
    case ResourceKind::Sampler:
      RDCERR("Unexpected %s shape %u", srv ? "SRV" : "UAV", shape);
      defName = srv ? "SRV" : "UAV";
      break;
    case ResourceKind::RTAccelerationStructure:
      bind.type = ShaderInputBind::TYPE_RTAS;
      defName = "RaytracingAccelerationStructure";
      bind.dimension = ShaderInputBind::DIM_RTAS;
      break;
    case ResourceKind::Texture1D:
      bind.type = srv ? ShaderInputBind::TYPE_TEXTURE : ShaderInputBind::TYPE_UAV_RWTYPED;
      defName = srv ? "Texture1D" : "RWTexture1D";
      bind.dimension = ShaderInputBind::DIM_TEXTURE1D;
      break;
    case ResourceKind::FeedbackTexture2D:
    // fallthrough, resource type unhandled right now
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
    case ResourceKind::FeedbackTexture2DArray:
    // fallthrough, resource type unhandled right now
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
        refl->ResourceBinds[bind.name].bytesize = structStride;
        refl->ResourceBinds[bind.name].cols = 1;
        refl->ResourceBinds[bind.name].rows = 1;
        refl->ResourceBinds[bind.name].elements = structStride;
        refl->ResourceBinds[bind.name].varClass = DXBC::CLASS_SCALAR;
        refl->ResourceBinds[bind.name].varType = VarType::UByte;
      }
    }
    default: break;
  }

  if(srv)
    refl->SRVs.push_back(bind);
  else
    refl->UAVs.push_back(bind);
}

rdcarray<ShaderEntryPoint> Program::GetEntryPoints()
{
  rdcarray<ShaderEntryPoint> ret;

  DXMeta dx(m_NamedMeta);

  if(dx.entryPoints)
  {
    for(Metadata *entry : dx.entryPoints->children)
    {
      if(entry->children.size() > 2 && entry->children[0] != NULL)
      {
        ShaderEntryPoint entryPoint;
        entryPoint.name = entry->children[1]->str;

        Metadata *tags = entry->children[4];
        if(tags)
        {
          for(size_t i = 0; i < tags->children.size(); i += 2)
          {
            // 8 is the type tag
            if(getival<uint32_t>(tags->children[i]) == 8U)
            {
              entryPoint.stage =
                  GetShaderStage((DXBC::ShaderType)getival<uint32_t>(tags->children[i + 1]));
              break;
            }
          }
        }

        ret.push_back(entryPoint);
      }
    }
  }

  return ret;
}

DXBC::Reflection *Program::GetReflection()
{
  const bool dxcStyleFormatting = m_DXCStyle;
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
    m_CompilerSig += StringFormat::Fmt(
        " (Validation version %s.%s)",
        dx.valver->children[0]->children[0]->value->toString(dxcStyleFormatting).c_str(),
        dx.valver->children[0]->children[1]->value->toString(dxcStyleFormatting).c_str());
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
    m_Profile = StringFormat::Fmt(
        "%s_%s_%s", dx.shaderModel->children[0]->children[0]->str.c_str(),
        dx.shaderModel->children[0]->children[1]->value->toString(dxcStyleFormatting).c_str(),
        dx.shaderModel->children[0]->children[2]->value->toString(dxcStyleFormatting).c_str());
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
          if(Files[i].filename == mainFile)
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
        bind.hasReflectionData = true;

        if(bind.name.empty())
        {
          bind.hasReflectionData = false;
          bind.name = StringFormat::Fmt("cbuffer%u", bind.identifier);
        }

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
          bind.hasReflectionData = false;

          CBufferVariable var;

          var.name = "unknown";
          var.offset = 0;

          // if we don't have type annotations, create a dummy struct member
          var.type.bytesize = bind.descriptor.byteSize / 16;
          var.type.cols = 4;
          var.type.rows = 1;
          var.type.elements = bind.descriptor.byteSize / 16;
          var.type.varClass = DXBC::CLASS_SCALAR;
          var.type.varType = VarType::UInt;

          uint32_t remainingBytes = var.type.bytesize * 16;

          bind.variables.push_back(var);

          // add any remaining bytes if the struct isn't a multiple of float4 size
          var.type.cols = 1;
          var.type.bytesize = var.type.elements = 1;
          var.type.varType = VarType::UByte;

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

rdcstr Program::GetDebugStatus()
{
  return "Debugging DXIL is not supported";
}

void Program::GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const
{
  lineInfo = LineColumnInfo();

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &f = *m_Functions[i];

    if(instruction < f.instructions.size())
    {
      lineInfo.disassemblyLine = f.instructions[instruction]->disassemblyLine;
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

const ResourceReference *Program::GetResourceReference(const rdcstr &handleStr) const
{
  if(m_ResourceHandles.count(handleStr) > 0)
  {
    size_t resRefIndex = m_ResourceHandles.find(handleStr)->second;
    if(resRefIndex < m_ResourceHandles.size())
    {
      return &m_ResourceReferences[resRefIndex];
    }
  }
  return NULL;
}

size_t Program::GetInstructionCount() const
{
  size_t ret = 0;

  for(size_t i = 0; i < m_Functions.size(); i++)
    ret += m_Functions[i]->instructions.size();

  return ret;
}
};    // namespace DXIL
