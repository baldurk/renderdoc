/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include "driver/shaders/dxbc/dxbc_debug.h"
#include "d3d12_debug.h"
#include "d3d12_resources.h"

class D3D12DebugAPIWrapper : public ShaderDebug::DebugAPIWrapper
{
public:
  D3D12DebugAPIWrapper(WrappedID3D12Device *device, DXBC::DXBCContainer *dxbc,
                       const ShaderDebug::GlobalState &globalState);

  void SetCurrentInstruction(uint32_t instruction) { m_instruction = instruction; }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, std::string d);

  bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                              ShaderVariable &output1, ShaderVariable &output2);

  ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource, UINT slot,
                               const char *opString);

  ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, UINT slot, const char *opString);
  ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, UINT slot, uint32_t mipLevel,
                                 int &dim);

  bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                             ShaderDebug::SampleGatherResourceData resourceData,
                             ShaderDebug::SampleGatherSamplerData samplerData, ShaderVariable uv,
                             ShaderVariable ddxCalc, ShaderVariable ddyCalc,
                             const int texelOffsets[3], int multisampleIndex, float lodOrCompareValue,
                             const uint8_t swizzle[4], ShaderDebug::GatherChannel gatherChannel,
                             const char *opString, ShaderVariable &output);

private:
  DXBC::ShaderType GetShaderType() { return m_dxbc ? m_dxbc->m_Type : DXBC::ShaderType::Pixel; }
  WrappedID3D12Device *m_pDevice;
  DXBC::DXBCContainer *m_dxbc;
  const ShaderDebug::GlobalState &m_globalState;
  uint32_t m_instruction;
};

D3D12DebugAPIWrapper::D3D12DebugAPIWrapper(WrappedID3D12Device *device, DXBC::DXBCContainer *dxbc,
                                           const ShaderDebug::GlobalState &globalState)
    : m_pDevice(device), m_dxbc(dxbc), m_globalState(globalState), m_instruction(0)
{
}

void D3D12DebugAPIWrapper::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                           std::string d)
{
  m_pDevice->AddDebugMessage(c, sv, src, d);
}

bool D3D12DebugAPIWrapper::CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode,
                                                  const ShaderVariable &input,
                                                  ShaderVariable &output1, ShaderVariable &output2)
{
  RDCUNIMPLEMENTED("CalculateMathIntrinsic not yet implemented for D3D12");
  return false;
}

ShaderVariable D3D12DebugAPIWrapper::GetSampleInfo(DXBCBytecode::OperandType type,
                                                   bool isAbsoluteResource, UINT slot,
                                                   const char *opString)
{
  RDCUNIMPLEMENTED("GetSampleInfo not yet implemented for D3D12");
  ShaderVariable result("", 0U, 0U, 0U, 0U);
  return result;
}

ShaderVariable D3D12DebugAPIWrapper::GetBufferInfo(DXBCBytecode::OperandType type, UINT slot,
                                                   const char *opString)
{
  RDCUNIMPLEMENTED("GetBufferInfo not yet implemented for D3D12");
  ShaderVariable result("", 0U, 0U, 0U, 0U);
  return result;
}

ShaderVariable D3D12DebugAPIWrapper::GetResourceInfo(DXBCBytecode::OperandType type, UINT slot,
                                                     uint32_t mipLevel, int &dim)
{
  RDCUNIMPLEMENTED("GetResourceInfo not yet implemented for D3D12");
  ShaderVariable result("", 0U, 0U, 0U, 0U);
  return result;
}

bool D3D12DebugAPIWrapper::CalculateSampleGather(
    DXBCBytecode::OpcodeType opcode, ShaderDebug::SampleGatherResourceData resourceData,
    ShaderDebug::SampleGatherSamplerData samplerData, ShaderVariable uv, ShaderVariable ddxCalc,
    ShaderVariable ddyCalc, const int texelOffsets[3], int multisampleIndex,
    float lodOrCompareValue, const uint8_t swizzle[4], ShaderDebug::GatherChannel gatherChannel,
    const char *opString, ShaderVariable &output)
{
  RDCUNIMPLEMENTED("CalculateSampleGather not yet implemented for D3D12");
  return false;
}

ShaderDebug::State D3D12DebugManager::CreateShaderDebugState(ShaderDebugTrace &trace, int quadIdx,
                                                             DXBC::DXBCContainer *dxbc,
                                                             const ShaderReflection &refl,
                                                             bytebuf *cbufData)
{
  RDCUNIMPLEMENTED("CreateShaderDebugState not yet implemented for D3D12");

  using namespace DXBCBytecode;
  using namespace ShaderDebug;

  State initialState = State(quadIdx, &trace, dxbc->GetReflection(), dxbc->GetDXBCByteCode());

  initialState.Init();
  return initialState;
}

void D3D12DebugManager::CreateShaderGlobalState(ShaderDebug::GlobalState &global,
                                                DXBC::DXBCContainer *dxbc)
{
  RDCUNIMPLEMENTED("CreateShaderGlobalState not yet implemented for D3D12");
}

ShaderDebugTrace D3D12Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  RDCUNIMPLEMENTED("Vertex debugging not yet implemented for D3D12");
  ShaderDebugTrace ret;
  return ret;
}

ShaderDebugTrace D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  RDCUNIMPLEMENTED("Pixel debugging not yet implemented for D3D12");
  ShaderDebugTrace ret;
  return ret;
}

ShaderDebugTrace D3D12Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  RDCUNIMPLEMENTED("Compute shader debugging not yet implemented for D3D12");
  ShaderDebugTrace ret;
  return ret;
}
