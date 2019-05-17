/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include <unistd.h>
#include "os/os_specific.h"

void CacheDebuggerPresent();

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
  return __sync_add_and_fetch(i, int32_t(1));
}

int32_t Dec32(volatile int32_t *i)
{
  return __sync_add_and_fetch(i, int32_t(-1));
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

int32_t CmpExch32(volatile int32_t *dest, int32_t oldVal, int32_t newVal)
{
  return __sync_val_compare_and_swap(dest, oldVal, newVal);
}
};

namespace Threading
{
template <>
CriticalSection::CriticalSectionTemplate()
{
  pthread_mutexattr_init(&m_Data.attr);
  pthread_mutexattr_settype(&m_Data.attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&m_Data.lock, &m_Data.attr);
}

template <>
CriticalSection::~CriticalSectionTemplate()
{
  pthread_mutex_destroy(&m_Data.lock);
  pthread_mutexattr_destroy(&m_Data.attr);
}

template <>
void CriticalSection::Lock()
{
  pthread_mutex_lock(&m_Data.lock);
}

template <>
bool CriticalSection::Trylock()
{
  return pthread_mutex_trylock(&m_Data.lock) == 0;
}

template <>
void CriticalSection::Unlock()
{
  pthread_mutex_unlock(&m_Data.lock);
}

template <>
RWLock::RWLockTemplate()
{
  pthread_rwlockattr_init(&m_Data.attr);
  pthread_rwlock_init(&m_Data.rwlock, &m_Data.attr);
}

template <>
RWLock::~RWLockTemplate()
{
  pthread_rwlock_destroy(&m_Data.rwlock);
  pthread_rwlockattr_destroy(&m_Data.attr);
}

template <>
void RWLock::WriteLock()
{
  pthread_rwlock_wrlock(&m_Data.rwlock);
}

template <>
bool RWLock::TryWritelock()
{
  return pthread_rwlock_trywrlock(&m_Data.rwlock) == 0;
}

template <>
void RWLock::WriteUnlock()
{
  pthread_rwlock_unlock(&m_Data.rwlock);
}

template <>
void RWLock::ReadLock()
{
  pthread_rwlock_rdlock(&m_Data.rwlock);
}

template <>
bool RWLock::TryReadlock()
{
  return pthread_rwlock_tryrdlock(&m_Data.rwlock) == 0;
}

template <>
void RWLock::ReadUnlock()
{
  pthread_rwlock_unlock(&m_Data.rwlock);
}

struct ThreadInitData
{
  std::function<void()> entryFunc;
};

static void *sThreadInit(void *init)
{
  ThreadInitData *data = (ThreadInitData *)init;

  // delete before going into entry function so lifetime is limited.
  ThreadInitData local = *data;
  delete data;

  local.entryFunc();

  return NULL;
}

// to not exhaust OS slots, we only allocate one that points
// to our own array
pthread_key_t OSTLSHandle;
int64_t nextTLSSlot = 0;

struct TLSData
{
  std::vector<void *> data;
};

static CriticalSection *m_TLSListLock = NULL;
static std::vector<TLSData *> *m_TLSList = NULL;

void Init()
{
  int err = pthread_key_create(&OSTLSHandle, NULL);
  if(err != 0)
    RDCFATAL("Can't allocate OS TLS slot");

  m_TLSListLock = new CriticalSection();
  m_TLSList = new std::vector<TLSData *>();

  CacheDebuggerPresent();
}

void Shutdown()
{
  for(size_t i = 0; i < m_TLSList->size(); i++)
    delete m_TLSList->at(i);

  delete m_TLSList;
  delete m_TLSListLock;

  pthread_key_delete(OSTLSHandle);
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
  TLSData *slots = (TLSData *)pthread_getspecific(OSTLSHandle);
  if(slots == NULL || slot - 1 >= slots->data.size())
    return NULL;
  return slots->data[(size_t)slot - 1];
}

void SetTLSValue(uint64_t slot, void *value)
{
  TLSData *slots = (TLSData *)pthread_getspecific(OSTLSHandle);

  // resize or allocate slot data if needed.
  // We don't need to lock this, as it is by definition thread local so we are
  // blocking on the only possible concurrent access.
  if(slots == NULL || slot - 1 >= slots->data.size())
  {
    if(slots == NULL)
    {
      slots = new TLSData;
      pthread_setspecific(OSTLSHandle, slots);

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
  pthread_t thread;

  ThreadInitData *initData = new ThreadInitData();
  initData->entryFunc = entryFunc;

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
void DetachThread(ThreadHandle handle)
{
  pthread_detach((pthread_t)handle);
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
  usleep(milliseconds * 1000);
}
};
