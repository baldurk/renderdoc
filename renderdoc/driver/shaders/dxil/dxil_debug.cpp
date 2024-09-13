/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#pragma once

#include "dxil_debug.h"
#include "maths/formatpacking.h"

using namespace DXIL;
using namespace DXDebug;

const uint32_t DXIL_INVALID_ID = ~0U;

DXILDebug::Id GetSSAId(DXIL::Value *value)
{
  if(const Instruction *inst = cast<Instruction>(value))
    return inst->slot;

  RDCERR("Unhandled DXIL::Value type");
  return DXIL_INVALID_ID;
}

bool OperationFlushing(const Operation op, DXOp dxOpCode)
{
  if(dxOpCode != DXOp::NumOpCodes)
  {
    RDCASSERTEQUAL(op, Operation::Call);

    switch(dxOpCode)
    {
      // sample operations flush denorms
      case DXOp::Sample:
      case DXOp::SampleBias:
      case DXOp::SampleLevel:
      case DXOp::SampleGrad:
      case DXOp::SampleCmp:
      case DXOp::SampleCmpBias:
      case DXOp::SampleCmpLevel:
      case DXOp::SampleCmpGrad:
      case DXOp::SampleCmpLevelZero:
      case DXOp::TextureGather:
      case DXOp::TextureGatherCmp:
      case DXOp::TextureGatherRaw: return true;

      // unclear if these flush and it's unlikely denorms will come up, conservatively flush
      case DXOp::CalculateLOD:
      case DXOp::DerivCoarseX:
      case DXOp::DerivCoarseY:
      case DXOp::DerivFineX:
      case DXOp::DerivFineY:
      case DXOp::EvalSampleIndex: return true;

      // Float mathematical operations all flush denorms
      case DXOp::FAbs:
      case DXOp::Cos:
      case DXOp::Sin:
      case DXOp::Tan:
      case DXOp::Acos:
      case DXOp::Asin:
      case DXOp::Atan:
      case DXOp::Hcos:
      case DXOp::Hsin:
      case DXOp::Htan:
      case DXOp::Exp:
      case DXOp::Frc:
      case DXOp::Log:
      case DXOp::Sqrt:
      case DXOp::Rsqrt:
      case DXOp::Round_ne:
      case DXOp::Round_ni:
      case DXOp::Round_pi:
      case DXOp::Round_z:
      case DXOp::FMax:
      case DXOp::FMin:
      case DXOp::FMad:
      case DXOp::Fma:
      case DXOp::Dot2:
      case DXOp::Dot3:
      case DXOp::Dot4: return true;

      // Not floating point operations, no need to flush
      case DXOp::TempRegLoad:
      case DXOp::TempRegStore:
      case DXOp::MinPrecXRegLoad:
      case DXOp::MinPrecXRegStore:
      case DXOp::LoadInput:
      case DXOp::StoreOutput:
      case DXOp::Saturate:
      case DXOp::IsNaN:
      case DXOp::IsInf:
      case DXOp::IsFinite:
      case DXOp::IsNormal:
      case DXOp::Bfrev:
      case DXOp::Countbits:
      case DXOp::FirstbitLo:
      case DXOp::FirstbitHi:
      case DXOp::FirstbitSHi:
      case DXOp::IMax:
      case DXOp::IMin:
      case DXOp::UMax:
      case DXOp::UMin:
      case DXOp::IMul:
      case DXOp::UMul:
      case DXOp::UDiv:
      case DXOp::UAddc:
      case DXOp::USubb:
      case DXOp::IMad:
      case DXOp::UMad:
      case DXOp::Msad:
      case DXOp::Ibfe:
      case DXOp::Ubfe:
      case DXOp::Bfi:
      case DXOp::CreateHandle:
      case DXOp::CBufferLoad:
      case DXOp::CBufferLoadLegacy:
      case DXOp::TextureLoad:
      case DXOp::TextureStore:
      case DXOp::BufferLoad:
      case DXOp::BufferStore:
      case DXOp::BufferUpdateCounter:
      case DXOp::CheckAccessFullyMapped:
      case DXOp::GetDimensions:
      case DXOp::Texture2DMSGetSamplePosition:
      case DXOp::RenderTargetGetSamplePosition:
      case DXOp::RenderTargetGetSampleCount:
      case DXOp::AtomicBinOp:
      case DXOp::AtomicCompareExchange:
      case DXOp::Barrier:
      case DXOp::Discard:
      case DXOp::EvalSnapped:
      case DXOp::EvalCentroid:
      case DXOp::SampleIndex:
      case DXOp::Coverage:
      case DXOp::InnerCoverage:
      case DXOp::ThreadId:
      case DXOp::GroupId:
      case DXOp::ThreadIdInGroup:
      case DXOp::FlattenedThreadIdInGroup:
      case DXOp::EmitStream:
      case DXOp::CutStream:
      case DXOp::EmitThenCutStream:
      case DXOp::GSInstanceID:
      case DXOp::MakeDouble:
      case DXOp::SplitDouble:
      case DXOp::LoadOutputControlPoint:
      case DXOp::LoadPatchConstant:
      case DXOp::DomainLocation:
      case DXOp::StorePatchConstant:
      case DXOp::OutputControlPointID:
      case DXOp::PrimitiveID:
      case DXOp::CycleCounterLegacy:
      case DXOp::WaveIsFirstLane:
      case DXOp::WaveGetLaneIndex:
      case DXOp::WaveGetLaneCount:
      case DXOp::WaveAnyTrue:
      case DXOp::WaveAllTrue:
      case DXOp::WaveActiveAllEqual:
      case DXOp::WaveActiveBallot:
      case DXOp::WaveReadLaneAt:
      case DXOp::WaveReadLaneFirst:
      case DXOp::WaveActiveOp:
      case DXOp::WaveActiveBit:
      case DXOp::WavePrefixOp:
      case DXOp::QuadReadLaneAt:
      case DXOp::QuadOp:
      case DXOp::BitcastI16toF16:
      case DXOp::BitcastF16toI16:
      case DXOp::BitcastI32toF32:
      case DXOp::BitcastF32toI32:
      case DXOp::BitcastI64toF64:
      case DXOp::BitcastF64toI64:
      case DXOp::LegacyF32ToF16:
      case DXOp::LegacyF16ToF32:
      case DXOp::LegacyDoubleToFloat:
      case DXOp::LegacyDoubleToSInt32:
      case DXOp::LegacyDoubleToUInt32:
      case DXOp::WaveAllBitCount:
      case DXOp::WavePrefixBitCount:
      case DXOp::AttributeAtVertex:
      case DXOp::ViewID:
      case DXOp::RawBufferLoad:
      case DXOp::RawBufferStore:
      case DXOp::InstanceID:
      case DXOp::InstanceIndex:
      case DXOp::HitKind:
      case DXOp::RayFlags:
      case DXOp::DispatchRaysIndex:
      case DXOp::DispatchRaysDimensions:
      case DXOp::WorldRayOrigin:
      case DXOp::WorldRayDirection:
      case DXOp::ObjectRayOrigin:
      case DXOp::ObjectRayDirection:
      case DXOp::ObjectToWorld:
      case DXOp::WorldToObject:
      case DXOp::RayTMin:
      case DXOp::RayTCurrent:
      case DXOp::IgnoreHit:
      case DXOp::AcceptHitAndEndSearch:
      case DXOp::TraceRay:
      case DXOp::ReportHit:
      case DXOp::CallShader:
      case DXOp::CreateHandleForLib:
      case DXOp::PrimitiveIndex:
      case DXOp::Dot2AddHalf:
      case DXOp::Dot4AddI8Packed:
      case DXOp::Dot4AddU8Packed:
      case DXOp::WaveMatch:
      case DXOp::WaveMultiPrefixOp:
      case DXOp::WaveMultiPrefixBitCount:
      case DXOp::SetMeshOutputCounts:
      case DXOp::EmitIndices:
      case DXOp::GetMeshPayload:
      case DXOp::StoreVertexOutput:
      case DXOp::StorePrimitiveOutput:
      case DXOp::DispatchMesh:
      case DXOp::WriteSamplerFeedback:
      case DXOp::WriteSamplerFeedbackBias:
      case DXOp::WriteSamplerFeedbackLevel:
      case DXOp::WriteSamplerFeedbackGrad:
      case DXOp::AllocateRayQuery:
      case DXOp::RayQuery_TraceRayInline:
      case DXOp::RayQuery_Proceed:
      case DXOp::RayQuery_Abort:
      case DXOp::RayQuery_CommitNonOpaqueTriangleHit:
      case DXOp::RayQuery_CommitProceduralPrimitiveHit:
      case DXOp::RayQuery_CommittedStatus:
      case DXOp::RayQuery_CandidateType:
      case DXOp::RayQuery_CandidateObjectToWorld3x4:
      case DXOp::RayQuery_CandidateWorldToObject3x4:
      case DXOp::RayQuery_CommittedObjectToWorld3x4:
      case DXOp::RayQuery_CommittedWorldToObject3x4:
      case DXOp::RayQuery_CandidateProceduralPrimitiveNonOpaque:
      case DXOp::RayQuery_CandidateTriangleFrontFace:
      case DXOp::RayQuery_CommittedTriangleFrontFace:
      case DXOp::RayQuery_CandidateTriangleBarycentrics:
      case DXOp::RayQuery_CommittedTriangleBarycentrics:
      case DXOp::RayQuery_RayFlags:
      case DXOp::RayQuery_WorldRayOrigin:
      case DXOp::RayQuery_WorldRayDirection:
      case DXOp::RayQuery_RayTMin:
      case DXOp::RayQuery_CandidateTriangleRayT:
      case DXOp::RayQuery_CommittedRayT:
      case DXOp::RayQuery_CandidateInstanceIndex:
      case DXOp::RayQuery_CandidateInstanceID:
      case DXOp::RayQuery_CandidateGeometryIndex:
      case DXOp::RayQuery_CandidatePrimitiveIndex:
      case DXOp::RayQuery_CandidateObjectRayOrigin:
      case DXOp::RayQuery_CandidateObjectRayDirection:
      case DXOp::RayQuery_CommittedInstanceIndex:
      case DXOp::RayQuery_CommittedInstanceID:
      case DXOp::RayQuery_CommittedGeometryIndex:
      case DXOp::RayQuery_CommittedPrimitiveIndex:
      case DXOp::RayQuery_CommittedObjectRayOrigin:
      case DXOp::RayQuery_CommittedObjectRayDirection:
      case DXOp::GeometryIndex:
      case DXOp::RayQuery_CandidateInstanceContributionToHitGroupIndex:
      case DXOp::RayQuery_CommittedInstanceContributionToHitGroupIndex:
      case DXOp::AnnotateHandle:
      case DXOp::CreateHandleFromBinding:
      case DXOp::CreateHandleFromHeap:
      case DXOp::Unpack4x8:
      case DXOp::Pack4x8:
      case DXOp::IsHelperLane:
      case DXOp::QuadVote:
      case DXOp::TextureStoreSample:
      case DXOp::WaveMatrix_Annotate:
      case DXOp::WaveMatrix_Depth:
      case DXOp::WaveMatrix_Fill:
      case DXOp::WaveMatrix_LoadRawBuf:
      case DXOp::WaveMatrix_LoadGroupShared:
      case DXOp::WaveMatrix_StoreRawBuf:
      case DXOp::WaveMatrix_StoreGroupShared:
      case DXOp::WaveMatrix_Multiply:
      case DXOp::WaveMatrix_MultiplyAccumulate:
      case DXOp::WaveMatrix_ScalarOp:
      case DXOp::WaveMatrix_SumAccumulate:
      case DXOp::WaveMatrix_Add:
      case DXOp::AllocateNodeOutputRecords:
      case DXOp::GetNodeRecordPtr:
      case DXOp::IncrementOutputCount:
      case DXOp::OutputComplete:
      case DXOp::GetInputRecordCount:
      case DXOp::FinishedCrossGroupSharing:
      case DXOp::BarrierByMemoryType:
      case DXOp::BarrierByMemoryHandle:
      case DXOp::BarrierByNodeRecordHandle:
      case DXOp::CreateNodeOutputHandle:
      case DXOp::IndexNodeHandle:
      case DXOp::AnnotateNodeHandle:
      case DXOp::CreateNodeInputRecordHandle:
      case DXOp::AnnotateNodeRecordHandle:
      case DXOp::NodeOutputIsValid:
      case DXOp::GetRemainingRecursionLevels:
      case DXOp::StartVertexLocation:
      case DXOp::StartInstanceLocation: return false;
      case DXOp::NumOpCodes:
        RDCERR("Unhandled DXOpCode %s in DXIL shader debugger", ToStr(dxOpCode).c_str());
        break;
    }
  }

  switch(op)
  {
    // Float mathematical operations all flush denorms including comparisons
    case Operation::FAdd:
    case Operation::FSub:
    case Operation::FMul:
    case Operation::FDiv:
    case Operation::FRem:
    case Operation::FPTrunc:
    case Operation::FPExt:
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
    case Operation::FOrdTrue: return true;

    // Casts do not flush
    case Operation::Trunc:
    case Operation::SExt:
    case Operation::ZExt:
    case Operation::PtrToI:
    case Operation::IToPtr:
    case Operation::Bitcast:
    case Operation::AddrSpaceCast: return false;

    // Integer operations do not flush
    case Operation::IEqual:
    case Operation::INotEqual:
    case Operation::UGreater:
    case Operation::UGreaterEqual:
    case Operation::ULess:
    case Operation::ULessEqual:
    case Operation::SGreater:
    case Operation::SGreaterEqual:
    case Operation::SLess:
    case Operation::SLessEqual: return false;

    // Can't generate denorms or denorm inputs are implicitly rounded to 0, no need to flush
    case Operation::FToU:
    case Operation::FToS:
    case Operation::UToF:
    case Operation::SToF: return false;

    // Non arithmetic operations do not flush
    case Operation::NoOp:
    case Operation::Call:
    case Operation::ExtractVal:
    case Operation::Ret:
    case Operation::Unreachable:
    case Operation::Alloca:
    case Operation::GetElementPtr:
    case Operation::Branch:
    case Operation::Fence:
    case Operation::Switch:
    case Operation::Load:
    case Operation::Store:
    case Operation::Select:
    case Operation::ExtractElement:
    case Operation::InsertElement:
    case Operation::ShuffleVector:
    case Operation::InsertValue:
    case Operation::Phi:
    case Operation::CompareExchange: return false;

    // Integer operations do not flush
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
    case Operation::LoadAtomic:
    case Operation::StoreAtomic:
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
    case Operation::AtomicUMin: return false;
    default: RDCERR("Unhandled LLVM OpCode %s in DXIL shader debugger", ToStr(op).c_str()); break;
  }

  return false;
}

