/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
void CodeWriter::Open()
{
  for(uint32_t i = 0; i < ID_COUNT; i++)
  {
    if(i == ID_MAIN || i == ID_VAR)
    {
      files[i] = new CodeFile(rootDirectory + "/sample_cpp_trace", funcs[i]);
    }
    else
    {
      files[i] = new MultiPartCodeFile(rootDirectory + "/sample_cpp_trace", funcs[i]);
    }
    files[i]->Open(funcs[i]);
  }

  // 'main' generated file contains functions per each stage of a project.
  // For example, there is a main_render() function that will call all
  // indexed render_i() functions that a trace produces. This applies to
  // render, [pre|post]reset, create, release generated functions. It
  // serves as the glue between the template application, which calls into
  // main_render() or main_create() functions and the core of the generated
  // code.
  for(int i = ID_RENDER; i < ID_COUNT; i++)
  {
    files[ID_MAIN]->PrintLnH("#include \"gen_%s.h\"", funcs[i].c_str());
  }
  for(int i = ID_RENDER; i < ID_COUNT; i++)
  {
    files[ID_MAIN]->PrintLnH("void %s_%s();", funcs[ID_MAIN].c_str(), funcs[i].c_str());
  }
  if(strlen(shimPrefix) != 0)
  {
    files[ID_VAR]->PrintLnH("#include \"sample_cpp_shim/shim_vulkan.h\"");
  }

  WriteTemplateFile("helper", "helper.h", helperH.c_str());
  WriteTemplateFile("helper", "CMakeLists.txt", helperCMakeLists.c_str());
  WriteTemplateFile("helper", "helper.cpp", (helperCppP1 + helperCppP2).c_str());
  WriteTemplateFile("sample_cpp_trace", "main_win.cpp", mainWinCpp.c_str());
  WriteTemplateFile("sample_cpp_trace", "main_xlib.cpp", mainXlibCpp.c_str());
  WriteTemplateFile("sample_cpp_trace", "common.h", commonH.c_str());
  WriteTemplateFile("sample_cpp_trace", "CMakeLists.txt", projectCMakeLists.c_str());
  WriteTemplateFile("", "CMakeLists.txt", rootCMakeLists.c_str());
  WriteTemplateFile("", "build_vs2015.bat", genScriptWin.c_str());
  WriteTemplateFile("", "build_xlib.bat", genScriptLinux.c_str());
  WriteTemplateFile("", "build_vs2015_ninja.bat", genScriptWinNinja.c_str());
}

void CodeWriter::WriteTemplateFile(std::string subdir, std::string file, const char* str) {
  std::string path = rootDirectory;
  if (!subdir.empty())
    path+="/" + subdir;
  std::string filepath = path + "/" + file;
  FileIO::CreateParentDirectory(filepath);

  FILE *templateFile = FileIO::fopen(filepath.c_str(), "wt");
  fprintf(templateFile, "%s", str);
  fclose(templateFile);
}

void CodeWriter::Close()
{
  for(int i = ID_RENDER; i < ID_COUNT; i++)
  {
    if(files[i] == NULL)
      continue;

    files[ID_MAIN]->PrintLn("void %s_%s() {", funcs[ID_MAIN].c_str(), funcs[i].c_str());

    if(i == ID_PRERESET || i == ID_POSTRESET || i == ID_INIT)
    {
      files[ID_MAIN]
          ->PrintLn("vkResetFences(%s, 1, &aux.fence);", tracker->GetDeviceVar())
          .PrintLn("VkCommandBufferBeginInfo cmd_buffer_bi = {")
          .PrintLn("VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL")
          .PrintLn("};")
          .PrintLn("vkBeginCommandBuffer(aux.command_buffer, &cmd_buffer_bi);\n");
    }

    int fileCount = static_cast<MultiPartCodeFile *>(files[i])->GetIndex();
    for(int j = 0; j <= fileCount; j++)
    {
      const char *stage = NULL;
      if(i == ID_INIT)
        stage = "Initializing Resources";
      if(i == ID_CREATE)
        stage = "Creating Resources";

      if(i == ID_INIT || i == ID_CREATE)
        files[ID_MAIN]->PrintLn("PostStageProgress(\"%s\", %d, %d);", stage, j, fileCount);

      files[ID_MAIN]->PrintLn("%s_%d();", funcs[i].c_str(), j);
    }

    if(i == ID_PRERESET || i == ID_POSTRESET || i == ID_INIT)
    {
      files[ID_MAIN]
          ->PrintLn("")
          .PrintLn("vkEndCommandBuffer(aux.command_buffer);")
          .PrintLn("VkSubmitInfo si = {")
          .PrintLn("VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0,")
          .PrintLn("NULL, NULL, 1, &aux.command_buffer, 0, NULL")
          .PrintLn("};")
          .PrintLn("vkQueueSubmit(aux.queue, 1, &si, aux.fence);")
          .PrintLn("vkQueueWaitIdle(aux.queue);");
    }

    files[ID_MAIN]->PrintLn("}");
    delete files[i];
    files[i] = NULL;
  }

  delete files[ID_MAIN];
  files[ID_MAIN] = NULL;
  delete files[ID_VAR];
  files[ID_VAR] = NULL;
}

void CodeWriter::PrintReadBuffers(StructuredBufferList &buffers) {
  for (size_t i = 0; i < buffers.size(); i++) {
    if (buffers[i]->size() == 0)
      continue;

    const char *name = tracker->GetDataBlobVar(i);
    files[ID_CREATE]->PrintLn("ReadBuffer(\"%s\", %s);", name, name);
  }
}

void CodeWriter::EarlyCreateResource(uint32_t pass)
{
  for(ResourceWithViewsMapIter it = tracker->ResourceCreateBegin();
      it != tracker->ResourceCreateEnd(); it++)
  {
    if(it->second.sdobj->ChunkID() == (uint32_t)VulkanChunk::vkCreateBuffer)
    {
      tracker->CreateResource(it->second.sdobj);
      CreateBuffer(it->second.sdobj, pass, true);
    }
    else if(it->second.sdobj->ChunkID() == (uint32_t)VulkanChunk::vkCreateImage)
    {
      tracker->CreateResource(it->second.sdobj);
      CreateImage(it->second.sdobj, pass, true);
    }
  }
}

void CodeWriter::RemapMemAlloc(uint32_t pass, MemAllocWithResourcesMapIter alloc_it)
{
  ExtObject *o = alloc_it->second.allocateSDObj;
  ExtObject *memory = o->At(3);
  const char *mem_remap = tracker->GetMemRemapVar(memory->U64());

  std::vector<size_t> order = alloc_it->second.BoundResourcesOrderByResetRequiremnet();
  if((tracker->Optimizations() & CODE_GEN_OPT_REORDER_MEMORY_BINDINGS_BIT) == 0)
  {
    for(size_t i = 0; i < order.size(); i++)
    {
      order[i] = i;
    }
  }

  ResetRequirement reset = RESET_REQUIREMENT_RESET;

  // Loop over all the bound resources, in the following order:
  // 1. Resources requiring reset before every frame
  // 2. Resources requiring initialization, but no reset between frames
  // 3. Resources requiring neither reset nor initialization.
  for(size_t resource_idx = 0; resource_idx < order.size(); resource_idx++)
  {
    const BoundResource &abr = alloc_it->second.boundResources[order[resource_idx]];
    if(tracker->Optimizations() & CODE_GEN_OPT_REORDER_MEMORY_BINDINGS_BIT)
    {
      switch(reset)
      {
        case RESET_REQUIREMENT_RESET:
        {
          if(abr.reset == RESET_REQUIREMENT_RESET)
          {
            break;
          }

          // This is the first non-reset resource.
          // Save the current memory_size to the ResetSize_ variable.
          const char *reset_size_name = tracker->GetMemResetSizeVar(memory->U64());
          files[pass]->PrintLn("%s = memory_size;", reset_size_name);

          // Look for initialization resources next.
          reset = RESET_REQUIREMENT_INIT;
          // fall through to RESET_REQUIREMENT_INIT case
        }
        case RESET_REQUIREMENT_INIT:
        {
          if(abr.reset == RESET_REQUIREMENT_INIT)
          {
            break;
          }

          // This is the first non-initialization resource.
          // Save the current memory_size to the InitSize_ variable.
          const char *init_size_name = tracker->GetMemInitSizeVar(memory->U64());
          files[pass]->PrintLn("%s = memory_size;", init_size_name);

          // all remaining resources should be not require reset nor initialization.
          reset = RESET_REQUIREMENT_NO_RESET;
        }
        break;
        default: break;
      }
      RDCASSERT(abr.reset >= reset);
    }

    const char *mem_bind_offset = tracker->GetReplayBindOffsetVar(abr.resource->U64());
    // Calculate the correct memory bits and correct memory size.
    files[pass]
        ->PrintLn("memory_bits = memory_bits & %s.memoryTypeBits;", abr.requirement->Name())
        .PrintLn("%s = AlignedSize(memory_size, %s.alignment);", mem_bind_offset,
                 abr.requirement->Name())
        .PrintLn("memory_size = %s + %s.size;", mem_bind_offset, abr.requirement->Name());

    // If there are no aliased resources we can recompute allocation size requirements
    // and new binding offsets for every resource correctly.
    if(!alloc_it->second.HasAliasedResources())
    {
      files[pass]
          ->PrintLn("%s[%u].replay.offset = %s;", mem_remap, resource_idx, mem_bind_offset)
          .PrintLn("%s[%u].replay.size = %s.size;", mem_remap, resource_idx, abr.requirement->Name())
          .PrintLn("%s[%u].capture.offset = %llu;", mem_remap, resource_idx, abr.offset->U64())
          .PrintLn("%s[%u].capture.size = VkMemoryRequirements_captured_%llu.size;", mem_remap,
                   resource_idx, abr.resource->U64());
    }
  }
  switch(reset)
  {
    case RESET_REQUIREMENT_RESET:
    {
      // All bound resources required reset.
      // Set ResetSize_ to the final memory_size.
      const char *reset_size_name = tracker->GetMemResetSizeVar(memory->U64());
      files[pass]->PrintLn("%s = memory_size;", reset_size_name);
      // fall through to RESET_REQUIREMENT_INIT case
    }
    case RESET_REQUIREMENT_INIT:
    {
      // All bound resources required initialization or reset.
      // Set InitSize_ to the final memory_size.
      const char *init_size_name = tracker->GetMemInitSizeVar(memory->U64());
      files[pass]->PrintLn("%s = memory_size;", init_size_name);
    }
    break;
    default: break;
  }
}

