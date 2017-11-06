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

#include "replay_controller.h"
#include <string.h>
#include <time.h>
#include "common/dds_readwrite.h"
#include "jpeg-compressor/jpgd.h"
#include "jpeg-compressor/jpge.h"
#include "maths/formatpacking.h"
#include "os/os_specific.h"
#include "serialise/rdcfile.h"
#include "serialise/serialiser.h"
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"
#include "strings/string_utils.h"
#include "tinyexr/tinyexr.h"

float ConvertComponent(const ResourceFormat &fmt, byte *data)
{
  if(fmt.compByteWidth == 8)
  {
    // we just downcast
    uint64_t *u64 = (uint64_t *)data;
    int64_t *i64 = (int64_t *)data;

    if(fmt.compType == CompType::Double || fmt.compType == CompType::Float)
    {
      return float(*(double *)u64);
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u64);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i64);
    }
  }
  else if(fmt.compByteWidth == 4)
  {
    uint32_t *u32 = (uint32_t *)data;
    int32_t *i32 = (int32_t *)data;

    if(fmt.compType == CompType::Float || fmt.compType == CompType::Depth)
    {
      return *(float *)u32;
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u32);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i32);
    }
  }
  else if(fmt.compByteWidth == 3 && fmt.compType == CompType::Depth)
  {
    // 24-bit depth is a weird edge case we need to assemble it by hand
    uint8_t *u8 = (uint8_t *)data;

    uint32_t depth = 0;
    depth |= uint32_t(u8[1]);
    depth |= uint32_t(u8[2]) << 8;
    depth |= uint32_t(u8[3]) << 16;

    return float(depth) / float(16777215.0f);
  }
  else if(fmt.compByteWidth == 2)
  {
    uint16_t *u16 = (uint16_t *)data;
    int16_t *i16 = (int16_t *)data;

    if(fmt.compType == CompType::Float)
    {
      return ConvertFromHalf(*u16);
    }
    else if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u16);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i16);
    }
    // 16-bit depth is UNORM
    else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::Depth)
    {
      return float(*u16) / 65535.0f;
    }
    else if(fmt.compType == CompType::SNorm)
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

    if(fmt.compType == CompType::UInt || fmt.compType == CompType::UScaled)
    {
      return float(*u8);
    }
    else if(fmt.compType == CompType::SInt || fmt.compType == CompType::SScaled)
    {
      return float(*i8);
    }
    else if(fmt.compType == CompType::UNorm)
    {
      if(fmt.srgbCorrected)
        return SRGB8_lookuptable[*u8];
      else
        return float(*u8) / 255.0f;
    }
    else if(fmt.compType == CompType::SNorm)
    {
      float f = -1.0f;

      if(*i8 == -128)
        f = -1.0f;
      else
        f = ((float)*i8) / 127.0f;

      return f;
    }
  }

  RDCERR("Unexpected format to convert from %u %u", fmt.compByteWidth, fmt.compType);

  return 0.0f;
}

static void fileWriteFunc(void *context, void *data, int size)
{
  FileIO::fwrite(data, 1, size, (FILE *)context);
}

ReplayController::ReplayController()
{
  m_pDevice = NULL;

  m_EventID = 100000;
}

ReplayController::~ReplayController()
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

void ReplayController::SetFrameEvent(uint32_t eventID, bool force)
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
}

const D3D11Pipe::State &ReplayController::GetD3D11PipelineState()
{
  return *m_D3D11PipelineState;
}

const D3D12Pipe::State &ReplayController::GetD3D12PipelineState()
{
  return *m_D3D12PipelineState;
}

const GLPipe::State &ReplayController::GetGLPipelineState()
{
  return *m_GLPipelineState;
}

const VKPipe::State &ReplayController::GetVulkanPipelineState()
{
  return *m_VulkanPipelineState;
}

rdcarray<rdcstr> ReplayController::GetDisassemblyTargets()
{
  rdcarray<rdcstr> ret;

  vector<string> targets = m_pDevice->GetDisassemblyTargets();

  ret.reserve(targets.size());
  for(const std::string &t : targets)
    ret.push_back(t);

  return ret;
}

rdcstr ReplayController::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                           const char *target)
{
  return m_pDevice->DisassembleShader(pipeline, refl, target);
}

