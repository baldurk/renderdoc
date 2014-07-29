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

namespace renderdoc
{
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderVariable
    {
        public UInt32 rows, columns;

        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string name;

        public VarType type;

        [StructLayout(LayoutKind.Sequential)]
        public struct ValueUnion
        {
            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.UInt32)]
            public UInt32[] uv;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.Float)]
            public float[] fv;

            [CustomMarshalAs(CustomUnmanagedType.FixedArray, FixedLength = 16, FixedType = CustomFixedType.Int32)]
            public Int32[] iv;

            [CustomMarshalAs(CustomUnmanagedType.Skip)]
            private double[] dv_arr;

            public double[] dv
            {
                get
                {
                    if (dv_arr == null)
                    {
                        UInt64[] ds = { 0, 0 };
                        ds[0] = uv[1];
                        ds[1] = uv[3];

                        ds[0] <<= 32;
                        ds[1] <<= 32;

                        ds[0] |= uv[0];
                        ds[1] |= uv[2];

                        dv_arr = new double[2];
                        dv_arr[0] = BitConverter.Int64BitsToDouble(unchecked((long)ds[0]));
                        dv_arr[1] = BitConverter.Int64BitsToDouble(unchecked((long)ds[1]));
                    }

                    return dv_arr;
                }
            }
        };

        [CustomMarshalAs(CustomUnmanagedType.Union)]
        public ValueUnion value;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderVariable[] members;
        
        public override string ToString()
		{ 
			if(members.Length > 0) return String.Format("struct[{0}]", members.Length);
			if(rows == 1) return Row(0);
			
			string ret = "";
            for (int i = 0; i < (int)rows; i++)
			{
				if(i > 0) ret += ", ";
				ret += "{" + Row(i) + "}";
			}

			return "{ " + ret + " }";
		}

		public string RowTypeString()
		{
            if (members.Length > 0) return "struct";

			if(rows == 0 && columns == 0)
				return "-";

			if(columns == 1)
				return type.Str();

            return String.Format("{0}{1}", type.Str(), columns);
		}

		public string TypeString()
		{
            if (members.Length > 0) return "struct";

            if (rows == 1 && columns == 1) return type.Str();
            if (rows == 1) return String.Format("{0}{1}", type.Str(), columns);
            else return String.Format("{0}{1}x{2}", type.Str(), rows, columns);
		}

        public string RowValuesToString(int cols, double x, double y)
		{
            if (cols == 1) return Formatter.Format(x);
            else return Formatter.Format(x) + ", " + Formatter.Format(y);
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
            if (cols == 1) return Formatter.Format(x);
            else if (cols == 2) return Formatter.Format(x) + ", " + Formatter.Format(y);
            else if (cols == 3) return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z);
            else return Formatter.Format(x) + ", " + Formatter.Format(y) + ", " + Formatter.Format(z) + ", " + Formatter.Format(w);
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
				return RowValuesToString((int)columns, value.dv[row*columns+0], value.dv[row*columns+1]);
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

        public UInt32 nextInstruction;
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
        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string varName;
        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string semanticName;

        public UInt32 semanticIndex;

        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string semanticIdxName;

        public bool needSemanticIndex;

        public UInt32 regIndex;
        public SystemAttribute systemValue;

        public FormatComponentType compType;

        public byte regChannelMask;
        public byte channelUsedMask;
        public UInt32 compCount;
        public UInt32 stream;
        
		public string TypeString
        {
            get
            {
                string ret = "";

                if (compType == FormatComponentType.Float)
                    ret += "float";
                else if (compType == FormatComponentType.UInt)
                    ret += "uint";
                else if (compType == FormatComponentType.SInt)
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

		public string D3D11SemanticString
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
            [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
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
        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string name;

        [StructLayout(LayoutKind.Sequential)]
        public struct RegSpan
        {
            public UInt32 vec;
            public UInt32 comp;
        };
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public RegSpan reg;

        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderVariableType type;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ConstantBlock
    {
        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string name;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderConstant[] variables;

        public Int32 bufferAddress;
        public Int32 bindPoint;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ShaderResource
    {
        public bool IsSampler;
        public bool IsTexture;
        public bool IsSRV;
        public bool IsUAV;

        public ShaderResourceType resType;

        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string name;
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderVariableType variableType;
        public Int32 variableAddress;
        public Int32 bindPoint;
    };

    [StructLayout(LayoutKind.Sequential)]
    public class ShaderDebugChunk
    {
        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string entryFunc;

        public UInt32 compileFlags;

        public struct DebugFile
        {
            [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
            public string filename;
            [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
            public string filetext;
        };

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public DebugFile[] files;
    };
    
    [StructLayout(LayoutKind.Sequential)]
    public class ShaderReflection
    {
        [CustomMarshalAs(CustomUnmanagedType.CustomClass)]
        public ShaderDebugChunk DebugInfo;

        [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
        public string Disassembly;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public SigParameter[] InputSig;
        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public SigParameter[] OutputSig;

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ConstantBlock[] ConstantBlocks; // sparse - index indicates bind point

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public ShaderResource[] Resources; // non-sparse, since bind points can overlap.

        [StructLayout(LayoutKind.Sequential)]
        public struct Interface
        {
            [CustomMarshalAs(CustomUnmanagedType.AsciiTemplatedString)]
            public string Name;
        };

        [CustomMarshalAs(CustomUnmanagedType.TemplatedArray)]
        public Interface[] Interfaces;
    };
};