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
#include "common/formatting.h"
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

static bool OperationFlushing(const Operation op, DXOp dxOpCode)
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

static ShaderEvents AssignValue(ShaderVariable &result, const ShaderVariable &src, bool flushDenorm)
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

static uint8_t GetElementByteSize(VarType type)
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

static DXBC::ResourceRetType ConvertComponentTypeToResourceRetType(const ComponentType compType)
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

static DXBCBytecode::ResourceDimension ConvertResourceKindToResourceDimension(const ResourceKind kind)
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

static DXBCBytecode::SamplerMode ConvertSamplerKindToSamplerMode(const SamplerKind kind)
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

static void TypedUAVStore(DXILDebug::GlobalState::ViewFmt &fmt, byte *d, const ShaderValue &value)
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

static ShaderValue TypedUAVLoad(DXILDebug::GlobalState::ViewFmt &fmt, const byte *d)
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

static void FillViewFmt(VarType type, DXILDebug::GlobalState::ViewFmt &fmt)
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

ThreadState::ThreadState(uint32_t workgroupIndex, Debugger &debugger, const GlobalState &globalState)
    : m_Debugger(debugger), m_GlobalState(globalState), m_Program(debugger.GetProgram())
{
  m_WorkgroupIndex = workgroupIndex;
  m_FunctionInfo = NULL;
  m_FunctionInstructionIdx = 0;
  m_GlobalInstructionIdx = 0;
  m_Killed = false;
  m_Ended = false;
  m_Callstack.clear();
  m_ShaderType = m_Program.GetShaderType();
  m_Semantics.coverage = ~0U;
  m_Semantics.isFrontFace = false;
  m_Semantics.primID = ~0U;
}

ThreadState::~ThreadState()
{
  for(auto it : m_StackAllocs)
    free(it.second.backingMemory);
}

void ThreadState::InitialiseHelper(const ThreadState &activeState)
{
  m_Input = activeState.m_Input;
  m_Semantics = activeState.m_Semantics;
  m_LiveVariables = activeState.m_LiveVariables;
}

bool ThreadState::Finished() const
{
  return m_Killed || m_Ended || m_Callstack.empty();
}

void ThreadState::ProcessScopeChange(const rdcarray<Id> &oldLive, const rdcarray<Id> &newLive)
{
  // nothing to do if we aren't tracking into a state
  if(!m_State)
    return;

  // all oldLive (except globals) are going out of scope. all newLive (except globals) are coming
  // into scope

  const rdcarray<Id> &liveGlobals = m_Debugger.GetLiveGlobals();

  for(const Id &id : oldLive)
  {
    if(liveGlobals.contains(id))
      continue;

    m_State->changes.push_back({m_LiveVariables[id]});
  }

  for(const Id &id : newLive)
  {
    if(liveGlobals.contains(id))
      continue;

    m_State->changes.push_back({ShaderVariable(), m_LiveVariables[id]});
  }
}

void ThreadState::EnterFunction(const Function *function, const rdcarray<Value *> &args)
{
  StackFrame *frame = new StackFrame(function);
  m_FunctionInstructionIdx = 0;
  m_FunctionInfo = m_Debugger.GetFunctionInfo(function);

  // if there's a previous stack frame, save its live list
  if(!m_Callstack.empty())
  {
    // process the outgoing scope
    ProcessScopeChange(m_Live, {});
    m_Callstack.back()->live = m_Live;
    m_Callstack.back()->dormant = m_Dormant;
  }

  // start with just globals
  m_Live = m_Debugger.GetLiveGlobals();
  m_Dormant.clear();
  m_Block = 0;
  m_PreviousBlock = ~0U;

  m_GlobalInstructionIdx = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
  m_Callstack.push_back(frame);

  ShaderDebugState *state = m_State;
  m_State = state;
}

void ThreadState::EnterEntryPoint(const Function *function, ShaderDebugState *state)
{
  m_State = state;

  EnterFunction(function, {});

  /*
    //TODO : add the globals to known variables
    for(const ShaderVariable &v : m_GlobalState.globals)
      m_LiveVariables[v.name] = v;
  */

  m_State = NULL;
}

