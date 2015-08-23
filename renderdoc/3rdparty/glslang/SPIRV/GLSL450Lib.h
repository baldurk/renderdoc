/*
** Copyright (c) 2014-2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

//
// Author: John Kessenich, LunarG
//

namespace GLSL_STD_450 {

const int Version = 99;
const int Revision = 1;

enum Entrypoints {
    Round = 0,
    RoundEven = 1,
    Trunc = 2,
    Abs = 3,
    Sign = 4,
    Floor = 5,
    Ceil = 6,
    Fract = 7,

    Radians = 8,
    Degrees = 9,
    Sin = 10,
    Cos = 11,
    Tan = 12,
    Asin = 13,
    Acos = 14,
    Atan = 15,
    Sinh = 16,
    Cosh = 17,
    Tanh = 18,
    Asinh = 19,
    Acosh = 20,
    Atanh = 21,
    Atan2 = 22,

    Pow = 23,
    Exp = 24,
    Log = 25,
    Exp2 = 26,
    Log2 = 27,
    Sqrt = 28,
    InverseSqrt = 29,

    Determinant = 30,
    MatrixInverse = 31,

    Modf = 32,            // second argument needs the OpVariable = , not an OpLoad
    Min = 33,
    Max = 34,
    Clamp = 35,
    Mix = 36,
    Step = 37,
    SmoothStep = 38,

    FloatBitsToInt = 39,
    FloatBitsToUint = 40,
    IntBitsToFloat = 41,
    UintBitsToFloat = 42,

    Fma = 43,
    Frexp = 44,
    Ldexp = 45,

    PackSnorm4x8 = 46,
    PackUnorm4x8 = 47,
    PackSnorm2x16 = 48,
    PackUnorm2x16 = 49,
    PackHalf2x16 = 50,
    PackDouble2x32 = 51,
    UnpackSnorm2x16 = 52,
    UnpackUnorm2x16 = 53,
    UnpackHalf2x16 = 54,
    UnpackSnorm4x8 = 55,
    UnpackUnorm4x8 = 56,
    UnpackDouble2x32 = 57,

    Length = 58,
    Distance = 59,
    Cross = 60,
    Normalize = 61,
    Ftransform = 62,
    FaceForward = 63,
    Reflect = 64,
    Refract = 65,

    UaddCarry = 66,
    UsubBorrow = 67,
    UmulExtended = 68,
    ImulExtended = 69,
    BitfieldExtract = 70,
    BitfieldInsert = 71,
    BitfieldReverse = 72,
    BitCount = 73,
    FindLSB = 74,
    FindMSB = 75,

    InterpolateAtCentroid = 76,
    InterpolateAtSample = 77,
    InterpolateAtOffset = 78,

    Count
};

inline void GetDebugNames(const char** names)
{
    for (int i = 0; i < Count; ++i)
        names[i] = "Unknown";

    names[Round]                   = "round";
    names[RoundEven]               = "roundEven";
    names[Trunc]                   = "trunc";
    names[Abs]                     = "abs";
    names[Sign]                    = "sign";
    names[Floor]                   = "floor";
    names[Ceil]                    = "ceil";
    names[Fract]                   = "fract";
    names[Radians]                 = "radians";
    names[Degrees]                 = "degrees";
    names[Sin]                     = "sin";
    names[Cos]                     = "cos";
    names[Tan]                     = "tan";
    names[Asin]                    = "asin";
    names[Acos]                    = "acos";
    names[Atan]                    = "atan";
    names[Sinh]                    = "sinh";
    names[Cosh]                    = "cosh";
    names[Tanh]                    = "tanh";
    names[Asinh]                   = "asinh";
    names[Acosh]                   = "acosh";
    names[Atanh]                   = "atanh";
    names[Atan2]                   = "atan2";
    names[Pow]                     = "pow";
    names[Exp]                     = "exp";
    names[Log]                     = "log";
    names[Exp2]                    = "exp2";
    names[Log2]                    = "log2";
    names[Sqrt]                    = "sqrt";
    names[InverseSqrt]             = "inverseSqrt";
    names[Determinant]             = "determinant";
    names[MatrixInverse]           = "matrixInverse";
    names[Modf]                    = "modf";
    names[Min]                     = "min";
    names[Max]                     = "max";
    names[Clamp]                   = "clamp";
    names[Mix]                     = "mix";
    names[Step]                    = "step";
    names[SmoothStep]              = "smoothStep";
    names[FloatBitsToInt]          = "floatBitsToInt";
    names[FloatBitsToUint]         = "floatBitsToUint";
    names[IntBitsToFloat]          = "intBitsToFloat";
    names[UintBitsToFloat]         = "uintBitsToFloat";
    names[Fma]                     = "fma";
    names[Frexp]                   = "frexp";
    names[Ldexp]                   = "ldexp";
    names[PackSnorm4x8]            = "packSnorm4x8";
    names[PackUnorm4x8]            = "packUnorm4x8";
    names[PackSnorm2x16]           = "packSnorm2x16";
    names[PackUnorm2x16]           = "packUnorm2x16";
    names[PackHalf2x16]            = "packHalf2x16";
    names[PackDouble2x32]          = "packDouble2x32";
    names[UnpackSnorm2x16]         = "unpackSnorm2x16";
    names[UnpackUnorm2x16]         = "unpackUnorm2x16";
    names[UnpackHalf2x16]          = "unpackHalf2x16";
    names[UnpackSnorm4x8]          = "unpackSnorm4x8";
    names[UnpackUnorm4x8]          = "unpackUnorm4x8";
    names[UnpackDouble2x32]        = "unpackDouble2x32";
    names[Length]                  = "length";
    names[Distance]                = "distance";
    names[Cross]                   = "cross";
    names[Normalize]               = "normalize";
    names[Ftransform]              = "ftransform";
    names[FaceForward]             = "faceForward";
    names[Reflect]                 = "reflect";
    names[Refract]                 = "refract";
    names[UaddCarry]               = "uaddCarry";
    names[UsubBorrow]              = "usubBorrow";
    names[UmulExtended]            = "umulExtended";
    names[ImulExtended]            = "imulExtended";
    names[BitfieldExtract]         = "bitfieldExtract";
    names[BitfieldInsert]          = "bitfieldInsert";
    names[BitfieldReverse]         = "bitfieldReverse";
    names[BitCount]                = "bitCount";
    names[FindLSB]                 = "findLSB";
    names[FindMSB]                 = "findMSB";
    names[InterpolateAtCentroid]   = "interpolateAtCentroid";
    names[InterpolateAtSample]     = "interpolateAtSample";
    names[InterpolateAtOffset]     = "interpolateAtOffset";
}

}; // end namespace GLSL_STD_450
