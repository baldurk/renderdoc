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


#pragma once

#include "vec.h"

class Matrix4f;

class Camera
{
	public:
		Camera()
			: type(eType_FPSLook), pos(), dist(0.0f), angles()
		{ }

		void Arcball(const Vec3f &pos, float dist, const Vec3f &rot);
		void fpsLook(const Vec3f &pos, const Vec3f &rot);

		void SetPosition(const Vec3f &p) { pos = p; }
		void SetAngles(const Vec3f &r) { angles = r; }
		
		const Vec3f GetPosition() const;
		const Vec3f GetForward() const;
		const Vec3f GetRight() const;
		const Vec3f GetUp() const;
		const Matrix4f GetMatrix() const;

	private:
		enum CameraType
		{
			eType_Arcball = 0,
			eType_FPSLook,
		} type;

		Vec3f pos;
		float dist;
		Vec3f angles;
};
