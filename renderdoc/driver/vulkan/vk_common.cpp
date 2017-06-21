/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include "vk_common.h"
#include "vk_manager.h"
#include "vk_resources.h"

const uint32_t AMD_PCI_ID = 0x1002;
const uint32_t NV_PCI_ID = 0x10DE;

VkAccessFlags MakeAccessMask(VkImageLayout layout)
{
  switch(layout)
  {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VkAccessFlags(VK_ACCESS_TRANSFER_WRITE_BIT);
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return VkAccessFlags(VK_ACCESS_HOST_WRITE_BIT);
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VkAccessFlags(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT);
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VkAccessFlags(VK_ACCESS_TRANSFER_READ_BIT);
    default: break;
  }

  return VkAccessFlags(0);
}

void ReplacePresentableImageLayout(VkImageLayout &layout)
{
  if(layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    layout = VK_IMAGE_LAYOUT_GENERAL;
}

void ReplaceExternalQueueFamily(uint32_t &srcQueueFamily, uint32_t &dstQueueFamily)
{
  if(srcQueueFamily == VK_QUEUE_FAMILY_EXTERNAL_KHX || dstQueueFamily == VK_QUEUE_FAMILY_EXTERNAL_KHX)
  {
    // we should ignore this family transition since we're not synchronising with an
    // external access.
    srcQueueFamily = dstQueueFamily = VK_QUEUE_FAMILY_IGNORED;
  }
}

int SampleCount(VkSampleCountFlagBits countFlag)
{
  switch(countFlag)
  {
    case VK_SAMPLE_COUNT_1_BIT: return 1;
    case VK_SAMPLE_COUNT_2_BIT: return 2;
    case VK_SAMPLE_COUNT_4_BIT: return 4;
    case VK_SAMPLE_COUNT_8_BIT: return 8;
    case VK_SAMPLE_COUNT_16_BIT: return 16;
    case VK_SAMPLE_COUNT_32_BIT: return 32;
    case VK_SAMPLE_COUNT_64_BIT: return 64;
    default: RDCERR("Unrecognised/not single flag %x", countFlag); break;
  }

  return 1;
}

int SampleIndex(VkSampleCountFlagBits countFlag)
{
  switch(countFlag)
  {
    case VK_SAMPLE_COUNT_1_BIT: return 0;
    case VK_SAMPLE_COUNT_2_BIT: return 1;
    case VK_SAMPLE_COUNT_4_BIT: return 2;
    case VK_SAMPLE_COUNT_8_BIT: return 3;
    case VK_SAMPLE_COUNT_16_BIT: return 4;
    case VK_SAMPLE_COUNT_32_BIT: return 5;
    case VK_SAMPLE_COUNT_64_BIT: return 6;
    default: RDCERR("Unrecognised/not single flag %x", countFlag); break;
  }

  return 0;
}

int StageIndex(VkShaderStageFlagBits stageFlag)
{
  switch(stageFlag)
  {
    case VK_SHADER_STAGE_VERTEX_BIT: return 0;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: return 1;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return 2;
    case VK_SHADER_STAGE_GEOMETRY_BIT: return 3;
    case VK_SHADER_STAGE_FRAGMENT_BIT: return 4;
    case VK_SHADER_STAGE_COMPUTE_BIT: return 5;
    default: RDCERR("Unrecognised/not single flag %x", stageFlag); break;
  }

  return 0;
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkImageMemoryBarrier *barriers)
{
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0,
                                   NULL,                // global memory barriers
                                   0, NULL,             // buffer memory barriers
                                   count, barriers);    // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkBufferMemoryBarrier *barriers)
{
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0,
                                   NULL,               // global memory barriers
                                   count, barriers,    // buffer memory barriers
                                   0, NULL);           // image memory barriers
}

void DoPipelineBarrier(VkCommandBuffer cmd, uint32_t count, VkMemoryBarrier *barriers)
{
  ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, count,
                                   barriers,    // global memory barriers
                                   0, NULL,     // buffer memory barriers
                                   0, NULL);    // image memory barriers
}

ResourceFormat MakeResourceFormat(VkFormat fmt)
{
  ResourceFormat ret;

  ret.special = false;
  ret.specialFormat = SpecialFormat::Unknown;
  ret.strname = ToStr::Get(fmt).substr(10);    // 3 == strlen("VK_FORMAT_")
  ret.compByteWidth = 0;
  ret.compCount = 0;
  ret.compType = CompType::Typeless;
  ret.srgbCorrected = false;

  if(fmt == VK_FORMAT_UNDEFINED)
  {
    return ret;
  }

  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R4G4;
      break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R4G4B4A4;
      break;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R10G10B10A2;
      break;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R11G11B10;
      break;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R9G9B9E5;
      break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R5G6B5;
      break;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
      ret.special = true;
      ret.specialFormat = SpecialFormat::R5G5B5A1;
      break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
      ret.special = true;
      ret.specialFormat = SpecialFormat::D16S8;
      break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      ret.special = true;
      ret.specialFormat = SpecialFormat::D24S8;
      break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      ret.special = true;
      ret.specialFormat = SpecialFormat::D32S8;
      break;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC1;
      break;
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC2;
      break;
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC3;
      break;
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC4;
      break;
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC5;
      break;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC6;
      break;
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::BC7;
      break;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::ETC2;
      break;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::EAC;
      break;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
      ret.special = true;
      ret.specialFormat = SpecialFormat::ASTC;
      break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: ret.bgraOrder = true; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK: ret.compCount = 1; break;
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: ret.compCount = 2; break;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB: ret.compCount = 3; break;
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: ret.compCount = 4; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB: ret.srgbCorrected = true; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R4G4_UNORM_PACK8:
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: ret.compType = CompType::UNorm; break;
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32: ret.compType = CompType::SNorm; break;
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32: ret.compType = CompType::UScaled; break;
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: ret.compType = CompType::SScaled; break;
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    // Maybe S8 should be identified by something else?
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32: ret.compType = CompType::UInt; break;
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: ret.compType = CompType::SInt; break;
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: ret.compType = CompType::Float; break;
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret.compType = CompType::Double; break;
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT: ret.compType = CompType::Depth; break;
    default: break;
  }

  switch(fmt)
  {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_USCALED:
    case VK_FORMAT_R8_SSCALED:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_USCALED:
    case VK_FORMAT_R8G8_SSCALED:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SNORM:
    case VK_FORMAT_R8G8B8_USCALED:
    case VK_FORMAT_R8G8B8_SSCALED:
    case VK_FORMAT_R8G8B8_UINT:
    case VK_FORMAT_R8G8B8_SINT:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_USCALED:
    case VK_FORMAT_R8G8B8A8_SSCALED:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_B8G8R8_UNORM:
    case VK_FORMAT_B8G8R8_SNORM:
    case VK_FORMAT_B8G8R8_USCALED:
    case VK_FORMAT_B8G8R8_SSCALED:
    case VK_FORMAT_B8G8R8_UINT:
    case VK_FORMAT_B8G8R8_SINT:
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SNORM:
    case VK_FORMAT_B8G8R8A8_USCALED:
    case VK_FORMAT_B8G8R8A8_SSCALED:
    case VK_FORMAT_B8G8R8A8_UINT:
    case VK_FORMAT_B8G8R8A8_SINT:
    case VK_FORMAT_B8G8R8A8_SRGB: ret.compByteWidth = 1; break;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_USCALED:
    case VK_FORMAT_R16_SSCALED:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_USCALED:
    case VK_FORMAT_R16G16_SSCALED:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_USCALED:
    case VK_FORMAT_R16G16B16_SSCALED:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_USCALED:
    case VK_FORMAT_R16G16B16A16_SSCALED:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_D16_UNORM: ret.compByteWidth = 2; break;
    case VK_FORMAT_X8_D24_UNORM_PACK32: ret.compByteWidth = 3; break;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_D32_SFLOAT: ret.compByteWidth = 4; break;
    case VK_FORMAT_R64_SFLOAT:
    case VK_FORMAT_R64G64_SFLOAT:
    case VK_FORMAT_R64G64B64_SFLOAT:
    case VK_FORMAT_R64G64B64A64_SFLOAT: ret.compByteWidth = 8; break;
    default: break;
  }

  return ret;
}

