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

#include <stdio.h>
#include "gl_test.h"

RD_TEST(GL_Pixel_History, OpenGLGraphicsTest)
{
  static constexpr const char *Description = "Tests pixel history";

  std::string common = R"EOSHADER(

#version 460 core

)EOSHADER";

  const std::string vertex = R"EOSHADER(

out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
};

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;

layout(location = 0) INTERP out COL_TYPE outCol;

void main()
{
	gl_Position = vec4(Position.xyz, 1);
	outCol = ProcessColor(Color);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) INTERP in COL_TYPE inCol;

#if COLOR == 1
layout(location = 0, index = 0) out COL_TYPE Color;
#endif

void main()
{
  if (gl_FragCoord.x < DISCARD_X+1 && gl_FragCoord.x > DISCARD_X)
    discard;
  if (inCol.x > 10000 && inCol.y > 10000 && inCol.z > 10000)
    discard;
#if COLOR == 1
	Color = inCol + COL_TYPE(0, 0, 0, ProcessColor(vec4(ALPHA_ADD)).x);
#endif
}

)EOSHADER";

  std::string mspixel = R"EOSHADER(

#if COLOR == 1
layout(location = 0, index = 0) out COL_TYPE Color;
#endif

void main()
{
#if COLOR == 1
  vec4 col;
  if (gl_SampleID == 0)
    col = vec4(1, 0, 0, 1+ALPHA_ADD);
  else if (gl_SampleID == 1)
    col = vec4(0, 0, 1, 1+ALPHA_ADD);
  else if (gl_SampleID == 2)
    col = vec4(0, 1, 1, 1+ALPHA_ADD);
  else if (gl_SampleID == 3)
    col = vec4(1, 1, 1, 1+ALPHA_ADD);
  Color = ProcessColor(col);
#endif
}

)EOSHADER";

  std::string comp = R"EOSHADER(

