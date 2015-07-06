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

#include <d3d11.h>

#include "core/core.h"
#include "driver/d3d11/d3d11_manager.h"
#include "common/common.h"

class Serialiser;
class WrappedID3D11Device;
class WrappedID3D11DeviceContext;
class D3D11ResourceManager;

struct D3D11RenderState
{
	D3D11RenderState(Serialiser *ser);
	D3D11RenderState(WrappedID3D11DeviceContext *context);
	D3D11RenderState(const D3D11RenderState &other);
	~D3D11RenderState();

	D3D11RenderState &operator =(const D3D11RenderState &other);

	void ApplyState(WrappedID3D11DeviceContext *context);
	void Clear();

	///////////////////////////////////////////////////////////////////////////////
	// pipeline-auto NULL. When binding a resource for write, it will be
	// unbound anywhere that it is bound for read.
	//
	// if binding a resource for READ, and it's still bound for write, the
	// bind will be disallowed and NULL will be bound instead
	//
	// need to be aware of depth-stencil as a special case, DSV can be flagged read-only
	// of depth, stencil or both to allow read binds of that component at the same time.

	bool IsBoundIUnknownForWrite(IUnknown *resource, bool readDepthOnly, bool readStencilOnly);
	void UnbindIUnknownForRead(IUnknown *resource, bool allowDepthOnly, bool allowStencilOnly);

	// just for utility, not used below
	void UnbindIUnknownForWrite(IUnknown *resource);

	template<typename T>
	bool IsBoundForWrite(T *resource)
	{
		if(resource == NULL) return false;
		
		return IsBoundIUnknownForWrite((IUnknown *)resource, false, false);
	}
		
