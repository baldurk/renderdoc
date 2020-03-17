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
#include "BC1_Encode_kernel.h"

//============================================== BC1 INTERFACES  =======================================================
void CompressBlockBC1_Fast(
    CMP_Vec4uc  srcBlockTemp[16],
    CMP_GLOBAL CGU_UINT32 compressedBlock[2])
{
    int i, k;

    CMP_Vec3f rgb;
    CMP_Vec3f average_rgb;                  // The centrepoint of the axis
    CMP_Vec3f v_rgb;                        // The axis
    CMP_Vec3f uniques[16];                  // The list of unique colours
    int unique_pixels;                     // The number of unique pixels
    CGU_FLOAT unique_recip;                    // Reciprocal of the above for fast multiplication
    int index_map[16];                     // The map of source pixels to unique indices
                                    
    CGU_FLOAT pos_on_axis[16];                 // The distance each unique falls along the compression axis
    CGU_FLOAT dist_from_axis[16];              // The distance each unique falls from the compression axis
    CGU_FLOAT left = 0, right = 0, centre = 0; // The extremities and centre (average of left/right) of uniques along the compression axis
    CGU_FLOAT axis_mapping_error = 0;          // The total computed error in mapping pixels to the axis

    int swap;                              // Indicator if the RGB values need swapping to generate an opaque result

    // -------------------------------------------------------------------------------------
    // (3) Find the array of unique pixel values and sum them to find their average position
    // -------------------------------------------------------------------------------------
    {
        // Find the array of unique pixel values and sum them to find their average position      
        int current_pixel, firstdiff;
        current_pixel = unique_pixels = 0;
        average_rgb = 0.0f;
        firstdiff = -1;
        for (i = 0; i<16; i++)
        {
                for (k = 0; k<i; k++)
                    if ((((srcBlockTemp[k].x ^ srcBlockTemp[i].x) & 0xf8) == 0) && (((srcBlockTemp[k].y ^ srcBlockTemp[i].y) & 0xfc) == 0) && (((srcBlockTemp[k].z ^ srcBlockTemp[i].z) & 0xf8) == 0))
                        break;
                index_map[i] = current_pixel++;
                //pixel_count[i] = 1;
                CMP_Vec3f trgb;
                rgb.x = (CGU_FLOAT)((srcBlockTemp[i].x) & 0xff);
                rgb.y = (CGU_FLOAT)((srcBlockTemp[i].y) & 0xff);
                rgb.z = (CGU_FLOAT)((srcBlockTemp[i].z) & 0xff);

                trgb.x = CS_RED(rgb.x, rgb.y, rgb.z);
                trgb.y = CS_GREEN(rgb.x, rgb.y, rgb.z);
                trgb.z = CS_BLUE(rgb.x, rgb.y, rgb.z);
                uniques[i] = trgb;

                if (k == i)
                {
                    unique_pixels++;
                    if ((i != 0) && (firstdiff < 0)) firstdiff = i;
                }
                average_rgb = average_rgb + trgb;
        }

        unique_pixels = 16;
        // Compute average of the uniques
        unique_recip = 1.0f / (CGU_FLOAT)unique_pixels;
        average_rgb = average_rgb * unique_recip;
    }

    // -------------------------------------------------------------------------------------
    // (4) For each component, reflect points about the average so all lie on the same side
    // of the average, and compute the new average - this gives a second point that defines the axis
    // To compute the sign of the axis sum the positive differences of G for each of R and B (the
    // G axis is always positive in this implementation
    // -------------------------------------------------------------------------------------
    // An interesting situation occurs if the G axis contains no information, in which case the RB
    // axis is also compared. I am not entirely sure if this is the correct implementation - should
    // the priority axis be determined by magnitude?
    {

        CGU_FLOAT rg_pos, bg_pos, rb_pos;
        v_rgb = 0.0f;
        rg_pos = bg_pos = rb_pos = 0;

        for (i = 0; i < unique_pixels; i++)
        {
            rgb = uniques[i] - average_rgb;

#ifndef ASPM_GPU
            v_rgb.x += (CGU_FLOAT)fabs(rgb.x);
            v_rgb.y += (CGU_FLOAT)fabs(rgb.y);
            v_rgb.z += (CGU_FLOAT)fabs(rgb.z);
#else
            v_rgb = v_rgb + fabs(rgb);
#endif

            if (rgb.x > 0) { rg_pos += rgb.y; rb_pos += rgb.z; }
            if (rgb.z > 0) bg_pos += rgb.y;
        }
        v_rgb = v_rgb*unique_recip;
        if (rg_pos < 0) v_rgb.x = -v_rgb.x;
        if (bg_pos < 0) v_rgb.z = -v_rgb.z;
        if ((rg_pos == bg_pos) && (rg_pos == 0))
            if (rb_pos < 0) v_rgb.z = -v_rgb.z;
    }

    // -------------------------------------------------------------------------------------
    // (5) Axis projection and remapping
    // -------------------------------------------------------------------------------------
    {
        CGU_FLOAT v2_recip;
        // Normalise the axis for simplicity of future calculation
        v2_recip = (v_rgb.x*v_rgb.x + v_rgb.y*v_rgb.y + v_rgb.z*v_rgb.z);
        if (v2_recip > 0)
            v2_recip = 1.0f / (CGU_FLOAT)sqrt(v2_recip);
        else
            v2_recip = 1.0f;
        v_rgb = v_rgb*v2_recip;
    }

    // -------------------------------------------------------------------------------------
    // (6) Map the axis
    // -------------------------------------------------------------------------------------
    // the line joining (and extended on either side of) average and axis
    // defines the axis onto which the points will be projected
    // Project all the points onto the axis, calculate the distance along
    // the axis from the centre of the axis (average)
    // From Foley & Van Dam: Closest point of approach of a line (P + v) to a point (R) is
    //                            P + ((R-P).v) / (v.v))v
    // The distance along v is therefore (R-P).v / (v.v)
    // (v.v) is 1 if v is a unit vector.
    //
    // Calculate the extremities at the same time - these need to be reasonably accurately
    // represented in all cases
    //
    // In this first calculation, also find the error of mapping the points to the axis - this
    // is our major indicator of whether or not the block has compressed well - if the points
    // map well onto the axis then most of the noise introduced is high-frequency noise
    {
        left = 10000.0f;
        right = -10000.0f;
        axis_mapping_error = 0;
        for (i = 0; i < unique_pixels; i++)
        {
            // Compute the distance along the axis of the point of closest approach
            CMP_Vec3f temp = (uniques[i] - average_rgb);
            pos_on_axis[i] = (temp.x * v_rgb.x) + (temp.y * v_rgb.y) + (temp.z * v_rgb.z);

            // Compute the actual point and thence the mapping error
            rgb = uniques[i] - (average_rgb + (v_rgb * pos_on_axis[i]));
            dist_from_axis[i] = rgb.x*rgb.x + rgb.y*rgb.y + rgb.z*rgb.z;
            axis_mapping_error += dist_from_axis[i];

            // Work out the extremities
            if (pos_on_axis[i] < left)
                left = pos_on_axis[i];
            if (pos_on_axis[i] > right)
                right = pos_on_axis[i];
        }
    }

    // -------------------------------------------------------------------------------------
    // (7) Now we have a good axis and the basic information about how the points are mapped
    // to it
    // Our initial guess is to represent the endpoints accurately, by moving the average
    // to the centre and recalculating the point positions along the line
    // -------------------------------------------------------------------------------------
    {
        centre = (left + right) / 2;
        average_rgb = average_rgb + (v_rgb*centre);
        for (i = 0; i<unique_pixels; i++)
            pos_on_axis[i] -= centre;
        right -= centre;
        left -= centre;

        // Accumulate our final resultant error
        axis_mapping_error *= unique_recip * (1 / 255.0f);

    }

    // -------------------------------------------------------------------------------------
    // (8) Calculate the high and low output colour values
    // Involved in this is a rounding procedure which is undoubtedly slightly twitchy. A
    // straight rounded average is not correct, as the decompressor 'unrounds' by replicating
    // the top bits to the bottom.
    // In order to take account of this process, we don't just apply a straight rounding correction,
    // but base our rounding on the input value (a straight rounding is actually pretty good in terms of
    // error measure, but creates a visual colour and/or brightness shift relative to the original image)
    // The method used here is to apply a centre-biased rounding dependent on the input value, which was
    // (mostly by experiment) found to give minimum MSE while preserving the visual characteristics of
    // the image.
    // rgb = (average_rgb + (left|right)*v_rgb);
    // -------------------------------------------------------------------------------------
    {
        CGU_UINT32 c0, c1, t;
        int rd, gd, bd;
        rgb = (average_rgb + (v_rgb * left));
        rd = ( CGU_INT32)DCS_RED(rgb.x, rgb.y, rgb.z);
        gd = ( CGU_INT32)DCS_GREEN(rgb.x, rgb.y, rgb.z);
        bd = ( CGU_INT32)DCS_BLUE(rgb.x, rgb.y, rgb.z);
        ROUND_AND_CLAMP(rd, 5);
        ROUND_AND_CLAMP(gd, 6);
        ROUND_AND_CLAMP(bd, 5);
        c0 = ((rd & 0xf8) << 8) + ((gd & 0xfc) << 3) + ((bd & 0xf8) >> 3);

        rgb = average_rgb + (v_rgb * right);
        rd = ( CGU_INT32)DCS_RED(rgb.x, rgb.y, rgb.z);
        gd = ( CGU_INT32)DCS_GREEN(rgb.x, rgb.y, rgb.z);
        bd = ( CGU_INT32)DCS_BLUE(rgb.x, rgb.y, rgb.z);
        ROUND_AND_CLAMP(rd, 5);
        ROUND_AND_CLAMP(gd, 6);
        ROUND_AND_CLAMP(bd, 5);
        c1 = (((rd & 0xf8) << 8) + ((gd & 0xfc) << 3) + ((bd & 0xf8) >> 3));

        // Force to be a 4-colour opaque block - in which case, c0 is greater than c1
        // blocktype == 4
        {
            if (c0 < c1)
            {
                t = c0;
                c0 = c1;
                c1 = t;
                swap = 1;
            }
            else if (c0 == c1)
            {
                // This block will always be encoded in 3-colour mode
                // Need to ensure that only one of the two points gets used,
                // avoiding accidentally setting some transparent pixels into the block
                for (i = 0; i<unique_pixels; i++)
                    pos_on_axis[i] = left;
                swap = 0;
            }
            else
                swap = 0;
        }

        compressedBlock[0] = c0 | (c1 << 16);
    }

    // -------------------------------------------------------------------------------------
    // (9) Final clustering, creating the 2-bit values that define the output
    // -------------------------------------------------------------------------------------
    {
        CGU_UINT32 bit;
        CGU_FLOAT division;
        CGU_FLOAT cluster_x[4];
        CGU_FLOAT cluster_y[4];
        int cluster_count[4];

        // (blocktype == 4)
        {
            compressedBlock[1] = 0;
            division = right*2.0f / 3.0f;
            centre = (left + right) / 2;        // Actually, this code only works if centre is 0 or approximately so

            for (i = 0; i<4; i++)
            {
                cluster_x[i] = cluster_y[i] = 0.0f;
                cluster_count[i] = 0;
            }


            for (i = 0; i<16; i++)
            {
                rgb.z = pos_on_axis[index_map[i]];
                // Endpoints (indicated by block > average) are 0 and 1, while
                // interpolants are 2 and 3
                if (fabs(rgb.z) >= division)
                    bit = 0;
                else
                    bit = 2;
                // Positive is in the latter half of the block
                if (rgb.z >= centre)
                    bit += 1;
                // Set the output, taking swapping into account
                compressedBlock[1] |= ((bit^swap) << (2 * i));

                // Average the X and Y locations for each cluster
                cluster_x[bit] += (CGU_FLOAT)(i & 3);
                cluster_y[bit] += (CGU_FLOAT)(i >> 2);
                cluster_count[bit]++;
            }

            for (i = 0; i<4; i++)
            {
                CGU_FLOAT cr;
                if (cluster_count[i])
                {
                    cr = 1.0f / cluster_count[i];
                    cluster_x[i] *= cr;
                    cluster_y[i] *= cr;
                }
                else
                {
                    cluster_x[i] = cluster_y[i] = -1;
                }
            }

            // patterns in axis position detection
            // (same algorithm as used in the SSE version)
            if ((compressedBlock[0] & 0xffff) != (compressedBlock[0] >> 16))
            {
                CGU_UINT32 i1, k1;
                CGU_UINT32 x = 0, y = 0;
                int xstep = 0, ystep = 0;

                // Find a corner to search from
                for (k1 = 0; k1<4; k1++)
                {
                    switch (k1)
                    {
                    case 0:
                        x = 0; y = 0; xstep = 1; ystep = 1;
                        break;
                    case 1:
                        x = 0; y = 3; xstep = 1; ystep = -1;
                        break;
                    case 2:
                        x = 3; y = 0; xstep = -1; ystep = 1;
                        break;
                    case 3:
                        x = 3; y = 3; xstep = -1; ystep = -1;
                        break;
                    }

                    for (i1 = 0; i1<4; i1++)
                    {
                        if ((POS(x, y + ystep*i1)                < POS(x + xstep, y + ystep*i1)) ||
                            (POS(x + xstep, y + ystep*i1)        < POS(x + 2 * xstep, y + ystep*i1)) ||
                            (POS(x + 2 * xstep, y + ystep*i1)    < POS(x + 3 * xstep, y + ystep*i1))
                            )
                            break;
                        if ((POS(x + xstep*i1, y)                < POS(x + xstep*i1, y + ystep)) ||
                            (POS(x + xstep*i1, y + ystep)        < POS(x + xstep*i1, y + 2 * ystep)) ||
                            (POS(x + xstep*i1, y + 2 * ystep)    < POS(x + xstep*i1, y + 3 * ystep))
                            )
                            break;
                    }
                    if (i1 == 4)
                        break;
                }
            }
        }

    }
    // done
}

