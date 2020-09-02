/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "vk_replay.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <algorithm>
#include "core/settings.h"
#include "driver/ihv/amd/amd_rgp.h"
#include "driver/shaders/spirv/spirv_compile.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_resources.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

static const char *SPIRVDisassemblyTarget = "SPIR-V (RenderDoc)";
static const char *AMDShaderInfoTarget = "AMD_shader_info";
static const char *KHRExecutablePropertiesTarget = "KHR_pipeline_executable_properties";

VulkanReplay::VulkanReplay(WrappedVulkan *d)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(VulkanReplay));

  m_pDriver = d;
  m_Proxy = false;

  m_HighlightCache.driver = this;

  m_OutputWinID = 1;
  m_ActiveWinID = 0;
  m_BindDepth = false;

  m_DebugWidth = m_DebugHeight = 1;

  RDCEraseEl(m_DriverInfo);
}

VulkanDebugManager *VulkanReplay::GetDebugManager()
{
  return m_pDriver->GetDebugManager();
}

VulkanResourceManager *VulkanReplay::GetResourceManager()
{
  return m_pDriver->GetResourceManager();
}

void VulkanReplay::Shutdown()
{
  SAFE_DELETE(m_RGP);

  m_pDriver->Shutdown();
  delete m_pDriver;
}

rdcarray<GPUDevice> VulkanReplay::GetAvailableGPUs()
{
  rdcarray<GPUDevice> ret;

  // do a manual enumerate to avoid any possible remapping
  VkInstance instance = m_pDriver->GetInstance();

  uint32_t count = 0;
  VkResult vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, NULL);

  if(vkr != VK_SUCCESS)
    return ret;

  VkPhysicalDevice *devices = new VkPhysicalDevice[count];

  vkr = ObjDisp(instance)->EnumeratePhysicalDevices(Unwrap(instance), &count, devices);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t p = 0; p < count; p++)
  {
    VkPhysicalDeviceProperties props = {};
    ObjDisp(instance)->GetPhysicalDeviceProperties(devices[p], &props);

    VkPhysicalDeviceDriverProperties driverProps = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
    };

    VkPhysicalDeviceProperties2 physProps2 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driverProps,
    };

    // get driver properties if available
    if(m_pDriver->GetExtensions(GetRecord(instance)).ext_KHR_get_physical_device_properties2)
    {
      uint32_t extCount = 0;
      ObjDisp(instance)->EnumerateDeviceExtensionProperties(devices[p], NULL, &extCount, NULL);

      VkExtensionProperties *extProps = new VkExtensionProperties[extCount];
      ObjDisp(instance)->EnumerateDeviceExtensionProperties(devices[p], NULL, &extCount, extProps);

      for(uint32_t e = 0; e < extCount; e++)
      {
        if(!strcmp(extProps[e].extensionName, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
        {
          ObjDisp(instance)->GetPhysicalDeviceProperties2(devices[p], &physProps2);
          break;
        }
      }

      SAFE_DELETE_ARRAY(extProps);
    }

    GPUDevice dev;
    dev.vendor = GPUVendorFromPCIVendor(props.vendorID);
    dev.deviceID = props.deviceID;
    dev.name = props.deviceName;
    dev.apis = {GraphicsAPI::Vulkan};

    // only set the driver name when it's useful to disambiguate
    dev.driver = HumanDriverName(driverProps.driverID);

    // don't add duplicate devices even if they get enumerated.
    if(ret.indexOf(dev) == -1)
      ret.push_back(dev);
  }

  // loop over devices and remove the driver string unless it's needed to disambiguate from another
  // identical device.
  for(size_t i = 0; i < ret.size(); i++)
  {
    bool needDriver = false;

    for(size_t j = 0; j < ret.size(); j++)
    {
      if(i == j)
        continue;

      if(ret[i].vendor == ret[j].vendor && ret[i].deviceID == ret[j].deviceID)
      {
        RDCASSERT(ret[i].driver != ret[j].driver);
        needDriver = true;
        break;
      }
    }

    if(!needDriver)
      ret[i].driver = rdcstr();
  }

  SAFE_DELETE_ARRAY(devices);

  return ret;
}

APIProperties VulkanReplay::GetAPIProperties()
{
  APIProperties ret = m_pDriver->APIProps;

  ret.pipelineType = GraphicsAPI::Vulkan;
  ret.localRenderer = GraphicsAPI::Vulkan;
  ret.degraded = false;
  ret.shadersMutable = false;
  ret.rgpCapture =
      m_DriverInfo.vendor == GPUVendor::AMD && m_RGP != NULL && m_RGP->DriverSupportsInterop();
  ret.shaderDebugging = true;
  ret.pixelHistory = true;

  return ret;
}

ReplayStatus VulkanReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDriver->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

void VulkanReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDriver->ReplayLog(0, endEventID, replayType);
}

const SDFile &VulkanReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
}

rdcarray<uint32_t> VulkanReplay::GetPassEvents(uint32_t eventId)
{
  rdcarray<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDriver->GetDrawcall(eventId);

  if(!draw)
    return passEvents;

  // for vulkan a pass == a renderpass, if we're not inside a
  // renderpass then there are no pass events.
  const DrawcallDescription *start = draw;
  while(start)
  {
    // if we've come to the beginning of a pass, break out of the loop, we've
    // found the start.
    // Note that vkCmdNextSubPass has both Begin and End flags set, so it will
    // break out here before we hit the terminating case looking for DrawFlags::EndPass
    if(start->flags & DrawFlags::BeginPass)
      break;

    // if we come to the END of a pass, since we were iterating backwards that
    // means we started outside of a pass, so return empty set.
    // Note that vkCmdNextSubPass has both Begin and End flags set, so it will
    // break out above before we hit this terminating case
    if(start->flags & DrawFlags::EndPass)
      return passEvents;

    // if we've come to the start of the log we were outside of a render pass
    // to start with
    if(start->previous == NULL)
      return passEvents;

    // step back
    start = start->previous;
  }

  // store all the draw eventIDs up to the one specified at the start
  while(start)
  {
    if(start == draw)
      break;

    // include pass boundaries, these will be filtered out later
    // so we don't actually do anything (init postvs/draw overlay)
    // but it's useful to have the first part of the pass as part
    // of the list
    if(start->flags & (DrawFlags::Drawcall | DrawFlags::PassBoundary))
      passEvents.push_back(start->eventId);

    start = start->next;
  }

  return passEvents;
}

ResourceId VulkanReplay::GetLiveID(ResourceId id)
{
  if(!m_pDriver->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDriver->GetResourceManager()->GetLiveID(id);
}

rdcarray<DebugMessage> VulkanReplay::GetDebugMessages()
{
  return m_pDriver->GetDebugMessages();
}

ResourceDescription &VulkanReplay::GetResourceDesc(ResourceId id)
{
  auto it = m_ResourceIdx.find(id);
  if(it == m_ResourceIdx.end())
  {
    m_ResourceIdx[id] = m_Resources.size();
    m_Resources.push_back(ResourceDescription());
    m_Resources.back().resourceId = id;
    return m_Resources.back();
  }

  return m_Resources[it->second];
}

rdcarray<ResourceDescription> VulkanReplay::GetResources()
{
  return m_Resources;
}

rdcarray<TextureDescription> VulkanReplay::GetTextures()
{
  rdcarray<TextureDescription> texs;

  for(auto it = m_pDriver->m_ImageStates.begin(); it != m_pDriver->m_ImageStates.end(); ++it)
  {
    // skip textures that aren't from the capture
    if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    texs.push_back(GetTexture(it->first));
  }

  return texs;
}

rdcarray<BufferDescription> VulkanReplay::GetBuffers()
{
  rdcarray<BufferDescription> bufs;

  for(auto it = m_pDriver->m_CreationInfo.m_Buffer.begin();
      it != m_pDriver->m_CreationInfo.m_Buffer.end(); ++it)
  {
    // skip textures that aren't from the capture
    if(m_pDriver->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    bufs.push_back(GetBuffer(it->first));
  }

  // sort the buffers by ID
  std::sort(bufs.begin(), bufs.end());

  return bufs;
}

TextureDescription VulkanReplay::GetTexture(ResourceId id)
{
  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[id];

  TextureDescription ret = {};
  ret.resourceId = m_pDriver->GetResourceManager()->GetOriginalID(id);
  ret.arraysize = iminfo.arrayLayers;
  ret.creationFlags = iminfo.creationFlags;
  ret.cubemap = iminfo.cube;
  ret.width = iminfo.extent.width;
  ret.height = iminfo.extent.height;
  ret.depth = iminfo.extent.depth;
  ret.mips = iminfo.mipLevels;

  ret.byteSize = 0;
  for(uint32_t s = 0; s < ret.mips; s++)
    ret.byteSize += GetByteSize(ret.width, ret.height, ret.depth, iminfo.format, s);
  ret.byteSize *= ret.arraysize;

  ret.msQual = 0;
  ret.msSamp = RDCMAX(1U, (uint32_t)iminfo.samples);

  ret.byteSize *= ret.msSamp;

  ret.format = MakeResourceFormat(iminfo.format);

  switch(iminfo.type)
  {
    case VK_IMAGE_TYPE_1D:
      ret.type = iminfo.arrayLayers > 1 ? TextureType::Texture1DArray : TextureType::Texture1D;
      ret.dimension = 1;
      break;
    case VK_IMAGE_TYPE_2D:
      if(ret.msSamp > 1)
        ret.type = iminfo.arrayLayers > 1 ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;
      else if(ret.cubemap)
        ret.type = iminfo.arrayLayers > 6 ? TextureType::TextureCubeArray : TextureType::TextureCube;
      else
        ret.type = iminfo.arrayLayers > 1 ? TextureType::Texture2DArray : TextureType::Texture2D;
      ret.dimension = 2;
      break;
    case VK_IMAGE_TYPE_3D:
      ret.type = TextureType::Texture3D;
      ret.dimension = 3;
      break;
    default:
      ret.dimension = 2;
      RDCERR("Unexpected image type");
      break;
  }

  return ret;
}

BufferDescription VulkanReplay::GetBuffer(ResourceId id)
{
  VulkanCreationInfo::Buffer &bufinfo = m_pDriver->m_CreationInfo.m_Buffer[id];

  BufferDescription ret;
  ret.resourceId = m_pDriver->GetResourceManager()->GetOriginalID(id);
  ret.length = bufinfo.size;
  ret.creationFlags = BufferCategory::NoFlags;
  ret.gpuAddress = bufinfo.gpuAddress;

  if(bufinfo.usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
    ret.creationFlags |= BufferCategory::ReadWrite;
  if(bufinfo.usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT))
    ret.creationFlags |= BufferCategory::Constants;
  if(bufinfo.usage & (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT))
    ret.creationFlags |= BufferCategory::Indirect;
  if(bufinfo.usage & (VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
    ret.creationFlags |= BufferCategory::Index;
  if(bufinfo.usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
    ret.creationFlags |= BufferCategory::Vertex;

  return ret;
}

rdcarray<ShaderEntryPoint> VulkanReplay::GetShaderEntryPoints(ResourceId shader)
{
  auto shad = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);

  if(shad == m_pDriver->m_CreationInfo.m_ShaderModule.end())
    return {};

  rdcarray<rdcstr> entries = shad->second.spirv.EntryPoints();

  rdcarray<ShaderEntryPoint> ret;

  for(const rdcstr &e : entries)
    ret.push_back({e, shad->second.spirv.StageForEntry(e)});

  return ret;
}

ShaderReflection *VulkanReplay::GetShader(ResourceId pipeline, ResourceId shader,
                                          ShaderEntryPoint entry)
{
  auto shad = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);

  if(shad == m_pDriver->m_CreationInfo.m_ShaderModule.end())
  {
    RDCERR("Can't get shader details");
    return NULL;
  }

  // if this shader was never used in a pipeline the reflection won't be prepared. Do that now -
  // this will be ignored if it was already prepared.
  shad->second.GetReflection(entry.name, pipeline)
      .Init(GetResourceManager(), shader, shad->second.spirv, entry.name,
            VkShaderStageFlagBits(1 << uint32_t(entry.stage)), {});

  return &shad->second.GetReflection(entry.name, pipeline).refl;
}

rdcarray<rdcstr> VulkanReplay::GetDisassemblyTargets(bool withPipeline)
{
  rdcarray<rdcstr> ret;

  if(withPipeline && m_pDriver->GetExtensions(NULL).ext_AMD_shader_info)
    ret.push_back(AMDShaderInfoTarget);

  if(withPipeline && m_pDriver->GetExtensions(NULL).ext_KHR_pipeline_executable_properties)
    ret.push_back(KHRExecutablePropertiesTarget);

  // default is always first
  ret.insert(0, SPIRVDisassemblyTarget);

  // could add canonical disassembly here if spirv-dis is available
  // Ditto for SPIRV-cross (to glsl/hlsl)

  return ret;
}

void VulkanReplay::CachePipelineExecutables(ResourceId pipeline)
{
  auto it = m_PipelineExecutables.insert({pipeline, rdcarray<PipelineExecutables>()});

  if(!it.second)
    return;

  rdcarray<PipelineExecutables> &data = it.first->second;

  VkPipeline pipe = m_pDriver->GetResourceManager()->GetCurrentHandle<VkPipeline>(pipeline);

  VkPipelineInfoKHR pipeInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR, NULL, Unwrap(pipe),
  };

  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  uint32_t execCount = 0;
  vt->GetPipelineExecutablePropertiesKHR(Unwrap(dev), &pipeInfo, &execCount, NULL);

  rdcarray<VkPipelineExecutablePropertiesKHR> executables;
  executables.resize(execCount);
  data.resize(execCount);
  vt->GetPipelineExecutablePropertiesKHR(Unwrap(dev), &pipeInfo, &execCount, executables.data());

  for(uint32_t i = 0; i < execCount; i++)
  {
    const VkPipelineExecutablePropertiesKHR &exec = executables[i];
    PipelineExecutables &out = data[i];
    out.name = exec.name;
    out.description = exec.description;
    out.stages = exec.stages;
    out.subgroupSize = exec.subgroupSize;
    rdcarray<VkPipelineExecutableStatisticKHR> &stats = out.statistics;
    rdcarray<VkPipelineExecutableInternalRepresentationKHR> &irs = out.representations;

    VkPipelineExecutableInfoKHR pipeExecInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR, NULL, Unwrap(pipe), i,
    };

    // enumerate statistics
    uint32_t statCount = 0;
    vt->GetPipelineExecutableStatisticsKHR(Unwrap(dev), &pipeExecInfo, &statCount, NULL);

    stats.resize(statCount);
    for(uint32_t s = 0; s < statCount; s++)
      stats[s].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR;
    vt->GetPipelineExecutableStatisticsKHR(Unwrap(dev), &pipeExecInfo, &statCount, stats.data());

    // enumerate internal representations
    uint32_t irCount = 0;
    vt->GetPipelineExecutableInternalRepresentationsKHR(Unwrap(dev), &pipeExecInfo, &irCount, NULL);

    irs.resize(irCount);
    for(uint32_t ir = 0; ir < irCount; ir++)
      irs[ir].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR;
    vt->GetPipelineExecutableInternalRepresentationsKHR(Unwrap(dev), &pipeExecInfo, &irCount,
                                                        irs.data());

    // need to now allocate space, and try again
    out.irbytes.resize(irCount);
    for(uint32_t ir = 0; ir < irCount; ir++)
    {
      out.irbytes[ir].resize(irs[ir].dataSize);
      irs[ir].pData = out.irbytes[ir].data();
    }

    vt->GetPipelineExecutableInternalRepresentationsKHR(Unwrap(dev), &pipeExecInfo, &irCount,
                                                        irs.data());
  }
}

rdcstr VulkanReplay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                       const rdcstr &target)
{
  auto it = m_pDriver->m_CreationInfo.m_ShaderModule.find(
      GetResourceManager()->GetLiveID(refl->resourceId));

  if(it == m_pDriver->m_CreationInfo.m_ShaderModule.end())
    return "; Invalid Shader Specified";

  if(target == SPIRVDisassemblyTarget || target.empty())
  {
    VulkanCreationInfo::ShaderModuleReflection &moduleRefl =
        it->second.GetReflection(refl->entryPoint, pipeline);
    moduleRefl.PopulateDisassembly(it->second.spirv);

    return moduleRefl.disassembly;
  }

  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  if(target == AMDShaderInfoTarget && vt->GetShaderInfoAMD)
  {
    if(pipeline == ResourceId())
    {
      return "; No pipeline specified, VK_AMD_shader_info disassembly is not available\n"
             "; Shader must be disassembled with a specific pipeline.";
    }

    VkPipeline pipe = m_pDriver->GetResourceManager()->GetCurrentHandle<VkPipeline>(pipeline);

    VkShaderStageFlagBits stageBit =
        VkShaderStageFlagBits(1 << it->second.GetReflection(refl->entryPoint, pipeline).stageIndex);

    size_t size;
    vt->GetShaderInfoAMD(Unwrap(dev), Unwrap(pipe), stageBit, VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD,
                         &size, NULL);

    rdcstr disasm;
    disasm.resize(size);
    vt->GetShaderInfoAMD(Unwrap(dev), Unwrap(pipe), stageBit, VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD,
                         &size, (void *)disasm.data());

    return disasm;
  }

  if(target == KHRExecutablePropertiesTarget && vt->GetPipelineExecutablePropertiesKHR)
  {
    if(pipeline == ResourceId())
    {
      return "; No pipeline specified, VK_KHR_pipeline_executable_properties disassembly is not "
             "available\n"
             "; Shader must be disassembled with a specific pipeline.";
    }

    CachePipelineExecutables(pipeline);

    VkShaderStageFlagBits stageBit =
        VkShaderStageFlagBits(1 << it->second.GetReflection(refl->entryPoint, pipeline).stageIndex);

    const rdcarray<PipelineExecutables> &executables = m_PipelineExecutables[pipeline];

    rdcstr disasm;

    for(const PipelineExecutables &exec : executables)
    {
      // if this executable is associated with our stage, definitely include it. If this executable
      // is associated with *no* stages, then also include it (since we don't know what it
      // corresponds to)
      if((exec.stages & stageBit) || exec.stages == 0)
      {
        disasm += "======== " + exec.name + " ========\n\n";
        disasm += exec.description + "\n\n";

        // statistics first
        disasm += StringFormat::Fmt(
            "==== Statistics ====\n\n"
            "Subgroup Size: %u"
            "      // the subgroup size with which this executable is dispatched.\n",
            exec.subgroupSize);

        for(const VkPipelineExecutableStatisticKHR &stat : exec.statistics)
        {
          rdcstr value;

          switch(stat.format)
          {
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
              value = stat.value.b32 ? "true" : "false";
              break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
              value = ToStr(stat.value.i64);
              break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
              value = ToStr(stat.value.u64);
              break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:
              value = ToStr(stat.value.f64);
              break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_MAX_ENUM_KHR: value = "???"; break;
          }

          disasm +=
              StringFormat::Fmt("%s: %s      // %s\n", stat.name, value.c_str(), stat.description);
        }

        // then IRs

        if(!exec.representations.empty())
          disasm += "\n\n==== Internal Representations ====\n\n";

        for(const VkPipelineExecutableInternalRepresentationKHR &ir : exec.representations)
        {
          disasm += "---- " + rdcstr(ir.name) + " ----\n\n";
          disasm += "; " + rdcstr(ir.description) + "\n\n";
          if(ir.isText)
          {
            char *str = (char *)ir.pData;
            // should already be NULL terminated but let's be sure
            str[ir.dataSize - 1] = 0;
            disasm += str;
          }
          else
          {
            // canonical hexdump display
            size_t bytesRemaining = ir.dataSize;
            size_t offset = 0;
            const byte *src = (const byte *)ir.pData;
            while(bytesRemaining > 0)
            {
              uint8_t row[16];
              const size_t copySize = RDCMIN(sizeof(row), bytesRemaining);

              memcpy(row, src + offset, copySize);

              disasm += StringFormat::Fmt("%08zx ", offset);
              for(size_t b = 0; b < 16; b++)
              {
                if(b < bytesRemaining)
                  disasm += StringFormat::Fmt("%02hhx ", row[b]);
                else
                  disasm += "   ";

                if(b == 7 || b == 15)
                  disasm += " ";
              }

              disasm += "|";

              for(size_t b = 0; b < 16; b++)
              {
                if(b < bytesRemaining)
                {
                  char c = (char)row[b];
                  if(isprint(c))
                    disasm.push_back(c);
                  else
                    disasm.push_back('.');
                }
              }

              disasm += "|";

              disasm += "\n";

              offset += copySize;
              bytesRemaining -= copySize;
            }

            disasm += StringFormat::Fmt("%08zx", offset);
          }
        }
      }
    }

    return disasm;
  }

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
}

