//=====================================================================
// Copyright (c) 2019    Advanced Micro Devices, Inc. All rights reserved.
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
// Ref: GPUOpen-Tools/Compressonator

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------
// Common BC7 Header
//--------------------------------------
#include "BC7_Encode_Kernel.h"

#ifndef ASPM
//---------------------------------------------
// Predefinitions for GPU and CPU compiled code
//---------------------------------------------
#define ENABLE_CODE

#ifndef ASPM_GPU
    // using code for CPU or hybrid (CPU/GPU) 
    //#include "BC7.h"
#endif


INLINE CGU_INT a_compare( const void *arg1, const void *arg2 )
{
    if (((CMP_di* )arg1)->image-((CMP_di* )arg2)->image > 0 ) return  1;
    if (((CMP_di* )arg1)->image-((CMP_di* )arg2)->image < 0 ) return -1;
    return 0;
};

#endif

#ifndef ASPM_GPU
CMP_GLOBAL BC7_EncodeRamps BC7EncodeRamps
#ifndef ASPM
    = {0}
#endif
;

//---------------------------------------------
// CPU: Computes max of two float values
//---------------------------------------------
float bc7_maxf(float l1, float r1)
{
    return (l1 > r1 ? l1 : r1);
}

//---------------------------------------------
// CPU: Computes max of two float values
//---------------------------------------------
float bc7_minf(float l1, float r1)
{
    return (l1 < r1 ? l1 : r1);
}

#endif

INLINE CGV_EPOCODE shift_right_epocode(CGV_EPOCODE v,  CGU_INT bits)
{
   return v>>bits; // (perf warning expected)
}

INLINE CGV_EPOCODE expand_epocode(CGV_EPOCODE v,  CGU_INT bits)
{
   CGV_EPOCODE vv = v<<(8-bits);
   return vv + shift_right_epocode(vv, bits);
}

// valid bit range is 0..8
CGU_INT expandbits(CGU_INT bits, CGU_INT v) 
{
    return (  v << (8-bits) | v >> (2* bits - 8)); 
}

CMP_EXPORT CGU_INT bc7_isa() {
#if defined(ISPC_TARGET_SSE2)
    ASPM_PRINT(("SSE2"));
    return 0;
#elif defined(ISPC_TARGET_SSE4)
    ASPM_PRINT(("SSE4"));
    return 1;
#elif defined(ISPC_TARGET_AVX)
    ASPM_PRINT(("AVX"));
    return 2;
#elif defined(ISPC_TARGET_AVX2)
    ASPM_PRINT(("AVX2"));
    return 3;
#else
    ASPM_PRINT(("CPU"));
    return -1;
#endif
}

CMP_EXPORT void init_BC7ramps()
{
#ifdef ASPM_GPU
#else
    CMP_STATIC CGU_BOOL g_rampsInitialized = FALSE;
    if (g_rampsInitialized == TRUE) return;
    g_rampsInitialized = TRUE;
    BC7EncodeRamps.ramp_init = TRUE;
    BC7EncodeRamps.sp_err = new CGU_UINT8[3 * 4 * 256 * 2 * 2 * 16];
    BC7EncodeRamps.sp_idx = new CGU_INT[3 * 4 * 256 * 2 * 2 * 16 * 2];
    BC7EncodeRamps.ramp = new CGU_FLOAT[3 * 4 * 256 * 256 * 16];

    //bc7_isa(); ASPM_PRINT((" INIT Ramps\n"));

    CGU_INT     bits;
    CGU_INT     p1;
    CGU_INT     p2;
    CGU_INT     clogBC7;
    CGU_INT     index;
    CGU_INT     j;
    CGU_INT     o1;
    CGU_INT     o2;
    CGU_INT     maxi = 0;


    for (bits = BIT_BASE; bits<BIT_RANGE; bits++)
    {
        for (p1 = 0; p1<(1 << bits); p1++)
        {
            BC7EncodeRamps.ep_d[BTT(bits)][p1] = expandbits(bits, p1);
        } //p1
    }//bits<BIT_RANGE

    for (clogBC7 = LOG_CL_BASE; clogBC7<LOG_CL_RANGE; clogBC7++)
    {
        for (bits = BIT_BASE; bits<BIT_RANGE; bits++)
        {

#ifdef USE_BC7_RAMP
            for (p1 = 0; p1<(1 << bits); p1++)
            {
                for (p2 = 0; p2<(1 << bits); p2++)
                {
                    for (index = 0; index<(1 << clogBC7); index++)
                    {
                        if (index > maxi) maxi = index;
                        BC7EncodeRamps.ramp[(CLT(clogBC7)*4*256*256*16)+(BTT(bits)*256*256*16)+(p1*256*16)+(p2*16)+index] =
                        //floor((CGV_IMAGE)BC7EncodeRamps.ep_d[BTT(bits)][p1] + rampWeights[clogBC7][index] * (CGV_IMAGE)((BC7EncodeRamps.ep_d[BTT(bits)][p2] - BC7EncodeRamps.ep_d[BTT(bits)][p1]))+ 0.5F);
                            static_cast<CGU_FLOAT> (floor(BC7EncodeRamps.ep_d[BTT(bits)][p1] +
                                                         rampWeights[clogBC7][index] *
                                                             ((BC7EncodeRamps.ep_d[BTT(bits)][p2] -
                                                               BC7EncodeRamps.ep_d[BTT(bits)][p1])) +
                                                         0.5F));
                    }//index<(1 << clogBC7)
                }//p2<(1 << bits)
            }//p1<(1 << bits)
#endif

#ifdef USE_BC7_SP_ERR_IDX
            for (j = 0; j<256; j++)
            {
                for (o1 = 0; o1<2; o1++)
                {
                    for (o2 = 0; o2<2; o2++)
                    {
                        for (index = 0; index<16; index++) {
                            BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+0] = 0;
                            BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+1] = 255;
                            BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(j*2*2*16)+(o1*2*16)+(o2*16)+index] = 255;
                        } // i<16
                    }//o2<2;
                }//o1<2
            } //j<256

            for (p1 = 0; p1<(1 << bits); p1++)
            {
                for (p2 = 0; p2<(1 << bits); p2++)
                {
                    for (index = 0; index<(1 << clogBC7); index++) 
                    {
#ifdef USE_BC7_RAMP
                        CGV_EPOCODE floatf = (CGV_EPOCODE)BC7EncodeRamps.ramp[(CLT(clogBC7)*4*256*256*16)+(BTT(bits)*256*256*16)+(p1*256*16)+(p2*16)+index];
#else
                        CGV_EPOCODE floatf = floor((CGV_IMAGE)BC7EncodeRamps.ep_d[BTT(bits)][p1] + rampWeights[clogBC7][index] * (CGV_IMAGE)((BC7EncodeRamps.ep_d[BTT(bits)][p2] - BC7EncodeRamps.ep_d[BTT(bits)][p1]))+ 0.5F);
#endif
                        BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(floatf*2*2*16*2)+((p1 & 0x1)*2*16*2)+((p2 & 0x1)*16*2)+(index*2)+0] = p1;
                        BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(floatf*2*2*16*2)+((p1 & 0x1)*2*16*2)+((p2 & 0x1)*16*2)+(index*2)+1] = p2;
                        BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(floatf*2*2*16)+((p1 & 0x1)*2*16)+(p2 & 0x1*16)+index] = 0;
                    } //i<(1 << clogBC7)
                } //p2
            }//p1<(1 << bits)

            for (j = 0; j<256; j++)
            {
                for (o1 = 0; o1<2; o1++)
                {
                    for (o2 = 0; o2<2; o2++)
                    {
                        for (index = 0; index<(1 << clogBC7); index++)
                        {
                            if ( // check for unitialized sp_idx
                                (BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+0] == 0) && 
                                (BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+1] == 255)
                                )

                            {
                                CGU_INT k;
                                CGU_INT tf;
                                CGU_INT tc;

                                for (k = 1; k<256; k++)
                                {
                                    tf = j - k;
                                    tc = j + k;
                                    if ((tf >= 0 && BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(tf*2*2*16)+(o1*2*16)+(o2*16)+index] == 0)) 
                                    {
                                        BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+0] = BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(tf*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+0];
                                        BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+1] = BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(tf*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+1];
                                        break;
                                    }
                                    else if ((tc < 256 && BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(tc*2*2*16)+(o1*2*16)+(o2*16)+index] == 0)) 
                                    {
                                        BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(j*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+0] = BC7EncodeRamps.sp_idx[(CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits)*256*2*2*16*2)+(tc*2*2*16*2)+(o1*2*16*2)+(o2*16*2)+(index*2)+0];
                                        break;
                                    }
                                }

                                //BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(j*2*2*16)+(o1*2*16)+(o2*16)+index] = (CGV_ERROR) k;
                                BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(j*2*2*16)+(o1*2*16)+(o2*16)+index] = (CGU_UINT8)k;

                            } //sp_idx < 0
                        }//i<(1 << clogBC7)
                    }//o2
                }//o1
            }//j
#endif

        } //bits<BIT_RANGE
    } //clogBC7<LOG_CL_RANGE
#endif
}

//----------------------------------------------------------
//====== Common BC7 ASPM Code used for SPMD (CPU/GPU) ======
//----------------------------------------------------------
#ifndef ASPM_GPU
//#define USE_ICMP
#endif

#define SOURCE_BLOCK_SIZE               16      // Size of a source block in pixels (each pixel has RGBA:8888 channels)
#define COMPRESSED_BLOCK_SIZE           16  // Size of a compressed block in bytes
#define MAX_CHANNELS                    4
#define MAX_SUBSETS                     3   // Maximum number of possible subsets
#define MAX_SUBSET_SIZE                 16  // Largest possible size for an individual subset

#ifndef ASPM_GPU
    extern "C" CGU_INT timerStart(CGU_INT id);
    extern "C" CGU_INT timerEnd(CGU_INT id);

#define TIMERSTART(x)   timerStart(x)
    #define TIMEREND(x)     timerEnd(x)
#else
    #define TIMERSTART(x)
    #define TIMEREND(x)
#endif

#ifdef ASPM_GPU
#define GATHER_UINT8(x,y)   x[y]
#else
#define GATHER_UINT8(x,y)   gather_uint8(x,y)
#endif
// INLINE CGV_BYTE  gather_uint8 (CMP_CONSTANT  CGU_UINT8 * __constant uniform ptr, CGV_INT idx)
// {
//    return ptr[idx]; // (perf warning expected)
// }
//
// INLINE CGV_CMPOUT gather_cmpout(CMP_CONSTANT CGV_CMPOUT * __constant uniform ptr, CGU_INT idx)
// {
//    return ptr[idx]; // (perf warning expected)
// }
//
//INLINE CGV_INDEX gather_index(CMP_CONSTANT varying CGV_INDEX* __constant uniform ptr, CGV_INT idx)
//{
//   return ptr[idx]; // (perf warning expected)
//}
//
//INLINE void       scatter_index(CGV_INDEX* ptr, CGV_INT idx, CGV_INDEX value)
//{
//   ptr[idx] = value; // (perf warning expected)
//}
//

#ifdef USE_VARYING
INLINE CGV_EPOCODE gather_epocode(CMP_CONSTANT CGV_EPOCODE* ptr, CGV_TYPEINT idx)
{
   return ptr[idx]; // (perf warning expected)
}
#endif

INLINE CGV_SHIFT32 gather_partid(CMP_CONSTANT CGV_SHIFT32 * uniform ptr, CGV_PARTID idx)
{
   return ptr[idx]; // (perf warning expected)
}

//INLINE CGV_BYTE gather_vuint8(CMP_CONSTANT varying CGV_BYTE* __constant uniform ptr, CGV_INT idx)
//{
//   return ptr[idx]; // (perf warning expected)
//}

INLINE void cmp_swap_epo(CGV_EPOCODE u[], CGV_EPOCODE v[], CGV_EPOCODE n)
{
    for (CGU_INT i=0; i<n; i++)
   {
      CGV_EPOCODE t = u[i];
      u[i] = v[i];
      v[i] = t;
   }
}

INLINE void cmp_swap_index(CGV_INDEX u[], CGV_INDEX v[], CGU_INT n)
{
    for (CGU_INT i=0; i<n; i++)
   {
      CGV_INDEX t = u[i];
      u[i] = v[i];
      v[i] = t;
   }
}

void cmp_memsetBC7(CGV_BYTE ptr[], CGV_BYTE value, CGU_UINT32 size)
{
    for (CGV_SHIFT32 i=0; i<size; i++)
    {
        ptr[i] = value;
    }
}

void cmp_memcpy(CMP_GLOBAL CGU_UINT8 dst[],CGU_UINT8 src[],CGU_UINT32 size)
{
#ifdef ASPM_GPU
    for (CGV_INT i=0; i<size; i++)
    {
        dst[i] = src[i];
    }
#else
    memcpy(dst,src,size);
#endif
}

INLINE CGV_IMAGE sq_image(CGV_IMAGE v)
{
   return v*v;
}

INLINE CGV_EPOCODE clampEPO(CGV_EPOCODE v, CGV_EPOCODE a, CGV_EPOCODE b)
{
    if (v < a)
        return a;
    else
    if (v > b)
        return b;
    return v;
}

INLINE CGV_INDEX clampIndex(CGV_INDEX v, CGV_INDEX a, CGV_INDEX b)
{
    if (v < a)
        return a;
    else
    if (v > b)
        return b;
    return v;
}

INLINE CGV_SHIFT32 shift_right_uint32(CGV_SHIFT32 v, CGU_INT bits)
{
   return v>>bits; // (perf warning expected)
}

INLINE CGV_BYTE   shift_right_uint8(CGV_BYTE v,  CGU_UINT8 bits)
{
   return v>>bits; // (perf warning expected)
}

INLINE CGV_BYTE   shift_right_uint8V(CGV_BYTE v,  CGV_UINT8 bits)
{
   return v>>bits; // (perf warning expected)
}

// valid bit range is 0..8
INLINE CGV_EPOCODE expandEPObits(CGV_EPOCODE v, uniform CGV_EPOCODE bits)
{
   CGV_EPOCODE vv = v<<(8-bits);
   return vv + shift_right_uint32(vv, bits);
}

CGV_ERROR err_absf(CGV_ERROR a) { return a>0.0F?a:-a;}
CGV_IMAGE img_absf(CGV_IMAGE a) { return a>0.0F?a:-a;}

CGU_UINT8  min8(CGU_UINT8 a, CGU_UINT8 b) { return a<b?a:b;}
CGU_UINT8  max8(CGU_UINT8 a, CGU_UINT8 b) { return a>b?a:b;}

void pack_index(CGV_INDEXPACKED  packed_index[2], CGV_INDEX   src_index[MAX_SUBSET_SIZE])
{
    // Converts from unpacked index to packed index
    packed_index[0] = 0x0000;
    packed_index[1] = 0x0000;
    CGV_BYTE shift = 0;                // was CGV_UINT8
    for (CGU_INT k=0; k<16; k++)
    {
        packed_index[k/8] |= (CGV_UINT32)(src_index[k]&0x0F) << shift;
        shift +=4;
    }
}

void unpack_index(CGV_INDEX  unpacked_index[MAX_SUBSET_SIZE],CGV_INDEXPACKED  src_packed[2])
{
    // Converts from packed index to unpacked index
    CGV_BYTE shift = 0;    // was CGV_UINT8
    for (CGV_BYTE k=0; k<16; k++)
    {
        unpacked_index[k] = (CGV_BYTE)(src_packed[k/8] >> shift)&0xF;
        if (k == 7)
            shift = 0;
        else
            shift +=4;
    }
}

//====================================== CMP MATH UTILS  ============================================
CGV_ERROR err_Total(
                    CGV_IMAGE       image_src1[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                    CGV_IMAGE       image_src2[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                    CGV_ENTRIES     numEntries,                   // < 16
                    CGU_CHANNEL     channels3or4)                 // IN:  3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{
    CGV_ERROR err_t=0.0F;
    for (CGU_CHANNEL ch=0;ch<channels3or4; ch++) 
       for (CGV_ENTRIES k=0;k<numEntries;k++) 
       {
           err_t = err_t + sq_image(image_src1[k+ch*SOURCE_BLOCK_SIZE]-image_src2[k+ch*SOURCE_BLOCK_SIZE]);
       }
    return err_t;
};

void GetImageCentered(
                    CGV_IMAGE     image_centered_out[SOURCE_BLOCK_SIZE*MAX_CHANNELS], 
                    CGV_IMAGE     mean_out[MAX_CHANNELS],
                    CGV_IMAGE     image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                    CGV_ENTRIES   numEntries,                  // < 16
                    CGU_CHANNEL   channels3or4)                // IN:  3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{
    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
    {
       mean_out[ch]=0.0F;
       if (numEntries > 0)
       {
            for (CGV_ENTRIES k=0;k<numEntries;k++)
            {
                mean_out[ch] = mean_out[ch] + image_src[k+(ch*SOURCE_BLOCK_SIZE)];
            }
            mean_out[ch] /= numEntries;
            for (CGV_ENTRIES k=0;k<numEntries;k++)
                image_centered_out[k+(ch*SOURCE_BLOCK_SIZE)] = image_src[k+(ch*SOURCE_BLOCK_SIZE)] - mean_out[ch];
       }
    }

}

void GetCovarianceVector(
                    CGV_IMAGE      covariance_out[MAX_CHANNELS*MAX_CHANNELS],  // OUT: Covariance vector
                    CGV_IMAGE      image_centered[SOURCE_BLOCK_SIZE*MAX_CHANNELS], 
                    CGV_ENTRIES    numEntries,                       // < 16
                    CGU_CHANNEL    channels3or4)                     // IN:  3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{
    for (CGU_CHANNEL ch1=0; ch1<channels3or4; ch1++)
        for (CGU_CHANNEL ch2=0;ch2<=ch1;ch2++)
        {
            covariance_out[ch1+ch2*4]=0;
            for (CGV_ENTRIES k=0;k<numEntries;k++)
                covariance_out[ch1+ch2*4] += image_centered[k+(ch1*SOURCE_BLOCK_SIZE)]*image_centered[k+(ch2*SOURCE_BLOCK_SIZE)];
        }

    for (CGU_CHANNEL ch1=0; ch1<channels3or4; ch1++)
        for (CGU_CHANNEL ch2=ch1+1;ch2<channels3or4;ch2++)
            covariance_out[ch1+ch2*4] = covariance_out[ch2+ch1*4];
}

void GetProjecedImage(
                    CGV_IMAGE     projection_out[SOURCE_BLOCK_SIZE],  //output projected data
                    CGV_IMAGE     image_centered[SOURCE_BLOCK_SIZE*MAX_CHANNELS], 
                    CGV_ENTRIES   numEntries,               // < 16
                    CGV_IMAGE     EigenVector[MAX_CHANNELS],
                    CGU_CHANNEL   channels3or4)             // 3 = RGB or 4 = RGBA 
{
    projection_out[0] = 0.0F;

    // EigenVector must be normalized
    for (CGV_ENTRIES k=0; k<numEntries; k++)
    {
        projection_out[k]=0.0F;
        for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
        {
             projection_out[k] = projection_out[k] + (image_centered[k+(ch*SOURCE_BLOCK_SIZE)]*EigenVector[ch]);
        }
    }
}


INLINE CGV_UINT8 get_partition_subset(CGV_INT part_id, CGU_INT maxSubsets, CGV_INT index)
{
   CMP_STATIC  uniform CMP_CONSTANT   CGU_UINT32 subset_mask_table[] = {
        // 2 subset region patterns
        0x0000CCCCu, // 0   1100 1100 1100 1100  (MSB..LSB)
        0x00008888u, // 1   1000 1000 1000 1000
        0x0000EEEEu, // 2   1110 1110 1110 1110
        0x0000ECC8u, // 3   1110 1100 1100 1000
        0x0000C880u, // 4   1100 1000 1000 0000
        0x0000FEECu, // 5   1111 1110 1110 1100
        0x0000FEC8u, // 6   1111 1110 1100 1000
        0x0000EC80u, // 7   1110 1100 1000 0000
        0x0000C800u, // 8   1100 1000 0000 0000
        0x0000FFECu, // 9   1111 1111 1110 1100
        0x0000FE80u, // 10  1111 1110 1000 0000
        0x0000E800u, // 11  1110 1000 0000 0000
        0x0000FFE8u, // 12  1111 1111 1110 1000
        0x0000FF00u, // 13  1111 1111 0000 0000
        0x0000FFF0u, // 14  1111 1111 1111 0000
        0x0000F000u, // 15  1111 0000 0000 0000
        0x0000F710u, // 16  1111 0111 0001 0000
        0x0000008Eu, // 17  0000 0000 1000 1110
        0x00007100u, // 18  0111 0001 0000 0000
        0x000008CEu, // 19  0000 1000 1100 1110
        0x0000008Cu, // 20  0000 0000 1000 1100
        0x00007310u, // 21  0111 0011 0001 0000
        0x00003100u, // 22  0011 0001 0000 0000
        0x00008CCEu, // 23  1000 1100 1100 1110
        0x0000088Cu, // 24  0000 1000 1000 1100
        0x00003110u, // 25  0011 0001 0001 0000
        0x00006666u, // 26  0110 0110 0110 0110
        0x0000366Cu, // 27  0011 0110 0110 1100
        0x000017E8u, // 28  0001 0111 1110 1000
        0x00000FF0u, // 29  0000 1111 1111 0000
        0x0000718Eu, // 30  0111 0001 1000 1110
        0x0000399Cu, // 31  0011 1001 1001 1100
        0x0000AAAAu, // 32  1010 1010 1010 1010
        0x0000F0F0u, // 33  1111 0000 1111 0000
        0x00005A5Au, // 34  0101 1010 0101 1010
        0x000033CCu, // 35  0011 0011 1100 1100
        0x00003C3Cu, // 36  0011 1100 0011 1100
        0x000055AAu, // 37  0101 0101 1010 1010
        0x00009696u, // 38  1001 0110 1001 0110
        0x0000A55Au, // 39  1010 0101 0101 1010
        0x000073CEu, // 40  0111 0011 1100 1110
        0x000013C8u, // 41  0001 0011 1100 1000
        0x0000324Cu, // 42  0011 0010 0100 1100
        0x00003BDCu, // 43  0011 1011 1101 1100
        0x00006996u, // 44  0110 1001 1001 0110
        0x0000C33Cu, // 45  1100 0011 0011 1100
        0x00009966u, // 46  1001 1001 0110 0110
        0x00000660u, // 47  0000 0110 0110 0000
        0x00000272u, // 48  0000 0010 0111 0010
        0x000004E4u, // 49  0000 0100 1110 0100
        0x00004E40u, // 50  0100 1110 0100 0000
        0x00002720u, // 51  0010 0111 0010 0000
        0x0000C936u, // 52  1100 1001 0011 0110
        0x0000936Cu, // 53  1001 0011 0110 1100
        0x000039C6u, // 54  0011 1001 1100 0110
        0x0000639Cu, // 55  0110 0011 1001 1100
        0x00009336u, // 56  1001 0011 0011 0110
        0x00009CC6u, // 57  1001 1100 1100 0110
        0x0000817Eu, // 58  1000 0001 0111 1110
        0x0000E718u, // 59  1110 0111 0001 1000
        0x0000CCF0u, // 60  1100 1100 1111 0000
        0x00000FCCu, // 61  0000 1111 1100 1100
        0x00007744u, // 62  0111 0111 0100 0100
        0x0000EE22u, // 63  1110 1110 0010 0010
        // 3 Subset region patterns
        0xF60008CCu,// 0    1111 0110 0000 0000 : 0000 1000 1100 1100 = 2222122011001100 (MSB...LSB)
        0x73008CC8u,// 1    0111 0011 0000 0000 : 1000 1100 1100 1000 = 1222112211001000
        0x3310CC80u,// 2    0011 0011 0001 0000 : 1100 1100 1000 0000 = 1122112210020000
        0x00CEEC00u,// 3    0000 0000 1100 1110 : 1110 1100 0000 0000 = 1110110022002220
        0xCC003300u,// 4    1100 1100 0000 0000 : 0011 0011 0000 0000 = 2211221100000000
        0xCC0000CCu,// 5    1100 1100 0000 0000 : 0000 0000 1100 1100 = 2200220011001100
        0x00CCFF00u,// 6    0000 0000 1100 1100 : 1111 1111 0000 0000 = 1111111122002200
        0x3300CCCCu,// 7    0011 0011 0000 0000 : 1100 1100 1100 1100 = 1122112211001100
        0xF0000F00u,// 8    1111 0000 0000 0000 : 0000 1111 0000 0000 = 2222111100000000
        0xF0000FF0u,// 9    1111 0000 0000 0000 : 0000 1111 1111 0000 = 2222111111110000
        0xFF0000F0u,// 10   1111 1111 0000 0000 : 0000 0000 1111 0000 = 2222222211110000
        0x88884444u,// 11   1000 1000 1000 1000 : 0100 0100 0100 0100 = 2100210021002100
        0x88886666u,// 12   1000 1000 1000 1000 : 0110 0110 0110 0110 = 2110211021102110
        0xCCCC2222u,// 13   1100 1100 1100 1100 : 0010 0010 0010 0010 = 2210221022102210
        0xEC80136Cu,// 14   1110 1100 1000 0000 : 0001 0011 0110 1100 = 2221221121101100
        0x7310008Cu,// 15   0111 0011 0001 0000 : 0000 0000 1000 1100 = 0222002210021100
        0xC80036C8u,// 16   1100 1000 0000 0000 : 0011 0110 1100 1000 = 2211211011001000
        0x310008CEu,// 17   0011 0001 0000 0000 : 0000 1000 1100 1110 = 0022100211001110
        0xCCC03330u,// 18   1100 1100 1100 0000 : 0011 0011 0011 0000 = 2211221122110000
        0x0CCCF000u,// 19   0000 1100 1100 1100 : 1111 0000 0000 0000 = 1111220022002200
        0xEE0000EEu,// 20   1110 1110 0000 0000 : 0000 0000 1110 1110 = 2220222011101110
        0x77008888u,// 21   0111 0111 0000 0000 : 1000 1000 1000 1000 = 1222122210001000
        0xCC0022C0u,// 22   1100 1100 0000 0000 : 0010 0010 1100 0000 = 2210221011000000
        0x33004430u,// 23   0011 0011 0000 0000 : 0100 0100 0011 0000 = 0122012200110000
        0x00CC0C22u,// 24   0000 0000 1100 1100 : 0000 1100 0010 0010 = 0000110022102210
        0xFC880344u,// 25   1111 1100 1000 1000 : 0000 0011 0100 0100 = 2222221121002100
        0x06606996u,// 26   0000 0110 0110 0000 : 0110 1001 1001 0110 = 0110122112210110
        0x66009960u,// 27   0110 0110 0000 0000 : 1001 1001 0110 0000 = 1221122101100000
        0xC88C0330u,// 28   1100 1000 1000 1100 : 0000 0011 0011 0000 = 2200201120112200
        0xF9000066u,// 29   1111 1001 0000 0000 : 0000 0000 0110 0110 = 2222200201100110
        0x0CC0C22Cu,// 30   0000 1100 1100 0000 : 1100 0010 0010 1100 = 1100221022101100
        0x73108C00u,// 31   0111 0011 0001 0000 : 1000 1100 0000 0000 = 1222112200020000
        0xEC801300u,// 32   1110 1100 1000 0000 : 0001 0011 0000 0000 = 2221221120000000
        0x08CEC400u,// 33   0000 1000 1100 1110 : 1100 0100 0000 0000 = 1100210022002220
        0xEC80004Cu,// 34   1110 1100 1000 0000 : 0000 0000 0100 1100 = 2220220021001100
        0x44442222u,// 35   0100 0100 0100 0100 : 0010 0010 0010 0010 = 0210021002100210
        0x0F0000F0u,// 36   0000 1111 0000 0000 : 0000 0000 1111 0000 = 0000222211110000
        0x49242492u,// 37   0100 1001 0010 0100 : 0010 0100 1001 0010 = 0210210210210210
        0x42942942u,// 38   0100 0010 1001 0100 : 0010 1001 0100 0010 = 0210102121020210
        0x0C30C30Cu,// 39   0000 1100 0011 0000 : 1100 0011 0000 1100 = 1100221100221100
        0x03C0C03Cu,// 40   0000 0011 1100 0000 : 1100 0000 0011 1100 = 1100002222111100
        0xFF0000AAu,// 41   1111 1111 0000 0000 : 0000 0000 1010 1010 = 2222222210101010
        0x5500AA00u,// 42   0101 0101 0000 0000 : 1010 1010 0000 0000 = 1212121200000000
        0xCCCC3030u,// 43   1100 1100 1100 1100 : 0011 0000 0011 0000 = 2211220022112200
        0x0C0CC0C0u,// 44   0000 1100 0000 1100 : 1100 0000 1100 0000 = 1100220011002200
        0x66669090u,// 45   0110 0110 0110 0110 : 1001 0000 1001 0000 = 1221022012210220
        0x0FF0A00Au,// 46   0000 1111 1111 0000 : 1010 0000 0000 1010 = 1010222222221010
        0x5550AAA0u,// 47   0101 0101 0101 0000 : 1010 1010 1010 0000 = 1212121212120000
        0xF0000AAAu,// 48   1111 0000 0000 0000 : 0000 1010 1010 1010 = 2222101010101010
        0x0E0EE0E0u,// 49   0000 1110 0000 1110 : 1110 0000 1110 0000 = 1110222011102220
        0x88887070u,// 50   1000 1000 1000 1000 : 0111 0000 0111 0000 = 2111200021112000
        0x99906660u,// 51   1001 1001 1001 0000 : 0110 0110 0110 0000 = 2112211221120000
        0xE00E0EE0u,// 52   1110 0000 0000 1110 : 0000 1110 1110 0000 = 2220111011102220
        0x88880770u,// 53   1000 1000 1000 1000 : 0000 0111 0111 0000 = 2000211121112000
        0xF0000666u,// 54   1111 0000 0000 0000 : 0000 0110 0110 0110 = 2222011001100110
        0x99006600u,// 55   1001 1001 0000 0000 : 0110 0110 0000 0000 = 2112211200000000
        0xFF000066u,// 56   1111 1111 0000 0000 : 0000 0000 0110 0110 = 2222222201100110
        0xC00C0CC0u,// 57   1100 0000 0000 1100 : 0000 1100 1100 0000 = 2200110011002200
        0xCCCC0330u,// 58   1100 1100 1100 1100 : 0000 0011 0011 0000 = 2200221122112200
        0x90006000u,// 59   1001 0000 0000 0000 : 0110 0000 0000 0000 = 2112000000000000
        0x08088080u,// 60   0000 1000 0000 1000 : 1000 0000 1000 0000 = 1000200010002000
        0xEEEE1010u,// 61   1110 1110 1110 1110 : 0001 0000 0001 0000 = 2221222022212220
        0xFFF0000Au,// 62   1111 1111 1111 0000 : 0000 0000 0000 1010 = 2222222222221010
        0x731008CEu,// 63   0111 0011 0001 0000 : 0000 1000 1100 1110 = 0222102211021110
        };

  if (maxSubsets == 2)
  {
      CGV_UINT32 mask_packed = subset_mask_table[part_id];
      return ((mask_packed & (0x01<<index))?1:0);       // This can be moved to caller, just return mask!!
  }

  // 3 region subsets
  part_id += 64;
  CGV_UINT32 mask0 = subset_mask_table[part_id] & 0xFFFF;
  CGV_UINT32 mask1 = subset_mask_table[part_id] >> 16;
  CGV_UINT32 mask =  0x01 << index;

  return ((mask1 & mask)?2:0 + (mask0 & mask)?1:0);       // This can be moved to caller, just return mask!!
}

