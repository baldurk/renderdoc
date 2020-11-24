static const char *ags_shader_intrinsics_dx12_hlsl_array[] = {R"EOSHADER(
//
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

/**
***********************************************************************************************************************
* @file  ags_shader_intrinsics_dx12.hlsl
* @brief
*    AMD D3D Shader Intrinsics HLSL include file.
*    This include file contains the Shader Intrinsics definitions used in shader code by the application.
* @note
*    This does not work with immediate values or values that the compiler determines can produces denorms
*
***********************************************************************************************************************
*/

#ifndef _AMDEXTD3DSHADERINTRINICS_HLSL
#define _AMDEXTD3DSHADERINTRINICS_HLSL

// Default AMD shader intrinsics designated SpaceId.
#define AmdExtD3DShaderIntrinsicsSpaceId space2147420894

// Dummy UAV used to access shader intrinsics. Applications need to add a root signature entry for this resource in
// order to use shader extensions. Applications may specify an alternate UAV binding by defining
// AMD_EXT_SHADER_INTRINSIC_UAV_OVERRIDE. The application must also call IAmdExtD3DShaderIntrinsics1::SetExtensionUavBinding()
// in order to use an alternate binding. This must be done before creating the root signature and pipeline.
#ifdef AMD_EXT_SHADER_INTRINSIC_UAV_OVERRIDE
RWByteAddressBuffer AmdExtD3DShaderIntrinsicsUAV : register(AMD_EXT_SHADER_INTRINSIC_UAV_OVERRIDE);
#else
RWByteAddressBuffer AmdExtD3DShaderIntrinsicsUAV : register(u0, AmdExtD3DShaderIntrinsicsSpaceId);
#endif

/**
***********************************************************************************************************************
*   Definitions to construct the intrinsic instruction composed of an opcode and optional immediate data.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsics_MagicCodeShift   28
#define AmdExtD3DShaderIntrinsics_MagicCodeMask    0xf
#define AmdExtD3DShaderIntrinsics_OpcodePhaseShift 24
#define AmdExtD3DShaderIntrinsics_OpcodePhaseMask  0x3
#define AmdExtD3DShaderIntrinsics_DataShift        8
#define AmdExtD3DShaderIntrinsics_DataMask         0xffff
#define AmdExtD3DShaderIntrinsics_OpcodeShift      0
#define AmdExtD3DShaderIntrinsics_OpcodeMask       0xff

#define AmdExtD3DShaderIntrinsics_MagicCode        0x5


/**
***********************************************************************************************************************
*   Intrinsic opcodes.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsOpcode_Readfirstlane     0x01
#define AmdExtD3DShaderIntrinsicsOpcode_Readlane          0x02
#define AmdExtD3DShaderIntrinsicsOpcode_LaneId            0x03
#define AmdExtD3DShaderIntrinsicsOpcode_Swizzle           0x04
#define AmdExtD3DShaderIntrinsicsOpcode_Ballot            0x05
#define AmdExtD3DShaderIntrinsicsOpcode_MBCnt             0x06
#define AmdExtD3DShaderIntrinsicsOpcode_Min3U             0x07
#define AmdExtD3DShaderIntrinsicsOpcode_Min3F             0x08
#define AmdExtD3DShaderIntrinsicsOpcode_Med3U             0x09
#define AmdExtD3DShaderIntrinsicsOpcode_Med3F             0x0a
#define AmdExtD3DShaderIntrinsicsOpcode_Max3U             0x0b
#define AmdExtD3DShaderIntrinsicsOpcode_Max3F             0x0c
#define AmdExtD3DShaderIntrinsicsOpcode_BaryCoord         0x0d
#define AmdExtD3DShaderIntrinsicsOpcode_VtxParam          0x0e
#define AmdExtD3DShaderIntrinsicsOpcode_Reserved1         0x0f
#define AmdExtD3DShaderIntrinsicsOpcode_Reserved2         0x10
#define AmdExtD3DShaderIntrinsicsOpcode_Reserved3         0x11
#define AmdExtD3DShaderIntrinsicsOpcode_WaveReduce        0x12
#define AmdExtD3DShaderIntrinsicsOpcode_WaveScan          0x13
#define AmdExtD3DShaderIntrinsicsOpcode_LoadDwAtAddr      0x14
#define AmdExtD3DShaderIntrinsicsOpcode_DrawIndex         0x17
#define AmdExtD3DShaderIntrinsicsOpcode_AtomicU64         0x18
#define AmdExtD3DShaderIntrinsicsOpcode_GetWaveSize       0x19
#define AmdExtD3DShaderIntrinsicsOpcode_BaseInstance      0x1a
#define AmdExtD3DShaderIntrinsicsOpcode_BaseVertex        0x1b
#define AmdExtD3DShaderIntrinsicsOpcode_FloatConversion   0x1c
#define AmdExtD3DShaderIntrinsicsOpcode_ReadlaneAt        0x1d

/**
***********************************************************************************************************************
*   Intrinsic opcode phases.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsOpcodePhase_0    0x0
#define AmdExtD3DShaderIntrinsicsOpcodePhase_1    0x1
#define AmdExtD3DShaderIntrinsicsOpcodePhase_2    0x2
#define AmdExtD3DShaderIntrinsicsOpcodePhase_3    0x3

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsWaveOp defines for supported operations. Can be used as the parameter for the
*   AmdExtD3DShaderIntrinsicsOpcode_WaveOp intrinsic.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsWaveOp_AddF 0x01
#define AmdExtD3DShaderIntrinsicsWaveOp_AddI 0x02
#define AmdExtD3DShaderIntrinsicsWaveOp_AddU 0x03
#define AmdExtD3DShaderIntrinsicsWaveOp_MulF 0x04
#define AmdExtD3DShaderIntrinsicsWaveOp_MulI 0x05
#define AmdExtD3DShaderIntrinsicsWaveOp_MulU 0x06
#define AmdExtD3DShaderIntrinsicsWaveOp_MinF 0x07
#define AmdExtD3DShaderIntrinsicsWaveOp_MinI 0x08
#define AmdExtD3DShaderIntrinsicsWaveOp_MinU 0x09
#define AmdExtD3DShaderIntrinsicsWaveOp_MaxF 0x0a
#define AmdExtD3DShaderIntrinsicsWaveOp_MaxI 0x0b
#define AmdExtD3DShaderIntrinsicsWaveOp_MaxU 0x0c
#define AmdExtD3DShaderIntrinsicsWaveOp_And  0x0d    // Reduction only
#define AmdExtD3DShaderIntrinsicsWaveOp_Or   0x0e    // Reduction only
#define AmdExtD3DShaderIntrinsicsWaveOp_Xor  0x0f    // Reduction only

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsWaveOp masks and shifts for opcode and flags
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift 0
#define AmdExtD3DShaderIntrinsicsWaveOp_OpcodeMask  0xff
#define AmdExtD3DShaderIntrinsicsWaveOp_FlagShift   8
#define AmdExtD3DShaderIntrinsicsWaveOp_FlagMask    0xff

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsWaveOp flags for use with AmdExtD3DShaderIntrinsicsOpcode_WaveScan.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsWaveOp_Inclusive   0x01
#define AmdExtD3DShaderIntrinsicsWaveOp_Exclusive   0x02

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsSwizzle defines for common swizzles.  Can be used as the operation parameter for the
*   AmdExtD3DShaderIntrinsics_Swizzle intrinsic.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsSwizzle_SwapX1      0x041f
#define AmdExtD3DShaderIntrinsicsSwizzle_SwapX2      0x081f
#define AmdExtD3DShaderIntrinsicsSwizzle_SwapX4      0x101f
#define AmdExtD3DShaderIntrinsicsSwizzle_SwapX8      0x201f
#define AmdExtD3DShaderIntrinsicsSwizzle_SwapX16     0x401f
#define AmdExtD3DShaderIntrinsicsSwizzle_ReverseX2   0x041f
#define AmdExtD3DShaderIntrinsicsSwizzle_ReverseX4   0x0c1f
#define AmdExtD3DShaderIntrinsicsSwizzle_ReverseX8   0x1c1f
#define AmdExtD3DShaderIntrinsicsSwizzle_ReverseX16  0x3c1f
#define AmdExtD3DShaderIntrinsicsSwizzle_ReverseX32  0x7c1f
#define AmdExtD3DShaderIntrinsicsSwizzle_BCastX2     0x003e
#define AmdExtD3DShaderIntrinsicsSwizzle_BCastX4     0x003c
#define AmdExtD3DShaderIntrinsicsSwizzle_BCastX8     0x0038
#define AmdExtD3DShaderIntrinsicsSwizzle_BCastX16    0x0030
#define AmdExtD3DShaderIntrinsicsSwizzle_BCastX32    0x0020


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsBarycentric defines for barycentric interpolation mode.  To be used with
*   AmdExtD3DShaderIntrinsicsOpcode_IjBarycentricCoords to specify the interpolation mode.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsBarycentric_LinearCenter    0x1
#define AmdExtD3DShaderIntrinsicsBarycentric_LinearCentroid  0x2
#define AmdExtD3DShaderIntrinsicsBarycentric_LinearSample    0x3
#define AmdExtD3DShaderIntrinsicsBarycentric_PerspCenter     0x4
#define AmdExtD3DShaderIntrinsicsBarycentric_PerspCentroid   0x5
#define AmdExtD3DShaderIntrinsicsBarycentric_PerspSample     0x6
#define AmdExtD3DShaderIntrinsicsBarycentric_PerspPullModel  0x7

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsBarycentric defines for specifying vertex and parameter indices.  To be used as inputs to
*   the AmdExtD3DShaderIntrinsicsOpcode_VertexParameter function
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsBarycentric_Vertex0        0x0
#define AmdExtD3DShaderIntrinsicsBarycentric_Vertex1        0x1
#define AmdExtD3DShaderIntrinsicsBarycentric_Vertex2        0x2

#define AmdExtD3DShaderIntrinsicsBarycentric_Param0         0x00
#define AmdExtD3DShaderIntrinsicsBarycentric_Param1         0x01
#define AmdExtD3DShaderIntrinsicsBarycentric_Param2         0x02
#define AmdExtD3DShaderIntrinsicsBarycentric_Param3         0x03
#define AmdExtD3DShaderIntrinsicsBarycentric_Param4         0x04
#define AmdExtD3DShaderIntrinsicsBarycentric_Param5         0x05
#define AmdExtD3DShaderIntrinsicsBarycentric_Param6         0x06
#define AmdExtD3DShaderIntrinsicsBarycentric_Param7         0x07
#define AmdExtD3DShaderIntrinsicsBarycentric_Param8         0x08
#define AmdExtD3DShaderIntrinsicsBarycentric_Param9         0x09
#define AmdExtD3DShaderIntrinsicsBarycentric_Param10        0x0a
#define AmdExtD3DShaderIntrinsicsBarycentric_Param11        0x0b
#define AmdExtD3DShaderIntrinsicsBarycentric_Param12        0x0c
#define AmdExtD3DShaderIntrinsicsBarycentric_Param13        0x0d
#define AmdExtD3DShaderIntrinsicsBarycentric_Param14        0x0e
#define AmdExtD3DShaderIntrinsicsBarycentric_Param15        0x0f
#define AmdExtD3DShaderIntrinsicsBarycentric_Param16        0x10
#define AmdExtD3DShaderIntrinsicsBarycentric_Param17        0x11
#define AmdExtD3DShaderIntrinsicsBarycentric_Param18        0x12
#define AmdExtD3DShaderIntrinsicsBarycentric_Param19        0x13
#define AmdExtD3DShaderIntrinsicsBarycentric_Param20        0x14
#define AmdExtD3DShaderIntrinsicsBarycentric_Param21        0x15
#define AmdExtD3DShaderIntrinsicsBarycentric_Param22        0x16
#define AmdExtD3DShaderIntrinsicsBarycentric_Param23        0x17
#define AmdExtD3DShaderIntrinsicsBarycentric_Param24        0x18
#define AmdExtD3DShaderIntrinsicsBarycentric_Param25        0x19
#define AmdExtD3DShaderIntrinsicsBarycentric_Param26        0x1a
#define AmdExtD3DShaderIntrinsicsBarycentric_Param27        0x1b
#define AmdExtD3DShaderIntrinsicsBarycentric_Param28        0x1c
#define AmdExtD3DShaderIntrinsicsBarycentric_Param29        0x1d
#define AmdExtD3DShaderIntrinsicsBarycentric_Param30        0x1e
#define AmdExtD3DShaderIntrinsicsBarycentric_Param31        0x1f

#define AmdExtD3DShaderIntrinsicsBarycentric_ComponentX     0x0
#define AmdExtD3DShaderIntrinsicsBarycentric_ComponentY     0x1
#define AmdExtD3DShaderIntrinsicsBarycentric_ComponentZ     0x2
#define AmdExtD3DShaderIntrinsicsBarycentric_ComponentW     0x3

#define AmdExtD3DShaderIntrinsicsBarycentric_ParamShift     0
#define AmdExtD3DShaderIntrinsicsBarycentric_ParamMask      0x1f
#define AmdExtD3DShaderIntrinsicsBarycentric_VtxShift       0x5
#define AmdExtD3DShaderIntrinsicsBarycentric_VtxMask        0x3
#define AmdExtD3DShaderIntrinsicsBarycentric_ComponentShift 0x7
#define AmdExtD3DShaderIntrinsicsBarycentric_ComponentMask  0x3

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsAtomic defines for supported operations. Can be used as the parameter for the
*   AmdExtD3DShaderIntrinsicsOpcode_AtomicU64 intrinsic.
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsAtomicOp_MinU64     0x01
#define AmdExtD3DShaderIntrinsicsAtomicOp_MaxU64     0x02
#define AmdExtD3DShaderIntrinsicsAtomicOp_AndU64     0x03
#define AmdExtD3DShaderIntrinsicsAtomicOp_OrU64      0x04
#define AmdExtD3DShaderIntrinsicsAtomicOp_XorU64     0x05
#define AmdExtD3DShaderIntrinsicsAtomicOp_AddU64     0x06
#define AmdExtD3DShaderIntrinsicsAtomicOp_XchgU64    0x07
#define AmdExtD3DShaderIntrinsicsAtomicOp_CmpXchgU64 0x08

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsicsFloatConversion defines for supported rounding modes from float to float16 conversions.
*   To be used as an input AmdExtD3DShaderIntrinsicsOpcode_FloatConversion instruction
***********************************************************************************************************************
*/
#define AmdExtD3DShaderIntrinsicsFloatConversionOp_FToF16Near    0x01
#define AmdExtD3DShaderIntrinsicsFloatConversionOp_FToF16NegInf  0x02
#define AmdExtD3DShaderIntrinsicsFloatConversionOp_FToF16PlusInf 0x03


/**
***********************************************************************************************************************
*   MakeAmdShaderIntrinsicsInstruction
*
*   Creates instruction from supplied opcode and immediate data.
*   NOTE: This is an internal function and should not be called by the source HLSL shader directly.
*
***********************************************************************************************************************
*/
uint MakeAmdShaderIntrinsicsInstruction(uint opcode, uint opcodePhase, uint immediateData)
{
    return ((AmdExtD3DShaderIntrinsics_MagicCode << AmdExtD3DShaderIntrinsics_MagicCodeShift) |
            (immediateData << AmdExtD3DShaderIntrinsics_DataShift) |
            (opcodePhase << AmdExtD3DShaderIntrinsics_OpcodePhaseShift) |
            (opcode << AmdExtD3DShaderIntrinsics_OpcodeShift));
}

)EOSHADER", R"EOSHADER(


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ReadfirstlaneF
*
*   Returns the value of float src for the first active lane of the wavefront.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Readfirstlane) returned S_OK.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_ReadfirstlaneF(float src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Readfirstlane, 0, 0);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);
    return asfloat(retVal);
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ReadfirstlaneU
*
*   Returns the value of unsigned integer src for the first active lane of the wavefront.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Readfirstlane) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_ReadfirstlaneU(uint src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Readfirstlane, 0, 0);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);
    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Readlane