void CodeWriter::EarlyAllocateMemory(uint32_t pass)
{
  for(MemAllocWithResourcesMapIter alloc_it = tracker->MemAllocBegin();
      alloc_it != tracker->MemAllocEnd(); alloc_it++)
  {
    ExtObject *o = alloc_it->second.allocateSDObj;
    ExtObject *device = o->At(0);
    ExtObject *ai = o->At(1);
    ExtObject *memory = o->At(3);

    const char *device_name = tracker->GetResourceVar(device->U64());
    const char *memory_name = tracker->GetResourceVar(memory->Type(), memory->U64());
    const char *ai_name = tracker->GetMemAllocInfoVar(memory->U64(), true);
    const char *mem_remap = tracker->GetMemRemapVar(memory->U64());

    files[pass]->PrintLn("{");
    LocalVariable(ai, "", pass);

    // This device memory allocation has multiple resources bound to it.
    // I need to check that all resources can be bound to the same allocation
    // on the replay system. Additionally if there are no aliased resources
    // I can recompute proper size and alignment requirements and bind
    // the resources to correct offsets and make an allocation of the correct size.

    if(alloc_it->second.HasAliasedResources())
    {
      files[pass]->PrintLn("// Memory allocation %llu has aliased resources", memory->U64());
    }
    else
    {
      files[pass]->PrintLn("// Memory allocation %llu doesn't have aliased resources", memory->U64());
    }

    // Default values for memory_size and memory_bits
    files[pass]
        ->PrintLn("VkDeviceSize memory_size = 0;")
        .PrintLn("uint32_t memory_bits = 0xFFFFFFFF;");

    if(alloc_it->second.BoundResourceCount() > 0)
    {    // akharlamov: Why is there a device memory that has no resources bound to it?
         // We are not going to fill the Remap vector if there are aliased resources.
      if(!alloc_it->second.HasAliasedResources())
      {
        files[pass]->PrintLn("%s.resize(%llu);", mem_remap, alloc_it->second.BoundResourceCount());
      }
      RemapMemAlloc(pass, alloc_it);
    }

    // If allocation doesn't have any resources bound to it, or if it has aliased resources
    // change the memory_size to whatever was captured. Memory bits are still either
    // default value (~0) or correctly set.
    if(alloc_it->second.BoundResourceCount() == 0 || alloc_it->second.HasAliasedResources())
    {
      files[pass]->PrintLn("memory_size = %llu; // rdoc: reset size to capture value",
                           ai->At(2)->U64());
    }

    files[pass]
        ->PrintLn("%s.%s = memory_size;", ai->Name(), ai->At(2)->Name())
        .PrintLn("assert(memory_bits != 0);")
        .PrintLn(
            "%s.%s = CompatibleMemoryTypeIndex(%llu, "
            "VkPhysicalDeviceMemoryProperties_captured_%llu, "
            "VkPhysicalDeviceMemoryProperties_%llu, "
            "memory_bits);",
            ai->Name(), ai->At(3)->Name(), ai->At(3)->U64(), tracker->PhysDevID(),
            tracker->PhysDevID())
        .PrintLn("%s = %s;", ai_name, ai->Name())
        .PrintLn("VkResult result = %s(%s, &%s, NULL, &%s);", o->Name(), device_name, ai->Name(),
                 memory_name)
        .PrintLn("assert(result == VK_SUCCESS);")
        .PrintLn("}");
  }
}

void CodeWriter::EarlyBindResourceMemory(uint32_t pass)
{
  // For each memory allocation look at the bound resources and generate code for those.
  for(MemAllocWithResourcesMapIter alloc_it = tracker->MemAllocBegin();
      alloc_it != tracker->MemAllocEnd(); alloc_it++)
  {
    for(BoundResourcesIter resource_it = alloc_it->second.FirstBoundResource();
        resource_it != alloc_it->second.EndOfBoundResources(); resource_it++)
    {
      ExtObject *o = resource_it->bindSDObj;
      ExtObject *device = o->At(0);
      ExtObject *object = o->At(1);
      ExtObject *memory = o->At(2);
      ExtObject *offset = o->At(3);

      const char *device_name = tracker->GetResourceVar(device->U64());
      const char *memory_name = tracker->GetResourceVar(memory->U64());
      const char *object_name = tracker->GetResourceVar(object->U64());
      const char *object_mem_reqs = tracker->GetMemReqsVar(object->U64());
      std::string captured_bind_offset = tracker->GetCaptureBindOffsetVar(object->U64());
      std::string replayed_bind_offset = tracker->GetReplayBindOffsetVar(object->U64());

      int64_t memType = tracker->MemAllocTypeIndex(memory->U64());

      std::string phys_dev_mem_props_captured =
          "VkPhysicalDeviceMemoryProperties_captured_" + std::to_string(tracker->PhysDevID());

      std::string phys_dev_mem_props =
          std::string("VkPhysicalDeviceMemoryProperties_") + std::to_string(tracker->PhysDevID());

      files[pass]
          ->PrintLn("{")
          .PrintLn("VkResult result = CheckMemoryAllocationCompatibility(%lld, %s, %s, %s);", memType,
                   phys_dev_mem_props_captured.c_str(), phys_dev_mem_props.c_str(), object_mem_reqs)
          .PrintLn("assert(result == VK_SUCCESS);");

      if(alloc_it->second.HasAliasedResources())
      {
        files[pass]->PrintLn("result = %s(%s, %s, %s, %llu);", o->Name(), device_name, object_name,
                             memory_name, offset->U64());
      }
      else
      {
        files[pass]
            ->PrintLn("%s = %llu;", captured_bind_offset.c_str(), offset->U64())
            .PrintLn("result = %s(%s, %s, %s, %s /* rdoc:value %llu */);", o->Name(), device_name,
                     object_name, memory_name, replayed_bind_offset.c_str(), offset->U64());
      }
      files[pass]->PrintLn("assert(result == VK_SUCCESS);").PrintLn("}");
    }
  }
}

void CodeWriter::BindResourceMemory(ExtObject *o, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *object = o->At(1);
  ExtObject *memory = o->At(2);
  ExtObject *offset = o->At(3);

  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *memory_name = tracker->GetResourceVar(memory->U64());
  const char *object_name = tracker->GetResourceVar(object->U64());
  const char *object_mem_reqs = tracker->GetMemReqsVar(object->U64());

  int64_t memType = tracker->MemAllocTypeIndex(memory->U64());

  std::string phys_dev_mem_props_captured =
      "VkPhysicalDeviceMemoryProperties_captured_" + std::to_string(tracker->PhysDevID());

  std::string phys_dev_mem_props =
      std::string("VkPhysicalDeviceMemoryProperties_") + std::to_string(tracker->PhysDevID());

  files[pass]
      ->PrintLn("{")
      .PrintLn("VkResult result = CheckMemoryAllocationCompatibility(%lld, %s, %s, %s);", memType,
               phys_dev_mem_props_captured.c_str(), phys_dev_mem_props.c_str(), object_mem_reqs)
      .PrintLn("assert(result == VK_SUCCESS);");

  files[pass]
      ->PrintLn("result = %s(%s, %s, %s, %llu);", o->Name(), device_name, object_name, memory_name,
                offset->U64())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");
}

void CodeWriter::Resolution(uint32_t pass)
{
  files[pass]
      ->PrintLn("unsigned int resolutionWidth = %llu;", tracker->SwapchainWidth())
      .PrintLn("unsigned int resolutionHeight = %llu;", tracker->SwapchainHeight())
      .PrintLnH("extern int frameLoops;")
      .PrintLnH("extern unsigned int resolutionWidth;")
      .PrintLnH("extern unsigned int resolutionHeight;")
      .PrintLnH("extern bool automated;")
      .PrintLnH("extern bool resourceReset;")
      .PrintLnH("#if _WIN32")
      .PrintLnH("extern HINSTANCE appInstance;")
      .PrintLnH("extern HWND appHwnd;")
      .PrintLnH("#elif defined(__linux__)")
      .PrintLnH("extern Display *appDisplay;")
      .PrintLnH("extern Window appWindow;")
      .PrintLnH("#endif");
}

