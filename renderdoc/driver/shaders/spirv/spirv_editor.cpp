/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "spirv_editor.h"
#include <algorithm>
#include <utility>
#include "common/common.h"
#include "serialise/serialiser.h"

// hopefully this will be in the official spirv.hpp soon
// clang-format off
namespace spv
{
inline void HasResultAndType(Op opcode, bool *hasResult, bool *hasResultType) {
    *hasResult = *hasResultType = false;
    switch (opcode) {
    default: /* unknown opcode */ break;
    case OpNop: *hasResult = false; *hasResultType = false; break;
    case OpUndef: *hasResult = true; *hasResultType = true; break;
    case OpSourceContinued: *hasResult = false; *hasResultType = false; break;
    case OpSource: *hasResult = false; *hasResultType = false; break;
    case OpSourceExtension: *hasResult = false; *hasResultType = false; break;
    case OpName: *hasResult = false; *hasResultType = false; break;
    case OpMemberName: *hasResult = false; *hasResultType = false; break;
    case OpString: *hasResult = true; *hasResultType = false; break;
    case OpLine: *hasResult = false; *hasResultType = false; break;
    case OpExtension: *hasResult = false; *hasResultType = false; break;
    case OpExtInstImport: *hasResult = true; *hasResultType = false; break;
    case OpExtInst: *hasResult = true; *hasResultType = true; break;
    case OpMemoryModel: *hasResult = false; *hasResultType = false; break;
    case OpEntryPoint: *hasResult = false; *hasResultType = false; break;
    case OpExecutionMode: *hasResult = false; *hasResultType = false; break;
    case OpCapability: *hasResult = false; *hasResultType = false; break;
    case OpTypeVoid: *hasResult = true; *hasResultType = false; break;
    case OpTypeBool: *hasResult = true; *hasResultType = false; break;
    case OpTypeInt: *hasResult = true; *hasResultType = false; break;
    case OpTypeFloat: *hasResult = true; *hasResultType = false; break;
    case OpTypeVector: *hasResult = true; *hasResultType = false; break;
    case OpTypeMatrix: *hasResult = true; *hasResultType = false; break;
    case OpTypeImage: *hasResult = true; *hasResultType = false; break;
    case OpTypeSampler: *hasResult = true; *hasResultType = false; break;
    case OpTypeSampledImage: *hasResult = true; *hasResultType = false; break;
    case OpTypeArray: *hasResult = true; *hasResultType = false; break;
    case OpTypeRuntimeArray: *hasResult = true; *hasResultType = false; break;
    case OpTypeStruct: *hasResult = true; *hasResultType = false; break;
    case OpTypeOpaque: *hasResult = true; *hasResultType = false; break;
    case OpTypePointer: *hasResult = true; *hasResultType = false; break;
    case OpTypeFunction: *hasResult = true; *hasResultType = false; break;
    case OpTypeEvent: *hasResult = true; *hasResultType = false; break;
    case OpTypeDeviceEvent: *hasResult = true; *hasResultType = false; break;
    case OpTypeReserveId: *hasResult = true; *hasResultType = false; break;
    case OpTypeQueue: *hasResult = true; *hasResultType = false; break;
    case OpTypePipe: *hasResult = true; *hasResultType = false; break;
    case OpTypeForwardPointer: *hasResult = false; *hasResultType = false; break;
    case OpConstantTrue: *hasResult = true; *hasResultType = true; break;
    case OpConstantFalse: *hasResult = true; *hasResultType = true; break;
    case OpConstant: *hasResult = true; *hasResultType = true; break;
    case OpConstantComposite: *hasResult = true; *hasResultType = true; break;
    case OpConstantSampler: *hasResult = true; *hasResultType = true; break;
    case OpConstantNull: *hasResult = true; *hasResultType = true; break;
    case OpSpecConstantTrue: *hasResult = true; *hasResultType = true; break;
    case OpSpecConstantFalse: *hasResult = true; *hasResultType = true; break;
    case OpSpecConstant: *hasResult = true; *hasResultType = true; break;
    case OpSpecConstantComposite: *hasResult = true; *hasResultType = true; break;
    case OpSpecConstantOp: *hasResult = true; *hasResultType = true; break;
    case OpFunction: *hasResult = true; *hasResultType = true; break;
    case OpFunctionParameter: *hasResult = true; *hasResultType = true; break;
    case OpFunctionEnd: *hasResult = false; *hasResultType = false; break;
    case OpFunctionCall: *hasResult = true; *hasResultType = true; break;
    case OpVariable: *hasResult = true; *hasResultType = true; break;
    case OpImageTexelPointer: *hasResult = true; *hasResultType = true; break;
    case OpLoad: *hasResult = true; *hasResultType = true; break;
    case OpStore: *hasResult = false; *hasResultType = false; break;
    case OpCopyMemory: *hasResult = false; *hasResultType = false; break;
    case OpCopyMemorySized: *hasResult = false; *hasResultType = false; break;
    case OpAccessChain: *hasResult = true; *hasResultType = true; break;
    case OpInBoundsAccessChain: *hasResult = true; *hasResultType = true; break;
    case OpPtrAccessChain: *hasResult = true; *hasResultType = true; break;
    case OpArrayLength: *hasResult = true; *hasResultType = true; break;
    case OpGenericPtrMemSemantics: *hasResult = true; *hasResultType = true; break;
    case OpInBoundsPtrAccessChain: *hasResult = true; *hasResultType = true; break;
    case OpDecorate: *hasResult = false; *hasResultType = false; break;
    case OpMemberDecorate: *hasResult = false; *hasResultType = false; break;
    case OpDecorationGroup: *hasResult = true; *hasResultType = false; break;
    case OpGroupDecorate: *hasResult = false; *hasResultType = false; break;
    case OpGroupMemberDecorate: *hasResult = false; *hasResultType = false; break;
    case OpVectorExtractDynamic: *hasResult = true; *hasResultType = true; break;
    case OpVectorInsertDynamic: *hasResult = true; *hasResultType = true; break;
    case OpVectorShuffle: *hasResult = true; *hasResultType = true; break;
    case OpCompositeConstruct: *hasResult = true; *hasResultType = true; break;
    case OpCompositeExtract: *hasResult = true; *hasResultType = true; break;
    case OpCompositeInsert: *hasResult = true; *hasResultType = true; break;
    case OpCopyObject: *hasResult = true; *hasResultType = true; break;
    case OpTranspose: *hasResult = true; *hasResultType = true; break;
    case OpSampledImage: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleDrefImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleDrefExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleProjImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleProjExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleProjDrefImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleProjDrefExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageFetch: *hasResult = true; *hasResultType = true; break;
    case OpImageGather: *hasResult = true; *hasResultType = true; break;
    case OpImageDrefGather: *hasResult = true; *hasResultType = true; break;
    case OpImageRead: *hasResult = true; *hasResultType = true; break;
    case OpImageWrite: *hasResult = false; *hasResultType = false; break;
    case OpImage: *hasResult = true; *hasResultType = true; break;
    case OpImageQueryFormat: *hasResult = true; *hasResultType = true; break;
    case OpImageQueryOrder: *hasResult = true; *hasResultType = true; break;
    case OpImageQuerySizeLod: *hasResult = true; *hasResultType = true; break;
    case OpImageQuerySize: *hasResult = true; *hasResultType = true; break;
    case OpImageQueryLod: *hasResult = true; *hasResultType = true; break;
    case OpImageQueryLevels: *hasResult = true; *hasResultType = true; break;
    case OpImageQuerySamples: *hasResult = true; *hasResultType = true; break;
    case OpConvertFToU: *hasResult = true; *hasResultType = true; break;
    case OpConvertFToS: *hasResult = true; *hasResultType = true; break;
    case OpConvertSToF: *hasResult = true; *hasResultType = true; break;
    case OpConvertUToF: *hasResult = true; *hasResultType = true; break;
    case OpUConvert: *hasResult = true; *hasResultType = true; break;
    case OpSConvert: *hasResult = true; *hasResultType = true; break;
    case OpFConvert: *hasResult = true; *hasResultType = true; break;
    case OpQuantizeToF16: *hasResult = true; *hasResultType = true; break;
    case OpConvertPtrToU: *hasResult = true; *hasResultType = true; break;
    case OpSatConvertSToU: *hasResult = true; *hasResultType = true; break;
    case OpSatConvertUToS: *hasResult = true; *hasResultType = true; break;
    case OpConvertUToPtr: *hasResult = true; *hasResultType = true; break;
    case OpPtrCastToGeneric: *hasResult = true; *hasResultType = true; break;
    case OpGenericCastToPtr: *hasResult = true; *hasResultType = true; break;
    case OpGenericCastToPtrExplicit: *hasResult = true; *hasResultType = true; break;
    case OpBitcast: *hasResult = true; *hasResultType = true; break;
    case OpSNegate: *hasResult = true; *hasResultType = true; break;
    case OpFNegate: *hasResult = true; *hasResultType = true; break;
    case OpIAdd: *hasResult = true; *hasResultType = true; break;
    case OpFAdd: *hasResult = true; *hasResultType = true; break;
    case OpISub: *hasResult = true; *hasResultType = true; break;
    case OpFSub: *hasResult = true; *hasResultType = true; break;
    case OpIMul: *hasResult = true; *hasResultType = true; break;
    case OpFMul: *hasResult = true; *hasResultType = true; break;
    case OpUDiv: *hasResult = true; *hasResultType = true; break;
    case OpSDiv: *hasResult = true; *hasResultType = true; break;
    case OpFDiv: *hasResult = true; *hasResultType = true; break;
    case OpUMod: *hasResult = true; *hasResultType = true; break;
    case OpSRem: *hasResult = true; *hasResultType = true; break;
    case OpSMod: *hasResult = true; *hasResultType = true; break;
    case OpFRem: *hasResult = true; *hasResultType = true; break;
    case OpFMod: *hasResult = true; *hasResultType = true; break;
    case OpVectorTimesScalar: *hasResult = true; *hasResultType = true; break;
    case OpMatrixTimesScalar: *hasResult = true; *hasResultType = true; break;
    case OpVectorTimesMatrix: *hasResult = true; *hasResultType = true; break;
    case OpMatrixTimesVector: *hasResult = true; *hasResultType = true; break;
    case OpMatrixTimesMatrix: *hasResult = true; *hasResultType = true; break;
    case OpOuterProduct: *hasResult = true; *hasResultType = true; break;
    case OpDot: *hasResult = true; *hasResultType = true; break;
    case OpIAddCarry: *hasResult = true; *hasResultType = true; break;
    case OpISubBorrow: *hasResult = true; *hasResultType = true; break;
    case OpUMulExtended: *hasResult = true; *hasResultType = true; break;
    case OpSMulExtended: *hasResult = true; *hasResultType = true; break;
    case OpAny: *hasResult = true; *hasResultType = true; break;
    case OpAll: *hasResult = true; *hasResultType = true; break;
    case OpIsNan: *hasResult = true; *hasResultType = true; break;
    case OpIsInf: *hasResult = true; *hasResultType = true; break;
    case OpIsFinite: *hasResult = true; *hasResultType = true; break;
    case OpIsNormal: *hasResult = true; *hasResultType = true; break;
    case OpSignBitSet: *hasResult = true; *hasResultType = true; break;
    case OpLessOrGreater: *hasResult = true; *hasResultType = true; break;
    case OpOrdered: *hasResult = true; *hasResultType = true; break;
    case OpUnordered: *hasResult = true; *hasResultType = true; break;
    case OpLogicalEqual: *hasResult = true; *hasResultType = true; break;
    case OpLogicalNotEqual: *hasResult = true; *hasResultType = true; break;
    case OpLogicalOr: *hasResult = true; *hasResultType = true; break;
    case OpLogicalAnd: *hasResult = true; *hasResultType = true; break;
    case OpLogicalNot: *hasResult = true; *hasResultType = true; break;
    case OpSelect: *hasResult = true; *hasResultType = true; break;
    case OpIEqual: *hasResult = true; *hasResultType = true; break;
    case OpINotEqual: *hasResult = true; *hasResultType = true; break;
    case OpUGreaterThan: *hasResult = true; *hasResultType = true; break;
    case OpSGreaterThan: *hasResult = true; *hasResultType = true; break;
    case OpUGreaterThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpSGreaterThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpULessThan: *hasResult = true; *hasResultType = true; break;
    case OpSLessThan: *hasResult = true; *hasResultType = true; break;
    case OpULessThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpSLessThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpFOrdEqual: *hasResult = true; *hasResultType = true; break;
    case OpFUnordEqual: *hasResult = true; *hasResultType = true; break;
    case OpFOrdNotEqual: *hasResult = true; *hasResultType = true; break;
    case OpFUnordNotEqual: *hasResult = true; *hasResultType = true; break;
    case OpFOrdLessThan: *hasResult = true; *hasResultType = true; break;
    case OpFUnordLessThan: *hasResult = true; *hasResultType = true; break;
    case OpFOrdGreaterThan: *hasResult = true; *hasResultType = true; break;
    case OpFUnordGreaterThan: *hasResult = true; *hasResultType = true; break;
    case OpFOrdLessThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpFUnordLessThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpFOrdGreaterThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpFUnordGreaterThanEqual: *hasResult = true; *hasResultType = true; break;
    case OpShiftRightLogical: *hasResult = true; *hasResultType = true; break;
    case OpShiftRightArithmetic: *hasResult = true; *hasResultType = true; break;
    case OpShiftLeftLogical: *hasResult = true; *hasResultType = true; break;
    case OpBitwiseOr: *hasResult = true; *hasResultType = true; break;
    case OpBitwiseXor: *hasResult = true; *hasResultType = true; break;
    case OpBitwiseAnd: *hasResult = true; *hasResultType = true; break;
    case OpNot: *hasResult = true; *hasResultType = true; break;
    case OpBitFieldInsert: *hasResult = true; *hasResultType = true; break;
    case OpBitFieldSExtract: *hasResult = true; *hasResultType = true; break;
    case OpBitFieldUExtract: *hasResult = true; *hasResultType = true; break;
    case OpBitReverse: *hasResult = true; *hasResultType = true; break;
    case OpBitCount: *hasResult = true; *hasResultType = true; break;
    case OpDPdx: *hasResult = true; *hasResultType = true; break;
    case OpDPdy: *hasResult = true; *hasResultType = true; break;
    case OpFwidth: *hasResult = true; *hasResultType = true; break;
    case OpDPdxFine: *hasResult = true; *hasResultType = true; break;
    case OpDPdyFine: *hasResult = true; *hasResultType = true; break;
    case OpFwidthFine: *hasResult = true; *hasResultType = true; break;
    case OpDPdxCoarse: *hasResult = true; *hasResultType = true; break;
    case OpDPdyCoarse: *hasResult = true; *hasResultType = true; break;
    case OpFwidthCoarse: *hasResult = true; *hasResultType = true; break;
    case OpEmitVertex: *hasResult = false; *hasResultType = false; break;
    case OpEndPrimitive: *hasResult = false; *hasResultType = false; break;
    case OpEmitStreamVertex: *hasResult = false; *hasResultType = false; break;
    case OpEndStreamPrimitive: *hasResult = false; *hasResultType = false; break;
    case OpControlBarrier: *hasResult = false; *hasResultType = false; break;
    case OpMemoryBarrier: *hasResult = false; *hasResultType = false; break;
    case OpAtomicLoad: *hasResult = true; *hasResultType = true; break;
    case OpAtomicStore: *hasResult = false; *hasResultType = false; break;
    case OpAtomicExchange: *hasResult = true; *hasResultType = true; break;
    case OpAtomicCompareExchange: *hasResult = true; *hasResultType = true; break;
    case OpAtomicCompareExchangeWeak: *hasResult = true; *hasResultType = true; break;
    case OpAtomicIIncrement: *hasResult = true; *hasResultType = true; break;
    case OpAtomicIDecrement: *hasResult = true; *hasResultType = true; break;
    case OpAtomicIAdd: *hasResult = true; *hasResultType = true; break;
    case OpAtomicISub: *hasResult = true; *hasResultType = true; break;
    case OpAtomicSMin: *hasResult = true; *hasResultType = true; break;
    case OpAtomicUMin: *hasResult = true; *hasResultType = true; break;
    case OpAtomicSMax: *hasResult = true; *hasResultType = true; break;
    case OpAtomicUMax: *hasResult = true; *hasResultType = true; break;
    case OpAtomicAnd: *hasResult = true; *hasResultType = true; break;
    case OpAtomicOr: *hasResult = true; *hasResultType = true; break;
    case OpAtomicXor: *hasResult = true; *hasResultType = true; break;
    case OpPhi: *hasResult = true; *hasResultType = true; break;
    case OpLoopMerge: *hasResult = false; *hasResultType = false; break;
    case OpSelectionMerge: *hasResult = false; *hasResultType = false; break;
    case OpLabel: *hasResult = true; *hasResultType = false; break;
    case OpBranch: *hasResult = false; *hasResultType = false; break;
    case OpBranchConditional: *hasResult = false; *hasResultType = false; break;
    case OpSwitch: *hasResult = false; *hasResultType = false; break;
    case OpKill: *hasResult = false; *hasResultType = false; break;
    case OpReturn: *hasResult = false; *hasResultType = false; break;
    case OpReturnValue: *hasResult = false; *hasResultType = false; break;
    case OpUnreachable: *hasResult = false; *hasResultType = false; break;
    case OpLifetimeStart: *hasResult = false; *hasResultType = false; break;
    case OpLifetimeStop: *hasResult = false; *hasResultType = false; break;
    case OpGroupAsyncCopy: *hasResult = true; *hasResultType = true; break;
    case OpGroupWaitEvents: *hasResult = false; *hasResultType = false; break;
    case OpGroupAll: *hasResult = true; *hasResultType = true; break;
    case OpGroupAny: *hasResult = true; *hasResultType = true; break;
    case OpGroupBroadcast: *hasResult = true; *hasResultType = true; break;
    case OpGroupIAdd: *hasResult = true; *hasResultType = true; break;
    case OpGroupFAdd: *hasResult = true; *hasResultType = true; break;
    case OpGroupFMin: *hasResult = true; *hasResultType = true; break;
    case OpGroupUMin: *hasResult = true; *hasResultType = true; break;
    case OpGroupSMin: *hasResult = true; *hasResultType = true; break;
    case OpGroupFMax: *hasResult = true; *hasResultType = true; break;
    case OpGroupUMax: *hasResult = true; *hasResultType = true; break;
    case OpGroupSMax: *hasResult = true; *hasResultType = true; break;
    case OpReadPipe: *hasResult = true; *hasResultType = true; break;
    case OpWritePipe: *hasResult = true; *hasResultType = true; break;
    case OpReservedReadPipe: *hasResult = true; *hasResultType = true; break;
    case OpReservedWritePipe: *hasResult = true; *hasResultType = true; break;
    case OpReserveReadPipePackets: *hasResult = true; *hasResultType = true; break;
    case OpReserveWritePipePackets: *hasResult = true; *hasResultType = true; break;
    case OpCommitReadPipe: *hasResult = false; *hasResultType = false; break;
    case OpCommitWritePipe: *hasResult = false; *hasResultType = false; break;
    case OpIsValidReserveId: *hasResult = true; *hasResultType = true; break;
    case OpGetNumPipePackets: *hasResult = true; *hasResultType = true; break;
    case OpGetMaxPipePackets: *hasResult = true; *hasResultType = true; break;
    case OpGroupReserveReadPipePackets: *hasResult = true; *hasResultType = true; break;
    case OpGroupReserveWritePipePackets: *hasResult = true; *hasResultType = true; break;
    case OpGroupCommitReadPipe: *hasResult = false; *hasResultType = false; break;
    case OpGroupCommitWritePipe: *hasResult = false; *hasResultType = false; break;
    case OpEnqueueMarker: *hasResult = true; *hasResultType = true; break;
    case OpEnqueueKernel: *hasResult = true; *hasResultType = true; break;
    case OpGetKernelNDrangeSubGroupCount: *hasResult = true; *hasResultType = true; break;
    case OpGetKernelNDrangeMaxSubGroupSize: *hasResult = true; *hasResultType = true; break;
    case OpGetKernelWorkGroupSize: *hasResult = true; *hasResultType = true; break;
    case OpGetKernelPreferredWorkGroupSizeMultiple: *hasResult = true; *hasResultType = true; break;
    case OpRetainEvent: *hasResult = false; *hasResultType = false; break;
    case OpReleaseEvent: *hasResult = false; *hasResultType = false; break;
    case OpCreateUserEvent: *hasResult = true; *hasResultType = true; break;
    case OpIsValidEvent: *hasResult = true; *hasResultType = true; break;
    case OpSetUserEventStatus: *hasResult = false; *hasResultType = false; break;
    case OpCaptureEventProfilingInfo: *hasResult = false; *hasResultType = false; break;
    case OpGetDefaultQueue: *hasResult = true; *hasResultType = true; break;
    case OpBuildNDRange: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleDrefImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleDrefExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleProjImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleProjExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleProjDrefImplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseSampleProjDrefExplicitLod: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseFetch: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseGather: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseDrefGather: *hasResult = true; *hasResultType = true; break;
    case OpImageSparseTexelsResident: *hasResult = true; *hasResultType = true; break;
    case OpNoLine: *hasResult = false; *hasResultType = false; break;
    case OpAtomicFlagTestAndSet: *hasResult = true; *hasResultType = true; break;
    case OpAtomicFlagClear: *hasResult = false; *hasResultType = false; break;
    case OpImageSparseRead: *hasResult = true; *hasResultType = true; break;
    case OpSizeOf: *hasResult = true; *hasResultType = true; break;
    case OpTypePipeStorage: *hasResult = true; *hasResultType = false; break;
    case OpConstantPipeStorage: *hasResult = true; *hasResultType = true; break;
    case OpCreatePipeFromPipeStorage: *hasResult = true; *hasResultType = true; break;
    case OpGetKernelLocalSizeForSubgroupCount: *hasResult = true; *hasResultType = true; break;
    case OpGetKernelMaxNumSubgroups: *hasResult = true; *hasResultType = true; break;
    case OpTypeNamedBarrier: *hasResult = true; *hasResultType = false; break;
    case OpNamedBarrierInitialize: *hasResult = true; *hasResultType = true; break;
    case OpMemoryNamedBarrier: *hasResult = false; *hasResultType = false; break;
    case OpModuleProcessed: *hasResult = false; *hasResultType = false; break;
    case OpExecutionModeId: *hasResult = false; *hasResultType = false; break;
    case OpDecorateId: *hasResult = false; *hasResultType = false; break;
    case OpGroupNonUniformElect: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformAll: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformAny: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformAllEqual: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBroadcast: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBroadcastFirst: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBallot: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformInverseBallot: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBallotBitExtract: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBallotBitCount: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBallotFindLSB: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBallotFindMSB: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformShuffle: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformShuffleXor: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformShuffleUp: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformShuffleDown: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformIAdd: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformFAdd: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformIMul: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformFMul: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformSMin: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformUMin: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformFMin: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformSMax: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformUMax: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformFMax: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBitwiseAnd: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBitwiseOr: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformBitwiseXor: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformLogicalAnd: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformLogicalOr: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformLogicalXor: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformQuadBroadcast: *hasResult = true; *hasResultType = true; break;
    case OpGroupNonUniformQuadSwap: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupBallotKHR: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupFirstInvocationKHR: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupAllKHR: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupAnyKHR: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupAllEqualKHR: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupReadInvocationKHR: *hasResult = true; *hasResultType = true; break;
    case OpGroupIAddNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupFAddNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupFMinNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupUMinNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupSMinNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupFMaxNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupUMaxNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpGroupSMaxNonUniformAMD: *hasResult = true; *hasResultType = true; break;
    case OpFragmentMaskFetchAMD: *hasResult = true; *hasResultType = true; break;
    case OpFragmentFetchAMD: *hasResult = true; *hasResultType = true; break;
    case OpWritePackedPrimitiveIndices4x8NV: *hasResult = false; *hasResultType = false; break;
    case OpReportIntersectionNV: *hasResult = true; *hasResultType = true; break;
    case OpIgnoreIntersectionNV: *hasResult = false; *hasResultType = false; break;
    case OpTerminateRayNV: *hasResult = false; *hasResultType = false; break;
    case OpTraceNV: *hasResult = false; *hasResultType = false; break;
    case OpTypeAccelerationStructureNV: *hasResult = true; *hasResultType = false; break;
    case OpExecuteCallableNV: *hasResult = false; *hasResultType = false; break;
    case OpSubgroupShuffleINTEL: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupShuffleDownINTEL: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupShuffleUpINTEL: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupShuffleXorINTEL: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupBlockReadINTEL: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupBlockWriteINTEL: *hasResult = false; *hasResultType = false; break;
    case OpSubgroupImageBlockReadINTEL: *hasResult = true; *hasResultType = true; break;
    case OpSubgroupImageBlockWriteINTEL: *hasResult = false; *hasResultType = false; break;
    case OpDecorateStringGOOGLE: *hasResult = false; *hasResultType = false; break;
    case OpMemberDecorateStringGOOGLE: *hasResult = false; *hasResultType = false; break;
    case OpGroupNonUniformPartitionNV: *hasResult = true; *hasResultType = true; break;
    case OpImageSampleFootprintNV: *hasResult = true; *hasResultType = true; break;
    }
}
};    // namespace spv
// clang-format on

