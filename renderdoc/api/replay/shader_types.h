/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include "basic_types.h"
#include "replay_enums.h"

DOCUMENT("A ``float`` 4 component vector.")
struct FloatVecVal
{
  DOCUMENT("");
  FloatVecVal() = default;
  FloatVecVal(const FloatVecVal &) = default;

  DOCUMENT("The x component.");
  float x;
  DOCUMENT("The y component.");
  float y;
  DOCUMENT("The z component.");
  float z;
  DOCUMENT("The w component.");
  float w;
};

DOCUMENT("A ``double`` 4 component vector.")
struct DoubleVecVal
{
  DOCUMENT("");
  DoubleVecVal() = default;
  DoubleVecVal(const DoubleVecVal &) = default;

  DOCUMENT("The x component.");
  double x;
  DOCUMENT("The y component.");
  double y;
  DOCUMENT("The z component.");
  double z;
  DOCUMENT("The w component.");
  double w;
};

DOCUMENT("A 32-bit signed ``int`` 4 component vector.")
struct IntVecVal
{
  DOCUMENT("");
  IntVecVal() = default;
  IntVecVal(const IntVecVal &) = default;

  DOCUMENT("The x component.");
  int32_t x;
  DOCUMENT("The y component.");
  int32_t y;
  DOCUMENT("The z component.");
  int32_t z;
  DOCUMENT("The w component.");
  int32_t w;
};

DOCUMENT("A 32-bit unsigned ``int`` 4 component vector.")
struct UIntVecVal
{
  DOCUMENT("");
  UIntVecVal() = default;
  UIntVecVal(const UIntVecVal &) = default;

  DOCUMENT("The x component.");
  uint32_t x;
  DOCUMENT("The y component.");
  uint32_t y;
  DOCUMENT("The z component.");
  uint32_t z;
  DOCUMENT("The w component.");
  uint32_t w;
};

DOCUMENT("A C union that holds 16 values, with each different basic variable type.");
union ShaderValue
{
  DOCUMENT("A convenient subset of :data:`fv` as a named 4 component vector.");
  FloatVecVal f;
  DOCUMENT("``float`` values.");
  float fv[16];

  DOCUMENT("A convenient subset of :data:`iv` as a named 4 component vector.");
  IntVecVal i;
  DOCUMENT("Signed integer values.");
  int32_t iv[16];

  DOCUMENT("A convenient subset of :data:`uv` as a named 4 component vector.");
  UIntVecVal u;
  DOCUMENT("Unsigned integer values.");
  uint32_t uv[16];

  DOCUMENT("A convenient subset of :data:`dv` as a named 4 component vector.");
  DoubleVecVal d;
  DOCUMENT("``double`` values.");
  double dv[16];

  DOCUMENT("64-bit unsigned integer values.");
  uint64_t u64v[16];

