/******************************************************************************
 * The MIT License (MIT)
 * 
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


#include <string.h>
#include <math.h>

#include "camera.h"
#include "matrix.h"

void Camera::Arcball(float dist, const Vec3f &rot)
{
	pos = Vec3f(0.0f, 0.0f, dist);

	order = ORDER_ROT_TRANS;

	angles.x = rot.x;
	angles.y = rot.y;
}

void Camera::fpsLook(const Vec3f &p, const Vec3f &rot)
{
	pos = -p;
	
	angles.x = -rot.x;
	angles.y = -rot.y;

	order = ORDER_TRANS_ROT;
}

const Matrix4f Camera::GetMatrix() const
{
	Matrix4f p = Matrix4f::Translation(pos);
	Matrix4f r = Matrix4f::RotationXYZ(angles);

	if(order == ORDER_TRANS_ROT)
		return r.Mul(p);

	return p.Mul(r);
}

const Vec3f Camera::GetPosition() const
{
	return GetMatrix().GetPosition();
}

const Vec3f Camera::GetForward() const
{
	return Matrix4f::RotationZYX(-angles).GetForward();
}

const Vec3f Camera::GetRight() const
{
	return Matrix4f::RotationZYX(-angles).GetRight();
}
