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

#include "driver/shaders/spirv/spirv_debug.h"
#include "maths/formatpacking.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_replay.h"

class VulkanAPIWrapper : public rdcspv::DebugAPIWrapper
{
public:
  VulkanAPIWrapper(WrappedVulkan *vk) { m_pDriver = vk; }
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                               rdcstr d) override
  {
    m_pDriver->AddDebugMessage(c, sv, src, d);
  }

  virtual void ReadConstantBufferValue(uint32_t set, uint32_t bind, uint32_t offset,
                                       uint32_t byteSize, void *dst) override
  {
    auto it = cbuffers.find(make_rdcpair(set, bind));
    if(it == cbuffers.end())
      return;

    bytebuf &data = it->second;

    if(offset + byteSize <= data.size())
      memcpy(dst, data.data() + offset, byteSize);
  }

  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t location,
                              uint32_t offset) override
  {
    if(builtin != ShaderBuiltin::Undefined)
    {
      auto it = builtin_inputs.find(builtin);
      if(it != builtin_inputs.end())
      {
        var.value = it->second.value;
        return;
      }

      RDCERR("Couldn't get input for %s", ToStr(builtin).c_str());
      return;
    }

    RDCASSERT(offset == 0);

    if(location < location_inputs.size())
    {
      var.value = location_inputs[location].value;
      return;
    }

    RDCERR("Couldn't get input for location=%u, offset=%u", location, offset);
  }

  std::map<rdcpair<uint32_t, uint32_t>, bytebuf> cbuffers;
  std::map<ShaderBuiltin, ShaderVariable> builtin_inputs;
  rdcarray<ShaderVariable> location_inputs;

private:
  WrappedVulkan *m_pDriver = NULL;
};

