/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2025 Baldur Karlsson
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

RD_TEST(D3D12_Pixel_History, D3D12GraphicsTest)
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
      Output[uint3(STORE_X+x, STORE_Y+y, STORE_Z)] = ProcessColor(data);
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(!opts2.DepthBoundsTestSupported)
      Avail = "Depth Bounds is not supported";
  }

  struct D3D12TestBatch
  {
    std::string name;

    bool uint = false;

    uint32_t mip = 0;

    ID3D12ResourcePtr colImg;
    ID3D12ResourcePtr depthImg;

    ID3D12RootSignaturePtr rootSig;

    D3D12_CPU_DESCRIPTOR_HANDLE RTV = {}, DSV = {};

    DXGI_FORMAT depthFormat;

    ID3D12RootSignaturePtr compRootSig;
    D3D12_GPU_DESCRIPTOR_HANDLE compUAV;
    ID3D12PipelineStatePtr comppipe;

    uint32_t width, height;

    ID3D12PipelineStatePtr depthWritePipe;
    ID3D12PipelineStatePtr scissorPipe;
    ID3D12PipelineStatePtr stencilRefPipe;
    ID3D12PipelineStatePtr stencilMaskPipe;
    ID3D12PipelineStatePtr depthEqualPipe;
    ID3D12PipelineStatePtr depthPipe;
    ID3D12PipelineStatePtr stencilWritePipe;
    ID3D12PipelineStatePtr backgroundPipe;
    ID3D12PipelineStatePtr noPsPipe;
    ID3D12PipelineStatePtr noOutPipe;
    ID3D12PipelineStatePtr basicPipe;
    ID3D12PipelineStatePtr colorMaskPipe;
    ID3D12PipelineStatePtr cullFrontPipe;
    ID3D12PipelineStatePtr depthBoundsPipe;
    ID3D12PipelineStatePtr sampleColourPipe;
  };

  std::vector<D3D12TestBatch> batches;

  void BuildTestBatch(const std::string &name, const std::string &sm_suffix, uint32_t sampleCount,
                      DXGI_FORMAT colourFormat, DXGI_FORMAT depthStencilFormat, int targetMip = -1,
                      int targetSlice = -1, int targetDepthSlice = -1)
  {
    batches.push_back({});

    D3D12TestBatch &batch = batches.back();
    batch.name = name + "(" + sm_suffix + ")";

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

    static uint32_t rtvIndex = 1;    // Start at 1, backbuffer takes id 0
    static uint32_t dsvIndex = 0;
    static uint32_t uavIndex = 1;

    struct DepthBoundsTestStream
    {
      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type0 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
      UINT Padding0;
      ID3D12RootSignature *RootSignature;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type1 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1;
      D3D12_DEPTH_STENCIL_DESC1 DepthStencil;
      UINT Padding1;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type2 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER;
      D3D12_RASTERIZER_DESC Rasterizer;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type3 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
      D3D12_INPUT_LAYOUT_DESC InputLayout;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type4 =
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS;
      D3D12_RT_FORMAT_ARRAY RTVFormats;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type5 =
          D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT;
      DXGI_FORMAT DSVFormat;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type6 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS;
      UINT Padding6;
      D3D12_SHADER_BYTECODE VS;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type7 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS;
      UINT Padding7;
      D3D12_SHADER_BYTECODE PS;

      D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type8 = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC;
      DXGI_SAMPLE_DESC SampleDesc;
    } depthBoundsTestStream;

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
    defines +=
        "#define IMAGE_TYPE " + std::string(depth > 1 ? "RWTexture3D" : "RWTexture2DArray") + "\n";
    defines += "#define STORE_X " + std::to_string(220 >> mip) + "\n";
    defines += "#define STORE_Y " + std::to_string(80 >> mip) + "\n";
    defines += "#define STORE_Z " + std::to_string(slice) + "\n";
    defines += "\n\n";

    ID3DBlobPtr vsBlob = Compile(defines + vertex, "main", ("vs_" + sm_suffix).c_str());
    ID3DBlobPtr psBlob = Compile(defines + pixel, "main", ("ps_" + sm_suffix).c_str());
    ID3DBlobPtr psMsaaBlob = Compile(defines + mspixel, "main", ("ps_" + sm_suffix).c_str());

    ID3DBlobPtr nooutpsBlob;

    if(colourFormat != DXGI_FORMAT_UNKNOWN)
    {
      defines += "#undef COLOR\n#define COLOR 0\n";
      nooutpsBlob = Compile(defines + pixel, "main", ("ps_" + sm_suffix).c_str());

      D3D12TextureCreator colCreator =
          MakeTexture(colourFormat, batch.width, batch.height, depth)
              .Mips(mips)
              // this is DepthOrArraySize so need to set it correctly from either
              .Array(std::max(depth, arraySize))
              .Multisampled(sampleCount)
              .InitialState(D3D12_RESOURCE_STATE_COMMON)
              .RTV();

      if(sampleCount == 1)
        colCreator.UAV();

      batch.colImg = colCreator;
      setName(batch.colImg, batch.name + " colImg");

      batch.RTV =
          MakeRTV(batch.colImg).FirstMip(mip).NumMips(1).FirstSlice(slice).NumSlices(1).CreateCPU(rtvIndex++);

      if(sampleCount == 1)
      {
        ID3DBlobPtr compBlob = Compile(defines + comp, "main", ("cs_" + sm_suffix).c_str());
        batch.compRootSig = MakeSig({
            tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1, 0),
        });
        batch.comppipe = MakePSO().RootSig(batch.compRootSig).CS(compBlob);

        batch.compUAV = MakeUAV(batch.colImg).FirstMip(mip).NumMips(1).CreateGPU(uavIndex++);
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

      batch.depthImg = MakeTexture(depthStencilFormat, batch.width, batch.height, depth)
                           .Mips(mips)
                           // this is DepthOrArraySize so need to set it correctly from either
                           .Array(std::max(depth, arraySize))
                           .Multisampled(sampleCount)
                           .InitialState(D3D12_RESOURCE_STATE_COMMON)
                           .DSV();
      setName(batch.depthImg, batch.name + " depthImg");

      batch.DSV = MakeDSV(batch.depthImg)
                      .FirstMip(mip)
                      .NumMips(1)
                      .FirstSlice(slice)
                      .NumSlices(1)
                      .CreateCPU(dsvIndex++);
    }

    batch.width >>= mip;
    batch.height >>= mip;

    batch.rootSig = MakeSig({});

    D3D12PSOCreator pipeInfo = MakePSO()
                                   .RootSig(batch.rootSig)
                                   .InputLayout()
                                   .DSV(depthStencilFormat)
                                   .SampleCount(sampleCount)
                                   .VS(vsBlob)
                                   .PS(psBlob);

    if(colourFormat == DXGI_FORMAT_UNKNOWN)
      pipeInfo.RTVs({});
    else
      pipeInfo.RTVs({colourFormat});

    pipeInfo.GraphicsDesc.RasterizerState.DepthClipEnable = TRUE;
    pipeInfo.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

    pipeInfo.GraphicsDesc.DepthStencilState.DepthEnable = TRUE;
    pipeInfo.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeInfo.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
    pipeInfo.GraphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeInfo.GraphicsDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    pipeInfo.GraphicsDesc.DepthStencilState.StencilReadMask = 0xff;
    pipeInfo.GraphicsDesc.DepthStencilState.StencilWriteMask = 0xff;
    pipeInfo.GraphicsDesc.DepthStencilState.BackFace =
        pipeInfo.GraphicsDesc.DepthStencilState.FrontFace;

    {
      const D3D12_DEPTH_STENCIL_DESC &depthState = pipeInfo.GraphicsDesc.DepthStencilState;
      depthBoundsTestStream.RootSignature = pipeInfo.GraphicsDesc.pRootSignature;
      depthBoundsTestStream.DepthStencil.DepthEnable = depthState.DepthEnable;
      depthBoundsTestStream.DepthStencil.DepthWriteMask = depthState.DepthWriteMask;
      depthBoundsTestStream.DepthStencil.DepthFunc = depthState.DepthFunc;
      depthBoundsTestStream.DepthStencil.StencilEnable = depthState.StencilEnable;
      depthBoundsTestStream.DepthStencil.StencilReadMask = depthState.StencilReadMask;
      depthBoundsTestStream.DepthStencil.StencilWriteMask = depthState.StencilWriteMask;
      depthBoundsTestStream.DepthStencil.FrontFace = depthState.FrontFace;
      depthBoundsTestStream.DepthStencil.BackFace = depthState.BackFace;
      depthBoundsTestStream.DepthStencil.DepthBoundsTestEnable = TRUE;

      depthBoundsTestStream.SampleDesc = pipeInfo.GraphicsDesc.SampleDesc;

      depthBoundsTestStream.Rasterizer = pipeInfo.GraphicsDesc.RasterizerState;
      depthBoundsTestStream.InputLayout = pipeInfo.GraphicsDesc.InputLayout;
      depthBoundsTestStream.RTVFormats.NumRenderTargets = pipeInfo.GraphicsDesc.NumRenderTargets;
      for(int j = 0; j < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++j)
        depthBoundsTestStream.RTVFormats.RTFormats[j] = pipeInfo.GraphicsDesc.RTVFormats[j];
      depthBoundsTestStream.DSVFormat = pipeInfo.GraphicsDesc.DSVFormat;

      depthBoundsTestStream.VS = pipeInfo.GraphicsDesc.VS;
      depthBoundsTestStream.PS = pipeInfo.GraphicsDesc.PS;
    }
    D3D12_PIPELINE_STATE_STREAM_DESC depthBoundsTestStreamDesc = {};
    depthBoundsTestStreamDesc.SizeInBytes = sizeof(depthBoundsTestStream);
    depthBoundsTestStreamDesc.pPipelineStateSubobjectStream = &depthBoundsTestStream;

    pipeInfo.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    batch.depthWritePipe = pipeInfo;
    setName(batch.depthWritePipe, batch.name + " depthWritePipe");

    {
      D3D12PSOCreator depthEqualPipeInfo = pipeInfo;
      depthEqualPipeInfo.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      depthEqualPipeInfo.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

      batch.depthEqualPipe = depthEqualPipeInfo;
      setName(batch.depthEqualPipe, batch.name + " depthEqualPipe");
    }

    {
      D3D12PSOCreator scissorStencilPipes = pipeInfo;
      scissorStencilPipes.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      scissorStencilPipes.GraphicsDesc.DepthStencilState.DepthEnable = FALSE;

      batch.scissorPipe = scissorStencilPipes;
      setName(batch.scissorPipe, batch.name + " scissorPipe");

      batch.stencilRefPipe = scissorStencilPipes;
      setName(batch.stencilRefPipe, batch.name + " stencilRefPipe");

      scissorStencilPipes.GraphicsDesc.DepthStencilState.StencilReadMask = 0;
      scissorStencilPipes.GraphicsDesc.DepthStencilState.StencilWriteMask = 0;

      batch.stencilMaskPipe = scissorStencilPipes;
      setName(batch.stencilMaskPipe, batch.name + " dynamicStencilMaskPipe");
    }

    pipeInfo.GraphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    depthBoundsTestStream.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    CHECK_HR(dev2->CreatePipelineState(&depthBoundsTestStreamDesc, __uuidof(ID3D12PipelineState),
                                       (void **)&batch.depthPipe));
    setName(batch.depthPipe, batch.name + " depthPipe");

    pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;
    batch.stencilWritePipe = pipeInfo;
    setName(batch.stencilWritePipe, batch.name + " stencilWritePipe");

    pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;
    batch.backgroundPipe = pipeInfo;
    setName(batch.backgroundPipe, batch.name + " backgroundPipe");

    {
      D3D12_SHADER_BYTECODE PS = pipeInfo.GraphicsDesc.PS;
      pipeInfo.GraphicsDesc.PS = {};
      pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;
      batch.noPsPipe = pipeInfo;
      setName(batch.noPsPipe, batch.name + " noPsPipe");
      pipeInfo.GraphicsDesc.PS = PS;
    }

    if(nooutpsBlob)
    {
      D3D12_SHADER_BYTECODE PS = pipeInfo.GraphicsDesc.PS;
      pipeInfo.PS(nooutpsBlob);
      pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;
      batch.noOutPipe = pipeInfo;
      setName(batch.noOutPipe, batch.name + " noOutPipe");
      pipeInfo.GraphicsDesc.PS = PS;
    }

    pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = TRUE;
    pipeInfo.GraphicsDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_GREATER;
    batch.basicPipe = pipeInfo;
    setName(batch.basicPipe, batch.name + " basicPipe");
    pipeInfo.GraphicsDesc.DepthStencilState.StencilEnable = FALSE;

    {
      D3D12PSOCreator maskPipeInfo = pipeInfo;
      maskPipeInfo.GraphicsDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0x3;
      batch.colorMaskPipe = maskPipeInfo;
      setName(batch.colorMaskPipe, batch.name + " colorMaskPipe");
    }

    pipeInfo.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    batch.cullFrontPipe = pipeInfo;
    setName(batch.cullFrontPipe, batch.name + " cullFrontPipe");
    pipeInfo.GraphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

    depthBoundsTestStream.DepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    CHECK_HR(dev2->CreatePipelineState(&depthBoundsTestStreamDesc, __uuidof(ID3D12PipelineState),
                                       (void **)&batch.depthBoundsPipe));
    setName(batch.depthBoundsPipe, batch.name + " depthBoundsPipe");

    pipeInfo.PS(psMsaaBlob);

    pipeInfo.GraphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    if(sampleCount == 4)
      pipeInfo.GraphicsDesc.SampleMask = 0x7;
    batch.sampleColourPipe = pipeInfo;
    setName(batch.sampleColourPipe, batch.name + " sampleColourPipe");
  }

  void RunDraw(ID3D12GraphicsCommandList1Ptr cmd, const PixelHistory::draw &draw,
               uint32_t numInstances = 1)
  {
    cmd->DrawInstanced(draw.count, numInstances, draw.first, 0);
  }

  void RunBatch(const D3D12TestBatch &b, ID3D12GraphicsCommandList1Ptr cmd)
  {
    float factor = float(b.width) / float(screenWidth);

    D3D12_VIEWPORT v = {};
    v.Width = (float)b.width - (20.0f * factor);
    v.Height = (float)b.height - (20.0f * factor);
    v.TopLeftX = floorf(10.0f * factor);
    v.TopLeftY = floorf(10.0f * factor);
    v.MaxDepth = 1.0f;

    D3D12_RECT scissor = {0, 0, (LONG)b.width, (LONG)b.height};
    D3D12_RECT scissorPass = {95 >> b.mip, 245 >> b.mip, 115U >> b.mip, 253U >> b.mip};
    D3D12_RECT scissorFail = {95 >> b.mip, 245 >> b.mip, 103U >> b.mip, 253U >> b.mip};

    cmd->RSSetViewports(1, &v);
    cmd->RSSetScissorRects(1, &scissor);

    cmd->OMSetStencilRef(0x55);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmd->SetGraphicsRootSignature(b.rootSig);

    // draw the setup triangles

    pushMarker(cmd, "Setup");
    {
      setMarker(cmd, "Depth Write");
      cmd->SetPipelineState(b.depthWritePipe);
      RunDraw(cmd, PixelHistory::DepthWrite);

      setMarker(cmd, "Depth Equal Setup");
      RunDraw(cmd, PixelHistory::DepthEqualSetup);

      cmd->OMSetStencilRef(0x33);

      setMarker(cmd, "Unbound Shader");
      cmd->SetPipelineState(b.noPsPipe);
      RunDraw(cmd, PixelHistory::UnboundPS);

      cmd->OMSetStencilRef(0x55);

      setMarker(cmd, "Stencil Write");
      cmd->SetPipelineState(b.stencilWritePipe);
      RunDraw(cmd, PixelHistory::StencilWrite);

      setMarker(cmd, "Background");
      cmd->SetPipelineState(b.backgroundPipe);
      RunDraw(cmd, PixelHistory::Background);

      setMarker(cmd, "Cull Front");
      cmd->SetPipelineState(b.cullFrontPipe);
      RunDraw(cmd, PixelHistory::CullFront);

      setMarker(cmd, "Depth Bounds Prep");
      cmd->SetPipelineState(b.depthBoundsPipe);
      cmd->OMSetDepthBounds(0.0f, 1.0f);
      RunDraw(cmd, PixelHistory::DepthBoundsPrep);
      setMarker(cmd, "Depth Bounds Clip");
      cmd->OMSetDepthBounds(0.4f, 0.6f);
      RunDraw(cmd, PixelHistory::DepthBoundsClip);
    }
    popMarker(cmd);

    pushMarker(cmd, "Stress Test");
    {
      pushMarker(cmd, "Lots of Drawcalls");
      {
        setMarker(cmd, "300 Draws");
        cmd->SetPipelineState(b.depthWritePipe);
        for(int d = 0; d < 300; ++d)
          RunDraw(cmd, PixelHistory::Draws300);
      }
      popMarker(cmd);

      setMarker(cmd, "300 Instances");
      RunDraw(cmd, PixelHistory::Instances300, 300);
    }
    popMarker(cmd);

    setMarker(cmd, "Simple Test");
    cmd->SetPipelineState(b.basicPipe);
    RunDraw(cmd, PixelHistory::MainTest);

    cmd->SetPipelineState(b.scissorPipe);

    setMarker(cmd, "Scissor Fail");
    cmd->RSSetScissorRects(1, &scissorFail);
    RunDraw(cmd, PixelHistory::ScissorFail);

    setMarker(cmd, "Scissor Pass");
    cmd->RSSetScissorRects(1, &scissorPass);
    RunDraw(cmd, PixelHistory::ScissorPass);

    cmd->RSSetScissorRects(1, &scissor);

    setMarker(cmd, "Stencil Ref");
    cmd->SetPipelineState(b.stencilRefPipe);
    cmd->OMSetStencilRef(0x67);
    RunDraw(cmd, PixelHistory::StencilRef);

    cmd->OMSetStencilRef(0x55);

    setMarker(cmd, "Stencil Mask");
    cmd->SetPipelineState(b.stencilMaskPipe);
    RunDraw(cmd, PixelHistory::StencilMask);

    setMarker(cmd, "Depth Test");
    cmd->SetPipelineState(b.depthPipe);
    cmd->OMSetDepthBounds(0.15f, 1.0f);
    RunDraw(cmd, PixelHistory::DepthTest);

    setMarker(cmd, "Sample Colouring");
    cmd->SetPipelineState(b.sampleColourPipe);
    RunDraw(cmd, PixelHistory::SampleColour);

    setMarker(cmd, "Depth Equal Fail");
    cmd->SetPipelineState(b.depthEqualPipe);
    RunDraw(cmd, PixelHistory::DepthEqualFail);

    setMarker(cmd, "Depth Equal Pass");
    cmd->SetPipelineState(b.depthEqualPipe);
    if(b.depthFormat == DXGI_FORMAT_D24_UNORM_S8_UINT)
      RunDraw(cmd, PixelHistory::DepthEqualPass24);
    else if(b.depthFormat == DXGI_FORMAT_D16_UNORM)
      RunDraw(cmd, PixelHistory::DepthEqualPass16);
    else
      RunDraw(cmd, PixelHistory::DepthEqualPass32);

    setMarker(cmd, "Colour Masked");
    cmd->SetPipelineState(b.colorMaskPipe);
    RunDraw(cmd, PixelHistory::ColourMask);

    setMarker(cmd, "Overflowing");
    cmd->SetPipelineState(b.backgroundPipe);
    RunDraw(cmd, PixelHistory::OverflowingDraw);

    setMarker(cmd, "Per-Fragment discarding");
    cmd->SetPipelineState(b.backgroundPipe);
    RunDraw(cmd, PixelHistory::PerFragDiscard);

    if(b.noOutPipe)
    {
      setMarker(cmd, "No Output Shader");
      cmd->SetPipelineState(b.noOutPipe);
      RunDraw(cmd, PixelHistory::UnboundPS);
    }
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    PixelHistory::init();

    ID3D12ResourcePtr vb = MakeBuffer()
                               .Size(UINT(sizeof(DefaultA2V) * PixelHistory::vb.size()))
                               .Data(PixelHistory::vb.data());

    ID3D12ResourcePtr bbBlitSource;

    BuildTestBatch("Basic", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    bbBlitSource = batches.back().colImg;

    if(m_DXILSupport)
    {
      BuildTestBatch("Basic", "6_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

      if(m_HighestShaderModel >= D3D_SHADER_MODEL_6_6)
        BuildTestBatch("Basic", "6_6", 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                       DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    }

    BuildTestBatch("MSAA", "5_0", 4, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("Colour Only", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    BuildTestBatch("Depth Only", "5_0", 1, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("Mip & Slice", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 2, 3);
    BuildTestBatch("Slice MSAA", "5_0", 4, DXGI_FORMAT_R8G8B8A8_UNORM,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT, -1, 3);

    // with new features we can have depth in this test, via a 2D array textures. Otherwise we need
    // to test without depth
    if(opts19.MismatchingOutputDimensionsSupported)
      BuildTestBatch("3D texture", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                     DXGI_FORMAT_D32_FLOAT_S8X24_UINT, -1, -1, 8);
    else
      BuildTestBatch("3D texture", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, -1,
                     -1, 8);

    BuildTestBatch("D24S8", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);

    BuildTestBatch("D16", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D16_UNORM);
    BuildTestBatch("D32", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

    BuildTestBatch("F16 UNORM", "5_0", 1, DXGI_FORMAT_R16G16B16A16_UNORM,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("F16 FLOAT", "5_0", 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("F32 FLOAT", "5_0", 1, DXGI_FORMAT_R32G32B32A32_FLOAT,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

    BuildTestBatch("8-bit uint", "5_0", 1, DXGI_FORMAT_R8G8B8A8_UINT,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("16-bit uint", "5_0", 1, DXGI_FORMAT_R16G16B16A16_UINT,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    BuildTestBatch("32-bit uint", "5_0", 1, DXGI_FORMAT_R32G32B32A32_UINT,
                   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

    while(Running())
    {
      std::vector<ID3D12GraphicsCommandListPtr> cmds;

      for(const D3D12TestBatch &b : batches)
      {
        ID3D12GraphicsCommandList1Ptr cmd = GetCommandBuffer();
        Reset(cmd);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);

        {
          pushMarker(cmd, "Batch: " + b.name);
          {
            setMarker(cmd, "Begin RenderPass");

            float clearColor[4] = {0.2f, 0.2f, 0.2f, 1.0f};
            if(b.uint)
            {
              clearColor[0] = 80.0f;
              clearColor[1] = 80.0f;
              clearColor[2] = 80.0f;
              clearColor[3] = 16.0f;
            }

            if(b.RTV.ptr)
              cmd->ClearRenderTargetView(b.RTV, clearColor, 0, NULL);
            if(b.DSV.ptr)
              cmd->ClearDepthStencilView(b.DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                         1.0f, 0, 0, NULL);

            cmd->OMSetRenderTargets(b.RTV.ptr ? 1 : 0, &b.RTV, FALSE, b.DSV.ptr ? &b.DSV : NULL);

            RunBatch(b, cmd);

            if(b.depthImg)
              ResourceBarrier(cmd, b.depthImg, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                              D3D12_RESOURCE_STATE_COMMON);

            if(b.comppipe)
            {
              setMarker(cmd, "Compute write");
              if(b.colImg)
                ResourceBarrier(cmd, b.colImg, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

              cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
              cmd->SetComputeRootSignature(b.compRootSig);
              cmd->SetPipelineState(b.comppipe);
              cmd->SetComputeRootDescriptorTable(0, b.compUAV);
              cmd->Dispatch(1, 1, 1);

              if(b.colImg)
                ResourceBarrier(cmd, b.colImg, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                D3D12_RESOURCE_STATE_COMMON);
            }
            else
            {
              if(b.colImg)
                ResourceBarrier(cmd, b.colImg, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                D3D12_RESOURCE_STATE_COMMON);
            }
          }
          popMarker(cmd);
        }

        cmd->Close();
        {
          ID3D12GraphicsCommandList1Ptr barrierCmd = GetCommandBuffer();
          Reset(barrierCmd);
          if(b.depthImg)
            ResourceBarrier(barrierCmd, b.depthImg, D3D12_RESOURCE_STATE_COMMON,
                            D3D12_RESOURCE_STATE_DEPTH_WRITE);
          if(b.colImg)
            ResourceBarrier(barrierCmd, b.colImg, D3D12_RESOURCE_STATE_COMMON,
                            D3D12_RESOURCE_STATE_RENDER_TARGET);
          barrierCmd->Close();
          cmds.push_back(barrierCmd);
        }

        cmds.push_back(cmd);
      }

      {
        ID3D12GraphicsCommandList1Ptr cmd = GetCommandBuffer();
        Reset(cmd);

        ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        if(bbBlitSource != NULL)
        {
          ResourceBarrier(cmd, bbBlitSource, D3D12_RESOURCE_STATE_COMMON,
                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

          blitToSwap(cmd, bbBlitSource, bb, DXGI_FORMAT_R8G8B8A8_UNORM);

          ResourceBarrier(cmd, bbBlitSource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                          D3D12_RESOURCE_STATE_COMMON);
        }

        FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmd->Close();
        cmds.push_back(cmd);
      }

      Submit(cmds);
      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
