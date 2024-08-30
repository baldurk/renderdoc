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

#include "d3d12_rootsig.h"
#include "driver/shaders/dxbc/dxbc_container.h"

static D3D12_STATIC_SAMPLER_DESC1 Upconvert(const D3D12_STATIC_SAMPLER_DESC &StaticSampler)
{
  D3D12_STATIC_SAMPLER_DESC1 ret;
  memcpy(&ret, &StaticSampler, sizeof(StaticSampler));
  ret.Flags = D3D12_SAMPLER_FLAG_NONE;
  return ret;
}

static D3D12_STATIC_SAMPLER_DESC Downconvert(const D3D12_STATIC_SAMPLER_DESC1 &StaticSampler)
{
  D3D12_STATIC_SAMPLER_DESC ret;
  memcpy(&ret, &StaticSampler, sizeof(ret));
  if(StaticSampler.Flags != 0)
    RDCWARN("Downconverting sampler with advanced features set");
  if(ret.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT)
    ret.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT;
  else if(ret.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT)
    ret.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE_UINT;
  return ret;
}

static D3D12_DESCRIPTOR_RANGE Downconvert(const D3D12_DESCRIPTOR_RANGE1 &Range)
{
  D3D12_DESCRIPTOR_RANGE ret;
  memcpy(&ret, &Range, sizeof(ret));
  // this mismatches because it's not a subset, we copied the flags
  ret.OffsetInDescriptorsFromTableStart = Range.OffsetInDescriptorsFromTableStart;
  return ret;
}

struct RootSigHeader
{
  uint32_t Version;
  uint32_t NumParams;
  uint32_t ParamDataOffset;
  uint32_t NumStaticSamplers;
  uint32_t StaticSamplerOffset;
  uint32_t Flags;
};

struct RootSigParameter
{
  uint32_t Type;
  uint32_t Visibility;
  uint32_t DataOffset;
};

struct RootSigDescriptorTable
{
  uint32_t NumRanges;
  uint32_t DataOffset;
};

