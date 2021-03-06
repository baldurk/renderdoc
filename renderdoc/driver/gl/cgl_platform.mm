#import <Cocoa/Cocoa.h>
#include <libkern/OSAtomic.h>
#include <pthread.h>

#define RD_THREAD_RANDOM_SLEEP (0)
#define RD_USE_CONTEXT_LOCK_COUNTS (0)

void NSGL_LogText(const char *text);

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static pthread_key_t s_currentContextTLSkey;

static int s_WindowCountMax;
static int s_WindowCount;
static NSLock *s_WindowIndexesArrayLock;
static NSView **s_WindowIndexes;
static int *s_WindowWidths;
static int *s_WindowHeights;

static int s_ContextCountMax;
static int s_ContextCount;
static NSLock *s_ContextIndexesArrayLock;
static NSOpenGLContext **s_ContextIndexes;
static NSMutableArray<NSRecursiveLock *> *s_ContextLocks;
#if RD_USE_CONTEXT_LOCK_COUNTS
static int32_t *s_ContextLocksCount;
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS

static void scheduleContextSetView(int contextIndex, NSView *view);
static void scheduleContextUpdate(int contextIndex);

static void RandomSleep()
{
#if RD_THREAD_RANDOM_SLEEP
  usleep(rand() % 2000);
#endif    // #if RD_THREAD_RANDOM_SLEEP
}

static NSOpenGLContext *GetNSOpenGLContext(void *context)
{
  NSOpenGLContext *nsglContext = (NSOpenGLContext *)context;
  assert([nsglContext isKindOfClass:[NSOpenGLContext class]]);
  return nsglContext;
}

static NSView *GetNSView(void *view)
{
  NSView *nsView = (NSView *)view;
  assert([nsView isKindOfClass:[NSView class]]);
  return nsView;
}

static void IncrementContextLockCount(int contextIndex)
{
#if RD_USE_CONTEXT_LOCK_COUNTS
  OSAtomicIncrement32Barrier(s_ContextLocksCount + contextIndex);
#endif    //#if RD_USE_CONTEXT_LOCK_COUNTS
}

static void DecrementContextLockCount(int contextIndex)
{
#if RD_USE_CONTEXT_LOCK_COUNTS
  OSAtomicDecrement32Barrier(s_ContextLocksCount + contextIndex);
#endif    //#if RD_USE_CONTEXT_LOCK_COUNTS
}

static void LockContext(int contextIndex)
{
  [s_ContextLocks[contextIndex] lock];
  IncrementContextLockCount(contextIndex);
}

static bool TryLockContext(int contextIndex)
{
  if([s_ContextLocks[contextIndex] tryLock])
  {
    IncrementContextLockCount(contextIndex);
    return true;
  }
  return false;
}

static void UnLockContext(int contextIndex)
{
  DecrementContextLockCount(contextIndex);
  [s_ContextLocks[contextIndex] unlock];
}

static void LockWindowIndexesArray()
{
  [s_WindowIndexesArrayLock lock];
}

static void UnLockWindowIndexesArray()
{
  [s_WindowIndexesArrayLock unlock];
}

static int getWindowIndex(NSView *nsView)
{
  LockWindowIndexesArray();
  int firstFreeIndex = s_WindowCount;
  for(int i = 0; i < s_WindowCount; ++i)
  {
    if(s_WindowIndexes[i] == nsView)
    {
      UnLockWindowIndexesArray();
      return i;
    }
    if((s_WindowIndexes[i] == nil) && (i < firstFreeIndex))
    {
      firstFreeIndex = i;
    }
  }
  const int index = firstFreeIndex;
  if(index >= s_WindowCountMax)
  {
    const int oldCapacity = s_WindowCountMax;
    s_WindowCountMax = s_WindowCount * 2;
    s_WindowIndexes = (NSView **)realloc(s_WindowIndexes, s_WindowCountMax * sizeof(NSView *));
    s_WindowWidths = (int *)realloc(s_WindowWidths, s_WindowCountMax * sizeof(int));
    s_WindowHeights = (int *)realloc(s_WindowHeights, s_WindowCountMax * sizeof(int));
    for(int i = oldCapacity; i < s_WindowCountMax; ++i)
    {
      s_WindowIndexes[i] = nil;
    }
  }
  assert(index >= 0 && index < s_WindowCountMax);
  s_WindowIndexes[index] = nsView;
  s_WindowWidths[index] = 0;
  s_WindowHeights[index] = 0;
  if(index == s_WindowCount)
    ++s_WindowCount;
  UnLockWindowIndexesArray();
  return index;
}