static const uint32_t FirstRealWord = 5;

template <>
std::string DoStringise(const SPIRVId &el)
{
  return StringFormat::Fmt("%u", el.id);
}

void SPIRVOperation::nopRemove(size_t idx, size_t count)
{
  RDCASSERT(idx >= 1);
  size_t oldSize = size();

  if(count == 0)
    count = oldSize - idx;

  // reduce the size of this op
  *iter = MakeHeader(iter.opcode(), oldSize - count);

  if(idx + count < oldSize)
  {
    // move any words on the end into the middle, then nop them
    for(size_t i = 0; i < count; i++)
    {
      iter.word(idx + i) = iter.word(idx + count + i);
      iter.word(oldSize - i - 1) = SPV_NOP;
    }
  }
  else
  {
    for(size_t i = 0; i < count; i++)
    {
      iter.word(idx + i) = SPV_NOP;
    }
  }
}

void SPIRVOperation::nopRemove()
{
  for(size_t i = 0, sz = size(); i < sz; i++)
    iter.word(i) = SPV_NOP;
}

SPIRVScalar::SPIRVScalar(SPIRVIterator it)
{
  type = it.opcode();

  if(type == spv::OpTypeInt || type == spv::OpTypeFloat)
    width = it.word(2);
  else
    width = 0;

  if(type == spv::OpTypeInt)
    signedness = it.word(3) == 1;
  else
    signedness = false;
}