VkFormat MakeVkFormat(ResourceFormat fmt)
{
  VkFormat ret = VK_FORMAT_UNDEFINED;

  if(fmt.special)
  {
    switch(fmt.specialFormat)
    {
      case SpecialFormat::BC1:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        else
          ret = fmt.srgbCorrected ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        break;
      }
      case SpecialFormat::BC2:
        ret = fmt.srgbCorrected ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
        break;
      case SpecialFormat::BC3:
        ret = fmt.srgbCorrected ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        break;
      case SpecialFormat::BC4:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC4_SNORM_BLOCK : VK_FORMAT_BC4_UNORM_BLOCK;
        break;
      case SpecialFormat::BC5:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC5_SNORM_BLOCK : VK_FORMAT_BC5_UNORM_BLOCK;
        break;
      case SpecialFormat::BC6:
        ret = fmt.compType == CompType::SNorm ? VK_FORMAT_BC6H_SFLOAT_BLOCK
                                              : VK_FORMAT_BC6H_UFLOAT_BLOCK;
        break;
      case SpecialFormat::BC7:
        ret = fmt.srgbCorrected ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
        break;
      case SpecialFormat::ETC2:
      {
        if(fmt.compCount == 3)
          ret = fmt.srgbCorrected ? VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK
                                  : VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        else
          ret = fmt.srgbCorrected ? VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK
                                  : VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        break;
      }
      case SpecialFormat::EAC:
      {
        if(fmt.compCount == 1)
          ret = fmt.compType == CompType::SNorm ? VK_FORMAT_EAC_R11_SNORM_BLOCK
                                                : VK_FORMAT_EAC_R11_UNORM_BLOCK;
        else if(fmt.compCount == 2)
          ret = fmt.compType == CompType::SNorm ? VK_FORMAT_EAC_R11G11_SNORM_BLOCK
                                                : VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        break;
      }
      case SpecialFormat::R10G10B10A2:
        if(fmt.compType == CompType::UNorm)
          ret = fmt.bgraOrder ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
                              : VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        else if(fmt.compType == CompType::UInt)
          ret = fmt.bgraOrder ? VK_FORMAT_A2B10G10R10_UINT_PACK32 : VK_FORMAT_A2R10G10B10_UINT_PACK32;
        else if(fmt.compType == CompType::UScaled)
          ret = fmt.bgraOrder ? VK_FORMAT_A2B10G10R10_USCALED_PACK32
                              : VK_FORMAT_A2R10G10B10_USCALED_PACK32;
        else if(fmt.compType == CompType::SNorm)
          ret = fmt.bgraOrder ? VK_FORMAT_A2B10G10R10_SNORM_PACK32
                              : VK_FORMAT_A2R10G10B10_SNORM_PACK32;
        else if(fmt.compType == CompType::SInt)
          ret = fmt.bgraOrder ? VK_FORMAT_A2B10G10R10_SINT_PACK32 : VK_FORMAT_A2R10G10B10_SINT_PACK32;
        else if(fmt.compType == CompType::SScaled)
          ret = fmt.bgraOrder ? VK_FORMAT_A2B10G10R10_SSCALED_PACK32
                              : VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
        break;
      case SpecialFormat::R11G11B10: ret = VK_FORMAT_B10G11R11_UFLOAT_PACK32; break;
      case SpecialFormat::R5G6B5: ret = VK_FORMAT_B5G6R5_UNORM_PACK16; break;
      case SpecialFormat::R5G5B5A1:
        ret = fmt.bgraOrder ? VK_FORMAT_B5G5R5A1_UNORM_PACK16 : VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        break;
      case SpecialFormat::R9G9B9E5: ret = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32; break;
      case SpecialFormat::R4G4B4A4:
        ret = fmt.bgraOrder ? VK_FORMAT_R4G4B4A4_UNORM_PACK16 : VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        break;
      case SpecialFormat::R4G4: ret = VK_FORMAT_R4G4_UNORM_PACK8; break;
      case SpecialFormat::D24S8: ret = VK_FORMAT_D24_UNORM_S8_UINT; break;
      case SpecialFormat::D32S8: ret = VK_FORMAT_D32_SFLOAT_S8_UINT; break;
      default: RDCERR("Unsupported special format %u", fmt.specialFormat); break;
    }
  }
  else if(fmt.compCount == 4)
  {
    if(fmt.srgbCorrected)
    {
      ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB;
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32B32A32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32B32A32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32B32A32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16B16A16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16B16A16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16B16A16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16B16A16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16B16A16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16B16A16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16B16A16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SINT : VK_FORMAT_R8G8B8A8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_UINT : VK_FORMAT_R8G8B8A8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SNORM : VK_FORMAT_R8G8B8A8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_SSCALED : VK_FORMAT_R8G8B8A8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = fmt.bgraOrder ? VK_FORMAT_B8G8R8A8_USCALED : VK_FORMAT_R8G8B8A8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 4-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 3)
  {
    if(fmt.srgbCorrected)
    {
      ret = VK_FORMAT_R8G8B8_SRGB;
    }
    else if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32B32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32B32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32B32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16B16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16B16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16B16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16B16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16B16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16B16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16B16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8G8B8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8G8B8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8G8B8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8G8B8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8G8B8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8G8B8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 2)
  {
    if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32G32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32G32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32G32_UINT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16G16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16G16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16G16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16G16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16G16_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16G16_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16G16_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8G8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8G8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8G8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8G8_UNORM;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8G8_SSCALED;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8G8_USCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else if(fmt.compCount == 1)
  {
    if(fmt.compByteWidth == 4)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R32_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R32_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R32_UINT;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_D32_SFLOAT;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 2)
    {
      if(fmt.compType == CompType::Float)
        ret = VK_FORMAT_R16_SFLOAT;
      else if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R16_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R16_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R16_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R16_UNORM;
      else if(fmt.compType == CompType::Depth)
        ret = VK_FORMAT_D16_UNORM;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R16_USCALED;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R16_SSCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else if(fmt.compByteWidth == 1)
    {
      if(fmt.compType == CompType::SInt)
        ret = VK_FORMAT_R8_SINT;
      else if(fmt.compType == CompType::UInt)
        ret = VK_FORMAT_R8_UINT;
      else if(fmt.compType == CompType::SNorm)
        ret = VK_FORMAT_R8_SNORM;
      else if(fmt.compType == CompType::UNorm)
        ret = VK_FORMAT_R8_UNORM;
      else if(fmt.compType == CompType::UScaled)
        ret = VK_FORMAT_R8_USCALED;
      else if(fmt.compType == CompType::SScaled)
        ret = VK_FORMAT_R8_SSCALED;
      else
        RDCERR("Unrecognised component type");
    }
    else
    {
      RDCERR("Unrecognised 3-component byte width: %d", fmt.compByteWidth);
    }
  }
  else
  {
    RDCERR("Unrecognised component count: %d", fmt.compCount);
  }

  if(ret == VK_FORMAT_UNDEFINED)
    RDCERR("No known vulkan format corresponding to resource format!");

  return ret;
}

Topology MakePrimitiveTopology(VkPrimitiveTopology Topo, uint32_t patchControlPoints)
{
  switch(Topo)
  {
    default: break;
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: return Topology::PointList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: return Topology::LineList; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: return Topology::LineStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return Topology::TriangleList; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return Topology::TriangleStrip; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: return Topology::TriangleFan; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: return Topology::LineList_Adj; break;
    case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: return Topology::LineStrip_Adj; break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      return Topology::TriangleList_Adj;
      break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      return Topology::TriangleStrip_Adj;
      break;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: return PatchList_Topology(patchControlPoints); break;
  }

  return Topology::Unknown;
}

VkPrimitiveTopology MakeVkPrimitiveTopology(Topology Topo)
{
  switch(Topo)
  {
    case Topology::LineLoop: RDCWARN("Unsupported primitive topology on Vulkan: %x", Topo); break;
    default: return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    case Topology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case Topology::LineStrip_Adj: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
    case Topology::LineList_Adj: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Topology::TriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Topology::TriangleStrip_Adj: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
    case Topology::TriangleList_Adj: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case Topology::PatchList_1CPs:
    case Topology::PatchList_2CPs:
    case Topology::PatchList_3CPs:
    case Topology::PatchList_4CPs:
    case Topology::PatchList_5CPs:
    case Topology::PatchList_6CPs:
    case Topology::PatchList_7CPs:
    case Topology::PatchList_8CPs:
    case Topology::PatchList_9CPs:
    case Topology::PatchList_10CPs:
    case Topology::PatchList_11CPs:
    case Topology::PatchList_12CPs:
    case Topology::PatchList_13CPs:
    case Topology::PatchList_14CPs:
    case Topology::PatchList_15CPs:
    case Topology::PatchList_16CPs:
    case Topology::PatchList_17CPs:
    case Topology::PatchList_18CPs:
    case Topology::PatchList_19CPs:
    case Topology::PatchList_20CPs:
    case Topology::PatchList_21CPs:
    case Topology::PatchList_22CPs:
    case Topology::PatchList_23CPs:
    case Topology::PatchList_24CPs:
    case Topology::PatchList_25CPs:
    case Topology::PatchList_26CPs:
    case Topology::PatchList_27CPs:
    case Topology::PatchList_28CPs:
    case Topology::PatchList_29CPs:
    case Topology::PatchList_30CPs:
    case Topology::PatchList_31CPs:
    case Topology::PatchList_32CPs: return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  }

  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

AddressMode MakeAddressMode(VkSamplerAddressMode addr)
{
  switch(addr)
  {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT: return AddressMode::Wrap;
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return AddressMode::Mirror;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return AddressMode::ClampEdge;
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return AddressMode::ClampBorder;
    case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return AddressMode::MirrorOnce;
    default: break;
  }

  return AddressMode::Wrap;
}

void MakeBorderColor(VkBorderColor border, FloatVector *BorderColor)
{
  // we don't distinguish float/int, assume it matches
  switch(border)
  {
    case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
    case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
      *BorderColor = FloatVector(0.0f, 0.0f, 0.0f, 0.0f);
      break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
    case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
      *BorderColor = FloatVector(0.0f, 0.0f, 0.0f, 1.0f);
      break;
    case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
    case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
      *BorderColor = FloatVector(1.0f, 1.0f, 1.0f, 1.0f);
      break;
    default: memset(BorderColor, 0, sizeof(FloatVector)); break;
  }
}

CompareFunc MakeCompareFunc(VkCompareOp func)
{
  switch(func)
  {
    case VK_COMPARE_OP_NEVER: return CompareFunc::Never;
    case VK_COMPARE_OP_LESS: return CompareFunc::Less;
    case VK_COMPARE_OP_EQUAL: return CompareFunc::Equal;
    case VK_COMPARE_OP_LESS_OR_EQUAL: return CompareFunc::LessEqual;
    case VK_COMPARE_OP_GREATER: return CompareFunc::Greater;
    case VK_COMPARE_OP_NOT_EQUAL: return CompareFunc::NotEqual;
    case VK_COMPARE_OP_GREATER_OR_EQUAL: return CompareFunc::GreaterEqual;
    case VK_COMPARE_OP_ALWAYS: return CompareFunc::AlwaysTrue;
    default: break;
  }

  return CompareFunc::AlwaysTrue;
}

static FilterMode MakeFilterMode(VkFilter f)
{
  switch(f)
  {
    case VK_FILTER_NEAREST: return FilterMode::Point;
    case VK_FILTER_LINEAR: return FilterMode::Linear;
    case VK_FILTER_CUBIC_IMG: return FilterMode::Cubic;
    default: break;
  }

  return FilterMode::NoFilter;
}

static FilterMode MakeFilterMode(VkSamplerMipmapMode f)
{
  switch(f)
  {
    case VK_SAMPLER_MIPMAP_MODE_NEAREST: return FilterMode::Point;
    case VK_SAMPLER_MIPMAP_MODE_LINEAR: return FilterMode::Linear;
    default: break;
  }

  return FilterMode::NoFilter;
}

TextureFilter MakeFilter(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode,
                         bool anisoEnable, bool compareEnable)
{
  TextureFilter ret;

  if(anisoEnable)
  {
    ret.minify = ret.magnify = ret.mip = FilterMode::Anisotropic;
  }
  else
  {
    ret.minify = MakeFilterMode(minFilter);
    ret.magnify = MakeFilterMode(magFilter);
    ret.mip = MakeFilterMode(mipmapMode);
  }
  ret.func = compareEnable ? FilterFunc::Comparison : FilterFunc::Normal;

  return ret;
}

LogicOp MakeLogicOp(VkLogicOp op)
{
  switch(op)
  {
    case VK_LOGIC_OP_CLEAR: return LogicOp::Clear;
    case VK_LOGIC_OP_AND: return LogicOp::And;
    case VK_LOGIC_OP_AND_REVERSE: return LogicOp::AndReverse;
    case VK_LOGIC_OP_COPY: return LogicOp::Copy;
    case VK_LOGIC_OP_AND_INVERTED: return LogicOp::AndInverted;
    case VK_LOGIC_OP_NO_OP: return LogicOp::NoOp;
    case VK_LOGIC_OP_XOR: return LogicOp::Xor;
    case VK_LOGIC_OP_OR: return LogicOp::Or;
    case VK_LOGIC_OP_NOR: return LogicOp::Nor;
    case VK_LOGIC_OP_EQUIVALENT: return LogicOp::Equivalent;
    case VK_LOGIC_OP_INVERT: return LogicOp::Invert;
    case VK_LOGIC_OP_OR_REVERSE: return LogicOp::OrReverse;
    case VK_LOGIC_OP_COPY_INVERTED: return LogicOp::CopyInverted;
    case VK_LOGIC_OP_OR_INVERTED: return LogicOp::OrInverted;
    case VK_LOGIC_OP_NAND: return LogicOp::Nand;
    case VK_LOGIC_OP_SET: return LogicOp::Set;
    default: break;
  }

  return LogicOp::NoOp;
}

BlendMultiplier MakeBlendMultiplier(VkBlendFactor blend)
{
  switch(blend)
  {
    case VK_BLEND_FACTOR_ZERO: return BlendMultiplier::Zero;
    case VK_BLEND_FACTOR_ONE: return BlendMultiplier::One;
    case VK_BLEND_FACTOR_SRC_COLOR: return BlendMultiplier::SrcCol;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return BlendMultiplier::InvSrcCol;
    case VK_BLEND_FACTOR_DST_COLOR: return BlendMultiplier::DstCol;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return BlendMultiplier::InvDstCol;
    case VK_BLEND_FACTOR_SRC_ALPHA: return BlendMultiplier::SrcAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return BlendMultiplier::InvSrcAlpha;
    case VK_BLEND_FACTOR_DST_ALPHA: return BlendMultiplier::DstAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return BlendMultiplier::InvDstAlpha;
    case VK_BLEND_FACTOR_CONSTANT_COLOR: return BlendMultiplier::FactorRGB;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return BlendMultiplier::InvFactorRGB;
    case VK_BLEND_FACTOR_CONSTANT_ALPHA: return BlendMultiplier::FactorAlpha;
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return BlendMultiplier::InvFactorAlpha;
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return BlendMultiplier::SrcAlphaSat;
    case VK_BLEND_FACTOR_SRC1_COLOR: return BlendMultiplier::Src1Col;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return BlendMultiplier::InvSrc1Col;
    case VK_BLEND_FACTOR_SRC1_ALPHA: return BlendMultiplier::Src1Alpha;
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOp MakeBlendOp(VkBlendOp op)
{
  switch(op)
  {
    case VK_BLEND_OP_ADD: return BlendOp::Add;
    case VK_BLEND_OP_SUBTRACT: return BlendOp::Subtract;
    case VK_BLEND_OP_REVERSE_SUBTRACT: return BlendOp::ReversedSubtract;
    case VK_BLEND_OP_MIN: return BlendOp::Minimum;
    case VK_BLEND_OP_MAX: return BlendOp::Maximum;
    default: break;
  }

  return BlendOp::Add;
}

StencilOp MakeStencilOp(VkStencilOp op)
{
  switch(op)
  {
    case VK_STENCIL_OP_KEEP: return StencilOp::Keep;
    case VK_STENCIL_OP_ZERO: return StencilOp::Zero;
    case VK_STENCIL_OP_REPLACE: return StencilOp::Replace;
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP: return StencilOp::IncSat;
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP: return StencilOp::DecSat;
    case VK_STENCIL_OP_INVERT: return StencilOp::Invert;
    case VK_STENCIL_OP_INCREMENT_AND_WRAP: return StencilOp::IncWrap;
    case VK_STENCIL_OP_DECREMENT_AND_WRAP: return StencilOp::DecWrap;
    default: break;
  }

  return StencilOp::Keep;
}

// we cast to this type when serialising as a placeholder indicating that
// the given flags field doesn't have any bits defined
enum VkFlagWithNoBits
{
  FlagWithNoBits_Dummy_Bit = 1,
};

template <>
string ToStrHelper<false, VkResourceType>::Get(const VkResourceType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(eResUnknown)
    TOSTR_CASE_STRINGIZE(eResPhysicalDevice)
    TOSTR_CASE_STRINGIZE(eResInstance)
    TOSTR_CASE_STRINGIZE(eResDevice)
    TOSTR_CASE_STRINGIZE(eResQueue)
    TOSTR_CASE_STRINGIZE(eResDeviceMemory)
    TOSTR_CASE_STRINGIZE(eResBuffer)
    TOSTR_CASE_STRINGIZE(eResBufferView)
    TOSTR_CASE_STRINGIZE(eResImage)
    TOSTR_CASE_STRINGIZE(eResImageView)
    TOSTR_CASE_STRINGIZE(eResFramebuffer)
    TOSTR_CASE_STRINGIZE(eResRenderPass)
    TOSTR_CASE_STRINGIZE(eResShaderModule)
    TOSTR_CASE_STRINGIZE(eResPipelineCache)
    TOSTR_CASE_STRINGIZE(eResPipelineLayout)
    TOSTR_CASE_STRINGIZE(eResPipeline)
    TOSTR_CASE_STRINGIZE(eResSampler)
    TOSTR_CASE_STRINGIZE(eResDescriptorPool)
    TOSTR_CASE_STRINGIZE(eResDescriptorSetLayout)
    TOSTR_CASE_STRINGIZE(eResDescriptorSet)
    TOSTR_CASE_STRINGIZE(eResCommandPool)
    TOSTR_CASE_STRINGIZE(eResCommandBuffer)
    TOSTR_CASE_STRINGIZE(eResFence)
    TOSTR_CASE_STRINGIZE(eResEvent)
    TOSTR_CASE_STRINGIZE(eResQueryPool)
    TOSTR_CASE_STRINGIZE(eResSemaphore)
    TOSTR_CASE_STRINGIZE(eResSwapchain)
    TOSTR_CASE_STRINGIZE(eResSurface)
    default: break;
  }

  return StringFormat::Fmt("VkResourceType<%d>", el);
}

template <>
string ToStrHelper<false, VkQueueFlagBits>::Get(const VkQueueFlagBits &el)
{
  string ret;

  if(el & VK_QUEUE_GRAPHICS_BIT)
    ret += " | VK_QUEUE_GRAPHICS_BIT";
  if(el & VK_QUEUE_COMPUTE_BIT)
    ret += " | VK_QUEUE_COMPUTE_BIT";
  if(el & VK_QUEUE_TRANSFER_BIT)
    ret += " | VK_QUEUE_TRANSFER_BIT";
  if(el & VK_QUEUE_SPARSE_BINDING_BIT)
    ret += " | VK_QUEUE_SPARSE_BINDING_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkFlagWithNoBits>::Get(const VkFlagWithNoBits &el)
{
  if(el != 0)
    return StringFormat::Fmt("Invalid bits set: %x", el);
  return "";
}

template <>
string ToStrHelper<false, VkPipelineCreateFlagBits>::Get(const VkPipelineCreateFlagBits &el)
{
  string ret;

  if(el & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT)
    ret += " | VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT";
  if(el & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT)
    ret += " | VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT";
  if(el & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
    ret += " | VK_PIPELINE_CREATE_DERIVATIVE_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkPipelineStageFlagBits>::Get(const VkPipelineStageFlagBits &el)
{
  string ret;

  if(el & VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
    ret += " | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT";
  if(el & VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT)
    ret += " | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT";
  if(el & VK_PIPELINE_STAGE_VERTEX_INPUT_BIT)
    ret += " | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT";
  if(el & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT)
    ret += " | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT";
  if(el & VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT)
    ret += " | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT";
  if(el & VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT)
    ret += " | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT";
  if(el & VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT)
    ret += " | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT";
  if(el & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
    ret += " | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT";
  if(el & VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
    ret += " | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT";
  if(el & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
    ret += " | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT";
  if(el & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
    ret += " | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT";
  if(el & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
    ret += " | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT";
  if(el & VK_PIPELINE_STAGE_TRANSFER_BIT)
    ret += " | VK_PIPELINE_STAGE_TRANSFER_BIT";
  if(el & VK_PIPELINE_STAGE_HOST_BIT)
    ret += " | VK_PIPELINE_STAGE_HOST_BIT";
  if(el & VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT)
    ret += " | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT";
  if(el & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
    ret += " | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT";
  if(el & VK_PIPELINE_STAGE_COMMAND_PROCESS_BIT_NVX)
    ret += " | VK_PIPELINE_STAGE_COMMAND_PROCESS_BIT_NVX";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkBufferUsageFlagBits>::Get(const VkBufferUsageFlagBits &el)
{
  string ret = "";

  if(el & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    ret += " | VK_BUFFER_USAGE_TRANSFER_SRC_BIT";
  if(el & VK_BUFFER_USAGE_TRANSFER_DST_BIT)
    ret += " | VK_BUFFER_USAGE_TRANSFER_DST_BIT";
  if(el & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT";
  if(el & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT";
  if(el & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT";
  if(el & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT";
  if(el & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_INDEX_BUFFER_BIT";
  if(el & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT";
  if(el & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
    ret += " | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkImageUsageFlagBits>::Get(const VkImageUsageFlagBits &el)
{
  string ret = "";

  if(el & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    ret += " | VK_IMAGE_USAGE_TRANSFER_SRC_BIT";
  if(el & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    ret += " | VK_IMAGE_USAGE_TRANSFER_DST_BIT";
  if(el & VK_IMAGE_USAGE_SAMPLED_BIT)
    ret += " | VK_IMAGE_USAGE_SAMPLED_BIT";
  if(el & VK_IMAGE_USAGE_STORAGE_BIT)
    ret += " | VK_IMAGE_USAGE_STORAGE_BIT";
  if(el & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    ret += " | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT";
  if(el & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    ret += " | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT";
  if(el & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
    ret += " | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT";
  if(el & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
    ret += " | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkBufferCreateFlagBits>::Get(const VkBufferCreateFlagBits &el)
{
  string ret;

  if(el & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
    ret += " | VK_BUFFER_CREATE_SPARSE_BINDING_BIT";
  if(el & VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT)
    ret += " | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT";
  if(el & VK_BUFFER_CREATE_SPARSE_ALIASED_BIT)
    ret += " | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkImageCreateFlagBits>::Get(const VkImageCreateFlagBits &el)
{
  string ret;

  if(el & VK_IMAGE_CREATE_SPARSE_BINDING_BIT)
    ret += " | VK_IMAGE_CREATE_SPARSE_BINDING_BIT";
  if(el & VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT)
    ret += " | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT";
  if(el & VK_IMAGE_CREATE_SPARSE_ALIASED_BIT)
    ret += " | VK_IMAGE_CREATE_SPARSE_ALIASED_BIT";
  if(el & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT)
    ret += " | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT";
  if(el & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
    ret += " | VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT";
  if(el & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR)
    ret += " | VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkSparseMemoryBindFlagBits>::Get(const VkSparseMemoryBindFlagBits &el)
{
  string ret;

  if(el & VK_SPARSE_MEMORY_BIND_METADATA_BIT)
    ret += " | VK_SPARSE_MEMORY_BIND_METADATA_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkCommandPoolCreateFlagBits>::Get(const VkCommandPoolCreateFlagBits &el)
{
  string ret;

  if(el & VK_COMMAND_POOL_CREATE_TRANSIENT_BIT)
    ret += " | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT";
  if(el & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
    ret += " | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkCommandPoolResetFlagBits>::Get(const VkCommandPoolResetFlagBits &el)
{
  string ret;

  if(el & VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT)
    ret += " | VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkCommandBufferUsageFlagBits>::Get(const VkCommandBufferUsageFlagBits &el)
{
  string ret;

  if(el & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
    ret += " | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT";
  if(el & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT)
    ret += " | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT";
  if(el & VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)
    ret += " | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkDescriptorPoolCreateFlagBits>::Get(const VkDescriptorPoolCreateFlagBits &el)
{
  string ret;

  if(el & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
    ret += " | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkFenceCreateFlagBits>::Get(const VkFenceCreateFlagBits &el)
{
  string ret;

  if(el & VK_FENCE_CREATE_SIGNALED_BIT)
    ret += " | VK_FENCE_CREATE_SIGNALED_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkQueryPipelineStatisticFlagBits>::Get(
    const VkQueryPipelineStatisticFlagBits &el)
{
  string ret;

  if(el & VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT";
  if(el & VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT)
    ret += " | VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkQueryControlFlagBits>::Get(const VkQueryControlFlagBits &el)
{
  string ret;

  if(el & VK_QUERY_CONTROL_PRECISE_BIT)
    ret += " | VK_QUERY_CONTROL_PRECISE_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkQueryResultFlagBits>::Get(const VkQueryResultFlagBits &el)
{
  string ret;

  if(el & VK_QUERY_RESULT_64_BIT)
    ret += " | VK_QUERY_RESULT_64_BIT";
  if(el & VK_QUERY_RESULT_WAIT_BIT)
    ret += " | VK_QUERY_RESULT_WAIT_BIT";
  if(el & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
    ret += " | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT";
  if(el & VK_QUERY_RESULT_PARTIAL_BIT)
    ret += " | VK_QUERY_RESULT_PARTIAL_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkAttachmentDescriptionFlagBits>::Get(const VkAttachmentDescriptionFlagBits &el)
{
  string ret;

  if(el & VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT)
    ret += " | VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkSampleCountFlagBits>::Get(const VkSampleCountFlagBits &el)
{
  string ret;

  if(el & VK_SAMPLE_COUNT_1_BIT)
    ret += " | VK_SAMPLE_COUNT_1_BIT";
  if(el & VK_SAMPLE_COUNT_2_BIT)
    ret += " | VK_SAMPLE_COUNT_2_BIT";
  if(el & VK_SAMPLE_COUNT_4_BIT)
    ret += " | VK_SAMPLE_COUNT_4_BIT";
  if(el & VK_SAMPLE_COUNT_8_BIT)
    ret += " | VK_SAMPLE_COUNT_8_BIT";
  if(el & VK_SAMPLE_COUNT_16_BIT)
    ret += " | VK_SAMPLE_COUNT_16_BIT";
  if(el & VK_SAMPLE_COUNT_32_BIT)
    ret += " | VK_SAMPLE_COUNT_32_BIT";
  if(el & VK_SAMPLE_COUNT_64_BIT)
    ret += " | VK_SAMPLE_COUNT_64_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkImageAspectFlagBits>::Get(const VkImageAspectFlagBits &el)
{
  string ret;

  if(el & VK_IMAGE_ASPECT_COLOR_BIT)
    ret += " | VK_IMAGE_ASPECT_COLOR_BIT";
  if(el & VK_IMAGE_ASPECT_DEPTH_BIT)
    ret += " | VK_IMAGE_ASPECT_DEPTH_BIT";
  if(el & VK_IMAGE_ASPECT_STENCIL_BIT)
    ret += " | VK_IMAGE_ASPECT_STENCIL_BIT";
  if(el & VK_IMAGE_ASPECT_METADATA_BIT)
    ret += " | VK_IMAGE_ASPECT_METADATA_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkDependencyFlagBits>::Get(const VkDependencyFlagBits &el)
{
  string ret;

  if(el & VK_DEPENDENCY_BY_REGION_BIT)
    ret += " | VK_DEPENDENCY_BY_REGION_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkShaderStageFlagBits>::Get(const VkShaderStageFlagBits &el)
{
  string ret;

  if(el == VK_SHADER_STAGE_ALL_GRAPHICS)
    return "VK_SHADER_STAGE_ALL_GRAPHICS";
  if(el == VK_SHADER_STAGE_ALL)
    return "VK_SHADER_STAGE_ALL";

  if(el & VK_SHADER_STAGE_VERTEX_BIT)
    ret += " | VK_SHADER_STAGE_VERTEX";
  if(el & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    ret += " | VK_SHADER_STAGE_TESSELLATION_CONTROL";
  if(el & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
    ret += " | VK_SHADER_STAGE_TESSELLATION_EVALUATION";
  if(el & VK_SHADER_STAGE_GEOMETRY_BIT)
    ret += " | VK_SHADER_STAGE_GEOMETRY";
  if(el & VK_SHADER_STAGE_FRAGMENT_BIT)
    ret += " | VK_SHADER_STAGE_FRAGMENT";
  if(el & VK_SHADER_STAGE_COMPUTE_BIT)
    ret += " | VK_SHADER_STAGE_COMPUTE";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkStencilFaceFlagBits>::Get(const VkStencilFaceFlagBits &el)
{
  // technically a bitfield but each combination has a particular meaning
  if(el == VK_STENCIL_FACE_FRONT_BIT)
    return "VK_STENCIL_FACE_FRONT";
  if(el == VK_STENCIL_FACE_BACK_BIT)
    return "VK_STENCIL_FACE_BACK";
  if(el == VK_STENCIL_FRONT_AND_BACK)
    return "VK_STENCIL_FRONT_AND_BACK";

  if(el == 0)
    return "VK_STENCIL_FACE_NONE";

  return StringFormat::Fmt("VkStencilFaceFlagBits<%d>", el);
}

template <>
string ToStrHelper<false, VkCullModeFlagBits>::Get(const VkCullModeFlagBits &el)
{
  // technically a bitfield but each combination has a particular meaning
  if(el == VK_CULL_MODE_NONE)
    return "VK_CULL_MODE_NONE";
  if(el == VK_CULL_MODE_FRONT_BIT)
    return "VK_CULL_MODE_FRONT";
  if(el == VK_CULL_MODE_BACK_BIT)
    return "VK_CULL_MODE_BACK";
  if(el == VK_CULL_MODE_FRONT_AND_BACK)
    return "VK_CULL_MODE_FRONT_AND_BACK";

  return StringFormat::Fmt("VkCullModeFlagBits<%d>", el);
}

template <>
string ToStrHelper<false, VkPipelineBindPoint>::Get(const VkPipelineBindPoint &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_PIPELINE_BIND_POINT_COMPUTE)
    TOSTR_CASE_STRINGIZE(VK_PIPELINE_BIND_POINT_GRAPHICS)
    default: break;
  }

  return StringFormat::Fmt("VkPipelineBindPoint<%d>", el);
}

template <>
string ToStrHelper<false, VkIndexType>::Get(const VkIndexType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_INDEX_TYPE_UINT16)
    TOSTR_CASE_STRINGIZE(VK_INDEX_TYPE_UINT32)
    default: break;
  }

  return StringFormat::Fmt("VkIndexType<%d>", el);
}

template <>
string ToStrHelper<false, VkImageType>::Get(const VkImageType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_IMAGE_TYPE_1D)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_TYPE_2D)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_TYPE_3D)
    default: break;
  }

  return StringFormat::Fmt("VkImageType<%d>", el);
}

template <>
string ToStrHelper<false, VkImageTiling>::Get(const VkImageTiling &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_IMAGE_TILING_LINEAR)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_TILING_OPTIMAL)
    default: break;
  }

  return StringFormat::Fmt("VkImageTiling<%d>", el);
}

template <>
string ToStrHelper<false, VkImageViewType>::Get(const VkImageViewType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_1D)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_2D)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_3D)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_CUBE)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_1D_ARRAY)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_2D_ARRAY)
    TOSTR_CASE_STRINGIZE(VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
    default: break;
  }

  return StringFormat::Fmt("VkImageViewType<%d>", el);
}

template <>
string ToStrHelper<false, VkVertexInputRate>::Get(const VkVertexInputRate &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_VERTEX_INPUT_RATE_VERTEX)
    TOSTR_CASE_STRINGIZE(VK_VERTEX_INPUT_RATE_INSTANCE)
    default: break;
  }

  return StringFormat::Fmt("VkVertexInputRate<%d>", el);
}

template <>
string ToStrHelper<false, VkPolygonMode>::Get(const VkPolygonMode &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_POLYGON_MODE_FILL)
    TOSTR_CASE_STRINGIZE(VK_POLYGON_MODE_LINE)
    TOSTR_CASE_STRINGIZE(VK_POLYGON_MODE_POINT)
    default: break;
  }

  return StringFormat::Fmt("VkPolygonMode<%d>", el);
}

template <>
string ToStrHelper<false, VkFrontFace>::Get(const VkFrontFace &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_FRONT_FACE_COUNTER_CLOCKWISE)
    TOSTR_CASE_STRINGIZE(VK_FRONT_FACE_CLOCKWISE)
    default: break;
  }

  return StringFormat::Fmt("VkFrontFace<%d>", el);
}

template <>
string ToStrHelper<false, VkBlendFactor>::Get(const VkBlendFactor &el)
{
  switch(el)
  {
    case VK_BLEND_FACTOR_ZERO: return "ZERO";
    case VK_BLEND_FACTOR_ONE: return "ONE";
    case VK_BLEND_FACTOR_SRC_COLOR: return "SRC_COLOR";
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return "INV_SRC_COLOR";
    case VK_BLEND_FACTOR_DST_COLOR: return "DST_COLOR";
    case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return "INV_DST_COLOR";
    case VK_BLEND_FACTOR_SRC_ALPHA: return "SRC_ALPHA";
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return "INV_SRC_ALPHA";
    case VK_BLEND_FACTOR_DST_ALPHA: return "DST_ALPHA";
    case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return "INV_DST_ALPHA";
    case VK_BLEND_FACTOR_CONSTANT_COLOR: return "CONST_COLOR";
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return "INV_CONST_COLOR";
    case VK_BLEND_FACTOR_CONSTANT_ALPHA: return "CONST_ALPHA";
    case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return "INV_CONST_ALPHA";
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE: return "SRC_ALPHA_SAT";
    case VK_BLEND_FACTOR_SRC1_COLOR: return "SRC1_COLOR";
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return "INV_SRC1_COLOR";
    case VK_BLEND_FACTOR_SRC1_ALPHA: return "SRC1_ALPHA";
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return "INV_SRC1_ALPHA";
    default: break;
  }

  return StringFormat::Fmt("VkBlendFactor<%d>", el);
}

template <>
string ToStrHelper<false, VkBlendOp>::Get(const VkBlendOp &el)
{
  switch(el)
  {
    case VK_BLEND_OP_ADD: return "ADD";
    case VK_BLEND_OP_SUBTRACT: return "SUB";
    case VK_BLEND_OP_REVERSE_SUBTRACT: return "REV_SUB";
    case VK_BLEND_OP_MIN: return "MIN";
    case VK_BLEND_OP_MAX: return "MAX";
    default: break;
  }

  return StringFormat::Fmt("VkBlendOp<%d>", el);
}

template <>
string ToStrHelper<false, VkDynamicState>::Get(const VkDynamicState &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_VIEWPORT)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_SCISSOR)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_LINE_WIDTH)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_DEPTH_BIAS)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_BLEND_CONSTANTS)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_DEPTH_BOUNDS)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK)
    TOSTR_CASE_STRINGIZE(VK_DYNAMIC_STATE_STENCIL_REFERENCE)
    default: break;
  }

  return StringFormat::Fmt("VkDynamicState<%d>", el);
}

template <>
string ToStrHelper<false, VkAttachmentLoadOp>::Get(const VkAttachmentLoadOp &el)
{
  switch(el)
  {
    case VK_ATTACHMENT_LOAD_OP_LOAD: return "Load";
    case VK_ATTACHMENT_LOAD_OP_CLEAR: return "Clear";
    case VK_ATTACHMENT_LOAD_OP_DONT_CARE: return "Don't Care";
    default: break;
  }

  return StringFormat::Fmt("VkAttachmentLoadOp<%d>", el);
}

template <>
string ToStrHelper<false, VkAttachmentStoreOp>::Get(const VkAttachmentStoreOp &el)
{
  switch(el)
  {
    case VK_ATTACHMENT_STORE_OP_STORE: return "Store";
    case VK_ATTACHMENT_STORE_OP_DONT_CARE: return "Don't Care";
    default: break;
  }

  return StringFormat::Fmt("VkAttachmentStoreOp<%d>", el);
}

template <>
string ToStrHelper<false, VkStencilOp>::Get(const VkStencilOp &el)
{
  switch(el)
  {
    case VK_STENCIL_OP_KEEP: return "KEEP";
    case VK_STENCIL_OP_ZERO: return "ZERO";
    case VK_STENCIL_OP_REPLACE: return "REPLACE";
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP: return "INC_SAT";
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP: return "DEC_SAT";
    case VK_STENCIL_OP_INVERT: return "INVERT";
    case VK_STENCIL_OP_INCREMENT_AND_WRAP: return "INC_WRAP";
    case VK_STENCIL_OP_DECREMENT_AND_WRAP: return "DEC_WRAP";
    default: break;
  }

  return StringFormat::Fmt("VkStencilOp<%d>", el);
}

template <>
string ToStrHelper<false, VkLogicOp>::Get(const VkLogicOp &el)
{
  switch(el)
  {
    case VK_LOGIC_OP_CLEAR: return "CLEAR";
    case VK_LOGIC_OP_AND: return "AND";
    case VK_LOGIC_OP_AND_REVERSE: return "AND_REV";
    case VK_LOGIC_OP_COPY: return "COPY";
    case VK_LOGIC_OP_AND_INVERTED: return "AND_INV";
    case VK_LOGIC_OP_NO_OP: return "NOOP";
    case VK_LOGIC_OP_XOR: return "XOR";
    case VK_LOGIC_OP_OR: return "OR";
    case VK_LOGIC_OP_NOR: return "NOR";
    case VK_LOGIC_OP_EQUIVALENT: return "EQUIV";
    case VK_LOGIC_OP_INVERT: return "INVERT";
    case VK_LOGIC_OP_OR_REVERSE: return "OR_REV";
    case VK_LOGIC_OP_COPY_INVERTED: return "COPY_INV";
    case VK_LOGIC_OP_OR_INVERTED: return "OR_INV";
    case VK_LOGIC_OP_NAND: return "NAND";
    case VK_LOGIC_OP_SET: return "SET";
    default: break;
  }

  return StringFormat::Fmt("VkLogicOp<%d>", el);
}

template <>
string ToStrHelper<false, VkCompareOp>::Get(const VkCompareOp &el)
{
  switch(el)
  {
    case VK_COMPARE_OP_NEVER: return "NEVER";
    case VK_COMPARE_OP_LESS: return "LESS";
    case VK_COMPARE_OP_EQUAL: return "EQUAL";
    case VK_COMPARE_OP_LESS_OR_EQUAL: return "LESS_EQUAL";
    case VK_COMPARE_OP_GREATER: return "GREATER";
    case VK_COMPARE_OP_NOT_EQUAL: return "NOT_EQUAL";
    case VK_COMPARE_OP_GREATER_OR_EQUAL: return "GREATER_EQUAL";
    case VK_COMPARE_OP_ALWAYS: return "ALWAYS";
    default: break;
  }

  return StringFormat::Fmt("VkCompareOp<%d>", el);
}

template <>
string ToStrHelper<false, VkFilter>::Get(const VkFilter &el)
{
  switch(el)
  {
    case VK_FILTER_NEAREST: return "NEAREST";
    case VK_FILTER_LINEAR: return "LINEAR";
    default: break;
  }

  return StringFormat::Fmt("VkFilter<%d>", el);
}

template <>
string ToStrHelper<false, VkSamplerMipmapMode>::Get(const VkSamplerMipmapMode &el)
{
  switch(el)
  {
    case VK_SAMPLER_MIPMAP_MODE_NEAREST: return "NEAREST";
    case VK_SAMPLER_MIPMAP_MODE_LINEAR: return "LINEAR";
    default: break;
  }

  return StringFormat::Fmt("VkTexMipmapMode<%d>", el);
}

template <>
string ToStrHelper<false, VkSamplerAddressMode>::Get(const VkSamplerAddressMode &el)
{
  switch(el)
  {
    case VK_SAMPLER_ADDRESS_MODE_REPEAT: return "WRAP";
    case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return "MIRROR_WRAP";
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: return "CLAMP_EDGE";
    case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: return "CLAMP_BORDER";
    case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: return "MIRROR_CLAMP";
    default: break;
  }

  return StringFormat::Fmt("VkSamplerAddressMode<%d>", el);
}

template <>
string ToStrHelper<false, VkBorderColor>::Get(const VkBorderColor &el)
{
  switch(el)
  {
    case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK: return "float(0,0,0,0)";
    case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK: return "int(0,0,0,0)";
    case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK: return "float(0,0,0,1)";
    case VK_BORDER_COLOR_INT_OPAQUE_BLACK: return "int(0,0,0,1)";
    case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE: return "float(1,1,1,1)";
    case VK_BORDER_COLOR_INT_OPAQUE_WHITE: return "int(1,1,1,1)";
    default: break;
  }

  return StringFormat::Fmt("VkBorderColor<%d>", el);
}

template <>
string ToStrHelper<false, VkPrimitiveTopology>::Get(const VkPrimitiveTopology &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
    TOSTR_CASE_STRINGIZE(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
    default: break;
  }

  return StringFormat::Fmt("VkPrimitiveTopology<%d>", el);
}

template <>
string ToStrHelper<false, VkDescriptorType>::Get(const VkDescriptorType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_SAMPLER)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
    TOSTR_CASE_STRINGIZE(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
    default: break;
  }

  return StringFormat::Fmt("VkDescriptorType<%d>", el);
}

template <>
string ToStrHelper<false, VkQueryType>::Get(const VkQueryType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_QUERY_TYPE_OCCLUSION)
    TOSTR_CASE_STRINGIZE(VK_QUERY_TYPE_PIPELINE_STATISTICS)
    TOSTR_CASE_STRINGIZE(VK_QUERY_TYPE_TIMESTAMP)
    default: break;
  }

  return StringFormat::Fmt("VkQueryType<%d>", el);
}

template <>
string ToStrHelper<false, VkPhysicalDeviceType>::Get(const VkPhysicalDeviceType &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_PHYSICAL_DEVICE_TYPE_OTHER)
    TOSTR_CASE_STRINGIZE(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    TOSTR_CASE_STRINGIZE(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    TOSTR_CASE_STRINGIZE(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
    TOSTR_CASE_STRINGIZE(VK_PHYSICAL_DEVICE_TYPE_CPU)
    default: break;
  }

  return StringFormat::Fmt("VkPhysicalDeviceType<%d>", el);
}

template <>
string ToStrHelper<false, VkMemoryHeapFlagBits>::Get(const VkMemoryHeapFlagBits &el)
{
  string ret;

  if(el & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
    ret += " | VK_MEMORY_HEAP_DEVICE_LOCAL_BIT";

  if(!ret.empty())
    ret = ret.substr(3);

  if(ret.empty())
    return "-";

  return ret;
}

template <>
string ToStrHelper<false, VkMemoryPropertyFlagBits>::Get(const VkMemoryPropertyFlagBits &el)
{
  string ret;

  if(el & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    ret += " | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT";
  if(el & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    ret += " | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT";
  if(el & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    ret += " | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT";
  if(el & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    ret += " | VK_MEMORY_PROPERTY_HOST_CACHED_BIT";
  if(el & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
    ret += " | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT";

  if(!ret.empty())
    ret = ret.substr(3);
  else
    ret = "-";

  return ret;
}

template <>
string ToStrHelper<false, VkAccessFlagBits>::Get(const VkAccessFlagBits &el)
{
  string ret;

  if(el & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
    ret += " | VK_ACCESS_INDIRECT_COMMAND_READ_BIT";
  if(el & VK_ACCESS_INDEX_READ_BIT)
    ret += " | VK_ACCESS_INDEX_READ_BIT";
  if(el & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)
    ret += " | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT";
  if(el & VK_ACCESS_UNIFORM_READ_BIT)
    ret += " | VK_ACCESS_UNIFORM_READ_BIT";
  if(el & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
    ret += " | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT";
  if(el & VK_ACCESS_SHADER_READ_BIT)
    ret += " | VK_ACCESS_SHADER_READ_BIT";
  if(el & VK_ACCESS_SHADER_WRITE_BIT)
    ret += " | VK_ACCESS_SHADER_WRITE_BIT";
  if(el & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
    ret += " | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT";
  if(el & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    ret += " | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT";
  if(el & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
    ret += " | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT";
  if(el & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
    ret += " | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT";
  if(el & VK_ACCESS_TRANSFER_READ_BIT)
    ret += " | VK_ACCESS_TRANSFER_READ_BIT";
  if(el & VK_ACCESS_TRANSFER_WRITE_BIT)
    ret += " | VK_ACCESS_TRANSFER_WRITE_BIT";
  if(el & VK_ACCESS_HOST_READ_BIT)
    ret += " | VK_ACCESS_HOST_READ_BIT";
  if(el & VK_ACCESS_HOST_WRITE_BIT)
    ret += " | VK_ACCESS_HOST_WRITE_BIT";
  if(el & VK_ACCESS_MEMORY_READ_BIT)
    ret += " | VK_ACCESS_MEMORY_READ_BIT";
  if(el & VK_ACCESS_MEMORY_WRITE_BIT)
    ret += " | VK_ACCESS_MEMORY_WRITE_BIT";
  if(el & VK_ACCESS_COMMAND_PROCESS_READ_BIT_NVX)
    ret += " | VK_ACCESS_COMMAND_PROCESS_READ_BIT_NVX";
  if(el & VK_ACCESS_COMMAND_PROCESS_WRITE_BIT_NVX)
    ret += " | VK_ACCESS_COMMAND_PROCESS_WRITE_BIT_NVX";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkSharingMode>::Get(const VkSharingMode &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_SHARING_MODE_EXCLUSIVE)
    TOSTR_CASE_STRINGIZE(VK_SHARING_MODE_CONCURRENT)
    default: break;
  }

  return StringFormat::Fmt("VkSharingMode<%d>", el);
}

template <>
string ToStrHelper<false, VkCommandBufferLevel>::Get(const VkCommandBufferLevel &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_COMMAND_BUFFER_LEVEL_PRIMARY)
    TOSTR_CASE_STRINGIZE(VK_COMMAND_BUFFER_LEVEL_SECONDARY)
    default: break;
  }

  return StringFormat::Fmt("VkCommandBufferLevel<%d>", el);
}

template <>
string ToStrHelper<false, VkSubpassContents>::Get(const VkSubpassContents &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_SUBPASS_CONTENTS_INLINE)
    TOSTR_CASE_STRINGIZE(VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS)
    default: break;
  }

  return StringFormat::Fmt("VkSubpassContents<%d>", el);
}

template <>
string ToStrHelper<false, VkImageLayout>::Get(const VkImageLayout &el)
{
  switch(el)
  {
    case VK_IMAGE_LAYOUT_UNDEFINED: return "UNDEFINED";
    case VK_IMAGE_LAYOUT_GENERAL: return "GENERAL";
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "COLOR_ATTACHMENT_OPTIMAL";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "SHADER_READ_ONLY_OPTIMAL";
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "TRANSFER_SRC_OPTIMAL";
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "TRANSFER_DST_OPTIMAL";
    case VK_IMAGE_LAYOUT_PREINITIALIZED: return "PREINITIALIZED";
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "PRESENT_SRC_KHR";
    default: break;
  }

  return StringFormat::Fmt("VkImageLayout<%d>", el);
}

template <>
string ToStrHelper<false, VkStructureType>::Get(const VkStructureType &el)
{
  // cast to int for WSI enum
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_APPLICATION_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SUBMIT_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BIND_SPARSE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MEMORY_BARRIER)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_MIR_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2_KHR)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_OBJECT_TABLE_CREATE_INFO_NVX)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NVX)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_CMD_PROCESS_COMMANDS_INFO_NVX)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_CMD_RESERVE_SPACE_FOR_COMMANDS_INFO_NVX)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_GENERATED_COMMANDS_LIMITS_NVX)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_GENERATED_COMMANDS_FEATURES_NVX)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES2_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT)
    TOSTR_CASE_STRINGIZE(VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT)
    default: break;
  }

  return StringFormat::Fmt("VkStructureType<%d>", el);
}

template <>
string ToStrHelper<false, VkComponentSwizzle>::Get(const VkComponentSwizzle &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_IDENTITY)
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_ZERO)
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_ONE)
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_R)
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_G)
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_B)
    TOSTR_CASE_STRINGIZE(VK_COMPONENT_SWIZZLE_A)
    default: break;
  }

  return StringFormat::Fmt("VkComponentSwizzle<%d>", el);
}

template <>
string ToStrHelper<false, VkFormat>::Get(const VkFormat &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_FORMAT_UNDEFINED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R4G4_UNORM_PACK8)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R4G4B4A4_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B4G4R4A4_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R5G6B5_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B5G6R5_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R5G5B5A1_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B5G5R5A1_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A1R5G5B5_UNORM_PACK16)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8_SRGB)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8_SRGB)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8_SRGB)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8_SRGB)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R8G8B8A8_SRGB)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B8G8R8A8_SRGB)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_UNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_SNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_USCALED_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_SSCALED_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_UINT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_SINT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A8B8G8R8_SRGB_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2R10G10B10_UNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2R10G10B10_SNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2R10G10B10_USCALED_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2R10G10B10_SSCALED_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2R10G10B10_UINT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2R10G10B10_SINT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2B10G10R10_SNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2B10G10R10_USCALED_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2B10G10R10_SSCALED_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2B10G10R10_UINT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_A2B10G10R10_SINT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_USCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SSCALED)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R16G16B16A16_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32A32_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32A32_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R32G32B32A32_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64A64_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64A64_SINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_R64G64B64A64_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_D16_UNORM)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_X8_D24_UNORM_PACK32)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_D32_SFLOAT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_S8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_D16_UNORM_S8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_D24_UNORM_S8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_D32_SFLOAT_S8_UINT)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGB_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGB_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC1_RGBA_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC2_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC2_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC3_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC3_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC4_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC4_SNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC5_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC5_SNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC6H_UFLOAT_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC6H_SFLOAT_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC7_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_BC7_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11_SNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11G11_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_EAC_R11G11_SNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_4x4_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_4x4_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x4_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x4_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x5_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_5x5_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x5_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x5_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x6_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_6x6_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x5_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x5_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x6_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x6_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x8_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_8x8_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x5_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x5_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x6_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x6_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x8_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x8_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x10_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_10x10_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x10_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x10_SRGB_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x12_UNORM_BLOCK)
    TOSTR_CASE_STRINGIZE(VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
    default: break;
  }

  return StringFormat::Fmt("VkFormat<%d>", el);
}

