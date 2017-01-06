/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "quat.h"
#include "vec.h"

class Matrix4f;

class Camera
{
public:
  enum CameraType
  {
    eType_Arcball = 0,
    eType_FPSLook,
  };

  Camera(CameraType t) : type(t), dirty(true), pos(), dist(0.0f), angles() { ResetArcball(); }
  void SetPosition(const Vec3f &p)
  {
    dirty = true;
    pos = p;
  }

  // Arcball functions
  void ResetArcball();
  void SetArcballDistance(float d)
  {
    dirty = true;
    dist = d;
  }
  void RotateArcball(const Vec2f &from, const Vec2f &to);

  // FPS look functions
  void SetFPSRotation(const Vec3f &rot)
  {
    dirty = true;
    angles = rot;
  }

  const Vec3f GetPosition();
  const Vec3f GetForward();
  const Vec3f GetRight();
  const Vec3f GetUp();
  const Matrix4f GetMatrix();

private:
  void Update();

  CameraType type;

  bool dirty;
  Matrix4f mat, basis;

  Vec3f pos;

  // Arcball
  Quatf arcrot;
  float dist;

  // FPS look
  Vec3f angles;
};
