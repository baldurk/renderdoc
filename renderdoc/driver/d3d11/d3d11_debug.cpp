/******************************************************************************
 * The MIT License (MIT)
 * 
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


#include "d3d11_manager.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "shaders/dxbc_debug.h"
#include "maths/matrix.h"
#include "maths/camera.h"
#include "data/resource.h"
#include "common/string_utils.h"
#include "maths/formatpacking.h"

#include "driver/d3d11/d3d11_resources.h"

#include "d3d11_renderstate.h"

#include <d3dcompiler.h>

// used for serialising out ms textures - converts typeless to uint typed where possible,
// or float/unorm if necessary. Only typeless formats are converted.
DXGI_FORMAT GetTypedFormatUIntPreferred(DXGI_FORMAT f)
{
	switch(f)
	{
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			return DXGI_FORMAT_R32G32B32A32_UINT;
    case DXGI_FORMAT_R32G32B32_TYPELESS:
			return DXGI_FORMAT_R32G32B32_UINT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
			return DXGI_FORMAT_R16G16B16A16_UINT;
    case DXGI_FORMAT_R32G32_TYPELESS:
			return DXGI_FORMAT_R32G32_UINT;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
			return DXGI_FORMAT_R10G10B10A2_UINT;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			return DXGI_FORMAT_R8G8B8A8_UINT;
    case DXGI_FORMAT_R16G16_TYPELESS:
			return DXGI_FORMAT_R16G16_UINT;
    case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_R32_UINT;
    case DXGI_FORMAT_R8G8_TYPELESS:
			return DXGI_FORMAT_R8G8_UINT;
    case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_R16_UINT;
		case DXGI_FORMAT_R8_TYPELESS:
			return DXGI_FORMAT_R8_UINT;
		case DXGI_FORMAT_BC1_TYPELESS:
			return DXGI_FORMAT_BC1_UNORM;
		case DXGI_FORMAT_BC2_TYPELESS:
			return DXGI_FORMAT_BC2_UNORM;
		case DXGI_FORMAT_BC3_TYPELESS:
			return DXGI_FORMAT_BC3_UNORM;
		case DXGI_FORMAT_BC4_TYPELESS:
			return DXGI_FORMAT_BC4_UNORM;
		case DXGI_FORMAT_BC5_TYPELESS:
			return DXGI_FORMAT_BC5_UNORM;
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
			return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
		case DXGI_FORMAT_BC6H_TYPELESS:
			return DXGI_FORMAT_BC6H_UF16;
		case DXGI_FORMAT_BC7_TYPELESS:
			return DXGI_FORMAT_BC7_UNORM;

		default:
			break;
	}

	return f;
}

D3D11DebugManager::D3D11DebugManager(WrappedID3D11Device *wrapper)
{
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D11DebugManager));

	m_pDevice = wrapper->GetReal();
	m_pDevice->GetImmediateContext(&m_pImmediateContext);
	m_ResourceManager = wrapper->GetResourceManager();

	m_OutputWindowID = 1;

	m_WrappedDevice = wrapper;
	ID3D11DeviceContext *ctx = NULL;
	m_WrappedDevice->GetImmediateContext(&ctx);
	m_WrappedDevice->InternalRef();
	m_WrappedContext = (WrappedID3D11DeviceContext *)ctx;

	RenderDoc::Inst().SetProgress(DebugManagerInit, 0.0f);
	
	m_pFactory = NULL;

	HRESULT hr = S_OK;

	IDXGIDevice *pDXGIDevice;
	hr = m_WrappedDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);

	if(FAILED(hr))
	{
		RDCERR("Couldn't get DXGI device from D3D device");
	}
	else
	{
		IDXGIAdapter *pDXGIAdapter;
		hr = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&pDXGIAdapter);

		if(FAILED(hr))
		{
			RDCERR("Couldn't get DXGI adapter from DXGI device");
			SAFE_RELEASE(pDXGIDevice);
		}
		else
		{
			hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&m_pFactory);

			SAFE_RELEASE(pDXGIDevice);
			SAFE_RELEASE(pDXGIAdapter);

			if(FAILED(hr))
			{
				RDCERR("Couldn't get DXGI factory from DXGI adapter");
			}
		}
	}

	wstring shadercache = FileIO::GetAppFolderFilename(L"shaders.cache");

	m_ShaderCacheDirty = true;

	FILE *f = FileIO::fopen(shadercache.c_str(), L"rb");
	if(f)
	{
		FileIO::fseek64(f, 0, SEEK_END);
		uint64_t cachelen = FileIO::ftell64(f);
		FileIO::fseek64(f, 0, SEEK_SET);

		if(cachelen < 8)
		{
			RDCERR("Invalid shader cache");
			m_ShaderCacheDirty = true;
		}
		else
		{
			byte *cache = new byte[(size_t)cachelen];
			FileIO::fread(cache, 1, (size_t)cachelen, f);

			uint32_t *header = (uint32_t *)cache;

			uint32_t version = header[0];

			if(version != m_ShaderCacheVersion)
			{
				RDCDEBUG("Out of date or invalid shader cache version: %d", version);
				m_ShaderCacheDirty = true;
			}
			else
			{
				uint32_t numentries = header[1];

				byte *ptr = cache+sizeof(uint32_t)*2;

				int64_t bufsize = (int64_t)cachelen-sizeof(uint32_t)*2;

				HMODULE d3dcompiler = GetD3DCompiler();

				if(d3dcompiler == NULL)
				{
					RDCFATAL("Can't get handle to d3dcompiler_??.dll");
				}

				typedef HRESULT (WINAPI *pD3DCreateBlob)(SIZE_T Size, ID3DBlob** ppBlob);

				pD3DCreateBlob blobCreate = (pD3DCreateBlob)GetProcAddress(d3dcompiler, "D3DCreateBlob");

				if(blobCreate == NULL)
				{
					RDCFATAL("Can't get D3DCreateBlob from d3dcompiler_??.dll");
				}

				m_ShaderCacheDirty = false;

				for(uint32_t i=0; i < numentries; i++)
				{
					if(bufsize < sizeof(uint32_t))
					{
						RDCERR("Invalid shader cache");
						m_ShaderCacheDirty = true;
						break;
					}

					uint32_t hash = *(uint32_t *)ptr; ptr += sizeof(uint32_t); bufsize -= sizeof(uint32_t);

					if(bufsize < sizeof(uint32_t))
					{
						RDCERR("Invalid shader cache");
						m_ShaderCacheDirty = true;
						break;
					}

					uint32_t len = *(uint32_t *)ptr; ptr += sizeof(uint32_t); bufsize -= sizeof(uint32_t);

					if(bufsize < len)
					{
						RDCERR("Invalid shader cache");
						m_ShaderCacheDirty = true;
						break;
					}

					byte *data = ptr; ptr += len; bufsize -= len;

					ID3DBlob *blob = NULL;
					HRESULT hr = blobCreate((SIZE_T)len, &blob);

					if(FAILED(hr))
					{
						RDCERR("Couldn't create blob of size %d from shadercache: %08x", len, hr);
						m_ShaderCacheDirty = true;
					}

					memcpy(blob->GetBufferPointer(), data, len);

					m_ShaderCache[hash] = blob;
				}

				if(bufsize != 0)
				{
					RDCERR("Invalid shader cache");
					m_ShaderCacheDirty = true;
				}

				RDCDEBUG("Successfully loaded %d shaders from shader cache", m_ShaderCache.size());
			}

			delete[] cache;
		}

		fclose(f);
	}

	m_CacheShaders = true;

	InitStreamOut();
	InitDebugRendering();
	InitFontRendering();

	m_CacheShaders = false;
	
	RenderDoc::Inst().SetProgress(DebugManagerInit, 1.0f);
}

D3D11DebugManager::~D3D11DebugManager()
{
	if(m_ShaderCacheDirty)
	{
		wstring shadercache = FileIO::GetAppFolderFilename(L"shaders.cache");

		FILE *f = FileIO::fopen(shadercache.c_str(), L"wb");
		if(f)
		{
			uint32_t version = m_ShaderCacheVersion;
			FileIO::fwrite(&version, 1, sizeof(version), f);
			uint32_t numentries = (uint32_t)m_ShaderCache.size();
			FileIO::fwrite(&numentries, 1, sizeof(numentries), f);
			
			auto it = m_ShaderCache.begin();
			for(uint32_t i=0; i < numentries; i++)
			{
				uint32_t hash = it->first;
				uint32_t len = (uint32_t)it->second->GetBufferSize();
				FileIO::fwrite(&hash, 1, sizeof(hash), f);
				FileIO::fwrite(&len, 1, sizeof(len), f);
				FileIO::fwrite(it->second->GetBufferPointer(), 1, len, f);

				it->second->Release();
				++it;
			}
			
			RDCDEBUG("Successfully wrote %d shaders to shader cache", m_ShaderCache.size());

			fclose(f);
		}
		else
		{
			RDCERR("Error opening shader cache for write");
		}
	}

	ShutdownFontRendering();
	ShutdownStreamOut();
				
	if(m_OverlayResourceId != ResourceId())
		SAFE_RELEASE(m_OverlayRenderTex);

	SAFE_RELEASE(m_CustomShaderRTV);
	
	if(m_CustomShaderResourceId != ResourceId())
		SAFE_RELEASE(m_CustomShaderTex);

	SAFE_RELEASE(m_pFactory);

	while(!m_ShaderItemCache.empty())
	{
		CacheElem &elem = m_ShaderItemCache.back();
		elem.Release();
		m_ShaderItemCache.pop_back();
	}

	for(auto it=m_PostVSData.begin(); it != m_PostVSData.end(); ++it)
	{
		SAFE_RELEASE(it->second.vsout.buf);
		SAFE_RELEASE(it->second.vsout.idxBuf);
		SAFE_RELEASE(it->second.gsout.buf);
		SAFE_RELEASE(it->second.gsout.idxBuf);
	}

	m_PostVSData.clear();
	
	SAFE_RELEASE(m_WrappedContext);
	m_WrappedDevice->InternalRelease();
	SAFE_RELEASE(m_pImmediateContext);
	
	if(RenderDoc::Inst().GetCrashHandler())
		RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

//////////////////////////////////////////////////////
// debug/replay functions

static uint32_t strhash(const char *str, uint32_t seed = 5381)
{
	if(str == NULL) return seed;

    uint32_t hash = seed;
    int c = *str;
	str++;

    while(c)
	{
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
		c = *str;
		str++;
	}

    return hash;
}

string D3D11DebugManager::GetShaderBlob(const char *source, const char *entry, const uint32_t compileFlags, const char *profile, ID3DBlob **srcblob)
{
	uint32_t hash = strhash(source);
	hash = strhash(entry, hash);
	hash = strhash(profile, hash);
	hash ^= compileFlags;

	if(m_ShaderCache.find(hash) != m_ShaderCache.end())
	{
		*srcblob = m_ShaderCache[hash];
		(*srcblob)->AddRef();
		return "";
	}

	HRESULT hr = S_OK;

	ID3DBlob *byteBlob = NULL;
	ID3DBlob *errBlob = NULL;

	HMODULE d3dcompiler = GetD3DCompiler();

	if(d3dcompiler == NULL)
	{
		RDCFATAL("Can't get handle to d3dcompiler_??.dll");
	}

	pD3DCompile compileFunc = (pD3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");

	if(compileFunc == NULL)
	{
		RDCFATAL("Can't get D3DCompile from d3dcompiler_??.dll");
	}

	uint32_t flags = compileFlags & ~D3DCOMPILE_NO_PRESHADER;

	hr = compileFunc(source, strlen(source), entry, NULL, NULL, entry, profile,
										flags, 0, &byteBlob, &errBlob);
	
	string errors = "";

	if(errBlob)
	{
		errors = (char *)errBlob->GetBufferPointer();

		string logerror = errors;
		if(logerror.length() > 1024)
			logerror = logerror.substr(0, 1024) + "...";

		RDCWARN("Shader compile error in '%hs':\n%hs", entry, logerror.c_str());

		SAFE_RELEASE(errBlob);

		if(FAILED(hr))
		{
			SAFE_RELEASE(byteBlob);
			return errors;
		}
	}
	
	void *bytecode = byteBlob->GetBufferPointer();
	size_t bytecodeLen = byteBlob->GetBufferSize();

	if(m_CacheShaders)
	{
		m_ShaderCache[hash] = byteBlob;
		byteBlob->AddRef();
		m_ShaderCacheDirty = true;
	}

	SAFE_RELEASE(errBlob);
	
	*srcblob = byteBlob;
	return errors;
}

ID3D11VertexShader *D3D11DebugManager::MakeVShader(const char *source, const char *entry, const char *profile,
														int numInputDescs, D3D11_INPUT_ELEMENT_DESC *inputs, ID3D11InputLayout **ret,
														vector<byte> *blob)
{
	ID3DBlob *byteBlob = NULL;

	if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
	{
		RDCERR("Couldn't get shader blob for %hs", entry);
		return NULL;
	}
	
	void *bytecode = byteBlob->GetBufferPointer();
	size_t bytecodeLen = byteBlob->GetBufferSize();

	ID3D11VertexShader *ps = NULL;

	HRESULT hr = m_pDevice->CreateVertexShader(bytecode, bytecodeLen, NULL, &ps);
	
	if(FAILED(hr))
	{
		RDCERR("Couldn't create vertex shader for %hs %08x", entry, hr);
	
		SAFE_RELEASE(byteBlob);

		return NULL;
	}

	if(numInputDescs)
	{
		hr = m_pDevice->CreateInputLayout(inputs, numInputDescs, bytecode, bytecodeLen, ret);
	
		if(FAILED(hr))
		{
			RDCERR("Couldn't create input layout for %hs %08x", entry, hr);
		}
	}

	if(blob)
	{
		blob->resize(bytecodeLen);
		memcpy(&(*blob)[0], bytecode, bytecodeLen);
	}
	
	SAFE_RELEASE(byteBlob);

	return ps;
}

ID3D11GeometryShader *D3D11DebugManager::MakeGShader(const char *source, const char *entry, const char *profile)
{
	ID3DBlob *byteBlob = NULL;

	if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
	{
		return NULL;
	}
	
	void *bytecode = byteBlob->GetBufferPointer();
	size_t bytecodeLen = byteBlob->GetBufferSize();

	ID3D11GeometryShader *gs = NULL;

	HRESULT hr = m_pDevice->CreateGeometryShader(bytecode, bytecodeLen, NULL, &gs);
	
	SAFE_RELEASE(byteBlob);
	
	if(FAILED(hr))
	{
		RDCERR("Couldn't create geometry shader for %hs %08x", entry, hr);
		return NULL;
	}

	return gs;
}

ID3D11PixelShader *D3D11DebugManager::MakePShader(const char *source, const char *entry, const char *profile)
{
	ID3DBlob *byteBlob = NULL;

	if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
	{
		return NULL;
	}
	
	void *bytecode = byteBlob->GetBufferPointer();
	size_t bytecodeLen = byteBlob->GetBufferSize();

	ID3D11PixelShader *ps = NULL;

	HRESULT hr = m_pDevice->CreatePixelShader(bytecode, bytecodeLen, NULL, &ps);
	
	SAFE_RELEASE(byteBlob);
	
	if(FAILED(hr))
	{
		RDCERR("Couldn't create pixel shader for %hs %08x", entry, hr);
		return NULL;
	}

	return ps;
}

ID3D11ComputeShader *D3D11DebugManager::MakeCShader(const char *source, const char *entry, const char *profile)
{
	ID3DBlob *byteBlob = NULL;

	if(GetShaderBlob(source, entry, D3DCOMPILE_WARNINGS_ARE_ERRORS, profile, &byteBlob) != "")
	{
		return NULL;
	}
	
	void *bytecode = byteBlob->GetBufferPointer();
	size_t bytecodeLen = byteBlob->GetBufferSize();

	ID3D11ComputeShader *cs = NULL;

	HRESULT hr = m_pDevice->CreateComputeShader(bytecode, bytecodeLen, NULL, &cs);

	SAFE_RELEASE(byteBlob);

	if(FAILED(hr))
	{
		RDCERR("Couldn't create compute shader for %hs %08x", entry, hr);
		return NULL;
	}

	return cs;
}

void D3D11DebugManager::BuildShader(string source, string entry, const uint32_t compileFlags, ShaderStageType type, ResourceId *id, string *errors)
{
	if(id == NULL || errors == NULL)
	{
		if(id) *id = ResourceId();
		return;
	}

	char *profile = NULL;

	switch(type)
	{
		case eShaderStage_Vertex:	profile = "vs_5_0"; break;
		case eShaderStage_Hull:		profile = "hs_5_0"; break;
		case eShaderStage_Domain:	profile = "ds_5_0"; break;
		case eShaderStage_Geometry:	profile = "gs_5_0"; break;
		case eShaderStage_Pixel:		profile = "ps_5_0"; break;
		case eShaderStage_Compute:	profile = "cs_5_0"; break;
		default: RDCERR("Unexpected type in BuildShader!"); *id = ResourceId(); return;
	}

	ID3DBlob *blob = NULL;
	*errors = GetShaderBlob(source.c_str(), entry.c_str(), compileFlags, profile, &blob);

	if(blob == NULL)
	{
		*id = ResourceId();
		return;
	}
	
	switch(type)
	{
		case eShaderStage_Vertex:
		{
			ID3D11VertexShader *sh = NULL;
			m_WrappedDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

			SAFE_RELEASE(blob);
		
			*id = ((WrappedID3D11Shader<ID3D11VertexShader> *)sh)->GetResourceID();
			return;
		}
		case eShaderStage_Hull:
		{
			ID3D11HullShader *sh = NULL;
			m_WrappedDevice->CreateHullShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

			SAFE_RELEASE(blob);
		
			*id = ((WrappedID3D11Shader<ID3D11HullShader> *)sh)->GetResourceID();
			return;
		}
		case eShaderStage_Domain:
		{
			ID3D11DomainShader *sh = NULL;
			m_WrappedDevice->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

			SAFE_RELEASE(blob);
		
			*id = ((WrappedID3D11Shader<ID3D11DomainShader> *)sh)->GetResourceID();
			return;
		}
		case eShaderStage_Geometry:
		{
			ID3D11GeometryShader *sh = NULL;
			m_WrappedDevice->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

			SAFE_RELEASE(blob);
		
			*id = ((WrappedID3D11Shader<ID3D11GeometryShader> *)sh)->GetResourceID();
			return;
		}
		case eShaderStage_Pixel:
		{
			ID3D11PixelShader *sh = NULL;
			m_WrappedDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

			SAFE_RELEASE(blob);
		
			*id = ((WrappedID3D11Shader<ID3D11PixelShader> *)sh)->GetResourceID();
			return;
		}
		case eShaderStage_Compute:
		{
			ID3D11ComputeShader *sh = NULL;
			m_WrappedDevice->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &sh);

			SAFE_RELEASE(blob);
		
			*id = ((WrappedID3D11Shader<ID3D11ComputeShader> *)sh)->GetResourceID();
			return;
		}
		default:
			break;
	}

	SAFE_RELEASE(blob);

	RDCERR("Unexpected type in BuildShader!");
	*id = ResourceId();
}

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(UINT size)
{
	D3D11_BUFFER_DESC bufDesc;

	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = size;
	bufDesc.StructureByteStride = 0;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.MiscFlags = 0;

	ID3D11Buffer *ret = NULL;

	HRESULT hr = m_pDevice->CreateBuffer(&bufDesc, NULL, &ret);

	if(FAILED(hr))
	{
		RDCERR("Failed to create CBuffer %08x", hr);
		return NULL;
	}

	return ret;
}

void D3D11DebugManager::FillCBuffer(ID3D11Buffer *buf, float *data, size_t size)
{
	D3D11_MAPPED_SUBRESOURCE mapped;

	HRESULT hr = m_pImmediateContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if(FAILED(hr))
	{
		RDCERR("Can't fill cbuffer %08x", hr);
	}
	else
	{
		memcpy(mapped.pData, data, size);
		m_pImmediateContext->Unmap(buf, 0);
	}
}

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(float *data, size_t size)
{
	int idx = m_DebugRender.publicCBufIdx;

	FillCBuffer(m_DebugRender.PublicCBuffers[idx], data, size);

	m_DebugRender.publicCBufIdx = (m_DebugRender.publicCBufIdx+1)%ARRAY_COUNT(m_DebugRender.PublicCBuffers);

	return m_DebugRender.PublicCBuffers[idx];
}

#include "data/hlsl/debugcbuffers.h"

bool D3D11DebugManager::InitDebugRendering()
{
	HRESULT hr = S_OK;
	
	m_CustomShaderTex = NULL;
	m_CustomShaderRTV = NULL;
	m_CustomShaderResourceId = ResourceId();
	
	m_OverlayRenderTex = NULL;
	m_OverlayResourceId = ResourceId();

	m_DebugRender.GenericVSCBuffer = MakeCBuffer(sizeof(DebugVertexCBuffer));
	m_DebugRender.GenericGSCBuffer = MakeCBuffer(sizeof(DebugGeometryCBuffer));
	m_DebugRender.GenericPSCBuffer = MakeCBuffer(sizeof(DebugPixelCBufferData));

	for(int i=0; i < ARRAY_COUNT(m_DebugRender.PublicCBuffers); i++)
		m_DebugRender.PublicCBuffers[i] = MakeCBuffer(sizeof(float)*4 * 100);

	m_DebugRender.publicCBufIdx = 0;

	string displayhlsl = GetEmbeddedResource(debugcbuffers_h);
	displayhlsl += GetEmbeddedResource(debugcommon_hlsl);
	displayhlsl += GetEmbeddedResource(debugdisplay_hlsl);

	D3D11_INPUT_ELEMENT_DESC inputDesc;

	inputDesc.SemanticName = "POSITION";
	inputDesc.SemanticIndex = 0;
	inputDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputDesc.InputSlot = 0;
	inputDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	inputDesc.AlignedByteOffset = 0;
	inputDesc.InstanceDataStepRate = 0;

	vector<byte> bytecode;

	m_DebugRender.GenericVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_DebugVS", "vs_4_0", 1, &inputDesc, &m_DebugRender.GenericLayout);
	m_DebugRender.TexDisplayPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_TexDisplayPS", "ps_5_0");
	m_DebugRender.WireframeVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_WireframeVS", "vs_4_0");
	m_DebugRender.MeshVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_MeshVS", "vs_4_0", 0, NULL, NULL, &bytecode);
	m_DebugRender.MeshGS = MakeGShader(displayhlsl.c_str(), "RENDERDOC_MeshGS", "gs_4_0");
	m_DebugRender.MeshPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_MeshPS", "ps_4_0");

	m_DebugRender.MeshVSBytecode = new byte[bytecode.size()];
	m_DebugRender.MeshVSBytelen = (uint32_t)bytecode.size();
	memcpy(m_DebugRender.MeshVSBytecode, &bytecode[0], bytecode.size());
	
	inputDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	m_DebugRender.WireframeHomogVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_WireframeHomogVS", "vs_4_0", 1, &inputDesc, &m_DebugRender.GenericHomogLayout);
	m_DebugRender.WireframePS = MakePShader(displayhlsl.c_str(), "RENDERDOC_WireframePS", "ps_4_0");
	m_DebugRender.FullscreenVS = MakeVShader(displayhlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");
	m_DebugRender.OverlayPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_OverlayPS", "ps_4_0");
	m_DebugRender.CheckerboardPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_CheckerboardPS", "ps_4_0");

	m_DebugRender.QuadOverdrawPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_QuadOverdrawPS", "ps_5_0");
	m_DebugRender.QOResolvePS = MakePShader(displayhlsl.c_str(), "RENDERDOC_QOResolvePS", "ps_5_0");

	m_DebugRender.PixelHistoryUnusedCS = MakeCShader(displayhlsl.c_str(), "RENDERDOC_PixelHistoryUnused", "cs_5_0");
	m_DebugRender.PixelHistoryCopyCS = MakeCShader(displayhlsl.c_str(), "RENDERDOC_PixelHistoryCopyPixel", "cs_5_0");
	m_DebugRender.PrimitiveIDPS = MakePShader(displayhlsl.c_str(), "RENDERDOC_PrimitiveIDPS", "ps_5_0");
	
	string multisamplehlsl = GetEmbeddedResource(multisample_hlsl);

	m_DebugRender.CopyMSToArrayPS = MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyMSToArray", "ps_5_0");
	m_DebugRender.CopyArrayToMSPS = MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyArrayToMS", "ps_5_0");
	m_DebugRender.FloatCopyMSToArrayPS = MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyMSToArray", "ps_5_0");
	m_DebugRender.FloatCopyArrayToMSPS = MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyArrayToMS", "ps_5_0");
	m_DebugRender.DepthCopyMSToArrayPS = MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyMSToArray", "ps_5_0");
	m_DebugRender.DepthCopyArrayToMSPS = MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyArrayToMS", "ps_5_0");

	string histogramhlsl = GetEmbeddedResource(debugcbuffers_h);
	histogramhlsl += GetEmbeddedResource(debugcommon_hlsl);
	histogramhlsl += GetEmbeddedResource(histogram_hlsl);

	RenderDoc::Inst().SetProgress(DebugManagerInit, 0.1f);

	if(RenderDoc::Inst().IsReplayApp())
	{
		for(int t=eTexType_1D; t < eTexType_Max; t++)
		{
			// float, uint, sint
			for(int i=0; i < 3; i++)
			{
				string hlsl = string("#define SHADER_RESTYPE ") + ToStr::Get(t) + "\n";
				hlsl += string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
				hlsl += string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
				hlsl += histogramhlsl;

				m_DebugRender.TileMinMaxCS[t][i] = MakeCShader(hlsl.c_str(), "RENDERDOC_TileMinMaxCS", "cs_5_0");
				m_DebugRender.HistogramCS[t][i] = MakeCShader(hlsl.c_str(), "RENDERDOC_HistogramCS", "cs_5_0");

				if(t == 1)
					m_DebugRender.ResultMinMaxCS[i] = MakeCShader(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS", "cs_5_0");

				RenderDoc::Inst().SetProgress(DebugManagerInit, (float(i + 3.0f*t)/float(2.0f + 3.0f*(eTexType_Max-1)))*0.7f+0.1f);
			}
		}
	}
	
	RenderDoc::Inst().SetProgress(DebugManagerInit, 0.8f);
	
	RDCCOMPILE_ASSERT(eTexType_1D == RESTYPE_TEX1D, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_2D == RESTYPE_TEX2D, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_3D == RESTYPE_TEX3D, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_Depth == RESTYPE_DEPTH, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_Stencil == RESTYPE_DEPTH_STENCIL, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_DepthMS == RESTYPE_DEPTH_MS, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_StencilMS == RESTYPE_DEPTH_STENCIL_MS, "Tex type enum doesn't match shader defines");
	RDCCOMPILE_ASSERT(eTexType_2DMS == RESTYPE_TEX2D_MS, "Tex type enum doesn't match shader defines");

	D3D11_BLEND_DESC blendDesc;
	RDCEraseEl(blendDesc);

	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_pDevice->CreateBlendState(&blendDesc, &m_DebugRender.BlendState);

	if(FAILED(hr))
	{
		RDCERR("Failed to create default blendstate %08x", hr);
	}

	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;

	hr = m_pDevice->CreateBlendState(&blendDesc, &m_DebugRender.NopBlendState);

	if(FAILED(hr))
	{
		RDCERR("Failed to create nop blendstate %08x", hr);
	}
	
	D3D11_RASTERIZER_DESC rastDesc;
	RDCEraseEl(rastDesc);

	rastDesc.CullMode = D3D11_CULL_NONE;
	rastDesc.FillMode = D3D11_FILL_SOLID;
	rastDesc.DepthBias = 0;

	hr = m_pDevice->CreateRasterizerState(&rastDesc, &m_DebugRender.RastState);

	if(FAILED(hr))
	{
		RDCERR("Failed to create default rasterizer state %08x", hr);
	}

	D3D11_SAMPLER_DESC sampDesc;
	RDCEraseEl(sampDesc);

	sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sampDesc.MaxAnisotropy = 1;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = FLT_MAX;
	sampDesc.MipLODBias = 0.0f;

	hr = m_pDevice->CreateSamplerState(&sampDesc, &m_DebugRender.LinearSampState);

	if(FAILED(hr))
	{
		RDCERR("Failed to create linear sampler state %08x", hr);
	}
	
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

	hr = m_pDevice->CreateSamplerState(&sampDesc, &m_DebugRender.PointSampState);

	if(FAILED(hr))
	{
		RDCERR("Failed to create point sampler state %08x", hr);
	}

	{
		D3D11_DEPTH_STENCIL_DESC desc;
		
		desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.DepthEnable = FALSE;
		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.StencilEnable = FALSE;
		desc.StencilReadMask = desc.StencilWriteMask = 0xff;
		
		hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.NoDepthState);

		if(FAILED(hr))
		{
			RDCERR("Failed to create no-depth depthstencilstate %08x", hr);
		}

		desc.DepthEnable = TRUE;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		
		hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.LEqualDepthState);

		if(FAILED(hr))
		{
			RDCERR("Failed to create less-equal depthstencilstate %08x", hr);
		}

		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.StencilEnable = TRUE;
		
		hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.AllPassDepthState);

		if(FAILED(hr))
		{
			RDCERR("Failed to create always pass depthstencilstate %08x", hr);
		}

		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.StencilReadMask = desc.StencilWriteMask = 0;
		desc.StencilEnable = FALSE;
		
		hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.NopDepthState);

		if(FAILED(hr))
		{
			RDCERR("Failed to create nop depthstencilstate %08x", hr);
		}
		
		desc.StencilReadMask = desc.StencilWriteMask = 0xff;
		desc.StencilEnable = TRUE;
		desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
		desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		
		hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.AllPassIncrDepthState);

		if(FAILED(hr))
		{
			RDCERR("Failed to create always pass stencil increment depthstencilstate %08x", hr);
		}
		
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		
		hr = m_pDevice->CreateDepthStencilState(&desc, &m_DebugRender.StencIncrEqDepthState);

		if(FAILED(hr))
		{
			RDCERR("Failed to create always pass stencil increment depthstencilstate %08x", hr);
		}
	}

	{
		D3D11_SUBRESOURCE_DATA initialPos;

		float *buf = new float[(2 + FONT_MAX_CHARS*4) *3];
		
		// tri strip with degenerates to split characters:
		//
		// 0--24--68--..
		// | /|| /|| /
		// |/ ||/ ||/
		// 1--35--79--..

		buf[0] = 0.0f;
		buf[1] = 0.0f;
		buf[2] = 0.0f;

		buf[3] = 0.0f;
		buf[4] = -1.0f;
		buf[5] = 0.0f;

		for(int i=1; i <= FONT_MAX_CHARS; i++)
		{
			buf[i*12 - 6 + 0] = 1.0f;
			buf[i*12 - 6 + 1] = 0.0f;
			buf[i*12 - 6 + 2] = float(i-1);

			buf[i*12 - 6 + 3] = 1.0f;
			buf[i*12 - 6 + 4] = -1.0f;
			buf[i*12 - 6 + 5] = float(i-1);


			buf[i*12 + 0 + 0] = 0.0f;
			buf[i*12 + 0 + 1] = 0.0f;
			buf[i*12 + 0 + 2] = float(i);

			buf[i*12 + 0 + 3] = 0.0f;
			buf[i*12 + 0 + 4] = -1.0f;
			buf[i*12 + 0 + 5] = float(i);
		}

		initialPos.pSysMem = buf;
		initialPos.SysMemPitch = initialPos.SysMemSlicePitch = 0;
		
		D3D11_BUFFER_DESC bufDesc;
		
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = (2 + FONT_MAX_CHARS*4) *3*sizeof(float);
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		
		hr = m_pDevice->CreateBuffer(&bufDesc, &initialPos, &m_DebugRender.PosBuffer);

		if(FAILED(hr))
		{
			RDCERR("Failed to create font pos buffer %08x", hr);
		}

		delete[] buf;
	}
	
	RenderDoc::Inst().SetProgress(DebugManagerInit, 0.9f);

	{
		float data[] = {
			0.0f, -1.0f, 0.0f,
			1.0f, -1.0f, 0.0f,
			1.0f,  0.0f, 0.0f,
			0.0f,  0.0f, 0.0f,
			0.0f, -1.0f, 0.0f,
		};

		D3D11_SUBRESOURCE_DATA initialPos;

		initialPos.pSysMem = data;
		initialPos.SysMemPitch = initialPos.SysMemSlicePitch = 0;
		
		D3D11_BUFFER_DESC bufDesc;
		
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufDesc.ByteWidth = sizeof(data);
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		
		hr = m_pDevice->CreateBuffer(&bufDesc, &initialPos, &m_DebugRender.OutlineStripVB);

		if(FAILED(hr))
		{
			RDCERR("Failed to create outline strip buffer %08x", hr);
		}
	}

	{
		D3D11_TEXTURE2D_DESC desc;
		ID3D11Texture2D *pickTex = NULL;

		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.Width = 100;
		desc.Height = 100;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = 0;

		hr = m_pDevice->CreateTexture2D(&desc, NULL, &pickTex);

		if(FAILED(hr))
		{
			RDCERR("Failed to create pick tex %08x", hr);
		}
		else
		{
			hr = m_pDevice->CreateRenderTargetView(pickTex, NULL, &m_DebugRender.PickPixelRT);

			if(FAILED(hr))
			{
				RDCERR("Failed to create pick rt %08x", hr);
			}

			SAFE_RELEASE(pickTex);
		}
	}

	{
		D3D11_TEXTURE2D_DESC desc;
		RDCEraseEl(desc);
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.Width = 1;
		desc.Height = 1;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

		hr = m_pDevice->CreateTexture2D(&desc, NULL, &m_DebugRender.PickPixelStageTex);

		if(FAILED(hr))
		{
			RDCERR("Failed to create pick stage tex %08x", hr);
		}
	}

	{
		D3D11_BUFFER_DESC bDesc;

		const uint32_t maxTexDim = 16384;
		const uint32_t blockPixSize = HGRAM_TILES_PER_BLOCK*HGRAM_PIXELS_PER_TILE;
		const uint32_t maxBlocksNeeded = (maxTexDim*maxTexDim)/(blockPixSize*blockPixSize);

		bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bDesc.ByteWidth = 2*4*sizeof(float)*HGRAM_TILES_PER_BLOCK*HGRAM_TILES_PER_BLOCK*maxBlocksNeeded;
		bDesc.CPUAccessFlags = 0;
		bDesc.MiscFlags = 0;
		bDesc.StructureByteStride = 0;
		bDesc.Usage = D3D11_USAGE_DEFAULT;

		hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.tileResultBuff);

		if(FAILED(hr))
		{
			RDCERR("Failed to create tile result buffer %08x", hr);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvDesc.Buffer.ElementOffset = 0;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.ElementWidth = 4*sizeof(float);
		srvDesc.Buffer.NumElements = bDesc.ByteWidth/srvDesc.Buffer.ElementWidth;

		hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc, &m_DebugRender.tileResultSRV[0]);

		if(FAILED(hr))
			RDCERR("Failed to create tile result SRV 0 %08x", hr);

		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
		hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc, &m_DebugRender.tileResultSRV[1]);

		if(FAILED(hr))
			RDCERR("Failed to create tile result SRV 1 %08x", hr);

		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
		hr = m_pDevice->CreateShaderResourceView(m_DebugRender.tileResultBuff, &srvDesc, &m_DebugRender.tileResultSRV[2]);

		if(FAILED(hr))
			RDCERR("Failed to create tile result SRV 2 %08x", hr);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = 0;
		uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements;

		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc, &m_DebugRender.tileResultUAV[0]);

		if(FAILED(hr))
			RDCERR("Failed to create tile result UAV 0 %08x", hr);

		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc, &m_DebugRender.tileResultUAV[1]);

		if(FAILED(hr))
			RDCERR("Failed to create tile result UAV 1 %08x", hr);

		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.tileResultBuff, &uavDesc, &m_DebugRender.tileResultUAV[2]);

		if(FAILED(hr))
			RDCERR("Failed to create tile result UAV 2 %08x", hr);
		
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
		bDesc.ByteWidth = uavDesc.Buffer.NumElements*sizeof(int);

		hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.histogramBuff);

		if(FAILED(hr))
			RDCERR("Failed to create histogram buff %08x", hr);

		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.histogramBuff, &uavDesc, &m_DebugRender.histogramUAV);

		if(FAILED(hr))
			RDCERR("Failed to create histogram UAV %08x", hr);

		bDesc.BindFlags = 0;
		bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bDesc.Usage = D3D11_USAGE_STAGING;

		hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.histogramStageBuff);

		if(FAILED(hr))
			RDCERR("Failed to create histogram stage buff %08x", hr);

		bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		bDesc.CPUAccessFlags = 0;
		bDesc.ByteWidth = 2*4*sizeof(float);
		bDesc.Usage = D3D11_USAGE_DEFAULT;

		hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.resultBuff);

		if(FAILED(hr))
			RDCERR("Failed to create result buff %08x", hr);
		
		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		uavDesc.Buffer.NumElements = 2;

		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc, &m_DebugRender.resultUAV[0]);

		if(FAILED(hr))
			RDCERR("Failed to create result UAV 0 %08x", hr);

		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc, &m_DebugRender.resultUAV[1]);

		if(FAILED(hr))
			RDCERR("Failed to create result UAV 1 %08x", hr);

		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
		hr = m_pDevice->CreateUnorderedAccessView(m_DebugRender.resultBuff, &uavDesc, &m_DebugRender.resultUAV[2]);

		if(FAILED(hr))
			RDCERR("Failed to create result UAV 2 %08x", hr);

		bDesc.BindFlags = 0;
		bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bDesc.Usage = D3D11_USAGE_STAGING;

		hr = m_pDevice->CreateBuffer(&bDesc, NULL, &m_DebugRender.resultStageBuff);

		if(FAILED(hr))
			RDCERR("Failed to create result stage buff %08x", hr);
	}

	{
		D3D11_BUFFER_DESC desc;

		desc.StructureByteStride = 0;
		desc.ByteWidth = STAGE_BUFFER_BYTE_SIZE;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.Usage = D3D11_USAGE_STAGING;

		hr = m_pDevice->CreateBuffer(&desc, NULL, &m_DebugRender.StageBuffer);

		if(FAILED(hr))
			RDCERR("Failed to create map staging buffer %08x", hr);
	}

	return true;
}

void D3D11DebugManager::ShutdownFontRendering()
{
}

void D3D11DebugManager::ShutdownStreamOut()
{
	SAFE_RELEASE(m_SOBuffer);
	SAFE_RELEASE(m_SOStatsQuery);
	SAFE_RELEASE(m_SOStagingBuffer);

	SAFE_RELEASE(m_WireframeHelpersRS);
	SAFE_RELEASE(m_WireframeHelpersBS);
	SAFE_RELEASE(m_SolidHelpersRS);

	SAFE_RELEASE(m_MeshDisplayLayout);

	SAFE_RELEASE(m_FrustumHelper);
	SAFE_RELEASE(m_AxisHelper);
	SAFE_RELEASE(m_TriHighlightHelper);
}

bool D3D11DebugManager::InitStreamOut()
{
	m_MeshDisplayLayout = NULL;
	m_MeshDisplayNULLVB = 0;
	m_PrevMeshInputLayout = NULL;

	D3D11_BUFFER_DESC bufferDesc =
	{
		m_SOBufferSize,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_STREAM_OUTPUT,
		0,
		0,
		0
	};
	HRESULT hr = S_OK;
	
	hr = m_pDevice->CreateBuffer( &bufferDesc, NULL, &m_SOBuffer );
	
	if(FAILED(hr)) RDCERR("Failed to create m_SOBuffer %08x", hr);

	bufferDesc.Usage = D3D11_USAGE_STAGING;
	bufferDesc.BindFlags = 0;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	hr = m_pDevice->CreateBuffer( &bufferDesc, NULL, &m_SOStagingBuffer );
	if(FAILED(hr)) RDCERR("Failed to create m_SOStagingBuffer %08x", hr);

	D3D11_QUERY_DESC qdesc;
	qdesc.MiscFlags = 0;
	qdesc.Query = D3D11_QUERY_SO_STATISTICS;

	hr = m_pDevice->CreateQuery(&qdesc, &m_SOStatsQuery);
	if(FAILED(hr)) RDCERR("Failed to create m_SOStatsQuery %08x", hr);

	D3D11_RASTERIZER_DESC desc;
	{
		desc.AntialiasedLineEnable = TRUE;
		desc.DepthBias = 0;
		desc.DepthBiasClamp = 0.0f;
		desc.DepthClipEnable = FALSE;
		desc.FrontCounterClockwise = FALSE;
		desc.MultisampleEnable = FALSE;
		desc.ScissorEnable = FALSE;
		desc.SlopeScaledDepthBias = 0.0f;
		desc.FillMode = D3D11_FILL_WIREFRAME;
		desc.CullMode = D3D11_CULL_NONE;

		hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersRS);
		if(FAILED(hr)) RDCERR("Failed to create m_WireframeHelpersRS %08x", hr);
		
		desc.FrontCounterClockwise = TRUE;
		desc.CullMode = D3D11_CULL_FRONT;

		hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersCullCCWRS);
		if(FAILED(hr)) RDCERR("Failed to create m_WireframeHelpersCullCCWRS %08x", hr);
		
		desc.FrontCounterClockwise = FALSE;
		desc.CullMode = D3D11_CULL_FRONT;

		hr = m_pDevice->CreateRasterizerState(&desc, &m_WireframeHelpersCullCWRS);
		if(FAILED(hr)) RDCERR("Failed to create m_WireframeHelpersCullCCWRS %08x", hr);
	}

	{
		D3D11_BLEND_DESC desc;
		RDCEraseEl(desc);

		desc.AlphaToCoverageEnable = TRUE;
		desc.IndependentBlendEnable = FALSE;
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].RenderTargetWriteMask = 0xf;

		hr = m_pDevice->CreateBlendState(&desc, &m_WireframeHelpersBS);
		if(FAILED(hr)) RDCERR("Failed to create m_WireframeHelpersRS %08x", hr);
	}

	{
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = D3D11_CULL_NONE;

		hr = m_pDevice->CreateRasterizerState(&desc, &m_SolidHelpersRS);
		if(FAILED(hr)) RDCERR("Failed to create m_SolidHelpersRS %08x", hr);
	}
	
	{
		Vec3f axisVB[6] =
		{
			Vec3f(0.0f, 0.0f, 0.0f),
			Vec3f(1.0f, 0.0f, 0.0f),
			Vec3f(0.0f, 0.0f, 0.0f),
			Vec3f(0.0f, 1.0f, 0.0f),
			Vec3f(0.0f, 0.0f, 0.0f),
			Vec3f(0.0f, 0.0f, 1.0f),
		};

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = axisVB;
		data.SysMemPitch = data.SysMemSlicePitch = 0;

		D3D11_BUFFER_DESC bdesc;
		bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = 0;
		bdesc.ByteWidth = sizeof(axisVB);
		bdesc.MiscFlags = 0;
		bdesc.Usage = D3D11_USAGE_IMMUTABLE;
		
		hr = m_pDevice->CreateBuffer(&bdesc, &data, &m_AxisHelper);
		if(FAILED(hr)) RDCERR("Failed to create m_AxisHelper %08x", hr);
	}
	
	{
		Vec3f TLN = Vec3f(-1.0f,  1.0f, 0.0f); // TopLeftNear, etc...
		Vec3f TRN = Vec3f( 1.0f,  1.0f, 0.0f);
		Vec3f BLN = Vec3f(-1.0f, -1.0f, 0.0f);
		Vec3f BRN = Vec3f( 1.0f, -1.0f, 0.0f);

		Vec3f TLF = Vec3f(-1.0f,  1.0f, 1.0f);
		Vec3f TRF = Vec3f( 1.0f,  1.0f, 1.0f);
		Vec3f BLF = Vec3f(-1.0f, -1.0f, 1.0f);
		Vec3f BRF = Vec3f( 1.0f, -1.0f, 1.0f);

		// 12 frustum lines => 24 verts
		Vec3f axisVB[24] =
		{
			TLN, TRN,
			TRN, BRN,
			BRN, BLN,
			BLN, TLN,

			TLN, TLF,
			TRN, TRF,
			BLN, BLF,
			BRN, BRF,

			TLF, TRF,
			TRF, BRF,
			BRF, BLF,
			BLF, TLF,
		};

		D3D11_SUBRESOURCE_DATA data;
		data.pSysMem = axisVB;
		data.SysMemPitch = data.SysMemSlicePitch = 0;

		D3D11_BUFFER_DESC bdesc;
		bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = 0;
		bdesc.ByteWidth = sizeof(axisVB);
		bdesc.MiscFlags = 0;
		bdesc.Usage = D3D11_USAGE_IMMUTABLE;

		hr = m_pDevice->CreateBuffer(&bdesc, &data, &m_FrustumHelper);

		if(FAILED(hr))
			RDCERR("Failed to create m_FrustumHelper %08x", hr);
	}

	{
		D3D11_BUFFER_DESC bdesc;
		bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bdesc.ByteWidth = sizeof(Vec4f)*16;
		bdesc.MiscFlags = 0;
		bdesc.Usage = D3D11_USAGE_DYNAMIC;

		hr = m_pDevice->CreateBuffer(&bdesc, NULL, &m_TriHighlightHelper);

		if(FAILED(hr))
			RDCERR("Failed to create m_TriHighlightHelper %08x", hr);
	}
		
	return true;
}

bool D3D11DebugManager::InitFontRendering()
{
	D3D11_TEXTURE2D_DESC desc;
	RDCEraseEl(desc);
	
	int width = FONT_TEX_WIDTH, height = FONT_TEX_HEIGHT;

	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Width = width;
	desc.Height = height;

	{
		int h=height>>1;

		desc.MipLevels = 1;

		while(h >= 8)
		{
			desc.MipLevels++;

			h >>= 1;
		}
	}
	desc.MiscFlags = 0;
	desc.SampleDesc.Quality = 0;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA *initialData = new D3D11_SUBRESOURCE_DATA[desc.MipLevels];

	/////////////////////////////////////////////////////////////////////
	BITMAPINFOHEADER bih;
	RDCEraseEl(bih);
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = width;
	bih.biHeight = height;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;
	
	byte **buffers = new byte *[desc.MipLevels];

	HDC pDC = GetDC(NULL);
	HDC MemDC = CreateCompatibleDC(pDC);

	SetBkColor(MemDC, RGB(0, 0, 0));
	SetTextColor(MemDC, RGB(255, 255, 255));

	HBITMAP *bmps = new HBITMAP[desc.MipLevels];

	for(UINT i=0; i < desc.MipLevels; i++)
	{
		int w = width>>i;
		int h = height>>i;

		bih.biWidth = w;
		bih.biHeight = h;

		HFONT font = CreateFont(h,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY, FIXED_PITCH,TEXT("Consolas"));

		bmps[i] = CreateCompatibleBitmap(pDC, w, h);

		SelectObject(MemDC, bmps[i]);

		SelectObject(MemDC, font);

		char str[2] = {0, 0};

		for(int s=0; s < 127-' '-1; s++)
		{
			str[0] = (char)(' '+s+1);
			TextOutA(MemDC, int(s*h*0.75), -1, str, 1);
		}

		byte *buf = buffers[i] = new byte[w*h*4];

		GetDIBits(MemDC, bmps[i], 0, h, buf, (BITMAPINFO *)&bih, DIB_RGB_COLORS);

		DeleteObject(font);

		// flip it right side up
		byte *tmpRow = new byte[w*4];
		for(int j=0; j < h/2; j++)
		{
			int x = h-j-1;
			memcpy(tmpRow, &buf[j*w*4], w*4);
			memcpy(&buf[j*w*4], &buf[x*w*4], w*4);
			memcpy(&buf[x*w*4], tmpRow, w*4);
		}
		delete[] tmpRow;

		/////////////////////////////////////////////////////////////////////

		initialData[i].pSysMem = buffers[i];
		initialData[i].SysMemPitch = w*4;
		initialData[i].SysMemSlicePitch = w*h*4;
	}
	
	ID3D11Texture2D *debugTex;

	HRESULT hr = S_OK;

	hr = m_pDevice->CreateTexture2D(&desc, initialData, &debugTex);

	if(FAILED(hr))
		RDCERR("Failed to create debugTex %08x", hr);

	delete[] initialData;

	hr = m_pDevice->CreateShaderResourceView(debugTex, NULL, &m_Font.Tex);

	if(FAILED(hr))
		RDCERR("Failed to create m_Font.Tex %08x", hr);

	SAFE_RELEASE(debugTex);

	for(UINT i=0; i < desc.MipLevels; i++)
	{
		SAFE_DELETE_ARRAY(buffers[i]);
		DeleteObject(bmps[i]);
	}
	SAFE_DELETE_ARRAY(buffers);
	SAFE_DELETE_ARRAY(bmps);

	DeleteDC(pDC);
	DeleteDC(MemDC);

	{
		HRESULT hr = S_OK;

		m_Font.CBuffer = MakeCBuffer(sizeof(FontCBuffer));

		D3D11_BUFFER_DESC bufDesc;
		
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.MiscFlags = 0;
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufDesc.ByteWidth = 2+FONT_MAX_CHARS*4*4;
		
		hr = m_pDevice->CreateBuffer(&bufDesc, NULL, &m_Font.CharBuffer);

		if(FAILED(hr))
			RDCERR("Failed to create m_Font.CharBuffer %08x", hr);
		
		string fullhlsl = "";
		{
			string textShaderHLSL = GetEmbeddedResource(debugtext_hlsl);
			string debugShaderCBuf = GetEmbeddedResource(debugcbuffers_h);

			fullhlsl = debugShaderCBuf + textShaderHLSL;
		}

		D3D11_INPUT_ELEMENT_DESC inputDescs[2];

		inputDescs[0].SemanticName = "POSITION";
		inputDescs[0].SemanticIndex = 0;
		inputDescs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		inputDescs[0].InputSlot = 0;
		inputDescs[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		inputDescs[0].AlignedByteOffset = 0;
		inputDescs[0].InstanceDataStepRate = 0;

		inputDescs[1].SemanticName = "TEXCOORD";
		inputDescs[1].SemanticIndex = 0;
		inputDescs[1].Format = DXGI_FORMAT_R32_UINT;
		inputDescs[1].InputSlot = 1;
		inputDescs[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		inputDescs[1].AlignedByteOffset = 0;
		inputDescs[1].InstanceDataStepRate = 0;
		
		m_Font.VS = MakeVShader(fullhlsl.c_str(), "RENDERDOC_TextVS", "vs_4_0", 2, inputDescs, &m_Font.Layout);
		m_Font.PS = MakePShader(fullhlsl.c_str(), "RENDERDOC_TextPS", "ps_4_0");
	}

	return true;
}

void D3D11DebugManager::OutputWindow::MakeRTV()
{
	ID3D11Texture2D *texture = NULL;
	HRESULT hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&texture);
	
	if(FAILED(hr))
	{
		RDCERR("Failed to get swap chain buffer, HRESULT: 0x%08x", hr);
		SAFE_RELEASE(texture);
		return;
	}

	hr = dev->CreateRenderTargetView(texture, NULL, &rtv);

	SAFE_RELEASE(texture);

	if(FAILED(hr))
	{
		RDCERR("Failed to create RTV for swap chain buffer, HRESULT: 0x%08x", hr);
		SAFE_RELEASE(swap);
		return;
	}
}

void D3D11DebugManager::OutputWindow::MakeDSV()
{
	ID3D11Texture2D *texture = NULL;
	HRESULT hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&texture);
	
	if(FAILED(hr))
	{
		RDCERR("Failed to get swap chain buffer, HRESULT: 0x%08x", hr);
		SAFE_RELEASE(texture);
		return;
	}

	D3D11_TEXTURE2D_DESC texDesc;
	texture->GetDesc(&texDesc);

	SAFE_RELEASE(texture);

	texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	
	hr = dev->CreateTexture2D(&texDesc, NULL, &texture);

	if(FAILED(hr))
	{
		RDCERR("Failed to create DSV texture for main output, HRESULT: 0x%08x", hr);
		SAFE_RELEASE(swap);
		SAFE_RELEASE(rtv);
		return;
	}

	hr = dev->CreateDepthStencilView(texture, NULL, &dsv);

	SAFE_RELEASE(texture);

	if(FAILED(hr))
	{
		RDCERR("Failed to create DSV for main output, HRESULT: 0x%08x", hr);
		SAFE_RELEASE(swap);
		SAFE_RELEASE(rtv);
		return;
	}
}

uint64_t D3D11DebugManager::MakeOutputWindow(void *w, bool depth)
{
	OutputWindow outw;
	outw.wnd = (HWND)w;
	outw.dev = m_WrappedDevice;
	
	DXGI_SWAP_CHAIN_DESC swapDesc;
	RDCEraseEl(swapDesc);

	RECT rect;GetClientRect(outw.wnd, &rect);

	swapDesc.BufferCount = 2;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	outw.width = swapDesc.BufferDesc.Width = rect.right-rect.left;
	outw.height = swapDesc.BufferDesc.Height = rect.bottom-rect.top;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;
	swapDesc.OutputWindow = outw.wnd;
	swapDesc.Windowed = TRUE;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapDesc.Flags = 0;

	HRESULT hr = S_OK;

	hr = m_pFactory->CreateSwapChain(m_WrappedDevice, &swapDesc, &outw.swap);

	if(FAILED(hr))
	{
		RDCERR("Failed to create swap chain for HWND, HRESULT: 0x%08x", hr);
		return 0;
	}

	outw.MakeRTV();

	outw.dsv = NULL;
	if(depth) outw.MakeDSV();

	uint64_t id = m_OutputWindowID++;
	m_OutputWindows[id] = outw;
	return id;
}

void D3D11DebugManager::DestroyOutputWindow(uint64_t id)
{
	auto it = m_OutputWindows.find(id);
	if(id == 0 || it == m_OutputWindows.end())
		return;

	OutputWindow &outw = it->second;

	SAFE_RELEASE(outw.swap);
	SAFE_RELEASE(outw.rtv);
	SAFE_RELEASE(outw.dsv);

	m_OutputWindows.erase(it);
}

bool D3D11DebugManager::CheckResizeOutputWindow(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;
	
	OutputWindow &outw = m_OutputWindows[id];

	if(outw.wnd == NULL || outw.swap == NULL)
		return false;

	RECT rect;GetClientRect(outw.wnd, &rect);
	long w = rect.right-rect.left;
	long h = rect.bottom-rect.top;

	if(w != outw.width || h != outw.height)
	{
		outw.width = w;
		outw.height = h;

		m_WrappedContext->OMSetRenderTargets(0, 0, 0);
		
		if(outw.width > 0 && outw.height > 0)
		{
			SAFE_RELEASE(outw.rtv);
			SAFE_RELEASE(outw.dsv);

			DXGI_SWAP_CHAIN_DESC desc;
			outw.swap->GetDesc(&desc);

			HRESULT hr = outw.swap->ResizeBuffers(desc.BufferCount, outw.width, outw.height, desc.BufferDesc.Format, desc.Flags);

			if(FAILED(hr))
			{
				RDCERR("Failed to resize swap chain, HRESULT: 0x%08x", hr);
				return true;
			}

			outw.MakeRTV();
			outw.MakeDSV();
		}

		return true;
	}

	return false;
}

void D3D11DebugManager::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	w = m_OutputWindows[id].width;
	h = m_OutputWindows[id].height;
}

void D3D11DebugManager::ClearOutputWindowColour(uint64_t id, float col[4])
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	m_WrappedContext->ClearRenderTargetView(m_OutputWindows[id].rtv, col);
}

void D3D11DebugManager::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;

	if(m_OutputWindows[id].dsv)
		m_WrappedContext->ClearDepthStencilView(m_OutputWindows[id].dsv, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, depth, stencil);
}

void D3D11DebugManager::BindOutputWindow(uint64_t id, bool depth)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	m_WrappedContext->OMSetRenderTargets(1, &m_OutputWindows[id].rtv, depth && m_OutputWindows[id].dsv ? m_OutputWindows[id].dsv : NULL);
	
	D3D11_VIEWPORT viewport = { 0, 0, (float)m_OutputWindows[id].width, (float)m_OutputWindows[id].height, 0.0f, 1.0f };
	m_WrappedContext->RSSetViewports(1, &viewport);
	
	SetOutputDimensions(m_OutputWindows[id].width, m_OutputWindows[id].height);
}

bool D3D11DebugManager::IsOutputWindowVisible(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return false;

	return (IsWindowVisible(m_OutputWindows[id].wnd) == TRUE);
}
	
void D3D11DebugManager::FlipOutputWindow(uint64_t id)
{
	if(id == 0 || m_OutputWindows.find(id) == m_OutputWindows.end())
		return;
	
	if(m_OutputWindows[id].swap)
		m_OutputWindows[id].swap->Present(0, 0);
}

uint32_t D3D11DebugManager::GetStructCount(ID3D11UnorderedAccessView *uav)
{
	m_pImmediateContext->CopyStructureCount(m_DebugRender.StageBuffer, 0, UNWRAP(WrappedID3D11UnorderedAccessView, uav));

	D3D11_MAPPED_SUBRESOURCE mapped;
	HRESULT hr = m_pImmediateContext->Map(m_DebugRender.StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);

	if(FAILED(hr))
	{
		RDCERR("Failed to Map %08x", hr);
		return ~0U;
	}

	uint32_t ret = *((uint32_t *)mapped.pData);

	m_pImmediateContext->Unmap(m_DebugRender.StageBuffer, 0);
	
	return ret;
}

bool D3D11DebugManager::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float minval, float maxval, bool channels[4], vector<uint32_t> &histogram)
{
	if(minval >= maxval) return false;
	
	TextureShaderDetails details = GetShaderDetails(texid, true);

	if(details.texFmt == DXGI_FORMAT_UNKNOWN)
		return false;
	
	D3D11RenderStateTracker tracker(m_WrappedContext);

	HistogramCBufferData cdata;
	cdata.HistogramTextureResolution.x = (float)RDCMAX(details.texWidth>>mip, 1U);
	cdata.HistogramTextureResolution.y = (float)RDCMAX(details.texHeight>>mip, 1U);
	cdata.HistogramTextureResolution.z = (float)RDCMAX(details.texDepth>>mip, 1U);
	cdata.HistogramSlice = (float)sliceFace;
	cdata.HistogramMip = mip;
	cdata.HistogramSample = (int)RDCCLAMP(sample, 0U, details.sampleCount-1);
	if(sample == ~0U) cdata.HistogramSample = -int(details.sampleCount);
	cdata.HistogramMin = minval;
	cdata.HistogramMax = maxval;
	cdata.HistogramChannels = 0;
	if(channels[0]) cdata.HistogramChannels |= 0x1;
	if(channels[1]) cdata.HistogramChannels |= 0x2;
	if(channels[2]) cdata.HistogramChannels |= 0x4;
	if(channels[3]) cdata.HistogramChannels |= 0x8;
	cdata.HistogramFlags = 0;
	
	int srvOffset = 0;
	int intIdx = 0;

	if(IsUIntFormat(details.texFmt))
	{
		cdata.HistogramFlags |= TEXDISPLAY_UINT_TEX;
		srvOffset = 10;
		intIdx = 1;
	}
	if(IsIntFormat(details.texFmt))
	{
		cdata.HistogramFlags |= TEXDISPLAY_SINT_TEX;
		srvOffset = 20;
		intIdx = 2;
	}
	
	if(details.texType == eTexType_3D)
		cdata.HistogramSlice = float(sliceFace)/float(details.texDepth);

	ID3D11Buffer *cbuf = MakeCBuffer((float *)&cdata, sizeof(cdata));

	UINT zeroes[] = { 0, 0, 0, 0 };
	m_pImmediateContext->ClearUnorderedAccessViewUint(m_DebugRender.histogramUAV, zeroes);

	m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);
	
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { 0 };
	UINT UAV_keepcounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = { (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1 };
	uavs[0] = m_DebugRender.histogramUAV;
	m_pImmediateContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs, UAV_keepcounts);

	m_pImmediateContext->CSSetConstantBuffers(0, 1, &cbuf);

	m_pImmediateContext->CSSetShaderResources(srvOffset, eTexType_Max, details.srv);

	ID3D11SamplerState *samps[] = { m_DebugRender.PointSampState, m_DebugRender.LinearSampState };
	m_pImmediateContext->CSSetSamplers(0, 2, samps);

	m_pImmediateContext->CSSetShader(m_DebugRender.HistogramCS[details.texType][intIdx], NULL, 0);

	int tilesX = (int)ceil(cdata.HistogramTextureResolution.x/float(HGRAM_PIXELS_PER_TILE*HGRAM_PIXELS_PER_TILE));
	int tilesY = (int)ceil(cdata.HistogramTextureResolution.y/float(HGRAM_PIXELS_PER_TILE*HGRAM_PIXELS_PER_TILE));

	m_pImmediateContext->Dispatch(tilesX, tilesY, 1);
	
	m_pImmediateContext->CopyResource(m_DebugRender.histogramStageBuff, m_DebugRender.histogramBuff);

	D3D11_MAPPED_SUBRESOURCE mapped;

	HRESULT hr = m_pImmediateContext->Map(m_DebugRender.histogramStageBuff, 0, D3D11_MAP_READ, 0, &mapped);

	histogram.clear();
	histogram.resize(HGRAM_NUM_BUCKETS);
	
	if(FAILED(hr))
	{
		RDCERR("Can't map histogram stage buff %08x", hr);
	}
	else
	{
		memcpy(&histogram[0], mapped.pData, sizeof(uint32_t)*HGRAM_NUM_BUCKETS);

		m_pImmediateContext->Unmap(m_DebugRender.histogramStageBuff, 0);
	}

	return true;
}
		
bool D3D11DebugManager::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample, float *minval, float *maxval)
{
	TextureShaderDetails details = GetShaderDetails(texid, true);

	if(details.texFmt == DXGI_FORMAT_UNKNOWN)
		return false;
	
	D3D11RenderStateTracker tracker(m_WrappedContext);

	HistogramCBufferData cdata;
	cdata.HistogramTextureResolution.x = (float)RDCMAX(details.texWidth>>mip, 1U);
	cdata.HistogramTextureResolution.y = (float)RDCMAX(details.texHeight>>mip, 1U);
	cdata.HistogramTextureResolution.z = (float)RDCMAX(details.texDepth>>mip, 1U);
	cdata.HistogramSlice = (float)sliceFace;
	cdata.HistogramMip = mip;
	cdata.HistogramSample = (int)RDCCLAMP(sample, 0U, details.sampleCount-1);
	if(sample == ~0U) cdata.HistogramSample = -int(details.sampleCount);
	cdata.HistogramMin = 0.0f;
	cdata.HistogramMax = 1.0f;
	cdata.HistogramChannels = 0xf;
	cdata.HistogramFlags = 0;
	
	int srvOffset = 0;
	int intIdx = 0;

	DXGI_FORMAT fmt = GetTypedFormat(details.texFmt);

	if(IsUIntFormat(fmt))
	{
		cdata.HistogramFlags |= TEXDISPLAY_UINT_TEX;
		srvOffset = 10;
		intIdx = 1;
	}
	if(IsIntFormat(fmt))
	{
		cdata.HistogramFlags |= TEXDISPLAY_SINT_TEX;
		srvOffset = 20;
		intIdx = 2;
	}
	
	if(details.texType == eTexType_3D)
		cdata.HistogramSlice = float(sliceFace)/float(details.texDepth);

	ID3D11Buffer *cbuf = MakeCBuffer((float *)&cdata, sizeof(cdata));

	m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);
	
	m_pImmediateContext->CSSetConstantBuffers(0, 1, &cbuf);
	
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { NULL };
	uavs[intIdx] = m_DebugRender.tileResultUAV[intIdx];
	m_pImmediateContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs, NULL);
	
	m_pImmediateContext->CSSetShaderResources(srvOffset, eTexType_Max, details.srv);

	ID3D11SamplerState *samps[] = { m_DebugRender.PointSampState, m_DebugRender.LinearSampState };
	m_pImmediateContext->CSSetSamplers(0, 2, samps);

	m_pImmediateContext->CSSetShader(m_DebugRender.TileMinMaxCS[details.texType][intIdx], NULL, 0);

	int blocksX = (int)ceil(cdata.HistogramTextureResolution.x/float(HGRAM_PIXELS_PER_TILE*HGRAM_PIXELS_PER_TILE));
	int blocksY = (int)ceil(cdata.HistogramTextureResolution.y/float(HGRAM_PIXELS_PER_TILE*HGRAM_PIXELS_PER_TILE));

	m_pImmediateContext->Dispatch(blocksX, blocksY, 1);

	m_pImmediateContext->CSSetUnorderedAccessViews(intIdx, 1, &m_DebugRender.resultUAV[intIdx], NULL);
	m_pImmediateContext->CSSetShaderResources(intIdx, 1, &m_DebugRender.tileResultSRV[intIdx]);

	m_pImmediateContext->CSSetShader(m_DebugRender.ResultMinMaxCS[intIdx], NULL, 0);

	m_pImmediateContext->Dispatch(1, 1, 1);

	m_pImmediateContext->CopyResource(m_DebugRender.resultStageBuff, m_DebugRender.resultBuff);

	D3D11_MAPPED_SUBRESOURCE mapped;

	HRESULT hr = m_pImmediateContext->Map(m_DebugRender.resultStageBuff, 0, D3D11_MAP_READ, 0, &mapped);

	if(FAILED(hr))
	{
		RDCERR("Failed to map minmax results buffer %08x", hr);
	}
	else
	{
		Vec4f *minmax = (Vec4f *)mapped.pData;

		minval[0] = minmax[0].x;
		minval[1] = minmax[0].y;
		minval[2] = minmax[0].z;
		minval[3] = minmax[0].w;

		maxval[0] = minmax[1].x;
		maxval[1] = minmax[1].y;
		maxval[2] = minmax[1].z;
		maxval[3] = minmax[1].w;

		m_pImmediateContext->Unmap(m_DebugRender.resultStageBuff, 0);
	}
	
	/*
	// debugging - copy out tile results
	const uint32_t maxTexDim = 16384;
	const uint32_t blockPixSize = HGRAM_TILES_PER_BLOCK*HGRAM_PIXELS_PER_TILE;
	const uint32_t maxBlocksNeeded = (maxTexDim*maxTexDim)/(blockPixSize*blockPixSize);

	D3D11_BUFFER_DESC bdesc;
	bdesc.BindFlags = 0;
	bdesc.Usage = D3D11_USAGE_STAGING;
	bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	bdesc.MiscFlags = 0;
	bdesc.StructureByteStride = 0;
	bdesc.ByteWidth = 2*4*sizeof(float)*HGRAM_TILES_PER_BLOCK*HGRAM_TILES_PER_BLOCK*maxBlocksNeeded;

	ID3D11Buffer *test = NULL;

	m_pDevice->CreateBuffer(&bdesc, NULL, &test);
	
	m_pImmediateContext->CopyResource(test, m_DebugRender.tileResultBuff);
	
	m_pImmediateContext->Map(test, 0, D3D11_MAP_READ, 0, &mapped);

	m_pImmediateContext->Unmap(test, 0);
	
	SAFE_RELEASE(test);
	*/

	return true;
}

