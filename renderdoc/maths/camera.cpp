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

#include "camera.h"
#include <math.h>
#include <string.h>
#include "common/common.h"
#include "matrix.h"

void Camera::ResetArcball()
{
  dirty = true;

  arcrot = Quatf::AxisAngle(Vec3f(1, 0, 0), 0.0f);
}

// https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball
void Camera::RotateArcball(float ax, float ay, float bx, float by)
{
  Vec3f a, b;

  Vec2f from(ax, ay);
  Vec2f to(bx, by);

  float az = from.x * from.x + from.y * from.y;
  float bz = to.x * to.x + to.y * to.y;

  // keep the controls stable by rejecting very small movements.
  if(fabsf(az - bz) < 1e-5f)
    return;

  if(az < 1.0f)
  {
    a = Vec3f(from.x, from.y, sqrt(1.0f - az));
  }
  else
  {
    a = Vec3f(from.x, from.y, 0.0f);
    a.Normalise();
  }

  if(bz < 1.0f)
  {
    b = Vec3f(to.x, to.y, sqrt(1.0f - bz));
  }
  else
  {
    b = Vec3f(to.x, to.y, 0.0f);
    b.Normalise();
  }

  float angle = acosf(RDCMIN(1.0f, a.Dot(b)));

  Vec3f axis = a.Cross(b);
  axis.Normalise();

  dirty = true;

  Quatf delta = Quatf::AxisAngle(axis, angle);
  arcrot = arcrot * delta;
}

void Camera::Update()
{
  if(!dirty)
    return;

  if(type == CameraType::FPSLook)
  {
    Matrix4f p = Matrix4f::Translation(-pos);
    Matrix4f r = Matrix4f::RotationXYZ(-angles);

    mat = r.Mul(p);
    basis = mat.Transpose();
  }
  else
  {
    Matrix4f p = Matrix4f::Translation(-pos);
    Matrix4f r = arcrot.GetMatrix();
    Matrix4f d = Matrix4f::Translation(Vec3f(0.0f, 0.0f, dist));

    mat = d.Mul(r.Mul(p));
  }
}

const Matrix4f Camera::GetMatrix()
{
  Update();
  return mat;
}

FloatVector Camera::GetPosition()
{
  return FloatVector(pos.x, pos.y, pos.z, 1.0f);
}

FloatVector Camera::GetForward()
{
  Vec3f fwd = basis.GetForward();
  return FloatVector(fwd.x, fwd.y, fwd.z, 1.0f);
}

FloatVector Camera::GetRight()
{
  Vec3f right = basis.GetRight();
  return FloatVector(right.x, right.y, right.z, 1.0f);
}

FloatVector Camera::GetUp()
{
  Vec3f up = basis.GetUp();
  return FloatVector(up.x, up.y, up.z, 1.0f);
}
