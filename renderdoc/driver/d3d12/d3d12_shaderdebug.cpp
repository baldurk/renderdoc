/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/formatpacking.h"
#include "strings/string_utils.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

struct DebugHit
{
  uint32_t numHits;
  float posx;
  float posy;
  float depth;
  uint32_t primitive;
  uint32_t isFrontFace;
  uint32_t sample;
  uint32_t coverage;
  uint32_t rawdata;    // arbitrary, depending on shader
};

static bool IsShaderParameterVisible(DXBC::ShaderType shaderType,
                                     D3D12_SHADER_VISIBILITY shaderVisibility)
{
  if(shaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
    return true;

  if(shaderType == DXBC::ShaderType::Vertex && shaderVisibility == D3D12_SHADER_VISIBILITY_VERTEX)
    return true;

  if(shaderType == DXBC::ShaderType::Pixel && shaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL)
    return true;

  if(shaderType == DXBC::ShaderType::Amplification &&
     shaderVisibility == D3D12_SHADER_VISIBILITY_AMPLIFICATION)
    return true;

  if(shaderType == DXBC::ShaderType::Mesh && shaderVisibility == D3D12_SHADER_VISIBILITY_MESH)
    return true;

  return false;
}

class D3D12DebugAPIWrapper : public DXBCDebug::DebugAPIWrapper
{
public:
  D3D12DebugAPIWrapper(WrappedID3D12Device *device, const DXBC::DXBCContainer *dxbc,
                       DXBCDebug::GlobalState &globalState, uint32_t eid);
  ~D3D12DebugAPIWrapper();

  void SetCurrentInstruction(uint32_t instruction) { m_instruction = instruction; }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d);

  void FetchSRV(const DXBCDebug::BindingSlot &slot);
  void FetchUAV(const DXBCDebug::BindingSlot &slot);

  bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                              ShaderVariable &output1, ShaderVariable &output2);

  ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource,
                               const DXBCDebug::BindingSlot &slot, const char *opString);
  ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot,
                               const char *opString);

  D3D12Descriptor FindDescriptor(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot);

  ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot,
                                 uint32_t mipLevel, int &dim);

  bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                             DXBCDebug::SampleGatherResourceData resourceData,
                             DXBCDebug::SampleGatherSamplerData samplerData, ShaderVariable uv,
                             ShaderVariable ddxCalc, ShaderVariable ddyCalc,
                             const int8_t texelOffsets[3], int multisampleIndex,
                             float lodOrCompareValue, const uint8_t swizzle[4],
                             DXBCDebug::GatherChannel gatherChannel, const char *opString,
                             ShaderVariable &output);

private:
  DXBC::ShaderType GetShaderType() { return m_dxbc ? m_dxbc->m_Type : DXBC::ShaderType::Pixel; }
  WrappedID3D12Device *m_pDevice;
  const DXBC::DXBCContainer *m_dxbc;
  DXBCDebug::GlobalState &m_globalState;
  uint32_t m_instruction;
  uint32_t m_EventID;
  bool m_DidReplay = false;
};

D3D12DebugAPIWrapper::D3D12DebugAPIWrapper(WrappedID3D12Device *device,
                                           const DXBC::DXBCContainer *dxbc,
                                           DXBCDebug::GlobalState &globalState, uint32_t eid)
    : m_pDevice(device), m_dxbc(dxbc), m_globalState(globalState), m_instruction(0), m_EventID(eid)
{
}

D3D12DebugAPIWrapper::~D3D12DebugAPIWrapper()
{
  // if we replayed to before the action for fetching some UAVs, replay back to after the action to
  // keep
  // the state consistent.
  if(m_DidReplay)
  {
    D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "ResetReplay");
    // replay the action to get back to 'normal' state for this event, and mark that we need to
    // replay back to pristine state next time we need to fetch data.
    m_pDevice->ReplayLog(0, m_EventID, eReplay_OnlyDraw);
  }
}

void D3D12DebugAPIWrapper::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                           rdcstr d)
{
  m_pDevice->AddDebugMessage(c, sv, src, d);
}

void D3D12DebugAPIWrapper::FetchSRV(const DXBCDebug::BindingSlot &slot)
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(GetShaderType() == DXBC::ShaderType::Compute)
  {
    if(rs.compute.rootsig != ResourceId())
    {
      pRootSignature = &rs.compute;
    }
  }
  else if(rs.graphics.rootsig != ResourceId())
  {
    pRootSignature = &rs.graphics;
  }

  DXBCDebug::GlobalState::SRVData &srvData = m_globalState.srvs[slot];

  if(pRootSignature)
  {
    WrappedID3D12RootSignature *pD3D12RootSig =
        rm->GetCurrentAs<WrappedID3D12RootSignature>(pRootSignature->rootsig);

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(GetShaderType(), param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && element.type == eRootSRV)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            // Found the requested SRV
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);

            if(pResource)
            {
              D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

              // DXBC allows root buffers to have a stride of up to 16 bytes in the shader, which
              // means encoding the byte offset into the first element here is wrong without knowing
              // what the actual accessed stride is. Instead we only fetch the data from that offset
              // onwards.

              // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
              // resource desc format or the DXBC reflection info might be more correct.
              DXBCDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, srvData.format);
              srvData.firstElement = 0;
              // root arguments have no bounds checking, so use the most conservative number of
              // elements
              srvData.numElements = uint32_t(resDesc.Width - element.offset);

              if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                m_pDevice->GetDebugManager()->GetBufferData(pResource, element.offset, 0,
                                                            srvData.data);
            }

            return;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                element.type == eRootTable)
        {
          UINT prevTableOffset = 0;
          WrappedID3D12DescriptorHeap *heap =
              rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

          size_t numRanges = param.ranges.size();
          for(size_t r = 0; r < numRanges; ++r)
          {
            const D3D12_DESCRIPTOR_RANGE1 &range = param.ranges[r];

            // For every range, check the number of descriptors so that we are accessing the
            // correct data for append descriptor tables, even if the range type doesn't match
            // what we need to fetch
            UINT offset = range.OffsetInDescriptorsFromTableStart;
            if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
              offset = prevTableOffset;

            D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
            desc += element.offset;
            desc += offset;

            UINT numDescriptors = range.NumDescriptors;
            if(numDescriptors == UINT_MAX)
            {
              // Find out how many descriptors are left after
              numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

              // TODO: Should we look up the bind point in the D3D12 state to try to get
              // a better guess at the number of descriptors?
            }

            prevTableOffset = offset + numDescriptors;

            // Check if the range is for SRVs and the slot we want is contained
            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV &&
               slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              if(desc)
              {
                ResourceId srvId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);

                if(pResource)
                {
                  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc->GetSRV();
                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
                    srvDesc = MakeSRVDesc(pResource->GetDesc());

                  if(srvDesc.Format != DXGI_FORMAT_UNKNOWN)
                  {
                    DXBCDebug::FillViewFmt(srvDesc.Format, srvData.format);
                  }
                  else
                  {
                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                      srvData.format.stride = srvDesc.Buffer.StructureByteStride;

                      // If we didn't get a type from the SRV description, try to pull it from the
                      // shader reflection info
                      DXBCDebug::LookupSRVFormatFromShaderReflection(*m_dxbc->GetReflection(), slot,
                                                                     srvData.format);
                    }
                  }

                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
                  {
                    srvData.firstElement = (uint32_t)srvDesc.Buffer.FirstElement;
                    srvData.numElements = srvDesc.Buffer.NumElements;

                    m_pDevice->GetDebugManager()->GetBufferData(pResource, 0, 0, srvData.data);
                  }

                  // Textures are sampled via a pixel shader, so there's no need to copy their data
                }

                return;
              }
            }
          }
        }
      }
    }

    RDCERR("Couldn't find root signature parameter corresponding to SRV %u in space %u",
           slot.shaderRegister, slot.registerSpace);
    return;
  }

  RDCERR("No root signature bound, couldn't identify SRV %u in space %u", slot.shaderRegister,
         slot.registerSpace);
}

void D3D12DebugAPIWrapper::FetchUAV(const DXBCDebug::BindingSlot &slot)
{
  // if the UAV might be dirty from side-effects from the action, replay back to right
  // before it.
  if(!m_DidReplay)
  {
    D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "un-dirtying resources");
    m_pDevice->ReplayLog(0, m_EventID, eReplay_WithoutDraw);
    m_DidReplay = true;
  }

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(GetShaderType() == DXBC::ShaderType::Compute)
  {
    if(rs.compute.rootsig != ResourceId())
    {
      pRootSignature = &rs.compute;
    }
  }
  else if(rs.graphics.rootsig != ResourceId())
  {
    pRootSignature = &rs.graphics;
  }

  DXBCDebug::GlobalState::UAVData &uavData = m_globalState.uavs[slot];

  if(pRootSignature)
  {
    WrappedID3D12RootSignature *pD3D12RootSig =
        rm->GetCurrentAs<WrappedID3D12RootSignature>(pRootSignature->rootsig);

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(GetShaderType(), param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && element.type == eRootUAV)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            // Found the requested UAV
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);

            if(pResource)
            {
              D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

              // DXBC allows root buffers to have a stride of up to 16 bytes in the shader, which
              // means encoding the byte offset into the first element here is wrong without knowing
              // what the actual accessed stride is. Instead we only fetch the data from that offset
              // onwards.

              // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
              // resource desc format or the DXBC reflection info might be more correct.
              DXBCDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, uavData.format);
              uavData.firstElement = 0;
              // root arguments have no bounds checking, so use the most conservative number of
              // elements
              uavData.numElements = uint32_t(resDesc.Width - element.offset);

              if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                m_pDevice->GetDebugManager()->GetBufferData(pResource, element.offset, 0,
                                                            uavData.data);
            }

            return;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                element.type == eRootTable)
        {
          UINT prevTableOffset = 0;
          WrappedID3D12DescriptorHeap *heap =
              rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

          size_t numRanges = param.ranges.size();
          for(size_t r = 0; r < numRanges; ++r)
          {
            const D3D12_DESCRIPTOR_RANGE1 &range = param.ranges[r];

            // For every range, check the number of descriptors so that we are accessing the
            // correct data for append descriptor tables, even if the range type doesn't match
            // what we need to fetch
            UINT offset = range.OffsetInDescriptorsFromTableStart;
            if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
              offset = prevTableOffset;

            D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
            desc += element.offset;
            desc += offset;

            UINT numDescriptors = range.NumDescriptors;
            if(numDescriptors == UINT_MAX)
            {
              // Find out how many descriptors are left after
              numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

              // TODO: Should we look up the bind point in the D3D12 state to try to get
              // a better guess at the number of descriptors?
            }

            prevTableOffset = offset + numDescriptors;

            // Check if the range is for UAVs and the slot we want is contained
            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV &&
               slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              if(desc)
              {
                ResourceId uavId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);

                if(pResource)
                {
                  // TODO: Need to fetch counter resource if applicable

                  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = desc->GetUAV();

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
                    uavDesc = MakeUAVDesc(pResource->GetDesc());

                  if(uavDesc.Format != DXGI_FORMAT_UNKNOWN)
                  {
                    DXBCDebug::FillViewFmt(uavDesc.Format, uavData.format);
                  }
                  else
                  {
                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                      uavData.format.stride = uavDesc.Buffer.StructureByteStride;

                      // TODO: Try looking up UAV from shader reflection info?
                    }
                  }

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
                  {
                    uavData.firstElement = (uint32_t)uavDesc.Buffer.FirstElement;
                    uavData.numElements = uavDesc.Buffer.NumElements;

                    m_pDevice->GetDebugManager()->GetBufferData(pResource, 0, 0, uavData.data);
                  }
                  else
                  {
                    uavData.tex = true;
                    m_pDevice->GetReplay()->GetTextureData(uavId, Subresource(),
                                                           GetTextureDataParams(), uavData.data);

                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    uavData.rowPitch = GetByteSize((int)resDesc.Width, 1, 1, uavDesc.Format, 0);
                  }
                }

                return;
              }
            }
          }
        }
      }
    }

    RDCERR("Couldn't find root signature parameter corresponding to UAV %u in space %u",
           slot.shaderRegister, slot.registerSpace);
    return;
  }

  RDCERR("No root signature bound, couldn't identify UAV %u in space %u", slot.shaderRegister,
         slot.registerSpace);
}

