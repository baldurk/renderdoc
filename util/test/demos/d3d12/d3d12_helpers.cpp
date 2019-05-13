/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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

#include "d3d12_test.h"

static const UINT formatStrides[] = {
    0,        // DXGI_FORMAT_UNKNOWN
    4 * 4,    // DXGI_FORMAT_R32G32B32A32_TYPELESS
    4 * 4,    // DXGI_FORMAT_R32G32B32A32_FLOAT
    4 * 4,    // DXGI_FORMAT_R32G32B32A32_UINT
    4 * 4,    // DXGI_FORMAT_R32G32B32A32_SINT
    4 * 3,    // DXGI_FORMAT_R32G32B32_TYPELESS
    4 * 3,    // DXGI_FORMAT_R32G32B32_FLOAT
    4 * 3,    // DXGI_FORMAT_R32G32B32_UINT
    4 * 3,    // DXGI_FORMAT_R32G32B32_SINT
    2 * 4,    // DXGI_FORMAT_R16G16B16A16_TYPELESS
    2 * 4,    // DXGI_FORMAT_R16G16B16A16_FLOAT
    2 * 4,    // DXGI_FORMAT_R16G16B16A16_UNORM
    2 * 4,    // DXGI_FORMAT_R16G16B16A16_UINT
    2 * 4,    // DXGI_FORMAT_R16G16B16A16_SNORM
    2 * 4,    // DXGI_FORMAT_R16G16B16A16_SINT
    4 * 2,    // DXGI_FORMAT_R32G32_TYPELESS
    4 * 2,    // DXGI_FORMAT_R32G32_FLOAT
    4 * 2,    // DXGI_FORMAT_R32G32_UINT
    4 * 2,    // DXGI_FORMAT_R32G32_SINT
    4 * 2,    // DXGI_FORMAT_R32G8X24_TYPELESS
    5,        // DXGI_FORMAT_D32_FLOAT_S8X24_UINT
    5,        // DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS
    5,        // DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
    4,        // DXGI_FORMAT_R10G10B10A2_TYPELESS
    4,        // DXGI_FORMAT_R10G10B10A2_UNORM
    4,        // DXGI_FORMAT_R10G10B10A2_UINT
    4,        // DXGI_FORMAT_R11G11B10_FLOAT
    1 * 4,    // DXGI_FORMAT_R8G8B8A8_TYPELESS
    1 * 4,    // DXGI_FORMAT_R8G8B8A8_UNORM
    1 * 4,    // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    1 * 4,    // DXGI_FORMAT_R8G8B8A8_UINT
    1 * 4,    // DXGI_FORMAT_R8G8B8A8_SNORM
    1 * 4,    // DXGI_FORMAT_R8G8B8A8_SINT
    2 * 2,    // DXGI_FORMAT_R16G16_TYPELESS
    2 * 2,    // DXGI_FORMAT_R16G16_FLOAT
    2 * 2,    // DXGI_FORMAT_R16G16_UNORM
    2 * 2,    // DXGI_FORMAT_R16G16_UINT
    2 * 2,    // DXGI_FORMAT_R16G16_SNORM
    2 * 2,    // DXGI_FORMAT_R16G16_SINT
    4 * 1,    // DXGI_FORMAT_R32_TYPELESS
    4 * 1,    // DXGI_FORMAT_D32_FLOAT
    4 * 1,    // DXGI_FORMAT_R32_FLOAT
    4 * 1,    // DXGI_FORMAT_R32_UINT
    4 * 1,    // DXGI_FORMAT_R32_SINT
    4,        // DXGI_FORMAT_R24G8_TYPELESS
    4,        // DXGI_FORMAT_D24_UNORM_S8_UINT
    4,        // DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    4,        // DXGI_FORMAT_X24_TYPELESS_G8_UINT
    1 * 2,    // DXGI_FORMAT_R8G8_TYPELESS
    1 * 2,    // DXGI_FORMAT_R8G8_UNORM
    1 * 2,    // DXGI_FORMAT_R8G8_UINT
    1 * 2,    // DXGI_FORMAT_R8G8_SNORM
    1 * 2,    // DXGI_FORMAT_R8G8_SINT
    2 * 1,    // DXGI_FORMAT_R16_TYPELESS
    2 * 1,    // DXGI_FORMAT_R16_FLOAT
    2,        // DXGI_FORMAT_D16_UNORM
    2 * 1,    // DXGI_FORMAT_R16_UNORM
    2 * 1,    // DXGI_FORMAT_R16_UINT
    2 * 1,    // DXGI_FORMAT_R16_SNORM
    2 * 1,    // DXGI_FORMAT_R16_SINT
    1 * 1,    // DXGI_FORMAT_R8_TYPELESS
    1 * 1,    // DXGI_FORMAT_R8_UNORM
    1 * 1,    // DXGI_FORMAT_R8_UINT
    1 * 1,    // DXGI_FORMAT_R8_SNORM
    1 * 1,    // DXGI_FORMAT_R8_SINT
    1,        // DXGI_FORMAT_A8_UNORM
    1,        // DXGI_FORMAT_R1_UNORM
    4,        // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
    1 * 3,    // DXGI_FORMAT_R8G8_B8G8_UNORM
    1 * 3,    // DXGI_FORMAT_G8R8_G8B8_UNORM
    0,        // DXGI_FORMAT_BC1_TYPELESS
    0,        // DXGI_FORMAT_BC1_UNORM
    0,        // DXGI_FORMAT_BC1_UNORM_SRGB
    0,        // DXGI_FORMAT_BC2_TYPELESS
    0,        // DXGI_FORMAT_BC2_UNORM
    0,        // DXGI_FORMAT_BC2_UNORM_SRGB
    0,        // DXGI_FORMAT_BC3_TYPELESS
    0,        // DXGI_FORMAT_BC3_UNORM
    0,        // DXGI_FORMAT_BC3_UNORM_SRGB
    0,        // DXGI_FORMAT_BC4_TYPELESS
    0,        // DXGI_FORMAT_BC4_UNORM
    0,        // DXGI_FORMAT_BC4_SNORM
    0,        // DXGI_FORMAT_BC5_TYPELESS
    0,        // DXGI_FORMAT_BC5_UNORM
    0,        // DXGI_FORMAT_BC5_SNORM
    0,        // DXGI_FORMAT_B5G6R5_UNORM
    0,        // DXGI_FORMAT_B5G5R5A1_UNORM
    1 * 4,    // DXGI_FORMAT_B8G8R8A8_UNORM
    1 * 3,    // DXGI_FORMAT_B8G8R8X8_UNORM
    4,        // DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM
    1 * 4,    // DXGI_FORMAT_B8G8R8A8_TYPELESS
    1 * 4,    // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
    1 * 3,    // DXGI_FORMAT_B8G8R8X8_TYPELESS
    1 * 3,    // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
    0,        // DXGI_FORMAT_BC6H_TYPELESS
    0,        // DXGI_FORMAT_BC6H_UF16
    0,        // DXGI_FORMAT_BC6H_SF16
    0,        // DXGI_FORMAT_BC7_TYPELESS
    0,        // DXGI_FORMAT_BC7_UNORM
    0,        // DXGI_FORMAT_BC7_UNORM_SRGB
    0,        // DXGI_FORMAT_AYUV
    0,        // DXGI_FORMAT_Y410
    0,        // DXGI_FORMAT_Y416
    0,        // DXGI_FORMAT_NV12
    0,        // DXGI_FORMAT_P010
    0,        // DXGI_FORMAT_P016
    0,        // DXGI_FORMAT_420_OPAQUE
    0,        // DXGI_FORMAT_YUY2
    0,        // DXGI_FORMAT_Y210
    0,        // DXGI_FORMAT_Y216
    0,        // DXGI_FORMAT_NV11
    0,        // DXGI_FORMAT_AI44
    0,        // DXGI_FORMAT_IA44
    0,        // DXGI_FORMAT_P8
    0,        // DXGI_FORMAT_A8P8
    0,        // DXGI_FORMAT_B4G4R4A4_UNORM
};