D3D12RootSignature DecodeRootSig(const void *data, size_t dataSize, bool withStandardContainer)
{
  D3D12RootSignature ret;

  const byte *base = (const byte *)data;
  if(withStandardContainer)
  {
    size_t rts0Size = 0;
    const byte *rts0 = DXBC::DXBCContainer::FindChunk(base, dataSize, DXBC::FOURCC_RTS0, rts0Size);

    if(!rts0)
      return {};

    base = rts0;
    dataSize = rts0Size;
  }

  const RootSigHeader *header = (const RootSigHeader *)base;

  ret.Flags = (D3D12_ROOT_SIGNATURE_FLAGS)header->Flags;

  ret.Parameters.resize(header->NumParams);

  ret.dwordLength = 0;

  const RootSigParameter *paramArray = (const RootSigParameter *)(base + header->ParamDataOffset);
  for(size_t i = 0; i < ret.Parameters.size(); i++)
  {
    const RootSigParameter &param = paramArray[i];

    if(header->Version >= D3D_ROOT_SIGNATURE_VERSION_1_1)
    {
      D3D12_ROOT_PARAMETER1 desc;
      desc.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)param.Type;
      desc.ShaderVisibility = (D3D12_SHADER_VISIBILITY)param.Visibility;

      if(desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      {
        desc.Constants = *(D3D12_ROOT_CONSTANTS *)(base + param.DataOffset);
      }
      else if(desc.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        desc.Descriptor = *(D3D12_ROOT_DESCRIPTOR1 *)(base + param.DataOffset);
      }
      else
      {
        RootSigDescriptorTable table = *(RootSigDescriptorTable *)(base + param.DataOffset);
        desc.DescriptorTable.NumDescriptorRanges = table.NumRanges;
        desc.DescriptorTable.pDescriptorRanges = (D3D12_DESCRIPTOR_RANGE1 *)(base + table.DataOffset);
      }

      ret.Parameters[i].MakeFrom(desc, ret.maxSpaceIndex);
    }
    else
    {
      D3D12_ROOT_PARAMETER desc = {};
      desc.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)param.Type;
      desc.ShaderVisibility = (D3D12_SHADER_VISIBILITY)param.Visibility;

      if(desc.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      {
        desc.Constants = *(D3D12_ROOT_CONSTANTS *)(base + param.DataOffset);
      }
      else if(desc.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      {
        D3D12_ROOT_DESCRIPTOR root = *(D3D12_ROOT_DESCRIPTOR *)(base + param.DataOffset);
        desc.Descriptor.RegisterSpace = root.RegisterSpace;
        desc.Descriptor.ShaderRegister = root.ShaderRegister;
      }
      else
      {
        RootSigDescriptorTable table = *(RootSigDescriptorTable *)(base + param.DataOffset);
        desc.DescriptorTable.NumDescriptorRanges = table.NumRanges;
        desc.DescriptorTable.pDescriptorRanges = (D3D12_DESCRIPTOR_RANGE *)(base + table.DataOffset);
      }

      ret.Parameters[i].MakeFrom(desc, ret.maxSpaceIndex);
    }

    // Descriptor tables cost 1 DWORD each.
    // Root constants cost 1 DWORD each, since they are 32-bit values.
    // Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
    if(ret.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      ret.dwordLength++;
    else if(ret.Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      ret.dwordLength += ret.Parameters[i].Constants.Num32BitValues;
    else
      ret.dwordLength += 2;
  }

  if(header->NumStaticSamplers > 0)
  {
    if(header->Version >= D3D_ROOT_SIGNATURE_VERSION_1_2)
    {
      const D3D12_STATIC_SAMPLER_DESC1 *pStaticSamplers =
          (const D3D12_STATIC_SAMPLER_DESC1 *)(base + header->StaticSamplerOffset);
      ret.StaticSamplers.assign(pStaticSamplers, header->NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
    }
    else
    {
      const D3D12_STATIC_SAMPLER_DESC *pStaticSamplers =
          (const D3D12_STATIC_SAMPLER_DESC *)(base + header->StaticSamplerOffset);
      ret.StaticSamplers.resize(header->NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
      {
        ret.StaticSamplers[i] = Upconvert(pStaticSamplers[i]);
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
      }
    }
  }

  return ret;
}

bytebuf EncodeRootSig(D3D_ROOT_SIGNATURE_VERSION targetVersion,
                      const rdcarray<D3D12_ROOT_PARAMETER1> &params, D3D12_ROOT_SIGNATURE_FLAGS Flags,
                      UINT NumStaticSamplers, const D3D12_STATIC_SAMPLER_DESC1 *StaticSamplers)
{
  StreamWriter writer(128);

  RootSigHeader header = {};
  header.Version = targetVersion;
  header.NumParams = (uint32_t)params.size();
  header.NumStaticSamplers = NumStaticSamplers;
  header.Flags = Flags;

  writer.Write(header);

  writer.AlignTo<sizeof(uint32_t)>();
  header.ParamDataOffset = (uint32_t)writer.GetOffset();
  writer.WriteAt(offsetof(RootSigHeader, ParamDataOffset), header.ParamDataOffset);

  rdcarray<RootSigParameter> paramPtrs;
  paramPtrs.resize(params.size());
  for(size_t i = 0; i < params.size(); i++)
  {
    paramPtrs[i].Type = params[i].ParameterType;
    paramPtrs[i].Visibility = params[i].ShaderVisibility;
  }
  writer.Write(paramPtrs.data(), paramPtrs.byteSize());

  for(size_t i = 0; i < params.size(); i++)
  {
    writer.WriteAt(header.ParamDataOffset + sizeof(RootSigParameter) * i +
                       offsetof(RootSigParameter, DataOffset),
                   (uint32_t)writer.GetOffset());
    switch(params[i].ParameterType)
    {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: writer.Write(params[i].Constants); break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
      {
        if(header.Version >= D3D_ROOT_SIGNATURE_VERSION_1_1)
          writer.Write(params[i].Descriptor);
        else
          writer.Write(&params[i].Descriptor, sizeof(D3D12_ROOT_DESCRIPTOR));
        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
      {
        RootSigDescriptorTable table = {
            params[i].DescriptorTable.NumDescriptorRanges,
            uint32_t(writer.GetOffset()) + sizeof(RootSigDescriptorTable),
        };
        writer.Write(table);

        for(UINT r = 0; r < params[i].DescriptorTable.NumDescriptorRanges; r++)
        {
          if(header.Version >= D3D_ROOT_SIGNATURE_VERSION_1_1)
          {
            writer.Write(params[i].DescriptorTable.pDescriptorRanges[r]);
          }
          else
          {
            writer.Write(Downconvert(params[i].DescriptorTable.pDescriptorRanges[r]));
          }
        }
        break;
      }
    }
  }

  writer.AlignTo<sizeof(uint32_t)>();
  writer.WriteAt(offsetof(RootSigHeader, StaticSamplerOffset), (uint32_t)writer.GetOffset());

  if(header.Version >= D3D_ROOT_SIGNATURE_VERSION_1_2)
  {
    writer.Write(StaticSamplers, NumStaticSamplers * sizeof(D3D12_STATIC_SAMPLER_DESC1));
  }
  else
  {
    for(UINT i = 0; i < NumStaticSamplers; i++)
    {
      D3D12_STATIC_SAMPLER_DESC samp = Downconvert(StaticSamplers[i]);
      writer.Write(samp);
    }
  }

  return DXBC::DXBCContainer::MakeContainerForChunk(DXBC::FOURCC_RTS0, writer.GetData(),
                                                    writer.GetOffset());
}

bytebuf EncodeRootSig(D3D_ROOT_SIGNATURE_VERSION targetVersion, const D3D12RootSignature &rootsig)
{
  rdcarray<D3D12_ROOT_PARAMETER1> params;
  params.resize(rootsig.Parameters.size());
  for(size_t i = 0; i < params.size(); i++)
    params[i] = rootsig.Parameters[i];

  return EncodeRootSig(targetVersion, params, rootsig.Flags, (UINT)rootsig.StaticSamplers.size(),
                       rootsig.StaticSamplers.empty() ? NULL : &rootsig.StaticSamplers[0]);
}