bool ThreadState::ExecuteInstruction(DebugAPIWrapper *apiWrapper,
                                     const rdcarray<ThreadState> &workgroups)
{
  m_CurrentInstruction = m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
  const Instruction &inst = *m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
  m_FunctionInstructionIdx++;

  if(IsDXCNop(inst))
    return false;

  Operation opCode = inst.op;
  DXOp dxOpCode = DXOp::NumOpCodes;
  ShaderEvents eventFlags = ShaderEvents::NoEvent;
  // ResultId should always be the original SSA name
  Id resultId = inst.slot;
  const Type *retType = inst.type;
  // Sensible defaults
  ShaderVariable result;
  Program::MakeResultId(inst, result.name);
  result.rows = 1;
  result.columns = 1;
  result.type = ConvertDXILTypeToVarType(retType);
  result.value.u64v[0] = 0;
  result.value.u64v[1] = 0;
  result.value.u64v[2] = 0;
  result.value.u64v[3] = 0;

  bool recordChange = true;
  switch(opCode)
  {
      // TODO: increment the basic block
      // Operation::Switch
    case Operation::Call:
    {
      const Function *callFunc = inst.getFuncCall();
      if(callFunc->family == FunctionFamily::DXOp)
      {
        RDCASSERT(getival<DXOp>(inst.args[0], dxOpCode));
        RDCASSERT(dxOpCode < DXOp::NumOpCodes, dxOpCode, DXOp::NumOpCodes);
        switch(dxOpCode)
        {
          case DXOp::LoadInput:
          {
            // LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            uint32_t inputIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            // uint32_t rowIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            uint32_t colIdx = arg.value.u32v[0];
            // TODO: get the type of the result and copy the correct value(s)
            // TODO: rowIdx
            // TODO: matrices
            result.value.f32v[0] = m_Input.members[inputIdx].value.f32v[colIdx];
            break;
          }
          case DXOp::StoreOutput:
          {
            // StoreOutput(outputSigId,rowIndex,colIndex,value)
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            uint32_t outputIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            // uint32_t rowIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            uint32_t colIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[4], opCode, dxOpCode, arg));
            // TODO: get the type of the result and copy the correct value(s)
            // TODO: rowIdx
            // TODO: matrices
            // Only the active lane stores outputs
            if(m_State)
            {
              m_Output.members[outputIdx].value.f32v[colIdx] = arg.value.f32v[0];
              result = m_Output;
              resultId = m_OutputSSAId;
            }
            else
            {
              resultId = DXIL_INVALID_ID;
              result.name.clear();
            }
            break;
          }
          case DXOp::GetDimensions:
          {
            // GetDimensions(handle,mipLevel)
            rdcstr handleId = GetArgumentName(1);
            const ResourceReference *resRef = GetResource(handleId);
            if(!resRef)
              break;

            BindingSlot binding(resRef->resourceBase.regBase, resRef->resourceBase.space);
            ShaderVariable data;
            uint32_t mipLevel = 0;
            if(!isUndef(inst.args[2]))
            {
              ShaderVariable arg;
              RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
              mipLevel = arg.value.u32v[0];
            }
            int dim;
            data = apiWrapper->GetResourceInfo(resRef->resourceBase.resClass, binding, mipLevel,
                                               m_ShaderType, dim);
            // Returns a vector with: w, h, d, numLevels
            result.value = data.value;
            // DXIL reports the vector result as a struct of 4 x int.
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
            RDCASSERTEQUAL(retType->members.size(), 4);
            const Type *baseType = retType->members[0];
            RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(baseType->scalarType, Type::ScalarKind::Int);
            RDCASSERTEQUAL(baseType->bitWidth, 32);
            result.type = VarType::SInt;
            result.columns = 4;
            break;
          }
          case DXOp::Texture2DMSGetSamplePosition:
          {
            // Texture2DMSGetSamplePosition(srv,index)
            rdcstr handleId = GetArgumentName(1);
            const ResourceReference *resRef = GetResource(handleId);
            if(!resRef)
              break;

            BindingSlot binding(resRef->resourceBase.regBase, resRef->resourceBase.space);
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            const char *opString = ToStr(dxOpCode).c_str();
            ShaderVariable data = apiWrapper->GetSampleInfo(resRef->resourceBase.resClass, binding,
                                                            m_ShaderType, opString);

            uint32_t sampleCount = data.value.u32v[0];
            uint32_t sampleIndex = arg.value.u32v[0];
            DXDebug::get_sample_position(sampleIndex, sampleCount, result.value.f32v.data());

            // DXIL reports the vector result as a struct of 2 x float.
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
            RDCASSERTEQUAL(retType->members.size(), 2);
            const Type *baseType = retType->members[0];
            RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(baseType->scalarType, Type::ScalarKind::Float);
            RDCASSERTEQUAL(baseType->bitWidth, 32);
            result.type = VarType::Float;
            result.columns = 2;
            break;
          }
          case DXOp::RenderTargetGetSampleCount:
          {
            const char *opString = ToStr(dxOpCode).c_str();
            ShaderVariable data = apiWrapper->GetRenderTargetSampleInfo(m_ShaderType, opString);
            result.value.u32v[0] = data.value.u32v[0];
            break;
          }
          case DXOp::RenderTargetGetSamplePosition:
          {
            const char *opString = ToStr(dxOpCode).c_str();
            ShaderVariable data = apiWrapper->GetRenderTargetSampleInfo(m_ShaderType, opString);
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));

            uint32_t sampleCount = data.value.u32v[0];
            uint32_t sampleIndex = arg.value.u32v[0];
            DXDebug::get_sample_position(sampleIndex, sampleCount, result.value.f32v.data());

            // DXIL reports the vector result as a struct of 2 x float.
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
            RDCASSERTEQUAL(retType->members.size(), 2);
            const Type *baseType = retType->members[0];
            RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(baseType->scalarType, Type::ScalarKind::Float);
            RDCASSERTEQUAL(baseType->bitWidth, 32);
            result.type = VarType::Float;
            result.columns = 2;
            break;
          }
          case DXOp::Sample:
          case DXOp::SampleLevel:
          case DXOp::SampleCmpLevelZero:
          {
            // TODO
            // case DXOp::SampleBias:
            // case DXOp::SampleGrad:
            // case DXOp::SampleCmp:
            // case DXOp::SampleCmpLevel:
            // case DXOp::SampleCmpGrad:
            // case DXOp::SampleCmpBias:
            rdcstr handleId = GetArgumentName(1);
            const ResourceReference *resRef = GetResource(handleId);
            if(!resRef)
              break;

            PerformGPUResourceOp(workgroups, opCode, dxOpCode, resRef, apiWrapper, inst, result);
            eventFlags |= ShaderEvents::SampleLoadGather;
            break;
          }
          case DXOp::TextureLoad:
          case DXOp::TextureStore:
          case DXOp::RawBufferLoad:
          case DXOp::RawBufferStore:
          case DXOp::BufferLoad:
          case DXOp::BufferStore:
          {
            // TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
            // TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
            // BufferLoad(res,index,wot)
            // BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
            // RawBufferLoad(srv,index,elementOffset,mask,alignment)
            // RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
            rdcstr handleId = GetArgumentName(1);
            const ResourceReference *resRef = GetResource(handleId);
            if(!resRef)
              break;

            ResourceClass resClass = resRef->resourceBase.resClass;
            // SRV TextureLoad is done on the GPU
            if((dxOpCode == DXOp::TextureLoad) && (resClass == ResourceClass::SRV))
            {
              PerformGPUResourceOp(workgroups, opCode, dxOpCode, resRef, apiWrapper, inst, result);
              eventFlags |= ShaderEvents::SampleLoadGather;
              break;
            }

            const bool raw = (dxOpCode == DXOp::RawBufferLoad) || (dxOpCode == DXOp::RawBufferStore);
            const bool buffer = (dxOpCode == DXOp::BufferLoad) || (dxOpCode == DXOp::BufferStore) ||
                                (dxOpCode == DXOp::TextureLoad) || (dxOpCode == DXOp::TextureStore);
            bool byteAddress = raw;

            const bool load = (dxOpCode == DXOp::TextureLoad) || (dxOpCode == DXOp::BufferLoad) ||
                              (dxOpCode == DXOp::RawBufferLoad);
            // TODO: could be a TextureStore
            const Type *baseType = NULL;
            uint32_t resultNumComps = 0;
            if(load)
            {
              // DXIL will create a vector of a single type with total size of 16-bytes
              // The vector element type will change to match what value will be extracted
              // ie. float, double, int, short
              // DXIL reports this vector as a struct of N members of Element type.
              RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
              baseType = retType->members[0];
              resultNumComps = retType->members.count() - 1;
            }
            else
            {
              // Get the type from the first value to be stored
              baseType = inst.args[4]->type;
              resultNumComps = 1;
            }
            if(baseType)
            {
              uint32_t elemByteSize = (baseType->bitWidth / 8);
              RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
              result.type = ConvertDXILTypeToVarType(baseType);
              result.columns = (uint8_t)resultNumComps;
              RDCASSERTEQUAL(GetElementByteSize(result.type), elemByteSize);
            }

            uint32_t structOffset = 0;
            uint32_t stride = 0;

            if(buffer)
            {
              // Fixed for BufferLoad, BufferStore
              stride = 4;
            }
            else if(raw)
            {
              stride = 1;
            }

            RDCASSERT(stride != 0);

            const byte *data = NULL;
            size_t dataSize = 0;
            bool texData = false;
            uint32_t rowPitch = 0;
            uint32_t depthPitch = 0;
            uint32_t firstElem = 0;
            uint32_t numElems = 0;
            GlobalState::ViewFmt fmt;

            BindingSlot resourceBinding(resRef->resourceBase.regBase, resRef->resourceBase.space);
            RDCASSERT((resClass == ResourceClass::SRV || resClass == ResourceClass::UAV), resClass);
            switch(resClass)
            {
              case ResourceClass::UAV:
              {
                GlobalState::UAVIterator uavIter = m_GlobalState.uavs.find(resourceBinding);
                if(uavIter == m_GlobalState.uavs.end())
                {
                  apiWrapper->FetchUAV(resourceBinding);
                  uavIter = m_GlobalState.uavs.find(resourceBinding);
                }
                const GlobalState::UAVData &uav = uavIter->second;
                data = uav.data.data();
                dataSize = uav.data.size();
                texData = uav.tex;
                rowPitch = uav.rowPitch;
                depthPitch = uav.depthPitch;
                firstElem = uav.firstElement;
                numElems = uav.numElements;
                fmt = uav.format;
                stride = fmt.Stride();
                break;
              }
              case ResourceClass::SRV:
              {
                GlobalState::SRVIterator srvIter = m_GlobalState.srvs.find(resourceBinding);
                if(srvIter == m_GlobalState.srvs.end())
                {
                  apiWrapper->FetchSRV(resourceBinding);
                  srvIter = m_GlobalState.srvs.find(resourceBinding);
                }
                const GlobalState::SRVData &srv = srvIter->second;
                data = srv.data.data();
                dataSize = srv.data.size();
                firstElem = srv.firstElement;
                numElems = srv.numElements;
                fmt = srv.format;
                break;
              }
              default: RDCERR("Unexpected ResourceClass %s", ToStr(resClass).c_str()); break;
            }

            // Unbound resource
            if(data == NULL)
            {
              if(load)
              {
                result.value.f32v[0] = 0.0f;
                result.value.f32v[1] = 0.0f;
                result.value.f32v[2] = 0.0f;
                result.value.f32v[3] = 0.0f;
              }
              break;
            }

            uint32_t texCoords[3] = {0, 0, 0};
            uint32_t elemIdx = 0;
            ShaderVariable arg;
            // TODO: BufferStore 2D
            if(!texData)
            {
              if(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg))
                elemIdx = arg.value.u32v[0];
            }
            else
            {
              size_t offsetStart = (dxOpCode == DXOp::TextureLoad) ? 3 : 2;
              if(GetShaderVariable(inst.args[offsetStart], opCode, dxOpCode, arg))
                texCoords[0] = (int8_t)arg.value.s32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 1], opCode, dxOpCode, arg))
                texCoords[1] = (int8_t)arg.value.s32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 2], opCode, dxOpCode, arg))
                texCoords[2] = (int8_t)arg.value.s32v[0];
            }

            // Update the format if it is Typeless
            // See FetchUAV() comment about root buffers being typeless
            if(fmt.fmt == CompType::Typeless)
            {
              FillViewFmt(result.type, fmt);
              byteAddress = (fmt.stride == 0);
              fmt.numComps = result.columns;
            }
            else
            {
              fmt.stride = fmt.Stride();
            }
            if(fmt.stride != 0)
              stride = fmt.stride;
            else
              stride = 1;
            RDCASSERTNOTEQUAL(fmt.fmt, CompType::Typeless);

            // buffer offsets are in bytes
            // firstElement/numElements is in format-sized units. Convert to byte offsets
            if(byteAddress)
            {
              // For byte address buffer
              // element index is in bytes and a multiple of four, GPU behavious seems to be to round down
              elemIdx = elemIdx & ~0x3;
              firstElem *= RDCMIN(4, fmt.byteWidth);
              numElems *= RDCMIN(4, fmt.byteWidth);
            }

            uint64_t dataOffset = 0;
            if(raw || buffer)
            {
              ShaderVariable offset;
              if(GetShaderVariable(inst.args[3], opCode, dxOpCode, offset))
                dataOffset = offset.value.u64v[0];
            }

            if(texData)
            {
              dataOffset += texCoords[0] * fmt.Stride();
              dataOffset += texCoords[1] * rowPitch;
              dataOffset += texCoords[2] * depthPitch;
            }
            else
            {
              dataOffset += (firstElem + elemIdx) * stride;
              dataOffset += structOffset;
            }

            // NULL resource or out of bounds
            if((!texData && elemIdx >= numElems) || (texData && dataOffset >= dataSize))
            {
              if(load)
              {
                result.value.f32v[0] = 0.0f;
                result.value.f32v[1] = 0.0f;
                result.value.f32v[2] = 0.0f;
                result.value.f32v[3] = 0.0f;
              }
            }
            else
            {
              data += dataOffset;
              int numComps = fmt.numComps;
              // Clamp the number of components to read based on the amount of data in the buffer
              int maxNumComps = (int)((dataSize - dataOffset) / fmt.byteWidth);
              fmt.numComps = RDCMIN(fmt.numComps, maxNumComps);
              size_t maxOffset = (firstElem + numElems) * stride + structOffset;
              maxNumComps = (int)((maxOffset - dataOffset) / fmt.byteWidth);
              fmt.numComps = RDCMIN(fmt.numComps, maxNumComps);

              // For stores load the whole data, update the component, save the whole data back
              // This is to support per component writes to packed formats
              result.value = TypedUAVLoad(fmt, data);

              // Zero out any out of bounds components
              if(fmt.numComps < numComps)
              {
                for(uint32_t c = fmt.numComps; c < result.columns; ++c)
                  result.value.f32v[c] = 0.0f;
              }
              if(!load)
              {
                numComps = 0;
                // Modify the correct components
                for(uint32_t a = 4; a < 8; ++a)
                {
                  if(GetShaderVariable(inst.args[a], opCode, dxOpCode, arg))
                  {
                    // TODO: get the type of the value's make sure it an expected value
                    const uint32_t dstComp = a - 4;
                    const uint32_t srcComp = 0;
                    result.value.u32v[dstComp] = arg.value.u32v[srcComp];
                    ++numComps;
                  }
                }
                fmt.numComps = numComps;
                TypedUAVStore(fmt, (byte *)data, result.value);
              }
            }
            break;
          }
          case DXOp::AnnotateHandle:
          case DXOp::CreateHandle:
          case DXOp::CreateHandleFromBinding:
          {
            // AnnotateHandle(res,props)
            // CreateHandle(resourceClass,rangeId,index,nonUniformIndex
            // CreateHandleFromBinding(bind,index,nonUniformIndex)
            rdcstr baseResource = result.name;
            if(dxOpCode == DXOp::AnnotateHandle)
              baseResource = GetArgumentName(1);

            const ResourceReference *resRef = m_Program.GetResourceReference(baseResource);
            if(resRef)
            {
              const rdcarray<ShaderVariable> *list = NULL;
              // a static known handle which should be in the global resources container
              switch(resRef->resourceBase.resClass)
              {
                case ResourceClass::CBuffer: list = &m_GlobalState.constantBlocks; break;
                case ResourceClass::SRV: list = &m_GlobalState.readOnlyResources; break;
                case ResourceClass::UAV: list = &m_GlobalState.readWriteResources; break;
                case ResourceClass::Sampler: list = &m_GlobalState.samplers; break;
                default:
                  RDCERR("Invalid ResourceClass %u", (uint32_t)resRef->resourceBase.resClass);
                  break;
              };
              RDCASSERT(list);

              rdcstr resName = m_Program.GetHandleAlias(baseResource);
              const rdcarray<ShaderVariable> &resources = *list;
              for(uint32_t i = 0; i < resources.size(); ++i)
              {
                if(resources[i].name == resName)
                {
                  result = resources[i];
                  break;
                }
              }
              recordChange = false;
              RDCASSERT(!result.name.empty());
            }
            else
            {
              // TODO: support for dynamic handles i.e. array lookups
              RDCERR("Unhandled dynamic handle");
              /*
                            DescriptorCategory category;
                            uint32_t index;
                            uint32_t arrayElement = 0;
                            // Need to make a shader variable for the return : it needs to have a
                 binding point result.SetBindIndex(ShaderBindIndex(category, index, arrayElement));
              */
            }
            break;
          }
          case DXOp::CBufferLoadLegacy:
          {
            // CBufferLoadLegacy(handle,regIndex)
            // Need to find the resource
            rdcstr handleName = GetArgumentName(1);
            const ResourceReference *resRef = GetResource(handleName);
            if(!resRef)
              break;

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            uint32_t regIndex = arg.value.u32v[0];

            Id handleId = GetArgumentId(1);
            RDCASSERT(m_Live.contains(handleId));
            auto itVar = m_LiveVariables.find(handleId);
            RDCASSERT(itVar != m_LiveVariables.end());
            result.value = itVar->second.members[regIndex].value;

            // DXIL will create a vector of a single type with total size of 16-bytes
            // The vector element type will change to match what value will be extracted
            // ie. float, double, int, short
            // DXIL reports this vector as a struct of N members of Element type.
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
            const Type *baseType = retType->members[0];
            RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
            result.type = ConvertDXILTypeToVarType(baseType);
            result.columns = 16 / GetElementByteSize(result.type);
            break;
          }
          case DXOp::Frc:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            // TODO: HALF TYPE
            // TODO: DOUBLE TYPE
            RDCASSERTEQUAL(arg.type, VarType::Float);
            RDCASSERTEQUAL(arg.rows, 1);
            RDCASSERTEQUAL(arg.columns, 1);
            result.value.f32v[0] = arg.value.f32v[0] - floorf(arg.value.f32v[0]);
            break;
          }
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
          case DXOp::Log:
          case DXOp::Sqrt:
          case DXOp::Rsqrt:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            apiWrapper->CalculateMathIntrinsic(dxOpCode, arg, result);
            break;
          }
          case DXOp::Round_ne:
          case DXOp::Round_ni:
          case DXOp::Round_z:
          case DXOp::Round_pi:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            // TODO: HALF TYPE
            // TODO: DOUBLE TYPE
            RDCASSERTEQUAL(arg.rows, 1);
            RDCASSERTEQUAL(arg.columns, 1);
            RDCASSERTEQUAL(arg.type, VarType::Float);
            if(dxOpCode == DXOp::Round_pi)
              // Round_pi(value) : positive infinity -> ceil()
              result.value.f32v[0] = ceilf(arg.value.f32v[0]);
            else if(dxOpCode == DXOp::Round_ne)
              // Round_ne(value) : to nearest even int (banker's rounding)
              result.value.f32v[0] = round_ne(arg.value.f32v[0]);
            else if(dxOpCode == DXOp::Round_ni)
              // Round_ni(value) : negative infinity -> floor()
              result.value.f32v[0] = floorf(arg.value.f32v[0]);
            else if(dxOpCode == DXOp::Round_z)
              // Round_z(value) : towards zero
              result.value.f32v[0] =
                  arg.value.f32v[0] < 0 ? ceilf(arg.value.f32v[0]) : floorf(arg.value.f32v[0]);
            break;
          }
          case DXOp::FAbs:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.type, VarType::Float);
            RDCASSERTEQUAL(result.type, VarType::Float);
            result.value.f32v[0] = fabsf(arg.value.f32v[0]);
            break;
          }
          case DXOp::IMin:
          case DXOp::IMax:
          {
            // IMin(a,b)
            // IMax(a,b)
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, VarType::SInt);
            RDCASSERTEQUAL(b.type, VarType::SInt);
            RDCASSERTEQUAL(result.type, VarType::SInt);
            if(dxOpCode == DXOp::IMin)
              result.value.s32v[0] = RDCMIN(a.value.s32v[0], b.value.s32v[0]);
            else if(dxOpCode == DXOp::IMax)
              result.value.s32v[0] = RDCMAX(a.value.s32v[0], b.value.s32v[0]);
          }
          case DXOp::UMin:
          case DXOp::UMax:
          {
            // UMin(a,b)
            // UMax(a,b)
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, VarType::SInt);
            RDCASSERTEQUAL(b.type, VarType::SInt);
            RDCASSERTEQUAL(result.type, VarType::SInt);
            if(dxOpCode == DXOp::UMin)
              result.value.u32v[0] = RDCMIN(a.value.u32v[0], b.value.u32v[0]);
            else if(dxOpCode == DXOp::UMax)
              result.value.u32v[0] = RDCMAX(a.value.u32v[0], b.value.u32v[0]);
            break;
          }
          case DXOp::FMin:
          case DXOp::FMax:
          {
            // FMin(a,b)
            // FMax(a,b)
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, VarType::Float);
            RDCASSERTEQUAL(b.type, VarType::Float);
            RDCASSERTEQUAL(result.type, VarType::Float);
            if(dxOpCode == DXOp::FMin)
              result.value.f32v[0] = dxbc_min(a.value.f32v[0], b.value.f32v[0]);
            else if(dxOpCode == DXOp::FMax)
              result.value.f32v[0] = dxbc_max(a.value.f32v[0], b.value.f32v[0]);
            break;
          }
          case DXOp::FMad:
          {
            // FMad(a,b,c)
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERTEQUAL(a.type, VarType::Float);
            RDCASSERTEQUAL(b.type, VarType::Float);
            RDCASSERTEQUAL(c.type, VarType::Float);
            RDCASSERTEQUAL(result.type, VarType::Float);
            result.value.f32v[0] = (a.value.f32v[0] * b.value.f32v[0]) + c.value.f32v[0];
            break;
          }
          case DXOp::Saturate:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.type, VarType::Float);
            RDCASSERTEQUAL(result.type, VarType::Float);
            result.value.f32v[0] = dxbc_min(1.0f, dxbc_max(0.0f, arg.value.f32v[0]));
            break;
          }
          case DXOp::Dot2:
          case DXOp::Dot3:
          case DXOp::Dot4:
          {
            // Float or Int
            // 2/3/4 Vector
            // Result type must match input types
            uint32_t numComps = 4;
            uint32_t argAStart = 1;
            if(dxOpCode == DXOp::Dot2)
              numComps = 2;
            else if(dxOpCode == DXOp::Dot3)
              numComps = 3;
            uint32_t argBStart = argAStart + numComps;

            result.value.f32v[0] = 0.0f;
            bool isFloat = (result.type == VarType::Float);
            if(isFloat || result.type == VarType::SInt)
            {
              for(uint32_t c = 0; c < numComps; ++c)
              {
                ShaderVariable a;
                ShaderVariable b;
                RDCASSERT(GetShaderVariable(inst.args[argAStart + c], opCode, dxOpCode, a));
                RDCASSERT(GetShaderVariable(inst.args[argBStart + c], opCode, dxOpCode, b));
                RDCASSERTEQUAL(result.type, a.type);
                RDCASSERTEQUAL(result.type, b.type);
                if(isFloat)
                  result.value.f32v[0] += a.value.f32v[0] * b.value.f32v[0];
                else
                  result.value.s32v[0] += a.value.s32v[0] * b.value.s32v[0];
              }
            }
            else
            {
              RDCERR("Unhandled result type %s", ToStr(result.type).c_str());
            }
            break;
          }
          case DXOp::FirstbitHi:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            unsigned char found = BitScanReverse((DWORD *)&result.value.u32v[0], arg.value.u32v[0]);
            if(found == 0)
              result.value.u32v[0] = ~0U;
            else
              // BitScanReverse result which counts index 0 as the LSB and firstbit_hi counts index 0 as the MSB
              result.value.u32v[0] = 31 - result.value.u32v[0];
            break;
          }
          case DXOp::FirstbitLo:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            unsigned char found = BitScanForward((DWORD *)&result.value.u32v[0], arg.value.u32v[0]);
            if(found == 0)
              result.value.u32v[0] = ~0U;
            break;
          }
          case DXOp::FirstbitSHi:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            uint32_t u = arg.value.u32v[0];
            if(arg.value.s32v[0] < 0)
              u = ~u;

            unsigned char found = BitScanReverse((DWORD *)&result.value.u32v[0], u);

            if(found == 0)
              result.value.u32v[0] = ~0U;
            else
              // BitScanReverse result which counts index 0 as the LSB and firstbit_shi counts index 0 as the MSB
              result.value.u32v[0] = 31 - result.value.u32v[0];
            break;
          }
          case DXOp::ThreadId:
          {
            // ThreadId(component) -> SV_DispatchThreadID
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.type, VarType::SInt);
            RDCASSERTEQUAL(result.type, VarType::SInt);
            uint32_t component = arg.value.u32v[0];
            RDCASSERT(m_GlobalState.builtinInputs.count(ShaderBuiltin::DispatchThreadIndex) != 0);
            result.value.u32v[0] =
                m_GlobalState.builtinInputs.at(ShaderBuiltin::DispatchThreadIndex).value.u32v[component];
            break;
          }
          case DXOp::GroupId:
          {
            // GroupId(component) -> SV_GroupID
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.type, VarType::SInt);
            RDCASSERTEQUAL(result.type, VarType::SInt);
            uint32_t component = arg.value.u32v[0];
            RDCASSERT(m_GlobalState.builtinInputs.count(ShaderBuiltin::GroupIndex) != 0);
            result.value.u32v[0] =
                m_GlobalState.builtinInputs.at(ShaderBuiltin::GroupIndex).value.u32v[component];
            break;
          }
          case DXOp::ThreadIdInGroup:
          {
            // ThreadIdInGroup(component)->SV_GroupThreadID
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.type, VarType::SInt);
            RDCASSERTEQUAL(result.type, VarType::SInt);
            uint32_t component = arg.value.u32v[0];
            RDCASSERT(m_GlobalState.builtinInputs.count(ShaderBuiltin::GroupThreadIndex) != 0);
            result.value.u32v[0] =
                m_GlobalState.builtinInputs.at(ShaderBuiltin::GroupThreadIndex).value.u32v[component];
            break;
          }
          case DXOp::FlattenedThreadIdInGroup:
          {
            // FlattenedThreadIdInGroup()->SV_GroupIndex
            RDCASSERTEQUAL(result.type, VarType::SInt);
            RDCASSERT(m_GlobalState.builtinInputs.count(ShaderBuiltin::GroupFlatIndex) != 0);
            result.value.u32v[0] =
                m_GlobalState.builtinInputs.at(ShaderBuiltin::GroupFlatIndex).value.u32v[0];
            break;
          }
          case DXOp::DerivCoarseX:
          case DXOp::DerivCoarseY:
          {
            if(m_ShaderType != DXBC::ShaderType::Pixel || workgroups.size() != 4)
            {
              RDCERR("Undefined results using derivative instruction outside of a pixel shader.");
            }
            else
            {
              Id id = GetArgumentId(1);
              if(dxOpCode == DXOp::DerivCoarseX)
                result.value = DDX(false, opCode, dxOpCode, workgroups, id);
              else
                result.value = DDY(false, opCode, dxOpCode, workgroups, id);
            }
            break;
          }
          case DXOp::TempRegLoad:
          case DXOp::TempRegStore:
          case DXOp::MinPrecXRegLoad:
          case DXOp::MinPrecXRegStore:
          case DXOp::IsNaN:
          case DXOp::IsInf:
          case DXOp::IsFinite:
          case DXOp::IsNormal:
          case DXOp::Bfrev:
          case DXOp::Countbits:
          case DXOp::IMul:
          case DXOp::UMul:
          case DXOp::UDiv:
          case DXOp::UAddc:
          case DXOp::USubb:
          case DXOp::Fma:
          case DXOp::IMad:
          case DXOp::UMad:
          case DXOp::Msad:
          case DXOp::Ibfe:
          case DXOp::Ubfe:
          case DXOp::Bfi:
          case DXOp::CBufferLoad:
          case DXOp::SampleBias:
          case DXOp::SampleGrad:
          case DXOp::SampleCmp:
          case DXOp::BufferUpdateCounter:
          case DXOp::CheckAccessFullyMapped:
          case DXOp::TextureGather:
          case DXOp::TextureGatherCmp:
          case DXOp::AtomicBinOp:
          case DXOp::AtomicCompareExchange:
          case DXOp::Barrier:
          case DXOp::CalculateLOD:
          case DXOp::Discard:
          case DXOp::DerivFineX:
          case DXOp::DerivFineY:
          case DXOp::EvalSnapped:
          case DXOp::EvalSampleIndex:
          case DXOp::EvalCentroid:
          case DXOp::SampleIndex:
          case DXOp::Coverage:
          case DXOp::InnerCoverage:
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
          case DXOp::CreateHandleFromHeap:
          case DXOp::Unpack4x8:
          case DXOp::Pack4x8:
          case DXOp::IsHelperLane:
          case DXOp::QuadVote:
          case DXOp::TextureGatherRaw:
          case DXOp::SampleCmpLevel:
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
          case DXOp::SampleCmpGrad:
          case DXOp::SampleCmpBias:
          case DXOp::StartVertexLocation:
          case DXOp::StartInstanceLocation:
          case DXOp::NumOpCodes:
            RDCERR("Unhandled dx.op method `%s` %s", callFunc->name.c_str(), ToStr(dxOpCode).c_str());
            break;
        }
      }
      else if(callFunc->family == FunctionFamily::LLVMDbg)
      {
        return false;
      }
      else
      {
        RDCERR("Unhandled call to function `%s`", callFunc->name.c_str());
        break;
      }
      break;
    }
    case Operation::Ret: m_Ended = true; break;
    case Operation::NoOp: return false;
    case Operation::Unreachable:
    {
      // TODO: DXOP::Discard might behave similarly
      m_Killed = true;
      RDCERR("Operation::Unreachable reached, terminating debugging!");
      return true;
    }
    case Operation::Branch:
    {
      m_PreviousBlock = m_Block;
      // Branch <label>
      // Branch <label_true> <label_false> <BOOL_VAR>
      uint32_t targetArg = 0;
      if(inst.args.size() > 1)
      {
        ShaderVariable cond;
        RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, cond));
        if(!cond.value.u32v[0])
          targetArg = 1;
      }
      const Block *block = cast<Block>(inst.args[targetArg]);
      RDCASSERT(block);
      uint32_t blockId = block->id;
      if(blockId < m_FunctionInfo->function->blocks.size())
      {
        m_Block = blockId;
        m_FunctionInstructionIdx = m_FunctionInfo->function->blocks[m_Block]->startInstructionIdx;
        m_GlobalInstructionIdx = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
      }
      else
      {
        RDCERR("Unknown branch target %u '%s'", m_Block, GetArgumentName(targetArg).c_str());
      }
      if(m_State && !m_Ended)
        m_State->nextInstruction = m_GlobalInstructionIdx;
      break;
    }
    case Operation::Phi:
    {
      // Pairs of { value, label }
      DXIL::Value *dxilValue = NULL;
      for(uint32_t a = 0; a < inst.args.size(); a += 2)
      {
        const Block *block = cast<Block>(inst.args[a + 1]);
        RDCASSERT(block);
        uint32_t blockId = block->id;
        if(blockId == m_PreviousBlock)
        {
          dxilValue = inst.args[a];
          break;
        }
      }
      if(dxilValue)
      {
        ShaderVariable arg;
        RDCASSERT(GetShaderVariable(dxilValue, opCode, dxOpCode, arg));
        result.value = arg.value;
        break;
      }
      else
      {
        RDCERR("PreviousBlock not found in Phi list: %u", m_PreviousBlock);
      }
      break;
    }
    case Operation::ExtractVal:
    {
      // TODO: need helper function to convert DXIL::Type* -> ShaderVariable
      Id src = GetArgumentId(0);
      const ShaderVariable &srcVal = m_LiveVariables[src];
      RDCASSERT(srcVal.members.empty());
      // TODO: handle greater than one index
      RDCASSERTEQUAL(inst.args.size(), 2);
      uint32_t idx = ~0U;
      RDCASSERT(getival(inst.args[1], idx));
      RDCASSERT(idx < srcVal.columns);

      RDCASSERTEQUAL(result.type, srcVal.type);
      switch(result.type)
      {
        case VarType::Double: result.value.f64v[0] = srcVal.value.f64v[idx]; break;
        case VarType::Float: result.value.f32v[0] = srcVal.value.f32v[idx]; break;
        case VarType::Half: result.value.f16v[0] = srcVal.value.f16v[idx]; break;
        case VarType::SLong: result.value.s64v[0] = srcVal.value.s64v[idx]; break;
        case VarType::SInt: result.value.s32v[0] = srcVal.value.s32v[idx]; break;
        case VarType::SShort: result.value.s16v[0] = srcVal.value.s16v[idx]; break;
        case VarType::SByte: result.value.s8v[0] = srcVal.value.s8v[idx]; break;
        default: RDCERR("Unexpected Result VarType %s", ToStr(result.type).c_str()); break;
      };
      break;
    }
    case Operation::Select:
    {
      // arg[2] ? arg[0] : arg[1]
      ShaderVariable selector;
      RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, selector));
      uint32_t resultIdx = (selector.value.u32v[0] == 1) ? 0 : 1;
      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[resultIdx], opCode, dxOpCode, arg));
      result.value = arg.value;
      break;
    }
    case Operation::Load:
    {
      // TODO: full proper load from resource memory i.e. group shared
      // Currently only supporting Stack allocated pointers
      // Load(ptr)
      Id ptrId = GetArgumentId(0);
      RDCASSERT(m_StackAllocPointers.count(ptrId) == 1);
      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      result.value = arg.value;
      break;
    }
    case Operation::Store:
    {
      // TODO: full proper store to resource memory i.e. group shared
      // Currently only supporting Stack allocated pointers
      // Store(ptr, value)
      Id ptrId = GetArgumentId(0);
      auto itPtr = m_StackAllocPointers.find(ptrId);
      RDCASSERT(itPtr != m_StackAllocPointers.end());
      const StackAllocPointer &ptr = itPtr->second;
      Id baseMemoryId = ptr.baseMemoryId;
      RDCASSERT(ptr.backingMemory);
      RDCASSERTNOTEQUAL(baseMemoryId, DXIL_INVALID_ID);
      auto itAlloc = m_StackAllocs.find(baseMemoryId);
      RDCASSERT(itAlloc != m_StackAllocs.end());
      StackAlloc &alloc = itAlloc->second;

      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
      RDCASSERTEQUAL(resultId, DXIL_INVALID_ID);

      // Memory copy from value to backing memory
      VarType type = ConvertDXILTypeToVarType(inst.args[1]->type);
      size_t size = GetElementByteSize(type);
      RDCASSERT(size <= alloc.size);
      RDCASSERT(size < sizeof(arg.value.f32v));
      memcpy(ptr.backingMemory, &arg.value.f32v[0], size);

      ShaderVariableChange change;
      change.before = m_LiveVariables[baseMemoryId];
      ShaderVariable &baseMemory = m_LiveVariables[baseMemoryId];
      // TODO: Make this be a helper function UpdateVariableFromBackingMemory()
      // Memory copy from backing memory to base memory variable
      const uint8_t *src = (uint8_t *)alloc.backingMemory;
      size_t elementSize = GetElementByteSize(baseMemory.type);
      for(uint32_t i = 0; i < baseMemory.rows; ++i)
      {
        memcpy(&m_LiveVariables[baseMemoryId].members[i].value.f32v[0], src, elementSize);
        src += elementSize;
      }

      // record the change to the base memory variable
      change.after = m_LiveVariables[baseMemoryId];
      if(m_State)
        m_State->changes.push_back(change);

      // Update the ptr variable value
      // Set the result to be the ptr variable whcih will then be reocrded as a change
      result = m_LiveVariables[ptrId];
      result.value = arg.value;
      resultId = ptrId;
      break;
    }
    case Operation::Alloca:
    {
      const DXIL::Type *resultType = inst.type->inner;
      uint32_t countElems = 1;
      VarType baseType = ConvertDXILTypeToVarType(resultType);
      if(resultType->type == DXIL::Type::TypeKind::Array)
      {
        countElems = resultType->elemCount;
        resultType = resultType->inner;
      }
      RDCASSERT((resultType->type == DXIL::Type::TypeKind::Scalar) ||
                (resultType->type == DXIL::Type::TypeKind::Struct));
      // TODO: NEED TO DEMANGLE THE NAME TO DEMANGLE TO MATCH DISASSEMBLY
      result.type = baseType;
      result.rows = (uint8_t)countElems;
      result.members.resize(countElems);
      for(uint32_t i = 0; i < countElems; ++i)
      {
        result.members[i].name = "[" + ToStr(i) + "]";
        result.members[i].type = baseType;
        result.members[i].rows = 1;
        result.members[i].columns = 1;
      }
      // Add the SSA to m_StackAllocs with its backing memory and size
      size_t size = countElems * GetElementByteSize(baseType);
      void *backingMem = malloc(size);
      StackAlloc &alloc = m_StackAllocs[resultId];
      alloc = {backingMem, size};

      // For non-array allocs set the backing memory now instead of in GetElementPtr
      m_StackAllocPointers[resultId] = {resultId, backingMem, size};
      break;
    }
    case Operation::GetElementPtr:
    {
      const DXIL::Type *resultType = inst.type->inner;
      Id ptrId = GetArgumentId(0);

      // Only handling stack allocations at the moment
      RDCASSERT(m_StackAllocs.count(ptrId) == 1);
      RDCASSERT(m_LiveVariables.count(ptrId) == 1);

      // arg[1..] : indecies 1...N
      rdcarray<uint64_t> indexes;
      indexes.reserve(inst.args.size() - 1);
      for(uint32_t a = 1; a < inst.args.size(); ++a)
      {
        ShaderVariable arg;
        RDCASSERT(GetShaderVariable(inst.args[a], opCode, dxOpCode, arg));
        indexes.push_back(arg.value.u64v[0]);
      }

      // Index 0 is in ptr terms as if pointer was an array of pointers
      RDCASSERTEQUAL(indexes[0], 0);
      uint64_t offset = 0;

      // TODO: Resolve indexes to a single offset
      const ShaderVariable &basePtr = m_LiveVariables[ptrId];
      if(indexes.size() > 1)
        offset += indexes[1] * GetElementByteSize(basePtr.type);
      RDCASSERT(indexes.size() <= 2);

      // TODO: function to convert DXIL::Type* -> ShaderVariable
      VarType baseType = ConvertDXILTypeToVarType(resultType);
      RDCASSERTNOTEQUAL(resultType->type, DXIL::Type::TypeKind::Struct);
      RDCASSERTEQUAL(resultType->type, DXIL::Type::TypeKind::Scalar);

      uint32_t countElems = RDCMAX(1U, resultType->elemCount);
      size_t size = countElems * GetElementByteSize(baseType);

      // Copy from the backing memory to the result
      StackAlloc &alloc = m_StackAllocs[ptrId];
      uint8_t *backingMemory = (uint8_t *)alloc.backingMemory;

      result.type = baseType;
      result.rows = (uint8_t)countElems;
      backingMemory += offset;
      m_StackAllocPointers[resultId] = {ptrId, backingMemory, size};

      RDCASSERT(offset + size <= alloc.size);
      RDCASSERT(size < sizeof(result.value.f32v));
      memcpy(&result.value.f32v[0], backingMemory, size);
      break;
    }
    case Operation::Bitcast:
    {
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      result.value = a.value;
      break;
    }
    case Operation::SToF:
    {
      const Type *argType = inst.args[0]->type;
      RDCASSERTEQUAL(argType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(argType->scalarType, Type::Int);
      int64_t valueA = 0;
      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      switch(argType->bitWidth)
      {
        case 64: valueA = arg.value.s64v[0]; break;
        case 32: valueA = arg.value.s32v[0]; break;
        case 16: valueA = arg.value.s16v[0]; break;
        case 8: valueA = arg.value.s8v[0]; break;
        case 1: valueA = arg.value.s8v[0]; break;
        default: RDCERR("Unexpected bitWidth %d", argType->bitWidth); break;
      }

      switch(result.type)
      {
        case VarType::Double: result.value.f64v[0] = (double)valueA; break;
        case VarType::Float: result.value.f32v[0] = (float)valueA; break;
        case VarType::Half: result.value.f16v[0].set((float)valueA); break;
        default: RDCERR("Unexpected Result VarType %s", ToStr(result.type).c_str()); break;
      };
      break;
    }
    case Operation::UToF:
    {
      // TODO: NEED TO GET THE ARGUMENT AT THE CORRECT INTEGER SIZE TO SUPPORT THIS
      // TODO: NEED TO GET THE UNSIGNED VALUE AT THE CORRECT INTEGER SIZE
      //_Y = uitofp i8 -1 to double; yields double : 255.0
      const Type *argType = inst.args[0]->type;
      RDCASSERTEQUAL(argType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(argType->scalarType, Type::Int);
      uint64_t valueA = 0;
      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      switch(argType->bitWidth)
      {
        case 64: valueA = (uint64_t)arg.value.s64v[0]; break;
        case 32: valueA = (uint64_t)(uint32_t)arg.value.s32v[0]; break;
        case 16: valueA = (uint64_t)(uint16_t)arg.value.s16v[0]; break;
        case 8: valueA = (uint64_t)(uint8_t)arg.value.s8v[0]; break;
        case 1: valueA = (uint64_t)arg.value.s8v[0]; break;
        default: RDCERR("Unexpected bitWidth %d", argType->bitWidth); break;
      }

      switch(result.type)
      {
        case VarType::Double: result.value.f64v[0] = (double)valueA; break;
        case VarType::Float: result.value.f32v[0] = (float)valueA; break;
        case VarType::Half: result.value.f16v[0].set((float)valueA); break;
        default: RDCERR("Unexpected Result VarType %s", ToStr(result.type).c_str()); break;
      };
      break;
    }
    case Operation::Add:
    case Operation::Sub:
    case Operation::Mul:
    {
      // TODO: check the bitwidth
      // TODO: support i1, i8, i16, i64
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      if(opCode == Operation::Add)
        result.value.u64v[0] = a.value.u64v[0] + b.value.u64v[0];
      else if(opCode == Operation::Mul)
        result.value.u64v[0] = a.value.u64v[0] * b.value.u64v[0];
      else if(opCode == Operation::Sub)
        result.value.u64v[0] = a.value.u64v[0] - b.value.u64v[0];
      break;
    }
    case Operation::FAdd:
    case Operation::FSub:
    case Operation::FMul:
    case Operation::FDiv:
    {
      // TODO: check the bitwidth
      // TODO: support F16, F64
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));

      if(opCode == Operation::FAdd)
        result.value.f32v[0] = a.value.f32v[0] + b.value.f32v[0];
      else if(opCode == Operation::FSub)
        result.value.f32v[0] = a.value.f32v[0] - b.value.f32v[0];
      else if(opCode == Operation::FMul)
        result.value.f32v[0] = a.value.f32v[0] * b.value.f32v[0];
      else if(opCode == Operation::FDiv)
        result.value.f32v[0] = a.value.f32v[0] / b.value.f32v[0];
      break;
    }
    case Operation::FOrdEqual:
    case Operation::FOrdNotEqual:
    {
      // TODO: handle different bitwidths
      // TODO: support F16, F64
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      uint32_t res = ~0U;
      if(opCode == Operation::FOrdEqual)
        res = (a.value.s32v[0] == b.value.f32v[0]) ? 1 : 0;
      else if(opCode == Operation::FOrdNotEqual)
        res = (a.value.s32v[0] != b.value.f32v[0]) ? 1 : 0;

      RDCASSERTNOTEQUAL(res, ~0U);
      RDCASSERTEQUAL(result.type, VarType::Bool);
      result.value.u32v[0] = res;
      break;
    }
    case Operation::IEqual:
    case Operation::INotEqual:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
      // TODO: assert bitwidth
      // TODO: support i1, i8, i16, i64
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      uint32_t res = ~0U;
      if(opCode == Operation::IEqual)
        res = (a.value.s32v[0] == b.value.s32v[0]) ? 1 : 0;
      else if(opCode == Operation::INotEqual)
        res = (a.value.s32v[0] != b.value.s32v[0]) ? 1 : 0;

      RDCASSERTNOTEQUAL(res, ~0U);
      RDCASSERTEQUAL(result.type, VarType::Bool);
      result.value.u32v[0] = res;
      break;
    }
    case Operation::FToS:
    case Operation::FToU:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
      // TODO: handle different bitwidths
      // TODO: support F16, F64
      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      if(opCode == Operation::FToS)
        result.value.s64v[0] = (int64_t)arg.value.f32v[0];
      else if(opCode == Operation::FToU)
        result.value.u64v[0] = (uint64_t)arg.value.f32v[0];
      break;
    }
    case Operation::Trunc:
    {
      uint32_t retBitWidth = retType->bitWidth;
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, Type::Int);
      RDCASSERT(inst.args[0]->type->bitWidth > retBitWidth);

      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      // Removes bits
      // %X = trunc i32 257 to i8; yields i8 : 1
      uint64_t mask = (1UL << retBitWidth) - 1UL;
      switch(retType->bitWidth)
      {
        case 32:
        case 16:
        case 8:
        case 1: result.value.u64v[0] = arg.value.u64v[0] & mask; break;
        default: RDCERR("Unexpected result bitWidth %d", retType->bitWidth); break;
      }
      break;
    }
    case Operation::ZExt:
    {
      uint32_t srcBitWidth = inst.args[0]->type->bitWidth;
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, Type::Int);
      RDCASSERT(srcBitWidth < retType->bitWidth);

      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      // Extras bits are 0's
      // %X = zext i32 257 to i64; yields i64 : 257
      uint64_t mask = (1UL << srcBitWidth) - 1UL;
      switch(retType->bitWidth)
      {
        case 64:
        case 32:
        case 16:
        case 8: result.value.u64v[0] = arg.value.u64v[0] & mask; break;
        default: RDCERR("Unexpected result bitWidth %d", retType->bitWidth); break;
      }
      break;
    }
    case Operation::SExt:
    {
      // Value Type
      // Value & Type must be Integer
      // Value->Type->bit_width < Type->bit_width
      // Sign Extend : copy sign (highest bit of Value) -> Result
      // %X = sext i8  -1 to i16              ; yields i16   :65535
      break;
    }
    case Operation::And:
    case Operation::Or:
    case Operation::Xor:
    case Operation::ShiftLeft:
    case Operation::LogicalShiftRight:
    case Operation::ArithShiftRight:
    {
      // Both args and the result must be Integer and the same bitwidth
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(inst.args[0]->type->bitWidth, inst.args[1]->type->bitWidth);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, Type::Int);
      RDCASSERTEQUAL(retType->bitWidth, inst.args[0]->type->bitWidth);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      if(opCode == Operation::And)
        result.value.u64v[0] = a.value.u64v[0] & b.value.u64v[0];
      else if(opCode == Operation::And)
        result.value.u64v[0] = a.value.u64v[0] | b.value.u64v[0];
      else if(opCode == Operation::Xor)
        result.value.u64v[0] = a.value.u64v[0] ^ b.value.u64v[0];
      else if(opCode == Operation::ShiftLeft)
        result.value.u64v[0] = a.value.u64v[0] << b.value.u64v[0];
      else if(opCode == Operation::LogicalShiftRight)
        result.value.u64v[0] = a.value.u64v[0] >> b.value.u64v[0];
      else if(opCode == Operation::ArithShiftRight)
        result.value.s64v[0] = a.value.s64v[0] << b.value.u64v[0];
      break;
    }
    case Operation::FPTrunc:
    case Operation::FPExt:
    case Operation::PtrToI:
    case Operation::IToPtr:
    case Operation::AddrSpaceCast:
    case Operation::FRem:
    case Operation::UDiv:
    case Operation::SDiv:
    case Operation::URem:
    case Operation::SRem:
    case Operation::FOrdFalse:
    case Operation::FOrdGreater:
    case Operation::FOrdGreaterEqual:
    case Operation::FOrdLess:
    case Operation::FOrdLessEqual:
    case Operation::FOrd:
    case Operation::FUnord:
    case Operation::FUnordEqual:
    case Operation::FUnordGreater:
    case Operation::FUnordGreaterEqual:
    case Operation::FUnordLess:
    case Operation::FUnordLessEqual:
    case Operation::FUnordNotEqual:
    case Operation::FOrdTrue:
    case Operation::UGreater:
    case Operation::UGreaterEqual:
    case Operation::ULess:
    case Operation::ULessEqual:
    case Operation::SGreater:
    case Operation::SGreaterEqual:
    case Operation::SLess:
    case Operation::SLessEqual:
    case Operation::ExtractElement:
    case Operation::InsertElement:
    case Operation::ShuffleVector:
    case Operation::InsertValue:
    case Operation::Switch:
    case Operation::Fence:
    case Operation::CompareExchange:
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
    case Operation::AtomicUMin: RDCERR("Unhandled LLVM opcode %s", ToStr(opCode).c_str()); break;
  };

  // Remove variables which have gone out of scope
  rdcarray<Id> liveIdsToRemove;
  for(const Id &id : m_Live)
  {
    // The fake output variable is always in scope
    if(id == m_OutputSSAId)
      continue;

    auto itRange = m_FunctionInfo->rangePerId.find(id);
    RDCASSERT(itRange != m_FunctionInfo->rangePerId.end());
    const uint32_t maxInstruction = itRange->second.max;
    if(m_FunctionInstructionIdx > maxInstruction)
    {
      RDCASSERTNOTEQUAL(id, resultId);
      liveIdsToRemove.push_back(id);
      m_DormantVariables[id] = m_LiveVariables[id];
      if(!m_Dormant.contains(id))
        m_Dormant.push_back(id);

      if(m_State)
      {
        ShaderVariableChange change;
        change.before = m_LiveVariables[id];
        m_State->changes.push_back(change);
      }

      m_LiveVariables.erase(id);
    }
  }
  for(const Id &id : liveIdsToRemove)
    m_Live.removeOne(id);

  if(!m_Ended)
  {
    const Instruction &nextInst = *m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
    Id nextResultId = nextInst.slot;
    // Bring back dormant variables which are now in scope
    rdcarray<Id> dormantIdsToRemove;
    for(const Id &id : m_Dormant)
    {
      auto itRange = m_FunctionInfo->rangePerId.find(id);
      RDCASSERT(itRange != m_FunctionInfo->rangePerId.end());
      const uint32_t minInstruction = itRange->second.min;
      if(m_FunctionInstructionIdx >= minInstruction)
      {
        const uint32_t maxInstruction = itRange->second.max;
        if(m_FunctionInstructionIdx <= maxInstruction)
        {
          dormantIdsToRemove.push_back(id);
          m_LiveVariables[id] = m_DormantVariables[id];
          m_DormantVariables.erase(id);

          // Do not record the change for the resultId of the next instruction
          if(m_State && (id != nextResultId))
          {
            ShaderVariableChange change;
            change.after = m_LiveVariables[id];
            m_State->changes.push_back(change);
          }
        }
      }
    }
    for(const Id &id : dormantIdsToRemove)
    {
      m_Dormant.removeOne(id);
      if(!m_Live.contains(id))
        m_Live.push_back(id);
    }
  }

  // Update the result variable after the dormant variables have been brought back
  RDCASSERT(!(result.name.empty() ^ (resultId == DXIL_INVALID_ID)));
  if(!result.name.empty() && resultId != DXIL_INVALID_ID)
  {
    if(recordChange)
      SetResult(resultId, result, opCode, dxOpCode, eventFlags);

    // Fake Output results won't be in the referencedIds
    RDCASSERT(resultId == m_OutputSSAId || m_FunctionInfo->referencedIds.count(resultId) == 1);

    if(!m_Live.contains(resultId))
      m_Live.push_back(resultId);
    m_LiveVariables[resultId] = result;
  }

  return true;
}