SPIRVOperation SPIRVVector::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeVector, {0U, editor.DeclareType(scalar), count});
}

SPIRVOperation SPIRVMatrix::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeMatrix, {0U, editor.DeclareType(vector), count});
}

SPIRVOperation SPIRVPointer::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypePointer, {0U, (uint32_t)storage, baseId});
}

SPIRVOperation SPIRVImage::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeImage, {0U, editor.DeclareType(retType), (uint32_t)dim, depth,
                                           arrayed, ms, sampled, (uint32_t)format});
}

SPIRVOperation SPIRVSampler::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeSampler, {0U});
}

SPIRVOperation SPIRVSampledImage::decl(SPIRVEditor &editor) const
{
  return SPIRVOperation(spv::OpTypeSampledImage, {0U, baseId});
}

SPIRVOperation SPIRVFunction::decl(SPIRVEditor &editor) const
{
  std::vector<uint32_t> words;

  words.push_back(0U);
  words.push_back(returnId);
  for(SPIRVId id : argumentIds)
    words.push_back(id);

  return SPIRVOperation(spv::OpTypeFunction, words);
}

SPIRVEditor::SPIRVEditor(std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirv.size() < FirstRealWord || spirv[0] != spv::MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    return;
  }

  moduleVersion.major = uint8_t((spirv[1] & 0x00ff0000) >> 16);
  moduleVersion.minor = uint8_t((spirv[1] & 0x0000ff00) >> 8);
  generator = spirv[2];
  idOffsets.resize(spirv[3]);
  idTypes.resize(spirv[3]);

  // [4] is reserved
  RDCASSERT(spirv[4] == 0);

  // simple state machine to track which section we're in.
  // Note that a couple of sections are optional and could be skipped over, at which point we insert
  // a dummy OpNop so they're not empty (which will be stripped later) and record them as in
  // between.
  //
  // We only handle single-shader modules at the moment, so some things are required by virtue of
  // being required in a shader - e.g. at least the Shader capability, at least one entry point, etc
  //
  // Capabilities: REQUIRED (we assume - must declare Shader capability)
  // Extensions: OPTIONAL
  // ExtInst: OPTIONAL
  // MemoryModel: REQUIRED (required by spec)
  // EntryPoints: REQUIRED (we assume)
  // ExecutionMode: OPTIONAL
  // Debug: OPTIONAL
  // Annotations: OPTIONAL (in theory - would require empty shader)
  // TypesVariables: REQUIRED (must at least have the entry point function type)
  // Functions: REQUIRED (must have the entry point)

  // set the book-ends: start of the first section and end of the last
  sections[SPIRVSection::Count - 1].endOffset = spirvWords.size();

