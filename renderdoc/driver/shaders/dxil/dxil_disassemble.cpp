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
static bool dxcStyleFormatting = true;
static char dxilIdentifier = '%';

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

template <typename T>
T getival(const Value *v)
{
  const Constant *c = cast<Constant>(v);
  if(c && c->isLiteral())
    return T(c->getU32());
  return T();
}

static const char *shaderNames[] = {
    "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
    "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
    "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
};

// clang-format off
static const char *funcNames[] = {
"TempRegLoad",
"TempRegStore",
"MinPrecXRegLoad",
"MinPrecXRegStore",
"LoadInput",
"StoreOutput",
"FAbs",
"Saturate",
"IsNaN",
"IsInf",
"IsFinite",
"IsNormal",
"Cos",
"Sin",
"Tan",
"Acos",
"Asin",
"Atan",
"Hcos",
"Hsin",
"Htan",
"Exp",
"Frc",
"Log",
"Sqrt",
"Rsqrt",
"Round_ne",
"Round_ni",
"Round_pi",
"Round_z",
"Bfrev",
"Countbits",
"FirstbitLo",
"FirstbitHi",
"FirstbitSHi",
"FMax",
"FMin",
"IMax",
"IMin",
"UMax",
"UMin",
"IMul",
"UMul",
"UDiv",
"UAddc",
"USubb",
"FMad",
"Fma",
"IMad",
"UMad",
"Msad",
"Ibfe",
"Ubfe",
"Bfi",
"Dot2",
"Dot3",
"Dot4",
"CreateHandle",
"CBufferLoad",
"CBufferLoadLegacy",
"Sample",
"SampleBias",
"SampleLevel",
"SampleGrad",
"SampleCmp",
"SampleCmpLevelZero",
"TextureLoad",
"TextureStore",
"BufferLoad",
"BufferStore",
"BufferUpdateCounter",
"CheckAccessFullyMapped",
"GetDimensions",
"TextureGather",
"TextureGatherCmp",
"Texture2DMSGetSamplePosition",
"RenderTargetGetSamplePosition",
"RenderTargetGetSampleCount",
"AtomicBinOp",
"AtomicCompareExchange",
"Barrier",
"CalculateLOD",
"Discard",
"DerivCoarseX",
"DerivCoarseY",
"DerivFineX",
"DerivFineY",
"EvalSnapped",
"EvalSampleIndex",
"EvalCentroid",
"SampleIndex",
"Coverage",
"InnerCoverage",
"ThreadId",
"GroupId",
"ThreadIdInGroup",
"FlattenedThreadIdInGroup",
"EmitStream",
"CutStream",
"EmitThenCutStream",
"GSInstanceID",
"MakeDouble",
"SplitDouble",
"LoadOutputControlPoint",
"LoadPatchConstant",
"DomainLocation",
"StorePatchConstant",
"OutputControlPointID",
"PrimitiveID",
"CycleCounterLegacy",
"WaveIsFirstLane",
"WaveGetLaneIndex",
"WaveGetLaneCount",
"WaveAnyTrue",
"WaveAllTrue",
"WaveActiveAllEqual",
"WaveActiveBallot",
"WaveReadLaneAt",
"WaveReadLaneFirst",
"WaveActiveOp",
"WaveActiveBit",
"WavePrefixOp",
"QuadReadLaneAt",
"QuadOp",
"BitcastI16toF16",
"BitcastF16toI16",
"BitcastI32toF32",
"BitcastF32toI32",
"BitcastI64toF64",
"BitcastF64toI64",
"LegacyF32ToF16",
"LegacyF16ToF32",
"LegacyDoubleToFloat",
"LegacyDoubleToSInt32",
"LegacyDoubleToUInt32",
"WaveAllBitCount",
"WavePrefixBitCount",
"AttributeAtVertex",
"ViewID",
"RawBufferLoad",
"RawBufferStore",
"InstanceID",
"InstanceIndex",
"HitKind",
"RayFlags",
"DispatchRaysIndex",
"DispatchRaysDimensions",
"WorldRayOrigin",
"WorldRayDirection",
"ObjectRayOrigin",
"ObjectRayDirection",
"ObjectToWorld",
"WorldToObject",
"RayTMin",
"RayTCurrent",
"IgnoreHit",
"AcceptHitAndEndSearch",
"TraceRay",
"ReportHit",
"CallShader",
"CreateHandleForLib",
"PrimitiveIndex",
"Dot2AddHalf",
"Dot4AddI8Packed",
"Dot4AddU8Packed",
"WaveMatch",
"WaveMultiPrefixOp",
"WaveMultiPrefixBitCount",
"SetMeshOutputCounts",
"EmitIndices",
"GetMeshPayload",
"StoreVertexOutput",
"StorePrimitiveOutput",
"DispatchMesh",
"WriteSamplerFeedback",
"WriteSamplerFeedbackBias",
"WriteSamplerFeedbackLevel",
"WriteSamplerFeedbackGrad",
"AllocateRayQuery",
"RayQuery_TraceRayInline",
"RayQuery_Proceed",
"RayQuery_Abort",
"RayQuery_CommitNonOpaqueTriangleHit",
"RayQuery_CommitProceduralPrimitiveHit",
"RayQuery_CommittedStatus",
"RayQuery_CandidateType",
"RayQuery_CandidateObjectToWorld3x4",
"RayQuery_CandidateWorldToObject3x4",
"RayQuery_CommittedObjectToWorld3x4",
"RayQuery_CommittedWorldToObject3x4",
"RayQuery_CandidateProceduralPrimitiveNonOpaque",
"RayQuery_CandidateTriangleFrontFace",
"RayQuery_CommittedTriangleFrontFace",
"RayQuery_CandidateTriangleBarycentrics",
"RayQuery_CommittedTriangleBarycentrics",
"RayQuery_RayFlags",
"RayQuery_WorldRayOrigin",
"RayQuery_WorldRayDirection",
"RayQuery_RayTMin",
"RayQuery_CandidateTriangleRayT",
"RayQuery_CommittedRayT",
"RayQuery_CandidateInstanceIndex",
"RayQuery_CandidateInstanceID",
"RayQuery_CandidateGeometryIndex",
"RayQuery_CandidatePrimitiveIndex",
"RayQuery_CandidateObjectRayOrigin",
"RayQuery_CandidateObjectRayDirection",
"RayQuery_CommittedInstanceIndex",
"RayQuery_CommittedInstanceID",
"RayQuery_CommittedGeometryIndex",
"RayQuery_CommittedPrimitiveIndex",
"RayQuery_CommittedObjectRayOrigin",
"RayQuery_CommittedObjectRayDirection",
"GeometryIndex",
"RayQuery_CandidateInstanceContributionToHitGroupIndex",
"RayQuery_CommittedInstanceContributionToHitGroupIndex",
"AnnotateHandle",
"CreateHandleFromBinding",
"CreateHandleFromHeap",
"Unpack4x8",
"Pack4x8",
"IsHelperLane",
"QuadVote",
"TextureGatherRaw",
"SampleCmpLevel",
"TextureStoreSample",
"WaveMatrix_Annotate",
"WaveMatrix_Depth",
"WaveMatrix_Fill",
"WaveMatrix_LoadRawBuf",
"WaveMatrix_LoadGroupShared",
"WaveMatrix_StoreRawBuf",
"WaveMatrix_StoreGroupShared",
"WaveMatrix_Multiply",
"WaveMatrix_MultiplyAccumulate",
"WaveMatrix_ScalarOp",
"WaveMatrix_SumAccumulate",
"WaveMatrix_Add",
"AllocateNodeOutputRecords",
"GetNodeRecordPtr",
"IncrementOutputCount",
"OutputComplete",
"GetInputRecordCount",
"FinishedCrossGroupSharing",
"BarrierByMemoryType",
"BarrierByMemoryHandle",
"BarrierByNodeRecordHandle",
"CreateNodeOutputHandle",
"IndexNodeHandle",
"AnnotateNodeHandle",
"CreateNodeInputRecordHandle",
"AnnotateNodeRecordHandle",
"NodeOutputIsValid",
"GetRemainingRecursionLevels",
"SampleCmpGrad",
"SampleCmpBias",
"StartVertexLocation",
"StartInstanceLocation",
};

