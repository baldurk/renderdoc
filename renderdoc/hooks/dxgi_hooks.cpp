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



#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks.h"

#define DLL_NAME "dxgi.dll"

typedef HRESULT (WINAPI* PFN_CREATE_DXGI_FACTORY)( __in REFIID, __out  void **ppFactory);

class DXGIHook : LibraryHook
{
public:
	DXGIHook() { LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this); m_EnabledHooks = true; }

	bool CreateHooks(const char *libName)
	{
		bool success = true;

#if USE_MHOOK
		// require d3d11.dll hooked as well for proper operation
		if(GetModuleHandleA("d3d11.dll") == NULL) return false;
#endif

		success &= CreateDXGIFactory.Initialize("CreateDXGIFactory", DLL_NAME, CreateDXGIFactory_hook);
		success &= CreateDXGIFactory1.Initialize("CreateDXGIFactory1", DLL_NAME, CreateDXGIFactory1_hook);

		if(!success) return false;

		m_HasHooks = true;
		m_EnabledHooks = true;

		return true;
	}

	void EnableHooks(const char *libName, bool enable)
	{
		m_EnabledHooks = enable;
	}

	bool UseHooks()
	{
		return (dxgihooks.m_HasHooks && dxgihooks.m_EnabledHooks);
	}

	static HRESULT CreateWrappedFactory1(REFIID riid, void **ppFactory)
	{
		if(dxgihooks.m_HasHooks)
			return dxgihooks.CreateDXGIFactory1_hook(riid, ppFactory);

		PFN_CREATE_DXGI_FACTORY createFunc = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(GetModuleHandleA("dxgi.dll"), "CreateDXGIFactory1");

		if(!createFunc)
		{
			RDCERR("Trying to create hooked dxgi factory without dxgi loaded");
			return E_INVALIDARG;
		}
		
		HRESULT ret = createFunc(riid, ppFactory);
		
		if(SUCCEEDED(ret))
			RefCountDXGIObject::HandleWrap(riid, ppFactory);

		return ret;
	}

private:
	static DXGIHook dxgihooks;

	bool m_HasHooks;
	bool m_EnabledHooks;

	Hook<PFN_CREATE_DXGI_FACTORY> CreateDXGIFactory;
	Hook<PFN_CREATE_DXGI_FACTORY> CreateDXGIFactory1;
	
	static HRESULT WINAPI CreateDXGIFactory_hook(__in REFIID riid, __out  void **ppFactory)
	{
		HRESULT ret = dxgihooks.CreateDXGIFactory()(riid, ppFactory);
		
		if(SUCCEEDED(ret) && dxgihooks.m_EnabledHooks)
			RefCountDXGIObject::HandleWrap(riid, ppFactory);

		return ret;
	}

	static HRESULT WINAPI CreateDXGIFactory1_hook(__in REFIID riid, __out  void **ppFactory)
	{
		HRESULT ret = dxgihooks.CreateDXGIFactory1()(riid, ppFactory);
		
		if(SUCCEEDED(ret) && dxgihooks.m_EnabledHooks)
			RefCountDXGIObject::HandleWrap(riid, ppFactory);

		return ret;
	}
};

DXGIHook DXGIHook::dxgihooks;

extern "C" __declspec(dllexport)
HRESULT __cdecl RENDERDOC_CreateWrappedDXGIFactory1(__in REFIID riid, __out  void **ppFactory)
{
	return DXGIHook::CreateWrappedFactory1(riid, ppFactory);
}
