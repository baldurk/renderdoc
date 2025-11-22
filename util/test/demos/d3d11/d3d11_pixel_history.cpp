/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2025 Baldur Karlsson
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

RD_TEST(D3D11_Pixel_History, D3D11GraphicsTest)
{
  static constexpr const char *Description = "Tests pixel history";

  const std::string vertex = R"EOSHADER(

void main(float3 pos : POSITION, float4 col : COLOR0,
          out float4 outPos : SV_POSITION, out COL_TYPE outCol : COLOR0)
{
	outPos = float4(pos, 1.0f);
	outCol = ProcessColor(col);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

void main(float4 pos : SV_POSITION, COL_TYPE col : COLOR0
#if COLOR == 1
  , out COL_TYPE outCol : SV_Target0
#endif
  )
{
  if (pos.x < DISCARD_X+1 && pos.x > DISCARD_X)
    discard;
  if (col.x > 10000 && col.y > 10000 && col.z > 10000)
    discard;
#if COLOR == 1
	outCol = col + COL_TYPE(0, 0, 0, ProcessColor((ALPHA_ADD).xxxx).x);
#endif
}

)EOSHADER";

  std::string mspixel = R"EOSHADER(

void main(float4 pos : SV_POSITION, uint sampleId : SV_SampleIndex
#if COLOR == 1
  , out COL_TYPE outCol : SV_Target0
#endif
  )
{
#if COLOR == 1
  float4 col = 0.0f.xxxx;
  if (sampleId == 0)
    col = float4(1, 0, 0, 1+ALPHA_ADD);
  else if (sampleId == 1)
    col = float4(0, 0, 1, 1+ALPHA_ADD);
  else if (sampleId == 2)
    col = float4(0, 1, 1, 1+ALPHA_ADD);
  else if (sampleId == 3)
    col = float4(1, 1, 1, 1+ALPHA_ADD);
  outCol = ProcessColor(col);
#endif
}

)EOSHADER";

  std::string comp = R"EOSHADER(

IMAGE_TYPE<COL_TYPE> Output : register(u0);

