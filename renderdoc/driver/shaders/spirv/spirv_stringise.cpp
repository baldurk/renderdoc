/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"

template <>
rdcstr DoStringise(const spv::Op &el)
{
  BEGIN_ENUM_STRINGISE(spv::Op)
  {
    STRINGISE_ENUM_NAMED(spv::OpNop, "Nop");
    STRINGISE_ENUM_NAMED(spv::OpUndef, "Undef");
    STRINGISE_ENUM_NAMED(spv::OpSourceContinued, "SourceContinued");
    STRINGISE_ENUM_NAMED(spv::OpSource, "Source");
    STRINGISE_ENUM_NAMED(spv::OpSourceExtension, "SourceExtension");
    STRINGISE_ENUM_NAMED(spv::OpName, "Name");
    STRINGISE_ENUM_NAMED(spv::OpMemberName, "MemberName");
    STRINGISE_ENUM_NAMED(spv::OpString, "String");
    STRINGISE_ENUM_NAMED(spv::OpLine, "Line");
    STRINGISE_ENUM_NAMED(spv::OpExtension, "Extension");
    STRINGISE_ENUM_NAMED(spv::OpExtInstImport, "ExtInstImport");
    STRINGISE_ENUM_NAMED(spv::OpExtInst, "ExtInst");
    STRINGISE_ENUM_NAMED(spv::OpMemoryModel, "MemoryModel");
    STRINGISE_ENUM_NAMED(spv::OpEntryPoint, "EntryPoint");
    STRINGISE_ENUM_NAMED(spv::OpExecutionMode, "ExecutionMode");
    STRINGISE_ENUM_NAMED(spv::OpCapability, "Capability");
    STRINGISE_ENUM_NAMED(spv::OpTypeVoid, "TypeVoid");
    STRINGISE_ENUM_NAMED(spv::OpTypeBool, "TypeBool");
    STRINGISE_ENUM_NAMED(spv::OpTypeInt, "TypeInt");
    STRINGISE_ENUM_NAMED(spv::OpTypeFloat, "TypeFloat");
    STRINGISE_ENUM_NAMED(spv::OpTypeVector, "TypeVector");
    STRINGISE_ENUM_NAMED(spv::OpTypeMatrix, "TypeMatrix");
    STRINGISE_ENUM_NAMED(spv::OpTypeImage, "TypeImage");
    STRINGISE_ENUM_NAMED(spv::OpTypeSampler, "TypeSampler");
    STRINGISE_ENUM_NAMED(spv::OpTypeSampledImage, "TypeSampledImage");
    STRINGISE_ENUM_NAMED(spv::OpTypeArray, "TypeArray");
    STRINGISE_ENUM_NAMED(spv::OpTypeRuntimeArray, "TypeRuntimeArray");
    STRINGISE_ENUM_NAMED(spv::OpTypeStruct, "TypeStruct");
    STRINGISE_ENUM_NAMED(spv::OpTypeOpaque, "TypeOpaque");
    STRINGISE_ENUM_NAMED(spv::OpTypePointer, "TypePointer");
    STRINGISE_ENUM_NAMED(spv::OpTypeFunction, "TypeFunction");
    STRINGISE_ENUM_NAMED(spv::OpTypeEvent, "TypeEvent");
    STRINGISE_ENUM_NAMED(spv::OpTypeDeviceEvent, "TypeDeviceEvent");
    STRINGISE_ENUM_NAMED(spv::OpTypeReserveId, "TypeReserveId");
    STRINGISE_ENUM_NAMED(spv::OpTypeQueue, "TypeQueue");
    STRINGISE_ENUM_NAMED(spv::OpTypePipe, "TypePipe");
    STRINGISE_ENUM_NAMED(spv::OpTypeForwardPointer, "TypeForwardPointer");
    STRINGISE_ENUM_NAMED(spv::OpConstantTrue, "ConstantTrue");
    STRINGISE_ENUM_NAMED(spv::OpConstantFalse, "ConstantFalse");
    STRINGISE_ENUM_NAMED(spv::OpConstant, "Constant");
    STRINGISE_ENUM_NAMED(spv::OpConstantComposite, "ConstantComposite");
    STRINGISE_ENUM_NAMED(spv::OpConstantSampler, "ConstantSampler");
    STRINGISE_ENUM_NAMED(spv::OpConstantNull, "ConstantNull");
    STRINGISE_ENUM_NAMED(spv::OpSpecConstantTrue, "SpecConstantTrue");
    STRINGISE_ENUM_NAMED(spv::OpSpecConstantFalse, "SpecConstantFalse");
    STRINGISE_ENUM_NAMED(spv::OpSpecConstant, "SpecConstant");
    STRINGISE_ENUM_NAMED(spv::OpSpecConstantComposite, "SpecConstantComposite");
    STRINGISE_ENUM_NAMED(spv::OpSpecConstantOp, "SpecConstantOp");
    STRINGISE_ENUM_NAMED(spv::OpFunction, "Function");
    STRINGISE_ENUM_NAMED(spv::OpFunctionParameter, "FunctionParameter");
    STRINGISE_ENUM_NAMED(spv::OpFunctionEnd, "FunctionEnd");
    STRINGISE_ENUM_NAMED(spv::OpFunctionCall, "FunctionCall");
    STRINGISE_ENUM_NAMED(spv::OpVariable, "Variable");
    STRINGISE_ENUM_NAMED(spv::OpImageTexelPointer, "ImageTexelPointer");
    STRINGISE_ENUM_NAMED(spv::OpLoad, "Load");
    STRINGISE_ENUM_NAMED(spv::OpStore, "Store");
    STRINGISE_ENUM_NAMED(spv::OpCopyMemory, "CopyMemory");
    STRINGISE_ENUM_NAMED(spv::OpCopyMemorySized, "CopyMemorySized");
    STRINGISE_ENUM_NAMED(spv::OpAccessChain, "AccessChain");
    STRINGISE_ENUM_NAMED(spv::OpInBoundsAccessChain, "InBoundsAccessChain");
    STRINGISE_ENUM_NAMED(spv::OpPtrAccessChain, "PtrAccessChain");
    STRINGISE_ENUM_NAMED(spv::OpArrayLength, "ArrayLength");
    STRINGISE_ENUM_NAMED(spv::OpGenericPtrMemSemantics, "GenericPtrMemSemantics");
    STRINGISE_ENUM_NAMED(spv::OpInBoundsPtrAccessChain, "InBoundsPtrAccessChain");
    STRINGISE_ENUM_NAMED(spv::OpDecorate, "Decorate");
    STRINGISE_ENUM_NAMED(spv::OpMemberDecorate, "MemberDecorate");
    STRINGISE_ENUM_NAMED(spv::OpDecorationGroup, "DecorationGroup");
    STRINGISE_ENUM_NAMED(spv::OpGroupDecorate, "GroupDecorate");
    STRINGISE_ENUM_NAMED(spv::OpGroupMemberDecorate, "GroupMemberDecorate");
    STRINGISE_ENUM_NAMED(spv::OpVectorExtractDynamic, "VectorExtractDynamic");
    STRINGISE_ENUM_NAMED(spv::OpVectorInsertDynamic, "VectorInsertDynamic");
    STRINGISE_ENUM_NAMED(spv::OpVectorShuffle, "VectorShuffle");
    STRINGISE_ENUM_NAMED(spv::OpCompositeConstruct, "CompositeConstruct");
    STRINGISE_ENUM_NAMED(spv::OpCompositeExtract, "CompositeExtract");
    STRINGISE_ENUM_NAMED(spv::OpCompositeInsert, "CompositeInsert");
    STRINGISE_ENUM_NAMED(spv::OpCopyObject, "CopyObject");
    STRINGISE_ENUM_NAMED(spv::OpTranspose, "Transpose");
    STRINGISE_ENUM_NAMED(spv::OpSampledImage, "SampledImage");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleImplicitLod, "ImageSampleImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleExplicitLod, "ImageSampleExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleDrefImplicitLod, "ImageSampleDrefImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleDrefExplicitLod, "ImageSampleDrefExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleProjImplicitLod, "ImageSampleProjImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleProjExplicitLod, "ImageSampleProjExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleProjDrefImplicitLod, "ImageSampleProjDrefImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleProjDrefExplicitLod, "ImageSampleProjDrefExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageFetch, "ImageFetch");
    STRINGISE_ENUM_NAMED(spv::OpImageGather, "ImageGather");
    STRINGISE_ENUM_NAMED(spv::OpImageDrefGather, "ImageDrefGather");
    STRINGISE_ENUM_NAMED(spv::OpImageRead, "ImageRead");
    STRINGISE_ENUM_NAMED(spv::OpImageWrite, "ImageWrite");
    STRINGISE_ENUM_NAMED(spv::OpImage, "Image");
    STRINGISE_ENUM_NAMED(spv::OpImageQueryFormat, "ImageQueryFormat");
    STRINGISE_ENUM_NAMED(spv::OpImageQueryOrder, "ImageQueryOrder");
    STRINGISE_ENUM_NAMED(spv::OpImageQuerySizeLod, "ImageQuerySizeLod");
    STRINGISE_ENUM_NAMED(spv::OpImageQuerySize, "ImageQuerySize");
    STRINGISE_ENUM_NAMED(spv::OpImageQueryLod, "ImageQueryLod");
    STRINGISE_ENUM_NAMED(spv::OpImageQueryLevels, "ImageQueryLevels");
    STRINGISE_ENUM_NAMED(spv::OpImageQuerySamples, "ImageQuerySamples");
    STRINGISE_ENUM_NAMED(spv::OpConvertFToU, "ConvertFToU");
    STRINGISE_ENUM_NAMED(spv::OpConvertFToS, "ConvertFToS");
    STRINGISE_ENUM_NAMED(spv::OpConvertSToF, "ConvertSToF");
    STRINGISE_ENUM_NAMED(spv::OpConvertUToF, "ConvertUToF");
    STRINGISE_ENUM_NAMED(spv::OpUConvert, "UConvert");
    STRINGISE_ENUM_NAMED(spv::OpSConvert, "SConvert");
    STRINGISE_ENUM_NAMED(spv::OpFConvert, "FConvert");
    STRINGISE_ENUM_NAMED(spv::OpQuantizeToF16, "QuantizeToF16");
    STRINGISE_ENUM_NAMED(spv::OpConvertPtrToU, "ConvertPtrToU");
    STRINGISE_ENUM_NAMED(spv::OpSatConvertSToU, "SatConvertSToU");
    STRINGISE_ENUM_NAMED(spv::OpSatConvertUToS, "SatConvertUToS");
    STRINGISE_ENUM_NAMED(spv::OpConvertUToPtr, "ConvertUToPtr");
    STRINGISE_ENUM_NAMED(spv::OpPtrCastToGeneric, "PtrCastToGeneric");
    STRINGISE_ENUM_NAMED(spv::OpGenericCastToPtr, "GenericCastToPtr");
    STRINGISE_ENUM_NAMED(spv::OpGenericCastToPtrExplicit, "GenericCastToPtrExplicit");
    STRINGISE_ENUM_NAMED(spv::OpBitcast, "Bitcast");
    STRINGISE_ENUM_NAMED(spv::OpSNegate, "SNegate");
    STRINGISE_ENUM_NAMED(spv::OpFNegate, "FNegate");
    STRINGISE_ENUM_NAMED(spv::OpIAdd, "IAdd");
    STRINGISE_ENUM_NAMED(spv::OpFAdd, "FAdd");
    STRINGISE_ENUM_NAMED(spv::OpISub, "ISub");
    STRINGISE_ENUM_NAMED(spv::OpFSub, "FSub");
    STRINGISE_ENUM_NAMED(spv::OpIMul, "IMul");
    STRINGISE_ENUM_NAMED(spv::OpFMul, "FMul");
    STRINGISE_ENUM_NAMED(spv::OpUDiv, "UDiv");
    STRINGISE_ENUM_NAMED(spv::OpSDiv, "SDiv");
    STRINGISE_ENUM_NAMED(spv::OpFDiv, "FDiv");
    STRINGISE_ENUM_NAMED(spv::OpUMod, "UMod");
    STRINGISE_ENUM_NAMED(spv::OpSRem, "SRem");
    STRINGISE_ENUM_NAMED(spv::OpSMod, "SMod");
    STRINGISE_ENUM_NAMED(spv::OpFRem, "FRem");
    STRINGISE_ENUM_NAMED(spv::OpFMod, "FMod");
    STRINGISE_ENUM_NAMED(spv::OpVectorTimesScalar, "VectorTimesScalar");
    STRINGISE_ENUM_NAMED(spv::OpMatrixTimesScalar, "MatrixTimesScalar");
    STRINGISE_ENUM_NAMED(spv::OpVectorTimesMatrix, "VectorTimesMatrix");
    STRINGISE_ENUM_NAMED(spv::OpMatrixTimesVector, "MatrixTimesVector");
    STRINGISE_ENUM_NAMED(spv::OpMatrixTimesMatrix, "MatrixTimesMatrix");
    STRINGISE_ENUM_NAMED(spv::OpOuterProduct, "OuterProduct");
    STRINGISE_ENUM_NAMED(spv::OpDot, "Dot");
    STRINGISE_ENUM_NAMED(spv::OpIAddCarry, "IAddCarry");
    STRINGISE_ENUM_NAMED(spv::OpISubBorrow, "ISubBorrow");
    STRINGISE_ENUM_NAMED(spv::OpUMulExtended, "UMulExtended");
    STRINGISE_ENUM_NAMED(spv::OpSMulExtended, "SMulExtended");
    STRINGISE_ENUM_NAMED(spv::OpAny, "Any");
    STRINGISE_ENUM_NAMED(spv::OpAll, "All");
    STRINGISE_ENUM_NAMED(spv::OpIsNan, "IsNan");
    STRINGISE_ENUM_NAMED(spv::OpIsInf, "IsInf");
    STRINGISE_ENUM_NAMED(spv::OpIsFinite, "IsFinite");
    STRINGISE_ENUM_NAMED(spv::OpIsNormal, "IsNormal");
    STRINGISE_ENUM_NAMED(spv::OpSignBitSet, "SignBitSet");
    STRINGISE_ENUM_NAMED(spv::OpLessOrGreater, "LessOrGreater");
    STRINGISE_ENUM_NAMED(spv::OpOrdered, "Ordered");
    STRINGISE_ENUM_NAMED(spv::OpUnordered, "Unordered");
    STRINGISE_ENUM_NAMED(spv::OpLogicalEqual, "LogicalEqual");
    STRINGISE_ENUM_NAMED(spv::OpLogicalNotEqual, "LogicalNotEqual");
    STRINGISE_ENUM_NAMED(spv::OpLogicalOr, "LogicalOr");
    STRINGISE_ENUM_NAMED(spv::OpLogicalAnd, "LogicalAnd");
    STRINGISE_ENUM_NAMED(spv::OpLogicalNot, "LogicalNot");
    STRINGISE_ENUM_NAMED(spv::OpSelect, "Select");
    STRINGISE_ENUM_NAMED(spv::OpIEqual, "IEqual");
    STRINGISE_ENUM_NAMED(spv::OpINotEqual, "INotEqual");
    STRINGISE_ENUM_NAMED(spv::OpUGreaterThan, "UGreaterThan");
    STRINGISE_ENUM_NAMED(spv::OpSGreaterThan, "SGreaterThan");
    STRINGISE_ENUM_NAMED(spv::OpUGreaterThanEqual, "UGreaterThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpSGreaterThanEqual, "SGreaterThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpULessThan, "ULessThan");
    STRINGISE_ENUM_NAMED(spv::OpSLessThan, "SLessThan");
    STRINGISE_ENUM_NAMED(spv::OpULessThanEqual, "ULessThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpSLessThanEqual, "SLessThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpFOrdEqual, "FOrdEqual");
    STRINGISE_ENUM_NAMED(spv::OpFUnordEqual, "FUnordEqual");
    STRINGISE_ENUM_NAMED(spv::OpFOrdNotEqual, "FOrdNotEqual");
    STRINGISE_ENUM_NAMED(spv::OpFUnordNotEqual, "FUnordNotEqual");
    STRINGISE_ENUM_NAMED(spv::OpFOrdLessThan, "FOrdLessThan");
    STRINGISE_ENUM_NAMED(spv::OpFUnordLessThan, "FUnordLessThan");
    STRINGISE_ENUM_NAMED(spv::OpFOrdGreaterThan, "FOrdGreaterThan");
    STRINGISE_ENUM_NAMED(spv::OpFUnordGreaterThan, "FUnordGreaterThan");
    STRINGISE_ENUM_NAMED(spv::OpFOrdLessThanEqual, "FOrdLessThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpFUnordLessThanEqual, "FUnordLessThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpFOrdGreaterThanEqual, "FOrdGreaterThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpFUnordGreaterThanEqual, "FUnordGreaterThanEqual");
    STRINGISE_ENUM_NAMED(spv::OpShiftRightLogical, "ShiftRightLogical");
    STRINGISE_ENUM_NAMED(spv::OpShiftRightArithmetic, "ShiftRightArithmetic");
    STRINGISE_ENUM_NAMED(spv::OpShiftLeftLogical, "ShiftLeftLogical");
    STRINGISE_ENUM_NAMED(spv::OpBitwiseOr, "BitwiseOr");
    STRINGISE_ENUM_NAMED(spv::OpBitwiseXor, "BitwiseXor");
    STRINGISE_ENUM_NAMED(spv::OpBitwiseAnd, "BitwiseAnd");
    STRINGISE_ENUM_NAMED(spv::OpNot, "Not");
    STRINGISE_ENUM_NAMED(spv::OpBitFieldInsert, "BitFieldInsert");
    STRINGISE_ENUM_NAMED(spv::OpBitFieldSExtract, "BitFieldSExtract");
    STRINGISE_ENUM_NAMED(spv::OpBitFieldUExtract, "BitFieldUExtract");
    STRINGISE_ENUM_NAMED(spv::OpBitReverse, "BitReverse");
    STRINGISE_ENUM_NAMED(spv::OpBitCount, "BitCount");
    STRINGISE_ENUM_NAMED(spv::OpDPdx, "DPdx");
    STRINGISE_ENUM_NAMED(spv::OpDPdy, "DPdy");
    STRINGISE_ENUM_NAMED(spv::OpFwidth, "Fwidth");
    STRINGISE_ENUM_NAMED(spv::OpDPdxFine, "DPdxFine");
    STRINGISE_ENUM_NAMED(spv::OpDPdyFine, "DPdyFine");
    STRINGISE_ENUM_NAMED(spv::OpFwidthFine, "FwidthFine");
    STRINGISE_ENUM_NAMED(spv::OpDPdxCoarse, "DPdxCoarse");
    STRINGISE_ENUM_NAMED(spv::OpDPdyCoarse, "DPdyCoarse");
    STRINGISE_ENUM_NAMED(spv::OpFwidthCoarse, "FwidthCoarse");
    STRINGISE_ENUM_NAMED(spv::OpEmitVertex, "EmitVertex");
    STRINGISE_ENUM_NAMED(spv::OpEndPrimitive, "EndPrimitive");
    STRINGISE_ENUM_NAMED(spv::OpEmitStreamVertex, "EmitStreamVertex");
    STRINGISE_ENUM_NAMED(spv::OpEndStreamPrimitive, "EndStreamPrimitive");
    STRINGISE_ENUM_NAMED(spv::OpControlBarrier, "ControlBarrier");
    STRINGISE_ENUM_NAMED(spv::OpMemoryBarrier, "MemoryBarrier");
    STRINGISE_ENUM_NAMED(spv::OpAtomicLoad, "AtomicLoad");
    STRINGISE_ENUM_NAMED(spv::OpAtomicStore, "AtomicStore");
    STRINGISE_ENUM_NAMED(spv::OpAtomicExchange, "AtomicExchange");
    STRINGISE_ENUM_NAMED(spv::OpAtomicCompareExchange, "AtomicCompareExchange");
    STRINGISE_ENUM_NAMED(spv::OpAtomicCompareExchangeWeak, "AtomicCompareExchangeWeak");
    STRINGISE_ENUM_NAMED(spv::OpAtomicIIncrement, "AtomicIIncrement");
    STRINGISE_ENUM_NAMED(spv::OpAtomicIDecrement, "AtomicIDecrement");
    STRINGISE_ENUM_NAMED(spv::OpAtomicIAdd, "AtomicIAdd");
    STRINGISE_ENUM_NAMED(spv::OpAtomicISub, "AtomicISub");
    STRINGISE_ENUM_NAMED(spv::OpAtomicSMin, "AtomicSMin");
    STRINGISE_ENUM_NAMED(spv::OpAtomicUMin, "AtomicUMin");
    STRINGISE_ENUM_NAMED(spv::OpAtomicSMax, "AtomicSMax");
    STRINGISE_ENUM_NAMED(spv::OpAtomicUMax, "AtomicUMax");
    STRINGISE_ENUM_NAMED(spv::OpAtomicAnd, "AtomicAnd");
    STRINGISE_ENUM_NAMED(spv::OpAtomicOr, "AtomicOr");
    STRINGISE_ENUM_NAMED(spv::OpAtomicXor, "AtomicXor");
    STRINGISE_ENUM_NAMED(spv::OpPhi, "Phi");
    STRINGISE_ENUM_NAMED(spv::OpLoopMerge, "LoopMerge");
    STRINGISE_ENUM_NAMED(spv::OpSelectionMerge, "SelectionMerge");
    STRINGISE_ENUM_NAMED(spv::OpLabel, "Label");
    STRINGISE_ENUM_NAMED(spv::OpBranch, "Branch");
    STRINGISE_ENUM_NAMED(spv::OpBranchConditional, "BranchConditional");
    STRINGISE_ENUM_NAMED(spv::OpSwitch, "Switch");
    STRINGISE_ENUM_NAMED(spv::OpKill, "Kill");
    STRINGISE_ENUM_NAMED(spv::OpReturn, "Return");
    STRINGISE_ENUM_NAMED(spv::OpReturnValue, "ReturnValue");
    STRINGISE_ENUM_NAMED(spv::OpUnreachable, "Unreachable");
    STRINGISE_ENUM_NAMED(spv::OpLifetimeStart, "LifetimeStart");
    STRINGISE_ENUM_NAMED(spv::OpLifetimeStop, "LifetimeStop");
    STRINGISE_ENUM_NAMED(spv::OpGroupAsyncCopy, "GroupAsyncCopy");
    STRINGISE_ENUM_NAMED(spv::OpGroupWaitEvents, "GroupWaitEvents");
    STRINGISE_ENUM_NAMED(spv::OpGroupAll, "GroupAll");
    STRINGISE_ENUM_NAMED(spv::OpGroupAny, "GroupAny");
    STRINGISE_ENUM_NAMED(spv::OpGroupBroadcast, "GroupBroadcast");
    STRINGISE_ENUM_NAMED(spv::OpGroupIAdd, "GroupIAdd");
    STRINGISE_ENUM_NAMED(spv::OpGroupFAdd, "GroupFAdd");
    STRINGISE_ENUM_NAMED(spv::OpGroupFMin, "GroupFMin");
    STRINGISE_ENUM_NAMED(spv::OpGroupUMin, "GroupUMin");
    STRINGISE_ENUM_NAMED(spv::OpGroupSMin, "GroupSMin");
    STRINGISE_ENUM_NAMED(spv::OpGroupFMax, "GroupFMax");
    STRINGISE_ENUM_NAMED(spv::OpGroupUMax, "GroupUMax");
    STRINGISE_ENUM_NAMED(spv::OpGroupSMax, "GroupSMax");
    STRINGISE_ENUM_NAMED(spv::OpReadPipe, "ReadPipe");
    STRINGISE_ENUM_NAMED(spv::OpWritePipe, "WritePipe");
    STRINGISE_ENUM_NAMED(spv::OpReservedReadPipe, "ReservedReadPipe");
    STRINGISE_ENUM_NAMED(spv::OpReservedWritePipe, "ReservedWritePipe");
    STRINGISE_ENUM_NAMED(spv::OpReserveReadPipePackets, "ReserveReadPipePackets");
    STRINGISE_ENUM_NAMED(spv::OpReserveWritePipePackets, "ReserveWritePipePackets");
    STRINGISE_ENUM_NAMED(spv::OpCommitReadPipe, "CommitReadPipe");
    STRINGISE_ENUM_NAMED(spv::OpCommitWritePipe, "CommitWritePipe");
    STRINGISE_ENUM_NAMED(spv::OpIsValidReserveId, "IsValidReserveId");
    STRINGISE_ENUM_NAMED(spv::OpGetNumPipePackets, "GetNumPipePackets");
    STRINGISE_ENUM_NAMED(spv::OpGetMaxPipePackets, "GetMaxPipePackets");
    STRINGISE_ENUM_NAMED(spv::OpGroupReserveReadPipePackets, "GroupReserveReadPipePackets");
    STRINGISE_ENUM_NAMED(spv::OpGroupReserveWritePipePackets, "GroupReserveWritePipePackets");
    STRINGISE_ENUM_NAMED(spv::OpGroupCommitReadPipe, "GroupCommitReadPipe");
    STRINGISE_ENUM_NAMED(spv::OpGroupCommitWritePipe, "GroupCommitWritePipe");
    STRINGISE_ENUM_NAMED(spv::OpEnqueueMarker, "EnqueueMarker");
    STRINGISE_ENUM_NAMED(spv::OpEnqueueKernel, "EnqueueKernel");
    STRINGISE_ENUM_NAMED(spv::OpGetKernelNDrangeSubGroupCount, "GetKernelNDrangeSubGroupCount");
    STRINGISE_ENUM_NAMED(spv::OpGetKernelNDrangeMaxSubGroupSize, "GetKernelNDrangeMaxSubGroupSize");
    STRINGISE_ENUM_NAMED(spv::OpGetKernelWorkGroupSize, "GetKernelWorkGroupSize");
    STRINGISE_ENUM_NAMED(spv::OpGetKernelPreferredWorkGroupSizeMultiple,
                         "GetKernelPreferredWorkGroupSizeMultiple");
    STRINGISE_ENUM_NAMED(spv::OpRetainEvent, "RetainEvent");
    STRINGISE_ENUM_NAMED(spv::OpReleaseEvent, "ReleaseEvent");
    STRINGISE_ENUM_NAMED(spv::OpCreateUserEvent, "CreateUserEvent");
    STRINGISE_ENUM_NAMED(spv::OpIsValidEvent, "IsValidEvent");
    STRINGISE_ENUM_NAMED(spv::OpSetUserEventStatus, "SetUserEventStatus");
    STRINGISE_ENUM_NAMED(spv::OpCaptureEventProfilingInfo, "CaptureEventProfilingInfo");
    STRINGISE_ENUM_NAMED(spv::OpGetDefaultQueue, "GetDefaultQueue");
    STRINGISE_ENUM_NAMED(spv::OpBuildNDRange, "BuildNDRange");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleImplicitLod, "ImageSparseSampleImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleExplicitLod, "ImageSparseSampleExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleDrefImplicitLod,
                         "ImageSparseSampleDrefImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleDrefExplicitLod,
                         "ImageSparseSampleDrefExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleProjImplicitLod,
                         "ImageSparseSampleProjImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleProjExplicitLod,
                         "ImageSparseSampleProjExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleProjDrefImplicitLod,
                         "ImageSparseSampleProjDrefImplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseSampleProjDrefExplicitLod,
                         "ImageSparseSampleProjDrefExplicitLod");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseFetch, "ImageSparseFetch");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseGather, "ImageSparseGather");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseDrefGather, "ImageSparseDrefGather");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseTexelsResident, "ImageSparseTexelsResident");
    STRINGISE_ENUM_NAMED(spv::OpNoLine, "NoLine");
    STRINGISE_ENUM_NAMED(spv::OpAtomicFlagTestAndSet, "AtomicFlagTestAndSet");
    STRINGISE_ENUM_NAMED(spv::OpAtomicFlagClear, "AtomicFlagClear");
    STRINGISE_ENUM_NAMED(spv::OpImageSparseRead, "ImageSparseRead");
    STRINGISE_ENUM_NAMED(spv::OpSizeOf, "SizeOf");
    STRINGISE_ENUM_NAMED(spv::OpTypePipeStorage, "TypePipeStorage");
    STRINGISE_ENUM_NAMED(spv::OpConstantPipeStorage, "ConstantPipeStorage");
    STRINGISE_ENUM_NAMED(spv::OpCreatePipeFromPipeStorage, "CreatePipeFromPipeStorage");
    STRINGISE_ENUM_NAMED(spv::OpGetKernelLocalSizeForSubgroupCount,
                         "GetKernelLocalSizeForSubgroupCount");
    STRINGISE_ENUM_NAMED(spv::OpGetKernelMaxNumSubgroups, "GetKernelMaxNumSubgroups");
    STRINGISE_ENUM_NAMED(spv::OpTypeNamedBarrier, "TypeNamedBarrier");
    STRINGISE_ENUM_NAMED(spv::OpNamedBarrierInitialize, "NamedBarrierInitialize");
    STRINGISE_ENUM_NAMED(spv::OpMemoryNamedBarrier, "MemoryNamedBarrier");
    STRINGISE_ENUM_NAMED(spv::OpModuleProcessed, "ModuleProcessed");
    STRINGISE_ENUM_NAMED(spv::OpExecutionModeId, "ExecutionModeId");
    STRINGISE_ENUM_NAMED(spv::OpDecorateId, "DecorateId");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformElect, "GroupNonUniformElect");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformAll, "GroupNonUniformAll");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformAny, "GroupNonUniformAny");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformAllEqual, "GroupNonUniformAllEqual");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBroadcast, "GroupNonUniformBroadcast");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBroadcastFirst, "GroupNonUniformBroadcastFirst");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBallot, "GroupNonUniformBallot");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformInverseBallot, "GroupNonUniformInverseBallot");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBallotBitExtract, "GroupNonUniformBallotBitExtract");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBallotBitCount, "GroupNonUniformBallotBitCount");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBallotFindLSB, "GroupNonUniformBallotFindLSB");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBallotFindMSB, "GroupNonUniformBallotFindMSB");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformShuffle, "GroupNonUniformShuffle");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformShuffleXor, "GroupNonUniformShuffleXor");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformShuffleUp, "GroupNonUniformShuffleUp");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformShuffleDown, "GroupNonUniformShuffleDown");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformIAdd, "GroupNonUniformIAdd");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformFAdd, "GroupNonUniformFAdd");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformIMul, "GroupNonUniformIMul");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformFMul, "GroupNonUniformFMul");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformSMin, "GroupNonUniformSMin");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformUMin, "GroupNonUniformUMin");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformFMin, "GroupNonUniformFMin");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformSMax, "GroupNonUniformSMax");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformUMax, "GroupNonUniformUMax");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformFMax, "GroupNonUniformFMax");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBitwiseAnd, "GroupNonUniformBitwiseAnd");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBitwiseOr, "GroupNonUniformBitwiseOr");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformBitwiseXor, "GroupNonUniformBitwiseXor");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformLogicalAnd, "GroupNonUniformLogicalAnd");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformLogicalOr, "GroupNonUniformLogicalOr");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformLogicalXor, "GroupNonUniformLogicalXor");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformQuadBroadcast, "GroupNonUniformQuadBroadcast");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformQuadSwap, "GroupNonUniformQuadSwap");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupBallotKHR, "SubgroupBallotKHR");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupFirstInvocationKHR, "SubgroupFirstInvocationKHR");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAllKHR, "SubgroupAllKHR");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAnyKHR, "SubgroupAnyKHR");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAllEqualKHR, "SubgroupAllEqualKHR");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupReadInvocationKHR, "SubgroupReadInvocationKHR");
    STRINGISE_ENUM_NAMED(spv::OpGroupIAddNonUniformAMD, "GroupIAddNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupFAddNonUniformAMD, "GroupFAddNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupFMinNonUniformAMD, "GroupFMinNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupUMinNonUniformAMD, "GroupUMinNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupSMinNonUniformAMD, "GroupSMinNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupFMaxNonUniformAMD, "GroupFMaxNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupUMaxNonUniformAMD, "GroupUMaxNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpGroupSMaxNonUniformAMD, "GroupSMaxNonUniformAMD");
    STRINGISE_ENUM_NAMED(spv::OpFragmentMaskFetchAMD, "FragmentMaskFetchAMD");
    STRINGISE_ENUM_NAMED(spv::OpFragmentFetchAMD, "FragmentFetchAMD");
    STRINGISE_ENUM_NAMED(spv::OpImageSampleFootprintNV, "ImageSampleFootprintNV");
    STRINGISE_ENUM_NAMED(spv::OpGroupNonUniformPartitionNV, "GroupNonUniformPartitionNV");
    STRINGISE_ENUM_NAMED(spv::OpWritePackedPrimitiveIndices4x8NV,
                         "WritePackedPrimitiveIndices4x8NV");
    STRINGISE_ENUM_NAMED(spv::OpReportIntersectionNV, "ReportIntersectionNV");
    STRINGISE_ENUM_NAMED(spv::OpIgnoreIntersectionNV, "IgnoreIntersectionNV");
    STRINGISE_ENUM_NAMED(spv::OpTerminateRayNV, "TerminateRayNV");
    STRINGISE_ENUM_NAMED(spv::OpTraceNV, "TraceNV");
    STRINGISE_ENUM_NAMED(spv::OpTypeAccelerationStructureNV, "TypeAccelerationStructureNV");
    STRINGISE_ENUM_NAMED(spv::OpExecuteCallableNV, "ExecuteCallableNV");
    STRINGISE_ENUM_NAMED(spv::OpTypeCooperativeMatrixNV, "TypeCooperativeMatrixNV");
    STRINGISE_ENUM_NAMED(spv::OpCooperativeMatrixLoadNV, "CooperativeMatrixLoadNV");
    STRINGISE_ENUM_NAMED(spv::OpCooperativeMatrixStoreNV, "CooperativeMatrixStoreNV");
    STRINGISE_ENUM_NAMED(spv::OpCooperativeMatrixMulAddNV, "CooperativeMatrixMulAddNV");
    STRINGISE_ENUM_NAMED(spv::OpCooperativeMatrixLengthNV, "CooperativeMatrixLengthNV");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupShuffleINTEL, "SubgroupShuffleINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupShuffleDownINTEL, "SubgroupShuffleDownINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupShuffleUpINTEL, "SubgroupShuffleUpINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupShuffleXorINTEL, "SubgroupShuffleXorINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupBlockReadINTEL, "SubgroupBlockReadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupBlockWriteINTEL, "SubgroupBlockWriteINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupImageBlockReadINTEL, "SubgroupImageBlockReadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupImageBlockWriteINTEL, "SubgroupImageBlockWriteINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupImageMediaBlockReadINTEL,
                         "SubgroupImageMediaBlockReadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupImageMediaBlockWriteINTEL,
                         "SubgroupImageMediaBlockWriteINTEL");
    STRINGISE_ENUM_NAMED(spv::OpDecorateStringGOOGLE, "DecorateStringGOOGLE");
    STRINGISE_ENUM_NAMED(spv::OpMemberDecorateStringGOOGLE, "MemberDecorateStringGOOGLE");
    STRINGISE_ENUM_NAMED(spv::OpVmeImageINTEL, "VmeImageINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeVmeImageINTEL, "TypeVmeImageINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcImePayloadINTEL, "TypeAvcImePayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcRefPayloadINTEL, "TypeAvcRefPayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcSicPayloadINTEL, "TypeAvcSicPayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcMcePayloadINTEL, "TypeAvcMcePayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcMceResultINTEL, "TypeAvcMceResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcImeResultINTEL, "TypeAvcImeResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcImeResultSingleReferenceStreamoutINTEL,
                         "TypeAvcImeResultSingleReferenceStreamoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcImeResultDualReferenceStreamoutINTEL,
                         "TypeAvcImeResultDualReferenceStreamoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcImeSingleReferenceStreaminINTEL,
                         "TypeAvcImeSingleReferenceStreaminINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcImeDualReferenceStreaminINTEL,
                         "TypeAvcImeDualReferenceStreaminINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcRefResultINTEL, "TypeAvcRefResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpTypeAvcSicResultINTEL, "TypeAvcSicResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL,
                         "SubgroupAvcMceGetDefaultInterBaseMultiReferencePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL,
                         "SubgroupAvcMceSetInterBaseMultiReferencePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultInterShapePenaltyINTEL,
                         "SubgroupAvcMceGetDefaultInterShapePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetInterShapePenaltyINTEL,
                         "SubgroupAvcMceSetInterShapePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL,
                         "SubgroupAvcMceGetDefaultInterDirectionPenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetInterDirectionPenaltyINTEL,
                         "SubgroupAvcMceSetInterDirectionPenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL,
                         "SubgroupAvcMceGetDefaultIntraLumaShapePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL,
                         "SubgroupAvcMceGetDefaultInterMotionVectorCostTableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL,
                         "SubgroupAvcMceGetDefaultHighPenaltyCostTableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL,
                         "SubgroupAvcMceGetDefaultMediumPenaltyCostTableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL,
                         "SubgroupAvcMceGetDefaultLowPenaltyCostTableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetMotionVectorCostFunctionINTEL,
                         "SubgroupAvcMceSetMotionVectorCostFunctionINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL,
                         "SubgroupAvcMceGetDefaultIntraLumaModePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL,
                         "SubgroupAvcMceGetDefaultNonDcLumaIntraPenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL,
                         "SubgroupAvcMceGetDefaultIntraChromaModeBasePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetAcOnlyHaarINTEL,
                         "SubgroupAvcMceSetAcOnlyHaarINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL,
                         "SubgroupAvcMceSetSourceInterlacedFieldPolarityINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL,
                         "SubgroupAvcMceSetSingleReferenceInterlacedFieldPolarityINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL,
                         "SubgroupAvcMceSetDualReferenceInterlacedFieldPolaritiesINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceConvertToImePayloadINTEL,
                         "SubgroupAvcMceConvertToImePayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceConvertToImeResultINTEL,
                         "SubgroupAvcMceConvertToImeResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceConvertToRefPayloadINTEL,
                         "SubgroupAvcMceConvertToRefPayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceConvertToRefResultINTEL,
                         "SubgroupAvcMceConvertToRefResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceConvertToSicPayloadINTEL,
                         "SubgroupAvcMceConvertToSicPayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceConvertToSicResultINTEL,
                         "SubgroupAvcMceConvertToSicResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetMotionVectorsINTEL,
                         "SubgroupAvcMceGetMotionVectorsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterDistortionsINTEL,
                         "SubgroupAvcMceGetInterDistortionsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetBestInterDistortionsINTEL,
                         "SubgroupAvcMceGetBestInterDistortionsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterMajorShapeINTEL,
                         "SubgroupAvcMceGetInterMajorShapeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterMinorShapeINTEL,
                         "SubgroupAvcMceGetInterMinorShapeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterDirectionsINTEL,
                         "SubgroupAvcMceGetInterDirectionsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterMotionVectorCountINTEL,
                         "SubgroupAvcMceGetInterMotionVectorCountINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterReferenceIdsINTEL,
                         "SubgroupAvcMceGetInterReferenceIdsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL,
                         "SubgroupAvcMceGetInterReferenceInterlacedFieldPolaritiesINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeInitializeINTEL, "SubgroupAvcImeInitializeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeSetSingleReferenceINTEL,
                         "SubgroupAvcImeSetSingleReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeSetDualReferenceINTEL,
                         "SubgroupAvcImeSetDualReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeRefWindowSizeINTEL,
                         "SubgroupAvcImeRefWindowSizeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeAdjustRefOffsetINTEL,
                         "SubgroupAvcImeAdjustRefOffsetINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeConvertToMcePayloadINTEL,
                         "SubgroupAvcImeConvertToMcePayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeSetMaxMotionVectorCountINTEL,
                         "SubgroupAvcImeSetMaxMotionVectorCountINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeSetUnidirectionalMixDisableINTEL,
                         "SubgroupAvcImeSetUnidirectionalMixDisableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeSetEarlySearchTerminationThresholdINTEL,
                         "SubgroupAvcImeSetEarlySearchTerminationThresholdINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeSetWeightedSadINTEL,
                         "SubgroupAvcImeSetWeightedSadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithSingleReferenceINTEL,
                         "SubgroupAvcImeEvaluateWithSingleReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithDualReferenceINTEL,
                         "SubgroupAvcImeEvaluateWithDualReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL,
                         "SubgroupAvcImeEvaluateWithSingleReferenceStreaminINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL,
                         "SubgroupAvcImeEvaluateWithDualReferenceStreaminINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL,
                         "SubgroupAvcImeEvaluateWithSingleReferenceStreamoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL,
                         "SubgroupAvcImeEvaluateWithDualReferenceStreamoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL,
                         "SubgroupAvcImeEvaluateWithSingleReferenceStreaminoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL,
                         "SubgroupAvcImeEvaluateWithDualReferenceStreaminoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeConvertToMceResultINTEL,
                         "SubgroupAvcImeConvertToMceResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetSingleReferenceStreaminINTEL,
                         "SubgroupAvcImeGetSingleReferenceStreaminINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetDualReferenceStreaminINTEL,
                         "SubgroupAvcImeGetDualReferenceStreaminINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeStripSingleReferenceStreamoutINTEL,
                         "SubgroupAvcImeStripSingleReferenceStreamoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeStripDualReferenceStreamoutINTEL,
                         "SubgroupAvcImeStripDualReferenceStreamoutINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL,
                         "SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeMotionVectorsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL,
                         "SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeDistortionsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL,
                         "SubgroupAvcImeGetStreamoutSingleReferenceMajorShapeReferenceIdsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL,
                         "SubgroupAvcImeGetStreamoutDualReferenceMajorShapeMotionVectorsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL,
                         "SubgroupAvcImeGetStreamoutDualReferenceMajorShapeDistortionsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL,
                         "SubgroupAvcImeGetStreamoutDualReferenceMajorShapeReferenceIdsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetBorderReachedINTEL,
                         "SubgroupAvcImeGetBorderReachedINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetTruncatedSearchIndicationINTEL,
                         "SubgroupAvcImeGetTruncatedSearchIndicationINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL,
                         "SubgroupAvcImeGetUnidirectionalEarlySearchTerminationINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL,
                         "SubgroupAvcImeGetWeightingPatternMinimumMotionVectorINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL,
                         "SubgroupAvcImeGetWeightingPatternMinimumDistortionINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcFmeInitializeINTEL, "SubgroupAvcFmeInitializeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcBmeInitializeINTEL, "SubgroupAvcBmeInitializeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefConvertToMcePayloadINTEL,
                         "SubgroupAvcRefConvertToMcePayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefSetBidirectionalMixDisableINTEL,
                         "SubgroupAvcRefSetBidirectionalMixDisableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefSetBilinearFilterEnableINTEL,
                         "SubgroupAvcRefSetBilinearFilterEnableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefEvaluateWithSingleReferenceINTEL,
                         "SubgroupAvcRefEvaluateWithSingleReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefEvaluateWithDualReferenceINTEL,
                         "SubgroupAvcRefEvaluateWithDualReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefEvaluateWithMultiReferenceINTEL,
                         "SubgroupAvcRefEvaluateWithMultiReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL,
                         "SubgroupAvcRefEvaluateWithMultiReferenceInterlacedINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcRefConvertToMceResultINTEL,
                         "SubgroupAvcRefConvertToMceResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicInitializeINTEL, "SubgroupAvcSicInitializeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicConfigureSkcINTEL, "SubgroupAvcSicConfigureSkcINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicConfigureIpeLumaINTEL,
                         "SubgroupAvcSicConfigureIpeLumaINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicConfigureIpeLumaChromaINTEL,
                         "SubgroupAvcSicConfigureIpeLumaChromaINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetMotionVectorMaskINTEL,
                         "SubgroupAvcSicGetMotionVectorMaskINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicConvertToMcePayloadINTEL,
                         "SubgroupAvcSicConvertToMcePayloadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicSetIntraLumaShapePenaltyINTEL,
                         "SubgroupAvcSicSetIntraLumaShapePenaltyINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicSetIntraLumaModeCostFunctionINTEL,
                         "SubgroupAvcSicSetIntraLumaModeCostFunctionINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicSetIntraChromaModeCostFunctionINTEL,
                         "SubgroupAvcSicSetIntraChromaModeCostFunctionINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicSetBilinearFilterEnableINTEL,
                         "SubgroupAvcSicSetBilinearFilterEnableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicSetSkcForwardTransformEnableINTEL,
                         "SubgroupAvcSicSetSkcForwardTransformEnableINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicSetBlockBasedRawSkipSadINTEL,
                         "SubgroupAvcSicSetBlockBasedRawSkipSadINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicEvaluateIpeINTEL, "SubgroupAvcSicEvaluateIpeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicEvaluateWithSingleReferenceINTEL,
                         "SubgroupAvcSicEvaluateWithSingleReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicEvaluateWithDualReferenceINTEL,
                         "SubgroupAvcSicEvaluateWithDualReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicEvaluateWithMultiReferenceINTEL,
                         "SubgroupAvcSicEvaluateWithMultiReferenceINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL,
                         "SubgroupAvcSicEvaluateWithMultiReferenceInterlacedINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicConvertToMceResultINTEL,
                         "SubgroupAvcSicConvertToMceResultINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetIpeLumaShapeINTEL,
                         "SubgroupAvcSicGetIpeLumaShapeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetBestIpeLumaDistortionINTEL,
                         "SubgroupAvcSicGetBestIpeLumaDistortionINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetBestIpeChromaDistortionINTEL,
                         "SubgroupAvcSicGetBestIpeChromaDistortionINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetPackedIpeLumaModesINTEL,
                         "SubgroupAvcSicGetPackedIpeLumaModesINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetIpeChromaModeINTEL,
                         "SubgroupAvcSicGetIpeChromaModeINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL,
                         "SubgroupAvcSicGetPackedSkcLumaCountThresholdINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL,
                         "SubgroupAvcSicGetPackedSkcLumaSumThresholdINTEL");
    STRINGISE_ENUM_NAMED(spv::OpSubgroupAvcSicGetInterRawSadsINTEL,
                         "SubgroupAvcSicGetInterRawSadsINTEL");
    STRINGISE_ENUM_NAMED(spv::OpMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::SourceLanguage &el)
{
  BEGIN_ENUM_STRINGISE(spv::SourceLanguage)
  {
    STRINGISE_ENUM_NAMED(spv::SourceLanguageUnknown, "Unknown");
    STRINGISE_ENUM_NAMED(spv::SourceLanguageESSL, "ESSL");
    STRINGISE_ENUM_NAMED(spv::SourceLanguageGLSL, "GLSL");
    STRINGISE_ENUM_NAMED(spv::SourceLanguageOpenCL_C, "OpenCL_C");
    STRINGISE_ENUM_NAMED(spv::SourceLanguageOpenCL_CPP, "OpenCL_CPP");
    STRINGISE_ENUM_NAMED(spv::SourceLanguageHLSL, "HLSL");
    STRINGISE_ENUM_NAMED(spv::SourceLanguageMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::Capability &el)
{
  BEGIN_ENUM_STRINGISE(spv::Capability)
  {
    STRINGISE_ENUM_NAMED(spv::CapabilityMatrix, "Matrix");
    STRINGISE_ENUM_NAMED(spv::CapabilityShader, "Shader");
    STRINGISE_ENUM_NAMED(spv::CapabilityGeometry, "Geometry");
    STRINGISE_ENUM_NAMED(spv::CapabilityTessellation, "Tessellation");
    STRINGISE_ENUM_NAMED(spv::CapabilityAddresses, "Addresses");
    STRINGISE_ENUM_NAMED(spv::CapabilityLinkage, "Linkage");
    STRINGISE_ENUM_NAMED(spv::CapabilityKernel, "Kernel");
    STRINGISE_ENUM_NAMED(spv::CapabilityVector16, "Vector16");
    STRINGISE_ENUM_NAMED(spv::CapabilityFloat16Buffer, "Float16Buffer");
    STRINGISE_ENUM_NAMED(spv::CapabilityFloat16, "Float16");
    STRINGISE_ENUM_NAMED(spv::CapabilityFloat64, "Float64");
    STRINGISE_ENUM_NAMED(spv::CapabilityInt64, "Int64");
    STRINGISE_ENUM_NAMED(spv::CapabilityInt64Atomics, "Int64Atomics");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageBasic, "ImageBasic");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageReadWrite, "ImageReadWrite");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageMipmap, "ImageMipmap");
    STRINGISE_ENUM_NAMED(spv::CapabilityPipes, "Pipes");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroups, "Groups");
    STRINGISE_ENUM_NAMED(spv::CapabilityDeviceEnqueue, "DeviceEnqueue");
    STRINGISE_ENUM_NAMED(spv::CapabilityLiteralSampler, "LiteralSampler");
    STRINGISE_ENUM_NAMED(spv::CapabilityAtomicStorage, "AtomicStorage");
    STRINGISE_ENUM_NAMED(spv::CapabilityInt16, "Int16");
    STRINGISE_ENUM_NAMED(spv::CapabilityTessellationPointSize, "TessellationPointSize");
    STRINGISE_ENUM_NAMED(spv::CapabilityGeometryPointSize, "GeometryPointSize");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageGatherExtended, "ImageGatherExtended");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageImageMultisample, "StorageImageMultisample");
    STRINGISE_ENUM_NAMED(spv::CapabilityUniformBufferArrayDynamicIndexing,
                         "UniformBufferArrayDynamicIndexing");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampledImageArrayDynamicIndexing,
                         "SampledImageArrayDynamicIndexing");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageBufferArrayDynamicIndexing,
                         "StorageBufferArrayDynamicIndexing");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageImageArrayDynamicIndexing,
                         "StorageImageArrayDynamicIndexing");
    STRINGISE_ENUM_NAMED(spv::CapabilityClipDistance, "ClipDistance");
    STRINGISE_ENUM_NAMED(spv::CapabilityCullDistance, "CullDistance");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageCubeArray, "ImageCubeArray");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampleRateShading, "SampleRateShading");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageRect, "ImageRect");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampledRect, "SampledRect");
    STRINGISE_ENUM_NAMED(spv::CapabilityGenericPointer, "GenericPointer");
    STRINGISE_ENUM_NAMED(spv::CapabilityInt8, "Int8");
    STRINGISE_ENUM_NAMED(spv::CapabilityInputAttachment, "InputAttachment");
    STRINGISE_ENUM_NAMED(spv::CapabilitySparseResidency, "SparseResidency");
    STRINGISE_ENUM_NAMED(spv::CapabilityMinLod, "MinLod");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampled1D, "Sampled1D");
    STRINGISE_ENUM_NAMED(spv::CapabilityImage1D, "Image1D");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampledCubeArray, "SampledCubeArray");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampledBuffer, "SampledBuffer");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageBuffer, "ImageBuffer");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageMSArray, "ImageMSArray");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageImageExtendedFormats, "StorageImageExtendedFormats");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageQuery, "ImageQuery");
    STRINGISE_ENUM_NAMED(spv::CapabilityDerivativeControl, "DerivativeControl");
    STRINGISE_ENUM_NAMED(spv::CapabilityInterpolationFunction, "InterpolationFunction");
    STRINGISE_ENUM_NAMED(spv::CapabilityTransformFeedback, "TransformFeedback");
    STRINGISE_ENUM_NAMED(spv::CapabilityGeometryStreams, "GeometryStreams");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageImageReadWithoutFormat,
                         "StorageImageReadWithoutFormat");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageImageWriteWithoutFormat,
                         "StorageImageWriteWithoutFormat");
    STRINGISE_ENUM_NAMED(spv::CapabilityMultiViewport, "MultiViewport");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupDispatch, "SubgroupDispatch");
    STRINGISE_ENUM_NAMED(spv::CapabilityNamedBarrier, "NamedBarrier");
    STRINGISE_ENUM_NAMED(spv::CapabilityPipeStorage, "PipeStorage");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniform, "GroupNonUniform");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformVote, "GroupNonUniformVote");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformArithmetic, "GroupNonUniformArithmetic");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformBallot, "GroupNonUniformBallot");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformShuffle, "GroupNonUniformShuffle");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformShuffleRelative,
                         "GroupNonUniformShuffleRelative");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformClustered, "GroupNonUniformClustered");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformQuad, "GroupNonUniformQuad");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupBallotKHR, "SubgroupBallotKHR");
    STRINGISE_ENUM_NAMED(spv::CapabilityDrawParameters, "DrawParameters");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupVoteKHR, "SubgroupVoteKHR");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageBuffer16BitAccess, "StorageBuffer16BitAccess");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageUniform16, "StorageUniform16");
    STRINGISE_ENUM_NAMED(spv::CapabilityStoragePushConstant16, "StoragePushConstant16");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageInputOutput16, "StorageInputOutput16");
    STRINGISE_ENUM_NAMED(spv::CapabilityDeviceGroup, "DeviceGroup");
    STRINGISE_ENUM_NAMED(spv::CapabilityMultiView, "MultiView");
    STRINGISE_ENUM_NAMED(spv::CapabilityVariablePointersStorageBuffer,
                         "VariablePointersStorageBuffer");
    STRINGISE_ENUM_NAMED(spv::CapabilityVariablePointers, "VariablePointers");
    STRINGISE_ENUM_NAMED(spv::CapabilityAtomicStorageOps, "AtomicStorageOps");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampleMaskPostDepthCoverage, "SampleMaskPostDepthCoverage");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageBuffer8BitAccess, "StorageBuffer8BitAccess");
    STRINGISE_ENUM_NAMED(spv::CapabilityUniformAndStorageBuffer8BitAccess,
                         "UniformAndStorageBuffer8BitAccess");
    STRINGISE_ENUM_NAMED(spv::CapabilityStoragePushConstant8, "StoragePushConstant8");
    STRINGISE_ENUM_NAMED(spv::CapabilityDenormPreserve, "DenormPreserve");
    STRINGISE_ENUM_NAMED(spv::CapabilityDenormFlushToZero, "DenormFlushToZero");
    STRINGISE_ENUM_NAMED(spv::CapabilitySignedZeroInfNanPreserve, "SignedZeroInfNanPreserve");
    STRINGISE_ENUM_NAMED(spv::CapabilityRoundingModeRTE, "RoundingModeRTE");
    STRINGISE_ENUM_NAMED(spv::CapabilityRoundingModeRTZ, "RoundingModeRTZ");
    STRINGISE_ENUM_NAMED(spv::CapabilityFloat16ImageAMD, "Float16ImageAMD");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageGatherBiasLodAMD, "ImageGatherBiasLodAMD");
    STRINGISE_ENUM_NAMED(spv::CapabilityFragmentMaskAMD, "FragmentMaskAMD");
    STRINGISE_ENUM_NAMED(spv::CapabilityStencilExportEXT, "StencilExportEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageReadWriteLodAMD, "ImageReadWriteLodAMD");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampleMaskOverrideCoverageNV,
                         "SampleMaskOverrideCoverageNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityGeometryShaderPassthroughNV, "GeometryShaderPassthroughNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityShaderViewportIndexLayerEXT, "ShaderViewportIndexLayerEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityShaderViewportMaskNV, "ShaderViewportMaskNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityShaderStereoViewNV, "ShaderStereoViewNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityPerViewAttributesNV, "PerViewAttributesNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityFragmentFullyCoveredEXT, "FragmentFullyCoveredEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityMeshShadingNV, "MeshShadingNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityImageFootprintNV, "ImageFootprintNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityFragmentBarycentricNV, "FragmentBarycentricNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityComputeDerivativeGroupQuadsNV,
                         "ComputeDerivativeGroupQuadsNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityFragmentDensityEXT, "FragmentDensityEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityGroupNonUniformPartitionedNV,
                         "GroupNonUniformPartitionedNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityShaderNonUniformEXT, "ShaderNonUniformEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityRuntimeDescriptorArrayEXT, "RuntimeDescriptorArrayEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityInputAttachmentArrayDynamicIndexingEXT,
                         "InputAttachmentArrayDynamicIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityUniformTexelBufferArrayDynamicIndexingEXT,
                         "UniformTexelBufferArrayDynamicIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageTexelBufferArrayDynamicIndexingEXT,
                         "StorageTexelBufferArrayDynamicIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityUniformBufferArrayNonUniformIndexingEXT,
                         "UniformBufferArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilitySampledImageArrayNonUniformIndexingEXT,
                         "SampledImageArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageBufferArrayNonUniformIndexingEXT,
                         "StorageBufferArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageImageArrayNonUniformIndexingEXT,
                         "StorageImageArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityInputAttachmentArrayNonUniformIndexingEXT,
                         "InputAttachmentArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityUniformTexelBufferArrayNonUniformIndexingEXT,
                         "UniformTexelBufferArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityStorageTexelBufferArrayNonUniformIndexingEXT,
                         "StorageTexelBufferArrayNonUniformIndexingEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityRayTracingNV, "RayTracingNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityVulkanMemoryModelKHR, "VulkanMemoryModelKHR");
    STRINGISE_ENUM_NAMED(spv::CapabilityVulkanMemoryModelDeviceScopeKHR,
                         "VulkanMemoryModelDeviceScopeKHR");
    STRINGISE_ENUM_NAMED(spv::CapabilityPhysicalStorageBufferAddressesEXT,
                         "PhysicalStorageBufferAddressesEXT");
    STRINGISE_ENUM_NAMED(spv::CapabilityComputeDerivativeGroupLinearNV,
                         "ComputeDerivativeGroupLinearNV");
    STRINGISE_ENUM_NAMED(spv::CapabilityCooperativeMatrixNV, "CooperativeMatrixNV");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupShuffleINTEL, "SubgroupShuffleINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupBufferBlockIOINTEL, "SubgroupBufferBlockIOINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupImageBlockIOINTEL, "SubgroupImageBlockIOINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupImageMediaBlockIOINTEL,
                         "SubgroupImageMediaBlockIOINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupAvcMotionEstimationINTEL,
                         "SubgroupAvcMotionEstimationINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupAvcMotionEstimationIntraINTEL,
                         "SubgroupAvcMotionEstimationIntraINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilitySubgroupAvcMotionEstimationChromaINTEL,
                         "SubgroupAvcMotionEstimationChromaINTEL");
    STRINGISE_ENUM_NAMED(spv::CapabilityMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::ExecutionMode &el)
{
  BEGIN_ENUM_STRINGISE(spv::ExecutionMode)
  {
    STRINGISE_ENUM_NAMED(spv::ExecutionModeInvocations, "Invocations");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSpacingEqual, "SpacingEqual");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSpacingFractionalEven, "SpacingFractionalEven");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSpacingFractionalOdd, "SpacingFractionalOdd");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeVertexOrderCw, "VertexOrderCw");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeVertexOrderCcw, "VertexOrderCcw");
    STRINGISE_ENUM_NAMED(spv::ExecutionModePixelCenterInteger, "PixelCenterInteger");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOriginUpperLeft, "OriginUpperLeft");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOriginLowerLeft, "OriginLowerLeft");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeEarlyFragmentTests, "EarlyFragmentTests");
    STRINGISE_ENUM_NAMED(spv::ExecutionModePointMode, "PointMode");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeXfb, "Xfb");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDepthReplacing, "DepthReplacing");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDepthGreater, "DepthGreater");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDepthLess, "DepthLess");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDepthUnchanged, "DepthUnchanged");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeLocalSize, "LocalSize");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeLocalSizeHint, "LocalSizeHint");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeInputPoints, "InputPoints");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeInputLines, "InputLines");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeInputLinesAdjacency, "InputLinesAdjacency");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeTriangles, "Triangles");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeInputTrianglesAdjacency, "InputTrianglesAdjacency");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeQuads, "Quads");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeIsolines, "Isolines");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputVertices, "OutputVertices");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputPoints, "OutputPoints");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputLineStrip, "OutputLineStrip");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputTriangleStrip, "OutputTriangleStrip");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeVecTypeHint, "VecTypeHint");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeContractionOff, "ContractionOff");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeInitializer, "Initializer");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeFinalizer, "Finalizer");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSubgroupSize, "SubgroupSize");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSubgroupsPerWorkgroup, "SubgroupsPerWorkgroup");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSubgroupsPerWorkgroupId, "SubgroupsPerWorkgroupId");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeLocalSizeId, "LocalSizeId");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeLocalSizeHintId, "LocalSizeHintId");
    STRINGISE_ENUM_NAMED(spv::ExecutionModePostDepthCoverage, "PostDepthCoverage");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDenormPreserve, "DenormPreserve");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDenormFlushToZero, "DenormFlushToZero");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeSignedZeroInfNanPreserve, "SignedZeroInfNanPreserve");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeRoundingModeRTE, "RoundingModeRTE");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeRoundingModeRTZ, "RoundingModeRTZ");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeStencilRefReplacingEXT, "StencilRefReplacingEXT");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputLinesNV, "OutputLinesNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputPrimitivesNV, "OutputPrimitivesNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDerivativeGroupQuadsNV, "DerivativeGroupQuadsNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeDerivativeGroupLinearNV, "DerivativeGroupLinearNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeOutputTrianglesNV, "OutputTrianglesNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModeMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::AddressingModel &el)
{
  BEGIN_ENUM_STRINGISE(spv::AddressingModel)
  {
    STRINGISE_ENUM_NAMED(spv::AddressingModelLogical, "Logical");
    STRINGISE_ENUM_NAMED(spv::AddressingModelPhysical32, "Physical32");
    STRINGISE_ENUM_NAMED(spv::AddressingModelPhysical64, "Physical64");
    STRINGISE_ENUM_NAMED(spv::AddressingModelPhysicalStorageBuffer64EXT,
                         "PhysicalStorageBuffer64EXT");
    STRINGISE_ENUM_NAMED(spv::AddressingModelMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::MemoryModel &el)
{
  BEGIN_ENUM_STRINGISE(spv::MemoryModel)
  {
    STRINGISE_ENUM_NAMED(spv::MemoryModelSimple, "Simple");
    STRINGISE_ENUM_NAMED(spv::MemoryModelGLSL450, "GLSL450");
    STRINGISE_ENUM_NAMED(spv::MemoryModelOpenCL, "OpenCL");
    STRINGISE_ENUM_NAMED(spv::MemoryModelVulkanKHR, "VulkanKHR");
    STRINGISE_ENUM_NAMED(spv::MemoryModelMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::ExecutionModel &el)
{
  BEGIN_ENUM_STRINGISE(spv::ExecutionModel)
  {
    STRINGISE_ENUM_NAMED(spv::ExecutionModelVertex, "Vertex");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelTessellationControl, "TessellationControl");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelTessellationEvaluation, "TessellationEvaluation");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelGeometry, "Geometry");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelFragment, "Fragment");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelGLCompute, "GLCompute");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelKernel, "Kernel");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelTaskNV, "TaskNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelMeshNV, "MeshNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelRayGenerationNV, "RayGenerationNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelIntersectionNV, "IntersectionNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelAnyHitNV, "AnyHitNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelClosestHitNV, "ClosestHitNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelMissNV, "MissNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelCallableNV, "CallableNV");
    STRINGISE_ENUM_NAMED(spv::ExecutionModelMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::Decoration &el)
{
  BEGIN_ENUM_STRINGISE(spv::Decoration)
  {
    STRINGISE_ENUM_NAMED(spv::DecorationRelaxedPrecision, "RelaxedPrecision");
    STRINGISE_ENUM_NAMED(spv::DecorationSpecId, "SpecId");
    STRINGISE_ENUM_NAMED(spv::DecorationBlock, "Block");
    STRINGISE_ENUM_NAMED(spv::DecorationBufferBlock, "BufferBlock");
    STRINGISE_ENUM_NAMED(spv::DecorationRowMajor, "RowMajor");
    STRINGISE_ENUM_NAMED(spv::DecorationColMajor, "ColMajor");
    STRINGISE_ENUM_NAMED(spv::DecorationArrayStride, "ArrayStride");
    STRINGISE_ENUM_NAMED(spv::DecorationMatrixStride, "MatrixStride");
    STRINGISE_ENUM_NAMED(spv::DecorationGLSLShared, "GLSLShared");
    STRINGISE_ENUM_NAMED(spv::DecorationGLSLPacked, "GLSLPacked");
    STRINGISE_ENUM_NAMED(spv::DecorationCPacked, "CPacked");
    STRINGISE_ENUM_NAMED(spv::DecorationBuiltIn, "BuiltIn");
    STRINGISE_ENUM_NAMED(spv::DecorationNoPerspective, "NoPerspective");
    STRINGISE_ENUM_NAMED(spv::DecorationFlat, "Flat");
    STRINGISE_ENUM_NAMED(spv::DecorationPatch, "Patch");
    STRINGISE_ENUM_NAMED(spv::DecorationCentroid, "Centroid");
    STRINGISE_ENUM_NAMED(spv::DecorationSample, "Sample");
    STRINGISE_ENUM_NAMED(spv::DecorationInvariant, "Invariant");
    STRINGISE_ENUM_NAMED(spv::DecorationRestrict, "Restrict");
    STRINGISE_ENUM_NAMED(spv::DecorationAliased, "Aliased");
    STRINGISE_ENUM_NAMED(spv::DecorationVolatile, "Volatile");
    STRINGISE_ENUM_NAMED(spv::DecorationConstant, "Constant");
    STRINGISE_ENUM_NAMED(spv::DecorationCoherent, "Coherent");
    STRINGISE_ENUM_NAMED(spv::DecorationNonWritable, "NonWritable");
    STRINGISE_ENUM_NAMED(spv::DecorationNonReadable, "NonReadable");
    STRINGISE_ENUM_NAMED(spv::DecorationUniform, "Uniform");
    STRINGISE_ENUM_NAMED(spv::DecorationSaturatedConversion, "SaturatedConversion");
    STRINGISE_ENUM_NAMED(spv::DecorationStream, "Stream");
    STRINGISE_ENUM_NAMED(spv::DecorationLocation, "Location");
    STRINGISE_ENUM_NAMED(spv::DecorationComponent, "Component");
    STRINGISE_ENUM_NAMED(spv::DecorationIndex, "Index");
    STRINGISE_ENUM_NAMED(spv::DecorationBinding, "Binding");
    STRINGISE_ENUM_NAMED(spv::DecorationDescriptorSet, "DescriptorSet");
    STRINGISE_ENUM_NAMED(spv::DecorationOffset, "Offset");
    STRINGISE_ENUM_NAMED(spv::DecorationXfbBuffer, "XfbBuffer");
    STRINGISE_ENUM_NAMED(spv::DecorationXfbStride, "XfbStride");
    STRINGISE_ENUM_NAMED(spv::DecorationFuncParamAttr, "FuncParamAttr");
    STRINGISE_ENUM_NAMED(spv::DecorationFPRoundingMode, "FPRoundingMode");
    STRINGISE_ENUM_NAMED(spv::DecorationFPFastMathMode, "FPFastMathMode");
    STRINGISE_ENUM_NAMED(spv::DecorationLinkageAttributes, "LinkageAttributes");
    STRINGISE_ENUM_NAMED(spv::DecorationNoContraction, "NoContraction");
    STRINGISE_ENUM_NAMED(spv::DecorationInputAttachmentIndex, "InputAttachmentIndex");
    STRINGISE_ENUM_NAMED(spv::DecorationAlignment, "Alignment");
    STRINGISE_ENUM_NAMED(spv::DecorationMaxByteOffset, "MaxByteOffset");
    STRINGISE_ENUM_NAMED(spv::DecorationAlignmentId, "AlignmentId");
    STRINGISE_ENUM_NAMED(spv::DecorationMaxByteOffsetId, "MaxByteOffsetId");
    STRINGISE_ENUM_NAMED(spv::DecorationNoSignedWrap, "NoSignedWrap");
    STRINGISE_ENUM_NAMED(spv::DecorationNoUnsignedWrap, "NoUnsignedWrap");
    STRINGISE_ENUM_NAMED(spv::DecorationExplicitInterpAMD, "ExplicitInterpAMD");
    STRINGISE_ENUM_NAMED(spv::DecorationOverrideCoverageNV, "OverrideCoverageNV");
    STRINGISE_ENUM_NAMED(spv::DecorationPassthroughNV, "PassthroughNV");
    STRINGISE_ENUM_NAMED(spv::DecorationViewportRelativeNV, "ViewportRelativeNV");
    STRINGISE_ENUM_NAMED(spv::DecorationSecondaryViewportRelativeNV, "SecondaryViewportRelativeNV");
    STRINGISE_ENUM_NAMED(spv::DecorationPerPrimitiveNV, "PerPrimitiveNV");
    STRINGISE_ENUM_NAMED(spv::DecorationPerViewNV, "PerViewNV");
    STRINGISE_ENUM_NAMED(spv::DecorationPerTaskNV, "PerTaskNV");
    STRINGISE_ENUM_NAMED(spv::DecorationPerVertexNV, "PerVertexNV");
    STRINGISE_ENUM_NAMED(spv::DecorationNonUniformEXT, "NonUniformEXT");
    STRINGISE_ENUM_NAMED(spv::DecorationRestrictPointerEXT, "RestrictPointerEXT");
    STRINGISE_ENUM_NAMED(spv::DecorationAliasedPointerEXT, "AliasedPointerEXT");
    STRINGISE_ENUM_NAMED(spv::DecorationHlslCounterBufferGOOGLE, "HlslCounterBufferGOOGLE");
    STRINGISE_ENUM_NAMED(spv::DecorationHlslSemanticGOOGLE, "HlslSemanticGOOGLE");
    STRINGISE_ENUM_NAMED(spv::DecorationMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::Dim &el)
{
  BEGIN_ENUM_STRINGISE(spv::Dim)
  {
    STRINGISE_ENUM_NAMED(spv::Dim1D, "1D");
    STRINGISE_ENUM_NAMED(spv::Dim2D, "2D");
    STRINGISE_ENUM_NAMED(spv::Dim3D, "3D");
    STRINGISE_ENUM_NAMED(spv::DimCube, "Cube");
    STRINGISE_ENUM_NAMED(spv::DimRect, "Rect");
    STRINGISE_ENUM_NAMED(spv::DimBuffer, "Buffer");
    STRINGISE_ENUM_NAMED(spv::DimSubpassData, "SubpassData");
    STRINGISE_ENUM_NAMED(spv::DimMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::StorageClass &el)
{
  BEGIN_ENUM_STRINGISE(spv::StorageClass)
  {
    STRINGISE_ENUM_NAMED(spv::StorageClassUniformConstant, "UniformConstant");
    STRINGISE_ENUM_NAMED(spv::StorageClassInput, "Input");
    STRINGISE_ENUM_NAMED(spv::StorageClassUniform, "Uniform");
    STRINGISE_ENUM_NAMED(spv::StorageClassOutput, "Output");
    STRINGISE_ENUM_NAMED(spv::StorageClassWorkgroup, "Workgroup");
    STRINGISE_ENUM_NAMED(spv::StorageClassCrossWorkgroup, "CrossWorkgroup");
    STRINGISE_ENUM_NAMED(spv::StorageClassPrivate, "Private");
    STRINGISE_ENUM_NAMED(spv::StorageClassFunction, "Function");
    STRINGISE_ENUM_NAMED(spv::StorageClassGeneric, "Generic");
    STRINGISE_ENUM_NAMED(spv::StorageClassPushConstant, "PushConstant");
    STRINGISE_ENUM_NAMED(spv::StorageClassAtomicCounter, "AtomicCounter");
    STRINGISE_ENUM_NAMED(spv::StorageClassImage, "Image");
    STRINGISE_ENUM_NAMED(spv::StorageClassStorageBuffer, "StorageBuffer");
    STRINGISE_ENUM_NAMED(spv::StorageClassCallableDataNV, "CallableDataNV");
    STRINGISE_ENUM_NAMED(spv::StorageClassIncomingCallableDataNV, "IncomingCallableDataNV");
    STRINGISE_ENUM_NAMED(spv::StorageClassRayPayloadNV, "RayPayloadNV");
    STRINGISE_ENUM_NAMED(spv::StorageClassHitAttributeNV, "HitAttributeNV");
    STRINGISE_ENUM_NAMED(spv::StorageClassIncomingRayPayloadNV, "IncomingRayPayloadNV");
    STRINGISE_ENUM_NAMED(spv::StorageClassShaderRecordBufferNV, "ShaderRecordBufferNV");
    STRINGISE_ENUM_NAMED(spv::StorageClassPhysicalStorageBufferEXT, "PhysicalStorageBufferEXT");
    STRINGISE_ENUM_NAMED(spv::StorageClassMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::ImageFormat &el)
{
  BEGIN_ENUM_STRINGISE(spv::ImageFormat)
  {
    STRINGISE_ENUM_NAMED(spv::ImageFormatUnknown, "Unknown");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba32f, "Rgba32f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba16f, "Rgba16f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR32f, "R32f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba8, "Rgba8");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba8Snorm, "Rgba8Snorm");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg32f, "Rg32f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg16f, "Rg16f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR11fG11fB10f, "R11fG11fB10f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR16f, "R16f");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba16, "Rgba16");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgb10A2, "Rgb10A2");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg16, "Rg16");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg8, "Rg8");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR16, "R16");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR8, "R8");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba16Snorm, "Rgba16Snorm");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg16Snorm, "Rg16Snorm");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg8Snorm, "Rg8Snorm");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR16Snorm, "R16Snorm");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR8Snorm, "R8Snorm");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba32i, "Rgba32i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba16i, "Rgba16i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba8i, "Rgba8i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR32i, "R32i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg32i, "Rg32i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg16i, "Rg16i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg8i, "Rg8i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR16i, "R16i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR8i, "R8i");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba32ui, "Rgba32ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba16ui, "Rgba16ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgba8ui, "Rgba8ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR32ui, "R32ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRgb10a2ui, "Rgb10a2ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg32ui, "Rg32ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg16ui, "Rg16ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatRg8ui, "Rg8ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR16ui, "R16ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatR8ui, "R8ui");
    STRINGISE_ENUM_NAMED(spv::ImageFormatMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::BuiltIn &el)
{
  BEGIN_ENUM_STRINGISE(spv::BuiltIn)
  {
    STRINGISE_ENUM_NAMED(spv::BuiltInPosition, "Position");
    STRINGISE_ENUM_NAMED(spv::BuiltInPointSize, "PointSize");
    STRINGISE_ENUM_NAMED(spv::BuiltInClipDistance, "ClipDistance");
    STRINGISE_ENUM_NAMED(spv::BuiltInCullDistance, "CullDistance");
    STRINGISE_ENUM_NAMED(spv::BuiltInVertexId, "VertexId");
    STRINGISE_ENUM_NAMED(spv::BuiltInInstanceId, "InstanceId");
    STRINGISE_ENUM_NAMED(spv::BuiltInPrimitiveId, "PrimitiveId");
    STRINGISE_ENUM_NAMED(spv::BuiltInInvocationId, "InvocationId");
    STRINGISE_ENUM_NAMED(spv::BuiltInLayer, "Layer");
    STRINGISE_ENUM_NAMED(spv::BuiltInViewportIndex, "ViewportIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInTessLevelOuter, "TessLevelOuter");
    STRINGISE_ENUM_NAMED(spv::BuiltInTessLevelInner, "TessLevelInner");
    STRINGISE_ENUM_NAMED(spv::BuiltInTessCoord, "TessCoord");
    STRINGISE_ENUM_NAMED(spv::BuiltInPatchVertices, "PatchVertices");
    STRINGISE_ENUM_NAMED(spv::BuiltInFragCoord, "FragCoord");
    STRINGISE_ENUM_NAMED(spv::BuiltInPointCoord, "PointCoord");
    STRINGISE_ENUM_NAMED(spv::BuiltInFrontFacing, "FrontFacing");
    STRINGISE_ENUM_NAMED(spv::BuiltInSampleId, "SampleId");
    STRINGISE_ENUM_NAMED(spv::BuiltInSamplePosition, "SamplePosition");
    STRINGISE_ENUM_NAMED(spv::BuiltInSampleMask, "SampleMask");
    STRINGISE_ENUM_NAMED(spv::BuiltInFragDepth, "FragDepth");
    STRINGISE_ENUM_NAMED(spv::BuiltInHelperInvocation, "HelperInvocation");
    STRINGISE_ENUM_NAMED(spv::BuiltInNumWorkgroups, "NumWorkgroups");
    STRINGISE_ENUM_NAMED(spv::BuiltInWorkgroupSize, "WorkgroupSize");
    STRINGISE_ENUM_NAMED(spv::BuiltInWorkgroupId, "WorkgroupId");
    STRINGISE_ENUM_NAMED(spv::BuiltInLocalInvocationId, "LocalInvocationId");
    STRINGISE_ENUM_NAMED(spv::BuiltInGlobalInvocationId, "GlobalInvocationId");
    STRINGISE_ENUM_NAMED(spv::BuiltInLocalInvocationIndex, "LocalInvocationIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInWorkDim, "WorkDim");
    STRINGISE_ENUM_NAMED(spv::BuiltInGlobalSize, "GlobalSize");
    STRINGISE_ENUM_NAMED(spv::BuiltInEnqueuedWorkgroupSize, "EnqueuedWorkgroupSize");
    STRINGISE_ENUM_NAMED(spv::BuiltInGlobalOffset, "GlobalOffset");
    STRINGISE_ENUM_NAMED(spv::BuiltInGlobalLinearId, "GlobalLinearId");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupSize, "SubgroupSize");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupMaxSize, "SubgroupMaxSize");
    STRINGISE_ENUM_NAMED(spv::BuiltInNumSubgroups, "NumSubgroups");
    STRINGISE_ENUM_NAMED(spv::BuiltInNumEnqueuedSubgroups, "NumEnqueuedSubgroups");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupId, "SubgroupId");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupLocalInvocationId, "SubgroupLocalInvocationId");
    STRINGISE_ENUM_NAMED(spv::BuiltInVertexIndex, "VertexIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInInstanceIndex, "InstanceIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupEqMask, "SubgroupEqMask");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupGeMask, "SubgroupGeMask");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupGtMask, "SubgroupGtMask");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupLeMask, "SubgroupLeMask");
    STRINGISE_ENUM_NAMED(spv::BuiltInSubgroupLtMask, "SubgroupLtMask");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaseVertex, "BaseVertex");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaseInstance, "BaseInstance");
    STRINGISE_ENUM_NAMED(spv::BuiltInDrawIndex, "DrawIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInDeviceIndex, "DeviceIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInViewIndex, "ViewIndex");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordNoPerspAMD, "BaryCoordNoPerspAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordNoPerspCentroidAMD, "BaryCoordNoPerspCentroidAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordNoPerspSampleAMD, "BaryCoordNoPerspSampleAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordSmoothAMD, "BaryCoordSmoothAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordSmoothCentroidAMD, "BaryCoordSmoothCentroidAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordSmoothSampleAMD, "BaryCoordSmoothSampleAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordPullModelAMD, "BaryCoordPullModelAMD");
    STRINGISE_ENUM_NAMED(spv::BuiltInFragStencilRefEXT, "FragStencilRefEXT");
    STRINGISE_ENUM_NAMED(spv::BuiltInViewportMaskNV, "ViewportMaskNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInSecondaryPositionNV, "SecondaryPositionNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInSecondaryViewportMaskNV, "SecondaryViewportMaskNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInPositionPerViewNV, "PositionPerViewNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInViewportMaskPerViewNV, "ViewportMaskPerViewNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInFullyCoveredEXT, "FullyCoveredEXT");
    STRINGISE_ENUM_NAMED(spv::BuiltInTaskCountNV, "TaskCountNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInPrimitiveCountNV, "PrimitiveCountNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInPrimitiveIndicesNV, "PrimitiveIndicesNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInClipDistancePerViewNV, "ClipDistancePerViewNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInCullDistancePerViewNV, "CullDistancePerViewNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInLayerPerViewNV, "LayerPerViewNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInMeshViewCountNV, "MeshViewCountNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInMeshViewIndicesNV, "MeshViewIndicesNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordNV, "BaryCoordNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInBaryCoordNoPerspNV, "BaryCoordNoPerspNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInFragSizeEXT, "FragSizeEXT");
    STRINGISE_ENUM_NAMED(spv::BuiltInFragInvocationCountEXT, "FragInvocationCountEXT");
    STRINGISE_ENUM_NAMED(spv::BuiltInLaunchIdNV, "LaunchIdNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInLaunchSizeNV, "LaunchSizeNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInWorldRayOriginNV, "WorldRayOriginNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInWorldRayDirectionNV, "WorldRayDirectionNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInObjectRayOriginNV, "ObjectRayOriginNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInObjectRayDirectionNV, "ObjectRayDirectionNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInRayTminNV, "RayTminNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInRayTmaxNV, "RayTmaxNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInInstanceCustomIndexNV, "InstanceCustomIndexNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInObjectToWorldNV, "ObjectToWorldNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInWorldToObjectNV, "WorldToObjectNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInHitTNV, "HitTNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInHitKindNV, "HitKindNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInIncomingRayFlagsNV, "IncomingRayFlagsNV");
    STRINGISE_ENUM_NAMED(spv::BuiltInMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::Scope &el)
{
  BEGIN_ENUM_STRINGISE(spv::Scope)
  {
    STRINGISE_ENUM_NAMED(spv::ScopeCrossDevice, "CrossDevice");
    STRINGISE_ENUM_NAMED(spv::ScopeDevice, "Device");
    STRINGISE_ENUM_NAMED(spv::ScopeWorkgroup, "Workgroup");
    STRINGISE_ENUM_NAMED(spv::ScopeSubgroup, "Subgroup");
    STRINGISE_ENUM_NAMED(spv::ScopeInvocation, "Invocation");
    STRINGISE_ENUM_NAMED(spv::ScopeQueueFamilyKHR, "QueueFamilyKHR");
    STRINGISE_ENUM_NAMED(spv::ScopeMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::GroupOperation &el)
{
  BEGIN_ENUM_STRINGISE(spv::GroupOperation)
  {
    STRINGISE_ENUM_NAMED(spv::GroupOperationReduce, "Reduce");
    STRINGISE_ENUM_NAMED(spv::GroupOperationInclusiveScan, "InclusiveScan");
    STRINGISE_ENUM_NAMED(spv::GroupOperationExclusiveScan, "ExclusiveScan");
    STRINGISE_ENUM_NAMED(spv::GroupOperationClusteredReduce, "ClusteredReduce");
    STRINGISE_ENUM_NAMED(spv::GroupOperationPartitionedReduceNV, "PartitionedReduceNV");
    STRINGISE_ENUM_NAMED(spv::GroupOperationPartitionedInclusiveScanNV,
                         "PartitionedInclusiveScanNV");
    STRINGISE_ENUM_NAMED(spv::GroupOperationPartitionedExclusiveScanNV,
                         "PartitionedExclusiveScanNV");
    STRINGISE_ENUM_NAMED(spv::GroupOperationMax, "Max");
  }
  END_ENUM_STRINGISE()
}

template <>
rdcstr DoStringise(const spv::FunctionControlMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::FunctionControlMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::FunctionControlInlineMask, "Inline");
    STRINGISE_BITFIELD_BIT_NAMED(spv::FunctionControlDontInlineMask, "DontInline");
    STRINGISE_BITFIELD_BIT_NAMED(spv::FunctionControlPureMask, "Pure");
    STRINGISE_BITFIELD_BIT_NAMED(spv::FunctionControlConstMask, "Const");
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const spv::SelectionControlMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::SelectionControlMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::SelectionControlFlattenMask, "Flatten");
    STRINGISE_BITFIELD_BIT_NAMED(spv::SelectionControlDontFlattenMask, "DontFlatten");
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const spv::LoopControlMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::LoopControlMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::LoopControlUnrollMask, "Unroll");
    STRINGISE_BITFIELD_BIT_NAMED(spv::LoopControlDontUnrollMask, "DontUnroll");
    STRINGISE_BITFIELD_BIT_NAMED(spv::LoopControlDependencyInfiniteMask, "DependencyInfinite");
    STRINGISE_BITFIELD_BIT_NAMED(spv::LoopControlDependencyLengthMask, "DependencyLength");
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const spv::MemoryAccessMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::MemoryAccessMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessVolatileMask, "Volatile");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessAlignedMask, "Aligned");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessNontemporalMask, "Nontemporal");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessMakePointerAvailableKHRMask,
                                 "MakePointerAvailable");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessMakePointerVisibleKHRMask, "MakePointerVisible");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessNonPrivatePointerKHRMask, "NonPrivatePointer");
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const spv::MemorySemanticsMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::MemorySemanticsMask);
  {
    STRINGISE_BITFIELD_VALUE_NAMED(spv::MemorySemanticsMaskNone, "None");

    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsAcquireMask, "Acquire");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsReleaseMask, "Release");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsAcquireReleaseMask, "Acquire/Release");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsSequentiallyConsistentMask,
                                 "Sequentially Consistent");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsUniformMemoryMask, "Uniform Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsSubgroupMemoryMask, "Subgroup Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsWorkgroupMemoryMask, "Workgroup Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsCrossWorkgroupMemoryMask,
                                 "Cross Workgroup Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsAtomicCounterMemoryMask,
                                 "Atomic Counter Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsImageMemoryMask, "Image Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsOutputMemoryKHRMask, "Output Memory");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsMakeAvailableKHRMask, "Make Available");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemorySemanticsMakeVisibleKHRMask, "Make Visible");
  }
  END_BITFIELD_STRINGISE();
}
