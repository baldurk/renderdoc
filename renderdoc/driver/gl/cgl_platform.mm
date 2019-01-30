#import <Cocoa/Cocoa.h>

extern "C" void RENDERDOC_LogText(const char *text);

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

extern "C" int NSGL_getLayerWidth(void *handle)
{
  CALayer *layer = (CALayer *)handle;
  assert([layer isKindOfClass:[CALayer class]]);

  return layer.bounds.size.width;
}

extern "C" int NSGL_getLayerHeight(void *handle)
{
  CALayer *layer = (CALayer *)handle;
  assert([layer isKindOfClass:[CALayer class]]);

  return layer.bounds.size.height;
}

extern "C" void *NSGL_createContext(void *handle, void *sharehandle)
{
  NSView *view = (NSView *)handle;
  assert(view == nil || [view isKindOfClass:[NSView class]]);

  NSOpenGLContext *share = (NSOpenGLContext *)sharehandle;
  assert(share == nil || [share isKindOfClass:[NSOpenGLContext class]]);

  NSOpenGLPixelFormatAttribute attr[] = {
      NSOpenGLPFANoRecovery,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAAccelerated,
      NSOpenGLPFAAllowOfflineRenderers,

      NSOpenGLPFAOpenGLProfile,
      NSOpenGLProfileVersion4_1Core,

      NSOpenGLPFAColorSize,
      32,

      0,
  };

  NSOpenGLPixelFormat *pix = [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];

  if(pix == nil)
  {
    RENDERDOC_LogText("Failed to create NSOpenGLPixelFormat");
    return nil;
  }

  NSOpenGLContext *context = [[NSOpenGLContext alloc] initWithFormat:pix shareContext:share];
  [pix release];

  if(context == nil)
  {
    RENDERDOC_LogText("Failed to create NSOpenGLContext");
    return nil;
  }

  GLint aboveWindow = 1;
  [context setValues:&aboveWindow forParameter:NSOpenGLCPSurfaceOrder];

  [context setView:view];
  [context update];

  return context;
}

extern "C" void NSGL_makeCurrentContext(void *handle)
{
  NSOpenGLContext *context = (NSOpenGLContext *)handle;
  assert([context isKindOfClass:[NSOpenGLContext class]]);

  [context makeCurrentContext];
}

extern "C" void NSGL_update(void *handle)
{
  NSOpenGLContext *context = (NSOpenGLContext *)handle;
  assert([context isKindOfClass:[NSOpenGLContext class]]);

  [context update];
}

extern "C" void NSGL_flushBuffer(void *handle)
{
  NSOpenGLContext *context = (NSOpenGLContext *)handle;
  assert([context isKindOfClass:[NSOpenGLContext class]]);

  [context flushBuffer];
}

extern "C" void NSGL_destroyContext(void *handle)
{
  @autoreleasepool
  {
    NSOpenGLContext *context = (NSOpenGLContext *)handle;
    assert([context isKindOfClass:[NSOpenGLContext class]]);

    [context makeCurrentContext];
    [context clearDrawable];
    [context update];
    [context release];
  }
}