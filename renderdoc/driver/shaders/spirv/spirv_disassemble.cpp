/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include "common/common.h"

#include "serialise/serialiser.h"

#include "spirv_common.h"

#undef min
#undef max

#include "3rdparty/glslang/SPIRV/spirv.h"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

void DisassembleSPIRV(SPIRVShaderStage shadType, const vector<uint32_t> &spirv, string &disasm)
{
	if(shadType >= eSPIRVInvalid)
		return;

	// temporary function until we build our own structure from the SPIR-V
	const char *header[] = {
		"Vertex Shader",
		"Tessellation Control Shader",
		"Tessellation Evaluation Shader",
		"Geometry Shader",
		"Fragment Shader",
		"Compute Shader",
	};

	disasm = header[(int)shadType];
	disasm += " SPIR-V:\n\n";

	if(spirv[0] != (uint32_t)spv::MagicNumber)
	{
		disasm += StringFormat::Fmt("Unrecognised magic number %08x", spirv[0]);
		return;
	}

	const char *gen = "Unrecognised";

	// list of known generators, just for kicks
	struct { uint32_t magic; const char *name; } gens[] = {
		{ 0x051a00bb, "glslang" },
	};

	for(size_t i=0; i < ARRAY_COUNT(gens); i++) if(gens[i].magic == spirv[2])	gen = gens[i].name;
	
	disasm += StringFormat::Fmt("Version %u, Generator %08x (%s)\n", spirv[1], spirv[2], gen);
	disasm += StringFormat::Fmt("IDs up to <%u>\n", spirv[3]);

	uint32_t idbound = spirv[3];

	if(spirv[4] != 0) disasm += "Reserved word 4 is non-zero\n";

	disasm += "\n";

	uint32_t opidx = 0;
	bool infunc = false;

	vector<string> resultnames;
	resultnames.resize(idbound);

	// fetch names and things to be used in the second pass
	size_t it = 5;
	while(it < spirv.size())
	{
		uint16_t WordCount = spirv[it]>>16;
		spv::Op OpCode = spv::Op(spirv[it]&0xffff);

		if(OpCode == spv::OpName)
			resultnames[ spirv[it+1] ] = (const char *)&spirv[it+2];
		
		it += WordCount;
	}

	for(size_t i=0; i < resultnames.size(); i++)
		if(resultnames[i].empty())
			resultnames[i] = StringFormat::Fmt("<%d>", i);

	it = 5;
	while(it < spirv.size())
	{
		uint16_t WordCount = spirv[it]>>16;
		spv::Op OpCode = spv::Op(spirv[it]&0xffff);

		string body;
		bool silent = false;

		switch(OpCode)
		{
			case spv::OpSource:
				body = StringFormat::Fmt("%s %d", ToStr::Get(spv::SourceLanguage(spirv[it+1])).c_str(), spirv[it+2]);
				break;
			case spv::OpExtInstImport:
				resultnames[ spirv[it+1] ] = (char *)&spirv[it+2];
				body = StringFormat::Fmt("%s", (char *)&spirv[it+2]);
				break;
			case spv::OpMemoryModel:
				body = StringFormat::Fmt("%s Addressing, %s Memory model",
					ToStr::Get(spv::AddressingModel(spirv[it+1])).c_str(),
					ToStr::Get(spv::MemoryModel(spirv[it+2])).c_str());
				break;
			case spv::OpEntryPoint:
				body = StringFormat::Fmt("%s (%s)",
					resultnames[ spirv[it+2] ].c_str(),
					ToStr::Get(spv::ExecutionModel(spirv[it+1])).c_str());
				break;
			case spv::OpFunction:
				infunc = true;
				break;
			case spv::OpFunctionEnd:
				infunc = false;
				break;
			case spv::OpName:
				silent = true;
				break;
			default:
				break;
		}
		
		if(infunc)
		{
			disasm += StringFormat::Fmt("% 4u: %s %s\n", opidx, ToStr::Get(OpCode).c_str(), body.c_str());
			opidx++;
		}
		else if(!silent)
		{
			disasm += StringFormat::Fmt("      %s %s\n",        ToStr::Get(OpCode).c_str(), body.c_str());
		}

		it += WordCount;
	}
}

