#import <Cocoa/Cocoa.h>
#include <QuartzCore/CAMetalLayer.h>

// taken from Arseny's comment explaining how to make Qt widgets metal compatible:
// https://github.com/KhronosGroup/MoltenVK/issues/78#issuecomment-369838674
extern "C" void *makeNSViewMetalCompatible(void *handle)
{
  NSView *view = (NSView *)handle;
  assert([view isKindOfClass:[NSView class]]);

  if(![view.layer isKindOfClass:[CAMetalLayer class]])
  {
    [view setLayer:[CAMetalLayer layer]];
    [view setWantsLayer:YES];
  }

  return view.layer;
}
