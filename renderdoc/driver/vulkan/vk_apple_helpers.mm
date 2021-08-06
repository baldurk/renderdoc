#import <Cocoa/Cocoa.h>

void getMetalLayerSize(void *layerHandle, int &width, int &height)
{
  CALayer *layer = (CALayer *)layerHandle;
  assert([layer isKindOfClass:[CALayer class]]);

  const CGFloat scaleFactor = layer.contentsScale;
  width = layer.bounds.size.width * scaleFactor;
  height = layer.bounds.size.height * scaleFactor;
}
