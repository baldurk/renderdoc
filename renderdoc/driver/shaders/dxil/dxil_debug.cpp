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

#include "dxil_debug.h"
#include "common/formatting.h"
#include "maths/formatpacking.h"
#include "replay/common/var_dispatch_helpers.h"
#include "dxil_controlflow.h"

// normal is not zero, not subnormal, not infinite, not NaN
inline bool RDCISNORMAL(float input)
{
  union
  {
    uint32_t u;
    float f;
  } x;

  x.f = input;

  x.u &= 0x7fffffffU;
  if(x.u < 0x800000U)
    return false;
  if(x.u >= 0x7f800000U)
    return false;

  return true;
}

inline bool RDCISNORMAL(double input)
{
  union
  {
    uint64_t u;
    double f;
  } x;

  x.f = input;

  x.u &= 0x7fffffffffffffffULL;
  if(x.u < 0x80000000000000ULL)
    return false;
  if(x.u >= 0x7ff0000000000000ULL)
    return false;

  return true;
}

using namespace DXIL;
using namespace DXDebug;

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

static void ConvertDXILTypeToShaderVariable(const Type *type, ShaderVariable &var)
{
  switch(type->type)
  {
    case Type::TypeKind::Struct:
    {
      var.rows = 0;
      var.columns = 0;
      var.type = VarType::Struct;
      var.members.resize(type->members.size());
      for(size_t i = 0; i < type->members.size(); i++)
      {
        var.members[i].name = ".member" + ToStr(i);
        ConvertDXILTypeToShaderVariable(type->members[i], var.members[i]);
      }
      break;
    }
    case Type::TypeKind::Vector:
    {
      var.rows = 1;
      var.columns = (uint8_t)type->elemCount;
      var.type = ConvertDXILTypeToVarType(type->inner);
      break;
    }
    case Type::TypeKind::Array:
    {
      var.rows = (uint8_t)type->elemCount;
      var.columns = 1;
      var.type = ConvertDXILTypeToVarType(type->inner);
      var.members.resize(type->elemCount);
      for(size_t i = 0; i < type->elemCount; i++)
      {
        var.members[i].name = "[" + ToStr(i) + "]";
        ConvertDXILTypeToShaderVariable(type->inner, var.members[i]);
      }
      break;
    }
    case Type::TypeKind::Pointer:
    {
      ConvertDXILTypeToShaderVariable(type->inner, var);
      break;
    }
    case Type::TypeKind::Scalar:
    {
      var.rows = 1;
      var.columns = 1;
      var.type = ConvertDXILTypeToVarType(type);
      break;
    }
    default: RDCERR("Unexpected type kind %s", ToStr(type->type).c_str()); break;
  }
}

static bool ConvertDXILConstantToShaderValue(const DXIL::Constant *c, const size_t index,
                                             ShaderValue &value)
{
  if(c->isShaderVal())
  {
    value = c->getShaderVal();
    return true;
  }
  else if(c->isLiteral())
  {
    if(c->type->bitWidth == 64)
      value.u64v[index] = c->getU64();
    else
      value.u32v[index] = c->getU32();
    return true;
  }
  else if(c->isNULL())
  {
    if(c->type->bitWidth == 64)
      value.u64v[index] = 0;
    else
      value.u32v[index] = 0;
    return true;
  }
  else if(c->isUndef())
  {
    if(c->op == Operation::NoOp)
    {
      if(c->type->bitWidth == 64)
        value.u64v[index] = 0;
      else
        value.u32v[index] = 0;
      return true;
    }
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
    RDCERR("Constant isCompound DXIL Value not supported");
  }
  else
  {
    RDCERR("Constant DXIL Value with no value");
  }
  return false;
}

static bool ConvertDXILValueToShaderValue(const DXIL::Value *v, const VarType varType,
                                          const size_t index, ShaderValue &value)
{
  if(const Constant *c = cast<Constant>(v))
  {
    return ConvertDXILConstantToShaderValue(c, index, value);
  }
  else if(const Literal *lit = cast<Literal>(v))
  {
    switch(varType)
    {
      case VarType::ULong: value.u64v[index] = lit->literal; break;
      case VarType::SLong: value.s64v[index] = (int64_t)lit->literal; break;
      case VarType::UInt: value.u32v[index] = (uint32_t)lit->literal; break;
      case VarType::SInt: value.s32v[index] = (int32_t)lit->literal; break;
      case VarType::UShort: value.u16v[index] = (uint16_t)lit->literal; break;
      case VarType::SShort: value.s16v[index] = (int16_t)lit->literal; break;
      case VarType::UByte: value.u8v[index] = (uint8_t)lit->literal; break;
      case VarType::SByte: value.s8v[index] = (int8_t)lit->literal; break;
      case VarType::Float: value.u32v[index] = (uint32_t)lit->literal; break;
      case VarType::Double: value.u64v[index] = lit->literal; break;
      case VarType::Bool: value.u32v[index] = lit->literal ? 1 : 0; break;
      case VarType::Half: value.u16v[index] = (uint16_t)lit->literal; break;
      case VarType::Enum: value.u32v[index] = (uint32_t)lit->literal; break;
      case VarType::GPUPointer:
      case VarType::ConstantBlock:
      case VarType::ReadOnlyResource:
      case VarType::ReadWriteResource:
      case VarType::Sampler:
      case VarType::Struct:
      case VarType::Unknown: RDCERR("Unhandled VarType %s", ToStr(varType).c_str()); return false;
    }
    return true;
  }
  RDCERR("Unexpected DXIL Value type %s", ToStr(v->kind()).c_str());
  return false;
}

static bool ConvertDXILConstantToShaderVariable(const Constant *constant, ShaderVariable &var)
{
  // Vector: rows == 1, columns >= 1 : var.members is empty
  // Scalar: rows = 1, columns = 1 : var.members is empty
  if(var.members.empty())
  {
    RDCASSERTEQUAL(var.rows, 1);
    RDCASSERT(var.columns >= 1);
    if(var.columns > 1)
    {
      if(constant->isCompound())
      {
        const rdcarray<DXIL::Value *> &members = constant->getMembers();
        for(size_t i = 0; i < members.size(); ++i)
          RDCASSERT(ConvertDXILValueToShaderValue(members[i], var.type, i, var.value));
      }
      return true;
    }
    else if(var.columns == 1)
    {
      const DXIL::Value *value = constant;
      if(constant->isCompound())
      {
        const rdcarray<DXIL::Value *> &members = constant->getMembers();
        value = members[0];
      }
      RDCASSERT(ConvertDXILValueToShaderValue(value, var.type, 0, var.value));
      return true;
    }
    return false;
  }
  // Struct: rows = 0, columns = 0 : var.members is structure members
  // Array: rows >= 1, columns == 1 : var.members is array elements
  const rdcarray<DXIL::Value *> &members = constant->getMembers();
  RDCASSERT(members.size() == var.members.size());
  for(size_t i = 0; i < var.members.size(); ++i)
  {
    const Constant *c = cast<Constant>(members[i]);
    if(c)
      RDCASSERT(ConvertDXILConstantToShaderVariable(c, var.members[i]));
    else
      RDCASSERT(
          ConvertDXILValueToShaderValue(members[i], var.members[i].type, 0, var.members[i].value));
  }
  return true;
}

size_t ComputeDXILTypeByteSize(const Type *type)
{
  // TODO: byte alignment
  size_t byteSize = 0;
  switch(type->type)
  {
    case Type::TypeKind::Struct:
    {
      for(size_t i = 0; i < type->members.size(); i++)
      {
        byteSize += ComputeDXILTypeByteSize(type->members[i]);
      }
      break;
    }
    case Type::TypeKind::Vector:
    {
      byteSize += type->elemCount * ComputeDXILTypeByteSize(type->inner);
      break;
    }
    case Type::TypeKind::Array:
    {
      byteSize += type->elemCount * ComputeDXILTypeByteSize(type->inner);
      break;
    }
    case Type::TypeKind::Pointer:
    {
      byteSize += ComputeDXILTypeByteSize(type->inner);
      break;
    }
    case Type::TypeKind::Scalar:
    {
      byteSize += type->bitWidth / 8;
      break;
    }
    default: RDCERR("Unexpected type kind %s", ToStr(type->type).c_str()); break;
  }
  return byteSize;
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

void ConvertTypeToViewFormat(const DXIL::Type *type, DXILDebug::GlobalState::ViewFmt &fmt)
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

  // find the inner type of any arrays
  while(resType->type == Type::Array)
    resType = resType->inner;

  uint32_t compCount = 1;
  // get the inner type for a vector
  if(resType->type == Type::Vector)
  {
    compCount = resType->elemCount;
    resType = resType->inner;
  }

  fmt.fmt = CompType::Typeless;
  if(resType->type == Type::Scalar)
  {
    fmt.numComps = compCount;
    fmt.byteWidth = resType->bitWidth / 8;
    fmt.stride = fmt.byteWidth * fmt.numComps;
    if(resType->scalarType == Type::ScalarKind::Int)
    {
      if(resType->bitWidth == 32)
        fmt.fmt = CompType::SInt;
    }
    else if(resType->scalarType == Type::ScalarKind::Float)
    {
      if(resType->bitWidth == 32)
        fmt.fmt = CompType::Float;
    }
  }
  else if(resType->type == Type::Struct)
  {
    fmt.numComps = 0;
    fmt.byteWidth = 0;
    fmt.stride = 0;
  }
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
    if(!psInput.included)
      continue;

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
    if(!psInput.included)
      continue;
    const int input = psInput.input;
    const int numWords = psInput.numwords;
    if(destIdx == 0)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, 1.0f, 2, 3);
    else if(destIdx == 1)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, 1.0f, 2, -1);
    else if(destIdx == 2)
      ApplyDerivatives(global, quad, input, numWords, ddy_coarse, -1.0f, 0, 1);

    ddy_coarse += numWords;
  }

  float *ddxfine = ddy_coarse;

  for(const PSInputData &psInput : psInputs)
  {
    if(!psInput.included)
      continue;
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
    if(!psInput.included)
      continue;
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
  for(auto it : m_MemoryAllocs)
    free(it.second.backingMemory);
}

void ThreadState::InitialiseHelper(const ThreadState &activeState)
{
  m_Input = activeState.m_Input;
  m_Semantics = activeState.m_Semantics;
  m_Variables = activeState.m_Variables;
}

bool ThreadState::Finished() const
{
  return m_Killed || m_Ended || m_Callstack.empty();
}

bool ThreadState::InUniformBlock() const
{
  return m_FunctionInfo->uniformBlocks.contains(m_Block);
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

    m_State->changes.push_back({m_Variables[id]});
  }

  for(const Id &id : newLive)
  {
    if(liveGlobals.contains(id))
      continue;

    m_State->changes.push_back({ShaderVariable(), m_Variables[id]});
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
  StepOverNopInstructions();
}

void ThreadState::EnterEntryPoint(const Function *function, ShaderDebugState *state)
{
  m_State = state;

  EnterFunction(function, {});

  for(const GlobalVariable &gv : m_GlobalState.globals)
    m_Variables[gv.id] = gv.var;

  m_State = NULL;
}

bool IsNopInstruction(const Instruction &inst)
{
  if(inst.op == Operation::Call)
  {
    const Function *callFunc = inst.getFuncCall();
    if(callFunc->family == FunctionFamily::LLVMDbg)
      return true;
  }

  if(IsDXCNop(inst))
    return true;

  if(inst.op == Operation::NoOp)
    return true;

  return false;
}

bool ThreadState::ExecuteInstruction(DebugAPIWrapper *apiWrapper,
                                     const rdcarray<ThreadState> &workgroups)
{
  m_CurrentInstruction = m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
  const Instruction &inst = *m_CurrentInstruction;
  m_FunctionInstructionIdx++;

  RDCASSERT(!IsNopInstruction(inst));

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
            uint32_t rowIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            uint32_t colIdx = arg.value.u32v[0];
            const ShaderVariable &a = m_Input.members[inputIdx];
            RDCASSERT(rowIdx < a.rows, rowIdx, a.rows);
            RDCASSERT(colIdx < a.columns, colIdx, a.columns);
            const uint32_t c = a.ColMajor() ? rowIdx * a.columns + colIdx : colIdx * a.rows + rowIdx;

#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, 0) = comp<I>(a, c)

            IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);