bool D3D12DebugAPIWrapper::CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode,
                                                  const ShaderVariable &input,
                                                  ShaderVariable &output1, ShaderVariable &output2)
{
  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "CalculateMathIntrinsic");

  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_RCP == DEBUG_SAMPLE_MATH_RCP,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_RSQ == DEBUG_SAMPLE_MATH_RSQ,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_EXP == DEBUG_SAMPLE_MATH_EXP,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_LOG == DEBUG_SAMPLE_MATH_LOG,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SINCOS == DEBUG_SAMPLE_MATH_SINCOS,
                    "Opcode enum doesn't match shader define");

  if(opcode != DXBCBytecode::OPCODE_RCP && opcode != DXBCBytecode::OPCODE_RSQ &&
     opcode != DXBCBytecode::OPCODE_EXP && opcode != DXBCBytecode::OPCODE_LOG &&
     opcode != DXBCBytecode::OPCODE_SINCOS)
  {
    // To support a new instruction, the shader created in
    // D3D12DebugManager::CreateShaderDebugResources will need updated
    RDCERR("Unsupported instruction for CalculateMathIntrinsic: %u", opcode);
    return false;
  }

  ID3D12Resource *pResultBuffer = m_pDevice->GetDebugManager()->GetShaderDebugResultBuffer();
  ID3D12Resource *pReadbackBuffer = m_pDevice->GetDebugManager()->GetReadbackBuffer();

  DebugMathOperation cbufferData = {};
  memcpy(&cbufferData.mathInVal, input.value.f32v.data(), sizeof(Vec4f));
  cbufferData.mathOp = (int)opcode;

  // Set root signature & sig params on command list, then execute the shader
  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  m_pDevice->GetDebugManager()->SetDescriptorHeaps(cmdList, true, false);
  cmdList->SetPipelineState(m_pDevice->GetDebugManager()->GetMathIntrinsicsPso());
  cmdList->SetComputeRootSignature(m_pDevice->GetDebugManager()->GetShaderDebugRootSig());
  cmdList->SetComputeRootConstantBufferView(
      0, m_pDevice->GetDebugManager()->UploadConstants(&cbufferData, sizeof(cbufferData)));
  cmdList->SetComputeRootUnorderedAccessView(1, pResultBuffer->GetGPUVirtualAddress());
  cmdList->Dispatch(1, 1, 1);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = pResultBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  cmdList->ResourceBarrier(1, &barrier);

  cmdList->CopyBufferRegion(pReadbackBuffer, 0, pResultBuffer, 0, sizeof(Vec4f) * 6);

  HRESULT hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  {
    ID3D12CommandList *l = cmdList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();
  }

  D3D12_RANGE range = {0, sizeof(Vec4f) * 6};

  byte *results = NULL;
  hr = pReadbackBuffer->Map(0, &range, (void **)&results);

  if(FAILED(hr))
  {
    pReadbackBuffer->Unmap(0, &range);
    RDCERR("Failed to map readback buffer HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  memcpy(output1.value.u32v.data(), results, sizeof(Vec4f));
  memcpy(output2.value.u32v.data(), results + sizeof(Vec4f), sizeof(Vec4f));

  range.End = 0;
  pReadbackBuffer->Unmap(0, &range);

  return true;
}

D3D12Descriptor D3D12DebugAPIWrapper::FindDescriptor(DXBCBytecode::OperandType type,
                                                     const DXBCDebug::BindingSlot &slot)
{
  D3D12Descriptor descriptor;

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(GetShaderType() == DXBC::ShaderType::Compute)
  {
    if(rs.compute.rootsig != ResourceId())
    {
      pRootSignature = &rs.compute;
    }
  }
  else if(rs.graphics.rootsig != ResourceId())
  {
    pRootSignature = &rs.graphics;
  }

  if(pRootSignature)
  {
    WrappedID3D12RootSignature *pD3D12RootSig =
        rm->GetCurrentAs<WrappedID3D12RootSignature>(pRootSignature->rootsig);

    D3D12_DESCRIPTOR_RANGE_TYPE searchRangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

    if(type == DXBCBytecode::TYPE_SAMPLER)
      searchRangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    else if(type == DXBCBytecode::TYPE_RESOURCE)
      searchRangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    else if(type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
      searchRangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    else if(type == DXBCBytecode::TYPE_CONSTANT_BUFFER)
      searchRangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

    if(searchRangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
    {
      for(const D3D12_STATIC_SAMPLER_DESC1 &samp : pD3D12RootSig->sig.StaticSamplers)
      {
        if(samp.RegisterSpace == slot.registerSpace && samp.ShaderRegister == slot.shaderRegister)
        {
          D3D12_SAMPLER_DESC2 desc;

          desc.Filter = samp.Filter;
          desc.AddressU = samp.AddressU;
          desc.AddressV = samp.AddressV;
          desc.AddressW = samp.AddressW;
          desc.MipLODBias = samp.MipLODBias;
          desc.MaxAnisotropy = samp.MaxAnisotropy;
          desc.ComparisonFunc = samp.ComparisonFunc;
          switch(samp.BorderColor)
          {
            default:
            case D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK:
              desc.FloatBorderColor[0] = desc.FloatBorderColor[1] = desc.FloatBorderColor[2] =
                  desc.FloatBorderColor[3] = 0.0f;
              break;
            case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK:
              desc.FloatBorderColor[0] = desc.FloatBorderColor[1] = desc.FloatBorderColor[2] = 0.0f;
              desc.FloatBorderColor[3] = 1.0f;
              break;
            case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE:
              desc.FloatBorderColor[0] = desc.FloatBorderColor[1] = desc.FloatBorderColor[2] =
                  desc.FloatBorderColor[3] = 1.0f;
              break;
            case D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT:
              desc.UintBorderColor[0] = desc.UintBorderColor[1] = desc.UintBorderColor[2] = 0;
              desc.UintBorderColor[3] = 1;
              break;
            case D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT:
              desc.UintBorderColor[0] = desc.UintBorderColor[1] = desc.UintBorderColor[2] =
                  desc.UintBorderColor[3] = 1;
              break;
          }
          desc.MinLOD = samp.MinLOD;
          desc.MaxLOD = samp.MaxLOD;
          desc.Flags = samp.Flags;

          descriptor.Init(&desc);
          return descriptor;
        }
      }
    }

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(GetShaderType(), param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && element.type == eRootSRV &&
           type == DXBCBytecode::TYPE_RESOURCE)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.Buffer.FirstElement = 0;
            // we don't know the real length or structure stride from a root descriptor, so set
            // defaults. This behaviour seems undefined in drivers, so returning 1 as the number of
            // elements is as sensible as anything else
            srvDesc.Buffer.NumElements = 1;
            srvDesc.Buffer.StructureByteStride = 4;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            descriptor.Init(pResource, &srvDesc);
            return descriptor;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && element.type == eRootUAV &&
                type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.Buffer.FirstElement = 0;
            // we don't know the real length or structure stride from a root descriptor, so set
            // defaults. This behaviour seems undefined in drivers, so returning 1 as the number of
            // elements is as sensible as anything else
            uavDesc.Buffer.NumElements = 1;
            uavDesc.Buffer.StructureByteStride = 4;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            descriptor.Init(pResource, NULL, &uavDesc);
            return descriptor;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
                element.type == eRootTable)
        {
          UINT prevTableOffset = 0;
          WrappedID3D12DescriptorHeap *heap =
              rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

          size_t numRanges = param.ranges.size();
          for(size_t r = 0; r < numRanges; ++r)
          {
            const D3D12_DESCRIPTOR_RANGE1 &range = param.ranges[r];

            // For every range, check the number of descriptors so that we are accessing the
            // correct data for append descriptor tables, even if the range type doesn't match
            // what we need to fetch
            UINT offset = range.OffsetInDescriptorsFromTableStart;
            if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
              offset = prevTableOffset;

            UINT numDescriptors = range.NumDescriptors;
            if(numDescriptors == UINT_MAX)
            {
              // Find out how many descriptors are left after
              numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

              // TODO: Should we look up the bind point in the D3D12 state to try to get
              // a better guess at the number of descriptors?
            }

            prevTableOffset = offset + numDescriptors;

            if(range.RangeType != searchRangeType)
              continue;

            D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
            desc += element.offset;
            desc += offset;

            // Check if the slot we want is contained
            if(slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              return *desc;
            }
          }
        }
      }
    }
  }

  return descriptor;
}

ShaderVariable D3D12DebugAPIWrapper::GetSampleInfo(DXBCBytecode::OperandType type,
                                                   bool isAbsoluteResource,
                                                   const DXBCDebug::BindingSlot &slot,
                                                   const char *opString)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(type == DXBCBytecode::TYPE_RASTERIZER)
  {
    if(GetShaderType() != DXBC::ShaderType::Compute)
    {
      const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

      // try depth first - both should match sample count though to be valid
      ResourceId res = rs.GetDSVID();
      if(res == ResourceId() && !rs.rts.empty())
        res = rs.rts[0].GetResResourceId();

      ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(res);
      D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
      result.value.u32v[0] = resDesc.SampleDesc.Count;
      result.value.u32v[1] = 0;
      result.value.u32v[2] = 0;
      result.value.u32v[3] = 0;
    }
    return result;
  }

  D3D12Descriptor descriptor = FindDescriptor(type, slot);

  if(descriptor.GetType() == D3D12DescriptorType::SRV &&
     type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ResourceId srvId = descriptor.GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = descriptor.GetSRV();
    if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      srvDesc = MakeSRVDesc(resDesc);

    if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS ||
       srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
    {
      result.value.u32v[0] = resDesc.SampleDesc.Count;
      result.value.u32v[1] = 0;
      result.value.u32v[2] = 0;
      result.value.u32v[3] = 0;
    }
    else
    {
      RDCERR("Invalid resource dimension for GetSampleInfo");
    }
  }

  return result;
}

ShaderVariable D3D12DebugAPIWrapper::GetBufferInfo(DXBCBytecode::OperandType type,
                                                   const DXBCDebug::BindingSlot &slot,
                                                   const char *opString)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  D3D12Descriptor descriptor = FindDescriptor(type, slot);

  if(descriptor.GetType() == D3D12DescriptorType::SRV &&
     type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(descriptor.GetResResourceId());
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = descriptor.GetSRV();
    if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      srvDesc = MakeSRVDesc(resDesc);

    if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
    {
      result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = result.value.u32v[3] =
          (uint32_t)srvDesc.Buffer.NumElements;
    }
  }

  if(descriptor.GetType() == D3D12DescriptorType::UAV &&
     type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(descriptor.GetResResourceId());
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = descriptor.GetUAV();

    if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
      uavDesc = MakeUAVDesc(resDesc);

    if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
    {
      result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = result.value.u32v[3] =
          (uint32_t)uavDesc.Buffer.NumElements;
    }
  }

  return result;
}

