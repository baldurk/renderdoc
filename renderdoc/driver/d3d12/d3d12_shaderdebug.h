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

#include "driver/shaders/dxbc/dxbc_common.h"
#include "driver/shaders/dxbc/dxbcdxil_debug.h"
#include "d3d12_manager.h"

namespace D3D12ShaderDebug
{
using namespace DXBCDXILDebug;

typedef DXBCDXILDebug::SampleGatherResourceData SampleGatherResourceData;
typedef DXBCDXILDebug::SampleGatherSamplerData SampleGatherSamplerData;
typedef DXBCDXILDebug::BindingSlot BindingSlot;
typedef DXBCDXILDebug::GatherChannel GatherChannel;
typedef DXBCBytecode::SamplerMode SamplerMode;

// Helpers used by DXBC and DXIL debuggers to interact with GPU and resources
bool CalculateMathIntrinsic(bool dxil, WrappedID3D12Device *device, int mathOp,
                            const ShaderVariable &input, ShaderVariable &output1,
                            ShaderVariable &output2);

bool CalculateSampleGather(bool dxil, WrappedID3D12Device *device, int sampleOp,
                           SampleGatherResourceData resourceData, SampleGatherSamplerData samplerData,
                           const ShaderVariable &uv, const ShaderVariable &ddxCalc,
                           const ShaderVariable &ddyCalc, const int8_t texelOffsets[3],
                           int multisampleIndex, float lodOrCompareValue, const uint8_t swizzle[4],
                           GatherChannel gatherChannel, const DXBC::ShaderType shaderType,
                           uint32_t instruction, const char *opString, ShaderVariable &output);

D3D12Descriptor FindDescriptor(WrappedID3D12Device *device, D3D12_DESCRIPTOR_RANGE_TYPE descRangeType,
                               const DXBCDXILDebug::BindingSlot &slot,
                               const DXBC::ShaderType shaderType);
};
