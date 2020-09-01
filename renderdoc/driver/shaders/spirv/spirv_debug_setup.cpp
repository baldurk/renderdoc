/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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
#include "spirv_op_helpers.h"
#include "spirv_reflect.h"

static ShaderVariable *pointerIfMutable(const ShaderVariable &var)
{
  return NULL;
}
static ShaderVariable *pointerIfMutable(ShaderVariable &var)
{
  return &var;
}

static void ClampScalars(rdcspv::DebugAPIWrapper *apiWrapper, const ShaderVariable &var,
                         uint32_t &scalar0)
{
  if(scalar0 > var.columns && scalar0 != ~0U)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Invalid scalar index %u at %u-vector %s. Clamping to %u", scalar0,
                          var.columns, var.name.c_str(), var.columns - 1));
    scalar0 = RDCMIN(0U, uint32_t(var.columns - 1));
  }
}

static void ClampScalars(rdcspv::DebugAPIWrapper *apiWrapper, const ShaderVariable &var,
                         uint32_t &scalar0, uint32_t &scalar1)
{
  if(scalar0 > var.columns && scalar0 != ~0U)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Invalid scalar index %u at matrix %s with %u columns. Clamping to %u",
                          scalar0, var.columns, var.name.c_str(), var.columns - 1));
    scalar0 = RDCMIN(0U, uint32_t(var.columns - 1));
  }
  if(scalar1 > var.rows && scalar1 != ~0U)
  {
    apiWrapper->AddDebugMessage(
        MessageCategory::Execution, MessageSeverity::High, MessageSource::RuntimeWarning,
        StringFormat::Fmt("Invalid scalar index %u at matrix %s with %u rows. Clamping to %u",
                          scalar1, var.rows, var.name.c_str(), var.rows - 1));
    scalar1 = RDCMIN(0U, uint32_t(var.rows - 1));
  }
}

static uint32_t VarByteSize(const ShaderVariable &var)
{
  return VarTypeByteSize(var.type) * RDCMAX(1U, (uint32_t)var.rows) *
         RDCMAX(1U, (uint32_t)var.columns);
}

namespace rdcspv
{
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

  if(m_MajorVersion > 1 || m_MinorVersion > 5)
  {
    debugStatus +=
        StringFormat::Fmt("Unsupported SPIR-V version %u.%u\n", m_MajorVersion, m_MinorVersion);
    debuggable = false;
  }