void CodeWriter::EnumeratePhysicalDevices(ExtObject *o, uint32_t pass)
{
  // Handles vkEnumeratePhysicalDevices, and then also covers the API calls
  // vkGetPhysicalDeviceProperties, vkGetPhysicalDeviceMemoryProperties,
  // vkGetPhysicalDeviceFeatures, vkGetPhysicalDeviceQueueFamilyProperties,
  RDCASSERT(o->Size() == 9);

  // Make a name for the VkPhysicalDevice object that will be used throughout the code project.
  ExtObject *instance = o->At(0);
  ExtObject *physicalDevice = o->At(2);

  const char *phys_device_name =
      tracker->GetResourceVar(physicalDevice->Type(), physicalDevice->U64());

  // Find the name for the VkInstance variable that was used here.
  RDCASSERT(tracker->InstanceID() == instance->U64());    // These must match.
  const char *instance_name = tracker->GetInstanceVar();

  files[pass]->PrintLn("{");    // Open bracket.

  // Actually do the enumeration
  files[pass]
      ->PrintLn("uint32_t phys_device_count = 0;")
      .PrintLn("std::vector<VkPhysicalDevice> phys_devices;")
      .PrintLn("VkResult r = vkEnumeratePhysicalDevices(%s, &phys_device_count, NULL);", instance_name)
      .PrintLn("assert(r == VK_SUCCESS && phys_device_count > 0);")
      .PrintLn("phys_devices.resize(phys_device_count);")
      .PrintLn("r = vkEnumeratePhysicalDevices(%s, &phys_device_count, phys_devices.data());",
               instance_name)
      .PrintLn("assert(r == VK_SUCCESS);")
      .PrintLn("if (phys_devices.size() > %llu) {", o->At(1)->U64())
      .PrintLn("%s = phys_devices[%llu]; // trace used %llu", phys_device_name, o->At(1)->U64(),
               o->At(1)->U64());

  // Print device properties that were captured in comments.
  ExtObject *phys_dev_props = o->At(4);
  LocalVariable(phys_dev_props, "_captured", pass);

  // Declare the VkPhysicalDeviceProperties variable. This is what current
  // device supports. An app developer can compare and contrast properties
  // that were captured with the one that were available.
  std::string phys_dev_props_name = AddVar("VkPhysicalDeviceProperties", physicalDevice->U64());
  files[pass]->PrintLn("%svkGetPhysicalDeviceProperties(%s, &%s);", shimPrefix, phys_device_name,
                       phys_dev_props_name.c_str());

  // Print device memory properties in comments.
  ExtObject *dev_mem_props = o->At(5);
  LocalVariable(dev_mem_props, "", pass);
  std::string phys_dev_mem_captured =
      AddVar("VkPhysicalDeviceMemoryProperties", "VkPhysicalDeviceMemoryProperties_captured",
             physicalDevice->U64());
  files[pass]->PrintLn("%s = %s;", phys_dev_mem_captured.c_str(), dev_mem_props->Name());

  // Declare the VkPhysicalDeviceMemoryProperties variable.
  std::string phys_dev_mem_props = AddVar("VkPhysicalDeviceMemoryProperties", physicalDevice->U64());
  files[pass]->PrintLn("%svkGetPhysicalDeviceMemoryProperties(%s, &%s);", shimPrefix,
                       phys_device_name, phys_dev_mem_props.c_str());

  // Print device memory features in comments.
  ExtObject *phys_dev_feats = o->At(6);
  LocalVariable(phys_dev_feats, "_captured", pass);

  // Declare the VkPhysicalDeviceFeatures variable.
  std::string phys_dev_feats_name = AddVar("VkPhysicalDeviceFeatures", physicalDevice->U64());
  files[pass]->PrintLn("%svkGetPhysicalDeviceFeatures(%s, &%s);", shimPrefix, phys_device_name,
                       phys_dev_feats_name.c_str());

  // Print queue properties in comments.
  ExtObject *queue_props = o->At(8);
  LocalVariable(queue_props, "_captured", pass);

  // Declare the vkGetPhysicalDeviceQueueFamilyProperties variable.
  std::string queue_prop_name =
      AddNamedVar("std::vector<VkQueueFamilyProperties>", tracker->GetQueueFamilyPropertiesVar());

  files[pass]
      ->PrintLn("{")
      .PrintLn("uint32_t count = 0;")
      .PrintLn("%svkGetPhysicalDeviceQueueFamilyProperties(%s, &count, NULL);", shimPrefix,
               phys_device_name)
      .PrintLn("%s.resize(count);", queue_prop_name.c_str())
      .PrintLn("%svkGetPhysicalDeviceQueueFamilyProperties(%s, &count, %s.data());", shimPrefix,
               phys_device_name,
               queue_prop_name.c_str())
      .PrintLn("}")
      .PrintLn("}");    // Close bracket for 'if (phys_devices.size() > %llu)'

  files[pass]->PrintLn("}");    // Close bracket.
}

void CodeWriter::CreateInstance(ExtObject *o, uint32_t pass, bool global_ci)
{
  RDCASSERT(o->Size() == 1);
  ExtObject *init_params = o->At(0);
  RDCASSERT(init_params->Size() == 8);

  ExtObject *instance = init_params->At(7);
  tracker->InstanceID(instance->U64());
  const char *instance_name = tracker->GetResourceVar(instance->Type(), instance->U64());

  files[pass]->PrintLn("{");

  ExtObject *layers = init_params->At(5);
  bool enables_vl = false;
  for(uint64_t i = 0; i < layers->Size(); i++)
  {
    if(layers->At(i)->data.str == "VK_LAYER_LUNARG_standard_validation")
    {
      enables_vl = true;
      break;
    }
  }
  if(!enables_vl)
  {
    uint64_t last = layers->Size();
    layers->AddChild(new SDObject("Validation Layer", "string"));
    layers->At(last)->data.str = "VK_LAYER_LUNARG_standard_validation";
    layers->At(last)->type.basetype = SDBasic::String;
  }

  if(!layers->IsNULL())
    LocalVariable(layers, "", pass);

  ExtObject *extensions = init_params->At(6);
  int64_t enables_surface = -1;
  bool enables_debug_report = false;
  for(uint64_t i = 0; i < extensions->Size(); i++)
  {
    if(extensions->At(i)->data.str == "VK_KHR_win32_surface" ||
       extensions->At(i)->data.str == "VK_KHR_xlib_surface" ||
       extensions->At(i)->data.str == "VK_KHR_xcb_surface")
    {
      enables_surface = i;
    }

    if(extensions->At(i)->data.str == "VK_EXT_debug_report")
    {
      enables_debug_report = true;
    }
  }

  if(!enables_debug_report)
  {
    uint64_t last = extensions->Size();
    extensions->AddChild(new SDObject("Debug Report Extension", "string"));
    extensions->At(last)->data.str = "VK_EXT_debug_report";
    extensions->At(last)->type.basetype = SDBasic::String;
  }

  if(!extensions->IsNULL())
    LocalVariable(extensions, "", pass);

  if(enables_surface != -1)
  {
    files[pass]
        ->PrintLn("#if defined(_WIN32)")
        .PrintLn("%s[%lld] = \"VK_KHR_win32_surface\";", extensions->Name(), enables_surface)
        .PrintLn("#elif defined(__linux__)")
        .PrintLn("%s[%lld] = \"VK_KHR_xlib_surface\";", extensions->Name(), enables_surface)
        .PrintLn("#endif");
  }

  files[pass]
      ->PrintLn("VkApplicationInfo ApplicationInfo = {")
      .PrintLn("/* sType */ VK_STRUCTURE_TYPE_APPLICATION_INFO,")
      .PrintLn("/* pNext */ NULL,")
      .PrintLn("/* pApplicationName */ \"%s\",", init_params->At(0)->Str())
      .PrintLn("/* applicationVersion */ %llu,", init_params->At(2)->U64())
      .PrintLn("/* pEngineName */ \"%s\",", init_params->At(1)->Str())
      .PrintLn("/* engineVersion */ %llu,", init_params->At(3)->U64())
      .PrintLn("/* apiVersion */ %llu,", VK_API_VERSION_1_1)
      .PrintLn("};")
      .PrintLn("VkInstanceCreateInfo InstanceCreateInfo = {")
      .PrintLn("/* sType */ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,")
      .PrintLn("/* pNext */ NULL,")
      .PrintLn("/* flags */  VkInstanceCreateFlags(0),")
      .PrintLn("/* pApplicationInfo */ &ApplicationInfo,");
  if(enables_vl)
  {    // if VL were not added by RenderDoc code gen, generate as is.
    files[pass]->PrintLn("/* enabledLayerCount */ %llu,", layers->Size());
  }
  else
  {    // otherwise include VL in debug builds only.
    files[pass]
        ->PrintLn("#if defined(_DEBUG) || defined(DEBUG)")
        .PrintLn("/* enabledLayerCount */ %llu,", layers->Size())
        .PrintLn("#else")
        .PrintLn("/* enabledLayerCount */ %llu,", layers->Size() - 1)
        .PrintLn("#endif");
  }
  files[pass]
      ->PrintLn("/* ppEnabledLayerNames */ %s,", layers->Size() > 0 ? layers->Name() : "NULL")
      .PrintLn("/* enabledExtensionCount */ %llu,", extensions->Size())
      .PrintLn("/* ppEnabledExtensionNames */ %s", extensions->Name())
      .PrintLn("};")
      .PrintLn("VkResult r = %svkCreateInstance(&InstanceCreateInfo, NULL, &%s);", shimPrefix,
               instance_name)
      .PrintLn("assert(r == VK_SUCCESS);")
      .PrintLn(
          "RegisterDebugCallback(aux, %s, VkDebugReportFlagBitsEXT(VK_DEBUG_REPORT_ERROR_BIT_EXT | "
          "VK_DEBUG_REPORT_DEBUG_BIT_EXT));",
          instance_name);

  files[pass]->PrintLn("}");
}

void CodeWriter::CreatePresentImageView(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *device = o->At(0);
  ExtObject *ci = o->At(1);
  ExtObject *view = o->At(3);
  ExtObject *image = ci->At(3);

  {
    ExtObject *image_copy = image;
    const char *device_name = tracker->GetResourceVar(device->U64());

    // Each ImageView actually becomes an array of views.
    const char *present_views =
        tracker->GetResourceVar("std::vector<VkImageView>", view->Type(), view->U64());
    files[pass]->PrintLn("%s.resize(%s);", present_views, tracker->SwapchainCountStr());

    // Create min(captured_swapchain_count, replayed_swapchain_count) views.
    // Basically create a view for each presentable image from swapchain.
    uint64_t i = 0;
    for(ExtObjectVecIter it = tracker->PresentImageBegin(); it != tracker->PresentImageEnd(); it++)
    {
      files[pass]->PrintLn("if (%s > %u) {", tracker->SwapchainCountStr(), i);
      image->U64() = (*it)->U64();
      LocalVariable(ci, "", pass);
      files[pass]
          ->PrintLn("VkResult result = %s(%s, &%s, NULL, &%s[%u]);", o->Name(), device_name,
                    ci->Name(), present_views, i++)
          .PrintLn("assert(result == VK_SUCCESS);");
      files[pass]->PrintLn("}");
    }

    image = image_copy;
  }
}

void CodeWriter::CreatePresentFramebuffer(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *device = o->At(0);
  ExtObject *ci = o->At(1);
  ExtObject *framebuffer = o->At(3);

  const char *present_fbs =
      tracker->GetResourceVar("std::vector<VkFramebuffer>", framebuffer->Type(), framebuffer->U64());
  files[pass]->PrintLn("%s.resize(%s);", present_fbs, tracker->SwapchainCountStr());

  ExtObject *presentView = tracker->FramebufferPresentView(o);
  VariableIDMapIter var_it = tracker->GetResourceVarIt(presentView->U64());
  std::string varName = var_it->second.name;

  for(uint32_t i = 0; i < tracker->SwapchainCount(); i++)
  {
    var_it->second.name = varName + "[" + std::to_string(i) + "]";
    files[pass]->PrintLn("if (%s > %u) {", tracker->SwapchainCountStr(), i);
    LocalVariable(ci, "", pass);

    files[pass]
        ->PrintLn("VkResult result = %s(%s, &%s, NULL, &%s[%u]);", o->Name(),
                  tracker->GetResourceVar(device->U64()), ci->Name(), present_fbs, i)
        .PrintLn("assert(result == VK_SUCCESS);");
    files[pass]->PrintLn("}");
  }
  var_it->second.name = varName;
}