	template<>
	bool IsBoundForWrite(ID3D11Resource *resource)
	{
		if(resource == NULL) return false;
		
		bool readDepthOnly = false;
		bool readStencilOnly = false;

		D3D11_RESOURCE_DIMENSION dim;
		resource->GetType(&dim);

		if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
		{
			D3D11_TEXTURE1D_DESC d;
			((ID3D11Texture1D *)resource)->GetDesc(&d);

			if(d.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
				d.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
			{
				readStencilOnly = true;
			}
			if(d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
				d.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
			{
				readDepthOnly = true;
			}
		}
		else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		{
			D3D11_TEXTURE2D_DESC d;
			((ID3D11Texture2D *)resource)->GetDesc(&d);

			if(d.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
				d.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
			{
				readStencilOnly = true;
			}
			if(d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
				d.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
			{
				readDepthOnly = true;
			}
		}

		return IsBoundIUnknownForWrite((IUnknown *)resource, readDepthOnly, readStencilOnly);
	}
	
	template<>
	bool IsBoundForWrite(ID3D11ShaderResourceView *resource)
	{
		if(resource == NULL) return false;

		ID3D11Resource *res = NULL;
		resource->GetResource(&res);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
		resource->GetDesc(&srvd);

		bool readDepthOnly = false;
		bool readStencilOnly = false;

		D3D11_RESOURCE_DIMENSION dim;
		res->GetType(&dim);

		if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
		{
			D3D11_TEXTURE1D_DESC d;
			((ID3D11Texture1D *)res)->GetDesc(&d);

			if(d.Format == DXGI_FORMAT_R24G8_TYPELESS ||
				d.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
			{
				d.Format = srvd.Format;
			}

			if(d.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
				d.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
			{
				readStencilOnly = true;
			}
			if(d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
				d.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
			{
				readDepthOnly = true;
			}
		}
		else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		{
			D3D11_TEXTURE2D_DESC d;
			((ID3D11Texture2D *)res)->GetDesc(&d);

			if(d.Format == DXGI_FORMAT_R24G8_TYPELESS ||
				d.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
			{
				d.Format = srvd.Format;
			}

			if(d.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
				d.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
			{
				readStencilOnly = true;
			}
			if(d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
				d.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
			{
				readDepthOnly = true;
			}
		}

		bool ret = IsBoundIUnknownForWrite((IUnknown *)res, readDepthOnly, readStencilOnly);

		SAFE_RELEASE(res);

		return ret;
	}
	
	template<typename T>
	void UnbindForRead(T *resource)
	{
		if(resource == NULL) return;
		UnbindIUnknownForRead((IUnknown *)resource, false, false);
	}
	
	template<>
	void UnbindForRead(ID3D11RenderTargetView *resource)
	{
		if(resource == NULL) return;

		ID3D11Resource *res = NULL;
		resource->GetResource(&res);

		UnbindIUnknownForRead((IUnknown *)res, false, false);

		SAFE_RELEASE(res);
	}
	
	template<>
	void UnbindForRead(ID3D11DepthStencilView *resource)
	{
		if(resource == NULL) return;

		ID3D11Resource *res = NULL;
		resource->GetResource(&res);

		D3D11_DEPTH_STENCIL_VIEW_DESC d;
		resource->GetDesc(&d);

		if(d.Flags == (D3D11_DSV_READ_ONLY_DEPTH|D3D11_DSV_READ_ONLY_STENCIL))
		{
			// don't need to.
		}
		else if(d.Flags == D3D11_DSV_READ_ONLY_DEPTH)
		{
			UnbindIUnknownForRead((IUnknown *)res, true, false);
		}
		else if(d.Flags == D3D11_DSV_READ_ONLY_STENCIL)
		{
			UnbindIUnknownForRead((IUnknown *)res, false, true);
		}
		else
		{
			UnbindIUnknownForRead((IUnknown *)res, false, false);
		}

		SAFE_RELEASE(res);
	}
	
	template<>
	void UnbindForRead(ID3D11UnorderedAccessView *resource)
	{
		if(resource == NULL) return;

		ID3D11Resource *res = NULL;
		resource->GetResource(&res);

		UnbindIUnknownForRead((IUnknown *)res, false, false);

		SAFE_RELEASE(res);
	}

	/////////////////////////////////////////////////////////////////////////
	// Utility functions to swap resources around, removing and adding refs

	void TakeRef(ID3D11DeviceChild *p);
	void ReleaseRef(ID3D11DeviceChild *p);

	template<typename T>
	void ChangeRefRead(T **stateArray, T *const*newArray, size_t offset, size_t num)
	{
		for(size_t i=0; i < num; i++)
		{
			T *old = stateArray[offset+i];
			ReleaseRef(old);

			if(newArray[i] == NULL)
			{
				stateArray[offset+i] = newArray[i];
			}
			else
			{
				if(IsBoundForWrite(newArray[i]))
				{
					//RDCDEBUG("Resource %d was bound for write, forcing to NULL", offset+i);
					stateArray[offset+i] = NULL;
				}
				else
				{
					stateArray[offset+i] = newArray[i];
				}
			}
			TakeRef(stateArray[offset+i]);
		}
	}

	template<typename T>
	void ChangeRefWrite(T **stateArray, T *const*newArray, size_t offset, size_t num)
	{
		for(size_t i=0; i < num; i++)
		{
			T *old = stateArray[offset+i];
			ReleaseRef(old);
			
			if(newArray[i]) UnbindForRead(newArray[i]);
			stateArray[offset+i] = newArray[i];
			TakeRef(stateArray[offset+i]);
		}
	}

	template<typename T>
	void ChangeRefRead(T *&stateItem, T *newItem)
	{
		ReleaseRef(stateItem);
		stateItem = newItem;

		if(newItem == NULL)
		{
			stateItem = newItem;
		}
		else
		{
			if(IsBoundForWrite(newItem))
			{
				//RDCDEBUG("Resource was bound for write, forcing to NULL");
				stateItem = NULL;
			}
			else
			{
				stateItem = newItem;
			}
		}

		TakeRef(newItem);
	}

	template<typename T>
	void ChangeRefWrite(T *&stateItem, T *newItem)
	{
		ReleaseRef(stateItem);
		stateItem = newItem;
		if(newItem) UnbindForRead(newItem);
		TakeRef(newItem);
	}
	
	template<typename T>
	void Change(T *stateArray, const T *newArray, size_t offset, size_t num)
	{
		for(size_t i=0; i < num; i++)
			stateArray[i+offset] = newArray[i];
	}

	template<typename T>
	void Change(T &stateItem, const T &newItem)
	{
		stateItem = newItem;
	}

	/////////////////////////////////////////////////////////////////////////
	// Implement any checks that D3D does that will change the state in ways
	// that might not be obvious/intended.

	// validate an output merger combination of render targets and depth view
	bool ValidOutputMerger(ID3D11RenderTargetView **RTs, ID3D11DepthStencilView *depth);

	struct inputassembler
	{
		ID3D11InputLayout *Layout;
		D3D11_PRIMITIVE_TOPOLOGY Topo;
		ID3D11Buffer *VBs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT Strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		UINT Offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
		ID3D11Buffer *IndexBuffer;
		DXGI_FORMAT IndexFormat;
		UINT IndexOffset;

		bool Used_VB(WrappedID3D11Device *device, uint32_t slot) const;
	} IA;

	struct shader
	{
		ID3D11DeviceChild *Shader;
		ID3D11Buffer *ConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		UINT CBOffsets[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		UINT CBCounts[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
		ID3D11ShaderResourceView *SRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
		ID3D11SamplerState *Samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
		ID3D11ClassInstance *Instances[D3D11_SHADER_MAX_INTERFACES];
		UINT NumInstances;
		
		bool Used_CB(uint32_t slot) const;
		bool Used_SRV(uint32_t slot) const;
		bool Used_UAV(uint32_t slot) const;
	} VS, HS, DS, GS, PS, CS;
	
	ID3D11UnorderedAccessView *CSUAVs[D3D11_1_UAV_SLOT_COUNT];

	struct streamout
	{
		ID3D11Buffer *Buffers[D3D11_SO_BUFFER_SLOT_COUNT];
		UINT Offsets[D3D11_SO_BUFFER_SLOT_COUNT];
	} SO;

	struct rasterizer
	{
		UINT NumViews, NumScissors;
		D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		ID3D11RasterizerState *State;
	} RS;

	struct outmerger
	{
		ID3D11DepthStencilState *DepthStencilState;
		UINT StencRef;

		ID3D11BlendState *BlendState;
		FLOAT BlendFactor[4];
		UINT SampleMask;

		ID3D11DepthStencilView *DepthView;

		ID3D11RenderTargetView *RenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];

		UINT UAVStartSlot;
		ID3D11UnorderedAccessView *UAVs[D3D11_1_UAV_SLOT_COUNT];
	} OM;

	void SetSerialiser(Serialiser *ser) { m_pSerialiser = ser; }
	void Serialise(LogState state, WrappedID3D11Device *device);

	void SetImmediatePipeline(WrappedID3D11Device *device) { m_ImmediatePipeline = true; m_pDevice = device; }
	void SetDevice(WrappedID3D11Device *device) { m_pDevice = device; }

	void MarkReferenced(WrappedID3D11DeviceContext *ctx, bool initial) const;
	void MarkDirty(D3D11ResourceManager *manager) const;
private:
	void AddRefs();
	void ReleaseRefs();

	Serialiser *m_pSerialiser;
	bool m_ImmediatePipeline;
	WrappedID3D11Device *m_pDevice;
};

struct D3D11RenderStateTracker
{
	public:
		D3D11RenderStateTracker(WrappedID3D11DeviceContext *ctx);
		~D3D11RenderStateTracker();
	private:
		D3D11RenderState m_RS;
		WrappedID3D11DeviceContext *m_pContext;
};