#define START_SECTION(section)           \
  if(sections[section].startOffset == 0) \
    sections[section].startOffset = it.offset;

  for(SPIRVIterator it(spirv, FirstRealWord); it; it++)
  {
    spv::Op opcode = it.opcode();

    if(opcode == spv::OpCapability)
    {
      START_SECTION(SPIRVSection::Capabilities);
    }
    else if(opcode == spv::OpExtension)
    {
      START_SECTION(SPIRVSection::Extensions);
    }
    else if(opcode == spv::OpExtInstImport)
    {
      START_SECTION(SPIRVSection::ExtInst);
    }
    else if(opcode == spv::OpMemoryModel)
    {
      START_SECTION(SPIRVSection::MemoryModel);
    }
    else if(opcode == spv::OpEntryPoint)
    {
      START_SECTION(SPIRVSection::EntryPoints);
    }
    else if(opcode == spv::OpExecutionMode || opcode == spv::OpExecutionModeId)
    {
      START_SECTION(SPIRVSection::ExecutionMode);
    }
    else if(opcode == spv::OpString || opcode == spv::OpSource ||
            opcode == spv::OpSourceContinued || opcode == spv::OpSourceExtension ||
            opcode == spv::OpName || opcode == spv::OpMemberName || opcode == spv::OpModuleProcessed)
    {
      START_SECTION(SPIRVSection::Debug);
    }
    else if(opcode == spv::OpDecorate || opcode == spv::OpMemberDecorate ||
            opcode == spv::OpGroupDecorate || opcode == spv::OpGroupMemberDecorate ||
            opcode == spv::OpDecorationGroup || opcode == spv::OpDecorateStringGOOGLE ||
            opcode == spv::OpMemberDecorateStringGOOGLE)
    {
      START_SECTION(SPIRVSection::Annotations);
    }
    else if(opcode == spv::OpFunction)
    {
      START_SECTION(SPIRVSection::Functions);
    }
    else
    {
      // if we've reached another instruction, check if we've reached the function section yet. If
      // we have then assume it's an instruction inside a function and ignore. If we haven't, assume
      // it's a type/variable/constant type instruction
      if(sections[SPIRVSection::Functions].startOffset == 0)
      {
        START_SECTION(SPIRVSection::TypesVariablesConstants);
      }
    }

    RegisterOp(it);
  }

