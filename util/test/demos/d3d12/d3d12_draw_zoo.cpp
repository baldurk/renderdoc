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

#include "d3d12_test.h"

RD_TEST(D3D12_Draw_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Draws several variants using different vertex/index offsets.";

  std::string common = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR;
	float4 uv : TEXCOORD;

  float vertidx : VID;
  float instidx : IID;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

struct DefaultA2V
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f main(DefaultA2V IN, uint vid : SV_VertexID, uint instid : SV_InstanceID)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.xyz, 1);
  OUT.pos.x += IN.col.w;
	OUT.col = IN.col;
	OUT.uv = float4(IN.uv, 0, 1);

  OUT.vertidx = float(vid);
  OUT.instidx = float(instid);

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

float4 main(v2f IN) : SV_Target0
{
	return float4(IN.vertidx, IN.instidx, IN.col.w, IN.col.g + IN.uv.x);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(common + pixel, "main", "ps_5_0");

    D3D12_INPUT_CLASSIFICATION perVertex = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    std::vector<D3D12_INPUT_ELEMENT_DESC> layoutdesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, perVertex, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, perVertex, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, perVertex, 0},
    };

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO()
                                     .RootSig(sig)
                                     .InputLayout(layoutdesc)
                                     .StripRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF)
                                     .VS(vsblob)
                                     .PS(psblob)
                                     .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});

    layoutdesc[1].AlignedByteOffset = 0;
    layoutdesc[1].InputSlot = 1;
    layoutdesc[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
    layoutdesc[1].InstanceDataStepRate = 1;

    ID3D12PipelineStatePtr instpso = MakePSO()
                                         .RootSig(sig)
                                         .InputLayout(layoutdesc)
                                         .VS(vsblob)
                                         .PS(psblob)
                                         .RTVs({DXGI_FORMAT_R32G32B32A32_FLOAT});

    DefaultA2V triangle[] = {
        // 0
        {Vec3f(-1.0f, -1.0f, -1.0f), Vec4f(1.0f, 1.0f, 1.0f, 0.0f), Vec2f(-1.0f, -1.0f)},
        // 1, 2, 3
        {Vec3f(-0.5f, 0.5f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // 4, 5, 6
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // 7, 8, 9
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // 10, 11, 12
        {Vec3f(0.0f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        // strips: 13, 14, 15, ...
        {Vec3f(-0.5f, 0.2f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 0.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-0.5f, 0.0f, 0.0f), Vec4f(0.2f, 0.1f, 0.0f, 0.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.3f, 0.2f, 0.0f), Vec4f(0.4f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.3f, 0.0f, 0.0f), Vec4f(0.6f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.2f, 0.0f), Vec4f(0.8f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(-0.1f, 0.0f, 0.0f), Vec4f(1.0f, 0.5f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.1f, 0.2f, 0.0f), Vec4f(0.0f, 0.8f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.1f, 0.0f, 0.0f), Vec4f(0.2f, 0.1f, 0.5f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.3f, 0.2f, 0.0f), Vec4f(0.4f, 0.3f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.3f, 0.0f, 0.0f), Vec4f(0.6f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, 0.2f, 0.0f), Vec4f(0.8f, 0.3f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, 0.0f, 0.0f), Vec4f(1.0f, 0.1f, 1.0f, 0.0f), Vec2f(1.0f, 0.0f)},
    };

    std::vector<DefaultA2V> vbData;
    vbData.resize(600);

    {
      DefaultA2V *src = (DefaultA2V *)triangle;
      DefaultA2V *dst = (DefaultA2V *)&vbData[0];

      // up-pointing src to offset 0
      memcpy(dst + 0, src + 1, sizeof(DefaultA2V));
      memcpy(dst + 1, src + 2, sizeof(DefaultA2V));
      memcpy(dst + 2, src + 3, sizeof(DefaultA2V));

      // invalid vert for index 3 and 4
      memcpy(dst + 3, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 4, src + 0, sizeof(DefaultA2V));

      // down-pointing src at offset 5
      memcpy(dst + 5, src + 4, sizeof(DefaultA2V));
      memcpy(dst + 6, src + 5, sizeof(DefaultA2V));
      memcpy(dst + 7, src + 6, sizeof(DefaultA2V));

      // invalid vert for 8 - 12
      memcpy(dst + 8, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 9, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 10, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 11, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 12, src + 0, sizeof(DefaultA2V));

      // left-pointing src data to offset 13
      memcpy(dst + 13, src + 7, sizeof(DefaultA2V));
      memcpy(dst + 14, src + 8, sizeof(DefaultA2V));
      memcpy(dst + 15, src + 9, sizeof(DefaultA2V));

      // invalid vert for 16-22
      memcpy(dst + 16, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 17, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 18, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 19, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 20, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 21, src + 0, sizeof(DefaultA2V));
      memcpy(dst + 22, src + 0, sizeof(DefaultA2V));

      // right-pointing src data to offset 23
      memcpy(dst + 23, src + 10, sizeof(DefaultA2V));
      memcpy(dst + 24, src + 11, sizeof(DefaultA2V));
      memcpy(dst + 25, src + 12, sizeof(DefaultA2V));

      // strip after 30
      memcpy(dst + 30, src + 13, sizeof(DefaultA2V));
      memcpy(dst + 31, src + 14, sizeof(DefaultA2V));
      memcpy(dst + 32, src + 15, sizeof(DefaultA2V));
      memcpy(dst + 33, src + 16, sizeof(DefaultA2V));
      memcpy(dst + 34, src + 17, sizeof(DefaultA2V));
      memcpy(dst + 35, src + 18, sizeof(DefaultA2V));
      memcpy(dst + 36, src + 19, sizeof(DefaultA2V));
      memcpy(dst + 37, src + 20, sizeof(DefaultA2V));
      memcpy(dst + 38, src + 21, sizeof(DefaultA2V));
      memcpy(dst + 39, src + 22, sizeof(DefaultA2V));
      memcpy(dst + 40, src + 23, sizeof(DefaultA2V));
      memcpy(dst + 41, src + 24, sizeof(DefaultA2V));
    }

    for(size_t i = 0; i < vbData.size(); i++)
    {
      vbData[i].uv.x = float(i);
      vbData[i].col.y = float(i) / 200.0f;
    }

    vbData.resize(vbData.size() + 100);

    ID3D12ResourcePtr vb = MakeBuffer().Data(vbData).Size(UINT(vbData.size() * sizeof(DefaultA2V)));

    Vec4f instData[256] = {};
    for(int i = 0; i < ARRAY_COUNT(instData); i++)
      instData[i] = Vec4f(-100.0f, -100.0f, -100.0f, -100.0f);

    {
      instData[0] = Vec4f(0.0f, 0.4f, 1.0f, 0.0f);
      instData[1] = Vec4f(0.5f, 0.5f, 0.0f, 0.5f);

      instData[5] = Vec4f(0.0f, 0.6f, 0.5f, 0.0f);
      instData[6] = Vec4f(0.5f, 0.7f, 1.0f, 0.5f);

      instData[13] = Vec4f(0.0f, 0.8f, 0.3f, 0.0f);
      instData[14] = Vec4f(0.5f, 0.9f, 0.1f, 0.5f);
    }

    ID3D12ResourcePtr instvb = MakeBuffer().Data(instData).Size(4096);

    std::vector<uint16_t> idxData;
    idxData.resize(2048);

    {
      idxData[0] = 0;
      idxData[1] = 1;
      idxData[2] = 2;

      idxData[5] = 5;
      idxData[6] = 6;
      idxData[7] = 7;

      idxData[13] = 63;
      idxData[14] = 64;
      idxData[15] = 65;

      idxData[23] = 103;
      idxData[24] = 104;
      idxData[25] = 105;

      idxData[37] = 104;
      idxData[38] = 105;
      idxData[39] = 106;

      idxData[42] = 30;
      idxData[43] = 31;
      idxData[44] = 32;
      idxData[45] = 33;
      idxData[46] = 34;
      idxData[47] = 0xffff;
      idxData[48] = 36;
      idxData[49] = 37;
      idxData[50] = 38;
      idxData[51] = 39;
      idxData[52] = 40;
      idxData[53] = 41;

      idxData[54] = 130;
      idxData[55] = 131;
      idxData[56] = 132;
      idxData[57] = 133;
      idxData[58] = 134;
      idxData[59] = 0xffff;
      idxData[60] = 136;
      idxData[61] = 137;
      idxData[62] = 138;
      idxData[63] = 139;
      idxData[64] = 140;
      idxData[65] = 141;
    }

    ID3D12ResourcePtr ib = MakeBuffer().Data(idxData).Size(4096);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(instvb, D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(ib, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_INDEX_BUFFER);

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      D3D12_CPU_DESCRIPTOR_HANDLE offrtv = MakeRTV(rtvtex).CreateCPU(1);

      OMSetRenderTargets(cmd, {offrtv}, {});

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, bbrtv, {0.2f, 0.2f, 0.2f, 1.0f});

      ClearRenderTargetView(cmd, offrtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      cmd->SetGraphicsRootSignature(sig);

      D3D12_VIEWPORT view = {0.0f, 0.0f, 48.0f, 48.0f, 0.0f, 1.0f};

      RSSetViewport(cmd, view);
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      D3D12_VERTEX_BUFFER_VIEW vbs[2] = {};
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress();
      vbs[0].SizeInBytes = 4096;
      vbs[0].StrideInBytes = sizeof(DefaultA2V);
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress();
      vbs[1].StrideInBytes = sizeof(Vec4f);
      vbs[1].SizeInBytes = 4096;

      D3D12_INDEX_BUFFER_VIEW ibv = {};
      ibv.BufferLocation = ib->GetGPUVirtualAddress();
      ibv.Format = DXGI_FORMAT_R16_UINT;
      ibv.SizeInBytes = 1024;

      cmd->SetPipelineState(pso);

      setMarker(cmd, "Test Begin");

      ///////////////////////////////////////////////////
      // non-indexed, non-instanced

      // basic test
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->DrawInstanced(3, 1, 0, 0);
      view.TopLeftX += view.Width;

      // test with vertex offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->DrawInstanced(3, 1, 5, 0);
      view.TopLeftX += view.Width;

      // test with vertex offset and vbuffer offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 5 * sizeof(DefaultA2V);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->DrawInstanced(3, 1, 8, 0);
      view.TopLeftX += view.Width;

      // adjust to next row
      view.TopLeftX = 0.0f;
      view.TopLeftY += view.Height;

      ///////////////////////////////////////////////////
      // indexed, non-instanced

      ibv.BufferLocation = ib->GetGPUVirtualAddress();

      // basic test
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 1, 0, 0, 0);
      view.TopLeftX += view.Width;

      // test with first index
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 1, 5, 0, 0);
      view.TopLeftX += view.Width;

      // test with first index and vertex offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 1, 13, -50, 0);
      view.TopLeftX += view.Width;

      // test with first index and vertex offset and vbuffer offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 10 * sizeof(DefaultA2V);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 1, 23, -100, 0);
      view.TopLeftX += view.Width;

      // test with first index and vertex offset and vbuffer offset and ibuffer offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 19 * sizeof(DefaultA2V);
      ibv.BufferLocation = ib->GetGPUVirtualAddress() + 14 * sizeof(uint16_t);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 1, 23, -100, 0);
      view.TopLeftX += view.Width;

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      // indexed strip with primitive restart
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress();
      ibv.BufferLocation = ib->GetGPUVirtualAddress();
      RSSetViewport(cmd, view);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(12, 1, 42, 0, 0);
      view.TopLeftX += view.Width;

      // indexed strip with primitive restart and vertex offset
      RSSetViewport(cmd, view);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(12, 1, 54, -100, 0);
      view.TopLeftX += view.Width;

      // adjust to next row
      view.TopLeftX = 0.0f;
      view.TopLeftY += view.Height;

      cmd->SetPipelineState(instpso);
      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ///////////////////////////////////////////////////
      // non-indexed, instanced

      // basic test
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->DrawInstanced(3, 2, 0, 0);
      view.TopLeftX += view.Width;

      // basic test with first instance
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 5 * sizeof(DefaultA2V);
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->DrawInstanced(3, 2, 0, 5);
      view.TopLeftX += view.Width;

      // basic test with first instance and instance buffer offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 13 * sizeof(DefaultA2V);
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress() + 8 * sizeof(Vec4f);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->DrawInstanced(3, 2, 0, 5);
      view.TopLeftX += view.Width;

      // adjust to next row
      view.TopLeftX = 0.0f;
      view.TopLeftY += view.Height;

      ///////////////////////////////////////////////////
      // indexed, instanced

      ibv.BufferLocation = ib->GetGPUVirtualAddress();

      // basic test
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 2, 5, 0, 0);
      view.TopLeftX += view.Width;

      // basic test with first instance
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress() + 0;
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 2, 13, -50, 5);
      view.TopLeftX += view.Width;

      // basic test with first instance and instance buffer offset
      RSSetViewport(cmd, view);
      vbs[0].BufferLocation = vb->GetGPUVirtualAddress() + 0;
      vbs[1].BufferLocation = instvb->GetGPUVirtualAddress() + 8 * sizeof(Vec4f);
      cmd->IASetVertexBuffers(0, 2, vbs);
      cmd->IASetIndexBuffer(&ibv);
      cmd->DrawIndexedInstanced(3, 2, 23, -80, 5);
      view.TopLeftX += view.Width;

      ResourceBarrier(cmd, rtvtex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

      blitToSwap(cmd, rtvtex, bb);

      ResourceBarrier(cmd, rtvtex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