  DOCUMENT("64-bit signed integer values.");
  int64_t s64v[16];
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
    name = "";
    rows = columns = 0;
    displayAsHex = isStruct = rowMajor = false;
    type = VarType::Float;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
  }
  ShaderVariable(const ShaderVariable &) = default;
  ShaderVariable(const char *n, float x, float y, float z, float w)
  {
    name = n;
    rows = 1;
    columns = 4;
    displayAsHex = isStruct = rowMajor = false;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
    type = VarType::Float;
    value.f.x = x;
    value.f.y = y;
    value.f.z = z;
    value.f.w = w;
  }
  ShaderVariable(const char *n, int x, int y, int z, int w)
  {
    name = n;
    rows = 1;
    columns = 4;
    displayAsHex = isStruct = rowMajor = false;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
    type = VarType::SInt;
    value.i.x = x;
    value.i.y = y;
    value.i.z = z;
    value.i.w = w;
  }
  ShaderVariable(const char *n, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
  {
    name = n;
    rows = 1;
    columns = 4;
    displayAsHex = isStruct = rowMajor = false;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
    type = VarType::UInt;
    value.u.x = x;
    value.u.y = y;
    value.u.z = z;
    value.u.w = w;
  }
  bool operator==(const ShaderVariable &o) const
  {
    return rows == o.rows && columns == o.columns && name == o.name && type == o.type &&
           displayAsHex == o.displayAsHex && !memcmp(&value, &o.value, sizeof(value)) &&
           isStruct == o.isStruct && members == o.members;
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
    if(!(displayAsHex == o.displayAsHex))
      return displayAsHex < o.displayAsHex;
    if(!(isStruct == o.isStruct))
      return isStruct < o.isStruct;
    if(!(rowMajor == o.rowMajor))
      return rowMajor < o.rowMajor;
    if(memcmp(&value, &o.value, sizeof(value)) < 0)
      return true;
    if(!(members == o.members))
      return members < o.members;
    return false;
  }

  DOCUMENT("The number of rows in this matrix.");
  uint32_t rows;
  DOCUMENT("The number of columns in this matrix.");
  uint32_t columns;
  DOCUMENT("The name of this variable.");
  rdcstr name;

  DOCUMENT("The :class:`basic type <VarType>` of this variable.");
  VarType type;

  DOCUMENT("The :class:`contents <ShaderValue>` of this variable if it has no members.");
  ShaderValue value;

  DOCUMENT("``True`` if the contents of this variable should be displayed as hex.");
  bool displayAsHex;

  DOCUMENT("``True`` if this variable is a structure and not an array or basic type.");
  bool isStruct;

  DOCUMENT("``True`` if this variable is stored in rows in memory. Only relevant for matrices.");
  bool rowMajor;

  DOCUMENT("The members of this variable as a list of :class:`ShaderVariable`.");
  rdcarray<ShaderVariable> members;
};

DECLARE_REFLECTION_STRUCT(ShaderVariable);

DOCUMENT(
    "A particular component of a variable register that a high-level variable component maps to");
struct RegisterRange
{
  DOCUMENT("");
  RegisterRange() = default;
  RegisterRange(const RegisterRange &) = default;

  bool operator==(const RegisterRange &o) const
  {
    return type == o.type && index == o.index && component == o.component;
  }
  bool operator<(const RegisterRange &o) const
  {
    if(!(type == o.type))
      return type < o.type;
    if(!(index == o.index))
      return index < o.index;
    if(!(component == o.component))
      return component < o.component;
    return false;
  }

  DOCUMENT("The :class:`RegisterType` of the register being mapped to.");
  RegisterType type = RegisterType::Undefined;

  DOCUMENT("The index of the register within its type.");
  uint16_t index = 0xFFFF;

  DOCUMENT("The component of the register.");
  uint16_t component = 0;
};

DECLARE_REFLECTION_STRUCT(RegisterRange);

DOCUMENT(R"(Maps the contents of a high-level local variable to one or more shader variables in a
:class:`ShaderDebugState`, with type information.

A single high-level variable may be represented by multiple mappings but only along regular
boundaries, typically whole vectors. For example an array may have each element in a different
mapping, or a matrix may have a mapping per row. The properties such as :data:`rows` and
:data:`elements` reflect the *parent* object.

Since locals don't always map directly this can change over time.
)");
struct LocalVariableMapping
{
  DOCUMENT("");
  LocalVariableMapping() = default;
  LocalVariableMapping(const LocalVariableMapping &) = default;

  bool operator==(const LocalVariableMapping &o) const
  {
    return localName == o.localName && type == o.type && builtin == o.builtin && rows == o.rows &&
           columns == o.columns && elements == o.elements && registers == o.registers;
  }
  bool operator<(const LocalVariableMapping &o) const
  {
    if(!(localName == o.localName))
      return localName < o.localName;
    if(!(type == o.type))
      return type < o.type;
    if(!(builtin == o.builtin))
      return builtin < o.builtin;
    if(!(rows == o.rows))
      return rows < o.rows;
    if(!(columns == o.columns))
      return columns < o.columns;
    if(!(elements == o.elements))
      return elements < o.elements;
    if(!(registers == o.registers))
      return registers < o.registers;
    return false;
  }
  DOCUMENT("The name and member of this local variable that's being mapped from.");
  rdcstr localName;