template <>
string ToStrHelper<false, VkResult>::Get(const VkResult &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_SUCCESS)
    TOSTR_CASE_STRINGIZE(VK_NOT_READY)
    TOSTR_CASE_STRINGIZE(VK_TIMEOUT)
    TOSTR_CASE_STRINGIZE(VK_EVENT_SET)
    TOSTR_CASE_STRINGIZE(VK_EVENT_RESET)
    TOSTR_CASE_STRINGIZE(VK_INCOMPLETE)
    TOSTR_CASE_STRINGIZE(VK_ERROR_OUT_OF_HOST_MEMORY)
    TOSTR_CASE_STRINGIZE(VK_ERROR_OUT_OF_DEVICE_MEMORY)
    TOSTR_CASE_STRINGIZE(VK_ERROR_INITIALIZATION_FAILED)
    TOSTR_CASE_STRINGIZE(VK_ERROR_DEVICE_LOST)
    TOSTR_CASE_STRINGIZE(VK_ERROR_MEMORY_MAP_FAILED)
    TOSTR_CASE_STRINGIZE(VK_ERROR_LAYER_NOT_PRESENT)
    TOSTR_CASE_STRINGIZE(VK_ERROR_EXTENSION_NOT_PRESENT)
    TOSTR_CASE_STRINGIZE(VK_ERROR_FEATURE_NOT_PRESENT)
    TOSTR_CASE_STRINGIZE(VK_ERROR_INCOMPATIBLE_DRIVER)
    TOSTR_CASE_STRINGIZE(VK_ERROR_TOO_MANY_OBJECTS)
    TOSTR_CASE_STRINGIZE(VK_ERROR_FORMAT_NOT_SUPPORTED)
    TOSTR_CASE_STRINGIZE(VK_ERROR_SURFACE_LOST_KHR)
    TOSTR_CASE_STRINGIZE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
    TOSTR_CASE_STRINGIZE(VK_SUBOPTIMAL_KHR)
    TOSTR_CASE_STRINGIZE(VK_ERROR_OUT_OF_DATE_KHR)
    TOSTR_CASE_STRINGIZE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)
    TOSTR_CASE_STRINGIZE(VK_ERROR_VALIDATION_FAILED_EXT)
    TOSTR_CASE_STRINGIZE(VK_ERROR_INVALID_SHADER_NV)
    TOSTR_CASE_STRINGIZE(VK_ERROR_OUT_OF_POOL_MEMORY_KHR)
    default: break;
  }

  return StringFormat::Fmt("VkResult<%d>", el);
}

