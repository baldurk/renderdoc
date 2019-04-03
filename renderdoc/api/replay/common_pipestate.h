/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "shader_types.h"

DOCUMENT("Information about a viewport.");
struct Viewport
{
  DOCUMENT("");
  Viewport() = default;
  Viewport(float X, float Y, float W, float H, float MN, float MX, bool en = true)
      : x(X), y(Y), width(W), height(H), minDepth(MN), maxDepth(MX), enabled(en)
  {
  }
  Viewport(const Viewport &) = default;

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
  Scissor() = default;
  Scissor(int X, int Y, int W, int H, bool en = true) : x(X), y(Y), width(W), height(H), enabled(en)
  {
  }
  Scissor(const Scissor &) = default;

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
  BlendEquation() = default;
  BlendEquation(const BlendEquation &) = default;

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
  ColorBlend() = default;
  ColorBlend(const ColorBlend &) = default;

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
  DOCUMENT("");
  StencilFace() = default;
  StencilFace(const StencilFace &) = default;

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

DOCUMENT("Information about a single resource bound to a slot in an API-specific way.");
struct BoundResource
{
  DOCUMENT("");
  BoundResource()
  {
    resourceId = ResourceId();
    dynamicallyUsed = true;
    firstMip = -1;
    firstSlice = -1;
    typeHint = CompType::Typeless;
  }
  BoundResource(ResourceId id)
  {
    resourceId = id;
    dynamicallyUsed = true;
    firstMip = -1;
    firstSlice = -1;
    typeHint = CompType::Typeless;
  }
  BoundResource(const BoundResource &) = default;

  bool operator==(const BoundResource &o) const
  {
    return resourceId == o.resourceId && firstMip == o.firstMip && firstSlice == o.firstSlice &&
           typeHint == o.typeHint;
  }
  bool operator<(const BoundResource &o) const
  {
    if(resourceId != o.resourceId)
      return resourceId < o.resourceId;
    if(firstMip != o.firstMip)
      return firstMip < o.firstMip;
    if(firstSlice != o.firstSlice)
      return firstSlice < o.firstSlice;
    if(typeHint != o.typeHint)
      return typeHint < o.typeHint;
    return false;
  }
  DOCUMENT("A :class:`~renderdoc.ResourceId` identifying the bound resource.");
  ResourceId resourceId;
  DOCUMENT(R"(``True`` if this binding element is dynamically used.

Some APIs provide fine-grained usage based on dynamic shader feedback, to support 'bindless'
scenarios where only a small sparse subset of bound resources are actually used.
)");
  bool dynamicallyUsed = true;
  DOCUMENT("For textures, the highest mip level available on this binding, or -1 for all mips");
  int firstMip;
  DOCUMENT("For textures, the first array slice available on this binding. or -1 for all slices.");
  int firstSlice;
  DOCUMENT(
      "For textures, a :class:`~renderdoc.CompType` hint for how to interpret typeless textures.");
  CompType typeHint;
};

DECLARE_REFLECTION_STRUCT(BoundResource);

// TODO this should be replaced with an rdcmap
DOCUMENT(R"(Contains all of the bound resources at a particular bindpoint. In APIs that don't
support resource arrays, there will only be one bound resource.
)");
struct BoundResourceArray
{
  DOCUMENT("");
  BoundResourceArray() = default;
  BoundResourceArray(const BoundResourceArray &) = default;
  BoundResourceArray(Bindpoint b) : bindPoint(b) {}
  BoundResourceArray(Bindpoint b, const rdcarray<BoundResource> &r) : bindPoint(b), resources(r)
  {
    dynamicallyUsedCount = (uint32_t)r.size();
  }
  // for convenience for searching the array, we compare only using the BindPoint
  bool operator==(const BoundResourceArray &o) const { return bindPoint == o.bindPoint; }
  bool operator!=(const BoundResourceArray &o) const { return !(bindPoint == o.bindPoint); }
  bool operator<(const BoundResourceArray &o) const { return bindPoint < o.bindPoint; }
  DOCUMENT("The bind point for this array of bound resources.");
  Bindpoint bindPoint;

  DOCUMENT("The resources at this bind point");
  rdcarray<BoundResource> resources;

