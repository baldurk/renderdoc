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
using System.Linq;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Collections.Generic;

namespace renderdoc
{
    // corresponds to rdctype::array on the C side
    [StructLayout(LayoutKind.Sequential)]
    public struct templated_array
    {
        public IntPtr elems;
        public Int32 count;
    };
    
    public enum CustomUnmanagedType
    {
        TemplatedArray = 0,
        UTF8TemplatedString,
        FixedArray,
        Union,
        Skip,
        CustomClass,
        CustomClassPointer,
    }

    public enum CustomFixedType
    {
        None = 0,
        Float,
        UInt32,
        Int32,
        UInt16,
        Double,
    }

    // custom attribute that we can apply to structures we want to serialise
    public sealed class CustomMarshalAsAttribute : Attribute
    {
        public CustomMarshalAsAttribute(CustomUnmanagedType unmanagedType)
        {
            m_UnmanagedType = unmanagedType;
        }

        public CustomUnmanagedType CustomType
        {
            get { return m_UnmanagedType; }
        }

        public int FixedLength
        {
            get { return m_FixedLen; }
            set { m_FixedLen = value; }
        }

        public CustomFixedType FixedType
        {
            get { return m_FixedType; }
            set { m_FixedType = value; }
        }

        private CustomUnmanagedType m_UnmanagedType;
        private int m_FixedLen;
        private CustomFixedType m_FixedType = CustomFixedType.None;
    }

    // custom marshalling code to handle converting complex data types with our given formatting
    // over to .NET managed copies.
    public static class CustomMarshal
    {
        [DllImport("kernel32.dll", EntryPoint = "RtlFillMemory", SetLastError = false)]
        private static extern void FillMemory(IntPtr destination, int length, byte fill);