ShaderEvents AssignValue(ShaderVariable &result, const ShaderVariable &src, bool flushDenorm)
{
  RDCASSERTEQUAL(result.type, src.type);

  ShaderEvents flags = ShaderEvents::NoEvent;

  if(result.type == VarType::Float)
  {
    float ft = src.value.f32v[0];
    if(!RDCISFINITE(ft))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }
  else if(result.type == VarType::Double)
  {
    double dt = src.value.f64v[0];
    if(!RDCISFINITE(dt))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }

  result.value.u32v[0] = src.value.u32v[0];

  if(flushDenorm)
  {
    if(result.type == VarType::Float)
      result.value.f32v[0] = flush_denorm(src.value.f32v[0]);
    else if(result.type == VarType::Double)
      RDCERR("Unhandled flushing denormalised double");
  }

  return flags;
}

uint8_t GetElementByteSize(VarType type)
{
  switch(type)
  {
    case VarType::SLong:
    case VarType::ULong:
    case VarType::Double: return 8; break;
    case VarType::SInt:
    case VarType::UInt:
    case VarType::Float: return 4; break;
    case VarType::SShort:
    case VarType::UShort:
    case VarType::Half: return 2; break;
    case VarType::SByte:
    case VarType::UByte: return 1; break;
    case VarType::Bool:
    case VarType::Enum:
    case VarType::Struct:
    case VarType::GPUPointer:
    case VarType::ConstantBlock:
    case VarType::ReadOnlyResource:
    case VarType::ReadWriteResource:
    case VarType::Sampler:
    case VarType::Unknown: RDCERR("Unhandled VarType %s", ToStr(type).c_str()); break;
  };
  return 0;
}