[numthreads(1, 1, 1)]
void main()
{
  float4 data = float4(3, 3, 3, 9);

  for(int x=0; x < 10; x++)
    for(int y=0; y < 10; y++)
      Output[STORE_TYPE(STORE_X+x, STORE_Y+y STORE_Z)] = ProcessColor(data);
}

)EOSHADER";

  struct D3D11StateSetup
  {
    struct SetupInfo
    {
      ID3D11VertexShaderPtr VS;
      ID3D11PixelShaderPtr PS;
      D3D11_DEPTH_STENCIL_DESC DepthStencilState;
      D3D11_RASTERIZER_DESC RasterizerState;
      D3D11_BLEND_DESC BlendState;
    };

    ID3D11VertexShaderPtr VS;
    ID3D11PixelShaderPtr PS;
    ID3D11DepthStencilStatePtr DS;
    ID3D11RasterizerStatePtr RS;
    ID3D11BlendStatePtr BS;
    UINT ref = 0x55;
    UINT SampleMask = ~0U;

    void init(ID3D11DevicePtr device, const SetupInfo &info)
    {
      VS = info.VS;
      PS = info.PS;
      device->CreateDepthStencilState(&info.DepthStencilState, &DS);
      device->CreateRasterizerState(&info.RasterizerState, &RS);
      device->CreateBlendState(&info.BlendState, &BS);
    }

    void set(ID3D11DeviceContextPtr context) const
    {
      context->VSSetShader(VS, NULL, 0);
      context->PSSetShader(PS, NULL, 0);
      context->RSSetState(RS);
      context->OMSetDepthStencilState(DS, ref);
      context->OMSetBlendState(BS, NULL, SampleMask);
    }
  };

  struct D3D11TestBatch
  {
    std::string name;

    bool uint = false;

    uint32_t mip = 0;

    ID3D11Texture2DPtr colImg2D;

    ID3D11RenderTargetViewPtr RTV;
    ID3D11DepthStencilViewPtr DSV;

    DXGI_FORMAT depthFormat;

    ID3D11UnorderedAccessViewPtr compUAV;
    ID3D11ComputeShaderPtr comp;

    uint32_t width, height;

    D3D11StateSetup depthWriteState;
    D3D11StateSetup scissorState;
    D3D11StateSetup stencilRefState;
    D3D11StateSetup stencilMaskState;
    D3D11StateSetup depthEqualState;
    D3D11StateSetup depthState;
    D3D11StateSetup stencilWriteState;
    D3D11StateSetup backgroundState;
    D3D11StateSetup noPsState;
    D3D11StateSetup noOutState;
    D3D11StateSetup basicState;
    D3D11StateSetup colorMaskState;
    D3D11StateSetup cullFrontState;
    D3D11StateSetup sampleColourState;
  };

  std::vector<D3D11TestBatch> batches;

  void BuildTestBatch(const std::string &name, uint32_t sampleCount, DXGI_FORMAT colourFormat,
                      DXGI_FORMAT depthStencilFormat, int targetMip = -1, int targetSlice = -1,
                      int targetDepthSlice = -1)
  {
    batches.push_back({});

    D3D11TestBatch &batch = batches.back();
    batch.name = name;

    batch.width = screenWidth;
    batch.height = screenHeight;

    uint32_t arraySize = targetSlice >= 0 ? 5 : 1;
    uint32_t mips = targetMip >= 0 ? 4 : 1;
    uint32_t depth = targetDepthSlice >= 0 ? 14 : 1;

    uint32_t mip = (uint32_t)std::max(0, targetMip);
    uint32_t slice = (uint32_t)std::max(0, targetSlice);
    if(depth > 1)
      slice = (uint32_t)targetDepthSlice;

    batch.mip = mip;

    std::string defines;
    defines +=
        "#define COLOR " + std::string(colourFormat == DXGI_FORMAT_UNKNOWN ? "0" : "1") + "\n";

    if(colourFormat == DXGI_FORMAT_R8G8B8A8_UINT || colourFormat == DXGI_FORMAT_R16G16B16A16_UINT ||
       colourFormat == DXGI_FORMAT_R32G32B32A32_UINT)
    {
      batch.uint = true;
      defines += R"(

#define COL_TYPE uint4
#define ALPHA_ADD 3

uint4 ProcessColor(float4 col)
{
  uint4 ret = uint4(16*col);

  if(col.x < 0.0f) ret.x = 0xfffffff0;
  if(col.y < 0.0f) ret.y = 0xfffffff0;
  if(col.z < 0.0f) ret.z = 0xfffffff0;


  return ret;
}

)";
    }
    else
    {
      defines += R"(

#define COL_TYPE float4
#define ALPHA_ADD 1.75

float4 ProcessColor(float4 col)
{
  float4 ret = col;

  // this obviously won't overflow F32 but it will overflow anything 16-bit and under
  if(col.x < 0.0f) ret.x = 100000.0f;
  if(col.y < 0.0f) ret.y = 100000.0f;
  if(col.z < 0.0f) ret.z = 100000.0f;

  return ret;
}

)";
    }

    defines += "#define DISCARD_X " + std::to_string(150 >> mip) + "\n";
    defines += "#define IMAGE_TYPE " +
               std::string(depth > 1       ? "RWTexture3D"
                           : arraySize > 1 ? "RWTexture2DArray"
                                           : "RWTexture2D") +
               "\n";
    defines += "#define STORE_X " + std::to_string(220 >> mip) + "\n";
    defines += "#define STORE_Y " + std::to_string(80 >> mip) + "\n";
    if(depth > 1 || arraySize > 1)
    {
      defines += "#define STORE_TYPE uint3\n";
      defines += "#define STORE_Z , " + std::to_string(slice) + "\n";
    }
    else
    {
      defines += "#define STORE_TYPE uint2\n";
      defines += "#define STORE_Z \n";
    }
    defines += "\n\n";

    ID3DBlobPtr vsBlob = Compile(defines + vertex, "main", "vs_5_0");
    ID3DBlobPtr psBlob = Compile(defines + pixel, "main", "ps_5_0");
    ID3DBlobPtr psMsaaBlob = Compile(defines + mspixel, "main", "ps_5_0");

    ID3DBlobPtr noOutPSBlob;

    if(colourFormat != DXGI_FORMAT_UNKNOWN)
    {
      defines += "#undef COLOR\n#define COLOR 0\n";
      noOutPSBlob = Compile(defines + pixel, "main", "ps_5_0");

      D3D11TextureCreator colCreator =
          MakeTexture(colourFormat, batch.width, batch.height, depth)
              .Mips(mips)
              // this is DepthOrArraySize so need to set it correctly from either
              .Array(arraySize)
              .Multisampled(sampleCount)
              .SRV()
              .RTV();

      if(sampleCount == 1)
        colCreator.UAV();

      if(depth > 1)
      {
        ID3D11Texture3DPtr colImg = colCreator;
        SetDebugName(colImg, batch.name + " colImg");

        batch.RTV = MakeRTV(colImg).FirstMip(mip).NumMips(1).FirstSlice(slice).NumSlices(1);

        batch.comp = CreateCS(Compile(defines + comp, "main", "cs_5_0"));
        SetDebugName(batch.comp, batch.name + " comp");
        batch.compUAV = MakeUAV(colImg).FirstMip(mip);
        SetDebugName(batch.compUAV, batch.name + " compUAV");
      }
      else
      {
        batch.colImg2D = colCreator;
        SetDebugName(batch.colImg2D, batch.name + " colImg");

        batch.RTV = MakeRTV(batch.colImg2D).FirstMip(mip).NumMips(1).FirstSlice(slice).NumSlices(1);

        if(sampleCount == 1)
        {
          batch.comp = CreateCS(Compile(defines + comp, "main", "cs_5_0"));
          SetDebugName(batch.comp, batch.name + " comp");
          batch.compUAV = MakeUAV(batch.colImg2D).FirstMip(mip);
          SetDebugName(batch.compUAV, batch.name + " compUAV");
        }
      }
    }

    batch.depthFormat = depthStencilFormat;

    if(depthStencilFormat != DXGI_FORMAT_UNKNOWN)
    {
      // can't create 2D views of 3D with depth images, so we just use a normal 2D array image
      if(depth > 1)
      {
        arraySize = depth;
        depth = 1;
      }

      ID3D11Texture2DPtr depthImg =
          MakeTexture(depthStencilFormat, batch.width, batch.height, depth)
              .Mips(mips)
              // this is DepthOrArraySize so need to set it correctly from either
              .Array(arraySize)
              .Multisampled(sampleCount)
              .DSV();
      SetDebugName(depthImg, batch.name + " depthImg");

      batch.DSV = MakeDSV(depthImg).FirstMip(mip).NumMips(1).FirstSlice(slice).NumSlices(1);
    }

    batch.width >>= mip;
    batch.height >>= mip;

    D3D11StateSetup::SetupInfo stateInfo = {};

    ID3D11VertexShaderPtr VS = CreateVS(vsBlob);
    SetDebugName(VS, batch.name + " NormalVS");
    stateInfo.VS = VS;

    ID3D11PixelShaderPtr PS = CreatePS(psBlob);
    SetDebugName(PS, batch.name + " NormalPS");
    stateInfo.PS = PS;

    ID3D11PixelShaderPtr noOutPS;

    if(noOutPSBlob)
    {
      noOutPS = CreatePS(noOutPSBlob);
      SetDebugName(noOutPS, batch.name + " NoOutPS");
    }

    stateInfo.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xf;

    stateInfo.RasterizerState.FillMode = D3D11_FILL_SOLID;
    stateInfo.RasterizerState.MultisampleEnable = sampleCount > 1;
    stateInfo.RasterizerState.ScissorEnable = TRUE;
    stateInfo.RasterizerState.DepthClipEnable = TRUE;
    stateInfo.RasterizerState.CullMode = D3D11_CULL_BACK;

    stateInfo.DepthStencilState.DepthEnable = TRUE;
    stateInfo.DepthStencilState.DepthFunc = D3D11_COMPARISON_ALWAYS;
    stateInfo.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    stateInfo.DepthStencilState.StencilEnable = FALSE;
    stateInfo.DepthStencilState.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    stateInfo.DepthStencilState.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    stateInfo.DepthStencilState.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    stateInfo.DepthStencilState.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    stateInfo.DepthStencilState.StencilReadMask = 0xff;
    stateInfo.DepthStencilState.StencilWriteMask = 0xff;
    stateInfo.DepthStencilState.BackFace = stateInfo.DepthStencilState.FrontFace;

    stateInfo.DepthStencilState.DepthFunc = D3D11_COMPARISON_ALWAYS;
    batch.depthWriteState.init(dev, stateInfo);

    {
      D3D11StateSetup::SetupInfo depthEqualStateInfo = stateInfo;
      depthEqualStateInfo.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
      depthEqualStateInfo.DepthStencilState.DepthFunc = D3D11_COMPARISON_EQUAL;

      batch.depthEqualState.init(dev, depthEqualStateInfo);
    }

    {
      D3D11StateSetup::SetupInfo scissorStencilStates = stateInfo;
      scissorStencilStates.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
      scissorStencilStates.DepthStencilState.DepthEnable = FALSE;

      batch.scissorState.init(dev, scissorStencilStates);

      batch.stencilRefState.init(dev, scissorStencilStates);
      batch.stencilRefState.ref = 0x67;

      scissorStencilStates.DepthStencilState.StencilReadMask = 0;
      scissorStencilStates.DepthStencilState.StencilWriteMask = 0;

      batch.stencilMaskState.init(dev, scissorStencilStates);
    }

    stateInfo.DepthStencilState.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    batch.depthState.init(dev, stateInfo);

    stateInfo.DepthStencilState.StencilEnable = TRUE;
    batch.stencilWriteState.init(dev, stateInfo);

    stateInfo.DepthStencilState.StencilEnable = FALSE;
    batch.backgroundState.init(dev, stateInfo);

    {
      stateInfo.PS = NULL;
      stateInfo.DepthStencilState.StencilEnable = TRUE;
      batch.noPsState.init(dev, stateInfo);
      batch.noPsState.ref = 0x33;
      stateInfo.PS = PS;
    }

    if(noOutPS)
    {
      stateInfo.PS = noOutPS;
      stateInfo.DepthStencilState.StencilEnable = FALSE;
      batch.noOutState.init(dev, stateInfo);
      stateInfo.PS = PS;
    }

    stateInfo.DepthStencilState.StencilEnable = TRUE;
    stateInfo.DepthStencilState.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
    batch.basicState.init(dev, stateInfo);
    stateInfo.DepthStencilState.StencilEnable = FALSE;

    {
      D3D11StateSetup::SetupInfo maskStateInfo = stateInfo;
      maskStateInfo.BlendState.RenderTarget[0].RenderTargetWriteMask = 0x3;
      batch.colorMaskState.init(dev, maskStateInfo);
    }

    stateInfo.RasterizerState.CullMode = D3D11_CULL_FRONT;
    batch.cullFrontState.init(dev, stateInfo);
    stateInfo.RasterizerState.CullMode = D3D11_CULL_BACK;

    stateInfo.PS = CreatePS(psMsaaBlob);
    SetDebugName(stateInfo.PS, batch.name + " MSAAPS");

    stateInfo.DepthStencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    batch.sampleColourState.init(dev, stateInfo);
    if(sampleCount == 4)
      batch.sampleColourState.SampleMask = 0x7;
  }

  void RunDraw(const PixelHistory::draw &draw, uint32_t numInstances = 1)
  {
    ctx->DrawInstanced(draw.count, numInstances, draw.first, 0);
  }

  void RunBatch(const D3D11TestBatch &b)
  {
    float factor = float(b.width) / float(screenWidth);

    D3D11_VIEWPORT v = {};
    v.Width = (float)b.width - (20.0f * factor);
    v.Height = (float)b.height - (20.0f * factor);
    v.TopLeftX = floorf(10.0f * factor);
    v.TopLeftY = floorf(10.0f * factor);
    v.MaxDepth = 1.0f;

    D3D11_RECT scissor = {0, 0, (LONG)b.width, (LONG)b.height};
    D3D11_RECT scissorPass = {95 >> b.mip, 245 >> b.mip, 115U >> b.mip, 253U >> b.mip};
    D3D11_RECT scissorFail = {95 >> b.mip, 245 >> b.mip, 103U >> b.mip, 253U >> b.mip};

    ctx->RSSetViewports(1, &v);
    ctx->RSSetScissorRects(1, &scissor);

    ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(defaultLayout);

    // draw the setup triangles

    pushMarker("Setup");
    {
      setMarker("Depth Write");
      b.depthWriteState.set(ctx);
      RunDraw(PixelHistory::DepthWrite);

      setMarker("Depth Equal Setup");
      RunDraw(PixelHistory::DepthEqualSetup);

      setMarker("Unbound Shader");
      b.noPsState.set(ctx);
      RunDraw(PixelHistory::UnboundPS);

      setMarker("Stencil Write");
      b.stencilWriteState.set(ctx);
      RunDraw(PixelHistory::StencilWrite);

      setMarker("Background");
      b.backgroundState.set(ctx);
      RunDraw(PixelHistory::Background);

      setMarker("Cull Front");
      b.cullFrontState.set(ctx);
      RunDraw(PixelHistory::CullFront);

      // depth bounds would be here but D3D11 does not support it
    }
    popMarker();

    pushMarker("Stress Test");
    {
      pushMarker("Lots of Drawcalls");
      {
        setMarker("300 Draws");
        b.depthWriteState.set(ctx);
        for(int d = 0; d < 300; ++d)
          RunDraw(PixelHistory::Draws300);
      }
      popMarker();

      setMarker("300 Instances");
      RunDraw(PixelHistory::Instances300, 300);
    }
    popMarker();

    setMarker("Simple Test");
    b.basicState.set(ctx);
    RunDraw(PixelHistory::MainTest);

    b.scissorState.set(ctx);

    setMarker("Scissor Fail");
    ctx->RSSetScissorRects(1, &scissorFail);
    RunDraw(PixelHistory::ScissorFail);

    setMarker("Scissor Pass");
    ctx->RSSetScissorRects(1, &scissorPass);
    RunDraw(PixelHistory::ScissorPass);

    ctx->RSSetScissorRects(1, &scissor);

    setMarker("Stencil Ref");
    b.stencilRefState.set(ctx);
    RunDraw(PixelHistory::StencilRef);

    setMarker("Stencil Mask");
    b.stencilMaskState.set(ctx);
    RunDraw(PixelHistory::StencilMask);

    setMarker("Depth Test");
    b.depthState.set(ctx);
    RunDraw(PixelHistory::DepthTest);

    setMarker("Sample Colouring");
    b.sampleColourState.set(ctx);
    RunDraw(PixelHistory::SampleColour);

    setMarker("Depth Equal Fail");
    b.depthEqualState.set(ctx);
    RunDraw(PixelHistory::DepthEqualFail);

    setMarker("Depth Equal Pass");
    b.depthEqualState.set(ctx);
    if(b.depthFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
      RunDraw(PixelHistory::DepthEqualPass24);
    else if(b.depthFormat == DXGI_FORMAT_D16_UNORM)
      RunDraw(PixelHistory::DepthEqualPass16);
    else
      RunDraw(PixelHistory::DepthEqualPass32);

    setMarker("Colour Masked");
    b.colorMaskState.set(ctx);
    RunDraw(PixelHistory::ColourMask);

    setMarker("Overflowing");
    b.backgroundState.set(ctx);
    RunDraw(PixelHistory::OverflowingDraw);

    setMarker("Per-Fragment discarding");
    b.backgroundState.set(ctx);
    RunDraw(PixelHistory::PerFragDiscard);

    if(b.noOutState.VS)
    {
      setMarker("No Output Shader");
      b.noOutState.set(ctx);
      RunDraw(PixelHistory::UnboundPS);
    }
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    PixelHistory::init();

    ID3D11BufferPtr vb = MakeBuffer()
                             .Vertex()
                             .Size(UINT(sizeof(DefaultA2V) * PixelHistory::vb.size()))
                             .Data(PixelHistory::vb.data());

    ID3D11Texture2DPtr bbBlitSource;

    BuildTestBatch("Basic", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    bbBlitSource = batches.back().colImg2D;

    BuildTestBatch("MSAA", 4, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("Colour Only", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    BuildTestBatch("Depth Only", 1, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("Mip & Slice", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                   2, 3);
    BuildTestBatch("Slice MSAA", 4, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
                   -1, 3);

    // can't test 3D textures with depth as 3D depth is not supported and can't mix 3D with 2D array as in other APIs
    BuildTestBatch("3D texture", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, -1, -1, 8);

    BuildTestBatch("D24S8", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);

    BuildTestBatch("D16", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D16_UNORM);
    BuildTestBatch("D32", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    BuildTestBatch("F16 UNORM", 1, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("F16 FLOAT", 1, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("F32 FLOAT", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

    BuildTestBatch("8-bit uint", 1, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("16-bit uint", 1, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("32-bit uint", 1, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

    while(Running())
    {
      for(const D3D11TestBatch &b : batches)
      {
        IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);

        {
          pushMarker("Batch: " + b.name);
          {
            setMarker("Begin RenderPass");

            float clearColor[4] = {0.2f, 0.2f, 0.2f, 1.0f};
            if(b.uint)
            {
              clearColor[0] = 80.0f;
              clearColor[1] = 80.0f;
              clearColor[2] = 80.0f;
              clearColor[3] = 16.0f;
            }

            if(b.RTV)
              ctx->ClearRenderTargetView(b.RTV, clearColor);
            if(b.DSV)
              ctx->ClearDepthStencilView(b.DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            ID3D11RenderTargetViewPtr RTV = b.RTV;
            ctx->OMSetRenderTargets(b.RTV ? 1 : 0, &RTV.GetInterfacePtr(), b.DSV);

            RunBatch(b);

            if(b.comp)
            {
              setMarker("Compute write");

              ctx->OMSetRenderTargets(0, NULL, NULL);
              ctx->CSSetShader(b.comp, NULL, 0);
              ID3D11UnorderedAccessViewPtr UAV = b.compUAV;
              ctx->CSSetUnorderedAccessViews(0, 1, &UAV.GetInterfacePtr(), NULL);
              ctx->Dispatch(1, 1, 1);
            }
          }
          popMarker();
        }
      }

      if(bbBlitSource)
      {
        ctx->OMSetBlendState(NULL, NULL, ~0U);
        blitToSwap(bbBlitSource);
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