FrameDescription ReplayController::GetFrameInfo()
{
  return m_FrameRecord.frameInfo;
}

DrawcallDescription *ReplayController::GetDrawcallByEID(uint32_t eventID)
{
  if(eventID >= m_Drawcalls.size())
    return NULL;

  return m_Drawcalls[eventID];
}

rdcarray<DrawcallDescription> ReplayController::GetDrawcalls()
{
  return m_FrameRecord.drawcallList;
}

rdcarray<CounterResult> ReplayController::FetchCounters(const rdcarray<GPUCounter> &counters)
{
  std::vector<GPUCounter> counterArray(counters.begin(), counters.end());

  return m_pDevice->FetchCounters(counterArray);
}

rdcarray<GPUCounter> ReplayController::EnumerateCounters()
{
  return m_pDevice->EnumerateCounters();
}

CounterDescription ReplayController::DescribeCounter(GPUCounter counterID)
{
  return m_pDevice->DescribeCounter(counterID);
}

rdcarray<BufferDescription> ReplayController::GetBuffers()
{
  if(m_Buffers.empty())
  {
    vector<ResourceId> ids = m_pDevice->GetBuffers();

    m_Buffers.resize(ids.size());

    for(size_t i = 0; i < ids.size(); i++)
      m_Buffers[i] = m_pDevice->GetBuffer(ids[i]);
  }

  return m_Buffers;
}

rdcarray<TextureDescription> ReplayController::GetTextures()
{
  if(m_Textures.empty())
  {
    vector<ResourceId> ids = m_pDevice->GetTextures();

    m_Textures.resize(ids.size());

    for(size_t i = 0; i < ids.size(); i++)
      m_Textures[i] = m_pDevice->GetTexture(ids[i]);
  }

  return m_Textures;
}

rdcarray<DebugMessage> ReplayController::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

rdcarray<EventUsage> ReplayController::GetUsage(ResourceId id)
{
  return m_pDevice->GetUsage(m_pDevice->GetLiveID(id));
}

MeshFormat ReplayController::GetPostVSData(uint32_t instID, MeshDataStage stage)
{
  DrawcallDescription *draw = GetDrawcallByEID(m_EventID);

  MeshFormat ret;
  RDCEraseEl(ret);

  if(draw == NULL || !(draw->flags & DrawFlags::Drawcall))
    return MeshFormat();

  instID = RDCMIN(instID, draw->numInstances - 1);

  return m_pDevice->GetPostVSBuffers(draw->eventID, instID, stage);
}

bytebuf ReplayController::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len)
{
  if(buff == ResourceId())
    return bytebuf();

  ResourceId liveId = m_pDevice->GetLiveID(buff);

  if(liveId == ResourceId())
  {
    RDCERR("Couldn't get Live ID for %llu getting buffer data", buff);
    return bytebuf();
  }

  vector<byte> retData;
  m_pDevice->GetBufferData(liveId, offset, len, retData);

  return retData;
}

bytebuf ReplayController::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip)
{
  bytebuf ret;

  ResourceId liveId = m_pDevice->GetLiveID(tex);

  if(liveId == ResourceId())
  {
    RDCERR("Couldn't get Live ID for %llu getting texture data", tex);
    return ret;
  }

  m_pDevice->GetTextureData(liveId, arrayIdx, mip, GetTextureDataParams(), ret);

  return ret;
}

