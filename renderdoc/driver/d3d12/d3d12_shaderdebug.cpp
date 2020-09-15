/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

  return false;
}

class D3D12DebugAPIWrapper : public DXBCDebug::DebugAPIWrapper
{
public:
  D3D12DebugAPIWrapper(WrappedID3D12Device *device, const DXBC::DXBCContainer *dxbc,
                       DXBCDebug::GlobalState &globalState);

  void SetCurrentInstruction(uint32_t instruction) { m_instruction = instruction; }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d);

  bool FetchSRV(const DXBCDebug::BindingSlot &slot);
  bool FetchUAV(const DXBCDebug::BindingSlot &slot);

  bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                              ShaderVariable &output1, ShaderVariable &output2);

  ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource,
                               const DXBCDebug::BindingSlot &slot, const char *opString);
  ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot,
                               const char *opString);
  ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot,
                                 uint32_t mipLevel, int &dim);

  bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                             DXBCDebug::SampleGatherResourceData resourceData,
                             DXBCDebug::SampleGatherSamplerData samplerData, ShaderVariable uv,
                             ShaderVariable ddxCalc, ShaderVariable ddyCalc,
                             const int texelOffsets[3], int multisampleIndex, float lodOrCompareValue,
                             const uint8_t swizzle[4], DXBCDebug::GatherChannel gatherChannel,
                             const char *opString, ShaderVariable &output);

private:
  DXBC::ShaderType GetShaderType() { return m_dxbc ? m_dxbc->m_Type : DXBC::ShaderType::Pixel; }
  WrappedID3D12Device *m_pDevice;
  const DXBC::DXBCContainer *m_dxbc;
  DXBCDebug::GlobalState &m_globalState;
  uint32_t m_instruction;
};

D3D12DebugAPIWrapper::D3D12DebugAPIWrapper(WrappedID3D12Device *device,
                                           const DXBC::DXBCContainer *dxbc,
                                           DXBCDebug::GlobalState &globalState)
    : m_pDevice(device), m_dxbc(dxbc), m_globalState(globalState), m_instruction(0)
{
}

void D3D12DebugAPIWrapper::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                           rdcstr d)
{
  m_pDevice->AddDebugMessage(c, sv, src, d);
}

bool D3D12DebugAPIWrapper::FetchSRV(const DXBCDebug::BindingSlot &slot)
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
            D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

            DXBCDebug::GlobalState::SRVData &srvData = m_globalState.srvs[slot];

            // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
            // resource desc format or the DXBC reflection info might be more correct.
            DXBCDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, srvData.format);
            srvData.firstElement = (uint32_t)(element.offset / sizeof(uint32_t));
            srvData.numElements = (uint32_t)((resDesc.Width - element.offset) / sizeof(uint32_t));

            if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
              m_pDevice->GetDebugManager()->GetBufferData(pResource, 0, 0, srvData.data);

            return true;
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
                DXBCDebug::GlobalState::SRVData &srvData = m_globalState.srvs[slot];
                ResourceId srvId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);

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

                return true;
              }
            }
          }
        }
      }
    }
  }

  return false;
}
bool D3D12DebugAPIWrapper::FetchUAV(const DXBCDebug::BindingSlot &slot)
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
            D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

            DXBCDebug::GlobalState::UAVData &uavData = m_globalState.uavs[slot];

            // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
            // resource desc format or the DXBC reflection info might be more correct.
            DXBCDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, uavData.format);
            uavData.firstElement = (uint32_t)(element.offset / sizeof(uint32_t));
            uavData.numElements = (uint32_t)((resDesc.Width - element.offset) / sizeof(uint32_t));

            if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
              m_pDevice->GetDebugManager()->GetBufferData(pResource, 0, 0, uavData.data);

            return true;
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
                DXBCDebug::GlobalState::UAVData &uavData = m_globalState.uavs[slot];
                ResourceId uavId = desc->GetResResourceId();
                ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);

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

                return true;
              }
            }
          }
        }
      }
    }
  }

  return false;
}

bool D3D12DebugAPIWrapper::CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode,
                                                  const ShaderVariable &input,
                                                  ShaderVariable &output1, ShaderVariable &output2)
{
  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "CalculateMathIntrinsic");

  if(opcode != DXBCBytecode::OPCODE_RCP && opcode != DXBCBytecode::OPCODE_RSQ &&
     opcode != DXBCBytecode::OPCODE_EXP && opcode != DXBCBytecode::OPCODE_LOG &&
     opcode != DXBCBytecode::OPCODE_SINCOS)
  {
    // To support a new instruction, the shader created in
    // D3D12DebugManager::CreateMathIntrinsicsResources will need updated
    RDCERR("Unsupported instruction for CalculateMathIntrinsic: %u", opcode);
    return false;
  }

  // Create UAV to store the computed results
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
  ZeroMemory(&uavDesc, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = 2;
  uavDesc.Buffer.StructureByteStride = sizeof(Vec4f);

  ID3D12Resource *pResultBuffer = m_pDevice->GetDebugManager()->GetMathIntrinsicsResultBuffer();
  D3D12_CPU_DESCRIPTOR_HANDLE uav = m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV);
  m_pDevice->CreateUnorderedAccessView(pResultBuffer, NULL, &uavDesc, uav);

  // Set root signature & sig params on command list, then execute the shader
  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  m_pDevice->GetDebugManager()->SetDescriptorHeaps(cmdList, true, false);
  cmdList->SetPipelineState(m_pDevice->GetDebugManager()->GetMathIntrinsicsPso());
  cmdList->SetComputeRootSignature(m_pDevice->GetDebugManager()->GetMathIntrinsicsRootSig());
  cmdList->SetComputeRoot32BitConstants(0, 4, &input.value.uv[0], 0);
  cmdList->SetComputeRoot32BitConstants(1, 1, &opcode, 0);
  cmdList->SetComputeRootUnorderedAccessView(2, pResultBuffer->GetGPUVirtualAddress());
  cmdList->Dispatch(1, 1, 1);

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

  bytebuf results;
  m_pDevice->GetDebugManager()->GetBufferData(pResultBuffer, 0, 0, results);
  RDCASSERT(results.size() >= sizeof(Vec4f) * 2);

  memcpy(output1.value.uv, results.data(), sizeof(Vec4f));
  memcpy(output2.value.uv, results.data() + sizeof(Vec4f), sizeof(Vec4f));

  return true;
}

