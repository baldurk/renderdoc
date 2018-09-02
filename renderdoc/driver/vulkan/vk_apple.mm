#import <Cocoa/Cocoa.h>

extern "C" int getCALayerWidth(void *handle)
{
  CALayer *layer = (CALayer *)handle;
  assert([layer isKindOfClass:[CALayer class]]);

  return layer.bounds.size.width;
}

extern "C" int getCALayerHeight(void *handle)
{
  CALayer *layer = (CALayer *)handle;
  assert([layer isKindOfClass:[CALayer class]]);

  return layer.bounds.size.height;
}