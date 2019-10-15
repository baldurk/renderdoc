/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include "spirv_op_helpers.h"

namespace rdcspv
{
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
    others.push_back({mode.mode.value, mode.mode.invocations});
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
    others.push_back({mode.mode.value, mode.mode.invocations});
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
    for(size_t i = 0; i < others.size(); i++)
    {
      if(others[i].first == mode.mode.value)
      {
        others.erase(i);
        break;
      }
    }
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
    for(size_t i = 0; i < others.size(); i++)
    {
      if(others[i].first == mode.mode.value)
      {
        others.erase(i);
        break;
      }
    }
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
    for(size_t i = 0; i < others.size(); i++)
    {
      if(others[i].value == decoration.value)
      {
        others.erase(i);
        break;
      }
    }
  }
}

Processor::Processor()
{
}

Processor::~Processor()
{
}

void Processor::Parse(const std::vector<uint32_t> &spirvWords)
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
  // Debug: OPTIONAL
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
            opcode == Op::SourceExtension || opcode == Op::Name || opcode == Op::MemberName ||
            opcode == Op::ModuleProcessed)
    {
      START_SECTION(Section::Debug);
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
  }
  else if(opdata.op == Op::EntryPoint)
  {
    OpEntryPoint decoded(it);
    entries.push_back(EntryPoint(decoded.executionModel, decoded.entryPoint, decoded.name));
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
  else if(opdata.op == Op::Variable)
  {
    OpVariable decoded(it);

    // only register global variables here
    if(decoded.storageClass != rdcspv::StorageClass::Function)
      globals.push_back(Variable(decoded.resultType, decoded.result, decoded.storageClass));
  }
  else if(opdata.op == Op::ConstantNull)
  {
    OpConstantNull decoded(it);

    ShaderVariable v("NULL", 0, 0, 0, 0);
    v.columns = 1;

    constants[decoded.result] = {decoded.resultType, decoded.result, v};
  }
  else if(opdata.op == Op::ConstantTrue || opdata.op == Op::SpecConstantTrue)
  {
    OpConstantTrue decoded(it);

    ShaderVariable v("true", 1, 0, 0, 0);
    v.columns = 1;

    constants[decoded.result] = {decoded.resultType, decoded.result, v};
    if(opdata.op == Op::SpecConstantTrue)
      specConstants.insert(decoded.result);
  }
  else if(opdata.op == Op::ConstantFalse || opdata.op == Op::SpecConstantFalse)
  {
    OpConstantFalse decoded(it);

    ShaderVariable v("true", 0, 0, 0, 0);
    v.columns = 1;

    constants[decoded.result] = {decoded.resultType, decoded.result, v};
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
    v.isStruct = (type.type == DataType::StructType);

    if(type.type == DataType::VectorType)
    {
      v.type = type.scalar().Type();
      v.rows = 1;
      v.columns = type.vector().count & 0xf;

      if(type.scalar().width == 32)
      {
        for(uint32_t i = 0; i < v.columns; i++)
          v.value.u64v[i] = constants[decoded.constituents[i]].value.value.u64v[i];
      }
      else
      {
        for(uint32_t i = 0; i < v.columns; i++)
          v.value.uv[i] = constants[decoded.constituents[i]].value.value.uv[i];
      }
    }
    else if(type.type == DataType::MatrixType)
    {
      v.type = type.scalar().Type();
      v.rows = type.vector().count & 0xf;
      v.columns = type.matrix().count & 0xf;
      // always store constants row major
      v.rowMajor = true;

      for(uint32_t c = 0; c < v.columns; c++)
      {
        for(uint32_t r = 0; r < v.rows; r++)
        {
          if(type.scalar().width == 64)
            v.value.u64v[r * v.columns + c] = constants[decoded.constituents[c]].value.value.u64v[r];
          else
            v.value.uv[r * v.columns + c] = constants[decoded.constituents[c]].value.value.uv[r];
        }
      }
    }

    v.members.resize(decoded.constituents.size());
    for(size_t i = 0; i < v.members.size(); i++)
      v.members[i] = constants[decoded.constituents[i]].value;

    constants[decoded.result] = {decoded.resultType, decoded.result, v, decoded.constituents};
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
    constants[opdata.result] = {opdata.resultType, opdata.result};
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

    v.value.uv[0] = it.word(3);

    if(type.scalar().width > 32)
    {
      v.value.uv[1] = it.word(4);
    }
    else
    {
      // if it's signed, sign extend
      if(type.scalar().signedness)
      {
        if(v.value.uv[0] & (1 << (type.scalar().width - 1)))
        {
          uint32_t mask = (1 << type.scalar().width) - 1;

          v.value.uv[0] |= ~mask & ~0U;
        }
      }
    }

    constants[opdata.result] = {opdata.resultType, opdata.result, v};
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

    m_MemberDecorations.push_back({decoded.structureType, decoded.member, decoded.decoration});
  }
  else if(opdata.op == Op::MemberDecorateString)
  {
    OpMemberDecorateString decoded(it);

    m_MemberDecorations.push_back({decoded.structType, decoded.member, decoded.decoration});
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

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->id == decoded.entryPoint)
      {
        entries.erase(entryIt);
        break;
      }
    }
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
    for(auto varIt = globals.begin(); varIt != globals.end(); ++varIt)
    {
      if(varIt->id == opdata.result)
      {
        globals.erase(varIt);
        break;
      }
    }
  }
  else if(opdata.op == Op::ConstantNull || opdata.op == Op::ConstantTrue ||
          opdata.op == Op::ConstantFalse || opdata.op == Op::ConstantComposite ||
          opdata.op == Op::Constant || opdata.op == Op::SpecConstantTrue ||
          opdata.op == Op::SpecConstantFalse || opdata.op == Op::SpecConstantComposite ||
          opdata.op == Op::SpecConstant)
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
          opdata.op == Op::TypePointer || opdata.op == Op::TypeRuntimeArray)
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
}

};    // namespace rdcspv