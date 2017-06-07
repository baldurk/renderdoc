/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "driver/d3d11/d3d11_resources.h"
#include "3rdparty/lz4/lz4.h"
#include "api/app/renderdoc_app.h"
#include "driver/d3d11/d3d11_context.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/dxgi/dxgi_wrapped.h"

WRAPPED_POOL_INST(WrappedID3D11Buffer);
WRAPPED_POOL_INST(WrappedID3D11Texture1D);
WRAPPED_POOL_INST(WrappedID3D11Texture2D1);
WRAPPED_POOL_INST(WrappedID3D11Texture3D1);
WRAPPED_POOL_INST(WrappedID3D11InputLayout);
WRAPPED_POOL_INST(WrappedID3D11SamplerState);
WRAPPED_POOL_INST(WrappedID3D11RasterizerState2);
WRAPPED_POOL_INST(WrappedID3D11DepthStencilState);
WRAPPED_POOL_INST(WrappedID3D11BlendState1);
WRAPPED_POOL_INST(WrappedID3D11ShaderResourceView1);
WRAPPED_POOL_INST(WrappedID3D11UnorderedAccessView1);
WRAPPED_POOL_INST(WrappedID3D11RenderTargetView1);
WRAPPED_POOL_INST(WrappedID3D11DepthStencilView);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11VertexShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11HullShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11DomainShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11GeometryShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11PixelShader>);
WRAPPED_POOL_INST(WrappedID3D11Shader<ID3D11ComputeShader>);
WRAPPED_POOL_INST(WrappedID3D11Counter);
WRAPPED_POOL_INST(WrappedID3D11Query1);
WRAPPED_POOL_INST(WrappedID3D11Predicate);
WRAPPED_POOL_INST(WrappedID3D11ClassInstance);
WRAPPED_POOL_INST(WrappedID3D11ClassLinkage);
WRAPPED_POOL_INST(WrappedID3DDeviceContextState);

map<ResourceId, WrappedID3D11Texture1D::TextureEntry>
    WrappedTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, ID3D11Texture1D>::m_TextureList;
map<ResourceId, WrappedID3D11Texture2D1::TextureEntry>
    WrappedTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, ID3D11Texture2D1>::m_TextureList;
map<ResourceId, WrappedID3D11Texture3D1::TextureEntry>
    WrappedTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, ID3D11Texture3D1>::m_TextureList;
map<ResourceId, WrappedID3D11Buffer::BufferEntry> WrappedID3D11Buffer::m_BufferList;
map<ResourceId, WrappedShader::ShaderEntry *> WrappedShader::m_ShaderList;
Threading::CriticalSection WrappedShader::m_ShaderListLock;
std::vector<WrappedID3DDeviceContextState *> WrappedID3DDeviceContextState::m_List;
Threading::CriticalSection WrappedID3DDeviceContextState::m_Lock;

const GUID RENDERDOC_ID3D11ShaderGUID_ShaderDebugMagicValue = RENDERDOC_ShaderDebugMagicValue_struct;

