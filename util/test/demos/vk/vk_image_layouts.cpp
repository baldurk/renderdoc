/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "vk_test.h"

TEST(VK_Image_Layouts, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Tests edge-cases of image layout transitions, such as images being in UNDEFINED, "
      "PREINITIALIZED or PRESENT_SRC at the start of the frame.";

  std::string common = R"EOSHADER(

#version 420 core

struct v2f
{
	vec4 pos;
	vec4 col;
	vec4 uv;
};

)EOSHADER";

  const std::string vertex = R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

  const std::string pixel = R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos), vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(common + vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(common + pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    AllocatedBuffer vb(
        allocator, vkh::BufferCreateInfo(sizeof(DefaultTri), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(DefaultTri);

    vkh::ImageCreateInfo preinitInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    preinitInfo.tiling = VK_IMAGE_TILING_LINEAR;
    preinitInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    const VkPhysicalDeviceMemoryProperties *props = NULL;
    vmaGetMemoryProperties(allocator, &props);

    while(Running())
    {
      VkImage preinitImg = VK_NULL_HANDLE;
      VkDeviceMemory preinitMem = VK_NULL_HANDLE;

      vkCreateImage(device, preinitInfo, NULL, &preinitImg);

      setName(preinitImg, "Image:Preinitialised");

      AllocatedImage undefImg(allocator, vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                  VK_IMAGE_USAGE_SAMPLED_BIT),
                              VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

      setName(undefImg.image, "Image:Undefined");

      {
        VkMemoryRequirements mrq;
        vkGetImageMemoryRequirements(device, preinitImg, &mrq);

        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = mrq.size;
        info.memoryTypeIndex = 100;

        for(uint32_t i = 0; i < props->memoryTypeCount; i++)
        {
          if(mrq.memoryTypeBits & (1 << i) &&
             (props->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
          {
            info.memoryTypeIndex = i;
            break;
          }
        }

        TEST_ASSERT(info.memoryTypeIndex != 100, "Couldn't find compatible memory type");

        vkAllocateMemory(device, &info, NULL, &preinitMem);
        vkBindImageMemory(device, preinitImg, preinitMem, 0);

        void *data = NULL;
        vkMapMemory(device, preinitMem, 0, mrq.size, 0, &data);
        memset(data, 0x40, (size_t)mrq.size);
        vkUnmapMemory(device, preinitMem);
      }

      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg = mainWindow->GetImage();
      if((size_t)curFrame <= mainWindow->GetCount())
        setName(swapimg, "Image:Swapchain");

      setMarker(cmd, "Before Transition");

      // after the first N frames, we expect the swapchain to be in PRESENT_SRC
      vkh::cmdPipelineBarrier(cmd,
                              {
                                  vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                          (size_t)curFrame <= mainWindow->GetCount()
                                                              ? VK_IMAGE_LAYOUT_UNDEFINED
                                                              : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                          VK_IMAGE_LAYOUT_GENERAL, swapimg),
                              });

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.4f, 0.5f, 0.6f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      // the manual images are transitioned into general for copying, from pre-initialised and
      // undefined
      vkh::cmdPipelineBarrier(
          cmd, {
                   vkh::ImageMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                           VK_IMAGE_LAYOUT_PREINITIALIZED,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, preinitImg),
                   vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, undefImg.image),
               });

      VkImageCopy region = {
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          {0, 0, 0},
          {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          {0, 0, 0},
          {4, 4, 1},
      };

      vkCmdCopyImage(cmd, preinitImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, undefImg.image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      vkCmdBeginRenderPass(
          cmd, vkh::RenderPassBeginInfo(mainWindow->rp, mainWindow->GetFB(), mainWindow->scissor),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();

      vkDeviceWaitIdle(device);

      vkDestroyImage(device, preinitImg, NULL);
      vkFreeMemory(device, preinitMem, NULL);
    }

    return 0;
  }
};

REGISTER_TEST();
