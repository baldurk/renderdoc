/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include <time.h>
#include "os/os_specific.h"

double Timing::GetTickFrequency()
{
  LARGE_INTEGER li;
  QueryPerformanceFrequency(&li);
  return double(li.QuadPart) / 1000.0;
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

time_t Timing::GetUTCTime()
{
  return time(NULL);
}

namespace Atomic
{
int32_t Inc32(volatile int32_t *i)
{
  return (int32_t)InterlockedIncrement((volatile LONG *)i);
}

int32_t Dec32(volatile int32_t *i)
{
  return (int32_t)InterlockedDecrement((volatile LONG *)i);
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

int32_t CmpExch32(volatile int32_t *dest, int32_t oldVal, int32_t newVal)
{
  return (int32_t)InterlockedCompareExchange((volatile LONG *)dest, newVal, oldVal);
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
  return TryEnterCriticalSection(&m_Data) != FALSE;
}

void CriticalSection::Unlock()
{
  LeaveCriticalSection(&m_Data);
}

RWLock::RWLockTemplate()
{
  InitializeSRWLock(&m_Data);
}

RWLock::~RWLockTemplate()
{
}

void RWLock::WriteLock()
{
  AcquireSRWLockExclusive(&m_Data);
}

bool RWLock::TryWritelock()
{
  return TryAcquireSRWLockExclusive(&m_Data) != FALSE;
}

void RWLock::WriteUnlock()
{
  ReleaseSRWLockExclusive(&m_Data);
}

void RWLock::ReadLock()
{
  AcquireSRWLockShared(&m_Data);
}

bool RWLock::TryReadlock()
{
  return TryAcquireSRWLockShared(&m_Data) != FALSE;
}

void RWLock::ReadUnlock()
{
  ReleaseSRWLockShared(&m_Data);
}

struct ThreadInitData
{
  std::function<void()> entryFunc;
};

static DWORD __stdcall sThreadInit(void *init)
{
  ThreadInitData *data = (ThreadInitData *)init;

  // delete before going into entry function so lifetime is limited.
  ThreadInitData local = *data;
  delete data;

  local.entryFunc();

  return 0;
}

// to not exhaust OS slots, we only allocate one that points
// to our own array
DWORD OSTLSHandle;
int64_t nextTLSSlot = 0;

struct TLSData
{
  std::vector<void *> data;
};

static CriticalSection *m_TLSListLock = NULL;
static std::vector<TLSData *> *m_TLSList = NULL;

void Init()
{
  OSTLSHandle = TlsAlloc();
  if(OSTLSHandle == TLS_OUT_OF_INDEXES)
    RDCFATAL("Can't allocate OS TLS slot");

  m_TLSListLock = new CriticalSection();
  m_TLSList = new std::vector<TLSData *>();
}

void Shutdown()
{
  if(m_TLSList)
  {
    for(size_t i = 0; i < m_TLSList->size(); i++)
      delete m_TLSList->at(i);
  }

  delete m_TLSList;
  delete m_TLSListLock;

  TlsFree(OSTLSHandle);
}

// allocate a TLS slot in our per-thread vectors with an atomic increment.
// Note this is going to be 1-indexed because Inc64 returns the post-increment
// value
uint64_t AllocateTLSSlot()
{
  return Atomic::Inc64(&nextTLSSlot);
}

// look up our per-thread vector.
void *GetTLSValue(uint64_t slot)
{
  TLSData *slots = (TLSData *)TlsGetValue(OSTLSHandle);
  if(slots == NULL || slot - 1 >= slots->data.size())
    return NULL;
  return slots->data[(size_t)slot - 1];
}

void SetTLSValue(uint64_t slot, void *value)
{
  TLSData *slots = (TLSData *)TlsGetValue(OSTLSHandle);

  // resize or allocate slot data if needed.
  // We don't need to lock this, as it is by definition thread local so we are
  // blocking on the only possible concurrent access.
  if(slots == NULL || slot - 1 >= slots->data.size())
  {
    if(slots == NULL)
    {
      slots = new TLSData;
      TlsSetValue(OSTLSHandle, slots);

      // in the case where this thread is entirely new, we globally lock so we can
      // store its data for shutdown (as we might not get notified of every thread
      // that exits). This only happens once, so we take the hit of the lock.
      m_TLSListLock->Lock();
      m_TLSList->push_back(slots);
      m_TLSListLock->Unlock();
    }

    if(slot - 1 >= slots->data.size())
    slots->data.resize((size_t)slot);
  }

  slots->data[(size_t)slot - 1] = value;
}

ThreadHandle CreateThread(std::function<void()> entryFunc)
{
  ThreadInitData *initData = new ThreadInitData;
  initData->entryFunc = entryFunc;

  HANDLE h = ::CreateThread(NULL, 0, &sThreadInit, (void *)initData, 0, NULL);

  return (ThreadHandle)h;
}

uint64_t GetCurrentID()
{
  return (uint64_t)::GetCurrentThreadId();
}

void JoinThread(ThreadHandle handle)
{
  if(handle == 0)
    return;
  WaitForSingleObject((HANDLE)handle, INFINITE);
}

void DetachThread(ThreadHandle handle)
{
  if(handle == 0)
    return;
  CloseHandle((HANDLE)handle);
}

void CloseThread(ThreadHandle handle)
{
  if(handle == 0)
    return;
  CloseHandle((HANDLE)handle);
}

static HMODULE ownModuleHandle = 0;

void KeepModuleAlive()
{
  // deliberately omitting GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT to bump refcount
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (const char *)&ownModuleHandle,
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
