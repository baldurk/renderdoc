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

#include "spirv_editor.h"
#include <algorithm>
#include <utility>
#include "common/common.h"
#include "serialise/serialiser.h"
#include "spirv_op_helpers.h"

namespace rdcspv
{
Scalar::Scalar(Iter it)
{
  type = it.opcode();

  if(type == Op::TypeInt)
  {
    OpTypeInt decoded(it);
    width = decoded.width;
    signedness = decoded.signedness == 1;
  }
  else if(type == Op::TypeFloat)
  {
    OpTypeFloat decoded(it);
    width = decoded.width;
    signedness = false;
  }
  else if(type == Op::TypeBool)
  {
    width = 32;
    signedness = false;
  }
  else
  {
    width = 0;
    signedness = false;
  }
}

Id OperationList::add(const rdcspv::Operation &op)
{
  push_back(op);
  return OpDecoder(op.AsIter()).result;
}

Editor::Editor(rdcarray<uint32_t> &spirvWords) : m_ExternalSPIRV(spirvWords)
{
}

void Editor::Prepare()
{
  Processor::Parse(m_ExternalSPIRV);

  if(m_SPIRV.empty())
    return;

  // In 1.3 and after we can (and should - it's gone in 1.4+) use the real SSBO storage class
  // instead of Uniform + BufferBlock
  if(m_MajorVersion > 1 || m_MinorVersion >= 3)
    m_StorageBufferClass = rdcspv::StorageClass::StorageBuffer;

  // find any empty sections and insert a nop into the stream there. We need to fixup later section
  // offsets by hand as addWords doesn't handle empty sections properly (it thinks we're inserting
  // into the later section by offset since the offsets overlap). That's why we're adding these
  // padding nops in the first place!
  for(uint32_t s = 0; s < Section::Count; s++)
  {
    if(m_Sections[s].startOffset == m_Sections[s].endOffset)
    {
      m_SPIRV.insert(m_Sections[s].startOffset, OpNopWord);
      m_Sections[s].endOffset++;

      for(uint32_t t = s + 1; t < Section::Count; t++)
      {
        m_Sections[t].startOffset++;
        m_Sections[t].endOffset++;
      }

      // look through every id, and update its offset
      for(size_t &o : idOffsets)
        if(o >= m_Sections[s].startOffset)
          o++;
    }
  }

  // each section should now precisely match each other end-to-end and not be empty
  for(uint32_t s = Section::First; s < Section::Count; s++)
  {
    RDCASSERTNOTEQUAL(m_Sections[s].startOffset, 0);
    RDCASSERTNOTEQUAL(m_Sections[s].endOffset, 0);

    RDCASSERT(m_Sections[s].endOffset - m_Sections[s].startOffset > 0, m_Sections[s].startOffset,
              m_Sections[s].endOffset);

    if(s != 0)
      RDCASSERTEQUAL(m_Sections[s - 1].endOffset, m_Sections[s].startOffset);

    if(s + 1 < Section::Count)
      RDCASSERTEQUAL(m_Sections[s].endOffset, m_Sections[s + 1].startOffset);
  }
}

void Editor::CreateEmpty(uint32_t major, uint32_t minor)
{
  if(!m_ExternalSPIRV.empty())
  {
    RDCERR("Creating empty SPIR-V module with some SPIR-V words already in place!");
    m_ExternalSPIRV.clear();
  }

  // create an empty SPIR-V header with an upper ID bound of 1

  m_ExternalSPIRV = {
      MagicNumber, (major << 16) | (minor << 8),
      0,    // TODO maybe register a generator ID?
      1,    // bound
      0,    // instruction schema
  };

  // we need at least one opcode to parse properly, and we'll always need shader.
  Operation shader = Operation(OpCapability(Capability::Shader));
  m_ExternalSPIRV.append(&shader[0], shader.size());

  Prepare();
}

Editor::~Editor()
{
  for(const Operation &op : m_DeferredConstants)
    AddConstant(op);
  m_DeferredConstants.clear();

  m_ExternalSPIRV.clear();
  m_ExternalSPIRV.reserve(m_SPIRV.size());

  // copy into m_ExternalSPIRV, but skipping nops
  auto it = m_SPIRV.begin();

  for(size_t i = 0; i < FirstRealWord; ++i, ++it)
    m_ExternalSPIRV.push_back(*it);

  while(it != m_SPIRV.end())
  {
    if(*it == OpNopWord)
    {
      ++it;
      continue;
    }

    uint32_t len = *it >> WordCountShift;

    if(len == 0)
    {
      RDCERR("Malformed SPIR-V");
      break;
    }

    m_ExternalSPIRV.append(it, len);
    it += len;
  }
}

Id Editor::MakeId()
{
  uint32_t ret = m_SPIRV[3];
  m_SPIRV[3]++;
  Processor::UpdateMaxID(m_SPIRV[3]);
  return Id::fromWord(ret);
}

void Editor::DecorateStorageBufferStruct(Id id)
{
  // set bufferblock if needed
  if(m_StorageBufferClass == rdcspv::StorageClass::Uniform)
    AddDecoration(rdcspv::OpDecorate(id, rdcspv::Decoration::BufferBlock));
  else
    AddDecoration(rdcspv::OpDecorate(id, rdcspv::Decoration::Block));
}

void Editor::SetName(Id id, const rdcstr &name)
{
  Operation op = OpName(id, name);

  Iter it;

  // OpName/OpMemberName must be before OpModuleProcessed.
  for(it = Begin(Section::DebugNames); it < End(Section::DebugNames); ++it)
  {
    if(it.opcode() == Op::ModuleProcessed)
      break;
  }

  op.insertInto(m_SPIRV, it.offs());
  RegisterOp(Iter(m_SPIRV, it.offs()));
  addWords(it.offs(), op.size());
}

void Editor::SetMemberName(Id id, uint32_t member, const rdcstr &name)
{
  Operation op = OpMemberName(id, member, name);

  size_t offset = m_Sections[Section::DebugNames].endOffset;
  op.insertInto(m_SPIRV, offset);
  RegisterOp(Iter(m_SPIRV, offset));
  addWords(offset, op.size());
}

void Editor::AddDecoration(const Operation &op)
{
  size_t offset = m_Sections[Section::Annotations].endOffset;
  op.insertInto(m_SPIRV, offset);
  RegisterOp(Iter(m_SPIRV, offset));
  addWords(offset, op.size());
}

void Editor::AddCapability(Capability cap)
{
  // don't add duplicate capabilities
  if(HasCapability(cap))
    return;

  // insert the operation at the very start
  Operation op(Op::Capability, {(uint32_t)cap});
  op.insertInto(m_SPIRV, FirstRealWord);
  RegisterOp(Iter(m_SPIRV, FirstRealWord));
  addWords(FirstRealWord, op.size());
}

bool Editor::HasCapability(Capability cap)
{
  return capabilities.find(cap) != capabilities.end();
}

void Editor::AddExtension(const rdcstr &extension)
{
  // don't add duplicate extensions
  if(extensions.find(extension) != extensions.end())
    return;

  // start at the beginning
  Iter it(m_SPIRV, FirstRealWord);

  // skip past any capabilities
  while(it.opcode() == Op::Capability)
    it++;

  // insert the extension instruction
  size_t sz = extension.size();
  rdcarray<uint32_t> uintName;
  uintName.resize((sz / 4) + 1);
  memcpy(&uintName[0], extension.c_str(), sz);

  Operation op(Op::Extension, uintName);
  op.insertInto(m_SPIRV, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());
}

void Editor::AddExecutionMode(const Operation &mode)
{
  size_t offset = m_Sections[Section::ExecutionMode].endOffset;

  mode.insertInto(m_SPIRV, offset);
  RegisterOp(Iter(m_SPIRV, offset));
  addWords(offset, mode.size());
}

Id Editor::HasExtInst(const char *setname)
{
  for(auto it = extSets.begin(); it != extSets.end(); ++it)
  {
    if(it->second == setname)
      return it->first;
  }

  return Id();
}

Id Editor::ImportExtInst(const char *setname)
{
  for(auto it = extSets.begin(); it != extSets.end(); ++it)
  {
    if(it->second == setname)
      return it->first;
  }

  // start at the beginning
  Iter it(m_SPIRV, FirstRealWord);

  // skip past any capabilities and extensions
  while(it.opcode() == Op::Capability || it.opcode() == Op::Extension)
    it++;

  // insert the import instruction
  Id ret = MakeId();

  size_t sz = strlen(setname);
  rdcarray<uint32_t> uintName;
  uintName.resize((sz / 4) + 1);
  memcpy(&uintName[0], setname, sz);

  uintName.insert(0, ret.value());

  Operation op(Op::ExtInstImport, uintName);
  op.insertInto(m_SPIRV, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());

  extSets[ret] = setname;

  return ret;
}

Id Editor::AddType(const Operation &op)
{
  size_t offset = m_Sections[Section::Types].endOffset;

  Id id = Id::fromWord(op[1]);
  op.insertInto(m_SPIRV, offset);
  RegisterOp(Iter(m_SPIRV, offset));
  addWords(offset, op.size());
  return id;
}

Id Editor::AddVariable(const Operation &op)
{
  size_t offset = m_Sections[Section::Variables].endOffset;

  Id id = Id::fromWord(op[2]);
  op.insertInto(m_SPIRV, offset);
  RegisterOp(Iter(m_SPIRV, offset));
  addWords(offset, op.size());
  return id;
}

Id Editor::AddConstant(const Operation &op)
{
  size_t offset = m_Sections[Section::Constants].endOffset;

  Id id = Id::fromWord(op[2]);
  op.insertInto(m_SPIRV, offset);
  RegisterOp(Iter(m_SPIRV, offset));
  addWords(offset, op.size());
  return id;
}

void Editor::AddFunction(const OperationList &ops)
{
  size_t offset = m_SPIRV.size();

  for(const Operation &op : ops)
    op.appendTo(m_SPIRV);

  RegisterOp(Iter(m_SPIRV, offset));
}

Iter Editor::GetID(Id id)
{
  size_t offs = idOffsets[id];

  if(offs)
    return Iter(m_SPIRV, offs);

  return Iter();
}

Iter Editor::GetEntry(Id id)
{
  Iter it(m_SPIRV, m_Sections[Section::EntryPoints].startOffset);
  Iter end(m_SPIRV, m_Sections[Section::EntryPoints].endOffset);

  while(it && it < end)
  {
    OpEntryPoint entry(it);

    if(entry.entryPoint == id)
      return it;
    it++;
  }

  return Iter();
}

rdcpair<Id, Id> Editor::AddBuiltinInputLoad(OperationList &ops, ShaderStage stage, BuiltIn builtin,
                                            Id type)
{
  BuiltinInputData &data = builtinInputs[builtin];

  Id ptrType = DeclareType(Pointer(type, StorageClass::Input));

  if(data.variable == Id())
  {
    const DataType &dataType = dataTypes[type];

    Id var = AddVariable(OpVariable(ptrType, MakeId(), StorageClass::Input));
    AddDecoration(OpDecorate(var, DecorationParam<Decoration::BuiltIn>(builtin)));

    // Fragment shader inputs that are signed or unsigned integers, integer vectors, or any
    // double-precision floating-point type must be decorated with Flat.
    if(dataType.scalar().type != Op::TypeFloat && stage == ShaderStage::Pixel)
      AddDecoration(OpDecorate(var, Decoration::Flat));

    builtinInputs[builtin] = {var, ptrType, {}};

    return {ops.add(OpLoad(type, MakeId(), var)), var};
  }

  Id varType = dataTypes[data.type].InnerType();

  Id ret;

  if(data.chain.empty())
  {
    ret = ops.add(OpLoad(varType, MakeId(), data.variable));
  }
  else
  {
    rdcarray<rdcspv::Id> chain;
    for(uint32_t accessIdx : data.chain)
      chain.push_back(AddConstantImmediate<uint32_t>(accessIdx));
    Id subElement = ops.add(OpAccessChain(ptrType, MakeId(), data.variable, chain));

    ret = ops.add(rdcspv::OpLoad(varType, MakeId(), subElement));
  }

  if(varType != type)
    ret = ops.add(rdcspv::OpBitcast(type, MakeId(), ret));

  return {ret, Id()};
}

Id Editor::DeclareStructType(const rdcarray<Id> &members)
{
  Id typeId = MakeId();
  AddType(OpTypeStruct(typeId, members));
  return typeId;
}

Id Editor::AddOperation(Iter iter, const Operation &op)
{
  if(!iter)
    return Id();

  // add op
  op.insertInto(m_SPIRV, iter.offs());

  // update offsets
  addWords(iter.offs(), op.size());

  return OpDecoder(iter).result;
}

Iter Editor::AddOperations(Iter iter, const OperationList &ops)
{
  for(const rdcspv::Operation &op : ops)
  {
    AddOperation(iter, op);
    ++iter;
  }

  return iter;
}

void Editor::RegisterOp(Iter it)
{
  Processor::RegisterOp(it);

  OpDecoder opdata(it);

  if(opdata.op == Op::TypeVoid || opdata.op == Op::TypeBool || opdata.op == Op::TypeInt ||
     opdata.op == Op::TypeFloat)
  {
    Scalar scalar(it);
    scalarTypeToId[scalar] = opdata.result;
  }
  else if(opdata.op == Op::TypeVector)
  {
    OpTypeVector decoded(it);
    vectorTypeToId[Vector(dataTypes[decoded.componentType].scalar(), decoded.componentCount)] =
        decoded.result;
  }
  else if(opdata.op == Op::TypeMatrix)
  {
    OpTypeMatrix decoded(it);
    matrixTypeToId[Matrix(dataTypes[decoded.columnType].vector(), decoded.columnCount)] =
        decoded.result;
  }
  else if(opdata.op == Op::TypeImage)
  {
    OpTypeImage decoded(it);
    imageTypeToId[Image(dataTypes[decoded.sampledType].scalar(), decoded.dim, decoded.depth,
                        decoded.arrayed, decoded.mS, decoded.sampled, decoded.imageFormat)] =
        decoded.result;
  }
  else if(opdata.op == Op::TypeSampler)
  {
    Sampler s;
    samplerTypeToId[s] = opdata.result;
  }
  else if(opdata.op == Op::TypeSampledImage)
  {
    OpTypeSampledImage decoded(it);
    sampledImageTypeToId[SampledImage(decoded.imageType)] = decoded.result;
  }
  else if(opdata.op == Op::TypePointer)
  {
    OpTypePointer decoded(it);
    pointerTypeToId[Pointer(decoded.type, decoded.storageClass)] = decoded.result;
  }
  else if(opdata.op == Op::TypeFunction)
  {
    OpTypeFunction decoded(it);
    functionTypeToId[FunctionType(decoded.returnType, decoded.parameters)] = decoded.result;
  }
  else if(opdata.op == Op::Decorate)
  {
    OpDecorate decorate(it);

    if(decorate.decoration == Decoration::DescriptorSet)
      bindings[decorate.target].set = decorate.decoration.descriptorSet;
    if(decorate.decoration == Decoration::Binding)
      bindings[decorate.target].binding = decorate.decoration.binding;
  }
}

void Editor::UnregisterOp(Iter it)
{
  Processor::UnregisterOp(it);

  OpDecoder opdata(it);

  if(opdata.op == Op::TypeVoid || opdata.op == Op::TypeBool || opdata.op == Op::TypeInt ||
     opdata.op == Op::TypeFloat)
  {
    Scalar scalar(it);
    scalarTypeToId.erase(scalar);
  }
  else if(opdata.op == Op::TypeVector)
  {
    OpTypeVector decoded(it);
    vectorTypeToId.erase(Vector(dataTypes[decoded.componentType].scalar(), decoded.componentCount));
  }
  else if(opdata.op == Op::TypeMatrix)
  {
    OpTypeMatrix decoded(it);
    matrixTypeToId.erase(Matrix(dataTypes[decoded.columnType].vector(), decoded.columnCount));
  }
  else if(opdata.op == Op::TypeImage)
  {
    OpTypeImage decoded(it);
    imageTypeToId.erase(Image(dataTypes[decoded.sampledType].scalar(), decoded.dim, decoded.depth,
                              decoded.arrayed, decoded.mS, decoded.sampled, decoded.imageFormat));
  }
  else if(opdata.op == Op::TypeSampler)
  {
    samplerTypeToId.erase(Sampler());
  }
  else if(opdata.op == Op::TypeSampledImage)
  {
    OpTypeSampledImage decoded(it);
    sampledImageTypeToId.erase(SampledImage(decoded.imageType));
  }
  else if(opdata.op == Op::TypePointer)
  {
    OpTypePointer decoded(it);
    pointerTypeToId.erase(Pointer(decoded.type, decoded.storageClass));
  }
  else if(opdata.op == Op::TypeFunction)
  {
    OpTypeFunction decoded(it);
    functionTypeToId.erase(FunctionType(decoded.returnType, decoded.parameters));
  }
  else if(opdata.op == Op::Decorate)
  {
    OpDecorate decorate(it);

    if(decorate.decoration == Decoration::DescriptorSet)
      bindings[decorate.target].set = Binding().set;
    if(decorate.decoration == Decoration::Binding)
      bindings[decorate.target].binding = Binding().binding;
  }
}

void Editor::RegisterBuiltinMembers(rdcspv::Id baseId, const rdcarray<uint32_t> chainSoFar,
                                    const DataType *type)
{
  uint32_t i = 0;
  for(const DataType::Child &member : type->children)
  {
    if(member.decorations.flags & Decorations::HasBuiltIn)
    {
      rdcarray<uint32_t> chain = chainSoFar;
      chain.push_back(i);
      builtinInputs[member.decorations.builtIn] = {baseId, member.type, chain};
    }
    i++;
  }
}

void Editor::PostParse()
{
  Processor::PostParse();

  for(const Variable &v : globals)
  {
    if(v.storage == StorageClass::Input)
    {
      if(decorations[v.id].flags & Decorations::HasBuiltIn)
      {
        builtinInputs[decorations[v.id].builtIn] = {v.id, v.type, {}};
      }
      else
      {
        RegisterBuiltinMembers(v.id, {}, &dataTypes[v.type]);
      }
    }
  }
}

void Editor::addWords(size_t offs, int32_t num)
{
  // look through every section, any that are >= this point, adjust the offsets
  // note that if we're removing words then any offsets pointing directly to the removed words
  // will go backwards - but they no longer have anywhere valid to point.
  for(LogicalSection &section : m_Sections)
  {
    // we have three cases to consider: either the offset matches start, is within (up to and
    // including end) or is outside the section.
    // We ensured during parsing that all sections were non-empty by adding nops if necessary, so we
    // don't have to worry about the situation where we can't decide if an insert is at the end of
    // one section or inside the next. Note this means we don't support inserting at the start of a
    // section.

    if(offs == section.startOffset)
    {
      // if the offset matches the start, we're appending at the end of the previous section so move
      // both
      section.startOffset += num;
      section.endOffset += num;
    }
    else if(offs > section.startOffset && offs <= section.endOffset)
    {
      // if the offset is in the section (up to and including the end) then we're inserting in this
      // section, so move the end only
      section.endOffset += num;
    }
    else if(section.startOffset >= offs)
    {
      // otherwise move both or neither depending on which side the offset is.
      section.startOffset += num;
      section.endOffset += num;
    }
  }

  // look through every id, and do the same
  for(size_t &o : idOffsets)
    if(o >= offs)
      o += num;
}

Operation Editor::MakeDeclaration(const Scalar &s)
{
  if(s.type == Op::TypeVoid)
    return OpTypeVoid(Id());
  else if(s.type == Op::TypeBool)
    return OpTypeBool(Id());
  else if(s.type == Op::TypeFloat)
    return OpTypeFloat(Id(), s.width);
  else if(s.type == Op::TypeInt)
    return OpTypeInt(Id(), s.width, s.signedness ? 1U : 0U);
  else
    return OpNop();
}

Operation Editor::MakeDeclaration(const Vector &v)
{
  return OpTypeVector(Id(), DeclareType(v.scalar), v.count);
}

Operation Editor::MakeDeclaration(const Matrix &m)
{
  return OpTypeMatrix(Id(), DeclareType(m.vector), m.count);
}

Operation Editor::MakeDeclaration(const Pointer &p)
{
  return OpTypePointer(Id(), p.storage, p.baseId);
}

Operation Editor::MakeDeclaration(const Image &i)
{
  return OpTypeImage(Id(), DeclareType(i.retType), i.dim, i.depth, i.arrayed, i.ms, i.sampled,
                     i.format);
}

Operation Editor::MakeDeclaration(const Sampler &s)
{
  return OpTypeSampler(Id());
}

Operation Editor::MakeDeclaration(const SampledImage &s)
{
  return OpTypeSampledImage(Id(), s.baseId);
}

Operation Editor::MakeDeclaration(const FunctionType &f)
{
  return OpTypeFunction(Id(), f.returnId, f.argumentIds);
}

#define TYPETABLE(StructType, variable)                                \
  template <>                                                          \
  std::map<StructType, Id> &Editor::GetTable<StructType>()             \
  {                                                                    \
    return variable;                                                   \
  }                                                                    \
  template <>                                                          \
  const std::map<StructType, Id> &Editor::GetTable<StructType>() const \
  {                                                                    \
    return variable;                                                   \
  }

TYPETABLE(Scalar, scalarTypeToId);
TYPETABLE(Vector, vectorTypeToId);
TYPETABLE(Matrix, matrixTypeToId);
TYPETABLE(Pointer, pointerTypeToId);
TYPETABLE(Image, imageTypeToId);
TYPETABLE(Sampler, samplerTypeToId);
TYPETABLE(SampledImage, sampledImageTypeToId);
TYPETABLE(FunctionType, functionTypeToId);

};    // namespace rdcspv

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"
#include "core/core.h"
#include "spirv_common.h"
#include "spirv_compile.h"

