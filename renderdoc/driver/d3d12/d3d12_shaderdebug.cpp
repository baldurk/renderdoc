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
#include "d3d12_command_queue.h"
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

bool IsShaderParameterVisible(DXBC::ShaderType shaderType, D3D12_SHADER_VISIBILITY shaderVisibility)
{
  if(shaderVisibility == D3D12_SHADER_VISIBILITY_ALL)
    return true;

  if(shaderType == DXBC::ShaderType::Vertex && shaderVisibility == D3D12_SHADER_VISIBILITY_VERTEX)
    return true;

  if(shaderType == DXBC::ShaderType::Pixel && shaderVisibility == D3D12_SHADER_VISIBILITY_PIXEL)
    return true;

  return false;
}

void D3D12DebugManager::CreateShaderGlobalState(ShaderDebug::GlobalState &global,
                                                DXBC::DXBCContainer *dxbc)
{
  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(dxbc->m_Type == DXBC::ShaderType::Compute)
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
      if(IsShaderParameterVisible(dxbc->m_Type, param.ShaderVisibility))
      {
        // Note that constant buffers are not handled as part of the shader global state

        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && element.type == eRootSRV)
        {
          UINT shaderReg = param.Descriptor.ShaderRegister;
          ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);
          D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

          // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
          // resource desc format or the DXBC reflection info might be more correct.
          ShaderDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, global.srvs[shaderReg].format);
          global.srvs[shaderReg].firstElement = (uint32_t)(element.offset / sizeof(uint32_t));
          global.srvs[shaderReg].numElements =
              (uint32_t)((resDesc.Width - element.offset) / sizeof(uint32_t));

          if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
            GetBufferData(pResource, 0, 0, global.srvs[shaderReg].data);
        }
        else if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV && element.type == eRootUAV)
        {
          UINT shaderReg = param.Descriptor.ShaderRegister;
          ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(element.id);
          D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();

          // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
          // resource desc format or the DXBC reflection info might be more correct.
          ShaderDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, global.uavs[shaderReg].format);
          global.uavs[shaderReg].firstElement = (uint32_t)(element.offset / sizeof(uint32_t));
          global.uavs[shaderReg].numElements =
              (uint32_t)((resDesc.Width - element.offset) / sizeof(uint32_t));

          if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
            GetBufferData(pResource, 0, 0, global.uavs[shaderReg].data);
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

            UINT shaderReg = range.BaseShaderRegister;

            if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
            {
              for(UINT n = 0; n < numDescriptors; ++n, ++shaderReg)
              {
                if(desc)
                {
                  ResourceId srvId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);

                  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = desc->GetSRV();
                  if(srvDesc.Format != DXGI_FORMAT_UNKNOWN)
                  {
                    ShaderDebug::FillViewFmt(srvDesc.Format, global.srvs[shaderReg].format);
                  }
                  else
                  {
                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                      global.srvs[shaderReg].format.stride = srvDesc.Buffer.StructureByteStride;

                      // If we didn't get a type from the SRV description, try to pull it from the
                      // shader reflection info
                      ShaderDebug::LookupSRVFormatFromShaderReflection(
                          *dxbc->GetReflection(), (uint32_t)shaderReg, global.srvs[shaderReg].format);
                    }
                  }

                  if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
                  {
                    global.srvs[shaderReg].firstElement = (uint32_t)srvDesc.Buffer.FirstElement;
                    global.srvs[shaderReg].numElements = srvDesc.Buffer.NumElements;

                    GetBufferData(pResource, 0, 0, global.srvs[shaderReg].data);
                  }
                  // Textures are sampled via a pixel shader, so there's no need to copy their data

                  desc++;
                }
              }
            }
            else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
            {
              for(UINT n = 0; n < numDescriptors; ++n, ++shaderReg)
              {
                if(desc)
                {
                  ResourceId uavId = desc->GetResResourceId();
                  ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);

                  // TODO: Need to fetch counter resource if applicable

                  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = desc->GetUAV();
                  if(uavDesc.Format != DXGI_FORMAT_UNKNOWN)
                  {
                    ShaderDebug::FillViewFmt(uavDesc.Format, global.uavs[shaderReg].format);
                  }
                  else
                  {
                    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
                    if(resDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                    {
                      global.uavs[shaderReg].format.stride = uavDesc.Buffer.StructureByteStride;

                      // TODO: Try looking up UAV from shader reflection info?
                    }
                  }

                  if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
                  {
                    global.uavs[shaderReg].firstElement = (uint32_t)uavDesc.Buffer.FirstElement;
                    global.uavs[shaderReg].numElements = uavDesc.Buffer.NumElements;

                    GetBufferData(pResource, 0, 0, global.uavs[shaderReg].data);
                  }
                  else
                  {
                    // TODO: Handle texture resources in UAVs - need to copy/map to fetch the data
                    global.uavs[shaderReg].tex = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  global.PopulateGroupshared(dxbc->GetDXBCByteCode());
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
