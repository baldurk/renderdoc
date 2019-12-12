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

#include "driver/dx/official/d3dcompiler.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_resources.h"
#include "d3d12_shader_cache.h"

#define D3D12SHADERDEBUG_PIXEL 0

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

void GatherConstantBuffers(WrappedID3D12Device *pDevice, DXBC::ShaderType shaderType,
                           const D3D12RenderState::RootSignature &rootsig,
                           bytebuf cbufData[D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT])
{
  WrappedID3D12RootSignature *pD3D12RootSig =
      pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootsig.rootsig);

  size_t numParams = RDCMIN(pD3D12RootSig->sig.Parameters.size(), rootsig.sigelems.size());
  for(size_t i = 0; i < numParams; i++)
  {
    const D3D12RootSignatureParameter &rootSigParam = pD3D12RootSig->sig.Parameters[i];
    const D3D12RenderState::SignatureElement &element = rootsig.sigelems[i];
    if(IsShaderParameterVisible(shaderType, rootSigParam.ShaderVisibility))
    {
      if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
         element.type == eRootConst)
      {
        UINT cbufIndex = rootSigParam.Constants.ShaderRegister;
        UINT sizeBytes = sizeof(uint32_t) * RDCMIN(rootSigParam.Constants.Num32BitValues,
                                                   (UINT)element.constants.size());
        cbufData[cbufIndex].assign((const byte *)element.constants.data(), sizeBytes);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && element.type == eRootCBV)
      {
        UINT cbufIndex = rootSigParam.Descriptor.ShaderRegister;
        ID3D12Resource *cbv = pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(element.id);
        pDevice->GetDebugManager()->GetBufferData(cbv, element.offset, 0, cbufData[cbufIndex]);
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
          // For this traversal we only care about CBV descriptor ranges
          const D3D12_DESCRIPTOR_RANGE1 &range = rootSigParam.ranges[r];
          if(range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
            continue;

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

          UINT cbufIndex = range.BaseShaderRegister;

          for(UINT n = 0; n < numDescriptors; ++n, ++cbufIndex)
          {
            if(desc)
            {
              const D3D12_CONSTANT_BUFFER_VIEW_DESC &cbv = desc->GetCBV();
              ResourceId resId;
              uint64_t byteOffset = 0;
              WrappedID3D12Resource1::GetResIDFromAddr(cbv.BufferLocation, resId, byteOffset);
              ID3D12Resource *pCbvResource =
                  pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(resId);
              pDevice->GetDebugManager()->GetBufferData(pCbvResource, element.offset, 0,
                                                        cbufData[cbufIndex]);
            }
          }
        }
      }
    }
  }
}

ShaderDebugTrace D3D12Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  RDCUNIMPLEMENTED("Vertex debugging not yet implemented for D3D12");
  ShaderDebugTrace ret;
  return ret;
}

#if D3D12SHADERDEBUG_PIXEL == 0

ShaderDebugTrace D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  RDCUNIMPLEMENTED("Pixel debugging not yet implemented for D3D12");
  ShaderDebugTrace ret;
  return ret;
}

#else

