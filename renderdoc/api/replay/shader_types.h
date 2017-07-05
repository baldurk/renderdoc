/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

typedef uint8_t byte;
typedef uint32_t bool32;

DOCUMENT("A ``float`` 4 component vector.")
struct FloatVecVal
{
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
};

DOCUMENT(R"(Holds a single named shader variable. It contains either a primitive type (up to a 4x4
matrix of a :class:`basic type <VarType>`) or a list of members, which can either be struct or array
members of this parent variable.

Matrices are always stored row-major. If necessary they are transposed when retrieving from the raw
data bytes when they are specified to be column-major in the API/shader metadata.
)");
struct ShaderVariable
{
  ShaderVariable()
  {
    name = "";
    rows = columns = 0;
    displayAsHex = isStruct = false;
    type = VarType::Float;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
  }
  ShaderVariable(const char *n, float x, float y, float z, float w)
  {
    name = n;
    rows = 1;
    columns = 4;
    displayAsHex = isStruct = false;
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
    displayAsHex = isStruct = false;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
    type = VarType::Int;
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
    displayAsHex = isStruct = false;
    for(int i = 0; i < 16; i++)
      value.uv[i] = 0;
    type = VarType::UInt;
    value.u.x = x;
    value.u.y = y;
    value.u.z = z;
    value.u.w = w;
  }

  DOCUMENT("The number of rows in this matrix.");
  uint32_t rows;
  DOCUMENT("The number of columns in this matrix.");
  uint32_t columns;
  DOCUMENT("The name of this variable.");
  rdctype::str name;

  DOCUMENT("The :class:`basic type <VarType>` of this variable.");
  VarType type;

  DOCUMENT("``True`` if the contents of this variable should be displayed as hex.");
  bool32 displayAsHex;

  DOCUMENT("The :class:`contents <ShaderValue>` of this variable if it has no members.");
  ShaderValue value;

  DOCUMENT("``True`` if this variable is a structure and not an array or basic type.");
  bool32 isStruct;

  DOCUMENT("The members of this variable as a list of :class:`ShaderValue`.");
  rdctype::array<ShaderVariable> members;
};

DECLARE_REFLECTION_STRUCT(ShaderVariable);

DOCUMENT(R"(This stores the current state of shader debugging at one particular step in the shader,
with all mutable variable contents.
)");
struct ShaderDebugState
{
  DOCUMENT("The temporary variables for this shader as a list of :class:`ShaderValue`.");
  rdctype::array<ShaderVariable> registers;
  DOCUMENT("The output variables for this shader as a list of :class:`ShaderValue`.");
  rdctype::array<ShaderVariable> outputs;

  DOCUMENT(
      "Indexable temporary variables for this shader as a list of :class:`ShaderValue` lists.");
  rdctype::array<rdctype::array<ShaderVariable> > indexableTemps;

  DOCUMENT(R"(The next instruction to be executed after this state. The initial state before any
shader execution happened will have ``nextInstruction == 0``.)");
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
  DOCUMENT("The input variables for this shader as a list of :class:`ShaderValue`.");
  rdctype::array<ShaderVariable> inputs;
  DOCUMENT(R"(Constant variables for this shader as a list of :class:`ShaderValue` lists.

Each entry in this list corresponds to a constant block with the same index in the
:data:`ShaderBindpointMapping.ConstantBlocks` list, which can be used to look up the metadata.
)");
  rdctype::array<rdctype::array<ShaderVariable> > cbuffers;

  DOCUMENT(R"(A list of :class:`ShaderDebugState` states representing the state after each
instruction was executed
)");
  rdctype::array<ShaderDebugState> states;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugTrace);

DOCUMENT(R"(The information describing an input or output signature element describing the interface
between shader stages.

.. data:: NoIndex

  Value for an index that means it is invalid or not applicable for this parameter.
)");
struct SigParameter
{
  SigParameter()
      : semanticIndex(0),
        needSemanticIndex(false),
        regIndex(0),
        systemValue(ShaderBuiltin::Undefined),
        compType(CompType::Float),
        regChannelMask(0),
        channelUsedMask(0),
        compCount(0),
        stream(0),
        arrayIndex(~0U)
  {
  }

  DOCUMENT("The name of this variable - may not be present in the metadata for all APIs.");
  rdctype::str varName;
  DOCUMENT("The semantic name of this variable, if the API uses semantic matching for bindings.");
  rdctype::str semanticName;
  DOCUMENT("The semantic index of this variable - see :data:`semanticName`.");
  uint32_t semanticIndex;
  DOCUMENT("The combined semantic name and index.");
  rdctype::str semanticIdxName;

