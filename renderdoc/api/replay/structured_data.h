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

#include <stdint.h>
#include <functional>
#include "apidefs.h"
#include "rdcarray.h"
#include "rdcstr.h"
#include "resourceid.h"
#include "stringise.h"

DOCUMENT(R"(The basic irreducible type of an object. Every other more complex type is built on these.

.. data:: Chunk

  A 'special' type indicating that the object is a chunk. A chunk can be treated like a
  :data:`Struct` otherwise. See :class:`SDChunk`.

.. data:: Struct

  A composite type with some number of children of different types, each child with its own name.
  May in some cases be empty, so the presence of children should not be assumed.

.. data:: Array

  A composite type with some number of children with an identical type and referred to purely by
  their index in the array. May be empty.

.. data:: Null

  An indicator that an object could be here, but is optional and is currently not present. See
  :data:`SDTypeFlags.Nullable`.

.. data:: Buffer

  An opaque byte buffer.

.. data:: String

  A string, encoded as UTF-8.

.. data:: Enum

  An enum value - stored as an integer but with a distinct set of possible named values.

.. data:: UnsignedInteger

  An unsigned integer.

.. data:: SignedInteger

  An signed integer.

.. data:: Float

  A floating point value.

.. data:: Boolean

  A boolean true/false value.

.. data:: Character

  A single byte character. Wide/multi-byte characters are not supported (these would be stored as a
  string with 1 character and multiple bytes in UTF-8).

.. data:: Resource

  A ResourceId. Equivalent to (and stored as) an 8-byte unsigned integer, but specifically contains
  the unique Id of a resource in a capture.
)");
enum class SDBasic : uint32_t
{
  Chunk,
  Struct,
  Array,
  Null,
  Buffer,
  String,
  Enum,
  UnsignedInteger,
  SignedInteger,
  Float,
  Boolean,
  Character,
  Resource,
};

DECLARE_REFLECTION_ENUM(SDBasic);

DOCUMENT(R"(Bitfield flags that could be applied to a type.

.. data:: NoFlags

  This type has no special properties.

.. data:: HasCustomString

  This type has a custom string. This could be used for example for enums, to display the string
  value of the enum as well as the integer storage, or perhaps for opaque types that should be
  displayed to the user as a string even if the underlying representation is not a string.

.. data:: Hidden

  This type is considered an implementation detail and should not typically be displayed to the user.

.. data:: Nullable

  This type is nullable and can sometimes be removed and replaced simply with a Null type. See
  :data:`SDBasic.Null`.

.. data:: NullString

  Special flag to indicate that this is a C-string which was NULL, not just empty.

.. data:: FixedArray

  Special flag to indicate that this is array was a fixed-size real array, rather than a complex
  container type or a pointer & length.

.. data:: Union

  Special flag to indicate that this is structure is stored as a union, meaning all children share
  the same memory and some external flag indicates which element is valid.

.. data:: Important

  Indicates that this object is important or significant, to aid in generating a summary/one-line
  view of a particular chunk by only including important children.

  This property can be recursive - so an important child which is a structure can have only some
  members which are important.

.. data:: ImportantChildren

  Indicates that only important children should be processed, as noted in :data:`Important`. This
  may appear on an object which has no important children - which indicates explicitly that there
  are no important children so when summarising no parameters should be shown.

.. data:: HiddenChildren

  Indicates that some children are marked as hidden. This can be important for cases where the
  number of children is important.
)");
enum class SDTypeFlags : uint32_t
{
  NoFlags = 0x0,
  HasCustomString = 0x1,
  Hidden = 0x2,
  Nullable = 0x4,
  NullString = 0x8,
  FixedArray = 0x10,
  Union = 0x20,
  Important = 0x40,
  ImportantChildren = 0x80,
  HiddenChildren = 0x100,
};

BITMASK_OPERATORS(SDTypeFlags);
DECLARE_REFLECTION_ENUM(SDTypeFlags);

struct SDObject;
struct SDChunk;

DOCUMENT("Details the name and properties of a structured type");
struct SDType
{
  SDType(const rdcinflexiblestr &n)
      : name(n), basetype(SDBasic::Struct), flags(SDTypeFlags::NoFlags), byteSize(0)
  {
  }
#if !defined(SWIG)
  SDType(rdcinflexiblestr &&n)
      : name(std::move(n)), basetype(SDBasic::Struct), flags(SDTypeFlags::NoFlags), byteSize(0)
  {
  }
#endif

  DOCUMENT("The name of this type.");
  rdcinflexiblestr name;

  DOCUMENT("The :class:`SDBasic` category that this type belongs to.");
  SDBasic basetype;

  DOCUMENT("The :class:`SDTypeFlags` flags for this type.");
  SDTypeFlags flags;

  DOCUMENT(R"(The size in bytes that an instance of this type takes up.

This is only valid for whole chunks (where it contains the whole chunk size), for buffers that have
an arbitrary size, or for basic types such as integers and floating point values where it gives the
size/precision of the type.

For variable size types like structs, arrays, etc it will be set to 0.
)");
  uint64_t byteSize;

protected:
  friend struct SDObject;
  friend struct SDChunk;