ShaderDebugTrace *VulkanReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                            uint32_t idx)
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Vertex debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[0].module];
  rdcstr entryPoint = pipe.shaders[0].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[0].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);
  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver);

  for(uint32_t set = 0; set < state.graphics.descSets.size(); set++)
  {
    const VulkanStatePipeline::DescriptorAndOffsets &src = state.graphics.descSets[set];

    const WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[src.descSet];
    ResourceId layoutId = setInfo.layout;

    uint32_t dynOffsetIdx = 0;

    for(uint32_t bind = 0; bind < setInfo.currentBindings.size(); bind++)
    {
      DescriptorSetSlot *info = setInfo.currentBindings[bind];
      const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[bind];

      if(layoutBind.stageFlags == 0)
        continue;

      uint32_t dynOffset = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        dynOffset = src.offsets[dynOffsetIdx++];

      // TODO handle arrays of bindings
      const uint32_t arrayIdx = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      {
        const DescriptorSetSlotBufferInfo &bufInfo = info[arrayIdx].bufferInfo;
        GetDebugManager()->GetBufferData(bufInfo.buffer, bufInfo.offset + dynOffset, bufInfo.range,
                                         apiWrapper->cbuffers[make_rdcpair(set, bind)]);
      }
    }
  }

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::BaseInstance] = ShaderVariable(rdcstr(), draw->instanceOffset, 0U, 0U, 0U);
  builtins[ShaderBuiltin::BaseVertex] = ShaderVariable(
      rdcstr(), (draw->flags & DrawFlags::Indexed) ? draw->baseVertex : draw->vertexOffset, 0U, 0U,
      0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);
  builtins[ShaderBuiltin::VertexIndex] = ShaderVariable(rdcstr(), vertid, 0U, 0U, 0U);
  builtins[ShaderBuiltin::InstanceIndex] = ShaderVariable(rdcstr(), instid, 0U, 0U, 0U);

  rdcarray<ShaderVariable> &locations = apiWrapper->location_inputs;
  for(const VulkanCreationInfo::Pipeline::Attribute &attr : pipe.vertexAttrs)
  {
    if(attr.location >= locations.size())
      locations.resize(attr.location + 1);

    ShaderValue &val = locations[attr.location].value;

    bytebuf data;

    size_t size = GetByteSize(1, 1, 1, attr.format, 0);

    if(attr.binding < pipe.vertexBindings.size())
    {
      const VulkanCreationInfo::Pipeline::Binding &bind = pipe.vertexBindings[attr.binding];

      if(bind.vbufferBinding < state.vbuffers.size())
      {
        const VulkanRenderState::VertBuffer &vb = state.vbuffers[bind.vbufferBinding];

        uint32_t vertexOffset = 0;

        if(bind.perInstance)
        {
          if(bind.instanceDivisor == 0)
            vertexOffset = draw->instanceOffset * bind.bytestride;
          else
            vertexOffset = draw->instanceOffset + (instid / bind.instanceDivisor) * bind.bytestride;
        }
        else
        {
          vertexOffset = idx * bind.bytestride;
        }

        GetDebugManager()->GetBufferData(vb.buf, vb.offs + attr.byteoffset + vertexOffset, size,
                                         data);
      }
    }

    if(size > data.size())
    {
      // out of bounds read
      m_pDriver->AddDebugMessage(
          MessageCategory::Execution, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt(
              "Attribute location %u from binding %u reads out of bounds at vertex %u "
              "(index %u) in instance %u.",
              attr.location, attr.binding, vertid, idx, instid));

      if(IsUIntFormat(attr.format) || IsSIntFormat(attr.format))
        val.u = {0, 0, 0, 1};
      else
        val.f = {0.0f, 0.0f, 0.0f, 1.0f};
    }
    else
    {
      FloatVector decoded = ConvertComponents(MakeResourceFormat(attr.format), data.data());

      val.f.x = decoded.x;
      val.f.y = decoded.y;
      val.f.z = decoded.z;
      val.f.w = decoded.w;
    }
  }

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                           uint32_t sample, uint32_t primitive)
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Pixel debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  if(!m_pDriver->GetDeviceFeatures().fragmentStoresAndAtomics)
  {
    RDCWARN("Pixel debugging is not supported without fragment stores");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(StringFormat::Fmt("DebugPixel @ %u of (%u,%u) sample %u primitive %u",
                                          eventId, x, y, sample, primitive));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Drawcall))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.graphics.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[4].module];
  rdcstr entryPoint = pipe.shaders[4].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[4].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.graphics.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);
  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver);

  for(uint32_t set = 0; set < state.graphics.descSets.size(); set++)
  {
    const VulkanStatePipeline::DescriptorAndOffsets &src = state.graphics.descSets[set];

    const WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[src.descSet];
    ResourceId layoutId = setInfo.layout;

    uint32_t dynOffsetIdx = 0;

    for(uint32_t bind = 0; bind < setInfo.currentBindings.size(); bind++)
    {
      DescriptorSetSlot *info = setInfo.currentBindings[bind];
      const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[bind];

      if(layoutBind.stageFlags == 0)
        continue;

      uint32_t dynOffset = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        dynOffset = src.offsets[dynOffsetIdx++];

      // TODO handle arrays of bindings
      const uint32_t arrayIdx = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      {
        const DescriptorSetSlotBufferInfo &bufInfo = info[arrayIdx].bufferInfo;
        GetDebugManager()->GetBufferData(bufInfo.buffer, bufInfo.offset + dynOffset, bufInfo.range,
                                         apiWrapper->cbuffers[make_rdcpair(set, bind)]);
      }
    }
  }

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);
  builtins[ShaderBuiltin::DrawIndex] = ShaderVariable(rdcstr(), draw->drawIndex, 0U, 0U, 0U);
  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Pixel, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                            const uint32_t threadid[3])
{
  if(!GetAPIProperties().shaderDebugging)
  {
    RDCUNIMPLEMENTED("Compute debugging not yet implemented for Vulkan");
    return new ShaderDebugTrace;
  }

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VkMarkerRegion region(StringFormat::Fmt("DebugThread @ %u of (%u,%u,%u) (%u,%u,%u)", eventId,
                                          groupid[0], groupid[1], groupid[2], threadid[0],
                                          threadid[1], threadid[2]));

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!(draw->flags & DrawFlags::Dispatch))
    return new ShaderDebugTrace();

  const VulkanCreationInfo::Pipeline &pipe = c.m_Pipeline[state.compute.pipeline];
  VulkanCreationInfo::ShaderModule &shader = c.m_ShaderModule[pipe.shaders[5].module];
  rdcstr entryPoint = pipe.shaders[5].entryPoint;
  const rdcarray<SpecConstant> &spec = pipe.shaders[5].specialization;

  VulkanCreationInfo::ShaderModuleReflection &shadRefl =
      shader.GetReflection(entryPoint, state.compute.pipeline);

  shadRefl.PopulateDisassembly(shader.spirv);
  VulkanAPIWrapper *apiWrapper = new VulkanAPIWrapper(m_pDriver);

  for(uint32_t set = 0; set < state.compute.descSets.size(); set++)
  {
    const VulkanStatePipeline::DescriptorAndOffsets &src = state.compute.descSets[set];

    const WrappedVulkan::DescriptorSetInfo &setInfo = m_pDriver->m_DescriptorSetState[src.descSet];
    ResourceId layoutId = setInfo.layout;

    uint32_t dynOffsetIdx = 0;

    for(uint32_t bind = 0; bind < setInfo.currentBindings.size(); bind++)
    {
      DescriptorSetSlot *info = setInfo.currentBindings[bind];
      const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[bind];

      if(layoutBind.stageFlags == 0)
        continue;

      uint32_t dynOffset = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        dynOffset = src.offsets[dynOffsetIdx++];

      // TODO handle arrays of bindings
      const uint32_t arrayIdx = 0;

      if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
         layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
      {
        const DescriptorSetSlotBufferInfo &bufInfo = info[arrayIdx].bufferInfo;
        GetDebugManager()->GetBufferData(bufInfo.buffer, bufInfo.offset + dynOffset, bufInfo.range,
                                         apiWrapper->cbuffers[make_rdcpair(set, bind)]);
      }
    }
  }

  uint32_t threadDim[3];
  threadDim[0] = shadRefl.refl.dispatchThreadsDimension[0];
  threadDim[1] = shadRefl.refl.dispatchThreadsDimension[1];
  threadDim[2] = shadRefl.refl.dispatchThreadsDimension[2];

  std::map<ShaderBuiltin, ShaderVariable> &builtins = apiWrapper->builtin_inputs;
  builtins[ShaderBuiltin::DispatchSize] =
      ShaderVariable(rdcstr(), draw->dispatchDimension[0], draw->dispatchDimension[1],
                     draw->dispatchDimension[2], 0U);
  builtins[ShaderBuiltin::DispatchThreadIndex] = ShaderVariable(
      rdcstr(), groupid[0] * threadDim[0] + threadid[0], groupid[1] * threadDim[1] + threadid[1],
      groupid[2] * threadDim[2] + threadid[2], 0U);
  builtins[ShaderBuiltin::GroupIndex] =
      ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);
  builtins[ShaderBuiltin::GroupSize] =
      ShaderVariable(rdcstr(), threadDim[0], threadDim[1], threadDim[2], 0U);
  builtins[ShaderBuiltin::GroupThreadIndex] =
      ShaderVariable(rdcstr(), threadid[0], threadid[1], threadid[2], 0U);
  builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
      rdcstr(), threadid[2] * threadDim[0] * threadDim[1] + threadid[1] * threadDim[0] + threadid[0],
      0U, 0U, 0U);
  builtins[ShaderBuiltin::DeviceIndex] = ShaderVariable(rdcstr(), 0U, 0U, 0U, 0U);

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Compute, entryPoint, spec,
                                               shadRefl.instructionLines, shadRefl.patchData, 0);

  return ret;
}

rdcarray<ShaderDebugState> VulkanReplay::ContinueDebug(ShaderDebugger *debugger)
{
  rdcspv::Debugger *spvDebugger = (rdcspv::Debugger *)debugger;

  if(!spvDebugger)
    return {};

  VkMarkerRegion region("ContinueDebug Simulation Loop");

  return spvDebugger->ContinueDebug();
}
