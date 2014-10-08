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

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace renderdoc
{
    public class Camera
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Maths_CameraArcball(float dist, ref FloatVector rot, IntPtr pos, IntPtr fwd, IntPtr right);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Maths_CameraFPSLook(ref FloatVector lookpos, ref FloatVector rot, IntPtr pos, IntPtr fwd, IntPtr right);

        public void Arcball(float dist, Vec3f rot)
        {
            IntPtr p = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr f = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr r = CustomMarshal.Alloc(typeof(FloatVector));

            isarc = true;
            parampos.x = dist;
            parampos.y = 0.0f;
            parampos.z = 0.0f;
            paramrot = new Vec3f(rot);

            var rt = new FloatVector(rot);

            Maths_CameraArcball(dist, ref rt, p, f, r);

            pos = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(p, typeof(FloatVector), false));
            fwd = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(f, typeof(FloatVector), false));
            right = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(r, typeof(FloatVector), false));

            CustomMarshal.Free(p);
            CustomMarshal.Free(f);
            CustomMarshal.Free(r);
        }
        public void fpsLook(Vec3f lookpos, Vec3f lookrot)
        {
            IntPtr p = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr f = CustomMarshal.Alloc(typeof(FloatVector));
            IntPtr r = CustomMarshal.Alloc(typeof(FloatVector));

            isarc = false;
            parampos = new Vec3f(lookpos);
            paramrot = new Vec3f(lookrot);

            var ps = new FloatVector(lookpos);
            var rt = new FloatVector(lookrot);

            Maths_CameraFPSLook(ref ps, ref rt, p, f, r);

            pos = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(p, typeof(FloatVector), false));
            fwd = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(f, typeof(FloatVector), false));
            right = new Vec3f((FloatVector)CustomMarshal.PtrToStructure(r, typeof(FloatVector), false));

            CustomMarshal.Free(p);
            CustomMarshal.Free(f);
            CustomMarshal.Free(r);
        }

        private Vec3f pos = new Vec3f(0.0f, 0.0f, 0.0f);
        private Vec3f fwd = new Vec3f(0.0f, 0.0f, 1.0f);
        private Vec3f right = new Vec3f(1.0f, 0.0f, 0.0f);

        public Vec3f Position { get { return pos; } }
        public Vec3f Forward { get { return fwd; } }
        public Vec3f Right { get { return right; } }

        private bool isarc = false;
        private Vec3f parampos = new Vec3f(0.0f, 0.0f, 0.0f);
        private Vec3f paramrot = new Vec3f(0.0f, 0.0f, 0.0f);

        public bool IsArcball { get { return isarc; } }
        public Vec3f PositionParam { get { return parampos; } }
        public Vec3f RotationParam { get { return paramrot; } }
    }
}
