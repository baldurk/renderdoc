/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "replay_renderer.h"
#include <string.h>
#include <time.h>
#include "common/dds_readwrite.h"
#include "jpeg-compressor/jpgd.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "os/os_specific.h"
#include "serialise/serialiser.h"
#include "serialise/string_utils.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "tinyexr/tinyexr.h"

float ConvertComponent(const ResourceFormat &fmt, byte *data)
{
  if(fmt.compByteWidth == 4)
  {
    uint32_t *u32 = (uint32_t *)data;
    int32_t *i32 = (int32_t *)data;

    if(fmt.compType == eCompType_Float)
    {
      return *(float *)u32;
    }
    else if(fmt.compType == eCompType_UInt || fmt.compType == eCompType_UScaled)
    {
      return float(*u32);
    }
    else if(fmt.compType == eCompType_SInt || fmt.compType == eCompType_SScaled)
    {
      return float(*i32);
    }
  }
  else if(fmt.compByteWidth == 2)
  {
    uint16_t *u16 = (uint16_t *)data;
    int16_t *i16 = (int16_t *)data;

    if(fmt.compType == eCompType_Float)
    {
      return ConvertFromHalf(*u16);
    }
    else if(fmt.compType == eCompType_UInt || fmt.compType == eCompType_UScaled)
    {
      return float(*u16);
    }
    else if(fmt.compType == eCompType_SInt || fmt.compType == eCompType_SScaled)
    {
      return float(*i16);
    }
    else if(fmt.compType == eCompType_UNorm)
    {
      return float(*u16) / 65535.0f;
    }
    else if(fmt.compType == eCompType_SNorm)
    {
      float f = -1.0f;

      if(*i16 == -32768)
        f = -1.0f;
      else
        f = ((float)*i16) / 32767.0f;

      return f;
    }
  }
  else if(fmt.compByteWidth == 1)
  {
    uint8_t *u8 = (uint8_t *)data;
    int8_t *i8 = (int8_t *)data;

    if(fmt.compType == eCompType_UInt || fmt.compType == eCompType_UScaled)
    {
      return float(*u8);
    }
    else if(fmt.compType == eCompType_SInt || fmt.compType == eCompType_SScaled)
    {
      return float(*i8);
    }
    else if(fmt.compType == eCompType_UNorm)
    {
      if(fmt.srgbCorrected)
        return SRGB8_lookuptable[*u8];
      else
        return float(*u8) / 255.0f;
    }
    else if(fmt.compType == eCompType_SNorm)
    {
      float f = -1.0f;

      if(*i8 == -128)
        f = -1.0f;
      else
        f = ((float)*i8) / 127.0f;

      return f;
    }
  }

  RDCERR("Unexpected format to convert from");

  return 0.0f;
}

static void fileWriteFunc(void *context, void *data, int size)
{
  FileIO::fwrite(data, 1, size, (FILE *)context);
}

ReplayRenderer::ReplayRenderer()
{
  m_pDevice = NULL;

  m_EventID = 100000;
}

ReplayRenderer::~ReplayRenderer()
{
  RDCLOG("Shutting down replay renderer");

  for(size_t i = 0; i < m_Outputs.size(); i++)
    SAFE_DELETE(m_Outputs[i]);

  m_Outputs.clear();

  for(auto it = m_CustomShaders.begin(); it != m_CustomShaders.end(); ++it)
    m_pDevice->FreeCustomShader(*it);

  m_CustomShaders.clear();

  for(auto it = m_TargetResources.begin(); it != m_TargetResources.end(); ++it)
    m_pDevice->FreeTargetResource(*it);

  m_TargetResources.clear();

  if(m_pDevice)
    m_pDevice->Shutdown();
  m_pDevice = NULL;
}

bool ReplayRenderer::SetFrameEvent(uint32_t eventID, bool force)
{
  if(eventID != m_EventID || force)
  {
    m_EventID = eventID;

    m_pDevice->ReplayLog(eventID, eReplay_WithoutDraw);

    for(size_t i = 0; i < m_Outputs.size(); i++)
      m_Outputs[i]->SetFrameEvent(eventID);

    m_pDevice->ReplayLog(eventID, eReplay_OnlyDraw);

    FetchPipelineState();
  }

  return true;
}

bool ReplayRenderer::GetD3D11PipelineState(D3D11PipelineState *state)
{
  if(state)
  {
    *state = m_D3D11PipelineState;
    return true;
  }

  return false;
}

bool ReplayRenderer::GetD3D12PipelineState(D3D12PipelineState *state)
{
  if(state)
  {
    *state = m_D3D12PipelineState;
    return true;
  }

  return false;
}

bool ReplayRenderer::GetGLPipelineState(GLPipelineState *state)
{
  if(state)
  {
    *state = m_GLPipelineState;
    return true;
  }

  return false;
}

bool ReplayRenderer::GetVulkanPipelineState(VulkanPipelineState *state)
{
  if(state)
  {
    *state = m_VulkanPipelineState;
    return true;
  }

  return false;
}

bool ReplayRenderer::GetFrameInfo(FetchFrameInfo *info)
{
  if(info == NULL)
    return false;

  *info = m_FrameRecord.frameInfo;

  return true;
}

FetchDrawcall *ReplayRenderer::GetDrawcallByEID(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}

bool ReplayRenderer::GetDrawcalls(rdctype::array<FetchDrawcall> *draws)
{
  if(draws == NULL)
    return false;

  *draws = m_FrameRecord.m_DrawCallList;
  return true;
}

bool ReplayRenderer::FetchCounters(uint32_t *counters, uint32_t numCounters,
                                   rdctype::array<CounterResult> *results)
{
  if(results == NULL)
    return false;

  vector<uint32_t> counterArray;
  counterArray.reserve(numCounters);
  for(uint32_t i = 0; i < numCounters; i++)
    counterArray.push_back(counters[i]);

  *results = m_pDevice->FetchCounters(counterArray);

  return true;
}

bool ReplayRenderer::EnumerateCounters(rdctype::array<uint32_t> *counters)
{
  if(counters == NULL)
    return false;

  *counters = m_pDevice->EnumerateCounters();

  return true;
}

bool ReplayRenderer::DescribeCounter(uint32_t counterID, CounterDescription *desc)
{
  if(desc == NULL)
    return false;

  m_pDevice->DescribeCounter(counterID, *desc);

  return true;
}

bool ReplayRenderer::GetBuffers(rdctype::array<FetchBuffer> *out)
{
  if(m_Buffers.empty())
  {
    vector<ResourceId> ids = m_pDevice->GetBuffers();

    m_Buffers.resize(ids.size());

    for(size_t i = 0; i < ids.size(); i++)
      m_Buffers[i] = m_pDevice->GetBuffer(ids[i]);
  }

  if(out)
  {
    *out = m_Buffers;
    return true;
  }

  return false;
}

bool ReplayRenderer::GetTextures(rdctype::array<FetchTexture> *out)
{
  if(m_Textures.empty())
  {
    vector<ResourceId> ids = m_pDevice->GetTextures();

    m_Textures.resize(ids.size());

    for(size_t i = 0; i < ids.size(); i++)
      m_Textures[i] = m_pDevice->GetTexture(ids[i]);
  }

  if(out)
  {
    *out = m_Textures;
    return true;
  }

  return false;
}