  // whitelist supported extensions
  for(const rdcstr &ext : extensions)
  {
    if(ext == "SPV_KHR_shader_draw_parameters" || ext == "SPV_KHR_device_group" ||
       ext == "SPV_KHR_multiview" || ext == "SPV_KHR_storage_buffer_storage_class" ||
       ext == "SPV_KHR_post_depth_coverage" || ext == "SPV_EXT_shader_stencil_export" ||
       ext == "SPV_EXT_shader_viewport_index_layer" || ext == "SPV_EXT_fragment_fully_covered" ||
       ext == "SPV_GOOGLE_decorate_string" || ext == "SPV_GOOGLE_hlsl_functionality1" ||
       ext == "SPV_EXT_descriptor_indexing" || ext == "SPV_KHR_vulkan_memory_model" ||
       ext == "SPV_EXT_fragment_invocation_density" ||
       ext == "SPV_KHR_no_integer_wrap_decoration" || ext == "SPV_KHR_float_controls" ||
       ext == "SPV_KHR_shader_clock")
    {
      continue;
    }

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
      case Capability::Float64:
      {
        supported = true;
        break;
      }

      // we plan to support these but needs additional testing/proving

      // all these are related to non-32-bit types
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

      // atomics
      case Capability::Int64Atomics:

      // physical pointers
      case Capability::PhysicalStorageBufferAddresses:

      // MSAA custom interpolation
      case Capability::InterpolationFunction:

      // variable pointers
      case Capability::VariablePointersStorageBuffer:
      case Capability::VariablePointers:

      // float controls
      case Capability::DenormPreserve:
      case Capability::DenormFlushToZero:
      case Capability::SignedZeroInfNanPreserve:
      case Capability::RoundingModeRTE:
      case Capability::RoundingModeRTZ:

      // group instructions
      case Capability::Groups:
      case Capability::GroupNonUniform:
      case Capability::GroupNonUniformVote:
      case Capability::GroupNonUniformArithmetic:
      case Capability::GroupNonUniformBallot:
      case Capability::GroupNonUniformShuffle:
      case Capability::GroupNonUniformShuffleRelative:
      case Capability::GroupNonUniformClustered:
      case Capability::GroupNonUniformQuad:
      case Capability::SubgroupBallotKHR:
      case Capability::SubgroupVoteKHR:

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
      case Capability::Max:
      case Capability::Invalid:
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

ShaderDebugTrace *Debugger::BeginDebug(DebugAPIWrapper *api, const ShaderStage shaderStage,
                                       const rdcstr &entryPoint,
                                       const rdcarray<SpecConstant> &specInfo,
                                       const std::map<size_t, uint32_t> &instructionLines,
                                       const SPIRVPatchData &patchData, uint32_t activeIndex)
{
  Id entryId = entryLookup[entryPoint];

  if(entryId == Id())
  {
    RDCERR("Invalid entry point '%s'", entryPoint.c_str());
    return new ShaderDebugTrace;
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

  // evaluate all constants
  for(auto it = constants.begin(); it != constants.end(); it++)
    active.ids[it->first] = EvaluateConstant(it->first, specInfo);

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
      }

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      const rdcarray<rdcstr> &sigNames = isInput ? inputSigNames : outputSigNames;

      // fill the interface variable
      auto fillInputCallback = [this, isInput, ret, &sigNames, &rawName, &sourceName](
          ShaderVariable &var, const Decorations &curDecorations, const DataType &type,
          uint64_t location, const rdcstr &accessSuffix) {

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
          memset(var.value.u64v, 0xcc, sizeof(var.value.u64v));
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

          for(uint32_t x = 0; x < uint32_t(var.rows) * var.columns; x++)
            sourceVar.variables.push_back(DebugVariableReference(
                isInput ? DebugVariableType::Input : DebugVariableType::Variable, debugVarName, x));

          ret->sourceVars.push_back(sourceVar);
          if(!isInput)
            globalSourceVars.push_back(sourceVar);
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
      if(innertype->type == DataType::ArrayType)
      {
        isArray = true;
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

        uint32_t bindset = 0, bind = 0;
        if(v.storage == StorageClass::PushConstant)
        {
          bindset = PushConstantBindSet;
        }
        else
        {
          if(decorations[v.id].flags & Decorations::HasDescriptorSet)
            bindset = decorations[v.id].set;
          if(decorations[v.id].flags & Decorations::HasBinding)
            bind = decorations[v.id].binding;
        }

        SourceVariableMapping sourceVar;
        sourceVar.name = sourceName;
        sourceVar.offset = 0;

        if(ssbo)
        {
          var.rows = 1;
          var.columns = 1;
          var.type = VarType::ReadWriteResource;

          var.SetBinding((int32_t)bindset, (int32_t)bind, 0U);

          var.value.u64v[SSBOVariableSlot] = 1;

          if(isArray)
            var.value.u64v[ArrayVariableSlot] = 1;

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
          BindpointIndex bindpoint;

          // TODO handle arrays
          bindpoint.bindset = (int32_t)bindset;
          bindpoint.bind = (int32_t)bind;

          auto cbufferCallback = [this, bindpoint](
              ShaderVariable &var, const Decorations &curDecorations, const DataType &type,
              uint64_t offset, const rdcstr &) {

            if(!var.members.empty())
              return;

            // non-matrix case is simple, just read the size of the variable
            if(var.rows == 1)
            {
              this->apiWrapper->ReadBufferValue(bindpoint, offset, VarByteSize(var), var.value.uv);
            }
            else
            {
              // matrix case is more complicated. Either read column by column or row by row
              // depending on
              // majorness
              uint32_t matrixStride = curDecorations.matrixStride;

              if(!(curDecorations.flags & Decorations::HasMatrixStride))
              {
                RDCWARN("Matrix without matrix stride - assuming legacy vec4 packed");
                matrixStride = 16;
              }

              if(curDecorations.flags & Decorations::ColMajor)
              {
                ShaderValue tmp;

                uint32_t colSize = VarTypeByteSize(var.type) * var.rows;
                for(uint32_t c = 0; c < var.columns; c++)
                {
                  // read the column
                  this->apiWrapper->ReadBufferValue(bindpoint, offset + c * matrixStride, colSize,
                                                    &tmp.uv[0]);

                  // now write it into the appropiate elements in the destination ShaderValue
                  for(uint32_t r = 0; r < var.rows; r++)
                    var.value.uv[r * var.columns + c] = tmp.uv[r];
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
                  this->apiWrapper->ReadBufferValue(bindpoint, offset + r * matrixStride, rowSize,
                                                    &var.value.uv[r * var.columns]);
                }
              }
            }
          };

          WalkVariable<ShaderVariable, true>(decorations[v.id], *innertype, 0U, var, rdcstr(),
                                             cbufferCallback);

          if(isArray)
            RDCERR("Uniform buffer arrays not supported yet");

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
        sourceName = StringFormat::Fmt("_%s_set%u_bind%u", innerName.c_str(), decorations[v.id].set,
                                       decorations[v.id].binding);
      }

      DebugVariableType debugType = DebugVariableType::ReadOnlyResource;

      uint32_t set = 0, bind = 0;
      if(decorations[v.id].flags & Decorations::HasDescriptorSet)
        set = decorations[v.id].set;
      if(decorations[v.id].flags & Decorations::HasBinding)
        bind = decorations[v.id].binding;

      var.SetBinding((int32_t)set, (int32_t)bind, 0U);

      if(innertype->type == DataType::ArrayType)
      {
        var.value.u64v[ArrayVariableSlot] = 1;
        innertype = &dataTypes[innertype->InnerType()];
      }

      if(innertype->type == DataType::SamplerType)
      {
        var.type = VarType::Sampler;
        debugType = DebugVariableType::Sampler;

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

        Id imgid = type.InnerType();

        if(innertype->type == DataType::SampledImageType)
          imgid = sampledImageTypes[imgid].baseId;

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

        var.value.u64v[TextureTypeVariableSlot] = texType;

        if(imageTypes[imgid].sampled == 2 && imageTypes[imgid].dim != Dim::SubpassData)
        {
          var.type = VarType::ReadWriteResource;
          debugType = DebugVariableType::ReadWriteResource;

          global.readWriteResources.push_back(var);
          pointerIDs.push_back(GLOBAL_POINTER(v.id, readWriteResources));
        }
        else
        {
          global.readOnlyResources.push_back(var);
          pointerIDs.push_back(GLOBAL_POINTER(v.id, readOnlyResources));
        }
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

        memset(var.value.u64v, 0xcc, sizeof(var.value.u64v));
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

      if(sourceName != var.name)
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

        globalSourceVars.push_back(sourceVar);
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

  ret->lineInfo.resize(instructionOffsets.size());
  for(size_t i = 0; i < instructionOffsets.size(); i++)
  {
    ret->lineInfo[i] = m_LineColInfo[instructionOffsets[i]];

    {
      auto it = instructionLines.find(instructionOffsets[i]);
      if(it != instructionLines.end())
        ret->lineInfo[i].disassemblyLine = it->second;
      else
        ret->lineInfo[i].disassemblyLine = 0;
    }
  }

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
        thread.FillCallstack(initial);
        initial.nextInstruction = thread.nextInstruction;
        initial.sourceVars = thread.sourceVars;
      }
      else
      {
        thread.EnterEntryPoint(NULL);
      }
    }

    // globals won't be filled out by entering the entry point, ensure their change is registered.
    for(const Id &v : liveGlobals)
      initial.changes.push_back({ShaderVariable(), GetPointerValue(active.ids[v])});

    ret.push_back(initial);

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
            ret.push_back(ShaderDebugState());

          continue;
        }

        if(lane == activeLaneIndex)
        {
          ShaderDebugState state;

          // see if we're retiring any IDs at this state
          for(size_t l = 0; l < thread.live.size();)
          {
            Id id = thread.live[l];
            if(idDeathOffset[id] < instructionOffsets[thread.nextInstruction])
            {
              thread.live.erase(l);
              ShaderVariableChange change;
              change.before = GetPointerValue(thread.ids[id]);
              state.changes.push_back(change);

              rdcstr name = GetRawName(id);

              thread.sourceVars.removeIf([name](const SourceVariableMapping &var) {
                return var.variables[0].name.beginsWith(name);
              });

              continue;
            }

            l++;
          }

          thread.StepNext(&state, workgroup);
          state.stepIndex = steps;
          state.sourceVars = thread.sourceVars;
          thread.FillCallstack(state);
          ret.push_back(state);

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

ShaderVariable Debugger::MakePointerVariable(Id id, const ShaderVariable *v, uint32_t scalar0,
                                             uint32_t scalar1) const
{
  ShaderVariable var;
  var.rows = var.columns = 1;
  var.type = VarType::GPUPointer;
  var.name = GetRawName(id);
  var.value.u64v[PointerVariableSlot] = (uint64_t)(uintptr_t)v;
  var.value.u64v[Scalar0VariableSlot] = scalar0;
  var.value.u64v[Scalar1VariableSlot] = scalar1;
  var.value.u64v[BaseIdVariableSlot] = id.value();
  return var;
}

ShaderVariable Debugger::MakeCompositePointer(const ShaderVariable &base, Id id,
                                              rdcarray<uint32_t> &indices)
{
  const ShaderVariable *leaf = &base;

  // if the base is a plain value, we just start walking down the chain. If the base is a pointer
  // though, we want to step down the chain in the underlying storage, so dereference first.
  if(base.type == VarType::GPUPointer)
    leaf = (const ShaderVariable *)(uintptr_t)base.value.u64v[PointerVariableSlot];

  bool isArray = false;

  if((leaf->type == VarType::ReadWriteResource || leaf->type == VarType::ReadOnlyResource ||
      leaf->type == VarType::Sampler) &&
     leaf->value.u64v[ArrayVariableSlot])
  {
    isArray = true;
  }

  if(leaf->type == VarType::ReadWriteResource && leaf->value.u64v[SSBOVariableSlot])
  {
    ShaderVariable ret = MakePointerVariable(id, leaf);

    uint64_t byteOffset = base.value.u64v[BufferPointerByteOffsetVariableSlot];
    ret.value.u64v[MajorStrideVariableSlot] = base.value.u64v[MajorStrideVariableSlot];

    const DataType *type = &dataTypes[idTypes[id]];

    RDCASSERT(type->type == DataType::PointerType);
    type = &dataTypes[type->InnerType()];

    // first walk any aggregate types
    size_t i = 0;

    // if it's an array, consume the array index first
    if(isArray)
    {
      ret.value.u64v[ArrayVariableSlot] = indices[i++];
      type = &dataTypes[type->InnerType()];
    }

    Decorations curDecorations = decorations[type->id];

    while(i < indices.size() &&
          (type->type == DataType::ArrayType || type->type == DataType::StructType))
    {
      if(type->type == DataType::ArrayType)
      {
        // look up the array stride
        const Decorations &dec = decorations[type->id];
        RDCASSERT(dec.flags & Decorations::HasArrayStride);

        // offset increases by index * arrayStride
        byteOffset += indices[i] * dec.arrayStride;

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

    if(curDecorations.flags & Decorations::HasMatrixStride)
      ret.value.u64v[MajorStrideVariableSlot] = curDecorations.matrixStride;

    if(curDecorations.flags & Decorations::RowMajor)
      ret.value.u64v[MajorStrideVariableSlot] |= 0x80000000U;

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

    ret.value.u64v[BufferPointerTypeIdVariableSlot] = type->id.value();
    ret.value.u64v[BufferPointerByteOffsetVariableSlot] = byteOffset;

    return ret;
  }

  // first walk any struct member/array indices
  size_t i = 0;
  if(isArray)
    i++;
  while(i < indices.size() && !leaf->members.empty())
  {
    uint32_t idx = indices[i++];
    if(idx > leaf->members.size())
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
  uint32_t scalar0 = ~0U, scalar1 = ~0U;

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
    scalar0 = indices[i];
    scalar1 = indices[i + 1];
  }
  else if(remaining == 1)
  {
    scalar0 = indices[i];
  }

  ShaderVariable ret = MakePointerVariable(id, leaf, scalar0, scalar1);

  if(isArray)
    ret.value.u64v[ArrayVariableSlot] = indices[0];

  return ret;
}

ShaderVariable Debugger::GetPointerValue(const ShaderVariable &ptr) const
{
  // opaque pointers display as their inner value
  if(IsOpaquePointer(ptr))
  {
    const ShaderVariable *inner =
        (const ShaderVariable *)(uintptr_t)ptr.value.u64v[PointerVariableSlot];
    ShaderVariable ret = *inner;
    ret.name = ptr.name;
    // inherit any array index from the pointer
    BindpointIndex bind = ret.GetBinding();
    ret.SetBinding(bind.bindset, bind.bind, (uint32_t)ptr.value.u64v[ArrayVariableSlot]);
    return ret;
  }

  // every other kind of pointer displays as its contents
  return ReadFromPointer(ptr);
}

ShaderVariable Debugger::ReadFromPointer(const ShaderVariable &ptr) const
{
  if(ptr.type != VarType::GPUPointer)
    return ptr;

  const ShaderVariable *inner =
      (const ShaderVariable *)(uintptr_t)ptr.value.u64v[PointerVariableSlot];

  ShaderVariable ret;

  if(inner->type == VarType::ReadWriteResource && inner->value.u64v[SSBOVariableSlot])
  {
    rdcspv::Id typeId =
        rdcspv::Id::fromWord(uint32_t(ptr.value.u64v[BufferPointerTypeIdVariableSlot]));
    uint64_t byteOffset = ptr.value.u64v[BufferPointerByteOffsetVariableSlot];

    BindpointIndex bind = inner->GetBinding();

    bind.arrayIndex = (uint32_t)ptr.value.u64v[ArrayVariableSlot];

    uint32_t matrixStride = (uint32_t)ptr.value.u64v[MajorStrideVariableSlot];
    bool rowMajor = (matrixStride & 0x80000000U) != 0;
    matrixStride &= 0xff;

    auto readCallback = [this, bind, matrixStride, rowMajor](
        ShaderVariable &var, const Decorations &, const DataType &type, uint64_t offset,
        const rdcstr &) {

      // ignore any callbacks we get on the way up for structs/arrays, we don't need it we only read
      // or write at primitive level
      if(!var.members.empty())
        return;

      if(type.type == DataType::MatrixType)
      {
        RDCASSERT(matrixStride != 0);

        if(rowMajor)
        {
          for(uint8_t r = 0; r < var.rows; r++)
          {
            apiWrapper->ReadBufferValue(bind, offset + r * matrixStride,
                                        VarTypeByteSize(var.type) * var.columns,
                                        &var.value.uv[r * var.columns]);
          }
        }
        else
        {
          ShaderValue tmp = {};
          // read column-wise
          for(uint8_t c = 0; c < var.columns; c++)
          {
            apiWrapper->ReadBufferValue(bind, offset + c * matrixStride,
                                        VarTypeByteSize(var.type) * var.rows, &tmp.uv[c * var.rows]);
          }

          // transpose into our row major storage
          for(uint8_t r = 0; r < var.rows; r++)
          {
            for(uint8_t c = 0; c < var.columns; c++)
            {
              if(VarTypeByteSize(var.type) == 8)
                var.value.u64v[r * var.columns + c] = tmp.u64v[c * var.rows + r];
              else
                var.value.uv[r * var.columns + c] = tmp.uv[c * var.rows + r];
            }
          }
        }
      }
      else if(type.type == DataType::VectorType)
      {
        if(!rowMajor)
        {
          // we can read a vector at a time if the matrix is column major
          apiWrapper->ReadBufferValue(bind, offset, VarTypeByteSize(var.type) * var.columns,
                                      var.value.uv);
        }
        else
        {
          for(uint8_t c = 0; c < var.columns; c++)
          {
            if(VarTypeByteSize(var.type) == 8)
              apiWrapper->ReadBufferValue(bind, offset + c * matrixStride,
                                          VarTypeByteSize(var.type), &var.value.u64v[c]);
            else
              apiWrapper->ReadBufferValue(bind, offset + c * matrixStride,
                                          VarTypeByteSize(var.type), &var.value.uv[c]);
          }
        }
      }
      else if(type.type == DataType::ScalarType)
      {
        apiWrapper->ReadBufferValue(bind, offset, VarTypeByteSize(var.type), var.value.uv);
      }
    };

    WalkVariable<ShaderVariable, true>(Decorations(), dataTypes[typeId], byteOffset, ret, rdcstr(),
                                       readCallback);

    ret.name = ptr.name;
    return ret;
  }

  ret = *inner;
  ret.name = ptr.name;

  if(inner->type == VarType::ReadOnlyResource || inner->type == VarType::ReadWriteResource ||
     inner->type == VarType::Sampler)
  {
    BindpointIndex bind = ret.GetBinding();

    ret.SetBinding(bind.bindset, bind.bind, (uint32_t)ptr.value.u64v[ArrayVariableSlot]);
  }

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint32_t scalar0 = (uint32_t)ptr.value.u64v[Scalar0VariableSlot];
  uint32_t scalar1 = (uint32_t)ptr.value.u64v[Scalar1VariableSlot];

  ShaderValue val;

  if(ret.rows > 1)
  {
    // matrix case
    ClampScalars(apiWrapper, ret, scalar0, scalar1);

    if(scalar0 != ~0U && scalar1 != ~0U)
    {
      // two indices - selecting a scalar. scalar0 is the first index in the chain so it chooses
      // column
      if(VarTypeByteSize(ret.type) == 8)
        val.u64v[0] = ret.value.u64v[scalar1 * ret.columns + scalar0];
      else
        val.uv[0] = ret.value.uv[scalar1 * ret.columns + scalar0];

      // it's a scalar now, even if it was a matrix before
      ret.rows = ret.columns = 1;
      ret.value = val;
    }
    else if(scalar0 != ~0U)
    {
      // one index, selecting a column
      for(uint32_t row = 0; row < ret.rows; row++)
      {
        if(VarTypeByteSize(ret.type) == 8)
          val.u64v[row] = ret.value.u64v[row * ret.columns + scalar0];
        else
          val.uv[row] = ret.value.uv[row * ret.columns + scalar0];
      }

      // it's a vector now, even if it was a matrix before
      ret.rows = 1;
      ret.value = val;
    }
  }
  else
  {
    ClampScalars(apiWrapper, ret, scalar0);

    // vector case, selecting a scalar (if anything)
    if(scalar0 != ~0U)
    {
      if(VarTypeByteSize(ret.type) == 8)
        val.u64v[0] = ret.value.u64v[scalar0];
      else
        val.uv[0] = ret.value.uv[scalar0];

      // it's a scalar now, even if it was a matrix before
      ret.columns = 1;
      ret.value = val;
    }
  }

  return ret;
}

Id Debugger::GetPointerBaseId(const ShaderVariable &ptr) const
{
  RDCASSERT(ptr.type == VarType::GPUPointer);

  // we stored the base ID so that it's always available regardless of access chains
  return Id::fromWord((uint32_t)ptr.value.u64v[BaseIdVariableSlot]);
}

bool Debugger::IsOpaquePointer(const ShaderVariable &ptr) const
{
  if(ptr.type != VarType::GPUPointer)
    return false;

  ShaderVariable *inner = (ShaderVariable *)(uintptr_t)ptr.value.u64v[PointerVariableSlot];
  return inner->type == VarType::ReadOnlyResource || inner->type == VarType::Sampler ||
         inner->type == VarType::ReadWriteResource;
}

bool Debugger::ArePointersAndEqual(const ShaderVariable &a, const ShaderVariable &b) const
{
  // we can do a pointer comparison by checking the values, since we store all pointer-related
  // data in there
  if(a.type == VarType::GPUPointer && b.type == VarType::GPUPointer)
    return memcmp(&a.value, &b.value, sizeof(ShaderValue)) == 0;

  return false;
}

void Debugger::WriteThroughPointer(const ShaderVariable &ptr, const ShaderVariable &val)
{
  ShaderVariable *storage = (ShaderVariable *)(uintptr_t)ptr.value.u64v[PointerVariableSlot];

  if(storage->type == VarType::ReadWriteResource)
  {
    rdcspv::Id typeId =
        rdcspv::Id::fromWord(uint32_t(ptr.value.u64v[BufferPointerTypeIdVariableSlot]));
    uint64_t byteOffset = ptr.value.u64v[BufferPointerByteOffsetVariableSlot];

    const DataType &type = dataTypes[typeId];

    BindpointIndex bind = storage->GetBinding();

    bind.arrayIndex = (uint32_t)ptr.value.u64v[ArrayVariableSlot];
    uint32_t matrixStride = (uint32_t)ptr.value.u64v[MajorStrideVariableSlot];
    bool rowMajor = (matrixStride & 0x80000000U) != 0;
    matrixStride &= 0xff;

    auto writeCallback = [this, bind, matrixStride, rowMajor](
        const ShaderVariable &var, const Decorations &, const DataType &type, uint64_t offset,
        const rdcstr &) {
      if(!var.members.empty())
        return;

      if(type.type == DataType::MatrixType)
      {
        RDCASSERT(matrixStride != 0);

        if(rowMajor)
        {
          for(uint8_t r = 0; r < var.rows; r++)
          {
            apiWrapper->WriteBufferValue(bind, offset + r * matrixStride,
                                         VarTypeByteSize(var.type) * var.columns,
                                         &var.value.uv[r * var.columns]);
          }
        }
        else
        {
          ShaderValue tmp = {};

          // transpose from our row major storage
          for(uint8_t r = 0; r < var.rows; r++)
          {
            for(uint8_t c = 0; c < var.columns; c++)
            {
              if(VarTypeByteSize(var.type) == 8)
                tmp.u64v[c * var.rows + r] = var.value.u64v[r * var.columns + c];
              else
                tmp.uv[c * var.rows + r] = var.value.uv[r * var.columns + c];
            }
          }

          // read column-wise
          for(uint8_t c = 0; c < var.columns; c++)
          {
            apiWrapper->WriteBufferValue(bind, offset + c * matrixStride,
                                         VarTypeByteSize(var.type) * var.rows, &tmp.uv[c * var.rows]);
          }
        }
      }
      else if(type.type == DataType::VectorType)
      {
        if(!rowMajor)
        {
          // we can write a vector at a time if the matrix is column major
          apiWrapper->WriteBufferValue(bind, offset, VarTypeByteSize(var.type) * var.columns,
                                       var.value.uv);
        }
        else
        {
          for(uint8_t c = 0; c < var.columns; c++)
          {
            if(VarTypeByteSize(var.type) == 8)
              apiWrapper->WriteBufferValue(bind, offset + c * matrixStride,
                                           VarTypeByteSize(var.type), &var.value.u64v[c]);
            else
              apiWrapper->WriteBufferValue(bind, offset + c * matrixStride,
                                           VarTypeByteSize(var.type), &var.value.uv[c]);
          }
        }
      }
      else if(type.type == DataType::ScalarType)
      {
        apiWrapper->WriteBufferValue(bind, offset, VarTypeByteSize(var.type), var.value.uv);
      }
    };

    WalkVariable<const ShaderVariable, false>(Decorations(), dataTypes[typeId], byteOffset, val,
                                              rdcstr(), writeCallback);

    return;
  }

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint32_t scalar0 = (uint32_t)ptr.value.u64v[Scalar0VariableSlot];
  uint32_t scalar1 = (uint32_t)ptr.value.u64v[Scalar1VariableSlot];

  // in the common case we don't have scalar selectors. In this case just assign the value
  if(scalar0 == ~0U && scalar1 == ~0U)
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

      if(scalar0 != ~0U && scalar1 != ~0U)
      {
        // two indices - selecting a scalar. scalar0 is the first index in the chain so it chooses
        // column
        if(VarTypeByteSize(storage->type) == 8)
          storage->value.u64v[scalar1 * storage->columns + scalar0] = val.value.u64v[0];
        else
          storage->value.uv[scalar1 * storage->columns + scalar0] = val.value.uv[0];
      }
      else if(scalar0 != ~0U)
      {
        // one index, selecting a column
        for(uint32_t row = 0; row < storage->rows; row++)
        {
          if(VarTypeByteSize(storage->type) == 8)
            storage->value.u64v[row * storage->columns + scalar0] = val.value.u64v[row];
          else
            storage->value.uv[row * storage->columns + scalar0] = val.value.uv[row];
        }
      }
    }
    else
    {
      ClampScalars(apiWrapper, *storage, scalar0);

      // vector case, selecting a scalar
      if(VarTypeByteSize(storage->type) == 8)
        storage->value.u64v[scalar0] = val.value.u64v[0];
      else
        storage->value.uv[scalar0] = val.value.uv[0];
    }
  }
}

rdcstr Debugger::GetRawName(Id id) const
{
  return StringFormat::Fmt("_%u", id.value());
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

void Debugger::AddSourceVars(rdcarray<SourceVariableMapping> &sourceVars, const ShaderVariable &var,
                             Id id)
{
  rdcstr name;

  auto it = dynamicNames.find(id);
  if(it != dynamicNames.end())
    name = it->second;
  else
    name = strings[id];

  if(!name.empty())
  {
    Id type = idTypes[id];

    SourceVariableMapping sourceVar;

    sourceVar.name = name;
    sourceVar.offset = 0;
    sourceVar.type = var.type;
    sourceVar.rows = RDCMAX(1U, (uint32_t)var.rows);
    sourceVar.columns = RDCMAX(1U, (uint32_t)var.columns);
    for(uint32_t x = 0; x < sourceVar.rows * sourceVar.columns; x++)
      sourceVar.variables.push_back(DebugVariableReference(DebugVariableType::Variable, var.name, x));

    sourceVars.push_back(sourceVar);
  }
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
    memset(var.value.u64v, 0xcc, sizeof(var.value.u64v));
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
        outVar->rows = 1;
        outVar->columns = RDCMAX(1U, type.vector().count);
      }
      numLocations = 1;
      break;
    }
    case DataType::MatrixType:
    {
      if(outVar)
      {
        outVar->type = type.scalar().Type();
        outVar->columns = RDCMAX(1U, type.matrix().count);
        outVar->rows = RDCMAX(1U, type.vector().count);
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

      ShaderVariable len = GetActiveLane().ids[type.length];
      for(uint32_t i = 0; i < len.value.u.x; i++)
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
    case DataType::ImageType:
    case DataType::SamplerType:
    case DataType::SampledImageType:
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

uint32_t Debugger::ApplyDerivatives(uint32_t quadIndex, const Decorations &curDecorations,
                                    uint32_t location, const DataType &inType, ShaderVariable &outVar)
{
  // only floats have derivatives
  if(outVar.type == VarType::Float)
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

    if(curDecorations.flags & Decorations::HasLocation)
      location = curDecorations.location;

    DebugAPIWrapper::DerivativeDeltas derivs =
        apiWrapper->GetDerivative(builtin, location, component);

    Vec4f &dst = *(Vec4f *)outVar.value.fv;

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
          case 1: dst += derivs.ddxcoarse; break;
          case 2: dst += derivs.ddycoarse; break;
          case 3:
            dst += derivs.ddxcoarse;
            dst += derivs.ddycoarse;
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
          case 0: dst -= derivs.ddxcoarse; break;
          case 1: break;
          case 2:
            dst -= derivs.ddxcoarse;
            dst += derivs.ddycoarse;
            break;
          case 3: dst += derivs.ddyfine; break;
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
          case 0: dst -= derivs.ddycoarse; break;
          case 1:
            dst -= derivs.ddycoarse;
            dst += derivs.ddxcoarse;
            break;
          case 2: break;
          case 3: dst += derivs.ddxfine; break;
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
            dst -= derivs.ddyfine;
            dst -= derivs.ddxcoarse;
            break;
          case 1: dst -= derivs.ddyfine; break;
          case 2: dst -= derivs.ddxfine; break;
          case 3: break;
          default: break;
        }
        break;
      }
      default: break;
    }
  }

  // each row consumes a new location
  return outVar.rows;
}

void Debugger::PreParse(uint32_t maxId)
{
  Processor::PreParse(maxId);

  strings.resize(idTypes.size());
}

void Debugger::PostParse()
{
  Processor::PostParse();

  for(const MemberName &mem : memberNames)
    dataTypes[mem.id].children[mem.member].name = mem.name;

  // global IDs never hit a death point
  for(const Variable &v : globals)
    idDeathOffset[v.id] = ~0U;

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
    idDeathOffset[id] = RDCMAX(it.offs() + 1, idDeathOffset[id]);
  });

