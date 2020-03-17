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
#ifndef BC6_ENCODE_KERNEL_H
#define BC6_ENCODE_KERNEL_H

#include "Common_Def.h"

#define MAX_TRACE 10
#define MAX_ENTRIES_QUANT_TRACE 16
#define BlockX 4
#define BlockY 4
#define BYTEPP 4
#define COMPRESSED_BLOCK_SIZE 16    // Size of a compressed block in bytes
#define MAX_DIMENSION_BIG 4
#define MAX_SUBSET_SIZE 16    // Largest possible size for an individual subset
#define NUM_BLOCK_TYPES 8     // Number of block types in the format
#define MAX_SUBSETS 3         // Maximum number of possible subsets
#define MAX_PARTITIONS 64     // Maximum number of partition types
#define MAX_ENTRIES 64
#define MAX_TRY 20

#define MAX_PARTITIONS_TABLE (1 + 64 + 64)
#define DIMENSION 4
#define MAX_CLUSTERS_BIG 16
#define EPSILON 0.000001
#define MAX_CLUSTERS_QUANT_TRACE 8

//# Image Quality will increase as this number gets larger and end-to-end performance time will
// reduce
#define MAX_INDEX_BITS 4
#define HIGHQULITY_THRESHOLD 0.7F
#define qFAST_THRESHOLD 0.5F

#define F16NEGPREC_LIMIT_VAL -2048.0f    // f16 negative precision limit value

#define LOG_CL_RANGE 5
#define LOG_CL_BASE 2
#define BIT_BASE 5
#define BIT_RANGE 9
#define MAX_CLUSTERS 8
#define BTT(bits) (bits - BIT_BASE)
#define CLT(cl) (cl - LOG_CL_BASE)
#define MASK(n) ((1 << (n)) - 1)
#define SIGN_EXTEND_TYPELESS(x, nb) ((((x) & (1 << ((nb)-1))) ? ((~0) << (nb)) : 0) | (x))
#define CMP_HALF_MAX 65504.0f    // positive half max

#ifndef ASPM_GPU
#include <assert.h>
#include <bitset>
// typedef uint8_t        byte;
#else
// typedef bitset       uint8_t;
// typedef uint8          byte;
#endif

#define BC6CompBlockSize 16
#define BC6BlockX 4
#define BC6BlockY 4

typedef struct
{
  CGU_INT k;
  CGU_FLOAT d;
} BC6H_TRACE;

#define NCHANNELS 3
#define MAX_END_POINTS 2
#define MAX_BC6H_MODES 14
#define MAX_BC6H_PARTITIONS 32
#define MAX_TWOREGION_MODES 10
#define COMPRESSED_BLOCK_SIZE 16    // Size of a compressed block in bytes
#define ONE_REGION_INDEX_OFFSET \
  65    // bit location to start saving color index values for single region shape
#define TWO_REGION_INDEX_OFFSET \
  82    // bit location to start saving color index values for two region shapes
#define MIN_MODE_FOR_ONE_REGION 11    // Two regions shapes use modes 1..9 and single use 11..14
#define R_0(ep) (ep)[0][0][i]
#define R_1(ep) (ep)[0][1][i]
#define R_2(ep) (ep)[1][0][i]
#define R_3(ep) (ep)[1][1][i]
#define FLT16_MAX 0x7bff

#ifndef ASPM_GPU
#define USE_SHAKERHD
#endif

#define USE_NEWRAMP

typedef struct
{
  CGU_FLOAT A[NCHANNELS];
  CGU_FLOAT B[NCHANNELS];
} END_Points;

typedef struct
{
  CGU_FLOAT x, y, z;
} BC6H_Vec3f;

typedef struct
{
  CGU_INT nbits;          // Number of bits
  CGU_INT prec[3];        // precission of the Qunatized RGB endpoints
  CGU_INT transformed;    // if 0, deltas are unsigned and no transform; otherwise, signed and
                          // transformed
  CGU_INT modebits;       // number of mode bits
  CGU_INT IndexPrec;      // Index Precision
  CGU_INT mode;           // Mode value to save
  CGU_INT lowestPrec;     // Step size of each precesion incriment
} ModePartitions;

