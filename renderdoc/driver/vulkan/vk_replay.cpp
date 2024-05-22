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

#include "vk_replay.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <algorithm>
#include "core/settings.h"
#include "data/glsl_shaders.h"
#include "driver/ihv/amd/amd_rgp.h"
#include "driver/shaders/spirv/glslang_compile.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "replay/dummy_driver.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "vk_core.h"
#include "vk_debug.h"
#include "vk_resources.h"
#include "vk_shader_cache.h"

#define VULKAN 1
#include "data/glsl/glsl_ubos_cpp.h"

RDOC_EXTERN_CONFIG(bool, Vulkan_Debug_SingleSubmitFlushing);

static const char *SPIRVDisassemblyTarget = "SPIR-V (RenderDoc)";
static const char *AMDShaderInfoTarget = "AMD_shader_info";
static const char *KHRExecutablePropertiesTarget = "KHR_pipeline_executable_properties";

VulkanReplay::VulkanReplay(WrappedVulkan *d)
{
  RenderDoc::Inst().RegisterMemoryRegion(this, sizeof(VulkanReplay));

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

RDResult VulkanReplay::FatalErrorCheck()
{
  return m_pDriver->FatalErrorCheck();
}

IReplayDriver *VulkanReplay::MakeDummyDriver()
{
  // gather up the shaders we've allocated to pass to the dummy driver
  rdcarray<ShaderReflection *> shaders;
  for(auto it = m_pDriver->m_CreationInfo.m_ShaderModule.begin();
      it != m_pDriver->m_CreationInfo.m_ShaderModule.end(); it++)
  {
    for(auto reflit = it->second.m_Reflections.begin(); reflit != it->second.m_Reflections.end();
        ++reflit)
    {
      shaders.push_back(reflit->second.refl);
      reflit->second.refl = NULL;
    }
  }

  IReplayDriver *dummy = new DummyDriver(this, shaders, m_pDriver->DetachStructuredFile());

  return dummy;
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
  CheckVkResult(vkr);

  for(uint32_t p = 0; p < count; p++)
  {
    VkPhysicalDeviceProperties props = {};
    ObjDisp(instance)->GetPhysicalDeviceProperties(devices[p], &props);

    VkPhysicalDeviceDriverProperties driverProps = {};
    GetPhysicalDeviceDriverProperties(ObjDisp(instance), devices[p], driverProps);

    VkDriverInfo driverInfo(props, driverProps);
    GPUDevice dev;
    dev.vendor = driverInfo.Vendor();
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
  ret.rgpCapture =
      (m_DriverInfo.vendor == GPUVendor::AMD || m_DriverInfo.vendor == GPUVendor::Samsung) &&
      m_RGP != NULL && m_RGP->DriverSupportsInterop();
  ret.shaderDebugging = true;
  ret.pixelHistory = true;

  return ret;
}

RDResult VulkanReplay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDriver->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

void VulkanReplay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  if(replayType == eReplay_OnlyDraw)
  {
    bool replayed = FetchShaderFeedback(endEventID);
    if(replayed)
      return;
  }
  m_pDriver->ReplayLog(0, endEventID, replayType);
}

SDFile *VulkanReplay::GetStructuredFile()
{
  return m_pDriver->GetStructuredFile();
}

rdcarray<uint32_t> VulkanReplay::GetPassEvents(uint32_t eventId)
{
  rdcarray<uint32_t> passEvents;

  const ActionDescription *action = m_pDriver->GetAction(eventId);

  if(!action)
    return passEvents;

  // for vulkan a pass == a renderpass, if we're not inside a
  // renderpass then there are no pass events.
  const ActionDescription *start = action;
  while(start)
  {
    // if we've come to the beginning of a pass, break out of the loop, we've
    // found the start.
    // Note that vkCmdNextSubPass has both Begin and End flags set, so it will
    // break out here before we hit the terminating case looking for ActionFlags::EndPass
    if(start->flags & ActionFlags::BeginPass)
      break;

    // if we come to the END of a pass, since we were iterating backwards that
    // means we started outside of a pass, so return empty set.
    // Note that vkCmdNextSubPass has both Begin and End flags set, so it will
    // break out above before we hit this terminating case
    if(start->flags & ActionFlags::EndPass)
      return passEvents;

    // if we've come to the start of the log we were outside of a render pass
    // to start with
    if(start->previous == NULL)
      return passEvents;

    // step back
    start = start->previous;
  }

  // store all the action eventIDs up to the one specified at the start
  while(start)
  {
    if(start->eventId >= action->eventId)
      break;

    // include pass boundaries, these will be filtered out later
    // so we don't actually do anything (init postvs/action overlay)
    // but it's useful to have the first part of the pass as part
    // of the list
    if(start->flags & (ActionFlags::MeshDispatch | ActionFlags::Drawcall | ActionFlags::PassBoundary))
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

rdcarray<DescriptorStoreDescription> VulkanReplay::GetDescriptorStores()
{
  return m_DescriptorStores;
}

void VulkanReplay::RegisterDescriptorStore(const DescriptorStoreDescription &desc)
{
  m_DescriptorStores.push_back(desc);
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

  return shad->second.spirv.EntryPoints();
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
  shad->second.GetReflection(entry.stage, entry.name, pipeline)
      .Init(GetResourceManager(), shader, shad->second.spirv, entry.name,
            VkShaderStageFlagBits(1 << uint32_t(entry.stage)), {});

  return shad->second.GetReflection(entry.stage, entry.name, pipeline).refl;
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
      VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
      NULL,
      Unwrap(pipe),
  };

  VkDevice dev = m_pDriver->GetDev();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  uint32_t execCount = 0;
  vt->GetPipelineExecutablePropertiesKHR(Unwrap(dev), &pipeInfo, &execCount, NULL);

  rdcarray<VkPipelineExecutablePropertiesKHR> executables;
  executables.resize(execCount);
  for(uint32_t i = 0; i < execCount; i++)
    executables[i].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;
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
        VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR,
        NULL,
        Unwrap(pipe),
        i,
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
        it->second.GetReflection(refl->stage, refl->entryPoint, pipeline);
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

    VkShaderStageFlagBits stageBit = VkShaderStageFlagBits(
        1 << it->second.GetReflection(refl->stage, refl->entryPoint, pipeline).stageIndex);

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

    VkShaderStageFlagBits stageBit = VkShaderStageFlagBits(
        1 << it->second.GetReflection(refl->stage, refl->entryPoint, pipeline).stageIndex);

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

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  uint32_t uboOffs = 0;

  VkRenderPassBeginInfo rpbegin = {
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      NULL,
      Unwrap(outw.rp),
      Unwrap(outw.fb),
      {{
           0,
           0,
       },
       {m_DebugWidth, m_DebugHeight}},
      0,
      NULL,
  };
  vt->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_SUBPASS_CONTENTS_INLINE);

  if(m_Overlay.m_CheckerPipeline != VK_NULL_HANDLE)
  {
    CheckerboardUBOData *data = (CheckerboardUBOData *)m_Overlay.m_CheckerUBO.Map(&uboOffs);
    if(!data)
      return;
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
                                {0, 0},
                                {outw.width, outw.height},
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
                                  {x, y},
                                  {64, 64},
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
  CheckVkResult(vkr);

  if(Vulkan_Debug_SingleSubmitFlushing())
    m_pDriver->SubmitCmds();
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

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  {
    VkRenderPassBeginInfo rpbegin = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        NULL,
        Unwrap(outw.rp),
        Unwrap(outw.fb),
        {{
             0,
             0,
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
             {tl.x, tl.y},
             {1, sz},
         },
         0,
         1},
        {{
             {tl.x + (int32_t)sz, tl.y},
             {1, sz + 1},
         },
         0,
         1},
        {{
             {tl.x, tl.y},
             {sz, 1},
         },
         0,
         1},
        {{
             {tl.x, tl.y + (int32_t)sz},
             {sz, 1},
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
  CheckVkResult(vkr);

  if(Vulkan_Debug_SingleSubmitFlushing())
    m_pDriver->SubmitCmds();
}

void VulkanReplay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &ret)
{
  bytebuf inlineData;
  bool useInlineData = false;

  // specialisation constants 'descriptor' stored in a pipeline or shader object
  auto pipe = m_pDriver->m_CreationInfo.m_Pipeline.find(buff);
  auto shad = m_pDriver->m_CreationInfo.m_ShaderObject.find(buff);
  if(pipe != m_pDriver->m_CreationInfo.m_Pipeline.end())
  {
    const VulkanCreationInfo::Pipeline &p = pipe->second;

    for(size_t i = 0; i < NumShaderStages; i++)
    {
      // set up the defaults
      if(p.shaders[i].refl)
      {
        for(size_t cb = 0; cb < p.shaders[i].refl->constantBlocks.size(); cb++)
        {
          if(p.shaders[i].refl->constantBlocks[cb].compileConstants)
          {
            for(const ShaderConstant &sc : p.shaders[i].refl->constantBlocks[cb].variables)
            {
              inlineData.resize_for_index(sc.byteOffset + sizeof(uint64_t));
              memcpy(inlineData.data() + sc.byteOffset, &sc.defaultValue, sizeof(uint64_t));
            }
            break;
          }
        }
      }

      // apply any specializations
      for(const SpecConstant &s : p.shaders[i].specialization)
      {
        int32_t idx = p.shaders[i].patchData->specIDs.indexOf(s.specID);

        if(idx == -1)
        {
          RDCWARN("Couldn't find offset for spec ID %u", s.specID);
          continue;
        }

        size_t offs = idx * sizeof(uint64_t);

        inlineData.resize_for_index(offs + sizeof(uint64_t));
        memcpy(inlineData.data() + offs, &s.value, s.dataSize);
      }
    }

    useInlineData = true;
  }
  else if(shad != m_pDriver->m_CreationInfo.m_ShaderObject.end())
  {
    const VulkanCreationInfo::ShaderEntry &shader = shad->second.shad;

    // set up the defaults
    if(shader.refl)
    {
      for(size_t cb = 0; cb < shader.refl->constantBlocks.size(); cb++)
      {
        if(shader.refl->constantBlocks[cb].compileConstants)
        {
          for(const ShaderConstant &sc : shader.refl->constantBlocks[cb].variables)
          {
            inlineData.resize_for_index(sc.byteOffset + sizeof(uint64_t));
            memcpy(inlineData.data() + sc.byteOffset, &sc.defaultValue, sizeof(uint64_t));
          }
          break;
        }
      }
    }

    // apply any specializations
    for(const SpecConstant &s : shader.specialization)
    {
      int32_t idx = shader.patchData->specIDs.indexOf(s.specID);

      if(idx == -1)
      {
        RDCWARN("Couldn't find offset for spec ID %u", s.specID);
        continue;
      }

      size_t offs = idx * sizeof(uint64_t);

      inlineData.resize_for_index(offs + sizeof(uint64_t));
      memcpy(inlineData.data() + offs, &s.value, s.dataSize);
    }

    useInlineData = true;
  }

  // push constants 'descriptor' stored in a command buffer
  if(WrappedVkCommandBuffer::IsAlloc(GetResourceManager()->GetCurrentResource(buff)))
  {
    inlineData.assign(m_pDriver->m_RenderState.pushconsts, m_pDriver->m_RenderState.pushConstSize);
    useInlineData = true;
  }

  // inline uniform data inside a descriptor set
  auto descit = m_pDriver->m_DescriptorSetState.find(buff);
  if(descit != m_pDriver->m_DescriptorSetState.end())
  {
    const WrappedVulkan::DescriptorSetInfo &set = descit->second;

    inlineData = set.data.inlineBytes;
    useInlineData = true;
  }

  if(useInlineData)
  {
    if(offset >= inlineData.size())
      return;

    if(len == 0 || len > inlineData.size())
      len = inlineData.size() - offset;

    if(offset + len > inlineData.size())
    {
      RDCWARN(
          "Attempting to read off the end of current push constants (%llu %llu). Will be clamped "
          "(%llu)",
          offset, len, inlineData.size());
      len = RDCMIN(len, inlineData.size() - offset);
    }

    ret.resize((size_t)len);

    memcpy(ret.data(), inlineData.data() + offset, ret.size());

    return;
  }

  GetDebugManager()->GetBufferData(buff, offset, len, ret);
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

  VkPhysicalDeviceProperties props = {};
  ObjDisp(inst)->GetPhysicalDeviceProperties(firstDevice, &props);

  VkPhysicalDeviceDriverProperties driverProps = {};
  GetPhysicalDeviceDriverProperties(ObjDisp(inst), firstDevice, driverProps);

  SetDriverInformation(props, driverProps);
}

void VulkanReplay::SetDriverInformation(const VkPhysicalDeviceProperties &props,
                                        const VkPhysicalDeviceDriverProperties &driverProps)
{
  VkDriverInfo info(props, driverProps);
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
  if(!m_VulkanPipelineState)
    return;

  const VulkanRenderState &state = m_pDriver->m_RenderState;
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  VKPipe::State &ret = *m_VulkanPipelineState;

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  VkMarkerRegion::Begin(StringFormat::Fmt("FetchShaderFeedback for %u", eventId));

  FetchShaderFeedback(eventId);

  VkMarkerRegion::End();

  {
    // reset the pipeline state, but keep the descriptor set arrays. This prevents needless
    // reallocations, we'll ensure that descriptors are fully overwritten below.
    rdcarray<VKPipe::DescriptorSet> graphicsDescriptors;
    rdcarray<VKPipe::DescriptorSet> computeDescriptors;

    ret.graphics.descriptorSets.swap(graphicsDescriptors);
    ret.compute.descriptorSets.swap(computeDescriptors);

    ret = VKPipe::State();

    ret.graphics.descriptorSets.swap(graphicsDescriptors);
    ret.compute.descriptorSets.swap(computeDescriptors);
  }

  ret.pushconsts.resize(state.pushConstSize);
  memcpy(ret.pushconsts.data(), state.pushconsts, state.pushConstSize);

  // General pipeline properties
  ret.compute.pipelineResourceId = rm->GetUnreplacedOriginalID(state.compute.pipeline);
  ret.graphics.pipelineResourceId = rm->GetUnreplacedOriginalID(state.graphics.pipeline);

  if(state.compute.pipeline != ResourceId() || state.compute.shaderObject)
  {
    const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.compute.pipeline];

    ret.compute.pipelineComputeLayoutResourceId = rm->GetOriginalID(p.compLayout);

    ret.compute.flags = p.flags;

    VKPipe::Shader &stage = ret.computeShader;

    int i = 5;    // 5 is the CS idx (VS, TCS, TES, GS, FS, CS)
    {
      stage.shaderObject = state.compute.shaderObject;

      const VulkanCreationInfo::ShaderEntry &shad =
          stage.shaderObject ? c.m_ShaderObject[state.shaderObjects[i]].shad : p.shaders[i];

      const rdcarray<VkPushConstantRange> &pushRanges =
          stage.shaderObject ? c.m_ShaderObject[state.shaderObjects[i]].pushRanges
                             : c.m_PipelineLayout[p.compLayout].pushRanges;

      stage.resourceId = rm->GetUnreplacedOriginalID(shad.module);
      stage.entryPoint = shad.entryPoint;

      stage.stage = ShaderStage::Compute;
      if(shad.refl)
        stage.reflection = shad.refl;

      stage.pushConstantRangeByteOffset = stage.pushConstantRangeByteSize = 0;
      for(const VkPushConstantRange &pr : pushRanges)
      {
        if(pr.stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
        {
          stage.pushConstantRangeByteOffset = pr.offset;
          stage.pushConstantRangeByteSize = pr.size;
          break;
        }
      }

      stage.requiredSubgroupSize = p.shaders[i].requiredSubgroupSize;

      stage.specializationData.clear();

      // set up the defaults
      if(shad.refl)
      {
        for(size_t cb = 0; cb < shad.refl->constantBlocks.size(); cb++)
        {
          if(shad.refl->constantBlocks[cb].compileConstants)
          {
            for(const ShaderConstant &sc : shad.refl->constantBlocks[cb].variables)
            {
              stage.specializationData.resize_for_index(sc.byteOffset + sizeof(uint64_t));
              memcpy(stage.specializationData.data() + sc.byteOffset, &sc.defaultValue,
                     sizeof(uint64_t));
            }
            break;
          }
        }
      }

      // apply any specializations
      for(const SpecConstant &s : shad.specialization)
      {
        int32_t idx = shad.patchData->specIDs.indexOf(s.specID);

        if(idx == -1)
        {
          RDCWARN("Couldn't find offset for spec ID %u", s.specID);
          continue;
        }

        size_t offs = idx * sizeof(uint64_t);

        stage.specializationData.resize_for_index(offs + sizeof(uint64_t));
        memcpy(stage.specializationData.data() + offs, &s.value, s.dataSize);
      }
      if(shad.patchData)
        stage.specializationIds = shad.patchData->specIDs;
    }
  }
  else
  {
    ret.compute.pipelineComputeLayoutResourceId = ResourceId();
    ret.compute.flags = 0;
    ret.computeShader = VKPipe::Shader();
  }

  if(state.graphics.pipeline != ResourceId() || state.graphics.shaderObject)
  {
    const VulkanCreationInfo::Pipeline &p = c.m_Pipeline[state.graphics.pipeline];

    ret.graphics.pipelinePreRastLayoutResourceId = rm->GetOriginalID(p.vertLayout);
    ret.graphics.pipelineFragmentLayoutResourceId = rm->GetOriginalID(p.fragLayout);

    ret.graphics.flags = p.flags;

    // Input Assembly
    ret.inputAssembly.indexBuffer.resourceId = rm->GetOriginalID(state.ibuffer.buf);
    ret.inputAssembly.indexBuffer.byteOffset = state.ibuffer.offs;
    ret.inputAssembly.indexBuffer.byteStride = state.ibuffer.bytewidth;
    ret.inputAssembly.primitiveRestartEnable = state.primRestartEnable != VK_FALSE;
    ret.inputAssembly.topology =
        MakePrimitiveTopology(state.primitiveTopology, state.patchControlPoints);

    // Vertex Input
    ret.vertexInput.attributes.resize(state.vertexAttributes.size());
    for(size_t i = 0; i < state.vertexAttributes.size(); i++)
    {
      ret.vertexInput.attributes[i].location = state.vertexAttributes[i].location;
      ret.vertexInput.attributes[i].binding = state.vertexAttributes[i].binding;
      ret.vertexInput.attributes[i].byteOffset = state.vertexAttributes[i].offset;
      ret.vertexInput.attributes[i].format = MakeResourceFormat(state.vertexAttributes[i].format);
    }

    ret.vertexInput.bindings.resize(state.vertexBindings.size());
    for(const VkVertexInputBindingDescription2EXT &b : state.vertexBindings)
    {
      ret.vertexInput.bindings.resize_for_index(b.binding);
      ret.vertexInput.bindings[b.binding].vertexBufferBinding = b.binding;
      ret.vertexInput.bindings[b.binding].perInstance = b.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE;
      ret.vertexInput.bindings[b.binding].instanceDivisor = b.divisor;
    }

    ret.vertexInput.vertexBuffers.resize(state.vbuffers.size());
    for(size_t i = 0; i < state.vbuffers.size(); i++)
    {
      ret.vertexInput.vertexBuffers[i].resourceId = rm->GetOriginalID(state.vbuffers[i].buf);
      ret.vertexInput.vertexBuffers[i].byteOffset = state.vbuffers[i].offs;
      ret.vertexInput.vertexBuffers[i].byteStride = (uint32_t)state.vbuffers[i].stride;
      ret.vertexInput.vertexBuffers[i].byteSize = (uint32_t)state.vbuffers[i].size;
    }

    // Shader Stages
    VKPipe::Shader *stages[] = {
        &ret.vertexShader,
        &ret.tessControlShader,
        &ret.tessEvalShader,
        &ret.geometryShader,
        &ret.fragmentShader,
        // compute
        NULL,
        &ret.taskShader,
        &ret.meshShader,
    };

    for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
    {
      if(stages[i] == NULL)
        continue;

      stages[i]->shaderObject = state.graphics.shaderObject;

      const VulkanCreationInfo::ShaderEntry &shad =
          stages[i]->shaderObject ? c.m_ShaderObject[state.shaderObjects[i]].shad : p.shaders[i];

      const rdcarray<VkPushConstantRange> &pushRanges =
          stages[i]->shaderObject ? c.m_ShaderObject[state.shaderObjects[i]].pushRanges
                                  : c.m_PipelineLayout[p.vertLayout].pushRanges;

      stages[i]->resourceId = rm->GetUnreplacedOriginalID(shad.module);
      stages[i]->entryPoint = shad.entryPoint;

      stages[i]->stage = StageFromIndex(i);
      if(shad.refl)
        stages[i]->reflection = shad.refl;

      stages[i]->pushConstantRangeByteOffset = stages[i]->pushConstantRangeByteSize = 0;
      // don't have to handle separate vert/frag layouts as push constant ranges must be identical
      for(const VkPushConstantRange &pr : pushRanges)
      {
        if(pr.stageFlags & ShaderMaskFromIndex(i))
        {
          stages[i]->pushConstantRangeByteOffset = pr.offset;
          stages[i]->pushConstantRangeByteSize = pr.size;
          break;
        }
      }

      stages[i]->specializationData.clear();

      stages[i]->requiredSubgroupSize = p.shaders[i].requiredSubgroupSize;

      // set up the defaults
      if(shad.refl)
      {
        for(size_t cb = 0; cb < shad.refl->constantBlocks.size(); cb++)
        {
          if(shad.refl->constantBlocks[cb].compileConstants)
          {
            for(const ShaderConstant &sc : shad.refl->constantBlocks[cb].variables)
            {
              stages[i]->specializationData.resize_for_index(sc.byteOffset + sizeof(uint64_t));
              memcpy(stages[i]->specializationData.data() + sc.byteOffset, &sc.defaultValue,
                     sizeof(uint64_t));
            }
            break;
          }
        }
      }

      // apply any specializations
      for(const SpecConstant &s : shad.specialization)
      {
        int32_t idx = shad.patchData->specIDs.indexOf(s.specID);

        if(idx == -1)
        {
          RDCWARN("Couldn't find offset for spec ID %u", s.specID);
          continue;
        }

        size_t offs = idx * sizeof(uint64_t);

        stages[i]->specializationData.resize_for_index(offs + sizeof(uint64_t));
        memcpy(stages[i]->specializationData.data() + offs, &s.value, s.dataSize);
      }
      if(shad.patchData)
        stages[i]->specializationIds = shad.patchData->specIDs;
    }

    // Tessellation
    ret.tessellation.numControlPoints = p.patchControlPoints;

    ret.tessellation.domainOriginUpperLeft =
        state.domainOrigin == VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT;

    ret.transformFeedback.rasterizedStream = state.rasterStream;

    // Transform feedback
    ret.transformFeedback.buffers.resize(state.xfbbuffers.size());
    for(size_t i = 0; i < state.xfbbuffers.size(); i++)
    {
      ret.transformFeedback.buffers[i].bufferResourceId = rm->GetOriginalID(state.xfbbuffers[i].buf);
      ret.transformFeedback.buffers[i].byteOffset = state.xfbbuffers[i].offs;
      ret.transformFeedback.buffers[i].byteSize = state.xfbbuffers[i].size;

      ret.transformFeedback.buffers[i].active = false;
      ret.transformFeedback.buffers[i].counterBufferResourceId = ResourceId();
      ret.transformFeedback.buffers[i].counterBufferOffset = 0;

      if(i >= state.firstxfbcounter)
      {
        size_t xfb = i - state.firstxfbcounter;
        if(xfb < state.xfbcounters.size())
        {
          ret.transformFeedback.buffers[i].active = true;
          ret.transformFeedback.buffers[i].counterBufferResourceId =
              rm->GetOriginalID(state.xfbcounters[xfb].buf);
          ret.transformFeedback.buffers[i].counterBufferOffset = state.xfbcounters[xfb].offs;
        }
      }
    }

    // Viewport/Scissors
    size_t numViewScissors = state.views.size();
    ret.viewportScissor.viewportScissors.resize(numViewScissors);
    for(size_t i = 0; i < numViewScissors; i++)
    {
      if(i < state.views.size())
      {
        ret.viewportScissor.viewportScissors[i].vp.x = state.views[i].x;
        ret.viewportScissor.viewportScissors[i].vp.y = state.views[i].y;
        ret.viewportScissor.viewportScissors[i].vp.width = state.views[i].width;
        ret.viewportScissor.viewportScissors[i].vp.height = state.views[i].height;
        ret.viewportScissor.viewportScissors[i].vp.minDepth = state.views[i].minDepth;
        ret.viewportScissor.viewportScissors[i].vp.maxDepth = state.views[i].maxDepth;
      }
      else
      {
        RDCEraseEl(ret.viewportScissor.viewportScissors[i].vp);
      }

      if(i < state.scissors.size())
      {
        ret.viewportScissor.viewportScissors[i].scissor.x = state.scissors[i].offset.x;
        ret.viewportScissor.viewportScissors[i].scissor.y = state.scissors[i].offset.y;
        ret.viewportScissor.viewportScissors[i].scissor.width = state.scissors[i].extent.width;
        ret.viewportScissor.viewportScissors[i].scissor.height = state.scissors[i].extent.height;
      }
      else
      {
        RDCEraseEl(ret.viewportScissor.viewportScissors[i].scissor);
      }
    }

    {
      ret.viewportScissor.discardRectangles.resize(p.discardRectangles.size());
      for(size_t i = 0; i < p.discardRectangles.size() && i < state.discardRectangles.size(); i++)
      {
        ret.viewportScissor.discardRectangles[i].x = state.discardRectangles[i].offset.x;
        ret.viewportScissor.discardRectangles[i].y = state.discardRectangles[i].offset.y;
        ret.viewportScissor.discardRectangles[i].width = state.discardRectangles[i].extent.width;
        ret.viewportScissor.discardRectangles[i].height = state.discardRectangles[i].extent.height;
      }

      ret.viewportScissor.discardRectanglesExclusive =
          (p.discardMode == VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT);
    }

    {
      ret.viewportScissor.depthNegativeOneToOne = state.negativeOneToOne != VK_FALSE;
    }

    // Rasterizer
    ret.rasterizer.depthClampEnable = state.depthClampEnable != VK_FALSE;
    ret.rasterizer.depthClipEnable = state.depthClipEnable != VK_FALSE;
    ret.rasterizer.rasterizerDiscardEnable = state.rastDiscardEnable != VK_FALSE;
    ret.rasterizer.frontCCW = state.frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE;

    ret.rasterizer.conservativeRasterization = ConservativeRaster::Disabled;
    switch(state.conservativeRastMode)
    {
      case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
        ret.rasterizer.conservativeRasterization = ConservativeRaster::Underestimate;
        break;
      case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
        ret.rasterizer.conservativeRasterization = ConservativeRaster::Overestimate;
        break;
      default: break;
    }

    ret.rasterizer.pipelineShadingRate = {state.pipelineShadingRate.width,
                                          state.pipelineShadingRate.height};

    ShadingRateCombiner combiners[2] = {};
    for(int i = 0; i < 2; i++)
    {
      switch(state.shadingRateCombiners[i])
      {
        default:
        case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR:
          combiners[i] = ShadingRateCombiner::Keep;
          break;
        case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR:
          combiners[i] = ShadingRateCombiner::Replace;
          break;
        case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR:
          combiners[i] = ShadingRateCombiner::Min;
          break;
        case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR:
          combiners[i] = ShadingRateCombiner::Max;
          break;
        case VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR:
          combiners[i] = ShadingRateCombiner::Multiply;
          break;
      }
    }
    ret.rasterizer.shadingRateCombiners = {combiners[0], combiners[1]};

    ret.rasterizer.lineRasterMode = LineRaster::Default;

    // "VK_LINE_RASTERIZATION_MODE_DEFAULT_HKR is equivalent to
    // VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR if VkPhysicalDeviceLimits::strictLines is VK_TRUE"
    if(m_pDriver->GetDeviceProps().limits.strictLines)
      ret.rasterizer.lineRasterMode = LineRaster::Rectangular;

    switch(state.lineRasterMode)
    {
      case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR:
        ret.rasterizer.lineRasterMode = LineRaster::Rectangular;
        break;
      case VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR:
        ret.rasterizer.lineRasterMode = LineRaster::Bresenham;
        break;
      case VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_KHR:
        ret.rasterizer.lineRasterMode = LineRaster::RectangularSmooth;
        break;
      default: break;
    }

    ret.rasterizer.lineStippleFactor = 0;    // stippled line disable
    ret.rasterizer.lineStipplePattern = 0;

    if(state.stippledLineEnable)
    {
      ret.rasterizer.lineStippleFactor = state.stippleFactor;
      ret.rasterizer.lineStipplePattern = state.stipplePattern;
    }

    ret.rasterizer.extraPrimitiveOverestimationSize = state.primOverestimationSize;

    switch(state.polygonMode)
    {
      case VK_POLYGON_MODE_POINT: ret.rasterizer.fillMode = FillMode::Point; break;
      case VK_POLYGON_MODE_LINE: ret.rasterizer.fillMode = FillMode::Wireframe; break;
      case VK_POLYGON_MODE_FILL: ret.rasterizer.fillMode = FillMode::Solid; break;
      default:
        ret.rasterizer.fillMode = FillMode::Solid;
        RDCERR("Unexpected value for FillMode %x", state.polygonMode);
        break;
    }

    switch(state.cullMode)
    {
      case VK_CULL_MODE_NONE: ret.rasterizer.cullMode = CullMode::NoCull; break;
      case VK_CULL_MODE_FRONT_BIT: ret.rasterizer.cullMode = CullMode::Front; break;
      case VK_CULL_MODE_BACK_BIT: ret.rasterizer.cullMode = CullMode::Back; break;
      case VK_CULL_MODE_FRONT_AND_BACK: ret.rasterizer.cullMode = CullMode::FrontAndBack; break;
      default:
        ret.rasterizer.cullMode = CullMode::NoCull;
        RDCERR("Unexpected value for CullMode %x", state.cullMode);
        break;
    }

    ret.rasterizer.provokingVertexFirst =
        state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT;

    ret.rasterizer.depthBiasEnable = state.depthBiasEnable != VK_FALSE;
    ret.rasterizer.depthBias = state.bias.depth;
    ret.rasterizer.depthBiasClamp = state.bias.biasclamp;
    ret.rasterizer.slopeScaledDepthBias = state.bias.slope;
    ret.rasterizer.lineWidth = state.lineWidth;

    // MSAA
    ret.multisample.rasterSamples = state.rastSamples;
    ret.multisample.sampleShadingEnable = p.sampleShadingEnable;
    ret.multisample.minSampleShading = p.minSampleShading;
    ret.multisample.sampleMask = state.sampleMask[0];

    ret.multisample.sampleLocations.customLocations.clear();
    if(state.sampleLocEnable)
    {
      ret.multisample.sampleLocations.gridWidth = state.sampleLocations.gridSize.width;
      ret.multisample.sampleLocations.gridHeight = state.sampleLocations.gridSize.height;
      ret.multisample.sampleLocations.customLocations.reserve(state.sampleLocations.locations.size());
      for(const VkSampleLocationEXT &loc : state.sampleLocations.locations)
      {
        ret.multisample.sampleLocations.customLocations.push_back({loc.x, loc.y, 0.0f, 0.0f});
      }
    }

    // Color Blend
    ret.colorBlend.alphaToCoverageEnable = state.alphaToCoverageEnable != VK_FALSE;
    ret.colorBlend.alphaToOneEnable = state.alphaToOneEnable != VK_FALSE;

    // find size if no static pipeline state
    size_t numAttach = RDCMAX(state.colorBlendEnable.size(), state.colorBlendEquation.size());
    numAttach = RDCMAX(numAttach, state.colorWriteEnable.size());
    numAttach = RDCMAX(numAttach, state.colorWriteMask.size());

    ret.colorBlend.blends.resize(numAttach);

    for(size_t i = 0; i < numAttach; i++)
    {
      ret.colorBlend.blends[i].enabled =
          (i < state.colorBlendEnable.size()) ? state.colorBlendEnable[i] != VK_FALSE : VK_FALSE;

      // due to shared structs, this is slightly duplicated - Vulkan doesn't have separate states
      // for logic operations
      ret.colorBlend.blends[i].logicOperationEnabled = state.logicOpEnable != VK_FALSE;
      ret.colorBlend.blends[i].logicOperation = MakeLogicOp(state.logicOp);

      if(ret.colorBlend.blends[i].enabled && i < state.colorBlendEquation.size())
      {
        ret.colorBlend.blends[i].colorBlend.source =
            MakeBlendMultiplier(state.colorBlendEquation[i].srcColorBlendFactor);
        ret.colorBlend.blends[i].colorBlend.destination =
            MakeBlendMultiplier(state.colorBlendEquation[i].dstColorBlendFactor);
        ret.colorBlend.blends[i].colorBlend.operation =
            MakeBlendOp(state.colorBlendEquation[i].colorBlendOp);

        ret.colorBlend.blends[i].alphaBlend.source =
            MakeBlendMultiplier(state.colorBlendEquation[i].srcAlphaBlendFactor);
        ret.colorBlend.blends[i].alphaBlend.destination =
            MakeBlendMultiplier(state.colorBlendEquation[i].dstAlphaBlendFactor);
        ret.colorBlend.blends[i].alphaBlend.operation =
            MakeBlendOp(state.colorBlendEquation[i].alphaBlendOp);
      }
      else
      {
        ret.colorBlend.blends[i].colorBlend.source = MakeBlendMultiplier(VK_BLEND_FACTOR_ZERO);
        ret.colorBlend.blends[i].colorBlend.destination = MakeBlendMultiplier(VK_BLEND_FACTOR_ZERO);
        ret.colorBlend.blends[i].colorBlend.operation = MakeBlendOp(VK_BLEND_OP_ADD);
        ret.colorBlend.blends[i].alphaBlend.source = MakeBlendMultiplier(VK_BLEND_FACTOR_ZERO);
        ret.colorBlend.blends[i].alphaBlend.destination = MakeBlendMultiplier(VK_BLEND_FACTOR_ZERO);
        ret.colorBlend.blends[i].alphaBlend.operation = MakeBlendOp(VK_BLEND_OP_ADD);
      }

      ret.colorBlend.blends[i].writeMask =
          (i < state.colorWriteMask.size()) ? (uint8_t)state.colorWriteMask[i] : 0;

      if(i < state.colorWriteEnable.size() && !state.colorWriteEnable[i])
        ret.colorBlend.blends[i].writeMask = 0;
    }

    ret.colorBlend.blendFactor = state.blendConst;

    // Depth Stencil
    ret.depthStencil.depthTestEnable = state.depthTestEnable != VK_FALSE;
    ret.depthStencil.depthWriteEnable = state.depthWriteEnable != VK_FALSE;
    ret.depthStencil.depthBoundsEnable = state.depthBoundsTestEnable != VK_FALSE;
    ret.depthStencil.depthFunction = MakeCompareFunc(state.depthCompareOp);
    ret.depthStencil.stencilTestEnable = state.stencilTestEnable != VK_FALSE;

    ret.depthStencil.frontFace.passOperation = MakeStencilOp(state.front.passOp);
    ret.depthStencil.frontFace.failOperation = MakeStencilOp(state.front.failOp);
    ret.depthStencil.frontFace.depthFailOperation = MakeStencilOp(state.front.depthFailOp);
    ret.depthStencil.frontFace.function = MakeCompareFunc(state.front.compareOp);

    ret.depthStencil.backFace.passOperation = MakeStencilOp(state.back.passOp);
    ret.depthStencil.backFace.failOperation = MakeStencilOp(state.back.failOp);
    ret.depthStencil.backFace.depthFailOperation = MakeStencilOp(state.back.depthFailOp);
    ret.depthStencil.backFace.function = MakeCompareFunc(state.back.compareOp);

    ret.depthStencil.minDepthBounds = state.mindepth;
    ret.depthStencil.maxDepthBounds = state.maxdepth;

    ret.depthStencil.frontFace.reference = state.front.ref;
    ret.depthStencil.frontFace.compareMask = state.front.compare;
    ret.depthStencil.frontFace.writeMask = state.front.write;

    ret.depthStencil.backFace.reference = state.back.ref;
    ret.depthStencil.backFace.compareMask = state.back.compare;
    ret.depthStencil.backFace.writeMask = state.back.write;
  }
  else
  {
    ret.graphics.pipelinePreRastLayoutResourceId = ResourceId();
    ret.graphics.pipelineFragmentLayoutResourceId = ResourceId();

    ret.graphics.flags = 0;

    ret.vertexInput.attributes.clear();
    ret.vertexInput.bindings.clear();
    ret.vertexInput.vertexBuffers.clear();

    VKPipe::Shader *stages[] = {
        &ret.vertexShader,   &ret.tessControlShader, &ret.tessEvalShader, &ret.geometryShader,
        &ret.fragmentShader, &ret.taskShader,        &ret.meshShader,
    };

    for(size_t i = 0; i < ARRAY_COUNT(stages); i++)
      *stages[i] = VKPipe::Shader();

    ret.viewportScissor.viewportScissors.clear();
    ret.viewportScissor.discardRectangles.clear();
    ret.viewportScissor.discardRectanglesExclusive = true;
    ret.viewportScissor.depthNegativeOneToOne = false;

    ret.colorBlend.blends.clear();
  }

  if(state.dynamicRendering.active)
  {
    VKPipe::RenderPass &rpState = ret.currentPass.renderpass;
    VKPipe::Framebuffer &fbState = ret.currentPass.framebuffer;
    const VulkanRenderState::DynamicRendering &dyn = state.dynamicRendering;

    rpState.dynamic = true;
    rpState.suspended = dyn.suspended;
    rpState.feedbackLoop = false;
    rpState.resourceId = ResourceId();
    rpState.subpass = 0;
    rpState.fragmentDensityOffsets.clear();
    rpState.tileOnlyMSAASampleCount = 0;

    fbState.resourceId = ResourceId();
    // dynamic rendering does not provide a framebuffer dimension, it's implicit from the image
    // views
    fbState.width = 0;
    fbState.height = 0;
    fbState.layers = dyn.layerCount;

    fbState.attachments.clear();
    rpState.inputAttachments.clear();
    rpState.colorAttachments.clear();
    rpState.resolveAttachments.clear();

    size_t attIdx = 0;
    for(size_t i = 0; i < dyn.color.size(); i++)
    {
      fbState.attachments.push_back({});

      ResourceId viewid = GetResID(dyn.color[i].imageView);

      if(viewid != ResourceId())
      {
        fbState.attachments.back().view = rm->GetOriginalID(viewid);
        ret.currentPass.framebuffer.attachments[attIdx].resource =
            rm->GetOriginalID(c.m_ImageView[viewid].image);

        fbState.attachments.back().format = MakeResourceFormat(c.m_ImageView[viewid].format);
        fbState.attachments.back().firstMip = c.m_ImageView[viewid].range.baseMipLevel & 0xff;
        fbState.attachments.back().firstSlice = c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
        fbState.attachments.back().numMips = c.m_ImageView[viewid].range.levelCount & 0xff;
        fbState.attachments.back().numSlices = c.m_ImageView[viewid].range.layerCount & 0xffff;

        Convert(fbState.attachments.back().swizzle, c.m_ImageView[viewid].componentMapping);
      }
      else
      {
        fbState.attachments.back().view = ResourceId();
        fbState.attachments.back().resource = ResourceId();

        fbState.attachments.back().firstMip = 0;
        fbState.attachments.back().firstSlice = 0;
        fbState.attachments.back().numMips = 1;
        fbState.attachments.back().numSlices = 1;
      }

      rpState.colorAttachments.push_back(uint32_t(attIdx++));

      if(dyn.color[i].resolveMode && dyn.color[i].resolveImageView != VK_NULL_HANDLE)
      {
        fbState.attachments.push_back({});

        viewid = GetResID(dyn.color[i].resolveImageView);

        fbState.attachments.back().view = rm->GetOriginalID(viewid);
        ret.currentPass.framebuffer.attachments[attIdx].resource =
            rm->GetOriginalID(c.m_ImageView[viewid].image);

        fbState.attachments.back().format = MakeResourceFormat(c.m_ImageView[viewid].format);
        fbState.attachments.back().firstMip = c.m_ImageView[viewid].range.baseMipLevel & 0xff;
        fbState.attachments.back().firstSlice = c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
        fbState.attachments.back().numMips = c.m_ImageView[viewid].range.levelCount & 0xff;
        fbState.attachments.back().numSlices = c.m_ImageView[viewid].range.layerCount & 0xffff;

        Convert(fbState.attachments.back().swizzle, c.m_ImageView[viewid].componentMapping);

        rpState.resolveAttachments.push_back(uint32_t(attIdx++));
      }
    }

    if(dyn.depth.imageView != VK_NULL_HANDLE || dyn.stencil.imageView != VK_NULL_HANDLE)
    {
      fbState.attachments.push_back({});

      ResourceId viewid = GetResID(dyn.depth.imageView);
      if(dyn.depth.imageView == VK_NULL_HANDLE)
        viewid = GetResID(dyn.stencil.imageView);

      fbState.attachments.back().view = rm->GetOriginalID(viewid);
      ret.currentPass.framebuffer.attachments[attIdx].resource =
          rm->GetOriginalID(c.m_ImageView[viewid].image);

      fbState.attachments.back().format = MakeResourceFormat(c.m_ImageView[viewid].format);
      fbState.attachments.back().firstMip = c.m_ImageView[viewid].range.baseMipLevel & 0xff;
      fbState.attachments.back().firstSlice = c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
      fbState.attachments.back().numMips = c.m_ImageView[viewid].range.levelCount & 0xff;
      fbState.attachments.back().numSlices = c.m_ImageView[viewid].range.layerCount & 0xffff;

      Convert(fbState.attachments.back().swizzle, c.m_ImageView[viewid].componentMapping);

      rpState.depthstencilAttachment = int32_t(attIdx++);
    }
    else
    {
      rpState.depthstencilAttachment = -1;
    }

    if(dyn.fragmentDensityView != VK_NULL_HANDLE)
    {
      fbState.attachments.push_back({});

      ResourceId viewid = GetResID(dyn.fragmentDensityView);

      fbState.attachments.back().view = rm->GetOriginalID(viewid);
      ret.currentPass.framebuffer.attachments[attIdx].resource =
          rm->GetOriginalID(c.m_ImageView[viewid].image);

      fbState.attachments.back().format = MakeResourceFormat(c.m_ImageView[viewid].format);
      fbState.attachments.back().firstMip = c.m_ImageView[viewid].range.baseMipLevel & 0xff;
      fbState.attachments.back().firstSlice = c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
      fbState.attachments.back().numMips = c.m_ImageView[viewid].range.levelCount & 0xff;
      fbState.attachments.back().numSlices = c.m_ImageView[viewid].range.layerCount & 0xffff;

      Convert(fbState.attachments.back().swizzle, c.m_ImageView[viewid].componentMapping);

      rpState.fragmentDensityAttachment = int32_t(attIdx++);
    }
    else
    {
      rpState.fragmentDensityAttachment = -1;
    }

    if(dyn.shadingRateView != VK_NULL_HANDLE)
    {
      fbState.attachments.push_back({});

      ResourceId viewid = GetResID(dyn.shadingRateView);

      fbState.attachments.back().view = rm->GetOriginalID(viewid);
      ret.currentPass.framebuffer.attachments[attIdx].resource =
          rm->GetOriginalID(c.m_ImageView[viewid].image);

      fbState.attachments.back().format = MakeResourceFormat(c.m_ImageView[viewid].format);
      fbState.attachments.back().firstMip = c.m_ImageView[viewid].range.baseMipLevel & 0xff;
      fbState.attachments.back().firstSlice = c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
      fbState.attachments.back().numMips = c.m_ImageView[viewid].range.levelCount & 0xff;
      fbState.attachments.back().numSlices = c.m_ImageView[viewid].range.layerCount & 0xffff;

      Convert(fbState.attachments.back().swizzle, c.m_ImageView[viewid].componentMapping);

      rpState.shadingRateAttachment = int32_t(attIdx++);
      rpState.shadingRateTexelSize = {dyn.shadingRateTexelSize.width,
                                      dyn.shadingRateTexelSize.height};
    }
    else
    {
      rpState.shadingRateAttachment = -1;
      rpState.shadingRateTexelSize = {1, 1};
    }

    rpState.multiviews.clear();
    for(uint32_t v = 0; v < 32; v++)
    {
      if(dyn.viewMask & (1 << v))
        rpState.multiviews.push_back(v);
    }
  }
  else if(state.GetRenderPass() != ResourceId())
  {
    // Renderpass
    ret.currentPass.renderpass.dynamic = false;
    ret.currentPass.renderpass.resourceId = rm->GetOriginalID(state.GetRenderPass());
    ret.currentPass.renderpass.subpass = state.subpass;

    ret.currentPass.renderpass.inputAttachments =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].inputAttachments;
    ret.currentPass.renderpass.colorAttachments =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].colorAttachments;
    ret.currentPass.renderpass.resolveAttachments =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].resolveAttachments;
    ret.currentPass.renderpass.depthstencilAttachment =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].depthstencilAttachment;
    ret.currentPass.renderpass.depthstencilResolveAttachment =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].depthstencilResolveAttachment;
    ret.currentPass.renderpass.fragmentDensityAttachment =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].fragmentDensityAttachment;
    ret.currentPass.renderpass.shadingRateAttachment =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].shadingRateAttachment;
    VkExtent2D texelSize =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].shadingRateTexelSize;
    ret.currentPass.renderpass.shadingRateTexelSize = {texelSize.width, texelSize.height};

    ret.currentPass.renderpass.multiviews =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].multiviews;
    ret.currentPass.renderpass.feedbackLoop =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].feedbackLoop;
    ret.currentPass.renderpass.tileOnlyMSAASampleCount =
        c.m_RenderPass[state.GetRenderPass()].subpasses[state.subpass].tileOnlyMSAASampleCount;

    ResourceId fb = state.GetFramebuffer();

    ret.currentPass.framebuffer.resourceId = rm->GetOriginalID(fb);

    if(fb != ResourceId())
    {
      ret.currentPass.framebuffer.width = c.m_Framebuffer[fb].width;
      ret.currentPass.framebuffer.height = c.m_Framebuffer[fb].height;
      ret.currentPass.framebuffer.layers = c.m_Framebuffer[fb].layers;

      ret.currentPass.framebuffer.attachments.resize(c.m_Framebuffer[fb].attachments.size());
      for(size_t i = 0; i < c.m_Framebuffer[fb].attachments.size(); i++)
      {
        ResourceId viewid = state.GetFramebufferAttachments()[i];

        if(viewid != ResourceId())
        {
          ret.currentPass.framebuffer.attachments[i].view = rm->GetOriginalID(viewid);
          ret.currentPass.framebuffer.attachments[i].resource =
              rm->GetOriginalID(c.m_ImageView[viewid].image);

          ret.currentPass.framebuffer.attachments[i].format =
              MakeResourceFormat(c.m_ImageView[viewid].format);
          ret.currentPass.framebuffer.attachments[i].firstMip =
              c.m_ImageView[viewid].range.baseMipLevel & 0xff;
          ret.currentPass.framebuffer.attachments[i].firstSlice =
              c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
          ret.currentPass.framebuffer.attachments[i].numMips =
              c.m_ImageView[viewid].range.levelCount & 0xff;
          ret.currentPass.framebuffer.attachments[i].numSlices =
              c.m_ImageView[viewid].range.layerCount & 0xffff;

          Convert(ret.currentPass.framebuffer.attachments[i].swizzle,
                  c.m_ImageView[viewid].componentMapping);
        }
        else
        {
          ret.currentPass.framebuffer.attachments[i].view = ResourceId();
          ret.currentPass.framebuffer.attachments[i].resource = ResourceId();

          ret.currentPass.framebuffer.attachments[i].firstMip = 0;
          ret.currentPass.framebuffer.attachments[i].firstSlice = 0;
          ret.currentPass.framebuffer.attachments[i].numMips = 1;
          ret.currentPass.framebuffer.attachments[i].numSlices = 1;
        }
      }
    }
    else
    {
      ret.currentPass.framebuffer.width = 0;
      ret.currentPass.framebuffer.height = 0;
      ret.currentPass.framebuffer.layers = 0;
    }

    ret.currentPass.renderpass.fragmentDensityOffsets.resize(state.fragmentDensityMapOffsets.size());
    for(size_t i = 0; i < state.fragmentDensityMapOffsets.size(); i++)
    {
      const VkOffset2D &o = state.fragmentDensityMapOffsets[i];
      ret.currentPass.renderpass.fragmentDensityOffsets[i] = Offset(o.x, o.y);
    }
  }
  else
  {
    ret.currentPass.renderpass.resourceId = ResourceId();
    ret.currentPass.renderpass.subpass = 0;
    ret.currentPass.renderpass.inputAttachments.clear();
    ret.currentPass.renderpass.colorAttachments.clear();
    ret.currentPass.renderpass.resolveAttachments.clear();
    ret.currentPass.renderpass.fragmentDensityOffsets.clear();
    ret.currentPass.renderpass.depthstencilAttachment = -1;
    ret.currentPass.renderpass.depthstencilResolveAttachment = -1;
    ret.currentPass.renderpass.fragmentDensityAttachment = -1;
    ret.currentPass.renderpass.shadingRateAttachment = -1;
    ret.currentPass.renderpass.shadingRateTexelSize = {1, 1};
    ret.currentPass.renderpass.tileOnlyMSAASampleCount = 0;

    ret.currentPass.framebuffer.resourceId = ResourceId();
    ret.currentPass.framebuffer.attachments.clear();
  }

  if(state.GetRenderPass() != ResourceId() || (state.dynamicRendering.active))
  {
    ret.currentPass.renderArea.x = state.renderArea.offset.x;
    ret.currentPass.renderArea.y = state.renderArea.offset.y;
    ret.currentPass.renderArea.width = state.renderArea.extent.width;
    ret.currentPass.renderArea.height = state.renderArea.extent.height;
  }

  ret.currentPass.colorFeedbackAllowed = (state.feedbackAspects & VK_IMAGE_ASPECT_COLOR_BIT) != 0;
  ret.currentPass.depthFeedbackAllowed = (state.feedbackAspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
  ret.currentPass.stencilFeedbackAllowed = (state.feedbackAspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;

  // Descriptor sets
  ret.graphics.descriptorSets.resize(state.graphics.descSets.size());
  ret.compute.descriptorSets.resize(state.compute.descSets.size());

  // store dynamic offsets
  {
    rdcarray<VKPipe::DescriptorSet> *dsts[] = {
        &ret.graphics.descriptorSets,
        &ret.compute.descriptorSets,
    };

    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> *srcs[] = {
        &state.graphics.descSets,
        &state.compute.descSets,
    };

    for(size_t p = 0; p < ARRAY_COUNT(srcs); p++)
    {
      for(size_t i = 0; i < srcs[p]->size(); i++)
      {
        const VulkanStatePipeline::DescriptorAndOffsets &srcData = srcs[p]->at(i);
        ResourceId sourceSet = srcData.descSet;
        const uint32_t *srcOffset = srcData.offsets.begin();
        VKPipe::DescriptorSet &destSet = dsts[p]->at(i);

        destSet.dynamicOffsets.clear();

        if(sourceSet == ResourceId())
          continue;

        destSet.dynamicOffsets.reserve(srcData.offsets.size());

        VKPipe::DynamicOffset dynOffset;

        const WrappedVulkan::DescriptorSetInfo &descSetState =
            m_pDriver->m_DescriptorSetState[sourceSet];
        const DescriptorSetSlot *first =
            descSetState.data.binds.empty() ? NULL : descSetState.data.binds[0];
        for(size_t b = 0; b < descSetState.data.binds.size(); b++)
        {
          const DescSetLayout::Binding &layoutBind =
              c.m_DescSetLayout[descSetState.layout].bindings[b];

          if(layoutBind.layoutDescType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC &&
             layoutBind.layoutDescType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            continue;

          uint64_t descriptorByteOffset = descSetState.data.binds[b] - first;

          // inline UBOs aren't dynamic and variable size can't be used with dynamic buffers, so the
          // count is what it is at definition time
          for(uint32_t a = 0; a < layoutBind.descriptorCount; a++)
          {
            dynOffset.descriptorByteOffset = descriptorByteOffset + a;
            dynOffset.dynamicBufferByteOffset = *srcOffset;
            srcOffset++;

            destSet.dynamicOffsets.push_back(dynOffset);
          }
        }
      }
    }
  }

  {
    rdcarray<VKPipe::DescriptorSet> *dsts[] = {
        &ret.graphics.descriptorSets,
        &ret.compute.descriptorSets,
    };

    const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> *srcs[] = {
        &state.graphics.descSets,
        &state.compute.descSets,
    };

    const VKDynamicShaderFeedback &usage = m_BindlessFeedback.Usage[eventId];

    ret.shaderMessages = usage.messages;

    for(size_t p = 0; p < ARRAY_COUNT(srcs); p++)
    {
      for(size_t i = 0; i < srcs[p]->size(); i++)
      {
        ResourceId sourceSet = (*srcs[p])[i].descSet;
        VKPipe::DescriptorSet &destSet = (*dsts[p])[i];

        if(sourceSet == ResourceId())
        {
          destSet.descriptorSetResourceId = ResourceId();
          destSet.pushDescriptor = false;
          destSet.layoutResourceId = ResourceId();
          continue;
        }

        ResourceId layoutId = m_pDriver->m_DescriptorSetState[sourceSet].layout;

        destSet.descriptorSetResourceId = rm->GetOriginalID(sourceSet);
        destSet.pushDescriptor = (c.m_DescSetLayout[layoutId].flags &
                                  VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

        destSet.layoutResourceId = rm->GetOriginalID(layoutId);
      }
    }
  }

  // image layouts
  {
    size_t i = 0;
    ret.images.resize(m_pDriver->m_ImageStates.size());
    for(auto it = m_pDriver->m_ImageStates.begin(); it != m_pDriver->m_ImageStates.end(); ++it)
    {
      VKPipe::ImageData &img = ret.images[i];

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

    ret.images.resize(i);
  }

  if(state.conditionalRendering.buffer != ResourceId())
  {
    ret.conditionalRendering.bufferId = rm->GetOriginalID(state.conditionalRendering.buffer);
    ret.conditionalRendering.byteOffset = state.conditionalRendering.offset;
    ret.conditionalRendering.isInverted =
        state.conditionalRendering.flags == VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT;

    bytebuf data;
    GetBufferData(state.conditionalRendering.buffer, state.conditionalRendering.offset,
                  sizeof(uint32_t), data);

    uint32_t value;
    memcpy(&value, data.data(), sizeof(uint32_t));

    ret.conditionalRendering.isPassing = value != 0;

    if(ret.conditionalRendering.isInverted)
      ret.conditionalRendering.isPassing = !ret.conditionalRendering.isPassing;
  }
}

void VulkanReplay::FillSamplerDescriptor(SamplerDescriptor &dstel, const DescriptorSetSlot &srcel)
{
  VulkanResourceManager *rm = m_pDriver->GetResourceManager();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  if(srcel.type == DescriptorSlotType::Sampler)
    dstel.type = DescriptorType::Sampler;
  else if(srcel.type == DescriptorSlotType::CombinedImageSampler)
    dstel.type = DescriptorType::ImageSampler;
  else
    return;

  if(srcel.sampler == ResourceId())
    return;

  const VulkanCreationInfo::Sampler &sampl = c.m_Sampler[srcel.sampler];

  dstel.object = rm->GetOriginalID(srcel.sampler);

  // sampler info
  dstel.filter = MakeFilter(sampl.minFilter, sampl.magFilter, sampl.mipmapMode,
                            sampl.maxAnisotropy >= 1.0f, sampl.compareEnable, sampl.reductionMode);
  dstel.addressU = MakeAddressMode(sampl.address[0]);
  dstel.addressV = MakeAddressMode(sampl.address[1]);
  dstel.addressW = MakeAddressMode(sampl.address[2]);
  dstel.mipBias = sampl.mipLodBias;
  dstel.maxAnisotropy = sampl.maxAnisotropy;
  dstel.compareFunction = MakeCompareFunc(sampl.compareOp);
  dstel.minLOD = sampl.minLod;
  dstel.maxLOD = sampl.maxLod;
  MakeBorderColor(sampl.borderColor, dstel.borderColorValue.floatValue);
  dstel.borderColorType = CompType::Float;
  dstel.unnormalized = sampl.unnormalizedCoordinates;
  dstel.seamlessCubemaps = sampl.seamless;

  // immutable samplers set the offset to non-zero so that we can check it here without knowing what
  // layout this descriptor binding came from
  dstel.creationTimeConstant = srcel.offset != 0;

  if(sampl.ycbcr != ResourceId())
  {
    const VulkanCreationInfo::YCbCrSampler &ycbcr = c.m_YCbCrSampler[sampl.ycbcr];
    dstel.ycbcrSampler = rm->GetOriginalID(sampl.ycbcr);

    dstel.ycbcrModel = ycbcr.ycbcrModel;
    dstel.ycbcrRange = ycbcr.ycbcrRange;
    Convert(dstel.swizzle, ycbcr.componentMapping);
    dstel.xChromaOffset = ycbcr.xChromaOffset;
    dstel.yChromaOffset = ycbcr.yChromaOffset;
    dstel.chromaFilter = ycbcr.chromaFilter;
    dstel.forceExplicitReconstruction = ycbcr.forceExplicitReconstruction;
  }
  else
  {
    Convert(dstel.swizzle, sampl.componentMapping);
    dstel.srgbBorder = sampl.srgbBorder;
  }

  if(sampl.customBorder)
  {
    if(sampl.borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT)
    {
      dstel.borderColorValue.uintValue = sampl.customBorderColor.uint32;
      dstel.borderColorType = CompType::UInt;
    }
    else
    {
      dstel.borderColorValue.floatValue = sampl.customBorderColor.float32;
    }
  }
}

void VulkanReplay::FillDescriptor(Descriptor &dstel, const DescriptorSetSlot &srcel)
{
  DescriptorSlotType descriptorType = srcel.type;

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();
  VulkanCreationInfo &c = m_pDriver->m_CreationInfo;

  switch(descriptorType)
  {
    case DescriptorSlotType::Sampler: dstel.type = DescriptorType::Sampler; break;
    case DescriptorSlotType::CombinedImageSampler: dstel.type = DescriptorType::ImageSampler; break;
    case DescriptorSlotType::SampledImage: dstel.type = DescriptorType::Image; break;
    case DescriptorSlotType::StorageImage: dstel.type = DescriptorType::ReadWriteImage; break;
    case DescriptorSlotType::UniformTexelBuffer: dstel.type = DescriptorType::TypedBuffer; break;
    case DescriptorSlotType::StorageTexelBuffer:
      dstel.type = DescriptorType::ReadWriteTypedBuffer;
      break;
    case DescriptorSlotType::UniformBuffer: dstel.type = DescriptorType::ConstantBuffer; break;
    case DescriptorSlotType::StorageBuffer: dstel.type = DescriptorType::ReadWriteBuffer; break;
    case DescriptorSlotType::UniformBufferDynamic:
      dstel.type = DescriptorType::ConstantBuffer;
      break;
    case DescriptorSlotType::StorageBufferDynamic:
      dstel.type = DescriptorType::ReadWriteBuffer;
      break;
    case DescriptorSlotType::AccelerationStructure:
      dstel.type = DescriptorType::AccelerationStructure;
      break;
    case DescriptorSlotType::InputAttachment: dstel.type = DescriptorType::Image; break;
    case DescriptorSlotType::InlineBlock: dstel.type = DescriptorType::ConstantBuffer; break;
    case DescriptorSlotType::Unwritten:
    case DescriptorSlotType::Count: dstel.type = DescriptorType::Unknown; break;
  }

  // now look at the 'base' type. Sampler is excluded from these ifs
  if(descriptorType == DescriptorSlotType::SampledImage ||
     descriptorType == DescriptorSlotType::CombinedImageSampler ||
     descriptorType == DescriptorSlotType::InputAttachment ||
     descriptorType == DescriptorSlotType::StorageImage)
  {
    ResourceId viewid = srcel.resource;

    if(descriptorType == DescriptorSlotType::CombinedImageSampler)
    {
      dstel.secondary = rm->GetOriginalID(srcel.sampler);
    }

    if(viewid != ResourceId())
    {
      dstel.view = rm->GetOriginalID(viewid);
      dstel.resource = rm->GetOriginalID(c.m_ImageView[viewid].image);
      dstel.format = MakeResourceFormat(c.m_ImageView[viewid].format);

      Convert(dstel.swizzle, c.m_ImageView[viewid].componentMapping);
      dstel.firstMip = c.m_ImageView[viewid].range.baseMipLevel & 0xff;
      dstel.firstSlice = c.m_ImageView[viewid].range.baseArrayLayer & 0xffff;
      dstel.numMips = c.m_ImageView[viewid].range.levelCount & 0xff;
      dstel.numSlices = c.m_ImageView[viewid].range.layerCount & 0xffff;

      if(c.m_ImageView[viewid].viewType == VK_IMAGE_VIEW_TYPE_3D)
        dstel.firstSlice = dstel.numSlices = 0;

      // cheeky hack, store image layout enum in byteOffset as it's not used for images
      dstel.byteOffset = convert(srcel.imageLayout);

      dstel.minLODClamp = c.m_ImageView[viewid].minLOD;
    }
    else
    {
      dstel.view = ResourceId();
      dstel.resource = ResourceId();
      dstel.firstMip = 0;
      dstel.firstSlice = 0;
      dstel.numMips = 1;
      dstel.numSlices = 1;
      dstel.minLODClamp = 0.0f;
    }
  }
  else if(descriptorType == DescriptorSlotType::UniformTexelBuffer ||
          descriptorType == DescriptorSlotType::StorageTexelBuffer)
  {
    ResourceId viewid = srcel.resource;

    if(viewid != ResourceId())
    {
      dstel.view = rm->GetOriginalID(viewid);
      dstel.resource = rm->GetOriginalID(c.m_BufferView[viewid].buffer);
      dstel.byteOffset = c.m_BufferView[viewid].offset;
      dstel.format = MakeResourceFormat(c.m_BufferView[viewid].format);
      dstel.byteSize = c.m_BufferView[viewid].size;
    }
    else
    {
      dstel.view = ResourceId();
      dstel.resource = ResourceId();
      dstel.byteOffset = 0;
      dstel.byteSize = 0;
    }
  }
  else if(descriptorType == DescriptorSlotType::InlineBlock)
  {
    dstel.view = ResourceId();
    dstel.resource = ResourceId();
    dstel.byteOffset = srcel.offset;
    dstel.byteSize = srcel.range;
    dstel.flags = DescriptorFlags::InlineData;
  }
  else if(descriptorType == DescriptorSlotType::StorageBuffer ||
          descriptorType == DescriptorSlotType::StorageBufferDynamic ||
          descriptorType == DescriptorSlotType::UniformBuffer ||
          descriptorType == DescriptorSlotType::UniformBufferDynamic)
  {
    dstel.view = ResourceId();

    if(srcel.resource != ResourceId())
      dstel.resource = rm->GetOriginalID(srcel.resource);

    dstel.byteOffset = srcel.offset;
    dstel.byteSize = srcel.GetRange();
  }
  else if(descriptorType == DescriptorSlotType::AccelerationStructure)
  {
    dstel.view = ResourceId();

    if(srcel.resource != ResourceId())
    {
      dstel.resource = rm->GetOriginalID(srcel.resource);
      dstel.byteSize = c.m_AccelerationStructure[srcel.resource].size;
    }
  }
}

rdcarray<Descriptor> VulkanReplay::GetDescriptors(ResourceId descriptorStore,
                                                  const rdcarray<DescriptorRange> &ranges)
{
  if(descriptorStore == ResourceId())
    return {};

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  rdcarray<Descriptor> ret;
  ret.resize(count);

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  // specialisation constants 'descriptor' stored in a pipeline or shader object
  auto pipe = m_pDriver->m_CreationInfo.m_Pipeline.find(descriptorStore);
  auto shad = m_pDriver->m_CreationInfo.m_ShaderObject.find(descriptorStore);
  bool isShader = shad != m_pDriver->m_CreationInfo.m_ShaderObject.end();
  if(pipe != m_pDriver->m_CreationInfo.m_Pipeline.end() || isShader)
  {
    // should only be one descriptor referred here, but just munge them all to be the same
    for(Descriptor &d : ret)
    {
      d.type = DescriptorType::ConstantBuffer;
      d.flags = DescriptorFlags::InlineData;
      d.view = ResourceId();
      d.resource = rm->GetOriginalID(descriptorStore);
      // specialisation constants implicitly always view the whole data, the shader reflection
      // offsets are absolute (by specialisation ID)
      d.byteOffset = 0;
      d.byteSize = isShader ? shad->second.virtualSpecialisationByteSize
                            : pipe->second.virtualSpecialisationByteSize;
    }

    return ret;
  }

  // push constants 'descriptor' stored in a command buffer
  if(WrappedVkCommandBuffer::IsAlloc(rm->GetCurrentResource(descriptorStore)))
  {
    const VulkanRenderState &state = m_pDriver->m_RenderState;

    // should only be one descriptor referred here, but just munge them all to be the same
    for(Descriptor &d : ret)
    {
      d.type = DescriptorType::ConstantBuffer;
      d.flags = DescriptorFlags::InlineData;
      d.view = ResourceId();
      d.resource = rm->GetOriginalID(descriptorStore);
      // push constants also implicitly always view the whole data, since the ranges specified in
      // the pipeline must match offsets declared in the shader
      d.byteOffset = 0;
      // we don't verify that the current command buffer is the one being requested - since push
      // constants are not valid outside of the current event. We just pretend that all push
      // constants are the same and mutable
      d.byteSize = state.pushConstSize;
    }

    return ret;
  }

  auto descit = m_pDriver->m_DescriptorSetState.find(descriptorStore);
  if(descit == m_pDriver->m_DescriptorSetState.end())
  {
    RDCERR("Invalid/unrecognised descriptor store %s", ToStr(descriptorStore).c_str());
    return ret;
  }

  const WrappedVulkan::DescriptorSetInfo &set = descit->second;

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    const DescriptorSetSlot *desc = set.data.binds.empty() ? NULL : set.data.binds[0];
    const DescriptorSetSlot *end = desc + set.data.totalDescriptorCount();

    desc += (r.offset - set.data.inlineBytes.size());

    for(uint32_t i = 0; i < r.count; i++)
    {
      if(desc >= end)
      {
        // silently drop out of bounds descriptor reads
      }
      else if(desc->type == DescriptorSlotType::Sampler)
      {
        ret[dst].type = DescriptorType::Sampler;
      }
      else
      {
        FillDescriptor(ret[dst], *desc);

        if(ret[dst].flags & DescriptorFlags::InlineData)
        {
          // inline data stored in the descriptor set
          ret[dst].resource = rm->GetOriginalID(descriptorStore);
        }
      }

      dst++;
      desc++;
    }
  }

  return ret;
}

rdcarray<SamplerDescriptor> VulkanReplay::GetSamplerDescriptors(ResourceId descriptorStore,
                                                                const rdcarray<DescriptorRange> &ranges)
{
  if(descriptorStore == ResourceId())
    return {};

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  rdcarray<SamplerDescriptor> ret;
  ret.resize(count);

  // specialisation constants 'descriptor' stored in a pipeline or shader object
  if(m_pDriver->m_CreationInfo.m_Pipeline.find(descriptorStore) !=
         m_pDriver->m_CreationInfo.m_Pipeline.end() ||
     m_pDriver->m_CreationInfo.m_ShaderObject.find(descriptorStore) !=
         m_pDriver->m_CreationInfo.m_ShaderObject.end())
  {
    // not sampler data
    return ret;
  }

  // push constants 'descriptor' stored in a command buffer
  if(WrappedVkCommandBuffer::IsAlloc(GetResourceManager()->GetCurrentResource(descriptorStore)))
  {
    // not sampler data
    return ret;
  }

  auto descit = m_pDriver->m_DescriptorSetState.find(descriptorStore);
  if(descit == m_pDriver->m_DescriptorSetState.end())
  {
    RDCERR("Invalid/unrecognised descriptor store %s", ToStr(descriptorStore).c_str());
    return ret;
  }

  const WrappedVulkan::DescriptorSetInfo &set = descit->second;

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    const DescriptorSetSlot *desc = set.data.binds.empty() ? NULL : set.data.binds[0];
    const DescriptorSetSlot *end = desc + set.data.totalDescriptorCount();

    desc += r.offset;

    for(uint32_t i = 0; i < r.count; i++)
    {
      if(desc >= end)
      {
        // silently drop out of bounds descriptor reads
      }
      else if(desc->type == DescriptorSlotType::Sampler ||
              desc->type == DescriptorSlotType::CombinedImageSampler)
      {
        FillSamplerDescriptor(ret[dst], *desc);
      }

      dst++;
      desc++;
    }
  }

  return ret;
}

rdcarray<DescriptorAccess> VulkanReplay::GetDescriptorAccess(uint32_t eventId)
{
  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  const VulkanRenderState &state = m_pDriver->m_RenderState;

  rdcarray<DescriptorAccess> ret;

  if(state.graphics.pipeline != ResourceId())
    ret.append(m_pDriver->m_CreationInfo.m_Pipeline[state.graphics.pipeline].staticDescriptorAccess);

  if(state.compute.pipeline != ResourceId())
    ret.append(m_pDriver->m_CreationInfo.m_Pipeline[state.compute.pipeline].staticDescriptorAccess);

  if(state.graphics.shaderObject)
  {
    for(uint32_t i = 0; i < (uint32_t)ShaderStage::Count; i++)
    {
      if(i == (uint32_t)ShaderStage::Compute)
        continue;
      ResourceId shadid = state.shaderObjects[i];
      if(shadid != ResourceId())
        ret.append(m_pDriver->m_CreationInfo.m_ShaderObject[shadid].staticDescriptorAccess);
    }
  }

  if(state.compute.shaderObject && state.shaderObjects[(uint32_t)ShaderStage::Compute] != ResourceId())
    ret.append(m_pDriver->m_CreationInfo
                   .m_ShaderObject[state.shaderObjects[(uint32_t)ShaderStage::Compute]]
                   .staticDescriptorAccess);

  for(DescriptorAccess &access : ret)
  {
    uint32_t bindset = (uint32_t)access.byteSize;
    access.byteSize = 1;
    if(access.descriptorStore == m_pDriver->m_CreationInfo.pushConstantDescriptorStorage)
    {
      access.descriptorStore = m_pDriver->GetPushConstantCommandBuffer();
    }
    else if(access.descriptorStore == ResourceId())
    {
      const rdcarray<VulkanStatePipeline::DescriptorAndOffsets> &descSets =
          access.stage == ShaderStage::Compute ? state.compute.descSets : state.graphics.descSets;

      if(bindset >= descSets.size())
      {
        RDCERR("Unbound descriptor set referenced in static usage");
      }
      else
      {
        access.descriptorStore = rm->GetOriginalID(descSets[bindset].descSet);
      }
    }

    if(access.descriptorStore == ResourceId())
      access = DescriptorAccess();
  }

  const VKDynamicShaderFeedback &usage = m_BindlessFeedback.Usage[eventId];

  if(usage.valid)
    ret.append(usage.access);

  // remove any invalid accesses
  ret.removeIf([](const DescriptorAccess &access) { return access.descriptorStore == ResourceId(); });

  return ret;
}

rdcarray<DescriptorLogicalLocation> VulkanReplay::GetDescriptorLocations(
    ResourceId descriptorStore, const rdcarray<DescriptorRange> &ranges)
{
  rdcarray<DescriptorLogicalLocation> ret;

  size_t count = 0;
  for(const DescriptorRange &r : ranges)
    count += r.count;
  ret.resize(count);

  // specialisation constants 'descriptor' stored in a pipeline or shader object
  auto pipe = m_pDriver->m_CreationInfo.m_Pipeline.find(descriptorStore);
  auto shad = m_pDriver->m_CreationInfo.m_ShaderObject.find(descriptorStore);
  if(pipe != m_pDriver->m_CreationInfo.m_Pipeline.end() ||
     shad != m_pDriver->m_CreationInfo.m_ShaderObject.end())
  {
    // should only be one descriptor referred here, but just munge them all to be the same
    for(DescriptorLogicalLocation &d : ret)
    {
      d.category = DescriptorCategory::ConstantBlock;
      d.fixedBindNumber = ~0U - 2;
      d.logicalBindName = "Specialization";
    }

    return ret;
  }

  VulkanResourceManager *rm = m_pDriver->GetResourceManager();

  // push constants 'descriptor' stored in a command buffer
  if(WrappedVkCommandBuffer::IsAlloc(rm->GetCurrentResource(descriptorStore)))
  {
    // should only be one descriptor referred here, but just munge them all to be the same
    for(DescriptorLogicalLocation &d : ret)
    {
      d.category = DescriptorCategory::ConstantBlock;
      d.fixedBindNumber = ~0U - 1;
      d.logicalBindName = "Push constants";
    }

    return ret;
  }

  auto descit = m_pDriver->m_DescriptorSetState.find(descriptorStore);
  if(descit == m_pDriver->m_DescriptorSetState.end())
  {
    RDCERR("Invalid/unrecognised descriptor store %s", ToStr(descriptorStore).c_str());
    return ret;
  }

  const WrappedVulkan::DescriptorSetInfo &descState = descit->second;
  uint32_t varDescCount = descState.data.variableDescriptorCount;

  const DescSetLayout &descLayout = m_pDriver->m_CreationInfo.m_DescSetLayout[descState.layout];

  size_t dst = 0;
  for(const DescriptorRange &r : ranges)
  {
    uint32_t descriptorOffset = r.offset;

    const DescSetLayout::Binding *bind = descLayout.bindings.data();
    const DescSetLayout::Binding *firstBind = bind;
    const DescSetLayout::Binding *lastBind = bind + descLayout.bindings.size();

    for(uint32_t i = 0; i < r.count; i++, dst++, descriptorOffset++)
    {
      while(bind < lastBind &&
            descLayout.inlineByteSize + bind->elemOffset + bind->GetDescriptorCount(varDescCount) <=
                descriptorOffset)
        bind++;

      if(bind >= lastBind)
      {
        RDCERR("Ran off end of descriptor layout looking for matching offset");
        break;
      }

      DescriptorLogicalLocation &d = ret[dst];

      const DescriptorSetSlot *slot = descState.data.binds[0] + descriptorOffset;

      switch(slot->type)
      {
        case DescriptorSlotType::Sampler: d.category = DescriptorCategory::Sampler; break;
        case DescriptorSlotType::UniformBuffer:
        case DescriptorSlotType::InlineBlock:
        case DescriptorSlotType::UniformBufferDynamic:
        case DescriptorSlotType::SampledImage:
        case DescriptorSlotType::CombinedImageSampler:
        case DescriptorSlotType::UniformTexelBuffer:
        case DescriptorSlotType::InputAttachment:
        case DescriptorSlotType::AccelerationStructure:
          d.category = DescriptorCategory::ReadOnlyResource;
          break;
        case DescriptorSlotType::StorageBuffer:
        case DescriptorSlotType::StorageBufferDynamic:
        case DescriptorSlotType::StorageImage:
        case DescriptorSlotType::StorageTexelBuffer:
          d.category = DescriptorCategory::ReadWriteResource;
          break;
        case DescriptorSlotType::Unwritten:
        case DescriptorSlotType::Count: d.category = DescriptorCategory::Unknown; break;
      }

      if(bind->stageFlags == VK_SHADER_STAGE_ALL)
        d.stageMask = ShaderStageMask::All;
      else
        d.stageMask = (ShaderStageMask)bind->stageFlags;
      // we only have one bind number, for simplicity, so we put the bind here and omit the array
      // element entirely. Users that want to decode this are expected to either be aware of arrays
      // and determine that contiguous identical bind numbers are arrays, or display with the
      // logical name string below
      d.fixedBindNumber = uint32_t(bind - firstBind);
      if(bind->descriptorCount > 1 && bind->layoutDescType != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        d.logicalBindName = StringFormat::Fmt("%zu[%u]", size_t(bind - firstBind),
                                              descriptorOffset - bind->elemOffset);
      else
        d.logicalBindName = StringFormat::Fmt("%zu", size_t(bind - firstBind));
    }
  }

  return ret;
}

void VulkanReplay::FillCBufferVariables(ResourceId pipeline, ResourceId shader, ShaderStage stage,
                                        rdcstr entryPoint, uint32_t cbufSlot,
                                        rdcarray<ShaderVariable> &outvars, const bytebuf &data)
{
  auto it = m_pDriver->m_CreationInfo.m_ShaderModule.find(shader);

  if(it == m_pDriver->m_CreationInfo.m_ShaderModule.end())
  {
    RDCERR("Can't get shader details");
    return;
  }

  ShaderReflection &refl = *it->second.GetReflection(stage, entryPoint, pipeline).refl;

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

    if(c.fixedBindSetOrSpace < descSets.size())
    {
      ResourceId set = descSets[c.fixedBindSetOrSpace].descSet;

      const WrappedVulkan::DescriptorSetInfo &setData = m_pDriver->m_DescriptorSetState[set];

      ResourceId layoutId = setData.layout;

      if(c.fixedBindNumber < m_pDriver->m_CreationInfo.m_DescSetLayout[layoutId].bindings.size())
      {
        const DescSetLayout::Binding &layoutBind =
            m_pDriver->m_CreationInfo.m_DescSetLayout[layoutId].bindings[c.fixedBindNumber];

        if(layoutBind.layoutDescType == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
        {
          bytebuf inlineData;
          inlineData.assign(
              setData.data.inlineBytes.data() + setData.data.binds[c.fixedBindNumber]->offset,
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
    if(c.compileConstants)
    {
      auto pipeIt = m_pDriver->m_CreationInfo.m_Pipeline.find(pipeline);

      if(pipeIt != m_pDriver->m_CreationInfo.m_Pipeline.end())
      {
        const VulkanCreationInfo::ShaderModuleReflection &reflection =
            it->second.GetReflection(stage, entryPoint, pipeline);
        const rdcarray<SpecConstant> &specInfo =
            pipeIt->second.shaders[reflection.stageIndex].specialization;

        FillSpecConstantVariables(refl.resourceId, reflection.patchData, c.variables, outvars,
                                  specInfo);
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

      uint32_t mipWidth = RDCMAX(1U, iminfo.extent.width >> sub.mip);
      uint32_t mipHeight = RDCMAX(1U, iminfo.extent.height >> sub.mip);

      texDisplay.xOffset = -(float(x) / float(mipWidth)) * iminfo.extent.width;
      texDisplay.yOffset = -(float(y) / float(mipHeight)) * iminfo.extent.height;

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
               0,
               0,
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

    if(cmd == VK_NULL_HANDLE)
      return;

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
      CheckVkResult(vkr);

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
    vkr = vt->MapMemory(Unwrap(dev), Unwrap(m_PixelPick.ReadbackBuffer.mem), 0, VK_WHOLE_SIZE, 0,
                        (void **)&pData);
    CheckVkResult(vkr);
    if(vkr != VK_SUCCESS)
      return;
    if(!pData)
    {
      RDCERR("Manually reporting failed memory map");
      CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
      return;
    }

    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        NULL,
        Unwrap(m_PixelPick.ReadbackBuffer.mem),
        0,
        VK_WHOLE_SIZE,
    };

    vkr = vt->InvalidateMappedMemoryRanges(Unwrap(dev), 1, &range);
    CheckVkResult(vkr);

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
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
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
  if(!data)
    return false;

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

  if(cmd == VK_NULL_HANDLE)
    return false;

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

    if(cmd == VK_NULL_HANDLE)
      return false;
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
      0,
      0,
      m_Histogram.m_MinMaxResult.totalsize,
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
  if(!minmax)
    return false;

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

void VulkanReplay::CheckVkResult(VkResult vkr)
{
  return m_pDriver->CheckVkResult(vkr);
}

bool VulkanReplay::GetHistogram(ResourceId texid, const Subresource &sub, CompType typeCast,
                                float minval, float maxval, const rdcfixedarray<bool, 4> &channels,
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
  if(!data)
    return false;

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

  // shuffle the channel selection, since stencil comes back in red
  if(stencil)
    chans = 0x1;

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

  if(cmd == VK_NULL_HANDLE)
    return false;

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

    if(cmd == VK_NULL_HANDLE)
      return false;
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
      0,
      0,
      m_Histogram.m_HistogramBuf.totalsize,
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
  if(!buckets)
    return false;

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
  bool copyToBuffer = true;

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
  bool isPlanar = (imageAspects & VK_IMAGE_ASPECT_PLANE_0_BIT) != 0;
  uint32_t planeCount = GetYUVPlaneCount(imInfo.format);

  VkImage liveWrappedImage = GetResourceManager()->GetCurrentHandle<VkImage>(tex);

  VkImage srcImage = Unwrap(liveWrappedImage);
  VkImage tmpImage = VK_NULL_HANDLE;
  VkImage wrappedTmpImage = VK_NULL_HANDLE;
  VkDeviceMemory tmpMemory = VK_NULL_HANDLE;

  VkFramebuffer *tmpFB = NULL;
  VkImageView *tmpView = NULL;
  uint32_t numFBs = 0;
  VkRenderPass tmpRP = VK_NULL_HANDLE;
  VkRenderPass tmpRPStencil = VK_NULL_HANDLE;

  VkDevice dev = m_pDriver->GetDev();
  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  const VkDevDispatchTable *vt = ObjDisp(dev);

  if(cmd == VK_NULL_HANDLE)
    return;

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  VkResult vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  CheckVkResult(vkr);

  size_t dataSize = 0;
  VkBuffer readbackBuf = VK_NULL_HANDLE;
  VkDeviceMemory readbackMem = VK_NULL_HANDLE;

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
      if(IsSRGBFormat(imCreateInfo.format))
      {
        imCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        renderFlags |= eTexDisplay_RemapSRGB;
      }
      else
      {
        imCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
      }
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

    imCreateInfo.format = GetViewCastedFormat(imCreateInfo.format, BaseRemapType(params));

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
    if(IsStencilFormat(imInfo.format))
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

    NameVulkanObject(wrappedTmpImage, "GetTextureData tmpImage");

    VkMemoryRequirements mrq = {0};
    vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
    CheckVkResult(vkr);

    if(vkr != VK_SUCCESS)
      return;

    vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
    CheckVkResult(vkr);

    tmpImageState.InlineTransition(
        cmd, m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, m_pDriver->GetImageTransitionInfo());

    // end this command buffer, the rendertexture below will use its own and we want to ensure
    // ordering
    vt->EndCommandBuffer(Unwrap(cmd));

    if(Vulkan_Debug_SingleSubmitFlushing())
      m_pDriver->SubmitCmds();

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
    if(IsStencilFormat(imInfo.format))
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
              VK_IMAGE_ASPECT_COLOR_BIT,
              0,
              VK_REMAINING_MIP_LEVELS,
              i,
              1,
          },
      };

      vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, NULL, &tmpView[i]);
      CheckVkResult(vkr);

      NameUnwrappedVulkanObject(tmpView[i], "GetTextureData tmpView[i]");

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
      CheckVkResult(vkr);

      VkClearValue clearval = {};
      VkRenderPassBeginInfo rpbegin = {
          VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          NULL,
          tmpRP,
          tmpFB[i],
          {{
               0,
               0,
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

        attDesc.format = viewInfo.format;
        vkr = vt->CreateRenderPass(Unwrap(dev), &rpinfo, NULL, &tmpRPStencil);
        CheckVkResult(vkr);
        fbinfo.renderPass = tmpRPStencil;
        rpbegin.renderPass = tmpRPStencil;

        vkr = vt->CreateImageView(Unwrap(dev), &viewInfo, NULL, &tmpView[i + numFBs]);
        CheckVkResult(vkr);
        NameUnwrappedVulkanObject(tmpView[i + numFBs], "GetTextureData tmpView[i]");
        fbinfo.pAttachments = &tmpView[i + numFBs];
        vkr = vt->CreateFramebuffer(Unwrap(dev), &fbinfo, NULL, &tmpFB[i + numFBs]);
        CheckVkResult(vkr);
        rpbegin.framebuffer = tmpFB[i + numFBs];

        int stencilFlags = renderFlags;
        stencilFlags &= ~eTexDisplay_RemapFloat;
        stencilFlags &= ~eTexDisplay_RemapSRGB;
        stencilFlags |= eTexDisplay_RemapUInt | eTexDisplay_GreenOnly;

        texDisplay.red = texDisplay.blue = texDisplay.alpha = false;

        // S8 renders into red
        if(IsStencilOnlyFormat(imInfo.format))
        {
          texDisplay.red = true;
          texDisplay.green = false;
          stencilFlags &= ~eTexDisplay_GreenOnly;
        }

        RenderTextureInternal(texDisplay, *srcImageState, rpbegin, stencilFlags);
        renderCount++;
      }
    }

    m_DebugWidth = oldW;
    m_DebugHeight = oldH;

    srcImage = tmpImage;
    srcImageState = &tmpImageState;

    // fetch a new command buffer for copy & readback
    cmd = m_pDriver->GetNextCmd();

    if(cmd == VK_NULL_HANDLE)
      return;

    vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    CheckVkResult(vkr);

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

    NameVulkanObject(wrappedTmpImage, "GetTextureData tmpImage");

    VkMemoryRequirements mrq = {0};
    vt->GetImageMemoryRequirements(Unwrap(dev), tmpImage, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetGPULocalMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &tmpMemory);
    CheckVkResult(vkr);

    if(vkr != VK_SUCCESS)
      return;

    vkr = vt->BindImageMemory(Unwrap(dev), tmpImage, tmpMemory, 0);
    CheckVkResult(vkr);

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
      CheckVkResult(vkr);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);

      // fetch a new command buffer for remaining work
      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return;

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);
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
    dataSize = (size_t)GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth,
                                   imCreateInfo.format, s.mip);

    // buffer size needs to be align to the int for shader writing
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        AlignUp(dataSize, (size_t)4U),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    vkr = vt->CreateBuffer(Unwrap(dev), &bufInfo, NULL, &readbackBuf);
    CheckVkResult(vkr);

    VkMemoryRequirements mrq = {0};

    vt->GetBufferMemoryRequirements(Unwrap(dev), readbackBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };
    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &readbackMem);
    CheckVkResult(vkr);

    if(vkr != VK_SUCCESS)
      return;

    vkr = vt->BindBufferMemory(Unwrap(dev), readbackBuf, readbackMem, 0);
    CheckVkResult(vkr);

    // copy/expand multisampled live texture to readback buffer
    ImageBarrierSequence setupBarriers, cleanupBarriers;
    srcImageState->TempTransition(m_pDriver->m_QueueFamilyIdx,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_ACCESS_SHADER_READ_BIT, setupBarriers, cleanupBarriers,
                                  m_pDriver->GetImageTransitionInfo());
    m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
    m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);

    GetDebugManager()->CopyTex2DMSToBuffer(cmd, readbackBuf, srcImage, imCreateInfo.extent, s.slice,
                                           1, s.sample, 1, imCreateInfo.format);

    m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);

    if(!cleanupBarriers.empty())
    {
      // ensure this resolve happens before handing back the source image to the original queue
      vkr = vt->EndCommandBuffer(Unwrap(cmd));
      CheckVkResult(vkr);

      m_pDriver->SubmitCmds();
      m_pDriver->FlushQ();

      m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);

      // fetch a new command buffer for remaining work
      cmd = m_pDriver->GetNextCmd();

      if(cmd == VK_NULL_HANDLE)
        return;

      vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      CheckVkResult(vkr);
    }

    // readback buffer has already been populated, no need to call CmdCopyImageToBuffer
    copyToBuffer = false;
  }

  VkDeviceSize stencilOffset = 0;
  // if we have no tmpImage, we're copying directly from the real image
  if(copyToBuffer)
  {
    ImageBarrierSequence cleanupBarriers;
    if(tmpImage == VK_NULL_HANDLE)
    {
      ImageBarrierSequence setupBarriers;
      srcImageState->TempTransition(m_pDriver->m_QueueFamilyIdx, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    VK_ACCESS_TRANSFER_READ_BIT, setupBarriers, cleanupBarriers,
                                    m_pDriver->GetImageTransitionInfo());
      m_pDriver->InlineSetupImageBarriers(cmd, setupBarriers);
      m_pDriver->SubmitAndFlushImageStateBarriers(setupBarriers);
    }

    rdcarray<VkBufferImageCopy> copyregions;

    VkBufferImageCopy copyRegionTemplate = {
        0,
        0,
        0,
        {VK_IMAGE_ASPECT_NONE, s.mip, s.slice, 1},
        {
            0,
            0,
            0,
        },
        {RDCMAX(1U, imCreateInfo.extent.width >> s.mip),
         RDCMAX(1U, imCreateInfo.extent.height >> s.mip),
         RDCMAX(1U, imCreateInfo.extent.depth >> s.mip)},
    };

    if(isDepth || isStencil)
    {
      if(isDepth)
      {
        copyRegionTemplate.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        copyregions.push_back(copyRegionTemplate);

        // Stencil offset (if present)
        copyRegionTemplate.bufferOffset = stencilOffset =
            GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth,
                        GetDepthOnlyFormat(imCreateInfo.format), s.mip);
        copyRegionTemplate.bufferOffset = AlignUp(copyRegionTemplate.bufferOffset, (VkDeviceSize)4);
      }

      if(isStencil)
      {
        copyRegionTemplate.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        copyregions.push_back(copyRegionTemplate);
      }
    }
    else if(isPlanar)
    {
      for(uint32_t i = 0; i < planeCount; i++)
      {
        copyRegionTemplate.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT << i;

        VkExtent2D planeExtent =
            GetPlaneShape(RDCMAX(1U, imCreateInfo.extent.width >> s.mip),
                          RDCMAX(1U, imCreateInfo.extent.height >> s.mip), imCreateInfo.format, i);
        copyRegionTemplate.imageExtent.width = planeExtent.width;
        copyRegionTemplate.imageExtent.height = planeExtent.height;

        copyregions.push_back(copyRegionTemplate);

        copyRegionTemplate.bufferOffset +=
            GetPlaneByteSize(imCreateInfo.extent.width, imCreateInfo.extent.height,
                             imCreateInfo.extent.depth, imCreateInfo.format, s.mip, i);
      }
    }
    else
    {
      copyRegionTemplate.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyregions.push_back(copyRegionTemplate);
    }

    dataSize = (size_t)GetByteSize(imInfo.extent.width, imInfo.extent.height, imInfo.extent.depth,
                                   imCreateInfo.format, s.mip);

    if(imCreateInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
    {
      // for most combined depth-stencil images this will be large enough for both to be copied
      // separately, but for D24S8 we need to add extra space since they won't be copied packed
      dataSize = AlignUp(dataSize, (size_t)4U);
      dataSize += (size_t)GetByteSize(imInfo.extent.width, imInfo.extent.height,
                                      imInfo.extent.depth, VK_FORMAT_S8_UINT, s.mip);
    }

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        dataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    vkr = vt->CreateBuffer(Unwrap(dev), &bufInfo, NULL, &readbackBuf);
    CheckVkResult(vkr);

    VkMemoryRequirements mrq = {0};

    vt->GetBufferMemoryRequirements(Unwrap(dev), readbackBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        mrq.size,
        m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };
    vkr = vt->AllocateMemory(Unwrap(dev), &allocInfo, NULL, &readbackMem);
    CheckVkResult(vkr);

    if(vkr != VK_SUCCESS)
      return;

    vkr = vt->BindBufferMemory(Unwrap(dev), readbackBuf, readbackMem, 0);
    CheckVkResult(vkr);

    if(imInfo.type == VK_IMAGE_TYPE_3D && params.remap != RemapTexture::NoRemap)
    {
      // copy in each slice from the 2D array we created to render out the 3D texture
      for(uint32_t i = 0; i < imCreateInfo.arrayLayers; i++)
      {
        copyregions[0].imageSubresource.baseArrayLayer = i;
        copyregions[0].bufferOffset =
            i * GetByteSize(imCreateInfo.extent.width, imCreateInfo.extent.height, 1,
                            imCreateInfo.format, s.mip);
        vt->CmdCopyImageToBuffer(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 readbackBuf, (uint32_t)copyregions.size(), copyregions.data());
      }
    }
    else
    {
      if(imInfo.type == VK_IMAGE_TYPE_3D)
        copyregions[0].imageSubresource.baseArrayLayer = 0;

      // copy from desired subresource in srcImage to buffer
      vt->CmdCopyImageToBuffer(Unwrap(cmd), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readbackBuf, (uint32_t)copyregions.size(), copyregions.data());
    }

    // if we have no tmpImage, we're copying directly from the real image
    if(tmpImage == VK_NULL_HANDLE)
    {
      m_pDriver->InlineCleanupImageBarriers(cmd, cleanupBarriers);

      if(!cleanupBarriers.empty())
      {
        // ensure this resolve happens before handing back the source image to the original queue
        vkr = vt->EndCommandBuffer(Unwrap(cmd));
        CheckVkResult(vkr);

        m_pDriver->SubmitCmds();
        m_pDriver->FlushQ();

        m_pDriver->SubmitAndFlushImageStateBarriers(cleanupBarriers);

        // fetch a new command buffer for remaining work
        cmd = m_pDriver->GetNextCmd();

        if(cmd == VK_NULL_HANDLE)
          return;

        vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        CheckVkResult(vkr);
      }
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
  CheckVkResult(vkr);
  if(vkr != VK_SUCCESS)
    return;
  if(!pData)
  {
    RDCERR("Manually reporting failed memory map");
    CheckVkResult(VK_ERROR_MEMORY_MAP_FAILED);
    return;
  }

  VkMappedMemoryRange range = {
      VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, readbackMem, 0, VK_WHOLE_SIZE,
  };

  vkr = vt->InvalidateMappedMemoryRanges(Unwrap(dev), 1, &range);
  CheckVkResult(vkr);

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
  else if(isDepth && isStencil && copyToBuffer)
  {
    // We only need to manually interleave if we use CmdCopyImageToBuffer.
    // CopyDepthTex2DMS2Buffer will produce interleaved results.
    size_t pixelCount = std::max(1U, imCreateInfo.extent.width >> s.mip) *
                        std::max(1U, imCreateInfo.extent.height >> s.mip) *
                        std::max(1U, imCreateInfo.extent.depth >> s.mip);

    // for some reason reading direct from mapped memory here is *super* slow on android (1.5s to
    // iterate over the image), so we memcpy to a temporary buffer.
    rdcarray<byte> tmp;
    tmp.resize((size_t)stencilOffset + pixelCount * sizeof(uint8_t));
    memcpy(tmp.data(), pData, tmp.size());

    if(imCreateInfo.format == VK_FORMAT_D16_UNORM_S8_UINT)
    {
      uint16_t *dSrc = (uint16_t *)tmp.data();
      uint8_t *sSrc = (uint8_t *)(tmp.data() + stencilOffset);

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
      uint8_t *sSrc = (uint8_t *)(tmp.data() + stencilOffset);

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
      uint8_t *sSrc = (uint8_t *)(tmp.data() + stencilOffset);

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
    if(IsStencilFormat(imInfo.format))
      numFBs *= 2;

    for(uint32_t i = 0; i < numFBs; i++)
    {
      vt->DestroyFramebuffer(Unwrap(dev), tmpFB[i], NULL);
      vt->DestroyImageView(Unwrap(dev), tmpView[i], NULL);
    }
    delete[] tmpFB;
    delete[] tmpView;
    vt->DestroyRenderPass(Unwrap(dev), tmpRP, NULL);
    vt->DestroyRenderPass(Unwrap(dev), tmpRPStencil, NULL);
  }
}

void VulkanReplay::SetCustomShaderIncludes(const rdcarray<rdcstr> &directories)
{
}

void VulkanReplay::BuildCustomShader(ShaderEncoding sourceEncoding, const bytebuf &source,
                                     const rdcstr &entry, const ShaderCompileFlags &compileFlags,
                                     ShaderStage type, ResourceId &id, rdcstr &errors)
{
  if(sourceEncoding == ShaderEncoding::GLSL)
  {
    rdcstr sourceText = InsertSnippetAfterVersion(ShaderType::Vulkan, (const char *)source.data(),
                                                  source.count(), GLSL_CUSTOM_PREFIX);

    bytebuf patchedSource;
    patchedSource.assign((byte *)sourceText.begin(), sourceText.size());

    return BuildTargetShader(sourceEncoding, patchedSource, entry, compileFlags, type, id, errors);
  }

  BuildTargetShader(sourceEncoding, source, entry, compileFlags, type, id, errors);
}

void VulkanReplay::FreeCustomShader(ResourceId id)
{
  if(id == ResourceId())
    return;

  m_pDriver->ReleaseResource(GetResourceManager()->GetCurrentResource(id));
}

ResourceId VulkanReplay::ApplyCustomShader(TextureDisplay &display)
{
  if(display.customShaderId == ResourceId() || display.resourceId == ResourceId())
    return ResourceId();

  VulkanCreationInfo::Image &iminfo = m_pDriver->m_CreationInfo.m_Image[display.resourceId];

  GetDebugManager()->CreateCustomShaderTex(iminfo.extent.width, iminfo.extent.height,
                                           display.subresource.mip);

  int oldW = m_DebugWidth, oldH = m_DebugHeight;

  m_DebugWidth = RDCMAX(1U, iminfo.extent.width);
  m_DebugHeight = RDCMAX(1U, iminfo.extent.height);

  TextureDisplay disp;
  disp.red = disp.green = disp.blue = disp.alpha = true;
  disp.flipY = false;
  disp.xOffset = 0.0f;
  disp.yOffset = 0.0f;
  disp.customShaderId = display.customShaderId;
  disp.resourceId = display.resourceId;
  disp.typeCast = display.typeCast;
  disp.hdrMultiplier = -1.0f;
  disp.linearDisplayAsGamma = false;
  disp.subresource = display.subresource;
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
           0,
           0,
       },
       {RDCMAX(1U, iminfo.extent.width >> display.subresource.mip),
        RDCMAX(1U, iminfo.extent.height >> display.subresource.mip)}},
      1,
      &clearval,
  };

  LockedConstImageStateRef imageState = m_pDriver->FindConstImageState(display.resourceId);
  if(!imageState)
  {
    RDCWARN("Could not find image info for image %s", ToStr(display.resourceId).c_str());
    return ResourceId();
  }
  if(!imageState->isMemoryBound)
    return ResourceId();

  RenderTextureInternal(disp, *imageState, rpbegin, eTexDisplay_MipShift);

  m_DebugWidth = oldW;
  m_DebugHeight = oldH;

  return GetResID(GetDebugManager()->GetCustomTexture());
}