layout(binding = 0) writeonly uniform IMAGE_TYPE Output;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
  vec4 data = vec4(3, 3, 3, 9);

  for(int x=0; x < 10; x++)
    for(int y=0; y < 10; y++)
      imageStore(Output, ivec3(STORE_X+x, STORE_Y+y, STORE_Z) STORE_SAMPLE, ProcessColor(data));
}

)EOSHADER";

  void Prepare(int argc, char **argv)
  {
    OpenGLGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(!GL_ARB_clip_control)
      Avail = "Require GL_ARB_clip_control";

    if(!GL_EXT_depth_bounds_test)
      Avail = "Require GL_EXT_depth_bounds_test";
  }

  struct GLStateSetup
  {
    // only one will be set
    GLuint program;
    GLuint pipeline;

    bool depthTest;
    bool depthWrites;
    GLenum depthFunc;
    bool stencilTest;
    bool depthBounds;
    GLenum stencilFunc;
    uint32_t stencilMask;
    GLenum cullMode;
    uint32_t stencilRef;
    GLuint sampleMask;
    GLuint colorMask;

    void set() const
    {
      glSampleMaski(0, sampleMask);
      glUseProgram(program);
      glBindProgramPipeline(pipeline);
      if(depthTest)
        glEnable(GL_DEPTH_TEST);
      else
        glDisable(GL_DEPTH_TEST);
      if(depthBounds)
        glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
      else
        glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
      glDepthMask(depthWrites ? GL_TRUE : GL_FALSE);
      glDepthFunc(depthFunc);
      if(stencilTest)
        glEnable(GL_STENCIL_TEST);
      else
        glDisable(GL_STENCIL_TEST);
      glStencilFuncSeparate(GL_FRONT, stencilFunc, stencilRef, stencilMask);
      glCullFace(cullMode);
      glColorMask(colorMask & 0x1, colorMask & 0x2, colorMask & 0x4, colorMask & 0x8);
    }
  };

  struct GLTestBatch
  {
    std::string name;

    bool uint = false;

    uint32_t mip = 0;

    GLenum colFmt;
    GLuint colImg2D;

    GLuint framebuffer;

    GLenum depthFormat;

    GLuint compProg;

    uint32_t width, height;

    GLStateSetup depthWriteState;
    GLStateSetup scissorState;
    GLStateSetup stencilRefState;
    GLStateSetup stencilMaskState;
    GLStateSetup depthEqualState;
    GLStateSetup depthState;
    GLStateSetup stencilWriteState;
    GLStateSetup backgroundState;
    GLStateSetup noPsState;
    GLStateSetup basicState;
    GLStateSetup colorMaskState;
    GLStateSetup cullFrontState;
    GLStateSetup depthBoundsState;
    GLStateSetup sampleColourState;
  };

  std::vector<GLTestBatch> batches;

  void BuildTestBatch(const std::string &name, bool pipeline, uint32_t sampleCount,
                      GLenum colourFormat, GLenum depthStencilFormat, int targetMip = -1,
                      int targetSlice = -1, int targetDepthSlice = -1)
  {
    batches.push_back({});

    GLTestBatch &batch = batches.back();
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
    defines += "#define COLOR " + std::string(colourFormat == GL_NONE ? "0" : "1") + "\n";

    std::string imgprefix;
    if(colourFormat == GL_RGBA8UI || colourFormat == GL_RGBA16UI || colourFormat == GL_RGBA32UI)
    {
      batch.uint = true;
      imgprefix = "u";
      defines += R"(

#define COL_TYPE uvec4
#define ALPHA_ADD 3
#define INTERP flat

uvec4 ProcessColor(vec4 col)
{
  uvec4 ret = uvec4(16*col);

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

#define COL_TYPE vec4
#define ALPHA_ADD 1.75
#define INTERP 

vec4 ProcessColor(vec4 col)
{
  vec4 ret = col;

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
               std::string(depth > 1          ? imgprefix + "image3D"
                           : sampleCount != 1 ? imgprefix + "image2DMSArray"
                                              : imgprefix + "image2DArray") +
               "\n";
    defines += "#define STORE_X " + std::to_string(220 >> mip) + "\n";
    defines += "#define STORE_Y " + std::to_string(210 >> mip) + "\n";
    defines += "#define STORE_Z " + std::to_string(slice) + "\n";
    defines += "#define STORE_SAMPLE " + std::string(sampleCount != 1 ? ", 0" : "") + "\n";
    defines += "\n\n";

    batch.framebuffer = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, batch.framebuffer);

    if(colourFormat != GL_NONE)
    {
      GLuint colTex = MakeTexture();
      if(sampleCount > 1)
      {
        if(arraySize > 1)
        {
          glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, colTex);
          glTextureStorage3DMultisample(colTex, sampleCount, colourFormat, batch.width,
                                        batch.height, arraySize, GL_TRUE);
        }
        else
        {
          glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colTex);
          glTextureStorage2DMultisample(colTex, sampleCount, colourFormat, batch.width,
                                        batch.height, GL_TRUE);
        }
      }
      else if(depth > 1)
      {
        glBindTexture(GL_TEXTURE_3D, colTex);
        glTextureStorage3D(colTex, mips, colourFormat, batch.width, batch.height, depth);
      }
      else if(arraySize > 1)
      {
        glBindTexture(GL_TEXTURE_2D_ARRAY, colTex);
        glTextureStorage3D(colTex, mips, colourFormat, batch.width, batch.height, arraySize);

        batch.colImg2D = colTex;
      }
      else
      {
        glBindTexture(GL_TEXTURE_2D, colTex);
        glTextureStorage2D(colTex, mips, colourFormat, batch.width, batch.height);

        batch.colImg2D = colTex;
      }

      if(arraySize > 1 || depth > 1)
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colTex, mip, slice);
      else
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colTex, mip);

      batch.compProg = MakeProgram(common + defines + comp);
      batch.colFmt = colourFormat;
    }

    batch.depthFormat = depthStencilFormat;

    if(depthStencilFormat != GL_NONE)
    {
      GLuint depthTex = MakeTexture();

      if(sampleCount > 1)
      {
        if(arraySize > 1)
        {
          glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, depthTex);
          glTextureStorage3DMultisample(depthTex, sampleCount, depthStencilFormat, batch.width,
                                        batch.height, arraySize, GL_TRUE);
        }
        else
        {
          glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, depthTex);
          glTextureStorage2DMultisample(depthTex, sampleCount, depthStencilFormat, batch.width,
                                        batch.height, GL_TRUE);
        }
      }
      else if(depth > 1 || arraySize > 1)
      {
        // can't create 2D views of 3D with depth images, so we just use a normal 2D array image
        glBindTexture(GL_TEXTURE_2D_ARRAY, depthTex);
        glTextureStorage3D(depthTex, mips, depthStencilFormat, batch.width, batch.height,
                           std::max(depth, arraySize));
      }
      else
      {
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTextureStorage2D(depthTex, mips, depthStencilFormat, batch.width, batch.height);
      }

      GLenum att = GL_DEPTH_STENCIL_ATTACHMENT;

      if(depthStencilFormat == GL_DEPTH_COMPONENT16 || depthStencilFormat == GL_DEPTH_COMPONENT32F)
        att = GL_DEPTH_ATTACHMENT;

      if(arraySize > 1 || depth > 1)
        glFramebufferTextureLayer(GL_FRAMEBUFFER, att, depthTex, mip, slice);
      else
        glFramebufferTexture(GL_FRAMEBUFFER, att, depthTex, mip);
    }

    batch.width >>= mip;
    batch.height >>= mip;

    GLStateSetup stateInfo = {};

    if(pipeline)
    {
      stateInfo.pipeline = MakePipeline();
      glUseProgramStages(stateInfo.pipeline, GL_VERTEX_SHADER_BIT,
                         MakePipelineProgram(GL_VERTEX_SHADER, common + defines + vertex));
      glUseProgramStages(stateInfo.pipeline, GL_FRAGMENT_SHADER_BIT,
                         MakePipelineProgram(GL_FRAGMENT_SHADER, common + defines + pixel));
    }
    else
    {
      stateInfo.program = MakeProgram(common + defines + vertex, common + defines + pixel);
    }

    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_SAMPLE_MASK);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_KEEP, GL_REPLACE);
    // this isn't a full test because almost all GL applications will be -1 to 1 but we hope/assume
    // that our code isn't sensitive to that and just treats depth as whatever it comes out as.
    // Using zero to one means all our testing code can be unified
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

    stateInfo.depthTest = true;
    stateInfo.depthWrites = true;
    stateInfo.depthFunc = GL_ALWAYS;
    stateInfo.stencilTest = false;
    stateInfo.stencilFunc = GL_ALWAYS;
    stateInfo.stencilMask = 0xff;
    stateInfo.cullMode = GL_BACK;
    stateInfo.stencilRef = 0x55;
    stateInfo.sampleMask = ~0U;
    stateInfo.colorMask = 0xf;
    stateInfo.depthBounds = false;

    batch.depthWriteState = stateInfo;

    {
      GLStateSetup depthEqualStateInfo = stateInfo;
      depthEqualStateInfo.depthWrites = GL_FALSE;
      depthEqualStateInfo.depthFunc = GL_EQUAL;

      batch.depthEqualState = depthEqualStateInfo;
    }

    {
      GLStateSetup scissorStencilStates = stateInfo;
      scissorStencilStates.depthWrites = GL_FALSE;
      scissorStencilStates.depthTest = GL_FALSE;

      batch.scissorState = scissorStencilStates;

      batch.stencilRefState = scissorStencilStates;
      batch.stencilRefState.stencilRef = 0x67;

      scissorStencilStates.stencilMask = 0;

      batch.stencilMaskState = scissorStencilStates;
    }

    stateInfo.depthFunc = GL_LEQUAL;
    stateInfo.depthBounds = true;
    batch.depthState = stateInfo;
    stateInfo.depthBounds = false;

    stateInfo.stencilTest = GL_TRUE;
    batch.stencilWriteState = stateInfo;

    stateInfo.stencilTest = GL_FALSE;
    batch.backgroundState = stateInfo;

    stateInfo.stencilTest = GL_TRUE;

    batch.noPsState = stateInfo;
    batch.noPsState.stencilRef = 0x33;
    if(pipeline)
    {
      batch.noPsState.pipeline = MakePipeline();
      glUseProgramStages(batch.noPsState.pipeline, GL_VERTEX_SHADER_BIT,
                         MakePipelineProgram(GL_VERTEX_SHADER, common + defines + vertex));
    }
    else
    {
      batch.noPsState.program = MakeProgram(common + defines + vertex, "");
    }

    stateInfo.stencilFunc = GL_GREATER;
    batch.basicState = stateInfo;
    stateInfo.stencilTest = GL_FALSE;

    {
      GLStateSetup maskStateInfo = stateInfo;
      maskStateInfo.colorMask = 0x3;
      batch.colorMaskState = maskStateInfo;
    }

    stateInfo.cullMode = GL_FRONT;
    batch.cullFrontState = stateInfo;
    stateInfo.cullMode = GL_BACK;

    stateInfo.depthBounds = true;
    batch.depthBoundsState = stateInfo;
    stateInfo.depthBounds = false;

    if(pipeline)
    {
      stateInfo.pipeline = MakePipeline();
      glUseProgramStages(stateInfo.pipeline, GL_VERTEX_SHADER_BIT,
                         MakePipelineProgram(GL_VERTEX_SHADER, common + defines + vertex));
      glUseProgramStages(stateInfo.pipeline, GL_FRAGMENT_SHADER_BIT,
                         MakePipelineProgram(GL_FRAGMENT_SHADER, common + defines + mspixel));
    }
    else
    {
      stateInfo.program = MakeProgram(common + defines + vertex, common + defines + mspixel);
    }

    stateInfo.depthWrites = GL_TRUE;
    batch.sampleColourState = stateInfo;
    if(sampleCount == 4)
      batch.sampleColourState.sampleMask = 0x7;
  }

  void RunDraw(const PixelHistory::draw &draw, uint32_t numInstances = 1)
  {
    glDrawArraysInstanced(GL_TRIANGLES, draw.first, draw.count, numInstances);
  }

  void RunBatch(const GLTestBatch &b)
  {
    float factor = float(b.width) / float(screenWidth);

    // use ceil instead of floor on y to better match the other APIs with top left origin
    glViewportIndexedf(0, floorf(10.0f * factor), ceilf(10.0f * factor),
                       (float)b.width - (20.0f * factor), (float)b.height - (20.0f * factor));
    glScissor(0, 0, b.width, b.height);

    // draw the setup triangles

    pushMarker("Setup");
    {
      setMarker("Depth Write");
      b.depthWriteState.set();
      RunDraw(PixelHistory::DepthWrite);

      setMarker("Depth Equal Setup");
      RunDraw(PixelHistory::DepthEqualSetup);

      setMarker("Unbound Shader");
      b.noPsState.set();
      RunDraw(PixelHistory::UnboundPS);

      setMarker("Stencil Write");
      b.stencilWriteState.set();
      RunDraw(PixelHistory::StencilWrite);

      setMarker("Background");
      b.backgroundState.set();
      RunDraw(PixelHistory::Background);

      setMarker("Cull Front");
      b.cullFrontState.set();
      RunDraw(PixelHistory::CullFront);

      setMarker("Depth Bounds Prep");
      b.depthBoundsState.set();
      glDepthBoundsEXT(0.0f, 1.0f);
      RunDraw(PixelHistory::DepthBoundsPrep);
      setMarker("Depth Bounds Clip");
      glDepthBoundsEXT(0.4f, 0.6f);
      RunDraw(PixelHistory::DepthBoundsClip);
    }
    popMarker();

    pushMarker("Stress Test");
    {
      pushMarker("Lots of Drawcalls");
      {
        setMarker("300 Draws");
        b.depthWriteState.set();
        for(int d = 0; d < 300; ++d)
          RunDraw(PixelHistory::Draws300);
      }
      popMarker();

      setMarker("300 Instances");
      RunDraw(PixelHistory::Instances300, 300);
    }
    popMarker();

    setMarker("Simple Test");
    b.basicState.set();
    RunDraw(PixelHistory::MainTest);

    b.scissorState.set();

    setMarker("Scissor Fail");
    // set up scissors anchored top left as we expect in the tests
    glScissor(95 >> b.mip, (55 >> b.mip) - (8 >> b.mip), 8 >> b.mip, 8 >> b.mip);
    RunDraw(PixelHistory::ScissorFail);

    setMarker("Scissor Pass");
    glScissor(95 >> b.mip, (55 >> b.mip) - (8 >> b.mip), 20 >> b.mip, 8 >> b.mip);
    RunDraw(PixelHistory::ScissorPass);

    glScissor(0, 0, b.width, b.height);

    setMarker("Stencil Ref");
    b.stencilRefState.set();
    RunDraw(PixelHistory::StencilRef);

    setMarker("Stencil Mask");
    b.stencilMaskState.set();
    RunDraw(PixelHistory::StencilMask);

    setMarker("Depth Test");
    b.depthState.set();
    glDepthBoundsEXT(0.15f, 1.0f);
    RunDraw(PixelHistory::DepthTest);

    setMarker("Sample Colouring");
    b.sampleColourState.set();
    RunDraw(PixelHistory::SampleColour);

    setMarker("Depth Equal Fail");
    b.depthEqualState.set();
    RunDraw(PixelHistory::DepthEqualFail);

    setMarker("Depth Equal Pass");
    b.depthEqualState.set();
    if(b.depthFormat == GL_DEPTH24_STENCIL8)
      RunDraw(PixelHistory::DepthEqualPass24);
    else if(b.depthFormat == GL_DEPTH_COMPONENT16)
      RunDraw(PixelHistory::DepthEqualPass16);
    else
      RunDraw(PixelHistory::DepthEqualPass32);

    setMarker("Colour Masked");
    b.colorMaskState.set();
    RunDraw(PixelHistory::ColourMask);

    setMarker("Overflowing");
    b.backgroundState.set();
    RunDraw(PixelHistory::OverflowingDraw);

    setMarker("Per-Fragment discarding");
    b.backgroundState.set();
    RunDraw(PixelHistory::PerFragDiscard);

    setMarker("No Output Shader");
    b.noPsState.set();
    RunDraw(PixelHistory::UnboundPS);
  }

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    PixelHistory::init();

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferStorage(GL_ARRAY_BUFFER, sizeof(DefaultA2V) * PixelHistory::vb.size(),
                    PixelHistory::vb.data(), 0);

    ConfigureDefaultVAO();

    GLuint bbBlitSource = 0;

    BuildTestBatch("Basic", false, 1, GL_RGBA8, GL_DEPTH32F_STENCIL8);
    bbBlitSource = batches.back().colImg2D;

    BuildTestBatch("Basic Pipeline", true, 1, GL_RGBA8, GL_DEPTH32F_STENCIL8);

    BuildTestBatch("MSAA", false, 4, GL_RGBA8, GL_DEPTH32F_STENCIL8);
    BuildTestBatch("Colour Only", false, 1, GL_RGBA8, GL_NONE);
    BuildTestBatch("Depth Only", false, 1, GL_NONE, GL_DEPTH32F_STENCIL8);
    BuildTestBatch("Mip & Slice", false, 1, GL_RGBA8, GL_DEPTH32F_STENCIL8, 2, 3);
    BuildTestBatch("Slice MSAA", false, 4, GL_RGBA8, GL_DEPTH32F_STENCIL8, -1, 3);

    // can't test 3D textures with depth as 3D depth is not supported and can't mix 3D with 2D array as in other APIs
    BuildTestBatch("3D texture", false, 1, GL_RGBA8, GL_NONE, -1, -1, 8);

    BuildTestBatch("D24S8", false, 1, GL_RGBA8, GL_DEPTH24_STENCIL8);

    BuildTestBatch("D16", false, 1, GL_RGBA8, GL_DEPTH_COMPONENT16);
    BuildTestBatch("D32", false, 1, GL_RGBA8, GL_DEPTH_COMPONENT32F);

    BuildTestBatch("F16 UNORM", false, 1, GL_RGBA16, GL_DEPTH32F_STENCIL8);
    BuildTestBatch("F16 FLOAT", false, 1, GL_RGBA16F, GL_DEPTH32F_STENCIL8);
    BuildTestBatch("F32 FLOAT", false, 1, GL_RGBA32F, GL_DEPTH32F_STENCIL8);

    BuildTestBatch("8-bit uint", false, 1, GL_RGBA8UI, GL_DEPTH32F_STENCIL8);
    BuildTestBatch("16-bit uint", false, 1, GL_RGBA16UI, GL_DEPTH32F_STENCIL8);
    BuildTestBatch("32-bit uint", false, 1, GL_RGBA32UI, GL_DEPTH32F_STENCIL8);

    while(Running())
    {
      for(const GLTestBatch &b : batches)
      {
        {
          pushMarker("Batch: " + b.name);
          {
            setMarker("Begin RenderPass");

            glBindFramebuffer(GL_FRAMEBUFFER, b.framebuffer);

            glScissor(0, 0, b.width, b.height);

            GLfloat clearColor[4] = {0.2f, 0.2f, 0.2f, 1.0f};
            GLuint clearColorUInt[4] = {80, 80, 80, 16};
            if(b.uint)
              glClearBufferuiv(GL_COLOR, 0, clearColorUInt);
            else
              glClearBufferfv(GL_COLOR, 0, clearColor);

            glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

            RunBatch(b);

            if(b.colImg2D)
            {
              setMarker("Compute write");

              glBindFramebuffer(GL_FRAMEBUFFER, 0);
              glUseProgram(b.compProg);
              glBindImageTexture(0, b.colImg2D, b.mip, GL_TRUE, 0, GL_READ_WRITE, b.colFmt);
              glDispatchCompute(1, 1, 1);
            }
          }
          popMarker();
        }
      }

      if(bbBlitSource)
      {
        blitToSwap(bbBlitSource);
      }

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