void ThreadState::StepNext(ShaderDebugState *state, DebugAPIWrapper *apiWrapper,
                           const rdcarray<ThreadState> &workgroups)
{
  m_State = state;

  do
  {
    m_GlobalInstructionIdx = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
    if(m_State)
    {
      if(!m_Ended)
        m_State->nextInstruction = m_GlobalInstructionIdx + 1;

      m_State->flags = ShaderEvents::NoEvent;
      m_State->changes.clear();
    }

  } while(!ExecuteInstruction(apiWrapper, workgroups));

  if(m_State && m_Ended)
    --m_State->nextInstruction;

  m_State = NULL;
}

bool ThreadState::GetShaderVariable(const DXIL::Value *dxilValue, Operation op, DXOp dxOpCode,
                                    ShaderVariable &var, bool flushDenormInput) const
{
  var.name.clear();
  var.members.clear();
  var.flags = ShaderVariableFlags::NoFlags;
  var.rows = 1;
  var.columns = 1;
  var.type = ConvertDXILTypeToVarType(dxilValue->type);
  bool flushDenorm = flushDenormInput && OperationFlushing(op, dxOpCode);
  if(var.type == VarType::Double)
    flushDenorm = false;

  RDCASSERT(!flushDenorm || var.type == VarType::Float);
  if(const Constant *c = cast<Constant>(dxilValue))
  {
    if(c->isShaderVal())
    {
      var.value = c->getShaderVal();
      if(flushDenorm)
        var.value.f32v[0] = flush_denorm(var.value.f32v[0]);
      return true;
    }
    else if(c->isLiteral())
    {
      var.value.u64v[0] = c->getU64();
      return true;
    }
    else if(c->isNULL())
    {
      var.value.u64v[0] = 0;
      return true;
    }
    else if(c->isUndef())
    {
      return false;
    }
    else if(c->isData())
    {
      RDCERR("Constant isData DXIL Value not supported");
    }
    else if(c->isCast())
    {
      RDCERR("Constant isCast DXIL Value not supported");
    }
    else if(c->isCompound())
    {
      // TODO: Might be a vector
      if(c->op == Operation::GetElementPtr)
      {
        const rdcarray<DXIL::Value *> &members = c->getMembers();
        const Type *baseType = members.at(0)->type;
        RDCASSERTEQUAL(baseType->type, Type::Pointer);
        ShaderVariable ptrVal;
        RDCASSERT(GetShaderVariable(members.at(0), op, dxOpCode, ptrVal));
        rdcarray<uint64_t> indexes;
        for(size_t i = 1; i < members.size(); i++)
        {
          ShaderVariable index;
          RDCASSERT(GetShaderVariable(members.at(i), op, dxOpCode, index));
          indexes.push_back(index.value.u64v[0]);
        }
        var.value = ptrVal.value;
        // TODO: Need to do the arithmetic with indexes
        return true;
      }
      else if(c->op != Operation::NoOp)
      {
        RDCERR("Constant isCompound DXIL Value with unsupported operaiton %s", ToStr(c->op).c_str());
      }
    }
    else
    {
      RDCERR("Constant DXIL Value with no value");
      return false;
    }
  }
  else if(const Literal *lit = cast<Literal>(dxilValue))
  {
    var.value.u64v[0] = lit->literal;
    return true;
  }
  else if(const GlobalVar *gv = cast<GlobalVar>(dxilValue))
  {
    var.value.u64v[0] = gv->initialiser->getU64();
    return true;
  }

  if(const Instruction *inst = cast<Instruction>(dxilValue))
  {
    GetVariable(inst->slot, op, dxOpCode, var);
    return true;
  }
  RDCERR("Unandled DXIL Value type");

  return false;
}

