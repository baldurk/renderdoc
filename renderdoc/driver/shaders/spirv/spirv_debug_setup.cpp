/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "spirv_debug.h"
#include "common/formatting.h"
#include "core/settings.h"
#include "spirv_op_helpers.h"
#include "spirv_reflect.h"
#include "var_dispatch_helpers.h"

RDOC_CONFIG(bool, Vulkan_Debug_UseDebugColumnInformation, false,
            "Control whether column information should be read from vulkan debug info.");

RDOC_CONFIG(bool, Vulkan_Hack_AllowNonUniformSubgroups, false,
            "Allow shaders to be debugged with subgroup ops. Most subgroup ops will break, this "
            "will only work for a limited set and not with the 'real' subgroup.");

// this could be cleaner if ShaderVariable wasn't a very public struct, but it's not worth it so
// we just reserve value slots that we know won't be used in opaque variables.
// there's significant wasted space to keep things simple with one property = one slot
static const uint32_t OpaquePointerTypeID = 0x0dd0beef;

enum class PointerFlags
{
  RowMajorMatrix = 0x1,
  SSBO = 0x2,
  GlobalArrayBinding = 0x4,
  DereferencedPhysical = 0x8,
};

BITMASK_OPERATORS(PointerFlags);

// slot 0 for the actual pointer. Shares the same slot as the actual pointer value for GPU pointers
ShaderVariable *getPointer(ShaderVariable &var)
{
  return (ShaderVariable *)(uintptr_t)var.value.u64v[0];
}

const ShaderVariable *getPointer(const ShaderVariable &var)
{
  return (const ShaderVariable *)(uintptr_t)var.value.u64v[0];
}

void setPointer(ShaderVariable &var, const ShaderVariable *ptr)
{
  var.value.u64v[0] = (uint64_t)(uintptr_t)ptr;
}

// slot 1 is the type ID, for opaque pointers this is OpaquePointerTypeID and for real GPU pointers
// this is the type ID of the pointer. We only display this properly for base pointers -
// dereferenced pointers just show the value behind them (otherwise we'd need a pointer type for
// every child element of any pointer type, which is feasible but probably unnecessary)

// slot 2 contains the scalar indices that we carry around from dereferences
void setScalars(ShaderVariable &var, uint8_t scalar0, uint8_t scalar1)
{
  var.value.u64v[2] = (scalar0 << 8) | scalar1;
}

rdcpair<uint8_t, uint8_t> getScalars(const ShaderVariable &var)
{
  return {uint8_t((var.value.u64v[2] >> 8) & 0xff), uint8_t(var.value.u64v[2] & 0xff)};
}

// slot 3 contains the base ID of the structure, for registering pointer changes
void setBaseId(ShaderVariable &var, rdcspv::Id id)
{
  var.value.u64v[3] = id.value();
}

rdcspv::Id getBaseId(const ShaderVariable &var)
{
  return rdcspv::Id::fromWord((uint32_t)var.value.u64v[3]);
}

// slot 4 has the different flags we keep track of
void setPointerFlags(ShaderVariable &var, PointerFlags flags)
{
  var.value.u64v[4] = uint32_t(flags);
}

PointerFlags getPointerFlags(const ShaderVariable &var)
{
  return (PointerFlags)var.value.u64v[4];
}

void enablePointerFlags(ShaderVariable &var, PointerFlags flags)
{
  var.value.u64v[4] = uint32_t((PointerFlags)var.value.u64v[4] | flags);
}

void disablePointerFlags(ShaderVariable &var, PointerFlags flags)
{
  var.value.u64v[4] = uint32_t(PointerFlags((PointerFlags)var.value.u64v[4] & ~flags));
}

bool checkPointerFlags(const ShaderVariable &var, PointerFlags flags)
{
  return ((PointerFlags)var.value.u64v[4] & flags) == flags;
}

// slot 5 has the matrix stride
void setMatrixStride(ShaderVariable &var, uint32_t stride)
{
  var.value.u64v[5] = stride;
}

uint32_t getMatrixStride(const ShaderVariable &var)
{
  return (uint32_t)var.value.u64v[5];
}

// slot 6 has the relative byte offset. For plain bindings the global is created with an offset 0
// and then it's added to for access chains
void setByteOffset(ShaderVariable &var, uint64_t offset)
{
  var.value.u64v[6] = offset;
}

uint64_t getByteOffset(const ShaderVariable &var)
{
  return var.value.u64v[6];
}

// we also use slot 6 for the texture type (because textures and buffers requiring a byte offset are
// disjoint)
void setTextureType(ShaderVariable &var, rdcspv::DebugAPIWrapper::TextureType type)
{
  var.value.u64v[6] = type;
}

rdcspv::DebugAPIWrapper::TextureType getTextureType(const ShaderVariable &var)
{
  return (rdcspv::DebugAPIWrapper::TextureType)var.value.u64v[6];
}

// slot 7 contains the binding array index if we indexed into a global binding array
void setBindArrayIndex(ShaderVariable &var, uint32_t arrayIndex)
{
  var.value.u64v[7] = arrayIndex;
}

uint32_t getBindArrayIndex(const ShaderVariable &var)
{
  return (uint32_t)var.value.u64v[7];
}

// slot 8 contains the ID of the pointer's type, for further buffer type chasing
void setBufferTypeId(ShaderVariable &var, rdcspv::Id id)
{
  var.value.u64v[8] = id.value();
}

rdcspv::Id getBufferTypeId(const ShaderVariable &var)
{
  return rdcspv::Id::fromWord((uint32_t)var.value.u64v[8]);
}

// slot 9 is the array stride. Can't be shared with matrix stride (slot 5) in the case of matrix arrays.
void setArrayStride(ShaderVariable &var, uint32_t stride)
{
  var.value.u64v[9] = stride;
}

uint32_t getArrayStride(const ShaderVariable &var)
{
  return (uint32_t)var.value.u64v[9];
}

static ShaderVariable *pointerIfMutable(const ShaderVariable &var)
{
  return NULL;
}
static ShaderVariable *pointerIfMutable(ShaderVariable &var)
{
  return &var;
}

static void ClampScalars(rdcspv::DebugAPIWrapper *apiWrapper, const ShaderVariable &var,
                         uint8_t &scalar0)
{
  if(scalar0 > var.columns && scalar0 != 0xff)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Invalid scalar index %u at %u-vector %s. Clamping to %u", scalar0,
                          var.columns, var.name.c_str(), var.columns - 1));
    scalar0 = RDCMIN((uint8_t)1, var.columns) - 1;
  }
}

static void ClampScalars(rdcspv::DebugAPIWrapper *apiWrapper, const ShaderVariable &var,
                         uint8_t &scalar0, uint8_t &scalar1)
{
  if(scalar0 > var.columns && scalar0 != 0xff)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Invalid scalar index %u at matrix %s with %u columns. Clamping to %u",
                          scalar0, var.name.c_str(), var.columns, var.columns - 1));
    scalar0 = RDCMIN((uint8_t)1, var.columns) - 1;
  }
  if(scalar1 > var.rows && scalar1 != 0xff)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Invalid scalar index %u at matrix %s with %u rows. Clamping to %u",
                          scalar1, var.name.c_str(), var.rows, var.rows - 1));
    scalar1 = RDCMIN((uint8_t)1, var.rows) - 1;
  }
}

static uint32_t VarByteSize(const ShaderVariable &var)
{
  return VarTypeByteSize(var.type) * RDCMAX(1U, (uint32_t)var.rows) *
         RDCMAX(1U, (uint32_t)var.columns);
}

static void *VarElemPointer(ShaderVariable &var, uint32_t comp)
{
  RDCASSERTNOTEQUAL(var.type, VarType::Unknown);
  byte *ret = (byte *)var.value.u8v.data();
  return ret + comp * VarTypeByteSize(var.type);
}

static const void *VarElemPointer(const ShaderVariable &var, uint32_t comp)
{
  RDCASSERTNOTEQUAL(var.type, VarType::Unknown);
  const byte *ret = (const byte *)var.value.u8v.data();
  return ret + comp * VarTypeByteSize(var.type);
}