DXBC::ResourceRetType ConvertComponentTypeToResourceRetType(const ComponentType compType)
{
  switch(compType)
  {
    case ComponentType::I32: return DXBC::ResourceRetType::RETURN_TYPE_SINT;
    case ComponentType::U32: return DXBC::ResourceRetType::RETURN_TYPE_UINT;
    case ComponentType::F32: return DXBC::ResourceRetType::RETURN_TYPE_FLOAT;
    case ComponentType::F64: return DXBC::ResourceRetType::RETURN_TYPE_DOUBLE;
    case ComponentType::SNormF32: return DXBC ::ResourceRetType::RETURN_TYPE_SNORM;
    case ComponentType::UNormF32: return DXBC::ResourceRetType::RETURN_TYPE_UNORM;
    case ComponentType::I1:
    case ComponentType::I16:
    case ComponentType::U16:
    case ComponentType::F16:
    case ComponentType::SNormF16:
    case ComponentType::UNormF16:
    case ComponentType::I64:
    case ComponentType::U64:
    case ComponentType::SNormF64:
    case ComponentType::UNormF64:
    case ComponentType::Invalid: return DXBC::ResourceRetType::RETURN_TYPE_UNKNOWN;
  };
  return DXBC::ResourceRetType::RETURN_TYPE_UNKNOWN;
}