ShaderVariable D3D12DebugAPIWrapper::GetResourceInfo(DXBCBytecode::OperandType type,
                                                     const DXBCDebug::BindingSlot &slot,
                                                     uint32_t mipLevel, int &dim)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  D3D12Descriptor descriptor = FindDescriptor(type, slot);

  if(descriptor.GetType() == D3D12DescriptorType::UAV &&
     type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ResourceId uavId = descriptor.GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = descriptor.GetUAV();

    if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
      uavDesc = MakeUAVDesc(resDesc);

    switch(uavDesc.ViewDimension)
    {
      case D3D12_UAV_DIMENSION_UNKNOWN:
      case D3D12_UAV_DIMENSION_BUFFER:
      {
        RDCWARN("Invalid view dimension for GetResourceInfo");
        break;
      }
      case D3D12_UAV_DIMENSION_TEXTURE1D:
      case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
      {
        dim = 1;

        bool isarray = uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1DARRAY;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = isarray ? uavDesc.Texture1DArray.ArraySize : 0;
        result.value.u32v[2] = 0;

        // spec says "For UAVs (u#), the number of mip levels is always 1."
        result.value.u32v[3] = 1;

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = 0;

        break;
      }
      case D3D12_UAV_DIMENSION_TEXTURE2D:
      case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
      {
        dim = 2;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

        if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D)
          result.value.u32v[2] = 0;
        else if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
          result.value.u32v[2] = uavDesc.Texture2DArray.ArraySize;

        // spec says "For UAVs (u#), the number of mip levels is always 1."
        result.value.u32v[3] = 1;

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = 0;

        break;
      }
      case D3D12_UAV_DIMENSION_TEXTURE2DMS:
      case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY:
      {
        // note, DXBC doesn't support MSAA UAVs so this is here mostly for completeness and sanity
        dim = 2;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

        if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DMS)
          result.value.u32v[2] = 0;
        else if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY)
          result.value.u32v[2] = uavDesc.Texture2DMSArray.ArraySize;

        // spec says "For UAVs (u#), the number of mip levels is always 1."
        result.value.u32v[3] = 1;

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = 0;

        break;
      }
      case D3D12_UAV_DIMENSION_TEXTURE3D:
      {
        dim = 3;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));
        result.value.u32v[2] = RDCMAX(1U, (uint32_t)(resDesc.DepthOrArraySize >> mipLevel));

        // spec says "For UAVs (u#), the number of mip levels is always 1."
        result.value.u32v[3] = 1;

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = 0;

        break;
      }
    }

    return result;
  }
  else if(descriptor.GetType() == D3D12DescriptorType::SRV &&
          type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ResourceId srvId = descriptor.GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = descriptor.GetSRV();
    if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      srvDesc = MakeSRVDesc(resDesc);
    switch(srvDesc.ViewDimension)
    {
      case D3D12_SRV_DIMENSION_UNKNOWN:
      case D3D12_SRV_DIMENSION_BUFFER:
      {
        RDCWARN("Invalid view dimension for GetResourceInfo");
        break;
      }
      case D3D12_SRV_DIMENSION_TEXTURE1D:
      case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
      {
        dim = 1;

        bool isarray = srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1DARRAY;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = isarray ? srvDesc.Texture1DArray.ArraySize : 0;
        result.value.u32v[2] = 0;
        result.value.u32v[3] =
            isarray ? srvDesc.Texture1DArray.MipLevels : srvDesc.Texture1D.MipLevels;

        if(isarray && (result.value.u32v[1] == 0 || result.value.u32v[1] == ~0U))
          result.value.u32v[1] = resDesc.DepthOrArraySize;

        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = resDesc.MipLevels;
        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = CalcNumMips((int)resDesc.Width, resDesc.Height, 1);

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = 0;

        break;
      }
      case D3D12_SRV_DIMENSION_TEXTURE2D:
      case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
      case D3D12_SRV_DIMENSION_TEXTURE2DMS:
      case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
      {
        dim = 2;
        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

        if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
        {
          result.value.u32v[2] = 0;
          result.value.u32v[3] = srvDesc.Texture2D.MipLevels;
        }
        else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
        {
          result.value.u32v[2] = srvDesc.Texture2DArray.ArraySize;
          result.value.u32v[3] = srvDesc.Texture2DArray.MipLevels;

          if(result.value.u32v[2] == 0 || result.value.u32v[2] == ~0U)
            result.value.u32v[2] = resDesc.DepthOrArraySize;
        }
        else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS)
        {
          result.value.u32v[2] = 0;
          result.value.u32v[3] = 1;
        }
        else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
        {
          result.value.u32v[2] = srvDesc.Texture2DMSArray.ArraySize;
          result.value.u32v[3] = 1;

          if(result.value.u32v[2] == 0 || result.value.u32v[2] == ~0U)
            result.value.u32v[2] = resDesc.DepthOrArraySize;
        }

        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = resDesc.MipLevels;
        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = CalcNumMips((int)resDesc.Width, resDesc.Height, 1);

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = 0;

        break;
      }
      case D3D12_SRV_DIMENSION_TEXTURE3D:
      {
        dim = 3;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));
        result.value.u32v[2] = RDCMAX(1U, (uint32_t)(resDesc.DepthOrArraySize >> mipLevel));
        result.value.u32v[3] = srvDesc.Texture3D.MipLevels;

        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = resDesc.MipLevels;
        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] =
              CalcNumMips((int)resDesc.Width, resDesc.Height, resDesc.DepthOrArraySize);

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = 0;

        break;
      }
      case D3D12_SRV_DIMENSION_TEXTURECUBE:
      case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
      {
        // Even though it's a texture cube, an individual face's dimensions are
        // returned
        dim = 2;

        bool isarray = srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

        // the spec says "If srcResource is a TextureCubeArray, [...]. dest.z is set
        // to an undefined value."
        // but that's stupid, and implementations seem to return the number of cubes
        result.value.u32v[2] = isarray ? srvDesc.TextureCubeArray.NumCubes : 0;
        result.value.u32v[3] =
            isarray ? srvDesc.TextureCubeArray.MipLevels : srvDesc.TextureCube.MipLevels;

        if(result.value.u32v[2] == 0 || result.value.u32v[2] == ~0U)
          result.value.u32v[2] = resDesc.DepthOrArraySize / 6;

        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = resDesc.MipLevels;
        if(result.value.u32v[3] == 0 || result.value.u32v[3] == ~0U)
          result.value.u32v[3] = CalcNumMips((int)resDesc.Width, resDesc.Height, 1);

        if(mipLevel >= result.value.u32v[3])
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] = 0;

        break;
      }
      case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
      {
        RDCERR("Raytracing is unsupported");
        break;
      }
    }
  }

  return result;
}