*
*   Returns the value of float src for the lane within the wavefront specified by laneId.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Readlane) returned S_OK.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_ReadlaneF(float src, uint laneId)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Readlane, 0, laneId);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);
    return asfloat(retVal);
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ReadlaneU
*
*   Returns the value of unsigned integer src for the lane within the wavefront specified by laneId.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Readlane) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_ReadlaneU(uint src, uint laneId)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Readlane, 0, laneId);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);
    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_LaneId
*
*   Returns the current lane id for the thread within the wavefront.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_LaneId) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_LaneId()
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_LaneId, 0, 0);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);
    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_GetWaveSize
*
*   Returns the wave size for the current shader, including active, inactive and helper lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_GetWaveSize) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_GetWaveSize()
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_GetWaveSize, 0, 0);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);
    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Swizzle
*
*   Generic instruction to shuffle the float src value among different lanes as specified by the operation.
*   Note that the operation parameter must be an immediately specified value not a value from a variable.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Swizzle) returned S_OK.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_SwizzleF(float src, uint operation)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Swizzle, 0, operation);

    uint retVal;
    //InterlockedCompareExchange(AmdExtD3DShaderIntrinsicsUAV[instruction], asuint(src), 0, retVal);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);
    return asfloat(retVal);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_SwizzleU