DXBCBytecode::ResourceDimension ConvertResourceKindToResourceDimension(const ResourceKind kind)
{
  switch(kind)
  {
    case ResourceKind::Texture1D:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE1D;
    case ResourceKind::Texture1DArray:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE1DARRAY;
    case ResourceKind::Texture2D:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2D;
    case ResourceKind::Texture2DArray:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DARRAY;
    case ResourceKind::Texture2DMS:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DMS;
    case ResourceKind::Texture2DMSArray:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DMSARRAY;
    case ResourceKind::Texture3D:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE3D;
    case ResourceKind::TextureCube:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURECUBE;
    case ResourceKind::TextureCubeArray:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURECUBEARRAY;
    case ResourceKind::TypedBuffer:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_BUFFER;
    case ResourceKind::RawBuffer:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_RAW_BUFFER;
    case ResourceKind::StructuredBuffer:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_STRUCTURED_BUFFER;
    case ResourceKind::Unknown:
    case ResourceKind::CBuffer:
    case ResourceKind::Sampler:
    case ResourceKind::TBuffer:
    case ResourceKind::RTAccelerationStructure:
    case ResourceKind::FeedbackTexture2D:
    case ResourceKind::FeedbackTexture2DArray:
    case ResourceKind::StructuredBufferWithCounter:
    case ResourceKind::SamplerComparison:
      return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_UNKNOWN;
  }
  return DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_UNKNOWN;
}

