/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

.. data:: ResourceId

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
  ResourceId,
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
)");
enum class SDTypeFlags : uint32_t
{
  NoFlags = 0x0,
  HasCustomString = 0x1,
  Hidden = 0x2,
  Nullable = 0x4,
  NullString = 0x8,
  FixedArray = 0x10,
};

BITMASK_OPERATORS(SDTypeFlags);
DECLARE_REFLECTION_ENUM(SDTypeFlags);

struct SDObject;
struct SDChunk;

DOCUMENT("Details the name and properties of a structured type");
struct SDType
{
  SDType(const char *n)
      : name(n), basetype(SDBasic::Struct), flags(SDTypeFlags::NoFlags), byteSize(0)
  {
  }

  DOCUMENT("The name of this type.");
  rdcstr name;

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
};

DECLARE_REFLECTION_STRUCT(SDType);

DOCUMENT(R"(Bitfield flags that could be applied to an :class:`SDChunk`.

.. data:: NoFlags

  This chunk has no special properties.

.. data:: OpaqueChunk

  This chunk wasn't supported for decoding or was skipped for another reason and was detailed as
  an opaque byte stream. It should be preserved as-is and will remain in native RDC format.
)");
enum class SDChunkFlags : uint64_t
{
  NoFlags = 0x0,
  OpaqueChunk = 0x1,
};

BITMASK_OPERATORS(SDChunkFlags);
DECLARE_REFLECTION_ENUM(SDChunkFlags);

DOCUMENT("The metadata that goes along with a :class:`SDChunk` to detail how it was recorded.");
struct SDChunkMetaData
{
  DOCUMENT("The internal chunk ID - unique given a particular driver in use.");
  uint32_t chunkID = 0;

  DOCUMENT("The :class:`SDChunkFlags` for this chunk.");
  SDChunkFlags flags = SDChunkFlags::NoFlags;

  DOCUMENT(R"(The length in bytes of this chunk - may be longer than the actual sum of the data if a
conservative size estimate was used on creation to avoid seeking to fix-up the stored length.
)");
  uint32_t length = 0;

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

  // mostly here just for debugging
  DOCUMENT("A useful alias of :data:`u` - the number of children when a struct/array.");
  uint64_t numChildren;

  SDObjectPODData() : u(0) {}
};

DECLARE_REFLECTION_STRUCT(SDObjectPODData);

DOCUMENT("A ``list`` of :class:`SDObject` objects");
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
};

DECLARE_REFLECTION_STRUCT(StructuredObjectList);

DOCUMENT("The data inside an class:`SDObject`, whether it's plain old data or complex children.");
struct SDObjectData
{
  DOCUMENT("The plain-old data contents of the object, in a :class:`SDObjectPODData`.");
  SDObjectPODData basic;

  DOCUMENT("The string contents of the object.");
  rdcstr str;

  DOCUMENT("A ``list`` of class:`SDObject` containing the children of this class:`SDObject`.");
  StructuredObjectList children;

  SDObjectData &operator=(const SDObjectData &other) = delete;
};

DECLARE_REFLECTION_STRUCT(SDObjectData);

DOCUMENT("Defines a single structured object.");
struct SDObject
{
  SDObject(const char *n, const char *t) : type(t)
  {
    name = n;
    data.basic.u = 0;
  }

  ~SDObject()
  {
    for(size_t i = 0; i < data.children.size(); i++)
      delete data.children[i];

    data.children.clear();
  }

  DOCUMENT("Create a deep copy of this object.");
  SDObject *Duplicate()
  {
    SDObject *ret = new SDObject();
    ret->name = name;
    ret->type = type;
    ret->data.basic = data.basic;
    ret->data.str = data.str;

    ret->data.children.resize(data.children.size());
    for(size_t i = 0; i < data.children.size(); i++)
      ret->data.children[i] = data.children[i]->Duplicate();

    return ret;
  }

  DOCUMENT("The name of this object.");
  rdcstr name;

  DOCUMENT("The :class:`SDType` of this object.");
  SDType type;

  DOCUMENT("The :class:`SDObjectData` with the contents of this object.");
  SDObjectData data;

