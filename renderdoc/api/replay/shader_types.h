/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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
#include "apidefs.h"
#include "rdcarray.h"
#include "rdcstr.h"
#include "replay_enums.h"
#include "resourceid.h"
#include "stringise.h"

DOCUMENT(R"(
PointerVal()
PointerVal(other: PointerVal)

A 64-bit pointer value with optional type information.
)")
struct PointerVal
{
  DOCUMENT("");
  PointerVal() = default;
  PointerVal(const PointerVal &) = default;
  PointerVal &operator=(const PointerVal &) = default;

  DOCUMENT(R"(The actual pointer value itself.

:type: int
)");
  uint64_t pointer;

  DOCUMENT(R"(An optional :class:`ResourceId` identifying the shader containing the type info.

:type: ResourceId
)");
  ResourceId shader;

  DOCUMENT(R"(The index into :data:`ShaderReflection.pointerTypes` of the pointed type.

:type: int
)");
  uint32_t pointerTypeID;
};

DECLARE_STRINGISE_TYPE(PointerVal);

struct DescriptorAccess;

DOCUMENT(R"(
ShaderBindIndex()
ShaderBindIndex(other: ShaderBindIndex)
ShaderBindIndex(category: DescriptorCategory, index: int)
ShaderBindIndex(category: DescriptorCategory, index: int, arrayElement: int)
ShaderBindIndex(access: DescriptorAccess)

References a particular individual binding element in a shader interface.

This is the shader interface side of a :class:`DescriptorAccess` and so can be compared to one to
check if an access refers to a given index or not.

The context of which shader reflection this index refers to must be provided to properly interpret
this information, as it is relative to a particular :class:`ShaderReflection`.
)");
struct ShaderBindIndex
{
  DOCUMENT("");
  ShaderBindIndex()
  {
    category = DescriptorCategory::Unknown;
    index = 0;
    arrayElement = 0;
  }
  ShaderBindIndex(const ShaderBindIndex &) = default;
  ShaderBindIndex &operator=(const ShaderBindIndex &) = default;

  ShaderBindIndex(DescriptorCategory category, uint32_t index, uint32_t arrayElement)
      : category(category), index(index), arrayElement(arrayElement)
  {
  }
  ShaderBindIndex(DescriptorCategory category, uint32_t index) : ShaderBindIndex(category, index, 0)
  {
  }
  ShaderBindIndex(const DescriptorAccess &access);

  bool operator<(const ShaderBindIndex &o) const
  {
    if(!(category == o.category))
      return category < o.category;
    if(!(index == o.index))
      return index < o.index;
    return arrayElement < o.arrayElement;
  }
  bool operator>(const ShaderBindIndex &o) const
  {
    if(!(category == o.category))
      return category > o.category;
    if(!(index == o.index))
      return index > o.index;
    return arrayElement > o.arrayElement;
  }
  bool operator==(const ShaderBindIndex &o) const
  {
    return category == o.category && index == o.index && arrayElement == o.arrayElement;
  }

  DOCUMENT(R"(The type of binding this refers to, with each category referring to a different
shader interface in the :data:`ShaderReflection`.

:type: DescriptorCategory
)");
  DescriptorCategory category;

  DOCUMENT(R"(The index within the given :data:`category` for the binding.

:type: int
)");
  uint32_t index;

  DOCUMENT(R"(If the binding identified by :data:`category` and :data:`index` is arrayed, this
identifies the particular array index being referred to.

:type: int
)");
  uint32_t arrayElement;
};

DECLARE_REFLECTION_STRUCT(ShaderBindIndex);

DOCUMENT(R"(
ShaderDirectAccess()
ShaderDirectAccess(other: ShaderDirectAccess)
ShaderDirectAccess(category: DescriptorCategory, index: int)
ShaderDirectAccess(type: DescriptorType, descriptorStore: ResourceId, byteOffset: int, byteSize: int)

References a particular resource accessed via the shader using direct heap access (as opposed to a direct binding).
)");
struct ShaderDirectAccess
{
  DOCUMENT("");
  ShaderDirectAccess()
  {
    type = DescriptorType::Unknown;
    descriptorStore = ResourceId();
    byteOffset = 0;
    byteSize = 0;
  }
  ShaderDirectAccess(const ShaderDirectAccess &) = default;
  ShaderDirectAccess &operator=(const ShaderDirectAccess &) = default;

  ShaderDirectAccess(DescriptorType type, ResourceId descriptorStore, uint32_t byteOffset,
                     uint32_t byteSize)
      : type(type), descriptorStore(descriptorStore), byteOffset(byteOffset), byteSize(byteSize)
  {
  }
  ShaderDirectAccess(const DescriptorAccess &access);

  bool operator<(const ShaderDirectAccess &o) const
  {
    if(type != o.type)
      return type < o.type;
    if(descriptorStore != o.descriptorStore)
      return descriptorStore < o.descriptorStore;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    return byteSize < o.byteSize;
  }
  bool operator==(const ShaderDirectAccess &o) const
  {
    return type == o.type && descriptorStore == o.descriptorStore && byteOffset == o.byteOffset &&
           byteSize == o.byteSize;
  }

  DOCUMENT(R"(The type of the resource being accessed.

:type: DescriptorType
)");
  DescriptorType type;

  DOCUMENT(R"(The backing storage of the descriptor.

:type: ResourceId
)");
  ResourceId descriptorStore;
  DOCUMENT(R"(The offset in bytes to the descriptor in the descriptor store.

:type: int
)");
  uint32_t byteOffset = 0;
  DOCUMENT(R"(The size in bytes of the descriptor.

:type: int
)");
  uint32_t byteSize = 0;
};

DECLARE_REFLECTION_STRUCT(ShaderDirectAccess);

#if !defined(SWIG)
// similarly these need to be pre-declared for use in rdhalf
extern "C" RENDERDOC_API float RENDERDOC_CC RENDERDOC_HalfToFloat(uint16_t half);
extern "C" RENDERDOC_API uint16_t RENDERDOC_CC RENDERDOC_FloatToHalf(float flt);
#endif

struct rdhalf
{
#if !defined(SWIG)
  static rdhalf make(const uint16_t &u)
  {
    rdhalf ret;
    ret.storage = u;
    return ret;
  }
  static rdhalf make(const float &f)
  {
    rdhalf ret;
    ret.storage = RENDERDOC_FloatToHalf(f);
    return ret;
  }
  void set(const uint16_t &u) { storage = u; }
  void set(const float &f) { storage = RENDERDOC_FloatToHalf(f); }
#endif
  explicit operator float() const { return RENDERDOC_HalfToFloat(storage); }
  explicit operator uint16_t() const { return storage; }
private:
  uint16_t storage;
};
DECLARE_STRINGISE_TYPE(rdhalf);

DOCUMENT(R"(
ShaderValue()
ShaderValue(other: ShaderValue)

A C union that holds 16 values, with each different basic variable type.
)");
union ShaderValue
{
  DOCUMENT(R"(16-tuple of ``float`` values.

:type: Tuple[float,...]
)");
  rdcfixedarray<float, 16> f32v;

