/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include "common/formatting.h"
#include "maths/half_convert.h"
#include "dxil_bytecode.h"
#include "dxil_common.h"

#if ENABLED(DXC_COMPATIBLE_DISASM) && ENABLED(RDOC_RELEASE)

#error "DXC compatible disassembly should only be enabled in debug builds for testing"

#endif

namespace DXIL
{
struct TypeOrderer
{
  rdcarray<const Type *> types;

  rdcarray<const Type *> visitedTypes = {NULL};
  rdcarray<const Metadata *> visitedMeta;

  void accumulate(const Type *t)
  {
    if(!t || visitedTypes.contains(t))
      return;

    visitedTypes.push_back(t);

    // LLVM doesn't do quite a depth-first search, so we replicate its search order to ensure types
    // are printed in the same order

    rdcarray<const Type *> workingSet;
    workingSet.push_back(t);
    do
    {
      const Type *cur = workingSet.back();
      workingSet.pop_back();

      types.push_back(cur);

      for(size_t i = 0; i < cur->members.size(); i++)
      {
        size_t idx = cur->members.size() - 1 - i;
        if(!visitedTypes.contains(cur->members[idx]))
        {
          visitedTypes.push_back(cur->members[idx]);
          workingSet.push_back(cur->members[idx]);
        }
      }

      if(!visitedTypes.contains(cur->inner))
      {
        visitedTypes.push_back(cur->inner);
        workingSet.push_back(cur->inner);
      }
    } while(!workingSet.empty());
  }