#undef START_SECTION

  // ensure we got everything right. First section should start at the beginning
  RDCASSERTEQUAL(sections[SPIRVSection::First].startOffset, FirstRealWord);

  // we now set the endOffset of each section to the start of the next. Any empty sections
  // temporarily have startOffset set to endOffset, we'll pad them with a nop below.
  for(int s = SPIRVSection::Count - 1; s > 0; s--)
  {
    RDCASSERTEQUAL(sections[s - 1].endOffset, 0);
    sections[s - 1].endOffset = sections[s].startOffset;
    if(sections[s - 1].startOffset == 0)
      sections[s - 1].startOffset = sections[s - 1].endOffset;
  }

  // find any empty sections and insert a nop into the stream there. We need to fixup later section
  // offsets by hand as addWords doesn't handle empty sections properly (it thinks we're inserting
  // into the later section by offset since the offsets overlap). That's why we're adding these
  // padding nops in the first place!
  for(uint32_t s = 0; s < SPIRVSection::Count; s++)
  {
    if(sections[s].startOffset == sections[s].endOffset)
    {
      spirv.insert(spirv.begin() + sections[s].startOffset, SPV_NOP);
      sections[s].endOffset++;

      for(uint32_t t = s + 1; t < SPIRVSection::Count; t++)
      {
        sections[t].startOffset++;
        sections[t].endOffset++;
      }

      // look through every id, and update its offset
      for(size_t &o : idOffsets)
        if(o >= sections[s].startOffset)
          o++;
    }
  }

  // each section should now precisely match each other end-to-end and not be empty
  for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
  {
    RDCASSERTNOTEQUAL(sections[s].startOffset, 0);
    RDCASSERTNOTEQUAL(sections[s].endOffset, 0);

    RDCASSERT(sections[s].endOffset - sections[s].startOffset > 0, sections[s].startOffset,
              sections[s].endOffset);

    if(s != 0)
      RDCASSERTEQUAL(sections[s - 1].endOffset, sections[s].startOffset);

    if(s + 1 < SPIRVSection::Count)
      RDCASSERTEQUAL(sections[s].endOffset, sections[s + 1].startOffset);
  }
}

void SPIRVEditor::StripNops()
{
  for(size_t i = FirstRealWord; i < spirv.size();)
  {
    while(spirv[i] == SPV_NOP)
    {
      spirv.erase(spirv.begin() + i);
      addWords(i, -1);
    }

    uint32_t len = spirv[i] >> spv::WordCountShift;

    if(len == 0)
    {
      RDCERR("Malformed SPIR-V");
      break;
    }

    i += len;
  }
}

SPIRVId SPIRVEditor::MakeId()
{
  uint32_t ret = spirv[3];
  spirv[3]++;
  idOffsets.resize(spirv[3]);
  idTypes.resize(spirv[3]);
  return ret;
}

void SPIRVEditor::SetName(uint32_t id, const char *name)
{
  size_t sz = strlen(name);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], name, sz);

  uintName.insert(uintName.begin(), id);

  SPIRVOperation op(spv::OpName, uintName);

  SPIRVIterator it;

  // OpName must be before OpModuleProcessed.
  for(it = Begin(SPIRVSection::Debug); it < End(SPIRVSection::Debug); ++it)
  {
    if(it.opcode() == spv::OpModuleProcessed)
      break;
  }

  spirv.insert(spirv.begin() + it.offs(), op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, it.offs()));
  addWords(it.offs(), op.size());
}

