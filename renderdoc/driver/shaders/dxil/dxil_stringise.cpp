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

#include "dxil_bytecode.h"
#include "dxil_common.h"

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

template <>
rdcstr DoStringise(const DXIL::AtomicBinOpCode &el)
{
  BEGIN_ENUM_STRINGISE(DXIL::AtomicBinOpCode)
  {
    STRINGISE_ENUM_CLASS(Add)
    STRINGISE_ENUM_CLASS(And)
    STRINGISE_ENUM_CLASS(Or)
    STRINGISE_ENUM_CLASS(Xor)
    STRINGISE_ENUM_CLASS(IMin)
    STRINGISE_ENUM_CLASS(IMax)
    STRINGISE_ENUM_CLASS(UMin)
    STRINGISE_ENUM_CLASS(UMax)
    STRINGISE_ENUM_CLASS(Exchange)
    STRINGISE_ENUM_CLASS_NAMED(Invalid, "<invalid AtommicBinOpCode>");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::Operation &el)
{
  BEGIN_ENUM_STRINGISE(DXIL::Operation)
  {
    STRINGISE_ENUM_CLASS(NoOp)
    STRINGISE_ENUM_CLASS(Call)
    STRINGISE_ENUM_CLASS(Trunc)
    STRINGISE_ENUM_CLASS(ZExt)
    STRINGISE_ENUM_CLASS(SExt)
    STRINGISE_ENUM_CLASS(FToU)
    STRINGISE_ENUM_CLASS(FToS)
    STRINGISE_ENUM_CLASS(UToF)
    STRINGISE_ENUM_CLASS(SToF)
    STRINGISE_ENUM_CLASS(FPTrunc)
    STRINGISE_ENUM_CLASS(FPExt)
    STRINGISE_ENUM_CLASS(PtrToI)
    STRINGISE_ENUM_CLASS(IToPtr)
    STRINGISE_ENUM_CLASS(Bitcast)
    STRINGISE_ENUM_CLASS(AddrSpaceCast)
    STRINGISE_ENUM_CLASS(ExtractVal)
    STRINGISE_ENUM_CLASS(Ret)
    STRINGISE_ENUM_CLASS(FAdd)
    STRINGISE_ENUM_CLASS(FSub)
    STRINGISE_ENUM_CLASS(FMul)
    STRINGISE_ENUM_CLASS(FDiv)
    STRINGISE_ENUM_CLASS(FRem)
    STRINGISE_ENUM_CLASS(Add)
    STRINGISE_ENUM_CLASS(Sub)
    STRINGISE_ENUM_CLASS(Mul)
    STRINGISE_ENUM_CLASS(UDiv)
    STRINGISE_ENUM_CLASS(SDiv)
    STRINGISE_ENUM_CLASS(URem)
    STRINGISE_ENUM_CLASS(SRem)
    STRINGISE_ENUM_CLASS(ShiftLeft)
    STRINGISE_ENUM_CLASS(LogicalShiftRight)
    STRINGISE_ENUM_CLASS(ArithShiftRight)
    STRINGISE_ENUM_CLASS(And)
    STRINGISE_ENUM_CLASS(Or)
    STRINGISE_ENUM_CLASS(Xor)
    STRINGISE_ENUM_CLASS(Unreachable)
    STRINGISE_ENUM_CLASS(Alloca)
    STRINGISE_ENUM_CLASS(GetElementPtr)
    STRINGISE_ENUM_CLASS(Load)
    STRINGISE_ENUM_CLASS(Store)
    STRINGISE_ENUM_CLASS(FOrdFalse)
    STRINGISE_ENUM_CLASS(FOrdEqual)
    STRINGISE_ENUM_CLASS(FOrdGreater)
    STRINGISE_ENUM_CLASS(FOrdGreaterEqual)
    STRINGISE_ENUM_CLASS(FOrdLess)
    STRINGISE_ENUM_CLASS(FOrdLessEqual)
    STRINGISE_ENUM_CLASS(FOrdNotEqual)
    STRINGISE_ENUM_CLASS(FOrd)
    STRINGISE_ENUM_CLASS(FUnord)
    STRINGISE_ENUM_CLASS(FUnordEqual)
    STRINGISE_ENUM_CLASS(FUnordGreater)
    STRINGISE_ENUM_CLASS(FUnordGreaterEqual)
    STRINGISE_ENUM_CLASS(FUnordLess)
    STRINGISE_ENUM_CLASS(FUnordLessEqual)
    STRINGISE_ENUM_CLASS(FUnordNotEqual)
    STRINGISE_ENUM_CLASS(FOrdTrue)
    STRINGISE_ENUM_CLASS(IEqual)
    STRINGISE_ENUM_CLASS(INotEqual)
    STRINGISE_ENUM_CLASS(UGreater)
    STRINGISE_ENUM_CLASS(UGreaterEqual)
    STRINGISE_ENUM_CLASS(ULess)
    STRINGISE_ENUM_CLASS(ULessEqual)
    STRINGISE_ENUM_CLASS(SGreater)
    STRINGISE_ENUM_CLASS(SGreaterEqual)
    STRINGISE_ENUM_CLASS(SLess)
    STRINGISE_ENUM_CLASS(SLessEqual)
    STRINGISE_ENUM_CLASS(Select)
    STRINGISE_ENUM_CLASS(ExtractElement)
    STRINGISE_ENUM_CLASS(InsertElement)
    STRINGISE_ENUM_CLASS(ShuffleVector)
    STRINGISE_ENUM_CLASS(InsertValue)
    STRINGISE_ENUM_CLASS(Branch)
    STRINGISE_ENUM_CLASS(Phi)
    STRINGISE_ENUM_CLASS(Switch)
    STRINGISE_ENUM_CLASS(Fence)
    STRINGISE_ENUM_CLASS(CompareExchange)
    STRINGISE_ENUM_CLASS(LoadAtomic)
    STRINGISE_ENUM_CLASS(StoreAtomic)
    STRINGISE_ENUM_CLASS(AtomicExchange)
    STRINGISE_ENUM_CLASS(AtomicAdd)
    STRINGISE_ENUM_CLASS(AtomicSub)
    STRINGISE_ENUM_CLASS(AtomicAnd)
    STRINGISE_ENUM_CLASS(AtomicNand)
    STRINGISE_ENUM_CLASS(AtomicOr)
    STRINGISE_ENUM_CLASS(AtomicXor)
    STRINGISE_ENUM_CLASS(AtomicMax)
    STRINGISE_ENUM_CLASS(AtomicMin)
    STRINGISE_ENUM_CLASS(AtomicUMax)
    STRINGISE_ENUM_CLASS(AtomicUMin)
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::DXOp &el)
{
  BEGIN_ENUM_STRINGISE(DXIL::DXOp);
  {
    STRINGISE_ENUM_CLASS(TempRegLoad);
    STRINGISE_ENUM_CLASS(TempRegStore);
    STRINGISE_ENUM_CLASS(MinPrecXRegLoad);
    STRINGISE_ENUM_CLASS(MinPrecXRegStore);
    STRINGISE_ENUM_CLASS(LoadInput);
    STRINGISE_ENUM_CLASS(StoreOutput);
    STRINGISE_ENUM_CLASS(FAbs);
    STRINGISE_ENUM_CLASS(Saturate);
    STRINGISE_ENUM_CLASS(IsNaN);
    STRINGISE_ENUM_CLASS(IsInf);
    STRINGISE_ENUM_CLASS(IsFinite);
    STRINGISE_ENUM_CLASS(IsNormal);
    STRINGISE_ENUM_CLASS(Cos);
    STRINGISE_ENUM_CLASS(Sin);
    STRINGISE_ENUM_CLASS(Tan);
    STRINGISE_ENUM_CLASS(Acos);
    STRINGISE_ENUM_CLASS(Asin);
    STRINGISE_ENUM_CLASS(Atan);
    STRINGISE_ENUM_CLASS(Hcos);
    STRINGISE_ENUM_CLASS(Hsin);
    STRINGISE_ENUM_CLASS(Htan);
    STRINGISE_ENUM_CLASS(Exp);
    STRINGISE_ENUM_CLASS(Frc);
    STRINGISE_ENUM_CLASS(Log);
    STRINGISE_ENUM_CLASS(Sqrt);
    STRINGISE_ENUM_CLASS(Rsqrt);
    STRINGISE_ENUM_CLASS(Round_ne);
    STRINGISE_ENUM_CLASS(Round_ni);
    STRINGISE_ENUM_CLASS(Round_pi);
    STRINGISE_ENUM_CLASS(Round_z);
    STRINGISE_ENUM_CLASS(Bfrev);
    STRINGISE_ENUM_CLASS(Countbits);
    STRINGISE_ENUM_CLASS(FirstbitLo);
    STRINGISE_ENUM_CLASS(FirstbitHi);
    STRINGISE_ENUM_CLASS(FirstbitSHi);
    STRINGISE_ENUM_CLASS(FMax);
    STRINGISE_ENUM_CLASS(FMin);
    STRINGISE_ENUM_CLASS(IMax);
    STRINGISE_ENUM_CLASS(IMin);
    STRINGISE_ENUM_CLASS(UMax);
    STRINGISE_ENUM_CLASS(UMin);
    STRINGISE_ENUM_CLASS(IMul);
    STRINGISE_ENUM_CLASS(UMul);
    STRINGISE_ENUM_CLASS(UDiv);
    STRINGISE_ENUM_CLASS(UAddc);
    STRINGISE_ENUM_CLASS(USubb);
    STRINGISE_ENUM_CLASS(FMad);
    STRINGISE_ENUM_CLASS(Fma);
    STRINGISE_ENUM_CLASS(IMad);
    STRINGISE_ENUM_CLASS(UMad);
    STRINGISE_ENUM_CLASS(Msad);
    STRINGISE_ENUM_CLASS(Ibfe);
    STRINGISE_ENUM_CLASS(Ubfe);
    STRINGISE_ENUM_CLASS(Bfi);
    STRINGISE_ENUM_CLASS(Dot2);
    STRINGISE_ENUM_CLASS(Dot3);
    STRINGISE_ENUM_CLASS(Dot4);
    STRINGISE_ENUM_CLASS(CreateHandle);
    STRINGISE_ENUM_CLASS(CBufferLoad);
    STRINGISE_ENUM_CLASS(CBufferLoadLegacy);
    STRINGISE_ENUM_CLASS(Sample);
    STRINGISE_ENUM_CLASS(SampleBias);
    STRINGISE_ENUM_CLASS(SampleLevel);
    STRINGISE_ENUM_CLASS(SampleGrad);
    STRINGISE_ENUM_CLASS(SampleCmp);
    STRINGISE_ENUM_CLASS(SampleCmpLevelZero);
    STRINGISE_ENUM_CLASS(TextureLoad);
    STRINGISE_ENUM_CLASS(TextureStore);
    STRINGISE_ENUM_CLASS(BufferLoad);
    STRINGISE_ENUM_CLASS(BufferStore);
    STRINGISE_ENUM_CLASS(BufferUpdateCounter);
    STRINGISE_ENUM_CLASS(CheckAccessFullyMapped);
    STRINGISE_ENUM_CLASS(GetDimensions);
    STRINGISE_ENUM_CLASS(TextureGather);
    STRINGISE_ENUM_CLASS(TextureGatherCmp);
    STRINGISE_ENUM_CLASS(Texture2DMSGetSamplePosition);
    STRINGISE_ENUM_CLASS(RenderTargetGetSamplePosition);
    STRINGISE_ENUM_CLASS(RenderTargetGetSampleCount);
    STRINGISE_ENUM_CLASS(AtomicBinOp);
    STRINGISE_ENUM_CLASS(AtomicCompareExchange);
    STRINGISE_ENUM_CLASS(Barrier);
    STRINGISE_ENUM_CLASS(CalculateLOD);
    STRINGISE_ENUM_CLASS(Discard);
    STRINGISE_ENUM_CLASS(DerivCoarseX);
    STRINGISE_ENUM_CLASS(DerivCoarseY);
    STRINGISE_ENUM_CLASS(DerivFineX);
    STRINGISE_ENUM_CLASS(DerivFineY);
    STRINGISE_ENUM_CLASS(EvalSnapped);
    STRINGISE_ENUM_CLASS(EvalSampleIndex);
    STRINGISE_ENUM_CLASS(EvalCentroid);
    STRINGISE_ENUM_CLASS(SampleIndex);
    STRINGISE_ENUM_CLASS(Coverage);
    STRINGISE_ENUM_CLASS(InnerCoverage);
    STRINGISE_ENUM_CLASS(ThreadId);
    STRINGISE_ENUM_CLASS(GroupId);
    STRINGISE_ENUM_CLASS(ThreadIdInGroup);
    STRINGISE_ENUM_CLASS(FlattenedThreadIdInGroup);
    STRINGISE_ENUM_CLASS(EmitStream);
    STRINGISE_ENUM_CLASS(CutStream);
    STRINGISE_ENUM_CLASS(EmitThenCutStream);
    STRINGISE_ENUM_CLASS(GSInstanceID);
    STRINGISE_ENUM_CLASS(MakeDouble);
    STRINGISE_ENUM_CLASS(SplitDouble);
    STRINGISE_ENUM_CLASS(LoadOutputControlPoint);
    STRINGISE_ENUM_CLASS(LoadPatchConstant);
    STRINGISE_ENUM_CLASS(DomainLocation);
    STRINGISE_ENUM_CLASS(StorePatchConstant);
    STRINGISE_ENUM_CLASS(OutputControlPointID);
    STRINGISE_ENUM_CLASS(PrimitiveID);
    STRINGISE_ENUM_CLASS(CycleCounterLegacy);
    STRINGISE_ENUM_CLASS(WaveIsFirstLane);
    STRINGISE_ENUM_CLASS(WaveGetLaneIndex);
    STRINGISE_ENUM_CLASS(WaveGetLaneCount);
    STRINGISE_ENUM_CLASS(WaveAnyTrue);
    STRINGISE_ENUM_CLASS(WaveAllTrue);
    STRINGISE_ENUM_CLASS(WaveActiveAllEqual);
    STRINGISE_ENUM_CLASS(WaveActiveBallot);
    STRINGISE_ENUM_CLASS(WaveReadLaneAt);
    STRINGISE_ENUM_CLASS(WaveReadLaneFirst);
    STRINGISE_ENUM_CLASS(WaveActiveOp);
    STRINGISE_ENUM_CLASS(WaveActiveBit);
    STRINGISE_ENUM_CLASS(WavePrefixOp);
    STRINGISE_ENUM_CLASS(QuadReadLaneAt);
    STRINGISE_ENUM_CLASS(QuadOp);
    STRINGISE_ENUM_CLASS(BitcastI16toF16);
    STRINGISE_ENUM_CLASS(BitcastF16toI16);
    STRINGISE_ENUM_CLASS(BitcastI32toF32);
    STRINGISE_ENUM_CLASS(BitcastF32toI32);
    STRINGISE_ENUM_CLASS(BitcastI64toF64);
    STRINGISE_ENUM_CLASS(BitcastF64toI64);
    STRINGISE_ENUM_CLASS(LegacyF32ToF16);
    STRINGISE_ENUM_CLASS(LegacyF16ToF32);
    STRINGISE_ENUM_CLASS(LegacyDoubleToFloat);
    STRINGISE_ENUM_CLASS(LegacyDoubleToSInt32);
    STRINGISE_ENUM_CLASS(LegacyDoubleToUInt32);
    STRINGISE_ENUM_CLASS(WaveAllBitCount);
    STRINGISE_ENUM_CLASS(WavePrefixBitCount);
    STRINGISE_ENUM_CLASS(AttributeAtVertex);
    STRINGISE_ENUM_CLASS(ViewID);
    STRINGISE_ENUM_CLASS(RawBufferLoad);
    STRINGISE_ENUM_CLASS(RawBufferStore);
    STRINGISE_ENUM_CLASS(InstanceID);
    STRINGISE_ENUM_CLASS(InstanceIndex);
    STRINGISE_ENUM_CLASS(HitKind);
    STRINGISE_ENUM_CLASS(RayFlags);
    STRINGISE_ENUM_CLASS(DispatchRaysIndex);
    STRINGISE_ENUM_CLASS(DispatchRaysDimensions);
    STRINGISE_ENUM_CLASS(WorldRayOrigin);
    STRINGISE_ENUM_CLASS(WorldRayDirection);
    STRINGISE_ENUM_CLASS(ObjectRayOrigin);
    STRINGISE_ENUM_CLASS(ObjectRayDirection);
    STRINGISE_ENUM_CLASS(ObjectToWorld);
    STRINGISE_ENUM_CLASS(WorldToObject);
    STRINGISE_ENUM_CLASS(RayTMin);
    STRINGISE_ENUM_CLASS(RayTCurrent);
    STRINGISE_ENUM_CLASS(IgnoreHit);
    STRINGISE_ENUM_CLASS(AcceptHitAndEndSearch);
    STRINGISE_ENUM_CLASS(TraceRay);
    STRINGISE_ENUM_CLASS(ReportHit);
    STRINGISE_ENUM_CLASS(CallShader);
    STRINGISE_ENUM_CLASS(CreateHandleForLib);
    STRINGISE_ENUM_CLASS(PrimitiveIndex);
    STRINGISE_ENUM_CLASS(Dot2AddHalf);
    STRINGISE_ENUM_CLASS(Dot4AddI8Packed);
    STRINGISE_ENUM_CLASS(Dot4AddU8Packed);
    STRINGISE_ENUM_CLASS(WaveMatch);
    STRINGISE_ENUM_CLASS(WaveMultiPrefixOp);
    STRINGISE_ENUM_CLASS(WaveMultiPrefixBitCount);
    STRINGISE_ENUM_CLASS(SetMeshOutputCounts);
    STRINGISE_ENUM_CLASS(EmitIndices);
    STRINGISE_ENUM_CLASS(GetMeshPayload);
    STRINGISE_ENUM_CLASS(StoreVertexOutput);
    STRINGISE_ENUM_CLASS(StorePrimitiveOutput);
    STRINGISE_ENUM_CLASS(DispatchMesh);
    STRINGISE_ENUM_CLASS(WriteSamplerFeedback);
    STRINGISE_ENUM_CLASS(WriteSamplerFeedbackBias);
    STRINGISE_ENUM_CLASS(WriteSamplerFeedbackLevel);
    STRINGISE_ENUM_CLASS(WriteSamplerFeedbackGrad);
    STRINGISE_ENUM_CLASS(AllocateRayQuery);
    STRINGISE_ENUM_CLASS(RayQuery_TraceRayInline);
    STRINGISE_ENUM_CLASS(RayQuery_Proceed);
    STRINGISE_ENUM_CLASS(RayQuery_Abort);
    STRINGISE_ENUM_CLASS(RayQuery_CommitNonOpaqueTriangleHit);
    STRINGISE_ENUM_CLASS(RayQuery_CommitProceduralPrimitiveHit);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedStatus);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateType);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateObjectToWorld3x4);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateWorldToObject3x4);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedObjectToWorld3x4);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedWorldToObject3x4);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateProceduralPrimitiveNonOpaque);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateTriangleFrontFace);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedTriangleFrontFace);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateTriangleBarycentrics);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedTriangleBarycentrics);
    STRINGISE_ENUM_CLASS(RayQuery_RayFlags);
    STRINGISE_ENUM_CLASS(RayQuery_WorldRayOrigin);
    STRINGISE_ENUM_CLASS(RayQuery_WorldRayDirection);
    STRINGISE_ENUM_CLASS(RayQuery_RayTMin);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateTriangleRayT);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedRayT);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateInstanceIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateInstanceID);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateGeometryIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CandidatePrimitiveIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateObjectRayOrigin);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateObjectRayDirection);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedInstanceIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedInstanceID);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedGeometryIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedPrimitiveIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedObjectRayOrigin);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedObjectRayDirection);
    STRINGISE_ENUM_CLASS(GeometryIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CandidateInstanceContributionToHitGroupIndex);
    STRINGISE_ENUM_CLASS(RayQuery_CommittedInstanceContributionToHitGroupIndex);
    STRINGISE_ENUM_CLASS(AnnotateHandle);
    STRINGISE_ENUM_CLASS(CreateHandleFromBinding);
    STRINGISE_ENUM_CLASS(CreateHandleFromHeap);
    STRINGISE_ENUM_CLASS(Unpack4x8);
    STRINGISE_ENUM_CLASS(Pack4x8);
    STRINGISE_ENUM_CLASS(IsHelperLane);
    STRINGISE_ENUM_CLASS(QuadVote);
    STRINGISE_ENUM_CLASS(TextureGatherRaw);
    STRINGISE_ENUM_CLASS(SampleCmpLevel);
    STRINGISE_ENUM_CLASS(TextureStoreSample);
    STRINGISE_ENUM_CLASS(WaveMatrix_Annotate);
    STRINGISE_ENUM_CLASS(WaveMatrix_Depth);
    STRINGISE_ENUM_CLASS(WaveMatrix_Fill);
    STRINGISE_ENUM_CLASS(WaveMatrix_LoadRawBuf);
    STRINGISE_ENUM_CLASS(WaveMatrix_LoadGroupShared);
    STRINGISE_ENUM_CLASS(WaveMatrix_StoreRawBuf);
    STRINGISE_ENUM_CLASS(WaveMatrix_StoreGroupShared);
    STRINGISE_ENUM_CLASS(WaveMatrix_Multiply);
    STRINGISE_ENUM_CLASS(WaveMatrix_MultiplyAccumulate);
    STRINGISE_ENUM_CLASS(WaveMatrix_ScalarOp);
    STRINGISE_ENUM_CLASS(WaveMatrix_SumAccumulate);
    STRINGISE_ENUM_CLASS(WaveMatrix_Add);
    STRINGISE_ENUM_CLASS(AllocateNodeOutputRecords);
    STRINGISE_ENUM_CLASS(GetNodeRecordPtr);
    STRINGISE_ENUM_CLASS(IncrementOutputCount);
    STRINGISE_ENUM_CLASS(OutputComplete);
    STRINGISE_ENUM_CLASS(GetInputRecordCount);
    STRINGISE_ENUM_CLASS(FinishedCrossGroupSharing);
    STRINGISE_ENUM_CLASS(BarrierByMemoryType);
    STRINGISE_ENUM_CLASS(BarrierByMemoryHandle);
    STRINGISE_ENUM_CLASS(BarrierByNodeRecordHandle);
    STRINGISE_ENUM_CLASS(CreateNodeOutputHandle);
    STRINGISE_ENUM_CLASS(IndexNodeHandle);
    STRINGISE_ENUM_CLASS(AnnotateNodeHandle);
    STRINGISE_ENUM_CLASS(CreateNodeInputRecordHandle);
    STRINGISE_ENUM_CLASS(AnnotateNodeRecordHandle);
    STRINGISE_ENUM_CLASS(NodeOutputIsValid);
    STRINGISE_ENUM_CLASS(GetRemainingRecursionLevels);
    STRINGISE_ENUM_CLASS(SampleCmpGrad);
    STRINGISE_ENUM_CLASS(SampleCmpBias);
    STRINGISE_ENUM_CLASS(StartVertexLocation);
    STRINGISE_ENUM_CLASS(StartInstanceLocation);
    STRINGISE_ENUM_CLASS(NumOpCodes);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::Type::TypeKind &el)
{
  BEGIN_ENUM_STRINGISE(DXIL::Type::TypeKind);
  {
    STRINGISE_ENUM_CLASS(None);
    STRINGISE_ENUM_CLASS(Scalar);
    STRINGISE_ENUM_CLASS(Vector);
    STRINGISE_ENUM_CLASS(Pointer);
    STRINGISE_ENUM_CLASS(Array);
    STRINGISE_ENUM_CLASS(Function);
    STRINGISE_ENUM_CLASS(Struct);
    STRINGISE_ENUM_CLASS(Metadata);
    STRINGISE_ENUM_CLASS(Label);
  }
  END_ENUM_STRINGISE();
}