D3D12_ROOT_PARAMETER1 cbvParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg)
{
  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  ret.Descriptor.RegisterSpace = space;
  ret.Descriptor.ShaderRegister = reg;
  ret.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

  return ret;
}

D3D12_ROOT_PARAMETER1 constParam(D3D12_SHADER_VISIBILITY vis, UINT space, UINT reg, UINT num)
{
  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  ret.Constants.RegisterSpace = space;
  ret.Constants.ShaderRegister = reg;
  ret.Constants.Num32BitValues = num;

  return ret;
}

D3D12_ROOT_PARAMETER1 tableParam(D3D12_SHADER_VISIBILITY vis, D3D12_DESCRIPTOR_RANGE_TYPE type,
                                 UINT space, UINT basereg, UINT numreg, UINT descOffset)
{
  // this is a super hack but avoids the need to be clumsy with allocation of these structs
  static D3D12_DESCRIPTOR_RANGE1 ranges[32] = {};
  static int rangeIdx = 0;

  D3D12_DESCRIPTOR_RANGE1 &range = ranges[rangeIdx];
  rangeIdx = (rangeIdx + 1) % ARRAY_COUNT(ranges);

  D3D12_ROOT_PARAMETER1 ret;

  ret.ShaderVisibility = vis;
  ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  ret.DescriptorTable.NumDescriptorRanges = 1;
  ret.DescriptorTable.pDescriptorRanges = &range;

  memset(&range, 0, sizeof(range));

  range.RangeType = type;
  range.RegisterSpace = space;
  range.BaseShaderRegister = basereg;
  range.NumDescriptors = numreg;
  range.OffsetInDescriptorsFromTableStart = descOffset;
  range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

  return ret;
}

