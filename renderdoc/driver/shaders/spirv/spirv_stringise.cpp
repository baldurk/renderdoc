/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
std::string DoStringise(const spv::Op &el)
{
  switch(el)
  {
    case spv::OpNop: return "Nop";
    case spv::OpUndef: return "Undef";
    case spv::OpSourceContinued: return "SourceContinued";
    case spv::OpSource: return "Source";
    case spv::OpSourceExtension: return "SourceExtension";
    case spv::OpName: return "Name";
    case spv::OpMemberName: return "MemberName";
    case spv::OpString: return "String";
    case spv::OpLine: return "Line";
    case spv::OpExtension: return "Extension";
    case spv::OpExtInstImport: return "ExtInstImport";
    case spv::OpExtInst: return "ExtInst";
    case spv::OpMemoryModel: return "MemoryModel";
    case spv::OpEntryPoint: return "EntryPoint";
    case spv::OpExecutionMode: return "ExecutionMode";
    case spv::OpCapability: return "Capability";
    case spv::OpTypeVoid: return "TypeVoid";
    case spv::OpTypeBool: return "TypeBool";
    case spv::OpTypeInt: return "TypeInt";
    case spv::OpTypeFloat: return "TypeFloat";
    case spv::OpTypeVector: return "TypeVector";
    case spv::OpTypeMatrix: return "TypeMatrix";
    case spv::OpTypeImage: return "TypeImage";
    case spv::OpTypeSampler: return "TypeSampler";
    case spv::OpTypeSampledImage: return "TypeSampledImage";
    case spv::OpTypeArray: return "TypeArray";
    case spv::OpTypeRuntimeArray: return "TypeRuntimeArray";
    case spv::OpTypeStruct: return "TypeStruct";
    case spv::OpTypeOpaque: return "TypeOpaque";
    case spv::OpTypePointer: return "TypePointer";
    case spv::OpTypeFunction: return "TypeFunction";
    case spv::OpTypeEvent: return "TypeEvent";
    case spv::OpTypeDeviceEvent: return "TypeDeviceEvent";
    case spv::OpTypeReserveId: return "TypeReserveId";
    case spv::OpTypeQueue: return "TypeQueue";
    case spv::OpTypePipe: return "TypePipe";
    case spv::OpTypeForwardPointer: return "TypeForwardPointer";
    case spv::OpConstantTrue: return "ConstantTrue";
    case spv::OpConstantFalse: return "ConstantFalse";
    case spv::OpConstant: return "Constant";
    case spv::OpConstantComposite: return "ConstantComposite";
    case spv::OpConstantSampler: return "ConstantSampler";
    case spv::OpConstantNull: return "ConstantNull";
    case spv::OpSpecConstantTrue: return "SpecConstantTrue";
    case spv::OpSpecConstantFalse: return "SpecConstantFalse";
    case spv::OpSpecConstant: return "SpecConstant";
    case spv::OpSpecConstantComposite: return "SpecConstantComposite";
    case spv::OpSpecConstantOp: return "SpecConstantOp";
    case spv::OpFunction: return "Function";
    case spv::OpFunctionParameter: return "FunctionParameter";
    case spv::OpFunctionEnd: return "FunctionEnd";
    case spv::OpFunctionCall: return "FunctionCall";
    case spv::OpVariable: return "Variable";
    case spv::OpImageTexelPointer: return "ImageTexelPointer";
    case spv::OpLoad: return "Load";
    case spv::OpStore: return "Store";
    case spv::OpCopyMemory: return "CopyMemory";
    case spv::OpCopyMemorySized: return "CopyMemorySized";
    case spv::OpAccessChain: return "AccessChain";
    case spv::OpInBoundsAccessChain: return "InBoundsAccessChain";
    case spv::OpPtrAccessChain: return "PtrAccessChain";
    case spv::OpArrayLength: return "ArrayLength";
    case spv::OpGenericPtrMemSemantics: return "GenericPtrMemSemantics";
    case spv::OpInBoundsPtrAccessChain: return "InBoundsPtrAccessChain";
    case spv::OpDecorate: return "Decorate";
    case spv::OpMemberDecorate: return "MemberDecorate";
    case spv::OpDecorationGroup: return "DecorationGroup";
    case spv::OpGroupDecorate: return "GroupDecorate";
    case spv::OpGroupMemberDecorate: return "GroupMemberDecorate";
    case spv::OpVectorExtractDynamic: return "VectorExtractDynamic";
    case spv::OpVectorInsertDynamic: return "VectorInsertDynamic";
    case spv::OpVectorShuffle: return "VectorShuffle";
    case spv::OpCompositeConstruct: return "CompositeConstruct";
    case spv::OpCompositeExtract: return "CompositeExtract";
    case spv::OpCompositeInsert: return "CompositeInsert";
    case spv::OpCopyObject: return "CopyObject";
    case spv::OpTranspose: return "Transpose";
    case spv::OpSampledImage: return "SampledImage";
    case spv::OpImageSampleImplicitLod: return "ImageSampleImplicitLod";
    case spv::OpImageSampleExplicitLod: return "ImageSampleExplicitLod";
    case spv::OpImageSampleDrefImplicitLod: return "ImageSampleDrefImplicitLod";
    case spv::OpImageSampleDrefExplicitLod: return "ImageSampleDrefExplicitLod";
    case spv::OpImageSampleProjImplicitLod: return "ImageSampleProjImplicitLod";
    case spv::OpImageSampleProjExplicitLod: return "ImageSampleProjExplicitLod";
    case spv::OpImageSampleProjDrefImplicitLod: return "ImageSampleProjDrefImplicitLod";
    case spv::OpImageSampleProjDrefExplicitLod: return "ImageSampleProjDrefExplicitLod";
    case spv::OpImageFetch: return "ImageFetch";
    case spv::OpImageGather: return "ImageGather";
    case spv::OpImageDrefGather: return "ImageDrefGather";
    case spv::OpImageRead: return "ImageRead";
    case spv::OpImageWrite: return "ImageWrite";
    case spv::OpImage: return "Image";
    case spv::OpImageQueryFormat: return "ImageQueryFormat";
    case spv::OpImageQueryOrder: return "ImageQueryOrder";
    case spv::OpImageQuerySizeLod: return "ImageQuerySizeLod";
    case spv::OpImageQuerySize: return "ImageQuerySize";
    case spv::OpImageQueryLod: return "ImageQueryLod";
    case spv::OpImageQueryLevels: return "ImageQueryLevels";
    case spv::OpImageQuerySamples: return "ImageQuerySamples";
    case spv::OpConvertFToU: return "ConvertFToU";
    case spv::OpConvertFToS: return "ConvertFToS";
    case spv::OpConvertSToF: return "ConvertSToF";
    case spv::OpConvertUToF: return "ConvertUToF";
    case spv::OpUConvert: return "UConvert";
    case spv::OpSConvert: return "SConvert";
    case spv::OpFConvert: return "FConvert";
    case spv::OpQuantizeToF16: return "QuantizeToF16";
    case spv::OpConvertPtrToU: return "ConvertPtrToU";
    case spv::OpSatConvertSToU: return "SatConvertSToU";
    case spv::OpSatConvertUToS: return "SatConvertUToS";
    case spv::OpConvertUToPtr: return "ConvertUToPtr";
    case spv::OpPtrCastToGeneric: return "PtrCastToGeneric";
    case spv::OpGenericCastToPtr: return "GenericCastToPtr";
    case spv::OpGenericCastToPtrExplicit: return "GenericCastToPtrExplicit";
    case spv::OpBitcast: return "Bitcast";
    case spv::OpSNegate: return "SNegate";
    case spv::OpFNegate: return "FNegate";
    case spv::OpIAdd: return "IAdd";
    case spv::OpFAdd: return "FAdd";
    case spv::OpISub: return "ISub";
    case spv::OpFSub: return "FSub";
    case spv::OpIMul: return "IMul";
    case spv::OpFMul: return "FMul";
    case spv::OpUDiv: return "UDiv";
    case spv::OpSDiv: return "SDiv";
    case spv::OpFDiv: return "FDiv";
    case spv::OpUMod: return "UMod";
    case spv::OpSRem: return "SRem";
    case spv::OpSMod: return "SMod";
    case spv::OpFRem: return "FRem";
    case spv::OpFMod: return "FMod";
    case spv::OpVectorTimesScalar: return "VectorTimesScalar";
    case spv::OpMatrixTimesScalar: return "MatrixTimesScalar";
    case spv::OpVectorTimesMatrix: return "VectorTimesMatrix";
    case spv::OpMatrixTimesVector: return "MatrixTimesVector";
    case spv::OpMatrixTimesMatrix: return "MatrixTimesMatrix";
    case spv::OpOuterProduct: return "OuterProduct";
    case spv::OpDot: return "Dot";
    case spv::OpIAddCarry: return "IAddCarry";
    case spv::OpISubBorrow: return "ISubBorrow";
    case spv::OpUMulExtended: return "UMulExtended";
    case spv::OpSMulExtended: return "SMulExtended";
    case spv::OpAny: return "Any";
    case spv::OpAll: return "All";
    case spv::OpIsNan: return "IsNan";
    case spv::OpIsInf: return "IsInf";
    case spv::OpIsFinite: return "IsFinite";
    case spv::OpIsNormal: return "IsNormal";
    case spv::OpSignBitSet: return "SignBitSet";
    case spv::OpLessOrGreater: return "LessOrGreater";
    case spv::OpOrdered: return "Ordered";
    case spv::OpUnordered: return "Unordered";
    case spv::OpLogicalEqual: return "LogicalEqual";
    case spv::OpLogicalNotEqual: return "LogicalNotEqual";
    case spv::OpLogicalOr: return "LogicalOr";
    case spv::OpLogicalAnd: return "LogicalAnd";
    case spv::OpLogicalNot: return "LogicalNot";
    case spv::OpSelect: return "Select";
    case spv::OpIEqual: return "IEqual";
    case spv::OpINotEqual: return "INotEqual";
    case spv::OpUGreaterThan: return "UGreaterThan";
    case spv::OpSGreaterThan: return "SGreaterThan";
    case spv::OpUGreaterThanEqual: return "UGreaterThanEqual";
    case spv::OpSGreaterThanEqual: return "SGreaterThanEqual";
    case spv::OpULessThan: return "ULessThan";
    case spv::OpSLessThan: return "SLessThan";
    case spv::OpULessThanEqual: return "ULessThanEqual";
    case spv::OpSLessThanEqual: return "SLessThanEqual";
    case spv::OpFOrdEqual: return "FOrdEqual";
    case spv::OpFUnordEqual: return "FUnordEqual";
    case spv::OpFOrdNotEqual: return "FOrdNotEqual";
    case spv::OpFUnordNotEqual: return "FUnordNotEqual";
    case spv::OpFOrdLessThan: return "FOrdLessThan";
    case spv::OpFUnordLessThan: return "FUnordLessThan";
    case spv::OpFOrdGreaterThan: return "FOrdGreaterThan";
    case spv::OpFUnordGreaterThan: return "FUnordGreaterThan";
    case spv::OpFOrdLessThanEqual: return "FOrdLessThanEqual";
    case spv::OpFUnordLessThanEqual: return "FUnordLessThanEqual";
    case spv::OpFOrdGreaterThanEqual: return "FOrdGreaterThanEqual";
    case spv::OpFUnordGreaterThanEqual: return "FUnordGreaterThanEqual";
    case spv::OpShiftRightLogical: return "ShiftRightLogical";
    case spv::OpShiftRightArithmetic: return "ShiftRightArithmetic";
    case spv::OpShiftLeftLogical: return "ShiftLeftLogical";
    case spv::OpBitwiseOr: return "BitwiseOr";
    case spv::OpBitwiseXor: return "BitwiseXor";
    case spv::OpBitwiseAnd: return "BitwiseAnd";
    case spv::OpNot: return "Not";
    case spv::OpBitFieldInsert: return "BitFieldInsert";
    case spv::OpBitFieldSExtract: return "BitFieldSExtract";
    case spv::OpBitFieldUExtract: return "BitFieldUExtract";
    case spv::OpBitReverse: return "BitReverse";
    case spv::OpBitCount: return "BitCount";
    case spv::OpDPdx: return "ddx";
    case spv::OpDPdy: return "ddy";
    case spv::OpFwidth: return "Fwidth";
    case spv::OpDPdxFine: return "ddx_fine";
    case spv::OpDPdyFine: return "ddy_fine";
    case spv::OpFwidthFine: return "Fwidth_fine";
    case spv::OpDPdxCoarse: return "ddx_coarse";
    case spv::OpDPdyCoarse: return "ddy_coarse";
    case spv::OpFwidthCoarse: return "Fwidth_coarse";
    case spv::OpEmitVertex: return "EmitVertex";
    case spv::OpEndPrimitive: return "EndPrimitive";
    case spv::OpEmitStreamVertex: return "EmitStreamVertex";
    case spv::OpEndStreamPrimitive: return "EndStreamPrimitive";
    case spv::OpControlBarrier: return "ControlBarrier";
    case spv::OpMemoryBarrier: return "MemoryBarrier";
    case spv::OpAtomicLoad: return "AtomicLoad";
    case spv::OpAtomicStore: return "AtomicStore";
    case spv::OpAtomicExchange: return "AtomicExchange";
    case spv::OpAtomicCompareExchange: return "AtomicCompareExchange";
    case spv::OpAtomicCompareExchangeWeak: return "AtomicCompareExchangeWeak";
    case spv::OpAtomicIIncrement: return "AtomicIIncrement";
    case spv::OpAtomicIDecrement: return "AtomicIDecrement";
    case spv::OpAtomicIAdd: return "AtomicIAdd";
    case spv::OpAtomicISub: return "AtomicISub";
    case spv::OpAtomicSMin: return "AtomicSMin";
    case spv::OpAtomicUMin: return "AtomicUMin";
    case spv::OpAtomicSMax: return "AtomicSMax";
    case spv::OpAtomicUMax: return "AtomicUMax";
    case spv::OpAtomicAnd: return "AtomicAnd";
    case spv::OpAtomicOr: return "AtomicOr";
    case spv::OpAtomicXor: return "AtomicXor";
    case spv::OpPhi: return "Phi";
    case spv::OpLoopMerge: return "LoopMerge";
    case spv::OpSelectionMerge: return "SelectionMerge";
    case spv::OpLabel: return "Label";
    case spv::OpBranch: return "Branch";
    case spv::OpBranchConditional: return "BranchConditional";
    case spv::OpSwitch: return "Switch";
    case spv::OpKill: return "Kill";
    case spv::OpReturn: return "Return";
    case spv::OpReturnValue: return "ReturnValue";
    case spv::OpUnreachable: return "Unreachable";
    case spv::OpLifetimeStart: return "LifetimeStart";
    case spv::OpLifetimeStop: return "LifetimeStop";
    case spv::OpGroupAsyncCopy: return "GroupAsyncCopy";
    case spv::OpGroupWaitEvents: return "GroupWaitEvents";
    case spv::OpGroupAll: return "GroupAll";
    case spv::OpGroupAny: return "GroupAny";
    case spv::OpGroupBroadcast: return "GroupBroadcast";
    case spv::OpGroupIAdd: return "GroupIAdd";
    case spv::OpGroupFAdd: return "GroupFAdd";
    case spv::OpGroupFMin: return "GroupFMin";
    case spv::OpGroupUMin: return "GroupUMin";
    case spv::OpGroupSMin: return "GroupSMin";
    case spv::OpGroupFMax: return "GroupFMax";
    case spv::OpGroupUMax: return "GroupUMax";
    case spv::OpGroupSMax: return "GroupSMax";
    case spv::OpReadPipe: return "ReadPipe";
    case spv::OpWritePipe: return "WritePipe";
    case spv::OpReservedReadPipe: return "ReservedReadPipe";
    case spv::OpReservedWritePipe: return "ReservedWritePipe";
    case spv::OpReserveReadPipePackets: return "ReserveReadPipePackets";
    case spv::OpReserveWritePipePackets: return "ReserveWritePipePackets";
    case spv::OpCommitReadPipe: return "CommitReadPipe";
    case spv::OpCommitWritePipe: return "CommitWritePipe";
    case spv::OpIsValidReserveId: return "IsValidReserveId";
    case spv::OpGetNumPipePackets: return "GetNumPipePackets";
    case spv::OpGetMaxPipePackets: return "GetMaxPipePackets";
    case spv::OpGroupReserveReadPipePackets: return "GroupReserveReadPipePackets";
    case spv::OpGroupReserveWritePipePackets: return "GroupReserveWritePipePackets";
    case spv::OpGroupCommitReadPipe: return "GroupCommitReadPipe";
    case spv::OpGroupCommitWritePipe: return "GroupCommitWritePipe";
    case spv::OpEnqueueMarker: return "EnqueueMarker";
    case spv::OpEnqueueKernel: return "EnqueueKernel";
    case spv::OpGetKernelNDrangeSubGroupCount: return "GetKernelNDrangeSubGroupCount";
    case spv::OpGetKernelNDrangeMaxSubGroupSize: return "GetKernelNDrangeMaxSubGroupSize";
    case spv::OpGetKernelWorkGroupSize: return "GetKernelWorkGroupSize";
    case spv::OpGetKernelPreferredWorkGroupSizeMultiple:
      return "GetKernelPreferredWorkGroupSizeMultiple";
    case spv::OpRetainEvent: return "RetainEvent";
    case spv::OpReleaseEvent: return "ReleaseEvent";
    case spv::OpCreateUserEvent: return "CreateUserEvent";
    case spv::OpIsValidEvent: return "IsValidEvent";
    case spv::OpSetUserEventStatus: return "SetUserEventStatus";
    case spv::OpCaptureEventProfilingInfo: return "CaptureEventProfilingInfo";
    case spv::OpGetDefaultQueue: return "GetDefaultQueue";
    case spv::OpBuildNDRange: return "BuildNDRange";
    case spv::OpImageSparseSampleImplicitLod: return "ImageSparseSampleImplicitLod";
    case spv::OpImageSparseSampleExplicitLod: return "ImageSparseSampleExplicitLod";
    case spv::OpImageSparseSampleDrefImplicitLod: return "ImageSparseSampleDrefImplicitLod";
    case spv::OpImageSparseSampleDrefExplicitLod: return "ImageSparseSampleDrefExplicitLod";
    case spv::OpImageSparseSampleProjImplicitLod: return "ImageSparseSampleProjImplicitLod";
    case spv::OpImageSparseSampleProjExplicitLod: return "ImageSparseSampleProjExplicitLod";
    case spv::OpImageSparseSampleProjDrefImplicitLod: return "ImageSparseSampleProjDrefImplicitLod";
    case spv::OpImageSparseSampleProjDrefExplicitLod: return "ImageSparseSampleProjDrefExplicitLod";
    case spv::OpImageSparseFetch: return "ImageSparseFetch";
    case spv::OpImageSparseGather: return "ImageSparseGather";
    case spv::OpImageSparseDrefGather: return "ImageSparseDrefGather";
    case spv::OpImageSparseTexelsResident: return "ImageSparseTexelsResident";
    case spv::OpNoLine: return "NoLine";
    case spv::OpAtomicFlagTestAndSet: return "AtomicFlagTestAndSet";
    case spv::OpAtomicFlagClear: return "AtomicFlagClear";
    case spv::OpImageSparseRead: return "ImageSparseRead";
    case spv::OpSubgroupBallotKHR: return "ImageSparseRead";
    case spv::OpSubgroupFirstInvocationKHR: return "SubgroupFirstInvocationKHR";
    case spv::OpSubgroupAllKHR: return "SubgroupAllKHR";
    case spv::OpSubgroupAnyKHR: return "SubgroupAnyKHR";
    case spv::OpSubgroupAllEqualKHR: return "SubgroupAllEqualKHR";
    case spv::OpSubgroupReadInvocationKHR: return "SubgroupReadInvocationKHR";
    case spv::OpGroupIAddNonUniformAMD: return "GroupIAddNonUniformAMD";
    case spv::OpGroupFAddNonUniformAMD: return "GroupFAddNonUniformAMD";
    case spv::OpGroupFMinNonUniformAMD: return "GroupFMinNonUniformAMD";
    case spv::OpGroupUMinNonUniformAMD: return "GroupUMinNonUniformAMD";
    case spv::OpGroupSMinNonUniformAMD: return "GroupSMinNonUniformAMD";
    case spv::OpGroupFMaxNonUniformAMD: return "GroupFMaxNonUniformAMD";
    case spv::OpGroupUMaxNonUniformAMD: return "GroupUMaxNonUniformAMD";
    case spv::OpGroupSMaxNonUniformAMD: return "GroupSMaxNonUniformAMD";
    case spv::OpMax: break;
  }

  return StringFormat::Fmt("UnrecognisedOp{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::SourceLanguage &el)
{
  switch(el)
  {
    case spv::SourceLanguageUnknown: return "Unknown";
    case spv::SourceLanguageESSL: return "ESSL";
    case spv::SourceLanguageGLSL: return "GLSL";
    case spv::SourceLanguageOpenCL_C: return "OpenCL C";
    case spv::SourceLanguageOpenCL_CPP: return "OpenCL C++";
    case spv::SourceLanguageHLSL: return "HLSL";
    case spv::SourceLanguageMax: break;
  }

  return StringFormat::Fmt("UnrecognisedLanguage{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::Capability &el)
{
  switch(el)
  {
    case spv::CapabilityMatrix: return "Matrix";
    case spv::CapabilityShader: return "Shader";
    case spv::CapabilityGeometry: return "Geometry";
    case spv::CapabilityTessellation: return "Tessellation";
    case spv::CapabilityAddresses: return "Addresses";
    case spv::CapabilityLinkage: return "Linkage";
    case spv::CapabilityKernel: return "Kernel";
    case spv::CapabilityVector16: return "Vector16";
    case spv::CapabilityFloat16Buffer: return "Float16Buffer";
    case spv::CapabilityFloat16: return "Float16";
    case spv::CapabilityFloat64: return "Float64";
    case spv::CapabilityInt64: return "Int64";
    case spv::CapabilityInt64Atomics: return "Int64Atomics";
    case spv::CapabilityImageBasic: return "ImageBasic";
    case spv::CapabilityImageReadWrite: return "ImageReadWrite";
    case spv::CapabilityImageMipmap: return "ImageMipmap";
    case spv::CapabilityPipes: return "Pipes";
    case spv::CapabilityGroups: return "Groups";
    case spv::CapabilityDeviceEnqueue: return "DeviceEnqueue";
    case spv::CapabilityLiteralSampler: return "LiteralSampler";
    case spv::CapabilityAtomicStorage: return "AtomicStorage";
    case spv::CapabilityInt16: return "Int16";
    case spv::CapabilityTessellationPointSize: return "TessellationPointSize";
    case spv::CapabilityGeometryPointSize: return "GeometryPointSize";
    case spv::CapabilityImageGatherExtended: return "ImageGatherExtended";
    case spv::CapabilityStorageImageMultisample: return "StorageImageMultisample";
    case spv::CapabilityUniformBufferArrayDynamicIndexing:
      return "UniformBufferArrayDynamicIndexing";
    case spv::CapabilitySampledImageArrayDynamicIndexing: return "SampledImageArrayDynamicIndexing";
    case spv::CapabilityStorageBufferArrayDynamicIndexing:
      return "StorageBufferArrayDynamicIndexing";
    case spv::CapabilityStorageImageArrayDynamicIndexing: return "StorageImageArrayDynamicIndexing";
    case spv::CapabilityClipDistance: return "ClipDistance";
    case spv::CapabilityCullDistance: return "CullDistance";
    case spv::CapabilityImageCubeArray: return "ImageCubeArray";
    case spv::CapabilitySampleRateShading: return "SampleRateShading";
    case spv::CapabilityImageRect: return "ImageRect";
    case spv::CapabilitySampledRect: return "SampledRect";
    case spv::CapabilityGenericPointer: return "GenericPointer";
    case spv::CapabilityInt8: return "Int8";
    case spv::CapabilityInputAttachment: return "InputAttachment";
    case spv::CapabilitySparseResidency: return "SparseResidency";
    case spv::CapabilityMinLod: return "MinLod";
    case spv::CapabilitySampled1D: return "Sampled1D";
    case spv::CapabilityImage1D: return "Image1D";
    case spv::CapabilitySampledCubeArray: return "SampledCubeArray";
    case spv::CapabilitySampledBuffer: return "SampledBuffer";
    case spv::CapabilityImageBuffer: return "ImageBuffer";
    case spv::CapabilityImageMSArray: return "ImageMSArray";
    case spv::CapabilityStorageImageExtendedFormats: return "StorageImageExtendedFormats";
    case spv::CapabilityImageQuery: return "ImageQuery";
    case spv::CapabilityDerivativeControl: return "DerivativeControl";
    case spv::CapabilityInterpolationFunction: return "InterpolationFunction";
    case spv::CapabilityTransformFeedback: return "TransformFeedback";
    case spv::CapabilityGeometryStreams: return "GeometryStreams";
    case spv::CapabilityStorageImageReadWithoutFormat: return "StorageImageReadWithoutFormat";
    case spv::CapabilityStorageImageWriteWithoutFormat: return "StorageImageWriteWithoutFormat";
    case spv::CapabilityMultiViewport: return "MultiViewport";
    case spv::CapabilitySubgroupBallotKHR: return "SubgroupBallotKHR";
    case spv::CapabilityDrawParameters: return "DrawParameters";
    case spv::CapabilitySubgroupVoteKHR: return "SubgroupVoteKHR";
    case spv::CapabilityStorageUniformBufferBlock16: return "StorageUniformBufferBlock16";
    case spv::CapabilityStorageUniform16: return "StorageUniform16";
    case spv::CapabilityStoragePushConstant16: return "StoragePushConstant16";
    case spv::CapabilityStorageInputOutput16: return "StorageInputOutput16";
    case spv::CapabilityDeviceGroup: return "DeviceGroup";
    case spv::CapabilityMultiView: return "MultiView";
    case spv::CapabilityVariablePointersStorageBuffer: return "VariablePointersStorageBuffer";
    case spv::CapabilityVariablePointers: return "VariablePointers";
    case spv::CapabilityAtomicStorageOps: return "AtomicStorageOps";
    case spv::CapabilitySampleMaskPostDepthCoverage: return "SampleMaskPostDepthCoverage";
    case spv::CapabilityImageGatherBiasLodAMD: return "ImageGatherBiasLodAMD";
    case spv::CapabilityStencilExportEXT: return "StencilExportEXT";
    case spv::CapabilitySampleMaskOverrideCoverageNV: return "SampleMaskOverrideCoverageNV";
    case spv::CapabilityGeometryShaderPassthroughNV: return "GeometryShaderPassthroughNV";
    case spv::CapabilityShaderViewportIndexLayerNV: return "ShaderViewportIndexLayerNV";
    case spv::CapabilityShaderViewportMaskNV: return "ShaderViewportMaskNV";
    case spv::CapabilityShaderStereoViewNV: return "ShaderStereoViewNV";
    case spv::CapabilityPerViewAttributesNV: return "PerViewAttributesNV";
    case spv::CapabilityMax: break;
  }

  return StringFormat::Fmt("UnrecognisedCap{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::ExecutionMode &el)
{
  switch(el)
  {
    case spv::ExecutionModeInvocations: return "Invocations";
    case spv::ExecutionModeSpacingEqual: return "SpacingEqual";
    case spv::ExecutionModeSpacingFractionalEven: return "SpacingFractionalEven";
    case spv::ExecutionModeSpacingFractionalOdd: return "SpacingFractionalOdd";
    case spv::ExecutionModeVertexOrderCw: return "VertexOrderCw";
    case spv::ExecutionModeVertexOrderCcw: return "VertexOrderCcw";
    case spv::ExecutionModePixelCenterInteger: return "PixelCenterInteger";
    case spv::ExecutionModeOriginUpperLeft: return "OriginUpperLeft";
    case spv::ExecutionModeOriginLowerLeft: return "OriginLowerLeft";
    case spv::ExecutionModeEarlyFragmentTests: return "EarlyFragmentTests";
    case spv::ExecutionModePointMode: return "PointMode";
    case spv::ExecutionModeXfb: return "Xfb";
    case spv::ExecutionModeDepthReplacing: return "DepthReplacing";
    case spv::ExecutionModeDepthGreater: return "DepthGreater";
    case spv::ExecutionModeDepthLess: return "DepthLess";
    case spv::ExecutionModeDepthUnchanged: return "DepthUnchanged";
    case spv::ExecutionModeLocalSize: return "LocalSize";
    case spv::ExecutionModeLocalSizeHint: return "LocalSizeHint";
    case spv::ExecutionModeInputPoints: return "InputPoints";
    case spv::ExecutionModeInputLines: return "InputLines";
    case spv::ExecutionModeInputLinesAdjacency: return "InputLinesAdjacency";
    case spv::ExecutionModeTriangles: return "Triangles";
    case spv::ExecutionModeInputTrianglesAdjacency: return "InputTrianglesAdjacency";
    case spv::ExecutionModeQuads: return "Quads";
    case spv::ExecutionModeIsolines: return "Isolines";
    case spv::ExecutionModeOutputVertices: return "OutputVertices";
    case spv::ExecutionModeOutputPoints: return "OutputPoints";
    case spv::ExecutionModeOutputLineStrip: return "OutputLineStrip";
    case spv::ExecutionModeOutputTriangleStrip: return "OutputTriangleStrip";
    case spv::ExecutionModeVecTypeHint: return "VecTypeHint";
    case spv::ExecutionModeContractionOff: return "ContractionOff";
    case spv::ExecutionModePostDepthCoverage: return "PostDepthCoverage";
    case spv::ExecutionModeMax: break;
  }

  return StringFormat::Fmt("UnrecognisedMode{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::AddressingModel &el)
{
  switch(el)
  {
    case spv::AddressingModelLogical: return "Logical";
    case spv::AddressingModelPhysical32: return "Physical (32-bit)";
    case spv::AddressingModelPhysical64: return "Physical (64-bit)";
    case spv::AddressingModelMax: break;
  }

  return StringFormat::Fmt("UnrecognisedModel{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::MemoryModel &el)
{
  switch(el)
  {
    case spv::MemoryModelSimple: return "Simple";
    case spv::MemoryModelGLSL450: return "GLSL450";
    case spv::MemoryModelOpenCL: return "OpenCL";
    case spv::MemoryModelMax: break;
  }

  return StringFormat::Fmt("UnrecognisedModel{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::ExecutionModel &el)
{
  switch(el)
  {
    case spv::ExecutionModelVertex: return "Vertex Shader";
    case spv::ExecutionModelTessellationControl: return "Tess. Control Shader";
    case spv::ExecutionModelTessellationEvaluation: return "Tess. Eval Shader";
    case spv::ExecutionModelGeometry: return "Geometry Shader";
    case spv::ExecutionModelFragment: return "Fragment Shader";
    case spv::ExecutionModelGLCompute: return "Compute Shader";
    case spv::ExecutionModelKernel: return "Kernel";
    case spv::ExecutionModelMax: break;
  }

  return StringFormat::Fmt("UnrecognisedModel{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::Decoration &el)
{
  switch(el)
  {
    case spv::DecorationRelaxedPrecision: return "RelaxedPrecision";
    case spv::DecorationSpecId: return "SpecId";
    case spv::DecorationBlock: return "Block";
    case spv::DecorationBufferBlock: return "BufferBlock";
    case spv::DecorationRowMajor: return "RowMajor";
    case spv::DecorationColMajor: return "ColMajor";
    case spv::DecorationArrayStride: return "ArrayStride";
    case spv::DecorationMatrixStride: return "MatrixStride";
    case spv::DecorationGLSLShared: return "GLSLShared";
    case spv::DecorationGLSLPacked: return "GLSLPacked";
    case spv::DecorationCPacked: return "CPacked";
    case spv::DecorationBuiltIn: return "BuiltIn";
    case spv::DecorationNoPerspective: return "NoPerspective";
    case spv::DecorationFlat: return "Flat";
    case spv::DecorationPatch: return "Patch";
    case spv::DecorationCentroid: return "Centroid";
    case spv::DecorationSample: return "Sample";
    case spv::DecorationInvariant: return "Invariant";
    case spv::DecorationRestrict: return "Restrict";
    case spv::DecorationAliased: return "Aliased";
    case spv::DecorationVolatile: return "Volatile";
    case spv::DecorationConstant: return "Constant";
    case spv::DecorationCoherent: return "Coherent";
    case spv::DecorationNonWritable: return "NonWritable";
    case spv::DecorationNonReadable: return "NonReadable";
    case spv::DecorationUniform: return "Uniform";
    case spv::DecorationSaturatedConversion: return "SaturatedConversion";
    case spv::DecorationStream: return "Stream";
    case spv::DecorationLocation: return "Location";
    case spv::DecorationComponent: return "Component";
    case spv::DecorationIndex: return "Index";
    case spv::DecorationBinding: return "Binding";
    case spv::DecorationDescriptorSet: return "DescriptorSet";
    case spv::DecorationOffset: return "Offset";
    case spv::DecorationXfbBuffer: return "XfbBuffer";
    case spv::DecorationXfbStride: return "XfbStride";
    case spv::DecorationFuncParamAttr: return "FuncParamAttr";
    case spv::DecorationFPRoundingMode: return "FPRoundingMode";
    case spv::DecorationFPFastMathMode: return "FPFastMathMode";
    case spv::DecorationLinkageAttributes: return "LinkageAttributes";
    case spv::DecorationNoContraction: return "NoContraction";
    case spv::DecorationInputAttachmentIndex: return "InputAttachmentIndex";
    case spv::DecorationAlignment: return "Alignment";
    case spv::DecorationExplicitInterpAMD: return "ExplicitInterpAMD";
    case spv::DecorationOverrideCoverageNV: return "OverrideCoverageNV";
    case spv::DecorationPassthroughNV: return "PassthroughNV";
    case spv::DecorationViewportRelativeNV: return "ViewportRelativeNV";
    case spv::DecorationSecondaryViewportRelativeNV: return "SecondaryViewportRelativeNV";
    case spv::DecorationMax: break;
  }

  return StringFormat::Fmt("UnrecognisedDecoration{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::Dim &el)
{
  switch(el)
  {
    case spv::Dim1D: return "1D";
    case spv::Dim2D: return "2D";
    case spv::Dim3D: return "3D";
    case spv::DimCube: return "Cube";
    case spv::DimRect: return "Rect";
    case spv::DimBuffer: return "Buffer";
    case spv::DimSubpassData: return "Subpass Data";
    case spv::DimMax: break;
  }

  return StringFormat::Fmt("{%u}D", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::StorageClass &el)
{
  switch(el)
  {
    case spv::StorageClassUniformConstant: return "UniformConstant";
    case spv::StorageClassInput: return "Input";
    case spv::StorageClassUniform: return "Uniform";
    case spv::StorageClassOutput: return "Output";
    case spv::StorageClassWorkgroup: return "Workgroup";
    case spv::StorageClassCrossWorkgroup: return "CrossWorkgroup";
    case spv::StorageClassPrivate: return "Private";
    case spv::StorageClassFunction: return "Function";
    case spv::StorageClassGeneric: return "Generic";
    case spv::StorageClassPushConstant: return "PushConstant";
    case spv::StorageClassAtomicCounter: return "AtomicCounter";
    case spv::StorageClassImage: return "Image";
    case spv::StorageClassStorageBuffer: return "StorageBuffer";
    case spv::StorageClassMax: break;
  }

  return StringFormat::Fmt("UnrecognisedClass{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::ImageFormat &el)
{
  switch(el)
  {
    case spv::ImageFormatUnknown: return "Unknown";
    case spv::ImageFormatRgba32f: return "RGBA32f";
    case spv::ImageFormatRgba16f: return "RGBA16f";
    case spv::ImageFormatR32f: return "R32f";
    case spv::ImageFormatRgba8: return "RGBA8";
    case spv::ImageFormatRgba8Snorm: return "RGBA8SNORM";
    case spv::ImageFormatRg32f: return "RG32F";
    case spv::ImageFormatRg16f: return "RG16F";
    case spv::ImageFormatR11fG11fB10f: return "R11FG11FB10F";
    case spv::ImageFormatR16f: return "R16F";
    case spv::ImageFormatRgba16: return "RGBA16";
    case spv::ImageFormatRgb10A2: return "RGB10A2";
    case spv::ImageFormatRg16: return "RG16";
    case spv::ImageFormatRg8: return "RG8";
    case spv::ImageFormatR16: return "R16";
    case spv::ImageFormatR8: return "R8";
    case spv::ImageFormatRgba16Snorm: return "RGBA16SNORM";
    case spv::ImageFormatRg16Snorm: return "RG16SNORM";
    case spv::ImageFormatRg8Snorm: return "RG8SNORM";
    case spv::ImageFormatR16Snorm: return "R16SNORM";
    case spv::ImageFormatR8Snorm: return "R8SNORM";
    case spv::ImageFormatRgba32i: return "RGBA32I";
    case spv::ImageFormatRgba16i: return "RGBA16I";
    case spv::ImageFormatRgba8i: return "RGBA8I";
    case spv::ImageFormatR32i: return "R32I";
    case spv::ImageFormatRg32i: return "RG32I";
    case spv::ImageFormatRg16i: return "RG16I";
    case spv::ImageFormatRg8i: return "RG8I";
    case spv::ImageFormatR16i: return "R16I";
    case spv::ImageFormatR8i: return "R8I";
    case spv::ImageFormatRgba32ui: return "RGBA32UI";
    case spv::ImageFormatRgba16ui: return "RGBA16UI";
    case spv::ImageFormatRgba8ui: return "RGBA8UI";
    case spv::ImageFormatR32ui: return "R32UI";
    case spv::ImageFormatRgb10a2ui: return "RGB10A2UI";
    case spv::ImageFormatRg32ui: return "RG32UI";
    case spv::ImageFormatRg16ui: return "RG16UI";
    case spv::ImageFormatRg8ui: return "RG8UI";
    case spv::ImageFormatR16ui: return "R16UI";
    case spv::ImageFormatR8ui: return "R8UI";
    case spv::ImageFormatMax: break;
  }

  return StringFormat::Fmt("UnrecognisedFormat{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::BuiltIn &el)
{
  switch(el)
  {
    case spv::BuiltInPosition: return "Position";
    case spv::BuiltInPointSize: return "PointSize";
    case spv::BuiltInClipDistance: return "ClipDistance";
    case spv::BuiltInCullDistance: return "CullDistance";
    case spv::BuiltInVertexId: return "VertexId";
    case spv::BuiltInInstanceId: return "InstanceId";
    case spv::BuiltInPrimitiveId: return "PrimitiveId";
    case spv::BuiltInInvocationId: return "InvocationId";
    case spv::BuiltInLayer: return "Layer";
    case spv::BuiltInViewportIndex: return "ViewportIndex";
    case spv::BuiltInTessLevelOuter: return "TessLevelOuter";
    case spv::BuiltInTessLevelInner: return "TessLevelInner";
    case spv::BuiltInTessCoord: return "TessCoord";
    case spv::BuiltInPatchVertices: return "PatchVertices";
    case spv::BuiltInFragCoord: return "FragCoord";
    case spv::BuiltInPointCoord: return "PointCoord";
    case spv::BuiltInFrontFacing: return "FrontFacing";
    case spv::BuiltInSampleId: return "SampleId";
    case spv::BuiltInSamplePosition: return "SamplePosition";
    case spv::BuiltInSampleMask: return "SampleMask";
    case spv::BuiltInFragDepth: return "FragDepth";
    case spv::BuiltInHelperInvocation: return "HelperInvocation";
    case spv::BuiltInNumWorkgroups: return "NumWorkgroups";
    case spv::BuiltInWorkgroupSize: return "WorkgroupSize";
    case spv::BuiltInWorkgroupId: return "WorkgroupId";
    case spv::BuiltInLocalInvocationId: return "LocalInvocationId";
    case spv::BuiltInGlobalInvocationId: return "GlobalInvocationId";
    case spv::BuiltInLocalInvocationIndex: return "LocalInvocationIndex";
    case spv::BuiltInWorkDim: return "WorkDim";
    case spv::BuiltInGlobalSize: return "GlobalSize";
    case spv::BuiltInEnqueuedWorkgroupSize: return "EnqueuedWorkgroupSize";
    case spv::BuiltInGlobalOffset: return "GlobalOffset";
    case spv::BuiltInGlobalLinearId: return "GlobalLinearId";
    case spv::BuiltInSubgroupSize: return "SubgroupSize";
    case spv::BuiltInSubgroupMaxSize: return "SubgroupMaxSize";
    case spv::BuiltInNumSubgroups: return "NumSubgroups";
    case spv::BuiltInNumEnqueuedSubgroups: return "NumEnqueuedSubgroups";
    case spv::BuiltInSubgroupId: return "SubgroupId";
    case spv::BuiltInSubgroupLocalInvocationId: return "SubgroupLocalInvocationId";
    case spv::BuiltInVertexIndex: return "VertexIndex";
    case spv::BuiltInInstanceIndex: return "InstanceIndex";
    case spv::BuiltInSubgroupEqMaskKHR: return "SubgroupEqMaskKHR";
    case spv::BuiltInSubgroupGeMaskKHR: return "SubgroupGeMaskKHR";
    case spv::BuiltInSubgroupGtMaskKHR: return "SubgroupGtMaskKHR";
    case spv::BuiltInSubgroupLeMaskKHR: return "SubgroupLeMaskKHR";
    case spv::BuiltInSubgroupLtMaskKHR: return "SubgroupLtMaskKHR";
    case spv::BuiltInBaseVertex: return "BaseVertex";
    case spv::BuiltInBaseInstance: return "BaseInstance";
    case spv::BuiltInDrawIndex: return "DrawIndex";
    case spv::BuiltInDeviceIndex: return "DeviceIndex";
    case spv::BuiltInViewIndex: return "ViewIndex";
    case spv::BuiltInBaryCoordNoPerspAMD: return "BaryCoordNoPerspAMD";
    case spv::BuiltInBaryCoordNoPerspCentroidAMD: return "BaryCoordNoPerspCentroidAMD";
    case spv::BuiltInBaryCoordNoPerspSampleAMD: return "BaryCoordNoPerspSampleAMD";
    case spv::BuiltInBaryCoordSmoothAMD: return "BaryCoordSmoothAMD";
    case spv::BuiltInBaryCoordSmoothCentroidAMD: return "BaryCoordSmoothCentroidAMD";
    case spv::BuiltInBaryCoordSmoothSampleAMD: return "BaryCoordSmoothSampleAMD";
    case spv::BuiltInBaryCoordPullModelAMD: return "BaryCoordPullModelAMD";
    case spv::BuiltInFragStencilRefEXT: return "FragStencilRefEXT";
    case spv::BuiltInViewportMaskNV: return "ViewportMaskNV";
    case spv::BuiltInSecondaryPositionNV: return "SecondaryPositionNV";
    case spv::BuiltInSecondaryViewportMaskNV: return "SecondaryViewportMaskNV";
    case spv::BuiltInPositionPerViewNV: return "PositionPerViewNV";
    case spv::BuiltInViewportMaskPerViewNV: return "ViewportMaskPerViewNV";
    case spv::BuiltInMax: break;
  }

  return StringFormat::Fmt("UnrecognisedBuiltIn{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::Scope &el)
{
  switch(el)
  {
    case spv::ScopeCrossDevice: return "CrossDevice";
    case spv::ScopeDevice: return "Device";
    case spv::ScopeWorkgroup: return "Workgroup";
    case spv::ScopeSubgroup: return "Subgroup";
    case spv::ScopeInvocation: return "Invocation";
    case spv::ScopeMax: break;
  }

  return StringFormat::Fmt("UnrecognisedScope{%u}", (uint32_t)el);
}

template <>
std::string DoStringise(const spv::FunctionControlMask &el)
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
std::string DoStringise(const spv::SelectionControlMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::SelectionControlMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::SelectionControlFlattenMask, "Flatten");
    STRINGISE_BITFIELD_BIT_NAMED(spv::SelectionControlDontFlattenMask, "DontFlatten");
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const spv::LoopControlMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::LoopControlMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::LoopControlUnrollMask, "Unroll");
    STRINGISE_BITFIELD_BIT_NAMED(spv::LoopControlDontUnrollMask, "DontUnroll");
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const spv::MemoryAccessMask &el)
{
  BEGIN_BITFIELD_STRINGISE(spv::MemoryAccessMask);
  {
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessVolatileMask, "Volatile");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessAlignedMask, "Aligned");
    STRINGISE_BITFIELD_BIT_NAMED(spv::MemoryAccessNontemporalMask, "Nontemporal");
  }
  END_BITFIELD_STRINGISE();
}

template <>
std::string DoStringise(const spv::MemorySemanticsMask &el)
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
  }
  END_BITFIELD_STRINGISE();
}