template <>
string ToStrHelper<false, VkMemoryType>::Get(const VkMemoryType &el)
{
  return StringFormat::Fmt("VkMemoryType<heap %u, %s>", el.heapIndex,
                           ToStr::Get((VkMemoryPropertyFlagBits)el.propertyFlags).c_str());
}

template <>
string ToStrHelper<false, VkMemoryHeap>::Get(const VkMemoryHeap &el)
{
  return StringFormat::Fmt("VkMemoryHeap<%.3fMB, %s>", float(el.size) / (1024.0f * 1024.0f),
                           ToStr::Get((VkMemoryHeapFlagBits)el.flags).c_str());
}

template <>
string ToStrHelper<false, VkRect2D>::Get(const VkRect2D &el)
{
  return StringFormat::Fmt("VkRect2D<%dx%d+%d+%d>", el.extent.width, el.extent.height, el.offset.x,
                           el.offset.y);
}

template <>
string ToStrHelper<false, VkClearRect>::Get(const VkClearRect &el)
{
  return StringFormat::Fmt("VkClearRect<%dx%d+%d+%d %u->%u>", el.rect.extent.width,
                           el.rect.extent.height, el.rect.offset.x, el.rect.offset.y,
                           el.baseArrayLayer, el.baseArrayLayer + el.layerCount);
}

template <>
string ToStrHelper<false, VkClearAttachment>::Get(const VkClearAttachment &el)
{
  return StringFormat::Fmt("%s[%u] = %s", ToStr::Get((VkImageAspectFlagBits)el.aspectMask).c_str(),
                           el.colorAttachment, ToStr::Get(el.clearValue).c_str());
}

template <>
string ToStrHelper<false, VkQueueFamilyProperties>::Get(const VkQueueFamilyProperties &el)
{
  return StringFormat::Fmt(
      "%s x %u, %u bits, %s", ToStr::Get((VkQueueFlagBits)el.queueFlags).c_str(), el.queueCount,
      el.timestampValidBits, ToStr::Get(el.minImageTransferGranularity).c_str());
}

template <>
string ToStrHelper<false, VkExtent2D>::Get(const VkExtent2D &el)
{
  return StringFormat::Fmt("VkExtent<%u,%u>", el.width, el.height);
}

template <>
string ToStrHelper<false, VkExtent3D>::Get(const VkExtent3D &el)
{
  return StringFormat::Fmt("VkExtent<%u,%u,%u>", el.width, el.height, el.depth);
}

template <>
string ToStrHelper<false, VkOffset2D>::Get(const VkOffset2D &el)
{
  return StringFormat::Fmt("VkOffset<%d,%d>", el.x, el.y);
}

template <>
string ToStrHelper<false, VkOffset3D>::Get(const VkOffset3D &el)
{
  return StringFormat::Fmt("VkOffset<%d,%d,%d>", el.x, el.y, el.z);
}

template <>
string ToStrHelper<false, VkViewport>::Get(const VkViewport &el)
{
  return StringFormat::Fmt("VkViewport<%f,%f, %fx%f, %f-%f>", el.x, el.y, el.width, el.height,
                           el.minDepth, el.maxDepth);
}

template <>
string ToStrHelper<false, VkClearColorValue>::Get(const VkClearColorValue &el)
{
  return StringFormat::Fmt("VkClearColorValue<%f,%f,%f,%f>", el.float32[0], el.float32[1],
                           el.float32[2], el.float32[3]);
}
template <>
string ToStrHelper<false, VkClearDepthStencilValue>::Get(const VkClearDepthStencilValue &el)
{
  return StringFormat::Fmt("VkClearDepthStencilValue<%f %u>", el.depth, el.stencil);
}
template <>
string ToStrHelper<false, VkClearValue>::Get(const VkClearValue &el)
{
  return StringFormat::Fmt("VkClearValue[ col:<%f,%f,%f,%f> / d:%f s:%u ]", el.color.float32[0],
                           el.color.float32[1], el.color.float32[2], el.color.float32[3],
                           el.depthStencil.depth, el.depthStencil.stencil);
}

template <>
string ToStrHelper<false, VkAttachmentReference>::Get(const VkAttachmentReference &el)
{
  return StringFormat::Fmt("VkAttachmentReference<%u, %s>", el.attachment,
                           ToStr::Get(el.layout).c_str());
}

////////////////////////////////////////////////////////////
// VK_KHR_surface
////////////////////////////////////////////////////////////