void WrappedShader::ShaderEntry::TryReplaceOriginalByteCode()
{
  if(!DXBC::DXBCFile::CheckForDebugInfo((const void *)&m_Bytecode[0], m_Bytecode.size()))
  {
    string originalPath = m_DebugInfoPath;

    if(originalPath.empty())
      originalPath =
          DXBC::DXBCFile::GetDebugBinaryPath((const void *)&m_Bytecode[0], m_Bytecode.size());

    if(!originalPath.empty())
    {
      bool lz4 = false;

      if(!strncmp(originalPath.c_str(), "lz4#", 4))
      {
        originalPath = originalPath.substr(4);
        lz4 = true;
      }
      // could support more if we're willing to compile in the decompressor

      FILE *originalShaderFile = NULL;

      size_t numSearchPaths = m_DebugInfoSearchPaths ? m_DebugInfoSearchPaths->size() : 0;

      string foundPath;

      // while we haven't found a file, keep trying through the search paths. For i==0
      // check the path on its own, in case it's an absolute path.
      for(size_t i = 0; originalShaderFile == NULL && i <= numSearchPaths; i++)
      {
        if(i == 0)
        {
          originalShaderFile = FileIO::fopen(originalPath.c_str(), "rb");
          foundPath = originalPath;
          continue;
        }
        else
        {
          const std::string &searchPath = (*m_DebugInfoSearchPaths)[i - 1];
          foundPath = searchPath + "/" + originalPath;
          originalShaderFile = FileIO::fopen(foundPath.c_str(), "rb");
        }
      }

      if(originalShaderFile == NULL)
        return;

      FileIO::fseek64(originalShaderFile, 0L, SEEK_END);
      uint64_t originalShaderSize = FileIO::ftell64(originalShaderFile);
      FileIO::fseek64(originalShaderFile, 0, SEEK_SET);

      if(lz4 || originalShaderSize >= m_Bytecode.size())
      {
        vector<byte> originalBytecode;

        originalBytecode.resize((size_t)originalShaderSize);
        FileIO::fread(&originalBytecode[0], sizeof(byte), (size_t)originalShaderSize,
                      originalShaderFile);

        if(lz4)
        {
          vector<byte> decompressed;

          // first try decompressing to 1MB flat
          decompressed.resize(100 * 1024);

          int ret = LZ4_decompress_safe((const char *)&originalBytecode[0], (char *)&decompressed[0],
                                        (int)originalBytecode.size(), (int)decompressed.size());

          if(ret < 0)
          {
            // if it failed, either source is corrupt or we didn't allocate enough space.
            // Just allocate 255x compressed size since it can't need any more than that.
            decompressed.resize(255 * originalBytecode.size());

            ret = LZ4_decompress_safe((const char *)&originalBytecode[0], (char *)&decompressed[0],
                                      (int)originalBytecode.size(), (int)decompressed.size());

            if(ret < 0)
            {
              RDCERR("Failed to decompress LZ4 data from %s", foundPath.c_str());
              return;
            }
          }

          RDCASSERT(ret > 0, ret);

          // we resize and memcpy instead of just doing .swap() because that would
          // transfer over the over-large pessimistic capacity needed for decompression
          originalBytecode.resize(ret);
          memcpy(&originalBytecode[0], &decompressed[0], originalBytecode.size());
        }

        if(DXBC::DXBCFile::CheckForDebugInfo((const void *)&originalBytecode[0],
                                             originalBytecode.size()))
        {
          m_Bytecode.swap(originalBytecode);
        }
      }

      FileIO::fclose(originalShaderFile);
    }
  }
}

UINT GetMipForSubresource(ID3D11Resource *res, int Subresource)
{
  D3D11_RESOURCE_DIMENSION dim;

  // check for wrapped types first as they will be most common and don't
  // require a virtual call
  if(WrappedID3D11Texture1D::IsAlloc(res))
    dim = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  else if(WrappedID3D11Texture2D1::IsAlloc(res))
    dim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  else if(WrappedID3D11Texture3D1::IsAlloc(res))
    dim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  else
    res->GetType(&dim);

  ID3D11Texture1D *tex1 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D) ? (ID3D11Texture1D *)res : NULL;
  ID3D11Texture2D *tex2 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) ? (ID3D11Texture2D *)res : NULL;
  ID3D11Texture3D *tex3 = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) ? (ID3D11Texture3D *)res : NULL;

  RDCASSERT(tex1 || tex2 || tex3);

  UINT mipLevel = Subresource;

  if(tex1)
  {
    D3D11_TEXTURE1D_DESC desc;
    tex1->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, 1, 1);

    mipLevel %= mipLevels;
  }
  else if(tex2)
  {
    D3D11_TEXTURE2D_DESC desc;
    tex2->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, 1);

    mipLevel %= mipLevels;
  }
  else if(tex3)
  {
    D3D11_TEXTURE3D_DESC desc;
    tex3->GetDesc(&desc);

    int mipLevels = desc.MipLevels;

    if(mipLevels == 0)
      mipLevels = CalcNumMips(desc.Width, desc.Height, desc.Depth);

    mipLevel %= mipLevels;
  }

  return mipLevel;
}

UINT GetByteSize(ID3D11Texture1D *tex, int SubResource)
{
  D3D11_TEXTURE1D_DESC desc;
  tex->GetDesc(&desc);

  return GetByteSize(desc.Width, 1, 1, desc.Format, SubResource % desc.MipLevels);
}

UINT GetByteSize(ID3D11Texture2D *tex, int SubResource)
{
  D3D11_TEXTURE2D_DESC desc;
  tex->GetDesc(&desc);

  return GetByteSize(desc.Width, desc.Height, 1, desc.Format, SubResource % desc.MipLevels);
}

UINT GetByteSize(ID3D11Texture3D *tex, int SubResource)
{
  D3D11_TEXTURE3D_DESC desc;
  tex->GetDesc(&desc);

  return GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, SubResource);
}