#undef _IMPL
#define _IMPL(T) comp<T>(result, 0) = comp<T>(a, c)
            IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, result.type);
            break;
          }
          case DXOp::StoreOutput:
          {
            // StoreOutput(outputSigId,rowIndex,colIndex,value)
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            uint32_t outputIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            uint32_t rowIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            uint32_t colIdx = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[4], opCode, dxOpCode, arg));

            // Only the active lane stores outputs
            if(m_State)
            {
              ShaderVariable &a = m_Output.var.members[outputIdx];
              RDCASSERT(rowIdx < a.rows, rowIdx, a.rows);
              RDCASSERT(colIdx < a.columns, colIdx, a.columns);
              const uint32_t c =
                  a.ColMajor() ? rowIdx * a.columns + colIdx : colIdx * a.rows + rowIdx;
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(a, c) = comp<I>(arg, 0)

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(T) comp<T>(a, c) = comp<T>(arg, 0)
              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);

              result = m_Output.var;
              resultId = m_Output.id;
            }
            else
            {
              resultId = DXILDebug::INVALID_ID;
              result.name.clear();
            }
            break;
          }
          case DXOp::GetDimensions:
          {
            // GetDimensions(handle,mipLevel)
            Id handleId = GetArgumentId(1);
            ShaderBindIndex bindIndex;
            bool annotatedHandle;
            const DXIL::ResourceReference *resRef = GetResource(handleId, bindIndex, annotatedHandle);
            if(!resRef)
              break;

            BindingSlot binding(resRef->resourceBase.regBase + bindIndex.arrayElement,
                                resRef->resourceBase.space);
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
            Id handleId = GetArgumentId(1);
            ShaderBindIndex bindIndex;
            bool annotatedHandle;
            const DXIL::ResourceReference *resRef = GetResource(handleId, bindIndex, annotatedHandle);
            if(!resRef)
              break;

            BindingSlot binding(resRef->resourceBase.regBase + bindIndex.arrayElement,
                                resRef->resourceBase.space);
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
          {
            Id handleId = GetArgumentId(1);
            ShaderBindIndex bindIndex;
            bool annotatedHandle;
            const DXIL::ResourceReference *resRef = GetResource(handleId, bindIndex, annotatedHandle);
            if(!resRef)
              break;

            PerformGPUResourceOp(workgroups, opCode, dxOpCode, resRef, bindIndex, apiWrapper, inst,
                                 result);
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
            const Id handleId = GetArgumentId(1);
            bool annotatedHandle;
            ShaderBindIndex bindIndex;
            const DXIL::ResourceReference *resRef = GetResource(handleId, bindIndex, annotatedHandle);
            if(!resRef)
              break;

            ResourceClass resClass = resRef->resourceBase.resClass;
            // SRV TextureLoad is done on the GPU
            if((dxOpCode == DXOp::TextureLoad) && (resClass == ResourceClass::SRV))
            {
              PerformGPUResourceOp(workgroups, opCode, dxOpCode, resRef, bindIndex, apiWrapper,
                                   inst, result);
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
              // Default for BufferLoad, BufferStore
              // ByteAddressBuffer will have a stride of one
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
                stride = fmt.Stride();
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

            // Update the format if it is Typeless
            // See FetchUAV() comment about root buffers being typeless
            if(fmt.fmt == CompType::Typeless)
            {
              FillViewFmt(result.type, fmt);
              fmt.numComps = result.columns;
              if(fmt.stride == 0)
              {
                fmt.stride = 1;
                stride = 1;
              }
              else
              {
                byteAddress = false;
                stride = fmt.stride;
              }
            }
            else
            {
              if(fmt.stride == 0)
                fmt.stride = fmt.Stride();
              stride = fmt.stride;
            }

            if(stride == 1)
              byteAddress = true;

            if(annotatedHandle)
            {
              auto it = m_AnnotatedProperties.find(handleId);
              RDCASSERT(it != m_AnnotatedProperties.end());
              const AnnotationProperties &props = m_AnnotatedProperties.at(handleId);
              if((props.resKind == ResourceKind::StructuredBuffer) ||
                 (props.resKind == ResourceKind::StructuredBufferWithCounter))
              {
                fmt.stride = props.structStride;
                byteAddress = false;
              }
              stride = fmt.stride;
              RDCASSERTNOTEQUAL(stride, 0);
            }

            RDCASSERTNOTEQUAL(stride, 0);
            RDCASSERTNOTEQUAL(fmt.fmt, CompType::Typeless);

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

            // buffer offsets are in bytes
            // firstElement/numElements is in format-sized units. Convert to byte offsets
            if(byteAddress)
            {
              // For byte address buffer
              // element index is in bytes and a multiple of four, GPU behaviour seems to be to round down
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
            result.name.clear();
            uint32_t resIndexArgId = ~0U;
            if(dxOpCode == DXOp::AnnotateHandle)
              baseResource = GetArgumentName(1);
            else if(dxOpCode == DXOp::CreateHandle)
              resIndexArgId = 3;
            else if(dxOpCode == DXOp::CreateHandleFromBinding)
              resIndexArgId = 2;
            else
              RDCERR("Unhandled DXOp %s", ToStr(dxOpCode).c_str());

            const ResourceReference *resRef = m_Program.GetResourceReference(resultId);
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
              result.name.clear();
              for(uint32_t i = 0; i < resources.size(); ++i)
              {
                if(resources[i].name == resName)
                {
                  result = resources[i];
                  break;
                }
              }
              recordChange = false;
              if(result.name.isEmpty())
              {
                if(resIndexArgId < inst.args.size())
                {
                  // Make the ShaderVariable to represent the dynamic binding
                  // The base binding exists : array index is in argument "resIndexArgId"
                  ShaderVariable arg;
                  RDCASSERT(GetShaderVariable(inst.args[resIndexArgId], opCode, dxOpCode, arg));
                  uint32_t arrayIndex = arg.value.u32v[0];
                  bool isSRV = (resRef->resourceBase.resClass == ResourceClass::SRV);
                  DescriptorCategory category = isSRV ? DescriptorCategory::ReadOnlyResource
                                                      : DescriptorCategory::ReadWriteResource;
                  result.SetBindIndex(ShaderBindIndex(category, resRef->resourceIndex, arrayIndex));
                  result.name = baseResource;
                }
                else
                {
                  RDCERR("Unhandled dynamic handle %s with invalid resIndexArgId", resName.c_str(),
                         resIndexArgId);
                }
              }
              // Default to unannotated handle
              result.value.u32v[3] = 0;

              // Parse the packed annotate handle properties
              // resKind : {compType, compCount} | {structStride}
              if(dxOpCode == DXOp::AnnotateHandle)
              {
                // Set as an annotated handle
                result.value.u32v[3] = 1;
                ShaderVariable props;
                RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, props));
                uint32_t packedProps[2] = {};
                packedProps[0] = props.members[0].value.u32v[0];
                packedProps[1] = props.members[1].value.u32v[0];
                bool uav = (packedProps[0] & (1 << 12)) != 0;
                ResourceKind resKind = (ResourceKind)(packedProps[0] & 0xFF);
                ResourceClass resClass;
                if(resKind == ResourceKind::Sampler)
                  resClass = ResourceClass::Sampler;
                else if(resKind == ResourceKind::CBuffer)
                  resClass = ResourceClass::CBuffer;
                else if(uav)
                  resClass = ResourceClass::UAV;
                else
                  resClass = ResourceClass::SRV;

                uint32_t structStride = 0;
                if((resKind == ResourceKind::StructuredBuffer) ||
                   (resKind == ResourceKind::StructuredBufferWithCounter))
                {
                  structStride = packedProps[1];
                }
                else if(resKind == ResourceKind::Texture1D || resKind == ResourceKind::Texture2D ||
                        resKind == ResourceKind::Texture3D || resKind == ResourceKind::TextureCube ||
                        resKind == ResourceKind::Texture1DArray ||
                        resKind == ResourceKind::Texture2DArray ||
                        resKind == ResourceKind::TextureCubeArray ||
                        resKind == ResourceKind::TypedBuffer || resKind == ResourceKind::Texture2DMS ||
                        resKind == ResourceKind::Texture2DMSArray)
                {
                  ComponentType dxilCompType = ComponentType(packedProps[1] & 0xFF);
                  VarType compType = VarTypeForComponentType(dxilCompType);
                  uint32_t compCount = (packedProps[1] & 0xFF00) >> 8;
                  uint32_t byteWidth = GetElementByteSize(compType);
                  structStride = compCount * byteWidth;
                }
                // Store the annotate properties for the result
                auto it = m_AnnotatedProperties.find(resultId);
                if(it == m_AnnotatedProperties.end())
                {
                  m_AnnotatedProperties[resultId] = {resKind, resClass, structStride};
                }
                else
                {
                  const AnnotationProperties &existingProps = it->second;
                  RDCASSERTEQUAL(existingProps.resKind, resKind);
                  RDCASSERTEQUAL(existingProps.resClass, resClass);
                  RDCASSERTEQUAL(existingProps.structStride, structStride);
                }
              }
            }
            else
            {
              RDCERR("Unknown Base Resource %s", baseResource.c_str());
            }
            break;
          }
          case DXOp::CBufferLoadLegacy:
          {
            // CBufferLoadLegacy(handle,regIndex)
            Id handleId = GetArgumentId(1);

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            uint32_t regIndex = arg.value.u32v[0];

            RDCASSERT(m_Live.contains(handleId));
            auto itVar = m_Variables.find(handleId);
            RDCASSERT(itVar != m_Variables.end());
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
            RDCASSERTEQUAL(arg.rows, 1);
            RDCASSERTEQUAL(arg.columns, 1);
            const uint32_t c = 0;
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = comp<T>(arg, c) - floor(comp<T>(arg, c));

            IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
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
            RDCASSERTEQUAL(arg.rows, 1);
            RDCASSERTEQUAL(arg.columns, 1);
            const uint32_t c = 0;
            if(dxOpCode == DXOp::Round_pi)
            {
              // Round_pi(value) : positive infinity -> ceil()
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = ceil(comp<T>(arg, c));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            else if(dxOpCode == DXOp::Round_ne)
            {
              // Round_ne(value) : to nearest even int (banker's rounding)
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = round_ne(comp<T>(arg, c));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            else if(dxOpCode == DXOp::Round_ni)
            {
              // Round_ni(value) : negative infinity -> floor()
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = floor(comp<T>(arg, c));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            else if(dxOpCode == DXOp::Round_z)
            {
              // Round_z(value) : towards zero
#undef _IMPL
#define _IMPL(T) \
  comp<T>(result, c) = comp<T>(arg, c) < 0.0 ? ceil(comp<T>(arg, c)) : floor(comp<T>(arg, c));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
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
              RDCASSERT(!ThreadsAreDiverged(workgroups));
              Id id = GetArgumentId(1);
              if(dxOpCode == DXOp::DerivCoarseX)
                result.value = DDX(false, opCode, dxOpCode, workgroups, id);
              else
                result.value = DDY(false, opCode, dxOpCode, workgroups, id);
            }
            break;
          }
          case DXOp::IsNaN:
          case DXOp::IsInf:
          case DXOp::IsFinite:
          case DXOp::IsNormal:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.rows, 1);
            RDCASSERTEQUAL(arg.columns, 1);
            const uint32_t c = 0;
            if(dxOpCode == DXOp::IsNaN)
            {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = RDCISNAN(comp<T>(arg, c)) ? 1 : 0

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            else if(dxOpCode == DXOp::IsInf)
            {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = RDCISINF(comp<T>(arg, c)) ? 1 : 0

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            else if(dxOpCode == DXOp::IsFinite)
            {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = RDCISFINITE(comp<T>(arg, c)) ? 1 : 0

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            else if(dxOpCode == DXOp::IsNormal)
            {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = RDCISNORMAL(comp<T>(arg, c)) ? 1 : 0

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
            }
            break;
          }
          case DXOp::Bfrev:
          case DXOp::Countbits:
          {
            ShaderVariable arg;
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERTEQUAL(arg.rows, 1);
            RDCASSERTEQUAL(arg.columns, 1);

            if(dxOpCode == DXOp::Bfrev)
              result.value.u32v[0] = BitwiseReverseLSB16(arg.value.u32v[0]);
            else if(dxOpCode == DXOp::Countbits)
              result.value.u32v[0] = PopCount(arg.value.u32v[0]);
            break;
          }
          case DXOp::IMul:
          case DXOp::UMul:
          case DXOp::UDiv:
          {
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, b.type);
            const uint32_t c = 0;

            if(dxOpCode == DXOp::IMul)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, c) = comp<I>(a, c) * comp<I>(b, c)

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
            }
            else if(dxOpCode == DXOp::UMul)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) * comp<U>(b, c)

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
            }
            else if(dxOpCode == DXOp::UDiv)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) / comp<U>(b, c)

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
            }
            break;
          }
          case DXOp::Barrier:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            BarrierMode barrierMode = (BarrierMode)arg.value.u32v[0];
            // For thread barriers the threads must be converged
            if(barrierMode & BarrierMode::SyncThreadGroup)
              RDCASSERT(!ThreadsAreDiverged(workgroups));
            break;
          }
          case DXOp::Discard:
          {
            ShaderVariable cond;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, cond));
            if(cond.value.u32v[0] != 0)
            {
              m_Killed = true;
              return true;
            }
            break;
          }
          case DXOp::TempRegLoad:
          case DXOp::TempRegStore:
          case DXOp::MinPrecXRegLoad:
          case DXOp::MinPrecXRegStore:
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
          case DXOp::BufferUpdateCounter:
          case DXOp::CheckAccessFullyMapped:
          case DXOp::AtomicBinOp:
          case DXOp::AtomicCompareExchange:
          case DXOp::CalculateLOD:
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
          case DXOp::StartInstanceLocation:
          case DXOp::NumOpCodes:
            RDCERR("Unhandled dx.op method `%s` %s", callFunc->name.c_str(), ToStr(dxOpCode).c_str());
            break;
        }
      }
      else if(callFunc->family == FunctionFamily::LLVMDbg)
      {
        RDCERR("LLVMDbg Instructions should not be executed %s", callFunc->name.c_str());
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
    case Operation::NoOp: RDCERR("NoOp instructions should not be executed"); return false;
    case Operation::Unreachable:
    {
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
      Id src = GetArgumentId(0);
      const ShaderVariable &srcVal = m_Variables[src];
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
    case Operation::LoadAtomic:
    {
      // Load(ptr)
      Id ptrId = GetArgumentId(0);
      RDCASSERT(m_MemoryAllocPointers.count(ptrId) == 1);
      ShaderVariable arg;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, arg));
      result.value = arg.value;
      break;
    }
    case Operation::Store:
    case Operation::StoreAtomic:
    {
      // Store(ptr, value)
      Id baseMemoryId = DXILDebug::INVALID_ID;
      void *baseMemoryBackingPtr = NULL;
      size_t allocSize = 0;
      void *allocMemoryBackingPtr = NULL;
      Id ptrId = GetArgumentId(0);
      auto itPtr = m_MemoryAllocPointers.find(ptrId);
      RDCASSERT(itPtr != m_MemoryAllocPointers.end());

      const MemoryAllocPointer &ptr = itPtr->second;
      baseMemoryId = ptr.baseMemoryId;
      baseMemoryBackingPtr = ptr.backingMemory;

      auto itAlloc = m_MemoryAllocs.find(baseMemoryId);
      RDCASSERT(itAlloc != m_MemoryAllocs.end());
      const MemoryAlloc &alloc = itAlloc->second;
      allocSize = alloc.size;
      allocMemoryBackingPtr = alloc.backingMemory;

      RDCASSERT(baseMemoryBackingPtr);
      RDCASSERTNOTEQUAL(baseMemoryId, DXILDebug::INVALID_ID);

      ShaderVariable val;
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, val));
      RDCASSERTEQUAL(resultId, DXILDebug::INVALID_ID);

      UpdateBackingMemoryFromVariable(baseMemoryBackingPtr, allocSize, val);

      ShaderVariableChange change;
      change.before = m_Variables[baseMemoryId];

      UpdateMemoryVariableFromBackingMemory(baseMemoryId, allocMemoryBackingPtr);

      // record the change to the base memory variable
      change.after = m_Variables[baseMemoryId];
      if(m_State)
        m_State->changes.push_back(change);

      // Update the ptr variable value
      // Set the result to be the ptr variable which will then be recorded as a change
      resultId = ptrId;
      result = m_Variables[resultId];
      result.value = val.value;
      break;
    }
    case Operation::Alloca:
    {
      result.name = DXBC::BasicDemangle(result.name);
      AllocateMemoryForType(inst.type, resultId, result);
      break;
    }
    case Operation::GetElementPtr:
    {
      const DXIL::Type *resultType = inst.type->inner;
      Id ptrId = GetArgumentId(0);

      RDCASSERT(m_MemoryAllocs.count(ptrId) == 1);
      RDCASSERT(m_Variables.count(ptrId) == 1);

      // arg[1..] : indices 1...N
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
      const ShaderVariable &basePtr = m_Variables[ptrId];
      if(indexes.size() > 1)
        offset += indexes[1] * GetElementByteSize(basePtr.type);
      RDCASSERT(indexes.size() <= 2);

      VarType baseType = ConvertDXILTypeToVarType(resultType);
      RDCASSERTNOTEQUAL(resultType->type, DXIL::Type::TypeKind::Struct);
      RDCASSERTEQUAL(resultType->type, DXIL::Type::TypeKind::Scalar);

      uint32_t countElems = RDCMAX(1U, resultType->elemCount);
      size_t size = countElems * GetElementByteSize(baseType);

      // Copy from the backing memory to the result
      MemoryAlloc &alloc = m_MemoryAllocs[ptrId];
      uint8_t *backingMemory = (uint8_t *)alloc.backingMemory;

      result.type = baseType;
      result.rows = (uint8_t)countElems;
      backingMemory += offset;
      m_MemoryAllocPointers[resultId] = {ptrId, backingMemory, size};

      RDCASSERT(offset + size <= alloc.size);
      RDCASSERT(size < sizeof(result.value.f32v));
      memcpy(&result.value.f32v[0], backingMemory, size);
      break;
    }
    case Operation::Bitcast:
    {
      RDCASSERTEQUAL(retType->bitWidth, inst.args[0]->type->bitWidth);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      result.value = a.value;
      break;
    }
    case Operation::Add:
    case Operation::Sub:
    case Operation::Mul:
    case Operation::UDiv:
    case Operation::SDiv:
    case Operation::URem:
    case Operation::SRem:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      RDCASSERTEQUAL(a.type, b.type);
      const uint32_t c = 0;

      if(opCode == Operation::Add)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, c) = comp<I>(a, c) + comp<I>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::Sub)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, c) = comp<I>(a, c) - comp<I>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::Mul)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, c) = comp<I>(a, c) * comp<I>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::UDiv)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) / comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SDiv)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) / comp<S>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::URem)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) % comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SRem)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) % comp<S>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
      break;
    }
    case Operation::FAdd:
    case Operation::FSub:
    case Operation::FMul:
    case Operation::FDiv:
    case Operation::FRem:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      RDCASSERTEQUAL(a.type, b.type);
      const uint32_t c = 0;

      if(opCode == Operation::FAdd)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = comp<T>(a, c) + comp<T>(b, c);

        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::FSub)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = comp<T>(a, c) - comp<T>(b, c);

        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::FMul)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = comp<T>(a, c) * comp<T>(b, c);

        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::FDiv)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = comp<T>(a, c) / comp<T>(b, c);

        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::FRem)
      {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = fmod(comp<T>(a, c), comp<T>(b, c));

        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
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
    case Operation::FOrdTrue:
    case Operation::FUnord:
    case Operation::FUnordEqual:
    case Operation::FUnordGreater:
    case Operation::FUnordGreaterEqual:
    case Operation::FUnordLess:
    case Operation::FUnordLessEqual:
    case Operation::FUnordNotEqual:
    {
      RDCASSERTEQUAL(result.type, VarType::Bool);

      if(opCode == Operation::FOrdFalse)
        result.value.u32v[0] = 0;
      else if(opCode == Operation::FOrdTrue)
        result.value.u32v[0] = 1;
      else
      {
        RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
        RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
        RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
        RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
        ShaderVariable a;
        ShaderVariable b;
        RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
        RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
        RDCASSERTEQUAL(a.type, b.type);
        const uint32_t c = 0;

        // FOrd are all floating-point comparison where both operands are guaranteed to be ordered
        // Using normal comparison operators will give the correct result
        if(opCode == Operation::FOrdEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) == comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FOrdGreater)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) > comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FOrdGreaterEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) >= comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FOrdLess)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) < comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FOrdLessEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) <= comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FOrdNotEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) != comp<T>(b, c)) ? 1 : 0

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FOrd)
        {
          // Both operands are ordered (not NaN)
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = !RDCISNAN(comp<T>(a, c)) && !RDCISNAN(comp<T>(b, c));

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        // FUnord are all floating-point comparison where any operands may be unordered
        // Any comparison with unordered comparisons will return false. Since we want
        // 'or are unordered' then we want to negate the comparison so that unordered comparisons
        // will always return true. So we negate and invert the actual comparison so that the
        // comparison will be unchanged effectively.
        else if(opCode == Operation::FUnord)
        {
          // Either operand is unordered (NaN)
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = RDCISNAN(comp<T>(a, c)) || RDCISNAN(comp<T>(b, c));

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FUnordEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) != comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FUnordGreater)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) <= comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FUnordGreaterEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) < comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FUnordLess)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) >= comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FUnordLessEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) > comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else if(opCode == Operation::FUnordNotEqual)
        {
#undef _IMPL
#define _IMPL(T) comp<uint32_t>(result, c) = (comp<T>(a, c) == comp<T>(b, c)) ? 0 : 1

          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
        }
        else
        {
          RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
        }
      }
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
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
      ShaderVariable a;
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      RDCASSERTEQUAL(a.type, b.type);
      const uint32_t c = 0;

      if(opCode == Operation::IEqual)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, c) = (comp<I>(a, c) == comp<I>(b, c)) ? 1 : 0;

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::INotEqual)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, c) = (comp<I>(a, c) != comp<I>(b, c)) ? 1 : 0;

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::UGreater)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) > comp<U>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::UGreaterEqual)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) >= comp<U>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::ULess)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) < comp<U>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::ULessEqual)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) <= comp<U>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SGreater)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) > comp<S>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SGreaterEqual)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) >= comp<S>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SLess)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) < comp<S>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SLessEqual)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) <= comp<S>(b, c) ? 1 : 0

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
      break;
    }
    case Operation::FToS:
    case Operation::FToU:
    case Operation::SToF:
    case Operation::UToF:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      const uint32_t c = 0;

      if(opCode == Operation::FToS)
      {
        RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
        double x = 0.0;
#undef _IMPL
#define _IMPL(T) x = comp<T>(a, c);
        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = (S)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::FToU)
      {
        RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
        double x = 0.0;

#undef _IMPL
#define _IMPL(T) x = comp<T>(a, c);
        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = (U)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::SToF)
      {
        RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
        int64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<S>(a, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

        if(result.type == VarType::Float)
          comp<float>(result, c) = (float)x;
        else if(result.type == VarType::Half)
          comp<half_float::half>(result, c) = (float)x;
        else if(result.type == VarType::Double)
          comp<double>(result, c) = (double)x;
      }
      else if(opCode == Operation::UToF)
      {
        RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
        // Need to handle this case, cast to unsigned at the width of the argument
        //_Y = uitofp i8 -1 to double; yields double : 255.0
        uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(a, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

        if(result.type == VarType::Float)
          comp<float>(result, c) = (float)x;
        else if(result.type == VarType::Half)
          comp<half_float::half>(result, c) = (float)x;
        else if(result.type == VarType::Double)
          comp<double>(result, c) = (double)x;
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
      break;
    }
    case Operation::Trunc:
    case Operation::ZExt:
    case Operation::SExt:
    {
      // Result & Value must be Integer
      const uint32_t srcBitWidth = inst.args[0]->type->bitWidth;
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, Type::Int);

      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      const uint32_t c = 0;

      if(opCode == Operation::Trunc)
      {
        // Result bit_width < Value bit_width
        RDCASSERT(retType->bitWidth < srcBitWidth);

        uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(a, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = (U)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      if(opCode == Operation::ZExt)
      {
        // Result bit_width >= Value bit_width
        RDCASSERT(retType->bitWidth >= srcBitWidth);
        // Extras bits are 0's
        // %X = zext i32 257 to i64; yields i64 : 257
        uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(a, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = (U)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::SExt)
      {
        // Result bit_width >= Value bit_width
        RDCASSERT(retType->bitWidth >= srcBitWidth);
        // Sign Extend : copy sign (highest bit of Value) -> Result
        // %X = sext i8  -1 to i16              ; yields i16   :65535
        int64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<S>(a, c);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = (S)x;
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
      break;
    }
    case Operation::FPTrunc:
    case Operation::FPExt:
    {
      // Result & Value must be Float
      const uint32_t srcBitWidth = inst.args[0]->type->bitWidth;
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Float);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, Type::Float);

      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      const uint32_t c = 0;

      if(opCode == Operation::FPTrunc)
      {
        // Result bit_width < Value bit_width
        RDCASSERT(retType->bitWidth < srcBitWidth);
      }
      else if(opCode == Operation::FPExt)
      {
        // Result bit_width > Value bit_width
        RDCASSERT(retType->bitWidth > srcBitWidth);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
      double x = 0.0;

#undef _IMPL
#define _IMPL(T) x = comp<T>(a, c);
      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);

      if(result.type == VarType::Float)
        comp<float>(result, c) = (float)x;
      else if(result.type == VarType::Half)
        comp<half_float::half>(result, c) = (float)x;
      else if(result.type == VarType::Double)
        comp<double>(result, c) = (double)x;

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
      const uint32_t c = 0;

      if(opCode == Operation::And)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) & comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::Or)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) | comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::Xor)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) ^ comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::ShiftLeft)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) << comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::LogicalShiftRight)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = comp<U>(a, c) >> comp<U>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else if(opCode == Operation::ArithShiftRight)
      {
        result.value.s64v[0] = a.value.s64v[0] << b.value.u64v[0];
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = comp<S>(a, c) >> comp<S>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }
      break;
    }
    case Operation::PtrToI:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Pointer);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, Type::Int);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      const uint32_t c = 0;
      uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(a, c);
      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = (U)x;
      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);

      break;
    }
    case Operation::IToPtr:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[0]->type->scalarType, Type::Int);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Pointer);
      RDCASSERTEQUAL(retType->scalarType, Type::Int);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      const uint32_t c = 0;
      uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(a, c);
      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = (U)x;
      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);

      break;
    }
    case Operation::ExtractElement:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Vector);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(retType->scalarType, inst.args[0]->type->inner->scalarType);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      const uint32_t idx = b.value.u32v[0];

