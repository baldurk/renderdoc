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

#include <float.h>
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

MeshDisplayPipelines VulkanDebugManager::CacheMeshDisplayPipelines(VkPipelineLayout pipeLayout,
                                                                   const MeshFormat &primary,
                                                                   const MeshFormat &secondary)
{
  // generate a key to look up the map
  uint64_t key = 0;

  uint64_t bit = 0;

  if(primary.indexByteStride == 4)
    key |= 1ULL << bit;
  bit++;

  RDCASSERT((uint32_t)primary.topology < 64);
  key |= uint64_t((uint32_t)primary.topology & 0x3f) << bit;
  bit += 6;

  VkFormat primaryFmt = MakeVkFormat(primary.format);
  VkFormat secondaryFmt = secondary.vertexResourceId == ResourceId()
                              ? VK_FORMAT_UNDEFINED
                              : MakeVkFormat(secondary.format);

  RDCCOMPILE_ASSERT(VK_FORMAT_RANGE_SIZE <= 255,
                    "Mesh pipeline cache key needs an extra bit for format");

  key |= uint64_t((uint32_t)primaryFmt & 0xff) << bit;
  bit += 8;

  key |= uint64_t((uint32_t)secondaryFmt & 0xff) << bit;
  bit += 8;

  RDCASSERT(primary.vertexByteStride <= 0xffff);
  key |= uint64_t((uint32_t)primary.vertexByteStride & 0xffff) << bit;
  bit += 16;

  if(secondary.vertexResourceId != ResourceId())
  {
    RDCASSERT(secondary.vertexByteStride <= 0xffff);
    key |= uint64_t((uint32_t)secondary.vertexByteStride & 0xffff) << bit;
  }
  bit += 16;

  if(primary.instanced)
    key |= 1ULL << bit;
  bit++;

  if(secondary.instanced)
    key |= 1ULL << bit;
  bit++;

  // only 64 bits, make sure they all fit
  RDCASSERT(bit < 64);

  MeshDisplayPipelines &cache = m_CachedMeshPipelines[key];

  if(cache.pipes[(uint32_t)SolidShade::NoSolid] != VK_NULL_HANDLE)
    return cache;

  const VkLayerDispatchTable *vt = ObjDisp(m_Device);
  VkResult vkr = VK_SUCCESS;

  // should we try and evict old pipelines from the cache here?
  // or just keep them forever

  VkVertexInputBindingDescription binds[] = {
      // primary
      {0, primary.vertexByteStride,
       primary.instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX},
      // secondary
      {1, secondary.vertexByteStride,
       secondary.instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX}};

  RDCASSERT(primaryFmt != VK_FORMAT_UNDEFINED);

  VkVertexInputAttributeDescription vertAttrs[] = {
      // primary
      {
          0, 0, primaryFmt, 0,
      },
      // secondary
      {
          1, 0, primaryFmt, 0,
      },
  };

  VkPipelineVertexInputStateCreateInfo vi = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0, 1, binds, 2, vertAttrs,
  };

  VkPipelineShaderStageCreateInfo stages[3] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_ALL_GRAPHICS,
       VK_NULL_HANDLE, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_ALL_GRAPHICS,
       VK_NULL_HANDLE, "main", NULL},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_ALL_GRAPHICS,
       VK_NULL_HANDLE, "main", NULL},
  };

  VkPipelineInputAssemblyStateCreateInfo ia = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
      primary.topology >= Topology::PatchList ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                                              : MakeVkPrimitiveTopology(primary.topology),
      false,
  };

  if(IsStrip(primary.topology))
  {
    ia.primitiveRestartEnable = true;
  }

  VkRect2D scissor = {{0, 0}, {16384, 16384}};

  VkPipelineViewportStateCreateInfo vp = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, NULL, 1, &scissor};

  VkPipelineRasterizationStateCreateInfo rs = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      false,
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_NONE,
      VK_FRONT_FACE_CLOCKWISE,
      false,
      0.0f,
      0.0f,
      0.0f,
      1.0f,
  };

  VkPipelineMultisampleStateCreateInfo msaa = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      NULL,
      0,
      VULKAN_MESH_VIEW_SAMPLES,
      false,
      0.0f,
      NULL,
      false,
      false};

  VkPipelineDepthStencilStateCreateInfo ds = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      NULL,
      0,
      true,
      true,
      VK_COMPARE_OP_LESS_OR_EQUAL,
      false,
      false,
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      {VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0},
      0.0f,
      1.0f,
  };

  VkPipelineColorBlendAttachmentState attState = {
      false,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_ZERO,
      VK_BLEND_OP_ADD,
      0xf,
  };

  VkPipelineColorBlendStateCreateInfo cb = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      NULL,
      0,
      false,
      VK_LOGIC_OP_NO_OP,
      1,
      &attState,
      {1.0f, 1.0f, 1.0f, 1.0f}};

  VkDynamicState dynstates[] = {VK_DYNAMIC_STATE_VIEWPORT};

  VkPipelineDynamicStateCreateInfo dyn = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      NULL,
      0,
      ARRAY_COUNT(dynstates),
      dynstates,
  };

  VkRenderPass rp;    // compatible render pass

  {
    VkAttachmentDescription attDesc[] = {
        {0, VK_FORMAT_R8G8B8A8_SRGB, VULKAN_MESH_VIEW_SAMPLES, VK_ATTACHMENT_LOAD_OP_LOAD,
         VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
         VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {0, VK_FORMAT_D32_SFLOAT, VULKAN_MESH_VIEW_SAMPLES, VK_ATTACHMENT_LOAD_OP_LOAD,
         VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
         VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };

    VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dsRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub = {
        0,      VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,      NULL,       // inputs
        1,      &attRef,    // color
        NULL,               // resolve
        &dsRef,             // depth-stencil
        0,      NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        2,
        attDesc,
        1,
        &sub,
        0,
        NULL,    // dependencies
    };

    vt->CreateRenderPass(Unwrap(m_Device), &rpinfo, NULL, &rp);
  }

  VkGraphicsPipelineCreateInfo pipeInfo = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      NULL,
      0,
      2,
      stages,
      &vi,
      &ia,
      NULL,    // tess
      &vp,
      &rs,
      &msaa,
      &ds,
      &cb,
      &dyn,
      Unwrap(pipeLayout),
      rp,
      0,                 // sub pass
      VK_NULL_HANDLE,    // base pipeline handle
      0,                 // base pipeline index
  };

  // wireframe pipeline
  stages[0].module = Unwrap(m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::MeshVS));
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[1].module = Unwrap(m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::MeshFS));
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  rs.polygonMode = VK_POLYGON_MODE_LINE;
  rs.lineWidth = 1.0f;
  ds.depthTestEnable = false;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_Wire]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ds.depthTestEnable = true;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // solid shading pipeline
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  ds.depthTestEnable = false;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_Solid]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  ds.depthTestEnable = true;

  vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                    &cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(secondary.vertexResourceId != ResourceId())
  {
    // pull secondary information from second vertex buffer
    vertAttrs[1].binding = 1;
    vertAttrs[1].format = secondaryFmt;
    RDCASSERT(secondaryFmt != VK_FORMAT_UNDEFINED);

    vi.vertexBindingDescriptionCount = 2;

    vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                      &cache.pipes[MeshDisplayPipelines::ePipe_Secondary]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  vertAttrs[1].binding = 0;
  vi.vertexBindingDescriptionCount = 1;

  // flat lit pipeline, needs geometry shader to calculate face normals
  stages[2].module = Unwrap(m_pDriver->GetShaderCache()->GetBuiltinModule(BuiltinShader::MeshGS));
  stages[2].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
  pipeInfo.stageCount = 3;

  if(stages[2].module != VK_NULL_HANDLE)
  {
    vkr = vt->CreateGraphicsPipelines(Unwrap(m_Device), VK_NULL_HANDLE, 1, &pipeInfo, NULL,
                                      &cache.pipes[MeshDisplayPipelines::ePipe_Lit]);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }

  for(uint32_t i = 0; i < MeshDisplayPipelines::ePipe_Count; i++)
    if(cache.pipes[i] != VK_NULL_HANDLE)
      m_pDriver->GetResourceManager()->WrapResource(Unwrap(m_Device), cache.pipes[i]);

  vt->DestroyRenderPass(Unwrap(m_Device), rp, NULL);

  return cache;
}

