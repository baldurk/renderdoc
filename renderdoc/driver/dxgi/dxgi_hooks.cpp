/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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



#include "dxgi_wrapped.h"
#include "hooks/hooks.h"

#define DLL_NAME "dxgi.dll"

typedef HRESULT (WINAPI* PFN_CREATE_DXGI_FACTORY)(REFIID, void **);
typedef HRESULT (WINAPI* PFN_CREATE_DXGI_FACTORY2)(UINT, REFIID, void **);

class DXGIHook : LibraryHook
{
public:
	DXGIHook() { LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this); m_EnabledHooks = true; }

	bool CreateHooks(const char *libName)
	{
		bool success = true;

		success &= CreateDXGIFactory.Initialize("CreateDXGIFactory", DLL_NAME, CreateDXGIFactory_hook);
		success &= CreateDXGIFactory1.Initialize("CreateDXGIFactory1", DLL_NAME, CreateDXGIFactory1_hook);
		// don't mind if this doesn't succeed
		CreateDXGIFactory2.Initialize("CreateDXGIFactory2", DLL_NAME, CreateDXGIFactory2_hook);

		if(!success) return false;

		m_HasHooks = true;
		m_EnabledHooks = true;

		return true;
	}

	void EnableHooks(const char *libName, bool enable)
	{
		m_EnabledHooks = enable;
	}

	void OptionsUpdated(const char *libName) {}

	bool UseHooks()
	{
		return (dxgihooks.m_HasHooks && dxgihooks.m_EnabledHooks);
	}

	static HRESULT CreateWrappedFactory1(REFIID riid, void **ppFactory)
	{
		if(dxgihooks.m_HasHooks)
			return dxgihooks.CreateDXGIFactory1_hook(riid, ppFactory);

		HMODULE dxgi = GetModuleHandleA("dxgi.dll");

		if(dxgi)
		{
			PFN_CREATE_DXGI_FACTORY createFunc = (PFN_CREATE_DXGI_FACTORY)GetProcAddress(dxgi, "CreateDXGIFactory1");

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
		else
		{
			RDCERR("Something went seriously wrong, dxgi.dll couldn't be loaded!");
			return E_UNEXPECTED;
		}
	}

	static HRESULT CreateWrappedFactory2(UINT Flags, REFIID riid, void **ppFactory)
	{
		if(dxgihooks.m_HasHooks)
			return dxgihooks.CreateDXGIFactory2_hook(Flags, riid, ppFactory);

		HMODULE dxgi = GetModuleHandleA("dxgi.dll");

		if(dxgi)
		{
			PFN_CREATE_DXGI_FACTORY2 createFunc = (PFN_CREATE_DXGI_FACTORY2)GetProcAddress(dxgi, "CreateDXGIFactory2");

			if(!createFunc)
			{
				RDCERR("Trying to create hooked dxgi factory without dxgi loaded");
				return E_INVALIDARG;
			}
		
			HRESULT ret = createFunc(Flags, riid, ppFactory);
		
			if(SUCCEEDED(ret))
				RefCountDXGIObject::HandleWrap(riid, ppFactory);

			return ret;
		}
		else
		{
			RDCERR("Something went seriously wrong, dxgi.dll couldn't be loaded!");
			return E_UNEXPECTED;
		}
	}

private:
	static DXGIHook dxgihooks;

	bool m_HasHooks;
	bool m_EnabledHooks;

	Hook<PFN_CREATE_DXGI_FACTORY> CreateDXGIFactory;
	Hook<PFN_CREATE_DXGI_FACTORY> CreateDXGIFactory1;
	Hook<PFN_CREATE_DXGI_FACTORY2> CreateDXGIFactory2;
	
	static HRESULT WINAPI CreateDXGIFactory_hook(__in REFIID riid, __out  void **ppFactory)
	{
		if(ppFactory) *ppFactory = NULL;
		HRESULT ret = dxgihooks.CreateDXGIFactory()(riid, ppFactory);
		
		if(SUCCEEDED(ret) && dxgihooks.m_EnabledHooks)
			RefCountDXGIObject::HandleWrap(riid, ppFactory);

		return ret;
	}

	static HRESULT WINAPI CreateDXGIFactory1_hook(__in REFIID riid, __out  void **ppFactory)
	{
		if(ppFactory) *ppFactory = NULL;
		HRESULT ret = dxgihooks.CreateDXGIFactory1()(riid, ppFactory);
		
		if(SUCCEEDED(ret) && dxgihooks.m_EnabledHooks)
			RefCountDXGIObject::HandleWrap(riid, ppFactory);

		return ret;
	}

	static HRESULT WINAPI CreateDXGIFactory2_hook(UINT Flags, REFIID riid,  void **ppFactory)
	{
		if(ppFactory) *ppFactory = NULL;
		HRESULT ret = dxgihooks.CreateDXGIFactory2()(Flags, riid, ppFactory);
		
		if(SUCCEEDED(ret) && dxgihooks.m_EnabledHooks)
			RefCountDXGIObject::HandleWrap(riid, ppFactory);

		return ret;
	}
};

DXGIHook DXGIHook::dxgihooks;

extern "C" __declspec(dllexport)
HRESULT __cdecl RENDERDOC_CreateWrappedDXGIFactory1(REFIID riid, void **ppFactory)
{
	return DXGIHook::CreateWrappedFactory1(riid, ppFactory);
}

extern "C" __declspec(dllexport)
HRESULT __cdecl RENDERDOC_CreateWrappedDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory)
{
	return DXGIHook::CreateWrappedFactory2(Flags, riid, ppFactory);
}