void VulkanReplay::RenderCheckerboard(FloatVector dark, FloatVector light)
{
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
  const VkDevDispatchTable *vt = ObjDisp(dev);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  uint32_t uboOffs = 0;

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(outw.rp),
      Unwrap(outw.fb),
      {{
           0, 0,
       },
       {m_DebugWidth, m_DebugHeight}},
      0,
      NULL,
  };
  vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

  if(m_Overlay.m_CheckerPipeline != VK_NULL_HANDLE)
  {
    CheckerboardUBOData *data = (CheckerboardUBOData *)m_Overlay.m_CheckerUBO.Map(&uboOffs);
    data->BorderWidth = 0.0f;
    data->RectPosition = Vec2f();
    data->RectSize = Vec2f();
    data->CheckerSquareDimension = 64.0f;
    data->InnerColor = Vec4f();

    data->PrimaryColor = ConvertSRGBToLinear(light);
    data->SecondaryColor = ConvertSRGBToLinear(dark);
    m_Overlay.m_CheckerUBO.Unmap();

    vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                        outw.dsimg == VK_NULL_HANDLE ? Unwrap(m_Overlay.m_CheckerPipeline)
                                                     : Unwrap(m_Overlay.m_CheckerMSAAPipeline));
    vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                              Unwrap(m_Overlay.m_CheckerPipeLayout), 0, 1,
                              UnwrapPtr(m_Overlay.m_CheckerDescSet), 1, &uboOffs);

    VkViewport viewport = {0.0f, 0.0f, (float)m_DebugWidth, (float)m_DebugHeight, 0.0f, 1.0f};
    vt->CmdSetViewport(Unwrap(cmd), 0, 1, &viewport);

    vt->CmdDraw(Unwrap(cmd), 4, 1, 0, 0);

    if(m_pDriver->GetDriverInfo().QualcommLeakingUBOOffsets())
    {
      uboOffs = 0;
      vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                Unwrap(m_Overlay.m_CheckerPipeLayout), 0, 1,
                                UnwrapPtr(m_Overlay.m_CheckerDescSet), 1, &uboOffs);
    }
  }
  else
  {
    // some mobile chips fail to create the checkerboard pipeline. Use an alternate approach with
    // CmdClearAttachment and many rects.

    VkClearAttachment lightCol = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{light.x, light.y, light.z, light.w}}}};
    VkClearAttachment darkCol = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{dark.x, dark.y, dark.z, dark.w}}}};

    VkClearRect fullRect = {{
                                {0, 0}, {outw.width, outw.height},
                            },
                            0,
                            1};

    vt->CmdClearAttachments(Unwrap(cmd), 1, &lightCol, 1, &fullRect);

    rdcarray<VkClearRect> squares;

    for(int32_t y = 0; y < (int32_t)outw.height; y += 128)
    {
      for(int32_t x = 0; x < (int32_t)outw.width; x += 128)
      {
        VkClearRect square = {{
                                  {x, y}, {64, 64},
                              },
                              0,
                              1};

        squares.push_back(square);

        square.rect.offset.x += 64;
        square.rect.offset.y += 64;
        squares.push_back(square);
      }
    }

    vt->CmdClearAttachments(Unwrap(cmd), 1, &darkCol, (uint32_t)squares.size(), squares.data());
  }

  vt->CmdEndRenderPass(Unwrap(cmd));

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif
}

void VulkanReplay::RenderHighlightBox(float w, float h, float scale)
{
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
  const VkDevDispatchTable *vt = ObjDisp(dev);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  {
    VkRenderPassBeginInfo rpbegin = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        NULL,
        Unwrap(outw.rp),
        Unwrap(outw.fb),
        {{
             0, 0,
         },
         {m_DebugWidth, m_DebugHeight}},
        0,
        NULL,
    };
    vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

    VkClearAttachment black = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{0.0f, 0.0f, 0.0f, 1.0f}}}};
    VkClearAttachment white = {VK_IMAGE_ASPECT_COLOR_BIT, 0, {{{1.0f, 1.0f, 1.0f, 1.0f}}}};

    uint32_t sz = uint32_t(scale);

    VkOffset2D tl = {int32_t(w / 2.0f + 0.5f), int32_t(h / 2.0f + 0.5f)};

    VkClearRect rect[4] = {
        {{
             {tl.x, tl.y}, {1, sz},
         },
         0,
         1},
        {{
             {tl.x + (int32_t)sz, tl.y}, {1, sz + 1},
         },
         0,
         1},
        {{
             {tl.x, tl.y}, {sz, 1},
         },
         0,
         1},
        {{
             {tl.x, tl.y + (int32_t)sz}, {sz, 1},
         },
         0,
         1},
    };

    // inner
    vt->CmdClearAttachments(Unwrap(cmd), 1, &white, 4, rect);

    rect[0].rect.offset.x--;
    rect[1].rect.offset.x++;
    rect[2].rect.offset.x--;
    rect[3].rect.offset.x--;

    rect[0].rect.offset.y--;
    rect[1].rect.offset.y--;
    rect[2].rect.offset.y--;
    rect[3].rect.offset.y++;

    rect[0].rect.extent.height += 2;
    rect[1].rect.extent.height += 2;
    rect[2].rect.extent.width += 2;
    rect[3].rect.extent.width += 2;

    // outer
    vt->CmdClearAttachments(Unwrap(cmd), 1, &black, 4, rect);

    vt->CmdEndRenderPass(Unwrap(cmd));
  }

  vkr = vt->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
  m_pDriver->SubmitCmds();
#endif
}

void VulkanReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  GetDebugManager()->GetBufferData(buff, offset, len, retData);
}

void VulkanReplay::FileChanged()
{
}

void VulkanReplay::GetInitialDriverVersion()
{
  RDCEraseEl(m_DriverInfo);

  VkInstance inst = m_pDriver->GetInstance();

  uint32_t count;
  VkResult vkr = ObjDisp(inst)->EnumeratePhysicalDevices(Unwrap(inst), &count, NULL);

  if(vkr != VK_SUCCESS)
  {
    RDCERR("Couldn't enumerate physical devices");
    return;
  }

  if(count == 0)
  {
    RDCERR("No physical devices available");
  }

  count = 1;
  VkPhysicalDevice firstDevice = VK_NULL_HANDLE;

  vkr = ObjDisp(inst)->EnumeratePhysicalDevices(Unwrap(inst), &count, &firstDevice);

  // incomplete is expected if multiple GPUs are present, and we're just grabbing the first
  if(vkr != VK_SUCCESS && vkr != VK_INCOMPLETE)
  {
    RDCERR("Couldn't fetch first physical device");
    return;
  }

  VkPhysicalDeviceProperties props;
  ObjDisp(inst)->GetPhysicalDeviceProperties(firstDevice, &props);

  SetDriverInformation(props);
}

void VulkanReplay::SetDriverInformation(const VkPhysicalDeviceProperties &props)
{
  VkDriverInfo info(props);
  m_DriverInfo.vendor = info.Vendor();
  rdcstr versionString =
      StringFormat::Fmt("%s %u.%u.%u", props.deviceName, info.Major(), info.Minor(), info.Patch());
  versionString.resize(RDCMIN(versionString.size(), ARRAY_COUNT(m_DriverInfo.version) - 1));
  memcpy(m_DriverInfo.version, versionString.c_str(), versionString.size());
}

static TextureSwizzle Convert(VkComponentSwizzle src, int i)
{
  switch(src)
  {
    default: RDCWARN("Unexpected component swizzle value %d", (int)src); DELIBERATE_FALLTHROUGH();
    case VK_COMPONENT_SWIZZLE_IDENTITY: break;
    case VK_COMPONENT_SWIZZLE_ZERO: return TextureSwizzle::Zero;
    case VK_COMPONENT_SWIZZLE_ONE: return TextureSwizzle::One;
    case VK_COMPONENT_SWIZZLE_R: return TextureSwizzle::Red;
    case VK_COMPONENT_SWIZZLE_G: return TextureSwizzle::Green;
    case VK_COMPONENT_SWIZZLE_B: return TextureSwizzle::Blue;
    case VK_COMPONENT_SWIZZLE_A: return TextureSwizzle::Alpha;
  }

  return TextureSwizzle(uint32_t(TextureSwizzle::Red) + i);
}

static void Convert(TextureSwizzle4 &dst, VkComponentMapping src)
{
  dst.red = Convert(src.r, 0);
  dst.green = Convert(src.g, 1);
  dst.blue = Convert(src.b, 2);
  dst.alpha = Convert(src.a, 3);
}

