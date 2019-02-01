/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "common/common.h"
#include "serialise/serialiser.h"
#include "d3d11_common.h"
#include "d3d11_manager.h"
#include "d3d11_resources.h"

// serialisation of object handles via IDs.
template <class SerialiserType, class Interface>
void DoSerialiseViaResourceId(SerialiserType &ser, Interface *&el)
{
  D3D11ResourceManager *rm = (D3D11ResourceManager *)ser.GetUserData();

  ResourceId id;

  if(ser.IsWriting() && rm)
    id = GetIDForResource(el);

  DoSerialise(ser, id);

  if(ser.IsReading())
  {
    if(id != ResourceId() && rm && rm->HasLiveResource(id))
      el = (Interface *)rm->GetLiveResource(id);
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
void DoSerialise(SerialiserType &ser, D3D11_BUFFER_DESC &el)
{
  SERIALISE_MEMBER(ByteWidth);
  SERIALISE_MEMBER(Usage);
  SERIALISE_MEMBER_TYPED(D3D11_BIND_FLAG, BindFlags);
  SERIALISE_MEMBER_TYPED(D3D11_CPU_ACCESS_FLAG, CPUAccessFlags);
  SERIALISE_MEMBER_TYPED(D3D11_RESOURCE_MISC_FLAG, MiscFlags);
  SERIALISE_MEMBER(StructureByteStride);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXTURE1D_DESC &el)
{
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(Usage);
  SERIALISE_MEMBER_TYPED(D3D11_BIND_FLAG, BindFlags);
  SERIALISE_MEMBER_TYPED(D3D11_CPU_ACCESS_FLAG, CPUAccessFlags);
  SERIALISE_MEMBER_TYPED(D3D11_RESOURCE_MISC_FLAG, MiscFlags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXTURE2D_DESC &el)
{
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(SampleDesc);
  SERIALISE_MEMBER(Usage);
  SERIALISE_MEMBER_TYPED(D3D11_BIND_FLAG, BindFlags);
  SERIALISE_MEMBER_TYPED(D3D11_CPU_ACCESS_FLAG, CPUAccessFlags);
  SERIALISE_MEMBER_TYPED(D3D11_RESOURCE_MISC_FLAG, MiscFlags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXTURE2D_DESC1 &el)
{
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(SampleDesc);
  SERIALISE_MEMBER(Usage);
  SERIALISE_MEMBER_TYPED(D3D11_BIND_FLAG, BindFlags);
  SERIALISE_MEMBER_TYPED(D3D11_CPU_ACCESS_FLAG, CPUAccessFlags);
  SERIALISE_MEMBER_TYPED(D3D11_RESOURCE_MISC_FLAG, MiscFlags);
  SERIALISE_MEMBER(TextureLayout);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXTURE3D_DESC &el)
{
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(Depth);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(Usage);
  SERIALISE_MEMBER_TYPED(D3D11_BIND_FLAG, BindFlags);
  SERIALISE_MEMBER_TYPED(D3D11_CPU_ACCESS_FLAG, CPUAccessFlags);
  SERIALISE_MEMBER_TYPED(D3D11_RESOURCE_MISC_FLAG, MiscFlags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXTURE3D_DESC1 &el)
{
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(Depth);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(Usage);
  SERIALISE_MEMBER_TYPED(D3D11_BIND_FLAG, BindFlags);
  SERIALISE_MEMBER_TYPED(D3D11_CPU_ACCESS_FLAG, CPUAccessFlags);
  SERIALISE_MEMBER_TYPED(D3D11_RESOURCE_MISC_FLAG, MiscFlags);
  SERIALISE_MEMBER(TextureLayout);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BUFFER_SRV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BUFFEREX_SRV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
  SERIALISE_MEMBER_TYPED(D3D11_BUFFEREX_SRV_FLAG, Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_SRV1 &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_SRV1 &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX3D_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXCUBE_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEXCUBE_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(MostDetailedMip);
  SERIALISE_MEMBER(MipLevels);
  SERIALISE_MEMBER(First2DArrayFace);
  SERIALISE_MEMBER(NumCubes);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2DMS_SRV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2DMS_ARRAY_SRV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_SHADER_RESOURCE_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_UNKNOWN: break;
    case D3D11_SRV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D11_SRV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_SRV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D11_SRV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE: SERIALISE_MEMBER(TextureCube); break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: SERIALISE_MEMBER(TextureCubeArray); break;
    case D3D11_SRV_DIMENSION_BUFFEREX: SERIALISE_MEMBER(BufferEx); break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_SHADER_RESOURCE_VIEW_DESC1 &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_SRV_DIMENSION_UNKNOWN: break;
    case D3D11_SRV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D11_SRV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_SRV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_SRV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_SRV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D11_SRV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    case D3D11_SRV_DIMENSION_TEXTURECUBE: SERIALISE_MEMBER(TextureCube); break;
    case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY: SERIALISE_MEMBER(TextureCubeArray); break;
    case D3D11_SRV_DIMENSION_BUFFEREX: SERIALISE_MEMBER(BufferEx); break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BUFFER_RTV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_ARRAY_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2DMS_RTV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2DMS_ARRAY_RTV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_RTV1 &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_RTV1 &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX3D_RTV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstWSlice);
  SERIALISE_MEMBER(WSize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RENDER_TARGET_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_UNKNOWN: break;
    case D3D11_RTV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D11_RTV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_RTV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D11_RTV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RENDER_TARGET_VIEW_DESC1 &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_RTV_DIMENSION_UNKNOWN: break;
    case D3D11_RTV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D11_RTV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_RTV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    case D3D11_RTV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BUFFER_UAV &el)
{
  SERIALISE_MEMBER(FirstElement);
  SERIALISE_MEMBER(NumElements);
  SERIALISE_MEMBER_TYPED(D3D11_BUFFER_UAV_FLAG, Flags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_ARRAY_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_UAV1 &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_UAV1 &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
  SERIALISE_MEMBER(PlaneSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX3D_UAV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstWSlice);
  SERIALISE_MEMBER(WSize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_UNORDERED_ACCESS_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_UNKNOWN: break;
    case D3D11_UAV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D11_UAV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_UAV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_UAV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_UNORDERED_ACCESS_VIEW_DESC1 &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D11_UAV_DIMENSION_UNKNOWN: break;
    case D3D11_UAV_DIMENSION_BUFFER: SERIALISE_MEMBER(Buffer); break;
    case D3D11_UAV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_UAV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_UAV_DIMENSION_TEXTURE3D: SERIALISE_MEMBER(Texture3D); break;
    default: RDCERR("Unrecognised UAV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX1D_ARRAY_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2D_ARRAY_DSV &el)
{
  SERIALISE_MEMBER(MipSlice);
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2DMS_DSV &el)
{
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_TEX2DMS_ARRAY_DSV &el)
{
  SERIALISE_MEMBER(FirstArraySlice);
  SERIALISE_MEMBER(ArraySize);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_DEPTH_STENCIL_VIEW_DESC &el)
{
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(ViewDimension);
  SERIALISE_MEMBER_TYPED(D3D11_DSV_FLAG, Flags);

  switch(el.ViewDimension)
  {
    case D3D11_DSV_DIMENSION_UNKNOWN: break;
    case D3D11_DSV_DIMENSION_TEXTURE1D: SERIALISE_MEMBER(Texture1D); break;
    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: SERIALISE_MEMBER(Texture1DArray); break;
    case D3D11_DSV_DIMENSION_TEXTURE2D: SERIALISE_MEMBER(Texture2D); break;
    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: SERIALISE_MEMBER(Texture2DArray); break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMS: SERIALISE_MEMBER(Texture2DMS); break;
    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: SERIALISE_MEMBER(Texture2DMSArray); break;
    default: RDCERR("Unrecognised DSV Dimension %d", el.ViewDimension); break;
  }
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RENDER_TARGET_BLEND_DESC &el)
{
  SERIALISE_MEMBER_TYPED(bool, BlendEnable);

  SERIALISE_MEMBER(SrcBlend);
  SERIALISE_MEMBER(DestBlend);
  SERIALISE_MEMBER(BlendOp);
  SERIALISE_MEMBER(SrcBlendAlpha);
  SERIALISE_MEMBER(DestBlendAlpha);
  SERIALISE_MEMBER(BlendOpAlpha);
  SERIALISE_MEMBER_TYPED(D3D11_COLOR_WRITE_ENABLE, RenderTargetWriteMask);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RENDER_TARGET_BLEND_DESC1 &el)
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
  SERIALISE_MEMBER_TYPED(D3D11_COLOR_WRITE_ENABLE, RenderTargetWriteMask);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BLEND_DESC &el)
{
  SERIALISE_MEMBER_TYPED(bool, AlphaToCoverageEnable);
  SERIALISE_MEMBER_TYPED(bool, IndependentBlendEnable);
  SERIALISE_MEMBER(RenderTarget);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BLEND_DESC1 &el)
{
  SERIALISE_MEMBER_TYPED(bool, AlphaToCoverageEnable);
  SERIALISE_MEMBER_TYPED(bool, IndependentBlendEnable);
  SERIALISE_MEMBER(RenderTarget);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_DEPTH_STENCILOP_DESC &el)
{
  SERIALISE_MEMBER(StencilFailOp);
  SERIALISE_MEMBER(StencilDepthFailOp);
  SERIALISE_MEMBER(StencilPassOp);
  SERIALISE_MEMBER(StencilFunc);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_DEPTH_STENCIL_DESC &el)
{
  SERIALISE_MEMBER_TYPED(bool, DepthEnable);
  SERIALISE_MEMBER(DepthWriteMask);
  SERIALISE_MEMBER(DepthFunc);
  SERIALISE_MEMBER_TYPED(bool, StencilEnable);
  SERIALISE_MEMBER(StencilReadMask);
  SERIALISE_MEMBER(StencilWriteMask);
  SERIALISE_MEMBER(FrontFace);
  SERIALISE_MEMBER(BackFace);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RASTERIZER_DESC &el)
{
  SERIALISE_MEMBER(FillMode);
  SERIALISE_MEMBER(CullMode);
  SERIALISE_MEMBER_TYPED(bool, FrontCounterClockwise);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER_TYPED(bool, DepthClipEnable);
  SERIALISE_MEMBER_TYPED(bool, ScissorEnable);
  SERIALISE_MEMBER_TYPED(bool, MultisampleEnable);
  SERIALISE_MEMBER_TYPED(bool, AntialiasedLineEnable);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RASTERIZER_DESC1 &el)
{
  SERIALISE_MEMBER(FillMode);
  SERIALISE_MEMBER(CullMode);
  SERIALISE_MEMBER_TYPED(bool, FrontCounterClockwise);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER_TYPED(bool, DepthClipEnable);
  SERIALISE_MEMBER_TYPED(bool, ScissorEnable);
  SERIALISE_MEMBER_TYPED(bool, MultisampleEnable);
  SERIALISE_MEMBER_TYPED(bool, AntialiasedLineEnable);
  SERIALISE_MEMBER(ForcedSampleCount);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_RASTERIZER_DESC2 &el)
{
  SERIALISE_MEMBER(FillMode);
  SERIALISE_MEMBER(CullMode);
  SERIALISE_MEMBER_TYPED(bool, FrontCounterClockwise);
  SERIALISE_MEMBER(DepthBias);
  SERIALISE_MEMBER(DepthBiasClamp);
  SERIALISE_MEMBER(SlopeScaledDepthBias);
  SERIALISE_MEMBER_TYPED(bool, DepthClipEnable);
  SERIALISE_MEMBER_TYPED(bool, ScissorEnable);
  SERIALISE_MEMBER_TYPED(bool, MultisampleEnable);
  SERIALISE_MEMBER_TYPED(bool, AntialiasedLineEnable);
  SERIALISE_MEMBER(ForcedSampleCount);
  SERIALISE_MEMBER(ConservativeRaster);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_QUERY_DESC &el)
{
  SERIALISE_MEMBER(Query);
  SERIALISE_MEMBER(MiscFlags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_QUERY_DESC1 &el)
{
  SERIALISE_MEMBER(Query);
  SERIALISE_MEMBER(MiscFlags);
  SERIALISE_MEMBER(ContextType);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_COUNTER_DESC &el)
{
  SERIALISE_MEMBER(Counter);
  SERIALISE_MEMBER(MiscFlags);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_SAMPLER_DESC &el)
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
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_SO_DECLARATION_ENTRY &el)
{
  SERIALISE_MEMBER(Stream);
  SERIALISE_MEMBER(SemanticName);
  SERIALISE_MEMBER(SemanticIndex);
  SERIALISE_MEMBER(StartComponent);
  SERIALISE_MEMBER(ComponentCount);
  SERIALISE_MEMBER(OutputSlot);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_INPUT_ELEMENT_DESC &el)
{
  SERIALISE_MEMBER(SemanticName);
  SERIALISE_MEMBER(SemanticIndex);
  SERIALISE_MEMBER(Format);
  SERIALISE_MEMBER(InputSlot);
  SERIALISE_MEMBER(AlignedByteOffset);
  SERIALISE_MEMBER(InputSlotClass);
  SERIALISE_MEMBER(InstanceDataStepRate);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_SUBRESOURCE_DATA &el)
{
  // don't serialise pSysMem, just set it to NULL. See the definition of SERIALISE_MEMBER_DUMMY
  SERIALISE_MEMBER_ARRAY_EMPTY(pSysMem);
  SERIALISE_MEMBER(SysMemPitch);
  SERIALISE_MEMBER(SysMemSlicePitch);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_VIEWPORT &el)
{
  SERIALISE_MEMBER(TopLeftX);
  SERIALISE_MEMBER(TopLeftY);
  SERIALISE_MEMBER(Width);
  SERIALISE_MEMBER(Height);
  SERIALISE_MEMBER(MinDepth);
  SERIALISE_MEMBER(MaxDepth);
}

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, D3D11_BOX &el)
{
  SERIALISE_MEMBER(left);
  SERIALISE_MEMBER(top);
  SERIALISE_MEMBER(front);
  SERIALISE_MEMBER(right);
  SERIALISE_MEMBER(bottom);
  SERIALISE_MEMBER(back);
}

INSTANTIATE_SERIALISE_TYPE(D3D11_BUFFER_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXTURE1D_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXTURE2D_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXTURE2D_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXTURE3D_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXTURE3D_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_BUFFER_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_BUFFEREX_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_ARRAY_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_SRV1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_SRV1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX3D_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXCUBE_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEXCUBE_ARRAY_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2DMS_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2DMS_ARRAY_SRV);
INSTANTIATE_SERIALISE_TYPE(D3D11_SHADER_RESOURCE_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_SHADER_RESOURCE_VIEW_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_BUFFER_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_ARRAY_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2DMS_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2DMS_ARRAY_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_RTV1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_RTV1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX3D_RTV);
INSTANTIATE_SERIALISE_TYPE(D3D11_RENDER_TARGET_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_RENDER_TARGET_VIEW_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_BUFFER_UAV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_UAV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_ARRAY_UAV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_UAV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_UAV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_UAV1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_UAV1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX3D_UAV);
INSTANTIATE_SERIALISE_TYPE(D3D11_UNORDERED_ACCESS_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_UNORDERED_ACCESS_VIEW_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_DSV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX1D_ARRAY_DSV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_DSV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2D_ARRAY_DSV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2DMS_DSV);
INSTANTIATE_SERIALISE_TYPE(D3D11_TEX2DMS_ARRAY_DSV);
INSTANTIATE_SERIALISE_TYPE(D3D11_DEPTH_STENCIL_VIEW_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_RENDER_TARGET_BLEND_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_RENDER_TARGET_BLEND_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_BLEND_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_BLEND_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_DEPTH_STENCILOP_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_DEPTH_STENCIL_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_RASTERIZER_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_RASTERIZER_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_RASTERIZER_DESC2);
INSTANTIATE_SERIALISE_TYPE(D3D11_QUERY_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_QUERY_DESC1);
INSTANTIATE_SERIALISE_TYPE(D3D11_COUNTER_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_SAMPLER_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_SO_DECLARATION_ENTRY);
INSTANTIATE_SERIALISE_TYPE(D3D11_INPUT_ELEMENT_DESC);
INSTANTIATE_SERIALISE_TYPE(D3D11_SUBRESOURCE_DATA);
INSTANTIATE_SERIALISE_TYPE(D3D11_VIEWPORT);
INSTANTIATE_SERIALISE_TYPE(D3D11_RECT);
INSTANTIATE_SERIALISE_TYPE(D3D11_BOX);
