#include "glsl_globals.h"

IO_LOCATION(0) out vec4 color_out;

void main(void)
{
  color_out = intBitsToFloat(gl_PrimitiveID).xxxx;
}
