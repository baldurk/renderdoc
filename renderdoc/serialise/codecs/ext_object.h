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
#pragma once

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "core/core.h"
#include "driver/vulkan/vk_common.h"
#include "driver/vulkan/vk_resources.h"
#include "serialise/rdcfile.h"
#include "intervals.h"

namespace vk_cpp_codec
{
extern const char *VkImageLayoutStrings[15];

inline uint32_t as_uint32(uint64_t val)
{
  RDCASSERT(val <= UINT32_MAX);
  return static_cast<uint32_t>(val);
}

// Extend the SDObject with some API calls to simplify code.
struct ExtObject : public SDObject
{
  ExtObject(const char *n, const char *t) : SDObject(n, t) {}
  ExtObject(const char *n, const char *t, uint64_t value, SDBasic basetype = SDBasic::UnsignedInteger)
      : SDObject(n, t)
  {
    type.basetype = basetype;
    data.basic.u = value;
  }

  ExtObject(const char *n, const char *t, std::string value) : SDObject(n, t)
  {
    type.basetype = SDBasic::String;
    data.str = value;
  }

  ExtObject(const char *n, const char *t, uint64_t value, std::string valueStr) : SDObject(n, t)
  {
    type.basetype = SDBasic::Enum;
    data.basic.u = value;
    data.str = valueStr;
    type.byteSize = 4;
    type.flags = SDTypeFlags::HasCustomString;
  }

  void PushOne(ExtObject *o) { data.children.push_back(o); }
  void RemoveOne(ExtObject *o) { data.children.removeOne(o); }
  void RemoveOne(uint64_t i) { data.children.erase(i); }
  const char *Type()
  {
    // TODO(akharlamov) move this to filtering stage in tracker class.
    static std::string str = "const char* ";
    static std::string pipeline_stage_flags = "VkPipelineStageFlags";

    // Just-in-time replacement for types
    if(IsString() || type.name == "string")
      return str.c_str();
    else if(type.name == "VkPipelineStageFlagBits")
      return pipeline_stage_flags.c_str();

    return type.name.c_str();
  }

  const char *Name() { return name.c_str(); }
  const char *Str() { return data.str.c_str(); }
  uint64_t &U64() { return data.basic.u; }
  uint64_t I64() { return data.basic.i; }
  double D64()
  {
    if(isnan(data.basic.d))
      data.basic.d = 1.0f;
    return data.basic.d;
  }
  ExtObject *At(uint64_t i)
  {
    RDCASSERT(i < Size());
    return static_cast<ExtObject *>(data.children[i]);
  }
  ExtObject *At(std::string child)
  {
    ExtObject *ext = static_cast<ExtObject *>(FindChild(child.c_str()));
    RDCASSERT(ext != NULL);
    return ext;
  }
  bool Exists(std::string child)
  {
    ExtObject *ext = static_cast<ExtObject *>(FindChild(child.c_str()));
    return (ext != NULL);
  }
  ExtObject *operator[](uint64_t i) { return At(i); }
  uint64_t Size() { return data.children.size(); }
  bool IsStruct() { return type.basetype == SDBasic::Struct; }
  bool IsArray() { return type.basetype == SDBasic::Array; }
  bool IsNULL()
  {
    return type.basetype == SDBasic::Null || (type.basetype == SDBasic::Array && Size() == 0);
  }
  bool IsU64() { return type.basetype == SDBasic::UnsignedInteger; }
  bool IsI64() { return type.basetype == SDBasic::SignedInteger; }
  bool IsD64() { return type.basetype == SDBasic::Float; }
  bool IsString() { return type.basetype == SDBasic::String; }
  bool IsFixedArray() { return IsArray() && (type.flags == SDTypeFlags::FixedArray); }
  bool IsFixedArray(uint64_t size)
  {
    return IsArray() && (type.flags == SDTypeFlags::FixedArray) && Size() <= size;
  }
  bool IsVariableArray() { return IsArray() && (type.flags != SDTypeFlags::FixedArray); }
  bool IsEnum() { return type.basetype == SDBasic::Enum; }
  bool IsBuffer() { return type.basetype == SDBasic::Buffer; }
  bool IsPointer() { return type.flags == SDTypeFlags::Nullable && (Size() != 0); }
  bool IsResource() { return type.basetype == SDBasic::Resource; }
  bool IsUnion()
  {
    return ((type.basetype == SDBasic::Struct) && (type.flags & SDTypeFlags::Union));
  }
  bool IsSimpleType()
  {
    if(IsNULL())
      return true;
    return !IsStruct() && !IsArray() && !IsPointer();
  }

  // Is it possible to fully inline the data structure declaration?
  bool IsInlineable()
  {
    if(IsVariableArray() && !IsNULL())
      return false;
    if(IsStruct() && IsPointer() && !IsNULL())
      return false;

    for(uint64_t i = 0; i < Size(); i++)
      if(!At(i)->IsInlineable())
        return false;

    return true;
  }

  std::string ValueStr()
  {
    std::string result;

    RDCASSERT(IsSimpleType());

    if(IsBuffer())
    {
      std::string buf_name = Str();
      if(buf_name.empty())
        buf_name = "buffer_" + std::to_string(U64());
      result = buf_name + ".data()";
      // just-in-time fix for vkCreatShaderModule pCode variable
      if(name == "pCode")
        result = "(const uint32_t*) " + result;
    }
    if(IsNULL())
      result = "NULL";
    if(IsU64())
    {
      result = std::to_string(U64()) + "u";
    }
    if(IsI64())
      result = std::to_string(I64());
    if(IsD64())
    {
      result = std::to_string(D64()) + "f";
    }
    if(IsEnum())
    {
      result = data.str;
      size_t open_bracket = result.find("<");
      size_t close_bracket = result.find(">");
      if(open_bracket != std::string::npos && close_bracket != std::string::npos)
      {
        result.replace(open_bracket, 1, "(");
        result.replace(close_bracket, 1, ")");
      }
      return result.c_str();
    }
    if(IsString())
      result = std::string("\"") + data.str.c_str() + std::string("\"");

    return result.c_str();
  }
  uint32_t ChunkID() const
  {
    RDCASSERT(type.basetype == SDBasic::Chunk);
    SDChunk *chunk = (SDChunk *)(this);
    return chunk->metadata.chunkID;
  }
};

inline ExtObject *as_ext(SDObject *sdo)
{
  return static_cast<ExtObject *>(sdo);
}

typedef std::vector<ExtObject *> ExtObjectVec;
typedef ExtObjectVec::iterator ExtObjectVecIter;

typedef std::map<uint64_t, ExtObject *> ExtObjectIDMap;
typedef ExtObjectIDMap::iterator ExtObjectIDMapIter;
typedef std::pair<uint64_t, ExtObject *> ExtObjectIDMapPair;

}    // namespace vk_cpp_codec