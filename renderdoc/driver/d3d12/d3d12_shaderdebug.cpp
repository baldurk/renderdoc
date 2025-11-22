/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2025 Baldur Karlsson
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

#include "d3d12_shaderdebug.h"
#include "core/settings.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "driver/shaders/dxil/dxil_debug.h"
#include "maths/formatpacking.h"
#include "replay/common/var_dispatch_helpers.h"
#include "strings/string_utils.h"
#include "d3d12_command_queue.h"
#include "d3d12_debug.h"
#include "d3d12_dxil_debug.h"
#include "d3d12_replay.h"
#include "d3d12_resources.h"
#include "d3d12_rootsig.h"
#include "d3d12_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

using namespace DXBCBytecode;

const uint64_t s_MathOpResultByteSize = sizeof(Vec4f) * 2;
const uint64_t s_SampleGatherOpResultByteSize = sizeof(Vec4f) * 6;

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

static D3D12_DESCRIPTOR_RANGE_TYPE ConvertOperandTypeToDescriptorType(DXBCBytecode::OperandType type)
{
  D3D12_DESCRIPTOR_RANGE_TYPE descType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

  switch(type)
  {
    case DXBCBytecode::TYPE_SAMPLER: descType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
    case DXBCBytecode::TYPE_RESOURCE: descType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
    case DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW:
      descType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      break;
    case DXBCBytecode::TYPE_CONSTANT_BUFFER: descType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
    default: RDCERR("Unknown operand type %s", ToStr(type).c_str());
  };
  return descType;
}

// Helpers used by DXBC and DXIL debuggers to interact with GPU and resources
bool D3D12ShaderDebug::QueueMathIntrinsic(bool dxil, WrappedID3D12Device *device,
                                          ID3D12GraphicsCommandListX *cmdList, int mathOp,
                                          const ShaderVariable &input, const uint32_t queueIndex)
{
  D3D12MarkerRegion region(device->GetQueue()->GetReal(), "QueueMathIntrinsic");

  ID3D12Resource *pResultBuffer = device->GetDebugManager()->GetShaderDebugResultBuffer();
  ID3D12Resource *pReadbackBuffer = device->GetDebugManager()->GetReadbackBuffer();

  DebugMathOperation cbufferData = {};
  memcpy(&cbufferData.mathInVal, input.value.f32v.data(), sizeof(Vec4f));
  cbufferData.mathOp = mathOp;

  // Set root signature & sig params on command list, then execute the shader
  device->GetDebugManager()->SetDescriptorHeaps(cmdList, true, false);
  cmdList->SetPipelineState(dxil ? device->GetDebugManager()->GetDXILMathIntrinsicsPso()
                                 : device->GetDebugManager()->GetMathIntrinsicsPso());
  cmdList->SetComputeRootSignature(device->GetDebugManager()->GetShaderDebugRootSig());
  cmdList->SetComputeRootConstantBufferView(
      0, device->GetDebugManager()->UploadConstants(&cbufferData, sizeof(cbufferData)));
  cmdList->SetComputeRootUnorderedAccessView(1, pResultBuffer->GetGPUVirtualAddress());
  cmdList->Dispatch(1, 1, 1);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = pResultBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  cmdList->ResourceBarrier(1, &barrier);

  uint64_t destOffset = queueIndex * s_MathOpResultByteSize;
  cmdList->CopyBufferRegion(pReadbackBuffer, destOffset, pResultBuffer, 0, s_MathOpResultByteSize);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  cmdList->ResourceBarrier(1, &barrier);

  return true;
}

bool D3D12ShaderDebug::QueueSampleGather(
    bool dxil, WrappedID3D12Device *device, ID3D12GraphicsCommandListX *cmdList, int sampleOp,
    SampleGatherResourceData resourceData, SampleGatherSamplerData samplerData,
    const ShaderVariable &uvIn, const ShaderVariable &ddxCalcIn, const ShaderVariable &ddyCalcIn,
    const int8_t texelOffsets[3], int multisampleIndex, float lodValue, float compareValue,
    const uint8_t swizzle[4], GatherChannel gatherChannel, const DXBC::ShaderType shaderType,
    uint32_t instruction, const char *opString, const uint32_t queueIndex, int &sampleRetType)
{
  D3D12MarkerRegion region(device->GetQueue()->GetReal(), "QueueSampleGather");

  ShaderVariable uv(uvIn);
  ShaderVariable ddxCalc(ddxCalcIn);
  ShaderVariable ddyCalc(ddyCalcIn);

  for(uint32_t i = 0; i < ddxCalc.columns; i++)
  {
    if(!RDCISFINITE(ddxCalc.value.f32v[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddxCalc.value.f32v[i] = 0.0f;

      device->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                              MessageSource::RuntimeWarning,
                              StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                "texture lookup ddx - using 0.0 instead",
                                                instruction, opString));
    }
    if(!RDCISFINITE(ddyCalc.value.f32v[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddyCalc.value.f32v[i] = 0.0f;

      device->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                              MessageSource::RuntimeWarning,
                              StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                "texture lookup ddy - using 0.0 instead",
                                                instruction, opString));
    }
  }

  for(uint32_t i = 0; i < uv.columns; i++)
  {
    if(sampleOp != DEBUG_SAMPLE_TEX_LOAD && sampleOp != DEBUG_SAMPLE_TEX_LOAD_MS &&
       (!RDCISFINITE(uv.value.f32v[i])))
    {
      RDCWARN("NaN or Inf in texlookup");
      uv.value.f32v[i] = 0.0f;

      device->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                              MessageSource::RuntimeWarning,
                              StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                "texture lookup uv - using 0.0 instead",
                                                instruction, opString));
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
      DEBUG_SAMPLE_SNORM,    // RETURN_TYPE_SNORM
      DEBUG_SAMPLE_INT,      // RETURN_TYPE_SINT
      DEBUG_SAMPLE_UINT,     // RETURN_TYPE_UINT
      DEBUG_SAMPLE_FLOAT,    // RETURN_TYPE_FLOAT
      0,                     // RETURN_TYPE_MIXED
      DEBUG_SAMPLE_FLOAT,    // RETURN_TYPE_DOUBLE (treat as floats)
      0,                     // RETURN_TYPE_CONTINUED
      0,                     // RETURN_TYPE_UNUSED
  };

  cbufferData.debugSampleRetType = retTypes[resourceData.retType];
  if(cbufferData.debugSampleRetType == 0)
  {
    RDCERR("Unsupported return type %d in sample operation", resourceData.retType);
  }
  sampleRetType = cbufferData.debugSampleRetType;

  cbufferData.debugSampleGatherChannel = (int)gatherChannel;
  cbufferData.debugSampleSampleIndex = multisampleIndex;
  cbufferData.debugSampleOperation = sampleOp;
  cbufferData.debugSampleLod = lodValue;
  cbufferData.debugSampleCompare = compareValue;

  uint32_t srvStart = FIRST_SHADDEBUG_SRV;
  srvStart += ShaderDebugConstants::COUNT_SRVS_PER_DEBUG * queueIndex;
  CBVUAVSRVSlot srvSlot = (CBVUAVSRVSlot)srvStart;
  D3D12_CPU_DESCRIPTOR_HANDLE srv = device->GetDebugManager()->GetCPUHandle(srvSlot);
  srv.ptr += ((cbufferData.debugSampleTexDim - 1) + 5 * (cbufferData.debugSampleRetType - 1)) *
             sizeof(D3D12Descriptor);
  {
    D3D12Descriptor descriptor =
        FindDescriptor(device, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, resourceData.binding, shaderType);

    descriptor.Create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, device, srv);
  }

  uint32_t samplerStart = SHADDEBUG_SAMPLER0;
  samplerStart += ShaderDebugConstants::COUNT_SAMPLERS_PER_DEBUG * queueIndex;
  SamplerSlot samplerSlot = (SamplerSlot)samplerStart;
  if(samplerData.mode != SamplerMode::NUM_SAMPLERS)
  {
    D3D12Descriptor descriptor =
        FindDescriptor(device, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, samplerData.binding, shaderType);

    D3D12_CPU_DESCRIPTOR_HANDLE samp = device->GetDebugManager()->GetCPUHandle(samplerSlot);

    if(sampleOp == DEBUG_SAMPLE_TEX_SAMPLE_CMP || sampleOp == DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO ||
       sampleOp == DEBUG_SAMPLE_TEX_GATHER4_CMP || sampleOp == DEBUG_SAMPLE_TEX_GATHER4_PO_CMP)
      samp.ptr += sizeof(D3D12Descriptor);

    if((sampleOp == DEBUG_SAMPLE_TEX_SAMPLE_BIAS || sampleOp == DEBUG_SAMPLE_TEX_SAMPLE_CMP_BIAS) &&
       samplerData.bias != 0.0f)
    {
      D3D12_SAMPLER_DESC2 desc = descriptor.GetSampler();
      desc.MipLODBias = RDCCLAMP(desc.MipLODBias + samplerData.bias, -15.99f, 15.99f);
      descriptor.Init(&desc);
    }
    descriptor.Create(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, device, samp);
  }

  // Store a copy of the event's render state to restore later
  D3D12RenderState &rs = device->GetQueue()->GetCommandData()->m_RenderState;
  D3D12RenderState prevState = rs;

  ID3D12RootSignature *sig = device->GetDebugManager()->GetShaderDebugRootSig();
  ID3D12PipelineState *pso = dxil ? device->GetDebugManager()->GetDXILTexSamplePso(texelOffsets)
                                  : device->GetDebugManager()->GetTexSamplePso(texelOffsets);

  rs.pipe = GetResID(pso);
  rs.rts.clear();
  // Set viewport/scissor unconditionally - we need to set this all the time for sampling for a
  // compute shader, but also a graphics action might exclude pixel (0, 0) from its view or scissor
  rs.views.clear();
  rs.views.push_back({0, 0, 1, 1, 0, 1});
  rs.scissors.clear();
  rs.scissors.push_back({0, 0, 1, 1});

  device->GetDebugManager()->SetDescriptorHeaps(rs.heaps, true, true);

  // Set our modified root signature, and transfer sigelems if we're debugging a compute shader
  rs.graphics.rootsig = GetResID(sig);
  rs.graphics.sigelems.clear();
  rs.compute.rootsig = ResourceId();
  rs.compute.sigelems.clear();

  ID3D12Resource *pResultBuffer = device->GetDebugManager()->GetShaderDebugResultBuffer();
  ID3D12Resource *pReadbackBuffer = device->GetDebugManager()->GetReadbackBuffer();

  rs.graphics.sigelems = {
      D3D12RenderState::SignatureElement(
          eRootCBV, device->GetDebugManager()->UploadConstants(&cbufferData, sizeof(cbufferData))),
      D3D12RenderState::SignatureElement(eRootUAV, pResultBuffer->GetGPUVirtualAddress()),
      D3D12RenderState::SignatureElement(eRootTable, device->GetDebugManager()->GetCPUHandle(srvSlot)),
      D3D12RenderState::SignatureElement(eRootTable,
                                         device->GetDebugManager()->GetCPUHandle(samplerSlot)),
  };

  rs.topo = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  rs.ApplyState(device, cmdList);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = device->GetDebugManager()->GetCPUHandle(PICK_PIXEL_RTV);
  cmdList->OMSetRenderTargets(1, &rtv, FALSE, NULL);
  cmdList->DrawInstanced(3, 1, 0, 0);

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = pResultBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  cmdList->ResourceBarrier(1, &barrier);

  const uint64_t sampleGatherOpResultsStart(ShaderDebugConstants::MAX_SHADER_DEBUG_QUEUED_OPS *
                                            s_MathOpResultByteSize);

  uint64_t destOffset = sampleGatherOpResultsStart + queueIndex * s_SampleGatherOpResultByteSize;
  cmdList->CopyBufferRegion(pReadbackBuffer, destOffset, pResultBuffer, 0,
                            s_SampleGatherOpResultByteSize);

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  cmdList->ResourceBarrier(1, &barrier);

  // Restore D3D12 state to what the event uses
  rs = prevState;
  return true;
}