template <>
string ToStrHelper<false, VkSurfaceTransformFlagBitsKHR>::Get(const VkSurfaceTransformFlagBitsKHR &el)
{
  string ret;

  if(el & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR";
  if(el & VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR)
    ret += " | VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkCompositeAlphaFlagBitsKHR>::Get(const VkCompositeAlphaFlagBitsKHR &el)
{
  string ret;

  if(el & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    ret += " | VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR";
  if(el & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
    ret += " | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR";
  if(el & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    ret += " | VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR";
  if(el & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    ret += " | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

template <>
string ToStrHelper<false, VkColorSpaceKHR>::Get(const VkColorSpaceKHR &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_COLORSPACE_SRGB_NONLINEAR_KHR)
    default: break;
  }

  return StringFormat::Fmt("VkColorSpaceKHR<%d>", el);
}

template <>
string ToStrHelper<false, VkPresentModeKHR>::Get(const VkPresentModeKHR &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_IMMEDIATE_KHR)
    TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_MAILBOX_KHR)
    TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_FIFO_KHR)
    TOSTR_CASE_STRINGIZE(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
    default: break;
  }

  return StringFormat::Fmt("VkPresentModeKHR<%d>", el);
}

// we know the object will be a non-dispatchable object type
#define SerialiseObjectInternal(type, name, obj, opt)                         \
  {                                                                           \
    VulkanResourceManager *rm = (VulkanResourceManager *)GetUserData();       \
    ResourceId id;                                                            \
    if(m_Mode >= WRITING)                                                     \
      id = GetResID(obj);                                                     \
    Serialise(name, id);                                                      \
    if(m_Mode < WRITING)                                                      \
    {                                                                         \
      obj = VK_NULL_HANDLE;                                                   \
      if(id != ResourceId())                                                  \
      {                                                                       \
        if(rm->HasLiveResource(id))                                           \
          obj = Unwrap(rm->GetLiveHandle<type>(id));                          \
        else if(!opt)                                                         \
          /* It can be OK for a resource to have no live equivalent if the    \
          capture decided its not needed, which some APIs do fairly often. */ \
          RDCWARN("Capture may be missing reference to " #type " resource."); \
      }                                                                       \
    }                                                                         \
  }

#define SerialiseObject(type, name, obj) SerialiseObjectInternal(type, name, obj, false)
#define SerialiseObjectOptional(type, name, obj) SerialiseObjectInternal(type, name, obj, true)

static void SerialiseNext(Serialiser *ser, VkStructureType &sType, const void *&pNext)
{
  ser->Serialise("sType", sType);

  if(ser->IsReading())
  {
    pNext = NULL;
  }
  else
  {
    if(pNext == NULL)
      return;

    VkGenericStruct *next = (VkGenericStruct *)pNext;

    while(next)
    {
      // we can ignore this entirely, we don't need to serialise or replay it as we won't
      // actually use external memory. Unwrapping, if necessary, happens elsewhere
      if(next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV ||
         next->sType == VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV ||

         next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHX ||
         next->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHX)
      {
        // do nothing
      }
      // likewise we don't create real swapchains, so we can ignore surface counters
      else if(next->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT)
      {
        // do nothing
      }
      // for now we don't serialise dedicated memory on replay as it's only a performance hint,
      // and is only required in conjunction with shared memory (which we don't replay). In future
      // it might be helpful to serialise this for informational purposes.
      else if(next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV ||
              next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV ||
              next->sType == VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV)
      {
        // do nothing
      }
      else
      {
        RDCERR("Unrecognised extension structure type %d", next->sType);
      }

      next = (VkGenericStruct *)next->pNext;
    }
  }
}

template <typename T>
void SerialiseOptionalObject(Serialiser *ser, const char *name, T *&el)
{
  bool present;

  present = el != NULL;
  ser->Serialise((string(name) + "Present").c_str(), present);
  if(present)
  {
    if(ser->IsReading())
      el = new T;
    ser->Serialise(name, *el);
  }
  else if(ser->IsReading())
  {
    el = NULL;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkDeviceQueueCreateInfo &el)
{
  ScopedContext scope(this, name, "VkDeviceQueueCreateInfo", 0, true);

  // RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
  if(m_Mode >= WRITING && el.sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO)
    RDCWARN("sType not set properly: %u", el.sType);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("queueFamilyIndex", el.queueFamilyIndex);
  Serialise("queueCount", el.queueCount);
  if(m_Mode == READING)
    el.pQueuePriorities = NULL;
  SerialisePODArray("pQueuePriorities", (float *&)el.pQueuePriorities, el.queueCount);
}

// technically this doesn't need a serialise function as it's POD,
// but we give it one just for ease of printing etc.
template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceFeatures &el)
{
  ScopedContext scope(this, name, "VkPhysicalDeviceFeatures", 0, true);

  Serialise("robustBufferAccess", el.robustBufferAccess);
  Serialise("fullDrawIndexUint32", el.fullDrawIndexUint32);
  Serialise("imageCubeArray", el.imageCubeArray);
  Serialise("independentBlend", el.independentBlend);
  Serialise("geometryShader", el.geometryShader);
  Serialise("tessellationShader", el.tessellationShader);
  Serialise("sampleRateShading", el.sampleRateShading);
  Serialise("dualSrcBlend", el.dualSrcBlend);
  Serialise("logicOp", el.logicOp);
  Serialise("multiDrawIndirect", el.multiDrawIndirect);
  Serialise("drawIndirectFirstInstance", el.drawIndirectFirstInstance);
  Serialise("depthClamp", el.depthClamp);
  Serialise("depthBiasClamp", el.depthBiasClamp);
  Serialise("fillModeNonSolid", el.fillModeNonSolid);
  Serialise("depthBounds", el.depthBounds);
  Serialise("wideLines", el.wideLines);
  Serialise("largePoints", el.largePoints);
  Serialise("alphaToOne", el.alphaToOne);
  Serialise("multiViewport", el.multiViewport);
  Serialise("samplerAnisotropy", el.samplerAnisotropy);
  Serialise("textureCompressionETC2", el.textureCompressionETC2);
  Serialise("textureCompressionASTC_LDR", el.textureCompressionASTC_LDR);
  Serialise("textureCompressionBC", el.textureCompressionBC);
  Serialise("occlusionQueryPrecise", el.occlusionQueryPrecise);
  Serialise("pipelineStatisticsQuery", el.pipelineStatisticsQuery);
  Serialise("vertexPipelineStoresAndAtomics", el.vertexPipelineStoresAndAtomics);
  Serialise("fragmentStoresAndAtomics", el.fragmentStoresAndAtomics);
  Serialise("shaderTessellationAndGeometryPointSize", el.shaderTessellationAndGeometryPointSize);
  Serialise("shaderImageGatherExtended", el.shaderImageGatherExtended);
  Serialise("shaderStorageImageExtendedFormats", el.shaderStorageImageExtendedFormats);
  Serialise("shaderStorageImageMultisample", el.shaderStorageImageMultisample);
  Serialise("shaderStorageImageReadWithoutFormat", el.shaderStorageImageReadWithoutFormat);
  Serialise("shaderStorageImageWriteWithoutFormat", el.shaderStorageImageWriteWithoutFormat);
  Serialise("shaderUniformBufferArrayDynamicIndexing", el.shaderUniformBufferArrayDynamicIndexing);
  Serialise("shaderSampledImageArrayDynamicIndexing", el.shaderSampledImageArrayDynamicIndexing);
  Serialise("shaderStorageBufferArrayDynamicIndexing", el.shaderStorageBufferArrayDynamicIndexing);
  Serialise("shaderStorageImageArrayDynamicIndexing", el.shaderStorageImageArrayDynamicIndexing);
  Serialise("shaderClipDistance", el.shaderClipDistance);
  Serialise("shaderCullDistance", el.shaderCullDistance);
  Serialise("shaderFloat64", el.shaderFloat64);
  Serialise("shaderInt64", el.shaderInt64);
  Serialise("shaderInt16", el.shaderInt16);
  Serialise("shaderResourceResidency", el.shaderResourceResidency);
  Serialise("shaderResourceMinLod", el.shaderResourceMinLod);
  Serialise("sparseBinding", el.sparseBinding);
  Serialise("sparseResidencyBuffer", el.sparseResidencyBuffer);
  Serialise("sparseResidencyImage2D", el.sparseResidencyImage2D);
  Serialise("sparseResidencyImage3D", el.sparseResidencyImage3D);
  Serialise("sparseResidency2Samples", el.sparseResidency2Samples);
  Serialise("sparseResidency4Samples", el.sparseResidency4Samples);
  Serialise("sparseResidency8Samples", el.sparseResidency8Samples);
  Serialise("sparseResidency16Samples", el.sparseResidency16Samples);
  Serialise("sparseResidencyAliased", el.sparseResidencyAliased);
  Serialise("variableMultisampleRate", el.variableMultisampleRate);
  Serialise("inheritedQueries", el.inheritedQueries);
}

template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceMemoryProperties &el)
{
  ScopedContext scope(this, name, "VkPhysicalDeviceMemoryProperties", 0, true);

  VkMemoryType *types = el.memoryTypes;
  VkMemoryHeap *heaps = el.memoryHeaps;

  SerialisePODArray("memoryTypes", types, el.memoryTypeCount);
  SerialisePODArray("memoryHeaps", heaps, el.memoryHeapCount);
}

template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceLimits &el)
{
  ScopedContext scope(this, name, "VkPhysicalDeviceLimits", 0, true);

  Serialise("maxImageDimension1D", el.maxImageDimension1D);
  Serialise("maxImageDimension2D", el.maxImageDimension2D);
  Serialise("maxImageDimension3D", el.maxImageDimension3D);
  Serialise("maxImageDimensionCube", el.maxImageDimensionCube);
  Serialise("maxImageArrayLayers", el.maxImageArrayLayers);
  Serialise("maxTexelBufferElements", el.maxTexelBufferElements);
  Serialise("maxUniformBufferRange", el.maxUniformBufferRange);
  Serialise("maxStorageBufferRange", el.maxStorageBufferRange);
  Serialise("maxPushConstantsSize", el.maxPushConstantsSize);
  Serialise("maxMemoryAllocationCount", el.maxMemoryAllocationCount);
  Serialise("maxSamplerAllocationCount", el.maxSamplerAllocationCount);
  Serialise("bufferImageGranularity", el.bufferImageGranularity);
  Serialise("sparseAddressSpaceSize", el.sparseAddressSpaceSize);
  Serialise("maxBoundDescriptorSets", el.maxBoundDescriptorSets);
  Serialise("maxPerStageDescriptorSamplers", el.maxPerStageDescriptorSamplers);
  Serialise("maxPerStageDescriptorUniformBuffers", el.maxPerStageDescriptorUniformBuffers);
  Serialise("maxPerStageDescriptorStorageBuffers", el.maxPerStageDescriptorStorageBuffers);
  Serialise("maxPerStageDescriptorSampledImages", el.maxPerStageDescriptorSampledImages);
  Serialise("maxPerStageDescriptorStorageImages", el.maxPerStageDescriptorStorageImages);
  Serialise("maxPerStageDescriptorInputAttachments", el.maxPerStageDescriptorInputAttachments);
  Serialise("maxPerStageResources", el.maxPerStageResources);
  Serialise("maxDescriptorSetSamplers", el.maxDescriptorSetSamplers);
  Serialise("maxDescriptorSetUniformBuffers", el.maxDescriptorSetUniformBuffers);
  Serialise("maxDescriptorSetUniformBuffersDynamic", el.maxDescriptorSetUniformBuffersDynamic);
  Serialise("maxDescriptorSetStorageBuffers", el.maxDescriptorSetStorageBuffers);
  Serialise("maxDescriptorSetStorageBuffersDynamic", el.maxDescriptorSetStorageBuffersDynamic);
  Serialise("maxDescriptorSetSampledImages", el.maxDescriptorSetSampledImages);
  Serialise("maxDescriptorSetStorageImages", el.maxDescriptorSetStorageImages);
  Serialise("maxDescriptorSetInputAttachments", el.maxDescriptorSetInputAttachments);
  Serialise("maxVertexInputAttributes", el.maxVertexInputAttributes);
  Serialise("maxVertexInputBindings", el.maxVertexInputBindings);
  Serialise("maxVertexInputAttributeOffset", el.maxVertexInputAttributeOffset);
  Serialise("maxVertexInputBindingStride", el.maxVertexInputBindingStride);
  Serialise("maxVertexOutputComponents", el.maxVertexOutputComponents);
  Serialise("maxTessellationGenerationLevel", el.maxTessellationGenerationLevel);
  Serialise("maxTessellationPatchSize", el.maxTessellationPatchSize);
  Serialise("maxTessellationControlPerVertexInputComponents",
            el.maxTessellationControlPerVertexInputComponents);
  Serialise("maxTessellationControlPerVertexOutputComponents",
            el.maxTessellationControlPerVertexOutputComponents);
  Serialise("maxTessellationControlPerPatchOutputComponents",
            el.maxTessellationControlPerPatchOutputComponents);
  Serialise("maxTessellationControlTotalOutputComponents",
            el.maxTessellationControlTotalOutputComponents);
  Serialise("maxTessellationEvaluationInputComponents", el.maxTessellationEvaluationInputComponents);
  Serialise("maxTessellationEvaluationOutputComponents",
            el.maxTessellationEvaluationOutputComponents);
  Serialise("maxGeometryShaderInvocations", el.maxGeometryShaderInvocations);
  Serialise("maxGeometryInputComponents", el.maxGeometryInputComponents);
  Serialise("maxGeometryOutputComponents", el.maxGeometryOutputComponents);
  Serialise("maxGeometryOutputVertices", el.maxGeometryOutputVertices);
  Serialise("maxGeometryTotalOutputComponents", el.maxGeometryTotalOutputComponents);
  Serialise("maxFragmentInputComponents", el.maxFragmentInputComponents);
  Serialise("maxFragmentOutputAttachments", el.maxFragmentOutputAttachments);
  Serialise("maxFragmentDualSrcAttachments", el.maxFragmentDualSrcAttachments);
  Serialise("maxFragmentCombinedOutputResources", el.maxFragmentCombinedOutputResources);
  Serialise("maxComputeSharedMemorySize", el.maxComputeSharedMemorySize);
  SerialisePODArray<3>("maxComputeWorkGroupCount", el.maxComputeWorkGroupCount);
  Serialise("maxComputeWorkGroupInvocations", el.maxComputeWorkGroupInvocations);
  SerialisePODArray<3>("maxComputeWorkGroupSize", el.maxComputeWorkGroupSize);
  Serialise("subPixelPrecisionBits", el.subPixelPrecisionBits);
  Serialise("subTexelPrecisionBits", el.subTexelPrecisionBits);
  Serialise("mipmapPrecisionBits", el.mipmapPrecisionBits);
  Serialise("maxDrawIndexedIndexValue", el.maxDrawIndexedIndexValue);
  Serialise("maxDrawIndirectCount", el.maxDrawIndirectCount);
  Serialise("maxSamplerLodBias", el.maxSamplerLodBias);
  Serialise("maxSamplerAnisotropy", el.maxSamplerAnisotropy);
  Serialise("maxViewports", el.maxViewports);
  SerialisePODArray<2>("maxViewportDimensions", el.maxViewportDimensions);
  SerialisePODArray<2>("viewportBoundsRange", el.viewportBoundsRange);
  Serialise("viewportSubPixelBits", el.viewportSubPixelBits);
  uint64_t minMemoryMapAlignment = (uint64_t)el.minMemoryMapAlignment;
  Serialise("minMemoryMapAlignment", minMemoryMapAlignment);
  el.minMemoryMapAlignment = (size_t)minMemoryMapAlignment;
  Serialise("minTexelBufferOffsetAlignment", el.minTexelBufferOffsetAlignment);
  Serialise("minUniformBufferOffsetAlignment", el.minUniformBufferOffsetAlignment);
  Serialise("minStorageBufferOffsetAlignment", el.minStorageBufferOffsetAlignment);
  Serialise("minTexelOffset", el.minTexelOffset);
  Serialise("maxTexelOffset", el.maxTexelOffset);
  Serialise("minTexelGatherOffset", el.minTexelGatherOffset);
  Serialise("maxTexelGatherOffset", el.maxTexelGatherOffset);
  Serialise("minInterpolationOffset", el.minInterpolationOffset);
  Serialise("maxInterpolationOffset", el.maxInterpolationOffset);
  Serialise("subPixelInterpolationOffsetBits", el.subPixelInterpolationOffsetBits);
  Serialise("maxFramebufferWidth", el.maxFramebufferWidth);
  Serialise("maxFramebufferHeight", el.maxFramebufferHeight);
  Serialise("maxFramebufferLayers", el.maxFramebufferLayers);
  Serialise("framebufferColorSampleCounts", el.framebufferColorSampleCounts);
  Serialise("framebufferDepthSampleCounts", el.framebufferDepthSampleCounts);
  Serialise("framebufferStencilSampleCounts", el.framebufferStencilSampleCounts);
  Serialise("framebufferNoAttachmentsSampleCounts", el.framebufferNoAttachmentsSampleCounts);
  Serialise("maxColorAttachments", el.maxColorAttachments);
  Serialise("sampledImageColorSampleCounts", el.sampledImageColorSampleCounts);
  Serialise("sampledImageIntegerSampleCounts", el.sampledImageIntegerSampleCounts);
  Serialise("sampledImageDepthSampleCounts", el.sampledImageDepthSampleCounts);
  Serialise("sampledImageStencilSampleCounts", el.sampledImageStencilSampleCounts);
  Serialise("storageImageSampleCounts", el.storageImageSampleCounts);
  Serialise("maxSampleMaskWords", el.maxSampleMaskWords);
  Serialise("timestampComputeAndGraphics", el.timestampComputeAndGraphics);
  Serialise("timestampPeriod", el.timestampPeriod);
  Serialise("maxClipDistances", el.maxClipDistances);
  Serialise("maxCullDistances", el.maxCullDistances);
  Serialise("maxCombinedClipAndCullDistances", el.maxCombinedClipAndCullDistances);
  Serialise("discreteQueuePriorities", el.discreteQueuePriorities);
  SerialisePODArray<2>("pointSizeRange", el.pointSizeRange);
  SerialisePODArray<2>("lineWidthRange", el.lineWidthRange);
  Serialise("pointSizeGranularity", el.pointSizeGranularity);
  Serialise("lineWidthGranularity", el.lineWidthGranularity);
  Serialise("strictLines", el.strictLines);
  Serialise("standardSampleLocations", el.standardSampleLocations);
  Serialise("optimalBufferCopyOffsetAlignment", el.optimalBufferCopyOffsetAlignment);
  Serialise("optimalBufferCopyRowPitchAlignment", el.optimalBufferCopyRowPitchAlignment);
  Serialise("nonCoherentAtomSize", el.nonCoherentAtomSize);
}

template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceSparseProperties &el)
{
  ScopedContext scope(this, name, "VkPhysicalDeviceSparseProperties", 0, true);

  Serialise("residencyStandard2DBlockShape", el.residencyStandard2DBlockShape);
  Serialise("residencyStandard2DMultisampleBlockShape", el.residencyStandard2DMultisampleBlockShape);
  Serialise("residencyStandard3DBlockShape", el.residencyStandard3DBlockShape);
  Serialise("residencyAlignedMipSize", el.residencyAlignedMipSize);
  Serialise("residencyNonResidentStrict", el.residencyNonResidentStrict);
}

