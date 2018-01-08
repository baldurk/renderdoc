/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "driver/shaders/spirv/spirv_common.h"
#include "driver/shaders/spirv/spirv_editor.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

static void AddOutputDumping(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                             const char *entryName, uint32_t &descSet, uint32_t vertexIndexOffset,
                             uint32_t instanceIndexOffset, uint32_t numVerts,
                             std::vector<uint32_t> &modSpirv, uint32_t &bufStride)
{
  SPIRVEditor editor(modSpirv);

  uint32_t numOutputs = (uint32_t)refl.outputSignature.size();
  RDCASSERT(numOutputs > 0);

  descSet = 0;

  for(SPIRVIterator it = editor.BeginDecorations(), end = editor.EndDecorations(); it != end; ++it)
  {
    // we will use the descriptor set immediately after the last set statically used by the shader.
    // This means we don't have to worry about if the descriptor set layout declares more sets which
    // might be invalid and un-bindable, we just trample over the next set that's unused.
    // This is much easier than trying to add a new bind to an existing descriptor set (which would
    // cascade into a new descriptor set layout, new pipeline layout, etc etc!). However, this might
    // push us over the limit on number of descriptor sets.
    if(it.opcode() == spv::OpDecorate && it.word(2) == spv::DecorationDescriptorSet)
      descSet = RDCMAX(descSet, it.word(3) + 1);
  }

  struct outputIDs
  {
    SPIRVId constID;         // constant ID for the index of this output
    SPIRVId basetypeID;      // the type ID for this output. Must be present already by definition!
    SPIRVId uniformPtrID;    // Uniform Pointer ID for this output. Used to write the output data
    SPIRVId outputPtrID;     // Output Pointer ID for this output. Used to read the output data
  };
  std::vector<outputIDs> outs;
  outs.resize(numOutputs);

  // we'll need these for intermediary steps
  SPIRVId uint32ID = editor.DeclareType(scalar<uint32_t>());
  SPIRVId sint32ID = editor.DeclareType(scalar<int32_t>());
  SPIRVId sint32PtrInID = editor.DeclareType(SPIRVPointer(sint32ID, spv::StorageClassInput));

  // declare necessary variables per-output, types and constants
  for(uint32_t i = 0; i < numOutputs; i++)
  {
    outputIDs &o = outs[i];

    // constant for this index
    o.constID = editor.AddConstantImmediate(i);

    // base type - either a scalar or a vector, since matrix outputs are decayed to vectors
    {
      SPIRVScalar scalarType = scalar<uint32_t>();

      if(refl.outputSignature[i].compType == CompType::UInt)
        scalarType = scalar<uint32_t>();
      else if(refl.outputSignature[i].compType == CompType::SInt)
        scalarType = scalar<int32_t>();
      else if(refl.outputSignature[i].compType == CompType::Float)
        scalarType = scalar<float>();
      else if(refl.outputSignature[i].compType == CompType::Double)
        scalarType = scalar<double>();

      if(refl.outputSignature[i].compCount > 1)
        o.basetypeID = editor.DeclareType(SPIRVVector(scalarType, refl.outputSignature[i].compCount));
      else
        o.basetypeID = editor.DeclareType(scalarType);
    }

    o.uniformPtrID = editor.DeclareType(SPIRVPointer(outs[i].basetypeID, spv::StorageClassUniform));
    o.outputPtrID = editor.DeclareType(SPIRVPointer(outs[i].basetypeID, spv::StorageClassOutput));

    RDCASSERT(o.basetypeID && o.constID && o.outputPtrID && o.uniformPtrID, o.basetypeID, o.constID,
              o.outputPtrID, o.uniformPtrID);
  }

  SPIRVId outBufferVarID = 0;
  SPIRVId numVertsConstID = editor.AddConstantImmediate(numVerts);
  SPIRVId vertexIndexOffsetConstID = editor.AddConstantImmediate(vertexIndexOffset);
  SPIRVId instanceIndexOffsetConstID = editor.AddConstantImmediate(instanceIndexOffset);

  editor.SetName(numVertsConstID, "numVerts");
  editor.SetName(vertexIndexOffsetConstID, "vertexIndexOffset");
  editor.SetName(instanceIndexOffsetConstID, "instanceIndexOffset");

  // declare the output buffer and its type
  {
    std::vector<uint32_t> words;
    for(uint32_t o = 0; o < numOutputs; o++)
      words.push_back(outs[o].basetypeID);

    // struct vertex { ... outputs };
    SPIRVId vertStructID = editor.DeclareStructType(words);
    editor.SetName(vertStructID, "vertex_struct");

    // vertex vertArray[];
    SPIRVId runtimeArrayID =
        editor.AddType(SPIRVOperation(spv::OpTypeRuntimeArray, {editor.MakeId(), vertStructID}));
    editor.SetName(runtimeArrayID, "vertex_array");

    // struct meshOutput { vertex vertArray[]; };
    SPIRVId outputStructID = editor.DeclareStructType({runtimeArrayID});
    editor.SetName(outputStructID, "meshOutput");

    // meshOutput *
    SPIRVId outputStructPtrID =
        editor.DeclareType(SPIRVPointer(outputStructID, spv::StorageClassUniform));
    editor.SetName(outputStructPtrID, "meshOutput_ptr");

    // meshOutput *outputData;
    outBufferVarID = editor.AddVariable(SPIRVOperation(
        spv::OpVariable, {outputStructPtrID, editor.MakeId(), spv::StorageClassUniform}));
    editor.SetName(outBufferVarID, "outputData");

    uint32_t memberOffset = 0;
    for(uint32_t o = 0; o < numOutputs; o++)
    {
      uint32_t elemSize = 0;
      if(refl.outputSignature[o].compType == CompType::Double)
        elemSize = 8;
      else if(refl.outputSignature[o].compType == CompType::SInt ||
              refl.outputSignature[o].compType == CompType::UInt ||
              refl.outputSignature[o].compType == CompType::Float)
        elemSize = 4;
      else
        RDCERR("Unexpected component type for output signature element");

      uint32_t numComps = refl.outputSignature[o].compCount;

      // ensure member is std430 packed (vec4 alignment for vec3/vec4)
      if(numComps == 2)
        memberOffset = AlignUp(memberOffset, 2U * elemSize);
      else if(numComps > 2)
        memberOffset = AlignUp(memberOffset, 4U * elemSize);

      // apply decoration to each member in the struct with its offset in the struct
      editor.AddDecoration(SPIRVOperation(spv::OpMemberDecorate,
                                          {vertStructID, o, spv::DecorationOffset, memberOffset}));

      memberOffset += elemSize * refl.outputSignature[o].compCount;
    }

    // align to 16 bytes (vec4) since we will almost certainly have
    // a vec4 in the struct somewhere, and even in std430 alignment,
    // the base struct alignment is still the largest base alignment
    // of any member
    bufStride = AlignUp16(memberOffset);

    // the array is the only element in the output struct, so
    // it's at offset 0
    editor.AddDecoration(
        SPIRVOperation(spv::OpMemberDecorate, {outputStructID, 0, spv::DecorationOffset, 0}));

    // set array stride
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {runtimeArrayID, spv::DecorationArrayStride, bufStride}));

    // set object type
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {outputStructID, spv::DecorationBufferBlock}));

    // set binding
    editor.AddDecoration(
        SPIRVOperation(spv::OpDecorate, {outBufferVarID, spv::DecorationDescriptorSet, descSet}));
    editor.AddDecoration(SPIRVOperation(spv::OpDecorate, {outBufferVarID, spv::DecorationBinding, 0}));
  }

  // the spec allows for multiple declarations of VertexIndex/InstanceIndex, so instead of trying to
  // locate the existing declaration we just declare our own.
  // declare global inputs (vertexindex/instanceindex)
  SPIRVId vertidxID = editor.AddVariable(
      SPIRVOperation(spv::OpVariable, {sint32PtrInID, editor.MakeId(), spv::StorageClassInput}));
  editor.AddDecoration(SPIRVOperation(
      spv::OpDecorate, {vertidxID, spv::DecorationBuiltIn, spv::BuiltInVertexIndex}));

  SPIRVId instidxID = editor.AddVariable(
      SPIRVOperation(spv::OpVariable, {sint32PtrInID, editor.MakeId(), spv::StorageClassInput}));
  editor.AddDecoration(SPIRVOperation(
      spv::OpDecorate, {instidxID, spv::DecorationBuiltIn, spv::BuiltInInstanceIndex}));

  editor.SetName(vertidxID, "rdoc_vtxidx");
  editor.SetName(instidxID, "rdoc_instidx");

  // make a new entry point that will call the old function, then when it returns extract & write
  // the outputs.
  SPIRVId wrapperEntry = editor.MakeId();
  // we set a debug name, but we don't rename the actual entry point since the API needs to hook up
  // to it the same way.
  editor.SetName(wrapperEntry, "RenderDoc_MeshFetch_Wrapper_Entrypoint");

  SPIRVId entryID = 0;

  for(const SPIRVEntry &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
      entryID = entry.id;
  }

  RDCASSERT(entryID);

  // add our new global inputs to the entry point's interface, and repoint it to the new function
  // we'll write
  {
    SPIRVIterator entry = editor.GetEntry(entryID);
    editor.AddWord(entry, vertidxID);
    editor.AddWord(entry, instidxID);

    // repoint the entry point to our new wrapper
    entry.word(2) = wrapperEntry;
  }

  // add the wrapper function
  {
    std::vector<SPIRVOperation> ops;

    SPIRVId voidType = editor.DeclareType(SPIRVVoid());
    SPIRVId funcType = editor.DeclareType(SPIRVFunction(voidType, {}));

    ops.push_back(SPIRVOperation(spv::OpFunction,
                                 {voidType, wrapperEntry, spv::FunctionControlMaskNone, funcType}));

    ops.push_back(SPIRVOperation(spv::OpLabel, {editor.MakeId()}));
    {
      // real_main();
      ops.push_back(SPIRVOperation(spv::OpFunctionCall, {voidType, editor.MakeId(), entryID}));

      // int vtx = *rdoc_vtxidx;
      uint32_t loadedVtxID = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpLoad, {sint32ID, loadedVtxID, vertidxID}));

      // int inst = *rdoc_instidx;
      uint32_t loadedInstID = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpLoad, {sint32ID, loadedInstID, instidxID}));

      // int rebasedInst = inst - instanceIndexOffset
      uint32_t rebasedInstID = editor.MakeId();
      ops.push_back(SPIRVOperation(
          spv::OpISub, {sint32ID, rebasedInstID, loadedInstID, instanceIndexOffsetConstID}));

      // int startVert = rebasedInst * numVerts
      uint32_t startVertID = editor.MakeId();
      ops.push_back(
          SPIRVOperation(spv::OpIMul, {sint32ID, startVertID, rebasedInstID, numVertsConstID}));

      // int rebasedVert = vtx - vertexIndexOffset
      uint32_t rebasedVertID = editor.MakeId();
      ops.push_back(SPIRVOperation(
          spv::OpISub, {sint32ID, rebasedVertID, loadedVtxID, vertexIndexOffsetConstID}));

      // int arraySlot = startVert + rebasedVert
      uint32_t arraySlotID = editor.MakeId();
      ops.push_back(SPIRVOperation(spv::OpIAdd, {sint32ID, arraySlotID, startVertID, rebasedVertID}));

      SPIRVId zero = outs[0].constID;

      for(uint32_t o = 0; o < numOutputs; o++)
      {
        uint32_t loaded = 0;

        // not a structure member or array child, can load directly
        if(patchData.outputs[o].accessChain.empty())
        {
          loaded = editor.MakeId();
          // type loaded = *globalvar;
          ops.push_back(
              SPIRVOperation(spv::OpLoad, {outs[o].basetypeID, loaded, patchData.outputs[o].ID}));
        }
        else
        {
          uint32_t readPtr = editor.MakeId();
          loaded = editor.MakeId();

          // structure member, need to access chain first
          std::vector<uint32_t> words = {outs[o].outputPtrID, readPtr, patchData.outputs[o].ID};

          for(uint32_t idx : patchData.outputs[o].accessChain)
            words.push_back(outs[idx].constID);

          // type *readPtr = globalvar.globalsub...;
          ops.push_back(SPIRVOperation(spv::OpAccessChain, words));
          // type loaded = *readPtr;
          ops.push_back(SPIRVOperation(spv::OpLoad, {outs[o].basetypeID, loaded, readPtr}));
        }

        // access chain the destination
        // type *writePtr = outBuffer.verts[arraySlot].outputN
        uint32_t writePtr = editor.MakeId();
        ops.push_back(SPIRVOperation(
            spv::OpAccessChain,
            {outs[o].uniformPtrID, writePtr, outBufferVarID, zero, arraySlotID, outs[o].constID}));

        // *writePtr = loaded;
        ops.push_back(SPIRVOperation(spv::OpStore, {writePtr, loaded}));
      }
    }
    ops.push_back(SPIRVOperation(spv::OpReturn, {}));

    ops.push_back(SPIRVOperation(spv::OpFunctionEnd, {}));

    editor.AddFunction(ops.data(), ops.size());
  }
}