bool ThreadState::GetVariable(const Id &id, Operation op, DXOp dxOpCode, ShaderVariable &var) const
{
  RDCASSERT(m_Live.contains(id));
  RDCASSERTEQUAL(m_LiveVariables.count(id), 1);
  var = m_LiveVariables.at(id);

  bool flushDenorm = OperationFlushing(op, dxOpCode);
  if(var.type == VarType::Double)
    flushDenorm = false;
  RDCASSERT(!flushDenorm || var.type == VarType::Float);
  if(flushDenorm)
    var.value.f32v[0] = flush_denorm(var.value.f32v[0]);
  return true;
}

void ThreadState::SetResult(const Id &id, ShaderVariable &result, Operation op, DXOp dxOpCode,
                            ShaderEvents flags)
{
  RDCASSERT(result.rows > 0);
  RDCASSERT(result.columns > 0);
  RDCASSERT(result.columns <= 4);
  RDCASSERTNOTEQUAL(result.type, VarType::Unknown);

  // Can only flush denorms for float types
  bool flushDenorm = OperationFlushing(op, dxOpCode) && (result.type == VarType::Float);

  flags |= AssignValue(result, result, flushDenorm);

  if(m_State)
  {
    ShaderVariableChange change;
    m_State->flags |= flags;
    change.before = m_LiveVariables[id];
    change.after = result;
    m_State->changes.push_back(change);
  }
}