bool D3D12DebugAPIWrapper::CalculateSampleGather(
    DXBCBytecode::OpcodeType opcode, DXBCDebug::SampleGatherResourceData resourceData,
    DXBCDebug::SampleGatherSamplerData samplerData, ShaderVariable uv, ShaderVariable ddxCalc,
    ShaderVariable ddyCalc, const int8_t texelOffsets[3], int multisampleIndex,
    float lodOrCompareValue, const uint8_t swizzle[4], DXBCDebug::GatherChannel gatherChannel,
    const char *opString, ShaderVariable &output)
{
  using namespace DXBCBytecode;

  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SAMPLE == DEBUG_SAMPLE_TEX_SAMPLE,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SAMPLE_L == DEBUG_SAMPLE_TEX_SAMPLE_L,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SAMPLE_B == DEBUG_SAMPLE_TEX_SAMPLE_B,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SAMPLE_C == DEBUG_SAMPLE_TEX_SAMPLE_C,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SAMPLE_D == DEBUG_SAMPLE_TEX_SAMPLE_D,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_SAMPLE_C_LZ == DEBUG_SAMPLE_TEX_SAMPLE_C_LZ,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_GATHER4 == DEBUG_SAMPLE_TEX_GATHER4,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_GATHER4_C == DEBUG_SAMPLE_TEX_GATHER4_C,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_GATHER4_PO == DEBUG_SAMPLE_TEX_GATHER4_PO,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_GATHER4_PO_C == DEBUG_SAMPLE_TEX_GATHER4_PO_C,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_LOD == DEBUG_SAMPLE_TEX_LOD,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_LD == DEBUG_SAMPLE_TEX_LD,
                    "Opcode enum doesn't match shader define");
  RDCCOMPILE_ASSERT((int)DXBCBytecode::OPCODE_LD_MS == DEBUG_SAMPLE_TEX_LD_MS,
                    "Opcode enum doesn't match shader define");

  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "CalculateSampleGather");

  for(uint32_t i = 0; i < ddxCalc.columns; i++)
  {
    if(!RDCISFINITE(ddxCalc.value.f32v[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddxCalc.value.f32v[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddx - using 0.0 instead",
                                                   m_instruction, opString));
    }
    if(!RDCISFINITE(ddyCalc.value.f32v[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddyCalc.value.f32v[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddy - using 0.0 instead",
                                                   m_instruction, opString));
    }
  }

  for(uint32_t i = 0; i < uv.columns; i++)
  {
    if(opcode != OPCODE_LD && opcode != OPCODE_LD_MS && (!RDCISFINITE(uv.value.f32v[i])))
    {
      RDCWARN("NaN or Inf in texlookup");
      uv.value.f32v[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup uv - using 0.0 instead",
                                                   m_instruction, opString));
    }
  }

  // set array slice selection to 0 if the resource is declared non-arrayed

  if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE1D)
    uv.value.f32v[1] = 0.0f;
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2D ||
          resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
          resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBE)
    uv.value.f32v[2] = 0.0f;

  DebugSampleOperation cbufferData = {};

  memcpy(&cbufferData.debugSampleUV, uv.value.u32v.data(), sizeof(Vec4f));
  memcpy(&cbufferData.debugSampleDDX, ddxCalc.value.u32v.data(), sizeof(Vec4f));
  memcpy(&cbufferData.debugSampleDDY, ddyCalc.value.u32v.data(), sizeof(Vec4f));
  memcpy(&cbufferData.debugSampleUVInt, uv.value.u32v.data(), sizeof(Vec4f));

  if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE1D ||
     resourceData.dim == RESOURCE_DIMENSION_TEXTURE1DARRAY)
  {
    cbufferData.debugSampleTexDim = DEBUG_SAMPLE_TEX1D;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2D ||
          resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY)
  {
    cbufferData.debugSampleTexDim = DEBUG_SAMPLE_TEX2D;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE3D)
  {
    cbufferData.debugSampleTexDim = DEBUG_SAMPLE_TEX3D;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
          resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
  {
    cbufferData.debugSampleTexDim = DEBUG_SAMPLE_TEXMS;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBE ||
          resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBEARRAY)
  {
    cbufferData.debugSampleTexDim = DEBUG_SAMPLE_TEXCUBE;
  }
  else
  {
    RDCERR("Unsupported resource type %d in sample operation", resourceData.dim);
  }

  int retTypes[DXBC::NUM_RETURN_TYPES] = {
      0,                     // RETURN_TYPE_UNKNOWN
      DEBUG_SAMPLE_UNORM,    // RETURN_TYPE_UNORM
      DEBUG_SAMPLE_SNORM,    // RETURN_TYPE_UNORM
      DEBUG_SAMPLE_INT,      // RETURN_TYPE_SINT
      DEBUG_SAMPLE_UINT,     // RETURN_TYPE_UINT
      DEBUG_SAMPLE_FLOAT,    // RETURN_TYPE_FLOAT
      0,                     // RETURN_TYPE_MIXED
      DEBUG_SAMPLE_FLOAT,    // RETURN_TYPE_DOUBLE (treat as floats)
      0,                     // RETURN_TYPE_CONTINUED
      0,                     // RETURN_TYPE_UNUSED
  };

  cbufferData.debugSampleRetType = retTypes[resourceData.retType];

  cbufferData.debugSampleGatherChannel = (int)gatherChannel;
  cbufferData.debugSampleSampleIndex = multisampleIndex;
  cbufferData.debugSampleOperation = (int)opcode;
  cbufferData.debugSampleLodCompare = lodOrCompareValue;

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12RenderState prevState = rs;

  ID3D12RootSignature *sig = m_pDevice->GetDebugManager()->GetShaderDebugRootSig();
  ID3D12PipelineState *pso = m_pDevice->GetDebugManager()->GetTexSamplePso(texelOffsets);

  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  rs.pipe = GetResID(pso);
  rs.rts.clear();
  // Set viewport/scissor unconditionally - we need to set this all the time for sampling for a
  // compute shader, but also a graphics action might exclude pixel (0, 0) from its view or scissor
  rs.views.clear();
  rs.views.push_back({0, 0, 1, 1, 0, 1});
  rs.scissors.clear();
  rs.scissors.push_back({0, 0, 1, 1});

  D3D12_CPU_DESCRIPTOR_HANDLE srv = m_pDevice->GetDebugManager()->GetCPUHandle(FIRST_SHADDEBUG_SRV);
  srv.ptr += ((cbufferData.debugSampleTexDim - 1) + 5 * (cbufferData.debugSampleRetType - 1)) *
             sizeof(D3D12Descriptor);
  {
    D3D12Descriptor descriptor = FindDescriptor(DXBCBytecode::TYPE_RESOURCE, resourceData.binding);

    descriptor.Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_pDevice, srv);
  }

  if(samplerData.mode != NUM_SAMPLERS)
  {
    D3D12Descriptor descriptor = FindDescriptor(DXBCBytecode::TYPE_SAMPLER, samplerData.binding);

    D3D12_CPU_DESCRIPTOR_HANDLE samp = m_pDevice->GetDebugManager()->GetCPUHandle(SHADDEBUG_SAMPLER0);

    if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ || opcode == OPCODE_GATHER4_C ||
       opcode == OPCODE_GATHER4_PO_C)
      samp.ptr += sizeof(D3D12Descriptor);

    descriptor.Create(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, m_pDevice, samp);
  }

  m_pDevice->GetDebugManager()->SetDescriptorHeaps(rs.heaps, true, true);

  // Set our modified root signature, and transfer sigelems if we're debugging a compute shader
  rs.graphics.rootsig = GetResID(sig);
  rs.graphics.sigelems.clear();
  rs.compute.rootsig = ResourceId();
  rs.compute.sigelems.clear();

  ID3D12Resource *pResultBuffer = m_pDevice->GetDebugManager()->GetShaderDebugResultBuffer();
  ID3D12Resource *pReadbackBuffer = m_pDevice->GetDebugManager()->GetReadbackBuffer();

  rs.graphics.sigelems = {
      D3D12RenderState::SignatureElement(eRootCBV, m_pDevice->GetDebugManager()->UploadConstants(
                                                       &cbufferData, sizeof(cbufferData))),
      D3D12RenderState::SignatureElement(eRootUAV, pResultBuffer->GetGPUVirtualAddress()),
      D3D12RenderState::SignatureElement(
          eRootTable, m_pDevice->GetDebugManager()->GetCPUHandle(FIRST_SHADDEBUG_SRV)),
      D3D12RenderState::SignatureElement(
          eRootTable, m_pDevice->GetDebugManager()->GetCPUHandle(SHADDEBUG_SAMPLER0)),
  };

  rs.topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  rs.ApplyState(m_pDevice, cmdList);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_pDevice->GetDebugManager()->GetCPUHandle(PICK_PIXEL_RTV);
  cmdList->OMSetRenderTargets(1, &rtv, FALSE, NULL);
  cmdList->DrawInstanced(3, 1, 0, 0);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = pResultBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  cmdList->ResourceBarrier(1, &barrier);

  cmdList->CopyBufferRegion(pReadbackBuffer, 0, pResultBuffer, 0, sizeof(Vec4f) * 6);

  HRESULT hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  {
    ID3D12CommandList *l = cmdList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();
  }

  rs = prevState;

  D3D12_RANGE range = {0, sizeof(Vec4f) * 6};

  void *results = NULL;
  hr = pReadbackBuffer->Map(0, &range, &results);

  if(FAILED(hr))
  {
    pReadbackBuffer->Unmap(0, &range);
    RDCERR("Failed to map readback buffer HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  ShaderVariable lookupResult("tex", 0.0f, 0.0f, 0.0f, 0.0f);

  float *retFloats = (float *)results;
  uint32_t *retUInts = (uint32_t *)(retFloats + 8);
  int32_t *retSInts = (int32_t *)(retUInts + 8);

  if(cbufferData.debugSampleRetType == DEBUG_SAMPLE_UINT)
  {
    for(int i = 0; i < 4; i++)
      lookupResult.value.u32v[i] = retUInts[swizzle[i]];
  }
  else if(cbufferData.debugSampleRetType == DEBUG_SAMPLE_INT)
  {
    for(int i = 0; i < 4; i++)
      lookupResult.value.s32v[i] = retSInts[swizzle[i]];
  }
  else
  {
    for(int i = 0; i < 4; i++)
      lookupResult.value.f32v[i] = retFloats[swizzle[i]];
  }

  range.End = 0;
  pReadbackBuffer->Unmap(0, &range);

  output = lookupResult;

  return true;
}

void GatherConstantBuffers(WrappedID3D12Device *pDevice, const DXBCBytecode::Program &program,
                           const D3D12RenderState::RootSignature &rootsig,
                           const ShaderReflection &refl, const ShaderBindpointMapping &mapping,
                           DXBCDebug::GlobalState &global,
                           rdcarray<SourceVariableMapping> &sourceVars)
{
  WrappedID3D12RootSignature *pD3D12RootSig =
      pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootsig.rootsig);

  size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), rootsig.sigelems.size());
  for(size_t i = 0; i < numParams; i++)
  {
    const D3D12RootSignatureParameter &rootSigParam = pD3D12RootSig->sig.Parameters[i];
    const D3D12RenderState::SignatureElement &element = rootsig.sigelems[i];
    if(IsShaderParameterVisible(program.GetShaderType(), rootSigParam.ShaderVisibility))
    {
      if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
         element.type == eRootConst)
      {
        DXBCDebug::BindingSlot slot(rootSigParam.Constants.ShaderRegister,
                                    rootSigParam.Constants.RegisterSpace);
        UINT sizeBytes = sizeof(uint32_t) * RDCMIN(rootSigParam.Constants.Num32BitValues,
                                                   (UINT)element.constants.size());
        bytebuf cbufData((const byte *)element.constants.data(), sizeBytes);
        AddCBufferToGlobalState(program, global, sourceVars, refl, mapping, slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && element.type == eRootCBV)
      {
        DXBCDebug::BindingSlot slot(rootSigParam.Descriptor.ShaderRegister,
                                    rootSigParam.Descriptor.RegisterSpace);
        ID3D12Resource *cbv = pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(element.id);
        bytebuf cbufData;
        pDevice->GetDebugManager()->GetBufferData(cbv, element.offset, 0, cbufData);
        AddCBufferToGlobalState(program, global, sourceVars, refl, mapping, slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
              element.type == eRootTable)
      {
        UINT prevTableOffset = 0;
        WrappedID3D12DescriptorHeap *heap =
            pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12DescriptorHeap>(element.id);

        size_t numRanges = rootSigParam.ranges.size();
        for(size_t r = 0; r < numRanges; r++)
        {
          // For this traversal we only care about CBV descriptor ranges, but we still need to
          // calculate the table offsets in case a descriptor table has a combination of
          // different range types
          const D3D12_DESCRIPTOR_RANGE1 &range = rootSigParam.ranges[r];

          UINT offset = range.OffsetInDescriptorsFromTableStart;
          if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
            offset = prevTableOffset;

          D3D12Descriptor *desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
          desc += element.offset;
          desc += offset;

          UINT numDescriptors = range.NumDescriptors;
          if(numDescriptors == UINT_MAX)
          {
            // Find out how many descriptors are left after
            numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

            // TODO: Look up the bind point in the D3D12 state to try to get
            // a better guess at the number of descriptors
          }

          prevTableOffset = offset + numDescriptors;

          if(range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
            continue;

          DXBCDebug::BindingSlot slot(range.BaseShaderRegister, range.RegisterSpace);

          bytebuf cbufData;
          for(UINT n = 0; n < numDescriptors; ++n, ++slot.shaderRegister)
          {
            const D3D12_CONSTANT_BUFFER_VIEW_DESC &cbv = desc->GetCBV();
            ResourceId resId;
            uint64_t byteOffset = 0;
            WrappedID3D12Resource::GetResIDFromAddr(cbv.BufferLocation, resId, byteOffset);
            ID3D12Resource *pCbvResource =
                pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(resId);
            cbufData.clear();

            if(cbv.SizeInBytes > 0)
              pDevice->GetDebugManager()->GetBufferData(pCbvResource, byteOffset, cbv.SizeInBytes,
                                                        cbufData);
            AddCBufferToGlobalState(program, global, sourceVars, refl, mapping, slot, cbufData);

            desc++;
          }
        }
      }
    }
  }
}

ShaderDebugTrace *D3D12Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                           uint32_t idx, uint32_t view)
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D12MarkerRegion region(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12PipelineState *pso =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(!pso || !pso->IsGraphics())
  {
    RDCERR("Can't debug with no current graphics pipeline");
    return new ShaderDebugTrace;
  }

  WrappedID3D12Shader *vs = (WrappedID3D12Shader *)pso->graphics->VS.pShaderBytecode;
  if(!vs)
  {
    RDCERR("Can't debug with no current vertex shader");
    return new ShaderDebugTrace;
  }

  DXBC::DXBCContainer *dxbc = vs->GetDXBC();
  const ShaderReflection &refl = vs->GetDetails();

  if(!dxbc)
  {
    RDCERR("Vertex shader couldn't be reflected");
    return new ShaderDebugTrace;
  }

  if(!refl.debugInfo.debuggable)
  {
    RDCERR("Vertex shader is not debuggable");
    return new ShaderDebugTrace;
  }

  dxbc->GetDisassembly();

  const ActionDescription *action = m_pDevice->GetAction(eventId);

  rdcarray<D3D12_INPUT_ELEMENT_DESC> inputlayout;
  uint32_t numElements = pso->graphics->InputLayout.NumElements;
  inputlayout.reserve(numElements);
  for(uint32_t i = 0; i < numElements; ++i)
    inputlayout.push_back(pso->graphics->InputLayout.pInputElementDescs[i]);

  std::set<UINT> vertexbuffers;
  uint32_t trackingOffs[32] = {0};

  UINT MaxStepRate = 1U;

  // need special handling for other step rates
  for(size_t i = 0; i < inputlayout.size(); i++)
  {
    if(inputlayout[i].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA &&
       inputlayout[i].InstanceDataStepRate < action->numInstances)
      MaxStepRate = RDCMAX(inputlayout[i].InstanceDataStepRate, MaxStepRate);

    UINT slot =
        RDCCLAMP(inputlayout[i].InputSlot, 0U, UINT(D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1));

    vertexbuffers.insert(slot);

    if(inputlayout[i].AlignedByteOffset == ~0U)
    {
      inputlayout[i].AlignedByteOffset = trackingOffs[slot];
    }
    else
    {
      trackingOffs[slot] = inputlayout[i].AlignedByteOffset;
    }

    ResourceFormat fmt = MakeResourceFormat(inputlayout[i].Format);

    trackingOffs[slot] += fmt.compByteWidth * fmt.compCount;
  }

  bytebuf vertData[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  bytebuf *instData = new bytebuf[MaxStepRate * D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  bytebuf staticData[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

  for(auto it = vertexbuffers.begin(); it != vertexbuffers.end(); ++it)
  {
    UINT i = *it;
    if(rs.vbuffers.size() > i)
    {
      const D3D12RenderState::VertBuffer &vb = rs.vbuffers[i];
      ID3D12Resource *buffer = m_pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(vb.buf);

      if(vb.stride * (action->vertexOffset + idx) < vb.size)
        GetDebugManager()->GetBufferData(buffer, vb.offs + vb.stride * (action->vertexOffset + idx),
                                         vb.stride, vertData[i]);

      for(UINT isr = 1; isr <= MaxStepRate; isr++)
      {
        if((action->instanceOffset + (instid / isr)) < vb.size)
          GetDebugManager()->GetBufferData(
              buffer, vb.offs + vb.stride * (action->instanceOffset + (instid / isr)), vb.stride,
              instData[i * MaxStepRate + isr - 1]);
      }

      if(vb.stride * action->instanceOffset < vb.size)
        GetDebugManager()->GetBufferData(buffer, vb.offs + vb.stride * action->instanceOffset,
                                         vb.stride, staticData[i]);
    }
  }

  InterpretDebugger *interpreter = new InterpretDebugger;
  interpreter->eventId = eventId;
  ShaderDebugTrace *ret = interpreter->BeginDebug(dxbc, refl, vs->GetMapping(), 0);
  GlobalState &global = interpreter->global;
  ThreadState &state = interpreter->activeLane();

  // Fetch constant buffer data from root signature
  GatherConstantBuffers(m_pDevice, *dxbc->GetDXBCByteCode(), rs.graphics, refl,
                        pso->VS()->GetMapping(), global, ret->sourceVars);

  for(size_t i = 0; i < state.inputs.size(); i++)
  {
    if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::Undefined ||
       dxbc->GetReflection()->InputSig[i].systemValue ==
           ShaderBuiltin::Position)    // SV_Position seems to get promoted
                                       // automatically, but it's invalid for
                                       // vertex input
    {
      const D3D12_INPUT_ELEMENT_DESC *el = NULL;

      rdcstr signame = strlower(dxbc->GetReflection()->InputSig[i].semanticName);

      for(size_t l = 0; l < inputlayout.size(); l++)
      {
        rdcstr layoutname = strlower(inputlayout[l].SemanticName);

        if(signame == layoutname &&
           dxbc->GetReflection()->InputSig[i].semanticIndex == inputlayout[l].SemanticIndex)
        {
          el = &inputlayout[l];
          break;
        }
        if(signame == layoutname + ToStr(inputlayout[l].SemanticIndex))
        {
          el = &inputlayout[l];
          break;
        }
      }

      RDCASSERT(el);

      if(!el)
        continue;

      byte *srcData = NULL;
      size_t dataSize = 0;

      if(el->InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA)
      {
        if(vertData[el->InputSlot].size() >= el->AlignedByteOffset)
        {
          srcData = &vertData[el->InputSlot][el->AlignedByteOffset];
          dataSize = vertData[el->InputSlot].size() - el->AlignedByteOffset;
        }
      }
      else
      {
        if(el->InstanceDataStepRate == 0 || el->InstanceDataStepRate >= action->numInstances)
        {
          if(staticData[el->InputSlot].size() >= el->AlignedByteOffset)
          {
            srcData = &staticData[el->InputSlot][el->AlignedByteOffset];
            dataSize = staticData[el->InputSlot].size() - el->AlignedByteOffset;
          }
        }
        else
        {
          UINT isrIdx = el->InputSlot * MaxStepRate + (el->InstanceDataStepRate - 1);
          if(instData[isrIdx].size() >= el->AlignedByteOffset)
          {
            srcData = &instData[isrIdx][el->AlignedByteOffset];
            dataSize = instData[isrIdx].size() - el->AlignedByteOffset;
          }
        }
      }

      ResourceFormat fmt = MakeResourceFormat(el->Format);

      // more data needed than is provided
      if(dxbc->GetReflection()->InputSig[i].compCount > fmt.compCount)
      {
        state.inputs[i].value.u32v[3] = 1;

        if(fmt.compType == CompType::Float)
          state.inputs[i].value.f32v[3] = 1.0f;
      }

      // interpret resource format types
      if(fmt.Special())
      {
        Vec3f *v3 = (Vec3f *)state.inputs[i].value.f32v.data();
        Vec4f *v4 = (Vec4f *)state.inputs[i].value.f32v.data();

        // only pull in all or nothing from these,
        // if there's only e.g. 3 bytes remaining don't read and unpack some of
        // a 4-byte resource format type
        size_t packedsize = 4;
        if(fmt.type == ResourceFormatType::R5G5B5A1 || fmt.type == ResourceFormatType::R5G6B5 ||
           fmt.type == ResourceFormatType::R4G4B4A4)
          packedsize = 2;

        if(srcData == NULL || packedsize > dataSize)
        {
          state.inputs[i].value.u32v[0] = state.inputs[i].value.u32v[1] =
              state.inputs[i].value.u32v[2] = state.inputs[i].value.u32v[3] = 0;
        }
        else if(fmt.type == ResourceFormatType::R5G5B5A1)
        {
          RDCASSERT(fmt.BGRAOrder());
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v4 = ConvertFromB5G5R5A1(packed);
        }
        else if(fmt.type == ResourceFormatType::R5G6B5)
        {
          RDCASSERT(fmt.BGRAOrder());
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v3 = ConvertFromB5G6R5(packed);
        }
        else if(fmt.type == ResourceFormatType::R4G4B4A4)
        {
          RDCASSERT(fmt.BGRAOrder());
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v4 = ConvertFromB4G4R4A4(packed);
        }
        else if(fmt.type == ResourceFormatType::R10G10B10A2)
        {
          uint32_t packed = ((uint32_t *)srcData)[0];

          if(fmt.compType == CompType::UInt)
          {
            state.inputs[i].value.u32v[2] = (packed >> 0) & 0x3ff;
            state.inputs[i].value.u32v[1] = (packed >> 10) & 0x3ff;
            state.inputs[i].value.u32v[0] = (packed >> 20) & 0x3ff;
            state.inputs[i].value.u32v[3] = (packed >> 30) & 0x003;
          }
          else
          {
            *v4 = ConvertFromR10G10B10A2(packed);
          }
        }
        else if(fmt.type == ResourceFormatType::R11G11B10)
        {
          uint32_t packed = ((uint32_t *)srcData)[0];
          *v3 = ConvertFromR11G11B10(packed);
        }
      }
      else
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          if(srcData == NULL || fmt.compByteWidth > dataSize)
          {
            state.inputs[i].value.u32v[c] = 0;
            continue;
          }

          dataSize -= fmt.compByteWidth;

          if(fmt.compByteWidth == 1)
          {
            byte *src = srcData + c * fmt.compByteWidth;

            if(fmt.compType == CompType::UInt)
              state.inputs[i].value.u32v[c] = *src;
            else if(fmt.compType == CompType::SInt)
              state.inputs[i].value.s32v[c] = *((int8_t *)src);
            else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
              state.inputs[i].value.f32v[c] = float(*src) / 255.0f;
            else if(fmt.compType == CompType::SNorm)
            {
              signed char *schar = (signed char *)src;

              // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
              if(*schar == -128)
                state.inputs[i].value.f32v[c] = -1.0f;
              else
                state.inputs[i].value.f32v[c] = float(*schar) / 127.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 2)
          {
            uint16_t *src = (uint16_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float)
              state.inputs[i].value.f32v[c] = ConvertFromHalf(*src);
            else if(fmt.compType == CompType::UInt)
              state.inputs[i].value.u32v[c] = *src;
            else if(fmt.compType == CompType::SInt)
              state.inputs[i].value.s32v[c] = *((int16_t *)src);
            else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
              state.inputs[i].value.f32v[c] = float(*src) / float(UINT16_MAX);
            else if(fmt.compType == CompType::SNorm)
            {
              int16_t *sint = (int16_t *)src;

              // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
              if(*sint == -32768)
                state.inputs[i].value.f32v[c] = -1.0f;
              else
                state.inputs[i].value.f32v[c] = float(*sint) / 32767.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 4)
          {
            uint32_t *src = (uint32_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float || fmt.compType == CompType::UInt ||
               fmt.compType == CompType::SInt)
              memcpy(&state.inputs[i].value.u32v[c], src, 4);
            else
              RDCERR("Unexpected component type");
          }
        }

        if(fmt.BGRAOrder())
        {
          RDCASSERT(fmt.compCount == 4);
          std::swap(state.inputs[i].value.f32v[2], state.inputs[i].value.f32v[0]);
        }
      }
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::VertexIndex)
    {
      uint32_t sv_vertid = vertid;

      if(action->flags & ActionFlags::Indexed)
        sv_vertid = idx - action->baseVertex;

      if(dxbc->GetReflection()->InputSig[i].varType == VarType::Float)
        state.inputs[i].value.f32v[0] = state.inputs[i].value.f32v[1] =
            state.inputs[i].value.f32v[2] = state.inputs[i].value.f32v[3] = (float)sv_vertid;
      else
        state.inputs[i].value.u32v[0] = state.inputs[i].value.u32v[1] =
            state.inputs[i].value.u32v[2] = state.inputs[i].value.u32v[3] = sv_vertid;
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::InstanceIndex)
    {
      if(dxbc->GetReflection()->InputSig[i].varType == VarType::Float)
        state.inputs[i].value.f32v[0] = state.inputs[i].value.f32v[1] =
            state.inputs[i].value.f32v[2] = state.inputs[i].value.f32v[3] = (float)instid;
      else
        state.inputs[i].value.u32v[0] = state.inputs[i].value.u32v[1] =
            state.inputs[i].value.u32v[2] = state.inputs[i].value.u32v[3] = instid;
    }
    else
    {
      RDCERR("Unhandled system value semantic on VS input");
    }
  }

  ret->constantBlocks = global.constantBlocks;
  ret->inputs = state.inputs;

  delete[] instData;

  dxbc->FillTraceLineInfo(*ret);

  return ret;
}

ShaderDebugTrace *D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                          const DebugPixelInputs &inputs)
{
  using namespace DXBC;
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  uint32_t sample = inputs.sample;
  uint32_t primitive = inputs.primitive;

  D3D12MarkerRegion debugpixRegion(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugPixel @ %u of (%u,%u) %u / %u", eventId, x, y, sample, primitive));

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12PipelineState *pso =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(!pso || !pso->IsGraphics())
  {
    RDCERR("Can't debug with no current graphics pipeline");
    return new ShaderDebugTrace;
  }

  WrappedID3D12Shader *ps = (WrappedID3D12Shader *)pso->graphics->PS.pShaderBytecode;
  if(!ps)
  {
    RDCERR("Can't debug with no current pixel shader");
    return new ShaderDebugTrace;
  }

  DXBCContainer *dxbc = ps->GetDXBC();
  const ShaderReflection &refl = ps->GetDetails();

  if(!dxbc)
  {
    RDCERR("Pixel shader couldn't be reflected");
    return new ShaderDebugTrace;
  }

  if(!refl.debugInfo.debuggable)
  {
    RDCERR("Pixel shader is not debuggable");
    return new ShaderDebugTrace;
  }

  dxbc->GetDisassembly();

  // Fetch the previous stage's disassembly, to match outputs to PS inputs
  DXBCContainer *prevDxbc = NULL;
  // Check for geometry shader first
  {
    WrappedID3D12Shader *gs = (WrappedID3D12Shader *)pso->graphics->GS.pShaderBytecode;
    if(gs)
      prevDxbc = gs->GetDXBC();
  }
  // Check for domain shader next
  if(prevDxbc == NULL)
  {
    WrappedID3D12Shader *ds = (WrappedID3D12Shader *)pso->graphics->DS.pShaderBytecode;
    if(ds)
      prevDxbc = ds->GetDXBC();
  }
  // Check for vertex shader last
  if(prevDxbc == NULL)
  {
    WrappedID3D12Shader *vs = (WrappedID3D12Shader *)pso->graphics->VS.pShaderBytecode;
    if(vs)
      prevDxbc = vs->GetDXBC();
  }

  rdcarray<PSInputElement> initialValues;
  rdcarray<rdcstr> floatInputs;
  rdcarray<rdcstr> inputVarNames;
  rdcstr extractHlsl;
  int structureStride = 0;

  DXBCDebug::GatherPSInputDataForInitialValues(dxbc, *prevDxbc->GetReflection(), initialValues,
                                               floatInputs, inputVarNames, extractHlsl,
                                               structureStride);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  // If the pipe contains a geometry shader, then SV_PrimitiveID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  bool usePrimitiveID = (prevDxbc->m_Type != ShaderType::Geometry);
  for(const PSInputElement &e : initialValues)
  {
    if(e.sysattribute == ShaderBuiltin::PrimitiveIndex)
    {
      usePrimitiveID = true;
      break;
    }
  }

  // Store a copy of the event's render state to restore later
  D3D12RenderState prevState = rs;

  // Fetch the multisample count from the PSO
  WrappedID3D12PipelineState *origPSO =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
  origPSO->Fill(pipeDesc);
  uint32_t outputSampleCount = RDCMAX(1U, pipeDesc.SampleDesc.Count);

  std::set<GlobalState::SampleEvalCacheKey> evalSampleCacheData;
  uint64_t sampleEvalRegisterMask = 0;

  // if we're not rendering at MSAA, no need to fill the cache because evaluates will all return the
  // plain input anyway.
  if(outputSampleCount > 1)
  {
    // scan the instructions to see if it contains any evaluates.
    size_t numInstructions = dxbc->GetDXBCByteCode()->GetNumInstructions();
    for(size_t i = 0; i < numInstructions; ++i)
    {
      const Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(i);

      // skip any non-eval opcodes
      if(op.operation != OPCODE_EVAL_CENTROID && op.operation != OPCODE_EVAL_SAMPLE_INDEX &&
         op.operation != OPCODE_EVAL_SNAPPED)
        continue;

      // the generation of this key must match what we'll generate in the corresponding lookup
      GlobalState::SampleEvalCacheKey key;

      // all the eval opcodes have rDst, vIn as the first two operands
      key.inputRegisterIndex = (int32_t)op.operands[1].indices[0].index;

      for(int c = 0; c < 4; c++)
      {
        if(op.operands[0].comps[c] == 0xff)
          break;

        key.numComponents = c + 1;
      }

      key.firstComponent = op.operands[1].comps[op.operands[0].comps[0]];

      sampleEvalRegisterMask |= 1ULL << key.inputRegisterIndex;

      if(op.operation == OPCODE_EVAL_CENTROID)
      {
        // nothing to do - default key is centroid, sample is -1 and offset x/y is 0
        evalSampleCacheData.insert(key);
      }
      else if(op.operation == OPCODE_EVAL_SAMPLE_INDEX)
      {
        if(op.operands[2].type == TYPE_IMMEDIATE32 || op.operands[2].type == TYPE_IMMEDIATE64)
        {
          // hooray, only sampling a single index, just add this key
          key.sample = (int32_t)op.operands[2].values[0];

          evalSampleCacheData.insert(key);
        }
        else
        {
          // parameter is a register and we don't know which sample will be needed, fetch them all.
          // In most cases this will be a loop over them all, so they'll all be needed anyway
          for(uint32_t c = 0; c < outputSampleCount; c++)
          {
            key.sample = (int32_t)c;
            evalSampleCacheData.insert(key);
          }
        }
      }
      else if(op.operation == OPCODE_EVAL_SNAPPED)
      {
        if(op.operands[2].type == TYPE_IMMEDIATE32 || op.operands[2].type == TYPE_IMMEDIATE64)
        {
          // hooray, only sampling a single offset, just add this key
          key.offsetx = (int32_t)op.operands[2].values[0];
          key.offsety = (int32_t)op.operands[2].values[1];

          evalSampleCacheData.insert(key);
        }
        else
        {
          m_pDevice->AddDebugMessage(
              MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
              "EvaluateAttributeSnapped called with dynamic parameter, caching all possible "
              "evaluations which could have performance impact.");

          for(key.offsetx = -8; key.offsetx <= 7; key.offsetx++)
            for(key.offsety = -8; key.offsety <= 7; key.offsety++)
              evalSampleCacheData.insert(key);
        }
      }
    }
  }

  extractHlsl += R"(
struct PSInitialData
{
  // metadata we need ourselves
  uint hit;
  float3 pos;
  uint prim;
  uint fface;
  uint sample;
  uint covge;
  float derivValid;

  // input values
  PSInput IN;
  PSInput INddx;
  PSInput INddy;
  PSInput INddxfine;
  PSInput INddyfine;
};

)";

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rs.graphics.rootsig);

  // Need to be able to add a descriptor table with our UAV without hitting the 64 DWORD limit
  RDCASSERT(sig->sig.dwordLength < 64);
  D3D12RootSignature modsig = sig->sig;

  UINT regSpace = GetFreeRegSpace(modsig, 0, D3D12DescriptorType::UAV, D3D12_SHADER_VISIBILITY_PIXEL);

  // If this event uses MSAA, then at least one render target must be preserved to get
  // multisampling info. leave u0 alone and start with register u1
  extractHlsl += StringFormat::Fmt(
      "RWStructuredBuffer<PSInitialData> PSInitialBuffer : register(u1, space%u);\n\n", regSpace);

  if(!evalSampleCacheData.empty())
  {
    // float4 is wasteful in some cases but it's easier than using byte buffers and manual packing
    extractHlsl +=
        StringFormat::Fmt("RWBuffer<float4> PSEvalBuffer : register(u2, space%u);\n\n", regSpace);
  }

  if(usePrimitiveID)
  {
    extractHlsl += R"(
void ExtractInputsPS(PSInput IN,
                     float4 debug_pixelPos : SV_Position,
                     uint prim : SV_PrimitiveID,
                     uint fface : SV_IsFrontFace,
                     uint sample : SV_SampleIndex,
                     uint covge : SV_Coverage)
{
)";
  }
  else
  {
    extractHlsl += R"(
void ExtractInputsPS(PSInput IN,
                     float4 debug_pixelPos : SV_Position,
                     uint fface : SV_IsFrontFace,
                     uint sample : SV_SampleIndex,
                     uint covge : SV_Coverage)
{
)";
  }

  extractHlsl += "  uint idx = " + ToStr(overdrawLevels) + ";\n";
  extractHlsl += StringFormat::Fmt(
      "  if(abs(debug_pixelPos.x - %u.5) < 0.5f && abs(debug_pixelPos.y - %u.5) < 0.5f)\n", x, y);
  extractHlsl += "    InterlockedAdd(PSInitialBuffer[0].hit, 1, idx);\n\n";
  extractHlsl += "  idx = min(idx, " + ToStr(overdrawLevels) + ");\n\n";
  extractHlsl += "  PSInitialBuffer[idx].pos = debug_pixelPos.xyz;\n";

  if(usePrimitiveID)
    extractHlsl += "  PSInitialBuffer[idx].prim = prim;\n";
  else
    extractHlsl += "  PSInitialBuffer[idx].prim = 0;\n";

  extractHlsl += "  PSInitialBuffer[idx].fface = fface;\n";
  extractHlsl += "  PSInitialBuffer[idx].covge = covge;\n";
  extractHlsl += "  PSInitialBuffer[idx].sample = sample;\n";
  extractHlsl += "  PSInitialBuffer[idx].IN = IN;\n";
  extractHlsl += "  PSInitialBuffer[idx].derivValid = ddx(debug_pixelPos.x);\n";
  extractHlsl += "  PSInitialBuffer[idx].INddx = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddy = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddxfine = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddyfine = (PSInput)0;\n";

  if(!evalSampleCacheData.empty())
  {
    extractHlsl += StringFormat::Fmt("  uint evalIndex = idx * %zu;\n", evalSampleCacheData.size());

    uint32_t evalIdx = 0;
    for(const GlobalState::SampleEvalCacheKey &key : evalSampleCacheData)
    {
      uint32_t keyMask = 0;

      for(int32_t i = 0; i < key.numComponents; i++)
        keyMask |= (1 << (key.firstComponent + i));

      // find the name of the variable matching the operand, in the case of merged input variables.
      rdcstr name, swizzle = "xyzw";
      for(size_t i = 0; i < dxbc->GetReflection()->InputSig.size(); i++)
      {
        if(dxbc->GetReflection()->InputSig[i].regIndex == (uint32_t)key.inputRegisterIndex &&
           dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::Undefined &&
           (dxbc->GetReflection()->InputSig[i].regChannelMask & keyMask) == keyMask)
        {
          name = inputVarNames[i];

          if(!name.empty())
            break;
        }
      }

      swizzle.resize(key.numComponents);

      if(name.empty())
      {
        RDCERR("Couldn't find matching input variable for v%d [%d:%d]", key.inputRegisterIndex,
               key.firstComponent, key.numComponents);
        extractHlsl += StringFormat::Fmt("  PSEvalBuffer[evalIndex+%u] = 0;\n", evalIdx);
        evalIdx++;
        continue;
      }

      name = StringFormat::Fmt("IN.%s.%s", name.c_str(), swizzle.c_str());

      // we must write all components, so just swizzle the values - they'll be ignored later.
      rdcstr expandSwizzle = swizzle;
      while(expandSwizzle.size() < 4)
        expandSwizzle.push_back('x');

      if(key.sample >= 0)
      {
        extractHlsl += StringFormat::Fmt(
            "  PSEvalBuffer[evalIndex+%u] = EvaluateAttributeAtSample(%s, %d).%s;\n", evalIdx,
            name.c_str(), key.sample, expandSwizzle.c_str());
      }
      else
      {
        // we don't need to special-case EvaluateAttributeAtCentroid, since it's just a case with
        // 0,0
        extractHlsl += StringFormat::Fmt(
            "  PSEvalBuffer[evalIndex+%u] = EvaluateAttributeSnapped(%s, int2(%d, %d)).%s;\n",
            evalIdx, name.c_str(), key.offsetx, key.offsety, expandSwizzle.c_str());
      }
      evalIdx++;
    }
  }

  for(size_t i = 0; i < floatInputs.size(); i++)
  {
    const rdcstr &name = floatInputs[i];
    extractHlsl += "  PSInitialBuffer[idx].INddx." + name + " = ddx(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddy." + name + " = ddy(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddxfine." + name + " = ddx_fine(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddyfine." + name + " = ddy_fine(IN." + name + ");\n";
  }
  extractHlsl += "\n}";

  // Create pixel shader to get initial values from previous stage output
  ID3DBlob *psBlob = NULL;
  UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(extractHlsl.c_str(), "ExtractInputsPS", flags, {},
                                                "ps_5_1", &psBlob) != "")
  {
    RDCERR("Failed to create shader to extract inputs");
    return new ShaderDebugTrace;
  }

  uint32_t structStride = sizeof(uint32_t)       // uint hit;
                          + sizeof(float) * 3    // float3 pos;
                          + sizeof(uint32_t)     // uint prim;
                          + sizeof(uint32_t)     // uint fface;
                          + sizeof(uint32_t)     // uint sample;
                          + sizeof(uint32_t)     // uint covge;
                          + sizeof(float)        // float derivValid;
                          +
                          structureStride * 5;    // PSInput IN, INddx, INddy, INddxfine, INddyfine;

  HRESULT hr = S_OK;

  // Create buffer to store initial values captured in pixel shader
  D3D12_RESOURCE_DESC rdesc;
  ZeroMemory(&rdesc, sizeof(D3D12_RESOURCE_DESC));
  rdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  rdesc.Width = structStride * (overdrawLevels + 1);
  rdesc.Height = 1;
  rdesc.DepthOrArraySize = 1;
  rdesc.MipLevels = 1;
  rdesc.Format = DXGI_FORMAT_UNKNOWN;
  rdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  rdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  rdesc.SampleDesc.Count = 1;    // TODO: Support MSAA
  rdesc.SampleDesc.Quality = 0;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  ID3D12Resource *pInitialValuesBuffer = NULL;
  D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &rdesc, resourceState,
                                          NULL, __uuidof(ID3D12Resource),
                                          (void **)&pInitialValuesBuffer);
  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer for pixel shader debugging HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(psBlob);
    return new ShaderDebugTrace;
  }

  // Create buffer to store MSAA evaluations captured in pixel shader
  ID3D12Resource *pMsaaEvalBuffer = NULL;
  if(!evalSampleCacheData.empty())
  {
    rdesc.Width = UINT(evalSampleCacheData.size() * sizeof(Vec4f) * (overdrawLevels + 1));
    hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &rdesc, resourceState,
                                            NULL, __uuidof(ID3D12Resource),
                                            (void **)&pMsaaEvalBuffer);
    if(FAILED(hr))
    {
      RDCERR("Failed to create MSAA buffer for pixel shader debugging HRESULT: %s",
             ToStr(hr).c_str());
      SAFE_RELEASE(pInitialValuesBuffer);
      SAFE_RELEASE(psBlob);
      return new ShaderDebugTrace;
    }
  }

  // Create UAV of initial values buffer
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
  ZeroMemory(&uavDesc, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = overdrawLevels + 1;
  uavDesc.Buffer.StructureByteStride = structStride;

  D3D12_CPU_DESCRIPTOR_HANDLE uav = m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV);
  m_pDevice->CreateUnorderedAccessView(pInitialValuesBuffer, NULL, &uavDesc, uav);

  uavDesc.Format = DXGI_FORMAT_R32_UINT;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = structStride * (overdrawLevels + 1) / sizeof(uint32_t);
  uavDesc.Buffer.StructureByteStride = 0;
  D3D12_CPU_DESCRIPTOR_HANDLE clearUav =
      m_pDevice->GetDebugManager()->GetUAVClearHandle(SHADER_DEBUG_UAV);
  m_pDevice->CreateUnorderedAccessView(pInitialValuesBuffer, NULL, &uavDesc, clearUav);

  // Create UAV of MSAA eval buffer
  D3D12_CPU_DESCRIPTOR_HANDLE msaaClearUav =
      m_pDevice->GetDebugManager()->GetUAVClearHandle(SHADER_DEBUG_MSAA_UAV);
  if(pMsaaEvalBuffer)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE msaaUav =
        m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV);
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    uavDesc.Buffer.NumElements = (overdrawLevels + 1) * (uint32_t)evalSampleCacheData.size();
    m_pDevice->CreateUnorderedAccessView(pMsaaEvalBuffer, NULL, &uavDesc, msaaUav);

    uavDesc.Format = DXGI_FORMAT_R32_UINT;
    uavDesc.Buffer.NumElements =
        (UINT)evalSampleCacheData.size() * (overdrawLevels + 1) / sizeof(uint32_t);
    m_pDevice->CreateUnorderedAccessView(pMsaaEvalBuffer, NULL, &uavDesc, msaaClearUav);
  }

  // Create the descriptor table for our UAV
  D3D12_DESCRIPTOR_RANGE1 descRange;
  descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  descRange.NumDescriptors = pMsaaEvalBuffer ? 2 : 1;
  descRange.BaseShaderRegister = 1;
  descRange.RegisterSpace = regSpace;
  descRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
  descRange.OffsetInDescriptorsFromTableStart = 0;

  modsig.Parameters.push_back(D3D12RootSignatureParameter());
  D3D12RootSignatureParameter &param = modsig.Parameters.back();
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &descRange;

  uint32_t sigElem = uint32_t(modsig.Parameters.size() - 1);

  modsig.Flags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

  // Create the root signature for gathering initial pixel shader values
  ID3DBlob *root = m_pDevice->GetShaderCache()->MakeRootSig(modsig);
  ID3D12RootSignature *pRootSignature = NULL;
  hr = m_pDevice->CreateRootSignature(0, root->GetBufferPointer(), root->GetBufferSize(),
                                      __uuidof(ID3D12RootSignature), (void **)&pRootSignature);
  if(FAILED(hr))
  {
    RDCERR("Failed to create root signature for pixel shader debugging HRESULT: %s",
           ToStr(hr).c_str());
    SAFE_RELEASE(root);
    SAFE_RELEASE(psBlob);
    SAFE_RELEASE(pInitialValuesBuffer);
    SAFE_RELEASE(pMsaaEvalBuffer);
    return new ShaderDebugTrace;
  }
  SAFE_RELEASE(root);

  // All PSO state is the same as the event's, except for the pixel shader and root signature
  pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
  pipeDesc.pRootSignature = pRootSignature;

  ID3D12PipelineState *initialPso = NULL;
  hr = m_pDevice->CreatePipeState(pipeDesc, &initialPso);
  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for pixel shader debugging HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(psBlob);
    SAFE_RELEASE(pInitialValuesBuffer);
    SAFE_RELEASE(pMsaaEvalBuffer);
    SAFE_RELEASE(pRootSignature);
    return new ShaderDebugTrace;
  }

  // if we have a depth buffer bound and we are testing EQUAL grab the current depth value for our
  // target sample
  D3D12_COMPARISON_FUNC depthFunc = pipeDesc.DepthStencilState.DepthFunc;
  float existingDepth = -1.0f;
  ResourceId depthTarget = rs.dsv.GetResResourceId();

  if(depthFunc == D3D12_COMPARISON_FUNC_EQUAL && depthTarget != ResourceId())
  {
    float depthStencilValue[4] = {};
    PickPixel(depthTarget, x, y,
              Subresource(rs.dsv.GetDSV().Texture2DArray.MipSlice,
                          rs.dsv.GetDSV().Texture2DArray.FirstArraySlice, sample),
              CompType::Depth, depthStencilValue);

    existingDepth = depthStencilValue[0];
  }

  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();

  // clear our UAVs
  m_pDevice->GetDebugManager()->SetDescriptorHeaps(cmdList, true, false);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuUav = m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_UAV);
  UINT zero[4] = {0, 0, 0, 0};
  cmdList->ClearUnorderedAccessViewUint(gpuUav, clearUav, pInitialValuesBuffer, zero, 0, NULL);

  if(pMsaaEvalBuffer)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE gpuMsaaUav =
        m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_MSAA_UAV);
    cmdList->ClearUnorderedAccessViewUint(gpuMsaaUav, msaaClearUav, pMsaaEvalBuffer, zero, 0, NULL);
  }

  // Add the descriptor for our UAV
  std::set<ResourceId> copiedHeaps;
  rdcarray<PortableHandle> debugHandles;
  debugHandles.push_back(ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV)));
  if(pMsaaEvalBuffer)
    debugHandles.push_back(ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV)));
  AddDebugDescriptorsToRenderState(m_pDevice, rs, debugHandles,
                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sigElem, copiedHeaps);

  rs.ApplyDescriptorHeaps(cmdList);

  // Execute the command to ensure that UAV clear and resource creation occur before replay
  hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(psBlob);
    SAFE_RELEASE(pInitialValuesBuffer);
    SAFE_RELEASE(pMsaaEvalBuffer);
    SAFE_RELEASE(pRootSignature);
    SAFE_RELEASE(initialPso);
    return new ShaderDebugTrace;
  }

  {
    ID3D12CommandList *l = cmdList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();
  }

  {
    D3D12MarkerRegion initState(m_pDevice->GetQueue()->GetReal(),
                                "Replaying event for initial states");

    // Set the PSO and root signature
    rs.pipe = GetResID(initialPso);
    rs.graphics.rootsig = GetResID(pRootSignature);

    // Replay the event with our modified state
    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    // Restore D3D12 state to what the event uses
    rs = prevState;
  }

  bytebuf initialData;
  m_pDevice->GetDebugManager()->GetBufferData(pInitialValuesBuffer, 0, 0, initialData);

  bytebuf evalData;
  if(pMsaaEvalBuffer)
    m_pDevice->GetDebugManager()->GetBufferData(pMsaaEvalBuffer, 0, 0, evalData);

  // Replaying the event has finished, and the data has been copied out.
  // Free all the resources that were created.
  SAFE_RELEASE(psBlob);
  SAFE_RELEASE(pRootSignature);
  SAFE_RELEASE(pInitialValuesBuffer);
  SAFE_RELEASE(pMsaaEvalBuffer);
  SAFE_RELEASE(initialPso);

  DebugHit *buf = (DebugHit *)initialData.data();

  D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                         StringFormat::Fmt("Got %u hits", buf[0].numHits));
  if(buf[0].numHits == 0)
  {
    RDCLOG("No hit for this event");
    return new ShaderDebugTrace;
  }

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  // figure out the TL pixel's coords. Assume even top left (towards 0,0)
  // this isn't spec'd but is a reasonable assumption.
  int xTL = x & (~1);
  int yTL = y & (~1);

  // get the index of our desired pixel
  int destIdx = (x - xTL) + 2 * (y - yTL);

  // Get depth func and determine "winner" pixel
  DebugHit *pWinnerHit = NULL;
  float *evalSampleCache = (float *)evalData.data();

  if(sample == ~0U)
    sample = 0;

  if(primitive != ~0U)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *pHit = (DebugHit *)(initialData.data() + i * structStride);

      if(pHit->primitive == primitive && pHit->sample == sample)
      {
        pWinnerHit = pHit;
        evalSampleCache = ((float *)evalData.data() + evalSampleCacheData.size() * 4 * i);
      }
    }
  }

  if(pWinnerHit == NULL)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *pHit = (DebugHit *)(initialData.data() + i * structStride);

      if(pWinnerHit == NULL)
      {
        // If we haven't picked a winner at all yet, use the first one
        pWinnerHit = pHit;
        evalSampleCache = ((float *)evalData.data()) + evalSampleCacheData.size() * 4 * i;
      }
      else if(pHit->sample == sample)
      {
        // If this hit is for the sample we want, check whether it's a better pick
        if(pWinnerHit->sample != sample)
        {
          // The previously selected winner was for the wrong sample, use this one
          pWinnerHit = pHit;
          evalSampleCache = ((float *)evalData.data()) + evalSampleCacheData.size() * 4 * i;
        }
        else if(depthFunc == D3D12_COMPARISON_FUNC_EQUAL && existingDepth >= 0.0f)
        {
          // for depth equal, check if this hit is closer than the winner, and if so use it.
          if(fabs(pHit->depth - existingDepth) < fabs(pWinnerHit->depth - existingDepth))
          {
            pWinnerHit = pHit;
            evalSampleCache = ((float *)evalData.data()) + evalSampleCacheData.size() * 4 * i;
          }
        }
        else if(depthFunc == D3D12_COMPARISON_FUNC_ALWAYS ||
                depthFunc == D3D12_COMPARISON_FUNC_NEVER ||
                depthFunc == D3D12_COMPARISON_FUNC_NOT_EQUAL)
        {
          // For depth functions without a sensible comparison, use the last sample encountered
          pWinnerHit = pHit;
          evalSampleCache = ((float *)evalData.data()) + evalSampleCacheData.size() * 4 * i;
        }
        else if((depthFunc == D3D12_COMPARISON_FUNC_LESS && pHit->depth < pWinnerHit->depth) ||
                (depthFunc == D3D12_COMPARISON_FUNC_LESS_EQUAL && pHit->depth <= pWinnerHit->depth) ||
                (depthFunc == D3D12_COMPARISON_FUNC_GREATER && pHit->depth > pWinnerHit->depth) ||
                (depthFunc == D3D12_COMPARISON_FUNC_GREATER_EQUAL && pHit->depth >= pWinnerHit->depth))
        {
          // For depth functions with an inequality, find the hit that "wins" the most
          pWinnerHit = pHit;
          evalSampleCache = ((float *)evalData.data()) + evalSampleCacheData.size() * 4 * i;
        }
      }
    }
  }

  if(pWinnerHit == NULL)
  {
    RDCLOG("Couldn't find any pixels that passed depth test at target coordinates");
    return new ShaderDebugTrace;
  }

  InterpretDebugger *interpreter = new InterpretDebugger;
  interpreter->eventId = eventId;
  ShaderDebugTrace *ret = interpreter->BeginDebug(dxbc, refl, origPSO->PS()->GetMapping(), destIdx);
  GlobalState &global = interpreter->global;
  ThreadState &state = interpreter->activeLane();

  // Fetch constant buffer data from root signature
  GatherConstantBuffers(m_pDevice, *dxbc->GetDXBCByteCode(), rs.graphics, refl,
                        origPSO->PS()->GetMapping(), global, ret->sourceVars);

  global.sampleEvalRegisterMask = sampleEvalRegisterMask;

  {
    DebugHit *pHit = pWinnerHit;

    rdcarray<ShaderVariable> &ins = state.inputs;
    if(!ins.empty() && ins.back().name == "vCoverage")
      ins.back().value.u32v[0] = pHit->coverage;

    state.semantics.coverage = pHit->coverage;
    state.semantics.primID = pHit->primitive;
    state.semantics.isFrontFace = pHit->isFrontFace;

    uint32_t *data = &pHit->rawdata;

    float *pos_ddx = (float *)data;

    // ddx(SV_Position.x) MUST be 1.0
    if(*pos_ddx != 1.0f)
    {
      RDCERR("Derivatives invalid");
      delete interpreter;
      delete ret;
      return new ShaderDebugTrace;
    }

    data++;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      int32_t *rawout = NULL;

      if(initialValues[i].reg >= 0)
      {
        ShaderVariable &invar = ins[initialValues[i].reg];

        if(initialValues[i].sysattribute == ShaderBuiltin::PrimitiveIndex)
        {
          invar.value.u32v[0] = pHit->primitive;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAASampleIndex)
        {
          invar.value.u32v[0] = pHit->sample;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAACoverage)
        {
          invar.value.u32v[0] = pHit->coverage;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::IsFrontFace)
        {
          invar.value.u32v[0] = pHit->isFrontFace ? ~0U : 0;
        }
        else
        {
          rawout = &invar.value.s32v[initialValues[i].elem];

          memcpy(rawout, data, initialValues[i].numwords * 4);
        }
      }

      if(initialValues[i].included)
        data += initialValues[i].numwords;
    }

    for(int i = 0; i < 4; i++)
    {
      if(i != destIdx)
      {
        interpreter->workgroup[i].inputs = state.inputs;
        interpreter->workgroup[i].semantics = state.semantics;
        interpreter->workgroup[i].variables = state.variables;
        interpreter->workgroup[i].SetHelper();
      }
    }

    // Fetch any inputs that were evaluated at sample granularity
    for(const GlobalState::SampleEvalCacheKey &key : evalSampleCacheData)
    {
      // start with the basic input value
      ShaderVariable var = state.inputs[key.inputRegisterIndex];

      // copy over the value into the variable
      memcpy(var.value.f32v.data(), evalSampleCache, var.columns * sizeof(float));

      // store in the global cache for each quad. We'll apply derivatives below to adjust for each
      GlobalState::SampleEvalCacheKey k = key;
      for(int i = 0; i < 4; i++)
      {
        k.quadIndex = i;
        global.sampleEvalCache[k] = var;
      }

      // advance past this data - always by float4 as that's the buffer stride
      evalSampleCache += 4;
    }

    ApplyAllDerivatives(global, interpreter->workgroup, destIdx, initialValues, (float *)data);
  }

  ret->constantBlocks = global.constantBlocks;
  ret->inputs = state.inputs;

  dxbc->FillTraceLineInfo(*ret);

  return ret;
}

