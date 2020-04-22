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
        name += StringFormat::Fmt("._child%u", chain);
        type = &dataTypes[type->children[chain].type];
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

ShaderDebugTrace *Debugger::BeginDebug(DebugAPIWrapper *apiWrapper, const ShaderStage stage,
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
      case Capability::TessellationPointSize:
      case Capability::GeometryPointSize:
      case Capability::ImageGatherExtended:
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
      {
        supported = true;
        break;
      }

      // we plan to support these but needs additional testing/proving

      // image storage
      case Capability::StorageImageMultisample:

      // all these are related to non-32-bit types
      case Capability::Float16Buffer:
      case Capability::Float16:
      case Capability::Float64:
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
      case Capability::AtomicStorage:
      case Capability::AtomicStorageOps:

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
      {
        supported = false;
        break;
      }

      // input attachments
      case Capability::InputAttachment:

      // sparse operations
      case Capability::SparseResidency:

      // fragment interlock
      case Capability::FragmentShaderSampleInterlockEXT:
      case Capability::FragmentShaderShadingRateInterlockEXT:
      case Capability::FragmentShaderPixelInterlockEXT:

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
      RDCERR("Unsupported capability '%s'", ToStr(c).c_str());
      return new ShaderDebugTrace;
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
    else
    {
      RDCERR("Unsupported extended instruction set: %s", setname.c_str());
      return new ShaderDebugTrace;
    }
  }

  for(const rdcstr &e : extensions)
  {
    if(e == "SPV_GOOGLE_decorate_string" || e == "SPV_GOOGLE_hlsl_functionality1")
    {
      // supported extensions
    }
    else
    {
      RDCERR("Unsupported extension '%s'", e.c_str());
      return new ShaderDebugTrace;
    }
  }

  ShaderDebugTrace *ret = new ShaderDebugTrace;
  ret->debugger = this;
  ret->stage = stage;
  this->activeLaneIndex = activeIndex;
  this->stage = stage;
  this->apiWrapper = apiWrapper;

  uint32_t workgroupSize = stage == ShaderStage::Pixel ? 4 : 1;
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

  rdcarray<Id> inputIDs, outputIDs, cbufferIDs, readOnlyIDs, readWriteIDs, samplerIDs, privateIDs;

  // allocate storage for globals with opaque storage classes, and prepare to set up pointers to
  // them for the global variables themselves
  for(const Variable &v : globals)
  {
    if(v.storage == StorageClass::Input || v.storage == StorageClass::Output)
    {
      const bool isInput = (v.storage == StorageClass::Input);

      ShaderVariable var;
      var.name = GetRawName(v.id);

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
      }

      size_t oldSize = globalSourceVars.size();

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      // fill the interface variable
      AllocateVariable(decorations[v.id], decorations[v.id],
                       isInput ? DebugVariableType::Input : DebugVariableType::Variable, sourceName,
                       decorations[v.id].location, dataTypes[type.InnerType()], var);

      for(size_t i = oldSize; i < globalSourceVars.size(); i++)
        globalSourceVars[i].signatureIndex =
            (isInput ? inputSigNames : outputSigNames).indexOf(globalSourceVars[i].variables[0].name);

      if(isInput)
      {
        // create the opaque storage
        active.inputs.push_back(var);

        // then make sure we know which ID to set up for the pointer
        inputIDs.push_back(v.id);
      }
      else
      {
        active.outputs.push_back(var);
        outputIDs.push_back(v.id);
      }
    }

    // pick up uniform globals, which could be cbuffers, and push constants
    else if((v.storage == StorageClass::Uniform || v.storage == StorageClass::PushConstant) &&
            (decorations[v.id].flags & Decorations::BufferBlock) == 0)
    {
      ShaderVariable var;
      var.name = GetRawName(v.id);

      rdcstr sourceName = GetHumanName(v.id);

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      const DataType &innertype = dataTypes[type.InnerType()];

      if(innertype.type == DataType::ArrayType)
      {
        RDCERR("uniform Arrays not supported yet");
      }
      else if(innertype.type == DataType::StructType)
      {
        // if we don't have a good human name, generate a better one using the interface information
        // we have
        if(sourceName == var.name)
        {
          if(v.storage == StorageClass::PushConstant)
            sourceName = "_pushconsts";
          else
            sourceName = StringFormat::Fmt("_cbuffer_set%u_bind%u", decorations[v.id].set,
                                           decorations[v.id].binding);
        }

        Decorations d = decorations[v.id];

        if(v.storage == StorageClass::PushConstant)
        {
          d.set = PushConstantBindSet;
          d.flags = Decorations::Flags(d.flags | Decorations::HasDescriptorSet);
        }

        uint32_t offset = 0;
        AllocateVariable(d, d, DebugVariableType::Constant, sourceName, 0, innertype, var);

        global.constantBlocks.push_back(var);
        cbufferIDs.push_back(v.id);

        SourceVariableMapping sourceVar;
        sourceVar.name = sourceName;
        sourceVar.type = VarType::Unknown;
        sourceVar.rows = 0;
        sourceVar.columns = 0;
        sourceVar.offset = 0;
        sourceVar.variables.push_back(DebugVariableReference(DebugVariableType::Constant, var.name));

        globalSourceVars.push_back(sourceVar);
      }
      else
      {
        RDCERR("Unhandled type of uniform: %u", innertype.type);
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

      const DataType &innertype = dataTypes[type.InnerType()];

      // if we don't have a good human name, generate a better one using the interface information
      // we have
      if(sourceName == var.name)
      {
        rdcstr innerName;
        if(innertype.type == DataType::SamplerType)
          innerName = "sampler";
        else if(innertype.type == DataType::SampledImageType)
          innerName = "sampledImage";
        else if(innertype.type == DataType::ImageType)
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

      // TODO handle arrays
      var.SetBinding((int32_t)set, (int32_t)bind, 0U);

      if(innertype.type == DataType::SamplerType)
      {
        var.type = VarType::Sampler;
        debugType = DebugVariableType::Sampler;

        global.samplers.push_back(var);
        samplerIDs.push_back(v.id);
      }
      else if(innertype.type == DataType::SampledImageType || innertype.type == DataType::ImageType)
      {
        var.type = VarType::ReadOnlyResource;
        debugType = DebugVariableType::ReadOnlyResource;

        // store the texture type here, since the image may be copied around and combined with a
        // sampler, so accessing the original type might be non-trivial at point of access
        uint32_t texType = DebugAPIWrapper::Float_Texture;

        Id imgid = type.InnerType();

        if(innertype.type == DataType::SampledImageType)
          imgid = sampledImageTypes[imgid].baseId;

        if(imageTypes[imgid].dim == Dim::Buffer)
          texType |= DebugAPIWrapper::Buffer_Texture;

        if(imageTypes[imgid].retType.type == Op::TypeInt)
        {
          if(imageTypes[imgid].retType.signedness)
            texType |= DebugAPIWrapper::SInt_Texture;
          else
            texType |= DebugAPIWrapper::UInt_Texture;
        }

        var.value.uv[TextureTypeVariableSlot] = texType;

        global.readOnlyResources.push_back(var);
        readOnlyIDs.push_back(v.id);
      }
      else
      {
        RDCERR("Unhandled type of uniform: %u", innertype.type);
      }

      SourceVariableMapping sourceVar;
      sourceVar.name = sourceName;
      sourceVar.type = var.type;
      sourceVar.rows = 1;
      sourceVar.columns = 1;
      sourceVar.offset = 0;
      sourceVar.variables.push_back(DebugVariableReference(debugType, var.name));

      globalSourceVars.push_back(sourceVar);
    }
    else if(v.storage == StorageClass::Private)
    {
      // private variables are allocated as globals the same as outputs
      ShaderVariable var;
      var.name = GetRawName(v.id);

      rdcstr sourceName = GetHumanName(v.id);

      const DataType &type = dataTypes[v.type];

      // global variables should all be pointers into opaque storage
      RDCASSERT(type.type == DataType::PointerType);

      // fill the interface variable
      AllocateVariable(decorations[v.id], decorations[v.id], DebugVariableType::Variable,
                       sourceName, decorations[v.id].location, dataTypes[type.InnerType()], var);

      active.privates.push_back(var);
      privateIDs.push_back(v.id);
    }
    else
    {
      RDCERR("Unhandled type of global variable: %s", ToStr(v.storage).c_str());
    }
  }

  std::sort(outputIDs.begin(), outputIDs.end());
  std::sort(privateIDs.begin(), privateIDs.end());

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
    for(size_t i = 0; i < lane.inputs.size(); i++)
      lane.ids[inputIDs[i]] = MakePointerVariable(inputIDs[i], &lane.inputs[i]);
    for(size_t i = 0; i < lane.outputs.size(); i++)
      lane.ids[outputIDs[i]] = MakePointerVariable(outputIDs[i], &lane.outputs[i]);
    for(size_t i = 0; i < global.constantBlocks.size(); i++)
      lane.ids[cbufferIDs[i]] = MakePointerVariable(cbufferIDs[i], &global.constantBlocks[i]);
    for(size_t i = 0; i < global.readOnlyResources.size(); i++)
      lane.ids[readOnlyIDs[i]] = MakePointerVariable(readOnlyIDs[i], &global.readOnlyResources[i]);
    for(size_t i = 0; i < global.readWriteResources.size(); i++)
      lane.ids[readWriteIDs[i]] = MakePointerVariable(readWriteIDs[i], &global.readWriteResources[i]);
    for(size_t i = 0; i < global.samplers.size(); i++)
      lane.ids[samplerIDs[i]] = MakePointerVariable(samplerIDs[i], &global.samplers[i]);
    for(size_t i = 0; i < lane.privates.size(); i++)
      lane.ids[privateIDs[i]] = MakePointerVariable(privateIDs[i], &lane.privates[i]);
  }

  // only outputs and privates are considered mutable
  liveGlobals.append(outputIDs);
  liveGlobals.append(privateIDs);

  for(size_t i = 0; i < globalSourceVars.size();)
  {
    if(!globalSourceVars[i].variables.empty() &&
       (globalSourceVars[i].variables[0].type == DebugVariableType::Input ||
        globalSourceVars[i].variables[0].type == DebugVariableType::ReadOnlyResource ||
        globalSourceVars[i].variables[0].type == DebugVariableType::ReadWriteResource ||
        globalSourceVars[i].variables[0].type == DebugVariableType::Sampler ||
        globalSourceVars[i].variables[0].type == DebugVariableType::Constant))
    {
      ret->sourceVars.push_back(globalSourceVars[i]);
      globalSourceVars.erase(i);
      continue;
    }
    i++;
  }

  for(size_t o = 0; o < outputIDs.size(); o++)
  {
    rdcstr varName = GetRawName(outputIDs[o]);

    for(size_t i = 0; i < globalSourceVars.size(); i++)
    {
      if(!globalSourceVars[i].variables.empty() && globalSourceVars[i].variables[0].name == varName)
      {
        ret->sourceVars.push_back(globalSourceVars[i]);
        break;
      }
    }
  }

  for(size_t o = 0; o < privateIDs.size(); o++)
  {
    rdcstr varName = GetRawName(privateIDs[o]);

    for(size_t i = 0; i < globalSourceVars.size(); i++)
    {
      if(!globalSourceVars[i].variables.empty() && globalSourceVars[i].variables[0].name == varName)
      {
        ret->sourceVars.push_back(globalSourceVars[i]);
        break;
      }
    }
  }

  ret->lineInfo.resize(instructionOffsets.size());
  for(size_t i = 0; i < instructionOffsets.size(); i++)
  {
    auto it = instructionLines.find(instructionOffsets[i]);
    if(it != instructionLines.end())
      ret->lineInfo[i].disassemblyLine = it->second;
    else
      ret->lineInfo[i].disassemblyLine = 0;
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

        ApplyDerivatives(q, decorations[id], decorations[id].location, innertype,
                         workgroup[q].inputs[i]);
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
    // we should be sitting at the entry point function prologue, step forward into the first block
    // and past any function-local variable declarations
    for(ThreadState &thread : workgroup)
      thread.EnterFunction({});

    ShaderDebugState initial;

    initial.nextInstruction = active.nextInstruction;

    for(const Id &v : active.live)
      initial.changes.push_back({ShaderVariable(), EvaluatePointerVariable(active.ids[v])});

    initial.sourceVars = active.sourceVars;

    initial.stepIndex = steps;

    active.FillCallstack(initial);

    ret.push_back(initial);

    steps++;
  }

  // if we've finished, return an empty set to signify that
  if(active.Finished())
    return ret;

  rdcarray<bool> activeMask;

  // do 100 in a chunk
  for(int cycleCounter = 0; cycleCounter < 100; cycleCounter++)
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
              change.before = EvaluatePointerVariable(thread.ids[id]);
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
        }
        else
        {
          thread.StepNext(NULL, workgroup);
        }
      }
    }

    steps++;
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
  // encode the pointer into the first u64v
  var.value.u64v[0] = (uint64_t)(uintptr_t)v;

  // uv[1] overlaps with u64v[0], so start from [2] storing scalar indices
  var.value.uv[2] = scalar0;
  var.value.uv[3] = scalar1;
  // store the base ID of the allocated storage in [4]
  var.value.uv[4] = id.value();
  return var;
}

