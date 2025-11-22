/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2025 Baldur Karlsson
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
#include "common/threading.h"
#include "core/settings.h"
#include "maths/formatpacking.h"
#include "replay/common/var_dispatch_helpers.h"
#include "shaders/controlflow.h"

RDOC_CONFIG(bool, D3D12_DXILShaderDebugger_Logging, false,
            "Debug logging for the DXIL shader debugger");

RDOC_CONFIG(bool, D3D12_DXILShaderDebugger_EnableMT, true,
            "Use multiple threads to run the shader debugger simulation.");

RDOC_DEBUG_CONFIG(bool, D3D12_Hack_ShaderDebugUsesJobSystemJobs, false,
                  "Use individual job system jobs to run shader debugging simulation.");

#if ENABLED(RDOC_RELEASE)
#define CHECK_DEBUGGER_THREAD() \
  do                            \
  {                             \
  } while((void)0, 0)
#else
#define CHECK_DEBUGGER_THREAD() \
  RDCASSERTMSG("Debugger function called from non-device thread!", IsDeviceThread());
#endif    // #if ENABLED(RDOC_RELEASE)

#if ENABLED(RDOC_RELEASE)
#define THREADSTATE_CHECK_DEBUGGER_THREAD() \
  do                                        \
  {                                         \
  } while((void)0, 0)
#else
#define THREADSTATE_CHECK_DEBUGGER_THREAD() \
  RDCASSERTMSG("Function called from non-debugger thread!", m_Debugger.IsDeviceThread());
#endif    // #if ENABLED(RDOC_RELEASE)

using namespace rdcshaders;

// TODO: Extend support for Compound Constants: arithmetic, logical ops
// TODO: Assert m_Block in ThreadState is correct per instruction
// TODO: Automatically execute phi instructions after a branch
// TODO: Support MSAA
// TODO: Support UAVs with counter
// TODO: Extend debug data parsing: DW_TAG_array_type for the base element type
// TODO: Extend debug data parsing: N-dimensional arrays, mapping covers whole sub-array
// TODO: Load's from Pointers should re-read the memory to get accurate values
// TODO: Change Pointers to be GPUPointers : update pointers aliased to base pointers
// TODO: Unify and share the code for reading and writing thru pointers

// Notes:
//   The phi node capture variables are not shown in the UI
//   LLVM poison values are not supported
//   Does it make sense to use ShaderVariable GPU pointers
//   ExtractVal: only handles one index
//   ComputeDXILTypeByteSize does not consider byte alignment
//   GetElementPtr: only handles two indexes
//   Sample*: Argument 10 which is called Clamp is not used
//   ShuffleVector: mask entries might be undef meaning "don't care"

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

const uint32_t POINTER_MAGIC = 0xBEAFDEAF;

static bool IsFloatingPointType(VarType type)
{
  switch(type)
  {
    case VarType::Double:
    case VarType::Float:
    case VarType::Half: return true;
    default: return false;
  }
}

static bool IsSignedIntegerType(VarType type)
{
  switch(type)
  {
    case VarType::SLong:
    case VarType::SInt:
    case VarType::SShort: return true;
    case VarType::SByte: return true;
    default: return false;
  }
}

static bool IsUnsignedIntegerType(VarType type)
{
  switch(type)
  {
    case VarType::ULong:
    case VarType::UInt:
    case VarType::UShort: return true;
    case VarType::UByte: return true;
    default: return false;
  }
}

static bool IsIntegerType(VarType type)
{
  return IsSignedIntegerType(type) || IsUnsignedIntegerType(type);
}

static bool IsEncodedPointer(const ShaderVariable &var)
{
  if(var.type != VarType::GPUPointer)
  {
    return false;
  }
  if(var.value.u32v[1] != POINTER_MAGIC)
  {
    return false;
  }
  return true;
}

static void EncodePointer(DXILDebug::Id ptrId, uint64_t offset, uint64_t size, VarType baseType,
                          ShaderVariable &var)
{
  var.type = VarType::GPUPointer;
  var.value.u32v[0] = ptrId;
  var.value.u32v[1] = POINTER_MAGIC;
  var.value.u64v[1] = offset;
  var.value.u64v[2] = size;
  var.value.u64v[3] = (uint64_t)baseType;
}

static bool DecodePointer(DXILDebug::Id &ptrId, uint64_t &offset, uint64_t &size, VarType &baseType,
                          const ShaderVariable &var)
{
  if(!IsEncodedPointer(var))
  {
    RDCERR("Calling DecodePointer on non encoded pointer");
    return false;
  }

  ptrId = var.value.u32v[0];
  offset = var.value.u64v[1];
  size = var.value.u64v[2];
  baseType = (VarType)var.value.u64v[3];
  return true;
}

static void SetFloatValue(float val, ShaderVariable &var)
{
  for(uint8_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(T) comp<T>(var, c) = val;

    IMPL_FOR_FLOAT_TYPES(_IMPL);
  }
}

static void SetUIntValue(uint64_t val, ShaderVariable &var)
{
  for(uint8_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(var, c) = U(val);

    IMPL_FOR_INT_TYPES(_IMPL);
  }
}

static void SetIntValue(int64_t val, ShaderVariable &var)
{
  for(uint8_t c = 0; c < var.columns; c++)
  {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(var, c) = I(val);

    IMPL_FOR_INT_TYPES(_IMPL);
  }
}

static void SetShaderValue(float fVal, uint64_t uVal, int64_t iVal, ShaderVariable &var)
{
  switch(var.type)
  {
    case VarType::Half:
    case VarType::Float:
    case VarType::Double: SetFloatValue(fVal, var); break;
    case VarType::SByte:
    case VarType::SShort:
    case VarType::SInt:
    case VarType::SLong: SetIntValue(iVal, var); break;
    case VarType::UByte:
    case VarType::UShort:
    case VarType::UInt:
    case VarType::ULong: SetUIntValue(uVal, var); break;
    default: RDCERR("Unknown type %s", ToStr(var.type).c_str());
  }
}

static void SetShaderValueZero(ShaderVariable &var)
{
  SetShaderValue(0.0f, 0, 0, var);
}

