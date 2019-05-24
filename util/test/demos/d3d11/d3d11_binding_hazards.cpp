/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

TEST(D3D11_Binding_Hazards, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Test of D3D11 hazard tracking write/read bindings";

  std::string compute = R"EOSHADER(

Texture2D<uint> texin : register(t0);
Buffer<uint> bufin : register(t1);
RWTexture2D<uint> texout1 : register(u0);
RWBuffer<uint> bufout1 : register(u1);
RWTexture2D<uint> texout2 : register(u2);
RWBuffer<uint> bufout2 : register(u3);

[numthreads(1,1,1)]
void main()
{
	texout1[uint2(3,4)] = bufin[3];
	texout2[uint2(4,4)] = texin[uint2(3,3)];
	bufout1[4] = bufin[4];
	bufout2[3] = texin[uint2(4,4)];
}

)EOSHADER";

  int main()
  {
    // so that running individually we get errors
    debugDevice = true;

    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11ComputeShaderPtr cs = CreateCS(Compile(compute, "main", "cs_5_0"));

    ID3D11Texture2DPtr tex0 = MakeTexture(DXGI_FORMAT_R32_UINT, 8, 8).UAV().RTV();
    ID3D11UnorderedAccessViewPtr uav0 = MakeUAV(tex0);
    ID3D11RenderTargetViewPtr rtv0 = MakeRTV(tex0);

    ID3D11Texture2DPtr tex1 = MakeTexture(DXGI_FORMAT_R32_UINT, 8, 8).UAV().RTV();
    ID3D11UnorderedAccessViewPtr uav1 = MakeUAV(tex1);
    ID3D11RenderTargetViewPtr rtv1 = MakeRTV(tex1);

    ID3D11BufferPtr buf1 = MakeBuffer().Size(65536).SRV().UAV();
    ID3D11BufferPtr buf2 = MakeBuffer().Size(65536).SRV();
    ID3D11ShaderResourceViewPtr buf1SRV = MakeSRV(buf1).Format(DXGI_FORMAT_R32_UINT);
    ID3D11UnorderedAccessViewPtr buf1UAV = MakeUAV(buf1).Format(DXGI_FORMAT_R32_UINT);

    while(Running())
    {
      ctx->ClearState();

      ctx->CSSetShader(cs, NULL, 0);

      ID3D11ShaderResourceViewPtr tempSRV =
          MakeSRV(buf2).Format(DXGI_FORMAT_R32_UINT).NumElements(128);

      ctx->CSSetShaderResources(1, 1, &tempSRV.GetInterfacePtr());

      tempSRV->AddRef();
      ULONG refcount = tempSRV->Release();

      ID3D11ShaderResourceView *srvs[2] = {NULL, tempSRV.GetInterfacePtr()};

      ctx->CSSetShaderResources(1, 2, srvs);

      refcount = tempSRV->AddRef();
      refcount = tempSRV->Release();

      ctx->CSSetShaderResources(3, 2, srvs);

      refcount = tempSRV->AddRef();
      refcount = tempSRV->Release();

      ctx->CSSetUnorderedAccessViews(0, 1, &uav0.GetInterfacePtr(), NULL);
      ctx->CSSetUnorderedAccessViews(2, 1, &uav1.GetInterfacePtr(), NULL);

      // try to bind the buffer to two slots, find it gets unbound
      ctx->CSSetUnorderedAccessViews(1, 1, &buf1UAV.GetInterfacePtr(), NULL);
      ctx->CSSetUnorderedAccessViews(3, 1, &buf1UAV.GetInterfacePtr(), NULL);

      ID3D11UnorderedAccessView *getCSUAVs[4] = {};
      ID3D11RenderTargetView *getOMRTV = NULL;
      ID3D11RenderTargetView *getOMRTVs[2] = {};
      ID3D11UnorderedAccessView *getOMUAV = NULL;

      // Dispatch each time so we can also check state in UI
      ctx->Dispatch(1, 1, 1);
      ctx->CSGetUnorderedAccessViews(0, 4, getCSUAVs);

      TEST_ASSERT(getCSUAVs[0] == uav0, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[1] == NULL, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[2] == uav1, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[3] == buf1UAV, "Unexpected binding");

      // this should unbind uav0 because it's re-bound as rtv0, then unbind uav1 because it's
      // rebound on another UAV slot
      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv0.GetInterfacePtr(), NULL, 1, 1,
                                                     &uav1.GetInterfacePtr(), NULL);

      ctx->Dispatch(1, 1, 1);
      ctx->OMGetRenderTargetsAndUnorderedAccessViews(1, &getOMRTV, NULL, 1, 1, &getOMUAV);
      ctx->CSGetUnorderedAccessViews(0, 4, getCSUAVs);

      TEST_ASSERT(getOMRTV == rtv0, "Unexpected binding");
      TEST_ASSERT(getOMUAV == uav1, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[0] == NULL, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[1] == NULL, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[2] == NULL, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[3] == buf1UAV, "Unexpected binding");

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
                                                     NULL, NULL, 1, 0, NULL, NULL);

      ctx->OMGetRenderTargetsAndUnorderedAccessViews(1, &getOMRTV, NULL, 1, 1, &getOMUAV);

      TEST_ASSERT(getOMRTV == rtv0, "Unexpected binding");
      TEST_ASSERT(getOMUAV == NULL, "Unexpected binding");

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv0.GetInterfacePtr(), NULL, 1, 1,
                                                     &uav1.GetInterfacePtr(), NULL);

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
                                                     &rtv1.GetInterfacePtr(), NULL, 1, 0, NULL, NULL);

      ctx->OMGetRenderTargetsAndUnorderedAccessViews(1, &getOMRTV, NULL, 1, 1, &getOMUAV);

      TEST_ASSERT(getOMRTV == rtv0, "Unexpected binding");
      TEST_ASSERT(getOMUAV == NULL, "Unexpected binding");

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv0.GetInterfacePtr(), NULL, 1, 1,
                                                     &uav1.GetInterfacePtr(), NULL);

      ID3D11RenderTargetView *empty = NULL;
      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &empty, NULL, 1,
                                                     D3D11_KEEP_UNORDERED_ACCESS_VIEWS, NULL, NULL);

      ctx->OMGetRenderTargetsAndUnorderedAccessViews(1, &getOMRTV, NULL, 1, 1, &getOMUAV);

      TEST_ASSERT(getOMRTV == empty, "Unexpected binding");
      TEST_ASSERT(getOMUAV == uav1, "Unexpected binding");

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv0.GetInterfacePtr(), NULL, 1, 1,
                                                     &uav1.GetInterfacePtr(), NULL);

      ctx->OMSetRenderTargetsAndUnorderedAccessViews(
          1, &empty, NULL, 1, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, &uav0.GetInterfacePtr(), NULL);

      ctx->OMGetRenderTargetsAndUnorderedAccessViews(1, &getOMRTV, NULL, 1, 1, &getOMUAV);

      TEST_ASSERT(getOMRTV == empty, "Unexpected binding");
      TEST_ASSERT(getOMUAV == uav1, "Unexpected binding");

      // finally this should unbind both OM views, and rebind back on the CS
      ctx->CSSetUnorderedAccessViews(0, 1, &uav0.GetInterfacePtr(), NULL);
      ctx->CSSetUnorderedAccessViews(2, 1, &uav1.GetInterfacePtr(), NULL);

      ctx->Dispatch(1, 1, 1);
      ctx->OMGetRenderTargetsAndUnorderedAccessViews(1, &getOMRTV, NULL, 1, 1, &getOMUAV);
      ctx->CSGetUnorderedAccessViews(0, 4, getCSUAVs);

      TEST_ASSERT(getOMRTV == NULL, "Unexpected binding");
      TEST_ASSERT(getOMUAV == NULL, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[0] == uav0, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[1] == NULL, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[2] == uav1, "Unexpected binding");
      TEST_ASSERT(getCSUAVs[3] == buf1UAV, "Unexpected binding");

      ctx->ClearState();

      ctx->CSSetShader(cs, NULL, 0);

      ID3D11RenderTargetView *RTVs[] = {
          rtv0, rtv0,
      };

      // can't bind the same RTV to two slots
      ctx->OMSetRenderTargets(2, RTVs, NULL);

      ctx->Dispatch(1, 1, 1);
      ctx->OMGetRenderTargetsAndUnorderedAccessViews(2, getOMRTVs, NULL, 2, 1, &getOMUAV);
      TEST_ASSERT(getOMRTVs[0] == NULL, "Unexpected binding");
      TEST_ASSERT(getOMRTVs[1] == NULL, "Unexpected binding");
      TEST_ASSERT(getOMUAV == NULL, "Unexpected binding");

      RTVs[0] = rtv1;
      RTVs[1] = rtv0;

      // this bind is fine, no overlapping state
      ctx->OMSetRenderTargetsAndUnorderedAccessViews(2, RTVs, NULL, 2, 1,
                                                     &buf1UAV.GetInterfacePtr(), NULL);

      ctx->Dispatch(1, 1, 1);
      ctx->OMGetRenderTargetsAndUnorderedAccessViews(2, getOMRTVs, NULL, 2, 1, &getOMUAV);
      TEST_ASSERT(getOMRTVs[0] == rtv1, "Unexpected binding");
      TEST_ASSERT(getOMRTVs[1] == rtv0, "Unexpected binding");
      TEST_ASSERT(getOMUAV == buf1UAV, "Unexpected binding");

      RTVs[0] = rtv0;
      RTVs[1] = rtv1;

      // this bind is discarded, because rtv0 overlaps uav0.
      ctx->OMSetRenderTargetsAndUnorderedAccessViews(2, RTVs, NULL, 2, 1, &uav0.GetInterfacePtr(),
                                                     NULL);

      ctx->Dispatch(1, 1, 1);
      ctx->OMGetRenderTargetsAndUnorderedAccessViews(2, getOMRTVs, NULL, 2, 1, &getOMUAV);
      TEST_ASSERT(getOMRTVs[0] == rtv1, "Unexpected binding");
      TEST_ASSERT(getOMRTVs[1] == rtv0, "Unexpected binding");
      TEST_ASSERT(getOMUAV == buf1UAV, "Unexpected binding");

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
