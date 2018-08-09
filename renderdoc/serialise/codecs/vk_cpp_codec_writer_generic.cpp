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

#include "common/common.h"
#include "core/core.h"
#include "driver/vulkan/vk_common.h"
#include "driver/vulkan/vk_resources.h"
#include "serialise/rdcfile.h"
#include "ext_object.h"
#include "vk_cpp_codec_state.h"
#include "vk_cpp_codec_tracker.h"
#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
void CodeWriter::InlineVariable(ExtObject *o, uint32_t pass)
{
  RDCASSERT(o->IsInlineable());

  files[pass]->PrintLn("{ /* %s = */", o->Name());
  uint64_t size = o->IsUnion() ? 1 : o->Size();
  for(uint64_t i = 0; i < size; i++)
  {
    std::string add_suffix;
    ExtObject *node = tracker->CopiesAdd(o, i, add_suffix);
    if(!node->IsSimpleType())
    {
      InlineVariable(node, pass);
    }
    else if(node->IsResource())
    {
      const char *name = tracker->GetResourceVar(node->U64());
      files[pass]->PrintLn("/* %s = */ %s,", node->Name(), name);
    }
    else
      files[pass]->PrintLn("/* %s = */ %s,", node->Name(), node->ValueStr().c_str());
  }
  files[pass]->PrintLn("},");
}

void CodeWriter::LocalVariable(ExtObject *o, std::string suffix, uint32_t pass)
{
  if(o->IsUnion() || o->IsStruct() || o->IsArray())
  {
    uint64_t size = o->IsUnion() ? std::min<uint64_t>(1, o->Size()) : o->Size();
    // Go through all the children and look for complex structures or variable-
    // size array. For each of those, declare and initialize them separately.
    for(uint64_t i = 0; i < size; i++)
    {
      // Handle cases when the member is a complex data type, such as a complex
      // structure or a variable sized array.
      if(!o->At(i)->IsInlineable())
      {
        std::string add_suffix;
        ExtObject *node = tracker->CopiesAdd(o, i, add_suffix);
        LocalVariable(node, suffix + add_suffix, pass);
      }
    }
    // Now, declare and initialize the data type. Simple members get inlined.
    // Complex structures or variable arrays get referenced by name.
    if(o->IsNULL())
    {
      files[pass]->PrintLn("%s* %s%s = NULL;", o->Type(), o->Name(), suffix.c_str());
    }
    else if(o->IsStruct() && !o->IsPointer())
    {
      files[pass]->PrintLn("%s %s%s = {", o->Type(), o->Name(), suffix.c_str());
    }
    else if(o->IsStruct() && o->IsPointer())
    {
      files[pass]->PrintLn("%s %s%s[1] = {", o->Type(), o->Name(), suffix.c_str());
    }
    else if(o->IsArray())
    {
      files[pass]->PrintLn("%s %s%s[%llu] = {", o->Type(), o->Name(), suffix.c_str(), size);
    }
    for(uint64_t i = 0; i < size; i++)
    {
      std::string add_suffix;
      ExtObject *node = tracker->CopiesAdd(o, i, add_suffix);
      if(!node->IsInlineable())
      {
        files[pass]->PrintLn("/* %s = */ %s%s,", node->Name(), node->Name(),
                             (suffix + add_suffix).c_str());
      }
      else if(!node->IsSimpleType() && node->IsInlineable())
        InlineVariable(node, pass);
      else if(node->IsResource())
      {
        files[pass]->PrintLn("/* %s = */ %s,", node->Name(), tracker->GetResourceVar(node->U64()));
      }
      else
        files[pass]->PrintLn("/* %s = */ %s,", node->Name(), node->ValueStr().c_str());
    }
    if(!o->IsNULL())
      files[pass]->PrintLn("};");
  }
}

void CodeWriter::GenericVkCreate(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *device = o->At(0);
  ExtObject *ci = o->At(1);
  ExtObject *vk_res = o->At(3);

  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *res_name = tracker->GetResourceVar(vk_res->Type(), vk_res->U64());

  files[pass]->PrintLn("{");
  LocalVariable(ci, "", pass);

  if(global_ci)
  {
    std::string ci_name = AddVar(ci->Type(), vk_res->U64());
    files[pass]->PrintLn("%s = %s;", ci_name.c_str(), ci->Name());
  }

  files[pass]
      ->PrintLn("VkResult result = %s(%s, &%s, NULL, &%s);", o->Name(), device_name, ci->Name(),
                res_name)
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");
}

void CodeWriter::GenericCreatePipelines(ExtObject *o, uint32_t pass, bool global_ci)
{
  ExtObject *device = o->At(0);
  ExtObject *cache = o->At(1);
  ExtObject *ci_count = o->At(2);
  ExtObject *ci = o->At(3);
  ExtObject *pipe = o->At(5);

  // CreateInfoCount must always be equal to '1'.
  // Create[Graphics|Compute]Pipelines can create multiple pipelines at the
  // same time, but RenderDoc splits these calls into multiple calls, one per
  // each pipeline object that is still alive at the time of capture.
  RDCASSERT(ci_count->U64() == 1);

  const char *device_name = tracker->GetResourceVar(device->U64());
  const char *cache_name = tracker->GetResourceVar(cache->U64());
  const char *pipe_name = tracker->GetResourceVar(pipe->Type(), pipe->U64());

  files[pass]->PrintLn("{");
  LocalVariable(ci, "", pass);
  files[pass]
      ->PrintLn("VkResult result = %s(%s, %s, 1, &%s, NULL, &%s);", o->Name(), device_name,
                cache_name, ci->Name(), pipe_name)
      .PrintLn("assert(result == VK_SUCCESS);")
      .PrintLn("}");
}

void CodeWriter::GenericEvent(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  files[pass]
      ->PrintLn("%s(%s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                tracker->GetResourceVar(o->At(1)->U64()))
      .PrintLn("}");
}

void CodeWriter::GenericWaitIdle(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()));
}

void CodeWriter::GenericCmdSetRectTest(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  LocalVariable(o->At(3), "", pass);
  files[pass]
      ->PrintLn("%s(%s, %llu, %llu, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                o->At(1)->U64(), o->At(2)->U64(), o->At(3)->Name())
      .PrintLn("}");
}
void CodeWriter::GenericCmdSetStencilParam(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("%s(%s, %s, %llu);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                       o->At(1)->Str(), o->At(2)->U64());
}

void CodeWriter::GenericCmdEvent(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn("{");
  files[pass]
      ->PrintLn("%s(%s, %s, %s);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
                tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->Str())
      .PrintLn("}");
}
void CodeWriter::GenericCmdDrawIndirect(ExtObject *o, uint32_t pass)
{
  files[pass]->PrintLn(
      "%s(%s, %s, %llu, %llu, %llu);", o->Name(), tracker->GetResourceVar(o->At(0)->U64()),
      tracker->GetResourceVar(o->At(1)->U64()), o->At(2)->U64(), o->At(3)->U64(), o->At(4)->U64());
}
}