#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, 0) = comp<I>(a, idx);
      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(T) comp<T>(result, 0) = comp<T>(a, idx);

      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);

      break;
    }
    case Operation::InsertElement:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Vector);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Vector);
      RDCASSERTEQUAL(retType->inner->scalarType, inst.args[0]->type->inner->scalarType);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
      RDCASSERTEQUAL(inst.args[1]->type->scalarType, inst.args[0]->type->inner->scalarType);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      ShaderVariable c;
      RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, c));
      const uint32_t idx = c.value.u32v[0];

      result = a;

#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, idx) = comp<I>(b, 0);
      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(T) comp<T>(result, idx) = comp<T>(b, 0);

      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, b.type);
      break;
    }
    case Operation::ShuffleVector:
    {
      RDCASSERTEQUAL(inst.args[0]->type->type, Type::TypeKind::Vector);
      RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Vector);
      RDCASSERTEQUAL(retType->type, Type::TypeKind::Vector);
      RDCASSERTEQUAL(retType->inner->scalarType, inst.args[0]->type->inner->scalarType);
      RDCASSERTEQUAL(inst.args[1]->type->inner->scalarType, inst.args[0]->type->inner->scalarType);
      RDCASSERTEQUAL(retType->elemCount, inst.args[2]->type->elemCount);
      ShaderVariable a;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, a));
      ShaderVariable b;
      bool bIsValid = GetShaderVariable(inst.args[1], opCode, dxOpCode, b);
      ShaderVariable c;
      RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, c));
      // TODO: mask entries might be undef meaning "don’t care"
      const uint32_t aMax = inst.args[0]->type->elemCount;
      for(uint32_t idx = 0; idx < retType->elemCount; idx++)
      {
        const uint32_t mask = c.value.u32v[idx];
        if(!bIsValid)
          RDCASSERT(mask < aMax);
        RDCASSERT(mask < retType->elemCount);

#undef _IMPL
#define _IMPL(I, S, U) \
  comp<I>(result, idx) = (mask < aMax) ? comp<I>(a, mask) : comp<I>(b, mask - aMax);
        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);