void VulkanReplay::RenderMesh(uint32_t eventId, const std::vector<MeshFormat> &secondaryDraws,
                              const MeshDisplay &cfg)
{
  if(cfg.position.vertexResourceId == ResourceId() || cfg.position.numIndices == 0)
    return;

  auto it = m_OutputWindows.find(m_ActiveWinID);
  if(m_ActiveWinID == 0 || it == m_OutputWindows.end())
    return;

  OutputWindow &outw = it->second;

  // if the swapchain failed to create, do nothing. We will try to recreate it
  // again in CheckResizeOutputWindow (once per render 'frame')
  if(outw.m_WindowSystem != WindowingSystem::Headless && outw.swap == VK_NULL_HANDLE)
    return;

  VkDevice dev = m_pDriver->GetDev();
  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  const VkLayerDispatchTable *vt = ObjDisp(dev);

  VkResult vkr = VK_SUCCESS;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMarkerRegion::Begin(
      StringFormat::Fmt("RenderMesh with %zu secondary draws", secondaryDraws.size()), cmd);

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(outw.rpdepth),
      Unwrap(outw.fbdepth),
      {{
           0, 0,
       },
       {m_DebugWidth, m_DebugHeight}},
      0,
      NULL,
  };
  vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f};
  vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

  Matrix4f projMat =
      Matrix4f::Perspective(90.0f, 0.1f, 100000.0f, float(m_DebugWidth) / float(m_DebugHeight));
  Matrix4f InvProj = projMat.Inverse();

  Matrix4f camMat = cfg.cam ? ((Camera *)cfg.cam)->GetMatrix() : Matrix4f::Identity();

  Matrix4f ModelViewProj = projMat.Mul(camMat);
  Matrix4f guessProjInv;

  if(cfg.position.unproject)
  {
    // the derivation of the projection matrix might not be right (hell, it could be an
    // orthographic projection). But it'll be close enough likely.
    Matrix4f guessProj =
        cfg.position.farPlane != FLT_MAX
            ? Matrix4f::Perspective(cfg.fov, cfg.position.nearPlane, cfg.position.farPlane, cfg.aspect)
            : Matrix4f::ReversePerspective(cfg.fov, cfg.position.nearPlane, cfg.aspect);

    if(cfg.ortho)
    {
      guessProj = Matrix4f::Orthographic(cfg.position.nearPlane, cfg.position.farPlane);
    }

    guessProjInv = guessProj.Inverse();

    ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
  }

  if(!secondaryDraws.empty())
  {
    size_t mapsUsed = 0;

    for(size_t i = 0; i < secondaryDraws.size(); i++)
    {
      const MeshFormat &fmt = secondaryDraws[i];

      if(fmt.vertexResourceId != ResourceId())
      {
        // TODO should move the color to a push constant so we don't have to map all the time
        uint32_t uboOffs = 0;
        MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

        data->mvp = ModelViewProj;
        data->color = Vec4f(fmt.meshColor.x, fmt.meshColor.y, fmt.meshColor.z, fmt.meshColor.w);
        data->homogenousInput = cfg.position.unproject;
        data->pointSpriteSize = Vec2f(0.0f, 0.0f);
        data->displayFormat = MESHDISPLAY_SOLID;
        data->rawoutput = 0;

        m_MeshRender.UBO.Unmap();

        mapsUsed++;

        if(mapsUsed + 1 >= m_MeshRender.UBO.GetRingCount())
        {
          // flush and sync so we can use more maps
          vt->CmdEndRenderPass(Unwrap(cmd));

          vkr = vt->EndCommandBuffer(Unwrap(cmd));
          RDCASSERTEQUAL(vkr, VK_SUCCESS);

          m_pDriver->SubmitCmds();
          m_pDriver->FlushQ();

          mapsUsed = 0;

          cmd = m_pDriver->GetNextCmd();

          vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
          RDCASSERTEQUAL(vkr, VK_SUCCESS);
          vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

          vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
        }

        MeshDisplayPipelines secondaryCache = GetDebugManager()->CacheMeshDisplayPipelines(
            m_MeshRender.PipeLayout, secondaryDraws[i], secondaryDraws[i]);

        vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                  UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

        vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Unwrap(secondaryCache.pipes[MeshDisplayPipelines::ePipe_WireDepth]));

        VkBuffer vb =
            m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(fmt.vertexResourceId);

        VkDeviceSize offs = fmt.vertexByteOffset;
        vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);

        if(fmt.indexByteStride)
        {
          VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
          if(fmt.indexByteStride == 4)
            idxtype = VK_INDEX_TYPE_UINT32;

          if(fmt.indexResourceId != ResourceId())
          {
            VkBuffer ib =
                m_pDriver->GetResourceManager()->GetLiveHandle<VkBuffer>(fmt.indexResourceId);

            vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), fmt.indexByteOffset, idxtype);
          }
          vt->CmdDrawIndexed(Unwrap(cmd), fmt.numIndices, 1, 0, fmt.baseVertex, 0);
        }
        else
        {
          vt->CmdDraw(Unwrap(cmd), fmt.numIndices, 1, 0, 0);
        }
      }
    }

    {
      // flush and sync so we can use more maps
      vt->CmdEndRenderPass(Unwrap(cmd));

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
      vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
    }
  }

  MeshDisplayPipelines cache = GetDebugManager()->CacheMeshDisplayPipelines(
      m_MeshRender.PipeLayout, cfg.position, cfg.second);

  if(cfg.position.vertexResourceId != ResourceId())
  {
    VkBuffer vb =
        m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.vertexResourceId);

    VkDeviceSize offs = cfg.position.vertexByteOffset;

    // we source all data from the first instanced value in the instanced case, so make sure we
    // offset correctly here.
    if(cfg.position.instanced)
      offs += cfg.position.vertexByteStride * (cfg.curInstance / cfg.position.instStepRate);

    vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(vb), &offs);
  }

  SolidShade solidShadeMode = cfg.solidShadeMode;

  // can't support secondary shading without a buffer - no pipeline will have been created
  if(solidShadeMode == SolidShade::Secondary && cfg.second.vertexResourceId == ResourceId())
    solidShadeMode = SolidShade::NoSolid;

  if(solidShadeMode == SolidShade::Secondary)
  {
    VkBuffer vb =
        m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.second.vertexResourceId);

    VkDeviceSize offs = cfg.second.vertexByteOffset;

    // we source all data from the first instanced value in the instanced case, so make sure we
    // offset correctly here.
    if(cfg.second.instanced)
      offs += cfg.second.vertexByteStride * (cfg.curInstance / cfg.second.instStepRate);

    vt->CmdBindVertexBuffers(Unwrap(cmd), 1, 1, UnwrapPtr(vb), &offs);
  }

  // solid render
  if(solidShadeMode != SolidShade::NoSolid && cfg.position.topology < Topology::PatchList)
  {
    VkPipeline pipe = VK_NULL_HANDLE;
    switch(solidShadeMode)
    {
      default:
      case SolidShade::Solid: pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth]; break;
      case SolidShade::Lit: pipe = cache.pipes[MeshDisplayPipelines::ePipe_Lit]; break;
      case SolidShade::Secondary: pipe = cache.pipes[MeshDisplayPipelines::ePipe_Secondary]; break;
    }

    // can't support lit rendering without the pipeline - maybe geometry shader wasn't supported.
    if(solidShadeMode == SolidShade::Lit && pipe == VK_NULL_HANDLE)
      pipe = cache.pipes[MeshDisplayPipelines::ePipe_SolidDepth];

    uint32_t uboOffs = 0;
    MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    if(solidShadeMode == SolidShade::Lit)
      data->invProj = projMat.Inverse();

    data->mvp = ModelViewProj;
    data->color = Vec4f(0.8f, 0.8f, 0.0f, 1.0f);
    data->homogenousInput = cfg.position.unproject;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->displayFormat = (uint32_t)solidShadeMode;
    data->rawoutput = 0;

    if(solidShadeMode == SolidShade::Secondary && cfg.second.showAlpha)
      data->displayFormat = MESHDISPLAY_SECONDARY_ALPHA;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(pipe));

    if(cfg.position.indexByteStride)
    {
      VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
      if(cfg.position.indexByteStride == 4)
        idxtype = VK_INDEX_TYPE_UINT32;

      if(cfg.position.indexResourceId != ResourceId())
      {
        VkBuffer ib =
            m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.indexResourceId);

        vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), cfg.position.indexByteOffset, idxtype);
      }
      vt->CmdDrawIndexed(Unwrap(cmd), cfg.position.numIndices, 1, 0, cfg.position.baseVertex, 0);
    }
    else
    {
      vt->CmdDraw(Unwrap(cmd), cfg.position.numIndices, 1, 0, 0);
    }
  }

  // wireframe render
  if(solidShadeMode == SolidShade::NoSolid || cfg.wireframeDraw ||
     cfg.position.topology >= Topology::PatchList)
  {
    Vec4f wireCol =
        Vec4f(cfg.position.meshColor.x, cfg.position.meshColor.y, cfg.position.meshColor.z, 1.0f);

    uint32_t uboOffs = 0;
    MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    data->mvp = ModelViewProj;
    data->color = wireCol;
    data->displayFormat = (uint32_t)SolidShade::Solid;
    data->homogenousInput = cfg.position.unproject;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->rawoutput = 0;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]));

    if(cfg.position.indexByteStride)
    {
      VkIndexType idxtype = VK_INDEX_TYPE_UINT16;
      if(cfg.position.indexByteStride == 4)
        idxtype = VK_INDEX_TYPE_UINT32;

      if(cfg.position.indexResourceId != ResourceId())
      {
        VkBuffer ib =
            m_pDriver->GetResourceManager()->GetCurrentHandle<VkBuffer>(cfg.position.indexResourceId);

        vt->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(ib), cfg.position.indexByteOffset, idxtype);
      }
      vt->CmdDrawIndexed(Unwrap(cmd), cfg.position.numIndices, 1, 0, cfg.position.baseVertex, 0);
    }
    else
    {
      vt->CmdDraw(Unwrap(cmd), cfg.position.numIndices, 1, 0, 0);
    }
  }

  MeshFormat helper;
  helper.indexByteStride = 2;
  helper.topology = Topology::LineList;

  helper.format.type = ResourceFormatType::Regular;
  helper.format.compByteWidth = 4;
  helper.format.compCount = 4;
  helper.format.compType = CompType::Float;

  helper.vertexByteStride = sizeof(Vec4f);

  // cache pipelines for use in drawing wireframe helpers
  cache = GetDebugManager()->CacheMeshDisplayPipelines(m_MeshRender.PipeLayout, helper, helper);

  if(cfg.showBBox)
  {
    Vec4f a = Vec4f(cfg.minBounds.x, cfg.minBounds.y, cfg.minBounds.z, cfg.minBounds.w);
    Vec4f b = Vec4f(cfg.maxBounds.x, cfg.maxBounds.y, cfg.maxBounds.z, cfg.maxBounds.w);

    Vec4f TLN = Vec4f(a.x, b.y, a.z, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(b.x, b.y, a.z, 1.0f);
    Vec4f BLN = Vec4f(a.x, a.y, a.z, 1.0f);
    Vec4f BRN = Vec4f(b.x, a.y, a.z, 1.0f);

    Vec4f TLF = Vec4f(a.x, b.y, b.z, 1.0f);
    Vec4f TRF = Vec4f(b.x, b.y, b.z, 1.0f);
    Vec4f BLF = Vec4f(a.x, a.y, b.z, 1.0f);
    Vec4f BRF = Vec4f(b.x, a.y, b.z, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f bbox[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    VkDeviceSize vboffs = 0;
    Vec4f *ptr = (Vec4f *)m_MeshRender.BBoxVB.Map(vboffs);

    memcpy(ptr, bbox, sizeof(bbox));

    m_MeshRender.BBoxVB.Unmap();

    vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.BBoxVB.buf), &vboffs);

    uint32_t uboOffs = 0;
    MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    data->mvp = ModelViewProj;
    data->color = Vec4f(0.2f, 0.2f, 1.0f, 1.0f);
    data->displayFormat = (uint32_t)SolidShade::Solid;
    data->homogenousInput = 0;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->rawoutput = 0;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_WireDepth]));

    vt->CmdDraw(Unwrap(cmd), 24, 1, 0, 0);
  }

  // draw axis helpers
  if(!cfg.position.unproject)
  {
    VkDeviceSize vboffs = 0;
    vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.AxisFrustumVB.buf), &vboffs);

    uint32_t uboOffs = 0;
    MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    data->mvp = ModelViewProj;
    data->color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
    data->displayFormat = (uint32_t)SolidShade::Solid;
    data->homogenousInput = 0;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->rawoutput = 0;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Wire]));

    vt->CmdDraw(Unwrap(cmd), 2, 1, 0, 0);

    // poke the color (this would be a good candidate for a push constant)
    data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    data->mvp = ModelViewProj;
    data->color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
    data->displayFormat = (uint32_t)SolidShade::Solid;
    data->homogenousInput = 0;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->rawoutput = 0;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);
    vt->CmdDraw(Unwrap(cmd), 2, 1, 2, 0);

    data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    data->mvp = ModelViewProj;
    data->color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
    data->displayFormat = (uint32_t)SolidShade::Solid;
    data->homogenousInput = 0;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->rawoutput = 0;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);
    vt->CmdDraw(Unwrap(cmd), 2, 1, 4, 0);
  }

  // 'fake' helper frustum
  if(cfg.position.unproject)
  {
    VkDeviceSize vboffs = sizeof(Vec4f) * 6;    // skim the axis helpers
    vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.AxisFrustumVB.buf), &vboffs);

    uint32_t uboOffs = 0;
    MeshUBOData *data = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);

    data->mvp = ModelViewProj;
    data->color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    data->displayFormat = (uint32_t)SolidShade::Solid;
    data->homogenousInput = 0;
    data->pointSpriteSize = Vec2f(0.0f, 0.0f);
    data->rawoutput = 0;

    m_MeshRender.UBO.Unmap();

    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_MeshRender.PipeLayout), 0, 1,
                              UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Wire]));

    vt->CmdDraw(Unwrap(cmd), 24, 1, 0, 0);
  }

  // show highlighted vertex
  if(cfg.highlightVert != ~0U)
  {
    {
      // need to end our cmd buffer, it might be submitted in GetBufferData when caching highlight
      // data
      vt->CmdEndRenderPass(Unwrap(cmd));

      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      m_pDriver->SubmitCmds();
#endif
    }

    m_HighlightCache.CacheHighlightingData(eventId, cfg);

    {
      // get a new cmdbuffer and begin it
      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
      vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

      vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);
    }

    Topology meshtopo = cfg.position.topology;

    ///////////////////////////////////////////////////////////////
    // vectors to be set from buffers, depending on topology

    // this vert (blue dot, required)
    FloatVector activeVertex;

    // primitive this vert is a part of (red prim, optional)
    std::vector<FloatVector> activePrim;

    // for patch lists, to show other verts in patch (green dots, optional)
    // for non-patch lists, we use the activePrim and adjacentPrimVertices
    // to show what other verts are related
    std::vector<FloatVector> inactiveVertices;

    // adjacency (line or tri, strips or lists) (green prims, optional)
    // will be N*M long, N adjacent prims of M verts each. M = primSize below
    std::vector<FloatVector> adjacentPrimVertices;

    helper.topology = Topology::TriangleList;
    uint32_t primSize = 3;    // number of verts per primitive

    if(meshtopo == Topology::LineList || meshtopo == Topology::LineStrip ||
       meshtopo == Topology::LineList_Adj || meshtopo == Topology::LineStrip_Adj)
    {
      primSize = 2;
      helper.topology = Topology::LineList;
    }
    else
    {
      // update the cache, as it's currently linelist
      helper.topology = Topology::TriangleList;
      cache = GetDebugManager()->CacheMeshDisplayPipelines(m_MeshRender.PipeLayout, helper, helper);
    }

    bool valid = m_HighlightCache.FetchHighlightPositions(cfg, activeVertex, activePrim,
                                                          adjacentPrimVertices, inactiveVertices);

    if(valid)
    {
      ////////////////////////////////////////////////////////////////
      // prepare rendering (for both vertices & primitives)

      // if data is from post transform, it will be in clipspace
      if(cfg.position.unproject)
        ModelViewProj = projMat.Mul(camMat.Mul(guessProjInv));
      else
        ModelViewProj = projMat.Mul(camMat);

      MeshUBOData uniforms = {};
      uniforms.mvp = ModelViewProj;
      uniforms.color = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
      uniforms.displayFormat = (uint32_t)SolidShade::Solid;
      uniforms.homogenousInput = cfg.position.unproject;
      uniforms.pointSpriteSize = Vec2f(0.0f, 0.0f);

      uint32_t uboOffs = 0;
      MeshUBOData *ubodata = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);
      *ubodata = uniforms;
      m_MeshRender.UBO.Unmap();

      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

      vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Solid]));

      ////////////////////////////////////////////////////////////////
      // render primitives

      // Draw active primitive (red)
      uniforms.color = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
      // poke the color (this would be a good candidate for a push constant)
      ubodata = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);
      *ubodata = uniforms;
      m_MeshRender.UBO.Unmap();
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

      if(activePrim.size() >= primSize)
      {
        VkDeviceSize vboffs = 0;
        Vec4f *ptr = (Vec4f *)m_MeshRender.BBoxVB.Map(vboffs, sizeof(Vec4f) * primSize);

        memcpy(ptr, &activePrim[0], sizeof(Vec4f) * primSize);

        m_MeshRender.BBoxVB.Unmap();

        vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.BBoxVB.buf), &vboffs);

        vt->CmdDraw(Unwrap(cmd), primSize, 1, 0, 0);
      }

      // Draw adjacent primitives (green)
      uniforms.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      // poke the color (this would be a good candidate for a push constant)
      ubodata = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);
      *ubodata = uniforms;
      m_MeshRender.UBO.Unmap();
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

      if(adjacentPrimVertices.size() >= primSize && (adjacentPrimVertices.size() % primSize) == 0)
      {
        VkDeviceSize vboffs = 0;
        Vec4f *ptr =
            (Vec4f *)m_MeshRender.BBoxVB.Map(vboffs, sizeof(Vec4f) * adjacentPrimVertices.size());

        memcpy(ptr, &adjacentPrimVertices[0], sizeof(Vec4f) * adjacentPrimVertices.size());

        m_MeshRender.BBoxVB.Unmap();

        vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.BBoxVB.buf), &vboffs);

        vt->CmdDraw(Unwrap(cmd), (uint32_t)adjacentPrimVertices.size(), 1, 0, 0);
      }

      ////////////////////////////////////////////////////////////////
      // prepare to render dots
      float scale = 800.0f / float(m_DebugHeight);
      float asp = float(m_DebugWidth) / float(m_DebugHeight);

      uniforms.pointSpriteSize = Vec2f(scale / asp, scale);

      // Draw active vertex (blue)
      uniforms.color = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      // poke the color (this would be a good candidate for a push constant)
      ubodata = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);
      *ubodata = uniforms;
      m_MeshRender.UBO.Unmap();
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

      // vertices are drawn with tri strips
      helper.topology = Topology::TriangleStrip;
      cache = GetDebugManager()->CacheMeshDisplayPipelines(m_MeshRender.PipeLayout, helper, helper);

      FloatVector vertSprite[4] = {
          activeVertex, activeVertex, activeVertex, activeVertex,
      };

      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

      vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          Unwrap(cache.pipes[MeshDisplayPipelines::ePipe_Solid]));

      {
        VkDeviceSize vboffs = 0;
        Vec4f *ptr = (Vec4f *)m_MeshRender.BBoxVB.Map(vboffs, sizeof(vertSprite));

        memcpy(ptr, &vertSprite[0], sizeof(vertSprite));

        m_MeshRender.BBoxVB.Unmap();

        vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.BBoxVB.buf), &vboffs);

        vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);
      }

      // Draw inactive vertices (green)
      uniforms.color = Vec4f(0.0f, 1.0f, 0.0f, 1.0f);
      // poke the color (this would be a good candidate for a push constant)
      ubodata = (MeshUBOData *)m_MeshRender.UBO.Map(&uboOffs);
      *ubodata = uniforms;
      m_MeshRender.UBO.Unmap();
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_MeshRender.PipeLayout), 0, 1,
                                UnwrapPtr(m_MeshRender.DescSet), 1, &uboOffs);

      if(!inactiveVertices.empty())
      {
        VkDeviceSize vboffs = 0;
        FloatVector *ptr = (FloatVector *)m_MeshRender.BBoxVB.Map(vboffs, sizeof(vertSprite));

        for(size_t i = 0; i < inactiveVertices.size(); i++)
        {
          *ptr++ = inactiveVertices[i];
          *ptr++ = inactiveVertices[i];
          *ptr++ = inactiveVertices[i];
          *ptr++ = inactiveVertices[i];
        }

        m_MeshRender.BBoxVB.Unmap();

        for(size_t i = 0; i < inactiveVertices.size(); i++)
        {
          vt->CmdBindVertexBuffers(Unwrap(cmd), 0, 1, UnwrapPtr(m_MeshRender.BBoxVB.buf), &vboffs);

          vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

          vboffs += sizeof(FloatVector) * 4;
        }
      }
    }
  }

  vt->CmdEndRenderPass(Unwrap(cmd));

  VkMarkerRegion::End(cmd);

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif
}