static const char *funcSigs[] = {
"index",
"index,value",
"regIndex,index,component",
"regIndex,index,component,value",
"inputSigId,rowIndex,colIndex,gsVertexAxis",
"outputSigId,rowIndex,colIndex,value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b",
"a,b,c",
"a,b,c",
"a,b,c",
"a,b,c",
"a,b,c",
"a,b,c",
"a,b,c",
"width,offset,value,replacedValue",
"ax,ay,bx,by",
"ax,ay,az,bx,by,bz",
"ax,ay,az,aw,bx,by,bz,bw",
"resourceClass,rangeId,index,nonUniformIndex",
"handle,byteOffset,alignment",
"handle,regIndex",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue",
"srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2",
"srv,coord0,coord1,coord2,value0,value1,value2,value3,mask",
"srv,index,wot",
"uav,coord0,coord1,value0,value1,value2,value3,mask",
"uav,inc",
"status",
"handle,mipLevel",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel,compareValue",
"srv,index",
"index",
"",
"handle,atomicOp,offset0,offset1,offset2,newValue",
"handle,offset0,offset1,offset2,compareValue,newValue",
"barrierMode",
"handle,sampler,coord0,coord1,coord2,clamped",
"condition",
"value",
"value",
"value",
"value",
"inputSigId,inputRowIndex,inputColIndex,offsetX,offsetY",
"inputSigId,inputRowIndex,inputColIndex,sampleIndex",
"inputSigId,inputRowIndex,inputColIndex",
"",
"",
"",
"component",
"component",
"component",
"",
"streamId",
"streamId",
"streamId",
"",
"lo,hi",
"value",
"inputSigId,row,col,index",
"inputSigId,row,col",
"component",
"outputSigID,row,col,value",
"",
"",
"",
"",
"",
"",
"cond",
"cond",
"value",
"cond",
"value,lane",
"value",
"value,op,sop",
"value,op",
"value,op,sop",
"value,quadLane",
"value,op",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"value",
"inputSigId,inputRowIndex,inputColIndex,VertexID",
"",
"srv,index,elementOffset,mask,alignment",
"uav,index,elementOffset,value0,value1,value2,value3,mask,alignment",
"",
"",
"",
"",
"col",
"col",
"col",
"col",
"col",
"col",
"row,col",
"row,col",
"",
"",
"",
"",
"AccelerationStructure,RayFlags,InstanceInclusionMask,RayContributionToHitGroupIndex,MultiplierForGeometryContributionToShaderIndex,MissShaderIndex,Origin_X,Origin_Y,Origin_Z,TMin,Direction_X,Direction_Y,Direction_Z,TMax,payload",
"THit,HitKind,Attributes",
"ShaderIndex,Parameter",
"Resource",
"",
"acc,ax,ay,bx,by",
"acc,a,b",
"acc,a,b",
"value",
"value,mask0,mask1,mask2,mask3,op,sop",
"value,mask0,mask1,mask2,mask3",
"numVertices,numPrimitives",
"PrimitiveIndex,VertexIndex0,VertexIndex1,VertexIndex2",
"",
"outputSigId,rowIndex,colIndex,value,vertexIndex",
"outputSigId,rowIndex,colIndex,value,primitiveIndex",
"threadGroupCountX,threadGroupCountY,threadGroupCountZ,payload",
"feedbackTex,sampledTex,sampler,c0,c1,c2,c3,clamp",
"feedbackTex,sampledTex,sampler,c0,c1,c2,c3,bias,clamp",
"feedbackTex,sampledTex,sampler,c0,c1,c2,c3,lod",
"feedbackTex,sampledTex,sampler,c0,c1,c2,c3,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp",
"constRayFlags",
"rayQueryHandle,accelerationStructure,rayFlags,instanceInclusionMask,origin_X,origin_Y,origin_Z,tMin,direction_X,direction_Y,direction_Z,tMax",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle,t",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle,row,col",
"rayQueryHandle,row,col",
"rayQueryHandle,row,col",
"rayQueryHandle,row,col",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle,component",
"rayQueryHandle,component",
"rayQueryHandle",
"rayQueryHandle,component",
"rayQueryHandle,component",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle,component",
"rayQueryHandle,component",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle",
"rayQueryHandle,component",
"rayQueryHandle,component",
"",
"rayQueryHandle",
"rayQueryHandle",
"res,props",
"bind,index,nonUniformIndex",
"index,samplerHeap,nonUniformIndex",
"unpackMode,pk",
"packMode,x,y,z,w",
"",
"cond,op",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,lod",
"srv,coord0,coord1,coord2,value0,value1,value2,value3,mask,sampleIdx",
"waveMatrixPtr,waveMatProps",
"waveMatProps",
"waveMatrixPtr,value",
"waveMatrixPtr,rawBuf,offsetInBytes,strideInBytes,alignmentInBytes,colMajor",
"waveMatrixPtr,groupsharedPtr,startArrayIndex,strideInElements,colMajor",
"waveMatrixPtr,rawBuf,offsetInBytes,strideInBytes,alignmentInBytes,colMajor",
"waveMatrixPtr,groupsharedPtr,startArrayIndex,strideInElements,colMajor",
"waveMatrixAccumulator,waveMatrixLeft,waveMatrixRight",
"waveMatrixAccumulator,waveMatrixLeft,waveMatrixRight",
"waveMatrixPtr,op,value",
"waveMatrixFragment,waveMatrixInput",
"waveMatrixAccumulator,waveMatrixAccumulatorOrFragment",
"output,numRecords,perThread",
"recordhandle,arrayIndex",
"output,count,perThread",
"output",
"input",
"input",
"MemoryTypeFlags,SemanticFlags",
"object,SemanticFlags",
"object,SemanticFlags",
"MetadataIdx",
"NodeOutputHandle,ArrayIndex",
"node,props",
"MetadataIdx",
"noderecord,props",
"output",
"",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp",
"srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,bias,clamp",
"",
"",
};
// clang-format on

void Program::SettleIDs()
{
  if(m_SettledIDs)
    return;

  RDCASSERTEQUAL(m_NextMetaSlot, 0);
  RDCASSERT(m_MetaSlots.isEmpty());

  m_Accum.processGlobals(this, false);

  // need to disassemble the named metadata here so the IDs are assigned first before any
  // functions get dibs
  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    const NamedMetadata &m = *m_NamedMeta[i];
    for(size_t c = 0; c < m.children.size(); c++)
    {
      if(m.children[c])
        AssignMetaSlot(m_MetaSlots, m_NextMetaSlot, m.children[c]);
    }
  }
  rdcarray<Metadata *> &metaSlots = m_MetaSlots;
  uint32_t &nextMetaSlot = m_NextMetaSlot;
  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    m_Accum.processFunction(m_Functions[i]);

    const Function &func = *m_Functions[i];

    auto argMetaSlot = [this, &metaSlots, &nextMetaSlot](const Value *v) {
      if(const Metadata *meta = cast<Metadata>(v))
      {
        const Metadata &m = *meta;
        {
          const Constant *metaConst = cast<Constant>(m.value);
          const GlobalVar *metaGlobal = cast<GlobalVar>(m.value);
          const Instruction *metaInst = cast<Instruction>(m.value);
          if(m.isConstant && metaConst &&
             (metaConst->type->type == Type::Scalar || metaConst->type->type == Type::Vector ||
              metaConst->isUndef() || metaConst->isNULL() ||
              metaConst->type->name.beginsWith("class.matrix.")))
          {
          }
          else if(m.isConstant && (metaInst || metaGlobal))
          {
          }
          else
          {
            AssignMetaSlot(metaSlots, nextMetaSlot, (Metadata *)&m);
          }
        }
      }
    };

    if(!func.external)
    {
      for(size_t funcIdx = 0; funcIdx < func.instructions.size(); funcIdx++)
      {
        Instruction &inst = *func.instructions[funcIdx];
        switch(inst.op)
        {
          case Operation::NoOp:
          case Operation::Unreachable:
          case Operation::Alloca:
          case Operation::Fence: break;
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
          case Operation::ExtractVal:
          {
            argMetaSlot(inst.args[0]);
            break;
          }
          case Operation::Ret:
          {
            if(!inst.args.empty())
              argMetaSlot(inst.args[0]);
            break;
          }
          case Operation::Store:
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
          case Operation::ExtractElement:
          case Operation::InsertValue:
          case Operation::StoreAtomic:
          {
            argMetaSlot(inst.args[0]);
            argMetaSlot(inst.args[1]);
            break;
          }
          case Operation::Select:
          case Operation::InsertElement:
          case Operation::ShuffleVector:
          case Operation::Branch:
          {
            if(inst.args.size() > 1)
            {
              argMetaSlot(inst.args[0]);
              argMetaSlot(inst.args[1]);
              argMetaSlot(inst.args[2]);
            }
            else
            {
              argMetaSlot(inst.args[0]);
            }
            break;
          }
          case Operation::Call:
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
          case Operation::GetElementPtr:
          case Operation::Load:
          case Operation::LoadAtomic:
          case Operation::CompareExchange:
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
          case Operation::Phi:
          case Operation::Switch:
          {
            for(const Value *s : inst.args)
              argMetaSlot(s);
            break;
          }
        }

        if(inst.debugLoc != ~0U)
        {
          DebugLocation &debugLoc = m_DebugLocations[inst.debugLoc];
          AssignMetaSlot(metaSlots, nextMetaSlot, debugLoc);
        }

        const AttachedMetadata &attachedMeta = inst.getAttachedMeta();
        if(!attachedMeta.empty())
        {
          for(size_t m = 0; m < attachedMeta.size(); m++)
            AssignMetaSlot(metaSlots, nextMetaSlot, attachedMeta[m].second);
        }
      }
    }
    m_Accum.exitFunction();
  }

  m_FuncAttrGroups.clear();
  for(size_t i = 0; i < m_AttributeGroups.size(); i++)
  {
    if(!m_AttributeGroups[i])
      continue;

    if(m_AttributeGroups[i]->slotIndex != AttributeGroup::FunctionSlot)
      continue;

    if(m_FuncAttrGroups.contains(m_AttributeGroups[i]))
      continue;

    m_FuncAttrGroups.push_back(m_AttributeGroups[i]);
  }

  m_SettledIDs = true;
}

const Metadata *Program::FindMetadata(uint32_t slot) const
{
  for(int i = 0; i < m_MetaSlots.count(); ++i)
  {
    const Metadata *m = m_MetaSlots[i];
    if(m_MetaSlots[i]->slot == slot)
      return m;
  }
  return NULL;
}

rdcstr Program::ArgToString(const Value *v, bool withTypes, const rdcstr &attrString) const
{
  rdcstr ret;

  if(const Literal *lit = cast<Literal>(v))
  {
    if(withTypes)
      ret += "i32 ";
    ret += attrString;
    ret += StringFormat::Fmt("%llu", lit->literal);
  }
  else if(const Metadata *meta = cast<Metadata>(v))
  {
    const Metadata &m = *meta;
    if(withTypes)
      ret += "metadata ";
    ret += attrString;
    {
      const Constant *metaConst = cast<Constant>(m.value);
      const GlobalVar *metaGlobal = cast<GlobalVar>(m.value);
      const Instruction *metaInst = cast<Instruction>(m.value);
      if(m.isConstant && metaConst &&
         (metaConst->type->type == Type::Scalar || metaConst->type->type == Type::Vector ||
          metaConst->isUndef() || metaConst->isNULL() ||
          metaConst->type->name.beginsWith("class.matrix.")))
      {
        ret += metaConst->toString(withTypes);
      }
      else if(m.isConstant && metaInst)
      {
        ret += m.valString();
      }
      else if(m.isConstant && metaGlobal)
      {
        if(withTypes)
          ret += metaGlobal->type->toString() + " ";
        ret += "@" + escapeStringIfNeeded(metaGlobal->name);
      }
      else
      {
        ret += StringFormat::Fmt("!%u", GetMetaSlot(&m));
      }
    }
  }
  else if(const Function *func = cast<Function>(v))
  {
    ret += attrString;
    ret = "@" + escapeStringIfNeeded(func->name);
  }
  else if(const GlobalVar *global = cast<GlobalVar>(v))
  {
    if(withTypes)
      ret = global->type->toString() + " ";
    ret += attrString;
    ret += "@" + escapeStringIfNeeded(global->name);
  }
  else if(const Constant *c = cast<Constant>(v))
  {
    ret += attrString;
    ret = c->toString(withTypes);
  }
  else if(const Instruction *inst = cast<Instruction>(v))
  {
    if(withTypes)
      ret = inst->type->toString() + " ";
    ret += attrString;
    if(inst->getName().empty())
      ret += StringFormat::Fmt("%c%u", DXIL::dxilIdentifier, inst->slot);
    else
      ret += StringFormat::Fmt("%c%s", DXIL::dxilIdentifier,
                               escapeStringIfNeeded(inst->getName()).c_str());
  }
  else if(const Block *block = cast<Block>(v))
  {
    if(withTypes)
      ret = "label ";
    ret += attrString;
    if(block->name.empty())
      ret += StringFormat::Fmt("%c%u", DXIL::dxilIdentifier, block->slot);
    else
      ret +=
          StringFormat::Fmt("%c%s", DXIL::dxilIdentifier, escapeStringIfNeeded(block->name).c_str());
  }
  else
  {
    ret = "???";
  }

  return ret;
};

rdcstr Program::DisassembleComDats(int &instructionLine) const
{
  rdcstr ret;
  for(const rdcpair<uint64_t, rdcstr> &comdat : m_Comdats)
  {
    rdcstr type = "unknown";
    switch(comdat.first)
    {
      case 1: type = "any"; break;
      case 2: type = "exactmatch"; break;
      case 3: type = "largest"; break;
      case 4: type = "noduplicates"; break;
      case 5: type = "samesize"; break;
    }
    ret += StringFormat::Fmt("$%s = comdat %s\n", escapeStringIfNeeded(comdat.second).c_str(),
                             type.c_str());
    instructionLine++;
  }
  if(!m_Comdats.empty())
  {
    ret += "\n";
    instructionLine++;
  }
  return ret;
}

