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

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

D3D12RootSignature DLLDecodeRootSig(D3D12DevConfiguration *devconfig, const void *data,
                                    size_t dataSize)
{
  ID3D12VersionedRootSignatureDeserializer *deser = NULL;
  HRESULT hr;

  hr = devconfig->devconfig->CreateVersionedRootSignatureDeserializer(
      data, dataSize, __uuidof(ID3D12VersionedRootSignatureDeserializer), (void **)&deser);

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get deserializer");
    return D3D12RootSignature();
  }

  D3D12RootSignature ret;

  uint32_t version = 12;
  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *verdesc = NULL;
  hr = deser->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_2, &verdesc);
  if(FAILED(hr))
  {
    version = 11;
    hr = deser->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1, &verdesc);
  }

  if(FAILED(hr))
  {
    SAFE_RELEASE(deser);
    RDCERR("Can't get descriptor");
    return D3D12RootSignature();
  }

  const D3D12_ROOT_SIGNATURE_DESC1 *desc = &verdesc->Desc_1_1;

  ret.Flags = desc->Flags;

  ret.Parameters.resize(desc->NumParameters);

  ret.dwordLength = 0;

  for(size_t i = 0; i < ret.Parameters.size(); i++)
  {
    ret.Parameters[i].MakeFrom(desc->pParameters[i], ret.maxSpaceIndex);

    // Descriptor tables cost 1 DWORD each.
    // Root constants cost 1 DWORD each, since they are 32-bit values.
    // Root descriptors (64-bit GPU virtual addresses) cost 2 DWORDs each.
    if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      ret.dwordLength++;
    else if(desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      ret.dwordLength += desc->pParameters[i].Constants.Num32BitValues;
    else
      ret.dwordLength += 2;
  }

  if(desc->NumStaticSamplers > 0)
  {
    if(version >= 12)
    {
      ret.StaticSamplers.assign(verdesc->Desc_1_2.pStaticSamplers,
                                verdesc->Desc_1_2.NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
    }
    else
    {
      ret.StaticSamplers.resize(desc->NumStaticSamplers);

      for(size_t i = 0; i < ret.StaticSamplers.size(); i++)
      {
        ret.StaticSamplers[i] = Upconvert(desc->pStaticSamplers[i]);
        ret.maxSpaceIndex = RDCMAX(ret.maxSpaceIndex, ret.StaticSamplers[i].RegisterSpace + 1);
      }
    }
  }

  SAFE_RELEASE(deser);

  return ret;
}

bytebuf DLLEncodeRootSig(D3D12DevConfiguration *devconfig, D3D_ROOT_SIGNATURE_VERSION targetVersion,
                         const rdcarray<D3D12_ROOT_PARAMETER1> &params,
                         D3D12_ROOT_SIGNATURE_FLAGS Flags, UINT NumStaticSamplers,
                         const D3D12_STATIC_SAMPLER_DESC1 *StaticSamplers)
{
  rdcarray<D3D12_ROOT_PARAMETER> params_1_0;
  params_1_0.resize(params.size());
  for(size_t i = 0; i < params.size(); i++)
  {
    params_1_0[i].ShaderVisibility = params[i].ShaderVisibility;
    params_1_0[i].ParameterType = params[i].ParameterType;

    if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      params_1_0[i].Constants = params[i].Constants;
    }
    else if(params[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
      params_1_0[i].DescriptorTable.NumDescriptorRanges =
          params[i].DescriptorTable.NumDescriptorRanges;

      D3D12_DESCRIPTOR_RANGE *dst =
          new D3D12_DESCRIPTOR_RANGE[params[i].DescriptorTable.NumDescriptorRanges];
      params_1_0[i].DescriptorTable.pDescriptorRanges = dst;

      for(UINT r = 0; r < params[i].DescriptorTable.NumDescriptorRanges; r++)
      {
        dst[r].BaseShaderRegister = params[i].DescriptorTable.pDescriptorRanges[r].BaseShaderRegister;
        dst[r].NumDescriptors = params[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
        dst[r].OffsetInDescriptorsFromTableStart =
            params[i].DescriptorTable.pDescriptorRanges[r].OffsetInDescriptorsFromTableStart;
        dst[r].RangeType = params[i].DescriptorTable.pDescriptorRanges[r].RangeType;
        dst[r].RegisterSpace = params[i].DescriptorTable.pDescriptorRanges[r].RegisterSpace;

        if(params[i].DescriptorTable.pDescriptorRanges[r].Flags !=
           (D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE))
          RDCWARN("Losing information when reducing down to 1.0 root signature");
      }
    }
    else
    {
      params_1_0[i].Descriptor.RegisterSpace = params[i].Descriptor.RegisterSpace;
      params_1_0[i].Descriptor.ShaderRegister = params[i].Descriptor.ShaderRegister;

      if(params[i].Descriptor.Flags != D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE)
        RDCWARN("Losing information when reducing down to 1.0 root signature");
    }
  }

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC verdesc;
  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_2;

  D3D12_ROOT_SIGNATURE_DESC2 &desc12 = verdesc.Desc_1_2;
  desc12.Flags = Flags;
  desc12.NumStaticSamplers = NumStaticSamplers;
  desc12.pStaticSamplers = StaticSamplers;
  desc12.NumParameters = (UINT)params.size();
  desc12.pParameters = &params[0];

  ID3DBlob *ret = NULL;
  ID3DBlob *errBlob = NULL;
  HRESULT hr = E_INVALIDARG;

  if(targetVersion >= D3D_ROOT_SIGNATURE_VERSION_1_2)
  {
    hr = devconfig->devconfig->SerializeVersionedRootSignature(&verdesc, &ret, &errBlob);
    if(errBlob)
      RDCERR("%s", errBlob->GetBufferPointer());
    SAFE_RELEASE(errBlob);
  }

  if(SUCCEEDED(hr))
  {
    bytebuf retBuf((byte *)ret->GetBufferPointer(), ret->GetBufferSize());
    return retBuf;
  }

  // if it failed or we're trying version 1.1, try again at version 1.1
  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
  D3D12_ROOT_SIGNATURE_DESC1 &desc11 = verdesc.Desc_1_1;
  rdcarray<D3D12_STATIC_SAMPLER_DESC> oldSamplers;
  oldSamplers.resize(NumStaticSamplers);
  for(size_t i = 0; i < oldSamplers.size(); i++)
    oldSamplers[i] = Downconvert(StaticSamplers[i]);
  desc11.NumStaticSamplers = oldSamplers.count();
  desc11.pStaticSamplers = oldSamplers.data();

  if(targetVersion >= D3D_ROOT_SIGNATURE_VERSION_1_1)
  {
    hr = devconfig->devconfig->SerializeVersionedRootSignature(&verdesc, &ret, &errBlob);
    if(errBlob)
      RDCERR("%s", errBlob->GetBufferPointer());
    SAFE_RELEASE(errBlob);
  }

  if(SUCCEEDED(hr))
  {
    bytebuf retBuf((byte *)ret->GetBufferPointer(), ret->GetBufferSize());
    return retBuf;
  }

  verdesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
  D3D12_ROOT_SIGNATURE_DESC &desc10 = verdesc.Desc_1_0;

  desc10.NumStaticSamplers = oldSamplers.count();
  desc10.pStaticSamplers = oldSamplers.data();

  desc10.NumParameters = params.count();
  desc10.pParameters = params_1_0.data();

  hr = devconfig->devconfig->SerializeVersionedRootSignature(&verdesc, &ret, &errBlob);
  if(errBlob)
    RDCERR("%s", errBlob->GetBufferPointer());
  SAFE_RELEASE(errBlob);

  for(size_t i = 0; i < params_1_0.size(); i++)
    if(params_1_0[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      delete[] params_1_0[i].DescriptorTable.pDescriptorRanges;

  if(SUCCEEDED(hr))
  {
    bytebuf retBuf((byte *)ret->GetBufferPointer(), ret->GetBufferSize());
    return retBuf;
  }

  return bytebuf();
}

bool operator==(const D3D12_ROOT_CONSTANTS &a, const D3D12_ROOT_CONSTANTS &b)
{
  return a.Num32BitValues == b.Num32BitValues && a.RegisterSpace == b.RegisterSpace &&
         a.ShaderRegister == b.ShaderRegister;
}

bool operator==(const D3D12_ROOT_DESCRIPTOR1 &a, const D3D12_ROOT_DESCRIPTOR1 &b)
{
  return a.Flags == b.Flags && a.RegisterSpace == b.RegisterSpace &&
         a.ShaderRegister == b.ShaderRegister;
}

bool operator==(const D3D12_STATIC_SAMPLER_DESC1 &a, const D3D12_STATIC_SAMPLER_DESC1 &b)
{
  return memcmp(&a, &b, sizeof(a)) == 0;
}

bool operator==(const D3D12RootSignatureParameter &a, const D3D12RootSignatureParameter &b)
{
  if(a.ShaderVisibility != b.ShaderVisibility)
    return false;

  if(a.ParameterType == b.ParameterType)
  {
    if(a.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
      return a.Constants == b.Constants;
    else if(a.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      return a.Descriptor == b.Descriptor;

    return a.ranges == b.ranges;
  }

  return false;
}

bool operator<(const D3D12RootSignatureParameter &a, const D3D12RootSignatureParameter &b)
{
  if(a == b)
    return false;

  return true;
}

void CheckRootSig(D3D12DevConfiguration *devconfig, const D3D12_ROOT_SIGNATURE_FLAGS &Flags,
                  const rdcarray<D3D12_ROOT_PARAMETER1> &rootParams,
                  const rdcarray<D3D12_STATIC_SAMPLER_DESC1> &samplers)
{
  for(D3D_ROOT_SIGNATURE_VERSION ver :
      {D3D_ROOT_SIGNATURE_VERSION_1_0, D3D_ROOT_SIGNATURE_VERSION_1_1, D3D_ROOT_SIGNATURE_VERSION_1_2})
  {
    bytebuf a =
        DLLEncodeRootSig(devconfig, ver, rootParams, Flags, samplers.count(), samplers.data());
    bytebuf b = EncodeRootSig(ver, rootParams, Flags, samplers.count(), samplers.data());

    CHECK((a == b));

    D3D12RootSignature rootA = DLLDecodeRootSig(devconfig, a.data(), a.size());
    D3D12RootSignature rootB = DecodeRootSig(a.data(), a.size());

    CHECK(rootA.Flags == rootB.Flags);
    REQUIRE(rootA.Parameters.size() == rootB.Parameters.size());
    REQUIRE(rootA.StaticSamplers.size() == rootB.StaticSamplers.size());
    for(size_t i = 0; i < rootA.Parameters.size(); i++)
    {
      CHECK((rootA.Parameters[i] == rootB.Parameters[i]));
    }
    for(size_t i = 0; i < rootA.StaticSamplers.size(); i++)
    {
      CHECK((rootA.StaticSamplers[i] == rootB.StaticSamplers[i]));
    }
  }
}

TEST_CASE("Test root signature encoding/decoding", "[rootsig]")
{
  HMODULE d3d12lib = LoadLibraryA("d3d12.dll");
  if(d3d12lib)
  {
    static D3D12DevConfiguration *devconfig =
        D3D12_PrepareReplaySDKVersion(false, 1, bytebuf(), bytebuf(), d3d12lib);

    // only run if we got a devconfig to know we can test v1.2
    if(devconfig)
    {
      D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

      rdcarray<D3D12_ROOT_PARAMETER1> rootParams;
      rdcarray<D3D12_DESCRIPTOR_RANGE1> ranges;
      rdcarray<D3D12_STATIC_SAMPLER_DESC1> samplers;

      {
        Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        rootParams.resize(8);
        ranges.resize(1 + 2 + 4 + 8);

        for(D3D12_ROOT_PARAMETER1 &root : rootParams)
          root.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParams[0].Descriptor = {1, 2, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE};

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParams[1].Descriptor = {3, 4,
                                    D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE};

        rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        rootParams[2].Descriptor = {5, 6, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC};

        rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParams[3].Constants = {7, 8, 4};

        D3D12_DESCRIPTOR_RANGE1 *range = ranges.data();

        rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[4].DescriptorTable.pDescriptorRanges = range;

        range[0] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 17, 9, 10, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 1010,
        };
        range += 1;

        rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[5].DescriptorTable.NumDescriptorRanges = 2;
        rootParams[5].DescriptorTable.pDescriptorRanges = range;

        range[0] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            18,
            11,
            12,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            2020,
        };
        range[1] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV,           19,   13, 14,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE, 3030,
        };
        range += 2;

        rootParams[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[6].DescriptorTable.NumDescriptorRanges = 4;
        rootParams[6].DescriptorTable.pDescriptorRanges = range;

        range[0] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            17,
            9,
            10,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE |
                D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,
            4040,
        };
        range[1] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            17,
            9,
            10,
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS,
            5050,
        };
        range[2] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 17, 9, 10, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 6060,
        };
        range[3] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,         17,   9, 11,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 7070,
        };
        range += 4;

        rootParams[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[7].DescriptorTable.NumDescriptorRanges = 8;
        rootParams[7].DescriptorTable.pDescriptorRanges = range;

        range[0] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 12,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[1] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 13,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[2] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 14,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[3] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 15,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[4] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 16,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[5] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 17,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[6] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 18,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range[7] = {
            D3D12_DESCRIPTOR_RANGE_TYPE_CBV,         17,   9, 19,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 1010,
        };
        range += 8;

        samplers = {
            // point
            {
                D3D12_FILTER_MIN_MAG_MIP_POINT,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                0.0f,
                1,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                0.0f,
                FLT_MAX,
                0,
                0,
                D3D12_SHADER_VISIBILITY_PIXEL,
                D3D12_SAMPLER_FLAG_NONE,
            },

            // linear
            {
                D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                0.0f,
                1,
                D3D12_COMPARISON_FUNC_ALWAYS,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK_UINT,
                0.0f,
                FLT_MAX,
                1,
                0,
                D3D12_SHADER_VISIBILITY_PIXEL,
                D3D12_SAMPLER_FLAG_UINT_BORDER_COLOR,
            },
        };
      }

      SECTION("Empty root signature")
      {
        Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        rootParams.clear();
        samplers.clear();

        CheckRootSig(devconfig, Flags, rootParams, samplers);
      }

      SECTION("Only parameters")
      {
        samplers.clear();

        CheckRootSig(devconfig, Flags, rootParams, samplers);
      }

      SECTION("Only samplers")
      {
        rootParams.clear();

        CheckRootSig(devconfig, Flags, rootParams, samplers);
      }

      SECTION("Trim parameters")
      {
        rootParams.erase(0);

        CheckRootSig(devconfig, Flags, rootParams, samplers);
      }

      SECTION("Trim parameters")
      {
        rootParams.pop_back();

        CheckRootSig(devconfig, Flags, rootParams, samplers);
      }

      SECTION("Remove a range")
      {
        rootParams.back().DescriptorTable.NumDescriptorRanges--;

        CheckRootSig(devconfig, Flags, rootParams, samplers);
      }
    }
  }
}

#endif
