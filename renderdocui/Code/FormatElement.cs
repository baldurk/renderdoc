﻿/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Baldur Karlsson
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
using System.IO;
using System.Text.RegularExpressions;
using renderdoc;

namespace renderdocui.Code
{
    public class FormatElement
    {
        public FormatElement()
        {
            name = "";
            buffer = 0;
            offset = 0;
            perinstance = false;
            rowmajor = false;
            matrixdim = 0;
            format = new ResourceFormat();
            hex = false;
        }

        public FormatElement(string Name, int buf, uint offs, bool pi, bool rowMat, uint matDim, ResourceFormat f, bool h)
        {
            name = Name;
            buffer = buf;
            offset = offs;
            format = f;
            perinstance = pi;
            rowmajor = rowMat;
            matrixdim = matDim;
            hex = h;
        }

        public uint ByteSize
        {
            get
            {
                return format.compByteWidth * format.compCount * matrixdim;
            }
        }

        public object[] GetObjects(BinaryReader read)
        {
            var ret = new List<object>();

            if (format.special && format.specialFormat == SpecialFormat.B8G8R8A8)
            {
                byte b = read.ReadByte();
                byte g = read.ReadByte();
                byte r = read.ReadByte();
                byte a = read.ReadByte();

                ret.Add((float)r / 255.0f);
                ret.Add((float)g / 255.0f);
                ret.Add((float)b / 255.0f);
                ret.Add((float)a / 255.0f);
            }
            else if (format.special && format.specialFormat == SpecialFormat.B5G5R5A1)
            {
                ushort packed = read.ReadUInt16();

                ret.Add((float)((packed >> 10) & 0x1f) / 31.0f);
                ret.Add((float)((packed >> 5) & 0x1f) / 31.0f);
                ret.Add((float)((packed >> 0) & 0x1f) / 31.0f);
                ret.Add(((packed & 0x8000) > 0) ? 1.0f : 0.0f);
            }
            else if (format.special && format.specialFormat == SpecialFormat.B5G6R5)
            {
                ushort packed = read.ReadUInt16();

                ret.Add((float)((packed >> 11) & 0x1f) / 31.0f);
                ret.Add((float)((packed >> 5) & 0x3f) / 63.0f);
                ret.Add((float)((packed >> 0) & 0x1f) / 31.0f);
            }
            else if (format.special && format.specialFormat == SpecialFormat.B4G4R4A4)
            {
                ushort packed = read.ReadUInt16();

                ret.Add((float)((packed >> 8) & 0xf) / 15.0f);
                ret.Add((float)((packed >> 4) & 0xf) / 15.0f);
                ret.Add((float)((packed >> 0) & 0xf) / 15.0f);
                ret.Add((float)((packed >> 12) & 0xf) / 15.0f);
            }
            else if (format.special && format.specialFormat == SpecialFormat.R10G10B10A2)
            {
                // allow for vectors of this format - for raw buffer viewer
                for (int i = 0; i < (format.compCount / 4); i++)
                {
                    uint packed = read.ReadUInt32();

                    uint r = (packed >> 0) & 0x3ff;
                    uint g = (packed >> 10) & 0x3ff;
                    uint b = (packed >> 20) & 0x3ff;
                    uint a = (packed >> 30) & 0x003;

                    if (format.compType == FormatComponentType.UInt)
                    {
                        ret.Add(r);
                        ret.Add(g);
                        ret.Add(b);
                        ret.Add(a);
                    }
                    else
                    {
                        ret.Add((float)r / 1023.0f);
                        ret.Add((float)g / 1023.0f);
                        ret.Add((float)b / 1023.0f);
                        ret.Add((float)a / 3.0f);
                    }
                }
            }
            else if (format.special && format.specialFormat == SpecialFormat.R11G11B10)
            {
                uint packed = read.ReadUInt32();

                uint xMantissa = ((packed >> 0) & 0x3f);
                uint xExponent = ((packed >> 6) & 0x1f);
                uint yMantissa = ((packed >> 11) & 0x3f);
                uint yExponent = ((packed >> 17) & 0x1f);
                uint zMantissa = ((packed >> 22) & 0x1f);
                uint zExponent = ((packed >> 27) & 0x1f);

                ret.Add(((float)(xMantissa) / 64.0f) * Math.Pow(2.0f, (float)xExponent - 15.0f));
                ret.Add(((float)(yMantissa) / 32.0f) * Math.Pow(2.0f, (float)yExponent - 15.0f));
                ret.Add(((float)(zMantissa) / 32.0f) * Math.Pow(2.0f, (float)zExponent - 15.0f));
            }
            else
            {
                int dim = (int)(Math.Max(matrixdim, 1) * format.compCount);

                for (int i = 0; i < dim; i++)
                {
                    if (format.compType == FormatComponentType.Float)
                    {
                        if (format.compByteWidth == 8)
                            ret.Add(read.ReadDouble());
                        else if (format.compByteWidth == 4)
                            ret.Add(read.ReadSingle());
                        else if (format.compByteWidth == 2)
                            ret.Add(format.ConvertFromHalf(read.ReadUInt16()));
                    }
                    else if (format.compType == FormatComponentType.SInt)
                    {
                        if (format.compByteWidth == 4)
                            ret.Add((int)read.ReadInt32());
                        else if (format.compByteWidth == 2)
                            ret.Add((int)read.ReadInt16());
                        else if (format.compByteWidth == 1)
                            ret.Add((int)read.ReadSByte());
                    }
                    else if (format.compType == FormatComponentType.UInt)
                    {
                        if (format.compByteWidth == 4)
                            ret.Add((uint)read.ReadUInt32());
                        else if (format.compByteWidth == 2)
                            ret.Add((uint)read.ReadUInt16());
                        else if (format.compByteWidth == 1)
                            ret.Add((uint)read.ReadByte());
                    }
                    else if (format.compType == FormatComponentType.Depth)
                    {
                        float f = (float)read.ReadUInt32();
                        if (format.compByteWidth == 4)
                            ret.Add(f / (float)uint.MaxValue);
                        else if (format.compByteWidth == 3)
                            ret.Add(f / (float)0x00ffffff);
                        else if (format.compByteWidth == 2)
                            ret.Add(f / (float)0xffff);
                    }
                    else if (format.compType == FormatComponentType.Double)
                    {
                        ret.Add(read.ReadDouble());
                    }
                    else
                    {
                        // unorm/snorm

                        if (format.compByteWidth == 4)
                        {
                            renderdoc.StaticExports.LogText("Unexpected 4-byte unorm/snorm value");
                            ret.Add((float)read.ReadUInt32() / (float)uint.MaxValue); // should never hit this - no 32bit unorm/snorm type
                        }
                        else if (format.compByteWidth == 2)
                        {
                            ret.Add(format.Interpret(read.ReadUInt16()));
                        }
                        else if (format.compByteWidth == 1)
                        {
                            ret.Add(format.Interpret(read.ReadByte()));
                        }
                    }
                }
            }