ShaderDebugTrace *D3D12Replay::DebugThread(uint32_t eventId,
                                           const rdcfixedarray<uint32_t, 3> &groupid,
                                           const rdcfixedarray<uint32_t, 3> &threadid)
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D12MarkerRegion simloop(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugThread @ %u: [%u, %u, %u] (%u, %u, %u)", eventId, groupid[0],
                        groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]));

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12PipelineState *pso =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  WrappedID3D12Shader *cs =
      pso && pso->IsCompute() ? (WrappedID3D12Shader *)pso->compute->CS.pShaderBytecode : NULL;

  if(!cs)
  {
    RDCERR("Can't debug with no current compute shader");
    return new ShaderDebugTrace;
  }

  DXBC::DXBCContainer *dxbc = cs->GetDXBC();
  const ShaderReflection &refl = cs->GetDetails();

  if(!dxbc)
  {
    RDCERR("Compute shader couldn't be reflected");
    return new ShaderDebugTrace;
  }

  if(!refl.debugInfo.debuggable)
  {
    RDCERR("Compute shader is not debuggable");
    return new ShaderDebugTrace;
  }

  dxbc->GetDisassembly();

  InterpretDebugger *interpreter = new InterpretDebugger;
  interpreter->eventId = eventId;
  ShaderDebugTrace *ret = interpreter->BeginDebug(dxbc, refl, pso->CS()->GetMapping(), 0);
  GlobalState &global = interpreter->global;
  ThreadState &state = interpreter->activeLane();

  GatherConstantBuffers(m_pDevice, *dxbc->GetDXBCByteCode(), rs.compute, refl,
                        pso->CS()->GetMapping(), global, ret->sourceVars);

  for(int i = 0; i < 3; i++)
  {
    state.semantics.GroupID[i] = groupid[i];
    state.semantics.ThreadID[i] = threadid[i];
  }

  ret->constantBlocks = global.constantBlocks;

  dxbc->FillTraceLineInfo(*ret);

  // add fake inputs for semantics
  for(size_t i = 0; i < dxbc->GetDXBCByteCode()->GetNumDeclarations(); i++)
  {
    const DXBCBytecode::Declaration &decl = dxbc->GetDXBCByteCode()->GetDeclaration(i);

    if(decl.declaration == OPCODE_DCL_INPUT &&
       (decl.operand.type == TYPE_INPUT_THREAD_ID || decl.operand.type == TYPE_INPUT_THREAD_GROUP_ID ||
        decl.operand.type == TYPE_INPUT_THREAD_ID_IN_GROUP ||
        decl.operand.type == TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED))
    {
      ShaderVariable v;

      v.name = decl.operand.toString(dxbc->GetReflection(), ToString::IsDecl);
      v.rows = 1;
      v.type = VarType::UInt;

      switch(decl.operand.type)
      {
        case TYPE_INPUT_THREAD_GROUP_ID:
          memcpy(v.value.u32v.data(), state.semantics.GroupID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP:
          memcpy(v.value.u32v.data(), state.semantics.ThreadID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID:
          v.value.u32v[0] =
              state.semantics.GroupID[0] * dxbc->GetReflection()->DispatchThreadsDimension[0] +
              state.semantics.ThreadID[0];
          v.value.u32v[1] =
              state.semantics.GroupID[1] * dxbc->GetReflection()->DispatchThreadsDimension[1] +
              state.semantics.ThreadID[1];
          v.value.u32v[2] =
              state.semantics.GroupID[2] * dxbc->GetReflection()->DispatchThreadsDimension[2] +
              state.semantics.ThreadID[2];
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          v.value.u32v[0] =
              state.semantics.ThreadID[2] * dxbc->GetReflection()->DispatchThreadsDimension[0] *
                  dxbc->GetReflection()->DispatchThreadsDimension[1] +
              state.semantics.ThreadID[1] * dxbc->GetReflection()->DispatchThreadsDimension[0] +
              state.semantics.ThreadID[0];
          v.columns = 1;
          break;
        default: v.columns = 4; break;
      }

      ret->inputs.push_back(v);
    }
  }

  return ret;
}

rdcarray<ShaderDebugState> D3D12Replay::ContinueDebug(ShaderDebugger *debugger)
{
  DXBCDebug::InterpretDebugger *interpreter = (DXBCDebug::InterpretDebugger *)debugger;

  if(!interpreter)
    return {};

  D3D12DebugAPIWrapper apiWrapper(m_pDevice, interpreter->dxbc, interpreter->global,
                                  interpreter->eventId);

  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "ContinueDebug Simulation Loop");

  return interpreter->ContinueDebug(&apiWrapper);
}

void D3D12Replay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
}