  DOCUMENT("Find a child object by a given name.");
  inline SDObject *FindChild(const char *childName) const
  {
    for(size_t i = 0; i < data.children.size(); i++)
      if(data.children[i]->name == childName)
        return data.children[i];

    return NULL;
  }

  DOCUMENT("Add a new child object by duplicating it.");
  inline void AddChild(SDObject *child) { data.children.push_back(child->Duplicate()); }
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
      case SDBasic::ResourceId: return (QVariant)data.basic.id;
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
};

DECLARE_REFLECTION_STRUCT(SDObject);

#if defined(RENDERDOC_QT_COMPAT)
inline SDObject *makeSDObject(const char *name, QVariant val)
{
  SDObject *ret = new SDObject(name, "QVariant");
  ret->type.basetype = SDBasic::Null;

  // coverity[mixed_enums]
  QMetaType::Type type = (QMetaType::Type)val.type();

  switch(type)
  {
    case QMetaType::Bool:
      ret->type.name = "bool";
      ret->type.basetype = SDBasic::Boolean;
      ret->type.byteSize = 1;
      ret->data.basic.b = val.toBool();
      break;
    case QMetaType::Short:
      ret->type.name = "int16_t";
      ret->type.basetype = SDBasic::SignedInteger;
      ret->type.byteSize = 2;
      ret->data.basic.i = val.toInt();
      break;
    case QMetaType::UShort:
      ret->type.name = "uint16_t";
      ret->type.basetype = SDBasic::UnsignedInteger;
      ret->type.byteSize = 2;
      ret->data.basic.u = val.toUInt();
      break;
    case QMetaType::Long:
    case QMetaType::Int:
      ret->type.name = "int32_t";
      ret->type.basetype = SDBasic::SignedInteger;
      ret->type.byteSize = 4;
      ret->data.basic.i = val.toInt();
      break;
    case QMetaType::ULong:
    case QMetaType::UInt:
      ret->type.name = "uint32_t";
      ret->type.basetype = SDBasic::UnsignedInteger;
      ret->type.byteSize = 4;
      ret->data.basic.u = val.toUInt();
      break;
    case QMetaType::LongLong:
      ret->type.name = "int64_t";
      ret->type.basetype = SDBasic::SignedInteger;
      ret->type.byteSize = 8;
      ret->data.basic.i = val.toLongLong();
      break;
    case QMetaType::ULongLong:
      ret->type.name = "uint64_t";
      ret->type.basetype = SDBasic::UnsignedInteger;
      ret->type.byteSize = 8;
      ret->data.basic.u = val.toULongLong();
      break;
    case QMetaType::Float:
      ret->type.name = "float";
      ret->type.basetype = SDBasic::Float;
      ret->type.byteSize = 4;
      ret->data.basic.d = val.toFloat();
      break;
    case QMetaType::Double:
      ret->type.name = "double";
      ret->type.basetype = SDBasic::Float;
      ret->type.byteSize = 8;
      ret->data.basic.d = val.toDouble();
      break;
    case QMetaType::UChar:
    case QMetaType::Char:
    case QMetaType::QChar:
      ret->type.name = "char";
      ret->type.basetype = SDBasic::Character;
      ret->type.byteSize = 1;
      ret->data.basic.c = val.toChar().toLatin1();
      break;
    case QMetaType::QString:
      ret->type.name = "string";
      ret->type.basetype = SDBasic::String;
      ret->data.str = val.toString().toUtf8().data();
      ret->type.byteSize = ret->data.str.size();
      break;
    default: break;
  }

  return ret;
}
#endif

DOCUMENT("Make a structured object out of a signed integer");
inline SDObject *makeSDObject(const char *name, int64_t val)
{
  SDObject *ret = new SDObject(name, "int64_t");
  ret->type.basetype = SDBasic::SignedInteger;
  ret->type.byteSize = 8;
  ret->data.basic.i = val;
  return ret;
}

DOCUMENT("Make a structured object out of an unsigned integer");
inline SDObject *makeSDObject(const char *name, uint64_t val)
{
  SDObject *ret = new SDObject(name, "uint64_t");
  ret->type.basetype = SDBasic::UnsignedInteger;
  ret->type.byteSize = 8;
  ret->data.basic.u = val;
  return ret;
}