D3D12BufferCreator::D3D12BufferCreator(D3D12GraphicsTest *test) : m_Test(test)
{
  m_BufDesc.Alignment = 0;
  m_BufDesc.DepthOrArraySize = 1;
  m_BufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  m_BufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  m_BufDesc.Format = DXGI_FORMAT_UNKNOWN;
  m_BufDesc.Height = 1;
  m_BufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  m_BufDesc.Width = 64;
  m_BufDesc.MipLevels = 1;
  m_BufDesc.SampleDesc.Count = 1;
  m_BufDesc.SampleDesc.Quality = 0;

  m_HeapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
  m_HeapDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  m_HeapDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  m_HeapDesc.CreationNodeMask = 1;
  m_HeapDesc.VisibleNodeMask = 1;
}

D3D12BufferCreator &D3D12BufferCreator::UAV()
{
  m_BufDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  return *this;
}

D3D12BufferCreator &D3D12BufferCreator::Upload()
{
  m_HeapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
  return *this;
}

D3D12BufferCreator &D3D12BufferCreator::Readback()
{
  m_HeapDesc.Type = D3D12_HEAP_TYPE_READBACK;
  return *this;
}

D3D12BufferCreator &D3D12BufferCreator::Data(const void *data)
{
  m_Initdata = data;
  return *this;
}

D3D12BufferCreator &D3D12BufferCreator::Size(UINT size)
{
  m_BufDesc.Width = size;
  return *this;
}

D3D12BufferCreator::operator ID3D12ResourcePtr() const
{
  D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

  if(m_HeapDesc.Type == D3D12_HEAP_TYPE_UPLOAD)
    initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
  else if(m_HeapDesc.Type == D3D12_HEAP_TYPE_READBACK)
    initialState = D3D12_RESOURCE_STATE_COPY_DEST;

  ID3D12ResourcePtr buf;
  CHECK_HR(m_Test->dev->CreateCommittedResource(&m_HeapDesc, D3D12_HEAP_FLAG_NONE, &m_BufDesc,
                                                initialState, NULL, __uuidof(ID3D12Resource),
                                                (void **)&buf));

  if(m_Initdata)
    m_Test->SetBufferData(buf, D3D12_RESOURCE_STATE_COMMON, (const byte *)m_Initdata,
                          m_BufDesc.Width);

  return buf;
}

D3D12TextureCreator::D3D12TextureCreator(D3D12GraphicsTest *test, DXGI_FORMAT format, UINT width,
                                         UINT height, UINT depth)
    : m_Test(test)
{
  m_InitialState = D3D12_RESOURCE_STATE_COMMON;

  m_TexDesc.Alignment = 0;
  m_TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
  if(height > 1)
    m_TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  if(depth > 1)
    m_TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
  m_TexDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
  m_TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  m_TexDesc.Format = format;
  m_TexDesc.Width = width;
  m_TexDesc.Height = height;
  m_TexDesc.DepthOrArraySize = (UINT16)depth;
  m_TexDesc.MipLevels = 1;
  m_TexDesc.SampleDesc.Count = 1;
  m_TexDesc.SampleDesc.Quality = 0;

  m_HeapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
  m_HeapDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  m_HeapDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  m_HeapDesc.CreationNodeMask = 1;
  m_HeapDesc.VisibleNodeMask = 1;
}

