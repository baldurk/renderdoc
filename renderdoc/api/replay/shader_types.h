/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

DOCUMENT("A 64-bit pointer value with optional type information.")
struct PointerVal
{
  DOCUMENT("");
  PointerVal() = default;
  PointerVal(const PointerVal &) = default;
  PointerVal &operator=(const PointerVal &) = default;

  DOCUMENT("The actual pointer value itself.");
  uint64_t pointer;

  DOCUMENT("An optional :class:`ResourceId` identifying the shader containing the type info.");
  ResourceId shader;

  DOCUMENT("The index into :data:`ShaderReflection.pointerTypes` of the pointed type.");
  uint32_t pointerTypeID;
};

DECLARE_STRINGISE_TYPE(PointerVal);

DOCUMENT("References a particular element in a :class:`Bindpoint`.");
struct BindpointIndex
{
  DOCUMENT("");
  BindpointIndex()
  {
    bindset = 0;
    bind = 0;
    arrayIndex = 0;
  }
  BindpointIndex(const BindpointIndex &) = default;
  BindpointIndex &operator=(const BindpointIndex &) = default;

  BindpointIndex(int32_t s, int32_t b)
  {
    bindset = s;
    bind = b;
    arrayIndex = 0;
  }
  BindpointIndex(int32_t s, int32_t b, uint32_t a)
  {
    bindset = s;
    bind = b;
    arrayIndex = a;
  }

  bool operator<(const BindpointIndex &o) const
  {
    if(!(bindset == o.bindset))
      return bindset < o.bindset;
    if(!(bind == o.bind))
      return bind < o.bind;
    return arrayIndex < o.arrayIndex;
  }
  bool operator>(const BindpointIndex &o) const
  {
    if(!(bindset == o.bindset))
      return bindset > o.bindset;
    if(!(bind == o.bind))
      return bind > o.bind;
    return arrayIndex > o.arrayIndex;
  }
  bool operator==(const BindpointIndex &o) const
  {
    return bindset == o.bindset && bind == o.bind && arrayIndex == o.arrayIndex;
  }
  DOCUMENT("The binding set.");
  int32_t bindset;
  DOCUMENT("The binding index.");
  int32_t bind;
  DOCUMENT("If this is an arrayed binding, the element in the array being referenced.");
  uint32_t arrayIndex;
};

DECLARE_REFLECTION_STRUCT(BindpointIndex);

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

DOCUMENT("A C union that holds 16 values, with each different basic variable type.");
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

DOCUMENT(R"(Holds a single named shader variable. It contains either a primitive type (up to a 4x4
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

  DOCUMENT("The name of this variable.");
  rdcstr name;

  DOCUMENT("The number of rows in this matrix.");
  uint8_t rows;
  DOCUMENT("The number of columns in this matrix.");
  uint8_t columns;

  DOCUMENT("The :class:`basic type <VarType>` of this variable.");
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

  DOCUMENT(R"(Utility function for setting a bindpoint reference.

See :class:`ShaderBindpointMapping` for the details of how bindpoints are interpreted. The type of
binding is given by the :data:`type` member.

:param int bindset: The bind set.
:param int bind: The bind itself.
:param int arrayIndex: The array index, if the bind is an array. If it isn't an array this should be
  set to 0.
)");
  inline void SetBinding(int32_t bindset, int32_t bind, uint32_t arrayIndex)
  {
    value.s32v[0] = bindset;
    value.s32v[1] = bind;
    value.u32v[2] = arrayIndex;
  }

  DOCUMENT(R"(Utility function for getting the bindpoint referenced by this variable.

.. note::

  The return value is undefined if this variable is not a binding reference.

:return: A :class:`BindpointIndex` with the binding referenced.
:rtype: BindpointIndex
)");
  inline BindpointIndex GetBinding() const
  {
    return BindpointIndex(value.s32v[0], value.s32v[1], value.u32v[2]);
  }
};

DECLARE_REFLECTION_STRUCT(ShaderVariable);

DOCUMENT(
    "A particular component of a debugging variable that a high-level variable component maps to");
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

  DOCUMENT("The name of the base debug variable.");
  rdcstr name;

  DOCUMENT("The type of variable this is referring to.");
  DebugVariableType type = DebugVariableType::Undefined;

  DOCUMENT("The component within the variable.");
  uint32_t component = 0;
};

