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
using System.Runtime.InteropServices;
using System.Text;

namespace renderdoc
{
    class StaticExports
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RENDERDOC_SupportLocalReplay(string logfile, IntPtr outdriver);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RENDERDOC_CreateReplayRenderer(string logfile, ref float progress, ref IntPtr rendPtr);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_ExecuteAndInject(string app, string workingDir, string cmdLine,
                                                                    string logfile, CaptureOptions opts, bool waitForExit);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_InjectIntoProcess(UInt32 pid, string logfile, CaptureOptions opts, bool waitForExit);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_CreateRemoteAccessConnection(string host, UInt32 ident, string clientName, bool forceConnection);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_EnumerateRemoteConnections(string host, UInt32[] idents);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RENDERDOC_CreateRemoteReplayConnection(string host, ref IntPtr outrend);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_SpawnReplayHost(ref bool killReplay);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_TriggerExceptionHandler(IntPtr exceptionPtrs, bool crashed);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_LogText(string text);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_GetLogFile();

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RENDERDOC_GetThumbnail(string filename, byte[] outmem, ref UInt32 len);

        public static bool SupportLocalReplay(string logfile, out string driverName)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));
            bool ret = RENDERDOC_SupportLocalReplay(logfile, mem);

            driverName = CustomMarshal.TemplatedArrayToUniString(mem, true);

            CustomMarshal.Free(mem);
            return ret;
        }

        public static ReplayRenderer CreateReplayRenderer(string logfile, ref float progress)
        {
            IntPtr rendPtr = IntPtr.Zero;

            ReplayCreateStatus ret = RENDERDOC_CreateReplayRenderer(logfile, ref progress, ref rendPtr);

            if (rendPtr == IntPtr.Zero || ret != ReplayCreateStatus.Success)
            {
                var e = new System.ApplicationException("Failed to load log for local replay");
                e.Data.Add("status", ret);
                throw e;
            }

            return new ReplayRenderer(rendPtr);
        }

        public static UInt32 ExecuteAndInject(string app, string workingDir, string cmdLine, string logfile, CaptureOptions opts)
        {
            return RENDERDOC_ExecuteAndInject(app, workingDir, cmdLine, logfile, opts, false);
        }

        public static UInt32 InjectIntoProcess(UInt32 pid, string logfile, CaptureOptions opts)
        {
            return RENDERDOC_InjectIntoProcess(pid, logfile, opts, false);
        }

        public static RemoteAccess CreateRemoteAccessConnection(string host, UInt32 ident, string clientName, bool forceConnection)
        {
            IntPtr rendPtr = RENDERDOC_CreateRemoteAccessConnection(host, ident, clientName, forceConnection);

            if (rendPtr == IntPtr.Zero)
            {
                var e = new System.ApplicationException("Failed to open remote access connection");
                e.Data.Add("status", ReplayCreateStatus.UnknownError);
                throw e;
            }

            return new RemoteAccess(rendPtr);
        }

        public static UInt32[] EnumerateRemoteConnections(string host)
        {
            UInt32 numIdents = RENDERDOC_EnumerateRemoteConnections(host, null);

            if (numIdents == 0)
                return null;

            UInt32[] ret = new UInt32[numIdents];

            RENDERDOC_EnumerateRemoteConnections(host, ret);

            return ret;
        }

        public static RemoteRenderer CreateRemoteReplayConnection(string host)
        {
            IntPtr rendPtr = IntPtr.Zero;

            ReplayCreateStatus ret = RENDERDOC_CreateRemoteReplayConnection(host, ref rendPtr);

            if (rendPtr == IntPtr.Zero || ret != ReplayCreateStatus.Success)
            {
                var e = new System.ApplicationException("Failed to connect to remote replay host");
                e.Data.Add("status", ret);
                throw e;
            }

            return new RemoteRenderer(rendPtr);
        }

        public static void SpawnReplayHost(ref bool killReplay)
        {
            RENDERDOC_SpawnReplayHost(ref killReplay);
        }

        public static void TriggerExceptionHandler(IntPtr exceptionPtrs, bool crashed)
        {
            RENDERDOC_TriggerExceptionHandler(exceptionPtrs, crashed);
        }

        public static void LogText(string text)
        {
            RENDERDOC_LogText(text);
        }

        public static string GetLogFilename()
        {
            return Marshal.PtrToStringUni(RENDERDOC_GetLogFile());
        }

        public static byte[] GetThumbnail(string filename)
        {
            UInt32 len = 0;
            
            bool success = RENDERDOC_GetThumbnail(filename, null, ref len);

            if (!success || len == 0)
                return null;

            byte[] ret = new byte[len];

            success = RENDERDOC_GetThumbnail(filename, ret, ref len);

            if (!success || len == 0)
                return null;

            return ret;
        }
    }
}