namespace rdcspv
{
rdcstr GetRawName(Id id)
{
  // 32-bit value means at most 10 decimal digits, plus a preceeding _, plus trailing NULL.
  char name[12] = {};
  char *ptr = name + 10;
  uint32_t val = id.value();
  do
  {
    *ptr = char('0' + (val % 10));
    ptr--;
    val /= 10;
  } while(val);

  *ptr = '_';

  return ptr;
}

Id ParseRawName(const rdcstr &name)
{
  if(name[0] != '_')
    return Id();

  uint32_t val = 0;
  for(int i = 1; i < name.count(); i++)
  {
    if(name[i] < '0' || name[i] > '9')
      return Id();

    val *= 10;
    val += uint32_t(name[i] - '0');
  }

  return Id::fromWord(val);
}

void AssignValue(ShaderVariable &dst, const ShaderVariable &src)
{
  dst.value = src.value;

  RDCASSERTEQUAL(dst.members.size(), src.members.size());

  for(size_t i = 0; i < src.members.size(); i++)
    AssignValue(dst.members[i], src.members[i]);
}

Debugger::Debugger()
{
}

Debugger::~Debugger()
{
  SAFE_DELETE(apiWrapper);
}

void Debugger::Parse(const rdcarray<uint32_t> &spirvWords)
{
  Processor::Parse(spirvWords);
}

Iter Debugger::GetIterForInstruction(uint32_t inst)
{
  return Iter(m_SPIRV, instructionOffsets[inst]);
}

uint32_t Debugger::GetInstructionForIter(Iter it)
{
  return instructionOffsets.indexOf(it.offs());
}

uint32_t Debugger::GetInstructionForFunction(Id id)
{
  return instructionOffsets.indexOf(functions[id].begin);
}

uint32_t Debugger::GetInstructionForLabel(Id id)
{
  uint32_t ret = labelInstruction[id];
  RDCASSERT(ret);
  return ret;
}

const rdcspv::DataType &Debugger::GetType(Id typeId)
{
  return dataTypes[typeId];
}

const rdcspv::DataType &Debugger::GetTypeForId(Id ssaId)
{
  return dataTypes[idTypes[ssaId]];
}

const Decorations &Debugger::GetDecorations(Id typeId)
{
  return decorations[typeId];
}

void Debugger::MakeSignatureNames(const rdcarray<SPIRVInterfaceAccess> &sigList,
                                  rdcarray<rdcstr> &sigNames)
{
  for(const SPIRVInterfaceAccess &sig : sigList)
  {
    rdcstr name = GetRawName(sig.ID);

    const DataType *type = &dataTypes[idTypes[sig.ID]];

    RDCASSERT(type->type == DataType::PointerType);
    type = &dataTypes[type->InnerType()];

    for(uint32_t chain : sig.accessChain)
    {
      if(type->type == DataType::ArrayType)
      {
        name += StringFormat::Fmt("[%u]", chain);
        type = &dataTypes[type->InnerType()];
      }
      else if(type->type == DataType::StructType)
      {
        if(!type->children[chain].name.empty())
          name += "." + type->children[chain].name;
        else
          name += StringFormat::Fmt("._child%u", chain);
        type = &dataTypes[type->children[chain].type];
      }
      else if(type->type == DataType::MatrixType)
      {
        name += StringFormat::Fmt(".col%u", chain);
        type = &dataTypes[type->InnerType()];
      }
      else
      {
        RDCERR("Got access chain with non-aggregate type in interface.");
        break;
      }
    }

    sigNames.push_back(name);
  }
}

// this function is implemented here to keep it next to the code we might need to update, even
// though it's checked at reflection time.
void Reflector::CheckDebuggable(bool &debuggable, rdcstr &debugStatus) const
{
  debuggable = true;
  debugStatus.clear();

  if(m_MajorVersion > 1 || m_MinorVersion > 6)
  {
    debugStatus +=
        StringFormat::Fmt("Unsupported SPIR-V version %u.%u\n", m_MajorVersion, m_MinorVersion);
    debuggable = false;
  }

  // this list is sorted in order of the SPIR-V registry.
  const rdcstr whitelist[] = {
      "SPV_KHR_shader_draw_parameters",
      "SPV_KHR_16bit_storage",
      "SPV_KHR_device_group",
      "SPV_KHR_multiview",
      "SPV_KHR_storage_buffer_storage_class",
      "SPV_KHR_post_depth_coverage",
      "SPV_KHR_shader_atomic_counter_ops",
      "SPV_EXT_shader_stencil_export",
      "SPV_EXT_shader_viewport_index_layer",
      "SPV_EXT_fragment_fully_covered",
      "SPV_GOOGLE_decorate_string",
      "SPV_GOOGLE_hlsl_functionality1",
      "SPV_EXT_descriptor_indexing",
      "SPV_KHR_8bit_storage",
      "SPV_KHR_vulkan_memory_model",
      "SPV_EXT_fragment_invocation_density",
      "SPV_KHR_no_integer_wrap_decoration",
      "SPV_KHR_float_controls",
      "SPV_EXT_physical_storage_buffer",
      "SPV_KHR_shader_clock",
      "SPV_EXT_demote_to_helper_invocation",
      "SPV_KHR_non_semantic_info",
      "SPV_EXT_shader_atomic_float_add",
      "SPV_KHR_terminate_invocation",
      "SPV_EXT_shader_image_int64",
      "SPV_GOOGLE_user_type",
      "SPV_KHR_physical_storage_buffer",
      "SPV_KHR_relaxed_extended_instruction",
  };

  // whitelist supported extensions
  for(const rdcstr &ext : extensions)
  {
    bool supported = false;
    for(const rdcstr &check : whitelist)
    {
      if(ext == check)
      {
        supported = true;
        break;
      }
    }

    if(supported)
      continue;

    debuggable = false;
    debugStatus += StringFormat::Fmt("Unsupported SPIR-V extension %s\n", ext.c_str());
  }

  for(Capability c : capabilities)
  {
    bool supported = false;
    switch(c)
    {
      case Capability::Matrix:
      case Capability::Shader:
      // we "support" geometry/tessellation in case the module contains other entry points, but
      // these can't be debugged right now.
      case Capability::Geometry:
      case Capability::Tessellation:
      case Capability::AtomicStorage:
      case Capability::TessellationPointSize:
      case Capability::GeometryPointSize:
      case Capability::ImageGatherExtended:
      case Capability::StorageImageMultisample:
      case Capability::UniformBufferArrayDynamicIndexing:
      case Capability::SampledImageArrayDynamicIndexing:
      case Capability::StorageBufferArrayDynamicIndexing:
      case Capability::StorageImageArrayDynamicIndexing:
      case Capability::ClipDistance:
      case Capability::CullDistance:
      case Capability::ImageCubeArray:
      case Capability::SampleRateShading:
      case Capability::ImageRect:
      case Capability::SampledRect:
      case Capability::InputAttachment:
      case Capability::MinLod:
      case Capability::Sampled1D:
      case Capability::Image1D:
      case Capability::SampledCubeArray:
      case Capability::SampledBuffer:
      case Capability::ImageBuffer:
      case Capability::ImageMSArray:
      case Capability::StorageImageExtendedFormats:
      case Capability::ImageQuery:
      case Capability::DerivativeControl:
      case Capability::TransformFeedback:
      case Capability::GeometryStreams:
      case Capability::StorageImageReadWithoutFormat:
      case Capability::StorageImageWriteWithoutFormat:
      case Capability::MultiViewport:
      case Capability::ShaderLayer:
      case Capability::ShaderViewportIndex:
      case Capability::DrawParameters:
      case Capability::DeviceGroup:
      case Capability::MultiView:
      case Capability::AtomicStorageOps:
      case Capability::SampleMaskPostDepthCoverage:
      case Capability::StencilExportEXT:
      case Capability::ShaderClockKHR:
      case Capability::ShaderViewportIndexLayerEXT:
      case Capability::FragmentFullyCoveredEXT:
      case Capability::FragmentDensityEXT:
      case Capability::ShaderNonUniform:
      case Capability::RuntimeDescriptorArray:
      case Capability::InputAttachmentArrayDynamicIndexing:
      case Capability::UniformTexelBufferArrayDynamicIndexing:
      case Capability::StorageTexelBufferArrayDynamicIndexing:
      case Capability::UniformBufferArrayNonUniformIndexing:
      case Capability::SampledImageArrayNonUniformIndexing:
      case Capability::StorageBufferArrayNonUniformIndexing:
      case Capability::StorageImageArrayNonUniformIndexing:
      case Capability::InputAttachmentArrayNonUniformIndexing:
      case Capability::UniformTexelBufferArrayNonUniformIndexing:
      case Capability::StorageTexelBufferArrayNonUniformIndexing:
      case Capability::VulkanMemoryModel:
      case Capability::VulkanMemoryModelDeviceScope:
      case Capability::DemoteToHelperInvocationEXT:
      case Capability::AtomicFloat32AddEXT:
      case Capability::AtomicFloat32MinMaxEXT:
      case Capability::AtomicFloat16AddEXT:
      case Capability::AtomicFloat16MinMaxEXT:
      case Capability::AtomicFloat64AddEXT:
      case Capability::AtomicFloat64MinMaxEXT:
      case Capability::Float16Buffer:
      case Capability::Float16:
      case Capability::Int64:
      case Capability::Int16:
      case Capability::Int8:
      case Capability::StorageBuffer16BitAccess:
      case Capability::UniformAndStorageBuffer16BitAccess:
      case Capability::StoragePushConstant16:
      case Capability::StorageInputOutput16:
      case Capability::StorageBuffer8BitAccess:
      case Capability::UniformAndStorageBuffer8BitAccess:
      case Capability::StoragePushConstant8:
      case Capability::Float64:
      case Capability::Int64Atomics:
      case Capability::Int64ImageEXT:
      case Capability::ExpectAssumeKHR:
      case Capability::BitInstructions:
      case Capability::UniformDecoration:
      case Capability::SignedZeroInfNanPreserve:
      case Capability::PhysicalStorageBufferAddresses:
      {
        supported = true;
        break;
      }

      case Capability::GroupNonUniformArithmetic:
      {
        if(Vulkan_Hack_AllowNonUniformSubgroups())
        {
          supported = true;
        }
        else
        {
          supported = false;
        }
        break;
      }

      // we plan to support these but needs additional testing/proving

      // MSAA custom interpolation
      case Capability::InterpolationFunction:

      // variable pointers
      case Capability::VariablePointersStorageBuffer:
      case Capability::VariablePointers:

      // float controls
      case Capability::DenormPreserve:
      case Capability::DenormFlushToZero:
      case Capability::RoundingModeRTE:
      case Capability::RoundingModeRTZ:

      // group instructions
      case Capability::Groups:
      case Capability::GroupNonUniform:
      case Capability::GroupNonUniformVote:
      case Capability::GroupNonUniformBallot:
      case Capability::GroupNonUniformShuffle:
      case Capability::GroupNonUniformShuffleRelative:
      case Capability::GroupNonUniformClustered:
      case Capability::GroupNonUniformQuad:
      case Capability::SubgroupBallotKHR:
      case Capability::SubgroupVoteKHR:
      case Capability::GroupNonUniformRotateKHR:

      // workgroup layout:
      case Capability::WorkgroupMemoryExplicitLayout16BitAccessKHR:
      case Capability::WorkgroupMemoryExplicitLayout8BitAccessKHR:
      case Capability::WorkgroupMemoryExplicitLayoutKHR:

      // sparse operations
      case Capability::SparseResidency:

      // fragment interlock
      case Capability::FragmentShaderSampleInterlockEXT:
      case Capability::FragmentShaderShadingRateInterlockEXT:
      case Capability::FragmentShaderPixelInterlockEXT:
      {
        supported = false;
        break;
      }

      // fragment shading rate
      case Capability::FragmentShadingRateKHR:
      {
        supported = false;
        break;
      }

      // integer dot product
      case Capability::DotProductKHR:
      case Capability::DotProductInput4x8BitKHR:
      case Capability::DotProductInput4x8BitPackedKHR:
      case Capability::DotProductInputAllKHR:
      {
        supported = false;
        break;
      }

      // raytracing
      case Capability::RayQueryKHR:
      case Capability::RayTraversalPrimitiveCullingKHR:
      case Capability::RayTracingKHR:
      case Capability::RayCullMaskKHR:
      case Capability::RayTracingOpacityMicromapEXT:
      case Capability::ShaderInvocationReorderNV:
      {
        supported = false;
        break;
      }

      // mesh shading
      case Capability::MeshShadingEXT:
      {
        supported = false;
        break;
      }

      // no plans to support these - mostly Kernel/OpenCL related or vendor extensions
      case Capability::Addresses:
      case Capability::Linkage:
      case Capability::Kernel:
      case Capability::Vector16:
      case Capability::ImageBasic:
      case Capability::ImageReadWrite:
      case Capability::ImageMipmap:
      case Capability::Pipes:
      case Capability::DeviceEnqueue:
      case Capability::LiteralSampler:
      case Capability::GenericPointer:
      case Capability::SubgroupDispatch:
      case Capability::NamedBarrier:
      case Capability::PipeStorage:
      case Capability::Float16ImageAMD:
      case Capability::ImageGatherBiasLodAMD:
      case Capability::FragmentMaskAMD:
      case Capability::ImageReadWriteLodAMD:
      case Capability::SampleMaskOverrideCoverageNV:
      case Capability::GeometryShaderPassthroughNV:
      case Capability::ShaderViewportMaskNV:
      case Capability::ShaderStereoViewNV:
      case Capability::PerViewAttributesNV:
      case Capability::MeshShadingNV:
      case Capability::FragmentBarycentricNV:
      case Capability::ImageFootprintNV:
      case Capability::ComputeDerivativeGroupQuadsNV:
      case Capability::GroupNonUniformPartitionedNV:
      case Capability::RayTracingNV:
      case Capability::ComputeDerivativeGroupLinearNV:
      case Capability::CooperativeMatrixNV:
      case Capability::ShaderSMBuiltinsNV:
      case Capability::SubgroupShuffleINTEL:
      case Capability::SubgroupBufferBlockIOINTEL:
      case Capability::SubgroupImageBlockIOINTEL:
      case Capability::SubgroupImageMediaBlockIOINTEL:
      case Capability::IntegerFunctions2INTEL:
      case Capability::SubgroupAvcMotionEstimationINTEL:
      case Capability::SubgroupAvcMotionEstimationIntraINTEL:
      case Capability::SubgroupAvcMotionEstimationChromaINTEL:
      case Capability::FunctionPointersINTEL:
      case Capability::IndirectReferencesINTEL:
      case Capability::FPGAKernelAttributesINTEL:
      case Capability::FPGALoopControlsINTEL:
      case Capability::FPGAMemoryAttributesINTEL:
      case Capability::FPGARegINTEL:
      case Capability::UnstructuredLoopControlsINTEL:
      case Capability::KernelAttributesINTEL:
      case Capability::BlockingPipesINTEL:
      case Capability::OptNoneINTEL:
      case Capability::RayTracingMotionBlurNV:
      case Capability::RoundToInfinityINTEL:
      case Capability::FloatingPointModeINTEL:
      case Capability::AsmINTEL:
      case Capability::VectorAnyINTEL:
      case Capability::VectorComputeINTEL:
      case Capability::VariableLengthArrayINTEL:
      case Capability::FunctionFloatControlINTEL:
      case Capability::FPFastMathModeINTEL:
      case Capability::ArbitraryPrecisionFixedPointINTEL:
      case Capability::ArbitraryPrecisionFloatingPointINTEL:
      case Capability::ArbitraryPrecisionIntegersINTEL:
      case Capability::FPGAMemoryAccessesINTEL:
      case Capability::FPGAClusterAttributesINTEL:
      case Capability::LoopFuseINTEL:
      case Capability::FPGABufferLocationINTEL:
      case Capability::USMStorageClassesINTEL:
      case Capability::IOPipesINTEL:
      case Capability::LongCompositesINTEL:
      case Capability::DebugInfoModuleINTEL:
      case Capability::BindlessTextureNV:
      case Capability::MemoryAccessAliasingINTEL:
      case Capability::SplitBarrierINTEL:
      case Capability::GroupUniformArithmeticKHR:
      case Capability::CoreBuiltinsARM:
      case Capability::FPGADSPControlINTEL:
      case Capability::FPGAInvocationPipeliningAttributesINTEL:
      case Capability::RuntimeAlignedAttributeINTEL:
      case Capability::TileImageColorReadAccessEXT:
      case Capability::TileImageDepthReadAccessEXT:
      case Capability::TileImageStencilReadAccessEXT:
      case Capability::TextureSampleWeightedQCOM:
      case Capability::TextureBoxFilterQCOM:
      case Capability::TextureBlockMatchQCOM:
      case Capability::RayQueryPositionFetchKHR:
      case Capability::RayTracingPositionFetchKHR:
      case Capability::BFloat16ConversionINTEL:
      case Capability::FPGAKernelAttributesv2INTEL:
      case Capability::FPGALatencyControlINTEL:
      case Capability::FPGAArgumentInterfacesINTEL:
      case Capability::TextureBlockMatch2QCOM:
      case Capability::ShaderEnqueueAMDX:
      case Capability::QuadControlKHR:
      case Capability::DisplacementMicromapNV:
      case Capability::AtomicFloat16VectorNV:
      case Capability::RayTracingDisplacementMicromapNV:
      case Capability::CooperativeMatrixKHR:
      case Capability::FloatControls2:
      case Capability::FPGAClusterAttributesV2INTEL:
      case Capability::FPMaxErrorINTEL:
      case Capability::GlobalVariableFPGADecorationsINTEL:
      case Capability::MaskedGatherScatterINTEL:
      case Capability::CacheControlsINTEL:
      case Capability::RegisterLimitsINTEL:
      case Capability::GlobalVariableHostAccessINTEL:
      case Capability::CooperativeMatrixLayoutsARM:
      case Capability::RawAccessChainsNV:
      case Capability::ReplicatedCompositesEXT:
      case Capability::Max:
      case Capability::Invalid:
      {
        supported = false;
        break;
      }

      // deprecated provisional raytracing
      case Capability::RayQueryProvisionalKHR:
      case Capability::RayTracingProvisionalKHR:
      {
        supported = false;
        break;
      }
    }

    if(!supported)
    {
      debuggable = false;
      debugStatus += StringFormat::Fmt("Unsupported capability '%s'\n", ToStr(c).c_str());
    }
  }

  for(auto it = extSets.begin(); it != extSets.end(); it++)
  {
    Id id = it->first;
    const rdcstr &setname = it->second;

    if(setname == "GLSL.std.450" || setname.beginsWith("NonSemantic."))
      continue;

    debuggable = false;
    debugStatus += StringFormat::Fmt("Unsupported extended instruction set: '%s'\n", setname.c_str());
  }

  debugStatus.trim();
}

void Debugger::SetStructArrayNames(ShaderVariable &c, const DataType *typeWalk,
                                   const rdcarray<SpecConstant> &specInfo)
{
  if(typeWalk->type == DataType::StructType)
  {
    RDCASSERTEQUAL(c.members.size(), typeWalk->children.size());
    for(size_t i = 0; i < c.members.size(); ++i)
    {
      const DataType::Child &child = typeWalk->children[i];
      const DataType *childType = &dataTypes[child.type];
      if(!child.name.empty())
        c.members[i].name = child.name;
      else
        c.members[i].name = StringFormat::Fmt("_child%d", i);

      SetStructArrayNames(c.members[i], childType, specInfo);
    }
  }
  else if(typeWalk->type == DataType::ArrayType)
  {
    uint32_t arraySize = EvaluateConstant(typeWalk->length, specInfo).value.u32v[0];
    const DataType *childType = &dataTypes[typeWalk->InnerType()];
    for(size_t i = 0; i < arraySize; ++i)
    {
      c.members[i].name = StringFormat::Fmt("[%u]", i);
      SetStructArrayNames(c.members[i], childType, specInfo);
    }
  }
}

ShaderDebugTrace *Debugger::BeginDebug(DebugAPIWrapper *api, const ShaderStage shaderStage,
                                       const rdcstr &entryPoint,
                                       const rdcarray<SpecConstant> &specInfo,
                                       const std::map<size_t, uint32_t> &instructionLines,
                                       const SPIRVPatchData &patchData, uint32_t activeIndex)
{
  Id entryId = entryLookup[ShaderEntryPoint(entryPoint, shaderStage)];

  if(entryId == Id())
  {
    RDCERR("Invalid entry point '%s'", entryPoint.c_str());
    return new ShaderDebugTrace;
  }

  rdcarray<Id> entryInterface;

  for(const EntryPoint &e : entries)
  {
    if(e.id == entryId)
    {
      entryInterface = e.usedIds;
      break;
    }
  }

  global.clock = uint64_t(time(NULL)) << 32;

  for(auto it = extSets.begin(); it != extSets.end(); it++)
  {
    Id id = it->first;
    const rdcstr &setname = it->second;

    if(setname == "GLSL.std.450")
    {
      ExtInstDispatcher extinst;

      extinst.name = setname;

      ConfigureGLSLStd450(extinst);

      global.extInsts[id] = extinst;
    }
    else if(setname.beginsWith("NonSemantic."))
    {
      ExtInstDispatcher extinst;

      extinst.name = setname;

      extinst.nonsemantic = true;

      global.extInsts[id] = extinst;
    }
  }

  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->debugger = this;
  ret->stage = shaderStage;
  activeLaneIndex = activeIndex;
  stage = shaderStage;
  apiWrapper = api;

  uint32_t workgroupSize = shaderStage == ShaderStage::Pixel ? 4 : 1;
  for(uint32_t i = 0; i < workgroupSize; i++)
    workgroup.push_back(ThreadState(i, *this, global));

  ThreadState &active = GetActiveLane();

  active.nextInstruction = instructionOffsets.indexOf(functions[entryId].begin);

  active.ids.resize(idOffsets.size());

  // array names and struct member names are not set when constants are created
  for(auto it = constants.begin(); it != constants.end(); ++it)
  {
    Constant &c = it->second;

    const DataType *typeWalk = &dataTypes[c.type];
    SetStructArrayNames(c.value, typeWalk, specInfo);
  }

  // evaluate all constants
  for(auto it = constants.begin(); it != constants.end(); it++)
  {
    active.ids[it->first] = EvaluateConstant(it->first, specInfo);
    active.ids[it->first].name = GetRawName(it->first);
  }

  rdcarray<rdcstr> inputSigNames, outputSigNames;

  MakeSignatureNames(patchData.inputs, inputSigNames);
  MakeSignatureNames(patchData.outputs, outputSigNames);

  struct PointerId
  {
    PointerId(Id i, rdcarray<ShaderVariable> GlobalState::*th, rdcarray<ShaderVariable> &storage)
        : id(i), globalStorage(th), index(storage.size() - 1)
    {
    }
    PointerId(Id i, rdcarray<ShaderVariable> ThreadState::*th, rdcarray<ShaderVariable> &storage)
        : id(i), threadStorage(th), index(storage.size() - 1)
    {
    }

    void Set(Debugger &d, const GlobalState &global, ThreadState &lane) const
    {
      if(globalStorage)
        lane.ids[id] = d.MakePointerVariable(id, &(global.*globalStorage)[index]);
      else
        lane.ids[id] = d.MakePointerVariable(id, &(lane.*threadStorage)[index]);
    }

    Id id;
    rdcarray<ShaderVariable> GlobalState::*globalStorage = NULL;
    rdcarray<ShaderVariable> ThreadState::*threadStorage = NULL;
    size_t index;
  };

#define GLOBAL_POINTER(id, list) PointerId(id, &GlobalState::list, global.list)
#define THREAD_POINTER(id, list) PointerId(id, &ThreadState::list, active.list)

  rdcarray<Id> inputIDs, outputIDs;
  rdcarray<PointerId> pointerIDs;

  // allocate storage for globals with opaque storage classes, and prepare to set up pointers to
  // them for the global variables themselves
  for(const Variable &v : globals)
  {
    if(v.storage == StorageClass::Input || v.storage == StorageClass::Output)
    {
      if(!entryInterface.contains(v.id))
        continue;

      const bool isInput = (v.storage == StorageClass::Input);

      ShaderVariable var;
      var.name = GetRawName(v.id);

      rdcstr rawName = var.name;
      rdcstr sourceName = GetHumanName(v.id);

      // if we don't have a good human name, generate a better one using the interface information
      // we have
      if(sourceName == var.name)
      {
        if(decorations[v.id].flags & Decorations::HasBuiltIn)
          sourceName = StringFormat::Fmt("_%s", ToStr(decorations[v.id].builtIn).c_str());
        else if(decorations[v.id].flags & Decorations::HasLocation)
          sourceName =
              StringFormat::Fmt("_%s%u", isInput ? "input" : "output", decorations[v.id].location);
        else
          sourceName = StringFormat::Fmt("_sig%u", v.id.value());

        for(const DecorationAndParamData &d : decorations[v.id].others)
        {
          if(d.value == Decoration::Component)
            sourceName += StringFormat::Fmt("_%u", d.component);
        }
      }

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      const rdcarray<rdcstr> &sigNames = isInput ? inputSigNames : outputSigNames;

      bool addSource = m_DebugInfo.valid ? m_DebugInfo.globals.contains(v.id) : true;

      // fill the interface variable
      auto fillInputCallback = [this, isInput, addSource, ret, &sigNames, &rawName, &sourceName](
                                   ShaderVariable &var, const Decorations &curDecorations,
                                   const DataType &type, uint64_t location,
                                   const rdcstr &accessSuffix) {
        if(!var.members.empty())
          return;

        if(isInput)
        {
          uint32_t component = 0;
          for(const DecorationAndParamData &dec : curDecorations.others)
          {
            if(dec.value == Decoration::Component)
            {
              component = dec.component;
              break;
            }
          }

          ShaderBuiltin builtin = ShaderBuiltin::Undefined;
          if(curDecorations.flags & Decorations::HasBuiltIn)
            builtin = MakeShaderBuiltin(stage, curDecorations.builtIn);

          this->apiWrapper->FillInputValue(var, builtin, (uint32_t)location, component);
        }
        else
        {
          // make it obvious when uninitialised outputs are written
          memset(&var.value, 0xcc, sizeof(var.value));
        }

        if(sourceName != rawName)
        {
          rdcstr debugVarName = rawName + accessSuffix;

          SourceVariableMapping sourceVar;
          sourceVar.name = sourceName + accessSuffix;
          sourceVar.offset = (uint32_t)location;
          sourceVar.type = var.type;
          sourceVar.rows = var.rows;
          sourceVar.columns = var.columns;
          sourceVar.signatureIndex = sigNames.indexOf(debugVarName);

          StripCommonGLPrefixes(sourceVar.name);

          for(uint32_t x = 0; x < uint32_t(var.rows) * var.columns; x++)
            sourceVar.variables.push_back(DebugVariableReference(
                isInput ? DebugVariableType::Input : DebugVariableType::Variable, debugVarName, x));

          if(isInput)
            ret->sourceVars.push_back(sourceVar);
          else if(addSource)
            ret->sourceVars.push_back(sourceVar);
        }
      };

      WalkVariable<ShaderVariable, true>(decorations[v.id], dataTypes[type.InnerType()], ~0U, var,
                                         rdcstr(), fillInputCallback);

      if(isInput)
      {
        // create the opaque storage
        active.inputs.push_back(var);

        // then make sure we know which ID to set up for the pointer
        inputIDs.push_back(v.id);
        pointerIDs.push_back(THREAD_POINTER(v.id, inputs));
      }
      else
      {
        active.outputs.push_back(var);
        outputIDs.push_back(v.id);
        liveGlobals.push_back(v.id);
        pointerIDs.push_back(THREAD_POINTER(v.id, outputs));
      }
    }

    // pick up uniform globals, which could be cbuffers, and push constants
    else if(v.storage == StorageClass::Uniform || v.storage == StorageClass::StorageBuffer ||
            v.storage == StorageClass::PushConstant)
    {
      if(!patchData.usedIds.contains(v.id))
        continue;

      ShaderVariable var;
      var.name = GetRawName(v.id);

      rdcstr sourceName = GetHumanName(v.id);

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      const DataType *innertype = &dataTypes[type.InnerType()];

      if(sourceName == var.name)
        sourceName = GetHumanName(innertype->id);

      bool isArray = false;
      uint32_t arraySize = 1;
      if(innertype->type == DataType::ArrayType)
      {
        isArray = true;
        if(innertype->length == Id())
          arraySize = ~0U;
        else
          arraySize = EvaluateConstant(innertype->length, specInfo).value.u32v[0];
        innertype = &dataTypes[innertype->InnerType()];
      }

      const bool ssbo = (v.storage == StorageClass::StorageBuffer) ||
                        (decorations[innertype->id].flags & Decorations::BufferBlock);

      if(innertype->type == DataType::StructType)
      {
        // if we don't have a good human name, generate a better one using the interface information
        // we have
        if(sourceName == var.name)
        {
          if(v.storage == StorageClass::PushConstant)
            sourceName = "_pushconsts";
          else if(ssbo)
            sourceName = StringFormat::Fmt("_buffer_set%u_bind%u", decorations[v.id].set,
                                           decorations[v.id].binding);
          else
            sourceName = StringFormat::Fmt("_cbuffer_set%u_bind%u", decorations[v.id].set,
                                           decorations[v.id].binding);
        }

        SourceVariableMapping sourceVar;
        sourceVar.name = sourceName;
        sourceVar.offset = 0;

        if(ssbo)
        {
          var.rows = 1;
          var.columns = 1;
          var.type = VarType::ReadWriteResource;

          int32_t idx = patchData.rwInterface.indexOf(v.id);
          RDCASSERT(idx >= 0);
          var.SetBindIndex(ShaderBindIndex(DescriptorCategory::ReadWriteResource, idx, 0U));

          enablePointerFlags(var, PointerFlags::SSBO);

          if(isArray)
            enablePointerFlags(var, PointerFlags::GlobalArrayBinding);

          sourceVar.type = VarType::ReadWriteResource;
          sourceVar.rows = 1;
          sourceVar.columns = 1;
          sourceVar.variables.push_back(
              DebugVariableReference(DebugVariableType::ReadWriteResource, var.name));

          global.readWriteResources.push_back(var);
          pointerIDs.push_back(GLOBAL_POINTER(v.id, readWriteResources));
        }
        else
        {
          ShaderBindIndex binding;

          binding.category = DescriptorCategory::ConstantBlock;
          binding.index = patchData.cblockInterface.indexOf(v.id);
          RDCASSERT(binding.index != ~0U);

          auto cbufferCallback = [this, &binding](
                                     ShaderVariable &var, const Decorations &curDecorations,
                                     const DataType &type, uint64_t offset, const rdcstr &) {
            if(!var.members.empty())
              return;

            // non-matrix case is simple, just read the size of the variable
            if(var.rows == 1)
            {
              this->apiWrapper->ReadBufferValue(binding, offset, VarByteSize(var),
                                                var.value.u8v.data());

              if(type.type == DataType::PointerType)
                var.SetTypedPointer(var.value.u64v[0], this->apiWrapper->GetShaderID(),
                                    idToPointerType[type.InnerType()]);
            }
            else
            {
              // matrix case is more complicated. Either read column by column or row by row
              // depending on majorness
              uint32_t matrixStride = curDecorations.matrixStride;

              if(!(curDecorations.flags & Decorations::HasMatrixStride))
              {
                RDCWARN("Matrix without matrix stride - assuming legacy vec4 packed");
                matrixStride = 16;
              }

              if(curDecorations.flags & Decorations::ColMajor)
              {
                ShaderVariable tmp;
                tmp.type = var.type;

                uint32_t colSize = VarTypeByteSize(var.type) * var.rows;
                for(uint32_t c = 0; c < var.columns; c++)
                {
                  // read the column
                  this->apiWrapper->ReadBufferValue(binding, offset + c * matrixStride, colSize,
                                                    VarElemPointer(tmp, 0));

                  // now write it into the appropiate elements in the destination ShaderValue
                  for(uint32_t r = 0; r < var.rows; r++)
                    copyComp(var, r * var.columns + c, tmp, r);
                }
              }
              else
              {
                // row major is easier, read row-by-row directly into the output variable
                uint32_t rowSize = VarTypeByteSize(var.type) * var.columns;
                for(uint32_t r = 0; r < var.rows; r++)
                {
                  // read the column into the destination ShaderValue, which is tightly packed with
                  // rows
                  this->apiWrapper->ReadBufferValue(binding, offset + r * matrixStride, rowSize,
                                                    VarElemPointer(var, r * var.columns));
                }
              }
            }
          };

          if(isArray)
          {
            if(arraySize == ~0U)
            {
              RDCERR("Unsupported runtime array of UBOs");
              arraySize = 1;
            }

            var.members.reserve(arraySize);

            for(uint32_t a = 0; a < arraySize; a++)
            {
              binding.arrayElement = a;
              var.members.push_back(ShaderVariable());
              var.members.back().name = StringFormat::Fmt("[%u]", a);
              WalkVariable<ShaderVariable, true>(decorations[v.id], *innertype, 0U,
                                                 var.members.back(), rdcstr(), cbufferCallback);
            }
          }
          else
          {
            WalkVariable<ShaderVariable, true>(decorations[v.id], *innertype, 0U, var, rdcstr(),
                                               cbufferCallback);
          }

          sourceVar.type = VarType::ConstantBlock;
          sourceVar.rows = 1;
          sourceVar.columns = 1;
          sourceVar.variables.push_back(DebugVariableReference(DebugVariableType::Constant, var.name));

          global.constantBlocks.push_back(var);
          pointerIDs.push_back(GLOBAL_POINTER(v.id, constantBlocks));
        }

        ret->sourceVars.push_back(sourceVar);
      }
      else
      {
        RDCERR("Unhandled type of uniform: %u", innertype->type);
      }
    }
    else if(v.storage == StorageClass::UniformConstant)
    {
      if(!patchData.usedIds.contains(v.id))
        continue;

      // only images/samplers are allowed to be in UniformConstant
      ShaderVariable var;
      var.rows = 1;
      var.columns = 1;
      var.name = GetRawName(v.id);

      rdcstr sourceName = GetHumanName(v.id);

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      const DataType *innertype = &dataTypes[type.InnerType()];

      // if we don't have a good human name, generate a better one using the interface information
      // we have
      if(sourceName == var.name)
      {
        rdcstr innerName;
        if(innertype->type == DataType::SamplerType)
          innerName = "sampler";
        else if(innertype->type == DataType::SampledImageType)
          innerName = "sampledImage";
        else if(innertype->type == DataType::ImageType)
          innerName = "image";
        else if(innertype->type == DataType::AccelerationStructureType)
          innerName = "accelerationStructure";
        sourceName = StringFormat::Fmt("_%s_set%u_bind%u", innerName.c_str(), decorations[v.id].set,
                                       decorations[v.id].binding);
      }

      DebugVariableType debugType = DebugVariableType::ReadOnlyResource;

      uint32_t set = 0, bind = 0;
      if(decorations[v.id].flags & Decorations::HasDescriptorSet)
        set = decorations[v.id].set;
      if(decorations[v.id].flags & Decorations::HasBinding)
        bind = decorations[v.id].binding;

      if(innertype->type == DataType::ArrayType)
      {
        enablePointerFlags(var, PointerFlags::GlobalArrayBinding);
        innertype = &dataTypes[innertype->InnerType()];
      }

      if(innertype->type == DataType::SamplerType)
      {
        var.type = VarType::Sampler;
        debugType = DebugVariableType::Sampler;

        int32_t idx = patchData.samplerInterface.indexOf(v.id);
        RDCASSERT(idx >= 0);
        var.SetBindIndex(ShaderBindIndex(DescriptorCategory::Sampler, idx, 0U));

        global.samplers.push_back(var);
        pointerIDs.push_back(GLOBAL_POINTER(v.id, samplers));
      }
      else if(innertype->type == DataType::SampledImageType || innertype->type == DataType::ImageType)
      {
        var.type = VarType::ReadOnlyResource;
        debugType = DebugVariableType::ReadOnlyResource;

        // store the texture type here, since the image may be copied around and combined with a
        // sampler, so accessing the original type might be non-trivial at point of access
        uint32_t texType = DebugAPIWrapper::Float_Texture;

        Id imgid = innertype->id;

        if(innertype->type == DataType::SampledImageType)
          imgid = sampledImageTypes[imgid].baseId;

        RDCASSERT(imageTypes[imgid].dim != Dim::Max);

        if(imageTypes[imgid].dim == Dim::Buffer)
          texType |= DebugAPIWrapper::Buffer_Texture;

        if(imageTypes[imgid].dim == Dim::SubpassData)
          texType |= DebugAPIWrapper::Subpass_Texture;

        if(imageTypes[imgid].retType.type == Op::TypeInt)
        {
          if(imageTypes[imgid].retType.signedness)
            texType |= DebugAPIWrapper::SInt_Texture;
          else
            texType |= DebugAPIWrapper::UInt_Texture;
        }

        setTextureType(var, (DebugAPIWrapper::TextureType)texType);

        if(imageTypes[imgid].sampled == 2 && imageTypes[imgid].dim != Dim::SubpassData)
        {
          var.type = VarType::ReadWriteResource;
          debugType = DebugVariableType::ReadWriteResource;

          int32_t idx = patchData.rwInterface.indexOf(v.id);
          RDCASSERT(idx >= 0);
          var.SetBindIndex(ShaderBindIndex(DescriptorCategory::ReadWriteResource, idx, 0U));

          global.readWriteResources.push_back(var);
          pointerIDs.push_back(GLOBAL_POINTER(v.id, readWriteResources));
        }
        else
        {
          int32_t idx = patchData.roInterface.indexOf(v.id);
          RDCASSERT(idx >= 0);
          var.SetBindIndex(ShaderBindIndex(DescriptorCategory::ReadOnlyResource, idx, 0U));

          global.readOnlyResources.push_back(var);
          pointerIDs.push_back(GLOBAL_POINTER(v.id, readOnlyResources));
        }
      }
      else if(innertype->type == DataType::AccelerationStructureType)
      {
        var.type = VarType::ReadOnlyResource;
        debugType = DebugVariableType::ReadOnlyResource;

        global.readOnlyResources.push_back(var);
        pointerIDs.push_back(GLOBAL_POINTER(v.id, readOnlyResources));
      }
      else
      {
        RDCERR("Unhandled type of uniform: %u", innertype->type);
      }

      SourceVariableMapping sourceVar;
      sourceVar.name = sourceName;
      sourceVar.type = var.type;
      sourceVar.rows = 1;
      sourceVar.columns = 1;
      sourceVar.offset = 0;
      sourceVar.variables.push_back(DebugVariableReference(debugType, var.name));

      ret->sourceVars.push_back(sourceVar);
    }
    else if(v.storage == StorageClass::Private || v.storage == StorageClass::Workgroup)
    {
      // private variables are allocated as globals. Similar to outputs
      ShaderVariable var;
      var.name = GetRawName(v.id);

      rdcstr sourceName = GetHumanName(v.id);

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      auto uninitialisedCallback = [](ShaderVariable &var, const Decorations &, const DataType &,
                                      uint64_t, const rdcstr &) {
        if(!var.members.empty())
          return;

        memset(&var.value, 0xcc, sizeof(var.value));
      };

      WalkVariable<ShaderVariable, true>(decorations[v.id], dataTypes[type.InnerType()], ~0U, var,
                                         rdcstr(), uninitialisedCallback);

      if(v.initializer != Id())
        AssignValue(var, active.ids[v.initializer]);

      if(v.storage == StorageClass::Private)
      {
        active.privates.push_back(var);
        pointerIDs.push_back(THREAD_POINTER(v.id, privates));
      }
      else if(v.storage == StorageClass::Workgroup)
      {
        global.workgroups.push_back(var);
        pointerIDs.push_back(GLOBAL_POINTER(v.id, workgroups));
      }

      liveGlobals.push_back(v.id);

      if(sourceName != var.name && (!m_DebugInfo.valid || m_DebugInfo.globals.contains(v.id)))
      {
        SourceVariableMapping sourceVar;
        sourceVar.name = sourceName;
        sourceVar.type = var.type;
        sourceVar.rows = RDCMAX(1U, (uint32_t)var.rows);
        sourceVar.columns = RDCMAX(1U, (uint32_t)var.columns);
        sourceVar.offset = 0;
        for(uint32_t x = 0; x < sourceVar.rows * sourceVar.columns; x++)
          sourceVar.variables.push_back(
              DebugVariableReference(DebugVariableType::Variable, var.name, x));

        ret->sourceVars.push_back(sourceVar);
      }
    }
    else
    {
      RDCERR("Unhandled type of global variable: %s", ToStr(v.storage).c_str());
    }
  }

  std::sort(liveGlobals.begin(), liveGlobals.end());

  for(uint32_t i = 0; i < workgroupSize; i++)
  {
    ThreadState &lane = workgroup[i];
    if(i != activeLaneIndex)
    {
      lane.nextInstruction = active.nextInstruction;
      lane.inputs = active.inputs;
      lane.outputs = active.outputs;
      lane.privates = active.privates;
      lane.ids = active.ids;
      // mark as inactive/helper lane
      lane.helperInvocation = true;
    }

    // now that the globals are allocated and their storage won't move, we can take pointers to them
    for(const PointerId &p : pointerIDs)
      p.Set(*this, global, lane);
  }

  // this contains all the accumulated line number information. Add in our disassembly mapping
  ret->instInfo = m_InstInfo;
  for(size_t i = 0; i < m_InstInfo.size(); i++)
  {
    auto it = instructionLines.find(instructionOffsets[m_InstInfo[i].instruction]);
    if(it != instructionLines.end())
      ret->instInfo[i].lineInfo.disassemblyLine = it->second;
    else
      ret->instInfo[i].lineInfo.disassemblyLine = 0;
  }

  if(m_DebugInfo.valid)
    FillDebugSourceVars(ret->instInfo);
  else
    FillDefaultSourceVars(ret->instInfo);

  ret->constantBlocks = global.constantBlocks;
  ret->readOnlyResources = global.readOnlyResources;
  ret->readWriteResources = global.readWriteResources;
  ret->samplers = global.samplers;
  ret->inputs = active.inputs;

  if(stage == ShaderStage::Pixel)
  {
    // apply derivatives to generate the correct inputs for the quad neighbours
    for(uint32_t q = 0; q < workgroupSize; q++)
    {
      if(q == activeLaneIndex)
        continue;

      for(size_t i = 0; i < inputIDs.size(); i++)
      {
        Id id = inputIDs[i];

        const DataType &type = dataTypes[idTypes[id]];

        // global variables should all be pointers into opaque storage
        RDCASSERT(type.type == DataType::PointerType);

        const DataType &innertype = dataTypes[type.InnerType()];

        auto derivCallback = [this, q](ShaderVariable &var, const Decorations &dec,
                                       const DataType &type, uint64_t location, const rdcstr &) {
          if(!var.members.empty())
            return;

          ApplyDerivatives(q, dec, (uint32_t)location, type, var);
        };

        WalkVariable<ShaderVariable, false>(decorations[id], innertype, ~0U, workgroup[q].inputs[i],
                                            rdcstr(), derivCallback);
      }
    }
  }

  return ret;
}

void Debugger::FillCallstack(ThreadState &thread, ShaderDebugState &state)
{
  rdcarray<Id> funcs;
  thread.FillCallstack(funcs);

  for(Id f : funcs)
  {
    if(m_DebugInfo.valid)
    {
      auto it = m_DebugInfo.funcToDebugFunc.find(f);
      if(it != m_DebugInfo.funcToDebugFunc.end())
      {
        state.callstack.push_back(m_DebugInfo.scopes[it->second].name);
        continue;
      }
    }

    state.callstack.push_back(GetHumanName(f));
  }
}

void Debugger::FillDebugSourceVars(rdcarray<InstructionSourceInfo> &instInfo)
{
  for(InstructionSourceInfo &i : instInfo)
  {
    size_t offs = instructionOffsets[i.instruction];

    const ScopeData *scope = GetScope(offs);

    if(!scope)
      continue;

    // track which mappings we've processed, so if the same variable has mappings in multiple scopes
    // we only pick the innermost.
    rdcarray<LocalMapping> processed;
    rdcarray<Id> sourceVars;

    // capture the scopes upwards (from child to parent)
    rdcarray<const ScopeData *> scopes;
    while(scope)
    {
      scopes.push_back(scope);
      // if we reach a function scope, don't go up any further.
      if(scope->type == DebugScope::Function)
        break;

      scope = scope->parent;
    }

    // Iterate over the scopes downwards (parent->child)
    for(size_t s = 0; s < scopes.size(); ++s)
    {
      scope = scopes[scopes.size() - 1 - s];
      for(size_t m = 0; m < scope->localMappings.size(); m++)
      {
        const LocalMapping &mapping = scope->localMappings[m];

        // if this mapping is past the current instruction, stop here.
        if(mapping.instIndex > i.instruction)
          break;

        // see if this mapping is superceded by a later mapping in this scope for this instruction.
        // This is a bit inefficient but simple. The alternative would be to do record
        // start and end points for each mapping and update the end points, but this is simple and
        // should be limited since it's only per-scope
        bool supercede = false;
        for(size_t n = m + 1; n < scope->localMappings.size(); n++)
        {
          const LocalMapping &laterMapping = scope->localMappings[n];

          // if this mapping is past the current instruction, stop here.
          if(laterMapping.instIndex > i.instruction)
            break;

          // if this mapping will supercede and starts later
          if(laterMapping.isSourceSupersetOf(mapping) && laterMapping.instIndex > mapping.instIndex)
          {
            supercede = true;
            break;
          }
        }

        // don't add the current mapping if it's going to be superceded by something later
        if(supercede)
          continue;

        processed.push_back(mapping);
        Id sourceVar = mapping.sourceVar;
        if(!sourceVars.contains(mapping.sourceVar))
          sourceVars.push_back(mapping.sourceVar);
      }
    }

    // Converting debug variable mappings to SourceVariableMapping is a two phase algorithm.

    // Phase One
    // For each source variable, repeatedly apply the debug variable mappings.
    // This debug variable usage is tracked in a tree-like structure built using DebugVarNode
    // elements.
    // As each mapping is applied, the new mapping can fully or partially override the
    // existing mapping. When an existing mapping is:
    //  - fully overrideen: any sub-elements of that mapping are cleared
    //    i.e. assigning a vector, array, structure
    //  - partially overriden: the existing mapping is expanded into its sub-elements which are
    //    mapped to the current mapping and then the new mapping is set to its corresponding
    //    elements i.e. y-component in a vector, member in a structure, a single array element
    // The DebugVarNode member "emitSourceVar" determines if the DebugVar mapping should be
    // converted to a source variable mapping.

    // Phase Two
    // The DebugVarNode tree is walked to find the nodes which have "emitSourceVar" set to true and
    // then those nodes are converted to SourceVariableMapping

    struct DebugVarNode
    {
      rdcarray<DebugVarNode> children;
      Id debugVar;
      rdcstr name;
      rdcstr debugVarSuffix;
      VarType type = VarType::Unknown;
      uint32_t rows = 0;
      uint32_t columns = 0;
      uint32_t debugVarComponent = 0;
      uint32_t offset = 0;
      bool emitSourceVar = false;
    };

    ::std::map<Id, DebugVarNode> roots;

    // Phase One: generate the DebugVarNode tree by repeatedly apply debug variables updating
    // existing mappings with later mappings
    for(size_t sv = 0; sv < sourceVars.size(); ++sv)
    {
      Id sourceVarId = sourceVars[sv];
      const LocalData &l = m_DebugInfo.locals[sourceVarId];

      // Convert processed mappings into a usage map
      for(size_t m = 0; m < processed.size(); ++m)
      {
        const LocalMapping &mapping = processed[m];
        if(mapping.sourceVar != sourceVarId)
          continue;

        const TypeData *typeWalk = l.type;
        DebugVarNode *usage = &roots[sourceVarId];
        if(usage->name.isEmpty())
        {
          usage->name = l.name;
          usage->rows = 1U;
          usage->columns = 1U;
        }

        // if it doesn't have indexes this is simple, set up a 1:1 map
        if(mapping.indexes.isEmpty())
        {
          uint32_t rows = 1;
          uint32_t columns = 1;
          // skip past any pointer types to get the 'real' type that we'll see
          while(typeWalk && typeWalk->baseType != Id() && typeWalk->type == VarType::GPUPointer)
            typeWalk = &m_DebugInfo.types[typeWalk->baseType];

          const uint32_t arrayDimension = typeWalk->arrayDimensions.size();
          if(arrayDimension > 0)
          {
            // walk down until we get to a scalar type, if we get there. This means arrays of
            // basic types will get the right type
            while(typeWalk && typeWalk->baseType != Id() && typeWalk->type == VarType::Unknown)
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

          usage->debugVar = mapping.debugVar;
          // Remove any child mappings : this mapping covers everything
          usage->children.clear();
          usage->emitSourceVar = true;
          usage->rows = rows;
          usage->columns = columns;
        }
        else
        {
          rdcarray<uint32_t> indexes = mapping.indexes;

          // walk any aggregate types
          while(!indexes.empty())
          {
            uint32_t idx = ~0U;
            const TypeData *childType = NULL;
            const uint32_t arrayDimension = typeWalk->arrayDimensions.size();
            if(arrayDimension > 0)
            {
              const rdcarray<uint32_t> &dims = typeWalk->arrayDimensions;
              uint32_t numIdxs = (uint32_t)indexes.size();
              childType = &m_DebugInfo.types[typeWalk->baseType];
              uint32_t childRows = 1U;
              uint32_t childColumns = 1U;
              VarType elementType = childType->type;
              uint32_t elementSize = 1;
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
                elementSize += childType->memberOffsets[childType->memberOffsets.count() - 1];
              }
              elementSize *= childRows * childColumns;
              const uint32_t countDims = RDCMIN(arrayDimension, numIdxs);
              // handle N dimensional arrays
              for(uint32_t d = 0; d < countDims; ++d)
              {
                idx = indexes[0];
                indexes.erase(0);
                uint32_t rows = dims[d];
                usage->rows = rows;
                usage->columns = 1U;
                // Expand the node if required
                if(usage->children.isEmpty())
                {
                  usage->children.resize(rows);
                  for(uint32_t x = 0; x < rows; x++)
                  {
                    usage->children[x].debugVar = usage->debugVar;
                    rdcstr suffix = StringFormat::Fmt("[%u]", x);
                    usage->children[x].debugVarSuffix = usage->debugVarSuffix + suffix;
                    usage->children[x].name = usage->name + suffix;
                    usage->children[x].type = elementType;
                    usage->children[x].rows = childRows;
                    usage->children[x].columns = childColumns;
                    usage->children[x].offset = usage->offset + x * elementSize;
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
                usage = &usage->children[idx];
                usage->type = childType->type;
                typeWalk = childType;
              }
            }
            else if(!typeWalk->structMembers.empty())
            {
              idx = indexes[0];
              indexes.erase(0);
              childType = &m_DebugInfo.types[typeWalk->structMembers[idx].second];
              uint32_t rows = typeWalk->structMembers.size();
              usage->rows = rows;
              usage->columns = 1U;
              // Expand the node if required
              if(usage->children.isEmpty())
              {
                usage->children.resize(rows);
                for(uint32_t x = 0; x < rows; x++)
                {
                  rdcstr suffix = StringFormat::Fmt(".%s", typeWalk->structMembers[x].first.c_str());
                  usage->children[x].debugVar = usage->debugVar;
                  usage->children[x].debugVarSuffix = usage->debugVarSuffix + suffix;
                  usage->children[x].name = usage->name + suffix;
                  usage->children[x].offset = usage->offset + typeWalk->memberOffsets[x];
                  uint32_t memberRows = 1U;
                  uint32_t memberColumns = 1U;
                  const TypeData *memberType = &m_DebugInfo.types[typeWalk->structMembers[x].second];
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

              usage = &usage->children[idx];
              usage->type = childType->type;
              typeWalk = childType;
            }
            else
            {
              break;
            }
          }

          const char swizzle[] = "xyzw";
          uint32_t rows = 1U;
          uint32_t columns = 1U;
          size_t countRemainingIndexes = indexes.size();
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
            usage->rows = rows;
            usage->columns = columns;

            if((countRemainingIndexes == 2) || (countRemainingIndexes == 1))
            {
              if(usage->children.isEmpty())
              {
                // Matrices are stored as [row][col]
                usage->children.resize(rows);
                for(uint32_t r = 0; r < rows; ++r)
                {
                  usage->children[r].emitSourceVar = false;
                  usage->children[r].name = usage->name + StringFormat::Fmt(".row%u", r);
                  usage->children[r].type = scalar.type;
                  usage->children[r].debugVar = usage->debugVar;
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
                    usage->children[r].children[c].debugVar = usage->debugVar;
                    usage->children[r].children[c].debugVarComponent = r;
                    usage->children[r].children[c].rows = 1U;
                    usage->children[r].children[c].columns = 1U;
                    usage->children[r].children[c].offset = usage->children[r].offset + c;
                  }
                }
              }
              RDCASSERTEQUAL(usage->children.size(), rows);

              // two remaining indices selects a scalar within the matrix
              if(countRemainingIndexes == 2)
              {
                uint32_t row, col;

                if(typeWalk->colMajorMat)
                {
                  col = indexes[0];
                  row = indexes[1];
                }
                else
                {
                  row = indexes[0];
                  col = indexes[1];
                }
                RDCASSERT(row < rows, row, rows);
                RDCASSERT(col < columns, col, columns);

                RDCASSERTEQUAL(usage->children[row].children.size(), columns);
                usage->children[row].children[col].emitSourceVar =
                    !usage->children[row].emitSourceVar;
                usage->children[row].children[col].debugVar = mapping.debugVar;
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
              else
              {
                if(typeWalk->colMajorMat)
                {
                  uint32_t col = indexes[0];
                  RDCASSERT(col < columns, col, columns);
                  // one remaining index selects a column within the matrix.
                  // source vars are displayed as row-major, need <rows> mappings
                  for(uint32_t r = 0; r < rows; ++r)
                  {
                    RDCASSERTEQUAL(usage->children[r].children.size(), columns);
                    usage->children[r].children[col].emitSourceVar =
                        !usage->children[r].emitSourceVar;
                    usage->children[r].children[col].debugVar = mapping.debugVar;
                    usage->children[r].children[col].debugVarComponent = r;
                  }
                }
                else
                {
                  uint32_t row = indexes[0];
                  RDCASSERT(row < rows, row, rows);
                  RDCASSERTEQUAL(usage->children.size(), rows);
                  RDCASSERTEQUAL(usage->children[row].children.size(), columns);
                  // one remaining index selects a row within the matrix.
                  // source vars are displayed as row-major, need <rows> mappings
                  for(uint32_t c = 0; c < columns; ++c)
                  {
                    usage->children[row].children[c].emitSourceVar =
                        !usage->children[row].emitSourceVar;
                    usage->children[row].children[c].debugVar = mapping.debugVar;
                    usage->children[row].children[c].debugVarComponent = c;
                  }
                }
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
              RDCASSERTEQUAL(countRemainingIndexes, 0);
              // Remove mappings : this mapping covers everything
              usage->debugVar = mapping.debugVar;
              usage->children.clear();
              usage->emitSourceVar = true;
              usage->debugVarSuffix.clear();
            }
          }
          else if(typeWalk->vecSize != 0)
          {
            const TypeData &scalar = m_DebugInfo.types[typeWalk->baseType];
            columns = RDCMAX(1U, typeWalk->vecSize);

            usage->type = scalar.type;

            usage->rows = 1U;
            usage->columns = columns;

            // remaining index selects a scalar within the vector
            if(countRemainingIndexes == 1)
            {
              if(usage->children.isEmpty())
              {
                usage->children.resize(columns);
                for(uint32_t x = 0; x < columns; ++x)
                {
                  usage->children[x].emitSourceVar = usage->emitSourceVar;
                  usage->children[x].name =
                      usage->name + StringFormat::Fmt(".%c", swizzle[RDCMIN(x, 3U)]);
                  usage->children[x].type = scalar.type;
                  usage->children[x].debugVar = usage->debugVar;
                  usage->children[x].debugVarComponent = x;
                  usage->children[x].rows = 1U;
                  usage->children[x].columns = 1U;
                  usage->children[x].offset = usage->offset + x;
                }
                usage->emitSourceVar = false;
              }
              uint32_t col = indexes[0];
              RDCASSERT(col < columns, col, columns);
              RDCASSERTEQUAL(usage->children.size(), columns);
              usage->children[col].debugVar = mapping.debugVar;
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
              RDCASSERTEQUAL(countRemainingIndexes, 0);
              // Remove mappings : this mapping covers everything
              usage->debugVar = mapping.debugVar;
              usage->children.clear();
              usage->emitSourceVar = true;
              usage->debugVarSuffix.clear();
            }
          }
          else
          {
            // walk down until we get to a scalar type, if we get there. This means arrays of
            // basic types will get the right type
            while(typeWalk && typeWalk->baseType != Id() && typeWalk->type == VarType::Unknown)
              typeWalk = &m_DebugInfo.types[typeWalk->baseType];

            usage->type = typeWalk->type;
            usage->debugVar = mapping.debugVar;
            usage->debugVarComponent = 0;
            usage->rows = 1U;
            usage->columns = 1U;
            usage->emitSourceVar = true;
            usage->children.clear();
            usage->debugVarSuffix.clear();
          }
        }
      }
    }

    // Phase Two: walk the DebugVarNode tree and convert "emitSourceVar = true" nodes to a SourceVariableMapping
    for(size_t sv = 0; sv < sourceVars.size(); ++sv)
    {
      Id sourceVarId = sourceVars[sv];
      DebugVarNode *usage = &roots[sourceVarId];
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
                DebugVariableType::Variable, GetRawName(n->debugVar) + n->debugVarSuffix, c));
          }
        }
        else
        {
          RDCASSERTEQUAL(n->rows * n->columns, (uint32_t)n->children.count());
          for(int32_t c = 0; c < n->children.count(); ++c)
            sourceVar.variables.push_back(DebugVariableReference(
                DebugVariableType::Variable,
                GetRawName(n->children[c].debugVar) + n->children[c].debugVarSuffix,
                n->children[c].debugVarComponent));
        }
        i.sourceVars.push_back(sourceVar);
      }
    }
  }
}