bool ReplayRenderer::GetResolve(uint64_t *callstack, uint32_t callstackLen,
                                rdctype::array<rdctype::str> *arr)
{
  if(arr == NULL || callstack == NULL || callstackLen == 0)
    return false;

  Callstack::StackResolver *resolv = m_pDevice->GetCallstackResolver();

  if(resolv == NULL)
  {
    create_array_uninit(*arr, 1);
    arr->elems[0] = "";
    return true;
  }

  create_array_uninit(*arr, callstackLen);
  for(size_t i = 0; i < callstackLen; i++)
  {
    Callstack::AddressDetails info = resolv->GetAddr(callstack[i]);
    arr->elems[i] = info.formattedString();
  }

  return true;
}

bool ReplayRenderer::GetDebugMessages(rdctype::array<DebugMessage> *msgs)
{
  if(msgs)
  {
    *msgs = m_pDevice->GetDebugMessages();
    return true;
  }

  return false;
}

bool ReplayRenderer::GetUsage(ResourceId id, rdctype::array<EventUsage> *usage)
{
  if(usage)
  {
    *usage = m_pDevice->GetUsage(m_pDevice->GetLiveID(id));
    return true;
  }

  return false;
}

bool ReplayRenderer::GetPostVSData(uint32_t instID, MeshDataStage stage, MeshFormat *data)
{
  if(data == NULL)
    return false;

  FetchDrawcall *draw = GetDrawcallByEID(m_EventID);

  MeshFormat ret;
  RDCEraseEl(ret);

  if(draw == NULL || (draw->flags & eDraw_Drawcall) == 0)
  {
    *data = MeshFormat();
    return false;
  }

  instID = RDCMIN(instID, draw->numInstances - 1);

  *data = m_pDevice->GetPostVSBuffers(draw->eventID, instID, stage);

  return true;
}

bool ReplayRenderer::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len,
                                   rdctype::array<byte> *data)
{
  if(data == NULL || buff == ResourceId())
    return false;

  ResourceId liveId = m_pDevice->GetLiveID(buff);

  if(liveId == ResourceId())
  {
    RDCERR("Couldn't get Live ID for %llu getting buffer data", buff);
    return false;
  }

  vector<byte> retData;
  m_pDevice->GetBufferData(liveId, offset, len, retData);

  create_array_init(*data, retData.size(), !retData.empty() ? &retData[0] : NULL);

  return true;
}

bool ReplayRenderer::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                    rdctype::array<byte> *data)
{
  if(data == NULL)
    return false;

  ResourceId liveId = m_pDevice->GetLiveID(tex);

  if(liveId == ResourceId())
  {
    RDCERR("Couldn't get Live ID for %llu getting texture data", tex);
    return false;
  }

  size_t sz = 0;
  byte *bytes = m_pDevice->GetTextureData(liveId, arrayIdx, mip, GetTextureDataParams(), sz);

  if(sz == 0 || bytes == NULL)
    create_array_uninit(*data, 0);
  else
    create_array_init(*data, sz, bytes);

  SAFE_DELETE_ARRAY(bytes);

  return true;
}