  DOCUMENT(R"(16-tuple of 32-bit signed integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<int32_t, 16> s32v;

  DOCUMENT(R"(16-tuple of 32-bit unsigned integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<uint32_t, 16> u32v;

  DOCUMENT(R"(16-tuple of ``double`` values.

:type: Tuple[float,...]
)");
  rdcfixedarray<double, 16> f64v;

  DOCUMENT(R"(16-tuple of 16-bit half-precision float values.

:type: Tuple[int,...]
)");
  rdcfixedarray<rdhalf, 16> f16v;

  DOCUMENT(R"(16-tuple of 64-bit unsigned integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<uint64_t, 16> u64v;

  DOCUMENT(R"(16-tuple of 64-bit signed integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<int64_t, 16> s64v;

  DOCUMENT(R"(16-tuple of 16-bit unsigned integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<uint16_t, 16> u16v;

  DOCUMENT(R"(16-tuple of 16-bit signed integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<int16_t, 16> s16v;

  DOCUMENT(R"(16-tuple of 8-bit unsigned integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<uint8_t, 16> u8v;

  DOCUMENT(R"(16-tuple of 8-bit signed integer values.

:type: Tuple[int,...]
)");
  rdcfixedarray<int8_t, 16> s8v;
};

DOCUMENT(R"(
ShaderVariable()
ShaderVariable(other: ShaderVariable)
ShaderVariable(name: str, x: float, y: float, z: float, w: float)
ShaderVariable(name: str, x: int, y: int, z: int, w: int)

Holds a single named shader variable. It contains either a primitive type (up to a 4x4
matrix of a :class:`basic type <VarType>`) or a list of members, which can either be struct or array
members of this parent variable.

Matrices are always stored row-major. If necessary they are transposed when retrieving from the raw
data bytes when they are specified to be column-major in the API/shader metadata.
)");
struct ShaderVariable
{
  DOCUMENT("");
  ShaderVariable()
  {
    rows = columns = 0;
    type = VarType::Unknown;
    memset(&value, 0, sizeof(value));
  }
  ShaderVariable(const ShaderVariable &) = default;
  ShaderVariable &operator=(const ShaderVariable &) = default;
#if !defined(SWIG)
  ShaderVariable(ShaderVariable &&) = default;
  ShaderVariable &operator=(ShaderVariable &&) = default;
#endif
  ShaderVariable(const rdcstr &n, float x, float y, float z, float w)
  {
    name = n;
    rows = 1;
    columns = 4;
    memset(&value, 0, sizeof(value));
    type = VarType::Float;
    value.f32v[0] = x;
    value.f32v[1] = y;
    value.f32v[2] = z;
    value.f32v[3] = w;
  }
  ShaderVariable(const rdcstr &n, int x, int y, int z, int w)
  {
    name = n;
    rows = 1;
    columns = 4;
    memset(&value, 0, sizeof(value));
    type = VarType::SInt;
    value.s32v[0] = x;
    value.s32v[1] = y;
    value.s32v[2] = z;
    value.s32v[3] = w;
  }
  ShaderVariable(const rdcstr &n, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
  {
    name = n;
    rows = 1;
    columns = 4;
    memset(&value, 0, sizeof(value));
    type = VarType::UInt;
    value.u32v[0] = x;
    value.u32v[1] = y;
    value.u32v[2] = z;
    value.u32v[3] = w;
  }
  bool operator==(const ShaderVariable &o) const
  {
    return rows == o.rows && columns == o.columns && name == o.name && type == o.type &&
           flags == o.flags && value.u64v == o.value.u64v && members == o.members;
  }
  bool operator<(const ShaderVariable &o) const
  {
    if(!(rows == o.rows))
      return rows < o.rows;
    if(!(columns == o.columns))
      return columns < o.columns;
    if(!(name == o.name))
      return name < o.name;
    if(!(type == o.type))
      return type < o.type;
    if(!(flags == o.flags))
      return flags < o.flags;
    if(value.u64v != o.value.u64v)
      return value.u64v < o.value.u64v;
    if(!(members == o.members))
      return members < o.members;
    return false;
  }

  DOCUMENT(R"(The name of this variable.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The number of rows in this matrix.

:type: int
)");
  uint8_t rows;
  DOCUMENT(R"(The number of columns in this matrix.

:type: int
)");
  uint8_t columns;

  DOCUMENT(R"(The :class:`basic type <VarType>` of this variable.

:type: VarType
)");
  VarType type;

  DOCUMENT(R"(The flags controlling how this constant is interpreted and displayed.

:type: ShaderVariableFlags
)");
  ShaderVariableFlags flags = ShaderVariableFlags::NoFlags;

  DOCUMENT(R"(The contents of this variable if it has no members.

:type: ShaderValue
)");
  ShaderValue value;

  DOCUMENT(R"(The members of this variable.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> members;

  DOCUMENT(R"(Helper function for checking if :data:`flags` has
:data:`ShaderVariableFlags.RowMajorMatrix` set. This is entirely equivalent to checking that flag
manually, but since it is common this helper is provided.

.. note::
  Vectors and scalars will be marked as row-major by convention for convenience.

:return: If the storage is row-major order in memory
:rtype: bool
)");
  inline bool RowMajor() const { return bool(flags & ShaderVariableFlags::RowMajorMatrix); }
  DOCUMENT(R"(Helper function for checking if :data:`flags` *does not* have
:data:`ShaderVariableFlags.RowMajorMatrix` set. This is entirely equivalent to checking that flag
manually, but since it is common this helper is provided.

.. note::
  Vectors and scalars will be marked as row-major by convention for convenience.

:return: If the storage is column-major order in memory
:rtype: bool
)");
  inline bool ColMajor() const { return !(flags & ShaderVariableFlags::RowMajorMatrix); }
  DOCUMENT(R"(Utility function for setting a pointer value with no type information.

:param int pointer: The actual pointer value.
)");
  inline void SetTypelessPointer(uint64_t pointer)
  {
    type = VarType::GPUPointer;
    value.u64v[0] = pointer;
  }

  DOCUMENT(R"(Utility function for setting a pointer value with type information.

:param int pointer: The actual pointer value.
:param ResourceId shader: The shader containing the type information.
:param int pointerTypeID: The type's index in the shader's :data:`ShaderReflection.pointerTypes`
  list.
)");
  inline void SetTypedPointer(uint64_t pointer, ResourceId shader, uint32_t pointerTypeID)
  {
    type = VarType::GPUPointer;
    value.u64v[0] = pointer;
    value.u64v[1] = pointerTypeID;
    static_assert(sizeof(shader) == sizeof(value.u64v[2]), "ResourceId can't be packed");
    memcpy(&value.u64v[2], &shader, sizeof(shader));
  }

  DOCUMENT(R"(Utility function for getting a pointer value, with optional type information.

.. note::

  The return value is undefined if this variable is not a pointer.

:return: A :class:`PointerVal` with the pointer value.
:rtype: PointerVal
)");
  inline PointerVal GetPointer() const
  {
    ResourceId pointerShader;
    memcpy(&pointerShader, &value.u64v[2], sizeof(pointerShader));
    return {value.u64v[0], pointerShader, uint32_t(value.u64v[1] & 0xFFFFFFFF)};
  }

  DOCUMENT(R"(Utility function for setting a reference to a shader binding.

The :class:`ShaderBindIndex` uniquely refers to a given shader binding in one of the shader interfaces
(constant blocks, samplers, read-only and read-write resources) and if necessary the element within
an arrayed binding.

:param ShaderBindIndex idx: The index of the bind being referred to.
)");
  inline void SetBindIndex(const ShaderBindIndex &idx)
  {
    value.u32v[0] = (uint32_t)idx.category;
    value.u32v[1] = idx.index;
    value.u32v[2] = idx.arrayElement;
    // This marks the variable as ShaderBindIndex and not ShaderDirectAccess
    value.u32v[3] = 0;
  }

  DOCUMENT(R"(Utility function for getting a shader binding referenced by this variable.

.. note::

  The return value is undefined if this variable is not a binding reference.

:return: A :class:`ShaderBindIndex` with the binding referenced.
:rtype: ShaderBindIndex
)");
  inline ShaderBindIndex GetBindIndex() const
  {
    return ShaderBindIndex((DescriptorCategory)value.u32v[0], value.u32v[1], value.u32v[2]);
  }
  DOCUMENT(R"(Utility function for setting a resource which is accessed directly from a shader without using bindings.

The :class:`ShaderDirectAccess` uniquely refers to a resource descriptor.

:param ShaderDirectAccess access: The resource descriptor being referenced.
)");
  inline void SetDirectAccess(const ShaderDirectAccess &access)
  {
    value.u32v[0] = (uint32_t)access.type;
    value.u32v[1] = access.byteOffset;
    value.u32v[2] = access.byteSize;
    // This marks the variable as ShaderDirectAccess and not ShaderBindIndex
    value.u32v[3] = 1;
    static_assert(sizeof(access.descriptorStore) == sizeof(value.u64v[2]),
                  "ResourceId can't be packed");
    memcpy(&value.u64v[2], &access.descriptorStore, sizeof(access.descriptorStore));
  }

  DOCUMENT(R"(Utility function for getting the resource which is accessed directly from a shader without using bindings.

.. note::

  The return value is undefined if this variable is not a resource referenced directly by a shader.

:return: A :class:`ShaderDirectAccess` containing the resource reference.
:rtype: ShaderDirectAccess
)");
  inline ShaderDirectAccess GetDirectAccess() const
  {
    ResourceId descriptorStore;
    memcpy(&descriptorStore, &value.u64v[2], sizeof(descriptorStore));
    return ShaderDirectAccess((DescriptorType)value.u64v[0], descriptorStore, value.u32v[1],
                              value.u32v[2]);
  }

  DOCUMENT(R"(Utility function to check if this variable stores a resource reference directly accessed by a shader.

:return: If the variable represents a :class:`ShaderDirectAccess`.
:rtype: bool
)");
  inline bool IsDirectAccess() const { return (value.u32v[3] == 1); }
};

DECLARE_REFLECTION_STRUCT(ShaderVariable);

DOCUMENT(R"(
DebugVariableReference()
DebugVariableReference(other: DebugVariableReference)
DebugVariableReference(type: DebugVariableType, name: str, component: int)

A particular component of a debugging variable that a high-level variable component maps to
)");
struct DebugVariableReference
{
  DOCUMENT("");
  DebugVariableReference() = default;
  DebugVariableReference(const DebugVariableReference &) = default;
  DebugVariableReference &operator=(const DebugVariableReference &) = default;
  DebugVariableReference(DebugVariableType type, rdcstr name, uint32_t component = 0)
      : name(name), type(type), component(component)
  {
  }
#if !defined(SWIG)
  DebugVariableReference(DebugVariableReference &&) = default;
  DebugVariableReference &operator=(DebugVariableReference &&) = default;
#endif
  bool operator==(const DebugVariableReference &o) const
  {
    return name == o.name && type == o.type && component == o.component;
  }
  bool operator<(const DebugVariableReference &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(type == o.type))
      return type < o.type;
    if(!(component == o.component))
      return component < o.component;
    return false;
  }

  DOCUMENT(R"(The name of the base debug variable.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The type of variable this is referring to.

:type: DebugVariableType
)");
  DebugVariableType type = DebugVariableType::Undefined;

  DOCUMENT(R"(The component within the variable.

:type: int
)");
  uint32_t component = 0;
};

DECLARE_REFLECTION_STRUCT(DebugVariableReference);

DOCUMENT(R"(
SourceVariableMapping()
SourceVariableMapping(other: SourceVariableMapping)

Maps the contents of a high-level source variable to one or more shader variables in a
:class:`ShaderDebugState`, with type information.

A single high-level variable may be represented by multiple mappings but only along regular
boundaries, typically whole vectors. For example an array may have each element in a different
mapping, or a matrix may have a mapping per row. The properties such as :data:`rows` and
:data:`columns` reflect the *parent* object.

.. note::

  There is not necessarily a 1:1 mapping from source variable to debug variable, so this can change
  over time.
)");
struct SourceVariableMapping
{
  DOCUMENT("");
  SourceVariableMapping() = default;
  SourceVariableMapping(const SourceVariableMapping &) = default;
  SourceVariableMapping &operator=(const SourceVariableMapping &) = default;
#if !defined(SWIG)
  SourceVariableMapping(SourceVariableMapping &&) = default;
  SourceVariableMapping &operator=(SourceVariableMapping &&) = default;
#endif

  bool operator==(const SourceVariableMapping &o) const
  {
    return name == o.name && type == o.type && rows == o.rows && columns == o.columns &&
           offset == o.offset && signatureIndex == o.signatureIndex && variables == o.variables;
  }
  bool operator<(const SourceVariableMapping &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(type == o.type))
      return type < o.type;
    if(!(rows == o.rows))
      return rows < o.rows;
    if(!(columns == o.columns))
      return columns < o.columns;
    if(!(offset == o.offset))
      return offset < o.offset;
    if(!(signatureIndex == o.signatureIndex))
      return signatureIndex < o.signatureIndex;
    if(!(variables == o.variables))
      return variables < o.variables;
    return false;
  }

  DOCUMENT(R"(The name and member of this source variable that's being mapped from.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The variable type of the source being mapped from, if the debug variable is untyped.

:type: VarType
)");
  VarType type = VarType::Unknown;

  DOCUMENT(R"(A flag indicating if this mapping indicates an undefined value - the meaning of which
is compiler defined but may mean either uninitialised data, unassigned values, or optimised away values.

:type: bool
)");
  bool undefinedValue = false;

  DOCUMENT(R"(The number of rows in this variable - 1 for vectors, >1 for matrices.

:type: int
)");
  uint32_t rows = 0;

  DOCUMENT(R"(The number of columns in this variable.

:type: int
)");
  uint32_t columns = 0;

  DOCUMENT(R"(The offset in the parent source variable, for struct members. Useful for sorting.

:type: int
)");
  uint32_t offset;

  DOCUMENT(R"(The index in the input or output signature of the shader that this variable represents.

The type of signature can be disambiguated by the debug variables referenced - inputs are stored
separately.

This will be set to -1 if the variable is not part of either signature.

:type: int
)");
  int32_t signatureIndex = -1;

  DOCUMENT(R"(The debug variables that the components of this high level variable map to. Multiple
ranges could refer to the same variable if a contiguous range is mapped to - the mapping is
component-by-component to greatly simplify algorithms at the expense of a small amount of storage
space.

:type: List[DebugVariableReference]
)");
  rdcarray<DebugVariableReference> variables;
};
DECLARE_REFLECTION_STRUCT(SourceVariableMapping);

DOCUMENT(R"(
LineColumnInfo()
LineColumnInfo(other: LineColumnInfo)

Details the current region of code that an instruction maps to.
)");
struct LineColumnInfo
{
  DOCUMENT("");
  LineColumnInfo() = default;
  LineColumnInfo(const LineColumnInfo &) = default;
  LineColumnInfo &operator=(const LineColumnInfo &) = default;

  bool operator==(const LineColumnInfo &o) const
  {
    return disassemblyLine == o.disassemblyLine && fileIndex == o.fileIndex &&
           lineStart == o.lineStart && lineEnd == o.lineEnd && colStart == o.colStart &&
           colEnd == o.colEnd;
  }
  bool operator<(const LineColumnInfo &o) const
  {
    if(!(disassemblyLine == o.disassemblyLine))
      return disassemblyLine < o.disassemblyLine;
    if(!(fileIndex == o.fileIndex))
      return fileIndex < o.fileIndex;
    if(!(lineStart == o.lineStart))
      return lineStart < o.lineStart;
    if(!(lineEnd == o.lineEnd))
      return lineEnd < o.lineEnd;
    if(!(colStart == o.colStart))
      return colStart < o.colStart;
    if(!(colEnd == o.colEnd))
      return colEnd < o.colEnd;
    return false;
  }

  DOCUMENT(R"(
:param LineColumnInfo o: The object to compare against.
:return: ``True`` if this object is equal to the parameter, disregarding :data:`disassemblyLine`.
:rtype: bool
)");
  bool SourceEqual(const LineColumnInfo &o) const
  {
    // comparison without considering the disassembly line
    return fileIndex == o.fileIndex && lineStart == o.lineStart && lineEnd == o.lineEnd &&
           colStart == o.colStart && colEnd == o.colEnd;
  }

  DOCUMENT(R"(The line (starting from 1) in the disassembly where this instruction is located.

:type: int
)");
  uint32_t disassemblyLine = 0;

  DOCUMENT(R"(The current file, as an index into the list of files for this shader.

If this is negative, no source mapping is available and only :data:`disassemblyLine` is valid.

:type: int
)");
  int32_t fileIndex = -1;

  DOCUMENT(R"(The starting line-number (starting from 1) of the source code.

:type: int
)");
  uint32_t lineStart = 0;

  DOCUMENT(R"(The ending line-number (starting from 1) of the source code.

:type: int
)");
  uint32_t lineEnd = 0;

  DOCUMENT(R"(The column number (starting from 1) of the start of the code on the line specified by
:data:`lineStart`. If set to 0, no column information is available and the whole lines should be
treated as covering the code.

:type: int
)");
  uint32_t colStart = 0;

  DOCUMENT(R"(The column number (starting from 1) of the end of the code on the line specified by
:data:`lineEnd`. If set to 0, no column information is available and the whole lines should be
treated as covering the code.

:type: int
)");
  uint32_t colEnd = 0;
};
DECLARE_REFLECTION_STRUCT(LineColumnInfo);

DOCUMENT(R"(
InstructionSourceInfo()
InstructionSourceInfo(other: InstructionSourceInfo)

Gives per-instruction source code mapping information, including what line(s) correspond
to this instruction and which source variables exist
)");
struct InstructionSourceInfo
{
  DOCUMENT("");
  InstructionSourceInfo() = default;
  InstructionSourceInfo(const InstructionSourceInfo &) = default;
  InstructionSourceInfo &operator=(const InstructionSourceInfo &) = default;

  bool operator==(const InstructionSourceInfo &o) const { return instruction == o.instruction; }
  bool operator<(const InstructionSourceInfo &o) const { return instruction < o.instruction; }
  DOCUMENT(R"(The instruction that this information is for.

:type: int
)");
  uint32_t instruction;

  DOCUMENT(R"(The source location that this instruction corresponds to

:type: LineColumnInfo
)");
  LineColumnInfo lineInfo;

  DOCUMENT(R"(An optional mapping of which high-level source variables map to which debug variables
and including extra type information.

This list contains source variable mapping that is only valid at this instruction, and is fully
complete & redundant including all previous source variables that are still valid at this
instruction.

:type: List[SourceVariableMapping]
)");
  rdcarray<SourceVariableMapping> sourceVars;
};
DECLARE_REFLECTION_STRUCT(InstructionSourceInfo);

DOCUMENT(R"(
ShaderVariableChange()
ShaderVariableChange(other: ShaderVariableChange)

This stores the before and after state of a :class:`ShaderVariable`.
)");
struct ShaderVariableChange
{
  DOCUMENT("");
  ShaderVariableChange() = default;
  ShaderVariableChange(const ShaderVariableChange &) = default;
  ShaderVariableChange &operator=(const ShaderVariableChange &) = default;
#if !defined(SWIG)
  ShaderVariableChange(ShaderVariableChange &&) = default;
  ShaderVariableChange &operator=(ShaderVariableChange &&) = default;
#endif

  bool operator==(const ShaderVariableChange &o) const
  {
    return before == o.before && after == o.after;
  }
  bool operator<(const ShaderVariableChange &o) const
  {
    if(!(before == o.before))
      return before < o.before;
    if(!(after == o.after))
      return after < o.after;
    return false;
  }

  DOCUMENT(R"(The value of the variable before the change. If this variable is uninitialised that
means the variable came into existence on this step.

:type: ShaderVariable
)");
  ShaderVariable before;

  DOCUMENT(R"(The value of the variable after the change. If this variable is uninitialised that
means the variable stopped existing on this step.

:type: ShaderVariable
)");
  ShaderVariable after;
};
DECLARE_REFLECTION_STRUCT(ShaderVariableChange);

DOCUMENT(R"(
ShaderDebugState()
ShaderDebugState(other: ShaderDebugState)

This stores the current state of shader debugging at one particular step in the shader,
with all mutable variable contents.
)");
struct ShaderDebugState
{
  DOCUMENT("");
  ShaderDebugState() = default;
  ShaderDebugState(const ShaderDebugState &) = default;
  ShaderDebugState &operator=(const ShaderDebugState &) = default;
#if !defined(SWIG)
  ShaderDebugState(ShaderDebugState &&) = default;
  ShaderDebugState &operator=(ShaderDebugState &&) = default;
#endif

  bool operator==(const ShaderDebugState &o) const
  {
    return nextInstruction == o.nextInstruction && flags == o.flags && changes == o.changes &&
           stepIndex == o.stepIndex;
  }
  bool operator<(const ShaderDebugState &o) const
  {
    if(!(nextInstruction == o.nextInstruction))
      return nextInstruction < o.nextInstruction;
    if(!(flags == o.flags))
      return flags < o.flags;
    if(!(stepIndex == o.stepIndex))
      return stepIndex < o.stepIndex;
    if(!(changes == o.changes))
      return changes < o.changes;
    return false;
  }

  DOCUMENT(R"(The next instruction to be executed after this state. The initial state before any
shader execution happened will have ``nextInstruction == 0``.

:type: int
)");
  uint32_t nextInstruction = 0;

  DOCUMENT(R"(The program counter within the debug trace. The initial state will be index 0, and it
will increment linearly after that regardless of loops or branching.

:type: int
)");
  uint32_t stepIndex = 0;

  DOCUMENT(R"(A set of :class:`ShaderEvents` flags that indicate what events happened on this step.

:type: ShaderEvents
)");
  ShaderEvents flags = ShaderEvents::NoEvent;

  DOCUMENT(R"(The changes in mutable variables for this shader. The change documents the
bidirectional change of variables, so that a single state can be updated either forwards or
backwards using the information.

:type: List[ShaderVariableChange]
)");
  rdcarray<ShaderVariableChange> changes;

  DOCUMENT(R"(The function names in the current callstack at this instruction.

The oldest/outer function is first in the list, the newest/inner function is last.

:type: List[str]
)");
  rdcarray<rdcstr> callstack;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugState);

DOCUMENT("An opaque structure that has internal state for shader debugging");
struct ShaderDebugger
{
protected:
  DOCUMENT("");
  ShaderDebugger() = default;
  ShaderDebugger(const ShaderDebugger &) = default;
  ShaderDebugger &operator=(const ShaderDebugger &) = default;

public:
  virtual ~ShaderDebugger() = default;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugger);

DOCUMENT(R"(
This stores the whole state of a shader's execution from start to finish, with each
individual debugging step along the way, as well as the immutable global constant values that do not
change with shader execution.
)");
struct ShaderDebugTrace
{
  // do not allow swig to create/copy traces
#if defined(SWIG)
protected:
  DOCUMENT("");
  ShaderDebugTrace() = default;
  ShaderDebugTrace(const ShaderDebugTrace &) = default;
  ShaderDebugTrace &operator=(const ShaderDebugTrace &) = default;

public:
#else
  ShaderDebugTrace() = default;
  ShaderDebugTrace(const ShaderDebugTrace &) = default;
  ShaderDebugTrace &operator=(const ShaderDebugTrace &) = default;
#endif

  DOCUMENT(R"(The shader stage being debugged in this trace

:type: ShaderStage
)");
  ShaderStage stage;

  DOCUMENT(R"(The input variables for this shader.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> inputs;

  DOCUMENT(R"(Constant buffer backed variables for this shader.

Each entry in this list corresponds to a constant block with the same index in the
:data:`ShaderReflection.constantBlocks` list, which can be used to look up the metadata.

Depending on the underlying shader representation, the constant block may retain any structure or
it may have been vectorised and flattened.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> constantBlocks;

  DOCUMENT(R"(The read-only resource variables for this shader.

The 'value' of the variable is always a single unsigned integer, which is the bindpoint - an index
into the :data:`ShaderReflection.readOnlyResources` list, which can be used to look up the
other metadata as well as find the binding from the pipeline state.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> readOnlyResources;

  DOCUMENT(R"(The read-write resource variables for this shader.

The 'value' of the variable is always a single unsigned integer, which is the bindpoint - an index
into the :data:`ShaderReflection.readWriteResources` list, which can be used to look up the
other metadata as well as find the binding from the pipeline state.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> readWriteResources;

  DOCUMENT(R"(The sampler variables for this shader.

The 'value' of the variable is always a single unsigned integer, which is the bindpoint - an index
into the :data:`ShaderReflection.samplers` list, which can be used to look up the
other metadata as well as find the binding from the pipeline state.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> samplers;

  DOCUMENT(R"(An optional mapping from high-level source variables to which debug variables and
includes extra type information.

This list contains source variable mapping that is valid for the lifetime of a debug trace. It may
be empty if there is no source variable mapping that extends to the life of the debug trace.

:type: List[SourceVariableMapping]
)");
  rdcarray<SourceVariableMapping> sourceVars;

  DOCUMENT(R"(An opaque handle identifying by the underlying debugger, which is used to simulate the
shader and generate new debug states.

If this is ``None`` then the trace is invalid.

:type: Optional[ShaderDebugger]
)");
  ShaderDebugger *debugger = NULL;

  DOCUMENT(R"(An array of the same size as the number of instructions in the shader, with
per-instruction information such as source line mapping, and source variables.

.. warning::

  This array is *not* indexed by instruction. Since it is common for adjacent instructions to have
  effectively identical source information, this array only stores unique information ordered by
  instruction. On some internal representations this may be one entry per instruction, and on others
  it may be sparse and require a binary lookup to locate the corresponding information for an
  instruction. If no direct match is found, the lower bound match is valid (i.e. the data for
  instruction A before the data for instruction B is valid for all instructions in range ``[A, B)``.

:type: List[InstructionSourceInfo]
)");
  rdcarray<InstructionSourceInfo> instInfo;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugTrace);

DOCUMENT(R"(
SigParameter()
SigParameter(other: SigParameter)

The information describing an input or output signature element describing the interface
between shader stages.

.. data:: NoIndex

  Value for an index that means it is invalid or not applicable for this parameter.
)");
struct SigParameter
{
  DOCUMENT("");
  SigParameter() = default;
  SigParameter(const SigParameter &) = default;
  SigParameter &operator=(const SigParameter &) = default;

  bool operator==(const SigParameter &o) const
  {
    return varName == o.varName && semanticName == o.semanticName &&
           semanticIdxName == o.semanticIdxName && semanticIndex == o.semanticIndex &&
           regIndex == o.regIndex && systemValue == o.systemValue && varType == o.varType &&
           regChannelMask == o.regChannelMask && channelUsedMask == o.channelUsedMask &&
           needSemanticIndex == o.needSemanticIndex && compCount == o.compCount &&
           stream == o.stream;
  }
  bool operator<(const SigParameter &o) const
  {
    if(!(varName == o.varName))
      return varName < o.varName;
    if(!(semanticName == o.semanticName))
      return semanticName < o.semanticName;
    if(!(semanticIdxName == o.semanticIdxName))
      return semanticIdxName < o.semanticIdxName;
    if(!(semanticIndex == o.semanticIndex))
      return semanticIndex < o.semanticIndex;
    if(!(regIndex == o.regIndex))
      return regIndex < o.regIndex;
    if(!(systemValue == o.systemValue))
      return systemValue < o.systemValue;
    if(!(varType == o.varType))
      return varType < o.varType;
    if(!(regChannelMask == o.regChannelMask))
      return regChannelMask < o.regChannelMask;
    if(!(channelUsedMask == o.channelUsedMask))
      return channelUsedMask < o.channelUsedMask;
    if(!(needSemanticIndex == o.needSemanticIndex))
      return needSemanticIndex < o.needSemanticIndex;
    if(!(compCount == o.compCount))
      return compCount < o.compCount;
    if(!(stream == o.stream))
      return stream < o.stream;
    return false;
  }

  DOCUMENT(R"(The name of this variable - may not be present in the metadata for all APIs.

:type: str
)");
  rdcstr varName;
  DOCUMENT(R"(The semantic name of this variable, if the API uses semantic matching for bindings.

:type: str
)");
  rdcstr semanticName;
  DOCUMENT(R"(The combined semantic name and index.

:type: str
)");
  rdcstr semanticIdxName;
  DOCUMENT(R"(The semantic index of this variable - see :data:`semanticName`.

:type: int
)");
  uint16_t semanticIndex = 0;

  DOCUMENT(R"(A flag indicating if this parameter is output at per-primitive rate rather than per-vertex.

:type: bool
)");
  bool perPrimitiveRate = false;

  DOCUMENT(R"(The index of the shader register/binding used to store this signature element.

This may be :data:`NoIndex` if the element is system-generated and not consumed by another shader
stage. See :data:`systemValue`.

:type: int
)");
  uint32_t regIndex = 0;
  DOCUMENT(R"(The :class:`ShaderBuiltin` value that this element contains.

:type: ShaderBuiltin
)");
  ShaderBuiltin systemValue = ShaderBuiltin::Undefined;

  DOCUMENT(R"(The :class:`variable type <VarType>` of data that this element stores.

:type: VarType
)");
  VarType varType = VarType::Float;

  DOCUMENT(R"(A bitmask indicating which components in the shader register are stored, for APIs that
pack signatures together.

:type: int
)");
  uint8_t regChannelMask = 0;
  DOCUMENT(R"(A bitmask indicating which components in the shader register are actually used by the
shader itself, for APIs that pack signatures together.

:type: int
)");
  uint8_t channelUsedMask = 0;

  DOCUMENT(R"(A convenience flag - ``True`` if the semantic name is unique and no index is needed.

:type: bool
)");
  bool needSemanticIndex = false;

  DOCUMENT(R"(The number of components used to store this element. See :data:`varType`.

:type: int
)");
  uint32_t compCount = 0;
  DOCUMENT(R"(Selects a stream for APIs that provide multiple output streams for the same named output.

:type: int
)");
  uint32_t stream = 0;

  static const uint32_t NoIndex = ~0U;
};

DECLARE_REFLECTION_STRUCT(SigParameter);

struct ShaderConstant;

DOCUMENT(R"(
ShaderConstantType()
ShaderConstantType(other: ShaderConstantType)

Describes the type and members of a :class:`ShaderConstant`.
)");
struct ShaderConstantType
{
  DOCUMENT("");
  ShaderConstantType() = default;
  ShaderConstantType(const ShaderConstantType &) = default;
  ShaderConstantType &operator=(const ShaderConstantType &) = default;

  bool operator==(const ShaderConstantType &o) const
  {
    return baseType == o.baseType && rows == o.rows && columns == o.columns && flags == o.flags &&
           elements == o.elements && arrayByteStride == o.arrayByteStride &&
           matrixByteStride == o.matrixByteStride && pointerTypeID == o.pointerTypeID &&
           name == o.name && members == o.members;
  }
  bool operator<(const ShaderConstantType &o) const
  {
    if(!(baseType == o.baseType))
      return baseType < o.baseType;
    if(!(rows == o.rows))
      return rows < o.rows;
    if(!(columns == o.columns))
      return columns < o.columns;
    if(!(flags == o.flags))
      return flags < o.flags;
    if(!(elements == o.elements))
      return elements < o.elements;
    if(!(arrayByteStride == o.arrayByteStride))
      return arrayByteStride < o.arrayByteStride;
    if(!(matrixByteStride == o.matrixByteStride))
      return matrixByteStride < o.matrixByteStride;
    if(!(name == o.name))
      return name < o.name;
    if(!(members == o.members))
      return members < o.members;
    return false;
  }

  DOCUMENT(R"(The name of the type of this constant, e.g. a ``struct`` name.

:type: str
)");
  rdcstr name;
  DOCUMENT(R"(Any members that this constant may contain.

:type: List[ShaderConstant]
)");
  rdcarray<ShaderConstant> members;
  DOCUMENT(R"(The flags controlling how this constant is interpreted and displayed.

:type: ShaderVariableFlags
)");
  ShaderVariableFlags flags = ShaderVariableFlags::NoFlags;
  DOCUMENT(R"(The index in :data:`ShaderReflection.pointerTypes` of the pointee type.

:type: int
)");
  uint32_t pointerTypeID = ~0U;
  DOCUMENT(R"(The number of elements in the array, or 1 if it's not an array.

:type: int
)");
  uint32_t elements = 1;
  DOCUMENT(R"(The number of bytes between the start of one element in the array and the next.

:type: int
)");
  uint32_t arrayByteStride = 0;
  DOCUMENT(R"(The base :class:`VarType` of this constant.

:type: VarType
)");
  VarType baseType = VarType::Unknown;
  DOCUMENT(R"(The number of rows in this matrix.

:type: int
)");
  uint8_t rows = 1;
  DOCUMENT(R"(The number of columns in this matrix.

:type: int
)");
  uint8_t columns = 1;
  DOCUMENT(R"(The number of bytes between the start of one column/row in a matrix and the next.

:type: int
)");
  uint8_t matrixByteStride = 0;

  DOCUMENT(R"(Helper function for checking if :data:`flags` has
:data:`ShaderVariableFlags.RowMajorMatrix` set. This is entirely equivalent to checking that flag
manually, but since it is common this helper is provided.

.. note::
  Vectors and scalars will be marked as row-major by convention for convenience.

:return: If the storage is row-major order in memory
:rtype: bool
)");
  inline bool RowMajor() const { return bool(flags & ShaderVariableFlags::RowMajorMatrix); }

  DOCUMENT(R"(Helper function for checking if :data:`flags` *does not* have
:data:`ShaderVariableFlags.RowMajorMatrix` set. This is entirely equivalent to checking that flag
manually, but since it is common this helper is provided.

.. note::
  Vectors and scalars will be marked as row-major by convention for convenience.

:return: If the storage is column-major order in memory
:rtype: bool
)");
  inline bool ColMajor() const { return !(flags & ShaderVariableFlags::RowMajorMatrix); }
};

DECLARE_REFLECTION_STRUCT(ShaderConstantType);

DOCUMENT(R"(
ShaderConstant()
ShaderConstant(other: ShaderConstant)

Contains the detail of a constant within a struct, such as a :class:`ConstantBlock`,
with its type and relative location in memory.
)");
struct ShaderConstant
{
  DOCUMENT("");
  ShaderConstant() = default;
  ShaderConstant(const ShaderConstant &) = default;
  ShaderConstant &operator=(const ShaderConstant &) = default;

  bool operator==(const ShaderConstant &o) const
  {
    return name == o.name && byteOffset == o.byteOffset && defaultValue == o.defaultValue &&
           type == o.type;
  }
  bool operator<(const ShaderConstant &o) const
  {
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(name == o.name))
      return name < o.name;
    if(!(defaultValue == o.defaultValue))
      return defaultValue < o.defaultValue;
    if(!(type == o.type))
      return type < o.type;
    return false;
  }

  DOCUMENT(R"(The name of this constant

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The byte offset of this constant relative to the parent structure

:type: int
)");
  uint32_t byteOffset = 0;

  DOCUMENT(R"(If the variable is bitfield packed, the bit offset from :data:`byteOffset` above where
this variable starts.

If the variable is not a bitfield, this value will be 0. Only integer scalars will have bitfield
packing.

.. note::
   Although the offset specified in :data:`byteOffset` is in bytes, this bitfield offset may be
   larger than 0 depending on the surrounding values and their types and packing. However it is
   guaranteed that the offset and the size (from :data:`bitFieldSize`) will be contained within the
   normal bit size for the variable type. For example if the variable type is a 32-bit integer, the
   offsets may range from 0 to 31 and the sum of offset and size will be no more than 32. If the
   variable is an 8-bit integer, similarly the offset will be 0 to 7 and the sum will be no more
   than 8.

:type: int
)");
  uint16_t bitFieldOffset = 0;

  DOCUMENT(R"(If the variable is bitfield packed, the number of bits this variable spans starting
from :data:`bitFieldOffset` into memory.

If the variable is not a bitfield, this value will be 0. Only integer scalars will have bitfield
packing.

:type: int
)");
  uint16_t bitFieldSize = 0;

  DOCUMENT(R"(If this constant is no larger than a 64-bit constant, gives a default value for it.

:type: int
)");
  uint64_t defaultValue = 0;

  DOCUMENT(R"(The type information for this constant.

:type: ShaderConstantType
)");
  ShaderConstantType type;
};

DECLARE_REFLECTION_STRUCT(ShaderConstant);

DOCUMENT(R"(
ConstantBlock()
ConstantBlock(other: ConstantBlock)

Contains the information for a block of constant values. The values are not present,
only the metadata about how the variables are stored in memory itself and their type/name
information.
)");
struct ConstantBlock
{
  DOCUMENT("");
  ConstantBlock() = default;
  ConstantBlock(const ConstantBlock &) = default;
  ConstantBlock &operator=(const ConstantBlock &) = default;

  bool operator==(const ConstantBlock &o) const
  {
    return name == o.name && variables == o.variables &&
           fixedBindSetOrSpace == o.fixedBindSetOrSpace && bufferBacked == o.bufferBacked &&
           byteSize == o.byteSize && bufferBacked == o.bufferBacked &&
           inlineDataBytes == o.inlineDataBytes && compileConstants == o.compileConstants;
  }
  bool operator<(const ConstantBlock &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(fixedBindNumber == o.fixedBindNumber))
      return fixedBindNumber < o.fixedBindNumber;
    if(!(fixedBindSetOrSpace == o.fixedBindSetOrSpace))
      return fixedBindSetOrSpace < o.fixedBindSetOrSpace;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(bufferBacked == o.bufferBacked))
      return bufferBacked < o.bufferBacked;
    if(!(inlineDataBytes == o.inlineDataBytes))
      return inlineDataBytes < o.inlineDataBytes;
    if(!(compileConstants == o.compileConstants))
      return compileConstants < o.compileConstants;
    if(!(variables == o.variables))
      return variables < o.variables;
    return false;
  }

  DOCUMENT(R"(The name of this constant block, may be empty on some APIs.

:type: str
)");
  rdcstr name;
  DOCUMENT(R"(The constants contained within this block.

:type: List[ShaderConstant]
)");
  rdcarray<ShaderConstant> variables;

  DOCUMENT(R"(The fixed binding number for this binding. The interpretation of this is API-specific
and it is provided purely for informational purposes and has no bearing on how data is accessed or
described. Similarly some bindings don't have a fixed bind number and the value here should not be
relied on.

For OpenGL only, this value is not used as bindings are dynamic and cannot be determined by the
shader reflection. Bindings must be determined only by the descriptor mapped to.

Generally speaking sorting by this number will give a reasonable ordering by binding if it exists.

.. note::
  Because this number is API-specific, there is no guarantee that it will be unique across all
  resources, though generally it will be unique within all binds of the same type. It should be used
  only within contexts that can interpret it API-specifically, or else for purely
  informational/non-semantic purposes like sorting.

:type: int
)");
  uint32_t fixedBindNumber = 0;

  DOCUMENT(R"(The fixed binding set or space for this binding. This is API-specific, on Vulkan this
gives the set and on D3D12 this gives the register space.
It is provided purely for informational purposes and has no bearing on how data is accessed or
described.

Generally speaking sorting by this number before :data:`fixedBindNumber` will give a reasonable
ordering by binding if it exists.

:type: int
)");
  uint32_t fixedBindSetOrSpace = 0;

  DOCUMENT(R"(If this binding is natively arrayed, how large is the array size. If not arrayed, this
will be set to 1.

This value may be set to a very large number if the array is unbounded in the shader.

:type: int
)");
  uint32_t bindArraySize = 1;

  DOCUMENT(R"(The total number of bytes consumed by all of the constants contained in this block.

:type: int
)");
  uint32_t byteSize = 0;
  DOCUMENT(R"(``True`` if the contents are stored in a buffer of memory. If not then they are set by
some other API-specific method, such as direct function calls or they may be compile-time
specialisation constants.

:type: bool
)");
  bool bufferBacked = true;
  DOCUMENT(R"(``True`` if this is backed by in-line data bytes rather than a specific buffer.

:type: bool
)");
  bool inlineDataBytes = false;
  DOCUMENT(R"(``True`` if this is a virtual buffer listing compile-time specialisation constants.

:type: bool
)");
  bool compileConstants = false;
};

DECLARE_REFLECTION_STRUCT(ConstantBlock);

DOCUMENT(R"(
ShaderSampler()
ShaderSampler(other: ShaderSampler)

Contains the information for a separate sampler in a shader. If the API doesn't have
the concept of separate samplers, this struct will be unused and only :class:`ShaderResource` is
relevant.

.. note:: that constant blocks will not have a shader resource entry, see :class:`ConstantBlock`.
)");
struct ShaderSampler
{
  DOCUMENT("");
  ShaderSampler() = default;
  ShaderSampler(const ShaderSampler &) = default;
  ShaderSampler &operator=(const ShaderSampler &) = default;

  bool operator==(const ShaderSampler &o) const
  {
    return name == o.name && fixedBindNumber == o.fixedBindNumber &&
           fixedBindSetOrSpace == o.fixedBindSetOrSpace && bindArraySize == o.bindArraySize;
  }
  bool operator<(const ShaderSampler &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(fixedBindNumber == o.fixedBindNumber))
      return fixedBindNumber < o.fixedBindNumber;
    if(!(fixedBindSetOrSpace == o.fixedBindSetOrSpace))
      return fixedBindSetOrSpace < o.fixedBindSetOrSpace;
    if(!(bindArraySize == o.bindArraySize))
      return bindArraySize < o.bindArraySize;
    return false;
  }
  DOCUMENT(R"(The name of this sampler.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The fixed binding number for this binding. The interpretation of this is API-specific
and it is provided purely for informational purposes and has no bearing on how data is accessed or
described. Similarly some bindings don't have a fixed bind number and the value here should not be
relied on.

For OpenGL only, this value is not used as bindings are dynamic and cannot be determined by the
shader reflection. Bindings must be determined only by the descriptor mapped to.

Generally speaking sorting by this number will give a reasonable ordering by binding if it exists.

.. note::
  Because this number is API-specific, there is no guarantee that it will be unique across all
  resources, though generally it will be unique within all binds of the same type. It should be used
  only within contexts that can interpret it API-specifically, or else for purely
  informational/non-semantic purposes like sorting.

:type: int
)");
  uint32_t fixedBindNumber = 0;

  DOCUMENT(R"(The fixed binding set or space for this binding. This is API-specific, on Vulkan this
gives the set and on D3D12 this gives the register space.
It is provided purely for informational purposes and has no bearing on how data is accessed or
described.

Generally speaking sorting by this number before :data:`fixedBindNumber` will give a reasonable
ordering by binding if it exists.

:type: int
)");
  uint32_t fixedBindSetOrSpace = 0;

  DOCUMENT(R"(If this binding is natively arrayed, how large is the array size. If not arrayed, this
will be set to 1.

This value may be set to a very large number if the array is unbounded in the shader.

:type: int
)");
  uint32_t bindArraySize = 1;
};

DECLARE_REFLECTION_STRUCT(ShaderSampler);

DOCUMENT(R"(
ShaderResource()
ShaderResource(other: ShaderResource)

Contains the information for a shader resource that is made accessible to shaders
directly by means of the API resource binding system.

.. note:: that constant blocks and samplers will not have a shader resource entry, see
  :class:`ConstantBlock` and :class:`ShaderSampler`.
)");
struct ShaderResource
{
  DOCUMENT("");
  ShaderResource() = default;
  ShaderResource(const ShaderResource &) = default;
  ShaderResource &operator=(const ShaderResource &) = default;

  bool operator==(const ShaderResource &o) const
  {
    return textureType == o.textureType && name == o.name && variableType == o.variableType &&
           fixedBindNumber == o.fixedBindNumber && fixedBindSetOrSpace == o.fixedBindSetOrSpace &&
           isTexture == o.isTexture && hasSampler == o.hasSampler &&
           isInputAttachment == o.isInputAttachment && isReadOnly == o.isReadOnly;
  }
  bool operator<(const ShaderResource &o) const
  {
    if(!(textureType == o.textureType))
      return textureType < o.textureType;
    if(!(name == o.name))
      return name < o.name;
    if(!(variableType == o.variableType))
      return variableType < o.variableType;
    if(!(fixedBindSetOrSpace == o.fixedBindSetOrSpace))
      return fixedBindSetOrSpace < o.fixedBindSetOrSpace;
    if(!(isTexture == o.isTexture))
      return isTexture < o.isTexture;
    if(!(hasSampler == o.hasSampler))
      return hasSampler < o.hasSampler;
    if(!(isInputAttachment == o.isInputAttachment))
      return isInputAttachment < o.isInputAttachment;
    if(!(isReadOnly == o.isReadOnly))
      return isReadOnly < o.isReadOnly;
    return false;
  }

  DOCUMENT(R"(The :class:`TextureType` that describes the type of this resource.

:type: TextureType
)");
  TextureType textureType;

  DOCUMENT(R"(The :class:`DescriptorType` which this resource expects to access.

:type: DescriptorType
)");
  DescriptorType descriptorType;

  DOCUMENT(R"(The name of this resource.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The type of each element of this resource.

:type: ShaderConstantType
)");
  ShaderConstantType variableType;

  DOCUMENT(R"(The fixed binding number for this binding. The interpretation of this is API-specific
and it is provided purely for informational purposes and has no bearing on how data is accessed or
described. Similarly some bindings don't have a fixed bind number and the value here should not be
relied on.

For OpenGL only, this value is not used as bindings are dynamic and cannot be determined by the
shader reflection. Bindings must be determined only by the descriptor mapped to.

Generally speaking sorting by this number will give a reasonable ordering by binding if it exists.

.. note::
  Because this number is API-specific, there is no guarantee that it will be unique across all
  resources, though generally it will be unique within all binds of the same type. It should be used
  only within contexts that can interpret it API-specifically, or else for purely
  informational/non-semantic purposes like sorting.

:type: int
)");
  uint32_t fixedBindNumber = 0;

  DOCUMENT(R"(The fixed binding set or space for this binding. This is API-specific, on Vulkan this
gives the set and on D3D12 this gives the register space.
It is provided purely for informational purposes and has no bearing on how data is accessed or
described.

Generally speaking sorting by this number before :data:`fixedBindNumber` will give a reasonable
ordering by binding if it exists.

:type: int
)");
  uint32_t fixedBindSetOrSpace = 0;

  DOCUMENT(R"(If this binding is natively arrayed, how large is the array size. If not arrayed, this
will be set to 1.

This value may be set to a very large number if the array is unbounded in the shader.

:type: int
)");
  uint32_t bindArraySize = 1;

  DOCUMENT(R"(``True`` if this resource is a texture, otherwise it is a buffer.

:type: bool
)");
  bool isTexture;
  DOCUMENT(R"(``True`` if this texture resource has a sampler as well.

:type: bool
)");
  bool hasSampler = false;
  DOCUMENT(R"(``True`` if this texture resource is a subpass input attachment.

:type: bool
)");
  bool isInputAttachment = false;
  DOCUMENT(R"(``True`` if this resource is available to the shader for reading only, otherwise it is
able to be read from and written to arbitrarily.

:type: bool
)");
  bool isReadOnly;
};

DECLARE_REFLECTION_STRUCT(ShaderResource);

DOCUMENT(R"(
ShaderEntryPoint()
ShaderEntryPoint(other: ShaderEntryPoint)
ShaderEntryPoint(name: str, stage: ShaderStage)

Describes an entry point in a shader.
)");
struct ShaderEntryPoint
{
  ShaderEntryPoint() = default;
  ShaderEntryPoint(const ShaderEntryPoint &) = default;
  ShaderEntryPoint &operator=(const ShaderEntryPoint &) = default;
  ShaderEntryPoint(const rdcstr &n, ShaderStage s) : name(n), stage(s) {}
  DOCUMENT("");
  bool operator==(const ShaderEntryPoint &o) const { return name == o.name && stage == o.stage; }
  bool operator<(const ShaderEntryPoint &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(stage == o.stage))
      return stage < o.stage;
    return false;
  }
  DOCUMENT(R"(The name of the entry point.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The :class:`ShaderStage` for this entry point .

:type: ShaderStage
)");
  ShaderStage stage;
};

DECLARE_REFLECTION_STRUCT(ShaderEntryPoint);

DOCUMENT(R"(
ShaderCompileFlag()
ShaderCompileFlag(other: ShaderCompileFlag)

Contains a single flag used at compile-time on a shader.
)");
struct ShaderCompileFlag
{
  DOCUMENT("");
  ShaderCompileFlag() = default;
  ShaderCompileFlag(const ShaderCompileFlag &) = default;
  ShaderCompileFlag &operator=(const ShaderCompileFlag &) = default;

  bool operator==(const ShaderCompileFlag &o) const { return name == o.name && value == o.value; }
  bool operator<(const ShaderCompileFlag &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(value == o.value))
      return value < o.value;
    return false;
  }
  DOCUMENT(R"(The name of the compile flag.

:type: str
)");
  rdcstr name;

  DOCUMENT(R"(The value of the compile flag.

:type: str
)");
  rdcstr value;
};

DECLARE_REFLECTION_STRUCT(ShaderCompileFlag);

DOCUMENT(R"(
ShaderCompileFlags()
ShaderCompileFlags(other: ShaderCompileFlags)

Contains the information about the compilation environment of a shader
)");
struct ShaderCompileFlags
{
  DOCUMENT("");
  ShaderCompileFlags() = default;
  ShaderCompileFlags(const ShaderCompileFlags &) = default;
  ShaderCompileFlags &operator=(const ShaderCompileFlags &) = default;

  DOCUMENT(R"(The API or compiler specific flags used to compile this shader originally.

:type: List[ShaderCompileFlag]
)");
  rdcarray<ShaderCompileFlag> flags;
};

DECLARE_REFLECTION_STRUCT(ShaderCompileFlags);

DOCUMENT(R"(
ShaderSourcePrefix()
ShaderSourcePrefix(other: ShaderSourcePrefix)

Contains the source prefix to add to a given type of shader source
)");
struct ShaderSourcePrefix
{
  DOCUMENT("");
  ShaderSourcePrefix() = default;
  ShaderSourcePrefix(const ShaderSourcePrefix &) = default;
  ShaderSourcePrefix &operator=(const ShaderSourcePrefix &) = default;

  bool operator==(const ShaderSourcePrefix &o) const
  {
    return encoding == o.encoding && prefix == o.prefix;
  }
  bool operator<(const ShaderSourcePrefix &o) const
  {
    if(!(encoding == o.encoding))
      return encoding < o.encoding;
    if(!(prefix == o.prefix))
      return prefix < o.prefix;
    return false;
  }
  DOCUMENT(R"(The encoding of the language this prefix applies to.

:type: ShaderEncoding
)");
  ShaderEncoding encoding;

  DOCUMENT(R"(The source prefix to add.

:type: str
)");
  rdcstr prefix;
};

DECLARE_REFLECTION_STRUCT(ShaderSourcePrefix);

DOCUMENT(R"(
ShaderSourceFile()
ShaderSourceFile(other: ShaderSourceFile)

Contains a source file available in a debug-compiled shader.
)");
struct ShaderSourceFile
{
  DOCUMENT("");
  ShaderSourceFile() = default;
  ShaderSourceFile(const ShaderSourceFile &) = default;
  ShaderSourceFile &operator=(const ShaderSourceFile &) = default;

  bool operator==(const ShaderSourceFile &o) const
  {
    return filename == o.filename && contents == o.contents;
  }
  bool operator<(const ShaderSourceFile &o) const
  {
    if(!(filename == o.filename))
      return filename < o.filename;
    if(!(contents == o.contents))
      return contents < o.contents;
    return false;
  }
  DOCUMENT(R"(The filename of this source file.

:type: str
)");
  rdcstr filename;

  DOCUMENT(R"(The actual contents of the file.

:type: str
)");
  rdcstr contents;
};

DECLARE_REFLECTION_STRUCT(ShaderSourceFile);

DOCUMENT(R"(
ShaderDebugInfo()
ShaderDebugInfo(other: ShaderDebugInfo)

Contains the information about a shader contained within API-specific debugging
information attached to the shader.

Primarily this means the embedded original source files.
)");
struct ShaderDebugInfo
{
  ShaderDebugInfo() {}
  ShaderDebugInfo(const ShaderDebugInfo &) = default;
  ShaderDebugInfo &operator=(const ShaderDebugInfo &) = default;

  DOCUMENT(R"(The flags used to compile this shader.

:type: ShaderCompileFlags
)");
  ShaderCompileFlags compileFlags;

  DOCUMENT(R"(The shader files encoded in the form denoted by :data:`encoding`.

The first entry in the list is always the file where the entry point is.

:type: List[ShaderSourceFile]
)");
  rdcarray<ShaderSourceFile> files;

  DOCUMENT(R"(The name of the entry point in the source code, not necessarily the same as the
entry point name exported to the API.

:type: str
)");
  rdcstr entrySourceName;

  DOCUMENT(R"(The source location of the first executable line or the entry point.

.. note::

  The information is not guaranteed to be available depending on the underlying shader format, so
  all of the elements are optional.

:type: LineColumnInfo
)");
  LineColumnInfo entryLocation;

  DOCUMENT(R"(The index of the file which should be used for re-editing this shader's entry point.

This is an optional value, and if set to ``-1`` you should fall back to using the file specified
in :data:`entryLocation`, and if no file is specified there then use the first file listed.

:type: int
)");
  int32_t editBaseFile = -1;

  DOCUMENT(R"(The :class:`ShaderEncoding` of the source. See :data:`files`.

:type: ShaderEncoding
)");
  ShaderEncoding encoding = ShaderEncoding::Unknown;

  DOCUMENT(R"(The :class:`KnownShaderTool` of the compiling tool.

:type: KnownShaderTool
)");
  KnownShaderTool compiler = KnownShaderTool::Unknown;

  DOCUMENT(R"(Indicates whether this particular shader can be debugged. In some cases even if the
API can debug shaders in general, specific shaders cannot be debugged because they use unsupported
functionality

:type: bool
)");
  bool debuggable = true;

  DOCUMENT(R"(Indicates whether this shader has debug information to allow source-level debugging.

:type: bool
)");
  bool sourceDebugInformation = false;

  DOCUMENT(R"(If :data:`debuggable` is false then this contains a simple explanation of why the
shader is not supported for debugging

:type: str
)");
  rdcstr debugStatus;

  DOCUMENT(R"(Contains a log of the debug shader loading process i.e. searching for shader PDB.

:type: str
)");
  rdcstr debugInfoLoadingLog;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugInfo);

DOCUMENT(R"(The reflection and metadata fully describing a shader.

The information in this structure is API agnostic.
)");
struct ShaderReflection
{
  // do not allow swig to create/copy shader reflections
#if defined(SWIG)
protected:
  DOCUMENT("");
  ShaderDebugTrace() = default;
  ShaderDebugTrace(const ShaderDebugTrace &) = default;
  ShaderDebugTrace &operator=(const ShaderDebugTrace &) = default;

public:
#else
  DOCUMENT("");
  ShaderReflection() = default;
  ShaderReflection(const ShaderReflection &) = default;
  ShaderReflection &operator=(const ShaderReflection &) = default;
#endif

  DOCUMENT(R"(The :class:`ResourceId` of this shader.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The entry point in the shader for this reflection, if multiple entry points exist.

:type: str
)");
  rdcstr entryPoint;

  DOCUMENT(R"(The :class:`ShaderStage` that this shader corresponds to, if multiple entry points exist.

:type: ShaderStage
)");
  ShaderStage stage;

  DOCUMENT(R"(The embedded debugging information.

:type: ShaderDebugInfo
)");
  ShaderDebugInfo debugInfo;

  DOCUMENT(R"(The :class:`ShaderEncoding` of this shader. See :data:`rawBytes`.

:type: ShaderEncoding
)");
  ShaderEncoding encoding = ShaderEncoding::Unknown;

  DOCUMENT(R"(A raw ``bytes`` dump of the original shader, encoded in the form denoted by
:data:`encoding`.

:type: bytes
)");
  bytebuf rawBytes;

  DOCUMENT(R"(The 3D dimensions of a compute workgroup, for compute shaders.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> dispatchThreadsDimension;

  DOCUMENT(R"(The output topology for geometry, tessellation and mesh shaders.

:type: Topology
)");
  Topology outputTopology = Topology::Unknown;

  DOCUMENT(R"(The input signature.

:type: List[SigParameter]
)");
  rdcarray<SigParameter> inputSignature;

  DOCUMENT(R"(The output signature.

:type: List[SigParameter]
)");
  rdcarray<SigParameter> outputSignature;

  DOCUMENT(R"(The constant block bindings.

:type: List[ConstantBlock]
)");
  rdcarray<ConstantBlock> constantBlocks;

  DOCUMENT(R"(The sampler bindings.

:type: List[ShaderSampler]
)");
  rdcarray<ShaderSampler> samplers;

  DOCUMENT(R"(The read-only resource bindings.

:type: List[ShaderResource]
)");
  rdcarray<ShaderResource> readOnlyResources;

  DOCUMENT(R"(The read-write resource bindings.

:type: List[ShaderResource]
)");
  rdcarray<ShaderResource> readWriteResources;

  // TODO expand this to encompass shader subroutines.
  DOCUMENT(R"(The list of strings with the shader's interfaces. Largely an unused API feature.

:type: List[str]
)");
  rdcarray<rdcstr> interfaces;

  DOCUMENT(R"(The list of pointer types referred to in this shader.

:type: List[ShaderConstantType]
)");
  rdcarray<ShaderConstantType> pointerTypes;

  DOCUMENT(R"(The block layout of the task-mesh communication payload.

Only relevant for task or mesh shaders, this gives the output payload (for task shaders) or the
input payload (for mesh shaders)

:type: ConstantBlock
)");
  ConstantBlock taskPayload;

  DOCUMENT(R"(The block layout of the ray payload.

Only relevant for raytracing shaders, this gives the payload accessible for read and write by ray
evaluation during the processing of the ray

:type: ConstantBlock
)");
  ConstantBlock rayPayload;

  DOCUMENT(R"(The block layout of the ray attributes structure.

Only relevant for intersection shaders and closest/any hit shaders, this gives
the attributes structure produced by a custom intersection shader which is available by hit shaders,
or else the built-in structure if no intersection shader was used and a triangle intersection is
reported.

:type: ConstantBlock
)");
  ConstantBlock rayAttributes;
};

DECLARE_REFLECTION_STRUCT(ShaderReflection);