ShaderVariable Debugger::MakeCompositePointer(const ShaderVariable &base, Id id,
                                              rdcarray<uint32_t> &indices)
{
  const ShaderVariable *leaf = &base;

  // if the base is a plain value, we just start walking down the chain. If the base is a pointer
  // though, we want to step down the chain in the underlying storage, so dereference first.
  if(base.type == VarType::GPUPointer)
    leaf = (const ShaderVariable *)(uintptr_t)base.value.u64v[0];

  // first walk any struct member/array indices
  size_t i = 0;
  while(!leaf->members.empty())
  {
    RDCASSERT(i < indices.size(), i, indices.size());
    leaf = &leaf->members[indices[i++]];
  }

  // apply any remaining scalar selectors
  uint32_t scalar0 = ~0U, scalar1 = ~0U;

  size_t remaining = indices.size() - i;
  if(remaining == 2)
  {
    scalar0 = indices[i];
    scalar1 = indices[i + 1];
  }
  else if(remaining == 1)
  {
    scalar0 = indices[i];
  }

  return MakePointerVariable(id, leaf, scalar0, scalar1);
}

ShaderVariable Debugger::EvaluatePointerVariable(const ShaderVariable &ptr) const
{
  if(ptr.type != VarType::GPUPointer)
    return ptr;

  ShaderVariable ret;
  ret = *(const ShaderVariable *)(uintptr_t)ptr.value.u64v[0];
  ret.name = ptr.name;

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint32_t scalar0 = ptr.value.uv[2];
  uint32_t scalar1 = ptr.value.uv[3];

  ShaderValue val;

  if(ret.rows > 1)
  {
    // matrix case

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
          val.u64v[0] = ret.value.u64v[row * ret.columns + scalar0];
        else
          val.uv[0] = ret.value.uv[row * ret.columns + scalar0];
      }

      // it's a vector now, even if it was a matrix before
      ret.rows = 1;
      ret.value = val;
    }
  }
  else
  {
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

  // we stored the base ID in [4] so that it's always available regardless of access chains
  return Id::fromWord(ptr.value.uv[4]);
}