DECLARE_REFLECTION_STRUCT(DebugVariableReference);

DOCUMENT(R"(Maps the contents of a high-level source variable to one or more shader variables in a
:class:`ShaderDebugState`, with type information.

A single high-level variable may be represented by multiple mappings but only along regular
boundaries, typically whole vectors. For example an array may have each element in a different
mapping, or a matrix may have a mapping per row. The properties such as :data:`rows` and
:data:`elements` reflect the *parent* object.

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

  DOCUMENT("The name and member of this source variable that's being mapped from.");
  rdcstr name;

  DOCUMENT("The variable type of the source being mapped from, if the debug variable is untyped.");
  VarType type = VarType::Unknown;

  DOCUMENT("The number of rows in this variable - 1 for vectors, >1 for matrices.");
  uint32_t rows = 0;

  DOCUMENT("The number of columns in this variable.");
  uint32_t columns = 0;

  DOCUMENT("The offset in the parent source variable, for struct members. Useful for sorting.");
  uint32_t offset;

  DOCUMENT(R"(The index in the input or output signature of the shader that this variable represents.

The type of signature can be disambiguated by the debug variables referenced - inputs are stored
separately.

This will be set to -1 if the variable is not part of either signature.
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

DOCUMENT("Details the current region of code that an instruction maps to");
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

  DOCUMENT("The line (starting from 1) in the disassembly where this instruction is located.");
  uint32_t disassemblyLine = 0;

  DOCUMENT(R"(The current file, as an index into the list of files for this shader.

If this is negative, no source mapping is available and only :data:`disassemblyLine` is valid.
)");
  int32_t fileIndex = -1;

  DOCUMENT("The starting line-number (starting from 1) of the source code.");
  uint32_t lineStart = 0;

  DOCUMENT("The ending line-number (starting from 1) of the source code.");
  uint32_t lineEnd = 0;

  DOCUMENT(R"(The column number (starting from 1) of the start of the code on the line specified by
:data:`lineStart`. If set to 0, no column information is available and the whole lines should be
treated as covering the code.
)");
  uint32_t colStart = 0;

  DOCUMENT(R"(The column number (starting from 1) of the end of the code on the line specified by
:data:`lineEnd`. If set to 0, no column information is available and the whole lines should be
treated as covering the code.
)");
  uint32_t colEnd = 0;
};
DECLARE_REFLECTION_STRUCT(LineColumnInfo);

DOCUMENT(R"(Gives per-instruction source code mapping information, including what line(s) correspond
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
  DOCUMENT("The instruction that this information is for.");
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

DOCUMENT("This stores the before and after state of a :class:`ShaderVariable`.");
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
means the variable came into existance on this step.

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

DOCUMENT(R"(This stores the current state of shader debugging at one particular step in the shader,
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
)");
  uint32_t nextInstruction = 0;

  DOCUMENT(R"(The program counter within the debug trace. The initial state will be index 0, and it
will increment linearly after that regardless of loops or branching.
)");
  uint32_t stepIndex = 0;

  DOCUMENT("A set of :class:`ShaderEvents` flags that indicate what events happened on this step.");
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

DOCUMENT(R"(This stores the whole state of a shader's execution from start to finish, with each
individual debugging step along the way, as well as the immutable global constant values that do not
change with shader execution.
)");
struct ShaderDebugTrace
{
  DOCUMENT("");
  ShaderDebugTrace() = default;
  ShaderDebugTrace(const ShaderDebugTrace &) = default;
  ShaderDebugTrace &operator=(const ShaderDebugTrace &) = default;

  DOCUMENT("The shader stage being debugged in this trace");
  ShaderStage stage;

  DOCUMENT(R"(The input variables for this shader.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> inputs;

  DOCUMENT(R"(Constant buffer backed variables for this shader.

Each entry in this list corresponds to a constant block with the same index in the
:data:`ShaderBindpointMapping.constantBlocks` list, which can be used to look up the metadata.

Depending on the underlying shader representation, the constant block may retain any structure or
it may have been vectorised and flattened.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> constantBlocks;

  DOCUMENT(R"(The read-only resource variables for this shader.

The 'value' of the variable is always a single unsigned integer, which is the bindpoint - an index
into the :data:`ShaderBindpointMapping.readOnlyResources` list, which can be used to look up the
other metadata as well as find the binding from the pipeline state.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> readOnlyResources;

  DOCUMENT(R"(The read-write resource variables for this shader.

The 'value' of the variable is always a single unsigned integer, which is the bindpoint - an index
into the :data:`ShaderBindpointMapping.readWriteResources` list, which can be used to look up the
other metadata as well as find the binding from the pipeline state.

:type: List[ShaderVariable]
)");
  rdcarray<ShaderVariable> readWriteResources;

  DOCUMENT(R"(The sampler variables for this shader.

The 'value' of the variable is always a single unsigned integer, which is the bindpoint - an index
into the :data:`ShaderBindpointMapping.samplers` list, which can be used to look up the
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

:type: ShaderDebugger
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

DOCUMENT(R"(The information describing an input or output signature element describing the interface
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

  DOCUMENT("The name of this variable - may not be present in the metadata for all APIs.");
  rdcstr varName;
  DOCUMENT("The semantic name of this variable, if the API uses semantic matching for bindings.");
  rdcstr semanticName;
  DOCUMENT("The combined semantic name and index.");
  rdcstr semanticIdxName;
  DOCUMENT("The semantic index of this variable - see :data:`semanticName`.");
  uint32_t semanticIndex = 0;

  DOCUMENT(R"(The index of the shader register/binding used to store this signature element.

This may be :data:`NoIndex` if the element is system-generated and not consumed by another shader
stage. See :data:`systemValue`.
)");
  uint32_t regIndex = 0;
  DOCUMENT("The :class:`ShaderBuiltin` value that this element contains.");
  ShaderBuiltin systemValue = ShaderBuiltin::Undefined;

  DOCUMENT("The :class:`variable type <VarType>` of data that this element stores.");
  VarType varType = VarType::Float;

  DOCUMENT(R"(A bitmask indicating which components in the shader register are stored, for APIs that
pack signatures together.
)");
  uint8_t regChannelMask = 0;
  DOCUMENT(R"(A bitmask indicating which components in the shader register are actually used by the
shader itself, for APIs that pack signatures together.
)");
  uint8_t channelUsedMask = 0;

  DOCUMENT("A convenience flag - ``True`` if the semantic name is unique and no index is needed.");
  bool needSemanticIndex = false;

  DOCUMENT("The number of components used to store this element. See :data:`compType`.");
  uint32_t compCount = 0;
  DOCUMENT(
      "Selects a stream for APIs that provide multiple output streams for the same named output.");
  uint32_t stream = 0;

  static const uint32_t NoIndex = ~0U;
};

DECLARE_REFLECTION_STRUCT(SigParameter);

struct ShaderConstant;

DOCUMENT("Describes the type and members of a :class:`ShaderConstant`.");
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
  DOCUMENT("The name of the type of this constant, e.g. a ``struct`` name.");
  rdcstr name;
  DOCUMENT(R"(Any members that this constant may contain.

:type: List[ShaderConstant]
)");
  rdcarray<ShaderConstant> members;
  DOCUMENT(R"(The flags controlling how this constant is interpreted and displayed.

:type: ShaderVariableFlags
)");
  ShaderVariableFlags flags = ShaderVariableFlags::NoFlags;
  DOCUMENT("The index in :data:`ShaderReflection.pointerTypes` of the pointee type.");
  uint32_t pointerTypeID = ~0U;
  DOCUMENT("The number of elements in the array, or 1 if it's not an array.");
  uint32_t elements = 1;
  DOCUMENT("The number of bytes between the start of one element in the array and the next.");
  uint32_t arrayByteStride = 0;
  DOCUMENT("The base :class:`VarType` of this constant.");
  VarType baseType = VarType::Unknown;
  DOCUMENT("The number of rows in this matrix.");
  uint8_t rows = 1;
  DOCUMENT("The number of columns in this matrix.");
  uint8_t columns = 1;
  DOCUMENT("The number of bytes between the start of one column/row in a matrix and the next.");
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

DOCUMENT(R"(Contains the detail of a constant within a struct, such as a :class:`ConstantBlock`,
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
  DOCUMENT("The name of this constant");
  rdcstr name;
  DOCUMENT("The byte offset of this constant relative to the parent structure");
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
)");
  uint16_t bitFieldOffset = 0;
  DOCUMENT(R"(If the variable is bitfield packed, the number of bits this variable spans starting
from :data:`bitFieldOffset` into memory.

If the variable is not a bitfield, this value will be 0. Only integer scalars will have bitfield
packing.
)");
  uint16_t bitFieldSize = 0;
  DOCUMENT("If this constant is no larger than a 64-bit constant, gives a default value for it.");
  uint64_t defaultValue = 0;
  DOCUMENT(R"(The type information for this constant.

:type: ShaderConstantType
)");
  ShaderConstantType type;
};

DECLARE_REFLECTION_STRUCT(ShaderConstant);

DOCUMENT(R"(Contains the information for a block of constant values. The values are not present,
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
    return name == o.name && variables == o.variables && bindPoint == o.bindPoint &&
           byteSize == o.byteSize && bufferBacked == o.bufferBacked &&
           inlineDataBytes == o.inlineDataBytes && compileConstants == o.compileConstants;
  }
  bool operator<(const ConstantBlock &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(variables == o.variables))
      return variables < o.variables;
    if(!(bindPoint == o.bindPoint))
      return bindPoint < o.bindPoint;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(bufferBacked == o.bufferBacked))
      return bufferBacked < o.bufferBacked;
    if(!(inlineDataBytes == o.inlineDataBytes))
      return inlineDataBytes < o.inlineDataBytes;
    if(!(compileConstants == o.compileConstants))
      return compileConstants < o.compileConstants;
    return false;
  }
  DOCUMENT("The name of this constant block, may be empty on some APIs.");
  rdcstr name;
  DOCUMENT(R"(The constants contained within this block.

:type: List[ShaderConstant]
)");
  rdcarray<ShaderConstant> variables;
  DOCUMENT(R"(The bindpoint for this block. This is an index in the
:data:`ShaderBindpointMapping.constantBlocks` list.
)");
  int32_t bindPoint = 0;
  DOCUMENT("The total number of bytes consumed by all of the constants contained in this block.");
  uint32_t byteSize = 0;
  DOCUMENT(R"(``True`` if the contents are stored in a buffer of memory. If not then they are set by
some other API-specific method, such as direct function calls or they may be compile-time
specialisation constants.
)");
  bool bufferBacked = true;
  DOCUMENT("``True`` if this is backed by in-line data bytes rather than a specific buffer.");
  bool inlineDataBytes = false;
  DOCUMENT("``True`` if this is a virtual buffer listing compile-time specialisation constants.");
  bool compileConstants = false;
};

DECLARE_REFLECTION_STRUCT(ConstantBlock);

DOCUMENT(R"(Contains the information for a separate sampler in a shader. If the API doesn't have
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
    return name == o.name && bindPoint == o.bindPoint;
  }
  bool operator<(const ShaderSampler &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(bindPoint == o.bindPoint))
      return bindPoint < o.bindPoint;
    return false;
  }
  DOCUMENT("The name of this sampler.");
  rdcstr name;

  DOCUMENT(R"(The bindpoint for this block. This is an index in either the
:data:`ShaderBindpointMapping.samplers` list.
)");
  int32_t bindPoint;
};

DECLARE_REFLECTION_STRUCT(ShaderSampler);

DOCUMENT(R"(Contains the information for a shader resource that is made accessible to shaders
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
    return resType == o.resType && name == o.name && variableType == o.variableType &&
           bindPoint == o.bindPoint && isTexture == o.isTexture && isReadOnly == o.isReadOnly;
  }
  bool operator<(const ShaderResource &o) const
  {
    if(!(resType == o.resType))
      return resType < o.resType;
    if(!(name == o.name))
      return name < o.name;
    if(!(variableType == o.variableType))
      return variableType < o.variableType;
    if(!(bindPoint == o.bindPoint))
      return bindPoint < o.bindPoint;
    if(!(isTexture == o.isTexture))
      return isTexture < o.isTexture;
    if(!(isReadOnly == o.isReadOnly))
      return isReadOnly < o.isReadOnly;
    return false;
  }
  DOCUMENT("The :class:`TextureType` that describes the type of this resource.");
  TextureType resType;

  DOCUMENT("The name of this resource.");
  rdcstr name;

  DOCUMENT(R"(The type of each element of this resource.

:type: ShaderConstantType
)");
  ShaderConstantType variableType;

  DOCUMENT(R"(The bindpoint for this block. This is an index in either the
:data:`ShaderBindpointMapping.readOnlyResources` list or
:data:`ShaderBindpointMapping.readWriteResources` list as appropriate (see :data:`isReadOnly`).
)");
  int32_t bindPoint;

  DOCUMENT("``True`` if this resource is a texture, otherwise it is a buffer.");
  bool isTexture;
  DOCUMENT(R"(``True`` if this resource is available to the shader for reading only, otherwise it is
able to be read from and written to arbitrarily.
)");
  bool isReadOnly;
};