static void SetShaderValueOne(ShaderVariable &var)
{
  SetShaderValue(1.0f, 1, 1, var);
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

static void ClearAnnotatedHandle(ShaderVariable &var)
{
  var.value.u32v[15] = 0;
}

static void SetAnnotatedHandle(ShaderVariable &var)
{
  var.value.u32v[15] = 1;
}

static bool IsAnnotatedHandle(const ShaderVariable &var)
{
  return (var.value.u32v[15] == 1);
}

static ShaderEvents AssignValue(ShaderVariable &result, bool flushDenorm)
{
  ShaderEvents flags = ShaderEvents::NoEvent;

  if(result.type == VarType::Float)
  {
    float ft = result.value.f32v[0];
    if(!RDCISFINITE(ft))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }
  else if(result.type == VarType::Double)
  {
    double dt = result.value.f64v[0];
    if(!RDCISFINITE(dt))
      flags |= ShaderEvents::GeneratedNanOrInf;
  }

  if(flushDenorm)
  {
    if(result.type == VarType::Float)
      result.value.f32v[0] = flush_denorm(result.value.f32v[0]);
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

static uint8_t GetShaderVariableElementByteSize(const ShaderVariable &var)
{
  if(var.members.empty())
    return GetElementByteSize(var.type);
  return GetShaderVariableElementByteSize(var.members[0]);
}

static void UpdateShaderVariableFromBackingMemory(ShaderVariable &var, const void *ptr)
{
  // Memory copy from backing memory to base memory variable
  size_t elementSize = GetShaderVariableElementByteSize(var);
  const uint8_t *src = (const uint8_t *)ptr;
  if(var.members.size() == 0)
  {
    RDCASSERTEQUAL(var.rows, 1);
    RDCASSERTEQUAL(var.columns, 1);
    if(elementSize <= sizeof(ShaderValue))
      memcpy(&var.value, src, elementSize);
    else
      RDCERR("Updating MemoryVariable elementSize %u too large max %u", elementSize,
             sizeof(ShaderValue));
  }
  else
  {
    for(uint32_t i = 0; i < var.members.size(); ++i)
    {
      if(elementSize <= sizeof(ShaderValue))
        memcpy(&var.members[i].value, src, elementSize);
      else
        RDCERR("Updating MemoryVariable member %u elementSize %u too large max %u", i, elementSize,
               sizeof(ShaderValue));
      src += elementSize;
    }
  }
}

static DXBC::ResourceRetType ConvertComponentTypeToResourceRetType(const ComponentType compType)
{
  switch(compType)
  {
    // Treat 16-bit integer as 32-bit and do the conversion after resource access
    case ComponentType::I16:
    case ComponentType::I32: return DXBC::ResourceRetType::RETURN_TYPE_SINT;
    case ComponentType::U16:
    case ComponentType::U32: return DXBC::ResourceRetType::RETURN_TYPE_UINT;
    case ComponentType::F32: return DXBC::ResourceRetType::RETURN_TYPE_FLOAT;
    case ComponentType::F64: return DXBC::ResourceRetType::RETURN_TYPE_DOUBLE;
    case ComponentType::SNormF32: return DXBC ::ResourceRetType::RETURN_TYPE_SNORM;
    case ComponentType::UNormF32: return DXBC::ResourceRetType::RETURN_TYPE_UNORM;
    // Treat 16-bit float as 32-bit and do the conversion after resource access
    case ComponentType::SNormF16:
    case ComponentType::UNormF16:
    case ComponentType::F16: return DXBC::ResourceRetType::RETURN_TYPE_FLOAT;
    case ComponentType::I1:
    case ComponentType::I64:
    case ComponentType::U64:
    case ComponentType::SNormF64:
    case ComponentType::UNormF64:
      RDCERR("Unhandled component type %s", ToStr(compType).c_str());
      return DXBC::ResourceRetType::RETURN_TYPE_UNKNOWN;
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
      var.rows = 0;
      var.columns = 0;
      var.type = VarType::Unknown;
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
      if(constant->op == Operation::NoOp)
      {
        RDCASSERT(ConvertDXILValueToShaderValue(value, var.type, 0, var.value));
        return true;
      }
      else if(constant->op == Operation::GetElementPtr)
      {
        const rdcarray<DXIL::Value *> &members = constant->getMembers();
        RDCASSERT(members.size() >= 3, members.size());
        value = members[0];
        const GlobalVar *gv = cast<GlobalVar>(value);
        if(!gv)
        {
          RDCERR("Constant GetElementPtr first member is not a GlobalVar");
          return false;
        }
        if(gv->type->type != Type::TypeKind::Pointer)
        {
          RDCERR("Constant GetElementPtr global variable is not a Pointer");
          return false;
        }
        const DXIL::Type *elementType = constant->type;
        if(elementType->type != Type::TypeKind::Pointer)
        {
          RDCERR("Constant variable is not a Pointer");
          return false;
        }
        elementType = elementType->inner;
        VarType baseType = ConvertDXILTypeToVarType(elementType);
        uint32_t elementSize = GetElementByteSize(baseType);
        uint32_t countElems = RDCMAX(1U, elementType->elemCount);
        uint64_t size = countElems * elementSize;

        DXILDebug::Id ptrId = gv->ssaId;
        // members[1..] : indices 1...N
        rdcarray<uint64_t> indexes;
        indexes.reserve(members.size() - 1);
        for(uint32_t a = 1; a < members.size(); ++a)
        {
          value = members[a];
          VarType argType = ConvertDXILTypeToVarType(value->type);
          ShaderValue argValue;
          memset(&argValue, 0, sizeof(argValue));
          RDCASSERT(ConvertDXILValueToShaderValue(value, argType, 0, argValue));
          indexes.push_back(argValue.u64v[0]);
        }
        // Index 0 is in ptr terms as if pointer was an array of pointers
        RDCASSERTEQUAL(indexes[0], 0);
        uint64_t offset = 0;

        if(indexes.size() > 1)
          offset += indexes[1] * elementSize;
        RDCASSERT(indexes.size() <= 2);
        // Encode the pointer allocation: ptrId, offset, size, baseType
        EncodePointer(ptrId, offset, size, baseType, var);
        return true;
      }
      // case Operation::Trunc:
      // case Operation::ZExt:
      // case Operation::SExt:
      // case Operation::FToU:
      // case Operation::FToS:
      // case Operation::UToF:
      // case Operation::SToF:
      // case Operation::FPTrunc:
      // case Operation::FPExt:
      // case Operation::PtrToI:
      // case Operation::IToPtr:
      // case Operation::Bitcast:
      // case Operation::AddrSpaceCast:
      // case Operation::Select:
      // case Operation::IEqual:
      // plus other integer comparisons
      // case Operation::FOrdEqual:
      // plus other fp comparisons
      // case Operation::ExtractElement:
      // case Operation::ExtractVal:
      // case Operation::FAdd:
      // case Operation::FSub:
      // case Operation::FMul:
      // case Operation::FDiv:
      // case Operation::FRem:
      // case Operation::Add:
      // case Operation::Sub:
      // case Operation::Mul:
      // case Operation::UDiv:
      // case Operation::SDiv:
      // case Operation::URem:
      // case Operation::SRem:
      // case Operation::ShiftLeft:
      // case Operation::LogicalShiftRight:
      // case Operation::ArithShiftRight:
      // case Operation::And:
      // case Operation::Or:
      // case Operation::Xor:
      RDCERR("Unsupported Constant Op %s", ToStr(constant->op).c_str());
      return false;
    }
    return false;
  }
  // Struct: rows = 0, columns = 0 : var.members is structure members
  // Array: rows = 0, columns == 0 : var.members is array elements
  if(constant->isCompound())
  {
    const rdcarray<DXIL::Value *> &members = constant->getMembers();
    RDCASSERT(members.size() == var.members.size());
    for(size_t i = 0; i < var.members.size(); ++i)
    {
      const Constant *c = cast<Constant>(members[i]);
      if(c)
        RDCASSERT(ConvertDXILConstantToShaderVariable(c, var.members[i]));
      else
        RDCASSERT(ConvertDXILValueToShaderValue(members[i], var.members[i].type, 0,
                                                var.members[i].value));
    }
    return true;
  }
  else if(constant->isNULL())
  {
    // Nothing to do because Shader Variables created initialised to zero
    return true;
  }
  return false;
}

static bool ConvertShaderVariableStructToVector(const DXIL::Type *dxilType, ShaderVariable &var)
{
  // convert struct of four members, to be a vector
  if(var.type != VarType::Struct)
  {
    RDCERR("Expected type %s to be a struct", ToStr(var.type).c_str());
    return false;
  }

  // convert the DXIL return type to full variable
  ShaderVariable structVar;
  ConvertDXILTypeToShaderVariable(dxilType, structVar);

  RDCASSERT(structVar.members.size() <= 4);
  if(structVar.members.size() > 4)
  {
    RDCERR("Expected struct size %u to be four or less", structVar.members.size());
    return false;
  }

  for(uint32_t i = 0; i < structVar.members.size(); ++i)
  {
    if(structVar.members[i].type != structVar.members[0].type)
    {
      RDCERR("Expected struct member type to be the same for all members %s != %s",
             ToStr(structVar.members[i].type).c_str(), ToStr(structVar.members[0].type).c_str());
      return false;
    }
  }

  var.type = structVar.members[0].type;
  var.rows = 1;
  var.columns = (uint8_t)structVar.members.size();
  return true;
}

static size_t ComputeDXILTypeByteSize(const Type *type)
{
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

void ConvertTypeToViewFormat(const DXIL::Type *type, DXILDebug::ViewFmt &fmt)
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

  fmt.compType = CompType::Typeless;
  if(resType->type == Type::Scalar)
  {
    fmt.numComps = compCount;
    fmt.byteWidth = resType->bitWidth / 8;
    fmt.stride = fmt.byteWidth * fmt.numComps;
    if(resType->scalarType == Type::ScalarKind::Int)
    {
      if(resType->bitWidth == 32)
        fmt.compType = CompType::SInt;
    }
    else if(resType->scalarType == Type::ScalarKind::Float)
    {
      if(resType->bitWidth == 32)
        fmt.compType = CompType::Float;
    }
  }
  else if(resType->type == Type::Struct)
  {
    fmt.numComps = 0;
    fmt.byteWidth = 0;
    fmt.stride = 0;
  }
}

static void FillViewFmtFromVarType(VarType type, DXILDebug::ViewFmt &fmt)
{
  switch(type)
  {
    case VarType::Float:
      fmt.byteWidth = 4;
      fmt.compType = CompType::Float;
      break;
    case VarType::Double:
      fmt.byteWidth = 8;
      fmt.compType = CompType::Float;
      break;
    case VarType::Half:
      fmt.byteWidth = 2;
      fmt.compType = CompType::Float;
      break;
    case VarType::SInt:
      fmt.byteWidth = 4;
      fmt.compType = CompType::SInt;
      break;
    case VarType::UInt:
      fmt.byteWidth = 4;
      fmt.compType = CompType::UInt;
      break;
    case VarType::SShort:
      fmt.byteWidth = 2;
      fmt.compType = CompType::SInt;
      break;
    case VarType::UShort:
      fmt.byteWidth = 2;
      fmt.compType = CompType::UInt;
      break;
    case VarType::SLong:
      fmt.byteWidth = 8;
      fmt.compType = CompType::SInt;
      break;
    case VarType::ULong:
      fmt.byteWidth = 2;
      fmt.compType = CompType::UInt;
      break;
    case VarType::SByte:
      fmt.byteWidth = 1;
      fmt.compType = CompType::SInt;
      break;
    case VarType::UByte:
      fmt.byteWidth = 1;
      fmt.compType = CompType::UInt;
      break;
    default: RDCERR("Unhandled Result Type %s", ToStr(type).c_str()); break;
  }
}

namespace DXILDebug
{
bool ExecPointReference::IsAfter(const ExecPointReference &from,
                                 const DXIL::ControlFlow &controlFlow) const
{
  if(block == from.block)
    return instruction > from.instruction;
  return controlFlow.IsConnected(from.block, block);
}

void ResourceReferenceInfo::Create(const DXIL::ResourceReference *resRef, uint32_t arrayIndex)
{
  resClass = resRef->resourceBase.resClass;
  binding = BindingSlot(resRef->resourceBase.regBase + arrayIndex, resRef->resourceBase.space);
  switch(resClass)
  {
    case DXIL::ResourceClass::SRV:
    {
      srvData.dim = (DXDebug::ResourceDimension)ConvertResourceKindToResourceDimension(
          resRef->resourceBase.srvData.shape);
      srvData.sampleCount = resRef->resourceBase.srvData.sampleCount;
      srvData.compType = (DXDebug::ResourceRetType)ConvertComponentTypeToResourceRetType(
          resRef->resourceBase.srvData.compType);
      varType = VarType::ReadOnlyResource;

      switch(resRef->resourceBase.srvData.shape)
      {
        default:
          RDCERR("Unexpected resource shape");
          descType = DescriptorType::Unknown;
          break;
        case ResourceKind::Texture1D:
        case ResourceKind::Texture2D:
        case ResourceKind::Texture2DMS:
        case ResourceKind::Texture3D:
        case ResourceKind::TextureCube:
        case ResourceKind::Texture1DArray:
        case ResourceKind::Texture2DArray:
        case ResourceKind::Texture2DMSArray:
        case ResourceKind::TextureCubeArray:
        case ResourceKind::FeedbackTexture2D:
        case ResourceKind::FeedbackTexture2DArray: descType = DescriptorType::Image; break;
        case ResourceKind::TypedBuffer:
        case ResourceKind::TBuffer: descType = DescriptorType::TypedBuffer; break;
        case ResourceKind::RawBuffer:
        case ResourceKind::StructuredBuffer:
        case ResourceKind::StructuredBufferWithCounter: descType = DescriptorType::Buffer; break;
        case ResourceKind::RTAccelerationStructure:
          descType = DescriptorType::AccelerationStructure;
          break;
      }
      break;
    }
    case DXIL::ResourceClass::UAV:
    {
      varType = VarType::ReadWriteResource;

      switch(resRef->resourceBase.uavData.shape)
      {
        default:
          RDCERR("Unexpected resource shape");
          descType = DescriptorType::Unknown;
          break;
        case ResourceKind::Texture1D:
        case ResourceKind::Texture2D:
        case ResourceKind::Texture2DMS:
        case ResourceKind::Texture3D:
        case ResourceKind::TextureCube:
        case ResourceKind::Texture1DArray:
        case ResourceKind::Texture2DArray:
        case ResourceKind::Texture2DMSArray:
        case ResourceKind::TextureCubeArray:
        case ResourceKind::FeedbackTexture2D:
        case ResourceKind::FeedbackTexture2DArray: descType = DescriptorType::ReadWriteImage; break;
        case ResourceKind::TypedBuffer:
        case ResourceKind::TBuffer: descType = DescriptorType::ReadWriteTypedBuffer; break;
        case ResourceKind::RawBuffer:
        case ResourceKind::StructuredBuffer:
        case ResourceKind::StructuredBufferWithCounter:
          descType = DescriptorType::ReadWriteBuffer;
          break;
        case ResourceKind::RTAccelerationStructure:
          descType = DescriptorType::AccelerationStructure;
          break;
      }
      break;
    }
    case DXIL::ResourceClass::CBuffer:
    {
      varType = VarType::ConstantBlock;
      descType = DescriptorType::ConstantBuffer;
      break;
    }
    case DXIL::ResourceClass::Sampler:
    {
      samplerData.samplerMode =
          ConvertSamplerKindToSamplerMode(resRef->resourceBase.samplerData.samplerType);
      varType = VarType::Sampler;
      descType = DescriptorType::Sampler;
      break;
    }
    default: RDCERR("Unexpected resource class %s", ToStr(resClass).c_str()); break;
  }
}

void MemoryTracking::AllocateMemoryForType(const DXIL::Type *type, Id allocId, bool globalVar,
                                           bool gsm, ShaderVariable &var)
{
  RDCASSERTEQUAL(type->type, Type::TypeKind::Pointer);
  ConvertDXILTypeToShaderVariable(type->inner, var);

  // Add the SSA to m_Allocations with its backing memory and size
  size_t byteSize = ComputeDXILTypeByteSize(type->inner);
  void *backingMem = malloc(byteSize);
  memset(backingMem, 0, byteSize);
  // Invalid: gsm = false and globalVar = true
  RDCASSERT(!gsm || globalVar);
  m_Allocations[allocId] = {backingMem, byteSize, globalVar, gsm, !globalVar};

  // Create a pointer to represent this allocation
  m_Pointers[allocId] = {allocId, backingMem, byteSize};
}

void MemoryTracking::ConvertGlobalAllocToLocal(Id allocId)
{
  MemoryTracking::Allocation &alloc = m_Allocations[allocId];
  RDCASSERT(alloc.globalVarAlloc);
  RDCASSERT(!alloc.localMemory);
  void *globalBackingMemory = alloc.backingMemory;
  const size_t allocSize = (size_t)alloc.size;
  void *localBackingMemory = malloc(allocSize);
  memcpy(localBackingMemory, globalBackingMemory, allocSize);
  alloc.backingMemory = localBackingMemory;
  alloc.localMemory = true;

  // Update the pointer for the base memory allocation
  MemoryTracking::Pointer &allocPtr = m_Pointers[allocId];
  allocPtr.memory = localBackingMemory;
  RDCASSERTEQUAL(allocPtr.baseMemoryId, allocId);
  RDCASSERTEQUAL(allocPtr.size, alloc.size);

  // Update pointers which pointed to within the memory allocation
  for(auto &itPtr : m_Pointers)
  {
    MemoryTracking::Pointer &ptr = itPtr.second;
    Id baseMemoryId = ptr.baseMemoryId;
    if(baseMemoryId != allocId)
      continue;

    Id ptrId = itPtr.first;
    // pointers for backing allocations have already have been updated
    if(ptrId == baseMemoryId)
      continue;

    ptrdiff_t offset = (uintptr_t)ptr.memory - (uintptr_t)globalBackingMemory;
    if(offset < 0)
    {
      RDCERR("Invalid memory allocation offset ptrId %u BaseMemoryId %u", ptrId, baseMemoryId);
      continue;
    }
    void *localMemory = (void *)((uintptr_t)localBackingMemory + offset);
    RDCASSERTEQUAL(ptr.baseMemoryId, baseMemoryId);
    ptr.memory = localMemory;
  }
}

// Must be called from the replay manager thread (the debugger thread)
ThreadState::ThreadState(Debugger &debugger, const GlobalState &globalState, uint32_t maxSSAId,
                         uint32_t laneIndex, uint32_t numThreads)
    : m_Debugger(debugger),
      m_GlobalState(globalState),
      m_Program(debugger.GetProgram()),
      m_MaxSSAId(maxSSAId),
      m_WorkgroupIndex(laneIndex)
{
  THREADSTATE_CHECK_DEBUGGER_THREAD();
  m_ShaderType = m_Program.GetShaderType();
  m_Assigned.resize(maxSSAId);
  m_Live.resize(maxSSAId);
  m_Variables.resize(maxSSAId);
  m_ActiveMask.resize(numThreads);
}

// Must be called from the replay manager thread (the debugger thread)
ThreadState::~ThreadState()
{
  THREADSTATE_CHECK_DEBUGGER_THREAD();
  for(auto it : m_Memory.m_Allocations)
  {
    if(it.second.localMemory)
      free(it.second.backingMemory);
  }
}

bool ThreadState::Finished() const
{
  return m_Dead || m_Ended || m_Callstack.empty();
}

bool ThreadState::InUniformBlock() const
{
  return m_FunctionInfo->uniformBlocks.contains(m_Block);
}

// Must run on the device thread for the active simulation thread
void ThreadState::ProcessScopeChange(const rdcarray<bool> &oldLive, const rdcarray<bool> &newLive)
{
  // nothing to do if we aren't tracking into a state
  if(!m_HasDebugState)
    return;

  THREADSTATE_CHECK_DEBUGGER_THREAD();

  // all oldLive (except globals) are going out of scope. all newLive (except globals) are coming
  // into scope

  const rdcarray<bool> &liveGlobals = m_Debugger.GetLiveGlobals();

  for(uint32_t id = 0; id < oldLive.size(); id++)
  {
    if(liveGlobals[id])
      continue;

    m_PendingDebugState.changes.push_back({m_Variables[id]});
  }

  for(uint32_t id = 0; id < newLive.size(); id++)
  {
    if(liveGlobals[id])
      continue;

    m_PendingDebugState.changes.push_back({ShaderVariable(), m_Variables[id]});
  }
}

// Must run on the device thread for the active simulation thread
void ThreadState::EnterFunction(const Function *function, const rdcarray<Value *> &args)
{
  if(m_HasDebugState)
    THREADSTATE_CHECK_DEBUGGER_THREAD();

  StackFrame *frame = new StackFrame(function);
  m_FunctionInstructionIdx = 0;
  m_FunctionInfo = m_Debugger.GetFunctionInfo(function);

  // if there's a previous stack frame, save its live list
  if(!m_Callstack.empty())
  {
    // process the outgoing scope
    ProcessScopeChange(m_Live, {});
    m_Callstack.back()->live = m_Live;
  }

  // start with just globals
  m_Live = m_Debugger.GetLiveGlobals();
  m_IsGlobal = m_Live;

  m_Block = 0;
  m_PreviousBlock = ~0U;
  m_PhiVariables.clear();

  m_ActiveGlobalInstructionIdx = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
  m_Callstack.push_back(frame);

  StepOverNopInstructions();
}

// Must be called from the replay manager thread (the debugger thread)
void ThreadState::EnterEntryPoint(const Function *function, bool hasDebugState)
{
  THREADSTATE_CHECK_DEBUGGER_THREAD();
  m_HasDebugState = hasDebugState;

  EnterFunction(function, {});

  for(const GlobalVariable &gv : m_GlobalState.globals)
  {
    m_Variables[gv.id] = gv.var;
    m_Assigned[gv.id] = true;
  }
  for(const GlobalConstant &c : m_GlobalState.constants)
  {
    m_Variables[c.id] = c.var;
    m_Assigned[c.id] = true;
  }

  // Start with the global memory allocations
  m_Memory = m_GlobalState.memory;

  // Create per thread memory for non-groupshared global memory
  for(auto &it : m_Memory.m_Allocations)
  {
    Id allocId = it.first;
    MemoryTracking::Allocation &alloc = it.second;

    RDCASSERT(alloc.globalVarAlloc);
    if(!alloc.gsm)
      m_Memory.ConvertGlobalAllocToLocal(allocId);
  }

  // active lane : needs it own local backing memory for GSM
  if(m_HasDebugState)
  {
    for(Id id : m_GlobalState.groupSharedMemoryIds)
      m_Memory.ConvertGlobalAllocToLocal(id);
  }

  m_HasDebugState = false;
  UpdateCurrentInstruction();
}

void ThreadState::FillCallstack(ShaderDebugState &state)
{
  if(m_FunctionInfo->callstacks.size() == 1)
  {
    state.callstack = m_FunctionInfo->callstacks.begin()->second;
    return;
  }

  auto it = m_FunctionInfo->callstacks.upper_bound(state.nextInstruction);
  if(it == m_FunctionInfo->callstacks.end())
  {
    state.callstack.clear();
    state.callstack.push_back(m_FunctionInfo->function->name);
    return;
  }

  if(it != m_FunctionInfo->callstacks.begin())
    --it;

  if(it->first <= m_FunctionInstructionIdx)
  {
    state.callstack = it->second;
  }
  else
  {
    state.callstack.clear();
    state.callstack.push_back(m_FunctionInfo->function->name);
    return;
  }
}

bool IsNopInstruction(const Instruction &inst)
{
  if(inst.op == Operation::Call)
  {
    const Function *callFunc = inst.getFuncCall();
    if(callFunc->family == FunctionFamily::LLVMDbg)
      return true;
    if(callFunc->family == FunctionFamily::LLVMInstrinsic)
      return true;
  }

  if(inst.op == Operation::NoOp)
    return true;

  return false;
}

bool ThreadState::JumpToBlock(const Block *target, bool divergencePoint)
{
  m_PreviousBlock = m_Block;
  m_PhiVariables.clear();
  auto it = m_FunctionInfo->phiReferencedIdsPerBlock.find(m_PreviousBlock);
  if(it != m_FunctionInfo->phiReferencedIdsPerBlock.end())
  {
    const FunctionInfo::ReferencedIds &phiIds = it->second;
    for(Id id : phiIds)
      m_PhiVariables[id] = m_Variables[id];
  }

  RDCASSERT(target);
  uint32_t blockId = target->id;
  if(blockId < m_FunctionInfo->function->blocks.size())
  {
    m_Block = blockId;
    m_FunctionInstructionIdx = m_FunctionInfo->function->blocks[m_Block]->startInstructionIdx;
  }
  else
  {
    return false;
  }

  uint32_t nextInstruction = m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
  if(m_HasDebugState && !m_Ended)
    m_PendingDebugState.nextInstruction = nextInstruction;

  m_EnteredPoints.push_back(m_Block);
  RDCASSERTEQUAL(m_FunctionInfo->divergentBlocks.contains(m_PreviousBlock), divergencePoint);
  if(divergencePoint)
  {
    m_Diverged = true;
    RDCASSERTEQUAL(m_ConvergencePoint, INVALID_EXECUTION_POINT);
    for(const ConvergentBlockData &data : m_FunctionInfo->convergentBlocks)
    {
      if(data.first == m_PreviousBlock)
      {
        m_ConvergencePoint = data.second;
        break;
      }
    }
    RDCASSERTNOTEQUAL(m_ConvergencePoint, INVALID_EXECUTION_POINT);

    RDCASSERT(m_PartialConvergencePoints.empty());
    for(const PartialConvergentBlockData &data : m_FunctionInfo->partialConvergentBlocks)
    {
      if(data.first == m_PreviousBlock)
      {
        m_PartialConvergencePoints = data.second;
        RDCASSERT(!m_PartialConvergencePoints.empty());
        break;
      }
    }
  }

  return true;
}

uint32_t ThreadState::GetSubgroupActiveLanes(const rdcarray<ThreadState> &workgroup,
                                             rdcarray<uint32_t> &activeLanes) const
{
  const uint32_t firstLaneInSub = m_WorkgroupIndex - m_SubgroupIdx;
  for(uint32_t lane = firstLaneInSub; lane < firstLaneInSub + m_GlobalState.subgroupSize; lane++)
  {
    RDCASSERT(lane < m_ActiveMask.size(), lane, m_ActiveMask.size());
    // wave operations exclude helpers
    if(m_ActiveMask[lane])
    {
      RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
      if(!m_GlobalState.waveOpsIncludeHelpers && workgroup[lane].m_Helper)
        continue;
      activeLanes.push_back(lane);
    }
  }
  return firstLaneInSub;
}

bool ThreadState::ExecuteInstruction(const rdcarray<ThreadState> &workgroup)
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
  m_Program.GetSSAName(resultId, result.name);
  result.rows = 1;
  result.columns = 1;
  result.type = ConvertDXILTypeToVarType(retType);
  result.value.u64v[0] = 0;
  result.value.u64v[1] = 0;
  result.value.u64v[2] = 0;
  result.value.u64v[3] = 0;

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

            const ShaderVariable &var = m_Input.members[inputIdx];
            if(var.rows == 0)
              RDCASSERT(rowIdx < var.members.size(), rowIdx, var.members.size());

            const ShaderVariable &a = (var.rows != 0) ? var : var.members[rowIdx];
            RDCASSERT(colIdx < a.columns, colIdx, a.columns);
            const uint32_t c = colIdx;

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
            if(m_HasDebugState)
            {
              ShaderVariable &var = m_Output.var.members[outputIdx];
              if(var.rows == 0)
                RDCASSERT(rowIdx < var.members.size(), rowIdx, var.members.size());

              ShaderVariable &a = (var.rows != 0) ? var : var.members[rowIdx];
              RDCASSERT(colIdx < a.columns, colIdx, a.columns);
              const uint32_t c = colIdx;
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
            bool annotatedHandle;
            ShaderVariable handleVar;
            ResourceReferenceInfo resRefInfo = GetResource(handleId, annotatedHandle, handleVar);
            if(!resRefInfo.Valid())
              break;

            BindingSlot binding(resRefInfo.binding);
            ShaderVariable data;
            uint32_t mipLevel = 0;
            if(!isUndef(inst.args[2]))
            {
              ShaderVariable arg;
              RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
              mipLevel = arg.value.u32v[0];
            }
            if(m_Debugger.GetResourceInfo(resRefInfo.resClass, binding, mipLevel, data) ==
               DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            MarkResourceAccess(handleVar);

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
            bool annotatedHandle;
            ShaderVariable handleVar;
            ResourceReferenceInfo resRefInfo = GetResource(handleId, annotatedHandle, handleVar);
            if(!resRefInfo.Valid())
              break;

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            const char *opString = ToStr(dxOpCode).c_str();
            ShaderVariable data;
            if(m_Debugger.GetSampleInfo(resRefInfo.resClass, resRefInfo.binding, opString, data) ==
               DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            MarkResourceAccess(handleVar);

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
            ShaderVariable data;
            if(m_Debugger.GetRenderTargetSampleInfo(opString, data) == DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            result.value.u32v[0] = data.value.u32v[0];
            break;
          }
          case DXOp::RenderTargetGetSamplePosition:
          {
            const char *opString = ToStr(dxOpCode).c_str();
            ShaderVariable data;
            if(m_Debugger.GetRenderTargetSampleInfo(opString, data) == DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
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
          case DXOp::CalculateLOD:
          {
            if(IsPendingResultReady())
            {
              const ShaderVariable &data = GetPendingResult();
              ConvertSampleGatherReturn(dxOpCode, inst, data, result);
              if(dxOpCode == DXOp::CalculateLOD)
              {
                // clamped is in arg 6
                ShaderVariable arg;
                RDCASSERT(GetShaderVariable(inst.args[6], opCode, dxOpCode, arg, false));
                // CalculateSampleGather returns {CalculateLevelOfDetail(), CalculateLevelOfDetailUnclamped()}
                if(arg.value.u32v[0] == 0)
                  result.value.u32v[0] = data.value.u32v[1];
              }
              eventFlags |= ShaderEvents::SampleLoadGather;
              break;
            }
            Id handleId = GetArgumentId(1);
            bool annotatedHandle;
            ShaderVariable handleVar;
            ResourceReferenceInfo resRefInfo = GetResource(handleId, annotatedHandle, handleVar);
            if(!resRefInfo.Valid())
              break;
            MarkResourceAccess(handleVar);

            if(!PerformGPUResourceOp(workgroup, opCode, dxOpCode, resRefInfo, inst, result))
              break;

            DXIL_DEBUG_RDCASSERT(IsPendingResultPending());
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
            ShaderVariable handleVar;
            ResourceReferenceInfo resRefInfo = GetResource(handleId, annotatedHandle, handleVar);
            if(!resRefInfo.Valid())
              break;

            ResourceClass resClass = resRefInfo.resClass;
            // SRV TextureLoad is done on the GPU
            if((dxOpCode == DXOp::TextureLoad) && (resClass == ResourceClass::SRV))
            {
              if(IsPendingResultReady())
              {
                const ShaderVariable &data = GetPendingResult();
                ConvertSampleGatherReturn(dxOpCode, inst, data, result);
                MarkResourceAccess(handleVar);
                eventFlags |= ShaderEvents::SampleLoadGather;
              }
              else
              {
                if(PerformGPUResourceOp(workgroup, opCode, dxOpCode, resRefInfo, inst, result))
                {
                  DXIL_DEBUG_RDCASSERT(IsPendingResultPending());
                }
              }
              break;
            }

            const bool load = (dxOpCode == DXOp::TextureLoad) || (dxOpCode == DXOp::BufferLoad) ||
                              (dxOpCode == DXOp::RawBufferLoad);
            const Type *baseType = NULL;
            uint32_t resultNumComps = 0;
            ShaderVariable arg;
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

              // TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
              // BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
              // RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)

              // get the mask
              int maskIndex = 0;
              if(dxOpCode == DXOp::TextureStore)
                maskIndex = 9;
              else if(dxOpCode == DXOp::BufferStore)
                maskIndex = 8;
              else if(dxOpCode == DXOp::RawBufferStore)
                maskIndex = 8;
              else
                RDCERR("Unexpected store opcode %u", dxOpCode);

              uint32_t mask = 1;
              if(GetShaderVariable(inst.args[maskIndex], opCode, dxOpCode, arg))
                mask = arg.value.u32v[0];

              if(mask == 0)
                mask = 1;

              resultNumComps = 32 - Bits::CountLeadingZeroes(mask);

              RDCASSERTEQUAL(mask, (1U << resultNumComps) - 1U);
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
            bool texData = false;
            uint32_t rowPitch = 0;
            uint32_t depthPitch = 0;
            uint32_t firstElem = 0;
            uint32_t numElems = 0;
            ViewFmt fmt;

            // Helper lanes can't modify UAVs
            if(!load && (resClass == ResourceClass::UAV) && m_Helper)
              break;

            RDCASSERT((resClass == ResourceClass::SRV || resClass == ResourceClass::UAV), resClass);
            ResourceInfo resInfo;
            DeviceOpResult opResult = DeviceOpResult::Unknown;
            const BindingSlot &slot = resRefInfo.binding;
            switch(resClass)
            {
              case ResourceClass::UAV:
              {
                UAVInfo uav;
                opResult = m_Debugger.GetUAV(slot, uav);
                if(opResult == DeviceOpResult::Succeeded)
                {
                  resInfo = uav.resInfo;
                  texData = uav.tex;
                  rowPitch = uav.rowPitch;
                  depthPitch = uav.depthPitch;
                }
                break;
              }
              case ResourceClass::SRV:
              {
                SRVInfo srv;
                opResult = m_Debugger.GetSRV(slot, srv);
                if(opResult == DeviceOpResult::Succeeded)
                {
                  resInfo = srv.resInfo;
                }
                break;
              }
              default: RDCERR("Unexpected ResourceClass %s", ToStr(resClass).c_str()); break;
            }
            if(opResult == DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            MarkResourceAccess(handleVar);
            // Unbound resource
            if(!resInfo.hasData)
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

            firstElem = resInfo.firstElement;
            numElems = resInfo.numElements;
            fmt = resInfo.format;

            bool byteAddress = resInfo.isByteBuffer;

            // If the format is unknown, guess it using the result type
            // See FetchSRV(), FetchUAV() comment about root buffers being typeless
            // The stride should have been computed from the shader metadata
            if(fmt.compType == CompType::Typeless)
            {
              FillViewFmtFromVarType(result.type, fmt);
              fmt.numComps = result.columns;
            }

            if(byteAddress)
              fmt.stride = 1;

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
            }

            uint32_t stride = fmt.stride;
            RDCASSERTNOTEQUAL(stride, 0);
            RDCASSERTNOTEQUAL(fmt.compType, CompType::Typeless);

            uint64_t dataOffset = 0;
            uint32_t texCoords[3] = {0, 0, 0};
            uint32_t elemIdx = 0;
            if((dxOpCode == DXOp::BufferLoad) || (dxOpCode == DXOp::RawBufferLoad) ||
               (dxOpCode == DXOp::RawBufferStore) || (dxOpCode == DXOp::BufferStore))
            {
              // BufferLoad(res,index,wot)
              // BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
              // RawBufferLoad(srv,index,elementOffset,mask,alignment)
              // RawBufferStore(uav,index,elementOffset,value0,value1,value2,value3,mask,alignment)
              if(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg))
                elemIdx = arg.value.u32v[0];
              if(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg))
                dataOffset = arg.value.u64v[0];
              if(texData)
              {
                texCoords[0] = elemIdx;
                texCoords[1] = (uint32_t)dataOffset;
              }
            }
            else if((dxOpCode == DXOp::TextureLoad) || (dxOpCode == DXOp::TextureStore))
            {
              RDCASSERT(texData);
              // TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
              // TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
              size_t offsetStart = (dxOpCode == DXOp::TextureLoad) ? 3 : 2;
              if(GetShaderVariable(inst.args[offsetStart], opCode, dxOpCode, arg))
                texCoords[0] = (int8_t)arg.value.u32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 1], opCode, dxOpCode, arg))
                texCoords[1] = (int8_t)arg.value.u32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 2], opCode, dxOpCode, arg))
                texCoords[2] = (int8_t)arg.value.u32v[0];
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

            if(texData)
            {
              dataOffset += texCoords[0] * stride;
              dataOffset += texCoords[1] * rowPitch;
              dataOffset += texCoords[2] * depthPitch;
            }
            else
            {
              dataOffset += (firstElem + elemIdx) * stride;
              dataOffset += structOffset;
            }

            const uint64_t dataSize = resInfo.dataSize;
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
              int numComps = fmt.numComps;
              int maxNumComps = fmt.numComps;
              // Clamp the number of components to read based on the amount of data in the buffer
              if(!texData)
              {
                RDCASSERTNOTEQUAL(numElems, 0);
                const int maxNumCompsData = (int)((dataSize - dataOffset) / fmt.byteWidth);
                size_t maxOffset = (firstElem + numElems) * stride + structOffset;
                const int maxNumCompsOffset = (int)((maxOffset - dataOffset) / fmt.byteWidth);
                maxNumComps = RDCMIN(maxNumCompsData, maxNumCompsOffset);
                fmt.numComps = RDCMIN(fmt.numComps, maxNumComps);
              }

              // For stores load the whole data, update the component, save the whole data back
              // This is to support per component writes to packed formats
              result.value = m_Debugger.TypedResourceLoad(resClass, slot, fmt, dataOffset);

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
                const uint32_t valueStart = (dxOpCode == DXOp::TextureStore) ? 5 : 4;
                const uint32_t numArgs = RDCMIN(4, maxNumComps);
                for(uint32_t c = 0; c < numArgs; ++c)
                {
                  if(!isUndef(inst.args[c + valueStart]) &&
                     GetShaderVariable(inst.args[c + valueStart], opCode, dxOpCode, arg))
                  {
                    const uint32_t dstComp = c;
                    const uint32_t srcComp = 0;
                    result.value.u32v[dstComp] = arg.value.u32v[srcComp];
                    ++numComps;
                  }
                }
                fmt.numComps = RDCMIN(numComps, maxNumComps);
                m_Debugger.TypedResourceStore(resClass, slot, fmt, dataOffset, result.value);
              }
            }
            break;
          }
          case DXOp::CreateHandleFromHeap:
          {
            // CreateHandleFromHeap(index,samplerHeap,nonUniformIndex)
            // Make the ShaderVariable to represent the direct heap access binding
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            uint32_t descriptorIndex = arg.value.u32v[0];
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            bool samplerHeap = arg.value.u32v[0] != 0;
            HeapDescriptorType heapType =
                samplerHeap ? HeapDescriptorType::Sampler : HeapDescriptorType::CBV_SRV_UAV;

            // convert the direct heap access binding into ResourceReferenceIndo
            BindingSlot slot(heapType, descriptorIndex);
            ResourceReferenceInfo resRefInfo;
            if(m_Debugger.GetResourceReferenceInfo(slot, resRefInfo) == DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            ShaderDirectAccess access;
            if(m_Debugger.GetShaderDirectAccess(resRefInfo.descType, slot, access) ==
               DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            RDCASSERT(m_DirectHeapAccessBindings.count(resultId) == 0);
            m_DirectHeapAccessBindings[resultId] = resRefInfo;

            // Default to unannotated handle
            ClearAnnotatedHandle(result);
            result.type = resRefInfo.varType;
            result.SetDirectAccess(access);
            break;
          }
          case DXOp::AnnotateHandle:
          {
            // AnnotateHandle(res,props)
            rdcstr resultSSAName = result.name;
            rdcstr baseResource;
            Id baseResourceId = GetSSAId(inst.args[1]);
            m_Program.GetSSAName(baseResourceId, baseResource);

            ShaderVariable resource;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, resource));
            rdcstr resName;
            if(resource.IsDirectAccess())
            {
              resName = result.name;
              // Update m_DirectHeapAccessBindings for the annotated handle
              // to use the data from the source resource
              RDCASSERT(m_DirectHeapAccessBindings.count(baseResourceId) > 0);
              RDCASSERT(m_DirectHeapAccessBindings.count(resultId) == 0);
              m_DirectHeapAccessBindings[resultId] = m_DirectHeapAccessBindings.at(baseResourceId);
            }
            else
            {
              resName = baseResource;
            }
            result = resource;
            result.name = resultSSAName;

            // Parse the packed annotate handle properties
            // resKind : {compType, compCount} | {structStride}
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

            // Set as an annotated handle
            SetAnnotatedHandle(result);

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
            else if(resKind == ResourceKind::CBuffer)
            {
              // Create the cbuffer handle reference for the annotated handle
              auto it = m_ConstantBlockHandles.find(baseResourceId);
              if(it != m_ConstantBlockHandles.end())
              {
                m_ConstantBlockHandles[resultId] = it->second;
              }
              else
              {
                RDCERR("Annotated handle resName:%s %s has no cbuffer handle reference %u",
                       resName.c_str(), baseResource.c_str(), baseResourceId);
              }
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
            break;
          }
          case DXOp::CreateHandle:
          case DXOp::CreateHandleFromBinding:
          {
            // CreateHandle(resourceClass,rangeId,index,nonUniformIndex
            // CreateHandleFromBinding(bind,index,nonUniformIndex)
            rdcstr resultSSAName = result.name;
            uint32_t resIndexArgId = ~0U;
            if(dxOpCode == DXOp::CreateHandle)
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

              rdcstr resName = Debugger::GetResourceBaseName(&m_Program, resRef);

              const rdcarray<ShaderVariable> &resources = *list;
              result.name.clear();
              size_t constantBlockIndex = ~0U;
              for(uint32_t i = 0; i < resources.size(); ++i)
              {
                if(resources[i].name == resName)
                {
                  constantBlockIndex = i;
                  result = resources[i];
                  break;
                }
              }
              if(result.name.isEmpty())
              {
                if((resRef->resourceBase.resClass == ResourceClass::SRV) ||
                   (resRef->resourceBase.resClass == ResourceClass::UAV) ||
                   (resRef->resourceBase.resClass == ResourceClass::Sampler))
                {
                  if(resIndexArgId < inst.args.size())
                  {
                    // Make the ShaderVariable to represent the dynamic binding
                    // The base binding exists : array index is in argument "resIndexArgId"
                    ShaderVariable arg;
                    RDCASSERT(GetShaderVariable(inst.args[resIndexArgId], opCode, dxOpCode, arg));
                    uint32_t arrayIndex = arg.value.u32v[0];
                    RDCASSERT(arrayIndex >= resRef->resourceBase.regBase);
                    arrayIndex -= resRef->resourceBase.regBase;
                    bool isSRV = (resRef->resourceBase.resClass == ResourceClass::SRV);
                    DescriptorCategory category = isSRV ? DescriptorCategory::ReadOnlyResource
                                                        : DescriptorCategory::ReadWriteResource;
                    result.SetBindIndex(ShaderBindIndex(category, resRef->resourceIndex, arrayIndex));
                    result.type = isSRV ? VarType::ReadOnlyResource : VarType::ReadWriteResource;
                    // Default to unannotated handle
                    ClearAnnotatedHandle(result);
                  }
                  else
                  {
                    RDCERR("Unhandled dynamic handle %s with invalid resIndexArgId",
                           resName.c_str(), resIndexArgId);
                  }
                }
                else
                {
                  RDCERR("Unknown resource handle %s class %s", resName.c_str(),
                         ToStr(resRef->resourceBase.resClass).c_str());
                }
              }
              else
              {
                if(resRef->resourceBase.resClass == ResourceClass::CBuffer)
                {
                  uint32_t arrayIndex = 0;
                  // Look up the correct cbuffer variable for cbuffer arrays
                  if(resRef->resourceBase.regCount > 1)
                  {
                    if(resIndexArgId < inst.args.size())
                    {
                      ShaderVariable arg;
                      RDCASSERT(GetShaderVariable(inst.args[resIndexArgId], opCode, dxOpCode, arg));
                      arrayIndex = arg.value.u32v[0];
                      RDCASSERT(arrayIndex >= resRef->resourceBase.regBase);
                      if(arrayIndex >= resRef->resourceBase.regBase)
                      {
                        arrayIndex -= resRef->resourceBase.regBase;
                        RDCASSERT(arrayIndex < result.members.size(), arrayIndex,
                                  result.members.size());
                        if(arrayIndex < result.members.size())
                          RDCASSERT(!result.members[arrayIndex].members.empty());
                      }
                    }
                    else
                    {
                      RDCERR("Unhandled cbuffer handle %s with invalid resIndexArgId",
                             resName.c_str(), resIndexArgId);
                    }
                  }
                  // Create the cbuffer handle reference
                  ConstantBlockReference constantBlockRef = {constantBlockIndex, arrayIndex};
                  m_ConstantBlockHandles[resultId] = constantBlockRef;

                  // Create cbuffer struct variable of N uint4's : N comes from resource metadata
                  const size_t structSize = AlignUp16(resRef->resourceBase.cbufferData.sizeInBytes);
                  ShaderVariable cbufferVar;
                  cbufferVar.members.resize(structSize / 16);
                  cbufferVar.type = VarType::Struct;
                  cbufferVar.name = resultSSAName;
                  for(size_t i = 0; i < cbufferVar.members.size(); ++i)
                  {
                    ShaderVariable &var = cbufferVar.members[i];
                    var.type = VarType::UInt;
                    var.columns = 4;
                    var.name = StringFormat::Fmt("%s[%u]", resultSSAName.c_str(), i);
                    var.rows = 1;
                    // Initialise to 0xCC to aid determinism and show unset values
                    memset(&var.value, 0XCC, sizeof(var.value));
                  }

                  // Memory copy from the cbuffer data into the cbuffer variable
                  auto it = m_GlobalState.constantBlocksDatas.find(constantBlockRef);
                  if(it != m_GlobalState.constantBlocksDatas.end())
                  {
                    const bytebuf &cbufferData = it->second;
                    if(cbufferData.size() != 0)
                    {
                      size_t offset = 0;
                      const byte *data = cbufferData.data();
                      for(size_t i = 0; i < cbufferVar.members.size(); ++i)
                      {
                        ShaderVariable &var = cbufferVar.members[i];
                        size_t countBytes = RDCMIN((size_t)16, cbufferData.size() - offset);
                        memcpy(var.value.u32v.data(), data + offset, countBytes);
                        offset += 16;
                        if(offset >= cbufferData.size())
                          break;
                      }
                    }
                    else
                    {
                      RDCERR("No data for constant block %s", result.name.c_str());
                    }
                  }
                  else
                  {
                    RDCERR("Can't find the data for constant block %s", result.name.c_str());
                  }
                  result = cbufferVar;
                }
              }
            }
            else
            {
              RDCERR("Unknown Base Resource for %s", resultSSAName.c_str());
            }
            result.name = resultSSAName;
            break;
          }
          case DXOp::CBufferLoadLegacy:
          {
            // CBufferLoadLegacy(handle,regIndex)
            Id handleId = GetArgumentId(1);
            if(handleId == DXILDebug::INVALID_ID)
              break;

            // Find the cbuffer variable from the handleId
            if(!IsVariableAssigned(handleId))
            {
              RDCERR("Unknown cbuffer handle %u", handleId);
              break;
            }

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            uint32_t regIndex = arg.value.u32v[0];

            RDCASSERT(m_Live[handleId]);
            const ShaderVariable &handleVar = m_Variables[handleId];

            result.value.u32v[0] = 0;
            result.value.u32v[1] = 0;
            result.value.u32v[2] = 0;
            result.value.u32v[3] = 0;
            auto constantBlockRefIt = m_ConstantBlockHandles.find(handleId);
            if(constantBlockRefIt != m_ConstantBlockHandles.end())
            {
              const ConstantBlockReference &constantBlockRef = constantBlockRefIt->second;
              auto it = m_GlobalState.constantBlocksDatas.find(constantBlockRef);
              if(it != m_GlobalState.constantBlocksDatas.end())
              {
                const bytebuf &cbufferData = it->second;
                const uint32_t bufferSize = (uint32_t)cbufferData.size();
                const uint32_t maxIndex = AlignUp16(bufferSize) / 16;
                RDCASSERTMSG("Out of bounds cbuffer load", regIndex < maxIndex, regIndex, maxIndex);
                if(regIndex < maxIndex)
                {
                  const uint32_t dataOffset = regIndex * 16;
                  const uint32_t byteWidth = 4;
                  const byte *base = cbufferData.data() + dataOffset;
                  const uint32_t *data = (const uint32_t *)base;
                  const uint32_t numComps = RDCMIN(4U, (bufferSize - dataOffset) / byteWidth);
                  for(uint32_t c = 0; c < numComps; c++)
                    result.value.u32v[c] = data[c];
                }
              }
              else
              {
                RDCERR("Failed to find data for constant block data for %s", handleVar.name.c_str());
              }
            }
            else
            {
              RDCERR("Failed to find data for cbuffer %s", handleVar.name.c_str());
            }

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
            if(IsPendingResultReady())
            {
              result = GetPendingResult();
            }
            else
            {
              ShaderVariable arg;
              RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
              QueueMathOp(dxOpCode, arg, result);
            }
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
            RDCASSERT(IsFloatingPointType(arg.type));
            RDCASSERTEQUAL(result.type, arg.type);
            const uint32_t c = 0;
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = fabs(comp<T>(arg, c));

            IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, arg.type);
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
            RDCASSERT(IsSignedIntegerType(a.type));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(result.type, a.type);
            const uint32_t c = 0;
            if(dxOpCode == DXOp::IMin)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = RDCMIN(comp<S>(a, c), comp<S>(b, c));

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
            }
            else if(dxOpCode == DXOp::IMax)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, c) = RDCMAX(comp<S>(a, c), comp<S>(b, c));
              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
            }
            break;
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
            // LLVM does not make unsigned types
            RDCASSERT(IsIntegerType(a.type));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(result.type, a.type);
            const uint32_t c = 0;
            if(dxOpCode == DXOp::UMin)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = RDCMIN(comp<U>(a, c), comp<U>(b, c));

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
            }
            else if(dxOpCode == DXOp::UMax)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, c) = RDCMAX(comp<U>(a, c), comp<U>(b, c));
              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);
            }
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
            RDCASSERT(IsFloatingPointType(a.type));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(result.type, a.type);
            const uint32_t c = 0;
            if(dxOpCode == DXOp::FMin)
            {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = dxbc_min(comp<T>(a, c), comp<T>(b, c));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, result.type);
            }
            else if(dxOpCode == DXOp::FMax)
            {
#undef _IMPL
#define _IMPL(T) comp<T>(result, c) = dxbc_max(comp<T>(a, c), comp<T>(b, c));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, result.type);
            }
            break;
          }
          case DXOp::Fma:
          case DXOp::FMad:
          {
            // FMa(a,b,c) : fused
            // FMad(a,b,c) : not fused
            // Treat fused and not fused as the same
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERT(IsFloatingPointType(a.type));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(a.type, c.type);
            RDCASSERTEQUAL(result.type, a.type);
            if(result.type == VarType::Double)
            {
              result.value.f64v[0] = a.value.f64v[0] * b.value.f64v[0] + c.value.f64v[0];
            }
            else if(result.type == VarType::Float)
            {
              const double fma = ((double)(a.value.f32v[0]) * (double)(b.value.f32v[0])) +
                                 (double)(c.value.f32v[0]);
              result.value.f32v[0] = (float)fma;
            }
            else if(result.type == VarType::Half)
            {
              const double fma = ((double)ConvertFromHalf(a.value.u16v[0]) *
                                  (double)ConvertFromHalf(b.value.u16v[0])) +
                                 (double)ConvertFromHalf(c.value.u16v[0]);
              result.value.u16v[0] = ConvertToHalf((float)fma);
            }
            break;
          }
          case DXOp::Saturate:
          {
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            RDCASSERT(IsFloatingPointType(arg.type));
            RDCASSERTEQUAL(result.type, arg.type);
            if(result.type == VarType::Double)
            {
              result.value.f64v[0] = dxbc_min(1.0, dxbc_max(0.0, arg.value.f64v[0]));
              break;
            }
            else if(result.type == VarType::Float)
            {
              result.value.f32v[0] = dxbc_min(1.0f, dxbc_max(0.0f, arg.value.f32v[0]));
              break;
            }
            else if(result.type == VarType::Half)
            {
              result.value.u16v[0] =
                  ConvertToHalf(dxbc_min(1.0f, dxbc_max(0.0f, ConvertFromHalf(arg.value.u16v[0]))));
              break;
            }
            break;
          }
          case DXOp::Dot2:
          case DXOp::Dot3:
          case DXOp::Dot4:
          {
            // 2/3/4 Vector
            // Result type must match input types
            uint32_t numComps = 4;
            uint32_t argAStart = 1;
            if(dxOpCode == DXOp::Dot2)
              numComps = 2;
            else if(dxOpCode == DXOp::Dot3)
              numComps = 3;
            uint32_t argBStart = argAStart + numComps;

            result.value.s64v[0] = 0L;
            for(uint32_t c = 0; c < numComps; ++c)
            {
              ShaderVariable a;
              ShaderVariable b;
              RDCASSERT(GetShaderVariable(inst.args[argAStart + c], opCode, dxOpCode, a));
              RDCASSERT(GetShaderVariable(inst.args[argBStart + c], opCode, dxOpCode, b));
              RDCASSERTEQUAL(a.type, b.type);
              RDCASSERTEQUAL(result.type, a.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<I>(result, 0) += (comp<I>(a, 0) * comp<I>(b, 0));
              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, result.type);

#undef _IMPL
#define _IMPL(T) comp<T>(result, 0) += (comp<T>(a, 0) * comp<T>(b, 0));

              IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, result.type);
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
            result.value.u32v[0] =
                GetBuiltin(ShaderBuiltin::DispatchThreadIndex).value.u32v[component];
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
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::GroupIndex).value.u32v[component];
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
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::GroupThreadIndex).value.u32v[component];
            break;
          }
          case DXOp::FlattenedThreadIdInGroup:
          {
            // FlattenedThreadIdInGroup()->SV_GroupIndex
            RDCASSERTEQUAL(result.type, VarType::SInt);
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::GroupFlatIndex).value.u32v[0];
            break;
          }
          case DXOp::DerivCoarseX:
          case DXOp::DerivCoarseY:
          case DXOp::DerivFineX:
          case DXOp::DerivFineY:
          {
            if(m_ShaderType != DXBC::ShaderType::Pixel || workgroup.size() < 4)
            {
              RDCERR("Undefined results using derivative instruction outside of a pixel shader.");
            }
            else
            {
              RDCASSERT(!QuadIsDiverged(workgroup, m_QuadNeighbours));
              if(dxOpCode == DXOp::DerivCoarseX)
                result.value = DDX(false, opCode, dxOpCode, workgroup, inst.args[1]);
              else if(dxOpCode == DXOp::DerivCoarseY)
                result.value = DDY(false, opCode, dxOpCode, workgroup, inst.args[1]);
              else if(dxOpCode == DXOp::DerivFineX)
                result.value = DDX(true, opCode, dxOpCode, workgroup, inst.args[1]);
              else if(dxOpCode == DXOp::DerivFineY)
                result.value = DDY(true, opCode, dxOpCode, workgroup, inst.args[1]);
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
            const uint32_t col = 0;

            if(dxOpCode == DXOp::IMul)
            {
              // 32-bit operands to produce 64-bit result
              result.value.s64v[col] = (int64_t)a.value.s32v[col] * (int64_t)b.value.s32v[col];
            }
            else if(dxOpCode == DXOp::UMul)
            {
              // 32-bit operands to produce 64-bit result
              result.value.u64v[col] = (uint64_t)a.value.u32v[col] * (uint64_t)b.value.u32v[col];
            }
            else if(dxOpCode == DXOp::UDiv)
            {
              // destQUOT, destREM = UDiv(src0, src1);
              if(b.value.u32v[0] != 0)
              {
                result.value.u32v[0] = a.value.u32v[0] / b.value.u32v[0];
                result.value.u32v[1] = a.value.u32v[0] - (result.value.u32v[0] * b.value.u32v[0]);
              }
              else
              {
                // Divide by zero returns 0xffffffff for both quotient and remainder
                result.value.u32v[0] = 0xffffffff;
                result.value.u32v[1] = 0xffffffff;
                eventFlags |= ShaderEvents::GeneratedNanOrInf;
              }
            }
            break;
          }
          case DXOp::IMad:
          case DXOp::UMad:
          {
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(a.type, c.type);
            const uint32_t col = 0;
            if(dxOpCode == DXOp::IMad)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(result, col) = comp<S>(a, col) * comp<S>(b, col) + comp<S>(c, col)

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
            }
            else if(dxOpCode == DXOp::UMad)
            {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(result, col) = comp<U>(a, col) * comp<U>(b, col) + comp<U>(c, col)

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
              RDCASSERT(!WorkgroupIsDiverged(workgroup));
            if(barrierMode & BarrierMode::TGSMFence)
              ExecuteMemoryBarrier();
            break;
          }
          case DXOp::Discard:
          {
            ShaderVariable cond;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, cond));
            if(cond.value.u32v[0] != 0)
            {
              // Active lane is demoted to helper invocation which for pixel debug terminates the debug
              if(m_HasDebugState)
              {
                m_Dead = true;
                return true;
              }
            }
            break;
          }
          case DXOp::LegacyF32ToF16:
          {
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(retType->scalarType, Type::Int);
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            result.value.u16v[0] = ConvertToHalf(arg.value.f32v[0]);
            break;
          }
          case DXOp::LegacyF16ToF32:
          {
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(retType->scalarType, Type::Float);
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            result.value.f32v[0] = ConvertFromHalf(arg.value.u16v[0]);
            break;
          }
          case DXOp::LegacyDoubleToFloat:
          case DXOp::LegacyDoubleToSInt32:
          case DXOp::LegacyDoubleToUInt32:
          {
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 64);
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            if(dxOpCode == DXOp::LegacyDoubleToFloat)
              result.value.f32v[0] = (float)arg.value.f64v[0];
            else if(dxOpCode == DXOp::LegacyDoubleToSInt32)
              result.value.s32v[0] = (int32_t)arg.value.f64v[0];
            else if(dxOpCode == DXOp::LegacyDoubleToUInt32)
              result.value.u32v[0] = (uint32_t)arg.value.f64v[0];
            break;
          }
          case DXOp::AtomicBinOp:
          case DXOp::AtomicCompareExchange:
          {
            SCOPED_LOCK(m_Debugger.GetAtomicMemoryLock());
            // AtomicBinOp(handle, atomicOp, offset0, offset1, offset2, newValue)
            // AtomicCompareExchange(handle,offset0,offset1,offset2,compareValue,newValue)
            const Id handleId = GetArgumentId(1);
            bool annotatedHandle;
            ShaderVariable handleVar;
            ResourceReferenceInfo resRefInfo = GetResource(handleId, annotatedHandle, handleVar);
            if(!resRefInfo.Valid())
              break;

            ResourceClass resClass = resRefInfo.resClass;
            // handle must be a UAV
            if(resClass != ResourceClass::UAV)
            {
              RDCERR("AtomicBinOp on non-UAV resource %s", ToStr(resClass).c_str());
              break;
            }

            // a is the current resource value
            ShaderVariable a;

            uint32_t structOffset = 0;
            bool texData = false;
            uint32_t rowPitch = 0;
            uint32_t depthPitch = 0;
            uint32_t firstElem = 0;
            uint32_t numElems = 0;
            ViewFmt fmt;

            const BindingSlot &slot = resRefInfo.binding;
            UAVInfo uavInfo;
            if(m_Debugger.GetUAV(slot, uavInfo) == DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }
            MarkResourceAccess(handleVar);

            const ResourceInfo resInfo = uavInfo.resInfo;
            texData = uavInfo.tex;
            rowPitch = uavInfo.rowPitch;
            depthPitch = uavInfo.depthPitch;

            // Unbound resource
            if(!resInfo.hasData)
            {
              RDCERR("Unbound resource %s", GetArgumentName(1).c_str());
              a.value.u32v[0] = 0;
              a.value.u32v[1] = 0;
              a.value.u32v[2] = 0;
              a.value.u32v[3] = 0;
            }

            firstElem = resInfo.firstElement;
            numElems = resInfo.numElements;
            fmt = resInfo.format;

            // If the format is unknown, guess it using the result type
            // See FetchUAV() comment about root buffers being typeless
            // The stride should have been computed from the shader metadata
            if(fmt.compType == CompType::Typeless)
            {
              FillViewFmtFromVarType(result.type, fmt);
              fmt.numComps = result.columns;
            }

            bool byteAddress = resInfo.isByteBuffer;
            if(byteAddress)
              fmt.stride = 1;

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
            }

            uint32_t stride = fmt.stride;
            if(byteAddress)
              RDCASSERTEQUAL(stride, 1);
            else
              RDCASSERTNOTEQUAL(stride, 1);

            RDCASSERTEQUAL(result.columns, 1);
            RDCASSERTEQUAL(fmt.numComps * fmt.byteWidth, GetElementByteSize(result.type));
            RDCASSERTNOTEQUAL(stride, 0);
            RDCASSERTNOTEQUAL(fmt.compType, CompType::Typeless);

            uint64_t dataOffset = 0;
            uint32_t texCoords[3] = {0, 0, 0};
            uint32_t elemIdx = 0;
            ShaderVariable arg;
            size_t offsetStart = dxOpCode == DXOp::AtomicBinOp ? 3 : 2;
            if(!texData)
            {
              if(GetShaderVariable(inst.args[offsetStart], opCode, dxOpCode, arg))
                elemIdx = arg.value.u32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 1], opCode, dxOpCode, arg))
                dataOffset = arg.value.u64v[0];
            }
            else
            {
              if(GetShaderVariable(inst.args[offsetStart], opCode, dxOpCode, arg))
                texCoords[0] = (int8_t)arg.value.u32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 1], opCode, dxOpCode, arg))
                texCoords[1] = (int8_t)arg.value.u32v[0];
              if(GetShaderVariable(inst.args[offsetStart + 2], opCode, dxOpCode, arg))
                texCoords[2] = (int8_t)arg.value.u32v[0];
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

            if(texData)
            {
              dataOffset += texCoords[0] * stride;
              dataOffset += texCoords[1] * rowPitch;
              dataOffset += texCoords[2] * depthPitch;
            }
            else
            {
              dataOffset += (firstElem + elemIdx) * stride;
            }

            const uint64_t dataSize = resInfo.dataSize;
            // NULL resource or out of bounds
            if((!texData && elemIdx >= numElems) || (texData && dataOffset >= dataSize))
            {
              a.value.u32v[0] = 0;
              a.value.u32v[1] = 0;
              a.value.u32v[2] = 0;
              a.value.u32v[3] = 0;
            }
            else
            {
              // Clamp the number of components to read based on the amount of data in the buffer
              if(!texData)
              {
                RDCASSERTNOTEQUAL(numElems, 0);
                int maxNumComps = (int)((dataSize - dataOffset) / fmt.byteWidth);
                fmt.numComps = RDCMIN(fmt.numComps, maxNumComps);
                size_t maxOffset = (firstElem + numElems) * stride + structOffset;
                maxNumComps = (int)((maxOffset - dataOffset) / fmt.byteWidth);
                fmt.numComps = RDCMIN(fmt.numComps, maxNumComps);
              }
              a.value = m_Debugger.TypedResourceLoad(resClass, slot, fmt, dataOffset);
            }

            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[6], opCode, dxOpCode, b));

            RDCASSERTEQUAL(inst.args[6]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[6]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(retType->scalarType, Type::Int);

            ShaderVariable res;
            const uint32_t c = 0;
            if(dxOpCode == DXOp::AtomicBinOp)
            {
              RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
              AtomicBinOpCode atomicBinOpCode = (AtomicBinOpCode)arg.value.u32v[0];

              switch(atomicBinOpCode)
              {
                case AtomicBinOpCode::Add:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(res, c) = comp<I>(a, c) + comp<I>(b, c)

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::And:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = comp<U>(a, c) & comp<U>(b, c);

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::Or:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = comp<U>(a, c) | comp<U>(b, c);

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::Xor:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = comp<U>(a, c) ^ comp<U>(b, c);

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::IMin:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(res, c) = RDCMIN(comp<S>(a, c), comp<S>(b, c));

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::IMax:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(res, c) = RDCMAX(comp<S>(a, c), comp<S>(b, c));

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::UMin:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(res, c) = RDCMIN(comp<U>(a, c), comp<U>(b, c));

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::UMax:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(res, c) = RDCMAX(comp<S>(a, c), comp<S>(b, c));

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                case AtomicBinOpCode::Exchange:
                {
#undef _IMPL
#define _IMPL(I, S, U) comp<I>(res, c) = comp<I>(b, c)

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
                  break;
                }
                default: RDCERR("Unhandled AtomicBinOpCode %s", ToStr(atomicBinOpCode).c_str());
              }
            }
            else if(dxOpCode == DXOp::AtomicCompareExchange)
            {
              ShaderVariable cmp;
              RDCASSERT(GetShaderVariable(inst.args[5], opCode, dxOpCode, cmp));
#undef _IMPL
#define _IMPL(I, S, U) \
  comp<I>(res, c) = comp<I>(a, c) == comp<I>(cmp, c) ? comp<I>(b, c) : comp<I>(a, c)

              IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
            }
            else
            {
              RDCERR("Unhandled dxOpCode %s", ToStr(dxOpCode).c_str());
            }

            // Helper lanes don't modify UAVs
            if(!((resClass == ResourceClass::UAV) && m_Helper))
            {
              // NULL resource or out of bounds
              if((!texData && elemIdx >= numElems) || (texData && dataOffset >= dataSize))
              {
                RDCWARN("Ignoring store to unbound resource or out of bounds store %s",
                        GetArgumentName(1).c_str());
              }
              else
              {
                m_Debugger.TypedResourceStore(resClass, slot, fmt, dataOffset, res.value);
              }
            }

            // result is the original value
            result.value = a.value;
            break;
          }
          case DXOp::SampleIndex:
          {
            // SV_SampleIndex
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::MSAASampleIndex).value.u32v[0];
            break;
          }
          case DXOp::Coverage:
          {
            // SV_Coverage
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::MSAACoverage).value.u32v[0];
            break;
          }
          case DXOp::InnerCoverage:
          {
            // SV_InnerCoverage
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::IsFullyCovered).value.u32v[0];
            break;
          }
          case DXOp::ViewID:
          {
            // SV_ViewportArrayIndex
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::ViewportIndex).value.u32v[0];
            break;
          }
          case DXOp::PrimitiveID:
          {
            // SV_PrimitiveID
            result.value.u32v[0] = GetBuiltin(ShaderBuiltin::PrimitiveIndex).value.u32v[0];
            break;
          }
          case DXOp::IsHelperLane:
          {
            result.value.u32v[0] = m_Helper ? 0 : 1;
            break;
          }
          case DXOp::UAddc:
          {
            // a+b, carry = UAddc(a,b)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, b.type);

            uint64_t sum = (uint64_t)a.value.u32v[0] + (uint64_t)b.value.u32v[0];
            // a+b : 32-bits
            result.value.u32v[0] = (sum & 0xffffffff);
            // carry
            result.value.u32v[1] = sum > 0xffffffff ? 1 : 0;
            break;
          }
          case DXOp::USubb:
          {
            // a-b, borrow : USubb(a,b)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, b.type);
            uint64_t src0;
            uint64_t src1;

            // add on a 'borrow' bit
            src0 = 0x100000000 | (uint64_t)a.value.u32v[0];
            src1 = (uint64_t)b.value.u32v[0];

            // do the subtract
            uint64_t sub = src0 - src1;

            // a-b : 32-bits
            result.value.u32v[0] = (sub & 0xffffffff);

            // mark where the borrow bits was used
            result.value.u32v[1] = (sub <= 0xffffffff) ? 1U : 0U;
            break;
          }
          case DXOp::Msad:
          {
            // masked Sum of Absolute Differences.
            // Msad(ref,src,accum)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(a.type, c.type);
            uint32_t ref = a.value.u32v[0];
            uint32_t src = b.value.u32v[0];
            uint32_t accum = c.value.u32v[0];
            for(uint32_t i = 0; i < 4; ++i)
            {
              uint8_t refByte = (uint8_t)(ref >> (i * 8));
              if(refByte == 0)
                continue;

              uint8_t srcByte = (uint8_t)(src >> (i * 8));
              uint8_t absDiff = (refByte >= srcByte) ? refByte - srcByte : srcByte - refByte;

              // The recommended overflow behaviour for MSAD is to do a 32-bit saturate.
              // This is not required, however, and wrapping is allowed.
              // So from an application point of view, overflow behaviour is undefined.
              if(UINT_MAX - accum < absDiff)
              {
                accum = UINT_MAX;
                eventFlags |= ShaderEvents::GeneratedNanOrInf;
                break;
              }
              accum += absDiff;
            }
            result.value.u32v[0] = accum;
            break;
          }
          case DXOp::Ibfe:
          {
            // Ibfe(a,b,c)
            // Given a range of bits in a number:
            //   shift those bits to the LSB, sign extend the MSB of the range.
            // width : The LSB 5 bits of a (0-31).
            // offset: The LSB 5 bits of b (0-31)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(a.type, c.type);
            uint32_t width = a.value.u32v[0] & 0x1f;
            uint32_t offset = b.value.u32v[0] & 0x1f;

            if(width == 0)
            {
              result.value.s32v[0] = 0;
            }
            else if(width + offset < 32)
            {
              result.value.s32v[0] = c.value.s32v[0] << (32 - (width + offset));
              result.value.s32v[0] = result.value.s32v[0] >> (32 - width);
            }
            else
            {
              result.value.s32v[0] = c.value.s32v[0] >> offset;
            }
            break;
          }
          case DXOp::Ubfe:
          {
            // Ubfe(a,b,c)
            // Given a range of bits in a number:
            //   shift those bits to the LSB, sign extend the MSB of the range.
            // width : The LSB 5 bits of a (0-31).
            // offset: The LSB 5 bits of b (0-31)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(a.type, c.type);
            uint32_t width = a.value.u32v[0] & 0x1f;
            uint32_t offset = b.value.u32v[0] & 0x1f;

            if(width == 0)
            {
              result.value.u32v[0] = 0;
            }
            else if(width + offset < 32)
            {
              result.value.u32v[0] = c.value.u32v[0] << (32 - (width + offset));
              result.value.u32v[0] = result.value.u32v[0] >> (32 - width);
            }
            else
            {
              result.value.u32v[0] = c.value.u32v[0] >> offset;
            }
            break;
          }
          case DXOp::Bfi:
          {
            // bfi(width,offset,value,replacedValue)
            // The LSB 5 bits of width provide the bitfield width (0-31) to take from value.
            // The LSB 5 bits of offset provide the bitfield offset (0-31) to start replacing bits
            // in the number read from replacedValue.

            // Given width, offset:
            //   bitmask = (((1 << width)-1) << offset) & 0xffffffff
            //   dest = ((value << offset) & bitmask) | (replacedValue & ~bitmask)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[4]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[4]->type->scalarType, Type::Int);
            ShaderVariable a;
            ShaderVariable b;
            ShaderVariable c;
            ShaderVariable d;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, c));
            RDCASSERT(GetShaderVariable(inst.args[4], opCode, dxOpCode, d));
            RDCASSERTEQUAL(a.type, b.type);
            RDCASSERTEQUAL(a.type, c.type);
            RDCASSERTEQUAL(a.type, d.type);
            uint32_t width = a.value.u32v[0] & 0x1f;
            uint32_t offset = b.value.u32v[0] & 0x1f;
            uint32_t bitmask = (((1 << width) - 1) << offset) & 0xffffffff;
            result.value.u32v[0] =
                (uint32_t)(((c.value.u32v[0] << offset) & bitmask) | (d.value.u32v[0] & ~bitmask));
            break;
          }
          case DXOp::MakeDouble:
          {
            // MakeDouble(lo,hi)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            ShaderVariable a;
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, b));
            RDCASSERTEQUAL(a.type, b.type);
            result.value.u64v[0] = ((uint64_t)b.value.u32v[0] << 32) | a.value.u32v[0];
            break;
          }
          case DXOp::SplitDouble:
          {
            // SplitDouble(value)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 64);
            ShaderVariable a;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            // lo
            result.value.u32v[0] = (uint32_t)(a.value.u64v[0] & 0xffffffff);
            // hi
            result.value.u32v[1] = (uint32_t)(a.value.u64v[0] >> 32);
            break;
          }
          case DXOp::BitcastI16toF16:
          case DXOp::BitcastF16toI16:
          {
            // BitcastI16toF16(value)
            // BitcastF16toI16(value)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 16);
            ShaderVariable a;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            result.value.u16v[0] = a.value.u16v[0];
            break;
          }
          case DXOp::BitcastI32toF32:
          case DXOp::BitcastF32toI32:
          {
            // BitcastI32toF32(value)
            // BitcastF32toI32(value)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 32);
            ShaderVariable a;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            result.value.u32v[0] = a.value.u32v[0];
            break;
          }
          case DXOp::BitcastI64toF64:
          case DXOp::BitcastF64toI64:
          {
            // BitcastI64toF64(value)
            // BitcastF64toI64(value)
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 64);
            ShaderVariable a;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, a));
            result.value.u64v[0] = a.value.u64v[0];
            break;
          }
          // Wave/Subgroup Operations
          case DXOp::WaveGetLaneCount:
          {
            result.value.u32v[0] = m_GlobalState.subgroupSize;
            break;
          }
          case DXOp::WaveGetLaneIndex:
          {
            result.value.u32v[0] = m_SubgroupIdx;
            break;
          }
          case DXOp::WaveIsFirstLane:
          {
            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            const uint32_t firstLaneInSub = m_WorkgroupIndex - m_SubgroupIdx;
            for(uint32_t lane = firstLaneInSub; lane < firstLaneInSub + m_GlobalState.subgroupSize;
                lane++)
            {
              RDCASSERT(lane < m_ActiveMask.size(), lane, m_ActiveMask.size());
              // helper lanes are considered active for this instruction
              if(m_ActiveMask[lane])
              {
                RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
                activeLanes.push_back(lane);
              }
            }
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));
            result.value.u32v[0] = (m_WorkgroupIndex == activeLanes[0]) ? 1 : 0;
            break;
          }
          case DXOp::WaveReadLaneAt:
          {
            // WaveReadLaneAt(value,lane)
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            const uint32_t firstLaneInSub = m_WorkgroupIndex - m_SubgroupIdx;
            uint32_t lane = firstLaneInSub + arg.value.u32v[0];

            if(lane < workgroup.size())
            {
              ShaderVariable var;
              RDCASSERT(
                  GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, var));
              result.value = var.value;
            }
            else
            {
              RDCERR("Invalid workgroup lane %u", lane);
            }
            break;
          }
          case DXOp::WaveReadLaneFirst:
          {
            // WaveReadLaneFirst(value)
            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));
            uint32_t lane = activeLanes[0];
            if(lane < workgroup.size())
            {
              ShaderVariable var;
              RDCASSERT(
                  GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, var));
              result.value = var.value;
            }
            else
            {
              RDCERR("Invalid workgroup lane %u", lane);
            }
            break;
          }
          case DXOp::WavePrefixOp:
          {
            // WavePrefixOp(value,op,sop)

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            WaveOpCode waveOpCode = (WaveOpCode)arg.value.u32v[0];

            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            bool isUnsigned = (arg.value.u32v[0] != (uint32_t)SignedOpKind::Signed);

            // set the initial value
            ShaderVariable accum(result);
            switch(waveOpCode)
            {
              case WaveOpCode::Sum: SetShaderValueZero(accum); break;
              case WaveOpCode::Product: SetShaderValueOne(accum); break;
              default:
                RDCERR("Unhandled PrefixOp wave opcode %s", ToStr(waveOpCode).c_str());
                accum.value = {};
                break;
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));
            for(uint32_t lane : activeLanes)
            {
              // stop before processing our lane
              if(lane == m_WorkgroupIndex)
                break;

              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));

              for(uint8_t c = 0; c < x.columns; c++)
              {
                switch(waveOpCode)
                {
                  case WaveOpCode::Sum:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = comp<U>(accum, c) + comp<U>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<S>(accum, c) + comp<S>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = comp<T>(accum, c) + comp<T>(x, c)

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  case WaveOpCode::Product:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = comp<U>(accum, c) * comp<U>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<S>(accum, c) * comp<S>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = comp<T>(accum, c) * comp<T>(x, c)

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  default: RDCERR("Unhandled PrefixOp wave opcode %s", ToStr(waveOpCode).c_str());
                }
              }
            }
            result.value = accum.value;
            break;
          }
          case DXOp::WavePrefixBitCount:
          case DXOp::WaveAllBitCount:
          {
            // WavePrefixBitCount(cond)
            // WaveAllBitCount(cond)

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            uint32_t maxLane = (dxOpCode == DXOp::WavePrefixBitCount) ? m_WorkgroupIndex : UINT32_MAX;

            uint32_t count = 0;
            for(uint32_t lane : activeLanes)
            {
              // stop before processing our lane
              if(lane == maxLane)
                break;

              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));
              count += x.value.u32v[0];
            }

            result.value.u32v[0] = count;
            break;
          }
          case DXOp::WaveAnyTrue:
          case DXOp::WaveAllTrue:
          case DXOp::WaveActiveBallot:
          case DXOp::WaveActiveAllEqual:
          {
            // WaveActiveBallot returns struct of int x 4, convert this to a vector
            if(dxOpCode == DXOp::WaveActiveBallot)
            {
              if(!ConvertShaderVariableStructToVector(retType, result))
              {
                break;
              }
              if(result.type != VarType::SInt && result.type != VarType::UInt)
              {
                RDCERR("Expected WaveActiveBallot result struct member type %s to be SInt or UInt",
                       ToStr(result.type).c_str());
                break;
              }
            }

            ShaderVariable accum(result);

            ShaderVariable refValue;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, refValue));

            if(dxOpCode == DXOp::WaveAnyTrue)
            {
              // WaveAnyTrue(cond)
              accum.value.u32v[0] = 0;
            }
            else if(dxOpCode == DXOp::WaveAllTrue)
            {
              // WaveAllTrue(cond)
              accum.value.u32v[0] = 1;
            }
            else if(dxOpCode == DXOp::WaveActiveAllEqual)
            {
              // WaveActiveAllEqual(value)
              accum.value.u32v[0] = 1;
            }
            else if(dxOpCode == DXOp::WaveActiveBallot)
            {
              // WaveActiveBallot(value)
              accum.value.u32v[0] = 0;
            }
            else
            {
              RDCERR("Unhandled dxOpCode %s", ToStr(dxOpCode).c_str());
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            const uint32_t firstLaneInSub = GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            for(uint32_t lane : activeLanes)
            {
              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));

              if(dxOpCode == DXOp::WaveAnyTrue)
              {
                accum.value.u32v[0] |= x.value.u32v[0];
              }
              else if(dxOpCode == DXOp::WaveAllTrue)
              {
                accum.value.u32v[0] &= x.value.u32v[0];
              }
              else if(dxOpCode == DXOp::WaveActiveBallot)
              {
                uint32_t c = (lane - firstLaneInSub) / 32;
                uint32_t bit = 1U << ((lane - firstLaneInSub) % 32U);

                if(x.value.u32v[0])
                  accum.value.u32v[c] |= bit;
              }
              else if(dxOpCode == DXOp::WaveActiveAllEqual)
              {
                for(uint8_t c = 0; c < x.columns; c++)
                {
                  bool matches = false;
#undef _IMPL
#define _IMPL(I, S, U) matches = (comp<I>(x, c) == comp<I>(refValue, c));

                  IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) matches = (comp<T>(x, c) == comp<T>(refValue, c));

                  IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);

                  accum.value.u32v[c] &= (matches ? 1 : 0);
                }
              }
            }

            result.value = accum.value;
            break;
          }
          case DXOp::WaveActiveOp:
          {
            // WaveActiveOp(value,op,sop)
            ShaderVariable accum(result);

            ShaderVariable refValue;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, refValue));

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            WaveOpCode waveOpCode = (WaveOpCode)arg.value.u32v[0];

            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            bool isUnsigned = (arg.value.u32v[0] != (uint32_t)SignedOpKind::Signed);

            // set the initial value
            switch(waveOpCode)
            {
              case WaveOpCode::Sum: SetShaderValueZero(accum); break;
              case WaveOpCode::Product: SetShaderValueOne(accum); break;
              case WaveOpCode::Min:
              case WaveOpCode::Max:
              {
                accum.value = refValue.value;
                break;
              }
              default:
                RDCERR("Unhandled ActiveOp wave opcode %s", ToStr(waveOpCode).c_str());
                accum.value = {};
                break;
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            for(uint32_t lane : activeLanes)
            {
              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));

              for(uint8_t c = 0; c < x.columns; c++)
              {
                switch(waveOpCode)
                {
                  case WaveOpCode::Sum:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = comp<U>(accum, c) + comp<U>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<S>(accum, c) + comp<S>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = comp<T>(accum, c) + comp<T>(x, c)

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  case WaveOpCode::Product:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = comp<U>(accum, c) * comp<U>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<S>(accum, c) * comp<S>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = comp<T>(accum, c) * comp<T>(x, c)

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  case WaveOpCode::Min:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = RDCMIN(comp<U>(accum, c), comp<U>(x, c))
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = RDCMIN(comp<S>(accum, c), comp<S>(x, c))
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = RDCMIN(comp<T>(accum, c), comp<T>(x, c))

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  case WaveOpCode::Max:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = RDCMAX(comp<U>(accum, c), comp<U>(x, c))
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = RDCMAX(comp<S>(accum, c), comp<S>(x, c))
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = RDCMAX(comp<T>(accum, c), comp<T>(x, c))

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  default: RDCERR("Unhandled ActiveOp wave opcode %s", ToStr(waveOpCode).c_str());
                }
              }
            }

            result.value = accum.value;
            break;
          }
          case DXOp::WaveActiveBit:
          {
            // WaveActiveBit(value,op)
            ShaderVariable accum(result);

            ShaderVariable refValue;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, refValue));

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            WaveBitOpCode waveBitOpCode = (WaveBitOpCode)arg.value.u32v[0];

            // set the initial value
            switch(waveBitOpCode)
            {
              case WaveBitOpCode::Or: SetShaderValueZero(accum); break;
              case WaveBitOpCode::Xor: SetShaderValueZero(accum); break;
              case WaveBitOpCode::And: SetUIntValue(UINT64_MAX, accum); break;
              default:
                RDCERR("Unhandled ActiveBitOp wave opcode %s", ToStr(waveBitOpCode).c_str());
                accum.value = {};
                break;
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            for(uint32_t lane : activeLanes)
            {
              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));

              for(uint8_t c = 0; c < x.columns; c++)
              {
                switch(waveBitOpCode)
                {
                  case WaveBitOpCode::And:
                  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<I>(accum, c) & comp<I>(x, c)
                    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    break;
                  }
                  case WaveBitOpCode::Or:
                  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<I>(accum, c) | comp<I>(x, c)
                    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    break;
                  }
                  case WaveBitOpCode::Xor:
                  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<I>(accum, c) ^ comp<I>(x, c)
                    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    break;
                  }
                  default:
                    RDCERR("Unhandled ActiveBitOp wave opcode %s", ToStr(waveBitOpCode).c_str());
                    break;
                }
              }
            }

            result.value = accum.value;
            break;
          }
          case DXOp::WaveMatch:
          {
            // SM6.5
            // WaveMatch(value)
            ShaderVariable refValue;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, refValue));

            // convert return result struct of four members into a vector
            if(!ConvertShaderVariableStructToVector(retType, result))
            {
              break;
            }
            if(result.type != VarType::SInt && result.type != VarType::UInt)
            {
              RDCERR("Expected WaveMatch result struct member type %s to be SInt or UInt",
                     ToStr(result.type).c_str());
              break;
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            const uint32_t firstLaneInSub = GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            for(uint32_t lane : activeLanes)
            {
              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));

              bool matches = true;
              for(uint8_t c = 0; c < x.columns; c++)
              {
#undef _IMPL
#define _IMPL(I, S, U) matches &= (comp<I>(x, c) == comp<I>(refValue, c));

                IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) matches &= (comp<T>(x, c) == comp<T>(refValue, c));

                IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
              }
              if(matches)
              {
                uint32_t c = (lane - firstLaneInSub) / 32;
                uint32_t bit = 1U << ((lane - firstLaneInSub) % 32U);

                result.value.u32v[c] |= bit;
              }
            }
            break;
          }
          case DXOp::WaveMultiPrefixOp:
          {
            // SM6.5
            // WaveMultiPrefixOp(value,mask0,mask1,mask2,mask3,op,sop)
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[6], opCode, dxOpCode, arg));
            WaveMultiPrefixOpCode waveMultiPrefixOpCode = (WaveMultiPrefixOpCode)arg.value.u32v[0];

            RDCASSERT(GetShaderVariable(inst.args[7], opCode, dxOpCode, arg));
            bool isUnsigned = (arg.value.u32v[0] != (uint32_t)SignedOpKind::Signed);

            uint32_t mask[4];
            for(uint32_t i = 0; i < 4; ++i)
            {
              RDCASSERT(GetShaderVariable(inst.args[2 + i], opCode, dxOpCode, arg));
              mask[i] = arg.value.u32v[0];
            }

            // set the initial value
            ShaderVariable accum(result);
            switch(waveMultiPrefixOpCode)
            {
              case WaveMultiPrefixOpCode::Sum: SetShaderValueZero(accum); break;
              case WaveMultiPrefixOpCode::Product: SetShaderValueOne(accum); break;
              case WaveMultiPrefixOpCode::Or: SetShaderValueZero(accum); break;
              case WaveMultiPrefixOpCode::Xor: SetShaderValueZero(accum); break;
              case WaveMultiPrefixOpCode::And: SetUIntValue(UINT64_MAX, accum); break;
              default:
                RDCERR("Unhandled WaveMultiPrefixOp wave opcode %s",
                       ToStr(waveMultiPrefixOpCode).c_str());
                accum.value = {};
                break;
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            const uint32_t firstLaneInSub = GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            uint32_t maxLane = m_WorkgroupIndex;

            for(uint32_t lane : activeLanes)
            {
              // stop before processing our lane
              if(lane == maxLane)
                break;

              uint32_t maskCol = (lane - firstLaneInSub) / 32;
              uint32_t bit = 1U << ((lane - firstLaneInSub) % 32U);

              if((mask[maskCol] & bit) == 0)
                continue;

              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));
              for(uint8_t c = 0; c < x.columns; c++)
              {
                switch(waveMultiPrefixOpCode)
                {
                  case WaveMultiPrefixOpCode::And:
                  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<I>(accum, c) & comp<I>(x, c)
                    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    break;
                  }
                  case WaveMultiPrefixOpCode::Or:
                  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<I>(accum, c) | comp<I>(x, c)
                    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    break;
                  }
                  case WaveMultiPrefixOpCode::Xor:
                  {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<I>(accum, c) ^ comp<I>(x, c)
                    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    break;
                  }
                  case WaveMultiPrefixOpCode::Sum:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = comp<U>(accum, c) + comp<U>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<S>(accum, c) + comp<S>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = comp<T>(accum, c) + comp<T>(x, c)

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  case WaveMultiPrefixOpCode::Product:
                  {
                    if(isUnsigned)
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<U>(accum, c) = comp<U>(accum, c) * comp<U>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    else
                    {
#undef _IMPL
#define _IMPL(I, S, U) comp<S>(accum, c) = comp<S>(accum, c) * comp<S>(x, c)
                      IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, x.type);

#undef _IMPL
#define _IMPL(T) comp<T>(accum, c) = comp<T>(accum, c) * comp<T>(x, c)

                      IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, x.type);
                    }
                    break;
                  }
                  default:
                    RDCERR("Unhandled WaveMultiPrefixOp wave opcode %s",
                           ToStr(waveMultiPrefixOpCode).c_str());
                    break;
                }
              }
            }
            result.value = accum.value;
            break;
          }
          case DXOp::WaveMultiPrefixBitCount:
          {
            // SM6.5
            // WaveMultiPrefixBitCount(value,mask0,mask1,mask2,mask3)

            uint32_t mask[4];
            for(uint32_t i = 0; i < 4; ++i)
            {
              ShaderVariable arg;
              RDCASSERT(GetShaderVariable(inst.args[2 + i], opCode, dxOpCode, arg));
              mask[i] = arg.value.u32v[0];
            }

            // determine active lane indices in our subgroup
            rdcarray<uint32_t> activeLanes;
            const uint32_t firstLaneInSub = GetSubgroupActiveLanes(workgroup, activeLanes);
            RDCASSERT(!SubgroupIsDiverged(workgroup, activeLanes));

            uint32_t maxLane = m_WorkgroupIndex;

            uint32_t count = 0;
            for(uint32_t lane : activeLanes)
            {
              // stop before processing our lane
              if(lane == maxLane)
                break;

              uint32_t maskCol = (lane - firstLaneInSub) / 32;
              uint32_t bit = 1U << ((lane - firstLaneInSub) % 32U);

              if((mask[maskCol] & bit) == 0)
                continue;

              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));
              count += x.value.u32v[0];
            }

            result.value.u32v[0] = count;
            break;
          }
          // Quad Operations
          case DXOp::QuadReadLaneAt:
          case DXOp::QuadOp:
          {
            RDCASSERT(!QuadIsDiverged(workgroup, m_QuadNeighbours));
            // QuadOp(value,op)
            // QuadReadLaneAt(value,quadLane)
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            uint32_t lane = ~0U;
            if(dxOpCode == DXOp::QuadOp)
            {
              if(m_QuadLaneIndex == ~0U)
              {
                RDCERR("Quad operation without proper quad neighbours");
                lane = m_WorkgroupIndex;
              }
              else
              {
                QuadOpKind quadOp = (QuadOpKind)arg.value.u32v[0];
                switch(quadOp)
                {
                  case QuadOpKind::ReadAcrossX:
                  {
                    // 0->1
                    // 1->0
                    // 2->3
                    // 3->2
                    lane = m_QuadLaneIndex ^ 1;
                    break;
                  }
                  case QuadOpKind::ReadAcrossY:
                  {
                    // 0->2
                    // 1->3
                    // 2->0
                    // 3->1
                    lane = m_QuadLaneIndex ^ 2;
                    break;
                  }
                  case QuadOpKind::ReadAcrossDiagonal:
                  {
                    // 0->3
                    // 1->2
                    // 2->1
                    // 3->0
                    lane = m_QuadLaneIndex ^ 3;
                    break;
                  }
                  default: RDCERR("Unhandled QuadOpKind %s", ToStr(quadOp).c_str()); break;
                }
                if(lane < 4)
                  lane = m_QuadNeighbours[lane];

                if(lane == ~0U)
                {
                  RDCERR("QuadOp %s without proper quad neighbours", ToStr(quadOp).c_str());
                  lane = m_WorkgroupIndex;
                }
              }
            }
            else if(dxOpCode == DXOp::QuadReadLaneAt)
            {
              // QuadReadLaneAt(value,quadLane)
              lane = arg.value.u32v[0];
              RDCASSERT(lane < 4, lane);
              lane = RDCMIN(lane, 3U);
              lane = m_QuadNeighbours[lane];

              if(lane == ~0U)
              {
                RDCERR("QuadReadLaneAt without proper quad neighbours");
                lane = m_WorkgroupIndex;
              }
            }
            else
            {
              RDCERR("Unhandled dxOpCode %s", ToStr(dxOpCode).c_str());
            }
            if(lane < workgroup.size())
            {
              ShaderVariable var;
              RDCASSERT(
                  GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, var));
              result.value = var.value;
            }
            else
            {
              RDCERR("Invalid workgroup lane %u", lane);
            }
            break;
          }
          case DXOp::QuadVote:
          {
            // SM6.7 QuadVote(cond,op)
            RDCASSERT(!QuadIsDiverged(workgroup, m_QuadNeighbours));

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            QuadVoteOpKind quadVoteOp = (QuadVoteOpKind)arg.value.u32v[0];

            ShaderVariable accum(result);

            switch(quadVoteOp)
            {
              case QuadVoteOpKind::Any: accum.value.u32v[0] = 0; break;
              case QuadVoteOpKind::All: accum.value.u32v[0] = 1; break;
              default: RDCERR("Unhandled QuadVoteOpKind %s", ToStr(quadVoteOp).c_str()); break;
            };

            for(uint32_t i = 0; i < 4; ++i)
            {
              uint32_t lane = m_QuadNeighbours[i];
              if(lane == ~0U)
              {
                RDCERR("QuadVote %s without proper quad neighbours", ToStr(quadVoteOp).c_str());
                lane = m_WorkgroupIndex;
              }

              RDCASSERT(lane < workgroup.size(), lane, workgroup.size());
              ShaderVariable x;
              RDCASSERT(GetShaderVariableFromLane(workgroup[lane], inst.args[1], opCode, dxOpCode, x));

              switch(quadVoteOp)
              {
                case QuadVoteOpKind::Any: accum.value.u32v[0] |= x.value.u32v[0]; break;
                case QuadVoteOpKind::All: accum.value.u32v[0] &= x.value.u32v[0]; break;
                default: RDCERR("Unhandled QuadVoteOpKind %s", ToStr(quadVoteOp).c_str()); break;
              }
            }
            result.value = accum.value;
            break;
          }
          case DXOp::Dot2AddHalf:
          {
            // Dot2AddHalf(acc,ax,ay,bx,by)
            // SM6.4: 2D half dot product with accumulate to float
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 32);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[2]->type->bitWidth, 16);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[3]->type->bitWidth, 16);
            RDCASSERTEQUAL(inst.args[4]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[4]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[4]->type->bitWidth, 16);
            RDCASSERTEQUAL(inst.args[5]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[5]->type->scalarType, Type::Float);
            RDCASSERTEQUAL(inst.args[5]->type->bitWidth, 16);
            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            float acc = arg.value.f32v[0];
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            float ax = (float)arg.value.f16v[0];
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, arg));
            float ay = (float)arg.value.f16v[0];
            RDCASSERT(GetShaderVariable(inst.args[4], opCode, dxOpCode, arg));
            float bx = (float)arg.value.f16v[0];
            RDCASSERT(GetShaderVariable(inst.args[5], opCode, dxOpCode, arg));
            float by = (float)arg.value.f16v[0];
            result.value.f32v[0] = acc + ax * bx + ay * by;
            break;
          }
          case DXOp::Dot4AddI8Packed:
          case DXOp::Dot4AddU8Packed:
          {
            // SM6.4
            // Dot4AddI8Packed(acc,a,b)
            // signed dot product of 4 x i8 vectors packed into i32, with accumulate to i32
            // Dot4AddU8Packed(acc,a,b)
            // unsigned dot product of 4 x u8 vectors packed into i32, with accumulate to i32
            RDCASSERTEQUAL(inst.args[1]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[1]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[1]->type->bitWidth, 32);
            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[2]->type->bitWidth, 32);
            RDCASSERTEQUAL(inst.args[3]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[3]->type->scalarType, Type::Int);
            RDCASSERTEQUAL(inst.args[3]->type->bitWidth, 32);
            ShaderVariable acc;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, acc));
            ShaderVariable a;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, a));
            ShaderVariable b;
            RDCASSERT(GetShaderVariable(inst.args[3], opCode, dxOpCode, b));

            if(dxOpCode == DXOp::Dot4AddI8Packed)
            {
              int32_t res = acc.value.s32v[0];
              for(uint32_t col = 0; col < 4; ++col)
                res += (int32_t)a.value.s8v[col] * (int32_t)b.value.s8v[col];
              result.value.s32v[0] = res;
            }
            else
            {
              uint32_t res = acc.value.u32v[0];
              for(uint32_t col = 0; col < 4; ++col)
                res += (uint32_t)a.value.u8v[col] * (uint32_t)b.value.u8v[col];
              result.value.u32v[0] = res;
            }
            break;
          }
          case DXOp::Pack4x8:
          {
            // SM6.6: pack_u8, pack_s8, pack_clamp_u8 (0-255), pack_s8, pack_clamp_s8 (-128-127)
            // Pack4x8(packMode,x,y,z,w)
            //  packs vector of 4 signed or unsigned values into a packed datatype, drops or clamps unused bits

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            DXIL::PackMode packMode = (DXIL::PackMode)arg.value.u32v[0];

            for(uint32_t i = 0; i < 4; ++i)
            {
              RDCASSERT(GetShaderVariable(inst.args[i + 2], opCode, dxOpCode, arg));
              switch(packMode)
              {
                case DXIL::PackMode::Trunc:
                {
                  result.value.u8v[i] = arg.value.u32v[0] & 0xFF;
                  break;
                }
                case DXIL::PackMode::SClamp:
                {
                  result.value.s8v[i] = (int8_t)RDCCLAMP(arg.value.s32v[0], -128, 127);
                  break;
                }
                case DXIL::PackMode::UClamp:
                {
                  result.value.u8v[i] = (uint8_t)RDCCLAMP(arg.value.s32v[0], 0, 255);
                  break;
                }
                default: RDCERR("Unhandled PackMode %s", ToStr(packMode).c_str()); break;
              }
            }
            break;
          }
          case DXOp::Unpack4x8:
          {
            // SM6.6: unpack_s8s16, unpack_s8s32, unpack_u8u16, unpack_u8u32
            // Unpack4x8(unpackMode,pk)
            //  unpacks 4 8-bit signed or unsigned values into int32 or int16 vector
            // Result is a structure of four 8-bit values
            RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
            RDCASSERTEQUAL(retType->members.size(), 4);
            // Remap to an array
            const DXIL::Type *elementType = retType->members[0];
            RDCASSERTEQUAL(elementType->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(elementType->scalarType, Type::Int);
            result.type = ConvertDXILTypeToVarType(elementType);
            result.columns = 4;
            uint32_t bitWidth = elementType->bitWidth;

            ShaderVariable arg;
            RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, arg));
            DXIL::UnpackMode unpackMode = (DXIL::UnpackMode)arg.value.u32v[0];

            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, arg));
            for(uint32_t i = 0; i < 4; ++i)
            {
              if(unpackMode == DXIL::UnpackMode::Signed)
              {
                if(bitWidth == 32)
                  result.value.s32v[i] = arg.value.s8v[i];
                else if(bitWidth == 16)
                  result.value.s16v[i] = arg.value.s8v[i];
                else
                  RDCERR("Unhandled result bitwidth %d", bitWidth);
              }
              else if(unpackMode == DXIL::UnpackMode::Unsigned)
              {
                if(bitWidth == 32)
                  result.value.u32v[i] = arg.value.u8v[i];
                else if(bitWidth == 16)
                  result.value.u16v[i] = arg.value.u8v[i];
                else
                  RDCERR("Unhandled result bitwidth %d", bitWidth);
              }
              else
                RDCERR("Unhandled UnpackMode %s", ToStr(unpackMode).c_str());
            }
            break;
          }
          case DXOp::BufferUpdateCounter:
          {
            // HLSL: DecrementCounter, IncrementCounter
            // BufferUpdateCounter(uav,inc)
            // Treat as an atomic operation on the UAV's hidden counter
            SCOPED_LOCK(m_Debugger.GetAtomicMemoryLock());
            const Id handleId = GetArgumentId(1);
            bool annotatedHandle;
            ShaderVariable handleVar;
            ResourceReferenceInfo resRefInfo = GetResource(handleId, annotatedHandle, handleVar);
            if(!resRefInfo.Valid())
              break;

            ResourceClass resClass = resRefInfo.resClass;
            // handle must be a UAV
            if(resClass != ResourceClass::UAV)
            {
              RDCERR("BufferUpdateCounter on non-UAV resource %s", ToStr(resClass).c_str());
              break;
            }

            const BindingSlot &slot = resRefInfo.binding;
            UAVInfo uavInfo;
            if(m_Debugger.GetUAV(slot, uavInfo) == DeviceOpResult::NeedsDevice)
            {
              SetStepNeedsDeviceThread();
              break;
            }

            RDCASSERTEQUAL(inst.args[2]->type->type, Type::TypeKind::Scalar);
            RDCASSERTEQUAL(inst.args[2]->type->scalarType, Type::Int);

            ShaderVariable value;
            RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, value));

            RDCASSERT(IsIntegerType(value.type));
            RDCASSERT(IsIntegerType(result.type));

            // Returns the pre-increment value of the hidden counter
            result.value.s32v[0] = uavInfo.hiddenCounter;

            uavInfo.hiddenCounter += value.value.s32v[0];
            break;
          }

          // Implement when required
          case DXOp::CBufferLoad:
            // loads single value from byte offset in constant buffer, 8-byte alignment on the offset

          // SM6.1
          case DXOp::AttributeAtVertex:
            // Pixel shader: load input signature attributes for a specific vertexID (0-2)

          // SM6.7
          case DXOp::TextureStoreSample:
            // stores texel data at specified sample index
          case DXOp::TextureGatherRaw:
            // Gather raw elements from 4 texels with no type conversions (SRV type is constrained)

          // SM 6.8 : when SM6.8 is supporting by RenderDoc
          case DXOp::StartVertexLocation:
            // SV_BaseVertexLocation
            // BaseVertexLocation from DrawIndexedInstanced or StartVertexLocation from DrawInstanced
          case DXOp::StartInstanceLocation:
            // SV_StartInstanceLocation
            // StartInstanceLocation from Draw*Instanced
          case DXOp::BarrierByMemoryType:
          case DXOp::BarrierByMemoryHandle:

          // No plans to implement

          // MSAA
          case DXOp::EvalSnapped:
            // HLSL : EvaluateAttributeSnapped
          case DXOp::EvalSampleIndex:
            // HLSL : EvaluateAttributeAtSample
          case DXOp::EvalCentroid:
            // HLSL : EvaluateAttributeCentroid

          case DXOp::CycleCounterLegacy:
            // DXBC Shader-Internal Cycle Counter (Debug Only)

          case DXOp::CheckAccessFullyMapped:
            // determines whether all values from a Sample, Gather, or Load operation
            // accessed mapped tiles in a tiled resource
          case DXOp::WriteSamplerFeedback:
          case DXOp::WriteSamplerFeedbackBias:
          case DXOp::WriteSamplerFeedbackLevel:
          case DXOp::WriteSamplerFeedbackGrad:

          // DXIL Internal operations used during DXBC conversion
          case DXOp::TempRegLoad:
          case DXOp::TempRegStore:
          case DXOp::MinPrecXRegLoad:
          case DXOp::MinPrecXRegStore:

          // Mesh Shaders
          case DXOp::SetMeshOutputCounts:
          case DXOp::EmitIndices:
          case DXOp::StoreVertexOutput:
          case DXOp::StorePrimitiveOutput:
          case DXOp::GetMeshPayload:
          case DXOp::DispatchMesh:

          // Geometry Shaders: Hull/Domain
          case DXOp::GSInstanceID:
          case DXOp::LoadOutputControlPoint:
          case DXOp::LoadPatchConstant:
          case DXOp::DomainLocation:
          case DXOp::StorePatchConstant:
          case DXOp::OutputControlPointID:
          case DXOp::EmitStream:
          case DXOp::CutStream:
          case DXOp::EmitThenCutStream:

          // Wave Matrix Operations
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

          // Ray Tracing
          case DXOp::CreateHandleForLib:
          case DXOp::CallShader:
          case DXOp::InstanceID:
          case DXOp::InstanceIndex:
          case DXOp::PrimitiveIndex:
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
          case DXOp::RayQuery_CandidateInstanceContributionToHitGroupIndex:
          case DXOp::RayQuery_CommittedInstanceContributionToHitGroupIndex:
          case DXOp::GeometryIndex:

          // Workgraphs
          case DXOp::AllocateNodeOutputRecords:
          case DXOp::GetNodeRecordPtr:
          case DXOp::IncrementOutputCount:
          case DXOp::GetInputRecordCount:
          case DXOp::OutputComplete:
          case DXOp::CreateNodeOutputHandle:
          case DXOp::IndexNodeHandle:
          case DXOp::AnnotateNodeHandle:
          case DXOp::CreateNodeInputRecordHandle:
          case DXOp::AnnotateNodeRecordHandle:
          case DXOp::NodeOutputIsValid:
          case DXOp::GetRemainingRecursionLevels:
          case DXOp::FinishedCrossGroupSharing:
          case DXOp::BarrierByNodeRecordHandle:

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
      m_Dead = true;
      RDCERR("Operation::Unreachable reached, terminating debugging!");
      return true;
    }
    case Operation::Branch:
    {
      // Branch <label>
      // Branch <label_true> <label_false> <BOOL_VAR>
      uint32_t targetArg = 0;
      bool divergencePoint = false;
      if(inst.args.size() > 1)
      {
        divergencePoint = cast<Block>(inst.args[0])->id != cast<Block>(inst.args[1])->id;
        ShaderVariable cond;
        RDCASSERT(GetShaderVariable(inst.args[2], opCode, dxOpCode, cond));
        if(!cond.value.u32v[0])
          targetArg = 1;
      }

      const Block *target = cast<Block>(inst.args[targetArg]);
      if(!JumpToBlock(target, divergencePoint))
        RDCERR("Unknown branch target %u '%s'", m_Block, GetArgumentName(targetArg).c_str());
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
        RDCASSERT(GetPhiShaderVariable(dxilValue, opCode, dxOpCode, arg));
        rdcstr name = result.name;
        // Copy the whole variable to ensure we get the correct type information
        result = arg;
        result.name = name;
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
      if(src == DXILDebug::INVALID_ID)
        break;
      if(!IsVariableAssigned(src))
      {
        RDCERR("Unknown variable Id %u", src);
        break;
      }
      const ShaderVariable &srcVal = m_Variables[src];
      RDCASSERT(srcVal.members.empty());
      RDCASSERTEQUAL(inst.args.size(), 2);
      uint32_t idx = ~0U;
      RDCASSERT(getival(inst.args[1], idx));
      ShaderVariable sourceData;
      if(srcVal.type == VarType::Struct)
      {
        sourceData = srcVal.members[idx];
        idx = 0;
      }
      else
      {
        sourceData = srcVal;
      }
      RDCASSERT(idx < sourceData.columns);

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
    case Operation::Load: OperationLoad(false, inst, opCode, dxOpCode, resultId, result); break;
    case Operation::LoadAtomic:
    {
      SCOPED_LOCK(m_Debugger.GetAtomicMemoryLock());
      OperationLoad(true, inst, opCode, dxOpCode, resultId, result);
      break;
    }
    case Operation::Store:
    {
      OperationStore(inst, opCode, dxOpCode);
      RDCASSERTEQUAL(resultId, DXILDebug::INVALID_ID);
      result.name.clear();
      resultId = INVALID_ID;
      break;
    }
    case Operation::StoreAtomic:
    {
      SCOPED_LOCK(m_Debugger.GetAtomicMemoryLock());
      OperationStore(inst, opCode, dxOpCode);
      RDCASSERTEQUAL(resultId, DXILDebug::INVALID_ID);
      result.name.clear();
      resultId = INVALID_ID;
      break;
    }
    case Operation::Alloca:
    {
      m_Memory.AllocateMemoryForType(inst.type, resultId, false, false, result);
      break;
    }
    case Operation::GetElementPtr:
    {
      const DXIL::Type *resultType = inst.type->inner;
      Id ptrId = GetArgumentId(0);
      if(ptrId == DXILDebug::INVALID_ID)
        break;
      if(!IsVariableAssigned(ptrId))
      {
        RDCERR("Unknown variable Id %u", ptrId);
        break;
      }

      if(m_Memory.m_Allocations.count(ptrId) == 0)
      {
        RDCERR("Unknown memory allocation Id %u", ptrId);
        break;
      }

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

      const ShaderVariable &basePtr = m_Variables[ptrId];
      if(indexes.size() > 1)
        offset += indexes[1] * GetShaderVariableElementByteSize(basePtr);
      RDCASSERT(indexes.size() <= 2);

      VarType baseType = ConvertDXILTypeToVarType(resultType);
      RDCASSERTNOTEQUAL(resultType->type, DXIL::Type::TypeKind::Struct);
      RDCASSERTEQUAL(resultType->type, DXIL::Type::TypeKind::Scalar);

      uint32_t countElems = RDCMAX(1U, resultType->elemCount);
      size_t size = countElems * GetElementByteSize(baseType);

      // Copy from the backing memory to the result
      const MemoryTracking::Allocation &allocation = m_Memory.m_Allocations[ptrId];
      uint8_t *memory = (uint8_t *)allocation.backingMemory;

      // Ensure global variables use global memory
      // Ensure non-global variables do not use global memory
      if(allocation.globalVarAlloc)
        RDCASSERT(cast<GlobalVar>(inst.args[0]));
      else
        RDCASSERT(!cast<GlobalVar>(inst.args[0]));

      result.type = baseType;
      result.rows = (uint8_t)countElems;

      RDCASSERT(offset + size <= allocation.size);
      if(offset + size <= allocation.size)
      {
        memory += offset;
        m_Memory.m_Pointers[resultId] = {ptrId, memory, size};

        RDCASSERT(size <= sizeof(ShaderValue));
        if(size <= sizeof(ShaderValue))
          memcpy(&result.value, memory, size);
        else
          RDCERR("Size %u too large MAX %u for GetElementPtr", size, sizeof(ShaderValue));
      }
      else
      {
        RDCERR("Invalid GEP offset %u size %u for allocation size %u", offset, size, allocation.size);
      }
      break;
    }
    case Operation::Bitcast:
    {
      RDCASSERTEQUAL(retType->bitWidth, inst.args[0]->type->bitWidth);
      Id argId = GetArgumentId(0);
      auto itPtr = m_Memory.m_Pointers.find(argId);
      if(itPtr != m_Memory.m_Pointers.end())
        m_Memory.m_Pointers[resultId] = itPtr->second;

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
#define _IMPL(I, S, U)                                  \
  if(comp<U>(b, c) != 0)                                \
  {                                                     \
    comp<U>(result, c) = comp<U>(a, c) / comp<U>(b, c); \
  }                                                     \
  else                                                  \
  {                                                     \
    comp<U>(result, c) = 0;                             \
    eventFlags |= ShaderEvents::GeneratedNanOrInf;      \
  }

        IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, a.type);
      }
      else if(opCode == Operation::SDiv)
      {
#undef _IMPL
#define _IMPL(I, S, U)                                  \
  if(comp<S>(b, c) != 0)                                \
  {                                                     \
    comp<S>(result, c) = comp<S>(a, c) / comp<S>(b, c); \
  }                                                     \
  else                                                  \
  {                                                     \
    comp<S>(result, c) = 0;                             \
    eventFlags |= ShaderEvents::GeneratedNanOrInf;      \
  }

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
      else if(opCode == Operation::ZExt)
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
      bool divergencePoint = false;
      const uint32_t defaultBlockId = cast<Block>(inst.args[1])->id;
      for(uint32_t a = 2; a < inst.args.size(); a += 2)
      {
        const uint32_t targetBlockId = cast<Block>(inst.args[a + 1])->id;
        if(targetBlockId != defaultBlockId)
        {
          divergencePoint = true;
          break;
        }
      }
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
      if(!JumpToBlock(target, divergencePoint))
        RDCERR("Unknown switch target %u '%s'", m_Block, GetArgumentName(targetArg).c_str());
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
      SCOPED_LOCK(m_Debugger.GetAtomicMemoryLock());
      OperationAtomic(inst, opCode, dxOpCode, resultId, result);
      break;
    }
    case Operation::CompareExchange:
    {
      OperationAtomic(inst, opCode, dxOpCode, resultId, result);
      break;
    }
    case Operation::AddrSpaceCast:
    case Operation::InsertValue: RDCERR("Unhandled LLVM opcode %s", ToStr(opCode).c_str()); break;
  };

  // Waiting for result i.e. from the GPU or replay thread
  if(IsPendingResultPending())
  {
    m_FunctionInstructionIdx--;
    // This instruction is being deferred clear the pending debug state
    if(m_HasDebugState)
      ClearPendingDebugState();
    m_HasDebugState = false;
    return true;
  }
  SetPendingResultUnknown();

  // Update the result variable
  if(resultId == DXILDebug::INVALID_ID)
    RDCASSERT(result.name.empty());
  else
    RDCASSERT(!result.name.empty());

  if(!result.name.empty() && resultId != DXILDebug::INVALID_ID)
  {
    SetResult(resultId, result, opCode, dxOpCode, eventFlags);

    // Fake Output results won't be in the referencedIds
    RDCASSERT(resultId == m_Output.id || m_FunctionInfo->referencedIds.count(resultId) == 1);

    RDCASSERT(resultId < m_Live.size());
    m_Live[resultId] = true;
    RDCASSERT(resultId < m_Variables.size());
    m_Variables[resultId] = result;
    RDCASSERT(resultId < m_Assigned.size());
    m_Assigned[resultId] = true;
  }

  return true;
}