void Debugger::WriteThroughPointer(const ShaderVariable &ptr, const ShaderVariable &val)
{
  ShaderVariable *storage = (ShaderVariable *)(uintptr_t)ptr.value.u64v[0];

  // we don't support pointers to scalars since our 'unit' of pointer is a ShaderVariable, so check
  // if we have scalar indices to apply:
  uint32_t scalar0 = ptr.value.uv[2];
  uint32_t scalar1 = ptr.value.uv[3];

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
            storage->value.u64v[row * storage->columns + scalar0] = val.value.u64v[0];
          else
            storage->value.uv[row * storage->columns + scalar0] = val.value.uv[0];
        }
      }
    }
    else
    {
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

void Debugger::AddSourceVars(rdcarray<SourceVariableMapping> &sourceVars, Id id)
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

    uint32_t offset = 0;
    AddSourceVars(sourceVars, dataTypes[type], name, GetRawName(id), offset);
  }
}

void Debugger::AddSourceVars(rdcarray<SourceVariableMapping> &sourceVars, const DataType &inType,
                             const rdcstr &sourceName, const rdcstr &varName, uint32_t &offset)
{
  SourceVariableMapping sourceVar;

  switch(inType.type)
  {
    case DataType::UnknownType:
    case DataType::ImageType:
    case DataType::SamplerType:
    case DataType::SampledImageType: return;
    case DataType::PointerType:
    {
      // step silently into pointers
      AddSourceVars(sourceVars, dataTypes[inType.InnerType()], sourceName, varName, offset);
      return;
    }
    case DataType::ScalarType:
    {
      sourceVar.type = inType.scalar().Type();
      sourceVar.rows = 1;
      sourceVar.columns = 1;
      break;
    }
    case DataType::VectorType:
    {
      sourceVar.type = inType.scalar().Type();
      sourceVar.rows = 1;
      sourceVar.columns = RDCMAX(1U, inType.vector().count);
      break;
    }
    case DataType::MatrixType:
    {
      sourceVar.type = inType.scalar().Type();
      sourceVar.columns = RDCMAX(1U, inType.matrix().count);
      sourceVar.rows = RDCMAX(1U, inType.vector().count);
      break;
    }
    case DataType::StructType:
    {
      for(int32_t i = 0; i < inType.children.count(); i++)
      {
        rdcstr childVarName = StringFormat::Fmt("%s._child%d", varName.c_str(), i);

        rdcstr childSourceName;
        if(inType.children[i].name.empty())
          childSourceName = StringFormat::Fmt("%s._child%d", sourceName.c_str(), i);
        else
          childSourceName = sourceName + "." + inType.children[i].name;

        AddSourceVars(sourceVars, dataTypes[inType.children[i].type], childSourceName, childVarName,
                      offset);
      }
      return;
    }
    case DataType::ArrayType:
    {
      ShaderVariable len = GetActiveLane().ids[inType.length];
      for(uint32_t i = 0; i < len.value.u.x; i++)
      {
        rdcstr idx = StringFormat::Fmt("[%u]", i);

        AddSourceVars(sourceVars, dataTypes[inType.InnerType()], sourceName + idx, varName + idx,
                      offset);
      }
      return;
    }
  }

  sourceVar.name = sourceName;
  sourceVar.offset = offset;
  for(uint32_t x = 0; x < sourceVar.rows * sourceVar.columns; x++)
    sourceVar.variables.push_back(DebugVariableReference(DebugVariableType::Variable, varName, x));

  sourceVars.push_back(sourceVar);

  offset++;
}