template <>
void Serialiser::Serialise(const char *name, VkPhysicalDeviceProperties &el)
{
  ScopedContext scope(this, name, "VkPhysicalDeviceProperties", 0, true);

  Serialise("apiVersion", el.apiVersion);
  Serialise("driverVersion", el.driverVersion);
  Serialise("vendorID", el.vendorID);
  Serialise("deviceID", el.deviceID);
  Serialise("deviceType", el.deviceType);

  string deviceName;
  if(m_Mode == WRITING)
    deviceName = el.deviceName;
  Serialise("deviceName", deviceName);
  if(m_Mode == READING)
  {
    RDCEraseEl(el.deviceName);
    memcpy(el.deviceName, deviceName.c_str(),
           RDCMIN(deviceName.size(), (size_t)VK_MAX_PHYSICAL_DEVICE_NAME_SIZE));
  }

  SerialisePODArray<VK_UUID_SIZE>("pipelineCacheUUID", el.pipelineCacheUUID);
  Serialise("limits", el.limits);
  Serialise("sparseProperties", el.sparseProperties);
}

template <>
void Serialiser::Serialise(const char *name, VkDeviceCreateInfo &el)
{
  ScopedContext scope(this, name, "VkDeviceCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseComplexArray("pQueueCreateInfos", (VkDeviceQueueCreateInfo *&)el.pQueueCreateInfos,
                        el.queueCreateInfoCount);

  // need to do this by hand to use string DB
  Serialise("extensionCount", el.enabledExtensionCount);

  if(m_Mode == READING)
    el.ppEnabledExtensionNames =
        el.enabledExtensionCount ? new char *[el.enabledExtensionCount] : NULL;

  // cast away const on array so we can assign to it on reading
  const char **exts = (const char **)el.ppEnabledExtensionNames;
  for(uint32_t i = 0; i < el.enabledExtensionCount; i++)
  {
    string s = "";
    if(m_Mode == WRITING && exts[i] != NULL)
      s = exts[i];

    Serialise("ppEnabledExtensionNames", s);

    if(m_Mode == READING)
    {
      m_StringDB.insert(s);
      exts[i] = m_StringDB.find(s)->c_str();
    }
  }

  // need to do this by hand to use string DB
  Serialise("layerCount", el.enabledLayerCount);

  if(m_Mode == READING)
    el.ppEnabledLayerNames = el.enabledLayerCount ? new char *[el.enabledLayerCount] : NULL;

  // cast away const on array so we can assign to it on reading
  const char **layers = (const char **)el.ppEnabledLayerNames;
  for(uint32_t i = 0; i < el.enabledLayerCount; i++)
  {
    string s = "";
    if(m_Mode == WRITING && layers[i] != NULL)
      s = layers[i];

    Serialise("ppEnabledLayerNames", s);

    if(m_Mode == READING)
    {
      m_StringDB.insert(s);
      layers[i] = m_StringDB.find(s)->c_str();
    }
  }

  SerialiseOptionalObject(this, "pEnabledFeatures", (VkPhysicalDeviceFeatures *&)el.pEnabledFeatures);
}

