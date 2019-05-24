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

#if defined(WIN32)

#include "../d3d11/d3d11_test.h"
#include "gl_test.h"

#include "3rdparty/glad/glad_wgl.h"

TEST(GL_DX_Interop, OpenGLGraphicsTest)
{
  static constexpr const char *Description =
      "Test interop between GL and DX (Create and render to a DX surface and include into "
      "GL rendering)";

  std::string dxcommon = R"EOSHADER(

struct v2f
{
  float4 pos : SV_Position;
  float2 uv : UV;
};

)EOSHADER";

  std::string dxvertex = R"EOSHADER(

v2f main(uint vid : SV_VertexID)
{
	float2 positions[] = {
		float2(-1.0f, -1.0f),
		float2(-1.0f,  1.0f),
		float2( 1.0f, -1.0f),
		float2( 1.0f,  1.0f),
	};

  v2f OUT = (v2f)0;

	OUT.pos = float4(positions[vid]*0.8f, 0, 1);
  OUT.uv = positions[vid]*0.5f + 0.5f;

  return OUT;
}

)EOSHADER";

  std::string dxpixel = R"EOSHADER(

Texture2D<float4> tex : register(t0);

float4 main(v2f IN) : SV_Target0
{
	return tex.Load(int3(IN.uv.xy*1024.0f, 0));
}

)EOSHADER";

  std::string common = R"EOSHADER(

#version 420 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

)EOSHADER";

  std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

out v2f vertOut;

uniform vec2 wave;