__constant ModePartitions ModePartition[MAX_BC6H_MODES + 1] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,    // Mode = Invaild

    // Two region Partition
    10, 5, 5, 5, 1, 2, 3, 0x00, 31,    // Mode = 1
    7, 6, 6, 6, 1, 2, 3, 0x01, 248,    // Mode = 2
    11, 5, 4, 4, 1, 5, 3, 0x02, 15,    // Mode = 3
    11, 4, 5, 4, 1, 5, 3, 0x06, 15,    // Mode = 4
    11, 4, 4, 5, 1, 5, 3, 0x0a, 15,    // Mode = 5
    9, 5, 5, 5, 1, 5, 3, 0x0e, 62,     // Mode = 6
    8, 6, 5, 5, 1, 5, 3, 0x12, 124,    // Mode = 7
    8, 5, 6, 5, 1, 5, 3, 0x16, 124,    // Mode = 8
    8, 5, 5, 6, 1, 5, 3, 0x1a, 124,    // Mode = 9
    6, 6, 6, 6, 0, 5, 3, 0x1e, 496,    // Mode = 10

    // One region Partition
    10, 10, 10, 10, 0, 5, 4, 0x03, 31,    // Mode = 11
    11, 9, 9, 9, 1, 5, 4, 0x07, 15,       // Mode = 12
    12, 8, 8, 8, 1, 5, 4, 0x0b, 7,        // Mode = 13
    16, 4, 4, 4, 1, 5, 4, 0x0f, 1,        // Mode = 14
};

//================================================
// Mode Pathern order to try on endpoints
// The order can be rearranged to set which modes gets processed first
// for now it is set in order.
//================================================
__constant CGU_INT8 ModeFitOrder[MAX_BC6H_MODES + 1] = {
    0,    // 0: N/A
    // ----  2 region lower bits ---
    1,     // 10 5 5 5
    2,     // 7  6 6 6
    3,     // 11 5 4 5
    4,     // 11 4 5 4
    5,     // 11 4 4 5
    6,     // 9  5 5 5
    7,     // 8  6 5 5
    8,     // 8  5 6 5
    9,     // 8  5 5 6
    10,    // 6  6 6 6
    //------ 1 region high bits ---
    11,    // 10 10 10 10
    12,    // 11 9  9  9
    13,    // 12 8  8  8
    14     // 16 4  4  4
};

// The Region2FixUps are for our index[subset = 2][16][3] locations
// indexed by shape region 2
__constant CGU_INT g_Region2FixUp[32] = {
    7, 3, 11, 7, 3, 11, 9, 5, 2, 12, 7, 3, 11, 7, 11, 3,
    7, 1, 0,  1, 0, 1,  0, 7, 0, 1,  1, 0, 4,  4, 1,  0,
};

// Indexed by all shape regions
// Partition Set Fixups for region 1 note region 0 is always at 0
// that means normally we use 3 bits to define an index value
// if its at the fix up location then its one bit less
__constant CGU_INT g_indexfixups[32] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 2,  8,  2,  2,  8,  8,  15, 2,  8,  2,  2,  8,  8,  2,  2,
};

