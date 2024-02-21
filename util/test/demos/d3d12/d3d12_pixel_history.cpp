/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2024 Baldur Karlsson
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

#include "d3d12_test.h"

RD_TEST(D3D12_Pixel_History, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests pixel history";

  std::string common = R"EOSHADER(
struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

)EOSHADER";

  const std::string vertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f main(vertin IN, uint vid : SV_VertexID)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos, 1.0f);
	OUT.col = IN.col;
	OUT.uv = IN.uv;

	return OUT;
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

float4 main(v2f vertIn) : SV_Target0
{
  if (vertIn.pos.x < 151 && vertIn.pos.x > 150)
    discard;
	return vertIn.col + float4(0, 0, 0, 1.75);
}

)EOSHADER";

  std::string mspixel = R"EOSHADER(

float4 main(v2f vertIn, uint primId : SV_PrimitiveID, uint sampleId : SV_SampleIndex) : SV_Target0
{
  float4 color = (float4)0;
  if(primId == 0)
  {
    color = float4(1, 0, 1, 2.75);
  }
  else
  {
    if (sampleId == 0)
      color = float4(1, 0, 0, 2.75);
    else if (sampleId == 1)
      color = float4(0, 0, 1, 2.75);
    else if (sampleId == 2)
      color = float4(0, 1, 1, 2.75);
    else if (sampleId == 3)
      color = float4(1, 1, 1, 2.75);
  }

  return color;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    DefaultA2V VBData[] = {
        // this triangle occludes in depth
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle occludes in stencil
        {Vec3f(-0.5f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.5f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle is just in the background to contribute to overdraw
        {Vec3f(-0.9f, -0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.9f, -0.9f, 0.95f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // the draw has a few triangles, main one that is occluded for depth, another that is
        // adding to overdraw complexity, one that is backface culled, then a few more of various
        // sizes for triangle size overlay
        {Vec3f(-0.3f, -0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.5f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.5f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.2f, -0.2f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.2f, 0.0f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.2f, -0.4f, 0.6f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // backface culled
        {Vec3f(0.1f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.5f, -0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // depth clipped (i.e. not clamped)
        {Vec3f(0.6f, 0.0f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, 0.2f, 0.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.0f, 1.5f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // small triangles
        // size=0.005
        {Vec3f(0.0f, 0.4f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.41f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.01f, 0.4f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.015
        {Vec3f(0.0f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.515f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.015f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.02
        {Vec3f(0.0f, 0.6f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.62f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.02f, 0.6f, 0.5f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // size=0.025
        {Vec3f(0.0f, 0.7f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.725f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.025f, 0.7f, 0.5f), Vec4f(1.0f, 0.5f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // dynamic triangles
        {Vec3f(-0.6f, -0.75f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, -0.65f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, -0.75f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.6f, -0.75f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, -0.65f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, -0.75f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.6f, -0.75f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, -0.65f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, -0.75f, 0.5f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(-0.6f, -0.75f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, -0.65f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.4f, -0.75f, 0.5f), Vec4f(0.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // Different depth triangles
        {Vec3f(0.0f, -0.8f, 0.97f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, -0.2f, 0.97f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, -0.8f, 0.97f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.2f, -0.8f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, -0.4f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, -0.8f, 0.20f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.2f, -0.8f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, -0.6f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, -0.8f, 0.30f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(0.2f, -0.8f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, -0.7f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, -0.8f, 0.10f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // Fails depth bounds test.
        {Vec3f(0.2f, -0.8f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.4f, -0.7f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.6f, -0.8f, 0.05f), Vec4f(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // Should be back face culled.
        {Vec3f(0.6f, -0.8f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.4f, -0.7f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.2f, -0.8f, 0.25f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        // depth bounds prep
        {Vec3f(0.6f, 0.3f, 0.3f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, 0.5f, 0.5f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.3f, 0.7f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // depth bounds clip
        {Vec3f(0.6f, 0.3f, 0.3f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.7f, 0.5f, 0.5f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, 0.3f, 0.7f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // D16 triangle
        {Vec3f(-0.7f, 0.5f, 0.33f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.6f, 0.3f, 0.33f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.8f, 0.3f, 0.33f), Vec4f(1.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        // 1000 draws of 1 triangle
        {Vec3f(-0.7f, 0.0f, 0.33f), Vec4f(0.5f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.8f, 0.2f, 0.33f), Vec4f(0.5f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.6f, 0.2f, 0.33f), Vec4f(0.5f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        // 1000 instances of 1 triangle
        {Vec3f(-0.7f, 0.6f, 0.33f), Vec4f(1.0f, 0.5f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.8f, 0.8f, 0.33f), Vec4f(1.0f, 0.5f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.6f, 0.8f, 0.33f), Vec4f(1.0f, 0.5f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(VBData);

    uint32_t rtvIndex = 1;    // Start at 1, backbuffer takes id 0
    uint32_t dsvIndex = 0;

    const DXGI_FORMAT renderSurfaceFormat = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    const DXGI_FORMAT renderViewFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    const DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    const DXGI_FORMAT depth16Format = DXGI_FORMAT_D16_UNORM;

    struct PassResources
    {
      std::string markerName;
      D3D_SHADER_MODEL shaderModel;

      ID3D12ResourcePtr mainRT;
      D3D12_CPU_DESCRIPTOR_HANDLE mainRTV;
      ID3D12ResourcePtr mainDS;
      D3D12_CPU_DESCRIPTOR_HANDLE mainDSV;
      ID3D12ResourcePtr main16DS;
      D3D12_CPU_DESCRIPTOR_HANDLE main16DSV;

      ID3D12ResourcePtr mipArrayRT;
      D3D12_CPU_DESCRIPTOR_HANDLE mipArraySubRTV;
      ID3D12ResourcePtr mipArrayDS;
      D3D12_CPU_DESCRIPTOR_HANDLE mipArraySubDSV;

      ID3D12ResourcePtr msaaRT;
      D3D12_CPU_DESCRIPTOR_HANDLE msaaRTV;
      ID3D12ResourcePtr msaaDS;
      D3D12_CPU_DESCRIPTOR_HANDLE msaaDSV;

      ID3D12ResourcePtr msaaMipArrayRT;
      D3D12_CPU_DESCRIPTOR_HANDLE msaaMipArraySubRTV;
      ID3D12ResourcePtr msaaMipArrayDS;
      D3D12_CPU_DESCRIPTOR_HANDLE msaaMipArraySubDSV;

      ID3D12RootSignaturePtr rootSig;
      ID3D12PipelineStatePtr depthWritePipe;
      ID3D12PipelineStatePtr dynamicScissorPipe;
      ID3D12PipelineStatePtr depthPipe;
      ID3D12PipelineStatePtr stencilWritePipe;
      ID3D12PipelineStatePtr backgroundPipe;
      ID3D12PipelineStatePtr noPsPipe;
      ID3D12PipelineStatePtr mainTestPipe;
      ID3D12PipelineStatePtr cullFrontPipe;
      ID3D12PipelineStatePtr depthBoundsPipe;
      ID3D12PipelineStatePtr whitePipe;
      ID3D12PipelineStatePtr msaaPipe;
      ID3D12PipelineStatePtr depth16Pipe;
    };

    struct DepthBoundsTestStream
    {
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type0 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
      UINT Padding0;
      ID3D12RootSignature *RootSignature;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type1 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1;
      D3D12_DEPTH_STENCIL_DESC1 DepthStencil;
      UINT Padding1;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type2 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
      D3D12_RASTERIZER_DESC Rasterizer;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type3 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
      D3D12_INPUT_LAYOUT_DESC InputLayout;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type4 =
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
      D3D12_RT_FORMAT_ARRAY RTVFormats;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type5 =
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
      DXGI_FORMAT DSVFormat;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type6 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
      UINT Padding6;
      D3D12_SHADER_BYTECODE VS;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type7 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
      UINT Padding7;
      D3D12_SHADER_BYTECODE PS;
    } depthBoundsTestStream;

    D3D12_STATIC_SAMPLER_DESC staticSamp = {};
    staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamp.AddressU = staticSamp.AddressV = staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    PassResources passes[3];
    passes[0].shaderModel = D3D_SHADER_MODEL_5_1;    // DXBC with optional bindless
    passes[1].shaderModel = D3D_SHADER_MODEL_6_0;    // DXIL with optional bindless
    passes[2].shaderModel = D3D_SHADER_MODEL_6_6;    // DXIL with direct heap access bindless
    passes[0].markerName = "Begin SM5.1";
    passes[1].markerName = "Begin SM6.0";
    passes[2].markerName = "Begin SM6.6";

    const std::string profileSuffix[3] = {"_5_1", "_6_0", "_6_6"};
    const std::wstring nameSuffix[3] = {L"_SM51", L"_SM60", L"_SM66"};

    bool supportSM66 = m_HighestShaderModel >= D3D_SHADER_MODEL_6_6;
    int numPasses = (supportSM66 && m_DXILSupport) ? 3 : (m_DXILSupport ? 2 : 1);
    for(int i = 0; i < numPasses; ++i)
    {
      PassResources &pass = passes[i];
      pass.mainRT = MakeTexture(renderSurfaceFormat, screenWidth, screenHeight).RTV();
      pass.mainRT->SetName((L"mainRT" + nameSuffix[i]).c_str());
      pass.mainRTV = MakeRTV(pass.mainRT).Format(renderViewFormat).CreateCPU(rtvIndex++);

      pass.mainDS = MakeTexture(depthFormat, screenWidth, screenHeight).DSV();
      pass.mainDS->SetName((L"mainDS" + nameSuffix[i]).c_str());
      pass.mainDSV = MakeDSV(pass.mainDS).Format(depthFormat).CreateCPU(dsvIndex++);

      pass.main16DS = MakeTexture(depth16Format, screenWidth, screenHeight).DSV();
      pass.main16DS->SetName((L"main16DS" + nameSuffix[i]).c_str());
      pass.main16DSV = MakeDSV(pass.main16DS).Format(depth16Format).CreateCPU(dsvIndex++);

      pass.mipArrayRT =
          MakeTexture(renderSurfaceFormat, screenWidth, screenHeight).RTV().Mips(4).Array(5);
      pass.mipArrayRT->SetName((L"mipArrayRT" + nameSuffix[i]).c_str());

      pass.mipArraySubRTV = MakeRTV(pass.mipArrayRT)
                                .Format(renderViewFormat)
                                .FirstMip(2)
                                .NumMips(1)
                                .FirstSlice(2)
                                .NumSlices(1)
                                .CreateCPU(rtvIndex++);

      pass.mipArrayDS = MakeTexture(depthFormat, screenWidth, screenHeight).DSV().Mips(4).Array(5);
      pass.mipArrayDS->SetName((L"mipArrayDS" + nameSuffix[i]).c_str());

      pass.mipArraySubDSV = MakeDSV(pass.mipArrayDS)
                                .Format(depthFormat)
                                .FirstMip(2)
                                .NumMips(1)
                                .FirstSlice(2)
                                .NumSlices(1)
                                .CreateCPU(dsvIndex++);

      pass.msaaRT = MakeTexture(renderSurfaceFormat, screenWidth, screenHeight).RTV().Multisampled(4);
      pass.msaaRT->SetName((L"msaaRT" + nameSuffix[i]).c_str());
      pass.msaaRTV = MakeRTV(pass.msaaRT).Format(renderViewFormat).CreateCPU(rtvIndex++);

      pass.msaaDS = MakeTexture(depthFormat, screenWidth, screenHeight).DSV().Multisampled(4);
      pass.msaaDS->SetName((L"msaaDS" + nameSuffix[i]).c_str());
      pass.msaaDSV = MakeDSV(pass.msaaDS).Format(depthFormat).CreateCPU(dsvIndex++);

      pass.msaaMipArrayRT = MakeTexture(renderSurfaceFormat, screenWidth, screenHeight)
                                .RTV()
                                .Mips(1)
                                .Array(4)
                                .Multisampled(4);
      pass.msaaMipArrayRT->SetName((L"msaaMipArrayRT" + nameSuffix[i]).c_str());
      pass.msaaMipArraySubRTV = MakeRTV(pass.msaaMipArrayRT)
                                    .Format(renderViewFormat)
                                    .FirstMip(0)
                                    .NumMips(1)
                                    .FirstSlice(2)
                                    .NumSlices(1)
                                    .CreateCPU(rtvIndex++);

      pass.msaaMipArrayDS =
          MakeTexture(depthFormat, screenWidth, screenHeight).DSV().Mips(1).Array(4).Multisampled(4);
      pass.msaaMipArrayDS->SetName((L"msaaMipArrayDS" + nameSuffix[i]).c_str());
      pass.msaaMipArraySubDSV = MakeDSV(pass.msaaMipArrayDS)
                                    .Format(depthFormat)
                                    .FirstMip(0)
                                    .NumMips(1)
                                    .FirstSlice(2)
                                    .NumSlices(1)
                                    .CreateCPU(dsvIndex++);

      ID3DBlobPtr vsBlob = Compile(common + vertex, "main", ("vs" + profileSuffix[i]).c_str());
      ID3DBlobPtr psBlob = Compile(common + pixel, "main", ("ps" + profileSuffix[i]).c_str());
      ID3DBlobPtr psMsaaBlob = Compile(common + mspixel, "main", ("ps" + profileSuffix[i]).c_str());

      pass.rootSig =
          MakeSig({}, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);
      // TODO: Different root sig setup for SM6.6

      D3D12PSOCreator baselinePSO = MakePSO()
                                        .RootSig(pass.rootSig)
                                        .InputLayout()
                                        .RTVs({DXGI_FORMAT_R8G8B8A8_UNORM_SRGB})
                                        .DSV(depthFormat)
                                        .VS(vsBlob)
                                        .PS(psBlob);

      baselinePSO.GraphicsDesc.RasterizerState.DepthClipEnable = TRUE;
      baselinePSO.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
      D3D12_DEPTH_STENCIL_DESC &depthState = baselinePSO.GraphicsDesc.DepthStencilState;
      depthState.DepthEnable = TRUE;
      depthState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
      depthState.StencilEnable = FALSE;
      depthState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      depthState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
      depthState.StencilReadMask = 0xff;
      depthState.StencilWriteMask = 0xff;
      depthState.BackFace = depthState.FrontFace;

      depthState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pass.depthWritePipe = baselinePSO;

      depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      depthState.DepthEnable = FALSE;
      pass.dynamicScissorPipe = baselinePSO;

      depthState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
      depthState.DepthEnable = TRUE;
      depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

      depthBoundsTestStream.RootSignature = baselinePSO.GraphicsDesc.pRootSignature;
      depthBoundsTestStream.DepthStencil.DepthEnable = depthState.DepthEnable;
      depthBoundsTestStream.DepthStencil.DepthWriteMask = depthState.DepthWriteMask;
      depthBoundsTestStream.DepthStencil.DepthFunc = depthState.DepthFunc;
      depthBoundsTestStream.DepthStencil.StencilEnable = depthState.StencilEnable;
      depthBoundsTestStream.DepthStencil.StencilReadMask = depthState.StencilReadMask;
      depthBoundsTestStream.DepthStencil.StencilWriteMask = depthState.StencilWriteMask;
      depthBoundsTestStream.DepthStencil.FrontFace = depthState.FrontFace;
      depthBoundsTestStream.DepthStencil.BackFace = depthState.BackFace;
      depthBoundsTestStream.DepthStencil.DepthBoundsTestEnable = TRUE;

      depthBoundsTestStream.Rasterizer = baselinePSO.GraphicsDesc.RasterizerState;
      depthBoundsTestStream.InputLayout = baselinePSO.GraphicsDesc.InputLayout;
      depthBoundsTestStream.RTVFormats.NumRenderTargets = baselinePSO.GraphicsDesc.NumRenderTargets;
      for(int j = 0; j < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++j)
        depthBoundsTestStream.RTVFormats.RTFormats[j] = baselinePSO.GraphicsDesc.RTVFormats[j];
      depthBoundsTestStream.DSVFormat = baselinePSO.GraphicsDesc.DSVFormat;

      depthBoundsTestStream.VS.BytecodeLength = vsBlob->GetBufferSize();
      depthBoundsTestStream.VS.pShaderBytecode = vsBlob->GetBufferPointer();
      depthBoundsTestStream.PS.BytecodeLength = psBlob->GetBufferSize();
      depthBoundsTestStream.PS.pShaderBytecode = psBlob->GetBufferPointer();

      D3D12_PIPELINE_STATE_STREAM_DESC depthBoundsTestStreamDesc = {};
      depthBoundsTestStreamDesc.SizeInBytes = sizeof(depthBoundsTestStream);
      depthBoundsTestStreamDesc.pPipelineStateSubobjectStream = &depthBoundsTestStream;
      HRESULT hr = dev2->CreatePipelineState(
          &depthBoundsTestStreamDesc, __uuidof(ID3D12PipelineState), (void **)&pass.depthPipe);
      TEST_ASSERT(hr == S_OK, "Pipe created");

      depthState.StencilEnable = TRUE;
      pass.stencilWritePipe = baselinePSO;

      depthState.StencilEnable = FALSE;
      pass.backgroundPipe = baselinePSO;

      depthState.StencilEnable = FALSE;
      pass.depth16Pipe = baselinePSO.DSV(DXGI_FORMAT_D16_UNORM);
      baselinePSO.DSV(depthFormat);

      depthState.StencilEnable = TRUE;
      pass.noPsPipe = baselinePSO.PS(NULL);

      depthState.StencilEnable = TRUE;
      depthState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER;
      pass.mainTestPipe = baselinePSO.PS(psBlob);

      depthState.StencilEnable = FALSE;

      baselinePSO.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
      pass.cullFrontPipe = baselinePSO;

      baselinePSO.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

      depthBoundsTestStream.RootSignature = baselinePSO.GraphicsDesc.pRootSignature;
      depthBoundsTestStream.DepthStencil.DepthEnable = TRUE;
      depthBoundsTestStream.DepthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
      depthBoundsTestStream.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
      depthBoundsTestStream.DepthStencil.StencilEnable = depthState.StencilEnable;
      depthBoundsTestStream.DepthStencil.StencilReadMask = depthState.StencilReadMask;
      depthBoundsTestStream.DepthStencil.StencilWriteMask = depthState.StencilWriteMask;
      depthBoundsTestStream.DepthStencil.FrontFace = depthState.FrontFace;
      depthBoundsTestStream.DepthStencil.BackFace = depthState.BackFace;
      depthBoundsTestStream.DepthStencil.DepthBoundsTestEnable = TRUE;
      depthBoundsTestStream.Rasterizer = baselinePSO.GraphicsDesc.RasterizerState;
      depthBoundsTestStream.InputLayout = baselinePSO.GraphicsDesc.InputLayout;
      depthBoundsTestStream.RTVFormats.NumRenderTargets = baselinePSO.GraphicsDesc.NumRenderTargets;
      for(int j = 0; j < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++j)
        depthBoundsTestStream.RTVFormats.RTFormats[j] = baselinePSO.GraphicsDesc.RTVFormats[j];
      depthBoundsTestStream.DSVFormat = baselinePSO.GraphicsDesc.DSVFormat;

      // Depth bounds values are set on the command list before draw
      depthBoundsTestStream.VS.BytecodeLength = vsBlob->GetBufferSize();
      depthBoundsTestStream.VS.pShaderBytecode = vsBlob->GetBufferPointer();
      depthBoundsTestStream.PS.BytecodeLength = psBlob->GetBufferSize();
      depthBoundsTestStream.PS.pShaderBytecode = psBlob->GetBufferPointer();
      hr = dev2->CreatePipelineState(&depthBoundsTestStreamDesc, __uuidof(ID3D12PipelineState),
                                     (void **)&pass.depthBoundsPipe);
      TEST_ASSERT(hr == S_OK, "Pipe created");

      depthState.StencilEnable = FALSE;
      depthState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      pass.whitePipe = baselinePSO.DSV(DXGI_FORMAT_UNKNOWN);

      depthState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
      pass.msaaPipe = baselinePSO.PS(psMsaaBlob).DSV(depthFormat).SampleCount(4);
    }

    // TODO: Additional testing:
    //    CS UAV usage that doesn't write, does direct write, does atomic write
    //    Bindless access of target resource as UAV
    //    SM6.6 bindless access of target resource as UAV
    //    Pixel history of RT with depth/stencil changing formats throughout history
    //    Pixel history of DSV with render target changing formats throughout history
    //    Several RTs bound, tracking history for pixel in various RT slots
    //    RTs swapping position in RT array, tracking history of a color pixel
    //    RT/DSV with mips/slices, getting history of pixel in non-0 mip/slice

    while(Running())
    {
      for(int i = 0; i < numPasses; ++i)
      {
        PassResources &pass = passes[i];

        ID3D12GraphicsCommandList1Ptr cmd = GetCommandBuffer();
        Reset(cmd);
        pushMarker(cmd, pass.markerName);

        cmd->OMSetStencilRef(0x55);

        D3D12_VIEWPORT v;
        v.TopLeftX = 10;
        v.TopLeftY = 10;
        v.MinDepth = 0;
        v.MaxDepth = 1;
        v.Width = screenWidth - 20.0f;
        v.Height = screenHeight - 20.0f;
        RSSetViewport(cmd, v);

        D3D12_RECT scissor;
        scissor.left = scissor.top = 0;
        scissor.right = screenWidth;
        scissor.bottom = screenHeight;
        RSSetScissorRect(cmd, scissor);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        float clearColor[4] = {0.2f, 0.2f, 0.2f, 1.0f};
        cmd->OMSetRenderTargets(1, &pass.mainRTV, FALSE, &pass.mainDSV);
        cmd->ClearRenderTargetView(pass.mainRTV, clearColor, 0, NULL);
        cmd->ClearDepthStencilView(pass.mainDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                   1.0f, 0, 0, NULL);

        cmd->SetGraphicsRootSignature(pass.rootSig);

        // Draw the setup triangles

        setMarker(cmd, "Depth Write");
        cmd->SetPipelineState(pass.depthWritePipe);
        cmd->DrawInstanced(3, 1, 0, 0);

        setMarker(cmd, "Unbound Fragment Shader");
        cmd->OMSetStencilRef(0x33);
        cmd->SetPipelineState(pass.noPsPipe);
        cmd->DrawInstanced(3, 1, 3, 0);
        cmd->OMSetStencilRef(0x55);

        setMarker(cmd, "Stencil Write");
        cmd->SetPipelineState(pass.stencilWritePipe);
        cmd->DrawInstanced(3, 1, 3, 0);

        setMarker(cmd, "Background");
        cmd->SetPipelineState(pass.backgroundPipe);
        cmd->DrawInstanced(3, 1, 6, 0);

        setMarker(cmd, "Cull Front");
        cmd->SetPipelineState(pass.cullFrontPipe);
        cmd->DrawInstanced(3, 1, 0, 0);

        setMarker(cmd, "Depth Bounds Prep");
        cmd->SetPipelineState(pass.depthBoundsPipe);
        cmd->OMSetDepthBounds(0.0f, 1.0f);
        cmd->DrawInstanced(3, 1, 63, 0);
        setMarker(cmd, "Depth Bounds Clip");
        cmd->OMSetDepthBounds(0.4f, 0.6f);
        cmd->DrawInstanced(3, 1, 66, 0);

        pushMarker(cmd, "Stress Test");
        pushMarker(cmd, "Lots of Drawcalls");
        setMarker(cmd, "1000 Draws");
        cmd->SetPipelineState(pass.depthWritePipe);
        for(int d = 0; d < 1000; ++d)
          cmd->DrawInstanced(3, 1, 72, 0);
        popMarker(cmd);
        setMarker(cmd, "1000 Instances");
        cmd->DrawInstanced(3, 1000, 75, 0);
        popMarker(cmd);

        // Add a marker so we can easily locate this draw
        setMarker(cmd, "Test Begin");

        cmd->SetPipelineState(pass.mainTestPipe);
        cmd->DrawInstanced(24, 1, 9, 0);

        setMarker(cmd, "Fixed Scissor Fail");
        cmd->SetPipelineState(pass.dynamicScissorPipe);
        D3D12_RECT testScissor;
        testScissor.left = 95;
        testScissor.top = 245;
        testScissor.right = 99;
        testScissor.bottom = 249;
        RSSetScissorRect(cmd, testScissor);
        cmd->DrawInstanced(3, 1, 33, 0);

        setMarker(cmd, "Fixed Scissor Pass");
        cmd->SetPipelineState(pass.dynamicScissorPipe);
        testScissor.left = 95;
        testScissor.top = 245;
        testScissor.right = 105;
        testScissor.bottom = 255;
        RSSetScissorRect(cmd, testScissor);
        cmd->DrawInstanced(3, 1, 36, 0);

        setMarker(cmd, "Dynamic Stencil Ref");
        cmd->SetPipelineState(pass.dynamicScissorPipe);
        RSSetScissorRect(cmd, scissor);
        cmd->OMSetStencilRef(0x67);
        cmd->DrawInstanced(3, 1, 39, 0);

        setMarker(cmd, "Dynamic Stencil Mask");
        cmd->SetPipelineState(pass.dynamicScissorPipe);
        cmd->DrawInstanced(3, 1, 42, 0);

        // Six triangles, five fragments reported.
        // 0: Fails depth test
        // 1: Passes
        // 2: Fails depth test compared to 1st fragment
        // 3: Passes
        // 4: Fails depth bounds test
        // 5: Fails backface culling, not reported.
        setMarker(cmd, "Depth Test");
        cmd->SetPipelineState(pass.depthPipe);
        cmd->OMSetDepthBounds(0.15f, 1.0f);
        cmd->DrawInstanced(6 * 3, 1, 45, 0);

        cmd->OMSetRenderTargets(1, &pass.mainRTV, FALSE, &pass.main16DSV);
        setMarker(cmd, "Clear Depth 16-bit");
        cmd->ClearDepthStencilView(pass.main16DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
        setMarker(cmd, "Depth 16-bit Test");
        cmd->SetPipelineState(pass.depth16Pipe);
        cmd->DrawInstanced(3, 1, 69, 0);

        ResourceBarrier(cmd, pass.main16DS, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                        D3D12_RESOURCE_STATE_COMMON);
        {
          pushMarker(cmd, "Begin MSAA");

          ResourceBarrier(cmd, pass.msaaDS, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_DEPTH_WRITE);
          ResourceBarrier(cmd, pass.msaaRT, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_RENDER_TARGET);

          cmd->OMSetRenderTargets(1, &pass.msaaRTV, FALSE, &pass.msaaDSV);
          float clearColorMsaa[4] = {0.0f, 1.0f, 0.0f, 1.0f};
          cmd->ClearRenderTargetView(pass.msaaRTV, clearColorMsaa, 0, NULL);
          cmd->ClearDepthStencilView(
              pass.msaaDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, NULL);

          cmd->SetPipelineState(pass.msaaPipe);

          setMarker(cmd, "Multisampled: test");
          cmd->DrawInstanced(6, 1, 3, 0);

          ResourceBarrier(cmd, pass.msaaDS, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                          D3D12_RESOURCE_STATE_COMMON);
          ResourceBarrier(cmd, pass.msaaRT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                          D3D12_RESOURCE_STATE_COMMON);

          popMarker(cmd);
        }

        v.Width = screenWidth / 4.0f - 10;
        v.Height = screenHeight / 4.0f - 10;
        v.TopLeftX = 5.0f;
        v.TopLeftY = 5.0f;

        scissor.right = (scissor.right - scissor.left) / 4 + scissor.left;
        scissor.bottom = (scissor.bottom - scissor.top) / 4 + scissor.top;

        // Render to a secondary surface
        {
          pushMarker(cmd, "Begin RenderPass Secondary");

          ResourceBarrier(cmd, pass.mipArrayDS, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_DEPTH_WRITE);
          ResourceBarrier(cmd, pass.mipArrayRT, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_RENDER_TARGET);
          cmd->OMSetRenderTargets(1, &pass.mipArraySubRTV, FALSE, NULL);
          float clearColorSecondary[4] = {0.0f, 1.0f, 0.0f, 1.0f};
          cmd->ClearRenderTargetView(pass.mipArraySubRTV, clearColorSecondary, 0, NULL);
          cmd->OMSetStencilRef(0x55);

          IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
          cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
          cmd->SetPipelineState(pass.whitePipe);

          RSSetViewport(cmd, v);
          RSSetScissorRect(cmd, scissor);

          setMarker(cmd, "Secondary: background");
          cmd->DrawInstanced(6, 1, 3, 0);
          setMarker(cmd, "Secondary: culled");
          cmd->DrawInstanced(6, 1, 12, 0);
          setMarker(cmd, "Secondary: pink");
          cmd->DrawInstanced(9, 1, 24, 0);
          setMarker(cmd, "Secondary: red and blue");
          cmd->DrawInstanced(6, 1, 0, 0);

          ResourceBarrier(cmd, pass.mipArrayDS, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                          D3D12_RESOURCE_STATE_COMMON);
          ResourceBarrier(cmd, pass.mipArrayRT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                          D3D12_RESOURCE_STATE_COMMON);
          popMarker(cmd);
        }

        ResourceBarrier(cmd, pass.mainDS, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                        D3D12_RESOURCE_STATE_COMMON);
        ResourceBarrier(cmd, pass.mainRT, D3D12_RESOURCE_STATE_RENDER_TARGET,
                        D3D12_RESOURCE_STATE_COMMON);

        popMarker(cmd);
        cmd->Close();
        {
          ID3D12GraphicsCommandList1Ptr barrierCmd = GetCommandBuffer();
          Reset(barrierCmd);
          ResourceBarrier(barrierCmd, pass.main16DS, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_DEPTH_WRITE);
          ResourceBarrier(barrierCmd, pass.mainDS, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_DEPTH_WRITE);
          ResourceBarrier(barrierCmd, pass.mainRT, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_RENDER_TARGET);
          barrierCmd->Close();
          Submit({barrierCmd});
        }

        Submit({cmd});
      }

      ID3D12GraphicsCommandList1Ptr cmd = GetCommandBuffer();
      Reset(cmd);
      // Now blit the main render targets to the back buffer
      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);
      for(int i = 0; i < numPasses; ++i)
      {
        ResourceBarrier(cmd, passes[i].mainRT, D3D12_RESOURCE_STATE_COMMON,
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        blitToSwap(cmd, passes[i].mainRT, bb, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

        ResourceBarrier(cmd, passes[i].mainRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_COMMON);
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