bool ReplayRenderer::SaveTexture(const TextureSave &saveData, const char *path)
{
  TextureSave sd = saveData;    // mutable copy
  ResourceId liveid = m_pDevice->GetLiveID(sd.id);
  FetchTexture td = m_pDevice->GetTexture(liveid);

  bool success = false;

  // clamp sample/mip/slice indices
  if(td.msSamp == 1)
  {
    sd.sample.sampleIndex = 0;
    sd.sample.mapToArray = false;
  }
  else
  {
    if(sd.sample.sampleIndex != ~0U)
      sd.sample.sampleIndex = RDCCLAMP(sd.sample.sampleIndex, 0U, td.msSamp);
  }

  // don't support cube cruciform for non cubemaps, or
  // cubemap arrays
  if(!td.cubemap || td.arraysize != 6 || td.msSamp != 1)
    sd.slice.cubeCruciform = false;

  if(sd.mip != -1)
    sd.mip = RDCCLAMP(sd.mip, 0, (int32_t)td.mips);
  if(sd.slice.sliceIndex != -1)
    sd.slice.sliceIndex = RDCCLAMP(sd.slice.sliceIndex, 0, int32_t(td.arraysize * td.depth));

  if(td.arraysize * td.depth * td.msSamp == 1)
  {
    sd.slice.sliceIndex = 0;
    sd.slice.slicesAsGrid = false;
  }

  // can't extract a channel that's not in the source texture
  if(sd.channelExtract >= 0 && (uint32_t)sd.channelExtract >= td.format.compCount)
    sd.channelExtract = -1;

  sd.slice.sliceGridWidth = RDCMAX(sd.slice.sliceGridWidth, 1);

  // store sample count so we know how many 'slices' is one real slice
  // multisampled textures cannot have mips, subresource layout is same as would be for mips:
  // [slice0 sample0], [slice0 sample1], [slice1 sample0], [slice1 sample1]
  uint32_t sampleCount = td.msSamp;
  bool multisampled = td.msSamp > 1;

  bool resolveSamples = (sd.sample.sampleIndex == ~0U);

  if(resolveSamples)
  {
    td.msSamp = 1;
    sd.sample.mapToArray = false;
    sd.sample.sampleIndex = 0;
  }

  // treat any multisampled texture as if it were an array
  // of <sample count> dimension (on top of potential existing array
  // dimension). GetTextureData() uses the same convention.
  if(td.msSamp > 1)
  {
    td.arraysize *= td.msSamp;
    td.msSamp = 1;
  }

  if(sd.destType != eFileType_DDS && sd.sample.mapToArray && !sd.slice.slicesAsGrid &&
     sd.slice.sliceIndex == -1)
  {
    sd.sample.mapToArray = false;
    sd.sample.sampleIndex = 0;
  }

  // only DDS supports writing multiple mips, fall back to mip 0 if 'all mips' was specified
  if(sd.destType != eFileType_DDS && sd.mip == -1)
    sd.mip = 0;

  // only DDS supports writing multiple slices, fall back to slice 0 if 'all slices' was specified
  if(sd.destType != eFileType_DDS && sd.slice.sliceIndex == -1 && !sd.slice.slicesAsGrid &&
     !sd.slice.cubeCruciform)
    sd.slice.sliceIndex = 0;

  // fetch source data subresources (typically only one, possibly more
  // if we're writing to DDS (so writing multiple mips/slices) or resolving
  // down a multisampled texture for writing as a single 'image' elsewhere)
  uint32_t sliceOffset = 0;
  uint32_t sliceStride = 1;
  uint32_t numSlices = td.arraysize * td.depth;

  uint32_t mipOffset = 0;
  uint32_t numMips = td.mips;

  bool singleSlice = (sd.slice.sliceIndex != -1);

  // set which slices/mips we need
  if(multisampled)
  {
    bool singleSample = !sd.sample.mapToArray;

    // multisampled images have no mips
    mipOffset = 0;
    numMips = 1;

    if(singleSlice)
    {
      if(singleSample)
      {
        // we want a specific sample in a specific real slice
        sliceOffset = sd.slice.sliceIndex * sampleCount + sd.sample.sampleIndex;
        numSlices = 1;
      }
      else
      {
        // we want all the samples (now mapped to slices) in a specific real slice
        sliceOffset = sd.slice.sliceIndex;
        numSlices = sampleCount;
      }
    }
    else
    {
      if(singleSample)
      {
        // we want one sample in every slice, so we have to set the stride to sampleCount
        // to skip every other sample (mapped to slices), starting from the sample we want
        // in the first real slice
        sliceOffset = sd.sample.sampleIndex;
        sliceStride = sampleCount;
        numSlices = RDCMAX(1U, td.arraysize / sampleCount);
      }
      else
      {
        // we want all slices, all samples
        sliceOffset = 0;
        numSlices = td.arraysize;
      }
    }
  }
  else
  {
    if(singleSlice)
    {
      numSlices = 1;
      sliceOffset = sd.slice.sliceIndex;
    }
    // otherwise take all slices, as by default

    if(sd.mip != -1)
    {
      mipOffset = sd.mip;
      numMips = 1;
    }
    // otherwise take all mips, as by default
  }

  vector<byte *> subdata;

  bool downcast = false;

  // don't support slice mappings for DDS - it supports slices natively
  if(sd.destType == eFileType_DDS)
  {
    sd.slice.cubeCruciform = false;
    sd.slice.slicesAsGrid = false;
  }

  // force downcast to be able to do grid mappings
  if(sd.slice.cubeCruciform || sd.slice.slicesAsGrid)
    downcast = true;

  // we don't support any file formats that handle these block compression formats
  if(td.format.specialFormat == eSpecial_ETC2 || td.format.specialFormat == eSpecial_EAC ||
     td.format.specialFormat == eSpecial_ASTC)
    downcast = true;

  // for DDS don't downcast, for non-HDR always downcast if we're not already RGBA8 unorm
  // for HDR&EXR we can convert from most regular types as well as 10.10.10.2 and 11.11.10
  if((sd.destType != eFileType_DDS && sd.destType != eFileType_HDR && sd.destType != eFileType_EXR &&
      (td.format.compByteWidth != 1 || td.format.compCount != 4 ||
       td.format.compType != eCompType_UNorm || td.format.bgraOrder)) ||
     downcast || (sd.destType != eFileType_DDS && td.format.special &&
                  td.format.specialFormat != eSpecial_R10G10B10A2 &&
                  td.format.specialFormat != eSpecial_R11G11B10))
  {
    downcast = true;
    td.format.compByteWidth = 1;
    td.format.compCount = 4;
    td.format.compType = eCompType_UNorm;
    td.format.special = false;
    td.format.specialFormat = eSpecial_Unknown;
  }

  uint32_t rowPitch = 0;
  uint32_t slicePitch = 0;

  bool blockformat = false;
  int blockSize = 0;
  uint32_t bytesPerPixel = 1;

  td.width = RDCMAX(1U, td.width >> mipOffset);
  td.height = RDCMAX(1U, td.height >> mipOffset);
  td.depth = RDCMAX(1U, td.depth >> mipOffset);

  if(td.format.specialFormat == eSpecial_BC1 || td.format.specialFormat == eSpecial_BC2 ||
     td.format.specialFormat == eSpecial_BC3 || td.format.specialFormat == eSpecial_BC4 ||
     td.format.specialFormat == eSpecial_BC5 || td.format.specialFormat == eSpecial_BC6 ||
     td.format.specialFormat == eSpecial_BC7)
  {
    blockSize = (td.format.specialFormat == eSpecial_BC1 || td.format.specialFormat == eSpecial_BC4)
                    ? 8
                    : 16;
    rowPitch = RDCMAX(1U, ((td.width + 3) / 4)) * blockSize;
    slicePitch = rowPitch * RDCMAX(1U, td.height / 4);
    blockformat = true;
  }
  else
  {
    switch(td.format.specialFormat)
    {
      case eSpecial_S8: bytesPerPixel = 1; break;
      case eSpecial_R10G10B10A2:
      case eSpecial_R9G9B9E5:
      case eSpecial_R11G11B10:
      case eSpecial_D24S8: bytesPerPixel = 4; break;
      case eSpecial_R5G6B5:
      case eSpecial_R5G5B5A1:
      case eSpecial_R4G4B4A4: bytesPerPixel = 2; break;
      case eSpecial_D32S8: bytesPerPixel = 8; break;
      case eSpecial_D16S8:
      case eSpecial_YUV:
      case eSpecial_R4G4:
        RDCERR("Unsupported file format %u", td.format.specialFormat);
        return false;
      default: bytesPerPixel = td.format.compCount * td.format.compByteWidth;
    }

    rowPitch = td.width * bytesPerPixel;
    slicePitch = rowPitch * td.height;
  }

  // loop over fetching subresources
  for(uint32_t s = 0; s < numSlices; s++)
  {
    uint32_t slice = s * sliceStride + sliceOffset;

    for(uint32_t m = 0; m < numMips; m++)
    {
      uint32_t mip = m + mipOffset;

      GetTextureDataParams params;
      params.forDiskSave = true;
      params.typeHint = sd.typeHint;
      params.resolve = resolveSamples;
      params.remap = downcast ? eRemap_RGBA8 : eRemap_None;
      params.blackPoint = sd.comp.blackPoint;
      params.whitePoint = sd.comp.whitePoint;

      size_t datasize = 0;
      byte *bytes = m_pDevice->GetTextureData(liveid, slice, mip, params, datasize);

      if(bytes == NULL)
      {
        RDCERR("Couldn't get bytes for mip %u, slice %u", mip, slice);

        for(size_t i = 0; i < subdata.size(); i++)
          delete[] subdata[i];

        return false;
      }

      if(td.depth == 1)
      {
        subdata.push_back(bytes);
        continue;
      }

      uint32_t mipSlicePitch = slicePitch;

      uint32_t w = RDCMAX(1U, td.width >> m);
      uint32_t h = RDCMAX(1U, td.height >> m);
      uint32_t d = RDCMAX(1U, td.depth >> m);

      if(blockformat)
      {
        mipSlicePitch = RDCMAX(1U, ((w + 3) / 4)) * blockSize * RDCMAX(1U, h / 4);
      }
      else
      {
        mipSlicePitch = w * bytesPerPixel * h;
      }

      // we don't support slice ranges, only all-or-nothing
      // we're also not dealing with multisampled slices if
      // depth > 1. So if we only want one slice out of a 3D texture
      // then make sure we get it
      if(numSlices == 1)
      {
        byte *depthslice = new byte[mipSlicePitch];
        byte *b = bytes + mipSlicePitch * sliceOffset;
        memcpy(depthslice, b, slicePitch);
        subdata.push_back(depthslice);

        delete[] bytes;
        continue;
      }

      s += (d - 1);

      byte *b = bytes;

      // add each depth slice as a separate subdata
      for(uint32_t di = 0; di < d; di++)
      {
        byte *depthslice = new byte[mipSlicePitch];

        memcpy(depthslice, b, mipSlicePitch);

        subdata.push_back(depthslice);

        b += mipSlicePitch;
      }

      delete[] bytes;
    }
  }

  // should have been handled above, but verify incoming data is RGBA8
  if(sd.slice.slicesAsGrid && td.format.compByteWidth == 1 && td.format.compCount == 4)
  {
    uint32_t sliceWidth = td.width;
    uint32_t sliceHeight = td.height;

    uint32_t sliceGridHeight = (td.arraysize * td.depth) / sd.slice.sliceGridWidth;
    if((td.arraysize * td.depth) % sd.slice.sliceGridWidth != 0)
      sliceGridHeight++;

    td.width *= sd.slice.sliceGridWidth;
    td.height *= sliceGridHeight;

    byte *combinedData = new byte[td.width * td.height * td.format.compCount];

    memset(combinedData, 0, td.width * td.height * td.format.compCount);

    for(size_t i = 0; i < subdata.size(); i++)
    {
      uint32_t gridx = (uint32_t)i % sd.slice.sliceGridWidth;
      uint32_t gridy = (uint32_t)i / sd.slice.sliceGridWidth;

      uint32_t yoffs = gridy * sliceHeight;
      uint32_t xoffs = gridx * sliceWidth;

      for(uint32_t y = 0; y < sliceHeight; y++)
      {
        for(uint32_t x = 0; x < sliceWidth; x++)
        {
          uint32_t *srcpix = (uint32_t *)&subdata[i][(y * sliceWidth + x) * 4 + 0];
          uint32_t *dstpix = (uint32_t *)&combinedData[((y + yoffs) * td.width + x + xoffs) * 4 + 0];

          *dstpix = *srcpix;
        }
      }

      delete[] subdata[i];
    }

    subdata.resize(1);
    subdata[0] = combinedData;
    rowPitch = td.width * 4;
  }

  // should have been handled above, but verify incoming data is RGBA8 and 6 slices
  if(sd.slice.cubeCruciform && td.format.compByteWidth == 1 && td.format.compCount == 4 &&
     subdata.size() == 6)
  {
    uint32_t sliceWidth = td.width;
    uint32_t sliceHeight = td.height;

    td.width *= 4;
    td.height *= 3;

    byte *combinedData = new byte[td.width * td.height * td.format.compCount];

    memset(combinedData, 0, td.width * td.height * td.format.compCount);

    /*
     Y X=0   1   2   3
     =     +---+
     0     |+y |
           |[2]|
       +---+---+---+---+
     1 |-x |+z |+x |-z |
       |[1]|[4]|[0]|[5]|
       +---+---+---+---+
     2     |-y |
           |[3]|
           +---+

    */

    uint32_t gridx[6] = {2, 0, 1, 1, 1, 3};
    uint32_t gridy[6] = {1, 1, 0, 2, 1, 1};

    for(size_t i = 0; i < subdata.size(); i++)
    {
      uint32_t yoffs = gridy[i] * sliceHeight;
      uint32_t xoffs = gridx[i] * sliceWidth;

      for(uint32_t y = 0; y < sliceHeight; y++)
      {
        for(uint32_t x = 0; x < sliceWidth; x++)
        {
          uint32_t *srcpix = (uint32_t *)&subdata[i][(y * sliceWidth + x) * 4 + 0];
          uint32_t *dstpix = (uint32_t *)&combinedData[((y + yoffs) * td.width + x + xoffs) * 4 + 0];

          *dstpix = *srcpix;
        }
      }

      delete[] subdata[i];
    }

    subdata.resize(1);
    subdata[0] = combinedData;
    rowPitch = td.width * 4;
  }

  int numComps = td.format.compCount;

  // if we want a grayscale image of one channel, splat it across all channels
  // and set alpha to full
  if(sd.channelExtract >= 0 && td.format.compByteWidth == 1 &&
     (uint32_t)sd.channelExtract < td.format.compCount)
  {
    uint32_t cc = td.format.compCount;

    for(uint32_t y = 0; y < td.height; y++)
    {
      for(uint32_t x = 0; x < td.width; x++)
      {
        subdata[0][(y * td.width + x) * cc + 0] =
            subdata[0][(y * td.width + x) * cc + sd.channelExtract];
        if(cc >= 2)
          subdata[0][(y * td.width + x) * cc + 1] =
              subdata[0][(y * td.width + x) * cc + sd.channelExtract];
        if(cc >= 3)
          subdata[0][(y * td.width + x) * cc + 2] =
              subdata[0][(y * td.width + x) * cc + sd.channelExtract];
        if(cc >= 4)
          subdata[0][(y * td.width + x) * cc + 3] = 255;
      }
    }
  }

  // handle formats that don't support alpha
  if(numComps == 4 && (sd.destType == eFileType_BMP || sd.destType == eFileType_JPG))
  {
    byte *nonalpha = new byte[td.width * td.height * 3];

    for(uint32_t y = 0; y < td.height; y++)
    {
      for(uint32_t x = 0; x < td.width; x++)
      {
        byte r = subdata[0][(y * td.width + x) * 4 + 0];
        byte g = subdata[0][(y * td.width + x) * 4 + 1];
        byte b = subdata[0][(y * td.width + x) * 4 + 2];
        byte a = subdata[0][(y * td.width + x) * 4 + 3];

        if(sd.alpha != eAlphaMap_Discard)
        {
          FloatVector col = sd.alphaCol;
          if(sd.alpha == eAlphaMap_BlendToCheckerboard)
          {
            bool lightSquare = ((x / 64) % 2) == ((y / 64) % 2);
            col = lightSquare ? sd.alphaCol : sd.alphaColSecondary;
          }

          col.x = powf(col.x, 1.0f / 2.2f);
          col.y = powf(col.y, 1.0f / 2.2f);
          col.z = powf(col.z, 1.0f / 2.2f);

          FloatVector pixel = FloatVector(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f,
                                          float(a) / 255.0f);

          pixel.x = pixel.x * pixel.w + col.x * (1.0f - pixel.w);
          pixel.y = pixel.y * pixel.w + col.y * (1.0f - pixel.w);
          pixel.z = pixel.z * pixel.w + col.z * (1.0f - pixel.w);

          r = byte(pixel.x * 255.0f);
          g = byte(pixel.y * 255.0f);
          b = byte(pixel.z * 255.0f);
        }

        nonalpha[(y * td.width + x) * 3 + 0] = r;
        nonalpha[(y * td.width + x) * 3 + 1] = g;
        nonalpha[(y * td.width + x) * 3 + 2] = b;
      }
    }

    delete[] subdata[0];

    subdata[0] = nonalpha;

    numComps = 3;
    rowPitch = td.width * 3;
  }

  // assume that (R,G,0) is better mapping than (Y,A) for 2 component data
  if(numComps == 2 && (sd.destType == eFileType_BMP || sd.destType == eFileType_JPG ||
                       sd.destType == eFileType_PNG || sd.destType == eFileType_TGA))
  {
    byte *rg0 = new byte[td.width * td.height * 3];

    for(uint32_t y = 0; y < td.height; y++)
    {
      for(uint32_t x = 0; x < td.width; x++)
      {
        byte r = subdata[0][(y * td.width + x) * 2 + 0];
        byte g = subdata[0][(y * td.width + x) * 2 + 1];

        rg0[(y * td.width + x) * 3 + 0] = r;
        rg0[(y * td.width + x) * 3 + 1] = g;
        rg0[(y * td.width + x) * 3 + 2] = 0;

        // if we're greyscaling the image, then keep the greyscale here.
        if(sd.channelExtract >= 0)
          rg0[(y * td.width + x) * 3 + 2] = r;
      }
    }

    delete[] subdata[0];

    subdata[0] = rg0;

    numComps = 3;
    rowPitch = td.width * 3;
  }

  FILE *f = FileIO::fopen(path, "wb");

  if(!f)
  {
    success = false;
  }
  else
  {
    if(sd.destType == eFileType_DDS)
    {
      dds_data ddsData;

      ddsData.width = td.width;
      ddsData.height = td.height;
      ddsData.depth = td.depth;
      ddsData.format = td.format;
      ddsData.mips = numMips;
      ddsData.slices = numSlices / td.depth;
      ddsData.subdata = &subdata[0];
      ddsData.cubemap = td.cubemap && numSlices == 6;

      success = write_dds_to_file(f, ddsData);
    }
    else if(sd.destType == eFileType_BMP)
    {
      int ret = stbi_write_bmp_to_func(fileWriteFunc, (void *)f, td.width, td.height, numComps,
                                       subdata[0]);
      success = (ret != 0);
    }
    else if(sd.destType == eFileType_PNG)
    {
      // discard alpha if requested
      for(uint32_t p = 0; sd.alpha == eAlphaMap_Discard && p < td.width * td.height; p++)
        subdata[0][p * 4 + 3] = 255;

      int ret = stbi_write_png_to_func(fileWriteFunc, (void *)f, td.width, td.height, numComps,
                                       subdata[0], rowPitch);
      success = (ret != 0);
    }
    else if(sd.destType == eFileType_TGA)
    {
      // discard alpha if requested
      for(uint32_t p = 0; sd.alpha == eAlphaMap_Discard && p < td.width * td.height; p++)
        subdata[0][p * 4 + 3] = 255;

      int ret = stbi_write_tga_to_func(fileWriteFunc, (void *)f, td.width, td.height, numComps,
                                       subdata[0]);
      success = (ret != 0);
    }
    else if(sd.destType == eFileType_JPG)
    {
      jpge::params p;
      p.m_quality = sd.jpegQuality;

      int len = td.width * td.height * td.format.compCount;
      // ensure buffer is at least 1024
      if(len < 1024)
        len = 1024;

      char *jpgdst = new char[len];

      success = jpge::compress_image_to_jpeg_file_in_memory(jpgdst, len, td.width, td.height,
                                                            numComps, subdata[0], p);

      if(success)
        fwrite(jpgdst, 1, len, f);

      delete[] jpgdst;
    }
    else if(sd.destType == eFileType_HDR || sd.destType == eFileType_EXR)
    {
      float *fldata = NULL;
      float *abgr[4] = {NULL, NULL, NULL, NULL};

      if(sd.destType == eFileType_HDR)
      {
        fldata = new float[td.width * td.height * 4];
      }
      else
      {
        abgr[0] = new float[td.width * td.height];
        abgr[1] = new float[td.width * td.height];
        abgr[2] = new float[td.width * td.height];
        abgr[3] = new float[td.width * td.height];
      }

      byte *srcData = subdata[0];

      ResourceFormat saveFmt = td.format;
      if(saveFmt.compType == eCompType_None)
        saveFmt.compType = sd.typeHint;
      if(saveFmt.compType == eCompType_None)
        saveFmt.compType = saveFmt.compByteWidth == 4 ? eCompType_Float : eCompType_UNorm;

      for(uint32_t y = 0; y < td.height; y++)
      {
        for(uint32_t x = 0; x < td.width; x++)
        {
          float r = 0.0f;
          float g = 0.0f;
          float b = 0.0f;
          float a = 1.0f;

          if(saveFmt.special && saveFmt.specialFormat == eSpecial_R10G10B10A2)
          {
            uint32_t *u32 = (uint32_t *)srcData;

            Vec4f vec = ConvertFromR10G10B10A2(*u32);

            r = vec.x;
            g = vec.y;
            b = vec.z;
            a = vec.w;

            srcData += 4;
          }
          else if(saveFmt.special && saveFmt.specialFormat == eSpecial_R11G11B10)
          {
            uint32_t *u32 = (uint32_t *)srcData;

            Vec3f vec = ConvertFromR11G11B10(*u32);

            r = vec.x;
            g = vec.y;
            b = vec.z;
            a = 1.0f;

            srcData += 4;
          }
          else
          {
            if(saveFmt.compCount >= 1)
              r = ConvertComponent(saveFmt, srcData + saveFmt.compByteWidth * 0);
            if(saveFmt.compCount >= 2)
              g = ConvertComponent(saveFmt, srcData + saveFmt.compByteWidth * 1);
            if(saveFmt.compCount >= 3)
              b = ConvertComponent(saveFmt, srcData + saveFmt.compByteWidth * 2);
            if(saveFmt.compCount >= 4)
              a = ConvertComponent(saveFmt, srcData + saveFmt.compByteWidth * 3);

            srcData += saveFmt.compCount * saveFmt.compByteWidth;
          }

          if(saveFmt.bgraOrder)
            std::swap(r, b);

          // HDR can't represent negative values
          if(sd.destType == eFileType_HDR)
          {
            r = RDCMAX(r, 0.0f);
            g = RDCMAX(g, 0.0f);
            b = RDCMAX(b, 0.0f);
            a = RDCMAX(a, 0.0f);
          }

          if(sd.channelExtract == 0)
          {
            g = b = r;
            a = 1.0f;
          }
          if(sd.channelExtract == 1)
          {
            r = b = g;
            a = 1.0f;
          }
          if(sd.channelExtract == 2)
          {
            r = g = b;
            a = 1.0f;
          }
          if(sd.channelExtract == 3)
          {
            r = g = b = a;
            a = 1.0f;
          }

          if(fldata)
          {
            fldata[(y * td.width + x) * 4 + 0] = r;
            fldata[(y * td.width + x) * 4 + 1] = g;
            fldata[(y * td.width + x) * 4 + 2] = b;
            fldata[(y * td.width + x) * 4 + 3] = a;
          }
          else
          {
            abgr[0][(y * td.width + x)] = a;
            abgr[1][(y * td.width + x)] = b;
            abgr[2][(y * td.width + x)] = g;
            abgr[3][(y * td.width + x)] = r;
          }
        }
      }

      if(sd.destType == eFileType_HDR)
      {
        int ret = stbi_write_hdr_to_func(fileWriteFunc, (void *)f, td.width, td.height, 4, fldata);
        success = (ret != 0);
      }
      else if(sd.destType == eFileType_EXR)
      {
        const char *err = NULL;

        EXRImage exrImage;
        InitEXRImage(&exrImage);

        int pixTypes[4] = {TINYEXR_PIXELTYPE_FLOAT, TINYEXR_PIXELTYPE_FLOAT,
                           TINYEXR_PIXELTYPE_FLOAT, TINYEXR_PIXELTYPE_FLOAT};
        int reqTypes[4] = {TINYEXR_PIXELTYPE_HALF, TINYEXR_PIXELTYPE_HALF, TINYEXR_PIXELTYPE_HALF,
                           TINYEXR_PIXELTYPE_HALF};

        // must be in this order as many viewers don't pay attention to channels and just assume
        // they are in this order
        const char *bgraNames[4] = {"A", "B", "G", "R"};

        exrImage.num_channels = 4;
        exrImage.channel_names = bgraNames;
        exrImage.images = (unsigned char **)abgr;
        exrImage.width = td.width;
        exrImage.height = td.height;
        exrImage.pixel_types = pixTypes;
        exrImage.requested_pixel_types = reqTypes;

        unsigned char *mem = NULL;

        size_t ret = SaveMultiChannelEXRToMemory(&exrImage, &mem, &err);

        success = (ret > 0);
        if(success)
          FileIO::fwrite(mem, 1, ret, f);
        else
          RDCERR("Error saving EXR file %d: '%s'", ret, err);

        free(mem);
      }

      if(fldata)
      {
        delete[] fldata;
      }
      else
      {
        delete[] abgr[0];
        delete[] abgr[1];
        delete[] abgr[2];
        delete[] abgr[3];
      }
    }

    FileIO::fclose(f);
  }

  for(size_t i = 0; i < subdata.size(); i++)
    delete[] subdata[i];

  return success;
}