void Debugger::CalcActiveMask(rdcarray<bool> &activeMask)
{
  // one bool per workgroup thread
  activeMask.resize(workgroup.size());

  // start as active, then if necessary turn off threads that are running diverged
  for(bool &active : activeMask)
    active = true;

  // only pixel shaders automatically converge workgroups, compute shaders need explicit sync
  if(stage != ShaderStage::Pixel)
    return;

  // TODO handle diverging control flow
}

void Debugger::AllocateVariable(Id id, Id typeId, DebugVariableType sourceVarType,
                                const rdcstr &sourceName, ShaderVariable &outVar)
{
  // allocs should always be pointers
  RDCASSERT(dataTypes[typeId].type == DataType::PointerType);

  AllocateVariable(decorations[id], decorations[id], sourceVarType, sourceName, 0,
                   dataTypes[dataTypes[typeId].InnerType()], outVar);
}

uint32_t Debugger::AllocateVariable(const Decorations &varDecorations,
                                    const Decorations &curDecorations,
                                    DebugVariableType sourceVarType, const rdcstr &sourceName,
                                    uint32_t offset, const DataType &inType, ShaderVariable &outVar)
{
  const bool genLocations = (varDecorations.flags & Decorations::HasLocation) != 0;

  switch(inType.type)
  {
    case DataType::PointerType:
    {
      RDCERR("Pointers not supported in interface variables");
      return 0;
    }
    case DataType::ScalarType:
    {
      outVar.type = inType.scalar().Type();
      outVar.rows = 1;
      outVar.columns = 1;
      break;
    }
    case DataType::VectorType:
    {
      outVar.type = inType.scalar().Type();
      outVar.rows = 1;
      outVar.columns = RDCMAX(1U, inType.vector().count);
      break;
    }
    case DataType::MatrixType:
    {
      outVar.type = inType.scalar().Type();
      outVar.columns = RDCMAX(1U, inType.matrix().count);
      outVar.rows = RDCMAX(1U, inType.vector().count);
      break;
    }
    case DataType::StructType:
    {
      uint32_t location = 0;
      for(int32_t i = 0; i < inType.children.count(); i++)
      {
        ShaderVariable var;
        var.name = StringFormat::Fmt("%s._child%d", outVar.name.c_str(), i);

        rdcstr childName;
        if(inType.children[i].name.empty())
          childName = StringFormat::Fmt("%s._child%d", sourceName.c_str(), i);
        else
          childName = sourceName + "." + inType.children[i].name;

        uint32_t childOffset = offset;

        const Decorations &childDecorations = inType.children[i].decorations;

        if(childDecorations.flags & Decorations::HasOffset)
          childOffset += childDecorations.offset;

        uint32_t locations =
            AllocateVariable(varDecorations, childDecorations, sourceVarType, childName,
                             location + childOffset, dataTypes[inType.children[i].type], var);

        if(genLocations)
          location += locations;

        var.name = StringFormat::Fmt("_child%d", i);

        outVar.members.push_back(var);
      }
      return location;
    }
    case DataType::ArrayType:
    {
      // array stride is decorated on the type, not the member itself
      const Decorations &typeDecorations = decorations[inType.id];

      uint32_t location = 0;

      ShaderVariable len = GetActiveLane().ids[inType.length];
      for(uint32_t i = 0; i < len.value.u.x; i++)
      {
        rdcstr idx = StringFormat::Fmt("[%u]", i);
        ShaderVariable var;
        var.name = outVar.name + idx;
        uint32_t locations =
            AllocateVariable(varDecorations, curDecorations, sourceVarType, sourceName + idx,
                             location + offset, dataTypes[inType.InnerType()], var);

        if(genLocations)
          location += locations;

        var.name = idx;

        if(typeDecorations.flags & Decorations::HasArrayStride)
          offset += typeDecorations.arrayStride;

        outVar.members.push_back(var);
      }
      return location;
    }
    case DataType::ImageType:
    case DataType::SamplerType:
    case DataType::SampledImageType:
    case DataType::UnknownType:
    {
      RDCERR("Unexpected variable type %d", inType.type);
      break;
    }
  }

  if(sourceVarType == DebugVariableType::Undefined)
    return 0;

  SourceVariableMapping sourceVar;
  sourceVar.name = sourceName;
  sourceVar.offset = offset;
  sourceVar.type = outVar.type;
  sourceVar.rows = outVar.rows;
  sourceVar.columns = outVar.columns;
  for(uint32_t x = 0; x < uint32_t(outVar.rows) * outVar.columns; x++)
    sourceVar.variables.push_back(DebugVariableReference(sourceVarType, outVar.name, x));

  ShaderBuiltin builtin = ShaderBuiltin::Undefined;
  if(curDecorations.flags & Decorations::HasBuiltIn)
    builtin = MakeShaderBuiltin(stage, curDecorations.builtIn);

  globalSourceVars.push_back(sourceVar);

  if(sourceVarType == DebugVariableType::Input)
  {
    uint32_t location = genLocations ? offset : 0;

    uint32_t component = 0;
    for(const DecorationAndParamData &dec : curDecorations.others)
    {
      if(dec.value == Decoration::Component)
      {
        component = dec.component;
        break;
      }
    }

    apiWrapper->FillInputValue(
        outVar, builtin,
        (curDecorations.flags & Decorations::HasLocation) ? curDecorations.location : location,
        component);
  }
  else if(sourceVarType == DebugVariableType::Constant)
  {
    uint32_t set = 0, bind = 0;
    if(varDecorations.flags & Decorations::HasDescriptorSet)
      set = varDecorations.set;
    if(varDecorations.flags & Decorations::HasBinding)
      bind = varDecorations.binding;

    // non-matrix case is simple, just read the size of the variable
    if(sourceVar.rows == 1)
    {
      apiWrapper->ReadConstantBufferValue(set, bind, offset, VarByteSize(outVar), outVar.value.uv);
    }
    else
    {
      // matrix case is more complicated. Either read column by column or row by row depending on
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

        uint32_t colSize = VarTypeByteSize(sourceVar.type) * sourceVar.rows;
        for(uint32_t c = 0; c < sourceVar.columns; c++)
        {
          // read the column
          apiWrapper->ReadConstantBufferValue(set, bind, offset + c * matrixStride, colSize,
                                              &tmp.uv[0]);

          // now write it into the appropiate elements in the destination ShaderValue
          for(uint32_t r = 0; r < sourceVar.rows; r++)
            outVar.value.uv[r * sourceVar.columns + c] = tmp.uv[r];
        }
      }
      else
      {
        // row major is easier, read row-by-row directly into the output variable
        uint32_t rowSize = VarTypeByteSize(sourceVar.type) * sourceVar.columns;
        for(uint32_t r = 0; r < sourceVar.rows; r++)
        {
          // read the column into the destination ShaderValue, which is tightly packed with rows
          apiWrapper->ReadConstantBufferValue(set, bind, offset + r * matrixStride, rowSize,
                                              &outVar.value.uv[r * sourceVar.columns]);
        }
      }
    }
  }

  // each row consumes a new location
  return outVar.rows;
}