bool D3D12ShaderDebug::GetQueuedResults(WrappedID3D12Device *device,
                                        ID3D12GraphicsCommandListX *cmdList,
                                        rdcarray<ShaderVariable *> &mathOpResults,
                                        uint32_t countMathResultsPerGpuOp,
                                        rdcarray<ShaderVariable *> &sampleGatherResults,
                                        const rdcarray<int> &sampleRetTypes,
                                        const rdcarray<const uint8_t *> &swizzles)
{
  RDCASSERTEQUAL(sampleGatherResults.size(), sampleRetTypes.size());
  RDCASSERTEQUAL(sampleGatherResults.size(), swizzles.size());

  HRESULT hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  {
    ID3D12CommandList *l = cmdList;
    device->GetQueue()->ExecuteCommandLists(1, &l);
    device->InternalQueueWaitForIdle();
    device->GetDebugManager()->ResetDebugAlloc();
  }

  ID3D12Resource *pReadbackBuffer = device->GetDebugManager()->GetReadbackBuffer();

  byte *gpuResults = NULL;
  hr = pReadbackBuffer->Map(0, NULL, (void **)&gpuResults);

  if(FAILED(hr))
  {
    pReadbackBuffer->Unmap(0, NULL);
    RDCERR("Failed to map readback buffer HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  uintptr_t bufferEnd = (uintptr_t)(gpuResults + pReadbackBuffer->GetDesc().Width);

  byte *gpuMathOpResults = gpuResults;
  for(uint32_t i = 0; i < mathOpResults.size(); i += countMathResultsPerGpuOp)
  {
    const size_t countBytes = sizeof(Vec4f);
    const size_t countBytesPerGpuOp = countBytes * countMathResultsPerGpuOp;
    RDCASSERT((uintptr_t)gpuMathOpResults + countBytesPerGpuOp <= bufferEnd,
              (uintptr_t)gpuMathOpResults, countBytesPerGpuOp, bufferEnd);
    RDCASSERT(countBytesPerGpuOp <= s_MathOpResultByteSize, countBytesPerGpuOp,
              s_MathOpResultByteSize);

    for(uint32_t r = 0; r < countMathResultsPerGpuOp; r++)
    {
      ShaderVariable *result = mathOpResults[i + r];
      memcpy(result->value.u32v.data(), gpuMathOpResults + r * countBytes, countBytes);
    }
    gpuMathOpResults += s_MathOpResultByteSize;
  }

  const uint64_t sampleGatherOpResultsStart(ShaderDebugConstants::MAX_SHADER_DEBUG_QUEUED_OPS *
                                            s_MathOpResultByteSize);
  byte *gpuSampleGatherOpResults = gpuResults + sampleGatherOpResultsStart;
  for(uint32_t s = 0; s < sampleGatherResults.size(); ++s)
  {
    float *retFloats = (float *)gpuSampleGatherOpResults;
    uint32_t *retUInts = (uint32_t *)(retFloats + 8);
    int32_t *retSInts = (int32_t *)(retUInts + 8);

    size_t countBytes = 16;
    RDCASSERT((uintptr_t)gpuSampleGatherOpResults + countBytes <= bufferEnd,
              (uintptr_t)gpuSampleGatherOpResults, countBytes, bufferEnd);
    RDCASSERT(countBytes <= s_SampleGatherOpResultByteSize, countBytes,
              s_SampleGatherOpResultByteSize);

    ShaderVariable &output = *sampleGatherResults[s];

    int debugSampleRetType = sampleRetTypes[s];
    const uint8_t *swizzle = swizzles[s];
    if(debugSampleRetType == DEBUG_SAMPLE_UINT)
    {
      for(int i = 0; i < 4; i++)
        output.value.u32v[i] = retUInts[swizzle[i]];
    }
    else if(debugSampleRetType == DEBUG_SAMPLE_INT)
    {
      for(int i = 0; i < 4; i++)
        output.value.s32v[i] = retSInts[swizzle[i]];
    }
    else
    {
      for(int i = 0; i < 4; i++)
        output.value.f32v[i] = retFloats[swizzle[i]];
    }
    gpuSampleGatherOpResults += s_SampleGatherOpResultByteSize;
  }

  pReadbackBuffer->Unmap(0, NULL);

  return true;
}

D3D12Descriptor D3D12ShaderDebug::FindDescriptor(WrappedID3D12Device *device,
                                                 const DXDebug::HeapDescriptorType heapType,
                                                 uint32_t descriptorIndex)
{
  RDCASSERT(heapType != HeapDescriptorType::NoHeap);

  const D3D12RenderState &rs = device->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = device->GetResourceManager();
  // Fetch the correct heap sampler and resource descriptor heaps
  WrappedID3D12DescriptorHeap *descHeap = NULL;

  rdcarray<ResourceId> descHeaps = rs.heaps;
  for(ResourceId heapId : descHeaps)
  {
    WrappedID3D12DescriptorHeap *pD3D12Heap = rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(heapId);
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = pD3D12Heap->GetDesc();
    if(heapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    {
      if((heapType == HeapDescriptorType::Sampler) && (descHeap == NULL))
        descHeap = pD3D12Heap;
    }
    else
    {
      RDCASSERT(heapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      if((heapType == HeapDescriptorType::CBV_SRV_UAV) && (descHeap == NULL))
        descHeap = pD3D12Heap;
    }
  }

  if(descHeap == NULL)
  {
    RDCERR("Couldn't find descriptor heap type %u", heapType);
    return D3D12Descriptor();
  }

  D3D12Descriptor *desc = (D3D12Descriptor *)descHeap->GetCPUDescriptorHandleForHeapStart().ptr;
  if(descriptorIndex >= descHeap->GetNumDescriptors())
  {
    RDCERR("Descriptor index %u out of bounds Max:%u", descriptorIndex,
           descHeap->GetNumDescriptors());
    return D3D12Descriptor();
  }

  desc += descriptorIndex;
  return *desc;
}

D3D12Descriptor D3D12ShaderDebug::FindDescriptor(WrappedID3D12Device *device,
                                                 D3D12_DESCRIPTOR_RANGE_TYPE descType,
                                                 const BindingSlot &slot,
                                                 const DXBC::ShaderType shaderType)
{
  D3D12Descriptor descriptor;

  if(slot.heapType != DXDebug::HeapDescriptorType::NoHeap)
  {
    return FindDescriptor(device, slot.heapType, slot.descriptorIndex);
  }

  const D3D12RenderState &rs = device->GetQueue()->GetCommandData()->m_RenderState;
  D3D12ResourceManager *rm = device->GetResourceManager();

  // Get the root signature
  const D3D12RenderState::RootSignature *pRootSignature = NULL;
  if(shaderType == DXBC::ShaderType::Compute)
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

    if(descType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
    {
      for(const D3D12_STATIC_SAMPLER_DESC1 &samp : pD3D12RootSig->sig.StaticSamplers)
      {
        if(samp.RegisterSpace == slot.registerSpace && samp.ShaderRegister == slot.shaderRegister)
        {
          D3D12_SAMPLER_DESC2 desc = ConvertStaticSampler(samp);
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
      if(IsShaderParameterVisible(shaderType, param.ShaderVisibility))
      {
        if(param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV && element.type == eRootSRV &&
           descType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
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
                descType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
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

            // For every range, check the number of descriptors so that we are accessing the correct
            // data for append descriptor tables, even if the range type doesn't match what we need to fetch
            UINT offset = range.OffsetInDescriptorsFromTableStart;
            if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
              offset = prevTableOffset;

            UINT numDescriptors = range.NumDescriptors;
            if(numDescriptors == UINT_MAX)
            {
              // Find out how many descriptors are left after
              numDescriptors = heap->GetNumDescriptors() - offset - (UINT)element.offset;

              // TODO: Should we look up the bind point in the D3D12 state to try to get a better
              // guess at the number of descriptors?
            }

            prevTableOffset = offset + numDescriptors;

            if(range.RangeType != descType)
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

ShaderVariable D3D12ShaderDebug::GetResourceInfo(WrappedID3D12Device *device,
                                                 D3D12_DESCRIPTOR_RANGE_TYPE descType,
                                                 const DXDebug::BindingSlot &slot, uint32_t mipLevel,
                                                 const DXBC::ShaderType shaderType, int &dim,
                                                 bool isDXIL)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

  D3D12ResourceManager *rm = device->GetResourceManager();

  D3D12Descriptor descriptor = FindDescriptor(device, descType, slot, shaderType);

  if(descriptor.GetType() == D3D12DescriptorType::UAV && descType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
  {
    ResourceId uavId = descriptor.GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(uavId);
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = descriptor.GetUAV();

    if(uavDesc.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
      uavDesc = MakeUAVDesc(resDesc);

    switch(uavDesc.ViewDimension)
    {
      case D3D12_UAV_DIMENSION_BUFFER:
      {
        if(isDXIL)
        {
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] =
              result.value.u32v[3] = (uint32_t)uavDesc.Buffer.NumElements;
          break;
        }
      }
      case D3D12_UAV_DIMENSION_UNKNOWN:
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
          descType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
  {
    ResourceId srvId = descriptor.GetResResourceId();
    ID3D12Resource *pResource = rm->GetCurrentAs<ID3D12Resource>(srvId);
    D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = descriptor.GetSRV();
    if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_UNKNOWN)
      srvDesc = MakeSRVDesc(resDesc);
    switch(srvDesc.ViewDimension)
    {
      case D3D12_SRV_DIMENSION_BUFFER:
      {
        if(isDXIL)
        {
          result.value.u32v[0] = result.value.u32v[1] = result.value.u32v[2] =
              result.value.u32v[3] = (uint32_t)srvDesc.Buffer.NumElements;
          break;
        }
      }
      case D3D12_SRV_DIMENSION_UNKNOWN:
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
          result.value.u32v[3] = resDesc.SampleDesc.Count;
        }
        else if(srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
        {
          result.value.u32v[2] = srvDesc.Texture2DMSArray.ArraySize;
          result.value.u32v[3] = resDesc.SampleDesc.Count;

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
        // Even though it's a texture cube, an individual face's dimensions are returned
        dim = 2;

        bool isarray = srvDesc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;

        result.value.u32v[0] = RDCMAX(1U, (uint32_t)(resDesc.Width >> mipLevel));
        result.value.u32v[1] = RDCMAX(1U, (uint32_t)(resDesc.Height >> mipLevel));

        // the spec says
        // "If srcResource is a TextureCubeArray, [...]. dest.z is set to an undefined value."
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

ShaderVariable D3D12ShaderDebug::GetSampleInfo(WrappedID3D12Device *device,
                                               D3D12_DESCRIPTOR_RANGE_TYPE descType,
                                               const DXDebug::BindingSlot &slot,
                                               const DXBC::ShaderType shaderType,
                                               const char *opString)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);

  D3D12Descriptor descriptor = FindDescriptor(device, descType, slot, shaderType);

  if(descriptor.GetType() == D3D12DescriptorType::SRV && descType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
  {
    D3D12ResourceManager *rm = device->GetResourceManager();

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

ShaderVariable D3D12ShaderDebug::GetRenderTargetSampleInfo(WrappedID3D12Device *device,
                                                           const DXBC::ShaderType shaderType,
                                                           const char *opString)
{
  ShaderVariable result("", 0U, 0U, 0U, 0U);
  if(shaderType != DXBC::ShaderType::Compute)
  {
    D3D12ResourceManager *rm = device->GetResourceManager();
    const D3D12RenderState &rs = device->GetQueue()->GetCommandData()->m_RenderState;

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

DXGI_FORMAT D3D12ShaderDebug::GetUAVResourceFormat(const D3D12_UNORDERED_ACCESS_VIEW_DESC &uavDesc,
                                                   ID3D12Resource *pResource)
{
  // Typed UAV (underlying resource is typeless)
  if(uavDesc.Format != DXGI_FORMAT_UNKNOWN)
    return uavDesc.Format;

  // Typeless UAV get format from the underlying resource
  D3D12_RESOURCE_DESC resDesc = pResource->GetDesc();
  return resDesc.Format;
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
                             DXDebug::SampleGatherResourceData resourceData,
                             DXDebug::SampleGatherSamplerData samplerData, const ShaderVariable &uv,
                             const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                             const int8_t texelOffsets[3], int multisampleIndex,
                             float lodOrCompareValue, const uint8_t swizzle[4],
                             DXDebug::GatherChannel gatherChannel, const char *opString,
                             ShaderVariable &output);

private:
  DXBC::ShaderType GetShaderType() { return m_dxbc ? m_dxbc->m_Type : DXBC::ShaderType::Pixel; }
  WrappedID3D12Device *m_pDevice;
  ID3D12GraphicsCommandListX *m_QueuedOpCmdList;
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
  m_QueuedOpCmdList = NULL;
}

D3D12DebugAPIWrapper::~D3D12DebugAPIWrapper()
{
  // if we replayed to before the action for fetching some UAVs, replay back to after the action to
  // keep the state consistent.
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

              // DXBC allows root buffers to have a stride of up to 16 bytes in the shader, which means
              // encoding the byte offset into the first element here is wrong without knowing what
              // the actual accessed stride is. Instead we only fetch the data from that offset onwards.

              // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
              // resource desc format or the DXBC reflection info might be more correct.
              DXBCDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, srvData.format);
              srvData.firstElement = 0;
              // root arguments have no bounds checking, so use the most conservative number of elements
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

            // For every range, check the number of descriptors so that we are accessing the correct
            // data for append descriptor tables, even if the range type doesn't match what we need to fetch
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

              // TODO: Should we look up the bind point in the D3D12 state to try to get a better
              // guess at the number of descriptors?
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

              // DXBC allows root buffers to have a stride of up to 16 bytes in the shader, which means
              // encoding the byte offset into the first element here is wrong without knowing what
              // the actual accessed stride is. Instead we only fetch the data from that offset onwards.

              // TODO: Root buffers can be 32-bit UINT/SINT/FLOAT. Using UINT for now, but the
              // resource desc format or the DXBC reflection info might be more correct.
              DXBCDebug::FillViewFmt(DXGI_FORMAT_R32_UINT, uavData.format);
              uavData.firstElement = 0;
              // root arguments have no bounds checking, so use the most conservative number of elements
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

                    uavDesc.Format = D3D12ShaderDebug::GetUAVResourceFormat(uavDesc, pResource);
                    DXBCDebug::FillViewFmt(uavDesc.Format, uavData.format);
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

// Used by the DXBC Debugger
bool D3D12DebugAPIWrapper::CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode,
                                                  const ShaderVariable &input,
                                                  ShaderVariable &output1, ShaderVariable &output2)
{
  int mathOp;
  switch(opcode)
  {
    case DXBCBytecode::OPCODE_RCP: mathOp = DEBUG_SAMPLE_MATH_DXBC_RCP; break;
    case DXBCBytecode::OPCODE_RSQ: mathOp = DEBUG_SAMPLE_MATH_DXBC_RSQ; break;
    case DXBCBytecode::OPCODE_EXP: mathOp = DEBUG_SAMPLE_MATH_DXBC_EXP; break;
    case DXBCBytecode::OPCODE_LOG: mathOp = DEBUG_SAMPLE_MATH_DXBC_LOG; break;
    case DXBCBytecode::OPCODE_SINCOS: mathOp = DEBUG_SAMPLE_MATH_DXBC_SINCOS; break;
    default:
      // To support a new instruction, the shader created in
      // D3D12DebugManager::CreateShaderDebugResources will need updating
      RDCERR("Unsupported instruction for CalculateMathIntrinsic: %u", opcode);
      return false;
  }

  RDCASSERT(!m_QueuedOpCmdList);
  m_QueuedOpCmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  const uint32_t queueIndex = 0;
  if(!D3D12ShaderDebug::QueueMathIntrinsic(false, m_pDevice, m_QueuedOpCmdList, mathOp, input,
                                           queueIndex))
  {
    HRESULT hr = m_QueuedOpCmdList->Close();
    if(FAILED(hr))
      RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());

    m_QueuedOpCmdList = NULL;
    return false;
  }

  rdcarray<ShaderVariable *> mathOpResults;
  mathOpResults.push_back(&output1);
  mathOpResults.push_back(&output2);
  rdcarray<ShaderVariable *> sampleGatherResults;
  rdcarray<int> sampleRetTypes;
  rdcarray<const uint8_t *> swizzles;

  const uint32_t countMathResultsPerGpuOp = 2;
  bool ret = D3D12ShaderDebug::GetQueuedResults(m_pDevice, m_QueuedOpCmdList, mathOpResults,
                                                countMathResultsPerGpuOp, sampleGatherResults,
                                                sampleRetTypes, swizzles);
  m_QueuedOpCmdList = NULL;
  return ret;
}

D3D12Descriptor D3D12DebugAPIWrapper::FindDescriptor(DXBCBytecode::OperandType type,
                                                     const DXBCDebug::BindingSlot &slot)
{
  D3D12_DESCRIPTOR_RANGE_TYPE descType = ConvertOperandTypeToDescriptorType(type);
  return D3D12ShaderDebug::FindDescriptor(m_pDevice, descType, slot, GetShaderType());
}

ShaderVariable D3D12DebugAPIWrapper::GetSampleInfo(DXBCBytecode::OperandType type,
                                                   bool isAbsoluteResource,
                                                   const DXBCDebug::BindingSlot &slot,
                                                   const char *opString)
{
  DXBC::ShaderType shaderType = GetShaderType();
  if(type == DXBCBytecode::TYPE_RASTERIZER)
    return D3D12ShaderDebug::GetRenderTargetSampleInfo(m_pDevice, shaderType, opString);

  D3D12_DESCRIPTOR_RANGE_TYPE descType = ConvertOperandTypeToDescriptorType(type);
  return D3D12ShaderDebug::GetSampleInfo(m_pDevice, descType, slot, shaderType, opString);
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
  D3D12_DESCRIPTOR_RANGE_TYPE descType = ConvertOperandTypeToDescriptorType(type);
  return D3D12ShaderDebug::GetResourceInfo(m_pDevice, descType, slot, mipLevel, GetShaderType(),
                                           dim, false);
}

// Used by the DXBC Debugger
bool D3D12DebugAPIWrapper::CalculateSampleGather(
    DXBCBytecode::OpcodeType opcode, DXDebug::SampleGatherResourceData resourceData,
    DXDebug::SampleGatherSamplerData samplerData, const ShaderVariable &uv,
    const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc, const int8_t texelOffsets[3],
    int multisampleIndex, float lodOrCompareValue, const uint8_t swizzle[4],
    DXDebug::GatherChannel gatherChannel, const char *opString, ShaderVariable &output)
{
  using namespace DXBCBytecode;

  int sampleOp;
  switch(opcode)
  {
    case OPCODE_SAMPLE: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE; break;
    case OPCODE_SAMPLE_L: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_LEVEL; break;
    case OPCODE_SAMPLE_B: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_BIAS; break;
    case OPCODE_SAMPLE_C: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP; break;
    case OPCODE_SAMPLE_D: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_GRAD; break;
    case OPCODE_SAMPLE_C_LZ: sampleOp = DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO; break;
    case OPCODE_GATHER4: sampleOp = DEBUG_SAMPLE_TEX_GATHER4; break;
    case OPCODE_GATHER4_C: sampleOp = DEBUG_SAMPLE_TEX_GATHER4_CMP; break;
    case OPCODE_GATHER4_PO: sampleOp = DEBUG_SAMPLE_TEX_GATHER4_PO; break;
    case OPCODE_GATHER4_PO_C: sampleOp = DEBUG_SAMPLE_TEX_GATHER4_PO_CMP; break;
    case OPCODE_LOD: sampleOp = DEBUG_SAMPLE_TEX_LOD; break;
    case OPCODE_LD: sampleOp = DEBUG_SAMPLE_TEX_LOAD; break;
    case OPCODE_LD_MS: sampleOp = DEBUG_SAMPLE_TEX_LOAD_MS; break;
    default:
      // To support a new instruction, the shader created in
      // D3D12DebugManager::CreateShaderDebugResources will need updating
      RDCERR("Unsupported instruction for CalculateSampleGather: %u", opcode);
      return false;
  }

  RDCASSERT(!m_QueuedOpCmdList);
  m_QueuedOpCmdList = m_pDevice->GetDebugManager()->ResetDebugList();
  int sampleRetType = 0;
  const uint32_t queueIndex = 0;
  if(!D3D12ShaderDebug::QueueSampleGather(
         false, m_pDevice, m_QueuedOpCmdList, sampleOp, resourceData, samplerData, uv, ddxCalc,
         ddyCalc, texelOffsets, multisampleIndex, lodOrCompareValue, lodOrCompareValue, swizzle,
         gatherChannel, GetShaderType(), m_instruction, opString, queueIndex, sampleRetType))
  {
    HRESULT hr = m_QueuedOpCmdList->Close();
    if(FAILED(hr))
      RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());

    m_QueuedOpCmdList = NULL;
    set0001(output);
    return true;
  }

  rdcarray<ShaderVariable *> mathOpResults;
  rdcarray<ShaderVariable *> sampleGatherResults;
  sampleGatherResults.push_back(&output);
  rdcarray<int> sampleRetTypes;
  sampleRetTypes.push_back(sampleRetType);
  rdcarray<const uint8_t *> swizzles;
  swizzles.push_back(swizzle);

  bool ret = D3D12ShaderDebug::GetQueuedResults(m_pDevice, m_QueuedOpCmdList, mathOpResults, 0,
                                                sampleGatherResults, sampleRetTypes, swizzles);
  m_QueuedOpCmdList = NULL;

  return ret;
}

void GatherConstantBuffers(WrappedID3D12Device *pDevice, const DXBCBytecode::Program &program,
                           const D3D12RenderState::RootSignature &rootsig,
                           const ShaderReflection &refl, DXBCDebug::GlobalState &global,
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
        AddCBufferToGlobalState(program, global, sourceVars, refl, slot, cbufData);
      }
      else if(rootSigParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV && element.type == eRootCBV)
      {
        DXBCDebug::BindingSlot slot(rootSigParam.Descriptor.ShaderRegister,
                                    rootSigParam.Descriptor.RegisterSpace);
        ID3D12Resource *cbv = pDevice->GetResourceManager()->GetCurrentAs<ID3D12Resource>(element.id);
        bytebuf cbufData;
        pDevice->GetDebugManager()->GetBufferData(cbv, element.offset, 0, cbufData);
        AddCBufferToGlobalState(program, global, sourceVars, refl, slot, cbufData);
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
            AddCBufferToGlobalState(program, global, sourceVars, refl, slot, cbufData);

            desc++;
          }
        }
      }
    }
  }
}

ID3DBlob *D3D12Replay::CompileShaderDebugFetcher(const DXBC::DXBCContainer *dxbc, const rdcstr &hlsl)
{
  ID3DBlob *psBlob = NULL;

  UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
  if(dxbc->GetDXBCByteCode())
  {
    if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "ExtractInputs", flags, {},
                                                  "ps_5_1", &psBlob) != "")
    {
      RDCERR("Failed to create shader to extract inputs");
      SAFE_RELEASE(psBlob);
    }
  }
  else
  {
    // get the profile and shader compile flags from the vertex shader
    const uint32_t smMajor = dxbc->m_Version.Major;
    const uint32_t smMinor = dxbc->m_Version.Minor;
    if(smMajor < 6)
    {
      RDCERR("Invalid vertex shader SM %d.%d expect SM6.0+", smMajor, smMinor);
      return NULL;
    }

    char stage = 'p';
    if(dxbc->m_Type == DXBC::ShaderType::Vertex)
      stage = 'v';
    else if(dxbc->m_Type == DXBC::ShaderType::Compute)
      stage = 'c';

    const rdcstr profile = StringFormat::Fmt("%cs_%u_%u", stage, smMajor, smMinor);

    ShaderCompileFlags compileFlags =
        DXBC::EncodeFlags(m_pDevice->GetShaderCache()->GetCompileFlags(), profile);

    const DXBC::GlobalShaderFlags shaderFlags = dxbc->GetGlobalShaderFlags();
    if(shaderFlags & DXBC::GlobalShaderFlags::NativeLowPrecision)
      compileFlags.flags.push_back({"@compile_option", "-enable-16bit-types"});

    if(m_pDevice->GetShaderCache()->GetShaderBlob(hlsl.c_str(), "ExtractInputs", compileFlags, {},
                                                  profile.c_str(), &psBlob) != "")
    {
      RDCERR("Failed to create shader to extract inputs");
      SAFE_RELEASE(psBlob);
    }
  }

  return psBlob;
}

ID3D12Resource *D3D12Replay::CreateInputFetchBuffer(DXDebug::InputFetcher &fetcher,
                                                    uint64_t &laneDataOffset,
                                                    uint64_t &evalDataOffset)
{
  HRESULT hr = S_OK;

  // Create buffer to store initial values captured in pixel shader
  D3D12_RESOURCE_DESC rdesc;
  ZeroMemory(&rdesc, sizeof(D3D12_RESOURCE_DESC));
  rdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  rdesc.Width = fetcher.hitBufferStride * (DXDebug::maxPixelHits + 1);

  const uint32_t countLaneElements =
      (fetcher.laneDataBufferStride > 0) ? fetcher.numLanesPerHit * (DXDebug::maxPixelHits + 1) : 0;
  // if we have separate lane data, allocate that at the end
  if(fetcher.laneDataBufferStride > 0)
  {
    rdesc.Width = AlignToMultiple(rdesc.Width, (uint64_t)fetcher.laneDataBufferStride);
    laneDataOffset = rdesc.Width;
    rdesc.Width += fetcher.laneDataBufferStride * countLaneElements;
  }

  // Create storage for MSAA evaluations captured in pixel shader
  if(!fetcher.evalSampleCacheData.empty())
  {
    rdesc.Width = AlignUp16(rdesc.Width);
    evalDataOffset = rdesc.Width;
    rdesc.Width +=
        UINT(fetcher.evalSampleCacheData.size() * sizeof(Vec4f) * (DXDebug::maxPixelHits + 1));
  }

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

  ID3D12Resource *dataBuffer = NULL;
  D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  hr = m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &rdesc, resourceState,
                                          NULL, __uuidof(ID3D12Resource), (void **)&dataBuffer);
  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer for pixel shader debugging HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  // Create UAV of initial values buffer
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
  ZeroMemory(&uavDesc, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = DXDebug::maxPixelHits + 1;
  uavDesc.Buffer.StructureByteStride = fetcher.hitBufferStride;

  D3D12_CPU_DESCRIPTOR_HANDLE uav = m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV);
  m_pDevice->CreateUnorderedAccessView(dataBuffer, NULL, &uavDesc, uav);

  // create UAV of separate lane data, if needed
  if(fetcher.laneDataBufferStride)
  {
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = laneDataOffset / fetcher.laneDataBufferStride;
    uavDesc.Buffer.StructureByteStride = fetcher.laneDataBufferStride;
    uavDesc.Buffer.NumElements = countLaneElements;

    uav = m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_LANEDATA_UAV);
    m_pDevice->CreateUnorderedAccessView(dataBuffer, NULL, &uavDesc, uav);
  }

  // Create UAV of MSAA eval buffer
  if(evalDataOffset)
  {
    D3D12_CPU_DESCRIPTOR_HANDLE msaaUav =
        m_pDevice->GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV);
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    uavDesc.Buffer.FirstElement = evalDataOffset / sizeof(Vec4f);
    uavDesc.Buffer.NumElements =
        (DXDebug::maxPixelHits + 1) * (uint32_t)fetcher.evalSampleCacheData.size();
    uavDesc.Buffer.StructureByteStride = 0;
    m_pDevice->CreateUnorderedAccessView(dataBuffer, NULL, &uavDesc, msaaUav);
  }

  uavDesc.Format = DXGI_FORMAT_R32_UINT;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = UINT(dataBuffer->GetDesc().Width / sizeof(uint32_t));
  uavDesc.Buffer.StructureByteStride = 0;
  D3D12_CPU_DESCRIPTOR_HANDLE clearUav =
      m_pDevice->GetDebugManager()->GetUAVClearHandle(SHADER_DEBUG_UAV);
  m_pDevice->CreateUnorderedAccessView(dataBuffer, NULL, &uavDesc, clearUav);

  return dataBuffer;
}

ID3D12RootSignature *D3D12Replay::CreateInputFetchRootSig(bool compute, uint32_t &uavspace,
                                                          uint32_t &sigElem)
{
  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
          compute ? rs.compute.rootsig : rs.graphics.rootsig);

  // Need to be able to add a descriptor table with our UAV without hitting the 64 DWORD limit
  RDCASSERT(sig->sig.dwordLength < 64);

  D3D12RootSignature modsig = sig->sig;
  uavspace = GetFreeRegSpace(modsig, 0, D3D12DescriptorType::UAV, D3D12_SHADER_VISIBILITY_ALL);

  // Create the descriptor table for our UAV
  D3D12_DESCRIPTOR_RANGE1 descRange = {
      D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 1, uavspace, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0,
  };

  modsig.Parameters.push_back(D3D12RootSignatureParameter());
  D3D12RootSignatureParameter &param = modsig.Parameters.back();
  param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  param.DescriptorTable.NumDescriptorRanges = 1;
  param.DescriptorTable.pDescriptorRanges = &descRange;

  sigElem = modsig.Parameters.count() - 1;

  modsig.Flags &= ~(D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS);

  // Create the root signature for gathering initial pixel shader values
  bytebuf root = EncodeRootSig(m_pDevice->RootSigVersion(), modsig);

  ID3D12RootSignature *pRootSignature = NULL;
  HRESULT hr = m_pDevice->CreateRootSignature(
      0, root.data(), root.size(), __uuidof(ID3D12RootSignature), (void **)&pRootSignature);
  if(FAILED(hr))
  {
    RDCERR("Failed to create root signature for pixel shader debugging HRESULT: %s",
           ToStr(hr).c_str());
    return NULL;
  }

  return pRootSignature;
}

ShaderDebugTrace *D3D12Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                           uint32_t idx, uint32_t view)
{
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

  const DXBC::DXBCContainer *dxbc = vs->GetDXBC();
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

  vs->GetWriteableDXBC()->GetDisassembly(false);

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
  rdcarray<bytebuf> instData;
  instData.resize(MaxStepRate * D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
  bytebuf staticData[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

  // if we're fetching from the GPU anyway, don't grab any buffer data
  if(dxbc->GetThreadScope() & DXBC::ThreadScope::Subgroup)
    vertexbuffers.clear();

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

  ShaderDebugTrace *ret = NULL;
  if(dxbc->GetDXBCByteCode())
  {
    DXBCDebug::InterpretDebugger *interpreter = new DXBCDebug::InterpretDebugger;
    interpreter->eventId = eventId;
    ret = interpreter->BeginDebug(dxbc, refl, 0);
    DXBCDebug::GlobalState &global = interpreter->global;
    DXBCDebug::ThreadState &state = interpreter->activeLane();

    // Fetch constant buffer data from root signature
    GatherConstantBuffers(m_pDevice, *dxbc->GetDXBCByteCode(), rs.graphics, refl, global,
                          ret->sourceVars);

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
  }
  else if(dxbc->GetThreadScope() & DXBC::ThreadScope::Subgroup)
  {
    DXDebug::InputFetcherConfig cfg;
    DXDebug::InputFetcher fetcher;

    D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
    m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe)->Fill(pipeDesc);

    // Store a copy of the event's render state to restore later
    D3D12RenderState prevState = rs;

    uint32_t sigElem = 0;
    ID3D12RootSignature *pRootSignature = CreateInputFetchRootSig(false, cfg.uavspace, sigElem);

    if(pRootSignature == NULL)
      return new ShaderDebugTrace;

    rs.graphics.rootsig = GetResID(pRootSignature);

    uint32_t sv_vertid = vertid;

    if(action->flags & ActionFlags::Indexed)
      sv_vertid = idx - action->baseVertex;

    cfg.vert = sv_vertid;
    cfg.inst = instid;
    cfg.uavslot = 1;
    cfg.waveOps = m_pDevice->GetOpts1().WaveOps != FALSE;
    cfg.maxWaveSize = m_pDevice->GetOpts1().WaveLaneCountMax;
    cfg.fetchWorkgroup = 0;

    DXDebug::CreateInputFetcher(dxbc, NULL, cfg, fetcher);

    // Create pixel shader to get initial values from previous stage output
    ID3DBlob *vsBlob = CompileShaderDebugFetcher(dxbc, fetcher.hlsl);

    if(vsBlob == NULL)
      return new ShaderDebugTrace;

    uint64_t laneDataOffset = 0;
    uint64_t evalDataOffset = 0;
    ID3D12Resource *dataBuffer = CreateInputFetchBuffer(fetcher, laneDataOffset, evalDataOffset);

    if(dataBuffer == NULL)
      return new ShaderDebugTrace;

    // Add the descriptor for our UAV
    std::set<ResourceId> copiedHeaps;
    rdcarray<PortableHandle> debugHandles = {
        ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV)),
        ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV)),
        ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_LANEDATA_UAV)),
    };
    AddDebugDescriptorsToRenderState(m_pDevice, rs, false, debugHandles,
                                     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sigElem, copiedHeaps);

    pipeDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
    pipeDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    pipeDesc.SetRootSig(pRootSignature);

    // disable rasterizaion
    pipeDesc.PS = {};
    pipeDesc.DepthStencilState.DepthEnable = FALSE;
    pipeDesc.DepthStencilState.StencilEnable = FALSE;

    ID3D12PipelineState *initialPso = NULL;
    HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &initialPso);

    SAFE_RELEASE(vsBlob);

    if(FAILED(hr))
    {
      RDCERR("Failed to create PSO for compute shader debugging HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(dataBuffer);
      SAFE_RELEASE(pRootSignature);
      return new ShaderDebugTrace;
    }

    rs.pipe = GetResID(initialPso);

    ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();

    // clear our UAVs
    m_pDevice->GetDebugManager()->SetDescriptorHeaps(cmdList, true, false);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuUav = m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuUav =
        m_pDevice->GetDebugManager()->GetUAVClearHandle(SHADER_DEBUG_UAV);
    UINT zero[4] = {0, 0, 0, 0};
    cmdList->ClearUnorderedAccessViewUint(gpuUav, cpuUav, dataBuffer, zero, 0, NULL);

    rs.ApplyDescriptorHeaps(cmdList);

    // Execute the command to ensure that UAV clear and resource creation occur before replay
    hr = cmdList->Close();
    if(FAILED(hr))
    {
      RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
      SAFE_RELEASE(dataBuffer);
      SAFE_RELEASE(pRootSignature);
      SAFE_RELEASE(initialPso);
      return new ShaderDebugTrace;
    }

    {
      ID3D12CommandList *l = cmdList;
      m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
      m_pDevice->InternalQueueWaitForIdle();
      m_pDevice->GetDebugManager()->ResetDebugAlloc();
    }

    {
      D3D12MarkerRegion initState(m_pDevice->GetQueue()->GetReal(),
                                  "Replaying event for initial states");

      // Replay the event with our modified state
      m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);
    }

    // Restore D3D12 state to what the event uses
    rs = prevState;

    bytebuf initialData;
    m_pDevice->GetDebugManager()->GetBufferData(dataBuffer, 0, 0, initialData);

    // Replaying the event has finished, and the data has been copied out.
    // Free all the resources that were created.
    SAFE_RELEASE(pRootSignature);
    SAFE_RELEASE(dataBuffer);
    SAFE_RELEASE(initialPso);

    DXDebug::DebugHit *buf = (DXDebug::DebugHit *)initialData.data();

    D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                           StringFormat::Fmt("Got %u hits", buf[0].numHits));
    if(buf[0].numHits == 0)
    {
      RDCLOG("No hit for this event");
      return new ShaderDebugTrace;
    }

    if(buf[0].numHits > 1)
      RDCLOG("Unexpected number of vertex hits: %u!", buf[0].numHits);

    DXILDebug::D3D12APIWrapper *apiWrapper = new DXILDebug::D3D12APIWrapper(
        m_pDevice, dxbc->GetDXILByteCode(), refl, eventId, dxbc->GetReflection()->InputSig);

    const uint32_t numThreads = buf->subgroupSize;
    rdcarray<DXILDebug::ThreadProperties> workgroupProperties;

    rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> threadsBuiltins;
    rdcarray<ShaderVariable> threadsInputs;
    workgroupProperties.resize(numThreads);
    threadsBuiltins.resize(numThreads);
    threadsInputs.resize(numThreads);

    apiWrapper->SetSubgroupSize(buf->subgroupSize);

    const rdcarray<DXIL::EntryPointInterface::Signature> &dxilInputs =
        apiWrapper->GetDXILEntryPointInputs();

    for(uint32_t t = 0; t < numThreads; t++)
    {
      threadsInputs[t] = apiWrapper->GetInputPlaceholder();
      DXDebug::VSLaneData *lane = (DXDebug::VSLaneData *)(initialData.data() + laneDataOffset +
                                                          t * fetcher.laneDataBufferStride);
      rdcarray<ShaderVariable> &ins = threadsInputs[t].members;

      byte *data = (byte *)(lane + 1);

      if(lane->active)
        RDCASSERTEQUAL(lane->laneIndex, t);
      workgroupProperties[t][DXILDebug::ThreadProperty::Active] = lane->active;
      workgroupProperties[t][DXILDebug::ThreadProperty::SubgroupIdx] = t;

      rdcarray<DXILDebug::InputData> inputDatas;
      for(int i = 0; i < fetcher.inputs.count(); i++)
      {
        DXDebug::InputElement &inputElement = fetcher.inputs[i];
        int packedRegister = inputElement.reg;
        if(packedRegister >= 0)
        {
          int dxilInputIdx = -1;
          int dxilArrayIdx = 0;
          int packedElement = inputElement.elem;
          int row = packedRegister;
          // Find the DXIL Input index and element from that matches the register and element
          for(int j = 0; j < dxilInputs.count(); ++j)
          {
            const DXIL::EntryPointInterface::Signature &dxilParam = dxilInputs[j];
            if((dxilParam.startRow <= row) && (row < (int)(dxilParam.startRow + dxilParam.rows)) &&
               (dxilParam.startCol == packedElement))
            {
              dxilInputIdx = j;
              dxilArrayIdx = row - dxilParam.startRow;
              break;
            }
          }
          RDCASSERT(dxilInputIdx >= 0);
          RDCASSERT(dxilArrayIdx >= 0);

          inputDatas.emplace_back(dxilInputIdx, dxilArrayIdx, inputElement.numwords,
                                  inputElement.sysattribute, inputElement.included, data);
        }

        if(inputElement.included)
          data += inputElement.numwords * sizeof(uint32_t);
      }

      rdcflatmap<ShaderBuiltin, ShaderVariable> &threadBuiltins = threadsBuiltins[t];
      threadBuiltins[ShaderBuiltin::IndexInSubgroup] = ShaderVariable(rdcstr(), t, 0U, 0U, 0U);

      for(const DXILDebug::InputData &input : inputDatas)
      {
        int32_t *rawout = NULL;

        ShaderVariable &invar = ins[input.input];
        int outElement = 0;

        if(input.sysattribute == ShaderBuiltin::VertexIndex)
        {
          invar.value.u32v[outElement] = lane->vert;
        }
        else if(input.sysattribute == ShaderBuiltin::InstanceIndex)
        {
          invar.value.u32v[outElement] = lane->vert;
        }
        else
        {
          if(invar.rows <= 1)
            rawout = &invar.value.s32v[outElement];
          else
            rawout = &invar.members[input.array].value.s32v[outElement];

          memcpy(rawout, input.data, input.numwords * 4);
        }

        if(input.sysattribute != ShaderBuiltin::Undefined)
          threadBuiltins[input.sysattribute] = invar;
      }
    }

    apiWrapper->SetWorkgroupProperties(workgroupProperties);
    apiWrapper->SetThreadsInputs(threadsInputs);
    apiWrapper->SetThreadsBuiltins(threadsBuiltins);

    // Fetch constant buffer data from root signature
    apiWrapper->FetchConstantBufferData(rs.graphics);

    DXILDebug::Debugger *debugger = new DXILDebug::Debugger();
    ret = debugger->BeginDebug(apiWrapper, eventId, dxbc, refl, buf->laneIndex, buf->subgroupSize);

    apiWrapper->ResetReplay();
  }
  else
  {
    const uint32_t activeLaneIndex = 0;
    const uint32_t numThreads = 1;
    DXILDebug::D3D12APIWrapper *apiWrapper = new DXILDebug::D3D12APIWrapper(
        m_pDevice, dxbc->GetDXILByteCode(), refl, eventId, dxbc->GetReflection()->InputSig);

    rdcarray<DXILDebug::ThreadProperties> workgroupProperties;
    rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> threadsBuiltins;
    rdcarray<ShaderVariable> threadsInputs;
    workgroupProperties.resize(numThreads);
    threadsBuiltins.resize(numThreads);
    threadsInputs.resize(numThreads);
    threadsInputs[activeLaneIndex] = apiWrapper->GetInputPlaceholder();

    workgroupProperties[activeLaneIndex][DXILDebug::ThreadProperty::Active] = 1;

    rdcarray<ShaderVariable> &inputs = threadsInputs[activeLaneIndex].members;

    rdcflatmap<ShaderBuiltin, ShaderVariable> &threadBuiltins = threadsBuiltins[activeLaneIndex];

    // Fetch constant buffer data from root signature
    apiWrapper->FetchConstantBufferData(rs.graphics);

    // Set input values
    for(size_t i = 0; i < inputs.size(); i++)
    {
      const SigParameter &sigParam = dxbc->GetReflection()->InputSig[i];
      switch(sigParam.systemValue)
      {
        case ShaderBuiltin::Undefined:
        case ShaderBuiltin::Position:    // SV_Position seems to get promoted automatically,
                                         // but it's invalid for vertex input
        {
          const D3D12_INPUT_ELEMENT_DESC *el = NULL;
          rdcstr signame = strlower(sigParam.semanticName);

          for(size_t l = 0; l < inputlayout.size(); l++)
          {
            rdcstr layoutname = strlower(inputlayout[l].SemanticName);
            if(signame == layoutname && sigParam.semanticIndex == inputlayout[l].SemanticIndex)
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
          if(sigParam.compCount > fmt.compCount)
          {
            inputs[i].value.u32v[3] = 1;

            if(fmt.compType == CompType::Float)
              inputs[i].value.f32v[3] = 1.0f;
          }

          // interpret resource format types
          if(fmt.Special())
          {
            Vec3f *v3 = (Vec3f *)inputs[i].value.f32v.data();
            Vec4f *v4 = (Vec4f *)inputs[i].value.f32v.data();

            // only pull in all or nothing from these,
            // if there's only e.g. 3 bytes remaining don't read and unpack some of
            // a 4-byte resource format type
            size_t packedsize = 4;
            if(fmt.type == ResourceFormatType::R5G5B5A1 || fmt.type == ResourceFormatType::R5G6B5 ||
               fmt.type == ResourceFormatType::R4G4B4A4)
              packedsize = 2;

            if(srcData == NULL || packedsize > dataSize)
            {
              inputs[i].value.u32v[0] = inputs[i].value.u32v[1] = inputs[i].value.u32v[2] =
                  inputs[i].value.u32v[3] = 0;
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
                inputs[i].value.u32v[2] = (packed >> 0) & 0x3ff;
                inputs[i].value.u32v[1] = (packed >> 10) & 0x3ff;
                inputs[i].value.u32v[0] = (packed >> 20) & 0x3ff;
                inputs[i].value.u32v[3] = (packed >> 30) & 0x003;
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
                inputs[i].value.u32v[c] = 0;
                continue;
              }

              dataSize -= fmt.compByteWidth;

              if(fmt.compByteWidth == 1)
              {
                byte *src = srcData + c * fmt.compByteWidth;

                if(fmt.compType == CompType::UInt)
                  inputs[i].value.u32v[c] = *src;
                else if(fmt.compType == CompType::SInt)
                  inputs[i].value.s32v[c] = *((int8_t *)src);
                else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
                  inputs[i].value.f32v[c] = float(*src) / 255.0f;
                else if(fmt.compType == CompType::SNorm)
                {
                  signed char *schar = (signed char *)src;

                  // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
                  if(*schar == -128)
                    inputs[i].value.f32v[c] = -1.0f;
                  else
                    inputs[i].value.f32v[c] = float(*schar) / 127.0f;
                }
                else
                  RDCERR("Unexpected component type");
              }
              else if(fmt.compByteWidth == 2)
              {
                uint16_t *src = (uint16_t *)(srcData + c * fmt.compByteWidth);

                if(fmt.compType == CompType::Float)
                  inputs[i].value.f32v[c] = ConvertFromHalf(*src);
                else if(fmt.compType == CompType::UInt)
                  inputs[i].value.u32v[c] = *src;
                else if(fmt.compType == CompType::SInt)
                  inputs[i].value.s32v[c] = *((int16_t *)src);
                else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
                  inputs[i].value.f32v[c] = float(*src) / float(UINT16_MAX);
                else if(fmt.compType == CompType::SNorm)
                {
                  int16_t *sint = (int16_t *)src;

                  // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
                  if(*sint == -32768)
                    inputs[i].value.f32v[c] = -1.0f;
                  else
                    inputs[i].value.f32v[c] = float(*sint) / 32767.0f;
                }
                else
                  RDCERR("Unexpected component type");
              }
              else if(fmt.compByteWidth == 4)
              {
                uint32_t *src = (uint32_t *)(srcData + c * fmt.compByteWidth);

                if(fmt.compType == CompType::Float || fmt.compType == CompType::UInt ||
                   fmt.compType == CompType::SInt)
                  memcpy(&inputs[i].value.u32v[c], src, 4);
                else
                  RDCERR("Unexpected component type");
              }
            }

            if(fmt.BGRAOrder())
            {
              RDCASSERT(fmt.compCount == 4);
              std::swap(inputs[i].value.f32v[2], inputs[i].value.f32v[0]);
            }
          }
          break;
        }
        case ShaderBuiltin::VertexIndex:
        {
          uint32_t sv_vertid = vertid;

          if(action->flags & ActionFlags::Indexed)
            sv_vertid = idx - action->baseVertex;

          if(sigParam.varType == VarType::Float)
            inputs[i].value.f32v[0] = inputs[i].value.f32v[1] = inputs[i].value.f32v[2] =
                inputs[i].value.f32v[3] = (float)sv_vertid;
          else
            inputs[i].value.u32v[0] = inputs[i].value.u32v[1] = inputs[i].value.u32v[2] =
                inputs[i].value.u32v[3] = sv_vertid;
          break;
        }
        case ShaderBuiltin::InstanceIndex:
        {
          if(sigParam.varType == VarType::Float)
            inputs[i].value.f32v[0] = inputs[i].value.f32v[1] = inputs[i].value.f32v[2] =
                inputs[i].value.f32v[3] = (float)instid;
          else
            inputs[i].value.u32v[0] = inputs[i].value.u32v[1] = inputs[i].value.u32v[2] =
                inputs[i].value.u32v[3] = instid;
          break;
        }
        default: RDCERR("Unhandled system value semantic on VS input"); break;
      }

      if(sigParam.systemValue != ShaderBuiltin::Undefined)
      {
        threadBuiltins[sigParam.systemValue] = inputs[i];
      }
    }

    apiWrapper->SetWorkgroupProperties(workgroupProperties);
    apiWrapper->SetThreadsInputs(threadsInputs);
    apiWrapper->SetThreadsBuiltins(threadsBuiltins);

    DXILDebug::Debugger *debugger = new DXILDebug::Debugger();
    ret = debugger->BeginDebug(apiWrapper, eventId, dxbc, refl, activeLaneIndex, 1);

    apiWrapper->ResetReplay();
  }

  if(ret)
    dxbc->FillTraceLineInfo(*ret);
  return ret;
}