  void accumulate(const Metadata *m)
  {
    auto it = std::lower_bound(visitedMeta.begin(), visitedMeta.end(), m);
    if(it != visitedMeta.end() && *it == m)
      return;
    visitedMeta.insert(it - visitedMeta.begin(), m);

    if(m->type)
      accumulate(m->type);

    if(!m->value.empty())
      accumulate(m->value.GetType());

    for(const Metadata *c : m->children)
      if(c)
        accumulate(c);
  }
};

bool needsEscaping(const rdcstr &name)
{
  return name.find_first_not_of(
             "-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._0123456789") >= 0;
}

rdcstr escapeString(const rdcstr &str)
{
  rdcstr ret;
  ret.push_back('"');

  for(size_t i = 0; i < str.size(); i++)
  {
    if(str[i] == '\r' || str[i] == '\n' || str[i] == '\t' || str[i] == '"' || str[i] == '\\' ||
       !isprint(str[i]))
    {
      ret.push_back('\\');
      ret.append(StringFormat::Fmt("%02X", (unsigned char)str[i]));
      continue;
    }

    ret.push_back(str[i]);
  }

  ret.push_back('"');

  return ret;
}

rdcstr escapeStringIfNeeded(const rdcstr &name)
{
  return needsEscaping(name) ? escapeString(name) : name;
}

void Program::MakeDisassemblyString()
{
  const char *shaderName[] = {
      "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
      "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
      "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
  };

  // clang-format off
  static const char *funcSigs[] = {
    "TempRegLoad(index)",
    "TempRegStore(index,value)",
    "MinPrecXRegLoad(regIndex,index,component)",
    "MinPrecXRegStore(regIndex,index,component,value)",
    "LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)",
    "StoreOutput(outputSigId,rowIndex,colIndex,value)",
    "FAbs(value)",
    "Saturate(value)",
    "IsNaN(value)",
    "IsInf(value)",
    "IsFinite(value)",
    "IsNormal(value)",
    "Cos(value)",
    "Sin(value)",
    "Tan(value)",
    "Acos(value)",
    "Asin(value)",
    "Atan(value)",
    "Hcos(value)",
    "Hsin(value)",
    "Htan(value)",
    "Exp(value)",
    "Frc(value)",
    "Log(value)",
    "Sqrt(value)",
    "Rsqrt(value)",
    "Round_ne(value)",
    "Round_ni(value)",
    "Round_pi(value)",
    "Round_z(value)",
    "Bfrev(value)",
    "Countbits(value)",
    "FirstbitLo(value)",
    "FirstbitHi(value)",
    "FirstbitSHi(value)",
    "FMax(a,b)",
    "FMin(a,b)",
    "IMax(a,b)",
    "IMin(a,b)",
    "UMax(a,b)",
    "UMin(a,b)",
    "IMul(a,b)",
    "UMul(a,b)",
    "UDiv(a,b)",
    "UAddc(a,b)",
    "USubb(a,b)",
    "FMad(a,b,c)",
    "Fma(a,b,c)",
    "IMad(a,b,c)",
    "UMad(a,b,c)",
    "Msad(a,b,c)",
    "Ibfe(a,b,c)",
    "Ubfe(a,b,c)",
    "Bfi(width,offset,value,replacedValue)",
    "Dot2(ax,ay,bx,by)",
    "Dot3(ax,ay,az,bx,by,bz)",
    "Dot4(ax,ay,az,aw,bx,by,bz,bw)",
    "CreateHandle(resourceClass,rangeId,index,nonUniformIndex)",
    "CBufferLoad(handle,byteOffset,alignment)",
    "CBufferLoadLegacy(handle,regIndex)",
    "Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)",
    "SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)",
    "SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)",
    "SampleGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)",
    "SampleCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp)",
    "SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)",
    "TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)",
    "TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)",
    "BufferLoad(srv,index,wot)",
    "BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)",
    "BufferUpdateCounter(uav,inc)",
    "CheckAccessFullyMapped(status)",
    "GetDimensions(handle,mipLevel)",
    "TextureGather(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel)",
    "TextureGatherCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel,compareValue)",
    "Texture2DMSGetSamplePosition(srv,index)",
    "RenderTargetGetSamplePosition(index)",
    "RenderTargetGetSampleCount()",
    "AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)",
    "AtomicCompareExchange(handle,offset0,offset1,offset2,compareValue,newValue)",
    "Barrier(barrierMode)",
    "CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)",
    "Discard(condition)",
    "DerivCoarseX(value)",
    "DerivCoarseY(value)",
    "DerivFineX(value)",
    "DerivFineY(value)",
    "EvalSnapped(inputSigId,inputRowIndex,inputColIndex,offsetX,offsetY)",
    "EvalSampleIndex(inputSigId,inputRowIndex,inputColIndex,sampleIndex)",
    "EvalCentroid(inputSigId,inputRowIndex,inputColIndex)",
    "SampleIndex()",
    "Coverage()",
    "InnerCoverage()",
    "ThreadId(component)",
    "GroupId(component)",
    "ThreadIdInGroup(component)",
    "FlattenedThreadIdInGroup()",
    "EmitStream(streamId)",
    "CutStream(streamId)",
    "EmitThenCutStream(streamId)",
    "GSInstanceID()",
    "MakeDouble(lo,hi)",
    "SplitDouble(value)",
    "LoadOutputControlPoint(inputSigId,row,col,index)",
    "LoadPatchConstant(inputSigId,row,col)",
    "DomainLocation(component)",
    "StorePatchConstant(outputSigID,row,col,value)",
    "OutputControlPointID()",
    "PrimitiveID()",
    "CycleCounterLegacy()",
    "WaveIsFirstLane()",
    "WaveGetLaneIndex()",
    "WaveGetLaneCount()",
    "WaveAnyTrue(cond)",
    "WaveAllTrue(cond)",
    "WaveActiveAllEqual(value)",
    "WaveActiveBallot(cond)",
    "WaveReadLaneAt(value,lane)",
    "WaveReadLaneFirst(value)",
    "WaveActiveOp(value,op,sop)",
    "WaveActiveBit(value,op)",
    "WavePrefixOp(value,op,sop)",
    "QuadReadLaneAt(value,quadLane)",
    "QuadOp(value,op)",
    "BitcastI16toF16(value)",
    "BitcastF16toI16(value)",
    "BitcastI32toF32(value)",
    "BitcastF32toI32(value)",
    "BitcastI64toF64(value)",
    "BitcastF64toI64(value)",
    "LegacyF32ToF16(value)",
    "LegacyF16ToF32(value)",
    "LegacyDoubleToFloat(value)",
    "LegacyDoubleToSInt32(value)",
    "LegacyDoubleToUInt32(value)",
    "WaveAllBitCount(value)",
    "WavePrefixBitCount(value)",
    "AttributeAtVertex(inputSigId,inputRowIndex,inputColIndex,VertexID)",
    "ViewID()",
    "RawBufferLoad(srv,index,elementOffset,mask,alignment)",
    "RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)",
    "InstanceID()",
    "InstanceIndex()",
    "HitKind()",
    "RayFlags()",
    "DispatchRaysIndex(col)",
    "DispatchRaysDimensions(col)",
    "WorldRayOrigin(col)",
    "WorldRayDirection(col)",
    "ObjectRayOrigin(col)",
    "ObjectRayDirection(col)",
    "ObjectToWorld(row,col)",
    "WorldToObject(row,col)",
    "RayTMin()",
    "RayTCurrent()",
    "IgnoreHit()",
    "AcceptHitAndEndSearch()",
    "TraceRay(AccelerationStructure,RayFlags,InstanceInclusionMask,RayContributionToHitGroupIndex,MultiplierForGeometryContributionToShaderIndex,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload)",
    "ReportHit(THit,HitKind,Attributes)",
    "CallShader(ShaderIndex,Parameter)",
    "CreateHandleForLib(Resource)",
    "PrimitiveIndex()",
    "Dot2AddHalf(acc,ax,ay,bx,by)",
    "Dot4AddI8Packed(acc,a,b)",
    "Dot4AddU8Packed(acc,a,b)",
    "WaveMatch(value)",
    "WaveMultiPrefixOp(value,mask0,mask1,mask2,mask3,op,sop)",
    "WaveMultiPrefixBitCount(value,mask0,mask1,mask2,mask3)",
    "SetMeshOutputCounts(numVertices,numPrimitives)",
    "EmitIndices(PrimitiveIndex,VertexIndex0,VertexIndex1,VertexIndex2)",
    "GetMeshPayload()",
    "StoreVertexOutput(outputSigId,rowIndex,colIndex,value,vertexIndex)",
    "StorePrimitiveOutput(outputSigId,rowIndex,colIndex,value,primitiveIndex)",
    "DispatchMesh(threadGroupCountX,threadGroupCountY,threadGroupCountZ,payload)",
    "WriteSamplerFeedback(feedbackTex,sampledTex,sampler,c0,c1,c2,c3,clamp)",
    "WriteSamplerFeedbackBias(feedbackTex,sampledTex,sampler,c0,c1,c2,c3,bias,clamp)",
    "WriteSamplerFeedbackLevel(feedbackTex,sampledTex,sampler,c0,c1,c2,c3,lod)",
    "WriteSamplerFeedbackGrad(feedbackTex,sampledTex,sampler,c0,c1,c2,c3,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)",
    "AllocateRayQuery(constRayFlags)",
    "RayQuery_TraceRayInline(rayQueryHandle,accelerationStructure,rayFlags,instanceInclusionMask,origin_X,origin_Y,origin_Z,tMin,direction_X,direction_Y,direction_Z,tMax)",
    "RayQuery_Proceed(rayQueryHandle)",
    "RayQuery_Abort(rayQueryHandle)",
    "RayQuery_CommitNonOpaqueTriangleHit(rayQueryHandle)",
    "RayQuery_CommitProceduralPrimitiveHit(rayQueryHandle,t)",
    "RayQuery_CommittedStatus(rayQueryHandle)",
    "RayQuery_CandidateType(rayQueryHandle)",
    "RayQuery_CandidateObjectToWorld3x4(rayQueryHandle,row,col)",
    "RayQuery_CandidateWorldToObject3x4(rayQueryHandle,row,col)",
    "RayQuery_CommittedObjectToWorld3x4(rayQueryHandle,row,col)",
    "RayQuery_CommittedWorldToObject3x4(rayQueryHandle,row,col)",
    "RayQuery_CandidateProceduralPrimitiveNonOpaque(rayQueryHandle)",
    "RayQuery_CandidateTriangleFrontFace(rayQueryHandle)",
    "RayQuery_CommittedTriangleFrontFace(rayQueryHandle)",
    "RayQuery_CandidateTriangleBarycentrics(rayQueryHandle,component)",
    "RayQuery_CommittedTriangleBarycentrics(rayQueryHandle,component)",
    "RayQuery_RayFlags(rayQueryHandle)",
    "RayQuery_WorldRayOrigin(rayQueryHandle,component)",
    "RayQuery_WorldRayDirection(rayQueryHandle,component)",
    "RayQuery_RayTMin(rayQueryHandle)",
    "RayQuery_CandidateTriangleRayT(rayQueryHandle)",
    "RayQuery_CommittedRayT(rayQueryHandle)",
    "RayQuery_CandidateInstanceIndex(rayQueryHandle)",
    "RayQuery_CandidateInstanceID(rayQueryHandle)",
    "RayQuery_CandidateGeometryIndex(rayQueryHandle)",
    "RayQuery_CandidatePrimitiveIndex(rayQueryHandle)",
    "RayQuery_CandidateObjectRayOrigin(rayQueryHandle,component)",
    "RayQuery_CandidateObjectRayDirection(rayQueryHandle,component)",
    "RayQuery_CommittedInstanceIndex(rayQueryHandle)",
    "RayQuery_CommittedInstanceID(rayQueryHandle)",
    "RayQuery_CommittedGeometryIndex(rayQueryHandle)",
    "RayQuery_CommittedPrimitiveIndex(rayQueryHandle)",
    "RayQuery_CommittedObjectRayOrigin(rayQueryHandle,component)",
    "RayQuery_CommittedObjectRayDirection(rayQueryHandle,component)",
    "GeometryIndex()",
    "RayQuery_CandidateInstanceContributionToHitGroupIndex(rayQueryHandle)",
    "RayQuery_CommittedInstanceContributionToHitGroupIndex(rayQueryHandle)",
    "CreateHandleFromHeap(index,nonUniformIndex)",
    "AnnotateHandle(res,resourceClass,resourceKind,props)"
  };
  // clang-format on

  m_Disassembly.clear();
#if DISABLED(DXC_COMPATIBLE_DISASM)
  m_Disassembly += StringFormat::Fmt("; %s Shader, compiled under SM%u.%u\n\n",
                                     shaderName[int(m_Type)], m_Major, m_Minor);
#endif
  m_Disassembly += StringFormat::Fmt("target datalayout = \"%s\"\n", m_Datalayout.c_str());
  m_Disassembly += StringFormat::Fmt("target triple = \"%s\"\n\n", m_Triple.c_str());

  int instructionLine = 6;

  TypeOrderer typeOrderer;
  rdcarray<const Type *> orderedTypes;

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = m_GlobalVars[i];
    typeOrderer.accumulate(g.type);

    if(g.initialiser)
      typeOrderer.accumulate(g.initialiser->type);
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = m_Functions[i];

    // the function type will also accumulate the arguments
    typeOrderer.accumulate(func.funcType);

    for(const Instruction &inst : func.instructions)
    {
      typeOrderer.accumulate(inst.type);
      for(size_t a = 0; a < inst.args.size(); a++)
      {
        if(inst.args[a].type != ValueType::Literal && inst.args[a].type != ValueType::BasicBlock)
          typeOrderer.accumulate(inst.args[a].GetType());
      }

      for(size_t m = 0; m < inst.attachedMeta.size(); m++)
        typeOrderer.accumulate(inst.attachedMeta[m].second);
    }
  }

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    typeOrderer.accumulate(&m_NamedMeta[i]);
  }

  bool printedTypes = false;

  for(const Type *typ : typeOrderer.types)
  {
    if(typ->type == Type::Struct && !typ->name.empty())
    {
      rdcstr name = typ->toString();
      m_Disassembly += StringFormat::Fmt("%s = type {", name.c_str());
      bool first = true;
      for(const Type *t : typ->members)
      {
        if(!first)
          m_Disassembly += ",";
        first = false;
        m_Disassembly += StringFormat::Fmt(" %s", t->toString().c_str());
      }
      if(typ->members.empty())
        m_Disassembly += "}\n";
      else
        m_Disassembly += " }\n";

      instructionLine++;
      printedTypes = true;
    }
  }

  if(printedTypes)
  {
    m_Disassembly += "\n";
    instructionLine++;
  }

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = m_GlobalVars[i];

    m_Disassembly += StringFormat::Fmt("@%s = ", escapeStringIfNeeded(g.name).c_str());
    switch(g.flags & GlobalFlags::LinkageMask)
    {
      case GlobalFlags::ExternalLinkage:
        if(!g.initialiser)
          m_Disassembly += "external ";
        break;
      case GlobalFlags::PrivateLinkage: m_Disassembly += "private "; break;
      case GlobalFlags::InternalLinkage: m_Disassembly += "internal "; break;
      case GlobalFlags::LinkOnceAnyLinkage: m_Disassembly += "linkonce "; break;
      case GlobalFlags::LinkOnceODRLinkage: m_Disassembly += "linkonce_odr "; break;
      case GlobalFlags::WeakAnyLinkage: m_Disassembly += "weak "; break;
      case GlobalFlags::WeakODRLinkage: m_Disassembly += "weak_odr "; break;
      case GlobalFlags::CommonLinkage: m_Disassembly += "common "; break;
      case GlobalFlags::AppendingLinkage: m_Disassembly += "appending "; break;
      case GlobalFlags::ExternalWeakLinkage: m_Disassembly += "extern_weak "; break;
      case GlobalFlags::AvailableExternallyLinkage: m_Disassembly += "available_externally "; break;
      default: break;
    }

    if(g.flags & GlobalFlags::LocalUnnamedAddr)
      m_Disassembly += "local_unnamed_addr ";
    else if(g.flags & GlobalFlags::GlobalUnnamedAddr)
      m_Disassembly += "unnamed_addr ";
    if(g.type->addrSpace != Type::PointerAddrSpace::Default)
      m_Disassembly += StringFormat::Fmt("addrspace(%d) ", g.type->addrSpace);
    if(g.flags & GlobalFlags::IsConst)
      m_Disassembly += "constant ";
    else
      m_Disassembly += "global ";

    if(g.initialiser)
      m_Disassembly += g.initialiser->toString(true);
    else
      m_Disassembly += g.type->inner->toString();

    if(g.align > 0)
      m_Disassembly += StringFormat::Fmt(", align %u", g.align);

    if(g.section >= 0)
      m_Disassembly += StringFormat::Fmt(", section %s", escapeString(m_Sections[g.section]).c_str());

    m_Disassembly += "\n";
    instructionLine++;
  }

  if(!m_GlobalVars.empty())
  {
    m_Disassembly += "\n";
    instructionLine++;
  }

  rdcstr namedMeta;

  // need to disassemble the named metadata here so the IDs are assigned first before any functions
  // get dibs
  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    namedMeta += StringFormat::Fmt("!%s = %s!{", m_NamedMeta[i].name.c_str(),
                                   m_NamedMeta[i].isDistinct ? "distinct " : "");
    for(size_t m = 0; m < m_NamedMeta[i].children.size(); m++)
    {
      if(m != 0)
        namedMeta += ", ";
      if(m_NamedMeta[i].children[m])
        namedMeta += StringFormat::Fmt("!%u", GetOrAssignMetaID(m_NamedMeta[i].children[m]));
      else
        namedMeta += "null";
    }

    namedMeta += "}\n";
  }

  rdcarray<const AttributeGroup *> funcAttrGroups;
  for(size_t i = 0; i < m_AttributeGroups.size(); i++)
  {
    if(m_AttributeGroups[i].slotIndex != AttributeGroup::FunctionSlot)
      continue;

    if(funcAttrGroups.contains(&m_AttributeGroups[i]))
      continue;

    funcAttrGroups.push_back(&m_AttributeGroups[i]);
  }

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    Function &func = m_Functions[i];

    auto argToString = [this, &func](Value v, bool withTypes, const rdcstr &attrString = "") {
      rdcstr ret;
      switch(v.type)
      {
        case ValueType::Unknown:
        case ValueType::Alias: ret = "???"; break;
        case ValueType::Literal:
          if(withTypes)
            ret += "i32 ";
          ret += attrString;
          ret += StringFormat::Fmt("%llu", v.literal);
          break;
        case ValueType::Metadata:
          if(withTypes)
            ret += "metadata ";
          ret += attrString;
          if(m_Metadata.begin() <= v.meta && v.meta < m_Metadata.end())
          {
            const Metadata &m = *v.meta;
            if(m.isConstant && m.value.type == ValueType::Constant &&
               (m.value.constant->type->type == Type::Scalar ||
                m.value.constant->type->type == Type::Vector || m.value.constant->undef ||
                m.value.constant->nullconst ||
                m.value.constant->type->name.beginsWith("class.matrix.")))
            {
              ret += m.value.constant->toString(withTypes);
            }
            else if(m.isConstant && m.value.type == ValueType::GlobalVar)
            {
              if(withTypes)
                ret += m.value.global->type->toString() + " ";
              ret += "@" + escapeStringIfNeeded(m.value.global->name);
            }
            else
            {
              ret += StringFormat::Fmt("!%u", GetOrAssignMetaID((Metadata *)&m));
            }
          }
          else
          {
            ret += v.meta->refString();
          }
          break;
        case ValueType::Function:
          ret += attrString;
          ret = "@" + escapeStringIfNeeded(v.function->name);
          break;
        case ValueType::GlobalVar:
          if(withTypes)
            ret = v.global->type->toString() + " ";
          ret += attrString;
          ret += "@" + escapeStringIfNeeded(v.global->name);
          break;
        case ValueType::Constant:
          ret += attrString;
          ret = v.constant->toString(withTypes);
          break;
        case ValueType::Instruction:
        {
          if(withTypes)
            ret = v.instruction->type->toString() + " ";
          ret += attrString;
          if(v.instruction->name.empty())
            ret += StringFormat::Fmt("%%%u", v.instruction->resultID);
          else
            ret += StringFormat::Fmt("%%%s", escapeStringIfNeeded(v.instruction->name).c_str());
          break;
        }
        case ValueType::BasicBlock:
        {
          if(withTypes)
            ret = "label ";
          ret += attrString;
          if(v.block->name.empty())
            ret += StringFormat::Fmt("%%%u", v.block->resultID);
          else
            ret += StringFormat::Fmt("%%%s", escapeStringIfNeeded(v.block->name).c_str());
        }
      }
      return ret;
    };

    if(func.attrs && func.attrs->functionSlot)
    {
      m_Disassembly += StringFormat::Fmt("; Function Attrs: %s\n",
                                         func.attrs->functionSlot->toString(false).c_str());
      instructionLine++;
    }

    m_Disassembly += (func.external ? "declare " : "define ");
    m_Disassembly +=
        func.funcType->declFunction("@" + escapeStringIfNeeded(func.name), func.args, func.attrs);

    if(func.attrs && func.attrs->functionSlot)
      m_Disassembly += StringFormat::Fmt(" #%u", funcAttrGroups.indexOf(func.attrs->functionSlot));

    if(!func.external)
    {
      m_Disassembly += " {\n";
      instructionLine++;

      size_t curBlock = 0;

      // if the first block has a name, use it
      if(!func.blocks[curBlock].name.empty())
      {
        m_Disassembly +=
            StringFormat::Fmt("%s:\n", escapeStringIfNeeded(func.blocks[curBlock].name).c_str());
        instructionLine++;
      }

      for(size_t funcIdx = 0; funcIdx < func.instructions.size(); funcIdx++)
      {
        Instruction &inst = func.instructions[funcIdx];

        inst.disassemblyLine = instructionLine;
        m_Disassembly += "  ";
        if(!inst.name.empty())
          m_Disassembly += "%" + escapeStringIfNeeded(inst.name) + " = ";
        else if(inst.resultID != ~0U)
          m_Disassembly += StringFormat::Fmt("%%%u = ", inst.resultID);

        bool debugCall = false;

        switch(inst.op)
        {
          case Operation::NoOp: m_Disassembly += "??? "; break;
          case Operation::Call:
          {
            m_Disassembly += "call " + inst.type->toString();
            m_Disassembly += " @" + escapeStringIfNeeded(inst.funcCall->name);
            m_Disassembly += "(";
            bool first = true;

            // attribute args start from 1
            size_t argIdx = 1;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";
              first = false;

              // see if we have param attrs for this param
              rdcstr attrString;
              if(inst.paramAttrs && argIdx < inst.paramAttrs->groupSlots.size() &&
                 inst.paramAttrs->groupSlots[argIdx])
              {
                attrString = inst.paramAttrs->groupSlots[argIdx]->toString(true) + " ";
              }

              m_Disassembly += argToString(s, true, attrString);

              argIdx++;
            }
            m_Disassembly += ")";
            debugCall = inst.funcCall->name.beginsWith("llvm.dbg.");

            if(inst.paramAttrs && inst.paramAttrs->functionSlot)
              m_Disassembly +=
                  StringFormat::Fmt(" #%u", funcAttrGroups.indexOf(inst.paramAttrs->functionSlot));
            break;
          }
          case Operation::Trunc:
          case Operation::ZExt:
          case Operation::SExt:
          case Operation::FToU:
          case Operation::FToS:
          case Operation::UToF:
          case Operation::SToF:
          case Operation::FPTrunc:
          case Operation::FPExt:
          case Operation::PtrToI:
          case Operation::IToPtr:
          case Operation::Bitcast:
          case Operation::AddrSpaceCast:
          {
            switch(inst.op)
            {
              case Operation::Trunc: m_Disassembly += "trunc "; break;
              case Operation::ZExt: m_Disassembly += "zext "; break;
              case Operation::SExt: m_Disassembly += "sext "; break;
              case Operation::FToU: m_Disassembly += "fptoui "; break;
              case Operation::FToS: m_Disassembly += "fptosi "; break;
              case Operation::UToF: m_Disassembly += "uitofp "; break;
              case Operation::SToF: m_Disassembly += "sitofp "; break;
              case Operation::FPTrunc: m_Disassembly += "fptrunc "; break;
              case Operation::FPExt: m_Disassembly += "fpext "; break;
              case Operation::PtrToI: m_Disassembly += "ptrtoi "; break;
              case Operation::IToPtr: m_Disassembly += "itoptr "; break;
              case Operation::Bitcast: m_Disassembly += "bitcast "; break;
              case Operation::AddrSpaceCast: m_Disassembly += "addrspacecast "; break;
              default: break;
            }

            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += " to ";
            m_Disassembly += inst.type->toString();
            break;
          }
          case Operation::ExtractVal:
          {
            m_Disassembly += "extractvalue ";
            m_Disassembly += argToString(inst.args[0], true);
            for(size_t n = 1; n < inst.args.size(); n++)
              m_Disassembly += StringFormat::Fmt(", %llu", inst.args[n].literal);
            break;
          }
          case Operation::FAdd:
          case Operation::FSub:
          case Operation::FMul:
          case Operation::FDiv:
          case Operation::FRem:
          case Operation::Add:
          case Operation::Sub:
          case Operation::Mul:
          case Operation::UDiv:
          case Operation::SDiv:
          case Operation::URem:
          case Operation::SRem:
          case Operation::ShiftLeft:
          case Operation::LogicalShiftRight:
          case Operation::ArithShiftRight:
          case Operation::And:
          case Operation::Or:
          case Operation::Xor:
          {
            switch(inst.op)
            {
              case Operation::FAdd: m_Disassembly += "fadd "; break;
              case Operation::FSub: m_Disassembly += "fsub "; break;
              case Operation::FMul: m_Disassembly += "fmul "; break;
              case Operation::FDiv: m_Disassembly += "fdiv "; break;
              case Operation::FRem: m_Disassembly += "frem "; break;
              case Operation::Add: m_Disassembly += "add "; break;
              case Operation::Sub: m_Disassembly += "sub "; break;
              case Operation::Mul: m_Disassembly += "mul "; break;
              case Operation::UDiv: m_Disassembly += "udiv "; break;
              case Operation::SDiv: m_Disassembly += "sdiv "; break;
              case Operation::URem: m_Disassembly += "urem "; break;
              case Operation::SRem: m_Disassembly += "srem "; break;
              case Operation::ShiftLeft: m_Disassembly += "shl "; break;
              case Operation::LogicalShiftRight: m_Disassembly += "lshr "; break;
              case Operation::ArithShiftRight: m_Disassembly += "ashr "; break;
              case Operation::And: m_Disassembly += "and "; break;
              case Operation::Or: m_Disassembly += "or "; break;
              case Operation::Xor: m_Disassembly += "xor "; break;
              default: break;
            }

            rdcstr opFlagsStr = ToStr(inst.opFlags);
            {
              int offs = opFlagsStr.indexOf('|');
              while(offs >= 0)
              {
                opFlagsStr.erase((size_t)offs, 2);
                offs = opFlagsStr.indexOf('|');
              }
            }
            m_Disassembly += opFlagsStr;
            if(inst.opFlags != InstructionFlags::NoFlags)
              m_Disassembly += " ";

            bool first = true;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += argToString(s, first);
              first = false;
            }

            break;
          }
          case Operation::Ret: m_Disassembly += "ret " + inst.type->toString(); break;
          case Operation::Unreachable: m_Disassembly += "unreachable"; break;
          case Operation::Alloca:
          {
            m_Disassembly += "alloca ";
            m_Disassembly += inst.type->inner->toString();
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Operation::GetElementPtr:
          {
            m_Disassembly += "getelementptr ";
            if(inst.opFlags & InstructionFlags::InBounds)
              m_Disassembly += "inbounds ";
            m_Disassembly += inst.args[0].GetType()->inner->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += argToString(s, true);
              first = false;
            }
            break;
          }
          case Operation::Load:
          {
            m_Disassembly += "load ";
            if(inst.opFlags & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += inst.type->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += argToString(s, true);
              first = false;
            }
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Operation::Store:
          {
            m_Disassembly += "store ";
            if(inst.opFlags & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += argToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[0], true);
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Operation::FOrdFalse:
          case Operation::FOrdEqual:
          case Operation::FOrdGreater:
          case Operation::FOrdGreaterEqual:
          case Operation::FOrdLess:
          case Operation::FOrdLessEqual:
          case Operation::FOrdNotEqual:
          case Operation::FOrd:
          case Operation::FUnord:
          case Operation::FUnordEqual:
          case Operation::FUnordGreater:
          case Operation::FUnordGreaterEqual:
          case Operation::FUnordLess:
          case Operation::FUnordLessEqual:
          case Operation::FUnordNotEqual:
          case Operation::FOrdTrue:
          {
            m_Disassembly += "fcmp ";
            rdcstr opFlagsStr = ToStr(inst.opFlags);
            {
              int offs = opFlagsStr.indexOf('|');
              while(offs >= 0)
              {
                opFlagsStr.erase((size_t)offs, 2);
                offs = opFlagsStr.indexOf('|');
              }
            }
            m_Disassembly += opFlagsStr;
            if(inst.opFlags != InstructionFlags::NoFlags)
              m_Disassembly += " ";
            switch(inst.op)
            {
              case Operation::FOrdFalse: m_Disassembly += "false "; break;
              case Operation::FOrdEqual: m_Disassembly += "oeq "; break;
              case Operation::FOrdGreater: m_Disassembly += "ogt "; break;
              case Operation::FOrdGreaterEqual: m_Disassembly += "oge "; break;
              case Operation::FOrdLess: m_Disassembly += "olt "; break;
              case Operation::FOrdLessEqual: m_Disassembly += "ole "; break;
              case Operation::FOrdNotEqual: m_Disassembly += "one "; break;
              case Operation::FOrd: m_Disassembly += "ord "; break;
              case Operation::FUnord: m_Disassembly += "uno "; break;
              case Operation::FUnordEqual: m_Disassembly += "ueq "; break;
              case Operation::FUnordGreater: m_Disassembly += "ugt "; break;
              case Operation::FUnordGreaterEqual: m_Disassembly += "uge "; break;
              case Operation::FUnordLess: m_Disassembly += "ult "; break;
              case Operation::FUnordLessEqual: m_Disassembly += "ule "; break;
              case Operation::FUnordNotEqual: m_Disassembly += "une "; break;
              case Operation::FOrdTrue: m_Disassembly += "true "; break;
              default: break;
            }
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], false);
            break;
          }
          case Operation::IEqual:
          case Operation::INotEqual:
          case Operation::UGreater:
          case Operation::UGreaterEqual:
          case Operation::ULess:
          case Operation::ULessEqual:
          case Operation::SGreater:
          case Operation::SGreaterEqual:
          case Operation::SLess:
          case Operation::SLessEqual:
          {
            m_Disassembly += "icmp ";
            switch(inst.op)
            {
              case Operation::IEqual: m_Disassembly += "eq "; break;
              case Operation::INotEqual: m_Disassembly += "ne "; break;
              case Operation::UGreater: m_Disassembly += "ugt "; break;
              case Operation::UGreaterEqual: m_Disassembly += "uge "; break;
              case Operation::ULess: m_Disassembly += "ult "; break;
              case Operation::ULessEqual: m_Disassembly += "ule "; break;
              case Operation::SGreater: m_Disassembly += "sgt "; break;
              case Operation::SGreaterEqual: m_Disassembly += "sge "; break;
              case Operation::SLess: m_Disassembly += "slt "; break;
              case Operation::SLessEqual: m_Disassembly += "sle "; break;
              default: break;
            }
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], false);
            break;
          }
          case Operation::Select:
          {
            m_Disassembly += "select ";
            m_Disassembly += argToString(inst.args[2], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
            break;
          }
          case Operation::ExtractElement:
          {
            m_Disassembly += "extractelement ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
            break;
          }
          case Operation::InsertElement:
          {
            m_Disassembly += "insertelement ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[2], true);
            break;
          }
          case Operation::ShuffleVector:
          {
            m_Disassembly += "shufflevector ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[2], true);
            break;
          }
          case Operation::InsertValue:
          {
            m_Disassembly += "insertvalue ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
            for(size_t a = 2; a < inst.args.size(); a++)
            {
              m_Disassembly += ", " + ToStr(inst.args[a].literal);
            }
            break;
          }
          case Operation::Branch:
          {
            m_Disassembly += "br ";
            if(inst.args.size() > 1)
            {
              m_Disassembly += argToString(inst.args[2], true);
              m_Disassembly += StringFormat::Fmt(", %s", argToString(inst.args[0], true).c_str());
              m_Disassembly += StringFormat::Fmt(", %s", argToString(inst.args[1], true).c_str());
            }
            else
            {
              m_Disassembly += argToString(inst.args[0], true);
            }
            break;
          }
          case Operation::Phi:
          {
            m_Disassembly += "phi ";
            m_Disassembly += inst.type->toString();
            for(size_t a = 0; a < inst.args.size(); a += 2)
            {
              if(a == 0)
                m_Disassembly += " ";
              else
                m_Disassembly += ", ";
              m_Disassembly +=
                  StringFormat::Fmt("[ %s, %s ]", argToString(inst.args[a], false).c_str(),
                                    argToString(inst.args[a + 1], false).c_str());
            }
            break;
          }
          case Operation::Switch:
          {
            m_Disassembly += "switch ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
            m_Disassembly += " [";
            m_Disassembly += "\n";
            instructionLine++;
            for(size_t a = 2; a < inst.args.size(); a += 2)
            {
              m_Disassembly +=
                  StringFormat::Fmt("    %s, %s\n", argToString(inst.args[a], true).c_str(),
                                    argToString(inst.args[a + 1], true).c_str());
              instructionLine++;
            }
            m_Disassembly += "  ]";
            break;
          }
          case Operation::Fence:
          {
            m_Disassembly += "fence ";
            if(inst.opFlags & InstructionFlags::SingleThread)
              m_Disassembly += "singlethread ";
            switch((inst.opFlags & InstructionFlags::SuccessOrderMask))
            {
              case InstructionFlags::SuccessUnordered: m_Disassembly += "unordered"; break;
              case InstructionFlags::SuccessMonotonic: m_Disassembly += "monotonic"; break;
              case InstructionFlags::SuccessAcquire: m_Disassembly += "acquire"; break;
              case InstructionFlags::SuccessRelease: m_Disassembly += "release"; break;
              case InstructionFlags::SuccessAcquireRelease: m_Disassembly += "acq_rel"; break;
              case InstructionFlags::SuccessSequentiallyConsistent:
                m_Disassembly += "seq_cst";
                break;
              default: break;
            }
          }
          case Operation::LoadAtomic:
          {
            m_Disassembly += "load atomic ";
            if(inst.opFlags & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += inst.type->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += argToString(s, true);
              first = false;
            }
            m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Operation::StoreAtomic:
          {
            m_Disassembly += "store atomic ";
            if(inst.opFlags & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += argToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Operation::CompareExchange:
          {
            m_Disassembly += "cmpxchg ";
            if(inst.opFlags & InstructionFlags::Weak)
              m_Disassembly += "weak ";
            if(inst.opFlags & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";

            bool first = true;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += argToString(s, true);
              first = false;
            }

            m_Disassembly += " ";
            if(inst.opFlags & InstructionFlags::SingleThread)
              m_Disassembly += "singlethread ";
            switch((inst.opFlags & InstructionFlags::SuccessOrderMask))
            {
              case InstructionFlags::SuccessUnordered: m_Disassembly += "unordered"; break;
              case InstructionFlags::SuccessMonotonic: m_Disassembly += "monotonic"; break;
              case InstructionFlags::SuccessAcquire: m_Disassembly += "acquire"; break;
              case InstructionFlags::SuccessRelease: m_Disassembly += "release"; break;
              case InstructionFlags::SuccessAcquireRelease: m_Disassembly += "acq_rel"; break;
              case InstructionFlags::SuccessSequentiallyConsistent:
                m_Disassembly += "seq_cst";
                break;
              default: break;
            }
            m_Disassembly += " ";
            switch((inst.opFlags & InstructionFlags::FailureOrderMask))
            {
              case InstructionFlags::FailureUnordered: m_Disassembly += "unordered"; break;
              case InstructionFlags::FailureMonotonic: m_Disassembly += "monotonic"; break;
              case InstructionFlags::FailureAcquire: m_Disassembly += "acquire"; break;
              case InstructionFlags::FailureRelease: m_Disassembly += "release"; break;
              case InstructionFlags::FailureAcquireRelease: m_Disassembly += "acq_rel"; break;
              case InstructionFlags::FailureSequentiallyConsistent:
                m_Disassembly += "seq_cst";
                break;
              default: break;
            }
            break;
          }
          case Operation::AtomicExchange:
          case Operation::AtomicAdd:
          case Operation::AtomicSub:
          case Operation::AtomicAnd:
          case Operation::AtomicNand:
          case Operation::AtomicOr:
          case Operation::AtomicXor:
          case Operation::AtomicMax:
          case Operation::AtomicMin:
          case Operation::AtomicUMax:
          case Operation::AtomicUMin:
          {
            m_Disassembly += "atomicrmw ";
            if(inst.opFlags & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            switch(inst.op)
            {
              case Operation::AtomicExchange: m_Disassembly += "xchg "; break;
              case Operation::AtomicAdd: m_Disassembly += "add "; break;
              case Operation::AtomicSub: m_Disassembly += "sub "; break;
              case Operation::AtomicAnd: m_Disassembly += "and "; break;
              case Operation::AtomicNand: m_Disassembly += "nand "; break;
              case Operation::AtomicOr: m_Disassembly += "or "; break;
              case Operation::AtomicXor: m_Disassembly += "xor "; break;
              case Operation::AtomicMax: m_Disassembly += "max "; break;
              case Operation::AtomicMin: m_Disassembly += "min "; break;
              case Operation::AtomicUMax: m_Disassembly += "umax "; break;
              case Operation::AtomicUMin: m_Disassembly += "umin "; break;
              default: break;
            }

            bool first = true;
            for(Value &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += argToString(s, true);
              first = false;
            }

            m_Disassembly += " ";
            if(inst.opFlags & InstructionFlags::SingleThread)
              m_Disassembly += "singlethread ";
            switch((inst.opFlags & InstructionFlags::SuccessOrderMask))
            {
              case InstructionFlags::SuccessUnordered: m_Disassembly += "unordered"; break;
              case InstructionFlags::SuccessMonotonic: m_Disassembly += "monotonic"; break;
              case InstructionFlags::SuccessAcquire: m_Disassembly += "acquire"; break;
              case InstructionFlags::SuccessRelease: m_Disassembly += "release"; break;
              case InstructionFlags::SuccessAcquireRelease: m_Disassembly += "acq_rel"; break;
              case InstructionFlags::SuccessSequentiallyConsistent:
                m_Disassembly += "seq_cst";
                break;
              default: break;
            }
            break;
          }
        }

        if(inst.debugLoc != ~0U)
        {
          DebugLocation &debugLoc = m_DebugLocations[inst.debugLoc];

          m_Disassembly += StringFormat::Fmt(", !dbg !%u", GetOrAssignMetaID(debugLoc));
        }

        if(!inst.attachedMeta.empty())
        {
          for(size_t m = 0; m < inst.attachedMeta.size(); m++)
          {
            m_Disassembly +=
                StringFormat::Fmt(", !%s !%u", m_Kinds[(size_t)inst.attachedMeta[m].first].c_str(),
                                  GetOrAssignMetaID(inst.attachedMeta[m].second));
          }
        }

        if(inst.debugLoc != ~0U)
        {
          DebugLocation &debugLoc = m_DebugLocations[inst.debugLoc];

          if(!debugCall && debugLoc.line > 0)
          {
            m_Disassembly += StringFormat::Fmt(" ; line:%llu col:%llu", debugLoc.line, debugLoc.col);
          }
        }

        if(debugCall && inst.funcCall)
        {
          size_t varIdx = 0, exprIdx = 0;
          if(inst.funcCall->name == "llvm.dbg.value")
          {
            varIdx = 2;
            exprIdx = 3;
          }
          else if(inst.funcCall->name == "llvm.dbg.declare")
          {
            varIdx = 1;
            exprIdx = 2;
          }

          if(varIdx > 0)
          {
            RDCASSERT(inst.args[varIdx].type == ValueType::Metadata);
            RDCASSERT(inst.args[exprIdx].type == ValueType::Metadata);
            m_Disassembly += StringFormat::Fmt(
                " ; var:%s ", escapeString(GetDebugVarName(inst.args[varIdx].meta->dwarf)).c_str());
            m_Disassembly += inst.args[exprIdx].meta->valString();

            rdcstr funcName = GetFunctionScopeName(inst.args[varIdx].meta->dwarf);
            if(!funcName.empty())
              m_Disassembly += StringFormat::Fmt(" func:%s", escapeString(funcName).c_str());
          }
        }

        if(inst.funcCall && inst.funcCall->name.beginsWith("dx.op."))
        {
          if(inst.args[0].type == ValueType::Constant)
          {
            uint32_t opcode = inst.args[0].constant->val.u32v[0];
            if(opcode < ARRAY_COUNT(funcSigs))
            {
              m_Disassembly += "  ; ";
              m_Disassembly += funcSigs[opcode];
            }
          }
        }

        if(inst.funcCall && inst.funcCall->name.beginsWith("dx.op.annotateHandle"))
        {
          if(inst.args[2].type == ValueType::Constant)
          {
            const Constant *props = inst.args[2].constant;
            if(props && !props->nullconst && props->members.size() == 2 &&
               props->members[0].type == ValueType::Constant)
            {
              uint32_t packedProps[2] = {};
              packedProps[0] = props->members[0].constant->val.u32v[0];
              packedProps[1] = props->members[1].constant->val.u32v[0];

              bool uav = (packedProps[0] & (1 << 12)) != 0;
              ResourceKind resKind = (ResourceKind)(packedProps[0] & 0xFF);
              ResourceClass resClass;
              if(resKind == ResourceKind::Sampler || resKind == ResourceKind::SamplerComparison)
                resClass = ResourceClass::Sampler;
              else if(resKind == ResourceKind::CBuffer)
                resClass = ResourceClass::CBuffer;
              else if(uav)
                resClass = ResourceClass::UAV;
              else
                resClass = ResourceClass::SRV;

              m_Disassembly += "  resource: ";

              bool srv = (resClass == ResourceClass::SRV);

              ComponentType compType = ComponentType(packedProps[1] & 0xFF);
              bool singleComp = (packedProps[1] & 0xFF00) == 1;

              uint32_t structStride = packedProps[1];

              bool rov = (packedProps[0] & (1 << 13)) != 0;
              bool globallyCoherent = (packedProps[0] & (1 << 14)) != 0;

              switch(resKind)
              {
                case ResourceKind::Unknown: m_Disassembly += "Unknown"; break;
                case ResourceKind::Texture1D:
                case ResourceKind::Texture2D:
                case ResourceKind::Texture3D:
                case ResourceKind::TextureCube:
                case ResourceKind::Texture1DArray:
                case ResourceKind::Texture2DArray:
                case ResourceKind::TextureCubeArray:
                case ResourceKind::TypedBuffer:
                  if(globallyCoherent)
                    m_Disassembly += "globallycoherent ";
                  if(!srv && rov)
                    m_Disassembly += "ROV";
                  else if(!srv)
                    m_Disassembly += "RW";
                  switch(resKind)
                  {
                    case ResourceKind::Texture1D: m_Disassembly += "Texture1D"; break;
                    case ResourceKind::Texture2D: m_Disassembly += "Texture2D"; break;
                    case ResourceKind::Texture3D: m_Disassembly += "Texture3D"; break;
                    case ResourceKind::TextureCube: m_Disassembly += "TextureCube"; break;
                    case ResourceKind::Texture1DArray: m_Disassembly += "Texture1DArray"; break;
                    case ResourceKind::Texture2DArray: m_Disassembly += "Texture2DArray"; break;
                    case ResourceKind::TextureCubeArray: m_Disassembly += "TextureCubeArray"; break;
                    case ResourceKind::TypedBuffer: m_Disassembly += "TypedBuffer"; break;
                    default: break;
                  }
                  m_Disassembly += StringFormat::Fmt("<%s%s>", ToStr(compType).c_str(),
                                                     !srv && !singleComp ? "[vec]" : "");
                  break;
                case ResourceKind::RTAccelerationStructure:
                  m_Disassembly += "RTAccelerationStructure";
                  break;
                case ResourceKind::FeedbackTexture2D: m_Disassembly += "FeedbackTexture2D"; break;
                case ResourceKind::FeedbackTexture2DArray:
                  m_Disassembly += "FeedbackTexture2DArray";
                  break;
                case ResourceKind::StructuredBuffer:
                  if(globallyCoherent)
                    m_Disassembly += "globallycoherent ";
                  m_Disassembly += srv ? "StructuredBuffer" : "RWStructuredBuffer";
                  m_Disassembly += StringFormat::Fmt("<stride=%u>", structStride);
                  break;
                case ResourceKind::StructuredBufferWithCounter:
                  if(globallyCoherent)
                    m_Disassembly += "globallycoherent ";
                  m_Disassembly +=
                      srv ? "StructuredBufferWithCounter" : "RWStructuredBufferWithCounter";
                  m_Disassembly += StringFormat::Fmt("<stride=%u>", structStride);
                  break;
                case ResourceKind::RawBuffer:
                  if(globallyCoherent)
                    m_Disassembly += "globallycoherent ";
                  m_Disassembly += srv ? "ByteAddressBuffer" : "RWByteAddressBuffer";
                  break;
                case ResourceKind::Texture2DMS:
                  m_Disassembly += StringFormat::Fmt("Texture2DMS<%s>", ToStr(compType).c_str());
                  break;
                case ResourceKind::Texture2DMSArray:
                  m_Disassembly += StringFormat::Fmt("Texture2DMSArray<%>", ToStr(compType).c_str());
                  break;
                case ResourceKind::CBuffer:
                  RDCASSERT(resClass == ResourceClass::CBuffer);
                  m_Disassembly += "CBuffer";
                  break;
                case ResourceKind::Sampler:
                  RDCASSERT(resClass == ResourceClass::Sampler);
                  m_Disassembly += "SamplerState";
                  break;
                case ResourceKind::TBuffer:
                  RDCASSERT(resClass == ResourceClass::SRV);
                  m_Disassembly += "TBuffer";
                  break;
                case ResourceKind::SamplerComparison:
                  RDCASSERT(resClass == ResourceClass::Sampler);
                  m_Disassembly += "SamplerComparisonState";
                  break;
              }
            }
          }
        }

        m_Disassembly += "\n";
        instructionLine++;

        // if this is the last instruction don't print the next block's label
        if(funcIdx == func.instructions.size() - 1)
          break;

        if(inst.op == Operation::Branch || inst.op == Operation::Unreachable ||
           inst.op == Operation::Switch || inst.op == Operation::Ret)
        {
          m_Disassembly += "\n";
          instructionLine++;

          curBlock++;

          rdcstr labelName;

          if(func.blocks[curBlock].name.empty())
            labelName = StringFormat::Fmt("; <label>:%u", func.blocks[curBlock].resultID);
          else
            labelName =
                StringFormat::Fmt("%s: ", escapeStringIfNeeded(func.blocks[curBlock].name).c_str());

          labelName.reserve(50);
          while(labelName.size() < 50)
            labelName.push_back(' ');

          labelName += "; preds = ";
#if ENABLED(DXC_COMPATIBLE_DISASM)
          // unfortunately due to how llvm/dxc packs its preds, this is not feasible to replicate so
          // instead we omit the pred list entirely and dxc's output needs to be regex replaced to
          // match
          labelName += "...";
#else
          bool first = true;
          for(const Block *pred : func.blocks[curBlock].preds)
          {
            if(!first)
              labelName += ", ";
            first = false;
            if(pred->name.empty())
              labelName += StringFormat::Fmt("%%%u", pred->resultID);
            else
              labelName += "%" + escapeStringIfNeeded(pred->name);
          }
#endif

          m_Disassembly += labelName;
          m_Disassembly += "\n";
          instructionLine++;
        }
      }
      m_Disassembly += "}\n\n";
      instructionLine += 2;
    }
    else
    {
      m_Disassembly += "\n\n";
      instructionLine += 2;
    }
  }

  for(size_t i = 0; i < funcAttrGroups.size(); i++)
  {
    m_Disassembly += StringFormat::Fmt("attributes #%zu = { %s }\n", i,
                                       funcAttrGroups[i]->toString(true).c_str());
  }

  if(!funcAttrGroups.empty())
    m_Disassembly += "\n";

  m_Disassembly += namedMeta + "\n";

  size_t numIdx = 0;
  size_t dbgIdx = 0;

  for(uint32_t i = 0; i < m_NextMetaID; i++)
  {
    if(numIdx < m_NumberedMeta.size() && m_NumberedMeta[numIdx]->id == i)
    {
      rdcstr metaline = StringFormat::Fmt("!%u = %s%s\n", i,
                                          m_NumberedMeta[numIdx]->isDistinct ? "distinct " : "",
                                          m_NumberedMeta[numIdx]->valString().c_str());
#if ENABLED(DXC_COMPATIBLE_DISASM)
      for(size_t c = 0; c < metaline.size(); c += 4096)
        m_Disassembly += metaline.substr(c, 4096);
#else
      m_Disassembly += metaline;
#endif
      if(m_NumberedMeta[numIdx]->dwarf)
        m_NumberedMeta[numIdx]->dwarf->setID(i);
      numIdx++;
    }
    else if(dbgIdx < m_DebugLocations.size() && m_DebugLocations[dbgIdx].id == i)
    {
      m_Disassembly +=
          StringFormat::Fmt("!%u = %s\n", i, m_DebugLocations[dbgIdx].toString().c_str());
      dbgIdx++;
    }
    else
    {
      RDCERR("Couldn't find meta ID %u", i);
    }
  }

  m_Disassembly += "\n";
}

