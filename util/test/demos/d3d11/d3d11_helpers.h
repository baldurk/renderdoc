/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018 Baldur Karlsson
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

#include "../test_common.h"

#include <comdef.h>
#include "dx/d3d_helpers.h"
#include "dx/official/d3d11.h"
#include "dx/official/d3d11_1.h"
#include "dx/official/d3d11_2.h"

#define COM_SMARTPTR(classname) _COM_SMARTPTR_TYPEDEF(classname, __uuidof(classname))

COM_SMARTPTR(ID3DBlob);
COM_SMARTPTR(IDXGISwapChain);

COM_SMARTPTR(ID3D11Device);
COM_SMARTPTR(ID3D11Device1);
COM_SMARTPTR(ID3D11Device2);

COM_SMARTPTR(ID3D11DeviceContext);
COM_SMARTPTR(ID3D11DeviceContext1);
COM_SMARTPTR(ID3D11DeviceContext2);

COM_SMARTPTR(ID3D11CommandList);

COM_SMARTPTR(ID3D11InputLayout);

COM_SMARTPTR(ID3D11Buffer);

COM_SMARTPTR(ID3D11Query);
COM_SMARTPTR(ID3D11Counter);
COM_SMARTPTR(ID3D11Predicate);

COM_SMARTPTR(ID3D11VertexShader);
COM_SMARTPTR(ID3D11PixelShader);
COM_SMARTPTR(ID3D11HullShader);
COM_SMARTPTR(ID3D11DomainShader);
COM_SMARTPTR(ID3D11GeometryShader);
COM_SMARTPTR(ID3D11ComputeShader);

COM_SMARTPTR(ID3D11RasterizerState);
COM_SMARTPTR(ID3D11BlendState);
COM_SMARTPTR(ID3D11DepthStencilState);
COM_SMARTPTR(ID3D11SamplerState);

COM_SMARTPTR(ID3D11Texture1D);
COM_SMARTPTR(ID3D11Texture2D);
COM_SMARTPTR(ID3D11Texture3D);
COM_SMARTPTR(ID3D11RenderTargetView);
COM_SMARTPTR(ID3D11ShaderResourceView);
COM_SMARTPTR(ID3D11UnorderedAccessView);
COM_SMARTPTR(ID3D11DepthStencilView);

COM_SMARTPTR(ID3D11InfoQueue);
COM_SMARTPTR(ID3DUserDefinedAnnotation);

struct D3D11GraphicsTest;

class D3D11BufferCreator
{
public:
  D3D11BufferCreator(D3D11GraphicsTest *test);

  D3D11BufferCreator &Vertex();
  D3D11BufferCreator &Index();
  D3D11BufferCreator &Constant();
  D3D11BufferCreator &StreamOut();
  D3D11BufferCreator &SRV();
  D3D11BufferCreator &UAV();

  D3D11BufferCreator &Structured(UINT structStride);
  D3D11BufferCreator &ByteAddressed();
  D3D11BufferCreator &Mappable();
  D3D11BufferCreator &Staging();

  D3D11BufferCreator &Data(const void *data);
  D3D11BufferCreator &Size(UINT size);

  template <typename T, size_t N>
  D3D11BufferCreator &Data(const T (&data)[N])
  {
    return Data(&data[0]).Size(UINT(N * sizeof(T)));
  }

  template <typename T>
  D3D11BufferCreator &Data(const std::vector<T> &data)
  {
    return Data(data.data()).Size(UINT(data.size() * sizeof(T)));
  }

  operator ID3D11Buffer *() const;
  operator ID3D11BufferPtr() const { return ID3D11BufferPtr((ID3D11Buffer *)*this); }
private:
  D3D11GraphicsTest *m_Test;

  D3D11_BUFFER_DESC m_BufDesc;
  D3D11_SUBRESOURCE_DATA m_Initdata = {};
};

class D3D11TextureCreator
{
public:
  D3D11TextureCreator(D3D11GraphicsTest *test, DXGI_FORMAT format, UINT width, UINT height,
                      UINT depth);

  D3D11TextureCreator &Mips(UINT mips);
  D3D11TextureCreator &Array(UINT size);
  D3D11TextureCreator &Multisampled(UINT count, UINT quality = 0);

  D3D11TextureCreator &SRV();
  D3D11TextureCreator &UAV();
  D3D11TextureCreator &RTV();
  D3D11TextureCreator &DSV();

  D3D11TextureCreator &Mappable();
  D3D11TextureCreator &Staging();