DXBCBytecode::SamplerMode ConvertSamplerKindToSamplerMode(const SamplerKind kind)
{
  switch(kind)
  {
    case SamplerKind::Comparison: return DXBCBytecode::SAMPLER_MODE_COMPARISON;
    case SamplerKind::Mono: return DXBCBytecode::SAMPLER_MODE_MONO;
    case SamplerKind::Default: return DXBCBytecode::SAMPLER_MODE_DEFAULT;
    case SamplerKind::Invalid: return DXBCBytecode::NUM_SAMPLERS;
  }
  return DXBCBytecode::SamplerMode::NUM_SAMPLERS;
}

static VarType ConvertDXILTypeToVarType(const Type *type)
{
  if(type->type == Type::TypeKind::Struct)
    return VarType::Struct;
  if(type->type == Type::TypeKind::Vector)
    return ConvertDXILTypeToVarType(type->inner);
  if(type->type == Type::TypeKind::Array)
    return ConvertDXILTypeToVarType(type->inner);
  if(type->type == Type::TypeKind::Pointer)
    return VarType::GPUPointer;

  RDCASSERTEQUAL(type->type, Type::TypeKind::Scalar);
  if(type->scalarType == Type::ScalarKind::Int)
  {
    if(type->bitWidth == 64)
      return VarType::SLong;
    else if(type->bitWidth == 32)
      return VarType::SInt;
    else if(type->bitWidth == 16)
      return VarType::SShort;
    else if(type->bitWidth == 8)
      return VarType::SByte;
    else if(type->bitWidth == 1)
      return VarType::Bool;
  }
  else if(type->scalarType == Type::ScalarKind::Float)
  {
    if(type->bitWidth == 64)
      return VarType::Double;
    else if(type->bitWidth == 32)
      return VarType::Float;
    else if(type->bitWidth == 16)
      return VarType::Half;
  }
  return VarType::Unknown;
}