bool ReplayRenderer::PixelHistory(ResourceId target, uint32_t x, uint32_t y, uint32_t slice,
                                  uint32_t mip, uint32_t sampleIdx, FormatComponentType typeHint,
                                  rdctype::array<PixelModification> *history)
{
  for(size_t t = 0; t < m_Textures.size(); t++)
  {
    if(m_Textures[t].ID == target)
    {
      if(x >= m_Textures[t].width || y >= m_Textures[t].height)
      {
        RDCDEBUG("PixelHistory out of bounds on %llu (%u,%u) vs (%u,%u)", target, x, y,
                 m_Textures[t].width, m_Textures[t].height);
        history->count = 0;
        history->elems = NULL;
        return false;
      }

      if(m_Textures[t].msSamp == 1)
        sampleIdx = ~0U;

      slice = RDCCLAMP(slice, 0U, m_Textures[t].arraysize);
      mip = RDCCLAMP(mip, 0U, m_Textures[t].mips);

      break;
    }
  }

  auto usage = m_pDevice->GetUsage(m_pDevice->GetLiveID(target));

  vector<EventUsage> events;

  for(size_t i = 0; i < usage.size(); i++)
  {
    if(usage[i].eventID > m_EventID)
      continue;

    switch(usage[i].usage)
    {
      case eUsage_VertexBuffer:
      case eUsage_IndexBuffer:
      case eUsage_VS_Constants:
      case eUsage_HS_Constants:
      case eUsage_DS_Constants:
      case eUsage_GS_Constants:
      case eUsage_PS_Constants:
      case eUsage_CS_Constants:
      case eUsage_All_Constants:
      case eUsage_VS_Resource:
      case eUsage_HS_Resource:
      case eUsage_DS_Resource:
      case eUsage_GS_Resource:
      case eUsage_PS_Resource:
      case eUsage_CS_Resource:
      case eUsage_All_Resource:
      case eUsage_InputTarget:
      case eUsage_CopySrc:
      case eUsage_ResolveSrc:
      case eUsage_Barrier:
      case eUsage_Indirect:
        // read-only, not a valid pixel history event
        continue;

      case eUsage_None:
      case eUsage_SO:
      case eUsage_VS_RWResource:
      case eUsage_HS_RWResource:
      case eUsage_DS_RWResource:
      case eUsage_GS_RWResource:
      case eUsage_PS_RWResource:
      case eUsage_CS_RWResource:
      case eUsage_All_RWResource:
      case eUsage_ColourTarget:
      case eUsage_DepthStencilTarget:
      case eUsage_Clear:
      case eUsage_Copy:
      case eUsage_CopyDst:
      case eUsage_Resolve:
      case eUsage_ResolveDst:
      case eUsage_GenMips:
        // writing - include in pixel history events
        break;
    }

    events.push_back(usage[i]);
  }

  if(events.empty())
  {
    RDCDEBUG("Target %llu not written to before %u", target, m_EventID);
    history->count = 0;
    history->elems = NULL;
    return false;
  }

  *history = m_pDevice->PixelHistory(events, m_pDevice->GetLiveID(target), x, y, slice, mip,
                                     sampleIdx, typeHint);

  SetFrameEvent(m_EventID, true);

  return true;
}