  operator ID3D11Texture1D *() const;
  operator ID3D11Texture1DPtr() const { return ID3D11Texture1DPtr((ID3D11Texture1D *)*this); }
  operator ID3D11Texture2D *() const;
  operator ID3D11Texture2DPtr() const { return ID3D11Texture2DPtr((ID3D11Texture2D *)*this); }
  operator ID3D11Texture3D *() const;
  operator ID3D11Texture3DPtr() const { return ID3D11Texture3DPtr((ID3D11Texture3D *)*this); }
protected:
  D3D11GraphicsTest *m_Test;

  UINT Width = 1;
  UINT Height = 1;
  UINT Depth = 1;
  UINT MipLevels = 1;
  UINT ArraySize = 1;
  DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
  DXGI_SAMPLE_DESC SampleDesc = {1, 0};
  D3D11_USAGE Usage = D3D11_USAGE_DEFAULT;
  UINT BindFlags = 0;
  UINT CPUAccessFlags = 0;
  UINT MiscFlags = 0;
};

enum class ResourceType
{
  Buffer,
  Texture1D,
  Texture1DArray,
  Texture2D,
  Texture2DArray,
  Texture2DMS,
  Texture2DMSArray,
  Texture3D,
};

enum class ViewType
{
  SRV,
  RTV,
  DSV,
  UAV,
};

class D3D11ViewCreator
{
public:
  D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Buffer *buf);
  D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Texture1D *tex);
  D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Texture2D *tex);
  D3D11ViewCreator(D3D11GraphicsTest *test, ViewType viewType, ID3D11Texture3D *tex);

  // common params
  D3D11ViewCreator &Format(DXGI_FORMAT format);

  // buffer params
  D3D11ViewCreator &FirstElement(UINT el);
  D3D11ViewCreator &NumElements(UINT num);

  // texture params
  D3D11ViewCreator &FirstMip(UINT mip);
  D3D11ViewCreator &NumMips(UINT num);
  D3D11ViewCreator &FirstSlice(UINT mip);
  D3D11ViewCreator &NumSlices(UINT num);

  // depth stencil only
  D3D11ViewCreator &ReadOnlyDepth();
  D3D11ViewCreator &ReadOnlyStencil();

  operator ID3D11ShaderResourceView *();
  operator ID3D11ShaderResourceViewPtr()
  {
    return ID3D11ShaderResourceViewPtr((ID3D11ShaderResourceView *)*this);
  }
  operator ID3D11RenderTargetView *();
  operator ID3D11RenderTargetViewPtr()
  {
    return ID3D11RenderTargetViewPtr((ID3D11RenderTargetView *)*this);
  }
  operator ID3D11DepthStencilView *();
  operator ID3D11DepthStencilViewPtr()
  {
    return ID3D11DepthStencilViewPtr((ID3D11DepthStencilView *)*this);
  }

  operator ID3D11UnorderedAccessView *();
  operator ID3D11UnorderedAccessViewPtr()
  {
    return ID3D11UnorderedAccessViewPtr((ID3D11UnorderedAccessView *)*this);
  }

private:
  void SetupDescriptors(ViewType viewType, ResourceType resType);

  D3D11GraphicsTest *m_Test;
  ID3D11Resource *m_Res;
  ViewType m_Type;

  // instead of a huge mess trying to auto populate the actual descriptors from saved values, as
  // they aren't very nicely compatible (e.g. RTVs have mipslice selection on 3D textures, SRVs
  // don't, SRVs support 1D texturse but DSVs don't, UAVs don't support MSAA textures, etc etc).
  // Instead we just set save pointers that might be NULL in the constructor based on view and
  // resource type
  union
  {
    D3D11_SHADER_RESOURCE_VIEW_DESC srv;
    D3D11_RENDER_TARGET_VIEW_DESC rtv;
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
  } desc;

  UINT *firstElement = NULL, *numElements = NULL;
  UINT *firstMip = NULL, *numMips = NULL;
  UINT *firstSlice = NULL, *numSlices = NULL;
};

#define GET_REFCOUNT(val, obj) \
  do                           \
  {                            \
    obj->AddRef();             \
    val = obj->Release();      \
  } while(0)

#define CHECK_HR(expr)                                                                    \
  {                                                                                       \
    HRESULT hr = (expr);                                                                  \
    if(FAILED(hr))                                                                        \
    {                                                                                     \
      TEST_ERROR("Failed HRESULT at %s:%d (%x): %s", __FILE__, (int)__LINE__, hr, #expr); \
      DEBUG_BREAK();                                                                      \
      exit(1);                                                                            \
    }                                                                                     \
  }

template <class T>
inline void SetDebugName(T pObj, const char *name)
{
  if(pObj)
    pObj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
}