void GetPartitionSubSet_mode01237(
                    CGV_IMAGE      subsets_out[MAX_SUBSETS][SOURCE_BLOCK_SIZE][MAX_CHANNELS], // OUT: Subset pattern mapped with image src colors
                    CGV_ENTRIES    entryCount_out[MAX_SUBSETS],                             // OUT: Number of entries per subset
                    CGV_UINT8      partition,                                               // Partition Shape 0..63
                    CGV_IMAGE      image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],               // Image colors
                    CGU_INT        blockMode,                                               // [0,1,2,3 or 7]
                    CGU_CHANNEL    channels3or4)                                            // 3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{
    CGU_UINT8   maxSubsets = 2;     if (blockMode == 0 || blockMode == 2)  maxSubsets  = 3;

    entryCount_out[0] = 0;
    entryCount_out[1] = 0;
    entryCount_out[2] = 0;

    for (CGV_INT i = 0; i < MAX_SUBSET_SIZE; i++)
    {
        CGV_UINT8   subset = get_partition_subset(partition,maxSubsets,i);

        for (CGU_INT ch = 0; ch<3; ch++)
            subsets_out[subset][entryCount_out[subset]][ch] = image_src[i+(ch*SOURCE_BLOCK_SIZE)];
            //subsets_out[subset*64+(entryCount_out[subset]*MAX_CHANNELS+ch)] = image_src[i+(ch*SOURCE_BLOCK_SIZE)];

        // if we have only 3 channels then set the alpha subset to 0
        if (channels3or4 == 3)
            subsets_out[subset][entryCount_out[subset]][3] = 0.0F;
        else
            subsets_out[subset][entryCount_out[subset]][3] = image_src[i+(COMP_ALPHA*SOURCE_BLOCK_SIZE)];
        entryCount_out[subset]++;
    }
}

INLINE void  GetClusterMean(
                    CGV_IMAGE       cluster_mean_out[SOURCE_BLOCK_SIZE][MAX_CHANNELS],
                    CGV_IMAGE       image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                    CGV_INDEX       index_in[MAX_SUBSET_SIZE],
                    CGV_ENTRIES     numEntries,                             // < 16
                    CGU_CHANNEL     channels3or4)                           // IN: 3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{
    // unused index values are underfined
    CGV_INDEX       i_cnt[MAX_SUBSET_SIZE];
    CGV_INDEX       i_comp[MAX_SUBSET_SIZE];


    for (CGV_ENTRIES i=0;i< numEntries;i++)
        for (CGU_CHANNEL ch=0; ch< channels3or4; ch++) 
        {
            CGV_INDEX idx = index_in[i]&0x0F;
            cluster_mean_out[idx][ch] = 0;
            i_cnt[idx]=0;
        }

    CGV_INDEX ic = 0; // was CGV_INT
    for (CGV_ENTRIES i=0;i< numEntries;i++)
    {
        CGV_INDEX idx = index_in[i]&0x0F;
        if (i_cnt[idx]==0) 
            i_comp[ic++]=idx;
        i_cnt[idx]++;

        for (CGU_CHANNEL ch=0; ch< channels3or4; ch++) 
        {
            cluster_mean_out[idx][ch] += image_src[i+(ch*SOURCE_BLOCK_SIZE)];
        }
    }

    for (CGU_CHANNEL ch=0; ch< channels3or4; ch++)
    for (CGU_INT i=0;i < ic;i++)
    {
        if (i_cnt[i_comp[i]] != 0)
        {
            CGV_INDEX icmp = i_comp[i];
            cluster_mean_out[icmp][ch] = (CGV_IMAGE) floor( (cluster_mean_out[icmp][ch] / (CGV_IMAGE) i_cnt[icmp]) +0.5F);
        }
    }

}

INLINE void GetImageMean(
                    CGV_IMAGE       image_mean_out[SOURCE_BLOCK_SIZE*MAX_CHANNELS], 
                    CGV_IMAGE       image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS], 
                    CGV_ENTRIES     numEntries, 
                    CGU_CHANNEL     channels) 
{
    for (CGU_CHANNEL ch=0; ch< channels; ch++)
        image_mean_out[ch] =0;

    for (CGV_ENTRIES i=0;i< numEntries;i++)
        for (CGU_CHANNEL ch=0; ch< channels; ch++)
            image_mean_out[ch] += image_src[i+ch*SOURCE_BLOCK_SIZE];

    for (CGU_CHANNEL ch=0; ch< channels; ch++)
        image_mean_out[ch] /=(CGV_IMAGE) numEntries;   // Performance Warning: Conversion from unsigned int to float is slow. Use "int" if possible
}

// calculate an eigen vector corresponding to a biggest eigen value
// will work for non-zero non-negative matricies only
void GetEigenVector(
                    CGV_IMAGE       EigenVector_out[MAX_CHANNELS],                  // Normalized Eigen Vector output
                    CGV_IMAGE       CovarianceVector[MAX_CHANNELS*MAX_CHANNELS],    // Covariance Vector
                    CGU_CHANNEL     channels3or4)                                   // IN: 3 = RGB or 4 = RGBA
{
    CGV_IMAGE vector_covIn[MAX_CHANNELS*MAX_CHANNELS];
    CGV_IMAGE vector_covOut[MAX_CHANNELS*MAX_CHANNELS];
    CGV_IMAGE vector_maxCovariance;

    for (CGU_CHANNEL ch1=0; ch1<channels3or4; ch1++)
        for (CGU_CHANNEL ch2=0; ch2<channels3or4; ch2++)
        {
            vector_covIn[ch1+ch2*4] = CovarianceVector[ch1+ch2*4];
        }

    vector_maxCovariance = 0;

    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
    {
        if (vector_covIn[ch+ch*4] > vector_maxCovariance)
                    vector_maxCovariance = vector_covIn[ch+ch*4];
    }

    // Normalize Input Covariance Vector 
    for (CGU_CHANNEL ch1=0; ch1<channels3or4; ch1++)
        for (CGU_CHANNEL ch2=0; ch2<channels3or4; ch2++)
        {
            if (vector_maxCovariance > 0)
                vector_covIn[ch1+ch2*4] = vector_covIn[ch1+ch2*4] / vector_maxCovariance;
        }

    for (CGU_CHANNEL ch1=0; ch1<channels3or4; ch1++) 
    {
        for (CGU_CHANNEL ch2=0; ch2<channels3or4; ch2++) 
        {
            CGV_IMAGE vector_temp_cov=0;
            for (CGU_CHANNEL ch3=0; ch3<channels3or4; ch3++)
            {
                vector_temp_cov = vector_temp_cov + vector_covIn[ch1+ch3*4]*vector_covIn[ch3+ch2*4];
            }
            vector_covOut[ch1+ch2*4] = vector_temp_cov; 
        }
    }

    vector_maxCovariance = 0;

    CGV_TYPEINT maxCovariance_channel = 0;

    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
    {
         if (vector_covOut[ch+ch*4] > vector_maxCovariance) 
         {
             maxCovariance_channel  = ch;
             vector_maxCovariance    = vector_covOut[ch+ch*4];
         }
    }

    CGV_IMAGE vector_t = 0;

    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
    {
        vector_t = vector_t + vector_covOut[maxCovariance_channel+ch*4]*vector_covOut[maxCovariance_channel+ch*4];
        EigenVector_out[ch] = vector_covOut[maxCovariance_channel+ch*4]; 
    }

    // Normalize the Eigen Vector
    vector_t = static_cast<CGV_IMAGE> (sqrt(vector_t));
    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
    {
        if (vector_t > 0)
            EigenVector_out[ch] = EigenVector_out[ch] / vector_t;
    }

}

CGV_INDEX  index_collapse(
                    CGV_INDEX    index[MAX_SUBSET_SIZE], 
                    CGV_ENTRIES  numEntries)
{
    CGV_INDEX minIndex=index[0];
    CGV_INDEX MaxIndex=index[0];

    for (CGV_ENTRIES k=1;k<numEntries;k++) {
        if (index[k] < minIndex)
                minIndex = index[k];
        if (index[k] > MaxIndex)
                MaxIndex = index[k];
    }

    CGV_INDEX D=1;

    for (CGV_INDEX d=2; d<= MaxIndex-minIndex; d++) 
    {
        for (CGV_ENTRIES ent=0;ent<numEntries;ent++)
        {
            if ((index[ent] -minIndex) % d !=0)
            {
                if (ent>=numEntries) 
                    D =d;
                break;
            }
        }
    }

    for (CGV_ENTRIES k=0;k<numEntries;k++)
    {
        index[k] = (index[k]- minIndex) / D;
    }

    for (CGV_ENTRIES k=1;k<numEntries;k++) {
        if (index[k] > MaxIndex)
                MaxIndex = index[k];
    }

    return (MaxIndex);

}

void sortProjected_indexs(
                    CGV_INDEX   index_ordered[MAX_SUBSET_SIZE], 
                    CGV_IMAGE   projection[SOURCE_BLOCK_SIZE],
                    CGV_ENTRIES numEntries                   // max 16
                    ) 
{
    CMP_di what[SOURCE_BLOCK_SIZE];

    for (CGV_INDEX i=0; i < numEntries;i++) 
    {
        what[i].index = i;
        what[i].image  = projection[i];
    }

    CGV_INDEX   tmp_index;
    CGV_IMAGE   tmp_image;

    for (CGV_ENTRIES i = 1; i < numEntries; i++) 
    {
        for (CGV_ENTRIES j=i; j>0; j--)
        {
            if (what[j - 1].image > what[j].image)
            {
                tmp_index = what[j].index;
                tmp_image = what[j].image;
                what[j].index = what[j - 1].index;
                what[j].image  = what[j - 1].image;
                what[j - 1].index  = tmp_index;
                what[j - 1].image  = tmp_image;
            }
        }
    }

    for (CGV_ENTRIES i=0; i < numEntries;i++) 
        index_ordered[i]=what[i].index;

};

void sortPartitionProjection(
                    CGV_IMAGE  projection[MAX_PARTITION_ENTRIES],
                    CGV_UINT8  order[MAX_PARTITION_ENTRIES], 
                    CGU_UINT8  numPartitions       // max 64
                    ) 
{
    CMP_du what[MAX_PARTITION_ENTRIES];
         
    for (CGU_UINT8 Parti=0; Parti < numPartitions;Parti++) 
    {
        what[Parti].index  = Parti;
        what[Parti].image  = projection[Parti];
    }

    CGV_UINT8   index;
    CGV_IMAGE   data;

    for (CGU_UINT8 Parti = 1; Parti < numPartitions; Parti++) 
    {
        for (CGU_UINT8 Partj=Parti; Partj>0; Partj--)
        {
            if (what[Partj - 1].image > what[Partj].image)
            {
                index = what[Partj].index;
                data  = what[Partj].image;
                what[Partj].index = what[Partj - 1].index;
                what[Partj].image = what[Partj - 1].image;
                what[Partj - 1].index = index;
                what[Partj - 1].image  = data;
            }
        }
    }

    for (CGU_UINT8 Parti=0; Parti < numPartitions;Parti++) 
        order[Parti]=what[Parti].index;

};


void cmp_Write8Bit(
                    CGV_CMPOUT          base[],
                    CGU_INT* uniform    offset, 
                    CGU_INT             bits,
                    CGV_BYTE            bitVal)
{
    base[*offset/8] |= bitVal << (*offset%8);
    if (*offset%8+bits>8)
    {
      base[*offset/8+1] |= shift_right_uint8(bitVal, 8-*offset%8);
    }
    *offset += bits;
}

void cmp_Write8BitV(
                    CGV_CMPOUT          base[],
                    CGV_INT             offset, 
                    CGU_INT             bits,
                    CGV_BYTE            bitVal)
{
    base[offset/8] |= bitVal << (offset%8);
    if (offset%8+bits>8)
    {
      base[offset/8+1] |= shift_right_uint8V(bitVal, 8-offset%8);
    }
}

INLINE CGV_EPOCODE ep_find_floor( 
                    CGV_IMAGE v, 
                    CGU_UINT8 bits, 
                    CGV_BYTE use_par, 
                    CGV_BYTE odd)
 {
     CGV_EPOCODE i1=0;
     CGV_EPOCODE i2=1<<(bits-use_par);
     odd = use_par ? odd : 0; 
     while (i2-i1>1) 
     {
         CGV_EPOCODE j = (i1+i2)/2;             // Warning in ASMP code
         CGV_EPOCODE ep_d = expandEPObits((j<<use_par)+odd,bits);
         if (v >= ep_d )
             i1=j;
         else
             i2=j;
     }
     
     return (i1<<use_par)+odd;
 }


//==========================================================

// Not used for Modes 4&5
 INLINE CGV_IMAGE GetRamp(
                    CGU_INT         clogBC7,      // ramp bits Valid range 2..4
                    CGU_INT         bits,      // Component Valid range 5..8
                    CGV_EPOCODE     p1,        // 0..255
                    CGV_EPOCODE     p2,        // 0..255
                    CGV_INDEX       index)     // 0..15
{
#ifdef ASPM_GPU // GPU Code 
    CGV_FLOAT rampf = 0.0F;
    CMP_CONSTANT CGV_EPOCODE rampI[5*SOURCE_BLOCK_SIZE] = {
    0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 , // 0 bit index
    0 ,64,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 , // 1 bit index
    0 ,21,43,64,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 , // 2 bit index
    0 ,9 ,18,27,37,46,55,64,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 , // 3 bit index
    0 ,4 ,9 ,13,17,21,26,30,34,38,43,47,51,55,60,64  // 4 bit index
    };

    CGV_EPOCODE e1 = expand_epocode(p1, bits);
    CGV_EPOCODE e2 = expand_epocode(p2,bits);
    CGV_FLOAT ramp = gather_epocode(rampI,clogBC7*16+index)/64.0F;
    rampf = floor(e1 + ramp * (e2 - e1) + 0.5F);
    return rampf;
#else // CPU ASPM Code
#ifdef USE_BC7_RAMP
      return BC7EncodeRamps.ramp[(CLT(clogBC7)*4*256*256*16)+(BTT(bits)*256*256*16)+(p1*256*16)+(p2*16)+index];
#else
      return (CGV_IMAGE)floor((CGV_IMAGE)BC7EncodeRamps.ep_d[BTT(bits)][p1] + rampWeights[clogBC7][index] * (CGV_IMAGE)((BC7EncodeRamps.ep_d[BTT(bits)][p2] - BC7EncodeRamps.ep_d[BTT(bits)][p1]))+ 0.5F);
#endif
#endif
}


 // Not used for Modes 4&5
 INLINE CGV_ERROR get_sperr(
                    CGU_INT         clogBC7,      // ramp bits Valid range 2..4
                    CGU_INT         bits,      // Component Valid range 5..8
                    CGV_EPOCODE     p1,        // 0..255
                    CGU_INT         t1,
                    CGU_INT         t2,
                    CGV_INDEX       index)
{
#ifdef ASPM_GPU
     return 0.0F;
#else
#ifdef USE_BC7_SP_ERR_IDX
      if (BC7EncodeRamps.ramp_init)
          return  BC7EncodeRamps.sp_err[(CLT(clogBC7)*4*256*2*2*16)+(BTT(bits)*256*2*2*16)+(p1*2*2*16)+(t1*2*16)+(t2*16)+index];
      else
          return 0.0F;
#else
     return 0.0F;
#endif
#endif
}

 INLINE void get_fixuptable(CGV_FIXUPINDEX  fixup[3], CGV_PARTID  part_id)
{
   // same as  CMP SDK v3.1 BC7_FIXUPINDEX1 &  BC7_FIXUPINDEX2 for each partition range 0..63
   // The data is saved as a packed INT = (BC7_FIXUPINDEX1 << 4 + BC7_FIXUPINDEX2)
   CMP_STATIC uniform __constant  CGV_FIXUPINDEX  FIXUPINDEX[] = {
       // 2 subset partitions 0..63
        0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u,
        0xf0u, 0x20u, 0x80u, 0x20u, 0x20u, 0x80u, 0x80u, 0xf0u, 0x20u, 0x80u, 0x20u, 0x20u, 0x80u, 0x80u, 0x20u, 0x20u,
        0xf0u, 0xf0u, 0x60u, 0x80u, 0x20u, 0x80u, 0xf0u, 0xf0u, 0x20u, 0x80u, 0x20u, 0x20u, 0x20u, 0xf0u, 0xf0u, 0x60u,
        0x60u, 0x20u, 0x60u, 0x80u, 0xf0u, 0xf0u, 0x20u, 0x20u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0x20u, 0x20u, 0xf0u,
        // 3 subset partitions 64..128
        0x3fu, 0x38u, 0xf8u, 0xf3u, 0x8fu, 0x3fu, 0xf3u, 0xf8u, 0x8fu, 0x8fu, 0x6fu, 0x6fu, 0x6fu, 0x5fu, 0x3fu, 0x38u,
        0x3fu, 0x38u, 0x8fu, 0xf3u, 0x3fu, 0x38u, 0x6fu, 0xa8u, 0x53u, 0x8fu, 0x86u, 0x6au, 0x8fu, 0x5fu, 0xfau, 0xf8u,
        0x8fu, 0xf3u, 0x3fu, 0x5au, 0x6au, 0xa8u, 0x89u, 0xfau, 0xf6u, 0x3fu, 0xf8u, 0x5fu, 0xf3u, 0xf6u, 0xf6u, 0xf8u,
        0x3fu, 0xf3u, 0x5fu, 0x5fu, 0x5fu, 0x8fu, 0x5fu, 0xafu, 0x5fu, 0xafu, 0x8fu, 0xdfu, 0xf3u, 0xcfu, 0x3fu, 0x38u 
   };

   CGV_FIXUPINDEX skip_packed = FIXUPINDEX[part_id];// gather_int2(FIXUPINDEX, part_id);
   fixup[0] = 0;
   fixup[1] = skip_packed>>4;
   fixup[2] = skip_packed&15;
}

//===================================== COMPRESS CODE =============================================
INLINE void SetDefaultIndex(CGV_INDEX  index_io[MAX_SUBSET_SIZE])
{
    // Use this a final call
    for (CGU_INT i=0; i<MAX_SUBSET_SIZE; i++)
        index_io[i] = 0;
}

INLINE void SetDefaultEPOCode(CGV_EPOCODE    epo_code_io[8], CGV_EPOCODE R,CGV_EPOCODE G,CGV_EPOCODE B,CGV_EPOCODE A)
{
    epo_code_io[0] = R;
    epo_code_io[1] = G;
    epo_code_io[2] = B;
    epo_code_io[3] = A;
    epo_code_io[4] = R;
    epo_code_io[5] = G;
    epo_code_io[6] = B;
    epo_code_io[7] = A;
}


void  GetProjectedIndex(
                    CGV_INDEX       projected_index_out[MAX_SUBSET_SIZE],   //output: index, uncentered, in the range 0..clusters-1
                    CGV_IMAGE       image_projected[SOURCE_BLOCK_SIZE], // image_block points, might be uncentered
                    CGV_INT         clusters,                       // clusters: number of points in the ramp   (max 16)
                    CGV_ENTRIES     numEntries)                     // n - number of points in v_  max 15
{ 
    CMP_di      what[SOURCE_BLOCK_SIZE];
    CGV_IMAGE   image_v[SOURCE_BLOCK_SIZE];
    CGV_IMAGE   image_z[SOURCE_BLOCK_SIZE];
    CGV_IMAGE   image_l;
    CGV_IMAGE   image_mm;
    CGV_IMAGE   image_r   = 0.0F;
    CGV_IMAGE   image_dm  = 0.0F;
    CGV_IMAGE   image_min;
    CGV_IMAGE   image_max;
    CGV_IMAGE   image_s;

    SetDefaultIndex(projected_index_out);

    image_min=image_projected[0];
    image_max=image_projected[0]; 

    for (CGV_ENTRIES i=1; i < numEntries;i++) 
    {
        if (image_min < image_projected[i])
            image_min = image_projected[i];
        if (image_max > image_projected[i])
            image_max = image_projected[i];
    }

    CGV_IMAGE img_diff = image_max-image_min;

    if (img_diff == 0.0f) return;
    // if (std::isnan(img_diff)) return;

    image_s = (clusters-1)/img_diff;

    for (CGV_INDEX i=0; i < numEntries;i++) 
    {

        image_v[i] = image_projected[i]*image_s;
        image_z[i] = static_cast<CGV_IMAGE> (floor(image_v[i] + 0.5F - image_min * image_s));
        projected_index_out[i] = (CGV_INDEX)image_z[i];

        what[i].image = image_v[i]-image_z[i]- image_min *image_s;
        what[i].index = i; 
        image_dm+= what[i].image;
        image_r += what[i].image*what[i].image;
    }

    if (numEntries*image_r- image_dm*image_dm >= (CGV_IMAGE)(numEntries-1)/8)
    { 

        image_dm /= numEntries;

        for (CGV_INT i=0; i < numEntries;i++) 
            what[i].image -= image_dm;

        CGV_INDEX tmp_index;
        CGV_IMAGE tmp_image;
        for (CGV_ENTRIES i = 1; i < numEntries; i++) 
        {
            for (CGV_ENTRIES j=i; j>0; j--)
            {
                if (what[j - 1].image > what[j].image) 
                {
                    tmp_index = what[j].index;
                    tmp_image = what[j].image;
                    what[j].index  = what[j - 1].index;
                    what[j].image  = what[j - 1].image;
                    what[j - 1].index  = tmp_index;
                    what[j - 1].image  = tmp_image;
                }
            }
        }

        // got into fundamental simplex
        // move coordinate system origin to its center

        // i=0 < numEntries avoids varying int division by 0
        for (CGV_ENTRIES i=0; i < numEntries;i++) 
        {
                what[i].image =  what[i].image  - (CGV_IMAGE) (((2.0f*i+1)-numEntries)/(2.0f*numEntries));
        }

        image_mm=0.0F;
        image_l=0.0F;

        CGV_INT j = -1;
        for (CGV_ENTRIES i=0; i < numEntries;i++) 
        {
            image_l += what[i].image;
            if (image_l < image_mm) 
            {
                image_mm = image_l;
                j=i;
            }
        }


        j = j + 1;
        // avoid  j = j%numEntries us this
        while (j > numEntries) j = j - numEntries;

        for (CGV_ENTRIES i=j; i < numEntries;i++) 
        {
            CGV_INDEX idx  = what[i].index;
            CGV_INDEX pidx = projected_index_out[idx] + 1;  //gather_index(projected_index_out,idx)+1;
            projected_index_out[idx] = pidx;                 // scatter_index(projected_index_out,idx,pidx);
        }
    }

    // get minimum index 
    CGV_INDEX   index_min=projected_index_out[0];
    for (CGV_ENTRIES i=1; i < numEntries;i++) 
    {
        if (projected_index_out[i] < index_min)
            index_min = projected_index_out[i];
    }

    // reposition all index by min index (using min index as 0)
    for (CGV_ENTRIES i=0; i < numEntries;i++)
    {
        projected_index_out[i] = clampIndex(projected_index_out[i] - index_min,0,15);
    }

}

CGV_ERROR   GetQuantizeIndex(
                    CGV_INDEXPACKED index_packed_out[2],
                    CGV_INDEX       index_out[MAX_SUBSET_SIZE],                   // OUT:
                    CGV_IMAGE       image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                    CGV_ENTRIES     numEntries,                                   //IN: range 0..15 (MAX_SUBSET_SIZE)
                    CGU_INT         numClusters,
                    CGU_CHANNEL     channels3or4)                                 // IN: 3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{
    CGV_IMAGE image_centered[SOURCE_BLOCK_SIZE*MAX_CHANNELS];
    CGV_IMAGE image_mean[MAX_CHANNELS];
    CGV_IMAGE eigen_vector[MAX_CHANNELS];
    CGV_IMAGE covariance_vector[MAX_CHANNELS*MAX_CHANNELS];

    GetImageCentered(image_centered,image_mean, image_src, numEntries, channels3or4);
    GetCovarianceVector(covariance_vector, image_centered, numEntries, channels3or4);

    //-----------------------------------------------------
    // check if all covariances are the same 
    // if so then set all index to same value 0 and return
    // use EPSILON to set the limit for all same limit
    //-----------------------------------------------------

    CGV_IMAGE image_covt=0.0F;
    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
         image_covt = image_covt + covariance_vector[ch+ch*4];
    
    if (image_covt < EPSILON) 
    {
        SetDefaultIndex(index_out);
        index_packed_out[0] = 0;
        index_packed_out[1] = 0;
        return 0.;
    }

    GetEigenVector(eigen_vector, covariance_vector,channels3or4);

    CGV_IMAGE image_projected[SOURCE_BLOCK_SIZE];

    GetProjecedImage(image_projected,image_centered, numEntries, eigen_vector, channels3or4);
    GetProjectedIndex(index_out, image_projected,  numClusters,numEntries);

    //==========================================
    // Refine 
    //==========================================
     CGV_IMAGE image_q  = 0.0F;
    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
    {
        eigen_vector[ch]=0;
        for (CGV_ENTRIES k=0;k<numEntries;k++) 
            eigen_vector[ch] =  eigen_vector[ch] + image_centered[k+(ch*SOURCE_BLOCK_SIZE)]*index_out[k];
        image_q = image_q + eigen_vector[ch]* eigen_vector[ch];
    }

    image_q = static_cast<CGV_IMAGE> (sqrt(image_q));

    // direction needs to be normalized
    if (image_q != 0.0F)
        for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
            eigen_vector[ch] = eigen_vector[ch] / image_q;

    // Get new projected data
    GetProjecedImage(image_projected, image_centered, numEntries, eigen_vector, channels3or4);
    GetProjectedIndex(index_out, image_projected,  numClusters,numEntries);

    // pack the index for use in icmp
    pack_index(index_packed_out, index_out);

    //===========================
    // Calc Error
    //===========================
    // Get the new image based on new index

    CGV_IMAGE image_t  = 0.0F;
    CGV_IMAGE index_average = 0.0F;

    for (CGV_ENTRIES ik=0;ik<numEntries;ik++)
    { 
        index_average   = index_average + index_out[ik];
        image_t         = image_t + index_out[ik]*index_out[ik]; 
    }

    index_average = index_average / (CGV_IMAGE) numEntries; 
    image_t = image_t - index_average * index_average * (CGV_IMAGE) numEntries;

    if (image_t != 0.0F)
        image_t =  1.0F/image_t;

    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
    {
        eigen_vector[ch]=0;
        for (CGV_ENTRIES nk=0; nk<numEntries; nk++) 
            eigen_vector[ch] = eigen_vector[ch] + image_centered[nk+(ch*SOURCE_BLOCK_SIZE)]*index_out[nk];
    }

    CGV_IMAGE image_decomp[SOURCE_BLOCK_SIZE*MAX_CHANNELS];
    for (CGV_ENTRIES i=0;i<numEntries;i++) 
            for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
                image_decomp[i+(ch*SOURCE_BLOCK_SIZE)] = image_mean[ch] + eigen_vector[ch]*image_t*(index_out[i]-index_average); 

    CGV_ERROR err_1 = err_Total(image_src,image_decomp,numEntries, channels3or4);

    return err_1;