string ToStrHelper<false, ResourceType>::Get(const ResourceType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(Resource_InputLayout)
    TOSTR_CASE_STRINGIZE(Resource_Buffer)
    TOSTR_CASE_STRINGIZE(Resource_Texture1D)
    TOSTR_CASE_STRINGIZE(Resource_Texture2D)
    TOSTR_CASE_STRINGIZE(Resource_Texture3D)
    TOSTR_CASE_STRINGIZE(Resource_RasterizerState)
    TOSTR_CASE_STRINGIZE(Resource_BlendState)
    TOSTR_CASE_STRINGIZE(Resource_DepthStencilState)
    TOSTR_CASE_STRINGIZE(Resource_SamplerState)
    TOSTR_CASE_STRINGIZE(Resource_RenderTargetView)
    TOSTR_CASE_STRINGIZE(Resource_ShaderResourceView)
    TOSTR_CASE_STRINGIZE(Resource_DepthStencilView)
    TOSTR_CASE_STRINGIZE(Resource_Shader)
    TOSTR_CASE_STRINGIZE(Resource_UnorderedAccessView)
    TOSTR_CASE_STRINGIZE(Resource_Counter)
    TOSTR_CASE_STRINGIZE(Resource_Query)
    TOSTR_CASE_STRINGIZE(Resource_Predicate)
    TOSTR_CASE_STRINGIZE(Resource_ClassInstance)
    TOSTR_CASE_STRINGIZE(Resource_ClassLinkage)

    TOSTR_CASE_STRINGIZE(Resource_DeviceContext)
    TOSTR_CASE_STRINGIZE(Resource_CommandList)
    default: break;
  }

  char tostrBuf[256] = {0};
  StringFormat::snprintf(tostrBuf, 255, "ResourceType<%d>", el);

  return tostrBuf;
}

ResourceId GetIDForResource(ID3D11DeviceChild *ptr)
{
  if(ptr == NULL)
    return ResourceId();

  if(WrappedID3D11Buffer::IsAlloc(ptr))
    return ((WrappedID3D11Buffer *)ptr)->GetResourceID();
  if(WrappedID3D11Texture2D1::IsAlloc(ptr))
    return ((WrappedID3D11Texture2D1 *)ptr)->GetResourceID();
  if(WrappedID3D11Texture3D1::IsAlloc(ptr))
    return ((WrappedID3D11Texture3D1 *)ptr)->GetResourceID();
  if(WrappedID3D11Texture1D::IsAlloc(ptr))
    return ((WrappedID3D11Texture1D *)ptr)->GetResourceID();

  if(WrappedID3D11InputLayout::IsAlloc(ptr))
    return ((WrappedID3D11InputLayout *)ptr)->GetResourceID();

  if(WrappedID3D11Shader<ID3D11VertexShader>::IsAlloc(ptr))
    return ((WrappedID3D11Shader<ID3D11VertexShader> *)ptr)->GetResourceID();
  if(WrappedID3D11Shader<ID3D11PixelShader>::IsAlloc(ptr))
    return ((WrappedID3D11Shader<ID3D11PixelShader> *)ptr)->GetResourceID();
  if(WrappedID3D11Shader<ID3D11GeometryShader>::IsAlloc(ptr))
    return ((WrappedID3D11Shader<ID3D11GeometryShader> *)ptr)->GetResourceID();
  if(WrappedID3D11Shader<ID3D11HullShader>::IsAlloc(ptr))
    return ((WrappedID3D11Shader<ID3D11HullShader> *)ptr)->GetResourceID();
  if(WrappedID3D11Shader<ID3D11DomainShader>::IsAlloc(ptr))
    return ((WrappedID3D11Shader<ID3D11DomainShader> *)ptr)->GetResourceID();
  if(WrappedID3D11Shader<ID3D11ComputeShader>::IsAlloc(ptr))
    return ((WrappedID3D11Shader<ID3D11ComputeShader> *)ptr)->GetResourceID();

  if(WrappedID3D11RasterizerState2::IsAlloc(ptr))
    return ((WrappedID3D11RasterizerState2 *)ptr)->GetResourceID();
  if(WrappedID3D11BlendState1::IsAlloc(ptr))
    return ((WrappedID3D11BlendState1 *)ptr)->GetResourceID();
  if(WrappedID3D11DepthStencilState::IsAlloc(ptr))
    return ((WrappedID3D11DepthStencilState *)ptr)->GetResourceID();
  if(WrappedID3D11SamplerState::IsAlloc(ptr))
    return ((WrappedID3D11SamplerState *)ptr)->GetResourceID();

  if(WrappedID3D11RenderTargetView1::IsAlloc(ptr))
    return ((WrappedID3D11RenderTargetView1 *)ptr)->GetResourceID();
  if(WrappedID3D11ShaderResourceView1::IsAlloc(ptr))
    return ((WrappedID3D11ShaderResourceView1 *)ptr)->GetResourceID();
  if(WrappedID3D11DepthStencilView::IsAlloc(ptr))
    return ((WrappedID3D11DepthStencilView *)ptr)->GetResourceID();
  if(WrappedID3D11UnorderedAccessView1::IsAlloc(ptr))
    return ((WrappedID3D11UnorderedAccessView1 *)ptr)->GetResourceID();

  if(WrappedID3D11Counter::IsAlloc(ptr))
    return ((WrappedID3D11Counter *)ptr)->GetResourceID();
  if(WrappedID3D11Query1::IsAlloc(ptr))
    return ((WrappedID3D11Query1 *)ptr)->GetResourceID();
  if(WrappedID3D11Predicate::IsAlloc(ptr))
    return ((WrappedID3D11Predicate *)ptr)->GetResourceID();

  if(WrappedID3D11ClassInstance::IsAlloc(ptr))
    return ((WrappedID3D11ClassInstance *)ptr)->GetResourceID();
  if(WrappedID3D11ClassLinkage::IsAlloc(ptr))
    return ((WrappedID3D11ClassLinkage *)ptr)->GetResourceID();

  if(WrappedID3D11DeviceContext::IsAlloc(ptr))
    return ((WrappedID3D11DeviceContext *)ptr)->GetResourceID();
  if(WrappedID3D11CommandList::IsAlloc(ptr))
    return ((WrappedID3D11CommandList *)ptr)->GetResourceID();
  if(WrappedID3DDeviceContextState::IsAlloc(ptr))
    return ((WrappedID3DDeviceContextState *)ptr)->GetResourceID();

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return ResourceId();
}