void SPIRVEditor::AddDecoration(const SPIRVOperation &op)
{
  size_t offs = sections[SPIRVSection::Annotations].endOffset;

  spirv.insert(spirv.begin() + offs, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offs));
  addWords(offs, op.size());
}

void SPIRVEditor::AddCapability(spv::Capability cap)
{
  // don't add duplicate capabilities
  if(capabilities.find(cap) != capabilities.end())
    return;

  // insert the operation at the very start
  SPIRVOperation op(spv::OpCapability, {(uint32_t)cap});
  spirv.insert(spirv.begin() + FirstRealWord, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, FirstRealWord));
  addWords(FirstRealWord, op.size());
}

void SPIRVEditor::AddExtension(const std::string &extension)
{
  // don't add duplicate extensions
  if(extensions.find(extension) != extensions.end())
    return;

  // start at the beginning
  SPIRVIterator it(spirv, FirstRealWord);

  // skip past any capabilities
  while(it.opcode() == spv::OpCapability)
    it++;

  // insert the extension instruction
  size_t sz = extension.size();
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], extension.c_str(), sz);

  SPIRVOperation op(spv::OpExtension, uintName);
  spirv.insert(spirv.begin() + it.offset, op.begin(), op.end());
  RegisterOp(it);
  addWords(it.offset, op.size());
}

void SPIRVEditor::AddExecutionMode(SPIRVId entry, spv::ExecutionMode mode,
                                   std::vector<uint32_t> params)
{
  size_t offset = sections[SPIRVSection::ExecutionMode].endOffset;

  params.insert(params.begin(), (uint32_t)mode);
  params.insert(params.begin(), (uint32_t)entry);

  SPIRVOperation op(spv::OpExecutionMode, params);
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
}

SPIRVId SPIRVEditor::ImportExtInst(const char *setname)
{
  SPIRVId ret = extSets[setname];

  if(ret)
    return ret;

  // start at the beginning
  SPIRVIterator it(spirv, FirstRealWord);

  // skip past any capabilities and extensions
  while(it.opcode() == spv::OpCapability || it.opcode() == spv::OpExtension)
    it++;

  // insert the import instruction
  ret = MakeId();

  size_t sz = strlen(setname);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], setname, sz);

  uintName.insert(uintName.begin(), ret);

  SPIRVOperation op(spv::OpExtInstImport, uintName);
  spirv.insert(spirv.begin() + it.offset, op.begin(), op.end());
  RegisterOp(it);
  addWords(it.offset, op.size());

  extSets[setname] = ret;

  return ret;
}

SPIRVId SPIRVEditor::AddType(const SPIRVOperation &op)
{
  size_t offset = sections[SPIRVSection::Types].endOffset;

  SPIRVId id = op[1];
  idOffsets[id] = offset;
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
  return id;
}

SPIRVId SPIRVEditor::AddVariable(const SPIRVOperation &op)
{
  size_t offset = sections[SPIRVSection::Variables].endOffset;

  SPIRVId id = op[2];
  idOffsets[id] = offset;
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
  return id;
}

SPIRVId SPIRVEditor::AddConstant(const SPIRVOperation &op)
{
  size_t offset = sections[SPIRVSection::Constants].endOffset;

  SPIRVId id = op[2];
  idOffsets[id] = offset;
  spirv.insert(spirv.begin() + offset, op.begin(), op.end());
  RegisterOp(SPIRVIterator(spirv, offset));
  addWords(offset, op.size());
  return id;
}

void SPIRVEditor::AddFunction(const SPIRVOperation *ops, size_t count)
{
  idOffsets[ops[0][2]] = spirv.size();

  for(size_t i = 0; i < count; i++)
    spirv.insert(spirv.end(), ops[i].begin(), ops[i].end());

  RegisterOp(SPIRVIterator(spirv, idOffsets[ops[0][2]]));
}

SPIRVIterator SPIRVEditor::GetID(SPIRVId id)
{
  size_t offs = idOffsets[id];

  if(offs)
    return SPIRVIterator(spirv, offs);

  return SPIRVIterator();
}

SPIRVIterator SPIRVEditor::GetEntry(SPIRVId id)
{
  SPIRVIterator it(spirv, sections[SPIRVSection::EntryPoints].startOffset);
  SPIRVIterator end(spirv, sections[SPIRVSection::EntryPoints].endOffset);

  while(it && it < end)
  {
    if(it.word(2) == id)
      return it;
    it++;
  }

  return SPIRVIterator();
}

SPIRVId SPIRVEditor::DeclareStructType(std::vector<uint32_t> members)
{
  SPIRVId typeId = MakeId();
  members.insert(members.begin(), typeId);
  AddType(SPIRVOperation(spv::OpTypeStruct, members));
  return typeId;
}

void SPIRVEditor::AddWord(SPIRVIterator iter, uint32_t word)
{
  if(!iter)
    return;

  // if it's just pointing at a SPIRVOperation, we can just push_back immediately
  if(iter.words != &spirv)
  {
    iter.words->push_back(word);
    return;
  }

  // add word
  spirv.insert(spirv.begin() + iter.offset + iter.size(), word);

  // fix up header
  iter.word(0) = SPIRVOperation::MakeHeader(iter.opcode(), iter.size() + 1);

  // update offsets
  addWords(iter.offset + iter.size(), 1);
}

void SPIRVEditor::AddOperation(SPIRVIterator iter, const SPIRVOperation &op)
{
  if(!iter)
    return;

  // if it's just pointing at a SPIRVOperation, this is invalid
  if(iter.words != &spirv)
    return;

  // add op
  spirv.insert(spirv.begin() + iter.offset, op.begin(), op.end());

  // update offsets
  addWords(iter.offset, op.size());
}

