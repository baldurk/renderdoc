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

#include "vk_test.h"

RD_TEST(VK_Mesh_Zoo, VulkanGraphicsTest)
{
  static constexpr const char *Description = "Draws some primitives for testing the mesh view.";

  std::string xfbvert = R"EOSHADER(
#version 460 core

void main()
{
	gl_Position = vec4(0,0,0,0);
}

)EOSHADER";

  std::string xfbgeom = R"EOSHADER(
#version 450 core

layout(points, invocations = 1) in;
layout(points, max_vertices = 8) out;

layout(location = 0, stream = 0, xfb_buffer = 0, xfb_stride = 32, xfb_offset = 0) out vec2 uv0;
layout(location = 1, stream = 0, xfb_buffer = 0, xfb_stride = 32, xfb_offset = 8) out vec3 fakepos;
layout(location = 2, stream = 0, xfb_buffer = 0, xfb_stride = 32, xfb_offset = 20) out vec3 extra;
layout(location = 3, stream = 1, xfb_buffer = 1, xfb_stride = 8, xfb_offset = 0) out float a;
layout(location = 4, stream = 1, xfb_buffer = 1, xfb_stride = 8, xfb_offset = 4) out float b;

layout(stream = 2, xfb_buffer = 2, xfb_stride = 24, xfb_offset = 0) out gl_PerVertex {
    vec4 gl_Position;
};
layout(location = 5, stream = 2, xfb_buffer = 2, xfb_stride = 24, xfb_offset = 16) out vec2 uv1;

void main()
{
  uv0 = vec2(1,2);
  fakepos = vec3(3,4,5);
  extra = vec3(6,7,8);
	EmitStreamVertex(0);
	EndStreamPrimitive(0);
	
	a = 9;b = 10;
	EmitStreamVertex(1);
	EndStreamPrimitive(1);
	
	a = 11;b = 12;
	EmitStreamVertex(1);
	EndStreamPrimitive(1);

  gl_Position = vec4(0.8, 0.8, 0.0, 1.0);
  uv1 = vec2(0.5, 0.5);
	EmitStreamVertex(2);
	EndStreamPrimitive(2);
  gl_Position = vec4(0.8, 0.9, 0.0, 1.0);
  uv1 = vec2(0.6, 0.6);
	EmitStreamVertex(2);
	EndStreamPrimitive(2);
  gl_Position = vec4(0.9, 0.8, 0.0, 1.0);
  uv1 = vec2(0.7, 0.7);
	EmitStreamVertex(2);
	EndStreamPrimitive(2);
  gl_Position = vec4(0.9, 0.9, 0.0, 1.0);
  uv1 = vec2(0.8, 0.8);
	EmitStreamVertex(2);
	EndStreamPrimitive(2);
}


)EOSHADER";

  std::string xfbpixel = R"EOSHADER(
#version 460 core

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vec4(0, 1, 1, 1);
}

)EOSHADER";

  std::string vertex = R"EOSHADER(
#version 460 core

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;

layout(push_constant, std140) uniform pushbuf
{
  vec4 scale;
  vec4 offset;
};

layout(location = 0) out vec2 vertOutCol2;
layout(location = 1) out vec4 vertOutcol;

layout(constant_id = 1) const int spec_canary = 0;

