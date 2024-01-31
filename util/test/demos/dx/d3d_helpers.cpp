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

#include "d3d_helpers.h"
#include "../test_common.h"

std::string D3DFullscreenQuadVertex = R"EOSHADER(

float4 main(uint vid : SV_VertexID) : SV_POSITION
{
	float2 positions[] = {
		float2(-1.0f,  1.0f),
		float2( 1.0f,  1.0f),
		float2(-1.0f, -1.0f),
		float2( 1.0f, -1.0f),
	};

	return float4(positions[vid], 0, 1);
}

)EOSHADER";

std::string D3DDefaultVertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

v2f main(vertin IN)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4(IN.pos.xyz, 1);
	OUT.col = IN.col;
	OUT.uv = IN.uv;

	return OUT;
}

)EOSHADER";

std::string D3DDefaultPixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

float4 main(v2f IN) : SV_Target0
{
	return IN.col;
}

)EOSHADER";

std::vector<IDXGIAdapterPtr> FindD3DAdapters(IDXGIFactoryPtr factory, int argc, char **argv,
                                             bool &warp)
{
  struct AdapterInfo
  {
    IDXGIAdapterPtr adapter;
    DXGI_ADAPTER_DESC desc;
  };

  std::vector<AdapterInfo> adapters;

  HRESULT hr = S_OK;
  {
    UINT i = 0;
    while(true)
    {
      IDXGIAdapterPtr a;
      hr = factory->EnumAdapters(i, &a);
      if(hr != S_OK || !a)
        break;

      DXGI_ADAPTER_DESC desc;
      a->GetDesc(&desc);
      adapters.push_back({a, desc});
      i++;
    }
  }

  IDXGIAdapterPtr specifiedAdapter = NULL;

  for(int i = 0; i < argc; i++)
  {
    if(!strcmp(argv[i], "--warp"))
    {
      warp = true;
      specifiedAdapter = NULL;
      break;
    }
    if(!strcmp(argv[i], "--gpu") && i + 1 < argc)
    {
      std::string needle = strlower(argv[i + 1]);

      if(needle == "warp")
      {
        warp = true;
        specifiedAdapter = NULL;
        break;
      }

      const bool nv = (needle == "nv" || needle == "nvidia");
      const bool amd = (needle == "amd");
      const bool intel = (needle == "intel");

      for(size_t a = 0; a < adapters.size(); a++)
      {
        std::string haystack = strlower(Wide2UTF8(adapters[a].desc.Description));

        if(haystack.find(needle) != std::string::npos ||
           (nv && adapters[a].desc.VendorId == PCI_VENDOR_NV) ||
           (amd && adapters[a].desc.VendorId == PCI_VENDOR_AMD) ||
           (intel && adapters[a].desc.VendorId == PCI_VENDOR_INTEL))
        {
          specifiedAdapter = adapters[a].adapter;
          break;
        }
      }

      break;
    }
  }

  // Return the adapters that we want to consider:
  // With an adapter specified by command line, only return that one
  // With warp specified, return an empty adapter list - fallback will occur
  // Otherwise, return all adapters, to be attempted in order
  if(specifiedAdapter)
    return {specifiedAdapter};

  if(warp)
    return {};

  std::vector<IDXGIAdapterPtr> returnedAdapters;
  for(size_t i = 0; i < adapters.size(); ++i)
    returnedAdapters.push_back(adapters[i].adapter);

  return returnedAdapters;
}