#undef _IMPL
#define _IMPL(T) comp<T>(result, idx) = (mask < aMax) ? comp<T>(a, mask) : comp<T>(b, mask - aMax);

        IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      break;
    }
    case Operation::Switch:
    {
      // Value, Default_Label then Pairs of { targetValue, label }
      ShaderVariable val;
      RDCASSERT(GetShaderVariable(inst.args[0], opCode, dxOpCode, val));
      uint32_t targetArg = 1;
      for(uint32_t a = 2; a < inst.args.size(); a += 2)
      {
        ShaderVariable targetVal;
        RDCASSERT(GetShaderVariable(inst.args[a], opCode, dxOpCode, targetVal));
        bool match = false;

#undef _IMPL
#define _IMPL(I, S, U) match = comp<I>(val, 0) == comp<I>(targetVal, 0);

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, val.type);

        if(match)
        {
          targetArg = a + 1;
          break;
        }
      }

      const Block *target = cast<Block>(inst.args[targetArg]);
      RDCASSERT(target);
      uint32_t blockId = target->id;
      if(blockId < m_FunctionInfo->function->blocks.size())
      {
        m_Block = blockId;
        m_FunctionInstructionIdx = m_FunctionInfo->function->blocks[m_Block]->startInstructionIdx;
        m_GlobalInstructionIdx = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
      }
      else
      {
        RDCERR("Unknown switch target %u '%s'", m_Block, GetArgumentName(targetArg).c_str());
      }
      break;
    }
    case Operation::Fence:
    {
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
      // TODO: full proper load and store from/to memory i.e. group shared
      // Currently only supporting Stack allocated pointers
      size_t allocSize = 0;
      void *allocMemoryBackingPtr = NULL;
      void *baseMemoryBackingPtr = NULL;
      Id baseMemoryId = DXILDebug::INVALID_ID;
      Id ptrId = GetArgumentId(0);
      {
        auto itPtr = m_MemoryAllocPointers.find(ptrId);
        RDCASSERT(itPtr != m_MemoryAllocPointers.end());

        const MemoryAllocPointer &ptr = itPtr->second;
        baseMemoryId = ptr.baseMemoryId;
        baseMemoryBackingPtr = ptr.backingMemory;

        auto itAlloc = m_MemoryAllocs.find(baseMemoryId);
        RDCASSERT(itAlloc != m_MemoryAllocs.end());
        const MemoryAlloc &alloc = itAlloc->second;
        allocSize = alloc.size;
        allocMemoryBackingPtr = alloc.backingMemory;
      }

      RDCASSERT(baseMemoryBackingPtr);
      RDCASSERTNOTEQUAL(baseMemoryId, DXILDebug::INVALID_ID);

      RDCASSERTEQUAL(resultId, DXILDebug::INVALID_ID);
      ShaderVariable a = m_Variables[baseMemoryId];

      ShaderVariable b;
      RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, b));
      const uint32_t c = 0;

      ShaderVariable res;

      if(opCode == Operation::AtomicExchange)
      {
        // *ptr = val
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(res, c) = comp<I>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicAdd)
      {
        // *ptr = *ptr + val
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(res, c) = comp<I>(a, c) + comp<I>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicSub)
      {
        // *ptr = *ptr - val
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(res, c) = comp<I>(a, c) - comp<I>(b, c)

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicAnd)
      {
        // *ptr = *ptr & val
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = comp<U>(a, c) & comp<U>(b, c);

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicNand)
      {
        // *ptr = ~(*ptr & val)
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = ~(comp<U>(a, c) & comp<U>(b, c));

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicOr)
      {
        // *ptr = *ptr | val
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = comp<U>(a, c) | comp<U>(b, c);

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicXor)
      {
        // *ptr = *ptr ^ val
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = comp<U>(a, c) ^ comp<U>(b, c);

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicMax)
      {
        // *ptr = max(*ptr, val)
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(res, c) = RDCMAX(comp<S>(a, c), comp<S>(b, c));

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicMin)
      {
        // *ptr = min(*ptr, val)
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(res, c) = RDCMIN(comp<S>(a, c), comp<S>(b, c));

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicUMax)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(res, c) = RDCMAX(comp<S>(a, c), comp<S>(b, c));

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else if(opCode == Operation::AtomicUMin)
      {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = RDCMIN(comp<U>(a, c), comp<U>(b, c));

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
      }
      else
      {
        RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
      }

      // Save the result back
      UpdateBackingMemoryFromVariable(baseMemoryBackingPtr, allocSize, res);

      ShaderVariableChange change;
      change.before = m_Variables[baseMemoryId];

      UpdateMemoryVariableFromBackingMemory(baseMemoryId, allocMemoryBackingPtr);

      // record the change to the base memory variable
      change.after = m_Variables[baseMemoryId];
      if(m_State)
        m_State->changes.push_back(change);

      // Update the ptr variable value
      // Set the result to be the ptr variable which will then be recorded as a change
      resultId = ptrId;
      result = m_Variables[resultId];
      result.value = res.value;
      break;
    }
    case Operation::AddrSpaceCast:
    case Operation::InsertValue:
    case Operation::CompareExchange:
      RDCERR("Unhandled LLVM opcode %s", ToStr(opCode).c_str());
      break;
  };

  // Remove variables which have gone out of scope
  rdcarray<Id> liveIdsToRemove;
  for(const Id &id : m_Live)
  {
    // The fake output variable is always in scope
    if(id == m_Output.id)
      continue;

    auto itRange = m_FunctionInfo->rangePerId.find(id);
    RDCASSERT(itRange != m_FunctionInfo->rangePerId.end());
    const uint32_t maxInstruction = itRange->second.max;
    if(m_FunctionInstructionIdx > maxInstruction)
    {
      RDCASSERTNOTEQUAL(id, resultId);
      liveIdsToRemove.push_back(id);
      if(!m_Dormant.contains(id))
        m_Dormant.push_back(id);

      if(m_State)
      {
        ShaderVariableChange change;
        change.before = m_Variables[id];
        m_State->changes.push_back(change);
      }
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

          // Do not record the change for the resultId of the next instruction
          if(m_State && (id != nextResultId))
          {
            ShaderVariableChange change;
            change.after = m_Variables[id];
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
  RDCASSERT(!(result.name.empty() ^ (resultId == DXILDebug::INVALID_ID)));
  if(!result.name.empty() && resultId != DXILDebug::INVALID_ID)
  {
    if(recordChange)
      SetResult(resultId, result, opCode, dxOpCode, eventFlags);

    // Fake Output results won't be in the referencedIds
    RDCASSERT(resultId == m_Output.id || m_FunctionInfo->referencedIds.count(resultId) == 1);

    if(!m_Live.contains(resultId))
      m_Live.push_back(resultId);
    m_Variables[resultId] = result;
  }

  return true;
}

void ThreadState::StepOverNopInstructions()
{
  if(m_Ended)
    return;
  do
  {
    m_GlobalInstructionIdx = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
    RDCASSERT(m_FunctionInstructionIdx < m_FunctionInfo->function->instructions.size());
    const Instruction *inst = m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
    if(!IsNopInstruction(*inst))
    {
      m_ActiveGlobalInstructionIdx = m_GlobalInstructionIdx;
      return;
    }

    m_FunctionInstructionIdx++;
  } while(true);
}

void ThreadState::StepNext(ShaderDebugState *state, DebugAPIWrapper *apiWrapper,
                           const rdcarray<ThreadState> &workgroups)
{
  m_State = state;

  RDCASSERTEQUAL(m_GlobalInstructionIdx,
                 m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx);
  RDCASSERTEQUAL(m_ActiveGlobalInstructionIdx, m_GlobalInstructionIdx);
  if(m_State)
  {
    if(!m_Ended)
      m_State->nextInstruction = m_GlobalInstructionIdx + 1;

    m_State->flags = ShaderEvents::NoEvent;
    m_State->changes.clear();
  }
  ExecuteInstruction(apiWrapper, workgroups);

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
      if(c->op == Operation::NoOp)
      {
        var.value.u64v[0] = 0;
        return true;
      }
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
      else if(c->op == Operation::NoOp)
      {
        ConvertDXILTypeToShaderVariable(c->type, var);
        RDCASSERT(ConvertDXILConstantToShaderVariable(c, var));
        return true;
      }
      else if(c->op != Operation::NoOp)
      {
        RDCERR("Constant isCompound DXIL Value with unsupported operation %s", ToStr(c->op).c_str());
      }
      return false;
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
  RDCERR("Unhandled DXIL Value type");

  return false;
}

bool ThreadState::GetVariable(const Id &id, Operation op, DXOp dxOpCode, ShaderVariable &var) const
{
  RDCASSERT(m_Live.contains(id));
  auto it = m_Variables.find(id);
  RDCASSERT(it != m_Variables.end());
  var = it->second;

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
    change.before = m_Variables[id];
    change.after = result;
    m_State->changes.push_back(change);
  }
}

void ThreadState::MarkResourceAccess(const rdcstr &name, const ShaderBindIndex &bindIndex)
{
  if(m_State == NULL)
    return;

  if(bindIndex.category != DescriptorCategory::ReadOnlyResource &&
     bindIndex.category != DescriptorCategory::ReadWriteResource)
    return;

  bool isSRV = (bindIndex.category == DescriptorCategory::ReadOnlyResource);

  m_State->changes.push_back(ShaderVariableChange());

  ShaderVariableChange &change = m_State->changes.back();
  change.after.rows = change.after.columns = 1;
  change.after.type = isSRV ? VarType::ReadOnlyResource : VarType::ReadWriteResource;
  change.after.SetBindIndex(bindIndex);
  // The resource name will already have the array index appended to it (perhaps unresolved)
  change.after.name = name;

  // Check whether this resource was visited before
  bool found = false;
  rdcarray<ShaderBindIndex> &accessed = isSRV ? m_accessedSRVs : m_accessedUAVs;
  for(size_t i = 0; i < accessed.size(); ++i)
  {
    if(accessed[i] == bindIndex)
    {
      found = true;
      break;
    }
  }

  if(found)
    change.before = change.after;
  else
    accessed.push_back(bindIndex);
}

void ThreadState::AllocateMemoryForType(const DXIL::Type *type, Id allocId, ShaderVariable &var)
{
  RDCASSERTEQUAL(type->type, Type::TypeKind::Pointer);
  ConvertDXILTypeToShaderVariable(type->inner, var);

  // Add the SSA to m_MemoryAllocs with its backing memory and size
  size_t byteSize = ComputeDXILTypeByteSize(type->inner);
  void *backingMem = malloc(byteSize);
  memset(backingMem, 0, byteSize);
  MemoryAlloc &alloc = m_MemoryAllocs[allocId];
  alloc = {backingMem, byteSize};

  // set the backing memory
  m_MemoryAllocPointers[allocId] = {allocId, backingMem, byteSize};
}

void ThreadState::UpdateBackingMemoryFromVariable(void *ptr, size_t &allocSize,
                                                  const ShaderVariable &var)
{
  // Memory copy from value to backing memory
  if(var.members.size() == 0)
  {
    RDCASSERTEQUAL(var.rows, 1);
    const size_t elementSize = GetElementByteSize(var.type);
    RDCASSERT(elementSize <= allocSize);
    RDCASSERT(elementSize < sizeof(var.value.f32v));
    const size_t varMemSize = var.columns * elementSize;
    memcpy(ptr, &var.value.f32v[0], varMemSize);
    allocSize -= varMemSize;
  }
  else
  {
    uint8_t *dst = (uint8_t *)ptr;
    for(uint32_t i = 0; i < var.members.size(); ++i)
    {
      const size_t elementSize = GetElementByteSize(var.members[i].type);
      const size_t varMemSize = var.members[i].columns * elementSize;
      UpdateBackingMemoryFromVariable(dst, allocSize, var.members[i]);
      dst += varMemSize;
    }
  }
}

void ThreadState::UpdateMemoryVariableFromBackingMemory(Id memoryId, const void *ptr)
{
  ShaderVariable &baseMemory = m_Variables[memoryId];
  // Memory copy from backing memory to base memory variable
  size_t elementSize = GetElementByteSize(baseMemory.type);
  const uint8_t *src = (const uint8_t *)ptr;
  if(baseMemory.members.size() == 0)
  {
    RDCASSERTEQUAL(baseMemory.rows, 1);
    RDCASSERTEQUAL(baseMemory.columns, 1);
    RDCASSERT(elementSize < sizeof(ShaderValue), elementSize);
    memcpy(&baseMemory.value.f32v[0], src, elementSize);
  }
  else
  {
    for(uint32_t i = 0; i < baseMemory.members.size(); ++i)
    {
      memcpy(&baseMemory.members[i].value.f32v[0], src, elementSize);
      src += elementSize;
    }
  }
}

void ThreadState::PerformGPUResourceOp(const rdcarray<ThreadState> &workgroups, Operation opCode,
                                       DXOp dxOpCode, const DXIL::ResourceReference *resRef,
                                       const ShaderBindIndex &bindIndex, DebugAPIWrapper *apiWrapper,
                                       const DXIL::Instruction &inst, ShaderVariable &result)
{
  // TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  // Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  // SampleBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,bias,clamp)
  // SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  // SampleGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)
  // SampleCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,clamp)
  // SampleCmpBias(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,bias,clamp)
  // SampleCmpLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,lod)
  // SampleCmpGrad(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue,ddx0,ddx1,ddx2,ddy0,ddy1,ddy2,clamp)
  // SampleCmpLevelZero(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,compareValue)
  // CalculateLOD(handle,sampler,coord0,coord1,coord2,clamped)

  // TextureGather(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel)
  // TextureGatherCmp(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,channel,compareValue)

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
  resourceData.binding.shaderRegister = resRef->resourceBase.regBase + bindIndex.arrayElement;

  ShaderVariable uv;
  int8_t texelOffsets[3] = {0, 0, 0};
  int msIndex = 0;
  float lodValue = 0.0f;
  float compareValue = 0.0f;

  SampleGatherSamplerData samplerData = {};
  samplerData.mode = SamplerMode::NUM_SAMPLERS;

  bool uvDDXY[4] = {false, false, false, false};
  GatherChannel gatherChannel = GatherChannel::Red;

  if(dxOpCode == DXOp::TextureLoad)
  {
    ShaderVariable arg;
    // mipLevelOrSampleCount is in arg 2
    if(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg, false))
    {
      uint32_t mipLevelOrSampleCount = arg.value.u32v[0];
      // The debug shader uses arrays of resources for 1D, 2D textures
      // mipLevel goes into UV[N] : N = 1D: 2, 2D: 3, 3D: 3
      switch(srv.shape)
      {
        case DXIL::ResourceKind::Texture1D: uv.value.u32v[2] = mipLevelOrSampleCount; break;
        case DXIL::ResourceKind::Texture2D: uv.value.u32v[3] = mipLevelOrSampleCount; break;
        case DXIL::ResourceKind::Texture3D: uv.value.u32v[3] = mipLevelOrSampleCount; break;
        case DXIL::ResourceKind::Texture2DMS: msIndex = mipLevelOrSampleCount; break;
        case DXIL::ResourceKind::Texture2DMSArray: msIndex = mipLevelOrSampleCount; break;
        default: break;
      }
    }

    // UV is int data in args 3,4,5
    // Offset is int data in args 6,7,8
    for(uint32_t i = 0; i < 3; ++i)
    {
      if(GetShaderVariable(inst.args[3 + i], opCode, dxOpCode, arg, false))
        uv.value.s32v[i] = arg.value.s32v[0];
      if(GetShaderVariable(inst.args[6 + i], opCode, dxOpCode, arg, false))
        texelOffsets[i] = (int8_t)arg.value.s32v[0];
    }
  }
  else
  {
    // Sampler is in arg 2
    Id samplerId = GetArgumentId(2);
    ShaderBindIndex samplerBindIndex;
    bool annotatedHandle;
    const DXIL::ResourceReference *samplerRef =
        GetResource(samplerId, samplerBindIndex, annotatedHandle);
    if(!samplerRef)
      return;

    RDCASSERTEQUAL(samplerRef->resourceBase.resClass, ResourceClass::Sampler);
    // samplerRef->resourceBase must be a Sampler
    const DXIL::EntryPointInterface::Sampler &sampler = samplerRef->resourceBase.samplerData;
    samplerData.bias = 0.0f;
    samplerData.binding.registerSpace = samplerRef->resourceBase.space;
    samplerData.binding.shaderRegister =
        samplerRef->resourceBase.regBase + samplerBindIndex.arrayElement;
    samplerData.mode = ConvertSamplerKindToSamplerMode(sampler.samplerType);

    int32_t biasArg = -1;
    int32_t lodArg = -1;
    int32_t compareArg = -1;
    int32_t gatherArg = -1;
    uint32_t countOffset = 3;
    uint32_t countUV = 4;
    // TODO: Sample*: Clamp is in arg 10
    // TODO: CalculateLOD: clamped is in arg 6
    // CalculateSampleGather returns {CalculateLevelOfDetail(), CalculateLevelOfDetailUnclamped()}

    // SampleBias : bias is arg 10
    // SampleLevel: lod is in arg 10
    // SampleCmp: compare is in arg 10
    // SampleCmpBias: compare is in arg 10, bias is in arg 11
    // SampleCmpLevel: compare is in arg 10, LOD is in arg 11
    // SampleCmpGrad: compare is in arg 10
    // SampleCmpLevelZero: compare is in arg 10
    // TextureGather: compare is in arg 10, gather is in 9
    // TextureGatherCmp: compare is in arg 10, gather is in 9
    switch(dxOpCode)
    {
      case DXOp::Sample: break;
      case DXOp::SampleBias: biasArg = 10; break;
      case DXOp::SampleLevel: lodArg = 10; break;
      case DXOp::SampleGrad: break;
      case DXOp::SampleCmp: compareArg = 10; break;
      case DXOp::SampleCmpBias:
        compareArg = 10;
        biasArg = 11;
        break;
      case DXOp::SampleCmpLevel:
        compareArg = 10;
        lodArg = 11;
        break;
      case DXOp::SampleCmpGrad: compareArg = 10; break;
      case DXOp::SampleCmpLevelZero: compareArg = 10; break;
      case DXOp::TextureGather:
        countOffset = 2;
        gatherArg = 9;
        break;
      case DXOp::CalculateLOD: countUV = 3; break;
      case DXOp::TextureGatherCmp:
        countOffset = 2;
        gatherArg = 9;
        compareArg = 10;
        break;
      default: RDCERR("Unhandled DX Operation %s", ToStr(dxOpCode).c_str()); break;
    }

    ShaderVariable arg;
    // UV is float data in args: Sample* 3,4,5,6 ; CalculateLOD 3,4,5
    for(uint32_t i = 0; i < countUV; ++i)
    {
      if(GetShaderVariable(inst.args[3 + i], opCode, dxOpCode, arg))
      {
        uv.value.f32v[i] = arg.value.f32v[0];
        // variables will have a name, constants will not have a name
        if(!arg.name.empty())
          uvDDXY[i] = true;
      }
    }

    // Offset is int data in args: Sample* 7,8,9 ; Gather* 7,8
    for(uint32_t i = 0; i < countOffset; ++i)
    {
      if(GetShaderVariable(inst.args[7 + i], opCode, dxOpCode, arg, false))
        texelOffsets[i] = (int8_t)arg.value.s32v[0];
    }

    if((lodArg > 0))
    {
      if(GetShaderVariable(inst.args[lodArg], opCode, dxOpCode, arg))
      {
        RDCASSERTEQUAL(arg.type, VarType::Float);
        lodValue = arg.value.f32v[0];
      }
    }
    if((compareArg > 0))
    {
      if(GetShaderVariable(inst.args[compareArg], opCode, dxOpCode, arg))
      {
        RDCASSERTEQUAL(arg.type, VarType::Float);
        compareValue = arg.value.f32v[0];
      }
    }

    if(biasArg > 0)
    {
      if(GetShaderVariable(inst.args[biasArg], opCode, dxOpCode, arg))
      {
        RDCASSERTEQUAL(arg.type, VarType::Float);
        samplerData.bias = arg.value.f32v[0];
      }
    }

    if(gatherArg > 0)
    {
      if(GetShaderVariable(inst.args[gatherArg], opCode, dxOpCode, arg))
      {
        RDCASSERTEQUAL(arg.type, VarType::SInt);
        // Red = 0, Green = 1, Blue = 2, Alpha = 3
        gatherChannel = (DXILDebug::GatherChannel)arg.value.s32v[0];
      }
    }
  }

  ShaderVariable ddx;
  ShaderVariable ddy;
  // Sample, SampleBias, CalculateLOD need DDX, DDY
  if((dxOpCode == DXOp::Sample) || (dxOpCode == DXOp::SampleBias) || (dxOpCode == DXOp::CalculateLOD))
  {
    if(m_ShaderType != DXBC::ShaderType::Pixel || workgroups.size() != 4)
    {
      RDCERR("Undefined results using derivative instruction outside of a pixel shader.");
    }
    else
    {
      RDCASSERT(!ThreadsAreDiverged(workgroups));
      // texture samples use coarse derivatives
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
  else if((dxOpCode == DXOp::SampleGrad) || (dxOpCode == DXOp::SampleCmpGrad))
  {
    // SampleGrad DDX is argument 10, DDY is argument 14
    // SampleCmpGrad DDX is argument 11, DDY is argument 15
    uint32_t ddx0 = dxOpCode == DXOp::SampleGrad ? 10 : 11;
    uint32_t ddy0 = ddx0 + 3;
    ShaderVariable arg;
    for(uint32_t i = 0; i < 4; i++)
    {
      if(uvDDXY[i])
      {
        RDCASSERT(GetShaderVariable(inst.args[ddx0 + i], opCode, dxOpCode, arg));
        ddx.value.f32v[i] = arg.value.f32v[0];
        RDCASSERT(GetShaderVariable(inst.args[ddy0 + i], opCode, dxOpCode, arg));
        ddy.value.f32v[i] = arg.value.f32v[0];
      }
    }
  }

  uint8_t swizzle[4] = {0, 1, 2, 3};

  uint32_t instructionIdx = m_FunctionInstructionIdx - 1;
  const char *opString = ToStr(dxOpCode).c_str();

  // TODO: TextureGatherRaw // SM 6.7
  // Return types for TextureGatherRaw
  // DXGI_FORMAT_R16_UINT : u16
  // DXGI_FORMAT_R32_UINT : u32
  // DXGI_FORMAT_R32G32_UINT : u32x2

  ShaderVariable data;
  apiWrapper->CalculateSampleGather(dxOpCode, resourceData, samplerData, uv, ddx, ddy, texelOffsets,
                                    msIndex, lodValue, compareValue, swizzle, gatherChannel,
                                    m_ShaderType, instructionIdx, opString, data);

  result.value = data.value;
}

rdcstr ThreadState::GetArgumentName(uint32_t i) const
{
  return m_Program.GetArgId(*m_CurrentInstruction, i);
}

DXILDebug::Id ThreadState::GetArgumentId(uint32_t i) const
{
  DXIL::Value *arg = m_CurrentInstruction->args[i];
  return GetSSAId(arg);
}

const DXIL::ResourceReference *ThreadState::GetResource(Id handleId, ShaderBindIndex &bindIndex,
                                                        bool &annotatedHandle)
{
  RDCASSERT(m_Live.contains(handleId));
  auto it = m_Variables.find(handleId);
  if(it != m_Variables.end())
  {
    const ShaderVariable &var = m_Variables.at(handleId);
    const DXIL::ResourceReference *resRef = m_Program.GetResourceReference(handleId);
    if(resRef)
    {
      rdcstr alias = m_Program.GetHandleAlias(resRef->handleID);
      bindIndex = var.GetBindIndex();
      annotatedHandle = var.value.u32v[3] != 0;
      RDCASSERT(!annotatedHandle || (m_AnnotatedProperties.count(handleId) == 1));
      MarkResourceAccess(alias, bindIndex);
      return resRef;
    }
  }

  RDCERR("Unknown resource handle %u", handleId);
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
  RDCASSERT(!ThreadsAreDiverged(quad));
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
  RDCASSERT(!ThreadsAreDiverged(quad));
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

bool ThreadState::ThreadsAreDiverged(const rdcarray<ThreadState> &workgroups)
{
  const uint32_t block0 = workgroups[0].m_Block;
  const uint32_t instr0 = workgroups[0].m_ActiveGlobalInstructionIdx;
  for(size_t i = 1; i < workgroups.size(); i++)
  {
    // not in the same basic block
    if(workgroups[i].m_Block != block0)
      return true;
    // not executing the same instruction
    if(workgroups[i].m_ActiveGlobalInstructionIdx != instr0)
      return true;
  }
  return false;
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

  // Not diverged then all active
  if(!ThreadState::ThreadsAreDiverged(m_Workgroups))
    return;

  for(size_t i = 0; i < m_Workgroups.size(); i++)
  {
    if(!activeMask[i])
      continue;
    // Run any thread that is not in a uniform block
    // Stop any thread that is not in a uniform block
    activeMask[i] = !m_Workgroups[i].InUniformBlock();
  }
  return;
}

size_t Debugger::FindScopedDebugDataIndex(const uint32_t instructionIndex) const
{
  size_t countScopes = m_DebugInfo.scopedDebugDatas.size();
  size_t scopeIndex = countScopes;
  // Scopes are sorted with increasing minInstruction
  for(size_t i = 0; i < countScopes; i++)
  {
    uint32_t scopeMinInstruction = m_DebugInfo.scopedDebugDatas[i].minInstruction;
    if((scopeMinInstruction <= instructionIndex) &&
       (instructionIndex <= m_DebugInfo.scopedDebugDatas[i].maxInstruction))
      scopeIndex = i;
    else if(scopeMinInstruction > instructionIndex)
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
  // Iterate upwards to find DIFile, DISubprogram or DILexicalBlock scope
  while((scopeMD->dwarf->type != DIBase::File) && (scopeMD->dwarf->type != DIBase::Subprogram) &&
        (scopeMD->dwarf->type != DIBase::LexicalBlock))
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
  else
  {
    ScopedDebugData &scope = m_DebugInfo.scopedDebugDatas[scopeIndex];
    scope.minInstruction = RDCMIN(scope.minInstruction, instructionIndex);
    scope.maxInstruction = RDCMAX(scope.maxInstruction, instructionIndex);
  }
  return scopeIndex;
}

const TypeData &Debugger::AddDebugType(const DXIL::Metadata *typeMD)
{
  TypeData typeData;

  const DXIL::DIBase *base = typeMD->dwarf;

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
        case DW_TAG_base_type:
        {
          typeData.alignInBytes = (uint32_t)(basicType->alignInBits / 8);
          typeData.sizeInBytes = sizeInBits / 8;
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
          else if(sizeInBits == 64)
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
          else if(sizeInBits == 64)
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
      typeData.baseType = typeMD;
      switch(compositeType->tag)
      {
        case DW_TAG_class_type:
        case DW_TAG_structure_type:
        {
          typeData.name = *compositeType->name;
          typeData.type = VarType::Struct;
          typeData.sizeInBytes = (uint32_t)(compositeType->sizeInBits / 8);
          typeData.alignInBytes = (uint32_t)(compositeType->alignInBits / 8);
          const Metadata *elementsMD = compositeType->elements;
          size_t countMembers = elementsMD->children.size();
          for(size_t i = 0; i < countMembers; ++i)
          {
            const Metadata *memberMD = elementsMD->children[i];
            const DXIL::DIBase *memberBase = memberMD->dwarf;
            RDCASSERTEQUAL(memberBase->type, DXIL::DIBase::DerivedType);
            const DXIL::DIDerivedType *member = memberBase->As<DIDerivedType>();
            RDCASSERTEQUAL(member->tag, DXIL::DW_TAG_member);
            // const TypeData &memberType = AddDebugType(member->base);
            AddDebugType(member->base);
            typeData.structMembers.push_back({*member->name, member->base});
            uint32_t offset = (uint32_t)member->offsetInBits / 8;
            typeData.memberOffsets.push_back(offset);
          }
          break;
        }
        case DW_TAG_array_type:
        {
          typeData.arrayDimensions.clear();
          typeData.sizeInBytes = (uint32_t)(compositeType->sizeInBits / 8);
          typeData.alignInBytes = (uint32_t)(compositeType->alignInBits / 8);
          // elements->children is the array dimensionality
          const Metadata *elementsMD = compositeType->elements;
          for(int32_t x = 0; x < elementsMD->children.count(); x++)
          {
            const DXIL::DIBase *baseElement = elementsMD->children[x]->dwarf;
            RDCASSERTEQUAL(baseElement->type, DXIL::DIBase::Type::Subrange);
            uint32_t countElements = (uint32_t)baseElement->As<DXIL::DISubrange>()->count;
            typeData.arrayDimensions.push_back(countElements);
          }
          // TODO : WHERE IS THE BASE ELEMENT TYPE
          AddDebugType(compositeType->base);
          typeData.baseType = compositeType->base;
          // RDCERR("Unhandled Array %s", ToStr(typeData.name).c_str());
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
      const DIDerivedType *derivedType = base->As<DIDerivedType>();
      switch(derivedType->tag)
      {
        case DW_TAG_const_type:
        case DW_TAG_typedef: typeData = AddDebugType(derivedType->base); break;
        default:
          RDCERR("Unhandled DIDerivedType DIDerivedType Tag type %s",
                 ToStr(derivedType->tag).c_str());
          break;
      }
      break;
    }
    default: RDCERR("Unhandled DXIL type %s", ToStr(base->type).c_str()); break;
  }

  m_DebugInfo.types[typeMD] = typeData;
  return m_DebugInfo.types[typeMD];
}

void Debugger::AddLocalVariable(const DXIL::Metadata *localVariableMD, uint32_t instructionIndex,
                                bool isDeclare, int32_t byteOffset, uint32_t countBytes,
                                const rdcstr &debugVarSSAName)
{
  RDCASSERT(localVariableMD);
  RDCASSERTEQUAL(localVariableMD->dwarf->type, DIBase::Type::LocalVariable);
  const DILocalVariable *localVariable = localVariableMD->dwarf->As<DILocalVariable>();

  size_t scopeIndex = AddScopedDebugData(localVariable->scope, instructionIndex);
  ScopedDebugData &scope = m_DebugInfo.scopedDebugDatas[scopeIndex];

  rdcstr sourceVarName = m_Program->GetDebugVarName(localVariable);
  LocalMapping localMapping;
  localMapping.variable = localVariable;
  localMapping.sourceVarName = sourceVarName;
  localMapping.debugVarSSAName = debugVarSSAName;
  localMapping.byteOffset = byteOffset;
  localMapping.countBytes = countBytes;
  localMapping.instIndex = instructionIndex;
  localMapping.isDeclare = isDeclare;

  scope.localMappings.push_back(localMapping);

  const DXIL::Metadata *typeMD = localVariable->type;
  if(m_DebugInfo.types.count(typeMD) == 0)
    AddDebugType(typeMD);

  if(m_DebugInfo.locals.count(localVariable) == 0)
    m_DebugInfo.locals[localVariable] = localMapping;
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
  int32_t byteOffset = 0;

  // arg 1 is DILocalVariable metadata
  const Metadata *localVariableMD = cast<Metadata>(inst.args[1]);

  // arg 2 is DIExpression metadata
  const Metadata *expressionMD = cast<Metadata>(inst.args[2]);
  uint32_t countBytes = 0;
  if(expressionMD)
  {
    if(expressionMD->dwarf->type == DIBase::Type::Expression)
    {
      const DIExpression *expression = expressionMD->dwarf->As<DXIL::DIExpression>();
      switch(expression->op)
      {
        case DXIL::DW_OP::DW_OP_bit_piece:
          byteOffset += (uint32_t)(expression->evaluated.bit_piece.offset / 8);
          countBytes = (uint32_t)(expression->evaluated.bit_piece.size / 8);
          break;
        case DXIL::DW_OP::DW_OP_none: break;
        case DXIL::DW_OP::DW_OP_nop: break;
        default: RDCERR("Unhandled DIExpression op %s", ToStr(expression->op).c_str()); break;
      }
    }
    else
    {
      RDCERR("Unhandled Expression Metadata %s", ToStr(expressionMD->dwarf->type).c_str());
    }
  }

  AddLocalVariable(localVariableMD, instructionIndex, true, byteOffset, countBytes, resultId);
}

void Debugger::ParseDbgOpValue(const DXIL::Instruction &inst, uint32_t instructionIndex)
{
  // arg 0 is metadata containing the new value
  const Metadata *valueMD = cast<Metadata>(inst.args[0]);
  rdcstr resultId = m_Program->GetArgId(valueMD->value);
  // arg 1 is i64 byte offset in the source variable where the new value is written
  int64_t value = 0;
  RDCASSERT(getival<int64_t>(inst.args[1], value));
  int32_t byteOffset = (int32_t)(value);

  // arg 2 is DILocalVariable metadata
  const Metadata *localVariableMD = cast<Metadata>(inst.args[2]);

  // arg 3 is DIExpression metadata
  uint32_t countBytes = 0;
  const Metadata *expressionMD = cast<Metadata>(inst.args[3]);
  if(expressionMD)
  {
    if(expressionMD->dwarf->type == DIBase::Type::Expression)
    {
      const DIExpression *expression = expressionMD->dwarf->As<DXIL::DIExpression>();
      switch(expression->op)
      {
        case DXIL::DW_OP::DW_OP_bit_piece:
          byteOffset += (uint32_t)(expression->evaluated.bit_piece.offset / 8);
          countBytes = (uint32_t)(expression->evaluated.bit_piece.size / 8);
          break;
        case DXIL::DW_OP::DW_OP_none: break;
        case DXIL::DW_OP::DW_OP_nop: break;
        default: RDCERR("Unhandled DIExpression op %s", ToStr(expression->op).c_str()); break;
      }
    }
    else
    {
      RDCERR("Unhandled Expression Metadata %s", ToStr(expressionMD->dwarf->type).c_str());
    }
  }

  AddLocalVariable(localVariableMD, instructionIndex, false, byteOffset, countBytes, resultId);
}

void Debugger::ParseDebugData()
{
  // Parse LLVM debug data
  // TODO : Track current active scope, previous scope
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      const FunctionInfo &info = m_FunctionInfos[f];
      uint32_t countInstructions = (uint32_t)f->instructions.size();
      uint32_t activeInstructionIndex = 0;

      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        uint32_t instructionIndex = i + info.globalInstructionOffset;
        const Instruction &inst = *(f->instructions[i]);
        if(!DXIL::IsLLVMDebugCall(inst))
        {
          // Include DebugLoc data for building up the list of scopes
          uint32_t dbgLoc = inst.debugLoc;
          if(dbgLoc != ~0U)
          {
            activeInstructionIndex = instructionIndex;
            const DebugLocation &debugLoc = m_Program->m_DebugLocations[dbgLoc];
            AddScopedDebugData(debugLoc.scope, activeInstructionIndex);
          }
          continue;
        }

        const Function *dbgFunc = inst.getFuncCall();
        switch(dbgFunc->llvmDbgOp)
        {
          case LLVMDbgOp::Declare: ParseDbgOpDeclare(inst, activeInstructionIndex); break;
          case LLVMDbgOp::Value: ParseDbgOpValue(inst, activeInstructionIndex); break;
          case LLVMDbgOp::Unknown:
            RDCASSERT("Unsupported LLVM debug operation", dbgFunc->llvmDbgOp);
            break;
        };
      }
    }
  }

  // Sort the scopes by instruction index
  std::sort(m_DebugInfo.scopedDebugDatas.begin(), m_DebugInfo.scopedDebugDatas.end(),
            [](const ScopedDebugData &a, const ScopedDebugData &b) { return a < b; });

  DXIL::Program *program = ((DXIL::Program *)m_Program);
  program->m_Locals.clear();

  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      const FunctionInfo &info = m_FunctionInfos[f];
      uint32_t countInstructions = (uint32_t)f->instructions.size();

      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        uint32_t instructionIndex = i + info.globalInstructionOffset;

        DXIL::Program::LocalSourceVariable localSrcVar;
        localSrcVar.startInst = instructionIndex;
        localSrcVar.endInst = instructionIndex;

        // For each instruction - find which scope it belongs
        size_t scopeIndex = FindScopedDebugDataIndex(instructionIndex);
        // track which mappings we've processed, so if the same variable has mappings in multiple
        // scopes we only pick the innermost.
        rdcarray<LocalMapping> processed;
        rdcarray<const DXIL::DILocalVariable *> sourceVars;

        // capture the scopes upwards (from child to parent)
        rdcarray<size_t> scopeIndexes;
        while(scopeIndex < m_DebugInfo.scopedDebugDatas.size())
        {
          const ScopedDebugData &scope = m_DebugInfo.scopedDebugDatas[scopeIndex];

          // Only add add scopes with mappings
          if(!scope.localMappings.empty())
            scopeIndexes.push_back(scopeIndex);

          // if we reach a function scope, don't go up any further.
          if(scope.md->dwarf->type == DIBase::Type::Subprogram)
            break;

          scopeIndex = scope.parentIndex;
        }

        // Iterate over the scopes downwards (parent->child)
        for(size_t s = 0; s < scopeIndexes.size(); ++s)
        {
          scopeIndex = scopeIndexes[scopeIndexes.size() - 1 - s];
          const ScopedDebugData &scope = m_DebugInfo.scopedDebugDatas[scopeIndex];
          size_t countLocalMappings = scope.localMappings.size();
          for(size_t m = 0; m < countLocalMappings; m++)
          {
            const LocalMapping &mapping = scope.localMappings[m];

            // see if this mapping is superceded by a later mapping in this scope for this
            // instruction. This is a bit inefficient but simple. The alternative would be to do
            // record start and end points for each mapping and update the end points, but this is
            // simple and should be limited since it's only per-scope
            size_t innerStart = m + 1;
            if(innerStart < countLocalMappings)
            {
              bool supercede = false;
              for(size_t n = innerStart; n < countLocalMappings; n++)
              {
                const LocalMapping &laterMapping = scope.localMappings[n];

                // if this mapping will supercede and starts later
                if(laterMapping.isSourceSupersetOf(mapping) &&
                   laterMapping.instIndex > mapping.instIndex)
                {
                  supercede = true;
                  break;
                }
              }

              // don't add the current mapping if it's going to be superceded by something later
              if(supercede)
                continue;
            }

            processed.push_back(mapping);
            const DXIL::DILocalVariable *sourceVar = mapping.variable;
            if(!sourceVars.contains(sourceVar))
              sourceVars.push_back(sourceVar);
          }
        }

        // Converting debug variable mappings to SourceVariableMapping is a two phase algorithm.

        // Phase One
        // For each source variable, repeatedly apply the debug variable mappings.
        // This debug variable usage is tracked in a tree-like structure built using DebugVarNode
        // elements.
        // As each mapping is applied, the new mapping can fully or partially override the
        // existing mapping. When an existing mapping is:
        //  - fully overridden: any sub-elements of that mapping are cleared
        //    i.e. assigning a vector, array, structure
        //  - partially overriden: the existing mapping is expanded into its sub-elements which are
        //    mapped to the current mapping and then the new mapping is set to its corresponding
        //    elements i.e. y-component in a vector, member in a structure, a single array element
        // The DebugVarNode member "emitSourceVar" determines if the DebugVar mapping should be
        // converted to a source variable mapping.

        // Phase Two
        // The DebugVarNode tree is walked to find the nodes which have "emitSourceVar" set to
        // true and then those nodes are converted to SourceVariableMapping

        struct DebugVarNode
        {
          rdcarray<DebugVarNode> children;
          rdcstr debugVarSSAName;
          rdcstr name;
          rdcstr debugVarSuffix;
          VarType type = VarType::Unknown;
          uint32_t rows = 0;
          uint32_t columns = 0;
          uint32_t debugVarComponent = 0;
          uint32_t offset = 0;
          bool emitSourceVar = false;
        };

        ::std::map<const DXIL::DILocalVariable *, DebugVarNode> roots;

        // Phase One: generate the DebugVarNode tree by repeatedly applying debug variables
        // updating existing mappings with later mappings
        for(size_t sv = 0; sv < sourceVars.size(); ++sv)
        {
          const DXIL::DILocalVariable *variable = sourceVars[sv];

          // Convert processed mappings into a usage map
          for(size_t m = 0; m < processed.size(); ++m)
          {
            const LocalMapping &mapping = processed[m];
            if(mapping.variable != variable)
              continue;

            DebugVarNode *usage = &roots[variable];
            if(usage->name.isEmpty())
            {
              usage->name = mapping.sourceVarName;
              usage->rows = 1U;
              usage->columns = 1U;
            }

            const DXIL::Metadata *typeMD = variable->type;
            const TypeData *typeWalk = &m_DebugInfo.types[typeMD];

            // if the mapping is the entire variable
            if((mapping.byteOffset == 0 && mapping.countBytes == 0))
            {
              uint32_t rows = 1;
              uint32_t columns = 1;
              // TODO: is it worth considering GPU pointers for DXIL
              // skip past any pointer types to get the 'real' type that we'll see
              while(typeWalk && typeWalk->baseType != NULL && typeWalk->type == VarType::GPUPointer)
                typeWalk = &m_DebugInfo.types[typeWalk->baseType];

              const size_t arrayDimension = typeWalk->arrayDimensions.size();
              if(arrayDimension > 0)
              {
                // walk down until we get to a scalar type, if we get there. This means arrays of
                // basic types will get the right type
                while(typeWalk && typeWalk->baseType != NULL && typeWalk->type == VarType::Unknown)
                  typeWalk = &m_DebugInfo.types[typeWalk->baseType];

                usage->type = typeWalk->type;
              }
              else if(!typeWalk->structMembers.empty())
              {
                usage->type = typeWalk->type;
              }
              if(typeWalk->matSize != 0)
              {
                const TypeData &vec = m_DebugInfo.types[typeWalk->baseType];
                const TypeData &scalar = m_DebugInfo.types[vec.baseType];

                usage->type = scalar.type;

                if(typeWalk->colMajorMat)
                {
                  rows = RDCMAX(1U, vec.vecSize);
                  columns = RDCMAX(1U, typeWalk->matSize);
                }
                else
                {
                  columns = RDCMAX(1U, vec.vecSize);
                  rows = RDCMAX(1U, typeWalk->matSize);
                }
              }
              else if(typeWalk->vecSize != 0)
              {
                const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];

                usage->type = scalar.type;
                columns = RDCMAX(1U, typeWalk->vecSize);
              }
              else
              {
                const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];

                usage->type = scalar.type;
                columns = 1U;
              }

              usage->debugVarSSAName = mapping.debugVarSSAName;
              // Remove any child mappings : this mapping covers everything
              usage->children.clear();
              usage->emitSourceVar = true;
              usage->rows = rows;
              usage->columns = columns;
            }
            else
            {
              uint32_t byteOffset = mapping.byteOffset;
              uint32_t bytesRemaining = mapping.countBytes;

              // walk arrays and structures
              while(bytesRemaining > 0)
              {
                const TypeData *childType = NULL;
                const size_t arrayDimension = typeWalk->arrayDimensions.size();
                if(arrayDimension > 0)
                {
                  if((byteOffset == 0) && (bytesRemaining == typeWalk->sizeInBytes))
                  {
                    // Remove mappings : this mapping covers everything
                    usage->debugVarSSAName = mapping.debugVarSSAName;
                    usage->children.clear();
                    usage->emitSourceVar = true;
                    usage->debugVarSuffix.clear();
                    bytesRemaining = 0;
                    break;
                  }

                  const rdcarray<uint32_t> &dims = typeWalk->arrayDimensions;
                  childType = &m_DebugInfo.types[typeWalk->baseType];
                  uint32_t childRows = 1U;
                  uint32_t childColumns = 1U;
                  VarType elementType = childType->type;
                  uint32_t elementOffset = 1;
                  if(childType->matSize != 0)
                  {
                    const TypeData &vec = m_DebugInfo.types[childType->baseType];
                    const TypeData &scalar = m_DebugInfo.types[vec.baseType];

                    elementType = scalar.type;
                    if(childType->colMajorMat)
                    {
                      childRows = RDCMAX(1U, vec.vecSize);
                      childColumns = RDCMAX(1U, childType->matSize);
                    }
                    else
                    {
                      childColumns = RDCMAX(1U, vec.vecSize);
                      childRows = RDCMAX(1U, childType->matSize);
                    }
                  }
                  else if(childType->vecSize != 0)
                  {
                    const TypeData &scalar = m_DebugInfo.types[childType->baseType];
                    uint32_t vecColumns = RDCMAX(1U, childType->vecSize);

                    elementType = scalar.type;

                    childRows = 1U;
                    childColumns = vecColumns;
                  }
                  else if(!childType->structMembers.empty())
                  {
                    elementOffset += childType->memberOffsets[childType->memberOffsets.count() - 1];
                  }
                  elementOffset *= childRows * childColumns;
                  const uint32_t countDims = (uint32_t)arrayDimension;
                  // TODO : N dimensional arrays
                  for(uint32_t d = 0; d < countDims; ++d)
                  {
                    uint32_t elementSize = childType->sizeInBytes;
                    uint32_t elementIndex = byteOffset / elementSize;
                    byteOffset -= elementIndex * elementSize;
                    uint32_t rows = dims[d];
                    usage->rows = rows;
                    usage->columns = 1U;
                    // Expand the node if required
                    if(usage->children.isEmpty())
                    {
                      usage->children.resize(rows);
                      for(uint32_t x = 0; x < rows; x++)
                      {
                        usage->children[x].debugVarSSAName = usage->debugVarSSAName;
                        rdcstr suffix = StringFormat::Fmt("[%u]", x);
                        usage->children[x].debugVarSuffix = usage->debugVarSuffix + suffix;
                        usage->children[x].name = usage->name + suffix;
                        usage->children[x].type = elementType;
                        usage->children[x].rows = childRows;
                        usage->children[x].columns = childColumns;
                        usage->children[x].offset = usage->offset + x * elementOffset;
                      }
                    }
                    RDCASSERTEQUAL(usage->children.size(), rows);
                    // if the whole node was displayed : display the sub-elements
                    if(usage->emitSourceVar)
                    {
                      for(uint32_t x = 0; x < rows; x++)
                        usage->children[x].emitSourceVar = true;
                      usage->emitSourceVar = false;
                    }
                    // TODO : mapping covers whole sub-array
                    {
                      usage = &usage->children[elementIndex];
                      usage->type = childType->type;
                      typeWalk = childType;
                    }
                  }
                  break;
                }
                else if(!typeWalk->structMembers.empty())
                {
                  uint32_t rows = (uint32_t)typeWalk->structMembers.size();
                  usage->rows = rows;
                  usage->columns = 1U;

                  if((byteOffset == 0) && (bytesRemaining == typeWalk->sizeInBytes))
                  {
                    // Remove mappings : this mapping covers everything
                    usage->debugVarSSAName = mapping.debugVarSSAName;
                    usage->children.clear();
                    usage->emitSourceVar = true;
                    usage->debugVarSuffix.clear();
                    bytesRemaining = 0;
                    break;
                  }

                  // Loop over the member offsets in reverse to find the first member in the byteOffset
                  uint32_t memberIndex = rows;
                  for(uint32_t x = 0; x < rows; x++)
                  {
                    uint32_t idx = rows - x - 1;
                    uint32_t memberOffset = typeWalk->memberOffsets[idx];
                    if(byteOffset >= memberOffset)
                    {
                      memberIndex = idx;
                      byteOffset -= memberOffset;
                      break;
                    }
                  }

                  childType = &m_DebugInfo.types[typeWalk->structMembers[memberIndex].second];

                  // Expand the node if required
                  if(usage->children.isEmpty())
                  {
                    usage->children.resize(rows);
                    for(uint32_t x = 0; x < rows; x++)
                    {
                      rdcstr suffix =
                          StringFormat::Fmt(".%s", typeWalk->structMembers[x].first.c_str());
                      usage->children[x].debugVarSSAName = usage->debugVarSSAName;
                      usage->children[x].debugVarSuffix = usage->debugVarSuffix + suffix;
                      usage->children[x].name = usage->name + suffix;
                      usage->children[x].offset = usage->offset + typeWalk->memberOffsets[x];
                      uint32_t memberRows = 1U;
                      uint32_t memberColumns = 1U;
                      const TypeData *memberType =
                          &m_DebugInfo.types[typeWalk->structMembers[x].second];
                      VarType elementType = memberType->type;
                      if(memberType->matSize != 0)
                      {
                        const TypeData &vec = m_DebugInfo.types[memberType->baseType];
                        const TypeData &scalar = m_DebugInfo.types[vec.baseType];

                        elementType = scalar.type;
                        if(memberType->colMajorMat)
                        {
                          memberRows = RDCMAX(1U, vec.vecSize);
                          memberColumns = RDCMAX(1U, memberType->matSize);
                        }
                        else
                        {
                          memberColumns = RDCMAX(1U, vec.vecSize);
                          memberRows = RDCMAX(1U, memberType->matSize);
                        }
                      }
                      else if(memberType->vecSize != 0)
                      {
                        const TypeData &scalar = m_DebugInfo.types[memberType->baseType];
                        uint32_t vecColumns = RDCMAX(1U, memberType->vecSize);

                        elementType = scalar.type;

                        memberRows = 1U;
                        memberColumns = vecColumns;
                      }
                      usage->children[x].type = elementType;
                      usage->children[x].rows = memberRows;
                      usage->children[x].columns = memberColumns;
                    }
                  }
                  RDCASSERTEQUAL(usage->children.size(), rows);
                  // if the whole node was displayed : display the sub-elements
                  if(usage->emitSourceVar)
                  {
                    for(uint32_t x = 0; x < rows; x++)
                      usage->children[x].emitSourceVar = true;
                    usage->emitSourceVar = false;
                  }

                  usage = &usage->children[memberIndex];
                  usage->type = childType->type;
                  typeWalk = childType;
                }
                else
                {
                  break;
                }
              }

              uint32_t rows = 1U;
              uint32_t columns = 1U;

              if(typeWalk->matSize != 0)
              {
                // Index into the matrix using byte offset and row/column layout
                const TypeData &vec = m_DebugInfo.types[typeWalk->baseType];
                const TypeData &scalar = m_DebugInfo.types[vec.baseType];
                usage->type = scalar.type;

                if(typeWalk->colMajorMat)
                {
                  rows = RDCMAX(1U, vec.vecSize);
                  columns = RDCMAX(1U, typeWalk->matSize);
                }
                else
                {
                  columns = RDCMAX(1U, vec.vecSize);
                  rows = RDCMAX(1U, typeWalk->matSize);
                }
                usage->rows = rows;
                usage->columns = columns;

                RDCERR("Matrix types not handled yet %s %u %u", typeWalk->name.c_str(), byteOffset,
                       bytesRemaining);

                if(bytesRemaining == 0)
                {
                  // Remove mappings : this mapping covers everything
                  usage->debugVarSSAName = mapping.debugVarSSAName;
                  usage->children.clear();
                  usage->emitSourceVar = true;
                  usage->debugVarSuffix.clear();
                }
              }
              else if(typeWalk->vecSize != 0)
              {
                // Index into the vector using byte offset and component size
                const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];
                uint32_t componentIndex = byteOffset / scalar.sizeInBytes;
                columns = RDCMAX(1U, typeWalk->vecSize);

                usage->type = scalar.type;

                usage->rows = 1U;
                usage->columns = columns;

                if(bytesRemaining > 0)
                {
                  bytesRemaining -= scalar.sizeInBytes;
                  RDCASSERTEQUAL(bytesRemaining, 0);
                  if(usage->children.isEmpty())
                  {
                    const char swizzle[] = "xyzw";
                    usage->children.resize(columns);
                    for(uint32_t x = 0; x < columns; ++x)
                    {
                      usage->children[x].emitSourceVar = usage->emitSourceVar;
                      usage->children[x].name =
                          usage->name + StringFormat::Fmt(".%c", swizzle[RDCMIN(x, 3U)]);
                      usage->children[x].type = scalar.type;
                      usage->children[x].debugVarSSAName = usage->debugVarSSAName;
                      usage->children[x].debugVarComponent = x;
                      usage->children[x].rows = 1U;
                      usage->children[x].columns = 1U;
                      usage->children[x].offset = usage->offset + x;
                    }
                    usage->emitSourceVar = false;
                  }
                  uint32_t col = componentIndex;
                  RDCASSERT(col < columns, col, columns);
                  RDCASSERTEQUAL(usage->children.size(), columns);
                  usage->children[col].debugVarSSAName = mapping.debugVarSSAName;
                  usage->children[col].debugVarComponent = 0;
                  usage->children[col].emitSourceVar = true;

                  // try to recombine vector to a single source var display
                  bool collapseVector = true;
                  for(uint32_t x = 0; x < columns; ++x)
                  {
                    collapseVector = usage->children[x].emitSourceVar;
                    if(!collapseVector)
                      break;
                  }
                  if(collapseVector)
                  {
                    usage->emitSourceVar = true;
                    for(uint32_t x = 0; x < columns; ++x)
                      usage->children[x].emitSourceVar = false;
                  }
                }
                else
                {
                  // Remove mappings : this mapping covers everything
                  usage->debugVarSSAName = mapping.debugVarSSAName;
                  usage->children.clear();
                  usage->emitSourceVar = true;
                  usage->debugVarSuffix.clear();
                }
              }
              else if(bytesRemaining > 0)
              {
                RDCASSERTEQUAL(byteOffset, 0);

                // walk down until we get to a scalar type, if we get there. This means arrays of
                // basic types will get the right type
                while(typeWalk && typeWalk->baseType != NULL && typeWalk->type == VarType::Unknown)
                  typeWalk = &m_DebugInfo.types[typeWalk->baseType];

                usage->type = typeWalk->type;
                usage->debugVarSSAName = mapping.debugVarSSAName;
                usage->debugVarComponent = 0;
                usage->rows = 1U;
                usage->columns = 1U;
                usage->emitSourceVar = true;
                usage->children.clear();
                usage->debugVarSuffix.clear();

                const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];
                bytesRemaining -= scalar.sizeInBytes;
                RDCASSERTEQUAL(bytesRemaining, 0);
              }
            }
          }
        }

        // Phase Two: walk the DebugVarNode tree and convert "emitSourceVar = true" nodes to a SourceVariableMapping
        for(size_t sv = 0; sv < sourceVars.size(); ++sv)
        {
          const DXIL::DILocalVariable *variable = sourceVars[sv];
          DebugVarNode *usage = &roots[variable];
          rdcarray<const DebugVarNode *> nodesToProcess;
          rdcarray<const DebugVarNode *> sourceVarNodes;
          nodesToProcess.push_back(usage);
          while(!nodesToProcess.isEmpty())
          {
            const DebugVarNode *n = nodesToProcess.back();
            nodesToProcess.pop_back();
            if(n->emitSourceVar)
            {
              sourceVarNodes.push_back(n);
            }
            else
            {
              for(size_t x = 0; x < n->children.size(); ++x)
              {
                const DebugVarNode *child = &n->children[x];
                nodesToProcess.push_back(child);
              }
            }
          }
          for(size_t x = 0; x < sourceVarNodes.size(); ++x)
          {
            const DebugVarNode *n = sourceVarNodes[x];
            SourceVariableMapping sourceVar;
            sourceVar.name = n->name;
            sourceVar.type = n->type;
            sourceVar.rows = n->rows;
            sourceVar.columns = n->columns;
            sourceVar.signatureIndex = -1;
            sourceVar.offset = n->offset;
            sourceVar.variables.clear();
            // unknown is treated as a struct
            if(sourceVar.type == VarType::Unknown)
              sourceVar.type = VarType::Struct;

            if(n->children.empty())
            {
              RDCASSERTNOTEQUAL(n->rows * n->columns, 0);
              for(uint32_t c = 0; c < n->rows * n->columns; ++c)
              {
                sourceVar.variables.push_back(DebugVariableReference(
                    DebugVariableType::Variable, n->debugVarSSAName + n->debugVarSuffix, c));
              }
            }
            else
            {
              RDCASSERTEQUAL(n->rows * n->columns, (uint32_t)n->children.count());
              for(int32_t c = 0; c < n->children.count(); ++c)
                sourceVar.variables.push_back(DebugVariableReference(
                    DebugVariableType::Variable,
                    n->children[c].debugVarSSAName + n->children[c].debugVarSuffix,
                    n->children[c].debugVarComponent));
            }

            localSrcVar.sourceVars.push_back(sourceVar);
          }
        }
        program->m_Locals.push_back(localSrcVar);
      }
    }
  }
}