void VulkanReplay::SavePipelineState(uint32_t eventId)
{
  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  VkMarkerRegion::Begin(StringFormat::Fmt("FetchShaderFeedback for %u", eventId));

  FetchShaderFeedback(eventId);

  VkMarkerRegion::End();

  {
    // reset the pipeline state, but keep the descriptor set arrays. This prevents needless
    // reallocations, we'll ensure that descriptors are fully overwritten below.
    rdcarray<VKPipe::DescriptorSet> graphicsDescriptors;
    rdcarray<VKPipe::DescriptorSet> computeDescriptors;

    m_VulkanPipelineState.graphics.descriptorSets.swap(graphicsDescriptors);
    m_VulkanPipelineState.compute.descriptorSets.swap(computeDescriptors);

    m_VulkanPipelineState = VKPipe::State();

    m_VulkanPipelineState.graphics.descriptorSets.swap(graphicsDescriptors);
    m_VulkanPipelineState.compute.descriptorSets.swap(computeDescriptors);
  }

  m_VulkanPipelineState.pushconsts.resize(state.pushConstSize);
  memcpy(m_VulkanPipelineState.pushconsts.data(), state.pushconsts, state.pushConstSize);

  // General pipeline properties
  m_VulkanPipelineState.compute.pipelineResourceId =
      rm->GetUnreplacedOriginalID(state.compute.pipeline);
  m_VulkanPipelineState.graphics.pipelineResourceId =
      rm->GetUnreplacedOriginalID(state.graphics.pipeline);

  if(state.compute.pipeline != ResourceId())
  {
    const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.compute.pipeline];

    m_VulkanPipelineState.compute.pipelineLayoutResourceId = rm->GetOriginalID(p.layout);

    m_VulkanPipelineState.compute.flags = p.flags;

    VKPipe::Shader &stage = m_VulkanPipelineState.computeShader;

    int i = 5;    // 5 is the CS idx (VS, TCS, TES, GS, FS, CS)
    {
      stage.resourceId = rm->GetUnreplacedOriginalID(p.shaders[i].module);
      stage.entryPoint = p.shaders[i].entryPoint;

      stage.stage = ShaderStage::Compute;
      if(p.shaders[i].mapping)
        stage.bindpointMapping = *p.shaders[i].mapping;
      if(p.shaders[i].refl)
        stage.reflection = p.shaders[i].refl;

      stage.specialization.resize(p.shaders[i].specialization.size());
      for(size_t s = 0; s < p.shaders[i].specialization.size(); s++)
      {
        const SpecConstant &spec = p.shaders[i].specialization[s];
        stage.specialization[s].specializationId = spec.specID;
        stage.specialization[s].data.resize(spec.dataSize);
        memcpy(stage.specialization[s].data.data(), &spec.value, spec.dataSize);
      }
    }
  }
  else
  {
    m_VulkanPipelineState.compute.pipelineLayoutResourceId = ResourceId();
    m_VulkanPipelineState.compute.flags = 0;
    m_VulkanPipelineState.computeShader = VKPipe::Shader();
  }

  if(state.graphics.pipeline != ResourceId())
  {
    const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.graphics.pipeline];

    m_VulkanPipelineState.graphics.pipelineLayoutResourceId = rm->GetOriginalID(p.layout);

    m_VulkanPipelineState.graphics.flags = p.flags;

    // Input Assembly
    m_VulkanPipelineState.inputAssembly.indexBuffer.resourceId = rm->GetOriginalID(state.ibuffer.buf);
    m_VulkanPipelineState.inputAssembly.indexBuffer.byteOffset = state.ibuffer.offs;
    m_VulkanPipelineState.inputAssembly.primitiveRestartEnable = p.primitiveRestartEnable;

    // Vertex Input
    m_VulkanPipelineState.vertexInput.attributes.resize(p.vertexAttrs.size());
    for(size_t i = 0; i < p.vertexAttrs.size(); i++)
    {
      m_VulkanPipelineState.vertexInput.attributes[i].location = p.vertexAttrs[i].location;
      m_VulkanPipelineState.vertexInput.attributes[i].binding = p.vertexAttrs[i].binding;
      m_VulkanPipelineState.vertexInput.attributes[i].byteOffset = p.vertexAttrs[i].byteoffset;
      m_VulkanPipelineState.vertexInput.attributes[i].format =
          MakeResourceFormat(p.vertexAttrs[i].format);
    }

    m_VulkanPipelineState.vertexInput.bindings.resize(p.vertexBindings.size());
    for(size_t i = 0; i < p.vertexBindings.size(); i++)
    {
      m_VulkanPipelineState.vertexInput.bindings[i].vertexBufferBinding =
          p.vertexBindings[i].vbufferBinding;
      m_VulkanPipelineState.vertexInput.bindings[i].perInstance = p.vertexBindings[i].perInstance;
      m_VulkanPipelineState.vertexInput.bindings[i].instanceDivisor =
          p.vertexBindings[i].instanceDivisor;
    }

    m_VulkanPipelineState.vertexInput.vertexBuffers.resize(state.vbuffers.size());
    for(size_t i = 0; i < state.vbuffers.size(); i++)
    {
      m_VulkanPipelineState.vertexInput.vertexBuffers[i].resourceId =
          rm->GetOriginalID(state.vbuffers[i].buf);
      m_VulkanPipelineState.vertexInput.vertexBuffers[i].byteOffset = state.vbuffers[i].offs;
      m_VulkanPipelineState.vertexInput.vertexBuffers[i].byteStride =
          (uint32_t)state.vbuffers[i].stride;
      m_VulkanPipelineState.vertexInput.vertexBuffers[i].byteSize = (uint32_t)state.vbuffers[i].size;
    }

    // Shader Stages
    VKPipe::Shader *stages[] = {
        &m_VulkanPipelineState.vertexShader,   &m_VulkanPipelineState.tessControlShader,
        &m_VulkanPipelineState.tessEvalShader, &m_VulkanPipelineState.geometryShader,
        &m_VulkanPipelineState.fragmentShader,
    };

    for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
    {
      stages[i]->resourceId = rm->GetUnreplacedOriginalID(p.shaders[i].module);
      stages[i]->entryPoint = p.shaders[i].entryPoint;

      stages[i]->stage = StageFromIndex(i);
      if(p.shaders[i].mapping)
        stages[i]->bindpointMapping = *p.shaders[i].mapping;
      if(p.shaders[i].refl)
        stages[i]->reflection = p.shaders[i].refl;

      stages[i]->specialization.resize(p.shaders[i].specialization.size());
      for(size_t s = 0; s < p.shaders[i].specialization.size(); s++)
      {
        const SpecConstant &spec = p.shaders[i].specialization[s];
        stages[i]->specialization[s].specializationId = spec.specID;
        stages[i]->specialization[s].data.resize(spec.dataSize);
        memcpy(stages[i]->specialization[s].data.data(), &spec.value, spec.dataSize);
      }
    }

    // Tessellation
    m_VulkanPipelineState.tessellation.numControlPoints = p.patchControlPoints;

    m_VulkanPipelineState.tessellation.domainOriginUpperLeft =
        p.tessellationDomainOrigin == VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

    // Transform feedback
    m_VulkanPipelineState.transformFeedback.buffers.resize(state.xfbbuffers.size());
    for(size_t i = 0; i < state.xfbbuffers.size(); i++)
    {
      m_VulkanPipelineState.transformFeedback.buffers[i].bufferResourceId =
          rm->GetOriginalID(state.xfbbuffers[i].buf);
      m_VulkanPipelineState.transformFeedback.buffers[i].byteOffset = state.xfbbuffers[i].offs;
      m_VulkanPipelineState.transformFeedback.buffers[i].byteSize = state.xfbbuffers[i].size;

      m_VulkanPipelineState.transformFeedback.buffers[i].active = false;
      m_VulkanPipelineState.transformFeedback.buffers[i].counterBufferResourceId = ResourceId();
      m_VulkanPipelineState.transformFeedback.buffers[i].counterBufferOffset = 0;

      if(i >= state.firstxfbcounter)
      {
        size_t xfb = i - state.firstxfbcounter;
        if(xfb < state.xfbcounters.size())
        {
          m_VulkanPipelineState.transformFeedback.buffers[i].active = true;
          m_VulkanPipelineState.transformFeedback.buffers[i].counterBufferResourceId =
              rm->GetOriginalID(state.xfbcounters[xfb].buf);
          m_VulkanPipelineState.transformFeedback.buffers[i].counterBufferOffset =
              state.xfbcounters[xfb].offs;
        }
      }
    }

    // Viewport/Scissors
    size_t numViewScissors = state.views.size();
    m_VulkanPipelineState.viewportScissor.viewportScissors.resize(numViewScissors);
    for(size_t i = 0; i < numViewScissors; i++)
    {
      if(i < state.views.size())
      {
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp.x = state.views[i].x;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp.y = state.views[i].y;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp.width = state.views[i].width;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp.height = state.views[i].height;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp.minDepth =
            state.views[i].minDepth;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp.maxDepth =
            state.views[i].maxDepth;
      }
      else
      {
        RDCEraseEl(m_VulkanPipelineState.viewportScissor.viewportScissors[i].vp);
      }

      if(i < state.scissors.size())
      {
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].scissor.x =
            state.scissors[i].offset.x;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].scissor.y =
            state.scissors[i].offset.y;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].scissor.width =
            state.scissors[i].extent.width;
        m_VulkanPipelineState.viewportScissor.viewportScissors[i].scissor.height =
            state.scissors[i].extent.height;
      }
      else
      {
        RDCEraseEl(m_VulkanPipelineState.viewportScissor.viewportScissors[i].scissor);
      }
    }

    {
      m_VulkanPipelineState.viewportScissor.discardRectangles.resize(p.discardRectangles.size());
      for(size_t i = 0; i < p.discardRectangles.size() && i < state.discardRectangles.size(); i++)
      {
        m_VulkanPipelineState.viewportScissor.discardRectangles[i].x =
            state.discardRectangles[i].offset.x;
        m_VulkanPipelineState.viewportScissor.discardRectangles[i].y =
            state.discardRectangles[i].offset.y;
        m_VulkanPipelineState.viewportScissor.discardRectangles[i].width =
            state.discardRectangles[i].extent.width;
        m_VulkanPipelineState.viewportScissor.discardRectangles[i].height =
            state.discardRectangles[i].extent.height;
      }

      m_VulkanPipelineState.viewportScissor.discardRectanglesExclusive =
          (p.discardMode == VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT);
    }

    // Rasterizer
    m_VulkanPipelineState.rasterizer.depthClampEnable = p.depthClampEnable;
    m_VulkanPipelineState.rasterizer.depthClipEnable = p.depthClipEnable;
    m_VulkanPipelineState.rasterizer.rasterizerDiscardEnable = p.rasterizerDiscardEnable;
    m_VulkanPipelineState.rasterizer.frontCCW = state.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE;

    m_VulkanPipelineState.rasterizer.conservativeRasterization = ConservativeRaster::Disabled;
    switch(p.conservativeRasterizationMode)
    {
      case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
        m_VulkanPipelineState.rasterizer.conservativeRasterization =
            ConservativeRaster::Underestimate;
        break;
      case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
        m_VulkanPipelineState.rasterizer.conservativeRasterization = ConservativeRaster::Overestimate;
        break;
      default: break;
    }

    m_VulkanPipelineState.rasterizer.lineRasterMode = LineRaster::Default;

    // "VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT is equivalent to
    // VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT if VkPhysicalDeviceLimits::strictLines is VK_TRUE"
    if(m_pDriver->GetDeviceProps().limits.strictLines)
      m_VulkanPipelineState.rasterizer.lineRasterMode = LineRaster::Rectangular;

    switch(p.lineRasterMode)
    {
      case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT:
        m_VulkanPipelineState.rasterizer.lineRasterMode = LineRaster::Rectangular;
        break;
      case VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT:
        m_VulkanPipelineState.rasterizer.lineRasterMode = LineRaster::Bresenham;
        break;
      case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT:
        m_VulkanPipelineState.rasterizer.lineRasterMode = LineRaster::RectangularSmooth;
        break;
      default: break;
    }

    m_VulkanPipelineState.rasterizer.lineStippleFactor = state.stippleFactor;
    m_VulkanPipelineState.rasterizer.lineStipplePattern = state.stipplePattern;

    m_VulkanPipelineState.rasterizer.extraPrimitiveOverestimationSize =
        p.extraPrimitiveOverestimationSize;

    switch(p.polygonMode)
    {
      case VK_POLYGON_MODE_POINT:
        m_VulkanPipelineState.rasterizer.fillMode = FillMode::Point;
        break;
      case VK_POLYGON_MODE_LINE:
        m_VulkanPipelineState.rasterizer.fillMode = FillMode::Wireframe;
        break;
      case VK_POLYGON_MODE_FILL: m_VulkanPipelineState.rasterizer.fillMode = FillMode::Solid; break;
      default:
        m_VulkanPipelineState.rasterizer.fillMode = FillMode::Solid;
        RDCERR("Unexpected value for FillMode %x", p.polygonMode);
        break;
    }

    switch(state.cullMode)
    {
      case VK_CULL_MODE_NONE: m_VulkanPipelineState.rasterizer.cullMode = CullMode::NoCull; break;
      case VK_CULL_MODE_FRONT_BIT:
        m_VulkanPipelineState.rasterizer.cullMode = CullMode::Front;
        break;
      case VK_CULL_MODE_BACK_BIT: m_VulkanPipelineState.rasterizer.cullMode = CullMode::Back; break;
      case VK_CULL_MODE_FRONT_AND_BACK:
        m_VulkanPipelineState.rasterizer.cullMode = CullMode::FrontAndBack;
        break;
      default:
        m_VulkanPipelineState.rasterizer.cullMode = CullMode::NoCull;
        RDCERR("Unexpected value for CullMode %x", state.cullMode);
        break;
    }

    m_VulkanPipelineState.rasterizer.depthBias = state.bias.depth;
    m_VulkanPipelineState.rasterizer.depthBiasClamp = state.bias.biasclamp;
    m_VulkanPipelineState.rasterizer.slopeScaledDepthBias = state.bias.slope;
    m_VulkanPipelineState.rasterizer.lineWidth = state.lineWidth;

    // MSAA
    m_VulkanPipelineState.multisample.rasterSamples = p.rasterizationSamples;
    m_VulkanPipelineState.multisample.sampleShadingEnable = p.sampleShadingEnable;
    m_VulkanPipelineState.multisample.minSampleShading = p.minSampleShading;
    m_VulkanPipelineState.multisample.sampleMask = p.sampleMask;

    m_VulkanPipelineState.multisample.sampleLocations.customLocations.clear();
    if(p.sampleLocations.enabled)
    {
      m_VulkanPipelineState.multisample.sampleLocations.gridWidth =
          state.sampleLocations.gridSize.width;
      m_VulkanPipelineState.multisample.sampleLocations.gridHeight =
          state.sampleLocations.gridSize.height;
      m_VulkanPipelineState.multisample.sampleLocations.customLocations.reserve(
          state.sampleLocations.locations.size());
      for(const VkSampleLocationEXT &loc : state.sampleLocations.locations)
      {
        m_VulkanPipelineState.multisample.sampleLocations.customLocations.push_back(
            {loc.x, loc.y, 0.0f, 0.0f});
      }
    }

    // Color Blend
    m_VulkanPipelineState.colorBlend.alphaToCoverageEnable = p.alphaToCoverageEnable;
    m_VulkanPipelineState.colorBlend.alphaToOneEnable = p.alphaToOneEnable;

    m_VulkanPipelineState.colorBlend.blends.resize(p.attachments.size());
    for(size_t i = 0; i < p.attachments.size(); i++)
    {
      m_VulkanPipelineState.colorBlend.blends[i].enabled = p.attachments[i].blendEnable;

      // due to shared structs, this is slightly duplicated - Vulkan doesn't have separate states
      // for logic operations
      m_VulkanPipelineState.colorBlend.blends[i].logicOperationEnabled = p.logicOpEnable;
      m_VulkanPipelineState.colorBlend.blends[i].logicOperation = MakeLogicOp(p.logicOp);

      m_VulkanPipelineState.colorBlend.blends[i].colorBlend.source =
          MakeBlendMultiplier(p.attachments[i].blend.Source);
      m_VulkanPipelineState.colorBlend.blends[i].colorBlend.destination =
          MakeBlendMultiplier(p.attachments[i].blend.Destination);
      m_VulkanPipelineState.colorBlend.blends[i].colorBlend.operation =
          MakeBlendOp(p.attachments[i].blend.Operation);

      m_VulkanPipelineState.colorBlend.blends[i].alphaBlend.source =
          MakeBlendMultiplier(p.attachments[i].alphaBlend.Source);
      m_VulkanPipelineState.colorBlend.blends[i].alphaBlend.destination =
          MakeBlendMultiplier(p.attachments[i].alphaBlend.Destination);
      m_VulkanPipelineState.colorBlend.blends[i].alphaBlend.operation =
          MakeBlendOp(p.attachments[i].alphaBlend.Operation);

      m_VulkanPipelineState.colorBlend.blends[i].writeMask = p.attachments[i].channelWriteMask;
    }

    memcpy(m_VulkanPipelineState.colorBlend.blendFactor, state.blendConst, sizeof(float) * 4);

    // Depth Stencil
    m_VulkanPipelineState.depthStencil.depthTestEnable = state.depthTestEnable != VK_FALSE;
    m_VulkanPipelineState.depthStencil.depthWriteEnable = state.depthWriteEnable != VK_FALSE;
    m_VulkanPipelineState.depthStencil.depthBoundsEnable = state.depthBoundsTestEnable != VK_FALSE;
    m_VulkanPipelineState.depthStencil.depthFunction = MakeCompareFunc(state.depthCompareOp);
    m_VulkanPipelineState.depthStencil.stencilTestEnable = state.stencilTestEnable != VK_FALSE;

    m_VulkanPipelineState.depthStencil.frontFace.passOperation = MakeStencilOp(state.front.passOp);
    m_VulkanPipelineState.depthStencil.frontFace.failOperation = MakeStencilOp(state.front.failOp);
    m_VulkanPipelineState.depthStencil.frontFace.depthFailOperation =
        MakeStencilOp(state.front.depthFailOp);
    m_VulkanPipelineState.depthStencil.frontFace.function = MakeCompareFunc(state.front.compareOp);

    m_VulkanPipelineState.depthStencil.backFace.passOperation = MakeStencilOp(state.back.passOp);
    m_VulkanPipelineState.depthStencil.backFace.failOperation = MakeStencilOp(state.back.failOp);
    m_VulkanPipelineState.depthStencil.backFace.depthFailOperation =
        MakeStencilOp(state.back.depthFailOp);
    m_VulkanPipelineState.depthStencil.backFace.function = MakeCompareFunc(state.back.compareOp);

    m_VulkanPipelineState.depthStencil.minDepthBounds = state.mindepth;
    m_VulkanPipelineState.depthStencil.maxDepthBounds = state.maxdepth;

    m_VulkanPipelineState.depthStencil.frontFace.reference = state.front.ref;
    m_VulkanPipelineState.depthStencil.frontFace.compareMask = state.front.compare;
    m_VulkanPipelineState.depthStencil.frontFace.writeMask = state.front.write;

    m_VulkanPipelineState.depthStencil.backFace.reference = state.back.ref;
    m_VulkanPipelineState.depthStencil.backFace.compareMask = state.back.compare;
    m_VulkanPipelineState.depthStencil.backFace.writeMask = state.back.write;
  }
  else
  {
    m_VulkanPipelineState.graphics.pipelineLayoutResourceId = ResourceId();

    m_VulkanPipelineState.graphics.flags = 0;

    m_VulkanPipelineState.vertexInput.attributes.clear();
    m_VulkanPipelineState.vertexInput.bindings.clear();
    m_VulkanPipelineState.vertexInput.vertexBuffers.clear();

    VKPipe::Shader *stages[] = {
        &m_VulkanPipelineState.vertexShader,   &m_VulkanPipelineState.tessControlShader,
        &m_VulkanPipelineState.tessEvalShader, &m_VulkanPipelineState.geometryShader,
        &m_VulkanPipelineState.fragmentShader,
    };

    for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
      *stages[i] = VKPipe::Shader();

    m_VulkanPipelineState.viewportScissor.viewportScissors.clear();
    m_VulkanPipelineState.viewportScissor.discardRectangles.clear();
    m_VulkanPipelineState.viewportScissor.discardRectanglesExclusive = true;

    m_VulkanPipelineState.colorBlend.blends.clear();
  }

  if(state.renderPass != ResourceId())
  {
    // Renderpass
    m_VulkanPipelineState.currentPass.renderpass.resourceId = rm->GetOriginalID(state.renderPass);
    m_VulkanPipelineState.currentPass.renderpass.subpass = state.subpass;
    if(state.renderPass != ResourceId())
    {
      m_VulkanPipelineState.currentPass.renderpass.inputAttachments =
          c.m_RenderPass[state.renderPass].subpasses[state.subpass].inputAttachments;
      m_VulkanPipelineState.currentPass.renderpass.colorAttachments =
          c.m_RenderPass[state.renderPass].subpasses[state.subpass].colorAttachments;
      m_VulkanPipelineState.currentPass.renderpass.resolveAttachments =
          c.m_RenderPass[state.renderPass].subpasses[state.subpass].resolveAttachments;
      m_VulkanPipelineState.currentPass.renderpass.depthstencilAttachment =
          c.m_RenderPass[state.renderPass].subpasses[state.subpass].depthstencilAttachment;
      m_VulkanPipelineState.currentPass.renderpass.fragmentDensityAttachment =
          c.m_RenderPass[state.renderPass].subpasses[state.subpass].fragmentDensityAttachment;

      m_VulkanPipelineState.currentPass.renderpass.multiviews =
          c.m_RenderPass[state.renderPass].subpasses[state.subpass].multiviews;
    }

    ResourceId fb = state.GetFramebuffer();

    m_VulkanPipelineState.currentPass.framebuffer.resourceId = rm->GetOriginalID(fb);

    if(fb != ResourceId())
    {
      m_VulkanPipelineState.currentPass.framebuffer.width = c.m_Framebuffer[fb].width;
      m_VulkanPipelineState.currentPass.framebuffer.height = c.m_Framebuffer[fb].height;
      m_VulkanPipelineState.currentPass.framebuffer.layers = c.m_Framebuffer[fb].layers;

      m_VulkanPipelineState.currentPass.framebuffer.attachments.resize(
          c.m_Framebuffer[fb].attachments.size());
      for(size_t i = 0; i < c.m_Framebuffer[fb].attachments.size(); i++)
      {
        ResourceId viewid = state.GetFramebufferAttachments()[i];

        if(viewid != ResourceId())
        {
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].viewResourceId =
              rm->GetOriginalID(viewid);
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].imageResourceId =
              rm->GetOriginalID(c.m_ImageView[viewid].image);

          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].viewFormat =
              MakeResourceFormat(c.m_ImageView[viewid].format);
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].firstMip =
              c.m_ImageView[viewid].range.baseMipLevel;
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].firstSlice =
              c.m_ImageView[viewid].range.baseArrayLayer;
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].numMips =
              c.m_ImageView[viewid].range.levelCount;
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].numSlices =
              c.m_ImageView[viewid].range.layerCount;

          Convert(m_VulkanPipelineState.currentPass.framebuffer.attachments[i].swizzle,
                  c.m_ImageView[viewid].componentMapping);
        }
        else
        {
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].viewResourceId = ResourceId();
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].imageResourceId = ResourceId();

          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].firstMip = 0;
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].firstSlice = 0;
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].numMips = 1;
          m_VulkanPipelineState.currentPass.framebuffer.attachments[i].numSlices = 1;
        }
      }
    }
    else
    {
      m_VulkanPipelineState.currentPass.framebuffer.width = 0;
      m_VulkanPipelineState.currentPass.framebuffer.height = 0;
      m_VulkanPipelineState.currentPass.framebuffer.layers = 0;
    }

    m_VulkanPipelineState.currentPass.renderArea.x = state.renderArea.offset.x;
    m_VulkanPipelineState.currentPass.renderArea.y = state.renderArea.offset.y;
    m_VulkanPipelineState.currentPass.renderArea.width = state.renderArea.extent.width;
    m_VulkanPipelineState.currentPass.renderArea.height = state.renderArea.extent.height;
  }
  else
  {
    m_VulkanPipelineState.currentPass.renderpass.resourceId = ResourceId();
    m_VulkanPipelineState.currentPass.renderpass.subpass = 0;
    m_VulkanPipelineState.currentPass.renderpass.inputAttachments.clear();
    m_VulkanPipelineState.currentPass.renderpass.colorAttachments.clear();
    m_VulkanPipelineState.currentPass.renderpass.resolveAttachments.clear();
    m_VulkanPipelineState.currentPass.renderpass.depthstencilAttachment = -1;
    m_VulkanPipelineState.currentPass.renderpass.fragmentDensityAttachment = -1;

    m_VulkanPipelineState.currentPass.framebuffer.resourceId = ResourceId();
    m_VulkanPipelineState.currentPass.framebuffer.attachments.clear();
  }

  // Descriptor sets
  m_VulkanPipelineState.graphics.descriptorSets.resize(state.graphics.descSets.size());
  m_VulkanPipelineState.compute.descriptorSets.resize(state.compute.descSets.size());

  {
    rdcarray<VKPipe::DescriptorSet> *dsts[] = {
        &m_VulkanPipelineState.graphics.descriptorSets, &m_VulkanPipelineState.compute.descriptorSets,
    };

    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> *srcs[] = {
        &state.graphics.descSets, &state.compute.descSets,
    };

    for(size_t p = 0; p < ARRAY_COUNT(srcs); p++)
    {
      bool hasUsedBinds = false;
      const BindpointIndex *usedBindsData = NULL;
      size_t usedBindsSize = 0;

      {
        const DynamicUsedBinds &usage = m_BindlessFeedback.Usage[eventId];
        bool curCompute = (p == 1);
        if(usage.valid && usage.compute == curCompute)
        {
          hasUsedBinds = true;
          usedBindsData = usage.used.data();
          usedBindsSize = usage.used.size();
        }

        const DrawcallDescription *drawcall = m_pDriver->GetDrawcall(eventId);
        if(drawcall)
        {
          bool isDispatch = bool(drawcall->flags & DrawFlags::Dispatch);

          // ifor compute stage on draws, and non-compute stages on dispatches, pretend all
          // resources are dynamically unused, to prevent the lack of data from causing large arrays
          // to be force-expanded
          if((curCompute && !isDispatch) || (!curCompute && isDispatch))
          {
            hasUsedBinds = true;
            usedBindsData = NULL;
            usedBindsSize = 0;
          }
        }
      }

      BindpointIndex curBind;

      for(size_t i = 0; i < srcs[p]->size(); i++)
      {
        ResourceId src = (*srcs[p])[i].descSet;
        VKPipe::DescriptorSet &dst = (*dsts[p])[i];

        curBind.bindset = (uint32_t)i;

        ResourceId layoutId = m_pDriver->m_DescriptorSetState[src].layout;

        // push descriptors don't have a real descriptor set backing them
        if(c.m_DescSetLayout[layoutId].flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
        {
          dst.descriptorSetResourceId = ResourceId();
          dst.pushDescriptor = true;
        }
        else
        {
          dst.descriptorSetResourceId = rm->GetOriginalID(src);
          dst.pushDescriptor = false;
        }

        dst.layoutResourceId = rm->GetOriginalID(layoutId);
        dst.bindings.resize(m_pDriver->m_DescriptorSetState[src].data.binds.size());
        for(size_t b = 0; b < m_pDriver->m_DescriptorSetState[src].data.binds.size(); b++)
        {
          DescriptorSetSlot *info = m_pDriver->m_DescriptorSetState[src].data.binds[b];
          const DescSetLayout::Binding &layoutBind = c.m_DescSetLayout[layoutId].bindings[b];

          curBind.bind = (uint32_t)b;

          bool dynamicOffset = false;

          uint32_t descriptorCount = layoutBind.descriptorCount;

          if(layoutBind.variableSize)
            descriptorCount = m_pDriver->m_DescriptorSetState[src].data.variableDescriptorCount;

          dst.bindings[b].descriptorCount = descriptorCount;

          dst.bindings[b].stageFlags = (ShaderStageMask)layoutBind.stageFlags;
          switch(layoutBind.descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER: dst.bindings[b].type = BindType::Sampler; break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
              dst.bindings[b].type = BindType::ImageSampler;
              break;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
              dst.bindings[b].type = BindType::ReadOnlyImage;
              break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
              dst.bindings[b].type = BindType::ReadWriteImage;
              break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
              dst.bindings[b].type = BindType::ReadOnlyTBuffer;
              break;
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
              dst.bindings[b].type = BindType::ReadWriteTBuffer;
              break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
              dst.bindings[b].type = BindType::ConstantBuffer;
              break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
              dst.bindings[b].type = BindType::ReadWriteBuffer;
              break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
              dst.bindings[b].type = BindType::ConstantBuffer;
              dynamicOffset = true;
              break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
              dst.bindings[b].type = BindType::ReadWriteBuffer;
              dynamicOffset = true;
              break;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
              dst.bindings[b].type = BindType::InputAttachment;
              break;
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
              dst.bindings[b].descriptorCount = 1;
              dst.bindings[b].type = BindType::ConstantBuffer;
              break;
            case VK_DESCRIPTOR_TYPE_MAX_ENUM: dst.bindings[b].type = BindType::Unknown; break;
            default: dst.bindings[b].type = BindType::Unknown; RDCERR("Unexpected descriptor type");
          }

          dst.bindings[b].firstUsedIndex = -1;
          dst.bindings[b].lastUsedIndex = -1;
          dst.bindings[b].dynamicallyUsedCount = 0;

          dst.bindings[b].binds.resize(dst.bindings[b].descriptorCount);
          for(uint32_t a = 0; a < dst.bindings[b].descriptorCount; a++)
          {
            VKPipe::BindingElement &dstel = dst.bindings[b].binds[a];

            // clear it so we don't have to manually reset all elements back to normal
            memset(&dstel, 0, sizeof(dstel));

            curBind.arrayIndex = a;

            // if we have a list of used binds, and this is an array descriptor (so would be
            // expected to be in the list), check it for dynamic usage.
            if(dst.bindings[b].descriptorCount > 1 && hasUsedBinds)
            {
              // if we exhausted the list, all other elements are unused
              if(usedBindsSize == 0)
              {
                dstel.dynamicallyUsed = false;
              }
              else
              {
                // we never saw the current value of usedBindsData (which is odd, we should have
                // when iterating over all descriptors. This could only happen if there's some
                // layout mismatch or a feedback bug that lead to an invalid entry in the list).
                // Keep advancing until we get to one that is >= our current bind
                while(curBind > *usedBindsData && usedBindsSize)
                {
                  usedBindsData++;
                  usedBindsSize--;
                }

                // the next used bind is equal to this one. Mark it as dynamically used, and consume
                if(usedBindsSize > 0 && curBind == *usedBindsData)
                {
                  dstel.dynamicallyUsed = true;
                  usedBindsData++;
                  usedBindsSize--;
                }
                // the next used bind is after the current one, this is not used.
                else if(usedBindsSize > 0 && curBind < *usedBindsData)
                {
                  dstel.dynamicallyUsed = false;
                }
              }
            }
            else
            {
              dstel.dynamicallyUsed = true;
            }

            if(dstel.dynamicallyUsed)
            {
              dst.bindings[b].dynamicallyUsedCount++;
              // we iterate in forward order, so we can unconditinoally set the last bind to the
              // current one, and only set the first bind if we haven't encountered one before
              dst.bindings[b].lastUsedIndex = a;

              if(dst.bindings[b].firstUsedIndex < 0)
                dst.bindings[b].firstUsedIndex = a;
            }

            // first handle the sampler separately because it might be in a combined descriptor
            if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
               layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            {
              if(layoutBind.immutableSampler)
              {
                dst.bindings[b].binds[a].samplerResourceId = layoutBind.immutableSampler[a];
                dst.bindings[b].binds[a].immutableSampler = true;
              }
              else if(info[a].imageInfo.sampler != ResourceId())
              {
                dst.bindings[b].binds[a].samplerResourceId = info[a].imageInfo.sampler;
              }

              if(dst.bindings[b].binds[a].samplerResourceId != ResourceId())
              {
                VKPipe::BindingElement &el = dst.bindings[b].binds[a];
                const VulkanCreationInfo::Sampler &sampl = c.m_Sampler[el.samplerResourceId];

                ResourceId liveId = el.samplerResourceId;

                el.samplerResourceId = rm->GetOriginalID(el.samplerResourceId);

                // sampler info
                el.filter = MakeFilter(sampl.minFilter, sampl.magFilter, sampl.mipmapMode,
                                       sampl.maxAnisotropy >= 1.0f, sampl.compareEnable,
                                       sampl.reductionMode);
                el.addressU = MakeAddressMode(sampl.address[0]);
                el.addressV = MakeAddressMode(sampl.address[1]);
                el.addressW = MakeAddressMode(sampl.address[2]);
                el.mipBias = sampl.mipLodBias;
                el.maxAnisotropy = sampl.maxAnisotropy;
                el.compareFunction = MakeCompareFunc(sampl.compareOp);
                el.minLOD = sampl.minLod;
                el.maxLOD = sampl.maxLod;
                MakeBorderColor(sampl.borderColor, (FloatVector *)el.borderColor);
                el.unnormalized = sampl.unnormalizedCoordinates;

                if(sampl.ycbcr != ResourceId())
                {
                  const VulkanCreationInfo::YCbCrSampler &ycbcr = c.m_YCbCrSampler[sampl.ycbcr];
                  el.ycbcrSampler = rm->GetOriginalID(sampl.ycbcr);

                  el.ycbcrModel = ycbcr.ycbcrModel;
                  el.ycbcrRange = ycbcr.ycbcrRange;
                  Convert(el.ycbcrSwizzle, ycbcr.componentMapping);
                  el.xChromaOffset = ycbcr.xChromaOffset;
                  el.yChromaOffset = ycbcr.yChromaOffset;
                  el.chromaFilter = ycbcr.chromaFilter;
                  el.forceExplicitReconstruction = ycbcr.forceExplicitReconstruction;
                }

                if(sampl.customBorder)
                {
                  if(sampl.borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT)
                  {
                    for(int bord = 0; bord < 4; bord++)
                      el.borderColor[bord] = float(sampl.customBorderColor.int32[bord]);
                  }
                  else
                  {
                    memcpy(el.borderColor, sampl.customBorderColor.float32, sizeof(Vec4f));
                  }
                }
              }
            }

            // now look at the 'base' type. Sampler is excluded from these ifs
            if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
               layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
               layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ||
               layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            {
              ResourceId viewid = info[a].imageInfo.imageView;

              if(viewid != ResourceId())
              {
                dst.bindings[b].binds[a].viewResourceId = rm->GetOriginalID(viewid);
                dst.bindings[b].binds[a].resourceResourceId =
                    rm->GetOriginalID(c.m_ImageView[viewid].image);
                dst.bindings[b].binds[a].viewFormat =
                    MakeResourceFormat(c.m_ImageView[viewid].format);

                Convert(dst.bindings[b].binds[a].swizzle, c.m_ImageView[viewid].componentMapping);
                dst.bindings[b].binds[a].firstMip = c.m_ImageView[viewid].range.baseMipLevel;
                dst.bindings[b].binds[a].firstSlice = c.m_ImageView[viewid].range.baseArrayLayer;
                dst.bindings[b].binds[a].numMips = c.m_ImageView[viewid].range.levelCount;
                dst.bindings[b].binds[a].numSlices = c.m_ImageView[viewid].range.layerCount;

                // temporary hack, store image layout enum in byteOffset as it's not used for images
                dst.bindings[b].binds[a].byteOffset = info[a].imageInfo.imageLayout;
              }
              else
              {
                dst.bindings[b].binds[a].viewResourceId = ResourceId();
                dst.bindings[b].binds[a].resourceResourceId = ResourceId();
                dst.bindings[b].binds[a].firstMip = 0;
                dst.bindings[b].binds[a].firstSlice = 0;
                dst.bindings[b].binds[a].numMips = 1;
                dst.bindings[b].binds[a].numSlices = 1;
              }
            }
            else if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                    layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
            {
              ResourceId viewid = info[a].texelBufferView;

              if(viewid != ResourceId())
              {
                dst.bindings[b].binds[a].viewResourceId = rm->GetOriginalID(viewid);
                dst.bindings[b].binds[a].resourceResourceId =
                    rm->GetOriginalID(c.m_BufferView[viewid].buffer);
                dst.bindings[b].binds[a].byteOffset = c.m_BufferView[viewid].offset;
                dst.bindings[b].binds[a].viewFormat =
                    MakeResourceFormat(c.m_BufferView[viewid].format);
                if(dynamicOffset)
                {
                  union
                  {
                    VkImageLayout l;
                    uint32_t u;
                  } offs;

                  RDCCOMPILE_ASSERT(sizeof(VkImageLayout) == sizeof(uint32_t),
                                    "VkImageLayout isn't 32-bit sized");

                  offs.l = info[a].imageInfo.imageLayout;

                  dst.bindings[b].binds[a].byteOffset += offs.u;
                }
                dst.bindings[b].binds[a].byteSize = c.m_BufferView[viewid].size;
              }
              else
              {
                dst.bindings[b].binds[a].viewResourceId = ResourceId();
                dst.bindings[b].binds[a].resourceResourceId = ResourceId();
                dst.bindings[b].binds[a].byteOffset = 0;
                dst.bindings[b].binds[a].byteSize = 0;
              }
            }
            else if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
            {
              dst.bindings[b].binds[a].viewResourceId = ResourceId();
              dst.bindings[b].binds[a].resourceResourceId = ResourceId();
              dst.bindings[b].binds[a].inlineBlock = true;
              dst.bindings[b].binds[a].byteOffset = 0;
              dst.bindings[b].binds[a].byteSize = descriptorCount;
            }
            else if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                    layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
                    layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                    layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
              dst.bindings[b].binds[a].viewResourceId = ResourceId();

              if(info[a].bufferInfo.buffer != ResourceId())
                dst.bindings[b].binds[a].resourceResourceId =
                    rm->GetOriginalID(info[a].bufferInfo.buffer);

              dst.bindings[b].binds[a].byteOffset = info[a].bufferInfo.offset;
              if(dynamicOffset)
              {
                union
                {
                  VkImageLayout l;
                  uint32_t u;
                } offs;

                RDCCOMPILE_ASSERT(sizeof(VkImageLayout) == sizeof(uint32_t),
                                  "VkImageLayout isn't 32-bit sized");

                offs.l = info[a].imageInfo.imageLayout;

                dst.bindings[b].binds[a].byteOffset += offs.u;
              }

              dst.bindings[b].binds[a].byteSize = info[a].bufferInfo.range;
            }
          }

          // if no bindings were set these will still be negative. Set them to something sensible.
          if(dst.bindings[b].firstUsedIndex < 0)
          {
            dst.bindings[b].firstUsedIndex = 0;
            dst.bindings[b].lastUsedIndex = 0x7fffffff;
          }
        }
      }
    }
  }

  // image layouts
  {
    size_t i = 0;
    m_VulkanPipelineState.images.resize(m_pDriver->m_ImageStates.size());
    for(auto it = m_pDriver->m_ImageStates.begin(); it != m_pDriver->m_ImageStates.end(); ++it)
    {
      VKPipe::ImageData &img = m_VulkanPipelineState.images[i];

      if(rm->GetOriginalID(it->first) == it->first)
        continue;

      img.resourceId = rm->GetOriginalID(it->first);

      LockedConstImageStateRef imState = it->second.LockRead();
      img.layouts.resize(imState->subresourceStates.size());
      auto subIt = imState->subresourceStates.begin();
      for(size_t l = 0; l < img.layouts.size(); ++l, ++subIt)
      {
        img.layouts[l].name = ToStr(subIt->state().newLayout);
        img.layouts[l].baseMip = subIt->range().baseMipLevel;
        img.layouts[l].numMip = subIt->range().levelCount;
        img.layouts[l].baseLayer = subIt->range().baseArrayLayer;
        img.layouts[l].numLayer = subIt->range().layerCount;
      }

      if(img.layouts.empty())
      {
        img.layouts.push_back(VKPipe::ImageLayout());
        img.layouts[0].name = "Unknown";
      }

      i++;
    }

    m_VulkanPipelineState.images.resize(i);
  }

  if(state.conditionalRendering.buffer != ResourceId())
  {
    m_VulkanPipelineState.conditionalRendering.bufferId =
        rm->GetOriginalID(state.conditionalRendering.buffer);
    m_VulkanPipelineState.conditionalRendering.byteOffset = state.conditionalRendering.offset;
    m_VulkanPipelineState.conditionalRendering.isInverted =
        state.conditionalRendering.flags == VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;

    bytebuf data;
    GetBufferData(state.conditionalRendering.buffer, state.conditionalRendering.offset,
                  sizeof(uint32_t), data);

    uint32_t value;
    memcpy(&value, data.data(), sizeof(uint32_t));

    m_VulkanPipelineState.conditionalRendering.isPassing = value != 0;

    if(m_VulkanPipelineState.conditionalRendering.isInverted)
      m_VulkanPipelineState.conditionalRendering.isPassing =
          !m_VulkanPipelineState.conditionalRendering.isPassing;
  }
}