void ThreadState::StepOverNopInstructions()
{
  if(m_Ended)
    return;
  do
  {
    RDCASSERT(m_FunctionInstructionIdx < m_FunctionInfo->function->instructions.size());
    const Instruction *inst = m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
    if(!IsNopInstruction(*inst))
    {
      m_ActiveGlobalInstructionIdx =
          m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx;
      return;
    }

    m_FunctionInstructionIdx++;
  } while(true);
}

void ThreadState::RetireLiveIDs()
{
  m_PendingDebugState.flags = ShaderEvents::NoEvent;
  m_PendingDebugState.changes.clear();

  // Remove variables which have gone out of scope
  ExecPointReference current(m_Block, m_FunctionInstructionIdx);
  for(uint32_t id = 0; id < m_Live.size(); ++id)
  {
    if(!m_Live[id])
      continue;
    // The fake output variable is always in scope
    if(id == m_Output.id)
      continue;
    // Globals are always in scope
    if(m_IsGlobal[id])
      continue;

    RDCASSERT(id < m_FunctionInfo->maxExecPointPerId.size());
    const ExecPointReference maxPoint = m_FunctionInfo->maxExecPointPerId[id];
    RDCASSERT(maxPoint.IsValid());
    // Use control flow to determine if the current execution point is after the maximum point
    if(current.IsAfter(maxPoint, m_FunctionInfo->controlFlow))
    {
      m_Live[id] = false;

      ShaderVariableChange change;
      change.before = m_Variables[id];
      m_PendingDebugState.changes.push_back(change);
    }
  }
}