void main()
{
	vertOut.pos = vec4(Position.xyz, 1);
  vertOut.pos.xy += wave*0.2f;
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

in v2f vertIn;

layout(binding = 0) uniform sampler2D tex2D;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = textureLod(tex2D, vertIn.uv.xy, 0.0f);
}

)EOSHADER";

  D3D11GraphicsTest d3d;

  void Prepare(int argc, char **argv)
  {
    d3d.headless = true;

    d3d.Prepare(argc, argv);

    OpenGLGraphicsTest::Prepare(argc, argv);
  }

  int main()
  {
    if(!d3d.Init())
      return 4;

    ID3DBlobPtr vsblob = d3d.Compile(dxcommon + dxvertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = d3d.Compile(dxcommon + dxpixel, "main", "ps_5_0");

    ID3D11VertexShaderPtr vs = d3d.CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = d3d.CreatePS(psblob);

    ID3D11Texture2DPtr d3d_fromd3d =
        d3d.MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024).RTV().Shared();
    ID3D11RenderTargetViewPtr rtv = d3d.MakeRTV(d3d_fromd3d);

    ID3D11Texture2DPtr d3d_tod3d =
        d3d.MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024).RTV().SRV().Shared();
    ID3D11ShaderResourceViewPtr srv = d3d.MakeSRV(d3d_tod3d);
    ID3D11RenderTargetViewPtr rtv2 = d3d.MakeRTV(d3d_tod3d);

    float black[4] = {};
    d3d.ctx->ClearRenderTargetView(rtv2, black);

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    DefaultA2V quad[] = {
        {Vec3f(-0.8f, -0.8f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(-0.8f, 0.8f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.8f, -0.8f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.8f, 0.8f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f)},
    };

    ID3D11BufferPtr buf = d3d.MakeBuffer().Vertex().Data(quad).Shared();

    GLuint vao = MakeVAO();
    glBindVertexArray(vao);

    GLuint vb = MakeBuffer();
    glBindBuffer(GL_ARRAY_BUFFER, vb);

    HANDLE interop_dev = wglDXOpenDeviceNV(d3d.dev);

    HANDLE interop_d3dbuf = NULL;
    interop_d3dbuf = wglDXRegisterObjectNV(interop_dev, buf.GetInterfacePtr(), vb, GL_NONE,
                                           WGL_ACCESS_READ_ONLY_NV);

    TEST_ASSERT(interop_d3dbuf, "wglDXRegisterObjectNV buffer failed");

    if(!interop_d3dbuf)
      glBufferStorage(GL_ARRAY_BUFFER, sizeof(quad), quad, 0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V), (void *)(sizeof(Vec3f)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(DefaultA2V),
                          (void *)(sizeof(Vec3f) + sizeof(Vec4f)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLuint program = MakeProgram(common + vertex, common + pixel);

    GLuint gl_fromd3d = MakeTexture();
    HANDLE interop_fromd3d =
        wglDXRegisterObjectNV(interop_dev, d3d_fromd3d.GetInterfacePtr(), gl_fromd3d, GL_TEXTURE_2D,
                              WGL_ACCESS_READ_ONLY_NV);

    TEST_ASSERT(interop_fromd3d, "wglDXRegisterObjectNV texture fromd3d failed");

    GLuint gl_tod3d = MakeTexture();
    HANDLE interop_tod3d = wglDXRegisterObjectNV(interop_dev, d3d_tod3d.GetInterfacePtr(), gl_tod3d,
                                                 GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);

    TEST_ASSERT(interop_tod3d, "wglDXRegisterObjectNV texture tod3d failed");

    GLuint fbo = MakeFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_tod3d, 0);

    GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);

    glDepthFunc(GL_ALWAYS);
    glDisable(GL_DEPTH_TEST);

    ID3D11DeviceContextPtr ctx = d3d.ctx;

    HANDLE lockHandles[] = {interop_tod3d, interop_fromd3d};

    float delta = 0.0f;

    int frame = 0;

    while(Running())
    {
      frame++;

      BOOL res;
      if(interop_d3dbuf)
        res = wglDXLockObjectsNV(interop_dev, 1, &interop_d3dbuf);
      else
        res = TRUE;

      TEST_ASSERT(res, "wglDXLockObjectsNV buffer failed");

      float col2[] = {0.6f, 0.4f, 0.6f, 1.0f};
      ctx->ClearRenderTargetView(rtv, col2);

      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      D3D11_VIEWPORT view = {0.0f, 0.0f, 1024.0f, 1024.0f, 0.0f, 1.0f};
      ctx->RSSetViewports(1, &view);

      ctx->OMSetRenderTargets(1, &rtv.GetInterfacePtr(), NULL);

      ctx->PSSetShaderResources(0, 1, &srv.GetInterfacePtr());

      ctx->Draw(4, 0);

      ctx->ClearState();

      ctx->Flush();

      res = wglDXLockObjectsNV(interop_dev, ARRAY_COUNT(lockHandles), lockHandles);

      TEST_ASSERT(res, "wglDXLockObjectsNV textures failed");

      glBindVertexArray(vao);

      glUseProgram(program);

      glUniform2f(glGetUniformLocation(program, "wave"), sinf(delta * 0.9f), -cosf(delta * 2.7f));

      delta += 0.1f;

      glBindTexture(GL_TEXTURE_2D, gl_fromd3d);

      float col[] = {0.4f, 0.5f, 0.6f, 1.0f};

      // render back into d3d
      glViewport(0, 0, 1024, 1024);

      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      // render to the backbuffer for visualisation
      glViewport(0, 0, GLsizei(screenWidth), GLsizei(screenHeight));

      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glClearBufferfv(GL_COLOR, 0, col);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      glBindTexture(GL_TEXTURE_2D, 0);

      res = wglDXUnlockObjectsNV(interop_dev, ARRAY_COUNT(lockHandles), lockHandles);

      TEST_ASSERT(res, "wglDXUnlockObjectsNV textures failed");

      if(interop_d3dbuf)
        res = wglDXUnlockObjectsNV(interop_dev, 1, &interop_d3dbuf);
      else
        res = TRUE;

      TEST_ASSERT(res, "wglDXUnlockObjectsNV buffer failed");

      Present();
    }

    wglDXUnregisterObjectNV(interop_dev, interop_d3dbuf);
    wglDXUnregisterObjectNV(interop_dev, interop_fromd3d);
    wglDXUnregisterObjectNV(interop_dev, interop_tod3d);
    wglDXCloseDeviceNV(interop_dev);

    return 0;
  }
};

REGISTER_TEST();

#endif