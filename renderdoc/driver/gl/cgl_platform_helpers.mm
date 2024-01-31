/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "api/replay/rdcstr.h"
#include "common/common.h"
#include "common/threading.h"
#include "os/os_specific.h"

#import <Cocoa/Cocoa.h>

#define RD_THREAD_RANDOM_SLEEP (0)
#define RD_USE_CONTEXT_LOCK_COUNTS (0)

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static uint64_t s_currentContextTLSkey;

static Threading::CriticalSection s_WindowPtrsArrayLock;
static rdcarray<NSView *> s_WindowPtrs;
static rdcarray<int> s_WindowWidths;
static rdcarray<int> s_WindowHeights;
static int s_ReplaySalt = 0;

static Threading::CriticalSection s_ContextPtrsArrayLock;
static rdcarray<NSOpenGLContext *> s_ContextPtrs;
static rdcarray<Threading::CriticalSection *> s_ContextLocks;
#if RD_USE_CONTEXT_LOCK_COUNTS
static rdcarray<int32_t> s_ContextLocksCount;
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS

static void scheduleContextSetView(int contextIndex, int replaySalt, NSView *view);
static void scheduleContextUpdate(int contextIndex, int replaySalt);

static void RandomSleep()
{
#if RD_THREAD_RANDOM_SLEEP
  usleep(rand() % 1000);
#endif    // #if RD_THREAD_RANDOM_SLEEP
}

static NSOpenGLContext *GetNSOpenGLContext(void *context)
{
  NSOpenGLContext *nsglContext = (NSOpenGLContext *)context;
  RDCASSERT([nsglContext isKindOfClass:[NSOpenGLContext class]]);
  return nsglContext;
}

static NSView *GetNSView(void *view)
{
  NSView *nsView = (NSView *)view;
  RDCASSERT([nsView isKindOfClass:[NSView class]]);
  return nsView;
}

static void IncrementContextLockCount(int contextIndex)
{
#if RD_USE_CONTEXT_LOCK_COUNTS
  Atomic::Inc32(s_ContextLocksCount.data() + contextIndex);
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
}

static void DecrementContextLockCount(int contextIndex)
{
#if RD_USE_CONTEXT_LOCK_COUNTS
  Atomic::Dec32(s_ContextLocksCount.data() + contextIndex);
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
}

static void LockContext(int contextIndex)
{
  s_ContextLocks[contextIndex]->Lock();
  IncrementContextLockCount(contextIndex);
}

static bool TryLockContext(int contextIndex)
{
  if(s_ContextLocks[contextIndex]->Trylock())
  {
    IncrementContextLockCount(contextIndex);
    return true;
  }
  return false;
}

static void UnLockContext(int contextIndex)
{
  DecrementContextLockCount(contextIndex);
  s_ContextLocks[contextIndex]->Unlock();
}

// s_WindowPtrsArrayLock must be locked by the caller
static bool findWindowIndex(NSView *nsView, int &index)
{
  const int windowCount = s_WindowPtrs.count();
  int firstFreeIndex = windowCount;
  for(int i = 0; i < windowCount; ++i)
  {
    if(s_WindowPtrs[i] == nsView)
    {
      index = i;
      return true;
    }
    if((s_WindowPtrs[i] == nil) && (i < firstFreeIndex))
    {
      firstFreeIndex = i;
    }
  }
  index = firstFreeIndex;
  return false;
}

static int getWindowIndex(NSView *nsView)
{
  SCOPED_LOCK(s_WindowPtrsArrayLock);
  int index = -1;
  if(findWindowIndex(nsView, index))
  {
    return index;
  }
  const int windowCount = s_WindowPtrs.count();
  if(index < windowCount)
  {
    RDCASSERT(index >= 0);
    RDCASSERT(index < s_WindowWidths.count());
    RDCASSERT(index < s_WindowHeights.count());
    s_WindowPtrs[index] = nsView;
    s_WindowWidths[index] = 0;
    s_WindowHeights[index] = 0;
  }
  else
  {
    RDCASSERT(windowCount == s_WindowWidths.count());
    RDCASSERT(windowCount == s_WindowHeights.count());
    s_WindowPtrs.push_back(nsView);
    s_WindowWidths.push_back(0);
    s_WindowHeights.push_back(0);
  }
  return index;
}

static void SetCurrentContextIndexTLS(int contextIndex)
{
  Threading::SetTLSValue(s_currentContextTLSkey, (void *)(uintptr_t)contextIndex);
}

static int GetCurrentContextIndexTLS()
{
  return (int)(uintptr_t)Threading::GetTLSValue(s_currentContextTLSkey);
}