ShaderDebugTrace *D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y,
                                          const DebugPixelInputs &inputs)
{
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

  const DXBC::DXBCContainer *dxbc = ps->GetDXBC();
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

  ps->GetWriteableDXBC()->GetDisassembly(false);

  ShaderDebugTrace *ret = NULL;

  // Fetch the previous stage's disassembly, to match outputs to PS inputs
  const DXBC::DXBCContainer *prevDxbc = NULL;
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
  // Check for mesh shader next
  if(prevDxbc == NULL)
  {
    WrappedID3D12Shader *ms = (WrappedID3D12Shader *)pso->graphics->MS.pShaderBytecode;
    if(ms)
      prevDxbc = ms->GetDXBC();
  }
  // Check for vertex shader last
  if(prevDxbc == NULL)
  {
    WrappedID3D12Shader *vs = (WrappedID3D12Shader *)pso->graphics->VS.pShaderBytecode;
    if(vs)
      prevDxbc = vs->GetDXBC();
  }

  DXDebug::InputFetcherConfig cfg;
  DXDebug::InputFetcher fetcher;

  D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
  m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe)->Fill(pipeDesc);

  // Store a copy of the event's render state to restore later
  D3D12RenderState prevState = rs;

  uint32_t sigElem = 0;
  ID3D12RootSignature *pRootSignature = CreateInputFetchRootSig(false, cfg.uavspace, sigElem);

  if(pRootSignature == NULL)
    return new ShaderDebugTrace;

  rs.graphics.rootsig = GetResID(pRootSignature);

  cfg.x = x;
  cfg.y = y;
  cfg.uavslot = 1;
  cfg.waveOps = m_pDevice->GetOpts1().WaveOps != FALSE;
  cfg.maxWaveSize = 4;
  cfg.outputSampleCount = RDCMAX(1U, pipeDesc.SampleDesc.Count);
  cfg.fetchWorkgroup = 0;

  if(dxbc->GetThreadScope() & DXBC::ThreadScope::Subgroup)
    cfg.maxWaveSize = m_pDevice->GetOpts1().WaveLaneCountMax;

  DXDebug::CreateInputFetcher(dxbc, prevDxbc, cfg, fetcher);

  // Create pixel shader to get initial values from previous stage output
  ID3DBlob *psBlob = CompileShaderDebugFetcher(dxbc, fetcher.hlsl);

  if(psBlob == NULL)
    return new ShaderDebugTrace;

  uint64_t laneDataOffset = 0;
  uint64_t evalDataOffset = 0;
  ID3D12Resource *dataBuffer = CreateInputFetchBuffer(fetcher, laneDataOffset, evalDataOffset);

  if(dataBuffer == NULL)
    return new ShaderDebugTrace;

  // Add the descriptor for our UAV
  std::set<ResourceId> copiedHeaps;
  rdcarray<PortableHandle> debugHandles = {
      ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV)),
      ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV)),
      ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_LANEDATA_UAV)),
  };
  AddDebugDescriptorsToRenderState(m_pDevice, rs, false, debugHandles,
                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sigElem, copiedHeaps);

  // All PSO state is the same as the event's, except for the pixel shader and root signature
  pipeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
  pipeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
  pipeDesc.SetRootSig(pRootSignature);

  ID3D12PipelineState *initialPso = NULL;
  HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &initialPso);

  SAFE_RELEASE(psBlob);

  if(FAILED(hr))
  {
    RDCERR("Failed to create PSO for pixel shader debugging HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(dataBuffer);
    SAFE_RELEASE(pRootSignature);
    return new ShaderDebugTrace;
  }

  rs.pipe = GetResID(initialPso);

  // if we have a depth buffer bound and we are testing EQUAL grab the current depth value for our target sample
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
  D3D12_CPU_DESCRIPTOR_HANDLE cpuUav =
      m_pDevice->GetDebugManager()->GetUAVClearHandle(SHADER_DEBUG_UAV);
  UINT zero[4] = {0, 0, 0, 0};
  cmdList->ClearUnorderedAccessViewUint(gpuUav, cpuUav, dataBuffer, zero, 0, NULL);

  rs.ApplyDescriptorHeaps(cmdList);

  // Execute the command to ensure that UAV clear and resource creation occur before replay
  hr = cmdList->Close();
  if(FAILED(hr))
  {
    RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
    SAFE_RELEASE(dataBuffer);
    SAFE_RELEASE(pRootSignature);
    SAFE_RELEASE(initialPso);
    return new ShaderDebugTrace;
  }

  {
    ID3D12CommandList *l = cmdList;
    m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
    m_pDevice->InternalQueueWaitForIdle();
    m_pDevice->GetDebugManager()->ResetDebugAlloc();
  }

  {
    D3D12MarkerRegion initState(m_pDevice->GetQueue()->GetReal(),
                                "Replaying event for initial states");

    // Replay the event with our modified state
    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);
  }

  // Restore D3D12 state to what the event uses
  rs = prevState;

  bytebuf initialData;
  m_pDevice->GetDebugManager()->GetBufferData(dataBuffer, 0, 0, initialData);

  // Replaying the event has finished, and the data has been copied out.
  // Free all the resources that were created.
  SAFE_RELEASE(pRootSignature);
  SAFE_RELEASE(dataBuffer);
  SAFE_RELEASE(initialPso);

  DXDebug::DebugHit *buf = (DXDebug::DebugHit *)initialData.data();

  D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                         StringFormat::Fmt("Got %u hits", buf[0].numHits));
  if(buf[0].numHits == 0)
  {
    RDCLOG("No hit for this event");
    return new ShaderDebugTrace;
  }

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we check to see if
  // a specific primitive was requested (via primitive parameter not being set to ~0U). If it was,
  // debug that pixel, otherwise do a best-estimate of which fragment was the last to successfully
  // depth test and debug that, just by checking if the depth test is ordered and picking the final
  // fragment in the series

  // Get depth func and determine "winner" pixel
  DXDebug::DebugHit *pWinnerHit = NULL;
  float *evalSampleCache = (float *)(initialData.data() + evalDataOffset);
  size_t winnerIdx = 0;

  if(sample == ~0U)
    sample = 0;

  if(primitive != ~0U)
  {
    for(size_t i = 0; i < buf[0].numHits && i < DXDebug::maxPixelHits; i++)
    {
      DXDebug::DebugHit *pHit =
          (DXDebug::DebugHit *)(initialData.data() + i * fetcher.hitBufferStride);

      if(pHit->primitive == primitive && pHit->sample == sample)
      {
        pWinnerHit = pHit;
        winnerIdx = i;
      }
    }
  }

  if(pWinnerHit == NULL)
  {
    for(size_t i = 0; i < buf[0].numHits && i < DXDebug::maxPixelHits; i++)
    {
      DXDebug::DebugHit *pHit =
          (DXDebug::DebugHit *)(initialData.data() + i * fetcher.hitBufferStride);

      if(pWinnerHit == NULL)
      {
        // If we haven't picked a winner at all yet, use the first one
        pWinnerHit = pHit;
        winnerIdx = i;
      }
      else if(pHit->sample == sample)
      {
        // If this hit is for the sample we want, check whether it's a better pick
        if(pWinnerHit->sample != sample)
        {
          // The previously selected winner was for the wrong sample, use this one
          pWinnerHit = pHit;
          winnerIdx = i;
        }
        else if(depthFunc == D3D12_COMPARISON_FUNC_EQUAL && existingDepth >= 0.0f)
        {
          // for depth equal, check if this hit is closer than the winner, and if so use it.
          if(fabs(pHit->depth - existingDepth) < fabs(pWinnerHit->depth - existingDepth))
          {
            pWinnerHit = pHit;
            winnerIdx = i;
          }
        }
        else if(depthFunc == D3D12_COMPARISON_FUNC_ALWAYS ||
                depthFunc == D3D12_COMPARISON_FUNC_NEVER ||
                depthFunc == D3D12_COMPARISON_FUNC_NOT_EQUAL)
        {
          // For depth functions without a sensible comparison, use the last sample encountered
          pWinnerHit = pHit;
          winnerIdx = i;
        }
        else if((depthFunc == D3D12_COMPARISON_FUNC_LESS && pHit->depth < pWinnerHit->depth) ||
                (depthFunc == D3D12_COMPARISON_FUNC_LESS_EQUAL && pHit->depth <= pWinnerHit->depth) ||
                (depthFunc == D3D12_COMPARISON_FUNC_GREATER && pHit->depth > pWinnerHit->depth) ||
                (depthFunc == D3D12_COMPARISON_FUNC_GREATER_EQUAL && pHit->depth >= pWinnerHit->depth))
        {
          // For depth functions with an inequality, find the hit that "wins" the most
          pWinnerHit = pHit;
          winnerIdx = i;
        }
      }
    }
  }

  evalSampleCache = (float *)(initialData.data() + evalDataOffset +
                              fetcher.evalSampleCacheData.size() * sizeof(Vec4f) * 4 * winnerIdx);

  if(pWinnerHit == NULL)
  {
    RDCLOG("Couldn't find any pixels that passed depth test at target coordinates");
    return new ShaderDebugTrace;
  }

  DXDebug::DebugHit *hit = pWinnerHit;

  // ddx(SV_Position.x) MUST be 1.0
  if(hit->derivValid != 1.0f)
  {
    RDCERR("Derivatives invalid");
    delete ret;
    return new ShaderDebugTrace;
  }

  byte *data = (byte *)(hit + 1);

  // if we have separate lane data, fetch it here
  if(fetcher.laneDataBufferStride)
  {
    data = (initialData.data() + laneDataOffset +
            winnerIdx * fetcher.numLanesPerHit * fetcher.laneDataBufferStride);
  }

  if(dxbc->GetDXBCByteCode())
  {
    DXBCDebug::InterpretDebugger *interpreter = new DXBCDebug::InterpretDebugger;
    interpreter->eventId = eventId;
    ret = interpreter->BeginDebug(dxbc, refl, hit->laneIndex);
    DXBCDebug::GlobalState &global = interpreter->global;

    // Fetch constant buffer data from root signature
    GatherConstantBuffers(m_pDevice, *dxbc->GetDXBCByteCode(), rs.graphics, refl, global,
                          ret->sourceVars);

    global.sampleEvalRegisterMask = fetcher.sampleEvalRegisterMask;

    for(uint32_t q = 0; q < 4; q++)
    {
      DXDebug::PSLaneData *lane = (DXDebug::PSLaneData *)data;

      DXBCDebug::ThreadState &state = interpreter->workgroup[q];
      rdcarray<ShaderVariable> &ins = state.inputs;

      if(q != hit->quadLaneIndex)
        ins = interpreter->workgroup[hit->quadLaneIndex].inputs;

      state.semantics.coverage = lane->coverage;
      state.semantics.primID = lane->primitive;
      state.semantics.isFrontFace = lane->isFrontFace;

      if(!ins.empty() && ins.back().name == dxbc->GetDXBCByteCode()->GetRegisterName(
                                                DXBCBytecode::TYPE_INPUT_COVERAGE_MASK, 0))
        ins.back().value.u32v[0] = lane->coverage;

      if(lane->isHelper)
        state.SetHelper();

      data += sizeof(DXDebug::PSLaneData);

      for(size_t i = 0; i < fetcher.inputs.size(); i++)
      {
        if(fetcher.inputs[i].reg >= 0)
        {
          ShaderVariable &invar = ins[fetcher.inputs[i].reg];

          if(fetcher.inputs[i].sysattribute == ShaderBuiltin::PrimitiveIndex)
          {
            invar.value.u32v[fetcher.inputs[i].elem] = lane->primitive;
          }
          else if(fetcher.inputs[i].sysattribute == ShaderBuiltin::MSAASampleIndex)
          {
            invar.value.u32v[fetcher.inputs[i].elem] = lane->sample;
          }
          else if(fetcher.inputs[i].sysattribute == ShaderBuiltin::MSAACoverage)
          {
            invar.value.u32v[fetcher.inputs[i].elem] = lane->coverage;
          }
          else if(fetcher.inputs[i].sysattribute == ShaderBuiltin::IsFrontFace)
          {
            invar.value.u32v[fetcher.inputs[i].elem] = lane->isFrontFace ? ~0U : 0;
          }
          else
          {
            int32_t *rawout = &invar.value.s32v[fetcher.inputs[i].elem];

            memcpy(rawout, data, fetcher.inputs[i].numwords * 4);
          }
        }

        if(fetcher.inputs[i].included)
          data += fetcher.inputs[i].numwords * sizeof(uint32_t);
      }

      // fetch any inputs that were evaluated at sample granularity
      for(const DXDebug::SampleEvalCacheKey &key : fetcher.evalSampleCacheData)
      {
        // start with the basic input value
        ShaderVariable var = state.inputs[key.inputRegisterIndex];

        // copy over the value into the variable
        memcpy(var.value.f32v.data(), evalSampleCache, var.columns * sizeof(float));

        // store in the global cache for this thread
        DXDebug::SampleEvalCacheKey k = key;
        k.quadIndex = q;
        global.sampleEvalCache[k] = var;

        // advance past this data - always by float4 as that's the buffer stride
        evalSampleCache += 4;
      }
    }

    ret->inputs = interpreter->activeLane().inputs;
    ret->constantBlocks = global.constantBlocks;
  }
  else
  {
    uint32_t numThreads = hit->subgroupSize;
    DXILDebug::D3D12APIWrapper *apiWrapper = new DXILDebug::D3D12APIWrapper(
        m_pDevice, dxbc->GetDXILByteCode(), refl, eventId, dxbc->GetReflection()->InputSig);

    rdcarray<DXILDebug::ThreadProperties> workgroupProperties;
    rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> threadsBuiltins;
    rdcarray<ShaderVariable> threadsInputs;
    workgroupProperties.resize(numThreads);
    threadsBuiltins.resize(numThreads);
    threadsInputs.resize(numThreads);

    // Fetch constant buffer data from root signature
    apiWrapper->FetchConstantBufferData(rs.graphics);
    const rdcarray<DXIL::EntryPointInterface::Signature> &dxilInputs =
        apiWrapper->GetDXILEntryPointInputs();

    apiWrapper->SetSubgroupSize(hit->subgroupSize);
    for(uint32_t q = 0; q < hit->subgroupSize; q++)
    {
      threadsInputs[q] = apiWrapper->GetInputPlaceholder();
      DXDebug::PSLaneData *lane = (DXDebug::PSLaneData *)data;

      rdcarray<ShaderVariable> &ins = threadsInputs[q].members;
      rdcflatmap<ShaderBuiltin, ShaderVariable> &threadBuiltins = threadsBuiltins[q];

      workgroupProperties[q][DXILDebug::ThreadProperty::Active] = lane->active;
      workgroupProperties[q][DXILDebug::ThreadProperty::Helper] = lane->isHelper;
      workgroupProperties[q][DXILDebug::ThreadProperty::QuadLane] = lane->quadLane;
      workgroupProperties[q][DXILDebug::ThreadProperty::QuadId] = lane->quadId;
      workgroupProperties[q][DXILDebug::ThreadProperty::SubgroupIdx] = q;

      data += sizeof(DXDebug::PSLaneData);

      // TODO: SAMPLE EVALUTE MASK
      // globalState.sampleEvalRegisterMask = sampleEvalRegisterMask;

      // The initial values are packed into register and elements
      // DXIL Inputs are not packed and contain the register and element linkage
      rdcarray<DXILDebug::InputData> inputDatas;
      for(int i = 0; i < fetcher.inputs.count(); i++)
      {
        DXDebug::InputElement &inputElement = fetcher.inputs[i];
        int packedRegister = inputElement.reg;
        if(packedRegister >= 0)
        {
          int dxilInputIdx = -1;
          int dxilArrayIdx = 0;
          int packedElement = inputElement.elem;
          int row = packedRegister;
          // Find the DXIL Input index and element from that matches the register and element
          for(int j = 0; j < dxilInputs.count(); ++j)
          {
            const DXIL::EntryPointInterface::Signature &dxilParam = dxilInputs[j];
            if((dxilParam.startRow <= row) && (row < (int)(dxilParam.startRow + dxilParam.rows)) &&
               (dxilParam.startCol == packedElement))
            {
              dxilInputIdx = j;
              dxilArrayIdx = row - dxilParam.startRow;
              break;
            }
          }
          RDCASSERT(dxilInputIdx >= 0);
          RDCASSERT(dxilArrayIdx >= 0);

          inputDatas.emplace_back(dxilInputIdx, dxilArrayIdx, inputElement.numwords,
                                  inputElement.sysattribute, inputElement.included, data);
        }

        if(inputElement.included)
          data += inputElement.numwords * sizeof(uint32_t);
      }

      threadBuiltins[ShaderBuiltin::IndexInSubgroup] = ShaderVariable(rdcstr(), q, 0U, 0U, 0U);
      threadBuiltins[ShaderBuiltin::PrimitiveIndex] =
          ShaderVariable(rdcstr(), lane->primitive, 0U, 0U, 0U);
      threadBuiltins[ShaderBuiltin::MSAACoverage] =
          ShaderVariable(rdcstr(), lane->coverage, 0U, 0U, 0U);
      threadBuiltins[ShaderBuiltin::IsFrontFace] =
          ShaderVariable(rdcstr(), lane->isFrontFace, 0U, 0U, 0U);

      for(const DXILDebug::InputData &input : inputDatas)
      {
        int32_t *rawout = NULL;

        ShaderVariable &invar = ins[input.input];
        int outElement = 0;

        if(input.sysattribute == ShaderBuiltin::PrimitiveIndex)
        {
          invar.value.u32v[outElement] = lane->primitive;
        }
        else if(input.sysattribute == ShaderBuiltin::MSAASampleIndex)
        {
          invar.value.u32v[outElement] = lane->sample;
        }
        else if(input.sysattribute == ShaderBuiltin::MSAACoverage)
        {
          invar.value.u32v[outElement] = lane->coverage;
        }
        else if(input.sysattribute == ShaderBuiltin::IsFrontFace)
        {
          invar.value.u32v[outElement] = lane->isFrontFace ? ~0U : 0;
        }
        else
        {
          if(invar.rows == 0)
          {
            RDCASSERT(input.array < invar.members.count(), input.array, invar.members.count());
            rawout = &invar.members[input.array].value.s32v[outElement];
          }
          else
          {
            rawout = &invar.value.s32v[outElement];
          }

          memcpy(rawout, input.data, input.numwords * 4);
        }

        if(input.sysattribute != ShaderBuiltin::Undefined)
          threadBuiltins[input.sysattribute] = invar;
      }

      // TODO: UPDATE INPUTS FROM SAMPLE CACHE
#if 0
      for(const DXDebug::SampleEvalCacheKey &key : fetcher.evalSampleCacheData)
      {
        // start with the basic input value
        ShaderVariable var = activeState.m_Input.members[key.inputRegisterIndex];

        // copy over the value into the variable
        memcpy(var.value.f32v.data(), evalSampleCache, var.columns * sizeof(float));

        // store in the global cache for each quad. We'll apply derivatives below to adjust for each
        DXDebug::SampleEvalCacheKey k = key;
        for(int i = 0; i < 4; i++)
        {
          k.quadIndex = i;
          activeState.sampleEvalCache[k] = var;
        }

        // advance past this data - always by float4 as that's the buffer stride
        evalSampleCache += 4;
      }
#endif
    }

    apiWrapper->SetWorkgroupProperties(workgroupProperties);
    apiWrapper->SetThreadsInputs(threadsInputs);
    apiWrapper->SetThreadsBuiltins(threadsBuiltins);

    DXILDebug::Debugger *debugger = new DXILDebug::Debugger();
    ret = debugger->BeginDebug(apiWrapper, eventId, dxbc, refl, hit->laneIndex, hit->subgroupSize);

    apiWrapper->ResetReplay();
  }

  if(ret)
    dxbc->FillTraceLineInfo(*ret);
  return ret;
}

