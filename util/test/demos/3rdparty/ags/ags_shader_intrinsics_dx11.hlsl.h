static const char *ags_shader_intrinsics_dx11_hlsl_array[] = {R"EOSHADER(
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
*************************************************************************************************************
* @file  ags_shader_intrinsics_dx11.hlsl
*
* @brief
*    AMD D3D Shader Intrinsics API hlsl file.
*    This include file contains the shader intrinsics definitions (structures, enums, constant)
*    and HLSL shader intrinsics functions.
*
* @version 2.3
*
*************************************************************************************************************
*/

#ifndef _AMDDXEXTSHADERINTRINSICS_HLSL_
#define _AMDDXEXTSHADERINTRINSICS_HLSL_

/**
*************************************************************************************************************
*   Definitions to construct the intrinsic instruction composed of an opcode and optional immediate data.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsics_MagicCodeShift   28
#define AmdDxExtShaderIntrinsics_MagicCodeMask    0xf
#define AmdDxExtShaderIntrinsics_OpcodePhaseShift 24
#define AmdDxExtShaderIntrinsics_OpcodePhaseMask  0x3
#define AmdDxExtShaderIntrinsics_DataShift        8
#define AmdDxExtShaderIntrinsics_DataMask         0xffff
#define AmdDxExtShaderIntrinsics_OpcodeShift      0
#define AmdDxExtShaderIntrinsics_OpcodeMask       0xff

#define AmdDxExtShaderIntrinsics_MagicCode        0x5


/**
*************************************************************************************************************
*   Intrinsic opcodes.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsOpcode_Readfirstlane  0x01
#define AmdDxExtShaderIntrinsicsOpcode_Readlane       0x02
#define AmdDxExtShaderIntrinsicsOpcode_LaneId         0x03
#define AmdDxExtShaderIntrinsicsOpcode_Swizzle        0x04
#define AmdDxExtShaderIntrinsicsOpcode_Ballot         0x05
#define AmdDxExtShaderIntrinsicsOpcode_MBCnt          0x06
#define AmdDxExtShaderIntrinsicsOpcode_Min3U          0x08
#define AmdDxExtShaderIntrinsicsOpcode_Min3F          0x09
#define AmdDxExtShaderIntrinsicsOpcode_Med3U          0x0a
#define AmdDxExtShaderIntrinsicsOpcode_Med3F          0x0b
#define AmdDxExtShaderIntrinsicsOpcode_Max3U          0x0c
#define AmdDxExtShaderIntrinsicsOpcode_Max3F          0x0d
#define AmdDxExtShaderIntrinsicsOpcode_BaryCoord      0x0e
#define AmdDxExtShaderIntrinsicsOpcode_VtxParam       0x0f
#define AmdDxExtShaderIntrinsicsOpcode_ViewportIndex  0x10
#define AmdDxExtShaderIntrinsicsOpcode_RtArraySlice   0x11
#define AmdDxExtShaderIntrinsicsOpcode_WaveReduce     0x12
#define AmdDxExtShaderIntrinsicsOpcode_WaveScan       0x13
#define AmdDxExtShaderIntrinsicsOpcode_Reserved1      0x14
#define AmdDxExtShaderIntrinsicsOpcode_Reserved2      0x15
#define AmdDxExtShaderIntrinsicsOpcode_Reserved3      0x16
#define AmdDxExtShaderIntrinsicsOpcode_DrawIndex      0x17
#define AmdDxExtShaderIntrinsicsOpcode_AtomicU64      0x18
#define AmdDxExtShaderIntrinsicsOpcode_GetWaveSize    0x19
#define AmdDxExtShaderIntrinsicsOpcode_BaseInstance   0x1a
#define AmdDxExtShaderIntrinsicsOpcode_BaseVertex     0x1b


/**
*************************************************************************************************************
*   Intrinsic opcode phases.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsOpcodePhase_0    0x0
#define AmdDxExtShaderIntrinsicsOpcodePhase_1    0x1
#define AmdDxExtShaderIntrinsicsOpcodePhase_2    0x2
#define AmdDxExtShaderIntrinsicsOpcodePhase_3    0x3

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsSwizzle defines for common swizzles.  Can be used as the operation parameter for
*   the AmdDxExtShaderIntrinsics_Swizzle intrinsic.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsSwizzle_SwapX1      0x041f
#define AmdDxExtShaderIntrinsicsSwizzle_SwapX2      0x081f
#define AmdDxExtShaderIntrinsicsSwizzle_SwapX4      0x101f
#define AmdDxExtShaderIntrinsicsSwizzle_SwapX8      0x201f
#define AmdDxExtShaderIntrinsicsSwizzle_SwapX16     0x401f
#define AmdDxExtShaderIntrinsicsSwizzle_ReverseX2   0x041f
#define AmdDxExtShaderIntrinsicsSwizzle_ReverseX4   0x0c1f
#define AmdDxExtShaderIntrinsicsSwizzle_ReverseX8   0x1c1f
#define AmdDxExtShaderIntrinsicsSwizzle_ReverseX16  0x3c1f
#define AmdDxExtShaderIntrinsicsSwizzle_ReverseX32  0x7c1f
#define AmdDxExtShaderIntrinsicsSwizzle_BCastX2     0x003e
#define AmdDxExtShaderIntrinsicsSwizzle_BCastX4     0x003c
#define AmdDxExtShaderIntrinsicsSwizzle_BCastX8     0x0038
#define AmdDxExtShaderIntrinsicsSwizzle_BCastX16    0x0030
#define AmdDxExtShaderIntrinsicsSwizzle_BCastX32    0x0020


/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsBarycentric defines for barycentric interpolation mode.  To be used with
*   AmdDxExtShaderIntrinsicsOpcode_IjBarycentricCoords to specify the interpolation mode.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsBarycentric_LinearCenter    0x1
#define AmdDxExtShaderIntrinsicsBarycentric_LinearCentroid  0x2
#define AmdDxExtShaderIntrinsicsBarycentric_LinearSample    0x3
#define AmdDxExtShaderIntrinsicsBarycentric_PerspCenter     0x4
#define AmdDxExtShaderIntrinsicsBarycentric_PerspCentroid   0x5
#define AmdDxExtShaderIntrinsicsBarycentric_PerspSample     0x6
#define AmdDxExtShaderIntrinsicsBarycentric_PerspPullModel  0x7

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsBarycentric defines for specifying vertex and parameter indices.  To be used as
*   the inputs to the AmdDxExtShaderIntrinsicsOpcode_VertexParameter function
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsBarycentric_Vertex0     0x0
#define AmdDxExtShaderIntrinsicsBarycentric_Vertex1     0x1
#define AmdDxExtShaderIntrinsicsBarycentric_Vertex2     0x2

#define AmdDxExtShaderIntrinsicsBarycentric_Param0      0x00
#define AmdDxExtShaderIntrinsicsBarycentric_Param1      0x01
#define AmdDxExtShaderIntrinsicsBarycentric_Param2      0x02
#define AmdDxExtShaderIntrinsicsBarycentric_Param3      0x03
#define AmdDxExtShaderIntrinsicsBarycentric_Param4      0x04
#define AmdDxExtShaderIntrinsicsBarycentric_Param5      0x05
#define AmdDxExtShaderIntrinsicsBarycentric_Param6      0x06
#define AmdDxExtShaderIntrinsicsBarycentric_Param7      0x07
#define AmdDxExtShaderIntrinsicsBarycentric_Param8      0x08
#define AmdDxExtShaderIntrinsicsBarycentric_Param9      0x09
#define AmdDxExtShaderIntrinsicsBarycentric_Param10     0x0a
#define AmdDxExtShaderIntrinsicsBarycentric_Param11     0x0b
#define AmdDxExtShaderIntrinsicsBarycentric_Param12     0x0c
#define AmdDxExtShaderIntrinsicsBarycentric_Param13     0x0d
#define AmdDxExtShaderIntrinsicsBarycentric_Param14     0x0e
#define AmdDxExtShaderIntrinsicsBarycentric_Param15     0x0f
#define AmdDxExtShaderIntrinsicsBarycentric_Param16     0x10
#define AmdDxExtShaderIntrinsicsBarycentric_Param17     0x11
#define AmdDxExtShaderIntrinsicsBarycentric_Param18     0x12
#define AmdDxExtShaderIntrinsicsBarycentric_Param19     0x13
#define AmdDxExtShaderIntrinsicsBarycentric_Param20     0x14
#define AmdDxExtShaderIntrinsicsBarycentric_Param21     0x15
#define AmdDxExtShaderIntrinsicsBarycentric_Param22     0x16
#define AmdDxExtShaderIntrinsicsBarycentric_Param23     0x17
#define AmdDxExtShaderIntrinsicsBarycentric_Param24     0x18
#define AmdDxExtShaderIntrinsicsBarycentric_Param25     0x19
#define AmdDxExtShaderIntrinsicsBarycentric_Param26     0x1a
#define AmdDxExtShaderIntrinsicsBarycentric_Param27     0x1b
#define AmdDxExtShaderIntrinsicsBarycentric_Param28     0x1c
#define AmdDxExtShaderIntrinsicsBarycentric_Param29     0x1d
#define AmdDxExtShaderIntrinsicsBarycentric_Param30     0x1e
#define AmdDxExtShaderIntrinsicsBarycentric_Param31     0x1f

#define AmdDxExtShaderIntrinsicsBarycentric_ComponentX  0x0
#define AmdDxExtShaderIntrinsicsBarycentric_ComponentY  0x1
#define AmdDxExtShaderIntrinsicsBarycentric_ComponentZ  0x2
#define AmdDxExtShaderIntrinsicsBarycentric_ComponentW  0x3

#define AmdDxExtShaderIntrinsicsBarycentric_ParamShift     0
#define AmdDxExtShaderIntrinsicsBarycentric_ParamMask      0x1f
#define AmdDxExtShaderIntrinsicsBarycentric_VtxShift       0x5
#define AmdDxExtShaderIntrinsicsBarycentric_VtxMask        0x3
#define AmdDxExtShaderIntrinsicsBarycentric_ComponentShift 0x7
#define AmdDxExtShaderIntrinsicsBarycentric_ComponentMask  0x3

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsWaveOp defines for supported operations. Can be used as the parameter for the
*   AmdDxExtShaderIntrinsicsOpcode_WaveOp intrinsic.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsWaveOp_AddF        0x01
#define AmdDxExtShaderIntrinsicsWaveOp_AddI        0x02
#define AmdDxExtShaderIntrinsicsWaveOp_AddU        0x03
#define AmdDxExtShaderIntrinsicsWaveOp_MulF        0x04
#define AmdDxExtShaderIntrinsicsWaveOp_MulI        0x05
#define AmdDxExtShaderIntrinsicsWaveOp_MulU        0x06
#define AmdDxExtShaderIntrinsicsWaveOp_MinF        0x07
#define AmdDxExtShaderIntrinsicsWaveOp_MinI        0x08
#define AmdDxExtShaderIntrinsicsWaveOp_MinU        0x09
#define AmdDxExtShaderIntrinsicsWaveOp_MaxF        0x0a
#define AmdDxExtShaderIntrinsicsWaveOp_MaxI        0x0b
#define AmdDxExtShaderIntrinsicsWaveOp_MaxU        0x0c
#define AmdDxExtShaderIntrinsicsWaveOp_And         0x0d    // Reduction only
#define AmdDxExtShaderIntrinsicsWaveOp_Or          0x0e    // Reduction only
#define AmdDxExtShaderIntrinsicsWaveOp_Xor         0x0f    // Reduction only

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsWaveOp masks and shifts for opcode and flags
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift 0
#define AmdDxExtShaderIntrinsicsWaveOp_OpcodeMask  0xff
#define AmdDxExtShaderIntrinsicsWaveOp_FlagShift   8
#define AmdDxExtShaderIntrinsicsWaveOp_FlagMask    0xff

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsWaveOp flags for use with AmdDxExtShaderIntrinsicsOpcode_WaveScan.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsWaveOp_Inclusive   0x01
#define AmdDxExtShaderIntrinsicsWaveOp_Exclusive   0x02

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsicsAtomic defines for supported operations. Can be used as the parameter for the
*   AmdDxExtShaderIntrinsicsOpcode_AtomicU64 intrinsic.
*************************************************************************************************************
*/
#define AmdDxExtShaderIntrinsicsAtomicOp_MinU64     0x01
#define AmdDxExtShaderIntrinsicsAtomicOp_MaxU64     0x02
#define AmdDxExtShaderIntrinsicsAtomicOp_AndU64     0x03
#define AmdDxExtShaderIntrinsicsAtomicOp_OrU64      0x04
#define AmdDxExtShaderIntrinsicsAtomicOp_XorU64     0x05
#define AmdDxExtShaderIntrinsicsAtomicOp_AddU64     0x06
#define AmdDxExtShaderIntrinsicsAtomicOp_XchgU64    0x07
#define AmdDxExtShaderIntrinsicsAtomicOp_CmpXchgU64 0x08


/**
*************************************************************************************************************
*   Resource slots for intrinsics using imm_atomic_cmp_exch.
*************************************************************************************************************
*/
#ifndef AmdDxExtShaderIntrinsicsUAVSlot
#define AmdDxExtShaderIntrinsicsUAVSlot       u7
#endif

RWByteAddressBuffer AmdDxExtShaderIntrinsicsUAV : register(AmdDxExtShaderIntrinsicsUAVSlot);

/**
*************************************************************************************************************
*   Resource and sampler slots for intrinsics using sample_l.
*************************************************************************************************************
*/
#ifndef AmdDxExtShaderIntrinsicsResSlot
#define AmdDxExtShaderIntrinsicsResSlot       t127
#endif

#ifndef AmdDxExtShaderIntrinsicsSamplerSlot
#define AmdDxExtShaderIntrinsicsSamplerSlot   s15
#endif

SamplerState AmdDxExtShaderIntrinsicsSamplerState : register (AmdDxExtShaderIntrinsicsSamplerSlot);
Texture3D<float4> AmdDxExtShaderIntrinsicsResource : register (AmdDxExtShaderIntrinsicsResSlot);

/**
*************************************************************************************************************
*   MakeAmdShaderIntrinsicsInstruction
*
*   Creates instruction from supplied opcode and immediate data.
*   NOTE: This is an internal function and should not be called by the source HLSL shader directly.
*
*************************************************************************************************************
*/
uint MakeAmdShaderIntrinsicsInstruction(uint opcode, uint opcodePhase, uint immediateData)
{
    return ((AmdDxExtShaderIntrinsics_MagicCode << AmdDxExtShaderIntrinsics_MagicCodeShift) |
            (immediateData << AmdDxExtShaderIntrinsics_DataShift) |
            (opcodePhase << AmdDxExtShaderIntrinsics_OpcodePhaseShift) |
            (opcode << AmdDxExtShaderIntrinsics_OpcodeShift));
}


/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_ReadfirstlaneF
*
*   Returns the value of float src for the first active lane of the wavefront.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Readfirstlane) returned S_OK.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_ReadfirstlaneF(float src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Readfirstlane,
                                                          0, 0);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);
    return asfloat(retVal);
}

)EOSHADER", R"EOSHADER(

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_ReadfirstlaneU
*
*   Returns the value of unsigned integer src for the first active lane of the wavefront.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Readfirstlane) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_ReadfirstlaneU(uint src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Readfirstlane,
                                                          0, 0);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_ReadlaneF