rdcstr Type::toString() const
{
  if(!name.empty())
  {
    return "%" + escapeStringIfNeeded(name);
  }

  switch(type)
  {
    case Scalar:
    {
      switch(scalarType)
      {
        case Void: return "void";
        case Int: return StringFormat::Fmt("i%u", bitWidth);
        case Float:
          switch(bitWidth)
          {
            case 16: return "half";
            case 32: return "float";
            case 64: return "double";
            default: return StringFormat::Fmt("fp%u", bitWidth);
          }
      }
    }
    case Vector: return StringFormat::Fmt("<%u x %s>", elemCount, inner->toString().c_str());
    case Pointer:
      if(addrSpace == Type::PointerAddrSpace::Default)
        return StringFormat::Fmt("%s*", inner->toString().c_str());
      else
        return StringFormat::Fmt("%s addrspace(%d)*", inner->toString().c_str(), addrSpace);
    case Array: return StringFormat::Fmt("[%u x %s]", elemCount, inner->toString().c_str());
    case Function: return declFunction(rdcstr(), {}, NULL) + "*";
    case Struct:
    {
      rdcstr ret;
      if(packedStruct)
        ret = "<{ ";
      else
        ret = "{ ";
      for(size_t i = 0; i < members.size(); i++)
      {
        if(i > 0)
          ret += ", ";
        ret += members[i]->toString();
      }
      if(packedStruct)
        ret += " }>";
      else
        ret += " }";
      return ret;
    }

    case Metadata: return "metadata";
    case Label: return "label";
    default: return "unknown_type";
  }
}