void CodeWriter::CreateDescriptorPool(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateCommandPool(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateFramebuffer(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateRenderPass(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateSemaphore(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateFence(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateEvent(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateQueryPool(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateDescriptorSetLayout(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateDescriptorUpdateTemplate(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateImage(ExtObject *o, uint32_t pass, bool global_ci)
{
  o->name = shimPrefix + string(o->name);
  GenericVkCreate(o, pass, global_ci);
  BufferOrImageMemoryReqs(o, "vkGetImageMemoryRequirements", pass);
}

void CodeWriter::CreateImageView(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateSampler(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateShaderModule(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreatePipelineLayout(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreatePipelineCache(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateBuffer(ExtObject *o, uint32_t pass, bool global_ci)
{
  o->name = shimPrefix + string(o->name);
  GenericVkCreate(o, pass, global_ci);
  BufferOrImageMemoryReqs(o, "vkGetBufferMemoryRequirements", pass);
}

void CodeWriter::CreateBufferView(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericVkCreate(o, pass, global_ci);
}

void CodeWriter::CreateSwapchainKHR(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *device = o->At(0);
  ExtObject *ci = o->At(1);
  ExtObject *swapchain = o->At(3);
  const char *instance_name = tracker->GetInstanceVar();
  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *swapchain_name = tracker->GetResourceVar(swapchain->Type(), swapchain->U64());
  const char *phys_dev_name = tracker->GetPhysDeviceVar();

  std::string surface = AddVar("VkSurfaceKHR", swapchain->U64());
  std::string supported_bool = AddVar("std::vector<VkBool32>", "SurfaceSupported", swapchain->U64());
  std::string format_count = AddVar("uint32_t", "SurfaceFormatCount", swapchain->U64());
  std::string formats = AddVar("std::vector<VkSurfaceFormatKHR>", "SurfaceFormats", swapchain->U64());
  std::string surface_caps = AddVar("VkSurfaceCapabilitiesKHR", swapchain->U64());
  std::string mode_count = AddVar("uint32_t", "SurfacePresentModeCount", swapchain->U64());
  std::string modes =
      AddVar("std::vector<VkPresentModeKHR>", "SurfacePresentModes", swapchain->U64());

  files[pass]
      ->PrintLn("{")
      .PrintLn("#if defined(WIN32)")
      .PrintLn("VkWin32SurfaceCreateInfoKHR VkWin32SurfaceCreateInfoKHR_%llu = {", swapchain->U64())
      .PrintLn("/* sType = */ VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,")
      .PrintLn("/* pNext = */ NULL,")
      .PrintLn("/* flags = */ VkWin32SurfaceCreateFlagsKHR(0),")
      .PrintLn("/* hinstance = */ appInstance,")
      .PrintLn("/* hwnd = */ appHwnd")
      .PrintLn("};")
      .PrintLn(
          "VkResult result = vkCreateWin32SurfaceKHR("
          "%s, &VkWin32SurfaceCreateInfoKHR_%llu, NULL, &%s);",
          instance_name, swapchain->U64(), surface.c_str())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("#elif defined(__linux__)")
      .PrintLn("VkXlibSurfaceCreateInfoKHR VkXlibSurfaceCreateInfoKHR_%llu = {", swapchain->U64())
      .PrintLn("/* sType = */ VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,")
      .PrintLn("/* pNext = */ NULL,")
      .PrintLn("/* VkXlibSurfaceCreateFlagsKHR = */ 0,")
      .PrintLn("/* Display */ appDisplay,")
      .PrintLn("/* Window */ appWindow")
      .PrintLn("};")
      .PrintLn(
          "VkResult result = vkCreateXlibSurfaceKHR(%s, &VkXlibSurfaceCreateInfoKHR_%llu, NULL, "
          "&%s);",
          instance_name, swapchain->U64(), surface.c_str())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("#endif");

  files[pass]->PrintLn("%s.resize(%s.size());", supported_bool.c_str(),
                       tracker->GetQueueFamilyPropertiesVar());
  for(uint64_t i = 0; i < tracker->QueueFamilyCount(); i++)
  {
    files[pass]
        ->PrintLn("if (%s.size() > %llu) {", supported_bool.c_str(), i)
        .PrintLn("result = %svkGetPhysicalDeviceSurfaceSupportKHR(%s, %llu, %s, &%s[%llu]);",
                 shimPrefix, phys_dev_name, i, surface.c_str(), supported_bool.c_str(), i)
        .PrintLn("assert(result == VK_SUCCESS);")
        .PrintLn("}");
  }

  files[pass]
      ->PrintLn("result = %svkGetPhysicalDeviceSurfaceFormatsKHR(%s, %s, &%s, NULL);", shimPrefix,
                phys_dev_name, surface.c_str(), format_count.c_str())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("%s.resize(%s);", formats.c_str(), format_count.c_str())
      .PrintLn("result = %svkGetPhysicalDeviceSurfaceFormatsKHR(%s, %s, &%s, %s.data());",
               shimPrefix, phys_dev_name, surface.c_str(), format_count.c_str(), formats.c_str())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("result = %svkGetPhysicalDeviceSurfacePresentModesKHR(%s, %s, &%s, NULL);",
               shimPrefix, phys_dev_name, surface.c_str(), mode_count.c_str())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("%s.resize(%s);", modes.c_str(), mode_count.c_str())
      .PrintLn("result = %svkGetPhysicalDeviceSurfacePresentModesKHR(%s, %s, &%s, %s.data());",
               shimPrefix, phys_dev_name, surface.c_str(), mode_count.c_str(), modes.c_str())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("result = %svkGetPhysicalDeviceSurfaceCapabilitiesKHR(%s, %s, &%s);", shimPrefix,
               phys_dev_name, surface.c_str(), surface_caps.c_str())
      .PrintLn("assert(result == VK_SUCCESS);");

  LocalVariable(ci, "", pass);

  files[pass]
      ->PrintLn("%s.%s = %s;", ci->Name(), "surface", surface.c_str())
      .PrintLn("%s.%s = GetCompatiblePresentMode(%s.%s, %s);", ci->Name(), "presentMode",
               ci->Name(), "presentMode", modes.c_str())
      .PrintLn("result = %s(%s, &%s, NULL, &%s);", o->Name(), device_name, ci->Name(), swapchain_name)
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");

  AddNamedVar("uint32_t", tracker->SwapchainCountStr());
  AddNamedVar("std::vector<VkImage>", tracker->PresentImagesStr());
}

void CodeWriter::CreateGraphicsPipelines(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericCreatePipelines(o, pass, global_ci);
}

void CodeWriter::CreateComputePipelines(ExtObject *o, uint32_t pass, bool global_ci)
{
  GenericCreatePipelines(o, pass, global_ci);
}

void CodeWriter::CreateDevice(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *phys_dev = o->At(0);
  ExtObject *ci = o->At(1);
  ExtObject *vk_res = o->At(3);

  RDCASSERT(phys_dev->U64() == tracker->PhysDevID());

  const char *device_name = tracker->GetPhysDeviceVar();
  const char *vk_res_name = tracker->GetResourceVar(vk_res->Type(), vk_res->U64());

  files[pass]->PrintLn("{");
  LocalVariable(ci, "", pass);
  files[pass]
      ->PrintLn("MakePhysicalDeviceFeaturesMatch(VkPhysicalDeviceFeatures_%llu, %s);",
                tracker->PhysDevID(), ci->At(9)->Name())
      .PrintLn("VkResult result = %s(%s, &%s, NULL, &%s);", o->Name(), device_name, ci->Name(),
               vk_res_name)
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");

  AddNamedVar("AuxVkTraceResources", "aux");
  CreateAuxResources(o, pass);

  // akharlamov: besides creating the device, resource creation,
  // memory allocation and resource binding happens on CreateDevice.
  // The reason behind this organization is that resource memory
  // type requirement can be different on replay system and memory
  // allocation needs to find an intersection of memory types of all
  // the resources that would be bound to that allocation.
  // In the code gen, this is achieved by:
  // 1. Creating the device
  // 2. For each memory allocation
  //   a. Go over the list of resources that are bound to that allocation
  //   b. Create those resources and get their memory requirements
  //   c. Bitmask and the memoryTypeBits
  //   d. The resulting bitmask of memoryTypeBits is used for memory allocation
  //   (and thus intersection of all memoryTypeBits needs to be != 0)
  //   If intersection is '0', the trace can't be replayed on this system.
  //   e. Additionally if the memory allocation doesn't host aliased resources
  //   then the size and binding offset of each resource is recalculated
  //   and stored in a 'Remap' vector.
  HandleMemoryAllocationAndResourceCreation(pass);
}

void CodeWriter::HandleMemoryAllocationAndResourceCreation(uint32_t pass)
{
  EarlyCreateResource(pass);
  EarlyAllocateMemory(pass);
  EarlyBindResourceMemory(pass);
}

void CodeWriter::CreateAuxResources(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *device = as_ext(o->At(3));
  files[pass]->PrintLn("InitializeAuxResources(&aux, %s, %s, %s);", tracker->GetInstanceVar(),
                       tracker->GetPhysDeviceVar(), tracker->GetResourceVar(device->U64()));
}

void CodeWriter::GetDeviceQueue(ExtObject *o, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *family = o->At(1);
  ExtObject *index = o->At(2);
  ExtObject *queue = o->At(3);
  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *queue_name = tracker->GetResourceVar(queue->Type(), queue->U64());
  files[pass]
      ->PrintLn("{")
      .PrintLn("%s(%s, %llu, %llu, &%s);", o->Name(), device_name, family->U64(), index->U64(),
               queue_name)
      .PrintLn("}");
}

void CodeWriter::GetSwapchainImagesKHR(ExtObject *o, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *swapchain = o->At(1);
  uint64_t swapchain_idx = o->At(2)->U64();
  ExtObject *image = o->At(3);

  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *swapchain_name = tracker->GetResourceVar(swapchain->U64());
  static int32_t count = (int32_t)0;

  // Do this only once: populate the PresentImages vector with swapchain images.
  if(count == 0)
  {
    files[pass]
        ->PrintLn("{")
        .PrintLn("VkResult result = %s(%s, %s, &%s, NULL);", o->Name(), device_name, swapchain_name,
                 tracker->SwapchainCountStr())
        .PrintLn("assert(result == VK_SUCCESS);")
        .PrintLn("%s.resize(%s);", tracker->PresentImagesStr(), tracker->SwapchainCountStr())
        .PrintLn("result = %s(%s, %s, &%s, %s.data());", o->Name(), device_name, swapchain_name,
                 tracker->SwapchainCountStr(), tracker->PresentImagesStr())
        .PrintLn("assert(result == VK_SUCCESS);");
    files[pass]->PrintLn("}");
  }

  // For every image that RenderDoc creates, associate it to a PresentImages[Index];
  const char *image_name = tracker->GetResourceVar(image->Type(), image->U64());
  files[pass]->PrintLn("if (%s > %u) %s = %s[%u];", tracker->SwapchainCountStr(), swapchain_idx,
                       image_name, tracker->PresentImagesStr(), swapchain_idx);

  count++;
}

void CodeWriter::AllocateCommandBuffers(ExtObject *o, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *ai = o->At(1);
  ExtObject *cmd_buffer = o->At(2);

  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *cmd_buffer_name = tracker->GetResourceVar(cmd_buffer->Type(), cmd_buffer->U64());

  files[pass]->PrintLn("{");
  LocalVariable(ai, "", pass);
  files[pass]
      ->PrintLn("std::vector<VkCommandBuffer> cmds(%llu);", ai->At(4)->U64())
      .PrintLn("VkResult result = %s(%s, &%s, cmds.data());", o->Name(), device_name, ai->Name())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("%s = cmds[0];", cmd_buffer_name)
      .PrintLn("}");
}

void CodeWriter::AllocateMemory(ExtObject *o, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *ai = o->At(1);
  ExtObject *memory = o->At(3);

  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *memory_name = tracker->GetResourceVar(memory->Type(), memory->U64());
  const char *ai_name = tracker->GetMemAllocInfoVar(memory->U64(), true);

  files[pass]->PrintLn("{");
  LocalVariable(ai, "", pass);

  files[pass]
      ->PrintLn(
          "%s.%s = CompatibleMemoryTypeIndex(%llu, "
          "VkPhysicalDeviceMemoryProperties_captured_%llu, "
          "VkPhysicalDeviceMemoryProperties_%llu, "
          "0xFFFFFFFF);",
          ai->Name(), ai->At(3)->Name(), ai->At(3)->U64(), tracker->PhysDevID(), tracker->PhysDevID())
      .PrintLn("%s = %s;", ai_name, ai->Name())
      .PrintLn("VkResult result = %s(%s, &%s, NULL, &%s);", o->Name(), device_name, ai->Name(),
               memory_name)
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");
}

void CodeWriter::AllocateDescriptorSets(ExtObject *o, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *ai = o->At(1);
  ExtObject *ds = o->At(2);
  // DescriptorSetAllocateInfo.descriptorSetCount must always be equal to '1'.
  // Descriptor set allocation can allocate multiple descriptor sets at the
  // same time, but RenderDoc splits these calls into multiple calls, one per
  // each descriptor set object that is still alive at the time of capture.
  RDCASSERT(ai->At(3)->U64() == 1);
  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *ds_name = tracker->GetResourceVar(ds->Type(), ds->U64());
  files[pass]->PrintLn("{");
  LocalVariable(ai, "", pass);
  files[pass]
      ->PrintLn("VkResult result = %s(%s, &%s, &%s);", o->Name(), device_name, ai->Name(), ds_name)
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");
}

void CodeWriter::BufferOrImageMemoryReqs(ExtObject *o, const char *get_mem_req_func, uint32_t pass)
{
  ExtObject *device = o->At(0);
  ExtObject *object = o->At(3);
  const char *device_name = tracker->GetResourceVar(device->Type(), device->U64());
  const char *object_name = tracker->GetResourceVar(object->Type(), object->U64());
  const char *mem_req_name = tracker->GetMemReqsVar(object->U64());
  std::string captured_mem_req_name =
      AddVar("VkMemoryRequirements", "VkMemoryRequirements_captured", object->U64());

  files[pass]->PrintLn("{");
  LocalVariable(o->At(4), "_temp", pass);
  files[pass]
      ->PrintLn("%s = %s_temp;", captured_mem_req_name.c_str(), o->At(4)->Name())
      .PrintLn("%s(%s, %s, &%s);", get_mem_req_func, device_name, object_name, mem_req_name)
      .PrintLn("}");
}

void CodeWriter::InitDstBuffer(ExtObject *o, uint32_t pass)
{
  uint64_t resourceID = o->At(1)->U64();
  InitResourceIDMapIter init_res_it = tracker->InitResourceFind(resourceID);
  const char *mem_dst_name = tracker->GetResourceVar(resourceID);
  std::string buf_dst_name = AddVar("VkBuffer", "VkBuffer_dst", resourceID);
  const char *init_size_name = tracker->GetMemInitSizeVar(resourceID);
  MemAllocWithResourcesMapIter mem_it = tracker->MemAllocFind(resourceID);
  RDCASSERT(mem_it != tracker->MemAllocEnd());

  const char *comment = "";
  if(!tracker->ResourceNeedsReset(resourceID, true, true))
  {
    comment = "// ";
  }

  std::string size;
  if(mem_it->second.HasAliasedResources())
  {
    ExtObject *allocateInfo = mem_it->second.allocateSDObj->At(1);
    uint64_t allocationSize = allocateInfo->At(2)->U64();
    size = std::to_string(allocationSize);
  }
  else
  {
    size = std::string(init_size_name);
  }

  files[pass]->PrintLn("%sInitializeDestinationBuffer(%s, &%s, %s, %s);", comment,
                       tracker->GetDeviceVar(), buf_dst_name.c_str(), mem_dst_name, size.c_str());
}

void CodeWriter::ClearBufferData()
{
  for(VariableIDMapIter i = tracker->DataBlobBegin(); i != tracker->DataBlobEnd(); i++)
  {
    files[ID_RELEASE]->PrintLn("%s.clear()", i->second.name.c_str());
  }
}

void CodeWriter::InitSrcBuffer(ExtObject *o, uint32_t pass)
{
  uint64_t resourceID = o->At(1)->U64();
  uint64_t bufferID = o->At(4)->U64();
  InitResourceIDMapIter init_res_it = tracker->InitResourceFind(resourceID);

  std::map<AccessState, std::string> stateNames;
  stateNames[ACCESS_STATE_INIT] = "Init";
  stateNames[ACCESS_STATE_READ] = "Read";
  stateNames[ACCESS_STATE_WRITE] = "Write";
  stateNames[ACCESS_STATE_CLEAR] = "Clear";
  stateNames[ACCESS_STATE_RESET] = "Reset";

  std::map<uint64_t, std::string> imageAspectFlagBitNames;
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT] = "COLOR";
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT] = "DEPTH";
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT] = "STENCIL";
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_METADATA_BIT] = "METADATA";
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_PLANE_0_BIT] = "PLANE_0";
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_PLANE_1_BIT] = "PLANE_1";
  imageAspectFlagBitNames[VkImageAspectFlagBits::VK_IMAGE_ASPECT_PLANE_2_BIT] = "PLANE_2";

  MemAllocWithResourcesMapIter mem_it = tracker->MemAllocFind(resourceID);
  bool hasAliasedResources = false;
  if(mem_it != tracker->MemAllocEnd())
  {
    hasAliasedResources = mem_it->second.HasAliasedResources();
    files[pass]->PrintLn("/* Memory %u Usage:", resourceID);
    for(IntervalsIter<MemoryState> it = mem_it->second.memoryState.begin();
        it != mem_it->second.memoryState.end(); it++)
    {
      files[pass]->PrintLn("    (%#X, %#X): %s", it.start(), it.end(),
                           stateNames[it.value().accessState].c_str());
    }
    files[pass]->PrintLn("*/");
  }

  ImageStateMapIter img_it = tracker->ImageStateFind(resourceID);
  if(img_it != tracker->ImageStateEnd())
  {
    files[pass]->PrintLn("/* Image %llu Usage:", resourceID);
    for(ImageSubresourceStateMapIter sub_it = img_it->second.begin();
        sub_it != img_it->second.end(); sub_it++)
    {
      files[pass]->PrintLn("    (%s, %llu, %llu): %s",
                           imageAspectFlagBitNames[sub_it->first.aspect].c_str(), sub_it->first.level,
                           sub_it->first.layer, stateNames[sub_it->second.AccessState()].c_str());
    }
    files[pass]->PrintLn("*/");
  }

  const char *comment = "";
  if(!tracker->ResourceNeedsReset(resourceID, true, true))
  {
    comment = "// ";
  }
  std::string mem_src_name = AddVar("VkDeviceMemory", "VkDeviceMemory_src", resourceID);
  std::string buf_src_name = AddVar("VkBuffer", "VkBuffer_src", resourceID);
  // If a mem_remap vector hasn't been generated, it will be automatically created now, and it will
  // be empty.
  const char *mem_remap = tracker->GetMemRemapVar(resourceID);

  // If a reset_size variable hasn't been generated, it will be automatically created now, and it
  // will be zero.
  const char *init_size_name = tracker->GetMemInitSizeVar(resourceID);

  std::string size = tracker->GetMemAllocInfoVar(resourceID);
  if(size == "nullptr" || size == "NULL")
  {
    size = std::string(tracker->GetDataBlobVar(bufferID)) + ".size()";
  }
  else if(hasAliasedResources)
  {
    ExtObject *allocateInfo = mem_it->second.allocateSDObj->At(1);
    uint64_t allocationSize = allocateInfo->At(2)->U64();
    size = std::to_string(allocationSize);
  }
  else
  {
    size = std::string(init_size_name);
  }

  files[pass]->PrintLn(
      "%sInitializeSourceBuffer(%s, &%s, &%s, %s, buffer_%llu.data(), "
      "VkPhysicalDeviceMemoryProperties_%llu, %s);",
      comment, tracker->GetDeviceVar(), buf_src_name.c_str(), mem_src_name.c_str(), size.c_str(),
      bufferID, tracker->PhysDevID(), mem_remap);
}

void CodeWriter::InitDescSet(ExtObject *o)
{
  uint64_t descriptorSetID = o->At(1)->U64();
  ExtObject *initBindings = o->At(2);

  struct DescSetInfoNames
  {
    std::string image_info = "NULL";
    std::string buffer_info = "NULL";
    std::string texel_view = "NULL";
    uint32_t binding;
    uint32_t type;
    uint32_t element;
    std::string typeStr = "";
  };

  std::vector<DescSetInfoNames> writeDescriptorSets[2];
  std::array<uint32_t, 2> passes = {{ID_INIT, ID_PRERESET}};

  DescriptorSetInfoMapIter descSetInfo_it = tracker->DescSetInfosFind(descriptorSetID);

  for(uint32_t p = 0; p < passes.size(); p++)
  {
    files[passes[p]]->PrintLn("{");
  }

  for(uint32_t j = 0; j < initBindings->Size(); j++)
  {
    ExtObject *initBinding = initBindings->At(j);
    RDCASSERT(initBinding->Size() == 6);
    ExtObject *binding = initBinding->At(3);
    ExtObject *type = initBinding->At(4);
    ExtObject *element = initBinding->At(5);
    DescSetInfoNames info;
    ExtObject *srcObj = NULL;

    info.binding = as_uint32(binding->U64());
    info.type = as_uint32(type->U64());
    info.element = as_uint32(element->U64());
    info.typeStr = type->Str();

    bool needsReset = descSetInfo_it->second.NeedsReset(info.binding, info.element);
    uint64_t passIdx = needsReset ? 1 : 0;
    uint64_t passID = passes[passIdx];

    switch(type->U64())
    {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      {    // use bufferinfo
        srcObj = initBinding->At(0);
        files[passID]
            ->PrintLn("%s %s_%u = {", srcObj->Type(), srcObj->Type(), j)
            .PrintLn("/* %s = */ %s,", srcObj->At(0)->Name(),
                     tracker->GetResourceVar(srcObj->At(0)->U64()))
            .PrintLn("/* %s = */ %llu,", srcObj->At(1)->Name(), srcObj->At(1)->U64())
            .PrintLn("/* %s = */ %llu,", srcObj->At(2)->Name(), srcObj->At(2)->U64())
            .PrintLn("};");
        info.buffer_info = "&" + std::string(srcObj->Type()) + "_" + std::to_string(j);
      }
      break;
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      {    // use image info.
        srcObj = initBinding->At(1);
        files[passID]
            ->PrintLn("%s %s_%u = {", srcObj->Type(), srcObj->Type(), j)
            .PrintLn("/* %s = */ %s,", srcObj->At(0)->Name(),
                     tracker->GetResourceVar(srcObj->At(0)->U64()))
            .PrintLn("/* %s = */ %s,", srcObj->At(1)->Name(),
                     tracker->GetResourceVar(srcObj->At(1)->U64()))
            .PrintLn("/* %s = */ %s,", srcObj->At(2)->Name(), srcObj->At(2)->Str())
            .PrintLn("};");
        info.image_info = "&" + std::string(srcObj->Type()) + "_" + std::to_string(j);
      }
      break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      {    // use texel buffer
        srcObj = initBinding->At(2);
        files[passID]->PrintLn("%s %s_%u = %s;", srcObj->Type(), srcObj->Name(), j,
                               tracker->GetResourceVar(srcObj->U64()));
        info.texel_view = "&" + std::string(srcObj->Name()) + "_" + std::to_string(j);
      }
      break;
    }
    writeDescriptorSets[passIdx].push_back(info);
  }

  for(uint32_t p = 0; p < passes.size(); p++)
  {
    if(!writeDescriptorSets[p].empty())
    {
      files[passes[p]]->PrintLn("VkWriteDescriptorSet VkWriteDescriptorSet_temp[%llu] = {",
                                writeDescriptorSets[p].size());

      for(uint32_t i = 0; i < writeDescriptorSets[p].size(); i++)
      {
        files[passes[p]]
            ->PrintLn("{")
            .PrintLn("/* sType = */ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,")
            .PrintLn("/* pNext = */ NULL,")
            .PrintLn("/* dstSet = */ %s,", tracker->GetResourceVar(descriptorSetID))
            .PrintLn("/* dstBinding = */ %u,", writeDescriptorSets[p][i].binding)
            .PrintLn("/* dstArrayElement = */ %u,", writeDescriptorSets[p][i].element)
            .PrintLn("/* descriptorCount = */ %llu,", 1)
            .PrintLn("/* descriptorType = */ %s,", writeDescriptorSets[p][i].typeStr.c_str())
            .PrintLn("/* pImageInfo = */ %s,", writeDescriptorSets[p][i].image_info.c_str())
            .PrintLn("/* pBufferInfo = */ %s,", writeDescriptorSets[p][i].buffer_info.c_str())
            .PrintLn("/* pTexelBufferView = */ %s,", writeDescriptorSets[p][i].texel_view.c_str())
            .PrintLn("},");
      }
      files[passes[p]]
          ->PrintLn("};")
          .PrintLn("vkUpdateDescriptorSets(%s, %llu, %s, 0, NULL);", tracker->GetDeviceVar(),
                   writeDescriptorSets[p].size(), "VkWriteDescriptorSet_temp")
          .PrintLn("}");
    }
    else
    {
      RDCWARN(
          "No valid update for descriptor set (%llu)"
          "with NumBindings (%llu) and Bindings.Size() (%llu)",
          descriptorSetID, o->At(3)->U64(), o->At(2)->U64());
      files[passes[p]]->PrintLn(
          "// No valid descriptor sets, with NumBindings (%llu) and Bindings.Size() (%llu)",
          o->At(3)->U64(), o->At(2)->U64());
      files[passes[p]]->PrintLn("}");
    }
  }
}

void CodeWriter::ImageLayoutTransition(uint64_t image_id, ExtObject *subres, const char *old_layout,
                                       uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(subres->At("subresourceRange"), "", pass);
  files[pass]
      ->PrintLn("ImageLayoutTransition(aux, %s, %s, %s, %s);", tracker->GetResourceVar(image_id),
                subres->At("subresourceRange")->Name(), subres->At("newLayout")->ValueStr().c_str(),
                old_layout)
      .PrintLn("}");
}

void CodeWriter::InitialLayouts(ExtObject *o, uint32_t pass)
{
  RDCASSERT(o->ChunkID() == (uint32_t)SystemChunk::CaptureBegin);
  RDCASSERT(o->At(0)->U64() > 0);
  uint64_t num = o->At(0)->U64();
  for(uint64_t i = 0; i < num; i++)
  {
    ExtObject *image = o->At(i * 2 + 1);
    ExtObject *layout = o->At(i * 2 + 2);

    uint64_t image_id = image->U64();

    ResourceWithViewsMapIter rc_it = tracker->ResourceCreateFind(image_id);

    ExtObject *subresources = layout->At("subresourceStates");

    if(rc_it == tracker->ResourceCreateEnd())
      continue;

    ImageStateMapIter imageState_it = tracker->ImageStateFind(image_id);
    RDCASSERT(imageState_it != tracker->ImageStateEnd());
    ImageState &imageState(imageState_it->second);

    for(uint64_t j = 0; j < subresources->Size(); j++)
    {
      ExtObject *imageRegionState = subresources->At(j);

      VkImageLayout newLayout = (VkImageLayout)imageRegionState->At("newLayout")->U64();

      if(newLayout == VK_IMAGE_LAYOUT_UNDEFINED || newLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
        continue;

      bool needsResourceReset = tracker->ResourceNeedsReset(image_id, false, true);
      bool needsResourceInit = tracker->ResourceNeedsReset(image_id, true, false);

      // I assume that INIT and RESET are mutually exclusive.
      if ((tracker->Optimizations() & CODE_GEN_OPT_IMAGE_RESET_BIT) != 0)
        RDCASSERT(!(needsResourceInit && needsResourceReset));

      ExtObject *subres = imageRegionState->At("subresourceRange");
      VkImageAspectFlags aspectMask = (VkImageAspectFlags)subres->At("aspectMask")->U64();
      uint64_t baseMip = subres->At("baseMipLevel")->U64();
      uint64_t levelCount = subres->At("levelCount")->U64();
      uint64_t baseLayer = subres->At("baseArrayLayer")->U64();
      uint64_t layerCount = subres->At("layerCount")->U64();

      ImageSubresourceRange range =
          imageState.Range(aspectMask, baseMip, levelCount, baseLayer, layerCount);

      ImageSubresourceRangeStateChanges changes = imageState.RangeChanges(range);

      // Assert that the startLayout is identical in all subresources inside current subresource
      // range by RenderDoc's design.
      // Also assert that the endLayout for this subresource range is the same for each subresource.
      RDCASSERT(changes.sameStartLayout && changes.sameEndLayout);

      if(needsResourceReset)
      {
        // If we have a RESET we alway make a transition from UNDEFINED TO DST_OPTIMAL for transfer.
        // Now we need to transition from DST_OPTIMAL to whatever is needed at the start of the
        // frame in PRERESET stage.
        ImageLayoutTransition(image_id, imageRegionState, "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL",
                              ID_PRERESET);
      }
      else if(needsResourceInit)
      {
        // If we have an INIT we alway make a transition from either PREINITIALIZED or UNDEFINED TO
        // DST_OPTIMAL for transfer.
        // Now we need to transition from DST_OPTIMAL to whatever is needed at the start of the
        // frame in INIT stage.
        ImageLayoutTransition(image_id, imageRegionState, "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL",
                              ID_INIT);

        // If the layout of the subresourceRange doesn't change throughout the frame, we don't need
        // to restore it.
        // If it changes though, we can transition from whatever was the endLayout to startLayout in
        // POSTRESET stage.
        if(changes.layoutChanged)
          ImageLayoutTransition(image_id, imageRegionState, VkImageLayoutStrings[changes.endLayout],
                                ID_POSTRESET);
      }
      else
      {
        // This resource doesn't have an INIT or PRERESET stage. It may still need to reset the
        // layout though.
        // First transition to the correct layout at INIT stage.
        std::string initialLayout = ToStr(imageState.InitialLayout());
        ImageLayoutTransition(image_id, imageRegionState, initialLayout.c_str(), ID_INIT);

        // If the layout of the subresourceRange doesn't change throughout the frame, we don't need
        // to restore it.
        // If it changes though, we can transition from whatever was the endLayout to startLayout in
        // POSTRESET stage.
        if(changes.layoutChanged)
          ImageLayoutTransition(image_id, imageRegionState, VkImageLayoutStrings[changes.endLayout],
                                ID_POSTRESET);
      }
    }
  }
}

void CodeWriter::InitialContents(ExtObject *o)
{
  switch(o->At(0)->U64())
  {
    case VkResourceType::eResImage:
      InitSrcBuffer(o, ID_CREATE);
      CopyResetImage(o, ID_INIT);
      CopyResetImage(o, ID_PRERESET);
      break;
    case VkResourceType::eResDeviceMemory:
      InitSrcBuffer(o, ID_CREATE);
      InitDstBuffer(o, ID_CREATE);
      CopyResetBuffer(o, ID_INIT);
      CopyResetBuffer(o, ID_PRERESET);
      break;
    case VkResourceType::eResDescriptorSet: { InitDescSet(o);
    }
    break;
  }
}

void CodeWriter::CopyResetImage(ExtObject *o, uint32_t pass)
{
  uint64_t resourceID = o->At(1)->U64();
  InitResourceIDMapIter init_res_it = tracker->InitResourceFind(resourceID);

  const char *comment = "";
  if(!tracker->ResourceNeedsReset(resourceID, pass == ID_INIT, pass == ID_PRERESET))
  {
    comment = "// ";
  }

  files[pass]->PrintLn("%sCopyResetImage(aux, %s, VkBuffer_src_%llu, VkImageCreateInfo_%llu);",
                       comment, tracker->GetResourceVar(o->At(1)->U64()), o->At(1)->U64(),
                       o->At(1)->U64());
}
void CodeWriter::CopyResetBuffer(ExtObject *o, uint32_t pass)
{
  uint64_t resourceID = o->At(1)->U64();
  InitResourceIDMapIter init_res_it = tracker->InitResourceFind(resourceID);

  const char *comment = "";
  if(!tracker->ResourceNeedsReset(resourceID, pass == ID_INIT, pass == ID_PRERESET))
  {
    comment = "// ";
  }

  const char *reset_size_name;
  if(pass == ID_INIT)
  {
    reset_size_name = tracker->GetMemInitSizeVar(o->At(1)->U64());
  }
  else
  {
    reset_size_name = tracker->GetMemResetSizeVar(o->At(1)->U64());
  }
  files[pass]->PrintLn("%sCopyResetBuffer(aux, VkBuffer_dst_%llu, VkBuffer_src_%llu, %s);", comment,
                       o->At(1)->U64(), o->At(1)->U64(), reset_size_name);
}

void CodeWriter::AcquireNextImage(ExtObject *o, uint32_t pass)
{
  AddNamedVar("uint32_t", "acquired_frame");
  files[pass]->PrintLn(
      "vkAcquireNextImageKHR(%s, %s, 0xFFFFFFFFFF, aux.semaphore, NULL, &acquired_frame);",
      tracker->GetDeviceVar(), tracker->GetSwapchainVar());
}

void CodeWriter::BeginCommandBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(1), "", pass);
  files[pass]
      ->PrintLn("%s(%s, &%s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->Name())
      .PrintLn("}");
}

void CodeWriter::EndCommandBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()));
}

void CodeWriter::WaitForFences(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  files[pass]
      ->PrintLn("// %s(%s, %llu, %s, %llu, %llu);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->U64(), o->At(2)->Name(),
                o->At(3)->U64(), o->At(4)->U64())
      .PrintLn("}");
}

void CodeWriter::GetFenceStatus(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("// %s(%s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       tracker->GetResourceVar(o->At(1)->U64()));
}

void CodeWriter::ResetFences(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  files[pass]
      ->PrintLn("// %s(%s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->U64(), o->At(2)->Name())
      .PrintLn("}");
}

void CodeWriter::GetEventStatus(ExtObject *o, uint32_t pass)
{
  GenericEvent(o, pass);
}

void CodeWriter::SetEvent(ExtObject *o, uint32_t pass)
{
  GenericEvent(o, pass);
}

void CodeWriter::ResetEvent(ExtObject *o, uint32_t pass)
{
  GenericEvent(o, pass);
}

void CodeWriter::QueueSubmit(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  files[pass]->PrintLn("%s(%s, %llu, %s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->U64(), o->At(2)->Name(), tracker->GetResourceVar(o->At(3)->U64()));

  if(tracker->IsValidNonNullResouce(o->At(3)->U64()))
  {
    files[pass]
        ->PrintLn("VkResult result = vkWaitForFences(%s, 1, &%s, VK_TRUE, 0xFFFFFFFF);",
                  tracker->GetDeviceVar(), tracker->GetResourceVar(o->At(3)->U64()))
        .PrintLn("assert(result == VK_SUCCESS);")
        .PrintLn("result = vkResetFences(%s, 1, &%s);", tracker->GetDeviceVar(),
                 tracker->GetResourceVar(o->At(3)->U64()))
        .PrintLn("assert(result == VK_SUCCESS);");
  }

  files[pass]->PrintLn("}");
}

void CodeWriter::QueueWaitIdle(ExtObject *o, uint32_t pass)
{
  GenericWaitIdle(o, pass);
}

void CodeWriter::DeviceWaitIdle(ExtObject *o, uint32_t pass)
{
  GenericWaitIdle(o, pass);
}

void CodeWriter::EndFramePresent(ExtObject *o, uint32_t pass)
{
  uint64_t semaphore_count = 0;

  for(U64MapIter it = tracker->SignalSemaphoreBegin(); it != tracker->SignalSemaphoreEnd(); it++)
    if(it->second > 0)    // semaphore was signaled but not waited for.
      semaphore_count++;

  files[pass]->PrintLn("{");
  if(semaphore_count > 0)
  {
    files[pass]->PrintLn("VkSemaphore pWaitSemaphore[%llu] = {", semaphore_count);
    for(U64MapIter it = tracker->SignalSemaphoreBegin(); it != tracker->SignalSemaphoreEnd(); it++)
      if(it->second > 0)
        files[pass]->PrintLn("%s,", tracker->GetResourceVar(it->first));
    files[pass]->PrintLn("};");
  }
  else
    files[pass]->PrintLn("VkSemaphore* pWaitSemaphore = NULL;");

  files[pass]
      ->PrintLn("VkPresentInfoKHR PresentInfo = {")
      .PrintLn("VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,")
      .PrintLn("NULL,")
      .PrintLn("%llu, pWaitSemaphore,", semaphore_count)
      .PrintLn("1, &%s,", tracker->GetSwapchainVar())
      .PrintLn("&acquired_frame,")
      .PrintLn("NULL")
      .PrintLn("};")
      .PrintLn("VkResult result = %svkQueuePresentKHR(%s, &PresentInfo);", shimPrefix,
               tracker->GetPresentQueueVar())
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");
}

void CodeWriter::EndFrameWaitIdle(ExtObject *o, uint32_t pass)
{
  for(U64MapIter it = tracker->SubmittedQueuesBegin(); it != tracker->SubmittedQueuesEnd(); it++)
    files[pass]->PrintLn("vkQueueWaitIdle(VkQueue_%llu);", it->second);
}

void CodeWriter::FlushMappedMemoryRanges(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  ExtObject *device = o->At(0);
  ExtObject *regions = o->At(2);
  ExtObject *memory = regions->At(2);
  ExtObject *buffer = o->At(3);

  MemAllocWithResourcesMapIter it = tracker->MemAllocFind(memory->U64());

  RDCASSERT(!regions->IsArray());
  LocalVariable(regions, "", pass);

  // TODO(akharlamov) calculate real map range based on existing map range, captured reqs and
  // runtime reqs.
  // For now assume Map is copying the device memory in it's entirety
  if(it->second.BoundResourceCount() == 0)
  {
    RDCWARN("Memory resource flushed, but doesn't have any bound resources.");
    // This is easy, no resources where bound to memory
    // I'll keep this unimplemented until I see that this ever happens (?)
  }
  else
  {
    const char *map_update_func = "MapUpdate";
    files[pass]
        ->PrintLn("uint8_t* data = NULL;")
        .PrintLn(
            "VkResult result = vkMapMemory(%s, %s, 0, VK_WHOLE_SIZE, 0, (void** ) &data); // RDOC: "
            "map the whole thing, but only copy the right subregions later",
            tracker->GetResourceVar(device->U64()), tracker->GetResourceVar(memory->U64()))
        .PrintLn("assert(result == VK_SUCCESS);")
        .PrintLn("%s(aux, data, buffer_%llu.data(), %s, %s, %s, %s);", map_update_func,
                 buffer->U64(), regions->Name(), tracker->GetMemAllocInfoVar(memory->U64()),
                 tracker->GetMemRemapVar(memory->U64()), tracker->GetResourceVar(device->U64()))
        .PrintLn("assert(result == VK_SUCCESS);")
        .PrintLn("vkUnmapMemory(%s, %s);", tracker->GetResourceVar(device->U64()),
                 tracker->GetResourceVar(memory->U64()));
  }
  files[pass]->PrintLn("}");
}

void CodeWriter::UpdateDescriptorSets(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  LocalVariable(o->At(4), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %llu, %s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->U64(), o->At(2)->Name(), o->At(3)->U64(), o->At(4)->Name())
      .PrintLn("}");
}

void CodeWriter::UpdateDescriptorSetWithTemplate(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(3), "", pass);

  files[pass]
      ->PrintLn("%s(%s, %llu, %s, %llu, %s); // UpdateDescriptorSetWithTemplate",
                "vkUpdateDescriptorSets", tracker->GetResourceVar(o->At(0)->U64()),
                o->At(3)->Size(), o->At(3)->Name(), 0, "NULL")
      .PrintLn("}");
#if 1
  return;
#else
  ExtObject *write_desc_set = o->At(3);

  ExtObject *desc_set_template = o->At(2);
  ResourceWithViewsMapIter template_it = tracker->ResourceCreateFind(desc_set_template->U64());
  RDCASSERT(template_it != tracker->ResourceCreateEnd());

  ExtObject *template_ci = template_it->second.sdobj->At(1);
  ExtObject *desc_update_entry = template_ci->At(4);
  RDCASSERT(desc_update_entry->Size() == write_desc_set->Size());

  uint32_t wds_byte_size = 0;
  uint32_t due_byte_size = 0;

  for(uint32_t i = 0; i < write_desc_set->Size(); i++)
  {
    ExtObject *wds = write_desc_set->At(i);
    ExtObject *due = desc_update_entry->At(i);

    ExtObject *wds_count = wds->At(5);
    ExtObject *due_count = due->At(2);
    RDCASSERT(due_count->U64() == wds_count->U64());
    ExtObject *due_offset = due->At(4);
    ExtObject *due_stride = due->At(5);
    ExtObject *image = wds->At(7);
    ExtObject *buffer = wds->At(8);
    ExtObject *texel = wds->At(9);
    if(image->Size() > 0)
      wds_byte_size = std::max<uint64_t>(
          wds_count->U64() * sizeof(VkDescriptorImageInfo) + due_offset->U64(), wds_byte_size);
    if(buffer->Size() > 0)
      wds_byte_size = std::max<uint64_t>(
          wds_count->U64() * sizeof(VkDescriptorBufferInfo) + due_offset->U64(), wds_byte_size);
    if(texel->Size() > 0)
      wds_byte_size = std::max<uint64_t>(
          wds_count->U64() * sizeof(VkBufferView) + due_offset->U64(), wds_byte_size);

    due_byte_size =
        std::max<uint64_t>(due_count->U64() * due->At(5)->U64() + due_offset->U64(), due_byte_size);
  }
  RDCASSERT(due_byte_size >= wds_byte_size);

  files[pass]->PrintLn("std::vector<uint8_t> Data(%u);", due_byte_size);

  for(uint64_t i = 0; i < write_desc_set->Size(); i++)
  {
    ExtObject *wds = write_desc_set->At(i);
    ExtObject *due = desc_update_entry->At(i);

    ExtObject *count = wds->At(5);
    ExtObject *image = wds->At(7);
    ExtObject *buffer = wds->At(8);
    ExtObject *texel = wds->At(9);
    ExtObject *due_offset = due->At(4);
    ExtObject *due_stride = due->At(5);
    ExtObject *info = NULL;
    if(image->Size() > 0 && buffer->Size() == 0 && texel->Size() == 0)
      info = image;
    if(image->Size() == 0 && buffer->Size() > 0 && texel->Size() == 0)
      info = buffer;
    if(image->Size() == 0 && buffer->Size() == 0 && texel->Size() > 0)
      info = texel;
    RDCASSERT(info != NULL);

    for(uint64_t j = 0; j < count->U64(); j++)
    {
      if(info != NULL)
      {
        files[pass]->PrintLn(
            "memcpy(Data.data() + %llu /*rdoc:offset*/ + %llu /*rdoc:stride*/"
            ", &%s_%llu[%llu], sizeof(%s_%llu[%llu]));",
            due_offset->U64(), due_stride->U64() * j, info->Name(), i, j, info->Name(), i, j);
      }
    }
  }

  files[pass]
      ->PrintLn("%s(%s, %s, %s, (const void *) %Data.data());", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
                tracker->GetResourceVar(o->At(2)->U64()))
      .PrintLn("}");
#endif
}

void CodeWriter::UnmapMemory(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("// %s(%s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       tracker->GetResourceVar(o->At(1)->U64()));
}

void CodeWriter::CmdBeginRenderPass(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(1), "", pass);
  files[pass]
      ->PrintLn("%s(%s, &%s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->Name(), o->At(2)->Str())
      .PrintLn("}");
}

void CodeWriter::CmdNextSubpass(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->Str());
}

void CodeWriter::CmdExecuteCommands(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->U64(), o->At(2)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdEndRenderPass(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()));
}

void CodeWriter::CmdSetViewport(ExtObject *o, uint32_t pass)
{
  GenericCmdSetRectTest(o, pass);
}

void CodeWriter::CmdSetScissor(ExtObject *o, uint32_t pass)
{
  GenericCmdSetRectTest(o, pass);
}

void CodeWriter::CmdBindDescriptorSets(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(5), "", pass);
  LocalVariable(o->At(7), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %llu, %llu, %s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->ValueStr().c_str(),
                tracker->GetResourceVar(o->At(2)->U64()), o->At(3)->U64(), o->At(4)->U64(),
                o->At(5)->Name(), o->At(6)->U64(), o->At(7)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdBindPipeline(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->ValueStr().c_str(), tracker->GetResourceVar(o->At(2)->U64()));
}

void CodeWriter::CmdBindVertexBuffers(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(3), "", pass);
  LocalVariable(o->At(4), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %llu, %llu, %s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->U64(), o->At(2)->U64(), o->At(3)->Name(), o->At(4)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdBindIndexBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->U64(),
                       o->At(3)->ValueStr().c_str());
}

void CodeWriter::CmdDraw(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %llu, %llu, %llu, %llu);", o->Name(),
                       tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->U64(), o->At(2)->U64(),
                       o->At(3)->U64(), o->At(4)->U64());
}

void CodeWriter::CmdDrawIndexed(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %llu, %llu, %llu, %lld, %llu);", o->Name(),
                       tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->U64(), o->At(2)->U64(),
                       o->At(3)->U64(), o->At(4)->I64(), o->At(4)->U64());
}

void CodeWriter::CmdDrawIndirect(ExtObject *o, uint32_t pass)
{
  GenericCmdDrawIndirect(o, pass);
}

void CodeWriter::CmdDrawIndexedIndirect(ExtObject *o, uint32_t pass)
{
  GenericCmdDrawIndirect(o, pass);
}

void CodeWriter::CmdDispatch(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %llu, %llu, %llu);", o->Name(),
                       tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->U64(), o->At(2)->U64(),
                       o->At(3)->U64());
}