  SDType() = default;
  SDType(const SDType &) = default;
  SDType &operator=(const SDType &) = default;
  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete(void *p) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(SDType);

DOCUMENT(R"(Bitfield flags that could be applied to an :class:`SDChunk`.

.. data:: NoFlags

  This chunk has no special properties.

.. data:: OpaqueChunk

  This chunk wasn't supported for decoding or was skipped for another reason and was detailed as
  an opaque byte stream. It should be preserved as-is and will remain in native RDC format.

.. data:: HasCallstack

  This chunk has a callstack. Used to indicate the presence of a callstack even if it's empty
  (perhaps due to failure to collect the stack frames).
)");
enum class SDChunkFlags : uint64_t
{
  NoFlags = 0x0,
  OpaqueChunk = 0x1,
  HasCallstack = 0x2,
};

BITMASK_OPERATORS(SDChunkFlags);
DECLARE_REFLECTION_ENUM(SDChunkFlags);

DOCUMENT("The metadata that goes along with a :class:`SDChunk` to detail how it was recorded.");
struct SDChunkMetaData
{
  DOCUMENT("");
  SDChunkMetaData() = default;
  SDChunkMetaData(const SDChunkMetaData &) = default;
  SDChunkMetaData &operator=(const SDChunkMetaData &) = default;

  DOCUMENT("The internal chunk ID - unique given a particular driver in use.");
  uint32_t chunkID = 0;

  DOCUMENT("The :class:`SDChunkFlags` for this chunk.");
  SDChunkFlags flags = SDChunkFlags::NoFlags;

  DOCUMENT(R"(The length in bytes of this chunk - may be longer than the actual sum of the data if a
conservative size estimate was used on creation to avoid seeking to fix-up the stored length.
)");
  uint64_t length = 0;

  DOCUMENT("The ID of the thread where this chunk was recorded.");
  uint64_t threadID = 0;

  DOCUMENT(R"(The duration in microseconds that this chunk took. This is the time for the actual
work, not the serialising.
Since 0 is a possible value for this (for extremely fast calls), -1 is the invalid/not present value.
)");
  int64_t durationMicro = -1;

  DOCUMENT("The point in time when this chunk was recorded, in microseconds since program start.");
  uint64_t timestampMicro = 0;

  DOCUMENT("The frames of the CPU-side callstack leading up to the chunk.");
  rdcarray<uint64_t> callstack;

private:
  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete(void *p) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(SDChunkMetaData);

DOCUMENT(R"(The plain-old-data contents of an :class:`SDObject`.

Only one member is valid, as defined by the type of the :class:`SDObject`.
)");
union SDObjectPODData
{
  DOCUMENT("The value as an unsigned integer.");
  uint64_t u;

  DOCUMENT("The value as a signed integer.");
  int64_t i;

  DOCUMENT("The value as a floating point number.");
  double d;

  DOCUMENT("The value as a boolean.");
  bool b;

  DOCUMENT("The value as a single byte character.");
  char c;

  DOCUMENT("The value as a :class:`ResourceId`.");
  ResourceId id;

  SDObjectPODData() : u(0) {}
private:
  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete(void *p) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(SDObjectPODData);

DOCUMENT("INTERNAL: An array of SDObject*, mapped to a pure list in python");
struct StructuredObjectList : public rdcarray<SDObject *>
{
  StructuredObjectList() : rdcarray<SDObject *>() {}
  StructuredObjectList(const StructuredObjectList &other) = delete;

// SWIG needs the assignment operator to treat member variables as assignable.
// The SWIG wrappers handle lifetime for python-owned objects both on the old data being overwritten
// and the new incoming data.
// On the C++ side we don't want to accidentally copy or assign we only want to do it explicitly
// with Duplicate().
#if defined(SWIG)
  StructuredObjectList &operator=(const StructuredObjectList &other)
  {
    assign(other.data(), other.size());
    return *this;
  }
#else
  StructuredObjectList &operator=(const StructuredObjectList &other) = delete;
#endif
  // allow placement new for swig
  void *operator new(size_t, void *ptr) { return ptr; }
  void operator delete(void *p, void *) {}
  void operator delete(void *p) {}
private:
  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(StructuredObjectList);

// due to some objects potentially being lazily generated, we use the ugly 'mutable' keyword here
// to avoid completely losing const on these objects but allowing us to actually modify objects
// behind the scenes inside const objects. This is only used for effectively caching the lazy
// generated results, so to the outside world the object is still const.

DOCUMENT("The data inside an :class:`SDObject` whether it's plain old data or complex children.");
struct SDObjectData
{
  DOCUMENT("");
  SDObjectData() = default;

  DOCUMENT("The plain-old data contents of the object, in a :class:`SDObjectPODData`.");
  SDObjectPODData basic;

  DOCUMENT("The string contents of the object.");
  rdcinflexiblestr str;

  SDObjectData(const SDObjectData &) = delete;
  SDObjectData &operator=(const SDObjectData &other) = delete;

private:
  friend struct SDObject;
  friend struct SDChunk;

  // allow serialisation functions access to the data
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDObjectData &el);
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDObject *el);
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDObject &el);
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDChunk &el);

  DOCUMENT("A list of :class:`SDObject` containing the children of this :class:`SDObject`.");
  mutable StructuredObjectList children;

  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete(void *p) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(SDObjectData);

#if !defined(SWIG)
using LazyGenerator = std::function<SDObject *(const void *)>;

struct LazyArrayData
{
  byte *data;
  size_t elemSize;
  LazyGenerator generator;
};
#endif

DOCUMENT(R"(Defines a single structured object. Structured objects are defined recursively and one
object can either be a basic type (integer, float, etc), an array, or a struct. Arrays and structs
are defined similarly.

Each object owns its children and they will be deleted when it is deleted. You can use
:meth:`Duplicate` to make a deep copy of an object.
)");
struct SDObject
{
#if !defined(SWIG)
  template <typename MaybeConstSDObject>
  struct SDObjectIt
  {
  private:
    MaybeConstSDObject *o;
    size_t i;

  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = MaybeConstSDObject *;
    using difference_type = intptr_t;
    using pointer = value_type *;
    using reference = value_type &;