template <>
void Serialiser::Deserialise(const VkDeviceCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    for(uint32_t i = 0; i < el->queueCreateInfoCount; i++)
      delete[] el->pQueueCreateInfos[i].pQueuePriorities;
    delete[] el->pQueueCreateInfos;
    delete[] el->ppEnabledExtensionNames;
    delete[] el->ppEnabledLayerNames;
    delete el->pEnabledFeatures;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkBufferCreateInfo &el)
{
  ScopedContext scope(this, name, "VkBufferCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkBufferCreateFlagBits &)el.flags);
  Serialise("size", el.size);
  Serialise("usage", (VkBufferUsageFlagBits &)el.usage);
  Serialise("sharingMode", el.sharingMode);
  if(m_Mode == READING)
  {
    el.pQueueFamilyIndices = NULL;
    el.queueFamilyIndexCount = 0;
  }
  if(el.sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    SerialisePODArray("pQueueFamilyIndices", (uint32_t *&)el.pQueueFamilyIndices,
                      el.queueFamilyIndexCount);
  }
  else
  {
    // for backwards compatibility with captures, ignore the family count and serialise empty array
    uint32_t zero = 0;
    uint32_t *empty = NULL;
    SerialisePODArray("pQueueFamilyIndices", empty, zero);
    delete[] empty;
  }
}

template <>
void Serialiser::Deserialise(const VkBufferCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pQueueFamilyIndices;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkBufferViewCreateInfo &el)
{
  ScopedContext scope(this, name, "VkBufferViewCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseObject(VkBuffer, "buffer", el.buffer);
  Serialise("format", el.format);
  Serialise("offset", el.offset);
  Serialise("range", el.range);
}

template <>
void Serialiser::Serialise(const char *name, VkImageCreateInfo &el)
{
  ScopedContext scope(this, name, "VkImageCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkImageCreateFlagBits &)el.flags);
  Serialise("imageType", el.imageType);
  Serialise("format", el.format);
  Serialise("extent", el.extent);
  Serialise("mipLevels", el.mipLevels);
  Serialise("arraySize", el.arrayLayers);
  Serialise("samples", el.samples);
  Serialise("tiling", el.tiling);
  Serialise("usage", (VkImageUsageFlagBits &)el.usage);
  Serialise("sharingMode", el.sharingMode);
  if(m_Mode == READING)
  {
    el.pQueueFamilyIndices = NULL;
    el.queueFamilyIndexCount = 0;
  }
  if(el.sharingMode == VK_SHARING_MODE_CONCURRENT)
  {
    SerialisePODArray("pQueueFamilyIndices", (uint32_t *&)el.pQueueFamilyIndices,
                      el.queueFamilyIndexCount);
  }
  else
  {
    // for backwards compatibility with captures, ignore the family count and serialise empty array
    uint32_t zero = 0;
    uint32_t empty[1] = {0};
    SerialisePODArray("pQueueFamilyIndices", (uint32_t *&)empty, zero);
  }
  Serialise("initialLayout", el.initialLayout);
}

template <>
void Serialiser::Deserialise(const VkImageCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pQueueFamilyIndices;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkImageViewCreateInfo &el)
{
  ScopedContext scope(this, name, "VkImageViewCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseObject(VkImage, "image", el.image);
  Serialise("viewType", el.viewType);
  Serialise("format", el.format);
  Serialise("components", el.components);
  Serialise("subresourceRange", el.subresourceRange);
}

template <>
void Serialiser::Serialise(const char *name, VkSparseMemoryBind &el)
{
  ScopedContext scope(this, name, "VkSparseMemoryBind", 0, true);

  Serialise("resourceOffset", el.resourceOffset);
  Serialise("size", el.size);
  SerialiseObject(VkDeviceMemory, "memory", el.memory);
  Serialise("memoryOffset", el.memoryOffset);
  Serialise("flags", (VkSparseMemoryBindFlagBits &)el.flags);
}

template <>
void Serialiser::Serialise(const char *name, VkSparseBufferMemoryBindInfo &el)
{
  ScopedContext scope(this, name, "VkSparseBufferMemoryBindInfo", 0, true);

  SerialiseObject(VkBuffer, "buffer", el.buffer);
  SerialiseComplexArray("pBinds", (VkSparseMemoryBind *&)el.pBinds, el.bindCount);
}

template <>
void Serialiser::Serialise(const char *name, VkSparseImageOpaqueMemoryBindInfo &el)
{
  ScopedContext scope(this, name, "VkSparseImageOpaqueMemoryBindInfo", 0, true);

  SerialiseObject(VkImage, "image", el.image);
  SerialiseComplexArray("pBinds", (VkSparseMemoryBind *&)el.pBinds, el.bindCount);
}

template <>
void Serialiser::Serialise(const char *name, VkSparseImageMemoryBind &el)
{
  ScopedContext scope(this, name, "VkSparseImageMemoryBind", 0, true);

  Serialise("subresource", el.subresource);
  Serialise("offset", el.offset);
  Serialise("extent", el.extent);
  SerialiseObject(VkDeviceMemory, "memory", el.memory);
  Serialise("memoryOffset", el.memoryOffset);
  Serialise("flags", (VkSparseMemoryBindFlagBits &)el.flags);
}

template <>
void Serialiser::Serialise(const char *name, VkSparseImageMemoryBindInfo &el)
{
  ScopedContext scope(this, name, "VkSparseImageMemoryBindInfo", 0, true);

  SerialiseObject(VkImage, "image", el.image);
  SerialiseComplexArray("pBinds", (VkSparseImageMemoryBind *&)el.pBinds, el.bindCount);
}

template <>
void Serialiser::Serialise(const char *name, VkBindSparseInfo &el)
{
  ScopedContext scope(this, name, "VkBindSparseInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BIND_SPARSE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  // do this one by hand because it's an array of objects that aren't Serialise
  // overloaded
  Serialise("waitSemaphoreCount", el.waitSemaphoreCount);

  if(m_Mode == READING)
    el.pWaitSemaphores = el.waitSemaphoreCount ? new VkSemaphore[el.waitSemaphoreCount] : NULL;

  VkSemaphore *waitsems = (VkSemaphore *)el.pWaitSemaphores;
  for(uint32_t i = 0; i < el.waitSemaphoreCount; i++)
    SerialiseObject(VkSemaphore, "pWaitSemaphores", waitsems[i]);

  SerialiseComplexArray("pBufferBinds", (VkSparseBufferMemoryBindInfo *&)el.pBufferBinds,
                        el.bufferBindCount);
  SerialiseComplexArray("pImageOpaqueBinds",
                        (VkSparseImageOpaqueMemoryBindInfo *&)el.pImageOpaqueBinds,
                        el.imageOpaqueBindCount);
  SerialiseComplexArray("pImageBinds", (VkSparseImageMemoryBindInfo *&)el.pImageBinds,
                        el.imageBindCount);

  // do this one by hand because it's an array of objects that aren't Serialise
  // overloaded
  Serialise("signalSemaphoreCount", el.signalSemaphoreCount);

  if(m_Mode == READING)
    el.pSignalSemaphores = el.signalSemaphoreCount ? new VkSemaphore[el.signalSemaphoreCount] : NULL;

  VkSemaphore *sigsems = (VkSemaphore *)el.pSignalSemaphores;
  for(uint32_t i = 0; i < el.signalSemaphoreCount; i++)
    SerialiseObject(VkSemaphore, "pSignalSemaphores", sigsems[i]);
}

template <>
void Serialiser::Deserialise(const VkBindSparseInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pWaitSemaphores;
    for(uint32_t i = 0; i < el->bufferBindCount; i++)
      delete[] el->pBufferBinds[i].pBinds;
    delete[] el->pBufferBinds;
    for(uint32_t i = 0; i < el->imageOpaqueBindCount; i++)
      delete[] el->pImageOpaqueBinds[i].pBinds;
    delete[] el->pImageOpaqueBinds;
    delete[] el->pImageBinds;
    delete[] el->pSignalSemaphores;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkFramebufferCreateInfo &el)
{
  ScopedContext scope(this, name, "VkFramebufferCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
  Serialise("width", el.width);
  Serialise("height", el.height);
  Serialise("layers", el.layers);

  // do this one by hand because it's an array of objects that aren't Serialise
  // overloaded
  Serialise("attachmentCount", el.attachmentCount);

  if(m_Mode == READING)
    el.pAttachments = el.attachmentCount ? new VkImageView[el.attachmentCount] : NULL;

  VkImageView *attaches = (VkImageView *)el.pAttachments;
  for(uint32_t i = 0; i < el.attachmentCount; i++)
    SerialiseObject(VkImageView, "pAttachments", attaches[i]);
}

template <>
void Serialiser::Deserialise(const VkFramebufferCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pAttachments;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkAttachmentDescription &el)
{
  ScopedContext scope(this, name, "VkAttachmentDescription", 0, true);

  Serialise("flags", (VkAttachmentDescriptionFlagBits &)el.flags);
  Serialise("format", el.format);
  Serialise("samples", el.samples);
  Serialise("loadOp", el.loadOp);
  Serialise("storeOp", el.storeOp);
  Serialise("stencilLoadOp", el.stencilLoadOp);
  Serialise("stencilStoreOp", el.stencilStoreOp);
  Serialise("initialLayout", el.initialLayout);
  Serialise("finalLayout", el.finalLayout);
}

template <>
void Serialiser::Serialise(const char *name, VkSubpassDescription &el)
{
  ScopedContext scope(this, name, "VkSubpassDescription", 0, true);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("pipelineBindPoint", el.pipelineBindPoint);
  SerialiseOptionalObject(this, "pDepthStencilAttachment",
                          (VkAttachmentReference *&)el.pDepthStencilAttachment);

  if(m_Mode == READING)
  {
    el.pInputAttachments = NULL;
    el.pColorAttachments = NULL;
    el.pResolveAttachments = NULL;
    el.pPreserveAttachments = NULL;
  }

  SerialisePODArray("inputAttachments", (VkAttachmentReference *&)el.pInputAttachments,
                    el.inputAttachmentCount);
  SerialisePODArray("colorAttachments", (VkAttachmentReference *&)el.pColorAttachments,
                    el.colorAttachmentCount);

  bool hasResolves = (el.pResolveAttachments != NULL);
  Serialise("hasResolves", hasResolves);

  if(hasResolves)
    SerialisePODArray("resolveAttachments", (VkAttachmentReference *&)el.pResolveAttachments,
                      el.colorAttachmentCount);

  SerialisePODArray("preserveAttachments", (VkAttachmentReference *&)el.pPreserveAttachments,
                    el.preserveAttachmentCount);
}

template <>
void Serialiser::Serialise(const char *name, VkSubpassDependency &el)
{
  ScopedContext scope(this, name, "VkSubpassDependency", 0, true);

  Serialise("srcSubpass", el.srcSubpass);
  Serialise("destSubpass", el.dstSubpass);
  Serialise("srcStageMask", (VkPipelineStageFlagBits &)el.srcStageMask);
  Serialise("destStageMask", (VkPipelineStageFlagBits &)el.dstStageMask);
  Serialise("srcAccessMask", (VkAccessFlagBits &)el.srcAccessMask);
  Serialise("dstAccessMask", (VkAccessFlagBits &)el.dstAccessMask);
  Serialise("dependencyFlags", (VkDependencyFlagBits &)el.dependencyFlags);
}

template <>
void Serialiser::Serialise(const char *name, VkRenderPassCreateInfo &el)
{
  ScopedContext scope(this, name, "VkRenderPassCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseComplexArray("pAttachments", (VkAttachmentDescription *&)el.pAttachments,
                        el.attachmentCount);
  SerialiseComplexArray("pSubpasses", (VkSubpassDescription *&)el.pSubpasses, el.subpassCount);
  SerialiseComplexArray("pDependencies", (VkSubpassDependency *&)el.pDependencies,
                        el.dependencyCount);
}

template <>
void Serialiser::Deserialise(const VkRenderPassCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pAttachments;
    for(uint32_t i = 0; i < el->subpassCount; i++)
    {
      delete el->pSubpasses[i].pDepthStencilAttachment;
      delete[] el->pSubpasses[i].pInputAttachments;
      delete[] el->pSubpasses[i].pColorAttachments;
      delete[] el->pSubpasses[i].pResolveAttachments;
      if(el->pSubpasses[i].pPreserveAttachments)
        delete[] el->pSubpasses[i].pPreserveAttachments;
    }
    delete[] el->pSubpasses;
    delete[] el->pDependencies;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkRenderPassBeginInfo &el)
{
  ScopedContext scope(this, name, "VkRenderPassBeginInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
  SerialiseObject(VkFramebuffer, "framebuffer", el.framebuffer);
  Serialise("renderArea", el.renderArea);

  if(m_Mode == READING)
    el.pClearValues = NULL;
  SerialisePODArray("pClearValues", (VkClearValue *&)el.pClearValues, el.clearValueCount);
}

template <>
void Serialiser::Deserialise(const VkRenderPassBeginInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pClearValues;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkVertexInputBindingDescription &el)
{
  ScopedContext scope(this, name, "VkVertexInputBindingDescription", 0, true);

  Serialise("binding", el.binding);
  Serialise("strideInBytes", el.stride);
  Serialise("inputRate", el.inputRate);
}

template <>
void Serialiser::Serialise(const char *name, VkVertexInputAttributeDescription &el)
{
  ScopedContext scope(this, name, "VkVertexInputAttributeDescription", 0, true);

  Serialise("location", el.location);
  Serialise("binding", el.binding);
  Serialise("format", el.format);
  Serialise("offset", el.offset);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineVertexInputStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineVertexInputStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseComplexArray("pVertexBindingDescriptions",
                        (VkVertexInputBindingDescription *&)el.pVertexBindingDescriptions,
                        el.vertexBindingDescriptionCount);
  SerialiseComplexArray("pVertexAttributeDescriptions",
                        (VkVertexInputAttributeDescription *&)el.pVertexAttributeDescriptions,
                        el.vertexAttributeDescriptionCount);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineInputAssemblyStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineInputAssemblyStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("topology", el.topology);
  Serialise("primitiveRestartEnable", el.primitiveRestartEnable);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineTessellationStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineTessStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("patchControlPoints", el.patchControlPoints);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineViewportStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineViewportStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);

  if(m_Mode == READING)
  {
    el.pViewports = NULL;
    el.pScissors = NULL;
  }

  // need to handle these arrays potentially being NULL if they're dynamic
  bool hasViews = (el.pViewports != NULL);
  bool hasScissors = (el.pScissors != NULL);

  Serialise("hasViews", hasViews);
  Serialise("hasScissors", hasScissors);

  if(hasViews)
    SerialisePODArray("viewports", (VkViewport *&)el.pViewports, el.viewportCount);
  else
    Serialise("viewportCount", el.viewportCount);

  if(hasScissors)
    SerialisePODArray("scissors", (VkRect2D *&)el.pScissors, el.scissorCount);
  else
    Serialise("scissorCount", el.scissorCount);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineRasterizationStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineRasterStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("depthClampEnable", el.depthClampEnable);
  Serialise("rasterizerDiscardEnable", el.rasterizerDiscardEnable);
  Serialise("polygonMode", el.polygonMode);
  Serialise("cullMode", el.cullMode);
  Serialise("frontFace", el.frontFace);
  Serialise("depthBiasEnable", el.depthBiasEnable);
  Serialise("depthBiasConstantFactor", el.depthBiasConstantFactor);
  Serialise("depthBiasClamp", el.depthBiasClamp);
  Serialise("depthBiasSlopeFactor", el.depthBiasSlopeFactor);
  Serialise("lineWidth", el.lineWidth);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineMultisampleStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineMultisampleStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("rasterizationSamples", el.rasterizationSamples);
  RDCASSERT(el.rasterizationSamples <= VK_SAMPLE_COUNT_32_BIT);
  Serialise("sampleShadingEnable", el.sampleShadingEnable);
  Serialise("minSampleShading", el.minSampleShading);
  SerialiseOptionalObject(this, "sampleMask", (VkSampleMask *&)el.pSampleMask);
  Serialise("alphaToCoverageEnable", el.alphaToCoverageEnable);
  Serialise("alphaToOneEnable", el.alphaToOneEnable);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineColorBlendAttachmentState &el)
{
  ScopedContext scope(this, name, "VkPipelineColorBlendAttachmentState", 0, true);

  Serialise("blendEnable", el.blendEnable);
  Serialise("srcColorBlendFactor", el.srcColorBlendFactor);
  Serialise("dstColorBlendFactor", el.dstColorBlendFactor);
  Serialise("colorBlendOp", el.colorBlendOp);
  Serialise("srcAlphaBlendFactor", el.srcAlphaBlendFactor);
  Serialise("dstAlphaBlendFactor", el.dstAlphaBlendFactor);
  Serialise("alphaBlendOp", el.alphaBlendOp);
  Serialise("channelWriteMask", el.colorWriteMask);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineColorBlendStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineColorBlendStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("logicOpEnable", el.logicOpEnable);
  Serialise("logicOp", el.logicOp);

  Serialise("attachmentCount", el.attachmentCount);

  SerialiseComplexArray("pAttachments", (VkPipelineColorBlendAttachmentState *&)el.pAttachments,
                        el.attachmentCount);

  SerialisePODArray<4>("blendConstants", el.blendConstants);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineDepthStencilStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineDepthStencilStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING ||
            el.sType == VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("depthTestEnable", el.depthTestEnable);
  Serialise("depthWriteEnable", el.depthWriteEnable);
  Serialise("depthCompareOp", el.depthCompareOp);
  Serialise("depthBoundsTestEnable", el.depthBoundsTestEnable);
  Serialise("stencilEnable", el.stencilTestEnable);
  Serialise("front", el.front);
  Serialise("back", el.back);
  Serialise("minDepthBounds", el.minDepthBounds);
  Serialise("maxDepthBounds", el.maxDepthBounds);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineDynamicStateCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineDynamicStateCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  if(m_Mode == READING)
    el.pDynamicStates = NULL;
  SerialisePODArray("dynamicStates", (VkDynamicState *&)el.pDynamicStates, el.dynamicStateCount);
}

template <>
void Serialiser::Serialise(const char *name, VkCommandPoolCreateInfo &el)
{
  ScopedContext scope(this, name, "VkCommandPoolCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkCommandPoolCreateFlagBits &)el.flags);
  Serialise("queueFamilyIndex", el.queueFamilyIndex);
}

template <>
void Serialiser::Serialise(const char *name, VkCommandBufferAllocateInfo &el)
{
  ScopedContext scope(this, name, "VkCommandBufferAllocateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  SerialiseObject(VkCommandPool, "commandPool", el.commandPool);
  Serialise("level", el.level);
  Serialise("bufferCount", el.commandBufferCount);
}

template <>
void Serialiser::Serialise(const char *name, VkCommandBufferInheritanceInfo &el)
{
  ScopedContext scope(this, name, "VkCommandBufferInheritanceInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
  Serialise("subpass", el.subpass);
  SerialiseObject(VkFramebuffer, "framebuffer", el.framebuffer);
  Serialise("occlusionQueryEnable", el.occlusionQueryEnable);
  Serialise("queryFlags", (VkQueryControlFlagBits &)el.queryFlags);
  Serialise("pipelineStatistics", (VkQueryPipelineStatisticFlagBits &)el.pipelineStatistics);
}

template <>
void Serialiser::Serialise(const char *name, VkCommandBufferBeginInfo &el)
{
  ScopedContext scope(this, name, "VkCommandBufferBeginInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkCommandBufferUsageFlagBits &)el.flags);
  SerialiseOptionalObject(this, "el.pInheritanceInfo",
                          (VkCommandBufferInheritanceInfo *&)el.pInheritanceInfo);
}

template <>
void Serialiser::Deserialise(const VkCommandBufferBeginInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete el->pInheritanceInfo;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkStencilOpState &el)
{
  ScopedContext scope(this, name, "VkStencilOpState", 0, true);

  Serialise("failOp", el.failOp);
  Serialise("passOp", el.passOp);
  Serialise("depthFailOp", el.depthFailOp);
  Serialise("compareOp", el.compareOp);
  Serialise("compareMask", el.compareMask);
  Serialise("writeMask", el.writeMask);
  Serialise("reference", el.reference);
}

template <>
void Serialiser::Serialise(const char *name, VkQueryPoolCreateInfo &el)
{
  ScopedContext scope(this, name, "VkQueryPoolCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("queryType", el.queryType);
  Serialise("queryCount", el.queryCount);
  Serialise("pipelineStatistics", (VkQueryPipelineStatisticFlagBits &)el.pipelineStatistics);
}

template <>
void Serialiser::Serialise(const char *name, VkSemaphoreCreateInfo &el)
{
  ScopedContext scope(this, name, "VkSemaphoreCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
}

template <>
void Serialiser::Serialise(const char *name, VkEventCreateInfo &el)
{
  ScopedContext scope(this, name, "VkEventCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
}

template <>
void Serialiser::Serialise(const char *name, VkFenceCreateInfo &el)
{
  ScopedContext scope(this, name, "VkFenceCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFenceCreateFlagBits &)el.flags);
}

template <>
void Serialiser::Serialise(const char *name, VkSamplerCreateInfo &el)
{
  ScopedContext scope(this, name, "VkSamplerCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("minFilter", el.minFilter);
  Serialise("magFilter", el.magFilter);
  Serialise("mipmapMode", el.mipmapMode);
  Serialise("addressModeU", el.addressModeU);
  Serialise("addressModeV", el.addressModeV);
  Serialise("addressModeW", el.addressModeW);
  Serialise("mipLodBias", el.mipLodBias);
  Serialise("anisotropyEnable", el.anisotropyEnable);
  Serialise("maxAnisotropy", el.maxAnisotropy);
  Serialise("compareEnable", el.compareEnable);
  Serialise("compareOp", el.compareOp);
  Serialise("minLod", el.minLod);
  Serialise("maxLod", el.maxLod);
  Serialise("borderColor", el.borderColor);
  Serialise("unnormalizedCoordinates", el.unnormalizedCoordinates);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineShaderStageCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineShaderStageCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  Serialise("stage", el.stage);
  SerialiseObject(VkShaderModule, "module", el.module);

  string s = "";
  if(m_Mode >= WRITING && el.pName != NULL)
    s = el.pName;

  Serialise("pName", s);

  if(m_Mode == READING)
  {
    if(s == "")
    {
      el.pName = "";
    }
    else
    {
      string str;
      str.assign((char *)m_BufferHead - s.length(), s.length());
      m_StringDB.insert(str);
      el.pName = m_StringDB.find(str)->c_str();
    }
  }

  SerialiseOptionalObject(this, "el.pSpecializationInfo",
                          (VkSpecializationInfo *&)el.pSpecializationInfo);
}

template <>
void Serialiser::Serialise(const char *name, VkSpecializationMapEntry &el)
{
  ScopedContext scope(this, name, "VkSpecializationMapEntry", 0, true);

  Serialise("constantId", el.constantID);
  Serialise("offset", el.offset);
  uint64_t size = el.size;
  Serialise("size", size);
  if(m_Mode == READING)
    el.size = (size_t)size;
}

template <>
void Serialiser::Serialise(const char *name, VkSpecializationInfo &el)
{
  ScopedContext scope(this, name, "VkSpecializationInfo", 0, true);

  uint64_t dataSize = el.dataSize;
  Serialise("dataSize", dataSize);
  size_t sz = (size_t)dataSize;
  if(m_Mode == READING)
  {
    el.pData = NULL;
    el.dataSize = sz;
  }
  SerialiseBuffer("pData", (byte *&)el.pData, sz);

  SerialiseComplexArray("pMapEntries", (VkSpecializationMapEntry *&)el.pMapEntries, el.mapEntryCount);
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineCacheCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineCacheCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);

  uint64_t initialDataSize = el.initialDataSize;
  Serialise("codeSize", initialDataSize);
  el.initialDataSize = (size_t)initialDataSize;

  if(m_Mode == READING)
    el.pInitialData = NULL;
  SerialiseBuffer("initialData", (byte *&)el.pInitialData, el.initialDataSize);
}

template <>
void Serialiser::Deserialise(const VkPipelineCacheCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[](byte *)(el->pInitialData);
  }
}

template <>
void Serialiser::Serialise(const char *name, VkPipelineLayoutCreateInfo &el)
{
  ScopedContext scope(this, name, "VkPipelineLayoutCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);

  // need to do this one by hand since it's just an array of objects that don't themselves have
  // a Serialise overload
  Serialise("descriptorSetCount", el.setLayoutCount);

  if(m_Mode == READING)
    el.pSetLayouts = el.setLayoutCount ? new VkDescriptorSetLayout[el.setLayoutCount] : NULL;

  // cast away const on array so we can assign to it on reading
  VkDescriptorSetLayout *layouts = (VkDescriptorSetLayout *)el.pSetLayouts;
  for(uint32_t i = 0; i < el.setLayoutCount; i++)
    SerialiseObject(VkDescriptorSetLayout, "layout", layouts[i]);

  SerialiseComplexArray("pPushConstantRanges", (VkPushConstantRange *&)el.pPushConstantRanges,
                        el.pushConstantRangeCount);
}

template <>
void Serialiser::Deserialise(const VkPipelineLayoutCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pSetLayouts;
    delete[] el->pPushConstantRanges;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkShaderModuleCreateInfo &el)
{
  ScopedContext scope(this, name, "VkShaderModuleCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);

  uint64_t codeSize = el.codeSize;
  Serialise("codeSize", codeSize);
  el.codeSize = (size_t)codeSize;

  size_t sz = (size_t)codeSize;
  if(m_Mode == READING)
    el.pCode = NULL;
  SerialiseBuffer("pCode", (byte *&)el.pCode, sz);
}

template <>
void Serialiser::Deserialise(const VkShaderModuleCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[](byte *)(el->pCode);
  }
}

template <>
void Serialiser::Serialise(const char *name, VkImageSubresourceRange &el)
{
  ScopedContext scope(this, name, "VkImageSubresourceRange", 0, true);

  Serialise("aspectMask", (VkImageAspectFlagBits &)el.aspectMask);
  Serialise("baseMipLevel", el.baseMipLevel);
  Serialise("levelCount", el.levelCount);
  Serialise("baseArrayLayer", el.baseArrayLayer);
  Serialise("layerCount", el.layerCount);
}

template <>
void Serialiser::Serialise(const char *name, VkImageSubresourceLayers &el)
{
  ScopedContext scope(this, name, "VkImageSubresourceLayers", 0, true);

  Serialise("aspectMask", (VkImageAspectFlagBits &)el.aspectMask);
  Serialise("mipLevel", el.mipLevel);
  Serialise("baseArrayLayer", el.baseArrayLayer);
  Serialise("layerCount", el.layerCount);
}

template <>
void Serialiser::Serialise(const char *name, VkImageSubresource &el)
{
  ScopedContext scope(this, name, "VkImageSubresource", 0, true);

  Serialise("aspectMask", (VkImageAspectFlagBits &)el.aspectMask);
  Serialise("mipLevel", el.mipLevel);
  Serialise("arrayLayer", el.arrayLayer);
}

template <>
void Serialiser::Serialise(const char *name, VkMemoryAllocateInfo &el)
{
  ScopedContext scope(this, name, "VkMemoryAllocateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("allocationSize", el.allocationSize);
  Serialise("memoryTypeIndex", el.memoryTypeIndex);
}

template <>
void Serialiser::Serialise(const char *name, VkMemoryBarrier &el)
{
  ScopedContext scope(this, name, "VkMemoryBarrier", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_MEMORY_BARRIER);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("srcAccessMask", (VkAccessFlagBits &)el.srcAccessMask);
  Serialise("dstAccessMask", (VkAccessFlagBits &)el.dstAccessMask);
}

template <>
void Serialiser::Serialise(const char *name, VkBufferMemoryBarrier &el)
{
  ScopedContext scope(this, name, "VkBufferMemoryBarrier", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("srcAccessMask", (VkAccessFlagBits &)el.srcAccessMask);
  Serialise("dstAccessMask", (VkAccessFlagBits &)el.dstAccessMask);
  // serialise as signed because then QUEUE_FAMILY_IGNORED is -1 and queue
  // family index won't be legitimately larger than 2 billion
  Serialise("srcQueueFamilyIndex", (int32_t &)el.srcQueueFamilyIndex);
  Serialise("dstQueueFamilyIndex", (int32_t &)el.dstQueueFamilyIndex);
  SerialiseObject(VkBuffer, "buffer", el.buffer);
  Serialise("offset", el.offset);
  Serialise("size", el.size);
}

template <>
void Serialiser::Serialise(const char *name, VkImageMemoryBarrier &el)
{
  ScopedContext scope(this, name, "VkImageMemoryBarrier", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("srcAccessMask", (VkAccessFlagBits &)el.srcAccessMask);
  Serialise("dstAccessMask", (VkAccessFlagBits &)el.dstAccessMask);
  Serialise("oldLayout", el.oldLayout);
  Serialise("newLayout", el.newLayout);
  // serialise as signed because then QUEUE_FAMILY_IGNORED is -1 and queue
  // family index won't be legitimately larger than 2 billion
  Serialise("srcQueueFamilyIndex", (int32_t &)el.srcQueueFamilyIndex);
  Serialise("dstQueueFamilyIndex", (int32_t &)el.dstQueueFamilyIndex);
  SerialiseObject(VkImage, "image", el.image);
  Serialise("subresourceRange", el.subresourceRange);
}

template <>
void Serialiser::Serialise(const char *name, VkGraphicsPipelineCreateInfo &el)
{
  ScopedContext scope(this, name, "VkGraphicsPipelineCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkPipelineCreateFlagBits &)el.flags);
  SerialiseObject(VkPipelineLayout, "layout", el.layout);
  SerialiseObject(VkRenderPass, "renderPass", el.renderPass);
  Serialise("subpass", el.subpass);
  SerialiseObject(VkPipeline, "basePipelineHandle", el.basePipelineHandle);
  Serialise("basePipelineIndex", el.basePipelineIndex);

  SerialiseOptionalObject(this, "pVertexInputState",
                          (VkPipelineVertexInputStateCreateInfo *&)el.pVertexInputState);
  SerialiseOptionalObject(this, "pInputAssemblyState",
                          (VkPipelineInputAssemblyStateCreateInfo *&)el.pInputAssemblyState);
  SerialiseOptionalObject(this, "pTessellationState",
                          (VkPipelineTessellationStateCreateInfo *&)el.pTessellationState);
  SerialiseOptionalObject(this, "pViewportState",
                          (VkPipelineViewportStateCreateInfo *&)el.pViewportState);
  SerialiseOptionalObject(this, "pRasterState",
                          (VkPipelineRasterizationStateCreateInfo *&)el.pRasterizationState);
  SerialiseOptionalObject(this, "pMultisampleState",
                          (VkPipelineMultisampleStateCreateInfo *&)el.pMultisampleState);
  SerialiseOptionalObject(this, "pDepthStencilState",
                          (VkPipelineDepthStencilStateCreateInfo *&)el.pDepthStencilState);
  SerialiseOptionalObject(this, "pColorBlendState",
                          (VkPipelineColorBlendStateCreateInfo *&)el.pColorBlendState);
  SerialiseOptionalObject(this, "pDynamicState",
                          (VkPipelineDynamicStateCreateInfo *&)el.pDynamicState);

  SerialiseComplexArray("pStages", (VkPipelineShaderStageCreateInfo *&)el.pStages, el.stageCount);
}

template <>
void Serialiser::Deserialise(const VkGraphicsPipelineCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    if(el->pVertexInputState)
    {
      RDCASSERT(el->pVertexInputState->pNext == NULL);    // otherwise delete
      delete[] el->pVertexInputState->pVertexBindingDescriptions;
      delete[] el->pVertexInputState->pVertexAttributeDescriptions;
      delete el->pVertexInputState;
    }
    if(el->pInputAssemblyState)
    {
      RDCASSERT(el->pInputAssemblyState->pNext == NULL);    // otherwise delete
      delete el->pInputAssemblyState;
    }
    if(el->pTessellationState)
    {
      RDCASSERT(el->pTessellationState->pNext == NULL);    // otherwise delete
      delete el->pTessellationState;
    }
    if(el->pViewportState)
    {
      RDCASSERT(el->pViewportState->pNext == NULL);    // otherwise delete
      if(el->pViewportState->pViewports)
        delete[] el->pViewportState->pViewports;
      if(el->pViewportState->pScissors)
        delete[] el->pViewportState->pScissors;
      delete el->pViewportState;
    }
    if(el->pRasterizationState)
    {
      RDCASSERT(el->pRasterizationState->pNext == NULL);    // otherwise delete
      delete el->pRasterizationState;
    }
    if(el->pMultisampleState)
    {
      RDCASSERT(el->pMultisampleState->pNext == NULL);    // otherwise delete
      delete el->pMultisampleState->pSampleMask;
      delete el->pMultisampleState;
    }
    if(el->pDepthStencilState)
    {
      RDCASSERT(el->pDepthStencilState->pNext == NULL);    // otherwise delete
      delete el->pDepthStencilState;
    }
    if(el->pColorBlendState)
    {
      RDCASSERT(el->pColorBlendState->pNext == NULL);    // otherwise delete
      delete[] el->pColorBlendState->pAttachments;
      delete el->pColorBlendState;
    }
    if(el->pDynamicState)
    {
      RDCASSERT(el->pDynamicState->pNext == NULL);    // otherwise delete
      if(el->pDynamicState->pDynamicStates)
        delete[] el->pDynamicState->pDynamicStates;
      delete el->pDynamicState;
    }
    for(uint32_t i = 0; i < el->stageCount; i++)
    {
      RDCASSERT(el->pStages[i].pNext == NULL);    // otherwise delete
      if(el->pStages[i].pSpecializationInfo)
      {
        delete[](byte *)(el->pStages[i].pSpecializationInfo->pData);
        delete[] el->pStages[i].pSpecializationInfo->pMapEntries;
        delete el->pStages[i].pSpecializationInfo;
      }
    }
    delete[] el->pStages;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkComputePipelineCreateInfo &el)
{
  ScopedContext scope(this, name, "VkComputePipelineCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("stage", el.stage);
  Serialise("flags", (VkPipelineCreateFlagBits &)el.flags);
  SerialiseObject(VkPipelineLayout, "layout", el.layout);
  SerialiseObject(VkPipeline, "basePipelineHandle", el.basePipelineHandle);
  Serialise("basePipelineIndex", el.basePipelineIndex);
}

template <>
void Serialiser::Deserialise(const VkComputePipelineCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);          // otherwise delete
    RDCASSERT(el->stage.pNext == NULL);    // otherwise delete
    if(el->stage.pSpecializationInfo)
    {
      delete[](byte *)(el->stage.pSpecializationInfo->pData);
      delete[] el->stage.pSpecializationInfo->pMapEntries;
      delete el->stage.pSpecializationInfo;
    }
  }
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorPoolSize &el)
{
  ScopedContext scope(this, name, "VkDescriptorPoolSize", 0, true);

  Serialise("type", el.type);
  Serialise("descriptorCount", el.descriptorCount);
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorPoolCreateInfo &el)
{
  ScopedContext scope(this, name, "VkDescriptorPoolCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkDescriptorPoolCreateFlagBits &)el.flags);
  Serialise("maxSets", el.maxSets);
  SerialiseComplexArray("pTypeCount", (VkDescriptorPoolSize *&)el.pPoolSizes, el.poolSizeCount);
}

template <>
void Serialiser::Deserialise(const VkDescriptorPoolCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pPoolSizes;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorSetAllocateInfo &el)
{
  ScopedContext scope(this, name, "VkDescriptorSetAllocateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  SerialiseObject(VkDescriptorPool, "descriptorPool", el.descriptorPool);

  // need to do this one by hand since it's just an array of objects that don't themselves have
  // a Serialise overload
  Serialise("descriptorSetCount", el.descriptorSetCount);

  if(m_Mode == READING)
    el.pSetLayouts = el.descriptorSetCount ? new VkDescriptorSetLayout[el.descriptorSetCount] : NULL;

  // cast away const on array so we can assign to it on reading
  VkDescriptorSetLayout *layouts = (VkDescriptorSetLayout *)el.pSetLayouts;
  for(uint32_t i = 0; i < el.descriptorSetCount; i++)
    SerialiseObject(VkDescriptorSetLayout, "pSetLayouts", layouts[i]);
}

template <>
void Serialiser::Deserialise(const VkDescriptorSetAllocateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    delete[] el->pSetLayouts;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorImageInfo &el)
{
  ScopedContext scope(this, name, "VkDescriptorImageInfo", 0, true);

  SerialiseObjectOptional(VkSampler, "sampler", el.sampler);
  SerialiseObjectOptional(VkImageView, "imageView", el.imageView);
  Serialise("imageLayout", el.imageLayout);
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorBufferInfo &el)
{
  ScopedContext scope(this, name, "VkDescriptorBufferInfo", 0, true);

  SerialiseObjectOptional(VkBuffer, "buffer", el.buffer);
  Serialise("offset", el.offset);
  Serialise("range", el.range);
}

template <>
void Serialiser::Serialise(const char *name, VkWriteDescriptorSet &el)
{
  ScopedContext scope(this, name, "VkWriteDescriptorSet", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
  SerialiseNext(this, el.sType, el.pNext);

  SerialiseObjectOptional(VkDescriptorSet, "dstSet", el.dstSet);
  Serialise("dstBinding", el.dstBinding);
  Serialise("dstArrayElement", el.dstArrayElement);
  Serialise("descriptorType", el.descriptorType);

  if(m_Mode == READING)
  {
    el.pImageInfo = NULL;
    el.pBufferInfo = NULL;
    el.pTexelBufferView = NULL;
  }

  // only serialise the array type used, the others are ignored
  if(el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
     el.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
  {
    SerialiseComplexArray("pImageInfo", (VkDescriptorImageInfo *&)el.pImageInfo, el.descriptorCount);
  }
  else if(el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
  {
    SerialiseComplexArray("pBufferInfo", (VkDescriptorBufferInfo *&)el.pBufferInfo,
                          el.descriptorCount);
  }
  else if(el.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
          el.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
  {
    // need to do this one by hand since it's just an array of objects that don't themselves have
    // a Serialise overload
    Serialise("descriptorCount", el.descriptorCount);

    if(m_Mode == READING)
      el.pTexelBufferView = el.descriptorCount ? new VkBufferView[el.descriptorCount] : NULL;

    // cast away const on array so we can assign to it on reading
    VkBufferView *views = (VkBufferView *)el.pTexelBufferView;
    for(uint32_t i = 0; i < el.descriptorCount; i++)
      SerialiseObjectOptional(VkBufferView, "pTexelBufferView", views[i]);
  }
}

template <>
void Serialiser::Deserialise(const VkWriteDescriptorSet *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    if(el->pImageInfo)
      delete[] el->pImageInfo;
    if(el->pBufferInfo)
      delete[] el->pBufferInfo;
    if(el->pTexelBufferView)
      delete[] el->pTexelBufferView;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkCopyDescriptorSet &el)
{
  ScopedContext scope(this, name, "VkCopyDescriptorSet", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
  SerialiseNext(this, el.sType, el.pNext);

  SerialiseObjectOptional(VkDescriptorSet, "srcSet", el.srcSet);
  Serialise("srcBinding", el.srcBinding);
  Serialise("srcArrayElement", el.srcArrayElement);
  SerialiseObjectOptional(VkDescriptorSet, "destSet", el.dstSet);
  Serialise("destBinding", el.dstBinding);
  Serialise("destArrayElement", el.dstArrayElement);

  Serialise("descriptorCount", el.descriptorCount);
}

template <>
void Serialiser::Serialise(const char *name, VkPushConstantRange &el)
{
  ScopedContext scope(this, name, "VkPushConstantRange", 0, true);

  Serialise("stageFlags", (VkShaderStageFlagBits &)el.stageFlags);
  Serialise("offset", el.offset);
  Serialise("size", el.size);
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutBinding &el)
{
  ScopedContext scope(this, name, "VkDescriptorSetLayoutBinding", 0, true);

  Serialise("binding", el.binding);
  Serialise("descriptorType", el.descriptorType);
  Serialise("descriptorCount", el.descriptorCount);
  Serialise("stageFlags", (VkShaderStageFlagBits &)el.stageFlags);

  bool hasSamplers = el.pImmutableSamplers != NULL;
  Serialise("hasSamplers", hasSamplers);

  // do this one by hand because it's an array of objects that aren't Serialise
  // overloaded
  if(m_Mode == READING)
  {
    if(hasSamplers)
      el.pImmutableSamplers = el.descriptorCount ? new VkSampler[el.descriptorCount] : NULL;
    else
      el.pImmutableSamplers = NULL;
  }

  VkSampler *samplers = (VkSampler *)el.pImmutableSamplers;

  for(uint32_t i = 0; hasSamplers && i < el.descriptorCount; i++)
  {
    SerialiseObject(VkSampler, "pImmutableSampler", samplers[i]);
  }
}

template <>
void Serialiser::Serialise(const char *name, VkDescriptorSetLayoutCreateInfo &el)
{
  ScopedContext scope(this, name, "VkDescriptorSetLayoutCreateInfo", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);
  SerialiseComplexArray("pBindings", (VkDescriptorSetLayoutBinding *&)el.pBindings, el.bindingCount);
}

template <>
void Serialiser::Deserialise(const VkDescriptorSetLayoutCreateInfo *const el) const
{
  if(m_Mode == READING)
  {
    RDCASSERT(el->pNext == NULL);    // otherwise delete
    for(uint32_t i = 0; i < el->bindingCount; i++)
      if(el->pBindings[i].pImmutableSamplers)
        delete[] el->pBindings[i].pImmutableSamplers;
    delete[] el->pBindings;
  }
}

template <>
void Serialiser::Serialise(const char *name, VkComponentMapping &el)
{
  ScopedContext scope(this, name, "VkComponentMapping", 0, true);

  Serialise("r", el.r);
  Serialise("g", el.g);
  Serialise("b", el.b);
  Serialise("a", el.a);
}

template <>
void Serialiser::Serialise(const char *name, VkBufferImageCopy &el)
{
  ScopedContext scope(this, name, "VkBufferImageCopy", 0, true);

  Serialise("memOffset", el.bufferOffset);
  Serialise("bufferRowLength", el.bufferRowLength);
  Serialise("bufferImageHeight", el.bufferImageHeight);
  Serialise("imageSubresource", el.imageSubresource);
  Serialise("imageOffset", el.imageOffset);
  Serialise("imageExtent", el.imageExtent);
}

template <>
void Serialiser::Serialise(const char *name, VkBufferCopy &el)
{
  ScopedContext scope(this, name, "VkBufferCopy", 0, true);

  Serialise("srcOffset", el.srcOffset);
  Serialise("dstOffset", el.dstOffset);
  Serialise("size", el.size);
}

template <>
void Serialiser::Serialise(const char *name, VkImageCopy &el)
{
  ScopedContext scope(this, name, "VkImageCopy", 0, true);

  Serialise("srcSubresource", el.srcSubresource);
  Serialise("srcOffset", el.srcOffset);
  Serialise("dstSubresource", el.dstSubresource);
  Serialise("dstOffset", el.dstOffset);
  Serialise("extent", el.extent);
}

template <>
void Serialiser::Serialise(const char *name, VkImageBlit &el)
{
  ScopedContext scope(this, name, "VkImageBlit", 0, true);

  Serialise("srcSubresource", el.srcSubresource);
  SerialisePODArray<2>("srcOffsets", el.srcOffsets);
  Serialise("dstSubresource", el.dstSubresource);
  SerialisePODArray<2>("dstOffsets", el.dstOffsets);
}

template <>
void Serialiser::Serialise(const char *name, VkImageResolve &el)
{
  ScopedContext scope(this, name, "VkImageResolve", 0, true);

  Serialise("srcSubresource", el.srcSubresource);
  Serialise("srcOffset", el.srcOffset);
  Serialise("dstSubresource", el.dstSubresource);
  Serialise("dstOffset", el.dstOffset);
  Serialise("extent", el.extent);
}

template <>
void Serialiser::Serialise(const char *name, VkRect2D &el)
{
  ScopedContext scope(this, name, "VkRect2D", 0, true);

  Serialise("offset", el.offset);
  Serialise("extent", el.extent);
}

template <>
void Serialiser::Serialise(const char *name, VkSwapchainCreateInfoKHR &el)
{
  ScopedContext scope(this, name, "VkSwapchainCreateInfoKHR", 0, true);

  RDCASSERT(m_Mode < WRITING || el.sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
  SerialiseNext(this, el.sType, el.pNext);

  Serialise("flags", (VkFlagWithNoBits &)el.flags);

  // don't need the surface

  Serialise("minImageCount", el.minImageCount);
  Serialise("imageFormat", el.imageFormat);
  Serialise("imageColorSpace", el.imageColorSpace);
  Serialise("imageExtent", el.imageExtent);
  Serialise("imageArrayLayers", el.imageArrayLayers);
  Serialise("imageUsage", el.imageUsage);

  // SHARING: sharingMode, queueFamilyCount, pQueueFamilyIndices

  Serialise("preTransform", el.preTransform);
  Serialise("compositeAlpha", el.compositeAlpha);
  Serialise("presentMode", el.presentMode);
  Serialise("clipped", el.clipped);

  // don't need the old swap chain
}

// this isn't a real vulkan type, it's our own "anything that could be in a descriptor"
// structure that
template <>
void Serialiser::Serialise(const char *name, DescriptorSetSlot &el)
{
  SerialiseObject(VkBuffer, "bufferInfo.buffer", el.bufferInfo.buffer);
  Serialise("bufferInfo.offset", el.bufferInfo.offset);
  Serialise("bufferInfo.range", el.bufferInfo.range);

  SerialiseObject(VkSampler, "imageInfo.sampler", el.imageInfo.sampler);
  SerialiseObject(VkImageView, "imageInfo.imageView", el.imageInfo.imageView);
  Serialise("imageInfo.imageLayout", el.imageInfo.imageLayout);

  SerialiseObject(VkBufferView, "texelBufferView", el.texelBufferView);
}