void Debugger::FillDefaultSourceVars(rdcarray<InstructionSourceInfo> &instInfo)
{
  rdcarray<SourceVariableMapping> sourceVars;
  rdcarray<Id> debugVars;

  for(InstructionSourceInfo &i : instInfo)
  {
    // the source vars for this instruction are whatever we have currently, because when we're
    // looking up the source vars for instruction X we are effectively talking abotu the state just
    // before X executes, not just after.
    i.sourceVars = sourceVars;

    // now update the sourcevars for after this instruction executed

    size_t offs = instructionOffsets[i.instruction];

    Iter it(m_SPIRV, offs);

    OpDecoder opdata(it);

    Id id = opdata.result;

    // stores can bring their pointer into being, if it's the first write.
    if(opdata.op == Op::Store)
      id = OpStore(it).pointer;

    // if this is the offset where the id's live range begins, try to add the source name for it if
    // one exists.
    if(id != Id() && idLiveRange[id].first == offs)
    {
      rdcstr name;

      auto dyn = dynamicNames.find(id);
      if(dyn != dynamicNames.end())
        name = dyn->second;
      else
        name = strings[id];

      if(!name.empty())
      {
        SourceVariableMapping sourceVar;

        const DataType *type = &GetTypeForId(id);

        while(type->type == DataType::PointerType || type->type == DataType::ArrayType)
          type = &GetType(type->InnerType());

        sourceVar.name = name;
        sourceVar.offset = 0;
        if(type->type == DataType::MatrixType || type->type == DataType::VectorType ||
           type->type == DataType::ScalarType)
          sourceVar.type = type->scalar().Type();
        else if(type->type == DataType::StructType)
          sourceVar.type = VarType::Struct;
        else if(type->type == DataType::ImageType || type->type == DataType::SampledImageType ||
                type->type == DataType::SamplerType)
          sourceVar.type = VarType::ReadOnlyResource;
        sourceVar.rows = RDCMAX(1U, (uint32_t)type->matrix().count);
        sourceVar.columns = RDCMAX(1U, (uint32_t)type->vector().count);
        rdcstr rawName = GetRawName(id);
        for(uint32_t x = 0; x < sourceVar.rows * sourceVar.columns; x++)
          sourceVar.variables.push_back(
              DebugVariableReference(DebugVariableType::Variable, rawName, x));

        sourceVars.push_back(sourceVar);
        debugVars.push_back(id);
      }
    }

    // see which vars have expired
    for(size_t d = 0; d < debugVars.size();)
    {
      if(offs > idLiveRange[debugVars[d]].second)
      {
        sourceVars.erase(d);
        debugVars.erase(d);
        continue;
      }

      d++;
    }

    // all variables/IDs are function-local
    if(opdata.op == Op::FunctionEnd)
    {
      sourceVars.clear();
      debugVars.clear();
    }
  }
}