bool ReplayController::SaveTexture(const TextureSave &saveData, const char *path)
{
  TextureSave sd = saveData;    // mutable copy
  ResourceId liveid = m_pDevice->GetLiveID(sd.id);
  TextureDescription td = m_pDevice->GetTexture(liveid);

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

  if(sd.destType != FileType::DDS && sd.sample.mapToArray && !sd.slice.slicesAsGrid &&
     sd.slice.sliceIndex == -1)
  {
    sd.sample.mapToArray = false;
    sd.sample.sampleIndex = 0;
  }

  // only DDS supports writing multiple mips, fall back to mip 0 if 'all mips' was specified
  if(sd.destType != FileType::DDS && sd.mip == -1)
    sd.mip = 0;

  // only DDS supports writing multiple slices, fall back to slice 0 if 'all slices' was specified
  if(sd.destType != FileType::DDS && sd.slice.sliceIndex == -1 && !sd.slice.slicesAsGrid &&
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
  if(sd.destType == FileType::DDS)
  {
    sd.slice.cubeCruciform = false;
    sd.slice.slicesAsGrid = false;
  }

  // force downcast to be able to do grid mappings
  if(sd.slice.cubeCruciform || sd.slice.slicesAsGrid)
    downcast = true;

  // we don't support any file formats that handle these block compression formats
  if(td.format.type == ResourceFormatType::ETC2 || td.format.type == ResourceFormatType::EAC ||
     td.format.type == ResourceFormatType::ASTC)
    downcast = true;

  // for DDS don't downcast, for non-HDR always downcast if we're not already RGBA8 unorm
  // for HDR&EXR we can convert from most regular types as well as 10.10.10.2 and 11.11.10
  if((sd.destType != FileType::DDS && sd.destType != FileType::HDR && sd.destType != FileType::EXR &&
      (td.format.compByteWidth != 1 || td.format.compCount != 4 ||
       td.format.compType != CompType::UNorm || td.format.bgraOrder)) ||
     downcast || (sd.destType != FileType::DDS && td.format.Special() &&
                  td.format.type != ResourceFormatType::R10G10B10A2 &&
                  td.format.type != ResourceFormatType::R11G11B10))
  {
    downcast = true;
    td.format.compByteWidth = 1;
    td.format.compCount = 4;
    td.format.compType = CompType::UNorm;
    td.format.type = ResourceFormatType::Regular;
  }

  uint32_t rowPitch = 0;
  uint32_t slicePitch = 0;

  bool blockformat = false;
  int blockSize = 0;
  uint32_t bytesPerPixel = 1;

  td.width = RDCMAX(1U, td.width >> mipOffset);
  td.height = RDCMAX(1U, td.height >> mipOffset);
  td.depth = RDCMAX(1U, td.depth >> mipOffset);

  if(td.format.type == ResourceFormatType::BC1 || td.format.type == ResourceFormatType::BC2 ||
     td.format.type == ResourceFormatType::BC3 || td.format.type == ResourceFormatType::BC4 ||
     td.format.type == ResourceFormatType::BC5 || td.format.type == ResourceFormatType::BC6 ||
     td.format.type == ResourceFormatType::BC7)
  {
    blockSize =
        (td.format.type == ResourceFormatType::BC1 || td.format.type == ResourceFormatType::BC4)
            ? 8
            : 16;
    rowPitch = RDCMAX(1U, ((td.width + 3) / 4)) * blockSize;
    slicePitch = rowPitch * RDCMAX(1U, td.height / 4);
    blockformat = true;
  }
  else
  {
    switch(td.format.type)
    {
      case ResourceFormatType::S8: bytesPerPixel = 1; break;
      case ResourceFormatType::R10G10B10A2:
      case ResourceFormatType::R9G9B9E5:
      case ResourceFormatType::R11G11B10:
      case ResourceFormatType::D24S8: bytesPerPixel = 4; break;
      case ResourceFormatType::R5G6B5:
      case ResourceFormatType::R5G5B5A1:
      case ResourceFormatType::R4G4B4A4: bytesPerPixel = 2; break;
      case ResourceFormatType::D32S8: bytesPerPixel = 8; break;
      case ResourceFormatType::D16S8:
      case ResourceFormatType::YUV:
      case ResourceFormatType::R4G4:
        RDCERR("Unsupported file format %u", td.format.type);
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
      params.remap = downcast ? RemapTexture::RGBA8 : RemapTexture::NoRemap;
      params.blackPoint = sd.comp.blackPoint;
      params.whitePoint = sd.comp.whitePoint;

      bytebuf data;
      m_pDevice->GetTextureData(liveid, slice, mip, params, data);

      if(data.empty())
      {
        RDCERR("Couldn't get bytes for mip %u, slice %u", mip, slice);

        for(size_t i = 0; i < subdata.size(); i++)
          delete[] subdata[i];

        return false;
      }

      if(td.depth == 1)
      {
        byte *bytes = new byte[data.size()];
        memcpy(bytes, data.data(), data.size());
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
        byte *b = data.data() + mipSlicePitch * sliceOffset;
        memcpy(depthslice, b, slicePitch);
        subdata.push_back(depthslice);

        continue;
      }

      s += (d - 1);

      byte *b = data.data();

      // add each depth slice as a separate subdata
      for(uint32_t di = 0; di < d; di++)
      {
        byte *depthslice = new byte[mipSlicePitch];

        memcpy(depthslice, b, mipSlicePitch);

        subdata.push_back(depthslice);

        b += mipSlicePitch;
      }
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
  if(numComps == 4 && (sd.destType == FileType::BMP || sd.destType == FileType::JPG))
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

        if(sd.alpha != AlphaMapping::Discard)
        {
          Vec4f col = Vec4f(sd.alphaCol.x, sd.alphaCol.y, sd.alphaCol.z);
          if(sd.alpha == AlphaMapping::BlendToCheckerboard)
          {
            bool lightSquare = ((x / 64) % 2) == ((y / 64) % 2);
            col = lightSquare ? RenderDoc::Inst().LightCheckerboardColor()
                              : RenderDoc::Inst().DarkCheckerboardColor();
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
  if(numComps == 2 && (sd.destType == FileType::BMP || sd.destType == FileType::JPG ||
                       sd.destType == FileType::PNG || sd.destType == FileType::TGA))
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
    RDCERR("Couldn't write to path %s, error: %s", path, FileIO::ErrorString().c_str());
  }
  else
  {
    if(sd.destType == FileType::DDS)
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
    else if(sd.destType == FileType::BMP)
    {
      int ret = stbi_write_bmp_to_func(fileWriteFunc, (void *)f, td.width, td.height, numComps,
                                       subdata[0]);
      success = (ret != 0);

      if(!success)
        RDCERR("stbi_write_bmp_to_func failed: %d", ret);
    }
    else if(sd.destType == FileType::PNG)
    {
      // discard alpha if requested
      for(uint32_t p = 0; sd.alpha == AlphaMapping::Discard && p < td.width * td.height; p++)
        subdata[0][p * 4 + 3] = 255;

      int ret = stbi_write_png_to_func(fileWriteFunc, (void *)f, td.width, td.height, numComps,
                                       subdata[0], rowPitch);
      success = (ret != 0);

      if(!success)
        RDCERR("stbi_write_png_to_func failed: %d", ret);
    }
    else if(sd.destType == FileType::TGA)
    {
      // discard alpha if requested
      for(uint32_t p = 0; sd.alpha == AlphaMapping::Discard && p < td.width * td.height; p++)
        subdata[0][p * 4 + 3] = 255;

      int ret = stbi_write_tga_to_func(fileWriteFunc, (void *)f, td.width, td.height, numComps,
                                       subdata[0]);
      success = (ret != 0);

      if(!success)
        RDCERR("stbi_write_tga_to_func failed: %d", ret);
    }
    else if(sd.destType == FileType::JPG)
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

      if(!success)
        RDCERR("jpge::compress_image_to_jpeg_file_in_memory failed");

      if(success)
        fwrite(jpgdst, 1, len, f);

      delete[] jpgdst;
    }
    else if(sd.destType == FileType::HDR || sd.destType == FileType::EXR)
    {
      float *fldata = NULL;
      float *abgr[4] = {NULL, NULL, NULL, NULL};

      if(sd.destType == FileType::HDR)
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
      if(saveFmt.compType == CompType::Typeless)
        saveFmt.compType = sd.typeHint;
      if(saveFmt.compType == CompType::Typeless)
        saveFmt.compType = saveFmt.compByteWidth == 4 ? CompType::Float : CompType::UNorm;

      uint32_t pixStride = saveFmt.compCount * saveFmt.compByteWidth;

      // 24-bit depth still has a stride of 4 bytes.
      if(saveFmt.compType == CompType::Depth && pixStride == 3)
        pixStride = 4;

      for(uint32_t y = 0; y < td.height; y++)
      {
        for(uint32_t x = 0; x < td.width; x++)
        {
          float r = 0.0f;
          float g = 0.0f;
          float b = 0.0f;
          float a = 1.0f;

          if(saveFmt.type == ResourceFormatType::R10G10B10A2)
          {
            uint32_t *u32 = (uint32_t *)srcData;

            Vec4f vec = ConvertFromR10G10B10A2(*u32);

            r = vec.x;
            g = vec.y;
            b = vec.z;
            a = vec.w;

            srcData += 4;
          }
          else if(saveFmt.type == ResourceFormatType::R11G11B10)
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

            srcData += pixStride;
          }

          if(saveFmt.bgraOrder)
            std::swap(r, b);

          // HDR can't represent negative values
          if(sd.destType == FileType::HDR)
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

      if(sd.destType == FileType::HDR)
      {
        int ret = stbi_write_hdr_to_func(fileWriteFunc, (void *)f, td.width, td.height, 4, fldata);
        success = (ret != 0);

        if(!success)
          RDCERR("stbi_write_hdr_to_func failed: %d", ret);
      }
      else if(sd.destType == FileType::EXR)
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

rdcarray<PixelModification> ReplayController::PixelHistory(ResourceId target, uint32_t x,
                                                           uint32_t y, uint32_t slice, uint32_t mip,
                                                           uint32_t sampleIdx, CompType typeHint)
{
  rdcarray<PixelModification> ret;

  for(size_t t = 0; t < m_Textures.size(); t++)
  {
    if(m_Textures[t].ID == target)
    {
      if(x >= m_Textures[t].width || y >= m_Textures[t].height)
      {
        RDCDEBUG("PixelHistory out of bounds on %llu (%u,%u) vs (%u,%u)", target, x, y,
                 m_Textures[t].width, m_Textures[t].height);
        return ret;
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
      case ResourceUsage::VertexBuffer:
      case ResourceUsage::IndexBuffer:
      case ResourceUsage::VS_Constants:
      case ResourceUsage::HS_Constants:
      case ResourceUsage::DS_Constants:
      case ResourceUsage::GS_Constants:
      case ResourceUsage::PS_Constants:
      case ResourceUsage::CS_Constants:
      case ResourceUsage::All_Constants:
      case ResourceUsage::VS_Resource:
      case ResourceUsage::HS_Resource:
      case ResourceUsage::DS_Resource:
      case ResourceUsage::GS_Resource:
      case ResourceUsage::PS_Resource:
      case ResourceUsage::CS_Resource:
      case ResourceUsage::All_Resource:
      case ResourceUsage::InputTarget:
      case ResourceUsage::CopySrc:
      case ResourceUsage::ResolveSrc:
      case ResourceUsage::Barrier:
      case ResourceUsage::Indirect:
        // read-only, not a valid pixel history event
        continue;

      case ResourceUsage::Unused:
      case ResourceUsage::StreamOut:
      case ResourceUsage::VS_RWResource:
      case ResourceUsage::HS_RWResource:
      case ResourceUsage::DS_RWResource:
      case ResourceUsage::GS_RWResource:
      case ResourceUsage::PS_RWResource:
      case ResourceUsage::CS_RWResource:
      case ResourceUsage::All_RWResource:
      case ResourceUsage::ColorTarget:
      case ResourceUsage::DepthStencilTarget:
      case ResourceUsage::Clear:
      case ResourceUsage::Copy:
      case ResourceUsage::CopyDst:
      case ResourceUsage::Resolve:
      case ResourceUsage::ResolveDst:
      case ResourceUsage::GenMips:
        // writing - include in pixel history events
        break;
    }

    events.push_back(usage[i]);
  }

  if(events.empty())
  {
    RDCDEBUG("Target %llu not written to before %u", target, m_EventID);
    return ret;
  }

  ret = m_pDevice->PixelHistory(events, m_pDevice->GetLiveID(target), x, y, slice, mip, sampleIdx,
                                typeHint);

  SetFrameEvent(m_EventID, true);

  return ret;
}

ShaderDebugTrace *ReplayController::DebugVertex(uint32_t vertid, uint32_t instid, uint32_t idx,
                                                uint32_t instOffset, uint32_t vertOffset)
{
  ShaderDebugTrace *ret = new ShaderDebugTrace;

  *ret = m_pDevice->DebugVertex(m_EventID, vertid, instid, idx, instOffset, vertOffset);

  SetFrameEvent(m_EventID, true);

  return ret;
}

ShaderDebugTrace *ReplayController::DebugPixel(uint32_t x, uint32_t y, uint32_t sample,
                                               uint32_t primitive)
{
  ShaderDebugTrace *ret = new ShaderDebugTrace;

  *ret = m_pDevice->DebugPixel(m_EventID, x, y, sample, primitive);

  SetFrameEvent(m_EventID, true);

  return ret;
}

ShaderDebugTrace *ReplayController::DebugThread(const uint32_t groupid[3], const uint32_t threadid[3])
{
  ShaderDebugTrace *ret = new ShaderDebugTrace;

  *ret = m_pDevice->DebugThread(m_EventID, groupid, threadid);

  SetFrameEvent(m_EventID, true);

  return ret;
}

void ReplayController::FreeTrace(ShaderDebugTrace *trace)
{
  delete trace;
}

rdcarray<ShaderVariable> ReplayController::GetCBufferVariableContents(
    ResourceId shader, const char *entryPoint, uint32_t cbufslot, ResourceId buffer, uint64_t offs)
{
  vector<byte> data;
  if(buffer != ResourceId())
    m_pDevice->GetBufferData(m_pDevice->GetLiveID(buffer), offs, 0, data);

  vector<ShaderVariable> v;

  m_pDevice->FillCBufferVariables(m_pDevice->GetLiveID(shader), entryPoint, cbufslot, v, data);

  return v;
}

rdcarray<WindowingSystem> ReplayController::GetSupportedWindowSystems()
{
  return m_pDevice->GetSupportedWindowSystems();
}

void ReplayController::ReplayLoop(WindowingSystem system, void *data, ResourceId texid)
{
  ReplayOutput *output = CreateOutput(system, data, ReplayOutputType::Texture);

  TextureDisplay d;
  d.texid = texid;
  d.mip = 0;
  d.sampleIdx = ~0U;
  d.overlay = DebugOverlay::NoOverlay;
  d.typeHint = CompType::Typeless;
  d.HDRMul = -1.0f;
  d.linearDisplayAsGamma = true;
  d.FlipY = false;
  d.rangemin = 0.0f;
  d.rangemax = 1.0f;
  d.scale = 1.0f;
  d.offx = 0.0f;
  d.offy = 0.0f;
  d.sliceFace = 0;
  d.rawoutput = false;
  d.Red = d.Green = d.Blue = true;
  d.Alpha = false;
  output->SetTextureDisplay(d);

  m_ReplayLoopCancel = 0;
  m_ReplayLoopFinished = 0;

  while(Atomic::CmpExch32(&m_ReplayLoopCancel, 0, 0) == 0)
  {
    m_pDevice->ReplayLog(10000000, eReplay_Full);

    output->Display();
  }

  // restore back to where we were
  m_pDevice->ReplayLog(m_EventID, eReplay_Full);

  ShutdownOutput(output);

  // mark that the loop is finished
  Atomic::Inc32(&m_ReplayLoopFinished);
}

void ReplayController::CancelReplayLoop()
{
  Atomic::Inc32(&m_ReplayLoopCancel);

  // wait for it to actually finish before returning
  while(Atomic::CmpExch32(&m_ReplayLoopFinished, 0, 0) == 0)
    Threading::Sleep(1);
}

ReplayOutput *ReplayController::CreateOutput(WindowingSystem system, void *data, ReplayOutputType type)
{
  ReplayOutput *out = new ReplayOutput(this, system, data, type);

  m_Outputs.push_back(out);

  m_pDevice->ReplayLog(m_EventID, eReplay_WithoutDraw);

  out->SetFrameEvent(m_EventID);

  m_pDevice->ReplayLog(m_EventID, eReplay_OnlyDraw);

  return out;
}

void ReplayController::ShutdownOutput(IReplayOutput *output)
{
  RDCUNIMPLEMENTED("Shutting down individual outputs");
}

void ReplayController::Shutdown()
{
  delete this;
}

rdcpair<ResourceId, rdcstr> ReplayController::BuildTargetShader(
    const char *entry, const char *source, const ShaderCompileFlags &compileFlags, ShaderStage type)
{
  ResourceId id;
  string errs;

  switch(type)
  {
    case ShaderStage::Vertex:
    case ShaderStage::Hull:
    case ShaderStage::Domain:
    case ShaderStage::Geometry:
    case ShaderStage::Pixel:
    case ShaderStage::Compute: break;
    default: RDCERR("Unexpected type in BuildShader!"); return rdcpair<ResourceId, rdcstr>();
  }

  m_pDevice->BuildTargetShader(source, entry, compileFlags, type, &id, &errs);

  if(id != ResourceId())
    m_TargetResources.insert(id);

  return make_rdcpair<ResourceId, rdcstr>(id, errs);
}

rdcpair<ResourceId, rdcstr> ReplayController::BuildCustomShader(
    const char *entry, const char *source, const ShaderCompileFlags &compileFlags, ShaderStage type)
{
  ResourceId id;
  string errs;

  switch(type)
  {
    case ShaderStage::Vertex:
    case ShaderStage::Hull:
    case ShaderStage::Domain:
    case ShaderStage::Geometry:
    case ShaderStage::Pixel:
    case ShaderStage::Compute: break;
    default: RDCERR("Unexpected type in BuildShader!"); return rdcpair<ResourceId, rdcstr>();
  }

  m_pDevice->BuildCustomShader(source, entry, compileFlags, type, &id, &errs);

  if(id != ResourceId())
    m_CustomShaders.insert(id);

  return make_rdcpair<ResourceId, rdcstr>(id, errs);
}

void ReplayController::FreeTargetResource(ResourceId id)
{
  m_TargetResources.erase(id);
  m_pDevice->FreeTargetResource(id);
}

void ReplayController::FreeCustomShader(ResourceId id)
{
  m_CustomShaders.erase(id);
  m_pDevice->FreeCustomShader(id);
}

void ReplayController::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDevice->ReplaceResource(from, to);

  SetFrameEvent(m_EventID, true);

  for(size_t i = 0; i < m_Outputs.size(); i++)
    if(m_Outputs[i]->GetType() != ReplayOutputType::Headless)
      m_Outputs[i]->Display();
}

void ReplayController::RemoveReplacement(ResourceId id)
{
  m_pDevice->RemoveReplacement(id);

  SetFrameEvent(m_EventID, true);

  for(size_t i = 0; i < m_Outputs.size(); i++)
    if(m_Outputs[i]->GetType() != ReplayOutputType::Headless)
      m_Outputs[i]->Display();
}

ReplayStatus ReplayController::CreateDevice(RDCFile *rdc)
{
  IReplayDriver *driver = NULL;
  ReplayStatus status = RenderDoc::Inst().CreateReplayDriver(rdc, &driver);

  if(driver && status == ReplayStatus::Succeeded)
  {
    RDCLOG("Created replay driver.");
    return PostCreateInit(driver, rdc);
  }

  RDCERR("Couldn't create a replay device :(.");
  return status;
}

ReplayStatus ReplayController::SetDevice(IReplayDriver *device)
{
  if(device)
  {
    RDCLOG("Got replay driver.");
    return PostCreateInit(device, NULL);
  }

  RDCERR("Given invalid replay driver.");
  return ReplayStatus::InternalError;
}

ReplayStatus ReplayController::PostCreateInit(IReplayDriver *device, RDCFile *rdc)
{
  m_pDevice = device;

  m_pDevice->ReadLogInitialisation(rdc);

  FetchPipelineState();

  m_FrameRecord = m_pDevice->GetFrameRecord();

  DrawcallDescription *previous = NULL;
  SetupDrawcallPointers(&m_Drawcalls, m_FrameRecord.drawcallList, NULL, previous);

  return ReplayStatus::Succeeded;
}

void ReplayController::FileChanged()
{
  m_pDevice->FileChanged();
}

APIProperties ReplayController::GetAPIProperties()
{
  return m_pDevice->GetAPIProperties();
}

void ReplayController::FetchPipelineState()
{
  m_pDevice->SavePipelineState();

  m_D3D11PipelineState = &m_pDevice->GetD3D11PipelineState();
  m_D3D12PipelineState = &m_pDevice->GetD3D12PipelineState();
  m_GLPipelineState = &m_pDevice->GetGLPipelineState();
  m_VulkanPipelineState = &m_pDevice->GetVulkanPipelineState();
}