ResourceType IdentifyTypeByPtr(IUnknown *ptr)
{
  if(WrappedID3D11InputLayout::IsAlloc(ptr))
    return Resource_InputLayout;

  if(WrappedID3D11Shader<ID3D11VertexShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11PixelShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11GeometryShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11HullShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11DomainShader>::IsAlloc(ptr) ||
     WrappedID3D11Shader<ID3D11ComputeShader>::IsAlloc(ptr))
    return Resource_Shader;

  if(WrappedID3D11Buffer::IsAlloc(ptr))
    return Resource_Buffer;

  if(WrappedID3D11Texture1D::IsAlloc(ptr))
    return Resource_Texture1D;
  if(WrappedID3D11Texture2D1::IsAlloc(ptr))
    return Resource_Texture2D;
  if(WrappedID3D11Texture3D1::IsAlloc(ptr))
    return Resource_Texture3D;

  if(WrappedID3D11RasterizerState2::IsAlloc(ptr))
    return Resource_RasterizerState;
  if(WrappedID3D11BlendState1::IsAlloc(ptr))
    return Resource_BlendState;
  if(WrappedID3D11DepthStencilState::IsAlloc(ptr))
    return Resource_DepthStencilState;
  if(WrappedID3D11SamplerState::IsAlloc(ptr))
    return Resource_SamplerState;

  if(WrappedID3D11RenderTargetView1::IsAlloc(ptr))
    return Resource_RenderTargetView;
  if(WrappedID3D11ShaderResourceView1::IsAlloc(ptr))
    return Resource_ShaderResourceView;
  if(WrappedID3D11DepthStencilView::IsAlloc(ptr))
    return Resource_DepthStencilView;
  if(WrappedID3D11UnorderedAccessView1::IsAlloc(ptr))
    return Resource_UnorderedAccessView;

  if(WrappedID3D11Counter::IsAlloc(ptr))
    return Resource_Counter;
  if(WrappedID3D11Query1::IsAlloc(ptr))
    return Resource_Query;
  if(WrappedID3D11Predicate::IsAlloc(ptr))
    return Resource_Predicate;

  if(WrappedID3D11ClassInstance::IsAlloc(ptr))
    return Resource_ClassInstance;
  if(WrappedID3D11ClassLinkage::IsAlloc(ptr))
    return Resource_ClassLinkage;

  if(WrappedID3D11DeviceContext::IsAlloc(ptr))
    return Resource_DeviceContext;
  if(WrappedID3D11CommandList::IsAlloc(ptr))
    return Resource_CommandList;

  if(WrappedID3DDeviceContextState::IsAlloc(ptr))
    return Resource_DeviceState;

  RDCERR("Unknown type for ptr 0x%p", ptr);

  return Resource_Unknown;
}

