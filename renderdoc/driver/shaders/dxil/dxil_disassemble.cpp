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

bool isUndef(const Value *v)
{
  if(const Constant *c = cast<Constant>(v))
    return c->isUndef();
  return false;
}

template <typename T>
bool getival(const Value *v, T &out)
{
  if(const Constant *c = cast<Constant>(v))
  {
    out = T(c->getU64());
    return true;
  }
  else if(const Literal *lit = cast<Literal>(v))
  {
    out = T(c->getU64());
    return true;
  }
  out = T();
  return false;
}

static const char *shaderNames[] = {
    "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
    "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
    "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
};

// clang-format off
static const char *funcNameSigs[] = {
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
"AnnotateHandle(res,props)",
"CreateHandleFromBinding(bind,index,nonUniformIndex)",
"CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)",
"Unpack4x8(unpackMode,pk)",
"Pack4x8(packMode,x,y,z,w)",
"IsHelperLane()",
"QuadVote(cond,op)",
"TextureGatherRaw(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1)",
"SampleCmpLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,lod)",
"TextureStoreSample(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask,sampleIdx)",
"WaveMatrix_Annotate(waveMatrixPtr,waveMatProps)",
"WaveMatrix_Depth(waveMatProps)",
"WaveMatrix_Fill(waveMatrixPtr,value)",
"WaveMatrix_LoadRawBuf(waveMatrixPtr,rawBuf,offsetInBytes,strideInBytes,alignmentInBytes,colMajor)",
"WaveMatrix_LoadGroupShared(waveMatrixPtr,groupsharedPtr,startArrayIndex,strideInElements,colMajor)",
"WaveMatrix_StoreRawBuf(waveMatrixPtr,rawBuf,offsetInBytes,strideInBytes,alignmentInBytes,colMajor)",
"WaveMatrix_StoreGroupShared(waveMatrixPtr,groupsharedPtr,startArrayIndex,strideInElements,colMajor)",
"WaveMatrix_Multiply(waveMatrixAccumulator,waveMatrixLeft,waveMatrixRight)",
"WaveMatrix_MultiplyAccumulate(waveMatrixAccumulator,waveMatrixLeft,waveMatrixRight)",
"WaveMatrix_ScalarOp(waveMatrixPtr,op,value)",
"WaveMatrix_SumAccumulate(waveMatrixFragment,waveMatrixInput)",
"WaveMatrix_Add(waveMatrixAccumulator,waveMatrixAccumulatorOrFragment)",
"AllocateNodeOutputRecords(output,numRecords,perThread)",
"GetNodeRecordPtr(recordhandle,arrayIndex)",
"IncrementOutputCount(output,count,perThread)",
"OutputComplete(output)",
"GetInputRecordCount(input)",
"FinishedCrossGroupSharing(input)",
"BarrierByMemoryType(MemoryTypeFlags,SemanticFlags)",
"BarrierByMemoryHandle(object,SemanticFlags)",
"BarrierByNodeRecordHandle(object,SemanticFlags)",
"CreateNodeOutputHandle(MetadataIdx)",
"IndexNodeHandle(NodeOutputHandle,ArrayIndex)",
"AnnotateNodeHandle(node,props)",
"CreateNodeInputRecordHandle(MetadataIdx)",
"AnnotateNodeRecordHandle(noderecord,props)",
"NodeOutputIsValid(output)",
"GetRemainingRecursionLevels()",
"SampleCmpGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)",
"SampleCmpBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,bias,clamp)",
"StartVertexLocation()",
"StartInstanceLocation()",
};
// clang-format on

static rdcstr GetResourceShapeName(DXIL::ResourceKind shape, bool uav)
{
  rdcstr prefix = uav ? "RW" : "";
  switch(shape)
  {
    case DXIL::ResourceKind::Unknown: return "Unknown";
    case DXIL::ResourceKind::Texture1D: return prefix + "Texture1D";
    case DXIL::ResourceKind::Texture2D: return prefix + "Texture2D";
    case DXIL::ResourceKind::Texture2DMS: return prefix + "Texture2DMS";
    case DXIL::ResourceKind::Texture3D: return prefix + "Texture3D";
    case DXIL::ResourceKind::TextureCube: return prefix + "TextureCube";
    case DXIL::ResourceKind::Texture1DArray: return prefix + "Texture1DArray";
    case DXIL::ResourceKind::Texture2DArray: return prefix + "Texture2DArray";
    case DXIL::ResourceKind::Texture2DMSArray: return prefix + "Texture2DMSArray";
    case DXIL::ResourceKind::TextureCubeArray: return prefix + "TextureCubeArray";
    case DXIL::ResourceKind::TypedBuffer: return prefix + "TypedBuffer";
    case DXIL::ResourceKind::RawBuffer: return prefix + "ByteAddressBuffer";
    case DXIL::ResourceKind::StructuredBuffer: return prefix + "StructuredBuffer";
    case DXIL::ResourceKind::CBuffer: return "CBuffer";
    case DXIL::ResourceKind::Sampler: return "Sampler";
    case DXIL::ResourceKind::TBuffer: return "TBuffer";
    case DXIL::ResourceKind::RTAccelerationStructure: return "RTAccelerationStructure";
    case DXIL::ResourceKind::FeedbackTexture2D: return prefix + "FeedbackTexture2D";
    case DXIL::ResourceKind::FeedbackTexture2DArray: return prefix + "FeedbackTexture2DArray";
    case DXIL::ResourceKind::StructuredBufferWithCounter:
      return prefix + "StructuredBufferWithCounter";
    case DXIL::ResourceKind::SamplerComparison: return "SamplerComparison";
  };
  return "INVALID RESOURCE KIND";
};