void TypedUAVStore(DXILDebug::GlobalState::ViewFmt &fmt, byte *d, const ShaderValue &value)
{
  if(fmt.byteWidth == 10)
  {
    uint32_t u = 0;

    if(fmt.fmt == CompType::UInt)
    {
      u |= (value.u32v[0] & 0x3ff) << 0;
      u |= (value.u32v[1] & 0x3ff) << 10;
      u |= (value.u32v[2] & 0x3ff) << 20;
      u |= (value.u32v[3] & 0x3) << 30;
    }
    else if(fmt.fmt == CompType::UNorm)
    {
      u = ConvertToR10G10B10A2(Vec4f(value.f32v[0], value.f32v[1], value.f32v[2], value.f32v[3]));
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
    memcpy(d, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u = ConvertToR11G11B10(Vec3f(value.f32v[0], value.f32v[1], value.f32v[2]));
    memcpy(d, &u, sizeof(uint32_t));
  }
  else if(fmt.byteWidth == 4)
  {
    uint32_t *u = (uint32_t *)d;

    for(int c = 0; c < fmt.numComps; c++)
      u[c] = value.u32v[c];
  }
  else if(fmt.byteWidth == 2)
  {
    if(fmt.fmt == CompType::Float)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = ConvertToHalf(value.f32v[c]);
    }
    else if(fmt.fmt == CompType::UInt)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = value.u32v[c] & 0xffff;
    }
    else if(fmt.fmt == CompType::SInt)
    {
      int16_t *i = (int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int16_t)RDCCLAMP(value.s32v[c], (int32_t)INT16_MIN, (int32_t)INT16_MAX);
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      uint16_t *u = (uint16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], 0.0f, 1.0f) * float(0xffff) + 0.5f;
        u[c] = uint16_t(f);
      }
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      int16_t *i = (int16_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], -1.0f, 1.0f) * 0x7fff;

        if(f < 0.0f)
          i[c] = int16_t(f - 0.5f);
        else
          i[c] = int16_t(f + 0.5f);
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
  else if(fmt.byteWidth == 1)
  {
    if(fmt.fmt == CompType::UInt)
    {
      uint8_t *u = (uint8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        u[c] = value.u32v[c] & 0xff;
    }
    else if(fmt.fmt == CompType::SInt)
    {
      int8_t *i = (int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        i[c] = (int8_t)RDCCLAMP(value.s32v[c], (int32_t)INT8_MIN, (int32_t)INT8_MAX);
    }
    else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
    {
      uint8_t *u = (uint8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], 0.0f, 1.0f) * float(0xff) + 0.5f;
        u[c] = uint8_t(f);
      }
    }
    else if(fmt.fmt == CompType::SNorm)
    {
      int8_t *i = (int8_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
      {
        float f = RDCCLAMP(value.f32v[c], -1.0f, 1.0f) * 0x7f;

        if(f < 0.0f)
          i[c] = int8_t(f - 0.5f);
        else
          i[c] = int8_t(f + 0.5f);
      }
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
}

ShaderValue TypedUAVLoad(DXILDebug::GlobalState::ViewFmt &fmt, const byte *d)
{
  ShaderValue result;
  result.f32v[0] = 0.0f;
  result.f32v[1] = 0.0f;
  result.f32v[2] = 0.0f;
  result.f32v[3] = 0.0f;

  if(fmt.byteWidth == 10)
  {
    uint32_t u;
    memcpy(&u, d, sizeof(uint32_t));

    if(fmt.fmt == CompType::UInt)
    {
      result.u32v[0] = (u >> 0) & 0x3ff;
      result.u32v[1] = (u >> 10) & 0x3ff;
      result.u32v[2] = (u >> 20) & 0x3ff;
      result.u32v[3] = (u >> 30) & 0x003;
    }
    else if(fmt.fmt == CompType::UNorm)
    {
      Vec4f res = ConvertFromR10G10B10A2(u);
      result.f32v[0] = res.x;
      result.f32v[1] = res.y;
      result.f32v[2] = res.z;
      result.f32v[3] = res.w;
    }
    else
    {
      RDCERR("Unexpected format type on buffer resource");
    }
  }
  else if(fmt.byteWidth == 11)
  {
    uint32_t u;
    memcpy(&u, d, sizeof(uint32_t));

    Vec3f res = ConvertFromR11G11B10(u);
    result.f32v[0] = res.x;
    result.f32v[1] = res.y;
    result.f32v[2] = res.z;
    result.f32v[3] = 1.0f;
  }
  else
  {
    if(fmt.byteWidth == 4)
    {
      const uint32_t *u = (const uint32_t *)d;

      for(int c = 0; c < fmt.numComps; c++)
        result.u32v[c] = u[c];
    }
    else if(fmt.byteWidth == 2)
    {
      if(fmt.fmt == CompType::Float)
      {
        const uint16_t *u = (const uint16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.f32v[c] = ConvertFromHalf(u[c]);
      }
      else if(fmt.fmt == CompType::UInt)
      {
        const uint16_t *u = (const uint16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.u32v[c] = u[c];
      }
      else if(fmt.fmt == CompType::SInt)
      {
        const int16_t *in = (const int16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.s32v[c] = in[c];
      }
      else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
      {
        const uint16_t *u = (const uint16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.f32v[c] = float(u[c]) / float(0xffff);
      }
      else if(fmt.fmt == CompType::SNorm)
      {
        const int16_t *in = (const int16_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
        {
          // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
          if(in[c] == -32768)
            result.f32v[c] = -1.0f;
          else
            result.f32v[c] = float(in[c]) / 32767.0f;
        }
      }
      else
      {
        RDCERR("Unexpected format type on buffer resource");
      }
    }
    else if(fmt.byteWidth == 1)
    {
      if(fmt.fmt == CompType::UInt)
      {
        const uint8_t *u = (const uint8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.u32v[c] = u[c];
      }
      else if(fmt.fmt == CompType::SInt)
      {
        const int8_t *in = (const int8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.s32v[c] = in[c];
      }
      else if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB)
      {
        const uint8_t *u = (const uint8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
          result.f32v[c] = float(u[c]) / float(0xff);
      }
      else if(fmt.fmt == CompType::SNorm)
      {
        const int8_t *in = (const int8_t *)d;

        for(int c = 0; c < fmt.numComps; c++)
        {
          // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
          if(in[c] == -128)
            result.f32v[c] = -1.0f;
          else
            result.f32v[c] = float(in[c]) / 127.0f;
        }
      }
      else
      {
        RDCERR("Unexpected format type on buffer resource");
      }
    }

    // fill in alpha with 1.0 or 1 as appropriate
    if(fmt.numComps < 4)
    {
      if(fmt.fmt == CompType::UNorm || fmt.fmt == CompType::UNormSRGB ||
         fmt.fmt == CompType::SNorm || fmt.fmt == CompType::Float)
        result.f32v[3] = 1.0f;
      else
        result.u32v[3] = 1;
    }
  }

  return result;
}

void FillViewFmt(VarType type, DXILDebug::GlobalState::ViewFmt &fmt)
{
  switch(type)
  {
    case VarType::Float:
      fmt.byteWidth = 4;
      fmt.fmt = CompType::Float;
      break;
    case VarType::Double:
      fmt.byteWidth = 8;
      fmt.fmt = CompType::Float;
      break;
    case VarType::Half:
      fmt.byteWidth = 2;
      fmt.fmt = CompType::Float;
      break;
    case VarType::SInt:
      fmt.byteWidth = 4;
      fmt.fmt = CompType::SInt;
      break;
    case VarType::UInt:
      fmt.byteWidth = 4;
      fmt.fmt = CompType::UInt;
      break;
    case VarType::SShort:
      fmt.byteWidth = 2;
      fmt.fmt = CompType::SInt;
      break;
    case VarType::UShort:
      fmt.byteWidth = 2;
      fmt.fmt = CompType::UInt;
      break;
    case VarType::SLong:
      fmt.byteWidth = 8;
      fmt.fmt = CompType::SInt;
      break;
    case VarType::ULong:
      fmt.byteWidth = 2;
      fmt.fmt = CompType::UInt;
      break;
    case VarType::SByte:
      fmt.byteWidth = 1;
      fmt.fmt = CompType::SInt;
      break;
    case VarType::UByte:
      fmt.byteWidth = 1;
      fmt.fmt = CompType::UInt;
      break;
    default: RDCERR("Unhandled Result Type %s", ToStr(type).c_str()); break;
  }
}

namespace DXILDebug
{

static void ApplyDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int input,
                             int numWords, float *data, float signmul, int32_t quadIdxA,
                             int32_t quadIdxB)
{
  for(int w = 0; w < numWords; w++)
  {
    quad[quadIdxA].m_Input.members[input].value.f32v[w] += signmul * data[w];
    if(quadIdxB >= 0)
      quad[quadIdxB].m_Input.members[input].value.f32v[w] += signmul * data[w];
  }

  // TODO: SAMPLE EVALUATE
#if 0
  // quick check to see if this register was evaluated
  if(global.sampleEvalRegisterMask & (1ULL << reg))
  {
    // apply derivative to any cached sample evaluations on these quad indices
    for(auto it = global.sampleEvalCache.begin(); it != global.sampleEvalCache.end(); ++it)
    {
      if((it->first.quadIndex == quadIdxA || it->first.quadIndex == quadIdxB) &&
         reg == it->first.inputRegisterIndex)
      {
        for(int w = 0; w < numWords; w++)
          it->second.value.f32v[w] += data[w];
      }
    }
  }
#endif
}

void ApplyAllDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int destIdx,
                         const rdcarray<PSInputData> &psInputs, float *data)
{
  // We make the assumption that the coarse derivatives are generated from (0,0) in the quad, and
  // fine derivatives are generated from the destination index and its neighbours in X and Y.
  // This isn't spec'd but we must assume something and this will hopefully get us closest to
  // reproducing actual results.
  //
  // For debugging, we need members of the quad to be able to generate coarse and fine
  // derivatives.
  //
  // For (0,0) we only need the coarse derivatives to get our neighbours (1,0) and (0,1) which
  // will give us coarse and fine derivatives being identical.
  //
  // For the others we will need to use a combination of coarse and fine derivatives to get the
  // diagonal element in the quad. In the examples below, remember that the quad indices are:
  //
  // +---+---+
  // | 0 | 1 |
  // +---+---+
  // | 2 | 3 |
  // +---+---+
  //
  // And that we have definitions of the derivatives:
  //
  // ddx_coarse = (1,0) - (0,0)
  // ddy_coarse = (0,1) - (0,0)
  //
  // i.e. the same for all members of the quad
  //
  // ddx_fine   = (x,y) - (1-x,y)
  // ddy_fine   = (x,y) - (x,1-y)
  //
  // i.e. the difference to the neighbour of our desired invocation (the one we have the actual
  // inputs for, from gathering above).
  //
  // So e.g. if our thread is at (1,1) destIdx = 3
  //
  // (1,0) = (1,1) - ddx_fine
  // (0,1) = (1,1) - ddy_fine
  // (0,0) = (1,1) - ddy_fine - ddx_coarse
  //
  // and ddy_coarse is unused. For (1,0) destIdx = 1:
  //
  // (1,1) = (1,0) + ddy_fine
  // (0,1) = (1,0) - ddx_coarse + ddy_coarse
  // (0,0) = (1,0) - ddx_coarse
  //
  // and ddx_fine is unused (it's identical to ddx_coarse anyway)

  // this is the value of input[1] - input[0]
  float *ddx_coarse = (float *)data;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;
    if(destIdx == 0)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, 1.0f, 1, 3);
    else if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, -1.0f, 0, 2);
    else if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, 1.0f, 1, -1);
    else if(destIdx == 3)
      ApplyDerivatives(global, quad, input, numWords, ddx_coarse, -1.0f, 0, -1);

    ddx_coarse += numWords;
  }

  // this is the value of input[2] - input[0]
  float *ddy_coarse = ddx_coarse;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;
    if(destIdx == 0)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, 1.0f, 2, 3);
    else if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, -1.0f, 2, -1);
    else if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, 1.0f, 0, 1);

    ddy_coarse += numWords;
  }

  float *ddxfine = ddy_coarse;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;

    if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddxfine, 1.0f, 3, -1);
    else if(destIdx == 3)
      ApplyDerivatives(global, quad, input, numWords, ddxfine, -1.0f, 2, -1);

    ddxfine += numWords;
  }

  float *ddyfine = ddxfine;

  for(const PSInputData &psInput : psInputs)
  {
    const int input = psInput.input;
    const int numWords = psInput.numwords;

    if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddyfine, 1.0f, 3, -1);
    else if(destIdx == 3)
      ApplyDerivatives(global, quad, input, numWords, ddyfine, -1.0f, 0, 1);

    ddyfine += numWords;
  }
}