rdcstr Type::declFunction(rdcstr funcName, const rdcarray<Instruction> &args,
                          const AttributeSet *attrs) const
{
  rdcstr ret = inner->toString();
  ret += " " + funcName + "(";
  for(size_t i = 0; i < members.size(); i++)
  {
    if(i > 0)
      ret += ", ";
    ret += members[i]->toString();

    if(attrs && i + 1 < attrs->groupSlots.size() && attrs->groupSlots[i + 1])
    {
      ret += " " + attrs->groupSlots[i + 1]->toString(true);
    }

    if(i < args.size() && !args[i].name.empty())
      ret += " %" + escapeStringIfNeeded(args[i].name);
  }
  ret += ")";
  return ret;
}

rdcstr AttributeGroup::toString(bool stringAttrs) const
{
  rdcstr ret = "";
  Attribute p = params;

  if(p & Attribute::Alignment)
  {
    ret += StringFormat::Fmt(" align=%llu", align);
    p &= ~Attribute::Alignment;
  }
  if(p & Attribute::StackAlignment)
  {
    ret += StringFormat::Fmt(" alignstack=%llu", stackAlign);
    p &= ~Attribute::StackAlignment;
  }
  if(p & Attribute::Dereferenceable)
  {
    ret += StringFormat::Fmt(" dereferenceable=%llu", derefBytes);
    p &= ~Attribute::Dereferenceable;
  }
  if(p & Attribute::DereferenceableOrNull)
  {
    ret += StringFormat::Fmt(" dereferenceable_or_null=%llu", derefOrNullBytes);
    p &= ~Attribute::DereferenceableOrNull;
  }

  if(p != Attribute::None)
  {
    ret = ToStr(p) + " " + ret;
    int offs = ret.indexOf('|');
    while(offs >= 0)
    {
      ret.erase((size_t)offs, 2);
      offs = ret.indexOf('|');
    }
  }

  if(stringAttrs)
  {
    ret.trim();

    for(const rdcpair<rdcstr, rdcstr> &str : strs)
    {
      if(str.second.empty())
        ret += " " + escapeString(str.first);
      else
        ret += " " + escapeString(str.first) + "=" + escapeString(str.second);
    }
  }

  return ret.trimmed();
}

