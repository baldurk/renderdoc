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

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include "common/formatting.h"
#include "dxil_bytecode.h"
#include "dxil_common.h"

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

    if(m->constant)
      accumulate(m->constant->type);

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
    "TextureGatherCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel,compareVale)",
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

  m_Disassembly = StringFormat::Fmt("; %s Shader, compiled under SM%u.%u\n\n",
                                    shaderName[int(m_Type)], m_Major, m_Minor);
  m_Disassembly += StringFormat::Fmt("target datalayout = \"%s\"\n", m_Datalayout.c_str());
  m_Disassembly += StringFormat::Fmt("target triple = \"%s\"\n\n", m_Triple.c_str());

  int instructionLine = 6;

  TypeOrderer typeOrderer;
  rdcarray<const Type *> orderedTypes;

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = m_GlobalVars[i];
    typeOrderer.accumulate(g.type);

    if(g.initialiser.type == SymbolType::Constant)
      typeOrderer.accumulate(m_Constants[(size_t)g.initialiser.idx].type);
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
        if(inst.args[a].type != SymbolType::Literal && inst.args[a].type != SymbolType::BasicBlock)
          typeOrderer.accumulate(GetSymbolType(func, inst.args[a]));
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
    if(g.initialiser.type != SymbolType::Constant)
    {
      if(g.flags & GlobalFlags::IsExternal)
        m_Disassembly += "external ";
    }
    if(!(g.flags & GlobalFlags::IsExternal))
      m_Disassembly += "internal ";
    if(g.flags & GlobalFlags::IsAppending)
      m_Disassembly += "appending ";
    if(g.type->addrSpace)
      m_Disassembly += StringFormat::Fmt("addrspace(%d) ", g.type->addrSpace);
    if(g.flags & GlobalFlags::LocalUnnamedAddr)
      m_Disassembly += "local_unnamed_addr ";
    else if(g.flags & GlobalFlags::GlobalUnnamedAddr)
      m_Disassembly += "unnamed_addr ";
    if(g.flags & GlobalFlags::IsConst)
      m_Disassembly += "constant ";
    else
      m_Disassembly += "global ";

    if(g.initialiser.type == SymbolType::Constant)
      m_Disassembly += m_Constants[(size_t)g.initialiser.idx].toString(true);
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

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    Function &func = m_Functions[i];

    auto argToString = [this, &func](Symbol s, bool withTypes) {
      rdcstr ret;
      switch(s.type)
      {
        case SymbolType::Unknown:
        case SymbolType::Alias: ret = "???"; break;
        case SymbolType::Literal:
          if(withTypes)
            ret += "i32 ";
          ret += StringFormat::Fmt("%lld", s.idx);
          break;
        case SymbolType::Metadata:
          if(withTypes)
            ret += "metadata ";
          if(s.idx < m_Metadata.size())
          {
            Metadata &m = m_Metadata[(size_t)s.idx];
            if(m.isConstant && m.constant && m.constant->symbol)
              ret += m.constant->toString(withTypes);
            else if(m.isConstant && m.constant &&
                    (m.constant->type->type == Type::Scalar || m.constant->nullconst))
              ret += m.constant->toString(withTypes);
            else
              ret += StringFormat::Fmt("!%u", GetOrAssignMetaID(&m));
          }
          else
          {
            ret += GetFunctionMetadata(func, s.idx)->refString();
          }
          break;
        case SymbolType::Function:
          ret = "@" + escapeStringIfNeeded(m_Functions[(size_t)s.idx].name);
          break;
        case SymbolType::GlobalVar:
          if(withTypes)
            ret = m_GlobalVars[(size_t)s.idx].type->toString() + " ";
          ret += "@" + escapeStringIfNeeded(m_GlobalVars[(size_t)s.idx].name);
          break;
        case SymbolType::Constant:
          ret = GetFunctionConstant(func, s.idx)->toString(withTypes);
          break;
        case SymbolType::Argument:
          if(withTypes)
            ret = func.args[(size_t)s.idx].type->toString() + " ";
          ret += "%" + escapeStringIfNeeded(func.args[(size_t)s.idx].name);
          break;
        case SymbolType::Instruction:
        {
          const Instruction &refinst = func.instructions[(size_t)s.idx];
          if(withTypes)
            ret = refinst.type->toString() + " ";
          if(refinst.name.empty())
            ret += StringFormat::Fmt("%%%u", refinst.resultID);
          else
            ret += StringFormat::Fmt("%%%s", escapeStringIfNeeded(refinst.name).c_str());
          break;
        }
        case SymbolType::BasicBlock:
        {
          const Block &block = func.blocks[(size_t)s.idx];
          if(withTypes)
            ret = "label ";
          if(block.name.empty())
            ret += StringFormat::Fmt("%%%u", block.resultID);
          else
            ret += StringFormat::Fmt("%%%s", escapeStringIfNeeded(block.name).c_str());
        }
      }
      return ret;
    };

    if(func.attrs)
    {
      m_Disassembly += StringFormat::Fmt("; Function Attrs: %s\n", func.attrs->toString().c_str());
      instructionLine++;
    }

    m_Disassembly += (func.external ? "declare " : "define ");
    m_Disassembly += func.funcType->declFunction("@" + escapeStringIfNeeded(func.name));

    if(func.attrs)
      m_Disassembly += StringFormat::Fmt(" #%u", func.attrs->index);

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
            for(Symbol &s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";
              first = false;

              m_Disassembly += argToString(s, true);
            }
            m_Disassembly += ")";
            debugCall = inst.funcCall->name.beginsWith("llvm.dbg.");
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
              m_Disassembly += StringFormat::Fmt(", %llu", inst.args[n].idx);
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
            for(Symbol &s : inst.args)
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
            m_Disassembly += GetSymbolType(func, inst.args[0])->inner->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(Symbol &s : inst.args)
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
            for(Symbol &s : inst.args)
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
              m_Disassembly += ", " + ToStr(inst.args[a].idx);
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
            for(Symbol &s : inst.args)
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
            for(Symbol &s : inst.args)
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
            for(Symbol &s : inst.args)
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
            RDCASSERT(inst.args[varIdx].type == SymbolType::Metadata);
            RDCASSERT(inst.args[exprIdx].type == SymbolType::Metadata);
            m_Disassembly += StringFormat::Fmt(
                " ; var:%s ",
                escapeString(GetDebugVarName(GetFunctionMetadata(func, inst.args[varIdx].idx)->dwarf))
                    .c_str());
            m_Disassembly += GetFunctionMetadata(func, inst.args[exprIdx].idx)->valString();
          }
        }

        if(inst.funcCall && inst.funcCall->name.beginsWith("dx.op."))
        {
          if(inst.args[0].type == SymbolType::Constant)
          {
            uint32_t opcode = GetFunctionConstant(func, inst.args[0].idx)->val.uv[0];
            if(opcode < ARRAY_COUNT(funcSigs))
            {
              m_Disassembly += "  ; ";
              m_Disassembly += funcSigs[opcode];
            }
          }
        }

        if(inst.funcCall && inst.funcCall->name.beginsWith("dx.op.annotateHandle"))
        {
          if(inst.args[2].type == SymbolType::Constant && inst.args[3].type == SymbolType::Constant)
          {
            ResourceClass resClass =
                (ResourceClass)GetFunctionConstant(func, inst.args[2].idx)->val.uv[0];
            ResourceKind resKind =
                (ResourceKind)GetFunctionConstant(func, inst.args[3].idx)->val.uv[0];

            m_Disassembly += "  resource: ";

            bool srv = (resClass == ResourceClass::SRV);

            uint32_t packedProps[2] = {};

            const Constant *props = GetFunctionConstant(func, inst.args[4].idx);

            if(props && !props->nullconst)
            {
              packedProps[0] = props->members[0].val.uv[0];
              packedProps[1] = props->members[1].val.uv[0];
            }

            ComponentType compType = ComponentType(packedProps[0] & 0x1f);
            bool singleComp = (packedProps[0] & 0x20) != 0;
            uint32_t sampleCount = (packedProps[0] & 0x1C0) >> 6;

            uint32_t structStride = packedProps[0];

            bool rov = (packedProps[1] & 0x1) != 0;
            bool globallyCoherent = (packedProps[1] & 0x2) != 0;

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
                m_Disassembly += StringFormat::Fmt("Texture2DMS<%s, samples=%u>",
                                                   ToStr(compType).c_str(), 1 << sampleCount);
                break;
              case ResourceKind::Texture2DMSArray:
                m_Disassembly += StringFormat::Fmt("Texture2DMSArray<%s, samples=%u>",
                                                   ToStr(compType).c_str(), 1 << sampleCount);
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

  for(size_t i = 0; i < m_Attributes.size(); i++)
    m_Disassembly +=
        StringFormat::Fmt("attributes #%zu = { %s }\n", i, m_Attributes[i].toString().c_str());

  if(!m_Attributes.empty())
    m_Disassembly += "\n";

  m_Disassembly += namedMeta + "\n";

  size_t numIdx = 0;
  size_t dbgIdx = 0;

  for(uint32_t i = 0; i < m_NextMetaID; i++)
  {
    if(numIdx < m_NumberedMeta.size() && m_NumberedMeta[numIdx]->id == i)
    {
      m_Disassembly += StringFormat::Fmt("!%u = %s%s\n", i,
                                         m_NumberedMeta[numIdx]->isDistinct ? "distinct " : "",
                                         m_NumberedMeta[numIdx]->valString().c_str());
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
      if(addrSpace == 0)
        return StringFormat::Fmt("%s*", inner->toString().c_str());
      else
        return StringFormat::Fmt("%s addrspace(%d)*", inner->toString().c_str(), addrSpace);
    case Array: return StringFormat::Fmt("[%u x %s]", elemCount, inner->toString().c_str());
    case Function: return declFunction(rdcstr());
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

rdcstr Type::declFunction(rdcstr funcName) const
{
  rdcstr ret = inner->toString();
  ret += " " + funcName + "(";
  for(size_t i = 0; i < members.size(); i++)
  {
    if(i > 0)
      ret += ", ";
    ret += members[i]->toString();
  }
  ret += ")";
  return ret;
}

rdcstr Attributes::toString() const
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

  for(const rdcpair<rdcstr, rdcstr> &str : strs)
    ret += " " + escapeString(str.first) + "=" + escapeString(str.second);

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
  rdcstr ret = StringFormat::Fmt("!DILocation(line: %llu, column: %llu, scope: %s", line, col,
                                 scope ? scope->refString().c_str() : "null");
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
      // truncate very long strings - most likely these are shader source
      if(str.length() > 400)
      {
        rdcstr trunc = str;
        trunc.erase(200, str.length() - 400);
        trunc.insert(200, "...");
        return StringFormat::Fmt("!%s", escapeString(trunc).c_str());
      }
      else
      {
        return StringFormat::Fmt("!%s", escapeString(str).c_str());
      }
    }
    else
    {
      if(constant)
      {
        if(type != constant->type)
          RDCERR("Type mismatch in metadata");
        return constant->toString(true);
      }
      else
      {
        if(func && instruction < func->instructions.size())
        {
          const Instruction &inst = func->instructions[instruction];
          if(inst.name.empty())
            return StringFormat::Fmt("%s %%%u", inst.type->toString().c_str(), inst.resultID);
          else
            return StringFormat::Fmt("%s %%%s", inst.type->toString().c_str(),
                                     escapeStringIfNeeded(inst.name).c_str());
        }
        else
        {
          RDCERR("No instruction symbol for value-less metadata");
          return "???";
        }
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
  else if(symbol)
  {
    ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(str).c_str());
  }
  else if(op != Operation::NoOp)
  {
    switch(op)
    {
      default: break;
      case Operation::GetElementPtr:
      {
        ret += "getelementptr inbounds (";

        const Type *baseType = members[0].type;
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
        ret += inner->toString(withType);
        ret += " to ";
        ret += type->toString();
        ret += ")";
        break;
      }
    }
  }
  else if(type->type == Type::Scalar)
  {
    if(type->scalarType == Type::Float)
    {
      double orig;
      if(type->bitWidth > 32)
        orig = val.dv[0];
      else
        orig = val.fv[0];

      // NaNs/infs are printed as hex to ensure we don't lose bits
      if(RDCISFINITE(orig))
      {
        // check we can reparse precisely a float-formatted string. Otherwise we print as hex
        rdcstr flt = StringFormat::Fmt("%.6le", orig);

        double reparse = strtod(flt.begin(), NULL);

        if(orig == reparse)
          return ret + flt;
      }

      ret += StringFormat::Fmt("0x%llX", orig);
    }
    else if(type->scalarType == Type::Int)
    {
      // LLVM seems to always interpret these as signed? :(
      if(type->bitWidth > 32)
        ret += StringFormat::Fmt("%lld", val.s64v[0]);
      else if(type->bitWidth == 1)
        ret += val.uv[0] ? "true" : "false";
      else
        ret += StringFormat::Fmt("%d", val.iv[0]);
    }
  }
  else if(type->type == Type::Vector)
  {
    ret += "<";
    for(uint32_t i = 0; i < type->elemCount; i++)
    {
      if(type->scalarType == Type::Float)
      {
        // TODO need to know how to determine signedness here
        if(type->bitWidth > 32)
          ret += StringFormat::Fmt("%le", val.dv[i]);
        else
          ret += StringFormat::Fmt("%e", val.fv[i]);
      }
      else if(type->scalarType == Type::Int)
      {
        // TODO need to know how to determine signedness here
        if(type->bitWidth > 32)
          ret += StringFormat::Fmt("%llu", val.u64v[i]);
        else
          ret += StringFormat::Fmt("%u", val.uv[i]);
      }
    }
    ret += ">";
  }
  else if(nullconst)
  {
    ret += "zeroinitializer";
  }
  else if(type->type == Type::Array)
  {
    ret += "[";
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i > 0)
        ret += ", ";

      ret += members[i].toString(withType);
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

      ret += members[i].toString(withType);
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