bool ReplayRenderer::DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx,
                                 uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace)
{
  if(trace == NULL)
    return false;

  *trace = m_pDevice->DebugVertex(m_EventID, vertid, instid, idx, instOffset, vertOffset);

  SetFrameEvent(m_EventID, true);

  return true;
}

bool ReplayRenderer::DebugPixel(uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive,
                                ShaderDebugTrace *trace)
{
  if(trace == NULL)
    return false;

  *trace = m_pDevice->DebugPixel(m_EventID, x, y, sample, primitive);

  SetFrameEvent(m_EventID, true);

  return true;
}

bool ReplayRenderer::DebugThread(uint32_t groupid[3], uint32_t threadid[3], ShaderDebugTrace *trace)
{
  if(trace == NULL)
    return false;

  *trace = m_pDevice->DebugThread(m_EventID, groupid, threadid);

  SetFrameEvent(m_EventID, true);

  return true;
}

bool ReplayRenderer::GetCBufferVariableContents(ResourceId shader, const char *entryPoint,
                                                uint32_t cbufslot, ResourceId buffer, uint64_t offs,
                                                rdctype::array<ShaderVariable> *vars)
{
  if(vars == NULL)
    return false;

  vector<byte> data;
  if(buffer != ResourceId())
    m_pDevice->GetBufferData(m_pDevice->GetLiveID(buffer), offs, 0, data);

  vector<ShaderVariable> v;

  m_pDevice->FillCBufferVariables(m_pDevice->GetLiveID(shader), entryPoint, cbufslot, v, data);

  *vars = v;

  return true;
}

