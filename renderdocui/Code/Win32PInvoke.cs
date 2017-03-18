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
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace renderdocui.Code
{
    class Win32PInvoke
    {
        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr LoadLibrary(string lpFileName);
        [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern IntPtr GetModuleHandle(string lpFileName);

        [StructLayout(LayoutKind.Sequential)]
        public struct POINT
        {
            public int X;
            public int Y;

            public POINT(int x, int y)
            {
                this.X = x;
                this.Y = y;
            }
        }

        // for redirecting mousewheel
        [DllImport("user32.dll")]
        public static extern IntPtr WindowFromPoint(POINT pt);
        [DllImport("user32.dll")]
        public static extern IntPtr SendMessage(IntPtr wnd, int msg, IntPtr wp, IntPtr lp);

        // windows message from winuser.h
        public enum Win32Message
        {
            WM_MOUSEWHEEL  = 0x020A,
            TCM_ADJUSTRECT = 0x1328,
        };

        [Flags]
        public enum HChangeNotifyEventID
        {
            SHCNE_ALLEVENTS = 0x7FFFFFFF,
            SHCNE_ASSOCCHANGED = 0x08000000,
            SHCNE_ATTRIBUTES = 0x00000800,
            SHCNE_CREATE = 0x00000002,
            SHCNE_DELETE = 0x00000004,
            SHCNE_DRIVEADD = 0x00000100,
            SHCNE_DRIVEADDGUI = 0x00010000,
            SHCNE_DRIVEREMOVED = 0x00000080,
            SHCNE_EXTENDED_EVENT = 0x04000000,
            SHCNE_FREESPACE = 0x00040000,
            SHCNE_MEDIAINSERTED = 0x00000020,
            SHCNE_MEDIAREMOVED = 0x00000040,
            SHCNE_MKDIR = 0x00000008,
            SHCNE_NETSHARE = 0x00000200,
            SHCNE_NETUNSHARE = 0x00000400,
            SHCNE_RENAMEFOLDER = 0x00020000,
            SHCNE_RENAMEITEM = 0x00000001,
            SHCNE_RMDIR = 0x00000010,
            SHCNE_SERVERDISCONNECT = 0x00004000,
            SHCNE_UPDATEDIR = 0x00001000,
            SHCNE_UPDATEIMAGE = 0x00008000,
        }

        [Flags]
        public enum HChangeNotifyFlags
        {
            SHCNF_DWORD = 0x0003,
            SHCNF_IDLIST = 0x0000,
            SHCNF_PATHA = 0x0001,
            SHCNF_PATHW = 0x0005,
            SHCNF_PRINTERA = 0x0002,
            SHCNF_PRINTERW = 0x0006,
            SHCNF_FLUSH = 0x1000,
            SHCNF_FLUSHNOWAIT = 0x2000,
            SHCNF_NOTIFYRECURSIVE = 0x10000,
        }

        [DllImport("shell32.dll")]
        public static extern void SHChangeNotify(HChangeNotifyEventID wEventId, HChangeNotifyFlags uFlags, IntPtr dwItem1, IntPtr dwItem2);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern uint GetShortPathName(string lpszLongPath, char[] lpszShortPath, int cchBuffer);

        public static string ShortPath(string longpath)
        {
            char[] buffer = new char[256];

            GetShortPathName(longpath, buffer, buffer.Length);

            return new string(buffer);
        }

        [DllImport("mpr.dll", CharSet = CharSet.Unicode)]
        private static extern uint WNetGetUniversalNameW(string lpLocalPath, int dwInfoLevel, IntPtr lpBuffer, ref int lpBufferSize);
        private const int UNIVERSAL_NAME_INFO_LEVEL = 0x00000001;
        private const uint ERROR_MORE_DATA = 234;

        public static string GetUniversalName(string localPath)
        {
            int size = 0;

            IntPtr buf = (IntPtr)IntPtr.Size; // don't initialise to zero, as otherwise the call fails

            uint ret = WNetGetUniversalNameW(localPath, UNIVERSAL_NAME_INFO_LEVEL, buf, ref size);

            if (ret != ERROR_MORE_DATA)
                return localPath;

            buf = Marshal.AllocHGlobal(size);

            ret = WNetGetUniversalNameW(localPath, UNIVERSAL_NAME_INFO_LEVEL, buf, ref size);

            string universalPath = localPath;

            if (ret == 0)
            {
                // buf points to a struct that contains just a string pointer, that points
                // immediately after it. So we need to advance by one IntPtr to get to the
                // actual string
                universalPath = Marshal.PtrToStringUni(IntPtr.Add(buf, IntPtr.Size));
            }

            Marshal.FreeHGlobal(buf);

            return universalPath;
        }
    }
}