*
*   Returns the value of float src for the lane within the wavefront specified by laneId.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Readlane) returned S_OK.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_ReadlaneF(float src, uint laneId)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Readlane, 0,
                                                          laneId);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);
    return asfloat(retVal);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_ReadlaneU
*
*   Returns the value of unsigned integer src for the lane within the wavefront specified by laneId.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Readlane) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_ReadlaneU(uint src, uint laneId)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Readlane, 0,
                                                          laneId);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_LaneId
*
*   Returns the current lane id for the thread within the wavefront.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_LaneId) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_LaneId()
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_LaneId, 0, 0);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetWaveSize
*
*   Returns the wave size for the current shader, including active, inactive and helper lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_GetWaveSize) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetWaveSize()
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_GetWaveSize, 0, 0);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Swizzle
*
*   Generic instruction to shuffle the float src value among different lanes as specified by the
*   operation.
*   Note that the operation parameter must be an immediately specified value not a value from a variable.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Swizzle) returned S_OK.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_SwizzleF(float src, uint operation)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Swizzle, 0,
                                                          operation);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);
    return asfloat(retVal);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_SwizzleU
*
*   Generic instruction to shuffle the unsigned integer src value among different lanes as specified by the
*   operation.
*   Note that the operation parameter must be an immediately specified value not a value from a variable.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Swizzle) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_SwizzleU(uint src, uint operation)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Swizzle, 0,
                                                          operation);

    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src, 0, retVal);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Ballot
