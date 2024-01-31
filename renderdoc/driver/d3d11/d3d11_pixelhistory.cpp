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

#include "data/resource.h"
#include "maths/formatpacking.h"
#include "maths/vec.h"
#include "serialise/serialiser.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_manager.h"
#include "d3d11_renderstate.h"
#include "d3d11_replay.h"
#include "d3d11_resources.h"

/*
 * The general algorithm for pixel history is this:
 *
 * we get passed a list of all events that could have touched the target texture
 * Iterate over all events replaying:
 *   Check the current state and determine which tests are enabled that could reject a pixel:
 *     - backface culling
 *     - depth clipping
 *     - scissor test
 *     - depth testing
 *     - stencil testing
 *   We also check for any tests that we can already tell will fail, e.g. our target pixel falls
 *   outside of the scissor or the sample we are interested in isn't included in the sample mask
 *
 *   Copy off the colour and depth values before the drawcall. These become the 'pre-modification'
 *   values.
 *
 *   Change the state:
 *     - Disable all tests that would reject pixels apart from scissor
 *     - Change the pixel shader to one that outputs a fixed colour (so it cannot fragment discard)
 *     - Render to off-screen dummy targets
 *     - Scissor to just around our target pixel.
 *
 *   Run the drawcall as normal with an occlusion query around it. This query will become the
 *   conservative test - i.e. if this passes we know at least something rasterized to this pixel at
 *   this draw so we can do finer tests later.
 *
 *   Run a second pass, with an off-screen depth-stencil buffer bound. First run the draw as above
 *   but using stencil op increment and saturate to count the number of fragments that wrote to the
 *   pixel. Then run with the real pixel shader rebound and count again, so we can see how many
 *   fragments discarded. Both stencil values are copied off for later.
 *
 *   If the target texture is bound as a UAV not as a render target, the above steps can be skipped
 *   as counting fragments is meaningless and we assume writes happen for UAVs (since we can't
 * detect
 *   if they do or not).
 *
 *   Copy off the colour and depth values after the drawcall. These become the 'post-modification'
 *   values.
 *
 * Iterate again over all events, this time checking the occlusion query fetched in the loop above.
 *   Check if the occlusion query hit anything (i.e. some fragment rasterized over the pixel, even
 *   if it was later rejected). Copies and UAV writing draws are assumed to pass just like if the
 *   query returned >0.
 *   At this point we also check that the view bound at the draw intersects with the particular
 *   slice & mip that we care about in the target texture (this could be done earlier).
 *
 *   For a texture that 'passes' relative to the above checks:
 *     Initialise one PixelModification for this event and push it into our list.
 *     Note any tests we know must have failed or must have passed
 *     If this event is a real draw (not a copy or UAV write):
 *       Run a series of checks, where we turn off all tests and turn them on one by one and run a
 *       single occlusion query for each.
 *       Read back the result of each occlusion query to see if any test failed. Note this in the
 *       PixelModification.
 *       These checks must be done in order, since the tests have a defined pipeline order and we
 *       don't want to claim a triangle that was backface culled actually got rejected due to depth
 *       testing.
 *
 *
 * We now have a list of PixelModifications where the pixel could have been written to but maybe
 * failed due to a test, which should be a reasonably small subset of the possible list of events we
 * started with
 *
 * Iterate over this list of modifications:
 *   Read back and decode from whatever format the pixels read above - pre- and post-modification.
 *   Also read the stencil values we recorded for how many fragments were written with a fixed
 *   shader (upper bound) and with the original shader (actual).
 *
 *   If the actual number is lower than the upper bound, some fragments were discarded so we need to
 *   go down a slow path. Otherwise we can take a fast path.
 *
 *   For each fragment written, duplicate the PixelModification we already have for this event -
 *   pre- and post-mod and all the test failures above will be identical, all that will vary is the
 *   primitive ID, fragment index, potentially shader discard status, etc.
 *
 * Finally iterate over the list of modifications:
 *   Again replay through each drawcall as needed (some modifications might duplicated on the same
 *   draw, from the above loop)
 *
 *   Set a stencil state that increment & saturates the stencil value, and tests equal. Set the
 *   stencil reference to the current fragment index. This ensures only the fragment we care about
 *   passes the stencil test.
 *
 *   If the current fragment is *not the last* on this event, replay the draw and fetch the current
 *   colour output value.
 *
 *   Run the draw again but this time with blending disabled and writing to a full float32 RGBA
 *   texture, to get the shader output value.
 *
 *   Replace the pixel shader with one that outputs the current primitive ID, and record that.
 *
 * Finally go through the shader colour values written above and slot them into the
 *   PixelModifications
 */

struct D3D11CopyPixelParams
{
  bool multisampled;
  bool floatTex;
  bool uintTex;
  bool intTex;

  UINT subres;

  bool depthcopy;     // are we copying depth or colour
  bool depthbound;    // if copying depth, was any depth bound (or should we write <-1,-1> marker)

  ID3D11Texture2D *sourceTex;    // texture with the actual data in it
  ID3D11Texture2D *srvTex;       // could be same as sourceTex if sourceTex had BIND_SRV flag on,
                                 // otherwise a texture of same format with BIND_SRV to copy to

  ID3D11ShaderResourceView *srv[2];    // srv[0] = colour or depth, srv[1] = stencil or NULL

  ID3D11UnorderedAccessView *uav;    // uav to copy pixel to

  ID3D11Buffer *srcxyCBuf;
  ID3D11Buffer *storeCBuf;
};

// reserve 4 slots per event
static const uint32_t pixstoreStride = 4;

// Helper function to copy a single pixel out of a source texture, which will handle any texture
// type and binding type, doing any copying as needed. Writes the result to a given buffer UAV.
void D3D11DebugManager::PixelHistoryCopyPixel(D3D11CopyPixelParams &p, size_t eventSlot,
                                              uint32_t storeSlot)
{
  // perform a subresource copy if the real source tex couldn't be directly bound as SRV
  if(p.sourceTex != p.srvTex && p.sourceTex && p.srvTex)
    m_pImmediateContext->CopySubresourceRegion(p.srvTex, p.subres, 0, 0, 0, p.sourceTex, p.subres,
                                               NULL);

  ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews, NULL);

  uint32_t UAVStartSlot = 0;
  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(tmpViews[i] != NULL)
    {
      UAVStartSlot = i + 1;
      SAFE_RELEASE(tmpViews[i]);
    }
  }

  ID3D11RenderTargetView *prevRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
  ID3D11UnorderedAccessView *prevUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
  ID3D11DepthStencilView *prevDSV = NULL;
  const UINT numUAVs =
      m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
  m_pImmediateContext->OMGetRenderTargetsAndUnorderedAccessViews(
      UAVStartSlot, prevRTVs, &prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs);

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 0, 0, NULL, NULL);

  ID3D11ComputeShader *curCS = NULL;
  ID3D11ClassInstance *curCSInst[D3D11_SHADER_MAX_INTERFACES] = {NULL};
  UINT curCSNumInst = D3D11_SHADER_MAX_INTERFACES;
  ID3D11Buffer *curCSCBuf[2] = {0};
  ID3D11ShaderResourceView *curCSSRVs[10] = {0};
  ID3D11UnorderedAccessView *curCSUAV[4] = {0};
  UINT initCounts[D3D11_1_UAV_SLOT_COUNT];
  memset(&initCounts[0], 0xff, sizeof(initCounts));

  m_pImmediateContext->CSGetShader(&curCS, curCSInst, &curCSNumInst);
  m_pImmediateContext->CSGetConstantBuffers(0, ARRAY_COUNT(curCSCBuf), curCSCBuf);
  m_pImmediateContext->CSGetShaderResources(0, ARRAY_COUNT(curCSSRVs), curCSSRVs);
  m_pImmediateContext->CSGetUnorderedAccessViews(0, ARRAY_COUNT(curCSUAV), curCSUAV);

  uint32_t storeData[3] = {uint32_t(eventSlot) * pixstoreStride + storeSlot, uint32_t(p.depthcopy),
                           uint32_t(p.srv[1] != NULL)};

  D3D11_MAPPED_SUBRESOURCE mapped;
  m_pImmediateContext->Map(p.storeCBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  memcpy(mapped.pData, storeData, sizeof(storeData));

  m_pImmediateContext->Unmap(p.storeCBuf, 0);

  m_pImmediateContext->CSSetConstantBuffers(0, 1, &p.srcxyCBuf);
  m_pImmediateContext->CSSetConstantBuffers(1, 1, &p.storeCBuf);

  UINT offs = 0;

  if(p.depthcopy)
  {
    offs = 0;
  }
  else
  {
    if(p.floatTex)
      offs = 1;
    else if(p.uintTex)
      offs = 2;
    else if(p.intTex)
      offs = 3;
  }

  m_pImmediateContext->CSSetUnorderedAccessViews(offs, 1, &p.uav, initCounts);

  if(p.depthcopy)
  {
    offs = p.multisampled ? 2 : 0;
  }
  else
  {
    if(p.floatTex)
      offs = 4;
    else if(p.uintTex)
      offs = 6;
    else if(p.intTex)
      offs = 8;

    if(p.multisampled)
      offs++;
  }

  m_pImmediateContext->CSSetShaderResources(offs, 2, p.srv);

  m_pImmediateContext->CSSetShader(
      !p.depthcopy || p.depthbound ? PixelHistoryCopyCS : PixelHistoryUnusedCS, NULL, 0);
  m_pImmediateContext->Dispatch(1, 1, 1);

  m_pImmediateContext->CSSetShader(curCS, curCSInst, curCSNumInst);
  m_pImmediateContext->CSSetConstantBuffers(0, ARRAY_COUNT(curCSCBuf), curCSCBuf);
  m_pImmediateContext->CSSetShaderResources(0, ARRAY_COUNT(curCSSRVs), curCSSRVs);
  m_pImmediateContext->CSSetUnorderedAccessViews(0, ARRAY_COUNT(curCSUAV), curCSUAV, initCounts);

  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
      UAVStartSlot, prevRTVs, prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs, initCounts);

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    SAFE_RELEASE(prevRTVs[i]);
  for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
    SAFE_RELEASE(prevUAVs[i]);
  SAFE_RELEASE(prevDSV);

  SAFE_RELEASE(curCS);
  for(UINT i = 0; i < curCSNumInst; i++)
    SAFE_RELEASE(curCSInst[i]);
  for(size_t i = 0; i < ARRAY_COUNT(curCSCBuf); i++)
    SAFE_RELEASE(curCSCBuf[i]);
  for(size_t i = 0; i < ARRAY_COUNT(curCSSRVs); i++)
    SAFE_RELEASE(curCSSRVs[i]);
  for(size_t i = 0; i < ARRAY_COUNT(curCSUAV); i++)
    SAFE_RELEASE(curCSUAV[i]);
}

