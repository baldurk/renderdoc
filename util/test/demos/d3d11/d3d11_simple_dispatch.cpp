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

TEST(D3D11_Simple_Dispatch, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test that just does a dispatch and some copies, for checking basic compute stuff";

  std::string compute = R"EOSHADER(

Texture2D<uint> texin : register(t0);
RWTexture2D<uint> texout : register(u0);

[numthreads(1,1,1)]
void main()
{
	texout[uint2(3,4)] = texin[uint2(4,3)];
	texout[uint2(4,4)] = texin[uint2(3,3)];
	texout[uint2(4,3)] = texin[uint2(3,4)];
	texout[uint2(3,3)] = texin[uint2(4,4)];
	texout[uint2(0,0)] = texin[uint2(0,0)] + 3;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11ComputeShaderPtr cs = CreateCS(Compile(compute, "main", "cs_5_0"));

    uint32_t data[8 * 8] = {0};
    for(size_t i = 0; i < 8 * 8; i++)
    {
      data[i] = 5 + rand() % 100;
    }

    ID3D11Texture2DPtr tex[2] = {
        MakeTexture(DXGI_FORMAT_R32_UINT, 8, 8).SRV().UAV(),
        MakeTexture(DXGI_FORMAT_R32_UINT, 8, 8).SRV().UAV(),
    };
    ID3D11ShaderResourceViewPtr srv[2] = {
        MakeSRV(tex[0]), MakeSRV(tex[1]),
    };
    ID3D11UnorderedAccessViewPtr uav[2] = {
        MakeUAV(tex[0]), MakeUAV(tex[1]),
    };

    for(int i = 0; i < 2; i++)
      ctx->UpdateSubresource(tex[i], 0, NULL, data, sizeof(uint32_t) * 8, sizeof(uint32_t) * 8 * 8);

    while(Running())
    {
      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};
      ctx->ClearRenderTargetView(bbRTV, col);

      ctx->CSSetShader(cs, NULL, 0);

      ctx->UpdateSubresource(tex[1], 0, NULL, data, sizeof(uint32_t) * 8, sizeof(uint32_t) * 8 * 8);

      ctx->CSSetUnorderedAccessViews(0, 1, &uav[0].GetInterfacePtr(), NULL);
      ctx->CSSetShaderResources(0, 1, &srv[1].GetInterfacePtr());

      ctx->Dispatch(1, 1, 1);

      // copy from source to test, just for the sake of it (we could flipflop)
      ctx->CopyResource(tex[1], tex[0]);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