rdcarray<ShaderDebugState> Debugger::ContinueDebug()
{
  ThreadState &active = GetActiveLane();

  rdcarray<ShaderDebugState> ret;

  // initialise the first ShaderDebugState if we haven't stepped yet
  if(steps == 0)
  {
    ShaderDebugState initial;

    // we should be sitting at the entry point function prologue, step forward into the first block
    // and past any function-local variable declarations
    for(size_t lane = 0; lane < workgroup.size(); lane++)
    {
      ThreadState &thread = workgroup[lane];

      if(lane == activeLaneIndex)
      {
        thread.EnterEntryPoint(&initial);
        FillCallstack(thread, initial);
        initial.nextInstruction = thread.nextInstruction;
      }
      else
      {
        thread.EnterEntryPoint(NULL);
      }
    }

    // globals won't be filled out by entering the entry point, ensure their change is registered.
    for(const Id &v : liveGlobals)
      initial.changes.push_back({ShaderVariable(), GetPointerValue(active.ids[v])});

    if(m_DebugInfo.valid)
    {
      // debug info can refer to constants for source variable values. Add an initial change for any
      // that are so referenced
      for(const Id &v : m_DebugInfo.constants)
        initial.changes.push_back({ShaderVariable(), GetPointerValue(active.ids[v])});
    }

    ret.push_back(std::move(initial));

    steps++;
  }

  // if we've finished, return an empty set to signify that
  if(active.Finished())
    return ret;

  rdcarray<bool> activeMask;

  // continue stepping until we have 100 target steps completed in a chunk. This may involve doing
  // more steps if our target thread is inactive
  for(int stepEnd = steps + 100; steps < stepEnd;)
  {
    global.clock++;

    if(active.Finished())
      break;

    // calculate the current mask of which threads are active
    CalcActiveMask(activeMask);

    // step all active members of the workgroup
    for(size_t lane = 0; lane < workgroup.size(); lane++)
    {
      ThreadState &thread = workgroup[lane];

      if(activeMask[lane])
      {
        if(thread.nextInstruction >= instructionOffsets.size())
        {
          if(lane == activeLaneIndex)
            ret.emplace_back();

          continue;
        }

        if(lane == activeLaneIndex)
        {
          ShaderDebugState state;

          size_t instOffs = instructionOffsets[thread.nextInstruction];

          // see if we're retiring any IDs at this state
          for(size_t l = 0; l < thread.live.size();)
          {
            Id id = thread.live[l];
            if(idLiveRange[id].second < instOffs)
            {
              thread.live.erase(l);
              ShaderVariableChange change;
              change.before = GetPointerValue(thread.ids[id]);
              state.changes.push_back(change);

              continue;
            }

            l++;
          }

          uint32_t funcRet = ~0U;
          size_t prevStackSize = thread.callstack.size();

          if(!thread.callstack.empty())
            funcRet = thread.callstack.back()->funcCallInstruction;

          state.stepIndex = steps;
          thread.StepNext(&state, workgroup);

          if(thread.callstack.size() > prevStackSize)
            instOffs =
                instructionOffsets[GetInstructionForFunction(thread.callstack.back()->function)];

          else if(thread.callstack.size() < prevStackSize && funcRet != ~0U)
            instOffs = instructionOffsets[funcRet];

          FillCallstack(thread, state);

          if(m_DebugInfo.valid)
          {
            size_t endOffs = instructionOffsets[thread.nextInstruction - 1];

            // append any inlined functions to the top of the stack
            InlineData *inlined = m_DebugInfo.lineInline[endOffs];

            size_t insertPoint = state.callstack.size();

            // start with the current scope, it refers to the *inlined* function
            if(inlined)
            {
              const ScopeData *scope = GetScope(endOffs);
              // find the function parent of the current scope
              while(scope && scope->parent && scope->type == DebugScope::Block)
                scope = scope->parent;

              state.callstack.insert(insertPoint, scope->name);
            }

            // if this instruction has no scope, don't give it a callstack
            if(GetScope(endOffs) == NULL)
            {
              state.callstack.clear();
            }

            // move to the next inline up on our inline stack. If we reach an actual function
            // call, this parent will be NULL as there was no more inlining - the final scope will
            // refer to the real function which is already on our stack
            while(inlined && inlined->parent)
            {
              const ScopeData *scope = inlined->scope;
              // find the function parent of the current scope
              while(scope && scope->parent && scope->type == DebugScope::Block)
                scope = scope->parent;

              state.callstack.insert(insertPoint, scope->name);

              inlined = inlined->parent;
            }
          }

          ret.push_back(std::move(state));

          steps++;
        }
        else
        {
          thread.StepNext(NULL, workgroup);
        }
      }
    }
  }

  return ret;
}

