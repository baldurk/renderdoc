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

TEST(D3D11_Many_UAVs, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test using more than 8 compute shader UAVs (D3D11.1 feature)";

  std::string compute = R"EOSHADER(

RWBuffer<uint4> uav : register(u20);

[numthreads(1, 1, 1)]
void main()
{
	uav[0] = uint4(7,8,9,10);
}

)EOSHADER";

  int main()
  {
    d3d11_1 = true;

    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11ComputeShaderPtr cs = CreateCS(Compile(compute, "main", "cs_5_0"));

    ID3D11BufferPtr buf = MakeBuffer().Size(16).UAV();
    ID3D11UnorderedAccessViewPtr uav = MakeUAV(buf).Format(DXGI_FORMAT_R32G32B32A32_UINT);

    while(Running())
    {
      Vec4f col(0.4f, 0.5f, 0.6f, 1.0f);
      ClearRenderTargetView(bbRTV, col);

      ctx->ClearUnorderedAccessViewUint(uav, (uint32_t *)&col.x);

      ctx->CSSetUnorderedAccessViews(20, 1, &uav.GetInterfacePtr(), NULL);
      ctx->CSSetShader(cs, NULL, 0);

      ctx->Dispatch(1, 1, 1);

      std::vector<byte> contents = GetBufferData(buf);

      uint32_t *u32 = (uint32_t *)&contents[0];

      TEST_LOG("Data: %u %u %u %u", u32[0], u32[1], u32[2], u32[3]);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();