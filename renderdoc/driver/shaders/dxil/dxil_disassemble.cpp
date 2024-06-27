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

// Detect the DXC output which uses a load from global variable called "dx.nothing.*" instead of a Nop
bool DXIL::IsDXCNop(const Instruction &inst)
{
  if(inst.op != Operation::Load)
    return false;

  if(const Constant *c = cast<Constant>(inst.args[0]))
  {
    if((c->op == Operation::GetElementPtr) && c->isCompound())
    {
      const rdcarray<DXIL::Value *> &members = c->getMembers();
      if(const GlobalVar *gv = cast<GlobalVar>(members[0]))
      {
        if(gv->name.beginsWith("dx.nothing."))
          return true;
      }
    }
  }
  return false;
}

bool DXIL::IsLLVMDebugCall(const Instruction &inst)
{
  return ((inst.op == Operation::Call) && (inst.getFuncCall()->name.beginsWith("llvm.dbg.")));
}

// true if the Value is an SSA value i.e. from an instruction, not a constant etc.
bool DXIL::IsSSA(const Value *dxilValue)
{
  if(const Instruction *inst = cast<Instruction>(dxilValue))
    return true;
  if(const Constant *c = cast<Constant>(dxilValue))
    return false;
  if(const Literal *lit = cast<Literal>(dxilValue))
    return false;
  if(const Block *block = cast<Block>(dxilValue))
    return false;
  if(const GlobalVar *gv = cast<GlobalVar>(dxilValue))
    return false;
  if(const Function *func = cast<Function>(dxilValue))
    return false;
  if(const Metadata *meta = cast<Metadata>(dxilValue))
    return false;

  RDCERR("Unknown DXIL::Value type");
  return false;
}

static const char *shaderNames[] = {
    "Pixel",      "Vertex",  "Geometry",      "Hull",         "Domain",
    "Compute",    "Library", "RayGeneration", "Intersection", "AnyHit",
    "ClosestHit", "Miss",    "Callable",      "Mesh",         "Amplification",
};

// clang-format off
static const char *dxOpFunctionNames[] = {
"dx.op.tempRegLoad",
"dx.op.tempRegStore",
"dx.op.minPrecXRegLoad",
"dx.op.minPrecXRegStore",
"dx.op.loadInput",
"dx.op.storeOutput",
"dx.op.unary",
"dx.op.unary",
"dx.op.isSpecialFloat",
"dx.op.isSpecialFloat",
"dx.op.isSpecialFloat",
"dx.op.isSpecialFloat",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unaryBits",
"dx.op.unaryBits",
"dx.op.unaryBits",
"dx.op.unaryBits",
"dx.op.binary",
"dx.op.binary",
"dx.op.binary",
"dx.op.binary",
"dx.op.binary",
"dx.op.binary",
"dx.op.binaryWithTwoOuts",
"dx.op.binaryWithTwoOuts",
"dx.op.binaryWithTwoOuts",
"dx.op.binaryWithCarryOrBorrow",
"dx.op.binaryWithCarryOrBorrow",
"dx.op.tertiary",
"dx.op.tertiary",
"dx.op.tertiary",
"dx.op.tertiary",
"dx.op.tertiary",
"dx.op.tertiary",
"dx.op.tertiary",
"dx.op.quaternary",
"dx.op.dot2",
"dx.op.dot3",
"dx.op.dot4",
"dx.op.createHandle",
"dx.op.cbufferLoad",
"dx.op.cbufferLoadLegacy",
"dx.op.sample",
"dx.op.sampleBias",
"dx.op.sampleLevel",
"dx.op.sampleGrad",
"dx.op.sampleCmp",
"dx.op.sampleCmpLevelZero",
"dx.op.textureLoad",
"dx.op.textureStore",
"dx.op.bufferLoad",
"dx.op.bufferStore",
"dx.op.bufferUpdateCounter",
"dx.op.checkAccessFullyMapped",
"dx.op.getDimensions",
"dx.op.textureGather",
"dx.op.textureGatherCmp",
"dx.op.texture2DMSGetSamplePosition",
"dx.op.renderTargetGetSamplePosition",
"dx.op.renderTargetGetSampleCount",
"dx.op.atomicBinOp",
"dx.op.atomicCompareExchange",
"dx.op.barrier",
"dx.op.calculateLOD",
"dx.op.discard",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.unary",
"dx.op.evalSnapped",
"dx.op.evalSampleIndex",
"dx.op.evalCentroid",
"dx.op.sampleIndex",
"dx.op.coverage",
"dx.op.innerCoverage",
"dx.op.threadId",
"dx.op.groupId",
"dx.op.threadIdInGroup",
"dx.op.flattenedThreadIdInGroup",
"dx.op.emitStream",
"dx.op.cutStream",
"dx.op.emitThenCutStream",
"dx.op.gsInstanceID",
"dx.op.makeDouble",
"dx.op.splitDouble",
"dx.op.loadOutputControlPoint",
"dx.op.loadPatchConstant",
"dx.op.domainLocation",
"dx.op.storePatchConstant",
"dx.op.outputControlPointID",
"dx.op.primitiveID",
"dx.op.cycleCounterLegacy",
"dx.op.waveIsFirstLane",
"dx.op.waveGetLaneIndex",
"dx.op.waveGetLaneCount",
"dx.op.waveAnyTrue",
"dx.op.waveAllTrue",
"dx.op.waveActiveAllEqual",
"dx.op.waveActiveBallot",
"dx.op.waveReadLaneAt",
"dx.op.waveReadLaneFirst",
"dx.op.waveActiveOp",
"dx.op.waveActiveBit",
"dx.op.wavePrefixOp",
"dx.op.quadReadLaneAt",
"dx.op.quadOp",
"dx.op.bitcastI16toF16",
"dx.op.bitcastF16toI16",
"dx.op.bitcastI32toF32",
"dx.op.bitcastF32toI32",
"dx.op.bitcastI64toF64",
"dx.op.bitcastF64toI64",
"dx.op.legacyF32ToF16",
"dx.op.legacyF16ToF32",
"dx.op.legacyDoubleToFloat",
"dx.op.legacyDoubleToSInt32",
"dx.op.legacyDoubleToUInt32",
"dx.op.waveAllOp",
"dx.op.wavePrefixOp",
"dx.op.attributeAtVertex",
"dx.op.viewID",
"dx.op.rawBufferLoad",
"dx.op.rawBufferStore",
"dx.op.instanceID",
"dx.op.instanceIndex",
"dx.op.hitKind",
"dx.op.rayFlags",
"dx.op.dispatchRaysIndex",
"dx.op.dispatchRaysDimensions",
"dx.op.worldRayOrigin",
"dx.op.worldRayDirection",
"dx.op.objectRayOrigin",
"dx.op.objectRayDirection",
"dx.op.objectToWorld",
"dx.op.worldToObject",
"dx.op.rayTMin",
"dx.op.rayTCurrent",
"dx.op.ignoreHit",
"dx.op.acceptHitAndEndSearch",
"dx.op.traceRay",
"dx.op.reportHit",
"dx.op.callShader",
"dx.op.createHandleForLib",
"dx.op.primitiveIndex",
"dx.op.dot2AddHalf",
"dx.op.dot4AddPacked",
"dx.op.dot4AddPacked",
"dx.op.waveMatch",
"dx.op.waveMultiPrefixOp",
"dx.op.waveMultiPrefixBitCount",
"dx.op.setMeshOutputCounts",
"dx.op.emitIndices",
"dx.op.getMeshPayload",
"dx.op.storeVertexOutput",
"dx.op.storePrimitiveOutput",
"dx.op.dispatchMesh",
"dx.op.writeSamplerFeedback",
"dx.op.writeSamplerFeedbackBias",
"dx.op.writeSamplerFeedbackLevel",
"dx.op.writeSamplerFeedbackGrad",
"dx.op.allocateRayQuery",
"dx.op.rayQuery_TraceRayInline",
"dx.op.rayQuery_Proceed",
"dx.op.rayQuery_Abort",
"dx.op.rayQuery_CommitNonOpaqueTriangleHit",
"dx.op.rayQuery_CommitProceduralPrimitiveHit",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateMatrix",
"dx.op.rayQuery_StateMatrix",
"dx.op.rayQuery_StateMatrix",
"dx.op.rayQuery_StateMatrix",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateVector",
"dx.op.rayQuery_StateVector",
"dx.op.geometryIndex",
"dx.op.rayQuery_StateScalar",
"dx.op.rayQuery_StateScalar",
"dx.op.annotateHandle",
"dx.op.createHandleFromBinding",
"dx.op.createHandleFromHeap",
"dx.op.unpack4x8",
"dx.op.pack4x8",
"dx.op.isHelperLane",
"dx.op.quadVote",
"dx.op.textureGatherRaw",
"dx.op.sampleCmpLevel",
"dx.op.textureStoreSample",
"dx.op.waveMatrix_Annotate",
"dx.op.waveMatrix_Depth",
"dx.op.waveMatrix_Fill",
"dx.op.waveMatrix_LoadRawBuf",
"dx.op.waveMatrix_LoadGroupShared",
"dx.op.waveMatrix_StoreRawBuf",
"dx.op.waveMatrix_StoreGroupShared",
"dx.op.waveMatrix_Multiply",
"dx.op.waveMatrix_Multiply",
"dx.op.waveMatrix_ScalarOp",
"dx.op.waveMatrix_Accumulate",
"dx.op.waveMatrix_Accumulate",
"dx.op.allocateNodeOutputRecords",
"dx.op.getNodeRecordPtr",
"dx.op.incrementOutputCount",
"dx.op.outputComplete",
"dx.op.getInputRecordCount",
"dx.op.finishedCrossGroupSharing",
"dx.op.barrierByMemoryType",
"dx.op.barrierByMemoryHandle",
"dx.op.barrierByNodeRecordHandle",
"dx.op.createNodeOutputHandle",
"dx.op.indexNodeHandle",
"dx.op.annotateNodeHandle",
"dx.op.createNodeInputRecordHandle",
"dx.op.annotateNodeRecordHandle",
"dx.op.nodeOutputIsValid",
"dx.op.getRemainingRecursionLevels",
"dx.op.sampleCmpGrad",
"dx.op.sampleCmpBias",
"dx.op.startVertexLocation",
"dx.op.startInstanceLocation",
};

