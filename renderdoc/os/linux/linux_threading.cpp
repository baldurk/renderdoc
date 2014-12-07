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
#include <unistd.h>

double Timing::GetTickFrequency()
{
	return 1000000.0;
}

uint64_t Timing::GetTick()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return uint64_t(ts.tv_sec)*1000000000ULL + uint32_t(ts.tv_nsec & 0xffffffff);
}

uint64_t Timing::GetUnixTimestamp()
{
	return (uint64_t)time(NULL);
}

namespace Atomic
{
	int32_t Inc32(volatile int32_t *i)
	{
		return __sync_add_and_fetch(i, int32_t(1));
	}

	int64_t Inc64(volatile int64_t *i)
	{
		return __sync_add_and_fetch(i, int64_t(1));
	}

	int64_t Dec64(volatile int64_t *i)
	{
		return __sync_add_and_fetch(i, int64_t(-1));
	}
	
	int64_t ExchAdd64(volatile int64_t *i, int64_t a)
	{
		return __sync_add_and_fetch(i, int64_t(a));
	}
};

namespace Threading
{
	template<>
	CriticalSection::CriticalSectionTemplate()
	{
		pthread_mutexattr_init(&m_Data.attr);
		pthread_mutexattr_settype(&m_Data.attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&m_Data.lock, &m_Data.attr);
	}
	
	template<>
	CriticalSection::~CriticalSectionTemplate()
	{
		pthread_mutex_destroy(&m_Data.lock);
		pthread_mutexattr_destroy(&m_Data.attr);
	}
	
	template<>
	void CriticalSection::Lock()
	{
		pthread_mutex_lock(&m_Data.lock);
	}
	
	template<>
	bool CriticalSection::Trylock()
	{
		return pthread_mutex_trylock(&m_Data.lock) == 0;
	}
	
	template<>
	void CriticalSection::Unlock()
	{
		pthread_mutex_unlock(&m_Data.lock);
	}

	struct ThreadInitData
	{
		ThreadEntry entryFunc;
		void *userData;
	};

	static void *sThreadInit(void *init)
	{
		ThreadInitData *data = (ThreadInitData *)init;

		// delete before going into entry function so lifetime is limited.
		ThreadInitData local = *data;
		delete data;

		local.entryFunc(local.userData);

		return NULL;
	}
	
	ThreadHandle CreateThread(ThreadEntry entryFunc, void *userData)
	{
		pthread_t thread;

		ThreadInitData *initData = new ThreadInitData();
		initData->entryFunc = entryFunc;
		initData->userData = userData;

		int res = pthread_create(&thread, NULL, sThreadInit, (void *)initData);
		if(res != 0)
		{
			delete initData;
			return (ThreadHandle)0;
		}

		return (ThreadHandle)thread;
	}

	uint64_t GetCurrentID()
	{
		return (uint64_t)pthread_self();
	}

	void JoinThread(ThreadHandle handle)
	{
		pthread_join((pthread_t)handle, NULL);
	}
	void CloseThread(ThreadHandle handle)
	{
	}
	
	void KeepModuleAlive()
	{
	}

	void ReleaseModuleExitThread()
	{
	}
	
	void Sleep(uint32_t milliseconds)
	{
		usleep(milliseconds*1000);
	}
};
