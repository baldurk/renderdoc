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
using System.Runtime.InteropServices;
using System.Collections.Generic;

namespace renderdoc
{
    // this isn't an Interop struct, since it needs to be passed C# -> C++ and
    // it's not POD which we don't support. It's here just as a utility container
    public class EnvironmentModification
    {
        public string variable;
        public string value;

        public EnvironmentModificationType type;
        public EnvironmentSeparator separator;

        public string GetTypeString()
        {
            string ret;

            if (type == EnvironmentModificationType.Append)
                ret = String.Format("Append, {0}", separator.Str());
            else if (type == EnvironmentModificationType.Prepend)
                ret = String.Format("Prepend, {0}", separator.Str());
            else
                ret = "Set";

            return ret;
        }

        public string GetDescription()
        {
            string ret;

            if (type == EnvironmentModificationType.Append)
                ret = String.Format("Append {0} with {1} using {2}", variable, value, separator.Str());
            else if (type == EnvironmentModificationType.Prepend)
                ret = String.Format("Prepend {0} with {1} using {2}", variable, value, separator.Str());
            else
                ret = String.Format("Set {0} to {1}", variable, value);

            return ret;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public class TargetControlMessage
    {
        public TargetControlMessageType Type;

        [StructLayout(LayoutKind.Sequential)]
        public struct NewCaptureData
        {
            public UInt32 ID;
            public UInt64 timestamp;
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public byte[] thumbnail;
            public Int32 thumbWidth;
            public Int32 thumbHeight;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string path;
            public bool local;
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
        private static extern void ReplayOutput_SetTextureDisplay(IntPtr real, TextureDisplay o);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_SetMeshDisplay(IntPtr real, MeshDisplay o);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_ClearThumbnails(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_AddThumbnail(IntPtr real, UInt32 windowSystem, IntPtr wnd, ResourceId texID, FormatComponentType typeHint);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_Display(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayOutput_SetPixelContext(IntPtr real, UInt32 windowSystem, IntPtr wnd);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_SetPixelContextLocation(IntPtr real, UInt32 x, UInt32 y);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_DisablePixelContext(IntPtr real);
        
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_GetCustomShaderTexID(IntPtr real, ref ResourceId texid);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_GetDebugOverlayTexID(IntPtr real, ref ResourceId texid);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_GetMinMax(IntPtr real, IntPtr outminval, IntPtr outmaxval);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_GetHistogram(IntPtr real, float minval, float maxval, bool[] channels, IntPtr outhistogram);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayOutput_PickPixel(IntPtr real, ResourceId texID, bool customShader,
                                                                UInt32 x, UInt32 y, UInt32 sliceFace, UInt32 mip, UInt32 sample, IntPtr outval);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 ReplayOutput_PickVertex(IntPtr real, UInt32 eventID, UInt32 x, UInt32 y, IntPtr outPickedInstance);

        private IntPtr m_Real = IntPtr.Zero;

        public ReplayOutput(IntPtr real) { m_Real = real; }

        public void SetTextureDisplay(TextureDisplay o)
        {
            ReplayOutput_SetTextureDisplay(m_Real, o);
        }
        public void SetMeshDisplay(MeshDisplay o)
        {
            ReplayOutput_SetMeshDisplay(m_Real, o);
        }

        public void ClearThumbnails()
        {
            ReplayOutput_ClearThumbnails(m_Real);
        }
        public bool AddThumbnail(IntPtr wnd, ResourceId texID, FormatComponentType typeHint)
        {
            // 1 == eWindowingSystem_Win32
            return ReplayOutput_AddThumbnail(m_Real, 1u, wnd, texID, typeHint);
        }

        public void Display()
        {
            ReplayOutput_Display(m_Real);
        }

        public bool SetPixelContext(IntPtr wnd)
        {
            // 1 == eWindowingSystem_Win32
            return ReplayOutput_SetPixelContext(m_Real, 1u, wnd);
        }
        public void SetPixelContextLocation(UInt32 x, UInt32 y)
        {
            ReplayOutput_SetPixelContextLocation(m_Real, x, y);
        }
        public void DisablePixelContext()
        {
            ReplayOutput_DisablePixelContext(m_Real);
        }

        public ResourceId GetCustomShaderTexID()
        {
            ResourceId ret = ResourceId.Null;

            ReplayOutput_GetCustomShaderTexID(m_Real, ref ret);

            return ret;
        }

        public ResourceId GetDebugOverlayTexID()
        {
            ResourceId ret = ResourceId.Null;

            ReplayOutput_GetDebugOverlayTexID(m_Real, ref ret);

            return ret;
        }

        public void GetMinMax(out PixelValue minval, out PixelValue maxval)
        {
            IntPtr mem1 = CustomMarshal.Alloc(typeof(PixelValue));
            IntPtr mem2 = CustomMarshal.Alloc(typeof(PixelValue));

            ReplayOutput_GetMinMax(m_Real, mem1, mem2);

            minval = (PixelValue)CustomMarshal.PtrToStructure(mem1, typeof(PixelValue), true);
            maxval = (PixelValue)CustomMarshal.PtrToStructure(mem2, typeof(PixelValue), true);

            CustomMarshal.Free(mem1);
            CustomMarshal.Free(mem2);
        }

        public void GetHistogram(float minval, float maxval,
                                 bool Red, bool Green, bool Blue, bool Alpha,
                                 out UInt32[] histogram)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            bool[] channels = new bool[] { Red, Green, Blue, Alpha };

            ReplayOutput_GetHistogram(m_Real, minval, maxval, channels, mem);

            histogram = (UInt32[])CustomMarshal.GetTemplatedArray(mem, typeof(UInt32), true);

            CustomMarshal.Free(mem);
        }

        public PixelValue PickPixel(ResourceId texID, bool customShader, UInt32 x, UInt32 y, UInt32 sliceFace, UInt32 mip, UInt32 sample)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(PixelValue));

            ReplayOutput_PickPixel(m_Real, texID, customShader, x, y, sliceFace, mip, sample, mem);

            PixelValue ret = (PixelValue)CustomMarshal.PtrToStructure(mem, typeof(PixelValue), false);

            CustomMarshal.Free(mem);

            return ret;
        }

        public UInt32 PickVertex(UInt32 eventID, UInt32 x, UInt32 y, out UInt32 pickedInstance)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(UInt32));

            UInt32 pickedVertex = ReplayOutput_PickVertex(m_Real, eventID,  x, y, mem);
            pickedInstance = (UInt32)CustomMarshal.PtrToStructure(mem, typeof(UInt32), true);

            CustomMarshal.Free(mem);

            return pickedVertex;
        }

    };

