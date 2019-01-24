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

// use some preprocessor hacks to compile the same header in both GLSL and C++ so we can define
// classes that represent a whole cbuffer

#include "common/common.h"
#include "maths/matrix.h"
#include "maths/vec.h"

#define uniform struct

#define vec2 Vec2f
#define vec3 Vec3f
#define vec4 Vec4f

#define mat4 Matrix4f

#define uint uint32_t
#define uvec4 Vec4u

#if !defined(VULKAN) && !defined(OPENGL)
#error Must define VULKAN or OPENGL before including glsl_ubos.h
#endif

#if defined(VULKAN) && defined(OPENGL)
#error Only one of VULKAN and OPENGL must be defined in glsl_ubos.h
#endif

#include "glsl_ubos.h"