    SDObjectIt(MaybeConstSDObject *obj, size_t index) : o(obj), i(index) {}
    SDObjectIt(const SDObjectIt &rhs) : o(rhs.o), i(rhs.i) {}
    SDObjectIt &operator++()
    {
      ++i;
      return *this;
    }
    SDObjectIt operator++(int)
    {
      SDObjectIt tmp(*this);
      operator++();
      return tmp;
    }
    SDObjectIt &operator--()
    {
      --i;
      return *this;
    }
    SDObjectIt operator--(int)
    {
      SDObjectIt tmp(*this);
      operator--();
      return tmp;
    }
    size_t operator-(const SDObjectIt &rhs) { return i - rhs.i; }
    SDObjectIt operator+(int shift)
    {
      SDObjectIt ret(*this);
      ret.i += shift;
      return ret;
    }
    bool operator==(const SDObjectIt &rhs) { return o == rhs.o && i == rhs.i; }
    bool operator!=(const SDObjectIt &rhs) { return !(*this == rhs); }
    SDObjectIt &operator=(const SDObjectIt &rhs)
    {
      o = rhs.o;
      i = rhs.i;
      return *this;
    }

    inline MaybeConstSDObject *operator*() const { return o->GetChild(i); }
    inline MaybeConstSDObject &operator->() const { return *o->GetChild(i); }
  };
#endif

  /////////////////////////////////////////////////////////////////
  // memory management, in a dll safe way
  void *operator new(size_t sz) { return SDObject::alloc(sz); }
  void operator delete(void *p) { SDObject::dealloc(p); }
  void *operator new[](size_t count) = delete;
  void operator delete[](void *p) = delete;

  SDObject(const rdcinflexiblestr &n, const rdcinflexiblestr &t) : name(n), type(t)
  {
    data.basic.u = 0;
    m_Parent = NULL;
    m_Lazy = NULL;
  }
#if !defined(SWIG)
  SDObject(rdcinflexiblestr &&n, rdcinflexiblestr &&t) : name(std::move(n)), type(std::move(t))
  {
    data.basic.u = 0;
    m_Parent = NULL;
    m_Lazy = NULL;
  }
#endif

  ~SDObject()
  {
    // we own our children, so delete them now.
    DeleteChildren();

    // delete the lazy array data if we used it (rare)
    DeleteLazyGenerator();

    m_Parent = NULL;
  }

  DOCUMENT(R"(
:return: A new deep copy of this object, which the caller owns.
:rtype: SDObject
)");
  SDObject *Duplicate() const
  {
    SDObject *ret = new SDObject();
    ret->name = name;
    ret->type = type;
    ret->data.basic = data.basic;
    ret->data.str = data.str;

    if(m_Lazy)
    {
      PopulateAllChildren();
    }

    ret->data.children.resize(data.children.size());
    for(size_t i = 0; i < data.children.size(); i++)
      ret->data.children[i] = data.children[i]->Duplicate();

    return ret;
  }

  DOCUMENT("The name of this object.");
  rdcinflexiblestr name;

  DOCUMENT("The :class:`SDType` of this object.");
  SDType type;

  DOCUMENT("The :class:`SDObjectData` with the contents of this object.");
  SDObjectData data;

  DOCUMENT(R"(Checks if the given object has the same value as this one. This equality is defined
recursively through children.

:param SDObject obj: The object to compare against
:return: A boolean indicating if the object is equal to this one.
:rtype: bool
)");
  bool HasEqualValue(const SDObject *obj) const
  {
    bool ret = true;

    if(data.str != obj->data.str)
    {
      ret = false;
    }
    else if(data.basic.u != obj->data.basic.u)
    {
      ret = false;
    }
    else if(data.children.size() != obj->data.children.size())
    {
      ret = false;
    }
    else
    {
      for(size_t c = 0; c < obj->data.children.size(); c++)
      {
        PopulateChild(c);
        ret &= data.children[c]->HasEqualValue(obj->GetChild(c));
      }
    }

    return ret;
  }

  // this is renamed to just AddChild in the python interface file, since we always duplicate for
  // python.
  DOCUMENT(R"(Add a new child object.

:param SDObject child: The new child to add
)");
  inline void DuplicateAndAddChild(const SDObject *child)
  {
    // if we're adding to a lazy-generated array we can't have a mixture between lazy generation and
    // fully owned children. This shouldn't happen, but just in case we'll evaluate the lazy array
    // here.
    PopulateAllChildren();
    data.children.push_back(child->Duplicate());
    data.children.back()->m_Parent = this;
  }
  DOCUMENT(R"(Find a child object by a given name. If no matching child is found, ``None`` is
returned.

:param str childName: The name to search for.
:return: A reference to the child object if found, or ``None`` if not.
:rtype: SDObject
)");
  inline SDObject *FindChild(const rdcstr &childName)
  {
    for(size_t i = 0; i < data.children.size(); i++)
      if(GetChild(i)->name == childName)
        return GetChild(i);
    return NULL;
  }

  DOCUMENT(R"(Find a child object by a given name recursively. If no matching child is found,
``None`` is returned.

The order of the search is not guaranteed, so care should be taken when the name may not be unique.

:param str childName: The name to search for.
:return: A reference to the child object if found, or ``None`` if not.
:rtype: SDObject
)");
  inline SDObject *FindChildRecursively(const rdcstr &childName)
  {
    SDObject *o = FindChild(childName);
    if(o)
      return o;

    for(size_t i = 0; i < NumChildren(); i++)
    {
      o = GetChild(i)->FindChildRecursively(childName);
      if(o)
        return o;
    }

    return NULL;
  }

  DOCUMENT(R"(Find a child object by a given index. If the index is out of bounds, ``None`` is
returned.

:param int index: The index to look up.
:return: A reference to the child object if valid, or ``None`` if not.
:rtype: SDObject
)");
  inline SDObject *GetChild(size_t index)
  {
    if(index < data.children.size())
    {
      PopulateChild(index);
      return data.children[index];
    }

    return NULL;
  }