static const char *dxcOpNames[] = {
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

static const char *funcNameSigs[] = {
"TempRegLoad(index)",
"TempRegStore(index,value)",
"MinPrecXRegLoad(regIndex,index,component)",
"MinPrecXRegStore(regIndex,index,component,value)",
"LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)",
"StoreOutput(outputSigId,rowIndex,colIndex,value)",
"abs()",
"sat()",
"isnan()",
"isinf()",
"isfinite()",
"IsNormal()",
"cos()",
"sin()",
"tan()",
"acos()",
"asin()",
"atan()",
"cosh()",
"sinh()",
"tanh()",
"exp()",
"Frc()",
"log()",
"sqrt()",
"rsqrt()",
"Round_ne()",
"Round_ni()",
"Round_pi()",
"Round_z()",
"Bfrev()",
"countbits()",
"firstbitlow()",
"firstbithigh()",
"FirstbitSHi()",
"max()",
"min()",
"IMax()",
"IMin()",
"UMax()",
"UMin()",
"IMul()",
"UMul()",
"UDiv()",
"UAddc()",
"USubb()",
"mad()",
"fma()",
"IMad()",
"UMad()",
"msad4()",
"Ibfe()",
"Ubfe()",
"Bfi(width,offset,value,replacedValue)",
"dot2(ax,ay,bx,by)",
"dot3(ax,ay,az,bx,by,bz)",
"dot4(ax,ay,az,aw,bx,by,bz,bw)",
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
"GetRenderTargetSamplePosition(index)",
"GetRenderTargetSampleCount()",
"AtomicBinOp(handle,atomicOp,offset0,offset1,offset2,newValue)",
"AtomicCompareExchange(handle,offset0,offset1,offset2,compareValue,newValue)",
"Barrier(barrierMode)",
"CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)",
"Discard(condition)",
"ddx_coarse()",
"ddy_coarse()",
"ddx_fine()",
"ddy_fine()",
"EvaluateAttributeSnapped(inputSigId,inputRowIndex,inputColIndex,offsetX,offsetY)",
"EvaluateAttributeAtSample(inputSigId,inputRowIndex,inputColIndex,sampleIndex)",
"EvaluateAttributeCentroid(inputSigId,inputRowIndex,inputColIndex)",
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
"asdouble(lo,hi)",
"SplitDouble()",
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
"WaveActiveAllEqual()",
"WaveActiveBallot(cond)",
"WaveReadLaneAt(value,lane)",
"WaveReadLaneFirst()",
"WaveActiveOp(value,op,sop)",
"WaveActiveBit(value,op)",
"WavePrefixOp(value,op,sop)",
"QuadReadLaneAt(value,quadLane)",
"QuadOp(value,op)",
"BitcastI16toF16()",
"BitcastF16toI16()",
"asfloat()",
"asint()",
"BitcastI64toF64()",
"BitcastF64toI64()",
"f32tof16()",
"f16tof32()",
"LegacyDoubleToFloat()",
"LegacyDoubleToSInt32()",
"LegacyDoubleToUInt32()",
"WaveAllBitCount()",
"WavePrefixBitCount()",
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
"WaveMatch()",
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

static rdcstr GetSamplerTypeName(const Type *type)
{
  // variable should be a pointer to the underlying type
  RDCASSERTEQUAL(type->type, Type::Pointer);
  const Type *resType = type->inner;

  // samplers should be entirely opaque, so we return the struct as-is now
  if(resType->type == Type::Struct)
  {
    rdcstr compType = resType->name;
    int start = compType.find('.');
    if(start > 0)
      compType = compType.substr(start + 1);
    return compType;
  }
  return "UNHANDLED RESOURCE TYPE";
}

static rdcstr GetResourceTypeName(const Type *type)
{
  // variable should be a pointer to the underlying type
  RDCASSERTEQUAL(type->type, Type::Pointer);
  const Type *resType = type->inner;

  // arrayed resources we want to remove the outer array-of-bindings here
  if(resType->type == Type::Array && resType->inner->type == Type::Struct)
    resType = resType->inner;

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

rdcstr Program::GetHandleAlias(const rdcstr &handleStr) const
{
  auto it = m_SsaAliases.find(handleStr);
  if(it != m_SsaAliases.end())
    return it->second;
  return handleStr;
}

void Program::Parse(const DXBC::Reflection *reflection)
{
  if(m_Parsed)
    return;

  SettleIDs();

  m_EntryPointInterfaces.clear();
  FillEntryPointInterfaces();
  m_SsaAliases.clear();
  ParseReferences(reflection);

  m_Parsed = true;
}

void Program::SettleIDs()
{
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
  const bool dxcStyleFormatting = m_DXCStyle;
  const char dxilIdentifier = Program::GetDXILIdentifier(dxcStyleFormatting);
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
        ret += metaConst->toString(dxcStyleFormatting, withTypes);
      }
      else if(m.isConstant && metaInst)
      {
        ret += m.valString(dxcStyleFormatting);
      }
      else if(m.isConstant && metaGlobal)
      {
        if(withTypes)
          ret += metaGlobal->type->toString(dxcStyleFormatting) + " ";
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
      ret = global->type->toString(dxcStyleFormatting) + " ";
    ret += attrString;
    ret += "@" + escapeStringIfNeeded(global->name);
  }
  else if(const Constant *c = cast<Constant>(v))
  {
    ret += attrString;
    ret = c->toString(dxcStyleFormatting, withTypes);
  }
  else if(const Instruction *inst = cast<Instruction>(v))
  {
    if(withTypes)
      ret = inst->type->toString(dxcStyleFormatting) + " ";
    ret += attrString;
    if(inst->getName().empty())
      ret += StringFormat::Fmt("%c%u", dxilIdentifier, inst->slot);
    else
      ret += StringFormat::Fmt("%c%s", dxilIdentifier, escapeStringIfNeeded(inst->getName()).c_str());
  }
  else if(const Block *block = cast<Block>(v))
  {
    if(withTypes)
      ret = "label ";
    ret += attrString;
    if(dxcStyleFormatting)
    {
      if(block->name.empty())
        ret += StringFormat::Fmt("%c%u", dxilIdentifier, block->slot);
      else
        ret += StringFormat::Fmt("%c%s", dxilIdentifier, escapeStringIfNeeded(block->name).c_str());
    }
    else
    {
      if(block->name.empty())
        ret += StringFormat::Fmt("%clabel%u", dxilIdentifier, block->slot);
      else
        ret += StringFormat::Fmt("%clabel_%s%u", dxilIdentifier,
                                 DXBC::BasicDemangle(block->name).c_str(), block->id);
    }
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
  const bool dxcStyleFormatting = m_DXCStyle;
  rdcstr ret;
  bool printedTypes = false;
  for(const Type *typ : m_Accum.printOrderTypes)
  {
    if(typ->type == Type::Struct && !typ->name.empty())
    {
      rdcstr name = typ->toString(dxcStyleFormatting);
      ret += StringFormat::Fmt("%s = type { ", name.c_str());
      bool first = true;
      for(const Type *t : typ->members)
      {
        if(!first)
          ret += ", ";
        first = false;
        ret += StringFormat::Fmt("%s", t->toString(dxcStyleFormatting).c_str());
      }
      if(typ->members.empty())
      {
        if(ret.back() == ' ')
          ret.pop_back();
        ret += "}\n";
      }
      else
      {
        ret += " }\n";
      }

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
  const bool dxcStyleFormatting = m_DXCStyle;
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
      ret += g.initialiser->toString(dxcStyleFormatting, true);
    else
      ret += g.type->inner->toString(dxcStyleFormatting);

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
  const bool dxcStyleFormatting = m_DXCStyle;
  rdcstr ret;
  size_t numIdx = 0;
  size_t dbgIdx = 0;

  for(uint32_t i = 0; i < m_NextMetaSlot; i++)
  {
    if(numIdx < m_MetaSlots.size() && m_MetaSlots[numIdx]->slot == i)
    {
      rdcstr metaline =
          StringFormat::Fmt("!%u = %s%s\n", i, m_MetaSlots[numIdx]->isDistinct ? "distinct " : "",
                            m_MetaSlots[numIdx]->valString(dxcStyleFormatting).c_str());
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
      ret += StringFormat::Fmt("!%u = %s\n", i,
                               m_DebugLocations[dbgIdx].toString(dxcStyleFormatting).c_str());
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

    Parse(reflection);

    if(dxcStyle)
      MakeDXCDisassemblyString();
    else
      MakeRDDisassemblyString(reflection);
  }
  return m_Disassembly;
}

void Program::MakeDXCDisassemblyString()
{
  const bool dxcStyleFormatting = true;
  const char dxilIdentifier = Program::GetDXILIdentifier(dxcStyleFormatting);

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
    m_Disassembly += func.type->declFunction("@" + escapeStringIfNeeded(func.name), func.args,
                                             func.attrs, dxcStyleFormatting);

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
          m_Disassembly += StringFormat::Fmt("%c%s = ", dxilIdentifier,
                                             escapeStringIfNeeded(inst.getName()).c_str());
        else if(inst.slot != ~0U)
          m_Disassembly += StringFormat::Fmt("%c%u = ", dxilIdentifier, inst.slot);

        bool debugCall = false;

        switch(inst.op)
        {
          case Operation::NoOp: m_Disassembly += "??? "; break;
          case Operation::Call:
          {
            const rdcstr &funcCallName = inst.getFuncCall()->name;
            m_Disassembly += "call " + inst.type->toString(dxcStyleFormatting);
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
            m_Disassembly += inst.type->toString(dxcStyleFormatting);
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
              m_Disassembly += "ret " + inst.type->toString(dxcStyleFormatting);
            else
              m_Disassembly += "ret " + ArgToString(inst.args[0], true);
            break;
          }
          case Operation::Unreachable: m_Disassembly += "unreachable"; break;
          case Operation::Alloca:
          {
            m_Disassembly += "alloca ";
            m_Disassembly += inst.type->inner->toString(dxcStyleFormatting);
            if(inst.align > 0)
              m_Disassembly += StringFormat::Fmt(", align %u", (1U << inst.align) >> 1);
            break;
          }
          case Operation::GetElementPtr:
          {
            m_Disassembly += "getelementptr ";
            if(inst.opFlags() & InstructionFlags::InBounds)
              m_Disassembly += "inbounds ";
            m_Disassembly += inst.args[0]->type->inner->toString(dxcStyleFormatting);
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
            m_Disassembly += inst.type->toString(dxcStyleFormatting);
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
            m_Disassembly += inst.type->toString(dxcStyleFormatting);
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
            m_Disassembly += inst.type->toString(dxcStyleFormatting);
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
            m_Disassembly += expr->valString(dxcStyleFormatting);

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
            if(opcode < ARRAY_COUNT(dxcOpNames))
            {
              m_Disassembly += "  ; ";
              m_Disassembly += dxcOpNames[opcode];
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
              labelName += StringFormat::Fmt("%c%u", dxilIdentifier, pred->slot);
            else
              labelName += StringFormat::Fmt("%c%s", dxilIdentifier,
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

    // Does does minOffset-maxOffset fit within this variable?
    if(voffs <= minOffset && voffs + v.type.bytesize >= maxOffset)
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

static rdcstr MakeCBufferRegisterStr(uint32_t reg, uint32_t bytesPerElement,
                                     DXIL::EntryPointInterface::CBuffer cbuffer,
                                     const rdcstr &handleStr)
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
    uint32_t maxOffset = minOffset + bytesPerElement;
    rdcstr prefix;
    const DXBC::CBufferVariable *var =
        FindCBufferVar(minOffset, maxOffset, cbuffer.cbufferRefl->variables, baseOffset, prefix);
    if(var)
    {
      ret += handleStr + "." + prefix + var->name;
      int32_t varOffset = baseOffset - regOffset;

      // if it's an array, add the index based on the relative index to the base offset
      if(var->type.elements > 1)
      {
        uint32_t byteSize = var->type.bytesize;

        // round up the byte size to a the nearest vec4 in case it's not quite a multiple
        byteSize = AlignUp16(byteSize);

        const uint32_t elementSize = byteSize / var->type.elements;
        const uint32_t elementIndex = abs(varOffset) / elementSize;

        ret += StringFormat::Fmt("[%u]", elementIndex);

        // add on so that if there's any further offset, it can be processed
        varOffset += elementIndex * elementSize;
      }

      // or if it's a matrix
      if((var->type.varClass == DXBC::CLASS_MATRIX_ROWS && var->type.cols > 1) ||
         (var->type.varClass == DXBC::CLASS_MATRIX_COLUMNS && var->type.rows > 1))
      {
        ret += StringFormat::Fmt("[%u]", abs(varOffset) / 16);
      }
      // or if it's a vector
      if((var->type.varClass == DXBC::CLASS_VECTOR && var->type.cols > 1) ||
         (var->type.varClass == DXBC::CLASS_SCALAR && var->type.cols > 1))
      {
        offset = var->offset;
        offset -= regOffset;

        uint32_t byteSize = var->type.bytesize;
        const uint32_t elementSize = byteSize / var->type.cols;
        const char *swizzle = "xyzw";
        // TODO: handle double vectors : the load might start part way through
        ret += ".x";
        offset += elementSize;
        for(uint32_t c = 1; c < var->type.cols; ++c)
        {
          if(offset > 16)
            break;
          ret += ", ";
          ret += handleStr + "." + prefix + var->name;
          ret += StringFormat::Fmt(".%c", swizzle[c & 0x3]);
          offset += elementSize;
        }
      }
      offset = varOffset + var->type.bytesize;
    }
    else
    {
      ret += "<padding>";
      offset += bytesPerElement;
    }
  }
  ret += "}";
  return ret;
}

rdcstr ProcessNormCompType(ComponentType &compType)
{
  if(compType == ComponentType::SNormF16)
  {
    compType = ComponentType::F16;
    return "snorm ";
  }
  else if(compType == ComponentType::SNormF32)
  {
    compType = ComponentType::F32;
    return "snorm ";
  }
  else if(compType == ComponentType::SNormF64)
  {
    compType = ComponentType::F64;
    return "snorm ";
  }
  else if(compType == ComponentType::UNormF16)
  {
    compType = ComponentType::F16;
    return "unorm ";
  }
  else if(compType == ComponentType::UNormF32)
  {
    compType = ComponentType::F32;
    return "unorm ";
  }
  else if(compType == ComponentType::UNormF64)
  {
    compType = ComponentType::F64;
    return "unorm ";
  }

  return rdcstr();
}

void Program::MakeRDDisassemblyString(const DXBC::Reflection *reflection)
{
  const bool dxcStyleFormatting = m_DXCStyle;
  const char dxilIdentifier = Program::GetDXILIdentifier(dxcStyleFormatting);

  m_Disassembly.clear();
  m_DisassemblyInstructionLine = 1;

  m_Disassembly += StringFormat::Fmt("; %s Shader, compiled under SM%u.%u",
                                     shaderNames[int(m_Type)], m_Major, m_Minor);
  DisassemblyAddNewLine(2);

  // TODO: output structs using named meta data if it exists
  m_Disassembly += DisassembleTypes(m_DisassemblyInstructionLine);
  m_Disassembly += DisassembleGlobalVars(m_DisassemblyInstructionLine);

  const char *swizzle = "xyzw";

  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = *m_Functions[i];

    m_Accum.processFunction(m_Functions[i]);

    if(func.external)
      continue;

    if(!func.external)
    {
      EntryPointInterface *entryPoint = NULL;
      for(size_t e = 0; e < m_EntryPointInterfaces.size(); ++e)
      {
        if(func.name == m_EntryPointInterfaces[e].name)
        {
          entryPoint = &m_EntryPointInterfaces[e];
          break;
        }
      }

      // Display inputs/outputs and resource
      if(entryPoint)
      {
        bool needBlankLine = false;

        if(!entryPoint->inputs.empty())
        {
          m_Disassembly += "Inputs";
          DisassemblyAddNewLine();
          for(size_t j = 0; j < entryPoint->inputs.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();
            EntryPointInterface::Signature &sig = entryPoint->inputs[j];

            m_Disassembly += "  ";

            ComponentType compType = sig.type;

            m_Disassembly += ProcessNormCompType(compType);

            VarType varType = VarTypeForComponentType(compType);
            m_Disassembly += ToStr(varType).c_str();

            if(sig.cols > 1)
              m_Disassembly += ToStr(sig.cols);

            if(reflection && sig.rows == 1 && j < reflection->InputSig.size())
            {
              const SigParameter &sigParam = reflection->InputSig[j];
              if(sigParam.semanticName == sig.name)
              {
                sig.name = sigParam.semanticIdxName;
              }
            }
            m_Disassembly += " " + sig.name;
            if(sig.rows > 1)
              m_Disassembly += "[" + ToStr(sig.rows) + "]";
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }
        else if(reflection && !reflection->InputSig.empty())
        {
          m_Disassembly += "Inputs";
          DisassemblyAddNewLine();
          for(size_t j = 0; j < reflection->InputSig.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();

            const SigParameter &sig = reflection->InputSig[j];

            m_Disassembly += "  ";
            m_Disassembly += ToStr(sig.varType).c_str();
            if(sig.compCount > 1)
              m_Disassembly += ToStr(sig.compCount);

            m_Disassembly += " " + sig.semanticIdxName;
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }

        if(!entryPoint->outputs.empty())
        {
          if(needBlankLine)
          {
            DisassemblyAddNewLine();
            needBlankLine = false;
          }

          m_Disassembly += "Outputs";
          DisassemblyAddNewLine();
          for(size_t j = 0; j < entryPoint->outputs.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();
            EntryPointInterface::Signature &sig = entryPoint->outputs[j];

            m_Disassembly += "  ";

            ComponentType compType = sig.type;

            m_Disassembly += ProcessNormCompType(compType);

            VarType varType = VarTypeForComponentType(compType);
            m_Disassembly += ToStr(varType).c_str();

            if(sig.cols > 1)
              m_Disassembly += ToStr(sig.cols);

            if(reflection && sig.rows == 1 && j < reflection->OutputSig.size())
            {
              const SigParameter &sigParam = reflection->OutputSig[j];
              if(sigParam.semanticName == sig.name)
                sig.name = sigParam.semanticIdxName;
            }
            m_Disassembly += " " + sig.name;
            if(sig.rows > 1)
              m_Disassembly += "[" + ToStr(sig.rows) + "]";
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }
        else if(reflection && !reflection->OutputSig.empty())
        {
          if(needBlankLine)
          {
            DisassemblyAddNewLine();
            needBlankLine = false;
          }

          m_Disassembly += "Outputs";
          DisassemblyAddNewLine();
          for(size_t j = 0; j < reflection->OutputSig.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();

            const SigParameter &sig = reflection->OutputSig[j];

            m_Disassembly += "  ";
            m_Disassembly += ToStr(sig.varType).c_str();
            if(sig.compCount > 1)
              m_Disassembly += ToStr(sig.compCount);

            m_Disassembly += " " + sig.semanticIdxName;
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }

        if(!entryPoint->srvs.empty())
        {
          for(size_t j = 0; j < entryPoint->srvs.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();
            EntryPointInterface::SRV &srv = entryPoint->srvs[j];
            m_Disassembly += GetResourceShapeName(srv.shape, false);
            if(srv.shape != DXIL::ResourceKind::RTAccelerationStructure)
              m_Disassembly += "<" + GetResourceTypeName(srv.type) + ">";
            m_Disassembly += " " + srv.name;
            if(srv.regCount > 1)
            {
              m_Disassembly += "[";
              if(srv.regCount != ~0U)
                m_Disassembly += ToStr(srv.regCount);
              m_Disassembly += "]";
            }
            m_Disassembly +=
                " : register(t" + ToStr(srv.regBase) + ", space" + ToStr(srv.space) + ")";
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }

        if(!entryPoint->uavs.empty())
        {
          for(size_t j = 0; j < entryPoint->uavs.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();
            EntryPointInterface::UAV &uav = entryPoint->uavs[j];
            m_Disassembly += GetResourceShapeName(uav.shape, true);
            m_Disassembly += "<" + GetResourceTypeName(uav.type) + ">";
            m_Disassembly += " " + uav.name;
            if(uav.regCount > 1)
            {
              m_Disassembly += "[";
              if(uav.regCount != ~0U)
                m_Disassembly += ToStr(uav.regCount);
              m_Disassembly += "]";
            }
            m_Disassembly +=
                " : register(u" + ToStr(uav.regBase) + ", space" + ToStr(uav.space) + ")";
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }

        if(!entryPoint->cbuffers.empty())
        {
          for(size_t j = 0; j < entryPoint->cbuffers.size(); ++j)
          {
            EntryPointInterface::CBuffer &cbuffer = entryPoint->cbuffers[j];
            if(reflection)
            {
              for(size_t cbIdx = 0; cbIdx < reflection->CBuffers.size(); ++cbIdx)
              {
                const DXBC::CBuffer &cb = reflection->CBuffers[cbIdx];
                if((cb.space == cbuffer.space) && (cb.reg == cbuffer.regBase) &&
                   (cb.bindCount == cbuffer.regCount))
                {
                  if(!cbuffer.cbufferRefl)
                    cbuffer.cbufferRefl = &reflection->CBuffers[cbIdx];
                }
              }
            }
            if(needBlankLine)
              DisassemblyAddNewLine();
            m_Disassembly += "cbuffer " + cbuffer.name;
            if(cbuffer.regCount > 1)
            {
              m_Disassembly += "[";
              if(cbuffer.regCount != ~0U)
                m_Disassembly += ToStr(cbuffer.regCount);
              m_Disassembly += "]";
            }
            m_Disassembly +=
                " : register(b" + ToStr(cbuffer.regBase) + ", space" + ToStr(cbuffer.space) + ")";
            // Ignore cbuffer's which don't have reflection data
            if(cbuffer.cbufferRefl && cbuffer.cbufferRefl->hasReflectionData)
            {
              if(!cbuffer.cbufferRefl->variables.empty())
              {
                DisassemblyAddNewLine();
                m_Disassembly += "{";
                DisassemblyAddNewLine();

                for(const DXBC::CBufferVariable &cbVar : cbuffer.cbufferRefl->variables)
                {
                  const DXBC::CBufferVariableType &cbType = cbVar.type;
                  m_Disassembly += "  ";
                  m_Disassembly += cbType.name;
                  m_Disassembly += " " + cbVar.name;
                  if(cbType.elements > 1)
                    m_Disassembly += "[" + ToStr(cbType.elements) + "]";
                  m_Disassembly += ";";
                  DisassemblyAddNewLine();
                }
                m_Disassembly += "};";
                DisassemblyAddNewLine();
                needBlankLine = true;
              }
            }
            else
            {
              DisassemblyAddNewLine();
            }
          }
          needBlankLine = true;
        }

        if(!entryPoint->samplers.empty())
        {
          for(size_t j = 0; j < entryPoint->samplers.size(); ++j)
          {
            if(needBlankLine)
              DisassemblyAddNewLine();
            EntryPointInterface::Sampler &sampler = entryPoint->samplers[j];
            m_Disassembly += GetSamplerTypeName(sampler.type);
            m_Disassembly += " " + sampler.name;
            if(sampler.regCount > 1)
            {
              m_Disassembly += "[";
              if(sampler.regCount != ~0U)
                m_Disassembly += ToStr(sampler.regCount);
              m_Disassembly += "]";
            }
            m_Disassembly +=
                " : register(s" + ToStr(sampler.regBase) + ", space" + ToStr(sampler.space) + ")";
            m_Disassembly += ";";
            needBlankLine = true;
          }
          DisassemblyAddNewLine();
        }

        if(needBlankLine)
          DisassemblyAddNewLine();
      }

      // Show the compute shader thread group size
      if(reflection && m_Type == DXBC::ShaderType::Compute)
      {
        m_Disassembly += StringFormat::Fmt(
            "[numthreads(%u, %u, %u)]", reflection->DispatchThreadsDimension[0],
            reflection->DispatchThreadsDimension[1], reflection->DispatchThreadsDimension[2]);
        DisassemblyAddNewLine();
      }

      if(func.internalLinkage)
        m_Disassembly += "internal ";
      m_Disassembly += func.type->declFunction(escapeStringIfNeeded(func.name), func.args,
                                               func.attrs, dxcStyleFormatting);

      if(func.comdatIdx < m_Comdats.size())
        m_Disassembly += StringFormat::Fmt(
            " comdat($%s)", escapeStringIfNeeded(m_Comdats[func.comdatIdx].second).c_str());

      if(func.align)
        m_Disassembly += StringFormat::Fmt(" align %u", (1U << func.align) >> 1);

      if(func.attrs && func.attrs->functionSlot)
        m_Disassembly +=
            StringFormat::Fmt(" #%u", m_FuncAttrGroups.indexOf(func.attrs->functionSlot));

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

        if(IsDXCNop(inst))
          continue;

        rdcstr resultTypeStr;
        if(!inst.type->isVoid())
        {
          resultTypeStr += inst.type->toString(dxcStyleFormatting);
          resultTypeStr += " ";
        }
        rdcstr resultIdStr;
        MakeResultId(inst, resultIdStr);

        bool showDxFuncName = false;
        rdcstr commentStr;

        rdcstr lineStr;
        switch(inst.op)
        {
          case Operation::NoOp: lineStr += "nop"; break;
          case Operation::Call:
          {
            rdcstr funcCallName = inst.getFuncCall()->name;
            DXOp dxOpCode = DXOp::NumOpCodes;
            if(funcCallName.beginsWith("dx.op."))
            {
              RDCASSERT(getival<DXOp>(inst.args[0], dxOpCode));
              // Have to use beginsWith to include the function names which a type suffix ie. ".f32"
              RDCASSERT(funcCallName.beginsWith(dxOpFunctionNames[(uint32_t)dxOpCode]));
            }

            switch(dxOpCode)
            {
              case DXOp::LoadInput:
              {
                // LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
                rdcstr name;
                rdcstr rowStr;
                rdcstr componentStr;
                uint32_t inputIdx;
                uint32_t rowIdx;
                bool hasRowIdx = getival<uint32_t>(inst.args[2], rowIdx);
                if(entryPoint && getival<uint32_t>(inst.args[1], inputIdx))
                {
                  EntryPointInterface::Signature &sig = entryPoint->inputs[inputIdx];
                  name = sig.name;
                  if(hasRowIdx)
                  {
                    if(sig.rows > 1)
                      rowStr = "[" + ToStr(rowIdx) + "]";
                  }
                }
                else
                {
                  name = GetArgId(inst, 1);
                  rowStr = "[";
                  if(hasRowIdx)
                    rowStr += ToStr(rowIdx);
                  else
                    rowStr += GetArgId(inst, 2);
                  rowStr += +"]";
                }
                uint32_t componentIdx;
                if(getival<uint32_t>(inst.args[3], componentIdx))
                  componentStr = StringFormat::Fmt("%c", swizzle[componentIdx & 0x3]);
                else
                  componentStr = GetArgId(inst, 3);

                lineStr += DXIL_FAKE_INPUT_STRUCT_NAME + "." + name + rowStr + "." + componentStr;
                break;
              }
              case DXOp::StoreOutput:
              {
                // StoreOutput(outputSigId,rowIndex,colIndex,value)
                rdcstr name;
                rdcstr rowStr;
                rdcstr componentStr;
                uint32_t outputIdx;
                uint32_t rowIdx;
                bool hasRowIdx = getival<uint32_t>(inst.args[2], rowIdx);
                if(entryPoint && getival<uint32_t>(inst.args[1], outputIdx))
                {
                  EntryPointInterface::Signature &sig = entryPoint->outputs[outputIdx];
                  name = sig.name;
                  if(hasRowIdx)
                  {
                    if(sig.rows > 1)
                      rowStr = "[" + ToStr(rowIdx) + "]";
                  }
                }
                else
                {
                  name = GetArgId(inst, 1);
                  rowStr = "[";
                  if(hasRowIdx)
                    rowStr += ToStr(rowIdx);
                  else
                    rowStr += GetArgId(inst, 2);
                  rowStr += +"]";
                }
                uint32_t componentIdx;
                if(getival<uint32_t>(inst.args[3], componentIdx))
                  componentStr = StringFormat::Fmt("%c", swizzle[componentIdx & 0x3]);
                else
                  componentStr = GetArgId(inst, 3);

                lineStr += DXIL_FAKE_OUTPUT_STRUCT_NAME + "." + name + rowStr + "." + componentStr;
                lineStr += " = " + GetArgId(inst, 4);
                break;
              }
              case DXOp::CreateHandle:
              case DXOp::CreateHandleFromBinding:
              {
                // CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
                // CreateHandleFromBinding(bind,index,nonUniformIndex)
                showDxFuncName = false;

                uint32_t resIndexArgId = 3;
                uint32_t nonUniformIndexArgId = 4;
                if(dxOpCode == DXOp::CreateHandleFromBinding)
                {
                  resIndexArgId = 2;
                  nonUniformIndexArgId = 3;
                }

                const ResourceReference *resRef = GetResourceReference(resultIdStr);
                if(resRef)
                {
                  uint32_t index = 0;
                  if(getival<uint32_t>(inst.args[resIndexArgId], index))
                    commentStr += " index = " + ToStr(index);
                }
                else
                {
                  commentStr += " index = " + GetArgId(inst, resIndexArgId);
                }

                lineStr = "InitialiseHandle(";
                lineStr += GetHandleAlias(resultIdStr);

                uint32_t value;
                if(getival<uint32_t>(inst.args[nonUniformIndexArgId], value))
                {
                  if(value != 0)
                    lineStr += ", nonUniformIndex = true";
                }

                lineStr += ")";
                resultIdStr.clear();
                break;
              }
              case DXOp::CreateHandleFromHeap:
              {
                // CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
                uint32_t samplerHeap;
                resultIdStr = GetHandleAlias(resultIdStr);
                if(getival<uint32_t>(inst.args[2], samplerHeap))
                {
                  lineStr += GetHandleAlias(resultIdStr);

                  uint32_t value;
                  if(getival<uint32_t>(inst.args[3], value))
                  {
                    if(value != 0)
                      commentStr += " nonUniformIndex = true";
                  }
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::AnnotateHandle:
              {
                // AnnotateHandle(res,props)
                showDxFuncName = false;

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

                    rdcstr typeStr;

                    bool srv = (resClass == ResourceClass::SRV);

                    ComponentType compType = ComponentType(packedProps[1] & 0xFF);
                    uint8_t compCount = (packedProps[1] & 0xFF00) >> 8;

                    uint8_t feedbackType = packedProps[1] & 0xFF;

                    uint32_t structStride = packedProps[1];

                    switch(resKind)
                    {
                      case ResourceKind::Unknown: typeStr += "Unknown"; break;
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
                          typeStr += "globallycoherent ";
                        if(!srv && rov)
                          typeStr += "ROV";
                        else if(!srv)
                          typeStr += "RW";
                        switch(resKind)
                        {
                          case ResourceKind::Texture1D: typeStr += "Texture1D"; break;
                          case ResourceKind::Texture2D: typeStr += "Texture2D"; break;
                          case ResourceKind::Texture2DMS: typeStr += "Texture2DMS"; break;
                          case ResourceKind::Texture3D: typeStr += "Texture3D"; break;
                          case ResourceKind::TextureCube: typeStr += "TextureCube"; break;
                          case ResourceKind::Texture1DArray: typeStr += "Texture1DArray"; break;
                          case ResourceKind::Texture2DArray: typeStr += "Texture2DArray"; break;
                          case ResourceKind::Texture2DMSArray: typeStr += "Texture2DMSArray"; break;
                          case ResourceKind::TextureCubeArray: typeStr += "TextureCubeArray"; break;
                          case ResourceKind::TypedBuffer: typeStr += "TypedBuffer"; break;
                          default: break;
                        }
                        break;
                      case ResourceKind::RTAccelerationStructure:
                        typeStr += "RTAccelerationStructure";
                        break;
                      case ResourceKind::FeedbackTexture2D: typeStr += "FeedbackTexture2D"; break;
                      case ResourceKind::FeedbackTexture2DArray:
                        typeStr += "FeedbackTexture2DArray";
                        break;
                      case ResourceKind::StructuredBuffer:
                        if(globallyCoherent)
                          typeStr += "globallycoherent ";
                        typeStr += srv ? "StructuredBuffer" : "RWStructuredBuffer";
                        typeStr += StringFormat::Fmt("<stride=%u", structStride);
                        if(sampelCmpOrCounter)
                          typeStr += ", counter";
                        typeStr += ">";
                        break;
                      case ResourceKind::StructuredBufferWithCounter:
                        if(globallyCoherent)
                          typeStr += "globallycoherent ";
                        typeStr +=
                            srv ? "StructuredBufferWithCounter" : "RWStructuredBufferWithCounter";
                        typeStr += StringFormat::Fmt("<stride=%u>", structStride);
                        break;
                      case ResourceKind::RawBuffer:
                        if(globallyCoherent)
                          typeStr += "globallycoherent ";
                        typeStr += srv ? "ByteAddressBuffer" : "RWByteAddressBuffer";
                        break;
                      case ResourceKind::CBuffer:
                        RDCASSERTEQUAL(resClass, ResourceClass::CBuffer);
                        typeStr += "CBuffer";
                        break;
                      case ResourceKind::Sampler:
                        RDCASSERTEQUAL(resClass, ResourceClass::Sampler);
                        typeStr += "SamplerState";
                        break;
                      case ResourceKind::TBuffer:
                        RDCASSERTEQUAL(resClass, ResourceClass::SRV);
                        typeStr += "TBuffer";
                        break;
                      case ResourceKind::SamplerComparison:
                        RDCASSERTEQUAL(resClass, ResourceClass::Sampler);
                        typeStr += "SamplerComparisonState";
                        break;
                    }

                    if(resKind == ResourceKind::FeedbackTexture2D ||
                       resKind == ResourceKind::FeedbackTexture2DArray)
                    {
                      if(feedbackType == 0)
                        typeStr += "<MinMip>";
                      else if(feedbackType == 1)
                        typeStr += "<MipRegionUsed>";
                      else
                        typeStr += "<Invalid>";
                    }
                    else if(resKind == ResourceKind::Texture1D ||
                            resKind == ResourceKind::Texture2D || resKind == ResourceKind::Texture3D ||
                            resKind == ResourceKind::TextureCube ||
                            resKind == ResourceKind::Texture1DArray ||
                            resKind == ResourceKind::Texture2DArray ||
                            resKind == ResourceKind::TextureCubeArray ||
                            resKind == ResourceKind::TypedBuffer ||
                            resKind == ResourceKind::Texture2DMS ||
                            resKind == ResourceKind::Texture2DMSArray)
                    {
                      VarType varType = VarTypeForComponentType(compType);
                      typeStr += "<";
                      typeStr += ToStr(varType);
                      if(compCount > 1)
                        typeStr += StringFormat::Fmt("%d", compCount);
                      typeStr += ">";
                    }
                    lineStr += "(";
                    lineStr += typeStr;
                    lineStr += ")";
                    rdcstr ssaStr = GetArgId(inst, 1);
                    lineStr += GetHandleAlias(ssaStr);
                    resultIdStr = GetHandleAlias(resultIdStr);
                  }
                  else
                  {
                    showDxFuncName = true;
                  }
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::CBufferLoad:
              case DXOp::CBufferLoadLegacy:
              {
                // CBufferLoad(handle,byteOffset,alignment)
                // CBufferLoadLegacy(handle,regIndex)
                rdcstr handleStr = GetArgId(inst, 1);
                const ResourceReference *resRef = GetResourceReference(handleStr);
                bool useFallback = true;
                if(entryPoint && resRef)
                {
                  uint32_t regIndex;
                  if(getival<uint32_t>(inst.args[2], regIndex))
                  {
                    useFallback = false;
                    if(dxOpCode == DXOp::CBufferLoad)
                    {
                      // TODO: handle non 16-byte aligned offsets
                      // Convert byte offset to a register index
                      regIndex = regIndex / 16;
                      // uint32_t alignment = getival<uint32_t>(inst.args[3]);
                    }
                    const EntryPointInterface::CBuffer &cbuffer =
                        entryPoint->cbuffers[resRef->resourceIndex];
                    if(cbuffer.cbufferRefl && cbuffer.cbufferRefl->hasReflectionData)
                    {
                      const Type *retType = inst.type;
                      uint32_t bytesPerElement = 4;
                      if(retType)
                      {
                        RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
                        const Type *baseType = retType->members[0];
                        RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
                        bytesPerElement = baseType->bitWidth / 8;
                      }
                      lineStr +=
                          MakeCBufferRegisterStr(regIndex, bytesPerElement, cbuffer, handleStr);
                      commentStr += " cbuffer = " + cbuffer.name;
                      commentStr += ", byte_offset = " + ToStr(regIndex * 16);
                    }
                    else
                    {
                      lineStr += handleStr;
                      lineStr += ".Load4(";
                      lineStr += "byte_offset = " + ToStr(regIndex * 16);
                      lineStr += ")";
                    }
                  }
                }
                if(useFallback)
                {
                  lineStr += GetHandleAlias(handleStr);
                  lineStr += ".Load4(";
                  lineStr += "byte_offset = ";
                  uint32_t regIndex;
                  if(getival<uint32_t>(inst.args[2], regIndex))
                  {
                    lineStr += ToStr(regIndex * 16);
                  }
                  else
                  {
                    lineStr += GetArgId(inst, 2);
                    if(dxOpCode == DXOp::CBufferLoadLegacy)
                      lineStr += " * 16";
                  }
                  lineStr += ")";
                }
                break;
              }
              case DXOp::BufferLoad:
              {
                // BufferLoad(srv,index,wot)
                // wot is unused
                rdcstr handleStr = GetArgId(inst, 1);
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty())
                {
                  lineStr += resName;
                  lineStr += "[" + GetArgId(inst, 2) + "]";
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::RawBufferLoad:
              {
                // RawBufferLoad(srv,index,elementOffset,mask,alignment)
                rdcstr handleStr = GetArgId(inst, 1);
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty())
                {
                  if(!isUndef(inst.args[2]))
                  {
                    rdcstr arrayStr = resName + "[" + GetArgId(inst, 2) + "]";
                    if(!isUndef(inst.args[3]))
                    {
                      // *(&<resName>[<index>] + <elementOffset> bytes)
                      uint32_t elementOffset;
                      if(getival<uint32_t>(inst.args[3], elementOffset))
                      {
                        if(elementOffset > 0)
                          lineStr += "*(&" + arrayStr + " + " + ToStr(elementOffset) + " bytes)";
                        else
                          lineStr += arrayStr;
                      }
                      else
                      {
                        lineStr += "*(&" + arrayStr + " + " + GetArgId(inst, 3) + " bytes)";
                      }
                    }
                    else
                    {
                      lineStr += arrayStr;
                    }
                  }
                  else
                  {
                    lineStr += resName + "[" + GetArgId(inst, 3) + "]";
                  }
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::BufferStore:
              case DXOp::RawBufferStore:
              {
                // BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
                // RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
                rdcstr handleStr = GetArgId(inst, 1);
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty())
                {
                  uint32_t offset = 0;
                  bool validElementOffset = !isUndef(inst.args[3]);
                  bool constantElementOffset = validElementOffset && getival(inst.args[3], offset);

                  rdcstr arrayStr = resName;
                  uint32_t index;
                  if(getival(inst.args[2], index))
                  {
                    arrayStr += "[" + ToStr(index) + "]";
                  }
                  else
                  {
                    arrayStr += "[" + GetArgId(inst, 2) + "]";
                  }
                  if(validElementOffset)
                  {
                    // *(&<resName>[<index>] + <elementOffset> bytes)
                    if(constantElementOffset)
                    {
                      if(offset > 0)
                        lineStr = "*(&" + arrayStr + " + " + ToStr(offset) + " bytes)";
                      else
                        lineStr += arrayStr;
                    }
                    else
                    {
                      lineStr += "*(&" + arrayStr + " + " + GetArgId(inst, 3) + " bytes)";
                    }
                  }
                  else
                  {
                    lineStr += arrayStr;
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
                      lineStr += GetArgId(inst, a);
                      needComma = true;
                    }
                  }
                  lineStr += "}";
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::TextureLoad:
              {
                // TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
                rdcstr handleStr = GetArgId(inst, 1);
                const ResourceReference *resRef = GetResourceReference(handleStr);
                uint32_t sampleCount = 0;
                if(entryPoint && resRef)
                {
                  uint32_t resourceIndex = resRef->resourceIndex;
                  const EntryPointInterface::SRV *texture = resourceIndex < entryPoint->srvs.size()
                                                                ? &entryPoint->srvs[resourceIndex]
                                                                : NULL;
                  if(texture)
                    sampleCount = texture->sampleCount;
                }
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty())
                {
                  lineStr += resName;
                  lineStr += ".Load(";
                  bool needComma = false;
                  for(uint32_t a = 3; a < 6; ++a)
                  {
                    if(!isUndef(inst.args[a]))
                    {
                      if(needComma)
                        lineStr += ", ";
                      lineStr += GetArgId(inst, a);
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
                      if(sampleCount > 1)
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
                      lineStr += GetArgId(inst, 2);
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
                      lineStr += GetArgId(inst, a);
                    }
                  }
                  lineStr += ")";
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::TextureStore:
              {
                // TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
                rdcstr handleStr = GetArgId(inst, 1);
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty())
                {
                  lineStr += resName;
                  lineStr += "[";
                  bool needComma = false;
                  for(uint32_t a = 2; a < 5; ++a)
                  {
                    if(!isUndef(inst.args[a]))
                    {
                      if(needComma)
                        lineStr += ", ";
                      lineStr += GetArgId(inst, a);
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
                      lineStr += GetArgId(inst, a);
                      needComma = true;
                    }
                  }
                  lineStr += "}";
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::Sample:
              case DXOp::SampleBias:
              case DXOp::SampleLevel:
              case DXOp::SampleGrad:
              case DXOp::SampleCmp:
              case DXOp::SampleCmpLevelZero:
              case DXOp::SampleCmpLevel:
              case DXOp::SampleCmpGrad:
              case DXOp::SampleCmpBias:
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
                rdcstr handleStr = GetArgId(inst, 1);
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty())
                {
                  lineStr += resName;
                  lineStr += ".";
                  rdcstr dxFuncSig = funcNameSigs[(uint32_t)dxOpCode];
                  int paramStart = dxFuncSig.find('(') + 1;
                  if(paramStart > 0)
                    lineStr += dxFuncSig.substr(0, paramStart);
                  else
                    lineStr += "UNKNOWN DX FUNCTION";

                  // sampler is 2
                  rdcstr samplerStr = GetArgId(inst, 2);
                  samplerStr = GetHandleAlias(samplerStr);
                  lineStr += samplerStr;

                  for(uint32_t a = 3; a < 7; ++a)
                  {
                    if(!isUndef(inst.args[a]))
                    {
                      lineStr += ", ";
                      lineStr += GetArgId(inst, a);
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
                      lineStr += GetArgId(inst, a);
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
                      lineStr += GetArgId(inst, a);
                    }
                  }
                  lineStr += ")";
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::AtomicBinOp:
              {
                // AtomicBinOp(handle, atomicOp, offset0, offset1, offset2, newValue)
                rdcstr handleStr = GetArgId(inst, 1);
                AtomicBinOpCode atomicBinOpCode;
                rdcstr resName = GetHandleAlias(handleStr);
                if(!resName.isEmpty() && getival<AtomicBinOpCode>(inst.args[2], atomicBinOpCode))
                {
                  lineStr += resName;
                  lineStr += ".";
                  lineStr += "Interlocked";
                  lineStr += ToStr(atomicBinOpCode);
                  lineStr += "(";
                  lineStr += "{";
                  bool needComma = false;
                  for(uint32_t a = 3; a < 6; ++a)
                  {
                    if(!isUndef(inst.args[a]))
                    {
                      if(needComma)
                        lineStr += ", ";
                      lineStr += GetArgId(inst, a);
                      needComma = true;
                    }
                  }
                  lineStr += "}";
                  if(!isUndef(inst.args[6]))
                  {
                    lineStr += ", ";
                    lineStr += GetArgId(inst, 6);
                  }
                  lineStr += ")";
                }
                else
                {
                  showDxFuncName = true;
                }
                break;
              }
              case DXOp::Dot2:
              case DXOp::Dot3:
              case DXOp::Dot4:
              {
                // Dot4(ax,ay,az,aw,bx,by,bz,bw)
                // Dot3(ax,ay,az,bx,by,bz)
                // Dot2(ax,ay,bx,by)
                uint32_t countComponents = 0;
                if(dxOpCode == DXOp::Dot4)
                  countComponents = 4;
                else if(dxOpCode == DXOp::Dot3)
                  countComponents = 3;
                else if(dxOpCode == DXOp::Dot2)
                  countComponents = 2;

                lineStr += "dot(";
                lineStr += "{";
                bool needComma = false;
                uint32_t aVecStart = 1;
                uint32_t aVecEnd = 1 + countComponents;
                for(uint32_t a = aVecStart; a < aVecEnd; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    if(needComma)
                      lineStr += ", ";
                    lineStr += GetArgId(inst, a);
                    needComma = true;
                  }
                }
                lineStr += "}";
                needComma = false;
                uint32_t bVecStart = aVecEnd;
                uint32_t bVecEnd = bVecStart + countComponents;
                lineStr += ", {";
                for(uint32_t a = bVecStart; a < bVecEnd; ++a)
                {
                  if(!isUndef(inst.args[a]))
                  {
                    if(needComma)
                      lineStr += ", ";
                    lineStr += GetArgId(inst, a);
                    needComma = true;
                  }
                }
                lineStr += "}";
                lineStr += ")";
                break;
              }
              case DXOp::NumOpCodes: showDxFuncName = false; break;
              default: showDxFuncName = true; break;
            }
            if(dxOpCode != DXOp::NumOpCodes)
            {
              if(showDxFuncName)
              {
                rdcstr dxFuncSig;
                int paramStart = -1;
                uint32_t opcode = (uint32_t)dxOpCode;
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
                bool first = true;
                int paramStrCount = (int)dxFuncSig.size();
                for(uint32_t a = 1; a < inst.args.size(); ++a)
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
                    rdcstr ssaStr = GetArgId(inst, a);
                    lineStr += GetHandleAlias(ssaStr);
                    first = false;
                  }
                }
                lineStr += ")";
              }
            }
            else if(funcCallName.beginsWith("llvm.dbg."))
            {
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
              case Operation::SToF:
                lineStr += "(" + inst.type->toString(dxcStyleFormatting) + ")";
                break;
              case Operation::IToPtr: lineStr += "(void *)"; break;
              case Operation::AddrSpaceCast: lineStr += "addrspacecast"; break;
              default: break;
            }
            switch(inst.op)
            {
              case Operation::Trunc: commentStr += "truncate ";
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
            lineStr += GetArgId(inst, 0);
            lineStr += ")";
            break;
          }
          case Operation::ExtractVal:
          {
            lineStr += "extractvalue ";
            lineStr += GetArgId(inst, 0);
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
              lineStr += " " + GetArgId(inst, 0);
            break;
          }
          case Operation::Unreachable: lineStr += "unreachable"; break;
          case Operation::Alloca:
          {
            lineStr += "alloca ";
            lineStr += inst.type->inner->toString(dxcStyleFormatting);
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
              rdcstr typeStr = inst.type->toString(dxcStyleFormatting);
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
                    case DXIL::Type::PointerAddrSpace::Default: resultTypeStr = ""; break;
                    case DXIL::Type::PointerAddrSpace::DeviceMemory:
                      resultTypeStr = "DeviceMemory";
                      break;
                    case DXIL::Type::PointerAddrSpace::CBuffer: resultTypeStr = "CBuffer"; break;
                    case DXIL::Type::PointerAddrSpace::GroupShared:
                      resultTypeStr = "GroupShared";
                      break;
                    case DXIL::Type::PointerAddrSpace::GenericPointer: resultTypeStr = ""; break;
                    case DXIL::Type::PointerAddrSpace::ImmediateCBuffer:
                      resultTypeStr = "ImmediateCBuffer";
                      break;
                  };

                  resultTypeStr += " ";
                  resultTypeStr += scalarType;
                  resultTypeStr += "* ";

                  // arg[0] : ptr
                  rdcstr ptrStr = GetArgId(inst, 0);
                  // Simple demangle take string between first "?" and next "@"
                  int nameStart = ptrStr.indexOf('?');
                  if(nameStart > 0)
                  {
                    nameStart++;
                    int nameEnd = ptrStr.indexOf('@', nameStart);
                    if(nameEnd > nameStart)
                      ptrStr = ptrStr.substr(nameStart, nameEnd - nameStart);
                  }
                  lineStr += ptrStr;
                  // arg[1] : index 0
                  bool first = true;
                  if(inst.args.size() > 1)
                  {
                    uint32_t v = 0;
                    if(!getival<uint32_t>(inst.args[1], v) || (v > 0))
                    {
                      lineStr += "[";
                      lineStr += GetArgId(inst, 1);
                      lineStr += "]";
                      first = false;
                    }
                  }

                  // arg[2..] : index 1...N
                  for(uint32_t a = 2; a < inst.args.size(); ++a)
                  {
                    if(first)
                      lineStr += "[";
                    else
                      lineStr += " + ";

                    lineStr += GetArgId(inst, a);

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
            lineStr += GetArgId(inst, 0);
            lineStr += " = ";
            lineStr += GetArgId(inst, 1);
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
              case Operation::FOrdEqual: opStr = " == "; break;
              case Operation::FOrdGreater: opStr = " > "; break;
              case Operation::FOrdGreaterEqual: opStr = " >= "; break;
              case Operation::FOrdLess: opStr = " < "; break;
              case Operation::FOrdLessEqual: opStr = " <= "; break;
              case Operation::FOrdNotEqual: opStr = " != "; break;
              case Operation::FUnordEqual: opStr = " == ";
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
            lineStr += GetArgId(inst, 0);
            lineStr += opStr;
            lineStr += GetArgId(inst, 1);
            lineStr += ")";
            break;
          }
          case Operation::FOrd:
          {
            // ord: yields true if both operands are not a QNAN.
            lineStr += "!isqnan(";
            lineStr += GetArgId(inst, 0);
            lineStr += ")";
            lineStr += " && ";
            lineStr += "!isqnan(";
            lineStr += GetArgId(inst, 1);
            lineStr += ")";
            break;
          }
          case Operation::FUnord:
          {
            // uno: yields true if either operand is a QNAN.
            lineStr += "isqnan(";
            lineStr += GetArgId(inst, 0);
            lineStr += ")";
            lineStr += " || ";
            lineStr += "isqnan(";
            lineStr += GetArgId(inst, 1);
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
              case Operation::IEqual: opStr += " == "; break;
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
              case Operation::SLessEqual: commentStr += "signed ";
              default: break;
            }
            lineStr += "(";
            lineStr += GetArgId(inst, 0);
            lineStr += opStr;
            lineStr += GetArgId(inst, 1);
            lineStr += ")";
            break;
          }
          case Operation::Select:
          {
            lineStr += GetArgId(inst, 2);
            lineStr += " ? ";
            lineStr += GetArgId(inst, 0);
            lineStr += " : ";
            lineStr += GetArgId(inst, 1);
            break;
          }
          case Operation::ExtractElement:
          {
            lineStr += "extractelement ";
            lineStr += GetArgId(inst, 0);
            lineStr += ", ";
            lineStr += GetArgId(inst, 1);
            break;
          }
          case Operation::InsertElement:
          {
            lineStr += "insertelement ";
            lineStr += GetArgId(inst, 0);
            lineStr += ", ";
            lineStr += GetArgId(inst, 1);
            lineStr += ", ";
            lineStr += GetArgId(inst, 2);
            break;
          }
          case Operation::ShuffleVector:
          {
            lineStr += "shufflevector ";
            lineStr += GetArgId(inst, 0);
            lineStr += ", ";
            lineStr += GetArgId(inst, 1);
            lineStr += ", ";
            lineStr += GetArgId(inst, 2);
            break;
          }
          case Operation::InsertValue:
          {
            lineStr += "insertvalue ";
            lineStr += GetArgId(inst, 0);
            lineStr += ", ";
            lineStr += GetArgId(inst, 1);
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
              lineStr += GetArgId(inst, 2);
              lineStr += ") goto ";
              lineStr += StringFormat::Fmt("%s", GetArgId(inst, 0).c_str());
              lineStr += "; else goto ";
              lineStr += StringFormat::Fmt("%s", GetArgId(inst, 1).c_str());
            }
            else
            {
              lineStr += "goto ";
              lineStr += GetArgId(inst, 0);
            }
            break;
          }
          case Operation::Phi:
          {
            lineStr += "phi ";
            lineStr += inst.type->toString(dxcStyleFormatting);
            for(uint32_t a = 0; a < inst.args.size(); a += 2)
            {
              if(a == 0)
                lineStr += " ";
              else
                lineStr += ", ";
              lineStr += StringFormat::Fmt("[ %s, %s ]", GetArgId(inst, a).c_str(),
                                           GetArgId(inst, a + 1).c_str());
            }
            break;
          }
          case Operation::Switch:
          {
            lineStr += "switch ";
            lineStr += GetArgId(inst, 0);
            lineStr += ", ";
            lineStr += GetArgId(inst, 1);
            lineStr += " [";
            lineStr += "\n";
            m_DisassemblyInstructionLine++;
            for(uint32_t a = 2; a < inst.args.size(); a += 2)
            {
              lineStr += StringFormat::Fmt("    %s, %s", GetArgId(inst, a).c_str(),
                                           GetArgId(inst, a + 1).c_str());
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
        if(!resultIdStr.empty())
          lineStr = resultTypeStr + resultIdStr + " = " + lineStr;

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
            labelName = StringFormat::Fmt("%clabel%u: ", dxilIdentifier, func.blocks[curBlock]->slot);
          else
            labelName = StringFormat::Fmt("%clabel_%s%u: ", dxilIdentifier,
                                          DXBC::BasicDemangle(func.blocks[curBlock]->name).c_str(),
                                          func.blocks[curBlock]->id);

          labelName.reserve(30);
          while(labelName.size() < 30)
            labelName.push_back(' ');

          rdcstr predicates;
          bool first = true;
          for(const Block *pred : func.blocks[curBlock]->preds)
          {
            if(pred->name.empty())
            {
              // Ignore predicate 0
              if(pred->slot > 0)
              {
                if(!first)
                  predicates += ", ";
                first = false;
                predicates += StringFormat::Fmt("%clabel%u", dxilIdentifier, pred->slot);
              }
            }
            else
            {
              if(!first)
                predicates += ", ";
              first = false;
              predicates += StringFormat::Fmt("%clabel_%s%u", dxilIdentifier,
                                              DXBC::BasicDemangle(pred->name).c_str(), pred->id);
            }
          }
          if(!predicates.empty())
            labelName += "// preceded by " + predicates;

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

void Program::ParseReferences(const DXBC::Reflection *reflection)
{
  for(size_t i = 0; i < m_Functions.size(); i++)
  {
    const Function &func = *m_Functions[i];

    m_Accum.processFunction(m_Functions[i]);

    if(func.external)
      continue;

    EntryPointInterface *entryPoint = NULL;
    for(size_t e = 0; e < m_EntryPointInterfaces.size(); ++e)
    {
      if(func.name == m_EntryPointInterfaces[e].name)
      {
        entryPoint = &m_EntryPointInterfaces[e];
        break;
      }
    }
    // Ignore functions which are not an entry point
    if(!entryPoint)
      continue;

    // Use resource names from the reflection data if the resource name data is empty
    if(reflection)
    {
      for(EntryPointInterface::CBuffer &cbuffer : entryPoint->cbuffers)
      {
        if(cbuffer.name.empty())
        {
          for(DXBC::CBuffer reflCB : reflection->CBuffers)
          {
            if((reflCB.space == cbuffer.space) && (reflCB.reg == cbuffer.regBase) &&
               (reflCB.bindCount == cbuffer.regCount))
              cbuffer.name = reflCB.name;
          }
        }
      }

      for(EntryPointInterface::SRV &srv : entryPoint->srvs)
      {
        if(srv.name.empty())
        {
          for(DXBC::ShaderInputBind bind : reflection->SRVs)
          {
            if((bind.space == srv.space) && (bind.reg == srv.regBase) &&
               (bind.bindCount == srv.regCount))
              srv.name = bind.name;
          }
        }
      }

      for(EntryPointInterface::UAV &uav : entryPoint->uavs)
      {
        if(uav.name.empty())
        {
          for(DXBC::ShaderInputBind bind : reflection->UAVs)
          {
            if((bind.space == uav.space) && (bind.reg == uav.regBase) &&
               (bind.bindCount == uav.regCount))
              uav.name = bind.name;
          }
        }
      }

      for(size_t j = 0; j < entryPoint->samplers.size(); ++j)
      {
        EntryPointInterface::Sampler &sampler = entryPoint->samplers[j];
        if(sampler.name.empty())
        {
          for(DXBC::ShaderInputBind bind : reflection->Samplers)
          {
            if((bind.space == sampler.space) && (bind.reg == sampler.regBase) &&
               (bind.bindCount == sampler.regCount))
              sampler.name = bind.name;
          }
        }
      }
    }
    // Find all the resource handle references and store them in
    // rdcarray<ResourceReference> m_ResourceReferences;
    for(size_t funcIdx = 0; funcIdx < func.instructions.size(); funcIdx++)
    {
      Instruction &inst = *func.instructions[funcIdx];
      rdcstr resultIdStr;
      MakeResultId(inst, resultIdStr);

      switch(inst.op)
      {
        case Operation::Call:
        {
          rdcstr funcCallName = inst.getFuncCall()->name;
          if(funcCallName.beginsWith("dx.op."))
          {
            DXOp dxOpCode;
            RDCASSERT(getival<DXOp>(inst.args[0], dxOpCode));
            switch(dxOpCode)
            {
              case DXOp::CreateHandle:
              case DXOp::CreateHandleFromBinding:
              {
                // CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
                // CreateHandleFromBinding(bind,index,nonUniformIndex)
                uint32_t resIndexArgId = 3;
                uint32_t nonUniformIndexArgId = 4;
                bool validBinding = false;
                EntryPointInterface::ResourceBase *resourceBase = NULL;
                rdcstr resName;
                uint32_t resIndex = 0;
                if(dxOpCode == DXOp::CreateHandle)
                {
                  ResourceClass resClass;
                  validBinding = getival<ResourceClass>(inst.args[1], resClass) &&
                                 getival<uint32_t>(inst.args[2], resIndex);
                  if(validBinding)
                  {
                    switch(resClass)
                    {
                      case ResourceClass::SRV: resourceBase = &entryPoint->srvs[resIndex]; break;
                      case ResourceClass::UAV: resourceBase = &entryPoint->uavs[resIndex]; break;
                      case ResourceClass::CBuffer:
                        resourceBase = &entryPoint->cbuffers[resIndex];
                        break;
                      case ResourceClass::Sampler:
                        resourceBase = &entryPoint->samplers[resIndex];
                        break;
                      default: break;
                    };
                  }
                  else
                  {
                    resName = "ResourceClass:" + GetArgId(inst, 1);
                    resName += "[" + GetArgId(inst, 2) + "]";
                    resName += "[" + GetArgId(inst, resIndexArgId) + "]";
                  }
                }
                else
                {
                  // bind is a structure
                  // struct Bind
                  // {
                  //   int32_t rangeLowerBound;
                  //   int32_t rangeUpperBound;
                  //   int32_t spaceID;
                  //   int8_t resourceClass;
                  // };
                  resIndexArgId = 2;
                  nonUniformIndexArgId = 3;
                  if(const Constant *bind = cast<Constant>(inst.args[1]))
                  {
                    uint32_t lowerBound = ~0U;
                    uint32_t upperBound = ~0U;
                    uint32_t spaceID = ~0U;
                    ResourceClass resClass = ResourceClass::Invalid;
                    if(bind->isCompound())
                    {
                      if(bind->getMembers().size() >= 4)
                      {
                        const rdcarray<Value *> &members = bind->getMembers();
                        validBinding = getival<uint32_t>(members[0], lowerBound);
                        validBinding &= getival<uint32_t>(members[1], upperBound);
                        validBinding &= getival<uint32_t>(members[2], spaceID);
                        validBinding &= getival<ResourceClass>(members[3], resClass);
                      }
                    }
                    else if(bind->isNULL())
                    {
                      lowerBound = 0;
                      upperBound = 0;
                      spaceID = 0;
                      resClass = (ResourceClass)0;
                      validBinding = true;
                    }

                    // Search through the resources to find the binding
                    if(validBinding)
                    {
                      switch(resClass)
                      {
                        case ResourceClass::SRV:
                        {
                          for(uint32_t r = 0; r < entryPoint->srvs.size(); ++r)
                          {
                            EntryPointInterface::ResourceBase *res = &entryPoint->srvs[r];
                            if(res->MatchesBinding(lowerBound, upperBound, spaceID))
                            {
                              resIndex = r;
                              resourceBase = res;
                              break;
                            }
                          }
                          break;
                        }
                        case ResourceClass::UAV:
                        {
                          for(uint32_t r = 0; r < entryPoint->uavs.size(); ++r)
                          {
                            EntryPointInterface::ResourceBase *res = &entryPoint->uavs[r];
                            if(res->MatchesBinding(lowerBound, upperBound, spaceID))
                            {
                              resIndex = r;
                              resourceBase = res;
                              break;
                            }
                          }
                          break;
                        }
                        case ResourceClass::CBuffer:
                        {
                          for(uint32_t r = 0; r < entryPoint->cbuffers.size(); ++r)
                          {
                            EntryPointInterface::ResourceBase *res = &entryPoint->cbuffers[r];
                            if(res->MatchesBinding(lowerBound, upperBound, spaceID))
                            {
                              resIndex = r;
                              resourceBase = res;
                              break;
                            }
                          }
                          break;
                        }
                        case ResourceClass::Sampler:
                        {
                          for(uint32_t r = 0; r < entryPoint->samplers.size(); ++r)
                          {
                            EntryPointInterface::ResourceBase *res = &entryPoint->samplers[r];
                            if(res->MatchesBinding(lowerBound, upperBound, spaceID))
                            {
                              resIndex = r;
                              resourceBase = res;
                              break;
                            }
                          }
                          break;
                          default: break;
                        }
                      }
                    }
                    if(!resourceBase)
                    {
                      resName = "ResourceClass:" + GetArgId(inst, 1);
                      resName += "[" + GetArgId(inst, 2) + "]";
                      resName += "[" + GetArgId(inst, resIndexArgId) + "]";
                    }
                  }
                }

                if(resourceBase)
                {
                  RDCASSERT(!GetResourceReference(resultIdStr));
                  ResourceReference resRef(resultIdStr, *resourceBase, resIndex);
                  m_ResourceHandles[resultIdStr] = m_ResourceHandles.size();
                  m_ResourceReferences.push_back(resRef);
                  resName = resourceBase->name;
                  uint32_t index = 0;
                  if(getival<uint32_t>(inst.args[resIndexArgId], index))
                  {
                    if(index != resIndex)
                    {
                      if(resourceBase->regCount > 1)
                        resName += StringFormat::Fmt("[%u]", index);
                    }
                  }
                  else
                  {
                    if(resourceBase->regCount > 1)
                      resName += "[" + GetArgId(inst, resIndexArgId) + "]";
                  }
                }
                if(!resName.isEmpty())
                  m_SsaAliases[resultIdStr] = resName;
                break;
              }
              case DXOp::CreateHandleFromHeap:
              {
                // CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
                rdcstr resBaseName = "untyped_descriptor";
                uint32_t annotateHandleCount = m_ResourceAnnotateCounts[resBaseName];
                rdcstr resultAlias = "__" + resBaseName + "_" + ToStr(annotateHandleCount);
                m_ResourceAnnotateCounts[resBaseName]++;
                m_SsaAliases[resultIdStr] = resultAlias;

                uint32_t samplerHeap;
                if(getival<uint32_t>(inst.args[2], samplerHeap))
                {
                  rdcstr resName =
                      (samplerHeap == 0) ? "ResourceDescriptorHeap" : "SamplerDescriptorHeap";

                  resName += "[";
                  resName += GetArgId(inst, 1);
                  resName += "]";
                  m_SsaAliases[resultAlias] = resName;
                }
                break;
              }
              case DXOp::AnnotateHandle:
              {
                // AnnotateHandle(res,props)

                // If the underlying handle points to a known resource then duplicate the resource
                // and register it as resultIdStr
                rdcstr baseResource = GetArgId(inst, 1);
                const ResourceReference *resRef = GetResourceReference(baseResource);
                rdcstr resBaseName = "typed_descriptor";
                if(resRef)
                {
                  resBaseName = resRef->resourceBase.name;
                  m_ResourceHandles[resultIdStr] = m_ResourceHandles.size();
                  m_ResourceReferences.push_back(*resRef);
                }
                uint32_t annotateHandleCount = m_ResourceAnnotateCounts[resBaseName];
                rdcstr resName = "__" + resBaseName + "_" + ToStr(annotateHandleCount);
                m_SsaAliases[resultIdStr] = resName;
                m_ResourceAnnotateCounts[resBaseName]++;

                break;
              }
              default: break;
            }
          }
        }
        default: break;
      }
    }
  }
}

rdcstr Type::toString(bool dxcStyleFormatting) const
{
  const char dxilIdentifier = Program::GetDXILIdentifier(dxcStyleFormatting);
  if(!name.empty())
  {
    return StringFormat::Fmt("%c%s", dxilIdentifier, escapeStringIfNeeded(name).c_str());
  }

  switch(type)
  {
    case Scalar:
    {
      switch(scalarType)
      {
        case Void: return "void";
        case Int:
          if(dxcStyleFormatting)
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
      if(dxcStyleFormatting)
        return StringFormat::Fmt("<%u x %s>", elemCount, inner->toString(dxcStyleFormatting).c_str());
      else
        return StringFormat::Fmt("%s%u", inner->toString(dxcStyleFormatting).c_str(), elemCount);
    }
    case Pointer:
    {
      if(inner->type == Type::Function)
      {
        if(addrSpace == Type::PointerAddrSpace::Default)
          return inner->toString(dxcStyleFormatting);
        else
          return StringFormat::Fmt("%s addrspace(%d)", inner->toString(dxcStyleFormatting).c_str(),
                                   addrSpace);
      }
      if(addrSpace == Type::PointerAddrSpace::Default)
        return StringFormat::Fmt("%s*", inner->toString(dxcStyleFormatting).c_str());
      else
        return StringFormat::Fmt("%s addrspace(%d)*", inner->toString(dxcStyleFormatting).c_str(),
                                 addrSpace);
    }
    case Array:
    {
      if(dxcStyleFormatting)
        return StringFormat::Fmt("[%u x %s]", elemCount, inner->toString(dxcStyleFormatting).c_str());
      else
        return StringFormat::Fmt("%s[%u]", inner->toString(dxcStyleFormatting).c_str(), elemCount);
    }
    case Function: return declFunction(rdcstr(), {}, NULL, dxcStyleFormatting) + "*";
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
        ret += members[i]->toString(dxcStyleFormatting);
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
                          const AttributeSet *attrs, bool dxcStyleFormatting) const
{
  rdcstr ret = inner->toString(dxcStyleFormatting);
  ret += " " + funcName + "(";
  for(size_t i = 0; i < members.size(); i++)
  {
    if(i > 0)
      ret += ", ";
    ret += members[i]->toString(dxcStyleFormatting);

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

rdcstr Metadata::refString(bool dxcStyleFormatting) const
{
  if(slot == ~0U)
    return valString(dxcStyleFormatting);
  return StringFormat::Fmt("!%u", slot);
}

rdcstr DebugLocation::toString(bool dxcStyleFormatting) const
{
  rdcstr ret = StringFormat::Fmt("!DILocation(line: %llu", line);
  if(col)
    ret += StringFormat::Fmt(", column: %llu", col);
  ret += StringFormat::Fmt(", scope: %s",
                           scope ? scope->refString(dxcStyleFormatting).c_str() : "null");
  if(inlinedAt)
    ret += StringFormat::Fmt(", inlinedAt: %s", inlinedAt->refString(dxcStyleFormatting).c_str());
  ret += ")";
  return ret;
}

rdcstr Metadata::valString(bool dxcStyleFormatting) const
{
  const char dxilIdentifier = Program::GetDXILIdentifier(dxcStyleFormatting);
  if(dwarf)
  {
    return dwarf->toString(dxcStyleFormatting);
  }
  else if(debugLoc)
  {
    return debugLoc->toString(dxcStyleFormatting);
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
          return StringFormat::Fmt("%s %c%u", i->type->toString(dxcStyleFormatting).c_str(),
                                   dxilIdentifier, i->slot);
        else
          return StringFormat::Fmt("%s %c%s", i->type->toString(dxcStyleFormatting).c_str(),
                                   dxilIdentifier, escapeStringIfNeeded(i->getName()).c_str());
      }
      else if(value)
      {
        return value->toString(dxcStyleFormatting, true);
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
        ret += children[i]->valString(dxcStyleFormatting);
      else
        ret += StringFormat::Fmt("!%u", children[i]->slot);
    }
    ret += "}";

    return ret;
  }
}

static void floatAppendToString(const Type *t, const ShaderValue &val, uint32_t i, rdcstr &ret,
                                bool dxcStyleFormatting)
{
  if(dxcStyleFormatting)
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
    if(dxcStyleFormatting)
    {
      // check we can reparse precisely a float-formatted string. Otherwise we print as hex
      rdcstr flt = StringFormat::Fmt("%.6le", d);

#if ENABLED(DXC_COMPATIBLE_DISASM)
      // dxc/llvm only prints floats as floats if they roundtrip, but our disassembly doesn't
      // need to roundtrip so it's better to display the value in all cases
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

void shaderValAppendToString(const Type *type, const ShaderValue &val, uint32_t i, rdcstr &ret,
                             bool dxcStyleFormatting)
{
  if(type->scalarType == Type::Float)
  {
    floatAppendToString(type, val, i, ret, dxcStyleFormatting);
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

rdcstr Value::toString(bool dxcStyleFormatting, bool withType) const
{
  rdcstr ret;
  if(withType)
  {
    if(type)
      ret += type->toString(dxcStyleFormatting) + " ";
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
    case ValueKind::Constant:
      return cast<Constant>(this)->toString(dxcStyleFormatting, withType);
      break;
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

rdcstr Constant::toString(bool dxcStyleFormatting, bool withType) const
{
  if(type == NULL)
    return escapeString(str);

  rdcstr ret;
  if(withType)
    ret += type->toString(dxcStyleFormatting) + " ";
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
        RDCASSERTEQUAL(baseType->type, Type::Pointer);
        ret += baseType->inner->toString(dxcStyleFormatting);
        for(size_t i = 0; i < members->size(); i++)
        {
          ret += ", ";

          ret += members->at(i)->toString(dxcStyleFormatting, withType);
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
        ret += inner->toString(dxcStyleFormatting, withType);
        ret += " to ";
        ret += type->toString(dxcStyleFormatting);
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

            shaderValAppendToString(members->at(i)->type, v, 0, ret, dxcStyleFormatting);
          }
          else
          {
            ret += members->at(i)->toString(dxcStyleFormatting, withType);
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
    shaderValAppendToString(type, v, 0, ret, dxcStyleFormatting);
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
        ret += type->inner->toString(dxcStyleFormatting) + " ";
      if(isCompound() && cast<Constant>(members->at(i))->isUndef())
        ret += "undef";
      else
        shaderValAppendToString(type, v, i, ret, dxcStyleFormatting);
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
            ret += type->inner->toString(dxcStyleFormatting) + " ";

          ShaderValue v;
          v.u64v[0] = l->literal;

          shaderValAppendToString(type->inner, v, 0, ret, dxcStyleFormatting);
        }
        else
        {
          ret += members->at(i)->toString(dxcStyleFormatting, withType);
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

          shaderValAppendToString(members->at(i)->type, v, 0, ret, dxcStyleFormatting);
        }
        else
        {
          ret += members->at(i)->toString(dxcStyleFormatting, withType);
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

rdcstr Program::GetArgId(const Instruction &inst, uint32_t arg) const
{
  return ArgToString(inst.args[arg], false);
}

void Program::MakeResultId(const DXIL::Instruction &inst, rdcstr &resultId)
{
  if(!inst.getName().empty())
    resultId = StringFormat::Fmt("%c%s", '_', escapeStringIfNeeded(inst.getName()).c_str());
  else if(inst.slot != ~0U)
    resultId = StringFormat::Fmt("%c%s", '_', ToStr(inst.slot).c_str());
}

};    // namespace DXIL
