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

#include "d3d11_test.h"

RD_TEST(D3D11_Draw_Zoo, D3D11GraphicsTest)
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

    D3D11_INPUT_ELEMENT_DESC layoutdesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ID3D11InputLayoutPtr vertLayout;
    CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                    vsblob->GetBufferSize(), &vertLayout));

    layoutdesc[1].AlignedByteOffset = 0;
    layoutdesc[1].InputSlot = 1;
    layoutdesc[1].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
    layoutdesc[1].InstanceDataStepRate = 1;

    ID3D11InputLayoutPtr instlayout;
    CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                    vsblob->GetBufferSize(), &instlayout));

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

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

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(vbData);

    Vec4f instData[16] = {};
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

    ID3D11BufferPtr instvb = MakeBuffer().Vertex().Data(instData);

    std::vector<uint16_t> idxData;
    idxData.resize(100);

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

    ID3D11BufferPtr ib = MakeBuffer().Index().Data(idxData);

    CD3D11_RASTERIZER_DESC rd = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
    rd.CullMode = D3D11_CULL_NONE;

    ID3D11RasterizerStatePtr rs;
    CHECK_HR(dev->CreateRasterizerState(&rd, &rs));

    ID3D11Texture2DPtr fltTex =
        MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight).RTV().SRV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    while(Running())
    {
      ctx->OMSetRenderTargets(1, &fltRT.GetInterfacePtr(), NULL);

      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->RSSetState(rs);

      ClearRenderTargetView(fltRT, {0.2f, 0.2f, 0.2f, 1.0f});

      D3D11_VIEWPORT view = {0.0f, 0.0f, 48.0f, 48.0f, 0.0f, 1.0f};

      ctx->RSSetViewports(1, &view);

      ID3D11Buffer *vbs[2] = {vb, instvb};
      UINT strides[2] = {sizeof(DefaultA2V), sizeof(Vec4f)};
      UINT offsets[2] = {0, 0};

      ctx->IASetInputLayout(vertLayout);

      setMarker("Test Begin");

      ///////////////////////////////////////////////////
      // non-indexed, non-instanced

      // basic test
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->Draw(3, 0);
      view.TopLeftX += view.Width;

      // test with vertex offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->Draw(3, 5);
      view.TopLeftX += view.Width;

      // test with vertex offset and vbuffer offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 5 * sizeof(DefaultA2V);
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->Draw(3, 8);
      view.TopLeftX += view.Width;

      // adjust to next row
      view.TopLeftX = 0.0f;
      view.TopLeftY += view.Height;

      ///////////////////////////////////////////////////
      // indexed, non-instanced

      // basic test
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexed(3, 0, 0);
      view.TopLeftX += view.Width;

      // test with first index
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexed(3, 5, 0);
      view.TopLeftX += view.Width;

      // test with first index and vertex offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexed(3, 13, -50);
      view.TopLeftX += view.Width;

      // test with first index and vertex offset and vbuffer offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 10 * sizeof(DefaultA2V);
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexed(3, 23, -100);
      view.TopLeftX += view.Width;

      // test with first index and vertex offset and vbuffer offset and ibuffer offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 19 * sizeof(DefaultA2V);
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 14 * sizeof(uint16_t));
      ctx->DrawIndexed(3, 23, -100);
      view.TopLeftX += view.Width;

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      // indexed strip with primitive restart
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexed(12, 42, 0);
      view.TopLeftX += view.Width;

      // indexed strip with primitive restart and vertex offset
      ctx->RSSetViewports(1, &view);
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexed(12, 54, -100);
      view.TopLeftX += view.Width;

      // adjust to next row
      view.TopLeftX = 0.0f;
      view.TopLeftY += view.Height;

      ctx->IASetInputLayout(instlayout);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      ///////////////////////////////////////////////////
      // non-indexed, instanced

      // basic test
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      offsets[1] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->DrawInstanced(3, 2, 0, 0);
      view.TopLeftX += view.Width;

      // basic test with first instance
      ctx->RSSetViewports(1, &view);
      offsets[0] = 5 * sizeof(DefaultA2V);
      offsets[1] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->DrawInstanced(3, 2, 0, 5);
      view.TopLeftX += view.Width;

      // basic test with first instance and instance buffer offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 13 * sizeof(DefaultA2V);
      offsets[1] = 8 * sizeof(Vec4f);
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->DrawInstanced(3, 2, 0, 5);
      view.TopLeftX += view.Width;

      // adjust to next row
      view.TopLeftX = 0.0f;
      view.TopLeftY += view.Height;

      ///////////////////////////////////////////////////
      // indexed, instanced

      // basic test
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      offsets[1] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexedInstanced(3, 2, 5, 0, 0);
      view.TopLeftX += view.Width;

      // basic test with first instance
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      offsets[1] = 0;
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexedInstanced(3, 2, 13, -50, 5);
      view.TopLeftX += view.Width;

      // basic test with first instance and instance buffer offset
      ctx->RSSetViewports(1, &view);
      offsets[0] = 0;
      offsets[1] = 8 * sizeof(Vec4f);
      ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
      ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
      ctx->DrawIndexedInstanced(3, 2, 23, -80, 5);
      view.TopLeftX += view.Width;

      blitToSwap(fltTex);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