ShaderVariable Debugger::MakeTypedPointer(uint64_t value, const DataType &type) const
{
  rdcspv::Id typeId = type.InnerType();
  ShaderVariable var;
  var.rows = var.columns = 1;
  var.type = VarType::GPUPointer;
  var.SetTypedPointer(value, apiWrapper ? apiWrapper->GetShaderID() : ResourceId(),
                      idToPointerType[typeId]);

  const Decorations &dec = decorations[type.id];
  if(dec.flags & Decorations::HasArrayStride)
  {
    uint32_t arrayStride = dec.arrayStride;
    setArrayStride(var, arrayStride);
  }
  return var;
}

ShaderVariable Debugger::MakePointerVariable(Id id, const ShaderVariable *v, uint8_t scalar0,
                                             uint8_t scalar1) const
{
  ShaderVariable var;
  var.rows = var.columns = 1;
  var.type = VarType::GPUPointer;
  var.name = GetRawName(id);
  var.SetTypedPointer(0, ResourceId(), OpaquePointerTypeID);
  setPointer(var, v);
  setScalars(var, scalar0, scalar1);
  setBaseId(var, id);
  return var;
}

ShaderVariable Debugger::MakeCompositePointer(const ShaderVariable &base, Id id,
                                              rdcarray<uint32_t> &indices)
{
  const ShaderVariable *leaf = &base;

  bool physicalPointer = IsPhysicalPointer(base);
  bool isArray = false;

  if(!physicalPointer)
  {
    // if the base is a plain value, we just start walking down the chain. If the base is a pointer
    // though, we want to step down the chain in the underlying storage, so dereference first.
    if(base.type == VarType::GPUPointer)
      leaf = getPointer(base);
  }

  // if this is an arrayed opaque binding, the first index is a 'virtual' array index into the
  // binding.
  // We only take this if this is the FIRST dereference from the global pointer.
  // If the SPIR-V does something like structType *_1234 =
  if((leaf->type == VarType::ReadWriteResource || leaf->type == VarType::ReadOnlyResource ||
      leaf->type == VarType::Sampler) &&
     checkPointerFlags(*leaf, PointerFlags::GlobalArrayBinding) &&
     getBufferTypeId(base) == rdcspv::Id())
  {
    isArray = true;
  }

  if((leaf->type == VarType::ReadWriteResource && checkPointerFlags(*leaf, PointerFlags::SSBO)) ||
     physicalPointer)
  {
    ShaderVariable ret;
    uint64_t byteOffset = 0;
    const DataType *type = NULL;

    if(physicalPointer)
    {
      // work purely with the pointer itself. All we're going to do effectively is move the address
      // and set the sub-type pointed to so that we know how to dereference it later
      ret = *leaf;
      // if this hasn't been dereferenced yet we should have a valid pointer type ID for a physical
      // pointer, which we can then use to get the base ID (and there will be no buffer type ID
      // below). If not, we rely on the buffer type ID.
      if(!checkPointerFlags(ret, PointerFlags::DereferencedPhysical))
      {
        rdcspv::Id typeId = pointerTypeToId[ret.GetPointer().pointerTypeID];
        RDCASSERT(typeId != rdcspv::Id());
        type = &dataTypes[typeId];
      }
    }
    else
    {
      ret = MakePointerVariable(id, leaf);

      byteOffset = getByteOffset(base);
      type = &dataTypes[idTypes[id]];

      RDCASSERT(type->type == DataType::PointerType);
      type = &dataTypes[type->InnerType()];
    }

    setMatrixStride(ret, getMatrixStride(base));
    setPointerFlags(ret, getPointerFlags(base));

    rdcspv::Id typeId = getBufferTypeId(base);

    if(typeId != rdcspv::Id())
      type = &dataTypes[typeId];

    // first walk any aggregate types
    size_t i = 0;

    // if it's an array, consume the array index first
    if(isArray)
    {
      setBindArrayIndex(ret, indices[i++]);
      type = &dataTypes[type->InnerType()];
    }
    else
    {
      setBindArrayIndex(ret, getBindArrayIndex(base));
    }

    Decorations curDecorations = decorations[type->id];

    uint32_t arrayStride = 0;

    while(i < indices.size() &&
          (type->type == DataType::ArrayType || type->type == DataType::StructType))
    {
      if(type->type == DataType::ArrayType)
      {
        // look up the array stride
        const Decorations &dec = decorations[type->id];
        RDCASSERT(dec.flags & Decorations::HasArrayStride);

        // offset increases by index * arrayStride
        arrayStride = dec.arrayStride;
        byteOffset += indices[i] * arrayStride;

        // new type is the inner type
        type = &dataTypes[type->InnerType()];
      }
      else
      {
        // otherwise it's a struct member
        const DataType::Child &child = type->children[indices[i]];

        // offset increases by member offset
        RDCASSERT(child.decorations.flags & Decorations::HasOffset);
        byteOffset += child.decorations.offset;

        // new type is the child type
        type = &dataTypes[child.type];
        curDecorations = child.decorations;
      }
      i++;
    }

    size_t remaining = indices.size() - i;
    if(remaining == 2)
    {
      // pointer to a scalar in a matrix. indices[i] is column, indices[i + 1] is row
      RDCASSERT(curDecorations.flags & Decorations::HasMatrixStride);

      // type is the resulting scalar (first inner does matrix->colun type, second does column
      // type->scalar type)
      type = &dataTypes[dataTypes[type->InnerType()].InnerType()];

      if(curDecorations.flags & Decorations::RowMajor)
      {
        byteOffset +=
            curDecorations.matrixStride * indices[i + 1] + indices[i] * (type->scalar().width / 8);
      }
      else
      {
        byteOffset +=
            curDecorations.matrixStride * indices[i] + indices[i + 1] * (type->scalar().width / 8);
      }
    }
    else if(remaining == 1)
    {
      if(type->type == DataType::VectorType)
      {
        // pointer to a scalar in a vector.

        // type is the resulting scalar (first inner does matrix->colun type, second does column
        // type->scalar type)
        type = &dataTypes[type->InnerType()];

        byteOffset += indices[i] * (type->scalar().width / 8);
      }
      else
      {
        // pointer to a column in a matrix
        RDCASSERT(curDecorations.flags & Decorations::HasMatrixStride);

        // type is the resulting vector
        type = &dataTypes[type->InnerType()];

        if(curDecorations.flags & Decorations::RowMajor)
        {
          byteOffset += indices[i] * (type->scalar().width / 8);
        }
        else
        {
          byteOffset += curDecorations.matrixStride * indices[i];
        }
      }
    }

    if(curDecorations.flags & Decorations::HasMatrixStride)
      setMatrixStride(ret, curDecorations.matrixStride);

    if(curDecorations.flags & Decorations::RowMajor)
      enablePointerFlags(ret, PointerFlags::RowMajorMatrix);
    else if(curDecorations.flags & Decorations::ColMajor)
      disablePointerFlags(ret, PointerFlags::RowMajorMatrix);

    setBufferTypeId(ret, type->id);
    setArrayStride(ret, arrayStride);
    if(physicalPointer)
    {
      PointerVal ptrval = ret.GetPointer();
      // we use the opaque type ID to ensure we don't accidentally leak the wrong type ID.
      // we check where the pointer is dereferenced to use the physical address instead of the inner
      // binding
      ret.SetTypedPointer(ptrval.pointer + byteOffset, ptrval.shader, OpaquePointerTypeID);
    }
    else
    {
      setByteOffset(ret, byteOffset);
    }
    // this flag is only used for physical pointers, to indicate that it's been dereferenced and
    // the pointer type should be fetched from our ID above and it returned as a plain value, rather
    // than showing the pointer 'natively'. This is because we may not have a pointer type to
    // reference if e.g. the only pointer type registered is struct foo { } and we've dereferenced
    // into inner struct bar { }
    //
    // effectively physical pointers currently decay into opaque pointers after any access chain
    // (but opaque that still uses an address, not that uses a global pointer inner value as in
    // other opaque pointers)
    if(physicalPointer)
      enablePointerFlags(ret, PointerFlags::DereferencedPhysical);
    return ret;
  }

  // first walk any struct member/array indices
  size_t i = 0;
  if(isArray)
    i++;
  while(i < indices.size() && !leaf->members.empty())
  {
    uint32_t idx = indices[i++];
    if(idx >= leaf->members.size())
    {
      apiWrapper->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Index %u invalid at leaf %s. Clamping to %zu", idx, leaf->name.c_str(),
                            leaf->members.size() - 1));
      idx = uint32_t(leaf->members.size() - 1);
    }
    leaf = &leaf->members[idx];
  }

  // apply any remaining scalar selectors
  uint8_t scalar0 = 0xff, scalar1 = 0xff;

  size_t remaining = indices.size() - i;

  if(remaining > 2)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Too many indices left (%zu) at leaf %s. Ignoring all but last two",
                          remaining, leaf->name.c_str()));
    i = indices.size() - 2;
  }

  if(remaining == 2)
  {
    scalar0 = indices[i] & 0xff;
    scalar1 = indices[i + 1] & 0xff;
  }
  else if(remaining == 1)
  {
    scalar0 = indices[i] & 0xff;
  }

  ShaderVariable ret = MakePointerVariable(id, leaf, scalar0, scalar1);

  if(isArray)
    setBindArrayIndex(ret, indices[0]);

  return ret;
}

uint64_t Debugger::GetPointerByteOffset(const ShaderVariable &ptr) const
{
  return getByteOffset(ptr);
}

DebugAPIWrapper::TextureType Debugger::GetTextureType(const ShaderVariable &img) const
{
  return getTextureType(img);
}

ShaderVariable Debugger::GetPointerValue(const ShaderVariable &ptr) const
{
  // opaque pointers display as their inner value
  if(IsOpaquePointer(ptr))
  {
    const ShaderVariable *inner = getPointer(ptr);
    ShaderVariable ret = *inner;
    ret.name = ptr.name;
    // inherit any array index from the pointer
    ShaderBindIndex bind = ret.GetBindIndex();
    bind.arrayElement = getBindArrayIndex(ptr);
    ret.SetBindIndex(bind);
    return ret;
  }
  // physical pointers which haven't been dereferenced are returned as-is, they're ready for display
  else if(IsPhysicalPointer(ptr) && !checkPointerFlags(ptr, PointerFlags::DereferencedPhysical))
  {
    return ptr;
  }

  // every other kind of pointer displays as its contents
  return ReadFromPointer(ptr);
}

