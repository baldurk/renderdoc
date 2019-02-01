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

#include "matrix.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "quat.h"
#include "vec.h"

static inline size_t matIdx(const size_t x, const size_t y)
{
  return x + y * 4;
}

Matrix4f Matrix4f::Mul(const Matrix4f &o) const
{
  Matrix4f m;
  for(size_t x = 0; x < 4; x++)
  {
    for(size_t y = 0; y < 4; y++)
    {
      m[matIdx(x, y)] =
          (*this)[matIdx(x, 0)] * o[matIdx(0, y)] + (*this)[matIdx(x, 1)] * o[matIdx(1, y)] +
          (*this)[matIdx(x, 2)] * o[matIdx(2, y)] + (*this)[matIdx(x, 3)] * o[matIdx(3, y)];
    }
  }

  return m;
}

Matrix4f Matrix4f::Transpose() const
{
  Matrix4f m;
  for(size_t x = 0; x < 4; x++)
    for(size_t y = 0; y < 4; y++)
      m[matIdx(x, y)] = (*this)[matIdx(y, x)];

  return m;
}

Matrix4f Matrix4f::Inverse() const
{
  float a0 = (*this)[0] * (*this)[5] - (*this)[1] * (*this)[4];
  float a1 = (*this)[0] * (*this)[6] - (*this)[2] * (*this)[4];
  float a2 = (*this)[0] * (*this)[7] - (*this)[3] * (*this)[4];
  float a3 = (*this)[1] * (*this)[6] - (*this)[2] * (*this)[5];
  float a4 = (*this)[1] * (*this)[7] - (*this)[3] * (*this)[5];
  float a5 = (*this)[2] * (*this)[7] - (*this)[3] * (*this)[6];
  float b0 = (*this)[8] * (*this)[13] - (*this)[9] * (*this)[12];
  float b1 = (*this)[8] * (*this)[14] - (*this)[10] * (*this)[12];
  float b2 = (*this)[8] * (*this)[15] - (*this)[11] * (*this)[12];
  float b3 = (*this)[9] * (*this)[14] - (*this)[10] * (*this)[13];
  float b4 = (*this)[9] * (*this)[15] - (*this)[11] * (*this)[13];
  float b5 = (*this)[10] * (*this)[15] - (*this)[11] * (*this)[14];

  float det = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
  if(fabsf(det) > FLT_EPSILON)
  {
    Matrix4f inverse;
    inverse[0] = +(*this)[5] * b5 - (*this)[6] * b4 + (*this)[7] * b3;
    inverse[4] = -(*this)[4] * b5 + (*this)[6] * b2 - (*this)[7] * b1;
    inverse[8] = +(*this)[4] * b4 - (*this)[5] * b2 + (*this)[7] * b0;
    inverse[12] = -(*this)[4] * b3 + (*this)[5] * b1 - (*this)[6] * b0;
    inverse[1] = -(*this)[1] * b5 + (*this)[2] * b4 - (*this)[3] * b3;
    inverse[5] = +(*this)[0] * b5 - (*this)[2] * b2 + (*this)[3] * b1;
    inverse[9] = -(*this)[0] * b4 + (*this)[1] * b2 - (*this)[3] * b0;
    inverse[13] = +(*this)[0] * b3 - (*this)[1] * b1 + (*this)[2] * b0;
    inverse[2] = +(*this)[13] * a5 - (*this)[14] * a4 + (*this)[15] * a3;
    inverse[6] = -(*this)[12] * a5 + (*this)[14] * a2 - (*this)[15] * a1;
    inverse[10] = +(*this)[12] * a4 - (*this)[13] * a2 + (*this)[15] * a0;
    inverse[14] = -(*this)[12] * a3 + (*this)[13] * a1 - (*this)[14] * a0;
    inverse[3] = -(*this)[9] * a5 + (*this)[10] * a4 - (*this)[11] * a3;
    inverse[7] = +(*this)[8] * a5 - (*this)[10] * a2 + (*this)[11] * a1;
    inverse[11] = -(*this)[8] * a4 + (*this)[9] * a2 - (*this)[11] * a0;
    inverse[15] = +(*this)[8] * a3 - (*this)[9] * a1 + (*this)[10] * a0;

    float invDet = 1.0f / det;
    inverse[0] *= invDet;
    inverse[1] *= invDet;
    inverse[2] *= invDet;
    inverse[3] *= invDet;
    inverse[4] *= invDet;
    inverse[5] *= invDet;
    inverse[6] *= invDet;
    inverse[7] *= invDet;
    inverse[8] *= invDet;
    inverse[9] *= invDet;
    inverse[10] *= invDet;
    inverse[11] *= invDet;
    inverse[12] *= invDet;
    inverse[13] *= invDet;
    inverse[14] *= invDet;
    inverse[15] *= invDet;

    return inverse;
  }

  // no inverse
  return Matrix4f::Identity();
}