  DOCUMENT(R"(Get the parent of this object. If this object has no parent, ``None`` is returned.

:return: A reference to the parent object if valid, or ``None`` if not.
:rtype: SDObject
)");
  inline SDObject *GetParent() { return m_Parent; }
#if !defined(SWIG)
  inline const SDObject *GetParent() const { return m_Parent; }
  // const versions of FindChild/GetChild
  inline const SDObject *FindChild(const rdcstr &childName) const
  {
    for(size_t i = 0; i < data.children.size(); i++)
      if(GetChild(i)->name == childName)
        return GetChild(i);
    return NULL;
  }
  inline const SDObject *FindChildRecursively(const rdcstr &childName) const
  {
    const SDObject *o = FindChild(childName);
    if(o)
      return o;

    for(size_t i = 0; i < NumChildren(); i++)
    {
      o = GetChild(i)->FindChildRecursively(childName);
      if(o)
        return o;
    }

    return NULL;
  }
  inline const SDObject *GetChild(size_t index) const
  {
    if(index < data.children.size())
    {
      PopulateChild(index);
      return data.children[index];
    }

    return NULL;
  }
#endif

  DOCUMENT(R"(Delete the child object at an index. If the index is out of bounds, nothing happens.

:param int index: The index to remove.
)");
  inline void RemoveChild(size_t index)
  {
    if(index < data.children.size())
    {
      // we really shouldn't be deleting individually from a lazy array but just in case we are,
      // fully evaluate it first.
      PopulateAllChildren();
      delete data.children.takeAt(index);
    }
  }

  DOCUMENT("Delete all child objects.");
  inline void DeleteChildren()
  {
    for(size_t i = 0; i < data.children.size(); i++)
      delete data.children[i];

    data.children.clear();

    DeleteLazyGenerator();
  }

  DOCUMENT(R"(Get the number of child objects.

:return: The number of children this object contains.
:rtype: int
)");
  inline size_t NumChildren() const { return data.children.size(); }
#if !defined(SWIG)
  // these are for C++ iteration so not defined when SWIG is generating interfaces
  inline SDObjectIt<const SDObject> begin() const { return SDObjectIt<const SDObject>(this, 0); }
  inline SDObjectIt<const SDObject> end() const
  {
    return SDObjectIt<const SDObject>(this, data.children.size());
  }
  inline SDObjectIt<SDObject> begin() { return SDObjectIt<SDObject>(this, 0); }
  inline SDObjectIt<SDObject> end() { return SDObjectIt<SDObject>(this, data.children.size()); }
#endif

#if !defined(SWIG)
  // this interface is 'more advanced' and is intended for C++ code manipulating structured data.
  // reserve a number of children up front, useful when constructing an array to avoid repeated
  // allocations.
  void ReserveChildren(size_t num) { data.children.reserve(num); }
  // add a new child without duplicating it, and take ownership of it. Returns the child back
  // immediately for easy chaining.
  SDObject *AddAndOwnChild(SDObject *child)
  {
    PopulateAllChildren();
    child->m_Parent = this;
    data.children.push_back(child);
    return child;
  }
  // similar to AddAndOwnChild, but insert at a given offset
  SDObject *InsertAndOwnChild(size_t offs, SDObject *child)
  {
    PopulateAllChildren();
    child->m_Parent = this;
    data.children.insert(offs, child);
    return child;
  }
  // Take ownership of the whole children array from the object.
  void TakeAllChildren(StructuredObjectList &objs)
  {
    PopulateAllChildren();
    for(size_t i = 0; i < data.children.size(); i++)
      data.children[i]->m_Parent = NULL;
    objs.clear();
    objs.swap(data.children);
  }

  template <typename T>
  void SetLazyArray(uint64_t arrayCount, T *arrayData, LazyGenerator generator)
  {
    DeleteChildren();

    void *lazyAlloc = alloc(sizeof(LazyArrayData));

    m_Lazy = new(lazyAlloc) LazyArrayData;
    m_Lazy->generator = generator;
    m_Lazy->elemSize = sizeof(T);
    size_t sz = size_t(sizeof(T) * arrayCount);
    m_Lazy->data = (byte *)alloc(sz);
    memcpy(m_Lazy->data, arrayData, sz);
    data.children.resize((size_t)arrayCount);
  }
#endif

// C++ gets more extensive typecasts. We'll add a couple for python in the interface file
#if !defined(SWIG)
  // templated enum cast
  template <typename EnumType>
  EnumType AsEnum() const
  {
    return (EnumType)data.basic.u;
  }
  inline double AsDouble() const { return data.basic.d; }
  inline float AsFloat() const { return (float)data.basic.d; }
  inline char AsChar() const { return data.basic.c; }
  inline const rdcinflexiblestr &AsString() const { return data.str; }
  inline uint64_t AsUInt64() const { return (uint64_t)data.basic.u; }
  inline int64_t AsInt64() const { return (int64_t)data.basic.i; }
  inline uint32_t AsUInt32() const { return (uint32_t)data.basic.u; }
  inline int32_t AsInt32() const { return (int32_t)data.basic.i; }
  inline uint16_t AsUInt16() const { return (uint16_t)data.basic.u; }
  inline int16_t AsInt16() const { return (int16_t)data.basic.i; }
  inline uint8_t AsUInt8() const { return (uint8_t)data.basic.u; }
  inline int8_t AsInt8() const { return (int8_t)data.basic.i; }
  inline double &Double() { return data.basic.d; }
  inline uint64_t &UInt64() { return data.basic.u; }
  inline int64_t &Int64() { return data.basic.i; }
  inline bool IsStruct() const { return type.basetype == SDBasic::Struct; }
  inline bool IsNULL() const
  {
    return type.basetype == SDBasic::Null || (IsArray() && NumChildren() == 0) ||
           (IsString() && (type.flags & SDTypeFlags::NullString));
  }
  inline bool IsUInt() const { return type.basetype == SDBasic::UnsignedInteger; }
  inline bool IsInt() const { return type.basetype == SDBasic::SignedInteger; }
  inline bool IsFloat() const { return type.basetype == SDBasic::Float; }
  inline bool IsString() const { return type.basetype == SDBasic::String; }
  inline bool IsArray() const { return type.basetype == SDBasic::Array; }
  inline bool IsFixedArray(uint64_t size = 0) const
  {
    return IsArray() && (type.flags & SDTypeFlags::FixedArray) &&
           (size > 0 ? NumChildren() <= size : true);
  }
  inline bool IsVariableArray() const
  {
    return IsArray() && ((type.flags & SDTypeFlags::FixedArray) == SDTypeFlags::NoFlags);
  }
  inline bool IsEnum() const { return type.basetype == SDBasic::Enum; }
  inline bool IsBuffer() const { return type.basetype == SDBasic::Buffer; }
  inline bool IsPointer() const
  {
    return (type.flags & SDTypeFlags::Nullable) && (NumChildren() != 0);
  }
  inline bool IsResource() const { return type.basetype == SDBasic::Resource; }
  inline bool IsUnion() const
  {
    return (type.basetype == SDBasic::Struct) && (type.flags & SDTypeFlags::Union);
  }
  inline bool IsSimpleType() const
  {
    return IsNULL() || (!IsStruct() && !IsArray() && !IsPointer() && !IsUnion());
  }

