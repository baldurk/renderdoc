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
        private static extern bool RENDERDOC_SupportLocalReplay(IntPtr logfile, IntPtr outdriver);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RENDERDOC_CreateReplayRenderer(IntPtr logfile, ref float progress, ref IntPtr rendPtr);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_StartGlobalHook(IntPtr pathmatch, IntPtr logfile, CaptureOptions opts);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_ExecuteAndInject(IntPtr app, IntPtr workingDir, IntPtr cmdLine,
                                                                    IntPtr logfile, CaptureOptions opts, bool waitForExit);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_InjectIntoProcess(UInt32 pid, IntPtr logfile, CaptureOptions opts, bool waitForExit);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_CreateRemoteAccessConnection(IntPtr host, UInt32 ident, IntPtr clientName, bool forceConnection);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_EnumerateRemoteConnections(IntPtr host, UInt32[] idents);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RENDERDOC_CreateRemoteReplayConnection(IntPtr host, ref IntPtr outrend);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_GetDefaultCaptureOptions(IntPtr outopts);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_SpawnReplayHost(ref bool killReplay);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_TriggerExceptionHandler(IntPtr exceptionPtrs, bool crashed);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_LogText(IntPtr text);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_GetLogFile();

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RENDERDOC_GetThumbnail(IntPtr filename, byte[] outmem, ref UInt32 len);

        public static bool SupportLocalReplay(string logfile, out string driverName)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            bool ret = RENDERDOC_SupportLocalReplay(logfile_mem, mem);

            CustomMarshal.Free(logfile_mem);

            driverName = CustomMarshal.TemplatedArrayToString(mem, true);

            CustomMarshal.Free(mem);
            return ret;
        }

        public static ReplayRenderer CreateReplayRenderer(string logfile, ref float progress)
        {
            IntPtr rendPtr = IntPtr.Zero;

            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            ReplayCreateStatus ret = RENDERDOC_CreateReplayRenderer(logfile_mem, ref progress, ref rendPtr);

            CustomMarshal.Free(logfile_mem);

            if (rendPtr == IntPtr.Zero || ret != ReplayCreateStatus.Success)
            {
                var e = new System.ApplicationException("Failed to load log for local replay");
                e.Data.Add("status", ret);
                throw e;
            }

            return new ReplayRenderer(rendPtr);
        }

        public static void StartGlobalHook(string pathmatch, string logfile, CaptureOptions opts)
        {
            IntPtr pathmatch_mem = CustomMarshal.MakeUTF8String(pathmatch);
            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            RENDERDOC_StartGlobalHook(pathmatch_mem, logfile_mem, opts);

            CustomMarshal.Free(logfile_mem);
            CustomMarshal.Free(pathmatch_mem);
        }

        public static UInt32 ExecuteAndInject(string app, string workingDir, string cmdLine, string logfile, CaptureOptions opts)
        {
            IntPtr app_mem = CustomMarshal.MakeUTF8String(app);
            IntPtr workingDir_mem = CustomMarshal.MakeUTF8String(workingDir);
            IntPtr cmdLine_mem = CustomMarshal.MakeUTF8String(cmdLine);
            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            UInt32 ret = RENDERDOC_ExecuteAndInject(app_mem, workingDir_mem, cmdLine_mem, logfile_mem, opts, false);

            CustomMarshal.Free(app_mem);
            CustomMarshal.Free(workingDir_mem);
            CustomMarshal.Free(cmdLine_mem);
            CustomMarshal.Free(logfile_mem);

            return ret;
        }

        public static UInt32 InjectIntoProcess(UInt32 pid, string logfile, CaptureOptions opts)
        {
            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            UInt32 ret = RENDERDOC_InjectIntoProcess(pid, logfile_mem, opts, false);

            CustomMarshal.Free(logfile_mem);

            return ret;
        }

        public static RemoteAccess CreateRemoteAccessConnection(string host, UInt32 ident, string clientName, bool forceConnection)
        {
            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);
            IntPtr clientName_mem = CustomMarshal.MakeUTF8String(clientName);

            IntPtr rendPtr = RENDERDOC_CreateRemoteAccessConnection(host_mem, ident, clientName_mem, forceConnection);

            CustomMarshal.Free(host_mem);
            CustomMarshal.Free(clientName_mem);

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
            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);

            UInt32 numIdents = RENDERDOC_EnumerateRemoteConnections(host_mem, null);

            if (numIdents == 0)
            {
                CustomMarshal.Free(host_mem);
                return null;
            }

            UInt32[] ret = new UInt32[numIdents];

            RENDERDOC_EnumerateRemoteConnections(host_mem, ret);

            CustomMarshal.Free(host_mem);

            return ret;
        }

        public static RemoteRenderer CreateRemoteReplayConnection(string host)
        {
            IntPtr rendPtr = IntPtr.Zero;

            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);

            ReplayCreateStatus ret = RENDERDOC_CreateRemoteReplayConnection(host_mem, ref rendPtr);

            CustomMarshal.Free(host_mem);

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
            IntPtr text_mem = CustomMarshal.MakeUTF8String(text);

            RENDERDOC_LogText(text_mem);

            CustomMarshal.Free(text_mem);
        }

        public static string GetLogFilename()
        {
            return CustomMarshal.PtrToStringUTF8(RENDERDOC_GetLogFile());
        }

        public static byte[] GetThumbnail(string filename)
        {
            UInt32 len = 0;

            IntPtr filename_mem = CustomMarshal.MakeUTF8String(filename);

            bool success = RENDERDOC_GetThumbnail(filename_mem, null, ref len);

            if (!success || len == 0)
            {
                CustomMarshal.Free(filename_mem);
                return null;
            }

            byte[] ret = new byte[len];

            success = RENDERDOC_GetThumbnail(filename_mem, ret, ref len);

            CustomMarshal.Free(filename_mem);

            if (!success || len == 0)
                return null;

            return ret;
        }

        public static CaptureOptions GetDefaultCaptureOptions()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(CaptureOptions));

            RENDERDOC_GetDefaultCaptureOptions(mem);

            CaptureOptions ret = (CaptureOptions)CustomMarshal.PtrToStructure(mem, typeof(CaptureOptions), true);

            CustomMarshal.Free(mem);

            return ret;
        }
    }
}
