/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

RD_TEST(D3D12_Overlay_Test, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Makes a couple of draws that show off all the overlays in some way";

  std::string vertexEndPosVert = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct v2f
{
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
	float4 pos : SV_POSITION;
};

v2f main(vertin IN)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.xyz, 1);
	OUT.col = IN.col;
	OUT.uv = IN.uv;

	return OUT;
}

)EOSHADER";

  std::string vertexEndPosPixel = R"EOSHADER(

struct v2f
{
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
	float4 pos : SV_POSITION;
};

float4 main(v2f IN) : SV_Target0
{
	return IN.col;
}

)EOSHADER";

  std::string whitePixel = R"EOSHADER(

float4 main() : SV_Target0
{
	return float4(1, 1, 1, 1);
}

)EOSHADER";

  std::string depthWritePixel = R"EOSHADER(

struct v2f
{
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
	float4 pos : SV_POSITION;
};

struct PixOut
{
	float4 colour : SV_Target0;
	float depth : SV_Depth;
};

PixOut main(v2f IN)
{
  PixOut OUT;
	OUT.colour  = IN.col;
  if ((IN.pos.x > 180.0) && (IN.pos.x < 185.0) &&
      (IN.pos.y > 155.0) && (IN.pos.y < 165.0))
	{
		OUT.depth = 0.0;
	}
	else
	{
		OUT.depth = IN.pos.z;
	}
  return OUT;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob[3] = {};
    ID3DBlobPtr psblob[3] = {};
    ID3DBlobPtr whitepsblob[3] = {};
    ID3DBlobPtr depthwritepsblob[3] = {};

    {
      int i = 0;
      for(std::string profile : {"_5_0", "_5_1", "_6_0"})
      {
        if(i == 2 && !m_DXILSupport)
          continue;

        vsblob[i] = Compile(vertexEndPosVert, "main", "vs" + profile);
        psblob[i] = Compile(vertexEndPosPixel, "main", "ps" + profile);
        whitepsblob[i] = Compile(whitePixel, "main", "ps" + profile);
        depthwritepsblob[i] = Compile(depthWritePixel, "main", "ps" + profile);
        i++;
      }
    }

    const DefaultA2V VBData[] = {
        // this triangle occludes in depth
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle occludes in stencil
        {Vec3f(-0.5f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.5f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.0f, 0.9f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // this triangle is just in the background to contribute to overdraw
        {Vec3f(-0.9f, -0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.9f, -0.9f, 0.95f), Vec4f(0.1f, 0.1f, 0.1f, 1.0f), Vec2f(1.0f, 0.0f)},

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

        // this triangle deliberately goes out of the viewport, it will test viewport & scissor
        // clipping
        {Vec3f(-1.3f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(1.3f, -1.3f, 0.95f), Vec4f(0.1f, 0.1f, 0.5f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(VBData);

    ID3D12RootSignaturePtr sig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_VERTEX, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 5, 0),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 5, 0),
        tableParam(D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 5, 0),
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 5, 0),
    });

    char *fmtNames[] = {"D24_S8", "D32F_S8", "D16_S0", "D32F_S0"};
    DXGI_FORMAT fmts[] = {DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                          DXGI_FORMAT_D16_UNORM, DXGI_FORMAT_D32_FLOAT};
    const size_t countFmts = ARRAY_COUNT(fmts);

    DXGI_SAMPLE_DESC noMSAA = {1, 0};

    ID3D12PipelineStatePtr depthWritePipe[3][countFmts][2];
    ID3D12PipelineStatePtr stencilWritePipe[3][countFmts][2];
    ID3D12PipelineStatePtr backgroundPipe[3][countFmts][2];
    ID3D12PipelineStatePtr pipe[3][countFmts][2];
    ID3D12PipelineStatePtr depthWritePixelShaderPipe[3][countFmts][2];
    ID3D12PipelineStatePtr whitepipe[3];
    ID3D12PipelineStatePtr sampleMaskPipe[3][countFmts];

    for(int i = 0; i < 3; i++)
    {
      if(vsblob[i] == NULL)
        continue;

      for(size_t f = 0; f < countFmts; f++)
      {
        DXGI_FORMAT fmt = fmts[f];

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualData = {};
        qualData.Format = fmt;
        qualData.SampleCount = 4;
        dev->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualData,
                                 sizeof(qualData));

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualData2 = {};
        qualData2.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        qualData2.SampleCount = 4;
        dev->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualData2,
                                 sizeof(qualData2));

        UINT qual = std::min(qualData.NumQualityLevels, qualData2.NumQualityLevels) > 1 ? 1 : 0;

        DXGI_SAMPLE_DESC yesMSAA = {4, qual};

        D3D12PSOCreator creator =
            MakePSO().RootSig(sig).InputLayout().VS(vsblob[i]).PS(psblob[i]).DSV(fmt);

        creator.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        creator.GraphicsDesc.RasterizerState.DepthClipEnable = TRUE;

        creator.GraphicsDesc.DepthStencilState.DepthEnable = TRUE;
        creator.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
        creator.GraphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        creator.GraphicsDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
        creator.GraphicsDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        creator.GraphicsDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        creator.GraphicsDesc.DepthStencilState.BackFace =
            creator.GraphicsDesc.DepthStencilState.FrontFace;
        creator.GraphicsDesc.DepthStencilState.StencilReadMask = 0xff;
        creator.GraphicsDesc.DepthStencilState.StencilWriteMask = 0xff;

        creator.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        creator.GraphicsDesc.SampleDesc = noMSAA;
        depthWritePipe[i][f][0] = creator;
        creator.GraphicsDesc.SampleDesc = yesMSAA;
        depthWritePipe[i][f][1] = creator;

        creator.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        creator.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;

        creator.GraphicsDesc.SampleDesc = noMSAA;
        stencilWritePipe[i][f][0] = creator;
        creator.GraphicsDesc.SampleDesc = yesMSAA;
        stencilWritePipe[i][f][1] = creator;

        creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
        creator.GraphicsDesc.SampleDesc = noMSAA;
        backgroundPipe[i][f][0] = creator;
        creator.GraphicsDesc.SampleDesc = yesMSAA;
        backgroundPipe[i][f][1] = creator;

        creator.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;
        creator.GraphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER;
        creator.GraphicsDesc.SampleDesc = noMSAA;
        pipe[i][f][0] = creator;
        creator.GraphicsDesc.SampleDesc = yesMSAA;
        pipe[i][f][1] = creator;

        creator.PS(depthwritepsblob[i]);
        creator.GraphicsDesc.SampleDesc = noMSAA;
        depthWritePixelShaderPipe[i][f][0] = creator;
        creator.GraphicsDesc.SampleDesc = yesMSAA;
        depthWritePixelShaderPipe[i][f][1] = creator;

        creator.PS(psblob[i]);
        creator.GraphicsDesc.SampleDesc = yesMSAA;
        sampleMaskPipe[i][f] = creator;
      }

      D3D12PSOCreator creator =
          MakePSO().RootSig(sig).InputLayout().VS(vsblob[i]).PS(psblob[i]).DSV(fmts[0]);
      creator.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
      creator.GraphicsDesc.SampleMask = 0xFFFFFFFF;
      creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
      creator.GraphicsDesc.DepthStencilState.DepthEnable = FALSE;
      creator.PS(whitepsblob[i]);
      creator.DSV(DXGI_FORMAT_UNKNOWN);
      creator.GraphicsDesc.SampleDesc = noMSAA;
      whitepipe[i] = creator;
    }

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr subtex = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight)
                                   .RTV()
                                   .Array(5)
                                   .Mips(4)
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12ResourcePtr msaatexs[countFmts];

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualData2 = {};
    qualData2.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    qualData2.SampleCount = 4;
    dev->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualData2, sizeof(qualData2));

    ID3D12ResourcePtr dsvs[countFmts];
    ID3D12ResourcePtr msaadsvs[countFmts];
    for(size_t f = 0; f < countFmts; f++)
    {
      DXGI_FORMAT fmt = fmts[f];

      D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualData = {};
      qualData.Format = fmt;
      qualData.SampleCount = 4;
      dev->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualData, sizeof(qualData));

      UINT qual = std::min(qualData.NumQualityLevels, qualData2.NumQualityLevels) > 1 ? 1 : 0;

      ID3D12ResourcePtr msaatex =
          MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight)
              .RTV()
              .Multisampled(4, qual)
              .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
      msaatexs[f] = msaatex;

      ID3D12ResourcePtr dsv =
          MakeTexture(fmt, screenWidth, screenHeight).DSV().InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
      dsvs[f] = dsv;

      ID3D12ResourcePtr msaadsv = MakeTexture(fmt, screenWidth, screenHeight)
                                      .DSV()
                                      .Multisampled(4, qual)
                                      .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
      msaadsvs[f] = msaadsv;
    }

    while(Running())
    {
      ID3D12ResourcePtr bb;

      int pass = 0;
      for(std::string profile : {"sm5.0", "sm5.1", "sm6.0"})
      {
        if(whitepipe[pass] == NULL)
          break;

        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        if(pass == 0)
          bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        pushMarker(cmd, profile + " tests");

        for(size_t f = 0; f < countFmts; f++)
        {
          std::string fmtName(fmtNames[f]);
          DXGI_FORMAT fmt = fmts[f];
          bool hasStencil = false;
          if(fmt == DXGI_FORMAT_D24_UNORM_S8_UINT || fmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
            hasStencil = true;
          for(bool is_msaa : {false, true})
          {
            D3D12_CPU_DESCRIPTOR_HANDLE rtv =
                MakeRTV(is_msaa ? msaatexs[f] : bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

            cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
            cmd->SetGraphicsRootSignature(sig);

            RSSetViewport(cmd, {10.0f, 10.0f, (float)screenWidth - 20.0f,
                                (float)screenHeight - 20.0f, 0.0f, 1.0f});
            RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

            OMSetRenderTargets(cmd, {rtv}, MakeDSV(is_msaa ? msaadsvs[f] : dsvs[f]).CreateCPU(0));

            ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});
            ClearDepthStencilView(cmd, is_msaa ? msaadsvs[f] : dsvs[f],
                                  D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0);

            D3D12_RECT stencilClearRect;
            stencilClearRect.left = 32;
            stencilClearRect.right = 38;
            stencilClearRect.top = 32;
            stencilClearRect.bottom = 38;
            cmd->ClearDepthStencilView(m_DSV->GetCPUDescriptorHandleForHeapStart(),
                                       D3D12_CLEAR_FLAG_STENCIL, 0.0f, 1, 1, &stencilClearRect);

            cmd->OMSetStencilRef(0x55);

            // draw the setup triangles
            cmd->SetPipelineState(depthWritePipe[pass][f][is_msaa ? 1 : 0]);
            cmd->DrawInstanced(3, 1, 0, 0);

            if(hasStencil)
            {
              cmd->SetPipelineState(stencilWritePipe[pass][f][is_msaa ? 1 : 0]);
              cmd->DrawInstanced(3, 1, 3, 0);
            }

            cmd->SetPipelineState(backgroundPipe[pass][f][is_msaa ? 1 : 0]);
            cmd->DrawInstanced(3, 1, 6, 0);

            // add a marker so we can easily locate this draw
            std::string markerName(is_msaa ? "MSAA Test " : "Normal Test ");
            markerName += fmtName;
            setMarker(cmd, markerName);

            cmd->SetPipelineState(depthWritePixelShaderPipe[pass][f][is_msaa ? 1 : 0]);
            cmd->DrawInstanced(24, 1, 9, 0);
            cmd->SetPipelineState(pipe[pass][f][is_msaa ? 1 : 0]);

            if(!is_msaa)
            {
              setMarker(cmd, "Viewport Test " + fmtName);

              RSSetViewport(cmd, {10.0f, 10.0f, 80.0f, 80.0f, 0.0f, 1.0f});
              RSSetScissorRect(cmd, {24, 24, 76, 76});
              cmd->SetPipelineState(backgroundPipe[pass][f][0]);
              cmd->DrawInstanced(3, 1, 33, 0);
            }

            if(is_msaa)
            {
              setMarker(cmd, "Sample Mask Test " + fmtName);

              RSSetViewport(cmd, {0.0f, 0.0f, 80.0f, 80.0f, 0.0f, 1.0f});
              RSSetScissorRect(cmd, {0, 0, 80, 80});
              cmd->SetPipelineState(sampleMaskPipe[pass][f]);
              cmd->DrawInstanced(3, 1, 6, 0);
            }
          }
        }

        D3D12_CPU_DESCRIPTOR_HANDLE subrtv = MakeRTV(subtex)
                                                 .Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                                                 .FirstSlice(2)
                                                 .NumSlices(1)
                                                 .FirstMip(2)
                                                 .NumMips(1)
                                                 .CreateCPU(1);

        RSSetViewport(cmd, {5.0f, 5.0f, float(screenWidth) / 4.0f - 10.0f,
                            float(screenHeight) / 4.0f - 10.0f, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth / 4, screenHeight / 4});

        OMSetRenderTargets(cmd, {subrtv}, {});

        ClearRenderTargetView(cmd, subrtv, {0.0f, 0.0f, 0.0f, 1.0f});

        cmd->SetPipelineState(whitepipe[pass]);

        subrtv = MakeRTV(subtex)
                     .Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                     .FirstSlice(2)
                     .NumSlices(1)
                     .FirstMip(3)
                     .NumMips(1)
                     .CreateCPU(1);

        setMarker(cmd, "Subresources mip 2");
        cmd->DrawInstanced(24, 1, 9, 0);

        RSSetViewport(cmd, {2.0f, 2.0f, float(screenWidth / 8) - 4.0f,
                            float(screenHeight / 8) - 4.0f, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth / 8, screenHeight / 8});

        OMSetRenderTargets(cmd, {subrtv}, {});

        ClearRenderTargetView(cmd, subrtv, {0.0f, 0.0f, 0.0f, 1.0f});

        setMarker(cmd, "Subresources mip 3");
        cmd->DrawInstanced(24, 1, 9, 0);

        cmd->Close();

        Submit({cmd});

        {
          cmd = GetCommandBuffer();

          Reset(cmd);

          D3D12_CPU_DESCRIPTOR_HANDLE rtv =
              MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

          cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

          IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
          cmd->SetGraphicsRootSignature(sig);

          OMSetRenderTargets(cmd, {rtv}, {});

          cmd->SetPipelineState(whitepipe[0]);

          setMarker(cmd, "NoView draw");

          cmd->DrawInstanced(3, 1, 33, 0);

          popMarker(cmd);

          cmd->Close();
        }

        Submit({cmd});

        pass++;
      }

      {
        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmd->Close();

        Submit({cmd});
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