ShaderVariable D3D12DebugAPIWrapper::GetSampleInfo(DXBCBytecode::OperandType type,
                                                   bool isAbsoluteResource,
                                                   const DXBCDebug::BindingSlot &slot,
                                                   const char *opString)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(type == DXBCBytecode::TYPE_RASTERIZER)
  {
    if(GetShaderType() != DXBC::ShaderType::Compute)
    {
      // try depth first - both should match sample count though to be valid
      ResourceId res = rs.GetDSVID();
      if(res == ResourceId() && !rs.rts.empty())
        res = rs.rts[0].GetResResourceId();

      ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(res);
      D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
      result.value.u.x = resDesc.SampleDesc.Count;
      result.value.u.y = 0;
      result.value.u.z = 0;
      result.value.u.w = 0;
    }
    return result;
  }

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

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(GetShaderType(), param.ShaderVisibility))
      {
        // Root SRV/UAV can only be buffers, so we don't need to check them for GetSampleInfo
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
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

            // Check if the slot we want is contained
            if(slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              if(desc)
              {
                if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV &&
                   type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
                {
                  ResourceId srvId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
                  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc->GetSRV();
                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
                    srvDesc = MakeSRVDesc(resDesc);

                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS ||
                     srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
                  {
                    result.value.u.x = resDesc.SampleDesc.Count;
                    result.value.u.y = 0;
                    result.value.u.z = 0;
                    result.value.u.w = 0;
                  }
                  else
                  {
                    RDCERR("Invalid resource dimension for GetSampleInfo");
                  }
                  return result;
                }
              }
            }
          }
        }
      }
    }
  }

  return result;
}

ShaderVariable D3D12DebugAPIWrapper::GetBufferInfo(DXBCBytecode::OperandType type,
                                                   const DXBCDebug::BindingSlot &slot,
                                                   const char *opString)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

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

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(GetShaderType(), param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && element.type == eRootSRV &&
           type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            // Found the requested SRV
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);
            D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

            // Root descriptors are always buffers with each element 32-bit
            uint32_t numElements = (uint32_t)((resDesc.Width - element.offset) / sizeof(uint32_t));
            result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w = numElements;
            return result;
          }
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && element.type == eRootUAV &&
                type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
        {
          if(param.Descriptor.ShaderRegister == slot.shaderRegister &&
             param.Descriptor.RegisterSpace == slot.registerSpace)
          {
            // Found the requested UAV
            ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);
            D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

            // Root descriptors are always buffers with each element 32-bit
            uint32_t numElements = (uint32_t)((resDesc.Width - element.offset) / sizeof(uint32_t));
            result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w = numElements;
            return result;
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

            // Check if the slot we want is contained
            if(slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              if(desc)
              {
                if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV &&
                   type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
                {
                  ResourceId uavId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);
                  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = desc->GetUAV();

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
                    uavDesc = MakeUAVDesc(resDesc);

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
                  {
                    result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w =
                        (uint32_t)uavDesc.Buffer.NumElements;
                  }
                  return result;
                }
                else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV &&
                        type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
                {
                  ResourceId srvId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
                  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc->GetSRV();
                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
                    srvDesc = MakeSRVDesc(resDesc);

                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
                  {
                    result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w =
                        (uint32_t)srvDesc.Buffer.NumElements;
                  }
                  return result;
                }
              }
            }
          }
        }
      }
    }
  }

  return result;
}

ShaderVariable D3D12DebugAPIWrapper::GetResourceInfo(DXBCBytecode::OperandType type,
                                                     const DXBCDebug::BindingSlot &slot,
                                                     uint32_t mipLevel, int &dim)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

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

    size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), pRootSignature->sigelems.size());
    for(size_t i = 0; i < numParams; ++i)
    {
      const D3D12RootSignatureParameter &param = pD3D12RootSig->sig.Parameters[i];
      const D3D12RenderState::SignatureElement &element = pRootSignature->sigelems[i];
      if(IsShaderParameterVisible(GetShaderType(), param.ShaderVisibility))
      {
        // Root SRV/UAV can only be buffers, so we don't need to check them for GetResourceInfo
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE &&
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

            // Check if the slot we want is contained
            if(slot.shaderRegister >= range.BaseShaderRegister &&
               slot.shaderRegister < range.BaseShaderRegister + numDescriptors &&
               range.RegisterSpace == slot.registerSpace)
            {
              desc += slot.shaderRegister - range.BaseShaderRegister;
              if(desc)
              {
                if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV &&
                   type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
                {
                  ResourceId uavId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);
                  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = desc->GetUAV();

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

                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = isarray ? uavDesc.Texture1DArray.ArraySize : 0;
                      result.value.u.z = 0;

                      // spec says "For UAVs (u#), the number of mip levels is always 1."
                      result.value.u.w = 1;

                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = 0;

                      break;
                    }
                    case D3D12_UAV_DIMENSION_TEXTURE2D:
                    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
                    {
                      dim = 2;

                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

                      if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D)
                        result.value.u.z = 0;
                      else if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
                        result.value.u.z = uavDesc.Texture2DArray.ArraySize;

                      // spec says "For UAVs (u#), the number of mip levels is always 1."
                      result.value.u.w = 1;

                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = result.value.u.z = 0;

                      break;
                    }
                    case D3D12_UAV_DIMENSION_TEXTURE3D:
                    {
                      dim = 3;

                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));
                      result.value.u.z = RDCMAX(1U, (uint32_t)(resDesc.DepthOrArraySize >> mipLevel));

                      // spec says "For UAVs (u#), the number of mip levels is always 1."
                      result.value.u.w = 1;

                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = result.value.u.z = 0;

                      break;
                    }
                  }

                  return result;
                }
                else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV &&
                        type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
                {
                  ResourceId srvId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
                  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc->GetSRV();
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

                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = isarray ? srvDesc.Texture1DArray.ArraySize : 0;
                      result.value.u.z = 0;
                      result.value.u.w =
                          isarray ? srvDesc.Texture1DArray.MipLevels : srvDesc.Texture1D.MipLevels;

                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = 0;

                      break;
                    }
                    case D3D12_SRV_DIMENSION_TEXTURE2D:
                    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
                    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
                    {
                      dim = 2;
                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

                      if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
                      {
                        result.value.u.z = 0;
                        result.value.u.w = srvDesc.Texture2D.MipLevels;
                      }
                      else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
                      {
                        result.value.u.z = srvDesc.Texture2DArray.ArraySize;
                        result.value.u.w = srvDesc.Texture2DArray.MipLevels;
                      }
                      else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS)
                      {
                        result.value.u.z = 0;
                        result.value.u.w = 1;
                      }
                      else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
                      {
                        result.value.u.z = srvDesc.Texture2DMSArray.ArraySize;
                        result.value.u.w = 1;
                      }
                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = result.value.u.z = 0;

                      break;
                    }
                    case D3D12_SRV_DIMENSION_TEXTURE3D:
                    {
                      dim = 3;

                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));
                      result.value.u.z = RDCMAX(1U, (uint32_t)(resDesc.DepthOrArraySize >> mipLevel));
                      result.value.u.w = srvDesc.Texture3D.MipLevels;

                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = result.value.u.z = 0;

                      break;
                    }
                    case D3D12_SRV_DIMENSION_TEXTURECUBE:
                    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
                    {
                      // Even though it's a texture cube, an individual face's dimensions are
                      // returned
                      dim = 2;

                      bool isarray = srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;

                      result.value.u.x = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
                      result.value.u.y = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

                      // the spec says "If srcResource is a TextureCubeArray, [...]. dest.z is set
                      // to an undefined value."
                      // but that's stupid, and implementations seem to return the number of cubes
                      result.value.u.z = isarray ? srvDesc.TextureCubeArray.NumCubes : 0;
                      result.value.u.w = isarray ? srvDesc.TextureCubeArray.MipLevels
                                                 : srvDesc.TextureCube.MipLevels;

                      if(mipLevel >= result.value.u.w)
                        result.value.u.x = result.value.u.y = result.value.u.z = 0;

                      break;
                    }
                    case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
                    {
                      RDCERR("Raytracing is unsupported");
                      break;
                    }
                  }
                  return result;
                }
              }
            }
          }
        }
      }
    }
  }

  return result;
}

