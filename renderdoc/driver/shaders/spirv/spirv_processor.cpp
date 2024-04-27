/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "spirv_processor.h"
#include "common/formatting.h"
#include "maths/half_convert.h"
#include "spirv_op_helpers.h"
#include "var_dispatch_helpers.h"

namespace rdcspv
{
static void ConstructCompositeConstant(ShaderVariable &v, const rdcarray<ShaderVariable> &members)
{
  if(v.rows == 0 && v.columns == 0)
  {
    v.members.resize(members.size());
    for(size_t i = 0; i < v.members.size(); i++)
      v.members[i] = members[i];
  }
  else
  {
    size_t sz = VarTypeByteSize(v.type);
    for(uint32_t c = 0; c < v.columns; c++)
    {
      // if this is a vector, v.rows is 1 so r is always 0, meaning that it is irrelevant
      for(uint32_t r = 0; r < v.rows; r++)
      {
        if(sz == 8)
          v.value.u64v[r * v.columns + c] = members[c].value.u64v[r];
        else if(sz == 4)
          v.value.u32v[r * v.columns + c] = members[c].value.u32v[r];
        else if(sz == 2)
          v.value.u16v[r * v.columns + c] = members[c].value.u16v[r];
        else
          v.value.u8v[r * v.columns + c] = members[c].value.u8v[r];
      }
    }
  }
}

Scalar scalar(VarType type)
{
  switch(type)
  {
    case VarType::Float: return scalar<float>();
    case VarType::Double: return scalar<double>();
    case VarType::Half: return scalar<half_float::half>();
    case VarType::SInt: return scalar<int32_t>();
    case VarType::UInt: return scalar<uint32_t>();
    case VarType::SShort: return scalar<int16_t>();
    case VarType::UShort: return scalar<uint16_t>();
    case VarType::SLong: return scalar<int64_t>();
    case VarType::ULong: return scalar<uint64_t>();
    case VarType::SByte: return scalar<int8_t>();
    case VarType::UByte: return scalar<uint8_t>();
    case VarType::Bool: return scalar<bool>();
    default: RDCERR("No scalar type for %s", ToStr(type).c_str()); return scalar<void>();
  }
}

void ExecutionModes::Register(const OpExecutionMode &mode)
{
  if(mode.mode == ExecutionMode::LocalSize)
  {
    localSize.x = mode.mode.localSize.xsize;
    localSize.y = mode.mode.localSize.ysize;
    localSize.z = mode.mode.localSize.zsize;
  }
  else if(mode.mode == ExecutionMode::Triangles)
  {
    outTopo = Topology::TriangleList;
  }
  else if(mode.mode == ExecutionMode::Isolines)
  {
    outTopo = Topology::LineList;
  }
  else if(mode.mode == ExecutionMode::OutputPoints)
  {
    outTopo = Topology::PointList;
  }
  else if(mode.mode == ExecutionMode::OutputLineStrip)
  {
    outTopo = Topology::LineStrip;
  }
  else if(mode.mode == ExecutionMode::OutputTriangleStrip)
  {
    outTopo = Topology::TriangleStrip;
  }
  else if(mode.mode == ExecutionMode::Quads)
  {
    outTopo = Topology::TriangleList;
  }
  else if(mode.mode == ExecutionMode::DepthGreater)
  {
    depthMode = DepthGreater;
  }
  else if(mode.mode == ExecutionMode::DepthLess)
  {
    depthMode = DepthLess;
  }
  else
  {
    others.push_back(mode.mode);
  }
}

void ExecutionModes::Register(const OpExecutionModeId &mode)
{
  if(mode.mode == ExecutionMode::LocalSizeId)
  {
    localSizeId.x = mode.mode.localSizeId.xsize;
    localSizeId.y = mode.mode.localSizeId.ysize;
    localSizeId.z = mode.mode.localSizeId.zsize;
  }
  else
  {
    others.push_back(mode.mode);
  }
}

void ExecutionModes::Unregister(const OpExecutionMode &mode)
{
  if(mode.mode == ExecutionMode::LocalSize)
  {
    localSize = {};
  }
  else if(mode.mode == ExecutionMode::Triangles)
  {
    outTopo = Topology::Unknown;
  }
  else if(mode.mode == ExecutionMode::Isolines)
  {
    outTopo = Topology::Unknown;
  }
  else if(mode.mode == ExecutionMode::OutputPoints)
  {
    outTopo = Topology::Unknown;
  }
  else if(mode.mode == ExecutionMode::OutputLineStrip)
  {
    outTopo = Topology::Unknown;
  }
  else if(mode.mode == ExecutionMode::OutputTriangleStrip)
  {
    outTopo = Topology::Unknown;
  }
  else if(mode.mode == ExecutionMode::Quads)
  {
    outTopo = Topology::Unknown;
  }
  else if(mode.mode == ExecutionMode::DepthGreater)
  {
    depthMode = DepthNormal;
  }
  else if(mode.mode == ExecutionMode::DepthLess)
  {
    depthMode = DepthNormal;
  }
  else
  {
    ExecutionMode val = mode.mode.value;
    others.removeOneIf([val](const ExecutionModeAndParamData &m) { return m.value == val; });
  }
}

void ExecutionModes::Unregister(const OpExecutionModeId &mode)
{
  if(mode.mode == ExecutionMode::LocalSizeId)
  {
    localSizeId = {};
  }
  else
  {
    ExecutionMode val = mode.mode.value;
    others.removeOneIf([val](const ExecutionModeAndParamData &m) { return m.value == val; });
  }
}

void Decorations::Register(const DecorationAndParamData &decoration)
{
  if(decoration == Decoration::Block)
  {
    flags = Flags(flags | Block);
  }
  else if(decoration == Decoration::BufferBlock)
  {
    flags = Flags(flags | BufferBlock);
  }
  else if(decoration == Decoration::RowMajor)
  {
    flags = Flags(flags | RowMajor);
  }
  else if(decoration == Decoration::ColMajor)
  {
    flags = Flags(flags | ColMajor);
  }
  else if(decoration == Decoration::Restrict)
  {
    flags = Flags(flags | Restrict);
  }
  else if(decoration == Decoration::Aliased)
  {
    flags = Flags(flags | Aliased);
  }
  else if(decoration == Decoration::Location)
  {
    RDCASSERT(!(flags & HasArrayStride));
    flags = Flags(flags | HasLocation);

    location = decoration.location;
  }
  else if(decoration == Decoration::ArrayStride)
  {
    RDCASSERT(!(flags & HasLocation));
    flags = Flags(flags | HasArrayStride);

    arrayStride = decoration.arrayStride;
  }
  else if(decoration == Decoration::DescriptorSet)
  {
    RDCASSERT(!(flags & HasOffset));
    flags = Flags(flags | HasDescriptorSet);

    set = decoration.descriptorSet;
  }
  else if(decoration == Decoration::Offset)
  {
    RDCASSERT(!(flags & HasDescriptorSet));
    flags = Flags(flags | HasOffset);

    offset = decoration.offset;
  }
  else if(decoration == Decoration::BuiltIn)
  {
    RDCASSERT(!(flags & HasBinding));
    flags = Flags(flags | HasBuiltIn);

    builtIn = decoration.builtIn;
  }
  else if(decoration == Decoration::Binding)
  {
    RDCASSERT(!(flags & HasBuiltIn));
    flags = Flags(flags | HasBinding);

    binding = decoration.binding;
  }
  else if(decoration == Decoration::SpecId)
  {
    RDCASSERT(!(flags & HasMatrixStride));
    flags = Flags(flags | HasSpecId);

    specID = decoration.specId;
  }
  else if(decoration == Decoration::MatrixStride)
  {
    RDCASSERT(!(flags & HasSpecId));
    flags = Flags(flags | HasMatrixStride);

    matrixStride = decoration.matrixStride;
  }
  else
  {
    others.push_back(decoration);
  }
}

void Decorations::Unregister(const DecorationAndParamData &decoration)
{
  if(decoration == Decoration::Block)
  {
    flags = Flags(flags & ~Block);
  }
  else if(decoration == Decoration::BufferBlock)
  {
    flags = Flags(flags & ~BufferBlock);
  }
  else if(decoration == Decoration::RowMajor)
  {
    flags = Flags(flags & ~RowMajor);
  }
  else if(decoration == Decoration::ColMajor)
  {
    flags = Flags(flags & ~ColMajor);
  }
  else if(decoration == Decoration::Location)
  {
    flags = Flags(flags & ~HasLocation);
    location = ~0U;
  }
  else if(decoration == Decoration::ArrayStride)
  {
    flags = Flags(flags & ~HasArrayStride);
    arrayStride = ~0U;
  }
  else if(decoration == Decoration::DescriptorSet)
  {
    flags = Flags(flags & ~HasDescriptorSet);
    set = ~0U;
  }
  else if(decoration == Decoration::Offset)
  {
    flags = Flags(flags & ~HasOffset);
    offset = ~0U;
  }
  else if(decoration == Decoration::BuiltIn)
  {
    flags = Flags(flags & ~HasBuiltIn);
    builtIn = (BuiltIn)~0U;
  }
  else if(decoration == Decoration::SpecId)
  {
    flags = Flags(flags & ~HasSpecId);
    specID = ~0U;
  }
  else if(decoration == Decoration::MatrixStride)
  {
    flags = Flags(flags & ~HasMatrixStride);
    matrixStride = ~0U;
  }
  else if(decoration == Decoration::Binding)
  {
    flags = Flags(flags & ~HasBinding);
    binding = ~0U;
  }
  else
  {
    Decoration val = decoration.value;
    others.removeIf([val](const DecorationAndParamData &m) { return m.value == val; });
  }
}

Processor::Processor()
{
}

Processor::~Processor()
{
}

void Processor::Parse(const rdcarray<uint32_t> &spirvWords)
{
  m_SPIRV = spirvWords;

  if(m_SPIRV.size() < FirstRealWord || m_SPIRV[0] != MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    m_SPIRV.clear();
    return;
  }

  uint32_t packedVersion = m_SPIRV[1];

  // Bytes: 0 | major | minor | 0
  m_MajorVersion = uint8_t((packedVersion & 0x00ff0000) >> 16);
  m_MinorVersion = uint8_t((packedVersion & 0x0000ff00) >> 8);

  if(packedVersion > VersionPacked)
  {
    RDCERR("Unsupported SPIR-V version: %08x", packedVersion);
    m_SPIRV.clear();
    return;
  }

  m_Generator = Generator(m_SPIRV[2] >> 16);
  m_GeneratorVersion = m_SPIRV[2] & 0xffff;

  // [4] is reserved
  RDCASSERT(m_SPIRV[4] == 0);

  PreParse(m_SPIRV[3]);

  // simple state machine to track which section we're in.
  // Note that a couple of sections are optional and could be skipped over, at which point we insert
  // a dummy OpNop so they're not empty (which will be stripped later) and record them as in
  // between.
  //
  // We only handle single-shader modules at the moment, so some things are required by virtue of
  // being required in a shader - e.g. at least the Shader capability, at least one entry point, etc
  //
  // Capabilities: REQUIRED (we assume - must declare Shader capability)
  // Extensions: OPTIONAL
  // ExtInst: OPTIONAL
  // MemoryModel: REQUIRED (required by spec)
  // EntryPoints: REQUIRED (we assume)
  // ExecutionMode: OPTIONAL
  // DebugStringSource: OPTIONAL
  // DebugNames: OPTIONAL
  // DebugModuleProcessed: OPTIONAL
  // Annotations: OPTIONAL (in theory - would require empty shader)
  // TypesVariables: REQUIRED (must at least have the entry point function type)
  // Functions: REQUIRED (must have the entry point)

  // set the book-ends: start of the first section and end of the last
  m_Sections[Section::Count - 1].endOffset = m_SPIRV.size();

#define START_SECTION(section)             \
  if(m_Sections[section].startOffset == 0) \
    m_Sections[section].startOffset = it.offs();

  for(Iter it(m_SPIRV, FirstRealWord); it; it++)
  {
    Op opcode = it.opcode();

    if(opcode == Op::Capability)
    {
      START_SECTION(Section::Capabilities);
    }
    else if(opcode == Op::Extension)
    {
      START_SECTION(Section::Extensions);
    }
    else if(opcode == Op::ExtInstImport)
    {
      START_SECTION(Section::ExtInst);
    }
    else if(opcode == Op::MemoryModel)
    {
      START_SECTION(Section::MemoryModel);
    }
    else if(opcode == Op::EntryPoint)
    {
      START_SECTION(Section::EntryPoints);
    }
    else if(opcode == Op::ExecutionMode || opcode == Op::ExecutionModeId)
    {
      START_SECTION(Section::ExecutionMode);
    }
    else if(opcode == Op::String || opcode == Op::Source || opcode == Op::SourceContinued ||
            opcode == Op::SourceExtension)
    {
      START_SECTION(Section::DebugStringSource);
    }
    else if(opcode == Op::Name || opcode == Op::MemberName)
    {
      START_SECTION(Section::DebugNames);
    }
    else if(opcode == Op::ModuleProcessed)
    {
      START_SECTION(Section::DebugModuleProcessed);
    }
    else if(opcode == Op::Decorate || opcode == Op::MemberDecorate || opcode == Op::GroupDecorate ||
            opcode == Op::GroupMemberDecorate || opcode == Op::DecorationGroup ||
            opcode == Op::DecorateStringGOOGLE || opcode == Op::MemberDecorateStringGOOGLE)
    {
      START_SECTION(Section::Annotations);
    }
    else if(opcode == Op::Function)
    {
      START_SECTION(Section::Functions);
    }
    else
    {
      // if we've reached another instruction, check if we've reached the function section yet. If
      // we have then assume it's an instruction inside a function and ignore. If we haven't, assume
      // it's a type/variable/constant type instruction
      if(m_Sections[Section::Functions].startOffset == 0)
      {
        START_SECTION(Section::TypesVariablesConstants);
      }
    }

    RegisterOp(it);
  }

#undef START_SECTION

  PostParse();

  // ensure we got everything right. First section should start at the beginning
  RDCASSERTEQUAL(m_Sections[Section::First].startOffset, FirstRealWord);

  // if the last section is empty, set its start ofset. This will cascade below
  if(m_Sections[Section::Count - 1].startOffset == 0)
    m_Sections[Section::Count - 1].startOffset = m_Sections[Section::Count - 1].endOffset;

  // we now set the endOffset of each section to the start of the next. Any empty sections
  // temporarily have startOffset set to endOffset, we'll pad them with a nop below.
  for(int s = Section::Count - 1; s > 0; s--)
  {
    RDCASSERTEQUAL(m_Sections[s - 1].endOffset, 0);
    m_Sections[s - 1].endOffset = m_Sections[s].startOffset;
    if(m_Sections[s - 1].startOffset == 0)
      m_Sections[s - 1].startOffset = m_Sections[s - 1].endOffset;
  }
}

void Processor::PreParse(uint32_t maxId)
{
  UpdateMaxID(maxId);

  m_DeferMemberDecorations = true;
}

void Processor::UpdateMaxID(uint32_t maxId)
{
  decorations.resize(maxId);
  idOffsets.resize(maxId);
  idTypes.resize(maxId);
}

void Processor::RegisterOp(Iter it)
{
  OpDecoder opdata(it);
  if(opdata.result != Id() && opdata.resultType != Id())
    idTypes[opdata.result] = opdata.resultType;

  if(opdata.result != Id())
    idOffsets[opdata.result] = it.offs();

  if(opdata.op == Op::Capability)
  {
    OpCapability decoded(it);
    capabilities.insert(decoded.capability);
  }
  else if(opdata.op == Op::Extension)
  {
    OpExtension decoded(it);
    extensions.insert(decoded.name);
  }
  else if(opdata.op == Op::ExtInstImport)
  {
    OpExtInstImport decoded(it);
    extSets[decoded.result] = decoded.name;

    if(decoded.name == "GLSL.std.450")
      knownExtSet[ExtSet_GLSL450] = decoded.result;
    else if(decoded.name == "NonSemantic.DebugPrintf")
      knownExtSet[ExtSet_Printf] = decoded.result;
    else if(decoded.name == "NonSemantic.Shader.DebugInfo.100")
      knownExtSet[ExtSet_ShaderDbg] = decoded.result;
  }
  else if(opdata.op == Op::EntryPoint)
  {
    OpEntryPoint decoded(it);
    entries.push_back(
        EntryPoint(decoded.executionModel, decoded.entryPoint, decoded.name, decoded.iface));
  }
  else if(opdata.op == Op::ExecutionMode)
  {
    OpExecutionMode decoded(it);

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == decoded.entryPoint)
      {
        entryIt->executionModes.Register(decoded);
        break;
      }
    }
  }
  else if(opdata.op == Op::ExecutionModeId)
  {
    OpExecutionModeId decoded(it);

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == decoded.entryPoint)
      {
        entryIt->executionModes.Register(decoded);
        break;
      }
    }
  }
  else if(opdata.op == Op::Variable)
  {
    OpVariable decoded(it);

    // only register global variables here
    if(decoded.storageClass != rdcspv::StorageClass::Function)
      globals.push_back(
          Variable(decoded.resultType, decoded.result, decoded.storageClass, decoded.initializer));
  }
  else if(opdata.op == Op::ConstantNull)
  {
    OpConstantNull decoded(it);

    DataType &type = dataTypes[decoded.resultType];

    ShaderVariable v = MakeNULL(type, 0ULL);
    v.name = "NULL";

    constants[decoded.result] = {decoded.resultType, decoded.result, v, {}, opdata.op};
  }
  else if(opdata.op == Op::Undef)
  {
    OpUndef decoded(it);

    DataType &type = dataTypes[decoded.resultType];

    ShaderVariable v = MakeNULL(type, 0xccccccccccccccccULL);
    v.name = "Undef";

    constants[decoded.result] = {decoded.resultType, decoded.result, v, {}, opdata.op};
  }
  else if(opdata.op == Op::ConstantTrue || opdata.op == Op::SpecConstantTrue)
  {
    OpConstantTrue decoded(it);

    ShaderVariable v("true", 1, 0, 0, 0);
    v.columns = 1;
    v.type = VarType::Bool;

    constants[decoded.result] = {decoded.resultType, decoded.result, v, {}, opdata.op};
    if(opdata.op == Op::SpecConstantTrue)
      specConstants.insert(decoded.result);
  }
  else if(opdata.op == Op::ConstantFalse || opdata.op == Op::SpecConstantFalse)
  {
    OpConstantFalse decoded(it);

    ShaderVariable v("true", 0, 0, 0, 0);
    v.columns = 1;
    v.type = VarType::Bool;

    constants[decoded.result] = {decoded.resultType, decoded.result, v, {}, opdata.op};
    if(opdata.op == Op::SpecConstantFalse)
      specConstants.insert(decoded.result);
  }
  else if(opdata.op == Op::ConstantComposite || opdata.op == Op::SpecConstantComposite)
  {
    OpConstantComposite decoded(it);

    DataType &type = dataTypes[decoded.resultType];

    RDCASSERT(type.type != DataType::UnknownType);

    ShaderVariable v("composite", 0, 0, 0, 0);
    v.rows = v.columns = 0;

    if(type.type == DataType::VectorType)
    {
      v.type = type.scalar().Type();
      v.rows = 1;
      v.columns = type.vector().count & 0xf;
    }
    else if(type.type == DataType::MatrixType)
    {
      v.type = type.scalar().Type();
      v.rows = type.vector().count & 0xf;
      v.columns = type.matrix().count & 0xf;
      // always store constants row major
      v.flags |= ShaderVariableFlags::RowMajorMatrix;
    }
    else if(type.type == DataType::StructType)
    {
      v.type = VarType::Struct;
    }

    rdcarray<ShaderVariable> members;

    for(size_t i = 0; i < decoded.constituents.size(); i++)
      members.push_back(constants[decoded.constituents[i]].value);

    ConstructCompositeConstant(v, members);

    constants[decoded.result] = {decoded.resultType, decoded.result, v, decoded.constituents,
                                 opdata.op};
    if(opdata.op == Op::SpecConstantComposite)
      specConstants.insert(decoded.result);
  }
  else if(opdata.op == Op::SpecConstantOp)
  {
    // this one has complex decoding rules, so we do it by hand.
    SpecOp specop = {opdata.resultType, opdata.result, (Op)it.word(3)};

    for(size_t w = 4; w < it.size(); w++)
      specop.params.push_back(Id::fromWord(it.word(w)));

    specOps[opdata.result] = specop;
    constants[opdata.result] = {opdata.resultType, opdata.result, ShaderVariable(), {}, opdata.op};
    specConstants.insert(opdata.result);
  }
  else if(opdata.op == Op::Constant || opdata.op == Op::SpecConstant)
  {
    // this one has complex decoding rules, so we do it by hand.
    DataType &type = dataTypes[opdata.resultType];

    RDCASSERT(type.type == DataType::ScalarType);

    ShaderVariable v("value", 1, 0, 0, 0);
    v.columns = 1;

    v.type = type.scalar().Type();

    v.value.u32v[0] = it.word(3);

    if(type.scalar().width > 32)
    {
      v.value.u32v[1] = it.word(4);
    }
    else
    {
      // if it's signed, sign extend
      if(type.scalar().signedness && type.scalar().width <= 16)
      {
        // is the top bit set?
        if(v.value.u32v[0] & (1 << (type.scalar().width - 1)))
        {
          uint32_t mask = 0xFFFFFFFFU >> (32 - type.scalar().width);

          v.value.u32v[0] |= ~mask & ~0U;
        }
      }
    }

    constants[opdata.result] = {opdata.resultType, opdata.result, v, {}, opdata.op};
    if(opdata.op == Op::SpecConstant)
      specConstants.insert(opdata.result);
  }
  else if(opdata.op == Op::TypeVoid || opdata.op == Op::TypeBool || opdata.op == Op::TypeInt ||
          opdata.op == Op::TypeFloat)
  {
    dataTypes[opdata.result] = DataType(opdata.result, Scalar(it));
  }
  else if(opdata.op == Op::TypeVector)
  {
    OpTypeVector decoded(it);

    dataTypes[opdata.result] =
        DataType(opdata.result, decoded.componentType,
                 Vector(dataTypes[decoded.componentType].scalar(), decoded.componentCount));
  }
  else if(opdata.op == Op::TypeMatrix)
  {
    OpTypeMatrix decoded(it);

    dataTypes[opdata.result] =
        DataType(opdata.result, decoded.columnType,
                 Matrix(dataTypes[decoded.columnType].vector(), decoded.columnCount));
  }
  else if(opdata.op == Op::TypeStruct)
  {
    OpTypeStruct decoded(it);
    dataTypes[opdata.result] = DataType(opdata.result, decoded.members);
  }
  else if(opdata.op == Op::TypePointer)
  {
    OpTypePointer decoded(it);
    dataTypes[opdata.result] = DataType(opdata.result, Pointer(decoded.type, decoded.storageClass));
  }
  else if(opdata.op == Op::TypeArray)
  {
    OpTypeArray decoded(it);
    dataTypes[opdata.result] = DataType(opdata.result, decoded.elementType, decoded.length);
  }
  else if(opdata.op == Op::TypeRuntimeArray)
  {
    OpTypeRuntimeArray decoded(it);
    dataTypes[opdata.result] = DataType(opdata.result, decoded.elementType, Id());
  }
  else if(opdata.op == Op::TypeImage)
  {
    OpTypeImage decoded(it);

    RDCASSERT(dataTypes[decoded.sampledType].type != DataType::UnknownType);

    imageTypes[opdata.result] =
        Image(dataTypes[decoded.sampledType].scalar(), decoded.dim, decoded.depth, decoded.arrayed,
              decoded.mS, decoded.sampled, decoded.imageFormat);

    dataTypes[opdata.result] = DataType(opdata.result, DataType::ImageType);
  }
  else if(opdata.op == Op::TypeSampler)
  {
    samplerTypes[opdata.result] = Sampler();
    dataTypes[opdata.result] = DataType(opdata.result, DataType::SamplerType);
  }
  else if(opdata.op == Op::TypeSampledImage)
  {
    OpTypeSampledImage decoded(it);

    sampledImageTypes[decoded.result] = SampledImage(decoded.imageType);
    dataTypes[opdata.result] = DataType(opdata.result, DataType::SampledImageType);
  }
  else if(opdata.op == Op::TypeFunction)
  {
    OpTypeFunction decoded(it);

    functionTypes[decoded.result] = FunctionType(decoded.returnType, decoded.parameters);
  }
  else if(opdata.op == Op::TypeRayQueryKHR)
  {
    OpTypeRayQueryKHR decoded(it);
    dataTypes[opdata.result] = DataType(decoded.result, DataType::RayQueryType);
  }
  else if(opdata.op == Op::TypeAccelerationStructureKHR)
  {
    OpTypeAccelerationStructureKHR decoded(it);
    dataTypes[opdata.result] = DataType(decoded.result, DataType::AccelerationStructureType);
  }
  else if(opdata.op == Op::Decorate)
  {
    OpDecorate decoded(it);

    decorations[decoded.target].Register(decoded.decoration);
  }
  else if(opdata.op == Op::DecorateId)
  {
    OpDecorateId decoded(it);

    decorations[decoded.target].Register(decoded.decoration);
  }
  else if(opdata.op == Op::DecorateString)
  {
    OpDecorateString decoded(it);

    decorations[decoded.target].Register(decoded.decoration);
  }
  else if(opdata.op == Op::MemberDecorate)
  {
    OpMemberDecorate decoded(it);

    if(m_DeferMemberDecorations)
    {
      m_MemberDecorations.push_back({decoded.structureType, decoded.member, decoded.decoration});
    }
    else
    {
      if(decoded.member < dataTypes[decoded.structureType].children.size())
        dataTypes[decoded.structureType].children[decoded.member].decorations.Register(
            decoded.decoration);
      else
        RDCERR("Non-deferred member decoration referenced invalid type member");
    }
  }
  else if(opdata.op == Op::MemberDecorateString)
  {
    OpMemberDecorateString decoded(it);

    if(m_DeferMemberDecorations)
    {
      m_MemberDecorations.push_back({decoded.structType, decoded.member, decoded.decoration});
    }
    else
    {
      if(decoded.member < dataTypes[decoded.structType].children.size())
        dataTypes[decoded.structType].children[decoded.member].decorations.Register(
            decoded.decoration);
      else
        RDCERR("Non-deferred member decoration referenced invalid type member");
    }
  }
  else if(opdata.op == Op::DecorationGroup || opdata.op == Op::GroupDecorate ||
          opdata.op == Op::GroupMemberDecorate)
  {
    RDCERR("Unhandled decoration group usage");
  }
}