template<>
string ToStrHelper<false, spv::Op>::Get(const spv::Op &el)
{
	switch(el)
	{
		case spv::OpNop:                                      return "Nop";
		case spv::OpSource:                                   return "Source";
		case spv::OpSourceExtension:                          return "SourceExtension";
		case spv::OpExtension:                                return "Extension";
		case spv::OpExtInstImport:                            return "ExtInstImport";
		case spv::OpMemoryModel:                              return "MemoryModel";
		case spv::OpEntryPoint:                               return "EntryPoint";
		case spv::OpExecutionMode:                            return "ExecutionMode";
		case spv::OpTypeVoid:                                 return "TypeVoid";
		case spv::OpTypeBool:                                 return "TypeBool";
		case spv::OpTypeInt:                                  return "TypeInt";
		case spv::OpTypeFloat:                                return "TypeFloat";
		case spv::OpTypeVector:                               return "TypeVector";
		case spv::OpTypeMatrix:                               return "TypeMatrix";
		case spv::OpTypeSampler:                              return "TypeSampler";
		case spv::OpTypeFilter:                               return "TypeFilter";
		case spv::OpTypeArray:                                return "TypeArray";
		case spv::OpTypeRuntimeArray:                         return "TypeRuntimeArray";
		case spv::OpTypeStruct:                               return "TypeStruct";
		case spv::OpTypeOpaque:                               return "TypeOpaque";
		case spv::OpTypePointer:                              return "TypePointer";
		case spv::OpTypeFunction:                             return "TypeFunction";
		case spv::OpTypeEvent:                                return "TypeEvent";
		case spv::OpTypeDeviceEvent:                          return "TypeDeviceEvent";
		case spv::OpTypeReserveId:                            return "TypeReserveId";
		case spv::OpTypeQueue:                                return "TypeQueue";
		case spv::OpTypePipe:                                 return "TypePipe";
		case spv::OpConstantTrue:                             return "ConstantTrue";
		case spv::OpConstantFalse:                            return "ConstantFalse";
		case spv::OpConstant:                                 return "Constant";
		case spv::OpConstantComposite:                        return "ConstantComposite";
		case spv::OpConstantSampler:                          return "ConstantSampler";
		case spv::OpConstantNullPointer:                      return "ConstantNullPointer";
		case spv::OpConstantNullObject:                       return "ConstantNullObject";
		case spv::OpSpecConstantTrue:                         return "SpecConstantTrue";
		case spv::OpSpecConstantFalse:                        return "SpecConstantFalse";
		case spv::OpSpecConstant:                             return "SpecConstant";
		case spv::OpSpecConstantComposite:                    return "SpecConstantComposite";
		case spv::OpVariable:                                 return "Variable";
		case spv::OpVariableArray:                            return "VariableArray";
		case spv::OpFunction:                                 return "Function";
		case spv::OpFunctionParameter:                        return "FunctionParameter";
		case spv::OpFunctionEnd:                              return "FunctionEnd";
		case spv::OpFunctionCall:                             return "FunctionCall";
		case spv::OpExtInst:                                  return "ExtInst";
		case spv::OpUndef:                                    return "Undef";
		case spv::OpLoad:                                     return "Load";
		case spv::OpStore:                                    return "Store";
		case spv::OpPhi:                                      return "Phi";
		case spv::OpDecorationGroup:                          return "DecorationGroup";
		case spv::OpDecorate:                                 return "Decorate";
		case spv::OpMemberDecorate:                           return "MemberDecorate";
		case spv::OpGroupDecorate:                            return "GroupDecorate";
		case spv::OpGroupMemberDecorate:                      return "GroupMemberDecorate";
		case spv::OpName:                                     return "Name";
		case spv::OpMemberName:                               return "MemberName";
		case spv::OpString:                                   return "String";
		case spv::OpLine:                                     return "Line";
		case spv::OpVectorExtractDynamic:                     return "VectorExtractDynamic";
		case spv::OpVectorInsertDynamic:                      return "VectorInsertDynamic";
		case spv::OpVectorShuffle:                            return "VectorShuffle";
		case spv::OpCompositeConstruct:                       return "CompositeConstruct";
		case spv::OpCompositeExtract:                         return "CompositeExtract";
		case spv::OpCompositeInsert:                          return "CompositeInsert";
		case spv::OpCopyObject:                               return "CopyObject";
		case spv::OpCopyMemory:                               return "CopyMemory";
		case spv::OpCopyMemorySized:                          return "CopyMemorySized";
		case spv::OpSampler:                                  return "Sampler";
		case spv::OpTextureSample:                            return "TextureSample";
		case spv::OpTextureSampleDref:                        return "TextureSampleDref";
		case spv::OpTextureSampleLod:                         return "TextureSampleLod";
		case spv::OpTextureSampleProj:                        return "TextureSampleProj";
		case spv::OpTextureSampleGrad:                        return "TextureSampleGrad";
		case spv::OpTextureSampleOffset:                      return "TextureSampleOffset";
		case spv::OpTextureSampleProjLod:                     return "TextureSampleProjLod";
		case spv::OpTextureSampleProjGrad:                    return "TextureSampleProjGrad";
		case spv::OpTextureSampleLodOffset:                   return "TextureSampleLodOffset";
		case spv::OpTextureSampleProjOffset:                  return "TextureSampleProjOffset";
		case spv::OpTextureSampleGradOffset:                  return "TextureSampleGradOffset";
		case spv::OpTextureSampleProjLodOffset:               return "TextureSampleProjLodOffset";
		case spv::OpTextureSampleProjGradOffset:              return "TextureSampleProjGradOffset";
		case spv::OpTextureFetchTexelLod:                     return "TextureFetchTexelLod";
		case spv::OpTextureFetchTexelOffset:                  return "TextureFetchTexelOffset";
		case spv::OpTextureFetchSample:                       return "TextureFetchSample";
		case spv::OpTextureFetchTexel:                        return "TextureFetchTexel";
		case spv::OpTextureGather:                            return "TextureGather";
		case spv::OpTextureGatherOffset:                      return "TextureGatherOffset";
		case spv::OpTextureGatherOffsets:                     return "TextureGatherOffsets";
		case spv::OpTextureQuerySizeLod:                      return "TextureQuerySizeLod";
		case spv::OpTextureQuerySize:                         return "TextureQuerySize";
		case spv::OpTextureQueryLod:                          return "TextureQueryLod";
		case spv::OpTextureQueryLevels:                       return "TextureQueryLevels";
		case spv::OpTextureQuerySamples:                      return "TextureQuerySamples";
		case spv::OpAccessChain:                              return "AccessChain";
		case spv::OpInBoundsAccessChain:                      return "InBoundsAccessChain";
		case spv::OpSNegate:                                  return "SNegate";
		case spv::OpFNegate:                                  return "FNegate";
		case spv::OpNot:                                      return "Not";
		case spv::OpAny:                                      return "Any";
		case spv::OpAll:                                      return "All";
		case spv::OpConvertFToU:                              return "ConvertFToU";
		case spv::OpConvertFToS:                              return "ConvertFToS";
		case spv::OpConvertSToF:                              return "ConvertSToF";
		case spv::OpConvertUToF:                              return "ConvertUToF";
		case spv::OpUConvert:                                 return "UConvert";
		case spv::OpSConvert:                                 return "SConvert";
		case spv::OpFConvert:                                 return "FConvert";
		case spv::OpConvertPtrToU:                            return "ConvertPtrToU";
		case spv::OpConvertUToPtr:                            return "ConvertUToPtr";
		case spv::OpPtrCastToGeneric:                         return "PtrCastToGeneric";
		case spv::OpGenericCastToPtr:                         return "GenericCastToPtr";
		case spv::OpBitcast:                                  return "Bitcast";
		case spv::OpTranspose:                                return "Transpose";
		case spv::OpIsNan:                                    return "IsNan";
		case spv::OpIsInf:                                    return "IsInf";
		case spv::OpIsFinite:                                 return "IsFinite";
		case spv::OpIsNormal:                                 return "IsNormal";
		case spv::OpSignBitSet:                               return "SignBitSet";
		case spv::OpLessOrGreater:                            return "LessOrGreater";
		case spv::OpOrdered:                                  return "Ordered";
		case spv::OpUnordered:                                return "Unordered";
		case spv::OpArrayLength:                              return "ArrayLength";
		case spv::OpIAdd:                                     return "IAdd";
		case spv::OpFAdd:                                     return "FAdd";
		case spv::OpISub:                                     return "ISub";
		case spv::OpFSub:                                     return "FSub";
		case spv::OpIMul:                                     return "IMul";
		case spv::OpFMul:                                     return "FMul";
		case spv::OpUDiv:                                     return "UDiv";
		case spv::OpSDiv:                                     return "SDiv";
		case spv::OpFDiv:                                     return "FDiv";
		case spv::OpUMod:                                     return "UMod";
		case spv::OpSRem:                                     return "SRem";
		case spv::OpSMod:                                     return "SMod";
		case spv::OpFRem:                                     return "FRem";
		case spv::OpFMod:                                     return "FMod";
		case spv::OpVectorTimesScalar:                        return "VectorTimesScalar";
		case spv::OpMatrixTimesScalar:                        return "MatrixTimesScalar";
		case spv::OpVectorTimesMatrix:                        return "VectorTimesMatrix";
		case spv::OpMatrixTimesVector:                        return "MatrixTimesVector";
		case spv::OpMatrixTimesMatrix:                        return "MatrixTimesMatrix";
		case spv::OpOuterProduct:                             return "OuterProduct";
		case spv::OpDot:                                      return "Dot";
		case spv::OpShiftRightLogical:                        return "ShiftRightLogical";
		case spv::OpShiftRightArithmetic:                     return "ShiftRightArithmetic";
		case spv::OpShiftLeftLogical:                         return "ShiftLeftLogical";
		case spv::OpLogicalOr:                                return "LogicalOr";
		case spv::OpLogicalXor:                               return "LogicalXor";
		case spv::OpLogicalAnd:                               return "LogicalAnd";
		case spv::OpBitwiseOr:                                return "BitwiseOr";
		case spv::OpBitwiseXor:                               return "BitwiseXor";
		case spv::OpBitwiseAnd:                               return "BitwiseAnd";
		case spv::OpSelect:                                   return "Select";
		case spv::OpIEqual:                                   return "IEqual";
		case spv::OpFOrdEqual:                                return "FOrdEqual";
		case spv::OpFUnordEqual:                              return "FUnordEqual";
		case spv::OpINotEqual:                                return "INotEqual";
		case spv::OpFOrdNotEqual:                             return "FOrdNotEqual";
		case spv::OpFUnordNotEqual:                           return "FUnordNotEqual";
		case spv::OpULessThan:                                return "ULessThan";
		case spv::OpSLessThan:                                return "SLessThan";
		case spv::OpFOrdLessThan:                             return "FOrdLessThan";
		case spv::OpFUnordLessThan:                           return "FUnordLessThan";
		case spv::OpUGreaterThan:                             return "UGreaterThan";
		case spv::OpSGreaterThan:                             return "SGreaterThan";
		case spv::OpFOrdGreaterThan:                          return "FOrdGreaterThan";
		case spv::OpFUnordGreaterThan:                        return "FUnordGreaterThan";
		case spv::OpULessThanEqual:                           return "ULessThanEqual";
		case spv::OpSLessThanEqual:                           return "SLessThanEqual";
		case spv::OpFOrdLessThanEqual:                        return "FOrdLessThanEqual";
		case spv::OpFUnordLessThanEqual:                      return "FUnordLessThanEqual";
		case spv::OpUGreaterThanEqual:                        return "UGreaterThanEqual";
		case spv::OpSGreaterThanEqual:                        return "SGreaterThanEqual";
		case spv::OpFOrdGreaterThanEqual:                     return "FOrdGreaterThanEqual";
		case spv::OpFUnordGreaterThanEqual:                   return "FUnordGreaterThanEqual";
		case spv::OpDPdx:                                     return "DPdx";
		case spv::OpDPdy:                                     return "DPdy";
		case spv::OpFwidth:                                   return "Fwidth";
		case spv::OpDPdxFine:                                 return "DPdxFine";
		case spv::OpDPdyFine:                                 return "DPdyFine";
		case spv::OpFwidthFine:                               return "FwidthFine";
		case spv::OpDPdxCoarse:                               return "DPdxCoarse";
		case spv::OpDPdyCoarse:                               return "DPdyCoarse";
		case spv::OpFwidthCoarse:                             return "FwidthCoarse";
		case spv::OpEmitVertex:                               return "EmitVertex";
		case spv::OpEndPrimitive:                             return "EndPrimitive";
		case spv::OpEmitStreamVertex:                         return "EmitStreamVertex";
		case spv::OpEndStreamPrimitive:                       return "EndStreamPrimitive";
		case spv::OpControlBarrier:                           return "ControlBarrier";
		case spv::OpMemoryBarrier:                            return "MemoryBarrier";
		case spv::OpImagePointer:                             return "ImagePointer";
		case spv::OpAtomicInit:                               return "AtomicInit";
		case spv::OpAtomicLoad:                               return "AtomicLoad";
		case spv::OpAtomicStore:                              return "AtomicStore";
		case spv::OpAtomicExchange:                           return "AtomicExchange";
		case spv::OpAtomicCompareExchange:                    return "AtomicCompareExchange";
		case spv::OpAtomicCompareExchangeWeak:                return "AtomicCompareExchangeWeak";
		case spv::OpAtomicIIncrement:                         return "AtomicIIncrement";
		case spv::OpAtomicIDecrement:                         return "AtomicIDecrement";
		case spv::OpAtomicIAdd:                               return "AtomicIAdd";
		case spv::OpAtomicISub:                               return "AtomicISub";
		case spv::OpAtomicUMin:                               return "AtomicUMin";
		case spv::OpAtomicUMax:                               return "AtomicUMax";
		case spv::OpAtomicAnd:                                return "AtomicAnd";
		case spv::OpAtomicOr:                                 return "AtomicOr";
		case spv::OpAtomicXor:                                return "AtomicXor";
		case spv::OpLoopMerge:                                return "LoopMerge";
		case spv::OpSelectionMerge:                           return "SelectionMerge";
		case spv::OpLabel:                                    return "Label";
		case spv::OpBranch:                                   return "Branch";
		case spv::OpBranchConditional:                        return "BranchConditional";
		case spv::OpSwitch:                                   return "Switch";
		case spv::OpKill:                                     return "Kill";
		case spv::OpReturn:                                   return "Return";
		case spv::OpReturnValue:                              return "ReturnValue";
		case spv::OpUnreachable:                              return "Unreachable";
		case spv::OpLifetimeStart:                            return "LifetimeStart";
		case spv::OpLifetimeStop:                             return "LifetimeStop";
		case spv::OpCompileFlag:                              return "CompileFlag";
		case spv::OpAsyncGroupCopy:                           return "AsyncGroupCopy";
		case spv::OpWaitGroupEvents:                          return "WaitGroupEvents";
		case spv::OpGroupAll:                                 return "GroupAll";
		case spv::OpGroupAny:                                 return "GroupAny";
		case spv::OpGroupBroadcast:                           return "GroupBroadcast";
		case spv::OpGroupIAdd:                                return "GroupIAdd";
		case spv::OpGroupFAdd:                                return "GroupFAdd";
		case spv::OpGroupFMin:                                return "GroupFMin";
		case spv::OpGroupUMin:                                return "GroupUMin";
		case spv::OpGroupSMin:                                return "GroupSMin";
		case spv::OpGroupFMax:                                return "GroupFMax";
		case spv::OpGroupUMax:                                return "GroupUMax";
		case spv::OpGroupSMax:                                return "GroupSMax";
		case spv::OpGenericCastToPtrExplicit:                 return "GenericCastToPtrExplicit";
		case spv::OpGenericPtrMemSemantics:                   return "GenericPtrMemSemantics";
		case spv::OpReadPipe:                                 return "ReadPipe";
		case spv::OpWritePipe:                                return "WritePipe";
		case spv::OpReservedReadPipe:                         return "ReservedReadPipe";
		case spv::OpReservedWritePipe:                        return "ReservedWritePipe";
		case spv::OpReserveReadPipePackets:                   return "ReserveReadPipePackets";
		case spv::OpReserveWritePipePackets:                  return "ReserveWritePipePackets";
		case spv::OpCommitReadPipe:                           return "CommitReadPipe";
		case spv::OpCommitWritePipe:                          return "CommitWritePipe";
		case spv::OpIsValidReserveId:                         return "IsValidReserveId";
		case spv::OpGetNumPipePackets:                        return "GetNumPipePackets";
		case spv::OpGetMaxPipePackets:                        return "GetMaxPipePackets";
		case spv::OpGroupReserveReadPipePackets:              return "GroupReserveReadPipePackets";
		case spv::OpGroupReserveWritePipePackets:             return "GroupReserveWritePipePackets";
		case spv::OpGroupCommitReadPipe:                      return "GroupCommitReadPipe";
		case spv::OpGroupCommitWritePipe:                     return "GroupCommitWritePipe";
		case spv::OpEnqueueMarker:                            return "EnqueueMarker";
		case spv::OpEnqueueKernel:                            return "EnqueueKernel";
		case spv::OpGetKernelNDrangeSubGroupCount:            return "GetKernelNDrangeSubGroupCount";
		case spv::OpGetKernelNDrangeMaxSubGroupSize:          return "GetKernelNDrangeMaxSubGroupSize";
		case spv::OpGetKernelWorkGroupSize:                   return "GetKernelWorkGroupSize";
		case spv::OpGetKernelPreferredWorkGroupSizeMultiple:  return "GetKernelPreferredWorkGroupSizeMultiple";
		case spv::OpRetainEvent:                              return "RetainEvent";
		case spv::OpReleaseEvent:                             return "ReleaseEvent";
		case spv::OpCreateUserEvent:                          return "CreateUserEvent";
		case spv::OpIsValidEvent:                             return "IsValidEvent";
		case spv::OpSetUserEventStatus:                       return "SetUserEventStatus";
		case spv::OpCaptureEventProfilingInfo:                return "CaptureEventProfilingInfo";
		case spv::OpGetDefaultQueue:                          return "GetDefaultQueue";
		case spv::OpBuildNDRange:                             return "BuildNDRange";
		case spv::OpSatConvertSToU:                           return "SatConvertSToU";
		case spv::OpSatConvertUToS:                           return "SatConvertUToS";
		case spv::OpAtomicIMin:                               return "AtomicIMin";
		case spv::OpAtomicIMax:                               return "AtomicIMax";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::SourceLanguage>::Get(const spv::SourceLanguage &el)
{
	switch(el)
	{
		case spv::SourceLanguageUnknown: return "Unknown";
		case spv::SourceLanguageESSL:    return "ESSL";
		case spv::SourceLanguageGLSL:    return "GLSL";
		case spv::SourceLanguageOpenCL:  return "OpenCL";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::AddressingModel>::Get(const spv::AddressingModel &el)
{
	switch(el)
	{
		case spv::AddressingModelLogical:    return "Logical";
		case spv::AddressingModelPhysical32: return "Physical (32-bit)";
		case spv::AddressingModelPhysical64: return "Physical (64-bit)";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::MemoryModel>::Get(const spv::MemoryModel &el)
{
	switch(el)
	{
		case spv::MemoryModelSimple:   return "Simple";
		case spv::MemoryModelGLSL450:  return "GLSL450";
		case spv::MemoryModelOpenCL12: return "OpenCL12";
		case spv::MemoryModelOpenCL20: return "OpenCL20";
		case spv::MemoryModelOpenCL21: return "OpenCL21";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::ExecutionModel>::Get(const spv::ExecutionModel &el)
{
	switch(el)
	{
		case spv::ExecutionModelVertex:    return "Vertex Shader";
		case spv::ExecutionModelTessellationControl: return "Tess. Control Shader";
		case spv::ExecutionModelTessellationEvaluation: return "Tess. Eval Shader";
		case spv::ExecutionModelGeometry:  return "Geometry Shader";
		case spv::ExecutionModelFragment:  return "Fragment Shader";
		case spv::ExecutionModelGLCompute: return "Compute Shader";
		case spv::ExecutionModelKernel:    return "Kernel";
		default: break;
	}
	
	return "Unrecognised";
}
