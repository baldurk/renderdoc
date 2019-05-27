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

TEST(D3D11_Byte_Address_Buffers, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests reading and writing from byte address buffers";

  std::string compute = R"EOSHADER(

ByteAddressBuffer inbuf : register(t0);
RWByteAddressBuffer outbuf : register(u0);

[numthreads(1, 1, 1)]
void main()
{
	uint4 data = inbuf.Load4(5*4);
	outbuf.Store4(10*4, data);

	data.xy = inbuf.Load2(15*4);
	outbuf.Store2(0, data.xy);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11ComputeShaderPtr cs = CreateCS(Compile(compute, "main", "cs_5_0"));

    ID3D11BufferPtr buf = MakeBuffer().ByteAddressed().UAV().Size(512);
    ID3D11UnorderedAccessViewPtr uav = MakeUAV(buf).Format(DXGI_FORMAT_R32_TYPELESS);

    uint32_t data[128] = {};
    for(int i = 0; i < 128; i++)
      data[i] = (uint32_t)rand();

    ID3D11BufferPtr buf2 = MakeBuffer().ByteAddressed().SRV().Data(data);
    ID3D11ShaderResourceViewPtr srv = MakeSRV(buf2).Format(DXGI_FORMAT_R32_TYPELESS);

    while(Running())
    {
      Vec4f col(0.4f, 0.5f, 0.6f, 1.0f);
      ClearRenderTargetView(bbRTV, col);

      ctx->ClearUnorderedAccessViewUint(uav, (uint32_t *)&col.x);

      ctx->CSSetShaderResources(0, 1, &srv.GetInterfacePtr());
      ctx->CSSetUnorderedAccessViews(0, 1, &uav.GetInterfacePtr(), NULL);
      ctx->CSSetShader(cs, NULL, 0);

      ctx->Dispatch(1, 1, 1);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