rdcstr Program::DisassembleTypes(int &instructionLine) const
{
  rdcstr ret;
  bool printedTypes = false;
  for(const Type *typ : m_Accum.printOrderTypes)
  {
    if(typ->type == Type::Struct && !typ->name.empty())
    {
      rdcstr name = typ->toString();
      ret += StringFormat::Fmt("%s = type { ", name.c_str());
      bool first = true;
      for(const Type *t : typ->members)
      {
        if(!first)
          ret += ", ";
        first = false;
        ret += StringFormat::Fmt("%s", t->toString().c_str());
      }
      if(typ->members.empty())
        ret += "}\n";
      else
        ret += " }\n";

      instructionLine++;
      printedTypes = true;
    }
  }
  if(printedTypes)
  {
    ret += "\n";
    instructionLine++;
  }
  return ret;
}

rdcstr Program::DisassembleGlobalVars(int &instructionLine) const
{
  rdcstr ret;
  for(size_t i = 0; i < m_GlobalVars.size(); i++)
  {
    const GlobalVar &g = *m_GlobalVars[i];

    ret += StringFormat::Fmt("@%s = ", escapeStringIfNeeded(g.name).c_str());
    switch(g.flags & GlobalFlags::LinkageMask)
    {
      case GlobalFlags::ExternalLinkage:
        if(!g.initialiser)
          ret += "external ";
        break;
      case GlobalFlags::PrivateLinkage: ret += "private "; break;
      case GlobalFlags::InternalLinkage: ret += "internal "; break;
      case GlobalFlags::LinkOnceAnyLinkage: ret += "linkonce "; break;
      case GlobalFlags::LinkOnceODRLinkage: ret += "linkonce_odr "; break;
      case GlobalFlags::WeakAnyLinkage: ret += "weak "; break;
      case GlobalFlags::WeakODRLinkage: ret += "weak_odr "; break;
      case GlobalFlags::CommonLinkage: ret += "common "; break;
      case GlobalFlags::AppendingLinkage: ret += "appending "; break;
      case GlobalFlags::ExternalWeakLinkage: ret += "extern_weak "; break;
      case GlobalFlags::AvailableExternallyLinkage: ret += "available_externally "; break;
      default: break;
    }

    if(g.flags & GlobalFlags::LocalUnnamedAddr)
      ret += "local_unnamed_addr ";
    else if(g.flags & GlobalFlags::GlobalUnnamedAddr)
      ret += "unnamed_addr ";
    if(g.type->addrSpace != Type::PointerAddrSpace::Default)
      ret += StringFormat::Fmt("addrspace(%d) ", g.type->addrSpace);
    if(g.flags & GlobalFlags::IsConst)
      ret += "constant ";
    else
      ret += "global ";

    if(g.initialiser)
      ret += g.initialiser->toString(true);
    else
      ret += g.type->inner->toString();

    if(g.align > 0)
      ret += StringFormat::Fmt(", align %u", g.align);

    if(g.section >= 0)
      ret += StringFormat::Fmt(", section %s", escapeString(m_Sections[g.section]).c_str());

    ret += "\n";
    instructionLine++;
  }

  if(!m_GlobalVars.empty())
  {
    ret += "\n";
    instructionLine++;
  }
  return ret;
}

rdcstr Program::DisassembleNamedMeta() const
{
  rdcstr ret;

  for(size_t i = 0; i < m_NamedMeta.size(); i++)
  {
    const NamedMetadata &m = *m_NamedMeta[i];

    ret += StringFormat::Fmt("!%s = %s!{", m.name.c_str(), m.isDistinct ? "distinct " : "");
    for(size_t c = 0; c < m.children.size(); c++)
    {
      if(c != 0)
        ret += ", ";
      if(m.children[c])
        ret += StringFormat::Fmt("!%u", GetMetaSlot(m.children[c]));
      else
        ret += "null";
    }

    ret += "}\n";
  }
  if(!m_NamedMeta.empty())
    ret += "\n";
  return ret;
}

rdcstr Program::DisassembleFuncAttrGroups() const
{
  rdcstr ret;
  for(size_t i = 0; i < m_FuncAttrGroups.size(); i++)
  {
    ret += StringFormat::Fmt("attributes #%zu = { %s }\n", i,
                             m_FuncAttrGroups[i]->toString(true).c_str());
  }
  if(!m_FuncAttrGroups.empty())
    ret += "\n";
  return ret;
}

rdcstr Program::DisassembleMeta() const
{
  rdcstr ret;
  size_t numIdx = 0;
  size_t dbgIdx = 0;

  for(uint32_t i = 0; i < m_NextMetaSlot; i++)
  {
    if(numIdx < m_MetaSlots.size() && m_MetaSlots[numIdx]->slot == i)
    {
      rdcstr metaline =
          StringFormat::Fmt("!%u = %s%s\n", i, m_MetaSlots[numIdx]->isDistinct ? "distinct " : "",
                            m_MetaSlots[numIdx]->valString().c_str());
#if ENABLED(DXC_COMPATIBLE_DISASM)
      for(size_t c = 0; c < metaline.size(); c += 4096)
        ret += metaline.substr(c, 4096);
#else
      ret += metaline;
#endif
      if(m_MetaSlots[numIdx]->dwarf)
        m_MetaSlots[numIdx]->dwarf->setID(i);
      numIdx++;
    }
    else if(dbgIdx < m_DebugLocations.size() && m_DebugLocations[dbgIdx].slot == i)
    {
      ret += StringFormat::Fmt("!%u = %s\n", i, m_DebugLocations[dbgIdx].toString().c_str());
      dbgIdx++;
    }
    else
    {
      RDCERR("Couldn't find meta ID %u", i);
    }
  }
  if(m_NextMetaSlot > 0)
    ret += "\n";

  return ret;
}

void Program::DisassemblyAddNewLine(int countLines)
{
  for(int i = 0; i < countLines; ++i)
    m_Disassembly += "\n";

  m_DisassemblyInstructionLine += countLines;
}

const rdcstr &Program::GetDisassembly(bool dxcStyle)
{
  if(m_Disassembly.empty() || (dxcStyle != m_DXCStyle))
  {
    m_DXCStyle = dxcStyle;
    SettleIDs();

    if(dxcStyle)
      MakeDXCDisassemblyString();
    else
      MakeRDDisassemblyString();
  }
  return m_Disassembly;
}