vector<byte> D3D11DebugManager::GetBufferData(ResourceId buff, uint32_t offset, uint32_t len)
{
	auto it = WrappedID3D11Buffer::m_BufferList.find(buff);

	if(it == WrappedID3D11Buffer::m_BufferList.end())
		return vector<byte>();

	ID3D11Buffer *buffer = it->second.m_Buffer;

	RDCASSERT(buffer);

	return GetBufferData(buffer, offset, len);
}

vector<byte> D3D11DebugManager::GetBufferData(ID3D11Buffer *buffer, uint32_t offset, uint32_t len)
{
	D3D11_MAPPED_SUBRESOURCE mapped;

	if(buffer == NULL)
		return vector<byte>();

	D3D11_BUFFER_DESC desc;
	buffer->GetDesc(&desc);

	if(len == 0)
	{
		len = desc.ByteWidth-offset;
	}

	if(len > 0 && offset+len > desc.ByteWidth)
	{
		RDCWARN("Attempting to read off the end of the array. Will be clamped");
		len = RDCMIN(len, desc.ByteWidth-offset);
	}

	uint32_t outOffs = 0;

	vector<byte> ret;

	ret.resize(len);

	D3D11_BOX box;
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;

	ID3D11Buffer *src = UNWRAP(WrappedID3D11Buffer, buffer);

	while(len > 0)
	{
		uint32_t chunkSize = RDCMIN(len, STAGE_BUFFER_BYTE_SIZE);

		if(desc.StructureByteStride > 0)
			chunkSize -= (chunkSize % desc.StructureByteStride);

		box.left = RDCMIN(offset + outOffs, desc.ByteWidth);
		box.right = RDCMIN(offset + outOffs + chunkSize, desc.ByteWidth);

		if(box.right-box.left == 0)
			break;
				
		m_pImmediateContext->CopySubresourceRegion(m_DebugRender.StageBuffer, 0, 0, 0, 0, src, 0, &box);

		HRESULT hr = m_pImmediateContext->Map(m_DebugRender.StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);

		if(FAILED(hr))
		{
			RDCERR("Failed to map bufferdata buffer %08x", hr);
			return ret;
		}
		else
		{
			memcpy(&ret[outOffs], mapped.pData, RDCMIN(len, STAGE_BUFFER_BYTE_SIZE));

			m_pImmediateContext->Unmap(m_DebugRender.StageBuffer, 0);
		}

		outOffs += chunkSize;
		len -= chunkSize;
	}

	return ret;
}

