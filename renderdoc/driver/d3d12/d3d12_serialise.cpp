/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_list.h"
#include "d3d12_common.h"
#include "d3d12_resources.h"

// some helper enums with custom stringise to handle special cases
enum D3D12ResourceBarrierSubresource
{
  D3D12AllSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
};

DECLARE_REFLECTION_ENUM(D3D12ResourceBarrierSubresource);

template <>
rdcstr DoStringise(const D3D12ResourceBarrierSubresource &el)
{
  RDCCOMPILE_ASSERT(sizeof(D3D12ResourceBarrierSubresource) == sizeof(uint32_t),
                    "Enum isn't uint sized");

  if(el == D3D12AllSubresources)
    return "All Subresources";

  return ToStr(uint32_t(el));
}

enum D3D12ComponentMapping
{
};

DECLARE_REFLECTION_ENUM(D3D12ComponentMapping);

template <>
rdcstr DoStringise(const D3D12ComponentMapping &el)
{
  RDCCOMPILE_ASSERT(sizeof(D3D12ComponentMapping) == sizeof(uint32_t), "Enum isn't uint sized");

  // handle some common ones as literals to avoid allocations
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, 3))
    return "RGBA"_lit;
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, 4))
    return "RGB0"_lit;
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2, 5))
    return "RGB1"_lit;
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 4, 4, 4))
    return "R000"_lit;
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 4, 4, 5))
    return "R001"_lit;
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 5, 5, 5))
    return "R111"_lit;
  if(el == D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(3, 2, 1, 3))
    return "BGRA"_lit;

  rdcstr ret;

  // value should always be <= 5, see D3D12_SHADER_COMPONENT_MAPPING
  const char mapping[] = {'R', 'G', 'B', 'A', '0', '1', '?', '!'};

  uint32_t swizzle = (uint32_t)el;

  for(int i = 0; i < 4; i++)
    ret += mapping[D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(i, swizzle)];

  return ret;
}

// serialisation of object handles via IDs.
template <class SerialiserType, class Interface>
void DoSerialiseViaResourceId(SerialiserType &ser, Interface *&el)
{
  D3D12ResourceManager *rm = (D3D12ResourceManager *)ser.GetUserData();

  ResourceId id;

  if(ser.IsWriting())
    id = GetResID(el);
  if(ser.IsStructurising() && rm)
    id = rm->GetOriginalID(GetResID(el));

  DoSerialise(ser, id);

  if(ser.IsReading() && !ser.IsStructurising())
  {
    if(id != ResourceId() && rm && rm->HasLiveResource(id))
      el = rm->GetLiveAs<Interface>(id);
    else
      el = NULL;
  }
}

#undef SERIALISE_INTERFACE
#define SERIALISE_INTERFACE(iface)                  \
  template <class SerialiserType>                   \
  void DoSerialise(SerialiserType &ser, iface *&el) \
  {                                                 \
    DoSerialiseViaResourceId(ser, el);              \
  }                                                 \
  INSTANTIATE_SERIALISE_TYPE(iface *);

SERIALISE_D3D_INTERFACES();

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DESCRIPTOR_RANGE1 &el)
{
  SERIALISE_MEMBER(RangeType);
  SERIALISE_MEMBER(NumDescriptors);
  SERIALISE_MEMBER(BaseShaderRegister);
  SERIALISE_MEMBER(RegisterSpace);
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(OffsetInDescriptorsFromTableStart);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_ROOT_DESCRIPTOR_TABLE1 &el)
{
  SERIALISE_MEMBER(NumDescriptorRanges);
  SERIALISE_MEMBER_ARRAY(pDescriptorRanges, NumDescriptorRanges);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_ROOT_CONSTANTS &el)
{
  SERIALISE_MEMBER(ShaderRegister);
  SERIALISE_MEMBER(RegisterSpace);
  SERIALISE_MEMBER(Num32BitValues);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_ROOT_DESCRIPTOR1 &el)
{
  SERIALISE_MEMBER(ShaderRegister);
  SERIALISE_MEMBER(RegisterSpace);
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_STATIC_SAMPLER_DESC &el)
{
  SERIALISE_MEMBER(Filter);
  SERIALISE_MEMBER(AddressU);
  SERIALISE_MEMBER(AddressV);
  SERIALISE_MEMBER(AddressW);
  SERIALISE_MEMBER(MipLODBias);
  SERIALISE_MEMBER(MaxAnisotropy);
  SERIALISE_MEMBER(ComparisonFunc);
  SERIALISE_MEMBER(BorderColor);
  SERIALISE_MEMBER(MinLOD);
  SERIALISE_MEMBER(MaxLOD);
  SERIALISE_MEMBER(ShaderRegister);
  SERIALISE_MEMBER(RegisterSpace);
  SERIALISE_MEMBER(ShaderVisibility);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_STATIC_SAMPLER_DESC1 &el)
{
  SERIALISE_MEMBER(Filter);
  SERIALISE_MEMBER(AddressU);
  SERIALISE_MEMBER(AddressV);
  SERIALISE_MEMBER(AddressW);
  SERIALISE_MEMBER(MipLODBias);
  SERIALISE_MEMBER(MaxAnisotropy);
  SERIALISE_MEMBER(ComparisonFunc);
  SERIALISE_MEMBER(BorderColor);
  SERIALISE_MEMBER(MinLOD);
  SERIALISE_MEMBER(MaxLOD);
  SERIALISE_MEMBER(ShaderRegister);
  SERIALISE_MEMBER(RegisterSpace);
  SERIALISE_MEMBER(ShaderVisibility);
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12RootSignatureParameter &el)
{
  RDCASSERTMSG(
      "root signature parameter serialisation is only supported for structured serialisers",
      ser.IsStructurising());

  SERIALISE_MEMBER(ParameterType);
  switch(el.ParameterType)
  {
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
    {
      SERIALISE_MEMBER(DescriptorTable);
      break;
    }
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
    {
      SERIALISE_MEMBER(Constants);
      break;
    }
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
    case D3D12_ROOT_PARAMETER_TYPE_UAV:
    {
      SERIALISE_MEMBER(Descriptor);
      break;
    }
  }
  SERIALISE_MEMBER(ShaderVisibility);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12RootSignature &el)
{
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(Parameters);
  SERIALISE_MEMBER(StaticSamplers);
}