            return ret.ToArray();
        }

        public ShaderVariable GetShaderVar(BinaryReader read)
        {
            object[] objs = GetObjects(read);

            ShaderVariable ret = new ShaderVariable();

            ret.name = name;
            ret.type = VarType.Float;
            if (format.compType == FormatComponentType.UInt)
                ret.type = VarType.UInt;
            if (format.compType == FormatComponentType.SInt)
                ret.type = VarType.Int;
            if (format.compType == FormatComponentType.Double)
                ret.type = VarType.Double;

            ret.columns = Math.Min(format.compCount, 4);
            ret.rows = Math.Min(matrixdim, 4);

            ret.members = new ShaderVariable[0] { };

            ret.value.fv = new float[16];
            ret.value.uv = new uint[16];
            ret.value.iv = new int[16];
            ret.value._dv_arr = new double[16];

            for (uint row = 0; row < ret.rows; row++)
            {
                for (uint col = 0; col < ret.columns; col++)
                {
                    uint dst = row * ret.columns + col;
                    uint src = row * format.compCount + col;

                    object o = objs[src];

                    if (o is double)
                        ret.value.dv[dst] = (double)o;
                    else if (o is float)
                        ret.value.fv[dst] = (float)o;
                    else if (o is uint)
                        ret.value.uv[dst] = (uint)o;
                    else if (o is int)
                        ret.value.iv[dst] = (int)o;
                }
            }

            return ret;
        }