Vec3f Matrix4f::Transform(const Vec3f &v, const float w) const
{
  Vec3f vout = Vec3f((*this)[matIdx(0, 0)] * v.x + (*this)[matIdx(0, 1)] * v.y +
                         (*this)[matIdx(0, 2)] * v.z + (*this)[matIdx(0, 3)] * w,
                     (*this)[matIdx(1, 0)] * v.x + (*this)[matIdx(1, 1)] * v.y +
                         (*this)[matIdx(1, 2)] * v.z + (*this)[matIdx(1, 3)] * w,
                     (*this)[matIdx(2, 0)] * v.x + (*this)[matIdx(2, 1)] * v.y +
                         (*this)[matIdx(2, 2)] * v.z + (*this)[matIdx(2, 3)] * w);
  float wout = (*this)[matIdx(3, 0)] * v.x + (*this)[matIdx(3, 1)] * v.y +
               (*this)[matIdx(3, 2)] * v.z + (*this)[matIdx(3, 3)] * w;

  return vout * (1.0f / wout);
}

const Vec3f Matrix4f::GetPosition() const
{
  return Vec3f(f[12], f[13], f[14]);
}

const Vec3f Matrix4f::GetForward() const
{
  return Vec3f(f[8], f[9], f[10]);
}

const Vec3f Matrix4f::GetRight() const
{
  return Vec3f(f[0], f[1], f[2]);
}

const Vec3f Matrix4f::GetUp() const
{
  return Vec3f(f[4], f[5], f[6]);
}

Matrix4f Matrix4f::Translation(const Vec3f &t)
{
  Matrix4f trans = Matrix4f::Identity();

  trans[12] = t.x;
  trans[13] = t.y;
  trans[14] = t.z;

  return trans;
}

Matrix4f Matrix4f::RotationX(const float r)
{
  float m[16] = {
      1.0f, 0.0f,    0.0f,    0.0f, 0.0f, cosf(r), -sinf(r), 0.0f,
      0.0f, sinf(r), cosf(r), 0.0f, 0.0f, 0.0f,    0.0f,     1.0f,
  };

  return Matrix4f(m);
}

Matrix4f Matrix4f::RotationY(const float r)
{
  float m[16] = {
      cosf(r),  0.0f, sinf(r), 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      -sinf(r), 0.0f, cosf(r), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };

  return Matrix4f(m);
}

Matrix4f Matrix4f::RotationZ(const float r)
{
  float m[16] = {
      cosf(r), -sinf(r), 0.0f, 0.0f, sinf(r), cosf(r), 0.0f, 0.0f,
      0.0f,    0.0f,     1.0f, 0.0f, 0.0f,    0.0f,    0.0f, 1.0f,
  };

  return Matrix4f(m);
}

Matrix4f Matrix4f::RotationZYX(const Vec3f &rot)
{
  Quatf Qx = Quatf::AxisAngle(Vec3f(1.0f, 0.0f, 0.0f), rot.x);
  Quatf Qy = Quatf::AxisAngle(Vec3f(0.0f, 1.0f, 0.0f), rot.y);
  Quatf Qz = Quatf::AxisAngle(Vec3f(0.0f, 0.0f, 1.0f), rot.z);

  Quatf R = Qx * Qy * Qz;

  return R.GetMatrix();
}

Matrix4f Matrix4f::RotationXYZ(const Vec3f &rot)
{
  Quatf Qx = Quatf::AxisAngle(Vec3f(1.0f, 0.0f, 0.0f), rot.x);
  Quatf Qy = Quatf::AxisAngle(Vec3f(0.0f, 1.0f, 0.0f), rot.y);
  Quatf Qz = Quatf::AxisAngle(Vec3f(0.0f, 0.0f, 1.0f), rot.z);

  Quatf R = Qz * Qy * Qx;

  return R.GetMatrix();
}

Matrix4f Matrix4f::Orthographic(const float Near, const float Far)
{
  float L = -10.0f;
  float R = 10.0f;

  float T = 10.0f;
  float B = -10.0f;

  float N = -fabsf(Far - Near) * 0.5f;
  float F = fabsf(Far - Near) * 0.5f;

  if(Far < Near)
  {
    float tmp = F;
    F = N;
    N = tmp;
  }

  float ortho[16] = {
      2.0f / (R - L), 0.0f,           0.0f,           (L + R) / (L - R),
      0.0f,           2.0f / (T - B), 0.0f,           (T + B) / (B - T),
      0.0f,           0.0f,           1.0f / (F - N), (F + N) / (N - F),
      0.0f,           0.0f,           0.0f,           1.0f,
  };

  return Matrix4f(ortho);
}

Matrix4f Matrix4f::Perspective(const float degfov, const float N, const float F, const float A)
{
  const float radfov = degfov * (3.1415926535f / 180.0f);
  float S = 1 / tanf(radfov * 0.5f);

  float persp[16] = {
      S / A,       0.0f, 0.0f, 0.0f, 0.0f,
      S,           0.0f, 0.0f, 0.0f, 0.0f,
      F / (F - N), 1.0f, 0.0f, 0.0f, -(F * N) / (F - N),
      0.0f,
  };

  return Matrix4f(persp);
}

Matrix4f Matrix4f::ReversePerspective(const float degfov, const float N, const float A)
{
  const float radfov = degfov * (3.1415926535f / 180.0f);
  float S = 1 / tanf(radfov * 0.5f);

  float persp[16] = {
      S / A, 0.0f, 0.0f, 0.0f, 0.0f, S, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, N, 0.0f,
  };

  return Matrix4f(persp);
}
