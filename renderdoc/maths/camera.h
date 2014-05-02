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
			: order(ORDER_TRANS_ROT), pos(), angles()
		{ }

		void Arcball(float dist, Vec3f rot);
		void fpsLook(Vec3f pos, Vec3f rot);

		void SetPosition(const Vec3f &p) { pos = p; }
		void SetAngles(const Vec3f &r) { angles = r; }
		
		const Vec3f GetPosition();
		const Vec3f GetForward();
		const Vec3f GetRight();
		const Vec3f GetUp();
		const Matrix4f GetMatrix();

	private:
		enum OperationOrder
		{
			ORDER_ROT_TRANS = 0,
			ORDER_TRANS_ROT,
		} order;

		Vec3f pos;
		Vec3f angles;
};