void ThreadState::MarkResourceAccess(const rdcstr &name, const DXIL::ResourceReference *resRef)
{
  if(m_State == NULL)
    return;

  ResourceClass resClass = resRef->resourceBase.resClass;
  if(resClass != ResourceClass::UAV && resClass != ResourceClass::SRV)
    return;

  bool isSRV = (resClass == ResourceClass::SRV);
  m_State->changes.push_back(ShaderVariableChange());
  ShaderVariableChange &change = m_State->changes.back();
  change.after.rows = change.after.columns = 1;
  change.after.type = isSRV ? VarType::ReadOnlyResource : VarType::ReadWriteResource;

  const DXIL::EntryPointInterface::ResourceBase &resourceBase = resRef->resourceBase;
  change.after.name = name;
  // TODO: find the array index
  uint32_t arrayIdx = 0;
  if(resourceBase.regCount > 1)
    change.after.name += StringFormat::Fmt("[%u]", arrayIdx);

  change.after.SetBindIndex(ShaderBindIndex(
      isSRV ? DescriptorCategory::ReadOnlyResource : DescriptorCategory::ReadWriteResource,
      resRef->resourceIndex, arrayIdx));

  // Check whether this resource was visited before
  bool found = false;
  ShaderBindIndex bp = change.after.GetBindIndex();
  rdcarray<ShaderBindIndex> &accessed = isSRV ? m_accessedSRVs : m_accessedUAVs;
  for(size_t i = 0; i < accessed.size(); ++i)
  {
    if(accessed[i] == bp)
    {
      found = true;
      break;
    }
  }

  if(found)
    change.before = change.after;
  else
    accessed.push_back(bp);
}