void SPIRVEditor::RegisterOp(SPIRVIterator it)
{
  spv::Op opcode = it.opcode();

  {
    bool hasResult = false, hasResultType = false;
    spv::HasResultAndType(opcode, &hasResult, &hasResultType);

    if(hasResult && hasResultType)
    {
      RDCASSERT(it.word(2) < idTypes.size());
      idTypes[it.word(2)] = it.word(1);
    }
  }

  if(opcode == spv::OpEntryPoint)
  {
    SPIRVEntry entry;
    entry.id = it.word(2);
    entry.name = (const char *)&it.word(3);

    entries.push_back(entry);
  }
  else if(opcode == spv::OpMemoryModel)
  {
    addressmodel = (spv::AddressingModel)it.word(2);
    memorymodel = (spv::MemoryModel)it.word(3);
  }
  else if(opcode == spv::OpCapability)
  {
    capabilities.insert((spv::Capability)it.word(1));
  }
  else if(opcode == spv::OpExtension)
  {
    const char *name = (const char *)&it.word(1);
    extensions.insert(name);
  }
  else if(opcode == spv::OpExtInstImport)
  {
    SPIRVId id = it.word(1);
    const char *name = (const char *)&it.word(2);
    extSets[name] = id;
  }
  else if(opcode == spv::OpFunction)
  {
    SPIRVId id = it.word(2);
    idOffsets[id] = it.offset;

    functions.push_back(id);
  }
  else if(opcode == spv::OpVariable)
  {
    SPIRVVariable var;
    var.type = it.word(1);
    var.id = it.word(2);
    var.storageClass = (spv::StorageClass)it.word(3);
    if(it.size() > 4)
      var.init = it.word(4);

    variables.push_back(var);
  }
  else if(opcode == spv::OpDecorate)
  {
    SPIRVDecoration decoration;
    decoration.id = it.word(1);
    decoration.dec = (spv::Decoration)it.word(2);

    RDCASSERTMSG("Too many parameters in decoration", it.size() <= 7, it.size());

    for(size_t i = 0; i + 3 < it.size() && i < ARRAY_COUNT(decoration.parameters); i++)
      decoration.parameters[i] = it.word(i + 3);

    auto it = std::lower_bound(decorations.begin(), decorations.end(), decoration);
    decorations.insert(it, decoration);

    if(decoration.dec == spv::DecorationDescriptorSet)
      bindings[decoration.id].set = decoration.parameters[0];
    if(decoration.dec == spv::DecorationBinding)
      bindings[decoration.id].binding = decoration.parameters[0];
  }
  else if(opcode == spv::OpTypeVoid || opcode == spv::OpTypeBool || opcode == spv::OpTypeInt ||
          opcode == spv::OpTypeFloat)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVScalar scalar(it);
    scalarTypes[scalar] = id;
  }
  else if(opcode == spv::OpTypeVector)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    vectorTypes[SPIRVVector(scalarIt, it.word(3))] = id;
  }
  else if(opcode == spv::OpTypeMatrix)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVIterator vectorIt = GetID(it.word(2));

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
      return;
    }

    SPIRVIterator scalarIt = GetID(vectorIt.word(2));
    uint32_t vectorDim = vectorIt.word(3);

    matrixTypes[SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3))] = id;
  }
  else if(opcode == spv::OpTypeImage)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    imageTypes[SPIRVImage(scalarIt, (spv::Dim)it.word(3), it.word(4), it.word(5), it.word(6),
                          it.word(7), (spv::ImageFormat)it.word(8))] = id;
  }
  else if(opcode == spv::OpTypeSampler)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    samplerTypes[SPIRVSampler()] = id;
  }
  else if(opcode == spv::OpTypeSampledImage)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    SPIRVId base = it.word(2);

    sampledImageTypes[SPIRVSampledImage(base)] = id;
  }
  else if(opcode == spv::OpTypePointer)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    pointerTypes[SPIRVPointer(it.word(3), (spv::StorageClass)it.word(2))] = id;
  }
  else if(opcode == spv::OpTypeStruct)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    structTypes.insert(id);
  }
  else if(opcode == spv::OpTypeFunction)
  {
    SPIRVId id = it.word(1);
    idOffsets[id] = it.offset;

    std::vector<SPIRVId> args;

    for(size_t i = 3; i < it.size(); i++)
      args.push_back(it.word(i));

    functionTypes[SPIRVFunction(it.word(2), args)] = id;
  }
}

void SPIRVEditor::UnregisterOp(SPIRVIterator it)
{
  spv::Op opcode = it.opcode();

  {
    bool hasResult = false, hasResultType = false;
    spv::HasResultAndType(opcode, &hasResult, &hasResultType);

    if(hasResult && hasResultType)
      idTypes[it.word(2)] = 0;
  }

  SPIRVId id;

  if(opcode == spv::OpEntryPoint)
  {
    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == it.word(2))
      {
        entries.erase(entryIt);
        break;
      }
    }
  }
  else if(opcode == spv::OpFunction)
  {
    id = it.word(2);
    for(auto funcIt = functions.begin(); funcIt != functions.end(); ++funcIt)
    {
      if(*funcIt == id)
      {
        functions.erase(funcIt);
        break;
      }
    }
  }
  else if(opcode == spv::OpVariable)
  {
    id = it.word(2);
    for(auto varIt = variables.begin(); varIt != variables.end(); ++varIt)
    {
      if(varIt->id == id)
      {
        variables.erase(varIt);
        break;
      }
    }
  }
  else if(opcode == spv::OpDecorate)
  {
    SPIRVDecoration decoration;
    decoration.id = it.word(1);
    decoration.dec = (spv::Decoration)it.word(2);

    RDCASSERTMSG("Too many parameters in decoration", it.size() <= 7, it.size());

    for(size_t i = 0; i + 3 < it.size() && i < ARRAY_COUNT(decoration.parameters); i++)
      decoration.parameters[i] = it.word(i + 3);

    auto it = std::find(decorations.begin(), decorations.end(), decoration);
    if(it != decorations.end())
      decorations.erase(it);

    if(decoration.dec == spv::DecorationDescriptorSet)
      bindings[decoration.id].set = SPIRVBinding().set;
    if(decoration.dec == spv::DecorationBinding)
      bindings[decoration.id].binding = SPIRVBinding().binding;
  }
  else if(opcode == spv::OpCapability)
  {
    capabilities.erase((spv::Capability)it.word(1));
  }
  else if(opcode == spv::OpExtension)
  {
    const char *name = (const char *)&it.word(1);
    extensions.erase(name);
  }
  else if(opcode == spv::OpExtInstImport)
  {
    const char *name = (const char *)&it.word(2);
    extSets.erase(name);
  }
  else if(opcode == spv::OpTypeVoid || opcode == spv::OpTypeBool || opcode == spv::OpTypeInt ||
          opcode == spv::OpTypeFloat)
  {
    id = it.word(1);

    SPIRVScalar scalar(it);
    scalarTypes.erase(scalar);
  }
  else if(opcode == spv::OpTypeVector)
  {
    id = it.word(1);

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    vectorTypes.erase(SPIRVVector(scalarIt, it.word(3)));
  }
  else if(opcode == spv::OpTypeMatrix)
  {
    id = it.word(1);

    SPIRVIterator vectorIt = GetID(it.word(2));

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", it.word(2));
      return;
    }

    SPIRVIterator scalarIt = GetID(vectorIt.word(2));
    uint32_t vectorDim = vectorIt.word(3);

    matrixTypes.erase(SPIRVMatrix(SPIRVVector(scalarIt, vectorDim), it.word(3)));
  }
  else if(opcode == spv::OpTypeImage)
  {
    id = it.word(1);

    SPIRVIterator scalarIt = GetID(it.word(2));

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", it.word(2));
      return;
    }

    imageTypes.erase(SPIRVImage(scalarIt, (spv::Dim)it.word(3), it.word(4), it.word(5), it.word(6),
                                it.word(7), (spv::ImageFormat)it.word(8)));
  }
  else if(opcode == spv::OpTypeSampler)
  {
    id = it.word(1);

    samplerTypes.erase(SPIRVSampler());
  }
  else if(opcode == spv::OpTypeSampledImage)
  {
    id = it.word(1);

    SPIRVId base = it.word(2);

    sampledImageTypes.erase(SPIRVSampledImage(base));
  }
  else if(opcode == spv::OpTypePointer)
  {
    id = it.word(1);

    pointerTypes.erase(SPIRVPointer(it.word(3), (spv::StorageClass)it.word(2)));
  }
  else if(opcode == spv::OpTypeStruct)
  {
    id = it.word(1);

    structTypes.erase(id);
  }
  else if(opcode == spv::OpTypeFunction)
  {
    id = it.word(1);

    std::vector<SPIRVId> args;

    for(size_t i = 3; i < it.size(); i++)
      args.push_back(it.word(i));

    functionTypes.erase(SPIRVFunction(it.word(2), args));
  }

  if(id)
    idOffsets[id] = 0;
}