*
*   Generic instruction to shuffle the unsigned integer src value among different lanes as specified by the operation.
*   Note that the operation parameter must be an immediately specified value not a value from a variable.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Swizzle) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_SwizzleU(uint src, uint operation)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Swizzle, 0, operation);

    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);
    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Ballot
*
*   Given an input predicate returns a bit mask indicating for which lanes the predicate is true.
*   Inactive or non-existent lanes will always return 0.  The number of existent lanes is the wavefront size.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Ballot) returned S_OK.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_Ballot(bool predicate)
{
    uint instruction;

    uint retVal1;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Ballot,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_0, 0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, predicate, 0, retVal1);

    uint retVal2;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Ballot,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_1, 0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, predicate, 0, retVal2);

    return uint2(retVal1, retVal2);
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_BallotAny
*
*   Convenience routine that uses Ballot and returns true if for any of the active lanes the predicate is true.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Ballot) returned S_OK.
*
***********************************************************************************************************************
*/
bool AmdExtD3DShaderIntrinsics_BallotAny(bool predicate)
{
    uint2 retVal = AmdExtD3DShaderIntrinsics_Ballot(predicate);

    return ((retVal.x | retVal.y) != 0 ? true : false);
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_BallotAll
*
*   Convenience routine that uses Ballot and returns true if for all of the active lanes the predicate is true.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Ballot) returned S_OK.
*
***********************************************************************************************************************
*/
bool AmdExtD3DShaderIntrinsics_BallotAll(bool predicate)
{
    uint2 ballot = AmdExtD3DShaderIntrinsics_Ballot(predicate);

    uint2 execMask = AmdExtD3DShaderIntrinsics_Ballot(true);

    return ((ballot.x == execMask.x) && (ballot.y == execMask.y));
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_MBCnt
*
*   Returns the masked bit count of the source register for this thread within all the active threads within a
*   wavefront.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_MBCnt) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_MBCnt(uint2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_MBCnt, 0, 0);

    uint retVal;

    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, src.y, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Min3F