rdcstr Metadata::refString() const
{
  if(id == ~0U)
    return valString();
  return StringFormat::Fmt("!%u", id);
}

rdcstr DebugLocation::toString() const
{
  rdcstr ret = StringFormat::Fmt("!DILocation(line: %llu", line);
  if(col)
    ret += StringFormat::Fmt(", column: %llu", col);
  ret += StringFormat::Fmt(", scope: %s", scope ? scope->refString().c_str() : "null");
  if(inlinedAt)
    ret += StringFormat::Fmt(", inlinedAt: %s", inlinedAt->refString().c_str());
  ret += ")";
  return ret;
}

rdcstr Metadata::valString() const
{
  if(dwarf)
  {
    return dwarf->toString();
  }
  else if(debugLoc)
  {
    return debugLoc->toString();
  }
  else if(isConstant)
  {
    if(type == NULL)
    {
// don't truncate here for dxc-compatible disassembly, instead we wrap at 4096 columns at a higher
// level
#if DISABLED(DXC_COMPATIBLE_DISASM)
      // truncate very long strings - most likely these are shader source
      if(str.length() > 400)
      {
        rdcstr trunc = str;
        trunc.erase(200, str.length() - 400);
        trunc.insert(200, "...");
        return StringFormat::Fmt("!%s", escapeString(trunc).c_str());
      }
#endif

      return StringFormat::Fmt("!%s", escapeString(str).c_str());
    }
    else
    {
      const Instruction *i = NULL;

      if(value.type == ValueType::Instruction)
        i = value.instruction;

      if(i)
      {
        if(i->name.empty())
          return StringFormat::Fmt("%s %%%u", i->type->toString().c_str(), i->resultID);
        else
          return StringFormat::Fmt("%s %%%s", i->type->toString().c_str(),
                                   escapeStringIfNeeded(i->name).c_str());
      }
      else if(!value.empty())
      {
        return value.toString(true);
      }
      else
      {
        RDCERR("No instruction for value-less metadata");
        return "???";
      }
    }
  }
  else
  {
    rdcstr ret = "!{";
    for(size_t i = 0; i < children.size(); i++)
    {
      if(i > 0)
        ret += ", ";
      if(!children[i])
        ret += "null";
      else if(children[i]->isConstant)
        ret += children[i]->valString();
      else
        ret += StringFormat::Fmt("!%u", children[i]->id);
    }
    ret += "}";

    return ret;
  }
}