void ThreadState::StepNext(bool hasDebugState, const rdcarray<ThreadState> &workgroup)
{
  m_HasDebugState = hasDebugState;
  m_Diverged = false;
  m_EnteredPoints.clear();
  m_ConvergencePoint = INVALID_EXECUTION_POINT;
  m_PartialConvergencePoints.clear();

  RDCASSERTEQUAL(m_ActiveGlobalInstructionIdx,
                 m_FunctionInfo->globalInstructionOffset + m_FunctionInstructionIdx);
  if(m_HasDebugState)
  {
    m_PendingDebugState.flags = ShaderEvents::NoEvent;
    m_PendingDebugState.changes.clear();
  }
  ExecuteInstruction(workgroup);
  StepOverNopInstructions();

  m_HasDebugState = false;
}

// When getting live variables : this must be a thread safe operation using only thread safe containers
bool ThreadState::GetShaderVariableHelper(const DXIL::Value *dxilValue, DXIL::Operation op,
                                          DXIL::DXOp dxOpCode, ShaderVariable &var,
                                          bool flushDenormInput, bool isLive,
                                          bool ignoreLiveCheck) const
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
  if(var.type == VarType::Half)
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
    if(gv->initialiser)
      var.value.u64v[0] = gv->initialiser->getU64();
    else
      memset(&var.value, 0, sizeof(var.value));
    return true;
  }

  if(const Instruction *inst = cast<Instruction>(dxilValue))
  {
    if(isLive)
      return GetLiveVariable(inst->slot, op, dxOpCode, ignoreLiveCheck, var);
    else
      return GetPhiVariable(inst->slot, op, dxOpCode, var);
  }
  RDCERR("Unhandled DXIL Value type");

  return false;
}