        static public FormatElement[] ParseFormatString(string formatString, bool tightPacking, out string errors)
        {
            var elems = new List<FormatElement>();

            var formatReader = new StringReader(formatString);

            // regex doesn't account for trailing or preceeding whitespace, or comments

            var regExpr = @"^(row_major\s+)?" + // row_major matrix
                          @"(" +
                          @"uintten|unormten" +
                          @"|unormh|unormb" +
                          @"|snormh|snormb" +
                          @"|bool" + // bool is stored as 4-byte int in hlsl
                          @"|byte|short|int" + // signed ints
                          @"|ubyte|ushort|uint" + // unsigned ints
                          @"|xbyte|xshort|xint" + // hex ints
                          @"|half|float|double" + // float types
                          @")" +
                          @"([1-9])?" + // might be a vector
                          @"(x[1-9])?" + // or a matrix
                          @"(\s+[A-Za-z_][A-Za-z0-9_]*)?" + // get identifier name
                          @"(\[[0-9]+\])?" + // optional array dimension
                          @"(\s*:\s*[A-Za-z_][A-Za-z0-9_]*)?" + // optional semantic
                          @"$";

            Regex regParser = new Regex(regExpr, RegexOptions.Compiled);

            bool success = true;
            errors = "";

            var text = formatReader.ReadToEnd();

            text = text.Replace("{", "").Replace("}", "");

            Regex c_comments = new Regex(@"/\*[^*]*\*+(?:[^*/][^*]*\*+)*/", RegexOptions.Compiled);
            text = c_comments.Replace(text, "");

            Regex cpp_comments = new Regex(@"//.*", RegexOptions.Compiled);
            text = cpp_comments.Replace(text, "");

            uint offset = 0;

            // get each line and parse it to determine the format the user wanted
            foreach (var l in text.Split(';'))
            {
                var line = l;
                line = line.Trim();

                if (line.Length == 0) continue;

                var match = regParser.Match(line);

                if (!match.Success)
                {
                    errors = "Couldn't parse line:\n" + line;
                    success = false;
                    break;
                }

                var basetype = match.Groups[2].Value;
                bool row_major = match.Groups[1].Success;
                var vectorDim = match.Groups[3].Success ? match.Groups[3].Value : "1";
                var matrixDim = match.Groups[4].Success ? match.Groups[4].Value.Substring(1) : "1";
                var name = match.Groups[5].Success ? match.Groups[5].Value.Trim() : "data";
                var arrayDim = match.Groups[6].Success ? match.Groups[6].Value.Trim() : "[1]";
                arrayDim = arrayDim.Substring(1, arrayDim.Length - 2);

                if (match.Groups[4].Success)
                {
                    var a = vectorDim;
                    vectorDim = matrixDim;
                    matrixDim = a;
                }

                ResourceFormat fmt = new ResourceFormat(FormatComponentType.None, 0, 0);

                bool hex = false;

                FormatComponentType type = FormatComponentType.Float;
                uint count = 0;
                uint arrayCount = 1;
                uint matrixCount = 0;
                uint width = 0;

                // calculate format
                {
                    if (!uint.TryParse(vectorDim, out count))
                    {
                        errors = "Invalid vector dimension on line:\n" + line;
                        success = false;
                        break;
                    }
                    if (!uint.TryParse(arrayDim, out arrayCount))
                    {
                        arrayCount = 1;
                    }
                    arrayCount = Math.Max(0, arrayCount);
                    if (!uint.TryParse(matrixDim, out matrixCount))
                    {
                        errors = "Invalid matrix second dimension on line:\n" + line;
                        success = false;
                        break;
                    }

                    if (basetype == "bool")
                    {
                        type = FormatComponentType.UInt;
                        width = 4;
                    }
                    else if (basetype == "byte")
                    {
                        type = FormatComponentType.SInt;
                        width = 1;
                    }
                    else if (basetype == "ubyte" || basetype == "xbyte")
                    {
                        type = FormatComponentType.UInt;
                        width = 1;
                    }
                    else if (basetype == "short")
                    {
                        type = FormatComponentType.SInt;
                        width = 2;
                    }
                    else if (basetype == "ushort" || basetype == "xshort")
                    {
                        type = FormatComponentType.UInt;
                        width = 2;
                    }
                    else if (basetype == "int")
                    {
                        type = FormatComponentType.SInt;
                        width = 4;
                    }
                    else if (basetype == "uint" || basetype == "xint")
                    {
                        type = FormatComponentType.UInt;
                        width = 4;
                    }
                    else if (basetype == "half")
                    {
                        type = FormatComponentType.Float;
                        width = 2;
                    }
                    else if (basetype == "float")
                    {
                        type = FormatComponentType.Float;
                        width = 4;
                    }
                    else if (basetype == "double")
                    {
                        type = FormatComponentType.Double;
                        width = 8;
                    }
                    else if (basetype == "unormh")
                    {
                        type = FormatComponentType.UNorm;
                        width = 2;
                    }
                    else if (basetype == "unormb")
                    {
                        type = FormatComponentType.UNorm;
                        width = 1;
                    }
                    else if (basetype == "snormh")
                    {
                        type = FormatComponentType.SNorm;
                        width = 2;
                    }
                    else if (basetype == "snormb")
                    {
                        type = FormatComponentType.SNorm;
                        width = 1;
                    }
                    else if (basetype == "uintten")
                    {
                        fmt = new ResourceFormat(FormatComponentType.UInt, 4 * count, 1);
                        fmt.special = true;
                        fmt.specialFormat = SpecialFormat.R10G10B10A2;
                    }
                    else if (basetype == "unormten")
                    {
                        fmt = new ResourceFormat(FormatComponentType.UNorm, 4 * count, 1);
                        fmt.special = true;
                        fmt.specialFormat = SpecialFormat.R10G10B10A2;
                    }
                    else
                    {
                        errors = "Unrecognised basic type on line:\n" + line;
                        success = false;
                        break;
                    }
                }

                if (basetype == "xint" || basetype == "xshort" || basetype == "xbyte")
                    hex = true;

                if (fmt.compType == FormatComponentType.None)
                    fmt = new ResourceFormat(type, count, width);

                if (arrayCount == 1)
                {
                    FormatElement elem = new FormatElement(name, 0, offset, false, row_major, matrixCount, fmt, hex);

                    uint advance = elem.ByteSize;

                    if (!tightPacking)
                    {
                        // cbuffer packing always works in floats
                        advance = (advance + 3U) & (~3U);

                        // cbuffer packing doesn't allow elements to cross float4 boundaries, nudge up if this was the case
                        if (offset / 16 != (offset + elem.ByteSize - 1) / 16)
                        {
                            elem.offset = offset = (offset + 0xFU) & (~0xFU);
                        }
                    }

                    elems.Add(elem);

                    offset += advance;
                }
                else
                {
                    // when cbuffer packing, arrays are always aligned at float4 boundary
                    if (!tightPacking)
                    {
                        if (offset % 16 != 0)
                        {
                            offset = (offset + 0xFU) & (~0xFU);
                        }
                    }

                    for (uint a = 0; a < arrayCount; a++)
                    {
                        FormatElement elem = new FormatElement(String.Format("{0}[{1}]", name, a), 0, offset, false, row_major, matrixCount, fmt, hex);

                        elems.Add(elem);

                        uint advance = elem.ByteSize;

                        // cbuffer packing each array element is always float4 aligned
                        if (!tightPacking)
                        {
                            advance = (advance + 0xFU) & (~0xFU);
                        }

                        offset += advance;
                    }
                }
            }

            if (!success || elems.Count == 0)
            {
                elems.Clear();

                var fmt = new ResourceFormat(FormatComponentType.UInt, 4, 4);

                elems.Add(new FormatElement("data", 0, 0, false, false, 1, fmt, true));
            }

            return elems.ToArray();
        }

        public string name;
        public int buffer;
        public uint offset;
        public bool perinstance;
        public bool rowmajor;
        public uint matrixdim;
        public ResourceFormat format;
        public bool hex;
    }
}
