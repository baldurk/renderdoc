/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Baldur Karlsson
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

#pragma once

DOCUMENT("Information about a viewport.");
struct Viewport
{
  DOCUMENT("");
  bool operator==(const Viewport &o) const
  {
    return x == o.x && y == o.y && width == o.width && height == o.height &&
           minDepth == o.minDepth && maxDepth == o.maxDepth;
  }
  bool operator<(const Viewport &o) const
  {
    if(!(x == o.x))
      return x < o.x;
    if(!(y == o.y))
      return y < o.y;
    if(!(width == o.width))
      return width < o.width;
    if(!(height == o.height))
      return height < o.height;
    if(!(minDepth == o.minDepth))
      return minDepth < o.minDepth;
    if(!(maxDepth == o.maxDepth))
      return maxDepth < o.maxDepth;
    return false;
  }
  Viewport() = default;
  Viewport(float X, float Y, float W, float H, float MN, float MX, bool en = true)
      : x(X), y(Y), width(W), height(H), minDepth(MN), maxDepth(MX), enabled(en)
  {
  }

  DOCUMENT("Is this viewport enabled.");
  bool enabled = true;
  DOCUMENT("The X co-ordinate of the viewport.");
  float x = 0.0f;
  DOCUMENT("The Y co-ordinate of the viewport.");
  float y = 0.0f;
  DOCUMENT("The width of the viewport.");
  float width = 0.0f;
  DOCUMENT("The height of the viewport.");
  float height = 0.0f;
  DOCUMENT("The minimum depth of the viewport.");
  float minDepth = 0.0f;
  DOCUMENT("The maximum depth of the viewport.");
  float maxDepth = 0.0f;
};

DECLARE_REFLECTION_STRUCT(Viewport);

DOCUMENT("Describes a single scissor region.");
struct Scissor
{
  DOCUMENT("");
  bool operator==(const Scissor &o) const
  {
    return x == o.x && y == o.y && width == o.width && height == o.height && enabled == o.enabled;
  }
  bool operator<(const Scissor &o) const
  {
    if(!(x == o.x))
      return x < o.x;
    if(!(y == o.y))
      return y < o.y;
    if(!(width == o.width))
      return width < o.width;
    if(!(height == o.height))
      return height < o.height;
    if(!(enabled == o.enabled))
      return enabled < o.enabled;
    return false;
  }
  Scissor() = default;
  Scissor(int X, int Y, int W, int H, bool en = true) : x(X), y(Y), width(W), height(H), enabled(en)
  {
  }
  DOCUMENT("X co-ordinate of the scissor region.");
  int32_t x = 0;
  DOCUMENT("Y co-ordinate of the scissor region.");
  int32_t y = 0;
  DOCUMENT("Width of the scissor region.");
  int32_t width = 0;
  DOCUMENT("Height of the scissor region.");
  int32_t height = 0;
  DOCUMENT("``True`` if this scissor region is enabled.");
  bool enabled = true;
};

DECLARE_REFLECTION_STRUCT(Scissor);

DOCUMENT("Describes the details of a blend operation.");
struct BlendEquation
{
  DOCUMENT("");
  bool operator==(const BlendEquation &o) const
  {
    return source == o.source && destination == o.destination && operation == o.operation;
  }
  bool operator<(const BlendEquation &o) const
  {
    if(!(source == o.source))
      return source < o.source;
    if(!(destination == o.destination))
      return destination < o.destination;
    if(!(operation == o.operation))
      return operation < o.operation;
    return false;
  }
  DOCUMENT("The :class:`BlendMultiplier` for the source blend value.");
  BlendMultiplier source = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendMultiplier` for the destination blend value.");
  BlendMultiplier destination = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendOperation` to use in the blend calculation.");
  BlendOperation operation = BlendOperation::Add;
};

DECLARE_REFLECTION_STRUCT(BlendEquation);

DOCUMENT("Describes the blend configuration for a given output target.");
struct ColorBlend
{
  DOCUMENT("");
  bool operator==(const ColorBlend &o) const
  {
    return enabled == o.enabled && logicOperationEnabled == o.logicOperationEnabled &&
           colorBlend == o.colorBlend && alphaBlend == o.alphaBlend &&
           logicOperation == o.logicOperation && writeMask == o.writeMask;
  }
  bool operator<(const ColorBlend &o) const
  {
    if(!(enabled == o.enabled))
      return enabled < o.enabled;
    if(!(logicOperationEnabled == o.logicOperationEnabled))
      return logicOperationEnabled < o.logicOperationEnabled;
    if(!(colorBlend == o.colorBlend))
      return colorBlend < o.colorBlend;
    if(!(alphaBlend == o.alphaBlend))
      return alphaBlend < o.alphaBlend;
    if(!(logicOperation == o.logicOperation))
      return logicOperation < o.logicOperation;
    if(!(writeMask == o.writeMask))
      return writeMask < o.writeMask;
    return false;
  }

  DOCUMENT("A :class:`BlendEquation` describing the blending for color values.");
  BlendEquation colorBlend;
  DOCUMENT("A :class:`BlendEquation` describing the blending for alpha values.");
  BlendEquation alphaBlend;

  DOCUMENT(R"(The :class:`LogicOperation` to use for logic operations, if
:data:`logicOperationEnabled` is ``True``.
)");
  LogicOperation logicOperation = LogicOperation::NoOp;

  DOCUMENT("``True`` if blending is enabled for this target.");
  bool enabled = false;
  DOCUMENT("``True`` if the logic operation in :data:`logicOperation` should be used.");
  bool logicOperationEnabled = false;
  DOCUMENT("The mask for writes to the render target.");
  byte writeMask = 0;
};

DECLARE_REFLECTION_STRUCT(ColorBlend);

DOCUMENT("Describes the details of a stencil operation.");
struct StencilFace
{
  DOCUMENT("The :class:`StencilOperation` to apply if the stencil-test fails.");
  StencilOperation failOperation = StencilOperation::Keep;
  DOCUMENT("the :class:`StencilOperation` to apply if the depth-test fails.");
  StencilOperation depthFailOperation = StencilOperation::Keep;
  DOCUMENT("the :class:`StencilOperation` to apply if the stencil-test passes.");
  StencilOperation passOperation = StencilOperation::Keep;
  DOCUMENT("the :class:`CompareFunction` to use for testing stencil values.");
  CompareFunction function = CompareFunction::AlwaysTrue;
  DOCUMENT("The current stencil reference value.");
  uint32_t reference = 0;
  DOCUMENT("The mask for testing stencil values.");
  uint32_t compareMask = 0;
  DOCUMENT("The mask for writing stencil values.");
  uint32_t writeMask = 0;
};

DECLARE_REFLECTION_STRUCT(StencilFace);