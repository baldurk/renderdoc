/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

namespace DXBC
{
// matches D3D11_SHADER_VERSION_TYPE from d3d11shader.h
enum class ShaderType
{
  // D3D10 Shaders
  Pixel = 0,
  Vertex = 1,
  Geometry = 2,

  // D3D11 Shaders
  Hull = 3,
  Domain = 4,
  Compute = 5,

  // D3D12 Shaders
  Library = 6,
  RayGeneration = 7,
  Intersection = 8,
  AnyHit = 9,
  ClosestHit = 10,
  Miss = 11,
  Callable = 12,
  Mesh = 13,
  Amplification = 14,

  // Max value
  Max,
};
};