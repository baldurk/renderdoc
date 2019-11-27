/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include <float.h>
#include "driver/shaders/spirv/spirv_editor.h"
#include "driver/shaders/spirv/spirv_op_helpers.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

struct feedbackData
{
  uint64_t offset;
  uint32_t numEntries;
};

void AnnotateShader(const SPIRVPatchData &patchData, const char *entryName,
                    const std::map<rdcspv::Binding, feedbackData> &offsetMap, VkDeviceAddress addr,
                    bool bufferAddressKHR, std::vector<uint32_t> &modSpirv)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  const bool useBufferAddress = (addr != 0);

  rdcspv::Id uint32ID = editor.DeclareType(rdcspv::scalar<uint32_t>());
  rdcspv::Id int32ID = editor.DeclareType(rdcspv::scalar<int32_t>());
  rdcspv::Id uint64ID, int64ID;
  rdcspv::Id uint32StructID;
  rdcspv::Id funcParamType;

  if(useBufferAddress)
  {
    // declare the int64 types we'll need
    uint64ID = editor.DeclareType(rdcspv::scalar<uint64_t>());
    int64ID = editor.DeclareType(rdcspv::scalar<int64_t>());

    uint32StructID = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {uint32ID}));

    // any function parameters we add are uint64 byte offsets
    funcParamType = uint64ID;
  }
  else
  {
    rdcspv::Id runtimeArrayID = editor.AddType(rdcspv::OpTypeRuntimeArray(editor.MakeId(), uint32ID));

    editor.AddDecoration(rdcspv::OpDecorate(
        runtimeArrayID, rdcspv::DecorationParam<rdcspv::Decoration::ArrayStride>(sizeof(uint32_t))));

    uint32StructID = editor.AddType(rdcspv::OpTypeStruct(editor.MakeId(), {runtimeArrayID}));

    // any function parameters we add are uint32 indices
    funcParamType = uint32ID;
  }

  editor.SetName(uint32StructID, "__rd_feedbackStruct");

  editor.AddDecoration(rdcspv::OpMemberDecorate(
      uint32StructID, 0, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(0)));

  // map from variable ID to watch, to variable ID to get offset from (as a SPIR-V constant,
  // or as either uint64 byte offset for buffer addressing or uint32 ssbo index otherwise)
  std::map<rdcspv::Id, rdcspv::Id> varLookup;

  // iterate over all variables. We do this here because in the absence of the buffer address
  // extension we might declare our own below and patch bindings - so we need to look these up now
  for(const rdcspv::Variable &var : editor.GetGlobals())
  {
    // skip variables without one of these storage classes, as they are not descriptors
    if(var.storage != rdcspv::StorageClass::UniformConstant &&
       var.storage != rdcspv::StorageClass::Uniform &&
       var.storage != rdcspv::StorageClass::StorageBuffer)
      continue;

    // get this variable's binding info
    rdcspv::Binding bind = editor.GetBinding(var.id);

    // if this is one of the bindings we care about
    auto it = offsetMap.find(bind);
    if(it != offsetMap.end())
    {
      // store the offset for this variable so we watch for access chains and know where to store to
      if(useBufferAddress)
      {
        rdcspv::Id id = varLookup[var.id] = editor.AddConstantImmediate<uint64_t>(it->second.offset);

        editor.SetName(
            id, StringFormat::Fmt("__feedbackOffset_set%u_bind%u", it->first.set, it->first.binding)
                    .c_str());
      }
      else
      {
        // check that the offset fits in 32-bit word, convert byte offset to uint32 index
        uint64_t index = it->second.offset / 4;
        RDCASSERT(index < 0xFFFFFFFFULL, bind.set, bind.binding, it->second.offset);
        rdcspv::Id id = varLookup[var.id] = editor.AddConstantImmediate<uint32_t>(uint32_t(index));

        editor.SetName(
            id, StringFormat::Fmt("__feedbackIndex_set%u_bind%u", it->first.set, it->first.binding)
                    .c_str());
      }
    }
  }

  rdcspv::Id bufferAddressConst, ssboVar, uint32ptrtype;

  if(useBufferAddress)
  {
    // add the extension
    editor.AddExtension(bufferAddressKHR ? "SPV_KHR_physical_storage_buffer"
                                         : "SPV_EXT_physical_storage_buffer");

    // change the memory model to physical storage buffer 64
    rdcspv::Iter it = editor.Begin(rdcspv::Section::MemoryModel);
    rdcspv::OpMemoryModel model(it);
    model.addressingModel = rdcspv::AddressingModel::PhysicalStorageBuffer64;
    it = model;

    // add capabilities
    editor.AddCapability(rdcspv::Capability::PhysicalStorageBufferAddresses);
    editor.AddCapability(rdcspv::Capability::Int64);

    // declare the address constants and make our pointers physical storage buffer pointers
    bufferAddressConst = editor.AddConstantImmediate<uint64_t>(addr);
    uint32ptrtype =
        editor.DeclareType(rdcspv::Pointer(uint32ID, rdcspv::StorageClass::PhysicalStorageBuffer));

    editor.SetName(bufferAddressConst, "__rd_feedbackAddress");

    // struct is block decorated
    editor.AddDecoration(rdcspv::OpDecorate(uint32StructID, rdcspv::Decoration::Block));
  }
  else
  {
    // the pointers are uniform pointers
    rdcspv::Id bufptrtype =
        editor.DeclareType(rdcspv::Pointer(uint32StructID, rdcspv::StorageClass::Uniform));
    uint32ptrtype = editor.DeclareType(rdcspv::Pointer(uint32ID, rdcspv::StorageClass::Uniform));

    // patch all bindings up by 1
    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations),
                     end = editor.End(rdcspv::Section::Annotations);
        it < end; ++it)
    {
      // we will use descriptor set 0 for our own purposes if we don't have a buffer address.
      //
      // Since bindings are arbitrary, we just increase all user bindings to make room, and we'll
      // redeclare the descriptor set layouts and pipeline layout. This is inevitable in the case
      // where all descriptor sets are already used. In theory we only have to do this with set 0,
      // but that requires knowing which variables are in set 0 and it's simpler to increase all
      // bindings.
      if(it.opcode() == rdcspv::Op::Decorate)
      {
        rdcspv::OpDecorate dec(it);
        if(dec.decoration == rdcspv::Decoration::Binding)
        {
          RDCASSERT(dec.decoration.binding != 0xffffffff);
          dec.decoration.binding += 1;
          it = dec;
        }
      }
    }

    // add our SSBO variable, at set 0 binding 0
    ssboVar = editor.MakeId();
    editor.AddVariable(rdcspv::OpVariable(bufptrtype, ssboVar, rdcspv::StorageClass::Uniform));
    editor.AddDecoration(
        rdcspv::OpDecorate(ssboVar, rdcspv::DecorationParam<rdcspv::Decoration::DescriptorSet>(0)));
    editor.AddDecoration(
        rdcspv::OpDecorate(ssboVar, rdcspv::DecorationParam<rdcspv::Decoration::Binding>(0)));

    editor.SetName(ssboVar, "__rd_feedbackBuffer");

    // struct is bufferblock decorated
    editor.AddDecoration(rdcspv::OpDecorate(uint32StructID, rdcspv::Decoration::BufferBlock));
  }

  rdcspv::Id rtarrayOffset = editor.AddConstantImmediate<uint32_t>(0U);
  rdcspv::Id usedValue = editor.AddConstantImmediate<uint32_t>(0xFFFFFFFFU);
  rdcspv::Id scope = editor.AddConstantImmediate<uint32_t>((uint32_t)rdcspv::Scope::Invocation);
  rdcspv::Id semantics = editor.AddConstantImmediate<uint32_t>(0U);
  rdcspv::Id uint32shift = editor.AddConstantImmediate<uint32_t>(2U);

  std::map<rdcspv::Id, rdcspv::Scalar> intTypeLookup;

  for(auto scalarType : editor.GetTypeInfo<rdcspv::Scalar>())
    if(scalarType.first.type == rdcspv::Op::TypeInt)
      intTypeLookup[scalarType.second] = scalarType.first;

  rdcspv::Id entryID;
  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
    {
      entryID = entry.id;
      break;
    }
  }

  rdcspv::TypeToIds<rdcspv::FunctionType> funcTypes = editor.GetTypes<rdcspv::FunctionType>();

  // functions that have been patched with annotation & extra function parameters if needed
  std::set<rdcspv::Id> patchedFunctions;

  // functions we need to patch, with the indices of which parameters have bindings coming along
  // with
  std::map<rdcspv::Id, std::vector<size_t>> functionPatchQueue;

  // start with the entry point, with no parameters to patch
  functionPatchQueue[entryID] = {};

  // now keep patching functions until we have no more to patch
  while(!functionPatchQueue.empty())
  {
    rdcspv::Id funcId;
    std::vector<size_t> patchArgIndices;

    {
      auto it = functionPatchQueue.begin();
      funcId = functionPatchQueue.begin()->first;
      patchArgIndices = functionPatchQueue.begin()->second;
      functionPatchQueue.erase(it);

      patchedFunctions.insert(funcId);
    }

    rdcspv::Iter it = editor.GetID(funcId);

    RDCASSERT(it.opcode() == rdcspv::Op::Function);

    if(!patchArgIndices.empty())
    {
      rdcspv::OpFunction func(it);

      // find the function's type declaration, add the necessary arguments, redeclare and patch it
      for(const rdcspv::TypeToId<rdcspv::FunctionType> &funcType : funcTypes)
      {
        if(funcType.second == func.functionType)
        {
          rdcspv::FunctionType patchedFuncType = funcType.first;
          for(size_t i = 0; i < patchArgIndices.size(); i++)
            patchedFuncType.argumentIds.push_back(funcParamType);

          rdcspv::Id newFuncTypeID = editor.DeclareType(patchedFuncType);

          // re-fetch the iterator as it might have moved with the type declaration
          it = editor.GetID(funcId);

          // change the declared function type
          func.functionType = newFuncTypeID;

          editor.PreModify(it);

          it = func;

          editor.PostModify(it);

          break;
        }
      }
    }

    ++it;

    // onto the OpFunctionParameters. First allocate IDs for all our new function parameters
    std::vector<rdcspv::Id> patchedParamIDs;
    for(size_t i = 0; i < patchArgIndices.size(); i++)
      patchedParamIDs.push_back(editor.MakeId());

    size_t argIndex = 0;
    size_t watchIndex = 0;
    while(it.opcode() == rdcspv::Op::FunctionParameter)
    {
      rdcspv::OpFunctionParameter param(it);

      // if this is a parameter we're patching, add it into varLookup
      if(watchIndex < patchArgIndices.size() && patchArgIndices[watchIndex] == argIndex)
      {
        // when we see use of this parameter, patch it using the added parameter
        varLookup[param.result] = patchedParamIDs[watchIndex];
        // watch for the next argument
        watchIndex++;
      }

      argIndex++;
      ++it;
    }

    // we're past the existing function parameters, now declare our new ones
    for(size_t i = 0; i < patchedParamIDs.size(); i++)
    {
      editor.AddOperation(it, rdcspv::OpFunctionParameter(funcParamType, patchedParamIDs[i]));
      ++it;
    }

    // now patch accesses in the function body
    for(; it; ++it)
    {
      // finish when we hit the end of the function
      if(it.opcode() == rdcspv::Op::FunctionEnd)
        break;

      // if we see an OpCopyObject, just add it to the map pointing to the same value
      if(it.opcode() == rdcspv::Op::CopyObject)
      {
        rdcspv::OpCopyObject copy(it);

        // is this a var we want to snoop?
        auto varIt = varLookup.find(copy.operand);
        if(varIt != varLookup.end())
        {
          varLookup[copy.result] = varIt->second;
        }
      }

      if(it.opcode() == rdcspv::Op::FunctionCall)
      {
        rdcspv::OpFunctionCall call(it);

        // check if any of the variables being passed are ones we care about. Accumulate the added
        // parameters
        std::vector<uint32_t> funccall;
        std::vector<size_t> patchArgs;

        // examine each argument to see if it's one we care about
        for(size_t i = 0; i < call.arguments.size(); i++)
        {
          // if this param we're snooping then pass our offset - whether it's a constant or a
          // function
          // argument itself - into the function call
          auto varIt = varLookup.find(call.arguments[i]);
          if(varIt != varLookup.end())
          {
            funccall.push_back(varIt->second.value());
            patchArgs.push_back(i);
          }
        }

        // if we have parameters to patch, replace the function call
        if(!funccall.empty())
        {
          // prepend all the existing words
          for(size_t i = 1; i < it.size(); i++)
            funccall.insert(funccall.begin() + i - 1, it.word(i));

          rdcspv::Iter oldCall = it;

          // add our patched call afterwards
          it++;
          editor.AddOperation(it, rdcspv::Operation(rdcspv::Op::FunctionCall, funccall));

          // remove the old call
          editor.Remove(oldCall);
        }

        // if this function isn't marked for patching yet, and isn't patched, queue it
        if(functionPatchQueue[call.function].empty() &&
           patchedFunctions.find(call.function) == patchedFunctions.end())
          functionPatchQueue[call.function] = patchArgs;
      }

      // if we see an access chain of a variable we're snooping, save out the result
      if(it.opcode() == rdcspv::Op::AccessChain || it.opcode() == rdcspv::Op::InBoundsAccessChain)
      {
        rdcspv::OpAccessChain chain(it);
        chain.op = it.opcode();

        // is this a var we want to snoop?
        auto varIt = varLookup.find(chain.base);
        if(varIt != varLookup.end())
        {
          // multi-dimensional arrays of descriptors is not allowed - however an access chain could
          // be longer than 5 words (1 index). Think of the case of a uniform buffer where the first
          // index goes into the descriptor array, and further indices go inside the uniform buffer
          // members.
          RDCASSERT(chain.indexes.size() >= 1, chain.indexes.size());

          rdcspv::Id index = chain.indexes[0];

          // patch after the access chain
          it++;

          // upcast the index to uint32 or uint64 depending on which path we're taking
          uint32_t targetIndexWidth = useBufferAddress ? 64 : 32;
          {
            rdcspv::Id indexType = editor.GetIDType(index);

            if(indexType == rdcspv::Id())
            {
              RDCERR("Unknown type for ID %u, defaulting to uint32_t", index);
              indexType = uint32ID;
            }

            rdcspv::Scalar indexTypeData = rdcspv::scalar<uint32_t>();
            auto indexTypeIt = intTypeLookup.find(indexType);

            if(indexTypeIt != intTypeLookup.end())
            {
              indexTypeData = indexTypeIt->second;
            }
            else
            {
              RDCERR("Unknown index type ID %u, defaulting to uint32_t", indexType);
            }

            // if it's signed, bitcast it to unsigned
            if(indexTypeData.signedness)
            {
              indexTypeData.signedness = false;

              rdcspv::Id unsignedIndex = editor.MakeId();
              editor.AddOperation(
                  it, rdcspv::OpBitcast(editor.DeclareType(indexTypeData), unsignedIndex, index));
              it++;

              index = unsignedIndex;
            }

            // if it's not wide enough, uconvert expand it
            if(indexTypeData.width != targetIndexWidth)
            {
              rdcspv::Id extendedtype =
                  editor.DeclareType(rdcspv::Scalar(rdcspv::Op::TypeInt, targetIndexWidth, false));
              rdcspv::Id extendedindex = editor.MakeId();
              editor.AddOperation(it, rdcspv::OpUConvert(extendedtype, extendedindex, index));
              it++;

              index = extendedindex;
            }
          }

          rdcspv::Id bufptr;

          if(useBufferAddress)
          {
            // convert the constant embedded device address to a pointer

            // get our output slot address by adding an offset to the base pointer
            // baseaddr = bufferAddressConst + bindingOffset
            rdcspv::Id baseaddr = editor.MakeId();
            editor.AddOperation(
                it, rdcspv::OpIAdd(uint64ID, baseaddr, bufferAddressConst, varIt->second));
            it++;

            // shift the index since this is a byte offset
            // shiftedindex = index << uint32shift
            rdcspv::Id shiftedindex = editor.MakeId();
            editor.AddOperation(
                it, rdcspv::OpShiftLeftLogical(uint64ID, shiftedindex, index, uint32shift));
            it++;

            // add the index on top of that
            // offsetaddr = baseaddr + shiftedindex
            rdcspv::Id offsetaddr = editor.MakeId();
            editor.AddOperation(it, rdcspv::OpIAdd(uint64ID, offsetaddr, baseaddr, shiftedindex));
            it++;

            // make a pointer out of it
            // uint32_t *bufptr = (uint32_t *)offsetaddr
            bufptr = editor.MakeId();
            editor.AddOperation(it, rdcspv::OpConvertUToPtr(uint32ptrtype, bufptr, offsetaddr));
            it++;
          }
          else
          {
            // accesschain into the SSBO, by adding the base offset for this var onto the index

            // add the index to this binding's base index
            // ssboindex = bindingOffset + index
            rdcspv::Id ssboindex = editor.MakeId();
            editor.AddOperation(it, rdcspv::OpIAdd(uint32ID, ssboindex, index, varIt->second));
            it++;

            // accesschain to get the pointer we'll atomic into.
            // accesschain is 0 to access rtarray (first member) then ssboindex for array index
            // uint32_t *bufptr = (uint32_t *)&buf.rtarray[ssboindex];
            bufptr = editor.MakeId();
            editor.AddOperation(it, rdcspv::OpAccessChain(uint32ptrtype, bufptr, ssboVar,
                                                          {rtarrayOffset, ssboindex}));
            it++;
          }

          // atomically set the uint32 that's pointed to
          editor.AddOperation(it, rdcspv::OpAtomicUMax(uint32ID, editor.MakeId(), bufptr, scope,
                                                       semantics, usedValue));

          // no it++ here, it will happen implicitly on loop continue
        }
      }
    }
  }
}