  DOCUMENT("The variable type of the local being mapped from, if the register is untyped.");
  VarType type = VarType::Unknown;

  DOCUMENT("The shader builtin this variable corresponds to.");
  ShaderBuiltin builtin = ShaderBuiltin::Undefined;

  DOCUMENT("The number of rows in this variable - 1 for vectors, >1 for matrices.");
  uint32_t rows;

  DOCUMENT("The number of columns in this variable.");
  uint32_t columns;

  DOCUMENT("The number of array elements in this variable.");
  uint32_t elements;

  DOCUMENT("The number of valid entries in :data:`registers`.");
  uint32_t regCount;

  DOCUMENT(R"(The registers that the components of this variable map to. Multiple ranges could refer
to the same register if a contiguous range is mapped to - the mapping is component-by-component to
greatly simplify algorithms at the expense of a small amount of storage space.
)");
  RegisterRange registers[16];
};
DECLARE_REFLECTION_STRUCT(LocalVariableMapping);

DOCUMENT("Details the current region of code that an instruction maps to");
struct LineColumnInfo
{
  DOCUMENT("");
  LineColumnInfo() = default;
  LineColumnInfo(const LineColumnInfo &) = default;

  bool operator==(const LineColumnInfo &o) const
  {
    return fileIndex == o.fileIndex && lineStart == o.lineStart && lineEnd == o.lineEnd &&
           colStart == o.colStart && colEnd == o.colEnd && callstack == o.callstack;
  }
  bool operator<(const LineColumnInfo &o) const
  {
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
    if(!(callstack == o.callstack))
      return callstack < o.callstack;
    return false;
  }

  DOCUMENT("The current file, as an index into the list of files for this shader.");
  int32_t fileIndex = -1;

  DOCUMENT("The line-number (starting from 1) of the start of the current section of code.");
  uint32_t lineStart = 0;

  DOCUMENT("The line-number (starting from 1) of the end of the current section of code.");
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

  DOCUMENT(R"(A ``list`` of ``str`` with each function call in the current callstack at this line.

The oldest/outer function is first in the list, the newest/inner function is last.
)");
  rdcarray<rdcstr> callstack;
};
DECLARE_REFLECTION_STRUCT(LineColumnInfo);

DOCUMENT(R"(This stores the current state of shader debugging at one particular step in the shader,
with all mutable variable contents.
)");
struct ShaderDebugState
{
  DOCUMENT("");
  ShaderDebugState() = default;
  ShaderDebugState(const ShaderDebugState &) = default;

  bool operator==(const ShaderDebugState &o) const
  {
    return registers == o.registers && outputs == o.outputs && indexableTemps == o.indexableTemps &&
           locals == o.locals && nextInstruction == o.nextInstruction && flags == o.flags;
  }
  bool operator<(const ShaderDebugState &o) const
  {
    if(!(registers == o.registers))
      return registers < o.registers;
    if(!(outputs == o.outputs))
      return outputs < o.outputs;
    if(!(indexableTemps == o.indexableTemps))
      return indexableTemps < o.indexableTemps;
    if(!(locals == o.locals))
      return locals < o.locals;
    if(!(nextInstruction == o.nextInstruction))
      return nextInstruction < o.nextInstruction;
    if(!(flags == o.flags))
      return flags < o.flags;
    return false;
  }
  DOCUMENT("The temporary variables for this shader as a list of :class:`ShaderVariable`.");
  rdcarray<ShaderVariable> registers;
  DOCUMENT("The output variables for this shader as a list of :class:`ShaderVariable`.");
  rdcarray<ShaderVariable> outputs;

  DOCUMENT("Indexable temporary variables for this shader as a list of :class:`ShaderVariable`.");
  rdcarray<ShaderVariable> indexableTemps;

  DOCUMENT(R"(An optional list of :class:`ShaderVariableRef` indicating which high-level locals map
to which registers, and their type
)");
  rdcarray<LocalVariableMapping> locals;

  DOCUMENT("A list of registers that were modified.");
  rdcarray<RegisterRange> modified;