INLINE void store_uint8(CMP_GLOBAL CGU_UINT8 u_dstptr[8], CGU_UINT32 data[2])
{
   int shift = 0;
   for (CGU_INT k=0; k<4; k++)
   {
      u_dstptr[k] = (data[0] >> shift)&0xFF;
      shift += 8;
   }
   shift = 0;
   for (CGU_INT k=4; k<8; k++)
   {
      u_dstptr[k] = (data[1] >> shift)&0xFF;
      shift += 8;
   }
}

void  CompressBlockBC1_Internal(
    const CMP_Vec4uc  srcBlockTemp[16],
    CMP_GLOBAL  CGU_UINT32      compressedBlock[2],
    CMP_GLOBAL  const CMP_BC15Options *BC15options)
{
    CGU_UINT8    blkindex = 0;
    CGU_UINT8    srcindex = 0;
    CGU_UINT8    rgbBlock[64];
    for ( CGU_INT32 j = 0; j < 4; j++) {
     for ( CGU_INT32 i = 0; i < 4; i++) {
        rgbBlock[blkindex++] = (CGU_UINT8)srcBlockTemp[srcindex].z;  // B
        rgbBlock[blkindex++] = (CGU_UINT8)srcBlockTemp[srcindex].y;  // G
        rgbBlock[blkindex++] = (CGU_UINT8)srcBlockTemp[srcindex].x;  // R
        rgbBlock[blkindex++] = (CGU_UINT8)srcBlockTemp[srcindex].w;  // A
        srcindex++;
        }
    }

    CMP_BC15Options internalOptions = *BC15options;
    CalculateColourWeightings(rgbBlock, &internalOptions);

    CompressRGBBlock(rgbBlock,
                     compressedBlock,
                     &internalOptions,
                     TRUE,
                     FALSE, 
                     internalOptions.m_nAlphaThreshold);
}