void Processor::UnregisterOp(Iter it)
{
  OpDecoder opdata(it);
  if(opdata.result != Id() && opdata.resultType != Id())
    idTypes[opdata.result] = Id();

  if(opdata.result != Id())
    idOffsets[opdata.result] = 0;

  if(opdata.op == Op::Capability)
  {
    OpCapability decoded(it);
    capabilities.erase(decoded.capability);
  }
  else if(opdata.op == Op::Extension)
  {
    OpExtension decoded(it);
    extensions.erase(decoded.name);
  }
  else if(opdata.op == Op::ExtInstImport)
  {
    OpExtInstImport decoded(it);
    extSets.erase(decoded.result);
  }
  else if(opdata.op == Op::EntryPoint)
  {
    OpEntryPoint decoded(it);

    Id entry = decoded.entryPoint;

    entries.removeOneIf([entry](const EntryPoint &e) { return e.id == entry; });
  }
  else if(opdata.op == Op::ExecutionMode)
  {
    OpExecutionMode decoded(it);

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == decoded.entryPoint)
      {
        entryIt->executionModes.Unregister(decoded);
        break;
      }
    }
  }
  else if(opdata.op == Op::Variable)
  {
    Id result = opdata.result;

    globals.removeOneIf([result](const Variable &e) { return e.id == result; });
  }
  else if(opdata.op == Op::ConstantNull || opdata.op == Op::Undef ||
          opdata.op == Op::ConstantTrue || opdata.op == Op::ConstantFalse ||
          opdata.op == Op::ConstantComposite || opdata.op == Op::Constant ||
          opdata.op == Op::SpecConstantTrue || opdata.op == Op::SpecConstantFalse ||
          opdata.op == Op::SpecConstantComposite || opdata.op == Op::SpecConstant)
  {
    constants.erase(opdata.result);
    specConstants.erase(opdata.result);
  }
  else if(opdata.op == Op::SpecConstantOp)
  {
    specOps.erase(opdata.result);
    specConstants.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeVoid || opdata.op == Op::TypeBool || opdata.op == Op::TypeInt ||
          opdata.op == Op::TypeFloat || opdata.op == Op::TypeVector ||
          opdata.op == Op::TypeMatrix || opdata.op == Op::TypeStruct || opdata.op == Op::TypeArray ||
          opdata.op == Op::TypePointer || opdata.op == Op::TypeRuntimeArray ||
          opdata.op == Op::TypeRayQueryKHR || opdata.op == Op::TypeAccelerationStructureKHR)
  {
    dataTypes[opdata.result] = DataType();
  }
  else if(opdata.op == Op::TypeImage)
  {
    imageTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeSampler)
  {
    samplerTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeSampledImage)
  {
    sampledImageTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::TypeFunction)
  {
    functionTypes.erase(opdata.result);
  }
  else if(opdata.op == Op::Decorate)
  {
    OpDecorate decoded(it);

    decorations[decoded.target].Unregister(decoded.decoration);
  }
  else if(opdata.op == Op::DecorateId)
  {
    OpDecorateId decoded(it);

    decorations[decoded.target].Unregister(decoded.decoration);
  }
  else if(opdata.op == Op::DecorateString)
  {
    OpDecorateString decoded(it);

    decorations[decoded.target].Unregister(decoded.decoration);
  }
  else if(opdata.op == Op::MemberDecorate)
  {
    OpMemberDecorate decoded(it);

    RDCASSERT(dataTypes[decoded.structureType].type == DataType::StructType);

    if(decoded.member < dataTypes[decoded.structureType].children.size())
      dataTypes[decoded.structureType].children[decoded.member].decorations.Unregister(
          decoded.decoration);
  }
  else if(opdata.op == Op::MemberDecorateString)
  {
    OpMemberDecorateString decoded(it);

    RDCASSERT(dataTypes[decoded.structType].type == DataType::StructType);

    if(decoded.member < dataTypes[decoded.structType].children.size())
      dataTypes[decoded.structType].children[decoded.member].decorations.Unregister(
          decoded.decoration);
  }
  else if(opdata.op == Op::DecorationGroup || opdata.op == Op::GroupDecorate ||
          opdata.op == Op::GroupMemberDecorate)
  {
    RDCERR("Unhandled decoration group usage");
  }
}