  // Is it possible to fully inline the data structure declaration?
  inline bool IsInlineable() const
  {
    // if it has elements that are not inlineable, return false.
    for(size_t i = 0; i < NumChildren(); i++)
      if(!GetChild(i)->IsInlineable())
        return false;
    if((IsPointer() || IsVariableArray()) && !IsNULL())
      return false;
    if(IsUnion())
      return false;
    return true;
  }
  SDObject *SetTypeName(const rdcstr &customTypeName)
  {
    type.name = customTypeName;
    return this;
  }
  SDObject *SetCustomString(const rdcstr &customString)
  {
    data.str = customString;
    type.flags = SDTypeFlags::HasCustomString;
    return this;
  }
#endif

  // these are common to both python and C++
  DOCUMENT(R"(Interprets the object as a ``bool`` and returns its value.
Invalid if the object is not actually a ``bool``.

:return: The interpreted bool value.
:rtype: bool
)");
  inline bool AsBool() const { return data.basic.b; }
  // these are common to both python and C++
  DOCUMENT(R"(Interprets the object as a :class:`ResourceId` and returns its value.
Invalid if the object is not actually a :class:`ResourceId`.

:return: The interpreted ID.
:rtype: ResourceId
)");
  inline ResourceId AsResourceId() const { return data.basic.id; }
#if defined(RENDERDOC_QT_COMPAT)
  operator QVariant() const
  {
    switch(type.basetype)
    {
      case SDBasic::Chunk:
      case SDBasic::Struct:
      {
        QVariantMap ret;
        for(size_t i = 0; i < data.children.size(); i++)
          ret[data.children[i]->name] = *data.children[i];
        break;
      }
      case SDBasic::Array:
      {
        QVariantList ret;
        for(size_t i = 0; i < data.children.size(); i++)
          ret.push_back(*data.children[i]);
      }
      case SDBasic::Null:
      case SDBasic::Buffer: return QVariant();
      case SDBasic::String: return data.str;
      case SDBasic::Enum:
      case SDBasic::UnsignedInteger: return QVariant(qulonglong(data.basic.u));
      case SDBasic::SignedInteger: return QVariant(qlonglong(data.basic.i));
      case SDBasic::Resource: return (QVariant)data.basic.id;
      case SDBasic::Float: return data.basic.d;
      case SDBasic::Boolean: return data.basic.b;
      case SDBasic::Character: return data.basic.c;
    }

    return QVariant();
  }
#endif

protected:
  SDObject() {}
  SDObject(const SDObject &other) = delete;
  SDObject &operator=(const SDObject &other) = delete;

  // these functions can be const because we have 'mutable' allowing us to modify these members.
  // It's ugly, but necessary
  inline void PopulateChild(size_t idx) const
  {
    if(m_Lazy)
    {
      if(data.children[idx] == NULL)
      {
        data.children[idx] = m_Lazy->generator(m_Lazy->data + idx * m_Lazy->elemSize);
        data.children[idx]->m_Parent = (SDObject *)this;
      }
    }
  }

  void PopulateAllChildren() const
  {
    if(m_Lazy)
    {
      for(size_t i = 0; i < data.children.size(); i++)
        PopulateChild(i);

      DeleteLazyGenerator();
    }
  }

  static void *alloc(size_t sz)
  {
    void *ret = NULL;
#ifdef RENDERDOC_EXPORTS
    ret = malloc(sz);
    if(ret == NULL)
      RENDERDOC_OutOfMemory(sz);
#else
    ret = RENDERDOC_AllocArrayMem(sz);
#endif
    return ret;
  }
  static void dealloc(void *p)
  {
#ifdef RENDERDOC_EXPORTS
    free(p);
#else
    RENDERDOC_FreeArrayMem(p);
#endif
  }

private:
  SDObject *m_Parent = NULL;
  mutable LazyArrayData *m_Lazy = NULL;

