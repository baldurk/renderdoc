//=====================================================================
// Copyright (c) 2018    Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================
#include "BC5_Encode_kernel.h"

//============================================== BC5 INTERFACES =======================================================

void  CompressBlockBC5_Internal(CMP_Vec4uc srcBlockTemp[16],
                                CMP_GLOBAL CGU_UINT32 compressedBlock[4],
                                CMP_GLOBAL  CMP_BC15Options *BC15options)
{
    if (BC15options->m_fquality) {
        // Resreved
    }
    CGU_UINT8    blkindex = 0;
    CGU_UINT8    srcindex = 0;
    CGU_UINT8    alphaBlock[16];
    for (CGU_INT32 j = 0; j < 4; j++) {
        for (CGU_INT32 i = 0; i < 4; i++) {
            alphaBlock[blkindex++] = (CGU_UINT8)srcBlockTemp[srcindex].x;  // Red channel
            srcindex++;
        }
    }
    CompressAlphaBlock(alphaBlock,&compressedBlock[0]);

    blkindex = 0;
    srcindex = 0;
    for (CGU_INT32 j = 0; j < 4; j++) {
        for (CGU_INT32 i = 0; i < 4; i++) {
            alphaBlock[blkindex++] = (CGU_UINT8)srcBlockTemp[srcindex].y;  // Green channel
            srcindex++;
        }
    }
    CompressAlphaBlock(alphaBlock,&compressedBlock[2]);

}

void  DecompressBC5_Internal(CMP_GLOBAL CGU_UINT8 rgbaBlock[64], 
                             CGU_UINT32 compressedBlock[4],
                             CMP_BC15Options *BC15options)
{
    CGU_UINT8 alphaBlockR[BLOCK_SIZE_4X4];
    CGU_UINT8 alphaBlockG[BLOCK_SIZE_4X4];

    DecompressAlphaBlock(alphaBlockR, &compressedBlock[0]);
    DecompressAlphaBlock(alphaBlockG, &compressedBlock[2]);
 
    CGU_UINT8    blkindex = 0;
    CGU_UINT8    srcindex = 0;

    if (BC15options->m_mapDecodeRGBA)
    {
        for (CGU_INT32 j = 0; j < 4; j++) {
            for (CGU_INT32 i = 0; i < 4; i++) {
                rgbaBlock[blkindex++] = (CGU_UINT8)alphaBlockR[srcindex];
                rgbaBlock[blkindex++] = (CGU_UINT8)alphaBlockG[srcindex];
                rgbaBlock[blkindex++] = 0;
                rgbaBlock[blkindex++] = 255;
                srcindex++;
            }
        }
    }
    else
    {
        for (CGU_INT32 j = 0; j < 4; j++) {
            for (CGU_INT32 i = 0; i < 4; i++) {
                rgbaBlock[blkindex++] = 0;
                rgbaBlock[blkindex++] = (CGU_UINT8)alphaBlockG[srcindex];
                rgbaBlock[blkindex++] = (CGU_UINT8)alphaBlockR[srcindex];
                rgbaBlock[blkindex++] = 255;
                srcindex++;
            }
        }
    }

}


void  CompressBlockBC5_DualChannel_Internal(const CGU_UINT8 srcBlockR[16],
                                            const CGU_UINT8 srcBlockG[16],
                                            CMP_GLOBAL  CGU_UINT32 compressedBlock[4],
                                            CMP_GLOBAL  const CMP_BC15Options *BC15options)
{
    if (BC15options) {}
    CompressAlphaBlock(srcBlockR,&compressedBlock[0]);
    CompressAlphaBlock(srcBlockG,&compressedBlock[2]);
}

void  DecompressBC5_DualChannel_Internal(CMP_GLOBAL CGU_UINT8 srcBlockR[16],
                                         CMP_GLOBAL CGU_UINT8 srcBlockG[16], 
                                         const CGU_UINT32 compressedBlock[4],
                                         const CMP_BC15Options *BC15options)
{
    if (BC15options) {}
    DecompressAlphaBlock(srcBlockR, &compressedBlock[0]);
    DecompressAlphaBlock(srcBlockG, &compressedBlock[2]);
}


//============================================== USER INTERFACES ========================================================
#ifndef ASPM_GPU

int CMP_CDECL CreateOptionsBC5(void **options)
{
    CMP_BC15Options *BC15optionsDefault = new CMP_BC15Options;
    if (BC15optionsDefault) {
        SetDefaultBC15Options(BC15optionsDefault);
        (*options) = BC15optionsDefault;
    }
    else {
        (*options) = NULL;
        return CGU_CORE_ERR_NEWMEM;
    }
    return CGU_CORE_OK;
}

int CMP_CDECL DestroyOptionsBC5(void *options)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    CMP_BC15Options *BCOptions = reinterpret_cast <CMP_BC15Options *>(options);
    delete BCOptions;
    return CGU_CORE_OK;
}