void VulkanReplay::FillCBufferVariables(ResourceId pipeline, ResourceId shader, rdcstr entryPoint,
                                        uint32_t cbufSlot, rdcarray<ShaderVariable> &outvars,
                                        const bytebuf &data)
{
  auto it = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);

  if(it == m_pDriver->m_CreationInfo.m_ShaderModule.end())
  {
    RDCERR("Can't get shader details");
    return;
  }

  ShaderReflection &refl = it->second.GetReflection(entryPoint, pipeline).refl;
  ShaderBindpointMapping &mapping = it->second.GetReflection(entryPoint, pipeline).mapping;

  if(cbufSlot >= (uint32_t)refl.constantBlocks.count())
  {
    RDCERR("Invalid cbuffer slot");
    return;
  }

  ConstantBlock &c = refl.constantBlocks[cbufSlot];

  if(c.bufferBacked)
  {
    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descSets =
        (refl.stage == ShaderStage::Compute) ? m_pDriver->m_RenderState.compute.descSets
                                             : m_pDriver->m_RenderState.graphics.descSets;

    Bindpoint bind = mapping.constantBlocks[c.bindPoint];

    if(bind.bindset < descSets.count())
    {
      ResourceId set = descSets[bind.bindset].descSet;

      const WrappedVulkan::DescriptorSetInfo &setData = m_pDriver->m_DescriptorSetState[set];

      ResourceId layoutId = setData.layout;

      if(bind.bind < m_pDriver->m_CreationInfo.m_DescSetLayout[layoutId].bindings.count())
      {
        const DescSetLayout::Binding &layoutBind =
            m_pDriver->m_CreationInfo.m_DescSetLayout[layoutId].bindings[bind.bind];

        if(layoutBind.descriptorType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
        {
          bytebuf inlineData;
          inlineData.assign(
              setData.data.inlineBytes.data() + setData.data.binds[bind.bind]->inlineOffset,
              layoutBind.variableSize ? setData.data.variableDescriptorCount
                                      : layoutBind.descriptorCount);
          StandardFillCBufferVariables(refl.resourceId, c.variables, outvars, inlineData);
          return;
        }
      }
    }

    StandardFillCBufferVariables(refl.resourceId, c.variables, outvars, data);
  }
  else
  {
    // specialised path to display specialization constants
    if(mapping.constantBlocks[c.bindPoint].bindset == SpecializationConstantBindSet)
    {
      auto pipeIt = m_pDriver->m_CreationInfo.m_Pipeline.find(pipeline);

      if(pipeIt != m_pDriver->m_CreationInfo.m_Pipeline.end())
      {
        auto specInfo =
            pipeIt->second.shaders[it->second.GetReflection(entryPoint, pipeline).stageIndex].specialization;

        FillSpecConstantVariables(refl.resourceId, c.variables, outvars, specInfo);
      }
    }
    else
    {
      bytebuf pushdata;
      pushdata.resize(sizeof(m_pDriver->m_RenderState.pushconsts));
      memcpy(&pushdata[0], m_pDriver->m_RenderState.pushconsts, pushdata.size());
      StandardFillCBufferVariables(refl.resourceId, c.variables, outvars, pushdata);
    }
  }
}