//============================================== USER INTERFACES  ========================================================
#ifndef ASPM_GPU
int CMP_CDECL CreateOptionsBC1(void **options)
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

int CMP_CDECL DestroyOptionsBC1(void *options)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    CMP_BC15Options *BCOptions = reinterpret_cast <CMP_BC15Options *>(options);
    delete BCOptions;
    return CGU_CORE_OK;
}

int CMP_CDECL SetQualityBC1(void *options, 
                            CGU_FLOAT fquality)
{
    if (!options) return CGU_CORE_ERR_NEWMEM;
    CMP_BC15Options *BC15optionsDefault =  reinterpret_cast <CMP_BC15Options *>(options);
    if (fquality < 0.0f) fquality = 0.0f;
    else
    if (fquality > 1.0f) fquality = 1.0f;
    BC15optionsDefault->m_fquality = fquality;
    return CGU_CORE_OK;
}


int CMP_CDECL SetAlphaThresholdBC1(void *options, 
                                   CGU_UINT8 alphaThreshold)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    CMP_BC15Options *BC15optionsDefault =  reinterpret_cast <CMP_BC15Options *>(options);
    BC15optionsDefault->m_nAlphaThreshold = alphaThreshold;
    return CGU_CORE_OK;
}

int CMP_CDECL SetDecodeChannelMapping(void *options,
                              CGU_BOOL mapRGBA)
{
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    CMP_BC15Options *BC15optionsDefault =  reinterpret_cast <CMP_BC15Options *>(options);
    BC15optionsDefault->m_mapDecodeRGBA = mapRGBA;
    return CGU_CORE_OK;
}