  DOCUMENT(R"(Lists how many bindings in :data:`resources` are dynamically used.

Some APIs provide fine-grained usage based on dynamic shader feedback, to support 'bindless'
scenarios where only a small sparse subset of bound resources are actually used.
)");
  uint32_t dynamicallyUsedCount = 0;
};

DECLARE_REFLECTION_STRUCT(BoundResourceArray);

DOCUMENT("Information about a single vertex or index buffer binding.");
struct BoundVBuffer
{
  DOCUMENT("");
  BoundVBuffer() = default;
  BoundVBuffer(const BoundVBuffer &) = default;

  bool operator==(const BoundVBuffer &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteStride == o.byteStride;
  }
  bool operator<(const BoundVBuffer &o) const
  {
    if(resourceId != o.resourceId)
      return resourceId < o.resourceId;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(byteStride != o.byteStride)
      return byteStride < o.byteStride;
    return false;
  }
  DOCUMENT("A :class:`~renderdoc.ResourceId` identifying the buffer.");
  ResourceId resourceId;
  DOCUMENT("The offset in bytes from the start of the buffer to the data.");
  uint64_t byteOffset = 0;
  DOCUMENT("The stride in bytes between the start of one element and the start of the next.");
  uint32_t byteStride = 0;
};

DECLARE_REFLECTION_STRUCT(BoundVBuffer);

DOCUMENT("Information about a single constant buffer binding.");
struct BoundCBuffer
{
  DOCUMENT("");
  BoundCBuffer() = default;
  BoundCBuffer(const BoundCBuffer &) = default;

  DOCUMENT("A :class:`~renderdoc.ResourceId` identifying the buffer.");
  ResourceId resourceId;
  DOCUMENT("The offset in bytes from the start of the buffer to the constant data.");
  uint64_t byteOffset = 0;
  DOCUMENT("The size in bytes for the constant buffer. Access outside this size returns 0.");
  uint64_t byteSize = 0;
};

DECLARE_REFLECTION_STRUCT(BoundCBuffer);

DOCUMENT("Information about a vertex input attribute feeding the vertex shader.");
struct VertexInputAttribute
{
  DOCUMENT("");
  VertexInputAttribute() = default;
  VertexInputAttribute(const VertexInputAttribute &) = default;

  bool operator==(const VertexInputAttribute &o) const
  {
    return name == o.name && vertexBuffer == o.vertexBuffer && byteOffset == o.byteOffset &&
           perInstance == o.perInstance && instanceRate == o.instanceRate && format == o.format &&
           !memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) &&
           genericEnabled == o.genericEnabled && used == o.used;
  }
  bool operator<(const VertexInputAttribute &o) const
  {
    if(name != o.name)
      return name < o.name;
    if(vertexBuffer != o.vertexBuffer)
      return vertexBuffer < o.vertexBuffer;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(perInstance != o.perInstance)
      return perInstance < o.perInstance;
    if(instanceRate != o.instanceRate)
      return instanceRate < o.instanceRate;
    if(format != o.format)
      return format < o.format;
    if(memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) < 0)
      return true;
    if(genericEnabled != o.genericEnabled)
      return genericEnabled < o.genericEnabled;
    if(used != o.used)
      return used < o.used;
    return false;
  }

  DOCUMENT("The name of this input. This may be a variable name or a semantic name.");
  rdcstr name;
  DOCUMENT("The index of the vertex buffer used to provide this attribute.");
  int vertexBuffer;
  DOCUMENT("The byte offset from the start of the vertex data for this VB to this attribute.");
  uint32_t byteOffset;
  DOCUMENT("``True`` if this attribute runs at instance rate.");
  bool perInstance;
  DOCUMENT(R"(If :data:`perInstance` is ``True``, the number of instances that source the same value
from the vertex buffer before advancing to the next value.
)");
  int instanceRate;
  DOCUMENT("A :class:`~renderdoc.ResourceFormat` with the interpreted format of this attribute.");
  ResourceFormat format;
  DOCUMENT(R"(A :class:`~renderdoc.PixelValue` with the generic value for this attribute if it has
no VB bound.
)");
  PixelValue genericValue;
  DOCUMENT("``True`` if this attribute is using :data:`genericValue` for its data.");
  bool genericEnabled;
  DOCUMENT("``True`` if this attribute is enabled and used by the vertex shader.");
  bool used;
};

DECLARE_REFLECTION_STRUCT(VertexInputAttribute);