ShaderDebugTrace *Debugger::BeginDebug(uint32_t eventId, const DXBC::DXBCContainer *dxbcContainer,
                                       const ShaderReflection &reflection, uint32_t activeLaneIndex)
{
  ShaderStage shaderStage = reflection.stage;

  m_DXBC = dxbcContainer;
  m_Program = m_DXBC->GetDXILByteCode();
  m_EventId = eventId;
  m_ActiveLaneIndex = activeLaneIndex;
  m_Steps = 0;
  m_Stage = shaderStage;

  // Ensure the DXIL reflection data is built
  DXIL::Program *program = ((DXIL::Program *)m_Program);
  program->BuildReflection();

  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->stage = shaderStage;

  uint32_t workgroupSize = shaderStage == ShaderStage::Pixel ? 4 : 1;
  for(uint32_t i = 0; i < workgroupSize; i++)
    m_Workgroups.push_back(ThreadState(i, *this, m_GlobalState));

  ThreadState &state = GetActiveLane();

  // Create the storage layout for the constant buffers
  // The constant buffer data and details are filled in outside of this method
  size_t count = reflection.constantBlocks.size();
  m_GlobalState.constantBlocks.resize(count);
  for(uint32_t i = 0; i < count; i++)
  {
    const ConstantBlock &cbuffer = reflection.constantBlocks[i];
    uint32_t bindCount = cbuffer.bindArraySize;
    if(bindCount > 1)
    {
      // Create nested structure for constant buffer array
      m_GlobalState.constantBlocks[i].members.resize(bindCount);
    }
  }

  struct ResourceList
  {
    VarType varType;
    DebugVariableType debugVarType;
    DescriptorCategory category;
    ResourceClass resourceClass;
    const rdcarray<ShaderResource> &resources;
    rdcarray<ShaderVariable> &dst;
  };

  // TODO: need to handle SRVs, UAVs, Samplers which are arrays

  // Create the variables for SRVs and UAVs
  ResourceList lists[] = {
      {
          VarType::ReadOnlyResource,
          DebugVariableType::ReadOnlyResource,
          DescriptorCategory::ReadOnlyResource,
          ResourceClass::SRV,
          reflection.readOnlyResources,
          m_GlobalState.readOnlyResources,
      },
      {
          VarType::ReadWriteResource,
          DebugVariableType::ReadWriteResource,
          DescriptorCategory::ReadWriteResource,
          ResourceClass::UAV,
          reflection.readWriteResources,
          m_GlobalState.readWriteResources,
      },
  };

  for(ResourceList &list : lists)
  {
    list.dst.reserve(list.resources.size());
    for(uint32_t i = 0; i < list.resources.size(); i++)
    {
      const ShaderResource &res = list.resources[i];
      // Ignore arrays the debugger execution will mark specific array elements used
      if(res.bindArraySize > 1)
        continue;

      // Fetch the resource name
      BindingSlot slot(res.fixedBindNumber, res.fixedBindSetOrSpace);
      rdcstr name = GetResourceReferenceName(m_Program, list.resourceClass, slot);

      ShaderVariable shaderVar(name, 0U, 0U, 0U, 0U);
      shaderVar.rows = 1;
      shaderVar.columns = 1;
      shaderVar.SetBindIndex(ShaderBindIndex(list.category, i, 0));
      shaderVar.type = list.varType;
      list.dst.push_back(shaderVar);

      SourceVariableMapping sourceVar;
      sourceVar.name = res.name;
      sourceVar.type = list.varType;
      sourceVar.rows = 1;
      sourceVar.columns = 1;
      sourceVar.offset = 0;

      DebugVariableReference ref;
      ref.type = list.debugVarType;
      ref.name = shaderVar.name;
      sourceVar.variables.push_back(ref);

      ret->sourceVars.push_back(sourceVar);
    }
  }

  // Create the variables for Samplers
  count = reflection.samplers.size();
  m_GlobalState.samplers.resize(count);
  for(uint32_t i = 0; i < count; i++)
  {
    ShaderSampler sampler = reflection.samplers[i];
    // Fetch the Sampler name
    BindingSlot slot(sampler.fixedBindNumber, sampler.fixedBindSetOrSpace);
    rdcstr name = GetResourceReferenceName(m_Program, ResourceClass::Sampler, slot);

    ShaderVariable shaderVar(name, 0U, 0U, 0U, 0U);
    shaderVar.rows = 1;
    shaderVar.columns = 1;
    shaderVar.SetBindIndex(ShaderBindIndex(DescriptorCategory::Sampler, i, 0));
    shaderVar.type = VarType::Sampler;
    m_GlobalState.samplers.push_back(shaderVar);

    SourceVariableMapping sourceVar;
    sourceVar.name = sampler.name;
    sourceVar.type = VarType::Sampler;
    sourceVar.rows = 1;
    sourceVar.columns = 1;
    sourceVar.offset = 0;

    DebugVariableReference ref;
    ref.type = DebugVariableType::Sampler;
    ref.name = shaderVar.name;
    sourceVar.variables.push_back(ref);
  }

  for(const DXIL::GlobalVar *gv : m_Program->m_GlobalVars)
  {
    // Ignore DXIL global variables which start with "dx.nothing."
    if(gv->name.beginsWith("dx.nothing."))
      continue;

    GlobalVariable globalVar;
    rdcstr n = DXBC::BasicDemangle(gv->name);
    DXIL::SanitiseName(n);
    globalVar.var.name = n;
    globalVar.id = gv->ssaId;
    state.AllocateMemoryForType(gv->type, globalVar.id, globalVar.var);
    if(gv->flags & GlobalFlags::IsConst)
    {
      if(gv->initialiser)
      {
        const Constant *initialData = gv->initialiser;
        RDCASSERT(ConvertDXILConstantToShaderVariable(initialData, globalVar.var));
        // Write ShaderVariable data back to memory
        auto itAlloc = state.m_MemoryAllocs.find(globalVar.id);
        RDCASSERT(itAlloc != state.m_MemoryAllocs.end());
        const ThreadState::MemoryAlloc &alloc = itAlloc->second;
        void *allocMemoryBackingPtr = alloc.backingMemory;
        size_t allocSize = alloc.size;
        state.UpdateBackingMemoryFromVariable(allocMemoryBackingPtr, allocSize, globalVar.var);
        RDCASSERTEQUAL(allocSize, 0);
      }
    }
    m_GlobalState.globals.push_back(globalVar);
  }

  rdcstr entryPoint = reflection.entryPoint;
  rdcstr entryFunction = m_Program->GetEntryFunction();
  RDCASSERTEQUAL(entryPoint, entryFunction);

  m_EntryPointFunction = NULL;
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external && (f->name == entryFunction))
    {
      m_EntryPointFunction = f;
      break;
    }
  }
  RDCASSERT(m_EntryPointFunction);

  uint32_t globalOffset = 0;
  // Generate helper data per function
  // global instruction offset
  // all SSA Ids referenced
  // minimum and maximum instruction per SSA reference
  // uniform control blocks
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      FunctionInfo &info = m_FunctionInfos[f];
      info.function = f;
      info.globalInstructionOffset = globalOffset;
      uint32_t countInstructions = (uint32_t)f->instructions.size();
      globalOffset += countInstructions;

      ReferencedIds &ssaRefs = info.referencedIds;
      InstructionRangePerId &ssaRange = info.rangePerId;

      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        const Instruction &inst = *(f->instructions[i]);
        if(DXIL::IsDXCNop(inst) || DXIL::IsLLVMDebugCall(inst))
          continue;

        // Allow the variable to live for one instruction longer
        const uint32_t maxInst = i + 1;
        {
          Id resultId = inst.slot;
          if(resultId != DXILDebug::INVALID_ID)
          {
            // The result SSA should not have been referenced before
            RDCASSERTEQUAL(ssaRefs.count(resultId), 0);
            ssaRefs.insert(resultId);

            // For assignment track maximum and minimum (as current instruction plus one)
            auto itRange = ssaRange.find(resultId);
            if(itRange == ssaRange.end())
            {
              ssaRange[resultId] = {i + 1, maxInst};
            }
            else
            {
              itRange->second.min = RDCMIN(i + 1, itRange->second.min);
              itRange->second.max = RDCMAX(maxInst, itRange->second.max);
            }

            // Stack allocations last until the end of the function
            if(inst.op == Operation::Alloca)
              itRange->second.max = countInstructions;
          }
        }
        // Track min and max when SSA is referenced
        bool isPhiNode = (inst.op == Operation::Phi);
        for(uint32_t a = 0; a < inst.args.size(); ++a)
        {
          DXIL::Value *arg = inst.args[a];
          if(DXIL::IsSSA(arg))
          {
            Id argId = GetSSAId(arg);
            // Add GlobalVar args to the SSA refs (they won't be the result of an instruction)
            if(cast<GlobalVar>(arg))
            {
              if(ssaRefs.count(argId) == 0)
                ssaRefs.insert(argId);
            }
            if(!isPhiNode)
            {
              // For non phi-nodes the argument SSA should already exist as the result of a previous operation
              RDCASSERTEQUAL(ssaRefs.count(argId), 1);
            }
            auto itRange = ssaRange.find(argId);
            if(itRange == ssaRange.end())
            {
              ssaRange[argId] = {i, maxInst};
            }
            else
            {
              itRange->second.min = RDCMIN(i, itRange->second.min);
              itRange->second.max = RDCMAX(maxInst, itRange->second.max);
            }
          }
        }
      }
      // If these do not match in size that means there is a result SSA that is never read
      RDCASSERTEQUAL(ssaRefs.size(), ssaRange.size());

      // Find the uniform control blocks in the function
      rdcarray<rdcpair<uint32_t, uint32_t>> links;
      for(const Block *block : f->blocks)
      {
        for(const Block *pred : block->preds)
        {
          if(pred->name.empty())
          {
            uint32_t from = pred->id;
            uint32_t to = block->id;
            links.push_back({from, to});
          }
        }
      }
      DXIL::FindUniformBlocks(links, info.uniformBlocks);
      // Handle de-generate case when a single block
      if(info.uniformBlocks.empty())
      {
        RDCASSERTEQUAL(f->blocks.size(), 1);
        info.uniformBlocks.push_back(f->blocks[0]->id);
      }
    }
  }

  ParseDebugData();

  // Add inputs to the shader trace
  // Use the DXIL reflection data to map the input signature to input variables
  const EntryPointInterface *entryPointIf = NULL;
  for(size_t e = 0; e < m_Program->m_EntryPointInterfaces.size(); ++e)
  {
    if(entryFunction == m_Program->m_EntryPointInterfaces[e].name)
    {
      entryPointIf = &m_Program->m_EntryPointInterfaces[e];
      break;
    }
  }
  RDCASSERT(entryPointIf);
  m_EntryPointInterface = entryPointIf;
  const rdcarray<EntryPointInterface::Signature> &inputs = m_EntryPointInterface->inputs;

  // TODO: compute coverage from DXIL
  const bool inputCoverage = false;
  const uint32_t countInParams = (uint32_t)inputs.size();

  if(countInParams || inputCoverage)
  {
    // Make fake ShaderVariable struct to hold all the inputs
    ShaderVariable &inStruct = state.m_Input;
    inStruct.name = DXIL_FAKE_INPUT_STRUCT_NAME;
    inStruct.rows = 1;
    inStruct.columns = 1;
    inStruct.type = VarType::Struct;
    inStruct.members.resize(countInParams + (inputCoverage ? 1 : 0));

    const rdcarray<SigParameter> &dxbcInParams = dxbcContainer->GetReflection()->InputSig;
    for(uint32_t i = 0; i < countInParams; ++i)
    {
      const EntryPointInterface::Signature &sig = inputs[i];

      ShaderVariable &v = inStruct.members[i];

      // Get the name from the DXBC reflection
      SigParameter sigParam;
      if(FindSigParameter(dxbcInParams, sig, sigParam))
      {
        v.name = sigParam.semanticIdxName;
      }
      else
      {
        v.name = sig.name;
      }
      v.rows = (uint8_t)sig.rows;
      v.columns = (uint8_t)sig.cols;
      v.type = VarTypeForComponentType(sig.type);

      SourceVariableMapping inputMapping;
      inputMapping.name = v.name;
      inputMapping.type = v.type;
      inputMapping.rows = sig.rows;
      inputMapping.columns = sig.cols;
      inputMapping.variables.reserve(sig.cols);
      inputMapping.signatureIndex = sig.startRow;
      for(uint32_t c = 0; c < sig.cols; ++c)
      {
        DebugVariableReference ref;
        ref.type = DebugVariableType::Input;
        ref.name = inStruct.name + "." + v.name;
        ref.component = c;
        inputMapping.variables.push_back(ref);
      }

      // Put the coverage mask at the end
      if(inputCoverage)
      {
        // TODO
        inStruct.members.back() = ShaderVariable("TODO_COVERAGE", 0U, 0U, 0U, 0U);
        inStruct.members.back().columns = 1;

        // TODO: handle the input of system values
        if(false)
        {
          SourceVariableMapping sourcemap;
          sourcemap.name = "SV_Coverage";
          sourcemap.type = VarType::UInt;
          sourcemap.rows = 1;
          sourcemap.columns = 1;
          // no corresponding signature element for this - maybe we should generate one?
          sourcemap.signatureIndex = -1;
          DebugVariableReference ref;
          ref.type = DebugVariableType::Input;
          ref.name = inStruct.members.back().name;
          sourcemap.variables.push_back(ref);
        }
      }
    }

    // Make a single source variable mapping for the whole input struct
    SourceVariableMapping inputMapping;
    inputMapping.name = inStruct.name;
    inputMapping.type = VarType::Struct;
    inputMapping.rows = 1;
    inputMapping.columns = 1;
    inputMapping.variables.resize(1);
    inputMapping.variables.push_back(DebugVariableReference(DebugVariableType::Input, inStruct.name));
    ret->sourceVars.push_back(inputMapping);
  }

  const rdcarray<SigParameter> &dxbcOutParams = dxbcContainer->GetReflection()->OutputSig;
  const rdcarray<EntryPointInterface::Signature> &outputs = m_EntryPointInterface->outputs;
  uint32_t countOutputs = (uint32_t)outputs.size();

  // Make fake ShaderVariable struct to hold all the outputs
  ShaderVariable &outStruct = state.m_Output.var;
  outStruct.name = DXIL_FAKE_OUTPUT_STRUCT_NAME;
  outStruct.rows = 1;
  outStruct.columns = 1;
  outStruct.type = VarType::Struct;
  outStruct.members.resize(countOutputs);
  state.m_Output.id = m_Program->m_NextSSAId;

  for(uint32_t i = 0; i < countOutputs; ++i)
  {
    const EntryPointInterface::Signature &sig = outputs[i];

    ShaderVariable &v = outStruct.members[i];

    // Get the name from the DXBC reflection
    SigParameter sigParam;
    if(FindSigParameter(dxbcOutParams, sig, sigParam))
    {
      v.name = sigParam.semanticIdxName;
    }
    else
    {
      v.name = sig.name;
    }
    v.rows = (uint8_t)sig.rows;
    v.columns = (uint8_t)sig.cols;
    v.type = VarTypeForComponentType(sig.type);
    // TODO: ShaderBuiltin::DepthOutput, ShaderBuiltin::DepthOutputLessEqual,
    // ShaderBuiltin::DepthOutputGreaterEqual, ShaderBuiltin::MSAACoverage,
    // ShaderBuiltin::StencilReference

    // Map the high level variables to the Output DXBC Signature
    SourceVariableMapping outputMapping;
    outputMapping.name = v.name;
    outputMapping.type = v.type;
    outputMapping.rows = sig.rows;
    outputMapping.columns = sig.cols;
    outputMapping.variables.reserve(sig.cols);
    outputMapping.signatureIndex = i;
    for(uint32_t c = 0; c < sig.cols; ++c)
    {
      DebugVariableReference ref;
      ref.type = DebugVariableType::Variable;
      ref.name = outStruct.name + "." + v.name;
      ref.component = c;
      outputMapping.variables.push_back(ref);
    }
    ret->sourceVars.push_back(outputMapping);

    // TODO: handle the output of system values
    if(0)
    {
      SourceVariableMapping sourcemap;

      if(sigParam.systemValue == ShaderBuiltin::DepthOutput)
      {
        sourcemap.name = "SV_Depth";
        sourcemap.type = VarType::Float;
      }
      else if(sigParam.systemValue == ShaderBuiltin::DepthOutputLessEqual)
      {
        sourcemap.name = "SV_DepthLessEqual";
        sourcemap.type = VarType::Float;
      }
      else if(sigParam.systemValue == ShaderBuiltin::DepthOutputGreaterEqual)
      {
        sourcemap.name = "SV_DepthGreaterEqual";
        sourcemap.type = VarType::Float;
      }
      else if(sigParam.systemValue == ShaderBuiltin::MSAACoverage)
      {
        sourcemap.name = "SV_Coverage";
        sourcemap.type = VarType::UInt;
      }
      else if(sigParam.systemValue == ShaderBuiltin::StencilReference)
      {
        sourcemap.name = "SV_StencilRef";
        sourcemap.type = VarType::UInt;
      }

      // all these variables are 1 scalar component
      sourcemap.rows = 1;
      sourcemap.columns = 1;
      sourcemap.signatureIndex = sig.startRow;
      DebugVariableReference ref;
      ref.type = DebugVariableType::Variable;
      ref.name = v.name;
      sourcemap.variables.push_back(ref);
      ret->sourceVars.push_back(sourcemap);
    }
  }

  if(0)
  {
    // Make a single source variable mapping for the whole output struct
    SourceVariableMapping outputMapping;
    outputMapping.name = state.m_Output.var.name;
    outputMapping.type = VarType::Struct;
    outputMapping.rows = 1;
    outputMapping.columns = 1;
    outputMapping.variables.resize(1);
    outputMapping.variables[0].name = state.m_Output.var.name;
    outputMapping.variables[0].type = DebugVariableType::Variable;
    ret->sourceVars.push_back(outputMapping);
  }

  // Global source variable mappings valid for lifetime of the debug session
  for(const GlobalVariable &gv : m_GlobalState.globals)
  {
    SourceVariableMapping outputMapping;
    outputMapping.name = gv.var.name;
    outputMapping.type = gv.var.type;
    outputMapping.rows = RDCMAX(1U, (uint32_t)gv.var.rows);
    outputMapping.columns = RDCMAX(1U, (uint32_t)gv.var.columns);
    outputMapping.variables.resize(1);
    outputMapping.variables[0].name = gv.var.name;
    outputMapping.variables[0].type = DebugVariableType::Variable;
    ret->sourceVars.push_back(outputMapping);
  }

  // Per instruction all source variable mappings at this instruction (cumulative and complete)
  // InstructionSourceInfo
  // {
  //   uint32_t instruction;
  //   LineColumnInfo lineInfo;
  //   {
  //     uint32_t disassemblyLine = 0;
  //     int32_t fileIndex = -1;
  //     uint32_t lineStart = 0;
  //     uint32_t lineEnd = 0;
  //     uint32_t colStart = 0;
  //     uint32_t colEnd = 0;
  //   }
  //   rdcarray<SourceVariableMapping> sourceVars;
  //   {
  //     rdcstr name;
  //     VarType type = VarType::Unknown;
  //     uint32_t rows = 0;
  //     uint32_t columns = 0;
  //     uint32_t offset;
  //     int32_t signatureIndex = -1;
  //     rdcarray<DebugVariableReference> variables;
  //     {
  //       rdcstr name;
  //       DebugVariableType type = DebugVariableType::Undefined;
  //       uint32_t component = 0;
  //     }
  //   }
  // }
  // ret->instInfo.push_back(InstructionSourceInfo())

  ret->inputs = {state.m_Input};
  ret->inputs.append(state.m_Input.members);
  ret->constantBlocks = m_GlobalState.constantBlocks;
  ret->readOnlyResources = m_GlobalState.readOnlyResources;
  ret->readWriteResources = m_GlobalState.readWriteResources;
  ret->samplers = m_GlobalState.samplers;
  ret->debugger = this;

  // Add the output struct to the global state
  if(countOutputs)
    m_GlobalState.globals.push_back(state.m_Output);

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
    for(const GlobalVariable &gv : m_GlobalState.globals)
      initial.changes.push_back({ShaderVariable(), gv.var});

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
    for(size_t lane = 0; lane < m_Workgroups.size(); lane++)
    {
      if(activeMask[lane])
        m_Workgroups[lane].StepOverNopInstructions();
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