*
*   Returns the minimum value of the three floating point source arguments.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Compare3) returned S_OK.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_Min3F(float src0, float src1, float src2)
{
    uint minimum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Min3F,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, asuint(src0), asuint(src1), minimum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Min3F,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, asuint(src2), minimum, minimum);

    return asfloat(minimum);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Min3U
*
*   Returns the minimum value of the three unsigned integer source arguments.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Compare3) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_Min3U(uint src0, uint src1, uint src2)
{
    uint minimum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Min3U,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, src0, src1, minimum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Min3U,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, src2, minimum, minimum);

    return minimum;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Med3F
*
*   Returns the median value of the three floating point source arguments.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Compare3) returned S_OK.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_Med3F(float src0, float src1, float src2)
{
    uint median;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Med3F,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, asuint(src0), asuint(src1), median);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Med3F,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, asuint(src2), median, median);

    return asfloat(median);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Med3U
*
*   Returns the median value of the three unsigned integer source arguments.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Compare3) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_Med3U(uint src0, uint src1, uint src2)
{
    uint median;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Med3U,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, src0, src1, median);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Med3U,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, src2, median, median);

    return median;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Max3F
*
*   Returns the maximum value of the three floating point source arguments.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Compare3) returned S_OK.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_Max3F(float src0, float src1, float src2)
{
    uint maximum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Max3F,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, asuint(src0), asuint(src1), maximum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Max3F,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, asuint(src2), maximum, maximum);

    return asfloat(maximum);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_Max3U
*
*   Returns the maximum value of the three unsigned integer source arguments.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_Compare3) returned S_OK.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_Max3U(uint src0, uint src1, uint src2)
{
    uint maximum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Max3U,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, src0, src1, maximum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_Max3U,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, src2, maximum, maximum);

    return maximum;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_IjBarycentricCoords
*
*   Returns the (i, j) barycentric coordinate pair for this shader invocation with the specified interpolation mode at
*   the specified pixel location.  Should not be used for "pull-model" interpolation, PullModelBarycentricCoords should
*   be used instead
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_BaryCoord) returned S_OK.
*
*   Can only be used in pixel shader stages.
*
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_IjBarycentricCoords(uint interpMode)
{
    uint2 retVal;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           interpMode);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, 0, 0, retVal.x);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           interpMode);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, retVal.x, 0, retVal.y);

    return float2(asfloat(retVal.x), asfloat(retVal.y));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_PullModelBarycentricCoords
