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


#pragma once

#define INITGUID

// if you don't have the windows 8.1 SDK, remove this define to exclude the 11.1 functionality
#define INCLUDE_D3D_11_1

#include <dxgi.h>
#include <d3d11.h>

#if defined(INCLUDE_D3D_11_1)
#include <d3d11_1.h>
#include <d3d11_2.h>
#else
#define D3D11_1_UAV_SLOT_COUNT 64
#endif

#include "api/replay/renderdoc_replay.h"
#include "core/core.h"

class WrappedID3D11Device;
struct D3D11RenderState;

HMODULE GetD3DCompiler();

ResourceFormat MakeResourceFormat(DXGI_FORMAT fmt);
DXGI_FORMAT MakeDXGIFormat(ResourceFormat fmt);
PrimitiveTopology MakePrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topo);
D3D11_PRIMITIVE_TOPOLOGY MakeD3D11PrimitiveTopology(PrimitiveTopology Topo);

ShaderReflection *MakeShaderReflection(DXBC::DXBCFile *dxbc);

template<class T>
inline void SetDebugName( T* pObj, const char* name )
{
    if(pObj) pObj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
}

template<class T>
inline const char* GetDebugName( T* pObj )
{
	static char tmpBuf[1024] = {0};
	UINT size = 1023;
	if(pObj)
	{
		HRESULT hr = pObj->GetPrivateData(WKPDID_D3DDebugObjectName, &size, tmpBuf);
		if(FAILED(hr)) return "";

		tmpBuf[size] = 0;
		return tmpBuf;
	}
	return "";
}

class RefCounter
{
private:
	IUnknown *m_pReal;
	unsigned int m_iRefcount;
	bool m_SelfDeleting;
protected:
	void SetSelfDeleting(bool selfDelete) { m_SelfDeleting = selfDelete; }

	// used for derived classes that need to soft ref but are handling their
	// own self-deletion
	static void AddDeviceSoftref(WrappedID3D11Device *device);
	static void ReleaseDeviceSoftref(WrappedID3D11Device *device);

public:
	RefCounter(IUnknown *real, bool selfDelete = true) : m_pReal(real), m_iRefcount(1), m_SelfDeleting(selfDelete) {}
	virtual ~RefCounter() {}
	
	unsigned int GetRefCount() { return m_iRefcount; }

	//////////////////////////////
	// implement IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface( 
		/* [in] */ REFIID riid,
		/* [annotation][iid_is][out] */ 
		__RPC__deref_out  void **ppvObject);

	ULONG STDMETHODCALLTYPE AddRef()
	{
		InterlockedIncrement(&m_iRefcount);
		return m_iRefcount;
	}
	ULONG STDMETHODCALLTYPE Release()
	{
		unsigned int ret = InterlockedDecrement(&m_iRefcount);
		if(ret == 0 && m_SelfDeleting)
			delete this;
		return ret;
	}

	unsigned int SoftRef(WrappedID3D11Device *device);
	unsigned int SoftRelease(WrappedID3D11Device *device);
};

#define IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER \
	ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::AddRef(); } \
	ULONG STDMETHODCALLTYPE Release() { return RefCounter::Release(); } \
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) { return RefCounter::QueryInterface(riid, ppvObject); }

#define IMPLEMENT_IUNKNOWN_WITH_REFCOUNTER_CUSTOMQUERY \
	ULONG STDMETHODCALLTYPE AddRef() { return RefCounter::AddRef(); } \
	ULONG STDMETHODCALLTYPE Release() { return RefCounter::Release(); }

#define IMPLEMENT_FUNCTION_SERIALISED(ret, func) ret func; bool CONCAT(Serialise_, func);

#include "serialise/serialiser.h"

// I don't really like this but it's not the end of the world - declare d3d specialisations to enforce
// that this specialisation gets used.
template<> void Serialiser::Serialise(const char *name, D3D11_BUFFER_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_TEXTURE1D_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_TEXTURE2D_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_TEXTURE3D_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_SHADER_RESOURCE_VIEW_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_RENDER_TARGET_VIEW_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_DEPTH_STENCIL_VIEW_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_UNORDERED_ACCESS_VIEW_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_BLEND_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_DEPTH_STENCIL_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_SAMPLER_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_QUERY_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_COUNTER_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_INPUT_ELEMENT_DESC &el);
template<> void Serialiser::Serialise(const char *name, D3D11_SO_DECLARATION_ENTRY &el);
template<> void Serialiser::Serialise(const char *name, D3D11_SUBRESOURCE_DATA &el);

#if defined(INCLUDE_D3D_11_1)
template<> void Serialiser::Serialise(const char *name, D3D11_BLEND_DESC1 &el);
template<> void Serialiser::Serialise(const char *name, D3D11_RASTERIZER_DESC1 &el);
#endif

