/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "3rdparty/fmt/core.h"
#include "d3d12_test.h"

RD_TEST(D3D12_Reflection_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests every kind of resource that could be reflected, to test that reflection is accurate "
      "on DXBC and DXIL.";

  std::string respixel = R"EOSHADER(

// ensure the source being passed through can preserve unicode characters

// Iñtërnâtiônàližætiøn

SamplerState s1 : register(s5);
SamplerComparisonState s2 : register(s8);

struct nested
{
  row_major float2x3 x;
};

struct buf_struct
{
  float a;
  float b[2];
  nested c;
};

// start with every texture dimension at float4
Texture1D<float4> tex1d : register(t0);
Texture2D<float4> tex2d : register(t1);
Texture3D<float4> tex3d : register(t2);
Texture1DArray<float4> tex1darray : register(t3);
Texture2DArray<float4> tex2darray : register(t4);
TextureCube<float4> texcube : register(t5);
TextureCubeArray<float4> texcubearray : register(t6);
Texture2DMS<float4> tex2dms : register(t7);
Texture2DMSArray<float4, 2> tex2dmsarray : register(t8);

// now check textures with different return types and sizes. Stick to 2D textures for simplicity
Texture2D<float> tex2d_f1 : register(t10);
Texture2D<float2> tex2d_f2 : register(t11);
Texture2D<float3> tex2d_f3 : register(t12);
Texture2D<uint2> tex2d_u2 : register(t13);
Texture2D<uint3> tex2d_u3 : register(t14);
Texture2D<int2> tex2d_i2 : register(t15);
Texture2D<int3> tex2d_i3 : register(t16);

// check MSAA textures with different sample counts (we don't reflect this info but we should handle these types still)
Texture2DMS<float2, 4> msaa_flt2_4x : register(t17);
Texture2DMS<float3, 2> msaa_flt3_2x : register(t18);
Texture2DMS<float4, 8> msaa_flt4_8x : register(t19);

// buffer textures / typed buffers
Buffer<float> buf_f1 : register(t20);
Buffer<float2> buf_f2 : register(t21);
Buffer<float3> buf_f3 : register(t22);
Buffer<float4> buf_f4 : register(t23);
Buffer<uint2> buf_u2 : register(t24);
Buffer<int3> buf_i3 : register(t25);

// byte address buffer
ByteAddressBuffer bytebuf : register(t30);

// structured buffer
StructuredBuffer<buf_struct> strbuf : register(t40);
StructuredBuffer<float2> strbuf_f2 : register(t41);

// arrayed resources
Texture2DArray<float> tex2dArray[4] : register(t50);

// now UAVs

RWTexture1D<float4> rwtex1d : register(u0);
RWTexture2D<float4> rwtex2d : register(u1);
RWTexture3D<float4> rwtex3d : register(u2);
RWTexture1DArray<float4> rwtex1darray : register(u3);
RWTexture2DArray<float4> rwtex2darray : register(u4);

RWTexture2D<float> rwtex2d_f1 : register(u10);
RWTexture2D<float2> rwtex2d_f2 : register(u11);
RWTexture2D<float3> rwtex2d_f3 : register(u12);
RWTexture2D<uint2> rwtex2d_u2 : register(u13);
RWTexture2D<uint3> rwtex2d_u3 : register(u14);
RWTexture2D<int2> rwtex2d_i2 : register(u15);
RWTexture2D<int3> rwtex2d_i3 : register(u16);

RWBuffer<float> rwbuf_f1 : register(u20);
RWBuffer<float2> rwbuf_f2 : register(u21);
RWBuffer<float3> rwbuf_f3 : register(u22);
RWBuffer<float4> rwbuf_f4 : register(u23);
RWBuffer<uint2> rwbuf_u2 : register(u24);
RWBuffer<int3> rwbuf_i3 : register(u25);

// ROV
#if ROV
RasterizerOrderedTexture2D<float4> rov : register(u30);
#endif

// byte address buffer
RWByteAddressBuffer rwbytebuf : register(u40);

// structured buffer
RWStructuredBuffer<buf_struct> rwstrbuf : register(u50);
RWStructuredBuffer<buf_struct> rwcounter : register(u51);
AppendStructuredBuffer<buf_struct> rwappend : register(u52);
ConsumeStructuredBuffer<buf_struct> rwconsume : register(u53);
RWStructuredBuffer<float2> rwstrbuf_f2 : register(u54);

float4 main(float4 pos : SV_Position) : SV_Target0
{
	float4 ret = float4(0,0,0,0);

  uint4 indices = ((uint4)pos.xyzw) % uint4(4, 5, 6, 7);

  ret.xyzw += tex1d.Sample(s1, pos.x);
  ret.xyzw += tex2d.Sample(s1, pos.xy);
  ret.xyzw += tex3d.Sample(s1, pos.xyz);
  ret.xyzw += tex1darray.Sample(s1, pos.xy);
  ret.xyzw += tex2darray.Sample(s1, pos.xyz);
  ret.xyzw += texcube.Sample(s1, pos.xyz);
  ret.xyzw += texcubearray.Sample(s1, pos.xyzw);
  ret.xyzw += tex2dms.Load(indices.xy, 0);
  ret.xyzw += tex2dmsarray.Load(indices.xyz, 0);
		
	ret.x += tex2d_f1.Load(indices.xyz);
	ret.xy += tex2d_f2.Load(indices.xyz);
	ret.xyz += tex2d_f3.Load(indices.xyz);
	ret.xy += (float2)tex2d_u2.Load(indices.xyz);
	ret.xyz += (float3)tex2d_u3.Load(indices.xyz);
	ret.xy += (float2)tex2d_i2.Load(indices.xyz);
	ret.xyz += (float3)tex2d_i3.Load(indices.xyz);
	
  ret.xy += msaa_flt2_4x.Load(indices.xy, 0);
  ret.xyz += msaa_flt3_2x.Load(indices.xy, 0);
  ret.xyzw += msaa_flt4_8x.Load(indices.xy, 0);
	
  ret.x += buf_f1[indices.x];
  ret.xy += buf_f2[indices.x];
  ret.xyz += buf_f3[indices.x];
  ret.xyzw += buf_f4[indices.x];
  ret.xy += (float2)buf_u2[indices.x];
  ret.xyz += (float3)buf_i3[indices.x];
  
  ret.xyzw += asfloat(bytebuf.Load4(indices.y));
  
  ret.x += strbuf[indices.y].a;
  ret.xy += mul(strbuf[indices.z].c.x, ret.xyz);
  ret.xy += strbuf_f2[indices.y];
  
  ret += tex2dArray[NonUniformResourceIndex(indices.x)].Load(indices.xyzw);

  rwtex1d[indices.x] = ret.xyzw;
  rwtex2d[indices.xy] = ret.xyzw;
  rwtex3d[indices.xyz] = ret.xyzw;
  rwtex1darray[indices.xy] = ret.xyzw;
  rwtex2darray[indices.xyz] = ret.xyzw;
  
  rwtex2d_f1[indices.xy] = ret.x;
  rwtex2d_f2[indices.xy] = ret.xy;
  rwtex2d_f3[indices.xy] = ret.xyz;
  rwtex2d_u2[indices.xy] = (uint2)ret.xy;
  rwtex2d_u3[indices.xy] = (uint3)ret.xyz;
  rwtex2d_i2[indices.xy] = (int2)ret.xy;
  rwtex2d_i3[indices.xy] = (int3)ret.xyz;

	rwbuf_f1[indices.x] = ret.x;
	rwbuf_f2[indices.x] = ret.xy;
	rwbuf_f3[indices.x] = ret.xyz;
	rwbuf_f4[indices.x] = ret.xyzw;
	rwbuf_u2[indices.x] = (uint2)ret.xy;
	rwbuf_i3[indices.x] = (int3)ret.xyz;
	
#if ROV
  rov[pos.xy] = sqrt(rov[pos.xy]) + ret;
#endif
	
  rwbytebuf.Store4(indices.y, asuint(ret));

  buf_struct dummy = rwconsume.Consume();

  rwstrbuf[indices.y] = dummy;
  rwstrbuf_f2[indices.y] = ret.xy;

  rwappend.Append(dummy);

  uint idx = rwcounter.IncrementCounter();

  rwcounter[idx] = dummy;

	return ret;
}

)EOSHADER";

  struct PSOs
  {
    const char *name;
    ID3D12PipelineStatePtr res;
  };

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vs5blob = Compile(D3DFullscreenQuadVertex, "main", "vs_5_0");
    ID3DBlobPtr vs6blob = m_DXILSupport ? Compile(D3DFullscreenQuadVertex, "main", "vs_6_0") : NULL;

    ID3D12PipelineStatePtr dxbc, dxil;

    D3D12_STATIC_SAMPLER_DESC samp = {
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,
        0,
        D3D12_COMPARISON_FUNC_ALWAYS,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        0.0f,
        0.0f,
        0,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL,
    };

    D3D12_STATIC_SAMPLER_DESC samps[2];
    samps[0] = samps[1] = samp;
    samps[0].ShaderRegister = 5;
    samps[1].ShaderRegister = 8;

    D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                                         D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

    ID3D12RootSignaturePtr sig = MakeSig(
        {
            tableParam(vis, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 0, 100, 0, flags),
            tableParam(vis, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 100, 100, flags),
            tableParam(vis, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 100, 200, flags),
        },
        D3D12_ROOT_SIGNATURE_FLAG_NONE, 2, samps);

    D3D12PSOCreator creator =
        MakePSO().RootSig(sig).RTVs({DXGI_FORMAT_R8G8B8A8_UNORM_SRGB}).VS(vs5blob);

    respixel = fmt::format("#define ROV {0}\n\n{1}", opts.ROVsSupported ? 1 : 0, respixel);

    ID3DBlobPtr dxbcBlob = Compile(respixel, "main", "ps_5_1");
    dxbc = creator.VS(vs5blob).PS(dxbcBlob);
    if(m_DXILSupport)
    {
      ID3DBlobPtr dxilBlob = Compile(respixel, "main", "ps_6_0");
      dxil = creator.VS(vs6blob).PS(dxilBlob);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    D3D12_SHADER_RESOURCE_VIEW_DESC defaultSrvDesc = {};
    defaultSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    defaultSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // make valid NULL descriptors
    D3D12_CPU_DESCRIPTOR_HANDLE start = m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart();
    UINT increment = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cur;

    // t0...
    cur.ptr = start.ptr + (100 + 0) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
    srvDesc.Texture1D.MipLevels = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 1) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 2) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 3) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    srvDesc.Texture1DArray.MipLevels = 1;
    srvDesc.Texture1DArray.ArraySize = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 4) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 5) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 6) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    srvDesc.TextureCubeArray.MipLevels = 1;
    srvDesc.TextureCubeArray.NumCubes = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 7) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 8) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    srvDesc.Texture2DMSArray.ArraySize = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 10) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 11) * increment;
    srvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 12) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 13) * increment;
    srvDesc.Format = DXGI_FORMAT_R8G8_UINT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 14) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 15) * increment;
    srvDesc.Format = DXGI_FORMAT_R8G8_SINT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 16) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 17) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 18) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 19) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 20) * increment;
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 21) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 22) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 23) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 24) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32_UINT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 25) * increment;
    srvDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 30) * increment;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 40) * increment;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Buffer.StructureByteStride = 16;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 41) * increment;
    srvDesc.Buffer.StructureByteStride = 8;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    cur.ptr = start.ptr + (100 + 50) * increment;
    srvDesc = defaultSrvDesc;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = 1;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);
    cur.ptr += increment;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);
    cur.ptr += increment;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);
    cur.ptr += increment;
    dev->CreateShaderResourceView(NULL, &srvDesc, cur);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC defaultUavDesc = {};
    defaultUavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // u0...
    cur.ptr = start.ptr + (200 + 0) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 1) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 2) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 3) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
    uavDesc.Texture1DArray.ArraySize = 1;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 4) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Texture2DArray.ArraySize = 1;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 10) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8_UNORM;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 11) * increment;
    uavDesc.Format = DXGI_FORMAT_R8G8_UNORM;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 12) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 13) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32_UINT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 14) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 15) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32_SINT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 16) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 20) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = 1;
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 21) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 22) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 23) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 24) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32_UINT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 25) * increment;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 30) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 40) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.Buffer.NumElements = 1;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 50) * increment;
    uavDesc = defaultUavDesc;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.Buffer.NumElements = 1;
    uavDesc.Buffer.StructureByteStride = 16;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    // NULL resources don't have to do anything special for the counter

    cur.ptr = start.ptr + (200 + 51) * increment;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 52) * increment;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 53) * increment;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    cur.ptr = start.ptr + (200 + 54) * increment;
    uavDesc.Buffer.StructureByteStride = 8;
    dev->CreateUnorderedAccessView(NULL, NULL, &uavDesc, cur);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, bbrtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootSignature(sig);
      cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRootDescriptorTable(2, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {bbrtv}, {});

      setMarker(cmd, "DXBC");
      cmd->SetPipelineState(dxbc);
      cmd->DrawInstanced(3, 1, 0, 0);

      if(m_DXILSupport)
      {
        setMarker(cmd, "DXIL");
        cmd->SetPipelineState(dxil);
        cmd->DrawInstanced(3, 1, 0, 0);
      }

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