static void floatAppendToString(const Type *t, const ShaderValue &val, uint32_t i, rdcstr &ret)
{
#if ENABLED(DXC_COMPATIBLE_DISASM)
  // dxc/llvm always prints half floats as their 16-bit hex representation.
  if(t->bitWidth == 16)
  {
    ret += StringFormat::Fmt("0xH%04X", val.u32v[i]);
    return;
  }
#endif

  double d = t->bitWidth == 64 ? val.f64v[i] : val.f32v[i];

  // NaNs/infs are printed as hex to ensure we don't lose bits
  if(RDCISFINITE(d))
  {
    // check we can reparse precisely a float-formatted string. Otherwise we print as hex
    rdcstr flt = StringFormat::Fmt("%.6le", d);

#if ENABLED(DXC_COMPATIBLE_DISASM)
    // dxc/llvm only prints floats as floats if they roundtrip, but our disassembly doesn't need to
    // roundtrip so it's better to display the value in all cases
    double reparse = strtod(flt.begin(), NULL);

    if(d == reparse)
    {
      ret += flt;
      return;
    }
#else
    ret += flt;
#endif
  }

  ret += StringFormat::Fmt("0x%llX", d);
}

void shaderValAppendToString(const Type *type, const ShaderValue &val, uint32_t i, rdcstr &ret)
{
  if(type->scalarType == Type::Float)
  {
    floatAppendToString(type, val, i, ret);
  }
  else if(type->scalarType == Type::Int)
  {
    // LLVM seems to always interpret these as signed? :(
    if(type->bitWidth > 32)
      ret += StringFormat::Fmt("%lld", val.s64v[i]);
    else if(type->bitWidth == 1)
      ret += val.u32v[i] ? "true" : "false";
    else
      ret += StringFormat::Fmt("%d", val.s32v[i]);
  }
}