static void RemoveSection(rdcarray<uint32_t> &spirv, size_t offsets[rdcspv::Section::Count][2],
                          rdcspv::Section::Type section)
{
  rdcspv::Editor ed(spirv);

  ed.Prepare();

  for(rdcspv::Iter it = ed.Begin(section), end = ed.End(section); it < end; it++)
    ed.Remove(it);

  size_t oldLength = offsets[section][1] - offsets[section][0];

  // section will still contain a nop
  offsets[section][1] = offsets[section][0] + 4;

  // subsequent sections will be shorter by the length - 4, because a nop will still be inserted
  // as padding to ensure no section is truly empty.
  size_t delta = oldLength - 4;

  for(uint32_t s = section + 1; s < rdcspv::Section::Count; s++)
  {
    offsets[s][0] -= delta;
    offsets[s][1] -= delta;
  }
}

static void CheckSPIRV(rdcspv::Editor &ed, size_t offsets[rdcspv::Section::Count][2])
{
  for(uint32_t s = rdcspv::Section::First; s < rdcspv::Section::Count; s++)
  {
    INFO("Section " << s);
    CHECK(ed.Begin((rdcspv::Section::Type)s).offs() == offsets[s][0] / sizeof(uint32_t));
    CHECK(ed.End((rdcspv::Section::Type)s).offs() == offsets[s][1] / sizeof(uint32_t));
  }

  // should only be one entry point
  REQUIRE(ed.GetEntries().size() == 1);

  rdcspv::Id entryId = ed.GetEntries()[0].id;

  // check that the iterator places us precisely at the start of the functions section
  CHECK(ed.GetID(entryId).offs() == ed.Begin(rdcspv::Section::Functions).offs());
}

