/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#define CHECKER_UBO

#include "glsl_ubos.h"

// for GLES compatibility where we must match blit.vert
IO_LOCATION(0) in vec2 uv;

IO_LOCATION(0) out vec4 color_out;

void main(void)
{
  vec2 RectRelativePos = gl_FragCoord.xy - checker.RectPosition;

  // if we have a border, and our pos is inside the border, return inner color
  if(checker.BorderWidth >= 0.0f)
  {
    if(RectRelativePos.x >= checker.BorderWidth &&
       RectRelativePos.x <= checker.RectSize.x - checker.BorderWidth &&
       RectRelativePos.y >= checker.BorderWidth &&
       RectRelativePos.y <= checker.RectSize.y - checker.BorderWidth)
    {
      color_out = checker.InnerColor;
      return;
    }
  }

  vec2 ab = mod(RectRelativePos.xy, vec2(checker.CheckerSquareDimension * 2.0f));

  bool checkerVariant =
      ((ab.x < checker.CheckerSquareDimension && ab.y < checker.CheckerSquareDimension) ||
       (ab.x > checker.CheckerSquareDimension && ab.y > checker.CheckerSquareDimension));

  // otherwise return checker pattern
  color_out = checkerVariant ? checker.PrimaryColor : checker.SecondaryColor;
}
