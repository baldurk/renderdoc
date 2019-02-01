/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <math.h>
#include "matrix.h"
#include "vec.h"

class Quatf
{
public:
  float w;
  Vec3f v;

  static Quatf AxisAngle(Vec3f axis, float angle)
  {
    Quatf q;

    q.w = cosf(angle / 2.0f);
    q.v.x = axis.x * sinf(angle / 2.0f);
    q.v.y = axis.y * sinf(angle / 2.0f);
    q.v.z = axis.z * sinf(angle / 2.0f);

    return q;
  }

  Matrix4f GetMatrix()
  {
    float q0 = w;
    float q1 = v.x;
    float q2 = v.y;
    float q3 = v.z;

    float m[16] = {
        1.0f - 2 * (q2 * q2 + q3 * q3),
        2.0f * (q1 * q2 - q0 * q3),
        2.0f * (q0 * q2 + q1 * q3),
        0.0f,
        2.0f * (q1 * q2 + q0 * q3),
        1.0f - 2 * (q1 * q1 + q3 * q3),
        2.0f * (q2 * q3 - q0 * q1),
        0.0f,
        2.0f * (q1 * q3 - q0 * q2),
        2.0f * (q0 * q1 + q2 * q3),
        1.0f - 2 * (q1 * q1 + q2 * q2),
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };

    return Matrix4f(m);
  }
};

inline Quatf operator*(const Quatf &a, const Quatf &b)
{
  Quatf r;

  r.w = a.w * b.w - a.v.Dot(b.v);
  r.v = b.v * a.w + a.v * b.w + a.v.Cross(b.v);

  return r;
}