  if(opdata.op == Op::ExtInst)
  {
    OpExtInst extinst(it);

    if(extSets[extinst.set] == "GLSL.std.450")
    {
      // all parameters to GLSL.std.450 are Ids, extend idDeathOffset appropriately
      for(const uint32_t param : extinst.params)
      {
        Id id = Id::fromWord(param);
        idDeathOffset[id] = RDCMAX(it.offs() + 1, idDeathOffset[id]);
      }
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

    m_CurLineCol.lineStart = line.line;
    m_CurLineCol.lineEnd = line.line;
    m_CurLineCol.colStart = line.column;
    m_CurLineCol.fileIndex = (int32_t)m_Files[line.file];
  }
  else if(opdata.op == Op::NoLine)
  {
    m_CurLineCol = LineColumnInfo();
  }
  else
  {
    m_LineColInfo[it.offs()] = m_CurLineCol;
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

    entryLookup[entryPoint.name] = entryPoint.entryPoint;
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
    if(strings[var.result].empty() && dataTypes[varType].type == DataType::StructType &&
       !strings[varType].empty())
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
    // don't automatically kill function parameters and variables. They will be manually killed when
    // returning from a function's scope
    for(const Id id : curFunction->parameters)
      idDeathOffset[id] = ~0U;
    for(const Id id : curFunction->variables)
      idDeathOffset[id] = ~0U;

    curFunction = NULL;
  }
}

};    // namespace rdcspv