void VulkanReplay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, const Subresource &sub,
                             CompType typeCast, float pixel[4])
{
  int oldW = m_DebugWidth, oldH = m_DebugHeight;

  m_DebugWidth = m_DebugHeight = 1;

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texture];
  LockedConstImageStateRef imageState = m_pDriver->FindConstImageState(texture);
  if(!imageState)
  {
    RDCWARN("Could not find image info for image %s", ToStr(texture).c_str());
    return;
  }
  if(!imageState->isMemoryBound)
    return;

  bool isStencil = IsStencilFormat(iminfo.format);

  // do a second pass to render the stencil, if needed
  for(int pass = 0; pass < (isStencil ? 2 : 1); pass++)
  {
    // render picked pixel to readback F32 RGBA texture
    {
      TextureDisplay texDisplay;

      texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
      texDisplay.hdrMultiplier = -1.0f;
      texDisplay.linearDisplayAsGamma = true;
      texDisplay.flipY = false;
      texDisplay.subresource = sub;
      texDisplay.customShaderId = ResourceId();
      texDisplay.overlay = DebugOverlay::NoOverlay;
      texDisplay.rangeMin = 0.0f;
      texDisplay.rangeMax = 1.0f;
      texDisplay.scale = 1.0f;
      texDisplay.resourceId = texture;
      texDisplay.typeCast = typeCast;
      texDisplay.rawOutput = true;
      texDisplay.xOffset = -float(x << sub.mip);
      texDisplay.yOffset = -float(y << sub.mip);

      // only render green (stencil) in second pass
      if(pass == 1)
      {
        texDisplay.green = true;
        texDisplay.red = texDisplay.blue = texDisplay.alpha = false;
      }

      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          Unwrap(m_PixelPick.RP),
          Unwrap(m_PixelPick.FB),
          {{
               0, 0,
           },
           {1, 1}},
          1,
          &clearval,
      };

      RenderTextureInternal(texDisplay, *imageState, rpbegin,
                            eTexDisplay_32Render | eTexDisplay_MipShift);
    }

    VkDevice dev = m_pDriver->GetDev();
    VkCommandBuffer cmd = m_pDriver->GetNextCmd();
    const VkDevDispatchTable *vt = ObjDisp(dev);

    VkResult vkr = VK_SUCCESS;

    {
      VkImageMemoryBarrier pickimBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            NULL,
                                            0,
                                            0,
                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            VK_QUEUE_FAMILY_IGNORED,
                                            VK_QUEUE_FAMILY_IGNORED,
                                            Unwrap(m_PixelPick.Image),
                                            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      // update image layout from color attachment to transfer source, with proper memory barriers
      pickimBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      pickimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

      VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      DoPipelineBarrier(cmd, 1, &pickimBarrier);
      pickimBarrier.oldLayout = pickimBarrier.newLayout;
      pickimBarrier.srcAccessMask = pickimBarrier.dstAccessMask;

      // do copy
      VkBufferImageCopy region = {
          0, 128, 1, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {1, 1, 1},
      };
      vt->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(m_PixelPick.Image),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               Unwrap(m_PixelPick.ReadbackBuffer.buf), 1, &region);

      // update image layout back to color attachment
      pickimBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      pickimBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      DoPipelineBarrier(cmd, 1, &pickimBarrier);

      vt->EndCommandBuffer(Unwrap(cmd));
    }

    // submit cmds and wait for idle so we can readback
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();

    float *pData = NULL;
    vt->MapMemory(Unwrap(dev), Unwrap(m_PixelPick.ReadbackBuffer.mem), 0, VK_WHOLE_SIZE, 0,
                  (void **)&pData);

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        NULL,
        Unwrap(m_PixelPick.ReadbackBuffer.mem),
        0,
        VK_WHOLE_SIZE,
    };

    vkr = vt->InvalidateMappedMemoryRanges(Unwrap(dev), 1, &range);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    RDCASSERT(pData != NULL);

    if(pData == NULL)
    {
      RDCERR("Failed ot map readback buffer memory");
    }
    else
    {
      // only write stencil to .y
      if(pass == 1)
      {
        pixel[1] = ((uint32_t *)pData)[0] / 255.0f;
      }
      else
      {
        pixel[0] = pData[0];
        pixel[1] = pData[1];
        pixel[2] = pData[2];
        pixel[3] = pData[3];
      }
    }

    vt->UnmapMemory(Unwrap(dev), Unwrap(m_PixelPick.ReadbackBuffer.mem));
  }

  m_DebugWidth = oldW;
  m_DebugHeight = oldH;
}

bool VulkanReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                             float *minval, float *maxval)
{
  const ImageInfo *imageInfo = NULL;
  {
    LockedConstImageStateRef state = m_pDriver->FindConstImageState(texid);
    if(!state)
      return false;
    imageInfo = &state->GetImageInfo();
  }

  if(IsDepthAndStencilFormat(imageInfo->format))
  {
    // for depth/stencil we need to run the code twice - once to fetch depth and once to fetch
    // stencil - since we can't process float depth and int stencil at the same time
    Vec4f depth[2] = {
        {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
    };
    Vec4u stencil[2] = {{0, 0, 0, 0}, {1, 1, 1, 1}};

    bool success = GetMinMax(texid, sub, typeCast, false, &depth[0].x, &depth[1].x);

    if(!success)
      return false;

    success = GetMinMax(texid, sub, typeCast, true, (float *)&stencil[0].x, (float *)&stencil[1].x);

    if(!success)
      return false;

    // copy across into green channel, casting up to float, dividing by the range for this texture
    depth[0].y = float(stencil[0].x) / 255.0f;
    depth[1].y = float(stencil[1].x) / 255.0f;

    memcpy(minval, &depth[0], sizeof(depth[0]));
    memcpy(maxval, &depth[1], sizeof(depth[1]));

    return true;
  }

  return GetMinMax(texid, sub, typeCast, false, minval, maxval);
}

bool VulkanReplay::GetMinMax(ResourceId texid, const Subresource &sub, CompType typeCast,
                             bool stencil, float *minval, float *maxval)
{
  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  LockedConstImageStateRef state = m_pDriver->FindConstImageState(texid);
  if(!state)
    return false;
  bool isMemoryBound = state->isMemoryBound;
  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
  TextureDisplayViews &texviews = m_TexRender.TextureViews[texid];
  VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(texid);

  if(!isMemoryBound)
    return false;

  if(!IsStencilFormat(iminfo.format))
    stencil = false;

  CreateTexImageView(liveIm, iminfo, typeCast, texviews);

  VkImageView liveImView = texviews.views[0];

  // if it's not stencil-only and we're displaying stencil, use view 1
  if(texviews.castedFormat != VK_FORMAT_S8_UINT && stencil)
    liveImView = texviews.views[1];

  RDCASSERT(liveImView != VK_NULL_HANDLE);

  VkDescriptorImageInfo imdesc = {0};
  imdesc.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imdesc.imageView = Unwrap(liveImView);
  imdesc.sampler = Unwrap(m_General.PointSampler);

  uint32_t descSetBinding = 0;
  uint32_t intTypeIndex = 0;

  if(IsUIntFormat(texviews.castedFormat))
  {
    descSetBinding = 10;
    intTypeIndex = 1;
  }
  else if(IsSIntFormat(texviews.castedFormat))
  {
    descSetBinding = 15;
    intTypeIndex = 2;
  }
  else
  {
    descSetBinding = 5;
  }

  int textype = 0;

  if(iminfo.type == VK_IMAGE_TYPE_1D)
  {
    textype = RESTYPE_TEX1D;
  }
  else if(iminfo.type == VK_IMAGE_TYPE_3D)
  {
    textype = RESTYPE_TEX3D;
  }
  else if(iminfo.type == VK_IMAGE_TYPE_2D)
  {
    textype = RESTYPE_TEX2D;
    if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
      textype = RESTYPE_TEX2DMS;
  }

  if(stencil)
  {
    descSetBinding = 10;
    intTypeIndex = 1;
  }

  descSetBinding += textype;

  if(m_Histogram.m_MinMaxTilePipe[textype][intTypeIndex] == VK_NULL_HANDLE)
    return false;

  VkDescriptorBufferInfo bufdescs[3];
  RDCEraseEl(bufdescs);
  m_Histogram.m_MinMaxTileResult.FillDescriptor(bufdescs[0]);
  m_Histogram.m_MinMaxResult.FillDescriptor(bufdescs[1]);
  m_Histogram.m_HistogramUBO.FillDescriptor(bufdescs[2]);

  VkDescriptorImageInfo altimdesc[2] = {};
  for(uint32_t i = 1; i < GetYUVPlaneCount(texviews.castedFormat); i++)
  {
    RDCASSERT(texviews.views[i] != VK_NULL_HANDLE);
    altimdesc[i - 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    altimdesc[i - 1].imageView = Unwrap(texviews.views[i]);
    altimdesc[i - 1].sampler = Unwrap(m_General.PointSampler);
  }

  VkWriteDescriptorSet writeSet[] = {

      // first pass on tiles
      {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]),
          0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufdescs[0],
          NULL    // destination = tile result
      },
      {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]),
          1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufdescs[0],
          NULL    // source = unused, bind tile result
      },
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]), 2,
       0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufdescs[2], NULL},

      // sampled view
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]),
       descSetBinding, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imdesc, NULL, NULL},
      // YUV secondary planes (if needed)
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]), 10,
       0, GetYUVPlaneCount(texviews.castedFormat) - 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       altimdesc, NULL, NULL},

      // second pass from tiles to result
      {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[1]),
          0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufdescs[1],
          NULL    // destination = result
      },
      {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[1]),
          1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufdescs[0],
          NULL    // source = tile result
      },
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[1]), 2,
       0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufdescs[2], NULL},
  };

  rdcarray<VkWriteDescriptorSet> writeSets;
  for(size_t i = 0; i < ARRAY_COUNT(writeSet); i++)
  {
    if(writeSet[i].descriptorCount > 0)
      writeSets.push_back(writeSet[i]);
  }

  for(size_t i = 0; i < ARRAY_COUNT(m_TexRender.DummyWrites); i++)
  {
    VkWriteDescriptorSet &write = m_TexRender.DummyWrites[i];

    // don't write dummy data in the actual slot
    if(write.dstBinding == descSetBinding)
      continue;

    // don't overwrite YUV texture slots if it's a YUV planar format
    if(write.dstBinding == 10)
    {
      if(write.dstArrayElement == 0 && GetYUVPlaneCount(texviews.castedFormat) >= 2)
        continue;
      if(write.dstArrayElement == 1 && GetYUVPlaneCount(texviews.castedFormat) >= 3)
        continue;
    }

    write.dstSet = Unwrap(m_Histogram.m_HistogramDescSet[0]);
    writeSets.push_back(write);
  }

  vt->UpdateDescriptorSets(Unwrap(dev), (uint32_t)writeSets.size(), &writeSets[0], 0, NULL);

  HistogramUBOData *data = (HistogramUBOData *)m_Histogram.m_HistogramUBO.Map(NULL);

  data->HistogramTextureResolution.x = (float)RDCMAX(uint32_t(iminfo.extent.width) >> sub.mip, 1U);
  data->HistogramTextureResolution.y = (float)RDCMAX(uint32_t(iminfo.extent.height) >> sub.mip, 1U);
  data->HistogramTextureResolution.z = (float)RDCMAX(uint32_t(iminfo.extent.depth) >> sub.mip, 1U);
  if(iminfo.type == VK_IMAGE_TYPE_3D)
    data->HistogramSlice =
        (float)RDCCLAMP(sub.slice, 0U, uint32_t(iminfo.extent.depth >> sub.mip) - 1) + 0.001f;
  else
    data->HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, iminfo.arrayLayers - 1) + 0.001f;
  data->HistogramMip = (int)sub.mip;
  data->HistogramNumSamples = iminfo.samples;
  data->HistogramSample = (int)RDCCLAMP(sub.sample, 0U, uint32_t(iminfo.samples) - 1);
  if(sub.sample == ~0U)
    data->HistogramSample = -iminfo.samples;
  data->HistogramMin = 0.0f;
  data->HistogramMax = 1.0f;
  data->HistogramChannels = 0xf;

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(texviews.castedFormat, YUVDownsampleRate, YUVAChannels);

  data->HistogramYUVDownsampleRate = YUVDownsampleRate;
  data->HistogramYUVAChannels = YUVAChannels;

  m_Histogram.m_HistogramUBO.Unmap();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ImageBarrierSequence setupBarriers, cleanupBarriers;
  state->TempTransition(m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_SHADER_READ_BIT, setupBarriers, cleanupBarriers,
                        m_pDriver->GetImageTransitionInfo());
  m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
  m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);

  int blocksX = (int)ceil(iminfo.extent.width / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY =
      (int)ceil(iminfo.extent.height / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                      Unwrap(m_Histogram.m_MinMaxTilePipe[textype][intTypeIndex]));
  vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                            Unwrap(m_Histogram.m_HistogramPipeLayout), 0, 1,
                            UnwrapPtr(m_Histogram.m_HistogramDescSet[0]), 0, NULL);

  vt->CmdDispatch(Unwrap(cmd), blocksX, blocksY, 1);

  m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);
  if(!cleanupBarriers.empty())
  {
    vt->EndCommandBuffer(Unwrap(cmd));
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
    m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);
    cmd = m_pDriver->GetNextCmd();
    vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  }

  VkBufferMemoryBarrier tilebarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_SHADER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(m_Histogram.m_MinMaxTileResult.buf),
      0,
      m_Histogram.m_MinMaxTileResult.totalsize,
  };

  // ensure shader writes complete before coalescing the tiles
  DoPipelineBarrier(cmd, 1, &tilebarrier);

  vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                      Unwrap(m_Histogram.m_MinMaxResultPipe[intTypeIndex]));
  vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                            Unwrap(m_Histogram.m_HistogramPipeLayout), 0, 1,
                            UnwrapPtr(m_Histogram.m_HistogramDescSet[1]), 0, NULL);

  vt->CmdDispatch(Unwrap(cmd), 1, 1, 1);

  // ensure shader writes complete before copying back to readback buffer
  tilebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  tilebarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  tilebarrier.buffer = Unwrap(m_Histogram.m_MinMaxResult.buf);
  tilebarrier.size = m_Histogram.m_MinMaxResult.totalsize;

  DoPipelineBarrier(cmd, 1, &tilebarrier);

  VkBufferCopy bufcopy = {
      0, 0, m_Histogram.m_MinMaxResult.totalsize,
  };

  vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_Histogram.m_MinMaxResult.buf),
                    Unwrap(m_Histogram.m_MinMaxReadback.buf), 1, &bufcopy);

  // wait for copy to complete before mapping
  tilebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  tilebarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  tilebarrier.buffer = Unwrap(m_Histogram.m_MinMaxReadback.buf);
  tilebarrier.size = m_Histogram.m_MinMaxResult.totalsize;

  DoPipelineBarrier(cmd, 1, &tilebarrier);

  vt->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  Vec4f *minmax = (Vec4f *)m_Histogram.m_MinMaxReadback.Map(NULL);

  minval[0] = minmax[0].x;
  minval[1] = minmax[0].y;
  minval[2] = minmax[0].z;
  minval[3] = minmax[0].w;

  maxval[0] = minmax[1].x;
  maxval[1] = minmax[1].y;
  maxval[2] = minmax[1].z;
  maxval[3] = minmax[1].w;

  m_Histogram.m_MinMaxReadback.Unmap();

  return true;
}

