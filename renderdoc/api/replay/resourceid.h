/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "apidefs.h"
#include "stringise.h"

#ifdef RENDERDOC_EXPORTS
struct ResourceId;

namespace ResourceIDGen
{
// the only function allowed access to ResourceId internals, for allocating a new ID
ResourceId GetNewUniqueID();
};
#endif

// We give every resource a globally unique ID so that we can differentiate
// between two textures allocated in the same memory (after the first is freed)
//
// it's a struct around a uint64_t to aid in template selection
DOCUMENT(R"(This is an opaque identifier that uniquely locates a resource.

.. note::
  These IDs do not overlap ever - textures, buffers, shaders and samplers will all have unique IDs
  and do not reuse the namespace. Likewise the IDs assigned for resources during capture  are not
  re-used on replay - the corresponding resources created on replay to stand-in for capture-time
  resources are given unique IDs and a mapping is stored to between the capture-time resource and
  the replay-time one.
)");
struct ResourceId
{
  ResourceId() : id() {}
  ResourceId(const ResourceId &) = default;
  ResourceId &operator=(const ResourceId &) = default;
#if !defined(SWIG)
  ResourceId(ResourceId &&) = default;
  ResourceId &operator=(ResourceId &&) = default;
#endif

  DOCUMENT(R"(A helper function that explicitly creates an empty/invalid/null :class:`ResourceId`.

:return: an empty/invalid/null :class:`ResourceId`.
:rtype: ResourceId
)");
  inline static ResourceId Null() { return ResourceId(); }
  DOCUMENT("Compares two ``ResourceId`` objects for equality.");
  bool operator==(const ResourceId u) const { return id == u.id; }
  DOCUMENT("Compares two ``ResourceId`` objects for inequality.");
  bool operator!=(const ResourceId u) const { return id != u.id; }
  DOCUMENT("Compares two ``ResourceId`` objects for less-than.");
  bool operator<(const ResourceId u) const { return id < u.id; }
#if defined(RENDERDOC_QT_COMPAT)
  operator QVariant() const { return QVariant::fromValue(*this); }
#endif

private:
  uint64_t id;

#ifdef RENDERDOC_EXPORTS
  friend ResourceId ResourceIDGen::GetNewUniqueID();
  friend struct std::hash<ResourceId>;
#endif
};

// declare metatype/reflection for ResourceId here as the struct itself is declared before including
// all relevant headers above
#if defined(RENDERDOC_QT_COMPAT)
Q_DECLARE_METATYPE(ResourceId);
#endif

DECLARE_REFLECTION_STRUCT(ResourceId);

// add a std::hash overload so ResourceId can be used in hashmaps
#ifdef RENDERDOC_EXPORTS
namespace std
{
template <>
struct hash<ResourceId>
{
  std::size_t operator()(const ResourceId &id) const { return std::hash<uint64_t>()(id.id); }
};
}
#endif
