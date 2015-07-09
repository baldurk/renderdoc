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
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace renderdoc
{
    public struct Vec3f
    {
        public Vec3f(float X, float Y, float Z) { x = X; y = Y; z = Z; }
        public Vec3f(Vec3f v) { x = v.x; y = v.y; z = v.z; }
        public Vec3f(FloatVector v) { x = v.x; y = v.y; z = v.z; }

        public float Length()
        {
            return (float)Math.Sqrt(x * x + y * y + z * z);
        }

        public Vec3f Sub(Vec3f o)
        {
            return new Vec3f(x - o.x,
                                y - o.y,
                                z - o.z);
        }

        public void Mul(float f)
        {
            x *= f;
            y *= f;
            z *= f;
        }

        public float x, y, z;
    };

    public struct Vec4f
    {
        public float x, y, z, w;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct FloatVector
    {
        public FloatVector(float X, float Y, float Z, float W) { x = X; y = Y; z = Z; w = W; }
        public FloatVector(float X, float Y, float Z) { x = X; y = Y; z = Z; w = 1; }
        public FloatVector(Vec3f v) { x = v.x; y = v.y; z = v.z; w = 1; }
        public FloatVector(Vec3f v, float W) { x = v.x; y = v.y; z = v.z; w = W; }

        public float x, y, z, w;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ResourceFormat
    {
        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern float Maths_HalfToFloat(UInt16 half);

        public ResourceFormat()
        {
            rawType = 0;
            special = false;
            specialFormat = SpecialFormat.Unknown;

            compType = FormatComponentType.None;
            compCount = 0;
            compByteWidth = 0;
            srgbCorrected = false;

            strname = "";
        }

        public ResourceFormat(FormatComponentType type, UInt32 count, UInt32 byteWidth)
        {
            rawType = 0;
            special = false;
            specialFormat = SpecialFormat.Unknown;

            compType = type;
            compCount = count;
            compByteWidth = byteWidth;
            srgbCorrected = false;

            strname = "";
        }

        public UInt32 rawType;

        // indicates it's not a type represented with the members below
        // usually this means non-uniform across components or block compressed
        public bool special;
        public SpecialFormat specialFormat;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string strname;

        public UInt32 compCount;
        public UInt32 compByteWidth;
        public FormatComponentType compType;

        public bool srgbCorrected;

        public override string ToString()
        {
            return strname;
        }

        public override bool Equals(Object obj)
        {
            return obj is ResourceFormat && this == (ResourceFormat)obj;
        }
        public override int GetHashCode()
        {
            int hash = specialFormat.GetHashCode() * 17;
            hash = hash * 17 + compCount.GetHashCode();
            hash = hash * 17 + compByteWidth.GetHashCode();
            hash = hash * 17 + compType.GetHashCode();
            hash = hash * 17 + srgbCorrected.GetHashCode();
            return hash;
        }
        public static bool operator ==(ResourceFormat x, ResourceFormat y)
        {
            if (x.special || y.special)
                return x.special == y.special && x.specialFormat == y.specialFormat;

            return x.compCount == y.compCount &&
                x.compByteWidth == y.compByteWidth &&
                x.compType == y.compType &&
                x.srgbCorrected == y.srgbCorrected;
        }
        public static bool operator !=(ResourceFormat x, ResourceFormat y)
        {
            return !(x == y);
        }

        public float ConvertFromHalf(UInt16 comp)
        {
            return Maths_HalfToFloat(comp);
        }

        public object Interpret(UInt16 comp)
        {
            if (compByteWidth != 2 || compType == FormatComponentType.Float) throw new ArgumentException();

            if (compType == FormatComponentType.SInt)
            {
                return (Int16)comp;
            }
            else if (compType == FormatComponentType.UInt)
            {
                return comp;
            }
            else if (compType == FormatComponentType.UNorm)
            {
                return (float)comp / (float)UInt16.MaxValue;
            }
            else if (compType == FormatComponentType.SNorm)
            {
                Int16 cast = (Int16)comp;

                float f = -1.0f;

                if (cast == -32768)
                    f = -1.0f;
                else
                    f = ((float)cast) / 32767.0f;

                return f;
            }

            throw new ArgumentException();
        }

        public object Interpret(byte comp)
        {
            if (compByteWidth != 1 || compType == FormatComponentType.Float) throw new ArgumentException();

            if (compType == FormatComponentType.SInt)
            {
                return (sbyte)comp;
            }
            else if (compType == FormatComponentType.UInt)
            {
                return comp;
            }
            else if (compType == FormatComponentType.UNorm)
            {
                return ((float)comp) / 255.0f;
            }
            else if (compType == FormatComponentType.SNorm)
            {
                sbyte cast = (sbyte)comp;

                float f = -1.0f;

                if (cast == -128)
                    f = -1.0f;
                else
                    f = ((float)cast) / 127.0f;

                return f;
            }

            throw new ArgumentException();
        }
    };

    [StructLayout(LayoutKind.Sequential)]
    public class FetchBuffer
    {
        public ResourceId ID;
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;
        public bool customName;
        public UInt32 length;
        public UInt32 structureSize;
        public BufferCreationFlags creationFlags;
        public UInt64 byteSize;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class FetchTexture
    {
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;
        public bool customName;
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ResourceFormat format;
        public UInt32 dimension;
        public ShaderResourceType resType;
        public UInt32 width, height, depth;
        public ResourceId ID;
        public bool cubemap;
        public UInt32 mips;
        public UInt32 arraysize;
        public UInt32 numSubresources;
        public TextureCreationFlags creationFlags;
        public UInt32 msQual, msSamp;
        public UInt64 byteSize;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class OutputConfig
    {
        public OutputType m_Type = OutputType.None;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class FetchFrameInfo
    {
        public UInt32 frameNumber;
        public UInt32 firstEvent;
        public UInt64 fileOffset;
        public UInt64 captureTime;
        public ResourceId immContextId;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public DebugMessage[] debugMessages;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class FetchAPIEvent
    {
        public UInt32 eventID;

        public ResourceId context;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public UInt64[] callstack;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string eventDesc;

        public UInt64 fileOffset;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class DebugMessage
    {
        public UInt32 eventID;
        public DebugMessageCategory category;
        public DebugMessageSeverity severity;
        public DebugMessageSource source;
        public UInt32 messageID;
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string description;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class EventUsage
    {
        public UInt32 eventID;
        public ResourceUsage usage;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class FetchDrawcall
    {
        public UInt32 eventID, drawcallID;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;

        public DrawcallFlags flags;

        public UInt32 numIndices;
        public UInt32 numInstances;
        public UInt32 indexOffset;
        public UInt32 vertexOffset;
        public UInt32 instanceOffset;

        [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 3)]
        public UInt32[] dispatchDimension;
        [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 3)]
        public UInt32[] dispatchThreadsDimension;

        public UInt32 indexByteWidth;
        public PrimitiveTopology topology;

        public ResourceId context;

        public Int64 parentDrawcall;
        public Int64 previousDrawcall;
        public Int64 nextDrawcall;

        [CustomMarshalAs(CustomUnmanagedType.Skip)]
        public FetchDrawcall parent;
        [CustomMarshalAs(CustomUnmanagedType.Skip)]
        public FetchDrawcall previous;
        [CustomMarshalAs(CustomUnmanagedType.Skip)]
        public FetchDrawcall next;

        [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 8)]
        public ResourceId[] outputs;
        public ResourceId depthOut;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public FetchAPIEvent[] events;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public FetchDrawcall[] children;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct MeshFormat
    {
        public ResourceId idxbuf;
        public UInt32 idxoffs;
        public UInt32 idxByteWidth;

        public ResourceId buf;
        public UInt32 offset;
        public UInt32 stride;

        public UInt32 compCount;
        public UInt32 compByteWidth;
        public FormatComponentType compType;
        public SpecialFormat specialFormat;

        public bool showAlpha;

        public PrimitiveTopology topo;
        public UInt32 numVerts;

        public bool unproject;
        public float nearPlane;
        public float farPlane;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class MeshDisplay
    {
        public MeshDataStage type = MeshDataStage.Unknown;

        public IntPtr cam = IntPtr.Zero;

        public bool ortho = false;
        public float fov = 90.0f;
        public float aspect = 0.0f;

        public bool thisDrawOnly = true;
        public UInt32 curInstance = 0;

        public UInt32 highlightVert;
        public MeshFormat position;
        public MeshFormat secondary;

        public FloatVector prevMeshColour = new FloatVector();
        public FloatVector currentMeshColour = new FloatVector();

        public FloatVector minBounds = new FloatVector();
        public FloatVector maxBounds = new FloatVector();
        public bool showBBox = false;

        public SolidShadeMode solidShadeMode = SolidShadeMode.None;
        public bool wireframeDraw = true;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class TextureDisplay
    {
        public ResourceId texid = ResourceId.Null;
        public float rangemin = 0.0f;
        public float rangemax = 1.0f;
        public float scale = 1.0f;
        public bool Red = true, Green = true, Blue = true, Alpha = false;
        public bool FlipY = false;
        public float HDRMul = -1.0f;
        public bool linearDisplayAsGamma = true;
        public ResourceId CustomShader = ResourceId.Null;
        public UInt32 mip = 0;
        public UInt32 sliceFace = 0;
        public UInt32 sampleIdx = 0;
        public bool rawoutput = false;

        public float offx = 0.0f, offy = 0.0f;

        public FloatVector lightBackgroundColour = new FloatVector(0.666f, 0.666f, 0.666f);
        public FloatVector darkBackgroundColour = new FloatVector(0.333f, 0.333f, 0.333f);

        public TextureDisplayOverlay overlay = TextureDisplayOverlay.None;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class TextureSave
    {
        public ResourceId id = ResourceId.Null;

        public FileType destType = FileType.DDS;

        public Int32 mip = -1;

        public struct ComponentMapping
        {
            public float blackPoint;
            public float whitePoint;
        };

        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ComponentMapping comp = new ComponentMapping();

        [StructLayout(LayoutKind.Sequential)]
        public struct SampleMapping
        {
            public bool mapToArray;

            public UInt32 sampleIndex;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public SampleMapping sample = new SampleMapping();

        [StructLayout(LayoutKind.Sequential)]
        public struct SliceMapping
        {
            public Int32 sliceIndex;

            public bool slicesAsGrid;

            public Int32 sliceGridWidth;

            public bool cubeCruciform;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public SliceMapping slice = new SliceMapping();

        public AlphaMapping alpha = AlphaMapping.Discard;
        public FloatVector alphaCol = new FloatVector(0.666f, 0.666f, 0.666f);
        public FloatVector alphaColSecondary = new FloatVector(0.333f, 0.333f, 0.333f);

        public int jpegQuality = 90;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class APIProperties
    {
        public APIPipelineStateType pipelineType;
        public bool degraded;

        public string ShaderExtension
        {
            get
            {
                return pipelineType == APIPipelineStateType.D3D11 ? ".hlsl" : ".glsl";
            }
        }
    };

    [StructLayout(LayoutKind.Sequential)]
    public class CounterDescription
    {
        public UInt32 counterID;
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string description;
        public FormatComponentType resultCompType;
        public UInt32 resultByteWidth;
        public CounterUnits units;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class CounterResult
    {
        public UInt32 eventID;
        public UInt32 counterID;

        [StructLayout(LayoutKind.Sequential)]
        public struct ValueUnion
        {
            public float f;
            public double d;
            public UInt32 u32;
            public UInt64 u64;
        };

        [CustomMarshalAs(CustomUnmanagedType.Union)]
        public ValueUnion value;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class PixelValue
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct ValueUnion
        {
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.UInt32)]
            public UInt32[] u;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.Float)]
            public float[] f;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.Int32)]
            public Int32[] i;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 4, FixedType = CustomFixedType.UInt16)]
            public UInt16[] u16;
        };

        [CustomMarshalAs(CustomUnmanagedType.Union)]
        public ValueUnion value;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ModificationValue
    {
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public PixelValue col;
        public float depth;
        public Int32 stencil;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class PixelModification
    {
        public UInt32 eventID;

        public bool uavWrite;

        public UInt32 fragIndex;
        public UInt32 primitiveID;

        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ModificationValue preMod;
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ModificationValue shaderOut;
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ModificationValue postMod;

        public bool backfaceCulled;
        public bool depthClipped;
        public bool viewClipped;
        public bool scissorClipped;
        public bool shaderDiscarded;
        public bool depthTestFailed;
        public bool stencilTestFailed;

        public bool EventPassed()
        {
            return !backfaceCulled && !depthClipped && !viewClipped &&
                !scissorClipped && !shaderDiscarded && !depthTestFailed &&
                !stencilTestFailed;
        }
    };
}