*
*   Returns the (1/W,1/I,1/J) coordinates at the pixel center which can be used for custom interpolation at any
*   location in the pixel.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_BaryCoord) returned S_OK.
*
*   Can only be used in pixel shader stages.
*
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_PullModelBarycentricCoords()
{
    uint3 retVal;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                           AmdExtD3DShaderIntrinsicsBarycentric_PerspPullModel);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, 0, 0, retVal.x);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                           AmdExtD3DShaderIntrinsicsBarycentric_PerspPullModel);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, retVal.x, 0, retVal.y);

    uint instruction3 = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdExtD3DShaderIntrinsicsOpcodePhase_2,
                                                           AmdExtD3DShaderIntrinsicsBarycentric_PerspPullModel);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction3, retVal.y, 0, retVal.z);

    return float3(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_VertexParameter
*
*   Returns the triangle's parameter information at the specified triangle vertex.
*   The vertex and parameter indices must specified as immediate values.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_VtxParam) returned S_OK.
*
*   Only available in pixel shader stages.
*
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_VertexParameter(uint vertexIdx, uint parameterIdx)
{
    uint4 retVal;
    uint4 instruction;

    instruction.x = MakeAmdShaderIntrinsicsInstruction(
             AmdExtD3DShaderIntrinsicsOpcode_VtxParam,
             AmdExtD3DShaderIntrinsicsOpcodePhase_0,
           ((vertexIdx << AmdExtD3DShaderIntrinsicsBarycentric_VtxShift) |
            (parameterIdx << AmdExtD3DShaderIntrinsicsBarycentric_ParamShift) |
            (AmdExtD3DShaderIntrinsicsBarycentric_ComponentX << AmdExtD3DShaderIntrinsicsBarycentric_ComponentShift)));
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.x, 0, 0, retVal.x);

    instruction.y = MakeAmdShaderIntrinsicsInstruction(
             AmdExtD3DShaderIntrinsicsOpcode_VtxParam,
             AmdExtD3DShaderIntrinsicsOpcodePhase_0,
           ((vertexIdx << AmdExtD3DShaderIntrinsicsBarycentric_VtxShift) |
            (parameterIdx << AmdExtD3DShaderIntrinsicsBarycentric_ParamShift) |
            (AmdExtD3DShaderIntrinsicsBarycentric_ComponentY << AmdExtD3DShaderIntrinsicsBarycentric_ComponentShift)));
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.y, 0, 0, retVal.y);

    instruction.z = MakeAmdShaderIntrinsicsInstruction(
             AmdExtD3DShaderIntrinsicsOpcode_VtxParam,
             AmdExtD3DShaderIntrinsicsOpcodePhase_0,
           ((vertexIdx << AmdExtD3DShaderIntrinsicsBarycentric_VtxShift) |
            (parameterIdx << AmdExtD3DShaderIntrinsicsBarycentric_ParamShift) |
            (AmdExtD3DShaderIntrinsicsBarycentric_ComponentZ << AmdExtD3DShaderIntrinsicsBarycentric_ComponentShift)));
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.z, 0, 0, retVal.z);

    instruction.w = MakeAmdShaderIntrinsicsInstruction(
             AmdExtD3DShaderIntrinsicsOpcode_VtxParam,
             AmdExtD3DShaderIntrinsicsOpcodePhase_0,
           ((vertexIdx << AmdExtD3DShaderIntrinsicsBarycentric_VtxShift) |
            (parameterIdx << AmdExtD3DShaderIntrinsicsBarycentric_ParamShift) |
            (AmdExtD3DShaderIntrinsicsBarycentric_ComponentW << AmdExtD3DShaderIntrinsicsBarycentric_ComponentShift)));
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.w, 0, 0, retVal.w);

    return float4(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z), asfloat(retVal.w));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_VertexParameterComponent
*
*   Returns the triangle's parameter information at the specified triangle vertex and component.
*   The vertex, parameter and component indices must be specified as immediate values.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_VtxParam) returned S_OK.
*
*   Only available in pixel shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_VertexParameterComponent(uint vertexIdx, uint parameterIdx, uint componentIdx)
{
    uint retVal;
    uint instruction =
        MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_VtxParam,
                                           AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                          ((vertexIdx << AmdExtD3DShaderIntrinsicsBarycentric_VtxShift) |
                                           (parameterIdx << AmdExtD3DShaderIntrinsicsBarycentric_ParamShift) |
                                           (componentIdx << AmdExtD3DShaderIntrinsicsBarycentric_ComponentShift)));
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return asfloat(retVal);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_WaveReduce) returned S_OK.
*
*   Performs reduction operation on wavefront (thread group) data.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : float
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, float src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));
    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);

    return asfloat(retVal);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : float2
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, float2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    uint2 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);

    return float2(asfloat(retVal.x), asfloat(retVal.y));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : float3
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, float3 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    uint3 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);

    return float3(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z));
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : float4
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, float4 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    uint4 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.w), 0, retVal.w);

    return float4(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z), asfloat(retVal.w));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : int
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, int src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    int retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : int2
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, int2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    int2 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.y, 0, retVal.y);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : int3
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, int3 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    int3 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.y, 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.z, 0, retVal.z);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveReduce : int4
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveReduce(uint waveOp, int4 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift));

    int4 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.y, 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.z, 0, retVal.z);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.w, 0, retVal.w);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_WaveScan) returned S_OK.
*
*   Performs scan operation on wavefront (thread group) data.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : float
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, float src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags  << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);
    uint retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);

    return asfloat(retVal);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : float2
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, float2 src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    uint2 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);

    return float2(asfloat(retVal.x), asfloat(retVal.y));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : float3
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, float3 src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    uint3 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);

    return float3(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : float4
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, float4 src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    uint4 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.w), 0, retVal.w);

    return float4(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z), asfloat(retVal.w));
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : int
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, int src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    int retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : int2
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, int2 src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    int2 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.y, 0, retVal.y);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : int3
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, int3 src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    int3 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.y, 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.z, 0, retVal.z);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveScan : int4
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveScan(uint waveOp, uint flags, int4 src)
{
    const uint waveScanOp = (waveOp << AmdExtD3DShaderIntrinsicsWaveOp_OpcodeShift) |
                            (flags << AmdExtD3DShaderIntrinsicsWaveOp_FlagShift);

    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_WaveScan,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          waveScanOp);

    int4 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.y, 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.z, 0, retVal.z);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.w, 0, retVal.w);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_LoadDwordAtAddr
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_LoadDwAtAddr) returned S_OK.
*
*   Loads a DWORD from GPU memory from a given 64-bit GPU VA and 32-bit offset.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_LoadDwordAtAddr
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(uint gpuVaLoBits, uint gpuVaHiBits, uint offset)
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_LoadDwAtAddr,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                     0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, gpuVaLoBits, gpuVaHiBits, retVal);

    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_LoadDwAtAddr,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_1,
                                                     0);

    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, offset, 0, retVal);

    return retVal;
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_LoadDwordAtAddrx2
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_LoadDwordAtAddrx2(uint gpuVaLoBits, uint gpuVaHiBits, uint offset)
{
    uint2 retVal;

    retVal.x = AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(gpuVaLoBits, gpuVaHiBits, offset);
    retVal.y = AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(gpuVaLoBits, gpuVaHiBits, offset + 0x4);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_LoadDwordAtAddrx4
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_LoadDwordAtAddrx4(uint gpuVaLoBits, uint gpuVaHiBits, uint offset)
{
    uint4 retVal;

    retVal.x = AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(gpuVaLoBits, gpuVaHiBits, offset);
    retVal.y = AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(gpuVaLoBits, gpuVaHiBits, offset + 0x4);
    retVal.z = AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(gpuVaLoBits, gpuVaHiBits, offset + 0x8);
    retVal.w = AmdExtD3DShaderIntrinsics_LoadDwordAtAddr(gpuVaLoBits, gpuVaHiBits, offset + 0xC);

    return retVal;
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_GetDrawIndex
*
*   The following function is available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_GetDrawIndex) returned S_OK.
*
*   Returns the 0-based draw index in an indirect draw. Always returns 0 for direct draws.
*
*   Available in vertex shader stage only.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_GetDrawIndex()
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_DrawIndex,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                     0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_GetBaseInstance
*
*   The following function is available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_BaseInstance) returned S_OK.
*
*   Returns the StartInstanceLocation parameter passed to direct or indirect drawing commands.
*
*   Available in vertex shader stage only.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_GetBaseInstance()
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaseInstance,
        AmdExtD3DShaderIntrinsicsOpcodePhase_0,
        0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_GetBaseVertex