void *UnwrapDXDevice(void *dxDevice)
{
  if(WrappedID3D11Device::IsAlloc(dxDevice))
    return ((WrappedID3D11Device *)(ID3D11Device *)dxDevice)->GetReal();

  return NULL;
}

ID3D11Resource *UnwrapDXResource(void *dxObject)
{
  if(WrappedID3D11Buffer::IsAlloc(dxObject))
  {
    WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)(ID3D11Buffer *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture1D::IsAlloc(dxObject))
  {
    WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)(ID3D11Texture1D *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)(ID3D11Texture2D1 *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)(ID3D11Texture3D1 *)dxObject;
    return w->GetReal();
  }

  return NULL;
}

void GetDXTextureProperties(void *dxObject, ResourceFormat &fmt, uint32_t &width, uint32_t &height,
                            uint32_t &depth, uint32_t &mips, uint32_t &layers, uint32_t &samples)
{
  if(WrappedID3D11Buffer::IsAlloc(dxObject))
  {
    WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)(ID3D11Buffer *)dxObject;

    D3D11_BUFFER_DESC desc;
    w->GetDesc(&desc);

    fmt = ResourceFormat();
    width = desc.ByteWidth;
    height = 1;
    depth = 1;
    mips = 1;
    layers = 1;
    samples = 1;

    return;
  }
  else if(WrappedID3D11Texture1D::IsAlloc(dxObject))
  {
    WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)(ID3D11Texture1D *)dxObject;

    D3D11_TEXTURE1D_DESC desc;
    w->GetDesc(&desc);

    fmt = MakeResourceFormat(desc.Format);
    width = desc.Width;
    height = 1;
    depth = 1;
    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);
    layers = desc.ArraySize;
    samples = 1;

    return;
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)(ID3D11Texture2D1 *)dxObject;

    D3D11_TEXTURE2D_DESC desc;
    w->GetDesc(&desc);

    fmt = MakeResourceFormat(desc.Format);
    width = desc.Width;
    height = desc.Height;
    depth = 1;
    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);
    layers = desc.ArraySize;
    samples = desc.SampleDesc.Count;

    return;
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)(ID3D11Texture3D1 *)dxObject;

    D3D11_TEXTURE3D_DESC desc;
    w->GetDesc(&desc);

    fmt = MakeResourceFormat(desc.Format);
    width = desc.Width;
    height = desc.Height;
    depth = desc.Depth;
    mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);
    layers = 1;
    samples = 1;

    return;
  }

  RDCERR("Getting DX texture properties for unknown/unhandled objects %p", dxObject);
}

HRESULT STDMETHODCALLTYPE RefCounter::QueryInterface(
    /* [in] */ REFIID riid,
    /* [annotation][iid_is][out] */
    __RPC__deref_out void **ppvObject)
{
  return RefCountDXGIObject::WrapQueryInterface(m_pReal, riid, ppvObject);
}

unsigned int RefCounter::SoftRef(WrappedID3D11Device *device)
{
  unsigned int ret = AddRef();
  if(device)
    device->SoftRef();
  else
    RDCWARN("No device pointer, is a deleted resource being AddRef()d?");
  return ret;
}

unsigned int RefCounter::SoftRelease(WrappedID3D11Device *device)
{
  unsigned int ret = Release();
  if(device)
    device->SoftRelease();
  else
    RDCWARN("No device pointer, is a deleted resource being Release()d?");
  return ret;
}

void RefCounter::AddDeviceSoftref(WrappedID3D11Device *device)
{
  if(device)
    device->SoftRef();
}

void RefCounter::ReleaseDeviceSoftref(WrappedID3D11Device *device)
{
  if(device)
    device->SoftRelease();
}

WrappedID3DDeviceContextState::WrappedID3DDeviceContextState(ID3DDeviceContextState *real,
                                                             WrappedID3D11Device *device)
    : WrappedDeviceChild11(real, device)
{
  state = new D3D11RenderState((Serialiser *)NULL);

  {
    SCOPED_LOCK(WrappedID3DDeviceContextState::m_Lock);
    WrappedID3DDeviceContextState::m_List.push_back(this);
  }
}

WrappedID3DDeviceContextState::~WrappedID3DDeviceContextState()
{
  SAFE_DELETE(state);
  Shutdown();

  {
    SCOPED_LOCK(WrappedID3DDeviceContextState::m_Lock);
    auto it = std::find(WrappedID3DDeviceContextState::m_List.begin(),
                        WrappedID3DDeviceContextState::m_List.end(), this);
    WrappedID3DDeviceContextState::m_List.erase(it);
  }
}