//    return 0.0F;
}

CGV_ERROR   quant_solid_color(
                    CGV_INDEX      index_out[MAX_SUBSET_SIZE],
                    CGV_EPOCODE    epo_code_out[2*MAX_CHANNELS],
                    CGV_IMAGE      image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS], 
                    CGV_ENTRIES    numEntries, 
                    CGU_UINT8      Mi_,                // last cluster
                    CGU_UINT8      bits[3],            // including parity
                    CGU_INT        type, 
                    CGU_CHANNEL    channels3or4                       // IN: 3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
                    ) 
{

    CGU_INT    clogBC7 = 0;
    CGU_INT    iv = Mi_ + 1;
    while (iv >>= 1)
        clogBC7++;

    // init epo_0
    CGV_EPOCODE  epo_0[2*MAX_CHANNELS];
    SetDefaultEPOCode(epo_0,0xFF,0,0,0);

    CGV_INDEX  image_log = 0;
    CGV_INDEX  image_idx = 0;
    CGU_BOOL   use_par  = FALSE;
    if (type != 0)
        use_par = TRUE;
    CGV_ERROR  error_1 = CMP_FLOAT_MAX;

    for (CGU_INT pn = 0; pn<npv_nd[channels3or4-3][type] && (error_1 != 0.0F);  pn++)
    { //1

        CGU_INT o1[2*MAX_CHANNELS]; // = { 0,2 };
        CGU_INT o2[2*MAX_CHANNELS]; // = { 0,2 };

        for (CGU_CHANNEL ch = 0; ch<channels3or4; ch++)
        { //A
            o2[  ch] = o1[  ch] = 0;
            o2[4+ch] = o1[4+ch] = 2;

            if (use_par == TRUE)
            {
                if (par_vectors_nd[channels3or4-3][type][pn][0][ch])
                    o1[ch] = 1;
                else
                    o1[4+ch] = 1;
                if (par_vectors_nd[channels3or4-3][type][pn][1][ch])
                    o2[ch] = 1; 
                else
                    o2[4+ch] = 1;
            }
        } //A

        CGV_EPOCODE image_tcr[MAX_CHANNELS];
        CGV_EPOCODE epo_dr_0[MAX_CHANNELS];
        CGV_ERROR   error_tr;
        CGV_ERROR   error_0 = CMP_FLOAT_MAX;

        for (CGV_INDEX iclogBC7 = 0; iclogBC7< (1 << clogBC7) && (error_0 != 0); iclogBC7++)
        { //E
            CGV_ERROR       error_t = 0;
            CGV_EPOCODE     t1o[MAX_CHANNELS], t2o[MAX_CHANNELS];

            for (CGU_CHANNEL ch1 = 0; ch1<channels3or4; ch1++)
            { // D
                CGV_ERROR error_ta = CMP_FLOAT_MAX;

                for (CGU_INT t1 = o1[ch1]; t1<o1[4+ch1]; t1++)
                { // C
                    // This is needed for non-integer mean points of "collapsed" sets
                    for (CGU_INT t2 = o2[ch1]; t2<o2[4+ch1]; t2++)
                    { // B
                        CGV_EPOCODE  image_tf; 
                        CGV_EPOCODE  image_tc; 
                        image_tf = (CGV_EPOCODE)floor(image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)]);
                        image_tc = (CGV_EPOCODE) ceil(image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)]);

#ifdef USE_BC7_SP_ERR_IDX
                        CGV_ERROR err_tf = get_sperr(clogBC7,bits[ch1],image_tf,t1,t2,iclogBC7);
                        CGV_ERROR err_tc = get_sperr(clogBC7,bits[ch1],image_tc,t1,t2,iclogBC7);
                        if (err_tf > err_tc)
                            image_tcr[ch1] = image_tc;
                        else if (err_tf < err_tc)
                            image_tcr[ch1] = image_tf;
                        else
                            image_tcr[ch1] = (CGV_EPOCODE)floor(image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)] + 0.5F);

                        //image_tcr[ch1] = image_tf + (image_tc - image_tf)/2; 

                        //===============================
                        // Refine this for better quality!
                        //===============================
                        error_tr = get_sperr(clogBC7,bits[ch1],image_tcr[ch1],t1,t2,iclogBC7);
                        error_tr = (error_tr*error_tr) 
                                   + 2 * error_tr 
                                   * img_absf(image_tcr[ch1]- image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)]) 
                                   + (image_tcr[ch1] - image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)]) 
                                   * (image_tcr[ch1] - image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)]);

                        if (error_tr < error_ta)
                        {
                            error_ta = error_tr;
                            t1o[ch1] = t1;
                            t2o[ch1] = t2;
                            epo_dr_0[ch1] = clampEPO(image_tcr[ch1],0,255);
                         }
#else
                      image_tcr[ch1] = floor(image_src[COMP_RED+(ch1*SOURCE_BLOCK_SIZE)] + 0.5F);
                      error_ta      = 0;
                      t1o[ch1] = t1;
                      t2o[ch1] = t2;
                      epo_dr_0[ch1] = clampi(image_tcr[ch1],0,255);
#endif
                  } // B
              } //C

              error_t += error_ta;
            } // D

            if (error_t < error_0)
            {
                image_log = iclogBC7;
                image_idx = image_log;
                CGU_BOOL srcIsWhite = FALSE;
                if ((image_src[0] == 255.0f)&&(image_src[1] == 255.0f)&&(image_src[2] == 255.0f)) srcIsWhite = TRUE;

                for (CGU_CHANNEL ch = 0; ch<channels3or4; ch++)
                {
#ifdef ASPM_GPU
                    if (srcIsWhite == TRUE)
                    {
                        // Default White block!
                        epo_0[  ch] = 0x7F;
                        epo_0[4+ch] = 0x7F;
                    }
                    else
                    {
                        // Default black block!
                        epo_0[  ch] = 0;
                        epo_0[4+ch] = 0;
                    }
#else
#ifdef USE_BC7_SP_ERR_IDX
                    if (BC7EncodeRamps.ramp_init) {
                        CGV_EPOCODE index = (CLT(clogBC7)*4*256*2*2*16*2)+(BTT(bits[ch])*256*2*2*16*2)+(epo_dr_0[ch]*2*2*16*2)+(t1o[ch]*2*16*2)+(t2o[ch]*16*2)+(iclogBC7*2);
                        epo_0[  ch] = BC7EncodeRamps.sp_idx[index+0]&0xFF;// gather_epocode(u_BC7Encode->sp_idx,index+0)&0xFF;
                        epo_0[4+ch] = BC7EncodeRamps.sp_idx[index+1]&0xFF;// gather_epocode(u_BC7Encode->sp_idx,index+1)&0xFF;
                    }
                    else {
                        epo_0[ch] = 0;
                        epo_0[4 + ch] = 0;
                    }
#else
                    epo_0[  ch] = 0;
                    epo_0[4+ch] = 0;
#endif
#endif
                }
                error_0 = error_t;
            }
            //if (error_0 == 0)
            //    break;
        } // E

        if (error_0 < error_1)
        {

            image_idx = image_log;
            for (CGU_CHANNEL chE = 0; chE<channels3or4; chE++)
            {
                epo_code_out[chE]   = epo_0[chE];
                epo_code_out[4+chE] = epo_0[4+chE];
            }
            error_1 = error_0;
        }

    } //1

    // Get Image error
    CGV_IMAGE  image_decomp[SOURCE_BLOCK_SIZE*MAX_CHANNELS];
    for (CGV_ENTRIES i = 0; i< numEntries; i++)
    {
        index_out[i] = image_idx;
        for (CGU_CHANNEL ch = 0; ch<channels3or4; ch++)
        {
            image_decomp[i+(ch*SOURCE_BLOCK_SIZE)] = GetRamp(clogBC7,bits[ch],epo_code_out[ch],epo_code_out[4+ch],image_idx);
        }
    }
    // Do we need to do this rather then err_1 * numEntries
    CGV_ERROR error_quant;
    error_quant = err_Total(image_src, image_decomp, numEntries, channels3or4);

    return error_quant;
    //return err_1 * numEntries;
}

CGV_ERROR requantized_image_err(
                        CGV_INDEX      index_out[MAX_SUBSET_SIZE],
                        CGV_EPOCODE    epo_code[2*MAX_CHANNELS],
                        CGU_INT        clogBC7,
                        CGU_UINT8      max_bits[MAX_CHANNELS],
                        CGV_IMAGE      image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                        CGV_ENTRIES    numEntries,         // max 16
                        CGU_CHANNEL    channels3or4)       // IN: 3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
{

        //=========================================
        // requantized image based on new epo_code
        //=========================================
        CGV_IMAGE   image_requantize[SOURCE_BLOCK_SIZE][MAX_CHANNELS];
        CGV_ERROR   err_r=0.0F;

        for (CGU_CHANNEL ch = 0; ch<channels3or4; ch++)
        {
            for (CGU_INT k = 0; k<SOURCE_BLOCK_SIZE; k++)
            {
                  image_requantize[k][ch] = GetRamp(clogBC7,max_bits[ch],epo_code[ch],epo_code[4+ch],(CGV_INDEX)k);
            }
        }

        //=========================================
        // Calc the error for the requantized image
        //=========================================

        for (CGV_ENTRIES k =0; k < numEntries; k++)
        {
            CGV_ERROR     err_cmin     = CMP_FLOAT_MAX;
            CGV_TYPEINT       hold_index_j = 0;

            for (CGV_TYPEINT iclogBC7=0; iclogBC7 < (1<<clogBC7); iclogBC7++)
            {
                CGV_IMAGE image_err = 0.0F;

                for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
                {
                    image_err+= sq_image(image_requantize[iclogBC7][ch]-image_src[k+(ch*SOURCE_BLOCK_SIZE)]);
                }

                if(image_err < err_cmin)
                {
                    err_cmin     = image_err;
                    hold_index_j = iclogBC7;
                }
            }

            index_out[k]=(CGV_INDEX)hold_index_j;
            err_r    +=err_cmin;
        }

        return err_r;
}

CGU_BOOL get_ideal_cluster(
                        CGV_IMAGE      image_out[2*MAX_CHANNELS],
                        CGV_INDEX      index_in[MAX_SUBSET_SIZE],
                        CGU_INT        Mi_,
                        CGV_IMAGE      image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                        CGV_ENTRIES    numEntries,
                        CGU_CHANNEL    channels3or4 )
{
            // get ideal cluster centers
            CGV_IMAGE   image_cluster_mean[SOURCE_BLOCK_SIZE][MAX_CHANNELS];
            GetClusterMean(image_cluster_mean, image_src, index_in, numEntries, channels3or4); // unrounded

            CGV_IMAGE image_matrix0[2] = {0,0};   // matrix /inverse matrix
            CGV_IMAGE image_matrix1[2] = {0,0};   // matrix /inverse matrix
            CGV_IMAGE image_rp[2*MAX_CHANNELS];            // right part for RMS fit problem

            for (CGU_INT i=0; i<2*MAX_CHANNELS; i++) image_rp[i]=0;

            // weight with cnt if runnning on compacted index
            for (CGV_ENTRIES k=0;k<numEntries;k++)
            {
                    image_matrix0[0] += (Mi_- index_in[k])* (Mi_-index_in[k]);
                    image_matrix0[1] +=       index_in[k] * (Mi_-index_in[k]);           // im is symmetric
                    image_matrix1[1] +=       index_in[k] *      index_in[k];

                    for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
                    {
                        image_rp[  ch] += (Mi_-index_in[k]) * image_cluster_mean[index_in[k]][ch];
                        image_rp[4+ch] +=      index_in[k]  * image_cluster_mean[index_in[k]][ch];
                    }
            }

            CGV_IMAGE matrix_dd = image_matrix0[0]*image_matrix1[1]- image_matrix0[1]*image_matrix0[1];

            // assert(matrix_dd !=0);
            // matrix_dd=0 means that index_cidx[k] and (Mi_-index_cidx[k]) collinear which implies only one active index;
            // taken care of separately
            if (matrix_dd == 0)
            {
                for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
                {
                    image_out[  ch]=0;
                    image_out[4+ch]=0;
                }
                return FALSE;
            }
            image_matrix1[0] = image_matrix0[0];
            image_matrix0[0] = image_matrix1[1]/matrix_dd;
            image_matrix1[1] = image_matrix1[0]/matrix_dd;
            image_matrix1[0] = image_matrix0[1]=-image_matrix0[1]/matrix_dd;

            CGV_IMAGE     Mif = (CGV_IMAGE)Mi_;

            for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
            {
                image_out[  ch]=(image_matrix0[0]*image_rp[ch]+image_matrix0[1]*image_rp[4+ch])*Mif;
                image_out[4+ch]=(image_matrix1[0]*image_rp[ch]+image_matrix1[1]*image_rp[4+ch])*Mif;
            }

   return TRUE;
}

CGV_ERROR shake(
        CGV_EPOCODE epo_code_shaker_out[2*MAX_CHANNELS],
        CGV_IMAGE   image_ep[2*MAX_CHANNELS],
        CGV_INDEX   index_cidx[MAX_SUBSET_SIZE],
        CGV_IMAGE   image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
        CGU_INT     clogBC7,
        CGU_INT     type,
        CGU_UINT8   max_bits[MAX_CHANNELS],
        CGU_UINT8   use_par,
        CGV_ENTRIES numEntries,         // max 16
        CGU_CHANNEL channels3or4 )
{
#define SHAKESIZE1 1
#define SHAKESIZE2 2
             // shake single or                                   - cartesian
             // shake odd/odd and even/even or                    - same parity
             // shake odd/odd odd/even , even/odd and even/even   - bcc

             CGV_ERROR    best_err = CMP_FLOAT_MAX;

             CGV_ERROR    err_ed[16] = {0};
             CGV_EPOCODE  epo_code_par[2][2][2][MAX_CHANNELS];

             for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
             {
                 CGU_UINT8         ppA = 0;
                 CGU_UINT8         ppB = 0;
                 CGU_UINT8         rr = (use_par ? 2:1);
                 CGV_EPOCODE     epo_code_epi[2][2];  // first/second, coord, begin rage end range

                 for (ppA=0; ppA<rr; ppA++) // loop max =2
                 {
                     for (ppB=0; ppB<rr; ppB++) //loop  max =2
                     {

                         // set default ranges
                         epo_code_epi[0][0] = epo_code_epi[0][1]= ep_find_floor( image_ep[  ch],max_bits[ch], use_par, ppA);
                         epo_code_epi[1][0] = epo_code_epi[1][1]= ep_find_floor( image_ep[4+ch],max_bits[ch], use_par, ppB);

                         // set begin range 
                         epo_code_epi[0][0] -= ( (epo_code_epi[0][0]  < SHAKESIZE1 ? epo_code_epi[0][0]:SHAKESIZE1))&(~use_par);
                         epo_code_epi[1][0] -= ( (epo_code_epi[1][0]  < SHAKESIZE1 ? epo_code_epi[1][0]:SHAKESIZE1))&(~use_par);

                         // set end range
                         epo_code_epi[0][1] += ((1<<max_bits[ch])-1 - epo_code_epi[0][1]  < SHAKESIZE2 ? (1<<max_bits[ch])-1-epo_code_epi[0][1]:SHAKESIZE2)&(~use_par);
                         epo_code_epi[1][1] += ((1<<max_bits[ch])-1 - epo_code_epi[1][1]  < SHAKESIZE2 ? (1<<max_bits[ch])-1-epo_code_epi[1][1]:SHAKESIZE2)&(~use_par);

                         CGV_EPOCODE step = (1<<use_par);
                         err_ed[(ppA*8)+(ppB*4)+ch]=CMP_FLOAT_MAX;

                         for (CGV_EPOCODE epo_p1=epo_code_epi[0][0]; epo_p1<=epo_code_epi[0][1]; epo_p1+=step) 
                         for (CGV_EPOCODE epo_p2=epo_code_epi[1][0]; epo_p2<=epo_code_epi[1][1]; epo_p2+=step)
                         {
                                 CGV_IMAGE      image_square_diff =0.0F;
                                 CGV_ENTRIES    _mc = numEntries;
                                 CGV_IMAGE      image_ramp;
 
                                 while(_mc > 0)
                                 {
                                     image_ramp = GetRamp(clogBC7,max_bits[ch],epo_p1,epo_p2,index_cidx[_mc-1]);
 
                                     image_square_diff += sq_image(image_ramp-image_src[(_mc-1)+(ch*SOURCE_BLOCK_SIZE)]);
                                     _mc--;
                                 }
                                 if (image_square_diff < err_ed[(ppA*8)+(ppB*4)+ch]) 
                                 {
                                     err_ed[(ppA*8)+(ppB*4)+ch] = image_square_diff;
                                     epo_code_par[ppA][ppB][0][ch] = epo_p1;
                                     epo_code_par[ppA][ppB][1][ch] = epo_p2;
                                 }
                             }
                        } // pp1
                     } // pp0
                 } // j
 
 //---------------------------------------------------------
             for (CGU_INT pn=0;  pn < npv_nd[channels3or4-3][type]; pn++)
             {
                 CGV_ERROR err_2=0.0F;
                 CGU_INT   d1;
                 CGU_INT   d2;
 
                 for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
                 {
                     d1 = par_vectors_nd[channels3or4-3][type][pn][0][ch];
                     d2 = par_vectors_nd[channels3or4-3][type][pn][1][ch];
                     err_2+=err_ed[(d1*8)+(d2*4)+ch];
                 }
 
                 if (err_2 < best_err)
                 {
                     best_err = err_2;
                     for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
                     {
                         d1 = par_vectors_nd[channels3or4-3][type][pn][0][ch];
                         d2 = par_vectors_nd[channels3or4-3][type][pn][1][ch];
                         epo_code_shaker_out[  ch]=epo_code_par[d1][d2][0][ch];
                         epo_code_shaker_out[4+ch]=epo_code_par[d1][d2][1][ch];
                     }
                 }
             }

             return best_err;
}

CGV_ERROR  optimize_IndexAndEndPoints(
                    CGV_INDEX      index_io[MAX_SUBSET_SIZE],
                    CGV_EPOCODE    epo_code_out[8],
                    CGV_IMAGE      image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS],
                    CGV_ENTRIES    numEntries,         // max 16
                    CGU_UINT8      Mi_,                // last cluster , This should be no larger than 16
                    CGU_UINT8      bits,               // total for all components
                    CGU_CHANNEL    channels3or4,       // IN: 3 = RGB or 4 = RGBA (4 = MAX_CHANNELS)
uniform CMP_GLOBAL    BC7_Encode     u_BC7Encode[]) 
{
    CGV_ERROR err_best = CMP_FLOAT_MAX;
    CGU_INT   type;
    CGU_CHANNEL   channels2 = 2*channels3or4;

    type = bits % channels2;

    CGU_UINT8 use_par =(type !=0);

    CGU_UINT8   max_bits[MAX_CHANNELS];
    for (CGU_UINT8 ch=0; ch<channels3or4; ch++) 
        max_bits[ch] = (bits+channels2-1) / channels2;

    CGU_INT  iv;
    CGU_INT  clogBC7=0;
    iv = Mi_;
    while (iv>>=1) 
        clogBC7++;

    CGU_INT clt_clogBC7 = CLT(clogBC7);

    if (clt_clogBC7 > 3)
    {
        ASPM_PRINT(("Err: optimize_IndexAndEndPoints, clt_clogBC7\n"));
        return CMP_FLOAT_MAX;
    }

    Mi_ = Mi_ - 1;

    CGV_INDEX     MaxIndex;
    CGV_INDEX     index_tmp[MAX_SUBSET_SIZE];
    CGU_INT       maxTry = MAX_TRY_SHAKER;

    CGV_INDEX       index_best[MAX_SUBSET_SIZE];

    for (CGV_ENTRIES k=0;k<numEntries;k++) 
    {
        index_best[k] = index_tmp[k] = clampIndex(index_io[k],0,15);
    }

    CGV_EPOCODE epo_code_best[2*MAX_CHANNELS];

    SetDefaultEPOCode(epo_code_out ,0xFF,0,0,0);
    SetDefaultEPOCode(epo_code_best,0,0,0,0);

    CGV_ERROR       err_requant = 0.0F;

   MaxIndex = index_collapse(index_tmp, numEntries);

   //===============================
   // we have a solid color 4x4 block
   //===============================
   if (MaxIndex == 0)
   {

       return quant_solid_color(index_io, epo_code_out, image_src, numEntries,  Mi_, max_bits,type, channels3or4);
   }

  do {
        //===============================
        // We have ramp colors to process
        //===============================
        CGV_ERROR   err_cluster = CMP_FLOAT_MAX;
        CGV_ERROR   err_shake;
        CGV_INDEX   index_cluster[MAX_PARTITION_ENTRIES];

        for (CGV_INDEX index_slope=1;  (MaxIndex != 0) && (index_slope*MaxIndex <= Mi_); index_slope++)
        {
            for (CGV_INDEX index_offset=0; index_offset<=Mi_-index_slope*MaxIndex; index_offset++)
            {
              //-------------------------------------
              // set a new index data to try
              //-------------------------------------
              for (CGV_ENTRIES k=0;k<numEntries;k++)
                  index_cluster[k] = index_tmp[k] * index_slope + index_offset;

              CGV_IMAGE     image_cluster[2*MAX_CHANNELS];
              CGV_EPOCODE  epo_code_shake[2*MAX_CHANNELS];
              SetDefaultEPOCode(epo_code_shake,0,0,0xFF,0);

              if (get_ideal_cluster(  image_cluster,
                                      index_cluster,
                                      Mi_,
                                      image_src,
                                      numEntries,
                                      channels3or4) == FALSE) 
              {
                  break;
              }

              err_shake = shake( epo_code_shake,  // return new epo 
                             image_cluster,
                             index_cluster,
                             image_src,
                             clogBC7,
                             type,
                             max_bits,
                             use_par,
                             numEntries,         // max 16
                             channels3or4);

              if (err_shake < err_cluster)
              { 
                  err_cluster = err_shake;
                  for (CGU_CHANNEL ch=0; ch<channels3or4; ch++) 
                  {
                      epo_code_best[  ch] = clampEPO(epo_code_shake[  ch], 0, 255);
                      epo_code_best[4+ch] = clampEPO(epo_code_shake[4+ch], 0, 255);
                  }
              }
            }
        }

        CGV_TYPEINT change = 0;
        CGV_TYPEINT better = 0;

        if ((err_cluster != CMP_FLOAT_MAX))
        {
            //=========================
            // test results for quality
            //=========================
             err_requant = requantized_image_err(
                                                 index_best,    // new index results
                                                 epo_code_best, // prior result input
                                                 clogBC7,
                                                 max_bits,
                                                 image_src,
                                                 numEntries,
                                                 channels3or4);

              // change/better
              // Has the index values changed from that last set 
              for (CGV_ENTRIES k=0;k<numEntries;k++)
                  change = change || (index_cluster[k] != index_best[k]);

              if (err_requant < err_best)
              {
                  better = 1;
                  for (CGV_ENTRIES k=0;k<numEntries;k++)
                  { 
                      index_io[k]=index_tmp[k]=index_best[k];
                  }

                  for (CGU_CHANNEL ch=0; ch<channels3or4; ch++)
                  {
                      epo_code_out[  ch]=epo_code_best[0*4+ch];
                      epo_code_out[4+ch]=epo_code_best[1*4+ch];
                  }
                  err_best=err_requant;
              }
         }

         // Early out if we have our target err
         if( err_best <= u_BC7Encode->errorThreshold)
         {
             break;
         }

        CGV_TYPEINT done;
        done = !(change  &&  better);
        if ((maxTry > 0)&&(!done)) 
        {
            maxTry--;
            MaxIndex = index_collapse(index_tmp, numEntries);
        }
        else 
        {
            maxTry = 0;
        }

    } while (maxTry);

   if (err_best == CMP_FLOAT_MAX)
   {
       ASPM_PRINT(("Err: requantized_image_err\n"));
   }

    return err_best;
}

CGU_UINT8  get_partitionsToTry(uniform CMP_GLOBAL BC7_Encode u_BC7Encode[],CGU_UINT8 maxPartitions)
{
    CGU_FLOAT u_minPartitionSearchSize = 0.30f;
    if(u_BC7Encode->quality <= BC7_qFAST_THRESHOLD) // Using this to match performance and quality of CPU code
    {
         u_minPartitionSearchSize       = u_minPartitionSearchSize + ( u_BC7Encode->quality*BC7_qFAST_THRESHOLD);
    }
    else
    {
        u_minPartitionSearchSize       =  u_BC7Encode->quality;
    }
    return (CGU_UINT8)(maxPartitions * u_minPartitionSearchSize);
}