  DOCUMENT(R"(The next instruction to be executed after this state. The initial state before any
shader execution happened will have ``nextInstruction == 0``.
)");
  uint32_t nextInstruction;

  DOCUMENT("A set of :class:`ShaderEvents` flags that indicate what events happened on this step.");
  ShaderEvents flags;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugState);

DOCUMENT(R"(This stores the whole state of a shader's execution from start to finish, with each
individual debugging step along the way, as well as the immutable global constant values that do not
change with shader execution.
)");
struct ShaderDebugTrace
{
  DOCUMENT("");
  ShaderDebugTrace() = default;
  ShaderDebugTrace(const ShaderDebugTrace &) = default;

  DOCUMENT("The input variables for this shader as a list of :class:`ShaderValue`.");
  rdcarray<ShaderVariable> inputs;
  DOCUMENT(R"(Constant variables for this shader as a list of :class:`ShaderValue` lists.

Each entry in this list corresponds to a constant block with the same index in the
:data:`ShaderBindpointMapping.constantBlocks` list, which can be used to look up the metadata.
)");
  rdcarray<ShaderVariable> constantBlocks;

  DOCUMENT(R"(A list of :class:`ShaderDebugState` states representing the state after each
instruction was executed
)");
  rdcarray<ShaderDebugState> states;

  DOCUMENT("A flag indicating whether this trace has locals information");
  bool hasLocals = false;

  DOCUMENT(R"(A ``list`` of :class:`LineColumnInfo` detailing which source lines each instruction
corresponds to
)");
  rdcarray<LineColumnInfo> lineInfo;
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

  bool operator==(const SigParameter &o) const
  {
    return varName == o.varName && semanticName == o.semanticName &&
           semanticIdxName == o.semanticIdxName && semanticIndex == o.semanticIndex &&
           regIndex == o.regIndex && systemValue == o.systemValue && compType == o.compType &&
           regChannelMask == o.regChannelMask && channelUsedMask == o.channelUsedMask &&
           needSemanticIndex == o.needSemanticIndex && compCount == o.compCount &&
           stream == o.stream && arrayIndex == o.arrayIndex;
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
    if(!(compType == o.compType))
      return compType < o.compType;
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
    if(!(arrayIndex == o.arrayIndex))
      return arrayIndex < o.arrayIndex;
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

  DOCUMENT("The :class:`component type <CompType>` of data that this element stores.");
  CompType compType = CompType::Float;

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

  DOCUMENT("If this element is part of an array, indicates the index, or :data:`NoIndex` if not.");
  uint32_t arrayIndex = ~0U;

  static const uint32_t NoIndex = ~0U;
};

DECLARE_REFLECTION_STRUCT(SigParameter);

struct ShaderConstant;

DOCUMENT("Describes the storage characteristics for a basic :class:`ShaderConstant` in memory.");
struct ShaderVariableDescriptor
{
  DOCUMENT("");
  ShaderVariableDescriptor() = default;
  ShaderVariableDescriptor(const ShaderVariableDescriptor &) = default;

