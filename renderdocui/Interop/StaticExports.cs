﻿/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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
    public class ReplayCreateException : Exception
    {
        public ReplayCreateException(ReplayCreateStatus status)
            : base(String.Format("Replay creation failure: {0}", status.Str()))
        {
            Status = status;
        }

        public ReplayCreateException(ReplayCreateStatus status, string msg)
            : base(msg)
        {
            Status = status;
        }

        public ReplayCreateStatus Status;
    }

    class StaticExports
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplaySupport RENDERDOC_SupportLocalReplay(IntPtr logfile, IntPtr outdriver, IntPtr outident);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RENDERDOC_CreateReplayRenderer(IntPtr logfile, ref float progress, ref IntPtr rendPtr);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_StartGlobalHook(IntPtr pathmatch, IntPtr logfile, CaptureOptions opts);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_ExecuteAndInject(IntPtr app, IntPtr workingDir, IntPtr cmdLine, IntPtr env,
                                                                    IntPtr logfile, CaptureOptions opts, bool waitForExit);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_InjectIntoProcess(UInt32 pid, IntPtr env, IntPtr logfile, CaptureOptions opts, bool waitForExit);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_CreateTargetControl(IntPtr host, UInt32 ident, IntPtr clientName, bool forceConnection);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_EnumerateRemoteTargets(IntPtr host, UInt32 nextIdent);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RENDERDOC_CreateRemoteServerConnection(IntPtr host, UInt32 port, ref IntPtr outrend);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_GetDefaultCaptureOptions(IntPtr outopts);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_BecomeRemoteServer(IntPtr host, UInt32 port, ref bool killReplay);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_TriggerExceptionHandler(IntPtr exceptionPtrs, bool crashed);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_LogText(IntPtr text);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_GetLogFile();

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_GetConfigSetting(IntPtr name);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_GetVersionString();

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_SetConfigSetting(IntPtr name, IntPtr value);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RENDERDOC_GetThumbnail(IntPtr filename, byte[] outmem, ref UInt32 len);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_MakeEnvironmentModificationList(int numElems);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_SetEnvironmentModification(IntPtr mem, int idx, IntPtr variable, IntPtr value,
                                                                        EnvironmentModificationType type, EnvironmentSeparator separator);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_FreeEnvironmentModificationList(IntPtr mem);

        public static ReplaySupport SupportLocalReplay(string logfile, out string driverName, out string recordMachineIdent)
        {
            IntPtr name_mem = CustomMarshal.Alloc(typeof(templated_array));
            IntPtr ident_mem = CustomMarshal.Alloc(typeof(templated_array));

            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            ReplaySupport ret = RENDERDOC_SupportLocalReplay(logfile_mem, name_mem, ident_mem);

            CustomMarshal.Free(logfile_mem);

            driverName = CustomMarshal.TemplatedArrayToString(name_mem, true);
            recordMachineIdent = CustomMarshal.TemplatedArrayToString(ident_mem, true);

            CustomMarshal.Free(name_mem);
            CustomMarshal.Free(ident_mem);

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
                throw new ReplayCreateException(ret, "Failed to load log for local replay");
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

        public static UInt32 ExecuteAndInject(string app, string workingDir, string cmdLine, EnvironmentModification[] env, string logfile, CaptureOptions opts)
        {
            IntPtr app_mem = CustomMarshal.MakeUTF8String(app);
            IntPtr workingDir_mem = CustomMarshal.MakeUTF8String(workingDir);
            IntPtr cmdLine_mem = CustomMarshal.MakeUTF8String(cmdLine);
            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            IntPtr env_mem = RENDERDOC_MakeEnvironmentModificationList(env.Length);

            for(int i=0; i < env.Length; i++)
            {
                IntPtr var_mem = CustomMarshal.MakeUTF8String(env[i].variable);
                IntPtr val_mem = CustomMarshal.MakeUTF8String(env[i].value);

                RENDERDOC_SetEnvironmentModification(env_mem, i, var_mem, val_mem, env[i].type, env[i].separator);

                CustomMarshal.Free(var_mem);
                CustomMarshal.Free(val_mem);
            }

            UInt32 ret = RENDERDOC_ExecuteAndInject(app_mem, workingDir_mem, cmdLine_mem, env_mem, logfile_mem, opts, false);

            RENDERDOC_FreeEnvironmentModificationList(env_mem);

            CustomMarshal.Free(app_mem);
            CustomMarshal.Free(workingDir_mem);
            CustomMarshal.Free(cmdLine_mem);
            CustomMarshal.Free(logfile_mem);

            return ret;
        }

        public static UInt32 InjectIntoProcess(UInt32 pid, EnvironmentModification[] env, string logfile, CaptureOptions opts)
        {
            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            IntPtr env_mem = RENDERDOC_MakeEnvironmentModificationList(env.Length);

            for (int i = 0; i < env.Length; i++)
            {
                IntPtr var_mem = CustomMarshal.MakeUTF8String(env[i].variable);
                IntPtr val_mem = CustomMarshal.MakeUTF8String(env[i].value);

                RENDERDOC_SetEnvironmentModification(env_mem, i, var_mem, val_mem, env[i].type, env[i].separator);

                CustomMarshal.Free(var_mem);
                CustomMarshal.Free(val_mem);
            }

            UInt32 ret = RENDERDOC_InjectIntoProcess(pid, env_mem, logfile_mem, opts, false);

            RENDERDOC_FreeEnvironmentModificationList(env_mem);

            CustomMarshal.Free(logfile_mem);

            return ret;
        }

        public static TargetControl CreateTargetControl(string host, UInt32 ident, string clientName, bool forceConnection)
        {
            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);
            IntPtr clientName_mem = CustomMarshal.MakeUTF8String(clientName);

            IntPtr rendPtr = RENDERDOC_CreateTargetControl(host_mem, ident, clientName_mem, forceConnection);

            CustomMarshal.Free(host_mem);
            CustomMarshal.Free(clientName_mem);

            if (rendPtr == IntPtr.Zero)
            {
                throw new ReplayCreateException(ReplayCreateStatus.NetworkIOFailed, "Failed to open remote access connection");
            }

            return new TargetControl(rendPtr);
        }

        public delegate void RemoteConnectionFound(UInt32 ident);

        public static void EnumerateRemoteTargets(string host, RemoteConnectionFound callback)
        {
            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);

            UInt32 nextIdent = 0;

            while (true)
            {
                // just a sanity check to make sure we don't hit some unexpected case
                UInt32 prevIdent = nextIdent;

                nextIdent = RENDERDOC_EnumerateRemoteTargets(host_mem, nextIdent);

                if (nextIdent == UInt32.MaxValue || prevIdent >= nextIdent)
                    break;

                callback(nextIdent);
            }

            CustomMarshal.Free(host_mem);
        }

        public static RemoteServer CreateRemoteServer(string host, uint port)
        {
            IntPtr rendPtr = IntPtr.Zero;

            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);

            ReplayCreateStatus ret = RENDERDOC_CreateRemoteServerConnection(host_mem, port, ref rendPtr);

            CustomMarshal.Free(host_mem);

            if (rendPtr == IntPtr.Zero || ret != ReplayCreateStatus.Success)
            {
                throw new ReplayCreateException(ret, "Failed to connect to remote replay host");
            }

            return new RemoteServer(rendPtr);
        }

        public static void BecomeRemoteServer(string host, uint port, ref bool killReplay)
        {
            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);

            RENDERDOC_BecomeRemoteServer(host_mem, port, ref killReplay);

            CustomMarshal.Free(host_mem);
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

        public static string GetVersionString()
        {
            return CustomMarshal.PtrToStringUTF8(RENDERDOC_GetVersionString());
        }

        public static string GetConfigSetting(string name)
        {
            IntPtr name_mem = CustomMarshal.MakeUTF8String(name);

            string ret = CustomMarshal.PtrToStringUTF8(RENDERDOC_GetConfigSetting(name_mem));

            CustomMarshal.Free(name_mem);

            return ret;
        }

        public static void SetConfigSetting(string name, string value)
        {
            IntPtr name_mem = CustomMarshal.MakeUTF8String(name);
            IntPtr value_mem = CustomMarshal.MakeUTF8String(value);

            RENDERDOC_SetConfigSetting(name_mem, value_mem);

            CustomMarshal.Free(name_mem);
            CustomMarshal.Free(value_mem);
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