static void LockContextIndexesArray()
{
  [s_ContextIndexesArrayLock lock];
}

static void UnLockContextIndexesArray()
{
  [s_ContextIndexesArrayLock unlock];
}

static void SetCurrentContextTLS(int contextIndex)
{
  const intptr_t key = contextIndex;
  pthread_setspecific(s_currentContextTLSkey, (void *)key);
}

static int GetCurrentContextTLS()
{
  const intptr_t value = (intptr_t)pthread_getspecific(s_currentContextTLSkey);
  return value;
}

static int getContextIndex(NSOpenGLContext *nsglContext)
{
  LockContextIndexesArray();
  int firstFreeIndex = s_ContextCount;
  for(int i = 0; i < s_ContextCount; ++i)
  {
    if(s_ContextIndexes[i] == nsglContext)
    {
      UnLockContextIndexesArray();
      return i;
    }
    if((s_ContextIndexes[i] == nil) && (i < firstFreeIndex))
    {
      firstFreeIndex = i;
    }
  }
  const int index = firstFreeIndex;
  if(index >= s_ContextCountMax)
  {
    const int oldCapacity = s_ContextCountMax;
    s_ContextCountMax = s_ContextCount * 2;
    s_ContextIndexes =
        (NSOpenGLContext **)realloc(s_ContextIndexes, s_ContextCountMax * sizeof(NSOpenGLContext *));
#if RD_USE_CONTEXT_LOCK_COUNTS
    s_ContextLocksCount =
        (int32_t *)realloc(s_ContextLocksCount, s_ContextCountMax * sizeof(int32_t));
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
    for(int i = oldCapacity; i < s_ContextCountMax; ++i)
    {
      s_ContextLocks[i] = [NSRecursiveLock alloc];
      s_ContextIndexes[i] = nil;
    }
  }
  assert(index >= 0 && index < s_ContextCountMax);
  s_ContextIndexes[index] = nsglContext;
#if RD_USE_CONTEXT_LOCK_COUNTS
  s_ContextLocksCount[index] = 0;
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
  assert(s_ContextLocks[index]);
  [s_ContextLocks[index] init];
  if(index == s_ContextCount)
    ++s_ContextCount;
  UnLockContextIndexesArray();
  return index;
}

static NSOpenGLContext *getLockedContext(int contextIndex)
{
  LockContextIndexesArray();
  assert(contextIndex >= 0 && contextIndex < s_ContextCount);
  NSOpenGLContext *context = s_ContextIndexes[contextIndex];
  if(context && !TryLockContext(contextIndex))
  {
    context = nil;
  }
  UnLockContextIndexesArray();
  return context;
}

static void viewSetWantBestResolutionMT(NSView *view)
{
  [view setWantsBestResolutionOpenGLSurface:true];
}

static void viewGetWindowSizeMT(int windowIndex)
{
  RandomSleep();
  LockWindowIndexesArray();
  assert(windowIndex >= 0 && windowIndex < s_WindowCount);
  NSView *view = s_WindowIndexes[windowIndex];
  if(view)
  {
    const NSRect contentRect = [view frame];
    CGSize viewSize = [view convertSizeToBacking:contentRect.size];

    s_WindowWidths[windowIndex] = viewSize.width;
    s_WindowHeights[windowIndex] = viewSize.height;
  }
  UnLockWindowIndexesArray();
}

static void contextUpdateMT(int contextIndex)
{
  RandomSleep();
  NSOpenGLContext *lockedContext = getLockedContext(contextIndex);
  if(lockedContext)
  {
    [lockedContext update];
    UnLockContext(contextIndex);
  }
  else
  {
    scheduleContextUpdate(contextIndex);
  }
}

static void contextSetViewMT(int contextIndex, NSView *view)
{
  RandomSleep();
  NSOpenGLContext *lockedContext = getLockedContext(contextIndex);
  if(lockedContext)
  {
    [lockedContext setView:view];
    [lockedContext update];
    UnLockContext(contextIndex);
  }
  else
  {
    scheduleContextSetView(contextIndex, view);
  }
}

static void scheduleContextSetView(int contextIndex, NSView *view)
{
  RandomSleep();
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    contextSetViewMT(contextIndex, view);
  });
}