void VulkanReplay::ClearPostVSCache()
{
  VkDevice dev = m_Device;

  for(auto it = m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
  {
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.buf, NULL);
    m_pDriver->vkDestroyBuffer(dev, it->second.vsout.idxBuf, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.bufmem, NULL);
    m_pDriver->vkFreeMemory(dev, it->second.vsout.idxBufMem, NULL);
  }

  m_PostVSData.clear();
}

void VulkanReplay::InitPostVSBuffers(uint32_t eventId)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    return;

  if(!m_pDriver->GetDeviceFeatures().vertexPipelineStoresAndAtomics)
    return;

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &creationInfo = m_pDriver->m_CreationInfo;

  if(state.graphics.pipeline == ResourceId() || state.renderPass == ResourceId())
    return;

  const VulkanCreationInfo::Pipeline &pipeInfo = creationInfo.m_Pipeline[state.graphics.pipeline];

  if(pipeInfo.shaders[0].module == ResourceId())
    return;

  const VulkanCreationInfo::ShaderModule &moduleInfo =
      creationInfo.m_ShaderModule[pipeInfo.shaders[0].module];

  ShaderReflection *refl = pipeInfo.shaders[0].refl;

  // no outputs from this shader? unexpected but theoretically possible (dummy VS before
  // tessellation maybe). Just fill out an empty data set
  if(refl->outputSignature.empty())
  {
    // empty vertex output signature
    m_PostVSData[eventId].vsin.topo = pipeInfo.topology;
    m_PostVSData[eventId].vsout.buf = VK_NULL_HANDLE;
    m_PostVSData[eventId].vsout.instStride = 0;
    m_PostVSData[eventId].vsout.vertStride = 0;
    m_PostVSData[eventId].vsout.nearPlane = 0.0f;
    m_PostVSData[eventId].vsout.farPlane = 0.0f;
    m_PostVSData[eventId].vsout.useIndices = false;
    m_PostVSData[eventId].vsout.hasPosOut = false;
    m_PostVSData[eventId].vsout.idxBuf = VK_NULL_HANDLE;

    m_PostVSData[eventId].vsout.topo = pipeInfo.topology;

    return;
  }

  const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);

  if(drawcall == NULL || drawcall->numIndices == 0 || drawcall->numInstances == 0)
    return;

  // the SPIR-V patching will determine the next descriptor set to use, after all sets statically
  // used by the shader. This gets around the problem where the shader only uses 0 and 1, but the
  // layout declares 0-4, and 2,3,4 are invalid at bind time and we are unable to bind our new set
  // 5. Instead we'll notice that only 0 and 1 are used and just use 2 ourselves (although it was in
  // the original set layout, we know it's statically unused by the shader so we can safely steal
  // it).
  uint32_t descSet = 0;

  // we go through the driver for all these creations since they need to be properly
  // registered in order to be put in the partial replay state
  VkResult vkr = VK_SUCCESS;
  VkDevice dev = m_Device;

  VkPipelineLayout pipeLayout;

  VkGraphicsPipelineCreateInfo pipeCreateInfo;

  // get pipeline create info
  m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, state.graphics.pipeline);

  // set primitive topology to point list
  VkPipelineInputAssemblyStateCreateInfo *ia =
      (VkPipelineInputAssemblyStateCreateInfo *)pipeCreateInfo.pInputAssemblyState;

  VkPrimitiveTopology topo = ia->topology;

  ia->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

  // remove all stages but the vertex shader, we just want to run it and write the data,
  // we don't want to tessellate/geometry shade, nor rasterize (which we disable below)
  uint32_t vertIdx = pipeCreateInfo.stageCount;

  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    if(pipeCreateInfo.pStages[i].stage & VK_SHADER_STAGE_VERTEX_BIT)
    {
      vertIdx = i;
      break;
    }
  }

  RDCASSERT(vertIdx < pipeCreateInfo.stageCount);

  if(vertIdx != 0)
    (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[0] = pipeCreateInfo.pStages[vertIdx];

  pipeCreateInfo.stageCount = 1;

  // enable rasterizer discard
  VkPipelineRasterizationStateCreateInfo *rs =
      (VkPipelineRasterizationStateCreateInfo *)pipeCreateInfo.pRasterizationState;
  rs->rasterizerDiscardEnable = true;

  VkBuffer meshBuffer = VK_NULL_HANDLE, readbackBuffer = VK_NULL_HANDLE;
  VkDeviceMemory meshMem = VK_NULL_HANDLE, readbackMem = VK_NULL_HANDLE;

  VkBuffer idxBuf = VK_NULL_HANDLE, uniqIdxBuf = VK_NULL_HANDLE;
  VkDeviceMemory idxBufMem = VK_NULL_HANDLE, uniqIdxBufMem = VK_NULL_HANDLE;

  uint32_t numVerts = drawcall->numIndices;
  VkDeviceSize bufSize = 0;

  vector<uint32_t> indices;
  uint32_t idxsize = state.ibuffer.bytewidth;
  bool index16 = (idxsize == 2);
  uint32_t numIndices = numVerts;
  bytebuf idxdata;
  uint16_t *idx16 = NULL;
  uint32_t *idx32 = NULL;

  uint32_t minIndex = 0, maxIndex = 0;

  uint32_t vertexIndexOffset = 0;

  if(drawcall->flags & DrawFlags::UseIBuffer)
  {
    // fetch ibuffer
    GetBufferData(state.ibuffer.buf, state.ibuffer.offs + drawcall->indexOffset * idxsize,
                  uint64_t(drawcall->numIndices) * idxsize, idxdata);

    // figure out what the maximum index could be, so we can clamp our index buffer to something
    // sane
    uint32_t maxIdx = 0;

    // if there are no active bindings assume the vertex shader is generating its own data
    // and don't clamp the indices
    if(pipeCreateInfo.pVertexInputState->vertexBindingDescriptionCount == 0)
      maxIdx = ~0U;

    for(uint32_t b = 0; b < pipeCreateInfo.pVertexInputState->vertexBindingDescriptionCount; b++)
    {
      const VkVertexInputBindingDescription &input =
          pipeCreateInfo.pVertexInputState->pVertexBindingDescriptions[b];
      // only vertex inputs (not instance inputs) count
      if(input.inputRate == VK_VERTEX_INPUT_RATE_VERTEX)
      {
        if(b >= state.vbuffers.size())
          continue;

        ResourceId buf = state.vbuffers[b].buf;
        VkDeviceSize offs = state.vbuffers[b].offs;

        VkDeviceSize bufsize = creationInfo.m_Buffer[buf].size;

        // the maximum valid index on this particular input is the one that reaches
        // the end of the buffer. The maximum valid index at all is the one that reads
        // off the end of ALL buffers (so we max it with any other maxindex value
        // calculated).
        if(input.stride > 0)
          maxIdx = RDCMAX(maxIdx, uint32_t((bufsize - offs) / input.stride));
      }
    }

    // in case the vertex buffers were set but had invalid stride (0), max with the number
    // of vertices too. This is fine since the max here is just a conservative limit
    maxIdx = RDCMAX(maxIdx, drawcall->numIndices);

    // do ibuffer rebasing/remapping

    idx16 = (uint16_t *)&idxdata[0];
    idx32 = (uint32_t *)&idxdata[0];

    // only read as many indices as were available in the buffer
    numIndices =
        RDCMIN(uint32_t(index16 ? idxdata.size() / 2 : idxdata.size() / 4), drawcall->numIndices);

    // grab all unique vertex indices referenced
    for(uint32_t i = 0; i < numIndices; i++)
    {
      uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

      // we clamp to maxIdx here, to avoid any invalid indices like 0xffffffff
      // from filtering through. Worst case we index to the end of the vertex
      // buffers which is generally much more reasonable
      i32 = RDCMIN(maxIdx, i32);

      auto it = std::lower_bound(indices.begin(), indices.end(), i32);

      if(it != indices.end() && *it == i32)
        continue;

      indices.insert(it, i32);
    }

    // if we read out of bounds, we'll also have a 0 index being referenced
    // (as 0 is read). Don't insert 0 if we already have 0 though
    if(numIndices < drawcall->numIndices && (indices.empty() || indices[0] != 0))
      indices.insert(indices.begin(), 0);

    minIndex = indices[0];
    maxIndex = indices[indices.size() - 1];

    vertexIndexOffset = minIndex + drawcall->baseVertex;

    // set numVerts
    numVerts = maxIndex - minIndex + 1;

    // create buffer with unique 0-based indices
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &uniqIdxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, uniqIdxBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &uniqIdxBufMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, uniqIdxBuf, uniqIdxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, uniqIdxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, &indices[0], indices.size() * sizeof(uint32_t));

    m_pDriver->vkUnmapMemory(m_Device, uniqIdxBufMem);

    bufInfo.size = numIndices * idxsize;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &idxBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, idxBuf, &mrq);

    allocInfo.allocationSize = mrq.size;
    allocInfo.memoryTypeIndex = m_pDriver->GetUploadMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &idxBufMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, idxBuf, idxBufMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else
  {
    // firstVertex
    vertexIndexOffset = drawcall->vertexOffset;
  }

  uint32_t bufStride = 0;
  vector<uint32_t> modSpirv = moduleInfo.spirv.spirv;

  AddOutputDumping(*refl, *pipeInfo.shaders[0].patchData, pipeInfo.shaders[0].entryPoint.c_str(),
                   descSet, vertexIndexOffset, drawcall->instanceOffset, numVerts, modSpirv,
                   bufStride);

  {
    VkDescriptorSetLayout *descSetLayouts;

    // descSet will be the index of our new descriptor set
    descSetLayouts = new VkDescriptorSetLayout[descSet + 1];

    for(uint32_t i = 0; i < descSet; i++)
      descSetLayouts[i] = m_pDriver->GetResourceManager()->GetCurrentHandle<VkDescriptorSetLayout>(
          creationInfo.m_PipelineLayout[pipeInfo.layout].descSetLayouts[i]);

    // this layout just says it has one storage buffer
    descSetLayouts[descSet] = m_MeshFetchDescSetLayout;

    const vector<VkPushConstantRange> &push =
        creationInfo.m_PipelineLayout[pipeInfo.layout].pushRanges;

    VkPipelineLayoutCreateInfo pipeLayoutInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        NULL,
        0,
        descSet + 1,
        descSetLayouts,
        (uint32_t)push.size(),
        push.empty() ? NULL : &push[0],
    };

    // create pipeline layout with same descriptor set layouts, plus our mesh output set
    vkr = m_pDriver->vkCreatePipelineLayout(dev, &pipeLayoutInfo, NULL, &pipeLayout);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    SAFE_DELETE_ARRAY(descSetLayouts);

    // repoint pipeline layout
    pipeCreateInfo.layout = pipeLayout;
  }

  // create vertex shader with modified code
  VkShaderModuleCreateInfo moduleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL,         0,
      modSpirv.size() * sizeof(uint32_t),          &modSpirv[0],
  };

  VkShaderModule module;
  vkr = m_pDriver->vkCreateShaderModule(dev, &moduleCreateInfo, NULL, &module);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // change vertex shader to use our modified code
  for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
  {
    VkPipelineShaderStageCreateInfo &sh =
        (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];
    if(sh.stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
      sh.module = module;
      // entry point name remains the same
      break;
    }
  }

  // create new pipeline
  VkPipeline pipe;
  vkr = m_pDriver->vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipeCreateInfo, NULL,
                                             &pipe);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // make copy of state to draw from
  VulkanRenderState modifiedstate = state;

  // bind created pipeline to partial replay state
  modifiedstate.graphics.pipeline = GetResID(pipe);

  // push back extra descriptor set to partial replay state
  // note that we examined the used pipeline layout above and inserted our descriptor set
  // after any the application used. So there might be more bound, but we want to ensure to
  // bind to the slot we're using
  modifiedstate.graphics.descSets.resize(descSet + 1);
  modifiedstate.graphics.descSets[descSet].descSet = GetResID(m_MeshFetchDescSet);

  if(!(drawcall->flags & DrawFlags::UseIBuffer))
  {
    // create buffer of sufficient size (num indices * bufStride)
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        drawcall->numIndices * drawcall->numInstances * bufStride,
        0,
    };

    bufSize = bufInfo.size;

    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, m_MeshFetchDescSet, 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,         NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
    ObjDisp(cmd)->CmdDraw(Unwrap(cmd), drawcall->numIndices, drawcall->numInstances,
                          drawcall->vertexOffset, drawcall->instanceOffset);
    modifiedstate.EndRenderPass(cmd);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(meshBuffer),
        0,
        bufInfo.size,
    };

    // wait for writing to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0, 0, bufInfo.size,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }
  else
  {
    // create buffer of sufficient size
    // this can't just be bufStride * num unique indices per instance, as we don't
    // have a compact 0-based index to index into the buffer. We must use
    // index-minIndex which is 0-based but potentially sparse, so this buffer may
    // be more or less wasteful
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,          NULL, 0,
        numVerts * drawcall->numInstances * bufStride, 0,
    };

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &meshBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkr = m_pDriver->vkCreateBuffer(dev, &bufInfo, NULL, &readbackBuffer);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};
    m_pDriver->vkGetBufferMemoryRequirements(dev, meshBuffer, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &meshMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, meshBuffer, meshMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    m_pDriver->vkGetBufferMemoryRequirements(dev, readbackBuffer, &mrq);

    allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);

    vkr = m_pDriver->vkAllocateMemory(dev, &allocInfo, NULL, &readbackMem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = m_pDriver->vkBindBufferMemory(dev, readbackBuffer, readbackMem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferMemoryBarrier meshbufbarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_HOST_WRITE_BIT,
        VK_ACCESS_INDEX_READ_BIT,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        Unwrap(uniqIdxBuf),
        0,
        indices.size() * sizeof(uint32_t),
    };

    VkCommandBuffer cmd = m_pDriver->GetNextCmd();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // wait for upload to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // fill destination buffer with 0s to ensure unwritten vertices have sane data
    ObjDisp(dev)->CmdFillBuffer(Unwrap(cmd), Unwrap(meshBuffer), 0, bufInfo.size, 0);

    // wait to finish
    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufInfo.size;
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // set bufSize
    bufSize = numVerts * drawcall->numInstances * bufStride;

    // bind unique'd ibuffer
    modifiedstate.ibuffer.bytewidth = 4;
    modifiedstate.ibuffer.offs = 0;
    modifiedstate.ibuffer.buf = GetResID(uniqIdxBuf);

    // vkUpdateDescriptorSet desc set to point to buffer
    VkDescriptorBufferInfo fetchdesc = {0};
    fetchdesc.buffer = meshBuffer;
    fetchdesc.offset = 0;
    fetchdesc.range = bufInfo.size;

    VkWriteDescriptorSet write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, m_MeshFetchDescSet, 0,   0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      NULL, &fetchdesc,         NULL};
    m_pDriver->vkUpdateDescriptorSets(dev, 1, &write, 0, NULL);

    // do single draw
    modifiedstate.BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
    ObjDisp(cmd)->CmdDrawIndexed(Unwrap(cmd), (uint32_t)indices.size(), drawcall->numInstances, 0,
                                 drawcall->baseVertex, drawcall->instanceOffset);
    modifiedstate.EndRenderPass(cmd);

    // rebase existing index buffer to point to the right elements in our stream-out'd
    // vertex buffer

    // An index buffer could be something like: 500, 520, 518, 553, 554, 556
    // in which case we can't use the existing index buffer without filling 499 slots of vertex
    // data with padding. Instead we rebase the indices based on the smallest index so it becomes
    // 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
    //
    // Note that there could also be gaps in the indices as above which must remain as
    // we don't have a 0-based dense 'vertex id' to base our SSBO indexing off, only index value.

    bool stripRestart = pipeCreateInfo.pInputAssemblyState->primitiveRestartEnable == VK_TRUE &&
                        IsStrip(drawcall->topology);

    if(index16)
    {
      for(uint32_t i = 0; i < numIndices; i++)
      {
        if(stripRestart && idx16[i] == 0xffff)
          continue;

        idx16[i] = idx16[i] - uint16_t(minIndex);
      }
    }
    else
    {
      for(uint32_t i = 0; i < numIndices; i++)
      {
        if(stripRestart && idx32[i] == 0xffffffff)
          continue;

        idx32[i] -= minIndex;
      }
    }

    // upload rebased memory
    byte *idxData = NULL;
    vkr = m_pDriver->vkMapMemory(m_Device, idxBufMem, 0, VK_WHOLE_SIZE, 0, (void **)&idxData);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    memcpy(idxData, idx32, numIndices * idxsize);

    m_pDriver->vkUnmapMemory(m_Device, idxBufMem);

    meshbufbarrier.buffer = Unwrap(idxBuf);
    meshbufbarrier.size = numIndices * idxsize;

    // wait for upload to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    // wait for mesh output writing to finish
    meshbufbarrier.buffer = Unwrap(meshBuffer);
    meshbufbarrier.size = bufSize;
    meshbufbarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    VkBufferCopy bufcopy = {
        0, 0, bufInfo.size,
    };

    // copy to readback buffer
    ObjDisp(dev)->CmdCopyBuffer(Unwrap(cmd), Unwrap(meshBuffer), Unwrap(readbackBuffer), 1, &bufcopy);

    meshbufbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    meshbufbarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    meshbufbarrier.buffer = Unwrap(readbackBuffer);

    // wait for copy to finish
    DoPipelineBarrier(cmd, 1, &meshbufbarrier);

    vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // submit & flush so that we don't have to keep pipeline around for a while
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
  }

  // readback mesh data
  byte *byteData = NULL;
  vkr = m_pDriver->vkMapMemory(m_Device, readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&byteData);

  // do near/far calculations

  float nearp = 0.1f;
  float farp = 100.0f;

  Vec4f *pos0 = (Vec4f *)byteData;

  bool found = false;

  // expect position at the start of the buffer, as system values are sorted first
  // and position is the first value

  for(uint32_t i = 1;
      refl->outputSignature[0].systemValue == ShaderBuiltin::Position && i < numVerts; i++)
  {
    //////////////////////////////////////////////////////////////////////////////////
    // derive near/far, assuming a standard perspective matrix
    //
    // the transformation from from pre-projection {Z,W} to post-projection {Z,W}
    // is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
    // and we know Wpost = Zpre from the perspective matrix.
    // we can then see from the perspective matrix that
    // m = F/(F-N)
    // c = -(F*N)/(F-N)
    //
    // with re-arranging and substitution, we then get:
    // N = -c/m
    // F = c/(1-m)
    //
    // so if we can derive m and c then we can determine N and F. We can do this with
    // two points, and we pick them reasonably distinct on z to reduce floating-point
    // error

    Vec4f *pos = (Vec4f *)(byteData + i * bufStride);

    // skip invalid vertices (w=0)
    if(pos->w != 0.0f && fabs(pos->w - pos0->w) > 0.01f && fabs(pos->z - pos0->z) > 0.01f)
    {
      Vec2f A(pos0->w, pos0->z);
      Vec2f B(pos->w, pos->z);

      float m = (B.y - A.y) / (B.x - A.x);
      float c = B.y - B.x * m;

      if(m == 1.0f)
        continue;

      if(-c / m <= 0.000001f)
        continue;

      nearp = -c / m;
      farp = c / (1 - m);

      found = true;

      break;
    }
  }

  // if we didn't find anything, all z's and w's were identical.
  // If the z is positive and w greater for the first element then
  // we detect this projection as reversed z with infinite far plane
  if(!found && pos0->z > 0.0f && pos0->w > pos0->z)
  {
    nearp = pos0->z;
    farp = FLT_MAX;
  }

  m_pDriver->vkUnmapMemory(m_Device, readbackMem);

  // clean up temporary memories
  m_pDriver->vkDestroyBuffer(m_Device, readbackBuffer, NULL);
  m_pDriver->vkFreeMemory(m_Device, readbackMem, NULL);

  if(uniqIdxBuf != VK_NULL_HANDLE)
  {
    m_pDriver->vkDestroyBuffer(m_Device, uniqIdxBuf, NULL);
    m_pDriver->vkFreeMemory(m_Device, uniqIdxBufMem, NULL);
  }

  // fill out m_PostVSData
  m_PostVSData[eventId].vsin.topo = topo;
  m_PostVSData[eventId].vsout.topo = topo;
  m_PostVSData[eventId].vsout.buf = meshBuffer;
  m_PostVSData[eventId].vsout.bufmem = meshMem;

  m_PostVSData[eventId].vsout.vertStride = bufStride;
  m_PostVSData[eventId].vsout.nearPlane = nearp;
  m_PostVSData[eventId].vsout.farPlane = farp;

  m_PostVSData[eventId].vsout.useIndices = bool(drawcall->flags & DrawFlags::UseIBuffer);
  m_PostVSData[eventId].vsout.numVerts = drawcall->numIndices;

  m_PostVSData[eventId].vsout.instStride = 0;
  if(drawcall->flags & DrawFlags::Instanced)
    m_PostVSData[eventId].vsout.instStride = uint32_t(bufSize / drawcall->numInstances);

  m_PostVSData[eventId].vsout.idxBuf = VK_NULL_HANDLE;
  if(m_PostVSData[eventId].vsout.useIndices && idxBuf != VK_NULL_HANDLE)
  {
    m_PostVSData[eventId].vsout.idxBuf = idxBuf;
    m_PostVSData[eventId].vsout.idxBufMem = idxBufMem;
    m_PostVSData[eventId].vsout.idxFmt =
        state.ibuffer.bytewidth == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  }

  m_PostVSData[eventId].vsout.hasPosOut =
      refl->outputSignature[0].systemValue == ShaderBuiltin::Position;

  // delete pipeline layout
  m_pDriver->vkDestroyPipelineLayout(dev, pipeLayout, NULL);

  // delete pipeline
  m_pDriver->vkDestroyPipeline(dev, pipe, NULL);

  // delete shader/shader module
  m_pDriver->vkDestroyShaderModule(dev, module, NULL);
}
struct VulkanInitPostVSCallback : public VulkanDrawcallCallback
{
  VulkanInitPostVSCallback(WrappedVulkan *vk, const vector<uint32_t> &events)
      : m_pDriver(vk), m_Events(events)
  {
    m_pDriver->SetDrawcallCB(this);
  }
  ~VulkanInitPostVSCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) != m_Events.end())
      m_pDriver->GetReplay()->InitPostVSBuffers(eid);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedraw(uint32_t eid, VkCommandBuffer cmd) {}
  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, VkCommandBuffer cmd) {}
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd) { return false; }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  // Ditto copy/etc
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) { return false; }
  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    if(std::find(m_Events.begin(), m_Events.end(), primary) != m_Events.end())
      m_pDriver->GetReplay()->AliasPostVSBuffers(primary, alias);
  }

  WrappedVulkan *m_pDriver;
  const std::vector<uint32_t> &m_Events;
};