void Program::MakeDXCDisassemblyString()
{
  DXIL::dxcStyleFormatting = true;
  DXIL::dxilIdentifier = '%';

  m_Disassembly.clear();
#if DISABLED(DXC_COMPATIBLE_DISASM)
  m_Disassembly += StringFormat::Fmt("; %s Shader, compiled under SM%u.%u\n\n",
                                     shaderNames[int(m_Type)], m_Major, m_Minor);
#endif
  m_Disassembly += StringFormat::Fmt("target datalayout = \"%s\"\n", m_Datalayout.c_str());
  m_Disassembly += StringFormat::Fmt("target triple = \"%s\"\n\n", m_Triple.c_str());

  m_DisassemblyInstructionLine = 6;

  m_Disassembly += DisassembleComDats(m_DisassemblyInstructionLine);
  m_Disassembly += DisassembleTypes(m_DisassemblyInstructionLine);
  m_Disassembly += DisassembleGlobalVars(m_DisassemblyInstructionLine);

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = *m_Functions[i];

    m_Accum.processFunction(m_Functions[i]);

    if(func.attrs && func.attrs->functionSlot)
    {
      rdcstr funcAttrs = func.attrs->functionSlot->toString(false).c_str();
      if(!funcAttrs.empty())
      {
        m_Disassembly += StringFormat::Fmt("; Function Attrs: %s", funcAttrs.c_str());
        DisassemblyAddNewLine();
      }
    }

    m_Disassembly += (func.external ? "declare " : "define ");
    if(func.internalLinkage)
      m_Disassembly += "internal ";
    m_Disassembly +=
        func.type->declFunction("@" + escapeStringIfNeeded(func.name), func.args, func.attrs);

    if(func.comdatIdx < m_Comdats.size())
      m_Disassembly += StringFormat::Fmt(
          " comdat($%s)", escapeStringIfNeeded(m_Comdats[func.comdatIdx].second).c_str());

    if(func.align)
      m_Disassembly += StringFormat::Fmt(" align %u", (1U << func.align) >> 1);

    if(func.attrs && func.attrs->functionSlot)
      m_Disassembly += StringFormat::Fmt(" #%u", m_FuncAttrGroups.indexOf(func.attrs->functionSlot));

    if(!func.external)
    {
      m_Disassembly += " {";
      DisassemblyAddNewLine();

      size_t curBlock = 0;

      // if the first block has a name, use it
      if(!func.blocks[curBlock]->name.empty())
      {
        m_Disassembly +=
            StringFormat::Fmt("%s:", escapeStringIfNeeded(func.blocks[curBlock]->name).c_str());
        DisassemblyAddNewLine();
      }

      for(size_t funcIdx = 0; funcIdx < func.instructions.size(); funcIdx++)
      {
        Instruction &inst = *func.instructions[funcIdx];

        inst.disassemblyLine = m_DisassemblyInstructionLine;
        m_Disassembly += "  ";
        if(!inst.getName().empty())
          m_Disassembly += StringFormat::Fmt("%c%s = ", DXIL::dxilIdentifier,
                                             escapeStringIfNeeded(inst.getName()).c_str());
        else if(inst.slot != ~0U)
          m_Disassembly += StringFormat::Fmt("%c%u = ", DXIL::dxilIdentifier, inst.slot);

        bool debugCall = false;

        switch(inst.op)
        {
          case Operation::NoOp: m_Disassembly += "??? "; break;
          case Operation::Call:
          {
            rdcstr funcCallName = inst.getFuncCall()->name;
            m_Disassembly += "call " + inst.type->toString();
            m_Disassembly += " @" + escapeStringIfNeeded(funcCallName);
            m_Disassembly += "(";
            bool first = true;

            const AttributeSet *paramAttrs = inst.getParamAttrs();
            // attribute args start from 1
            size_t argIdx = 1;
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";
              first = false;

              // see if we have param attrs for this param
              rdcstr attrString;
              if(paramAttrs && argIdx < paramAttrs->groupSlots.size() &&
                 paramAttrs->groupSlots[argIdx])
              {
                attrString = paramAttrs->groupSlots[argIdx]->toString(true) + " ";
              }

              m_Disassembly += ArgToString(s, true, attrString);

              argIdx++;
            }
            m_Disassembly += ")";
            debugCall = funcCallName.beginsWith("llvm.dbg.");

            if(paramAttrs && paramAttrs->functionSlot)
              m_Disassembly +=
                  StringFormat::Fmt(" #%u", m_FuncAttrGroups.indexOf(paramAttrs->functionSlot));
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

            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += " to ";
            m_Disassembly += inst.type->toString();
            break;
          }
          case Operation::ExtractVal:
          {
            m_Disassembly += "extractvalue ";
            m_Disassembly += ArgToString(inst.args[0], true);
            for(size_t n = 1; n < inst.args.size(); n++)
              m_Disassembly += StringFormat::Fmt(", %llu", cast<Literal>(inst.args[n])->literal);
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

            rdcstr opFlagsStr = ToStr(inst.opFlags());
            {
              int offs = opFlagsStr.indexOf('|');
              while(offs >= 0)
              {
                opFlagsStr.erase((size_t)offs, 2);
                offs = opFlagsStr.indexOf('|');
              }
            }
            m_Disassembly += opFlagsStr;
            if(inst.opFlags() != InstructionFlags::NoFlags)
              m_Disassembly += " ";

            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += ArgToString(s, first);
              first = false;
            }

            break;
          }
          case Operation::Ret:
          {
            if(inst.args.empty())
              m_Disassembly += "ret " + inst.type->toString();
            else
              m_Disassembly += "ret " + ArgToString(inst.args[0], true);
            break;
          }
          case Operation::Unreachable: m_Disassembly += "unreachable"; break;
          case Operation::Alloca:
          {
            m_Disassembly += "alloca ";
            m_Disassembly += inst.type->inner->toString();
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::GetElementPtr:
          {
            m_Disassembly += "getelementptr ";
            if(inst.opFlags() & InstructionFlags::InBounds)
              m_Disassembly += "inbounds ";
            m_Disassembly += inst.args[0]->type->inner->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += ArgToString(s, true);
              first = false;
            }
            break;
          }
          case Operation::Load:
          {
            m_Disassembly += "load ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += inst.type->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += ArgToString(s, true);
              first = false;
            }
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::Store:
          {
            m_Disassembly += "store ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += ArgToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[0], true);
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
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
            rdcstr opFlagsStr = ToStr(inst.opFlags());
            {
              int offs = opFlagsStr.indexOf('|');
              while(offs >= 0)
              {
                opFlagsStr.erase((size_t)offs, 2);
                offs = opFlagsStr.indexOf('|');
              }
            }
            m_Disassembly += opFlagsStr;
            if(inst.opFlags() != InstructionFlags::NoFlags)
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
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], false);
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
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], false);
            break;
          }
          case Operation::Select:
          {
            m_Disassembly += "select ";
            m_Disassembly += ArgToString(inst.args[2], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], true);
            break;
          }
          case Operation::ExtractElement:
          {
            m_Disassembly += "extractelement ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], true);
            break;
          }
          case Operation::InsertElement:
          {
            m_Disassembly += "insertelement ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[2], true);
            break;
          }
          case Operation::ShuffleVector:
          {
            m_Disassembly += "shufflevector ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[2], true);
            break;
          }
          case Operation::InsertValue:
          {
            m_Disassembly += "insertvalue ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], true);
            for(size_t a = 2; a < inst.args.size(); a++)
            {
              m_Disassembly += ", " + ToStr(cast<Literal>(inst.args[a])->literal);
            }
            break;
          }
          case Operation::Branch:
          {
            m_Disassembly += "br ";
            if(inst.args.size() > 1)
            {
              m_Disassembly += ArgToString(inst.args[2], true);
              m_Disassembly += StringFormat::Fmt(", %s", ArgToString(inst.args[0], true).c_str());
              m_Disassembly += StringFormat::Fmt(", %s", ArgToString(inst.args[1], true).c_str());
            }
            else
            {
              m_Disassembly += ArgToString(inst.args[0], true);
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
                  StringFormat::Fmt("[ %s, %s ]", ArgToString(inst.args[a], false).c_str(),
                                    ArgToString(inst.args[a + 1], false).c_str());
            }
            break;
          }
          case Operation::Switch:
          {
            m_Disassembly += "switch ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[1], true);
            m_Disassembly += " [";
            DisassemblyAddNewLine();
            for(size_t a = 2; a < inst.args.size(); a += 2)
            {
              m_Disassembly +=
                  StringFormat::Fmt("    %s, %s", ArgToString(inst.args[a], true).c_str(),
                                    ArgToString(inst.args[a + 1], true).c_str());
              DisassemblyAddNewLine();
            }
            m_Disassembly += "  ]";
            break;
          }
          case Operation::Fence:
          {
            m_Disassembly += "fence ";
            if(inst.opFlags() & InstructionFlags::SingleThread)
              m_Disassembly += "singlethread ";
            switch((inst.opFlags() & InstructionFlags::SuccessOrderMask))
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
          case Operation::LoadAtomic:
          {
            m_Disassembly += "load atomic ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += inst.type->toString();
            m_Disassembly += ", ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += ArgToString(s, true);
              first = false;
            }
            m_Disassembly += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::StoreAtomic:
          {
            m_Disassembly += "store atomic ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";
            m_Disassembly += ArgToString(inst.args[1], true);
            m_Disassembly += ", ";
            m_Disassembly += ArgToString(inst.args[0], true);
            m_Disassembly += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::CompareExchange:
          {
            m_Disassembly += "cmpxchg ";
            if(inst.opFlags() & InstructionFlags::Weak)
              m_Disassembly += "weak ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              m_Disassembly += "volatile ";

            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += ArgToString(s, true);
              first = false;
            }

            m_Disassembly += " ";
            if(inst.opFlags() & InstructionFlags::SingleThread)
              m_Disassembly += "singlethread ";
            switch((inst.opFlags() & InstructionFlags::SuccessOrderMask))
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
            switch((inst.opFlags() & InstructionFlags::FailureOrderMask))
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
            if(inst.opFlags() & InstructionFlags::Volatile)
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
            for(const Value *s : inst.args)
            {
              if(!first)
                m_Disassembly += ", ";

              m_Disassembly += ArgToString(s, true);
              first = false;
            }

            m_Disassembly += " ";
            if(inst.opFlags() & InstructionFlags::SingleThread)
              m_Disassembly += "singlethread ";
            switch((inst.opFlags() & InstructionFlags::SuccessOrderMask))
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
          m_Disassembly += StringFormat::Fmt(", !dbg !%u", GetMetaSlot(&debugLoc));
        }

        const AttachedMetadata &attachedMeta = inst.getAttachedMeta();
        if(!attachedMeta.empty())
        {
          for(size_t m = 0; m < attachedMeta.size(); m++)
          {
            m_Disassembly +=
                StringFormat::Fmt(", !%s !%u", m_Kinds[(size_t)attachedMeta[m].first].c_str(),
                                  GetMetaSlot(attachedMeta[m].second));
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
          if(inst.getFuncCall()->name == "llvm.dbg.value")
          {
            varIdx = 2;
            exprIdx = 3;
          }
          else if(inst.getFuncCall()->name == "llvm.dbg.declare")
          {
            varIdx = 1;
            exprIdx = 2;
          }

          if(varIdx > 0)
          {
            Metadata *var = cast<Metadata>(inst.args[varIdx]);
            Metadata *expr = cast<Metadata>(inst.args[exprIdx]);
            RDCASSERT(var);
            RDCASSERT(expr);
            m_Disassembly +=
                StringFormat::Fmt(" ; var:%s ", escapeString(GetDebugVarName(var->dwarf)).c_str());
            m_Disassembly += expr->valString();

            rdcstr funcName = GetFunctionScopeName(var->dwarf);
            if(!funcName.empty())
              m_Disassembly += StringFormat::Fmt(" func:%s", escapeString(funcName).c_str());
          }
        }

        if(inst.getFuncCall() && inst.getFuncCall()->name.beginsWith("dx.op."))
        {
          if(Constant *op = cast<Constant>(inst.args[0]))
          {
            uint32_t opcode = op->getU32();
            if(opcode < ARRAY_COUNT(funcNames))
            {
              m_Disassembly += "  ; ";
              m_Disassembly += funcNames[opcode];
              m_Disassembly += "(";
              m_Disassembly += funcSigs[opcode];
              m_Disassembly += ")";
            }
          }
        }

        if(inst.getFuncCall() && inst.getFuncCall()->name.beginsWith("dx.op.annotateHandle"))
        {
          if(const Constant *props = cast<Constant>(inst.args[2]))
          {
            const Constant *packed[2];
            if(props && !props->isNULL() && props->getMembers().size() == 2 &&
               (packed[0] = cast<Constant>(props->getMembers()[0])) != NULL &&
               (packed[1] = cast<Constant>(props->getMembers()[1])) != NULL)
            {
              uint32_t packedProps[2] = {};
              packedProps[0] = packed[0]->getU32();
              packedProps[1] = packed[1]->getU32();

              bool uav = (packedProps[0] & (1 << 12)) != 0;
              bool rov = (packedProps[0] & (1 << 13)) != 0;
              bool globallyCoherent = (packedProps[0] & (1 << 14)) != 0;
              bool sampelCmpOrCounter = (packedProps[0] & (1 << 15)) != 0;
              ResourceKind resKind = (ResourceKind)(packedProps[0] & 0xFF);
              ResourceClass resClass;
              if(sampelCmpOrCounter && resKind == ResourceKind::Sampler)
                resKind = ResourceKind::SamplerComparison;
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
              uint8_t compCount = (packedProps[1] & 0xFF00) >> 8;

              uint8_t feedbackType = packedProps[1] & 0xFF;

              uint32_t structStride = packedProps[1];

              switch(resKind)
              {
                case ResourceKind::Unknown: m_Disassembly += "Unknown"; break;
                case ResourceKind::Texture1D:
                case ResourceKind::Texture2D:
                case ResourceKind::Texture2DMS:
                case ResourceKind::Texture3D:
                case ResourceKind::TextureCube:
                case ResourceKind::Texture1DArray:
                case ResourceKind::Texture2DArray:
                case ResourceKind::Texture2DMSArray:
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
                    case ResourceKind::Texture2DMS: m_Disassembly += "Texture2DMS"; break;
                    case ResourceKind::Texture3D: m_Disassembly += "Texture3D"; break;
                    case ResourceKind::TextureCube: m_Disassembly += "TextureCube"; break;
                    case ResourceKind::Texture1DArray: m_Disassembly += "Texture1DArray"; break;
                    case ResourceKind::Texture2DArray: m_Disassembly += "Texture2DArray"; break;
                    case ResourceKind::Texture2DMSArray: m_Disassembly += "Texture2DMSArray"; break;
                    case ResourceKind::TextureCubeArray: m_Disassembly += "TextureCubeArray"; break;
                    case ResourceKind::TypedBuffer: m_Disassembly += "TypedBuffer"; break;
                    default: break;
                  }
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
                  m_Disassembly += StringFormat::Fmt("<stride=%u", structStride);
                  if(sampelCmpOrCounter)
                    m_Disassembly += ", counter";
                  m_Disassembly += ">";
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

              if(resKind == ResourceKind::FeedbackTexture2D ||
                 resKind == ResourceKind::FeedbackTexture2DArray)
              {
                if(feedbackType == 0)
                  m_Disassembly += "<MinMip>";
                else if(feedbackType == 1)
                  m_Disassembly += "<MipRegionUsed>";
                else
                  m_Disassembly += "<Invalid>";
              }
              else if(resKind == ResourceKind::Texture1D || resKind == ResourceKind::Texture2D ||
                      resKind == ResourceKind::Texture3D || resKind == ResourceKind::TextureCube ||
                      resKind == ResourceKind::Texture1DArray ||
                      resKind == ResourceKind::Texture2DArray ||
                      resKind == ResourceKind::TextureCubeArray ||
                      resKind == ResourceKind::TypedBuffer || resKind == ResourceKind::Texture2DMS ||
                      resKind == ResourceKind::Texture2DMSArray)
              {
                m_Disassembly += "<";
                if(compCount > 1)
                  m_Disassembly += StringFormat::Fmt("%dx", compCount);
                m_Disassembly += StringFormat::Fmt("%s>", ToStr(compType).c_str());
              }
            }
          }
        }

        DisassemblyAddNewLine();

        // if this is the last instruction don't print the next block's label
        if(funcIdx == func.instructions.size() - 1)
          break;

        if(inst.op == Operation::Branch || inst.op == Operation::Unreachable ||
           inst.op == Operation::Switch || inst.op == Operation::Ret)
        {
          DisassemblyAddNewLine();

          curBlock++;

          rdcstr labelName;

          if(func.blocks[curBlock]->name.empty())
            labelName = StringFormat::Fmt("; <label>:%u", func.blocks[curBlock]->slot);
          else
            labelName =
                StringFormat::Fmt("%s: ", escapeStringIfNeeded(func.blocks[curBlock]->name).c_str());

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
          for(const Block *pred : func.blocks[curBlock]->preds)
          {
            if(!first)
              labelName += ", ";
            first = false;
            if(pred->name.empty())
              labelName += StringFormat::Fmt("%c%u", DXIL::dxilIdentifier, pred->slot);
            else
              labelName += StringFormat::Fmt("%c%s", DXIL::dxilIdentifier,
                                             escapeStringIfNeeded(pred->name).c_str());
          }
#endif

          m_Disassembly += labelName;
          DisassemblyAddNewLine();
        }
      }
      m_Disassembly += "}";
      DisassemblyAddNewLine(2);
    }
    else
    {
      DisassemblyAddNewLine(2);
    }

    m_Accum.exitFunction();
  }

  m_Disassembly += DisassembleFuncAttrGroups();
  m_Disassembly += DisassembleNamedMeta();
  m_Disassembly += DisassembleMeta();

  if(m_Disassembly.back() != '\n')
    m_Disassembly += "\n";
}

