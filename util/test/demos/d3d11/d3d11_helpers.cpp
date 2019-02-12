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

#include "d3d11_helpers.h"
#include "d3d11_test.h"

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

D3D11BufferCreator::D3D11BufferCreator(D3D11GraphicsTest *test) : m_Test(test)
{
  m_BufDesc.ByteWidth = 0;
  m_BufDesc.MiscFlags = 0;
  m_BufDesc.StructureByteStride = 0;
  m_BufDesc.CPUAccessFlags = 0;
  m_BufDesc.Usage = D3D11_USAGE_DEFAULT;
  m_BufDesc.BindFlags = 0;
}

D3D11BufferCreator &D3D11BufferCreator::Vertex()
{
  m_BufDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Index()
{
  m_BufDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Constant()
{
  m_BufDesc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::StreamOut()
{
  m_BufDesc.BindFlags |= D3D11_BIND_STREAM_OUTPUT;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::SRV()
{
  m_BufDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::UAV()
{
  m_BufDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Structured(UINT structStride)
{
  if(structStride > 0 && (m_BufDesc.ByteWidth % structStride) != 0)
    TEST_FATAL("Invalid structure size - not divisor of byte size");

  m_BufDesc.StructureByteStride = structStride;
  m_BufDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::ByteAddressed()
{
  m_BufDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Mappable()
{
  m_BufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  m_BufDesc.Usage = D3D11_USAGE_DYNAMIC;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Staging()
{
  m_BufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  m_BufDesc.Usage = D3D11_USAGE_STAGING;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Shared()
{
  m_BufDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Data(const void *data)
{
  m_Initdata.pSysMem = data;
  m_Initdata.SysMemPitch = m_BufDesc.ByteWidth;
  m_Initdata.SysMemSlicePitch = m_BufDesc.ByteWidth;
  return *this;
}

D3D11BufferCreator &D3D11BufferCreator::Size(UINT size)
{
  if(m_BufDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)
    size = (size + 15) & ~0xf;

  m_BufDesc.ByteWidth = size;

  m_Initdata.SysMemPitch = size;
  m_Initdata.SysMemSlicePitch = size;
  return *this;
}

D3D11BufferCreator::operator ID3D11BufferPtr() const
{
  ID3D11BufferPtr buf;
  CHECK_HR(m_Test->dev->CreateBuffer(&m_BufDesc, m_Initdata.pSysMem ? &m_Initdata : NULL, &buf));
  return buf;
}

D3D11TextureCreator::D3D11TextureCreator(D3D11GraphicsTest *test, DXGI_FORMAT format, UINT width,
                                         UINT height, UINT depth)
    : m_Test(test)
{
  Format = format;
  Width = width;
  Height = height;
  depth = depth;
}

D3D11TextureCreator &D3D11TextureCreator::Mips(UINT mips)
{
  MipLevels = mips;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::Array(UINT size)
{
  ArraySize = size;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::Multisampled(UINT count, UINT quality)
{
  SampleDesc.Count = count;
  SampleDesc.Quality = quality;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::SRV()
{
  BindFlags |= D3D11_BIND_SHADER_RESOURCE;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::UAV()
{
  BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::RTV()
{
  BindFlags |= D3D11_BIND_RENDER_TARGET;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::DSV()
{
  BindFlags |= D3D11_BIND_DEPTH_STENCIL;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::Mappable()
{
  CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  Usage = D3D11_USAGE_DYNAMIC;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::Staging()
{
  CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  Usage = D3D11_USAGE_STAGING;
  return *this;
}

D3D11TextureCreator &D3D11TextureCreator::Shared()
{
  MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
  return *this;
}

D3D11TextureCreator::operator ID3D11Texture1DPtr() const
{
  D3D11_TEXTURE1D_DESC texdesc;

  texdesc.Width = Width;
  texdesc.ArraySize = ArraySize;
  texdesc.MipLevels = MipLevels;
  texdesc.MiscFlags = MiscFlags;
  texdesc.CPUAccessFlags = CPUAccessFlags;
  texdesc.Usage = Usage;
  texdesc.BindFlags = BindFlags;
  texdesc.Format = Format;

  ID3D11Texture1DPtr tex;
  CHECK_HR(m_Test->dev->CreateTexture1D(&texdesc, NULL, &tex));
  return tex;
}

D3D11TextureCreator::operator ID3D11Texture2DPtr() const
{
  D3D11_TEXTURE2D_DESC texdesc;

  texdesc.Width = Width;
  texdesc.Height = Height;
  texdesc.ArraySize = ArraySize;
  texdesc.MipLevels = MipLevels;
  texdesc.MiscFlags = MiscFlags;
  texdesc.CPUAccessFlags = CPUAccessFlags;
  texdesc.Usage = Usage;
  texdesc.BindFlags = BindFlags;
  texdesc.Format = Format;
  texdesc.SampleDesc.Count = SampleDesc.Count;
  texdesc.SampleDesc.Quality = SampleDesc.Quality;

  ID3D11Texture2DPtr tex;
  CHECK_HR(m_Test->dev->CreateTexture2D(&texdesc, NULL, &tex));
  return tex;
}

D3D11TextureCreator::operator ID3D11Texture3DPtr() const
{
  D3D11_TEXTURE3D_DESC texdesc;

  texdesc.Width = Width;
  texdesc.Height = Height;
  texdesc.Depth = Height;
  texdesc.MipLevels = MipLevels;
  texdesc.MiscFlags = MiscFlags;
  texdesc.CPUAccessFlags = CPUAccessFlags;
  texdesc.Usage = Usage;
  texdesc.BindFlags = BindFlags;
  texdesc.Format = Format;

  ID3D11Texture3DPtr tex;
  CHECK_HR(m_Test->dev->CreateTexture3D(&texdesc, NULL, &tex));
  return tex;
}

D3D11ViewCreator::D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Buffer *buf)
    : m_Test(test), m_Type(viewType), m_Res(buf)
{
  SetupDescriptors(viewType, ResourceType::Buffer);
}

D3D11ViewCreator::D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Texture1D *tex)
    : m_Test(test), m_Type(viewType), m_Res(tex)
{
  D3D11_TEXTURE1D_DESC texdesc;
  tex->GetDesc(&texdesc);

  ResourceType resType = ResourceType::Texture1D;

  if(texdesc.ArraySize > 1)
    resType = ResourceType::Texture1DArray;

  SetupDescriptors(viewType, resType);

  Format(texdesc.Format);
}

D3D11ViewCreator::D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Texture2D *tex)
    : m_Test(test), m_Type(viewType), m_Res(tex)
{
  D3D11_TEXTURE2D_DESC texdesc;
  tex->GetDesc(&texdesc);

  ResourceType resType;

  if(texdesc.SampleDesc.Count > 1)
  {
    resType = ResourceType::Texture2DMS;
    if(texdesc.ArraySize > 1)
      resType = ResourceType::Texture2DMSArray;
  }
  else
  {
    resType = ResourceType::Texture2D;
    if(texdesc.ArraySize > 1)
      resType = ResourceType::Texture2DArray;
  }

  SetupDescriptors(viewType, resType);

  Format(texdesc.Format);
}

D3D11ViewCreator::D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Texture3D *tex)
    : m_Test(test), m_Type(viewType), m_Res(tex)
{
  D3D11_TEXTURE3D_DESC texdesc;
  tex->GetDesc(&texdesc);

  SetupDescriptors(viewType, ResourceType::Texture3D);

  Format(texdesc.Format);
}

void D3D11ViewCreator::SetupDescriptors(ViewType viewType, ResourceType resType)
{
  memset(&desc, 0, sizeof(desc));

  constexpr D3D11_SRV_DIMENSION srvDim[] = {
      D3D11_SRV_DIMENSION_BUFFER,              // Buffer
      D3D11_SRV_DIMENSION_TEXTURE1D,           // Texture1D
      D3D11_SRV_DIMENSION_TEXTURE1DARRAY,      // Texture1DArray
      D3D11_SRV_DIMENSION_TEXTURE2D,           // Texture2D
      D3D11_SRV_DIMENSION_TEXTURE2DARRAY,      // Texture2DArray
      D3D11_SRV_DIMENSION_TEXTURE2DMS,         // Texture2DMS
      D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY,    // Texture2DMSArray
      D3D11_SRV_DIMENSION_TEXTURE3D,           // Texture3D
  };

  constexpr D3D11_RTV_DIMENSION rtvDim[] = {
      D3D11_RTV_DIMENSION_BUFFER,              // Buffer
      D3D11_RTV_DIMENSION_TEXTURE1D,           // Texture1D
      D3D11_RTV_DIMENSION_TEXTURE1DARRAY,      // Texture1DArray
      D3D11_RTV_DIMENSION_TEXTURE2D,           // Texture2D
      D3D11_RTV_DIMENSION_TEXTURE2DARRAY,      // Texture2DArray
      D3D11_RTV_DIMENSION_TEXTURE2DMS,         // Texture2DMS
      D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY,    // Texture2DMSArray
      D3D11_RTV_DIMENSION_TEXTURE3D,           // Texture3D
  };

  constexpr D3D11_DSV_DIMENSION dsvDim[] = {
      D3D11_DSV_DIMENSION_UNKNOWN,             // Buffer
      D3D11_DSV_DIMENSION_TEXTURE1D,           // Texture1D
      D3D11_DSV_DIMENSION_TEXTURE1DARRAY,      // Texture1DArray
      D3D11_DSV_DIMENSION_TEXTURE2D,           // Texture2D
      D3D11_DSV_DIMENSION_TEXTURE2DARRAY,      // Texture2DArray
      D3D11_DSV_DIMENSION_TEXTURE2DMS,         // Texture2DMS
      D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY,    // Texture2DMSArray
      D3D11_DSV_DIMENSION_UNKNOWN,             // Texture3D
  };

  constexpr D3D11_UAV_DIMENSION uavDim[] = {
      D3D11_UAV_DIMENSION_BUFFER,            // Buffer
      D3D11_UAV_DIMENSION_TEXTURE1D,         // Texture1D
      D3D11_UAV_DIMENSION_TEXTURE1DARRAY,    // Texture1DArray
      D3D11_UAV_DIMENSION_TEXTURE2D,         // Texture2D
      D3D11_UAV_DIMENSION_TEXTURE2DARRAY,    // Texture2DArray
      D3D11_UAV_DIMENSION_UNKNOWN,           // Texture2DMS
      D3D11_UAV_DIMENSION_UNKNOWN,           // Texture2DMSArray
      D3D11_UAV_DIMENSION_TEXTURE3D,         // Texture3D
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

    if(desc.dsv.ViewDimension == D3D11_DSV_DIMENSION_UNKNOWN)
      TEST_FATAL("Unsupported resource for DSV");
  }
  else if(viewType == ViewType::UAV)
  {
    desc.uav.ViewDimension = uavDim[(int)resType];

    if(desc.uav.ViewDimension == D3D11_UAV_DIMENSION_UNKNOWN)
      TEST_FATAL("Unsupported resource for UAV");

    if(resType == ResourceType::Buffer)
    {
      firstElement = &desc.uav.Buffer.FirstElement;
      numElements = &desc.uav.Buffer.NumElements;
    }
  }

  UINT *pointers[4][8][4] = {
      // SRV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL,
          },
          {
              &desc.srv.Texture1D.MostDetailedMip, &desc.srv.Texture1D.MipLevels, NULL, NULL,
          },
          {
              &desc.srv.Texture1DArray.MostDetailedMip, &desc.srv.Texture1DArray.MipLevels,
              &desc.srv.Texture1DArray.FirstArraySlice, &desc.srv.Texture1DArray.ArraySize,
          },
          {
              &desc.srv.Texture2D.MostDetailedMip, &desc.srv.Texture2D.MipLevels, NULL, NULL,
          },
          {
              &desc.srv.Texture2DArray.MostDetailedMip, &desc.srv.Texture2DArray.MipLevels,
              &desc.srv.Texture2DArray.FirstArraySlice, &desc.srv.Texture2DArray.ArraySize,
          },
          {
              NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, &desc.srv.Texture2DMSArray.FirstArraySlice,
              &desc.srv.Texture2DMSArray.ArraySize,
          },
          {
              &desc.srv.Texture3D.MostDetailedMip, &desc.srv.Texture2D.MipLevels, NULL, NULL,
          },
      },
      // RTV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL,
          },
          {
              &desc.rtv.Texture1D.MipSlice, NULL, NULL, NULL,
          },
          {
              &desc.rtv.Texture1DArray.MipSlice, NULL, &desc.rtv.Texture1DArray.FirstArraySlice,
              &desc.rtv.Texture1DArray.ArraySize,
          },
          {
              &desc.rtv.Texture2D.MipSlice, NULL, NULL, NULL,
          },
          {
              &desc.rtv.Texture2DArray.MipSlice, NULL, &desc.rtv.Texture2DArray.FirstArraySlice,
              &desc.rtv.Texture2DArray.ArraySize,
          },
          {
              NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, &desc.rtv.Texture2DMSArray.FirstArraySlice,
              &desc.rtv.Texture2DMSArray.ArraySize,
          },
          {
              &desc.rtv.Texture3D.MipSlice, NULL, &desc.rtv.Texture3D.FirstWSlice,
              &desc.rtv.Texture3D.WSize,
          },
      },
      // DSV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL,
          },
          {
              &desc.dsv.Texture1D.MipSlice, NULL, NULL, NULL,
          },
          {
              &desc.dsv.Texture1DArray.MipSlice, NULL, &desc.dsv.Texture1DArray.FirstArraySlice,
              &desc.dsv.Texture1DArray.ArraySize,
          },
          {
              &desc.dsv.Texture2D.MipSlice, NULL, NULL, NULL,
          },
          {
              &desc.dsv.Texture2DArray.MipSlice, NULL, &desc.dsv.Texture2DArray.FirstArraySlice,
              &desc.dsv.Texture2DArray.ArraySize,
          },
          {
              NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, &desc.dsv.Texture2DMSArray.FirstArraySlice,
              &desc.dsv.Texture2DMSArray.ArraySize,
          },
          {
              NULL, NULL, NULL, NULL,
          },
      },
      // UAV
      {
          // &firstMip, &numMips, &firstSlice, &numSlices
          {
              NULL, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture1D.MipSlice, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture1DArray.MipSlice, NULL, &desc.uav.Texture1DArray.FirstArraySlice,
              &desc.uav.Texture1DArray.ArraySize,
          },
          {
              &desc.uav.Texture2D.MipSlice, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture2DArray.MipSlice, NULL, &desc.uav.Texture2DArray.FirstArraySlice,
              &desc.uav.Texture2DArray.ArraySize,
          },
          {
              NULL, NULL, NULL, NULL,
          },
          {
              NULL, NULL, NULL, NULL,
          },
          {
              &desc.uav.Texture3D.MipSlice, NULL, &desc.uav.Texture3D.FirstWSlice,
              &desc.uav.Texture3D.WSize,
          },
      },
  };

  if(resType != ResourceType::Buffer)
  {
    firstMip = pointers[(int)viewType][(int)resType][0];
    numMips = pointers[(int)viewType][(int)resType][1];
    firstSlice = pointers[(int)viewType][(int)resType][2];
    numSlices = pointers[(int)viewType][(int)resType][3];

    if(numMips)
      *numMips = ~0U;
    if(numSlices)
      *numSlices = ~0U;
  }
}

D3D11ViewCreator &D3D11ViewCreator::Format(DXGI_FORMAT f)
{
  // this is always in the same place, just write it once
  desc.srv.Format = f;
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::FirstElement(UINT el)
{
  if(firstElement)
    *firstElement = el;
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::NumElements(UINT num)
{
  if(numElements)
    *numElements = num;
  else
    TEST_ERROR("This view & resource doesn't support NumElements");
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::FirstMip(UINT mip)
{
  if(firstMip)
    *firstMip = mip;
  else
    TEST_ERROR("This view & resource doesn't support FirstMip");
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::NumMips(UINT num)
{
  if(numMips)
    *numMips = num;
  else
    TEST_ERROR("This view & resource doesn't support NumMips");
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::FirstSlice(UINT mip)
{
  if(firstSlice)
    *firstSlice = mip;
  else
    TEST_ERROR("This view & resource doesn't support FirstSlice");
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::NumSlices(UINT num)
{
  if(numSlices)
    *numSlices = num;
  else
    TEST_ERROR("This view & resource doesn't support NumSlices");
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::ReadOnlyDepth()
{
  desc.dsv.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
  return *this;
}

D3D11ViewCreator &D3D11ViewCreator::ReadOnlyStencil()
{
  desc.dsv.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
  return *this;
}

D3D11ViewCreator::operator ID3D11ShaderResourceViewPtr()
{
  if(desc.srv.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
  {
    D3D11_BUFFER_DESC bufdesc;
    ((ID3D11Buffer *)m_Res)->GetDesc(&bufdesc);

    if(bufdesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
      desc.srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
      desc.srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    }

    UINT elementStride = bufdesc.StructureByteStride;

    if(bufdesc.StructureByteStride == 0 && desc.srv.Format == DXGI_FORMAT_UNKNOWN)
      TEST_FATAL("Can't create SRV on non-structured buffer with no format");

    if(desc.srv.Format != DXGI_FORMAT_UNKNOWN)
      elementStride = formatStrides[desc.srv.Format];

    if(desc.srv.Buffer.NumElements == 0)
      desc.srv.Buffer.NumElements = bufdesc.ByteWidth / std::max(elementStride, 1U);
  }

  TEST_ASSERT(m_Res, "Must have resource");
  TEST_ASSERT(m_Type == ViewType::SRV, "Casting non-SRV ViewCreator to SRV");

  ID3D11ShaderResourceViewPtr srv;
  CHECK_HR(m_Test->dev->CreateShaderResourceView(m_Res, &desc.srv, &srv));
  return srv;
}

D3D11ViewCreator::operator ID3D11UnorderedAccessViewPtr()
{
  if(desc.uav.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
  {
    D3D11_BUFFER_DESC bufdesc;
    ((ID3D11Buffer *)m_Res)->GetDesc(&bufdesc);

    if(bufdesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
      desc.uav.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    UINT elementStride = bufdesc.StructureByteStride;

    if(bufdesc.StructureByteStride == 0 && desc.uav.Format == DXGI_FORMAT_UNKNOWN)
      TEST_FATAL("Can't create uav on non-structured buffer with no format");

    if(desc.uav.Format != DXGI_FORMAT_UNKNOWN)
      elementStride = formatStrides[desc.uav.Format];

    if(desc.uav.Buffer.NumElements == 0)
      desc.uav.Buffer.NumElements = bufdesc.ByteWidth / std::max(elementStride, 1U);
  }

  TEST_ASSERT(m_Res, "Must have resource");
  TEST_ASSERT(m_Type == ViewType::UAV, "Casting non-UAV ViewCreator to UAV");

  ID3D11UnorderedAccessViewPtr uav;
  CHECK_HR(m_Test->dev->CreateUnorderedAccessView(m_Res, &desc.uav, &uav));
  return uav;
}

D3D11ViewCreator::operator ID3D11RenderTargetViewPtr()
{
  TEST_ASSERT(m_Res, "Must have resource");
  TEST_ASSERT(m_Type == ViewType::RTV, "Casting non-RTV ViewCreator to RTV");

  ID3D11RenderTargetViewPtr rtv;
  CHECK_HR(m_Test->dev->CreateRenderTargetView(m_Res, &desc.rtv, &rtv));
  return rtv;
}

D3D11ViewCreator::operator ID3D11DepthStencilViewPtr()
{
  TEST_ASSERT(m_Res, "Must have resource");
  TEST_ASSERT(m_Type == ViewType::DSV, "Casting non-DSV ViewCreator to DSV");

  ID3D11DepthStencilViewPtr dsv;
  CHECK_HR(m_Test->dev->CreateDepthStencilView(m_Res, &desc.dsv, &dsv));
  return dsv;
}

D3D11SamplerCreator::D3D11SamplerCreator(D3D11GraphicsTest *test)
{
  m_Test = test;

  m_Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  m_Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  m_Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  m_Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  m_Desc.MipLODBias = 0.0f;
  m_Desc.MaxAnisotropy = 1;
  m_Desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  m_Desc.BorderColor[0] = 1.0f;
  m_Desc.BorderColor[1] = 1.0f;
  m_Desc.BorderColor[2] = 1.0f;
  m_Desc.BorderColor[3] = 1.0f;
  m_Desc.MinLOD = -FLT_MAX;
  m_Desc.MaxLOD = FLT_MAX;
}

D3D11SamplerCreator::operator ID3D11SamplerStatePtr() const
{
  ID3D11SamplerStatePtr samp;
  CHECK_HR(m_Test->dev->CreateSamplerState(&m_Desc, &samp));
  return samp;
}