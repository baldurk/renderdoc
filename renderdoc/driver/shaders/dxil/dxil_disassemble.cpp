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
#include "common/formatting.h"
#include "dxil_bytecode.h"

namespace DXIL
{
bool needsEscaping(const rdcstr &name)
{
  return name.find_first_not_of(
             "-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ$._0123456789") >= 0;
}

rdcstr escapeString(rdcstr str)
{
  for(size_t i = 0; i < str.size(); i++)
  {
    if(str[i] == '\'' || str[i] == '\\')
    {
      str.insert(i, "\\", 1);
      i++;
    }
    else if(str[i] == '\r' || str[i] == '\n' || str[i] == '\t' || !isprint(str[i]))
    {
      str.insert(i + 1, StringFormat::Fmt("%02X", str[i]));
      str[i] = '\\';
    }
  }

  str.push_back('"');
  str.insert(0, '"');

  return str;
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

  bool typesPrinted = false;

  for(size_t i = 0; i < m_Types.size(); i++)
  {
    const Type &typ = m_Types[i];

    if(typ.type == Type::Struct && !typ.name.empty())
    {
      rdcstr name = typ.toString();
      m_Disassembly += StringFormat::Fmt("%s = type {", name.c_str());
      bool first = true;
      for(const Type *t : typ.members)
      {
        if(!first)
          m_Disassembly += ",";
        first = false;
        m_Disassembly += StringFormat::Fmt(" %s", t->toString().c_str());
      }
      m_Disassembly += " }\n";
      typesPrinted = true;

      instructionLine++;
    }
  }

  if(typesPrinted)
  {
    m_Disassembly += "\n";
    instructionLine++;
  }

  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = m_GlobalVars[i];

    m_Disassembly += StringFormat::Fmt("@%s = ", escapeStringIfNeeded(g.name).c_str());
    if(g.external)
      m_Disassembly += "external ";
    if(g.isconst)
      m_Disassembly += "constant ";
    m_Disassembly += g.type->toString();

    if(g.align > 0)
      m_Disassembly += StringFormat::Fmt(", align %u", g.align);

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
                                   m_NamedMeta[i].distinct ? "distinct " : "");
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
        case SymbolType::Alias:
        case SymbolType::Literal: ret = "???"; break;
        case SymbolType::Metadata:
          if(withTypes)
            ret += "metadata ";
          if(s.idx < m_Metadata.size())
          {
            Metadata &m = m_Metadata[s.idx];
            if(m.value && m.val && m.val->nullconst)
              ret += StringFormat::Fmt("%s zeroinitializer", m.val->type->toString());
            else
              ret += StringFormat::Fmt("!%u", GetOrAssignMetaID(&m));
          }
          else
          {
            ret += GetFunctionMetadata(func, s.idx)->refString();
          }
          break;
        case SymbolType::Function: ret = "@" + escapeStringIfNeeded(m_Functions[s.idx].name); break;
        case SymbolType::GlobalVar:
          ret = "@" + escapeStringIfNeeded(m_GlobalVars[s.idx].name);
          break;
        case SymbolType::Constant: ret = GetFunctionValue(func, s.idx)->toString(withTypes); break;
        case SymbolType::Argument: ret = "%" + escapeStringIfNeeded(func.args[s.idx].name); break;
        case SymbolType::Instruction:
        {
          const Instruction &refinst = func.instructions[s.idx];
          if(withTypes)
            ret = refinst.type->toString() + " ";
          if(refinst.name.empty())
            ret += StringFormat::Fmt("%%%u", refinst.resultID);
          else
            ret += StringFormat::Fmt("%%%s", escapeStringIfNeeded(refinst.name).c_str());
          break;
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

      for(Instruction &inst : func.instructions)
      {
        inst.disassemblyLine = instructionLine;
        m_Disassembly += "  ";
        if(!inst.name.empty())
          m_Disassembly += "%" + escapeStringIfNeeded(inst.name) + " = ";
        else if(inst.resultID != ~0U)
          m_Disassembly += StringFormat::Fmt("%%%u = ", inst.resultID);

        bool debugCall = false;

        switch(inst.op)
        {
          case Instruction::Unknown: m_Disassembly += "??? "; break;
          case Instruction::Call:
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
          case Instruction::Trunc:
          case Instruction::ZExt:
          case Instruction::SExt:
          case Instruction::FToU:
          case Instruction::FToS:
          case Instruction::UToF:
          case Instruction::SToF:
          case Instruction::FPTrunc:
          case Instruction::FPExt:
          case Instruction::PtrToI:
          case Instruction::IToPtr:
          case Instruction::Bitcast:
          case Instruction::AddrSpaceCast:
          {
            switch(inst.op)
            {
              case Instruction::Trunc: m_Disassembly += "trunc "; break;
              case Instruction::ZExt: m_Disassembly += "zext "; break;
              case Instruction::SExt: m_Disassembly += "sext "; break;
              case Instruction::FToU: m_Disassembly += "fptoui "; break;
              case Instruction::FToS: m_Disassembly += "fptosi "; break;
              case Instruction::UToF: m_Disassembly += "uitofp "; break;
              case Instruction::SToF: m_Disassembly += "sitofp "; break;
              case Instruction::FPTrunc: m_Disassembly += "fptrunc "; break;
              case Instruction::FPExt: m_Disassembly += "fpext "; break;
              case Instruction::PtrToI: m_Disassembly += "ptrtoi "; break;
              case Instruction::IToPtr: m_Disassembly += "itoptr "; break;
              case Instruction::Bitcast: m_Disassembly += "bitcast "; break;
              case Instruction::AddrSpaceCast: m_Disassembly += "addrspacecast "; break;
              default: break;
            }

            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += " to ";
            m_Disassembly += inst.type->toString();
            break;
          }
          case Instruction::ExtractVal:
          {
            m_Disassembly += "extractvalue ";
            m_Disassembly += argToString(inst.args[0], true);
            for(size_t n = 1; n < inst.args.size(); n++)
              m_Disassembly += StringFormat::Fmt(", %llu", inst.args[n].idx);
            break;
          }
          case Instruction::FAdd:
          case Instruction::FSub:
          case Instruction::FMul:
          case Instruction::FDiv:
          case Instruction::FRem:
          case Instruction::Add:
          case Instruction::Sub:
          case Instruction::Mul:
          case Instruction::UDiv:
          case Instruction::SDiv:
          case Instruction::URem:
          case Instruction::SRem:
          case Instruction::ShiftLeft:
          case Instruction::LogicalShiftRight:
          case Instruction::ArithShiftRight:
          case Instruction::And:
          case Instruction::Or:
          case Instruction::Xor:
          {
            switch(inst.op)
            {
              case Instruction::FAdd: m_Disassembly += "fadd "; break;
              case Instruction::FSub: m_Disassembly += "fsub "; break;
              case Instruction::FMul: m_Disassembly += "fmul "; break;
              case Instruction::FDiv: m_Disassembly += "fdiv "; break;
              case Instruction::FRem: m_Disassembly += "frem "; break;
              case Instruction::Add: m_Disassembly += "add "; break;
              case Instruction::Sub: m_Disassembly += "sub "; break;
              case Instruction::Mul: m_Disassembly += "mul "; break;
              case Instruction::UDiv: m_Disassembly += "udiv "; break;
              case Instruction::SDiv: m_Disassembly += "sdiv "; break;
              case Instruction::URem: m_Disassembly += "urem "; break;
              case Instruction::SRem: m_Disassembly += "srem "; break;
              case Instruction::ShiftLeft: m_Disassembly += "shl "; break;
              case Instruction::LogicalShiftRight: m_Disassembly += "lshr "; break;
              case Instruction::ArithShiftRight: m_Disassembly += "ashr "; break;
              case Instruction::And: m_Disassembly += "and "; break;
              case Instruction::Or: m_Disassembly += "or "; break;
              case Instruction::Xor: m_Disassembly += "xor "; break;
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
          case Instruction::Ret: m_Disassembly += "ret " + inst.type->toString(); break;
          case Instruction::Unreachable: m_Disassembly += "unreachable"; break;
          case Instruction::Alloca:
          {
            m_Disassembly += "alloca ";
            m_Disassembly += inst.type->inner->toString();
            m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Instruction::GetElementPtr:
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
          case Instruction::Load:
          {
            m_Disassembly += "load ";
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
          case Instruction::Store:
          {
            m_Disassembly += "store ";
            m_Disassembly += argToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += StringFormat::Fmt(", align %u", inst.align);
            break;
          }
          case Instruction::FOrdFalse:
          case Instruction::FOrdEqual:
          case Instruction::FOrdGreater:
          case Instruction::FOrdGreaterEqual:
          case Instruction::FOrdLess:
          case Instruction::FOrdLessEqual:
          case Instruction::FOrdNotEqual:
          case Instruction::FOrd:
          case Instruction::FUnord:
          case Instruction::FUnordEqual:
          case Instruction::FUnordGreater:
          case Instruction::FUnordGreaterEqual:
          case Instruction::FUnordLess:
          case Instruction::FUnordLessEqual:
          case Instruction::FUnordNotEqual:
          case Instruction::FOrdTrue:
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
              case Instruction::FOrdFalse: m_Disassembly += "false "; break;
              case Instruction::FOrdEqual: m_Disassembly += "oeq "; break;
              case Instruction::FOrdGreater: m_Disassembly += "ogt "; break;
              case Instruction::FOrdGreaterEqual: m_Disassembly += "oge "; break;
              case Instruction::FOrdLess: m_Disassembly += "olt "; break;
              case Instruction::FOrdLessEqual: m_Disassembly += "ole "; break;
              case Instruction::FOrdNotEqual: m_Disassembly += "one "; break;
              case Instruction::FOrd: m_Disassembly += "ord "; break;
              case Instruction::FUnord: m_Disassembly += "uno "; break;
              case Instruction::FUnordEqual: m_Disassembly += "ueq "; break;
              case Instruction::FUnordGreater: m_Disassembly += "ugt "; break;
              case Instruction::FUnordGreaterEqual: m_Disassembly += "uge "; break;
              case Instruction::FUnordLess: m_Disassembly += "ult "; break;
              case Instruction::FUnordLessEqual: m_Disassembly += "ule "; break;
              case Instruction::FUnordNotEqual: m_Disassembly += "une "; break;
              case Instruction::FOrdTrue: m_Disassembly += "true "; break;
              default: break;
            }
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], false);
            break;
          }
          case Instruction::IEqual:
          case Instruction::INotEqual:
          case Instruction::UGreater:
          case Instruction::UGreaterEqual:
          case Instruction::ULess:
          case Instruction::ULessEqual:
          case Instruction::SGreater:
          case Instruction::SGreaterEqual:
          case Instruction::SLess:
          case Instruction::SLessEqual:
          {
            m_Disassembly += "icmp ";
            switch(inst.op)
            {
              case Instruction::IEqual: m_Disassembly += "eq "; break;
              case Instruction::INotEqual: m_Disassembly += "ne "; break;
              case Instruction::UGreater: m_Disassembly += "ugt "; break;
              case Instruction::UGreaterEqual: m_Disassembly += "uge "; break;
              case Instruction::ULess: m_Disassembly += "ult "; break;
              case Instruction::ULessEqual: m_Disassembly += "ule "; break;
              case Instruction::SGreater: m_Disassembly += "sgt "; break;
              case Instruction::SGreaterEqual: m_Disassembly += "sge "; break;
              case Instruction::SLess: m_Disassembly += "slt "; break;
              case Instruction::SLessEqual: m_Disassembly += "sle "; break;
              default: break;
            }
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], false);
            break;
          }
          case Instruction::Select:
          {
            m_Disassembly += "select ";
            m_Disassembly += argToString(inst.args[2], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += argToString(inst.args[1], true);
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
                StringFormat::Fmt(", !%s !%u", m_Kinds[inst.attachedMeta[m].first].c_str(),
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

        if(debugCall)
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
                " ; var:%s ", escapeString(GetDebugVarName(
                                  GetFunctionMetadata(func, inst.args[varIdx].idx)->dwarf)));
            m_Disassembly += GetFunctionMetadata(func, inst.args[exprIdx].idx)->valString();
          }
        }

        if(inst.funcCall && inst.funcCall->name.beginsWith("dx.op."))
        {
          if(inst.args[0].type == SymbolType::Constant)
          {
            uint32_t opcode = GetFunctionValue(func, inst.args[0].idx)->val.uv[0];
            if(opcode < ARRAY_COUNT(funcSigs))
            {
              m_Disassembly += "  ; ";
              m_Disassembly += funcSigs[opcode];
            }
          }
        }

        m_Disassembly += "\n";
        instructionLine++;
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
      m_Disassembly +=
          StringFormat::Fmt("!%u = %s%s\n", i, m_NumberedMeta[numIdx]->distinct ? "distinct " : "",
                            m_NumberedMeta[numIdx]->valString().c_str());
      numIdx++;
    }
    else if(dbgIdx < m_DebugLocations.size() && m_DebugLocations[dbgIdx].id == i)
    {
      m_Disassembly += StringFormat::Fmt("!%u = !DILocation(line: %llu, column: %llu, scope: %s", i,
                                         m_DebugLocations[dbgIdx].line, m_DebugLocations[dbgIdx].col,
                                         m_DebugLocations[dbgIdx].scope
                                             ? m_DebugLocations[dbgIdx].scope->refString().c_str()
                                             : "null");
      if(m_DebugLocations[dbgIdx].inlinedAt)
        m_Disassembly +=
            StringFormat::Fmt(", inlinedAt: %s", m_DebugLocations[dbgIdx].inlinedAt->refString());
      m_Disassembly += ")\n";
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
    case Void: return "void";
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
    case Pointer: return StringFormat::Fmt("%s*", inner->toString().c_str());
    case Array: return StringFormat::Fmt("[%u x %s]", elemCount, inner->toString().c_str());
    case Function: return declFunction(rdcstr());
    case Struct:
    {
      rdcstr ret;
      if(packedStruct)
        ret = "<{";
      else
        ret = "{";
      for(size_t i = 0; i < members.size(); i++)
      {
        if(i > 0)
          ret += ", ";
        ret += members[i]->toString();
      }
      if(packedStruct)
        ret += "}>";
      else
        ret += "}";
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

rdcstr Metadata::valString() const
{
  if(dwarf)
  {
    return dwarf->toString();
  }
  else if(value)
  {
    if(type == NULL)
    {
      return StringFormat::Fmt("!%s", escapeString(str).c_str());
    }
    else
    {
      if(val)
      {
        if(type != val->type)
          RDCERR("Type mismatch in metadata");
        return val->toString(true);
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
      else if(children[i]->value)
        ret += children[i]->valString();
      else
        ret += StringFormat::Fmt("!%u", children[i]->id);
    }
    ret += "}";

    return ret;
  }
}

rdcstr Value::toString(bool withType) const
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
      if(!isnan(orig) && !isinf(orig))
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
    ret += "{";
    for(size_t i = 0; i < members.size(); i++)
    {
      if(i > 0)
        ret += ", ";

      ret += members[i].toString(withType);
    }
    ret += "}";
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