DECLARE_REFLECTION_STRUCT(ShaderResource);

DOCUMENT("Describes an entry point in a shader.");
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
  DOCUMENT("The name of the entry point.");
  rdcstr name;

  DOCUMENT("The :class:`ShaderStage` for this entry point .");
  ShaderStage stage;
};

DECLARE_REFLECTION_STRUCT(ShaderEntryPoint);

DOCUMENT("Contains a single flag used at compile-time on a shader.");
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
  DOCUMENT("The name of the compile flag.");
  rdcstr name;

  DOCUMENT("The value of the compile flag.");
  rdcstr value;
};

DECLARE_REFLECTION_STRUCT(ShaderCompileFlag);

DOCUMENT("Contains the information about the compilation environment of a shader");
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

DOCUMENT("Contains the source prefix to add to a given type of shader source");
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
  DOCUMENT("The encoding of the language this prefix applies to.");
  ShaderEncoding encoding;

  DOCUMENT("The source prefix to add.");
  rdcstr prefix;
};

DECLARE_REFLECTION_STRUCT(ShaderSourcePrefix);

DOCUMENT("Contains a source file available in a debug-compiled shader.");
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
  DOCUMENT("The filename of this source file.");
  rdcstr filename;

  DOCUMENT("The actual contents of the file.");
  rdcstr contents;
};

