/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

typedef int(WINAPI *PFN_BEGIN_EVENT)(DWORD, WCHAR *);
typedef int(WINAPI *PFN_END_EVENT)();

RD_TEST(D3D11_Refcount_Check, D3D11GraphicsTest)
{
  static constexpr const char *Description =
      "Ensures that the device etc doesn't delete itself when there are still outstanding "
      "references, and also that it *does* delete itself when any cycle is detected.";

  D3D11GraphicsTest reftest;

  void Prepare(int argc, char **argv)
  {
    reftest.headless = true;

    reftest.Prepare(argc, argv);

    D3D11GraphicsTest::Prepare(argc, argv);
  }

#define CHECK_REFCOUNT(obj, expected)                                                    \
  {                                                                                      \
    ULONG count = GetRefcount(obj);                                                      \
    if(count != expected)                                                                \
    {                                                                                    \
      if(!failed)                                                                        \
        DEBUG_BREAK();                                                                   \
      failed = true;                                                                     \
      TEST_WARN(#obj " has wrong reference count. Got %u expected %u", count, expected); \
    }                                                                                    \
  }

  bool IsICDLoaded()
  {
    if(rdoc || reftest.rdoc)
    {
      // renderdoc keeps driver DLLs around to avoid race condition bugs, so we can't check on those
      // DLLs being unloaded.
      // Instead we hack by calling D3D9's Begin/EndEvent. If there's no D3D11 device alive it
      // always returns 0, otherwise it returns the nesting level minus 1. Since we don't have a
      // device/context to force it to drain the annotation queue we take advantage of the nesting
      // level starting at 0, so calling an unbalanced end() will return -1.

      static HMODULE d3d9 = LoadLibraryA("d3d9.dll");
      static PFN_BEGIN_EVENT begin = (PFN_BEGIN_EVENT)GetProcAddress(d3d9, "D3DPERF_BeginEvent");
      static PFN_END_EVENT end = (PFN_END_EVENT)GetProcAddress(d3d9, "D3DPERF_EndEvent");

      // don't care about these being unbalanced
      return end() != 0;
    }

    // a bit of a hack but I don't know of a better way to test if the device was really destroyed
    return GetModuleHandleA("nvwgf2um.dll") != NULL || GetModuleHandleA("nvwgf2umx.dll") != NULL ||
           GetModuleHandleA("atidxx32.dll") != NULL || GetModuleHandleA("atidxx64.dll") != NULL ||
           GetModuleHandleA("igd10iumd32.dll") != NULL ||
           GetModuleHandleA("igd10iumd64.dll") != NULL;
  }

  bool HasMessages(ID3D11InfoQueue * infoQueue, const std::vector<std::string> &haystacks)
  {
    std::string concat;
    UINT64 num = infoQueue->GetNumStoredMessages();
    for(UINT64 i = 0; i < num; i++)
    {
      SIZE_T len = 0;
      infoQueue->GetMessage(i, NULL, &len);

      char *msgbuf = new char[len];
      D3D11_MESSAGE *message = (D3D11_MESSAGE *)msgbuf;

      infoQueue->GetMessage(i, message, &len);

      if(message->Severity == D3D11_MESSAGE_SEVERITY_INFO)
        concat += "INFO: ";
      concat += message->pDescription;
      concat += "\n";

      delete[] msgbuf;
    }
    infoQueue->ClearStoredMessages();
    bool ret = true;
    for(const std::string &haystack : haystacks)
      ret &= (concat.find(haystack) != std::string::npos);
    return ret;
  }

  bool HasMessage(ID3D11InfoQueue * infoQueue, const std::string &haystack)
  {
    return HasMessages(infoQueue, {haystack});
  }

  int main()
  {
    // force a debug device
    reftest.debugDevice = true;

    if(!reftest.Init())
      return 4;

    {
      D3D_FEATURE_LEVEL features[] = {D3D_FEATURE_LEVEL_11_0};
      ULONG ret = 0;
      UINT dummy[] = {16, 16, 16, 16, 16};

      bool failed = false;

      static const GUID unwrappedID3D11InfoQueue__uuid = {
          0x3fc4e618, 0x3f70, 0x452a, {0x8b, 0x8f, 0xa7, 0x3a, 0xcc, 0xb5, 0x8e, 0x3d}};

      // for the first device enable INFO for creation/destruction
      ID3D11InfoQueue *infoQueue = NULL;

      // try first with renderdoc's GUID to get the unwrapped queue for testing against
      reftest.dev.QueryInterface(unwrappedID3D11InfoQueue__uuid, &infoQueue);

      if(infoQueue == NULL)
        reftest.dev.QueryInterface(__uuidof(ID3D11InfoQueue), &infoQueue);

      infoQueue->ClearStorageFilter();
      infoQueue->ClearRetrievalFilter();
      infoQueue->ClearStoredMessages();

      ID3D11Debug *dbg = NULL;
      reftest.dev.QueryInterface(__uuidof(ID3D11Debug), &dbg);

      // remove our references to everything but vb which we take locally
      reftest.defaultLayout = NULL;
      reftest.swap = NULL;
      reftest.bbTex = NULL;
      reftest.bbRTV = NULL;
      reftest.dev1 = NULL;
      reftest.dev2 = NULL;
      reftest.dev3 = NULL;
      reftest.dev4 = NULL;
      reftest.dev5 = NULL;
      reftest.ctx = NULL;
      reftest.ctx1 = NULL;
      reftest.ctx2 = NULL;
      reftest.ctx3 = NULL;
      reftest.ctx4 = NULL;
      reftest.annot = NULL;
      reftest.swapBlitVS = NULL;
      reftest.swapBlitPS = NULL;
      reftest.DefaultTriVS = NULL;
      reftest.DefaultTriPS = NULL;
      reftest.DefaultTriVB = NULL;

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      // reference counting behaviour is NOT CONTRACTUAL but some applications check for it anyway.
      // this is particularly annoying when they're checking for implementation details, like
      // whether a resource hits 0 refcount even if it's still bound somewhere, etc.
      // The below refcounting behaviour was accurate for the D3D11 runtime at time of writing, and
      // we check it against renderdoc which is based on emulating that behaviour enough to fit this
      // test.

      // grab the device into a local pointer so we can AddRef / Release manually
      ID3D11Device *localdev = reftest.dev;
      localdev->AddRef();
      reftest.dev = NULL;

      ID3D11DeviceContext *localctx = NULL;
      localdev->GetImmediateContext(&localctx);

      ////////////////////////////////////////////////////////////
      // Create a VB and test basic 'child resource' <-> device refcounting

      ID3D11BufferPtr buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);

      ID3D11Buffer *localvb = buf;
      localvb->AddRef();
      buf = NULL;

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      // the device should have 5 references - localdev, localctx, localvb, dbg, and infoQueue
      CHECK_REFCOUNT(localdev, 5);

      // the VB has one reference
      CHECK_REFCOUNT(localvb, 1);

      // add 3 refs to the vertex buffer
      localvb->AddRef();
      localvb->AddRef();
      localvb->AddRef();

      // the device should still only have 5 references, localvb only holds one on the device
      CHECK_REFCOUNT(localdev, 5);

      // but the VB has 4 references
      CHECK_REFCOUNT(localvb, 4);

      localvb->Release();
      localvb->Release();
      localvb->Release();

      CHECK_REFCOUNT(localdev, 5);
      CHECK_REFCOUNT(localvb, 1);

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      ////////////////////////////////////////////////////////////
      // in spite of being cached, state objects should not refcount strangely (apart from
      // duplicates sharing a pointer)

      {
        D3D11_RASTERIZER_DESC rsdesc = {};

        // ensure this isn't the default rasterizer state
        rsdesc.CullMode = D3D11_CULL_BACK;
        rsdesc.FillMode = D3D11_FILL_WIREFRAME;
        rsdesc.DepthBias = 55;

        ID3D11RasterizerState *rs1 = NULL, *rs2 = NULL, *rs3 = NULL;
        localdev->CreateRasterizerState(&rsdesc, &rs1);

        CHECK_REFCOUNT(localdev, 6);
        CHECK_REFCOUNT(rs1, 1);

        // change the state, get a new object
        rsdesc.DepthBias = 99;
        localdev->CreateRasterizerState(&rsdesc, &rs2);

        CHECK_REFCOUNT(localdev, 7);
        CHECK_REFCOUNT(rs1, 1);
        CHECK_REFCOUNT(rs2, 1);

        // keep the same state, get the same object
        localdev->CreateRasterizerState(&rsdesc, &rs3);

        CHECK_REFCOUNT(localdev, 7);
        CHECK_REFCOUNT(rs1, 1);
        CHECK_REFCOUNT(rs2, 2);
        CHECK_REFCOUNT(rs3, 2);

        if(rs2 != rs3)
        {
          failed = true;
          TEST_ERROR("Expected to get the same state object back");
        }

        rs1->Release();
        rs2->Release();
        rs3->Release();
      }
      CHECK_REFCOUNT(localdev, 5);

      ////////////////////////////////////////////////////////////
      // create a texture and check view <-> resource <-> device refcounting

      ID3D11Texture2DPtr tex =
          D3D11TextureCreator(localdev, DXGI_FORMAT_BC1_UNORM, 128, 128, 1).SRV();

      ID3D11Texture2D *localtex = tex;
      localtex->AddRef();
      tex = NULL;

      // device has a new reference
      CHECK_REFCOUNT(localdev, 6);
      CHECK_REFCOUNT(localtex, 1);

      ID3D11ShaderResourceViewPtr srv = D3D11ViewCreator(localdev, ViewType::SRV, localtex);

      ID3D11ShaderResourceView *localsrv = srv;
      localsrv->AddRef();
      srv = NULL;

      // the device has a new ref from the texture, AND from the SRV
      CHECK_REFCOUNT(localdev, 7);
      // the texture doesn't get a ref from the view
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      // release the texture. It is kept alive by the SRV, but the device refcount goes down too
      localtex->Release();
      CHECK_REFCOUNT(localdev, 6);
      CHECK_REFCOUNT(localtex, 0);
      CHECK_REFCOUNT(localsrv, 1);

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      {
        ID3D11Texture2D *texretrieve = NULL;
        ID3D11Resource *resretrieve = NULL;
        localsrv->GetResource(&resretrieve);
        resretrieve->QueryInterface(__uuidof(ID3D11Texture2D), (void **)&texretrieve);
        ID3D11Texture2D *texcast = (ID3D11Texture2D *)resretrieve;

        if(texretrieve != localtex)
        {
          failed = true;
          TEST_ERROR("Expected texture to come back identically");
        }
        if(texcast != localtex)
        {
          failed = true;
          TEST_ERROR("Expected texture to come back identically");
        }

        texretrieve->Release();
        resretrieve->Release();
      }

      localtex->AddRef();

      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      localsrv->AddRef();
      localsrv->AddRef();
      localsrv->AddRef();

      // external SRV references only apply to the SRV, not the texture or device. Same as any other
      // ID3D11DeviceChild
      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 4);

      localsrv->Release();
      localsrv->Release();
      localsrv->Release();

      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      ////////////////////////////////////////////////////////////
      // check refcounting on a deferred context

      ID3D11DeviceContext *localdefctx;
      localdev->CreateDeferredContext(0, &localdefctx);

      // device gets another reference
      CHECK_REFCOUNT(localdev, 8);
      CHECK_REFCOUNT(localdefctx, 1);

      localdefctx->ClearState();

      // bind the VB. Doesn't change any public refcounts
      localdefctx->IASetVertexBuffers(0, 1, &localvb, dummy, dummy);

      CHECK_REFCOUNT(localdev, 8);
      CHECK_REFCOUNT(localdefctx, 1);
      CHECK_REFCOUNT(localvb, 1);

      // VB is now held alive by the defctx
      localvb->Release();
      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localdefctx, 1);
      CHECK_REFCOUNT(localvb, 0);

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      {
        ID3D11Buffer *vbretrieve = NULL;
        localdefctx->IAGetVertexBuffers(0, 1, &vbretrieve, NULL, NULL);

        if(vbretrieve != localvb)
        {
          failed = true;
          TEST_ERROR("Expected buffer to come back identically");
        }

        vbretrieve->Release();
      }

      localvb->AddRef();

      CHECK_REFCOUNT(localdev, 8);
      CHECK_REFCOUNT(localdefctx, 1);
      CHECK_REFCOUNT(localvb, 1);

      localdefctx->Draw(0, 0);

      ID3D11CommandList *locallist = NULL;
      localdefctx->FinishCommandList(FALSE, &locallist);

      infoQueue->ClearStoredMessages();

      // extra refcount for the list, but otherwise unchanged
      CHECK_REFCOUNT(localdev, 9);
      CHECK_REFCOUNT(localdefctx, 1);
      CHECK_REFCOUNT(locallist, 1);
      CHECK_REFCOUNT(localvb, 1);

      ret = localdefctx->Release();

      // this should release it
      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localdefctx still has outstanding references");
      }

      if(!HasMessage(infoQueue, "INFO: Destroy ID3D11Context"))
      {
        failed = true;
        TEST_ERROR("Expected localdefctx to be really destroyed");
      }

      CHECK_REFCOUNT(localdev, 8);
      CHECK_REFCOUNT(locallist, 1);
      CHECK_REFCOUNT(localvb, 1);

      // the VB is held alive by the list now, though we can't retrieve it anymore
      // we skip this test because although the runtime is smart enough to keep refs
      // on the necessary objects, we aren't and we hope no-one actually takes advantage of this.
      if(0)
      {
        localvb->Release();
        CHECK_REFCOUNT(localvb, 0);

        dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

        localvb->AddRef();
        CHECK_REFCOUNT(localvb, 1);

        if(HasMessage(infoQueue, "INFO: Destroy"))
        {
          failed = true;
          TEST_ERROR("localvb should not have been destroyed");
        }
      }

      CHECK_REFCOUNT(localdev, 8);
      CHECK_REFCOUNT(locallist, 1);
      CHECK_REFCOUNT(localvb, 1);

      ret = locallist->Release();
      localctx->Flush();

      // this should release it
      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("locallist still has outstanding references");
      }

      if(!HasMessage(infoQueue, "INFO: Destroy ID3D11CommandList"))
      {
        failed = true;
        TEST_ERROR("Expected locallist to be really destroyed");
      }

      CHECK_REFCOUNT(localdev, 7);

      ////////////////////////////////////////////////////////////
      // check that resources which are bound but don't have an external ref stay alive

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      infoQueue->ClearStoredMessages();

      // another new device refcount
      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localctx, 1);
      CHECK_REFCOUNT(localvb, 1);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      // binding doesn't change public refcounts
      localctx->ClearState();
      localctx->IASetVertexBuffers(0, 1, &localvb, dummy, dummy);
      localctx->PSSetShaderResources(0, 1, &localsrv);

      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localctx, 1);
      CHECK_REFCOUNT(localvb, 1);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      // but it means we can release things and they stay alive
      localvb->Release();
      localtex->Release();
      localsrv->Release();
      localctx->Flush();

      CHECK_REFCOUNT(localvb, 0);
      CHECK_REFCOUNT(localtex, 0);
      CHECK_REFCOUNT(localsrv, 0);

      if(HasMessage(infoQueue, "INFO: Destroy"))
      {
        failed = true;
        TEST_ERROR("Expected nothing to be destroyed");
      }

      localvb->AddRef();
      localtex->AddRef();
      localsrv->AddRef();

      CHECK_REFCOUNT(localdev, 7);
      CHECK_REFCOUNT(localctx, 1);
      CHECK_REFCOUNT(localvb, 1);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      localctx->ClearState();
      localctx->Flush();

      ////////////////////////////////////////////////////////////

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      ret = localvb->Release();
      localctx->Flush();

      // this should release it
      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localvb still has outstanding references");
      }

      if(!HasMessage(infoQueue, "INFO: Destroy ID3D11Buffer"))
      {
        failed = true;
        TEST_ERROR("Expected localvb to be really destroyed");
      }

      CHECK_REFCOUNT(localdev, 6);

      ret = localsrv->Release();
      localctx->Flush();

      // this should release it
      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localsrv still has outstanding references");
      }

      if(!HasMessage(infoQueue, "INFO: Destroy ID3D11ShaderResourceView"))
      {
        failed = true;
        TEST_ERROR("Expected localsrv to be really destroyed");
      }

      CHECK_REFCOUNT(localdev, 5);

      ret = localtex->Release();
      localctx->Flush();

      // this should release it
      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localtex still has outstanding references");
      }

      if(!HasMessage(infoQueue, "INFO: Destroy ID3D11Texture2D"))
      {
        failed = true;
        TEST_ERROR("Expected localtex to be really destroyed");
      }

      // the device should have 4 references - localdev, localctx, dbg and infoQueue
      CHECK_REFCOUNT(localdev, 4);

      dbg->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);

      // ID3DUserDefinedAnnotation shares the context's refcount
      {
        CHECK_REFCOUNT(localctx, 1);

        ID3DUserDefinedAnnotationPtr annottest = localctx;

        if(annottest)
        {
          CHECK_REFCOUNT(localctx, 2);
          ID3DUserDefinedAnnotation *localannot = annottest;
          CHECK_REFCOUNT(localctx, 2);
          CHECK_REFCOUNT(localannot, 2);
        }
      }
      CHECK_REFCOUNT(localctx, 1);

      CHECK_REFCOUNT(localdev, 4);

      localctx->Release();
      localctx = NULL;
      dbg->Release();
      dbg = NULL;
      infoQueue->Release();
      infoQueue = NULL;

      // the device should only have this reference - localdev
      CHECK_REFCOUNT(localdev, 1);

      bool before = IsICDLoaded();

      ret = localdev->Release();

      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localdev still has outstanding references");
      }

      bool after = IsICDLoaded();

      if(!before)
      {
        TEST_WARN("Couldn't detect ICD at all - unclear if device really was destroyed");
      }
      else if(before && after)
      {
        failed = true;
        TEST_ERROR("Device leaked - ICD dll stayed present");
      }

      ///////////////////////////////////////////////////////////////////////////
      // test a device staying alive based on an unbound child resource
      reftest.CreateDevice({}, NULL, features, D3D11_CREATE_DEVICE_DEBUG);

      localdev = reftest.dev;
      localdev->AddRef();
      reftest.dev = NULL;
      reftest.ctx = NULL;
      CHECK_REFCOUNT(localdev, 1);

      buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);
      localvb = buf;
      localvb->AddRef();
      buf = NULL;

      CHECK_REFCOUNT(localdev, 2);
      CHECK_REFCOUNT(localvb, 1);

      localdev->Release();

      CHECK_REFCOUNT(localdev, 1);
      CHECK_REFCOUNT(localvb, 1);

      // release the device with the VB
      before = IsICDLoaded();

      ret = localvb->Release();

      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localvb still has outstanding references");
      }

      after = IsICDLoaded();

      if(!before)
      {
        TEST_WARN("Couldn't detect ICD at all - unclear if device really was destroyed");
      }
      else if(before && after)
      {
        failed = true;
        TEST_ERROR("Device leaked - ICD dll stayed present");
      }

      ///////////////////////////////////////////////////////////////////////////
      // test a device staying alive based on a *bound* child resource on the immediate context
      reftest.CreateDevice({}, NULL, features, D3D11_CREATE_DEVICE_DEBUG);

      localdev = reftest.dev;
      localdev->AddRef();
      reftest.dev = NULL;
      reftest.ctx = NULL;
      CHECK_REFCOUNT(localdev, 1);

      buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);
      localvb = buf;
      localvb->AddRef();
      buf = NULL;

      CHECK_REFCOUNT(localdev, 2);
      CHECK_REFCOUNT(localvb, 1);

      localdev->Release();

      CHECK_REFCOUNT(localdev, 1);
      CHECK_REFCOUNT(localvb, 1);

      localdev->GetImmediateContext(&localctx);
      localctx->IASetVertexBuffers(0, 1, &localvb, dummy, dummy);
      localctx->Flush();

      localctx->Release();
      localctx = NULL;

      before = IsICDLoaded();

      ret = localvb->Release();

      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localvb still has outstanding references");
      }

      after = IsICDLoaded();

      if(!before)
      {
        TEST_WARN("Couldn't detect ICD at all - unclear if device really was destroyed");
      }
      else if(before && after)
      {
        failed = true;
        TEST_ERROR("Device leaked - ICD dll stayed present");
      }

      ///////////////////////////////////////////////////////////////////////////
      // test a resource or view being destroyed when unbound
      reftest.CreateDevice({}, NULL, features, D3D11_CREATE_DEVICE_DEBUG);

      localdev = reftest.dev;
      localdev->AddRef();
      reftest.dev = NULL;
      reftest.ctx = NULL;
      CHECK_REFCOUNT(localdev, 1);

      buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);
      localvb = buf;
      localvb->AddRef();
      buf = NULL;

      tex = D3D11TextureCreator(localdev, DXGI_FORMAT_BC1_UNORM, 128, 128, 1).SRV();
      localtex = tex;
      localtex->AddRef();
      tex = NULL;

      srv = D3D11ViewCreator(localdev, ViewType::SRV, localtex);
      localsrv = srv;
      localsrv->AddRef();
      srv = NULL;

      CHECK_REFCOUNT(localdev, 4);
      CHECK_REFCOUNT(localvb, 1);
      CHECK_REFCOUNT(localtex, 1);
      CHECK_REFCOUNT(localsrv, 1);

      localdev->GetImmediateContext(&localctx);

      // try first with renderdoc's GUID to get the unwrapped queue for testing against
      localdev->QueryInterface(unwrappedID3D11InfoQueue__uuid, (void **)&infoQueue);

      if(infoQueue == NULL)
        localdev->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&infoQueue);

      infoQueue->ClearStorageFilter();
      infoQueue->ClearRetrievalFilter();
      infoQueue->ClearStoredMessages();

      CHECK_REFCOUNT(localdev, 6);
      CHECK_REFCOUNT(localvb, 1);

      localctx->IASetVertexBuffers(0, 1, &localvb, dummy, dummy);
      localctx->PSSetShaderResources(0, 1, &localsrv);
      localctx->Flush();

      localvb->Release();
      localtex->Release();
      localsrv->Release();
      localctx->Flush();

      CHECK_REFCOUNT(localdev, 3);
      CHECK_REFCOUNT(localvb, 0);
      CHECK_REFCOUNT(localtex, 0);
      CHECK_REFCOUNT(localsrv, 0);

      if(HasMessage(infoQueue, "INFO: Destroy"))
      {
        failed = true;
        TEST_ERROR("Expected nothing to be destroyed");
      }

      localctx->ClearState();
      localctx->Flush();

      CHECK_REFCOUNT(localdev, 3);

      if(!HasMessages(infoQueue, {"INFO: Destroy ID3D11Buffer", "INFO: Destroy ID3D11Texture2D",
                                  "INFO: Destroy ID3D11ShaderResourceView"}))
      {
        failed = true;
        TEST_ERROR("Expected buffer, texture and SRV to be destroyed on unbind");
      }

      localctx->Release();
      localctx = NULL;
      infoQueue->Release();
      infoQueue = NULL;

      CHECK_REFCOUNT(localdev, 1);

      before = IsICDLoaded();

      ret = localdev->Release();

      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localdev still has outstanding references");
      }

      after = IsICDLoaded();

      if(!before)
      {
        TEST_WARN("Couldn't detect ICD at all - unclear if device really was destroyed");
      }
      else if(before && after)
      {
        failed = true;
        TEST_ERROR("Device leaked - ICD dll stayed present");
      }

      ///////////////////////////////////////////////////////////////////////////
      // test that resources which temporarily bounce off 0 refcounts in a naive bind/unbind don't
      // get destroyed.
      reftest.CreateDevice({}, NULL, features, D3D11_CREATE_DEVICE_DEBUG);

      localdev = reftest.dev;
      localdev->AddRef();
      reftest.dev = NULL;
      reftest.ctx = NULL;
      CHECK_REFCOUNT(localdev, 1);

      buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);
      localvb = buf;
      localvb->AddRef();
      buf = NULL;

      buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);
      ID3D11Buffer *localvb2 = buf;
      localvb2->AddRef();
      buf = NULL;

      buf = D3D11BufferCreator(localdev).Vertex().Data(DefaultTri);
      ID3D11Buffer *localvb3 = buf;
      localvb3->AddRef();
      buf = NULL;

      CHECK_REFCOUNT(localdev, 4);
      CHECK_REFCOUNT(localvb, 1);
      CHECK_REFCOUNT(localvb2, 1);
      CHECK_REFCOUNT(localvb3, 1);

      localdev->GetImmediateContext(&localctx);

      // try first with renderdoc's GUID to get the unwrapped queue for testing against
      localdev->QueryInterface(unwrappedID3D11InfoQueue__uuid, (void **)&infoQueue);

      if(infoQueue == NULL)
        localdev->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&infoQueue);

      infoQueue->ClearStorageFilter();
      infoQueue->ClearRetrievalFilter();
      infoQueue->ClearStoredMessages();

      CHECK_REFCOUNT(localdev, 6);
      CHECK_REFCOUNT(localvb, 1);
      CHECK_REFCOUNT(localvb2, 1);
      CHECK_REFCOUNT(localvb3, 1);

      ID3D11Buffer *firstBuffers[] = {localvb, localvb2, localvb3};
      localctx->IASetVertexBuffers(0, 3, firstBuffers, dummy, dummy);
      localctx->Flush();

      localvb->Release();
      localvb2->Release();
      localvb3->Release();
      localctx->Flush();

      CHECK_REFCOUNT(localdev, 3);
      CHECK_REFCOUNT(localvb, 0);
      CHECK_REFCOUNT(localvb2, 0);
      CHECK_REFCOUNT(localvb3, 0);

      if(HasMessage(infoQueue, "INFO: Destroy"))
      {
        failed = true;
        TEST_ERROR("Expected nothing to be destroyed");
      }

      // in a naive approach, when replacing localvb with localvb3 in slot 0, localvb is truly not
      // referenced anywhere at all. This test ensures that we don't immediately destroy localvb
      // when it's unbound from slot 0 because it's soon to be bound to slot 1. Note the same then
      // happens with localvb2 which is temporarily reference-less when it's unbound from slot 1

      ID3D11Buffer *secondBuffers[] = {localvb3, localvb, localvb2};
      localctx->IASetVertexBuffers(0, 3, secondBuffers, dummy, dummy);
      localctx->Flush();

      CHECK_REFCOUNT(localdev, 3);
      CHECK_REFCOUNT(localvb, 0);
      CHECK_REFCOUNT(localvb2, 0);
      CHECK_REFCOUNT(localvb3, 0);

      if(HasMessage(infoQueue, "INFO: Destroy"))
      {
        failed = true;
        TEST_ERROR("Expected nothing to be destroyed");
      }

      // clearing the state should still unbind and destroy the buffers
      localctx->ClearState();
      localctx->Flush();

      CHECK_REFCOUNT(localdev, 3);

      if(!HasMessage(infoQueue, "INFO: Destroy ID3D11Buffer"))
      {
        failed = true;
        TEST_ERROR("Expected buffer, texture and SRV to be destroyed on unbind");
      }

      localctx->Release();
      localctx = NULL;
      infoQueue->Release();
      infoQueue = NULL;

      CHECK_REFCOUNT(localdev, 1);

      before = IsICDLoaded();

      ret = localdev->Release();

      if(ret != 0)
      {
        failed = true;
        TEST_ERROR("localdev still has outstanding references");
      }

      after = IsICDLoaded();

      if(!before)
      {
        TEST_WARN("Couldn't detect ICD at all - unclear if device really was destroyed");
      }
      else if(before && after)
      {
        failed = true;
        TEST_ERROR("Device leaked - ICD dll stayed present");
      }

      ///////////////////////////////////////////////////////////////////////////
      if(failed)
      {
        TEST_ERROR("Encountered refcounting errors, aborting test");
        return 5;
      }
    }

    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3D11InfoQueuePtr infoQueue = dev;
    if(infoQueue)
    {
      infoQueue->ClearStorageFilter();
      infoQueue->ClearRetrievalFilter();
    }

    // run a normal test that we can capture from, so the checker can see that we got this far
    // without failing
    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    CreateDefaultInputLayout(vsblob);

    ID3D11VertexShaderPtr vs = CreateVS(vsblob);
    ID3D11PixelShaderPtr ps = CreatePS(psblob);

    ID3D11BufferPtr vb = MakeBuffer().Vertex().Data(DefaultTri);

    // destroy backbuffer RTV
    bbRTV = NULL;
    ctx->Flush();

    // save the backbuffer texture
    ID3D11Texture2D *localbbtex = bbTex;
    // release the backbuffer texture
    bbTex = NULL;
    ctx->Flush();

    if(GetRefcount(localbbtex) != 0)
      TEST_FATAL("backbuffer texture isn't 0 refcount!");

    // get it back again
    CHECK_HR(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&bbTex));

    if(bbTex != localbbtex)
      TEST_FATAL("Expected backbuffer texture to be identical after obtaining it again");

    // recreate the RTV
    CHECK_HR(dev->CreateRenderTargetView(bbTex, NULL, &bbRTV));

    while(Running())
    {
      ClearRenderTargetView(bbRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      IASetVertexBuffer(vb, sizeof(DefaultA2V), 0);
      ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      ctx->IASetInputLayout(defaultLayout);

      ctx->VSSetShader(vs, NULL, 0);
      ctx->PSSetShader(ps, NULL, 0);

      RSSetViewport({0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});

      ctx->OMSetRenderTargets(1, &bbRTV.GetInterfacePtr(), NULL);

      setMarker("Color Draw");
      ctx->Draw(3, 0);

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