rdcarray<ShaderSourcePrefix> VulkanReplay::GetCustomShaderSourcePrefixes()
{
  // this is a complete hack, since we *do* want to define a prefix for GLSL. However GLSL sucks
  // and has the #version as the first thing, so we can't do a simple prepend of some defines.
  // Instead we will return no prefix and insert our own in BuildCustomShader if we see GLSL
  // coming in.
  // For SPIR-V no prefix is needed (or possible)
  // For HLSL however we define our HLSL prefix so that custom-compiled HLSL to SPIR-V gets the
  // right binding and helper definitions
  return {
      {ShaderEncoding::HLSL, HLSL_CUSTOM_PREFIX},
      {ShaderEncoding::Slang, HLSL_CUSTOM_PREFIX},
  };
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
      case ShaderStage::Task: stage = rdcspv::ShaderStage::Task; break;
      case ShaderStage::Mesh: stage = rdcspv::ShaderStage::Mesh; break;
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

  // check for shader module or shader object
  const VulkanRenderState state = m_pDriver->GetCmdRenderState();

  if(type == ShaderStage::Compute ? state.compute.shaderObject : state.graphics.shaderObject)
  {
    VkShaderCreateInfoEXT shadinfo;
    m_pDriver->GetShaderCache()->MakeShaderObjectInfo(shadinfo, state.shaderObjects[(uint32_t)type]);

    shadinfo.codeSize = spirv.size() * sizeof(uint32_t);
    shadinfo.pCode = spirv.data();

    VkShaderEXT shader;
    VkResult vkr = m_pDriver->vkCreateShadersEXT(m_pDriver->GetDev(), 1, &shadinfo, NULL, &shader);
    CheckVkResult(vkr);

    id = GetResID(shader);

    return;
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
  CheckVkResult(vkr);

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

    // only for graphics pipelines
    if(pipeInfo.graphicsPipe)
    {
      // don't replace incomplete pipeline libraries (we already pull the full state into the final
      // pipeline, so these are not used in replay; the libraries contain invalid dummy data for the
      // non-available parts)
      if(!(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) ||
         !(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ||
         !(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) ||
         !(pipeInfo.availStages & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT))
      {
        continue;
      }
    }

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

      // check if this is a graphics or compute pipeline
      if(pipeInfo.graphicsPipe)
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
            rdcarray<ShaderEntryPoint> entries =
                m_pDriver->m_CreationInfo.m_ShaderModule[GetResID(sh.module)].spirv.EntryPoints();
            if(entries.size() > 1)
            {
              if(entries.contains({sh.pName, ShaderStage(StageIndex(sh.stage))}))
              {
                // nothing to do!
              }
              else
              {
                RDCWARN(
                    "Multiple entry points in edited shader, none matching original, using first "
                    "one '%s'",
                    entries[0].name.c_str());
                entrynames.push_back(entries[0].name);
                sh.pName = entrynames.back().c_str();
              }
            }
            else
            {
              entrynames.push_back(entries[0].name);
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
        CheckVkResult(vkr);
      }
      else
      {
        VkComputePipelineCreateInfo pipeCreateInfo;
        m_pDriver->GetShaderCache()->MakeComputePipelineInfo(pipeCreateInfo, it->first);

        // replace the module by going via the live ID to pick up any replacements
        VkPipelineShaderStageCreateInfo &sh = pipeCreateInfo.stage;
        ResourceId shadOrigId = rm->GetOriginalID(pipeInfo.shaders[5].module);
        sh.module = rm->GetLiveHandle<VkShaderModule>(shadOrigId);

        rdcarray<ShaderEntryPoint> entries;

        if(rm->HasReplacement(shadOrigId))
        {
          entries = m_pDriver->m_CreationInfo.m_ShaderModule[GetResID(sh.module)].spirv.EntryPoints();
          if(entries.size() > 1)
          {
            if(entries.contains({sh.pName, ShaderStage(StageIndex(sh.stage))}))
            {
              // nothing to do!
            }
            else
            {
              RDCWARN(
                  "Multiple entry points in edited shader, none matching original, using first "
                  "one '%s'",
                  entries[0].name.c_str());
              sh.pName = entries[0].name.c_str();
            }
          }
          else
          {
            sh.pName = entries[0].name.c_str();
          }
        }

        // if we have pipeline executable properties, capture the data
        if(m_pDriver->GetExtensions(NULL).ext_KHR_pipeline_executable_properties)
        {
          pipeCreateInfo.flags |= (VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR |
                                   VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR);
        }

        // create the new compute pipeline
        VkResult vkr = m_pDriver->vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &pipeCreateInfo,
                                                           NULL, &pipe);
        CheckVkResult(vkr);
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

RDResult Vulkan_CreateReplayDevice(RDCFile *rdc, const ReplayOptions &opts, IReplayDriver **driver)
{
  RDCDEBUG("Creating a VulkanReplay replay device");

  // disable the layer env var, just in case the user left it set from a previous capture run
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, RENDERDOC_VULKAN_LAYER_VAR, "0"));

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

  // OverWolf is some shitty software that forked OBS and changed the layer value
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_VULKAN_OW_OBS_CAPTURE", "1"));

  // buggy program AgaueEye which also doesn't have a proper layer configuration. As a result
  // this is likely to have side-effects but probably also on other buggy layers that duplicate
  // sample code without even changing the layer json
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_SAMPLE_LAYER", "1"));

  // buggy overlay gamepp
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_GAMEPP_LAYER", "1"));

  // mesa device select layer crashes when it calls GPDP2 inside vkCreateInstance, which fails on
  // the current loader.
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "NODEVICE_SELECT", "1"));

  Process::RegisterEnvironmentModification(EnvironmentModification(
      EnvMod::Set, EnvSep::NoSep, "DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1", "1"));

  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "VK_LAYER_bandicam_helper_DEBUG_1", "1"));

  // fpsmon not only has a buggy layer but it also picks an absurdly generic disable environment
  // variable :(. Hopefully no other program picks this, or if it does then it's probably not a
  // bad thing to disable too
  Process::RegisterEnvironmentModification(
      EnvironmentModification(EnvMod::Set, EnvSep::NoSep, "DISABLE_LAYER", "1"));

  Process::ApplyEnvironmentModification();

  void *module = LoadVulkanLibrary();

  if(module == NULL)
  {
    RETURN_ERROR_RESULT(ResultCode::APIInitFailed, "Failed to load vulkan library");
  }

  VkInitParams initParams;

  uint64_t ver = VkInitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!VkInitParams::IsSupportedVersion(ver))
    {
      RETURN_ERROR_RESULT(ResultCode::APIIncompatibleVersion,
                          "Vulkan capture is incompatible version %llu, newest supported by this "
                          "build of RenderDoc is %llu",
                          ver, VkInitParams::CurrentVersion);
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    ser.SetVersion(ver);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RETURN_ERROR_RESULT(ResultCode::FileCorrupted,
                          "Expected to get a DriverInit chunk, instead got %u", chunk);
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      return ser.GetError();
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

  RDResult status = vk->Initialise(initParams, ver, opts);

  if(status != ResultCode::Succeeded)
  {
    SAFE_DELETE(rgp);

    delete vk;
    return status;
  }

  RDCLOG("Created device.");
  replay->SetRGP(rgp);

  *driver = (IReplayDriver *)replay;

  replay->GetInitialDriverVersion();

  return ResultCode::Succeeded;
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

RDResult Vulkan_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedVulkan vulkan;

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    RETURN_ERROR_RESULT(ResultCode::FileCorrupted, "File does not contain captured API data");

  vulkan.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  RDResult status = vulkan.ReadLogInitialisation(rdc, true);

  if(status == ResultCode::Succeeded)
    vulkan.GetStructuredFile()->Swap(output);

  return status;
}

static StructuredProcessRegistration VulkanProcessRegistration(RDCDriver::Vulkan,
                                                               &Vulkan_ProcessStructured);
