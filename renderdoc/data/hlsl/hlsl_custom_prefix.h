
const char HLSL_CUSTOM_PREFIX[] =
    R"EOPREFIX(
#define RD_FLOAT_1D_ARRAY_BINDING t1
#define RD_FLOAT_1D_BINDING t1 // all textures treated as arrays, add macro aliases

#define RD_FLOAT_2D_ARRAY_BINDING t2
#define RD_FLOAT_2D_BINDING t2

#define RD_FLOAT_3D_BINDING t3

#define RD_FLOAT_DEPTH_BINDING t4
#define RD_FLOAT_DEPTH_ARRAY_BINDING t4

#define RD_FLOAT_STENCIL_BINDING t5
#define RD_FLOAT_STENCIL_ARRAY_BINDING t5

#define RD_FLOAT_DEPTHMS_BINDING t6
#define RD_FLOAT_DEPTHMS_ARRAY_BINDING t6

#define RD_FLOAT_STENCILMS_BINDING t7
#define RD_FLOAT_STENCILMS_ARRAY_BINDING t7

#define RD_FLOAT_2DMS_ARRAY_BINDING t9
#define RD_FLOAT_2DMS_BINDING t9

#define RD_FLOAT_YUV_ARRAY_BINDING t20
#define RD_FLOAT_YUV_BINDING t20

#define RD_INT_1D_ARRAY_BINDING t21
#define RD_INT_1D_BINDING t21

#define RD_INT_2D_ARRAY_BINDING t22
#define RD_INT_2D_BINDING t22

#define RD_INT_3D_BINDING t23

#define RD_INT_2DMS_ARRAY_BINDING t29
#define RD_INT_2DMS_BINDING t29

#define RD_UINT_1D_ARRAY_BINDING t11
#define RD_UINT_1D_BINDING t11

#define RD_UINT_2D_ARRAY_BINDING t12
#define RD_UINT_2D_BINDING t12

#define RD_UINT_3D_BINDING t13

#define RD_UINT_2DMS_ARRAY_BINDING t19
#define RD_UINT_2DMS_BINDING t19

#define RD_POINT_SAMPLER_BINDING s0
#define RD_LINEAR_SAMPLER_BINDING s1

#define RD_CONSTANT_BUFFER_BINDING b0

cbuffer RD_CBuffer_Type : register(RD_CONSTANT_BUFFER_BINDING)
{
	struct RD_CBuffer_Struct
	{
		uint4 TexDim;
		uint SelectedMip;
		uint TextureType;
		uint SelectedSliceFace;
		int SelectedSample;
		uint4 YUVDownsampleRate;
		uint4 YUVAChannels;
		float2 SelectedRange;
  } RD_CBuffer_Data;
};

#define RD_TextureType_1D 1
#define RD_TextureType_2D 2
#define RD_TextureType_3D 3
#define RD_TextureType_Depth 4
#define RD_TextureType_DepthStencil 5
#define RD_TextureType_DepthMS 6
#define RD_TextureType_DepthStencilMS 7
#define RD_TextureType_2DMS 9

// for compatibility
#define RD_TextureType_Cube 999
#define RD_TextureType_Cube_Array 999
#define RD_TextureType_1D_Array 999
#define RD_TextureType_2D_Array 999
#define RD_TextureType_Rect 999
#define RD_TextureType_Buffer 999
#define RD_TextureType_2DMS_Array 999

// possible values (these are only return values from this function, NOT texture binding points):
// RD_TextureType_1D
// RD_TextureType_2D
// RD_TextureType_3D
// RD_TextureType_Depth (D3D only)
// RD_TextureType_DepthStencil (D3D only)
// RD_TextureType_DepthMS (D3D only)
// RD_TextureType_DepthStencilMS (D3D only)
// RD_TextureType_2DMS
uint RD_TextureType()
{
  return RD_CBuffer_Data.TextureType;
}

// selected sample, or -numSamples for resolve
int RD_SelectedSample()
{
  return RD_CBuffer_Data.SelectedSample;
}

uint RD_SelectedSliceFace()
{
  return RD_CBuffer_Data.SelectedSliceFace;
}

uint RD_SelectedMip()
{
  return RD_CBuffer_Data.SelectedMip;
}

// xyz = width, height, depth (or array size). w = # mips
uint4 RD_TexDim()
{
  return RD_CBuffer_Data.TexDim;
}

// x = horizontal downsample rate (1 full rate, 2 half rate)
// y = vertical downsample rate
// z = number of planes in input texture
// w = number of bits per component (8, 10, 16)
uint4 RD_YUVDownsampleRate()
{
  return RD_CBuffer_Data.YUVDownsampleRate;
}

// x = where Y channel comes from
// y = where U channel comes from
// z = where V channel comes from
// w = where A channel comes from
// each index will be [0,1,2,3] for xyzw in first plane,
// [4,5,6,7] for xyzw in second plane texture, etc.
// it will be 0xff = 255 if the channel does not exist.
uint4 RD_YUVAChannels()
{
  return RD_CBuffer_Data.YUVAChannels;
}

// a pair with minimum and maximum selected range values
float2 RD_SelectedRange()
{
  return RD_CBuffer_Data.SelectedRange;
}

)EOPREFIX";
