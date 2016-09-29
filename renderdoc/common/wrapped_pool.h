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

#pragma once

#include <stdint.h>
#include <string.h>
#include "common.h"
#include "threading.h"

#if !defined(_RELEASE)
#define INCLUDE_TYPE_NAMES
#endif

#ifdef INCLUDE_TYPE_NAMES
template <class T>
class GetTypeName
{
public:
  static const char *Name();
};
#endif

template <class C>
class FriendMaker
{
public:
  typedef C Type;
};

// allocate each class in its own pool so we can identify the type by the pointer
template <typename WrapType, int PoolCount = 8192, int MaxPoolByteSize = 1024 * 1024, bool DebugClear = true>
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

// warn when we need to allocate an additional pool
#ifdef INCLUDE_TYPE_NAMES
    RDCWARN("Ran out of free slots in %s pool!", GetTypeName<WrapType>::Name());
#else
    RDCWARN("Ran out of free slots in pool 0x%p!", &m_ImmediatePool.items[0]);
#endif

    // allocate a new additional pool and use that to allocate from
    m_AdditionalPools.push_back(new ItemPool());

#ifdef INCLUDE_TYPE_NAMES
    RDCDEBUG("WrappingPool[%d]<%s>: %p -> %p", (uint32_t)m_AdditionalPools.size() - 1,
             GetTypeName<WrapType>::Name(), &m_AdditionalPools.back()->items[0],
             &m_AdditionalPools.back()->items[AllocCount - 1]);
#endif

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
#ifdef INCLUDE_TYPE_NAMES
    RDCERR("Resource being deleted through wrong pool - 0x%p not a member of %s", p,
           GetTypeName<WrapType>::Name());
#else
    RDCERR("Resource being deleted through wrong pool - 0x%p not a member of 0x%p", p,
           &m_ImmediatePool.items[0]);
#endif
  }

  static const size_t AllocCount = PoolCount;
  static const size_t AllocMaxByteSize = MaxPoolByteSize;
  static const size_t AllocByteSize;

private:
  WrappingPool()
  {
#ifdef INCLUDE_TYPE_NAMES
    // hack - print in kB because float printing relies on statics that might not be initialised
    // yet in loading order. Ugly :(
    RDCDEBUG("WrappingPool<%s> %d in %dkB: %p -> %p", GetTypeName<WrapType>::Name(), PoolCount,
             (PoolCount * AllocByteSize) / 1024, &m_ImmediatePool.items[0],
             &m_ImmediatePool.items[AllocCount - 1]);
#endif
  }
  ~WrappingPool()
  {
    for(size_t i = 0; i < m_AdditionalPools.size(); i++)
      delete m_AdditionalPools[i];

    m_AdditionalPools.clear();
  }

  Threading::CriticalSection m_Lock;

  struct ItemPool
  {
    ItemPool()
    {
      lastAllocIdx = 0;
      RDCEraseEl(allocated);

      items = (WrapType *)(new uint8_t[AllocCount * AllocByteSize]);
    }
    ~ItemPool() { delete[](uint8_t *) items; }
    void *Allocate()
    {
      int lastAlloc = lastAllocIdx;

      if(allocated[lastAlloc])
      {
        int end = lastAlloc;

        do
        {
          lastAlloc = (lastAlloc + 1) % PoolCount;
        } while(allocated[lastAlloc] && lastAlloc != end);

        if(allocated[lastAlloc])
        {
          return NULL;
        }
      }

      void *ret = (void *)&items[lastAlloc];
      allocated[lastAlloc] = true;

#if !defined(RELEASE)
      memset(ret, 0xb0, AllocByteSize);
#endif

      lastAllocIdx = lastAlloc;

      return ret;
    }

    void Deallocate(void *p)
    {
      RDCASSERT(IsAlloc(p));

#if !defined(RELEASE)
      if(!IsAlloc(p))
      {
        RDCERR("Resource being deleted through wrong pool - 0x%p not a memory of 0x%p", p, &items[0]);
        return;
      }
#endif

      size_t idx = (WrapType *)p - &items[0];

      allocated[idx] = false;

#if !defined(RELEASE)
      memset(p, 0xfe, DebugClear ? AllocByteSize : 0);
#endif
    }

    bool IsAlloc(const void *p) const { return p >= &items[0] && p < &items[PoolCount]; }
    WrapType *items;

    // could possibly make this uint32s and check via bitmasks, but
    // we'll see if it shows up in profiling
    bool allocated[PoolCount];

    // store the last allocations index. Good place to start from and we
    // go through the pool in a ring. Good performance when the pool is empty
    // or contiguously allocated, poorer performance when the pool gets filled up.
    // It also has the bonus of handling repeated new/free well by reallocating the
    // same element.
    int lastAllocIdx;
  };

  ItemPool m_ImmediatePool;
  std::vector<ItemPool *> m_AdditionalPools;

  friend typename FriendMaker<WrapType>::Type;
};

#define ALLOCATE_WITH_WRAPPED_POOL(...)                       \
  typedef WrappingPool<__VA_ARGS__> PoolType;                 \
  static PoolType m_Pool;                                     \
  void *operator new(size_t sz) { return m_Pool.Allocate(); } \
  void operator delete(void *p) { m_Pool.Deallocate(p); }     \
  static bool IsAlloc(void *p) { return m_Pool.IsAlloc(p); }
#ifdef INCLUDE_TYPE_NAMES
#define DECL_TYPENAME(a)             \
  template <>                        \
  const char *GetTypeName<a>::Name() \
  {                                  \
    return #a;                       \
  }
#else
#define DECL_TYPENAME(a)
#endif

#define WRAPPED_POOL_INST(a)                                                              \
  a::PoolType a::m_Pool;                                                                  \
  template <>                                                                             \
  const size_t a::PoolType::AllocByteSize = sizeof(a);                                    \
  RDCCOMPILE_ASSERT(a::PoolType::AllocCount * sizeof(a) <= a::PoolType::AllocMaxByteSize, \
                    "Pool is bigger than max pool size cap");                             \
  RDCCOMPILE_ASSERT(a::PoolType::AllocCount > 2,                                          \
                    "Pool isn't greater than 2 in size. Bad parameters?");                \
  DECL_TYPENAME(a);
