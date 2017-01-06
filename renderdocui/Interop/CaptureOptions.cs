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
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace renderdoc
{
    [DebuggerDisplay("{m_ID}")]
    [StructLayout(LayoutKind.Sequential)]
    public struct ResourceId
    {
        private UInt64 m_ID;

        public override string ToString()
        {
            return String.Format("{0}", m_ID);
        }

        public override bool Equals(Object obj)
        {
            return obj is ResourceId && this == (ResourceId)obj;
        }
        public override int GetHashCode()
        {
            return m_ID.GetHashCode();
        }
        public static bool operator ==(ResourceId x, ResourceId y)
        {
            return x.m_ID == y.m_ID;
        }
        public static bool operator !=(ResourceId x, ResourceId y)
        {
            return !(x == y);
        }

        public static ResourceId Null = new ResourceId(0);

        public ResourceId(UInt64 id)
        {
            m_ID = id;
        }
    };

    [StructLayout(LayoutKind.Sequential)]
    public class CaptureOptions
    {
        public bool AllowVSync;
        public bool AllowFullscreen;
        public bool APIValidation;
        public bool CaptureCallstacks;
        public bool CaptureCallstacksOnlyDraws;
        public UInt32 DelayForDebugger;
        public bool VerifyMapWrites;
        public bool HookIntoChildren;
        public bool RefAllResources;
        public bool SaveAllInitials;
        public bool CaptureAllCmdLists;
        public bool DebugOutputMute;
    };
};
