#import <Cocoa/Cocoa.h>

extern "C" int getMetalLayerWidth(void *handle)
{
  CALayer *layer = (CALayer *)handle;
  assert([layer isKindOfClass:[CALayer class]]);

  return layer.bounds.size.width;
}

extern "C" int getMetalLayerHeight(void *handle)
{
  CALayer *layer = (CALayer *)handle;
  assert([layer isKindOfClass:[CALayer class]]);

  return layer.bounds.size.height;
}