  // object serialisers need to be able to set the parent pointer. This is only for proxying really
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDObject &el);
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDChunk &el);
  template <class SerialiserType>
  friend void DoSerialise(SerialiserType &ser, SDObject &el, StructuredObjectList &children);

  void DeleteLazyGenerator() const
  {
    if(m_Lazy)
    {
      dealloc(m_Lazy->data);
      dealloc(m_Lazy);
      m_Lazy = NULL;
    }
  }
};

DECLARE_REFLECTION_STRUCT(SDObject);

#if defined(RENDERDOC_QT_COMPAT)
inline SDObject *makeSDObject(const rdcinflexiblestr &name, QVariant val)
{
  SDObject *ret = new SDObject(name, "QVariant"_lit);
  ret->type.basetype = SDBasic::Null;

  // coverity[mixed_enums]
  QMetaType::Type type = (QMetaType::Type)val.type();

  switch(type)
  {
    case QMetaType::Bool:
      ret->type.name = "bool"_lit;
      ret->type.basetype = SDBasic::Boolean;
      ret->type.byteSize = 1;
      ret->data.basic.b = val.toBool();
      break;
    case QMetaType::Short:
      ret->type.name = "int16_t"_lit;
      ret->type.basetype = SDBasic::SignedInteger;
      ret->type.byteSize = 2;
      ret->data.basic.i = val.toInt();
      break;
    case QMetaType::UShort:
      ret->type.name = "uint16_t"_lit;
      ret->type.basetype = SDBasic::UnsignedInteger;
      ret->type.byteSize = 2;
      ret->data.basic.u = val.toUInt();
      break;
    case QMetaType::Long:
    case QMetaType::Int:
      ret->type.name = "int32_t"_lit;
      ret->type.basetype = SDBasic::SignedInteger;
      ret->type.byteSize = 4;
      ret->data.basic.i = val.toInt();
      break;
    case QMetaType::ULong:
    case QMetaType::UInt:
      ret->type.name = "uint32_t"_lit;
      ret->type.basetype = SDBasic::UnsignedInteger;
      ret->type.byteSize = 4;
      ret->data.basic.u = val.toUInt();
      break;
    case QMetaType::LongLong:
      ret->type.name = "int64_t"_lit;
      ret->type.basetype = SDBasic::SignedInteger;
      ret->type.byteSize = 8;
      ret->data.basic.i = val.toLongLong();
      break;
    case QMetaType::ULongLong:
      ret->type.name = "uint64_t"_lit;
      ret->type.basetype = SDBasic::UnsignedInteger;
      ret->type.byteSize = 8;
      ret->data.basic.u = val.toULongLong();
      break;
    case QMetaType::Float:
      ret->type.name = "float"_lit;
      ret->type.basetype = SDBasic::Float;
      ret->type.byteSize = 4;
      ret->data.basic.d = val.toFloat();
      break;
    case QMetaType::Double:
      ret->type.name = "double"_lit;
      ret->type.basetype = SDBasic::Float;
      ret->type.byteSize = 8;
      ret->data.basic.d = val.toDouble();
      break;
    case QMetaType::UChar:
    case QMetaType::Char:
    case QMetaType::QChar:
      ret->type.name = "char"_lit;
      ret->type.basetype = SDBasic::Character;
      ret->type.byteSize = 1;
      ret->data.basic.c = val.toChar().toLatin1();
      break;
    case QMetaType::QString:
      ret->type.name = "string"_lit;
      ret->type.basetype = SDBasic::String;
      ret->data.str = val.toString().toUtf8().data();
      ret->type.byteSize = ret->data.str.size();
      break;
    case QMetaType::QVariantList:
    {
      QVariantList list = val.toList();
      ret->type.name = "array"_lit;
      ret->type.basetype = SDBasic::Array;
      ret->ReserveChildren(list.size());
      for(int i = 0; i < list.size(); i++)
        ret->AddAndOwnChild(makeSDObject("[]"_lit, list.at(i)));
      ret->type.byteSize = list.size();
      break;
    }
    case QMetaType::QVariantMap:
    {
      QVariantMap map = val.toMap();
      ret->type.name = "struct"_lit;
      ret->type.basetype = SDBasic::Struct;
      ret->ReserveChildren(map.size());
      for(const QString &str : map.keys())
        ret->AddAndOwnChild(makeSDObject(rdcstr(str.toUtf8().data()), map[str]));
      ret->type.byteSize = map.size();
      break;
    }
    default: break;
  }

  return ret;
}
#endif