int CMP_CDECL SetChannelWeightsBC1(void *options,
                              CGU_FLOAT WeightRed,
                              CGU_FLOAT WeightGreen,
                              CGU_FLOAT WeightBlue) {
    if (!options) return CGU_CORE_ERR_INVALIDPTR;
    CMP_BC15Options *BC15optionsDefault = (CMP_BC15Options *)options;

    if ((WeightRed < 0.0f)   || (WeightRed > 1.0f))      return CGU_CORE_ERR_RANGERED;
    if ((WeightGreen < 0.0f) || (WeightGreen > 1.0f))    return CGU_CORE_ERR_RANGEGREEN;
    if ((WeightBlue < 0.0f)  || (WeightBlue > 1.0f))     return CGU_CORE_ERR_RANGEBLUE;

    BC15optionsDefault->m_bUseChannelWeighting = true;
    BC15optionsDefault->m_fChannelWeights[0] = WeightRed;
    BC15optionsDefault->m_fChannelWeights[1] = WeightGreen;
    BC15optionsDefault->m_fChannelWeights[2] = WeightBlue;
    return CGU_CORE_OK;
}

int CMP_CDECL CompressBlockBC1(const unsigned char *srcBlock,
                               unsigned int srcStrideInBytes,
                               CMP_GLOBAL unsigned char cmpBlock[8],
                               const void *options = NULL) {
    CMP_Vec4uc inBlock[16];

    //----------------------------------
    // Fill the inBlock with source data
    //----------------------------------
    CGU_INT srcpos = 0;
    CGU_INT dstptr = 0;
    for (CGU_UINT8 row=0; row < 4; row++)
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

    CMP_BC15Options *BC15options = (CMP_BC15Options *)options;
    CMP_BC15Options BC15optionsDefault;
    if (BC15options == NULL)
    {
        BC15options     = &BC15optionsDefault;
        SetDefaultBC15Options(BC15options);
    }

    CompressBlockBC1_Internal(inBlock, (CMP_GLOBAL  CGU_UINT32 *)cmpBlock, BC15options);
    return CGU_CORE_OK;
}

