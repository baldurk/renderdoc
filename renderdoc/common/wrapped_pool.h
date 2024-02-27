/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <stdint.h>
#include <string.h>
#include "api/replay/stringise.h"
#include "common.h"
#include "threading.h"

template <class C>
class FriendMaker
{
public:
  typedef C Type;
};

// allocate each class in its own pool so we can identify the type by the pointer
template <typename WrapType, bool DebugClear = true>
class WrappingPool
{
public:
  void *Allocate()
  {
    SCOPED_LOCK(m_Lock);

    // try and allocate from immediate pool
    void *ret = m_ImmediatePool.Allocate();
    if(ret != NULL)
      return ret;

    // fall back to additional pools, if there are any
    for(size_t i = 0; i < m_AdditionalPools.size(); i++)
    {
      ret = m_AdditionalPools[i]->Allocate();
      if(ret != NULL)
        return ret;
    }

    // allocate a new additional pool and use that to allocate from
    m_AdditionalPools.push_back(new ItemPool(m_AdditionalPools.size() + 1));

    return m_AdditionalPools.back()->Allocate();
  }

  bool IsAlloc(const void *p)
  {
    // we can check the immediate pool without locking
    if(m_ImmediatePool.IsAlloc(p))
      return true;

    // if we have additional pools, lock and check them.
    // TODO: Check for additional pools in a lock-free manner,
    // to prevent the cost of locking if there are no more pools.
    {
      SCOPED_LOCK(m_Lock);

      for(size_t i = 0; i < m_AdditionalPools.size(); i++)
        if(m_AdditionalPools[i]->IsAlloc(p))
          return true;
    }

    return false;
  }

  void Deallocate(void *p)
  {
    if(p == NULL)
      return;

    SCOPED_LOCK(m_Lock);

    // try immediate pool
    if(m_ImmediatePool.IsAlloc(p))
    {
      m_ImmediatePool.Deallocate(p);
      return;
    }
    else if(!m_AdditionalPools.empty())
    {
      // fall back and try additional pools
      for(size_t i = 0; i < m_AdditionalPools.size(); i++)
      {
        if(m_AdditionalPools[i]->IsAlloc(p))
        {
          m_AdditionalPools[i]->Deallocate(p);
          return;
        }
      }
    }

    // this is an error - deleting an object that we don't recognise
    RDCERR("Resource being deleted through wrong pool - 0x%p not a member of this pool", p);
  }

  static const size_t AllocByteSize;

private:
  WrappingPool() : m_ImmediatePool(0) {}
  ~WrappingPool()
  {
    for(size_t i = 0; i < m_AdditionalPools.size(); i++)
      delete m_AdditionalPools[i];

    m_AdditionalPools.clear();
  }

  Threading::CriticalSection m_Lock;

  struct ItemPool
  {
    ItemPool(size_t poolIndex)
    {
      const size_t itemSize = sizeof(WrapType);

      size_t size;
      // first immediate pool is small - 1kB or enough for 4 objects (for every large objects like
      // devices/queues where we don't expect many)
      if(poolIndex == 0)
      {
        size = RDCMAX(itemSize * 4, (size_t)1024);
      }
      else if(poolIndex == 1)
      {
        // second pool is larger at 16kB, but still could be spillover from a very small immediate
        // pool.
        size = 16 * 1024;
      }
      else
      {
        // after that we jump up but don't get too crazy, allocate 512kB at a time
        size = 512 * 1024;
      }

      count = size / itemSize;

      items = (WrapType *)(new uint8_t[count * itemSize]);
      freeStack = new int[count];
      for(int i = 0; i < (int)count; ++i)
      {
        freeStack[i] = i;
      }
      freeStackHead = count;
    }
    ~ItemPool()
    {
      delete[](uint8_t *)items;
      delete[] freeStack;
    }
    void *Allocate()
    {
      if(freeStackHead == 0)
      {
        return NULL;
      }
      --freeStackHead;
      void *ret = items + freeStack[freeStackHead];

#if ENABLED(RDOC_DEVEL)
      const size_t itemSize = sizeof(WrapType);
      memset(ret, 0xb0, itemSize);
#endif

      return ret;
    }

    void Deallocate(void *p)
    {
      int idx = (int)((WrapType *)p - &items[0]);

      freeStack[freeStackHead] = idx;
      ++freeStackHead;

#if ENABLED(RDOC_DEVEL)
      const size_t itemSize = sizeof(WrapType);
      if(DebugClear)
        memset(p, 0xfe, itemSize);
#endif
    }

    bool IsAlloc(const void *p) const { return p >= &items[0] && p < &items[count]; }
    WrapType *items;
    size_t count;
    int *freeStack;
    size_t freeStackHead;
  };

  ItemPool m_ImmediatePool;
  rdcarray<ItemPool *> m_AdditionalPools;

  friend typename FriendMaker<WrapType>::Type;
};

#define ALLOCATE_WITH_WRAPPED_POOL(...)       \
  typedef WrappingPool<__VA_ARGS__> PoolType; \
  static PoolType m_Pool;                     \
  void *operator new(size_t sz)               \
  {                                           \
    return m_Pool.Allocate();                 \
  }                                           \
  void operator delete(void *p)               \
  {                                           \
    m_Pool.Deallocate(p);                     \
  }                                           \
  static bool IsAlloc(const void *p)          \
  {                                           \
    return m_Pool.IsAlloc(p);                 \
  }
#define WRAPPED_POOL_INST(a) \
  a::PoolType a::m_Pool;     \
  DECLARE_STRINGISE_TYPE(a);