static rdcstr GetResourceTypeName(const Type *type)
{
  // variable should be a pointer to the underlying type
  RDCASSERT(type->type == Type::Pointer);
  const Type *resType = type->inner;

  // textures are a struct containing the inner type and a mips type
  if(resType->type == Type::Struct && !resType->members.empty())
    resType = resType->members[0];

  rdcstr arrayDim = "";
  // find the inner type of any arrays
  while(resType->type == Type::Array)
  {
    arrayDim += "[" + ToStr(resType->elemCount) + "]";
    resType = resType->inner;
  }

  uint32_t compCount = 1;
  // get the inner type for a vector
  if(resType->type == Type::Vector)
  {
    compCount = resType->elemCount;
    resType = resType->inner;
  }

  if(resType->type == Type::Scalar)
  {
    rdcstr compType = "int";
    if(resType->scalarType == Type::Float)
      compType = resType->bitWidth > 32 ? "double" : "float";

    if(compCount > 1)
      compType += ToStr(compCount);
    return compType + arrayDim;
  }
  else if(resType->type == Type::Struct)
  {
    rdcstr compType = resType->name;
    int start = compType.find('.');
    if(start > 0)
      compType = compType.substr(start + 1);
    return compType + arrayDim;
  }
  return "UNHANDLED RESOURCE TYPE";
};

rdcstr GetCBufferVariableTypeName(const DXBC::CBufferVariableType &type)
{
  rdcstr ret;
  return ret;
}

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

