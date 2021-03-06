#import <Cocoa/Cocoa.h>

void getMetalLayerSize(void *viewHandle, void* layerHandle, int& width, int& height)
{
  NSView *view = (NSView *)viewHandle;
  assert([view isKindOfClass:[NSView class]]);
  CALayer *layer = (CALayer *)layerHandle;
  assert([layer isKindOfClass:[CALayer class]]);

  CGSize viewScale = [view convertSizeToBacking:layer.bounds.size];
  width = viewScale.width;
  height = viewScale.height;
}
