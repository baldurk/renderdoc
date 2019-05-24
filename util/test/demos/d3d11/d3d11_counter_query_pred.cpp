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

TEST(D3D11_Counter_Query_Pred, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Tests use of D3D11 counters, queries and predication. "
      "for any dead-simple tests that don't require any particular API use";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    DefaultA2V vertData[] = {
        // passing triangle
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        // failing triangle
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(vertData);

    int numCounters = 0;
    int countersizes[2] = {4, 4};

    {
      D3D11_COUNTER_INFO info;
      dev->CheckCounterInfo(&info);

      TEST_LOG("NumSimultaneousCounters = %d, NumDetectableParallelUnits = %d",
               info.NumSimultaneousCounters, info.NumDetectableParallelUnits);

      D3D11_COUNTER_TYPE type = D3D11_COUNTER_TYPE_FLOAT32;
      UINT activeCounters = 0;
      char name[256];
      char units[64];
      char *description = new char[8192];

      numCounters = info.LastDeviceDependentCounter - D3D11_COUNTER_DEVICE_DEPENDENT_0 + 1;

      if(info.LastDeviceDependentCounter == 0)
        numCounters = 0;

      // hack for AMD
      if(info.LastDeviceDependentCounter >= 0x60000000)
        numCounters = 0;

      TEST_LOG("first%x to last %x = %d total counters", D3D11_COUNTER_DEVICE_DEPENDENT_0,
               info.LastDeviceDependentCounter, numCounters);

      for(int c = 0; c < numCounters; c++)
      {
        D3D11_COUNTER_DESC desc;
        desc.Counter = D3D11_COUNTER(D3D11_COUNTER_DEVICE_DEPENDENT_0 + c);
        desc.MiscFlags = 0;

        name[0] = 0;
        units[0] = 0;
        description[0] = 0;
        UINT namelen = 255;
        UINT unitlen = 63;
        UINT descriptionlen = 8191;

        HRESULT hr = dev->CheckCounter(&desc, &type, &activeCounters, name, &namelen, units,
                                       &unitlen, description, &descriptionlen);

        if(FAILED(hr))
        {
          TEST_LOG("Counter %x failed: %x", desc.Counter, hr);
        }
        else
        {
          TEST_LOG("Counter %x: name: '%s' (units '%s'), description '%s'", desc.Counter, name,
                   units, description);

          if(c == 0 && type == D3D11_COUNTER_TYPE_UINT64)
          {
            countersizes[0] = 8;
            TEST_LOG("Counter %x is uint64", desc.Counter);
          }
          else if(c == 0 && type == D3D11_COUNTER_TYPE_UINT64)
          {
            countersizes[1] = 8;
            TEST_LOG("Counter %x is uint64", desc.Counter);
          }
        }
      }

      delete[] description;
    }

    ID3D11CounterPtr counterIncluded, counterExcluded;

    if(numCounters > 0)
    {
      D3D11_COUNTER_DESC desc;
      desc.Counter = D3D11_COUNTER(D3D11_COUNTER_DEVICE_DEPENDENT_0 + 0);
      desc.MiscFlags = 0;

      dev->CreateCounter(&desc, &counterExcluded);

      SetDebugName(counterExcluded, "Excluded Counter");

      if(numCounters > 1)
        desc.Counter = D3D11_COUNTER(D3D11_COUNTER_DEVICE_DEPENDENT_0 + 1);

      dev->CreateCounter(&desc, &counterIncluded);

      SetDebugName(counterIncluded, "Included Counter");
    }

    ID3D11QueryPtr queryIncluded, queryExcluded;

    {
      D3D11_QUERY_DESC desc;
      desc.Query = D3D11_QUERY_OCCLUSION;
      desc.MiscFlags = 0;

      dev->CreateQuery(&desc, &queryExcluded);
      SetDebugName(queryExcluded, "Excluded Query");

      dev->CreateQuery(&desc, &queryIncluded);
      SetDebugName(queryIncluded, "Included Query");
    }

    ID3D11PredicatePtr prevFramePass, prevFrameFail;
    ID3D11PredicatePtr curFramePass, curFrameFail;

    {
      D3D11_QUERY_DESC desc;
      desc.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
      desc.MiscFlags = 0;

      dev->CreatePredicate(&desc, &prevFrameFail);
      dev->CreatePredicate(&desc, &prevFramePass);
      dev->CreatePredicate(&desc, &curFrameFail);
      dev->CreatePredicate(&desc, &curFramePass);

      SetDebugName(prevFrameFail, "prevFrameFail");
      SetDebugName(prevFramePass, "prevFramePass");
      SetDebugName(curFrameFail, "curFrameFail");
      SetDebugName(curFramePass, "curFramePass");
    }

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_5_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    int frame = 0;

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.4f, 0.5f, 0.6f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      if(frame == 3)
      {
        ctx->Begin(prevFramePass);
        ctx->Draw(3, 0);
        ctx->End(prevFramePass);

        ctx->Begin(prevFrameFail);
        ctx->Draw(3, 3);
        ctx->End(prevFrameFail);

        ctx->Begin(queryExcluded);
        ctx->Draw(3, 0);
        ctx->End(queryExcluded);

        if(counterExcluded)
        {
          ctx->Begin(counterExcluded);
          ctx->Draw(3, 0);
          ctx->End(counterExcluded);
        }

        ctx->GetData(queryExcluded, NULL, 0, 0);
        if(counterExcluded)
          ctx->GetData(counterExcluded, NULL, 0, 0);
      }
      else
      {
        ctx->Begin(curFramePass);
        ctx->Draw(3, 0);
        ctx->End(curFramePass);

        ctx->Begin(curFrameFail);
        ctx->Draw(3, 3);
        ctx->End(curFrameFail);

        ctx->Begin(queryIncluded);
        ctx->Draw(3, 0);
        ctx->End(queryIncluded);

        if(counterIncluded)
        {
          ctx->Begin(counterIncluded);
          ctx->Draw(3, 0);
          ctx->End(counterIncluded);
        }

        ctx->GetData(queryExcluded, NULL, 0, 0);
        if(counterExcluded)
          ctx->GetData(counterExcluded, NULL, 0, 0);
        ctx->GetData(queryIncluded, NULL, 0, 0);
        if(counterIncluded)
          ctx->GetData(counterIncluded, NULL, 0, 0);

        D3D11_VIEWPORT view2 = {0.0f, 0.0f, 100.0f, 100.0f, 0.0f, 1.0f};
        ctx->RSSetViewports(1, &view2);

        ctx->SetPredication(curFramePass, FALSE);
        ctx->Draw(3, 0);

        view2.TopLeftX = 100.0f;
        ctx->RSSetViewports(1, &view2);

        ctx->SetPredication(curFrameFail, FALSE);
        ctx->Draw(3, 0);

        view2.TopLeftX = 200.0f;
        ctx->RSSetViewports(1, &view2);

        ctx->SetPredication(prevFramePass, FALSE);
        ctx->Draw(3, 0);

        view2.TopLeftX = 300.0f;
        ctx->RSSetViewports(1, &view2);

        ctx->SetPredication(prevFrameFail, FALSE);
        ctx->Draw(3, 0);

        ctx->SetPredication(NULL, FALSE);
      }

      Present();

      frame++;
    }

    return 0;
  }
};

REGISTER_TEST();
