#import <Cocoa/Cocoa.h>

extern "C" void NSGL_LogText(const char *text);

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

extern "C" int NSGL_getLayerWidth(void *layer)
{
  CALayer *caLayer = (CALayer *)layer;
  assert([caLayer isKindOfClass:[CALayer class]]);

  return caLayer.bounds.size.width;
}

extern "C" int NSGL_getLayerHeight(void *layer)
{
  CALayer *caLayer = (CALayer *)layer;
  assert([caLayer isKindOfClass:[CALayer class]]);

  return caLayer.bounds.size.height;
}

extern "C" void *NSGL_createContext(void *view, void *shareContext)
{
  NSView *nsView = (NSView *)view;
  assert(nsView == nil || [nsView isKindOfClass:[NSView class]]);

  NSOpenGLContext *share = (NSOpenGLContext *)shareContext;
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

  [context setView:nsView];
  [context update];

  return context;
}

extern "C" void NSGL_makeCurrentContext(void *context)
{
  NSOpenGLContext *nsglContext = (NSOpenGLContext *)context;
  assert([nsglContext isKindOfClass:[NSOpenGLContext class]]);

  [nsglContext makeCurrentContext];
}

extern "C" void NSGL_update(void *context)
{
  NSOpenGLContext *nsglContext = (NSOpenGLContext *)context;
  assert([nsglContext isKindOfClass:[NSOpenGLContext class]]);

  [nsglContext update];
}

extern "C" void NSGL_flushBuffer(void *context)
{
  NSOpenGLContext *nsglContext = (NSOpenGLContext *)context;
  assert([nsglContext isKindOfClass:[NSOpenGLContext class]]);

  [nsglContext flushBuffer];
}

extern "C" void NSGL_destroyContext(void *context)
{
  @autoreleasepool
  {
    NSOpenGLContext *nsglContext = (NSOpenGLContext *)context;
    assert([nsglContext isKindOfClass:[NSOpenGLContext class]]);

    [nsglContext makeCurrentContext];
    [nsglContext clearDrawable];
    [nsglContext update];
    [nsglContext release];
  }
}