  DOCUMENT("A convenience flag - ``True`` if the semantic name is unique and no index is needed.");
  bool32 needSemanticIndex;

  DOCUMENT(R"(The index of the shader register/binding used to store this signature element.

This may be :data:`NoIndex` if the element is system-generated and not consumed by another shader
stage. See :data:`systemValue`.
)");
  uint32_t regIndex;
  DOCUMENT("The :class:`ShaderBuiltin` value that this element contains.");
  ShaderBuiltin systemValue;

  DOCUMENT("The :class:`component type <CompType>` of data that this element stores.");
  CompType compType;

  DOCUMENT(R"(A bitmask indicating which components in the shader register are stored, for APIs that
pack signatures together.
)");
  uint8_t regChannelMask;
  DOCUMENT(R"(A bitmask indicating which components in the shader register are actually used by the
shader itself, for APIs that pack signatures together.
)");
  uint8_t channelUsedMask;

  DOCUMENT("The number of components used to store this element. See :data:`compType`.");
  uint32_t compCount;
  DOCUMENT(
      "Selects a stream for APIs that provide multiple output streams for the same named output.");
  uint32_t stream;

  DOCUMENT("If this element is part of an array, indicates the index, or :data:`NoIndex` if not.");
  uint32_t arrayIndex;

  static const uint32_t NoIndex = ~0U;
};

DECLARE_REFLECTION_STRUCT(SigParameter);

struct ShaderConstant;

DOCUMENT("Describes the storage characteristics for a basic :class:`ShaderConstant` in memory.");
struct ShaderVariableDescriptor
{
  DOCUMENT("The :class:`VarType` that this basic constant stores.");
  VarType type;
  DOCUMENT("The number of rows in this matrix.");
  uint32_t rows;
  DOCUMENT("The number of columns in this matrix.");
  uint32_t cols;
  DOCUMENT("The number of elements in the array, or 1 if it's not an array.");
  uint32_t elements;
  DOCUMENT("``True`` if the matrix is stored as row major instead of column major.");
  bool32 rowMajorStorage;
  DOCUMENT("The number of bytes between the start of one element in the array and the next.");
  uint32_t arrayStride;
  DOCUMENT("The name of the type of this constant, e.g. a ``struct`` name.");
  rdctype::str name;
};

DECLARE_REFLECTION_STRUCT(ShaderVariableDescriptor);

DOCUMENT("Describes the type and members of a :class:`ShaderConstant`.");
struct ShaderVariableType
{
  DOCUMENT("The :class:`ShaderVariableDescriptor` that describes the current constant.");
  ShaderVariableDescriptor descriptor;

  DOCUMENT("A list of :class:`ShaderConstant` with any members that this constant may contain.");
  rdctype::array<ShaderConstant> members;
};

DECLARE_REFLECTION_STRUCT(ShaderVariableType);

DOCUMENT("Describes the offset of a constant in memory in terms of 16 byte vectors.");
struct ShaderRegister
{
  DOCUMENT("The index of the 16 byte vector where this register begins");
  uint32_t vec;
  DOCUMENT("The 4 byte component within that vector where this register begins");
  uint32_t comp;
};

DECLARE_REFLECTION_STRUCT(ShaderRegister);

DOCUMENT("Contains the detail of a constant within a :class:`ConstantBlock` in memory.");
struct ShaderConstant
{
  DOCUMENT("The name of this constant");
  rdctype::str name;
  DOCUMENT(
      "A :class:`ShaderRegister` describing where this constant is offset from the start of the "
      "block");
  ShaderRegister reg;
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
  DOCUMENT("The name of this constant block, may be empty on some APIs.");
  rdctype::str name;
  DOCUMENT("The constants contained within this block as a list of :class:`ShaderConstant`.");
  rdctype::array<ShaderConstant> variables;
  DOCUMENT(R"(``True`` if the contents are stored in a buffer of memory. If not then they are set by
some other API-specific method, such as direct function calls or they may be compile-time
specialisation constants.
)");
  bool32 bufferBacked;
  DOCUMENT(R"(The bindpoint for this block. This is an index in the
:data:`ShaderBindpointMapping.ConstantBlocks` list.
)");
  int32_t bindPoint;
  DOCUMENT("The total number of bytes consumed by all of the constants contained in this block.");
  uint32_t byteSize;
};

DECLARE_REFLECTION_STRUCT(ConstantBlock);