void SPIRVEditor::addWords(size_t offs, int32_t num)
{
  // look through every section, any that are >= this point, adjust the offsets
  // note that if we're removing words then any offsets pointing directly to the removed words
  // will go backwards - but they no longer have anywhere valid to point.
  for(LogicalSection &section : sections)
  {
    // we have three cases to consider: either the offset matches start, is within (up to and
    // including end) or is outside the section.
    // We ensured during parsing that all sections were non-empty by adding nops if necessary, so we
    // don't have to worry about the situation where we can't decide if an insert is at the end of
    // one section or inside the next. Note this means we don't support inserting at the start of a
    // section.

    if(offs == section.startOffset)
    {
      // if the offset matches the start, we're appending at the end of the previous section so move
      // both
      section.startOffset += num;
      section.endOffset += num;
    }
    else if(offs > section.startOffset && offs <= section.endOffset)
    {
      // if the offset is in the section (up to and including the end) then we're inserting in this
      // section, so move the end only
      section.endOffset += num;
    }
    else if(section.startOffset >= offs)
    {
      // otherwise move both or neither depending on which side the offset is.
      section.startOffset += num;
      section.endOffset += num;
    }
  }

  // look through every id, and do the same
  for(size_t &o : idOffsets)
    if(o >= offs)
      o += num;
}

#define TYPETABLE(StructType, variable)                                          \
  template <>                                                                    \
  std::map<StructType, SPIRVId> &SPIRVEditor::GetTable<StructType>()             \
  {                                                                              \
    return variable;                                                             \
  }                                                                              \
  template <>                                                                    \
  const std::map<StructType, SPIRVId> &SPIRVEditor::GetTable<StructType>() const \
  {                                                                              \
    return variable;                                                             \
  }

TYPETABLE(SPIRVScalar, scalarTypes);
TYPETABLE(SPIRVVector, vectorTypes);
TYPETABLE(SPIRVMatrix, matrixTypes);
TYPETABLE(SPIRVPointer, pointerTypes);
TYPETABLE(SPIRVImage, imageTypes);
TYPETABLE(SPIRVSampler, samplerTypes);
TYPETABLE(SPIRVSampledImage, sampledImageTypes);
TYPETABLE(SPIRVFunction, functionTypes);

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"
#include "core/core.h"
#include "spirv_common.h"

static void RemoveSection(std::vector<uint32_t> &spirv, size_t offsets[SPIRVSection::Count][2],
                          SPIRVSection::Type section)
{
  SPIRVEditor ed(spirv);

  for(SPIRVIterator it = ed.Begin(section), end = ed.End(section); it < end; it++)
    ed.Remove(it);

  size_t oldLength = offsets[section][1] - offsets[section][0];

  // section will still contain a nop
  offsets[section][1] = offsets[section][0] + 4;

  // subsequent sections will be shorter by the length - 4, because a nop will still be inserted
  // as padding to ensure no section is truly empty.
  size_t delta = oldLength - 4;

  for(uint32_t s = section + 1; s < SPIRVSection::Count; s++)
  {
    offsets[s][0] -= delta;
    offsets[s][1] -= delta;
  }
}

static void CheckSPIRV(SPIRVEditor &ed, size_t offsets[SPIRVSection::Count][2])
{
  for(uint32_t s = SPIRVSection::First; s < SPIRVSection::Count; s++)
  {
    INFO("Section " << s);
    CHECK(ed.Begin((SPIRVSection::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
    CHECK(ed.End((SPIRVSection::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
  }

  // should only be one entry point
  REQUIRE(ed.GetEntries().size() == 1);

  SPIRVId entryId = ed.GetEntries()[0].id;

  // check that the iterator places us precisely at the start of the functions section
  CHECK(ed.GetID(entryId).offs() == ed.Begin(SPIRVSection::Functions).offs());
}

TEST_CASE("Test SPIR-V editor section handling", "[spirv]")
{
  InitSPIRVCompiler();
  RenderDoc::Inst().RegisterShutdownFunction(&ShutdownSPIRVCompiler);

  SPIRVCompilationSettings settings;
  settings.entryPoint = "main";
  settings.lang = SPIRVSourceLanguage::VulkanGLSL;
  settings.stage = SPIRVShaderStage::Fragment;

  // simple shader that has at least something in every section
  std::vector<std::string> sources = {
      R"(#version 450 core

#extension GL_EXT_shader_16bit_storage : require

layout(binding = 0) uniform block {
	float16_t val;
};

layout(location = 0) out vec4 col;

void main() {
  col = vec4(sin(gl_FragCoord.x)*float(val), 0, 0, 1);
}
)",
  };

  std::vector<uint32_t> spirv;
  std::string errors = CompileSPIRV(settings, sources, spirv);

  INFO("SPIR-V compilation - " << errors);

  // ensure that compilation succeeded
  REQUIRE(spirv.size() > 0);

  // these offsets may change if the compiler changes above. Verify manually with spirv-dis that
  // they should be updated.
  // For convenience the offsets are in bytes (which spirv-dis uses) and are converted in the loop
  // in CheckSPIRV.
  size_t offsets[SPIRVSection::Count][2] = {
      // Capabilities
      {0x14, 0x24},
      // Extensions
      {0x24, 0x40},
      // ExtInst
      {0x40, 0x58},
      // MemoryModel
      {0x58, 0x64},
      // EntryPoints
      {0x64, 0x80},
      // ExecutionMode
      {0x80, 0x8c},
      // Debug
      {0x8c, 0x118},
      // Annotations
      {0x118, 0x178},
      // TypesVariables
      {0x178, 0x2a0},
      // Functions
      {0x2a0, 0x370},
  };

  SECTION("Check that SPIR-V is correct with no changes")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  // we remove all sections we consider optional in arbitrary order. We don't care about keeping the
  // SPIR-V valid all we're testing is the section offsets are correct.
  RemoveSection(spirv, offsets, SPIRVSection::Extensions);

  SECTION("Check with extensions removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::Debug);

  SECTION("Check with debug removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::ExtInst);

  SECTION("Check with extension imports removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::ExecutionMode);

  SECTION("Check with execution mode removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, SPIRVSection::Annotations);

  SECTION("Check with annotations removed")
  {
    SPIRVEditor ed(spirv);

    CheckSPIRV(ed, offsets);
  }
}

#endif