void main()
{
  if(spec_canary != 1337)
  {
    gl_Position = vec4(-1, -1, -1, 1);
    vertOutcol = vec4(0, 0, 0, 0);
    vertOutCol2 = vec2(0, 0);
#if defined(USE_POINTS)
    gl_PointSize = 0.0f;
#endif
    return;
  }

	vec4 pos = vec4(Position.xy * scale.xy + offset.xy, Position.z, 1.0f);
	vertOutcol = Color;

  if(gl_InstanceIndex > 0)
  {
    pos *= 0.3f;
    pos.xy += vec2(0.1f);
    vertOutcol.x = 1.0f; 
  }

  vertOutCol2.xy = pos.xy;

	gl_Position = pos * vec4(1, -1 * offset.w, 1, 1);
#if defined(USE_POINTS)
  gl_PointSize = 1.0f;
#endif
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0) in vec2 vertInCol2;
layout(location = 1) in vec4 vertIncol;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIncol + 1.0e-20 * vertInCol2.xyxy;
}

)EOSHADER";

  bool useXfbStreamPipe = false;

  void Prepare(int argc, char **argv)
  {
    optDevExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);

    optFeatures.geometryShader = VK_TRUE;

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceTransformFeedbackFeaturesEXT xfb = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT,
    };

    if(hasExt(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME))
    {
      VkPhysicalDeviceTransformFeedbackPropertiesEXT xfbProp = {
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT,
      };
      getPhysFeatures2(&xfb);
      getPhysProperties2(&xfbProp);

      useXfbStreamPipe = xfb.transformFeedback && xfb.geometryStreams &&
                         xfbProp.maxTransformFeedbackStreams >= 3 &&
                         xfbProp.transformFeedbackRasterizationStreamSelect;

      devInfoNext = &xfb;
    }
  }

  int main()
  {
    optDevExts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    optDevExts.push_back(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    bool KHR_maintenance1 = hasExt(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    const DefaultA2V test[] = {
        // single color quad
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        // points, to test vertex picking
        {Vec3f(50.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 250.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(250.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(50.0f, 50.0f, 0.2f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(70.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(170.0f, 170.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(70.0f, 70.0f, 0.1f), Vec4f(1.0f, 0.0f, 1.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };

    // create depth-stencil image
    AllocatedImage depthimg(
        this,
        vkh::ImageCreateInfo(mainWindow->scissor.extent.width, mainWindow->scissor.extent.height, 0,
                             VK_FORMAT_D32_SFLOAT_S8_UINT,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    VkImageView dsvview = createImageView(vkh::ImageViewCreateInfo(
        depthimg.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT_S8_UINT, {},
        vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)));

    // create renderpass using the DS image
    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        mainWindow->format, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));
    renderPassCreateInfo.attachments.push_back(vkh::AttachmentDescription(
        VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})}, 1,
                                    VK_IMAGE_LAYOUT_GENERAL);

    VkRenderPass renderPass = createRenderPass(renderPassCreateInfo);

    // create framebuffers using swapchain images and DS image
    std::vector<VkFramebuffer> fbs;
    fbs.resize(mainWindow->GetCount());

    for(size_t i = 0; i < mainWindow->GetCount(); i++)
      fbs[i] = createFramebuffer(vkh::FramebufferCreateInfo(
          renderPass, {mainWindow->GetView(i), dsvview}, mainWindow->scissor.extent));

    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {}, {
                vkh::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       sizeof(Vec4f) * 2),
            }));

    VkPipelineLayout layout2 = createPipelineLayout(vkh::PipelineLayoutCreateInfo(
        {}, {
                vkh::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Vec4f) * 2),
            }));

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = renderPass;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(pixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    pipeCreateInfo.depthStencilState.depthTestEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.depthWriteEnable = VK_TRUE;
    pipeCreateInfo.depthStencilState.stencilTestEnable = VK_FALSE;
    pipeCreateInfo.depthStencilState.back = pipeCreateInfo.depthStencilState.front;

    VkSpecializationMapEntry specmap[1] = {
        {1, 0 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    uint32_t specvals[1] = {1337};

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = ARRAY_COUNT(specmap);
    spec.pMapEntries = specmap;
    spec.dataSize = sizeof(specvals);
    spec.pData = specvals;

    pipeCreateInfo.stages[0].pSpecializationInfo = &spec;

    VkPipeline pipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.stages[0] = CompileShaderModule(vertex, ShaderLang::glsl, ShaderStage::vert,
                                                   "main", {std::make_pair("USE_POINTS", "1")}),
    pipeCreateInfo.stages[0].pSpecializationInfo = &spec;
    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipeline pointspipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipeline linespipe = createGraphicsPipeline(pipeCreateInfo);

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {{0, 0, VK_VERTEX_INPUT_RATE_VERTEX}};

    pipeCreateInfo.layout = layout2;

    VkPipeline stride0pipe = createGraphicsPipeline(pipeCreateInfo);

    VkPipeline xfbpipe = VK_NULL_HANDLE;

    VkPipelineRasterizationStateStreamCreateInfoEXT rastInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,
        NULL,
        0,
        2,
    };

    VkBufferUsageFlags xfbUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if(useXfbStreamPipe)
    {
      pipeCreateInfo.inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
      pipeCreateInfo.rasterizationState.pNext = &rastInfo;

      pipeCreateInfo.stages = {
          CompileShaderModule(xfbvert, ShaderLang::glsl, ShaderStage::vert),
          CompileShaderModule(xfbgeom, ShaderLang::glsl, ShaderStage::geom),
          CompileShaderModule(xfbpixel, ShaderLang::glsl, ShaderStage::frag),
      };

      xfbpipe = createGraphicsPipeline(pipeCreateInfo);

      xfbUsage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
      xfbUsage |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
    }

    AllocatedBuffer xfbBuf(this, vkh::BufferCreateInfo(4096, xfbUsage),
                           VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY}));

    AllocatedBuffer vb(this,
                       vkh::BufferCreateInfo(sizeof(test), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                       VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    vb.upload(test);

    Vec4f cbufferdata[] = {
        Vec4f(2.0f / (float)screenWidth, 2.0f / (float)screenHeight, 1.0f, 1.0f),
        Vec4f(-1.0f, -1.0f, 1.0f, 1.0f),
    };

    AllocatedBuffer cb(
        this,
        vkh::BufferCreateInfo(sizeof(cbufferdata), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    cb.upload(cbufferdata);

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      VkImage swapimg =
          StartUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkCmdClearColorImage(cmd, swapimg, VK_IMAGE_LAYOUT_GENERAL,
                           vkh::ClearColorValue(0.2f, 0.2f, 0.2f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkCmdBeginRenderPass(
          cmd,
          vkh::RenderPassBeginInfo(renderPass, fbs[mainWindow->imgIndex], mainWindow->scissor,
                                   {{}, vkh::ClearValue(1.0f, 0)}),
          VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(cbufferdata), &cbufferdata);
      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdSetScissor(cmd, 0, 1, &mainWindow->scissor);
      vkh::cmdBindVertexBuffers(cmd, 0, {vb.buffer}, {0});

      vkCmdDraw(cmd, 3, 1, 10, 0);

      // if we have KHR_maintenance1, invert the viewport for the quad draw
      if(KHR_maintenance1)
      {
        VkViewport v = mainWindow->viewport;
        v.y += v.height;
        v.height = -v.height;
        vkCmdSetViewport(cmd, 0, 1, &v);
        cbufferdata[1].w = -1.0f;
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(cbufferdata), &cbufferdata);
      }

      setMarker(cmd, "Quad");
      vkCmdDraw(cmd, 6, 2, 0, 0);

      cbufferdata[1].w = 1.0f;

      vkCmdSetViewport(cmd, 0, 1, &mainWindow->viewport);
      vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(cbufferdata), &cbufferdata);

      setMarker(cmd, "Points");

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointspipe);

      vkCmdDraw(cmd, 4, 1, 6, 0);

      setMarker(cmd, "Lines");

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, linespipe);

      vkCmdDraw(cmd, 4, 1, 6, 0);

      setMarker(cmd, "Stride 0");

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stride0pipe);
      vkCmdPushConstants(cmd, layout2, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cbufferdata),
                         &cbufferdata);

      vkCmdDraw(cmd, 1, 1, 0, 0);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
      setMarker(cmd, "Empty");
      vkCmdDraw(cmd, 0, 0, 0, 0);

      if(xfbpipe != VK_NULL_HANDLE)
      {
        VkBuffer bufs[3] = {
            xfbBuf.buffer,
            xfbBuf.buffer,
            xfbBuf.buffer,
        };
        VkDeviceSize offs[3] = {
            0,
            1024,
            2048,
        };

        vkCmdBindTransformFeedbackBuffersEXT(cmd, 0, 3, bufs, offs, NULL);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, xfbpipe);
        setMarker(cmd, "XFB");

        vkCmdBeginTransformFeedbackEXT(cmd, 0, 3, NULL, NULL);
        vkCmdDraw(cmd, 1, 1, 0, 0);
        vkCmdEndTransformFeedbackEXT(cmd, 0, 3, NULL, NULL);

        setMarker(cmd, "XFB After");
      }

      vkCmdEndRenderPass(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