DOCUMENT(R"(Contains the information for a shader resource that is made accessible to shaders
directly by means of the API resource binding system.

.. note:: that constant blocks will not have a shader resource entry, see :class:`ConstantBlock`.
)");
struct ShaderResource
{
  DOCUMENT(R"(``True`` if this resource is a sampler.

If the API has no concept of separate samplers, this will always be ``False``.

.. note:: this is not exclusive with the other flags in the case of e.g. combined sampler/texture
  objects.
)");
  bool32 IsSampler;
  DOCUMENT(R"(``True`` if this resource is a texture, otherwise it is a buffer or sampler (see
:data:`IsSampler`).
)");
  bool32 IsTexture;
  DOCUMENT(R"(``True`` if this resource is available to the shader for reading only, otherwise it is
able to be read from and written to arbitrarily.
)");
  bool32 IsReadOnly;

  DOCUMENT("The :class:`TextureDim` that describes the type of this resource.");
  TextureDim resType;

  DOCUMENT("The name of this resource.");
  rdctype::str name;

  DOCUMENT("A :class:`ShaderVariableType` describing type of each element of this resource.");
  ShaderVariableType variableType;

  DOCUMENT(R"(The bindpoint for this block. This is an index in either the
:data:`ShaderBindpointMapping.ReadOnlyResources` list or
:data:`ShaderBindpointMapping.ReadWriteResources` list as appropriate (see :data:`IsReadOnly`).
)");
  int32_t bindPoint;
};

DECLARE_REFLECTION_STRUCT(ShaderResource);

DOCUMENT(R"(Contains the information about a shader contained within API-specific debugging
information attached to the shader.

Primarily this means the embedded original source files.
)");
struct ShaderDebugChunk
{
  ShaderDebugChunk() : compileFlags(0) {}
  DOCUMENT("An API or compiler specific set of flags used to compile this shader originally.");
  uint32_t compileFlags;

  DOCUMENT(R"(A list of tuples, where each tuple is a pair of filename, source code.

The first entry in the list is always the file where the entry point is.
)");
  rdctype::array<rdctype::pair<rdctype::str, rdctype::str> > files;
};

DECLARE_REFLECTION_STRUCT(ShaderDebugChunk);

DOCUMENT(R"(The reflection and metadata fully describing a shader.

The information in this structure is API agnostic, and is matched up against a
:class:`ShaderBindpointMapping` instance to map the information here to the API's binding points
and resource binding scheme.
)");
struct ShaderReflection
{
  DOCUMENT("The :class:`ResourceId` of this shader.");
  ResourceId ID;

  DOCUMENT("The entry point in the shader for this reflection, if multiple entry points exist.");
  rdctype::str EntryPoint;

  DOCUMENT(
      "A :class:`ShaderDebugChunk` containing any embedded debugging information in this shader.");
  ShaderDebugChunk DebugInfo;

  DOCUMENT("A raw ``bytes`` dump of the original shader, encoded in API specific binary form.");
  rdctype::array<byte> RawBytes;

  DOCUMENT("The 3D dimensions of a compute workgroup, for compute shaders.");
  uint32_t DispatchThreadsDimension[3];

  DOCUMENT("A list of :class:`SigParameter` with the shader's input signature.");
  rdctype::array<SigParameter> InputSig;
  DOCUMENT("A list of :class:`SigParameter` with the shader's output signature.");
  rdctype::array<SigParameter> OutputSig;

  DOCUMENT("A list of :class:`ConstantBlock` with the shader's constant bindings.");
  rdctype::array<ConstantBlock> ConstantBlocks;

  DOCUMENT("A list of :class:`ShaderResource` with the shader's read-only resources.");
  rdctype::array<ShaderResource> ReadOnlyResources;
  DOCUMENT("A list of :class:`ShaderResource` with the shader's read-write resources.");
  rdctype::array<ShaderResource> ReadWriteResources;

  // TODO expand this to encompass shader subroutines.
  DOCUMENT("A list of strings with the shader's interfaces. Largely an unused API feature.");
  rdctype::array<rdctype::str> Interfaces;
};

DECLARE_REFLECTION_STRUCT(ShaderReflection);

DOCUMENT(R"(Declares the binding information for a single resource binding.

See :class:`ShaderBindpointMapping` for how this mapping works in detail.
)");
struct BindpointMap
{
  BindpointMap()
  {
    bindset = 0;
    bind = 0;
    used = false;
    arraySize = 1;
  }

  BindpointMap(int32_t s, int32_t b)
  {
    bindset = s;
    bind = b;
    used = false;
    arraySize = 1;
  }