void ReplayRenderer::GetSupportedWindowSystems(rdctype::array<WindowingSystem> *systems)
{
  if(systems)
    *systems = m_pDevice->GetSupportedWindowSystems();
}

ReplayOutput *ReplayRenderer::CreateOutput(WindowingSystem system, void *data, OutputType type)
{
  ReplayOutput *out = new ReplayOutput(this, system, data, type);

  m_Outputs.push_back(out);

  m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);

  out->SetFrameEvent(m_EventID);

  m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);

  return out;
}

void ReplayRenderer::ShutdownOutput(ReplayOutput *output)
{
  RDCUNIMPLEMENTED("Shutting down individual outputs");
}

void ReplayRenderer::Shutdown()
{
  delete this;
}

ResourceId ReplayRenderer::BuildTargetShader(const char *entry, const char *source,
                                             const uint32_t compileFlags, ShaderStageType type,
                                             rdctype::str *errors)
{
  ResourceId id;
  string errs;

  switch(type)
  {
    case eShaderStage_Vertex:
    case eShaderStage_Hull:
    case eShaderStage_Domain:
    case eShaderStage_Geometry:
    case eShaderStage_Pixel:
    case eShaderStage_Compute: break;
    default: RDCERR("Unexpected type in BuildShader!"); return ResourceId();
  }

  m_pDevice->BuildTargetShader(source, entry, compileFlags, type, &id, &errs);

  if(id != ResourceId())
    m_TargetResources.insert(id);

  if(errors)
    *errors = errs;

  return id;
}