ShaderDebugTrace *D3D12Replay::DebugThread(uint32_t eventId,
                                           const rdcfixedarray<uint32_t, 3> &groupid,
                                           const rdcfixedarray<uint32_t, 3> &threadid)
{
  D3D12MarkerRegion simloop(
      m_pDevice->GetQueue()->GetReal(),
      StringFormat::Fmt("DebugThread @ %u: [%u, %u, %u] (%u, %u, %u)", eventId, groupid[0],
                        groupid[1], groupid[2], threadid[0], threadid[1], threadid[2]));

  const ActionDescription *action = m_pDevice->GetAction(eventId);
  if(!(action->flags & ActionFlags::Dispatch))
  {
    RDCERR("Can only debug a Dispatch action");
    return new ShaderDebugTrace();
  }

  D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  WrappedID3D12PipelineState *pso =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  WrappedID3D12Shader *cs =
      pso && pso->IsCompute() ? (WrappedID3D12Shader *)pso->compute->CS.pShaderBytecode : NULL;

  if(!cs)
  {
    RDCERR("Can't debug with no current compute shader");
    return new ShaderDebugTrace;
  }

  const DXBC::DXBCContainer *dxbc = cs->GetDXBC();
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

  cs->GetWriteableDXBC()->GetDisassembly(false);

  ShaderDebugTrace *ret = NULL;
  bool wholeWorkgroup = (dxbc->GetThreadScope() & DXBC::ThreadScope::Workgroup) ? true : false;
  if(dxbc->GetDXBCByteCode())
  {
    uint32_t activeIndex = 0;
    if(wholeWorkgroup)
    {
      activeIndex = threadid[0] + threadid[1] * refl.dispatchThreadsDimension[0] +
                    threadid[2] * refl.dispatchThreadsDimension[0] * refl.dispatchThreadsDimension[1];
    }

    DXBCDebug::InterpretDebugger *interpreter = new DXBCDebug::InterpretDebugger;
    interpreter->eventId = eventId;
    ret = interpreter->BeginDebug(dxbc, refl, activeIndex);
    DXBCDebug::GlobalState &global = interpreter->global;
    DXBCDebug::ThreadState &state = interpreter->activeLane();

    GatherConstantBuffers(m_pDevice, *dxbc->GetDXBCByteCode(), rs.compute, refl, global,
                          ret->sourceVars);

    for(int i = 0; i < 3; i++)
    {
      state.semantics.GroupID[i] = groupid[i];
      state.semantics.ThreadID[i] = threadid[i];
    }

    ret->constantBlocks = global.constantBlocks;

    // add fake inputs for semantics
    for(size_t i = 0; i < dxbc->GetDXBCByteCode()->GetNumDeclarations(); i++)
    {
      const DXBCBytecode::Declaration &decl = dxbc->GetDXBCByteCode()->GetDeclaration(i);

      if(decl.declaration == OPCODE_DCL_INPUT &&
         (decl.operand.type == TYPE_INPUT_THREAD_ID ||
          decl.operand.type == TYPE_INPUT_THREAD_GROUP_ID ||
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
  }
  else
  {
    // get ourselves in pristine state before this dispatch (without any side effects it may have had)
    m_pDevice->ReplayLog(0, eventId, eReplay_WithoutDraw);

    uint32_t threadDim[3] = {
        refl.dispatchThreadsDimension[0],
        refl.dispatchThreadsDimension[1],
        refl.dispatchThreadsDimension[2],
    };

    uint32_t subgroupSize = 1;
    uint32_t activeLaneIndex = ~0U;
    uint32_t numThreads = 1;

    DXILDebug::D3D12APIWrapper *apiWrapper = new DXILDebug::D3D12APIWrapper(
        m_pDevice, dxbc->GetDXILByteCode(), refl, eventId, dxbc->GetReflection()->InputSig);

    DXILDebug::BuiltinInputs globalBuiltins;
    rdcarray<DXILDebug::ThreadProperties> workgroupProperties;
    rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> threadsBuiltins;

    // hard case - with subgroups we want the actual layout so read that from the GPU
    if(dxbc->GetThreadScope() & DXBC::ThreadScope::Subgroup)
    {
      DXDebug::InputFetcherConfig cfg;
      DXDebug::InputFetcher fetcher;

      D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC pipeDesc;
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe)->Fill(
          pipeDesc);

      // Store a copy of the event's render state to restore later
      D3D12RenderState prevState = rs;

      uint32_t sigElem = 0;
      ID3D12RootSignature *pRootSignature = CreateInputFetchRootSig(true, cfg.uavspace, sigElem);

      if(pRootSignature == NULL)
        return new ShaderDebugTrace;

      rs.compute.rootsig = GetResID(pRootSignature);

      cfg.threadid = {
          groupid[0] * threadDim[0] + threadid[0],
          groupid[1] * threadDim[1] + threadid[1],
          groupid[2] * threadDim[2] + threadid[2],
      };
      cfg.uavslot = 1;
      cfg.waveOps = m_pDevice->GetOpts1().WaveOps != FALSE;
      cfg.maxWaveSize = m_pDevice->GetOpts1().WaveLaneCountMax;
      cfg.fetchWorkgroup = wholeWorkgroup ? 1 : 0;
      cfg.groupSize = wholeWorkgroup ? threadDim[0] * threadDim[1] * threadDim[2] : 0;
      cfg.groupid = groupid;

      DXDebug::CreateInputFetcher(dxbc, NULL, cfg, fetcher);

      // Create pixel shader to get initial values from previous stage output
      ID3DBlob *csBlob = CompileShaderDebugFetcher(dxbc, fetcher.hlsl);

      if(csBlob == NULL)
        return new ShaderDebugTrace;

      uint64_t laneDataOffset = 0;
      uint64_t evalDataOffset = 0;
      ID3D12Resource *dataBuffer = CreateInputFetchBuffer(fetcher, laneDataOffset, evalDataOffset);

      if(dataBuffer == NULL)
        return new ShaderDebugTrace;

      // Add the descriptor for our UAV
      std::set<ResourceId> copiedHeaps;
      rdcarray<PortableHandle> debugHandles = {
          ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_UAV)),
          ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_MSAA_UAV)),
          ToPortableHandle(GetDebugManager()->GetCPUHandle(SHADER_DEBUG_LANEDATA_UAV)),
      };
      AddDebugDescriptorsToRenderState(m_pDevice, rs, true, debugHandles,
                                       D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, sigElem, copiedHeaps);

      pipeDesc.CS.BytecodeLength = csBlob->GetBufferSize();
      pipeDesc.CS.pShaderBytecode = csBlob->GetBufferPointer();
      pipeDesc.SetRootSig(pRootSignature);

      ID3D12PipelineState *initialPso = NULL;
      HRESULT hr = m_pDevice->CreatePipeState(pipeDesc, &initialPso);

      SAFE_RELEASE(csBlob);

      if(FAILED(hr))
      {
        RDCERR("Failed to create PSO for compute shader debugging HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(dataBuffer);
        SAFE_RELEASE(pRootSignature);
        return new ShaderDebugTrace;
      }

      rs.pipe = GetResID(initialPso);

      ID3D12GraphicsCommandListX *cmdList = m_pDevice->GetDebugManager()->ResetDebugList();

      // clear our UAVs
      m_pDevice->GetDebugManager()->SetDescriptorHeaps(cmdList, true, false);
      D3D12_GPU_DESCRIPTOR_HANDLE gpuUav =
          m_pDevice->GetDebugManager()->GetGPUHandle(SHADER_DEBUG_UAV);
      D3D12_CPU_DESCRIPTOR_HANDLE cpuUav =
          m_pDevice->GetDebugManager()->GetUAVClearHandle(SHADER_DEBUG_UAV);
      UINT zero[4] = {0, 0, 0, 0};
      cmdList->ClearUnorderedAccessViewUint(gpuUav, cpuUav, dataBuffer, zero, 0, NULL);

      rs.ApplyDescriptorHeaps(cmdList);

      // Execute the command to ensure that UAV clear and resource creation occur before replay
      hr = cmdList->Close();
      if(FAILED(hr))
      {
        RDCERR("Failed to close command list HRESULT: %s", ToStr(hr).c_str());
        SAFE_RELEASE(dataBuffer);
        SAFE_RELEASE(pRootSignature);
        SAFE_RELEASE(initialPso);
        return new ShaderDebugTrace;
      }

      {
        ID3D12CommandList *l = cmdList;
        m_pDevice->GetQueue()->ExecuteCommandLists(1, &l);
        m_pDevice->InternalQueueWaitForIdle();
        m_pDevice->GetDebugManager()->ResetDebugAlloc();
      }

      {
        D3D12MarkerRegion initState(m_pDevice->GetQueue()->GetReal(),
                                    "Replaying event for initial states");

        // Replay the event with our modified state
        m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);
      }

      // Restore D3D12 state to what the event uses
      rs = prevState;

      bytebuf initialData;
      m_pDevice->GetDebugManager()->GetBufferData(dataBuffer, 0, 0, initialData);

      // Replaying the event has finished, and the data has been copied out.
      // Free all the resources that were created.
      SAFE_RELEASE(pRootSignature);
      SAFE_RELEASE(dataBuffer);
      SAFE_RELEASE(initialPso);

      DXDebug::DebugHit *buf = (DXDebug::DebugHit *)initialData.data();

      D3D12MarkerRegion::Set(m_pDevice->GetQueue()->GetReal(),
                             StringFormat::Fmt("Got %u hits", buf[0].numHits));
      if(buf[0].numHits == 0)
      {
        RDCLOG("No hit for this event");
        return new ShaderDebugTrace;
      }

      if(buf[0].numHits > 1)
        RDCLOG("Unexpected number of compute hits: %u!", buf[0].numHits);

      subgroupSize = buf->subgroupSize;
      numThreads = wholeWorkgroup ? threadDim[0] * threadDim[1] * threadDim[2] : subgroupSize;

      workgroupProperties.resize(numThreads);
      threadsBuiltins.resize(numThreads);

      // SV_GroupID
      globalBuiltins[ShaderBuiltin::GroupIndex] =
          ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);

      const uint32_t countData = wholeWorkgroup ? numThreads : subgroupSize;

      // Create a mappinig from LaneData index to simulation lane index
      // The simulation expects the lanes to be grouped by subgroup
      // The active subgroup is first
      // Within a subgroup the lanes are in increasing Subgroup Lane Index order

      struct SortedLaneData
      {
        rdcfixedarray<uint32_t, 3> threadid;
        uint32_t subgroupIndex;
        uint32_t laneIndex;
        uint32_t active;
        bool operator<(const SortedLaneData &o) const
        {
          // 1. subgroupIndex
          if(subgroupIndex != o.subgroupIndex)
            return subgroupIndex < o.subgroupIndex;
          // 2. laneIndex
          if(laneIndex != o.laneIndex)
            return laneIndex < o.laneIndex;
          // 3. active
          return active < o.active;
        }
      };
      rdcarray<SortedLaneData> laneDatas;
      laneDatas.resize(countData);
      rdcarray<uint32_t> subgroupCounts;
      subgroupCounts.resize(subgroupSize);
      uint32_t countActiveSubgroup = 0;
      for(uint32_t t = 0; t < countData; t++)
      {
        DXDebug::CSLaneData *value = (DXDebug::CSLaneData *)(initialData.data() + laneDataOffset +
                                                             t * fetcher.laneDataBufferStride);

        uint32_t subgroupIndex = ~0U;
        uint32_t laneIndex = value->laneIndex;
        if(value->activeSubgroup)
        {
          subgroupIndex = 0;
          countActiveSubgroup++;
        }
        else
        {
          subgroupIndex = ++subgroupCounts[laneIndex];
        }

        laneDatas[t].threadid =
            rdcfixedarray<uint32_t, 3>({value->threadid[0], value->threadid[1], value->threadid[2]});
        laneDatas[t].laneIndex = laneIndex;
        laneDatas[t].active = value->active;
        laneDatas[t].subgroupIndex = subgroupIndex;

        const uint32_t groupFlatIndex = value->threadid[2] * threadDim[0] * threadDim[1] +
                                        value->threadid[1] * threadDim[0] + value->threadid[0];
        if(value->active && wholeWorkgroup)
          RDCASSERTEQUAL(t, groupFlatIndex);
      }
      // Not a full subgroup : add padding lanes
      if(countActiveSubgroup < subgroupSize)
      {
        for(uint32_t i = 0; i < subgroupSize - countActiveSubgroup; i++)
        {
          SortedLaneData padLane = {};
          padLane.threadid = rdcfixedarray<uint32_t, 3>({0, 0, 0});
          padLane.laneIndex = i + countActiveSubgroup;
          padLane.active = 0;
          padLane.subgroupIndex = 0;
          laneDatas.push_back(padLane);
        }
      }
      // Sort by the following keys:
      // 1. subgroupIndex
      // 2. laneIndex
      // 3. active
      std::sort(laneDatas.begin(), laneDatas.end());

      uint32_t prevSubgroupIndex = 0;
      int32_t prevLaneIndex = -1;
      for(uint32_t lane = 0; lane < countData; lane++)
      {
        SortedLaneData &laneData = laneDatas[lane];

        // Validate the data is sorted correctly for the simulation
        // The active subgroup is first
        // Within a subgroup the lanes are in increasing Subgroup Lane Index order
        RDCASSERT(laneData.subgroupIndex >= prevSubgroupIndex);
        if(laneData.subgroupIndex == prevSubgroupIndex)
          RDCASSERT((int32_t)laneData.laneIndex > prevLaneIndex);

        prevSubgroupIndex = laneData.subgroupIndex;
        prevLaneIndex = laneData.laneIndex;

        if(laneData.active && !wholeWorkgroup)
        {
          RDCASSERTEQUAL(laneData.subgroupIndex, 0);
          RDCASSERTEQUAL(laneData.laneIndex, lane);
        }

        if(laneData.threadid == threadid)
        {
          RDCASSERTEQUAL(laneData.subgroupIndex, 0);
          activeLaneIndex = lane;
        }

        const uint32_t groupFlatIndex = laneData.threadid[2] * threadDim[0] * threadDim[1] +
                                        laneData.threadid[1] * threadDim[0] + laneData.threadid[0];

        workgroupProperties[lane][DXILDebug::ThreadProperty::Active] = laneData.active;
        workgroupProperties[lane][DXILDebug::ThreadProperty::SubgroupIdx] = laneData.laneIndex;
        // The simulation requires
        RDCASSERT(lane >= laneData.laneIndex);

        rdcflatmap<ShaderBuiltin, ShaderVariable> &threadBuiltins = threadsBuiltins[lane];
        threadBuiltins[ShaderBuiltin::DispatchThreadIndex] =
            ShaderVariable(rdcstr(), groupid[0] * threadDim[0] + laneData.threadid[0],
                           groupid[1] * threadDim[1] + laneData.threadid[1],
                           groupid[2] * threadDim[2] + laneData.threadid[2], 0U);
        threadBuiltins[ShaderBuiltin::GroupThreadIndex] = ShaderVariable(
            rdcstr(), laneData.threadid[0], laneData.threadid[1], laneData.threadid[2], 0U);
        threadBuiltins[ShaderBuiltin::GroupFlatIndex] =
            ShaderVariable(rdcstr(), groupFlatIndex, 0U, 0U, 0U);
        threadBuiltins[ShaderBuiltin::IndexInSubgroup] =
            ShaderVariable(rdcstr(), laneData.laneIndex, 0U, 0U, 0U);
      }

      if(activeLaneIndex == ~0U)
      {
        RDCERR("Didn't find desired lane in subgroup data");
        activeLaneIndex = 0;
      }
    }
    else if(wholeWorkgroup)
    {
      numThreads = threadDim[0] * threadDim[1] * threadDim[2];

      workgroupProperties.resize(numThreads);
      threadsBuiltins.resize(numThreads);

      // SV_GroupID
      globalBuiltins[ShaderBuiltin::GroupIndex] =
          ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);

      // if we have workgroup scope that means we need to simulate the whole workgroup but don't
      // have subgroup ops. We assume the layout of this is irrelevant and don't attempt to read it
      // back from the GPU like we do with subgroups. We lay things out in plain linear order, along
      // X and then Y and then Z, with groups iterated together.

      uint32_t i = 0;
      for(uint32_t tz = 0; tz < threadDim[2]; tz++)
      {
        for(uint32_t ty = 0; ty < threadDim[1]; ty++)
        {
          for(uint32_t tx = 0; tx < threadDim[0]; tx++)
          {
            rdcflatmap<ShaderBuiltin, ShaderVariable> &thread_builtins = threadsBuiltins[i];
            thread_builtins[ShaderBuiltin::DispatchThreadIndex] =
                ShaderVariable(rdcstr(), groupid[0] * threadDim[0] + tx,
                               groupid[1] * threadDim[1] + ty, groupid[2] * threadDim[2] + tz, 0U);
            thread_builtins[ShaderBuiltin::GroupThreadIndex] =
                ShaderVariable(rdcstr(), tx, ty, tz, 0U);
            thread_builtins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
                rdcstr(), tz * threadDim[0] * threadDim[1] + ty * threadDim[0] + tx, 0U, 0U, 0U);
            workgroupProperties[i][DXILDebug::ThreadProperty::Active] = 1;

            if(rdcfixedarray<uint32_t, 3>({tx, ty, tz}) == threadid)
              activeLaneIndex = i;

            i++;
          }
        }
      }
    }
    else
    {
      workgroupProperties.resize(numThreads);
      threadsBuiltins.resize(numThreads);

      activeLaneIndex = 0;
      workgroupProperties[0][DXILDebug::ThreadProperty::Active] = 1;

      // put everything in globals, no per-thread values

      // SV_GroupID
      globalBuiltins[ShaderBuiltin::GroupIndex] =
          ShaderVariable(rdcstr(), groupid[0], groupid[1], groupid[2], 0U);

      // SV_DispatchThreadID
      globalBuiltins[ShaderBuiltin::DispatchThreadIndex] = ShaderVariable(
          rdcstr(), groupid[0] * threadDim[0] + threadid[0],
          groupid[1] * threadDim[1] + threadid[1], groupid[2] * threadDim[2] + threadid[2], 0U);

      // SV_GroupThreadID
      globalBuiltins[ShaderBuiltin::GroupThreadIndex] =
          ShaderVariable(rdcstr(), threadid[0], threadid[1], threadid[2], 0U);

      // SV_GroupIndex
      globalBuiltins[ShaderBuiltin::GroupFlatIndex] = ShaderVariable(
          rdcstr(),
          threadid[2] * threadDim[0] * threadDim[1] + threadid[1] * threadDim[0] + threadid[0], 0U,
          0U, 0U);
    }

    // Add inactive padding lanes to round up to the subgroup size
    const uint32_t numPaddingThreads = AlignUp(numThreads, subgroupSize) - numThreads;
    if(numPaddingThreads > 0)
    {
      uint32_t newNumThreads = numThreads + numPaddingThreads;
      workgroupProperties.resize(newNumThreads);
      threadsBuiltins.resize(newNumThreads);
      for(uint32_t i = numThreads; i < newNumThreads; ++i)
      {
        workgroupProperties[i][DXILDebug::ThreadProperty::Active] = 0;
        workgroupProperties[i][DXILDebug::ThreadProperty::SubgroupIdx] = i % subgroupSize;
      }
      numThreads = newNumThreads;
    }
    rdcarray<ShaderVariable> threadsInputs;
    threadsInputs.resize(numThreads);
    for(uint32_t t = 0; t < numThreads; t++)
      threadsInputs[t] = apiWrapper->GetInputPlaceholder();

    apiWrapper->SetSubgroupSize(subgroupSize);
    apiWrapper->SetWorkgroupProperties(workgroupProperties);
    apiWrapper->SetThreadsInputs(threadsInputs);
    apiWrapper->SetThreadsBuiltins(threadsBuiltins);
    apiWrapper->SetBuiltins(globalBuiltins);

    // Fetch constant buffer data from root signature
    apiWrapper->FetchConstantBufferData(rs.compute);

    DXILDebug::Debugger *debugger = new DXILDebug::Debugger();
    ret = debugger->BeginDebug(apiWrapper, eventId, dxbc, refl, activeLaneIndex, numThreads);

    apiWrapper->ResetReplay();
  }

  if(ret)
    dxbc->FillTraceLineInfo(*ret);
  return ret;
}