  bool operator<(const BindpointMap &o) const
  {
    if(bindset != o.bindset)
      return bindset < o.bindset;
    return bind < o.bind;
  }

  DOCUMENT("The binding set.");
  int32_t bindset;
  DOCUMENT("The binding index.");
  int32_t bind;
  DOCUMENT(
      "``True`` if the shader actually uses this resource, otherwise it's declared but unused.");
  bool32 used;
  DOCUMENT("If this is an arrayed binding, the number of elements in the array.");
  uint32_t arraySize;
};

DECLARE_REFLECTION_STRUCT(BindpointMap);

DOCUMENT(R"(This structure goes hand in hand with :class:`ShaderReflection` to determine how to map
from bindpoint indices in the resource lists there to API-specific binding points. The ``bindPoint``
member in :class:`ShaderResource` or :class:`ConstantBlock` refers to an index in these associated
lists, which then map potentially sparsely and potentially in different orders to the appropriate
API registers, indices, or slots.

API specific details:

* Direct3D11 - All :data:`BindpointMap.bindset` values are 0 as D3D11 has no notion of sets, and the
  only namespacing that exists is by shader stage and object type. Mostly this already exists with
  the constant block, read only and read write resource lists.

  :data:`BindpointMap.arraySize` is likewise unused as D3D11 doesn't have arrayed resource bindings.

  The :data:`BindpointMap.bind` value corresponds directly to the index in the appropriate resource
  list.

  One important thing to note is that samplers are included with read only resources. This means
  consumers wanting to map to API bindpoints should know and expect that the
  :data:`ReadOnlyResources` list contains potentially duplicate :class:`BindpointMap`, with one
  being a SRV and one a sampler.

  Note that D3D11 currently uses an identity bindpoint mapping, such that the index in the bindpoint
  array is equal to the register, even if it's sparse. E.g. textures ``0`` and ``4`` will be in
  bindpoint maps ``0`` and ``4`` with three empty unused maps in ``1``, ``2``, and ``3``. This is
  not contractual and should not be relied upon, in future the bindpoint map may be only two
  elements that list ``0`` and ``4``, with the shader bindpoints then being ``0`` and ``1``.

* OpenGL - Similarly to D3D11, :data:`BindpointMap.bindset` and :data:`BindpointMap.arraySize` are
  unused as OpenGL does not have true binding sets or array resource binds.

  For OpenGL there may be many more duplicate :class:`BindpointMap` objects as the
  :data:`BindpointMap.bind` refers to the index in the type-specific list, which is much more
  granular on OpenGL. E.g. ``0`` may refer to images, storage buffers, and atomic buffers all within
  the :data:`ReadWriteResources` list. The index is the uniform value of the binding. Since no
  objects are namespaced by shader stage, the same value in two shaders refers to the same binding.

* Direct3D12 - As with 11 above, samplers are included in the read only resources array. Likewise
  since D3D12 doesn't have true resource arrays (they are linearised into sequential registers)
  :data:`BindpointMap.arraySize` is not used.

  :data:`BindpointMap.bindset` corresponds to register spaces, with :data:`BindpointMap.bind` then
  mapping to the register within that space. The root signature then maps these registers to
  descriptors.

* Vulkan - For Vulkan :data:`BindpointMap.bindset` corresponds to the index of the descriptor set,
  and :data:`BindpointMap.bind` refers to the index of the descriptor within that set.
  :data:`BindpointMap.arraySize` also is used as descriptors in Vulkan can be true arrays, bound all
  at once to a single binding.
)");
struct ShaderBindpointMapping
{
  DOCUMENT(R"(This maps input attributes as a simple swizzle on the
:data:`ShaderReflection.InputSig` indices for APIs where this mapping is mutable at runtime.
)");
  rdctype::array<int> InputAttributes;

  DOCUMENT(R"(Provides a list of :class:`BindpointMap` entries for remapping the
:data:`ShaderReflection.ConstantBlocks` list.
)");
  rdctype::array<BindpointMap> ConstantBlocks;

  DOCUMENT(R"(Provides a list of :class:`BindpointMap` entries for remapping the
:data:`ShaderReflection.ReadOnlyResources` list.
)");
  rdctype::array<BindpointMap> ReadOnlyResources;

  DOCUMENT(R"(Provides a list of :class:`BindpointMap` entries for remapping the
:data:`ShaderReflection.ReadWriteResources` list.
)");
  rdctype::array<BindpointMap> ReadWriteResources;
};

DECLARE_REFLECTION_STRUCT(ShaderBindpointMapping);