void Processor::PostParse()
{
  for(const DeferredMemberDecoration &dec : m_MemberDecorations)
    if(dec.member < dataTypes[dec.id].children.size())
      dataTypes[dec.id].children[dec.member].decorations.Register(dec.dec);

  m_MemberDecorations.clear();
  m_DeferMemberDecorations = false;
}

Iter Processor::GetID(Id id)
{
  size_t offs = idOffsets[id];

  if(offs)
    return Iter(m_SPIRV, offs);

  return Iter();
}

ConstIter Processor::GetID(Id id) const
{
  size_t offs = idOffsets[id];

  if(offs)
    return ConstIter(m_SPIRV, offs);

  return ConstIter();
}

ShaderVariable Processor::MakeNULL(const DataType &type, uint64_t value)
{
  ShaderVariable v("", 0, 0, 0, 0);
  v.rows = v.columns = 0;

  for(uint8_t c = 0; c < 16; c++)
    v.value.u64v[c] = value;

  if(type.type == DataType::VectorType)
  {
    v.type = type.scalar().Type();
    v.rows = 1;
    v.columns = type.vector().count & 0xf;
  }
  else if(type.type == DataType::MatrixType)
  {
    v.type = type.scalar().Type();
    v.rows = type.vector().count & 0xf;
    v.columns = type.matrix().count & 0xf;
    v.flags |= ShaderVariableFlags::RowMajorMatrix;
  }
  else if(type.type == DataType::ScalarType)
  {
    v.type = type.scalar().Type();
    v.rows = 1;
    v.columns = 1;
  }
  else if(type.type == DataType::PointerType)
  {
    v.type = VarType::GPUPointer;
    v.rows = 1;
    v.columns = 1;
  }
  else if(type.type == DataType::ArrayType)
  {
    // TODO handle spec constant array length here... somehow
    v.members.resize(EvaluateConstant(type.length, {}).value.u32v[0]);
    for(size_t i = 0; i < v.members.size(); i++)
    {
      v.members[i] = MakeNULL(dataTypes[type.InnerType()], value);
      v.members[i].name = StringFormat::Fmt("[%zu]", i);
    }
  }
  else
  {
    if(type.type == DataType::StructType)
      v.type = VarType::Struct;
    else
      RDCWARN("Unexpected type %d making NULL", type.type);

    v.members.resize(type.children.size());
    for(size_t i = 0; i < v.members.size(); i++)
    {
      v.members[i] = MakeNULL(dataTypes[type.children[i].type], value);
      v.members[i].name = StringFormat::Fmt("_child%zu", i);
    }
  }

  return v;
}