*
*   Given an input predicate returns a bit mask indicating for which lanes the predicate is true.
*   Inactive or non-existent lanes will always return 0.  The number of existent lanes is the
*   wavefront size.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Ballot) returned S_OK.
*
*************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_Ballot(bool predicate)
{
    uint instruction;

    uint retVal1;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Ballot,
                                                     AmdDxExtShaderIntrinsicsOpcodePhase_0, 0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, predicate, 0, retVal1);

    uint retVal2;
    instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Ballot,
                                                     AmdDxExtShaderIntrinsicsOpcodePhase_1, 0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, predicate, 0, retVal2);

    return uint2(retVal1, retVal2);
}


/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_BallotAny
*
*   Convenience routine that uses Ballot and returns true if for any of the active lanes the predicate
*   is true.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Ballot) returned S_OK.
*
*************************************************************************************************************
*/
bool AmdDxExtShaderIntrinsics_BallotAny(bool predicate)
{
    uint2 retVal = AmdDxExtShaderIntrinsics_Ballot(predicate);

    return ((retVal.x | retVal.y) != 0 ? true : false);
}


/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_BallotAll
*
*   Convenience routine that uses Ballot and returns true if for all of the active lanes the predicate
*   is true.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Ballot) returned S_OK.
*
*************************************************************************************************************
*/
bool AmdDxExtShaderIntrinsics_BallotAll(bool predicate)
{
    uint2 ballot = AmdDxExtShaderIntrinsics_Ballot(predicate);

    uint2 execMask = AmdDxExtShaderIntrinsics_Ballot(true);

    return ((ballot.x == execMask.x) && (ballot.y == execMask.y));
}