bool VulkanReplay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                                float minval, float maxval, bool channels[4],
                                rdcarray<uint32_t> &histogram)
{
  if(minval >= maxval)
    return false;

  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  LockedConstImageStateRef state = m_pDriver->FindConstImageState(texid);
  if(!state->isMemoryBound)
    return false;
  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];
  TextureDisplayViews &texviews = m_TexRender.TextureViews[texid];
  VkImage liveIm = m_pDriver->GetResourceManager()->GetCurrentHandle<VkImage>(texid);

  bool stencil = false;
  // detect if stencil is selected
  if(IsStencilFormat(iminfo.format) && !channels[0] && channels[1] && !channels[2] && !channels[3])
    stencil = true;

  CreateTexImageView(liveIm, iminfo, typeCast, texviews);

  uint32_t descSetBinding = 0;
  uint32_t intTypeIndex = 0;

  if(IsUIntFormat(texviews.castedFormat))
  {
    descSetBinding = 10;
    intTypeIndex = 1;
  }
  else if(IsSIntFormat(texviews.castedFormat))
  {
    descSetBinding = 15;
    intTypeIndex = 2;
  }
  else
  {
    descSetBinding = 5;
  }

  int textype = 0;

  if(iminfo.type == VK_IMAGE_TYPE_1D)
  {
    textype = RESTYPE_TEX1D;
  }
  else if(iminfo.type == VK_IMAGE_TYPE_3D)
  {
    textype = RESTYPE_TEX3D;
  }
  else if(iminfo.type == VK_IMAGE_TYPE_2D)
  {
    textype = RESTYPE_TEX2D;
    if(iminfo.samples != VK_SAMPLE_COUNT_1_BIT)
      textype = RESTYPE_TEX2DMS;
  }

  if(stencil)
  {
    descSetBinding = 10;
    intTypeIndex = 1;

    // rescale the range so that stencil seems to fit to 0-1
    minval *= 255.0f;
    maxval *= 255.0f;

    // shuffle the channel selection, since stencil comes back in red
    std::swap(channels[0], channels[1]);
  }

  descSetBinding += textype;

  if(m_Histogram.m_HistogramPipe[textype][intTypeIndex] == VK_NULL_HANDLE)
  {
    histogram.resize(HGRAM_NUM_BUCKETS);
    for(size_t i = 0; i < HGRAM_NUM_BUCKETS; i++)
      histogram[i] = 1;
    return false;
  }

  VkImageView liveImView = texviews.views[0];

  // if it's not stencil-only and we're displaying stencil, use view 1
  if(stencil && texviews.castedFormat != VK_FORMAT_S8_UINT)
    liveImView = texviews.views[1];

  RDCASSERT(liveImView != VK_NULL_HANDLE);

  VkDescriptorImageInfo imdesc = {0};
  imdesc.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imdesc.imageView = Unwrap(liveImView);
  imdesc.sampler = Unwrap(m_General.PointSampler);

  VkDescriptorBufferInfo bufdescs[2];
  RDCEraseEl(bufdescs);
  m_Histogram.m_HistogramBuf.FillDescriptor(bufdescs[0]);
  m_Histogram.m_HistogramUBO.FillDescriptor(bufdescs[1]);

  VkDescriptorImageInfo altimdesc[2] = {};
  for(uint32_t i = 1; i < GetYUVPlaneCount(texviews.castedFormat); i++)
  {
    RDCASSERT(texviews.views[i] != VK_NULL_HANDLE);
    altimdesc[i - 1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    altimdesc[i - 1].imageView = Unwrap(texviews.views[i]);
    altimdesc[i - 1].sampler = Unwrap(m_General.PointSampler);
  }

  VkWriteDescriptorSet writeSet[] = {

      // histogram pass
      {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]),
          0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufdescs[0],
          NULL    // destination = histogram result
      },
      {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]),
          1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &bufdescs[0],
          NULL    // source = unused, bind histogram result
      },
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]), 2,
       0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &bufdescs[1], NULL},
      // sampled view
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]),
       descSetBinding, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imdesc, NULL, NULL},
      // YUV secondary planes (if needed)
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, Unwrap(m_Histogram.m_HistogramDescSet[0]), 10,
       0, GetYUVPlaneCount(texviews.castedFormat) - 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       altimdesc, NULL, NULL},
  };

  rdcarray<VkWriteDescriptorSet> writeSets;
  for(size_t i = 0; i < ARRAY_COUNT(writeSet); i++)
  {
    if(writeSet[i].descriptorCount > 0)
      writeSets.push_back(writeSet[i]);
  }

  for(size_t i = 0; i < ARRAY_COUNT(m_TexRender.DummyWrites); i++)
  {
    VkWriteDescriptorSet &write = m_TexRender.DummyWrites[i];

    // don't write dummy data in the actual slot
    if(write.dstBinding == descSetBinding)
      continue;

    // don't overwrite YUV texture slots if it's a YUV planar format
    if(write.dstBinding == 10)
    {
      if(write.dstArrayElement == 0 && GetYUVPlaneCount(texviews.castedFormat) >= 2)
        continue;
      if(write.dstArrayElement == 1 && GetYUVPlaneCount(texviews.castedFormat) >= 3)
        continue;
    }

    write.dstSet = Unwrap(m_Histogram.m_HistogramDescSet[0]);
    writeSets.push_back(write);
  }

  vt->UpdateDescriptorSets(Unwrap(dev), (uint32_t)writeSets.size(), &writeSets[0], 0, NULL);

  HistogramUBOData *data = (HistogramUBOData *)m_Histogram.m_HistogramUBO.Map(NULL);

  data->HistogramTextureResolution.x = (float)RDCMAX(uint32_t(iminfo.extent.width) >> sub.mip, 1U);
  data->HistogramTextureResolution.y = (float)RDCMAX(uint32_t(iminfo.extent.height) >> sub.mip, 1U);
  data->HistogramTextureResolution.z = (float)RDCMAX(uint32_t(iminfo.extent.depth) >> sub.mip, 1U);
  if(iminfo.type == VK_IMAGE_TYPE_3D)
    data->HistogramSlice =
        (float)RDCCLAMP(sub.slice, 0U, uint32_t(iminfo.extent.depth >> sub.mip) - 1) + 0.001f;
  else
    data->HistogramSlice = (float)RDCCLAMP(sub.slice, 0U, iminfo.arrayLayers - 1) + 0.001f;
  data->HistogramMip = (int)sub.mip;
  data->HistogramNumSamples = iminfo.samples;
  data->HistogramSample = (int)RDCCLAMP(sub.sample, 0U, uint32_t(iminfo.samples) - 1);
  if(sub.sample == ~0U)
    data->HistogramSample = -iminfo.samples;
  data->HistogramMin = minval;

  // The calculation in the shader normalises each value between min and max, then multiplies by the
  // number of buckets.
  // But any value equal to HistogramMax must go into NUM_BUCKETS-1, so add a small delta.
  data->HistogramMax = maxval + maxval * 1e-6f;

  uint32_t chans = 0;
  if(channels[0])
    chans |= 0x1;
  if(channels[1])
    chans |= 0x2;
  if(channels[2])
    chans |= 0x4;
  if(channels[3])
    chans |= 0x8;

  data->HistogramChannels = chans;
  data->HistogramFlags = 0;

  Vec4u YUVDownsampleRate = {};
  Vec4u YUVAChannels = {};

  GetYUVShaderParameters(texviews.castedFormat, YUVDownsampleRate, YUVAChannels);

  data->HistogramYUVDownsampleRate = YUVDownsampleRate;
  data->HistogramYUVAChannels = YUVAChannels;

  m_Histogram.m_HistogramUBO.Unmap();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();

  vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);

  ImageBarrierSequence setupBarriers, cleanupBarriers;
  state->TempTransition(m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_GENERAL,
                        VK_ACCESS_SHADER_READ_BIT, setupBarriers, cleanupBarriers,
                        m_pDriver->GetImageTransitionInfo());
  m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
  m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);

  int blocksX = (int)ceil(iminfo.extent.width / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  int blocksY =
      (int)ceil(iminfo.extent.height / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  vt->CmdFillBuffer(Unwrap(cmd), Unwrap(m_Histogram.m_HistogramBuf.buf), 0,
                    m_Histogram.m_HistogramBuf.totalsize, 0);

  vt->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                      Unwrap(m_Histogram.m_HistogramPipe[textype][intTypeIndex]));
  vt->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                            Unwrap(m_Histogram.m_HistogramPipeLayout), 0, 1,
                            UnwrapPtr(m_Histogram.m_HistogramDescSet[0]), 0, NULL);

  vt->CmdDispatch(Unwrap(cmd), blocksX, blocksY, 1);

  m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);
  if(!cleanupBarriers.empty())
  {
    vt->EndCommandBuffer(Unwrap(cmd));
    m_pDriver->SubmitCmds();
    m_pDriver->FlushQ();
    m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);
    cmd = m_pDriver->GetNextCmd();
    vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  }

  VkBufferMemoryBarrier tilebarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_SHADER_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      Unwrap(m_Histogram.m_HistogramBuf.buf),
      0,
      m_Histogram.m_HistogramBuf.totalsize,
  };

  // ensure shader writes complete before copying to readback buf
  DoPipelineBarrier(cmd, 1, &tilebarrier);

  VkBufferCopy bufcopy = {
      0, 0, m_Histogram.m_HistogramBuf.totalsize,
  };

  vt->CmdCopyBuffer(Unwrap(cmd), Unwrap(m_Histogram.m_HistogramBuf.buf),
                    Unwrap(m_Histogram.m_HistogramReadback.buf), 1, &bufcopy);

  // wait for copy to complete before mapping
  tilebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  tilebarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  tilebarrier.buffer = Unwrap(m_Histogram.m_HistogramReadback.buf);
  tilebarrier.size = m_Histogram.m_HistogramReadback.totalsize;

  DoPipelineBarrier(cmd, 1, &tilebarrier);

  vt->EndCommandBuffer(Unwrap(cmd));

  // submit cmds and wait for idle so we can readback
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  uint32_t *buckets = (uint32_t *)m_Histogram.m_HistogramReadback.Map(NULL);

  histogram.assign(buckets, HGRAM_NUM_BUCKETS);

  m_Histogram.m_HistogramReadback.Unmap();

  return true;
}

rdcarray<EventUsage> VulkanReplay::GetUsage(ResourceId id)
{
  return m_pDriver->GetUsage(id);
}

void VulkanReplay::CopyPixelForPixelHistory(VkCommandBuffer cmd, VkOffset2D offset, uint32_t sample,
                                            uint32_t bufferOffset, VkFormat format,
                                            VkDescriptorSet descSet)
{
  VkPipeline pipe;
  if(IsDepthOrStencilFormat(format))
    pipe = m_PixelHistory.MSCopyDepthPipe;
  else
    pipe = m_PixelHistory.MSCopyPipe;
  if(pipe == VK_NULL_HANDLE)
    return;
  if(!m_pDriver->GetDeviceEnabledFeatures().shaderStorageImageWriteWithoutFormat)
    return;

  ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(pipe));

  int32_t params[8] = {(int32_t)sample,
                       offset.x,
                       offset.y,
                       (int32_t)bufferOffset,
                       !IsStencilOnlyFormat(format),
                       IsStencilFormat(format),
                       0,
                       0};
  ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE,
                                      Unwrap(m_PixelHistory.MSCopyPipeLayout), 0, 1,
                                      UnwrapPtr(descSet), 0, NULL);

  ObjDisp(cmd)->CmdPushConstants(Unwrap(cmd), Unwrap(m_PixelHistory.MSCopyPipeLayout),
                                 VK_SHADER_STAGE_ALL, 0, 8 * 4, params);
  ObjDisp(cmd)->CmdDispatch(Unwrap(cmd), 1, 1, 1);
}