INLINE void cmp_encode_swap(CGV_EPOCODE endpoint[], CGU_INT channels, CGV_INDEX block_index[MAX_SUBSET_SIZE], CGU_INT bits)
{
   CGU_INT levels = 1 << bits;
   if (block_index[0]>=levels/2)
   {
      cmp_swap_epo(&endpoint[0], &endpoint[channels], channels);
      for (CGU_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
         block_index[k] = CGV_INDEX(levels-1) - block_index[k];
   }
}

void cmp_encode_index(CGV_CMPOUT data[16], CGU_INT* uniform pPos, CGV_INDEX block_index[MAX_SUBSET_SIZE], CGU_INT bits)
{
   cmp_Write8Bit(data,pPos,bits-1,block_index[0]);
   for (CGU_INT j=1;j<SOURCE_BLOCK_SIZE;j++)
   {
       CGV_INDEX qbits = block_index[j]&0xFF;
       cmp_Write8Bit(data,pPos,bits,qbits);
   }
}

void encode_endpoint(CGV_CMPOUT data[16], CGU_INT* uniform pPos, CGV_BYTE block_index[16],  CGU_INT bits, CGV_SHIFT32 flips)
{
   CGU_INT      levels = 1 << bits;
   CGV_TYPEINT  flips_shifted = flips;
   for (CGU_INT k1=0; k1<16; k1++)
   {
      CGV_BYTE qbits_shifted = block_index[k1];
      for (CGU_INT k2=0; k2<8; k2++)
      {
         CGV_TYPEINT q = qbits_shifted&15;
         if ((flips_shifted&1)>0) q = (levels-1)-q;

         if (k1==0 && k2==0)   cmp_Write8Bit(data, pPos, bits - 1, static_cast <CGV_BYTE>(q));
         else                  cmp_Write8Bit(data, pPos, bits, static_cast<CGV_BYTE>(q));
         qbits_shifted >>= 4;
         flips_shifted >>= 1;
      }
   }
}


INLINE CGV_SHIFT32 pow32(CGV_SHIFT32 x) 
{
   return 1<<x; 
}

void  Encode_mode02137(
                    CGU_INT           blockMode,
                    CGV_UINT8         bestPartition,
                    CGV_TYPEUINT32    packedEndpoints[MAX_SUBSETS*2],
                    CGV_BYTE          index16[16],
                    CGV_CMPOUT        cmp_out[COMPRESSED_BLOCK_SIZE])
{
    CGU_INT     partitionBits;
    CGU_UINT32  componentBits;
    CGU_UINT8   maxSubsets;
    CGU_INT     channels;
    CGU_BYTE    indexBits;

    switch(blockMode)
    {
        case 0:
                componentBits   = 4;
                maxSubsets      = 3;
                partitionBits   = 4;
                channels        = 3;
                indexBits       = 3;
                break;
        case 2:
                componentBits   = 5;
                maxSubsets      = 3;
                partitionBits   = 6;
                channels        = 3;
                indexBits       = 2;
                break;
        case 3:
                componentBits   = 7;
                maxSubsets      = 2;
                partitionBits   = 6;
                channels        = 3;
                indexBits       = 2;
                break;
        case 7:
                componentBits   = 5;
                maxSubsets      = 2;
                partitionBits   = 6;
                channels        = 4;
                indexBits       = 2;
                break;
        default:
        case 1:
            componentBits = 6;
            maxSubsets = 2;
            partitionBits = 6;
            channels = 3;
            indexBits = 3;
            break;
    }

    CGV_BYTE  blockindex[SOURCE_BLOCK_SIZE];
    CGV_INT   indexBitsV = indexBits;

    for (CGU_INT k=0; k<COMPRESSED_BLOCK_SIZE; k++) cmp_out[k] = 0;

    // mode 0 = 1, mode 1 = 01, mode 2 = 001, mode 3 = 0001, ...
    CGU_INT    bitPosition = blockMode;
    cmp_Write8Bit(cmp_out,&bitPosition,1,1);

    // Write partition bits
     cmp_Write8Bit(cmp_out,&bitPosition,partitionBits,bestPartition);

  // Sort out the index set and tag whether we need to flip the 
  // endpoints to get the correct state in the implicit index bits
  // The implicitly encoded MSB of the fixup index must be 0
    CGV_FIXUPINDEX    fixup[3];
    get_fixuptable(fixup,(maxSubsets==2?bestPartition:bestPartition+64));

    // Extract indices and mark subsets that need to have their colours flipped to get the
    // right state for the implicit MSB of the fixup index
    CGV_INT     flipColours[3] = {0, 0, 0};

    for (CGV_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
    {
        blockindex[k]  = index16[k];
        for (CGU_UINT8 j=0;j<maxSubsets;j++)
        {
            if(k==fixup[j])
            {
                if(blockindex[k] & (1<<(indexBitsV-1)))
                {
                    flipColours[j] = 1;
                }
            }
        }
     }

    // Now we must flip the endpoints where necessary so that the implicitly encoded
    // index bits have the correct state
    for (CGU_INT subset=0; subset<maxSubsets; subset++)
    {
        if(flipColours[subset] == 1)
        {
            CGV_TYPEUINT32         temp = packedEndpoints[subset*2+0];
            packedEndpoints[subset*2+0] = packedEndpoints[subset*2+1];
            packedEndpoints[subset*2+1] = temp;
        }
    }

    // ...next flip the indices where necessary


    for (CGV_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
    {
        CGV_UINT8   partsub = get_partition_subset(bestPartition,maxSubsets,k);

        if(flipColours[partsub] == 1)
        {
            blockindex[k] = ((1 << indexBitsV) - 1) - blockindex[k];
        }
    }

    // Endpoints are stored in the following order RRRR GGGG BBBB (AAAA) (PPPP)
    // i.e. components are packed together
    CGV_SHIFT32  unpackedColours[MAX_SUBSETS*2*MAX_CHANNELS];
    CGV_BYTE     parityBits[MAX_SUBSETS][2];

    // Unpack the colour values for the subsets
    for (CGU_INT subset=0; subset<maxSubsets; subset++)
    {
        CGV_SHIFT32   packedColours[2] = {packedEndpoints[subset*2+0],packedEndpoints[subset*2+1]};

        if(blockMode == 0 || blockMode == 3|| blockMode == 7) // TWO_PBIT
        {
            parityBits[subset][0] = packedColours[0] & 1;
            parityBits[subset][1] = packedColours[1] & 1;
            packedColours[0] >>= 1;
            packedColours[1] >>= 1;
        }
        else 
        if(blockMode == 1) // ONE_PBIT
        {
            parityBits[subset][0] = packedColours[1] & 1;
            parityBits[subset][1] = packedColours[1] & 1;
            packedColours[0] >>= 1;
            packedColours[1] >>= 1;
        }
        else
        if(blockMode == 2)
        {
            parityBits[subset][0] = 0;
            parityBits[subset][1] = 0;
        }

        for (CGU_INT ch=0; ch<channels;ch++)
        {
            unpackedColours[(subset*2+0)*MAX_CHANNELS+ch] = packedColours[0] & ((1 << componentBits) - 1);
            unpackedColours[(subset*2+1)*MAX_CHANNELS+ch] = packedColours[1] & ((1 << componentBits) - 1);
            packedColours[0] >>= componentBits;
            packedColours[1] >>= componentBits;
        }
    }

    // Loop over component 
    for (CGU_INT ch=0; ch < channels; ch++)
    {
        // loop over subsets
        for (CGU_INT subset=0; subset<maxSubsets; subset++)
        {
            cmp_Write8Bit(cmp_out,&bitPosition,componentBits,unpackedColours[(subset*2+0)*MAX_CHANNELS+ch]&0xFF);
            cmp_Write8Bit(cmp_out,&bitPosition,componentBits,unpackedColours[(subset*2+1)*MAX_CHANNELS+ch]&0xFF);
        }
    }


    // write parity bits 
    if (blockMode != 2)
    {
        for (CGV_INT subset=0; subset<maxSubsets; subset++)
        {
            if(blockMode == 1) // ONE_PBIT
            {
                cmp_Write8Bit(cmp_out,&bitPosition,1,parityBits[subset][0]&0x01);
            }
            else // TWO_PBIT
            {
                cmp_Write8Bit(cmp_out,&bitPosition,1,parityBits[subset][0]&0x01);
                cmp_Write8Bit(cmp_out,&bitPosition,1,parityBits[subset][1]&0x01);
            }
        }
    }

    // Encode the index bits
    CGV_INT bitPositionV = bitPosition;
    for (CGV_FIXUPINDEX k=0; k<SOURCE_BLOCK_SIZE; k++)
    {
        CGV_UINT8   partsub = get_partition_subset(bestPartition,maxSubsets,k);

        // If this is a fixup index then drop the MSB which is implicitly 0
        if(k == fixup[partsub])
        {
            cmp_Write8BitV(cmp_out, bitPositionV, indexBits-1,blockindex[k]&0x07F);
            bitPositionV += indexBits-1;
        }
        else
        {
            cmp_Write8BitV(cmp_out,bitPositionV, indexBits,blockindex[k]);
            bitPositionV += indexBits;
        }
    }
}

void  Encode_mode4( CGV_CMPOUT     cmp_out[COMPRESSED_BLOCK_SIZE],
                    varying cmp_mode_parameters* uniform params )
{
    CGU_INT   bitPosition = 4;    // Position the pointer at the LSB

    for (CGU_INT k=0; k<COMPRESSED_BLOCK_SIZE; k++) cmp_out[k] = 0;

    // mode 4 (5 bits) 00001
    cmp_Write8Bit(cmp_out,&bitPosition,1,1);

    // rotation 2 bits
    cmp_Write8Bit(cmp_out, &bitPosition, 2, static_cast <CGV_BYTE> (params->rotated_channel));

    // idxMode 1 bit
    cmp_Write8Bit(cmp_out, &bitPosition, 1, static_cast <CGV_BYTE> (params->idxMode));

    CGU_INT   idxBits[2] = {2,3};

    if(params->idxMode)
    {
        idxBits[0] = 3;
        idxBits[1] = 2;
        // Indicate if we need to fixup the index
        cmp_swap_index(params->color_index,params->alpha_index,16);
        cmp_encode_swap(params->alpha_qendpoint, 4, params->color_index,2);
        cmp_encode_swap(params->color_qendpoint, 4, params->alpha_index,3);
    }
    else
    {
        cmp_encode_swap(params->color_qendpoint, 4, params->color_index,2);
        cmp_encode_swap(params->alpha_qendpoint, 4, params->alpha_index,3);
    }

   // color endpoints 5 bits each
   // R0 : R1
   // G0 : G1
   // B0 : B1
   for (CGU_INT component=0; component < 3; component++)
   {
        cmp_Write8Bit(cmp_out, &bitPosition, 5, static_cast<CGV_BYTE> (params->color_qendpoint[component]));
        cmp_Write8Bit(cmp_out, &bitPosition, 5, static_cast <CGV_BYTE> (params->color_qendpoint[4 + component]));
   }

   // alpha endpoints (6 bits each)
   // A0 : A1
   cmp_Write8Bit(cmp_out, &bitPosition, 6, static_cast<CGV_BYTE> (params->alpha_qendpoint[0]));
   cmp_Write8Bit(cmp_out, &bitPosition, 6, static_cast<CGV_BYTE> (params->alpha_qendpoint[4]));

    // index 2 bits each  (31 bits total)
    cmp_encode_index(cmp_out, &bitPosition, params->color_index, 2);
    // index 3 bits each  (47 bits total)
    cmp_encode_index(cmp_out, &bitPosition, params->alpha_index, 3);
}

void  Encode_mode5( CGV_CMPOUT     cmp_out[COMPRESSED_BLOCK_SIZE],
                    varying cmp_mode_parameters* uniform params)
{
    for (CGU_INT k=0; k<COMPRESSED_BLOCK_SIZE; k++) cmp_out[k] = 0;

    // mode 5 bits = 000001
    CGU_INT   bitPosition = 5;    // Position the pointer at the LSB
    cmp_Write8Bit(cmp_out,&bitPosition,1,1);

    // Write 2 bit rotation
    cmp_Write8Bit(cmp_out, &bitPosition, 2, static_cast<CGV_BYTE> (params->rotated_channel));

    cmp_encode_swap(params->color_qendpoint, 4, params->color_index,2);
    cmp_encode_swap(params->alpha_qendpoint, 4, params->alpha_index,2);

   // color endpoints (7 bits each)
   // R0 : R1
   // G0 : G1
   // B0 : B1
   for (CGU_INT component=0; component < 3; component++)
   {
        cmp_Write8Bit(cmp_out, &bitPosition, 7, static_cast<CGV_BYTE> (params->color_qendpoint[component]));
        cmp_Write8Bit(cmp_out, &bitPosition, 7, static_cast <CGV_BYTE> (params->color_qendpoint[4 + component]));
   }

   // alpha endpoints (8 bits each)
   // A0 : A1
   cmp_Write8Bit(cmp_out, &bitPosition, 8, static_cast<CGV_BYTE> (params->alpha_qendpoint[0]));
   cmp_Write8Bit(cmp_out, &bitPosition, 8, static_cast<CGV_BYTE> (params->alpha_qendpoint[4]));


   // color index 2 bits each  (31 bits total)
   // alpha index 2 bits each  (31 bits total)
   cmp_encode_index(cmp_out, &bitPosition, params->color_index, 2);
   cmp_encode_index(cmp_out, &bitPosition, params->alpha_index, 2);
}

void  Encode_mode6(
                    CGV_INDEX         index[MAX_SUBSET_SIZE],
                    CGV_EPOCODE       epo_code[8],
                    CGV_CMPOUT        cmp_out[COMPRESSED_BLOCK_SIZE])
{
    for (CGU_INT k=0; k<COMPRESSED_BLOCK_SIZE; k++) cmp_out[k] = 0;

    cmp_encode_swap(epo_code, 4, index,4);

    // Mode = 6  bits = 0000001
    CGU_INT    bitPosition = 6;    // Position the pointer at the LSB
    cmp_Write8Bit(cmp_out,&bitPosition,1, 1);

    // endpoints
    for (CGU_INT p=0; p<4; p++)
    {
        cmp_Write8Bit(cmp_out, &bitPosition, 7, static_cast<CGV_BYTE> (epo_code[0 + p] >> 1));
        cmp_Write8Bit(cmp_out, &bitPosition, 7, static_cast<CGV_BYTE> (epo_code[4 + p] >> 1));
    }

    // p bits
    cmp_Write8Bit(cmp_out, &bitPosition, 1, epo_code[0]&1);
    cmp_Write8Bit(cmp_out, &bitPosition, 1, epo_code[4]&1);

    // quantized values
    cmp_encode_index(cmp_out, &bitPosition, index, 4);
}


void  Compress_mode01237(
                    CGU_INT             blockMode,
                    BC7_EncodeState     EncodeState[],
uniform CMP_GLOBAL    BC7_Encode          u_BC7Encode[])
{
    CGV_INDEX       storedBestindex[MAX_PARTITIONS][MAX_SUBSETS][MAX_SUBSET_SIZE];
    CGV_ERROR       storedError[MAX_PARTITIONS];
    CGV_UINT8       sortedPartition[MAX_PARTITIONS];

    EncodeState->numPartitionModes = 64;
    EncodeState->maxSubSets = 2;

    if (blockMode == 0)
    {
        EncodeState->numPartitionModes = 16;
        EncodeState->channels3or4      = 3;
        EncodeState->bits              = 26;
        EncodeState->clusters          = 8; 
        EncodeState->componentBits     = 4; 
        EncodeState->maxSubSets        = 3;
    }
    else
    if (blockMode == 2)
    {
        EncodeState->channels3or4  = 3;
        EncodeState->bits          = 30;
        EncodeState->clusters      = 4;
        EncodeState->componentBits = 5;
        EncodeState->maxSubSets    = 3;
    }
    else
    if (blockMode == 1)
    {
    
        EncodeState->channels3or4  = 3;
        EncodeState->bits          = 37;
        EncodeState->clusters      = 8; 
        EncodeState->componentBits = 6;
    }
    else
    if (blockMode == 3)
    {
        EncodeState->channels3or4  = 3;
        EncodeState->bits          = 44;
        EncodeState->clusters      = 4;
        EncodeState->componentBits = 7;
    }
    else
    if (blockMode == 7)
    {
        EncodeState->channels3or4  = 4;
        EncodeState->bits          = 42; // (2* (R 5 + G 5 + B 5 + A 5)) + 2 parity bits
        EncodeState->clusters      = 4;
        EncodeState->componentBits = 5;  // 5 bit components
    }

    CGV_IMAGE        image_subsets[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_CHANNELS];
    CGV_ENTRIES      subset_entryCount[MAX_SUBSETS] = {0,0,0};

    // Loop over the available partitions for the block mode and quantize them 
    // to figure out the best candidates for further refinement
    CGU_UINT8       mode_partitionsToTry;
    mode_partitionsToTry = get_partitionsToTry(u_BC7Encode,EncodeState->numPartitionModes);

    CGV_UINT8     bestPartition = 0;

    for (CGU_INT mode_blockPartition = 0;  mode_blockPartition < mode_partitionsToTry;  mode_blockPartition++)
    {

        GetPartitionSubSet_mode01237(
                  image_subsets,
                  subset_entryCount,
                  static_cast<CGV_UINT8>(mode_blockPartition),
                  EncodeState->image_src,
                  blockMode,
                  EncodeState->channels3or4);

        CGV_IMAGE  subset_image_src[SOURCE_BLOCK_SIZE*MAX_CHANNELS];
        CGV_INDEX  index_out1[SOURCE_BLOCK_SIZE];
        CGV_ERROR  err_quant = 0.0F;

        // Store the quntize error for this partition to be sorted and processed later
        for (CGU_INT subset=0; subset < EncodeState->maxSubSets; subset++)
        {
                CGV_ENTRIES       numEntries = subset_entryCount[subset];

                for (CGU_INT ii=0; ii<SOURCE_BLOCK_SIZE; ii++)
                {
                    subset_image_src[ii+COMP_RED  *SOURCE_BLOCK_SIZE]   = image_subsets[subset][ii][0];
                    subset_image_src[ii+COMP_GREEN*SOURCE_BLOCK_SIZE]   = image_subsets[subset][ii][1];
                    subset_image_src[ii+COMP_BLUE *SOURCE_BLOCK_SIZE]   = image_subsets[subset][ii][2];
                    subset_image_src[ii+COMP_ALPHA*SOURCE_BLOCK_SIZE]   = image_subsets[subset][ii][3];
                }

                CGV_INDEXPACKED  color_index2[2];

                err_quant += GetQuantizeIndex(
                                       color_index2,
                                       index_out1,
                                       subset_image_src,
                                       numEntries,
                                       EncodeState->clusters,
                                       EncodeState->channels3or4);

                for (CGV_INT idx=0; idx < numEntries; idx++)
                {
                     storedBestindex[mode_blockPartition][subset][idx] = index_out1[idx];
                }
        }

        storedError[mode_blockPartition] = err_quant;
    }

    // Sort the results
    sortPartitionProjection( storedError,
                             sortedPartition,
                             mode_partitionsToTry);

    CGV_EPOCODE   epo_code[MAX_SUBSETS*2*MAX_CHANNELS];
    CGV_EPOCODE   bestEndpoints[MAX_SUBSETS*2*MAX_CHANNELS];
    CGV_BYTE      bestindex[MAX_SUBSETS*MAX_SUBSET_SIZE];
    CGV_ENTRIES   bestEntryCount[MAX_SUBSETS];
    CGV_BYTE      bestindex16[MAX_SUBSET_SIZE];

    // Extensive shaking is most important when the ramp is short, and
    // when we have less index. On a long ramp the quality of the
    // initial quantizing is relatively more important
    // We modulate the shake size according to the number of ramp index
    // - the more index we have the less shaking should be required to find a near
    // optimal match

    CGU_UINT8   numShakeAttempts = max8(1, min8((CGU_UINT8)floor(8 * u_BC7Encode->quality + 0.5), mode_partitionsToTry));
    CGV_ERROR       err_best = CMP_FLOAT_MAX;

    // Now do the endpoint shaking
    for (CGU_INT nSA =0; nSA < numShakeAttempts; nSA++)
    {

        CGV_ERROR err_optimized = 0.0F;
        CGV_UINT8 sortedBlockPartition;
        sortedBlockPartition = sortedPartition[nSA];

        //********************************************
        // Get the partition shape for the given mode
        //********************************************
        GetPartitionSubSet_mode01237(
                  image_subsets,
                  subset_entryCount,
                  sortedBlockPartition,
                  EncodeState->image_src,
                  blockMode,
                  EncodeState->channels3or4);

        //*****************************
        // Process the partition shape 
        //*****************************
        for (CGU_INT subset=0; subset < EncodeState->maxSubSets; subset++)
        {
             CGV_ENTRIES   numEntries = subset_entryCount[subset];
             CGV_IMAGE     src_image_block[SOURCE_BLOCK_SIZE*MAX_CHANNELS];
             CGV_INDEX     index_io[MAX_SUBSET_SIZE];
             CGV_EPOCODE   tmp_epo_code[8];

             for (CGU_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
             {
                 src_image_block[k+COMP_RED*SOURCE_BLOCK_SIZE]   = image_subsets[subset][k][0];
                 src_image_block[k+COMP_GREEN*SOURCE_BLOCK_SIZE] = image_subsets[subset][k][1];
                 src_image_block[k+COMP_BLUE*SOURCE_BLOCK_SIZE]  = image_subsets[subset][k][2];
                 src_image_block[k+COMP_ALPHA*SOURCE_BLOCK_SIZE] = image_subsets[subset][k][3];
             }

             for (CGU_INT k=0; k<MAX_SUBSET_SIZE; k++)
             {
                  index_io[k] = storedBestindex[sortedBlockPartition][subset][k];
             }

             err_optimized += optimize_IndexAndEndPoints(
                                   index_io,
                                   tmp_epo_code,
                                   src_image_block,
                                   numEntries,
                                   static_cast<CGU_INT8>(EncodeState->clusters),  // Mi_
                                   EncodeState->bits,
                                   EncodeState->channels3or4,
                                   u_BC7Encode);

             for (CGU_INT k=0; k < MAX_SUBSET_SIZE; k++)
             {
                storedBestindex[sortedBlockPartition][subset][k] = index_io[k];
             }

             for (CGU_INT ch=0; ch<MAX_CHANNELS; ch++)
             {
                 epo_code[(subset*2+0)*4+ch] = tmp_epo_code[  ch];
                 epo_code[(subset*2+1)*4+ch] = tmp_epo_code[4+ch];
             }
        }

        //****************************************
        // Check if result is better than the last
        //****************************************
        if(err_optimized < err_best)
        {
            bestPartition = sortedBlockPartition;
            CGV_INT bestIndexCount = 0;

            for (CGU_INT subset=0; subset < EncodeState->maxSubSets; subset++)
            {
                CGV_ENTRIES       numEntries = subset_entryCount[subset];
                bestEntryCount[subset] = numEntries;

                if(numEntries)
                {
                    for (CGU_INT ch=0; ch < EncodeState->channels3or4; ch++)
                    {
                        bestEndpoints[(subset*2+0)*4+ch] = epo_code[(subset*2+0)*4+ch];
                        bestEndpoints[(subset*2+1)*4+ch] = epo_code[(subset*2+1)*4+ch];
                    }

                    for (CGV_ENTRIES k=0; k< numEntries; k++)
                    {
                        bestindex[subset*MAX_SUBSET_SIZE+k] = storedBestindex[sortedBlockPartition][subset][k];
                        bestindex16[bestIndexCount++]       = storedBestindex[sortedBlockPartition][subset][k];
                    }
                }
            }

            err_best = err_optimized;
            // Early out if we  found we can compress with error below the quality threshold
            if(err_best <= u_BC7Encode->errorThreshold)
            {
                break;
            }
        }
    }


    if (blockMode != 7)
           err_best +=  EncodeState->opaque_err;

    if(err_best >  EncodeState->best_err) 
              return;

    //**************************
    // Save the encoded block
    //**************************
    EncodeState->best_err = err_best;


    // Now we have all the data needed to encode the block
    // We need to pack the endpoints prior to encoding
    CGV_TYPEUINT32   packedEndpoints[MAX_SUBSETS*2] = {0,0,0,0,0,0};
    for (CGU_INT subset=0; subset<EncodeState->maxSubSets; subset++)
    {
        packedEndpoints[(subset*2)+0] = 0;
        packedEndpoints[(subset*2)+1] = 0;

        if(bestEntryCount[subset])
        {
            CGU_UINT32   rightAlignment = 0;

            // Sort out parity bits
            if(blockMode != 2)
            {
                // Sort out BCC parity bits
                packedEndpoints[(subset*2)+0] = bestEndpoints[(subset*2+0)*4+0] & 1;
                packedEndpoints[(subset*2)+1] = bestEndpoints[(subset*2+1)*4+0] & 1;
                for (CGU_INT ch=0; ch<EncodeState->channels3or4; ch++)
                {
                    bestEndpoints[(subset*2+0)*4+ch] >>= 1;
                    bestEndpoints[(subset*2+1)*4+ch] >>= 1;
                }
                rightAlignment++;
            }

            // Fixup endpoints
            for (CGU_INT ch=0; ch<EncodeState->channels3or4; ch++)
            {
                    packedEndpoints[(subset*2)+0] |= bestEndpoints[((subset*2)+0)*4+ch] << rightAlignment;
                    packedEndpoints[(subset*2)+1] |= bestEndpoints[((subset*2)+1)*4+ch] << rightAlignment;
                    rightAlignment += EncodeState->componentBits;
            }
        }
    }

    CGV_UINT8    idxCount[3] = {0, 0, 0};
    for (CGV_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
    {
        CGV_UINT8     partsub = get_partition_subset(bestPartition,EncodeState->maxSubSets,k);
        CGV_UINT8 idxC = idxCount[partsub];
        bestindex16[k]  = bestindex[partsub*MAX_SUBSET_SIZE+idxC];
        idxCount[partsub]    = idxC + 1;
     }

    Encode_mode02137(
                     blockMode,
                     bestPartition,
                     packedEndpoints,
                     bestindex16,
                     EncodeState->cmp_out);
}

void  Compress_mode45(
                    CGU_INT             blockMode,
                    BC7_EncodeState     EncodeState[],
uniform CMP_GLOBAL    BC7_Encode          u_BC7Encode[])
{

    cmp_mode_parameters best_candidate;
    EncodeState->channels3or4 = 4;
    cmp_memsetBC7((CGV_BYTE *)&best_candidate, 0, sizeof(cmp_mode_parameters));

    if (blockMode == 4)
    {
        EncodeState->max_idxMode     = 2;
        EncodeState->modeBits[0]     = 30;  // bits = 2 * (Red 5+ Grn 5+ blu 5)
        EncodeState->modeBits[1]     = 36;  // bits = 2 * (Alpha 6+6+6)
        EncodeState->numClusters0[0] = 4;
        EncodeState->numClusters0[1] = 8;
        EncodeState->numClusters1[0] = 8;
        EncodeState->numClusters1[1] = 4;
    }
    else
    {
        EncodeState->max_idxMode     = 1;
        EncodeState->modeBits[0]     = 42;  // bits = 2 * (Red 7+ Grn 7+ blu 7)
        EncodeState->modeBits[1]     = 48;  // bits = 2 * (Alpha 8+8+8) = 48
        EncodeState->numClusters0[0] = 4;
        EncodeState->numClusters0[1] = 4;
        EncodeState->numClusters1[0] = 4;
        EncodeState->numClusters1[1] = 4;
    }


    CGV_IMAGE   src_color_Block[SOURCE_BLOCK_SIZE*MAX_CHANNELS];
    CGV_IMAGE   src_alpha_Block[SOURCE_BLOCK_SIZE*MAX_CHANNELS];

    // Go through each possible rotation and selection of index rotationBits)
    for (CGU_CHANNEL rotated_channel = 0; rotated_channel < EncodeState->channels3or4; rotated_channel++)  
    { // A

        for (CGU_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
        { 
            for (CGU_INT p=0; p<3; p++)
            {
                src_color_Block[k+p*SOURCE_BLOCK_SIZE] =  EncodeState->image_src[k+componentRotations[rotated_channel][p+1]*SOURCE_BLOCK_SIZE];
                src_alpha_Block[k+p*SOURCE_BLOCK_SIZE] =  EncodeState->image_src[k+componentRotations[rotated_channel][0]*SOURCE_BLOCK_SIZE];
            }
        }

        CGV_ERROR   err_quantizer;
        CGV_ERROR   err_bestQuantizer = CMP_FLOAT_MAX;

        for (CGU_INT idxMode = 0; idxMode < EncodeState->max_idxMode; idxMode++)
        { // B
            CGV_INDEXPACKED  color_index2[2]; // reserved .. Not used!

           err_quantizer = GetQuantizeIndex(
                                color_index2,
                                best_candidate.color_index,
                                src_color_Block,
                                SOURCE_BLOCK_SIZE,
                                EncodeState->numClusters0[idxMode],
                                3);

            err_quantizer += GetQuantizeIndex(
                                color_index2,
                                best_candidate.alpha_index,
                                src_alpha_Block,
                                SOURCE_BLOCK_SIZE,
                                EncodeState->numClusters1[idxMode],
                                3) / 3.0F;

            // If quality is high then run the full shaking for this config and
            // store the result if it beats the best overall error
            // Otherwise only run the shaking if the error is better than the best
            // quantizer error
            if(err_quantizer <= err_bestQuantizer)
            {
                err_bestQuantizer = err_quantizer;

                // Shake size gives the size of the shake cube
                CGV_ERROR   err_overallError;

                err_overallError = optimize_IndexAndEndPoints(
                                             best_candidate.color_index,
                                             best_candidate.color_qendpoint,
                                             src_color_Block,
                                             SOURCE_BLOCK_SIZE,
                                             EncodeState->numClusters0[idxMode],
                                             static_cast<CGU_INT8>(EncodeState->modeBits[0]),
                                             3,
                                             u_BC7Encode);

                // Alpha scalar block
                err_overallError += optimize_IndexAndEndPoints(
                                               best_candidate.alpha_index,
                                               best_candidate.alpha_qendpoint,
                                               src_alpha_Block,
                                               SOURCE_BLOCK_SIZE,
                                               EncodeState->numClusters1[idxMode],
                                               static_cast<CGU_UINT8>(EncodeState->modeBits[1]),
                                               3,
                                               u_BC7Encode) / 3.0f;

                // If we beat the previous best then encode the block
                if(err_overallError <  EncodeState->best_err)
                {
                   best_candidate.idxMode         = idxMode;
                   best_candidate.rotated_channel = rotated_channel;
                   if (blockMode == 4)
                        Encode_mode4( EncodeState->cmp_out, &best_candidate);
                   else
                        Encode_mode5( EncodeState->cmp_out, &best_candidate);
                   EncodeState->best_err = err_overallError;
                }
            }
        } // B
    } // A
}


void  Compress_mode6( BC7_EncodeState     EncodeState[],
uniform CMP_GLOBAL      BC7_Encode          u_BC7Encode[])
{
    CGV_ERROR        err;

    CGV_EPOCODE      epo_code_out[8] = {0};
    CGV_INDEX        best_index_out[MAX_SUBSET_SIZE];
    CGV_INDEXPACKED  best_packedindex_out[2];


   // CGV_IMAGE        block_endpoints[8];
   // icmp_get_block_endpoints(block_endpoints,  EncodeState->image_src, -1, 4);
   // icmp_GetQuantizedEpoCode(epo_code_out, block_endpoints, 6,4);
   // err = icmp_GetQuantizeIndex(best_packedindex_out, best_index_out, EncodeState->image_src, 4, block_endpoints, 0,4);

    err = GetQuantizeIndex(
                   best_packedindex_out,
                   best_index_out,
                   EncodeState->image_src,
                   16, // numEntries
                   16, // clusters
                   4);  // channels3or4

    //*****************************
    // Process the partition shape 
    //*****************************
    err  = optimize_IndexAndEndPoints(
                          best_index_out,
                          epo_code_out,
                          EncodeState->image_src,
                          16, //numEntries
                          16, // Mi_ = clusters
                          58, // bits
                          4,  // channels3or4
                          u_BC7Encode);
    
    //**************************
    // Save the encoded block
    //**************************
    
    if (err < EncodeState->best_err)
    {
        EncodeState->best_err = err;
        Encode_mode6(
                    best_index_out,
                    epo_code_out,
                    EncodeState->cmp_out);
    }
}

void copy_BC7_Encode_settings(BC7_EncodeState  EncodeState[], uniform CMP_GLOBAL BC7_Encode settings [])
{
 EncodeState->best_err          = CMP_FLOAT_MAX;
 EncodeState->validModeMask     = settings->validModeMask;
 #ifdef USE_ICMP
 EncodeState->part_count        = settings->part_count;
 EncodeState->channels          = settings->channels;
#endif
}

//===================================== ICMP CODE =========================================================
#ifdef USE_ICMP
//========================================
// Modified Intel Texture Compression Code
//========================================

void icmp_Write32Bit(CGV_CMPOUTPACKED base[], CGU_INT* uniform offset, CGU_INT bits, CGV_CMPOUTPACKED bitVal)
{
    base[*offset / 32] |= ((CGV_CMPOUTPACKED)bitVal) << (*offset % 32);
    if (*offset % 32 + bits > 32)
    {
        base[*offset / 32 + 1] |= shift_right_uint32(bitVal, 32 - *offset % 32);
    }
    *offset += bits;
}

//================ 32 bit cmp_out mode encoders ===============

INLINE void icmp_swap_epocode(CGV_EPOCODE u[], CGV_EPOCODE v[], CGU_INT n)
{
    for (CGU_INT i = 0; i < n; i++)
    {
        CGV_EPOCODE t = u[i];
        u[i] = v[i];
        v[i] = t;
    }
}

void icmp_encode_apply_swap(CGV_EPOCODE endpoint[], CGU_INT channel, CGV_INDEXPACKED block_index[2], CGU_INT bits)
{
    CGU_INT levels = 1 << bits;
    if ((block_index[0] & 15) >= levels / 2)
    {
        icmp_swap_epocode(&endpoint[0], &endpoint[channel], channel);

        for (CGU_INT k = 0; k < 2; k++)
            block_index[k] = (CGV_INDEXPACKED)(0x11111111 * (levels - 1)) - block_index[k];
    }
}

void icmp_encode_index(CGV_CMPOUTPACKED data[5], CGU_INT* uniform pPos, CGV_INDEXPACKED block_index[2], CGU_INT bits, CGV_MASK flips)
{
    CGU_INT levels = 1 << bits;
    CGV_MASK flips_shifted = flips;
    for (CGU_INT k1 = 0; k1 < 2; k1++)
    {
        CGV_CMPOUTPACKED qbits_shifted = block_index[k1];
        for (CGU_INT k2 = 0; k2 < 8; k2++)
        {
            CGV_CMPOUTPACKED q = qbits_shifted & 15;
            if ((flips_shifted & 1) > 0) q = (levels - 1) - q;

            if (k1 == 0 && k2 == 0)   icmp_Write32Bit(data, pPos, bits - 1, q);
            else            icmp_Write32Bit(data, pPos, bits, q);
            qbits_shifted >>= 4;
            flips_shifted >>= 1;
        }
    }
}

void icmp_bc7_encode_endpoint2(CGV_CMPOUTPACKED data[5], CGU_INT* uniform pPos, CGV_INDEXPACKED color_index[2], CGU_INT bits, CGV_MASK flips)
{
    CGU_INT levels = 1 << bits;
    CGV_MASK flips_shifted = flips;
    for (CGU_INT k1 = 0; k1 < 2; k1++)
    {
        CGV_INDEXPACKED qbits_shifted = color_index[k1];
        for (CGU_INT k2 = 0; k2 < 8; k2++)
        {
            CGV_INDEXPACKED q = qbits_shifted & 15;
            if ((flips_shifted & 1) > 0) q = (levels - 1) - q;

            if (k1 == 0 && k2 == 0)   icmp_Write32Bit(data, pPos, bits - 1, q);
            else                  icmp_Write32Bit(data, pPos, bits, q);
            qbits_shifted >>= 4;
            flips_shifted >>= 1;
        }
    }
}

INLINE CGV_CMPOUTPACKED icmp_pow2Packed(CGV_FIXUPINDEX x)
{
    return 1 << x;
}

INLINE void icmp_encode_data_shl_1bit_from(CGV_CMPOUTPACKED data[5], CGV_FIXUPINDEX from)
{
    if (from < 96)
    {
        //assert(from > 64+10);

        CGV_CMPOUTPACKED shifted = (data[2] >> 1) | (data[3] << 31);
        CGV_CMPOUTPACKED mask = (icmp_pow2Packed(from - 64) - 1) >> 1;
        data[2] = (mask&data[2]) | (~mask&shifted);
        data[3] = (data[3] >> 1) | (data[4] << 31);
        data[4] = data[4] >> 1;
    }
    else if (from < 128)
    {
        CGV_CMPOUTPACKED shifted = (data[3] >> 1) | (data[4] << 31);
        CGV_CMPOUTPACKED mask = (icmp_pow2Packed(from - 96) - 1) >> 1;
        data[3] = (mask&data[3]) | (~mask&shifted);
        data[4] = data[4] >> 1;
    }
}

INLINE void icmp_get_fixuptable(CGV_FIXUPINDEX fixup[3], CGV_PARTID part_id)
{
    // same as  CMP SDK v3.1 BC7_FIXUPINDEX1 &  BC7_FIXUPINDEX2 for each partition range 0..63
    // The data is saved as a packed INT = (BC7_FIXUPINDEX1 << 4 + BC7_FIXUPINDEX2)
        CMP_STATIC  uniform __constant  CGV_FIXUPINDEX FIXUPINDEX[] = {
        // 2 subset partitions 0..63
         0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u,
         0xf0u, 0x20u, 0x80u, 0x20u, 0x20u, 0x80u, 0x80u, 0xf0u, 0x20u, 0x80u, 0x20u, 0x20u, 0x80u, 0x80u, 0x20u, 0x20u,
         0xf0u, 0xf0u, 0x60u, 0x80u, 0x20u, 0x80u, 0xf0u, 0xf0u, 0x20u, 0x80u, 0x20u, 0x20u, 0x20u, 0xf0u, 0xf0u, 0x60u,
         0x60u, 0x20u, 0x60u, 0x80u, 0xf0u, 0xf0u, 0x20u, 0x20u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0xf0u, 0x20u, 0x20u, 0xf0u,
         // 3 subset partitions 64..128
         0x3fu, 0x38u, 0xf8u, 0xf3u, 0x8fu, 0x3fu, 0xf3u, 0xf8u, 0x8fu, 0x8fu, 0x6fu, 0x6fu, 0x6fu, 0x5fu, 0x3fu, 0x38u,
         0x3fu, 0x38u, 0x8fu, 0xf3u, 0x3fu, 0x38u, 0x6fu, 0xa8u, 0x53u, 0x8fu, 0x86u, 0x6au, 0x8fu, 0x5fu, 0xfau, 0xf8u,
         0x8fu, 0xf3u, 0x3fu, 0x5au, 0x6au, 0xa8u, 0x89u, 0xfau, 0xf6u, 0x3fu, 0xf8u, 0x5fu, 0xf3u, 0xf6u, 0xf6u, 0xf8u,
         0x3fu, 0xf3u, 0x5fu, 0x5fu, 0x5fu, 0x8fu, 0x5fu, 0xafu, 0x5fu, 0xafu, 0x8fu, 0xdfu, 0xf3u, 0xcfu, 0x3fu, 0x38u
    };

    CGV_FIXUPINDEX skip_packed = FIXUPINDEX[part_id];// gather_int2(FIXUPINDEX, part_id);
    fixup[0] = 0;
    fixup[1] = skip_packed >> 4;
    fixup[2] = skip_packed & 15;
}

void icmp_bc7_encode_adjust_skip_mode01237_2(CGV_CMPOUTPACKED data[5], CGU_INT mode, CGV_PARTID part_id)
{
    CGU_INT bits = 2;  if (mode == 0 || mode == 1) bits = 3;
    CGU_INT maxSubSets = 2;  if (mode == 0 || mode == 2) maxSubSets = 3;

    CGV_FIXUPINDEX fixup[3];
    icmp_get_fixuptable(fixup, part_id);

    if (maxSubSets > 2 && fixup[1] < fixup[2])
    {
        CGV_FIXUPINDEX t = fixup[1]; fixup[1] = fixup[2]; fixup[2] = t;
    }

    for (CGU_INT j = 1; j < maxSubSets; j++)
    {
        CGV_FIXUPINDEX k = fixup[j];
        icmp_encode_data_shl_1bit_from(data, 128 + (maxSubSets - 1) - (15 - k)*bits);
    }
}

INLINE CGV_UINT32 gather_uint32(__constant CGU_UINT32 * const uniform ptr, CGV_INT idx)
{
    return ptr[idx]; // (perf warning expected)
}

INLINE CGV_MASK icmp_get_partition_mask(CGV_PARTID part_id, CGU_INT subset)
{
    CMP_STATIC uniform __constant  CGV_SHIFT32 pattern_mask_table[] = {
        // 2 subset partitions
        0xCCCC3333u, 0x88887777u, 0xEEEE1111u, 0xECC81337u, 0xC880377Fu, 0xFEEC0113u, 0xFEC80137u, 0xEC80137Fu,
        0xC80037FFu, 0xFFEC0013u, 0xFE80017Fu, 0xE80017FFu, 0xFFE80017u, 0xFF0000FFu, 0xFFF0000Fu, 0xF0000FFFu,
        0xF71008EFu, 0x008EFF71u, 0x71008EFFu, 0x08CEF731u, 0x008CFF73u, 0x73108CEFu, 0x3100CEFFu, 0x8CCE7331u,
        0x088CF773u, 0x3110CEEFu, 0x66669999u, 0x366CC993u, 0x17E8E817u, 0x0FF0F00Fu, 0x718E8E71u, 0x399CC663u,
        0xAAAA5555u, 0xF0F00F0Fu, 0x5A5AA5A5u, 0x33CCCC33u, 0x3C3CC3C3u, 0x55AAAA55u, 0x96966969u, 0xA55A5AA5u,
        0x73CE8C31u, 0x13C8EC37u, 0x324CCDB3u, 0x3BDCC423u, 0x69969669u, 0xC33C3CC3u, 0x99666699u, 0x0660F99Fu,
        0x0272FD8Du, 0x04E4FB1Bu, 0x4E40B1BFu, 0x2720D8DFu, 0xC93636C9u, 0x936C6C93u, 0x39C6C639u, 0x639C9C63u,
        0x93366CC9u, 0x9CC66339u, 0x817E7E81u, 0xE71818E7u, 0xCCF0330Fu, 0x0FCCF033u, 0x774488BBu, 0xEE2211DDu,

        // 3 subset partitions
        0x08CC0133u, 0x8CC80037u, 0xCC80006Fu, 0xEC001331u, 0x330000FFu, 0x00CC3333u, 0xFF000033u, 0xCCCC0033u,
        0x0F0000FFu, 0x0FF0000Fu, 0x00F0000Fu, 0x44443333u, 0x66661111u, 0x22221111u, 0x136C0013u, 0x008C8C63u,
        0x36C80137u, 0x08CEC631u, 0x3330000Fu, 0xF0000333u, 0x00EE1111u, 0x88880077u, 0x22C0113Fu, 0x443088CFu,
        0x0C22F311u, 0x03440033u, 0x69969009u, 0x9960009Fu, 0x03303443u, 0x00660699u, 0xC22C3113u, 0x8C0000EFu,
        0x1300007Fu, 0xC4003331u, 0x004C1333u, 0x22229999u, 0x00F0F00Fu, 0x24929249u, 0x29429429u, 0xC30C30C3u,
        0xC03C3C03u, 0x00AA0055u, 0xAA0000FFu, 0x30300303u, 0xC0C03333u, 0x90900909u, 0xA00A5005u, 0xAAA0000Fu,
        0x0AAA0555u, 0xE0E01111u, 0x70700707u, 0x6660000Fu, 0x0EE01111u, 0x07707007u, 0x06660999u, 0x660000FFu,
        0x00660099u, 0x0CC03333u, 0x03303003u, 0x60000FFFu, 0x80807777u, 0x10100101u, 0x000A0005u, 0x08CE8421u
    };

    CGV_MASK mask_packed = gather_uint32(pattern_mask_table, part_id);
    CGV_MASK mask0 = mask_packed & 0xFFFF;
    CGV_MASK mask1 = mask_packed >> 16;

    CGV_MASK mask = (subset == 2) ? (~mask0)&(~mask1) : ((subset == 0) ? mask0 : mask1);
    return mask;
}

#ifdef USE_VARYING
#ifdef ASPM_GPU
INLINE CGV_INDEXPACKED gather_packedindex(CGV_INDEXPACKED* ptr, CGV_FIXUPINDEX idx)
{
    return ptr[idx];
}
#else
INLINE CGV_INDEXPACKED gather_packedindex(CMP_CONSTANT varying CGV_INDEXPACKED* CMP_CONSTANT uniform ptr, CGV_FIXUPINDEX idx)
{
    return ptr[idx]; // (perf warning expected)
}
#endif
#endif

CGV_MASK icmp_encode_apply_swap_mode01237(CGV_EPOCODE qep[], CGV_INDEXPACKED color_index[2], CGU_INT blockMode, CGV_PARTID part_id)
{
    CGU_INT bits = 2;  if (blockMode == 0 || blockMode == 1) bits = 3;
    CGU_INT maxSubSets = 2; if (blockMode == 0 || blockMode == 2) maxSubSets = 3;

    CGV_MASK flips = 0;
    CGU_INT levels = 1 << bits;
    CGV_FIXUPINDEX fixup[3];
    icmp_get_fixuptable(fixup, part_id);

    for (CGU_INT j = 0; j < maxSubSets; j++)
    {
        CGV_FIXUPINDEX k0 = fixup[j];

#ifdef USE_VARYING
        CGV_INDEXPACKED q = ((gather_packedindex(color_index, k0 >> 3) << (28 - (k0 & 7) * 4)) >> 28);
#else
        CGV_INDEXPACKED q = ((color_index[k0 >> 3] << (28 - (k0 & 7) * 4)) >> 28);
#endif

        if (q >= levels / 2)
        {
            icmp_swap_epocode(&qep[8 * j], &qep[8 * j + 4], 4);
            CGV_MASK partition_mask = icmp_get_partition_mask(part_id, j);
            flips |= partition_mask;
        }
    }

    return flips;
}

void icmp_encode_mode01237(CGV_CMPOUTPACKED cmp_out[5], CGV_EPOCODE color_qendpoint[], CGV_INDEXPACKED color_index[2], CGV_PARTID part_id, CGU_INT blockMode)
{
    CGU_INT bits = 2; if (blockMode == 0 || blockMode == 1) bits = 3;
    CGU_INT maxSubSets = 2; if (blockMode == 0 || blockMode == 2) maxSubSets = 3;
    CGU_INT channels = 3; if (blockMode == 7) channels = 4;

    CGV_MASK flips = icmp_encode_apply_swap_mode01237(color_qendpoint, color_index, blockMode, part_id);

    for (CGU_INT k = 0; k < 5; k++) cmp_out[k] = 0;
    CGU_INT pos = 0;

    // mode 0-3, 7
    icmp_Write32Bit(cmp_out, &pos, blockMode + 1, 1 << blockMode);

    // partition
    if (blockMode == 0)
    {
        icmp_Write32Bit(cmp_out, &pos, 4, part_id & 15);
    }
    else
    {
        icmp_Write32Bit(cmp_out, &pos, 6, part_id & 63);
    }

    // endpoints
    for (CGU_INT ch = 0; ch < channels; ch++)
        for (CGU_INT j = 0; j < maxSubSets * 2; j++)
        {
            if (blockMode == 0)
            {
                icmp_Write32Bit(cmp_out, &pos, 4, color_qendpoint[j * 4 + 0 + ch] >> 1);
            }
            else if (blockMode == 1)
            {
                icmp_Write32Bit(cmp_out, &pos, 6, color_qendpoint[j * 4 + 0 + ch] >> 1);
            }
            else if (blockMode == 2)
            {
                icmp_Write32Bit(cmp_out, &pos, 5, color_qendpoint[j * 4 + 0 + ch]);
            }
            else if (blockMode == 3)
            {
                icmp_Write32Bit(cmp_out, &pos, 7, color_qendpoint[j * 4 + 0 + ch] >> 1);
            }
            else if (blockMode == 7)
            {
                icmp_Write32Bit(cmp_out, &pos, 5, color_qendpoint[j * 4 + 0 + ch] >> 1);
            }
            //else
            //{
            //    assert(false);
            //}
        }

    // p bits
    if (blockMode == 1)
        for (CGU_INT j = 0; j < 2; j++)
        {
            icmp_Write32Bit(cmp_out, &pos, 1, color_qendpoint[j * 8] & 1);
        }

    if (blockMode == 0 || blockMode == 3 || blockMode == 7)
        for (CGU_INT j = 0; j < maxSubSets * 2; j++)
        {
            icmp_Write32Bit(cmp_out, &pos, 1, color_qendpoint[j * 4] & 1);
        }

    // quantized values
    icmp_bc7_encode_endpoint2(cmp_out, &pos, color_index, bits, flips);
    icmp_bc7_encode_adjust_skip_mode01237_2(cmp_out, blockMode, part_id);
}

INLINE void icmp_swap_indexpacked(CGV_INDEXPACKED u[], CGV_INDEXPACKED v[], CGU_INT n)
{
    for (CGU_INT i = 0; i < n; i++)
    {
        CGV_INDEXPACKED t = u[i];
        u[i] = v[i];
        v[i] = t;
    }
}


void icmp_encode_mode4(CGV_CMPOUTPACKED cmp_out[5], varying cmp_mode_parameters* uniform params)
{
    CGV_EPOCODE      color_qendpoint[8];
    CGV_INDEXPACKED   color_index[2];
    CGV_EPOCODE      alpha_qendpoint[2];
    CGV_INDEXPACKED   alpha_index[2];

    CGV_CMPOUTPACKED      rotated_channel = params->rotated_channel;
    CGV_SHIFT32           idxMode = params->idxMode;

    icmp_swap_epocode(params->color_qendpoint, color_qendpoint, 8);
    icmp_swap_indexpacked(params->best_color_index, color_index, 2);
    icmp_swap_epocode(params->alpha_qendpoint, alpha_qendpoint, 2);
    icmp_swap_indexpacked(params->best_alpha_index, alpha_index, 2);

    for (CGU_INT k = 0; k < 5; k++) cmp_out[k] = 0;
    CGU_INT pos = 0;

    // mode 4 (5 bits) 00001
    icmp_Write32Bit(cmp_out, &pos, 5, 16);

    // rotation channel 2 bits
    icmp_Write32Bit(cmp_out, &pos, 2, (rotated_channel + 1) & 3);

    // idxMode 1 bit
    icmp_Write32Bit(cmp_out, &pos, 1, idxMode);

    if (!idxMode)
    {
        icmp_encode_apply_swap(color_qendpoint, 4, color_index, 2);
        icmp_encode_apply_swap(alpha_qendpoint, 1, alpha_index, 3);
    }
    else
    {
        icmp_swap_indexpacked(color_index, alpha_index, 2);
        icmp_encode_apply_swap(alpha_qendpoint, 1, color_index, 2);
        icmp_encode_apply_swap(color_qendpoint, 4, alpha_index, 3);
    }

    // color endpoints 5 bits each
    // R0 : R1
    // G0 : G1
    // B0 : B1
    for (CGU_INT p = 0; p < 3; p++)
    {
        CGV_EPOCODE c0 = color_qendpoint[0 + p];
        CGV_EPOCODE c1 = color_qendpoint[4 + p];
        icmp_Write32Bit(cmp_out, &pos, 5, c0);  // 0
        icmp_Write32Bit(cmp_out, &pos, 5, c1);  // 1
    }

    // alpha endpoints (6 bits each)
    // A0 : A1
    icmp_Write32Bit(cmp_out, &pos, 6, alpha_qendpoint[0]);
    icmp_Write32Bit(cmp_out, &pos, 6, alpha_qendpoint[1]);

    // index data (color index 2 bits each) 31 bits total
    icmp_encode_index(cmp_out, &pos, color_index, 2, 0);

    // index data (alpha index 3 bits each)  47 bits total
    icmp_encode_index(cmp_out, &pos, alpha_index, 3, 0);
}

void icmp_Encode_mode5(CGV_CMPOUTPACKED cmp_out[5], varying cmp_mode_parameters* uniform params)
{

    CGV_EPOCODE           qep[8];
    CGV_INDEXPACKED       color_index[2];
    CGV_EPOCODE           alpha_qendpoint[2];
    CGV_INDEXPACKED       alpha_index[2];

    icmp_swap_epocode(params->color_qendpoint, qep, 8);
    icmp_swap_indexpacked(params->best_color_index, color_index, 2);
    icmp_swap_epocode(params->alpha_qendpoint, alpha_qendpoint, 2);
    icmp_swap_indexpacked(params->best_alpha_index, alpha_index, 2);

    CGV_CMPOUTPACKED rotated_channel = params->rotated_channel;

    icmp_encode_apply_swap(qep, 4, color_index, 2);
    icmp_encode_apply_swap(alpha_qendpoint, 1, alpha_index, 2);

    for (CGU_INT k = 0; k < 5; k++) cmp_out[k] = 0;
    CGU_INT pos = 0;

    // mode 5
    icmp_Write32Bit(cmp_out, &pos, 6, 1 << 5);

    // rotated channel
    icmp_Write32Bit(cmp_out, &pos, 2, (rotated_channel + 1) & 3);

    // endpoints
    for (CGU_INT p = 0; p < 3; p++)
    {
        icmp_Write32Bit(cmp_out, &pos, 7, qep[0 + p]);
        icmp_Write32Bit(cmp_out, &pos, 7, qep[4 + p]);
    }

    // alpha endpoints
    icmp_Write32Bit(cmp_out, &pos, 8, alpha_qendpoint[0]);
    icmp_Write32Bit(cmp_out, &pos, 8, alpha_qendpoint[1]);

    // quantized values
    icmp_encode_index(cmp_out, &pos, color_index, 2, 0);
    icmp_encode_index(cmp_out, &pos, alpha_index, 2, 0);

}

void icmp_encode_mode6(CGV_CMPOUTPACKED cmp_out[5], CGV_EPOCODE qep[8], CGV_INDEXPACKED color_index[2])
{
    icmp_encode_apply_swap(qep, 4, color_index, 4);

    for (CGU_INT k = 0; k < 5; k++) cmp_out[k] = 0;
    CGU_INT pos = 0;

    // mode 6
    icmp_Write32Bit(cmp_out, &pos, 7, 64);

    // endpoints
    for (CGU_INT p = 0; p < 4; p++)
    {
        icmp_Write32Bit(cmp_out, &pos, 7, qep[0 + p] >> 1);
        icmp_Write32Bit(cmp_out, &pos, 7, qep[4 + p] >> 1);
    }

    // p bits
    icmp_Write32Bit(cmp_out, &pos, 1, qep[0] & 1);
    icmp_Write32Bit(cmp_out, &pos, 1, qep[4] & 1);

    // quantized values
    icmp_encode_index(cmp_out, &pos, color_index, 4, 0);
}

///////////////////////////
//      PCA helpers

INLINE void icmp_compute_stats_masked(CGV_IMAGE stats[15], CGV_IMAGE image_src[64], CGV_MASK mask, CGU_CHANNEL channels)
{
    for (CGU_INT i = 0; i < 15; i++) stats[i] = 0;

    CGV_MASK mask_shifted = mask << 1;
    for (CGU_INT k = 0; k < 16; k++)
    {
        mask_shifted >>= 1;
        //if ((mask_shifted&1) == 0) continue;
        CGV_MASK flag = (mask_shifted & 1);

        CGV_IMAGE rgba[4];
        for (CGU_CHANNEL ch = 0; ch < channels; ch++) rgba[ch] = image_src[k + ch * 16];

        for (CGU_CHANNEL ch = 0; ch < channels; ch++) rgba[ch] *= flag;
        stats[14] += flag;

        stats[10] += rgba[0];
        stats[11] += rgba[1];
        stats[12] += rgba[2];

        stats[0] += rgba[0] * rgba[0];
        stats[1] += rgba[0] * rgba[1];
        stats[2] += rgba[0] * rgba[2];

        stats[4] += rgba[1] * rgba[1];
        stats[5] += rgba[1] * rgba[2];

        stats[7] += rgba[2] * rgba[2];

        if (channels == 4)
        {
            stats[13] += rgba[3];

            stats[3] += rgba[0] * rgba[3];
            stats[6] += rgba[1] * rgba[3];
            stats[8] += rgba[2] * rgba[3];
            stats[9] += rgba[3] * rgba[3];
        }
    }
}

INLINE void icmp_covar_from_stats(CGV_IMAGE covar[10], CGV_IMAGE stats[15], CGU_CHANNEL channels3or4)
{
    covar[0] = stats[0] - stats[10 + 0] * stats[10 + 0] / stats[14];
    covar[1] = stats[1] - stats[10 + 0] * stats[10 + 1] / stats[14];
    covar[2] = stats[2] - stats[10 + 0] * stats[10 + 2] / stats[14];

    covar[4] = stats[4] - stats[10 + 1] * stats[10 + 1] / stats[14];
    covar[5] = stats[5] - stats[10 + 1] * stats[10 + 2] / stats[14];

    covar[7] = stats[7] - stats[10 + 2] * stats[10 + 2] / stats[14];

    if (channels3or4 == 4)
    {
        covar[3] = stats[3] - stats[10 + 0] * stats[10 + 3] / stats[14];
        covar[6] = stats[6] - stats[10 + 1] * stats[10 + 3] / stats[14];
        covar[8] = stats[8] - stats[10 + 2] * stats[10 + 3] / stats[14];
        covar[9] = stats[9] - stats[10 + 3] * stats[10 + 3] / stats[14];
    }
}

INLINE void icmp_compute_covar_dc_masked(CGV_IMAGE covar[6], CGV_IMAGE dc[3], CGV_IMAGE image_src[64], CGV_MASK mask, CGU_INT channels3or4)
{
    CGV_IMAGE stats[15];
    icmp_compute_stats_masked(stats, image_src, mask, channels3or4);

    icmp_covar_from_stats(covar, stats, channels3or4);
    for (CGU_INT ch = 0; ch < channels3or4; ch++) dc[ch] = stats[10 + ch] / stats[14];
}

INLINE void icmp_ssymv3(CGV_IMAGE a[4], CGV_IMAGE covar[10], CGV_IMAGE b[4])
{
    a[0] = covar[0] * b[0] + covar[1] * b[1] + covar[2] * b[2];
    a[1] = covar[1] * b[0] + covar[4] * b[1] + covar[5] * b[2];
    a[2] = covar[2] * b[0] + covar[5] * b[1] + covar[7] * b[2];
}

INLINE void icmp_ssymv4_2(CGV_IMAGE a[4], CGV_IMAGE covar[10], CGV_IMAGE b[4])
{
    a[0] = covar[0] * b[0] + covar[1] * b[1] + covar[2] * b[2] + covar[3] * b[3];
    a[1] = covar[1] * b[0] + covar[4] * b[1] + covar[5] * b[2] + covar[6] * b[3];
    a[2] = covar[2] * b[0] + covar[5] * b[1] + covar[7] * b[2] + covar[8] * b[3];
    a[3] = covar[3] * b[0] + covar[6] * b[1] + covar[8] * b[2] + covar[9] * b[3];
}

#ifndef ASPM
// Computes inverse square root over an implementation-defined range. The maximum error is implementation-defined.
CGV_IMAGE Image_rsqrt(CGV_IMAGE f)
{
    CGV_IMAGE sf = sqrt(f);
    if (sf != 0)
        return 1 / sqrt(f);
    else
        return 0.0f;
}
#endif

INLINE void icmp_compute_axis(CGV_IMAGE axis[4], 
                              CGV_IMAGE covar[10], 
#ifdef ASPM_GPU
                              CGV_ITTERATIONS powerIterations, 
#else
                              uniform __constant CGV_ITTERATIONS powerIterations, 
#endif
                              CGU_CHANNEL channels)
{
    CGV_IMAGE vec[4] = { 1,1,1,1 };

    for (CGU_INT i = 0; i < powerIterations; i++)
    {
        if (channels == 3) icmp_ssymv3(axis, covar, vec);
        if (channels == 4) icmp_ssymv4_2(axis, covar, vec);

        for (CGU_CHANNEL ch = 0; ch < channels; ch++) vec[ch] = axis[ch];

        if (i % 2 == 1) // renormalize every other iteration
        {
            CGV_IMAGE norm_sq = 0;
            for (CGU_CHANNEL ch = 0; ch < channels; ch++)
                norm_sq += axis[ch] * axis[ch];

#ifndef ASPM
            CGV_IMAGE rnorm = Image_rsqrt(norm_sq);
#else
            CGV_IMAGE rnorm = rsqrt(norm_sq);
#endif
            for (CGU_CHANNEL ch = 0; ch < channels; ch++) vec[ch] *= rnorm;
        }
    }

    for (CGU_CHANNEL ch = 0; ch < channels; ch++) axis[ch] = vec[ch];
}

void icmp_block_pca_axis(CGV_IMAGE axis[4], CGV_IMAGE dc[4], CGV_IMAGE image_src[64], CGV_MASK mask, CGU_INT channels3or4)
{
    uniform __constant CGV_ITTERATIONS powerIterations = 8; // 4 not enough for HQ

    CGV_IMAGE covar[10];
    icmp_compute_covar_dc_masked(covar, dc, image_src, mask, channels3or4);

    CGV_IMAGE inv_var = 1.0 / (256 * 256);
    for (CGU_INT k = 0; k < 10; k++)
    {
        covar[k] *= inv_var;
    }

    CGV_IMAGE eps = sq_image(0.001F);
    covar[0] += eps;
    covar[4] += eps;
    covar[7] += eps;
    covar[9] += eps;

    icmp_compute_axis(axis, covar, powerIterations, channels3or4);
}

CGV_IMAGE minImage(CGV_IMAGE a, CGV_IMAGE b) { return a < b ? a : b; }
CGV_IMAGE maxImage(CGV_IMAGE a, CGV_IMAGE b) { return a > b ? a : b; }


void icmp_block_segment_core(CGV_IMAGE epo_code[], CGV_IMAGE image_src[64], CGV_MASK mask, CGU_INT channels3or4)
{
    CGV_IMAGE axis[4];
    CGV_IMAGE dc[4];
    icmp_block_pca_axis(axis, dc, image_src, mask, channels3or4);

    CGV_IMAGE ext[2];
    ext[0] = +1e32;
    ext[1] = -1e32;

    // find min/max
    CGV_MASK mask_shifted = mask << 1;
    for (CGU_INT k = 0; k < 16; k++)
    {
        mask_shifted >>= 1;
        if ((mask_shifted & 1) == 0) continue;

        CGV_IMAGE dot = 0;
        for (CGU_INT ch = 0; ch < channels3or4; ch++)
            dot += axis[ch] * (image_src[16 * ch + k] - dc[ch]);

        ext[0] = minImage(ext[0], dot);
        ext[1] = maxImage(ext[1], dot);
    }

    // create some distance if the endpoints collapse
    if (ext[1] - ext[0] < 1.0f)
    {
        ext[0] -= 0.5f;
        ext[1] += 0.5f;
    }

    for (CGU_INT i = 0; i < 2; i++)
        for (CGU_INT ch = 0; ch < channels3or4; ch++)
        {
            epo_code[4 * i + ch] = ext[i] * axis[ch] + dc[ch];
        }
}

INLINE CGV_IMAGE clampf(CGV_IMAGE v, CGV_IMAGE a, CGV_IMAGE b)
{
    if (v < a)
        return a;
    else
        if (v > b)
            return b;
    return v;
}


void icmp_get_block_endpoints(CGV_IMAGE block_endpoints[], CGV_IMAGE image_src[64], CGV_MASK mask, CGU_CHANNEL channels3or4)
{
    icmp_block_segment_core(block_endpoints, image_src, mask, channels3or4);

    for (CGU_INT i = 0; i < 2; i++)
        for (CGU_INT ch = 0; ch < channels3or4; ch++)
        {
            block_endpoints[4 * i + ch] = clampf(block_endpoints[4 * i + ch], 0.0f, 255.0f);
        }
}

void icmp_ep_quant0367_2(CGV_EPOCODE qep[], CGV_IMAGE ep[], CGU_INT blockMode, CGU_INT channels)
{
    CGU_INT bits = 7;
    if (blockMode == 0) bits = 4;
    if (blockMode == 7) bits = 5;

    CGU_INT levels = 1 << bits;
    CGU_INT levels2 = levels * 2 - 1;

    for (CGU_INT i = 0; i < 2; i++)
    {
        CGV_EPOCODE qep_b[8];

        for (CGU_INT b = 0; b < 2; b++)
            for (CGU_INT p = 0; p < 4; p++)
            {
                CGV_EPOCODE v = (CGV_TYPEINT)((ep[i * 4 + p] / 255.0f*levels2 - b) / 2.0f + 0.5f) * 2 + b;
                qep_b[b * 4 + p] = clampEPO(v, b, levels2 - 1 + b);
            }

        CGV_IMAGE ep_b[8];
        for (CGU_INT j = 0; j < 8; j++)
            ep_b[j] = qep_b[j];

        if (blockMode == 0)
            for (CGU_INT j = 0; j < 8; j++)
                ep_b[j] = expandEPObits(qep_b[j], 5);

        CGV_ERROR err0 = 0.0f;
        CGV_ERROR err1 = 0.0f;
        for (CGU_INT ch = 0; ch < channels; ch++)
        {
            err0 += sq_image(ep[i * 4 + ch] - ep_b[0 + ch]);
            err1 += sq_image(ep[i * 4 + ch] - ep_b[4 + ch]);
        }

        for (CGU_INT p = 0; p < 4; p++)
            qep[i * 4 + p] = (err0 < err1) ? qep_b[0 + p] : qep_b[4 + p];
    }
}

void icmp_ep_quant245_2(CGV_EPOCODE qep[], CGV_IMAGE ep[], CGU_INT mode)
{
    CGU_INT bits = 5;
    if (mode == 5) bits = 7;
    CGU_INT levels = 1 << bits;

    for (CGU_INT i = 0; i < 8; i++)
    {
        CGV_EPOCODE v = ((CGV_TYPEINT)(ep[i] / 255.0f*(levels - 1) + 0.5));
        qep[i] = clampEPO(v, 0, levels - 1);
    }
}

void icmp_ep_quant1_2(CGV_EPOCODE qep[], CGV_IMAGE ep[], CGU_INT mode)
{
    CGV_EPOCODE qep_b[16];

    for (CGU_INT b = 0; b < 2; b++)
        for (CGU_INT i = 0; i < 8; i++)
        {
            CGV_EPOCODE v = ((CGV_TYPEINT)((ep[i] / 255.0f*127.0f - b) / 2 + 0.5)) * 2 + b;
            qep_b[b * 8 + i] = clampEPO(v, b, 126 + b);
        }

    // dequant
    CGV_IMAGE ep_b[16];
    for (CGU_INT k = 0; k < 16; k++)
        ep_b[k] = expandEPObits(qep_b[k], 7);

    CGV_ERROR err0 = 0.0f;
    CGV_ERROR err1 = 0.0f;
    for (CGU_INT j = 0; j < 2; j++)
        for (CGU_INT p = 0; p < 3; p++)
        {
            err0 += sq_image(ep[j * 4 + p] - ep_b[0 + j * 4 + p]);
            err1 += sq_image(ep[j * 4 + p] - ep_b[8 + j * 4 + p]);
        }

    for (CGU_INT i = 0; i < 8; i++)
        qep[i] = (err0 < err1) ? qep_b[0 + i] : qep_b[8 + i];

}

void icmp_ep_quant2_2(CGV_EPOCODE qep[], CGV_IMAGE ep[], CGU_INT blockMode, CGU_INT channels3or4)
{
    //assert(mode <= 7);
    CMP_STATIC uniform __constant CGV_SUBSETS SubSetTable[] = { 3,2,3,2,1,1,1,2 };
#ifndef ASPM_GPU
    uniform CMP_CONSTANT 
#endif
    CGV_SUBSETS maxSubSets = SubSetTable[blockMode];

    if (blockMode == 0 || blockMode == 3 || blockMode == 6 || blockMode == 7)
    {
        for (CGU_INT i = 0; i < maxSubSets; i++)
            icmp_ep_quant0367_2(&qep[i * 8], &ep[i * 8], blockMode, channels3or4);
    }
    else
        if (blockMode == 1)
        {
            for (CGU_INT i = 0; i < maxSubSets; i++)
                icmp_ep_quant1_2(&qep[i * 8], &ep[i * 8], blockMode);
        }
        else
            if (blockMode == 2 || blockMode == 4 || blockMode == 5)
            {
                for (CGU_INT i = 0; i < maxSubSets; i++)
                    icmp_ep_quant245_2(&qep[i * 8], &ep[i * 8], blockMode);
            }
    //   else 
    //      assert(false);

}

void icmp_ep_dequant2(CGV_IMAGE ep[], CGV_EPOCODE qep[], CGU_INT blockMode)
{
    //assert(mode <= 7);
    CMP_STATIC uniform __constant CGV_SUBSETS subSetTable[] = { 3,2,3,2,1,1,1,2 };
#ifndef ASPM_GPU
    uniform CMP_CONSTANT
#endif
    CGV_SUBSETS maxSubSets = subSetTable[blockMode];

    // mode 3, 6 are 8-bit
    if (blockMode == 3 || blockMode == 6)
    {
        for (CGU_INT i = 0; i < 8 * maxSubSets; i++)
            ep[i] = qep[i];
    }
    else
        if (blockMode == 1 || blockMode == 5)
        {
            for (CGU_INT i = 0; i < 8 * maxSubSets; i++)
                ep[i] = expandEPObits(qep[i], 7);
        }
        else
            if (blockMode == 0 || blockMode == 2 || blockMode == 4)
            {
                for (CGU_INT i = 0; i < 8 * maxSubSets; i++)
                    ep[i] = expandEPObits(qep[i], 5);
            }
            else
                if (blockMode == 7)
                {
                    for (CGU_INT i = 0; i < 8 * maxSubSets; i++)
                        ep[i] = expandEPObits(qep[i], 6);
                }
    //else 
    //  assert(false);
}

void icmp_GetQuantizedEpoCode(CGV_EPOCODE epo_code_out[], CGV_IMAGE block_endpoints[], CGU_INT blockMode, CGU_CHANNEL channels3or4)
{
    icmp_ep_quant2_2(epo_code_out, block_endpoints, blockMode, channels3or4);
    icmp_ep_dequant2(block_endpoints, epo_code_out, blockMode);
}

void icmp_ep_quant_dequant_mode4(CGV_EPOCODE qep[], CGV_IMAGE ep[])
{
    icmp_ep_quant2_2(qep, ep, 4, 3);
    icmp_ep_dequant2(ep, qep, 4);
}

///////////////////////////
//   pixel quantization
//========================================
// Modified Intel Texture Compression Code
//========================================

INLINE uniform __constant CGV_RAMP* uniform icmp_GetRamp(CGU_INT bits)
{
    //assert(bits>=2 && bits<=4); // invalid bit size

    CMP_STATIC uniform __constant CGV_RAMP unquant_table_2bits[] = { 0, 21, 43, 64 };
    CMP_STATIC uniform __constant CGV_RAMP unquant_table_3bits[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
    CMP_STATIC uniform __constant CGV_RAMP unquant_table_4bits[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    uniform __constant CGV_RAMP* uniform unquant_tables[] = { unquant_table_2bits, unquant_table_3bits, unquant_table_4bits };

    return unquant_tables[bits - 2];
}

#ifdef USE_VARYING
INLINE CGV_IMAGE  gather_image(varying CGV_IMAGE* uniform ptr, CGV_SHIFT32 idx)
{
    return ptr[idx]; // (perf warning expected)
}
#endif

INLINE CGV_RAMP  gather_ramp(
#ifdef ASPM_GPU
    CMP_CONSTANT CGV_RAMP*  ptr, 
#else
    CMP_CONSTANT CGV_RAMP* CMP_CONSTANT uniform ptr, 
#endif
    CGV_INDEX idx)
{
    return ptr[idx]; // (perf warning expected)
}

CGV_ERROR icmp_GetQuantizeIndex(
    CGV_INDEXPACKED index_packed_out[2],
    CGV_INDEX       index_out[MAX_SUBSET_SIZE],
    CGV_IMAGE       image_src[64],
    CGU_INT         bits,
    CGV_IMAGE       image_block[],
    CGV_SHIFT32     pattern,
    CGU_CHANNEL     channels3or4)
{
    CGV_ERROR total_err = 0;
    uniform __constant CGV_RAMP* uniform Ramp = icmp_GetRamp(bits);
    CGV_LEVELS levels = 1 << bits;

    // 64-bit color_qendpoint: 5% overhead in this function
    for (CGU_INT k = 0; k < 2; k++) index_packed_out[k] = 0;

    CGV_SHIFT32 pattern_shifted = pattern;
    for (CGU_INT k = 0; k < 16; k++)
    {
        CGV_SHIFT32 j = pattern_shifted & 3;
        pattern_shifted >>= 2;

        CGV_IMAGE proj = 0;
        CGV_IMAGE div = 0;
        for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++)
        {
#ifdef USE_VARYING
            CGV_IMAGE ep_a = gather_image(image_block, 8 * j + 0 + ch);
            CGV_IMAGE ep_b = gather_image(image_block, 8 * j + 4 + ch);
#else
            CGV_IMAGE ep_a = image_block[8 * j + 0 + ch];
            CGV_IMAGE ep_b = image_block[8 * j + 4 + ch];
#endif
            proj += (image_src[k + ch * 16] - ep_a)*(ep_b - ep_a);
            div += sq_image(ep_b - ep_a);
        }

        proj /= div;

        CGV_INDEX index_q1 = (CGV_INDEX)(proj*levels + 0.5);
        index_q1 = clampIndex(index_q1, 1, levels - 1);

        CGV_ERROR err0 = 0;
        CGV_ERROR err1 = 0;
        CGV_RAMP ramp0 = gather_ramp(Ramp, index_q1 - 1);
        CGV_RAMP ramp1 = gather_ramp(Ramp, index_q1);

        for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++)
        {
#ifdef USE_VARYING
            CGV_IMAGE ep_a = gather_image(image_block, 8 * j + 0 + ch);
            CGV_IMAGE ep_b = gather_image(image_block, 8 * j + 4 + ch);
#else
            CGV_IMAGE ep_a = image_block[8 * j + 0 + ch];
            CGV_IMAGE ep_b = image_block[8 * j + 4 + ch];
#endif
            CGV_IMAGE dec_v0 = (CGV_TYPEINT)(((64 - ramp0)*ep_a + ramp0 * ep_b + 32) / 64);
            CGV_IMAGE dec_v1 = (CGV_TYPEINT)(((64 - ramp1)*ep_a + ramp1 * ep_b + 32) / 64);
            err0 += sq_image(dec_v0 - image_src[k + ch * 16]);
            err1 += sq_image(dec_v1 - image_src[k + ch * 16]);
        }

        CGV_ERROR best_err = err1;
        CGV_INDEX best_index = index_q1;
        if (err0 < err1)
        {
            best_err = err0;
            best_index = index_q1 - 1;
        }

        index_out[k] = best_index;
        index_packed_out[k / 8] += ((CGV_INDEXPACKED)best_index) << 4 * (k % 8);
        total_err += best_err;
    }

    return total_err;
}

///////////////////////////
// LS endpoint refinement

void icmp_opt_endpoints(CGV_IMAGE ep[], CGV_IMAGE image_src[64], CGU_INT bits, CGV_INDEXPACKED color_qendpoint[2], CGV_MASK mask, CGU_CHANNEL channels3or4)
{
    CGU_INT levels = 1 << bits;

    CGV_IMAGE Atb1[4] = { 0,0,0,0 };
    CGV_IMAGE sum_q = 0;
    CGV_IMAGE sum_qq = 0;
    CGV_IMAGE sum[5] = { 0,0,0,0,0 };

    CGV_MASK mask_shifted = mask << 1;
    for (CGU_INT k1 = 0; k1 < 2; k1++)
    {
        CGV_INDEXPACKED qbits_shifted = color_qendpoint[k1];
        for (CGU_INT k2 = 0; k2 < 8; k2++)
        {
            CGU_INT   k = k1 * 8 + k2;
            CGV_IMAGE q = (CGV_TYPEINT)(qbits_shifted & 15);

            qbits_shifted >>= 4;

            mask_shifted >>= 1;
            if ((mask_shifted & 1) == 0) continue;

            CGV_LEVELS x = (levels - 1) - q;
            CGV_LEVELS y = q;

            sum_q += q;
            sum_qq += q * q;

            sum[4] += 1;
            for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++)  sum[ch] += image_src[k + ch * 16];
            for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++) Atb1[ch] += x * image_src[k + ch * 16];
        }
    }

    CGV_IMAGE Atb2[4];
    for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++)
    {
        //sum[ch] = dc[ch]*16;
        Atb2[ch] = (levels - 1)*sum[ch] - Atb1[ch];
    }

    CGV_IMAGE Cxx = sum[4] * sq_image(levels - 1) - 2 * (levels - 1)*sum_q + sum_qq;
    CGV_IMAGE Cyy = sum_qq;
    CGV_IMAGE Cxy = (levels - 1)*sum_q - sum_qq;
    CGV_IMAGE scale = (levels - 1) / (Cxx*Cyy - Cxy * Cxy);

    for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++)
    {
        ep[0 + ch] = (Atb1[ch] * Cyy - Atb2[ch] * Cxy)*scale;
        ep[4 + ch] = (Atb2[ch] * Cxx - Atb1[ch] * Cxy)*scale;

        //ep[0+ch] = clamp(ep[0+ch], 0, 255);
        //ep[4+ch] = clamp(ep[4+ch], 0, 255);
    }

    if (img_absf(Cxx*Cyy - Cxy * Cxy) < 0.001f)
    {
        // flatten
        for (CGU_CHANNEL ch = 0; ch < channels3or4; ch++)
        {
            ep[0 + ch] = sum[ch] / sum[4];
            ep[4 + ch] = ep[0 + ch];
        }
    }
}

//////////////////////////
// parameter estimation

void icmp_channel_quant_dequant2(CGV_EPOCODE qep[2], CGV_IMAGE ep[2], CGU_INT epbits)
{
    CGV_LEVELS elevels = (1 << epbits);

    for (CGU_INT i = 0; i < 2; i++)
    {
        CGV_EPOCODE v = ((CGV_EPOCODE)(ep[i] / 255.0f*(elevels - 1) + 0.5f));
        qep[i] = clampEPO(v, 0, elevels - 1);
        ep[i] = expandEPObits(qep[i], epbits);
    }
}

void icmp_refineEndpoints(CGV_IMAGE ep[2], CGV_IMAGE block[16], CGU_INT bits, CGV_INDEXPACKED color_index[2])
{
    CGU_INT levels = 1 << bits;

    CGV_IMAGE Atb1 = 0;
    CGV_IMAGE sum_q = 0;
    CGV_IMAGE sum_qq = 0;
    CGV_IMAGE sum = 0;

    for (CGU_INT k1 = 0; k1 < 2; k1++)
    {
        CGV_INDEXPACKED qbits_shifted = color_index[k1];
        for (CGU_INT k2 = 0; k2 < 8; k2++)
        {
            CGU_INT   k = k1 * 8 + k2;
            CGV_IMAGE q = (CGV_TYPEINT)(qbits_shifted & 15);
            qbits_shifted >>= 4;

            CGV_TYPEINT x = (levels - 1) - q;
            CGV_TYPEINT y = q;

            sum_q += q;
            sum_qq += q * q;

            sum += block[k];
            Atb1 += x * block[k];
        }
    }

    CGV_IMAGE Atb2 = (levels - 1)*sum - Atb1;

    CGV_IMAGE Cxx = 16 * sq_image(levels - 1) - 2 * (levels - 1)*sum_q + sum_qq;
    CGV_IMAGE Cyy = sum_qq;
    CGV_IMAGE Cxy = (levels - 1)*sum_q - sum_qq;
    CGV_IMAGE scale = (levels - 1) / (Cxx*Cyy - Cxy * Cxy);

    ep[0] = (Atb1*Cyy - Atb2 * Cxy)*scale;
    ep[1] = (Atb2*Cxx - Atb1 * Cxy)*scale;

    ep[0] = clampf(ep[0], 0.0f, 255.0f);
    ep[1] = clampf(ep[1], 0.0f, 255.0f);

    if (img_absf(Cxx*Cyy - Cxy * Cxy) < 0.001)
    {
        ep[0] = sum / 16;
        ep[1] = ep[0];
    }
}

CGV_ERROR  icmp_channelQuantizeIndex(CGV_INDEXPACKED color_index[2], CGV_INDEX index[MAX_SUBSET_SIZE], CGV_IMAGE block[16], CGU_INT bits, CGV_IMAGE ep[])
{
    uniform __constant CGV_RAMP* uniform Ramp = icmp_GetRamp(bits);
    CGV_LEVELS levels = (1 << bits);

    color_index[0] = 0;
    color_index[1] = 0;

    CGV_ERROR total_err = 0;

    for (CGU_INT k = 0; k < 16; k++)
    {
        CGV_IMAGE proj = (block[k] - ep[0]) / (ep[1] - ep[0] + 0.001f);

        CGV_INDEX q1 = (CGV_TYPEINT)(proj*levels + 0.5);
        q1 = clampEPO(q1, 1, levels - 1);

        CGV_ERROR err0 = 0;
        CGV_ERROR err1 = 0;
        CGV_RAMP  ramp0 = gather_ramp(Ramp, q1 - 1);
        CGV_RAMP  ramp1 = gather_ramp(Ramp, q1);

        CGV_IMAGE dec_v0 = (CGV_TYPEINT)(((64 - ramp0)*ep[0] + ramp0 * ep[1] + 32) / 64);
        CGV_IMAGE dec_v1 = (CGV_TYPEINT)(((64 - ramp1)*ep[0] + ramp1 * ep[1] + 32) / 64);
        err0 += sq_image(dec_v0 - block[k]);
        err1 += sq_image(dec_v1 - block[k]);

        CGV_TYPEINT best_err = err1;
        CGV_INDEX   best_q = q1;
        if (err0 < err1)
        {
            best_err = err0;
            best_q = q1 - 1;
        }

        index[k] = best_q;
        color_index[k / 8] += ((CGV_INDEXPACKED)best_q) << 4 * (k % 8);
        total_err += best_err;
    }

    return total_err;
}

CGV_ERROR  icmp_optQuantizeIndex(BC7_EncodeState  EncodeState[], CGV_INDEXPACKED color_index[2], CGV_INDEX index[MAX_SUBSET_SIZE], CGV_EPOCODE qep[2], CGV_IMAGE block[16], CGU_INT bits, CGU_INT epbits)
{
    CGV_IMAGE ep[2] = { 255,0 };

    for (CGU_INT k = 0; k < 16; k++)
    {
        ep[0] = minImage(ep[0], block[k]);
        ep[1] = maxImage(ep[1], block[k]);
    }

    icmp_channel_quant_dequant2(qep, ep, epbits);
    CGV_ERROR err = icmp_channelQuantizeIndex(color_index, index, block, bits, ep);

    // refine
#ifndef ASPM_GPU
    uniform CMP_CONSTANT
#endif
    CGV_ITTERATIONS refineIterations = EncodeState->refineIterations;
    for (CGU_INT i = 0; i < refineIterations; i++)
    {
        icmp_refineEndpoints(ep, block, bits, color_index);
        icmp_channel_quant_dequant2(qep, ep, epbits);
        err = icmp_channelQuantizeIndex(color_index, index, block, bits, ep);
    }

    return err;
}


INLINE CGV_SHIFT32 icmp_get_pattern2(CGV_PARTID part_id)
{
    CMP_STATIC uniform __constant CGV_SHIFT32 pattern_table[] = {
       0x50505050u, 0x40404040u, 0x54545454u, 0x54505040u, 0x50404000u, 0x55545450u, 0x55545040u, 0x54504000u,
       0x50400000u, 0x55555450u, 0x55544000u, 0x54400000u, 0x55555440u, 0x55550000u, 0x55555500u, 0x55000000u,
       0x55150100u, 0x00004054u, 0x15010000u, 0x00405054u, 0x00004050u, 0x15050100u, 0x05010000u, 0x40505054u,
       0x00404050u, 0x05010100u, 0x14141414u, 0x05141450u, 0x01155440u, 0x00555500u, 0x15014054u, 0x05414150u,
       0x44444444u, 0x55005500u, 0x11441144u, 0x05055050u, 0x05500550u, 0x11114444u, 0x41144114u, 0x44111144u,
       0x15055054u, 0x01055040u, 0x05041050u, 0x05455150u, 0x14414114u, 0x50050550u, 0x41411414u, 0x00141400u,
       0x00041504u, 0x00105410u, 0x10541000u, 0x04150400u, 0x50410514u, 0x41051450u, 0x05415014u, 0x14054150u,
       0x41050514u, 0x41505014u, 0x40011554u, 0x54150140u, 0x50505500u, 0x00555050u, 0x15151010u, 0x54540404u,
       0xAA685050u, 0x6A5A5040u, 0x5A5A4200u, 0x5450A0A8u, 0xA5A50000u, 0xA0A05050u, 0x5555A0A0u, 0x5A5A5050u,
       0xAA550000u, 0xAA555500u, 0xAAAA5500u, 0x90909090u, 0x94949494u, 0xA4A4A4A4u, 0xA9A59450u, 0x2A0A4250u,
       0xA5945040u, 0x0A425054u, 0xA5A5A500u, 0x55A0A0A0u, 0xA8A85454u, 0x6A6A4040u, 0xA4A45000u, 0x1A1A0500u,
       0x0050A4A4u, 0xAAA59090u, 0x14696914u, 0x69691400u, 0xA08585A0u, 0xAA821414u, 0x50A4A450u, 0x6A5A0200u,
       0xA9A58000u, 0x5090A0A8u, 0xA8A09050u, 0x24242424u, 0x00AA5500u, 0x24924924u, 0x24499224u, 0x50A50A50u,
       0x500AA550u, 0xAAAA4444u, 0x66660000u, 0xA5A0A5A0u, 0x50A050A0u, 0x69286928u, 0x44AAAA44u, 0x66666600u,
       0xAA444444u, 0x54A854A8u, 0x95809580u, 0x96969600u, 0xA85454A8u, 0x80959580u, 0xAA141414u, 0x96960000u,
       0xAAAA1414u, 0xA05050A0u, 0xA0A5A5A0u, 0x96000000u, 0x40804080u, 0xA9A8A9A8u, 0xAAAAAA44u, 0x2A4A5254u
    };

    return gather_uint32(pattern_table, part_id);
}

CGV_IMAGE  icmp_get_pca_bound(CGV_IMAGE covar[10], CGU_CHANNEL channels)
{
    uniform __constant CGV_TYPEINT powerIterations = 4; // quite approximative, but enough for bounding

    CGV_IMAGE inv_var = 1.0 / (256 * 256);
    for (CGU_INT k = 0; k < 10; k++)
    {
        covar[k] *= inv_var;
    }

    CGV_IMAGE eps = sq_image(0.001);
    covar[0] += eps;
    covar[4] += eps;
    covar[7] += eps;

    CGV_IMAGE axis[4];
    icmp_compute_axis(axis, covar, powerIterations, channels);

    CGV_IMAGE vec[4];
    if (channels == 3) icmp_ssymv3(vec, covar, axis);
    if (channels == 4) icmp_ssymv4_2(vec, covar, axis);

    CGV_IMAGE sq_sum = 0.0f;
    for (CGU_INT p = 0; p < channels; p++) sq_sum += sq_image(vec[p]);
    CGV_IMAGE lambda = sqrt(sq_sum);

    CGV_IMAGE bound = covar[0] + covar[4] + covar[7];
    if (channels == 4) bound += covar[9];
    bound -= lambda;
    bound = maxImage(bound, 0.0f);

    return bound;
}

CGV_IMAGE icmp_block_pca_bound_split2(CGV_IMAGE image_src[64], CGV_MASK mask, CGV_IMAGE full_stats[15], CGU_CHANNEL channels)
{
    CGV_IMAGE stats[15];
    icmp_compute_stats_masked(stats, image_src, mask, channels);

    CGV_IMAGE covar1[10];
    icmp_covar_from_stats(covar1, stats, channels);

    for (CGU_INT i = 0; i < 15; i++)
        stats[i] = full_stats[i] - stats[i];

    CGV_IMAGE covar2[10];
    icmp_covar_from_stats(covar2, stats, channels);

    CGV_IMAGE bound = 0.0f;
    bound += icmp_get_pca_bound(covar1, channels);
    bound += icmp_get_pca_bound(covar2, channels);

    return sqrt(bound) * 256;
}


#ifdef USE_VARYING
INLINE void       scatter_partid(varying CGV_PARTID* uniform ptr, CGV_TYPEINT idx, CGV_PARTID value)
{
    ptr[idx] = value; // (perf warning expected)
}
#endif

void icmp_sort_partlist(CGV_PARTID list[], CGU_INT length, CGU_INT partial_count)
{
    for (CGU_INT k = 0; k < partial_count; k++)
    {
        CGV_TYPEINT  best_idx = k;
        CGV_PARTID   best_value = list[k];
        for (CGU_INT i = k + 1; i < length; i++)
        {
            if (best_value > list[i])
            {
                best_value = list[i];
                best_idx = i;
            }
        }

        // swap
#ifdef USE_VARYING
        scatter_partid(list, best_idx, list[k]);
#else
        list[best_idx] = list[k];
#endif
        list[k] = best_value;
    }
}

INLINE void copy_epocode(CGV_EPOCODE u[], CGV_EPOCODE v[], CGU_INT n)
{
    for (CGU_INT i = 0; i < n; i++)
    {
        u[i] = v[i];
    }
}


INLINE void copy_indexpacked(CGV_INDEXPACKED u[], CGV_INDEXPACKED v[], CGU_INT n)
{
    for (CGU_INT i = 0; i < n; i++)
    {
        u[i] = v[i];
    }
}


void icmp_enc_mode4_candidate(
    BC7_EncodeState     EncodeState[],
    cmp_mode_parameters best_candidate[],
    CGV_ERROR           best_err[],
    CGU_INT             rotated_channel,
    CGU_INT             idxMode)
{
    CGU_INT bits = 2;
    CGU_INT abits = 3;
    CGU_INT aepbits = 6;

    if (idxMode == 1)
    {
        bits = 3;
        abits = 2;
    }

    CGV_IMAGE src_block[48];
    for (CGU_INT k = 0; k < 16; k++)
    {
        for (CGU_INT p = 0; p < 3; p++)
            src_block[k + p * 16] = EncodeState->image_src[k + p * 16];

        if (rotated_channel < 3)
        {
            // apply channel rotation
            if (EncodeState->channels == 4) src_block[k + rotated_channel * 16] = EncodeState->image_src[k + 3 * 16];
            if (EncodeState->channels == 3) src_block[k + rotated_channel * 16] = 255;
        }
    }

    CGV_IMAGE        block_endpoints[8];
    CGV_INDEXPACKED  color_index[2];
    CGV_INDEX        c_index[MAX_SUBSET_SIZE];
    CGV_EPOCODE      color_qendpoint[8];

    icmp_get_block_endpoints(block_endpoints, src_block, -1, 3);
    icmp_ep_quant_dequant_mode4(color_qendpoint, block_endpoints);
    CGV_ERROR  err = icmp_GetQuantizeIndex(color_index, c_index, src_block, bits, block_endpoints, 0, 3);

    // refine
    CGU_INT refineIterations = EncodeState->refineIterations;
    for (CGU_INT i = 0; i < refineIterations; i++)
    {
        icmp_opt_endpoints(block_endpoints, src_block, bits, color_index, -1, 3);
        icmp_ep_quant_dequant_mode4(color_qendpoint, block_endpoints);
        err = icmp_GetQuantizeIndex(color_index, c_index, src_block, bits, block_endpoints, 0, 3);
    }

    // encoding selected channel 
    CGV_EPOCODE     alpha_qendpoint[2];
    CGV_INDEXPACKED alpha_index[2];
    CGV_INDEX       a_index[MAX_SUBSET_SIZE];
    err += icmp_optQuantizeIndex(EncodeState, alpha_index, a_index, alpha_qendpoint, &EncodeState->image_src[rotated_channel * 16], abits, aepbits);

    if (err < *best_err)
    {
        copy_epocode(best_candidate->color_qendpoint, color_qendpoint, 8);
        copy_epocode(best_candidate->alpha_qendpoint, alpha_qendpoint, 2);
        copy_indexpacked(best_candidate->best_color_index, color_index, 2);
        copy_indexpacked(best_candidate->best_alpha_index, alpha_index, 2);
        best_candidate->rotated_channel = rotated_channel;
        best_candidate->idxMode = idxMode;
        *best_err = err;
    }
}

void icmp_mode5_candidate(
    BC7_EncodeState      EncodeState[],
    cmp_mode_parameters  best_candidate[],
    CGV_ERROR            best_err[],
    CGU_INT              rotated_channel)
{
    CGU_INT bits = 2;
    CGU_INT abits = 2;
    CGU_INT aepbits = 8;

    CGV_IMAGE block[48];
    for (CGU_INT k = 0; k < 16; k++)
    {
        for (CGU_INT p = 0; p < 3; p++)
            block[k + p * 16] = EncodeState->image_src[k + p * 16];

        if (rotated_channel < 3)
        {
            // apply channel rotation
            if (EncodeState->channels == 4) block[k + rotated_channel * 16] = EncodeState->image_src[k + 3 * 16];
            if (EncodeState->channels == 3) block[k + rotated_channel * 16] = 255;
        }
    }

    CGV_IMAGE        block_endpoints[8];
    CGV_EPOCODE      color_qendpoint[8];
    CGV_INDEXPACKED  color_index[2];
    CGV_INDEX        c_index[MAX_SUBSET_SIZE];

    icmp_get_block_endpoints(block_endpoints, block, -1, 3);
    icmp_GetQuantizedEpoCode(color_qendpoint, block_endpoints, 5, 3);
    CGV_ERROR err = icmp_GetQuantizeIndex(color_index, c_index, block, bits, block_endpoints, 0, 3);

    // refine
    CGU_INT refineIterations = EncodeState->refineIterations;
    for (CGU_INT i = 0; i < refineIterations; i++)
    {
        icmp_opt_endpoints(block_endpoints, block, bits, color_index, -1, 3);
        icmp_GetQuantizedEpoCode(color_qendpoint, block_endpoints, 5, 3);
        err = icmp_GetQuantizeIndex(color_index, c_index, block, bits, block_endpoints, 0, 3);
    }

    // encoding selected channel 
    CGV_EPOCODE     alpha_qendpoint[2];
    CGV_INDEXPACKED alpha_index[2];
    CGV_INDEX       a_index[MAX_SUBSET_SIZE];
    err += icmp_optQuantizeIndex(EncodeState, alpha_index, a_index, alpha_qendpoint, &EncodeState->image_src[rotated_channel * 16], abits, aepbits);

    if (err < *best_err)
    {

        icmp_swap_epocode(best_candidate->color_qendpoint, color_qendpoint, 8);
        icmp_swap_indexpacked(best_candidate->best_color_index, color_index, 2);
        icmp_swap_epocode(best_candidate->alpha_qendpoint, alpha_qendpoint, 2);
        icmp_swap_indexpacked(best_candidate->best_alpha_index, alpha_index, 2);
        best_candidate->rotated_channel = rotated_channel;
        *best_err = err;
    }
}


// =============== Mode Compression

CGV_ERROR icmp_enc_mode01237_part_fast(
    CGV_EPOCODE     qep[24],
    CGV_INDEXPACKED color_index[2],
    CGV_INDEX  index[MAX_SUBSET_SIZE],
    CGV_IMAGE  image_src[64],
    CGV_PARTID part_id,
    CGU_INT    blockMode)
{
    CGV_SHIFT32  pattern = icmp_get_pattern2(part_id);
    CGU_INT      bits = 2;  if (blockMode == 0 || blockMode == 1) bits = 3;
    CGU_INT      maxSubSets = 2;  if (blockMode == 0 || blockMode == 2) maxSubSets = 3;
    CGU_CHANNEL  channels = 3;  if (blockMode == 7) channels = 4;

    CGV_IMAGE block_endpoints[24];
    for (CGU_INT subset = 0; subset < maxSubSets; subset++)
    {
        CGV_MASK partition_mask = icmp_get_partition_mask(part_id, subset);
        icmp_get_block_endpoints(&block_endpoints[subset * 8], image_src, partition_mask, channels);
    }

    icmp_GetQuantizedEpoCode(qep, block_endpoints, blockMode, channels);
    CGV_ERROR total_err = icmp_GetQuantizeIndex(color_index, index, image_src, bits, block_endpoints, pattern, channels);

    return total_err;
}

void icmp_enc_mode01237(BC7_EncodeState  EncodeState[], CGU_INT blockMode, CGV_PARTID part_list[], CGU_INT part_count)
{
    if (part_count == 0) return;
    CGU_INT     bits = 2;       if (blockMode == 0 || blockMode == 1) bits = 3;
    CGU_INT     maxSubSets = 2; if (blockMode == 0 || blockMode == 2) maxSubSets = 3;
    CGU_CHANNEL channels = 3;   if (blockMode == 7) channels = 4;

    CGV_EPOCODE       best_qep[24];
    CGV_INDEXPACKED   best_endpoint[2];
    CGV_PARTID        best_part_id = -1;
    CGV_ERROR         best_err = 1e99;

    for (CGU_INT part = 0; part < part_count; part++)
    {
        CGV_PARTID part_id = part_list[part] & 63;
        if (maxSubSets == 3) part_id += 64;

        CGV_EPOCODE      qep[24];
        CGV_INDEXPACKED  color_index[2];
        CGV_INDEX        index[MAX_SUBSET_SIZE];
        CGV_ERROR   err = icmp_enc_mode01237_part_fast(qep, color_index, index, EncodeState->image_src, part_id, blockMode);

        if (err < best_err)
        {
            for (CGU_INT subset = 0; subset < 8 * maxSubSets; subset++) best_qep[subset] = qep[subset];
            for (CGU_INT k = 0; k < 2; k++) best_endpoint[k] = color_index[k];
            best_part_id = part_id;
            best_err = err;
        }
    }

    // refine
    CGU_INT refineIterations = EncodeState->refineIterations;
    for (CGU_INT _i = 0; _i < refineIterations; _i++)
    {
        CGV_IMAGE ep[24];
        for (CGU_INT subset = 0; subset < maxSubSets; subset++)
        {
            CGV_SHIFT32 partition_mask = icmp_get_partition_mask(best_part_id, subset);
            icmp_opt_endpoints(&ep[subset * 8], EncodeState->image_src, bits, best_endpoint, partition_mask, channels);
        }

        CGV_EPOCODE      qep[24];
        CGV_INDEXPACKED  color_index[2];
        CGV_INDEX        index[MAX_SUBSET_SIZE];

        icmp_GetQuantizedEpoCode(qep, ep, blockMode, channels);

        CGV_SHIFT32 pattern = icmp_get_pattern2(best_part_id);
        CGV_ERROR   err = icmp_GetQuantizeIndex(color_index, index, EncodeState->image_src, bits, ep, pattern, channels);

        if (err < best_err)
        {
            for (CGU_INT subset = 0; subset < 8 * maxSubSets; subset++) best_qep[subset] = qep[subset];
            for (CGU_INT k = 0; k < 2; k++) best_endpoint[k] = color_index[k];
            best_err = err;
        }
    }

    if (blockMode != 7) best_err += EncodeState->opaque_err; // take into account alpha channel

    if (best_err < EncodeState->best_err)
    {
        EncodeState->best_err = best_err;
        icmp_encode_mode01237(EncodeState->best_cmp_out, best_qep, best_endpoint, best_part_id, blockMode);
    }
}

void icmp_mode5(BC7_EncodeState  EncodeState[])
{
    cmp_mode_parameters best_candidate;
    CGV_ERROR best_err = EncodeState->best_err;

#ifdef ASPM_GPU
    cmp_memsetBC7((CGV_BYTE *)&best_candidate, 0, sizeof(cmp_mode_parameters));
#else
    memset(&best_candidate, 0, sizeof(cmp_mode_parameters));
#endif

    for (CGU_CHANNEL ch = 0; ch < EncodeState->channels; ch++)
    {
        icmp_mode5_candidate(EncodeState, &best_candidate, &best_err, ch);
    }

    if (best_err < EncodeState->best_err)
    {
        EncodeState->best_err = best_err;
        EncodeState->cmp_isout16Bytes = FALSE;
        icmp_Encode_mode5(EncodeState->best_cmp_out, &best_candidate);
    }
}

void icmp_mode6(BC7_EncodeState  EncodeState[])
{
    CGV_IMAGE block_endpoints[8];
    icmp_get_block_endpoints(block_endpoints, EncodeState->image_src, -1, 4);

    CGV_EPOCODE epo_code[8];
    icmp_GetQuantizedEpoCode(epo_code, block_endpoints, 6, 4);

    CGV_INDEXPACKED color_index[2];
    CGV_INDEX        index[MAX_SUBSET_SIZE];
    CGV_ERROR err = icmp_GetQuantizeIndex(color_index, index, EncodeState->image_src, 4, block_endpoints, 0, 4);

    // refine
    CGU_INT refineIterations = EncodeState->refineIterations;
    for (CGU_INT i = 0; i < refineIterations; i++)
    {
        icmp_opt_endpoints(block_endpoints, EncodeState->image_src, 4, color_index, -1, 4);
        icmp_GetQuantizedEpoCode(epo_code, block_endpoints, 6, EncodeState->channels);
        err = icmp_GetQuantizeIndex(color_index, index, EncodeState->image_src, 4, block_endpoints, 0, 4);
    }

    if (err < EncodeState->best_err)
    {
        EncodeState->best_err = err;
        EncodeState->cmp_isout16Bytes = FALSE;
        icmp_encode_mode6(EncodeState->best_cmp_out, epo_code, color_index);
    }
}

void icmp_mode02(BC7_EncodeState  EncodeState[])
{
    CGV_PARTID part_list[64];
    for (CGU_INT part = 0; part < 64; part++)
        part_list[part] = part;

    if (EncodeState->validModeMask & 0x01)
        icmp_enc_mode01237(EncodeState, 0, part_list, 16);
    if (EncodeState->validModeMask & 0x04)
        icmp_enc_mode01237(EncodeState, 2, part_list, 64); // usually not worth the time
}

void icmp_mode7(BC7_EncodeState  EncodeState[])
{
    CGV_IMAGE full_stats[15];
    icmp_compute_stats_masked(full_stats, EncodeState->image_src, -1, EncodeState->channels);

    CGV_PARTID part_list[64];
    for (CGU_INT part = 0; part < 64; part++)
    {
        CGV_MASK   partition_mask = icmp_get_partition_mask(part + 0, 0);
        CGV_IMAGE  bound12 = icmp_block_pca_bound_split2(EncodeState->image_src, partition_mask, full_stats, EncodeState->channels);
        CGV_PARTID bound = (CGV_TYPEINT)(bound12);
        part_list[part] = part + bound * 64;
    }

    icmp_sort_partlist(part_list, 64, EncodeState->part_count);
    icmp_enc_mode01237(EncodeState, 7, part_list, EncodeState->part_count);
}

void icmp_mode13(BC7_EncodeState  EncodeState[])
{
    CGV_IMAGE full_stats[15];
    icmp_compute_stats_masked(full_stats, EncodeState->image_src, -1, 3);

    CGV_PARTID part_list[64];
    for (CGU_INT part = 0; part < 64; part++)
    {
        CGV_MASK   partition_mask = icmp_get_partition_mask(part + 0, 0);
        CGV_IMAGE  bound12 = icmp_block_pca_bound_split2(EncodeState->image_src, partition_mask, full_stats, 3);
        CGV_PARTID bound = (CGV_TYPEINT)(bound12);
        part_list[part] = part + bound * 64;
    }

    icmp_sort_partlist(part_list, 64, EncodeState->part_count);

    if (EncodeState->validModeMask & 0x02)
        icmp_enc_mode01237(EncodeState, 1, part_list, EncodeState->part_count);
    if (EncodeState->validModeMask & 0x08)
        icmp_enc_mode01237(EncodeState, 3, part_list, EncodeState->part_count);
}

void icmp_mode4(BC7_EncodeState  EncodeState[])
{
    cmp_mode_parameters best_candidate;
    CGV_ERROR best_err = EncodeState->best_err;
#ifdef ASPM_GPU
    cmp_memsetBC7((CGV_BYTE *)&best_candidate, 0, sizeof(cmp_mode_parameters));
#else
    memset(&best_candidate, 0, sizeof(cmp_mode_parameters));
#endif

    for (CGU_CHANNEL rotated_channel = 0; rotated_channel < EncodeState->channels; rotated_channel++)
    {
        icmp_enc_mode4_candidate(EncodeState, &best_candidate, &best_err, rotated_channel, 0);
        icmp_enc_mode4_candidate(EncodeState, &best_candidate, &best_err, rotated_channel, 1);
    }

    // mode 4
    if (best_err < EncodeState->best_err)
    {
        EncodeState->best_err = best_err;
        icmp_encode_mode4(EncodeState->best_cmp_out, &best_candidate);
    }
}

#endif
//===================================== COMPRESS CODE =============================================

bool notValidBlockForMode(
    CGU_UINT32  blockMode,
    CGU_BOOL    blockNeedsAlpha,
    CGU_BOOL    blockAlphaZeroOne,
    uniform CMP_GLOBAL  BC7_Encode  u_BC7Encode[])
{
     // Do we need to skip alpha processing blocks
     if((blockNeedsAlpha == FALSE) && (blockMode > 3))
     {
         return TRUE;
     }

     // Optional restriction for colour-only blocks so that they
     // don't use modes that have combined colour+alpha - this
     // avoids the possibility that the encoder might choose an
     // alpha other than 1.0 (due to parity) and cause something to
     // become accidentally slightly transparent (it's possible that
     // when encoding 3-component texture applications will assume that
     // the 4th component can safely be assumed to be 1.0 all the time)
     if ((blockNeedsAlpha == FALSE) &&
         (u_BC7Encode->colourRestrict == TRUE) && 
         ((blockMode == 6)||(blockMode == 7))) // COMBINED_ALPHA
     {
         return TRUE;
     }
    
     // Optional restriction for blocks with alpha to avoid issues with
     // punch-through or thresholded alpha encoding
     if((blockNeedsAlpha == TRUE) &&
        (u_BC7Encode->alphaRestrict == TRUE) &&
        (blockAlphaZeroOne == TRUE) &&
        ((blockMode == 6)||(blockMode == 7))) // COMBINED_ALPHA
     {
         return TRUE;
     }

     return FALSE;
}

void  BC7_CompressBlock(
                      BC7_EncodeState  EncodeState[],
uniform CMP_GLOBAL    BC7_Encode       u_BC7Encode[])
{
    CGU_BOOL            blockNeedsAlpha        = FALSE;
    CGU_BOOL            blockAlphaZeroOne      = FALSE;

    CGV_ERROR alpha_err = 0.0f;
    CGV_IMAGE alpha_min = 255.0F;

    for (CGU_INT k=0; k<SOURCE_BLOCK_SIZE; k++)
    {
        if ( EncodeState->image_src[k+COMP_ALPHA*SOURCE_BLOCK_SIZE] < alpha_min)
            alpha_min =  EncodeState->image_src[k+COMP_ALPHA*SOURCE_BLOCK_SIZE];

        alpha_err += sq_image( EncodeState->image_src[k+COMP_ALPHA*SOURCE_BLOCK_SIZE]-255.0F);

        if (blockAlphaZeroOne == FALSE)
        {
            if(( EncodeState->image_src[k+COMP_ALPHA*SOURCE_BLOCK_SIZE] == 255.0F) ||
               ( EncodeState->image_src[k+COMP_ALPHA*SOURCE_BLOCK_SIZE] == 0.0F))
            {
                blockAlphaZeroOne = TRUE;
            }
        }
    }

    if (alpha_min != 255.0F)
    {
        blockNeedsAlpha = TRUE;
    }

     EncodeState->best_err    = CMP_FLOAT_MAX;
     EncodeState->opaque_err  = alpha_err;

#ifdef USE_ICMP
     EncodeState->refineIterations = 4;
     EncodeState->fastSkipTreshold = 4;
     EncodeState->channels = 4;
     EncodeState->part_count = 64;
     EncodeState->cmp_isout16Bytes = FALSE;
#else
     EncodeState->cmp_isout16Bytes = TRUE;
#endif

    // We change the order in which we visit the block modes to try to maximize the chance
    // that we manage to early out as quickly as possible.
    // This is a significant performance optimization for the lower quality modes where the
    // exit threshold is higher, and also tends to improve quality (as the generally higher quality
    // modes are now enumerated earlier, so the first encoding that passes the threshold will
    // tend to pass by a greater margin than if we used a dumb ordering, and thus overall error will
    // be improved)
    CGU_INT   blockModeOrder[NUM_BLOCK_TYPES] = {4, 6, 1, 3, 0, 2, 7, 5};

    for (CGU_INT block=0; block < NUM_BLOCK_TYPES; block++)
    {
        CGU_INT blockMode = blockModeOrder[block];

        if (u_BC7Encode->quality < BC7_qFAST_THRESHOLD)
        {
            if ( notValidBlockForMode(blockMode,blockNeedsAlpha,blockAlphaZeroOne,u_BC7Encode) )
                continue;
        }

        CGU_INT      Mode = 0x0001 << blockMode;
        if (!(u_BC7Encode->validModeMask & Mode))
            continue;
        switch (blockMode)
        {
        // image processing with no alpha
        case 0:
                #ifdef USE_ICMP
                    icmp_mode02(EncodeState);
                #else
                    Compress_mode01237(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        case 1:
                #ifdef USE_ICMP
                    icmp_mode13(EncodeState);
                #else
                    Compress_mode01237(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        case 2:
                #ifdef USE_ICMP
                    icmp_mode13(EncodeState);
                #else
                    Compress_mode01237(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        case 3:
                #ifdef USE_ICMP
                    icmp_mode13(EncodeState);
                #else
                    Compress_mode01237(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        // image processing with alpha
        case 4: 
                #ifdef USE_ICMP
                    icmp_mode4(EncodeState);
                #else
                    Compress_mode45(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        case 5:
                #ifdef USE_ICMP
                    icmp_mode5(EncodeState);
                #else
                    Compress_mode45(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        case 6:
                #ifdef USE_ICMP
                    icmp_mode6(EncodeState);
                #else
                    Compress_mode6( EncodeState,  u_BC7Encode);
                #endif
                break;
        case 7:
                #ifdef USE_ICMP
                      icmp_mode7(EncodeState);
                #else
                      Compress_mode01237(blockMode, EncodeState, u_BC7Encode);
                #endif
                break;
        }

        // Early out if we  found we can compress with error below the quality threshold
        if( EncodeState->best_err <= u_BC7Encode->errorThreshold)
        {
            break;
        }
    }

}

//====================================== BC7_ENCODECLASS END =============================================

#ifndef ASPM_GPU

INLINE void load_block_interleaved_rgba2(CGV_IMAGE image_src[64], uniform texture_surface* uniform src, CGUV_BLOCKWIDTH block_xx, CGU_INT block_yy)
{
   for (CGU_INT y=0; y<4; y++)
   for (CGU_INT x=0; x<4; x++)
   {
      CGU_UINT32 * uniform src_ptr = (CGV_SHIFT32*)&src->ptr[(block_yy*4+y)*src->stride];
#ifdef USE_VARYING
      CGV_SHIFT32  rgba = gather_partid(src_ptr, block_xx*4+x);
      image_src[16*0+y*4+x] = (CGV_FLOAT)((rgba>> 0)&255);
      image_src[16*1+y*4+x] = (CGV_FLOAT)((rgba>> 8)&255);
      image_src[16*2+y*4+x] = (CGV_FLOAT)((rgba>>16)&255);
      image_src[16*3+y*4+x] = (CGV_FLOAT)((rgba>>24)&255);
#else
      CGV_SHIFT32  rgba = src_ptr[block_xx*4+x];
      image_src[16*0+y*4+x] = (CGU_FLOAT)((rgba>> 0)&255);
      image_src[16*1+y*4+x] = (CGU_FLOAT)((rgba>> 8)&255);
      image_src[16*2+y*4+x] = (CGU_FLOAT)((rgba>>16)&255);
      image_src[16*3+y*4+x] = (CGU_FLOAT)((rgba>>24)&255);
#endif
   }
}


#if defined(CMP_USE_FOREACH_ASPM) || defined(USE_VARYING)
INLINE void       scatter_uint2(CGU_UINT32 * ptr, CGUV_BLOCKWIDTH idx, CGV_SHIFT32 value)
{
   ptr[idx] = value; // (perf warning expected)
}
#endif

INLINE void store_data_uint32(CGU_UINT8 dst[], CGU_INT width, CGUV_BLOCKWIDTH v_xx, CGU_INT yy, CGV_SHIFT32 data[], CGU_INT data_size)
{
   for (CGU_INT k=0; k<data_size; k++)
   {
      CGU_UINT32 * dst_ptr = (CGV_SHIFT32*)&dst[(yy)*width*data_size];
#ifdef USE_VARYING
      scatter_uint2(dst_ptr, v_xx*data_size+k, data[k]);
#else
      dst_ptr[v_xx*data_size+k] = data[k];
#endif
   }
}

#ifdef USE_VARYING
INLINE void       scatter_uint8(CGU_UINT8* ptr, CGV_SHIFT32 idx, CGV_CMPOUT value)
{
   ptr[idx] = value; // (perf warning expected)
}
#endif

INLINE void store_data_uint8(CGU_UINT8 u_dstptr[], CGU_INT src_width, CGUV_BLOCKWIDTH block_x, CGU_INT block_y, CGUV_CMPOUT data[], CGU_INT data_size)
{
   for (CGU_INT k=0; k<data_size; k++)
   {
#ifdef USE_VARYING
      CGU_UINT8* dst_blockptr = (CGUV_DSTPTR*)&u_dstptr[(block_y*src_width*4)];
      scatter_uint8(dst_blockptr,k+(block_x*data_size),data[k]);
#else
      u_dstptr[(block_y*src_width*4)+k+(block_x*data_size)] = data[k];
#endif
   }
}

INLINE void store_data_uint32(CGU_UINT8 dst[], CGV_SHIFT32 width, CGUV_BLOCKWIDTH v_xx, CGU_INT yy, CGV_SHIFT32 data[], CGU_INT data_size)
{
    for (CGU_INT k = 0; k < data_size; k++)
    {
#if defined(CMP_USE_FOREACH_ASPM) || defined(USE_VARYING)
        CGU_UINT32 * dst_ptr = (CGV_SHIFT32*)&dst[(yy)*width*data_size];
        scatter_uint2(dst_ptr, v_xx*data_size + k, data[k]);
#else
        dst[((yy)*width*data_size) + v_xx * data_size + k] = data[k];
#endif
    }
}

void CompressBlockBC7_XY(uniform texture_surface u_srcptr[], CGUV_BLOCKWIDTH block_x, CGU_INT block_y, CGU_UINT8 u_dst[], uniform BC7_Encode u_settings[])
{
   BC7_EncodeState _state;
   varying BC7_EncodeState* uniform state = &_state;

   copy_BC7_Encode_settings(state, u_settings);

   load_block_interleaved_rgba2(state->image_src,u_srcptr, block_x, block_y);

   BC7_CompressBlock(state, u_settings);

   if (state->cmp_isout16Bytes)
       store_data_uint8(u_dst, u_srcptr->width, block_x, block_y, state->cmp_out, 16);
   else
       store_data_uint32(u_dst, u_srcptr->width, block_x, block_y, state->best_cmp_out, 4);

}

 CMP_EXPORT void CompressBlockBC7_encode( uniform texture_surface src[], CGU_UINT8 dst[], uniform BC7_Encode settings[])
{
  // bc7_isa(); ASPM_PRINT(("ASPM encode [%d,%d]\n",bc7_isa(),src->width,src->height));

  for (CGU_INT u_yy = 0; u_yy<src->height/4; u_yy++)
 #ifdef CMP_USE_FOREACH_ASPM
    foreach (v_xx = 0 ... src->width/4)
    {
 #else
    for (CGUV_BLOCKWIDTH v_xx = 0; v_xx<src->width/4; v_xx++)
    {
 #endif
        CompressBlockBC7_XY(src, v_xx, u_yy, dst, settings);
    }
}

#endif

#ifndef ASPM_GPU
#ifndef ASPM
//======================= DECOMPRESS =========================================
#ifndef USE_HIGH_PRECISION_INTERPOLATION_BC7
CGU_UINT16 aWeight2[] = { 0, 21, 43, 64 };
CGU_UINT16 aWeight3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
CGU_UINT16 aWeight4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

CGU_UINT8 interpolate(CGU_UINT8 e0, CGU_UINT8 e1, CGU_UINT8 index, CGU_UINT8 indexprecision)
{
    if (indexprecision == 2)
        return (CGU_UINT8)(((64 - aWeight2[index])*CGU_UINT16(e0) + aWeight2[index] * CGU_UINT16(e1) + 32) >> 6);
    else if (indexprecision == 3)
        return (CGU_UINT8)(((64 - aWeight3[index])*CGU_UINT16(e0) + aWeight3[index] * CGU_UINT16(e1) + 32) >> 6);
    else // indexprecision == 4
        return (CGU_UINT8)(((64 - aWeight4[index])*CGU_UINT16(e0) + aWeight4[index] * CGU_UINT16(e1) + 32) >> 6);
}
#endif

void GetBC7Ramp(CGU_UINT32 endpoint[][MAX_DIMENSION_BIG],
                CGU_FLOAT  ramp[MAX_DIMENSION_BIG][(1<<MAX_INDEX_BITS)],
                CGU_UINT32 clusters[2],
                CGU_UINT32 componentBits[MAX_DIMENSION_BIG])
{
    CGU_UINT32 ep[2][MAX_DIMENSION_BIG];
    CGU_UINT32 i;

    // Expand each endpoint component to 8 bits by shifting the MSB to bit 7
    // and then replicating the high bits to the low bits revealed by
    // the shift
    for(i=0; i<MAX_DIMENSION_BIG; i++)
    {
        ep[0][i] = 0;
        ep[1][i] = 0;
        if(componentBits[i])
        {
            ep[0][i]  = (CGU_UINT32)(endpoint[0][i] << (8 - componentBits[i]));
            ep[1][i]  = (CGU_UINT32)(endpoint[1][i] << (8 - componentBits[i]));
            ep[0][i] += (CGU_UINT32)(ep[0][i] >> componentBits[i]);
            ep[1][i] += (CGU_UINT32)(ep[1][i] >> componentBits[i]);

            ep[0][i] = min8(255, max8(0, static_cast<CGU_UINT8>(ep[0][i])));
            ep[1][i] = min8(255, max8(0, static_cast<CGU_UINT8>(ep[1][i])));
        }
    }

    // If this block type has no explicit alpha channel
    // then make sure alpha is 1.0 for all points on the ramp
    if(!componentBits[COMP_ALPHA])
    {
        ep[0][COMP_ALPHA] = ep[1][COMP_ALPHA] = 255;
    }

    CGU_UINT32   rampIndex = clusters[0];

    rampIndex = (CGU_UINT32)(log((double)rampIndex) / log(2.0));

    // Generate colours for the RGB ramp
    for(i=0; i < clusters[0]; i++)
    {
#ifdef USE_HIGH_PRECISION_INTERPOLATION_BC7
        ramp[COMP_RED][i] = (CGU_FLOAT)floor((ep[0][COMP_RED] * (1.0-rampLerpWeightsBC7[rampIndex][i])) +
                                  (ep[1][COMP_RED] * rampLerpWeightsBC7[rampIndex][i]) + 0.5);
        ramp[COMP_RED][i] = bc7_minf(255.0, bc7_maxf(0., ramp[COMP_RED][i]));
        ramp[COMP_GREEN][i] = (CGU_FLOAT)floor((ep[0][COMP_GREEN] * (1.0-rampLerpWeightsBC7[rampIndex][i])) +
                                  (ep[1][COMP_GREEN] * rampLerpWeightsBC7[rampIndex][i]) + 0.5);
        ramp[COMP_GREEN][i] = bc7_minf(255.0, bc7_maxf(0., ramp[COMP_GREEN][i]));
        ramp[COMP_BLUE][i] = (CGU_FLOAT)floor((ep[0][COMP_BLUE] * (1.0-rampLerpWeightsBC7[rampIndex][i])) +
                                  (ep[1][COMP_BLUE] * rampLerpWeightsBC7[rampIndex][i]) + 0.5);
        ramp[COMP_BLUE][i] = bc7_minf(255.0, bc7_maxf(0., ramp[COMP_BLUE][i]));
#else
        ramp[COMP_RED][i]   = interpolate(ep[0][COMP_RED], ep[1][COMP_RED], i, rampIndex);
        ramp[COMP_GREEN][i] = interpolate(ep[0][COMP_GREEN], ep[1][COMP_GREEN], i, rampIndex);
        ramp[COMP_BLUE][i]  = interpolate(ep[0][COMP_BLUE], ep[1][COMP_BLUE], i, rampIndex);
#endif
    }


    rampIndex = clusters[1];
    rampIndex = (CGU_UINT32)(log((CGU_FLOAT)rampIndex) / log(2.0));

    if(!componentBits[COMP_ALPHA])
    {
        for(i=0; i < clusters[1]; i++)
        {
            ramp[COMP_ALPHA][i] = 255.;
        }
    }
    else
    {

        // Generate alphas
        for(i=0; i < clusters[1]; i++)
        {
#ifdef USE_HIGH_PRECISION_INTERPOLATION_BC7
            ramp[COMP_ALPHA][i] = (CGU_FLOAT)floor((ep[0][COMP_ALPHA] * (1.0-rampLerpWeightsBC7[rampIndex][i])) +
                                        (ep[1][COMP_ALPHA] * rampLerpWeightsBC7[rampIndex][i]) + 0.5);
            ramp[COMP_ALPHA][i] = bc7_minf(255.0, bc7_maxf(0., ramp[COMP_ALPHA][i]));
#else
            ramp[COMP_ALPHA][i] = interpolate(ep[0][COMP_ALPHA], ep[1][COMP_ALPHA], i, rampIndex);
#endif
        }

    }
}

//
// Bit reader - reads one bit from a buffer at the current bit offset
//              and increments the offset
//

CGU_UINT32 ReadBit(const CGU_UINT8 base[],CGU_UINT32 &m_bitPosition)
{
    int             byteLocation;
    int             remainder;
    CGU_UINT32 bit = 0;
    byteLocation   = m_bitPosition/8;
    remainder      = m_bitPosition % 8;

    bit = base[byteLocation];
    bit >>= remainder;
    bit &= 0x1;
    // Increment bit position
    m_bitPosition++;
    return (bit);
}

void DecompressDualIndexBlock(
    CGU_UINT8   out[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG],
    const CGU_UINT8   in[COMPRESSED_BLOCK_SIZE],
    CGU_UINT32  endpoint[2][MAX_DIMENSION_BIG],
    CGU_UINT32  &m_bitPosition,
    CGU_UINT32  m_rotation,
    CGU_UINT32  m_blockMode,
    CGU_UINT32  m_indexSwap,
    CGU_UINT32  m_componentBits[MAX_DIMENSION_BIG])
{
    CGU_UINT32   i, j, k;
    CGU_FLOAT    ramp[MAX_DIMENSION_BIG][1<<MAX_INDEX_BITS];
    CGU_UINT32   blockIndices[2][MAX_SUBSET_SIZE];

    CGU_UINT32   clusters[2];
    clusters[0] = 1 << bti[m_blockMode].indexBits[0];
    clusters[1] = 1 << bti[m_blockMode].indexBits[1];
    if(m_indexSwap)
    {
        CGU_UINT32   temp = clusters[0];
        clusters[0] = clusters[1];
        clusters[1] = temp;
    }

    GetBC7Ramp(endpoint,
               ramp,
               clusters,
               m_componentBits);

    // Extract the indices
    for(i=0;i<2;i++)
    {
        for(j=0;j<MAX_SUBSET_SIZE;j++)
        {
            blockIndices[i][j] = 0;
            // If this is a fixup index then clear the implicit bit
            if(j==0)
            {
                blockIndices[i][j] &= ~(1 << (bti[m_blockMode].indexBits[i]-1));
                for(k=0;k<static_cast <CGU_UINT32>(bti[m_blockMode].indexBits[i] - 1); k++)
                {
                    blockIndices[i][j] |= (CGU_UINT32)ReadBit(in,m_bitPosition) << k;
                }
            }
            else
            {
               for(k=0;k<bti[m_blockMode].indexBits[i]; k++)
               {
                   blockIndices[i][j] |= (CGU_UINT32)ReadBit(in,m_bitPosition) << k;
               }
            }
        }
    }

    // Generate block colours
    for(i=0;i<MAX_SUBSET_SIZE;i++)
    {
        out[i][COMP_ALPHA] = (CGU_UINT8)ramp[COMP_ALPHA][blockIndices[m_indexSwap^1][i]];
        out[i][COMP_RED]   = (CGU_UINT8)ramp[COMP_RED][blockIndices[m_indexSwap][i]];
        out[i][COMP_GREEN] = (CGU_UINT8)ramp[COMP_GREEN][blockIndices[m_indexSwap][i]];
        out[i][COMP_BLUE]  = (CGU_UINT8)ramp[COMP_BLUE][blockIndices[m_indexSwap][i]];
    }

    // Resolve the component rotation
    CGU_INT8 swap;
    for(i=0; i<MAX_SUBSET_SIZE; i++)
    {
        switch(m_rotation)
        {
            case    0:
                // Do nothing
                break;
            case    1:
                // Swap A and R
                swap = out[i][COMP_ALPHA];
                out[i][COMP_ALPHA] = out[i][COMP_RED];
                out[i][COMP_RED] = swap;
                break;
            case    2:
                // Swap A and G
                swap = out[i][COMP_ALPHA];
                out[i][COMP_ALPHA] = out[i][COMP_GREEN];
                out[i][COMP_GREEN] = swap;
                break;
            case    3:
                // Swap A and B
                swap = out[i][COMP_ALPHA];
                out[i][COMP_ALPHA] = out[i][COMP_BLUE];
                out[i][COMP_BLUE] = swap;
                break;
        }
    }
}


void DecompressBC7_internal(CGU_UINT8  out[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG], const CGU_UINT8 in[COMPRESSED_BLOCK_SIZE], const BC7_Encode *u_BC7Encode)
{
    if (u_BC7Encode) {}
    CGU_UINT32           i, j;
    CGU_UINT32           blockIndices[MAX_SUBSET_SIZE];
    CGU_UINT32           endpoint[MAX_SUBSETS][2][MAX_DIMENSION_BIG];

    CGU_UINT32 m_blockMode;
    CGU_UINT32 m_partition;
    CGU_UINT32 m_rotation;
    CGU_UINT32 m_indexSwap;

    CGU_UINT32 m_bitPosition;
    CGU_UINT32 m_componentBits[MAX_DIMENSION_BIG];

    m_blockMode = 0;
    m_partition = 0;
    m_rotation = 0;
    m_indexSwap = 0;

    // Position the read pointer at the LSB of the block
    m_bitPosition = 0;

    while (!ReadBit(in, m_bitPosition) && (m_blockMode < 8))
    {
        m_blockMode++;
    }

    if (m_blockMode > 7)
    {
        // Something really bad happened...
        return;
    }

    for (i = 0; i < bti[m_blockMode].rotationBits; i++)
    {
        m_rotation |= ReadBit(in, m_bitPosition) << i;
    }
    for (i = 0; i < bti[m_blockMode].indexModeBits; i++)
    {
        m_indexSwap |= ReadBit(in, m_bitPosition) << i;
    }

    for (i = 0; i < bti[m_blockMode].partitionBits; i++)
    {
        m_partition |= ReadBit(in, m_bitPosition) << i;
    }



    if (bti[m_blockMode].encodingType == NO_ALPHA)
    {
        m_componentBits[COMP_ALPHA] = 0;
        m_componentBits[COMP_RED] =
            m_componentBits[COMP_GREEN] =
            m_componentBits[COMP_BLUE] = bti[m_blockMode].vectorBits / 3;
    }
    else if (bti[m_blockMode].encodingType == COMBINED_ALPHA)
    {
        m_componentBits[COMP_ALPHA] =
            m_componentBits[COMP_RED] =
            m_componentBits[COMP_GREEN] =
            m_componentBits[COMP_BLUE] = bti[m_blockMode].vectorBits / 4;
    }
    else if (bti[m_blockMode].encodingType == SEPARATE_ALPHA)
    {
        m_componentBits[COMP_ALPHA] = bti[m_blockMode].scalarBits;
        m_componentBits[COMP_RED] =
            m_componentBits[COMP_GREEN] =
            m_componentBits[COMP_BLUE] = bti[m_blockMode].vectorBits / 3;
    }

    CGU_UINT32   subset, ep, component;
    // Endpoints are stored in the following order RRRR GGGG BBBB (AAAA) (PPPP)
    // i.e. components are packed together
    // Loop over components
    for (component = 0; component < MAX_DIMENSION_BIG; component++)
    {
        // loop over subsets
        for (subset = 0; subset < (int)bti[m_blockMode].subsetCount; subset++)
        {
            // Loop over endpoints
            for (ep = 0; ep < 2; ep++)
            {
                endpoint[subset][ep][component] = 0;
                for (j = 0; j < m_componentBits[component]; j++)
                {
                    endpoint[subset][ep][component] |= ReadBit(in, m_bitPosition) << j;
                }
            }
        }
    }


    // Now get any parity bits
    if (bti[m_blockMode].pBitType != NO_PBIT)
    {
        for (subset = 0; subset < (int)bti[m_blockMode].subsetCount; subset++)
        {
            CGU_UINT32   pBit[2];
            if (bti[m_blockMode].pBitType == ONE_PBIT)
            {
                pBit[0] = ReadBit(in, m_bitPosition);
                pBit[1] = pBit[0];
            }
            else if (bti[m_blockMode].pBitType == TWO_PBIT)
            {
                pBit[0] = ReadBit(in, m_bitPosition);
                pBit[1] = ReadBit(in, m_bitPosition);
            }

            for (component = 0; component < MAX_DIMENSION_BIG; component++)
            {
                if (m_componentBits[component])
                {
                    endpoint[subset][0][component] <<= 1;
                    endpoint[subset][1][component] <<= 1;
                    endpoint[subset][0][component] |= pBit[0];
                    endpoint[subset][1][component] |= pBit[1];
                }
            }
        }
    }

    if (bti[m_blockMode].pBitType != NO_PBIT)
    {
        // Now that we've unpacked the parity bits, update the component size information
        // for the ramp generator
        for (j = 0; j < MAX_DIMENSION_BIG; j++)
        {
            if (m_componentBits[j])
            {
                m_componentBits[j] += 1;
            }
        }
    }

    // If this block has two independent sets of indices then put it to that decoder
    if (bti[m_blockMode].encodingType == SEPARATE_ALPHA)
    {
        DecompressDualIndexBlock(out, in, endpoint[0], m_bitPosition, m_rotation, m_blockMode, m_indexSwap, m_componentBits);
        return;
    }

    CGU_UINT32   fixup[MAX_SUBSETS] = { 0, 0, 0 };
    switch (bti[m_blockMode].subsetCount)
    {
    case    3:
        fixup[1] = BC7_FIXUPINDICES_LOCAL[2][m_partition][1];
        fixup[2] = BC7_FIXUPINDICES_LOCAL[2][m_partition][2];
        break;
    case    2:
        fixup[1] = BC7_FIXUPINDICES_LOCAL[1][m_partition][1];
        break;
    default:
        break;
    }

    //--------------------------------------------------------------------
    // New Code : Possible replacement for BC7_PARTITIONS for CPU code
    //--------------------------------------------------------------------
    // Extract index bits
    // for (i = 0; i < MAX_SUBSET_SIZE; i++)
    // {
    //     CGV_UINT8   p = get_partition_subset(m_partition, bti[m_blockMode].subsetCount - 1, i);
    //     //CGU_UINT32   p = partitionTable[i];
    //     blockIndices[i] = 0;
    //     CGU_UINT32   bitsToRead = bti[m_blockMode].indexBits[0];
    // 
    //     // If this is a fixup index then set the implicit bit
    //     if (i == fixup[p])
    //     {
    //         blockIndices[i] &= ~(1 << (bitsToRead - 1));
    //         bitsToRead--;
    //     }
    // 
    //     for (j = 0; j < bitsToRead; j++)
    //     {
    //         blockIndices[i] |= ReadBit(in, m_bitPosition) << j;
    //     }
    // }
    CGU_BYTE *partitionTable = (CGU_BYTE*)BC7_PARTITIONS[bti[m_blockMode].subsetCount-1][m_partition];

    // Extract index bits
    for(i=0; i < MAX_SUBSET_SIZE; i++)
    {
        CGU_BYTE   p = partitionTable[i];
        blockIndices[i] = 0;
        CGU_BYTE   bitsToRead = bti[m_blockMode].indexBits[0];

        // If this is a fixup index then set the implicit bit
        if(i==fixup[p])
        {
            blockIndices[i] &= ~(1 << (bitsToRead-1));
            bitsToRead--;
        }

        for(j=0;j<bitsToRead; j++)
        {
            blockIndices[i] |= ReadBit(in,m_bitPosition) << j;
        }
    }

    // Get the ramps
    CGU_UINT32   clusters[2];
    clusters[0] = clusters[1] = 1 << bti[m_blockMode].indexBits[0];

    // Colour Ramps
    CGU_FLOAT          c[MAX_SUBSETS][MAX_DIMENSION_BIG][1 << MAX_INDEX_BITS];

    for (i = 0; i < (int)bti[m_blockMode].subsetCount; i++)
    {
        // Unpack the colours
        GetBC7Ramp(endpoint[i],
            c[i],
            clusters,
            m_componentBits);
    }

    //--------------------------------------------------------------------
    // New Code : Possible replacement for BC7_PARTITIONS for CPU code
    //--------------------------------------------------------------------
    // Generate the block colours.
    // for (i = 0; i < MAX_SUBSET_SIZE; i++)
    // {
    //     CGV_UINT8   p = get_partition_subset(m_partition, bti[m_blockMode].subsetCount - 1, i);
    //     out[i][0] = c[p][0][blockIndices[i]];
    //     out[i][1] = c[p][1][blockIndices[i]];
    //     out[i][2] = c[p][2][blockIndices[i]];
    //     out[i][3] = c[p][3][blockIndices[i]];
    // }

    // Generate the block colours.
    for(i=0; i<MAX_SUBSET_SIZE; i++)
    {
        for(j=0; j < MAX_DIMENSION_BIG; j++)
        {
            out[i][j] = (CGU_UINT8)c[partitionTable[i]][j][blockIndices[i]];
        }
    }
}

void  CompressBlockBC7_Internal(
    CGU_UINT8                      image_src[SOURCE_BLOCK_SIZE][4],
    CMP_GLOBAL    CGV_CMPOUT       cmp_out[COMPRESSED_BLOCK_SIZE],
    uniform CMP_GLOBAL BC7_Encode  u_BC7Encode[])
{
    BC7_EncodeState _state = { 0 };
    varying BC7_EncodeState* uniform state = &_state;

    copy_BC7_Encode_settings(state, u_BC7Encode);

    CGU_UINT8 offsetR = 0;
    CGU_UINT8 offsetG = 16;
    CGU_UINT8 offsetB = 32;
    CGU_UINT8 offsetA = 48;
    for (CGU_UINT8 i = 0; i < SOURCE_BLOCK_SIZE; i++)
    {
        state->image_src[offsetR++] = (CGV_IMAGE)image_src[i][0];
        state->image_src[offsetG++] = (CGV_IMAGE)image_src[i][1];
        state->image_src[offsetB++] = (CGV_IMAGE)image_src[i][2];
        state->image_src[offsetA++] = (CGV_IMAGE)image_src[i][3];
    }

    BC7_CompressBlock(state, u_BC7Encode);

    if (state->cmp_isout16Bytes)
    {
        for (CGU_UINT8 i = 0; i < COMPRESSED_BLOCK_SIZE; i++)
        {
            cmp_out[i] = state->cmp_out[i];
        }
    }
    else
    {
#ifdef ASPM_GPU
        cmp_memcpy(cmp_out, (CGU_UINT8 *)state->best_cmp_out, 16);
#else
        memcpy(cmp_out, state->best_cmp_out, 16);
#endif
    }
}

//======================= CPU USER INTERFACES ====================================

int CMP_CDECL CreateOptionsBC7(void **options)
{
    (*options) = new BC7_Encode;
    if (!options) return CGU_CORE_ERR_NEWMEM;
    init_BC7ramps();
    SetDefaultBC7Options((BC7_Encode *)(*options));
    return CGU_CORE_OK;
}

int CMP_CDECL DestroyOptionsBC7(void *options)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC7_Encode *BCOptions = reinterpret_cast <BC7_Encode *>(options);
    delete BCOptions;
    return CGU_CORE_OK;
}

int CMP_CDECL SetErrorThresholdBC7(void *options, CGU_FLOAT minThreshold, CGU_FLOAT maxThreshold)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC7_Encode *BC7optionsDefault = (BC7_Encode *)options;

    if (minThreshold < 0.0f) minThreshold = 0.0f;
    if (maxThreshold < 0.0f) maxThreshold = 0.0f;

    BC7optionsDefault->minThreshold = minThreshold;
    BC7optionsDefault->maxThreshold = maxThreshold;
    return CGU_CORE_OK;
}

int CMP_CDECL SetQualityBC7(void *options, CGU_FLOAT fquality)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;

    BC7_Encode *BC7optionsDefault = (BC7_Encode *)options;
    if (fquality < 0.0f) fquality = 0.0f;
    else
        if (fquality > 1.0f) fquality = 1.0f;
    BC7optionsDefault->quality = fquality;

    // Set Error Thresholds
    BC7optionsDefault->errorThreshold           = BC7optionsDefault->maxThreshold * (1.0f - fquality);
    if(fquality > BC7_qFAST_THRESHOLD)
        BC7optionsDefault->errorThreshold      += BC7optionsDefault->minThreshold;

    return CGU_CORE_OK;
}

int CMP_CDECL SetMaskBC7(void *options, CGU_UINT8 mask)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC7_Encode *BC7options = (BC7_Encode *)options;
    BC7options->validModeMask = mask;
    return CGU_CORE_OK;
}

int CMP_CDECL SetAlphaOptionsBC7(void *options, CGU_BOOL imageNeedsAlpha, CGU_BOOL colourRestrict, CGU_BOOL alphaRestrict)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    BC7_Encode *u_BC7Encode = (BC7_Encode *)options;
    u_BC7Encode->imageNeedsAlpha = imageNeedsAlpha;
    u_BC7Encode->colourRestrict  = colourRestrict;
    u_BC7Encode->alphaRestrict   = alphaRestrict;
    return CGU_CORE_OK;
}

int CMP_CDECL CompressBlockBC7( const unsigned char *srcBlock,
                                unsigned int srcStrideInBytes,
                                CMP_GLOBAL unsigned char cmpBlock[16],
                                const void* options = NULL) 
{
    CMP_Vec4uc inBlock[SOURCE_BLOCK_SIZE];

    //----------------------------------
    // Fill the inBlock with source data
    //----------------------------------
    CGU_INT srcpos = 0;
    CGU_INT dstptr = 0;
    for (CGU_UINT8 row = 0; row < 4; row++)
    {
        srcpos = row * srcStrideInBytes;
        for (CGU_UINT8 col = 0; col < 4; col++)
        {
            inBlock[dstptr].x = CGU_UINT8(srcBlock[srcpos++]);
            inBlock[dstptr].y = CGU_UINT8(srcBlock[srcpos++]);
            inBlock[dstptr].z = CGU_UINT8(srcBlock[srcpos++]);
            inBlock[dstptr].w = CGU_UINT8(srcBlock[srcpos++]);
            dstptr++;
        }
    }


    BC7_Encode *u_BC7Encode = (BC7_Encode *)options;
    BC7_Encode       BC7EncodeDefault = { 0 };
    if (u_BC7Encode == NULL)
    {
        u_BC7Encode = &BC7EncodeDefault;
        SetDefaultBC7Options(u_BC7Encode);
        init_BC7ramps();
    }

    BC7_EncodeState EncodeState 
#ifndef ASPM
        = { 0 }
#endif
    ;
    EncodeState.best_err        = CMP_FLOAT_MAX;
    EncodeState.validModeMask   = u_BC7Encode->validModeMask;
    EncodeState.part_count      = u_BC7Encode->part_count;
    EncodeState.channels        = static_cast<CGU_CHANNEL>(u_BC7Encode->channels);

    CGU_UINT8 offsetR = 0;
    CGU_UINT8 offsetG = 16;
    CGU_UINT8 offsetB = 32;
    CGU_UINT8 offsetA = 48;
    CGU_UINT32 offsetSRC = 0;
    for (CGU_UINT8 i = 0; i < SOURCE_BLOCK_SIZE; i++)
    {
        EncodeState.image_src[offsetR++] = (CGV_IMAGE)inBlock[offsetSRC].x;
        EncodeState.image_src[offsetG++] = (CGV_IMAGE)inBlock[offsetSRC].y;
        EncodeState.image_src[offsetB++] = (CGV_IMAGE)inBlock[offsetSRC].z;
        EncodeState.image_src[offsetA++] = (CGV_IMAGE)inBlock[offsetSRC].w;
        offsetSRC++;
    }

    BC7_CompressBlock(&EncodeState, u_BC7Encode);

    if (EncodeState.cmp_isout16Bytes)
    {
        for (CGU_UINT8 i = 0; i < COMPRESSED_BLOCK_SIZE; i++)
        {
            cmpBlock[i] = EncodeState.cmp_out[i];
        }
    }
    else
    {
        memcpy(cmpBlock, EncodeState.best_cmp_out, 16);
    }

    return CGU_CORE_OK;
}

int  CMP_CDECL DecompressBlockBC7(const unsigned char cmpBlock[16],
                                  unsigned char srcBlock[64],
                                  const void *options = NULL) {
    BC7_Encode *u_BC7Encode = (BC7_Encode *)options;
    BC7_Encode       BC7EncodeDefault = { 0 }; // for q = 0.05
    if (u_BC7Encode == NULL)
    {
        // set for q = 1.0
        u_BC7Encode = &BC7EncodeDefault;
        SetDefaultBC7Options(u_BC7Encode);
        init_BC7ramps();
    }
    DecompressBC7_internal((CGU_UINT8(*)[4])srcBlock, (CGU_UINT8 *)cmpBlock,u_BC7Encode);
    return CGU_CORE_OK;
}
#endif
#endif

//============================================== OpenCL USER INTERFACE ====================================================
#ifdef ASPM_GPU
CMP_STATIC CMP_KERNEL void CMP_GPUEncoder(uniform CMP_GLOBAL const  CGU_Vec4uc      ImageSource[],
                                                  CMP_GLOBAL        CGV_CMPOUT      ImageDestination[],
                                          uniform CMP_GLOBAL        Source_Info     SourceInfo[],
                                          uniform CMP_GLOBAL        BC7_Encode      BC7Encode[] )
{
    CGU_INT xID=0;
    CGU_INT yID=0;

    xID = get_global_id(0);         // ToDo: Define a size_t 32 bit and 64 bit basd on clGetDeviceInfo
    yID = get_global_id(1);

    CGU_INT  srcWidth  = SourceInfo->m_src_width;
    CGU_INT  srcHeight = SourceInfo->m_src_height;
    if (xID >= (srcWidth  / BlockX)) return;
    if (yID >= (srcHeight / BlockY)) return;

    CGU_INT     destI = (xID*COMPRESSED_BLOCK_SIZE) + (yID*(srcWidth / BlockX)*COMPRESSED_BLOCK_SIZE);
    CGU_INT     srcindex = 4 * (yID * srcWidth + xID);
    CGU_INT     blkindex = 0;
    BC7_EncodeState EncodeState;
    varying BC7_EncodeState* uniform state = &EncodeState;

   copy_BC7_Encode_settings(state, BC7Encode);

    //Check if it is a complete 4X4 block
    if (((xID + 1)*BlockX <= srcWidth) && ((yID + 1)*BlockY <= srcHeight))
    {
        srcWidth = srcWidth - 4;
        for (CGU_INT j = 0; j < 4; j++) {
            for (CGU_INT i = 0; i < 4; i++) {
                state->image_src[blkindex+0*SOURCE_BLOCK_SIZE] = ImageSource[srcindex].x;
                state->image_src[blkindex+1*SOURCE_BLOCK_SIZE] = ImageSource[srcindex].y;
                state->image_src[blkindex+2*SOURCE_BLOCK_SIZE] = ImageSource[srcindex].z;
                state->image_src[blkindex+3*SOURCE_BLOCK_SIZE] = ImageSource[srcindex].w;
                blkindex++;
                srcindex++;
            }

            srcindex += srcWidth;
        }

   copy_BC7_Encode_settings(state, BC7Encode);

    BC7_CompressBlock(&EncodeState, BC7Encode);

    for (CGU_INT i=0; i<COMPRESSED_BLOCK_SIZE; i++)
    {
        ImageDestination[destI+i] = state->cmp_out[i];
    }

    }
    else
    {
        ASPM_PRINT(("[ASPM_GPU] Unable to process, make sure image size is divisible by 4"));
    }
}
#endif