void D3D11DebugManager::CopyArrayToTex2DMS(ID3D11Texture2D *destMS, ID3D11Texture2D *srcArray)
{
	D3D11RenderStateTracker tracker(m_WrappedContext);
	
	// copy to textures with right bind flags for operation
	D3D11_TEXTURE2D_DESC descArr;
	srcArray->GetDesc(&descArr);
	
	D3D11_TEXTURE2D_DESC descMS;
	destMS->GetDesc(&descMS);
	
	bool depth = IsDepthFormat(descMS.Format);

	ID3D11Texture2D *rtvResource = NULL;
	ID3D11Texture2D *srvResource = NULL;
	
	D3D11_TEXTURE2D_DESC rtvResDesc = descMS;
	D3D11_TEXTURE2D_DESC srvResDesc = descArr;

	rtvResDesc.BindFlags = depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
	srvResDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if(depth)
	{
		rtvResDesc.Format = GetTypelessFormat(rtvResDesc.Format);
		srvResDesc.Format = GetTypelessFormat(srvResDesc.Format);
	}

	rtvResDesc.Usage = D3D11_USAGE_DEFAULT;
	srvResDesc.Usage = D3D11_USAGE_DEFAULT;
	
	rtvResDesc.CPUAccessFlags = 0;
	srvResDesc.CPUAccessFlags = 0;

	HRESULT hr = S_OK;

	hr = m_pDevice->CreateTexture2D(&rtvResDesc, NULL, &rtvResource);
	if(FAILED(hr))
	{
		RDCERR("0x%08x", hr);
		return;
	}

	hr = m_pDevice->CreateTexture2D(&srvResDesc, NULL, &srvResource);
	if(FAILED(hr))
	{
		RDCERR("0x%08x", hr);
		return;
	}
	
	m_pImmediateContext->CopyResource(srvResource, srcArray);

	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { NULL };
	UINT uavCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = { (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1 };

	m_pImmediateContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs, uavCounts);
	
	m_pImmediateContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
	m_pImmediateContext->PSSetShader(depth ? m_DebugRender.DepthCopyArrayToMSPS : m_DebugRender.CopyArrayToMSPS, NULL, 0);

	m_pImmediateContext->HSSetShader(NULL, NULL, 0);
	m_pImmediateContext->DSSetShader(NULL, NULL, 0);
	m_pImmediateContext->GSSetShader(NULL, NULL, 0);

	D3D11_VIEWPORT view = { 0.0f, 0.0f, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f };

	m_pImmediateContext->RSSetState(m_DebugRender.RastState);
	m_pImmediateContext->RSSetViewports(1, &view);

	m_pImmediateContext->IASetInputLayout(NULL);
	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	float blendFactor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pImmediateContext->OMSetBlendState(NULL, blendFactor, ~0U);
	
	if(depth)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc;
		ID3D11DepthStencilState *dsState = NULL;
		RDCEraseEl(dsDesc);

		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsDesc.StencilEnable = FALSE;
		
		dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp = dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp = dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

		m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);
		m_pImmediateContext->OMSetDepthStencilState(dsState, 0);
		SAFE_RELEASE(dsState);
	}
	else
	{
		m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, 0);
	}

	ID3D11DepthStencilView *dsvMS = NULL;
	ID3D11RenderTargetView *rtvMS = NULL;
	ID3D11ShaderResourceView *srvArray = NULL;
	
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
	rtvDesc.Format = depth ? GetUIntTypedFormat(descMS.Format) : GetTypedFormatUIntPreferred(descMS.Format);
	rtvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
	rtvDesc.Texture2DMSArray.FirstArraySlice = 0;
	
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
	dsvDesc.Flags = 0;
	dsvDesc.Format = GetDepthTypedFormat(descMS.Format);
	dsvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
	dsvDesc.Texture2DMSArray.FirstArraySlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = depth ? GetUIntTypedFormat(descArr.Format) : GetTypedFormatUIntPreferred(descArr.Format);
	srvDesc.Texture2DArray.ArraySize = descArr.ArraySize;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.MipLevels = descArr.MipLevels;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	
	bool stencil = false;
	DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;
	
	if(depth)
	{
		switch(descArr.Format)
		{
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_TYPELESS:
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
				
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
				stencil = true;
				break;

			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
				stencil = true;
				break;

			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_TYPELESS:
				srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
				break;
		}
	}

	hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvArray);
	if(FAILED(hr))
	{
		RDCERR("0x%08x", hr);
		return;
	}
	
	ID3D11ShaderResourceView *srvs[8] = { NULL };
	srvs[0] = srvArray;

	m_pImmediateContext->PSSetShaderResources(0, D3D11_PS_CS_UAV_REGISTER_COUNT, srvs);
	
	// loop over every array slice in MS texture
	for(UINT slice=0; slice < descMS.ArraySize; slice++)
	{
		uint32_t cdata[4] = { descMS.SampleDesc.Count, 1000, 0, slice};

		ID3D11Buffer *cbuf = MakeCBuffer((float *)cdata, sizeof(cdata));

		m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

		rtvDesc.Texture2DMSArray.FirstArraySlice = slice;
		rtvDesc.Texture2DMSArray.ArraySize = 1;
		dsvDesc.Texture2DMSArray.FirstArraySlice = slice;
		dsvDesc.Texture2DMSArray.ArraySize = 1;

		if(depth)
			hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvMS);
		else
			hr = m_pDevice->CreateRenderTargetView(rtvResource, &rtvDesc, &rtvMS);
		if(FAILED(hr))
		{
			RDCERR("0x%08x", hr);
			return;
		}

		if(depth)
			m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvMS, 0, 0, NULL, NULL);
		else
			m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtvMS, NULL, 0, 0, NULL, NULL);

		m_pImmediateContext->Draw(3, 0);

		SAFE_RELEASE(rtvMS);
		SAFE_RELEASE(dsvMS);
	}

	SAFE_RELEASE(srvArray);
	
	if(stencil)
	{
		srvDesc.Format = stencilFormat;

		hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvArray);
		if(FAILED(hr))
		{
			RDCERR("0x%08x", hr);
			return;
		}

		m_pImmediateContext->PSSetShaderResources(1, 1, &srvArray);
		
		D3D11_DEPTH_STENCIL_DESC dsDesc;
		ID3D11DepthStencilState *dsState = NULL;
		RDCEraseEl(dsDesc);

		dsDesc.DepthEnable = FALSE;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.StencilEnable = TRUE;
		
		dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp = dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
		dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp = dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;
		
		dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
		dsvDesc.Texture2DArray.ArraySize = 1;

		m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);

		// loop over every array slice in MS texture
		for(UINT slice=0; slice < descMS.ArraySize; slice++)
		{
			dsvDesc.Texture2DMSArray.FirstArraySlice = slice;

			hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvMS);
			if(FAILED(hr))
			{
				RDCERR("0x%08x", hr);
				return;
			}

			m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvMS, 0, 0, NULL, NULL);

			// loop over every stencil value (zzzzzz, no shader stencil read/write)
			for(UINT stencil=0; stencil < 256; stencil++)
			{
				uint32_t cdata[4] = { descMS.SampleDesc.Count, stencil, 0, slice};

				ID3D11Buffer *cbuf = MakeCBuffer((float *)cdata, sizeof(cdata));

				m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

				m_pImmediateContext->OMSetDepthStencilState(dsState, stencil);

				m_pImmediateContext->Draw(3, 0);
			}

			SAFE_RELEASE(dsvMS);
		}

		SAFE_RELEASE(dsState);
	}

	m_pImmediateContext->CopyResource(destMS, rtvResource);

	SAFE_RELEASE(rtvResource);
	SAFE_RELEASE(srvResource);
}