rdcstr Value::toString(bool withType) const
{
  rdcstr ret;
  if(withType)
  {
    const Type *t = GetType();
    if(t)
      ret += t->toString() + " ";
    else
      RDCERR("Type requested in value string, but no type available");
  }
  switch(type)
  {
    case ValueType::Function:
      ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(function->name).c_str());
      break;
    case ValueType::GlobalVar:
      ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(global->name).c_str());
      break;
    case ValueType::Alias:
      ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(alias->name).c_str());
      break;
    case ValueType::Constant: return constant->toString(withType); break;
    case ValueType::Unknown:
      RDCERR("Invalid or forward-reference value being stringised");
      ret += "???";
      break;
    case ValueType::Instruction:
    case ValueType::Metadata:
    case ValueType::Literal:
    case ValueType::BasicBlock:
      RDCERR("Unexpected value being stringised");
      ret += "???";
      break;
  }
  return ret;
}

rdcstr Constant::toString(bool withType) const
{
  if(type == NULL)
    return escapeString(str);

  rdcstr ret;
  if(withType)
    ret += type->toString() + " ";
  if(undef)
  {
    ret += "undef";
  }
  else if(op != Operation::NoOp)
  {
    switch(op)
    {
      default: break;
      case Operation::GetElementPtr:
      {
        ret += "getelementptr inbounds (";

        const Type *baseType = members[0].GetType();
        RDCASSERT(baseType->type == Type::Pointer);
        ret += baseType->inner->toString();
        for(size_t i = 0; i < members.size(); i++)
        {
          ret += ", ";

          ret += members[i].toString(withType);
        }
        ret += ")";
        break;
      }
      case Operation::Trunc:
      case Operation::ZExt:
      case Operation::SExt:
      case Operation::FToU:
      case Operation::FToS:
      case Operation::UToF:
      case Operation::SToF:
      case Operation::FPTrunc:
      case Operation::FPExt:
      case Operation::PtrToI:
      case Operation::IToPtr:
      case Operation::Bitcast:
      case Operation::AddrSpaceCast:
      {
        switch(op)
        {
          case Operation::Trunc: ret += "trunc "; break;
          case Operation::ZExt: ret += "zext "; break;
          case Operation::SExt: ret += "sext "; break;
          case Operation::FToU: ret += "fptoui "; break;
          case Operation::FToS: ret += "fptosi "; break;
          case Operation::UToF: ret += "uitofp "; break;
          case Operation::SToF: ret += "sitofp "; break;
          case Operation::FPTrunc: ret += "fptrunc "; break;
          case Operation::FPExt: ret += "fpext "; break;
          case Operation::PtrToI: ret += "ptrtoi "; break;
          case Operation::IToPtr: ret += "itoptr "; break;
          case Operation::Bitcast: ret += "bitcast "; break;
          case Operation::AddrSpaceCast: ret += "addrspacecast "; break;
          default: break;
        }

        ret += "(";
        ret += inner.toString(withType);
        ret += " to ";
        ret += type->toString();
        ret += ")";
        break;
      }
    }
  }
  else if(type->type == Type::Scalar)
  {
    shaderValAppendToString(type, val, 0, ret);
  }
  else if(nullconst)
  {
    ret += "zeroinitializer";
  }
  else if(type->type == Type::Vector)
  {
    ret += "<";
    for(uint32_t i = 0; i < type->elemCount; i++)
    {
      if(i > 0)
        ret += ", ";
      if(withType)
        ret += type->inner->toString() + " ";
      shaderValAppendToString(type, val, i, ret);
    }
    ret += ">";
  }
  else if(type->type == Type::Array)
  {
    ret += "[";
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i > 0)
        ret += ", ";

      if(members[i].type == ValueType::Literal)
      {
        if(withType)
          ret += type->inner->toString() + " ";

        ShaderValue v;
        v.u64v[0] = members[i].literal;

        shaderValAppendToString(type->inner, v, 0, ret);
      }
      else
      {
        ret += members[i].toString(withType);
      }
    }
    ret += "]";
  }
  else if(type->type == Type::Struct)
  {
    ret += "{ ";
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i > 0)
        ret += ", ";

      if(members[i].type == ValueType::Literal)
      {
        ShaderValue v;
        v.u64v[0] = members[i].literal;

        shaderValAppendToString(members[i].GetType(), v, 0, ret);
      }
      else
      {
        ret += members[i].toString(withType);
      }
    }
    ret += " }";
  }
  else
  {
    ret += StringFormat::Fmt("unsupported type %u", type->type);
  }

  return ret;
}

};    // namespace DXIL