void ThreadState::PerformGPUResourceOp(const rdcarray<ThreadState> &workgroups, Operation opCode,
                                       DXOp dxOpCode, const DXIL::ResourceReference *resRef,
                                       DebugAPIWrapper *apiWrapper, const DXIL::Instruction &inst,
                                       ShaderVariable &result)
{
  // TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  // Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  // SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  // SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)

  // TODO
  // SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  // SampleGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)
  // SampleCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp)
  // SampleCmpLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,lod)
  // SampleCmpGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)
  // SampleCmpBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,bias,clamp)

  // DXIL reports the vector result as a struct of N members of Element type, plus an int.
  const Type *retType = inst.type;
  RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
  const Type *baseType = retType->members[0];
  RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
  result.type = ConvertDXILTypeToVarType(baseType);
  result.columns = (uint8_t)(retType->members.size() - 1);

  // CalculateSampleGather is only valid for SRV resources
  ResourceClass resClass = resRef->resourceBase.resClass;
  RDCASSERTEQUAL(resClass, ResourceClass::SRV);

  // resRef->resourceBase must be an SRV
  const DXIL::EntryPointInterface::SRV &srv = resRef->resourceBase.srvData;

  SampleGatherResourceData resourceData;
  resourceData.dim = (DXDebug::ResourceDimension)ConvertResourceKindToResourceDimension(srv.shape);
  resourceData.retType =
      (DXDebug::ResourceRetType)ConvertComponentTypeToResourceRetType(srv.compType);
  resourceData.sampleCount = srv.sampleCount;

  resourceData.binding.registerSpace = resRef->resourceBase.space;
  resourceData.binding.shaderRegister = resRef->resourceBase.regBase;

  // TODO: SET THIS TO INCLUDE UINT FORMATS
  if(result.type == VarType::Float)
    resourceData.retType = DXBC::RETURN_TYPE_FLOAT;
  else if(result.type == VarType::SInt)
    resourceData.retType = DXBC::RETURN_TYPE_SINT;
  else
    RDCERR("Unhanded return type %s", ToStr(result.type).c_str());

  ShaderVariable uv;
  int8_t texelOffsets[3] = {0, 0, 0};
  int msIndex = 0;
  float lodOrCompareValue = 0.0f;

  SampleGatherSamplerData samplerData = {};
  samplerData.mode = SamplerMode::NUM_SAMPLERS;

  bool uvDDXY[4] = {false, false, false, false};

  if(dxOpCode != DXOp::TextureLoad)
  {
    // Sampler is in arg 2
    rdcstr samplerId = GetArgumentName(2);
    const ResourceReference *samplerRef = GetResource(samplerId);
    if(!samplerRef)
      return;

    RDCASSERTEQUAL(samplerRef->resourceBase.resClass, ResourceClass::Sampler);
    // samplerRef->resourceBase must be a Sampler
    const DXIL::EntryPointInterface::Sampler &sampler = resRef->resourceBase.samplerData;
    // TODO: BIAS COMES FROM THE Sample*Bias arguments
    samplerData.bias = 0.0f;
    samplerData.binding.registerSpace = samplerRef->resourceBase.space;
    samplerData.binding.shaderRegister = samplerRef->resourceBase.regBase;
    samplerData.mode = ConvertSamplerKindToSamplerMode(sampler.samplerType);

    ShaderVariable arg;
    // UV is float data in args 3,4,5,6
    for(uint32_t i = 0; i < 4; ++i)
    {
      if(GetShaderVariable(inst.args[3 + i], opCode, dxOpCode, arg))
      {
        uv.value.f32v[i] = arg.value.f32v[0];
        // variables will have a name, constants will not have a name
        if(!arg.name.empty())
          uvDDXY[i] = true;
      }
    }

    // Offset is int data in args 7,8,9
    if(GetShaderVariable(inst.args[7], opCode, dxOpCode, arg, false))
      texelOffsets[0] = (int8_t)arg.value.s32v[0];
    if(GetShaderVariable(inst.args[8], opCode, dxOpCode, arg, false))
      texelOffsets[1] = (int8_t)arg.value.s32v[0];
    if(GetShaderVariable(inst.args[9], opCode, dxOpCode, arg, false))
      texelOffsets[2] = (int8_t)arg.value.s32v[0];

    // TODO: Sample: Clamp is in arg 10

    // SampleLevel: LOD is in arg 10
    // SampleCmpLevelZero: compare is in arg 10
    if((dxOpCode == DXOp::SampleLevel) || (dxOpCode == DXOp::SampleCmpLevelZero))
    {
      if(GetShaderVariable(inst.args[10], opCode, dxOpCode, arg))
      {
        RDCASSERTEQUAL(arg.type, VarType::Float);
        lodOrCompareValue = arg.value.f32v[0];
      }
    }
  }
  else
  {
    ShaderVariable arg;
    // TODO : mipLevelOrSampleCount is in arg 2
    if(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg))
    {
      msIndex = arg.value.u32v[0];
      lodOrCompareValue = arg.value.f32v[0];
    }

    // UV is int data in args 3,4,5
    if(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg))
      uv.value.s32v[0] = arg.value.s32v[0];
    if(GetShaderVariable(inst.args[4], opCode, dxOpCode, arg))
      uv.value.s32v[1] = arg.value.s32v[0];
    if(GetShaderVariable(inst.args[5], opCode, dxOpCode, arg))
      uv.value.s32v[2] = arg.value.s32v[0];

    // Offset is int data in args 6,7,8
    if(GetShaderVariable(inst.args[6], opCode, dxOpCode, arg))
      texelOffsets[0] = (int8_t)arg.value.s32v[0];
    if(GetShaderVariable(inst.args[7], opCode, dxOpCode, arg))
      texelOffsets[1] = (int8_t)arg.value.s32v[0];
    if(GetShaderVariable(inst.args[8], opCode, dxOpCode, arg))
      texelOffsets[2] = (int8_t)arg.value.s32v[0];
  }

  // TODO: DDX & DDY
  ShaderVariable ddx;
  ShaderVariable ddy;
  // Sample, SampleBias, SampleCmp, CalculateLOD need DDX, DDY
  if((dxOpCode == DXOp::Sample) || (dxOpCode == DXOp::SampleBias) ||
     (dxOpCode == DXOp::SampleCmp) || (dxOpCode == DXOp::CalculateLOD))
  {
    if(m_ShaderType != DXBC::ShaderType::Pixel || workgroups.size() != 4)
    {
      RDCERR("Undefined results using derivative instruction outside of a pixel shader.");
    }
    else
    {
      // texture samples use coarse derivatives
      // TODO: the UV should be the ID per UV compponent
      ShaderValue delta;
      for(uint32_t i = 0; i < 4; i++)
      {
        if(uvDDXY[i])
        {
          delta = DDX(false, opCode, dxOpCode, workgroups, GetArgumentId(3 + i));
          ddx.value.f32v[i] = delta.f32v[0];
          delta = DDY(false, opCode, dxOpCode, workgroups, GetArgumentId(3 + i));
          ddy.value.f32v[i] = delta.f32v[0];
        }
      }
    }
  }
  else if(dxOpCode == DXOp::SampleGrad)
  {
    // TODO: get from arguments
  }

  uint8_t swizzle[4] = {0, 1, 2, 3};

  // TODO: GATHER CHANNEL
  GatherChannel gatherChannel = GatherChannel::Red;
  uint32_t instructionIdx = m_FunctionInstructionIdx - 1;
  const char *opString = ToStr(dxOpCode).c_str();
  ShaderVariable data;

  apiWrapper->CalculateSampleGather(dxOpCode, resourceData, samplerData, uv, ddx, ddy, texelOffsets,
                                    msIndex, lodOrCompareValue, swizzle, gatherChannel,
                                    m_ShaderType, instructionIdx, opString, data);

  result.value = data.value;
}