static void scheduleContextUpdate(int contextIndex)
{
  RandomSleep();
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    contextUpdateMT(contextIndex);
  });
}

static void scheduleViewSetWantBestResolution(NSView *view)
{
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    viewSetWantBestResolutionMT(view);
  });
}

static void scheduleViewGetWindowSizeMT(int windowIndex)
{
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    viewGetWindowSizeMT(windowIndex);
  });
}

void getAppleWindowGetSize(void *view, int &width, int &height)
{
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
  scheduleViewGetWindowSizeMT(windowIndex);
}

void stopTrackingWindowSize(void *view)
{
  if(!view)
    return;
  NSView *nsView = GetNSView(view);
  const int windowIndex = getWindowIndex(nsView);
  LockWindowIndexesArray();
  s_WindowIndexes[windowIndex] = nil;
  s_WindowWidths[windowIndex] = 0;
  s_WindowHeights[windowIndex] = 0;
  UnLockWindowIndexesArray();
}

void NSGL_init()
{
  const int result = pthread_key_create(&s_currentContextTLSkey, nil);
  assert(result == 0);
  SetCurrentContextTLS(-1);

  s_WindowIndexesArrayLock = [[NSLock alloc] init];
  s_WindowCountMax = 8;
  s_WindowIndexes = (NSView **)calloc(s_WindowCountMax, sizeof(NSView *));
  s_WindowWidths = (int *)calloc(s_WindowCountMax, sizeof(int));
  s_WindowHeights = (int *)calloc(s_WindowCountMax, sizeof(int));
  s_WindowCount = 0;

  s_ContextIndexesArrayLock = [[NSLock alloc] init];
  s_ContextCountMax = 8;
  s_ContextIndexes = (NSOpenGLContext **)calloc(s_ContextCountMax, sizeof(NSView *));
#if RD_USE_CONTEXT_LOCK_COUNTS
  s_ContextLocksCount = (int32_t *)calloc(s_ContextCountMax, sizeof(int32_t));
#endif    // #if RD_USE_CONTEXT_LOCK_COUNTS
  s_ContextLocks = [NSMutableArray<NSRecursiveLock *> arrayWithCapacity:s_ContextCountMax];
  for(int i = 0; i < s_ContextCountMax; ++i)
    s_ContextLocks[i] = [NSRecursiveLock alloc];
  s_ContextCount = 0;
}

void *NSGL_createContext(void *view, void *shareContext)
{
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
    NSGL_LogText("Failed to create NSOpenGLPixelFormat");
    return nil;
  }

  NSOpenGLContext *context = [[NSOpenGLContext alloc] initWithFormat:pix shareContext:share];
  [pix release];

  if(context == nil)
  {
    NSGL_LogText("Failed to create NSOpenGLContext");
    return nil;
  }

  GLint aboveWindow = 1;
  [context setValues:&aboveWindow forParameter:NSOpenGLCPSurfaceOrder];

  scheduleViewSetWantBestResolution(nsView);

  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  scheduleContextSetView(contextIndex, nsView);

  return context;
}

void NSGL_makeCurrentContext(void *context)
{
  const int currentThreadContextIndex = GetCurrentContextTLS();
  if(currentThreadContextIndex >= 0)
    UnLockContext(currentThreadContextIndex);

  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  SetCurrentContextTLS(contextIndex);
  LockContext(contextIndex);
  [nsglContext makeCurrentContext];
  scheduleContextUpdate(contextIndex);
}

void NSGL_update(void *context)
{
  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  scheduleContextUpdate(contextIndex);
}

void NSGL_flushBuffer(void *context)
{
  NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
  const int contextIndex = getContextIndex(nsglContext);
  LockContext(contextIndex);
  [nsglContext flushBuffer];
  UnLockContext(contextIndex);
}

void NSGL_destroyContext(void *context)
{
  @autoreleasepool
  {
    NSOpenGLContext *nsglContext = GetNSOpenGLContext(context);
    const int contextIndex = getContextIndex(nsglContext);
    {
      LockContext(contextIndex);
      [nsglContext makeCurrentContext];
      [nsglContext clearDrawable];
      [nsglContext update];
      [nsglContext release];
      UnLockContext(contextIndex);
    }

    LockContextIndexesArray();
    s_ContextIndexes[contextIndex] = nil;
    UnLockContextIndexesArray();
  }
}
