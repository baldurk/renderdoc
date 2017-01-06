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

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace renderdoc
{
    public class Camera
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr Camera_InitArcball();
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr Camera_InitFPSLook();
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_Shutdown(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_SetPosition(IntPtr real, float x, float y, float z);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_SetFPSRotation(IntPtr real, float x, float y, float z);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_SetArcballDistance(IntPtr real, float dist);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_ResetArcball(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_RotateArcball(IntPtr real, float ax, float ay, float bx, float by);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Camera_GetBasis(IntPtr real, IntPtr pos, IntPtr fwd, IntPtr right, IntPtr up);

        private IntPtr m_Real = IntPtr.Zero;

        public IntPtr Real { get { return m_Real; } }

        public static Camera InitArcball()
        {
            return new Camera(Camera_InitArcball());
        }

        public static Camera InitFPSLook()
        {
            return new Camera(Camera_InitFPSLook());
        }

        private Camera(IntPtr real)
        {
            m_Real = real;
        }

        public void Shutdown()
        {
            Camera_Shutdown(m_Real);
        }

        public void SetPosition(Vec3f p)
        {
            Camera_SetPosition(m_Real, p.x, p.y, p.z);
        }

        public void SetFPSRotation(Vec3f r)
        {
            Camera_SetFPSRotation(m_Real, r.x, r.y, r.z);
        }

        public void SetArcballDistance(float dist)
        {
            Camera_SetArcballDistance(m_Real, dist);
        }

        public void ResetArcball()
        {
            Camera_ResetArcball(m_Real);
        }

        public void RotateArcball(System.Drawing.Point from, System.Drawing.Point to, System.Drawing.Size winSize)
        {
            float ax = ((float)from.X / (float)winSize.Width) * 2.0f - 1.0f;
            float ay = ((float)from.Y / (float)winSize.Height) * 2.0f - 1.0f;
            float bx = ((float)to.X / (float)winSize.Width) * 2.0f - 1.0f;
            float by = ((float)to.Y / (float)winSize.Height) * 2.0f - 1.0f;

            // this isn't a 'true arcball' but it handles extreme aspect ratios
            // better. We basically 'centre' around the from point always being
            // 0,0 (straight out of the screen) as if you're always dragging
            // the arcball from the middle, and just use the relative movement
            int minDimension = Math.Min(winSize.Width, winSize.Height);

            ax = ay = 0;
            bx = ((float)(to.X - from.X) / (float)minDimension) * 2.0f;
            by = ((float)(to.Y - from.Y) / (float)minDimension) * 2.0f;

            ay = -ay;
            by = -by;

            Camera_RotateArcball(m_Real, ax, ay, bx, by);
        }

        public void GetBasis(out Vec3f pos, out Vec3f fwd, out Vec3f right, out Vec3f up)
        {
            IntPtr p = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr f = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr r = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr u = CustomMarshal.Alloc(typeof(FloatVector));

            Camera_GetBasis(m_Real, p, f, r, u);

            pos = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(p, typeof(FloatVector), false));
            fwd = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(f, typeof(FloatVector), false));
            right = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(r, typeof(FloatVector), false));
            up = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(u, typeof(FloatVector), false));

            CustomMarshal.Free(p);
            CustomMarshal.Free(f);
            CustomMarshal.Free(r);
            CustomMarshal.Free(u);
        }
    }
}
