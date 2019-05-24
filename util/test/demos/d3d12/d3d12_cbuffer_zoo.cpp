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

TEST(D3D12_CBuffer_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests every kind of constant that can be in a cbuffer to make sure it's decoded "
      "correctly";

  std::string pixel = R"EOSHADER(

struct float3_1 { float3 a; float b; };

struct nested { float3_1 a; float4 b[4]; float3_1 c[4]; };

cbuffer consts : register(b0)
{
  // dummy* entries are just to 'reset' packing to avoid pollution between tests

  float4 a;                               // basic float4 = {0, 1, 2, 3}
  float3 b;                               // should have a padding word at the end = {4, 5, 6}, <7>

  float2 c; float2 d;                     // should be packed together = {8, 9}, {10, 11}
  float e; float3 f;                      // should be packed together = 12, {13, 14, 15}
  float g; float2 h; float i;             // should be packed together = 16, {17, 18}, 19
  float j; float2 k;                      // should have a padding word at the end = 20, {21, 22}, <23>
  float2 l; float m;                      // should have a padding word at the end = {24, 25}, 26, <27>

  float n[4];                             // should cover 4 float4s = 28, <29..31>, 32, <33..35>, 36, <37..39>, 40
  float4 dummy1;

  float o[4];                             // should cover 4 float4s = 48, <..>, 52, <..>, 56, <..>, 60
  float p;                                // should be packed in with above array, with two padding words = 61, <62, 63>
  float4 dummy2;

  float4 dummygl1;                         // padding to match GL so matrices start on same values
  float4 dummygl2;

  column_major float4x4 q;                // should cover 4 float4s.
                                          // row0: {76, 80, 84, 88}
                                          // row1: {77, 81, 85, 89}
                                          // row2: {78, 82, 86, 90}
                                          // row3: {79, 83, 87, 91}
  row_major float4x4 r;                   // should cover 4 float4s
                                          // row0: {92, 93, 94, 95}
                                          // row1: {96, 97, 98, 99}
                                          // row2: {100, 101, 102, 103}
                                          // row3: {104, 105, 106, 107}

  column_major float3x4 s;                // covers 4 float4s with padding at end of each column
                                          // row0: {108, 112, 116, 120}
                                          // row1: {109, 113, 117, 121}
                                          // row2: {110, 114, 118, 122}
                                          //       <111, 115, 119, 123>
  float4 dummy3;
  row_major float3x4 t;                   // covers 3 float4s with no padding
                                          // row0: {128, 129, 130, 131}
                                          // row1: {132, 133, 134, 135}
                                          // row2: {136, 137, 138, 139}
  float4 dummy4;

  column_major float2x3 u;                // covers 3 float4s with padding at end of each column (but not row)
                                          // row0: {144, 148, 152}
                                          // row1: {145, 149, 153}
                                          //       <146, 150, 154>
                                          //       <147, 151, 155>
  float4 dummy5;
  row_major float2x3 v;                   // covers 2 float4s with padding at end of each row (but not column)
                                          // row0: {160, 161, 162}, <163>
                                          // row1: {164, 165, 166}, <167>
  float4 dummy6;

  column_major float2x2 w;                // covers 2 float4s with padding at end of each column (but not row)
                                          // row0: {172, 176}
                                          // row1: {173, 177}
                                          //       <174, 178>
                                          //       <175, 179>
  float4 dummy7;
  row_major float2x2 x;                   // covers 2 float4s with padding at end of each row (but not column)
                                          // row1: {184, 185}, <186, 187>
                                          // row1: {188, 189}, <190, 191>
  float4 dummy8;

  row_major float2x2 y;                   // covers the same as above, but z overlaps
                                          // row0: {196, 197}, <198, 199>
                                          // row1: {200, 201}, <202, 203>
  float z;                                // overlaps after padding in final row = 202

  float4 gldummy3;                        // account for z not overlapping in GL/VK

  row_major float4x1 aa;                  // covers 4 vec4s with maximum padding
                                          // row0: {208}, <209, 210, 211>
                                          // row1: {212}, <213, 214, 215>
                                          // row2: {216}, <217, 218, 219>
                                          // row3: {220}, <221, 222, 223>

  column_major float4x1 ab;               // covers 1 vec4 (equivalent to a plain vec4)
                                          // row0: {224}
                                          // row1: {225}
                                          // row2: {226}
                                          // row3: {227}

  float4 multiarray[3][2];                // [0][0] = {228, 229, 230, 231}
                                          // [0][1] = {232, 233, 234, 235}
                                          // [1][0] = {236, 237, 238, 239}
                                          // [1][1] = {240, 241, 242, 243}
                                          // [2][0] = {244, 245, 246, 247}
                                          // [2][1] = {248, 249, 250, 251}

  nested structa[2];                      // [0] = {
                                          //   .a = { { 252, 253, 254 }, 255 }
                                          //   .b[0] = { 256, 257, 258, 259 }
                                          //   .b[1] = { 260, 261, 262, 263 }
                                          //   .b[2] = { 264, 265, 266, 267 }
                                          //   .b[3] = { 268, 269, 270, 271 }
                                          //   .c[0] = { { 272, 273, 274 }, 275 }
                                          //   .c[1] = { { 276, 277, 278 }, 279 }
                                          //   .c[2] = { { 280, 281, 282 }, 283 }
                                          //   .c[3] = { { 284, 285, 286 }, 287 }
                                          // }
                                          // [1] = {
                                          //   .a = { { 288, 289, 290 }, 291 }
                                          //   .b[0] = { 292, 293, 294, 295 }
                                          //   .b[1] = { 296, 297, 298, 299 }
                                          //   .b[2] = { 300, 301, 302, 303 }
                                          //   .b[3] = { 304, 305, 306, 307 }
                                          //   .c[0] = { { 308, 309, 310 }, 311 }
                                          //   .c[1] = { { 312, 313, 314 }, 315 }
                                          //   .c[2] = { { 316, 317, 318 }, 319 }
                                          //   .c[3] = { { 320, 321, 322 }, 323 }
                                          // }

  column_major float3x2 ac;               // covers 2 float4s with padding at end of each column (but not row)
                                          // row0: {324, 328}
                                          // row1: {325, 329}
                                          // row2: {326, 330}
                                          //       <327, 331>
  row_major float3x2 ad;                  // covers 3 float4s with padding at end of each row (but not column)
                                          // row0: {332, 333}, <334, 335>
                                          // row1: {336, 337}, <338, 339>
                                          // row2: {340, 341}, <342, 343>

  column_major float3x2 ae[2];            // covers 2 float4s with padding at end of each column (but not row)
                                          // [0] = {
                                          //   row0: {344, 348}
                                          //   row1: {345, 349}
                                          //   row2: {346, 350}
                                          //         <347, 351>
                                          // }
                                          // [1] = {
                                          //   row0: {352, 356}
                                          //   row1: {353, 357}
                                          //   row2: {354, 358}
                                          //         <355, 359>
                                          // }
  row_major float3x2 af[2];               // covers 3 float4s with padding at end of each row (but not column)
                                          // [0] = {
                                          //   row0: {360, 361}, <362, 363>
                                          //   row1: {364, 365}, <366, 367>
                                          //   row2: {368, 369}, <370, 371>
                                          // }
                                          // [1] = {
                                          //   row0: {372, 373}, <374, 375>
                                          //   row1: {376, 377}, <378, 379>
                                          //   row2: {380, 381},
                                          // }

  float2 dummy9;                          // consumes leftovers from above array = {382, 383}

  float2 dummy10;                          // should have padding at the end = {384, 385}, <386, 387>

  row_major float2x2 ag;                  // each row is aligned to float4:
                                          // row0: {388, 389}, <390, 391>
                                          // row1: {392, 393},

  float2 dummy11;                          // consumes leftovers from above matrix = {394, 395}
  float2 dummy12;                          // should have padding at the end = {396, 397}, <398, 399>

  column_major float2x2 ah;               // each column is aligned to float4:
                                          // row0: {400, 404}
                                          // row1: {401, 405}
                                          //       <402, 406>
                                          //       <403, 407>

  row_major float2x2 ai[2];               // [0] = {
                                          //   row0: {408, 409}, <410, 411>
                                          //   row1: {412, 413}, <414, 415>
                                          // }
                                          // [1] = {
                                          //   row0: {416, 417}, <418, 419>
                                          //   row1: {420, 421}, <422, 423>
                                          // }
  column_major float2x2 aj[2];            // [0] = {
                                          //   row0: {424, 428}
                                          //   row1: {425, 429}
                                          //         <426, 430>
                                          //         <427, 431>
                                          // }
                                          // [1] = {
                                          //   row0: {432, 436}
                                          //   row1: {433, 437}
                                          //         <434, 438>
                                          //         <435, 439>
                                          // }

  float4 test;                            // {440, 441, 442, 443}
};

// this comes from root signature constants
cbuffer rootconsts : register(b1)
{
  float4 root_zero;
  float4 root_a;
  float2 root_b, root_c;
  float3_1 root_d;
};

float4 main() : SV_Target0
{
	return test + root_zero + float4(0.1f, 0.0f, 0.0f, 0.0f);
}

)EOSHADER";

  struct float3_1
  {
    float a[3];
    float b;
  };

  struct RootData
  {
    float root_zero[4];
    float root_a[4];
    float root_b[2], root_c[2];
    float3_1 root_d;
  };

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    Vec4f cbufferdata[512];

    for(int i = 0; i < 512; i++)
      cbufferdata[i] = Vec4f(float(i * 4 + 0), float(i * 4 + 1), float(i * 4 + 2), float(i * 4 + 3));

    RootData rootData = {};

    rootData.root_a[0] = 10.0f;
    rootData.root_a[1] = 20.0f;
    rootData.root_a[2] = 30.0f;
    rootData.root_a[3] = 40.0f;

    rootData.root_b[0] = 50.0f;
    rootData.root_b[1] = 60.0f;

    rootData.root_c[0] = 70.0f;
    rootData.root_c[1] = 80.0f;

    rootData.root_d.a[0] = 90.0f;
    rootData.root_d.a[1] = 100.0f;
    rootData.root_d.a[2] = 110.0f;
    rootData.root_d.b = 120.0f;

    static_assert(sizeof(rootData) == 64, "Root data is mis-sized");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);
    ID3D12ResourcePtr cb = MakeBuffer().Data(cbufferdata);

    ID3D12RootSignaturePtr sig = MakeSig({
        cbvParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0),
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 1, sizeof(rootData) / sizeof(uint32_t)),
    });

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).RTVs(
        {DXGI_FORMAT_R32G32B32A32_FLOAT});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(cb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, bbrtv, {0.4f, 0.5f, 0.6f, 1.0f});

      D3D12_CPU_DESCRIPTOR_HANDLE offrtv = MakeRTV(rtvtex).CreateCPU(0);

      ClearRenderTargetView(cmd, offrtv, {0.4f, 0.5f, 0.6f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);
      cmd->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
      cmd->SetGraphicsRoot32BitConstants(1, sizeof(rootData) / sizeof(uint32_t), &rootData, 0);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {offrtv}, {});

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