void VulkanReplay::GetTextureData(ResourceId tex, const Subresource &sub,
                                  const GetTextureDataParams &params, bytebuf &data)
{
  bool wasms = false;
  bool resolve = params.resolve;

  if(m_pDriver->m_CreationInfo.m_Image.find(tex) == m_pDriver->m_CreationInfo.m_Image.end())
  {
    RDCERR("Trying to get texture data for unknown ID %s!", ToStr(tex).c_str());
    return;
  }

  const VulkanCreationInfo::Image &imInfo = m_pDriver->m_CreationInfo.m_Image[tex];

  LockedConstImageStateRef lockedImage = m_pDriver->FindConstImageState(tex);
  if(!lockedImage || !lockedImage->isMemoryBound)
    return;
  const ImageState *srcImageState = &*lockedImage;
  ImageState tmpImageState;

  VkMarkerRegion region(StringFormat::Fmt("GetTextureData(%u, %u, %u, remap=%d)", sub.mip,
                                          sub.slice, sub.sample, params.remap));

  Subresource s = sub;

  s.slice = RDCMIN(uint32_t(imInfo.arrayLayers - 1), s.slice);
  s.sample = RDCMIN(uint32_t(imInfo.samples - 1), s.sample);
  s.mip = RDCMIN(uint32_t(imInfo.mipLevels - 1), s.mip);

  VkImageCreateInfo imCreateInfo = {
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      NULL,
      0,
      imInfo.type,
      imInfo.format,
      imInfo.extent,
      imInfo.mipLevels,
      imInfo.arrayLayers,
      imInfo.samples,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0,
      NULL,
      VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImageAspectFlags imageAspects = FormatImageAspects(imInfo.format);
  bool isDepth = (imageAspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
  bool isStencil = (imageAspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;

  VkImage liveWrappedImage = GetResourceManager()->GetCurrentHandle<VkImage>(tex);

  VkImage srcImage = Unwrap(liveWrappedImage);
  VkImage tmpImage = VK_NULL_HANDLE;
  VkImage wrappedTmpImage = VK_NULL_HANDLE;
  VkDeviceMemory tmpMemory = VK_NULL_HANDLE;

  VkFramebuffer *tmpFB = NULL;
  VkImageView *tmpView = NULL;
  uint32_t numFBs = 0;
  VkRenderPass tmpRP = VK_NULL_HANDLE;

  VkDevice dev = m_pDriver->GetDev();
  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(imInfo.samples > 1)
  {
    // make image n-array instead of n-samples
    imCreateInfo.arrayLayers *= imCreateInfo.samples;
    imCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    wasms = true;
  }

  if(wasms && (isDepth || isStencil))
    resolve = false;

  if(params.remap != RemapTexture::NoRemap)
  {
    int renderFlags = 0;

    // force readback texture to RGBA8 unorm
    if(params.remap == RemapTexture::RGBA8)
    {
      imCreateInfo.format =
          IsSRGBFormat(imCreateInfo.format) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    }
    else if(params.remap == RemapTexture::RGBA16)
    {
      imCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
      renderFlags = eTexDisplay_16Render;
    }
    else if(params.remap == RemapTexture::RGBA32)
    {
      imCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
      renderFlags = eTexDisplay_32Render;
    }
    else
    {
      RDCERR("Unsupported remap format: %u", params.remap);
    }

    imCreateInfo.format = GetViewCastedFormat(imCreateInfo.format, BaseRemapType(params.typeCast));

    if(IsUIntFormat(imCreateInfo.format))
      renderFlags |= eTexDisplay_RemapUInt;
    else if(IsSIntFormat(imCreateInfo.format))
      renderFlags |= eTexDisplay_RemapSInt;
    else
      renderFlags |= eTexDisplay_RemapFloat;

    // force to 1 array slice, 1 mip
    imCreateInfo.arrayLayers = 1;
    imCreateInfo.mipLevels = 1;
    // force to 2D
    imCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // we'll need to cast to remap the stencil part
    if(IsDepthAndStencilFormat(imInfo.format))
      imCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    imCreateInfo.extent.width = RDCMAX(1U, imCreateInfo.extent.width >> s.mip);
    imCreateInfo.extent.height = RDCMAX(1U, imCreateInfo.extent.height >> s.mip);
    imCreateInfo.extent.depth = RDCMAX(1U, imCreateInfo.extent.depth >> s.mip);

    // convert a 3D texture into a 2D array, so we can render to the slices without needing
    // KHR_maintenance1
    if(imCreateInfo.extent.depth > 1)
    {
      imCreateInfo.arrayLayers = imCreateInfo.extent.depth;
      imCreateInfo.extent.depth = 1;
    }

    // create render texture similar to readback texture
    vt->CreateImage(Unwrap(dev), &imCreateInfo, NULL, &tmpImage);
    wrappedTmpImage = tmpImage;
    GetResourceManager()->WrapResource(Unwrap(dev), wrappedTmpImage);
    tmpImageState = ImageState(wrappedTmpImage, ImageInfo(imCreateInfo), eFrameRef_None);

    VkMemoryRequirements mrq = {0};
    vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    tmpImageState.InlineTransition(
        cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());

    // end this command buffer, the rendertexture below will use its own and we want to ensure
    // ordering
    vt->EndCommandBuffer(Unwrap(cmd));

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    m_pDriver->SubmitCmds();
#endif

    // create framebuffer/render pass to render to
    VkAttachmentDescription attDesc = {0,
                                       imCreateInfo.format,
                                       VK_SAMPLE_COUNT_1_BIT,
                                       VK_ATTACHMENT_LOAD_OP_LOAD,
                                       VK_ATTACHMENT_STORE_OP_STORE,
                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                       VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference attRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {
        0,    VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,    NULL,       // inputs
        1,    &attRef,    // color
        NULL,             // resolve
        NULL,             // depth-stencil
        0,    NULL,       // preserve
    };

    VkRenderPassCreateInfo rpinfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        1,
        &attDesc,
        1,
        &subpass,
        0,
        NULL,    // dependencies
    };
    vt->CreateRenderPass(Unwrap(dev), &rpinfo, NULL, &tmpRP);

    numFBs = imCreateInfo.arrayLayers;

    // we'll need twice as many temp views/FBs for stencil views
    if(IsDepthAndStencilFormat(imInfo.format))
    {
      tmpFB = new VkFramebuffer[numFBs * 2];
      tmpView = new VkImageView[numFBs * 2];
    }
    else
    {
      tmpFB = new VkFramebuffer[numFBs];
      tmpView = new VkImageView[numFBs];
    }

    int oldW = m_DebugWidth, oldH = m_DebugHeight;

    m_DebugWidth = imCreateInfo.extent.width;
    m_DebugHeight = imCreateInfo.extent.height;

    int renderCount = 0;

    // if 3d texture, render each slice separately, otherwise render once
    for(uint32_t i = 0; i < numFBs; i++)
    {
      if(numFBs > 1 && (renderCount % m_TexRender.UBO.GetRingCount()) == 0)
      {
        m_pDriver->SubmitCmds();
        m_pDriver->FlushQ();
      }

      TextureDisplay texDisplay;

      texDisplay.red = texDisplay.green = texDisplay.blue = texDisplay.alpha = true;
      texDisplay.hdrMultiplier = -1.0f;
      texDisplay.linearDisplayAsGamma = false;
      texDisplay.overlay = DebugOverlay::NoOverlay;
      texDisplay.flipY = false;
      texDisplay.subresource.mip = s.mip;
      texDisplay.subresource.slice = imInfo.type == VK_IMAGE_TYPE_3D ? i : s.slice;
      texDisplay.subresource.sample =
          imInfo.type == VK_IMAGE_TYPE_3D ? 0 : (resolve ? ~0U : s.sample);
      texDisplay.customShaderId = ResourceId();
      texDisplay.rangeMin = params.blackPoint;
      texDisplay.rangeMax = params.whitePoint;
      texDisplay.scale = 1.0f;
      texDisplay.resourceId = tex;
      texDisplay.typeCast = params.typeCast;
      texDisplay.rawOutput = false;
      texDisplay.xOffset = 0;
      texDisplay.yOffset = 0;

      VkImageViewCreateInfo viewInfo = {
          VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          NULL,
          0,
          tmpImage,
          VK_IMAGE_VIEW_TYPE_2D,
          imCreateInfo.format,
          {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
          {
              VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, i, 1,
          },
      };

      vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, NULL, &tmpView[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkFramebufferCreateInfo fbinfo = {
          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          NULL,
          0,
          tmpRP,
          1,
          &tmpView[i],
          (uint32_t)imCreateInfo.extent.width,
          (uint32_t)imCreateInfo.extent.height,
          1,
      };

      vkr = vt->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &tmpFB[i]);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          tmpRP,
          tmpFB[i],
          {{
               0, 0,
           },
           {imCreateInfo.extent.width, imCreateInfo.extent.height}},
          1,
          &clearval,
      };

      RenderTextureInternal(texDisplay, *srcImageState, rpbegin, renderFlags);
      renderCount++;

      // for textures with stencil, do another draw to copy the stencil
      if(isStencil)
      {
        viewInfo.format = GetViewCastedFormat(viewInfo.format, CompType::UInt);

        vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, NULL, &tmpView[i + numFBs]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
        fbinfo.pAttachments = &tmpView[i + numFBs];
        vkr = vt->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &tmpFB[i + numFBs]);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
        rpbegin.framebuffer = tmpFB[i + numFBs];

        texDisplay.red = texDisplay.blue = texDisplay.alpha = false;
        RenderTextureInternal(texDisplay, *srcImageState, rpbegin,
                              (renderFlags & ~eTexDisplay_RemapFloat) | eTexDisplay_RemapUInt |
                                  eTexDisplay_GreenOnly);
        renderCount++;
      }
    }

    m_DebugWidth = oldW;
    m_DebugHeight = oldH;

    srcImage = tmpImage;
    srcImageState = &tmpImageState;

    // fetch a new command buffer for copy & readback
    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    tmpImageState.InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_ACCESS_TRANSFER_READ_BIT, m_pDriver->GetImageTransitionInfo());

    // these have already been selected, don't need to fetch that subresource
    // when copying back to readback buffer
    s.slice = 0;
    s.mip = 0;

    // no longer depth, if it was
    isDepth = false;
    isStencil = false;
  }
  else if(wasms && resolve)
  {
    // force to 1 array slice, 1 mip
    imCreateInfo.arrayLayers = 1;
    imCreateInfo.mipLevels = 1;

    imCreateInfo.extent.width = RDCMAX(1U, imCreateInfo.extent.width >> s.mip);
    imCreateInfo.extent.height = RDCMAX(1U, imCreateInfo.extent.height >> s.mip);

    // create resolve texture
    vt->CreateImage(Unwrap(dev), &imCreateInfo, NULL, &tmpImage);
    wrappedTmpImage = tmpImage;
    GetResourceManager()->WrapResource(Unwrap(dev), wrappedTmpImage);
    tmpImageState = ImageState(wrappedTmpImage, ImageInfo(imCreateInfo), eFrameRef_None);

    VkMemoryRequirements mrq = {0};
    vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    RDCASSERT(!isDepth && !isStencil);

    VkImageResolve resolveRegion = {
        {VK_IMAGE_ASPECT_COLOR_BIT, s.mip, s.slice, 1},
        {0, 0, 0},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        {0, 0, 0},
        imCreateInfo.extent,
    };

    tmpImageState.InlineTransition(
        cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
        VK_ACCESS_TRANSFER_WRITE_BIT, m_pDriver->GetImageTransitionInfo());
    ImageBarrierSequence setupBarriers, cleanupBarriers;
    srcImageState->TempTransition(m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_ACCESS_TRANSFER_READ_BIT, setupBarriers, cleanupBarriers,
                                  m_pDriver->GetImageTransitionInfo());
    m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
    m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);

    // resolve from live texture to resolve texture
    vt->CmdResolveImage(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tmpImage,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolveRegion);

    tmpImageState.InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_ACCESS_TRANSFER_READ_BIT, m_pDriver->GetImageTransitionInfo());

    m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);

    if(!cleanupBarriers.empty())
    {
      // ensure this resolve happens before handing back the source image to the original queue
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);

      // fetch a new command buffer for remaining work
      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
    srcImageState = &tmpImageState;

    srcImage = tmpImage;

    // these have already been selected, don't need to fetch that subresource
    // when copying back to readback buffer
    s.slice = 0;
    s.mip = 0;
  }
  else if(wasms)
  {
    // copy/expand multisampled live texture to array readback texture

    // multiply array layers by sample count
    uint32_t numSamples = (uint32_t)imInfo.samples;
    imCreateInfo.mipLevels = 1;
    imCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    if(IsDepthOrStencilFormat(imCreateInfo.format))
      imCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else
      imCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    // create resolve texture
    vt->CreateImage(Unwrap(dev), &imCreateInfo, NULL, &tmpImage);
    wrappedTmpImage = tmpImage;
    GetResourceManager()->WrapResource(Unwrap(dev), wrappedTmpImage);
    tmpImageState = ImageState(wrappedTmpImage, ImageInfo(imCreateInfo), eFrameRef_None);

    VkMemoryRequirements mrq = {0};
    vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    tmpImageState.InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_GENERAL, 0,
                                   VK_ACCESS_SHADER_WRITE_BIT, m_pDriver->GetImageTransitionInfo());
    ImageBarrierSequence setupBarriers, cleanupBarriers;
    srcImageState->TempTransition(m_pDriver->m_QueueFamilyIdx,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_ACCESS_SHADER_READ_BIT, setupBarriers, cleanupBarriers,
                                  m_pDriver->GetImageTransitionInfo());
    m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
    m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);

    vkr = vt->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // expand multisamples out to array
    GetDebugManager()->CopyTex2DMSToArray(tmpImage, srcImage, imCreateInfo.extent,
                                          imCreateInfo.arrayLayers / numSamples, numSamples,
                                          imCreateInfo.format);

    // fetch a new command buffer for copy & readback
    cmd = m_pDriver->GetNextCmd();

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    tmpImageState.InlineTransition(cmd, m_pDriver->m_QueueFamilyIdx,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
                                   VK_ACCESS_TRANSFER_READ_BIT, m_pDriver->GetImageTransitionInfo());

    m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);

    if(!cleanupBarriers.empty())
    {
      // ensure this resolve happens before handing back the source image to the original queue
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);

      // fetch a new command buffer for remaining work
      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    srcImage = tmpImage;
    srcImageState = &tmpImageState;
    s.slice = s.slice * numSamples + s.sample;
    s.sample = 0;
  }

  ImageBarrierSequence cleanupBarriers;

  // if we have no tmpImage, we're copying directly from the real image
  if(tmpImage == VK_NULL_HANDLE)
  {
    ImageBarrierSequence setupBarriers;
    srcImageState->TempTransition(m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_ACCESS_TRANSFER_READ_BIT, setupBarriers, cleanupBarriers,
                                  m_pDriver->GetImageTransitionInfo());
    m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
    m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);
  }

  VkImageAspectFlags copyAspects = VK_IMAGE_ASPECT_COLOR_BIT;

  if(isDepth)
    copyAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
  else if(isStencil)
    copyAspects = VK_IMAGE_ASPECT_STENCIL_BIT;

  VkBufferImageCopy copyregion[2] = {
      {
          0,
          0,
          0,
          {copyAspects, s.mip, s.slice, 1},
          {
              0, 0, 0,
          },
          imCreateInfo.extent,
      },
      // second region is only used for combined depth-stencil images
      {
          0,
          0,
          0,
          {VK_IMAGE_ASPECT_STENCIL_BIT, s.mip, s.slice, 1},
          {
              0, 0, 0,
          },
          imCreateInfo.extent,
      },
  };

  for(int i = 0; i < 2; i++)
  {
    copyregion[i].imageExtent.width = RDCMAX(1U, copyregion[i].imageExtent.width >> s.mip);
    copyregion[i].imageExtent.height = RDCMAX(1U, copyregion[i].imageExtent.height >> s.mip);
    copyregion[i].imageExtent.depth = RDCMAX(1U, copyregion[i].imageExtent.depth >> s.mip);
  }

  uint32_t dataSize = 0;

  // for most combined depth-stencil images this will be large enough for both to be copied
  // separately, but for D24S8 we need to add extra space since they won't be copied packed
  dataSize = GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth,
                         imCreateInfo.format, s.mip);

  if(imCreateInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
  {
    dataSize = AlignUp(dataSize, 4U);
    dataSize += GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth,
                            VK_FORMAT_S8_UINT, s.mip);
  }

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      dataSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  };

  VkBuffer readbackBuf = VK_NULL_HANDLE;
  vkr = vt->CreateBuffer(Unwrap(dev), &bufInfo, NULL, &readbackBuf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};

  vt->GetBufferMemoryRequirements(Unwrap(dev), readbackBuf, &mrq);

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, dataSize,
      m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
  };

  VkDeviceMemory readbackMem = VK_NULL_HANDLE;
  vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &readbackMem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = vt->BindBufferMemory(Unwrap(dev), readbackBuf, readbackMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  if(isDepth && isStencil)
  {
    copyregion[1].bufferOffset =
        GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth,
                    GetDepthOnlyFormat(imCreateInfo.format), s.mip);

    copyregion[1].bufferOffset = AlignUp(copyregion[1].bufferOffset, (VkDeviceSize)4);

    vt->CmdCopyImageToBuffer(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             readbackBuf, 2, copyregion);
  }
  else if(imInfo.type == VK_IMAGE_TYPE_3D && params.remap != RemapTexture::NoRemap)
  {
    // copy in each slice from the 2D array we created to render out the 3D texture
    for(uint32_t i = 0; i < imCreateInfo.arrayLayers; i++)
    {
      copyregion[0].imageSubresource.baseArrayLayer = i;
      copyregion[0].bufferOffset =
          i * GetByteSize(imCreateInfo.extent.width, imCreateInfo.extent.height, 1,
                          imCreateInfo.format, s.mip);
      vt->CmdCopyImageToBuffer(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readbackBuf, 1, copyregion);
    }
  }
  else
  {
    if(imInfo.type == VK_IMAGE_TYPE_3D)
      copyregion[0].imageSubresource.baseArrayLayer = 0;

    // copy from desired subresource in srcImage to buffer
    vt->CmdCopyImageToBuffer(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             readbackBuf, 1, copyregion);
  }

  // if we have no tmpImage, we're copying directly from the real image
  if(tmpImage == VK_NULL_HANDLE)
  {
    m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);

    if(!cleanupBarriers.empty())
    {
      // ensure this resolve happens before handing back the source image to the original queue
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);

      // fetch a new command buffer for remaining work
      cmd = m_pDriver->GetNextCmd();

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }
  }

  VkBufferMemoryBarrier bufBarrier = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      NULL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_HOST_READ_BIT,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      readbackBuf,
      0,
      dataSize,
  };

  // wait for copy to finish before reading back to host
  DoPipelineBarrier(cmd, 1, &bufBarrier);

  vt->EndCommandBuffer(Unwrap(cmd));

  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  // map the buffer and copy to return buffer
  byte *pData = NULL;
  vkr = vt->MapMemory(Unwrap(dev), readbackMem, 0, VK_WHOLE_SIZE, 0, (void **)&pData);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMappedMemoryRange range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, readbackMem, 0, VK_WHOLE_SIZE,
  };

  vkr = vt->InvalidateMappedMemoryRanges(Unwrap(dev), 1, &range);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  RDCASSERT(pData != NULL);

  data.resize(dataSize);

  if(params.remap == RemapTexture::RGBA32 && IsDepthAndStencilFormat(imInfo.format))
  {
    memcpy(data.data(), pData, dataSize);

    Vec4f *output = (Vec4f *)data.data();
    Vec4u *input = (Vec4u *)pData;
    for(size_t i = 0; i < dataSize / sizeof(Vec4u); i++)
      output[i].y = float(input[i].y) / 255.0f;
  }
  else if(isDepth && isStencil)
  {
    size_t pixelCount = std::max(1U, imCreateInfo.extent.width >> s.mip) *
                        std::max(1U, imCreateInfo.extent.height >> s.mip) *
                        std::max(1U, imCreateInfo.extent.depth >> s.mip);

    // for some reason reading direct from mapped memory here is *super* slow on android (1.5s to
    // iterate over the image), so we memcpy to a temporary buffer.
    rdcarray<byte> tmp;
    tmp.resize((size_t)copyregion[1].bufferOffset + pixelCount * sizeof(uint8_t));
    memcpy(tmp.data(), pData, tmp.size());

    if(imCreateInfo.format == VK_FORMAT_D16_UNORM_S8_UINT)
    {
      uint16_t *dSrc = (uint16_t *)tmp.data();
      uint8_t *sSrc = (uint8_t *)(tmp.data() + copyregion[1].bufferOffset);

      uint16_t *dDst = (uint16_t *)data.data();
      uint16_t *sDst = dDst + 1;    // interleaved, next pixel

      for(size_t i = 0; i < pixelCount; i++)
      {
        *dDst = *dSrc;
        *sDst = *sSrc;

        // increment source pointers by 1 since they're separate, and dest pointers by 2 since
        // they're interleaved
        dDst += 2;
        sDst += 2;

        sSrc++;
        dSrc++;
      }
    }
    else if(imCreateInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
    {
      // we can copy the depth from D24 as a 32-bit integer, since the remaining bits are garbage
      // and we overwrite them with stencil
      uint32_t *dSrc = (uint32_t *)tmp.data();
      uint8_t *sSrc = (uint8_t *)(tmp.data() + copyregion[1].bufferOffset);

      uint32_t *dst = (uint32_t *)data.data();

      for(size_t i = 0; i < pixelCount; i++)
      {
        // pack the data together again, stencil in top bits
        *dst = (*dSrc & 0x00ffffff) | (uint32_t(*sSrc) << 24);

        dst++;
        sSrc++;
        dSrc++;
      }
    }
    else
    {
      uint32_t *dSrc = (uint32_t *)tmp.data();
      uint8_t *sSrc = (uint8_t *)(tmp.data() + copyregion[1].bufferOffset);

      uint32_t *dDst = (uint32_t *)data.data();
      uint32_t *sDst = dDst + 1;    // interleaved, next pixel

      for(size_t i = 0; i < pixelCount; i++)
      {
        *dDst = *dSrc;
        *sDst = *sSrc;

        // increment source pointers by 1 since they're separate, and dest pointers by 2 since
        // they're interleaved
        dDst += 2;
        sDst += 2;

        sSrc++;
        dSrc++;
      }
    }
    // need to manually copy to interleave pixels
  }
  else
  {
    memcpy(data.data(), pData, dataSize);

    // vulkan's bitpacking of some layouts puts alpha in the low bits, which is not our 'standard'
    // layout and is not representable in our resource formats
    if(params.standardLayout)
    {
      if(imCreateInfo.format == VK_FORMAT_R4G4B4A4_UNORM_PACK16 ||
         imCreateInfo.format == VK_FORMAT_B4G4R4A4_UNORM_PACK16)
      {
        uint16_t *ptr = (uint16_t *)data.data();

        for(uint32_t i = 0; i < dataSize; i += sizeof(uint16_t))
        {
          const uint16_t val = *ptr;
          *ptr = (val >> 4) | ((val & 0xf) << 12);
          ptr++;
        }
      }
      else if(imCreateInfo.format == VK_FORMAT_R5G5B5A1_UNORM_PACK16 ||
              imCreateInfo.format == VK_FORMAT_B5G5R5A1_UNORM_PACK16)
      {
        uint16_t *ptr = (uint16_t *)data.data();

        for(uint32_t i = 0; i < dataSize; i += sizeof(uint16_t))
        {
          const uint16_t val = *ptr;
          *ptr = (val >> 1) | ((val & 0x1) << 15);
          ptr++;
        }
      }
    }
  }

  vt->UnmapMemory(Unwrap(dev), readbackMem);

  // clean up temporary objects
  vt->DestroyBuffer(Unwrap(dev), readbackBuf, NULL);
  vt->FreeMemory(Unwrap(dev), readbackMem, NULL);

  if(tmpImage != VK_NULL_HANDLE)
  {
    GetResourceManager()->ReleaseWrappedResource(wrappedTmpImage, true);
    vt->DestroyImage(Unwrap(dev), tmpImage, NULL);
    vt->FreeMemory(Unwrap(dev), tmpMemory, NULL);
  }

  if(tmpFB != NULL)
  {
    if(IsDepthAndStencilFormat(imInfo.format))
      numFBs *= 2;

    for(uint32_t i = 0; i < numFBs; i++)
    {
      vt->DestroyFramebuffer(Unwrap(dev), tmpFB[i], NULL);
      vt->DestroyImageView(Unwrap(dev), tmpView[i], NULL);
    }
    delete[] tmpFB;
    delete[] tmpView;
    vt->DestroyRenderPass(Unwrap(dev), tmpRP, NULL);
  }
}