void CodeWriter::CmdDispatchIndirect(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %s, %llu);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->U64());
}

void CodeWriter::CmdSetEvent(ExtObject *o, uint32_t pass)
{
  GenericCmdEvent(o, pass);
}

void CodeWriter::CmdResetEvent(ExtObject *o, uint32_t pass)
{
  GenericCmdEvent(o, pass);
}

void CodeWriter::CmdWaitEvents(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  LocalVariable(o->At(6), "", pass);
  LocalVariable(o->At(8), "", pass);
  LocalVariable(o->At(10), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %llu, %s, %s, %s, %llu, %s, %llu, %s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->U64(), o->At(2)->Name(),
                o->At(3)->Str(), o->At(4)->Str(), o->At(5)->U64(), o->At(6)->Name(),
                o->At(7)->U64(), o->At(8)->Name(), o->At(9)->U64(), o->At(10)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdPipelineBarrier(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(5), "", pass);
  LocalVariable(o->At(7), "", pass);
  LocalVariable(o->At(9), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %s, %llu, %s, %llu, %s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->Str(), o->At(2)->Str(),
                o->At(3)->Str(), o->At(4)->U64(), o->At(5)->Name(), o->At(6)->U64(),
                o->At(7)->Name(), o->At(8)->U64(), o->At(9)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdPushConstants(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %s, %s, %llu, %llu, (const void*) buffer_%llu.data());", o->Name(),
                       tracker->GetResourceVar(o->At(0)->U64()),
                       tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->Str(), o->At(3)->U64(),
                       o->At(4)->U64(), o->At(5)->U64());
}

void CodeWriter::CmdSetDepthBias(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %f, %f, %f);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->D64(), o->At(2)->D64(), o->At(3)->D64());
}