DECLARE_REFLECTION_STRUCT(ShaderSourceFile);

DOCUMENT(R"(Contains the information about a shader contained within API-specific debugging
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
)");
  bool debuggable = true;

  DOCUMENT(R"(Indicates whether this shader has debug information to allow source-level debugging.
)");
  bool sourceDebugInformation = false;

  DOCUMENT(R"(If :data:`debuggable` is false then this contains a simple explanation of why the
shader is not supported for debugging
)");
  rdcstr debugStatus;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugInfo);

DOCUMENT(R"(The reflection and metadata fully describing a shader.

The information in this structure is API agnostic, and is matched up against a
:class:`ShaderBindpointMapping` instance to map the information here to the API's binding points
and resource binding scheme.
)");
struct ShaderReflection
{
  DOCUMENT("");
  ShaderReflection() = default;
  ShaderReflection(const ShaderReflection &) = default;
  ShaderReflection &operator=(const ShaderReflection &) = default;

  DOCUMENT("The :class:`ResourceId` of this shader.");
  ResourceId resourceId;

  DOCUMENT("The entry point in the shader for this reflection, if multiple entry points exist.");
  rdcstr entryPoint;

  DOCUMENT(
      "The :class:`ShaderStage` that this shader corresponds to, if multiple entry points exist.");
  ShaderStage stage;