static int getContextIndex(NSOpenGLContext *nsglContext)
{
  SCOPED_LOCK(s_ContextPtrsArrayLock);
  const int contextCount = s_ContextPtrs.count();
  int firstFreeIndex = contextCount;
  for(int i = 0; i < contextCount; ++i)
  {
    if(s_ContextPtrs[i] == nsglContext)
    {
      return i;
    }
    if((s_ContextPtrs[i] == nil) && (i < firstFreeIndex))
    {
      firstFreeIndex = i;
    }
  }
  int index = firstFreeIndex;
  if(index < contextCount)
  {
    RDCASSERT(index >= 0);
    RDCASSERT(index < s_ContextLocks.count());
    s_ContextPtrs[index] = nsglContext;
    RDCASSERT(s_ContextLocks[index]);
#if RD_USE_CONTEXT_LOCK_COUNTS
    RDCASSERT(0 == s_ContextLocksCount[index]);
    RDCASSERT(index < s_ContextLocksCount.count());
    s_ContextLocksCount[index] = 0;
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
  }
  else
  {
    RDCASSERT(contextCount == s_ContextLocks.count());
#if RD_USE_CONTEXT_LOCK_COUNTS
    RDCASSERT(contextCount == s_ContextLocksCount.count());
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
    s_ContextPtrs.push_back(nsglContext);
    s_ContextLocks.push_back(new Threading::CriticalSection);
#if RD_USE_CONTEXT_LOCK_COUNTS
    s_ContextLocksCount.push_back(0);
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
  }
  return index;
}

static NSOpenGLContext *getLockedContext(int contextIndex)
{
  SCOPED_LOCK(s_ContextPtrsArrayLock);
  RDCASSERT(contextIndex >= 0 && contextIndex < s_ContextPtrs.count());
  if(contextIndex >= 0 && contextIndex < s_ContextPtrs.count())
  {
    NSOpenGLContext *context = s_ContextPtrs[contextIndex];
    if(context && TryLockContext(contextIndex))
    {
      return context;
    }
  }
  return nil;
}

static void viewSetWantBestResolutionMT(NSView *view)
{
  RandomSleep();
  [view setWantsBestResolutionOpenGLSurface:true];
}

static void viewGetWindowSizeMT(int windowIndex, int replaySalt)
{
  RandomSleep();
  const int currentReplaySalt = s_ReplaySalt;
  if(replaySalt != currentReplaySalt)
  {
    return;
  }
  SCOPED_LOCK(s_WindowPtrsArrayLock);
  RDCASSERT(windowIndex >= 0 && windowIndex < s_WindowPtrs.count());
  if(windowIndex >= 0 && windowIndex < s_WindowPtrs.count())
  {
    NSView *view = s_WindowPtrs[windowIndex];
    if(view)
    {
      const NSRect contentRect = [view frame];
      CGSize viewSize = [view convertSizeToBacking:contentRect.size];

      s_WindowWidths[windowIndex] = viewSize.width;
      s_WindowHeights[windowIndex] = viewSize.height;
    }
  }
}

static void contextUpdateMT(int contextIndex, int replaySalt)
{
  RandomSleep();
  const int currentReplaySalt = s_ReplaySalt;
  if(replaySalt != currentReplaySalt)
  {
    return;
  }
  NSOpenGLContext *lockedContext = getLockedContext(contextIndex);
  if(lockedContext)
  {
    [lockedContext update];
    UnLockContext(contextIndex);
  }
  else
  {
    scheduleContextUpdate(contextIndex, replaySalt);
  }
}

static void contextSetViewMT(int contextIndex, int replaySalt, NSView *view)
{
  RandomSleep();
  const int currentReplaySalt = s_ReplaySalt;
  if(replaySalt != currentReplaySalt)
  {
    return;
  }
  NSOpenGLContext *lockedContext = getLockedContext(contextIndex);
  if(lockedContext)
  {
    [lockedContext setView:view];
    [lockedContext update];
    UnLockContext(contextIndex);
  }
  else
  {
    scheduleContextSetView(contextIndex, replaySalt, view);
  }
}

static void scheduleContextSetView(int contextIndex, int replaySalt, NSView *view)
{
  RandomSleep();
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    contextSetViewMT(contextIndex, replaySalt, view);
  });
}

static void scheduleContextUpdate(int contextIndex, int replaySalt)
{
  RandomSleep();
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    contextUpdateMT(contextIndex, replaySalt);
  });
}

static void scheduleViewSetWantBestResolution(NSView *view)
{
  RandomSleep();
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    viewSetWantBestResolutionMT(view);
  });
}

static void scheduleViewGetWindowSizeMT(int windowIndex, int replaySalt)
{
  RandomSleep();
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    viewGetWindowSizeMT(windowIndex, replaySalt);
  });
}

void Apple_getWindowSize(void *view, int &width, int &height)
{
  RandomSleep();
  if(!view)
  {
    width = 0;
    height = 0;
    return;
  }
  NSView *nsView = GetNSView(view);
  const int windowIndex = getWindowIndex(nsView);
  width = s_WindowWidths[windowIndex];
  height = s_WindowHeights[windowIndex];
  scheduleViewGetWindowSizeMT(windowIndex, s_ReplaySalt);
}