void VulkanReplay::InitPostVSBuffers(const vector<uint32_t> &events)
{
  // first we must replay up to the first event without replaying it. This ensures any
  // non-command buffer calls like memory unmaps etc all happen correctly before this
  // command buffer
  m_pDriver->ReplayLog(0, events.front(), eReplay_WithoutDraw);

  VulkanInitPostVSCallback cb(m_pDriver, events);

  // now we replay the events, which are guaranteed (because we generated them in
  // GetPassEvents above) to come from the same command buffer, so the event IDs are
  // still locally continuous, even if we jump into replaying.
  m_pDriver->ReplayLog(events.front(), events.back(), eReplay_Full);
}

MeshFormat VulkanReplay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage)
{
  // go through any aliasing
  if(m_PostVSAlias.find(eventId) != m_PostVSAlias.end())
    eventId = m_PostVSAlias[eventId];

  VulkanPostVSData postvs;
  RDCEraseEl(postvs);

  if(m_PostVSData.find(eventId) != m_PostVSData.end())
    postvs = m_PostVSData[eventId];

  VulkanPostVSData::StageData s = postvs.GetStage(stage);

  MeshFormat ret;

  if(s.useIndices && s.idxBuf != VK_NULL_HANDLE)
  {
    ret.indexResourceId = GetResID(s.idxBuf);
    ret.indexByteStride = s.idxFmt == VK_INDEX_TYPE_UINT16 ? 2 : 4;
  }
  else
  {
    ret.indexResourceId = ResourceId();
    ret.indexByteStride = 0;
  }
  ret.indexByteOffset = 0;
  ret.baseVertex = 0;

  if(s.buf != VK_NULL_HANDLE)
    ret.vertexResourceId = GetResID(s.buf);
  else
    ret.vertexResourceId = ResourceId();

  ret.vertexByteOffset = s.instStride * instID;
  ret.vertexByteStride = s.vertStride;

  ret.format.compCount = 4;
  ret.format.compByteWidth = 4;
  ret.format.compType = CompType::Float;
  ret.format.type = ResourceFormatType::Regular;
  ret.format.bgraOrder = false;

  ret.showAlpha = false;

  ret.topology = MakePrimitiveTopology(s.topo, 1);
  ret.numIndices = s.numVerts;

  ret.unproject = s.hasPosOut;
  ret.nearPlane = s.nearPlane;
  ret.farPlane = s.farPlane;

  return ret;
}
