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

#pragma once

#include "driver/shaders/dxil/dxil_debug.h"
#include "d3d12_device.h"
#include "d3d12_shaderdebug.h"
#include "d3d12_state.h"

namespace DXILDebug
{
class Debugger;

void FetchConstantBufferData(WrappedID3D12Device *device, const DXIL::Program *program,
                             const D3D12RenderState::RootSignature &rootsig,
                             const ShaderReflection &refl, DXILDebug::GlobalState &global,
                             rdcarray<SourceVariableMapping> &sourceVars);

class D3D12APIWrapper : public DebugAPIWrapper
{
public:
  D3D12APIWrapper(WrappedID3D12Device *device, const DXBC::DXBCContainer *dxbcContainer,
                  GlobalState &globalState, uint32_t eventId);
  ~D3D12APIWrapper();

  void FetchSRV(const BindingSlot &slot);
  void FetchUAV(const BindingSlot &slot);
  bool CalculateMathIntrinsic(DXIL::DXOp dxOp, const ShaderVariable &input, ShaderVariable &output);
  bool CalculateSampleGather(DXIL::DXOp dxOp, SampleGatherResourceData resourceData,
                             SampleGatherSamplerData samplerData, const ShaderVariable &uv,
                             const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                             const int8_t texelOffsets[3], int multisampleIndex,
                             float lodOrCompareValue, const uint8_t swizzle[4],
                             GatherChannel gatherChannel, DXBC::ShaderType shaderType,
                             uint32_t instructionIdx, const char *opString, ShaderVariable &output);
  ShaderVariable GetResourceInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                 uint32_t mipLevel, const DXBC::ShaderType shaderType, int &dim);
  ShaderVariable GetSampleInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                               const DXBC::ShaderType shaderType, const char *opString);
  ShaderVariable GetRenderTargetSampleInfo(const DXBC::ShaderType shaderType, const char *opString);
  bool IsResourceBound(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot);

private:
  bool IsSRVBound(const BindingSlot &slot);
  bool IsUAVBound(const BindingSlot &slot);

  WrappedID3D12Device *m_Device;
  const DXBC::DXBCContainer *m_DXBC;
  GlobalState &m_GlobalState;
  DXBC::ShaderType m_ShaderType;
  const uint32_t m_EventId;
  bool m_DidReplay = false;
};

};