DOCUMENT("Make a structured object out of a floating point value");
inline SDObject *makeSDObject(const char *name, float val)
{
  SDObject *ret = new SDObject(name, "float");
  ret->type.basetype = SDBasic::Float;
  ret->type.byteSize = 4;
  ret->data.basic.d = val;
  return ret;
}

DOCUMENT("Make a structured object out of a string");
inline SDObject *makeSDObject(const char *name, const char *val)
{
  SDObject *ret = new SDObject(name, "string");
  ret->type.basetype = SDBasic::String;
  ret->type.byteSize = strlen(val);
  ret->data.str = val;
  return ret;
}

DOCUMENT("Make a structured object out of a ResourceId");
inline SDObject *makeSDObject(const char *name, ResourceId val)
{
  SDObject *ret = new SDObject(name, "ResourceId");
  ret->type.basetype = SDBasic::ResourceId;
  ret->type.byteSize = 8;
  ret->data.basic.id = val;
  return ret;
}

DOCUMENT("Make an array-type structured object out of a string");
inline SDObject *makeSDArray(const char *name)
{
  SDObject *ret = new SDObject(name, "array");
  ret->type.basetype = SDBasic::Array;
  return ret;
}

DOCUMENT("Make an array-type structured object out of a string");
inline SDObject *makeSDStruct(const char *name)
{
  SDObject *ret = new SDObject(name, "struct");
  ret->type.basetype = SDBasic::Struct;
  return ret;
}

// some extra overloads that we don't bother exposing since python doesn't have the concept of
// different width types like 32-bit vs 64-bit ints
#if !defined(SWIG)

inline SDObject *makeSDObject(const char *name, int32_t val)
{
  SDObject *ret = new SDObject(name, "int32_t");
  ret->type.basetype = SDBasic::SignedInteger;
  ret->type.byteSize = 4;
  ret->data.basic.u = val;
  return ret;
}

inline SDObject *makeSDObject(const char *name, uint32_t val)
{
  SDObject *ret = new SDObject(name, "uint32_t");
  ret->type.basetype = SDBasic::UnsignedInteger;
  ret->type.byteSize = 4;
  ret->data.basic.u = val;
  return ret;
}

#endif

DOCUMENT("Defines a single structured chunk, which is a :class:`SDObject`.");
struct SDChunk : public SDObject
{
  SDChunk(const char *name) : SDObject(name, "Chunk") { type.basetype = SDBasic::Chunk; }
  DOCUMENT("The :class:`SDChunkMetaData` with the metadata for this chunk.");
  SDChunkMetaData metadata;

  DOCUMENT("Create a deep copy of this chunk.");
  SDChunk *Duplicate()
  {
    SDChunk *ret = new SDChunk();
    ret->name = name;
    ret->metadata = metadata;
    ret->type = type;
    ret->data.basic = data.basic;
    ret->data.str = data.str;

    ret->data.children.resize(data.children.size());
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

DOCUMENT("A ``list`` of :class:`SDChunk` objects");
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
};

DECLARE_REFLECTION_STRUCT(StructuredChunkList);

DECLARE_REFLECTION_STRUCT(bytebuf);

DOCUMENT("A ``list`` of ``bytes`` objects");
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
};

DECLARE_REFLECTION_STRUCT(StructuredBufferList);

DOCUMENT("Contains the structured information in a file. Owns the buffers and chunks.");
struct SDFile
{
  SDFile() {}
  ~SDFile()
  {
    for(SDChunk *chunk : chunks)
      delete chunk;

    for(bytebuf *buf : buffers)
      delete buf;
  }

  DOCUMENT("A ``list`` of :class:`SDChunk` objects with the chunks in order.");
  StructuredChunkList chunks;

  DOCUMENT("A ``list`` of serialised buffers stored as ``bytes`` objects");
  StructuredBufferList buffers;

  DOCUMENT("The version of this structured stream, typically only used internally.");
  uint64_t version = 0;

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