void VulkanReplay::ClearFeedbackCache()
{
  m_BindlessFeedback.Usage.clear();
}

void VulkanReplay::FetchShaderFeedback(uint32_t eventId)
{
  if(m_BindlessFeedback.Usage.find(eventId) != m_BindlessFeedback.Usage.end())
    return;

  // create it here so we won't re-run any code if the event is re-selected. We'll mark it as valid
  // if it actually has any data in it later.
  DynamicUsedBinds &result = m_BindlessFeedback.Usage[eventId];

  bool useBufferAddress = (m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address ||
                           m_pDriver->GetExtensions(NULL).ext_EXT_buffer_device_address) &&
                          m_pDriver->GetDeviceFeatures().shaderInt64;

  bool useBufferAddressKHR = m_pDriver->GetExtensions(NULL).ext_KHR_buffer_device_address;

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  if(drawcall == NULL || !(drawcall->flags & (DrawFlags::Dispatch | DrawFlags::Drawcall)))
    return;

  result.compute = bool(drawcall->flags & DrawFlags::Dispatch);

  const VulkanStatePipeline &pipe = result.compute ? state.compute : state.graphics;

  if(pipe.pipeline == ResourceId())
    return;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[pipe.pipeline];

  VkDeviceSize feedbackStorageSize = 0;

  std::map<rdcspv::Binding, feedbackData> offsetMap;

  {
    const std::vector<ResourceId> &descSetLayoutIds =
        creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts;

    rdcspv::Binding key;

    for(size_t set = 0; set < descSetLayoutIds.size(); set++)
    {
      key.set = (uint32_t)set;

      const DescSetLayout &layout = creationInfo.m_DescSetLayout[descSetLayoutIds[set]];

      for(size_t binding = 0; binding < layout.bindings.size(); binding++)
      {
        const DescSetLayout::Binding &bindData = layout.bindings[binding];

        // skip empty bindings
        if(bindData.descriptorCount == 0 || bindData.stageFlags == 0)
          continue;

        // only process array bindings
        if(bindData.descriptorCount > 1)
        {
          key.binding = (uint32_t)binding;

          offsetMap[key] = {feedbackStorageSize, bindData.descriptorCount};

          feedbackStorageSize += bindData.descriptorCount * sizeof(uint32_t);
        }
      }
    }
  }

  // if we don't have any array descriptors to feedback then just return now
  if(offsetMap.empty())
    return;

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkGraphicsPipelineCreateInfo graphicsInfo = {};
  VkComputePipelineCreateInfo computeInfo = {};

  // get pipeline create info
  if(result.compute)
    m_pDriver->GetShaderCache()->MakeComputePipelineInfo(computeInfo, state.compute.pipeline);
  else
    m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(graphicsInfo, state.graphics.pipeline);

  if(feedbackStorageSize > m_BindlessFeedback.FeedbackBuffer.sz)
  {
    uint32_t flags = GPUBuffer::eGPUBufferGPULocal | GPUBuffer::eGPUBufferSSBO;

    if(useBufferAddress)
      flags |= GPUBuffer::eGPUBufferAddressable;

    m_BindlessFeedback.FeedbackBuffer.Destroy();
    m_BindlessFeedback.FeedbackBuffer.Create(m_pDriver, dev, feedbackStorageSize, 1, flags);
  }

  VkDeviceAddress bufferAddress = 0;

  VkDescriptorPool descpool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSetLayout> setLayouts;
  std::vector<VkDescriptorSet> descSets;

  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;

  if(useBufferAddress)
  {
    RDCCOMPILE_ASSERT(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR ==
                          VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT,
                      "KHR and EXT buffer_device_address should be interchangeable here.");
    VkBufferDeviceAddressInfoKHR getAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
    getAddressInfo.buffer = m_BindlessFeedback.FeedbackBuffer.buf;

    if(useBufferAddressKHR)
      bufferAddress = m_pDriver->vkGetBufferDeviceAddressKHR(dev, &getAddressInfo);
    else
      bufferAddress = m_pDriver->vkGetBufferDeviceAddressEXT(dev, &getAddressInfo);
  }
  else
  {
    VkDescriptorSetLayoutBinding newBindings[] = {
        // output buffer
        {
            0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
            VkShaderStageFlags(result.compute ? VK_SHADER_STAGE_COMPUTE_BIT
                                              : VK_SHADER_STAGE_ALL_GRAPHICS),
            NULL,
        },
    };
    RDCCOMPILE_ASSERT(ARRAY_COUNT(newBindings) == 1,
                      "Should only be one new descriptor for bindless feedback");

    // create a duplicate set of descriptor sets, all visible to compute, with bindings shifted to
    // account for new ones we need. This also copies the existing bindings into the new sets
    PatchReservedDescriptors(pipe, descpool, setLayouts, descSets, VkShaderStageFlagBits(),
                             newBindings, ARRAY_COUNT(newBindings));

    // create pipeline layout with new descriptor set layouts
    {
      const std::vector<VkPushConstantRange> &push =
          creationInfo.m_PipelineLayout[pipeInfo.layout].pushRanges;

      VkPipelineLayoutCreateInfo pipeLayoutInfo = {
          VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          NULL,
          0,
          (uint32_t)setLayouts.size(),
          setLayouts.data(),
          (uint32_t)push.size(),
          push.data(),
      };

      vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      // we'll only use one, set both structs to keep things simple
      computeInfo.layout = pipeLayout;
      graphicsInfo.layout = pipeLayout;
    }

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo desc = {0};

    m_BindlessFeedback.FeedbackBuffer.FillDescriptor(desc);

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        NULL,
        Unwrap(descSets[0]),
        0,
        0,
        1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        NULL,
        &desc,
        NULL,
    };

    ObjDisp(dev)->UpdateDescriptorSets(Unwrap(dev), 1, &write, 0, NULL);
  }

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

  VkShaderModule modules[6] = {};

  if(result.compute)
  {
    VkPipelineShaderStageCreateInfo &stage = computeInfo.stage;

    const VulkanCreationInfo::ShaderModule &moduleInfo =
        creationInfo.m_ShaderModule[pipeInfo.shaders[5].module];

    std::vector<uint32_t> modSpirv = moduleInfo.spirv.GetSPIRV();

    AnnotateShader(*pipeInfo.shaders[5].patchData, stage.pName, offsetMap, bufferAddress,
                   useBufferAddressKHR, modSpirv);

    moduleCreateInfo.pCode = modSpirv.data();
    moduleCreateInfo.codeSize = modSpirv.size() * sizeof(uint32_t);

    vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &modules[0]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    stage.module = modules[0];
  }
  else
  {
    for(uint32_t i = 0; i < graphicsInfo.stageCount; i++)
    {
      VkPipelineShaderStageCreateInfo &stage =
          (VkPipelineShaderStageCreateInfo &)graphicsInfo.pStages[i];

      int idx = StageIndex(stage.stage);

      const VulkanCreationInfo::ShaderModule &moduleInfo =
          creationInfo.m_ShaderModule[pipeInfo.shaders[idx].module];

      std::vector<uint32_t> modSpirv = moduleInfo.spirv.GetSPIRV();

      AnnotateShader(*pipeInfo.shaders[idx].patchData, stage.pName, offsetMap, bufferAddress,
                     useBufferAddressKHR, modSpirv);

      moduleCreateInfo.pCode = modSpirv.data();
      moduleCreateInfo.codeSize = modSpirv.size() * sizeof(uint32_t);

      vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &modules[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      stage.module = modules[i];
    }
  }

  VkPipeline feedbackPipe;

  if(result.compute)
  {
    vkr = m_pDriver->vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computeInfo, NULL,
                                              &feedbackPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else
  {
    vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &graphicsInfo, NULL,
                                               &feedbackPipe);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;
  VulkanStatePipeline &modifiedpipe = result.compute ? modifiedstate.compute : modifiedstate.graphics;

  // bind created pipeline to partial replay state
  modifiedpipe.pipeline = GetResID(feedbackPipe);

  if(!useBufferAddress)
  {
    // replace descriptor set IDs with our temporary sets. The offsets we keep the same. If the
    // original draw had no sets, we ensure there's room (with no offsets needed)

    if(modifiedpipe.descSets.empty())
      modifiedpipe.descSets.resize(1);

    for(size_t i = 0; i < descSets.size(); i++)
    {
      modifiedpipe.descSets[i].pipeLayout = GetResID(pipeLayout);
      modifiedpipe.descSets[i].descSet = GetResID(descSets[i]);
    }
  }

  {
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // fill destination buffer with 0s to ensure a baseline to then feedback against
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(m_BindlessFeedback.FeedbackBuffer.buf), 0,
                                feedbackStorageSize, 0);

    VkBufferMemoryBarrier feedbackbufBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(m_BindlessFeedback.FeedbackBuffer.buf),
        0,
        feedbackStorageSize,
    };

    // wait for the above fill to finish.
    DoPipelineBarrier(cmd, 1, &feedbackbufBarrier);

    if(result.compute)
    {
      modifiedstate.BindPipeline(cmd, VulkanRenderState::BindCompute, true);

      ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), drawcall->dispatchDimension[0],
                                drawcall->dispatchDimension[1], drawcall->dispatchDimension[2]);
    }
    else
    {
      modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

      if(drawcall->flags & DrawFlags::Indexed)
      {
        ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                                     drawcall->indexOffset, drawcall->baseVertex,
                                     drawcall->instanceOffset);
      }
      else
      {
        ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                              drawcall->vertexOffset, drawcall->instanceOffset);
      }

      modifiedstate.EndRenderPass(cmd);
    }

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  bytebuf data;
  GetBufferData(GetResID(m_BindlessFeedback.FeedbackBuffer.buf), 0, 0, data);

  for(auto it = offsetMap.begin(); it != offsetMap.end(); ++it)
  {
    uint32_t *feedbackData = (uint32_t *)(data.data() + it->second.offset);

    BindIdx used;
    used.set = it->first.set;
    used.bind = it->first.binding;

    for(uint32_t i = 0; i < it->second.numEntries; i++)
    {
      if(feedbackData[i])
      {
        used.arrayidx = i;

        result.used.push_back(used);
      }
    }
  }

  result.valid = true;

  if(descpool != VK_NULL_HANDLE)
  {
    // delete descriptors. Technically we don't have to free the descriptor sets, but our tracking
    // on
    // replay doesn't handle destroying children of pooled objects so we do it explicitly anyway.
    m_pDriver->vkFreeDescriptorSets(dev, descpool, (uint32_t)descSets.size(), descSets.data());

    m_pDriver->vkDestroyDescriptorPool(dev, descpool, NULL);
  }

  for(VkDescriptorSetLayout layout : setLayouts)
    m_pDriver->vkDestroyDescriptorSetLayout(dev, layout, NULL);

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, feedbackPipe, NULL);

  // delete shader/shader module
  for(size_t i = 0; i < ARRAY_COUNT(modules); i++)
    if(modules[i] != VK_NULL_HANDLE)
      m_pDriver->vkDestroyShaderModule(dev, modules[i], NULL);

  // replay from the start as we may have corrupted state while fetching the above feedback.
  m_pDriver->ReplayLog(0, eventId, eReplay_Full);
}