ShaderVariable Processor::EvaluateConstant(Id constID, const rdcarray<SpecConstant> &specInfo) const
{
  auto it = constants.find(constID);

  if(it == constants.end())
  {
    RDCERR("Lookup of unknown constant %u", constID.value());
    return ShaderVariable("unknown", 0, 0, 0, 0);
  }

  auto specopit = specOps.find(constID);

  if(specopit != specOps.end())
  {
    const SpecOp &specop = specopit->second;
    ShaderVariable ret = {};

    const DataType &retType = dataTypes[specop.type];

    ret.type = retType.scalar().Type();

    if(specop.params.empty())
    {
      RDCERR("Expected paramaters for SpecConstantOp %s", ToStr(specop.op).c_str());
      return ret;
    }

    // these instructions have special rules, so handle them manually
    if(specop.op == Op::Select)
    {
      // evaluate the parameters
      rdcarray<ShaderVariable> params;
      for(size_t i = 0; i < specop.params.size(); i++)
        params.push_back(EvaluateConstant(specop.params[i], specInfo));

      if(params.size() != 3)
      {
        RDCERR("Expected 3 paramaters for SpecConstantOp Select, got %zu", params.size());
        return ret;
      }

      // "If Condition is a scalar and true, the result is Object 1. If Condition is a scalar and
      // false, the result is Object 2."
      if(params[0].columns == 1)
        return params[0].value.u32v[0] ? params[1] : params[2];

      // "If Condition is a vector, Result Type must be a vector with the same number of components
      // as Condition and the result is a [component-wise] mix of Object 1 and Object 2."
      ret = params[1];
      ret.name = "derived";
      for(size_t i = 0; i < params[0].columns; i++)
      {
        if(retType.scalar().width == 64)
          ret.value.u64v[i] =
              params[0].value.u32v[i] ? params[1].value.u64v[i] : params[2].value.u64v[i];
        else if(retType.scalar().width == 32)
          ret.value.u32v[i] =
              params[0].value.u32v[i] ? params[1].value.u32v[i] : params[2].value.u32v[i];
        else if(retType.scalar().width == 16)
          ret.value.u16v[i] =
              params[0].value.u32v[i] ? params[1].value.u16v[i] : params[2].value.u16v[i];
        else
          ret.value.u8v[i] = params[0].value.u8v[i] ? params[1].value.u8v[i] : params[2].value.u8v[i];
      }
    }
    else if(specop.op == Op::CompositeExtract)
    {
      ShaderVariable composite = EvaluateConstant(specop.params[0], specInfo);
      // the remaining parameters are actually indices
      rdcarray<uint32_t> indices;
      for(size_t i = 1; i < specop.params.size(); i++)
        indices.push_back(specop.params[i].value());

      ret = composite;
      ret.name = "derived";

      RDCEraseEl(ret.value);

      if(composite.rows > 1)
      {
        ret.rows = 1;

        if(indices.size() == 1)
        {
          // matrix returning a vector
          uint32_t row = indices[0];

          for(uint32_t c = 0; c < ret.columns; c++)
          {
            if(retType.scalar().width == 64)
              ret.value.u64v[c] = composite.value.u64v[row * composite.columns + c];
            else if(retType.scalar().width == 32)
              ret.value.u32v[c] = composite.value.u32v[row * composite.columns + c];
            else if(retType.scalar().width == 16)
              ret.value.u16v[c] = composite.value.u16v[row * composite.columns + c];
            else
              ret.value.u8v[c] = composite.value.u8v[row * composite.columns + c];
          }
        }
        else if(indices.size() == 2)
        {
          // matrix returning a scalar
          uint32_t row = indices[0];
          uint32_t col = indices[1];

          if(retType.scalar().width == 64)
            ret.value.u64v[0] = composite.value.u64v[row * composite.columns + col];
          else if(retType.scalar().width == 32)
            ret.value.u32v[0] = composite.value.u32v[row * composite.columns + col];
          else if(retType.scalar().width == 16)
            ret.value.u16v[0] = composite.value.u16v[row * composite.columns + col];
          else
            ret.value.u8v[0] = composite.value.u8v[row * composite.columns + col];
        }
        else
        {
          RDCERR("Unexpected number of indices %zu to SpecConstantOp CompositeInsert",
                 indices.size());
        }
      }
      else
      {
        ret.columns = 1;

        if(indices.size() == 1)
        {
          uint32_t col = indices[0];

          // vector returning a scalar
          if(retType.scalar().width == 64)
            ret.value.u64v[0] = composite.value.u64v[col];
          else if(retType.scalar().width == 32)
            ret.value.u32v[0] = composite.value.u32v[col];
          else if(retType.scalar().width == 16)
            ret.value.u16v[0] = composite.value.u16v[col];
          else
            ret.value.u8v[0] = composite.value.u8v[col];
        }
        else
        {
          RDCERR("Unexpected number of indices %zu to SpecConstantOp CompositeInsert",
                 indices.size());
        }
      }

      return ret;
    }
    else if(specop.op == Op::CompositeInsert)
    {
      if(specop.params.size() < 3)
      {
        RDCERR("Expected at least 3 paramaters for SpecConstantOp CompositeInsert, got %zu",
               specop.params.size());
        return ret;
      }

      ShaderVariable object = EvaluateConstant(specop.params[0], specInfo);
      ShaderVariable composite = EvaluateConstant(specop.params[1], specInfo);
      // the remaining parameters are actually indices
      rdcarray<uint32_t> indices;
      for(size_t i = 2; i < specop.params.size(); i++)
        indices.push_back(specop.params[i].value());

      composite.name = "derived";

      if(composite.rows > 1)
      {
        if(indices.size() == 1)
        {
          // matrix inserting a vector
          uint32_t row = indices[0];

          for(uint32_t c = 0; c < ret.columns; c++)
          {
            if(retType.scalar().width == 64)
              composite.value.u64v[row * composite.columns + c] = object.value.u64v[c];
            else if(retType.scalar().width == 32)
              composite.value.u32v[row * composite.columns + c] = object.value.u32v[c];
            else if(retType.scalar().width == 16)
              composite.value.u16v[row * composite.columns + c] = object.value.u16v[c];
            else if(retType.scalar().width == 8)
              composite.value.u8v[row * composite.columns + c] = object.value.u8v[c];
          }
        }
        else if(indices.size() == 2)
        {
          // matrix inserting a scalar
          uint32_t row = indices[0];
          uint32_t col = indices[1];

          if(retType.scalar().width == 64)
            composite.value.u64v[row * composite.columns + col] = object.value.u64v[0];
          else if(retType.scalar().width == 32)
            composite.value.u32v[row * composite.columns + col] = object.value.u32v[0];
          else if(retType.scalar().width == 16)
            composite.value.u16v[row * composite.columns + col] = object.value.u16v[0];
          else
            composite.value.u8v[row * composite.columns + col] = object.value.u8v[0];
        }
        else
        {
          RDCERR("Unexpected number of indices %zu to SpecConstantOp CompositeInsert",
                 indices.size());
        }
      }
      else
      {
        if(indices.size() == 1)
        {
          // vector inserting a scalar
          if(retType.scalar().width == 64)
            composite.value.u64v[indices[0]] = object.value.u64v[0];
          else if(retType.scalar().width == 32)
            composite.value.u32v[indices[0]] = object.value.u32v[0];
          else if(retType.scalar().width == 16)
            composite.value.u16v[indices[0]] = object.value.u16v[0];
          else
            composite.value.u8v[indices[0]] = object.value.u8v[0];
        }
        else
        {
          RDCERR("Unexpected number of indices %zu to SpecConstantOp CompositeInsert",
                 indices.size());
        }
      }

      return composite;
    }
    else if(specop.op == Op::VectorShuffle)
    {
      if(specop.params.size() < 3)
      {
        RDCERR("Expected at least 3 paramaters for SpecConstantOp VectorShuffle, got %zu",
               specop.params.size());
        return ret;
      }

      ShaderVariable vec1 = EvaluateConstant(specop.params[0], specInfo);
      ShaderVariable vec2 = EvaluateConstant(specop.params[1], specInfo);
      // the remaining parameters are actually indices
      rdcarray<uint32_t> indices;
      for(size_t i = 2; i < specop.params.size(); i++)
        indices.push_back(specop.params[i].value());

      ret = ShaderVariable("derived", 0, 0, 0, 0);
      ret.type = retType.scalar().Type();
      ret.columns = (uint8_t)indices.size();

      for(size_t i = 0; i < indices.size(); i++)
      {
        uint32_t idx = indices[i];
        if(idx < vec1.columns)
        {
          if(retType.scalar().width == 64)
            ret.value.u64v[i] = vec1.value.u64v[idx];
          else if(retType.scalar().width == 32)
            ret.value.u32v[i] = vec1.value.u32v[idx];
          else if(retType.scalar().width == 16)
            ret.value.u16v[i] = vec1.value.u16v[idx];
          else
            ret.value.u8v[i] = vec1.value.u8v[idx];
        }
        else
        {
          idx -= vec1.columns;

          if(retType.scalar().width == 64)
            ret.value.u64v[i] = vec2.value.u64v[idx];
          else if(retType.scalar().width == 32)
            ret.value.u32v[i] = vec2.value.u32v[idx];
          else if(retType.scalar().width == 16)
            ret.value.u16v[i] = vec2.value.u16v[idx];
          else
            ret.value.u8v[i] = vec2.value.u8v[idx];
        }
      }

      return ret;
    }
    else if(specop.op == Op::UConvert || specop.op == Op::SConvert || specop.op == Op::FConvert)
    {
      ShaderVariable param = EvaluateConstant(specop.params[0], specInfo);
      ret = param;

      ret.name = "converted";
      RDCEraseEl(ret.value);
      ret.type = retType.scalar().Type();

      for(uint8_t i = 0; i < param.columns; i++)
      {
        if(specop.op == Op::UConvert)
        {
          uint64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<U>(param, i);
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, param.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<U>(ret, i) = (U)x;
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, ret.type);
        }
        else if(specop.op == Op::SConvert)
        {
          int64_t x = 0;

#undef _IMPL
#define _IMPL(I, S, U) x = comp<S>(param, i);
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, param.type);

#undef _IMPL
#define _IMPL(I, S, U) comp<S>(ret, i) = (S)x;
          IMPL_FOR_INT_TYPES_FOR_TYPE(_IMPL, ret.type);
        }
        else if(specop.op == Op::FConvert)
        {
          double x = 0.0;

#undef _IMPL
#define _IMPL(T) x = comp<T>(param, i);
          IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, param.type);

#undef _IMPL
#define _IMPL(T) comp<T>(ret, i) = (T)x;
          // IMPL_FOR_FLOAT_TYPES_FOR_TYPE(_IMPL, ret.type);

          if(ret.type == VarType::Float)
            comp<float>(ret, i) = (float)x;
          else if(ret.type == VarType::Half)
            comp<half_float::half>(ret, i) = (float)x;
          else if(ret.type == VarType::Double)
            comp<double>(ret, i) = (double)x;
        }
      }

      return ret;
    }

    // evaluate the parameters
    rdcarray<ShaderVariable> params;
    for(size_t i = 0; i < specop.params.size(); i++)
      params.push_back(EvaluateConstant(specop.params[i], specInfo));

    // all other operations are component-wise on vectors or scalars. Check that rows are all 1 and
    // all cols are identical
    uint8_t cols = params[0].columns;
    for(size_t i = 0; i < params.size(); i++)
    {
      RDCASSERT(cols == params[i].columns, i, params[i].columns);
      RDCASSERT(params[i].rows == 1, i, params[i].rows);
      cols = RDCMIN(cols, params[i].columns);
    }

    // check number of parameters
    switch(specop.op)
    {
      case Op::SNegate:
      case Op::Not:
      case Op::LogicalNot:
        if(params.size() != 1)
        {
          RDCERR("Expected 1 parameter for SpecConstantOp %s, got %zu", ToStr(specop.op).c_str(),
                 params.size());
          return ret;
        }
        break;
      case Op::IAdd:
      case Op::ISub:
      case Op::IMul:
      case Op::UDiv:
      case Op::SDiv:
      case Op::UMod:
      case Op::SRem:
      case Op::SMod:
      case Op::ShiftRightLogical:
      case Op::ShiftRightArithmetic:
      case Op::ShiftLeftLogical:
      case Op::BitwiseOr:
      case Op::BitwiseXor:
      case Op::BitwiseAnd:
      case Op::LogicalOr:
      case Op::LogicalAnd:
      case Op::LogicalEqual:
      case Op::LogicalNotEqual:
      case Op::IEqual:
      case Op::INotEqual:
      case Op::ULessThan:
      case Op::SLessThan:
      case Op::UGreaterThan:
      case Op::SGreaterThan:
      case Op::ULessThanEqual:
      case Op::SLessThanEqual:
      case Op::UGreaterThanEqual:
      case Op::SGreaterThanEqual:
        if(params.size() != 2)
        {
          RDCERR("Expected 2 paramaters for SpecConstantOp %s, got %zu", ToStr(specop.op).c_str(),
                 params.size());
          return ret;
        }
        break;
      default:
        RDCERR("Unhandled SpecConstantOp:: operation %s", ToStr(specop.op).c_str());
        return ret;
    }

    ret = params[0];
    ret.name = "derived";

    for(uint32_t col = 0; col < cols; col++)
    {
      ShaderValue a, b;

      bool signedness = retType.scalar().signedness;

      // upcast parameters to 64-bit width to simplify applying operations
      for(size_t p = 0; p < params.size() && p < 2; p++)
      {
        const DataType &paramType = dataTypes[idTypes[specop.params[p]]];

        ShaderValue &val = (p == 0) ? a : b;

        if(paramType.scalar().type == Op::TypeFloat)
        {
          if(paramType.scalar().width == 64)
            val.f64v[0] = params[p].value.f64v[col];
          else if(paramType.scalar().width == 32)
            val.f64v[0] = params[p].value.f32v[col];
          else
            val.f64v[0] = (float)params[p].value.f16v[col];
        }
        else
        {
          if(paramType.scalar().signedness)
          {
            if(paramType.scalar().width == 64)
              val.s64v[0] = params[p].value.s64v[col];
            else if(paramType.scalar().width == 32)
              val.s64v[0] = params[p].value.s32v[col];
            else if(paramType.scalar().width == 16)
              val.s64v[0] = params[p].value.s16v[col];
            else
              val.s64v[0] = params[p].value.s8v[col];
          }
          else
          {
            if(paramType.scalar().width == 64)
              val.u64v[0] = params[p].value.u64v[col];
            else if(paramType.scalar().width == 32)
              val.u64v[0] = params[p].value.u32v[col];
            else if(paramType.scalar().width == 16)
              val.u64v[0] = params[p].value.u16v[col];
            else
              val.u64v[0] = params[p].value.u8v[col];
          }
        }
      }

      switch(specop.op)
      {
        case Op::SNegate: a.s64v[0] = -a.s64v[0]; break;
        case Op::Not: a.u64v[0] = ~a.u64v[0]; break;
        case Op::LogicalNot: a.u64v[0] = a.u64v[0] ? 0 : 1; break;
        case Op::IAdd:
          if(signedness)
            a.s64v[0] += b.s64v[0];
          else
            a.u64v[0] += b.u64v[0];
          break;
        case Op::ISub:
          if(signedness)
            a.s64v[0] -= b.s64v[0];
          else
            a.u64v[0] -= b.u64v[0];
          break;
        case Op::IMul:
          if(signedness)
            a.s64v[0] *= b.s64v[0];
          else
            a.u64v[0] *= b.u64v[0];
          break;
        case Op::UDiv: a.u64v[0] /= b.u64v[0]; break;
        case Op::SDiv: a.s64v[0] /= b.s64v[0]; break;
        case Op::UMod: a.u64v[0] %= b.u64v[0]; break;
        case Op::SRem:
        case Op::SMod:
        {
          int64_t result = a.s64v[0] % b.s64v[0];

          // flip sign to match given input operand

          // "the sign of r is the same as the sign of Operand 1."
          if(specop.op == Op::SRem && ((result < 0) != (a.s64v[0] < 0)))
            result = -result;
          // "the sign of r is the same as the sign of Operand 2."
          if(specop.op == Op::SMod && ((result < 0) != (b.s64v[0] < 0)))
            result = -result;

          break;
        }
        case Op::ShiftRightLogical: a.u64v[0] >>= b.u64v[0]; break;
        case Op::ShiftRightArithmetic: a.s64v[0] >>= b.s64v[0]; break;
        case Op::ShiftLeftLogical: a.u64v[0] <<= b.u64v[0]; break;
        case Op::BitwiseOr: a.u64v[0] |= b.u64v[0]; break;
        case Op::BitwiseXor: a.u64v[0] ^= b.u64v[0]; break;
        case Op::BitwiseAnd: a.u64v[0] &= b.u64v[0]; break;
        case Op::LogicalOr: a.u64v[0] = (a.u64v[0] || b.u64v[0]) ? 1 : 0; break;
        case Op::LogicalAnd: a.u64v[0] = (a.u64v[0] && b.u64v[0]) ? 1 : 0; break;
        case Op::LogicalEqual: a.u64v[0] = (a.u64v[0] == b.u64v[0]) ? 1 : 0; break;
        case Op::LogicalNotEqual: a.u64v[0] = (a.u64v[0] != b.u64v[0]) ? 1 : 0; break;
        case Op::IEqual: a.u64v[0] = (a.u64v[0] == b.u64v[0]) ? 1 : 0; break;
        case Op::INotEqual: a.u64v[0] = (a.u64v[0] != b.u64v[0]) ? 1 : 0; break;
        case Op::ULessThan: a.u64v[0] = (a.u64v[0] < b.u64v[0]) ? 1 : 0; break;
        case Op::SLessThan: a.s64v[0] = (a.s64v[0] < b.s64v[0]) ? 1 : 0; break;
        case Op::UGreaterThan: a.u64v[0] = (a.u64v[0] > b.u64v[0]) ? 1 : 0; break;
        case Op::SGreaterThan: a.s64v[0] = (a.s64v[0] > b.s64v[0]) ? 1 : 0; break;
        case Op::ULessThanEqual: a.u64v[0] = (a.u64v[0] <= b.u64v[0]) ? 1 : 0; break;
        case Op::SLessThanEqual: a.s64v[0] = (a.s64v[0] <= b.s64v[0]) ? 1 : 0; break;
        case Op::UGreaterThanEqual: a.u64v[0] = (a.u64v[0] >= b.u64v[0]) ? 1 : 0; break;
        case Op::SGreaterThanEqual: a.s64v[0] = (a.s64v[0] >= b.s64v[0]) ? 1 : 0; break;
        default: break;
      }

      // downcast back to the type required
      if(retType.scalar().type == Op::TypeFloat)
      {
        if(retType.scalar().width == 64)
          ret.value.f64v[col] = a.f64v[0];
        else if(retType.scalar().width == 32)
          ret.value.f32v[col] = (float)a.f64v[0];
        else
          ret.value.f16v[col].set((float)a.f64v[0]);
      }
      else if(signedness)
      {
        if(retType.scalar().width == 64)
          ret.value.s64v[col] = a.s64v[0];
        else if(retType.scalar().width == 32)
          ret.value.s32v[col] =
              (int32_t)RDCCLAMP(a.s64v[col], (int64_t)INT32_MIN, (int64_t)INT32_MAX);
        else if(retType.scalar().width == 16)
          ret.value.s16v[col] =
              (int16_t)RDCCLAMP(a.s64v[col], (int64_t)INT16_MIN, (int64_t)INT16_MAX);
        else
          ret.value.s8v[col] = (int8_t)RDCCLAMP(a.s64v[col], (int64_t)INT8_MIN, (int64_t)INT8_MAX);
      }
      else
      {
        if(retType.scalar().width == 64)
          ret.value.u64v[col] = a.u64v[0];
        else if(retType.scalar().width == 32)
          ret.value.u32v[col] = a.u64v[0] & 0xFFFFFFFF;
        else if(retType.scalar().width == 16)
          ret.value.u16v[col] = a.u64v[0] & 0xFFFF;
        else
          ret.value.u8v[col] = a.u64v[0] & 0xFF;
      }
    }

    return ret;
  }

  const Constant &c = it->second;

  if(decorations[c.id].flags & Decorations::HasSpecId)
  {
    for(const SpecConstant &spec : specInfo)
    {
      // if this constant is specialised, read its data instead
      if(spec.specID == decorations[c.id].specID)
      {
        ShaderVariable ret = c.value;

        // we can always just read into u64v - if the type is smaller the LSB maps nicely.
        ret.value.u64v[0] = spec.value;

        return ret;
      }
    }
  }

  if(c.op == Op::SpecConstantComposite)
  {
    ShaderVariable ret = c.value;

    rdcarray<ShaderVariable> children;

    // this is wasteful because we've probably already evaluated these constants, but we don't
    // expect a huge tree of spec constants so it's cleaner to do it here than expect the caller to
    // tidy up from its evaluated cache.
    for(size_t i = 0; i < c.children.size(); i++)
      children.push_back(EvaluateConstant(c.children[i], specInfo));

    ConstructCompositeConstant(ret, children);

    return ret;
  }

  return c.value;
}

};    // namespace rdcspv