int CMP_CDECL DecompressBlockBC1(const unsigned char cmpBlock[8], 
                                 CMP_GLOBAL unsigned char srcBlock[64],
                                 const void *options = NULL) {
    CMP_BC15Options *BC15options = (CMP_BC15Options *)options;
    CMP_BC15Options BC15optionsDefault;
    if (BC15options == NULL)
    {
        BC15options     = &BC15optionsDefault;
        SetDefaultBC15Options(BC15options);
    }
    DecompressDXTRGB_Internal(srcBlock, ( CGU_UINT32 *)cmpBlock, BC15options);


    return CGU_CORE_OK;
}
#endif

//============================================== OpenCL USER INTERFACE ========================================================
#ifdef ASPM_GPU
CMP_STATIC CMP_KERNEL void CMP_GPUEncoder(
    CMP_GLOBAL  const CMP_Vec4uc*   ImageSource,
    CMP_GLOBAL  CGU_UINT8*          ImageDestination,
    CMP_GLOBAL  Source_Info*        SourceInfo,
    CMP_GLOBAL  CMP_BC15Options*    BC15options
)
{
    CGU_UINT32 xID;
    CGU_UINT32 yID;

//printf("SourceInfo: (H:%d,W:%d) Quality %1.2f \n", SourceInfo->m_src_height, SourceInfo->m_src_width, SourceInfo->m_fquality);
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

    CGU_UINT32 destI = (xID*BC1CompBlockSize) + (yID*(srcWidth / BlockX)*BC1CompBlockSize);
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

    // fast low quality mode that matches v3.1 code
    if (SourceInfo->m_fquality <= 0.04f)
        CompressBlockBC1_Fast(srcData, (CMP_GLOBAL  CGU_UINT32 *)&ImageDestination[destI]);
    else
        CompressBlockBC1_Internal(srcData, (CMP_GLOBAL  CGU_UINT32 *)&ImageDestination[destI], BC15options);
}
#endif
