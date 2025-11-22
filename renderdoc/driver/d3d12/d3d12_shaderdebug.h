/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2025 Baldur Karlsson
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

#include "driver/shaders/dxbc/dx_debug.h"
#include "driver/shaders/dxbc/dxbc_common.h"
#include "d3d12_manager.h"

namespace D3D12ShaderDebug
{
using namespace DXDebug;

typedef DXDebug::SampleGatherResourceData SampleGatherResourceData;
typedef DXDebug::SampleGatherSamplerData SampleGatherSamplerData;
typedef DXDebug::BindingSlot BindingSlot;
typedef DXDebug::GatherChannel GatherChannel;
typedef DXBCBytecode::SamplerMode SamplerMode;

// Helpers used by DXBC and DXIL debuggers to interact with GPU and resources
bool QueueMathIntrinsic(bool dxil, WrappedID3D12Device *device, ID3D12GraphicsCommandListX *cmdList,
                        int mathOp, const ShaderVariable &input, const uint32_t queueIndex);

bool QueueSampleGather(bool dxil, WrappedID3D12Device *device, ID3D12GraphicsCommandListX *cmdList,
                       int sampleOp, SampleGatherResourceData resourceData,
                       SampleGatherSamplerData samplerData, const ShaderVariable &uv,
                       const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                       const int8_t texelOffsets[3], int multisampleIndex, float lodValue,
                       float compareValue, const uint8_t swizzle[4], GatherChannel gatherChannel,
                       const DXBC::ShaderType shaderType, uint32_t instruction,
                       const char *opString, const uint32_t queueIndex, int &sampleRetType);
bool GetQueuedResults(WrappedID3D12Device *device, ID3D12GraphicsCommandListX *cmdList,
                      rdcarray<ShaderVariable *> &mathOpResults, uint32_t countMathResultsPerGpuOp,
                      rdcarray<ShaderVariable *> &sampleGatherResults,
                      const rdcarray<int> &sampleRetTypes, const rdcarray<const uint8_t *> &swizzles);

D3D12Descriptor FindDescriptor(WrappedID3D12Device *device,
                               const DXDebug::HeapDescriptorType heapType, uint32_t descriptorIndex);

D3D12Descriptor FindDescriptor(WrappedID3D12Device *device, D3D12_DESCRIPTOR_RANGE_TYPE descType,
                               const DXDebug::BindingSlot &slot, const DXBC::ShaderType shaderType);

ShaderVariable GetResourceInfo(WrappedID3D12Device *device, D3D12_DESCRIPTOR_RANGE_TYPE descType,
                               const DXDebug::BindingSlot &slot, uint32_t mipLevel,
                               const DXBC::ShaderType shaderType, int &dim, bool isDXIL);

ShaderVariable GetSampleInfo(WrappedID3D12Device *device, D3D12_DESCRIPTOR_RANGE_TYPE descType,
                             const DXDebug::BindingSlot &slot, const DXBC::ShaderType shaderType,
                             const char *opString);

ShaderVariable GetRenderTargetSampleInfo(WrappedID3D12Device *device,
                                         const DXBC::ShaderType shaderType, const char *opString);

DXGI_FORMAT GetUAVResourceFormat(const D3D12_UNORDERED_ACCESS_VIEW_DESC &uavDesc,
                                 ID3D12Resource *pResource);
};