void D3D11DebugManager::CopyTex2DMSToArray(ID3D11Texture2D *destArray, ID3D11Texture2D *srcMS)
{
	D3D11RenderStateTracker tracker(m_WrappedContext);
	
	// copy to textures with right bind flags for operation
	D3D11_TEXTURE2D_DESC descMS;
	srcMS->GetDesc(&descMS);
	
	D3D11_TEXTURE2D_DESC descArr;
	destArray->GetDesc(&descArr);

	ID3D11Texture2D *rtvResource = NULL;
	ID3D11Texture2D *srvResource = NULL;
	
	D3D11_TEXTURE2D_DESC rtvResDesc = descArr;
	D3D11_TEXTURE2D_DESC srvResDesc = descMS;

	bool depth = IsDepthFormat(descMS.Format);

	rtvResDesc.BindFlags = depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
	srvResDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if(depth)
	{
		rtvResDesc.Format = GetTypelessFormat(rtvResDesc.Format);
		srvResDesc.Format = GetTypelessFormat(srvResDesc.Format);
	}

	rtvResDesc.Usage = D3D11_USAGE_DEFAULT;
	srvResDesc.Usage = D3D11_USAGE_DEFAULT;
	
	rtvResDesc.CPUAccessFlags = 0;
	srvResDesc.CPUAccessFlags = 0;

	HRESULT hr = S_OK;

	hr = m_pDevice->CreateTexture2D(&rtvResDesc, NULL, &rtvResource);
	if(FAILED(hr))
	{
		RDCERR("0x%08x", hr);
		return;
	}

	hr = m_pDevice->CreateTexture2D(&srvResDesc, NULL, &srvResource);
	if(FAILED(hr))
	{
		RDCERR("0x%08x", hr);
		return;
	}
	
	m_pImmediateContext->CopyResource(srvResource, srcMS);
	
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { NULL };
	UINT uavCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = { (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1 };

	m_pImmediateContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs, uavCounts);
	
	m_pImmediateContext->VSSetShader(m_DebugRender.FullscreenVS, NULL, 0);
	m_pImmediateContext->PSSetShader(depth ? m_DebugRender.DepthCopyMSToArrayPS : m_DebugRender.CopyMSToArrayPS, NULL, 0);
	
	D3D11_VIEWPORT view = { 0.0f, 0.0f, (float)descArr.Width, (float)descArr.Height, 0.0f, 1.0f };

	m_pImmediateContext->RSSetState(m_DebugRender.RastState);
	m_pImmediateContext->RSSetViewports(1, &view);

	m_pImmediateContext->IASetInputLayout(NULL);
	float blendFactor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pImmediateContext->OMSetBlendState(NULL, blendFactor, ~0U);

	if(depth)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc;
		ID3D11DepthStencilState *dsState = NULL;
		RDCEraseEl(dsDesc);

		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsDesc.StencilEnable = FALSE;
		
		dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp = dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp = dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;

		m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);
		m_pImmediateContext->OMSetDepthStencilState(dsState, 0);
		SAFE_RELEASE(dsState);
	}
	else
	{
		m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.AllPassDepthState, 0);
	}

	ID3D11RenderTargetView *rtvArray = NULL;
	ID3D11DepthStencilView *dsvArray = NULL;
	ID3D11ShaderResourceView *srvMS = NULL;

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Format = depth ? GetUIntTypedFormat(descArr.Format) : GetTypedFormatUIntPreferred(descArr.Format);
	rtvDesc.Texture2DArray.FirstArraySlice = 0;
	rtvDesc.Texture2DArray.ArraySize = 1;
	rtvDesc.Texture2DArray.MipSlice = 0;
	
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	dsvDesc.Format = GetDepthTypedFormat(descArr.Format);
	dsvDesc.Flags = 0;
	dsvDesc.Texture2DArray.FirstArraySlice = 0;
	dsvDesc.Texture2DArray.ArraySize = 1;
	dsvDesc.Texture2DArray.MipSlice = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
	srvDesc.Format = depth ? GetUIntTypedFormat(descMS.Format) : GetTypedFormatUIntPreferred(descMS.Format);
	srvDesc.Texture2DMSArray.ArraySize = descMS.ArraySize;
	srvDesc.Texture2DMSArray.FirstArraySlice = 0;

	bool stencil = false;
	DXGI_FORMAT stencilFormat = DXGI_FORMAT_UNKNOWN;

	if(depth)
	{
		switch(descMS.Format)
		{
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_TYPELESS:
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
				
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				stencilFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
				stencil = true;
				break;

			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
				srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				stencilFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
				stencil = true;
				break;

			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_TYPELESS:
				srvDesc.Format = DXGI_FORMAT_R16_FLOAT;
				break;
		}
	}

	hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvMS);
	if(FAILED(hr))
	{
		RDCERR("0x%08x", hr);
		return;
	}
	
	ID3D11ShaderResourceView *srvs[8] = { NULL };

	int srvIndex = 0;

	for(int i=0; i < 8; i++)
		if(descMS.SampleDesc.Count == UINT(1<<i))
			srvIndex = i;

	srvs[srvIndex] = srvMS;
	
	m_pImmediateContext->PSSetShaderResources(0, D3D11_PS_CS_UAV_REGISTER_COUNT, srvs);

	// loop over every array slice in MS texture
	for(UINT slice=0; slice < descMS.ArraySize; slice++)
	{
		// loop over every multi sample
		for(UINT sample=0; sample < descMS.SampleDesc.Count; sample++)
		{
			uint32_t cdata[4] = { descMS.SampleDesc.Count, 1000, sample, slice};
			
			ID3D11Buffer *cbuf = MakeCBuffer((float *)cdata, sizeof(cdata));

			m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

			rtvDesc.Texture2DArray.FirstArraySlice = slice*descMS.SampleDesc.Count + sample;
			dsvDesc.Texture2DArray.FirstArraySlice = slice*descMS.SampleDesc.Count + sample;

			if(depth)
				hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvArray);
			else
				hr = m_pDevice->CreateRenderTargetView(rtvResource, &rtvDesc, &rtvArray);

			if(FAILED(hr))
			{
				RDCERR("0x%08x", hr);
				return;
			}
			
			if(depth)
				m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvArray, 0, 0, NULL, NULL);
			else
				m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtvArray, NULL, 0, 0, NULL, NULL);

			m_pImmediateContext->Draw(3, 0);

			SAFE_RELEASE(rtvArray);
			SAFE_RELEASE(dsvArray);
		}
	}

	SAFE_RELEASE(srvMS);

	if(stencil)
	{
		srvDesc.Format = stencilFormat;

		hr = m_pDevice->CreateShaderResourceView(srvResource, &srvDesc, &srvMS);
		if(FAILED(hr))
		{
			RDCERR("0x%08x", hr);
			return;
		}

		m_pImmediateContext->PSSetShaderResources(10+srvIndex, 1, &srvMS);
		
		D3D11_DEPTH_STENCIL_DESC dsDesc;
		ID3D11DepthStencilState *dsState = NULL;
		RDCEraseEl(dsDesc);

		dsDesc.DepthEnable = FALSE;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.StencilEnable = TRUE;
		
		dsDesc.BackFace.StencilFailOp = dsDesc.BackFace.StencilPassOp = dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
		dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.FrontFace.StencilFailOp = dsDesc.FrontFace.StencilPassOp = dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilReadMask = dsDesc.StencilWriteMask = 0xff;
		
		dsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
		dsvDesc.Texture2DArray.ArraySize = 1;

		m_pDevice->CreateDepthStencilState(&dsDesc, &dsState);

		// loop over every array slice in MS texture
		for(UINT slice=0; slice < descMS.ArraySize; slice++)
		{
			// loop over every multi sample
			for(UINT sample=0; sample < descMS.SampleDesc.Count; sample++)
			{
				dsvDesc.Texture2DArray.FirstArraySlice = slice*descMS.SampleDesc.Count + sample;

				hr = m_pDevice->CreateDepthStencilView(rtvResource, &dsvDesc, &dsvArray);
				if(FAILED(hr))
				{
					RDCERR("0x%08x", hr);
					return;
				}

				m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dsvArray, 0, 0, NULL, NULL);

				// loop over every stencil value (zzzzzz, no shader stencil read/write)
				for(UINT stencil=0; stencil < 256; stencil++)
				{
					uint32_t cdata[4] = { descMS.SampleDesc.Count, stencil, sample, slice};
					
					ID3D11Buffer *cbuf = MakeCBuffer((float *)cdata, sizeof(cdata));

					m_pImmediateContext->PSSetConstantBuffers(0, 1, &cbuf);

					m_pImmediateContext->OMSetDepthStencilState(dsState, stencil);

					m_pImmediateContext->Draw(3, 0);
				}

				SAFE_RELEASE(dsvArray);
			}
		}

		SAFE_RELEASE(dsState);
	}

	m_pImmediateContext->CopyResource(destArray, rtvResource);

	SAFE_RELEASE(rtvResource);
	SAFE_RELEASE(srvResource);
}

D3D11DebugManager::CacheElem &D3D11DebugManager::GetCachedElem(ResourceId id, bool raw)
{
	for(auto it=m_ShaderItemCache.begin(); it != m_ShaderItemCache.end(); ++it)
	{
		if(it->id == id && it->raw == raw)
			return *it;
	}

	if(m_ShaderItemCache.size() >= NUM_CACHED_SRVS)
	{
		CacheElem &elem = m_ShaderItemCache.back();
		elem.Release();
		m_ShaderItemCache.pop_back();
	}
	
	m_ShaderItemCache.push_front(CacheElem(id, raw));
	return m_ShaderItemCache.front();
}