rdcstr ThreadState::GetArgumentName(uint32_t i) const
{
  return m_Program.GetArgId(*m_CurrentInstruction, i);
}

Id ThreadState::GetArgumentId(uint32_t i) const
{
  DXIL::Value *arg = m_CurrentInstruction->args[i];
  return GetSSAId(arg);
}

const DXIL::ResourceReference *ThreadState::GetResource(rdcstr handle)
{
  const DXIL::ResourceReference *resRef = m_Program.GetResourceReference(handle);
  if(resRef)
  {
    rdcstr alias = m_Program.GetHandleAlias(handle);
    MarkResourceAccess(alias, resRef);
    return resRef;
  }

  RDCERR("Unknown resource handle '%s'", handle.c_str());
  return NULL;
}

void ThreadState::Sub(const ShaderVariable &a, const ShaderVariable &b, ShaderValue &ret) const
{
  RDCASSERTEQUAL(a.type, b.type);
  RDCASSERTEQUAL(a.rows, b.rows);
  RDCASSERTEQUAL(a.columns, b.columns);
  if(a.type == VarType::Float)
    ret.f32v[0] = a.value.f32v[0] - b.value.f32v[0];
  else if(a.type == VarType::SInt)
    ret.s32v[0] = a.value.s32v[0] - b.value.s32v[0];
  else if(a.type == VarType::UInt)
    ret.u32v[0] = a.value.u32v[0] - b.value.u32v[0];
  else
    RDCERR("Unhandled type '%s'", ToStr(a.type).c_str());
}

ShaderValue ThreadState::DDX(bool fine, Operation opCode, DXOp dxOpCode,
                             const rdcarray<ThreadState> &quad, const Id &id) const
{
  uint32_t index = ~0U;
  int quadIndex = m_WorkgroupIndex;

  if(!fine)
  {
    // use top-left pixel's neighbours
    index = 0;
  }
  // find direct neighbours - left pixel in the quad
  else if(quadIndex % 2 == 0)
  {
    index = quadIndex;
  }
  else
  {
    index = quadIndex - 1;
  }

  ShaderValue ret;
  ShaderVariable a;
  ShaderVariable b;
  RDCASSERT(quad[index + 1].GetVariable(id, opCode, dxOpCode, a));
  RDCASSERT(quad[index].GetVariable(id, opCode, dxOpCode, b));
  Sub(a, b, ret);
  return ret;
}