void CodeWriter::CmdSetDepthBounds(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %f, %f);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->D64(), o->At(2)->D64());
}

void CodeWriter::CmdSetStencilCompareMask(ExtObject *o, uint32_t pass)
{
  GenericCmdSetStencilParam(o, pass);
}

void CodeWriter::CmdSetStencilWriteMask(ExtObject *o, uint32_t pass)
{
  GenericCmdSetStencilParam(o, pass);
}

void CodeWriter::CmdSetStencilReference(ExtObject *o, uint32_t pass)
{
  GenericCmdSetStencilParam(o, pass);
}

void CodeWriter::CmdSetLineWidth(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %f);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->D64());
}

void CodeWriter::CmdCopyBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(4), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                tracker->GetResourceVar(o->At(1)->U64()), tracker->GetResourceVar(o->At(2)->U64()),
                o->At(3)->U64(), o->At(4)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdUpdateBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]
      ->PrintLn("{")
      .PrintLn("%s(%s, %s, %llu, %llu, (const void* )buffer_%llu.data());", o->Name(),
               tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
               o->At(2)->U64(), o->At(3)->U64(), o->At(4)->U64())
      .PrintLn("}");
}

void CodeWriter::CmdFillBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]
      ->PrintLn("{")
      .PrintLn("%s(%s, %s, %llu, %llu, %llu);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
               tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->U64(), o->At(3)->U64(),
               o->At(4)->U64())
      .PrintLn("}");
}

