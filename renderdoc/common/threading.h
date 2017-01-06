/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
  ScopedLock(CriticalSection &cs) : m_CS(&cs) { m_CS->Lock(); }
  ~ScopedLock() { m_CS->Unlock(); }
private:
  CriticalSection *m_CS;
};

class TryScopedLock
{
public:
  TryScopedLock(CriticalSection &cs) : m_CS(&cs) { m_Owned = m_CS->Trylock(); }
  ~TryScopedLock()
  {
    if(m_Owned)
      m_CS->Unlock();
  }

  bool HasLock() const { return m_Owned; }
private:
  CriticalSection *m_CS;
  bool m_Owned;
};
};

#define SCOPED_LOCK(cs) Threading::ScopedLock CONCAT(scopedlock, __LINE__)(cs);
