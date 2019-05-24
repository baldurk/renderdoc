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

TEST(D3D11_Structured_Buffer_MisalignedDirty, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Test updating a misaligned chunk of a structured buffer";

  std::string pixel = R"EOSHADER(

struct mystruct
{
	uint data[5];
};

StructuredBuffer<mystruct> buf1 : register(t0);
StructuredBuffer<mystruct> buf2 : register(t1);

float4 main() : SV_Target0
{
	float3 first = float3(buf1[0].data[0], buf1[0].data[1], buf1[0].data[2]) +
									float3(buf2[0].data[0], buf2[0].data[1], buf2[0].data[2]);

	float last =	float(buf1[0].data[4]) + float(buf2[0].data[4]);

	return float4(first, last)/100.0f;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    if(!opts.MapNoOverwriteOnDynamicBufferSRV)
      TEST_ERROR("Can't run Structured_Buffer_MisalignedDirty test without mappable SRVs");

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    uint32_t data[5 * 100];

    for(int i = 0; i < 5 * 100; i++)
      data[i] = uint32_t(i);

    ID3D11BufferPtr structbuf =
        MakeBuffer().Structured(5 * sizeof(uint32_t)).Data(data).SRV().Mappable();
    ID3D11ShaderResourceViewPtr structbufSRV[2] = {
        MakeSRV(structbuf), MakeSRV(structbuf).FirstElement(5).NumElements(1),
    };

    while(Running())
    {
      ctx->Flush();

      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      // map the buffer, but only update a misaligned section - so the update won't be aligned to
      // structured size.
      {
        D3D11_MAPPED_SUBRESOURCE mapped = Map(structbuf, 0, D3D11_MAP_WRITE_NO_OVERWRITE);

        uint32_t *ptr = (uint32_t *)mapped.pData;

        if(ptr)
        {
          // find the 5th element (first in structbufSRV[1])
          ptr += 5 * 5;

          // move to the 3rd uint
          ptr += 3;

          // set the next 5 uints to 0 - that means [3], [4] in this element and [0], [1], [2] in
          // the next
          memset(ptr, 0, sizeof(uint32_t) * 5);

          ctx->Unmap(structbuf, 0);
        }
      }

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      ctx->PSSetShaderResources(0, 2, (ID3D11ShaderResourceView **)structbufSRV);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      ctx->Draw(3, 0);

      // ensure the initial state at the start of each frame is always pristine
      {
        D3D11_MAPPED_SUBRESOURCE mapped = Map(structbuf, 0, D3D11_MAP_WRITE_DISCARD);

        memcpy(mapped.pData, data, sizeof(data));

        ctx->Unmap(structbuf, 0);
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