TEST_CASE("Test SPIR-V editor section handling", "[spirv]")
{
  rdcspv::Init();
  RenderDoc::Inst().RegisterShutdownFunction(&rdcspv::Shutdown);

  rdcspv::CompilationSettings settings;
  settings.entryPoint = "main";
  settings.lang = rdcspv::InputLanguage::VulkanGLSL;
  settings.stage = rdcspv::ShaderStage::Fragment;

  // simple shader that has at least something in every section
  rdcarray<rdcstr> sources = {
      R"(#version 450 core

#extension GL_EXT_shader_16bit_storage : require

layout(binding = 0) uniform block {
	float16_t val;
};

layout(location = 0) out vec4 col;

void main() {
  col = vec4(sin(gl_FragCoord.x)*float(val), 0, 0, 1);
}
)",
  };

  rdcarray<uint32_t> spirv;
  rdcstr errors = rdcspv::Compile(settings, sources, spirv);

  INFO("SPIR-V compilation - " << errors);

  // ensure that compilation succeeded
  REQUIRE(spirv.size() > 0);

  // these offsets may change if the compiler changes above. Verify manually with spirv-dis that
  // they should be updated.
  // For convenience the offsets are in bytes (which spirv-dis uses) and are converted in the loop
  // in CheckSPIRV.
  size_t offsets[rdcspv::Section::Count][2] = {
      // Capabilities
      {0x14, 0x24},
      // Extensions
      {0x24, 0x40},
      // ExtInst
      {0x40, 0x58},
      // MemoryModel
      {0x58, 0x64},
      // EntryPoints
      {0x64, 0x80},
      // ExecutionMode
      {0x80, 0x8c},
      // DebugStringSource
      {0x8c, 0xb8},
      // DebugNames
      {0xb8, 0x118},
      // DebugModuleProcessed (contains inserted nop)
      {0x118, 0x11c},
      // Annotations
      {0x11c, 0x17c},
      // TypesVariables
      {0x17c, 0x2a4},
      // Functions
      {0x2a4, 0x374},
  };

  SECTION("Check that SPIR-V is correct with no changes")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  // we remove all sections we consider optional in arbitrary order. We don't care about keeping the
  // SPIR-V valid all we're testing is the section offsets are correct.
  RemoveSection(spirv, offsets, rdcspv::Section::Extensions);

  SECTION("Check with extensions removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::DebugNames);

  SECTION("Check with debug names removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::DebugStringSource);

  SECTION("Check with debug strings/sources removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::DebugModuleProcessed);

  SECTION("Check with module processed removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::ExtInst);

  SECTION("Check with extension imports removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::ExecutionMode);

  SECTION("Check with execution mode removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::Annotations);

  SECTION("Check with annotations removed")
  {
    rdcspv::Editor ed(spirv);

    ed.Prepare();

    CheckSPIRV(ed, offsets);
  }
}

#endif