bool ThreadState::IsVariableAssigned(const Id id) const
{
  if(id < m_Assigned.size())
  {
    return m_Assigned[id];
  }
  else
  {
    RDCERR("Variable Id %d is not in assigned list", id);
    return false;
  }
}

ShaderVariable ThreadState::GetBuiltin(ShaderBuiltin builtin) const
{
  auto local = m_Builtins.find(builtin);
  if(local != m_Builtins.end())
    return local->second;

  auto global = m_GlobalState.builtins.find(builtin);
  if(global != m_GlobalState.builtins.end())
    return global->second;

  RDCERR("Couldn't find data for builtin %s", ToStr(builtin).c_str());
  return {};
}

// This must be a thread safe operation using only thread safe containers
bool ThreadState::GetLiveVariable(const Id &id, Operation op, DXOp dxOpCode, bool ignoreLiveCheck,
                                  ShaderVariable &var) const
{
  if(id < m_Live.size())
  {
    RDCASSERT(ignoreLiveCheck || m_Live[id]);
  }
  else
  {
    RDCERR("Unknown Live Variable Id %d", id);
  }
  if(IsVariableAssigned(id))
  {
    var = m_Variables[id];
    return GetVariableHelper(op, dxOpCode, var);
  }
  RDCERR("Unknown Variable %d", id);
  return false;
}

bool ThreadState::GetPhiVariable(const Id &id, Operation op, DXOp dxOpCode, ShaderVariable &var) const
{
  auto it = m_PhiVariables.find(id);
  if(it != m_PhiVariables.end())
  {
    var = it->second;
    return GetVariableHelper(op, dxOpCode, var);
  }
  RDCERR("Phi Variable not found %d", id);
  return false;
}

bool ThreadState::GetVariableHelper(Operation op, DXOp dxOpCode, ShaderVariable &var) const
{
  bool flushDenorm = OperationFlushing(op, dxOpCode);
  if(var.type == VarType::Double)
    flushDenorm = false;
  if(var.type == VarType::Half)
    flushDenorm = false;
  RDCASSERT(!flushDenorm || var.type == VarType::Float);
  if(flushDenorm)
    var.value.f32v[0] = flush_denorm(var.value.f32v[0]);
  return true;
}

void ThreadState::SetResult(const Id &id, ShaderVariable &result, Operation op, DXOp dxOpCode,
                            ShaderEvents flags)
{
  RDCASSERT((result.rows > 0 && result.columns > 0) || !result.members.empty());
  RDCASSERT(result.columns <= 16);
  RDCASSERT(result.type != VarType::Unknown || !result.members.empty());

  // Can only flush denorms for float types
  bool flushDenorm = OperationFlushing(op, dxOpCode) && (result.type == VarType::Float);

  flags |= AssignValue(result, flushDenorm);

  if(m_HasDebugState)
  {
    ShaderVariableChange change;
    m_PendingDebugState.flags |= flags;
    change.before = m_Variables[id];
    change.after = result;
    m_PendingDebugState.changes.push_back(change);
  }
}

void ThreadState::MarkResourceAccess(const ShaderVariable &var)
{
  if(!m_HasDebugState)
    return;

  if(var.type != VarType::ReadOnlyResource && var.type != VarType::ReadWriteResource)
    return;

  ShaderVariableChange change;
  change.before = var;
  change.after = var;

  m_PendingDebugState.changes.push_back(change);
}

void ThreadState::UpdateBackingMemoryFromVariable(void *ptr, uint64_t &allocSize,
                                                  const ShaderVariable &var)
{
  // Memory copy from value to backing memory
  if(var.members.size() == 0)
  {
    RDCASSERTEQUAL(var.rows, 1);
    const size_t elementSize = GetElementByteSize(var.type);
    RDCASSERT(elementSize <= allocSize);
    RDCASSERT(elementSize <= sizeof(ShaderValue));
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
  UpdateShaderVariableFromBackingMemory(baseMemory, ptr);
}

void ThreadState::UpdateGlobalBackingMemory(Id ptrId, const MemoryTracking::Pointer &ptr,
                                            const MemoryTracking::Allocation &allocation,
                                            const ShaderVariable &val)
{
  Id baseMemoryId = ptr.baseMemoryId;
  if(m_GlobalState.groupSharedMemoryIds.contains(baseMemoryId))
  {
    void *const memory = ptr.memory;

    // Compute the local pointer offset and apply it to the global base memory
    ptrdiff_t offset = (uintptr_t)memory - (uintptr_t)allocation.backingMemory;
    if(offset < 0)
    {
      RDCERR("Invalid global memory allocation offset ptrId %u Id %u", ptrId, baseMemoryId);
      return;
    }
    auto globalMem = m_GlobalState.memory.m_Allocations.find(baseMemoryId);
    if(globalMem == m_GlobalState.memory.m_Allocations.end())
    {
      RDCERR("Unknown global memory allocation Id %u", baseMemoryId);
      return;
      ;
    }
    void *globalMemory = (void *)((uintptr_t)globalMem->second.backingMemory + offset);
    uint64_t allocSize = ptr.size;
    UpdateBackingMemoryFromVariable(globalMemory, allocSize, val);
  }
}

bool ThreadState::LoadGSMFromGlobalBackingMemory(const MemoryTracking::Pointer &ptr,
                                                 const MemoryTracking::Allocation &allocation,
                                                 ShaderVariable &var)
{
  const Id baseMemoryId = ptr.baseMemoryId;
  auto globalMem = m_GlobalState.memory.m_Allocations.find(baseMemoryId);
  if(globalMem != m_GlobalState.memory.m_Allocations.end())
  {
    // Compute the local pointer offset and apply it to the global base memory
    ptrdiff_t offset = (uintptr_t)ptr.memory - (uintptr_t)allocation.backingMemory;
    if(offset >= 0)
    {
      const void *globalBackingMemory = globalMem->second.backingMemory;
      void *globalMemory = (void *)((uintptr_t)globalBackingMemory + offset);
      RDCASSERT(ptr.size <= sizeof(ShaderValue));
      if(ptr.size <= sizeof(ShaderValue))
        memcpy(&var.value, globalMemory, (size_t)ptr.size);
    }
    else
    {
      RDCERR("Invalid memory allocation offset baseMemoryId %u", baseMemoryId);
      return false;
    }
  }
  else
  {
    RDCERR("Invalid GSM baseMemoryId %u", baseMemoryId);
    return false;
  }
  return true;
}

bool ThreadState::PerformGPUResourceOp(const rdcarray<ThreadState> &workgroup, Operation opCode,
                                       DXOp dxOpCode, const ResourceReferenceInfo &resRefInfo,
                                       const DXIL::Instruction &inst, ShaderVariable &result)
{
  DXIL_DEBUG_RDCASSERT(!IsPendingResultPending());
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

  // CalculateSampleGather is only valid for SRV resources
  ResourceClass resClass = resRefInfo.resClass;
  RDCASSERTEQUAL(resClass, ResourceClass::SRV);

  // Resource reference must be an SRV
  const ResourceReferenceInfo::SRVData &srv = resRefInfo.srvData;

  SampleGatherResourceData resourceData;
  resourceData.dim = srv.dim;
  resourceData.retType = srv.compType;
  resourceData.sampleCount = srv.sampleCount;
  resourceData.binding = resRefInfo.binding;
  RDCASSERTNOTEQUAL(resourceData.retType, ResourceRetType::RETURN_TYPE_UNKNOWN);

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
      switch(srv.dim)
      {
        case DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE1D:
          uv.value.u32v[2] = mipLevelOrSampleCount;
          break;
        case DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2D:
          uv.value.u32v[3] = mipLevelOrSampleCount;
          break;
        case DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE3D:
          uv.value.u32v[3] = mipLevelOrSampleCount;
          break;
        case DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DMS:
          msIndex = mipLevelOrSampleCount;
          break;
        case DXBCBytecode::ResourceDimension::RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
          msIndex = mipLevelOrSampleCount;
          break;
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
    bool annotatedHandle;
    ShaderVariable handleVar;
    ResourceReferenceInfo samplerRef = GetResource(samplerId, annotatedHandle, handleVar);
    if(!samplerRef.Valid())
      return false;
    MarkResourceAccess(handleVar);

    RDCASSERTEQUAL(samplerRef.resClass, ResourceClass::Sampler);
    // samplerRef->resourceBase must be a Sampler
    const ResourceReferenceInfo::SamplerData &sampler = samplerRef.samplerData;
    samplerData.bias = 0.0f;
    samplerData.binding = samplerRef.binding;
    samplerData.mode = sampler.samplerMode;

    int32_t biasArg = -1;
    int32_t lodArg = -1;
    int32_t compareArg = -1;
    int32_t gatherArg = -1;
    uint32_t countOffset = 3;
    uint32_t countUV = 4;

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
      case DXOp::CalculateLOD:
        countUV = 3;
        countOffset = 0;
        break;
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
      if(GetShaderVariable(inst.args[gatherArg], opCode, dxOpCode, arg, false))
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
    if(m_ShaderType != DXBC::ShaderType::Pixel || m_QuadNeighbours.contains(~0U))
    {
      RDCERR("Undefined results using derivative instruction outside of a pixel shader.");
    }
    else
    {
      RDCASSERT(!QuadIsDiverged(workgroup, m_QuadNeighbours));
      // texture samples use coarse derivatives
      ShaderValue delta;
      for(uint32_t i = 0; i < 4; i++)
      {
        if(uvDDXY[i])
        {
          delta = DDX(false, opCode, dxOpCode, workgroup, inst.args[3 + i]);
          ddx.value.f32v[i] = delta.f32v[0];
          delta = DDY(false, opCode, dxOpCode, workgroup, inst.args[3 + i]);
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

  uint32_t instructionIdx = m_FunctionInstructionIdx - 1;

  // TODO: TextureGatherRaw // SM 6.7
  // Return types for TextureGatherRaw
  // DXGI_FORMAT_R16_UINT : u16
  // DXGI_FORMAT_R32_UINT : u32
  // DXGI_FORMAT_R32G32_UINT : u32x2

  QueueSampleGather(dxOpCode, resourceData, samplerData, uv, ddx, ddy, texelOffsets, msIndex,
                    lodValue, compareValue, gatherChannel, instructionIdx, result);
  return true;
}

void ThreadState::ConvertSampleGatherReturn(DXIL::DXOp dxOpCode, const DXIL::Instruction &inst,
                                            const ShaderVariable &data, ShaderVariable &result) const
{
  // DXIL reports the vector result as a struct of N
  // members of Element type, plus an int.
  const Type *retType = inst.type;
  if(dxOpCode != DXOp::CalculateLOD)
  {
    RDCASSERTEQUAL(retType->type, Type::TypeKind::Struct);
    const Type *baseType = retType->members[0];
    RDCASSERTEQUAL(baseType->type, Type::TypeKind::Scalar);
    result.type = ConvertDXILTypeToVarType(baseType);
    result.columns = (uint8_t)(retType->members.size() - 1);
  }
  else
  {
    RDCASSERTEQUAL(retType->type, Type::TypeKind::Scalar);
    RDCASSERTEQUAL(retType->scalarType, Type::Float);
    RDCASSERTEQUAL(result.rows, 1);
    RDCASSERTEQUAL(result.columns, 1);
  }

  // Do conversion to the return type
  if((result.type == VarType::Float) || (result.type == VarType::SInt) ||
     (result.type == VarType::UInt))
  {
    result.value = data.value;
  }
  else if(result.type == VarType::Half)
  {
    for(uint32_t col = 0; col < result.columns; ++col)
      result.value.f16v[col].set(data.value.f32v[col]);
  }
  else if(result.type == VarType::SShort)
  {
    for(uint32_t col = 0; col < result.columns; ++col)
      result.value.s16v[col] = (int16_t)data.value.s32v[col];
  }
  else if(result.type == VarType::UShort)
  {
    for(uint32_t col = 0; col < result.columns; ++col)
      result.value.u16v[col] = (uint16_t)data.value.u32v[col];
  }
  else
  {
    RDCERR("Unhandled return type %s", ToStr(result.type).c_str());
    return;
  }
}

rdcstr ThreadState::GetArgumentName(uint32_t i) const
{
  rdcstr name;
  DXILDebug::Id id = GetArgumentId(i);
  m_Program.GetSSAName(id, name);
  return name;
}

DXILDebug::Id ThreadState::GetArgumentId(uint32_t i) const
{
  DXIL::Value *arg = m_CurrentInstruction->args[i];
  return GetSSAId(arg);
}

ResourceReferenceInfo ThreadState::GetResource(Id handleId, bool &annotatedHandle,
                                               ShaderVariable &handleVar) const
{
  ResourceReferenceInfo resRefInfo;
  if(IsVariableAssigned(handleId))
  {
    RDCASSERT(m_Live[handleId]);
    RDCASSERT(IsVariableAssigned(handleId));
    handleVar = m_Variables[handleId];
    bool directAccess = handleVar.IsDirectAccess();
    ShaderBindIndex bindIndex;
    ShaderDirectAccess access;
    annotatedHandle = IsAnnotatedHandle(handleVar);
    RDCASSERT(!annotatedHandle || (m_AnnotatedProperties.count(handleId) == 1));
    rdcstr alias = handleVar.name;
    if(!directAccess)
    {
      bindIndex = handleVar.GetBindIndex();
      const ResourceReference *resRef = m_Program.GetResourceReference(handleId);
      if(resRef)
      {
        resRefInfo.Create(resRef, bindIndex.arrayElement);
      }
      else
      {
        RDCERR("Shader binding not found for handle %d", handleId);
        return resRefInfo;
      }
    }
    else
    {
      access = handleVar.GetDirectAccess();
      // Direct heap access bindings must be annotated
      RDCASSERT(annotatedHandle);
      auto directHeapAccessBinding = m_DirectHeapAccessBindings.find(handleId);
      if(directHeapAccessBinding == m_DirectHeapAccessBindings.end())
      {
        RDCERR("Direct heap access binding not found for handle %d", handleId);
        return resRefInfo;
      }
      resRefInfo = directHeapAccessBinding->second;
    }
    return resRefInfo;
  }

  RDCERR("Unknown resource handle %u", handleId);
  return resRefInfo;
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
                             const rdcarray<ThreadState> &workgroup, const DXIL::Value *dxilValue) const
{
  ShaderValue ret = {};

  if(m_QuadNeighbours[0] == ~0U || m_QuadNeighbours[1] == ~0U || m_QuadNeighbours[2] == ~0U ||
     m_QuadNeighbours[3] == ~0U)
  {
    RDCERR("Derivative calculation within non-quad");
    return ret;
  }

  RDCASSERT(m_QuadNeighbours[0] < workgroup.size(), m_QuadNeighbours[0], workgroup.size());
  RDCASSERT(m_QuadNeighbours[1] < workgroup.size(), m_QuadNeighbours[1], workgroup.size());
  RDCASSERT(m_QuadNeighbours[2] < workgroup.size(), m_QuadNeighbours[2], workgroup.size());
  RDCASSERT(m_QuadNeighbours[3] < workgroup.size(), m_QuadNeighbours[3], workgroup.size());
  RDCASSERT(!QuadIsDiverged(workgroup, m_QuadNeighbours));

  uint32_t index = ~0U;
  int quadIndex = m_QuadLaneIndex;

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

  ShaderVariable a;
  ShaderVariable b;
  RDCASSERT(GetShaderVariableFromLane(workgroup[m_QuadNeighbours[index + 1]], dxilValue, opCode,
                                      dxOpCode, a));
  RDCASSERT(GetShaderVariableFromLane(workgroup[m_QuadNeighbours[index + 0]], dxilValue, opCode,
                                      dxOpCode, b));
  Sub(a, b, ret);
  return ret;
}

ShaderValue ThreadState::DDY(bool fine, Operation opCode, DXOp dxOpCode,
                             const rdcarray<ThreadState> &workgroup, const DXIL::Value *dxilValue) const
{
  ShaderValue ret = {};

  if(m_QuadNeighbours[0] == ~0U || m_QuadNeighbours[1] == ~0U || m_QuadNeighbours[2] == ~0U ||
     m_QuadNeighbours[3] == ~0U)
  {
    RDCERR("Derivative calculation within non-quad");
    return ret;
  }

  RDCASSERT(m_QuadNeighbours[0] < workgroup.size(), m_QuadNeighbours[0], workgroup.size());
  RDCASSERT(m_QuadNeighbours[1] < workgroup.size(), m_QuadNeighbours[1], workgroup.size());
  RDCASSERT(m_QuadNeighbours[2] < workgroup.size(), m_QuadNeighbours[2], workgroup.size());
  RDCASSERT(m_QuadNeighbours[3] < workgroup.size(), m_QuadNeighbours[3], workgroup.size());
  RDCASSERT(!QuadIsDiverged(workgroup, m_QuadNeighbours));

  uint32_t index = ~0U;
  int quadIndex = m_QuadLaneIndex;

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

  ShaderVariable a;
  ShaderVariable b;
  RDCASSERT(GetShaderVariableFromLane(workgroup[m_QuadNeighbours[index + 2]], dxilValue, opCode,
                                      dxOpCode, a));
  RDCASSERT(GetShaderVariableFromLane(workgroup[m_QuadNeighbours[index + 0]], dxilValue, opCode,
                                      dxOpCode, b));
  Sub(a, b, ret);
  return ret;
}

void ThreadState::ExecuteMemoryBarrier()
{
  // ignore if not the active thread
  if(!m_HasDebugState)
    return;

  // copy the global GSM memory into the local GSM cache
  for(Id id : m_GlobalState.groupSharedMemoryIds)
  {
    auto globalMem = m_GlobalState.memory.m_Allocations.find(id);
    if(globalMem == m_GlobalState.memory.m_Allocations.end())
    {
      RDCERR("Unknown global memory allocation for GSM Id %u", id);
      continue;
    }
    RDCASSERT(globalMem->second.globalVarAlloc);
    RDCASSERT(!globalMem->second.localMemory);

    auto localMem = m_Memory.m_Allocations.find(id);
    if(localMem == m_Memory.m_Allocations.end())
    {
      RDCERR("Unknown local memory allocation for GSM Id %u", id);
      continue;
    }
    RDCASSERT(localMem->second.globalVarAlloc);
    RDCASSERT(localMem->second.localMemory);

    if(!IsVariableAssigned(id))
    {
      RDCERR("Unknown local memory allocation for GSM Id %u", id);
      continue;
    }
    ShaderVariableChange change;
    const ShaderVariable &local = m_Variables[id];
    change.before = local;
    const void *globalBackingMemory = globalMem->second.backingMemory;
    UpdateMemoryVariableFromBackingMemory(id, globalBackingMemory);
    change.after = local;
    if(!(change.after == change.before))
      m_PendingDebugState.changes.push_back(change);

    // Update local backing memory from the local variable
    RDCASSERTEQUAL(globalMem->second.size, localMem->second.size);
    void *localBackingMemory = localMem->second.backingMemory;
    const size_t allocSize = (size_t)globalMem->second.size;
    memcpy(localBackingMemory, globalBackingMemory, allocSize);
  }
}

GlobalState::~GlobalState()
{
  for(auto it : memory.m_Allocations)
  {
    RDCASSERT(!it.second.localMemory);
    free(it.second.backingMemory);
  }
}

bool ThreadState::WorkgroupIsDiverged(const rdcarray<ThreadState> &workgroup)
{
  uint32_t block0 = ~0U;
  uint32_t instr0 = ~0U;
  for(size_t i = 0; i < workgroup.size(); i++)
  {
    if(workgroup[i].Finished())
      continue;
    if(block0 == ~0U)
    {
      block0 = workgroup[i].m_CurrentBlock;
      instr0 = workgroup[i].m_CurrentGlobalInstructionIdx;
      continue;
    }
    // not in the same basic block
    if(workgroup[i].m_CurrentBlock != block0)
      return true;
    // not executing the same instruction
    if(workgroup[i].m_CurrentGlobalInstructionIdx != instr0)
      return true;
  }
  return false;
}

bool ThreadState::SubgroupIsDiverged(const rdcarray<ThreadState> &workgroup,
                                     const rdcarray<uint32_t> &activeLanes)
{
  uint32_t block0 = ~0U;
  uint32_t instr0 = ~0U;
  for(uint32_t lane : activeLanes)
  {
    if(workgroup[lane].Finished())
      continue;
    if(block0 == ~0U)
    {
      block0 = workgroup[lane].m_CurrentBlock;
      instr0 = workgroup[lane].m_CurrentGlobalInstructionIdx;
      continue;
    }
    // not in the same basic block
    if(workgroup[lane].m_CurrentBlock != block0)
      return true;
    // not executing the same instruction
    if(workgroup[lane].m_CurrentGlobalInstructionIdx != instr0)
      return true;
  }
  return false;
}

bool ThreadState::QuadIsDiverged(const rdcarray<ThreadState> &workgroup,
                                 const rdcfixedarray<uint32_t, 4> &quadNeighbours)
{
  uint32_t block0 = ~0U;
  uint32_t instr0 = ~0U;
  for(size_t q = 0; q < quadNeighbours.size(); q++)
  {
    uint32_t i = quadNeighbours[q];
    if(i == ~0U)
    {
      RDCERR("Checking quad divergence on non-quad");
      continue;
    }

    if(workgroup[i].Finished())
      continue;
    if(block0 == ~0U)
    {
      block0 = workgroup[i].m_CurrentBlock;
      instr0 = workgroup[i].m_CurrentGlobalInstructionIdx;
      continue;
    }
    // not in the same basic block
    if(workgroup[i].m_CurrentBlock != block0)
      return true;
    // not executing the same instruction
    if(workgroup[i].m_CurrentGlobalInstructionIdx != instr0)
      return true;
  }
  return false;
}

// The conditions where it is not safe to run another step are based on:
// the current simulation state and the next instruction to simulate
bool ThreadState::CanRunAnotherStep() const
{
  // Thread has finished
  if(Finished())
    return false;

  // Current Simulated State that prevents running another step:
  // Any control flow state changes i.e. branch, convergence point, partial convergence
  if(m_Diverged)
    return false;
  if(!m_EnteredPoints.empty())
    return false;
  if(m_ConvergencePoint != INVALID_EXECUTION_POINT)
    return false;
  if(!m_PartialConvergencePoints.empty())
    return false;

  // Any pending result i.e. pending GPU math operation, need to run on the device thread
  if(IsPendingResultPending())
    return false;

  // current instructions that require full lockstep
  const Instruction *inst = m_CurrentInstruction;
  Operation opCode = inst->op;
  DXOp dxOpCode = DXOp::NumOpCodes;
  switch(opCode)
  {
    case Operation::Call:
    {
      const Function *callFunc = inst->getFuncCall();
      if(callFunc->family == FunctionFamily::DXOp)
      {
        RDCASSERT(getival<DXOp>(inst->args[0], dxOpCode));
        RDCASSERT(dxOpCode < DXOp::NumOpCodes, dxOpCode, DXOp::NumOpCodes);
        switch(dxOpCode)
        {
          // no thread can continue until all threads execute the barrier
          case DXOp::Barrier: return false;
          default: break;
        }
      }
    }
    default: break;
  }

  // Next instructions that prevent running another step:
  // any instruction that requires threads in the tangle to be in lockstep
  inst = m_FunctionInfo->function->instructions[m_FunctionInstructionIdx];
  opCode = inst->op;
  dxOpCode = DXOp::NumOpCodes;
  switch(opCode)
  {
    case Operation::Call:
    {
      const Function *callFunc = inst->getFuncCall();
      if(callFunc->family == FunctionFamily::DXOp)
      {
        RDCASSERT(getival<DXOp>(inst->args[0], dxOpCode));
        RDCASSERT(dxOpCode < DXOp::NumOpCodes, dxOpCode, DXOp::NumOpCodes);
        switch(dxOpCode)
        {
          // thread barriers require threads in the tangle to be in lockstep
          case DXOp::Barrier:
            return false;
            // Image operations require threads in the tangle to be in lockstep
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
          case DXOp::CalculateLOD: return false;
          case DXOp::TextureLoad:
            // TextureLoad does not require derivatives, does not have to be in lockstep
            return true;
            // derivatives require threads in the tangle to be in lockstep
          case DXOp::DerivCoarseX:
          case DXOp::DerivCoarseY:
          case DXOp::DerivFineX:
          case DXOp::DerivFineY: return false;
          // wave/subgroup ops require threads in the tangle to be in lockstep
          case DXOp::WaveIsFirstLane:
          case DXOp::WaveReadLaneAt:
          case DXOp::WaveReadLaneFirst:
          case DXOp::WavePrefixOp:
          case DXOp::WavePrefixBitCount:
          case DXOp::WaveAllBitCount:
          case DXOp::WaveAnyTrue:
          case DXOp::WaveAllTrue:
          case DXOp::WaveActiveBallot:
          case DXOp::WaveActiveAllEqual:
          case DXOp::WaveActiveOp:
          case DXOp::WaveActiveBit:
          case DXOp::WaveMatch:
          case DXOp::WaveMultiPrefixOp:
          case DXOp::WaveMultiPrefixBitCount:
          case DXOp::QuadReadLaneAt:
          case DXOp::QuadOp:
          case DXOp::QuadVote: return false;
          default: break;
        }
      }
    }
    default: break;
  }
  return true;
}

void ThreadState::QueueMathOp(DXIL::DXOp dxOp, const ShaderVariable &input, ShaderVariable &result)
{
  DXIL_DEBUG_RDCASSERT(!IsPendingResultPending());
  m_PendingResultData = result;
  m_QueuedGpuMathOp.workgroupIndex = m_WorkgroupIndex;
  m_QueuedGpuMathOp.dxOp = dxOp;
  m_QueuedGpuMathOp.input = input;
  m_QueuedGpuMathOp.result = &m_PendingResultData;
  SetStepNeedsGpuMathOp();
}

void ThreadState::QueueSampleGather(DXIL::DXOp dxOp, const SampleGatherResourceData &resourceData,
                                    const SampleGatherSamplerData &samplerData,
                                    const ShaderVariable &uv, const ShaderVariable &ddxCalc,
                                    const ShaderVariable &ddyCalc, const int8_t texelOffsets[3],
                                    int multisampleIndex, float lodValue, float compareValue,
                                    GatherChannel gatherChannel, uint32_t instructionIdx,
                                    ShaderVariable &result)
{
  DXIL_DEBUG_RDCASSERT(!IsPendingResultPending());
  m_PendingResultData = result;
  m_QueuedGpuSampleGatherOp.workgroupIndex = m_WorkgroupIndex;
  m_QueuedGpuSampleGatherOp.dxOp = dxOp;
  m_QueuedGpuSampleGatherOp.resourceData = resourceData;
  m_QueuedGpuSampleGatherOp.samplerData = samplerData;
  m_QueuedGpuSampleGatherOp.uv = uv;
  m_QueuedGpuSampleGatherOp.ddxCalc = ddxCalc;
  m_QueuedGpuSampleGatherOp.ddyCalc = ddyCalc;
  m_QueuedGpuSampleGatherOp.texelOffsets[0] = texelOffsets[0];
  m_QueuedGpuSampleGatherOp.texelOffsets[1] = texelOffsets[1];
  m_QueuedGpuSampleGatherOp.texelOffsets[2] = texelOffsets[2];
  m_QueuedGpuSampleGatherOp.multisampleIndex = multisampleIndex;
  m_QueuedGpuSampleGatherOp.lodValue = lodValue;
  m_QueuedGpuSampleGatherOp.compareValue = compareValue;
  m_QueuedGpuSampleGatherOp.gatherChannel = gatherChannel;
  m_QueuedGpuSampleGatherOp.instructionIdx = instructionIdx;
  m_QueuedGpuSampleGatherOp.result = &m_PendingResultData;
  SetStepNeedsGpuSampleGatherOp();
}

void ThreadState::OperationLoad(bool isAtomic, const DXIL::Instruction &inst, DXIL::Operation opCode,
                                DXIL::DXOp dxOpCode, Id &resultId, ShaderVariable &result)
{
  if(DXIL::IsDXCNop(inst))
  {
    resultId = DXILDebug::INVALID_ID;
    result.name.clear();
    return;
  }

  // Load(ptr)
  Id ptrId = GetArgumentId(0);
  if(ptrId == DXILDebug::INVALID_ID)
    return;

  auto itPtr = m_Memory.m_Pointers.find(ptrId);
  if(itPtr == m_Memory.m_Pointers.end())
  {
    RDCERR("Unknown memory pointer Id %u", ptrId);
    return;
  }

  const MemoryTracking::Pointer &ptr = itPtr->second;
  Id baseMemoryId = ptr.baseMemoryId;

  auto itAlloc = m_Memory.m_Allocations.find(baseMemoryId);
  if(itAlloc == m_Memory.m_Allocations.end())
  {
    RDCERR("Unknown memory allocation Id %u", baseMemoryId);
    return;
  }

  const MemoryTracking::Allocation &allocation = itAlloc->second;
  // active lane: Atomic Load for GSM then read from the global backing memory
  if(m_HasDebugState && isAtomic && allocation.gsm)
  {
    if(!LoadGSMFromGlobalBackingMemory(ptr, allocation, result))
      RDCERR("OperationLoad: LoadGSMFromGlobalBackingMemory failed ptrId %u", ptrId);
    return;
  }

  // Load from local backing memory
  RDCASSERT(ptr.size <= sizeof(ShaderValue));
  if(ptr.size <= sizeof(ShaderValue))
    memcpy(&result.value, ptr.memory, (size_t)ptr.size);
  else
    RDCERR("Size %u too large MAX %u for OperationLoad", ptr.size, sizeof(ShaderValue));
}

void ThreadState::OperationStore(const DXIL::Instruction &inst, DXIL::Operation opCode,
                                 DXIL::DXOp dxOpCode)
{
  // Store(ptr, value)
  Id ptrId = GetArgumentId(0);
  if(ptrId == DXILDebug::INVALID_ID)
    return;
  auto itPtr = m_Memory.m_Pointers.find(ptrId);
  if(itPtr == m_Memory.m_Pointers.end())
  {
    RDCERR("Unknown memory pointer Id %u", ptrId);
    return;
  }

  ShaderVariable originalValue(m_Variables[ptrId]);

  const MemoryTracking::Pointer &ptr = itPtr->second;
  const Id baseMemoryId = ptr.baseMemoryId;
  void *const memory = ptr.memory;
  uint64_t allocSize = ptr.size;

  RDCASSERT(memory);
  RDCASSERTNOTEQUAL(baseMemoryId, DXILDebug::INVALID_ID);

  ShaderVariable val;
  RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, val));

  UpdateBackingMemoryFromVariable(memory, allocSize, val);

  bool recordBaseMemoryChange = m_HasDebugState && baseMemoryId != ptrId;
  ShaderVariableChange change;
  RDCASSERT(IsVariableAssigned(baseMemoryId));
  if(recordBaseMemoryChange)
    change.before = m_Variables[baseMemoryId];

  auto itAlloc = m_Memory.m_Allocations.find(baseMemoryId);
  if(itAlloc == m_Memory.m_Allocations.end())
  {
    RDCERR("Unknown memory allocation Id %u", baseMemoryId);
    return;
  }
  const MemoryTracking::Allocation &allocation = itAlloc->second;
  UpdateMemoryVariableFromBackingMemory(baseMemoryId, allocation.backingMemory);

  // active lane : writes to a GSM variable, write to local and global backing memory
  if(m_HasDebugState && allocation.gsm)
    UpdateGlobalBackingMemory(ptrId, ptr, allocation, val);

  // record the change to the base memory variable if it is not the ptrId variable
  if(recordBaseMemoryChange)
  {
    if(!IsVariableAssigned(baseMemoryId))
    {
      change.before = {};
      m_Assigned[baseMemoryId] = true;
    }
    change.after = m_Variables[baseMemoryId];
    m_PendingDebugState.changes.push_back(change);
  }

  // Update the ptr variable value and manually record the change to the ptr variable
  RDCASSERT(IsVariableAssigned(ptrId));
  ShaderVariable newValue(originalValue);
  newValue.value = val.value;

  m_Live[ptrId] = true;
  m_Variables[ptrId] = newValue;
  m_Assigned[ptrId] = true;

  if(m_HasDebugState)
  {
    change.before = originalValue;
    change.after = newValue;
    m_PendingDebugState.changes.push_back(change);
  }
}

void ThreadState::OperationAtomic(const DXIL::Instruction &inst, DXIL::Operation opCode,
                                  DXIL::DXOp dxOpCode, Id &resultId, ShaderVariable &result)
{
  Id ptrId = GetArgumentId(0);
  if(ptrId == DXILDebug::INVALID_ID)
    return;

  auto itPtr = m_Memory.m_Pointers.find(ptrId);
  if(itPtr == m_Memory.m_Pointers.end())
  {
    RDCERR("Unknown memory pointer Id %u", ptrId);
    return;
  }

  const MemoryTracking::Pointer &ptr = itPtr->second;
  const Id baseMemoryId = ptr.baseMemoryId;

  RDCASSERTNOTEQUAL(baseMemoryId, DXILDebug::INVALID_ID);

  void *const memory = ptr.memory;
  RDCASSERT(memory);
  uint64_t allocSize = ptr.size;

  auto itAlloc = m_Memory.m_Allocations.find(baseMemoryId);
  if(itAlloc == m_Memory.m_Allocations.end())
  {
    RDCERR("Unknown memory allocation Id %u", ptrId);
    return;
  }
  const MemoryTracking::Allocation &allocation = itAlloc->second;
  void *allocMemoryBackingPtr = allocation.backingMemory;

  RDCASSERTNOTEQUAL(resultId, DXILDebug::INVALID_ID);
  RDCASSERT(IsVariableAssigned(ptrId));
  ShaderVariable a;
  if(allocation.globalVarAlloc && !IsVariableAssigned(ptrId))
  {
    RDCASSERT(IsVariableAssigned(baseMemoryId));
    a = m_Variables[baseMemoryId];
  }
  else
  {
    a = m_Variables[ptrId];
  }

  // Get the underling type of the GPUPointer
  if(a.type == VarType::GPUPointer)
  {
    Id id;
    uint64_t offset;
    uint64_t size;
    VarType baseType;
    // Decode the pointer allocation: ptrId, offset, size
    RDCASSERT(DecodePointer(id, offset, size, baseType, a));
    RDCASSERTEQUAL(baseMemoryId, id);
    RDCASSERTEQUAL(allocSize, size);
    a.type = baseType;
  }

  // GSM variable, read from the global backing memory
  if(allocation.gsm)
  {
    if(!LoadGSMFromGlobalBackingMemory(ptr, allocation, a))
    {
      RDCERR("OperationAtomic: LoadGSMFromGlobalBackingMemory failed ptrId %u", ptrId);
      return;
    }
  }

  size_t newValueArgIdx = (opCode == Operation::CompareExchange) ? 2 : 1;
  ShaderVariable b;
  RDCASSERT(GetShaderVariable(inst.args[newValueArgIdx], opCode, dxOpCode, b));
  const uint32_t c = 0;

  ShaderVariable res = a;
  bool matched = false;

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
  else if(opCode == Operation::CompareExchange)
  {
    ShaderVariable cmp;
    RDCASSERT(GetShaderVariable(inst.args[1], opCode, dxOpCode, cmp));

#undef _IMPL
#define _IMPL(I, S, U)                        \
  matched = comp<I>(a, c) == comp<I>(cmp, c); \
  comp<I>(res, c) = matched ? comp<I>(b, c) : comp<I>(a, c)

    IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, b.type);
  }
  else
  {
    RDCERR("Unhandled opCode %s", ToStr(opCode).c_str());
  }

  // Save the result back to the backing memory of the pointer
  UpdateBackingMemoryFromVariable(memory, allocSize, res);

  bool recordBaseMemoryChange = m_HasDebugState && baseMemoryId != resultId;
  ShaderVariableChange change;
  if(recordBaseMemoryChange)
    change.before = m_Variables[baseMemoryId];

  UpdateMemoryVariableFromBackingMemory(baseMemoryId, allocMemoryBackingPtr);

  // active lane : writes to a GSM variable, write to local and global backing memory
  if(m_HasDebugState && allocation.gsm)
    UpdateGlobalBackingMemory(ptrId, ptr, allocation, res);

  // record the change to the base memory variable
  if(recordBaseMemoryChange)
  {
    if(!IsVariableAssigned(baseMemoryId))
    {
      change.before = {};
      m_Assigned[baseMemoryId] = true;
    }
    change.after = m_Variables[baseMemoryId];
    m_PendingDebugState.changes.push_back(change);
  }

  // record the change to the ptr variable value
  bool recordPtrMemoryChange = m_HasDebugState && (ptrId != resultId) && (baseMemoryId != ptrId);
  RDCASSERT(IsVariableAssigned(ptrId));
  if(recordPtrMemoryChange)
    change.before = m_Variables[ptrId];
  // Update the ptr variable value
  m_Variables[ptrId].value = res.value;

  if(recordPtrMemoryChange)
  {
    change.after = m_Variables[ptrId];
    m_PendingDebugState.changes.push_back(change);
  }

  RDCASSERTNOTEQUAL(resultId, ptrId);
  if(opCode == Operation::CompareExchange)
  {
    // Returns a struct
    // { Original Value,  1 (equal) / 0 (not equal)
    result.rows = 0;
    result.columns = 0;
    RDCASSERTEQUAL(result.type, VarType::Struct);
    result.members.clear();
    result.members.resize(2);
    result.members[0] = res;
    result.members[0].name = "original";
    result.members[1].rows = 1;
    result.members[1].columns = 1;
    result.members[1].type = VarType::Bool;
    result.members[1].name = "flag";
    result.members[1].value.u32v[0] = matched ? 1 : 0;
  }
  else
  {
    result.value = res.value;
  }
}

Debugger::DebugInfo::~DebugInfo()
{
  for(const ScopedDebugData *scope : scopedDebugDatas)
    delete scope;
  scopedDebugDatas.clear();
}

// static helper function
rdcstr Debugger::GetResourceBaseName(const DXIL::Program *program,
                                     const DXIL::ResourceReference *resRef)
{
  rdcstr resName = resRef->resourceBase.name;
  // Special case for cbuffer arrays
  if((resRef->resourceBase.resClass == ResourceClass::CBuffer) && (resRef->resourceBase.regCount > 1))
  {
    // Remove any array suffix that might have been appended to the resource name
    int offs = resName.find('[');
    if(offs > 0)
      resName = resName.substr(0, offs);
  }
  return resName;
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
    if(resRef.resourceBase.regBase + resRef.resourceBase.regCount <= slot.shaderRegister)
      continue;

    return GetResourceBaseName(program, &resRef);
  }
  RDCERR("Failed to find DXIL %s Resource Space %d Register %d", ToStr(resClass).c_str(),
         slot.registerSpace, slot.shaderRegister);
  return "UNKNOWN_RESOURCE_HANDLE";
}

Debugger::Debugger() : DXBCContainerDebugger(true), m_DeviceThreadID(Threading::GetCurrentID()){};

// Must be called from the replay manager thread (the debugger thread)
Debugger::~Debugger()
{
  CHECK_DEBUGGER_THREAD();
  AtomicStore(&atomic_simulationFinished, 1);
  Threading::JobSystem::SyncAllJobs();
  SAFE_DELETE(m_ApiWrapper);
}

ScopedDebugData *Debugger::FindScopedDebugData(const DXIL::Metadata *md) const
{
  for(ScopedDebugData *s : m_DebugInfo.scopedDebugDatas)
  {
    if(s->md == md)
      return s;
  }
  return NULL;
}

const DXIL::Metadata *Debugger::GetMDScope(const DXIL::Metadata *scopeMD) const
{
  // Iterate upwards to find DIFile, DISubprogram or DILexicalBlock scope
  while(scopeMD && (scopeMD->dwarf->type != DIBase::File) &&
        (scopeMD->dwarf->type != DIBase::Subprogram) &&
        (scopeMD->dwarf->type != DIBase::LexicalBlock))
    scopeMD = m_Program->GetDebugScopeParent(scopeMD->dwarf);

  return scopeMD;
}

ScopedDebugData *Debugger::AddScopedDebugData(const DXIL::Metadata *scopeMD)
{
  scopeMD = GetMDScope(scopeMD);
  if(scopeMD == NULL)
    return NULL;
  ScopedDebugData *scope = FindScopedDebugData(scopeMD);
  // Add a new DebugScope
  if(!scope)
  {
    // Find the parent scope and add this to its children
    const DXIL::Metadata *parentScope = m_Program->GetDebugScopeParent(scopeMD->dwarf);

    scope = new ScopedDebugData();
    scope->md = scopeMD;
    scope->maxInstruction = 0;
    // File scope should not have a parent
    if(scopeMD->dwarf->type == DIBase::File)
    {
      RDCASSERT(!parentScope);
      scope->parent = NULL;
      scope->functionName = "File";
    }
    else
    {
      RDCASSERT(parentScope);
      scope->parent = AddScopedDebugData(parentScope);
      RDCASSERT(scope->parent);
      if(scopeMD->dwarf->type == DIBase::Subprogram)
        scope->functionName = *(scopeMD->dwarf->As<DISubprogram>()->name);
      else if(scopeMD->dwarf->type == DIBase::CompileUnit)
        scope->functionName = "CompileUnit";
    }

    scope->fileName = m_Program->GetDebugScopeFilePath(scope->md->dwarf);
    scope->line = (uint32_t)m_Program->GetDebugScopeLine(scope->md->dwarf);

    m_DebugInfo.scopedDebugDatas.push_back(scope);
  }
  return scope;
}

void Debugger::AddStructMembers(const DXIL::DICompositeType *structTypeData, TypeData &structType)
{
  if(structTypeData->type != DXIL::DIBase::Type::CompositeType)
  {
    RDCERR("Invalid dwarf struct type %s", ToStr(structTypeData->type).c_str());
    return;
  }

  if(structTypeData->tag != DXIL::DW_TAG_structure_type &&
     structTypeData->tag != DXIL::DW_TAG_class_type)
  {
    RDCERR("Invalid composite tag %s", ToStr(structTypeData->tag).c_str());
    return;
  }

  const Metadata *elementsMD = structTypeData->elements;
  size_t countMembers = elementsMD->children.size();
  for(size_t i = 0; i < countMembers; ++i)
  {
    const Metadata *memberMD = elementsMD->children[i];
    const DXIL::DIBase *memberBase = memberMD->dwarf;
    // Ignore member functions
    if(memberBase->type == DXIL::DIBase::Subprogram)
      continue;
    RDCASSERTEQUAL(memberBase->type, DXIL::DIBase::DerivedType);
    // Ignore anything that isn't DIBase::DerivedType
    if(memberBase->type != DXIL::DIBase::DerivedType)
      continue;

    const DXIL::DIDerivedType *member = memberBase->As<DIDerivedType>();
    if(member->tag == DXIL::DW_TAG_inheritance)
    {
      const Metadata *parentMD = member->base;
      const DXIL::DIBase *parentBase = parentMD->dwarf;
      RDCASSERTEQUAL(parentBase->type, DXIL::DIBase::Type::CompositeType);
      const DICompositeType *parentType = parentBase->As<DICompositeType>();
      AddStructMembers(parentType, structType);
      continue;
    }
    // Ignore any member tag that isn't DXIL::DW_TAG_member
    if(member->tag != DXIL::DW_TAG_member)
      continue;
    AddDebugType(member->base);
    RDCASSERT(member->name);
    rdcstr memberName = member->name ? *member->name : "NULL";
    structType.structMembers.push_back({memberName, member->base});
    uint32_t offset = (uint32_t)member->offsetInBits / 8;
    structType.memberOffsets.push_back(offset);
  }
}

const TypeData &Debugger::AddDebugType(const DXIL::Metadata *typeMD)
{
  {
    auto it = m_DebugInfo.types.find(typeMD);
    if(it != m_DebugInfo.types.end())
      return it->second;
  }

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
          typeData.sizeInBytes = (uint32_t)(compositeType->sizeInBits / 8);
          typeData.alignInBytes = (uint32_t)(compositeType->alignInBits / 8);

          bool isVector = compositeType->name && compositeType->name->beginsWith("vector<");
          bool isMatrix =
              compositeType->name && !isVector && compositeType->name->beginsWith("matrix<");

          if((compositeType->templateParams) && (isVector || isMatrix))
          {
            const Metadata *params = compositeType->templateParams;
            uint32_t countParams = (uint32_t)params->children.size();
            if(isVector)
              RDCASSERTEQUAL(countParams, 2);
            else if(isMatrix)
              RDCASSERTEQUAL(countParams, 3);
            // Vector needs at least two parameters
            isVector &= (countParams >= 2);
            // Matrix needs at least three parameters
            isMatrix &= (countParams >= 3);
          }

          if((compositeType->templateParams) && (isVector || isMatrix))
          {
            const Metadata *params = compositeType->templateParams;
            {
              RDCASSERTEQUAL(params->children[1]->dwarf->type, DXIL::DIBase::TemplateValueParameter);
              const DITemplateValueParameter *firstDim =
                  params->children[1]->dwarf->As<DITemplateValueParameter>();

              // don't need the template value parameter name, it should be 'element_count' or
              // 'row_count', just need the value
              RDCASSERT(getival<uint32_t>(firstDim->value->value, typeData.vecSize));
            }

            if(isMatrix)
            {
              RDCASSERTEQUAL(params->children[2]->dwarf->type, DXIL::DIBase::TemplateValueParameter);
              const DITemplateValueParameter *secondDim =
                  params->children[2]->dwarf->As<DITemplateValueParameter>();

              // don't need the template value parameter name, it should be 'col_count', just need the value
              RDCASSERT(getival<uint32_t>(secondDim->value->value, typeData.matSize));

              // treat all matrices as row major. n rows of vector<m>
              uint32_t rows = typeData.vecSize;
              uint32_t cols = typeData.matSize;

              typeData.colMajorMat = false;
              typeData.vecSize = cols;
              typeData.matSize = rows;
            }

            RDCASSERTEQUAL(params->children[0]->dwarf->type, DXIL::DIBase::TemplateTypeParameter);
            const DITemplateTypeParameter *baseType =
                params->children[0]->dwarf->As<DITemplateTypeParameter>();

            typeData.baseType = baseType->type;

            // don't need the template type parameter name, it should be 'element', just need the base type
            const TypeData &baseTypeData = AddDebugType(typeData.baseType);

            typeData.type = baseTypeData.type;

            if(isVector)
              typeData.name =
                  StringFormat::Fmt("%s%u", ToStr(typeData.type).c_str(), typeData.vecSize);
            else if(isMatrix)
              typeData.name = StringFormat::Fmt("%s%ux%u", ToStr(typeData.type).c_str(),
                                                typeData.matSize, typeData.vecSize);
          }
          else
          {
            typeData.name = compositeType->name ? *compositeType->name
                                                : StringFormat::Fmt("__anon%u", compositeType->line);

            RDCASSERT(!isVector && !isMatrix, isVector, isMatrix, typeData.name);

            typeData.type = VarType::Struct;
            AddStructMembers(compositeType, typeData);
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
          AddDebugType(compositeType->base);
          typeData.baseType = compositeType->base;
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
        case DW_TAG_restrict_type:
        case DW_TAG_const_type:
        case DW_TAG_reference_type:
        case DW_TAG_pointer_type:
        case DW_TAG_typedef: typeData = AddDebugType(derivedType->base); break;
        default:
          RDCERR("Unhandled DIDerivedType DIDerivedType Tag type %s",
                 ToStr(derivedType->tag).c_str());
          if(derivedType->base)
            typeData = AddDebugType(derivedType->base);
          break;
      }
      break;
    }
    default: RDCERR("Unhandled DXIL type %s", ToStr(base->type).c_str()); break;
  }

  m_DebugInfo.types[typeMD] = typeData;
  return m_DebugInfo.types[typeMD];
}