ResourceId ReplayRenderer::BuildCustomShader(const char *entry, const char *source,
                                             const uint32_t compileFlags, ShaderStageType type,
                                             rdctype::str *errors)
{
  ResourceId id;
  string errs;

  switch(type)
  {
    case eShaderStage_Vertex:
    case eShaderStage_Hull:
    case eShaderStage_Domain:
    case eShaderStage_Geometry:
    case eShaderStage_Pixel:
    case eShaderStage_Compute: break;
    default: RDCERR("Unexpected type in BuildShader!"); return ResourceId();
  }

  m_pDevice->BuildCustomShader(source, entry, compileFlags, type, &id, &errs);

  if(id != ResourceId())
    m_CustomShaders.insert(id);

  if(errors)
    *errors = errs;

  return id;
}

bool ReplayRenderer::FreeTargetResource(ResourceId id)
{
  m_TargetResources.erase(id);
  m_pDevice->FreeTargetResource(id);

  return true;
}

bool ReplayRenderer::FreeCustomShader(ResourceId id)
{
  m_CustomShaders.erase(id);
  m_pDevice->FreeCustomShader(id);

  return true;
}

bool ReplayRenderer::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDevice->ReplaceResource(from, to);

  SetFrameEvent(m_EventID, true);

  for(size_t i = 0; i < m_Outputs.size(); i++)
    if(m_Outputs[i]->GetType() != eOutputType_None)
      m_Outputs[i]->Display();

  return true;
}

bool ReplayRenderer::RemoveReplacement(ResourceId id)
{
  m_pDevice->RemoveReplacement(id);

  SetFrameEvent(m_EventID, true);

  for(size_t i = 0; i < m_Outputs.size(); i++)
    if(m_Outputs[i]->GetType() != eOutputType_None)
      m_Outputs[i]->Display();

  return true;
}

ReplayCreateStatus ReplayRenderer::CreateDevice(const char *logfile)
{
  RDCLOG("Creating replay device for %s", logfile);

  RDCDriver driverType = RDC_Unknown;
  string driverName = "";
  uint64_t fileMachineIdent = 0;
  auto status =
      RenderDoc::Inst().FillInitParams(logfile, driverType, driverName, fileMachineIdent, NULL);

  if(driverType == RDC_Unknown || driverName == "" || status != eReplayCreate_Success)
  {
    RDCERR("Couldn't get device type from log");
    return status;
  }

  IReplayDriver *driver = NULL;
  status = RenderDoc::Inst().CreateReplayDriver(driverType, logfile, &driver);

  if(driver && status == eReplayCreate_Success)
  {
    RDCLOG("Created replay driver.");
    return PostCreateInit(driver);
  }

  RDCERR("Couldn't create a replay device :(.");
  return status;
}

ReplayCreateStatus ReplayRenderer::SetDevice(IReplayDriver *device)
{
  if(device)
  {
    RDCLOG("Got replay driver.");
    return PostCreateInit(device);
  }

  RDCERR("Given invalid replay driver.");
  return eReplayCreate_InternalError;
}

ReplayCreateStatus ReplayRenderer::PostCreateInit(IReplayDriver *device)
{
  m_pDevice = device;

  m_pDevice->ReadLogInitialisation();

  FetchPipelineState();

  FetchFrameRecord fr = m_pDevice->GetFrameRecord();

  m_FrameRecord.frameInfo = fr.frameInfo;
  m_FrameRecord.m_DrawCallList = fr.drawcallList;
  SetupDrawcallPointers(&m_Drawcalls, m_FrameRecord.m_DrawCallList, NULL, NULL);

  return eReplayCreate_Success;
}

void ReplayRenderer::FileChanged()
{
  m_pDevice->FileChanged();
}

bool ReplayRenderer::HasCallstacks()
{
  return m_pDevice->HasCallstacks();
}

APIProperties ReplayRenderer::GetAPIProperties()
{
  return m_pDevice->GetAPIProperties();
}

bool ReplayRenderer::InitResolver()
{
  m_pDevice->InitCallstackResolver();
  return m_pDevice->GetCallstackResolver() != NULL;
}