  bool operator==(const ShaderVariableDescriptor &o) const
  {
    return type == o.type && rows == o.rows && columns == o.columns &&
           rowMajorStorage == o.rowMajorStorage && elements == o.elements &&
           arrayByteStride == o.arrayByteStride && matrixByteStride == o.matrixByteStride &&
           name == o.name;
  }
  bool operator<(const ShaderVariableDescriptor &o) const
  {
    if(!(type == o.type))
      return type < o.type;
    if(!(rows == o.rows))
      return rows < o.rows;
    if(!(columns == o.columns))
      return columns < o.columns;
    if(!(rowMajorStorage == o.rowMajorStorage))
      return rowMajorStorage < o.rowMajorStorage;
    if(!(elements == o.elements))
      return elements < o.elements;
    if(!(arrayByteStride == o.arrayByteStride))
      return arrayByteStride < o.arrayByteStride;
    if(!(matrixByteStride == o.matrixByteStride))
      return matrixByteStride < o.matrixByteStride;
    if(!(name == o.name))
      return name < o.name;
    return false;
  }
  DOCUMENT("The :class:`VarType` that this basic constant stores.");
  VarType type = VarType::Unknown;
  DOCUMENT("The number of rows in this matrix.");
  uint8_t rows = 1;
  DOCUMENT("The number of columns in this matrix.");
  uint8_t columns = 1;
  DOCUMENT("The number of bytes between the start of one column/row in a matrix and the next.");
  uint8_t matrixByteStride = 0;
  DOCUMENT("``True`` if the matrix is stored as row major instead of column major.");
  bool rowMajorStorage = false;
  DOCUMENT("The number of elements in the array, or 1 if it's not an array.");
  uint32_t elements = 1;
  DOCUMENT("The number of bytes between the start of one element in the array and the next.");
  uint32_t arrayByteStride = 0;
  DOCUMENT("The name of the type of this constant, e.g. a ``struct`` name.");
  rdcstr name;
};

DECLARE_REFLECTION_STRUCT(ShaderVariableDescriptor);

DOCUMENT("Describes the type and members of a :class:`ShaderConstant`.");
struct ShaderVariableType
{
  DOCUMENT("");
  ShaderVariableType() = default;
  ShaderVariableType(const ShaderVariableType &) = default;

  bool operator==(const ShaderVariableType &o) const
  {
    return descriptor == o.descriptor && members == o.members;
  }
  bool operator<(const ShaderVariableType &o) const
  {
    if(!(descriptor == o.descriptor))
      return descriptor < o.descriptor;
    if(!(members == o.members))
      return members < o.members;
    return false;
  }
  DOCUMENT("The :class:`ShaderVariableDescriptor` that describes the current constant.");
  ShaderVariableDescriptor descriptor;

  DOCUMENT("A list of :class:`ShaderConstant` with any members that this constant may contain.");
  rdcarray<ShaderConstant> members;
};

DECLARE_REFLECTION_STRUCT(ShaderVariableType);

DOCUMENT("Contains the detail of a constant within a :class:`ConstantBlock` in memory.");
struct ShaderConstant
{
  DOCUMENT("");
  ShaderConstant() = default;
  ShaderConstant(const ShaderConstant &) = default;

  bool operator==(const ShaderConstant &o) const
  {
    return name == o.name && byteOffset == o.byteOffset && defaultValue == o.defaultValue &&
           type == o.type;
  }
  bool operator<(const ShaderConstant &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(defaultValue == o.defaultValue))
      return defaultValue < o.defaultValue;
    if(!(type == o.type))
      return type < o.type;
    return false;
  }
  DOCUMENT("The name of this constant");
  rdcstr name;
  DOCUMENT("The byte offset of this constant relative to the start of the block");
  uint32_t byteOffset;
  DOCUMENT("If this constant is no larger than a 64-bit constant, gives a default value for it.");
  uint64_t defaultValue;
  DOCUMENT(
      "A :class:`ShaderVariableType` giving details of the type information for this constant.");
  ShaderVariableType type;
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

  bool operator==(const ConstantBlock &o) const
  {
    return name == o.name && variables == o.variables && bindPoint == o.bindPoint &&
           byteSize == o.byteSize && bufferBacked == o.bufferBacked;
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
    return false;
  }
  DOCUMENT("The name of this constant block, may be empty on some APIs.");
  rdcstr name;
  DOCUMENT("The constants contained within this block as a list of :class:`ShaderConstant`.");
  rdcarray<ShaderConstant> variables;
  DOCUMENT(R"(The bindpoint for this block. This is an index in the
:data:`ShaderBindpointMapping.constantBlocks` list.
)");
  int32_t bindPoint;
  DOCUMENT("The total number of bytes consumed by all of the constants contained in this block.");
  uint32_t byteSize;
  DOCUMENT(R"(``True`` if the contents are stored in a buffer of memory. If not then they are set by
some other API-specific method, such as direct function calls or they may be compile-time
specialisation constants.
)");
  bool bufferBacked;
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

  DOCUMENT("A :class:`ShaderVariableType` describing type of each element of this resource.");
  ShaderVariableType variableType;

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