uint32_t Debugger::ApplyDerivatives(uint32_t quadIndex, const Decorations &curDecorations,
                                    uint32_t location, const DataType &inType, ShaderVariable &outVar)
{
  switch(inType.type)
  {
    case DataType::PointerType:
    {
      RDCERR("Pointers not supported in interface variables");
      return 0;
    }
    case DataType::ScalarType:
    case DataType::VectorType:
    case DataType::MatrixType: break;
    case DataType::StructType:
    {
      uint32_t childLocation = 0;
      for(int32_t i = 0; i < inType.children.count(); i++)
      {
        const Decorations &childDecorations = inType.children[i].decorations;

        uint32_t locations = ApplyDerivatives(quadIndex, childDecorations, location + childLocation,
                                              dataTypes[inType.children[i].type], outVar.members[i]);

        childLocation += locations;
      }
      return childLocation;
    }
    case DataType::ArrayType:
    {
      uint32_t childLocation = 0;

      ShaderVariable len = GetActiveLane().ids[inType.length];
      for(uint32_t i = 0; i < len.value.u.x; i++)
      {
        uint32_t locations = ApplyDerivatives(quadIndex, curDecorations, location + childLocation,
                                              dataTypes[inType.InnerType()], outVar.members[i]);

        childLocation += locations;
      }
      return childLocation;
    }
    case DataType::ImageType:
    case DataType::SamplerType:
    case DataType::SampledImageType:
    case DataType::UnknownType:
    {
      RDCERR("Unexpected variable type %d", inType.type);
      return 0;
    }
  }

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

  if(opdata.op == Op::Line || opdata.op == Op::NoLine)
  {
    // ignore OpLine/OpNoLine
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
