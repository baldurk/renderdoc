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
#pragma once

#include <string.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "serialise/rdcfile.h"

namespace vk_cpp_codec
{
inline const char *Type(SDObject *ptr)
{
  // (akharlamov) Moving this to filtering stage in TraceTracker class
  // isn't trivial. Patching type.name, when done in Type() is only applied
  // for SDObjects that store data structures, and patching them during filtering
  // stage means the filter stage would need to touch on most of Vulkan API OR
  // crawl through the entire SDObject list and patch every object ignoring Vulkan
  // specifics.

  // Vulkan doesn't use std::string, so need to cast it to const char *
  if(ptr->IsString() || ptr->type.name == "string")
    return "const char* ";

  return ptr->type.name.c_str();
}

inline std::string ValueStr(SDObject *ptr)
{
  RDCASSERT(ptr->IsSimpleType());
  std::string result;

  if(ptr->IsBuffer())
  {
    std::string buf_name = ptr->AsString();
    RDCASSERT(!buf_name.empty());
    // A value for for a Buffer is it's $name.data().
    result = buf_name + ".data()";
    // just-in-time fix for vkCreatShaderModule pCode variable
    if(ptr->name == "pCode")
      result = "(const uint32_t*) " + result;
  }
  else if(ptr->IsNULL())
  {
    result = "NULL";
  }
  else if(ptr->IsUInt())
  {
    result = std::to_string(ptr->AsUInt64()) + "u";
  }
  else if(ptr->IsInt())
  {
    result = std::to_string(ptr->AsInt64());
  }
  else if(ptr->IsFloat())
  {
    if(isnan(ptr->data.basic.d))
      ptr->data.basic.d = 1.0f;
    result = std::to_string(ptr->AsDouble()) + "f";
  }
  else if(ptr->IsEnum())
  {
    result = ptr->data.str;
  }
  else if(ptr->IsString())
  {
    std::string escaped;
    escaped.reserve(ptr->data.str.size());
    for(char c : ptr->data.str)
    {
      switch(c)
      {
        case '\a': escaped += "\\a"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        case '\v': escaped += "\\v"; break;

        case '"':
        case '\\':
          escaped.push_back('\\');
          escaped.push_back(c);
          break;

        default:
          if(c < 32 || c > 127)
          {
            char buf[8] = {};
            snprintf(buf, sizeof(buf), "\\x%02X", c);
            escaped += buf;
          }
          else
          {
            escaped.push_back(c);
          }
          break;
      }
    }
    result = std::string("\"") + escaped + std::string("\"");
  }
  return result;
}

inline uint64_t CanonicalUnionBranch(SDObject *ptr)
{
  if(ptr->type.name == "VkClearValue")
  {
    return 0;    // Use `color`
  }
  else if(ptr->type.name == "VkClearColorValue")
  {
    return 2;    // Use `uint32`
  }
  // This function must be modified further to return the index of the canonical branch, which
  // should be chosen so that it's size is equal to the size of the entire union, and so that the
  // values can be represented exactly (e.g., not floating point).
  RDCERR("Attempting to output an unknown union type %s", ptr->type.name.c_str());
  return 0;
}

typedef std::vector<SDObject *> SDObjectVec;
typedef SDObjectVec::iterator SDObjectVecIter;

typedef std::map<uint64_t, SDObject *> SDObjectIDMap;
typedef SDObjectIDMap::iterator SDObjectIDMapIter;
typedef std::pair<uint64_t, SDObject *> SDObjectIDMapPair;

typedef std::map<uint64_t, SDObjectVec> SDObjectVecIDMap;
typedef SDObjectVecIDMap::iterator SDObjectVecIDMapIter;
typedef std::pair<uint64_t, SDObjectVec> SDObjectVecIDMapPair;

typedef std::vector<SDChunk *> SDChunkVec;
typedef SDChunkVec::iterator SDChunkVecIter;

typedef std::map<uint64_t, SDChunk *> SDChunkIDMap;
typedef SDChunkIDMap::iterator SDChunkIDMapIter;
typedef std::pair<uint64_t, SDChunk *> SDChunkIDMapPair;

typedef std::map<uint64_t, SDChunkVec> SDChunkVecIDMap;
typedef SDChunkVecIDMap::iterator SDChunkVecIDMapIter;
typedef std::pair<uint64_t, SDChunkVec> SDChunkVecIDMapPair;

}    // namespace vk_cpp_codec