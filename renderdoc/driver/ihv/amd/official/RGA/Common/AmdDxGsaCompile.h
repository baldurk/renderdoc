//=================================================================
// Copyright 2017 Advanced Micro Devices, Inc. All rights reserved.
//=================================================================
/**
***************************************************************************************************
* @file  AmdDxGsaCompile.h
* @brief Backdoor GSA compile extension include file.
***************************************************************************************************
*/
#ifndef _AMDDXGSACOMPILE_H_
#define _AMDDXGSACOMPILE_H_

#include <windows.h>

#if defined(__cplusplus)
extern "C"
{
#endif


/**
***************************************************************************************************
* @brief Identifies compile options to be modified in AmdDxGsaCompileShader() call.
***************************************************************************************************
*/
typedef enum _AmdDxGsaCompileOptionEnum
{
    AmdDxGsaBiasScheduleToMinimizeRegs,
    AmdDxGsaNoIfConversion,
    AmdDxGsaIfConversionGuarantee,
    AmdDxGsaIfConversionHeuristic,
    AmdDxGsaIfConversionHeuristicOgl,
    AmdDxGsaIfConversionAlways,
    AmdDxGsaEnableShaderIntrinsics,
    AmdDxGsaShaderIntrinsicsUAVSlot,
    AmdDxGsaCompileOptionLast
} AmdDxGsaCompileOptionEnum;

/**
***************************************************************************************************
* @brief Compiler settings/value pair specified in AmdDxGsaCompileShader() calls.
***************************************************************************************************
*/
typedef struct _AmdDxGsaCompileOption
{
    AmdDxGsaCompileOptionEnum setting;
    INT                       value;
} AmdDxGsaCompileOption;

/**
***************************************************************************************************
* @brief Stats about the compiled shader. This structure will be stored in the .stats ELF section
***************************************************************************************************
*/
typedef struct _AmdDxGsaCompileStats
{
    UINT numSgprsUsed;       ///< Number of SGPRs used by the shader
    UINT availableSgprs;     ///< Number of SGPRs available
    UINT numVgprsUsed;       ///< Number of VGPRs used by the shader
    UINT availableVgprs;     ///< Number of VGPRs available
    UINT usedLdsBytes;       ///< Bytes of LDS used by a thread group
    UINT availableLdsBytes;  ///< Bytes of LDS available to a thread group
    UINT usedScratchBytes;   ///< Bytes of scratch space used by the shader
    UINT numAluInst;         ///< Number of ALU instructions in the shader
    UINT numControlFlowInst; ///< Number of control flow instructions in the shader
    UINT numTfetchInst;      ///< Number of HW TFETCHinstructions / Tx Units used
    UINT reserved[6];
} AmdDxGsaCompileStats;

/**
***************************************************************************************************
* @brief Input type of shader code
***************************************************************************************************
*/
enum AmdDxGsaInputType
{
    GsaInputDxAsmBin = 0,   ///< DXASM binary
    GsaInputIlText   = 1    ///< IL text
};

/**
***************************************************************************************************
* @brief AmdDxGsaCompileShader() input structure.
***************************************************************************************************
*/
typedef struct _AmdDxGsaCompileShaderInput
{
    /// Target GPU chip family (defined in atiid.h, e.g. FAMILY_SI).  Only FAMILY_SI and later are
    /// currently supported.
    UINT chipFamily;

    /// Target GPU chip revision (defined in hardware-specific chip headers, e.g. si_id.h).
    UINT chipRevision;

    /// Pointer to DXASM binary or IL text to be compiled.
    const VOID* pShaderByteCode;

    /// Length of pShaderByteCode in bytes.
    SIZE_T      byteCodeLength;

    /// An array of setting/value pairs to control compilation options.  NULL is valid, if all
    /// default options are desired.
    const AmdDxGsaCompileOption* pCompileOptions;

    /// Length of pCompileOptions array.
    UINT                         numCompileOptions;

    /// Input type
    AmdDxGsaInputType            inputType;

    /// Reserved entry must be set to all 0s.
    UINT reserved[6];
} AmdDxGsaCompileShaderInput;

/**
***************************************************************************************************
* @brief AmdDxGsaCompileShader() output structure.
***************************************************************************************************
*/
typedef struct _AmdDxGsaCompileShaderOutput
{
    /// Must be set to sizeof(AmdDxGsaCompileShaderOutput).
    SIZE_T size;

    /// Output ELF object. Contains the following sections:
    // .amdil             - IL binary
    // .amdil_disassembly - IL text string
    // .text              - ISA binary
    // .disassembly       - ISA text string
    // .stats             - AmdDxGsaCompileStats structure
    VOID*  pShaderBinary;

    /// Size of the ELF object in bytes.
    SIZE_T shaderBinarySize;
} AmdDxGsaCompileShaderOutput;

HRESULT __cdecl AmdDxGsaCompileShader(const AmdDxGsaCompileShaderInput* pIn,
                                      AmdDxGsaCompileShaderOutput*      pOut);
typedef HRESULT (__cdecl *PfnAmdDxGsaCompileShader)(const AmdDxGsaCompileShaderInput*,
                                                    AmdDxGsaCompileShaderOutput*);

VOID __cdecl AmdDxGsaFreeCompiledShader(VOID* pShaderBinary);
typedef VOID (__cdecl *PfnAmdDxGsaFreeCompiledShader)(VOID*);

#if defined(__cplusplus)
}
#endif

#endif // _AMDDXGSACOMPILE_H_