void CodeWriter::CmdCopyImage(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(6), "", pass);
  const char *dst_image = tracker->GetResourceVar(o->At(3)->U64());
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %s, %s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
                o->At(2)->Str(), dst_image, o->At(4)->Str(), o->At(5)->U64(), o->At(6)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdBlitImage(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(6), "", pass);
  const char *dst_image = tracker->GetResourceVar(o->At(3)->U64());
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %s, %s, %llu, %s, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
                o->At(2)->Str(), dst_image, o->At(4)->Str(), o->At(5)->U64(), o->At(6)->Name(),
                o->At(7)->Str())
      .PrintLn("}");
}

void CodeWriter::CmdResolveImage(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(6), "", pass);
  const char *dst_image = tracker->GetResourceVar(o->At(3)->U64());
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %s, %s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
                o->At(2)->Str(), dst_image, o->At(4)->Str(), o->At(5)->U64(), o->At(6)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdSetBlendConstants(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(1), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()), o->At(1)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdCopyBufferToImage(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(5), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                tracker->GetResourceVar(o->At(1)->U64()), tracker->GetResourceVar(o->At(2)->U64()),
                o->At(3)->Str(), o->At(4)->U64(), o->At(5)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdCopyImageToBuffer(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(5), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, %s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->Str(),
                tracker->GetResourceVar(o->At(3)->U64()), o->At(4)->U64(), o->At(5)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdClearAttachments(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(2), "", pass);
  LocalVariable(o->At(4), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %llu, %s, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->U64(), o->At(2)->Name(), o->At(3)->U64(), o->At(4)->Name())
      .PrintLn("}");
}

// TODO(akharlamov) see to replace ClearDS and ClearImage functions with one.
void CodeWriter::CmdClearDepthStencilImage(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(3), "", pass);
  LocalVariable(o->At(5), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, &%s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
                o->At(2)->Str(), o->At(3)->Name(), o->At(4)->U64(), o->At(5)->Name())
      .PrintLn("}");
}

void CodeWriter::CmdClearColorImage(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(3), "", pass);
  LocalVariable(o->At(5), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %s, %s, &%s, %llu, %s);", o->Name(),
                tracker->GetResourceVar(o->At(0)->U64()), tracker->GetResourceVar(o->At(1)->U64()),
                o->At(2)->Str(), o->At(3)->Name(), o->At(4)->U64(), o->At(5)->Name())
      .PrintLn("}");
}

}    // namespace vk_cpp_codec
