/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

TEST(D3D12_Vertex_Attr_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle but using different kinds of vertex attributes, including doubles, arrays, "
      "matrices, and formats that require manual decode as they are vertex-buffer exclusive on "
      "some hardware such as USCALED.";

  struct vertin
  {
    int16_t i16[4];
    uint16_t u16[4];
    double df[2];
    float arr0[2];
    float arr1[2];
    float arr2[2];
    float mat0[2];
    float mat1[2];
  };

  std::string common = R"EOSHADER(

struct a2v
{
 float4 SNorm : SNORM;
 float4 UNorm : UNORM;
 uint4 UInt : UINT;
 float2 Array[3] : ARRAY;
 float2x2 Matrix : MATRIX;
};

struct v2f
{
  float4 pos : SV_Position;
  a2v data;
};

)EOSHADER";

  std::string vertex = R"EOSHADER(

v2f main(in a2v IN, in uint idx : SV_VertexID)
{
  float2 pos[3] = {float2(-0.5f, 0.5f), float2(0.0f, -0.5f), float2(0.5f, 0.5f)};

  v2f OUT = (v2f)0;
  OUT.pos = float4(pos[idx], 0.0f, 1.0f);
  OUT.data = IN;
  return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

float4 main(in v2f IN) : SV_Target0
{
  // check values came through correctly

  // SNorm should be in [-1, 1]
  if(any(clamp(IN.data.SNorm, -1.0, 1.0) != IN.data.SNorm))
    return float4(0.1f, 0, 0, 1);

  // UNorm should be in [0, 1]
  if(any(clamp(IN.data.UNorm, 0.0, 1.0) != IN.data.UNorm))
    return float4(0.2f, 0, 0, 1);

  // Similar for UInt
  if(IN.data.UInt.x > 65535 || IN.data.UInt.y > 65535 || IN.data.UInt.z > 65535 || IN.data.UInt.w > 65535)
    return float4(0.3f, 0, 0, 1);

  return float4(0, 1.0f, 0, 1);
}

)EOSHADER";

  std::string geom = R"EOSHADER(

[maxvertexcount(3)]
void main(triangle v2f input[3], inout TriangleStream<v2f> TriStream)
{
  for(int i = 0; i < 3; i++)
  {
    v2f output = input[i];
    output.pos = float4(output.pos.yx, 0.4f, 1.2f);
    TriStream.Append(output);
  }

  TriStream.RestartStrip();
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    vertin triangle[] = {
        {
            {32767, -32768, 32767, -32767},
            {12345, 6789, 1234, 567},
            {9.8765432109, -5.6789012345},
            {1.0f, 2.0f},
            {3.0f, 4.0f},
            {5.0f, 6.0f},
            {7.0, 8.0f},
            {9.0f, 10.0f},
        },
        {
            {32766, -32766, 16000, -16000},
            {56, 7890, 123, 4567},
            {-7.89012345678, 6.54321098765},
            {11.0f, 12.0f},
            {13.0f, 14.0f},
            {15.0f, 16.0f},
            {17.0, 18.0f},
            {19.0f, 20.0f},
        },
        {
            {5, -5, 0, 0},
            {8765, 43210, 987, 65432},
            {0.1234567890123, 4.5678901234},
            {21.0f, 22.0f},
            {23.0f, 24.0f},
            {25.0f, 26.0f},
            {27.0, 28.0f},
            {29.0f, 30.0f},
        },
    };

    ID3DBlobPtr vsblob = Compile(common + vertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(common + pixel, "main", "ps_4_0");
    ID3DBlobPtr gsblob = Compile(common + geom, "main", "gs_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(triangle);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).VS(vsblob).PS(psblob).GS(gsblob).InputLayout({
        {
            "SNORM", 0, DXGI_FORMAT_R16G16B16A16_SNORM, 0, offsetof(vertin, i16),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "UNORM", 0, DXGI_FORMAT_R16G16B16A16_UNORM, 0, offsetof(vertin, u16),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "UINT", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, offsetof(vertin, u16),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "ARRAY", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertin, arr0),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "ARRAY", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertin, arr1),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "ARRAY", 2, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertin, arr2),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "MATRIX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertin, mat0),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
        {
            "MATRIX", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(vertin, mat1),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0,
        },
    });

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.4f, 0.5f, 0.6f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(vertin), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