typedef struct
{
  CGU_INT8 region;           // one or two
  CGU_INT8 m_mode;           // m
  CGU_INT8 d_shape_index;    // d
  CGU_INT rw;                // endpt[0].A[0]
  CGU_INT rx;                // endpt[0].B[0]
  CGU_INT ry;                // endpt[1].A[0]
  CGU_INT rz;                // endpt[1].B[0]
  CGU_INT gw;                // endpt[0].A[1]
  CGU_INT gx;                // endpt[0].B[1]
  CGU_INT gy;                // endpt[1].A[1]
  CGU_INT gz;                // endpt[1].B[1]
  CGU_INT bw;                // endpt[0].A[2]
  CGU_INT bx;                // endpt[0].B[2]
  CGU_INT by;                // endpt[1].A[2]
  CGU_INT bz;                // endpt[1].B[2]

  union
  {
    CGU_UINT8 indices[4][4];    // Indices data after header block
    CGU_UINT8 indices16[16];
  };

  union
  {
    CGU_FLOAT din[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];    // Original data input as floats
    unsigned char cdin[256];                              // as uchar to match float
  };

  END_Points
      EC[MAX_END_POINTS];    // compressed endpoints expressed as endpt[0].A[] and endpt[1].B[]
  END_Points E[MAX_END_POINTS];    // decompressed endpoints
  CGU_BOOL issigned;               // Format is 16 bit signed floating point
  CGU_BOOL istransformed;          // region two: all modes = true except mode=10
  short wBits;                     // number of bits for the root endpoint
  short tBits[NCHANNELS];          // number of bits used for the transformed endpoints
  CGU_INT format;                  // floating point format are we using for decompression
  BC6H_Vec3f Paletef[2][16];

  CGU_INT index;    // for debugging
  CGU_FLOAT fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
  CGU_FLOAT cur_best_fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
  CGU_INT shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE];
  CGU_INT cur_best_shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE];
  CGU_INT entryCount[MAX_SUBSETS];
  CGU_INT cur_best_entryCount[MAX_SUBSETS];
  CGU_FLOAT partition[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
  CGU_FLOAT cur_best_partition[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
  CGU_BOOL optimized;    // were end points optimized during final encoding

} BC6H_Encode_local;

#ifndef ASPM_GPU
using namespace std;
class BitHeader
{
public:
  BitHeader(const CGU_UINT8 in[], CGU_INT sizeinbytes)
  {
    m_bits.reset();
    m_sizeinbytes = sizeinbytes;

    if((in != NULL) && (sizeinbytes <= 16))
    {
      // Init bits set with given data
      CGU_INT bitpos = 0;
      for(CGU_INT i = 0; i < sizeinbytes; i++)
      {
        CGU_INT bit = 1;
        for(CGU_INT j = 0; j < 8; j++)
        {
          m_bits[bitpos] = in[i] & bit ? 1 : 0;
          bit = bit << 1;
          bitpos++;
        }
      }
    }
  }

  ~BitHeader() {}
  void transferbits(CGU_UINT8 in[], CGU_INT sizeinbytes)
  {
    if((sizeinbytes <= m_sizeinbytes) && (in != NULL))
    {
      // Init bits set with given data
      memset(in, 0, sizeinbytes);
      CGU_INT bitpos = 0;
      for(CGU_INT i = 0; i < sizeinbytes; i++)
      {
        CGU_INT bit = 1;
        for(CGU_INT j = 0; j < 8; j++)
        {
          if(m_bits[bitpos])
            in[i] |= bit;
          bit = bit << 1;
          bitpos++;
        }
      }
    }
  }

  CGU_INT getvalue(CGU_INT start, CGU_INT bitsize)
  {
    CGU_INT value = 0;
    CGU_INT end = start + bitsize - 1;
    for(; end >= start; end--)
    {
      value |= m_bits[end] ? 1 : 0;
      if(end > start)
        value <<= 1;
    }

    return value;
  }

  void setvalue(CGU_INT start, CGU_INT bitsize, CGU_INT value, CGU_INT maskshift = 0)
  {
    CGU_INT end = start + bitsize - 1;
    CGU_INT mask = 0x1 << maskshift;
    for(; start <= end; start++)
    {
      m_bits[start] = (value & mask) ? 1 : 0;
      mask <<= 1;
    }
  }

  bitset<128> m_bits;    // 16 bytes max
  CGU_INT m_sizeinbytes;
};

//==================== DECODER CODE ======================
#define MAXENDPOINTS 2
#define U16MAX 0xffff
#define S16MAX 0x7fff
#define SIGN_EXTEND(w, tbits) \
  ((((signed(w)) & (1 << ((tbits)-1))) ? ((~0) << (tbits)) : 0) | (signed(w)))

enum
{
  UNSIGNED_F16 = 1,
  SIGNED_F16 = 2
};

enum
{
  BC6_ONE = 0,
  BC6_TWO
};

enum
{
  C_RED = 0,
  C_GREEN,
  C_BLUE
};

struct BC6H_Vec3
{
  int x, y, z;
};

struct AMD_BC6H_Format
{
  unsigned short region;    // one or two
  unsigned short m_mode;    // m
  int d_shape_index;        // d
  int rw;                   // endpt[0].A[0]
  int rx;                   // endpt[0].B[0]
  int ry;                   // endpt[1].A[0]
  int rz;                   // endpt[1].B[0]
  int gw;                   // endpt[0].A[1]
  int gx;                   // endpt[0].B[1]
  int gy;                   // endpt[1].A[1]
  int gz;                   // endpt[1].B[1]
  int bw;                   // endpt[0].A[2]
  int bx;                   // endpt[0].B[2]
  int by;                   // endpt[1].A[2]
  int bz;                   // endpt[1].B[2]

  union
  {
    CGU_UINT8 indices[4][4];    // Indices data after header block
    CGU_UINT8 indices16[16];
  };

  float din[MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];    // Original data input
  END_Points EC[MAXENDPOINTS];    // compressed endpoints expressed as endpt[0].A[] and endpt[1].B[]
  END_Points E[MAXENDPOINTS];     // decompressed endpoints
  bool issigned;                  // Format is 16 bit signed floating point
  bool istransformed;             // region two: all modes = true except mode=10
  short wBits;                    // number of bits for the root endpoint
  short tBits[NCHANNELS];         // number of bits used for the transformed endpoints
  int format;                     // floating point format are we using for decompression
  BC6H_Vec3 Palete[2][16];
  BC6H_Vec3f Paletef[2][16];

  int index;    // for debugging
  float fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
  float cur_best_fEndPoints[MAX_SUBSETS][MAX_END_POINTS][MAX_DIMENSION_BIG];
  int shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE];
  int cur_best_shape_indices[MAX_SUBSETS][MAX_SUBSET_SIZE];
  int entryCount[MAX_SUBSETS];
  int cur_best_entryCount[MAX_SUBSETS];
  float partition[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
  float cur_best_partition[MAX_SUBSETS][MAX_SUBSET_SIZE][MAX_DIMENSION_BIG];
  bool optimized;    // were end points optimized during final encoding
};

// ===================================  END OF DECODER CODE
// ========================================================
#endif

//-------------------------------------------------
// Set by Host : Read only in kernel
//-------------------------------------------------
typedef struct
{
  // Setup at initialization time
  CGU_FLOAT m_quality;
  CGU_FLOAT m_performance;
  CGU_FLOAT m_errorThreshold;
  CGU_DWORD m_validModeMask;
  CGU_BOOL m_imageNeedsAlpha;
  CGU_BOOL m_colourRestrict;
  CGU_BOOL m_alphaRestrict;
  CGU_BOOL m_isSigned;
} CMP_BC6HOptions;

typedef struct
{
  // These are quality parameters used to select when to use the high precision quantizer
  // and shaker paths
  CGU_FLOAT m_quantizerRangeThreshold;
  CGU_FLOAT m_shakerRangeThreshold;
  CGU_FLOAT m_partitionSearchSize;

  // Setup at initialization time
  CGU_FLOAT m_quality;
  CGU_FLOAT m_performance;
  CGU_FLOAT m_errorThreshold;
  CGU_DWORD m_validModeMask;
  CGU_BOOL m_imageNeedsAlpha;
  CGU_BOOL m_colourRestrict;
  CGU_BOOL m_alphaRestrict;
  CGU_BOOL m_isSigned;

  // Source image info : must be set prior to use in kernel
  CGU_UINT32 m_src_width;
  CGU_UINT32 m_src_height;
  CGU_UINT32 m_src_stride;

} BC6H_Encode;

CMP_STATIC void SetDefaultBC6Options(BC6H_Encode *BC6Encode)
{
  if(BC6Encode)
  {
    BC6Encode->m_quality = 1.0f;
    BC6Encode->m_quantizerRangeThreshold = 0.0f;
    BC6Encode->m_shakerRangeThreshold = 0.0f;
    BC6Encode->m_partitionSearchSize = 0.20f;
    BC6Encode->m_performance = 0.0f;
    BC6Encode->m_errorThreshold = 0.0f;
    BC6Encode->m_validModeMask = 0;
    BC6Encode->m_imageNeedsAlpha = 0;
    BC6Encode->m_colourRestrict = 0;
    BC6Encode->m_alphaRestrict = 0;
    BC6Encode->m_isSigned = 0;
    BC6Encode->m_src_width = 4;
    BC6Encode->m_src_height = 4;
    BC6Encode->m_src_stride = 0;
  }
}

#endif