/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_MBCnt
*
*   Returns the masked bit count of the source register for this thread within all the active threads
*   within a wavefront.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_MBCnt) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_MBCnt(uint2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_MBCnt, 0, 0);

    uint retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, src.x, src.y, retVal);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Min3F
*
*   Returns the minimum value of the three floating point source arguments.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Compare3) returned S_OK.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_Min3F(float src0, float src1, float src2)
{
    uint minimum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Min3F,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, asuint(src0), asuint(src1), minimum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Min3F,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, asuint(src2), minimum, minimum);

    return asfloat(minimum);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Min3U
*
*   Returns the minimum value of the three unsigned integer source arguments.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Compare3) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_Min3U(uint src0, uint src1, uint src2)
{
    uint minimum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Min3U,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, src0, src1, minimum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Min3U,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, src2, minimum, minimum);

    return minimum;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Med3F
*
*   Returns the median value of the three floating point source arguments.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Compare3) returned S_OK.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_Med3F(float src0, float src1, float src2)
{
    uint median;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Med3F,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, asuint(src0), asuint(src1), median);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Med3F,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, asuint(src2), median, median);

    return asfloat(median);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Med3U
*
*   Returns the median value of the three unsigned integer source arguments.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Compare3) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_Med3U(uint src0, uint src1, uint src2)
{
    uint median;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Med3U,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, src0, src1, median);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Med3U,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, src2, median, median);

    return median;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Max3F
*
*   Returns the maximum value of the three floating point source arguments.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Compare3) returned S_OK.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_Max3F(float src0, float src1, float src2)
{
    uint maximum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Max3F,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, asuint(src0), asuint(src1), maximum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Max3F,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, asuint(src2), maximum, maximum);

    return asfloat(maximum);
}

)EOSHADER", R"EOSHADER(

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_Max3U
*
*   Returns the maximum value of the three unsigned integer source arguments.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_Compare3) returned S_OK.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_Max3U(uint src0, uint src1, uint src2)
{
    uint maximum;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Max3U,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, src0, src1, maximum);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_Max3U,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, src2, maximum, maximum);

    return maximum;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_IjBarycentricCoords
*
*   Returns the (i, j) barycentric coordinate pair for this shader invocation with the specified
*   interpolation mode at the specified pixel location.  Should not be used for "pull-model" interpolation,
*   PullModelBarycentricCoords should be used instead
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_BaryCoord) returned S_OK.
*
*   Can only be used in pixel shader stages.
*
*************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_IjBarycentricCoords(uint interpMode)
{
    uint2 retVal;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           interpMode);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, 0, 0, retVal.x);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           interpMode);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, retVal.x, 0, retVal.y);

    return float2(asfloat(retVal.x), asfloat(retVal.y));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_PullModelBarycentricCoords