    public class ReplayRenderer
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_Shutdown(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetAPIProperties(IntPtr real, IntPtr propsOut);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ReplayRenderer_CreateOutput(IntPtr real, UInt32 windowSystem, IntPtr WindowHandle, OutputType type);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_ShutdownOutput(IntPtr real, IntPtr replayOutput);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_FileChanged(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_HasCallstacks(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_InitResolver(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_SetFrameEvent(IntPtr real, UInt32 eventID, bool force);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetD3D11PipelineState(IntPtr real, IntPtr mem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetD3D12PipelineState(IntPtr real, IntPtr mem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetGLPipelineState(IntPtr real, IntPtr mem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetVulkanPipelineState(IntPtr real, IntPtr mem);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetDisassemblyTargets(IntPtr real, IntPtr targets);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_DisassembleShader(IntPtr real, IntPtr refl, IntPtr target, IntPtr isa);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_BuildCustomShader(IntPtr real, IntPtr entry, IntPtr source, UInt32 compileFlags, ShaderStageType type, ref ResourceId shaderID, IntPtr errorMem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_FreeCustomShader(IntPtr real, ResourceId id);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_BuildTargetShader(IntPtr real, IntPtr entry, IntPtr source, UInt32 compileFlags, ShaderStageType type, ref ResourceId shaderID, IntPtr errorMem);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_ReplaceResource(IntPtr real, ResourceId from, ResourceId to);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_RemoveReplacement(IntPtr real, ResourceId id);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_FreeTargetResource(IntPtr real, ResourceId id);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetFrameInfo(IntPtr real, IntPtr outframe);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetDrawcalls(IntPtr real, IntPtr outdraws);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_FetchCounters(IntPtr real, IntPtr counters, UInt32 numCounters, IntPtr outresults);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_EnumerateCounters(IntPtr real, IntPtr outcounters);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_DescribeCounter(IntPtr real, UInt32 counter, IntPtr outdesc);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetTextures(IntPtr real, IntPtr outtexs);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetBuffers(IntPtr real, IntPtr outbufs);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetResolve(IntPtr real, UInt64[] callstack, UInt32 callstackLen, IntPtr outtrace);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetDebugMessages(IntPtr real, IntPtr outmsgs);
        
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_PixelHistory(IntPtr real, ResourceId target, UInt32 x, UInt32 y, UInt32 slice, UInt32 mip, UInt32 sampleIdx, FormatComponentType typeHint, IntPtr history);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_DebugVertex(IntPtr real, UInt32 vertid, UInt32 instid, UInt32 idx, UInt32 instOffset, UInt32 vertOffset, IntPtr outtrace);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_DebugPixel(IntPtr real, UInt32 x, UInt32 y, UInt32 sample, UInt32 primitive, IntPtr outtrace);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_DebugThread(IntPtr real, UInt32[] groupid, UInt32[] threadid, IntPtr outtrace);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetUsage(IntPtr real, ResourceId id, IntPtr outusage);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetCBufferVariableContents(IntPtr real, ResourceId shader, IntPtr entryPoint, UInt32 cbufslot, ResourceId buffer, UInt64 offs, IntPtr outvars);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool ReplayRenderer_SaveTexture(IntPtr real, TextureSave saveData, IntPtr path);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetPostVSData(IntPtr real, UInt32 instID, MeshDataStage stage, IntPtr outdata);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetBufferData(IntPtr real, ResourceId buff, UInt64 offset, UInt64 len, IntPtr outdata);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ReplayRenderer_GetTextureData(IntPtr real, ResourceId tex, UInt32 arrayIdx, UInt32 mip, IntPtr outdata);

        private IntPtr m_Real = IntPtr.Zero;

        public IntPtr Real { get { return m_Real; } }

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

        public ReplayOutput CreateOutput(IntPtr WindowHandle, OutputType type)
        {
            // 0 == eWindowingSystem_Unknown
            // 1 == eWindowingSystem_Win32
            IntPtr ret = ReplayRenderer_CreateOutput(m_Real, WindowHandle == IntPtr.Zero ? 0u : 1u, WindowHandle, type);

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

        public void SetFrameEvent(UInt32 eventID, bool force)
        { ReplayRenderer_SetFrameEvent(m_Real, eventID, force); }

        public GLPipelineState GetGLPipelineState()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(GLPipelineState));

            ReplayRenderer_GetGLPipelineState(m_Real, mem);

            GLPipelineState ret = (GLPipelineState)CustomMarshal.PtrToStructure(mem, typeof(GLPipelineState), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public D3D11PipelineState GetD3D11PipelineState()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(D3D11PipelineState));

            ReplayRenderer_GetD3D11PipelineState(m_Real, mem);

            D3D11PipelineState ret = (D3D11PipelineState)CustomMarshal.PtrToStructure(mem, typeof(D3D11PipelineState), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public D3D12PipelineState GetD3D12PipelineState()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(D3D12PipelineState));

            ReplayRenderer_GetD3D12PipelineState(m_Real, mem);

            D3D12PipelineState ret = (D3D12PipelineState)CustomMarshal.PtrToStructure(mem, typeof(D3D12PipelineState), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public VulkanPipelineState GetVulkanPipelineState()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(VulkanPipelineState));

            ReplayRenderer_GetVulkanPipelineState(m_Real, mem);

            VulkanPipelineState ret = (VulkanPipelineState)CustomMarshal.PtrToStructure(mem, typeof(VulkanPipelineState), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public string[] GetDisassemblyTargets()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetDisassemblyTargets(m_Real, mem);

            string[] ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public string DisassembleShader(ShaderReflection refl, string target)
        {
            IntPtr disasmMem = CustomMarshal.Alloc(typeof(templated_array));
            IntPtr target_mem = CustomMarshal.MakeUTF8String(target);

            ReplayRenderer_DisassembleShader(m_Real, refl.origPtr, target_mem, disasmMem);

            string disasm = CustomMarshal.TemplatedArrayToString(disasmMem, true);

            CustomMarshal.Free(target_mem);

            CustomMarshal.Free(disasmMem);

            return disasm;
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

        public void FreeCustomShader(ResourceId id)
        { ReplayRenderer_FreeCustomShader(m_Real, id); }

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

        public void ReplaceResource(ResourceId from, ResourceId to)
        { ReplayRenderer_ReplaceResource(m_Real, from, to); }
        public void RemoveReplacement(ResourceId id)
        { ReplayRenderer_RemoveReplacement(m_Real, id); }
        public void FreeTargetResource(ResourceId id)
        { ReplayRenderer_FreeTargetResource(m_Real, id); }

        public FetchFrameInfo GetFrameInfo()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(FetchFrameInfo));

            ReplayRenderer_GetFrameInfo(m_Real, mem);

            FetchFrameInfo ret = (FetchFrameInfo)CustomMarshal.PtrToStructure(mem, typeof(FetchFrameInfo), true);

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

        public Dictionary<uint, List<CounterResult>> FetchCounters(UInt32[] counters)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            IntPtr countersmem = CustomMarshal.Alloc(typeof(UInt32), counters.Length);

            // there's no Marshal.Copy for uint[], which is stupid.
            for (int i = 0; i < counters.Length; i++)
                Marshal.WriteInt32(countersmem, sizeof(UInt32) * i, (int)counters[i]);

            ReplayRenderer_FetchCounters(m_Real, countersmem, (uint)counters.Length, mem);

            CustomMarshal.Free(countersmem);

            Dictionary<uint, List<CounterResult>> ret = null;

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

            ReplayRenderer_EnumerateCounters(m_Real, mem);

            UInt32[] ret = (UInt32[])CustomMarshal.GetTemplatedArray(mem, typeof(UInt32), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public CounterDescription DescribeCounter(UInt32 counterID)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(CounterDescription));
            
            ReplayRenderer_DescribeCounter(m_Real, counterID, mem);

            CounterDescription ret = (CounterDescription)CustomMarshal.PtrToStructure(mem, typeof(CounterDescription), false);

            CustomMarshal.Free(mem);

            return ret;
        }

        public FetchDrawcall[] GetDrawcalls()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetDrawcalls(m_Real, mem);

            FetchDrawcall[] ret = null;

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

            ReplayRenderer_GetTextures(m_Real, mem);

            FetchTexture[] ret = (FetchTexture[])CustomMarshal.GetTemplatedArray(mem, typeof(FetchTexture), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public FetchBuffer[] GetBuffers()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetBuffers(m_Real, mem);

            FetchBuffer[] ret = (FetchBuffer[])CustomMarshal.GetTemplatedArray(mem, typeof(FetchBuffer), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public string[] GetResolve(UInt64[] callstack)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            UInt32 len = (UInt32)callstack.Length;

            ReplayRenderer_GetResolve(m_Real, callstack, len, mem);

            string[] ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public DebugMessage[] GetDebugMessages()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetDebugMessages(m_Real, mem);

            DebugMessage[] ret = (DebugMessage[])CustomMarshal.GetTemplatedArray(mem, typeof(DebugMessage), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public PixelModification[] PixelHistory(ResourceId target, UInt32 x, UInt32 y, UInt32 slice, UInt32 mip, UInt32 sampleIdx, FormatComponentType typeHint)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_PixelHistory(m_Real, target, x, y, slice, mip, sampleIdx, typeHint, mem);

            PixelModification[] ret = (PixelModification[])CustomMarshal.GetTemplatedArray(mem, typeof(PixelModification), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderDebugTrace DebugVertex(UInt32 vertid, UInt32 instid, UInt32 idx, UInt32 instOffset, UInt32 vertOffset)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(ShaderDebugTrace));

            ReplayRenderer_DebugVertex(m_Real, vertid, instid, idx, instOffset, vertOffset, mem);

            ShaderDebugTrace ret = (ShaderDebugTrace)CustomMarshal.PtrToStructure(mem, typeof(ShaderDebugTrace), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderDebugTrace DebugPixel(UInt32 x, UInt32 y, UInt32 sample, UInt32 primitive)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(ShaderDebugTrace));

            ReplayRenderer_DebugPixel(m_Real, x, y, sample, primitive, mem);

            ShaderDebugTrace ret = (ShaderDebugTrace)CustomMarshal.PtrToStructure(mem, typeof(ShaderDebugTrace), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderDebugTrace DebugThread(UInt32[] groupid, UInt32[] threadid)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(ShaderDebugTrace));

            ReplayRenderer_DebugThread(m_Real, groupid, threadid, mem);

            ShaderDebugTrace ret = (ShaderDebugTrace)CustomMarshal.PtrToStructure(mem, typeof(ShaderDebugTrace), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public EventUsage[] GetUsage(ResourceId id)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetUsage(m_Real, id, mem);

            EventUsage[] ret = (EventUsage[])CustomMarshal.GetTemplatedArray(mem, typeof(EventUsage), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public ShaderVariable[] GetCBufferVariableContents(ResourceId shader, string entryPoint, UInt32 cbufslot, ResourceId buffer, UInt64 offs)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            IntPtr entry_mem = CustomMarshal.MakeUTF8String(entryPoint);

            ReplayRenderer_GetCBufferVariableContents(m_Real, shader, entry_mem, cbufslot, buffer, offs, mem);

            ShaderVariable[] ret = (ShaderVariable[])CustomMarshal.GetTemplatedArray(mem, typeof(ShaderVariable), true);

            CustomMarshal.Free(entry_mem);
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

            ReplayRenderer_GetPostVSData(m_Real, instID, stage, mem);

            MeshFormat ret = (MeshFormat)CustomMarshal.PtrToStructure(mem, typeof(MeshFormat), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public byte[] GetBufferData(ResourceId buff, UInt64 offset, UInt64 len)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetBufferData(m_Real, buff, offset, len, mem);

            byte[] ret = (byte[])CustomMarshal.GetTemplatedArray(mem, typeof(byte), true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public byte[] GetTextureData(ResourceId tex, UInt32 arrayIdx, UInt32 mip)
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));

            ReplayRenderer_GetTextureData(m_Real, tex, arrayIdx, mip, mem);

            byte[] ret = (byte[])CustomMarshal.GetTemplatedArray(mem, typeof(byte), true);

            CustomMarshal.Free(mem);

            return ret;
        }
    };

    public class RemoteServer
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_ShutdownConnection(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_ShutdownServerAndConnection(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool RemoteServer_Ping(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_LocalProxies(IntPtr real, IntPtr outlist);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_RemoteSupportedReplays(IntPtr real, IntPtr outlist);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_GetHomeFolder(IntPtr real, IntPtr outfolder);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_ListFolder(IntPtr real, IntPtr path, IntPtr outlist);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RemoteServer_ExecuteAndInject(IntPtr real, IntPtr app, IntPtr workingDir, IntPtr cmdLine, IntPtr env, CaptureOptions opts);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_TakeOwnershipCapture(IntPtr real, IntPtr filename);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_CopyCaptureToRemote(IntPtr real, IntPtr filename, ref float progress, IntPtr remotepath);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_CopyCaptureFromRemote(IntPtr real, IntPtr remotepath, IntPtr localpath, ref float progress);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern ReplayCreateStatus RemoteServer_OpenCapture(IntPtr real, UInt32 proxyid, IntPtr logfile, ref float progress, ref IntPtr rendPtr);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RemoteServer_CloseCapture(IntPtr real, IntPtr rendPtr);

        // static exports for lists of environment modifications
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr RENDERDOC_MakeEnvironmentModificationList(int numElems);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_SetEnvironmentModification(IntPtr mem, int idx, IntPtr variable, IntPtr value,
                                                                        EnvironmentModificationType type, EnvironmentSeparator separator);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_FreeEnvironmentModificationList(IntPtr mem);

        private IntPtr m_Real = IntPtr.Zero;

        public RemoteServer(IntPtr real) { m_Real = real; }

        public void ShutdownConnection()
        {
            if (m_Real != IntPtr.Zero)
            {
                RemoteServer_ShutdownConnection(m_Real);
                m_Real = IntPtr.Zero;
            }
        }

        public void ShutdownServerAndConnection()
        {
            if (m_Real != IntPtr.Zero)
            {
                RemoteServer_ShutdownServerAndConnection(m_Real);
                m_Real = IntPtr.Zero;
            }
        }

        public string GetHomeFolder()
        {
            IntPtr homepath_mem = CustomMarshal.Alloc(typeof(templated_array));

            RemoteServer_GetHomeFolder(m_Real, homepath_mem);

            string home = CustomMarshal.TemplatedArrayToString(homepath_mem, true);

            CustomMarshal.Free(homepath_mem);

            // normalise to /s and with no trailing /s
            home = home.Replace('\\', '/');
            if (home[home.Length - 1] == '/')
                home = home.Remove(0, home.Length - 1);

            return home;
        }

        public DirectoryFile[] ListFolder(string path)
        {
            IntPtr path_mem = CustomMarshal.MakeUTF8String(path);
            IntPtr out_mem = CustomMarshal.Alloc(typeof(templated_array));

            RemoteServer_ListFolder(m_Real, path_mem, out_mem);

            DirectoryFile[] ret = (DirectoryFile[])CustomMarshal.GetTemplatedArray(out_mem, typeof(DirectoryFile), true);

            CustomMarshal.Free(out_mem);
            CustomMarshal.Free(path_mem);

            Array.Sort(ret);

            return ret;
        }

        public bool Ping()
        {
            return RemoteServer_Ping(m_Real);
        }

        public string[] LocalProxies()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));
            RemoteServer_LocalProxies(m_Real, mem);

            string[] ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public string[] RemoteSupportedReplays()
        {
            IntPtr mem = CustomMarshal.Alloc(typeof(templated_array));
            RemoteServer_RemoteSupportedReplays(m_Real, mem);

            string[] ret = CustomMarshal.TemplatedArrayToStringArray(mem, true);

            CustomMarshal.Free(mem);

            return ret;
        }

        public UInt32 ExecuteAndInject(string app, string workingDir, string cmdLine, EnvironmentModification[] env, CaptureOptions opts)
        {
            IntPtr app_mem = CustomMarshal.MakeUTF8String(app);
            IntPtr workingDir_mem = CustomMarshal.MakeUTF8String(workingDir);
            IntPtr cmdLine_mem = CustomMarshal.MakeUTF8String(cmdLine);

            IntPtr env_mem = RENDERDOC_MakeEnvironmentModificationList(env.Length);

            for (int i = 0; i < env.Length; i++)
            {
                IntPtr var_mem = CustomMarshal.MakeUTF8String(env[i].variable);
                IntPtr val_mem = CustomMarshal.MakeUTF8String(env[i].value);

                RENDERDOC_SetEnvironmentModification(env_mem, i, var_mem, val_mem, env[i].type, env[i].separator);

                CustomMarshal.Free(var_mem);
                CustomMarshal.Free(val_mem);
            }

            UInt32 ret = RemoteServer_ExecuteAndInject(m_Real, app_mem, workingDir_mem, cmdLine_mem, env_mem, opts);

            RENDERDOC_FreeEnvironmentModificationList(env_mem);

            CustomMarshal.Free(app_mem);
            CustomMarshal.Free(workingDir_mem);
            CustomMarshal.Free(cmdLine_mem);

            return ret;
        }

        public void TakeOwnershipCapture(string filename)
        {
            IntPtr filename_mem = CustomMarshal.MakeUTF8String(filename);

            RemoteServer_TakeOwnershipCapture(m_Real, filename_mem);

            CustomMarshal.Free(filename_mem);
        }

        public string CopyCaptureToRemote(string filename, ref float progress)
        {
            IntPtr remotepath = CustomMarshal.Alloc(typeof(templated_array));

            IntPtr filename_mem = CustomMarshal.MakeUTF8String(filename);

            RemoteServer_CopyCaptureToRemote(m_Real, filename_mem, ref progress, remotepath);

            CustomMarshal.Free(filename_mem);

            string remote = CustomMarshal.TemplatedArrayToString(remotepath, true);

            CustomMarshal.Free(remotepath);

            return remote;
        }

        public void CopyCaptureFromRemote(string remotepath, string localpath, ref float progress)
        {
            IntPtr remotepath_mem = CustomMarshal.MakeUTF8String(remotepath);
            IntPtr localpath_mem = CustomMarshal.MakeUTF8String(localpath);

            RemoteServer_CopyCaptureFromRemote(m_Real, remotepath_mem, localpath_mem, ref progress);

            CustomMarshal.Free(remotepath_mem);
            CustomMarshal.Free(localpath_mem);
        }

        public ReplayRenderer OpenCapture(int proxyid, string logfile, ref float progress)
        {
            IntPtr rendPtr = IntPtr.Zero;

            IntPtr logfile_mem = CustomMarshal.MakeUTF8String(logfile);

            ReplayCreateStatus ret = RemoteServer_OpenCapture(m_Real, proxyid == -1 ? UInt32.MaxValue : (UInt32)proxyid, logfile_mem, ref progress, ref rendPtr);

            CustomMarshal.Free(logfile_mem);

            if (rendPtr == IntPtr.Zero || ret != ReplayCreateStatus.Success)
            {
                throw new ReplayCreateException(ret, "Failed to set up local proxy replay with remote connection");
            }

            return new ReplayRenderer(rendPtr);
        }

        public void CloseCapture(ReplayRenderer renderer)
        {
            RemoteServer_CloseCapture(m_Real, renderer.Real);
        }
    };

    public class TargetControl
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void TargetControl_Shutdown(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr TargetControl_GetTarget(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr TargetControl_GetAPI(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 TargetControl_GetPID(IntPtr real);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr TargetControl_GetBusyClient(IntPtr real);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void TargetControl_TriggerCapture(IntPtr real, UInt32 numFrames);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void TargetControl_QueueCapture(IntPtr real, UInt32 frameNumber);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void TargetControl_CopyCapture(IntPtr real, UInt32 remoteID, IntPtr localpath);
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void TargetControl_DeleteCapture(IntPtr real, UInt32 remoteID);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void TargetControl_ReceiveMessage(IntPtr real, IntPtr outmsg);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 RENDERDOC_EnumerateRemoteConnections(IntPtr host, UInt32[] idents);

        private IntPtr m_Real = IntPtr.Zero;
        private bool m_Connected;

        public TargetControl(IntPtr real)
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
                Target = CustomMarshal.PtrToStringUTF8(TargetControl_GetTarget(m_Real));
                API = CustomMarshal.PtrToStringUTF8(TargetControl_GetAPI(m_Real));
                PID = TargetControl_GetPID(m_Real);
                BusyClient = CustomMarshal.PtrToStringUTF8(TargetControl_GetBusyClient(m_Real));
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
            if (m_Real != IntPtr.Zero) TargetControl_Shutdown(m_Real);
            m_Real = IntPtr.Zero;
        }

        public void TriggerCapture(UInt32 numFrames)
        {
            TargetControl_TriggerCapture(m_Real, numFrames);
        }

        public void QueueCapture(UInt32 frameNum)
        {
            TargetControl_QueueCapture(m_Real, frameNum);
        }

        public void CopyCapture(UInt32 id, string localpath)
        {
            IntPtr localpath_mem = CustomMarshal.MakeUTF8String(localpath);

            TargetControl_CopyCapture(m_Real, id, localpath_mem);

            CustomMarshal.Free(localpath_mem);
        }

        public void DeleteCapture(UInt32 id)
        {
            TargetControl_DeleteCapture(m_Real, id);
        }

        public void ReceiveMessage()
        {
            if (m_Real != IntPtr.Zero)
            {
                TargetControlMessage msg = null;

                {
                    IntPtr mem = CustomMarshal.Alloc(typeof(TargetControlMessage));

                    TargetControl_ReceiveMessage(m_Real, mem);

                    if (mem != IntPtr.Zero)
                        msg = (TargetControlMessage)CustomMarshal.PtrToStructure(mem, typeof(TargetControlMessage), true);

                    CustomMarshal.Free(mem);
                }

                if (msg == null)
                    return;

                if (msg.Type == TargetControlMessageType.Disconnected)
                {
                    m_Connected = false;
                    TargetControl_Shutdown(m_Real);
                    m_Real = IntPtr.Zero;
                }
                else if (msg.Type == TargetControlMessageType.NewCapture)
                {
                    CaptureFile = msg.NewCapture;
                    CaptureExists = true;
                }
                else if (msg.Type == TargetControlMessageType.CaptureCopied)
                {
                    CaptureFile.ID = msg.NewCapture.ID;
                    CaptureFile.path = msg.NewCapture.path;
                    CaptureCopied = true;
                }
                else if (msg.Type == TargetControlMessageType.RegisterAPI)
                {
                    API = msg.RegisterAPI.APIName;
                    InfoUpdated = true;
                }
                else if (msg.Type == TargetControlMessageType.NewChild)
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

        public TargetControlMessage.NewCaptureData CaptureFile = new TargetControlMessage.NewCaptureData();

        public TargetControlMessage.NewChildData NewChild = new TargetControlMessage.NewChildData();
    };
};