ShaderVariable Debugger::ReadFromPointer(const ShaderVariable &ptr) const
{
  if(ptr.type != VarType::GPUPointer)
    return ptr;

  ShaderVariable ret;
  // values for setting up pointer reads, either from a physical pointer or from an opaque pointer
  rdcspv::Id typeId;
  Decorations parentDecorations;
  uint64_t baseAddress;
  ShaderBindIndex bind;
  uint64_t byteOffset = 0;
  std::function<void(uint64_t offset, uint64_t size, void *dst)> pointerReadCallback;
  if(IsPhysicalPointer(ptr))
  {
    if(checkPointerFlags(ptr, PointerFlags::DereferencedPhysical))
      typeId = getBufferTypeId(ptr);
    else
      typeId = pointerTypeToId[ptr.GetPointer().pointerTypeID];

    RDCASSERT(typeId != rdcspv::Id());

    parentDecorations = decorations[typeId];
    uint32_t varMatrixStride = getMatrixStride(ptr);

    if(varMatrixStride != 0)
    {
      if(checkPointerFlags(ptr, PointerFlags::RowMajorMatrix))
        parentDecorations.flags = Decorations::RowMajor;
      else
        parentDecorations.flags = Decorations::ColMajor;
      parentDecorations.flags =
          Decorations::Flags(parentDecorations.flags | Decorations::HasMatrixStride);
      parentDecorations.matrixStride = varMatrixStride;
    }
    baseAddress = ptr.GetPointer().pointer;
    pointerReadCallback = [this, baseAddress](uint64_t offset, uint64_t size, void *dst) {
      apiWrapper->ReadAddress(baseAddress + offset, size, dst);
    };
  }
  else
  {
    const ShaderVariable *inner = getPointer(ptr);
    if(inner->type == VarType::ReadWriteResource && checkPointerFlags(*inner, PointerFlags::SSBO))
    {
      typeId = getBufferTypeId(ptr);
      byteOffset = getByteOffset(ptr);
      bind = inner->GetBindIndex();
      bind.arrayElement = getBindArrayIndex(ptr);
      uint32_t varMatrixStride = getMatrixStride(ptr);
      if(varMatrixStride != 0)
      {
        if(checkPointerFlags(ptr, PointerFlags::RowMajorMatrix))
          parentDecorations.flags = Decorations::RowMajor;
        else
          parentDecorations.flags = Decorations::ColMajor;
        parentDecorations.flags =
            Decorations::Flags(parentDecorations.flags | Decorations::HasMatrixStride);
        parentDecorations.matrixStride = varMatrixStride;
      }
      pointerReadCallback = [this, bind](uint64_t offset, uint64_t size, void *dst) {
        apiWrapper->ReadBufferValue(bind, offset, size, dst);
      };
    }
  }

  if(pointerReadCallback)
  {
    auto readCallback = [this, pointerReadCallback](ShaderVariable &var, const Decorations &dec,
                                                    const DataType &type, uint64_t offset,
                                                    const rdcstr &) {
      // ignore any callbacks we get on the way up for structs/arrays, we don't need it we only read
      // or write at primitive level
      if(!var.members.empty())
        return;

      bool rowMajor = (dec.flags & Decorations::RowMajor) != 0;
      uint32_t matrixStride = dec.matrixStride;

      if(type.type == DataType::MatrixType)
      {
        RDCASSERT(matrixStride != 0);

        if(rowMajor)
        {
          for(uint8_t r = 0; r < var.rows; r++)
          {
            pointerReadCallback(offset + r * matrixStride, VarTypeByteSize(var.type) * var.columns,
                                VarElemPointer(var, r * var.columns));
          }
        }
        else
        {
          ShaderVariable tmp;
          tmp.type = var.type;

          // read column-wise
          for(uint8_t c = 0; c < var.columns; c++)
          {
            pointerReadCallback(offset + c * matrixStride, VarTypeByteSize(var.type) * var.rows,
                                VarElemPointer(tmp, c * var.rows));
          }

          // transpose into our row major storage
          for(uint8_t r = 0; r < var.rows; r++)
            for(uint8_t c = 0; c < var.columns; c++)
              copyComp(var, r * var.columns + c, tmp, c * var.rows + r);
        }
      }
      else if(type.type == DataType::VectorType)
      {
        if(!rowMajor)
        {
          // we can read a vector at a time if the matrix is column major
          pointerReadCallback(offset, VarTypeByteSize(var.type) * var.columns,
                              VarElemPointer(var, 0));
        }
        else
        {
          for(uint8_t c = 0; c < var.columns; c++)
          {
            pointerReadCallback(offset + c * matrixStride, VarTypeByteSize(var.type),
                                VarElemPointer(var, c));
          }
        }
      }
      else if(type.type == DataType::ScalarType || type.type == DataType::PointerType)
      {
        pointerReadCallback(offset, VarTypeByteSize(var.type), VarElemPointer(var, 0));
        if(type.type == DataType::PointerType)
        {
          auto it = idToPointerType.find(type.InnerType());
          if(it != idToPointerType.end())
          {
            var.SetTypedPointer(var.value.u64v[0], this->apiWrapper->GetShaderID(), it->second);
          }
          else
          {
            var.SetTypedPointer(var.value.u64v[0], ResourceId(), OpaquePointerTypeID);
            enablePointerFlags(var, PointerFlags::DereferencedPhysical);
            setMatrixStride(var, matrixStride);
            setBufferTypeId(var, type.InnerType());
          }
        }
      }
    };

    WalkVariable<ShaderVariable, true>(parentDecorations, dataTypes[typeId], byteOffset, ret,
                                       rdcstr(), readCallback);

    ret.name = ptr.name;
    return ret;
  }

  // this is the case of 'reading' from a pointer where the data is entirely contained within the
  // inner pointed variable. Either opaque sampler/image etc which is just the binding, or a
  // cbuffer pointer which was already evaluated
  const ShaderVariable *inner = getPointer(ptr);

  ret = *inner;
  ret.name = ptr.name;

  if(inner->type == VarType::ReadOnlyResource || inner->type == VarType::ReadWriteResource ||
     inner->type == VarType::Sampler)
  {
    bind = ret.GetBindIndex();
    bind.arrayElement = getBindArrayIndex(ptr);
    ret.SetBindIndex(bind);
  }

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint8_t scalar0 = 0, scalar1 = 0;
  rdctie(scalar0, scalar1) = getScalars(ptr);

  ShaderVariable tmp = ret;

  if(ret.rows > 1)
  {
    // matrix case
    ClampScalars(apiWrapper, ret, scalar0, scalar1);

    if(scalar0 != 0xff && scalar1 != 0xff)
    {
      // two indices - selecting a scalar. scalar0 is the first index in the chain so it chooses
      // column
      copyComp(ret, 0, tmp, scalar1 * ret.columns + scalar0);

      // it's a scalar now, even if it was a matrix before
      ret.rows = ret.columns = 1;
    }
    else if(scalar0 != 0xff)
    {
      // one index, selecting a column
      for(uint32_t row = 0; row < ret.rows; row++)
        copyComp(ret, row, tmp, row * ret.columns + scalar0);

      // it's a vector now, even if it was a matrix before
      ret.rows = 1;
    }
  }
  else
  {
    ClampScalars(apiWrapper, ret, scalar0);

    // vector case, selecting a scalar (if anything)
    if(scalar0 != 0xff)
    {
      copyComp(ret, 0, tmp, scalar0);

      // it's a scalar now, even if it was a matrix before
      ret.columns = 1;
    }
  }

  return ret;
}

Id Debugger::GetPointerBaseId(const ShaderVariable &ptr) const
{
  RDCASSERT(ptr.type == VarType::GPUPointer);

  // we stored the base ID so that it's always available regardless of access chains
  return getBaseId(ptr);
}

uint32_t Debugger::GetPointerArrayStride(const ShaderVariable &ptr) const
{
  RDCASSERT(ptr.type == VarType::GPUPointer);
  return getArrayStride(ptr);
}

bool Debugger::IsOpaquePointer(const ShaderVariable &ptr) const
{
  if(ptr.type != VarType::GPUPointer)
    return false;

  if(IsPhysicalPointer(ptr))
    return false;

  const ShaderVariable *inner = getPointer(ptr);
  return inner->type == VarType::ReadOnlyResource || inner->type == VarType::Sampler ||
         inner->type == VarType::ReadWriteResource;
}

bool Debugger::IsPhysicalPointer(const ShaderVariable &ptr) const
{
  if(ptr.type == VarType::GPUPointer)
  {
    // non-dereferenced physical pointer
    if(ptr.GetPointer().pointerTypeID != OpaquePointerTypeID)
      return true;
    // dereferenced physical pointer
    if(checkPointerFlags(ptr, PointerFlags::DereferencedPhysical))
      return true;
  }
  return false;
}

bool Debugger::ArePointersAndEqual(const ShaderVariable &a, const ShaderVariable &b) const
{
  // we can do a pointer comparison by checking the values, since we store all pointer-related
  // data in there
  if(a.type == VarType::GPUPointer && b.type == VarType::GPUPointer)
    return memcmp(&a.value, &b.value, sizeof(ShaderValue)) == 0;

  return false;
}

void Debugger::WriteThroughPointer(ShaderVariable &ptr, const ShaderVariable &val)
{
  // values for setting up pointer reads, either from a physical pointer or from an opaque pointer
  rdcspv::Id typeId;
  Decorations parentDecorations;
  uint64_t baseAddress;
  ShaderBindIndex bind;
  uint64_t byteOffset = 0;
  std::function<void(uint64_t offset, uint64_t size, const void *src)> pointerWriteCallback;

  if(IsPhysicalPointer(ptr))
  {
    if(checkPointerFlags(ptr, PointerFlags::DereferencedPhysical))
      typeId = getBufferTypeId(ptr);
    else
      typeId = pointerTypeToId[ptr.GetPointer().pointerTypeID];

    RDCASSERT(typeId != rdcspv::Id());
    parentDecorations = decorations[typeId];
    uint32_t varMatrixStride = getMatrixStride(ptr);
    if(varMatrixStride != 0)
    {
      if(checkPointerFlags(ptr, PointerFlags::RowMajorMatrix))
        parentDecorations.flags = Decorations::RowMajor;
      else
        parentDecorations.flags = Decorations::ColMajor;
      parentDecorations.flags =
          Decorations::Flags(parentDecorations.flags | Decorations::HasMatrixStride);
      parentDecorations.matrixStride = varMatrixStride;
    }
    baseAddress = ptr.GetPointer().pointer;
    pointerWriteCallback = [this, baseAddress](uint64_t offset, uint64_t size, const void *src) {
      apiWrapper->WriteAddress(baseAddress + offset, size, src);
    };
  }
  else
  {
    const ShaderVariable *inner = getPointer(ptr);
    if(inner->type == VarType::ReadWriteResource && checkPointerFlags(*inner, PointerFlags::SSBO))
    {
      typeId = getBufferTypeId(ptr);
      byteOffset = getByteOffset(ptr);
      bind = inner->GetBindIndex();
      bind.arrayElement = getBindArrayIndex(ptr);
      uint32_t varMatrixStride = getMatrixStride(ptr);
      if(varMatrixStride != 0)
      {
        if(checkPointerFlags(ptr, PointerFlags::RowMajorMatrix))
          parentDecorations.flags = Decorations::RowMajor;
        else
          parentDecorations.flags = Decorations::ColMajor;
        parentDecorations.flags =
            Decorations::Flags(parentDecorations.flags | Decorations::HasMatrixStride);
        parentDecorations.matrixStride = varMatrixStride;
      }
      pointerWriteCallback = [this, bind](uint64_t offset, uint64_t size, const void *src) {
        apiWrapper->WriteBufferValue(bind, offset, size, src);
      };
    }
  }

  if(pointerWriteCallback)
  {
    auto writeCallback = [pointerWriteCallback](const ShaderVariable &var, const Decorations &dec,
                                                const DataType &type, uint64_t offset,
                                                const rdcstr &) {
      // ignore any callbacks we get on the way up for structs/arrays, we don't need it we only
      // read or write at primitive level
      if(!var.members.empty())
        return;

      bool rowMajor = (dec.flags & Decorations::RowMajor) != 0;
      uint32_t matrixStride = dec.matrixStride;

      if(type.type == DataType::MatrixType)
      {
        RDCASSERT(matrixStride != 0);

        if(rowMajor)
        {
          for(uint8_t r = 0; r < var.rows; r++)
          {
            pointerWriteCallback(offset + r * matrixStride, VarTypeByteSize(var.type) * var.columns,
                                 VarElemPointer(var, r * var.columns));
          }
        }
        else
        {
          ShaderVariable tmp;
          tmp.type = var.type;

          // transpose from our row major storage
          for(uint8_t r = 0; r < var.rows; r++)
            for(uint8_t c = 0; c < var.columns; c++)
              copyComp(tmp, c * var.rows + r, var, r * var.columns + c);

          // write column-wise
          for(uint8_t c = 0; c < var.columns; c++)
          {
            pointerWriteCallback(offset + c * matrixStride, VarTypeByteSize(var.type) * var.rows,
                                 VarElemPointer(tmp, c * var.rows));
          }
        }
      }
      else if(type.type == DataType::VectorType)
      {
        if(!rowMajor)
        {
          // we can write a vector at a time if the matrix is column major
          pointerWriteCallback(offset, VarTypeByteSize(var.type) * var.columns,
                               VarElemPointer(var, 0));
        }
        else
        {
          for(uint8_t c = 0; c < var.columns; c++)
            pointerWriteCallback(offset + c * matrixStride, VarTypeByteSize(var.type),
                                 VarElemPointer(var, c));
        }
      }
      else if(type.type == DataType::ScalarType || type.type == DataType::PointerType)
      {
        pointerWriteCallback(offset, VarTypeByteSize(var.type), VarElemPointer(var, 0));
      }
    };

    WalkVariable<const ShaderVariable, false>(parentDecorations, dataTypes[typeId], byteOffset, val,
                                              rdcstr(), writeCallback);

    return;
  }

  ShaderVariable *storage = getPointer(ptr);

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint8_t scalar0 = 0, scalar1 = 0;
  rdctie(scalar0, scalar1) = getScalars(ptr);

  // in the common case we don't have scalar selectors. In this case just assign the value
  if(scalar0 == 0xff && scalar1 == 0xff)
  {
    AssignValue(*storage, val);
  }
  else
  {
    // otherwise we need to store only the selected part of this pointer. We assume by SPIR-V
    // validity rules that the incoming value matches the pointed value
    if(storage->rows > 1)
    {
      // matrix case
      ClampScalars(apiWrapper, *storage, scalar0, scalar1);

      if(scalar0 != 0xff && scalar1 != 0xff)
      {
        // two indices - selecting a scalar. scalar0 is the first index in the chain so it chooses
        // column
        copyComp(*storage, scalar1 * storage->columns + scalar0, val, 0);
      }
      else if(scalar0 != 0xff)
      {
        // one index, selecting a column
        for(uint32_t row = 0; row < storage->rows; row++)
          copyComp(*storage, row * storage->columns + scalar0, val, row);
      }
    }
    else
    {
      ClampScalars(apiWrapper, *storage, scalar0);

      // vector case, selecting a scalar
      copyComp(*storage, scalar0, val, 0);
    }
  }
}

rdcstr Debugger::GetHumanName(Id id)
{
  // see if we have a dynamic name assigned (to disambiguate), if so use that
  auto it = dynamicNames.find(id);
  if(it != dynamicNames.end())
    return it->second;

  // otherwise try the string first
  rdcstr name = strings[id];

  // if we don't have a string name, we can be sure the id is unambiguous
  if(name.empty())
    return GetRawName(id);

  rdcstr basename = name;

  // otherwise check to see if it's been used before. If so give it a new name
  int alias = 2;
  while(usedNames.find(name) != usedNames.end())
  {
    name = basename + "@" + ToStr(alias);
    alias++;
  }

  usedNames.insert(name);
  dynamicNames[id] = name;

  return name;
}

void Debugger::CalcActiveMask(rdcarray<bool> &activeMask)
{
  // one bool per workgroup thread
  activeMask.resize(workgroup.size());

  // mark any threads that have finished as inactive, otherwise they're active
  for(size_t i = 0; i < workgroup.size(); i++)
    activeMask[i] = !workgroup[i].Finished();

  // only pixel shaders automatically converge workgroups, compute shaders need explicit sync
  if(stage != ShaderStage::Pixel)
    return;

  // otherwise we need to make sure that control flow which converges stays in lockstep so that
  // derivatives etc are still valid. While diverged, we don't have to keep threads in lockstep
  // since using derivatives is invalid.
  //
  // We take advantage of SPIR-V's structured control flow. We only ever diverge at a branch
  // instruction, and the preceeding OpLoopMerge/OpSelectionMerge.
  //
  // So the scheme is as follows:
  // * If we haven't diverged and all threads have the same nextInstruction, we're still uniform so
  //   continue in lockstep.
  // * As soon as they differ, we've diverged. Check the last mergeBlock that was specified - we
  //   won't be uniform again until all threads reach that block.
  // * Once we've diverged, any threads which are NOT in the merge block are active, and any threads
  //   which are in it are inactive. This causes them to pause and wait for others to catch up
  //   until the point where all threads are in the merge block at which point we've converged and
  //   can go back to uniformity.

  // if we're waiting on a converge block to be reached, we've diverged previously.
  bool wasDiverged = convergeBlock != Id();

  // see if we've diverged by starting procesing different next instructions
  bool diverged = false;
  for(size_t i = 1; !diverged && i < workgroup.size(); i++)
    diverged |= (workgroup[0].nextInstruction != workgroup[i].nextInstruction);

  if(!wasDiverged && diverged)
  {
    // if we've newly diverged, all workgroups should have the same merge block - the point where we
    // become uniform again.
    convergeBlock = workgroup[0].mergeBlock;
    for(size_t i = 1; i < workgroup.size(); i++)
      RDCASSERT(!activeMask[i] || convergeBlock == workgroup[i].mergeBlock);
  }

  if(wasDiverged || diverged)
  {
    // for every thread, turn it off if it's in the converge block
    rdcarray<bool> inConverge;
    inConverge.resize(activeMask.size());
    for(size_t i = 0; i < workgroup.size(); i++)
      inConverge[i] = (!workgroup[i].callstack.empty() &&
                       workgroup[i].callstack.back()->curBlock == convergeBlock);

    // is any thread active, but not converged?
    bool anyActiveNotConverged = false;
    for(size_t i = 0; i < workgroup.size(); i++)
      anyActiveNotConverged |= activeMask[i] && !inConverge[i];

    if(anyActiveNotConverged)
    {
      // if so, then only non-converged threads are active right now
      for(size_t i = 0; i < workgroup.size(); i++)
        activeMask[i] &= !inConverge[i];
    }
    else
    {
      // otherwise we can leave the active mask as is, forget the convergence point, and allow
      // everything to run as normal
      convergeBlock = Id();
    }
  }
}

void Debugger::AllocateVariable(Id id, Id typeId, ShaderVariable &outVar)
{
  // allocs should always be pointers
  RDCASSERT(dataTypes[typeId].type == DataType::PointerType);

  auto initCallback = [](ShaderVariable &var, const Decorations &, const DataType &, uint64_t,
                         const rdcstr &) {
    // ignore any callbacks we get on the way up for structs/arrays, we don't need it we only read
    // or write at primitive level
    if(!var.members.empty())
      return;

    // make it obvious when uninitialised values are used
    memset(&var.value, 0xcc, sizeof(var.value));
  };

  WalkVariable<ShaderVariable, true>(Decorations(), dataTypes[dataTypes[typeId].InnerType()], ~0U,
                                     outVar, rdcstr(), initCallback);
}