void ThreadState::InitialiseHelper(const ThreadState &activeState)
{
  m_Input = activeState.m_Input;
  m_Semantics = activeState.m_Semantics;
}

// static helper function
rdcstr Debugger::GetResourceReferenceName(const DXIL::Program *program,
                                          DXIL::ResourceClass resClass, const BindingSlot &slot)
{
  RDCASSERT(program);
  for(const ResourceReference &resRef : program->m_ResourceReferences)
  {
    if(resRef.resourceBase.resClass != resClass)
      continue;
    if(resRef.resourceBase.space != slot.registerSpace)
      continue;
    if(resRef.resourceBase.regBase > slot.shaderRegister)
      continue;
    if(resRef.resourceBase.regBase + resRef.resourceBase.regCount < slot.shaderRegister)
      continue;

    return program->GetHandleAlias(resRef.handleID);
  }
  RDCERR("Failed to find DXIL %s Resource Space %d Register %d", ToStr(resClass).c_str(),
         slot.registerSpace, slot.shaderRegister);
  return "UNKNOWN_RESOURCE_HANDLE";
}

// member functions

ShaderDebugTrace *Debugger::BeginDebug(uint32_t eventId, const DXBC::DXBCContainer *dxbcContainer,
                                       const ShaderReflection &reflection, uint32_t activeLaneIndex)
{
  ShaderStage shaderStage = reflection.stage;

  m_DXBC = dxbcContainer;
  m_EventId = eventId;
  m_ActiveLaneIndex = activeLaneIndex;

  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->stage = shaderStage;

  return ret;
}

rdcarray<ShaderDebugState> Debugger::ContinueDebug(DebugAPIWrapper *apiWrapper)
{
  rdcarray<ShaderDebugState> ret;

  return ret;
}

};    // namespace DXILDebug