void Debugger::AddLocalVariable(const DXIL::SourceMappingInfo &srcMapping, uint32_t instructionIndex)
{
  ScopedDebugData *scope = AddScopedDebugData(srcMapping.localVariable->scope);

  LocalMapping localMapping;
  localMapping.sourceVarName = m_Program->GetDebugVarName(srcMapping.localVariable);
  localMapping.variable = srcMapping.localVariable;
  localMapping.debugVarSSAName = srcMapping.dbgVarName;
  localMapping.debugVarSSAId = srcMapping.dbgVarId;
  localMapping.byteOffset = srcMapping.srcByteOffset;
  localMapping.countBytes = srcMapping.srcCountBytes;
  localMapping.isDeclare = srcMapping.isDeclare;
  localMapping.instIndex = instructionIndex;

  scope->localMappings.push_back(localMapping);

  const DXIL::Metadata *typeMD = srcMapping.localVariable->type;
  if(m_DebugInfo.types.count(typeMD) == 0)
    AddDebugType(typeMD);

  if(m_DebugInfo.locals.count(srcMapping.localVariable) == 0)
    m_DebugInfo.locals[srcMapping.localVariable] = localMapping;
}

void Debugger::ParseDbgOpDeclare(const DXIL::Instruction &inst, uint32_t instructionIndex)
{
  DXIL::SourceMappingInfo sourceMappingInfo = m_Program->ParseDbgOpDeclare(inst);
  AddLocalVariable(sourceMappingInfo, instructionIndex);
}