template <typename ShaderVarType, bool allocate>
uint32_t Debugger::WalkVariable(
    const Decorations &curDecorations, const DataType &type, uint64_t offsetOrLocation,
    ShaderVarType &var, const rdcstr &accessSuffix,
    std::function<void(ShaderVarType &, const Decorations &, const DataType &, uint64_t, const rdcstr &)>
        callback) const
{
  // if we're walking a const variable we just want to walk it without modification. So outVar
  // is NULL. Otherwise outVar points to the variable itself so we modify it before iterating
  ShaderVariable *outVar = allocate ? pointerIfMutable(var) : NULL;

  // the Location decoration should either be on the variable itself (in which case we hit this
  // first thing), or on the first member of a struct. i.e. once we have a location already and
  // we're auto-assigning from there we shouldn't encounter another location decoration somewhere
  // further down the struct chain. This also prevents us from using the same location for every
  // element in an array, since we have the same set of decorations on the array as on the members
  if((curDecorations.flags & Decorations::HasLocation) && offsetOrLocation == ~0U)
    offsetOrLocation = curDecorations.location;

  uint32_t numLocations = 0;

  switch(type.type)
  {
    case DataType::ScalarType:
    {
      if(outVar)
      {
        outVar->type = type.scalar().Type();
        outVar->rows = 1;
        outVar->columns = 1;
      }
      numLocations = 1;
      break;
    }
    case DataType::VectorType:
    {
      if(outVar)
      {
        outVar->type = type.scalar().Type();
        outVar->rows = 1U;
        outVar->columns = RDCMAX(1U, type.vector().count) & 0xff;
      }
      numLocations = 1U;
      break;
    }
    case DataType::MatrixType:
    {
      if(outVar)
      {
        outVar->type = type.scalar().Type();
        outVar->columns = RDCMAX(1U, type.matrix().count) & 0xff;
        outVar->rows = RDCMAX(1U, type.vector().count) & 0xff;
      }
      numLocations = var.rows;
      break;
    }
    case DataType::StructType:
    {
      for(int32_t i = 0; i < type.children.count(); i++)
      {
        if(outVar)
        {
          outVar->members.push_back(ShaderVariable());
          if(!type.children[i].name.empty())
            outVar->members.back().name = type.children[i].name;
          else
            outVar->members.back().name = StringFormat::Fmt("_child%d", i);
        }

        rdcstr childAccess = accessSuffix + "." + var.members.back().name;

        const Decorations &childDecorations = type.children[i].decorations;

        uint64_t childOffsetOrLocation = offsetOrLocation;

        // if the struct is concrete, it must have an offset. Otherwise it's opaque and we're using
        // locations
        if(childDecorations.flags & Decorations::HasOffset)
          childOffsetOrLocation += childDecorations.offset;
        else if(offsetOrLocation != ~0U)
          childOffsetOrLocation += numLocations;

        uint32_t childLocations = WalkVariable<ShaderVarType, allocate>(
            childDecorations, dataTypes[type.children[i].type], childOffsetOrLocation,
            var.members[i], childAccess, callback);

        numLocations += childLocations;
      }
      break;
    }
    case DataType::ArrayType:
    {
      // array stride is decorated on the type, not the member itself
      const Decorations &typeDecorations = decorations[type.id];

      uint32_t childOffset = 0;

      uint32_t len = uintComp(GetActiveLane().ids[type.length], 0);
      for(uint32_t i = 0; i < len; i++)
      {
        if(outVar)
        {
          outVar->members.push_back(ShaderVariable());
          outVar->members.back().name = StringFormat::Fmt("[%u]", i);
        }

        rdcstr childAccess = accessSuffix + var.members.back().name;

        uint32_t childLocations = WalkVariable<ShaderVarType, allocate>(
            curDecorations, dataTypes[type.InnerType()], offsetOrLocation + childOffset,
            var.members[i], childAccess, callback);

        numLocations += childLocations;

        // as above - either the type is concrete and has an array stride, or else we're using
        // locations
        if(typeDecorations.flags & Decorations::HasArrayStride)
          childOffset += decorations[type.id].arrayStride;
        else if(offsetOrLocation != ~0U)
          childOffset = numLocations;
      }
      break;
    }
    case DataType::PointerType:
    {
      RDCASSERT((dataTypes[type.id].pointerType.storage == StorageClass::PhysicalStorageBuffer) ||
                (dataTypes[type.id].pointerType.storage == StorageClass::PhysicalStorageBufferEXT));
      if(outVar)
      {
        outVar->type = VarType::GPUPointer;
        outVar->rows = 1;
        outVar->columns = 1;
      }
      numLocations = 1;
      break;
    }
    case DataType::ImageType:
    case DataType::SamplerType:
    case DataType::SampledImageType:
    case DataType::RayQueryType:
    case DataType::AccelerationStructureType:
    case DataType::UnknownType:
    {
      RDCERR("Unexpected variable type %d", type.type);
      return numLocations;
    }
  }

  if(callback)
    callback(var, curDecorations, type, offsetOrLocation, accessSuffix);

  // for auto-assigning locations, we return the number of locations
  return numLocations;
}

template <typename FloatType>
static void ApplyDerivative(uint32_t activeLaneIndex, uint32_t quadIndex, FloatType *dst,
                            DebugAPIWrapper::DerivativeDeltas &derivs)
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

  // in the diagrams below * marks the active lane index.
  //
  //   V and ^ == coarse ddy
  //   , and ` == fine ddy
  //   < and > == coarse ddx
  //   { and } == fine ddx
  //
  // We are basically making one or two cardinal direction moves from the starting point
  // (activeLaneIndex) to the end point (quadIndex).
  RDCASSERTNOTEQUAL(activeLaneIndex, quadIndex);

#define ADD_DERIV(src)       \
  for(int i = 0; i < 4; i++) \
    dst[i] += comp<FloatType>(src, i);
#define SUB_DERIV(src)       \
  for(int i = 0; i < 4; i++) \
    dst[i] -= comp<FloatType>(src, i);

  switch(activeLaneIndex)
  {
    case 0:
    {
      // +---+---+
      // |*0 > 1 |
      // +-V-+-V-+
      // | 2 | 3 |
      // +---+---+
      switch(quadIndex)
      {
        case 0: break;
        case 1: ADD_DERIV(derivs.ddxcoarse); break;
        case 2: ADD_DERIV(derivs.ddycoarse); break;
        case 3:
          ADD_DERIV(derivs.ddxcoarse);
          ADD_DERIV(derivs.ddycoarse);
          break;
        default: break;
      }
      break;
    }
    case 1:
    {
      // we need to use fine to get from 1 to 3 as coarse only ever involves 0->1 and 0->2
      // +---+---+
      // | 0 < 1*|
      // +-V-+-,-+
      // | 2 | 3 |
      // +---+---+
      switch(quadIndex)
      {
        case 0: SUB_DERIV(derivs.ddxcoarse); break;
        case 1: break;
        case 2:
          SUB_DERIV(derivs.ddxcoarse);
          ADD_DERIV(derivs.ddycoarse);
          break;
        case 3: ADD_DERIV(derivs.ddyfine); break;
        default: break;
      }
      break;
    }
    case 2:
    {
      // +---+---+
      // | 0 > 1 |
      // +-^-+---+
      // |*2 } 3 |
      // +---+---+
      switch(quadIndex)
      {
        case 0: SUB_DERIV(derivs.ddycoarse); break;
        case 1:
          SUB_DERIV(derivs.ddycoarse);
          ADD_DERIV(derivs.ddxcoarse);
          break;
        case 2: break;
        case 3: ADD_DERIV(derivs.ddxfine); break;
        default: break;
      }
      break;
    }
    case 3:
    {
      // +---+---+
      // | 0 < 1 |
      // +---+-`-+
      // | 2 { 3*|
      // +---+---+
      switch(quadIndex)
      {
        case 0:
          SUB_DERIV(derivs.ddyfine);
          SUB_DERIV(derivs.ddxcoarse);
          break;
        case 1: SUB_DERIV(derivs.ddyfine); break;
        case 2: SUB_DERIV(derivs.ddxfine); break;
        case 3: break;
        default: break;
      }
      break;
    }
    default: break;
  }
}

uint32_t Debugger::ApplyDerivatives(uint32_t quadIndex, const Decorations &curDecorations,
                                    uint32_t location, const DataType &inType, ShaderVariable &outVar)
{
  // only floats have derivatives
  if(outVar.type == VarType::Float || outVar.type == VarType::Half || outVar.type == VarType::Double)
  {
    ShaderBuiltin builtin = ShaderBuiltin::Undefined;
    if(curDecorations.flags & Decorations::HasBuiltIn)
      builtin = MakeShaderBuiltin(stage, curDecorations.builtIn);

    uint32_t component = 0;
    for(const DecorationAndParamData &dec : curDecorations.others)
    {
      if(dec.value == Decoration::Component)
      {
        component = dec.component;
        break;
      }
    }

    if(curDecorations.flags & Decorations::HasLocation)
      location = curDecorations.location;

    DebugAPIWrapper::DerivativeDeltas derivs =
        apiWrapper->GetDerivative(builtin, location, component, outVar.type);

    if(outVar.type == VarType::Float)
      ApplyDerivative<float>(activeLaneIndex, quadIndex, outVar.value.f32v.data(), derivs);
    else if(outVar.type == VarType::Half)
      ApplyDerivative<half_float::half>(activeLaneIndex, quadIndex,
                                        (half_float::half *)outVar.value.f16v.data(), derivs);
    else if(outVar.type == VarType::Double)
      ApplyDerivative<double>(activeLaneIndex, quadIndex, outVar.value.f64v.data(), derivs);
  }

  // each row consumes a new location
  return outVar.rows;
}

bool Debugger::IsDebugExtInstSet(Id id) const
{
  return knownExtSet[ExtSet_ShaderDbg] == id;
}

bool Debugger::InDebugScope(uint32_t inst) const
{
  return m_DebugInfo.lineScope.find(instructionOffsets[inst]) != m_DebugInfo.lineScope.end();
}

const ScopeData *Debugger::GetScope(size_t offset) const
{
  auto it = m_DebugInfo.lineScope.find(offset);
  if(it == m_DebugInfo.lineScope.end())
    return NULL;
  return it->second;
}

void Debugger::PreParse(uint32_t maxId)
{
  Processor::PreParse(maxId);

  strings.resize(idTypes.size());
  idLiveRange.resize(idTypes.size());

  m_InstInfo.reserve(idTypes.size());
}

void Debugger::PostParse()
{
  Processor::PostParse();

  // declare pointerTypes for all declared physical pointer types. This will match the reflection
  for(auto it = dataTypes.begin(); it != dataTypes.end(); ++it)
  {
    if(it->second.type == DataType::PointerType &&
       it->second.pointerType.storage == rdcspv::StorageClass::PhysicalStorageBuffer)
    {
      idToPointerType.insert(std::make_pair(it->second.InnerType(), (uint16_t)idToPointerType.size()));
    }
  }
  pointerTypeToId.resize(idToPointerType.size());
  for(auto it = idToPointerType.begin(); it != idToPointerType.end(); ++it)
    pointerTypeToId[it->second] = it->first;

  for(const MemberName &mem : memberNames)
    dataTypes[mem.id].children[mem.member].name = mem.name;

  // global IDs never hit a death point
  for(const Variable &v : globals)
    idLiveRange[v.id].second = ~0U;

  if(m_DebugInfo.valid)
  {
    for(auto it = m_DebugInfo.scopes.begin(); it != m_DebugInfo.scopes.end(); ++it)
    {
      ScopeData *scope = &it->second;

      // keep every ID referenced by a local alive until the scope ends. We do this even if a source
      // variable maps to multiple debug variables and technically the earlier ones could be left to
      // die when superceeded by the later ones. This is simple and only means a little bloating of
      // debug variables in the UI (which generally won't be viewed directly anyway)
      for(LocalMapping &m : scope->localMappings)
      {
        Id id = m.debugVar;

        if(id == Id())
          continue;

        idLiveRange[id].second = RDCMAX(scope->end + 1, idLiveRange[id].second);
      }

      // every scope's parent lasts at least as long as it
      while(scope->parent)
      {
        scope->parent->end = RDCMAX(scope->parent->end, scope->end);
        scope = scope->parent;
      }
    }
  }

  memberNames.clear();
}

