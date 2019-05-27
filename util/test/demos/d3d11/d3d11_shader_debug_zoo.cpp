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

TEST(D3D11_Shader_Debug_Zoo, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests shader debugging in different edge cases";

  struct ConstsA2V
  {
    Vec3f pos;
    float zero;
    float one;
    float negone;
  };

  std::string common = R"EOSHADER(

struct consts
{
	float3 pos : POSITION;
	float zeroVal : ZERO;
	float oneVal : ONE;
	float negoneVal : NEGONE;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float2 zeroVal : ZERO;
	float oneVal : ONE;
	float negoneVal : NEGONE;
	uint tri : TRIANGLE;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

v2f main(consts IN, uint tri : SV_InstanceID)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.x + IN.pos.z * float(tri), IN.pos.y, 0.0f, 1);

	OUT.zeroVal = IN.zeroVal.xx;
	OUT.oneVal = IN.oneVal;
	OUT.negoneVal = IN.negoneVal;
	OUT.tri = tri;

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

Buffer<float> test : register(t0);

float4 main(v2f IN) : SV_Target0
{
  float	posinf = IN.oneVal/IN.zeroVal.x;
  float	neginf = IN.negoneVal/IN.zeroVal.x;
  float	nan = IN.zeroVal.x/IN.zeroVal.y;

	float negone = IN.negoneVal;
	float posone = IN.oneVal;
	float zero = IN.zeroVal.x;

	if(IN.tri == 0)
		return float4(log(negone), log(zero), log(posone), 1.0f);
	if(IN.tri == 1)
		return float4(log(posinf), log(neginf), log(nan), 1.0f);
	if(IN.tri == 2)
		return float4(exp(negone), exp(zero), exp(posone), 1.0f);
	if(IN.tri == 3)
		return float4(exp(posinf), exp(neginf), exp(nan), 1.0f);
	if(IN.tri == 4)
		return float4(sqrt(negone), sqrt(zero), sqrt(posone), 1.0f);
	if(IN.tri == 5)
		return float4(sqrt(posinf), sqrt(neginf), sqrt(nan), 1.0f);
	if(IN.tri == 6)
		return float4(rsqrt(negone), rsqrt(zero), rsqrt(posone), 1.0f);
	if(IN.tri == 7)
		return float4(saturate(posinf), saturate(neginf), saturate(nan), 1.0f);
	if(IN.tri == 8)
		return float4(min(posinf, nan), min(neginf, nan), min(nan, nan), 1.0f);
	if(IN.tri == 9)
		return float4(min(posinf, posinf), min(neginf, posinf), min(nan, posinf), 1.0f);
	if(IN.tri == 10)
		return float4(min(posinf, neginf), min(neginf, neginf), min(nan, neginf), 1.0f);
	if(IN.tri == 11)
		return float4(max(posinf, nan), max(neginf, nan), max(nan, nan), 1.0f);
	if(IN.tri == 12)
		return float4(max(posinf, posinf), max(neginf, posinf), max(nan, posinf), 1.0f);
	if(IN.tri == 13)
		return float4(max(posinf, neginf), max(neginf, neginf), max(nan, neginf), 1.0f);

  // rounding tests
  float round_a = 1.7f*posone;
  float round_b = 2.1f*posone;
  float round_c = 1.5f*posone;
  float round_d = 2.5f*posone;
  float round_e = zero;
  float round_f = -1.7f*posone;
  float round_g = -2.1f*posone;
  float round_h = -1.5f*posone;
  float round_i = -2.5f*posone;

  if(IN.tri == 14)
    return float4(round(round_a), floor(round_a), ceil(round_a), trunc(round_a));
  if(IN.tri == 15)
    return float4(round(round_b), floor(round_b), ceil(round_b), trunc(round_b));
  if(IN.tri == 16)
    return float4(round(round_c), floor(round_c), ceil(round_c), trunc(round_c));
  if(IN.tri == 17)
    return float4(round(round_d), floor(round_d), ceil(round_d), trunc(round_d));
  if(IN.tri == 18)
    return float4(round(round_e), floor(round_e), ceil(round_e), trunc(round_e));
  if(IN.tri == 19)
    return float4(round(round_f), floor(round_f), ceil(round_f), trunc(round_f));
  if(IN.tri == 20)
    return float4(round(round_g), floor(round_g), ceil(round_g), trunc(round_g));
  if(IN.tri == 21)
    return float4(round(round_h), floor(round_h), ceil(round_h), trunc(round_h));
  if(IN.tri == 22)
    return float4(round(round_i), floor(round_i), ceil(round_i), trunc(round_i));

  if(IN.tri == 23)
    return float4(round(neginf), floor(neginf), ceil(neginf), trunc(neginf));
  if(IN.tri == 24)
    return float4(round(posinf), floor(posinf), ceil(posinf), trunc(posinf));
  if(IN.tri == 25)
    return float4(round(nan), floor(nan), ceil(nan), trunc(nan));

  if(IN.tri == 26)
    return test[5].xxxx;

  if(IN.tri == 27)
  {
    uint unsignedVal = uint(344.1f*posone);
    int signedVal = int(344.1f*posone);
    return float4(firstbithigh(unsignedVal), firstbitlow(unsignedVal),
                  firstbithigh(signedVal), firstbitlow(signedVal));
  }

  if(IN.tri == 28)
  {
    int signedVal = int(344.1f*negone);
    return float4(firstbithigh(signedVal), firstbitlow(signedVal), 0.0f, 0.0f);
  }

  // saturate NaN returns 0
  if(IN.tri == 29)
    return float4(0.1f+saturate(nan * 2.0f), 0.1f+saturate(nan * 3.0f), 0.1f+saturate(nan * 4.0f), 1.0f);

  // min() and max() with NaN return the other component if it's non-NaN, or else nan if it is nan
  if(IN.tri == 30)
    return float4(min(nan, 0.3f), max(nan, 0.3f), max(nan, nan), 1.0f);

  // the above applies componentwise
  if(IN.tri == 31)
    return max( float4(0.1f, 0.2f, 0.3f, 0.4f), nan.xxxx );
  if(IN.tri == 32)
    return min( float4(0.1f, 0.2f, 0.3f, 0.4f), nan.xxxx );

  // negating nan and abs(nan) gives nan
  if(IN.tri == 33)
    return float4(-nan, abs(nan), 0.0f, 1.0f);

	return float4(0.4f, 0.4f, 0.4f, 0.4f);
}

)EOSHADER";

  static const uint32_t numTests = 34;

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(common + pixel, "main", "ps_5_0");

    D3D11_INPUT_ELEMENT_DESC layoutdesc[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
        {
            "ZERO", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
        {
            "ONE", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
        {
            "NEGONE", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
            D3D11_INPUT_PER_VERTEX_DATA, 0,
        },
    };

    ID3D11InputLayoutPtr layout;
    CHECK_HR(dev->CreateInputLayout(layoutdesc, ARRAY_COUNT(layoutdesc), vsblob->GetBufferPointer(),
                                    vsblob->GetBufferSize(), &layout));

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    static const uint32_t texDim = AlignUp(numTests, 64U) * 4;

    ID3D11Texture2DPtr fltTex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, texDim, 4).RTV();
    ID3D11RenderTargetViewPtr fltRT = MakeRTV(fltTex);

    float triWidth = 8.0f / float(texDim);

    ConstsA2V triangle[] = {
        {Vec3f(-1.0f, -1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
        {Vec3f(-1.0f + triWidth, 1.0f, triWidth), 0.0f, 1.0f, -1.0f},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(triangle);

    float testdata[] = {
        1.0f,  2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,  9.0f,  10.0f,
        11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 120.0f,
    };

    ID3D11BufferPtr srvBuf = MakeBuffer().SRV().Data(testdata);
    ID3D11ShaderResourceViewPtr srv = MakeSRV(srvBuf).Format(DXGI_FORMAT_R32_FLOAT);

    ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

    while(Running())
    {
      ClearRenderTargetView(fltRT, {0.4f, 0.5f, 0.6f, 1.0f});
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(ConstsA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(layout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)texDim, 4.0f, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &fltRT.GetInterfacePtr(), NULL);

      ctx->DrawInstanced(3, numTests, 0, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