// similarly we serialise handles, buffer locations, through Id + indices

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, PortableHandle &el)
{
  SERIALISE_MEMBER(heap);
  SERIALISE_MEMBER(index);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_CPU_DESCRIPTOR_HANDLE &el)
{
  D3D12ResourceManager *rm = (D3D12ResourceManager *)ser.GetUserData();

  PortableHandle ph;

  if(ser.IsWriting() || ser.IsStructurising())
    ph = ToPortableHandle(el);
  if(ser.IsStructurising() && rm)
    ph.heap = rm->GetOriginalID(ph.heap);

  DoSerialise(ser, ph);

  if(ser.IsReading() && !ser.IsStructurising())
  {
    if(rm)
      el.ptr = (SIZE_T)DescriptorFromPortableHandle(rm, ph);
    else
      ser.ClearObj(el.ptr);
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_GPU_DESCRIPTOR_HANDLE &el)
{
  D3D12ResourceManager *rm = (D3D12ResourceManager *)ser.GetUserData();

  PortableHandle ph;

  if(ser.IsWriting() || ser.IsStructurising())
    ph = ToPortableHandle(el);
  if(ser.IsStructurising() && rm)
    ph.heap = rm->GetOriginalID(ph.heap);

  DoSerialise(ser, ph);

  if(ser.IsReading() && !ser.IsStructurising())
  {
    if(rm)
      el.ptr = (SIZE_T)DescriptorFromPortableHandle(rm, ph);
    else
      ser.ClearObj(el.ptr);
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, DynamicDescriptorCopy &el)
{
  D3D12ResourceManager *rm = (D3D12ResourceManager *)ser.GetUserData();

  SERIALISE_MEMBER(type);

  PortableHandle dst, src;

  if(ser.IsWriting() || ser.IsStructurising())
  {
    dst = ToPortableHandle(el.dst);
    src = ToPortableHandle(el.src);
  }
  if(ser.IsStructurising() && rm)
  {
    dst.heap = rm->GetOriginalID(dst.heap);
    src.heap = rm->GetOriginalID(src.heap);
  }

  ser.Serialise("dst"_lit, dst).Important();
  ser.Serialise("src"_lit, src).Important();

  if(ser.IsReading() && !ser.IsStructurising())
  {
    if(rm)
    {
      el.dst = DescriptorFromPortableHandle(rm, dst);
      el.src = DescriptorFromPortableHandle(rm, src);
    }
    else
    {
      el.dst = NULL;
      el.src = NULL;
    }
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12BufferLocation &el)
{
  D3D12ResourceManager *rm = (D3D12ResourceManager *)ser.GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(ser.IsWriting() || ser.IsStructurising())
    WrappedID3D12Resource::GetResIDFromAddrAllowOutOfBounds(el.Location, buffer, offs);
  if(ser.IsStructurising() && rm)
    buffer = rm->GetOriginalID(buffer);

  ser.Serialise("Buffer"_lit, buffer).Important();
  ser.Serialise("Offset"_lit, offs).OffsetOrSize();

  if(ser.IsReading() && !ser.IsStructurising())
  {
    if(rm && buffer != ResourceId() && rm->HasLiveResource(buffer))
      el.Location = rm->GetLiveAs<ID3D12Resource>(buffer)->GetGPUVirtualAddress() + offs;
    else
      ser.ClearObj(el.Location);
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12Descriptor &el)
{
  D3D12DescriptorType type = el.GetType();
  ser.Serialise("type"_lit, type);

  // SRV/RTV/DSV/UAV will make the resource important, others just display the type
  if(type == D3D12DescriptorType::Sampler)
    ser.Important();

  ID3D12DescriptorHeap *heap = (ID3D12DescriptorHeap *)el.data.samp.heap;

  ser.Serialise("heap"_lit, heap);
  ser.Serialise("index"_lit, el.data.samp.idx);

  if(ser.IsReading() && !ser.IsStructurising())
  {
    el.data.samp.heap = (WrappedID3D12DescriptorHeap *)heap;

    // for sampler types, this will be overwritten when serialising the sampler descriptor
    el.data.nonsamp.type = type;
  }

  // we serialise via a pointer. This means if the resource isn't present it becomes NULL and we set
  // the ResourceId to 0 on replay, and otherwise we get the live ID as we want. As a benefit, it's
  // also invisibly backwards compatible
  D3D12ResourceManager *rm = (D3D12ResourceManager *)ser.GetUserData();

  switch(type)
  {
    case D3D12DescriptorType::Sampler:
    {
      // special case because of squeezed descriptor
      D3D12_SAMPLER_DESC2 desc;
      if(ser.IsWriting() || ser.IsStructurising())
        desc = el.data.samp.desc.AsDesc();
      ser.Serialise("Descriptor"_lit, desc);
      if(ser.IsReading() && !ser.IsStructurising())
        el.data.samp.desc.Init(desc);
      RDCASSERTEQUAL(el.GetType(), D3D12DescriptorType::Sampler);
      break;
    }
    case D3D12DescriptorType::CBV:
    {
      ser.Serialise("Descriptor"_lit, el.data.nonsamp.cbv).Important();
      break;
    }
    case D3D12DescriptorType::SRV:
    {
      ResourceId Resource = el.data.nonsamp.resource;

      if(ser.IsStructurising())
        Resource = rm->GetOriginalID(Resource);

      ser.Serialise("Resource"_lit, Resource).TypedAs("ID3D12Resource *"_lit).Important();

      // convert to Live ID on replay
      if(ser.IsReading() && !ser.IsStructurising())
        el.data.nonsamp.resource =
            rm->HasLiveResource(Resource) ? rm->GetLiveID(Resource) : ResourceId();

      // special case because of squeezed descriptor
      D3D12_SHADER_RESOURCE_VIEW_DESC desc;
      if(ser.IsWriting() || ser.IsStructurising())
        desc = el.data.nonsamp.srv.AsDesc();
      ser.Serialise("Descriptor"_lit, desc);
      if(ser.IsReading() && !ser.IsStructurising())
        el.data.nonsamp.srv.Init(desc);
      break;
    }
    case D3D12DescriptorType::RTV:
    {
      ResourceId Resource = el.data.nonsamp.resource;

      if(ser.IsStructurising())
        Resource = rm->GetOriginalID(Resource);

      ser.Serialise("Resource"_lit, Resource).TypedAs("ID3D12Resource *"_lit).Important();

      // convert to Live ID on replay
      if(ser.IsReading() && !ser.IsStructurising())
        el.data.nonsamp.resource =
            rm->HasLiveResource(Resource) ? rm->GetLiveID(Resource) : ResourceId();

      ser.Serialise("Descriptor"_lit, el.data.nonsamp.rtv);
      break;
    }
    case D3D12DescriptorType::DSV:
    {
      ResourceId Resource = el.data.nonsamp.resource;

      if(ser.IsStructurising())
        Resource = rm->GetOriginalID(Resource);

      ser.Serialise("Resource"_lit, Resource).TypedAs("ID3D12Resource *"_lit).Important();

      // convert to Live ID on replay
      if(ser.IsReading() && !ser.IsStructurising())
        el.data.nonsamp.resource =
            rm->HasLiveResource(Resource) ? rm->GetLiveID(Resource) : ResourceId();

      ser.Serialise("Descriptor"_lit, el.data.nonsamp.dsv);
      break;
    }
    case D3D12DescriptorType::UAV:
    {
      ResourceId Resource = el.data.nonsamp.resource;
      ResourceId CounterResource = el.data.nonsamp.counterResource;

      if(ser.IsStructurising())
      {
        Resource = rm->GetOriginalID(Resource);
        CounterResource = rm->GetOriginalID(CounterResource);
      }

      ser.Serialise("Resource"_lit, Resource).TypedAs("ID3D12Resource *"_lit).Important();
      ser.Serialise("CounterResource"_lit, CounterResource).TypedAs("ID3D12Resource *"_lit);

      // convert to Live ID on replay
      if(ser.IsReading() && !ser.IsStructurising())
      {
        el.data.nonsamp.resource =
            rm->HasLiveResource(Resource) ? rm->GetLiveID(Resource) : ResourceId();
        el.data.nonsamp.counterResource =
            rm->HasLiveResource(CounterResource) ? rm->GetLiveID(CounterResource) : ResourceId();
      }

      // special case because of squeezed descriptor
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
      if(ser.IsWriting() || ser.IsStructurising())
        desc = el.data.nonsamp.uav.AsDesc();
      ser.Serialise("Descriptor"_lit, desc);
      if(ser.IsReading() && !ser.IsStructurising())
        el.data.nonsamp.uav.Init(desc);
      break;
    }
    case D3D12DescriptorType::Undefined:
    {
      el.data.nonsamp.type = type;
      break;
    }
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &el)
{
  SERIALISE_MEMBER(pRootSignature);
  SERIALISE_MEMBER(VS).Important();
  SERIALISE_MEMBER(PS).Important();
  SERIALISE_MEMBER(DS);
  SERIALISE_MEMBER(HS);
  SERIALISE_MEMBER(GS);

  if(ser.VersionAtLeast(0x11))
  {
    SERIALISE_MEMBER(AS);
    SERIALISE_MEMBER(MS);
  }

  SERIALISE_MEMBER(StreamOutput);
  SERIALISE_MEMBER(BlendState);
  SERIALISE_MEMBER(SampleMask);

  if(ser.VersionAtLeast(0x10))
  {
    SERIALISE_MEMBER(RasterizerState);
  }
  else
  {
    D3D12_RASTERIZER_DESC oldState;
    ser.Serialise("RasterizerState"_lit, oldState);
    el.RasterizerState = Upconvert(oldState);
  }

  if(ser.VersionAtLeast(0x10))
  {
    SERIALISE_MEMBER(DepthStencilState);
  }
  else
  {
    D3D12_DEPTH_STENCIL_DESC1 oldState;
    ser.Serialise("DepthStencilState"_lit, oldState);
    el.DepthStencilState = Upconvert(oldState);
  }

  SERIALISE_MEMBER(InputLayout);
  SERIALISE_MEMBER(IBStripCutValue);
  SERIALISE_MEMBER(PrimitiveTopologyType);
  SERIALISE_MEMBER(RTVFormats);
  SERIALISE_MEMBER(DSVFormat);
  SERIALISE_MEMBER(SampleDesc);
  SERIALISE_MEMBER(NodeMask);
  SERIALISE_MEMBER(CachedPSO);
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(ViewInstancing);
  SERIALISE_MEMBER(CS).Important();

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <>
void Deserialise(const D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC &el)
{
  delete[] el.ViewInstancing.pViewInstanceLocations;
  delete[] el.StreamOutput.pSODeclaration;
  delete[] el.StreamOutput.pBufferStrides;
  delete[] el.InputLayout.pInputElementDescs;
  FreeAlignedBuffer((byte *)(el.VS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.PS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.DS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.HS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.GS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.AS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.MS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.CS.pShaderBytecode));
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RESOURCE_DESC &el)
{
  SERIALISE_MEMBER(Dimension);
  SERIALISE_MEMBER(Alignment);
  SERIALISE_MEMBER(Width).Important();
  SERIALISE_MEMBER(Height);
  if(el.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
    ser.Important();
  SERIALISE_MEMBER(DepthOrArraySize);
  if(el.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
    ser.Important();
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(Format);
  if(el.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
    ser.Important();
  SERIALISE_MEMBER(SampleDesc);
  SERIALISE_MEMBER(Layout);
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_MIP_REGION &el)
{
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(Depth);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RESOURCE_DESC1 &el)
{
  SERIALISE_MEMBER(Dimension);
  SERIALISE_MEMBER(Alignment);
  SERIALISE_MEMBER(Width).Important();
  SERIALISE_MEMBER(Height).Important();
  SERIALISE_MEMBER(DepthOrArraySize).Important();
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(Format).Important();
  SERIALISE_MEMBER(SampleDesc);
  SERIALISE_MEMBER(Layout);
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(SamplerFeedbackMipRegion);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_COMMAND_QUEUE_DESC &el)
{
  SERIALISE_MEMBER(Type).Important();
  SERIALISE_MEMBER(Priority);
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(NodeMask);

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SHADER_BYTECODE &el)
{
  SERIALISE_MEMBER_ARRAY(pShaderBytecode, BytecodeLength).Important();

  // don't serialise size_t, otherwise capture/replay between different bit-ness won't work
  {
    uint64_t BytecodeLength = el.BytecodeLength;
    ser.Serialise("BytecodeLength"_lit, BytecodeLength);
    if(ser.IsReading())
      el.BytecodeLength = (size_t)BytecodeLength;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SO_DECLARATION_ENTRY &el)
{
  SERIALISE_MEMBER(Stream);
  SERIALISE_MEMBER(SemanticName);
  SERIALISE_MEMBER(SemanticIndex);
  SERIALISE_MEMBER(StartComponent);
  SERIALISE_MEMBER(ComponentCount);
  SERIALISE_MEMBER(OutputSlot);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_STREAM_OUTPUT_DESC &el)
{
  SERIALISE_MEMBER_ARRAY(pSODeclaration, NumEntries);
  SERIALISE_MEMBER(NumEntries);
  if(el.NumEntries > 0)
  {
    SERIALISE_MEMBER_ARRAY(pBufferStrides, NumStrides);
  }
  else
  {
    SERIALISE_MEMBER_ARRAY_EMPTY(pBufferStrides);
  }
  SERIALISE_MEMBER(NumStrides);
  SERIALISE_MEMBER(RasterizedStream);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_TARGET_BLEND_DESC &el)
{
  SERIALISE_MEMBER_TYPED(bool, BlendEnable);
  SERIALISE_MEMBER_TYPED(bool, LogicOpEnable);

  SERIALISE_MEMBER(SrcBlend);
  SERIALISE_MEMBER(DestBlend);
  SERIALISE_MEMBER(BlendOp);
  SERIALISE_MEMBER(SrcBlendAlpha);
  SERIALISE_MEMBER(DestBlendAlpha);
  SERIALISE_MEMBER(BlendOpAlpha);
  SERIALISE_MEMBER(LogicOp);
  SERIALISE_MEMBER_TYPED(D3D12_COLOR_WRITE_ENABLE, RenderTargetWriteMask);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BLEND_DESC &el)
{
  SERIALISE_MEMBER(AlphaToCoverageEnable);
  SERIALISE_MEMBER(IndependentBlendEnable);
  SERIALISE_MEMBER(RenderTarget);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RASTERIZER_DESC &el)
{
  SERIALISE_MEMBER(FillMode);
  SERIALISE_MEMBER(CullMode);
  SERIALISE_MEMBER(FrontCounterClockwise);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER(DepthClipEnable);
  SERIALISE_MEMBER(MultisampleEnable);
  SERIALISE_MEMBER(AntialiasedLineEnable);
  SERIALISE_MEMBER(ForcedSampleCount);
  SERIALISE_MEMBER(ConservativeRaster);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RASTERIZER_DESC1 &el)
{
  SERIALISE_MEMBER(FillMode);
  SERIALISE_MEMBER(CullMode);
  SERIALISE_MEMBER(FrontCounterClockwise);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER(DepthClipEnable);
  SERIALISE_MEMBER(MultisampleEnable);
  SERIALISE_MEMBER(AntialiasedLineEnable);
  SERIALISE_MEMBER(ForcedSampleCount);
  SERIALISE_MEMBER(ConservativeRaster);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RASTERIZER_DESC2 &el)
{
  SERIALISE_MEMBER(FillMode);
  SERIALISE_MEMBER(CullMode);
  SERIALISE_MEMBER(FrontCounterClockwise);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER(DepthClipEnable);
  SERIALISE_MEMBER(LineRasterizationMode);
  SERIALISE_MEMBER(ForcedSampleCount);
  SERIALISE_MEMBER(ConservativeRaster);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCILOP_DESC &el)
{
  SERIALISE_MEMBER(StencilFailOp);
  SERIALISE_MEMBER(StencilDepthFailOp);
  SERIALISE_MEMBER(StencilPassOp);
  SERIALISE_MEMBER(StencilFunc);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCIL_DESC &el)
{
  SERIALISE_MEMBER(DepthEnable);
  SERIALISE_MEMBER(DepthWriteMask);
  SERIALISE_MEMBER(DepthFunc).Important();
  SERIALISE_MEMBER(StencilEnable);
  SERIALISE_MEMBER(StencilReadMask);
  SERIALISE_MEMBER(StencilWriteMask);
  SERIALISE_MEMBER(FrontFace);
  SERIALISE_MEMBER(BackFace);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCIL_DESC1 &el)
{
  SERIALISE_MEMBER(DepthEnable);
  SERIALISE_MEMBER(DepthWriteMask);
  SERIALISE_MEMBER(DepthFunc).Important();
  SERIALISE_MEMBER(StencilEnable);
  SERIALISE_MEMBER(StencilReadMask);
  SERIALISE_MEMBER(StencilWriteMask);
  SERIALISE_MEMBER(FrontFace);
  SERIALISE_MEMBER(BackFace);
  SERIALISE_MEMBER(DepthBoundsTestEnable);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCILOP_DESC1 &el)
{
  SERIALISE_MEMBER(StencilFailOp);
  SERIALISE_MEMBER(StencilDepthFailOp);
  SERIALISE_MEMBER(StencilPassOp);
  SERIALISE_MEMBER(StencilFunc);
  SERIALISE_MEMBER(StencilReadMask);
  SERIALISE_MEMBER(StencilWriteMask);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCIL_DESC2 &el)
{
  SERIALISE_MEMBER(DepthEnable);
  SERIALISE_MEMBER(DepthWriteMask);
  SERIALISE_MEMBER(DepthFunc);
  SERIALISE_MEMBER(StencilEnable);
  SERIALISE_MEMBER(FrontFace);
  SERIALISE_MEMBER(BackFace);
  SERIALISE_MEMBER(DepthBoundsTestEnable);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_INPUT_ELEMENT_DESC &el)
{
  SERIALISE_MEMBER(SemanticName);
  SERIALISE_MEMBER(SemanticIndex);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(InputSlot);
  SERIALISE_MEMBER(AlignedByteOffset).OffsetOrSize();
  SERIALISE_MEMBER(InputSlotClass);
  SERIALISE_MEMBER(InstanceDataStepRate);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_INPUT_LAYOUT_DESC &el)
{
  SERIALISE_MEMBER_ARRAY(pInputElementDescs, NumElements);
  SERIALISE_MEMBER(NumElements);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_INDIRECT_ARGUMENT_DESC &el)
{
  SERIALISE_MEMBER(Type).Important();

  switch(el.Type)
  {
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
    case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
      // nothing to serialise
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
      SERIALISE_MEMBER(VertexBuffer.Slot);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
      SERIALISE_MEMBER(Constant.RootParameterIndex);
      SERIALISE_MEMBER(Constant.DestOffsetIn32BitValues);
      SERIALISE_MEMBER(Constant.Num32BitValuesToSet);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
      SERIALISE_MEMBER(ConstantBufferView.RootParameterIndex);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
      SERIALISE_MEMBER(ShaderResourceView.RootParameterIndex);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
      SERIALISE_MEMBER(UnorderedAccessView.RootParameterIndex);
      break;
    default: RDCERR("Unexpected indirect argument type: %u", el.Type); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_COMMAND_SIGNATURE_DESC &el)
{
  SERIALISE_MEMBER(ByteStride).OffsetOrSize();
  SERIALISE_MEMBER(NumArgumentDescs);
  SERIALISE_MEMBER_ARRAY(pArgumentDescs, NumArgumentDescs).Important();
  SERIALISE_MEMBER(NodeMask);

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <>
void Deserialise(const D3D12_COMMAND_SIGNATURE_DESC &el)
{
  delete[] el.pArgumentDescs;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_CACHED_PIPELINE_STATE &el)
{
  // don't serialise these, just set to NULL/0. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_ARRAY_EMPTY(pCachedBlob);
  uint64_t CachedBlobSizeInBytes = 0;
  ser.Serialise("CachedBlobSizeInBytes"_lit, CachedBlobSizeInBytes);

  if(ser.IsReading())
    el.CachedBlobSizeInBytes = (SIZE_T)CachedBlobSizeInBytes;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_GRAPHICS_PIPELINE_STATE_DESC &el)
{
  SERIALISE_MEMBER(pRootSignature);
  SERIALISE_MEMBER(VS).Important();
  SERIALISE_MEMBER(PS).Important();
  SERIALISE_MEMBER(DS);
  SERIALISE_MEMBER(HS);
  SERIALISE_MEMBER(GS);
  SERIALISE_MEMBER(StreamOutput);
  SERIALISE_MEMBER(BlendState);
  SERIALISE_MEMBER(SampleMask);
  SERIALISE_MEMBER(RasterizerState);
  SERIALISE_MEMBER(DepthStencilState);
  SERIALISE_MEMBER(InputLayout);
  SERIALISE_MEMBER(IBStripCutValue);
  SERIALISE_MEMBER(PrimitiveTopologyType);
  SERIALISE_MEMBER(NumRenderTargets);
  SERIALISE_MEMBER(RTVFormats);
  SERIALISE_MEMBER(DSVFormat);
  SERIALISE_MEMBER(SampleDesc);
  SERIALISE_MEMBER(NodeMask);
  SERIALISE_MEMBER(CachedPSO);
  SERIALISE_MEMBER(Flags);

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <>
void Deserialise(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &el)
{
  delete[] el.StreamOutput.pSODeclaration;
  delete[] el.StreamOutput.pBufferStrides;
  delete[] el.InputLayout.pInputElementDescs;
  FreeAlignedBuffer((byte *)(el.VS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.PS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.DS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.HS.pShaderBytecode));
  FreeAlignedBuffer((byte *)(el.GS.pShaderBytecode));
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_COMPUTE_PIPELINE_STATE_DESC &el)
{
  SERIALISE_MEMBER(pRootSignature);
  SERIALISE_MEMBER(CS).Important();
  SERIALISE_MEMBER(NodeMask);
  SERIALISE_MEMBER(CachedPSO);
  SERIALISE_MEMBER(Flags);

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <>
void Deserialise(const D3D12_COMPUTE_PIPELINE_STATE_DESC &el)
{
  FreeAlignedBuffer((byte *)(el.CS.pShaderBytecode));
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_VERTEX_BUFFER_VIEW &el)
{
  SERIALISE_MEMBER_TYPED(D3D12BufferLocation, BufferLocation).Important();
  SERIALISE_MEMBER(SizeInBytes).OffsetOrSize();
  SERIALISE_MEMBER(StrideInBytes).OffsetOrSize();
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_INDEX_BUFFER_VIEW &el)
{
  SERIALISE_MEMBER_TYPED(D3D12BufferLocation, BufferLocation).Important();
  SERIALISE_MEMBER(SizeInBytes).OffsetOrSize();
  SERIALISE_MEMBER(Format);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_STREAM_OUTPUT_BUFFER_VIEW &el)
{
  SERIALISE_MEMBER_TYPED(D3D12BufferLocation, BufferLocation);
  SERIALISE_MEMBER(SizeInBytes).OffsetOrSize();
  SERIALISE_MEMBER_TYPED(D3D12BufferLocation, BufferFilledSizeLocation);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_CONSTANT_BUFFER_VIEW_DESC &el)
{
  SERIALISE_MEMBER_TYPED(D3D12BufferLocation, BufferLocation).Important();
  SERIALISE_MEMBER(SizeInBytes);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BUFFER_SRV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
  SERIALISE_MEMBER(StructureByteStride).OffsetOrSize();
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(PlaneSlice);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(PlaneSlice);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_SRV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX3D_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEXCUBE_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEXCUBE_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(First2DArrayFace);
  SERIALISE_MEMBER(NumCubes);
  SERIALISE_MEMBER(ResourceMinLODClamp);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SHADER_RESOURCE_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format).Important();
  SERIALISE_MEMBER(ViewDimension);
  // cast to a special enum so we print nicely
  SERIALISE_MEMBER_TYPED(D3D12ComponentMapping, Shader4ComponentMapping);

  switch(el.ViewDimension)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_SRV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D12_SRV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D12_SRV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D12_SRV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE: SERIALISE_MEMBER(TextureCube); break;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: SERIALISE_MEMBER(TextureCubeArray); break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BUFFER_RTV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_ARRAY_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_ARRAY_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_RTV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_ARRAY_RTV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX3D_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstWSlice);
  SERIALISE_MEMBER(WSize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_TARGET_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format).Important();
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_RTV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_RTV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D12_RTV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D12_RTV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D12_RTV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_ARRAY_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_ARRAY_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_DSV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_ARRAY_DSV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCIL_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format).Important();
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_DSV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_DSV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D12_DSV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    default: RDCERR("Unrecognised DSV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BUFFER_UAV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
  SERIALISE_MEMBER(StructureByteStride).OffsetOrSize();
  SERIALISE_MEMBER(CounterOffsetInBytes).OffsetOrSize();
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX1D_ARRAY_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2D_ARRAY_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_UAV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX2DMS_ARRAY_UAV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEX3D_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstWSlice);
  SERIALISE_MEMBER(WSize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_UNORDERED_ACCESS_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format).Important();
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_UAV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_UAV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D12_UAV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D12_UAV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D12_UAV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D12_UAV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RESOURCE_TRANSITION_BARRIER &el)
{
  SERIALISE_MEMBER(pResource).Important();
  // cast to a special enum so we print 'all subresources' nicely
  SERIALISE_MEMBER_TYPED(D3D12ResourceBarrierSubresource, Subresource);
  SERIALISE_MEMBER(StateBefore);
  SERIALISE_MEMBER(StateAfter);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RESOURCE_ALIASING_BARRIER &el)
{
  SERIALISE_MEMBER(pResourceBefore);
  SERIALISE_MEMBER(pResourceAfter);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RESOURCE_UAV_BARRIER &el)
{
  SERIALISE_MEMBER(pResource);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RESOURCE_BARRIER &el)
{
  SERIALISE_MEMBER(Type).Important();
  SERIALISE_MEMBER(Flags);

  switch(el.Type)
  {
    case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: SERIALISE_MEMBER(Transition).Important(); break;
    case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: SERIALISE_MEMBER(Aliasing); break;
    case D3D12_RESOURCE_BARRIER_TYPE_UAV: SERIALISE_MEMBER(UAV); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_HEAP_PROPERTIES &el)
{
  SERIALISE_MEMBER(Type);
  SERIALISE_MEMBER(CPUPageProperty);
  SERIALISE_MEMBER(MemoryPoolPreference);
  SERIALISE_MEMBER(CreationNodeMask);
  SERIALISE_MEMBER(VisibleNodeMask);

  if(ser.IsReading() && !ser.IsStructurising())
    el.CreationNodeMask = el.VisibleNodeMask = 0;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_HEAP_DESC &el)
{
  SERIALISE_MEMBER(SizeInBytes).Important();
  SERIALISE_MEMBER(Properties);
  SERIALISE_MEMBER(Alignment);
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DESCRIPTOR_HEAP_DESC &el)
{
  SERIALISE_MEMBER(Type).Important();
  SERIALISE_MEMBER(NumDescriptors).Important();
  SERIALISE_MEMBER(Flags);
  SERIALISE_MEMBER(NodeMask);

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_QUERY_HEAP_DESC &el)
{
  SERIALISE_MEMBER(Type).Important();
  SERIALISE_MEMBER(Count).Important();
  SERIALISE_MEMBER(NodeMask);

  if(ser.IsReading() && !ser.IsStructurising())
    el.NodeMask = 0;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DEPTH_STENCIL_VALUE &el)
{
  SERIALISE_MEMBER(Depth);
  SERIALISE_MEMBER(Stencil);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_CLEAR_VALUE &el)
{
  SERIALISE_MEMBER(Format);

  if(IsDepthFormat(el.Format))
    SERIALISE_MEMBER(DepthStencil);
  else
    SERIALISE_MEMBER(Color);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SUBRESOURCE_FOOTPRINT &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(Depth);
  SERIALISE_MEMBER(RowPitch);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_PLACED_SUBRESOURCE_FOOTPRINT &el)
{
  SERIALISE_MEMBER(Offset).OffsetOrSize();
  SERIALISE_MEMBER(Footprint);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEXTURE_COPY_LOCATION &el)
{
  SERIALISE_MEMBER(pResource).Important();
  SERIALISE_MEMBER(Type);

  switch(el.Type)
  {
    case D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT: SERIALISE_MEMBER(PlacedFootprint); break;
    case D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX: SERIALISE_MEMBER(SubresourceIndex); break;
    default: RDCERR("Unexpected texture copy type %d", el.Type); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TILED_RESOURCE_COORDINATE &el)
{
  SERIALISE_MEMBER(X);
  SERIALISE_MEMBER(Y);
  SERIALISE_MEMBER(Z);
  SERIALISE_MEMBER(Subresource);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TILE_REGION_SIZE &el)
{
  SERIALISE_MEMBER(NumTiles);
  SERIALISE_MEMBER(UseBox);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(Depth);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DISCARD_REGION &el)
{
  SERIALISE_MEMBER(NumRects);
  SERIALISE_MEMBER_ARRAY(pRects, NumRects);
  SERIALISE_MEMBER(FirstSubresource);
  SERIALISE_MEMBER(NumSubresources);
}

template <>
void Deserialise(const D3D12_DISCARD_REGION &el)
{
  delete[] el.pRects;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RANGE &el)
{
  // serialise as uint64, so we're 32-bit/64-bit compatible

  uint64_t Begin = el.Begin;
  uint64_t End = el.End;

  ser.Serialise("Begin"_lit, Begin);
  ser.Serialise("End"_lit, End);

  if(ser.IsReading())
  {
    el.Begin = (SIZE_T)Begin;
    el.End = (SIZE_T)End;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_VIEWPORT &el)
{
  SERIALISE_MEMBER(TopLeftX).Important();
  SERIALISE_MEMBER(TopLeftY).Important();
  SERIALISE_MEMBER(Width).Important();
  SERIALISE_MEMBER(Height).Important();
  SERIALISE_MEMBER(MinDepth);
  SERIALISE_MEMBER(MaxDepth);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BOX &el)
{
  SERIALISE_MEMBER(left);
  SERIALISE_MEMBER(top);
  SERIALISE_MEMBER(front);
  SERIALISE_MEMBER(right);
  SERIALISE_MEMBER(bottom);
  SERIALISE_MEMBER(back);
}

// deliberately do not serialise D3D12_SAMPLER_DESC
// for ease of compatibility we serialise D3D12_SAMPLER_DESC2 here and use serialise versioning to
// detect old captures where the fields did not exist
// in descriptors where this is referenced, we unconditionally serialise the new version

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SAMPLER_DESC2 &el)
{
  SERIALISE_MEMBER(Filter).Important();
  SERIALISE_MEMBER(AddressU);
  SERIALISE_MEMBER(AddressV);
  SERIALISE_MEMBER(AddressW);
  SERIALISE_MEMBER(MipLODBias);
  SERIALISE_MEMBER(MaxAnisotropy);
  SERIALISE_MEMBER(ComparisonFunc);
  // serialise as floats since that is the most common case. It's also compatible with
  // D3D12_SAMPLER_DESC
  // since the flags come later in struct order, we can't do anything clever to serialise by that
  // type
  SERIALISE_MEMBER(FloatBorderColor);
  SERIALISE_MEMBER(MinLOD);
  SERIALISE_MEMBER(MaxLOD);
  if(ser.VersionAtLeast(0xF))
  {
    SERIALISE_MEMBER(Flags);
  }
  else
  {
    el.Flags = D3D12_SAMPLER_FLAG_NONE;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RT_FORMAT_ARRAY &el)
{
  SERIALISE_MEMBER(RTFormats);
  SERIALISE_MEMBER(NumRenderTargets);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_VIEW_INSTANCE_LOCATION &el)
{
  SERIALISE_MEMBER(ViewportArrayIndex);
  SERIALISE_MEMBER(RenderTargetArrayIndex);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_VIEW_INSTANCING_DESC &el)
{
  SERIALISE_MEMBER(ViewInstanceCount);
  SERIALISE_MEMBER_ARRAY(pViewInstanceLocations, ViewInstanceCount);
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SAMPLE_POSITION &el)
{
  SERIALISE_MEMBER(X);
  SERIALISE_MEMBER(Y);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RANGE_UINT64 &el)
{
  SERIALISE_MEMBER(Begin);
  SERIALISE_MEMBER(End);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_SUBRESOURCE_RANGE_UINT64 &el)
{
  SERIALISE_MEMBER(Subresource);
  SERIALISE_MEMBER(Range);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_WRITEBUFFERIMMEDIATE_PARAMETER &el)
{
  if(ser.VersionAtLeast(0x7))
  {
    SERIALISE_MEMBER_TYPED(D3D12BufferLocation, Dest);
  }
  else
  {
    RDCERR(
        "Replay will crash - old capture with corrupted D3D12_WRITEBUFFERIMMEDIATE_PARAMETER. "
        "Re-capture to fix this.");
    SERIALISE_MEMBER(Dest);
  }
  SERIALISE_MEMBER(Value).Important();
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS &el)
{
  SERIALISE_MEMBER(ClearValue);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_PASS_BEGINNING_ACCESS &el)
{
  SERIALISE_MEMBER(Type);

  switch(el.Type)
  {
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD: break;
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE: break;
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR: SERIALISE_MEMBER(Clear); break;
    case D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS: break;
    default: RDCERR("Unexpected beginning access type %d", el.Type); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser,
                 D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS &el)
{
  SERIALISE_MEMBER(SrcSubresource);
  SERIALISE_MEMBER(DstSubresource);
  SERIALISE_MEMBER(DstX);
  SERIALISE_MEMBER(DstY);
  SERIALISE_MEMBER(SrcRect);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS &el)
{
  SERIALISE_MEMBER(pSrcResource);
  SERIALISE_MEMBER(pDstResource);
  SERIALISE_MEMBER(SubresourceCount);
  SERIALISE_MEMBER_ARRAY(pSubresourceParameters, SubresourceCount);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ResolveMode);
  SERIALISE_MEMBER(PreserveResolveSource);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_PASS_ENDING_ACCESS &el)
{
  SERIALISE_MEMBER(Type);

  switch(el.Type)
  {
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD: break;
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE: break;
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE: SERIALISE_MEMBER(Resolve); break;
    case D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS: break;
    default: RDCERR("Unexpected ending access type %d", el.Type); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_PASS_RENDER_TARGET_DESC &el)
{
  SERIALISE_MEMBER(cpuDescriptor);
  SERIALISE_MEMBER(BeginningAccess);
  SERIALISE_MEMBER(EndingAccess);
}

template <>
void Deserialise(const D3D12_RENDER_PASS_RENDER_TARGET_DESC &el)
{
  if(el.EndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
    delete[] el.EndingAccess.Resolve.pSubresourceParameters;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_RENDER_PASS_DEPTH_STENCIL_DESC &el)
{
  SERIALISE_MEMBER(cpuDescriptor);
  SERIALISE_MEMBER(DepthBeginningAccess);
  SERIALISE_MEMBER(StencilBeginningAccess);
  SERIALISE_MEMBER(DepthEndingAccess);
  SERIALISE_MEMBER(StencilEndingAccess);
}

template <>
void Deserialise(const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC &el)
{
  if(el.DepthEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
    delete[] el.DepthEndingAccess.Resolve.pSubresourceParameters;
  if(el.StencilEndingAccess.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE)
    delete[] el.StencilEndingAccess.Resolve.pSubresourceParameters;
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DRAW_ARGUMENTS &el)
{
  SERIALISE_MEMBER(VertexCountPerInstance).Important();
  SERIALISE_MEMBER(InstanceCount).Important();
  SERIALISE_MEMBER(StartVertexLocation);
  SERIALISE_MEMBER(StartInstanceLocation);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DRAW_INDEXED_ARGUMENTS &el)
{
  SERIALISE_MEMBER(IndexCountPerInstance).Important();
  SERIALISE_MEMBER(InstanceCount).Important();
  SERIALISE_MEMBER(StartIndexLocation);
  SERIALISE_MEMBER(BaseVertexLocation);
  SERIALISE_MEMBER(StartInstanceLocation);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DISPATCH_ARGUMENTS &el)
{
  SERIALISE_MEMBER(ThreadGroupCountX).Important();
  SERIALISE_MEMBER(ThreadGroupCountY).Important();
  SERIALISE_MEMBER(ThreadGroupCountZ).Important();
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_DISPATCH_MESH_ARGUMENTS &el)
{
  SERIALISE_MEMBER(ThreadGroupCountX).Important();
  SERIALISE_MEMBER(ThreadGroupCountY).Important();
  SERIALISE_MEMBER(ThreadGroupCountZ).Important();
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12ResourceLayout &el)
{
  RDCCOMPILE_ASSERT(sizeof(el) == sizeof(uint32_t),
                    "D3D12ResourceLayout is expected to be a 32-bit value");
  ser.SerialiseValue(SDBasic::Enum, 4, (uint32_t &)el);
  ser.SerialiseStringify(el);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_GLOBAL_BARRIER &el)
{
  SERIALISE_MEMBER(SyncBefore);
  SERIALISE_MEMBER(SyncAfter);
  SERIALISE_MEMBER(AccessBefore);
  SERIALISE_MEMBER(AccessAfter);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BARRIER_SUBRESOURCE_RANGE &el)
{
  SERIALISE_MEMBER(IndexOrFirstMipLevel);
  SERIALISE_MEMBER(NumMipLevels);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(NumArraySlices);
  SERIALISE_MEMBER(FirstPlane);
  SERIALISE_MEMBER(NumPlanes);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_TEXTURE_BARRIER &el)
{
  SERIALISE_MEMBER(SyncBefore);
  SERIALISE_MEMBER(SyncAfter);
  SERIALISE_MEMBER(AccessBefore);
  SERIALISE_MEMBER(AccessAfter);
  SERIALISE_MEMBER(LayoutBefore);
  SERIALISE_MEMBER(LayoutAfter);
  SERIALISE_MEMBER(pResource).Important();
  SERIALISE_MEMBER(Subresources);
  SERIALISE_MEMBER(Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BUFFER_BARRIER &el)
{
  SERIALISE_MEMBER(SyncBefore);
  SERIALISE_MEMBER(SyncAfter);
  SERIALISE_MEMBER(AccessBefore);
  SERIALISE_MEMBER(AccessAfter);
  SERIALISE_MEMBER(pResource).Important();
  SERIALISE_MEMBER(Offset).OffsetOrSize();
  SERIALISE_MEMBER(Size);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D12_BARRIER_GROUP &el)
{
  SERIALISE_MEMBER(Type).Important();
  SERIALISE_MEMBER(NumBarriers).Important();

  switch(el.Type)
  {
    case D3D12_BARRIER_TYPE_GLOBAL:
    {
      SERIALISE_MEMBER_ARRAY(pGlobalBarriers, NumBarriers);
      break;
    }
    case D3D12_BARRIER_TYPE_TEXTURE:
    {
      SERIALISE_MEMBER_ARRAY(pTextureBarriers, NumBarriers).Important();
      break;
    }
    case D3D12_BARRIER_TYPE_BUFFER:
    {
      SERIALISE_MEMBER_ARRAY(pBufferBarriers, NumBarriers).Important();
      break;
    }
  }
}

template <>
void Deserialise(const D3D12_BARRIER_GROUP &el)
{
  switch(el.Type)
  {
    case D3D12_BARRIER_TYPE_GLOBAL:
    {
      delete[] el.pGlobalBarriers;
      break;
    }
    case D3D12_BARRIER_TYPE_TEXTURE:
    {
      delete[] el.pTextureBarriers;
      break;
    }
    case D3D12_BARRIER_TYPE_BUFFER:
    {
      delete[] el.pBufferBarriers;
      break;
    }
  }
}

INSTANTIATE_SERIALISE_TYPE(D3D12RootSignature);
INSTANTIATE_SERIALISE_TYPE(PortableHandle);
INSTANTIATE_SERIALISE_TYPE(D3D12_CPU_DESCRIPTOR_HANDLE);
INSTANTIATE_SERIALISE_TYPE(D3D12_GPU_DESCRIPTOR_HANDLE);
INSTANTIATE_SERIALISE_TYPE(DynamicDescriptorCopy);
INSTANTIATE_SERIALISE_TYPE(D3D12BufferLocation);
INSTANTIATE_SERIALISE_TYPE(D3D12Descriptor);
INSTANTIATE_SERIALISE_TYPE(D3D12_EXPANDED_PIPELINE_STATE_STREAM_DESC);

INSTANTIATE_SERIALISE_TYPE(D3D12_RESOURCE_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_RESOURCE_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D12_COMMAND_QUEUE_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_SHADER_BYTECODE);
INSTANTIATE_SERIALISE_TYPE(D3D12_GRAPHICS_PIPELINE_STATE_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_COMPUTE_PIPELINE_STATE_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_INDEX_BUFFER_VIEW);
INSTANTIATE_SERIALISE_TYPE(D3D12_VERTEX_BUFFER_VIEW);
INSTANTIATE_SERIALISE_TYPE(D3D12_STREAM_OUTPUT_BUFFER_VIEW);
INSTANTIATE_SERIALISE_TYPE(D3D12_RESOURCE_BARRIER);
INSTANTIATE_SERIALISE_TYPE(D3D12_HEAP_PROPERTIES);
INSTANTIATE_SERIALISE_TYPE(D3D12_HEAP_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_DESCRIPTOR_HEAP_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_INDIRECT_ARGUMENT_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_COMMAND_SIGNATURE_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_QUERY_HEAP_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_SAMPLER_DESC2);
INSTANTIATE_SERIALISE_TYPE(D3D12_CONSTANT_BUFFER_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_SHADER_RESOURCE_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_RENDER_TARGET_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_DEPTH_STENCIL_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_UNORDERED_ACCESS_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_CLEAR_VALUE);
INSTANTIATE_SERIALISE_TYPE(D3D12_BLEND_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_TEXTURE_COPY_LOCATION);
INSTANTIATE_SERIALISE_TYPE(D3D12_TILED_RESOURCE_COORDINATE);
INSTANTIATE_SERIALISE_TYPE(D3D12_TILE_REGION_SIZE);
INSTANTIATE_SERIALISE_TYPE(D3D12_DISCARD_REGION);
INSTANTIATE_SERIALISE_TYPE(D3D12_RANGE);
INSTANTIATE_SERIALISE_TYPE(D3D12_RECT);
INSTANTIATE_SERIALISE_TYPE(D3D12_BOX);
INSTANTIATE_SERIALISE_TYPE(D3D12_VIEWPORT);
INSTANTIATE_SERIALISE_TYPE(D3D12_PIPELINE_STATE_STREAM_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_RT_FORMAT_ARRAY);
INSTANTIATE_SERIALISE_TYPE(D3D12_RASTERIZER_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_RASTERIZER_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D12_RASTERIZER_DESC2);
INSTANTIATE_SERIALISE_TYPE(D3D12_DEPTH_STENCIL_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D12_DEPTH_STENCIL_DESC2);
INSTANTIATE_SERIALISE_TYPE(D3D12_VIEW_INSTANCING_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_SAMPLE_POSITION);
INSTANTIATE_SERIALISE_TYPE(D3D12_SUBRESOURCE_RANGE_UINT64);
INSTANTIATE_SERIALISE_TYPE(D3D12_WRITEBUFFERIMMEDIATE_PARAMETER);
INSTANTIATE_SERIALISE_TYPE(D3D12_RENDER_PASS_RENDER_TARGET_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_RENDER_PASS_DEPTH_STENCIL_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D12_BARRIER_GROUP);
INSTANTIATE_SERIALISE_TYPE(D3D12ResourceLayout);
INSTANTIATE_SERIALISE_TYPE(D3D12_DRAW_ARGUMENTS);
INSTANTIATE_SERIALISE_TYPE(D3D12_DRAW_INDEXED_ARGUMENTS);
INSTANTIATE_SERIALISE_TYPE(D3D12_DISPATCH_ARGUMENTS);
INSTANTIATE_SERIALISE_TYPE(D3D12_DISPATCH_MESH_ARGUMENTS);