void ReplayRenderer::FetchPipelineState()
{
  m_pDevice->SavePipelineState();

  m_D3D11PipelineState = m_pDevice->GetD3D11PipelineState();
  m_D3D12PipelineState = m_pDevice->GetD3D12PipelineState();
  m_GLPipelineState = m_pDevice->GetGLPipelineState();
  m_VulkanPipelineState = m_pDevice->GetVulkanPipelineState();

  {
    D3D11PipelineState::ShaderStage *stages[] = {
        &m_D3D11PipelineState.m_VS, &m_D3D11PipelineState.m_HS, &m_D3D11PipelineState.m_DS,
        &m_D3D11PipelineState.m_GS, &m_D3D11PipelineState.m_PS, &m_D3D11PipelineState.m_CS,
    };

    for(int i = 0; i < 6; i++)
      if(stages[i]->Shader != ResourceId())
        stages[i]->ShaderDetails = m_pDevice->GetShader(m_pDevice->GetLiveID(stages[i]->Shader), "");
  }

  {
    D3D12PipelineState::ShaderStage *stages[] = {
        &m_D3D12PipelineState.m_VS, &m_D3D12PipelineState.m_HS, &m_D3D12PipelineState.m_DS,
        &m_D3D12PipelineState.m_GS, &m_D3D12PipelineState.m_PS, &m_D3D12PipelineState.m_CS,
    };

    for(int i = 0; i < 6; i++)
      if(stages[i]->Shader != ResourceId())
        stages[i]->ShaderDetails = m_pDevice->GetShader(m_pDevice->GetLiveID(stages[i]->Shader), "");
  }

  {
    GLPipelineState::ShaderStage *stages[] = {
        &m_GLPipelineState.m_VS, &m_GLPipelineState.m_TCS, &m_GLPipelineState.m_TES,
        &m_GLPipelineState.m_GS, &m_GLPipelineState.m_FS,  &m_GLPipelineState.m_CS,
    };

    for(int i = 0; i < 6; i++)
      if(stages[i]->Shader != ResourceId())
        stages[i]->ShaderDetails = m_pDevice->GetShader(m_pDevice->GetLiveID(stages[i]->Shader), "");
  }

  {
    VulkanPipelineState::ShaderStage *stages[] = {
        &m_VulkanPipelineState.m_VS, &m_VulkanPipelineState.m_TCS, &m_VulkanPipelineState.m_TES,
        &m_VulkanPipelineState.m_GS, &m_VulkanPipelineState.m_FS,  &m_VulkanPipelineState.m_CS,
    };

    for(int i = 0; i < 6; i++)
      if(stages[i]->Shader != ResourceId())
        stages[i]->ShaderDetails = m_pDevice->GetShader(m_pDevice->GetLiveID(stages[i]->Shader),
                                                        stages[i]->entryPoint.elems);
  }
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_GetAPIProperties(ReplayRenderer *rend,
                                                                           APIProperties *props)
{
  if(props)
    *props = rend->GetAPIProperties();
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_GetSupportedWindowSystems(
    ReplayRenderer *rend, rdctype::array<WindowingSystem> *systems)
{
  rend->GetSupportedWindowSystems(systems);
}

extern "C" RENDERDOC_API ReplayOutput *RENDERDOC_CC ReplayRenderer_CreateOutput(
    ReplayRenderer *rend, WindowingSystem system, void *data, OutputType type)
{
  return rend->CreateOutput(system, data, type);
}
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_Shutdown(ReplayRenderer *rend)
{
  rend->Shutdown();
}
extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_ShutdownOutput(ReplayRenderer *rend,
                                                                         ReplayOutput *output)
{
  rend->ShutdownOutput(output);
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_FileChanged(ReplayRenderer *rend)
{
  rend->FileChanged();
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_HasCallstacks(ReplayRenderer *rend)
{
  return rend->HasCallstacks();
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_InitResolver(ReplayRenderer *rend)
{
  return rend->InitResolver();
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SetFrameEvent(ReplayRenderer *rend,
                                                                          uint32_t eventID,
                                                                          bool32 force)
{
  return rend->SetFrameEvent(eventID, force != 0);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetD3D11PipelineState(ReplayRenderer *rend, D3D11PipelineState *state)
{
  return rend->GetD3D11PipelineState(state);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetD3D12PipelineState(ReplayRenderer *rend, D3D12PipelineState *state)
{
  return rend->GetD3D12PipelineState(state);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetGLPipelineState(ReplayRenderer *rend,
                                                                               GLPipelineState *state)
{
  return rend->GetGLPipelineState(state);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetVulkanPipelineState(ReplayRenderer *rend, VulkanPipelineState *state)
{
  return rend->GetVulkanPipelineState(state);
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_BuildCustomShader(
    ReplayRenderer *rend, const char *entry, const char *source, const uint32_t compileFlags,
    ShaderStageType type, ResourceId *shaderID, rdctype::str *errors)
{
  if(shaderID == NULL)
    return;

  *shaderID = rend->BuildCustomShader(entry, source, compileFlags, type, errors);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FreeCustomShader(ReplayRenderer *rend,
                                                                             ResourceId id)
{
  return rend->FreeCustomShader(id);
}

extern "C" RENDERDOC_API void RENDERDOC_CC ReplayRenderer_BuildTargetShader(
    ReplayRenderer *rend, const char *entry, const char *source, const uint32_t compileFlags,
    ShaderStageType type, ResourceId *shaderID, rdctype::str *errors)
{
  if(shaderID == NULL)
    return;

  *shaderID = rend->BuildTargetShader(entry, source, compileFlags, type, errors);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_ReplaceResource(ReplayRenderer *rend,
                                                                            ResourceId from,
                                                                            ResourceId to)
{
  return rend->ReplaceResource(from, to);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_RemoveReplacement(ReplayRenderer *rend,
                                                                              ResourceId id)
{
  return rend->RemoveReplacement(id);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_FreeTargetResource(ReplayRenderer *rend,
                                                                               ResourceId id)
{
  return rend->FreeTargetResource(id);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetFrameInfo(ReplayRenderer *rend,
                                                                         FetchFrameInfo *frame)
{
  return rend->GetFrameInfo(frame);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetDrawcalls(ReplayRenderer *rend, rdctype::array<FetchDrawcall> *draws)
{
  return rend->GetDrawcalls(draws);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_FetchCounters(ReplayRenderer *rend, uint32_t *counters, uint32_t numCounters,
                             rdctype::array<CounterResult> *results)
{
  return rend->FetchCounters(counters, numCounters, results);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_EnumerateCounters(ReplayRenderer *rend, rdctype::array<uint32_t> *counters)
{
  return rend->EnumerateCounters(counters);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DescribeCounter(ReplayRenderer *rend,
                                                                            uint32_t counterID,
                                                                            CounterDescription *desc)
{
  return rend->DescribeCounter(counterID, desc);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetTextures(ReplayRenderer *rend, rdctype::array<FetchTexture> *texs)
{
  return rend->GetTextures(texs);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetBuffers(ReplayRenderer *rend, rdctype::array<FetchBuffer> *bufs)
{
  return rend->GetBuffers(bufs);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetResolve(ReplayRenderer *rend, uint64_t *callstack, uint32_t callstackLen,
                          rdctype::array<rdctype::str> *trace)
{
  return rend->GetResolve(callstack, callstackLen, trace);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetDebugMessages(ReplayRenderer *rend, rdctype::array<DebugMessage> *msgs)
{
  return rend->GetDebugMessages(msgs);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_PixelHistory(
    ReplayRenderer *rend, ResourceId target, uint32_t x, uint32_t y, uint32_t slice, uint32_t mip,
    uint32_t sampleIdx, FormatComponentType typeHint, rdctype::array<PixelModification> *history)
{
  return rend->PixelHistory(target, x, y, slice, mip, sampleIdx, typeHint, history);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_DebugVertex(ReplayRenderer *rend, uint32_t vertid, uint32_t instid, uint32_t idx,
                           uint32_t instOffset, uint32_t vertOffset, ShaderDebugTrace *trace)
{
  return rend->DebugVertex(vertid, instid, idx, instOffset, vertOffset, trace);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugPixel(ReplayRenderer *rend,
                                                                       uint32_t x, uint32_t y,
                                                                       uint32_t sample,
                                                                       uint32_t primitive,
                                                                       ShaderDebugTrace *trace)
{
  return rend->DebugPixel(x, y, sample, primitive, trace);
}
extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_DebugThread(ReplayRenderer *rend,
                                                                        uint32_t groupid[3],
                                                                        uint32_t threadid[3],
                                                                        ShaderDebugTrace *trace)
{
  return rend->DebugThread(groupid, threadid, trace);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC
ReplayRenderer_GetUsage(ReplayRenderer *rend, ResourceId id, rdctype::array<EventUsage> *usage)
{
  return rend->GetUsage(id, usage);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetCBufferVariableContents(
    ReplayRenderer *rend, ResourceId shader, const char *entryPoint, uint32_t cbufslot,
    ResourceId buffer, uint64_t offs, rdctype::array<ShaderVariable> *vars)
{
  return rend->GetCBufferVariableContents(shader, entryPoint, cbufslot, buffer, offs, vars);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_SaveTexture(ReplayRenderer *rend,
                                                                        const TextureSave &saveData,
                                                                        const char *path)
{
  return rend->SaveTexture(saveData, path);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetPostVSData(ReplayRenderer *rend,
                                                                          uint32_t instID,
                                                                          MeshDataStage stage,
                                                                          MeshFormat *data)
{
  return rend->GetPostVSData(instID, stage, data);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetBufferData(
    ReplayRenderer *rend, ResourceId buff, uint64_t offset, uint64_t len, rdctype::array<byte> *data)
{
  return rend->GetBufferData(buff, offset, len, data);
}

extern "C" RENDERDOC_API bool32 RENDERDOC_CC ReplayRenderer_GetTextureData(
    ReplayRenderer *rend, ResourceId tex, uint32_t arrayIdx, uint32_t mip, rdctype::array<byte> *data)
{
  return rend->GetTextureData(tex, arrayIdx, mip, data);
}
