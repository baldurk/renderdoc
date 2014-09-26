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


#include "os/os_specific.h"

#include <time.h>

double Timing::GetTickFrequency()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return double(li.QuadPart)/1000.0;
}

uint64_t Timing::GetTick()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);

	return li.QuadPart;
}

uint64_t Timing::GetUnixTimestamp()
{
	return (uint64_t)time(NULL);
}

namespace Atomic
{
	int32_t Inc32(volatile int32_t *i)
	{
		return (int32_t)InterlockedIncrement((volatile LONG *)i);
	}

	int64_t Inc64(volatile int64_t *i)
	{
		return (int64_t)InterlockedIncrement64((volatile LONG64 *)i);
	}

	int64_t Dec64(volatile int64_t *i)
	{
		return (int64_t)InterlockedDecrement64((volatile LONG64 *)i);
	}
	
	int64_t ExchAdd64(volatile int64_t *i, int64_t a)
	{
		return (int64_t)InterlockedExchangeAdd64((volatile LONG64 *)i, a);
	}
};

namespace Threading
{

	CriticalSection::CriticalSectionTemplate()
	{
		InitializeCriticalSection(&m_Data);
	}

	CriticalSection::~CriticalSectionTemplate()
	{
		DeleteCriticalSection(&m_Data);
	}

	void CriticalSection::Lock()
	{
		EnterCriticalSection(&m_Data);
	}

	bool CriticalSection::Trylock()
	{
		return TryEnterCriticalSection(&m_Data) == TRUE;
	}

	void CriticalSection::Unlock()
	{
		LeaveCriticalSection(&m_Data);
	}

	struct ThreadInitData
	{
		ThreadEntry entryFunc;
		void *userData;
	};

	static DWORD __stdcall sThreadInit(void *init)
	{
		ThreadInitData *data = (ThreadInitData *)init;

		// delete before going into entry function so lifetime is limited.
		ThreadInitData local = *data;
		delete data;

		local.entryFunc(local.userData);

		return 0;
	}

	ThreadHandle CreateThread(ThreadEntry entryFunc, void *userData)
	{
		ThreadInitData *initData = new ThreadInitData;
		initData->entryFunc = entryFunc;
		initData->userData = userData;

		HANDLE h = ::CreateThread(NULL, 0, &sThreadInit, (void *)initData, 0, NULL);

		return (ThreadHandle)h;
	}

	uint64_t GetCurrentID()
	{
		return (uint64_t)::GetCurrentThreadId();
	}

	void JoinThread(ThreadHandle handle)
	{
		if(handle == 0) return;
		WaitForSingleObject((HANDLE)handle, INFINITE);
	}

	void CloseThread(ThreadHandle handle)
	{
		if(handle == 0) return;
		CloseHandle((HANDLE)handle);
	}

	static HMODULE ownModuleHandle = 0;

	void KeepModuleAlive()
	{
		// deliberately omitting GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT to bump refcount
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
											 (const char *)&ownModuleHandle,
											 &ownModuleHandle);
	}

	void ReleaseModuleExitThread()
	{
		FreeLibraryAndExitThread(ownModuleHandle, 0);
	}
	
	void Sleep(uint32_t milliseconds)
	{
		::Sleep((DWORD)milliseconds);
	}
};