void Debugger::RegisterOp(Iter it)
{
  Processor::RegisterOp(it);

  OpDecoder opdata(it);

  // we add +1 so that we don't remove the ID on its last use, but the next subsequent instruction
  // since blocks always end with a terminator that doesn't consume IDs we're interested in
  // (variables) we'll always have one extra instruction to step to
  OpDecoder::ForEachID(it, [this, &it](Id id, bool result) {
    if(result)
      idLiveRange[id].first = it.offs();
    idLiveRange[id].second = RDCMAX(it.offs() + 1, idLiveRange[id].second);
  });

  bool leaveScope = false;
  bool executable = curFunction != NULL;

  const uint32_t curInstIndex = (uint32_t)instructionOffsets.size();

  if(opdata.op == Op::ExtInst || opdata.op == Op::ExtInstWithForwardRefsKHR)
  {
    OpExtInst extinst(it);

    if(knownExtSet[ExtSet_GLSL450] == extinst.set)
    {
      // all parameters to GLSL.std.450 are Ids, extend idDeathOffset appropriately
      for(const uint32_t param : extinst.params)
      {
        Id id = Id::fromWord(param);
        idLiveRange[id].second = RDCMAX(it.offs() + 1, idLiveRange[id].second);
      }
    }
    else if(knownExtSet[ExtSet_Printf] == extinst.set)
    {
      // all parameters to NonSemantic.DebugPrintf are Ids, extend idDeathOffset appropriately
      for(const uint32_t param : extinst.params)
      {
        Id id = Id::fromWord(param);
        idLiveRange[id].second = RDCMAX(it.offs() + 1, idLiveRange[id].second);
      }
    }
    else if(knownExtSet[ExtSet_ShaderDbg] == extinst.set)
    {
      // the types are identical just with different accessors
      OpShaderDbg &dbg = (OpShaderDbg &)extinst;

      if(dbg.inst != ShaderDbg::Value)
        executable = false;

      switch(dbg.inst)
      {
        case ShaderDbg::Source:
        {
          int32_t fileIndex = (int32_t)m_DebugInfo.sources.size();

          m_DebugInfo.sources[dbg.result] = fileIndex;
          m_DebugInfo.filenames[dbg.result] = strings[dbg.arg<Id>(0)];
          break;
        }
        case ShaderDbg::CompilationUnit:
        {
          m_DebugInfo.scopes[dbg.result] = {
              DebugScope::CompilationUnit,
              NULL,
              1,
              1,
              m_DebugInfo.sources[dbg.arg<Id>(2)],
              0,
              m_DebugInfo.filenames[dbg.arg<Id>(2)],
          };
          break;
        }
        case ShaderDbg::FunctionDefinition:
        {
          m_DebugInfo.funcToDebugFunc[dbg.arg<Id>(1)] = dbg.arg<Id>(0);
          break;
        }
        case ShaderDbg::Function:
        {
          rdcstr name = strings[dbg.arg<Id>(0)];
          // ignore arg 1 type
          // don't use arg 2 source - assume the parent is in the same file so it's redundant
          uint32_t line = EvaluateConstant(dbg.arg<Id>(3), {}).value.u32v[0];
          uint32_t column = EvaluateConstant(dbg.arg<Id>(4), {}).value.u32v[0];
          ScopeData *parent = &m_DebugInfo.scopes[dbg.arg<Id>(5)];
          // ignore arg 6 linkage name
          // ignore arg 7 flags
          // ignore arg 8 scope line
          // ignore arg 9 (optional) declaration

          m_DebugInfo.scopes[dbg.result] = {
              DebugScope::Function, parent, line, column, parent->fileIndex, 0, name,
          };
          break;
        }
        case ShaderDbg::TypeBasic:
        {
          uint32_t byteSize = EvaluateConstant(dbg.arg<Id>(1), {}).value.u32v[0];
          uint32_t encoding = EvaluateConstant(dbg.arg<Id>(2), {}).value.u32v[0];
          switch(encoding)
          {
            case 2: m_DebugInfo.types[dbg.result].type = VarType::Bool; break;
            case 3:
              if(byteSize == 64)
                m_DebugInfo.types[dbg.result].type = VarType::Double;
              else if(byteSize == 32)
                m_DebugInfo.types[dbg.result].type = VarType::Float;
              else if(byteSize == 16)
                m_DebugInfo.types[dbg.result].type = VarType::Half;
              break;
            case 4:
              if(byteSize == 64)
                m_DebugInfo.types[dbg.result].type = VarType::SLong;
              else if(byteSize == 32)
                m_DebugInfo.types[dbg.result].type = VarType::SInt;
              else if(byteSize == 16)
                m_DebugInfo.types[dbg.result].type = VarType::SShort;
              else if(byteSize == 8)
                m_DebugInfo.types[dbg.result].type = VarType::SByte;
              break;
            case 5: m_DebugInfo.types[dbg.result].type = VarType::SByte; break;
            case 6:
              if(byteSize == 64)
                m_DebugInfo.types[dbg.result].type = VarType::ULong;
              else if(byteSize == 32)
                m_DebugInfo.types[dbg.result].type = VarType::UInt;
              else if(byteSize == 16)
                m_DebugInfo.types[dbg.result].type = VarType::UShort;
              else if(byteSize == 8)
                m_DebugInfo.types[dbg.result].type = VarType::UByte;
              break;
            case 7: m_DebugInfo.types[dbg.result].type = VarType::UByte; break;
          }
          break;
        }
        case ShaderDbg::TypePointer:
        {
          m_DebugInfo.types[dbg.result].baseType = dbg.arg<Id>(0);
          m_DebugInfo.types[dbg.result].type = VarType::GPUPointer;
          break;
        }
        case ShaderDbg::TypeVector:
        {
          m_DebugInfo.types[dbg.result].baseType = dbg.arg<Id>(0);
          m_DebugInfo.types[dbg.result].vecSize = EvaluateConstant(dbg.arg<Id>(1), {}).value.u32v[0];
          break;
        }
        case ShaderDbg::TypeMatrix:
        {
          m_DebugInfo.types[dbg.result].baseType = dbg.arg<Id>(0);
          m_DebugInfo.types[dbg.result].matSize = EvaluateConstant(dbg.arg<Id>(1), {}).value.u32v[0];
          m_DebugInfo.types[dbg.result].colMajorMat =
              EvaluateConstant(dbg.arg<Id>(2), {}).value.u32v[0] != 0;
          break;
        }
        case ShaderDbg::TypeArray:
        {
          m_DebugInfo.types[dbg.result].baseType = dbg.arg<Id>(0);
          size_t countDims = dbg.params.size();
          m_DebugInfo.types[dbg.result].arrayDimensions.resize(countDims - 1);
          for(uint32_t i = 1; i < countDims; ++i)
          {
            size_t idx = i - 1;
            m_DebugInfo.types[dbg.result].arrayDimensions[idx] =
                EvaluateConstant(dbg.arg<Id>(i), {}).value.u32v[0];
          }

          break;
        }
        case ShaderDbg::TypeMember:
        {
          rdcstr name = strings[dbg.arg<Id>(0)];
          Id type = dbg.arg<Id>(1);
          uint32_t offset = EvaluateConstant(dbg.arg<Id>(4), {}).value.u32v[0];
          (void)offset;
          break;
        }
        case ShaderDbg::TypeComposite:
        {
          rdcstr name = strings[dbg.arg<Id>(0)];
          uint32_t tag = EvaluateConstant(dbg.arg<Id>(1), {}).value.u32v[0];
          const rdcstr tagString[3] = {
              "class ",
              "struct ",
              "union ",
          };

          // don't use arg 2 source - assume the parent is in the same file so it's redundant
          uint32_t line = EvaluateConstant(dbg.arg<Id>(3), {}).value.u32v[0];
          uint32_t column = EvaluateConstant(dbg.arg<Id>(4), {}).value.u32v[0];
          ScopeData *parent = &m_DebugInfo.scopes[dbg.arg<Id>(5)];
          // ignore arg 6 linkage name
          // ignore arg 7 size
          // ignore arg 8 flags

          for(uint32_t i = 9; i < dbg.params.size(); i++)
          {
            OpShaderDbg member(GetID(dbg.arg<Id>(i)));

            rdcstr memberName;
            Id memberType;
            uint32_t memberOffset = 0;
            switch(member.inst)
            {
              case ShaderDbg::TypeMember:
                memberName = strings[member.arg<Id>(0)];
                memberType = member.arg<Id>(1);
                memberOffset = EvaluateConstant(member.arg<Id>(5), {}).value.u32v[0];
                break;
              case ShaderDbg::TypeFunction:
                memberName = strings[member.arg<Id>(0)];
                memberType = member.arg<Id>(1);
                break;
              case ShaderDbg::TypeInheritance: memberName = "Inheritence"; break;
              default: RDCERR("Unhandled DebugTypeComposite entry %u", member.inst);
            }

            m_DebugInfo.types[dbg.result].structMembers.push_back({memberName, memberType});
            m_DebugInfo.types[dbg.result].memberOffsets.push_back(memberOffset);
          }

          name = tagString[tag % 3] + name;

          m_DebugInfo.scopes[dbg.result] = {
              DebugScope::Composite, parent, line, column, parent->fileIndex, 0, name,
          };
          break;
        }
        case ShaderDbg::LexicalBlock:
        {
          // don't use arg 0 source - assume the parent is in the same file so it's redundant
          uint32_t line = EvaluateConstant(dbg.arg<Id>(1), {}).value.u32v[0];
          uint32_t column = EvaluateConstant(dbg.arg<Id>(2), {}).value.u32v[0];
          ScopeData *parent = &m_DebugInfo.scopes[dbg.arg<Id>(3)];

          rdcstr name;
          if(dbg.params.count() >= 5)
          {
            name = strings[dbg.arg<Id>(4)];
            if(name.isEmpty())
              name = "anonymous_scope";
          }
          else
          {
            name = parent->name + ":" + ToStr(line);
          }

          m_DebugInfo.scopes[dbg.result] = {
              DebugScope::Block, parent, line, column, parent->fileIndex, 0, name,
          };
          break;
        }
        case ShaderDbg::Scope:
        {
          if(m_DebugInfo.curScope)
            m_DebugInfo.curScope->end = it.offs();

          m_DebugInfo.curScope = &m_DebugInfo.scopes[dbg.arg<Id>(0)];

          // pick up any pending mappings for this function if we just entered into a new function.
          // See the comment below in Value for this workaround
          for(size_t i = 0; i < m_DebugInfo.pendingMappings.size(); i++)
          {
            rdcpair<const ScopeData *, LocalMapping> &cur = m_DebugInfo.pendingMappings[i];
            if(m_DebugInfo.curScope->HasAncestor(cur.first))
            {
              m_DebugInfo.curScope->localMappings.push_back(std::move(cur.second));

              // the array isn't sorted so we can just swap the last one into this spot to avoid
              // moving everything
              std::swap(cur, m_DebugInfo.pendingMappings.back());
              m_DebugInfo.pendingMappings.pop_back();
            }
          }

          if(dbg.params.size() >= 2)
            m_DebugInfo.curInline = &m_DebugInfo.inlined[dbg.arg<Id>(1)];
          else
            m_DebugInfo.curInline = NULL;
          break;
        }
        case ShaderDbg::NoScope:
        {
          // don't want to set curScope to NULL until after this instruction. That way flood-fill of
          // scopes in PostParse() can find this instruction in a scope.
          leaveScope = true;
          break;
        }
        case ShaderDbg::GlobalVariable:
        {
          // copy the name string to the variable string only if it's empty. If it has a name
          // already,
          // we prefer that. If the variable is DebugInfoNone then we don't care about it's name.
          if(strings[dbg.arg<Id>(7)].empty())
            strings[dbg.arg<Id>(7)] = strings[dbg.arg<Id>(0)];

          OpVariable var(GetID(dbg.arg<Id>(7)));

          if(var.storageClass == StorageClass::Private ||
             var.storageClass == StorageClass::Workgroup || var.storageClass == StorageClass::Output)
          {
            m_DebugInfo.globals.push_back(var.result);
          }
          break;
        }
        case ShaderDbg::LocalVariable:
        {
          m_DebugInfo.locals[dbg.result] = {
              strings[dbg.arg<Id>(0)],
              &m_DebugInfo.scopes[dbg.arg<Id>(5)],
              &m_DebugInfo.types[dbg.arg<Id>(1)],
          };

          m_DebugInfo.scopes[dbg.arg<Id>(5)].locals.push_back(dbg.result);
          break;
        }
        case ShaderDbg::Declare:
        case ShaderDbg::Value:
        {
          Id sourceVarId = dbg.arg<Id>(0);
          Id debugVarId = dbg.arg<Id>(1);

          // check the function this variable is scoped inside of at declaration time.
          const ScopeData *varDeclScope = m_DebugInfo.locals[sourceVarId].scope;

          // bit of a hack - only process declares/values for variables inside a scope that is
          // within that function. If we see a declare/value in another function we defer it hoping
          // that we will encounter a scope later that's valid for it.
          const bool insideValidScope = m_DebugInfo.curScope->HasAncestor(varDeclScope);

          LocalMapping mapping = {curInstIndex, sourceVarId, debugVarId,
                                  dbg.inst == ShaderDbg::Declare};

          if(constants.find(debugVarId) != constants.end() &&
             !m_DebugInfo.constants.contains(debugVarId))
            m_DebugInfo.constants.push_back(debugVarId);

          mapping.indexes.resize(dbg.params.size() - 3);
          for(uint32_t i = 0; i < mapping.indexes.size(); i++)
            mapping.indexes[i] = EvaluateConstant(dbg.arg<Id>(i + 3), {}).value.u32v[0];

          {
            // don't support expressions, only allow for a single 'deref' which is used for
            // variables to 'deref' into the pointed value
            OpShaderDbg expr(GetID(dbg.arg<Id>(2)));

            for(uint32_t i = 0; i < expr.params.size(); i++)
            {
              OpShaderDbg op(GetID(expr.arg<Id>(i)));

              if(op.params.size() > 1 || EvaluateConstant(op.arg<Id>(0), {}).value.u32v[0] != 0)
              {
                RDCERR("Only deref expressions supported");
              }
            }
          }

          if(insideValidScope)
          {
            m_DebugInfo.curScope->localMappings.push_back(mapping);
          }
          else
          {
            // remove any pending mapping that already exists for this variable, without any way to
            // meaningfully know which one to use when we pick the latter.
            m_DebugInfo.pendingMappings.removeIf(
                [&mapping](const rdcpair<const ScopeData *, LocalMapping> &m) {
                  return mapping.isSourceSupersetOf(m.second);
                });

            m_DebugInfo.pendingMappings.push_back({varDeclScope, mapping});
          }

          break;
        }
        case ShaderDbg::InlinedAt:
        {
          // ignore arg 0 the line number
          ScopeData *scope = &m_DebugInfo.scopes[dbg.arg<Id>(1)];

          if(dbg.params.count() >= 3)
            m_DebugInfo.inlined[dbg.result] = {scope, &m_DebugInfo.inlined[dbg.arg<Id>(2)]};
          else
            m_DebugInfo.inlined[dbg.result] = {scope, NULL};
          break;
        }
        case ShaderDbg::InlinedVariable:
        {
          // TODO handle inlined variables
          break;
        }
        case ShaderDbg::Line:
        {
          m_CurLineCol.lineStart = EvaluateConstant(dbg.arg<Id>(1), {}).value.u32v[0];
          m_CurLineCol.lineEnd = EvaluateConstant(dbg.arg<Id>(2), {}).value.u32v[0];
          if(Vulkan_Debug_UseDebugColumnInformation())
          {
            m_CurLineCol.colStart = EvaluateConstant(dbg.arg<Id>(3), {}).value.u32v[0];
            m_CurLineCol.colEnd = EvaluateConstant(dbg.arg<Id>(4), {}).value.u32v[0];
          }

          // find file index by filename matching, this would be nice to improve as it's brittle
          m_CurLineCol.fileIndex = m_DebugInfo.sources[dbg.arg<Id>(0)];
          break;
        }
        case ShaderDbg::NoLine:
        {
          m_CurLineCol = LineColumnInfo();
          break;
        }
        default: break;
      }
    }
  }
  else if(opdata.op == Op::ExtInstImport)
  {
    OpExtInstImport extimport(it);

    if(extimport.result == knownExtSet[ExtSet_ShaderDbg])
    {
      m_DebugInfo.valid = true;
    }
  }

  if(opdata.op == Op::Source)
  {
    OpSource source(it);

    if(!source.source.empty())
    {
      m_Files[source.file] = m_Files.size();
    }
  }
  else if(opdata.op == Op::Line)
  {
    OpLine line(it);

    if(m_DebugInfo.valid)
    {
      // ignore any OpLine when we have proper debug info
    }
    else
    {
      m_CurLineCol.lineStart = line.line;
      m_CurLineCol.lineEnd = line.line;
      m_CurLineCol.colStart = line.column;
      m_CurLineCol.fileIndex = (int32_t)m_Files[line.file];
    }
  }
  else if(opdata.op == Op::NoLine)
  {
    if(!m_DebugInfo.valid)
      m_CurLineCol = LineColumnInfo();
  }
  else if(executable)
  {
    // for debug info, only apply line info if we're in a scope. Otherwise the line info may not
    // apply to this instruction. This means OpPhi's will never be line mapped
    if(m_DebugInfo.valid)
    {
      if(m_DebugInfo.curScope)
        m_InstInfo.push_back({curInstIndex, m_CurLineCol});
      else
        m_InstInfo.push_back({curInstIndex, LineColumnInfo()});
    }
    else
    {
      m_InstInfo.push_back({curInstIndex, m_CurLineCol});
    }
  }

  if(m_DebugInfo.valid)
  {
    m_DebugInfo.lineScope[it.offs()] = m_DebugInfo.curScope;
    m_DebugInfo.lineInline[it.offs()] = m_DebugInfo.curInline;
  }

  // if we're explicitly leaving the scope because of a DebugNoScope, or if we're leaving due to the
  // end of a block then set scope to NULL now.
  if(leaveScope || it.opcode() == Op::Kill || it.opcode() == Op::Unreachable ||
     it.opcode() == Op::Branch || it.opcode() == Op::BranchConditional ||
     it.opcode() == Op::Switch || it.opcode() == Op::Return || it.opcode() == Op::ReturnValue)
  {
    if(m_DebugInfo.curScope)
      m_DebugInfo.curScope->end = it.offs();

    m_DebugInfo.curScope = NULL;
    m_DebugInfo.curInline = NULL;
  }

  if(opdata.op == Op::String)
  {
    OpString string(it);

    strings[string.result] = string.string;
  }
  else if(opdata.op == Op::Name)
  {
    OpName name(it);

    // technically you could name a string - in that case we ignore the name
    if(strings[name.target].empty())
      strings[name.target] = name.name;
  }
  else if(opdata.op == Op::MemberName)
  {
    OpMemberName memberName(it);

    memberNames.push_back({memberName.type, memberName.member, memberName.name});
  }
  else if(opdata.op == Op::EntryPoint)
  {
    OpEntryPoint entryPoint(it);

    entryLookup[ShaderEntryPoint(entryPoint.name, MakeShaderStage(entryPoint.executionModel))] =
        entryPoint.entryPoint;
  }
  else if(opdata.op == Op::Function)
  {
    OpFunction func(it);

    curFunction = &functions[func.result];

    curFunction->begin = it.offs();
  }
  else if(opdata.op == Op::FunctionParameter)
  {
    OpFunctionParameter param(it);

    curFunction->parameters.push_back(param.result);
  }
  else if(opdata.op == Op::Variable)
  {
    OpVariable var(it);

    if(var.storageClass == StorageClass::Function && curFunction)
      curFunction->variables.push_back(var.result);

    // variables are always pointers
    Id varType = dataTypes[var.resultType].InnerType();

    // if we don't have a name for this variable but it's a pointer to a struct that is named then
    // give the variable a name based on the type. This is a common pattern in GLSL for global
    // blocks, and since the variable is how we access commonly we should give it a recognisable
    // name.
    //
    // Don't do this if we have debug info, rely on it purely to give us the right data
    if(strings[var.result].empty() && dataTypes[varType].type == DataType::StructType &&
       !strings[varType].empty() && !m_DebugInfo.valid)
    {
      strings[var.result] = strings[varType] + "_var";
    }
  }
  else if(opdata.op == Op::Label)
  {
    OpLabel lab(it);

    labelInstruction[lab.result] = instructionOffsets.count();
  }

  // everything else inside a function becomes an instruction, including the OpFunction and
  // OpFunctionEnd. We won't actually execute these instructions

  instructionOffsets.push_back(it.offs());

  if(opdata.op == Op::FunctionEnd)
  {
    // allow function parameters and variables to live indefinitely
    for(const Id &id : curFunction->parameters)
      idLiveRange[id].second = ~0U;
    for(const Id &id : curFunction->variables)
      idLiveRange[id].second = ~0U;
    curFunction = NULL;
  }
}

};    // namespace rdcspv

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

TEST_CASE("Check SPIRV Id naming", "[tostr]")
{
  SECTION("Test GetRawName")
  {
    CHECK(rdcspv::GetRawName(rdcspv::Id::fromWord(1234)) == "_1234");
    CHECK(rdcspv::GetRawName(rdcspv::Id::fromWord(12345)) == "_12345");
    CHECK(rdcspv::GetRawName(rdcspv::Id::fromWord(999)) == "_999");
    CHECK(rdcspv::GetRawName(rdcspv::Id::fromWord(0xffffffff)) == "_4294967295");
    CHECK(rdcspv::GetRawName(rdcspv::Id()) == "_0");
  };

  SECTION("Test ParseRawName")
  {
    CHECK(rdcspv::ParseRawName("_1234") == rdcspv::Id::fromWord(1234));
    CHECK(rdcspv::ParseRawName("_12345") == rdcspv::Id::fromWord(12345));
    CHECK(rdcspv::ParseRawName("_999") == rdcspv::Id::fromWord(999));
    CHECK(rdcspv::ParseRawName("_4294967295") == rdcspv::Id::fromWord(0xffffffff));
    CHECK(rdcspv::ParseRawName("_0") == rdcspv::Id());
    CHECK(rdcspv::ParseRawName("1234") == rdcspv::Id());
    CHECK(rdcspv::ParseRawName("999") == rdcspv::Id());
    CHECK(rdcspv::ParseRawName("1") == rdcspv::Id());
    CHECK(rdcspv::ParseRawName("-1234") == rdcspv::Id());
    CHECK(rdcspv::ParseRawName("asdf") == rdcspv::Id());
  };
}

#endif
