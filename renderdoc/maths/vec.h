/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#include <stdint.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)
#endif

struct Vec2f
{
  Vec2f(float X = 0.0f, float Y = 0.0f)
  {
    x = X;
    y = Y;
  }

  union
  {
    struct
    {
      float x, y;
    };
    float fv[2];
  };
};

class Vec3f
{
public:
  Vec3f(const float X = 0.0f, const float Y = 0.0f, const float Z = 0.0f) : x(X), y(Y), z(Z) {}
  inline float Dot(const Vec3f &o) const { return x * o.x + y * o.y + z * o.z; }
  inline Vec3f Cross(const Vec3f &o) const
  {
    return Vec3f(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
  }

  float Length() const;
  inline void Normalise()
  {
    float l = Length();
    x /= l;
    y /= l;
    z /= l;
  }

  union
  {
    struct
    {
      float x, y, z;
    };
    float fv[3];
  };
};

struct FloatVector;

struct Vec4f
{
  Vec4f(float X = 0.0f, float Y = 0.0f, float Z = 0.0f, float W = 0.0f)
  {
    x = X;
    y = Y;
    z = Z;
    w = W;
  }
  Vec4f(const FloatVector &v);
  operator Vec3f() const { return Vec3f(x, y, z); }
  operator FloatVector() const;
  union
  {
    struct
    {
      float x, y, z, w;
    };
    float fv[4];
  };
};

inline Vec3f operator*(const Vec3f &a, const float b)
{
  return Vec3f(a.x * b, a.y * b, a.z * b);
}

inline Vec3f operator+(const Vec3f &a, const Vec3f &b)
{
  return Vec3f(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline Vec3f operator-(const Vec3f &a)
{
  return Vec3f(-a.x, -a.y, -a.z);
}

inline Vec3f operator-(const Vec3f &a, const Vec3f &b)
{
  return a + (-b);
}

inline Vec3f operator-=(Vec3f &a, const Vec3f &b)
{
  a = a - b;
  return a;
}

inline Vec3f operator+=(Vec3f &a, const Vec3f &b)
{
  a = a + b;
  return a;
}

inline Vec4f operator*(const Vec4f &a, const float b)
{
  return Vec4f(a.x * b, a.y * b, a.z * b, a.w * b);
}

inline Vec4f operator+(const Vec4f &a, const Vec4f &b)
{
  return Vec4f(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

inline Vec4f operator-(const Vec4f &a)
{
  return Vec4f(-a.x, -a.y, -a.z, -a.w);
}

inline Vec4f operator-(const Vec4f &a, const Vec4f &b)
{
  return a + (-b);
}

inline Vec4f operator-=(Vec4f &a, const Vec4f &b)
{
  a = a - b;
  return a;
}

inline Vec4f operator+=(Vec4f &a, const Vec4f &b)
{
  a = a + b;
  return a;
}

struct Vec4u
{
  Vec4u(uint32_t X = 0, uint32_t Y = 0, uint32_t Z = 0, uint32_t W = 0)
  {
    x = X;
    y = Y;
    z = Z;
    w = W;
  }
  union
  {
    struct
    {
      uint32_t x, y, z, w;
    };
    uint32_t uv[4];
  };
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif
