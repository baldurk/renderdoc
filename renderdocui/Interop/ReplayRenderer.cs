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
using System.Collections.Generic;

namespace renderdoc
{
    [StructLayout(LayoutKind.Sequential)]
    public class RemoteMessage
    {
        public RemoteMessageType Type;

        [StructLayout(LayoutKind.Sequential)]
        public struct NewCaptureData
        {
            public UInt32 ID;
            public UInt64 timestamp;
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public byte[] thumbnail;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string localpath;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public NewCaptureData NewCapture;

        [StructLayout(LayoutKind.Sequential)]
        public struct RegisterAPIData
        {
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string APIName;
        };

        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public RegisterAPIData RegisterAPI;

        [StructLayout(LayoutKind.Sequential)]
        public struct BusyData
        {
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string ClientName;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public BusyData Busy;

        [StructLayout(LayoutKind.Sequential)]
        public struct NewChildData
        {
            public UInt32 PID;
            public UInt32 ident;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public NewChildData NewChild;
    };

    public class ReplayOutput
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_SetOutputConfig(IntPtr real, OutputConfig o);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_SetTextureDisplay(IntPtr real, TextureDisplay o);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_SetMeshDisplay(IntPtr real, MeshDisplay o);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_ClearThumbnails(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_AddThumbnail(IntPtr real, IntPtr wnd, ResourceId texID);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_Display(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_SetPixelContext(IntPtr real, IntPtr wnd);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_SetPixelContextLocation(IntPtr real, UInt32 x, UInt32 y);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_DisablePixelContext(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_PickPixel(IntPtr real, ResourceId texID, bool customShader,
                                                                UInt32 x, UInt32 y, UInt32 sliceFace, UInt32 mip, UInt32 sample, IntPtr outval);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 ReplayOutput_PickVertex(IntPtr real, UInt32 frameID, UInt32 eventID, UInt32 x, UInt32 y);

        private IntPtr m_Real = IntPtr.Zero;

        public ReplayOutput(IntPtr real) { m_Real = real; }

        public bool SetOutputConfig(OutputConfig o)
        {
            return ReplayOutput_SetOutputConfig(m_Real, o);
        }
        public bool SetTextureDisplay(TextureDisplay o)
        {
            return ReplayOutput_SetTextureDisplay(m_Real, o);
        }
        public bool SetMeshDisplay(MeshDisplay o)
        {
            return ReplayOutput_SetMeshDisplay(m_Real, o);
        }

        public bool ClearThumbnails()
        {
            return ReplayOutput_ClearThumbnails(m_Real);
        }
        public bool AddThumbnail(IntPtr wnd, ResourceId texID)
        {
            return ReplayOutput_AddThumbnail(m_Real, wnd, texID);
        }

        public bool Display()
        {
            return ReplayOutput_Display(m_Real);
        }

        public bool SetPixelContext(IntPtr wnd)
        {
            return ReplayOutput_SetPixelContext(m_Real, wnd);
        }
        public bool SetPixelContextLocation(UInt32 x, UInt32 y)
        {
            return ReplayOutput_SetPixelContextLocation(m_Real, x, y);
        }
        public void DisablePixelContext()
        {
            ReplayOutput_DisablePixelContext(m_Real);
        }

        public PixelValue PickPixel(ResourceId texID, bool customShader, UInt32 x, UInt32 y, UInt32 sliceFace, UInt32 mip, UInt32 sample)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(PixelValue));

            bool success = ReplayOutput_PickPixel(m_Real, texID, customShader, x, y, sliceFace, mip, sample, mem);

            PixelValue ret = null;
            
            if(success)
                ret = (PixelValue)CustomMarshal.PtrToStructure(mem, typeof(PixelValue), false);

            CustomMarshal.Free(mem);

            return ret;
        }

        public UInt32 PickVertex(UInt32 frameID, UInt32 eventID, UInt32 x, UInt32 y)
        {
            return ReplayOutput_PickVertex(m_Real, frameID, eventID,  x, y);
        }

    };

    public class ReplayRenderer
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_Shutdown(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetAPIProperties(IntPtr real, IntPtr propsOut);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ReplayRenderer_CreateOutput(IntPtr real, IntPtr WindowHandle);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_ShutdownOutput(IntPtr real, IntPtr replayOutput);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_FileChanged(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_HasCallstacks(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_InitResolver(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_SetContextFilter(IntPtr real, ResourceId id, UInt32 firstDefEv, UInt32 lastDefEv);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_SetFrameEvent(IntPtr real, UInt32 frameID, UInt32 eventID);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetD3D11PipelineState(IntPtr real, IntPtr mem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetGLPipelineState(IntPtr real, IntPtr mem);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_BuildCustomShader(IntPtr real, IntPtr entry, IntPtr source, UInt32 compileFlags, ShaderStageType type, ref ResourceId shaderID, IntPtr errorMem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_FreeCustomShader(IntPtr real, ResourceId id);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_BuildTargetShader(IntPtr real, IntPtr entry, IntPtr source, UInt32 compileFlags, ShaderStageType type, ref ResourceId shaderID, IntPtr errorMem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_ReplaceResource(IntPtr real, ResourceId from, ResourceId to);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_RemoveReplacement(IntPtr real, ResourceId id);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_FreeTargetResource(IntPtr real, ResourceId id);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetFrameInfo(IntPtr real, IntPtr outframe);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetDrawcalls(IntPtr real, UInt32 frameID, IntPtr outdraws);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_FetchCounters(IntPtr real, UInt32 frameID, UInt32 minEventID, UInt32 maxEventID, IntPtr counters, UInt32 numCounters, IntPtr outresults);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_EnumerateCounters(IntPtr real, IntPtr outcounters);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_DescribeCounter(IntPtr real, UInt32 counter, IntPtr outdesc);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetTextures(IntPtr real, IntPtr outtexs);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetBuffers(IntPtr real, IntPtr outbufs);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetResolve(IntPtr real, UInt64[] callstack, UInt32 callstackLen, IntPtr outtrace);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ReplayRenderer_GetShaderDetails(IntPtr real, ResourceId shader);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetDebugMessages(IntPtr real, IntPtr outmsgs);
        
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_PixelHistory(IntPtr real, ResourceId target, UInt32 x, UInt32 y, UInt32 slice, UInt32 mip, UInt32 sampleIdx, IntPtr history);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_DebugVertex(IntPtr real, UInt32 vertid, UInt32 instid, UInt32 idx, UInt32 instOffset, UInt32 vertOffset, IntPtr outtrace);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_DebugPixel(IntPtr real, UInt32 x, UInt32 y, UInt32 sample, UInt32 primitive, IntPtr outtrace);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_DebugThread(IntPtr real, UInt32[] groupid, UInt32[] threadid, IntPtr outtrace);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetUsage(IntPtr real, ResourceId id, IntPtr outusage);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetCBufferVariableContents(IntPtr real, ResourceId shader, UInt32 cbufslot, ResourceId buffer, UInt32 offs, IntPtr outvars);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_SaveTexture(IntPtr real, TextureSave saveData, IntPtr path);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetPostVSData(IntPtr real, UInt32 instID, MeshDataStage stage, IntPtr outdata);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetMinMax(IntPtr real, ResourceId tex, UInt32 sliceFace, UInt32 mip, UInt32 sample, IntPtr outminval, IntPtr outmaxval);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetHistogram(IntPtr real, ResourceId tex, UInt32 sliceFace, UInt32 mip, UInt32 sample, float minval, float maxval, bool[] channels, IntPtr outhistogram);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetBufferData(IntPtr real, ResourceId buff, UInt32 offset, UInt32 len, IntPtr outdata);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_GetTextureData(IntPtr real, ResourceId tex, UInt32 arrayIdx, UInt32 mip, IntPtr outdata);

        private IntPtr m_Real = IntPtr.Zero;

        public ReplayRenderer(IntPtr real) { m_Real = real; }

        public void Shutdown()
        {
            if (m_Real != IntPtr.Zero)
            {
                ReplayRenderer_Shutdown(m_Real);
                m_Real = IntPtr.Zero;
            }
        }

        public APIProperties GetAPIProperties()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(APIProperties));

            ReplayRenderer_GetAPIProperties(m_Real, mem);

            APIProperties ret = (APIProperties)CustomMarshal.PtrToStructure(mem, typeof(APIProperties), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ReplayOutput CreateOutput(IntPtr WindowHandle)
        {
            IntPtr ret = ReplayRenderer_CreateOutput(m_Real, WindowHandle);

            if (ret == IntPtr.Zero)
                return null;

            return new ReplayOutput(ret);
        }

        public void FileChanged()
        { ReplayRenderer_FileChanged(m_Real); }

        public bool HasCallstacks()
        { return ReplayRenderer_HasCallstacks(m_Real); }

        public bool InitResolver()
        { return ReplayRenderer_InitResolver(m_Real); }

        public bool SetContextFilter(ResourceId id, UInt32 firstDefEv, UInt32 lastDefEv)
        { return ReplayRenderer_SetContextFilter(m_Real, id, firstDefEv, lastDefEv); }
        public bool SetFrameEvent(UInt32 frameID, UInt32 eventID)
        { return ReplayRenderer_SetFrameEvent(m_Real, frameID, eventID); }

        public GLPipelineState GetGLPipelineState()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(GLPipelineState));

            bool success = ReplayRenderer_GetGLPipelineState(m_Real, mem);

            GLPipelineState ret = null;

            if (success)
                ret = (GLPipelineState)CustomMarshal.PtrToStructure(mem, typeof(GLPipelineState), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public D3D11PipelineState GetD3D11PipelineState()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(D3D11PipelineState));

            bool success = ReplayRenderer_GetD3D11PipelineState(m_Real, mem);

            D3D11PipelineState ret = null;

            if (success)
                ret = (D3D11PipelineState)CustomMarshal.PtrToStructure(mem, typeof(D3D11PipelineState), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ResourceId BuildCustomShader(string entry, string source, UInt32 compileFlags, ShaderStageType type, out string errors)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ResourceId ret = ResourceId.Null;

            IntPtr entry_mem = CustomMarshal.MakeUTF8String(entry);
            IntPtr source_mem = CustomMarshal.MakeUTF8String(source);

            ReplayRenderer_BuildCustomShader(m_Real, entry_mem, source_mem, compileFlags, type, ref ret, mem);

            CustomMarshal.Free(entry_mem);
            CustomMarshal.Free(source_mem);

            errors = CustomMarshal.TemplatedArrayToString(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public bool FreeCustomShader(ResourceId id)
        { return ReplayRenderer_FreeCustomShader(m_Real, id); }

        public ResourceId BuildTargetShader(string entry, string source, UInt32 compileFlags, ShaderStageType type, out string errors)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ResourceId ret = ResourceId.Null;

            IntPtr entry_mem = CustomMarshal.MakeUTF8String(entry);
            IntPtr source_mem = CustomMarshal.MakeUTF8String(source);

            ReplayRenderer_BuildTargetShader(m_Real, entry_mem, source_mem, compileFlags, type, ref ret, mem);

            CustomMarshal.Free(entry_mem);
            CustomMarshal.Free(source_mem);

            errors = CustomMarshal.TemplatedArrayToString(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public bool ReplaceResource(ResourceId from, ResourceId to)
        { return ReplayRenderer_ReplaceResource(m_Real, from, to); }
        public bool RemoveReplacement(ResourceId id)
        { return ReplayRenderer_RemoveReplacement(m_Real, id); }
        public bool FreeTargetResource(ResourceId id)
        { return ReplayRenderer_FreeTargetResource(m_Real, id); }

        public FetchFrameInfo[] GetFrameInfo()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetFrameInfo(m_Real, mem);

            FetchFrameInfo[] ret = null;

            if (success)
                ret = (FetchFrameInfo[])CustomMarshal.GetTemplatedArray(mem, typeof(FetchFrameInfo), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        private void PopulateDraws(ref Dictionary<Int64, FetchDrawcall> map, FetchDrawcall[] draws)
        {
            if (draws.Length == 0) return;

            foreach (var d in draws)
            {
                map.Add((Int64)d.eventID, d);
                PopulateDraws(ref map, d.children);
            }
        }

        private void FixupDraws(Dictionary<Int64, FetchDrawcall> map, FetchDrawcall[] draws)
        {
            if (draws.Length == 0) return;

            foreach (var d in draws)
            {
                if (d.previousDrawcall != 0 && map.ContainsKey(d.previousDrawcall)) d.previous = map[d.previousDrawcall];
                if (d.nextDrawcall != 0 && map.ContainsKey(d.nextDrawcall)) d.next = map[d.nextDrawcall];
                if (d.parentDrawcall != 0 && map.ContainsKey(d.parentDrawcall)) d.parent = map[d.parentDrawcall];

                FixupDraws(map, d.children);
            }
        }

        public Dictionary<uint, List<CounterResult>> FetchCounters(UInt32 frameID, UInt32 minEventID, UInt32 maxEventID, UInt32[] counters)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            IntPtr countersmem = CustomMarshal.Alloc(typeof(UInt32), counters.Length);

            // there's no Marshal.Copy for uint[], which is stupid.
            for (int i = 0; i < counters.Length; i++)
                Marshal.WriteInt32(countersmem, sizeof(UInt32) * i, (int)counters[i]);

            bool success = ReplayRenderer_FetchCounters(m_Real, frameID, minEventID, maxEventID, countersmem, (uint)counters.Length, mem);

            CustomMarshal.Free(countersmem);

            Dictionary<uint, List<CounterResult>> ret = null;

            if (success)
            {
                CounterResult[] resultArray = (CounterResult[])CustomMarshal.GetTemplatedArray(mem, typeof(CounterResult), true);

                // fixup previous/next/parent pointers
                ret = new Dictionary<uint, List<CounterResult>>();

                foreach (var result in resultArray)
                {
                    if (!ret.ContainsKey(result.eventID))
                        ret.Add(result.eventID, new List<CounterResult>());

                    ret[result.eventID].Add(result);
                }
            }

            CustomMarshal.Free(mem);

            return ret;
        }

        public UInt32[] EnumerateCounters()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_EnumerateCounters(m_Real, mem);

            UInt32[] ret = null;

            if (success)
            {
                ret = (UInt32[])CustomMarshal.GetTemplatedArray(mem, typeof(UInt32), true);
            }

            CustomMarshal.Free(mem);

            return ret;
        }

        public CounterDescription DescribeCounter(UInt32 counterID)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(CounterDescription));
            
            bool success = ReplayRenderer_DescribeCounter(m_Real, counterID, mem);

            CounterDescription ret = null;

            if (success)
            {
                ret = (CounterDescription)CustomMarshal.PtrToStructure(mem, typeof(CounterDescription), false);
            }

            CustomMarshal.Free(mem);

            return ret;
        }

        public FetchDrawcall[] GetDrawcalls(UInt32 frameID)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetDrawcalls(m_Real, frameID, mem);

            FetchDrawcall[] ret = null;

            if (success)
            {
                ret = (FetchDrawcall[])CustomMarshal.GetTemplatedArray(mem, typeof(FetchDrawcall), true);

                // fixup previous/next/parent pointers
                var map = new Dictionary<Int64, FetchDrawcall>();
                PopulateDraws(ref map, ret);
                FixupDraws(map, ret);
            }

            CustomMarshal.Free(mem);

            return ret;
        }

        public FetchTexture[] GetTextures()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetTextures(m_Real, mem);

            FetchTexture[] ret = null;

            if (success)
                ret = (FetchTexture[])CustomMarshal.GetTemplatedArray(mem, typeof(FetchTexture), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public FetchBuffer[] GetBuffers()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetBuffers(m_Real, mem);

            FetchBuffer[] ret = null;

            if (success)
                ret = (FetchBuffer[])CustomMarshal.GetTemplatedArray(mem, typeof(FetchBuffer), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public string[] GetResolve(UInt64[] callstack)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            UInt32 len = (UInt32)callstack.Length;

            bool success = ReplayRenderer_GetResolve(m_Real, callstack, len, mem);

            string[] ret = null;

            if (success)
                ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderReflection GetShaderDetails(ResourceId shader)
        {
            IntPtr mem = ReplayRenderer_GetShaderDetails(m_Real, shader);

            ShaderReflection ret = null;

            if (mem != IntPtr.Zero)
                ret = (ShaderReflection)CustomMarshal.PtrToStructure(mem, typeof(ShaderReflection), false);

            return ret;
        }

        public DebugMessage[] GetDebugMessages()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetDebugMessages(m_Real, mem);

            DebugMessage[] ret = null;

            if (success)
                ret = (DebugMessage[])CustomMarshal.GetTemplatedArray(mem, typeof(DebugMessage), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public PixelModification[] PixelHistory(ResourceId target, UInt32 x, UInt32 y, UInt32 slice, UInt32 mip, UInt32 sampleIdx)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_PixelHistory(m_Real, target, x, y, slice, mip, sampleIdx, mem);

            PixelModification[] ret = null;

            if (success)
                ret = (PixelModification[])CustomMarshal.GetTemplatedArray(mem, typeof(PixelModification), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderDebugTrace DebugVertex(UInt32 vertid, UInt32 instid, UInt32 idx, UInt32 instOffset, UInt32 vertOffset)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(ShaderDebugTrace));

            bool success = ReplayRenderer_DebugVertex(m_Real, vertid, instid, idx, instOffset, vertOffset, mem);

            ShaderDebugTrace ret = null;

            if (success)
                ret = (ShaderDebugTrace)CustomMarshal.PtrToStructure(mem, typeof(ShaderDebugTrace), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderDebugTrace DebugPixel(UInt32 x, UInt32 y, UInt32 sample, UInt32 primitive)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(ShaderDebugTrace));

            bool success = ReplayRenderer_DebugPixel(m_Real, x, y, sample, primitive, mem);

            ShaderDebugTrace ret = null;

            if (success)
                ret = (ShaderDebugTrace)CustomMarshal.PtrToStructure(mem, typeof(ShaderDebugTrace), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderDebugTrace DebugThread(UInt32[] groupid, UInt32[] threadid)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(ShaderDebugTrace));

            bool success = ReplayRenderer_DebugThread(m_Real, groupid, threadid, mem);

            ShaderDebugTrace ret = null;

            if (success)
                ret = (ShaderDebugTrace)CustomMarshal.PtrToStructure(mem, typeof(ShaderDebugTrace), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public EventUsage[] GetUsage(ResourceId id)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetUsage(m_Real, id, mem);

            EventUsage[] ret = null;

            if (success)
                ret = (EventUsage[])CustomMarshal.GetTemplatedArray(mem, typeof(EventUsage), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderVariable[] GetCBufferVariableContents(ResourceId shader, UInt32 cbufslot, ResourceId buffer, UInt32 offs)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetCBufferVariableContents(m_Real, shader, cbufslot, buffer, offs, mem);

            ShaderVariable[] ret = null;

            if (success)
                ret = (ShaderVariable[])CustomMarshal.GetTemplatedArray(mem, typeof(ShaderVariable), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public bool SaveTexture(TextureSave saveData, string path)
        {
            IntPtr path_mem = CustomMarshal.MakeUTF8String(path);

            bool ret = ReplayRenderer_SaveTexture(m_Real, saveData, path_mem);

            CustomMarshal.Free(path_mem);

            return ret;
        }

        public MeshFormat GetPostVSData(UInt32 instID, MeshDataStage stage)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(MeshFormat));

            MeshFormat ret = new MeshFormat();
            ret.buf = ResourceId.Null;

            bool success = ReplayRenderer_GetPostVSData(m_Real, instID, stage, mem);

            if (success)
                ret = (MeshFormat)CustomMarshal.PtrToStructure(mem, typeof(MeshFormat), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public bool GetMinMax(ResourceId tex, UInt32 sliceFace, UInt32 mip, UInt32 sample, out PixelValue minval, out PixelValue maxval)
        {
            IntPtr mem1 = CustomMarshal.Alloc(typeof(PixelValue));
            IntPtr mem2 = CustomMarshal.Alloc(typeof(PixelValue));

            bool success = ReplayRenderer_GetMinMax(m_Real, tex, sliceFace, mip, sample, mem1, mem2);

            if (success)
            {
                minval = (PixelValue)CustomMarshal.PtrToStructure(mem1, typeof(PixelValue), true);
                maxval = (PixelValue)CustomMarshal.PtrToStructure(mem2, typeof(PixelValue), true);
            }
            else
            {
                minval = null;
                maxval = null;
            }

            CustomMarshal.Free(mem1);
            CustomMarshal.Free(mem2);

            return success;
        }

        public bool GetHistogram(ResourceId tex, UInt32 sliceFace, UInt32 mip, UInt32 sample, float minval, float maxval,
                                 bool Red, bool Green, bool Blue, bool Alpha,
                                 out UInt32[] histogram)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool[] channels = new bool[] { Red, Green, Blue, Alpha };

            bool success = ReplayRenderer_GetHistogram(m_Real, tex, sliceFace, mip, sample, minval, maxval, channels, mem);

            histogram = null;

            if (success)
                histogram = (UInt32[])CustomMarshal.GetTemplatedArray(mem, typeof(UInt32), true);

            CustomMarshal.Free(mem);

            return success;
        }

        public byte[] GetBufferData(ResourceId buff, UInt32 offset, UInt32 len)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetBufferData(m_Real, buff, offset, len, mem);

            byte[] ret = null;

            if (success)
                ret = (byte[])CustomMarshal.GetTemplatedArray(mem, typeof(byte), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public byte[] GetTextureData(ResourceId tex, UInt32 arrayIdx, UInt32 mip)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool success = ReplayRenderer_GetTextureData(m_Real, tex, arrayIdx, mip, mem);

            byte[] ret = null;

            if (success)
                ret = (byte[])CustomMarshal.GetTemplatedArray(mem, typeof(byte), true);

            CustomMarshal.Free(mem);

            return ret;
        }
    };

    public class RemoteRenderer
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteRenderer_Shutdown(IntPtr real);
        
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RemoteRenderer_LocalProxies(IntPtr real, IntPtr outlist);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RemoteRenderer_RemoteSupportedReplays(IntPtr real, IntPtr outlist);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RemoteRenderer_CreateProxyRenderer(IntPtr real, UInt32 proxyid, IntPtr logfile, ref float progress, ref IntPtr rendPtr);

        private IntPtr m_Real = IntPtr.Zero;

        public RemoteRenderer(IntPtr real) { m_Real = real; }

        public void Shutdown()
        {
            if (m_Real != IntPtr.Zero)
            {
                RemoteRenderer_Shutdown(m_Real);
                m_Real = IntPtr.Zero;
            }
        }

        public string[] LocalProxies()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));
            bool success = RemoteRenderer_LocalProxies(m_Real, mem);

            string[] ret = null;

            if (success)
                ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public string[] RemoteSupportedReplays()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));
            bool success = RemoteRenderer_RemoteSupportedReplays(m_Real, mem);

            string[] ret = null;

            if (success)
                ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ReplayRenderer CreateProxyRenderer(int proxyid, string logfile, ref float progress)
        {
            IntPtr rendPtr = IntPtr.Zero;

            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            ReplayCreateStatus ret = RemoteRenderer_CreateProxyRenderer(m_Real, (UInt32)proxyid, logfile_mem, ref progress, ref rendPtr);

            CustomMarshal.Free(logfile_mem);

            if (rendPtr == IntPtr.Zero || ret != ReplayCreateStatus.Success)
            {
                var e = new System.ApplicationException("Failed to set up local proxy replay with remote connection");
                e.Data.Add("status", ret);
                throw e;
            }

            return new ReplayRenderer(rendPtr);
        }
    };

    public class RemoteAccess
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteAccess_Shutdown(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RemoteAccess_GetTarget(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RemoteAccess_GetAPI(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RemoteAccess_GetPID(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RemoteAccess_GetBusyClient(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteAccess_TriggerCapture(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteAccess_QueueCapture(IntPtr real, UInt32 frameNumber);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteAccess_CopyCapture(IntPtr real, UInt32 remoteID, IntPtr localpath);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteAccess_ReceiveMessage(IntPtr real, IntPtr outmsg);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_EnumerateRemoteConnections(IntPtr host, UInt32[] idents);

        private IntPtr m_Real = IntPtr.Zero;
        private bool m_Connected;

        public RemoteAccess(IntPtr real)
        {
            m_Real = real;

            if (real == IntPtr.Zero)
            {
                m_Connected = false;
                Target = "";
                API = "";
                BusyClient = "";
            }
            else
            {
                m_Connected = true;
                Target = CustomMarshal.PtrToStringUTF8(RemoteAccess_GetTarget(m_Real));
                API = CustomMarshal.PtrToStringUTF8(RemoteAccess_GetAPI(m_Real));
                PID = RemoteAccess_GetPID(m_Real);
                BusyClient = CustomMarshal.PtrToStringUTF8(RemoteAccess_GetBusyClient(m_Real));
            }

            CaptureExists = false;
            CaptureCopied = false;
            InfoUpdated = false;
        }

        public static UInt32[] GetRemoteIdents(string host)
        {
            UInt32 numIdents = RENDERDOC_EnumerateRemoteConnections(IntPtr.Zero, null);

            UInt32[] idents = new UInt32[numIdents];

            IntPtr host_mem = CustomMarshal.MakeUTF8String(host);

            RENDERDOC_EnumerateRemoteConnections(host_mem, idents);

            CustomMarshal.Free(host_mem);

            return idents;
        }

        public bool Connected { get { return m_Connected; } }

        public void Shutdown()
        {
            m_Connected = false;
            if (m_Real != IntPtr.Zero) RemoteAccess_Shutdown(m_Real);
            m_Real = IntPtr.Zero;
        }

        public void TriggerCapture()
        {
            RemoteAccess_TriggerCapture(m_Real);
        }

        public void QueueCapture(UInt32 frameNum)
        {
            RemoteAccess_QueueCapture(m_Real, frameNum);
        }

        public void CopyCapture(UInt32 id, string localpath)
        {
            IntPtr localpath_mem = CustomMarshal.MakeUTF8String(localpath);

            RemoteAccess_CopyCapture(m_Real, id, localpath_mem);

            CustomMarshal.Free(localpath_mem);
        }

        public void ReceiveMessage()
        {
            if (m_Real != IntPtr.Zero)
            {
                RemoteMessage msg = null;

                {
                    IntPtr mem = CustomMarshal.Alloc(typeof(RemoteMessage));

                    RemoteAccess_ReceiveMessage(m_Real, mem);

                    if (mem != IntPtr.Zero)
                        msg = (RemoteMessage)CustomMarshal.PtrToStructure(mem, typeof(RemoteMessage), true);

                    CustomMarshal.Free(mem);
                }

                if (msg.Type == RemoteMessageType.Disconnected)
                {
                    m_Connected = false;
                    RemoteAccess_Shutdown(m_Real);
                    m_Real = IntPtr.Zero;
                }
                else if (msg.Type == RemoteMessageType.NewCapture)
                {
                    CaptureFile = msg.NewCapture;
                    CaptureExists = true;
                }
                else if (msg.Type == RemoteMessageType.CaptureCopied)
                {
                    CaptureFile.ID = msg.NewCapture.ID;
                    CaptureFile.localpath = msg.NewCapture.localpath;
                    CaptureCopied = true;
                }
                else if (msg.Type == RemoteMessageType.RegisterAPI)
                {
                    API = msg.RegisterAPI.APIName;
                    InfoUpdated = true;
                }
                else if (msg.Type == RemoteMessageType.NewChild)
                {
                    NewChild = msg.NewChild;
                    ChildAdded = true;
                }
            }
        }

        public string BusyClient;
        public string Target;
        public string API;
        public UInt32 PID;

        public bool CaptureExists;
        public bool ChildAdded;
        public bool CaptureCopied;
        public bool InfoUpdated;

        public RemoteMessage.NewCaptureData CaptureFile = new RemoteMessage.NewCaptureData();

        public RemoteMessage.NewChildData NewChild = new RemoteMessage.NewChildData();
    };
};