D3D12TextureCreator &D3D12TextureCreator::Mips(UINT mips)
{
  m_TexDesc.MipLevels = (UINT16)mips;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::Array(UINT size)
{
  m_TexDesc.DepthOrArraySize = (UINT16)size;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::Multisampled(UINT count, UINT quality)
{
  m_TexDesc.SampleDesc.Count = count;
  m_TexDesc.SampleDesc.Quality = quality;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::UAV()
{
  m_TexDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::RTV()
{
  m_TexDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::DSV()
{
  m_TexDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::Upload()
{
  m_HeapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::Readback()
{
  m_HeapDesc.Type = D3D12_HEAP_TYPE_READBACK;
  return *this;
}

D3D12TextureCreator &D3D12TextureCreator::InitialState(D3D12_RESOURCE_STATES state)
{
  m_InitialState = state;
  return *this;
}

D3D12TextureCreator::operator ID3D12ResourcePtr() const
{
  ID3D12ResourcePtr tex;
  CHECK_HR(m_Test->dev->CreateCommittedResource(&m_HeapDesc, D3D12_HEAP_FLAG_NONE, &m_TexDesc,
                                                m_InitialState, NULL, __uuidof(ID3D12Resource),
                                                (void **)&tex));
  return tex;
}

D3D12ViewCreator::D3D12ViewCreator(D3D12GraphicsTest *test, ID3D12DescriptorHeap *heap,
                                   ViewType viewType, ID3D12Resource *res)
    : m_Test(test), m_Type(viewType), m_Heap(heap), m_Res(res)
{
  D3D12_RESOURCE_DESC resdesc = res->GetDesc();
  D3D12_RESOURCE_DIMENSION dim = resdesc.Dimension;

  Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  if(dim == D3D12_RESOURCE_DIMENSION_BUFFER)
  {
    SetupDescriptors(viewType, ResourceType::Buffer);
  }
  else if(dim == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
  {
    ResourceType resType = ResourceType::Texture1D;

    if(resdesc.DepthOrArraySize > 1)
      resType = ResourceType::Texture1DArray;

    SetupDescriptors(viewType, resType);
  }
  else if(dim == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
  {
    ResourceType resType;

    if(resdesc.SampleDesc.Count > 1)
    {
      resType = ResourceType::Texture2DMS;
      if(resdesc.DepthOrArraySize > 1)
        resType = ResourceType::Texture2DMSArray;
    }
    else
    {
      resType = ResourceType::Texture2D;
      if(resdesc.DepthOrArraySize > 1)
        resType = ResourceType::Texture2DArray;
    }

    SetupDescriptors(viewType, resType);
  }
  else if(dim == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
  {
    SetupDescriptors(viewType, ResourceType::Texture3D);
  }

  if(dim != D3D12_RESOURCE_DIMENSION_BUFFER)
    Format(resdesc.Format);
}

void D3D12ViewCreator::SetupDescriptors(ViewType viewType, ResourceType resType)
{
  memset(&desc, 0, sizeof(desc));

  constexpr D3D12_SRV_DIMENSION srvDim[] = {
      D3D12_SRV_DIMENSION_BUFFER,              // Buffer
      D3D12_SRV_DIMENSION_TEXTURE1D,           // Texture1D
      D3D12_SRV_DIMENSION_TEXTURE1DARRAY,      // Texture1DArray
      D3D12_SRV_DIMENSION_TEXTURE2D,           // Texture2D
      D3D12_SRV_DIMENSION_TEXTURE2DARRAY,      // Texture2DArray
      D3D12_SRV_DIMENSION_TEXTURE2DMS,         // Texture2DMS
      D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY,    // Texture2DMSArray
      D3D12_SRV_DIMENSION_TEXTURE3D,           // Texture3D
  };

  constexpr D3D12_RTV_DIMENSION rtvDim[] = {
      D3D12_RTV_DIMENSION_BUFFER,              // Buffer
      D3D12_RTV_DIMENSION_TEXTURE1D,           // Texture1D
      D3D12_RTV_DIMENSION_TEXTURE1DARRAY,      // Texture1DArray
      D3D12_RTV_DIMENSION_TEXTURE2D,           // Texture2D
      D3D12_RTV_DIMENSION_TEXTURE2DARRAY,      // Texture2DArray
      D3D12_RTV_DIMENSION_TEXTURE2DMS,         // Texture2DMS
      D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY,    // Texture2DMSArray
      D3D12_RTV_DIMENSION_TEXTURE3D,           // Texture3D
  };

  constexpr D3D12_DSV_DIMENSION dsvDim[] = {
      D3D12_DSV_DIMENSION_UNKNOWN,             // Buffer
      D3D12_DSV_DIMENSION_TEXTURE1D,           // Texture1D
      D3D12_DSV_DIMENSION_TEXTURE1DARRAY,      // Texture1DArray
      D3D12_DSV_DIMENSION_TEXTURE2D,           // Texture2D
      D3D12_DSV_DIMENSION_TEXTURE2DARRAY,      // Texture2DArray
      D3D12_DSV_DIMENSION_TEXTURE2DMS,         // Texture2DMS
      D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY,    // Texture2DMSArray
      D3D12_DSV_DIMENSION_UNKNOWN,             // Texture3D
  };

  constexpr D3D12_UAV_DIMENSION uavDim[] = {
      D3D12_UAV_DIMENSION_BUFFER,            // Buffer
      D3D12_UAV_DIMENSION_TEXTURE1D,         // Texture1D
      D3D12_UAV_DIMENSION_TEXTURE1DARRAY,    // Texture1DArray
      D3D12_UAV_DIMENSION_TEXTURE2D,         // Texture2D
      D3D12_UAV_DIMENSION_TEXTURE2DARRAY,    // Texture2DArray
      D3D12_UAV_DIMENSION_UNKNOWN,           // Texture2DMS
      D3D12_UAV_DIMENSION_UNKNOWN,           // Texture2DMSArray
      D3D12_UAV_DIMENSION_TEXTURE3D,         // Texture3D
  };

  if(viewType == ViewType::SRV)
  {
    desc.srv.ViewDimension = srvDim[(int)resType];

    if(resType == ResourceType::Buffer)
    {
      firstElement = &desc.srv.Buffer.FirstElement;
      numElements = &desc.srv.Buffer.NumElements;
    }
  }
  else if(viewType == ViewType::RTV)
  {
    desc.rtv.ViewDimension = rtvDim[(int)resType];

    if(resType == ResourceType::Buffer)
    {
      firstElement = &desc.rtv.Buffer.FirstElement;
      numElements = &desc.rtv.Buffer.NumElements;
    }
  }
  else if(viewType == ViewType::DSV)
  {
    desc.dsv.ViewDimension = dsvDim[(int)resType];

    if(desc.dsv.ViewDimension == D3D12_DSV_DIMENSION_UNKNOWN)
      TEST_FATAL("Unsupported resource for DSV");
  }
  else if(viewType == ViewType::UAV)
  {
    desc.uav.ViewDimension = uavDim[(int)resType];

    if(desc.uav.ViewDimension == D3D12_UAV_DIMENSION_UNKNOWN)
      TEST_FATAL("Unsupported resource for UAV");

    if(resType == ResourceType::Buffer)
    {
      firstElement = &desc.uav.Buffer.FirstElement;
      numElements = &desc.uav.Buffer.NumElements;
    }
  }

  UINT *pointers[4][8][5] = {
      // SRV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices, &planeSlice
          {NULL, NULL, NULL, NULL, NULL},
          {
              &desc.srv.Texture1D.MostDetailedMip, &desc.srv.Texture1D.MipLevels, NULL, NULL, NULL,
          },
          {
              &desc.srv.Texture1DArray.MostDetailedMip, &desc.srv.Texture1DArray.MipLevels,
              &desc.srv.Texture1DArray.FirstArraySlice, &desc.srv.Texture1DArray.ArraySize, NULL,
          },
          {
              &desc.srv.Texture2D.MostDetailedMip, &desc.srv.Texture2D.MipLevels, NULL, NULL,
              &desc.srv.Texture2D.PlaneSlice,
          },
          {
              &desc.srv.Texture2DArray.MostDetailedMip, &desc.srv.Texture2DArray.MipLevels,
              &desc.srv.Texture2DArray.FirstArraySlice, &desc.srv.Texture2DArray.ArraySize,
              &desc.srv.Texture2DArray.PlaneSlice,
          },
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, &desc.srv.Texture2DMSArray.FirstArraySlice,
              &desc.srv.Texture2DMSArray.ArraySize, NULL,
          },
          {
              &desc.srv.Texture3D.MostDetailedMip, &desc.srv.Texture2D.MipLevels, NULL, NULL, NULL,
          },
      },
      // RTV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              &desc.rtv.Texture1D.MipSlice, NULL, NULL, NULL, NULL,
          },
          {
              &desc.rtv.Texture1DArray.MipSlice, NULL, &desc.rtv.Texture1DArray.FirstArraySlice,
              &desc.rtv.Texture1DArray.ArraySize, NULL,
          },
          {
              &desc.rtv.Texture2D.MipSlice, NULL, NULL, NULL, &desc.rtv.Texture2D.PlaneSlice,
          },
          {
              &desc.rtv.Texture2DArray.MipSlice, NULL, &desc.rtv.Texture2DArray.FirstArraySlice,
              &desc.rtv.Texture2DArray.ArraySize, &desc.rtv.Texture2DArray.PlaneSlice,
          },
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, &desc.rtv.Texture2DMSArray.FirstArraySlice,
              &desc.rtv.Texture2DMSArray.ArraySize, NULL,
          },
          {
              &desc.rtv.Texture3D.MipSlice, NULL, &desc.rtv.Texture3D.FirstWSlice,
              &desc.rtv.Texture3D.WSize, NULL,
          },
      },
      // DSV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              &desc.dsv.Texture1D.MipSlice, NULL, NULL, NULL, NULL,
          },
          {
              &desc.dsv.Texture1DArray.MipSlice, NULL, &desc.dsv.Texture1DArray.FirstArraySlice,
              &desc.dsv.Texture1DArray.ArraySize, NULL,
          },
          {
              &desc.dsv.Texture2D.MipSlice, NULL, NULL, NULL, NULL,
          },
          {
              &desc.dsv.Texture2DArray.MipSlice, NULL, &desc.dsv.Texture2DArray.FirstArraySlice,
              &desc.dsv.Texture2DArray.ArraySize, NULL,
          },
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, &desc.dsv.Texture2DMSArray.FirstArraySlice,
              &desc.dsv.Texture2DMSArray.ArraySize, NULL,
          },
          {
              NULL, NULL, NULL, NULL, NULL,
          },
      },
      // UAV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture1D.MipSlice, NULL, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture1DArray.MipSlice, NULL, &desc.uav.Texture1DArray.FirstArraySlice,
              &desc.uav.Texture1DArray.ArraySize, NULL,
          },
          {
              &desc.uav.Texture2D.MipSlice, NULL, NULL, NULL, &desc.uav.Texture2D.PlaneSlice,
          },
          {
              &desc.uav.Texture2DArray.MipSlice, NULL, &desc.uav.Texture2DArray.FirstArraySlice,
              &desc.uav.Texture2DArray.ArraySize, &desc.uav.Texture2DArray.PlaneSlice,
          },
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture3D.MipSlice, NULL, &desc.uav.Texture3D.FirstWSlice,
              &desc.uav.Texture3D.WSize, NULL,
          },
      },
  };

  if(resType != ResourceType::Buffer)
  {
    firstMip = pointers[(int)viewType][(int)resType][0];
    numMips = pointers[(int)viewType][(int)resType][1];
    firstSlice = pointers[(int)viewType][(int)resType][2];
    numSlices = pointers[(int)viewType][(int)resType][3];
    planeSlice = pointers[(int)viewType][(int)resType][4];

    if(numMips)
      *numMips = ~0U;
    if(numSlices)
      *numSlices = ~0U;
  }
}