  DOCUMENT(R"(The embedded debugging information.

:type: ShaderDebugInfo
)");
  ShaderDebugInfo debugInfo;

  DOCUMENT("The :class:`ShaderEncoding` of this shader. See :data:`rawBytes`.");
  ShaderEncoding encoding = ShaderEncoding::Unknown;

  DOCUMENT(R"(A raw ``bytes`` dump of the original shader, encoded in the form denoted by
:data:`encoding`.
)");
  bytebuf rawBytes;

  DOCUMENT(R"(The 3D dimensions of a compute workgroup, for compute shaders.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> dispatchThreadsDimension;

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
};

DECLARE_REFLECTION_STRUCT(ShaderReflection);

DOCUMENT(R"(Declares the binding information for a single resource binding.

See :class:`ShaderBindpointMapping` for how this mapping works in detail.
)");
struct Bindpoint
{
  DOCUMENT("");
  Bindpoint()
  {
    bindset = 0;
    bind = 0;
    used = false;
    arraySize = 1;
  }
  Bindpoint(const Bindpoint &) = default;
  Bindpoint &operator=(const Bindpoint &) = default;

  Bindpoint(int32_t s, int32_t b)
  {
    bindset = s;
    bind = b;
    used = false;
    arraySize = 1;
  }
  // construct out of an index, useful for searching bindpoint lists (since we only compare by
  // bindset and bind)
  Bindpoint(const BindpointIndex &idx)
  {
    bindset = idx.bindset;
    bind = idx.bind;
    used = false;
    arraySize = 1;
  }