void Apple_stopTrackingWindowSize(void *view)
{
  RandomSleep();
  if(!view)
    return;
  NSView *nsView = (NSView *)view;
  {
    SCOPED_LOCK(s_WindowPtrsArrayLock);
    int windowIndex = -1;
    if(findWindowIndex(nsView, windowIndex))
    {
      s_WindowPtrs[windowIndex] = nil;
      s_WindowWidths[windowIndex] = 0;
      s_WindowHeights[windowIndex] = 0;
    }
  }
}

void NSGL_init()
{
  RandomSleep();
  static bool s_allocatedTLSKey = false;
  if(!s_allocatedTLSKey)
  {
    s_ReplaySalt = 0;
    s_allocatedTLSKey = true;
    s_currentContextTLSkey = Threading::AllocateTLSSlot();
    const int initialWindowCountMax = 8;
    s_WindowPtrs.reserve(initialWindowCountMax);
    s_WindowWidths.reserve(initialWindowCountMax);
    s_WindowHeights.reserve(initialWindowCountMax);

    const int initialContextCountMax = 8;
    s_ContextPtrs.reserve(initialContextCountMax);
    s_ContextLocks.reserve(initialContextCountMax);
#if RD_USE_CONTEXT_LOCK_COUNTS
    s_ContextLocksCount.reserve(initialContextCountMax);
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
  }
  Atomic::Inc32(&s_ReplaySalt);
  SetCurrentContextIndexTLS(-1);
  {
    SCOPED_LOCK(s_WindowPtrsArrayLock);
    s_WindowPtrs.resize(0);
    s_WindowWidths.resize(0);
    s_WindowHeights.resize(0);
  }
  {
    SCOPED_LOCK(s_ContextPtrsArrayLock);
    s_ContextPtrs.resize(0);
    s_ContextLocks.resize(0);
#if RD_USE_CONTEXT_LOCK_COUNTS
    s_ContextLocksCount.resize(0);
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
  }
}

void *NSGL_createContext(void *view, void *shareContext)
{
  RandomSleep();
  NSView *nsView = (NSView *)view;
  assert(nsView == nil || [nsView isKindOfClass:[NSView class]]);

  NSOpenGLContext *share = (NSOpenGLContext *)shareContext;
  assert(share == nil || [share isKindOfClass:[NSOpenGLContext class]]);

  NSOpenGLPixelFormatAttribute attr[] = {
      NSOpenGLPFAAccelerated,
      NSOpenGLPFAClosestPolicy,
      NSOpenGLPFAOpenGLProfile,
      NSOpenGLProfileVersion4_1Core,
      NSOpenGLPFAColorSize,
      24,
      NSOpenGLPFAAlphaSize,
      8,
      NSOpenGLPFADepthSize,
      24,
      NSOpenGLPFAStencilSize,
      8,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFASampleBuffers,
      0,
      0,
  };

  NSOpenGLPixelFormat *pix = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];

  if(pix == nil)
  {
    RDCERR("CGL: Failed to create NSOpenGLPixelFormat");
    return nil;
  }

  NSOpenGLContext *context = [[NSOpenGLContext alloc] initWithFormat:pix shareContext:share];
  [pix release];

  if(context == nil)
  {
    RDCERR("CGL: Failed to create NSOpenGLContext");
    return nil;
  }

  GLint aboveWindow = 1;
  [context setValues:&aboveWindow forParameter:NSOpenGLCPSurfaceOrder];

  scheduleViewSetWantBestResolution(nsView);

  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  scheduleContextSetView(contextIndex, s_ReplaySalt, nsView);

  return context;
}

void NSGL_makeCurrentContext(void *context)
{
  RandomSleep();
  const int currentThreadContextIndex = GetCurrentContextIndexTLS();
  if(currentThreadContextIndex >= 0)
    UnLockContext(currentThreadContextIndex);

  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  SetCurrentContextIndexTLS(contextIndex);
  LockContext(contextIndex);
  [nsglContext makeCurrentContext];
  scheduleContextUpdate(contextIndex, s_ReplaySalt);
}

void NSGL_update(void *context)
{
  RandomSleep();
  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  scheduleContextUpdate(contextIndex, s_ReplaySalt);
}

void NSGL_flushBuffer(void *context)
{
  RandomSleep();
  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  LockContext(contextIndex);
  [nsglContext flushBuffer];
  UnLockContext(contextIndex);
}

void NSGL_destroyContext(void *context)
{
  RandomSleep();
  @autoreleasepool
  {
    NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
    const int contextIndex = getContextIndex(nsglContext);
    const int currentThreadContextIndex = GetCurrentContextIndexTLS();
    if(currentThreadContextIndex == contextIndex)
    {
      SetCurrentContextIndexTLS(-1);
      UnLockContext(contextIndex);
    }

    LockContext(contextIndex);
    [nsglContext makeCurrentContext];
    [nsglContext clearDrawable];
    [nsglContext update];
    [nsglContext release];
    UnLockContext(contextIndex);

    {
      SCOPED_LOCK(s_ContextPtrsArrayLock);
      s_ContextPtrs[contextIndex] = nil;
#if RD_USE_CONTEXT_LOCK_COUNTS
      RDCASSERT(0 == s_ContextLocksCount[contextIndex]);
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
    }
  }
}