void Debugger::ParseDbgOpValue(const DXIL::Instruction &inst, uint32_t instructionIndex)
{
  DXIL::SourceMappingInfo sourceMappingInfo = m_Program->ParseDbgOpValue(inst);
  AddLocalVariable(sourceMappingInfo, instructionIndex);
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::ParseDebugData()
{
  CHECK_DEBUGGER_THREAD();
  // The scopes will have been created when parsing to generate the callstack information
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      const FunctionInfo &info = m_FunctionInfos[f];
      uint32_t countInstructions = (uint32_t)f->instructions.size();

      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        uint32_t instructionIndex = i + info.globalInstructionOffset;
        const Instruction &inst = *(f->instructions[i]);
        if(!DXIL::IsLLVMDebugCall(inst))
        {
          // Check the scope exists for this instruction
          uint32_t dbgLoc = ShouldIgnoreSourceMapping(inst) ? ~0U : inst.debugLoc;
          if(dbgLoc != ~0U)
          {
            const DebugLocation &debugLoc = m_Program->m_DebugLocations[dbgLoc];
            const DXIL::Metadata *debugLocScopeMD = GetMDScope(debugLoc.scope);
            ScopedDebugData *scope = FindScopedDebugData(debugLocScopeMD);
            RDCASSERT(scope);
            if(scope == NULL)
              scope = AddScopedDebugData(debugLoc.scope);
            RDCASSERT(scope->md == debugLoc.scope);
          }
          continue;
        }

        const Function *dbgFunc = inst.getFuncCall();
        switch(dbgFunc->llvmIntrinsicOp)
        {
          case LLVMIntrinsicOp::DbgDeclare: ParseDbgOpDeclare(inst, instructionIndex); break;
          case LLVMIntrinsicOp::DbgValue: ParseDbgOpValue(inst, instructionIndex); break;
          case LLVMIntrinsicOp::Unknown:
          default: RDCASSERT("Unsupported LLVM debug operation", dbgFunc->llvmIntrinsicOp); break;
        };
      }
    }
  }

  DXIL::Program *program = ((DXIL::Program *)m_Program);
  program->m_Locals.clear();

  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      const FunctionInfo &info = m_FunctionInfos[f];
      uint32_t countInstructions = (uint32_t)f->instructions.size();

      program->m_Locals.reserve(countInstructions);
      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        if(f->instructions[i]->debugLoc == ~0U)
          continue;

        uint32_t instructionIndex = i + info.globalInstructionOffset;

        DXIL::Program::LocalSourceVariable localSrcVar;
        localSrcVar.startInst = instructionIndex;
        localSrcVar.endInst = instructionIndex;

        // For each instruction - find which scope it belongs
        const DebugLocation &debugLoc = m_Program->m_DebugLocations[f->instructions[i]->debugLoc];
        const ScopedDebugData *scope = FindScopedDebugData(GetMDScope(debugLoc.scope));
        // track which mappings we've processed, so if the same variable has mappings in multiple
        // scopes we only pick the innermost.
        rdcarray<LocalMapping> processed;
        rdcarray<const DXIL::DILocalVariable *> sourceVars;

        // capture the scopes upwards (from child to parent)
        rdcarray<const ScopedDebugData *> scopes;
        while(scope)
        {
          // Only add add scopes with mappings
          if(!scope->localMappings.empty())
            scopes.push_back(scope);

          // if we reach a function scope, don't go up any further.
          if(scope->md->dwarf->type == DIBase::Type::Subprogram)
            break;

          scope = scope->parent;
        }

        // Iterate over the scopes downwards (parent->child)
        for(size_t s = 0; s < scopes.size(); ++s)
        {
          scope = scopes[scopes.size() - 1 - s];
          size_t countLocalMappings = scope->localMappings.size();
          for(size_t m = 0; m < countLocalMappings; m++)
          {
            const LocalMapping &mapping = scope->localMappings[m];

            // TODO: this should be using ExecPointReference::IsAfter()
            if(mapping.instIndex > instructionIndex)
              continue;

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
                const LocalMapping &laterMapping = scope->localMappings[n];

                // TODO: this should be using ExecPointReference::IsAfter()
                if(laterMapping.instIndex > instructionIndex)
                  continue;

                // TODO: this should be using ExecPointReference::IsAfter()
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
                  rows = RDCMAX(1U, typeWalk->vecSize);
                  columns = RDCMAX(1U, typeWalk->matSize);
                }
                else
                {
                  columns = RDCMAX(1U, typeWalk->vecSize);
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
                      childRows = RDCMAX(1U, childType->vecSize);
                      childColumns = RDCMAX(1U, childType->matSize);
                    }
                    else
                    {
                      childColumns = RDCMAX(1U, childType->vecSize);
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
                    // TODO: mapping covers whole sub-array
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
                  if(memberIndex >= rows)
                  {
                    RDCERR("Invalid memberIndex for source variable %s SSAID %s",
                           mapping.sourceVarName.c_str(), mapping.debugVarSSAName.c_str());
                    memberIndex = 0;
                    byteOffset = 0;
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
                          memberRows = RDCMAX(1U, memberType->vecSize);
                          memberColumns = RDCMAX(1U, memberType->matSize);
                        }
                        else
                        {
                          memberColumns = RDCMAX(1U, memberType->vecSize);
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
                  rows = RDCMAX(1U, typeWalk->vecSize);
                  columns = RDCMAX(1U, typeWalk->matSize);
                }
                else
                {
                  columns = RDCMAX(1U, typeWalk->vecSize);
                  rows = RDCMAX(1U, typeWalk->matSize);
                }
                usage->rows = rows;
                usage->columns = columns;

                if((bytesRemaining > 0) && (bytesRemaining != scalar.sizeInBytes * rows * columns))
                {
                  if(usage->children.isEmpty())
                  {
                    // Matrices are stored as [row][col]
                    const char swizzle[] = "xyzw";
                    usage->children.resize(rows);
                    for(uint32_t r = 0; r < rows; ++r)
                    {
                      usage->children[r].emitSourceVar = false;
                      usage->children[r].name = usage->name + StringFormat::Fmt(".row%u", r);
                      usage->children[r].type = scalar.type;
                      usage->children[r].debugVarSSAName = usage->debugVarSSAName;
                      usage->children[r].debugVarComponent = 0;
                      usage->children[r].rows = 1U;
                      usage->children[r].columns = columns;
                      usage->children[r].offset = usage->offset + r * rows;
                      usage->children[r].children.resize(columns);
                      for(uint32_t c = 0; c < columns; ++c)
                      {
                        usage->children[r].children[c].emitSourceVar = false;
                        usage->children[r].children[c].name =
                            usage->name + StringFormat::Fmt(".row%u.%c", r, swizzle[RDCMIN(c, 3U)]);
                        usage->children[r].children[c].type = scalar.type;
                        usage->children[r].children[c].debugVarSSAName = usage->debugVarSSAName;
                        usage->children[r].children[c].debugVarComponent = r;
                        usage->children[r].children[c].rows = 1U;
                        usage->children[r].children[c].columns = 1U;
                        usage->children[r].children[c].offset = usage->children[r].offset + c;
                      }
                    }
                  }
                  RDCASSERTEQUAL(usage->children.size(), rows);

                  // assigning to a vector (row or column)
                  uint32_t vecSize = (typeWalk->colMajorMat) ? rows : columns;
                  // assigning to a single element
                  if(bytesRemaining == scalar.sizeInBytes)
                  {
                    bytesRemaining -= scalar.sizeInBytes;
                    uint32_t componentIndex = byteOffset / scalar.sizeInBytes;
                    uint32_t row, col;

                    if(typeWalk->colMajorMat)
                    {
                      row = componentIndex % rows;
                      col = componentIndex / rows;
                    }
                    else
                    {
                      row = componentIndex / columns;
                      col = componentIndex % columns;
                    }
                    RDCASSERT(row < rows, row, rows);
                    RDCASSERT(col < columns, col, columns);

                    RDCASSERTEQUAL(usage->children[row].children.size(), columns);
                    usage->children[row].children[col].emitSourceVar =
                        !usage->children[row].emitSourceVar;
                    usage->children[row].children[col].debugVarSSAName = mapping.debugVarSSAName;
                    usage->children[row].children[col].debugVarComponent = 0;

                    // try to recombine matrix rows to a single source var display
                    if(!usage->children[row].emitSourceVar)
                    {
                      bool collapseVector = true;
                      for(uint32_t c = 0; c < columns; ++c)
                      {
                        collapseVector = usage->children[row].children[c].emitSourceVar;
                        if(!collapseVector)
                          break;
                      }
                      if(collapseVector)
                      {
                        usage->children[row].emitSourceVar = true;
                        for(uint32_t c = 0; c < columns; ++c)
                          usage->children[row].children[c].emitSourceVar = false;
                      }
                    }
                  }
                  // Assigning to a row/col
                  else if(bytesRemaining == scalar.sizeInBytes * vecSize)
                  {
                    uint32_t componentIndex = byteOffset / scalar.sizeInBytes;
                    if(typeWalk->colMajorMat)
                    {
                      uint32_t col = componentIndex / rows;
                      RDCASSERT(col < columns, col, columns);
                      // one remaining index selects a column within the matrix.
                      // source vars are displayed as row-major, need <rows> mappings
                      for(uint32_t r = 0; r < rows; ++r)
                      {
                        RDCASSERTEQUAL(usage->children[r].children.size(), columns);
                        usage->children[r].children[col].emitSourceVar =
                            !usage->children[r].emitSourceVar;
                        usage->children[r].children[col].debugVarSSAName = mapping.debugVarSSAName;
                        usage->children[r].children[col].debugVarComponent = r;
                      }
                    }
                    else
                    {
                      uint32_t row = componentIndex / columns;
                      RDCASSERT(row < rows, row, rows);
                      RDCASSERTEQUAL(usage->children.size(), rows);
                      RDCASSERTEQUAL(usage->children[row].children.size(), columns);
                      // one remaining index selects a row within the matrix.
                      // source vars are displayed as row-major, need <rows> mappings
                      for(uint32_t c = 0; c < columns; ++c)
                      {
                        usage->children[row].children[c].emitSourceVar =
                            !usage->children[row].emitSourceVar;
                        usage->children[row].children[c].debugVarSSAName = mapping.debugVarSSAName;
                        usage->children[row].children[c].debugVarComponent = c;
                      }
                    }
                  }
                  else
                  {
                    RDCERR("Unhandled matrix assignment");
                  }
                  // try to recombine matrix rows to a single source var display
                  for(uint32_t r = 0; r < rows; ++r)
                  {
                    if(!usage->children[r].emitSourceVar)
                    {
                      bool collapseVector = true;
                      RDCASSERTEQUAL(usage->children[r].children.size(), columns);
                      for(uint32_t c = 0; c < columns; ++c)
                      {
                        collapseVector = usage->children[r].children[c].emitSourceVar;
                        if(!collapseVector)
                          break;
                      }
                      if(collapseVector)
                      {
                        usage->children[r].emitSourceVar = true;
                        for(uint32_t c = 0; c < columns; ++c)
                          usage->children[r].children[c].emitSourceVar = false;
                      }
                    }
                  }
                  usage->emitSourceVar = false;
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
              else if(typeWalk->vecSize > 1)
              {
                // Index into the vector using byte offset and component size
                const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];
                uint32_t componentIndex = byteOffset / scalar.sizeInBytes;
                columns = RDCMAX(1U, typeWalk->vecSize);

                usage->type = scalar.type;

                usage->rows = 1U;
                usage->columns = columns;

                if(bytesRemaining == scalar.sizeInBytes)
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
                // walk down until we get to a scalar type, if we get there. This means arrays of
                // basic types will get the right type
                while(typeWalk && typeWalk->baseType != NULL && typeWalk->type == VarType::Unknown)
                  typeWalk = &m_DebugInfo.types[typeWalk->baseType];

                if(typeWalk == NULL || typeWalk->baseType == NULL)
                {
                  RDCERR("Unexpected type source variable %s SSAID %s",
                         mapping.sourceVarName.c_str(), mapping.debugVarSSAName.c_str());
                  continue;
                }
                const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];
                uint32_t elemCount = 1;

                if(scalar.vecSize > 0)
                  elemCount = scalar.vecSize;
                else if(!scalar.structMembers.empty())
                  elemCount = (uint32_t)scalar.structMembers.size();
                RDCASSERT(elemCount > 0);
                elemCount = RDCMAX(elemCount, 1U);

                uint32_t elementSize = scalar.sizeInBytes / elemCount;
                uint32_t mappingCount = bytesRemaining / elementSize;

                usage->type = typeWalk->type;
                usage->debugVarSSAName = mapping.debugVarSSAName;
                usage->debugVarComponent = byteOffset / elementSize;
                usage->rows = 1U;
                usage->columns = 1U;
                usage->emitSourceVar = true;
                usage->children.clear();
                usage->debugVarSuffix.clear();

                bytesRemaining -= mappingCount * elementSize;
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

// Must be called from the replay manager thread (the debugger thread)
ShaderDebugTrace *Debugger::BeginDebug(DebugAPIWrapper *apiWrapper, uint32_t eventId,
                                       const DXBC::DXBCContainer *dxbcContainer,
                                       const ShaderReflection &reflection, uint32_t activeLaneIndex,
                                       uint32_t threadsInWorkgroup)
{
  CHECK_DEBUGGER_THREAD();
  if(activeLaneIndex >= threadsInWorkgroup)
  {
    RDCERR("Invalid active lane index");
    return NULL;
  }

  ShaderStage shaderStage = reflection.stage;

  m_ApiWrapper = apiWrapper;
  m_Program = dxbcContainer->GetDXILByteCode();
  m_ActiveLaneIndex = activeLaneIndex;
  m_Steps = 0;
  m_Stage = shaderStage;
  m_EntryPointInterface = m_Program->GetEntryPointInterface();
  m_QueuedDeviceThreadSteps.resize(threadsInWorkgroup);
  m_QueuedGpuMathOps.resize(threadsInWorkgroup);
  m_QueuedGpuSampleGatherOps.resize(threadsInWorkgroup);
  m_PendingLanes.resize(threadsInWorkgroup);
  m_QueuedJobs.resize(threadsInWorkgroup);

  uint32_t outputSSAId = m_Program->m_NextSSAId;
  uint32_t maxSSAId = outputSSAId + 1;

  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->stage = shaderStage;

  // Get the global state from the API wrapper
  m_GlobalState.builtins = apiWrapper->GetBuiltins();
  m_GlobalState.subgroupSize = apiWrapper->GetSubgroupSize();
  m_GlobalState.constantBlocks = apiWrapper->GetConstantBlocks();
  m_GlobalState.constantBlocksDatas = apiWrapper->GetConstantBlocksDatas();

  for(uint32_t i = 0; i < threadsInWorkgroup; i++)
  {
    m_Workgroup.push_back(ThreadState(*this, m_GlobalState, maxSSAId, i, threadsInWorkgroup));
    m_QueuedDeviceThreadSteps[i] = false;
    m_QueuedGpuMathOps[i] = false;
    m_QueuedGpuSampleGatherOps[i] = false;
    m_PendingLanes[i] = false;
    m_QueuedJobs[i] = 0;
  }

  // Get the thread state from the API wrapper
  const rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> &threadsBuiltins =
      apiWrapper->GetThreadsBuiltins();
  const rdcarray<ShaderVariable> &threadsInputs = apiWrapper->GetThreadsInputs();

  for(uint32_t i = 0; i < threadsInWorkgroup; i++)
  {
    m_Workgroup[i].SetBuiltins(threadsBuiltins[i]);
    m_Workgroup[i].SetInput(threadsInputs[i]);
  }

  ret->sourceVars = apiWrapper->GetSourceVars();

  ThreadState &activeState = GetActiveLane();

  struct ResourceList
  {
    VarType varType;
    DebugVariableType debugVarType;
    DescriptorCategory category;
    ResourceClass resourceClass;
    const rdcarray<ShaderResource> &resources;
    rdcarray<ShaderVariable> &dst;
  };

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
  size_t count = reflection.samplers.size();
  m_GlobalState.samplers.reserve(count);
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

  m_LiveGlobals.resize(maxSSAId);
  MemoryTracking &globalMemory = m_GlobalState.memory;
  for(const DXIL::GlobalVar *gv : m_Program->m_GlobalVars)
  {
    GlobalVariable globalVar;
    rdcstr n;
    m_Program->GetSSAName(gv->ssaId, n);
    globalVar.var.name = n;
    globalVar.id = gv->ssaId;
    globalVar.gsm = (gv->type->addrSpace == DXIL::Type::PointerAddrSpace::GroupShared);
    globalMemory.AllocateMemoryForType(gv->type, globalVar.id, true, globalVar.gsm, globalVar.var);
    if(gv->initialiser)
    {
      const Constant *initialData = gv->initialiser;
      if(!initialData->isNULL() && !initialData->isUndef())
      {
        RDCASSERT(ConvertDXILConstantToShaderVariable(initialData, globalVar.var));
        // Write ShaderVariable data back to memory
        auto itAlloc = globalMemory.m_Allocations.find(globalVar.id);
        if(itAlloc != globalMemory.m_Allocations.end())
        {
          const MemoryTracking::Allocation &allocation = itAlloc->second;
          void *allocMemoryBackingPtr = allocation.backingMemory;
          uint64_t allocSize = allocation.size;
          activeState.UpdateBackingMemoryFromVariable(allocMemoryBackingPtr, allocSize,
                                                      globalVar.var);
          RDCASSERTEQUAL(allocSize, 0);
        }
        else
        {
          RDCERR("Unknown memory allocation for GlobalVariable Id %u", globalVar.id);
        }
      }
    }
    if(globalVar.gsm)
      m_GlobalState.groupSharedMemoryIds.push_back(globalVar.id);
    m_GlobalState.globals.push_back(globalVar);
    m_LiveGlobals[globalVar.id] = true;
  }
  // Find all the constants and create them as shader variables and store them in the global state
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      for(Instruction *inst : f->instructions)
      {
        if(IsNopInstruction(*inst))
          continue;
        for(const Value *arg : inst->args)
        {
          if(arg && arg->kind() == ValueKind::Constant)
          {
            Constant *c = (Constant *)arg;
            // Ignore if already created
            if(m_LiveGlobals[c->ssaId])
            {
              continue;
            }

            GlobalConstant constantVar;
            ShaderVariable &var = constantVar.var;
            ConvertDXILTypeToShaderVariable(c->type, var);
            ConvertDXILConstantToShaderVariable(c, var);
            var.name;
            Id id = c->ssaId;
            m_Program->GetSSAName(id, var.name);
            RDCASSERTNOTEQUAL(id, DXILDebug::INVALID_ID);
            constantVar.id = id;
            if(var.type == VarType::GPUPointer)
            {
              Id ptrId;
              uint64_t offset;
              uint64_t size;
              VarType baseType;
              // Decode the pointer allocation: ptrId, offset, size
              RDCASSERT(DecodePointer(ptrId, offset, size, baseType, var));

              auto it = globalMemory.m_Allocations.find(ptrId);
              if(it != globalMemory.m_Allocations.end())
              {
                const MemoryTracking::Allocation &allocation = it->second;
                uint8_t *memory = (uint8_t *)allocation.backingMemory;
                RDCASSERT(offset + size <= allocation.size);
                if(offset + size <= allocation.size)
                {
                  memory += offset;
                  globalMemory.m_Pointers[id] = {ptrId, memory, size};
                }
                else
                {
                  RDCERR("Invalid GEP offset %u size %u for allocation size %u", offset, size,
                         allocation.size);
                }
              }
              else
              {
                RDCERR("Failed to find allocation for Constant global variable pointer %u", ptrId);
              }
            }
            m_GlobalState.constants.push_back(constantVar);
            m_LiveGlobals[id] = true;
          }
        }
      }
    }
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
  // maximum execution point per SSA reference
  // uniform control blocks
  // block index from a function local instruction index
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      FunctionInfo &info = m_FunctionInfos[f];
      info.function = f;
      info.globalInstructionOffset = globalOffset;
      uint32_t countInstructions = (uint32_t)f->instructions.size();
      globalOffset += countInstructions;

      // Find the uniform control blocks in the function
      rdcarray<rdcpair<uint32_t, uint32_t>> links;
      for(const Block *block : f->blocks)
      {
        for(const Block *pred : block->preds)
        {
          uint32_t from = pred->id;
          uint32_t to = block->id;
          links.push_back({from, to});
        }
      }

      DXIL::ControlFlow &controlFlow = info.controlFlow;

      controlFlow.Construct(links);
      info.uniformBlocks = controlFlow.GetUniformBlocks();
      info.divergentBlocks = controlFlow.GetDivergentBlocks();
      info.convergentBlocks = controlFlow.GetConvergentBlocks();
      info.partialConvergentBlocks = controlFlow.GetPartialConvergentBlocks();
      const rdcarray<uint32_t> loopBlocks = controlFlow.GetLoopBlocks();

      // Handle de-generate case when a single block
      if(info.uniformBlocks.empty())
      {
        RDCASSERTEQUAL(f->blocks.size(), 1);
        info.uniformBlocks.push_back(f->blocks[0]->id);
      }

      FunctionInfo::ReferencedIds &ssaRefs = info.referencedIds;
      FunctionInfo::ExecutionPointPerId &ssaMaxExecPoints = info.maxExecPointPerId;
      ssaMaxExecPoints.resize(maxSSAId);
      std::set<Id> ssaMaxRefs;

      uint32_t curBlock = 0;
      info.instructionToBlock.resize(countInstructions);
      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        const ExecPointReference current(curBlock, i);
        info.instructionToBlock[i] = current.block;
        const Instruction &inst = *(f->instructions[i]);
        if(IsLLVMDebugCall(inst) || DXIL::IsLLVMIntrinsicCall(inst))
          continue;

        // Stack allocations last until the end of the function
        // Allow the variable to live for one instruction longer
        const uint32_t maxInst = (inst.op == Operation::Alloca) ? countInstructions : i;
        Id resultId = inst.slot;
        if(resultId != DXILDebug::INVALID_ID)
        {
          // The result SSA should not have been referenced before
          RDCASSERTEQUAL(ssaRefs.count(resultId), 0);
          ssaRefs.insert(resultId);

          auto it = ssaMaxRefs.find(resultId);
          if(it == ssaMaxRefs.end())
          {
            ssaMaxExecPoints[resultId] = current;
            ssaMaxRefs.insert(resultId);
          }
          else
          {
            // If the result SSA has tracking then that access should be after this assignment
            RDCASSERT(ssaMaxExecPoints[resultId].IsAfter(current, controlFlow));
          }
        }
        // Track maximum execution point when an SSA is referenced as an argument
        // Arguments to phi instructions are handled separately
        if(inst.op == Operation::Phi)
          continue;

        ExecPointReference maxPoint(curBlock, maxInst);
        for(uint32_t a = 0; a < inst.args.size(); ++a)
        {
          DXIL::Value *arg = inst.args[a];
          if(!DXIL::IsSSA(arg))
            continue;
          Id argId = GetSSAId(arg);
          // Add GlobalVar and Constant args to the SSA refs (they won't be the result of an instruction)
          if(cast<GlobalVar>(arg) || cast<Constant>(arg))
          {
            if(ssaRefs.count(argId) == 0)
              ssaRefs.insert(argId);
          }
          auto it = ssaMaxRefs.find(argId);
          if(it == ssaMaxRefs.end())
          {
            ssaMaxExecPoints[argId] = maxPoint;
            ssaMaxRefs.insert(argId);
          }
          else
          {
            // Update the maximum execution point if access is later than the existing access
            if(maxPoint.IsAfter(ssaMaxExecPoints[argId], controlFlow))
              ssaMaxExecPoints[argId] = maxPoint;
          }
        }
        if(inst.op == Operation::Branch || inst.op == Operation::Unreachable ||
           inst.op == Operation::Switch || inst.op == Operation::Ret)
          ++curBlock;
      }
      // If these do not match in size that means there is a result SSA that is never read
      RDCASSERTEQUAL(ssaRefs.size(), ssaMaxRefs.size());

      // Update any SSA max points which are inside a loop to the next uniform block
      // This covers the case of SSA IDs that are assigned to but never accessed
      for(Id id : ssaMaxRefs)
      {
        ExecPointReference &maxPoint = ssaMaxExecPoints[id];
        uint32_t block = maxPoint.block;
        // If the current block is in a loop, set the execution point to the next uniform block
        if(loopBlocks.contains(block))
        {
          uint32_t nextUniformBlock = controlFlow.GetNextUniformBlock(block);
          maxPoint.block = nextUniformBlock;
          maxPoint.instruction = f->blocks[nextUniformBlock]->startInstructionIdx + 1;
        }
      }

      // store the block captured SSA IDs used as arguments to phi nodes
      FunctionInfo::PhiReferencedIdsPerBlock &phiReferencedIdsPerBlock =
          info.phiReferencedIdsPerBlock;
      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        const Instruction &inst = *(f->instructions[i]);
        if(inst.op != Operation::Phi)
          continue;
        for(uint32_t a = 0; a < inst.args.size(); a += 2)
        {
          DXIL::Value *arg = inst.args[a];
          if(!DXIL::IsSSA(arg))
            continue;
          Id argId = GetSSAId(arg);
          const Block *block = cast<Block>(inst.args[a + 1]);
          RDCASSERT(block);
          uint32_t blockId = block->id;
          phiReferencedIdsPerBlock[blockId].insert(argId);
        }
      }
    }
  }

  // Generate scopes
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      FunctionInfo &info = m_FunctionInfos[f];
      uint32_t countInstructions = (uint32_t)f->instructions.size();

      ScopedDebugData *currentScope = NULL;
      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        uint32_t instructionIndex = i + info.globalInstructionOffset;
        const Instruction &inst = *f->instructions[i];
        ScopedDebugData *thisScope = NULL;
        // Use DebugLoc data for building up the list of scopes
        uint32_t dbgLoc = ShouldIgnoreSourceMapping(inst) ? ~0U : inst.debugLoc;
        if(dbgLoc != ~0U)
        {
          const DebugLocation &debugLoc = m_Program->m_DebugLocations[dbgLoc];
          thisScope = AddScopedDebugData(debugLoc.scope);
        }
        if(!thisScope)
          continue;

        if(currentScope)
          currentScope->maxInstruction = instructionIndex - 1;

        currentScope = thisScope;
        thisScope->maxInstruction = instructionIndex;
      }
    }
  }

  // Sort the scopes by instruction index
  std::sort(m_DebugInfo.scopedDebugDatas.begin(), m_DebugInfo.scopedDebugDatas.end(),
            [](const ScopedDebugData *a, const ScopedDebugData *b) { return *a < *b; });

  // Generate callstacks
  for(const Function *f : m_Program->m_Functions)
  {
    if(!f->external)
    {
      FunctionInfo &info = m_FunctionInfos[f];
      uint32_t countInstructions = (uint32_t)f->instructions.size();

      rdcarray<ScopedDebugData *> scopeHierarchy;
      for(uint32_t i = 0; i < countInstructions; ++i)
      {
        uint32_t instructionIndex = i + info.globalInstructionOffset;
        const Instruction &inst = *f->instructions[i];
        // Use DebugLoc data for building up the list of scopes
        uint32_t dbgLoc = ShouldIgnoreSourceMapping(inst) ? ~0U : inst.debugLoc;
        if(dbgLoc == ~0U)
          continue;

        FunctionInfo::Callstack callstack;

        const DebugLocation *debugLoc = &m_Program->m_DebugLocations[dbgLoc];
        // For each DILocation
        while(debugLoc)
        {
          // Walk scope upwards to make callstack : always a DebugScope
          const DXIL::Metadata *scopeMD = debugLoc->scope;
          while(scopeMD)
          {
            DXIL::DIBase *dwarf = scopeMD->dwarf;
            if(dwarf)
            {
              // Walk upwards through all the functions
              if(dwarf->type == DIBase::Subprogram)
              {
                rdcstr funcName = m_Program->GetFunctionScopeName(dwarf);
                if(!funcName.empty())
                  callstack.insert(0, funcName);
                scopeMD = dwarf->As<DISubprogram>()->scope;
              }
              else if(dwarf->type == DIBase::LexicalBlock)
              {
                scopeMD = dwarf->As<DILexicalBlock>()->scope;
              }
              else if(dwarf->type == DIBase::File)
              {
                scopeMD = NULL;
                break;
              }
              else if(dwarf->type == DIBase::CompositeType)
              {
                const DICompositeType *compType = dwarf->As<DICompositeType>();
                // Detect a member function
                if((compType->tag == DW_TAG_class_type) || (compType->tag == DW_TAG_structure_type))
                {
                  const rdcstr *typeName = compType->name;
                  if(typeName && !typeName->empty())
                  {
                    if(!callstack.empty())
                      callstack[0] = *typeName + "::" + callstack[0];
                  }
                }
                scopeMD = compType->scope;
              }
              else if(dwarf->type == DIBase::Namespace)
              {
                const DINamespace *nameType = dwarf->As<DINamespace>();
                const rdcstr *typeName = nameType->name;
                if(typeName && !typeName->empty())
                {
                  if(!callstack.empty())
                    callstack[0] = *typeName + "::" + callstack[0];
                }
                scopeMD = nameType->scope;
              }
              else
              {
                RDCERR("Unhandled scope type %s", ToStr(dwarf->type).c_str());
                scopeMD = NULL;
                break;
              }
            }
          }
          // Make new DILocation from inlinedAt and walk that DILocation
          if(debugLoc->inlinedAt)
            debugLoc = debugLoc->inlinedAt->debugLoc;
          else
            debugLoc = NULL;
        }
        info.callstacks[instructionIndex] = callstack;
      }
    }
  }

  ParseDebugData();

  // Extend the life time of any SSA ID which is mapped to a source variable
  // to the end of the scope the source variable is used in
  for(auto funcInfosIt = m_FunctionInfos.begin(); funcInfosIt != m_FunctionInfos.end(); ++funcInfosIt)
  {
    FunctionInfo &info = funcInfosIt->second;
    const DXIL::ControlFlow &controlFlow = info.controlFlow;
    const rdcarray<uint32_t> loopBlocks = controlFlow.GetLoopBlocks();
    for(ScopedDebugData *scope : m_DebugInfo.scopedDebugDatas)
    {
      for(LocalMapping &localMapping : scope->localMappings)
      {
        if(localMapping.debugVarSSAId < info.maxExecPointPerId.size())
        {
          const ExecPointReference &current = info.maxExecPointPerId[localMapping.debugVarSSAId];
          if(!current.IsValid())
            continue;
          uint32_t scopeEndInst = scope->maxInstruction;
          scopeEndInst = RDCMIN(scopeEndInst, (uint32_t)info.instructionToBlock.size() - 1);
          const uint32_t scopeEndBlock = info.instructionToBlock[scopeEndInst];
          ExecPointReference scopeEnd(scopeEndBlock, scopeEndInst);
          if(loopBlocks.contains(scopeEnd.block))
          {
            uint32_t nextUniformBlock = controlFlow.GetNextUniformBlock(scopeEnd.block);
            scopeEnd.block = nextUniformBlock;
            scopeEnd.instruction = info.function->blocks[nextUniformBlock]->startInstructionIdx + 1;
          }
          if(scopeEnd.IsAfter(current, controlFlow))
            info.maxExecPointPerId[localMapping.debugVarSSAId] = scopeEnd;
        }
      }
    }
  }

  const rdcarray<SigParameter> &dxbcOutParams = dxbcContainer->GetReflection()->OutputSig;
  const rdcarray<EntryPointInterface::Signature> &outputs = m_EntryPointInterface->outputs;
  uint32_t countOutputs = (uint32_t)outputs.size();

  // Make fake ShaderVariable struct to hold all the outputs
  ShaderVariable outStruct;
  outStruct.name = DXIL_FAKE_OUTPUT_STRUCT_NAME;
  outStruct.rows = 0;
  outStruct.columns = 0;
  outStruct.type = VarType::Struct;
  outStruct.members.resize(countOutputs);

  for(uint32_t i = 0; i < countOutputs; ++i)
  {
    const EntryPointInterface::Signature &sig = outputs[i];

    ShaderVariable &v = outStruct.members[i];

    // Get the name from the DXBC reflection
    SigParameter sigParam;
    if(FindSigParameter(dxbcOutParams, sig, sigParam))
      v.name = sigParam.semanticIdxName;
    else
      v.name = sig.name;

    v.type = VarTypeForComponentType(sig.type);
    v.columns = (uint8_t)sig.cols;
    v.rows = (uint8_t)sig.rows;
    if(v.rows <= 1)
    {
      v.rows = 1;
    }
    else
    {
      v.members.resize(v.rows);
      for(uint32_t r = 0; r < v.rows; r++)
      {
        v.members[r].rows = 1;
        v.members[r].columns = (uint8_t)sig.cols;
        v.members[r].type = v.type;
        v.members[r].name = StringFormat::Fmt("[%u]", r);
      }
      v.rows = 0;
      v.columns = 0;
      v.type = VarType::Struct;
    }
    // TODO: handle the output of system values
    // ShaderBuiltin::DepthOutput, ShaderBuiltin::DepthOutputLessEqual,
    // ShaderBuiltin::DepthOutputGreaterEqual, ShaderBuiltin::MSAACoverage,
    // ShaderBuiltin::StencilReference

    // Map the high level variables to the Output DXIL Signature
    if(v.rows == 1)
    {
      SourceVariableMapping outputMapping;
      outputMapping.name = v.name;
      outputMapping.type = v.type;
      outputMapping.rows = v.rows;
      outputMapping.columns = v.columns;
      outputMapping.signatureIndex = i;
      outputMapping.variables.reserve(sig.cols);
      for(uint32_t c = 0; c < sig.cols; ++c)
      {
        DebugVariableReference ref;
        ref.type = DebugVariableType::Variable;
        ref.name = outStruct.name + "." + v.name;
        ref.component = c;
        outputMapping.variables.push_back(ref);
      }
      ret->sourceVars.push_back(outputMapping);
    }
    else
    {
      // Make a mapping per element
      for(const ShaderVariable &member : v.members)
      {
        SourceVariableMapping outputMapping;
        outputMapping.name = v.name + member.name;
        outputMapping.type = member.type;
        outputMapping.rows = 1;
        outputMapping.columns = member.columns;
        outputMapping.signatureIndex = -1;
        for(uint32_t c = 0; c < member.columns; ++c)
        {
          DebugVariableReference ref;
          ref.type = DebugVariableType::Variable;
          ref.name = outStruct.name + "." + v.name + member.name;
          ref.component = c;
          outputMapping.variables.push_back(ref);
        }
        ret->sourceVars.push_back(outputMapping);
      }
    }

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
    outputMapping.name = outStruct.name;
    outputMapping.type = VarType::Struct;
    outputMapping.rows = 0;
    outputMapping.columns = 0;
    outputMapping.variables.resize(1);
    outputMapping.variables[0].name = outStruct.name;
    outputMapping.variables[0].type = DebugVariableType::Variable;
    ret->sourceVars.push_back(outputMapping);
  }
  activeState.SetOutput(outputSSAId, outStruct);

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

  ret->inputs = {activeState.GetInput()};

  if(shaderStage == ShaderStage::Compute)
  {
    // For Compute shaders add fake inputs for semantics
    const BuiltinInputs &globalBuiltins = m_GlobalState.builtins;
    const BuiltinInputs &activeBuiltins = activeState.GetBuiltins();
    rdcarray<ShaderBuiltin> builtinsToAdd;
    // Parse the instructions to find which builtins are read
    // ShaderBuiltin::DispatchThreadIndex,
    // ShaderBuiltin::GroupIndex,
    // ShaderBuiltin::GroupFlatIndex,
    // ShaderBuiltin::GroupThreadIndex,
    for(const Function *f : m_Program->m_Functions)
    {
      if(f->external)
        continue;
      for(const Instruction *inst : f->instructions)
      {
        switch(inst->op)
        {
          case Operation::Call:
          {
            const Function *callFunc = inst->getFuncCall();
            if(callFunc->family == FunctionFamily::DXOp)
            {
              DXOp dxOpCode;
              RDCASSERT(getival<DXOp>(inst->args[0], dxOpCode));
              RDCASSERT(dxOpCode < DXOp::NumOpCodes, dxOpCode, DXOp::NumOpCodes);
              switch(dxOpCode)
              {
                case DXOp::ThreadId:
                  if(!builtinsToAdd.contains(ShaderBuiltin::DispatchThreadIndex))
                    builtinsToAdd.push_back(ShaderBuiltin::DispatchThreadIndex);
                  break;
                case DXOp::GroupId:
                  if(!builtinsToAdd.contains(ShaderBuiltin::GroupIndex))
                    builtinsToAdd.push_back(ShaderBuiltin::GroupIndex);
                  break;
                case DXOp::FlattenedThreadIdInGroup:
                  if(!builtinsToAdd.contains(ShaderBuiltin::GroupFlatIndex))
                    builtinsToAdd.push_back(ShaderBuiltin::GroupFlatIndex);
                  break;
                case DXOp::ThreadIdInGroup:
                  if(!builtinsToAdd.contains(ShaderBuiltin::GroupThreadIndex))
                    builtinsToAdd.push_back(ShaderBuiltin::GroupThreadIndex);
                  break;
                default: break;
              }
            }
          }
          default: break;
        }
      }
    }

    for(const ShaderBuiltin &builtin : builtinsToAdd)
    {
      ShaderVariable value;
      bool found = false;
      auto itThread = activeBuiltins.find(builtin);
      if(itThread != activeBuiltins.end())
      {
        value = itThread->second;
        found = true;
      }
      else
      {
        auto itGlobal = globalBuiltins.find(builtin);
        if(itGlobal != activeBuiltins.end())
        {
          value = itGlobal->second;
          found = true;
        }
      }
      if(found)
      {
        value.rows = 1;
        value.columns = 3;
        value.type = VarType::UInt;
        switch(builtin)
        {
          case ShaderBuiltin::DispatchThreadIndex: value.name = "SV_DispatchThreadID"; break;
          case ShaderBuiltin::GroupIndex: value.name = "SV_GroupID"; break;
          case ShaderBuiltin::GroupFlatIndex:
            value.name = "SV_GroupIndex";
            value.columns = 1;
            break;
          case ShaderBuiltin::GroupThreadIndex: value.name = "SV_GroupThreadID"; break;
          default: RDCERR("Unhandled builtin %s", ToStr(builtin).c_str()); break;
        }
        ret->inputs.push_back(value);
      }
    }
  }

  ret->constantBlocks = m_GlobalState.constantBlocks;
  ret->readOnlyResources = m_GlobalState.readOnlyResources;
  ret->readWriteResources = m_GlobalState.readWriteResources;
  ret->samplers = m_GlobalState.samplers;
  ret->debugger = this;

  for(uint32_t i = 0; i < threadsInWorkgroup; i++)
  {
    ThreadState &lane = m_Workgroup[i];
    if(i != m_ActiveLaneIndex)
      lane.InitialiseFromActive(activeState);
  }

  // Add the output struct to the global state
  if(countOutputs)
    m_GlobalState.globals.push_back(activeState.GetOutput());

  InitialiseWorkgroup();

  m_MTSimulation = D3D12_DXILShaderDebugger_EnableMT();
  if(threadsInWorkgroup < 4)
    m_MTSimulation = false;

  AtomicStore(&atomic_simulationFinished, 0);
  if(m_MTSimulation)
  {
    if(!D3D12_Hack_ShaderDebugUsesJobSystemJobs())
    {
      uint32_t countJobs = RDCMIN(threadsInWorkgroup, Threading::JobSystem::GetCountWorkers() / 2U);
      for(uint32_t i = 0; i < countJobs; ++i)
        Threading::JobSystem::AddJob([this]() { SimulationJobHelper(); });
    }
  }
  return ret;
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::InitialiseWorkgroup()
{
  CHECK_DEBUGGER_THREAD();
  const rdcarray<ThreadProperties> &workgroupProperties = m_ApiWrapper->GetWorkgroupProperties();
  const uint32_t threadsInWorkgroup = (uint32_t)m_Workgroup.size();

  if(threadsInWorkgroup == 1)
  {
    rdcarray<ThreadIndex> threadIds;
    threadIds.push_back(0);
    m_ControlFlow.Construct(threadIds);
    return;
  }

  if(threadsInWorkgroup != workgroupProperties.size())
  {
    RDCERR("Workgroup properties has wrong count %zu, expected %u", workgroupProperties.size(),
           threadsInWorkgroup);
    return;
  }

  rdcarray<ThreadIndex> threadIds;
  for(uint32_t i = 0; i < threadsInWorkgroup; i++)
  {
    ThreadState &lane = m_Workgroup[i];

    if(m_Stage == ShaderStage::Pixel)
    {
      lane.SetHelper(workgroupProperties[i][ThreadProperty::Helper] != 0);
      lane.SetQuadLaneIndex(workgroupProperties[i][ThreadProperty::QuadLane]);
      lane.SetQuadId(workgroupProperties[i][ThreadProperty::QuadId]);
    }

    lane.SetDead(workgroupProperties[i][ThreadProperty::Active] == 0);
    lane.SetSubgroupIdx(workgroupProperties[i][ThreadProperty::SubgroupIdx]);

    // Only add active lanes to control flow
    if(!lane.IsDead())
      threadIds.push_back(i);
  }

  m_ControlFlow.Construct(threadIds);

  // find quad neighbours
  {
    rdcarray<uint32_t> processedQuads;
    for(uint32_t i = 0; i < threadsInWorkgroup; i++)
    {
      uint32_t desiredQuad = m_Workgroup[i].GetQuadId();

      // ignore threads not in any quad
      if(desiredQuad == 0)
        continue;

      // quads are almost certainly sorted together, so shortcut by checking the last one
      if((!processedQuads.empty() && processedQuads.back() == desiredQuad) ||
         processedQuads.contains(desiredQuad))
        continue;

      processedQuads.push_back(desiredQuad);

      // find the threads
      uint32_t threads[4] = {
          i,
          ~0U,
          ~0U,
          ~0U,
      };
      for(uint32_t j = i + 1, t = 1; j < threadsInWorkgroup && t < 4; j++)
      {
        if(m_Workgroup[j].GetQuadId() == desiredQuad)
          threads[t++] = j;
      }

      // now swizzle the threads to know each other
      for(uint32_t src = 0; src < 4; src++)
      {
        uint32_t lane = m_Workgroup[threads[src]].GetQuadLaneIndex();

        if(lane >= 4)
          continue;

        for(uint32_t dst = 0; dst < 4; dst++)
        {
          if(threads[dst] == ~0U)
            continue;

          m_Workgroup[threads[dst]].SetQuadNeighbours(lane, threads[src]);
        }
      }
    }
  }
}