D3D11DebugManager::TextureShaderDetails D3D11DebugManager::GetShaderDetails(ResourceId id, bool rawOutput)
{
	TextureShaderDetails details;
	HRESULT hr = S_OK;

	bool foundResource = false;
	
	CacheElem &cache = GetCachedElem(id, rawOutput);

	bool msaaDepth = false;

	bool cube = false;
	DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

	if(WrappedID3D11Texture1D::m_TextureList.find(id) != WrappedID3D11Texture1D::m_TextureList.end())
	{
		WrappedID3D11Texture1D *wrapTex1D = (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[id].m_Texture;
		TextureDisplayType mode = WrappedID3D11Texture1D::m_TextureList[id].m_Type;

		foundResource = true;

		details.texType = eTexType_1D;

		if(mode == TEXDISPLAY_DEPTH_TARGET)
			details.texType = eTexType_Depth;

		D3D11_TEXTURE1D_DESC desc1d = {0};
		wrapTex1D->GetDesc(&desc1d);

		details.texFmt = desc1d.Format;
		details.texWidth = desc1d.Width;
		details.texHeight = 1;
		details.texDepth = 1;
		details.texArraySize = desc1d.ArraySize;
		details.texMips = desc1d.MipLevels;

		srvFormat = GetTypedFormat(details.texFmt);

		details.srvResource = wrapTex1D->GetReal();

		if(mode == TEXDISPLAY_INDIRECT_VIEW ||
			mode == TEXDISPLAY_DEPTH_TARGET)
		{
			D3D11_TEXTURE1D_DESC desc = desc1d;
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

			if(mode == TEXDISPLAY_DEPTH_TARGET)
				desc.Format = GetTypelessFormat(desc.Format);

			if(!cache.created)
			{
				ID3D11Texture1D *tmp = NULL;
				hr = m_pDevice->CreateTexture1D(&desc, NULL, &tmp);

				if(FAILED(hr))
				{
					RDCERR("Failed to create temporary Texture1D %08x", hr);
				}

				cache.srvResource = tmp;
			}

			details.previewCopy = cache.srvResource;

			m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

			details.srvResource = details.previewCopy;
		}
	}
	else if(WrappedID3D11Texture2D::m_TextureList.find(id) != WrappedID3D11Texture2D::m_TextureList.end())
	{
		WrappedID3D11Texture2D *wrapTex2D = (WrappedID3D11Texture2D *)WrappedID3D11Texture2D::m_TextureList[id].m_Texture;
		TextureDisplayType mode = WrappedID3D11Texture2D::m_TextureList[id].m_Type;

		foundResource = true;

		details.texType = eTexType_2D;
		
		D3D11_TEXTURE2D_DESC desc2d = {0};
		wrapTex2D->GetDesc(&desc2d);

		if(desc2d.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
			cube = true;

		details.texFmt = desc2d.Format;
		details.texWidth = desc2d.Width;
		details.texHeight = desc2d.Height;
		details.texDepth = 1;
		details.texArraySize = desc2d.ArraySize;
		details.texMips = desc2d.MipLevels;
		details.sampleCount = RDCMAX(1U, desc2d.SampleDesc.Count);
		details.sampleQuality = desc2d.SampleDesc.Quality;

		if(desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0)
		{
			details.texType = eTexType_2DMS;
		}
		
		if(mode == TEXDISPLAY_DEPTH_TARGET || IsDepthFormat(details.texFmt))
		{
			details.texType = eTexType_Depth;
			details.texFmt = GetTypedFormat(details.texFmt);
		}

		// backbuffer is always interpreted as SRGB data regardless of format specified:
		// http://msdn.microsoft.com/en-us/library/windows/desktop/hh972627(v=vs.85).aspx
		//
		// "The app must always place sRGB data into back buffers with integer-valued formats
		// to present the sRGB data to the screen, even if the data doesn't have this format
		// modifier in its format name."
		//
		// This essentially corrects for us always declaring an SRGB render target for our
		// output displays, as any app with a non-SRGB backbuffer would be incorrectly converted
		// unless we read out SRGB here.
		//
		// However when picking a pixel we want the actual value stored, not the corrected perceptual
		// value so for raw output we don't do this. This does my head in, it really does.
		if(wrapTex2D->m_RealDescriptor)
		{
			if(rawOutput)
				details.texFmt = wrapTex2D->m_RealDescriptor->Format;
			else
				details.texFmt = GetSRGBFormat(wrapTex2D->m_RealDescriptor->Format);
		}

		srvFormat = GetTypedFormat(details.texFmt);

		details.srvResource = wrapTex2D->GetReal();

		if(mode == TEXDISPLAY_INDIRECT_VIEW ||
			mode == TEXDISPLAY_DEPTH_TARGET ||
			desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0)
		{
			D3D11_TEXTURE2D_DESC desc = desc2d;
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

			if(mode == TEXDISPLAY_DEPTH_TARGET)
			{
				desc.Format = GetTypelessFormat(desc.Format);
			}
			else
			{
				desc.Format = srvFormat;
			}

			if(!cache.created)
			{
				ID3D11Texture2D *tmp = NULL;
				hr = m_pDevice->CreateTexture2D(&desc, NULL, &tmp);

				if(FAILED(hr))
				{
					RDCERR("Failed to create temporary Texture2D %08x", hr);
				}

				cache.srvResource = tmp;
			}

			details.previewCopy = cache.srvResource;

			if((desc2d.SampleDesc.Count > 1 || desc2d.SampleDesc.Quality > 0) && mode == TEXDISPLAY_DEPTH_TARGET)
					msaaDepth = true;

			m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

			details.srvResource = details.previewCopy;
		}
	}
	else if(WrappedID3D11Texture3D::m_TextureList.find(id) != WrappedID3D11Texture3D::m_TextureList.end())
	{
		WrappedID3D11Texture3D *wrapTex3D = (WrappedID3D11Texture3D *)WrappedID3D11Texture3D::m_TextureList[id].m_Texture;
		TextureDisplayType mode = WrappedID3D11Texture3D::m_TextureList[id].m_Type;

		foundResource = true;

		details.texType = eTexType_3D;

		D3D11_TEXTURE3D_DESC desc3d = {0};
		wrapTex3D->GetDesc(&desc3d);

		details.texFmt = desc3d.Format;
		details.texWidth = desc3d.Width;
		details.texHeight = desc3d.Height;
		details.texDepth = desc3d.Depth;
		details.texArraySize = 1;
		details.texMips = desc3d.MipLevels;

		srvFormat = GetTypedFormat(details.texFmt);

		details.srvResource = wrapTex3D->GetReal();

		if(mode == TEXDISPLAY_INDIRECT_VIEW)
		{
			D3D11_TEXTURE3D_DESC desc = desc3d;
			desc.CPUAccessFlags = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

			if(IsUIntFormat(srvFormat) || IsIntFormat(srvFormat))
				desc.Format = GetTypelessFormat(desc.Format);

			if(!cache.created)
			{
				ID3D11Texture3D *tmp = NULL;
				hr = m_pDevice->CreateTexture3D(&desc, NULL, &tmp);

				if(FAILED(hr))
				{
					RDCERR("Failed to create temporary Texture3D %08x", hr);
				}

				cache.srvResource = tmp;
			}

			details.previewCopy = cache.srvResource;

			m_pImmediateContext->CopyResource(details.previewCopy, details.srvResource);

			details.srvResource = details.previewCopy;
		}
	}

	if(!foundResource)
	{
		RDCERR("bad texture trying to be displayed");
		return TextureShaderDetails();
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc[eTexType_Max];

	srvDesc[eTexType_1D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
	srvDesc[eTexType_1D].Texture1DArray.ArraySize = details.texArraySize;
	srvDesc[eTexType_1D].Texture1DArray.FirstArraySlice = 0;
	srvDesc[eTexType_1D].Texture1DArray.MipLevels = details.texMips;
	srvDesc[eTexType_1D].Texture1DArray.MostDetailedMip = 0;

	srvDesc[eTexType_2D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc[eTexType_2D].Texture2DArray.ArraySize = details.texArraySize;
	srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice = 0;
	srvDesc[eTexType_2D].Texture2DArray.MipLevels = details.texMips;
	srvDesc[eTexType_2D].Texture2DArray.MostDetailedMip = 0;
	
	srvDesc[eTexType_2DMS].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
	srvDesc[eTexType_2DMS].Texture2DMSArray.ArraySize = details.texArraySize;
	srvDesc[eTexType_2DMS].Texture2DMSArray.FirstArraySlice = 0;

	srvDesc[eTexType_Stencil] = srvDesc[eTexType_Depth] = srvDesc[eTexType_2D];

	srvDesc[eTexType_3D].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srvDesc[eTexType_3D].Texture3D.MipLevels = details.texMips;
	srvDesc[eTexType_3D].Texture3D.MostDetailedMip = 0;
	
	srvDesc[eTexType_Cube].ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	srvDesc[eTexType_Cube].TextureCubeArray.First2DArrayFace = 0;
	srvDesc[eTexType_Cube].TextureCubeArray.MipLevels = details.texMips;
	srvDesc[eTexType_Cube].TextureCubeArray.MostDetailedMip = 0;
	srvDesc[eTexType_Cube].TextureCubeArray.NumCubes = RDCMAX(1U, details.texArraySize/6);
	
	for(int i=0; i < eTexType_Max; i++)
		srvDesc[i].Format = srvFormat;

	if(details.texType == eTexType_Depth)
	{
		switch(details.texFmt)
		{
			case DXGI_FORMAT_R32G8X24_TYPELESS:
			case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
			case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			{
				srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
				break;
			}
			case DXGI_FORMAT_R32_FLOAT:
			case DXGI_FORMAT_R32_TYPELESS:
			case DXGI_FORMAT_D32_FLOAT:
			{
				srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R32_FLOAT;
				srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_UNKNOWN;
				break;
			}
			case DXGI_FORMAT_R24G8_TYPELESS:
			case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			case DXGI_FORMAT_D24_UNORM_S8_UINT:
			{
				srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
				break;
			}
			case DXGI_FORMAT_R16_FLOAT:
			case DXGI_FORMAT_R16_TYPELESS:
			case DXGI_FORMAT_D16_UNORM:
			case DXGI_FORMAT_R16_UINT:
			{
				srvDesc[eTexType_Depth].Format = DXGI_FORMAT_R16_UNORM;
				srvDesc[eTexType_Stencil].Format = DXGI_FORMAT_UNKNOWN;
				break;
			}
			default:
				break;
		}
	}

	if(msaaDepth)
	{
		srvDesc[eTexType_Stencil].ViewDimension = srvDesc[eTexType_Depth].ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
		
		srvDesc[eTexType_Depth].Texture2DMSArray.ArraySize = srvDesc[eTexType_2D].Texture2DArray.ArraySize;
		srvDesc[eTexType_Stencil].Texture2DMSArray.ArraySize = srvDesc[eTexType_2D].Texture2DArray.ArraySize;
		srvDesc[eTexType_Depth].Texture2DMSArray.FirstArraySlice = srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice;
		srvDesc[eTexType_Stencil].Texture2DMSArray.FirstArraySlice = srvDesc[eTexType_2D].Texture2DArray.FirstArraySlice;
	}

	if(!cache.created)
	{
		hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[details.texType], &cache.srv[0]);

		if(FAILED(hr))
			RDCERR("Failed to create cache SRV 0, type %d %08x", details.texType, hr);
	}

	details.srv[details.texType] = cache.srv[0];

	if(details.texType == eTexType_Depth && srvDesc[eTexType_Stencil].Format != DXGI_FORMAT_UNKNOWN)
	{
		if(!cache.created)
		{
			hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[eTexType_Stencil], &cache.srv[1]);

			if(FAILED(hr))
				RDCERR("Failed to create cache SRV 1, type %d %08x", details.texType, hr);
		}

		details.srv[eTexType_Stencil] = cache.srv[1];

		details.texType = eTexType_Stencil;
	}

	if(msaaDepth)
	{
		if(details.texType == eTexType_Depth)
			details.texType = eTexType_DepthMS;
		if(details.texType == eTexType_Stencil)
			details.texType = eTexType_StencilMS;

		details.srv[eTexType_Depth] = NULL;
		details.srv[eTexType_Stencil] = NULL;
		details.srv[eTexType_DepthMS] = cache.srv[0];
		details.srv[eTexType_StencilMS] = cache.srv[1];
	}
	
	if((details.texType == eTexType_2D ||
		details.texType == eTexType_Depth ||
		details.texType == eTexType_Stencil)
		&& cube)
	{
		if(!cache.created)
		{
			hr = m_pDevice->CreateShaderResourceView(details.srvResource, &srvDesc[eTexType_Cube], &cache.srv[2]);

			if(FAILED(hr))
				RDCERR("Failed to create cache SRV 2 %08x", hr);
		}

		details.srv[eTexType_Cube] = cache.srv[2];
	}

	cache.created = true;

	return details;
}

void D3D11DebugManager::RenderText(float x, float y, float size, const char *textfmt, ...)
{
	static char tmpBuf[4096];

	va_list args;
	va_start(args, textfmt);
	StringFormat::vsnprintf( tmpBuf, 4095, textfmt, args );
	tmpBuf[4095] = '\0';
	va_end(args);

	// normalize size to 720 (and scale respectively)
	// invert y co-ordinates for convenience
	RenderTextInternal(x, -y, size*(720.0f/float(GetHeight())), tmpBuf);
}

void D3D11DebugManager::RenderTextInternal(float x, float y, float size, const char *text)
{
	if(char *t = strchr((char *)text, '\n'))
	{
		*t = 0;
		RenderTextInternal(x, y, size, text);
		RenderTextInternal(x, y-18.0f, size, t+1);
		*t = '\n';
		return;
	}

	if(strlen(text) == 0)
		return;

	RDCASSERT(strlen(text) < FONT_MAX_CHARS);

	FontCBuffer data;

	data.TextPosition.x = x*(2.0f/float(GetWidth()));
	data.TextPosition.y = y*(2.0f/float(GetHeight()));

	data.FontScreenAspect.x = (float(GetHeight())/float(GetWidth()))*0.5f; // 0.5 = character width / character height
	data.FontScreenAspect.y = 1.0f;

	data.CharacterSize.x = 0.5f*(float(FONT_TEX_HEIGHT)/float(FONT_TEX_WIDTH));
	data.CharacterSize.y = 1.0f;

	data.TextSize = size*0.05f; // default arbitrary font size

	data.CharacterOffsetX = 0.75f*(float(FONT_TEX_HEIGHT)/float(FONT_TEX_WIDTH));

	D3D11_MAPPED_SUBRESOURCE mapped;

	FillCBuffer(m_Font.CBuffer, (float *)&data, sizeof(FontCBuffer));

	HRESULT hr = m_pImmediateContext->Map(m_Font.CharBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if(FAILED(hr))
	{
		RDCERR("Failed to map charbuffer %08x", hr);
		return;
	}

	unsigned long *texs = (unsigned long *)mapped.pData;
	
	for(size_t i=0; i < strlen(text); i++)
	{
		texs[i*4 + 0] = text[i] - ' ';
		texs[i*4 + 1] = text[i] - ' ';
		texs[i*4 + 2] = text[i] - ' ';
		texs[i*4 + 3] = text[i] - ' ';
	}
	m_pImmediateContext->Unmap(m_Font.CharBuffer, 0);

	ID3D11Buffer *bufs[2] = { m_DebugRender.PosBuffer, m_Font.CharBuffer };
	UINT strides[2] = { 3*sizeof(float), sizeof(long) };
	UINT offsets[2] = { 0, 0 };

	// can't just clear state because we need to keep things like render targets.
	{
		m_pImmediateContext->IASetInputLayout(m_Font.Layout);
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_pImmediateContext->IASetVertexBuffers(0, 2, bufs, strides, offsets);

		m_pImmediateContext->VSSetShader(m_Font.VS, NULL, 0);
		m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_Font.CBuffer);

		m_pImmediateContext->HSSetShader(NULL, NULL, 0);
		m_pImmediateContext->DSSetShader(NULL, NULL, 0);
		m_pImmediateContext->GSSetShader(NULL, NULL, 0);

		m_pImmediateContext->RSSetState(m_DebugRender.RastState);

		D3D11_VIEWPORT view;
		view.TopLeftX = 0;
		view.TopLeftY = 0;
		view.Width = (float)GetWidth();
		view.Height = (float)GetHeight();
		view.MinDepth = 0.0f;
		view.MaxDepth = 1.0f;
		m_pImmediateContext->RSSetViewports(1, &view);

		m_pImmediateContext->PSSetShader(m_Font.PS, NULL, 0);
		m_pImmediateContext->PSSetShaderResources(0, 1, &m_Font.Tex);

		ID3D11SamplerState *samps[] = { m_DebugRender.PointSampState, m_DebugRender.LinearSampState };
		m_pImmediateContext->PSSetSamplers(0, 2, samps);

		float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		m_pImmediateContext->OMSetBlendState(m_DebugRender.BlendState, factor, 0xffffffff);

		m_pImmediateContext->Draw((uint32_t)strlen(text)*4, 0);
	}
}

bool D3D11DebugManager::RenderTexture(TextureDisplay cfg, bool blendAlpha)
{
	DebugVertexCBuffer vertexData;
	DebugPixelCBufferData pixelData;

	float x = cfg.offx;
	float y = cfg.offy;
	
	vertexData.Position.x = x*(2.0f/float(GetWidth()));
	vertexData.Position.y = -y*(2.0f/float(GetHeight()));

	vertexData.ScreenAspect.x = (float(GetHeight())/float(GetWidth())); // 0.5 = character width / character height
	vertexData.ScreenAspect.y = 1.0f;

	vertexData.TextureResolution.x = 1.0f/vertexData.ScreenAspect.x;
	vertexData.TextureResolution.y = 1.0f;

	if(cfg.rangemax <= cfg.rangemin) cfg.rangemax += 0.00001f;

	pixelData.Channels.x = cfg.Red ? 1.0f : 0.0f;
	pixelData.Channels.y = cfg.Green ? 1.0f : 0.0f;
	pixelData.Channels.z = cfg.Blue ? 1.0f : 0.0f;
	pixelData.Channels.w = cfg.Alpha ? 1.0f : 0.0f;

	pixelData.RangeMinimum = cfg.rangemin;
	pixelData.InverseRangeSize = 1.0f/(cfg.rangemax-cfg.rangemin);

	pixelData.WireframeColour.x = cfg.HDRMul;

	pixelData.RawOutput = cfg.rawoutput ? 1 : 0;

	pixelData.FlipY = cfg.FlipY ? 1 : 0;
	
	TextureShaderDetails details = GetShaderDetails(cfg.texid, cfg.rawoutput ? true : false);

	static int sampIdx = 0;
	
	pixelData.SampleIdx = (int)RDCCLAMP(cfg.sampleIdx, 0U, details.sampleCount-1);

	// hacky resolve
	if(cfg.sampleIdx == ~0U)
		pixelData.SampleIdx = -int(details.sampleCount);
	
	if(details.texFmt == DXGI_FORMAT_UNKNOWN)
		return false;
	
	D3D11RenderStateTracker tracker(m_WrappedContext);

	if(details.texFmt == DXGI_FORMAT_A8_UNORM && cfg.scale <= 0.0f)
	{
		pixelData.Channels.x = pixelData.Channels.y = pixelData.Channels.z = 0.0f;
		pixelData.Channels.w = 1.0f;
	}

	float tex_x = float(details.texWidth);
	float tex_y = float(details.texType == eTexType_1D ? 100 : details.texHeight);

	vertexData.TextureResolution.x *= tex_x/float(GetWidth());
	vertexData.TextureResolution.y *= tex_y/float(GetHeight());
	
	pixelData.TextureResolutionPS.x = float(RDCMAX(1U,details.texWidth>>cfg.mip));
	pixelData.TextureResolutionPS.y = float(RDCMAX(1U,details.texHeight>>cfg.mip));
	pixelData.TextureResolutionPS.z = float(RDCMAX(1U,details.texDepth>>cfg.mip));

	if(details.texArraySize > 1 && details.texType != eTexType_3D)
		pixelData.TextureResolutionPS.z = float(details.texArraySize);

	vertexData.Scale = cfg.scale;
	pixelData.ScalePS = cfg.scale;

	if(cfg.scale <= 0.0f)
	{
		float xscale = float(GetWidth())/tex_x;
		float yscale = float(GetHeight())/tex_y;

		vertexData.Scale = RDCMIN(xscale, yscale);

		if(yscale > xscale)
		{
			vertexData.Position.x = 0;
			vertexData.Position.y = tex_y*vertexData.Scale/float(GetHeight()) - 1.0f;
		}
		else
		{
			vertexData.Position.y = 0;
			vertexData.Position.x = 1.0f - tex_x*vertexData.Scale/float(GetWidth());
		}
	}

	ID3D11PixelShader *customPS = NULL;
	ID3D11Buffer *customBuff = NULL;
	
	if(cfg.CustomShader != ResourceId())
	{
		auto it = WrappedShader::m_ShaderList.find(cfg.CustomShader);

		if(it != WrappedShader::m_ShaderList.end())
		{
			auto dxbc = it->second.m_DXBCFile;

			RDCASSERT(dxbc);
			RDCASSERT(dxbc->m_Type == D3D11_SHVER_PIXEL_SHADER);

			WrappedID3D11Shader<ID3D11PixelShader> *wrapped = (WrappedID3D11Shader<ID3D11PixelShader> *)m_WrappedDevice->GetResourceManager()->GetLiveResource(cfg.CustomShader);

			customPS = wrapped->GetReal();

			for(size_t i=0; i < dxbc->m_CBuffers.size(); i++)
			{
				const DXBC::CBuffer &cbuf = dxbc->m_CBuffers[i];
				if(cbuf.name == "$Globals")
				{
					float *cbufData = new float[cbuf.descriptor.byteSize/sizeof(float) + 1];
					byte *byteData = (byte *)cbufData;

					for(size_t v=0; v < cbuf.variables.size(); v++)
					{
						const DXBC::CBufferVariable &var = cbuf.variables[v];

						if(var.name == "RENDERDOC_TexDim")
						{
							if(var.type.descriptor.rows == 1 &&
								var.type.descriptor.cols == 4 &&
								var.type.descriptor.type == DXBC::VARTYPE_UINT)
							{
								uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

								d[0] = details.texWidth;
								d[1] = details.texHeight;
								d[2] = details.texType == D3D11DebugManager::eTexType_3D ? details.texDepth : details.texArraySize;
								d[3] = details.texMips;
							}
							else
							{
								RDCWARN("Custom shader: Variable recognised but type wrong, expected uint4: %hs", var.name.c_str());
							}
						}
						else if(var.name == "RENDERDOC_SelectedMip")
						{
							if(var.type.descriptor.rows == 1 &&
								var.type.descriptor.cols == 1 &&
								var.type.descriptor.type == DXBC::VARTYPE_UINT)
							{
								uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

								d[0] = cfg.mip;
							}
							else
							{
								RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %hs", var.name.c_str());
							}
						}
						else if(var.name == "RENDERDOC_TextureType")
						{
							if(var.type.descriptor.rows == 1 &&
								var.type.descriptor.cols == 1 &&
								var.type.descriptor.type == DXBC::VARTYPE_UINT)
							{
								uint32_t *d = (uint32_t *)(byteData + var.descriptor.offset);

								d[0] = details.texType;
							}
							else
							{
								RDCWARN("Custom shader: Variable recognised but type wrong, expected uint: %hs", var.name.c_str());
							}
						}
						else
						{
							RDCWARN("Custom shader: Variable not recognised: %hs", var.name.c_str());
						}
					}

					customBuff = MakeCBuffer(cbufData, cbuf.descriptor.byteSize);
				}
			}
		}
	}

	vertexData.Scale *= 2.0f; // viewport is -1 -> 1

	pixelData.MipLevel = (float)cfg.mip;

	ID3D11Buffer *bufs[2] = { m_DebugRender.PosBuffer, m_Font.CharBuffer };
	UINT stride = 3*sizeof(float);
	UINT offset = 0;
	
	pixelData.OutputDisplayFormat = RESTYPE_TEX2D;
	pixelData.Slice = float(RDCCLAMP(cfg.sliceFace, 0U, details.texArraySize-1));

	if(details.texType == eTexType_3D)
	{
		pixelData.OutputDisplayFormat = RESTYPE_TEX3D;
		pixelData.Slice = float(cfg.sliceFace)/float(details.texDepth);
	}
	else if(details.texType == eTexType_1D)
	{
		pixelData.OutputDisplayFormat = RESTYPE_TEX1D;
	}
	else if(details.texType == eTexType_Depth)
	{
		pixelData.OutputDisplayFormat = RESTYPE_DEPTH;
	}
	else if(details.texType == eTexType_Stencil)
	{
		pixelData.OutputDisplayFormat = RESTYPE_DEPTH_STENCIL;
	}
	else if(details.texType == eTexType_DepthMS)
	{
		pixelData.OutputDisplayFormat = RESTYPE_DEPTH_MS;
	}
	else if(details.texType == eTexType_StencilMS)
	{
		pixelData.OutputDisplayFormat = RESTYPE_DEPTH_STENCIL_MS;
	}
	else if(details.texType == eTexType_2DMS)
	{
		pixelData.OutputDisplayFormat = RESTYPE_TEX2D_MS;
	}
	
	if(cfg.overlay == eTexOverlay_NaN)
	{
		pixelData.OutputDisplayFormat |= TEXDISPLAY_NANS;
	}

	if(cfg.overlay == eTexOverlay_Clipping)
	{
		pixelData.OutputDisplayFormat |= TEXDISPLAY_CLIPPING;
	}
	
	int srvOffset = 0;

	if(IsUIntFormat(details.texFmt))
	{
		pixelData.OutputDisplayFormat |= TEXDISPLAY_UINT_TEX;
		srvOffset = 10;
	}
	if(IsIntFormat(details.texFmt))
	{
		pixelData.OutputDisplayFormat |= TEXDISPLAY_SINT_TEX;
		srvOffset = 20;
	}
	if(!IsSRGBFormat(details.texFmt) && cfg.linearDisplayAsGamma)
	{
		pixelData.OutputDisplayFormat |= TEXDISPLAY_GAMMA_CURVE;
	}

	FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));
	FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

	// can't just clear state because we need to keep things like render targets.
	{
		m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_pImmediateContext->IASetVertexBuffers(0, 1, &m_DebugRender.PosBuffer, &stride, &offset);

		m_pImmediateContext->VSSetShader(m_DebugRender.GenericVS, NULL, 0);
		m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);

		m_pImmediateContext->HSSetShader(NULL, NULL, 0);
		m_pImmediateContext->DSSetShader(NULL, NULL, 0);
		m_pImmediateContext->GSSetShader(NULL, NULL, 0);
		
		m_pImmediateContext->RSSetState(m_DebugRender.RastState);

		if(customPS == NULL)
		{
			m_pImmediateContext->PSSetShader(m_DebugRender.TexDisplayPS, NULL, 0);
			m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
		}
		else
		{
			m_pImmediateContext->PSSetShader(customPS, NULL, 0);
			m_pImmediateContext->PSSetConstantBuffers(0, 1, &customBuff);
		}
		
		ID3D11UnorderedAccessView *NullUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { 0 };
		UINT UAV_keepcounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = { (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1 };

		m_pImmediateContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, NullUAVs, UAV_keepcounts);

		m_pImmediateContext->PSSetShaderResources(srvOffset, eTexType_Max, details.srv);

		ID3D11SamplerState *samps[] = { m_DebugRender.PointSampState, m_DebugRender.LinearSampState };
		m_pImmediateContext->PSSetSamplers(0, 2, samps);

		float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		if(cfg.rawoutput || !blendAlpha)
			m_pImmediateContext->OMSetBlendState(NULL, factor, 0xffffffff);
		else
			m_pImmediateContext->OMSetBlendState(m_DebugRender.BlendState, factor, 0xffffffff);

		m_pImmediateContext->Draw(4, 0);
	}

	return true;
}