*
*   Returns the (1/W,1/I,1/J) coordinates at the pixel center which can be used for custom interpolation at
*   any location in the pixel.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_BaryCoord) returned S_OK.
*
*   Can only be used in pixel shader stages.
*
*************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_PullModelBarycentricCoords()
{
    uint3 retVal;

    uint instruction1 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                           AmdDxExtShaderIntrinsicsBarycentric_PerspPullModel);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction1, 0, 0, retVal.x);

    uint instruction2 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_1,
                                                           AmdDxExtShaderIntrinsicsBarycentric_PerspPullModel);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction2, retVal.x, 0, retVal.y);

    uint instruction3 = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaryCoord,
                                                           AmdDxExtShaderIntrinsicsOpcodePhase_2,
                                                           AmdDxExtShaderIntrinsicsBarycentric_PerspPullModel);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction3, retVal.y, 0, retVal.z);

    return float3(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_VertexParameter
*
*   Returns the triangle's parameter information at the specified triangle vertex.
*   The vertex and parameter indices must specified as immediate values.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_VtxParam) returned S_OK.
*
*   Only available in pixel shader stages.
*
*************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_VertexParameter(uint vertexIdx, uint parameterIdx)
{
    uint4 retVal;
    uint4 instruction;

    instruction.x = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_VtxParam,
                                 AmdDxExtShaderIntrinsicsOpcodePhase_0,
      ((vertexIdx << AmdDxExtShaderIntrinsicsBarycentric_VtxShift) |
       (parameterIdx << AmdDxExtShaderIntrinsicsBarycentric_ParamShift) |
       (AmdDxExtShaderIntrinsicsBarycentric_ComponentX << AmdDxExtShaderIntrinsicsBarycentric_ComponentShift)));
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.x, 0, 0, retVal.x);

    instruction.y = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_VtxParam,
                                 AmdDxExtShaderIntrinsicsOpcodePhase_0,
      ((vertexIdx << AmdDxExtShaderIntrinsicsBarycentric_VtxShift) |
       (parameterIdx << AmdDxExtShaderIntrinsicsBarycentric_ParamShift) |
       (AmdDxExtShaderIntrinsicsBarycentric_ComponentY << AmdDxExtShaderIntrinsicsBarycentric_ComponentShift)));
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.y, 0, 0, retVal.y);

    instruction.z = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_VtxParam,
                                 AmdDxExtShaderIntrinsicsOpcodePhase_0,
      ((vertexIdx << AmdDxExtShaderIntrinsicsBarycentric_VtxShift) |
       (parameterIdx << AmdDxExtShaderIntrinsicsBarycentric_ParamShift) |
       (AmdDxExtShaderIntrinsicsBarycentric_ComponentZ << AmdDxExtShaderIntrinsicsBarycentric_ComponentShift)));
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.z, 0, 0, retVal.z);

    instruction.w = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_VtxParam,
                                 AmdDxExtShaderIntrinsicsOpcodePhase_0,
      ((vertexIdx << AmdDxExtShaderIntrinsicsBarycentric_VtxShift) |
       (parameterIdx << AmdDxExtShaderIntrinsicsBarycentric_ParamShift) |
       (AmdDxExtShaderIntrinsicsBarycentric_ComponentW << AmdDxExtShaderIntrinsicsBarycentric_ComponentShift)));
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction.w, 0, 0, retVal.w);

    return float4(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z), asfloat(retVal.w));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_VertexParameterComponent
*
*   Returns the triangle's parameter information at the specified triangle vertex and component.
*   The vertex, parameter and component indices must be specified as immediate values.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_VtxParam) returned S_OK.
*
*   Only available in pixel shader stages.
*
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_VertexParameterComponent(uint vertexIdx, uint parameterIdx, uint componentIdx)
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_VtxParam,
                           AmdDxExtShaderIntrinsicsOpcodePhase_0,
                           ((vertexIdx << AmdDxExtShaderIntrinsicsBarycentric_VtxShift) |
                            (parameterIdx << AmdDxExtShaderIntrinsicsBarycentric_ParamShift) |
                            (componentIdx << AmdDxExtShaderIntrinsicsBarycentric_ComponentShift)));
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return asfloat(retVal);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetViewportIndex
*
*   Returns current viewport index for replicated draws when MultiView extension is enabled (broadcast masks
*   are set).
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_MultiViewIndices) returned S_OK.
*
*   Only available in vertex/geometry/domain shader stages.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetViewportIndex()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_ViewportIndex, 0, 0);

    retVal = asuint(AmdDxExtShaderIntrinsicsResource.SampleLevel(AmdDxExtShaderIntrinsicsSamplerState,
                                                                 float3(0, 0, 0),
                                                                 asfloat(instruction)).x);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetViewportIndexPsOnly
*
*   Returns current viewport index for replicated draws when MultiView extension is enabled (broadcast masks
*   are set).
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_MultiViewIndices) returned S_OK.
*
*   Only available in pixel shader stage.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetViewportIndexPsOnly()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_ViewportIndex, 0, 0);

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetRTArraySlice
*
*   Returns current RT array slice for replicated draws when MultiView extension is enabled (broadcast masks
*   are set).
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_MultiViewIndices) returned S_OK.
*
*   Only available in vertex/geometry/domain shader stages.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetRTArraySlice()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_RtArraySlice, 0, 0);

    retVal = asuint(AmdDxExtShaderIntrinsicsResource.SampleLevel(AmdDxExtShaderIntrinsicsSamplerState,
                                                                 float3(0, 0, 0),
                                                                 asfloat(instruction)).x);
    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetRTArraySlicePsOnly
*
*   Returns current RT array slice for replicated draws when MultiView extension is enabled (broadcast masks
*   are set).
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_MultiViewIndices) returned S_OK.
*
*   Only available in pixel shader stage.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetRTArraySlicePsOnly()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_RtArraySlice, 0, 0);

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce
*
*   The following functions perform the specified reduction operation across a wavefront.
*
*   They are available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
*************************************************************************************************************
*/

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : float
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, float src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);

    return asfloat(retVal);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : float2