int CMP_CDECL SetQualityBC5(void *options,
    CGU_FLOAT fquality)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    CMP_BC15Options *BC15optionsDefault = reinterpret_cast <CMP_BC15Options *>(options);
    if (fquality < 0.0f) fquality = 0.0f;
    else
        if (fquality > 1.0f) fquality = 1.0f;
    BC15optionsDefault->m_fquality = fquality;
    return CGU_CORE_OK;
}


int CMP_CDECL CompressBlockBC5(const CGU_UINT8 *srcBlockR,
                               unsigned int srcStrideInBytes1,
                               const CGU_UINT8 *srcBlockG,
                               unsigned int srcStrideInBytes2,
                               CMP_GLOBAL CGU_UINT8 cmpBlock[16],
                               const void *options = NULL) {
    CGU_UINT8 inBlockR[16];

    //----------------------------------
    // Fill the inBlock with source data
    //----------------------------------
    CGU_INT srcpos = 0;
    CGU_INT dstptr = 0;
    for (CGU_UINT8 row = 0; row < 4; row++)
    {
        srcpos = row * srcStrideInBytes1;
        for (CGU_UINT8 col = 0; col < 4; col++)
        {
            inBlockR[dstptr++] = CGU_UINT8(srcBlockR[srcpos++]);
        }
    }


    CGU_UINT8 inBlockG[16];
    //----------------------------------
    // Fill the inBlock with source data
    //----------------------------------
    srcpos = 0;
    dstptr = 0;
    for (CGU_UINT8 row = 0; row < 4; row++)
    {
        srcpos = row * srcStrideInBytes2;
        for (CGU_UINT8 col = 0; col < 4; col++)
        {
            inBlockG[dstptr++] = CGU_UINT8(srcBlockG[srcpos++]);
        }
    }


    CMP_BC15Options *BC15options = (CMP_BC15Options *)options;
    CMP_BC15Options BC15optionsDefault;
    if (BC15options == NULL)
    {
        BC15options = &BC15optionsDefault;
        SetDefaultBC15Options(BC15options);
    }

    CompressBlockBC5_DualChannel_Internal(inBlockR,inBlockG, (CMP_GLOBAL CGU_UINT32 *)cmpBlock, BC15options);
    return CGU_CORE_OK;
}

int  CMP_CDECL DecompressBlockBC5(const CGU_UINT8 cmpBlock[16],
                              CMP_GLOBAL CGU_UINT8 srcBlockR[16],
                              CMP_GLOBAL CGU_UINT8 srcBlockG[16],
                              const void *options = NULL) {
    CMP_BC15Options *BC15options = (CMP_BC15Options *)options;
    CMP_BC15Options BC15optionsDefault;
    if (BC15options == NULL)
    {
        BC15options = &BC15optionsDefault;
        SetDefaultBC15Options(BC15options);
    }
    DecompressBC5_DualChannel_Internal(srcBlockR,srcBlockG,(CGU_UINT32 *)cmpBlock,BC15options);

    return CGU_CORE_OK;
}

#endif

//============================================== OpenCL USER INTERFACE ====================================================
#ifdef ASPM_GPU
CMP_STATIC CMP_KERNEL void CMP_GPUEncoder(CMP_GLOBAL  const CMP_Vec4uc*   ImageSource,
                                          CMP_GLOBAL  CGU_UINT8*          ImageDestination,
                                          CMP_GLOBAL  Source_Info*        SourceInfo,
                                          CMP_GLOBAL  CMP_BC15Options*    BC15options
)
{
    CGU_UINT32 xID;
    CGU_UINT32 yID;

#ifdef ASPM_GPU
    xID = get_global_id(0);
    yID = get_global_id(1);
#else
    xID = 0;
    yID = 0;
#endif

    if (xID >= (SourceInfo->m_src_width / BlockX)) return;
    if (yID >= (SourceInfo->m_src_height / BlockX)) return;
    int  srcWidth = SourceInfo->m_src_width;

    CGU_UINT32 destI = (xID*BC5CompBlockSize) + (yID*(srcWidth / BlockX)*BC5CompBlockSize);
    int srcindex = 4 * (yID * srcWidth + xID);
    int blkindex = 0;
    CMP_Vec4uc srcData[16];
    srcWidth = srcWidth - 4;

    for ( CGU_INT32 j = 0; j < 4; j++) {
        for ( CGU_INT32 i = 0; i < 4; i++) {
            srcData[blkindex++] = ImageSource[srcindex++];
        }
        srcindex += srcWidth;
    }

    CompressBlockBC5_Internal(srcData, (CMP_GLOBAL CGU_UINT32 *)&ImageDestination[destI], BC15options);
}
#endif