ShaderDebugTrace D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  using namespace DXBC;
  using namespace DXBCBytecode;
  using namespace ShaderDebug;

  D3D12MarkerRegion debugpixRegion(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugPixel @ %u of (%u,%u) %u / %u", eventId, x, y, sample, primitive));

  const D3D12Pipe::State *pipelineState = GetD3D12PipelineState();

  ShaderDebugTrace empty;

  // Fetch the disassembly info from the pixel shader
  const D3D12Pipe::Shader &pixelShader = pipelineState->pixelShader;
  WrappedID3D12Shader *ps =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(pixelShader.resourceId);
  if(!ps)
    return empty;

  DXBCContainer *dxbc = ps->GetDXBC();
  const ShaderReflection &refl = ps->GetDetails();

  if(!dxbc)
    return empty;

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

  std::vector<PSInputElement> initialValues;
  std::vector<std::string> floatInputs;
  std::vector<std::string> inputVarNames;
  std::string extractHlsl;
  int structureStride = 0;

  ShaderDebug::GatherPSInputDataForInitialValues(*dxbc->GetReflection(), *prevDxbc->GetReflection(),
                                                 initialValues, floatInputs, inputVarNames,
                                                 extractHlsl, structureStride);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  // get the multisample count
  uint32_t outputSampleCount = RDCMAX(1U, pipelineState->outputMerger.multiSampleCount);

  // if we're not rendering at MSAA, no need to fill the cache because evaluates will all return the
  // plain input anyway.
  if(outputSampleCount > 1)
  {
    RDCUNIMPLEMENTED("MSAA debugging not yet implemented for D3D12");
    return empty;
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

  extractHlsl += "RWStructuredBuffer<PSInitialData> PSInitialBuffer : register(u0);\n\n";

  extractHlsl += R"(
void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position, uint prim : SV_PrimitiveID,
                     uint sample : SV_SampleIndex, uint covge : SV_Coverage,
                     bool fface : SV_IsFrontFace)
{
)";

  extractHlsl += "  uint idx = " + ToStr(overdrawLevels) + ";\n";
  extractHlsl += StringFormat::Fmt(
      "  if(abs(debug_pixelPos.x - %u.5) < 0.5f && abs(debug_pixelPos.y - %u.5) < 0.5f)\n", x, y);
  extractHlsl += "    InterlockedAdd(PSInitialBuffer[0].hit, 1, idx);\n\n";
  extractHlsl += "  idx = min(idx, " + ToStr(overdrawLevels) + ");\n\n";
  extractHlsl += "  PSInitialBuffer[idx].pos = debug_pixelPos.xyz;\n";
  extractHlsl += "  PSInitialBuffer[idx].prim = prim;\n";
  extractHlsl += "  PSInitialBuffer[idx].fface = fface;\n";
  extractHlsl += "  PSInitialBuffer[idx].covge = covge;\n";
  extractHlsl += "  PSInitialBuffer[idx].sample = sample;\n";
  extractHlsl += "  PSInitialBuffer[idx].IN = IN;\n";
  extractHlsl += "  PSInitialBuffer[idx].derivValid = ddx(debug_pixelPos.x);\n";
  extractHlsl += "  PSInitialBuffer[idx].INddx = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddy = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddxfine = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddyfine = (PSInput)0;\n";

  for(size_t i = 0; i < floatInputs.size(); i++)
  {
    const std::string &name = floatInputs[i];
    extractHlsl += "  PSInitialBuffer[idx].INddx." + name + " = ddx(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddy." + name + " = ddy(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddxfine." + name + " = ddx_fine(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddyfine." + name + " = ddy_fine(IN." + name + ");\n";
  }
  extractHlsl += "\n}";

  // Create pixel shader to get initial values from previous stage output
  ID3DBlob *psBlob = NULL;
  UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;
  if(m_pDevice->GetShaderCache()->GetShaderBlob(extractHlsl.c_str(), "ExtractInputsPS", flags,
                                                "ps_5_0", &psBlob) != "")
  {
    RDCERR("Failed to create shader to extract inputs");
    return empty;
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
    return empty;
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

  // Store a copy of the event's render state to restore later
  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
  D3D12RenderState prevState = rs;

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
  descRange.NumDescriptors = 1;
  descRange.BaseShaderRegister = 0;
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
    return empty;
  }
  SAFE_RELEASE(root);

  WrappedID3D12PipelineState *origPSO =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  RDCASSERT(origPSO->IsGraphics());

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
  origPSO->Fill(pipeDesc);

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
    SAFE_RELEASE(pRootSignature);
    return empty;
  }

  // Add the descriptor for our UAV, then clear it
  std::set<ResourceId> copiedHeaps;
  PortableHandle shaderDebugUav = ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV));
  AddDebugDescriptorToRenderState(m_pDevice, rs, shaderDebugUav,
                                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sigElem, copiedHeaps);

  ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  rs.ApplyDescriptorHeaps(cmdList);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuUav = m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_UAV);
  UINT zero[4] = {0, 0, 0, 0};
  cmdList->ClearUnorderedAccessViewUint(gpuUav, clearUav, pInitialValuesBuffer, zero, 0, NULL);

  // Execute the command to ensure that UAV clear and resource creation occur before replay
  hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(psBlob);
    SAFE_RELEASE(pInitialValuesBuffer);
    SAFE_RELEASE(pRootSignature);
    SAFE_RELEASE(initialPso);
    return empty;
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

  // Replaying the event has finished, and the data has been copied out.
  // Free all the resources that were created.
  SAFE_RELEASE(psBlob);
  SAFE_RELEASE(pRootSignature);
  SAFE_RELEASE(pInitialValuesBuffer);
  SAFE_RELEASE(initialPso);

  DebugHit *buf = (DebugHit *)initialData.data();

  D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                         StringFormat::Fmt("Got %u hits", buf[0].numHits));
  if(buf[0].numHits == 0)
  {
    RDCLOG("No hit for this event");
    return empty;
  }

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  // our debugging quad. Order is TL, TR, BL, BR
  State quad[4];

  // figure out the TL pixel's coords. Assume even top left (towards 0,0)
  // this isn't spec'd but is a reasonable assumption.
  int xTL = x & (~1);
  int yTL = y & (~1);

  // get the index of our desired pixel
  int destIdx = (x - xTL) + 2 * (y - yTL);

  // Fetch constant buffer data from root signature
  bytebuf cbufData[D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  GatherConstantBuffers(m_pDevice, dxbc->m_Type, rs.graphics, cbufData);

  // Get depth func and determine "winner" pixel
  D3D12_COMPARISON_FUNC depthFunc = pipeDesc.DepthStencilState.DepthFunc;
  DebugHit *pWinnerHit = NULL;

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
      }
    }
  }

  if(pWinnerHit == NULL)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *pHit = (DebugHit *)(initialData.data() + i * structStride);

      if(pWinnerHit == NULL || (pWinnerHit->sample != sample && pHit->sample == sample) ||
         depthFunc == D3D12_COMPARISON_FUNC_ALWAYS || depthFunc == D3D12_COMPARISON_FUNC_NEVER ||
         depthFunc == D3D12_COMPARISON_FUNC_NOT_EQUAL || depthFunc == D3D12_COMPARISON_FUNC_EQUAL)
      {
        pWinnerHit = pHit;
        continue;
      }

      if((depthFunc == D3D12_COMPARISON_FUNC_LESS && pHit->depth < pWinnerHit->depth) ||
         (depthFunc == D3D12_COMPARISON_FUNC_LESS_EQUAL && pHit->depth <= pWinnerHit->depth) ||
         (depthFunc == D3D12_COMPARISON_FUNC_GREATER && pHit->depth > pWinnerHit->depth) ||
         (depthFunc == D3D12_COMPARISON_FUNC_GREATER_EQUAL && pHit->depth >= pWinnerHit->depth))
      {
        if(pHit->sample == sample)
        {
          pWinnerHit = pHit;
        }
      }
    }
  }

  if(pWinnerHit == NULL)
  {
    RDCLOG("Couldn't find any pixels that passed depth test at target coordinates");
    return empty;
  }

  ShaderDebugTrace traces[4];

  GlobalState global;
  GetDebugManager()->CreateShaderGlobalState(global, dxbc);

  {
    DebugHit *pHit = pWinnerHit;
    State initialState;
    CreateShaderDebugStateAndTrace(initialState, traces[destIdx], destIdx, dxbc, refl, cbufData);

    rdcarray<ShaderVariable> &ins = traces[destIdx].inputs;
    if(!ins.empty() && ins.back().name == "vCoverage")
      ins.back().value.u.x = pHit->coverage;

    initialState.semantics.coverage = pHit->coverage;
    initialState.semantics.primID = pHit->primitive;
    initialState.semantics.isFrontFace = pHit->isFrontFace;

    uint32_t *data = &pHit->rawdata;

    float *pos_ddx = (float *)data;

    // ddx(SV_Position.x) MUST be 1.0
    if(*pos_ddx != 1.0f)
    {
      RDCERR("Derivatives invalid");
      return empty;
    }

    data++;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      int32_t *rawout = NULL;

      if(initialValues[i].reg >= 0)
      {
        ShaderVariable &invar = traces[destIdx].inputs[initialValues[i].reg];

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
        traces[i] = traces[destIdx];
      quad[i] = initialState;
      quad[i].SetTrace(i, &traces[i]);
      if(i != destIdx)
        quad[i].SetHelper();
    }

    // TODO: Handle inputs that were evaluated at sample granularity (MSAA)

    ApplyAllDerivatives(global, traces, destIdx, initialValues, (float *)data);
  }

  std::vector<ShaderDebugState> states;

  if(dxbc->GetDebugInfo())
    dxbc->GetDebugInfo()->GetLocals(0, dxbc->GetDXBCByteCode()->GetInstruction(0).offset,
                                    quad[destIdx].locals);

  states.push_back(quad[destIdx]);

  // ping pong between so that we can have 'current' quad to update into new one
  State quad2[4];

  State *curquad = quad;
  State *newquad = quad2;

  // marks any threads stalled waiting for others to catch up
  bool activeMask[4] = {true, true, true, true};

  int cycleCounter = 0;

  D3D12MarkerRegion simloop(m_pDevice->GetQueue()->GetReal(), "Simulation Loop");

  D3D12DebugAPIWrapper apiWrapper(m_pDevice, dxbc, global);

  // simulate lockstep until all threads are finished
  bool finished = true;
  do
  {
    for(size_t i = 0; i < 4; i++)
    {
      if(activeMask[i])
        newquad[i] = curquad[i].GetNext(global, &apiWrapper, curquad);
      else
        newquad[i] = curquad[i];
    }

    State *a = curquad;
    curquad = newquad;
    newquad = a;

    // if our destination quad is paused don't record multiple identical states.
    if(activeMask[destIdx])
    {
      State &s = curquad[destIdx];

      if(dxbc->GetDebugInfo())
      {
        size_t inst =
            RDCMIN((size_t)s.nextInstruction, dxbc->GetDXBCByteCode()->GetNumInstructions() - 1);
        const Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(inst);
        dxbc->GetDebugInfo()->GetLocals(s.nextInstruction, op.offset, s.locals);
      }

      states.push_back(s);
    }

    // we need to make sure that control flow which converges stays in lockstep so that
    // derivatives are still valid. While diverged, we don't have to keep threads in lockstep
    // since using derivatives is invalid.

    // Threads diverge either in ifs, loops, or switches. Due to the nature of the bytecode,
    // all threads *must* pass through the same exit instruction for each, there's no jumping
    // around with gotos. Note also for the same reason, the only time threads are on earlier
    // instructions is if they are still catching up to a thread that has exited the control
    // flow.

    // So the scheme is as follows:
    // * If all threads have the same nextInstruction, just continue we are still in lockstep.
    // * If threads are out of lockstep, find any thread which has nextInstruction pointing
    //   immediately *after* an ENDIF, ENDLOOP or ENDSWITCH. Pointing directly at one is not
    //   an indication the thread is done, as the next step for an ENDLOOP will jump back to
    //   the matching LOOP and continue iterating.
    // * Pause any thread matching the above until all threads are pointing to the same
    //   instruction. By the assumption above, all threads will eventually pass through this
    //   terminating instruction so we just pause any other threads and don't do anything
    //   until the control flow has converged and we can continue stepping in lockstep.

    // mark all threads as active again.
    // if we've converged, or we were never diverged, this keeps everything ticking
    activeMask[0] = activeMask[1] = activeMask[2] = activeMask[3] = true;

    if(curquad[0].nextInstruction != curquad[1].nextInstruction ||
       curquad[0].nextInstruction != curquad[2].nextInstruction ||
       curquad[0].nextInstruction != curquad[3].nextInstruction)
    {
      // this isn't *perfect* but it will still eventually continue. We look for the most
      // advanced thread, and check to see if it's just finished a control flow. If it has
      // then we assume it's at the convergence point and wait for every other thread to
      // catch up, pausing any threads that reach the convergence point before others.

      // Note this might mean we don't have any threads paused even within divergent flow.
      // This is fine and all we care about is pausing to make sure threads don't run ahead
      // into code that should be lockstep. We don't care at all about what they do within
      // the code that is divergent.

      // The reason this isn't perfect is that the most advanced thread could be on an
      // inner loop or inner if, not the convergence point, and we could be pausing it
      // fruitlessly. Worse still - it could be on a branch none of the other threads will
      // take so they will never reach that exact instruction.
      // But we know that all threads will eventually go through the convergence point, so
      // even in that worst case if we didn't pick the right waiting point, another thread
      // will overtake and become the new most advanced thread and the previous waiting
      // thread will resume. So in this case we caused a thread to wait more than it should
      // have but that's not a big deal as it's within divergent flow so they don't have to
      // stay in lockstep. Also if all threads will eventually pass that point we picked,
      // we just waited to converge even in technically divergent code which is also
      // harmless.

      // Phew!

      uint32_t convergencePoint = 0;

      // find which thread is most advanced
      for(size_t i = 0; i < 4; i++)
        if(curquad[i].nextInstruction > convergencePoint)
          convergencePoint = curquad[i].nextInstruction;

      if(convergencePoint > 0)
      {
        OpcodeType op = dxbc->GetDXBCByteCode()->GetInstruction(convergencePoint - 1).operation;

        // if the most advnaced thread hasn't just finished control flow, then all
        // threads are still running, so don't converge
        if(op != OPCODE_ENDIF && op != OPCODE_ENDLOOP && op != OPCODE_ENDSWITCH)
          convergencePoint = 0;
      }

      // pause any threads at that instruction (could be none)
      for(size_t i = 0; i < 4; i++)
        if(curquad[i].nextInstruction == convergencePoint)
          activeMask[i] = false;
    }

    finished = curquad[destIdx].Finished();

    cycleCounter++;

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(cycleCounter))
        break;
    }
  } while(!finished);

  traces[destIdx].states = states;

  traces[destIdx].hasLocals = dxbc->GetDebugInfo() && dxbc->GetDebugInfo()->HasLocals();

  traces[destIdx].lineInfo.resize(dxbc->GetDXBCByteCode()->GetNumInstructions());
  for(size_t i = 0; dxbc->GetDebugInfo() && i < dxbc->GetDXBCByteCode()->GetNumInstructions(); i++)
  {
    const Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(i);
    dxbc->GetDebugInfo()->GetLineInfo(i, op.offset, traces[destIdx].lineInfo[i]);
  }

  return traces[destIdx];
}

#endif    // D3D12SHADERDEBUG_PIXEL

ShaderDebugTrace D3D12Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  RDCUNIMPLEMENTED("Compute shader debugging not yet implemented for D3D12");
  ShaderDebugTrace ret;
  return ret;
}