        [DllImport("renderdoc.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void RENDERDOC_FreeArrayMem(IntPtr mem);

        // utility functions usable by wrappers around actual functions calling into the C++ core
        public static IntPtr Alloc(Type T)
        {
            IntPtr mem = Marshal.AllocHGlobal(CustomMarshal.SizeOf(T));
            FillMemory(mem, CustomMarshal.SizeOf(T), 0);

            return mem;
        }

        public static IntPtr Alloc(Type T, int arraylen)
        {
            IntPtr mem = Marshal.AllocHGlobal(CustomMarshal.SizeOf(T)*arraylen);
            FillMemory(mem, CustomMarshal.SizeOf(T) * arraylen, 0);

            return mem;
        }

        public static IntPtr MakeUTF8String(string s)
        {
            int len = System.Text.Encoding.UTF8.GetByteCount(s);

            IntPtr mem = Marshal.AllocHGlobal(len + 1);
            byte[] bytes = new byte[len + 1];
            bytes[len] = 0; // add NULL terminator
            System.Text.Encoding.UTF8.GetBytes(s, 0, s.Length, bytes, 0);

            Marshal.Copy(bytes, 0, mem, len+1);

            return mem;
        }

        public static void Free(IntPtr mem)
        {
            Marshal.FreeHGlobal(mem);
        }

        // note that AlignOf and AddFieldSize and others are called rarely as the results
        // are cached lower down

        // match C/C++ alignment rules
        private static int AlignOf(FieldInfo field)
        {
            var cma = GetCustomAttr(field);

            if (cma != null &&
                (cma.CustomType == CustomUnmanagedType.UTF8TemplatedString ||
                 cma.CustomType == CustomUnmanagedType.TemplatedArray ||
                 cma.CustomType == CustomUnmanagedType.CustomClassPointer)
                )
                return IntPtr.Size;

            if (cma != null && cma.CustomType == CustomUnmanagedType.Skip)
                return 1;

            if (field.FieldType.IsPrimitive ||
                (field.FieldType.IsArray && field.FieldType.GetElementType().IsPrimitive))
                return Marshal.SizeOf(NonArrayType(field.FieldType));

            // Get instance fields of the structure type. 
            FieldInfo[] fieldInfo = NonArrayType(field.FieldType).GetFields(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);

            int align = 1;

            foreach (FieldInfo f in fieldInfo)
                align = Math.Max(align, AlignOf(f));

            return align;
        }

        private static Type NonArrayType(Type t)
        {
            return t.IsArray ? t.GetElementType() : t;
        }

        private static Dictionary<Type, CustomMarshalAsAttribute[]> m_CustomAttrCache = new Dictionary<Type, CustomMarshalAsAttribute[]>();

        private static CustomMarshalAsAttribute GetCustomAttr(Type t, FieldInfo[] fields, int fieldIdx)
        {
            lock (m_CustomAttrCache)
            {
                if (!m_CustomAttrCache.ContainsKey(t))
                {
                    var arr = new CustomMarshalAsAttribute[fields.Length];

                    for (int i = 0; i < fields.Length; i++)
                        arr[i] = GetCustomAttr(fields[i]);

                    m_CustomAttrCache.Add(t, arr);
                }

                return m_CustomAttrCache[t][fieldIdx];
            }
        }

        private static CustomMarshalAsAttribute GetCustomAttr(FieldInfo field)
        {
            if (CustomAttributeDefined(field))
            {
                object[] attributes = field.GetCustomAttributes(false);
                foreach (object attribute in attributes)
                {
                    if (attribute is CustomMarshalAsAttribute)
                    {
                        return (attribute as CustomMarshalAsAttribute);
                    }
                }
            }

            return null;
        }

        // add a field's size to the size parameter, respecting alignment
        private static void AddFieldSize(FieldInfo field, ref long size)
        {
            int a = AlignOf(field);
            int alignment = (int)size % a;
            if (alignment != 0) size += a - alignment;

            var cma = GetCustomAttr(field);
            if (cma != null)
            {
                switch (cma.CustomType)
                {
                    case CustomUnmanagedType.CustomClass:
                        size += SizeOf(field.FieldType);
                        break;
                    case CustomUnmanagedType.CustomClassPointer:
                        size += IntPtr.Size;
                        break;
                    case CustomUnmanagedType.Union:
                        {
                            FieldInfo[] fieldInfo = field.FieldType.GetFields(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);

                            long unionsize = 0;

                            foreach (FieldInfo unionfield in fieldInfo)
                            {
                                long maxsize = 0;

                                AddFieldSize(unionfield, ref maxsize);

                                unionsize = Math.Max(unionsize, maxsize);
                            }

                            size += unionsize;
                            break;
                        }
                    case CustomUnmanagedType.TemplatedArray:
                    case CustomUnmanagedType.UTF8TemplatedString:
                        size += Marshal.SizeOf(typeof(templated_array));
                        break;
                    case CustomUnmanagedType.FixedArray:
                        size += cma.FixedLength * SizeOf(NonArrayType(field.FieldType));
                        break;
                    case CustomUnmanagedType.Skip:
                        break;
                    default:
                        throw new NotImplementedException("Unexpected attribute");
                }
            }
            else
            {
                size += SizeOf(field.FieldType);
            }

            alignment = (int)size % a;
            if (alignment != 0) size += a - alignment;
        }

        // cache for sizes of types, since this will get called a lot
        private static Dictionary<Type, int> m_SizeCache = new Dictionary<Type, int>();

        // return the size of the C++ equivalent of this type (so that we can allocate enough)
        // space to pass a pointer for example.
        private static int SizeOf(Type structureType)
        {
            if (structureType.IsPrimitive ||
                (structureType.IsArray && structureType.GetElementType().IsPrimitive))
                return Marshal.SizeOf(structureType);

            lock (m_SizeCache)
            {
                if (m_SizeCache.ContainsKey(structureType))
                    return m_SizeCache[structureType];

                // Get instance fields of the structure type. 
                FieldInfo[] fieldInfo = structureType.GetFields(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance);

                long size = 0;

                int a = 1;

                foreach (FieldInfo field in fieldInfo)
                {
                    AddFieldSize(field, ref size);
                    a = Math.Max(a, AlignOf(field));
                }

                int alignment = (int)size % a;
                if (alignment != 0) size += a - alignment;

                m_SizeCache.Add(structureType, (int)size);

                return (int)size;
            }
        }

        // caching the offset to the nth field from a base pointer to the type
        private static Dictionary<Type, Int64[]> m_OffsetCache = new Dictionary<Type, Int64[]>();
        // caching how much to align up a pointer by to the first field (above offsets take care after that)
        private static Dictionary<Type, int> m_OffsetAlignCache = new Dictionary<Type, int>();

        // offset a pointer to the idx'th field of a type
        private static IntPtr OffsetPtr(Type structureType, FieldInfo[] fieldInfo, int idx, IntPtr ptr)
        {
            if (fieldInfo.Length == 0)
                return ptr;

            lock (m_OffsetCache)
            {
                if (!m_OffsetAlignCache.ContainsKey(structureType))
                {
                    Int64[] cacheOffsets = new Int64[fieldInfo.Length];
                    int initialAlign = AlignOf(fieldInfo[0]);

                    Int64 p = 0;

                    for (int i = 0; i < fieldInfo.Length; i++)
                    {
                        FieldInfo field = fieldInfo[i];

                        int a = AlignOf(field);
                        int alignment = (int)p % a;
                        if (alignment != 0) p += a - alignment;

                        cacheOffsets[i] = p;

                        AddFieldSize(field, ref p);
                    }

                    m_OffsetAlignCache.Add(structureType, initialAlign);
                    m_OffsetCache.Add(structureType, cacheOffsets);
                }

                {
                    var p = ptr.ToInt64();

                    int a = m_OffsetAlignCache[structureType];
                    int alignment = (int)p % a;
                    if (alignment != 0) p += a - alignment;

                    p += m_OffsetCache[structureType][idx];

                    return new IntPtr(p);
                }
            }
        }

        private static bool CustomAttributeDefined(FieldInfo field)
        {
            return field.IsDefined(typeof(CustomMarshalAsAttribute), false);
        }

        // this function takes a pointer to a templated array (ie. a pointer to a list of Types, and a length)
        // and returns an array of that object type, and cleans up the memory if specified.
        public static object GetTemplatedArray(IntPtr sourcePtr, Type structureType, bool freeMem)
        {
            templated_array arr = (templated_array)Marshal.PtrToStructure(sourcePtr, typeof(templated_array));

            if (structureType == typeof(byte))
            {
                byte[] val = new byte[arr.count];
                if(val.Length > 0)
                    Marshal.Copy(arr.elems, val, 0, val.Length);

                if (freeMem)
                    RENDERDOC_FreeArrayMem(arr.elems);

                return val;
            }
            else
            {
                Array val = Array.CreateInstance(structureType, arr.count);

                int sizeInBytes = SizeOf(structureType);

                for (int i = 0; i < val.Length; i++)
                {
                    IntPtr p = new IntPtr((arr.elems.ToInt64() + i * sizeInBytes));

                    val.SetValue(PtrToStructure(p, structureType, freeMem), i);
                }

                if (freeMem)
                    RENDERDOC_FreeArrayMem(arr.elems);

                return val;
            }
        }

        public static string PtrToStringUTF8(IntPtr elems, int count)
        {
            byte[] buffer = new byte[count];
            if (count > 0)
                Marshal.Copy(elems, buffer, 0, buffer.Length);
            return System.Text.Encoding.UTF8.GetString(buffer);
        }

        public static string PtrToStringUTF8(IntPtr elems)
        {
            int len = 0;
            while (Marshal.ReadByte(elems, len) != 0) ++len;
            return PtrToStringUTF8(elems, len);
        }

        // specific versions of the above GetTemplatedArray for convenience.
        public static string TemplatedArrayToString(IntPtr sourcePtr, bool freeMem)
        {
            templated_array arr = (templated_array)Marshal.PtrToStructure(sourcePtr, typeof(templated_array));

            string val = PtrToStringUTF8(arr.elems, arr.count);

            if (freeMem)
                RENDERDOC_FreeArrayMem(arr.elems);

            return val;
        }

        public static string[] TemplatedArrayToStringArray(IntPtr sourcePtr, bool freeMem)
        {
            templated_array arr = (templated_array)Marshal.PtrToStructure(sourcePtr, typeof(templated_array));

            int arrSize = SizeOf(typeof(templated_array));

            string[] ret = new string[arr.count];
            for (int i = 0; i < arr.count; i++)
            {
                IntPtr ptr = new IntPtr((arr.elems.ToInt64() + i * arrSize));

                ret[i] = TemplatedArrayToString(ptr, freeMem);
            }

            if (freeMem)
                RENDERDOC_FreeArrayMem(arr.elems);

            return ret;
        }

        public static object PtrToStructure(IntPtr sourcePtr, Type structureType, bool freeMem)
        {
            return PtrToStructure(sourcePtr, structureType, freeMem, false);
        }

        // take a pointer to a C++ structure of a given type, and convert it into the managed equivalent,
        // while handling alignment etc and freeing memory returned if it should be caller-freed
        private static object PtrToStructure(IntPtr sourcePtr, Type structureType, bool freeMem, bool isUnion)
        {
            if (sourcePtr == IntPtr.Zero)
                return null;

            // Get instance fields of the structure type. 
            FieldInfo[] fields = structureType.GetFields(BindingFlags.NonPublic | BindingFlags.Public | BindingFlags.Instance)
                .OrderBy(field => field.MetadataToken).ToArray();

            object ret = Activator.CreateInstance(structureType);

            try
            {
                for (int fieldIdx = 0; fieldIdx < fields.Length; fieldIdx++)
                {
                    FieldInfo field = fields[fieldIdx];

                    IntPtr fieldPtr = isUnion ? sourcePtr : OffsetPtr(structureType, fields, fieldIdx, sourcePtr);

                    // no custom attribute, so just use the regular Marshal code
                    var cma = GetCustomAttr(structureType, fields, fieldIdx);
                    if (cma == null)
                    {
                        if (field.FieldType.IsEnum)
                            field.SetValue(ret, (VarType)Marshal.ReadInt32(fieldPtr));
                        else
                            field.SetValue(ret, Marshal.PtrToStructure(fieldPtr, field.FieldType));
                    }
                    else
                    {
                        switch (cma.CustomType)
                        {
                            case CustomUnmanagedType.CustomClass:
                                field.SetValue(ret, PtrToStructure(fieldPtr, field.FieldType, freeMem));
                                break;
                            case CustomUnmanagedType.CustomClassPointer:
                                IntPtr ptr = Marshal.ReadIntPtr(fieldPtr);
                                if (ptr == IntPtr.Zero)
                                    field.SetValue(ret, null);
                                else
                                    field.SetValue(ret, PtrToStructure(ptr, field.FieldType, freeMem));
                                break;
                            case CustomUnmanagedType.Union:
                                field.SetValue(ret, PtrToStructure(fieldPtr, field.FieldType, freeMem, true));
                                break;
                            case CustomUnmanagedType.Skip:
                                break;
                            case CustomUnmanagedType.FixedArray:
                                {
                                    if(cma.FixedType == CustomFixedType.Float)
                                    {
                                        float[] val = new float[cma.FixedLength];
                                        Marshal.Copy(fieldPtr, val, 0, cma.FixedLength);
                                        field.SetValue(ret, val);
                                    }
                                    else if (cma.FixedType == CustomFixedType.UInt16)
                                    {
                                        Int16[] val = new Int16[cma.FixedLength];
                                        Marshal.Copy(fieldPtr, val, 0, cma.FixedLength);
                                        UInt16[] realval = new UInt16[cma.FixedLength];
                                        for (int i = 0; i < val.Length; i++)
                                            realval[i] = unchecked((UInt16)val[i]);
                                        field.SetValue(ret, val);
                                    }
                                    else if (cma.FixedType == CustomFixedType.Int32)
                                    {
                                        Int32[] val = new Int32[cma.FixedLength];
                                        Marshal.Copy(fieldPtr, val, 0, cma.FixedLength);
                                        field.SetValue(ret, val);
                                    }
                                    else if (cma.FixedType == CustomFixedType.Double)
                                    {
                                        double[] val = new double[cma.FixedLength];
                                        Marshal.Copy(fieldPtr, val, 0, cma.FixedLength);
                                        field.SetValue(ret, val);
                                    }
                                    else if (cma.FixedType == CustomFixedType.UInt32)
                                    {
                                        Int32[] val = new Int32[cma.FixedLength];
                                        Marshal.Copy(fieldPtr, val, 0, cma.FixedLength);
                                        UInt32[] realval = new UInt32[cma.FixedLength];
                                        for (int i = 0; i < val.Length; i++)
                                            realval[i] = unchecked((UInt32)val[i]);
                                        field.SetValue(ret, realval);
                                    }
                                    else
                                    {
                                        var arrayType = NonArrayType(field.FieldType);
                                        int sizeInBytes = SizeOf(arrayType);

                                        Array val = Array.CreateInstance(arrayType, cma.FixedLength);

                                        for (int i = 0; i < val.Length; i++)
                                        {
                                            IntPtr p = new IntPtr((fieldPtr.ToInt64() + i * sizeInBytes));

                                            val.SetValue(PtrToStructure(p, arrayType, freeMem), i);
                                        }

                                        field.SetValue(ret, val);
                                    }
                                    break;
                                }
                            case CustomUnmanagedType.UTF8TemplatedString:
                            case CustomUnmanagedType.TemplatedArray:
                                {
                                    // templated_array must be pointer-aligned
                                    long alignment = fieldPtr.ToInt64() % IntPtr.Size;
                                    if (alignment != 0)
                                    {
                                        fieldPtr = new IntPtr(fieldPtr.ToInt64() + IntPtr.Size - alignment);
                                    }

                                    templated_array arr = (templated_array)Marshal.PtrToStructure(fieldPtr, typeof(templated_array));
                                    if (field.FieldType == typeof(string))
                                    {
                                        if (arr.elems == IntPtr.Zero)
                                            field.SetValue(ret, "");
                                        else
                                            field.SetValue(ret, PtrToStringUTF8(arr.elems, arr.count));
                                    }
                                    else
                                    {
                                        var arrayType = NonArrayType(field.FieldType);
                                        int sizeInBytes = SizeOf(arrayType);

                                        if (field.FieldType.IsArray && arrayType == typeof(byte))
                                        {
                                            byte[] val = new byte[arr.count];
                                            if(val.Length > 0)
                                                Marshal.Copy(arr.elems, val, 0, val.Length);
                                            field.SetValue(ret, val);
                                        }
                                        else if (field.FieldType.IsArray)
                                        {
                                            Array val = Array.CreateInstance(arrayType, arr.count);

                                            for (int i = 0; i < val.Length; i++)
                                            {
                                                IntPtr p = new IntPtr((arr.elems.ToInt64() + i * sizeInBytes));

                                                val.SetValue(PtrToStructure(p, arrayType, freeMem), i);
                                            }

                                            field.SetValue(ret, val);
                                        }
                                        else
                                        {
                                            throw new NotImplementedException("non-array element marked to marshal as TemplatedArray");
                                        }
                                    }
                                    if(freeMem)
                                        RENDERDOC_FreeArrayMem(arr.elems);
                                    break;
                                }
                            default:
                                throw new NotImplementedException("Unexpected attribute");
                        }
                    }
                }

                MethodInfo postMarshal = structureType.GetMethod("PostMarshal", BindingFlags.NonPublic | BindingFlags.Instance);
                if (postMarshal != null)
                    postMarshal.Invoke(ret, new object[] { });
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.Fail(ex.Message);
            }

            return ret;
        }
    }
}