*
*   The following function is available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_BaseVertex) returned S_OK.
*
*   For non-indexed draw commands, returns the StartVertexLocation parameter. For indexed draw commands, returns the
*   BaseVertexLocation parameter.
*
*   Available in vertex shader stage only.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_GetBaseVertex()
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_BaseVertex,
        AmdExtD3DShaderIntrinsicsOpcodePhase_0,
        0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}



/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ReadlaneAt : uint
*
*   The following function is available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_ReadlaneAt) returned S_OK.
*
*   Returns the value of the source for the given lane index within the specified wave.  The lane index
*   can be non-uniform across the wave.
*
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_ReadlaneAt(uint src, uint laneId)
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_ReadlaneAt,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                     0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, laneId, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ReadlaneAt : int
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_ReadlaneAt(int src, uint laneId)
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_ReadlaneAt,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                     0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), laneId, retVal);

    return asint(retVal);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ReadlaneAt : float
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_ReadlaneAt(float src, uint laneId)
{
    uint retVal;

    uint instruction;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_ReadlaneAt,
                                                     AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                     0);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), laneId, retVal);

    return asfloat(retVal);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ConvertF32toF16
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsSupport_FloatConversion) returned
*   S_OK.
*
*   Converts 32bit floating point numbers into 16bit floating point number using a specified rounding mode
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ConvertF32toF16 - helper to convert f32 to f16 number
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_ConvertF32toF16(in uint convOp, in float3 val)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdExtD3DShaderIntrinsicsOpcode_FloatConversion,
                                                          AmdExtD3DShaderIntrinsicsOpcodePhase_0,
                                                          convOp);

    uint3 retVal;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(val.x), 0, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(val.y), 0, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(val.z), 0, retVal.z);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ConvertF32toF16Near - convert f32 to f16 number using nearest rounding mode
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_ConvertF32toF16Near(in float3 inVec)
{
    return AmdExtD3DShaderIntrinsics_ConvertF32toF16(AmdExtD3DShaderIntrinsicsFloatConversionOp_FToF16Near, inVec);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ConvertF32toF16Near - convert f32 to f16 number using -inf rounding mode
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_ConvertF32toF16NegInf(in float3 inVec)
{
    return AmdExtD3DShaderIntrinsics_ConvertF32toF16(AmdExtD3DShaderIntrinsicsFloatConversionOp_FToF16NegInf, inVec);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_ConvertF32toF16Near - convert f32 to f16 number using +inf rounding mode
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_ConvertF32toF16PosInf(in float3 inVec)
{
    return AmdExtD3DShaderIntrinsics_ConvertF32toF16(AmdExtD3DShaderIntrinsicsFloatConversionOp_FToF16PlusInf, inVec);
}



/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_MakeAtomicInstructions
*
*   Creates uint4 with x/y/z/w components containing phase 0/1/2/3 for atomic instructions.
*   NOTE: This is an internal function and should not be called by the source HLSL shader directly.
*
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(uint op)
{
    uint4 instructions;
    instructions.x = MakeAmdShaderIntrinsicsInstruction(
        AmdExtD3DShaderIntrinsicsOpcode_AtomicU64, AmdExtD3DShaderIntrinsicsOpcodePhase_0, op);
    instructions.y = MakeAmdShaderIntrinsicsInstruction(
        AmdExtD3DShaderIntrinsicsOpcode_AtomicU64, AmdExtD3DShaderIntrinsicsOpcodePhase_1, op);
    instructions.z = MakeAmdShaderIntrinsicsInstruction(
        AmdExtD3DShaderIntrinsicsOpcode_AtomicU64, AmdExtD3DShaderIntrinsicsOpcodePhase_2, op);
    instructions.w = MakeAmdShaderIntrinsicsInstruction(
        AmdExtD3DShaderIntrinsicsOpcode_AtomicU64, AmdExtD3DShaderIntrinsicsOpcodePhase_3, op);
    return instructions;
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicOp
*
*   Creates intrinstic instructions for the specified atomic op.
*   NOTE: These are internal functions and should not be called by the source HLSL shader directly.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicOp(RWByteAddressBuffer uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav.Store(retVal.x, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(RWTexture1D<uint2> uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav[retVal.x] = retVal.y;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(RWTexture2D<uint2> uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav[uint2(retVal.x, retVal.x)] = retVal.y;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(RWTexture3D<uint2> uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav[uint3(retVal.x, retVal.x, retVal.x)] = retVal.y;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(
    RWByteAddressBuffer uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav.Store(retVal.x, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(
    RWTexture1D<uint2> uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav[retVal.x] = retVal.y;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(
    RWTexture2D<uint2> uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav[uint2(retVal.x, retVal.x)] = retVal.y;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOp(
    RWTexture3D<uint2> uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdExtD3DShaderIntrinsics_MakeAtomicInstructions(op);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav[uint3(retVal.x, retVal.x, retVal.x)] = retVal.y;
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdExtD3DShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicMinU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic minimum of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicMinU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MinU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicMinU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MinU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicMinU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MinU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicMinU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MinU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicMaxU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic maximum of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicMaxU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MaxU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicMaxU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MaxU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicMaxU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MaxU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicMaxU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_MaxU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicAndU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic AND of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicAndU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AndU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicAndU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AndU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicAndU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AndU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicAndU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AndU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicOrU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic OR of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicOrU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_OrU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOrU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_OrU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOrU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_OrU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicOrU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_OrU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicXorU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic XOR of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicXorU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XorU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicXorU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XorU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicXorU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XorU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicXorU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XorU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicAddU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic add of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicAddU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AddU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicAddU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AddU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicAddU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AddU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicAddU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_AddU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicXchgU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic exchange of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicXchgU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicXchgU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicXchgU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicXchgU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_XchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_AtomicCmpXchgU64
*
*   The following functions are available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic compare of comparison value with UAV at address, stores value if values match,
*   returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_AtomicCmpXchgU64(
    RWByteAddressBuffer uav, uint address, uint2 compare_value, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), compare_value, value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicCmpXchgU64(
    RWTexture1D<uint2> uav, uint address, uint2 compare_value, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), compare_value, value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicCmpXchgU64(
    RWTexture2D<uint2> uav, uint2 address, uint2 compare_value, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), compare_value, value, op);
}

uint2 AmdExtD3DShaderIntrinsics_AtomicCmpXchgU64(
    RWTexture3D<uint2> uav, uint3 address, uint2 compare_value, uint2 value)
{
    const uint op = AmdExtD3DShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdExtD3DShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), compare_value, value, op);
}


/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum
*
*   Performs reduction operation across a wave and returns the result of the reduction (sum of all threads in a wave)
*   to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WaveActiveSum(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WaveActiveSum(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WaveActiveSum(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WaveActiveSum(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveSum(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveSum(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveSum(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveSum(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveSum(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveSum(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveSum(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveSum<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveSum(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct
*
*   Performs reduction operation across a wave and returns the result of the reduction (product of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WaveActiveProduct(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WaveActiveProduct(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WaveActiveProduct(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WaveActiveProduct(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveProduct(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveProduct(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulI, src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveProduct(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveProduct(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveProduct(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveProduct(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveProduct(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveProduct<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveProduct(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin
*
*   Performs reduction operation across a wave and returns the result of the reduction (minimum of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WaveActiveMin(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WaveActiveMin(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WaveActiveMin(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WaveActiveMin(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveMin(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveMin(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveMin(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveMin(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveMin(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveMin(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveMin(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMin<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveMin(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax
*
*   Performs reduction operation across a wave and returns the result of the reduction (maximum of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WaveActiveMax(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WaveActiveMax(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WaveActiveMax(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WaveActiveMax(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveMax(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveMax(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveMax(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveMax(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveMax(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveMax(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveMax(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveMax<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveMax(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd
*
*   Performs reduction operation across a wave and returns the result of the reduction (Bitwise AND of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitAnd<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveBitAnd(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr
*
*   Performs reduction operation across a wave and returns the result of the reduction (Bitwise OR of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveBitOr(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveBitOr(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveBitOr(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveBitOr(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveBitOr(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveBitOr(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveBitOr(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitOr<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveBitOr(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor
*
*   Performs reduction operation across a wave and returns the result of the reduction (Bitwise XOR of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WaveActiveBitXor(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WaveActiveBitXor(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WaveActiveBitXor(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WaveActiveBitXor(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WaveActiveBitXor(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WaveActiveBitXor(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WaveActiveBitXor(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WaveActiveBitXor<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WaveActiveBitXor(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveReduce(AmdExtD3DShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum
*
*   Performs a prefix (exclusive) scan operation across a wave and returns the resulting sum to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePrefixSum(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePrefixSum(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePrefixSum(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePrefixSum(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePrefixSum(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePrefixSum(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePrefixSum(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePrefixSum(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePrefixSum(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePrefixSum(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePrefixSum(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixSum<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePrefixSum(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct
*
*   Performs a prefix scan operation across a wave and returns the resulting product to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePrefixProduct(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePrefixProduct(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePrefixProduct(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePrefixProduct(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePrefixProduct(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePrefixProduct(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePrefixProduct(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePrefixProduct(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePrefixProduct(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePrefixProduct(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePrefixProduct(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixProduct<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePrefixProduct(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin
*
*   Performs a prefix scan operation across a wave and returns the resulting minimum value to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePrefixMin(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePrefixMin(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePrefixMin(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePrefixMin(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePrefixMin(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePrefixMin(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePrefixMin(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePrefixMin(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePrefixMin(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePrefixMin(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePrefixMin(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMin<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePrefixMin(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax
*
*   Performs a prefix scan operation across a wave and returns the resulting maximum value to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePrefixMax(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePrefixMax(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePrefixMax(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePrefixMax(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePrefixMax(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePrefixMax(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePrefixMax(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePrefixMax(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePrefixMax(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePrefixMax(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePrefixMax(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePrefixMax<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePrefixMax(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Exclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum
*
*   Performs a Postfix (Inclusive) scan operation across a wave and returns the resulting sum to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePostfixSum(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePostfixSum(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePostfixSum(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePostfixSum(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePostfixSum(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePostfixSum(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePostfixSum(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePostfixSum(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePostfixSum(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePostfixSum(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePostfixSum(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixSum<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePostfixSum(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_AddU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct
*
*   Performs a Postfix scan operation across a wave and returns the resulting product to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePostfixProduct(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePostfixProduct(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePostfixProduct(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePostfixProduct(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePostfixProduct(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePostfixProduct(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePostfixProduct(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePostfixProduct(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePostfixProduct(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePostfixProduct(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePostfixProduct(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixProduct<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePostfixProduct(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MulU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin
*
*   Performs a Postfix scan operation across a wave and returns the resulting minimum value to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePostfixMin(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePostfixMin(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePostfixMin(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePostfixMin(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePostfixMin(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePostfixMin(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePostfixMin(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePostfixMin(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePostfixMin(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePostfixMin(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePostfixMin(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMin<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePostfixMin(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MinU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax
*
*   Performs a Postfix scan operation across a wave and returns the resulting maximum value to all participating lanes.
*
*   Available if CheckSupport(AmdExtD3DShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdExtD3DShaderIntrinsics_WavePostfixMax(float src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<float2>
***********************************************************************************************************************
*/
float2 AmdExtD3DShaderIntrinsics_WavePostfixMax(float2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<float3>
***********************************************************************************************************************
*/
float3 AmdExtD3DShaderIntrinsics_WavePostfixMax(float3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<float4>
***********************************************************************************************************************
*/
float4 AmdExtD3DShaderIntrinsics_WavePostfixMax(float4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxF,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<int>
***********************************************************************************************************************
*/
int AmdExtD3DShaderIntrinsics_WavePostfixMax(int src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<int2>
***********************************************************************************************************************
*/
int2 AmdExtD3DShaderIntrinsics_WavePostfixMax(int2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<int3>
***********************************************************************************************************************
*/
int3 AmdExtD3DShaderIntrinsics_WavePostfixMax(int3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<int4>
***********************************************************************************************************************
*/
int4 AmdExtD3DShaderIntrinsics_WavePostfixMax(int4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxI,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<uint>
***********************************************************************************************************************
*/
uint AmdExtD3DShaderIntrinsics_WavePostfixMax(uint src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<uint2>
***********************************************************************************************************************
*/
uint2 AmdExtD3DShaderIntrinsics_WavePostfixMax(uint2 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<uint3>
***********************************************************************************************************************
*/
uint3 AmdExtD3DShaderIntrinsics_WavePostfixMax(uint3 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}

/**
***********************************************************************************************************************
*   AmdExtD3DShaderIntrinsics_WavePostfixMax<uint4>
***********************************************************************************************************************
*/
uint4 AmdExtD3DShaderIntrinsics_WavePostfixMax(uint4 src)
{
    return AmdExtD3DShaderIntrinsics_WaveScan(AmdExtD3DShaderIntrinsicsWaveOp_MaxU,
                                              AmdExtD3DShaderIntrinsicsWaveOp_Inclusive,
                                              src);
}


#if defined (AGS_RAY_HIT_TOKEN)

//=====================================================================================================================
struct AmdExtRtHitToken
{
    uint dword[2];
};

/**
***********************************************************************************************************************
* @brief
*    AmdExtD3DShaderIntrinsicsRT structure when included in a Ray Tracing payload will indicate to the driver
*    that the dwords are already supplied in AmdExtRtHitTokenIn and only requires a call to intersect
*    ray, bypassing the traversal of the acceleration structure.
***********************************************************************************************************************
*/
struct AmdExtRtHitTokenIn : AmdExtRtHitToken { };

/**
***********************************************************************************************************************
* @brief
*    AmdExtD3DShaderIntrinsicsRT structure when included in a Ray Tracing payload will indicate to the driver
*    that the dwords must be patched into the payload after traversal.  The application can store this
*    data in a buffer which can then be used for hit group sorting so shading divergence can be avoided.
***********************************************************************************************************************
*/
struct AmdExtRtHitTokenOut : AmdExtRtHitToken { };

/**
***********************************************************************************************************************
* @brief
*    Group shared memory reserved for temprary storage of hit tokens. Not intended to touched by the app shader.
*    Application shader must only use the extension functions defined below to access the hit tokens
*
***********************************************************************************************************************
*/
groupshared AmdExtRtHitToken AmdHitToken;

/**
***********************************************************************************************************************
* @brief
*    Accessor function to obtain the hit tokens from the last call to TraceRays(). The data returned by this
*    function only guarantees valid values for the last call to TraceRays() prior to calling this function.
*
***********************************************************************************************************************
*/
uint2 AmdGetLastHitToken()
{
    return uint2(AmdHitToken.dword[0], AmdHitToken.dword[1]);
}

/**
***********************************************************************************************************************
* @brief
*    This function initialises hit tokens for subsequent TraceRays() call. Note, any TraceRay() that intends to use
*    these hit tokens must include this function call in the same basic block. Applications can use a convenience macro
*    defined below to enforce that.
*
***********************************************************************************************************************
*/
void AmdSetHitToken(uint2 token)
{
    AmdHitToken.dword[0] = token.x;
    AmdHitToken.dword[1] = token.y;
}

/**
***********************************************************************************************************************
* @brief
*    Convenience macro for calling TraceRays that uses the hit token
*
***********************************************************************************************************************
*/
#define AmdTraceRay(accelStruct,                    \
                    rayFlags,                       \
                    instanceInclusionMask,          \
                    rayContributionToHitGroupIndex, \
                    geometryMultiplier,             \
                    missShaderIndex,                \
                    ray,                            \
                    payload,                        \
                    token)                          \
AmdSetHitToken(token);                              \
TraceRay(accelStruct,                               \
         rayFlags,                                  \
         instanceInclusionMask,                     \
         rayContributionToHitGroupIndex,            \
         geometryMultiplier,                        \
         missShaderIndex,                           \
         ray,                                       \
         payload);                                  \

#endif // AGS_RAY_HIT_TOKEN

#endif // _AMDEXTD3DSHADERINTRINICS_HLSL
)EOSHADER"};

inline std::string ags_shader_intrinsics_dx12_hlsl()
{
  std::string ret;
  for(size_t i = 0; i < sizeof(ags_shader_intrinsics_dx12_hlsl_array) /
                            sizeof(ags_shader_intrinsics_dx12_hlsl_array[0]);
      i++)
    ret += ags_shader_intrinsics_dx12_hlsl_array[i];
  return ret;
}