struct InputOutput
{
  rdcstr name;
};

void Program::MakeRDDisassemblyString()
{
  DXIL::dxcStyleFormatting = false;
  DXIL::dxilIdentifier = '_';

  m_Disassembly.clear();
  m_DisassemblyInstructionLine = 1;

  m_Disassembly += StringFormat::Fmt("; %s Shader, compiled under SM%u.%u",
                                     shaderNames[int(m_Type)], m_Major, m_Minor);
  DisassemblyAddNewLine(2);

  // TODO: output structs using named meta data if it exists
  m_Disassembly += DisassembleTypes(m_DisassemblyInstructionLine);
  m_Disassembly += DisassembleGlobalVars(m_DisassemblyInstructionLine);

  rdcarray<InputOutput> inputs;
  rdcarray<InputOutput> outputs;

  // Decode entry points
  // TODO: handle resources: SRVs, UAVs, CBs, Samplers
  rdcarray<EntryPoint> entryPoints;
  FetchEntryPoints(entryPoints);
  for(size_t e = 0; e < entryPoints.size(); ++e)
  {
    const EntryPoint &entryPoint = entryPoints[e];
    m_Disassembly += entryPoint.name + "()";
    DisassemblyAddNewLine();
    m_Disassembly += "{";
    DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.inputs.size(); ++i)
    {
      const EntryPoint::Signature &sig = entryPoint.inputs[i];
      VarType varType = VarTypeForComponentType(sig.type);
      m_Disassembly += "  Input[" + ToStr(i) + "] " + ToStr(varType).c_str();

      if(sig.rows > 1)
        m_Disassembly += ToStr(sig.rows) + "x";
      if(sig.cols > 1)
        m_Disassembly += ToStr(sig.cols);

      m_Disassembly += " " + sig.name + ";";
      DisassemblyAddNewLine();

      InputOutput input;
      input.name = sig.name;
      inputs.push_back(input);
    }
    if(!entryPoint.inputs.empty())
      DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.outputs.size(); ++i)
    {
      const EntryPoint::Signature &sig = entryPoint.outputs[i];
      VarType varType = VarTypeForComponentType(sig.type);
      m_Disassembly += "  Output[" + ToStr(i) + "] " + ToStr(varType).c_str();

      if(sig.rows > 1)
        m_Disassembly += ToStr(sig.rows) + "x";
      if(sig.cols > 1)
        m_Disassembly += ToStr(sig.cols);

      m_Disassembly += " " + sig.name + ";";
      DisassemblyAddNewLine();

      InputOutput output;
      output.name = sig.name;
      outputs.push_back(output);
    }
    m_Disassembly += "}";
    DisassemblyAddNewLine();
  }

  const char *swizzle = "xyzw";

  DisassemblyAddNewLine();

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = *m_Functions[i];

    m_Accum.processFunction(m_Functions[i]);

    if(func.external)
      continue;

    m_Disassembly += (func.external ? "declare " : "");
    if(func.internalLinkage)
      m_Disassembly += "internal ";
    m_Disassembly +=
        func.type->declFunction("@" + escapeStringIfNeeded(func.name), func.args, func.attrs);

    if(func.comdatIdx < m_Comdats.size())
      m_Disassembly += StringFormat::Fmt(
          " comdat($%s)", escapeStringIfNeeded(m_Comdats[func.comdatIdx].second).c_str());

    if(func.align)
      m_Disassembly += StringFormat::Fmt(" align %u", (1U << func.align) >> 1);

    if(func.attrs && func.attrs->functionSlot)
      m_Disassembly += StringFormat::Fmt(" #%u", m_FuncAttrGroups.indexOf(func.attrs->functionSlot));

    if(!func.external)
    {
      DisassemblyAddNewLine();
      m_Disassembly += "{";
      DisassemblyAddNewLine();

      size_t curBlock = 0;

      // if the first block has a name, use it
      if(!func.blocks[curBlock]->name.empty())
      {
        m_Disassembly +=
            StringFormat::Fmt("%s:", escapeStringIfNeeded(func.blocks[curBlock]->name).c_str());
        DisassemblyAddNewLine(2);
      }

      for(size_t funcIdx = 0; funcIdx < func.instructions.size(); funcIdx++)
      {
        Instruction &inst = *func.instructions[funcIdx];

        inst.disassemblyLine = m_DisassemblyInstructionLine;
        rdcstr lineStr;
        if(!inst.type->isVoid())
        {
          lineStr += inst.type->toString();
          lineStr += " ";
        }
        if(!inst.getName().empty())
          lineStr += StringFormat::Fmt("%c%s = ", DXIL::dxilIdentifier,
                                       escapeStringIfNeeded(inst.getName()).c_str());
        else if(inst.slot != ~0U)
          lineStr += StringFormat::Fmt("%c%u = ", DXIL::dxilIdentifier, inst.slot);

        bool showDxFuncName = false;
        rdcstr commentStr;

        switch(inst.op)
        {
          case Operation::NoOp: lineStr += "??? "; break;
          case Operation::Call:
          {
            rdcstr funcCallName = inst.getFuncCall()->name;
            showDxFuncName = funcCallName.beginsWith("dx.op");
            if(funcCallName.beginsWith("dx.op.loadInput"))
            {
              showDxFuncName = false;
              uint32_t dxopCode = getival<uint32_t>(inst.args[0]);
              RDCASSERTEQUAL(dxopCode, 4);
              uint32_t inputIdx = getival<uint32_t>(inst.args[1]);
              lineStr += inputs[inputIdx].name;
              lineStr += ".";
              // TODO: decode colIndex access
              uint32_t componentIdx = getival<uint32_t>(inst.args[3]);
              lineStr += swizzle[componentIdx & 0x3];
            }
            else if(funcCallName.beginsWith("dx.op.storeOutput"))
            {
              showDxFuncName = false;
              uint32_t dxopCode = getival<uint32_t>(inst.args[0]);
              RDCASSERTEQUAL(dxopCode, 5);
              uint32_t outputIdx = getival<uint32_t>(inst.args[1]);
              lineStr += outputs[outputIdx].name;
              lineStr += ".";
              // TODO: decode colIndex access
              uint32_t componentIdx = getival<uint32_t>(inst.args[3]);
              lineStr += swizzle[componentIdx & 0x3];
              lineStr += " = " + ArgToString(inst.args[4], false);
            }
            else if(funcCallName.beginsWith("llvm.dbg."))
            {
            }
            else
            {
              if(!showDxFuncName)
              {
                lineStr += escapeStringIfNeeded(funcCallName);
              }
              else
              {
                if(Constant *op = cast<Constant>(inst.args[0]))
                {
                  uint32_t opcode = op->getU32();
                  if(opcode < ARRAY_COUNT(funcNames))
                    lineStr += funcNames[opcode];
                  else
                    lineStr += escapeStringIfNeeded(funcCallName);
                }
              }
              lineStr += "(";
              bool first = true;

              const AttributeSet *paramAttrs = inst.getParamAttrs();
              // attribute args start from 1
              size_t argIdx = 1;
              for(const Value *s : inst.args)
              {
                if(!first)
                  lineStr += ", ";

                // see if we have param attrs for this param
                rdcstr attrString;
                if(paramAttrs && argIdx < paramAttrs->groupSlots.size() &&
                   paramAttrs->groupSlots[argIdx])
                {
                  attrString = paramAttrs->groupSlots[argIdx]->toString(true) + " ";
                }

                if(!showDxFuncName || argIdx > 1)
                {
                  lineStr += ArgToString(s, false, attrString);
                  first = false;
                }

                argIdx++;
              }
              lineStr += ")";

              if(paramAttrs && paramAttrs->functionSlot)
                lineStr +=
                    StringFormat::Fmt(" #%u", m_FuncAttrGroups.indexOf(paramAttrs->functionSlot));
            }
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
              case Operation::Trunc:
              case Operation::ZExt:
              case Operation::SExt:
              case Operation::UToF:
              case Operation::FPTrunc:
              case Operation::FPExt:
              case Operation::Bitcast:
              case Operation::FToU:
              case Operation::FToS:
              case Operation::PtrToI:
              case Operation::SToF: lineStr += "(" + inst.type->toString() + ")"; break;
              case Operation::IToPtr: lineStr += "(void *)"; break;
              case Operation::AddrSpaceCast: lineStr += "addrspacecast"; break;
              default: break;
            }
            switch(inst.op)
            {
              case Operation::Trunc: commentStr = "truncate ";
              case Operation::ZExt: commentStr += "zero extend "; break;
              case Operation::SExt: commentStr += "signed extend "; break;
              case Operation::UToF: commentStr += "unsigned "; break;
              case Operation::FPTrunc: commentStr += "fp truncate "; break;
              case Operation::FPExt: commentStr += "fp extend"; break;
              case Operation::Bitcast: commentStr += "bitcast "; break;
              case Operation::FToU: commentStr += "unsigned "; break;
              case Operation::FToS: commentStr += "signed "; break;
              case Operation::PtrToI: commentStr += "ptrtoi "; break;
              case Operation::IToPtr: commentStr += "itoptr "; break;
              default: break;
            }

            lineStr += "(";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ")";
            break;
          }
          case Operation::ExtractVal:
          {
            lineStr += "extractvalue ";
            lineStr += ArgToString(inst.args[0], false);
            for(size_t n = 1; n < inst.args.size(); n++)
              lineStr += StringFormat::Fmt(", %llu", cast<Literal>(inst.args[n])->literal);
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
            rdcstr opStr;
            switch(inst.op)
            {
              case Operation::FAdd: opStr = " + "; break;
              case Operation::FSub: opStr = " - "; break;
              case Operation::FMul: opStr = " * "; break;
              case Operation::FDiv: opStr = " / "; break;
              case Operation::FRem: opStr = " % "; break;
              case Operation::Add: opStr = " + "; break;
              case Operation::Sub: opStr = " - "; break;
              case Operation::Mul: opStr = " * "; break;
              case Operation::UDiv: opStr = " / "; break;
              case Operation::SDiv: opStr = " / "; break;
              case Operation::URem: opStr = " % "; break;
              case Operation::ShiftLeft: opStr = " << "; break;
              case Operation::LogicalShiftRight: opStr = " >> "; break;
              case Operation::ArithShiftRight: opStr = " >> "; break;
              case Operation::And: opStr = " & "; break;
              case Operation::Or: opStr = " | "; break;
              case Operation::Xor: opStr = " ^ "; break;
              default: break;
            }
            switch(inst.op)
            {
              case Operation::FRem: commentStr += "float "; break;
              case Operation::UDiv:
              case Operation::URem: commentStr += "unsigned "; break;
              case Operation::SDiv:
              case Operation::SRem: commentStr += "signed "; break;
              case Operation::LogicalShiftRight: commentStr += "logical "; break;
              case Operation::ArithShiftRight: commentStr += "arithmetic "; break;
              default: break;
            }

            bool first = true;
            for(const Value *s : inst.args)
            {
              lineStr += ArgToString(s, false);
              if(first)
              {
                lineStr += opStr;
                first = false;
              }
            }

            break;
          }
          case Operation::Ret:
          {
            lineStr += "return";
            if(!inst.args.empty())
              lineStr += " " + ArgToString(inst.args[0], false);
            break;
          }
          case Operation::Unreachable: lineStr += "unreachable"; break;
          case Operation::Alloca:
          {
            lineStr += "alloca ";
            lineStr += inst.type->inner->toString();
            if(inst.align > 0)
              lineStr += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::GetElementPtr:
          {
            lineStr += "getelementptr ";
            if(inst.opFlags() & InstructionFlags::InBounds)
              lineStr += "inbounds ";
            lineStr += inst.args[0]->type->inner->toString();
            lineStr += ", ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                lineStr += ", ";

              lineStr += ArgToString(s, false);
              first = false;
            }
            break;
          }
          case Operation::Load:
          {
            lineStr += "load ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              lineStr += "volatile ";
            lineStr += inst.type->toString();
            lineStr += ", ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                lineStr += ", ";

              lineStr += ArgToString(s, false);
              first = false;
            }
            if(inst.align > 0)
              lineStr += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::Store:
          {
            lineStr += "store ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              lineStr += "volatile ";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[0], false);
            if(inst.align > 0)
              lineStr += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
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
            rdcstr opStr;
            switch(inst.op)
            {
              case Operation::FOrdFalse: opStr = " false "; break;
              case Operation::FOrdTrue: opStr = " true "; break;
              case Operation::FOrdEqual: opStr = " = "; break;
              case Operation::FOrdGreater: opStr = " > "; break;
              case Operation::FOrdGreaterEqual: opStr = " >= "; break;
              case Operation::FOrdLess: opStr = " < "; break;
              case Operation::FOrdLessEqual: opStr = " <= "; break;
              case Operation::FOrdNotEqual: opStr = " != "; break;
              case Operation::FOrd: opStr = "ord "; break;
              case Operation::FUnord: opStr = "uno "; break;
              case Operation::FUnordEqual: opStr = " = ";
              case Operation::FUnordGreater: opStr = " > "; break;
              case Operation::FUnordGreaterEqual: opStr = " >= "; break;
              case Operation::FUnordLess: opStr = " < "; break;
              case Operation::FUnordLessEqual: opStr = " <= "; break;
              case Operation::FUnordNotEqual: opStr = " != "; break;
              default: break;
            }
            switch(inst.op)
            {
              case Operation::FUnord:
              case Operation::FUnordEqual:
              case Operation::FUnordGreater:
              case Operation::FUnordGreaterEqual:
              case Operation::FUnordLess:
              case Operation::FUnordLessEqual: commentStr += "unordered ";
              default: break;
            }
            lineStr += "(";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += opStr;
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ")";
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
            rdcstr opStr;
            switch(inst.op)
            {
              case Operation::IEqual: opStr += " = "; break;
              case Operation::INotEqual: opStr += " != "; break;
              case Operation::UGreater: opStr += " > "; break;
              case Operation::UGreaterEqual: opStr += " >= "; break;
              case Operation::ULess: opStr += " < "; break;
              case Operation::ULessEqual: opStr += " <= "; break;
              case Operation::SGreater: opStr += " > "; break;
              case Operation::SGreaterEqual: opStr += " >= "; break;
              case Operation::SLess: opStr += " < "; break;
              case Operation::SLessEqual: opStr += " <= "; break;
              default: break;
            }
            switch(inst.op)
            {
              case Operation::SGreater:
              case Operation::SGreaterEqual:
              case Operation::SLess:
              case Operation::SLessEqual: commentStr = "signed ";
              default: break;
            }
            lineStr += "(";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += opStr;
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ")";
            break;
          }
          case Operation::Select:
          {
            lineStr += "select ";
            lineStr += ArgToString(inst.args[2], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[1], false);
            break;
          }
          case Operation::ExtractElement:
          {
            lineStr += "extractelement ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[1], false);
            break;
          }
          case Operation::InsertElement:
          {
            lineStr += "insertelement ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[2], false);
            break;
          }
          case Operation::ShuffleVector:
          {
            lineStr += "shufflevector ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[2], false);
            break;
          }
          case Operation::InsertValue:
          {
            lineStr += "insertvalue ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[1], false);
            for(size_t a = 2; a < inst.args.size(); a++)
            {
              lineStr += ", " + ToStr(cast<Literal>(inst.args[a])->literal);
            }
            break;
          }
          case Operation::Branch:
          {
            lineStr += "br ";
            if(inst.args.size() > 1)
            {
              lineStr += ArgToString(inst.args[2], false);
              lineStr += StringFormat::Fmt(", %s", ArgToString(inst.args[0], false).c_str());
              lineStr += StringFormat::Fmt(", %s", ArgToString(inst.args[1], false).c_str());
            }
            else
            {
              lineStr += ArgToString(inst.args[0], false);
            }
            break;
          }
          case Operation::Phi:
          {
            lineStr += "phi ";
            lineStr += inst.type->toString();
            for(size_t a = 0; a < inst.args.size(); a += 2)
            {
              if(a == 0)
                lineStr += " ";
              else
                lineStr += ", ";
              lineStr += StringFormat::Fmt("[ %s, %s ]", ArgToString(inst.args[a], false).c_str(),
                                           ArgToString(inst.args[a + 1], false).c_str());
            }
            break;
          }
          case Operation::Switch:
          {
            lineStr += "switch ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += " [";
            lineStr += "\n";
            m_DisassemblyInstructionLine++;
            for(size_t a = 2; a < inst.args.size(); a += 2)
            {
              lineStr += StringFormat::Fmt("    %s, %s", ArgToString(inst.args[a], false).c_str(),
                                           ArgToString(inst.args[a + 1], false).c_str());
              lineStr += "\n";
              m_DisassemblyInstructionLine++;
            }
            lineStr += "  ]";
            break;
          }
          case Operation::Fence:
          {
            lineStr += "fence ";
            if(inst.opFlags() & InstructionFlags::SingleThread)
              lineStr += "singlethread ";
            switch((inst.opFlags() & InstructionFlags::SuccessOrderMask))
            {
              case InstructionFlags::SuccessUnordered: lineStr += "unordered"; break;
              case InstructionFlags::SuccessMonotonic: lineStr += "monotonic"; break;
              case InstructionFlags::SuccessAcquire: lineStr += "acquire"; break;
              case InstructionFlags::SuccessRelease: lineStr += "release"; break;
              case InstructionFlags::SuccessAcquireRelease: lineStr += "acq_rel"; break;
              case InstructionFlags::SuccessSequentiallyConsistent: lineStr += "seq_cst"; break;
              default: break;
            }
            break;
          }
          case Operation::LoadAtomic:
          {
            lineStr += "load atomic ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              lineStr += "volatile ";
            lineStr += inst.type->toString();
            lineStr += ", ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                lineStr += ", ";

              lineStr += ArgToString(s, false);
              first = false;
            }
            lineStr += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::StoreAtomic:
          {
            lineStr += "store atomic ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              lineStr += "volatile ";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ", ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::CompareExchange:
          {
            lineStr += "cmpxchg ";
            if(inst.opFlags() & InstructionFlags::Weak)
              lineStr += "weak ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              lineStr += "volatile ";

            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                lineStr += ", ";

              lineStr += ArgToString(s, false);
              first = false;
            }

            lineStr += " ";
            if(inst.opFlags() & InstructionFlags::SingleThread)
              lineStr += "singlethread ";
            switch((inst.opFlags() & InstructionFlags::SuccessOrderMask))
            {
              case InstructionFlags::SuccessUnordered: lineStr += "unordered"; break;
              case InstructionFlags::SuccessMonotonic: lineStr += "monotonic"; break;
              case InstructionFlags::SuccessAcquire: lineStr += "acquire"; break;
              case InstructionFlags::SuccessRelease: lineStr += "release"; break;
              case InstructionFlags::SuccessAcquireRelease: lineStr += "acq_rel"; break;
              case InstructionFlags::SuccessSequentiallyConsistent: lineStr += "seq_cst"; break;
              default: break;
            }
            lineStr += " ";
            switch((inst.opFlags() & InstructionFlags::FailureOrderMask))
            {
              case InstructionFlags::FailureUnordered: lineStr += "unordered"; break;
              case InstructionFlags::FailureMonotonic: lineStr += "monotonic"; break;
              case InstructionFlags::FailureAcquire: lineStr += "acquire"; break;
              case InstructionFlags::FailureRelease: lineStr += "release"; break;
              case InstructionFlags::FailureAcquireRelease: lineStr += "acq_rel"; break;
              case InstructionFlags::FailureSequentiallyConsistent: lineStr += "seq_cst"; break;
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
            lineStr += "atomicrmw ";
            if(inst.opFlags() & InstructionFlags::Volatile)
              lineStr += "volatile ";
            switch(inst.op)
            {
              case Operation::AtomicExchange: lineStr += "xchg "; break;
              case Operation::AtomicAdd: lineStr += "add "; break;
              case Operation::AtomicSub: lineStr += "sub "; break;
              case Operation::AtomicAnd: lineStr += "and "; break;
              case Operation::AtomicNand: lineStr += "nand "; break;
              case Operation::AtomicOr: lineStr += "or "; break;
              case Operation::AtomicXor: lineStr += "xor "; break;
              case Operation::AtomicMax: lineStr += "max "; break;
              case Operation::AtomicMin: lineStr += "min "; break;
              case Operation::AtomicUMax: lineStr += "umax "; break;
              case Operation::AtomicUMin: lineStr += "umin "; break;
              default: break;
            }

            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                lineStr += ", ";

              lineStr += ArgToString(s, false);
              first = false;
            }

            lineStr += " ";
            if(inst.opFlags() & InstructionFlags::SingleThread)
              lineStr += "singlethread ";
            switch((inst.opFlags() & InstructionFlags::SuccessOrderMask))
            {
              case InstructionFlags::SuccessUnordered: lineStr += "unordered"; break;
              case InstructionFlags::SuccessMonotonic: lineStr += "monotonic"; break;
              case InstructionFlags::SuccessAcquire: lineStr += "acquire"; break;
              case InstructionFlags::SuccessRelease: lineStr += "release"; break;
              case InstructionFlags::SuccessAcquireRelease: lineStr += "acq_rel"; break;
              case InstructionFlags::SuccessSequentiallyConsistent: lineStr += "seq_cst"; break;
              default: break;
            }
            break;
          }
        }

        if(showDxFuncName)
        {
          if(inst.getFuncCall() && inst.getFuncCall()->name.beginsWith("dx.op.annotateHandle"))
          {
            if(const Constant *props = cast<Constant>(inst.args[2]))
            {
              const Constant *packed[2];
              if(props && !props->isNULL() && props->getMembers().size() == 2 &&
                 (packed[0] = cast<Constant>(props->getMembers()[0])) != NULL &&
                 (packed[1] = cast<Constant>(props->getMembers()[1])) != NULL)
              {
                uint32_t packedProps[2] = {};
                packedProps[0] = packed[0]->getU32();
                packedProps[1] = packed[1]->getU32();

                bool uav = (packedProps[0] & (1 << 12)) != 0;
                bool rov = (packedProps[0] & (1 << 13)) != 0;
                bool globallyCoherent = (packedProps[0] & (1 << 14)) != 0;
                bool sampelCmpOrCounter = (packedProps[0] & (1 << 15)) != 0;
                ResourceKind resKind = (ResourceKind)(packedProps[0] & 0xFF);
                ResourceClass resClass;
                if(sampelCmpOrCounter && resKind == ResourceKind::Sampler)
                  resKind = ResourceKind::SamplerComparison;
                if(resKind == ResourceKind::Sampler || resKind == ResourceKind::SamplerComparison)
                  resClass = ResourceClass::Sampler;
                else if(resKind == ResourceKind::CBuffer)
                  resClass = ResourceClass::CBuffer;
                else if(uav)
                  resClass = ResourceClass::UAV;
                else
                  resClass = ResourceClass::SRV;

                lineStr += "  resource: ";

                bool srv = (resClass == ResourceClass::SRV);

                ComponentType compType = ComponentType(packedProps[1] & 0xFF);
                uint8_t compCount = (packedProps[1] & 0xFF00) >> 8;

                uint8_t feedbackType = packedProps[1] & 0xFF;

                uint32_t structStride = packedProps[1];

                switch(resKind)
                {
                  case ResourceKind::Unknown: lineStr += "Unknown"; break;
                  case ResourceKind::Texture1D:
                  case ResourceKind::Texture2D:
                  case ResourceKind::Texture2DMS:
                  case ResourceKind::Texture3D:
                  case ResourceKind::TextureCube:
                  case ResourceKind::Texture1DArray:
                  case ResourceKind::Texture2DArray:
                  case ResourceKind::Texture2DMSArray:
                  case ResourceKind::TextureCubeArray:
                  case ResourceKind::TypedBuffer:
                    if(globallyCoherent)
                      lineStr += "globallycoherent ";
                    if(!srv && rov)
                      lineStr += "ROV";
                    else if(!srv)
                      lineStr += "RW";
                    switch(resKind)
                    {
                      case ResourceKind::Texture1D: lineStr += "Texture1D"; break;
                      case ResourceKind::Texture2D: lineStr += "Texture2D"; break;
                      case ResourceKind::Texture2DMS: lineStr += "Texture2DMS"; break;
                      case ResourceKind::Texture3D: lineStr += "Texture3D"; break;
                      case ResourceKind::TextureCube: lineStr += "TextureCube"; break;
                      case ResourceKind::Texture1DArray: lineStr += "Texture1DArray"; break;
                      case ResourceKind::Texture2DArray: lineStr += "Texture2DArray"; break;
                      case ResourceKind::Texture2DMSArray: lineStr += "Texture2DMSArray"; break;
                      case ResourceKind::TextureCubeArray: lineStr += "TextureCubeArray"; break;
                      case ResourceKind::TypedBuffer: lineStr += "TypedBuffer"; break;
                      default: break;
                    }
                    break;
                  case ResourceKind::RTAccelerationStructure:
                    lineStr += "RTAccelerationStructure";
                    break;
                  case ResourceKind::FeedbackTexture2D: lineStr += "FeedbackTexture2D"; break;
                  case ResourceKind::FeedbackTexture2DArray:
                    lineStr += "FeedbackTexture2DArray";
                    break;
                  case ResourceKind::StructuredBuffer:
                    if(globallyCoherent)
                      lineStr += "globallycoherent ";
                    lineStr += srv ? "StructuredBuffer" : "RWStructuredBuffer";
                    lineStr += StringFormat::Fmt("<stride=%u", structStride);
                    if(sampelCmpOrCounter)
                      lineStr += ", counter";
                    lineStr += ">";
                    break;
                  case ResourceKind::StructuredBufferWithCounter:
                    if(globallyCoherent)
                      lineStr += "globallycoherent ";
                    lineStr += srv ? "StructuredBufferWithCounter" : "RWStructuredBufferWithCounter";
                    lineStr += StringFormat::Fmt("<stride=%u>", structStride);
                    break;
                  case ResourceKind::RawBuffer:
                    if(globallyCoherent)
                      lineStr += "globallycoherent ";
                    lineStr += srv ? "ByteAddressBuffer" : "RWByteAddressBuffer";
                    break;
                  case ResourceKind::CBuffer:
                    RDCASSERT(resClass == ResourceClass::CBuffer);
                    lineStr += "CBuffer";
                    break;
                  case ResourceKind::Sampler:
                    RDCASSERT(resClass == ResourceClass::Sampler);
                    lineStr += "SamplerState";
                    break;
                  case ResourceKind::TBuffer:
                    RDCASSERT(resClass == ResourceClass::SRV);
                    lineStr += "TBuffer";
                    break;
                  case ResourceKind::SamplerComparison:
                    RDCASSERT(resClass == ResourceClass::Sampler);
                    lineStr += "SamplerComparisonState";
                    break;
                }

                if(resKind == ResourceKind::FeedbackTexture2D ||
                   resKind == ResourceKind::FeedbackTexture2DArray)
                {
                  if(feedbackType == 0)
                    lineStr += "<MinMip>";
                  else if(feedbackType == 1)
                    lineStr += "<MipRegionUsed>";
                  else
                    lineStr += "<Invalid>";
                }
                else if(resKind == ResourceKind::Texture1D || resKind == ResourceKind::Texture2D ||
                        resKind == ResourceKind::Texture3D || resKind == ResourceKind::TextureCube ||
                        resKind == ResourceKind::Texture1DArray ||
                        resKind == ResourceKind::Texture2DArray ||
                        resKind == ResourceKind::TextureCubeArray ||
                        resKind == ResourceKind::TypedBuffer || resKind == ResourceKind::Texture2DMS ||
                        resKind == ResourceKind::Texture2DMSArray)
                {
                  lineStr += "<";
                  if(compCount > 1)
                    lineStr += StringFormat::Fmt("%dx", compCount);
                  lineStr += StringFormat::Fmt("%s>", ToStr(compType).c_str());
                }
              }
            }
          }
        }
        if(!lineStr.empty())
        {
          lineStr += ";";
          if(!commentStr.empty())
            lineStr += " // " + commentStr;

          m_Disassembly += "  " + lineStr;
          DisassemblyAddNewLine();
        }

        // if this is the last instruction don't print the next block's label
        if(funcIdx == func.instructions.size() - 1)
          break;

        if(inst.op == Operation::Branch || inst.op == Operation::Unreachable ||
           inst.op == Operation::Switch || inst.op == Operation::Ret)
        {
          DisassemblyAddNewLine();

          curBlock++;

          rdcstr labelName;

          if(func.blocks[curBlock]->name.empty())
            labelName = StringFormat::Fmt("; <label>:%u", func.blocks[curBlock]->slot);
          else
            labelName =
                StringFormat::Fmt("%s: ", escapeStringIfNeeded(func.blocks[curBlock]->name).c_str());

          labelName.reserve(50);
          while(labelName.size() < 50)
            labelName.push_back(' ');

          labelName += "; preds = ";
          bool first = true;
          for(const Block *pred : func.blocks[curBlock]->preds)
          {
            if(!first)
              labelName += ", ";
            first = false;
            if(pred->name.empty())
              labelName += StringFormat::Fmt("%c%u", DXIL::dxilIdentifier, pred->slot);
            else
              labelName += StringFormat::Fmt("%c%s", DXIL::dxilIdentifier,
                                             escapeStringIfNeeded(pred->name).c_str());
          }

          m_Disassembly += labelName;
          DisassemblyAddNewLine();
        }
      }
      m_Disassembly += "}";
      DisassemblyAddNewLine(2);
    }
    else
    {
      DisassemblyAddNewLine(2);
    }

    m_Accum.exitFunction();
  }

  DisassemblyAddNewLine();

  // TODO: decide how much of this should be output
  // m_Disassembly += DisassembleFuncAttrGroups();
  // m_Disassembly += DisassembleNamedMeta();
  // m_Disassembly += DisassembleMeta();

  m_Disassembly += "\n";
}