const rdcstr &Program::GetDisassembly(bool dxcStyle, const DXBC::Reflection *reflection)
{
  if(m_Disassembly.empty() || (dxcStyle != m_DXCStyle))
  {
    m_DXCStyle = dxcStyle;
    SettleIDs();

    if(dxcStyle)
      MakeDXCDisassemblyString();
    else
      MakeRDDisassemblyString(reflection);
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
            if(opcode < ARRAY_COUNT(funcNameSigs))
            {
              m_Disassembly += "  ; ";
              m_Disassembly += funcNameSigs[opcode];
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
          // unfortunately due to how llvm/dxc packs its preds, this is not feasible to replicate
          // so instead we omit the pred list entirely and dxc's output needs to be regex replaced
          // to match
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

static const DXBC::CBufferVariable *FindCBufferVar(const uint32_t minOffset, const uint32_t maxOffset,
                                                   const rdcarray<DXBC::CBufferVariable> &variables,
                                                   uint32_t &byteOffset, rdcstr &prefix)
{
  for(const DXBC::CBufferVariable &v : variables)
  {
    // absolute byte offset of this variable in the cbuffer
    const uint32_t voffs = byteOffset + v.offset;

    // does minOffset-maxOffset reside in this variable? We don't handle the case where the range
    // crosses a variable (and I don't think FXC emits that anyway).
    if(voffs <= minOffset && voffs + v.type.bytesize > maxOffset)
    {
      byteOffset = voffs;

      // if it is a struct with members, recurse to find a closer match
      if(!v.type.members.empty())
      {
        prefix += v.name + ".";
        return FindCBufferVar(minOffset, maxOffset, v.type.members, byteOffset, prefix);
      }

      // otherwise return this variable.
      return &v;
    }
  }

  return NULL;
}

static rdcstr MakeCBufferRegisterStr(uint32_t reg, DXIL::EntryPointInterface::CBuffer cbuffer)
{
  rdcstr ret = "{";
  uint32_t offset = 0;
  uint32_t regOffset = reg * 16;
  while(offset < 16)
  {
    if(offset > 0)
      ret += ", ";

    uint32_t baseOffset = 0;
    uint32_t minOffset = regOffset + offset;
    uint32_t maxOffset = minOffset + 1;
    rdcstr prefix;
    const DXBC::CBufferVariable *var =
        FindCBufferVar(minOffset, maxOffset, cbuffer.cbufferRefl->variables, baseOffset, prefix);
    if(var)
    {
      ret += cbuffer.name + "." + prefix + var->name;
      uint32_t varOffset = regOffset - baseOffset;

      // if it's an array, add the index based on the relative index to the base offset
      if(var->type.elements > 1)
      {
        uint32_t byteSize = var->type.bytesize;

        // round up the byte size to a the nearest vec4 in case it's not quite a multiple
        byteSize = AlignUp16(byteSize);

        const uint32_t elementSize = byteSize / var->type.elements;
        const uint32_t elementIndex = varOffset / elementSize;

        ret += StringFormat::Fmt("[%u]", elementIndex);

        // subtract off so that if there's any further offset, it can be processed
        varOffset -= elementIndex;
      }

      // or if it's a matrix
      if((var->type.varClass == DXBC::CLASS_MATRIX_ROWS && var->type.cols > 1) ||
         (var->type.varClass == DXBC::CLASS_MATRIX_COLUMNS && var->type.rows > 1))
      {
        ret += StringFormat::Fmt("[%u]", varOffset / 16);
      }

      offset = var->offset + var->type.bytesize;
      offset -= regOffset;
    }
    else
    {
      ret += "<padding>";
      offset += 4;
    }
  }
  ret += "}";
  return ret;
}

struct ResourceHandle
{
  rdcstr name;
  ResourceClass resourceClass;
  uint32_t resourceIndex = 0;
  union
  {
    const DXIL::EntryPointInterface::UAV *uav;
    const DXIL::EntryPointInterface::SRV *srv;
    const DXIL::EntryPointInterface::CBuffer *cbuffer;
    const DXIL::EntryPointInterface::Sampler *sampler;
  };
};

void Program::MakeRDDisassemblyString(const DXBC::Reflection *reflection)
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

  // Decode entry points
  rdcarray<EntryPointInterface> entryPoints;
  FetchEntryPointInterfaces(entryPoints);
  for(size_t e = 0; e < entryPoints.size(); ++e)
  {
    EntryPointInterface &entryPoint = entryPoints[e];
    m_Disassembly += entryPoint.name + "()";
    DisassemblyAddNewLine();
    m_Disassembly += "{";
    DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.inputs.size(); ++i)
    {
      EntryPointInterface::Signature &sig = entryPoint.inputs[i];
      VarType varType = VarTypeForComponentType(sig.type);
      m_Disassembly += "  Input[" + ToStr(i) + "] " + ToStr(varType).c_str();

      if(sig.cols > 1)
        m_Disassembly += ToStr(sig.cols);

      if(reflection && sig.rows == 1)
      {
        const SigParameter &sigParam = reflection->InputSig[i];
        if(sigParam.semanticName == sig.name)
        {
          sig.name = sigParam.semanticIdxName;
        }
      }
      m_Disassembly += " " + sig.name;
      if(sig.rows > 1)
        m_Disassembly += "[" + ToStr(sig.rows) + "]";
      m_Disassembly += ";";
      DisassemblyAddNewLine();
    }
    if(!entryPoint.outputs.empty())
      DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.outputs.size(); ++i)
    {
      EntryPointInterface::Signature &sig = entryPoint.outputs[i];
      VarType varType = VarTypeForComponentType(sig.type);
      m_Disassembly += "  Output[" + ToStr(i) + "] " + ToStr(varType).c_str();

      if(sig.cols > 1)
        m_Disassembly += ToStr(sig.cols);

      if(reflection && sig.rows == 1)
      {
        const SigParameter &sigParam = reflection->OutputSig[i];
        if(sigParam.semanticName == sig.name)
          sig.name = sigParam.semanticIdxName;
      }
      m_Disassembly += " " + sig.name;
      if(sig.rows > 1)
        m_Disassembly += "[" + ToStr(sig.rows) + "]";
      m_Disassembly += ";";
      DisassemblyAddNewLine();
    }

    if(!entryPoint.srvs.empty())
      DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.srvs.size(); ++i)
    {
      EntryPointInterface::SRV &srv = entryPoint.srvs[i];
      if(srv.name.empty() && reflection)
      {
        for(DXBC::ShaderInputBind bind : reflection->SRVs)
        {
          if((bind.space == srv.space) && (bind.reg == srv.regBase) &&
             (bind.bindCount == srv.regCount))
            srv.name = bind.name;
        }
      }
      // SRV[0] StructureBuffer<int> fastCopySource Space : 0 Reg : 0 Count : 1;
      m_Disassembly += "  SRV[" + ToStr(i) + "]";
      m_Disassembly += " " + GetResourceShapeName(srv.shape, false);
      m_Disassembly += "<" + GetResourceTypeName(srv.type) + ">";
      m_Disassembly += " " + srv.name;
      m_Disassembly += " Space: " + ToStr(srv.space);
      m_Disassembly += " Reg: " + ToStr(srv.regBase);
      m_Disassembly += " Count: " + ToStr(srv.regCount);
      m_Disassembly += ";";
      DisassemblyAddNewLine();
    }

    if(!entryPoint.uavs.empty())
      DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.uavs.size(); ++i)
    {
      EntryPointInterface::UAV &uav = entryPoint.uavs[i];
      if(uav.name.empty() && reflection)
      {
        for(DXBC::ShaderInputBind bind : reflection->UAVs)
        {
          if((bind.space == uav.space) && (bind.reg == uav.regBase) &&
             (bind.bindCount == uav.regCount))
            uav.name = bind.name;
        }
      }
      m_Disassembly += "  UAV[" + ToStr(i) + "]";
      m_Disassembly += " " + GetResourceShapeName(uav.shape, true);
      m_Disassembly += "<" + GetResourceTypeName(uav.type) + ">";
      m_Disassembly += " " + uav.name;
      m_Disassembly += " Space: " + ToStr(uav.space);
      m_Disassembly += " Reg: " + ToStr(uav.regBase);
      m_Disassembly += " Count: " + ToStr(uav.regCount);
      m_Disassembly += ";";
      DisassemblyAddNewLine();
    }

    if(!entryPoint.cbuffers.empty())
      DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.cbuffers.size(); ++i)
    {
      EntryPointInterface::CBuffer &cbuffer = entryPoint.cbuffers[i];
      if(reflection)
      {
        for(size_t cbIdx = 0; cbIdx < reflection->CBuffers.size(); ++cbIdx)
        {
          const DXBC::CBuffer &cb = reflection->CBuffers[cbIdx];
          if((cb.space == cbuffer.space) && (cb.reg == cbuffer.regBase) &&
             (cb.bindCount == cbuffer.regCount))
          {
            if(cbuffer.name.empty())
              cbuffer.name = cb.name;
            if(!cbuffer.cbufferRefl)
              cbuffer.cbufferRefl = &reflection->CBuffers[cbIdx];
          }
        }
      }
      m_Disassembly += "  CBuffer[" + ToStr(i) + "] ";
      m_Disassembly += cbuffer.name;
      m_Disassembly += " Space: " + ToStr(cbuffer.space);
      m_Disassembly += " Reg: " + ToStr(cbuffer.regBase);
      m_Disassembly += " Count: " + ToStr(cbuffer.regCount);
      if(cbuffer.cbufferRefl)
      {
        if(!cbuffer.cbufferRefl->variables.empty())
        {
          DisassemblyAddNewLine();
          m_Disassembly += "  {";
          DisassemblyAddNewLine();

          for(const DXBC::CBufferVariable &cbVar : cbuffer.cbufferRefl->variables)
          {
            const DXBC::CBufferVariableType &cbType = cbVar.type;
            m_Disassembly += "    ";
            m_Disassembly += cbType.name;
            m_Disassembly += " " + cbVar.name;
            if(cbType.elements > 1)
              m_Disassembly += "[" + ToStr(cbType.elements) + "]";
            m_Disassembly += ";";
            DisassemblyAddNewLine();
          }
          m_Disassembly += "  };";
          DisassemblyAddNewLine();
        }
      }
      DisassemblyAddNewLine();
    }

    if(!entryPoint.samplers.empty())
      DisassemblyAddNewLine();

    for(size_t i = 0; i < entryPoint.samplers.size(); ++i)
    {
      EntryPointInterface::Sampler &sampler = entryPoint.samplers[i];
      if(sampler.name.empty() && reflection)
      {
        for(DXBC::ShaderInputBind s : reflection->Samplers)
        {
          if((s.space == sampler.space) && (s.reg == sampler.regBase) &&
             (s.bindCount == sampler.regCount))
            sampler.name = s.name;
        }
      }
      m_Disassembly += "  Sampler[" + ToStr(i) + "]";
      m_Disassembly += " " + GetResourceTypeName(sampler.type);
      m_Disassembly += " " + sampler.name;
      m_Disassembly += " Space: " + ToStr(sampler.space);
      m_Disassembly += " Reg: " + ToStr(sampler.regBase);
      m_Disassembly += " Count: " + ToStr(sampler.regCount);
      m_Disassembly += ";";
      DisassemblyAddNewLine();
    }
    m_Disassembly += "}";
    DisassemblyAddNewLine();
  }

  const char *swizzle = "xyzw";

  DisassemblyAddNewLine();

  EntryPointInterface &entryPoint = entryPoints[0];
  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = *m_Functions[i];

    m_Accum.processFunction(m_Functions[i]);

    if(func.external)
      continue;

    for(EntryPointInterface &ep : entryPoints)
    {
      if(func.name == ep.name)
      {
        entryPoint = ep;
        break;
      }
    }
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

      std::map<rdcstr, ResourceHandle> resHandles;
      std::map<rdcstr, rdcstr> ssaAliases;

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
        rdcstr resultIdStr;
        if(!inst.getName().empty())
          resultIdStr = StringFormat::Fmt("%c%s", DXIL::dxilIdentifier,
                                          escapeStringIfNeeded(inst.getName()).c_str());
        else if(inst.slot != ~0U)
          resultIdStr = StringFormat::Fmt("%c%s", DXIL::dxilIdentifier, ToStr(inst.slot).c_str());

        if(!resultIdStr.empty())
          lineStr += resultIdStr + " = ";

        bool showDxFuncName = false;
        rdcstr commentStr;

        switch(inst.op)
        {
          case Operation::NoOp: lineStr += "??? "; break;
          case Operation::Call:
          {
            rdcstr funcCallName = inst.getFuncCall()->name;
            showDxFuncName = funcCallName.beginsWith("dx.op");
            if(showDxFuncName && funcCallName.beginsWith("dx.op.loadInput"))
            {
              // LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 4);
              rdcstr name;
              rdcstr rowStr;
              rdcstr componentStr;
              uint32_t inputIdx;
              uint32_t rowIdx;
              bool hasRowIdx = getival<uint32_t>(inst.args[2], rowIdx);
              if(getival<uint32_t>(inst.args[1], inputIdx))
              {
                EntryPointInterface::Signature &sig = entryPoint.inputs[inputIdx];
                name = sig.name;
                if(hasRowIdx)
                {
                  if(sig.rows > 1)
                    rowStr = "[" + ToStr(rowIdx) + "]";
                }
              }
              else
              {
                name = ArgToString(inst.args[1], false);
                rowStr = "[";
                if(hasRowIdx)
                  rowStr += ToStr(rowIdx);
                else
                  rowStr += ArgToString(inst.args[2], false);
                rowStr += +"]";
              }
              uint32_t componentIdx;
              if(getival<uint32_t>(inst.args[3], componentIdx))
                componentStr = StringFormat::Fmt("%c", swizzle[componentIdx & 0x3]);
              else
                componentStr = ArgToString(inst.args[3], false);

              lineStr += "<IN>." + name + rowStr + "." + componentStr;
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.storeOutput"))
            {
              // StoreOutput(outputSigId,rowIndex,colIndex,value)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 5);
              rdcstr name;
              rdcstr rowStr;
              rdcstr componentStr;
              uint32_t outputIdx;
              uint32_t rowIdx;
              bool hasRowIdx = getival<uint32_t>(inst.args[2], rowIdx);
              if(getival<uint32_t>(inst.args[1], outputIdx))
              {
                EntryPointInterface::Signature &sig = entryPoint.outputs[outputIdx];
                name = sig.name;
                if(hasRowIdx)
                {
                  if(sig.rows > 1)
                    rowStr = "[" + ToStr(rowIdx) + "]";
                }
              }
              else
              {
                name = ArgToString(inst.args[1], false);
                rowStr = "[";
                if(hasRowIdx)
                  rowStr += ToStr(rowIdx);
                else
                  rowStr += ArgToString(inst.args[2], false);
                rowStr += +"]";
              }
              uint32_t componentIdx;
              if(getival<uint32_t>(inst.args[3], componentIdx))
                componentStr = StringFormat::Fmt("%c", swizzle[componentIdx & 0x3]);
              else
                componentStr = ArgToString(inst.args[3], false);

              lineStr += "<OUT>." + name + rowStr + "." + componentStr;
              lineStr += " = " + ArgToString(inst.args[4], false);
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.createHandle"))
            {
              // CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 57);
              rdcstr handleStr = resultIdStr;
              ResourceClass resClass;
              rdcstr resName;
              uint32_t resIndex;
              bool hasResIndex = getival<uint32_t>(inst.args[2], resIndex);
              if(getival<ResourceClass>(inst.args[1], resClass))
              {
                if(hasResIndex)
                {
                  ResourceHandle resHandle;
                  bool validRes = true;
                  switch(resClass)
                  {
                    case ResourceClass::SRV:
                      resName = entryPoint.srvs[resIndex].name;
                      resHandle.srv = &entryPoint.srvs[resIndex];
                      break;
                    case ResourceClass::UAV:
                      resName = entryPoint.uavs[resIndex].name;
                      resHandle.uav = &entryPoint.uavs[resIndex];
                      break;
                    case ResourceClass::CBuffer:
                      resName = entryPoint.cbuffers[resIndex].name;
                      resHandle.cbuffer = &entryPoint.cbuffers[resIndex];
                      break;
                    case ResourceClass::Sampler:
                      resName = entryPoint.samplers[resIndex].name;
                      resHandle.sampler = &entryPoint.samplers[resIndex];
                      break;
                    default:
                      validRes = false;
                      resName = "INVALID RESOURCE CLASS";
                      break;
                  };

                  if(validRes)
                  {
                    resHandle.name = resName;
                    resHandle.resourceClass = resClass;
                    resHandle.resourceIndex = resIndex;
                    resHandles[handleStr] = resHandle;
                    ssaAliases[handleStr] = resName;
                  }
                  uint32_t index;
                  if(getival<uint32_t>(inst.args[3], index))
                  {
                    if(index != resIndex)
                      commentStr += " index = " + ToStr(index);
                  }
                }
                else
                {
                  switch(resClass)
                  {
                    case ResourceClass::SRV: resName = "SRV"; break;
                    case ResourceClass::UAV: resName = "UAV"; break;
                    case ResourceClass::CBuffer: resName = "CBuffer"; break;
                    case ResourceClass::Sampler: resName = "Sampler"; break;
                    default: resName = "INVALID RESOURCE CLASS"; break;
                  };
                }
              }
              else
              {
                resName = "ResourceClass:" + ArgToString(inst.args[1], false);
              }
              if(!hasResIndex)
              {
                resName += "[" + ArgToString(inst.args[2], false) + "]";
                commentStr += " index = " + ArgToString(inst.args[3], false);
              }
              uint32_t value;
              if(getival<uint32_t>(inst.args[4], value))
              {
                if(value != 0)
                  commentStr += " nonUniformIndex = true";
              }
              lineStr += resName;
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.cbufferLoad"))
            {
              // CBufferLoad(handle,byteOffset,alignment)
              // CBufferLoadLegacy(handle,regIndex)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 59);
              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                uint32_t regIndex;
                if(getival<uint32_t>(inst.args[2], regIndex))
                {
                  if(!funcCallName.beginsWith("dx.op.cbufferLoadLegacy"))
                  {
                    // TODO: handle non 16-byte aligned offsets
                    // Convert byte offset to a register index
                    regIndex = regIndex / 16;
                    // uint32_t alignment = getival<uint32_t>(inst.args[3]);
                  }
                  uint32_t resourceIndex = resHandles[handleStr].resourceIndex;
                  lineStr += MakeCBufferRegisterStr(regIndex, entryPoint.cbuffers[resourceIndex]);
                }
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.bufferLoad"))
            {
              // BufferLoad(srv,index,wot)
              // wot is unused
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 68);
              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                lineStr += resHandles[handleStr].name;
                lineStr += "[" + ArgToString(inst.args[2], false) + "]";
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.rawBufferLoad"))
            {
              // RawBufferLoad(srv,index,elementOffset,mask,alignment)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 139);
              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                lineStr += resHandles[handleStr].name;
                if(!isUndef(inst.args[2]))
                {
                  lineStr += "[" + ArgToString(inst.args[2], false) + "]";
                  if(!isUndef(inst.args[3]))
                  {
                    uint32_t elementOffset;
                    if(getival<uint32_t>(inst.args[3], elementOffset))
                    {
                      if(elementOffset > 0)
                        lineStr += " + " + ToStr(elementOffset) + " bytes";
                    }
                    else
                    {
                      lineStr += " + " + ArgToString(inst.args[3], false) + " bytes";
                    }
                  }
                }
                else
                {
                  lineStr += "[" + ArgToString(inst.args[3], false) + "]";
                }
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.bufferStore") ||
                    funcCallName.beginsWith("dx.op.rawBufferStore"))
            {
              if(funcCallName.beginsWith("dx.op.bufferStore"))
              {
                // BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
                showDxFuncName = false;
                uint32_t dxopCode;
                RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
                RDCASSERTEQUAL(dxopCode, 69);
              }
              else
              {
                // RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
                showDxFuncName = false;
                uint32_t dxopCode;
                RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
                RDCASSERTEQUAL(dxopCode, 140);
              }

              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                uint32_t offset = 0;
                bool validElementOffset = !isUndef(inst.args[3]);
                bool constantElementOffset = validElementOffset && getival(inst.args[3], offset);

                lineStr += resHandles[handleStr].name;
                uint32_t index;
                if(getival(inst.args[2], index))
                {
                  if((offset == 0) || (index > 0))
                    lineStr += "[" + ToStr(index) + "]";
                }
                else
                {
                  lineStr += "[" + ArgToString(inst.args[2], false) + "]";
                }
                if(validElementOffset)
                {
                  if(constantElementOffset)
                  {
                    if(offset > 0)
                      lineStr += " + " + ToStr(offset) + " bytes";
                  }
                  else
                  {
                    lineStr += " + " + ArgToString(inst.args[3], false) + " bytes";
                  }
                }
                lineStr += " = ";
                lineStr += "{";
                bool needComma = false;
                for(uint32_t a = 4; a < 8; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    if(needComma)
                      lineStr += ", ";
                    lineStr += ArgToString(inst.args[a], false);
                    needComma = true;
                  }
                }
                lineStr += "}";
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.textureLoad"))
            {
              // TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 66);
              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                lineStr += resHandles[handleStr].name;
                lineStr += ".Load(";
                bool needComma = false;
                const EntryPointInterface::SRV *texture = resHandles[handleStr].srv;
                for(uint32_t a = 3; a < 6; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    if(needComma)
                      lineStr += ", ";
                    lineStr += ArgToString(inst.args[a], false);
                    needComma = true;
                  }
                }
                bool needText = true;
                if(!isUndef(inst.args[2]))
                {
                  rdcstr prefix;
                  bool showArg = true;
                  if(needText)
                  {
                    if(texture && texture->sampleCount > 1)
                    {
                      prefix = "SampleIndex = ";
                    }
                    else
                    {
                      prefix = "MipSlice = ";
                      uint32_t mipSlice;
                      if(getival<uint32_t>(inst.args[2], mipSlice))
                        showArg = mipSlice > 0;
                    }
                  }
                  if(showArg)
                  {
                    needText = false;
                    lineStr += ", ";
                    lineStr += prefix;
                    lineStr += ArgToString(inst.args[2], false);
                  }
                }
                needText = true;
                for(uint32_t a = 6; a < 9; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    lineStr += ", ";
                    if(needText)
                    {
                      lineStr += "Offset = ";
                      needText = false;
                    }
                    lineStr += ArgToString(inst.args[a], false);
                  }
                }
                lineStr += ")";
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.textureStore"))
            {
              // TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
              showDxFuncName = false;
              uint32_t dxopCode;
              RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
              RDCASSERTEQUAL(dxopCode, 67);
              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                lineStr += resHandles[handleStr].name;
                lineStr += "[";
                bool needComma = false;
                for(uint32_t a = 2; a < 5; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    if(needComma)
                      lineStr += ", ";
                    lineStr += ArgToString(inst.args[a], false);
                    needComma = true;
                  }
                }
                lineStr += "]";
                lineStr += " = ";
                lineStr += "{";
                needComma = false;
                for(uint32_t a = 5; a < 9; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    if(needComma)
                      lineStr += ", ";
                    lineStr += ArgToString(inst.args[a], false);
                    needComma = true;
                  }
                }
                lineStr += "}";
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(showDxFuncName && funcCallName.beginsWith("dx.op.sample") &&
                    !funcCallName.beginsWith("dx.op.sampleIndex"))
            {
              // Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
              // SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
              // SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
              // SampleGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)
              // SampleCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp)
              // SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
              // SampleCmpLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,lod)
              // SampleCmpGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)
              // SampleCmpBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,bias,clamp)
              showDxFuncName = false;
              rdcstr handleStr = ArgToString(inst.args[1], false);
              if(resHandles.count(handleStr) > 0)
              {
                uint32_t dxopCode;
                RDCASSERT(getival<uint32_t>(inst.args[0], dxopCode));
                lineStr += resHandles[handleStr].name;
                lineStr += ".";
                rdcstr dxFuncSig = funcNameSigs[dxopCode];
                int paramStart = dxFuncSig.find('(') + 1;
                if(paramStart > 0)
                  lineStr += dxFuncSig.substr(0, paramStart);
                else
                  lineStr += "UNKNOWN DX FUNCTION";

                // sampler is 2
                rdcstr samplerStr = ArgToString(inst.args[2], false);
                if(resHandles.count(samplerStr) > 0)
                  samplerStr = resHandles[samplerStr].name;
                lineStr += samplerStr;

                for(uint32_t a = 3; a < 7; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    lineStr += ", ";
                    lineStr += ArgToString(inst.args[a], false);
                  }
                }
                bool needText = true;
                for(uint32_t a = 7; a < 10; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    lineStr += ", ";
                    if(needText)
                    {
                      lineStr += "Offset = {";
                      needText = false;
                    }
                    lineStr += ArgToString(inst.args[a], false);
                  }
                }
                if(!needText)
                  lineStr += "}";

                int paramStrCount = (int)dxFuncSig.size();
                for(size_t a = 1; a < 10; ++a)
                {
                  if(paramStart < paramStrCount)
                  {
                    int paramEnd = dxFuncSig.find(',', paramStart);
                    if(paramEnd == -1)
                      paramEnd = paramStrCount;
                    paramStart = paramEnd + 1;
                  }
                }
                for(uint32_t a = 10; a < inst.args.size(); ++a)
                {
                  rdcstr paramNameStr;
                  if(paramStart < paramStrCount)
                  {
                    int paramEnd = dxFuncSig.find(',', paramStart);
                    if(paramEnd == -1)
                      paramEnd = paramStrCount - 1;
                    if(paramEnd > paramStart)
                    {
                      rdcstr dxParamName = dxFuncSig.substr(paramStart, paramEnd - paramStart);
                      paramStart = paramEnd + 1;
                      paramNameStr = "/*";
                      paramNameStr += dxParamName;
                      paramNameStr += "*/ ";
                    }
                  }
                  if(!isUndef(inst.args[a]))
                  {
                    lineStr += ", ";
                    lineStr += paramNameStr;
                    lineStr += ArgToString(inst.args[a], false);
                  }
                }
                lineStr += ")";
              }
              else
              {
                showDxFuncName = true;
              }
            }
            else if(funcCallName.beginsWith("llvm.dbg."))
            {
            }
            else
            {
              if(showDxFuncName)
              {
                rdcstr dxFuncSig;
                int paramStart = -1;
                if(Constant *op = cast<Constant>(inst.args[0]))
                {
                  uint32_t opcode = op->getU32();
                  if(opcode < ARRAY_COUNT(funcNameSigs))
                  {
                    dxFuncSig = funcNameSigs[opcode];
                    paramStart = dxFuncSig.find('(') + 1;
                    if(paramStart > 0)
                      lineStr += dxFuncSig.substr(0, paramStart);
                    else
                      lineStr += dxFuncSig;
                  }
                  else
                  {
                    lineStr += escapeStringIfNeeded(funcCallName);
                  }
                }
                bool first = true;
                int paramStrCount = (int)dxFuncSig.size();
                for(size_t a = 1; a < inst.args.size(); ++a)
                {
                  rdcstr paramNameStr;
                  if(paramStart < paramStrCount)
                  {
                    int paramEnd = dxFuncSig.find(',', paramStart);
                    if(paramEnd == -1)
                      paramEnd = paramStrCount - 1;
                    if(paramEnd > paramStart)
                    {
                      rdcstr dxParamName = dxFuncSig.substr(paramStart, paramEnd - paramStart);
                      paramStart = paramEnd + 1;
                      paramNameStr = "/*";
                      paramNameStr += dxParamName;
                      paramNameStr += "*/ ";
                    }
                  }
                  // Don't show "undef" parameters
                  if(!isUndef(inst.args[a]))
                  {
                    if(!first)
                      lineStr += ", ";

                    lineStr += paramNameStr;
                    rdcstr ssaStr = ArgToString(inst.args[a], false);
                    if(ssaAliases.count(ssaStr) == 0)
                      lineStr += ssaStr;
                    else
                      lineStr += ssaAliases[ssaStr];
                    first = false;
                  }
                }
                lineStr += ")";
              }
              else
              {
                lineStr += escapeStringIfNeeded(funcCallName);
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

                  lineStr += ArgToString(s, false, attrString);
                  first = false;

                  argIdx++;
                }
                lineStr += ")";

                if(paramAttrs && paramAttrs->functionSlot)
                  lineStr +=
                      StringFormat::Fmt(" #%u", m_FuncAttrGroups.indexOf(paramAttrs->functionSlot));
              }
            }
          }
          break;
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
            bool fallbackOutput = true;
            if(!inst.type->isVoid())
            {
              // type "float addrspace(3)*" : addrspace(3) is DXIL specific, see DXIL::Type::PointerAddrSpace
              rdcstr typeStr = inst.type->toString();
              int start = typeStr.find(" addrspace(");
              if(start > 0)
              {
                rdcstr scalarType = typeStr.substr(0, start);
                scalarType.trim();

                start += 11;
                int end = typeStr.find(')', start);
                if(end > start)
                {
                  // Example output:
                  // DXC:
                  // %3 = getelementptr [6 x float], [6 x float] addrspace(3)*
                  // @"\01?s_x@@3@$$A.1dim", i32 0, i32 %9

                  // RD: GroupShared float* _3 = s_x[_9];
                  fallbackOutput = false;

                  rdcstr addrspaceStr(typeStr.substr(start, end - start));
                  int32_t value = atoi(addrspaceStr.c_str());
                  DXIL::Type::PointerAddrSpace addrspace = (DXIL::Type::PointerAddrSpace)value;

                  switch(addrspace)
                  {
                    case DXIL::Type::PointerAddrSpace::Default: lineStr = ""; break;
                    case DXIL::Type::PointerAddrSpace::DeviceMemory:
                      lineStr = "DeviceMemory";
                      break;
                    case DXIL::Type::PointerAddrSpace::CBuffer: lineStr = "CBuffer"; break;
                    case DXIL::Type::PointerAddrSpace::GroupShared: lineStr = "GroupShared"; break;
                    case DXIL::Type::PointerAddrSpace::GenericPointer: lineStr = ""; break;
                    case DXIL::Type::PointerAddrSpace::ImmediateCBuffer:
                      lineStr = "ImmediateCBuffer";
                      break;
                  };

                  lineStr += " ";
                  lineStr += scalarType;
                  lineStr += "* ";

                  if(!inst.getName().empty())
                    lineStr += StringFormat::Fmt("%c%s = ", DXIL::dxilIdentifier,
                                                 escapeStringIfNeeded(inst.getName()).c_str());
                  else if(inst.slot != ~0U)
                    lineStr += StringFormat::Fmt("%c%u = ", DXIL::dxilIdentifier, inst.slot);

                  // arg[0] : ptr
                  rdcstr ptrStr = ArgToString(inst.args[0], false);
                  // Try to de-mangle the pointer name
                  // @"\01?shared_pos@@3PAY0BC@$$CAMA.1dim" -> shared_pos
                  // Take the string between first alphabetical character and last
                  // alphanumeric character or "_"
                  start = 0;
                  int strEnd = (int)ptrStr.size();
                  while(start < strEnd)
                  {
                    if(isalpha(ptrStr[start]))
                      break;
                    ++start;
                  }
                  if(start < strEnd)
                  {
                    end = start + 1;
                    while(end < strEnd)
                    {
                      char c = ptrStr[end];
                      if(!isalnum(c) && c != '_')
                        break;
                      ++end;
                    }
                  }
                  if(end > start)
                    ptrStr = ptrStr.substr(start, end - start);

                  lineStr += ptrStr;
                  // arg[1] : index 0
                  bool first = true;
                  if(inst.args.size() > 1)
                  {
                    uint32_t v = 0;
                    if(!getival<uint32_t>(inst.args[1], v) || (v > 0))
                    {
                      lineStr += "[";
                      lineStr += ArgToString(inst.args[1], false);
                      lineStr += "]";
                      first = false;
                    }
                  }

                  // arg[2..] : index 1...N
                  for(size_t a = 2; a < inst.args.size(); ++a)
                  {
                    if(first)
                      lineStr += "[";
                    else
                      lineStr += " + ";

                    lineStr += ArgToString(inst.args[a], false);

                    if(first)
                    {
                      lineStr += "]";
                      first = false;
                    }
                  }
                }
              }
            }
            if(fallbackOutput)
            {
              lineStr += "getelementptr ";
              bool first = true;
              for(const Value *s : inst.args)
              {
                if(!first)
                  lineStr += ", ";

                lineStr += ArgToString(s, false);
                first = false;
              }
            }
            if(inst.opFlags() & InstructionFlags::InBounds)
              commentStr += "inbounds ";
            break;
          }
          case Operation::LoadAtomic: commentStr += "atomic ";
          case Operation::Load:
          {
            lineStr += "*";
            if(inst.opFlags() & InstructionFlags::Volatile)
              commentStr += "volatile ";
            bool first = true;
            for(const Value *s : inst.args)
            {
              if(!first)
                lineStr += ", ";

              lineStr += ArgToString(s, false);
              first = false;
            }
            if(inst.align > 0)
              commentStr += StringFormat::Fmt("align %u ", (1U << inst.align) >> 1);
            break;
          }
          case Operation::StoreAtomic: commentStr += "atomic ";
          case Operation::Store:
          {
            if(inst.opFlags() & InstructionFlags::Volatile)
              commentStr += "volatile ";
            lineStr = "*";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += " = ";
            lineStr += ArgToString(inst.args[1], false);
            if(inst.align > 0)
              commentStr += StringFormat::Fmt("align %u ", (1U << inst.align) >> 1);
            break;
          }
          case Operation::FOrdEqual:
          case Operation::FOrdGreater:
          case Operation::FOrdGreaterEqual:
          case Operation::FOrdLess:
          case Operation::FOrdLessEqual:
          case Operation::FOrdNotEqual:
          case Operation::FUnordEqual:
          case Operation::FUnordGreater:
          case Operation::FUnordGreaterEqual:
          case Operation::FUnordLess:
          case Operation::FUnordLessEqual:
          case Operation::FUnordNotEqual:
          {
            rdcstr opStr;
            switch(inst.op)
            {
              case Operation::FOrdEqual: opStr = " = "; break;
              case Operation::FOrdGreater: opStr = " > "; break;
              case Operation::FOrdGreaterEqual: opStr = " >= "; break;
              case Operation::FOrdLess: opStr = " < "; break;
              case Operation::FOrdLessEqual: opStr = " <= "; break;
              case Operation::FOrdNotEqual: opStr = " != "; break;
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
          case Operation::FOrd:
          {
            // ord: yields true if both operands are not a QNAN.
            lineStr += "!isqnan(";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ")";
            lineStr += " && ";
            lineStr += "!isqnan(";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ")";
            break;
          }
          case Operation::FUnord:
          {
            // uno: yields true if either operand is a QNAN.
            lineStr += "isqnan(";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += ")";
            lineStr += " || ";
            lineStr += "isqnan(";
            lineStr += ArgToString(inst.args[1], false);
            lineStr += ")";
            break;
          }
          case Operation::FOrdFalse: lineStr += "false"; break;
          case Operation::FOrdTrue: lineStr += "true"; break;
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
            lineStr += ArgToString(inst.args[2], false);
            lineStr += " ? ";
            lineStr += ArgToString(inst.args[0], false);
            lineStr += " : ";
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
            if(inst.args.size() > 1)
            {
              lineStr += "if (";
              lineStr += ArgToString(inst.args[2], false);
              lineStr += ") goto ";
              lineStr += StringFormat::Fmt("%s", ArgToString(inst.args[0], false).c_str());
              lineStr += " else goto ";
              lineStr += StringFormat::Fmt("%s", ArgToString(inst.args[1], false).c_str());
            }
            else
            {
              lineStr += "goto ";
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
            // AnnotateHandle(res,props)
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
