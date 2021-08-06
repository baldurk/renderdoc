/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

  std::string whitePixel = R"EOSHADER(

float4 main() : SV_Target0
{
	return float4(1, 1, 1, 1);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");
    ID3DBlobPtr whitepsblob = Compile(whitePixel, "main", "ps_4_0");

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

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualData = {};
    qualData.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    qualData.SampleCount = 4;
    dev->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualData, sizeof(qualData));

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualData2 = {};
    qualData2.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    qualData2.SampleCount = 4;
    dev->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualData2, sizeof(qualData2));

    UINT qual = std::min(qualData.NumQualityLevels, qualData2.NumQualityLevels) > 1 ? 1 : 0;

    DXGI_SAMPLE_DESC noMSAA = {1, 0};
    DXGI_SAMPLE_DESC yesMSAA = {4, qual};

    D3D12PSOCreator creator = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).DSV(
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

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
    ID3D12PipelineStatePtr depthWritePipe[2];
    creator.GraphicsDesc.SampleDesc = noMSAA;
    depthWritePipe[0] = creator;
    creator.GraphicsDesc.SampleDesc = yesMSAA;
    depthWritePipe[1] = creator;

    creator.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    creator.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;

    ID3D12PipelineStatePtr stencilWritePipe[2];
    creator.GraphicsDesc.SampleDesc = noMSAA;
    stencilWritePipe[0] = creator;
    creator.GraphicsDesc.SampleDesc = yesMSAA;
    stencilWritePipe[1] = creator;

    creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
    ID3D12PipelineStatePtr backgroundPipe[2];
    creator.GraphicsDesc.SampleDesc = noMSAA;
    backgroundPipe[0] = creator;
    creator.GraphicsDesc.SampleDesc = yesMSAA;
    backgroundPipe[1] = creator;

    creator.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;
    creator.GraphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER;
    ID3D12PipelineStatePtr pipe[2];
    creator.GraphicsDesc.SampleDesc = noMSAA;
    pipe[0] = creator;
    creator.GraphicsDesc.SampleDesc = yesMSAA;
    pipe[1] = creator;

    creator.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
    creator.GraphicsDesc.DepthStencilState.DepthEnable = FALSE;
    creator.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    creator.PS(whitepsblob);
    creator.DSV(DXGI_FORMAT_UNKNOWN);
    creator.GraphicsDesc.SampleDesc = noMSAA;
    ID3D12PipelineStatePtr whitepipe = creator;

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr dsv = MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight)
                                .DSV()
                                .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    ID3D12ResourcePtr subtex = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight)
                                   .RTV()
                                   .Array(5)
                                   .Mips(4)
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12ResourcePtr msaadsv =
        MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, screenWidth, screenHeight)
            .DSV()
            .Multisampled(4, qual)
            .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

    ID3D12ResourcePtr msaatex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth, screenHeight)
            .RTV()
            .Multisampled(4, qual)
            .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      for(bool is_msaa : {false, true})
      {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            MakeRTV(is_msaa ? msaatex : bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->SetGraphicsRootSignature(sig);

        RSSetViewport(cmd, {10.0f, 10.0f, (float)screenWidth - 20.0f, (float)screenHeight - 20.0f,
                            0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, MakeDSV(is_msaa ? msaadsv : dsv).CreateCPU(0));

        ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});
        ClearDepthStencilView(cmd, is_msaa ? msaadsv : dsv,
                              D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0);

        cmd->OMSetStencilRef(0x55);

        // draw the setup triangles
        cmd->SetPipelineState(depthWritePipe[is_msaa ? 1 : 0]);
        cmd->DrawInstanced(3, 1, 0, 0);

        cmd->SetPipelineState(stencilWritePipe[is_msaa ? 1 : 0]);
        cmd->DrawInstanced(3, 1, 3, 0);

        cmd->SetPipelineState(backgroundPipe[is_msaa ? 1 : 0]);
        cmd->DrawInstanced(3, 1, 6, 0);

        // add a marker so we can easily locate this draw
        setMarker(cmd, is_msaa ? "MSAA Test" : "Normal Test");

        cmd->SetPipelineState(pipe[is_msaa ? 1 : 0]);
        cmd->DrawInstanced(24, 1, 9, 0);

        if(!is_msaa)
        {
          setMarker(cmd, "Viewport Test");

          RSSetViewport(cmd, {10.0f, 10.0f, 80.0f, 80.0f, 0.0f, 1.0f});
          RSSetScissorRect(cmd, {24, 24, 76, 76});
          cmd->SetPipelineState(backgroundPipe[0]);
          cmd->DrawInstanced(3, 1, 33, 0);
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

      cmd->SetPipelineState(whitepipe);

      subrtv = MakeRTV(subtex)
                   .Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                   .FirstSlice(2)
                   .NumSlices(1)
                   .FirstMip(3)
                   .NumMips(1)
                   .CreateCPU(1);

      setMarker(cmd, "Subresources mip 2");
      cmd->DrawInstanced(24, 1, 9, 0);

      RSSetViewport(cmd, {2.0f, 2.0f, float(screenWidth / 8) - 4.0f, float(screenHeight / 8) - 4.0f,
                          0.0f, 1.0f});
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

        cmd->SetPipelineState(whitepipe);

        setMarker(cmd, "NoView draw");

        cmd->DrawInstanced(3, 1, 33, 0);

        FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmd->Close();
      }

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