ShaderDebugTrace *D3D12Replay::DebugMeshThread(uint32_t eventId,
                                               const rdcfixedarray<uint32_t, 3> &groupid,
                                               const rdcfixedarray<uint32_t, 3> &threadid)
{
  // Not implemented yet
  return new ShaderDebugTrace;
}

rdcarray<ShaderDebugState> D3D12Replay::ContinueDebug(ShaderDebugger *debugger)
{
  if(!debugger)
    return {};

  if(((DXBCContainerDebugger *)debugger)->isDXIL)
  {
    DXILDebug::Debugger *dxilDebugger = (DXILDebug::Debugger *)debugger;
    D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "ContinueDebug Simulation Loop");
    rdcarray<ShaderDebugState> ret = dxilDebugger->ContinueDebug();
    DXILDebug::D3D12APIWrapper *api = (DXILDebug::D3D12APIWrapper *)dxilDebugger->GetAPIWrapper();
    api->ResetReplay();
    return ret;
  }
  else
  {
    DXBCDebug::InterpretDebugger *interpreter = (DXBCDebug::InterpretDebugger *)debugger;

    D3D12DebugAPIWrapper apiWrapper(m_pDevice, interpreter->dxbc, interpreter->global,
                                    interpreter->eventId);

    D3D12MarkerRegion region(m_pDevice->GetQueue()->GetReal(), "ContinueDebug Simulation Loop");

    return interpreter->ContinueDebug(&apiWrapper);
  }
}

void D3D12Replay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
}