void VulkanReplay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                     const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                     ShaderStage type, ResourceId &id, rdcstr &errors)
{
  BuildTargetShader(sourceEncoding, source, entry, compileFlags, type, id, errors);
}

void VulkanReplay::FreeCustomShader(ResourceId id)
{
  if(id == ResourceId())
    return;

  m_pDriver->ReleaseResource(GetResourceManager()->GetCurrentResource(id));
}

ResourceId VulkanReplay::ApplyCustomShader(ResourceId shader, ResourceId texid,
                                           const Subresource &sub, CompType typeCast)
{
  if(shader == ResourceId() || texid == ResourceId())
    return ResourceId();

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[texid];

  GetDebugManager()->CreateCustomShaderTex(iminfo.extent.width, iminfo.extent.height, sub.mip);

  int oldW = m_DebugWidth, oldH = m_DebugHeight;

  m_DebugWidth = RDCMAX(1U, iminfo.extent.width);
  m_DebugHeight = RDCMAX(1U, iminfo.extent.height);

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = shader;
  disp.resourceId = texid;
  disp.typeCast = typeCast;
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.subresource = sub;
  disp.overlay = DebugOverlay::NoOverlay;
  disp.rangeMin = 0.0f;
  disp.rangeMax = 1.0f;
  disp.rawOutput = false;
  disp.scale = 1.0f;

  VkClearValue clearval = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(GetDebugManager()->GetCustomRenderpass()),
      Unwrap(GetDebugManager()->GetCustomFramebuffer()),
      {{
           0, 0,
       },
       {RDCMAX(1U, iminfo.extent.width >> sub.mip), RDCMAX(1U, iminfo.extent.height >> sub.mip)}},
      1,
      &clearval,
  };

  LockedConstImageStateRef imageState = m_pDriver->FindConstImageState(texid);
  if(!imageState)
  {
    RDCWARN("Could not find image info for image %s", ToStr(texid).c_str());
    return ResourceId();
  }
  if(!imageState->isMemoryBound)
    return ResourceId();

  RenderTextureInternal(disp, *imageState, rpbegin, eTexDisplay_MipShift);

  m_DebugWidth = oldW;
  m_DebugHeight = oldH;

  return GetResID(GetDebugManager()->GetCustomTexture());
}

void VulkanReplay::BuildTargetShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                     const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                     ShaderStage type, ResourceId &id, rdcstr &errors)
{
  rdcarray<uint32_t> spirv;

  if(sourceEncoding == ShaderEncoding::GLSL)
  {
    rdcspv::ShaderStage stage = rdcspv::ShaderStage::Invalid;

    switch(type)
    {
      case ShaderStage::Vertex: stage = rdcspv::ShaderStage::Vertex; break;
      case ShaderStage::Hull: stage = rdcspv::ShaderStage::TessControl; break;
      case ShaderStage::Domain: stage = rdcspv::ShaderStage::TessEvaluation; break;
      case ShaderStage::Geometry: stage = rdcspv::ShaderStage::Geometry; break;
      case ShaderStage::Pixel: stage = rdcspv::ShaderStage::Fragment; break;
      case ShaderStage::Compute: stage = rdcspv::ShaderStage::Compute; break;
      default:
        RDCERR("Unexpected type in BuildShader!");
        id = ResourceId();
        return;
    }

    rdcarray<rdcstr> sources;
    sources.push_back(rdcstr((char *)source.begin(), source.size()));

    rdcspv::CompilationSettings settings(rdcspv::InputLanguage::VulkanGLSL, stage);

    rdcstr output = rdcspv::Compile(settings, sources, spirv);

    if(spirv.empty())
    {
      id = ResourceId();
      errors = output;
      return;
    }
  }
  else
  {
    spirv.resize(source.size() / 4);
    memcpy(&spirv[0], source.data(), source.size());
  }

  VkShaderModuleCreateInfo modinfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      NULL,
      0,
      spirv.size() * sizeof(uint32_t),
      &spirv[0],
  };

  VkShaderModule module;
  VkResult vkr = m_pDriver->vkCreateShaderModule(m_pDriver->GetDev(), &modinfo, NULL, &module);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  id = GetResID(module);
}

void VulkanReplay::FreeTargetResource(ResourceId id)
{
  if(id == ResourceId())
    return;

  m_pDriver->ReleaseResource(GetResourceManager()->GetCurrentResource(id));
}

void VulkanReplay::ReplaceResource(ResourceId from, ResourceId to)
{
  // replace the shader module
  m_pDriver->GetResourceManager()->ReplaceResource(from, to);

  // now update any derived resources
  RefreshDerivedReplacements();

  ClearPostVSCache();
  ClearFeedbackCache();
}

void VulkanReplay::RemoveReplacement(ResourceId id)
{
  if(m_pDriver->GetResourceManager()->HasReplacement(id))
  {
    m_pDriver->GetResourceManager()->RemoveReplacement(id);

    RefreshDerivedReplacements();

    ClearPostVSCache();
    ClearFeedbackCache();
  }
}

void VulkanReplay::RefreshDerivedReplacements()
{
  VkDevice dev = m_pDriver->GetDev();

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  // we defer deletes of old replaced resources since it will invalidate elements in the vector
  // we're iterating
  rdcarray<VkPipeline> deletequeue;

  // remake and replace any pipelines that reference a replaced shader
  for(auto it = m_pDriver->m_CreationInfo.m_Pipeline.begin();
      it != m_pDriver->m_CreationInfo.m_Pipeline.end(); ++it)
  {
    ResourceId pipesrcid = it->first;
    const VulkanCreationInfo::Pipeline &pipeInfo = it->second;

    ResourceId origsrcid = rm->GetOriginalID(pipesrcid);

    // only look at pipelines from the capture, no replay-time programs.
    if(origsrcid == pipesrcid)
      continue;

    // if this pipeline has a replacement, remove it and delete the program generated for it
    if(rm->HasReplacement(origsrcid))
    {
      deletequeue.push_back(rm->GetLiveHandle<VkPipeline>(origsrcid));

      rm->RemoveReplacement(origsrcid);
    }

    bool usesReplacedShader = false;
    for(size_t i = 0; i < ARRAY_COUNT(it->second.shaders); i++)
    {
      if(rm->HasReplacement(rm->GetOriginalID(it->second.shaders[i].module)))
      {
        usesReplacedShader = true;
        break;
      }
    }

    // if there are replaced shaders in use, create a new pipeline with any/all replaced shaders.
    if(usesReplacedShader)
    {
      VkPipeline pipe = VK_NULL_HANDLE;

      if(pipeInfo.renderpass != ResourceId())    // check if this is a graphics or compute pipeline
      {
        VkGraphicsPipelineCreateInfo pipeCreateInfo;
        m_pDriver->GetShaderCache()->MakeGraphicsPipelineInfo(pipeCreateInfo, it->first);

        rdcarray<rdcstr> entrynames;
        entrynames.reserve(pipeCreateInfo.stageCount);

        // replace the modules by going via the live ID to pick up any replacements
        for(uint32_t i = 0; i < pipeCreateInfo.stageCount; i++)
        {
          VkPipelineShaderStageCreateInfo &sh =
              (VkPipelineShaderStageCreateInfo &)pipeCreateInfo.pStages[i];

          ResourceId shadOrigId = rm->GetOriginalID(GetResID(sh.module));

          sh.module = rm->GetLiveHandle<VkShaderModule>(shadOrigId);

          if(rm->HasReplacement(shadOrigId))
          {
            rdcarray<rdcstr> entries =
                m_pDriver->m_CreationInfo.m_ShaderModule[GetResID(sh.module)].spirv.EntryPoints();
            if(entries.size() > 1)
            {
              if(entries.contains(sh.pName))
              {
                // nothing to do!
              }
              else
              {
                RDCWARN(
                    "Multiple entry points in edited shader, none matching original, using first "
                    "one '%s'",
                    entries[0].c_str());
                entrynames.push_back(entries[0]);
                sh.pName = entrynames.back().c_str();
              }
            }
            else
            {
              entrynames.push_back(entries[0]);
              sh.pName = entrynames.back().c_str();
            }
          }
        }

        // if we have pipeline executable properties, capture the data
        if(m_pDriver->GetExtensions(NULL).ext_KHR_pipeline_executable_properties)
        {
          pipeCreateInfo.flags |= (VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
                                   VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR);
        }

        // create the new graphics pipeline
        VkResult vkr = m_pDriver->vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                            NULL, &pipe);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }
      else
      {
        VkComputePipelineCreateInfo pipeCreateInfo;
        m_pDriver->GetShaderCache()->MakeComputePipelineInfo(pipeCreateInfo, it->first);

        // replace the module by going via the live ID to pick up any replacements
        VkPipelineShaderStageCreateInfo &sh = pipeCreateInfo.stage;
        ResourceId shadOrigId = rm->GetOriginalID(pipeInfo.shaders[5].module);
        sh.module = rm->GetLiveHandle<VkShaderModule>(shadOrigId);

        // if we have pipeline executable properties, capture the data
        if(m_pDriver->GetExtensions(NULL).ext_KHR_pipeline_executable_properties)
        {
          pipeCreateInfo.flags |= (VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
                                   VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR);
        }

        // create the new compute pipeline
        VkResult vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                           NULL, &pipe);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);
      }

      // remove the replacements
      rm->ReplaceResource(origsrcid, GetResID(pipe));
    }
  }

  for(VkPipeline pipe : deletequeue)
    m_pDriver->vkDestroyPipeline(dev, pipe, NULL);
}

ResourceId VulkanReplay::CreateProxyTexture(const TextureDescription &templateTex)
{
  VULKANNOTIMP("CreateProxyTexture");
  return ResourceId();
}

void VulkanReplay::SetProxyTextureData(ResourceId texid, const Subresource &sub, byte *data,
                                       size_t dataSize)
{
  VULKANNOTIMP("SetProxyTextureData");
}

bool VulkanReplay::IsTextureSupported(const TextureDescription &tex)
{
  return true;
}

bool VulkanReplay::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

ResourceId VulkanReplay::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  VULKANNOTIMP("CreateProxyBuffer");
  return ResourceId();
}

void VulkanReplay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
  VULKANNOTIMP("SetProxyTextureData");
}

ReplayStatus Vulkan_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  RDCDEBUG("Creating a VulkanReplay replay device");

  // disable the layer env var, just in case the user left it set from a previous capture run
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "ENABLE_VULKAN_RENDERDOC_CAPTURE", "0"));

  // disable buggy and user-hostile NV optimus layer, which can completely delete physical devices
  // (not just rearrange them) and cause problems between capture and replay.
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_LAYER_NV_OPTIMUS_1", ""));

  // RTSS layer is buggy, disable it to avoid bug reports that are caused by it
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_RTSS_LAYER", "1"));

  // OBS's layer causes crashes, disable it too.
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_VULKAN_OBS_CAPTURE", "1"));

  Process::ApplyEnvironmentModification();

  void *module = LoadVulkanLibrary();

  if(module == NULL)
  {
    RDCERR("Failed to load vulkan library");
    return ReplayStatus::APIInitFailed;
  }

  VkInitParams initParams;

  uint64_t ver = VkInitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!VkInitParams::IsSupportedVersion(ver))
    {
      RDCERR("Incompatible Vulkan serialise version %llu", ver);
      return ReplayStatus::APIIncompatibleVersion;
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    ser.SetVersion(ver);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RDCERR("Expected to get a DriverInit chunk, instead got %u", chunk);
      return ReplayStatus::FileCorrupted;
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      RDCERR("Failed reading driver init params.");
      return ReplayStatus::FileIOFailed;
    }
  }

  InitReplayTables(module);

  const bool isProxy = (rdc == NULL);

  AMDRGPControl *rgp = NULL;

  if(!isProxy)
  {
    rgp = new AMDRGPControl();

    if(!rgp->Initialised())
      SAFE_DELETE(rgp);
  }

  WrappedVulkan *vk = new WrappedVulkan();

  VulkanReplay *replay = vk->GetReplay();
  replay->SetProxy(isProxy);

  ReplayStatus status = vk->Initialise(initParams, ver, opts);

  if(status != ReplayStatus::Succeeded)
  {
    SAFE_DELETE(rgp);

    delete vk;
    return status;
  }

  RDCLOG("Created device.");
  replay->SetRGP(rgp);

  *driver = (IReplayDriver *)replay;

  replay->GetInitialDriverVersion();

  return ReplayStatus::Succeeded;
}

struct VulkanDriverRegistration
{
  VulkanDriverRegistration()
  {
    RenderDoc::Inst().RegisterReplayProvider(RDCDriver::Vulkan, &Vulkan_CreateReplayDevice);
    RenderDoc::Inst().SetVulkanLayerCheck(&VulkanReplay::CheckVulkanLayer);
    RenderDoc::Inst().SetVulkanLayerInstall(&VulkanReplay::InstallVulkanLayer);
  }
};

static VulkanDriverRegistration VkDriverRegistration;

void Vulkan_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedVulkan vulkan;

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  vulkan.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = vulkan.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    vulkan.GetStructuredFile().Swap(output);
}

static StructuredProcessRegistration VulkanProcessRegistration(RDCDriver::Vulkan,
                                                               &Vulkan_ProcessStructured);