void D3D11DebugManager::RenderHighlightBox(float w, float h, float scale)
{
	UINT stride = 3*sizeof(float);
	UINT offs = 0;
	
	D3D11RenderStateTracker tracker(m_WrappedContext);

	float overlayConsts[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	ID3D11Buffer *vconst = NULL;
	ID3D11Buffer *pconst = NULL;

	pconst = MakeCBuffer(overlayConsts, sizeof(overlayConsts));
	
	const float xpixdim = 2.0f/w;
	const float ypixdim = 2.0f/h;

	const float xdim = scale*xpixdim;
	const float ydim = scale*ypixdim;

	DebugVertexCBuffer vertCBuffer;
	RDCEraseEl(vertCBuffer);
	vertCBuffer.Scale = 1.0f;
	vertCBuffer.ScreenAspect.x = vertCBuffer.ScreenAspect.y = 1.0f;

	vertCBuffer.Position.x = 1.0f;
	vertCBuffer.Position.y = -1.0f;
	vertCBuffer.TextureResolution.x = xdim;
	vertCBuffer.TextureResolution.y = ydim;

	vconst = MakeCBuffer((float *)&vertCBuffer, sizeof(vertCBuffer));

	m_pImmediateContext->HSSetShader(NULL, NULL, 0);
	m_pImmediateContext->DSSetShader(NULL, NULL, 0);
	m_pImmediateContext->GSSetShader(NULL, NULL, 0);

	m_pImmediateContext->RSSetState(m_DebugRender.RastState);

	m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);
	m_pImmediateContext->IASetVertexBuffers(0, 1, &m_DebugRender.OutlineStripVB, &stride, &offs);

	m_pImmediateContext->VSSetShader(m_DebugRender.GenericVS, NULL, 0);
	m_pImmediateContext->PSSetShader(m_DebugRender.OverlayPS, NULL, 0);
	m_pImmediateContext->OMSetBlendState(NULL, NULL, 0xffffffff);
	
	m_pImmediateContext->PSSetConstantBuffers(1, 1, &pconst);
	m_pImmediateContext->VSSetConstantBuffers(0, 1, &vconst);

	m_pImmediateContext->Draw(5, 0);

	vertCBuffer.Position.x = 1.0f-xpixdim;
	vertCBuffer.Position.y = -1.0f+ypixdim;
	vertCBuffer.TextureResolution.x = xdim+xpixdim*2;
	vertCBuffer.TextureResolution.y = ydim+ypixdim*2;
	
	overlayConsts[0] = overlayConsts[1] = overlayConsts[2] = 0.0f;

	vconst = MakeCBuffer((float *)&vertCBuffer, sizeof(vertCBuffer));
	pconst = MakeCBuffer(overlayConsts, sizeof(overlayConsts));
	
	m_pImmediateContext->VSSetConstantBuffers(0, 1, &vconst);
	m_pImmediateContext->PSSetConstantBuffers(1, 1, &pconst);
	m_pImmediateContext->Draw(5, 0);
}

void D3D11DebugManager::RenderCheckerboard(Vec3f light, Vec3f dark)
{
	DebugVertexCBuffer vertexData;
	
	D3D11RenderStateTracker tracker(m_WrappedContext);

	vertexData.Scale = 2.0f;
	vertexData.Position.x = vertexData.Position.y = 0;
	
	vertexData.ScreenAspect.x = 1.0f;
	vertexData.ScreenAspect.y = 1.0f;

	vertexData.TextureResolution.x = 1.0f;
	vertexData.TextureResolution.y = 1.0f;
	
	FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));

	DebugPixelCBufferData pixelData;

	pixelData.Channels = Vec4f(light.x, light.y, light.z, 0.0f);
	pixelData.WireframeColour = dark;
	
	FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

	UINT stride = 3*sizeof(float);
	UINT offset = 0;

	// can't just clear state because we need to keep things like render targets.
	{
		m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_pImmediateContext->IASetVertexBuffers(0, 1, &m_DebugRender.PosBuffer, &stride, &offset);

		m_pImmediateContext->VSSetShader(m_DebugRender.GenericVS, NULL, 0);
		m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);

		m_pImmediateContext->HSSetShader(NULL, NULL, 0);
		m_pImmediateContext->DSSetShader(NULL, NULL, 0);
		m_pImmediateContext->GSSetShader(NULL, NULL, 0);

		m_pImmediateContext->RSSetState(m_DebugRender.RastState);

		m_pImmediateContext->PSSetShader(m_DebugRender.CheckerboardPS, NULL, 0);
		m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

		float factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		m_pImmediateContext->OMSetBlendState(NULL, factor, 0xffffffff);

		m_pImmediateContext->Draw(4, 0);
	}
}

PostVSData D3D11DebugManager::GetPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	auto idx = std::make_pair(frameID, eventID);
	if(m_PostVSData.find(idx) != m_PostVSData.end())
		return m_PostVSData[idx];

	RDCWARN("Post VS Buffers not initialised!");
	PostVSData empty;
	RDCEraseEl(empty);
	return empty;
}

PostVSMeshData D3D11DebugManager::GetPostVSBuffers(uint32_t frameID, uint32_t eventID, MeshDataStage stage)
{
	PostVSMeshData ret;

	PostVSData postvs = GetPostVSBuffers(frameID, eventID);
	PostVSData::StageData s = postvs.GetStage(stage);
	ret.numVerts = s.numVerts;
	ret.topo = MakePrimitiveTopology(s.topo);
	if(s.buf != NULL)
		ret.buf = GetBufferData(s.buf, 0, 0);
	else
		RDCWARN("No buffer for this stage!");

	return ret;
}

void D3D11DebugManager::InitPostVSBuffers(uint32_t frameID, uint32_t eventID)
{
	auto idx = std::make_pair(frameID, eventID);
	if(m_PostVSData.find(idx) != m_PostVSData.end())
		return;

	D3D11RenderStateTracker tracker(m_WrappedContext);

	ID3D11VertexShader *vs = NULL;
	m_pImmediateContext->VSGetShader(&vs, NULL, NULL);

	ID3D11GeometryShader *gs = NULL;
	m_pImmediateContext->GSGetShader(&gs, NULL, NULL);
	
	ID3D11HullShader *hs = NULL;
	m_pImmediateContext->HSGetShader(&hs, NULL, NULL);

	ID3D11DomainShader *ds = NULL;
	m_pImmediateContext->DSGetShader(&ds, NULL, NULL);
		
	if(vs) vs->Release();
	if(gs) gs->Release();
	if(hs) hs->Release();
	if(ds) ds->Release();

	if(!vs)
		return;

	D3D11_PRIMITIVE_TOPOLOGY topo;
	m_pImmediateContext->IAGetPrimitiveTopology(&topo);
	
	WrappedID3D11Shader<ID3D11VertexShader> *wrappedVS = (WrappedID3D11Shader<ID3D11VertexShader> *)m_WrappedDevice->GetResourceManager()->GetWrapper(vs);
	
	if(!wrappedVS)
	{
		RDCERR("Couldn't find wrapped vertex shader!");
		return;
	}

	DXBC::DXBCFile *dxbcVS = wrappedVS->GetDXBC();

	RDCASSERT(dxbcVS);
	
	DXBC::DXBCFile *dxbcGS = NULL;
	
	if(gs)
	{
		WrappedID3D11Shader<ID3D11GeometryShader> *wrappedGS = (WrappedID3D11Shader<ID3D11GeometryShader> *)m_WrappedDevice->GetResourceManager()->GetWrapper(gs);

		if(!wrappedGS)
		{
			RDCERR("Couldn't find wrapped geometry shader!");
			return;
		}

		dxbcGS = wrappedGS->GetDXBC();

		RDCASSERT(dxbcGS);
	}
	
	DXBC::DXBCFile *dxbcDS = NULL;
	
	if(ds)
	{
		WrappedID3D11Shader<ID3D11DomainShader> *wrappedDS = (WrappedID3D11Shader<ID3D11DomainShader> *)m_WrappedDevice->GetResourceManager()->GetWrapper(ds);

		if(!wrappedDS)
		{
			RDCERR("Couldn't find wrapped domain shader!");
			return;
		}

		dxbcDS = wrappedDS->GetDXBC();

		RDCASSERT(dxbcDS);
	}

	vector<D3D11_SO_DECLARATION_ENTRY> sodecls;

	UINT stride = 0;
	UINT posoffset = ~0U;
	int numPosComponents = 0;

	ID3D11GeometryShader *streamoutGS = NULL;

	if(!dxbcVS->m_OutputSig.empty())
	{
		for(size_t i=0; i < dxbcVS->m_OutputSig.size(); i++)
		{
			SigParameter &sign = dxbcVS->m_OutputSig[i];

			D3D11_SO_DECLARATION_ENTRY decl;

			decl.Stream = 0;
			decl.OutputSlot = 0;

			decl.SemanticName = sign.semanticName.elems;
			decl.SemanticIndex = sign.semanticIndex;
			decl.StartComponent = 0;
			decl.ComponentCount = sign.compCount&0xff;

			string a = strupper(string(decl.SemanticName));

			if(a.find("POSITION") != string::npos)
			{
				// force to 4 components, as we need it, and store its offset
				if(a.find("SV_POSITION") != string::npos || sign.systemValue == eAttr_Position || posoffset == ~0U)
				{
					if(a.find("SV_POSITION") != string::npos || sign.systemValue == eAttr_Position)
						decl.ComponentCount = 4;

					posoffset = stride;
					numPosComponents = decl.ComponentCount;
				}
			}

			stride += decl.ComponentCount * sizeof(float);
			sodecls.push_back(decl);
		}

		HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
			(void *)&dxbcVS->m_ShaderBlob[0],
			dxbcVS->m_ShaderBlob.size(),
			&sodecls[0],
			(UINT)sodecls.size(),
			&stride,
			1,
			D3D11_SO_NO_RASTERIZED_STREAM,
			NULL,
			&streamoutGS);

		if(FAILED(hr))
		{
			RDCERR("Failed to create Geometry Shader + SO %08x", hr);
			return;
		}

		m_pImmediateContext->GSSetShader(streamoutGS, NULL, 0);
		m_pImmediateContext->HSSetShader(NULL, NULL, 0);
		m_pImmediateContext->DSSetShader(NULL, NULL, 0);

		SAFE_RELEASE(streamoutGS);

		UINT offset = 0;
		m_pImmediateContext->SOSetTargets( 1, &m_SOBuffer, &offset );

		m_pImmediateContext->Begin(m_SOStatsQuery);

		const FetchDrawcall *drawcall = m_WrappedDevice->GetDrawcall(frameID, eventID);
		
		ID3D11Buffer *idxBuf = NULL;
		DXGI_FORMAT idxFmt = DXGI_FORMAT_UNKNOWN;

		if((drawcall->flags & eDraw_UseIBuffer) == 0)
		{
			m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			m_pImmediateContext->Draw(drawcall->numIndices, drawcall->vertexOffset);
			m_pImmediateContext->IASetPrimitiveTopology(topo);
		}
		else // drawcall is indexed
		{
			UINT idxOffs = 0;
			
			m_WrappedContext->IAGetIndexBuffer(&idxBuf, &idxFmt, &idxOffs);
			bool index16 = (idxFmt == DXGI_FORMAT_R16_UINT); 
			UINT bytesize = index16 ? 2 : 4; 

			ID3D11Buffer *origBuf = idxBuf;

			vector<byte> idxdata = GetBufferData(idxBuf, idxOffs + drawcall->indexOffset*bytesize, drawcall->numIndices*bytesize);

			SAFE_RELEASE(idxBuf);
			
			vector<uint32_t> indices;
			
			uint16_t *idx16 = (uint16_t *)&idxdata[0];
			uint32_t *idx32 = (uint32_t *)&idxdata[0];

			// grab all unique vertex indices referenced
			for(uint32_t i=0; i < drawcall->numIndices; i++)
			{
				uint32_t i32 = index16 ? uint32_t(idx16[i]) : idx32[i];

				auto it = std::lower_bound(indices.begin(), indices.end(), i32);

				if(it != indices.end() && *it == i32)
					continue;

				indices.insert(it, i32);
			}

			// An index buffer could be something like: 500, 501, 502, 501, 503, 502
			// in which case we can't use the existing index buffer without filling 499 slots of vertex
			// data with padding. Instead we rebase the indices based on the smallest vertex so it becomes
			// 0, 1, 2, 1, 3, 2 and then that matches our stream-out'd buffer.
			//
			// Since we want the indices to be preserved in order to easily match up inputs to outputs,
			// but shifted, fill in gaps in our streamout vertex buffer with the lowest index value.
			// (use the lowest index value so that even the gaps are a 'valid' vertex, rather than
			// potentially garbage data).
			uint32_t minindex = indices[0];

			// indices[] contains ascending unique vertex indices referenced. Fill gaps with minindex
			for(size_t i=1; i < indices.size(); i++)
			{
				if(indices[i]-1 > indices[i-1])
				{
					size_t gapsize = size_t( (indices[i]-1) - indices[i-1] );

					indices.insert(indices.begin()+i, gapsize, minindex);

					i += gapsize;
				}
			}

			D3D11_BUFFER_DESC desc = { UINT(sizeof(uint32_t)*indices.size()), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
			D3D11_SUBRESOURCE_DATA initData = { &indices[0], desc.ByteWidth, desc.ByteWidth };

			m_pDevice->CreateBuffer(&desc, &initData, &idxBuf);

			m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			m_pImmediateContext->IASetIndexBuffer(idxBuf, DXGI_FORMAT_R32_UINT, 0);
			SAFE_RELEASE(idxBuf);

			m_pImmediateContext->DrawIndexed((UINT)indices.size(), 0, drawcall->vertexOffset);

			m_pImmediateContext->IASetPrimitiveTopology(topo);
			m_pImmediateContext->IASetIndexBuffer(UNWRAP(WrappedID3D11Buffer, origBuf), idxFmt, idxOffs);
			
			// rebase existing index buffer to point from 0 onwards (which will index into our
			// stream-out'd vertex buffer)
			if(index16)
			{
				for(uint32_t i=0; i < drawcall->numIndices; i++)
					idx16[i] -= uint16_t(minindex&0xffff);
			}
			else
			{
				for(uint32_t i=0; i < drawcall->numIndices; i++)
					idx32[i] -= minindex;
			}
			
			desc.ByteWidth = (UINT)idxdata.size();
			initData.pSysMem = &idxdata[0];
			initData.SysMemPitch = initData.SysMemSlicePitch = desc.ByteWidth;

			m_WrappedDevice->CreateBuffer(&desc, &initData, &idxBuf);
		}

		m_pImmediateContext->End(m_SOStatsQuery);

		m_pImmediateContext->GSSetShader(NULL, NULL, 0);
		m_pImmediateContext->SOSetTargets(0, NULL, NULL);

		D3D11_QUERY_DATA_SO_STATISTICS numPrims;

		m_pImmediateContext->CopyResource(m_SOStagingBuffer, m_SOBuffer);

		do 
		{
			hr = m_pImmediateContext->GetData(m_SOStatsQuery, &numPrims, sizeof(D3D11_QUERY_DATA_SO_STATISTICS ), 0);
		} while(hr == S_FALSE);

		if(numPrims.NumPrimitivesWritten == 0)
		{
			m_PostVSData[idx] = PostVSData();
			SAFE_RELEASE(idxBuf);
			return;
		}

		D3D11_MAPPED_SUBRESOURCE mapped;
		hr = m_pImmediateContext->Map(m_SOStagingBuffer, 0, D3D11_MAP_READ, 0, &mapped);

		if(FAILED(hr))
		{
			RDCERR("Failed to map sobuffer %08x", hr);
			SAFE_RELEASE(idxBuf);
			return;
		}

		D3D11_BUFFER_DESC bufferDesc =
		{
			stride * (uint32_t)numPrims.NumPrimitivesWritten,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_VERTEX_BUFFER,
			0,
			0,
			0
		};

		if(bufferDesc.ByteWidth >= m_SOBufferSize)
		{
			RDCERR("Generated output data too large: %08x", bufferDesc.ByteWidth);

			m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
			SAFE_RELEASE(idxBuf);
			return;
		}

		ID3D11Buffer *vsoutBuffer = NULL;

		// we need to map this data into memory for read anyway, might as well make this VB
		// immutable while we're at it.
		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = mapped.pData;
		initialData.SysMemPitch = bufferDesc.ByteWidth;
		initialData.SysMemSlicePitch = bufferDesc.ByteWidth;

		hr = m_WrappedDevice->CreateBuffer( &bufferDesc, &initialData, &vsoutBuffer );

		if(FAILED(hr))
		{
			RDCERR("Failed to create postvs pos buffer %08x", hr);

			m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
			SAFE_RELEASE(idxBuf);
			return;
		}

		byte *byteData = (byte *)mapped.pData;

		float nearp = 0.0f;
		float farp = 0.0f;

		Vec4f *pos0 = (Vec4f *)(byteData + posoffset);

		for(UINT64 i=1; numPosComponents == 4 && i < numPrims.NumPrimitivesWritten; i++)
		{
			//////////////////////////////////////////////////////////////////////////////////
			// derive near/far, assuming a standard perspective matrix
			//
			// the transformation from from pre-projection {Z,W} to post-projection {Z,W}
			// is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
			// and we know Wpost = Zpre from the perspective matrix.
			// we can then see from the perspective matrix that
			// m = F/(F-N)
			// c = -(F*N)/(F-N)
			//
			// with re-arranging and substitution, we then get:
			// N = -c/m
			// F = c/(1-m)
			//
			// so if we can derive m and c then we can determine N and F. We can do this with
			// two points, and we pick them reasonably distinct on z to reduce floating-point
			// error

			Vec4f *pos = (Vec4f *)(byteData + posoffset + i*stride);

			if(fabs(pos->w - pos0->w) > 0.01f)
			{
				Vec2f A(pos0->w, pos0->z);
				Vec2f B(pos->w, pos->z);

				float m = (B.y-A.y)/(B.x-A.x);
				float c = B.y - B.x*m;

				if(m == 1.0f) continue;

				nearp = -c/m;
				farp = c/(1-m);

				break;
			}
		}

		m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);

		m_PostVSData[idx].vsin.topo = topo;
		m_PostVSData[idx].vsout.buf = vsoutBuffer;
		m_PostVSData[idx].vsout.posOffset = posoffset;
		m_PostVSData[idx].vsout.vertStride = stride;
		m_PostVSData[idx].vsout.nearPlane = nearp;
		m_PostVSData[idx].vsout.farPlane = farp;

		m_PostVSData[idx].vsout.useIndices = (drawcall->flags & eDraw_UseIBuffer) > 0;
		m_PostVSData[idx].vsout.numVerts = drawcall->numIndices;

		m_PostVSData[idx].vsout.idxBuf = NULL;
		if(m_PostVSData[idx].vsout.useIndices && idxBuf)
		{
			m_PostVSData[idx].vsout.idxBuf = idxBuf;
			m_PostVSData[idx].vsout.idxFmt = idxFmt;
		}

		m_PostVSData[idx].vsout.topo = topo;
	}
	else
	{
		// empty vertex output signature
		m_PostVSData[idx].vsin.topo = topo;
		m_PostVSData[idx].vsout.buf = NULL;
		m_PostVSData[idx].vsout.posOffset = ~0U;
		m_PostVSData[idx].vsout.vertStride = 0;
		m_PostVSData[idx].vsout.nearPlane = 0.0f;
		m_PostVSData[idx].vsout.farPlane = 0.0f;
		m_PostVSData[idx].vsout.useIndices = false;
		m_PostVSData[idx].vsout.idxBuf = NULL;

		m_PostVSData[idx].vsout.topo = topo;
	}

	if(dxbcGS || dxbcDS)
	{
		stride = 0;
		posoffset = ~0U;
		numPosComponents = 0;

		DXBC::DXBCFile *lastShader = dxbcGS;
		if(dxbcDS) lastShader = dxbcDS;

		sodecls.clear();
		for(size_t i=0; i < lastShader->m_OutputSig.size(); i++)
		{
			SigParameter &sign = lastShader->m_OutputSig[i];

			D3D11_SO_DECLARATION_ENTRY decl;

			// for now, skip streams that aren't stream 0
			if(sign.stream != 0)
				continue;

			decl.Stream = 0;
			decl.OutputSlot = 0;

			decl.SemanticName = sign.semanticName.elems;
			decl.SemanticIndex = sign.semanticIndex;
			decl.StartComponent = 0;
			decl.ComponentCount = sign.compCount&0xff;

			string a = strupper(string(decl.SemanticName));
			
			if(a.find("POSITION") != string::npos)
			{
				// force to 4 components, as we need it, and store its offset
				if(a.find("SV_POSITION") != string::npos || sign.systemValue == eAttr_Position || posoffset == ~0U)
				{
					if(a.find("SV_POSITION") != string::npos || sign.systemValue == eAttr_Position)
						decl.ComponentCount = 4;

					posoffset = stride;
					numPosComponents = decl.ComponentCount;
				}
			}

			stride += decl.ComponentCount * sizeof(float);
			sodecls.push_back(decl);
		}
		
		streamoutGS = NULL;

		HRESULT hr = m_pDevice->CreateGeometryShaderWithStreamOutput(
			(void *)&lastShader->m_ShaderBlob[0],
			lastShader->m_ShaderBlob.size(),
			&sodecls[0],
			(UINT)sodecls.size(),
			&stride,
			1,
			D3D11_SO_NO_RASTERIZED_STREAM,
			NULL,
			&streamoutGS);

		if(FAILED(hr))
		{
			RDCERR("Failed to create Geometry Shader + SO %08x", hr);
			return;
		}

		m_pImmediateContext->GSSetShader(streamoutGS, NULL, 0);
		m_pImmediateContext->HSSetShader(hs, NULL, 0);
		m_pImmediateContext->DSSetShader(ds, NULL, 0);

		SAFE_RELEASE(streamoutGS);

		UINT offset = 0;
		m_pImmediateContext->SOSetTargets( 1, &m_SOBuffer, &offset );

		m_pImmediateContext->Begin(m_SOStatsQuery);
		m_WrappedDevice->ReplayLog(frameID, 0, eventID, eReplay_OnlyDraw);
		m_pImmediateContext->End(m_SOStatsQuery);

		m_pImmediateContext->GSSetShader(NULL, NULL, 0);
		m_pImmediateContext->SOSetTargets(0, NULL, NULL);

		D3D11_QUERY_DATA_SO_STATISTICS numPrims;

		m_pImmediateContext->CopyResource(m_SOStagingBuffer, m_SOBuffer);

		do 
		{
			hr = m_pImmediateContext->GetData(m_SOStatsQuery, &numPrims, sizeof(D3D11_QUERY_DATA_SO_STATISTICS ), 0);
		} while(hr == S_FALSE);

		if(numPrims.NumPrimitivesWritten == 0)
		{
			return;
		}
		
		D3D11_MAPPED_SUBRESOURCE mapped;
		hr = m_pImmediateContext->Map(m_SOStagingBuffer, 0, D3D11_MAP_READ, 0, &mapped);

		if(FAILED(hr))
		{
			RDCERR("Failed to map sobuffer %08x", hr);
			return;
		}

		D3D11_BUFFER_DESC bufferDesc =
		{
			stride * (uint32_t)numPrims.NumPrimitivesWritten*3,
			D3D11_USAGE_IMMUTABLE,
			D3D11_BIND_VERTEX_BUFFER,
			0,
			0,
			0
		};

		if(bufferDesc.ByteWidth >= m_SOBufferSize)
		{
			RDCERR("Generated output data too large: %08x", bufferDesc.ByteWidth);

			m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
			return;
		}

		ID3D11Buffer *gsoutBuffer = NULL;

		// we need to map this data into memory for read anyway, might as well make this VB
		// immutable while we're at it.
		D3D11_SUBRESOURCE_DATA initialData;
		initialData.pSysMem = mapped.pData;
		initialData.SysMemPitch = bufferDesc.ByteWidth;
		initialData.SysMemSlicePitch = bufferDesc.ByteWidth;

		hr = m_WrappedDevice->CreateBuffer( &bufferDesc, &initialData, &gsoutBuffer );

		if(FAILED(hr))
		{
			RDCERR("Failed to create postvs pos buffer %08x", hr);

			m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
			return;
		}

		byte *byteData = (byte *)mapped.pData;

		float nearp = 0.0f;
		float farp = 0.0f;

		Vec4f *pos0 = (Vec4f *)(byteData + posoffset);

		for(UINT64 i=1; numPosComponents == 4 && i < numPrims.NumPrimitivesWritten; i++)
		{
			//////////////////////////////////////////////////////////////////////////////////
			// derive near/far, assuming a standard perspective matrix
			//
			// the transformation from from pre-projection {Z,W} to post-projection {Z,W}
			// is linear. So we can say Zpost = Zpre*m + c . Here we assume Wpre = 1
			// and we know Wpost = Zpre from the perspective matrix.
			// we can then see from the perspective matrix that
			// m = F/(F-N)
			// c = -(F*N)/(F-N)
			//
			// with re-arranging and substitution, we then get:
			// N = -c/m
			// F = c/(1-m)
			//
			// so if we can derive m and c then we can determine N and F. We can do this with
			// two points, and we pick them reasonably distinct on z to reduce floating-point
			// error

			Vec4f *pos = (Vec4f *)(byteData + posoffset + i*stride);

			if(fabs(pos->w - pos0->w) > 0.01f)
			{
				Vec2f A(pos0->w, pos0->z);
				Vec2f B(pos->w, pos->z);

				float m = (B.y-A.y)/(B.x-A.x);
				float c = B.y - B.x*m;

				if(m == 1.0f) continue;

				nearp = -c/m;
				farp = c/(1-m);

				break;
			}
		}

		m_pImmediateContext->Unmap(m_SOStagingBuffer, 0);
		
		m_PostVSData[idx].gsout.buf = gsoutBuffer;
		m_PostVSData[idx].gsout.posOffset = posoffset;
		m_PostVSData[idx].gsout.vertStride = stride;
		m_PostVSData[idx].gsout.nearPlane = nearp;
		m_PostVSData[idx].gsout.farPlane = farp;
		m_PostVSData[idx].gsout.useIndices = false;
		m_PostVSData[idx].gsout.idxBuf = NULL;

		D3D11_PRIMITIVE_TOPOLOGY topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		
		if(lastShader == dxbcGS)
		{
			for(size_t i=0; i < dxbcGS->m_Declarations.size(); i++)
			{
				if(dxbcGS->m_Declarations[i].declaration == DXBC::OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY)
				{
					topo = (D3D11_PRIMITIVE_TOPOLOGY)dxbcGS->m_Declarations[i].outTopology; // enums match
					break;
				}
			}
		}
		else if(lastShader == dxbcDS)
		{
			for(size_t i=0; i < dxbcDS->m_Declarations.size(); i++)
			{
				if(dxbcDS->m_Declarations[i].declaration == DXBC::OPCODE_DCL_TESS_DOMAIN)
				{
					if(dxbcDS->m_Declarations[i].domain == DXBC::DOMAIN_ISOLINE)
						topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
					else
						topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					break;
				}
			}
		}

		m_PostVSData[idx].gsout.topo = topo;

		// streamout expands strips unfortunately
		if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
			m_PostVSData[idx].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP)
			m_PostVSData[idx].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		else if(topo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
			m_PostVSData[idx].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
		else if(topo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
			m_PostVSData[idx].gsout.topo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;

		switch(m_PostVSData[idx].gsout.topo)
		{
			case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
				m_PostVSData[idx].gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten; break;
			case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
			case D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
				m_PostVSData[idx].gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten*2; break;
			default:
			case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
				m_PostVSData[idx].gsout.numVerts = (uint32_t)numPrims.NumPrimitivesWritten*3; break;
		}
	}
}

