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

namespace renderdoc
{
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderVariable
    {
        public UInt32 rows, columns;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;

        public VarType type;

        public bool displayAsHex;

        [StructLayout(LayoutKind.Sequential)]
        public struct ValueUnion
        {
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.UInt32)]
            public UInt32[] uv;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.Float)]
            public float[] fv;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.Int32)]
            public Int32[] iv;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.Double)]
            public double[] dv;
        };

        [CustomMarshalAs(CustomUnmanagedType.Union)]
        public ValueUnion value;

        public bool isStruct;
        
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderVariable[] members;

		public override string ToString()
		{
			if (members.Length > 0)
				return "";
			if (rows == 1) return Row(0);

			string ret = "";
			for (int i = 0; i < (int)rows; i++)
			{
				if (i > 0) ret += ", ";
				ret += "{" + Row(i) + "}";
			}

			return "{ " + ret + " }";
		}

		public string RowTypeString()
		{
			if (members.Length > 0)
			{
				if (isStruct)
					return "struct";
				else
					return "flibbertygibbet";
			}

			if (rows == 0 && columns == 0)
				return "-";

			string typeStr = type.Str();

			if(displayAsHex && type == VarType.UInt)
				typeStr = "xint";

			if (columns == 1)
				return typeStr;

			return String.Format("{0}{1}", typeStr, columns);
		}

		public string TypeString()
		{
			if (members.Length > 0)
			{
				if (isStruct)
					return "struct";
				else
					return String.Format("{0}[{1}]", members[0].TypeString(), members.Length);
			}

			string typeStr = type.Str();

			if (displayAsHex && type == VarType.UInt)
				typeStr = "xint";

			if (rows == 1 && columns == 1) return typeStr;
			if (rows == 1) return String.Format("{0}{1}", typeStr, columns);
			else return String.Format("{0}{1}x{2}", typeStr, rows, columns);
		}

        public string RowValuesToString(int cols, double x, double y, double z, double w)
        {
            if (cols == 1) return Formatter.Format(x);
            else if (cols == 2) return Formatter.Format(x) + ", " + Formatter.Format(y);
            else if (cols == 3) return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z);
            else return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z) + ", " + Formatter.Format(w);
        }

        public string RowValuesToString(int cols, float x, float y, float z, float w)
        {
            if (cols == 1) return Formatter.Format(x);
            else if (cols == 2) return Formatter.Format(x) + ", " + Formatter.Format(y);
            else if (cols == 3) return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z);
            else return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z) + ", " + Formatter.Format(w);
        }

        public string RowValuesToString(int cols, UInt32 x, UInt32 y, UInt32 z, UInt32 w)
        {
            if (cols == 1) return Formatter.Format(x, displayAsHex);
            else if (cols == 2) return Formatter.Format(x, displayAsHex) + ", " + Formatter.Format(y, displayAsHex);
            else if (cols == 3) return Formatter.Format(x, displayAsHex) + ", " + Formatter.Format(y, displayAsHex) + ", " + Formatter.Format(z, displayAsHex);
            else return Formatter.Format(x, displayAsHex) + ", " + Formatter.Format(y, displayAsHex) + ", " + Formatter.Format(z, displayAsHex) + ", " + Formatter.Format(w, displayAsHex);
        }

        public string RowValuesToString(int cols, Int32 x, Int32 y, Int32 z, Int32 w)
        {
            if (cols == 1) return Formatter.Format(x);
            else if (cols == 2) return Formatter.Format(x) + ", " + Formatter.Format(y);
            else if (cols == 3) return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z);
            else return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z) + ", " + Formatter.Format(w);
        }

		public string Row(int row, VarType t)
		{
			if(t == VarType.Double)
                return RowValuesToString((int)columns, value.dv[row * columns + 0], value.dv[row * columns + 1], value.dv[row * columns + 2], value.dv[row * columns + 3]);
			else if(t == VarType.Int)
                return RowValuesToString((int)columns, value.iv[row * columns + 0], value.iv[row * columns + 1], value.iv[row * columns + 2], value.iv[row * columns + 3]);
			else if(t == VarType.UInt)
                return RowValuesToString((int)columns, value.uv[row * columns + 0], value.uv[row * columns + 1], value.uv[row * columns + 2], value.uv[row * columns + 3]);
			else
                return RowValuesToString((int)columns, value.fv[row * columns + 0], value.fv[row * columns + 1], value.fv[row * columns + 2], value.fv[row * columns + 3]);
		}

        public string Row(int row)
		{
			return Row(row, type);
		}
    };
        
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderDebugState
    {
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderVariable[] registers;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderVariable[] outputs;

        [StructLayout(LayoutKind.Sequential)]
        public struct IndexableTempArray
        {
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ShaderVariable[] temps;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public IndexableTempArray[] indexableTemps;

        public UInt32 nextInstruction;
        public ShaderDebugStateFlags flags;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderDebugTrace
    {
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderVariable[] inputs;

        [StructLayout(LayoutKind.Sequential)]
        public struct CBuffer
        {
            [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
            public ShaderVariable[] variables;
        };
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public CBuffer[] cbuffers;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderDebugState[] states;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class SigParameter
    {
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string varName;
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string semanticName;

        public UInt32 semanticIndex;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string semanticIdxName;

        public bool needSemanticIndex;

        public UInt32 regIndex;
        public SystemAttribute systemValue;

        public FormatComponentType compType;

        public byte regChannelMask;
        public byte channelUsedMask;
        public UInt32 compCount;
        public UInt32 stream;

        public UInt32 arrayIndex;
        
		public string TypeString
        {
            get
            {
                string ret = "";

                if (compType == FormatComponentType.Float)
                    ret += "float";
                else if (compType == FormatComponentType.UInt || compType == FormatComponentType.UScaled)
                    ret += "uint";
                else if (compType == FormatComponentType.SInt || compType == FormatComponentType.SScaled)
                    ret += "int";
                else if (compType == FormatComponentType.UNorm)
                    ret += "unorm float";
                else if (compType == FormatComponentType.SNorm)
                    ret += "snorm float";
                else if (compType == FormatComponentType.Depth)
                    ret += "float";

                if (compCount > 1)
                    ret += compCount;

                return ret;
            }
        }

        public string D3DSemanticString
        {
            get
            {
                if (systemValue == SystemAttribute.None)
                    return semanticIdxName;

                string ret = systemValue.Str();

                // need to include the index if it's a system value semantic that's numbered
                if (systemValue == SystemAttribute.ColourOutput ||
                     systemValue == SystemAttribute.CullDistance ||
                     systemValue == SystemAttribute.ClipDistance)
                    ret += semanticIndex;

                return ret;
            }
        }
        
		public static string GetComponentString(byte mask)
        {
            string ret = "";

            if ((mask & 0x1) > 0) ret += "R";
            if ((mask & 0x2) > 0) ret += "G";
            if ((mask & 0x4) > 0) ret += "B";
            if ((mask & 0x8) > 0) ret += "A";

            return ret;
        }
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderVariableType
    {
        [StructLayout(LayoutKind.Sequential)]
        public struct ShaderVarDescriptor
        {
            public VarType type;
            public UInt32 rows;
            public UInt32 cols;
            public UInt32 elements;
            public bool rowMajorStorage;
            public UInt32 arrayStride;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string name;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderVarDescriptor descriptor;

        public string Name { get { return descriptor.name; } }

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderConstant[] members;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderConstant
    {
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;

        [StructLayout(LayoutKind.Sequential)]
        public struct RegSpan
        {
            public UInt32 vec;
            public UInt32 comp;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public RegSpan reg;

        public UInt64 defaultValue;

        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderVariableType type;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ConstantBlock
    {
        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderConstant[] variables;

        public bool bufferBacked;
        public Int32 bindPoint;
        public UInt32 byteSize;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ShaderResource
    {
        public bool IsSampler;
        public bool IsTexture;
        public bool IsSRV;

        public ShaderResourceType resType;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string name;
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderVariableType variableType;
        public Int32 bindPoint;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ShaderDebugChunk
    {
        public UInt32 compileFlags;

        public struct DebugFile
        {
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            private string filename_;
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string filetext;

            public string FullFilename
            {
                get
                {
                    return filename_;
                }
            }

            // get filename handling possibly invalid characters
            public string BaseFilename
            {
                get
                {
                    return renderdocui.Code.Helpers.SafeGetFileName(filename_);
                }
                set
                {
                    filename_ = value;
                }
            }
        };

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public DebugFile[] files;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderReflection
    {
        public ResourceId ID;

        [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
        public string EntryPoint;

        [CustomMarshalAs(CustomUnmanagedType.Skip)]
        public IntPtr origPtr;

        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderDebugChunk DebugInfo;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public byte[] RawBytes;

        [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 3)]
        public UInt32[] DispatchThreadsDimension;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public SigParameter[] InputSig;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public SigParameter[] OutputSig;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ConstantBlock[] ConstantBlocks;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderResource[] ReadOnlyResources;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderResource[] ReadWriteResources;

        [StructLayout(LayoutKind.Sequential)]
        public struct Interface
        {
            [CustomMarshalAs(CustomUnmanagedType.UTF8TemplatedString)]
            public string Name;
        };

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Interface[] Interfaces;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class BindpointMap
    {
        public Int32 bindset;
        public Int32 bind;
        public bool used;
        public UInt32 arraySize;

        public BindpointMap()
        {
        }

        public BindpointMap(Int32 set, Int32 slot)
        {
            bindset = set;
            bind = slot;
            used = false;
            arraySize = 1;
        }

        public override bool Equals(Object obj)
        {
            return obj is BindpointMap && this == (BindpointMap)obj;
        }
        public override int GetHashCode()
        {
            int hash = bindset.GetHashCode() * 17;
            hash = hash * 17 + bind.GetHashCode();
            return hash;
        }
        public static bool operator ==(BindpointMap x, BindpointMap y)
        {
            if ((object)x == null) return (object)y == null;
            if ((object)y == null) return (object)x == null;

            return x.bindset == y.bindset &&
                x.bind == y.bind;
        }
        public static bool operator !=(BindpointMap x, BindpointMap y)
        {
            return !(x == y);
        }
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ShaderBindpointMapping
    {
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public int[] InputAttributes;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public BindpointMap[] ConstantBlocks;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public BindpointMap[] ReadOnlyResources;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public BindpointMap[] ReadWriteResources;
    };
};