// Must be called from the replay manager thread (the debugger thread)
rdcarray<ShaderDebugState> Debugger::ContinueDebug()
{
  CHECK_DEBUGGER_THREAD();
  ThreadState &active = GetActiveLane();

  rdcarray<ShaderDebugState> ret;
  m_ShaderChangesReturn = NULL;

  // initialise the first ShaderDebugState if we haven't stepped yet
  if(m_Steps == 0)
  {
    ShaderDebugState initial;
    uint32_t startPoint = INVALID_EXECUTION_POINT;

    for(size_t lane = 0; lane < m_Workgroup.size(); lane++)
    {
      ThreadState &thread = m_Workgroup[lane];

      if(lane == m_ActiveLaneIndex)
      {
        thread.EnterEntryPoint(m_EntryPointFunction, true);
        thread.FillCallstack(initial);
        initial.nextInstruction = thread.GetActiveGlobalInstructionIdx();
        const ShaderDebugState &pendingDebugState = thread.GetPendingDebugState();
        initial.flags = pendingDebugState.flags;
        initial.changes.append(pendingDebugState.changes);
        startPoint = initial.nextInstruction;
      }
      else
      {
        thread.EnterEntryPoint(m_EntryPointFunction, false);
      }
    }

    // globals won't be filled out by entering the entry point, ensure their change is registered.
    for(const GlobalVariable &gv : m_GlobalState.globals)
      initial.changes.push_back({ShaderVariable(), gv.var});

    // constants won't be filled out by entering the entry point, ensure their change is registered.
    for(const GlobalConstant &c : m_GlobalState.constants)
      initial.changes.push_back({ShaderVariable(), c.var});

    ret.push_back(std::move(initial));

    // Set the initial execution point for the threads in the root tangle
    ThreadExecutionStates threadExecutionStates;
    TangleGroup &tangles = m_ControlFlow.GetTangles();
    RDCASSERTEQUAL(tangles.size(), 1);
    RDCASSERTNOTEQUAL(startPoint, INVALID_EXECUTION_POINT);
    for(Tangle &tangle : tangles)
    {
      RDCASSERT(tangle.IsAliveActive());
      for(uint32_t threadIdx = 0; threadIdx < m_Workgroup.size(); ++threadIdx)
      {
        if(!m_Workgroup[threadIdx].Finished())
          threadExecutionStates[threadIdx].push_back(startPoint);
      }
    }
    m_ControlFlow.UpdateState(threadExecutionStates);
    m_Steps++;
  }

  // if we've finished, return an empty set to signify that
  if(active.Finished())
  {
    AtomicStore(&atomic_simulationFinished, 1);
    Threading::JobSystem::SyncAllJobs();
    return ret;
  }

  bool allStepsCompleted = true;
  m_ShaderChangesReturn = &ret;

  // continue stepping until we have 1000000 target steps completed in a chunk.
  for(int stepEnd = m_Steps + 1000000; m_Steps < stepEnd;)
  {
    allStepsCompleted = true;
    if(active.Finished() && !active.IsSimulationStepActive())
      break;

    // Execute the threads in each active tangle
    ThreadExecutionStates threadExecutionStates;
    TangleGroup &tangles = m_ControlFlow.GetTangles();

    bool anyActiveThreads = false;
    bool hasDebugState = false;

    for(const Tangle &tangle : tangles)
    {
      if(!tangle.IsAliveActive())
        continue;

      rdcarray<bool> activeMask;
      // one bool per workgroup thread
      activeMask.resize(m_Workgroup.size());

      // calculate the current active thread mask from the threads in the tangle
      for(size_t i = 0; i < m_Workgroup.size(); i++)
        activeMask[i] = false;

      const rdcarray<ThreadReference> &threadRefs = tangle.GetThreadRefs();
      for(const ThreadReference &ref : threadRefs)
      {
        uint32_t lane = ref.id;
        RDCASSERT(lane < m_Workgroup.size(), lane, m_Workgroup.size());
        ThreadState &thread = m_Workgroup[lane];
        RDCASSERT(!thread.Finished());
        activeMask[lane] = true;
        anyActiveThreads = true;
      }

      // step all threads in the tangle
      for(const ThreadReference &ref : threadRefs)
      {
        const uint32_t threadId = ref.id;
        const uint32_t lane = threadId;

        ThreadState &thread = m_Workgroup[lane];
        if(thread.Finished())
        {
          if(lane == m_ActiveLaneIndex)
            ret.emplace_back();
          continue;
        }
        if(lane == m_ActiveLaneIndex)
          hasDebugState = true;

        thread.SetActiveMask(activeMask);
        QueueJob(lane);
      }
    }

    do
    {
      ProcessQueuedDeviceThreadSteps();
      // Convert the simulation threads queued operations into pending operations i.e. GPU commands
      ProcessQueuedOps();
      // Sync any pending GPU operations and set the results to the pending threads
      SyncPendingLanes();

      allStepsCompleted = true;
      for(const Tangle &tangle : tangles)
      {
        if(!tangle.IsAliveActive())
          continue;

        bool tangleStepsCompleted = true;
        const rdcarray<ThreadReference> &threadRefs = tangle.GetThreadRefs();
        for(const ThreadReference &ref : threadRefs)
        {
          const uint32_t threadId = ref.id;
          const uint32_t lane = threadId;
          ThreadState &thread = m_Workgroup[lane];
          if(thread.IsSimulationStepActive())
          {
            tangleStepsCompleted = false;
            break;
          }
        }
        if(!tangleStepsCompleted)
        {
          allStepsCompleted = false;
          break;
        }
      }
    } while(!allStepsCompleted);

    for(Tangle &tangle : tangles)
    {
      if(!tangle.IsAliveActive())
        continue;

      const rdcarray<ThreadReference> &threadRefs = tangle.GetThreadRefs();
#if ENABLED(RDOC_DEVEL)
      for(const ThreadReference &ref : threadRefs)
      {
        const uint32_t threadId = ref.id;
        const uint32_t lane = threadId;
        ThreadState &thread = m_Workgroup[lane];
        RDCASSERT(!thread.IsSimulationStepActive());
      }
#endif    // #if ENABLED(RDOC_DEVEL)

      const DXIL::BlockArray *newPartialConvergentPoints = NULL;
      ExecutionPoint newConvergencePoint = INVALID_EXECUTION_POINT;
      uint32_t countActiveThreads = 0;
      uint32_t countDivergedThreads = 0;
      uint32_t countConvergePointThreads = 0;
      uint32_t countPartialConvergePointThreads = 0;

      // Update the control flow state
      for(const ThreadReference &ref : threadRefs)
      {
        const uint32_t threadId = ref.id;
        const uint32_t lane = threadId;
        ThreadState &thread = m_Workgroup[lane];

        bool wasActive = !thread.Finished();

        threadExecutionStates[threadId] = thread.GetEnteredPoints();

        const uint32_t threadConvergencePoint = thread.GetConvergencePoint();
        // the thread activated a new convergence point
        if(threadConvergencePoint != INVALID_EXECUTION_POINT)
        {
          wasActive = true;
          if(newConvergencePoint == INVALID_EXECUTION_POINT)
          {
            newConvergencePoint = threadConvergencePoint;
            RDCASSERTNOTEQUAL(newConvergencePoint, INVALID_EXECUTION_POINT);
          }
          else
          {
            // All the threads in the tangle should set the same convergence point
            RDCASSERTEQUAL(threadConvergencePoint, newConvergencePoint);
          }
          ++countConvergePointThreads;
        }
        const DXIL::BlockArray *partialConvergentPoints = thread.GetPartialConvergencePoints();
        if(!partialConvergentPoints->empty())
        {
          wasActive = true;
          if(newPartialConvergentPoints == NULL)
          {
            newPartialConvergentPoints = partialConvergentPoints;
            RDCASSERT(newPartialConvergentPoints);
            if(newPartialConvergentPoints)
              RDCASSERT(!newPartialConvergentPoints->empty());
          }
          else
          {
            // All the threads in the tangle should set the same partial convergence points
            RDCASSERT(*newPartialConvergentPoints == *partialConvergentPoints);
          }
          ++countPartialConvergePointThreads;
        }

        if(thread.GetDiverged())
        {
          wasActive = true;
          ++countDivergedThreads;
        }

        if(thread.Finished())
          tangle.SetThreadDead(threadId);

        countActiveThreads += wasActive ? 1 : 0;
      }

      for(const ThreadReference &ref : threadRefs)
      {
        const uint32_t threadId = ref.id;
        const uint32_t lane = threadId;
        m_Workgroup[lane].UpdateCurrentInstruction();
      }

      if(countConvergePointThreads)
      {
        // all the active threads should have a convergence point if any have one
        RDCASSERTEQUAL(countConvergePointThreads, countActiveThreads);
        tangle.AddMergePoint(newConvergencePoint);
      }
      if(countPartialConvergePointThreads)
      {
        // all the active threads should have partial convergence points if any have them
        RDCASSERTEQUAL(countPartialConvergePointThreads, countActiveThreads);
        RDCASSERT(newPartialConvergentPoints);
        const size_t countPoints =
            newPartialConvergentPoints ? newPartialConvergentPoints->size() : 0;
        for(size_t i = 0; i < countPoints; i++)
        {
          // The partial convergence points are generated sorted from least to most inter-connections
          const uint32_t point = newPartialConvergentPoints->at(i);
          tangle.AddMergePoint(point);
        }
        // partial convergence requires full convergence
        RDCASSERTEQUAL(countConvergePointThreads, countActiveThreads);
      }
      if(countDivergedThreads)
      {
        // all the active threads should have diverged if any diverges
        RDCASSERTEQUAL(countDivergedThreads, countActiveThreads);
        tangle.SetDiverged(true);
      }
    }
    if(!anyActiveThreads)
    {
      active.SetDead(true);
      m_ControlFlow.UpdateState(threadExecutionStates);
      RDCERR("No active threads in any tangle, killing active thread to terminate the debugger");
    }
    m_ControlFlow.UpdateState(threadExecutionStates);
  }

  RDCASSERT(allStepsCompleted);
  m_ShaderChangesReturn = NULL;
  return ret;
}

const FunctionInfo *Debugger::GetFunctionInfo(const DXIL::Function *function) const
{
  RDCASSERT(m_FunctionInfos.count(function) != 0);
  return &m_FunctionInfos.at(function);
}

// Called from any thread
// Resource must be cached
ShaderValue Debugger::TypedResourceLoad(DXIL::ResourceClass resClass, const BindingSlot &slot,
                                        const DXILDebug::ViewFmt &fmt, uint64_t dataOffset)
{
  if(resClass == DXIL::ResourceClass::UAV)
  {
    DXIL_DEBUG_RDCASSERT(m_ApiWrapper->IsUAVCached(slot));
    return m_ApiWrapper->TypedUAVLoad(slot, fmt, dataOffset);
  }
  else if(resClass == DXIL::ResourceClass::SRV)
  {
    DXIL_DEBUG_RDCASSERT(m_ApiWrapper->IsSRVCached(slot));
    return m_ApiWrapper->TypedSRVLoad(slot, fmt, dataOffset);
  }
  RDCERR("Unexpected resource class %s", ToStr(resClass).c_str());
  return ShaderValue();
}

// Called from any thread
// Resource must be cached
bool Debugger::TypedResourceStore(DXIL::ResourceClass resClass, const BindingSlot &slot,
                                  const DXILDebug::ViewFmt &fmt, uint64_t dataOffset,
                                  ShaderValue &value)
{
  if(resClass == DXIL::ResourceClass::UAV)
  {
    DXIL_DEBUG_RDCASSERT(m_ApiWrapper->IsUAVCached(slot));
    return m_ApiWrapper->TypedUAVStore(slot, fmt, dataOffset, value);
  }
  else if(resClass == DXIL::ResourceClass::SRV)
  {
    DXIL_DEBUG_RDCASSERT(m_ApiWrapper->IsSRVCached(slot));
    return m_ApiWrapper->TypedSRVStore(slot, fmt, dataOffset, value);
  }
  RDCERR("Unexpected resource class %s", ToStr(resClass).c_str());
  return false;
}

// Called from any thread
DeviceOpResult Debugger::GetUAV(const BindingSlot &slot, UAVInfo &uavInfo) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsUAVCached(slot))
    return DeviceOpResult::NeedsDevice;

  uavInfo = m_ApiWrapper->GetUAV(slot);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
DeviceOpResult Debugger::GetSRV(const BindingSlot &slot, SRVInfo &srvInfo) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsSRVCached(slot))
    return DeviceOpResult::NeedsDevice;

  srvInfo = m_ApiWrapper->GetSRV(slot);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
DeviceOpResult Debugger::GetResourceInfo(DXIL::ResourceClass resClass,
                                         const DXDebug::BindingSlot &slot, uint32_t mipLevel,
                                         ShaderVariable &result) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsResourceInfoCached(slot, mipLevel))
    return DeviceOpResult::NeedsDevice;

  result = m_ApiWrapper->GetResourceInfo(resClass, slot, mipLevel);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
DeviceOpResult Debugger::GetSampleInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                       const char *opString, ShaderVariable &result) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsSampleInfoCached(slot))
    return DeviceOpResult::NeedsDevice;

  result = m_ApiWrapper->GetSampleInfo(resClass, slot, opString);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
DeviceOpResult Debugger::GetRenderTargetSampleInfo(const char *opString, ShaderVariable &result) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsRenderTargetSampleInfoCached())
    return DeviceOpResult::NeedsDevice;

  result = m_ApiWrapper->GetRenderTargetSampleInfo(opString);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
DeviceOpResult Debugger::GetResourceReferenceInfo(const DXDebug::BindingSlot &slot,
                                                  ResourceReferenceInfo &result) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsResourceReferenceInfoCached(slot))
    return DeviceOpResult::NeedsDevice;

  result = m_ApiWrapper->GetResourceReferenceInfo(slot);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
DeviceOpResult Debugger::GetShaderDirectAccess(DescriptorType type, const DXDebug::BindingSlot &slot,
                                               ShaderDirectAccess &result) const
{
  if(!IsDeviceThread() && !m_ApiWrapper->IsShaderDirectAccessCached(slot))
    return DeviceOpResult::NeedsDevice;

  result = m_ApiWrapper->GetShaderDirectAccess(type, slot);
  return DeviceOpResult::Succeeded;
}

// Called from any thread
void Debugger::StepThread(uint32_t lane, StepThreadMode stepMode)
{
  ThreadState &thread = m_Workgroup[lane];
  bool isActiveThread = lane == m_ActiveLaneIndex;
  bool simulateStep = true;
  DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());
  int curActiveSteps = isActiveThread ? m_Steps : 0;

  while(simulateStep)
  {
    simulateStep = false;
    {
      thread.ClearPendingDebugState();
      if(isActiveThread)
        m_ActiveDebugState.stepIndex = curActiveSteps;
      InternalStepThread(lane);
      thread.ClearPendingDebugState();
    }
    if(thread.StepNeedsGpuSampleGatherOp())
      break;
    else if(thread.StepNeedsGpuMathOp())
      break;
    else if(thread.StepNeedsDeviceThread())
      break;

    if(isActiveThread)
      curActiveSteps++;

    if(stepMode == StepThreadMode::RUN_SINGLE_STEP)
      break;

    if(stepMode == StepThreadMode::QUEUE_SINGLE_STEP)
      break;

    simulateStep = thread.CanRunAnotherStep();
    if(simulateStep)
    {
      DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());
    }
    if(simulateStep)
      thread.SetStepQueued();

    if(stepMode == StepThreadMode::QUEUE_MULTIPLE_STEPS)
      break;
  };
  // Update the number of simulation steps
  if(isActiveThread)
    m_Steps = curActiveSteps;

  DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());

  // The queueing has to be when the thread is not being simulated
  if(thread.StepNeedsGpuSampleGatherOp())
  {
    DXIL_DEBUG_RDCASSERT(!simulateStep);
    QueueGpuSampleGatherOp(lane);
    return;
  }
  if(thread.StepNeedsGpuMathOp())
  {
    DXIL_DEBUG_RDCASSERT(!simulateStep);
    QueueGpuMathOp(lane);
    return;
  }
  if(thread.StepNeedsDeviceThread())
  {
    DXIL_DEBUG_RDCASSERT(!simulateStep);
    QueueDeviceThreadStep(lane);
    return;
  }

  if(simulateStep)
  {
    DXIL_DEBUG_RDCASSERTEQUAL(stepMode, StepThreadMode::QUEUE_MULTIPLE_STEPS);
    QueueJob(lane);
    return;
  }
  thread.SetSimulationStepCompleted();
}

// Called from any thread
void Debugger::InternalStepThread(uint32_t lane)
{
  ThreadState &thread = m_Workgroup[lane];
  if(lane == m_ActiveLaneIndex)
  {
    if(m_RetireIDs)
    {
      thread.RetireLiveIDs();
      m_RetireIDs = false;
      const ShaderDebugState &pendingDebugState = thread.GetPendingDebugState();
      m_ActiveDebugState.changes.append(pendingDebugState.changes);
      thread.ClearPendingDebugState();
    }
    thread.StepNext(true, m_Workgroup);
    if(thread.StepNeedsGpuSampleGatherOp())
      return;
    if(thread.StepNeedsGpuMathOp())
      return;
    if(thread.StepNeedsDeviceThread())
      return;

    m_ActiveDebugState.nextInstruction = thread.GetActiveGlobalInstructionIdx();
    thread.FillCallstack(m_ActiveDebugState);

    const ShaderDebugState &pendingDebugState = thread.GetPendingDebugState();
    m_ActiveDebugState.flags = pendingDebugState.flags;
    m_ActiveDebugState.changes.append(pendingDebugState.changes);
    thread.ClearPendingDebugState();

    m_ShaderChangesReturn->push_back(m_ActiveDebugState);
    {
      m_ActiveDebugState.callstack.clear();
      m_ActiveDebugState.changes.clear();
      m_ActiveDebugState.flags = ShaderEvents::NoEvent;
      m_ActiveDebugState.stepIndex = 0;
      m_ActiveDebugState.nextInstruction = 0;
      m_RetireIDs = true;
    }
  }
  else
  {
    thread.StepNext(false, m_Workgroup);
    if(thread.StepNeedsGpuSampleGatherOp())
      return;
    if(thread.StepNeedsGpuMathOp())
      return;
    if(thread.StepNeedsDeviceThread())
      return;
  }
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::QueueJob(uint32_t lane)
{
  CHECK_DEBUGGER_THREAD();
  ThreadState &thread = m_Workgroup[lane];
  thread.SetStepQueued();
  if(m_MTSimulation)
  {
    if(D3D12_Hack_ShaderDebugUsesJobSystemJobs())
    {
      Threading::JobSystem::AddJob(
          [this, lane]() { StepThread(lane, StepThreadMode::RUN_MULTIPLE_STEPS); });
    }
    else
    {
      RDCASSERT(Atomic::CmpExch32(&m_QueuedJobs[lane], 0, 1) == 0);
    }
  }
  else
  {
    StepThread(lane, StepThreadMode::RUN_SINGLE_STEP);
  }
}

// Can be called from any thread
void Debugger::QueueDeviceThreadStep(uint32_t lane)
{
  ThreadState &thread = m_Workgroup[lane];
  DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());
  thread.SetStepQueued();
  DXIL_DEBUG_RDCASSERT(!m_QueuedDeviceThreadSteps[lane]);
  m_QueuedDeviceThreadSteps[lane] = true;
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::ProcessQueuedDeviceThreadSteps()
{
  CHECK_DEBUGGER_THREAD();
  for(uint32_t lane = 0; lane < m_QueuedDeviceThreadSteps.size(); ++lane)
  {
    if(m_QueuedDeviceThreadSteps[lane])
    {
      m_QueuedDeviceThreadSteps[lane] = false;
      ThreadState &thread = m_Workgroup[lane];
      thread.SetPendingResultUnknown();
      DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());
      StepThread(lane, StepThreadMode::QUEUE_MULTIPLE_STEPS);
    }
  }
}

// Can be called from any thread
void Debugger::QueueGpuMathOp(uint32_t lane)
{
  ThreadState &thread = m_Workgroup[lane];
  DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());
  DXIL_DEBUG_RDCASSERT(!m_QueuedGpuMathOps[lane]);
  m_QueuedGpuMathOps[lane] = true;
}

// Can be called from any thread
void Debugger::QueueGpuSampleGatherOp(uint32_t lane)
{
  ThreadState &thread = m_Workgroup[lane];
  DXIL_DEBUG_RDCASSERT(thread.IsSimulationStepActive());
  DXIL_DEBUG_RDCASSERT(!m_QueuedGpuSampleGatherOps[lane]);
  m_QueuedGpuSampleGatherOps[lane] = true;
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::ProcessQueuedOps()
{
  CHECK_DEBUGGER_THREAD();
  ProcessQueuedGpuMathOps();
  ProcessQueuedGpuSampleGatherOps();
  SyncPendingGpuOps();
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::SyncPendingLanes()
{
  CHECK_DEBUGGER_THREAD();
  for(uint32_t lane = 0; lane < m_PendingLanes.size(); ++lane)
  {
    if(m_PendingLanes[lane])
    {
      m_PendingLanes[lane] = false;
      ThreadState &thread = m_Workgroup[lane];
      thread.SetPendingResultReady();
      QueueJob(lane);
    }
  }
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::ProcessQueuedGpuMathOps()
{
  CHECK_DEBUGGER_THREAD();
  for(uint32_t lane = 0; lane < m_QueuedGpuMathOps.size(); ++lane)
  {
    if(m_QueuedGpuMathOps[lane])
    {
      if(!m_ApiWrapper->QueuedOpsHasSpace())
        SyncPendingGpuOps();

      m_QueuedGpuMathOps[lane] = false;
      const GpuMathOperation &mathOp = m_Workgroup[lane].GetQueuedGpuMathOp();

      uint32_t workgroupIndex = mathOp.workgroupIndex;
      if(m_ApiWrapper->QueueMathIntrinsic(mathOp.dxOp, mathOp.input))
      {
        m_PendingGpuMathsOpsResults.push_back(mathOp.result);
      }
      else
      {
        ShaderVariable &result = *mathOp.result;
        memset(&result.value, 0, sizeof(result.value));
      }

      DXIL_DEBUG_RDCASSERT(!m_PendingLanes[workgroupIndex]);
      m_PendingLanes[workgroupIndex] = true;
    }
  }
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::ProcessQueuedGpuSampleGatherOps()
{
  CHECK_DEBUGGER_THREAD();
  for(uint32_t lane = 0; lane < m_QueuedGpuSampleGatherOps.size(); ++lane)
  {
    if(m_QueuedGpuSampleGatherOps[lane])
    {
      if(!m_ApiWrapper->QueuedOpsHasSpace())
        SyncPendingGpuOps();

      m_QueuedGpuSampleGatherOps[lane] = false;
      const GpuSampleGatherOperation &sampleGatherOp = m_Workgroup[lane].GetQueuedGpuSampleGatherOp();

      uint32_t workgroupIndex = sampleGatherOp.workgroupIndex;
      ShaderVariable &result = *sampleGatherOp.result;
      bool hasResult = false;
      int sampleRetType = 0;
      if(!m_ApiWrapper->QueueSampleGather(
             sampleGatherOp.dxOp, sampleGatherOp.resourceData, sampleGatherOp.samplerData,
             sampleGatherOp.uv, sampleGatherOp.ddxCalc, sampleGatherOp.ddyCalc,
             sampleGatherOp.texelOffsets, sampleGatherOp.multisampleIndex, sampleGatherOp.lodValue,
             sampleGatherOp.compareValue, sampleGatherOp.gatherChannel,
             sampleGatherOp.instructionIdx, sampleRetType))
      {
        // sample failed. Pretend we got 0 columns back
        set0001(result);
        hasResult = true;
      }
      if(!hasResult)
      {
        m_PendingGpuSampleGatherOpsResults.push_back(sampleGatherOp.result);
        m_PendingGpuSampleGatherOpsSampleRetTypes.push_back(sampleRetType);
      }

      DXIL_DEBUG_RDCASSERT(!m_PendingLanes[workgroupIndex]);
      m_PendingLanes[workgroupIndex] = true;
    }
  }
}

// Must be called from the replay manager thread (the debugger thread)
void Debugger::SyncPendingGpuOps()
{
  CHECK_DEBUGGER_THREAD();
  if(m_PendingGpuMathsOpsResults.empty() && m_PendingGpuSampleGatherOpsResults.empty())
    return;

  if(!(m_ApiWrapper->GetQueuedResults(m_PendingGpuMathsOpsResults, m_PendingGpuSampleGatherOpsResults,
                                      m_PendingGpuSampleGatherOpsSampleRetTypes)))
  {
    RDCERR("GetQueuedResults failed");
    return;
  }
  m_PendingGpuMathsOpsResults.clear();
  m_PendingGpuSampleGatherOpsResults.clear();
  m_PendingGpuSampleGatherOpsSampleRetTypes.clear();
}

void Debugger::SimulationJobHelper()
{
  while(AtomicLoad(&atomic_simulationFinished) == 0)
  {
    for(uint32_t lane = 0; lane < m_Workgroup.size(); ++lane)
    {
      if(Atomic::CmpExch32(&m_QueuedJobs[lane], 1, 0) == 1)
      {
        StepThread(lane, StepThreadMode::RUN_MULTIPLE_STEPS);
      }
    }
  };
}

};    // namespace DXILDebug