#pragma region Chunks

enum D3D11ChunkType
{
	DEVICE_INIT = FIRST_CHUNK_ID,
	SET_RESOURCE_NAME,
	RELEASE_RESOURCE,
	CREATE_SWAP_BUFFER,

	CREATE_TEXTURE_1D,
	CREATE_TEXTURE_2D,
	CREATE_TEXTURE_3D,
	CREATE_BUFFER,
	CREATE_VERTEX_SHADER,
	CREATE_HULL_SHADER,
	CREATE_DOMAIN_SHADER,
	CREATE_GEOMETRY_SHADER,
	CREATE_GEOMETRY_SHADER_WITH_SO,
	CREATE_PIXEL_SHADER,
	CREATE_COMPUTE_SHADER,
	GET_CLASS_INSTANCE,
	CREATE_CLASS_INSTANCE,
	CREATE_CLASS_LINKAGE,
	CREATE_SRV,
	CREATE_RTV,
	CREATE_DSV,
	CREATE_UAV,
	CREATE_INPUT_LAYOUT,
	CREATE_BLEND_STATE,
	CREATE_DEPTHSTENCIL_STATE,
	CREATE_RASTER_STATE,
	CREATE_SAMPLER_STATE,
	CREATE_QUERY,
	CREATE_PREDICATE,
	CREATE_COUNTER,
	CREATE_DEFERRED_CONTEXT,
	SET_EXCEPTION_MODE,
	OPEN_SHARED_RESOURCE,
	
	CAPTURE_SCOPE,

	SET_INPUT_LAYOUT,
	FIRST_CONTEXT_CHUNK = SET_INPUT_LAYOUT,
	SET_VBUFFER,
	SET_IBUFFER,
	SET_TOPOLOGY,

	SET_VS_CBUFFERS,
	SET_VS_RESOURCES,
	SET_VS_SAMPLERS,
	SET_VS,
	
	SET_HS_CBUFFERS,
	SET_HS_RESOURCES,
	SET_HS_SAMPLERS,
	SET_HS,
	
	SET_DS_CBUFFERS,
	SET_DS_RESOURCES,
	SET_DS_SAMPLERS,
	SET_DS,
	
	SET_GS_CBUFFERS,
	SET_GS_RESOURCES,
	SET_GS_SAMPLERS,
	SET_GS,

	SET_SO_TARGETS,

	SET_PS_CBUFFERS,
	SET_PS_RESOURCES,
	SET_PS_SAMPLERS,
	SET_PS,
	
	SET_CS_CBUFFERS,
	SET_CS_RESOURCES,
	SET_CS_UAVS,
	SET_CS_SAMPLERS,
	SET_CS,

	SET_VIEWPORTS,
	SET_SCISSORS,
	SET_RASTER,

	SET_RTARGET,
	SET_RTARGET_AND_UAVS,
	SET_BLEND,
	SET_DEPTHSTENCIL,
	
	DRAW_INDEXED_INST,
	DRAW_INST,
	DRAW_INDEXED,
	DRAW,
	DRAW_AUTO,
	DRAW_INDEXED_INST_INDIRECT,
	DRAW_INST_INDIRECT,

	MAP,
	UNMAP,
	
	COPY_SUBRESOURCE_REGION,
	COPY_RESOURCE,
	UPDATE_SUBRESOURCE,
	COPY_STRUCTURE_COUNT,
	RESOLVE_SUBRESOURCE,
	GENERATE_MIPS,

	CLEAR_DSV,
	CLEAR_RTV,
	CLEAR_UAV_INT,
	CLEAR_UAV_FLOAT,
	CLEAR_STATE,
	
	EXECUTE_CMD_LIST,
	DISPATCH,
	DISPATCH_INDIRECT,
	FINISH_CMD_LIST,
	FLUSH,

	SET_PREDICATION,
	SET_RESOURCE_MINLOD,

	BEGIN,
	END,

	CREATE_RASTER_STATE1,
	CREATE_BLEND_STATE1,

	COPY_SUBRESOURCE_REGION1,
	UPDATE_SUBRESOURCE1,
	CLEAR_VIEW,

	SET_VS_CBUFFERS1,
	SET_HS_CBUFFERS1,
	SET_DS_CBUFFERS1,
	SET_GS_CBUFFERS1,
	SET_PS_CBUFFERS1,
	SET_CS_CBUFFERS1,

	PUSH_EVENT,
	SET_MARKER,
	POP_EVENT,

	DEBUG_MESSAGES,

	CONTEXT_CAPTURE_HEADER, // chunk at beginning of context's chunk stream
	CONTEXT_CAPTURE_FOOTER, // chunk at end of context's chunk stream

	NUM_D3D11_CHUNKS,
};

#pragma endregion Chunks