template <>
rdcstr DoStringise(const DXIL::InstructionFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(DXIL::InstructionFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "");

    // llvm doesn't print all bits if fastmath is set
    if(el & DXIL::InstructionFlags::FastMath)
      return "fast";

    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoNaNs, "nnan");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoInfs, "ninf");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoSignedZeros, "nsz");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(AllowReciprocal, "arcp");

    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoUnsignedWrap, "nuw");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoSignedWrap, "nsw");

    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Exact, "exact");
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::Attribute &el)
{
  BEGIN_BITFIELD_STRINGISE(DXIL::Attribute);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(None, "");

    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Alignment, "align");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(AlwaysInline, "alwaysinline");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ByVal, "byval");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InlineHint, "inlinehint");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InReg, "inreg");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(MinSize, "minsize");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Naked, "naked");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Nest, "nest");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoAlias, "noalias");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoBuiltin, "nobuiltin");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoCapture, "nocapture");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoDuplicate, "noduplicate");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoImplicitFloat, "noimplicitfloat");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoInline, "noinline");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NonLazyBind, "nonlazybind");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoRedZone, "noredzone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoReturn, "noreturn");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoUnwind, "nounwind");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(OptimizeForSize, "optsize");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReadNone, "readnone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReadOnly, "readonly");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Returned, "returned");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReturnsTwice, "returns_twice");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SExt, "signext");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackAlignment, "alignstack");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtect, "ssp");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtectReq, "sspreq");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtectStrong, "sspstrong");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StructRet, "sret");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeAddress, "sanitize_address");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeThread, "sanitize_thread");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeMemory, "sanitize_memory");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(UWTable, "uwtable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ZExt, "zeroext");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Builtin, "builtin");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Cold, "cold");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(OptimizeNone, "optnone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InAlloca, "inalloca");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NonNull, "nonnull");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(JumpTable, "jumptable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Dereferenceable, "dereferenceable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(DereferenceableOrNull, "dereferenceable_or_null");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Convergent, "convergent");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SafeStack, "safestack");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ArgMemOnly, "argmemonly");
  }
  END_BITFIELD_STRINGISE();
}