rdcstr Type::toString() const
{
  if(!name.empty())
  {
    return StringFormat::Fmt("%c%s", DXIL::dxilIdentifier, escapeStringIfNeeded(name).c_str());
  }

  switch(type)
  {
    case Scalar:
    {
      switch(scalarType)
      {
        case Void: return "void";
        case Int:
          if(DXIL::dxcStyleFormatting)
          {
            return StringFormat::Fmt("i%u", bitWidth);
          }
          else
          {
            switch(bitWidth)
            {
              case 1: return "bool";
              case 8: return "int8";
              case 16: return "short";
              case 32: return "int";
              case 64: return "long";
              default: return StringFormat::Fmt("i%u", bitWidth);
            }
          }
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
    case Vector:
    {
      if(DXIL::dxcStyleFormatting)
        return StringFormat::Fmt("<%u x %s>", elemCount, inner->toString().c_str());
      else
        return StringFormat::Fmt("%s%u", inner->toString().c_str(), elemCount);
    }
    case Pointer:
    {
      if(inner->type == Type::Function)
      {
        if(addrSpace == Type::PointerAddrSpace::Default)
          return inner->toString();
        else
          return StringFormat::Fmt("%s addrspace(%d)", inner->toString().c_str(), addrSpace);
      }
      if(addrSpace == Type::PointerAddrSpace::Default)
        return StringFormat::Fmt("%s*", inner->toString().c_str());
      else
        return StringFormat::Fmt("%s addrspace(%d)*", inner->toString().c_str(), addrSpace);
    }
    case Array:
    {
      if(DXIL::dxcStyleFormatting)
        return StringFormat::Fmt("[%u x %s]", elemCount, inner->toString().c_str());
      else
        return StringFormat::Fmt("%s[%u]", inner->toString().c_str(), elemCount);
    }
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

rdcstr Type::declFunction(rdcstr funcName, const rdcarray<Instruction *> &args,
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

    if(i < args.size() && !args[i]->getName().empty())
      ret += " %" + escapeStringIfNeeded(args[i]->getName());
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
    ret += StringFormat::Fmt(" dereferenceable(%llu)", derefBytes);
    p &= ~Attribute::Dereferenceable;
  }
  if(p & Attribute::DereferenceableOrNull)
  {
    ret += StringFormat::Fmt(" dereferenceable_or_null=%llu", derefOrNullBytes);
    p &= ~Attribute::DereferenceableOrNull;
  }

  if(p != Attribute::None)
  {
    ret = ToStr(p) + " " + ret.trimmed();
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
  if(slot == ~0U)
    return valString();
  return StringFormat::Fmt("!%u", slot);
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
      const Instruction *i = cast<Instruction>(value);

      if(i)
      {
        if(i->getName().empty())
          return StringFormat::Fmt("%s %c%u", i->type->toString().c_str(), DXIL::dxilIdentifier,
                                   i->slot);
        else
          return StringFormat::Fmt("%s %c%s", i->type->toString().c_str(), DXIL::dxilIdentifier,
                                   escapeStringIfNeeded(i->getName()).c_str());
      }
      else if(value)
      {
        return value->toString(true);
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
        ret += StringFormat::Fmt("!%u", children[i]->slot);
    }
    ret += "}";

    return ret;
  }
}

static void floatAppendToString(const Type *t, const ShaderValue &val, uint32_t i, rdcstr &ret)
{
  if(DXIL::dxcStyleFormatting)
  {
#if ENABLED(DXC_COMPATIBLE_DISASM)
    // dxc/llvm always prints half floats as their 16-bit hex representation.
    if(t->bitWidth == 16)
    {
      ret += StringFormat::Fmt("0xH%04X", val.u32v[i]);
      return;
    }
#endif
  }

  double d = t->bitWidth == 64 ? val.f64v[i] : val.f32v[i];

  // NaNs/infs are printed as hex to ensure we don't lose bits
  if(RDCISFINITE(d))
  {
    if(DXIL::dxcStyleFormatting)
    {
      // check we can reparse precisely a float-formatted string. Otherwise we print as hex
      rdcstr flt = StringFormat::Fmt("%.6le", d);

#if ENABLED(DXC_COMPATIBLE_DISASM)
      // dxc/llvm only prints floats as floats if they roundtrip, but our disassembly doesn't need
      // to roundtrip so it's better to display the value in all cases
      double reparse = strtod(flt.begin(), NULL);

      if(d == reparse)
      {
        ret += flt;
        return;
      }
#else
      ret += flt;
      return;
#endif
    }
    else
    {
      if(t->bitWidth == 64)
        ret += StringFormat::Fmt("%#g", val.f64v[i]);
      else
        ret += StringFormat::Fmt("%#g", val.f32v[i]);
      return;
    }
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
    if(type)
      ret += type->toString() + " ";
    else
      RDCERR("Type requested in value string, but no type available");
  }
  switch(kind())
  {
    case ValueKind::Function:
      ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(cast<Function>(this)->name).c_str());
      break;
    case ValueKind::GlobalVar:
      ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(cast<GlobalVar>(this)->name).c_str());
      break;
    case ValueKind::Alias:
      ret += StringFormat::Fmt("@%s", escapeStringIfNeeded(cast<Alias>(this)->name).c_str());
      break;
    case ValueKind::Constant: return cast<Constant>(this)->toString(withType); break;
    case ValueKind::ForwardReferencePlaceholder:
      RDCERR("forward-reference value being stringised");
      ret += "???";
      break;
    case ValueKind::Instruction:
    case ValueKind::Metadata:
    case ValueKind::Literal:
    case ValueKind::BasicBlock:
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
  if(isUndef())
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

        const Type *baseType = members->at(0)->type;
        RDCASSERT(baseType->type == Type::Pointer);
        ret += baseType->inner->toString();
        for(size_t i = 0; i < members->size(); i++)
        {
          ret += ", ";

          ret += members->at(i)->toString(withType);
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
        switch(op)
        {
          case Operation::FAdd: ret += "fadd "; break;
          case Operation::FSub: ret += "fsub "; break;
          case Operation::FMul: ret += "fmul "; break;
          case Operation::FDiv: ret += "fdiv "; break;
          case Operation::FRem: ret += "frem "; break;
          case Operation::Add: ret += "add "; break;
          case Operation::Sub: ret += "sub "; break;
          case Operation::Mul: ret += "mul "; break;
          case Operation::UDiv: ret += "udiv "; break;
          case Operation::SDiv: ret += "sdiv "; break;
          case Operation::URem: ret += "urem "; break;
          case Operation::SRem: ret += "srem "; break;
          case Operation::ShiftLeft: ret += "shl "; break;
          case Operation::LogicalShiftRight: ret += "lshr "; break;
          case Operation::ArithShiftRight: ret += "ashr "; break;
          case Operation::And: ret += "and "; break;
          case Operation::Or: ret += "or "; break;
          case Operation::Xor: ret += "xor "; break;
          default: break;
        }

        ret += "(";
        for(size_t i = 0; i < members->size(); i++)
        {
          if(i > 0)
            ret += ", ";

          if(Literal *l = cast<Literal>(members->at(i)))
          {
            ShaderValue v;
            v.u64v[0] = l->literal;

            shaderValAppendToString(members->at(i)->type, v, 0, ret);
          }
          else
          {
            ret += members->at(i)->toString(withType);
          }
        }

        ret += ")";

        break;
      }
    }
  }
  else if(type->type == Type::Scalar)
  {
    ShaderValue v;
    v.u64v[0] = u64;
    shaderValAppendToString(type, v, 0, ret);
  }
  else if(isNULL())
  {
    if(type->type == Type::Pointer)
      ret += "null";
    else
      ret += "zeroinitializer";
  }
  else if(type->type == Type::Vector)
  {
    ret += "<";
    ShaderValue v;

    // data vectors may only have a value, use it directly
    if(isShaderVal())
    {
      v = *val;
    }
    // all other vectors use just a members array, so we extract the values here
    else if(isCompound())
    {
      for(uint32_t i = 0; i < type->elemCount; i++)
      {
        if(type->bitWidth <= 32)
          v.u32v[i] = cast<Constant>(members->at(i))->getU32();
        else
          v.u64v[i] = cast<Constant>(members->at(i))->getU64();
      }
    }

    for(uint32_t i = 0; i < type->elemCount; i++)
    {
      if(i > 0)
        ret += ", ";
      if(withType)
        ret += type->inner->toString() + " ";
      if(isCompound() && cast<Constant>(members->at(i))->isUndef())
        ret += "undef";
      else
        shaderValAppendToString(type, v, i, ret);
    }
    ret += ">";
  }
  else if(type->type == Type::Array)
  {
    if(!members && !str.empty())
    {
      ret += "c" + escapeString(str);
    }
    else
    {
      ret += "[";
      for(size_t i = 0; i < members->size(); i++)
      {
        if(i > 0)
          ret += ", ";

        if(Literal *l = cast<Literal>(members->at(i)))
        {
          if(withType)
            ret += type->inner->toString() + " ";

          ShaderValue v;
          v.u64v[0] = l->literal;

          shaderValAppendToString(type->inner, v, 0, ret);
        }
        else
        {
          ret += members->at(i)->toString(withType);
        }
      }
      ret += "]";
    }
  }
  else if(type->type == Type::Struct)
  {
    bool allNULL = true, allUndef = true;
    for(size_t i = 0; i < members->size(); i++)
    {
      if(Constant *c = cast<Constant>(members->at(i)))
      {
        if(!c->isNULL())
          allNULL = false;
        if(!c->isUndef())
          allUndef = false;
      }
      else
      {
        allNULL = allUndef = false;
      }
    }

    if(allUndef)
    {
      ret += "undef";
    }
    else if(allNULL)
    {
      ret += "zeroinitializer";
    }
    else
    {
      ret += "{ ";
      for(size_t i = 0; i < members->size(); i++)
      {
        if(i > 0)
          ret += ", ";

        if(Literal *l = cast<Literal>(members->at(i)))
        {
          ShaderValue v;
          v.u64v[0] = l->literal;

          shaderValAppendToString(members->at(i)->type, v, 0, ret);
        }
        else
        {
          ret += members->at(i)->toString(withType);
        }
      }
      ret += " }";
    }
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

    // these bits are ordered not in declaration order (which matches how they're serialised) but in
    // the (mostly but not quite) alphabetical order since that's how LLVM prints them
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Alignment, "alignment");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(AlwaysInline, "alwaysinline");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Builtin, "builtin");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ByVal, "byval");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InAlloca, "inalloca");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Cold, "cold");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Convergent, "convergent");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InlineHint, "inlinehint");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(InReg, "inreg");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(JumpTable, "jumptable");
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
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NonNull, "nonnull");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Dereferenceable, "dereferenceable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(DereferenceableOrNull, "dereferenceable_or_null");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoRedZone, "noredzone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoReturn, "noreturn");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(NoUnwind, "nounwind");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(OptimizeForSize, "optsize");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(OptimizeNone, "optnone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReadNone, "readnone");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReadOnly, "readonly");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ArgMemOnly, "argmemonly");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(Returned, "returned");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ReturnsTwice, "returns_twice");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SExt, "signext");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackAlignment, "alignstack");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtect, "ssp");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtectReq, "sspreq");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StackProtectStrong, "sspstrong");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SafeStack, "safestack");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(StructRet, "sret");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeAddress, "sanitize_address");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeThread, "sanitize_thread");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(SanitizeMemory, "sanitize_memory");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(UWTable, "uwtable");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ZExt, "zeroext");
  }
  END_BITFIELD_STRINGISE();
}
