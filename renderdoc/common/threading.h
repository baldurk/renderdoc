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

#pragma once

#include "os/os_specific.h"

namespace Threading
{
class ScopedLock
{
public:
  ScopedLock(CriticalSection *cs) : m_CS(cs)
  {
    if(m_CS)
      m_CS->Lock();
  }
  ~ScopedLock()
  {
    if(m_CS)
      m_CS->Unlock();
  }

private:
  CriticalSection *m_CS;
};

class ScopedReadLock
{
public:
  ScopedReadLock(RWLock &rw) : m_RW(&rw) { m_RW->ReadLock(); }
  ~ScopedReadLock() { m_RW->ReadUnlock(); }
private:
  RWLock *m_RW;
};

class ScopedWriteLock
{
public:
  ScopedWriteLock(RWLock &rw) : m_RW(&rw) { m_RW->WriteLock(); }
  ~ScopedWriteLock() { m_RW->WriteUnlock(); }
private:
  RWLock *m_RW;
};

class SpinLock
{
public:
  void Lock()
  {
    while(!Trylock())
    {
      // spin!
    }
  }
  bool Trylock() { return Atomic::CmpExch32(&val, 0, 1) == 0; }
  void Unlock() { Atomic::CmpExch32(&val, 1, 0); }
private:
  volatile int32_t val = 0;
};

class ScopedSpinLock
{
public:
  ScopedSpinLock(SpinLock &spin) : m_Spin(&spin) { m_Spin->Lock(); }
  ~ScopedSpinLock() { m_Spin->Unlock(); }
private:
  SpinLock *m_Spin;
};
};

#define SCOPED_LOCK(cs) Threading::ScopedLock CONCAT(scopedlock, __LINE__)(&cs);
#define SCOPED_LOCK_OPTIONAL(cs, cond) \
  Threading::ScopedLock CONCAT(scopedlock, __LINE__)(cond ? &cs : NULL);

#define SCOPED_READLOCK(rw) Threading::ScopedReadLock CONCAT(scopedlock, __LINE__)(rw);
#define SCOPED_WRITELOCK(rw) Threading::ScopedWriteLock CONCAT(scopedlock, __LINE__)(rw);

#define SCOPED_SPINLOCK(cs) Threading::SpinLock CONCAT(scopedlock, __LINE__)(cs);