FloatVector D3D11DebugManager::InterpretVertex(byte *data, uint32_t vert, MeshDisplay cfg, byte *end, bool &valid)
{
	FloatVector ret(0.0f, 0.0f, 0.0f, 1.0f);

	if(m_HighlightCache.useidx)
	{
		if(vert >= (uint32_t)m_HighlightCache.indices.size())
		{
			valid = false;
			return ret;
		}

		vert = m_HighlightCache.indices[vert];
	}

	data += vert*cfg.positionStride;

	float *out = &ret.x;

	ResourceFormat fmt;
	fmt.compByteWidth = cfg.positionCompByteWidth;
	fmt.compCount = cfg.positionCompCount;
	fmt.compType = cfg.positionCompType;

	if(cfg.positionFormat == eSpecial_R10G10B10A2)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		Vec4f v = ConvertFromR10G10B10A2(*(uint32_t *)data);
		ret.x = v.x;
		ret.y = v.y;
		ret.z = v.z;
		ret.w = v.w;
		return ret;
	}
	else if(cfg.positionFormat == eSpecial_R11G11B10)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		Vec3f v = ConvertFromR11G11B10(*(uint32_t *)data);
		ret.x = v.x;
		ret.y = v.y;
		ret.z = v.z;
		return ret;
	}
	else if(cfg.positionFormat == eSpecial_B8G8R8A8)
	{
		if(data+4 >= end)
		{
			valid = false;
			return ret;
		}

		fmt.compByteWidth = 1;
		fmt.compCount = 4;
		fmt.compType = eCompType_UNorm;
	}
	
	if(data + cfg.positionCompCount*cfg.positionCompByteWidth >= end)
	{
		valid = false;
		return ret;
	}

	for(uint32_t i=0; i < cfg.positionCompCount; i++)
	{
		*out = ConvertComponent(fmt, data);

		data += cfg.positionCompByteWidth;
		out++;
	}

	if(cfg.positionFormat == eSpecial_B8G8R8A8)
	{
		FloatVector reversed;
		reversed.x = ret.x;
		reversed.y = ret.y;
		reversed.z = ret.z;
		reversed.w = ret.w;
		return reversed;
	}

	return ret;
}