DOCUMENT(R"(Make a structured object as a signed 64-bit integer.

.. note::
  You should ensure that the value you pass in has already been truncated to the appropriate range
  for the storage, as the resulting object will be undefined if the value is out of the valid range.

:param str name: The name of the object.
:param int val: The integer which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDInt64(const rdcinflexiblestr &name, int64_t val)
{
  SDObject *ret = new SDObject(name, "int64_t"_lit);
  ret->type.basetype = SDBasic::SignedInteger;
  ret->type.byteSize = 8;
  ret->data.basic.i = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as an unsigned 64-bit integer.

.. note::
  You should ensure that the value you pass in has already been truncated to the appropriate range
  for the storage, as the resulting object will be undefined if the value is out of the valid range.

:param str name: The name of the object.
:param int val: The integer which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDUInt64(const rdcinflexiblestr &name, uint64_t val)
{
  SDObject *ret = new SDObject(name, "uint64_t"_lit);
  ret->type.basetype = SDBasic::UnsignedInteger;
  ret->type.byteSize = 8;
  ret->data.basic.u = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as a signed 32-bit integer.

.. note::
  You should ensure that the value you pass in has already been truncated to the appropriate range
  for the storage, as the resulting object will be undefined if the value is out of the valid range.

:param str name: The name of the object.
:param int val: The integer which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDInt32(const rdcinflexiblestr &name, int32_t val)
{
  SDObject *ret = new SDObject(name, "int32_t"_lit);
  ret->type.basetype = SDBasic::SignedInteger;
  ret->type.byteSize = 4;
  ret->data.basic.i = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as an unsigned 32-bit integer.

.. note::
  You should ensure that the value you pass in has already been truncated to the appropriate range
  for the storage, as the resulting object will be undefined if the value is out of the valid range.

:param str name: The name of the object.
:param int val: The integer which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDUInt32(const rdcinflexiblestr &name, uint32_t val)
{
  SDObject *ret = new SDObject(name, "uint32_t"_lit);
  ret->type.basetype = SDBasic::UnsignedInteger;
  ret->type.byteSize = 4;
  ret->data.basic.u = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as a 32-bit float.

.. note::
  You should ensure that the value you pass in has already been truncated to the appropriate range
  for the storage, as the resulting object will be undefined if the value is out of the valid range.

:param str name: The name of the object.
:param float val: The float which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDFloat(const rdcinflexiblestr &name, float val)
{
  SDObject *ret = new SDObject(name, "float"_lit);
  ret->type.basetype = SDBasic::Float;
  ret->type.byteSize = 4;
  ret->data.basic.d = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as a boolean value.

:param str name: The name of the object.
:param bool val: The bool which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDBool(const rdcinflexiblestr &name, bool val)
{
  SDObject *ret = new SDObject(name, "bool"_lit);
  ret->type.basetype = SDBasic::Boolean;
  ret->type.byteSize = 1;
  ret->data.basic.b = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as a string value.

:param str name: The name of the object.
:param str val: The string which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDString(const rdcinflexiblestr &name, const rdcstr &val)
{
  SDObject *ret = new SDObject(name, "string"_lit);
  ret->type.basetype = SDBasic::String;
  ret->type.byteSize = val.size();
  ret->data.str = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as a ResourceId value.

:param str name: The name of the object.
:param ResourceId val: The ID which will be stored in the returned object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDResourceId(const rdcinflexiblestr &name, ResourceId val)
{
  SDObject *ret = new SDObject(name, "ResourceId"_lit);
  ret->type.basetype = SDBasic::Resource;
  ret->type.byteSize = 8;
  ret->data.basic.id = val;
  return ret;
}

DOCUMENT(R"(Make a structured object as an enum value.

.. note::
  The enum will be stored just as an integer value, but the string name of the enumeration value can
  be set with :meth:`SDObject.SetCustomString` if desired.

:param str name: The name of the object.
:param int val: The integer value of the enum itself.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDEnum(const rdcinflexiblestr &name, uint32_t val)
{
  SDObject *ret = new SDObject(name, "enum"_lit);
  ret->type.basetype = SDBasic::Enum;
  ret->type.byteSize = 4;
  ret->data.basic.u = val;
  return ret;
}

DOCUMENT(R"(Make a structured object which is an array.

The array will be created empty, and new members can be added using methods on :class:`SDObject`.

:param str name: The name of the object.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDArray(const rdcinflexiblestr &name)
{
  SDObject *ret = new SDObject(name, "array"_lit);
  ret->type.basetype = SDBasic::Array;
  return ret;
}

DOCUMENT(R"(Make a structured object which is a struct.

The struct will be created empty, and new members can be added using methods on :class:`SDObject`.

:param str name: The name of the object.
:param str structtype: The typename of the struct.
:return: The new object, owner by the caller.
:rtype: SDObject
)");
inline SDObject *makeSDStruct(const rdcinflexiblestr &name, const rdcinflexiblestr &structtype)
{
  SDObject *ret = new SDObject(name, structtype);
  ret->type.basetype = SDBasic::Struct;
  return ret;
}

// an overloaded function calling into the named equivalents above, since python doesn't have the
// concept of different width types like 32-bit vs 64-bit ints
#if !defined(SWIG)

#define SDOBJECT_MAKER(basetype, makeSDFunc)                                                        \
  inline SDObject *makeSDObject(const rdcinflexiblestr &name, basetype value,                       \
                                const char *customString = NULL, const char *customTypeName = NULL) \
  {                                                                                                 \
    SDObject *ptr = makeSDFunc(name, value);                                                        \
    if(customString)                                                                                \
      ptr->SetCustomString(rdcstr(customString));                                                   \
    if(customTypeName)                                                                              \
      ptr->SetTypeName(rdcstr(customTypeName));                                                     \
    return ptr;                                                                                     \
  }

SDOBJECT_MAKER(int64_t, makeSDInt64);
SDOBJECT_MAKER(uint64_t, makeSDUInt64);
SDOBJECT_MAKER(int32_t, makeSDInt32);
SDOBJECT_MAKER(uint32_t, makeSDUInt32);
SDOBJECT_MAKER(float, makeSDFloat);
SDOBJECT_MAKER(bool, makeSDBool);
SDOBJECT_MAKER(const char *, makeSDString);
SDOBJECT_MAKER(const rdcstr &, makeSDString);
SDOBJECT_MAKER(ResourceId, makeSDResourceId);

#undef SDOBJECT_MAKER

#endif

DOCUMENT("Defines a single structured chunk, which is a :class:`SDObject`.");
struct SDChunk : public SDObject
{
  /////////////////////////////////////////////////////////////////
  // memory management, in a dll safe way
  void *operator new(size_t sz)
  {
    void *ret = NULL;
#ifdef RENDERDOC_EXPORTS
    ret = malloc(sz);
    if(ret == NULL)
      RENDERDOC_OutOfMemory(sz);
#else
    ret = RENDERDOC_AllocArrayMem(sz);
#endif
    return ret;
  }
  void operator delete(void *p)
  {
#ifdef RENDERDOC_EXPORTS
    free(p);
#else
    RENDERDOC_FreeArrayMem(p);
#endif
  }
  void *operator new[](size_t count) = delete;
  void operator delete[](void *p) = delete;

  SDChunk(const rdcinflexiblestr &name) : SDObject(name, "Chunk"_lit)
  {
    type.basetype = SDBasic::Chunk;
  }
  DOCUMENT("The :class:`SDChunkMetaData` with the metadata for this chunk.");
  SDChunkMetaData metadata;

  DOCUMENT(R"(
:return: A new deep copy of this chunk, which the caller owns.
:rtype: SDChunk
)");
  SDChunk *Duplicate() const
  {
    SDChunk *ret = new SDChunk();
    ret->name = name;
    ret->metadata = metadata;
    ret->type = type;
    ret->data.basic = data.basic;
    ret->data.str = data.str;

    ret->data.children.resize(data.children.size());

    PopulateAllChildren();

    for(size_t i = 0; i < data.children.size(); i++)
      ret->data.children[i] = data.children[i]->Duplicate();

    return ret;
  }

protected:
  SDChunk() : SDObject() {}
  SDChunk(const SDChunk &other) = delete;
  SDChunk &operator=(const SDChunk &other) = delete;
};

DECLARE_REFLECTION_STRUCT(SDChunk);

DOCUMENT("INTERNAL: An array of SDChunk*, mapped to a pure list in python");
struct StructuredChunkList : public rdcarray<SDChunk *>
{
  StructuredChunkList() : rdcarray<SDChunk *>() {}
  StructuredChunkList(const StructuredChunkList &other) = delete;

  using rdcarray<SDChunk *>::swap;

// SWIG needs the assignment operator to treat member variables as assignable.
// The SWIG wrappers handle lifetime for python-owned objects both on the old data being overwritten
// and the new incoming data.
// On the C++ side we don't want to accidentally copy or assign we only want to do it explicitly
// with Duplicate().
#if defined(SWIG)
  StructuredChunkList &operator=(const StructuredChunkList &other)
  {
    assign(other.data(), other.size());
    return *this;
  }
#else
  StructuredChunkList &operator=(const StructuredChunkList &other) = delete;
#endif

  // allow placement new for swig
  void *operator new(size_t, void *ptr) { return ptr; }
  void operator delete(void *p, void *) {}
  void operator delete(void *p) {}
private:
  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(StructuredChunkList);

DECLARE_REFLECTION_STRUCT(bytebuf);

DOCUMENT("INTERNAL: An array of bytebuf*, mapped to a pure list of bytes in python");
struct StructuredBufferList : public rdcarray<bytebuf *>
{
  StructuredBufferList() : rdcarray<bytebuf *>() {}
  StructuredBufferList(const StructuredBufferList &other) = delete;

  using rdcarray<bytebuf *>::swap;

// SWIG needs the assignment operator to treat member variables as assignable.
// The SWIG wrappers handle lifetime for python-owned objects both on the old data being overwritten
// and the new incoming data.
// On the C++ side we don't want to accidentally copy or assign we only want to do it explicitly
// with Duplicate().
#if defined(SWIG)
  StructuredBufferList &operator=(const StructuredBufferList &other)
  {
    assign(other.data(), other.size());
    return *this;
  }
#else
  StructuredBufferList &operator=(const StructuredBufferList &other) = delete;
#endif

  // allow placement new for swig
  void *operator new(size_t, void *ptr) { return ptr; }
  void operator delete(void *p, void *) {}
  void operator delete(void *p) {}
private:
  void *operator new(size_t count) = delete;
  void *operator new[](size_t count) = delete;
  void operator delete[](void *p) = delete;
};

DECLARE_REFLECTION_STRUCT(StructuredBufferList);

DOCUMENT("Contains the structured information in a file. Owns the buffers and chunks.");
struct SDFile
{
private:
  /////////////////////////////////////////////////////////////////
  // memory management, in a dll safe way
  static void *allocate(size_t count)
  {
    const size_t sz = count * sizeof(SDFile);
    void *ret = NULL;
#ifdef RENDERDOC_EXPORTS
    ret = malloc(sz);
    if(ret == NULL)
      RENDERDOC_OutOfMemory(sz);
#else
    ret = RENDERDOC_AllocArrayMem(sz);
#endif
    return ret;
  }
  static void deallocate(void *p)
  {
#ifdef RENDERDOC_EXPORTS
    free(p);
#else
    RENDERDOC_FreeArrayMem(p);
#endif
  }

  void *operator new[](size_t count) = delete;
  void operator delete[](void *p) = delete;

public:
  void *operator new(size_t count) { return allocate(count); }
  void operator delete(void *p) { return deallocate(p); };
  SDFile() {}
  ~SDFile()
  {
    for(SDChunk *chunk : chunks)
      delete chunk;

    for(bytebuf *buf : buffers)
      delete buf;
  }

  DOCUMENT(R"(The chunks in the file in order.

:type: List[SDChunk]
)");
  StructuredChunkList chunks;

  DOCUMENT(R"(The buffers in the file, as referenced by the chunks in :data:`chunks`.

:type: List[bytes]
)");
  StructuredBufferList buffers;

  DOCUMENT("The version of this structured stream, typically only used internally.");
  uint64_t version = 0;

  DOCUMENT(R"(Swaps the contents of this file with another.

:param SDFile other: The other file to swap with.
)");
  inline void Swap(SDFile &other)
  {
    chunks.swap(other.chunks);
    buffers.swap(other.buffers);
    std::swap(version, other.version);
  }

protected:
  SDFile(const SDFile &) = delete;
  SDFile &operator=(const SDFile &) = delete;
};