  DOCUMENT(R"(A list of :class:`ShaderCompileFlag`.

Each entry is an API or compiler specific flag used to compile this shader originally.
)");
  rdcarray<ShaderCompileFlag> flags;
};

DECLARE_REFLECTION_STRUCT(ShaderCompileFlags);

DOCUMENT("Contains a source file available in a debug-compiled shader.");
struct ShaderSourceFile
{
  DOCUMENT("");
  ShaderSourceFile() = default;
  ShaderSourceFile(const ShaderSourceFile &) = default;

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

  DOCUMENT("A :class:`ShaderCompileFlags` containing the flags used to compile this shader.");
  ShaderCompileFlags compileFlags;

  DOCUMENT(R"(A list of :class:`ShaderSourceFile`, encoded in the form denoted by :data:`encoding`.

The first entry in the list is always the file where the entry point is.
)");
  rdcarray<ShaderSourceFile> files;

  DOCUMENT("The :class:`ShaderEncoding` of the source. See :data:`files`.");
  ShaderEncoding encoding = ShaderEncoding::Unknown;
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

  DOCUMENT("The :class:`ResourceId` of this shader.");
  ResourceId resourceId;

  DOCUMENT("The entry point in the shader for this reflection, if multiple entry points exist.");
  rdcstr entryPoint;

  DOCUMENT(
      "The :class:`ShaderStage` that this shader corresponds to, if multiple entry points exist.");
  ShaderStage stage;

  DOCUMENT(
      "A :class:`ShaderDebugInfo` containing any embedded debugging information in this shader.");
  ShaderDebugInfo debugInfo;

  DOCUMENT("The :class:`ShaderEncoding` of this shader. See :data:`rawBytes`.");
  ShaderEncoding encoding = ShaderEncoding::Unknown;

  DOCUMENT(R"(A raw ``bytes`` dump of the original shader, encoded in the form denoted by
:data:`encoding`.
)");
  bytebuf rawBytes;

  DOCUMENT("The 3D dimensions of a compute workgroup, for compute shaders.");
  uint32_t dispatchThreadsDimension[3];

  DOCUMENT("A list of :class:`SigParameter` with the shader's input signature.");
  rdcarray<SigParameter> inputSignature;
  DOCUMENT("A list of :class:`SigParameter` with the shader's output signature.");
  rdcarray<SigParameter> outputSignature;

  DOCUMENT("A list of :class:`ConstantBlock` with the shader's constant bindings.");
  rdcarray<ConstantBlock> constantBlocks;

  DOCUMENT("A list of :class:`ShaderSampler` with the shader's samplers.");
  rdcarray<ShaderSampler> samplers;

  DOCUMENT("A list of :class:`ShaderResource` with the shader's read-only resources.");
  rdcarray<ShaderResource> readOnlyResources;
  DOCUMENT("A list of :class:`ShaderResource` with the shader's read-write resources.");
  rdcarray<ShaderResource> readWriteResources;

  // TODO expand this to encompass shader subroutines.
  DOCUMENT("A list of strings with the shader's interfaces. Largely an unused API feature.");
  rdcarray<rdcstr> interfaces;
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

  Bindpoint(int32_t s, int32_t b)
  {
    bindset = s;
    bind = b;
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

  DOCUMENT(R"(This maps input attributes as a simple swizzle on the
:data:`ShaderReflection.inputSignature` indices for APIs where this mapping is mutable at runtime.
)");
  rdcarray<int> inputAttributes;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.constantBlocks` list.
)");
  rdcarray<Bindpoint> constantBlocks;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.samplers` list.
)");
  rdcarray<Bindpoint> samplers;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.readOnlyResources` list.
)");
  rdcarray<Bindpoint> readOnlyResources;

  DOCUMENT(R"(Provides a list of :class:`Bindpoint` entries for remapping the
:data:`ShaderReflection.readWriteResources` list.
)");
  rdcarray<Bindpoint> readWriteResources;
};

DECLARE_REFLECTION_STRUCT(ShaderBindpointMapping);