void D3D11DebugManager::RenderMesh(uint32_t frameID, const vector<uint32_t> &events, MeshDisplay cfg)
{
	DebugVertexCBuffer vertexData;

	D3D11PipelineState pipeState = m_WrappedDevice->GetReplay()->GetD3D11PipelineState();
	D3D11RenderState *curRS = m_WrappedDevice->GetImmediateContext()->GetCurrentPipelineState();

	float aspect = 1.0f;

	// guess the output aspect ratio, for mesh calculation
	if(pipeState.m_OM.DepthTarget.Resource != ResourceId())
	{
		FetchTexture desc = m_WrappedDevice->GetReplay()->GetTexture(m_ResourceManager->GetLiveID(pipeState.m_OM.DepthTarget.Resource));

		aspect = float(desc.width)/float(desc.height);
	}
	else
	{
		for(int32_t i=0; i < pipeState.m_OM.RenderTargets.count; i++)
		{
			if(pipeState.m_OM.RenderTargets[i].Resource != ResourceId())
			{
				FetchTexture desc = m_WrappedDevice->GetReplay()->GetTexture(m_ResourceManager->GetLiveID(pipeState.m_OM.RenderTargets[i].Resource));

				aspect = float(desc.width)/float(desc.height);
				break;
			}
		}
	}

	Matrix4f projMat = Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(GetWidth())/float(GetHeight()));

	Camera cam;
	if(cfg.arcballCamera)
		cam.Arcball(cfg.cameraPos.x, Vec3f(cfg.cameraRot.x, cfg.cameraRot.y, cfg.cameraRot.z));
	else
		cam.fpsLook(Vec3f(cfg.cameraPos.x, cfg.cameraPos.y, cfg.cameraPos.z), Vec3f(cfg.cameraRot.x, cfg.cameraRot.y, cfg.cameraRot.z));

	Matrix4f camMat = cam.GetMatrix();
	Matrix4f guessProjInv;

	vertexData.ModelViewProj = projMat.Mul(camMat);
	vertexData.SpriteSize = Vec2f();

	DebugPixelCBufferData pixelData;

	pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;
	pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 0.0f);
	FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

	m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
	m_pImmediateContext->PSSetShader(m_DebugRender.WireframePS, NULL, 0);

	m_pImmediateContext->HSSetShader(NULL, NULL, 0);
	m_pImmediateContext->DSSetShader(NULL, NULL, 0);
	m_pImmediateContext->GSSetShader(NULL, NULL, 0);

	m_pImmediateContext->OMSetDepthStencilState(NULL, 0);
	m_pImmediateContext->OMSetBlendState(m_WireframeHelpersBS, NULL, 0xffffffff);
	
	// don't cull in wireframe mesh display
	/*
	if(pipeState.m_RS.m_State.CullMode != eCull_None && pipeState.m_RS.m_State.FrontCCW)
		m_pImmediateContext->RSSetState(m_WireframeHelpersCullCWRS);
	else if(pipeState.m_RS.m_State.CullMode != eCull_None && !pipeState.m_RS.m_State.FrontCCW)
		m_pImmediateContext->RSSetState(m_WireframeHelpersCullCCWRS);
	else
		*/
		m_pImmediateContext->RSSetState(m_WireframeHelpersRS);
	
	if((cfg.type != eMeshDataStage_VSIn && pipeState.m_HS.Shader == ResourceId()) ||
		(cfg.type == eMeshDataStage_GSOut && pipeState.m_HS.Shader != ResourceId()))
	{
		float nearp = 0.1f;
		float farp = 1000.0f;

		for(size_t i=0; i < events.size(); i++)
		{
			PostVSData data = GetPostVSBuffers(frameID, events[i]);
			const PostVSData::StageData &stage = data.GetStage(cfg.type);

			if(stage.buf && stage.nearPlane != stage.farPlane)
			{
				nearp = stage.nearPlane;
				farp = stage.farPlane;
				break;
			}
		}

		// the derivation of the projection matrix might not be right (hell, it could be an
		// orthographic projection). But it'll be close enough likely.
		Matrix4f guessProj = Matrix4f::Perspective(cfg.fov,
												  cfg.nearPlane > -FLT_MAX ? cfg.nearPlane : nearp,
												  cfg.farPlane > -FLT_MAX ? cfg.farPlane : farp,
												  cfg.aspect > 0.0f ? cfg.aspect : aspect);

		if(cfg.ortho)
		{
			guessProj = Matrix4f::Orthographic(cfg.nearPlane > -FLT_MAX ? cfg.nearPlane : nearp,
											   cfg.farPlane > -FLT_MAX ? cfg.farPlane : farp);
		}
		
		guessProjInv = guessProj.Inverse();

		vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));

		FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));
		m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);
		m_pImmediateContext->VSSetShader(m_DebugRender.WireframeHomogVS, NULL, 0);

		m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
		m_pImmediateContext->PSSetShader(m_DebugRender.WireframePS, NULL, 0);

		{
			m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericHomogLayout);

			if(events.size() > 1)
			{
				pixelData.WireframeColour = Vec3f(cfg.prevMeshColour.x, cfg.prevMeshColour.y, cfg.prevMeshColour.z);
				FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));
			}

			for(size_t i=0; i < events.size()-1; i++)
			{
				PostVSData data = GetPostVSBuffers(frameID, events[i]);
				PostVSData::StageData stage = data.GetStage(cfg.type);

				if(stage.buf == NULL && cfg.type == eMeshDataStage_GSOut)
					stage = data.GetStage(eMeshDataStage_VSOut);

				if(stage.buf)
				{
					if(stage.topo > D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
						m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
					else
						m_pImmediateContext->IASetPrimitiveTopology(stage.topo);

					ID3D11Buffer *buf = UNWRAP(WrappedID3D11Buffer, stage.buf);
					m_pImmediateContext->IASetVertexBuffers(0, 1, &buf, (UINT *)&stage.vertStride, (UINT *)&stage.posOffset);
					if(stage.useIndices)
					{
						buf = UNWRAP(WrappedID3D11Buffer, stage.idxBuf);
						m_pImmediateContext->IASetIndexBuffer(buf, stage.idxFmt, 0);

						m_pImmediateContext->DrawIndexed(stage.numVerts, 0, 0);
					}
					else
					{
						m_pImmediateContext->Draw(stage.numVerts, 0);
					}
				}
			}

			pixelData.WireframeColour = Vec3f(cfg.currentMeshColour.x, cfg.currentMeshColour.y, cfg.currentMeshColour.z);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			PostVSData data = GetPostVSBuffers(frameID, events.back());
			const PostVSData::StageData &stage = data.GetStage(cfg.type);
			if(stage.buf)
			{
				if(stage.topo > D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
					m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
				else
					m_pImmediateContext->IASetPrimitiveTopology(stage.topo);

				ID3D11Buffer *buf = UNWRAP(WrappedID3D11Buffer, stage.buf);
				m_pImmediateContext->IASetVertexBuffers(0, 1, &buf, (UINT *)&stage.vertStride, (UINT *)&stage.posOffset);
				if(stage.useIndices)
				{
					buf = UNWRAP(WrappedID3D11Buffer, stage.idxBuf);
					m_pImmediateContext->IASetIndexBuffer(buf, stage.idxFmt, 0);

					m_pImmediateContext->DrawIndexed(stage.numVerts, 0, 0);
				}
				else
				{
					m_pImmediateContext->Draw(stage.numVerts, 0);
				}
			}
		}
	}
	else
	{
		FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));
		m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);
		m_pImmediateContext->VSSetShader(m_DebugRender.MeshVS, NULL, 0);
		m_pImmediateContext->PSSetShader(m_DebugRender.MeshPS, NULL, 0);

		if(m_PrevMeshInputLayout != curRS->IA.Layout)
		{
			SAFE_RELEASE(m_MeshDisplayLayout);

			m_PrevMeshInputLayout = curRS->IA.Layout;

			const vector<D3D11_INPUT_ELEMENT_DESC> curLayout = m_WrappedDevice->GetLayoutDesc(curRS->IA.Layout);

			vector<D3D11_INPUT_ELEMENT_DESC> layoutdesc;

			byte elems = 0;

			int buffers[3] = {0,0,0};

			for(size_t i=0; i < curLayout.size(); i++)
			{
				const D3D11_INPUT_ELEMENT_DESC &layout = curLayout[i];

				if((!_stricmp(layout.SemanticName, "POSITION") || !_stricmp(layout.SemanticName, "SV_Position")) &&
						layout.SemanticIndex == 0) // need to get name from input config
				{
					D3D11_INPUT_ELEMENT_DESC el = layout;
					el.SemanticName = "pos"; // these are the known values since they match WireframeVS
					el.SemanticIndex = 0;

					layoutdesc.push_back(el);

					buffers[0] = layout.InputSlot;

					elems |= 0x1;
				}
				else if(!_stricmp(layout.SemanticName, "TEXCOORD") && layout.SemanticIndex == 0)
				{
					D3D11_INPUT_ELEMENT_DESC el = layout;
					el.SemanticName = "tex"; // these are the known values since they match WireframeVS
					el.SemanticIndex = 0;

					layoutdesc.push_back(el);

					buffers[1] = layout.InputSlot;

					elems |= 0x2;
				}
				else if(!_stricmp(layout.SemanticName, "COLOR") && layout.SemanticIndex == 0)
				{
					D3D11_INPUT_ELEMENT_DESC el = layout;
					el.SemanticName = "col"; // these are the known values since they match WireframeVS
					el.SemanticIndex = 0;

					layoutdesc.push_back(el);

					buffers[2] = layout.InputSlot;

					elems |= 0x4;
				}
			}

			for(m_MeshDisplayNULLVB = 0; m_MeshDisplayNULLVB < 4; m_MeshDisplayNULLVB++)
			{
				bool used = false;

				for(int i=0; i < 3; i++)
				{
					if(elems & (1<<i) && buffers[i] == m_MeshDisplayNULLVB)
					{
						used = true;
						break;
					}
				}

				if(!used)
					break;
			}

			if((elems & 0x1) == 0)
			{
				RDCWARN("No position element!");
				return;
			}

			if((elems & 0x2) == 0)
			{
				D3D11_INPUT_ELEMENT_DESC el;
				el.SemanticName = "tex"; // these are the known values since they match WireframeVS
				el.SemanticIndex = 0;
				el.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				el.AlignedByteOffset = 0;
				el.InputSlot = m_MeshDisplayNULLVB;
				el.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
				el.InstanceDataStepRate = 0;

				layoutdesc.push_back(el);
			}

			if((elems & 0x4) == 0)
			{
				D3D11_INPUT_ELEMENT_DESC el;
				el.SemanticName = "col"; // these are the known values since they match WireframeVS
				el.SemanticIndex = 0;
				el.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				el.AlignedByteOffset = 0;
				el.InputSlot = m_MeshDisplayNULLVB;
				el.InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
				el.InstanceDataStepRate = 0;

				layoutdesc.push_back(el);
			}

			RDCASSERT(layoutdesc.size() == 3);

			if(layoutdesc.size() != 3)
				return;

			HRESULT hr = m_pDevice->CreateInputLayout(&layoutdesc[0], 3, m_DebugRender.MeshVSBytecode, m_DebugRender.MeshVSBytelen, &m_MeshDisplayLayout);

			if(FAILED(hr))
			{
				RDCERR("Failed to create rendermesh input layout %08x", hr);
				return;
			}
		}

		if(m_MeshDisplayLayout == NULL)
		{
			RDCWARN("Couldn't get a mesh display layout");
			return;
		}
		
		m_pImmediateContext->IASetInputLayout(m_MeshDisplayLayout);
		ID3D11Buffer *vb = NULL;
		UINT dummy = 4;
		m_pImmediateContext->IASetVertexBuffers(m_MeshDisplayNULLVB, 1, &vb, &dummy, &dummy);

		// draw solid shaded mode
		if(cfg.solidShadeMode != eShade_None && pipeState.m_IA.Topology < eTopology_PatchList_1CPs)
		{
			m_pImmediateContext->RSSetState(m_DebugRender.RastState);

			pixelData.OutputDisplayFormat = (int)cfg.solidShadeMode;
			pixelData.WireframeColour = Vec3f(0.8f, 0.8f, 0.0f);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

			if(cfg.solidShadeMode == eShade_Lit)
			{
				DebugGeometryCBuffer geomData;

				geomData.InvProj = projMat.Inverse();

				FillCBuffer(m_DebugRender.GenericGSCBuffer, (float *)&geomData, sizeof(DebugGeometryCBuffer));
				m_pImmediateContext->GSSetConstantBuffers(0, 1, &m_DebugRender.GenericGSCBuffer);
				
				m_pImmediateContext->GSSetShader(m_DebugRender.MeshGS, NULL, 0);
			}

			m_WrappedDevice->ReplayLog(frameID, 0, events[0], eReplay_OnlyDraw);
			
			if(cfg.solidShadeMode == eShade_Lit)
				m_pImmediateContext->GSSetShader(NULL, NULL, 0);
		}
		
		// draw wireframe mode
		if(cfg.solidShadeMode == eShade_None || cfg.wireframeDraw || pipeState.m_IA.Topology >= eTopology_PatchList_1CPs)
		{
			m_pImmediateContext->RSSetState(m_WireframeHelpersRS);

			m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.LEqualDepthState, 0);

			pixelData.OutputDisplayFormat = MESHDISPLAY_SOLID;
			pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 0.0f);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);

			if(pipeState.m_IA.Topology >= eTopology_PatchList_1CPs)
				m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

			m_WrappedDevice->ReplayLog(frameID, 0, events[0], eReplay_OnlyDraw);
		}
	}

	m_pImmediateContext->RSSetState(m_WireframeHelpersRS);

	// set up state for drawing helpers
	{
		vertexData.ModelViewProj = projMat.Mul(camMat);
		FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));

		m_pImmediateContext->RSSetState(m_SolidHelpersRS);

		m_pImmediateContext->OMSetDepthStencilState(m_DebugRender.NoDepthState, 0);
		
		m_pImmediateContext->VSSetConstantBuffers(0, 1, &m_DebugRender.GenericVSCBuffer);
		m_pImmediateContext->VSSetShader(m_DebugRender.WireframeVS, NULL, 0);
		m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
		m_pImmediateContext->PSSetShader(m_DebugRender.WireframePS, NULL, 0);
	}
	
	// axis markers
	if(cfg.type == eMeshDataStage_VSIn ||
		(cfg.type == eMeshDataStage_VSOut && pipeState.m_HS.Shader != ResourceId()))
	{
		m_pImmediateContext->PSSetConstantBuffers(0, 1, &m_DebugRender.GenericPSCBuffer);
		
		UINT strides[] = { sizeof(Vec3f) };
		UINT offsets[] = { 0 };

		m_pImmediateContext->IASetVertexBuffers(0, 1, &m_AxisHelper, strides, offsets);
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);
		
		pixelData.WireframeColour = Vec3f(1.0f, 0.0f, 0.0f);
		FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));
		m_pImmediateContext->Draw(2, 0);
		
		pixelData.WireframeColour = Vec3f(0.0f, 1.0f, 0.0f);
		FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));
		m_pImmediateContext->Draw(2, 2);
		
		pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 1.0f);
		FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));
		m_pImmediateContext->Draw(2, 4);
	}

	if(cfg.highlightVert != ~0U)
	{
		const FetchDrawcall *drawcall = m_WrappedDevice->GetDrawcall(frameID, events.back());

		MeshDataStage stage = cfg.type;
		
		if(cfg.type == eMeshDataStage_VSOut && pipeState.m_HS.Shader != ResourceId())
			stage = eMeshDataStage_VSIn;

		if(m_HighlightCache.EID != events.back() || m_HighlightCache.buf != cfg.positionBuf || stage != m_HighlightCache.stage)
		{
			m_HighlightCache.EID = events.back();
			m_HighlightCache.buf = cfg.positionBuf;
			m_HighlightCache.stage = stage;

			if(cfg.type == eMeshDataStage_VSIn)
			{
				m_HighlightCache.data = GetBufferData(cfg.positionBuf, 0, 0);
				m_HighlightCache.topo = curRS->IA.Topo;
			}
			else
			{
				PostVSMeshData postvs = GetPostVSBuffers(frameID, events.back(), stage);
				m_HighlightCache.data.resize(postvs.buf.count);
				memcpy(&m_HighlightCache.data[0], postvs.buf.elems, postvs.buf.count);

				m_HighlightCache.topo = GetPostVSBuffers(frameID, events.back()).GetStage(stage).topo;
			}

			if((drawcall->flags & eDraw_UseIBuffer) == 0)
			{
				m_HighlightCache.indices.clear();
				m_HighlightCache.useidx = false;
			}
			else
			{
				m_HighlightCache.useidx = true;

				ID3D11Buffer *idxBuf = curRS->IA.IndexBuffer;
				DXGI_FORMAT idxFmt = curRS->IA.IndexFormat;
				UINT idxOffs = curRS->IA.IndexOffset;

				bool index16 = (idxFmt == DXGI_FORMAT_R16_UINT); 
				UINT bytesize = index16 ? 2 : 4; 

				vector<byte> idxdata = GetBufferData(idxBuf, idxOffs + drawcall->indexOffset*bytesize, drawcall->numIndices*bytesize);

				uint16_t *idx16 = (uint16_t *)&idxdata[0];
				uint32_t *idx32 = (uint32_t *)&idxdata[0];

				uint32_t numIndices = RDCMIN(drawcall->numIndices, uint32_t(idxdata.size()/bytesize));

				m_HighlightCache.indices.resize(numIndices);

				for(uint32_t i=0; i < numIndices; i++)
					m_HighlightCache.indices[i] = index16 ? uint32_t(idx16[i]) : idx32[i];
			}
		}

		D3D11_PRIMITIVE_TOPOLOGY meshtopo = m_HighlightCache.topo;

		uint32_t idx = cfg.highlightVert;

		byte *data = &m_HighlightCache.data[0]; // buffer start
		byte *dataEnd = data + m_HighlightCache.data.size();

		data += cfg.positionOffset; // to start of position data
		data += drawcall->vertexOffset*cfg.positionStride; // to first vertex
		
		///////////////////////////////////////////////////////////////
		// vectors to be set from buffers, depending on topology

		bool valid = true;

		// this vert (blue dot, required)
		FloatVector activeVertex;
		 
		// primitive this vert is a part of (red prim, optional)
		vector<FloatVector> activePrim;

		// for patch lists, to show other verts in patch (green dots, optional)
		// for non-patch lists, we use the activePrim and adjacentPrimVertices
		// to show what other verts are related
		vector<FloatVector> inactiveVertices;

		// adjacency (line or tri, strips or lists) (green prims, optional)
		// will be N*M long, N adjacent prims of M verts each. M = primSize below
		vector<FloatVector> adjacentPrimVertices; 

		D3D11_PRIMITIVE_TOPOLOGY primTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; // tri or line list
		uint32_t primSize = 3; // number of verts per primitive
		
		if(meshtopo == eTopology_LineList ||
		   meshtopo == eTopology_LineList_Adj ||
		   meshtopo == eTopology_LineStrip ||
		   meshtopo == eTopology_LineStrip_Adj)
		{
			primSize = 2;
			primTopo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		}
		
		activeVertex = InterpretVertex(data, idx, cfg, dataEnd, valid);

		// see http://msdn.microsoft.com/en-us/library/windows/desktop/bb205124(v=vs.85).aspx for
		// how primitive topologies are laid out
		if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
		{
			uint32_t v = uint32_t(idx/2) * 2; // find first vert in primitive

			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
		{
			uint32_t v = uint32_t(idx/3) * 3; // find first vert in primitive

			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+2, cfg, dataEnd, valid));
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ)
		{
			uint32_t v = uint32_t(idx/4) * 4; // find first vert in primitive
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);

			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);

			activePrim.push_back(vs[1]);
			activePrim.push_back(vs[2]);
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ)
		{
			uint32_t v = uint32_t(idx/6) * 6; // find first vert in primitive
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, valid),
				InterpretVertex(data, v+4, cfg, dataEnd, valid),
				InterpretVertex(data, v+5, cfg, dataEnd, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);
			adjacentPrimVertices.push_back(vs[2]);
			
			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);
			adjacentPrimVertices.push_back(vs[4]);
			
			adjacentPrimVertices.push_back(vs[4]);
			adjacentPrimVertices.push_back(vs[5]);
			adjacentPrimVertices.push_back(vs[0]);

			activePrim.push_back(vs[0]);
			activePrim.push_back(vs[2]);
			activePrim.push_back(vs[4]);
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 1U) - 1;
			
			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 2U) - 2;
			
			activePrim.push_back(InterpretVertex(data, v+0, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+1, cfg, dataEnd, valid));
			activePrim.push_back(InterpretVertex(data, v+2, cfg, dataEnd, valid));
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ)
		{
			// find first vert in primitive. In strips a vert isn't
			// in only one primitive, so we pick the first primitive
			// it's in. This means the first N points are in the first
			// primitive, and thereafter each point is in the next primitive
			uint32_t v = RDCMAX(idx, 3U) - 3;
			
			FloatVector vs[] = {
				InterpretVertex(data, v+0, cfg, dataEnd, valid),
				InterpretVertex(data, v+1, cfg, dataEnd, valid),
				InterpretVertex(data, v+2, cfg, dataEnd, valid),
				InterpretVertex(data, v+3, cfg, dataEnd, valid),
			};

			adjacentPrimVertices.push_back(vs[0]);
			adjacentPrimVertices.push_back(vs[1]);

			adjacentPrimVertices.push_back(vs[2]);
			adjacentPrimVertices.push_back(vs[3]);

			activePrim.push_back(vs[1]);
			activePrim.push_back(vs[2]);
		}
		else if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
		{
			// Triangle strip with adjacency is the most complex topology, as
			// we need to handle the ends separately where the pattern breaks.

			uint32_t numidx = drawcall->numIndices;

			if(numidx < 6)
			{
				// not enough indices provided, bail to make sure logic below doesn't
				// need to have tons of edge case detection
				valid = false;
			}
			else if(idx <= 4 || numidx <= 7)
			{
				FloatVector vs[] = {
					InterpretVertex(data, 0, cfg, dataEnd, valid),
					InterpretVertex(data, 1, cfg, dataEnd, valid),
					InterpretVertex(data, 2, cfg, dataEnd, valid),
					InterpretVertex(data, 3, cfg, dataEnd, valid),
					InterpretVertex(data, 4, cfg, dataEnd, valid),

					// note this one isn't used as it's adjacency for the next triangle
					InterpretVertex(data, 5, cfg, dataEnd, valid),

					// min() with number of indices in case this is a tiny strip
					// that is basically just a list
					InterpretVertex(data, RDCMIN(6U, numidx-1), cfg, dataEnd, valid),
				};

				// these are the triangles on the far left of the MSDN diagram above
				adjacentPrimVertices.push_back(vs[0]);
				adjacentPrimVertices.push_back(vs[1]);
				adjacentPrimVertices.push_back(vs[2]);

				adjacentPrimVertices.push_back(vs[4]);
				adjacentPrimVertices.push_back(vs[3]);
				adjacentPrimVertices.push_back(vs[0]);

				adjacentPrimVertices.push_back(vs[4]);
				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[6]);

				activePrim.push_back(vs[0]);
				activePrim.push_back(vs[2]);
				activePrim.push_back(vs[4]);
			}
			else if(idx > numidx-4)
			{
				// in diagram, numidx == 14

				FloatVector vs[] = {
					/*[0]=*/ InterpretVertex(data, numidx-8, cfg, dataEnd, valid), // 6 in diagram

					// as above, unused since this is adjacency for 2-previous triangle
					/*[1]=*/ InterpretVertex(data, numidx-7, cfg, dataEnd, valid), // 7 in diagram
					/*[2]=*/ InterpretVertex(data, numidx-6, cfg, dataEnd, valid), // 8 in diagram
					
					// as above, unused since this is adjacency for previous triangle
					/*[3]=*/ InterpretVertex(data, numidx-5, cfg, dataEnd, valid), // 9 in diagram
					/*[4]=*/ InterpretVertex(data, numidx-4, cfg, dataEnd, valid), // 10 in diagram
					/*[5]=*/ InterpretVertex(data, numidx-3, cfg, dataEnd, valid), // 11 in diagram
					/*[6]=*/ InterpretVertex(data, numidx-2, cfg, dataEnd, valid), // 12 in diagram
					/*[7]=*/ InterpretVertex(data, numidx-1, cfg, dataEnd, valid), // 13 in diagram
				};

				// these are the triangles on the far right of the MSDN diagram above
				adjacentPrimVertices.push_back(vs[2]); // 8 in diagram
				adjacentPrimVertices.push_back(vs[0]); // 6 in diagram
				adjacentPrimVertices.push_back(vs[4]); // 10 in diagram

				adjacentPrimVertices.push_back(vs[4]); // 10 in diagram
				adjacentPrimVertices.push_back(vs[7]); // 13 in diagram
				adjacentPrimVertices.push_back(vs[6]); // 12 in diagram

				adjacentPrimVertices.push_back(vs[6]); // 12 in diagram
				adjacentPrimVertices.push_back(vs[5]); // 11 in diagram
				adjacentPrimVertices.push_back(vs[2]); // 8 in diagram

				activePrim.push_back(vs[2]); // 8 in diagram
				activePrim.push_back(vs[4]); // 10 in diagram
				activePrim.push_back(vs[6]); // 12 in diagram
			}
			else
			{
				// we're in the middle somewhere. Each primitive has two vertices for it
				// so our step rate is 2. The first 'middle' primitive starts at indices 5&6
				// and uses indices all the way back to 0
				uint32_t v = RDCMAX( ( (idx+1) / 2) * 2, 6U) - 6;

				// these correspond to the indices in the MSDN diagram, with {2,4,6} as the
				// main triangle
				FloatVector vs[] = {
					InterpretVertex(data, v+0, cfg, dataEnd, valid),

					// this one is adjacency for 2-previous triangle
					InterpretVertex(data, v+1, cfg, dataEnd, valid),
					InterpretVertex(data, v+2, cfg, dataEnd, valid),

					// this one is adjacency for previous triangle
					InterpretVertex(data, v+3, cfg, dataEnd, valid),
					InterpretVertex(data, v+4, cfg, dataEnd, valid),
					InterpretVertex(data, v+5, cfg, dataEnd, valid),
					InterpretVertex(data, v+6, cfg, dataEnd, valid),
					InterpretVertex(data, v+7, cfg, dataEnd, valid),
					InterpretVertex(data, v+8, cfg, dataEnd, valid),
				};

				// these are the triangles around {2,4,6} in the MSDN diagram above
				adjacentPrimVertices.push_back(vs[0]);
				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[4]);

				adjacentPrimVertices.push_back(vs[2]);
				adjacentPrimVertices.push_back(vs[5]);
				adjacentPrimVertices.push_back(vs[6]);

				adjacentPrimVertices.push_back(vs[6]);
				adjacentPrimVertices.push_back(vs[8]);
				adjacentPrimVertices.push_back(vs[4]);

				activePrim.push_back(vs[2]);
				activePrim.push_back(vs[4]);
				activePrim.push_back(vs[6]);
			}
		}
		else if(meshtopo >= D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST)
		{
			uint32_t dim = (meshtopo-D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) + 1;

			uint32_t v0 = uint32_t(idx/dim) * dim;

			for(uint32_t v = v0; v < v0+dim; v++)
			{
				if(v != idx && valid)
					inactiveVertices.push_back(InterpretVertex(data, v, cfg, dataEnd, valid));
			}
		}
		else // if(meshtopo == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST) point list, or unknown/unhandled type
		{
			// no adjacency, inactive verts or active primitive
		}

		if(valid)
		{
			////////////////////////////////////////////////////////////////
			// prepare rendering (for both vertices & primitives)

			// if data is from post transform, it will be in clipspace
			if((cfg.type != eMeshDataStage_VSIn && pipeState.m_HS.Shader == ResourceId()) ||
				(cfg.type == eMeshDataStage_GSOut && pipeState.m_HS.Shader != ResourceId()))
			{
				vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
				m_pImmediateContext->VSSetShader(m_DebugRender.WireframeHomogVS, NULL, 0);
				m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericHomogLayout);
			}
			else
			{
				vertexData.ModelViewProj = projMat.Mul(camMat);
				m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);
			}

			FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));

			D3D11_MAPPED_SUBRESOURCE mapped;
			HRESULT hr = S_OK;
			UINT strides[] = { sizeof(Vec4f) };
			UINT offsets[] = { 0 };
			m_pImmediateContext->IASetVertexBuffers(0, 1, &m_TriHighlightHelper, (UINT *)&strides, (UINT *)&offsets);

			////////////////////////////////////////////////////////////////
			// render primitives

			m_pImmediateContext->IASetPrimitiveTopology(primTopo);

			// Draw active primitive (red)
			pixelData.WireframeColour = Vec3f(1.0f, 0.0f, 0.0f);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			if(activePrim.size() >= primSize)
			{
				hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

				if(FAILED(hr))
				{
					RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
					return;
				}

				memcpy(mapped.pData, &activePrim[0], sizeof(Vec4f)*primSize);
				m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

				m_pImmediateContext->Draw(primSize, 0);
			}

			// Draw adjacent primitives (green)
			pixelData.WireframeColour = Vec3f(0.0f, 1.0f, 0.0f);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
			{
				hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

				if(FAILED(hr))
				{
					RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
					return;
				}

				memcpy(mapped.pData, &adjacentPrimVertices[0], sizeof(Vec4f)*adjacentPrimVertices.size());
				m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

				m_pImmediateContext->Draw((UINT)adjacentPrimVertices.size(), 0);
			}

			////////////////////////////////////////////////////////////////
			// prepare to render dots (set new VS params and topology)
			float scale = 800.0f/float(GetHeight());
			float asp = float(GetWidth())/float(GetHeight());

			vertexData.SpriteSize = Vec2f(scale/asp, scale);
			FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));

			// Draw active vertex (blue)
			pixelData.WireframeColour = Vec3f(0.0f, 0.0f, 1.0f);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			FloatVector vertSprite[4] = {
				activeVertex,
				activeVertex,
				activeVertex,
				activeVertex,
			};

			hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

			if(FAILED(hr))
			{
				RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
				return;
			}

			memcpy(mapped.pData, vertSprite, sizeof(vertSprite));
			m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

			m_pImmediateContext->Draw(4, 0);

			// Draw inactive vertices (green)
			pixelData.WireframeColour = Vec3f(0.0f, 1.0f, 0.0f);
			FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

			for(size_t i=0; i < inactiveVertices.size(); i++)
			{
				vertSprite[0] = vertSprite[1] = vertSprite[2] = vertSprite[3] = inactiveVertices[i];

				hr = m_pImmediateContext->Map(m_TriHighlightHelper, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

				if(FAILED(hr))
				{
					RDCERR("Failde to map m_TriHighlightHelper %08x", hr);
					return;
				}

				memcpy(mapped.pData, vertSprite, sizeof(vertSprite));
				m_pImmediateContext->Unmap(m_TriHighlightHelper, 0);

				m_pImmediateContext->Draw(4, 0);
			}
		}

		if(cfg.type != eMeshDataStage_VSIn)
			m_pImmediateContext->VSSetShader(m_DebugRender.WireframeVS, NULL, 0);
	}

	// 'fake' helper frustum
	if((cfg.type != eMeshDataStage_VSIn && pipeState.m_HS.Shader == ResourceId()) ||
		(cfg.type == eMeshDataStage_GSOut && pipeState.m_HS.Shader != ResourceId()))
	{
		UINT strides[] = { sizeof(Vec3f) };
		UINT offsets[] = { 0 };

		vertexData.SpriteSize = Vec2f();
		vertexData.ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
		FillCBuffer(m_DebugRender.GenericVSCBuffer, (float *)&vertexData, sizeof(DebugVertexCBuffer));

		m_pImmediateContext->IASetVertexBuffers(0, 1, &m_FrustumHelper, (UINT *)&strides, (UINT *)&offsets);
		m_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		m_pImmediateContext->IASetInputLayout(m_DebugRender.GenericLayout);

		pixelData.WireframeColour = Vec3f(0.5f, 0.5f, 0.5f);
		FillCBuffer(m_DebugRender.GenericPSCBuffer, (float *)&pixelData, sizeof(DebugPixelCBufferData));

		m_pImmediateContext->Draw(24, 0);
	}
}