  bool operator<(const Bindpoint &o) const
  {
    if(!(bindset == o.bindset))
      return bindset < o.bindset;
    return bind < o.bind;
  }
  bool operator==(const Bindpoint &o) const { return bindset == o.bindset && bind == o.bind; }
  DOCUMENT("The binding set.");
  int32_t bindset;
  DOCUMENT("The binding index.");
  int32_t bind;
  DOCUMENT("If this is an arrayed binding, the number of elements in the array.");
  uint32_t arraySize;
  DOCUMENT(
      "``True`` if the shader actually uses this resource, otherwise it's declared but unused.");
  bool used;
};

DECLARE_REFLECTION_STRUCT(Bindpoint);

DOCUMENT(R"(This structure goes hand in hand with :class:`ShaderReflection` to determine how to map
from bindpoint indices in the resource lists there to API-specific binding points. The ``bindPoint``
member in :class:`ShaderResource` or :class:`ConstantBlock` refers to an index in these associated
lists, which then map potentially sparsely and potentially in different orders to the appropriate
API registers, indices, or slots.

API specific details:

* Direct3D11 - All :data:`Bindpoint.bindset` values are 0 as D3D11 has no notion of sets, and the
  only namespacing that exists is by shader stage and object type. Mostly this already exists with
  the constant block, read only and read write resource lists.

  :data:`Bindpoint.arraySize` is likewise unused as D3D11 doesn't have arrayed resource bindings.

  :data:`Bindpoint.bind` refers to the register/slot binding within the appropriate type (SRVs for
  read-only resources, UAV for read-write resources, samplers/constant buffers in each type).

* OpenGL - Similarly to D3D11, :data:`Bindpoint.bindset` and :data:`Bindpoint.arraySize` are
  unused as OpenGL does not have true binding sets or array resource binds.

  For OpenGL there may be many more duplicate :class:`Bindpoint` objects as the
  :data:`Bindpoint.bind` refers to the index in the type-specific list, which is much more
  granular on OpenGL. E.g. ``0`` may refer to images, storage buffers, and atomic buffers all within
  the :data:`readWriteResources` list. The index is the uniform value of the binding. Since no
  objects are namespaced by shader stage, the same value in two shaders refers to the same binding.

* Direct3D12 - Since D3D12 doesn't have true resource arrays (they are linearised into sequential
  registers) :data:`Bindpoint.arraySize` is not used.

  :data:`Bindpoint.bindset` corresponds to register spaces, with :data:`Bindpoint.bind` then
  mapping to the register within that space. The root signature then maps these registers to
  descriptors.

* Vulkan - For Vulkan :data:`Bindpoint.bindset` corresponds to the index of the descriptor set,
  and :data:`Bindpoint.bind` refers to the index of the descriptor within that set.
  :data:`Bindpoint.arraySize` also is used as descriptors in Vulkan can be true arrays, bound all
  at once to a single binding.
)");
struct ShaderBindpointMapping
{
  DOCUMENT("");
  ShaderBindpointMapping() = default;
  ShaderBindpointMapping(const ShaderBindpointMapping &) = default;
  ShaderBindpointMapping &operator=(const ShaderBindpointMapping &) = default;

  DOCUMENT(R"(This maps input attributes as a simple swizzle on the
:data:`ShaderReflection.inputSignature` indices for APIs where this mapping is mutable at runtime.

:type: List[int]
)");
  rdcarray<int32_t> inputAttributes;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.constantBlocks` list.

:type: List[Bindpoint]
)");
  rdcarray<Bindpoint> constantBlocks;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.samplers` list.

:type: List[Bindpoint]
)");
  rdcarray<Bindpoint> samplers;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.readOnlyResources` list.

:type: List[Bindpoint]
)");
  rdcarray<Bindpoint> readOnlyResources;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.readWriteResources` list.

:type: List[Bindpoint]
)");
  rdcarray<Bindpoint> readWriteResources;
};

DECLARE_REFLECTION_STRUCT(ShaderBindpointMapping);