bool D3D12DebugAPIWrapper::CalculateSampleGather(
    DXBCBytecode::OpcodeType opcode, DXBCDebug::SampleGatherResourceData resourceData,
    DXBCDebug::SampleGatherSamplerData samplerData, ShaderVariable uv, ShaderVariable ddxCalc,
    ShaderVariable ddyCalc, const int texelOffsets[3], int multisampleIndex,
    float lodOrCompareValue, const uint8_t swizzle[4], DXBCDebug::GatherChannel gatherChannel,
    const char *opString, ShaderVariable &output)
{
  using namespace DXBCBytecode;

  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "CalculateSampleGather");

  rdcstr funcRet = "";
  DXGI_FORMAT retFmt = DXGI_FORMAT_UNKNOWN;

  if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ || opcode == OPCODE_GATHER4_C ||
     opcode == OPCODE_GATHER4_PO_C || opcode == OPCODE_LOD)
  {
    retFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
    funcRet = "float4";
  }

  rdcstr samplerDecl = "";
  if(samplerData.mode == SAMPLER_MODE_DEFAULT)
    samplerDecl = "SamplerState s";
  else if(samplerData.mode == SAMPLER_MODE_COMPARISON)
    samplerDecl = "SamplerComparisonState s";

  rdcstr textureDecl = "";
  int texdim = 2;
  int offsetDim = 2;
  bool useOffsets = true;

  if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE1D)
  {
    textureDecl = "Texture1D";
    texdim = 1;
    offsetDim = 1;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2D)
  {
    textureDecl = "Texture2D";
    texdim = 2;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS)
  {
    textureDecl = "Texture2DMS";
    texdim = 2;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE3D)
  {
    textureDecl = "Texture3D";
    texdim = 3;
    offsetDim = 3;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBE)
  {
    textureDecl = "TextureCube";
    texdim = 3;
    offsetDim = 3;
    useOffsets = false;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE1DARRAY)
  {
    textureDecl = "Texture1DArray";
    texdim = 2;
    offsetDim = 1;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY)
  {
    textureDecl = "Texture2DArray";
    texdim = 3;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
  {
    textureDecl = "Texture2DMSArray";
    texdim = 3;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBEARRAY)
  {
    textureDecl = "TextureCubeArray";
    texdim = 4;
    offsetDim = 3;
    useOffsets = false;
  }
  else
  {
    RDCERR("Unsupported resource type %d in sample operation", resourceData.dim);
  }

  {
    char *typeStr[DXBC::NUM_RETURN_TYPES] = {
        "",    // enum starts at ==1
        "unorm float",
        "snorm float",
        "int",
        "uint",
        "float",
        "__",    // RETURN_TYPE_MIXED
        "double",
        "__",    // RETURN_TYPE_CONTINUED
        "__",    // RETURN_TYPE_UNUSED
    };

    // obviously these may be overly optimistic in some cases
    // but since we don't know at debug time what the source texture format is
    // we just use the fattest one necessary. There's no harm in retrieving at
    // higher precision
    DXGI_FORMAT fmts[DXBC::NUM_RETURN_TYPES] = {
        DXGI_FORMAT_UNKNOWN,               // enum starts at ==1
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // unorm float
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // snorm float
        DXGI_FORMAT_R32G32B32A32_SINT,     // int
        DXGI_FORMAT_R32G32B32A32_UINT,     // uint
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // float
        DXGI_FORMAT_UNKNOWN,               // RETURN_TYPE_MIXED

        // should maybe be double, but there is no double texture format anyway!
        // spec is unclear but I presume reads are done at most at float
        // precision anyway since that's the source, and converted to doubles.
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // double

        DXGI_FORMAT_UNKNOWN,    // RETURN_TYPE_CONTINUED
        DXGI_FORMAT_UNKNOWN,    // RETURN_TYPE_UNUSED
    };

    rdcstr type = StringFormat::Fmt("%s4", typeStr[resourceData.retType]);

    if(retFmt == DXGI_FORMAT_UNKNOWN)
    {
      funcRet = type;
      retFmt = fmts[resourceData.retType];
    }

    if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
       resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
    {
      if(resourceData.sampleCount > 0)
        type += StringFormat::Fmt(", %d", resourceData.sampleCount);
    }

    textureDecl += "<" + type + "> t";
  }

  char *formats[4][2] = {
      {"float(%.10f)", "int(%d)"},
      {"float2(%.10f, %.10f)", "int2(%d, %d)"},
      {"float3(%.10f, %.10f, %.10f)", "int3(%d, %d, %d)"},
      {"float4(%.10f, %.10f, %.10f, %.10f)", "int4(%d, %d, %d, %d)"},
  };

  int texcoordType = 0;
  int texdimOffs = 0;

  if(opcode == OPCODE_SAMPLE || opcode == OPCODE_SAMPLE_L || opcode == OPCODE_SAMPLE_B ||
     opcode == OPCODE_SAMPLE_D || opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ ||
     opcode == OPCODE_GATHER4 || opcode == OPCODE_GATHER4_C || opcode == OPCODE_GATHER4_PO ||
     opcode == OPCODE_GATHER4_PO_C || opcode == OPCODE_LOD)
  {
    // all floats
    texcoordType = 0;
  }
  else if(opcode == OPCODE_LD)
  {
    // int address, one larger than texdim (to account for mip/slice parameter)
    texdimOffs = 1;
    texcoordType = 1;

    if(texdim == 4)
    {
      RDCERR("Unexpectedly large texture in load operation");
    }
  }
  else if(opcode == OPCODE_LD_MS)
  {
    texcoordType = 1;

    if(texdim == 4)
    {
      RDCERR("Unexpectedly large texture in load operation");
    }
  }

  for(uint32_t i = 0; i < ddxCalc.columns; i++)
  {
    if(!RDCISFINITE(ddxCalc.value.fv[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddxCalc.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddx - using 0.0 instead",
                                                   m_instruction, opString));
    }
    if(!RDCISFINITE(ddyCalc.value.fv[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddyCalc.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddy - using 0.0 instead",
                                                   m_instruction, opString));
    }
  }

  for(uint32_t i = 0; i < uv.columns; i++)
  {
    if(texcoordType == 0 && (!RDCISFINITE(uv.value.fv[i])))
    {
      RDCWARN("NaN or Inf in texlookup");
      uv.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup uv - using 0.0 instead",
                                                   m_instruction, opString));
    }
  }

  rdcstr texcoords;

  // because of unions in .value we can pass the float versions and printf will interpret it as
  // the right type according to formats
  if(texcoordType == 0)
    texcoords = StringFormat::Fmt(formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x,
                                  uv.value.f.y, uv.value.f.z, uv.value.f.w);
  else
    texcoords = StringFormat::Fmt(formats[texdim + texdimOffs - 1][texcoordType], uv.value.i.x,
                                  uv.value.i.y, uv.value.i.z, uv.value.i.w);

  rdcstr offsets = "";

  if(useOffsets)
  {
    if(offsetDim == 1)
      offsets = StringFormat::Fmt(", int(%d)", texelOffsets[0]);
    else if(offsetDim == 2)
      offsets = StringFormat::Fmt(", int2(%d, %d)", texelOffsets[0], texelOffsets[1]);
    else if(offsetDim == 3)
      offsets =
          StringFormat::Fmt(", int3(%d, %d, %d)", texelOffsets[0], texelOffsets[1], texelOffsets[2]);
    // texdim == 4 is cube arrays, no offset supported
  }

  char elems[] = "xyzw";
  rdcstr strSwizzle = ".";
  for(int i = 0; i < 4; ++i)
    strSwizzle += elems[swizzle[i]];

  rdcstr strGatherChannel;
  switch(gatherChannel)
  {
    case DXBCDebug::GatherChannel::Red: strGatherChannel = "Red"; break;
    case DXBCDebug::GatherChannel::Green: strGatherChannel = "Green"; break;
    case DXBCDebug::GatherChannel::Blue: strGatherChannel = "Blue"; break;
    case DXBCDebug::GatherChannel::Alpha: strGatherChannel = "Alpha"; break;
  }

  rdcstr uvSnippet = "float4 doUV(uint id) { return 0.0f.xxxx; }\n";
  rdcstr colSnippet = funcRet + " doCol() { return 0.0f.xxxx; }\n";
  rdcstr sampleSnippet;

  rdcstr strResourceBinding = StringFormat::Fmt("t%u, space%u", resourceData.binding.shaderRegister,
                                                resourceData.binding.registerSpace);
  rdcstr strSamplerBinding = StringFormat::Fmt("s%u, space%u", samplerData.binding.shaderRegister,
                                               samplerData.binding.registerSpace);

  if(opcode == OPCODE_SAMPLE || opcode == OPCODE_SAMPLE_B || opcode == OPCODE_SAMPLE_D)
  {
    rdcstr ddx = StringFormat::Fmt(formats[offsetDim + texdimOffs - 1][0], ddxCalc.value.f.x,
                                   ddxCalc.value.f.y, ddxCalc.value.f.z, ddxCalc.value.f.w);

    rdcstr ddy = StringFormat::Fmt(formats[offsetDim + texdimOffs - 1][0], ddyCalc.value.f.x,
                                   ddyCalc.value.f.y, ddyCalc.value.f.z, ddyCalc.value.f.w);

    sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                      textureDecl.c_str(), strResourceBinding.c_str(),
                                      samplerDecl.c_str(), strSamplerBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\nreturn ";
    sampleSnippet += StringFormat::Fmt("t.SampleGrad(s, %s, %s, %s %s)%s;\n", texcoords.c_str(),
                                       ddx.c_str(), ddy.c_str(), offsets.c_str(), strSwizzle.c_str());
    sampleSnippet += "}\n";
  }
  else if(opcode == OPCODE_SAMPLE_L)
  {
    // lod selection
    sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                      textureDecl.c_str(), strResourceBinding.c_str(),
                                      samplerDecl.c_str(), strSamplerBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\nreturn ";
    sampleSnippet += StringFormat::Fmt("t.SampleLevel(s, %s, %.10f %s)%s;\n", texcoords.c_str(),
                                       lodOrCompareValue, offsets.c_str(), strSwizzle.c_str());
    sampleSnippet += "}\n";
  }
  else if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_LOD)
  {
    // these operations need derivatives but have no hlsl function to call to provide them, so
    // we fake it in the vertex shader

    rdcstr uvswizzle = "xyzw";
    uvswizzle.resize(texdim);

    rdcstr uvPlusDDX = StringFormat::Fmt(
        formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x + ddyCalc.value.f.x * 2.0f,
        uv.value.f.y + ddyCalc.value.f.y * 2.0f, uv.value.f.z + ddyCalc.value.f.z * 2.0f,
        uv.value.f.w + ddyCalc.value.f.w * 2.0f);

    rdcstr uvPlusDDY = StringFormat::Fmt(
        formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x + ddxCalc.value.f.x * 2.0f,
        uv.value.f.y + ddxCalc.value.f.y * 2.0f, uv.value.f.z + ddxCalc.value.f.z * 2.0f,
        uv.value.f.w + ddxCalc.value.f.w * 2.0f);

    uvSnippet = "float4 uv(uint id) {\n";
    uvSnippet += "if(id == 0) return " + uvPlusDDX + ";\n";
    uvSnippet += "if(id == 1) return " + texcoords + ";\n";
    uvSnippet += "            return " + uvPlusDDY + ";\n";
    uvSnippet += "}\n";

    if(opcode == OPCODE_SAMPLE_C)
    {
      // comparison value
      sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                        textureDecl.c_str(), strResourceBinding.c_str(),
                                        samplerDecl.c_str(), strSamplerBinding.c_str());
      sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
      sampleSnippet += StringFormat::Fmt("t.SampleCmpLevelZero(s, uv.%s, %.10f %s).xxxx;\n",
                                         uvswizzle.c_str(), lodOrCompareValue, offsets.c_str());
      sampleSnippet += "}\n";
    }
    else if(opcode == OPCODE_LOD)
    {
      sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                        textureDecl.c_str(), strResourceBinding.c_str(),
                                        samplerDecl.c_str(), strSamplerBinding.c_str());
      sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
      sampleSnippet += StringFormat::Fmt(
          "return float4(t.CalculateLevelOfDetail(s, uv.%s),\n"
          "              t.CalculateLevelOfDetailUnclamped(s, uv.%s),\n"
          "              0.0f, 0.0f);\n",
          uvswizzle.c_str(), uvswizzle.c_str());
      sampleSnippet += "}\n";
    }
  }
  else if(opcode == OPCODE_SAMPLE_C_LZ)
  {
    // comparison value
    sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                      textureDecl.c_str(), strResourceBinding.c_str(),
                                      samplerDecl.c_str(), strSamplerBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
    sampleSnippet +=
        StringFormat::Fmt("return t.SampleCmpLevelZero(s, %s, %.10f %s)%s;\n", texcoords.c_str(),
                          lodOrCompareValue, offsets.c_str(), strSwizzle.c_str());
    sampleSnippet += "}\n";
  }
  else if(opcode == OPCODE_LD)
  {
    sampleSnippet =
        StringFormat::Fmt("%s : register(%s);\n\n", textureDecl.c_str(), strResourceBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
    sampleSnippet += "return t.Load(" + texcoords + offsets + ")" + strSwizzle + ";";
    sampleSnippet += "\n}\n";
  }
  else if(opcode == OPCODE_LD_MS)
  {
    sampleSnippet =
        StringFormat::Fmt("%s : register(%s);\n\n", textureDecl.c_str(), strResourceBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
    sampleSnippet += StringFormat::Fmt("return t.Load(%s, int(%d) %s)%s;\n", texcoords.c_str(),
                                       multisampleIndex, offsets.c_str(), strSwizzle.c_str());
    sampleSnippet += "\n}\n";
  }
  else if(opcode == OPCODE_GATHER4 || opcode == OPCODE_GATHER4_PO)
  {
    sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                      textureDecl.c_str(), strResourceBinding.c_str(),
                                      samplerDecl.c_str(), strSamplerBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
    sampleSnippet += StringFormat::Fmt("return t.Gather%s(s, %s %s)%s;\n", strGatherChannel.c_str(),
                                       texcoords.c_str(), offsets.c_str(), strSwizzle.c_str());
    sampleSnippet += "}\n";
  }
  else if(opcode == OPCODE_GATHER4_C || opcode == OPCODE_GATHER4_PO_C)
  {
    // comparison value
    sampleSnippet = StringFormat::Fmt("%s : register(%s);\n%s : register(%s);\n\n",
                                      textureDecl.c_str(), strResourceBinding.c_str(),
                                      samplerDecl.c_str(), strSamplerBinding.c_str());
    sampleSnippet += funcRet + " doSample(float4 uv)\n{\n";
    sampleSnippet += StringFormat::Fmt("return t.GatherCmp%s(s, %s, %.10f %s)%s;\n",
                                       strGatherChannel.c_str(), texcoords.c_str(),
                                       lodOrCompareValue, offsets.c_str(), strSwizzle.c_str());
    sampleSnippet += "}\n";
  }

  rdcstr evalSnippet;

  // if the sample happens in the vertex shader we need to do that too, otherwise root signature
  // visibility may not match
  if(GetShaderType() == DXBC::ShaderType::Vertex)
  {
    // include the sampleSnippet in the vertex shader and return it into the col
    colSnippet = sampleSnippet;
    // we can pass 0.0f to doSample() because the only doSample()s needing UVs are in the pixel
    // shader
    colSnippet += funcRet + " doCol() { return doSample(0.0f.xxxx); }\n";

    // return the passed through col
    evalSnippet = funcRet + " evalResult(" + funcRet + " col, float4 uv) { return col; }\n";
  }
  else
  {
    if(GetShaderType() != DXBC::ShaderType::Pixel && GetShaderType() != DXBC::ShaderType::Compute)
    {
      // other stages can't re-use the pixel shader visibility in the root signature, and it's not
      // feasible to do the sampling in a fake geometry/tessellation shader. Instead if we intend to
      // support other stages we need to stop re-using the root signature and instead patch it to be
      // set up how we want for pixel shader sampling.
      RDCERR("shader stages other than pixel/compute need special handling.");
    }

    // include the sample snippet and forward to doSample
    evalSnippet = sampleSnippet;
    evalSnippet +=
        funcRet + " evalResult(" + funcRet + " col, float4 uv) { return doSample(uv); }\n";
  }

  rdcstr vsProgram;

  vsProgram += uvSnippet;
  vsProgram += colSnippet;
  vsProgram += "void main(uint id : SV_VertexID, out float4 pos : SV_Position, out " + funcRet +
               " col : COL, out float4 uv : UV) {\n";
  vsProgram += "  pos = float4((id == 2) ? 3.0f : -1.0f, (id == 0) ? -3.0f : 1.0f, 0.5, 1.0);\n";
  vsProgram += "  uv = doUV(id);\n";
  vsProgram += "  col = doCol();\n";
  vsProgram += "}";

  rdcstr psProgram;

  psProgram += evalSnippet;
  psProgram += funcRet + " main(float4 pos : SV_Position, " + funcRet +
               " col : COL, float4 uv : UV) : SV_Target0 {\n";
  psProgram += "  return evalResult(col, uv);\n";
  psProgram += "}";

  // Create VS/PS to fetch the sample. Because the program being debugged might be using SM 5.1, we
  // need to do that too, to support reusing the existing root signature that may use a non-zero
  // register space for the resource or sampler.
  ID3DBlob *vsBlob = NULL;
  ID3DBlob *psBlob = NULL;
  UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(vsProgram.c_str(), "main", flags, "vs_5_1",
                                                &vsBlob) != "")
  {
    RDCERR("Failed to create shader to extract inputs");
    return false;
  }
  if(m_pDevice->GetShaderCache()->GetShaderBlob(psProgram.c_str(), "main", flags, "ps_5_1",
                                                &psBlob) != "")
  {
    RDCERR("Failed to create shader to extract inputs");
    SAFE_RELEASE(vsBlob);
    return false;
  }

  // Create a PSO with our VS/PS and all other state from the original event
  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12RenderState prevState = rs;

  // If we're debugging a compute shader, we should be able to reuse the rootsig for a
  // pixel shader, since the entries will have to use shader visibility all
  bool isCompute = m_dxbc->m_Type == DXBC::ShaderType::Compute;
  ResourceId sigId = isCompute ? rs.compute.rootsig : rs.graphics.rootsig;
  WrappedID3D12RootSignature *pRootSig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(sigId);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeDesc;
  ZeroMemory(&pipeDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

  pipeDesc.pRootSignature = pRootSig;

  pipeDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
  pipeDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
  pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();

  pipeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeDesc.RasterizerState.FrontCounterClockwise = TRUE;
  pipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  pipeDesc.SampleMask = UINT_MAX;
  pipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeDesc.NumRenderTargets = 1;
  pipeDesc.RTVFormats[0] = retFmt;
  pipeDesc.SampleDesc.Count = 1;

  ID3D12PipelineState *samplePso = NULL;
  HRESULT hr = m_pDevice->CreateGraphicsPipelineState(&pipeDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&samplePso);
  SAFE_RELEASE(vsBlob);
  SAFE_RELEASE(psBlob);
  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  rs.pipe = GetResID(samplePso);
  rs.rts.clear();
  // Set viewport/scissor unconditionally - we need to set this all the time for sampling for a
  // compute shader, but also a graphics draw might exclude pixel (0, 0) from its view or scissor
  rs.views.clear();
  rs.views.push_back({0, 0, 1, 1, 0, 1});
  rs.scissors.clear();
  rs.scissors.push_back({0, 0, 1, 1});
  if(isCompute)
  {
    // When debugging compute, we need to move the root sig and elems to the graphics portion
    rs.graphics.rootsig = sigId;
    rs.graphics.sigelems = rs.compute.sigelems;
    rs.compute.rootsig = ResourceId();
    rs.compute.sigelems.clear();
  }
  rs.topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  rs.ApplyState(m_pDevice, cmdList);

  // Create a 1x1 texture to store the sample result
  D3D12_RESOURCE_DESC rdesc;
  ZeroMemory(&rdesc, sizeof(D3D12_RESOURCE_DESC));
  rdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  rdesc.Width = 1;
  rdesc.Height = 1;
  rdesc.DepthOrArraySize = 1;
  rdesc.MipLevels = 0;
  rdesc.Format = retFmt;
  rdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  rdesc.SampleDesc.Count = 1;
  rdesc.SampleDesc.Quality = 0;

  D3D12_HEAP_PROPERTIES heapProps;
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  ID3D12Resource *pSampleResult = NULL;
  D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &rdesc, resourceState,
                                          NULL, __uuidof(ID3D12Resource), (void **)&pSampleResult);
  if(FAILED(hr))
  {
    RDCERR("Failed to create texture for shader debugging HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(samplePso);
    return false;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_RTV);
  m_pDevice->CreateRenderTargetView(pSampleResult, NULL, rtv);
  cmdList->OMSetRenderTargets(1, &rtv, FALSE, NULL);
  cmdList->DrawInstanced(3, 1, 0, 0);

  hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(samplePso);
    SAFE_RELEASE(pSampleResult);
    return false;
  }

  {
    ID3D12CommandList *l = cmdList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->GPUSync();
  }

  rs = prevState;

  bytebuf sampleResult;
  m_pDevice->GetReplay()->GetTextureData(GetResID(pSampleResult), Subresource(),
                                         GetTextureDataParams(), sampleResult);

  ShaderVariable lookupResult("tex", 0.0f, 0.0f, 0.0f, 0.0f);
  memcpy(lookupResult.value.iv, sampleResult.data(),
         RDCMIN(sampleResult.size(), sizeof(uint32_t) * 4));
  output = lookupResult;

  SAFE_RELEASE(samplePso);
  SAFE_RELEASE(pSampleResult);

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
            WrappedID3D12Resource1::GetResIDFromAddr(cbv.BufferLocation, resId, byteOffset);
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
                                           uint32_t idx)
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D12MarkerRegion region(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  const D3D12Pipe::State *pipelineState = GetD3D12PipelineState();
  const D3D12Pipe::Shader &vertexShader = pipelineState->vertexShader;
  WrappedID3D12Shader *vs =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(vertexShader.resourceId);
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

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12PipelineState *pso =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

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
       inputlayout[i].InstanceDataStepRate < draw->numInstances)
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

      if(vb.stride * (draw->vertexOffset + idx) < vb.size)
        GetDebugManager()->GetBufferData(buffer, vb.offs + vb.stride * (draw->vertexOffset + idx),
                                         vb.stride, vertData[i]);

      for(UINT isr = 1; isr <= MaxStepRate; isr++)
      {
        if((draw->instanceOffset + (instid / isr)) < vb.size)
          GetDebugManager()->GetBufferData(
              buffer, vb.offs + vb.stride * (draw->instanceOffset + (instid / isr)), vb.stride,
              instData[i * MaxStepRate + isr - 1]);
      }

      if(vb.stride * draw->instanceOffset < vb.size)
        GetDebugManager()->GetBufferData(buffer, vb.offs + vb.stride * draw->instanceOffset,
                                         vb.stride, staticData[i]);
    }
  }

  InterpretDebugger *interpreter = new InterpretDebugger;
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
        if(el->InstanceDataStepRate == 0 || el->InstanceDataStepRate >= draw->numInstances)
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
        state.inputs[i].value.u.w = 1;

        if(fmt.compType == CompType::Float)
          state.inputs[i].value.f.w = 1.0f;
      }

      // interpret resource format types
      if(fmt.Special())
      {
        Vec3f *v3 = (Vec3f *)state.inputs[i].value.fv;
        Vec4f *v4 = (Vec4f *)state.inputs[i].value.fv;

        // only pull in all or nothing from these,
        // if there's only e.g. 3 bytes remaining don't read and unpack some of
        // a 4-byte resource format type
        size_t packedsize = 4;
        if(fmt.type == ResourceFormatType::R5G5B5A1 || fmt.type == ResourceFormatType::R5G6B5 ||
           fmt.type == ResourceFormatType::R4G4B4A4)
          packedsize = 2;

        if(srcData == NULL || packedsize > dataSize)
        {
          state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z =
              state.inputs[i].value.u.w = 0;
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
            state.inputs[i].value.u.z = (packed >> 0) & 0x3ff;
            state.inputs[i].value.u.y = (packed >> 10) & 0x3ff;
            state.inputs[i].value.u.x = (packed >> 20) & 0x3ff;
            state.inputs[i].value.u.w = (packed >> 30) & 0x003;
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
            state.inputs[i].value.uv[c] = 0;
            continue;
          }

          dataSize -= fmt.compByteWidth;

          if(fmt.compByteWidth == 1)
          {
            byte *src = srcData + c * fmt.compByteWidth;

            if(fmt.compType == CompType::UInt)
              state.inputs[i].value.uv[c] = *src;
            else if(fmt.compType == CompType::SInt)
              state.inputs[i].value.iv[c] = *((int8_t *)src);
            else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
              state.inputs[i].value.fv[c] = float(*src) / 255.0f;
            else if(fmt.compType == CompType::SNorm)
            {
              signed char *schar = (signed char *)src;

              // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
              if(*schar == -128)
                state.inputs[i].value.fv[c] = -1.0f;
              else
                state.inputs[i].value.fv[c] = float(*schar) / 127.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 2)
          {
            uint16_t *src = (uint16_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float)
              state.inputs[i].value.fv[c] = ConvertFromHalf(*src);
            else if(fmt.compType == CompType::UInt)
              state.inputs[i].value.uv[c] = *src;
            else if(fmt.compType == CompType::SInt)
              state.inputs[i].value.iv[c] = *((int16_t *)src);
            else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
              state.inputs[i].value.fv[c] = float(*src) / float(UINT16_MAX);
            else if(fmt.compType == CompType::SNorm)
            {
              int16_t *sint = (int16_t *)src;

              // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
              if(*sint == -32768)
                state.inputs[i].value.fv[c] = -1.0f;
              else
                state.inputs[i].value.fv[c] = float(*sint) / 32767.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 4)
          {
            uint32_t *src = (uint32_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float || fmt.compType == CompType::UInt ||
               fmt.compType == CompType::SInt)
              memcpy(&state.inputs[i].value.uv[c], src, 4);
            else
              RDCERR("Unexpected component type");
          }
        }

        if(fmt.BGRAOrder())
        {
          RDCASSERT(fmt.compCount == 4);
          std::swap(state.inputs[i].value.fv[2], state.inputs[i].value.fv[0]);
        }
      }
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::VertexIndex)
    {
      uint32_t sv_vertid = vertid;

      if(draw->flags & DrawFlags::Indexed)
        sv_vertid = idx - draw->baseVertex;

      if(dxbc->GetReflection()->InputSig[i].varType == VarType::Float)
        state.inputs[i].value.f.x = state.inputs[i].value.f.y = state.inputs[i].value.f.z =
            state.inputs[i].value.f.w = (float)sv_vertid;
      else
        state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z =
            state.inputs[i].value.u.w = sv_vertid;
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::InstanceIndex)
    {
      if(dxbc->GetReflection()->InputSig[i].varType == VarType::Float)
        state.inputs[i].value.f.x = state.inputs[i].value.f.y = state.inputs[i].value.f.z =
            state.inputs[i].value.f.w = (float)instid;
      else
        state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z =
            state.inputs[i].value.u.w = instid;
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

ShaderDebugTrace *D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                          uint32_t primitive)
{
  using namespace DXBC;
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D12MarkerRegion debugpixRegion(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugPixel @ %u of (%u,%u) %u / %u", eventId, x, y, sample, primitive));

  const D3D12Pipe::State *pipelineState = GetD3D12PipelineState();

  // Fetch the disassembly info from the pixel shader
  const D3D12Pipe::Shader &pixelShader = pipelineState->pixelShader;
  WrappedID3D12Shader *ps =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(pixelShader.resourceId);
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
    const D3D12Pipe::Shader &geometryShader = pipelineState->geometryShader;
    WrappedID3D12Shader *gs =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(geometryShader.resourceId);
    if(gs)
      prevDxbc = gs->GetDXBC();
  }
  // Check for domain shader next
  if(prevDxbc == NULL)
  {
    const D3D12Pipe::Shader &domainShader = pipelineState->domainShader;
    WrappedID3D12Shader *ds =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(domainShader.resourceId);
    if(ds)
      prevDxbc = ds->GetDXBC();
  }
  // Check for vertex shader last
  if(prevDxbc == NULL)
  {
    const D3D12Pipe::Shader &vertexShader = pipelineState->vertexShader;
    WrappedID3D12Shader *vs =
        m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(vertexShader.resourceId);
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
  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
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

  // If this event uses MSAA, then at least one render target must be preserved to get
  // multisampling info. leave u0 alone and start with register u1
  extractHlsl += "RWStructuredBuffer<PSInitialData> PSInitialBuffer : register(u1);\n\n";

  if(!evalSampleCacheData.empty())
  {
    // float4 is wasteful in some cases but it's easier than using byte buffers and manual packing
    extractHlsl += "RWBuffer<float4> PSEvalBuffer : register(u2);\n\n";
  }

  if(usePrimitiveID)
  {
    extractHlsl += R"(
void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position, uint prim : SV_PrimitiveID,
                     uint sample : SV_SampleIndex, uint covge : SV_Coverage,
                     bool fface : SV_IsFrontFace)
{
)";
  }
  else
  {
    extractHlsl += R"(
void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position,
                     uint sample : SV_SampleIndex, uint covge : SV_Coverage,
                     bool fface : SV_IsFrontFace)
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
  UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(extractHlsl.c_str(), "ExtractInputsPS", flags,
                                                "ps_5_0", &psBlob) != "")
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

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rs.graphics.rootsig);

  // Need to be able to add a descriptor table with our UAV without hitting the 64 DWORD limit
  RDCASSERT(sig->sig.dwordLength < 64);
  D3D12RootSignature modsig = sig->sig;

  UINT regSpace = modsig.maxSpaceIndex + 1;
  MoveRootSignatureElementsToRegisterSpace(modsig, regSpace, D3D12DescriptorType::UAV,
                                           D3D12_SHADER_VISIBILITY_PIXEL);

  // Create the descriptor table for our UAV
  D3D12_DESCRIPTOR_RANGE1 descRange;
  descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  descRange.NumDescriptors = pMsaaEvalBuffer ? 2 : 1;
  descRange.BaseShaderRegister = 1;
  descRange.RegisterSpace = 0;
  descRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
  descRange.OffsetInDescriptorsFromTableStart = 0;

  modsig.Parameters.push_back(D3D12RootSignatureParameter());
  D3D12RootSignatureParameter &param = modsig.Parameters.back();
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &descRange;

  uint32_t sigElem = uint32_t(modsig.Parameters.size() - 1);

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

  // Add the descriptor for our UAV, then clear it
  std::set<ResourceId> copiedHeaps;
  rdcarray<PortableHandle> debugHandles;
  debugHandles.push_back(ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV)));
  if(pMsaaEvalBuffer)
    debugHandles.push_back(ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV)));
  AddDebugDescriptorsToRenderState(m_pDevice, rs, debugHandles,
                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sigElem, copiedHeaps);

  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  rs.ApplyDescriptorHeaps(cmdList);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuUav = m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_UAV);
  UINT zero[4] = {0, 0, 0, 0};
  cmdList->ClearUnorderedAccessViewUint(gpuUav, clearUav, pInitialValuesBuffer, zero, 0, NULL);

  if(pMsaaEvalBuffer)
  {
    D3D12_GPU_DESCRIPTOR_HANDLE gpuMsaaUav =
        m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_MSAA_UAV);
    cmdList->ClearUnorderedAccessViewUint(gpuMsaaUav, msaaClearUav, pMsaaEvalBuffer, zero, 0, NULL);
  }

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
  D3D12_COMPARISON_FUNC depthFunc = pipeDesc.DepthStencilState.DepthFunc;
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
        else if((depthFunc == D3D12_COMPARISON_FUNC_ALWAYS ||
                 depthFunc == D3D12_COMPARISON_FUNC_NEVER ||
                 depthFunc == D3D12_COMPARISON_FUNC_NOT_EQUAL ||
                 depthFunc == D3D12_COMPARISON_FUNC_EQUAL))
        {
          // For depth functions without an inequality comparison, use the last sample encountered
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
      ins.back().value.u.x = pHit->coverage;

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
          invar.value.u.x = pHit->primitive;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAASampleIndex)
        {
          invar.value.u.x = pHit->sample;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAACoverage)
        {
          invar.value.u.x = pHit->coverage;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::IsFrontFace)
        {
          invar.value.u.x = pHit->isFrontFace ? ~0U : 0;
        }
        else
        {
          rawout = &invar.value.iv[initialValues[i].elem];

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
      memcpy(var.value.fv, evalSampleCache, var.columns * sizeof(float));

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

ShaderDebugTrace *D3D12Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                           const uint32_t threadid[3])
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D12MarkerRegion simloop(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugThread @ %u: [%u, %u, %u] (%u, %u, %u)", eventId, groupid[0],
                        groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]));

  const D3D12Pipe::State *pipelineState = GetD3D12PipelineState();
  const D3D12Pipe::Shader &computeShader = pipelineState->computeShader;
  WrappedID3D12Shader *cs =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(computeShader.resourceId);
  if(!cs)
  {
    RDCERR("Can't debug with no current compute shader");
    return new ShaderDebugTrace;
  }

  DXBC::DXBCContainer *dxbc = cs->GetDXBC();
  const ShaderReflection &refl = cs->GetDetails();

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

  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12PipelineState *pso =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  InterpretDebugger *interpreter = new InterpretDebugger;
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
          memcpy(v.value.uv, state.semantics.GroupID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP:
          memcpy(v.value.uv, state.semantics.ThreadID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID:
          v.value.u.x =
              state.semantics.GroupID[0] * dxbc->GetReflection()->DispatchThreadsDimension[0] +
              state.semantics.ThreadID[0];
          v.value.u.y =
              state.semantics.GroupID[1] * dxbc->GetReflection()->DispatchThreadsDimension[1] +
              state.semantics.ThreadID[1];
          v.value.u.z =
              state.semantics.GroupID[2] * dxbc->GetReflection()->DispatchThreadsDimension[2] +
              state.semantics.ThreadID[2];
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          v.value.u.x =
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
    return NULL;

  D3D12DebugAPIWrapper apiWrapper(m_pDevice, interpreter->dxbc, interpreter->global);

  D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "ContinueDebug Simulation Loop");

  return interpreter->ContinueDebug(&apiWrapper);
}

void D3D12Replay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
}