D3D12ViewCreator &D3D12ViewCreator::Format(DXGI_FORMAT f)
{
  // this is always in the same place, just write it once
  desc.srv.Format = f;
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::FirstElement(UINT el)
{
  if(firstElement)
    *firstElement = el;
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::NumElements(UINT num)
{
  if(numElements)
    *numElements = num;
  else
    TEST_ERROR("This view & resource doesn't support NumElements");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::StructureStride(UINT stride)
{
  if(m_Type == ViewType::UAV)
    desc.uav.Buffer.StructureByteStride = stride;
  else if(m_Type == ViewType::SRV)
    desc.srv.Buffer.StructureByteStride = stride;
  else
    TEST_ERROR("This view & resource doesn't support StructureStride");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::ByteAddressed()
{
  if(m_Type == ViewType::UAV)
    desc.uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
  else if(m_Type == ViewType::SRV)
    desc.srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
  else
    TEST_ERROR("This view & resource doesn't support ByteAddressed");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::FirstMip(UINT mip)
{
  if(firstMip)
    *firstMip = mip;
  else
    TEST_ERROR("This view & resource doesn't support FirstMip");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::NumMips(UINT num)
{
  if(numMips)
    *numMips = num;
  else
    TEST_ERROR("This view & resource doesn't support NumMips");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::FirstSlice(UINT mip)
{
  if(firstSlice)
    *firstSlice = mip;
  else
    TEST_ERROR("This view & resource doesn't support FirstSlice");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::NumSlices(UINT num)
{
  if(numSlices)
    *numSlices = num;
  else
    TEST_ERROR("This view & resource doesn't support NumSlices");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::Swizzle(UINT swizzle)
{
  Shader4ComponentMapping = swizzle;
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::PlaneSlice(UINT plane)
{
  if(planeSlice)
    *planeSlice = plane;
  else
    TEST_ERROR("This view & resource doesn't support NumSlices");
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::ReadOnlyDepth()
{
  desc.dsv.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
  return *this;
}

D3D12ViewCreator &D3D12ViewCreator::ReadOnlyStencil()
{
  desc.dsv.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
  return *this;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12ViewCreator::CreateCPU(ID3D12DescriptorHeap *heap,
                                                        uint32_t descriptor)
{
  static UINT increment[] = {
      m_Test->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
      0,    // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
      m_Test->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
      m_Test->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
  };

  D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap->GetCPUDescriptorHandleForHeapStart();

  TEST_ASSERT(m_Res, "Must have resource");

  if(m_Type == ViewType::DSV)
  {
    cpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] * descriptor;
    m_Test->dev->CreateDepthStencilView(m_Res, &desc.dsv, cpu);
  }
  else if(m_Type == ViewType::RTV)
  {
    cpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] * descriptor;
    m_Test->dev->CreateRenderTargetView(m_Res, &desc.rtv, cpu);
  }
  else if(m_Type == ViewType::SRV)
  {
    if(desc.uav.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
    {
      D3D12_RESOURCE_DESC bufdesc = m_Res->GetDesc();

      UINT elementStride = desc.srv.Buffer.StructureByteStride;

      if(desc.srv.Buffer.StructureByteStride == 0 && desc.srv.Format == DXGI_FORMAT_UNKNOWN)
        TEST_FATAL("Can't create srv on non-structured buffer with no format");

      if(desc.srv.Format != DXGI_FORMAT_UNKNOWN)
        elementStride = formatStrides[desc.srv.Format];

      if(desc.srv.Buffer.NumElements == 0)
        desc.srv.Buffer.NumElements = UINT(bufdesc.Width / std::max(elementStride, 1U));
    }

    desc.srv.Shader4ComponentMapping = Shader4ComponentMapping;

    cpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * descriptor;
    m_Test->dev->CreateShaderResourceView(m_Res, &desc.srv, cpu);
  }
  else if(m_Type == ViewType::UAV)
  {
    if(desc.uav.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
    {
      D3D12_RESOURCE_DESC bufdesc = m_Res->GetDesc();

      UINT elementStride = desc.uav.Buffer.StructureByteStride;

      if(desc.uav.Buffer.StructureByteStride == 0 && desc.uav.Format == DXGI_FORMAT_UNKNOWN)
        TEST_FATAL("Can't create uav on non-structured buffer with no format");

      if(desc.uav.Format != DXGI_FORMAT_UNKNOWN)
        elementStride = formatStrides[desc.uav.Format];

      if(desc.uav.Buffer.NumElements == 0)
        desc.uav.Buffer.NumElements = UINT(bufdesc.Width / std::max(elementStride, 1U));
    }

    cpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * descriptor;
    m_Test->dev->CreateUnorderedAccessView(m_Res, NULL, &desc.uav, cpu);
  }

  return cpu;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12ViewCreator::CreateGPU(ID3D12DescriptorHeap *heap,
                                                        uint32_t descriptor)
{
  CreateCPU(heap, descriptor);

  static UINT increment[] = {
      m_Test->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
      0,    // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
      m_Test->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
      m_Test->dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
  };

  D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap->GetGPUDescriptorHandleForHeapStart();

  if(m_Type == ViewType::DSV)
    gpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] * descriptor;
  else if(m_Type == ViewType::RTV)
    gpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] * descriptor;
  else
    gpu.ptr += increment[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] * descriptor;

  return gpu;
}

D3D12PSOCreator::D3D12PSOCreator(D3D12GraphicsTest *test) : m_Test(test)
{
  GraphicsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  GraphicsDesc.SampleMask = 0xFFFFFFFF;
  GraphicsDesc.SampleDesc.Count = 1;
  GraphicsDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  GraphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  GraphicsDesc.NumRenderTargets = 1;
  GraphicsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  GraphicsDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  GraphicsDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
  GraphicsDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
  GraphicsDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  GraphicsDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
  GraphicsDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
  GraphicsDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  GraphicsDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
  GraphicsDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

D3D12PSOCreator &D3D12PSOCreator::VS(ID3DBlobPtr blob)
{
  GraphicsDesc.VS.pShaderBytecode = blob->GetBufferPointer();
  GraphicsDesc.VS.BytecodeLength = blob->GetBufferSize();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::HS(ID3DBlobPtr blob)
{
  GraphicsDesc.HS.pShaderBytecode = blob->GetBufferPointer();
  GraphicsDesc.HS.BytecodeLength = blob->GetBufferSize();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::DS(ID3DBlobPtr blob)
{
  GraphicsDesc.DS.pShaderBytecode = blob->GetBufferPointer();
  GraphicsDesc.DS.BytecodeLength = blob->GetBufferSize();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::GS(ID3DBlobPtr blob)
{
  GraphicsDesc.GS.pShaderBytecode = blob->GetBufferPointer();
  GraphicsDesc.GS.BytecodeLength = blob->GetBufferSize();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::PS(ID3DBlobPtr blob)
{
  GraphicsDesc.PS.pShaderBytecode = blob->GetBufferPointer();
  GraphicsDesc.PS.BytecodeLength = blob->GetBufferSize();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::CS(ID3DBlobPtr blob)
{
  ComputeDesc.CS.pShaderBytecode = blob->GetBufferPointer();
  ComputeDesc.CS.BytecodeLength = blob->GetBufferSize();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC> &elements)
{
  GraphicsDesc.InputLayout.NumElements = (UINT)elements.size();
  GraphicsDesc.InputLayout.pInputElementDescs = elements.data();
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::InputLayout()
{
  return InputLayout(m_Test->DefaultInputLayout());
}

D3D12PSOCreator &D3D12PSOCreator::RootSig(ID3D12RootSignaturePtr rootSig)
{
  GraphicsDesc.pRootSignature = rootSig;
  ComputeDesc.pRootSignature = rootSig;
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::RTVs(const std::vector<DXGI_FORMAT> &fmts)
{
  memset(GraphicsDesc.RTVFormats, 0, sizeof(GraphicsDesc.RTVFormats));
  GraphicsDesc.NumRenderTargets = (UINT)fmts.size();
  for(size_t i = 0; i < 8 && i < fmts.size(); i++)
    GraphicsDesc.RTVFormats[i] = fmts[i];
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::DSV(DXGI_FORMAT fmt)
{
  GraphicsDesc.DSVFormat = fmt;
  return *this;
}

D3D12PSOCreator &D3D12PSOCreator::SampleCount(UINT Samples)
{
  GraphicsDesc.SampleDesc.Count = Samples;
  return *this;
}

D3D12PSOCreator::operator ID3D12PipelineStatePtr() const
{
  ID3D12PipelineStatePtr pso;
  if(ComputeDesc.CS.BytecodeLength > 0)
  {
    CHECK_HR(m_Test->dev->CreateComputePipelineState(&ComputeDesc, __uuidof(ID3D12PipelineState),
                                                     (void **)&pso));
  }
  else
  {
    CHECK_HR(m_Test->dev->CreateGraphicsPipelineState(&GraphicsDesc, __uuidof(ID3D12PipelineState),
                                                      (void **)&pso));
  }
  return pso;
}