ShaderValue ThreadState::DDY(bool fine, Operation opCode, DXOp dxOpCode,
                             const rdcarray<ThreadState> &quad, const Id &id) const
{
  uint32_t index = ~0U;
  int quadIndex = m_WorkgroupIndex;

  if(!fine)
  {
    // use top-left pixel's neighbours
    index = 0;
  }
  // find direct neighbours - top pixel in the quad
  else if(quadIndex < 2)
  {
    index = quadIndex;
  }
  else
  {
    index = quadIndex - 2;
  }

  ShaderValue ret;
  ShaderVariable a;
  ShaderVariable b;
  RDCASSERT(quad[index + 2].GetVariable(id, opCode, dxOpCode, a));
  RDCASSERT(quad[index].GetVariable(id, opCode, dxOpCode, b));
  Sub(a, b, ret);
  return ret;
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
void Debugger::CalcActiveMask(rdcarray<bool> &activeMask)
{
  // one bool per workgroup thread
  activeMask.resize(m_Workgroups.size());

  // mark any threads that have finished as inactive, otherwise they're active
  for(size_t i = 0; i < m_Workgroups.size(); i++)
    activeMask[i] = !m_Workgroups[i].Finished();

  // only pixel shaders automatically converge workgroups, compute shaders need explicit sync
  if(m_Stage != ShaderStage::Pixel)
    return;

  // TODO: implement pixel shader convergence
  return;
}

size_t Debugger::FindScopedDebugDataIndex(const uint32_t instructionIndex) const
{
  size_t countScopes = m_DebugInfo.scopedDebugDatas.size();
  size_t scopeIndex = countScopes;
  for(size_t i = 0; i < countScopes; i++)
  {
    if((m_DebugInfo.scopedDebugDatas[i].minInstruction <= instructionIndex) &&
       (instructionIndex <= m_DebugInfo.scopedDebugDatas[i].maxInstruction))
      scopeIndex = i;
    else if(scopeIndex < countScopes)
      break;
  }
  return scopeIndex;
}

size_t Debugger::FindScopedDebugDataIndex(const DXIL::Metadata *md) const
{
  size_t countScopes = m_DebugInfo.scopedDebugDatas.size();
  for(size_t i = 0; i < countScopes; i++)
  {
    if(m_DebugInfo.scopedDebugDatas[i].md == md)
      return i;
  }
  return countScopes;
}

size_t Debugger::AddScopedDebugData(const DXIL::Metadata *scopeMD, uint32_t instructionIndex)
{
  // Iterate upwards to find DISubprogram or DIFile scope
  while((scopeMD->dwarf->type != DIBase::File) && (scopeMD->dwarf->type != DIBase::Subprogram))
    scopeMD = m_Program->GetDebugScopeParent(scopeMD->dwarf);

  size_t scopeIndex = FindScopedDebugDataIndex(scopeMD);
  // Add a new DebugScope
  if(scopeIndex == m_DebugInfo.scopedDebugDatas.size())
  {
    // Find the parent scope and add this to its children
    const DXIL::Metadata *parentScope = m_Program->GetDebugScopeParent(scopeMD->dwarf);

    ScopedDebugData scope;
    scope.md = scopeMD;
    scope.minInstruction = instructionIndex;
    scope.maxInstruction = instructionIndex;
    // File scope should not have a parent
    if(scopeMD->dwarf->type != DIBase::File)
    {
      RDCASSERT(parentScope);
      scope.parentIndex = AddScopedDebugData(parentScope, instructionIndex);
      RDCASSERT(scope.parentIndex < m_DebugInfo.scopedDebugDatas.size());
    }
    else
    {
      RDCASSERT(!parentScope);
      scope.parentIndex = (size_t)-1;
    }
    scope.fileName = m_Program->GetDebugScopeFilePath(scope.md->dwarf);
    scope.line = (uint32_t)m_Program->GetDebugScopeLine(scope.md->dwarf);

    m_DebugInfo.scopedDebugDatas.push_back(scope);
  }
  return scopeIndex;
}

void Debugger::AddDebugType(const DXIL::Metadata *typeMD)
{
  TypeData typeData;

  const DXIL::DIBase *base = typeMD->dwarf;
  /*
    rdcstr name;
    VarType type = VarType::Unknown;
    uint32_t vecSize = 0;
    uint32_t matSize = 0;
    bool colMajorMat = false;

    const DXIL::Metadata *baseType = NULL;
    rdcarray<uint32_t> arrayDimensions;
    rdcarray<rdcpair<rdcstr, const DXIL::Metadata *>> structMembers;
    rdcarray<uint32_t> memberOffsets;
  */

  switch(base->type)
  {
    case DXIL::DIBase::Type::BasicType:
    {
      const DIBasicType *basicType = base->As<DIBasicType>();
      typeData.name = *basicType->name;
      typeData.baseType = typeMD;
      typeData.vecSize = 1;
      uint32_t sizeInBits = (uint32_t)basicType->sizeInBits;
      switch(basicType->tag)
      {
        case DW_TAG_array_type:
        {
          // typeData.type = VAR TYPE OF THE ARRAY ELEMENTS
          // typeData.baseType = MD TYPE OF THE ARRAY ELEMENTS
          break;
        }
        case DW_TAG_base_type:
        {
          break;
        }
        default: RDCERR("Unhandled DIBasicType tag %s", ToStr(basicType->tag).c_str()); break;
      }
      switch(basicType->encoding)
      {
        case DW_ATE_boolean:
        {
          RDCASSERTEQUAL(sizeInBits, 8);
          typeData.type = VarType ::Bool;
          break;
        }
        case DW_ATE_float:
        {
          if(sizeInBits == 16)
            typeData.type = VarType::Half;
          else if(sizeInBits == 32)
            typeData.type = VarType::Float;
          else if(sizeInBits == 64)
            typeData.type = VarType::Double;
          else
            RDCERR("Unhandled DIBasicType DW_ATE_float size %u", sizeInBits);
          break;
        }
        case DW_ATE_signed:
        {
          if(sizeInBits == 8)
            typeData.type = VarType::SByte;
          else if(sizeInBits == 16)
            typeData.type = VarType::SShort;
          else if(sizeInBits == 32)
            typeData.type = VarType::SInt;
          else if(sizeInBits == 32)
            typeData.type = VarType::SLong;
          else
            RDCERR("Unhandled DIBasicType DW_ATE_signed size %u", sizeInBits);
          break;
        }
        case DW_ATE_unsigned:
        {
          if(sizeInBits == 8)
            typeData.type = VarType::UByte;
          else if(sizeInBits == 16)
            typeData.type = VarType::UShort;
          else if(sizeInBits == 32)
            typeData.type = VarType::UInt;
          else if(sizeInBits == 32)
            typeData.type = VarType::ULong;
          else
            RDCERR("Unhandled DIBasicType DW_ATE_unsigned size %u", sizeInBits);
          break;
        }
        case DW_ATE_signed_char:
        {
          RDCASSERTEQUAL(sizeInBits, 8);
          typeData.type = VarType::SByte;
          break;
        }
        case DW_ATE_unsigned_char:
        {
          RDCASSERTEQUAL(sizeInBits, 8);
          typeData.type = VarType::UByte;
          break;
        }
        case DW_ATE_complex_float:
        case DW_ATE_address:
        case DW_ATE_imaginary_float:
        case DW_ATE_packed_decimal:
        case DW_ATE_numeric_string:
        case DW_ATE_edited:
        case DW_ATE_signed_fixed:
        case DW_ATE_unsigned_fixed:
        case DW_ATE_decimal_float:
        case DW_ATE_UTF:
          RDCERR("Unhandled DIBasicType encoding %s", ToStr(basicType->encoding).c_str());
          break;
      };
      break;
    }
    case DXIL::DIBase::Type::CompositeType:
    {
      const DICompositeType *compositeType = base->As<DICompositeType>();
      typeData.name = *compositeType->name;
      typeData.baseType = typeMD;
      switch(compositeType->tag)
      {
        case DW_TAG_class_type:
        case DW_TAG_structure_type:
        {
          typeData.type = VarType::Struct;
          // rdcarray<rdcpair<rdcstr, const DXIL::Metadata *>> structMembers;
          // rdcarray<uint32_t> memberOffsets;
          //   AddDebugType(member);
          //   RDCASSERT(m_DebugInfo.types.count(compositeType->base) == 1);
          //   typeData = m_DebugInfo.types[compositeType->base];
          break;
        }
        default:
          RDCERR("Unhandled DICompositeType tag %s", ToStr(compositeType->tag).c_str());
          break;
      };
      break;
    }
    case DXIL::DIBase::Type::DerivedType:
    {
      const DIDerivedType *derviedType = base->As<DIDerivedType>();
      switch(derviedType->tag)
      {
        case DW_TAG_typedef:
          AddDebugType(derviedType->base);
          RDCASSERT(m_DebugInfo.types.count(derviedType->base) == 1);
          typeData = m_DebugInfo.types[derviedType->base];
          break;
        default:
          RDCERR("Unhandled DIDerivedType DIDerviedType Tag type %s",
                 ToStr(derviedType->tag).c_str());
          break;
      }
      break;
    }
    default: RDCERR("Unhandled DXIL type %s", ToStr(base->type).c_str()); break;
  }

  m_DebugInfo.types[typeMD] = typeData;
}

void Debugger::ParseDbgOpDeclare(const DXIL::Instruction &inst, uint32_t instructionIndex)
{
  // arg 0 contains the SSA Id of the alloca result which represents the local variable (a pointer)
  const Metadata *allocaInstMD = cast<Metadata>(inst.args[0]);
  RDCASSERT(allocaInstMD);
  const Instruction *allocaInst = cast<Instruction>(allocaInstMD->value);
  RDCASSERT(allocaInst);
  RDCASSERTEQUAL(allocaInst->op, Operation::Alloca);
  rdcstr resultId;
  Program::MakeResultId(*allocaInst, resultId);

  // arg 1 is DILocalVariable metadata
  // Tag
  // name
  // arguments
  // scope
  // file, line
  // type
  // flags
  const Metadata *localVariableMD = cast<Metadata>(inst.args[1]);
  RDCASSERT(localVariableMD);
  RDCASSERTEQUAL(localVariableMD->dwarf->type, DIBase::Type::LocalVariable);
  const DILocalVariable *localVariable = localVariableMD->dwarf->As<DILocalVariable>();

  // arg 2 is DIExpression metadata
  const Metadata *expressionMD = cast<Metadata>(inst.args[2]);
  uint32_t countBytes = 0;
  if(expressionMD)
  {
    RDCASSERTEQUAL(expressionMD->dwarf->type, DIBase::Type::Expression);
    const DIExpression *expression = expressionMD->dwarf->As<DIExpression>();
    RDCLOG("Expression Op %s", ToStr(expression->op).c_str());
  }

  size_t scopeIndex = AddScopedDebugData(localVariable->scope, instructionIndex);
  ScopedDebugData &scope = m_DebugInfo.scopedDebugDatas[scopeIndex];
  scope.minInstruction = RDCMIN(scope.minInstruction, instructionIndex);
  scope.maxInstruction = RDCMAX(scope.maxInstruction, instructionIndex);

  rdcstr sourceVarName = m_Program->GetDebugVarName(localVariable);
  LocalMapping localMapping;
  localMapping.variable = localVariable;
  localMapping.sourceVarName = sourceVarName;
  localMapping.ssaIdName = resultId;
  localMapping.byteOffset = 0;
  localMapping.countBytes = countBytes;
  localMapping.instIndex = instructionIndex;
  localMapping.isDeclare = true;

  scope.localMappings.push_back(localMapping);

  const DXIL::Metadata *typeMD = localVariable->type;
  if(m_DebugInfo.types.count(typeMD) == 0)
    AddDebugType(typeMD);

  if(m_DebugInfo.locals.count(sourceVarName) == 0)
    m_DebugInfo.locals[sourceVarName] = localMapping;
}

void Debugger::ParseDbgOpValue(const DXIL::Instruction &inst, uint32_t instructionIndex)
{
  // arg 0 is metadata containing the new value
  const Metadata *valueMD = cast<Metadata>(inst.args[0]);
  rdcstr resultId = m_Program->GetArgId(valueMD->value);
  // arg 1 is i64 byte offset in the source variable where the new value is written
  int64_t byteOffset;
  RDCASSERT(getival<int64_t>(inst.args[1], byteOffset));

  // arg 2 is DILocalVariable metadata
  // Tag
  // name
  // arguments
  // scope
  // file, line
  // type
  // flags
  const Metadata *localVariableMD = cast<Metadata>(inst.args[2]);
  RDCASSERT(localVariableMD);
  RDCASSERTEQUAL(localVariableMD->dwarf->type, DIBase::Type::LocalVariable);
  const DILocalVariable *localVariable = localVariableMD->dwarf->As<DILocalVariable>();

  // arg 3 is DIExpression metadata
  uint32_t countBytes = 0;
  const Metadata *expressionMD = cast<Metadata>(inst.args[2]);
  if(expressionMD)
  {
    if(expressionMD->dwarf->type == DIBase::Type::Expression)
    {
      // TODO: get the count bytes from the expression
      const DIExpression *expression = expressionMD->dwarf->As<DIExpression>();
      RDCLOG("Expression Op %s", ToStr(expression->op).c_str());
    }
  }

  size_t scopeIndex = AddScopedDebugData(localVariable->scope, instructionIndex);
  ScopedDebugData &scope = m_DebugInfo.scopedDebugDatas[scopeIndex];
  scope.minInstruction = RDCMIN(scope.minInstruction, instructionIndex);
  scope.maxInstruction = RDCMAX(scope.maxInstruction, instructionIndex);

  byteOffset = 0;
  countBytes = 0;

  rdcstr sourceVarName = m_Program->GetDebugVarName(localVariable);
  LocalMapping localMapping;
  localMapping.variable = localVariable;
  localMapping.sourceVarName = sourceVarName;
  localMapping.ssaIdName = resultId;
  localMapping.byteOffset = byteOffset;
  localMapping.countBytes = countBytes;
  localMapping.instIndex = instructionIndex;
  localMapping.isDeclare = false;

  scope.localMappings.push_back(localMapping);

  const DXIL::Metadata *typeMD = localVariable->type;
  if(m_DebugInfo.types.count(typeMD) == 0)
    AddDebugType(typeMD);

  if(m_DebugInfo.locals.count(sourceVarName) == 0)
    m_DebugInfo.locals[sourceVarName] = localMapping;
}

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
  ThreadState &active = GetActiveLane();

  rdcarray<ShaderDebugState> ret;

  // initialise the first ShaderDebugState if we haven't stepped yet
  if(m_Steps == 0)
  {
    ShaderDebugState initial;

    for(size_t lane = 0; lane < m_Workgroups.size(); lane++)
    {
      ThreadState &thread = m_Workgroups[lane];

      if(lane == m_ActiveLaneIndex)
      {
        thread.EnterEntryPoint(m_EntryPointFunction, &initial);
        // FillCallstack(thread, initial);
        initial.nextInstruction = thread.m_GlobalInstructionIdx;
      }
      else
      {
        thread.EnterEntryPoint(m_EntryPointFunction, NULL);
      }
    }

    // globals won't be filled out by entering the entry point, ensure their change is registered.
    for(const ShaderVariable &v : m_GlobalState.globals)
      initial.changes.push_back({ShaderVariable(), v});

    ret.push_back(std::move(initial));

    m_Steps++;
  }

  // if we've finished, return an empty set to signify that
  if(active.Finished())
    return ret;

  rdcarray<bool> activeMask;

  for(int stepEnd = m_Steps + 100; m_Steps < stepEnd;)
  {
    if(active.Finished())
      break;

    // calculate the current mask of which threads are active
    CalcActiveMask(activeMask);

    // step all active members of the workgroup
    for(size_t lane = 0; lane < m_Workgroups.size(); lane++)
    {
      if(activeMask[lane])
      {
        ThreadState &thread = m_Workgroups[lane];
        if(thread.Finished())
        {
          if(lane == m_ActiveLaneIndex)
            ret.emplace_back();
          continue;
        }

        if(lane == m_ActiveLaneIndex)
        {
          ShaderDebugState state;

          state.stepIndex = m_Steps;
          thread.StepNext(&state, apiWrapper, m_Workgroups);

          ret.push_back(std::move(state));

          m_Steps++;
        }
        else
        {
          thread.StepNext(NULL, apiWrapper, m_Workgroups);
        }
      }
    }
  }
  return ret;
}

const FunctionInfo *Debugger::GetFunctionInfo(const DXIL::Function *function) const
{
  RDCASSERT(m_FunctionInfos.count(function) != 0);
  return &m_FunctionInfos.at(function);
}
};    // namespace DXILDebug