rdcarray<PixelModification> D3D11Replay::PixelHistory(rdcarray<EventUsage> events,
                                                      ResourceId target, uint32_t x, uint32_t y,
                                                      const Subresource &sub, CompType typeCast)
{
  rdcarray<PixelModification> history;

  // this function needs a *huge* amount of tidying, refactoring and documenting.

  if(events.empty())
    return history;

  // cache the texture details of the destination texture that we're doing the pixel history on
  TextureShaderDetails details = GetDebugManager()->GetShaderDetails(target, typeCast, true);

  if(details.texFmt == DXGI_FORMAT_UNKNOWN)
    return history;

  uint32_t slice = sub.slice;
  uint32_t mip = sub.mip;
  uint32_t sampleIdx = sub.sample;

  D3D11MarkerRegion historyMarker(StringFormat::Fmt(
      "Doing PixelHistory on %s, (%u,%u) %u, %u, %u over %u events", ToStr(target).c_str(), x, y,
      slice, mip, sampleIdx, (uint32_t)events.size()));

  // Use the given type hint for typeless textures
  details.texFmt = GetTypedFormat(details.texFmt, typeCast);
  details.texFmt = GetNonSRGBFormat(details.texFmt);

  SCOPED_TIMER("D3D11DebugManager::PixelHistory");

  if(sampleIdx > details.sampleCount)
    sampleIdx = 0;

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
    sampleMask = 1U << sampleIdx;

  bool multisampled = (details.sampleCount > 1);

  // sampleIdx used later for deciding subresource to read from, so
  // set it to 0 for the no-sample case (resolved, or never MSAA in the
  // first place).
  if(sampleIdx == ~0U || !multisampled)
    sampleIdx = 0;

  // needed for comparison with viewports
  float xf = (float)x;
  float yf = (float)y;

  RDCDEBUG("Checking Pixel History on %s (%u, %u) with %u possible events", ToStr(target).c_str(),
           x, y, (uint32_t)events.size());

  // these occlusion queries are run with every test possible disabled
  rdcarray<ID3D11Query *> occl;
  occl.reserve(events.size());

  ID3D11Query *testQueries[6] = {0};    // one query for each test we do per-drawcall

  // reserve worst case all events. This is used for Pre value, Post value and
  // # frag overdraw (with & without original shader). It's reused later to retrieve per-fragment
  // post values.
  uint32_t pixstoreSlots = (uint32_t)events.size();

  // We always allocate at least 2048 slots, to allow for pixel history that only touches a couple
  // of events still being able to overdraw many times. The idea being that if we're taking the
  // history over many events, then the events which don't take up any slots or only one will mostly
  // dominate over those that take more than the average. If we only have one or two candidate
  // events then at least 2048 slots gives a huge amount of potential overdraw.
  pixstoreSlots = RDCMAX(pixstoreSlots, 2048U);

  pixstoreSlots *= pixstoreStride;

  // need UAV compatible format, so switch B8G8R8A8 for R8G8B8A8, everything will
  // render as normal and it will just be swizzled (which we were doing manually anyway).
  if(details.texFmt == DXGI_FORMAT_B8G8R8A8_UNORM)
    details.texFmt = DXGI_FORMAT_R8G8B8A8_UNORM;

  // other transformations, B8G8R8X8 also as R8G8B8A8 (alpha will be ignored)
  if(details.texFmt == DXGI_FORMAT_B8G8R8X8_UNORM)
    details.texFmt = DXGI_FORMAT_R8G8B8A8_UNORM;

  // R32G32B32 as R32G32B32A32 (alpha will be ignored)
  if(details.texFmt == DXGI_FORMAT_R32G32B32_FLOAT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
  if(details.texFmt == DXGI_FORMAT_R32G32B32_UINT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_UINT;
  if(details.texFmt == DXGI_FORMAT_R32G32B32_SINT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_SINT;

  // these formats are only valid for depth textures at which point pixstore doesn't matter, so it
  // can be anything.
  if(details.texFmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS ||
     details.texFmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
     details.texFmt == DXGI_FORMAT_R24G8_TYPELESS || details.texFmt == DXGI_FORMAT_D24_UNORM_S8_UINT ||

     details.texFmt == DXGI_FORMAT_D16_UNORM ||

     details.texFmt == DXGI_FORMAT_D32_FLOAT ||

     details.texFmt == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
     details.texFmt == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
     details.texFmt == DXGI_FORMAT_R32G8X24_TYPELESS ||
     details.texFmt == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
    details.texFmt = DXGI_FORMAT_R32G32B32A32_UINT;

  // define a buffer that we can copy before/after results into with PixelHistoryCopyPixel.
  // We previously used a texture but that doesn't always work - depth and MSAA textures can't use
  // CopySubresourceRegion to copy only one pixel, and copying with a UAV fails because some formats
  // don't support UAV. So instead we expand to float4/uint4/int4 in the UAV and write the full
  // expanded values here.
  D3D11_BUFFER_DESC pixstoreDesc = {
      (pixstoreSlots + 1) * sizeof(Vec4f),
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_UNORDERED_ACCESS,
      0,
      0,
      0,
  };

  ID3D11Buffer *pixstore = NULL, *shadoutStore = NULL, *pixstoreDepth = NULL;
  m_pDevice->CreateBuffer(&pixstoreDesc, NULL, &pixstore);
  m_pDevice->CreateBuffer(&pixstoreDesc, NULL, &shadoutStore);
  m_pDevice->CreateBuffer(&pixstoreDesc, NULL, &pixstoreDepth);

  // we'll only use the first two components of pixstoreDepth but for simplicity we keep it the same
  // size.

  pixstoreDesc.Usage = D3D11_USAGE_STAGING;
  pixstoreDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  pixstoreDesc.BindFlags = 0;

  ID3D11Buffer *pixstoreReadback = NULL, *shadoutStoreReadback = NULL, *pixstoreDepthReadback = NULL;
  m_pDevice->CreateBuffer(&pixstoreDesc, NULL, &pixstoreReadback);
  m_pDevice->CreateBuffer(&pixstoreDesc, NULL, &shadoutStoreReadback);
  m_pDevice->CreateBuffer(&pixstoreDesc, NULL, &pixstoreDepthReadback);

  // we create the UAV as typed

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = pixstoreSlots + 1;

  bool floatTex = false, uintTex = false, intTex = false;

  if(IsUIntFormat(details.texFmt))
  {
    uintTex = true;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  }
  else if(IsIntFormat(details.texFmt))
  {
    intTex = true;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  }
  else
  {
    floatTex = true;
    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  }

  ID3D11UnorderedAccessView *pixstoreUAV = NULL, *shadoutStoreUAV = NULL, *pixstoreDepthUAV = NULL;
  m_pDevice->CreateUnorderedAccessView(pixstore, &uavDesc, &pixstoreUAV);
  m_pDevice->CreateUnorderedAccessView(shadoutStore, &uavDesc, &shadoutStoreUAV);
  m_pDevice->CreateUnorderedAccessView(pixstoreDepth, &uavDesc, &pixstoreDepthUAV);

  // very wasteful, but we must leave the viewport as is to get correct rasterisation which means
  // same dimensions of render target.
  D3D11_TEXTURE2D_DESC shadoutDesc = {
      details.texWidth,
      details.texHeight,
      1U,
      1U,
      DXGI_FORMAT_R32G32B32A32_FLOAT,
      {details.sampleCount, details.sampleQuality},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
      0,
      0,
  };
  ID3D11Texture2D *shadOutput = NULL;
  m_pDevice->CreateTexture2D(&shadoutDesc, NULL, &shadOutput);

  ID3D11ShaderResourceView *shadOutputSRV = NULL;
  m_pDevice->CreateShaderResourceView(shadOutput, NULL, &shadOutputSRV);

  ID3D11RenderTargetView *shadOutputRTV = NULL;
  m_pDevice->CreateRenderTargetView(shadOutput, NULL, &shadOutputRTV);

  shadoutDesc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
  shadoutDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
  ID3D11Texture2D *shaddepthOutput = NULL;
  m_pDevice->CreateTexture2D(&shadoutDesc, NULL, &shaddepthOutput);

  ID3D11DepthStencilView *shaddepthOutputDSV = NULL;
  {
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    desc.Flags = 0;
    desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = 0;

    if(multisampled)
      desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

    m_pDevice->CreateDepthStencilView(shaddepthOutput, &desc, &shaddepthOutputDSV);
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC copyDepthSRVDesc, copyStencilSRVDesc;
  copyDepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  copyDepthSRVDesc.Texture2D.MipLevels = 1;
  copyDepthSRVDesc.Texture2D.MostDetailedMip = 0;
  copyStencilSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  copyStencilSRVDesc.Texture2D.MipLevels = 1;
  copyStencilSRVDesc.Texture2D.MostDetailedMip = 0;

  if(multisampled)
    copyDepthSRVDesc.ViewDimension = copyStencilSRVDesc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2DMS;

  ID3D11ShaderResourceView *shaddepthOutputDepthSRV = NULL, *shaddepthOutputStencilSRV = NULL;

  {
    copyDepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    m_pDevice->CreateShaderResourceView(shaddepthOutput, &copyDepthSRVDesc, &shaddepthOutputDepthSRV);
    copyDepthSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
    m_pDevice->CreateShaderResourceView(shaddepthOutput, &copyDepthSRVDesc,
                                        &shaddepthOutputStencilSRV);
  }

  // depth texture to copy to, as CopySubresourceRegion can't copy single pixels out of a depth
  // buffer, and we can't guarantee that the original depth texture is SRV-compatible to allow
  // single-pixel copies via compute shader.
  //
  // Due to copies having to match formats between source and destination we don't create these
  // textures up front but on demand, and resize up as necessary. We do a whole copy from this, then
  // a CS copy via SRV to UAV to copy into the pixstore (which we do a final copy to for readback).
  // The extra step is necessary as you can Copy to a staging texture but you can't use a CS, which
  // we need for single-pixel depth (and stencil) copy.

  D3D11_TEXTURE2D_DESC depthCopyD24S8Desc = {
      details.texWidth,
      details.texHeight,
      details.texMips,
      details.texArraySize,
      DXGI_FORMAT_R24G8_TYPELESS,
      {details.sampleCount, details.sampleQuality},
      D3D11_USAGE_DEFAULT,
      D3D11_BIND_SHADER_RESOURCE,
      0,
      0,
  };
  ID3D11Texture2D *depthCopyD24S8 = NULL;
  ID3D11ShaderResourceView *depthCopyD24S8_DepthSRV = NULL, *depthCopyD24S8_StencilSRV = NULL;

  D3D11_TEXTURE2D_DESC depthCopyD32S8Desc = depthCopyD24S8Desc;
  depthCopyD32S8Desc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
  ID3D11Texture2D *depthCopyD32S8 = NULL;
  ID3D11ShaderResourceView *depthCopyD32S8_DepthSRV = NULL, *depthCopyD32S8_StencilSRV = NULL;

  D3D11_TEXTURE2D_DESC depthCopyD32Desc = depthCopyD32S8Desc;
  depthCopyD32Desc.Format = DXGI_FORMAT_R32_TYPELESS;
  ID3D11Texture2D *depthCopyD32 = NULL;
  ID3D11ShaderResourceView *depthCopyD32_DepthSRV = NULL;

  D3D11_TEXTURE2D_DESC depthCopyD16Desc = depthCopyD24S8Desc;
  depthCopyD16Desc.Format = DXGI_FORMAT_R16_TYPELESS;
  ID3D11Texture2D *depthCopyD16 = NULL;
  ID3D11ShaderResourceView *depthCopyD16_DepthSRV = NULL;

  uint32_t srcxyData[8] = {
      x,
      y,
      multisampled ? sampleIdx : mip,
      slice,

      uint32_t(multisampled),
      uint32_t(floatTex),
      uint32_t(uintTex),
      uint32_t(intTex),
  };

  uint32_t shadoutsrcxyData[8];
  memcpy(shadoutsrcxyData, srcxyData, sizeof(srcxyData));

  // shadout texture doesn't have slices/mips, just one of the right dimension
  shadoutsrcxyData[2] = multisampled ? sampleIdx : 0;
  shadoutsrcxyData[3] = 0;

  ID3D11Buffer *srcxyCBuf = GetDebugManager()->MakeCBuffer(sizeof(srcxyData));
  ID3D11Buffer *shadoutsrcxyCBuf = GetDebugManager()->MakeCBuffer(sizeof(shadoutsrcxyData));
  ID3D11Buffer *storeCBuf = GetDebugManager()->MakeCBuffer(sizeof(srcxyData));

  GetDebugManager()->FillCBuffer(srcxyCBuf, srcxyData, sizeof(srcxyData));
  GetDebugManager()->FillCBuffer(shadoutsrcxyCBuf, shadoutsrcxyData, sizeof(shadoutsrcxyData));

  // so we do:
  // per sample: orig depth --copy--> depthCopyXXX (created/upsized on demand) --CS pixel copy-->
  // pixstoreDepth
  // at end: pixstoreDepth --copy--> pixstoreDepthReadback
  //
  // First copy is only needed if orig depth is not SRV-able
  // CS pixel copy is needed since it's the only way to copy only one pixel from depth texture,
  // CopySubresourceRegion
  // can't copy a sub-box of a depth copy. It also is required in the MSAA case to read a specific
  // pixel/sample out.
  //
  // final copy is needed to get data into a readback buffer since we can't have CS writing to
  // staging buffer
  //
  // for colour it's simple, it's just
  // per sample: orig color --copy--> pixstore
  // at end: pixstore --copy--> pixstoreReadback
  //
  // this is slightly redundant but it only adds one extra copy at the end and an extra target, and
  // allows to handle MSAA source textures (which can't copy direct to a staging texture)

  ID3D11Resource *targetres = NULL;

  if(WrappedID3D11Texture1D::m_TextureList.find(target) != WrappedID3D11Texture1D::m_TextureList.end())
    targetres = WrappedID3D11Texture1D::m_TextureList[target].m_Texture;
  else if(WrappedID3D11Texture2D1::m_TextureList.find(target) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
    targetres = WrappedID3D11Texture2D1::m_TextureList[target].m_Texture;
  else if(WrappedID3D11Texture3D1::m_TextureList.find(target) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
    targetres = WrappedID3D11Texture3D1::m_TextureList[target].m_Texture;

  D3D11CopyPixelParams colourCopyParams = {};

  // common parameters
  colourCopyParams.multisampled = multisampled;
  colourCopyParams.floatTex = floatTex;
  colourCopyParams.uintTex = uintTex;
  colourCopyParams.intTex = intTex;
  colourCopyParams.srcxyCBuf = srcxyCBuf;
  colourCopyParams.storeCBuf = storeCBuf;
  if(details.texType == eTexType_3D)
    colourCopyParams.subres = mip;
  else
    colourCopyParams.subres = details.texArraySize * slice + mip;

  D3D11CopyPixelParams depthCopyParams = colourCopyParams;

  colourCopyParams.depthcopy = false;
  colourCopyParams.sourceTex = (ID3D11Texture2D *)targetres;
  colourCopyParams.srvTex = (ID3D11Texture2D *)details.srvResource;
  colourCopyParams.srv[0] = details.srv[details.texType];
  colourCopyParams.srv[1] = NULL;
  colourCopyParams.uav = pixstoreUAV;

  depthCopyParams.depthcopy = true;
  depthCopyParams.uav = pixstoreDepthUAV;

  // while issuing the above queries we can check to see which tests are enabled so we don't
  // bother checking if depth testing failed if the depth test was disabled
  rdcarray<uint32_t> flags;
  flags.resize(events.size());
  std::map<uint32_t, D3D11_COMPARISON_FUNC> depthOps;
  std::map<uint32_t, DXGI_FORMAT> depthFormats;
  enum
  {
    TestEnabled_BackfaceCulling = 1 << 0,
    TestEnabled_DepthClip = 1 << 1,
    TestEnabled_Scissor = 1 << 2,
    TestEnabled_DepthTesting = 1 << 3,
    TestEnabled_StencilTesting = 1 << 4,

    // important to know if blending is enabled or not as we currently skip a bunch of stuff
    // and only pay attention to the final passing fragment if blending is off
    Blending_Enabled = 1 << 5,

    // additional flags we can trivially detect on the CPU for edge cases
    TestMustFail_Scissor =
        1 << 6,    // if the scissor is enabled, pixel lies outside all regions (could be only one)
    TestMustPass_Scissor =
        1 << 7,    // if the scissor is enabled, pixel lies inside all regions (could be only one)
    TestMustFail_DepthTesting = 1 << 8,      // if the comparison func is NEVER
    TestMustFail_StencilTesting = 1 << 9,    // if the comparison func is NEVER for both faces, or
                                             // one face is backface culled and the other is NEVER

    // if the sample mask set at this event doesn't have the right bit set
    TestMustFail_SampleMask = 1 << 10,

    // if predication was failing at this event
    Predication_Failed = 1 << 11,
  };

#if 1
  BOOL occlData = 0;
  const D3D11_QUERY_DESC occlDesc = {D3D11_QUERY_OCCLUSION_PREDICATE, 0};
#else
  UINT64 occlData = 0;
  const D3D11_QUERY_DESC occlDesc = {D3D11_QUERY_OCCLUSION, 0};
#endif

  HRESULT hr = S_OK;

  for(size_t i = 0; i < events.size(); i++)
  {
    ID3D11Query *q = NULL;
    m_pDevice->CreateQuery(&occlDesc, &q);
    occl.push_back(q);
  }

  for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
    m_pDevice->CreateQuery(&occlDesc, &testQueries[i]);

  //////////////////////////////////////////////////////////////////
  // Check that everything we need has successfully created.
  // We free everything together at the end

  bool allCreated = true;

  for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
  {
    if(!testQueries[i])
    {
      RDCERR("Failed to create test query %d", i);
      allCreated = false;
    }
  }

  if(!pixstore || !pixstoreUAV || !pixstoreReadback)
  {
    RDCERR("Failed to create pixstore (%p %p %p) (%u slots @ fmt %u)", pixstore, pixstoreUAV,
           pixstoreReadback, pixstoreSlots, details.texFmt);
    allCreated = false;
  }

  if(!pixstoreDepth || !pixstoreDepthUAV || !pixstoreDepthReadback)
  {
    RDCERR("Failed to create pixstoreDepth (%p %p %p) (%u slots @ fmt %u)", pixstoreDepth,
           pixstoreDepthUAV, pixstoreDepthReadback, pixstoreSlots, details.texFmt);
    allCreated = false;
  }

  if(!shadoutStore || !shadoutStoreUAV || !shadoutStoreReadback)
  {
    RDCERR("Failed to create shadoutStore (%p %p %p) (%u slots @ fmt %u)", shadoutStore,
           shadoutStoreUAV, shadoutStoreReadback, pixstoreSlots, details.texFmt);
    allCreated = false;
  }

  if(!shadOutput || !shadOutputSRV || !shadOutputRTV)
  {
    RDCERR("Failed to create shadOutput (%p %p %p) (%ux%u [%u,%u] @ fmt %u)", shadOutput,
           shadOutputSRV, shadOutputRTV, details.texWidth, details.texHeight, details.sampleCount,
           details.sampleQuality, details.texFmt);
    allCreated = false;
  }

  if(!shaddepthOutput || !shaddepthOutputDSV || !shaddepthOutputDepthSRV || !shaddepthOutputStencilSRV)
  {
    RDCERR("Failed to create shaddepthOutput (%p %p %p %p) (%ux%u [%u,%u] @ fmt %u)", shaddepthOutput,
           shaddepthOutputDSV, shaddepthOutputDepthSRV, shaddepthOutputStencilSRV, details.texWidth,
           details.texHeight, details.sampleCount, details.sampleQuality, details.texFmt);
    allCreated = false;
  }

  if(!srcxyCBuf || !storeCBuf)
  {
    RDCERR("Failed to create cbuffers (%p %p)", srcxyCBuf, storeCBuf);
    allCreated = false;
  }

  if(!allCreated)
  {
    for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
      SAFE_RELEASE(testQueries[i]);

    SAFE_RELEASE(pixstore);
    SAFE_RELEASE(shadoutStore);
    SAFE_RELEASE(pixstoreDepth);

    SAFE_RELEASE(pixstoreReadback);
    SAFE_RELEASE(shadoutStoreReadback);
    SAFE_RELEASE(pixstoreDepthReadback);

    SAFE_RELEASE(pixstoreUAV);
    SAFE_RELEASE(shadoutStoreUAV);
    SAFE_RELEASE(pixstoreDepthUAV);

    SAFE_RELEASE(shadOutput);
    SAFE_RELEASE(shadOutputSRV);
    SAFE_RELEASE(shadOutputRTV);
    SAFE_RELEASE(shaddepthOutput);
    SAFE_RELEASE(shaddepthOutputDSV);
    SAFE_RELEASE(shaddepthOutputDepthSRV);
    SAFE_RELEASE(shaddepthOutputStencilSRV);

    SAFE_RELEASE(depthCopyD24S8);
    SAFE_RELEASE(depthCopyD24S8_DepthSRV);
    SAFE_RELEASE(depthCopyD24S8_StencilSRV);

    SAFE_RELEASE(depthCopyD32S8);
    SAFE_RELEASE(depthCopyD32S8_DepthSRV);
    SAFE_RELEASE(depthCopyD32S8_StencilSRV);

    SAFE_RELEASE(depthCopyD32);
    SAFE_RELEASE(depthCopyD32_DepthSRV);

    SAFE_RELEASE(depthCopyD16);
    SAFE_RELEASE(depthCopyD16_DepthSRV);

    SAFE_RELEASE(srcxyCBuf);
    SAFE_RELEASE(shadoutsrcxyCBuf);
    SAFE_RELEASE(storeCBuf);

    return history;
  }

  {
    D3D11MarkerRegion pristine(
        StringFormat::Fmt("Replaying up to first event %u for pristine start", events[0].eventId));
    m_pDevice->ReplayLog(0, events[0].eventId, eReplay_WithoutDraw);
  }

  ID3D11RasterizerState *curRS = NULL;
  ID3D11RasterizerState *newRS = NULL;
  ID3D11DepthStencilState *newDS = NULL;
  ID3D11PixelShader *curPS = NULL;
  ID3D11ClassInstance *curInst[D3D11_SHADER_MAX_INTERFACES] = {NULL};
  UINT curNumInst = 0;
  UINT curNumViews = 0;
  UINT curNumScissors = 0;
  D3D11_VIEWPORT curViewports[16] = {0};
  D3D11_RECT curScissors[16] = {0};
  D3D11_RECT newScissors[16] = {0};
  ID3D11BlendState *curBS = NULL;
  float blendFactor[4] = {0};
  UINT curSample = 0;
  ID3D11DepthStencilState *curDS = NULL;
  UINT stencilRef = 0;

  ////////////////////////////////////////////////////////////////////////
  // Main loop over each event to determine if it rasterized to this pixel

  for(size_t ev = 0; ev < events.size(); ev++)
  {
    curNumInst = D3D11_SHADER_MAX_INTERFACES;
    curNumScissors = curNumViews = 16;

    D3D11MarkerRegion evmarker(StringFormat::Fmt("Processing output for %u", events[ev].eventId));

    bool uavOutput =
        ((events[ev].usage >= ResourceUsage::VS_RWResource &&
          events[ev].usage <= ResourceUsage::CS_RWResource) ||
         events[ev].usage == ResourceUsage::CopyDst || events[ev].usage == ResourceUsage::Copy ||
         events[ev].usage == ResourceUsage::Resolve ||
         events[ev].usage == ResourceUsage::ResolveDst || events[ev].usage == ResourceUsage::GenMips);

    m_pImmediateContext->RSGetState(&curRS);
    m_pImmediateContext->OMGetBlendState(&curBS, blendFactor, &curSample);
    m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);
    m_pImmediateContext->PSGetShader(&curPS, curInst, &curNumInst);
    m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);
    m_pImmediateContext->RSGetScissorRects(&curNumScissors, curScissors);

    // defaults (mostly)
    // disable tests/clips and enable scissor as we need it to clip visibility to just our pixel
    D3D11_RASTERIZER_DESC rd = {
        /*FillMode =*/D3D11_FILL_SOLID,
        /*CullMode =*/D3D11_CULL_NONE,
        /*FrontCounterClockwise =*/FALSE,
        /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
        /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
        /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /*DepthClipEnable =*/FALSE,
        /*ScissorEnable =*/TRUE,
        /*MultisampleEnable =*/FALSE,
        /*AntialiasedLineEnable =*/FALSE,
    };

    D3D11_RASTERIZER_DESC rsDesc = {
        /*FillMode =*/D3D11_FILL_SOLID,
        /*CullMode =*/D3D11_CULL_BACK,
        /*FrontCounterClockwise =*/FALSE,
        /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
        /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
        /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        /*DepthClipEnable =*/TRUE,
        /*ScissorEnable =*/FALSE,
        /*MultisampleEnable =*/FALSE,
        /*AntialiasedLineEnable =*/FALSE,
    };

    if(curRS)
    {
      curRS->GetDesc(&rsDesc);

      rd = rsDesc;

      if(rd.CullMode != D3D11_CULL_NONE)
        flags[ev] |= TestEnabled_BackfaceCulling;
      if(rd.DepthClipEnable)
        flags[ev] |= TestEnabled_DepthClip;
      if(rd.ScissorEnable)
        flags[ev] |= TestEnabled_Scissor;

      rd.CullMode = D3D11_CULL_NONE;
      rd.DepthClipEnable = FALSE;

      rd.ScissorEnable = TRUE;
    }
    else
    {
      rsDesc.CullMode = D3D11_CULL_BACK;
      rsDesc.ScissorEnable = FALSE;

      // defaults
      flags[ev] |= (TestEnabled_BackfaceCulling | TestEnabled_DepthClip);
    }

    D3D11_COMPARISON_FUNC depthOp = D3D11_COMPARISON_LESS;

    if(curDS)
    {
      D3D11_DEPTH_STENCIL_DESC dsDesc;
      curDS->GetDesc(&dsDesc);

      if(dsDesc.DepthEnable)
      {
        if(dsDesc.DepthFunc != D3D11_COMPARISON_ALWAYS)
          flags[ev] |= TestEnabled_DepthTesting;

        if(dsDesc.DepthFunc == D3D11_COMPARISON_NEVER)
          flags[ev] |= TestMustFail_DepthTesting;

        depthOp = dsDesc.DepthFunc;
      }
      else
      {
        depthOp = D3D11_COMPARISON_ALWAYS;
      }

      if(dsDesc.StencilEnable)
      {
        if(dsDesc.FrontFace.StencilFunc != D3D11_COMPARISON_ALWAYS ||
           dsDesc.BackFace.StencilFunc != D3D11_COMPARISON_ALWAYS)
          flags[ev] |= TestEnabled_StencilTesting;

        if(dsDesc.FrontFace.StencilFunc == D3D11_COMPARISON_NEVER &&
           dsDesc.BackFace.StencilFunc == D3D11_COMPARISON_NEVER)
          flags[ev] |= TestMustFail_StencilTesting;

        if(dsDesc.FrontFace.StencilFunc == D3D11_COMPARISON_NEVER &&
           rsDesc.CullMode == D3D11_CULL_BACK)
          flags[ev] |= TestMustFail_StencilTesting;

        if(rsDesc.CullMode == D3D11_CULL_FRONT &&
           dsDesc.BackFace.StencilFunc == D3D11_COMPARISON_NEVER)
          flags[ev] |= TestMustFail_StencilTesting;
      }
    }
    else
    {
      // defaults
      flags[ev] |= TestEnabled_DepthTesting;
    }

    depthOps[events[ev].eventId] = depthOp;

    if(rsDesc.ScissorEnable)
    {
      // see if we can find at least one scissor region this pixel could fall into
      bool inRegion = false;
      bool inAllRegions = true;

      for(UINT i = 0; i < curNumScissors && i < curNumViews; i++)
      {
        if(xf >= float(curScissors[i].left) && yf >= float(curScissors[i].top) &&
           xf < float(curScissors[i].right) && yf < float(curScissors[i].bottom))
        {
          inRegion = true;
        }
        else
        {
          inAllRegions = false;
        }
      }

      if(!inRegion)
        flags[ev] |= TestMustFail_Scissor;
      if(inAllRegions)
        flags[ev] |= TestMustPass_Scissor;
    }

    if(curBS)
    {
      D3D11_BLEND_DESC desc;
      curBS->GetDesc(&desc);

      if(desc.IndependentBlendEnable)
      {
        for(int i = 0; i < 8; i++)
        {
          if(desc.RenderTarget[i].BlendEnable)
          {
            flags[ev] |= Blending_Enabled;
            break;
          }
        }
      }
      else
      {
        if(desc.RenderTarget[0].BlendEnable)
          flags[ev] |= Blending_Enabled;
      }
    }
    else
    {
      // no blending enabled by default
    }

    // sampleMask is a mask containing only the bit for the sample we want
    // (or 0xFFFFFFFF if no sample was chosen and we are looking at them all).
    if((curSample & sampleMask) == 0)
    {
      flags[ev] |= TestMustFail_SampleMask;
    }

    if(!m_pImmediateContext->GetCurrentPipelineState()->PredicationWouldPass())
      flags[ev] |= Predication_Failed;

    m_pDevice->CreateRasterizerState(&rd, &newRS);
    m_pImmediateContext->RSSetState(newRS);
    SAFE_RELEASE(newRS);

    m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);

    m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
    m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.NopDepthState, stencilRef);

    for(UINT i = 0; i < curNumViews; i++)
    {
      // calculate scissor, relative to this viewport, that encloses only (x,y) pixel

      // if (x,y) pixel isn't in viewport, make empty rect)
      if(xf < curViewports[i].TopLeftX || yf < curViewports[i].TopLeftY ||
         xf >= curViewports[i].TopLeftX + curViewports[i].Width ||
         yf >= curViewports[i].TopLeftY + curViewports[i].Height)
      {
        newScissors[i].left = newScissors[i].top = newScissors[i].bottom = newScissors[i].right = 0;
      }
      else
      {
        newScissors[i].left = LONG(x);
        newScissors[i].top = LONG(y);
        newScissors[i].right = newScissors[i].left + 1;
        newScissors[i].bottom = newScissors[i].top + 1;
      }
    }

    // scissor every viewport
    m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

    // figure out where this event lies in the pixstore texture

    bool depthBound = false;
    ID3D11Texture2D **copyTex = NULL;
    ID3D11ShaderResourceView **copyDepthSRV = NULL;
    ID3D11ShaderResourceView **copyStencilSRV = NULL;
    ID3D11Resource *depthRes = NULL;

    // if the depth resource was already BIND_SRV we just create these SRVs pointing to it,
    // then release them after, instead of using srvs to texture copies
    ID3D11ShaderResourceView *releaseDepthSRV = NULL;
    ID3D11ShaderResourceView *releaseStencilSRV = NULL;

    {
      ID3D11DepthStencilView *dsv = NULL;
      m_pImmediateContext->OMGetRenderTargets(0, NULL, &dsv);

      if(dsv)
      {
        depthBound = true;

        dsv->GetResource(&depthRes);

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsv->GetDesc(&dsvDesc);

        SAFE_RELEASE(dsv);

        D3D11_RESOURCE_DIMENSION dim;
        depthRes->GetType(&dim);

        D3D11_TEXTURE2D_DESC desc2d;
        RDCEraseEl(desc2d);

        if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
        {
          ID3D11Texture1D *tex = (ID3D11Texture1D *)depthRes;
          D3D11_TEXTURE1D_DESC desc1d;
          tex->GetDesc(&desc1d);

          desc2d.Format = desc1d.Format;
          desc2d.Width = desc1d.Width;
          desc2d.Height = 1;
          desc2d.BindFlags = desc1d.BindFlags;
        }
        else if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
          ID3D11Texture2D *tex = (ID3D11Texture2D *)depthRes;
          tex->GetDesc(&desc2d);
        }
        else
        {
          RDCERR("Unexpected size of depth buffer");
        }

        depthFormats[events[ev].eventId] = desc2d.Format;

        bool srvable = (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) &&
                       (desc2d.BindFlags & D3D11_BIND_SHADER_RESOURCE) > 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        if(dsvDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS)
          srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = dsvDesc.Texture2D.MipSlice;

        D3D11_TEXTURE2D_DESC *copyDesc = NULL;
        if(desc2d.Format == DXGI_FORMAT_R16_FLOAT || desc2d.Format == DXGI_FORMAT_R16_SINT ||
           desc2d.Format == DXGI_FORMAT_R16_UINT || desc2d.Format == DXGI_FORMAT_R16_SNORM ||
           desc2d.Format == DXGI_FORMAT_R16_UNORM || desc2d.Format == DXGI_FORMAT_R16_TYPELESS ||
           desc2d.Format == DXGI_FORMAT_D16_UNORM)
        {
          copyDesc = &depthCopyD16Desc;
          copyTex = &depthCopyD16;
          copyDepthSRV = &depthCopyD16_DepthSRV;
          copyStencilSRV = NULL;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R16_UNORM;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R16_UNORM;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
          }
        }
        else if(desc2d.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_R24G8_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
        {
          copyDesc = &depthCopyD24S8Desc;
          copyTex = &depthCopyD24S8;
          copyDepthSRV = &depthCopyD24S8_DepthSRV;
          copyStencilSRV = &depthCopyD24S8_StencilSRV;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
          copyStencilSRVDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
            srvDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseStencilSRV);
            copyStencilSRV = &releaseStencilSRV;
          }
        }
        else if(desc2d.Format == DXGI_FORMAT_R32_FLOAT || desc2d.Format == DXGI_FORMAT_R32_SINT ||
                desc2d.Format == DXGI_FORMAT_R32_UINT ||
                desc2d.Format == DXGI_FORMAT_R32_TYPELESS || desc2d.Format == DXGI_FORMAT_D32_FLOAT)
        {
          copyDesc = &depthCopyD32Desc;
          copyTex = &depthCopyD32;
          copyDepthSRV = &depthCopyD32_DepthSRV;
          copyStencilSRV = NULL;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
          }
        }
        else if(desc2d.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_R32G8X24_TYPELESS ||
                desc2d.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
        {
          copyDesc = &depthCopyD32S8Desc;
          copyTex = &depthCopyD32S8;
          copyDepthSRV = &depthCopyD32S8_DepthSRV;
          copyStencilSRV = &depthCopyD32S8_StencilSRV;

          copyDepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
          copyStencilSRVDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

          if(srvable)
          {
            srvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            copyTex = (ID3D11Texture2D **)&depthRes;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseDepthSRV);
            copyDepthSRV = &releaseDepthSRV;
            srvDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
            m_pDevice->CreateShaderResourceView(depthRes, &srvDesc, &releaseStencilSRV);
            copyStencilSRV = &releaseStencilSRV;
          }
        }

        if(!srvable &&
           (*copyTex == NULL || desc2d.Width > copyDesc->Width || desc2d.Height > copyDesc->Height))
        {
          // recreate texture
          SAFE_RELEASE(*copyTex);
          SAFE_RELEASE(*copyDepthSRV);
          if(copyStencilSRV)
            SAFE_RELEASE(*copyStencilSRV);

          m_pDevice->CreateTexture2D(copyDesc, NULL, copyTex);
          m_pDevice->CreateShaderResourceView(*copyTex, &copyDepthSRVDesc, copyDepthSRV);
          if(copyStencilSRV)
            m_pDevice->CreateShaderResourceView(*copyTex, &copyStencilSRVDesc, copyStencilSRV);
        }
      }
    }

    D3D11MarkerRegion::Set("Copying pre-mod[0] col");
    GetDebugManager()->PixelHistoryCopyPixel(colourCopyParams, ev, 0);

    depthCopyParams.depthbound = depthBound;
    depthCopyParams.sourceTex = (ID3D11Texture2D *)depthRes;
    depthCopyParams.srvTex = copyTex ? *copyTex : NULL;
    depthCopyParams.srv[0] = copyDepthSRV ? *copyDepthSRV : NULL;
    depthCopyParams.srv[1] = copyStencilSRV ? *copyStencilSRV : NULL;

    D3D11MarkerRegion::Set("Copying pre-mod[0] depth");
    GetDebugManager()->PixelHistoryCopyPixel(depthCopyParams, ev, 0);

    m_pImmediateContext->Begin(occl[ev]);

    // For UAV output we only want to replay once in pristine conditions (only fetching before/after
    // values)
    if(!uavOutput)
      m_pDevice->ReplayLog(0, events[ev].eventId, eReplay_OnlyDraw);

    m_pImmediateContext->End(occl[ev]);

    // determine how many fragments returned from the shader
    if(!uavOutput)
    {
      D3D11_RASTERIZER_DESC rdsc = rsDesc;

      rdsc.ScissorEnable = TRUE;
      // leave depth clip mode as normal
      // leave backface culling mode as normal

      m_pDevice->CreateRasterizerState(&rdsc, &newRS);

      m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
      m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.AllPassIncrDepthState, stencilRef);
      m_pImmediateContext->RSSetState(newRS);

      SAFE_RELEASE(newRS);

      ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
      m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews, NULL);

      uint32_t UAVStartSlot = 0;
      for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
        if(tmpViews[i] != NULL)
        {
          UAVStartSlot = i + 1;
          SAFE_RELEASE(tmpViews[i]);
        }
      }

      ID3D11RenderTargetView *prevRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
      ID3D11UnorderedAccessView *prevUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
      ID3D11DepthStencilView *prevDSV = NULL;
      const UINT numUAVs =
          m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT : D3D11_PS_CS_UAV_REGISTER_COUNT;
      m_pImmediateContext->OMGetRenderTargetsAndUnorderedAccessViews(
          UAVStartSlot, prevRTVs, &prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs);

      D3D11CopyPixelParams params = depthCopyParams;
      params.depthbound = true;
      params.srvTex = params.sourceTex = shaddepthOutput;
      params.srv[0] = shaddepthOutputDepthSRV;
      params.srv[1] = shaddepthOutputStencilSRV;

      D3D11MarkerRegion::Set("Clearing depth/stencil for frag counting");
      m_pImmediateContext->ClearDepthStencilView(shaddepthOutputDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);

      m_pImmediateContext->OMSetRenderTargets(0, NULL, shaddepthOutputDSV);

      // replay first with overlay shader. This is guaranteed to count all fragments
      D3D11MarkerRegion::Set("Counting all fragments[2]");
      m_pDevice->ReplayLog(0, events[ev].eventId, eReplay_OnlyDraw);
      GetDebugManager()->PixelHistoryCopyPixel(params, ev, 2);

      m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);

      m_pImmediateContext->ClearDepthStencilView(shaddepthOutputDSV, D3D11_CLEAR_STENCIL, 1.0f, 0);

      // now replay with original shader. Some fragments may discard and not be counted
      D3D11MarkerRegion::Set("Counting discarded fragments[3]");
      m_pDevice->ReplayLog(0, events[ev].eventId, eReplay_OnlyDraw);
      GetDebugManager()->PixelHistoryCopyPixel(params, ev, 3);

      UINT initCounts[D3D11_1_UAV_SLOT_COUNT];
      memset(&initCounts[0], 0xff, sizeof(initCounts));

      m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(
          UAVStartSlot, prevRTVs, prevDSV, UAVStartSlot, numUAVs - UAVStartSlot, prevUAVs,
          initCounts);

      for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        SAFE_RELEASE(prevRTVs[i]);
      for(int i = 0; i < D3D11_1_UAV_SLOT_COUNT; i++)
        SAFE_RELEASE(prevUAVs[i]);
      SAFE_RELEASE(prevDSV);
    }
    else
    {
      m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);
    }

    m_pImmediateContext->RSSetState(curRS);
    m_pImmediateContext->RSSetScissorRects(curNumScissors, curScissors);
    m_pImmediateContext->OMSetBlendState(curBS, blendFactor, curSample);
    m_pImmediateContext->OMSetDepthStencilState(curDS, stencilRef);

    for(UINT i = 0; i < curNumInst; i++)
      SAFE_RELEASE(curInst[i]);

    SAFE_RELEASE(curPS);
    SAFE_RELEASE(curRS);
    SAFE_RELEASE(curBS);
    SAFE_RELEASE(curDS);

    // replay only action to get immediately post-modification values
    m_pDevice->ReplayLog(events[ev].eventId, events[ev].eventId, eReplay_OnlyDraw);

    D3D11MarkerRegion::Set("Copying post-mod col/depth[1]");
    GetDebugManager()->PixelHistoryCopyPixel(colourCopyParams, ev, 1);
    GetDebugManager()->PixelHistoryCopyPixel(depthCopyParams, ev, 1);

    SAFE_RELEASE(releaseDepthSRV);
    SAFE_RELEASE(releaseStencilSRV);

    if(ev < events.size() - 1)
    {
      D3D11MarkerRegion continuation(
          StringFormat::Fmt("Replaying partial continuation from %u to %u", events[ev].eventId + 1,
                            events[ev + 1].eventId));
      m_pDevice->ReplayLog(events[ev].eventId + 1, events[ev + 1].eventId, eReplay_WithoutDraw);
    }

    SAFE_RELEASE(depthRes);
  }

  ////////////////////////////////////////////////////////////////////////
  // Second loop over each event to determine if it the above query returned
  // true and narrow down which tests (if any) it failed

  for(size_t i = 0; i < occl.size(); i++)
  {
    do
    {
      hr = m_pImmediateContext->GetData(occl[i], &occlData, sizeof(occlData), 0);
    } while(hr == S_FALSE);
    RDCASSERTEQUAL(hr, S_OK);

    ResourceRange resourceRange(targetres, mip, slice);

    const ActionDescription *action = m_pDevice->GetAction(events[i].eventId);

    bool clear = bool(action->flags & ActionFlags::Clear);

    bool uavWrite =
        ((events[i].usage >= ResourceUsage::VS_RWResource &&
          events[i].usage <= ResourceUsage::CS_RWResource) ||
         events[i].usage == ResourceUsage::CopyDst || events[i].usage == ResourceUsage::Copy ||
         events[i].usage == ResourceUsage::Resolve ||
         events[i].usage == ResourceUsage::ResolveDst || events[i].usage == ResourceUsage::GenMips);

    if(events[i].view != ResourceId())
    {
      // if the access is through a view, check the mip/slice matches
      bool used = false;

      ID3D11DeviceChild *view = m_pDevice->GetResourceManager()->GetCurrentResource(events[i].view);

      if(WrappedID3D11RenderTargetView1::IsAlloc(view))
      {
        WrappedID3D11RenderTargetView1 *rtv = (WrappedID3D11RenderTargetView1 *)view;

        if(rtv->GetResourceRange().Intersects(resourceRange))
          used = true;
      }
      else if(WrappedID3D11DepthStencilView::IsAlloc(view))
      {
        WrappedID3D11DepthStencilView *dsv = (WrappedID3D11DepthStencilView *)view;

        if(dsv->GetResourceRange().Intersects(resourceRange))
          used = true;
      }
      else if(WrappedID3D11ShaderResourceView1::IsAlloc(view))
      {
        WrappedID3D11ShaderResourceView1 *srv = (WrappedID3D11ShaderResourceView1 *)view;

        if(srv->GetResourceRange().Intersects(resourceRange))
          used = true;
      }
      else if(WrappedID3D11UnorderedAccessView1::IsAlloc(view))
      {
        WrappedID3D11UnorderedAccessView1 *uav = (WrappedID3D11UnorderedAccessView1 *)view;

        if(uav->GetResourceRange().Intersects(resourceRange))
          used = true;
      }
      else
      {
        RDCWARN("Unexpected view type, ID %s. Assuming used...", ToStr(events[i].view).c_str());
        used = true;
      }

      if(!used)
      {
        RDCDEBUG("Usage %d at %u didn't refer to the matching mip/slice (%u/%u)", events[i].usage,
                 events[i].eventId, mip, slice);
        occlData = 0;
        clear = uavWrite = false;
      }
    }

    if(occlData > 0 || clear || uavWrite)
    {
      PixelModification mod;
      RDCEraseEl(mod);

      mod.eventId = events[i].eventId;

      mod.directShaderWrite = uavWrite;
      mod.unboundPS = false;

      mod.preMod.col.uintValue[0] = (uint32_t)i;

      if(!(action->flags & ActionFlags::Clear) && !uavWrite)
      {
        if(flags[i] & TestMustFail_DepthTesting)
          mod.depthTestFailed = true;
        if(flags[i] & TestMustFail_StencilTesting)
          mod.stencilTestFailed = true;
        if(flags[i] & TestMustFail_Scissor)
          mod.scissorClipped = true;
        if(flags[i] & TestMustFail_SampleMask)
          mod.sampleMasked = true;
        if(flags[i] & Predication_Failed)
          mod.predicationSkipped = true;

        {
          D3D11MarkerRegion pristine(
              StringFormat::Fmt("Replaying up to event %u for pristine start", events[i].eventId));
          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);
        }

        {
          ID3D11RenderTargetView *tmpViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
          m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpViews,
                                                  NULL);

          uint32_t UAVStartSlot = 0;
          for(int v = 0; v < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; v++)
          {
            if(tmpViews[v] != NULL)
            {
              UAVStartSlot = v + 1;
              SAFE_RELEASE(tmpViews[v]);
            }
          }

          ID3D11RenderTargetView *curRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};
          ID3D11UnorderedAccessView *curUAVs[D3D11_1_UAV_SLOT_COUNT] = {0};
          ID3D11DepthStencilView *curDSV = NULL;
          const UINT numUAVs = m_pImmediateContext->IsFL11_1() ? D3D11_1_UAV_SLOT_COUNT
                                                               : D3D11_PS_CS_UAV_REGISTER_COUNT;
          m_pImmediateContext->OMGetRenderTargetsAndUnorderedAccessViews(
              UAVStartSlot, curRTVs, &curDSV, UAVStartSlot, numUAVs - UAVStartSlot, curUAVs);

          // release these now in case we skip this modification, but don't NULL them
          // so we can still compare
          {
            for(int rtv = 0; rtv < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; rtv++)
              if(curRTVs[rtv])
                curRTVs[rtv]->Release();

            for(int uav = 0; uav < D3D11_1_UAV_SLOT_COUNT; uav++)
              if(curUAVs[uav])
                curUAVs[uav]->Release();

            if(curDSV)
              curDSV->Release();
          }
        }

        curNumScissors = curNumViews = 16;
        m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);
        m_pImmediateContext->RSGetScissorRects(&curNumScissors, curScissors);
        m_pImmediateContext->RSGetState(&curRS);
        m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);
        blendFactor[0] = blendFactor[1] = blendFactor[2] = blendFactor[3] = 1.0f;
        curSample = ~0U;

        D3D11_RASTERIZER_DESC rdesc = {
            /*FillMode =*/D3D11_FILL_SOLID,
            /*CullMode =*/D3D11_CULL_BACK,
            /*FrontCounterClockwise =*/FALSE,
            /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
            /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
            /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            /*DepthClipEnable =*/TRUE,
            /*ScissorEnable =*/FALSE,
            /*MultisampleEnable =*/FALSE,
            /*AntialiasedLineEnable =*/FALSE,
        };
        if(curRS)
          curRS->GetDesc(&rdesc);

        SAFE_RELEASE(curRS);

        D3D11_DEPTH_STENCIL_DESC dsdesc = {
            /*DepthEnable =*/TRUE,
            /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
            /*DepthFunc =*/D3D11_COMPARISON_LESS,
            /*StencilEnable =*/FALSE,
            /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
            /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
            /*FrontFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS},
            /*BackFace =*/{D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS},
        };

        if(curDS)
          curDS->GetDesc(&dsdesc);

        SAFE_RELEASE(curDS);

        for(UINT v = 0; v < curNumViews; v++)
        {
          // calculate scissor, relative to this viewport, that encloses only (x,y) pixel

          // if (x,y) pixel isn't in viewport, make empty rect)
          if(xf < curViewports[v].TopLeftX || yf < curViewports[v].TopLeftY ||
             xf >= curViewports[v].TopLeftX + curViewports[v].Width ||
             yf >= curViewports[v].TopLeftY + curViewports[v].Height)
          {
            newScissors[v].left = newScissors[v].top = newScissors[v].bottom =
                newScissors[v].right = 0;
          }
          else
          {
            newScissors[v].left = LONG(x);
            newScissors[v].top = LONG(y);
            newScissors[v].right = newScissors[v].left + 1;
            newScissors[v].bottom = newScissors[v].top + 1;
          }
        }

        // for each test we only disable pipeline rejection tests that fall *after* it.
        // e.g. to get an idea if a pixel failed backface culling or not, we enable only backface
        // culling and disable everything else (since it happens first).
        // For depth testing, we leave all tests enabled up to then - as we only want to know which
        // pixels were rejected by the depth test, not pixels that might have passed the depth test
        // had they not been discarded earlier by backface culling or depth clipping.

        // test shader discard
        {
          D3D11MarkerRegion pristine(
              StringFormat::Fmt("Test shader discard in event %u", events[i].eventId));
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);

          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[3]);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[3]);

          SAFE_RELEASE(newRS);
        }

        if(flags[i] & TestEnabled_BackfaceCulling)
        {
          D3D11MarkerRegion pristine(
              StringFormat::Fmt("Test backface culling in event %u", events[i].eventId));
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          rd.DepthClipEnable = FALSE;
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[0]);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[0]);

          SAFE_RELEASE(newRS);
        }

        if(flags[i] & TestEnabled_DepthClip)
        {
          D3D11MarkerRegion pristine(
              StringFormat::Fmt("Test depth clipping in event %u", events[i].eventId));
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[1]);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[1]);

          SAFE_RELEASE(newRS);
        }

        // only check scissor if test is enabled and we don't know if it's pass or fail yet
        if((flags[i] & (TestEnabled_Scissor | TestMustPass_Scissor | TestMustFail_Scissor)) ==
           TestEnabled_Scissor)
        {
          D3D11MarkerRegion pristine(StringFormat::Fmt("Test scissor in event %u", events[i].eventId));
          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          // newScissors has scissor regions calculated to hit our target pixel on every viewport,
          // but we must intersect that with the original scissors regions for correct testing
          // behaviour. This amounts to making any scissor region that doesn't overlap with the
          // target pixel empty.
          //
          // Note that in the case of only one scissor region we can trivially detect pass/fail of
          // the test against our pixel on the CPU so we won't come in here (see check above against
          // MustFail/MustPass). So we will only do this in the case where we have multiple scissor
          // regions/viewports, some intersecting the pixel and some not. So we make the not
          // intersecting scissor regions empty so our occlusion query tests to see if any pixels
          // were written to the "passing" viewports
          D3D11_RECT intersectScissors[16] = {0};
          memcpy(intersectScissors, newScissors, sizeof(intersectScissors));

          for(UINT s = 0; s < curNumScissors; s++)
          {
            if(curScissors[s].left > newScissors[s].left ||
               curScissors[s].right < newScissors[s].right ||
               curScissors[s].top > newScissors[s].top ||
               curScissors[s].bottom < newScissors[s].bottom)
            {
              // scissor region from the log doesn't touch our target pixel, make empty.
              intersectScissors[s].left = intersectScissors[s].right = intersectScissors[s].top =
                  intersectScissors[s].bottom = 0;
            }
          }

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.AllPassDepthState, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumScissors, intersectScissors);

          m_pImmediateContext->Begin(testQueries[2]);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[2]);

          SAFE_RELEASE(newRS);
        }

        if(flags[i] & TestEnabled_DepthTesting)
        {
          D3D11MarkerRegion pristine(
              StringFormat::Fmt("Test depth testing in event %u", events[i].eventId));

          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          // leave depth clip mode as normal
          // leave backface culling mode as normal

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          D3D11_DEPTH_STENCIL_DESC dsd = dsdesc;

          // make stencil trivially pass
          dsd.StencilEnable = TRUE;
          dsd.StencilReadMask = 0xff;
          dsd.StencilWriteMask = 0xff;
          dsd.FrontFace.StencilDepthFailOp = dsd.FrontFace.StencilFailOp =
              dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
          dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
          dsd.BackFace.StencilDepthFailOp = dsd.BackFace.StencilFailOp =
              dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
          dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

          m_pDevice->CreateDepthStencilState(&dsd, &newDS);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(newDS, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[4]);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[4]);

          SAFE_RELEASE(newRS);
          SAFE_RELEASE(newDS);
        }

        if(flags[i] & TestEnabled_StencilTesting)
        {
          D3D11MarkerRegion pristine(
              StringFormat::Fmt("Test stencil testing in event %u", events[i].eventId));

          D3D11_RASTERIZER_DESC rd = rdesc;

          rd.ScissorEnable = TRUE;
          rd.DepthClipEnable = FALSE;
          rd.CullMode = D3D11_CULL_NONE;

          m_pDevice->CreateRasterizerState(&rd, &newRS);

          // leave depthstencil testing exactly as is, because a depth-fail means
          // stencil isn't run
          m_pDevice->CreateDepthStencilState(&dsdesc, &newDS);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_WithoutDraw);

          m_pImmediateContext->PSSetShader(m_General.FixedColPS, NULL, 0);
          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetDepthStencilState(newDS, stencilRef);
          m_pImmediateContext->RSSetState(newRS);
          m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

          m_pImmediateContext->Begin(testQueries[5]);

          m_pDevice->ReplayLog(0, events[i].eventId, eReplay_OnlyDraw);

          m_pImmediateContext->End(testQueries[5]);

          SAFE_RELEASE(newRS);
          SAFE_RELEASE(newDS);
        }

        // we check these in the order defined, as a positive from the backface cull test
        // will invalidate tests later (as they will also be backface culled)

        do
        {
          if(flags[i] & TestEnabled_BackfaceCulling)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[0], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.backfaceCulled = (occlData == 0);

            if(mod.backfaceCulled)
              break;
          }

          if(flags[i] & TestEnabled_DepthClip)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[1], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.depthClipped = (occlData == 0);

            if(mod.depthClipped)
              break;
          }

          if(!mod.backfaceCulled && (flags[i] & (TestEnabled_Scissor | TestMustPass_Scissor |
                                                 TestMustFail_Scissor)) == TestEnabled_Scissor)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[2], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.scissorClipped = (occlData == 0);

            if(mod.scissorClipped)
              break;
          }

          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[3], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.shaderDiscarded = (occlData == 0);

            if(mod.shaderDiscarded)
              break;
          }

          if(flags[i] & TestEnabled_DepthTesting)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[4], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.depthTestFailed = (occlData == 0);

            if(mod.depthTestFailed)
              break;
          }

          if(flags[i] & TestEnabled_StencilTesting)
          {
            do
            {
              hr = m_pImmediateContext->GetData(testQueries[5], &occlData, sizeof(occlData), 0);
            } while(hr == S_FALSE);
            RDCASSERTEQUAL(hr, S_OK);

            mod.stencilTestFailed = (occlData == 0);

            if(mod.stencilTestFailed)
              break;
          }
        } while((void)0, 0);
      }

      history.push_back(mod);

      RDCDEBUG("Event %u is visible, %llu samples visible", events[i].eventId, (UINT64)occlData);
    }

    SAFE_RELEASE(occl[i]);
  }

  m_pImmediateContext->CopyResource(pixstoreReadback, pixstore);
  m_pImmediateContext->CopyResource(pixstoreDepthReadback, pixstoreDepth);

  D3D11_MAPPED_SUBRESOURCE mapped = {0};
  m_pImmediateContext->Map(pixstoreReadback, 0, D3D11_MAP_READ, 0, &mapped);

  D3D11_MAPPED_SUBRESOURCE mappedDepth = {0};
  m_pImmediateContext->Map(pixstoreDepthReadback, 0, D3D11_MAP_READ, 0, &mappedDepth);

  byte *pixstoreDepthData = (byte *)mappedDepth.pData;
  byte *pixstoreData = (byte *)mapped.pData;

  ////////////////////////////////////////////////////////////////////////////////////////
  // Third loop over each modification event to read back the pre-action colour + depth data
  // as well as the # fragments to use in the next step

  for(size_t h = 0; h < history.size(); h++)
  {
    PixelModification &mod = history[h];

    uint32_t pre = mod.preMod.col.uintValue[0];

    mod.preMod.col.uintValue[0] = 0;

    // the UAV copy on the GPU expanded out to full size when writing to the buffer, so we can now
    // just copy across without needing to interpret according to the format.
    {
      byte *data = pixstoreData + sizeof(Vec4f) * pixstoreStride * pre;
      memcpy(&mod.preMod.col.uintValue[0], data, sizeof(Vec4f));
      memcpy(&mod.postMod.col.uintValue[0], data + sizeof(Vec4f), sizeof(Vec4f));
    }

    {
      Vec4f *data = (Vec4f *)(pixstoreDepthData + sizeof(Vec4f) * pixstoreStride * pre);

      mod.preMod.depth = data[0].x;
      mod.preMod.stencil = int32_t(data[0].y);

      mod.postMod.depth = data[1].x;
      mod.postMod.stencil = int32_t(data[1].y);

      // data[2].x (depth) unused
      // fragments writing to the pixel in this event with overlay shader
      mod.shaderOut.col.intValue[0] = int32_t(data[2].y);

      // data[3].x (depth) unused
      // fragments writing to the pixel in this event with original shader
      mod.shaderOut.col.intValue[1] = int32_t(data[3].y);
    }
  }

  m_pImmediateContext->Unmap(pixstoreDepthReadback, 0);
  m_pImmediateContext->Unmap(pixstoreReadback, 0);

  /////////////////////////////////////////////////////////////////////////
  // simple loop to expand out the history events by number of fragments,
  // duplicating and setting fragIndex in each

  for(size_t h = 0; h < history.size();)
  {
    int32_t frags = RDCMAX(1, history[h].shaderOut.col.intValue[0]);
    int32_t fragsClipped = RDCCLAMP(history[h].shaderOut.col.intValue[1], 1, frags);

    // if we have fewer fragments with the original shader, some discarded
    // so we need to do a thorough check to see which fragments discarded
    bool someFragsClipped = (fragsClipped < frags);

    PixelModification mod = history[h];

    for(int32_t f = 1; f < frags; f++)
      history.insert(h + 1, mod);

    for(int32_t f = 0; f < frags; f++)
    {
      history[h + f].fragIndex = f;
      history[h + f].primitiveID = someFragsClipped;
    }

    h += frags;
  }

  uint32_t prev = 0;

  /////////////////////////////////////////////////////////////////////////
  // loop for each fragment, for non-final fragments fetch the post-output
  // buffer value, and for each fetch the shader output value

  uint32_t postColSlot = 0;
  uint32_t shadColSlot = 0;
  uint32_t depthSlot = 0;

  uint32_t rtIndex = 100000;
  ID3D11RenderTargetView *RTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {0};

  ID3D11DepthStencilState *ds = NULL;

  D3D11CopyPixelParams shadoutCopyParams = colourCopyParams;
  shadoutCopyParams.sourceTex = shadoutCopyParams.srvTex = shadOutput;
  shadoutCopyParams.srv[0] = shadOutputSRV;
  shadoutCopyParams.uav = shadoutStoreUAV;
  shadoutCopyParams.srcxyCBuf = shadoutsrcxyCBuf;

  depthCopyParams.sourceTex = depthCopyParams.srvTex = shaddepthOutput;
  depthCopyParams.srv[0] = shaddepthOutputDepthSRV;
  depthCopyParams.srv[1] = shaddepthOutputStencilSRV;

  for(size_t h = 0; h < history.size(); h++)
  {
    const ActionDescription *action = m_pDevice->GetAction(history[h].eventId);

    if(action->flags & ActionFlags::Clear)
      continue;

    D3D11MarkerRegion historyData(StringFormat::Fmt("Fetching history data for %u", action->eventId));

    if(prev != history[h].eventId)
    {
      D3D11MarkerRegion predraw("fetching pre-action");

      m_pDevice->ReplayLog(0, history[h].eventId, eReplay_WithoutDraw);
      prev = history[h].eventId;

      curNumScissors = curNumViews = 16;
      m_pImmediateContext->RSGetViewports(&curNumViews, curViewports);

      for(UINT v = 0; v < curNumViews; v++)
      {
        // calculate scissor, relative to this viewport, that encloses only (x,y) pixel

        // if (x,y) pixel isn't in viewport, make empty rect)
        if(xf < curViewports[v].TopLeftX || yf < curViewports[v].TopLeftY ||
           xf >= curViewports[v].TopLeftX + curViewports[v].Width ||
           yf >= curViewports[v].TopLeftY + curViewports[v].Height)
        {
          newScissors[v].left = newScissors[v].top = newScissors[v].bottom = newScissors[v].right = 0;
        }
        else
        {
          newScissors[v].left = LONG(x);
          newScissors[v].top = LONG(y);
          newScissors[v].right = newScissors[v].left + 1;
          newScissors[v].bottom = newScissors[v].top + 1;
        }
      }

      m_pImmediateContext->RSSetScissorRects(curNumViews, newScissors);

      m_pImmediateContext->RSGetState(&curRS);

      D3D11_RASTERIZER_DESC rdesc = {
          /*FillMode =*/D3D11_FILL_SOLID,
          /*CullMode =*/D3D11_CULL_BACK,
          /*FrontCounterClockwise =*/FALSE,
          /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
          /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
          /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
          /*DepthClipEnable =*/TRUE,
          /*ScissorEnable =*/FALSE,
          /*MultisampleEnable =*/FALSE,
          /*AntialiasedLineEnable =*/FALSE,
      };
      if(curRS)
        curRS->GetDesc(&rdesc);

      SAFE_RELEASE(curRS);

      m_pImmediateContext->OMGetDepthStencilState(&curDS, &stencilRef);

      // make a depth-stencil state object that writes to depth, uses same comparison
      // as currently set, and tests stencil INCR_SAT / GREATER_EQUAL for fragment selection
      D3D11_DEPTH_STENCIL_DESC dsdesc = {
          /*DepthEnable =*/TRUE,
          /*DepthWriteMask =*/D3D11_DEPTH_WRITE_MASK_ALL,
          /*DepthFunc =*/D3D11_COMPARISON_LESS,
          /*StencilEnable =*/TRUE,
          /*StencilReadMask =*/D3D11_DEFAULT_STENCIL_READ_MASK,
          /*StencilWriteMask =*/D3D11_DEFAULT_STENCIL_WRITE_MASK,
          /*FrontFace =*/
          {D3D11_STENCIL_OP_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT,
           D3D11_COMPARISON_GREATER_EQUAL},
          /*BackFace =*/
          {D3D11_STENCIL_OP_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT, D3D11_STENCIL_OP_INCR_SAT,
           D3D11_COMPARISON_GREATER_EQUAL},
      };
      if(curDS)
      {
        D3D11_DEPTH_STENCIL_DESC stateDesc;
        curDS->GetDesc(&stateDesc);
        dsdesc.DepthFunc = stateDesc.DepthFunc;
      }

      if(history[h].preMod.depth < 0.0f)
        dsdesc.DepthEnable = FALSE;

      SAFE_RELEASE(curDS);

      m_pDevice->CreateDepthStencilState(&dsdesc, &ds);

      D3D11_RASTERIZER_DESC rd = rdesc;

      rd.ScissorEnable = TRUE;
      // leave depth clip mode as normal
      // leave backface culling mode as normal

      m_pDevice->CreateRasterizerState(&rd, &newRS);
      m_pImmediateContext->RSSetState(newRS);
      SAFE_RELEASE(newRS);

      for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        SAFE_RELEASE(RTVs[i]);

      m_pImmediateContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, RTVs, NULL);

      rtIndex = 100000;

      for(uint32_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
      {
        if(RTVs[i])
        {
          if(rtIndex == 100000)
          {
            ID3D11Resource *res = NULL;
            RTVs[i]->GetResource(&res);

            if(res == targetres)
              rtIndex = i;

            SAFE_RELEASE(res);
          }

          // leave the target RTV in the array
          if(rtIndex != i)
            SAFE_RELEASE(RTVs[i]);
        }
      }

      if(rtIndex == 100000)
      {
        rtIndex = 0;
        RDCWARN("Couldn't find target RT bound at this event");
      }
    }

    float cleardepth = RDCCLAMP(history[h].preMod.depth, 0.0f, 1.0f);

    m_pImmediateContext->ClearDepthStencilView(
        shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, cleardepth, 0);

    m_pImmediateContext->OMSetDepthStencilState(ds, history[h].fragIndex);

    // if we're not the last modification in our event, need to fetch post fragment value
    if(h + 1 < history.size() && history[h].eventId == history[h + 1].eventId)
    {
      D3D11MarkerRegion middraw("fetching mid-action");

      m_pImmediateContext->OMSetRenderTargets(rtIndex + 1, RTVs, shaddepthOutputDSV);

      m_pDevice->ReplayLog(0, history[h].eventId, eReplay_OnlyDraw);

      GetDebugManager()->PixelHistoryCopyPixel(colourCopyParams, postColSlot, 0);
      postColSlot++;

      GetDebugManager()->PixelHistoryCopyPixel(depthCopyParams, depthSlot, 1);
    }

    // fetch shader output value & primitive ID
    {
      m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.StencIncrEqDepthState,
                                                  history[h].fragIndex);

      m_pImmediateContext->ClearDepthStencilView(
          shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, cleardepth, 0);

      m_pImmediateContext->OMGetBlendState(&curBS, blendFactor, &curSample);

      m_pImmediateContext->OMSetBlendState(NULL, blendFactor, curSample);

      // fetch shader output value
      {
        D3D11MarkerRegion shadout("fetching shader-out");

        ID3D11RenderTargetView *sparseRTVs[8] = {0};
        sparseRTVs[rtIndex] = shadOutputRTV;
        m_pImmediateContext->OMSetRenderTargets(rtIndex + 1, sparseRTVs, shaddepthOutputDSV);

        m_pDevice->ReplayLog(0, history[h].eventId, eReplay_OnlyDraw);

        GetDebugManager()->PixelHistoryCopyPixel(shadoutCopyParams, shadColSlot, 0);

        m_pImmediateContext->OMSetRenderTargets(0, NULL, NULL);

        GetDebugManager()->PixelHistoryCopyPixel(depthCopyParams, depthSlot, 0);
      }

      m_pImmediateContext->ClearDepthStencilView(
          shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, cleardepth, 0);

      // fetch primitive ID
      {
        D3D11MarkerRegion primid("fetching prim ID");

        m_pImmediateContext->OMSetRenderTargets(1, &shadOutputRTV, shaddepthOutputDSV);

        m_pImmediateContext->PSGetShader(&curPS, curInst, &curNumInst);
        m_pImmediateContext->PSSetShader(m_PixelHistory.PrimitiveIDPS, NULL, 0);

        if(curPS == NULL)
          history[h].unboundPS = true;

        m_pDevice->ReplayLog(0, history[h].eventId, eReplay_OnlyDraw);

        m_pImmediateContext->PSSetShader(curPS, curInst, curNumInst);

        for(UINT i = 0; i < curNumInst; i++)
          SAFE_RELEASE(curInst[i]);

        SAFE_RELEASE(curPS);

        GetDebugManager()->PixelHistoryCopyPixel(shadoutCopyParams, shadColSlot, 1);
      }

      shadColSlot++;
      depthSlot++;

      m_pImmediateContext->OMSetBlendState(curBS, blendFactor, curSample);
      SAFE_RELEASE(curBS);
    }
  }

  SAFE_RELEASE(ds);

  for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    SAFE_RELEASE(RTVs[i]);

  m_pImmediateContext->CopyResource(shadoutStoreReadback, shadoutStore);
  m_pImmediateContext->CopyResource(pixstoreReadback, pixstore);
  m_pImmediateContext->CopyResource(pixstoreDepthReadback, pixstoreDepth);

  D3D11_MAPPED_SUBRESOURCE mappedShadout = {0};
  m_pImmediateContext->Map(pixstoreReadback, 0, D3D11_MAP_READ, 0, &mapped);
  m_pImmediateContext->Map(pixstoreDepthReadback, 0, D3D11_MAP_READ, 0, &mappedDepth);
  m_pImmediateContext->Map(shadoutStoreReadback, 0, D3D11_MAP_READ, 0, &mappedShadout);

  byte *shadoutStoreData = (byte *)mappedShadout.pData;
  pixstoreData = (byte *)mapped.pData;
  pixstoreDepthData = (byte *)mappedDepth.pData;

  /////////////////////////////////////////////////////////////////////////
  // final loop to fetch the values from above into the modification events

  postColSlot = 0;
  shadColSlot = 0;
  depthSlot = 0;

  prev = 0;

  // this is used to track if any previous fragments in the current draw
  // discarded. If so, the shader output values will be off-by-one in the
  // shader output storage due to stencil counting errors, and we need to
  // offset.
  uint32_t discardedOffset = 0;

  ModificationValue lastKnownGood = {};
  if(!history.isEmpty())
    lastKnownGood = history[0].preMod;

  for(size_t h = 0; h < history.size(); h++)
  {
    const ActionDescription *action = m_pDevice->GetAction(history[h].eventId);

    if(action->flags & ActionFlags::Clear)
      continue;

    // reset discarded offset every event
    if(h > 0 && history[h].eventId != history[h - 1].eventId)
    {
      discardedOffset = 0;
      lastKnownGood = history[h].preMod;
    }

    // if we're not the last modification in our event, need to fetch post fragment value
    bool lastMod = true;
    if(h + 1 < history.size() && history[h].eventId == history[h + 1].eventId)
    {
      lastMod = false;
      // colour
      {
        uint32_t offsettedSlot = (postColSlot - discardedOffset);
        byte *data = pixstoreData + sizeof(Vec4f) * pixstoreStride * offsettedSlot;
        memcpy(&history[h].postMod.col.uintValue[0], data, sizeof(Vec4f));
      }

      {
        uint32_t offsettedSlot = (depthSlot - discardedOffset);
        RDCASSERT(discardedOffset <= depthSlot);
        // post fragment depth is in slot 1 of the depth
        float *depthdata = (float *)(pixstoreDepthData +
                                     sizeof(Vec4f) * pixstoreStride * offsettedSlot + sizeof(Vec4f));

        // this is not exactly the right value when the original depth was D16, it will be slightly
        // higher precision than the actual value but that's better than not having a value at all,
        // and allows us to identify fragments within a draw which fail the depth test.
        if(history[h].preMod.depth >= 0.0f)
          history[h].postMod.depth = *depthdata;

        // we can't retrieve stencil value after each fragment, as we use stencil to identify the
        // fragment
        if(history[h].preMod.stencil >= 0)
          history[h].postMod.stencil = -2;
        else
          history[h].postMod.stencil = -1;
      }

      // in each case we only mark as "unknown" when the depth/stencil isn't already known to be
      // unbound

      postColSlot++;
    }

    // if we're not the first modification in our event, set our preMod to the previous postMod
    if(h > 0 && history[h].eventId == history[h - 1].eventId)
    {
      history[h].preMod = history[h - 1].postMod;
    }

    // fetch shader output value
    {
      bool someFragsClipped = history[h].primitiveID != 0;

      // colour
      {
        // shader output is always 4 32bit components, so we can copy straight
        uint32_t offsettedSlot = (shadColSlot - discardedOffset);
        RDCASSERT(discardedOffset <= shadColSlot);

        byte *data = shadoutStoreData + sizeof(Vec4f) * pixstoreStride * offsettedSlot;

        memcpy(&history[h].shaderOut.col.uintValue[0], data, 4 * sizeof(float));

        // primitive ID is in slot 1 and ignores any discards
        data = shadoutStoreData + sizeof(Vec4f) * pixstoreStride * shadColSlot;
        memcpy(&history[h].primitiveID, data + sizeof(Vec4f), sizeof(uint32_t));
      }

      // depth
      {
        uint32_t offsettedSlot = (depthSlot - discardedOffset);
        RDCASSERT(discardedOffset <= depthSlot);

        float *data = (float *)(pixstoreDepthData + sizeof(Vec4f) * pixstoreStride * offsettedSlot);

        history[h].shaderOut.depth = data[0];
        if(history[h].postMod.stencil == -1)
          history[h].shaderOut.stencil = -1;
        else
          // can't retrieve this as we use stencil to identify each fragment
          history[h].shaderOut.stencil = -2;
      }

      // if some fragments clipped in this draw, we need to check to see if this
      // primitive ID was one of the ones that clipped.
      // Currently the way we do that is by drawing only that primitive
      // and doing a
      if(someFragsClipped)
      {
        // don't need to worry about trashing state, since at this point we don't need to restore it
        // anymore
        if(prev != history[h].eventId)
        {
          m_pDevice->ReplayLog(0, history[h].eventId, eReplay_WithoutDraw);

          //////////////////////////////////////////////////////////////
          // Set up an identical raster state, but with scissor enabled.
          // This matches the setup when we were originally fetching the
          // number of fragments.
          m_pImmediateContext->RSGetState(&curRS);

          D3D11_RASTERIZER_DESC rsDesc = {
              /*FillMode =*/D3D11_FILL_SOLID,
              /*CullMode =*/D3D11_CULL_BACK,
              /*FrontCounterClockwise =*/FALSE,
              /*DepthBias =*/D3D11_DEFAULT_DEPTH_BIAS,
              /*DepthBiasClamp =*/D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
              /*SlopeScaledDepthBias =*/D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
              /*DepthClipEnable =*/TRUE,
              /*ScissorEnable =*/FALSE,
              /*MultisampleEnable =*/FALSE,
              /*AntialiasedLineEnable =*/FALSE,
          };

          if(curRS)
            curRS->GetDesc(&rsDesc);

          SAFE_RELEASE(curRS);

          rsDesc.ScissorEnable = TRUE;

          // scissor to our pixel
          newScissors[0].left = LONG(x);
          newScissors[0].top = LONG(y);
          newScissors[0].right = newScissors[0].left + 1;
          newScissors[0].bottom = newScissors[0].top + 1;

          m_pImmediateContext->RSSetScissorRects(1, newScissors);

          m_pDevice->CreateRasterizerState(&rsDesc, &newRS);

          m_pImmediateContext->RSSetState(newRS);

          // other states can just be set to always pass, we already know this primitive ID renders
          m_pImmediateContext->OMSetBlendState(m_PixelHistory.NopBlendState, blendFactor, sampleMask);
          m_pImmediateContext->OMSetRenderTargets(0, NULL, shaddepthOutputDSV);
          m_pImmediateContext->OMSetDepthStencilState(m_PixelHistory.AllPassDepthState, 0);

          SAFE_RELEASE(newRS);
        }
        prev = history[h].eventId;

        m_pImmediateContext->ClearDepthStencilView(
            shaddepthOutputDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);

        m_pImmediateContext->Begin(testQueries[0]);

        Topology topo =
            MakePrimitiveTopology(m_pImmediateContext->GetCurrentPipelineState()->IA.Topo);

        // do action
        if(action->flags & ActionFlags::Indexed)
        {
          // TODO once pixel history distinguishes between instances, draw only the instance for
          // this fragment
          m_pImmediateContext->DrawIndexedInstanced(
              RENDERDOC_NumVerticesPerPrimitive(topo), RDCMAX(1U, action->numInstances),
              action->indexOffset + RENDERDOC_VertexOffset(topo, history[h].primitiveID),
              action->baseVertex, action->instanceOffset);
        }
        else
        {
          m_pImmediateContext->DrawInstanced(
              RENDERDOC_NumVerticesPerPrimitive(topo), RDCMAX(1U, action->numInstances),
              action->vertexOffset + RENDERDOC_VertexOffset(topo, history[h].primitiveID),
              action->instanceOffset);
        }

        m_pImmediateContext->End(testQueries[0]);

        do
        {
          hr = m_pImmediateContext->GetData(testQueries[0], &occlData, sizeof(occlData), 0);
        } while(hr == S_FALSE);
        RDCASSERTEQUAL(hr, S_OK);

        if(occlData == 0)
        {
          history[h].shaderDiscarded = true;
          discardedOffset++;
          RDCEraseEl(history[h].shaderOut);
          history[h].shaderOut.depth = -1.0f;
          history[h].shaderOut.stencil = -1;
          if(!lastMod)
            history[h].postMod = lastKnownGood;
        }
        else
        {
          lastKnownGood = history[h].postMod;
        }
      }

      shadColSlot++;
      depthSlot++;
    }

    // check the depth value between premod/shaderout against the known test if we have valid depth
    // values, as we don't have per-fragment depth test information.
    if(history[h].preMod.depth >= 0.0f && history[h].shaderOut.depth >= 0.0f)
    {
      DXGI_FORMAT dfmt = depthFormats[history[h].eventId];
      float shadDepth = history[h].shaderOut.depth;

      // quantise depth to match before comparing
      if(dfmt == DXGI_FORMAT_D24_UNORM_S8_UINT || dfmt == DXGI_FORMAT_X24_TYPELESS_G8_UINT ||
         dfmt == DXGI_FORMAT_R24_UNORM_X8_TYPELESS || dfmt == DXGI_FORMAT_R24G8_TYPELESS)
      {
        shadDepth = float(uint32_t(float(shadDepth * 0xffffff))) / float(0xffffff);
      }
      else if(dfmt == DXGI_FORMAT_D16_UNORM || dfmt == DXGI_FORMAT_R16_TYPELESS ||
              dfmt == DXGI_FORMAT_R16_UNORM)
      {
        shadDepth = float(uint32_t(float(shadDepth * 0xffff))) / float(0xffff);
      }

      bool passed = true;
      if(depthOps[history[h].eventId] == D3D11_COMPARISON_EQUAL)
        passed = (shadDepth == history[h].preMod.depth);
      else if(depthOps[history[h].eventId] == D3D11_COMPARISON_NOT_EQUAL)
        passed = (shadDepth != history[h].preMod.depth);
      else if(depthOps[history[h].eventId] == D3D11_COMPARISON_LESS)
        passed = (shadDepth < history[h].preMod.depth);
      else if(depthOps[history[h].eventId] == D3D11_COMPARISON_LESS_EQUAL)
        passed = (shadDepth <= history[h].preMod.depth);
      else if(depthOps[history[h].eventId] == D3D11_COMPARISON_GREATER)
        passed = (shadDepth > history[h].preMod.depth);
      else if(depthOps[history[h].eventId] == D3D11_COMPARISON_GREATER_EQUAL)
        passed = (shadDepth >= history[h].preMod.depth);

      history[h].depthTestFailed = !passed;
    }
  }

  m_pImmediateContext->Unmap(shadoutStoreReadback, 0);
  m_pImmediateContext->Unmap(pixstoreReadback, 0);
  m_pImmediateContext->Unmap(pixstoreDepthReadback, 0);

  for(size_t i = 0; i < ARRAY_COUNT(testQueries); i++)
    SAFE_RELEASE(testQueries[i]);

  SAFE_RELEASE(pixstore);
  SAFE_RELEASE(shadoutStore);
  SAFE_RELEASE(pixstoreDepth);

  SAFE_RELEASE(pixstoreReadback);
  SAFE_RELEASE(shadoutStoreReadback);
  SAFE_RELEASE(pixstoreDepthReadback);

  SAFE_RELEASE(pixstoreUAV);
  SAFE_RELEASE(shadoutStoreUAV);
  SAFE_RELEASE(pixstoreDepthUAV);

  SAFE_RELEASE(shadOutput);
  SAFE_RELEASE(shadOutputSRV);
  SAFE_RELEASE(shadOutputRTV);
  SAFE_RELEASE(shaddepthOutput);
  SAFE_RELEASE(shaddepthOutputDSV);
  SAFE_RELEASE(shaddepthOutputDepthSRV);
  SAFE_RELEASE(shaddepthOutputStencilSRV);

  SAFE_RELEASE(depthCopyD24S8);
  SAFE_RELEASE(depthCopyD24S8_DepthSRV);
  SAFE_RELEASE(depthCopyD24S8_StencilSRV);

  SAFE_RELEASE(depthCopyD32S8);
  SAFE_RELEASE(depthCopyD32S8_DepthSRV);
  SAFE_RELEASE(depthCopyD32S8_StencilSRV);

  SAFE_RELEASE(depthCopyD32);
  SAFE_RELEASE(depthCopyD32_DepthSRV);

  SAFE_RELEASE(depthCopyD16);
  SAFE_RELEASE(depthCopyD16_DepthSRV);

  SAFE_RELEASE(srcxyCBuf);
  SAFE_RELEASE(shadoutsrcxyCBuf);
  SAFE_RELEASE(storeCBuf);

  return history;
}
