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
#include "vk_core.h"
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

private:
  WrappedVulkan *m_pDriver = NULL;
};

ShaderDebugTrace *VulkanReplay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                            uint32_t idx)
{
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

  rdcspv::Debugger *debugger = new rdcspv::Debugger;
  debugger->Parse(shader.spirv.GetSPIRV());
  ShaderDebugTrace *ret = debugger->BeginDebug(apiWrapper, ShaderStage::Vertex, entryPoint, spec,
                                               shadRefl.instructionLines, 0);

  return ret;
}

ShaderDebugTrace *VulkanReplay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                           uint32_t sample, uint32_t primitive)
{
  VULKANNOTIMP("DebugPixel");
  return new ShaderDebugTrace();
}

ShaderDebugTrace *VulkanReplay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                            const uint32_t threadid[3])
{
  VULKANNOTIMP("DebugThread");
  return new ShaderDebugTrace();
}

rdcarray<ShaderDebugState> VulkanReplay::ContinueDebug(ShaderDebugger *debugger)
{
  rdcspv::Debugger *spvDebugger = (rdcspv::Debugger *)debugger;

  if(!spvDebugger)
    return {};

  VkMarkerRegion region("ContinueDebug Simulation Loop");

  return spvDebugger->ContinueDebug();
}