*************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, float2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint2 retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);

    return float2(asfloat(retVal.x), asfloat(retVal.y));
}

)EOSHADER", R"EOSHADER(

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : float3
*************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, float3 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint3 retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);

    return float3(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : float4
*************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, float4 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint4 retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.w), 0, retVal.w);

    return float4(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z), asfloat(retVal.w));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : int
*************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, int src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : int2
*************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, int2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint2 retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : int3
*************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, int3 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint3 retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveReduce : int4
*************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveReduce(uint waveOp, int4 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveReduce,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift));
    uint4 retVal;

    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.w), 0, retVal.w);

    return retVal;
}



/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveScan
*
*   The following functions perform the specified scan operation across a wavefront.
*
*   They are available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
*************************************************************************************************************
*/

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveScan : float
*************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WaveScan(uint waveOp, uint flags, float src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveScan,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift) |
                                (flags  << AmdDxExtShaderIntrinsicsWaveOp_FlagShift));
    uint retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src), 0, retVal);

    return asfloat(retVal);
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveScan : float2
*************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WaveScan(uint waveOp, uint flags, float2 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveScan,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift) |
                                (flags  << AmdDxExtShaderIntrinsicsWaveOp_FlagShift));
    uint2 retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);

    return float2(asfloat(retVal.x), asfloat(retVal.y));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveScan : float3
*************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WaveScan(uint waveOp, uint flags, float3 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveScan,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift) |
                                (flags  << AmdDxExtShaderIntrinsicsWaveOp_FlagShift));
    uint3 retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);

    return float3(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveScan : float4
*************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WaveScan(uint waveOp, uint flags, float4 src)
{
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_WaveScan,
                                AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                (waveOp << AmdDxExtShaderIntrinsicsWaveOp_OpcodeShift) |
                                (flags  << AmdDxExtShaderIntrinsicsWaveOp_FlagShift));
    uint4 retVal;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.x), 0, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.y), 0, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.z), 0, retVal.z);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, asuint(src.w), 0, retVal.w);

    return float4(asfloat(retVal.x), asfloat(retVal.y), asfloat(retVal.z), asfloat(retVal.w));
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetDrawIndex
*
*   Returns the 0-based draw index in an indirect draw. Always returns 0 for direct draws.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_DrawIndex) returned S_OK.
*
*   Only available in vertex shader stage.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetDrawIndex()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_DrawIndex,
                                                          AmdDxExtShaderIntrinsicsOpcodePhase_0,
                                                          0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetBaseInstance
*
*   Returns the StartInstanceLocation parameter passed to direct or indirect drawing commands.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_BaseInstance) returned S_OK.
*
*   Only available in vertex shader stage.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetBaseInstance()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaseInstance,
        AmdDxExtShaderIntrinsicsOpcodePhase_0,
        0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
*************************************************************************************************************
*   AmdDxExtShaderIntrinsics_GetBaseVertex
*
*   For non-indexed draw commands, returns the StartVertexLocation parameter. For indexed draw commands,
*   returns the BaseVertexLocation parameter.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsSupport_BaseVertex) returned S_OK.
*
*   Only available in vertex shader stage.
*
*************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_GetBaseVertex()
{
    uint retVal;
    uint instruction = MakeAmdShaderIntrinsicsInstruction(AmdDxExtShaderIntrinsicsOpcode_BaseVertex,
        AmdDxExtShaderIntrinsicsOpcodePhase_0,
        0);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instruction, 0, 0, retVal);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_MakeAtomicInstructions
*
*   Creates uint4 with x/y/z/w components containing phase 0/1/2/3 for atomic instructions.
*   NOTE: This is an internal function and should not be called by the source HLSL shader directly.
*
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_MakeAtomicInstructions(uint op)
{
    uint4 instructions;
    instructions.x = MakeAmdShaderIntrinsicsInstruction(
        AmdDxExtShaderIntrinsicsOpcode_AtomicU64, AmdDxExtShaderIntrinsicsOpcodePhase_0, op);
    instructions.y = MakeAmdShaderIntrinsicsInstruction(
        AmdDxExtShaderIntrinsicsOpcode_AtomicU64, AmdDxExtShaderIntrinsicsOpcodePhase_1, op);
    instructions.z = MakeAmdShaderIntrinsicsInstruction(
        AmdDxExtShaderIntrinsicsOpcode_AtomicU64, AmdDxExtShaderIntrinsicsOpcodePhase_2, op);
    instructions.w = MakeAmdShaderIntrinsicsInstruction(
        AmdDxExtShaderIntrinsicsOpcode_AtomicU64, AmdDxExtShaderIntrinsicsOpcodePhase_3, op);
    return instructions;
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicOp
*
*   Creates intrinstic instructions for the specified atomic op.
*   NOTE: These are internal functions and should not be called by the source HLSL shader directly.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicOp(RWByteAddressBuffer uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav.Store(retVal.x, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,   retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(RWTexture1D<uint2> uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav[retVal.x] = retVal.y;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(RWTexture2D<uint2> uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav[uint2(retVal.x, retVal.x)] = retVal.y;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(RWTexture3D<uint2> uav, uint3 address, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x, address.y, retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z, value.x,   retVal.y);
    uav[uint3(retVal.x, retVal.x, retVal.x)] = retVal.y;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,   retVal.y,  retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(
    RWByteAddressBuffer uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav.Store(retVal.x, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(
    RWTexture1D<uint2> uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav[retVal.x] = retVal.y;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(
    RWTexture2D<uint2> uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav[uint2(retVal.x, retVal.x)] = retVal.y;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

uint2 AmdDxExtShaderIntrinsics_AtomicOp(
    RWTexture3D<uint2> uav, uint3 address, uint2 compare_value, uint2 value, uint op)
{
    uint2 retVal;

    const uint4 instructions = AmdDxExtShaderIntrinsics_MakeAtomicInstructions(op);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.x, address.x,       address.y,       retVal.x);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.y, address.z,       value.x,         retVal.y);
    uav[uint3(retVal.x, retVal.x, retVal.x)] = retVal.y;
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.z, value.y,         compare_value.x, retVal.y);
    AmdDxExtShaderIntrinsicsUAV.InterlockedCompareExchange(instructions.w, compare_value.y, retVal.y,        retVal.y);

    return retVal;
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicMinU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic minimum of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicMinU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MinU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicMinU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MinU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicMinU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MinU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicMinU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MinU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicMaxU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic maximum of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicMaxU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MaxU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicMaxU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MaxU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicMaxU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MaxU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicMaxU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_MaxU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicAndU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic AND of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicAndU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AndU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicAndU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AndU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicAndU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AndU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicAndU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AndU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicOrU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic OR of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicOrU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_OrU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicOrU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_OrU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicOrU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_OrU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicOrU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_OrU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicXorU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic XOR of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicXorU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XorU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicXorU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XorU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicXorU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XorU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicXorU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XorU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicAddU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic add of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicAddU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AddU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicAddU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AddU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicAddU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AddU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicAddU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_AddU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicXchgU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic exchange of value with the UAV at address, returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicXchgU64(RWByteAddressBuffer uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicXchgU64(RWTexture1D<uint2> uav, uint address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicXchgU64(RWTexture2D<uint2> uav, uint2 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicXchgU64(RWTexture3D<uint2> uav, uint3 address, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_XchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), value, op);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_AtomicCmpXchgU64
*
*   The following functions are available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_AtomicU64) returned S_OK.
*
*   Performs 64-bit atomic compare of comparison value with UAV at address, stores value if values match,
*   returns the original value.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_AtomicCmpXchgU64(
    RWByteAddressBuffer uav, uint address, uint2 compare_value, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), compare_value, value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicCmpXchgU64(
    RWTexture1D<uint2> uav, uint address, uint2 compare_value, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address, 0, 0), compare_value, value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicCmpXchgU64(
    RWTexture2D<uint2> uav, uint2 address, uint2 compare_value, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, 0), compare_value, value, op);
}

uint2 AmdDxExtShaderIntrinsics_AtomicCmpXchgU64(
    RWTexture3D<uint2> uav, uint3 address, uint2 compare_value, uint2 value)
{
    const uint op = AmdDxExtShaderIntrinsicsAtomicOp_CmpXchgU64;
    return AmdDxExtShaderIntrinsics_AtomicOp(uav, uint3(address.x, address.y, address.z), compare_value, value, op);
}


/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum
*
*   Performs reduction operation across a wave and returns the result of the reduction (sum of all threads in a wave)
*   to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WaveActiveSum(float src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WaveActiveSum(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WaveActiveSum(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WaveActiveSum(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveSum(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveSum(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveSum(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveSum(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveSum(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveSum(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveSum(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveSum<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveSum(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_AddU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct
*
*   Performs reduction operation across a wave and returns the result of the reduction (product of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WaveActiveProduct(float src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WaveActiveProduct(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WaveActiveProduct(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WaveActiveProduct(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveProduct(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulI, src);
}

)EOSHADER",
                                                              R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveProduct(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveProduct(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveProduct(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveProduct(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveProduct(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveProduct(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveProduct<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveProduct(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MulU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin
*
*   Performs reduction operation across a wave and returns the result of the reduction (minimum of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WaveActiveMin(float src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WaveActiveMin(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WaveActiveMin(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WaveActiveMin(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveMin(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveMin(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveMin(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveMin(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveMin(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveMin(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveMin(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMin<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveMin(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MinU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax
*
*   Performs reduction operation across a wave and returns the result of the reduction (maximum of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WaveActiveMax(float src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WaveActiveMax(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WaveActiveMax(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WaveActiveMax(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxF, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveMax(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveMax(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveMax(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveMax(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxI, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveMax(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxU, src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveMax(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveMax(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveMax<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveMax(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_MaxU, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd
*
*   Performs reduction operation across a wave and returns the result of the reduction (Bitwise AND of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveBitAnd(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveBitAnd(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveBitAnd(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveBitAnd(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveBitAnd(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveBitAnd(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveBitAnd(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitAnd<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveBitAnd(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_And, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr
*
*   Performs reduction operation across a wave and returns the result of the reduction (Bitwise OR of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveBitOr(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce( AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveBitOr(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveBitOr(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveBitOr(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveBitOr(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveBitOr(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveBitOr(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitOr<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveBitOr(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Or, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor
*
*   Performs reduction operation across a wave and returns the result of the reduction (Bitwise XOR of all threads in a
*   wave) to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveReduce) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WaveActiveBitXor(int src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WaveActiveBitXor(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WaveActiveBitXor(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WaveActiveBitXor(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WaveActiveBitXor(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WaveActiveBitXor(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WaveActiveBitXor(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WaveActiveBitXor<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WaveActiveBitXor(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveReduce(AmdDxExtShaderIntrinsicsWaveOp_Xor, src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum
*
*   Performs a prefix (exclusive) scan operation across a wave and returns the resulting sum to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePrefixSum(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePrefixSum(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePrefixSum(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePrefixSum(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePrefixSum(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(
                                            AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                            AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                            src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePrefixSum(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePrefixSum(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePrefixSum(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePrefixSum(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePrefixSum(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePrefixSum(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixSum<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePrefixSum(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct
*
*   Performs a prefix scan operation across a wave and returns the resulting product to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePrefixProduct(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePrefixProduct(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePrefixProduct(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePrefixProduct(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePrefixProduct(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePrefixProduct(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePrefixProduct(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePrefixProduct(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePrefixProduct(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePrefixProduct(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePrefixProduct(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixProduct<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePrefixProduct(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin
*
*   Performs a prefix scan operation across a wave and returns the resulting minimum value to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePrefixMin(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePrefixMin(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePrefixMin(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePrefixMin(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePrefixMin(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePrefixMin(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePrefixMin(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePrefixMin(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePrefixMin(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePrefixMin(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePrefixMin(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMin<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePrefixMin(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax
*
*   Performs a prefix scan operation across a wave and returns the resulting maximum value to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePrefixMax(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePrefixMax(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePrefixMax(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePrefixMax(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePrefixMax(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePrefixMax(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePrefixMax(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePrefixMax(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePrefixMax(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePrefixMax(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePrefixMax(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePrefixMax<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePrefixMax(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Exclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum
*
*   Performs a Postfix (Inclusive) scan operation across a wave and returns the resulting sum to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePostfixSum(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePostfixSum(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePostfixSum(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePostfixSum(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePostfixSum(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePostfixSum(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePostfixSum(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePostfixSum(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePostfixSum(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePostfixSum(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePostfixSum(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixSum<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePostfixSum(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_AddU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct
*
*   Performs a Postfix scan operation across a wave and returns the resulting product to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePostfixProduct(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePostfixProduct(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePostfixProduct(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePostfixProduct(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePostfixProduct(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePostfixProduct(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePostfixProduct(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePostfixProduct(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePostfixProduct(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePostfixProduct(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePostfixProduct(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

)EOSHADER", R"EOSHADER(

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixProduct<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePostfixProduct(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MulU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin
*
*   Performs a Postfix scan operation across a wave and returns the resulting minimum value to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePostfixMin(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePostfixMin(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePostfixMin(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePostfixMin(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePostfixMin(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePostfixMin(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePostfixMin(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePostfixMin(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePostfixMin(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePostfixMin(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePostfixMin(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMin<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePostfixMin(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MinU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax
*
*   Performs a Postfix scan operation across a wave and returns the resulting maximum value to all participating lanes.
*
*   Available if CheckSupport(AmdDxExtShaderIntrinsicsOpcode_WaveScan) returned S_OK.
*
*   Available in all shader stages.
*
***********************************************************************************************************************
*/
float AmdDxExtShaderIntrinsics_WavePostfixMax(float src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<float2>
***********************************************************************************************************************
*/
float2 AmdDxExtShaderIntrinsics_WavePostfixMax(float2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<float3>
***********************************************************************************************************************
*/
float3 AmdDxExtShaderIntrinsics_WavePostfixMax(float3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<float4>
***********************************************************************************************************************
*/
float4 AmdDxExtShaderIntrinsics_WavePostfixMax(float4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxF,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<int>
***********************************************************************************************************************
*/
int AmdDxExtShaderIntrinsics_WavePostfixMax(int src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<int2>
***********************************************************************************************************************
*/
int2 AmdDxExtShaderIntrinsics_WavePostfixMax(int2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<int3>
***********************************************************************************************************************
*/
int3 AmdDxExtShaderIntrinsics_WavePostfixMax(int3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<int4>
***********************************************************************************************************************
*/
int4 AmdDxExtShaderIntrinsics_WavePostfixMax(int4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxI,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<uint>
***********************************************************************************************************************
*/
uint AmdDxExtShaderIntrinsics_WavePostfixMax(uint src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<uint2>
***********************************************************************************************************************
*/
uint2 AmdDxExtShaderIntrinsics_WavePostfixMax(uint2 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<uint3>
***********************************************************************************************************************
*/
uint3 AmdDxExtShaderIntrinsics_WavePostfixMax(uint3 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}

/**
***********************************************************************************************************************
*   AmdDxExtShaderIntrinsics_WavePostfixMax<uint4>
***********************************************************************************************************************
*/
uint4 AmdDxExtShaderIntrinsics_WavePostfixMax(uint4 src)
{
    return AmdDxExtShaderIntrinsics_WaveScan(AmdDxExtShaderIntrinsicsWaveOp_MaxU,
                                             AmdDxExtShaderIntrinsicsWaveOp_Inclusive,
                                             src);
}


#endif // _AMDDXEXTSHADERINTRINSICS_HLSL_
)EOSHADER"};

inline std::string ags_shader_intrinsics_dx11_hlsl()
{
  std::string ret;
  for(size_t i = 0; i < sizeof(ags_shader_intrinsics_dx11_hlsl_array) /
                            sizeof(ags_shader_intrinsics_dx11_hlsl_array[0]);
      i++)
    ret += ags_shader_intrinsics_dx11_hlsl_array[i];
  return ret;
}
