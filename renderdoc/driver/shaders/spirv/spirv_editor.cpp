/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
  else
  {
    width = 0;
    signedness = false;
  }
}

Operation Vector::decl(Editor &editor) const
{
  return OpTypeVector(Id(), editor.DeclareType(scalar), count);
}

Operation Matrix::decl(Editor &editor) const
{
  return OpTypeMatrix(Id(), editor.DeclareType(vector), count);
}

Operation Pointer::decl(Editor &editor) const
{
  return OpTypePointer(Id(), storage, baseId);
}

Operation Image::decl(Editor &editor) const
{
  return OpTypeImage(Id(), editor.DeclareType(retType), dim, depth, arrayed, ms, sampled, format);
}

Operation Sampler::decl(Editor &editor) const
{
  return OpTypeSampler(Id());
}

Operation SampledImage::decl(Editor &editor) const
{
  return OpTypeSampledImage(Id(), baseId);
}

Operation Function::decl(Editor &editor) const
{
  return OpTypeFunction(Id(), returnId, argumentIds);
}

Editor::Editor(std::vector<uint32_t> &spirvWords) : spirv(spirvWords)
{
  if(spirv.size() < FirstRealWord || spirv[0] != MagicNumber)
  {
    RDCERR("Empty or invalid SPIR-V module");
    return;
  }

  idOffsets.resize(spirv[3]);
  idTypes.resize(spirv[3]);

  // [4] is reserved
  RDCASSERT(spirv[4] == 0);

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
  sections[Section::Count - 1].endOffset = spirvWords.size();

#define START_SECTION(section)           \
  if(sections[section].startOffset == 0) \
    sections[section].startOffset = it.offs();

  for(Iter it(spirv, FirstRealWord); it; it++)
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
      if(sections[Section::Functions].startOffset == 0)
      {
        START_SECTION(Section::TypesVariablesConstants);
      }
    }

    RegisterOp(it);
  }

#undef START_SECTION

  // ensure we got everything right. First section should start at the beginning
  RDCASSERTEQUAL(sections[Section::First].startOffset, FirstRealWord);

  // we now set the endOffset of each section to the start of the next. Any empty sections
  // temporarily have startOffset set to endOffset, we'll pad them with a nop below.
  for(int s = Section::Count - 1; s > 0; s--)
  {
    RDCASSERTEQUAL(sections[s - 1].endOffset, 0);
    sections[s - 1].endOffset = sections[s].startOffset;
    if(sections[s - 1].startOffset == 0)
      sections[s - 1].startOffset = sections[s - 1].endOffset;
  }

  // find any empty sections and insert a nop into the stream there. We need to fixup later section
  // offsets by hand as addWords doesn't handle empty sections properly (it thinks we're inserting
  // into the later section by offset since the offsets overlap). That's why we're adding these
  // padding nops in the first place!
  for(uint32_t s = 0; s < Section::Count; s++)
  {
    if(sections[s].startOffset == sections[s].endOffset)
    {
      spirv.insert(spirv.begin() + sections[s].startOffset, OpNopWord);
      sections[s].endOffset++;

      for(uint32_t t = s + 1; t < Section::Count; t++)
      {
        sections[t].startOffset++;
        sections[t].endOffset++;
      }

      // look through every id, and update its offset
      for(size_t &o : idOffsets)
        if(o >= sections[s].startOffset)
          o++;
    }
  }

  // each section should now precisely match each other end-to-end and not be empty
  for(uint32_t s = Section::First; s < Section::Count; s++)
  {
    RDCASSERTNOTEQUAL(sections[s].startOffset, 0);
    RDCASSERTNOTEQUAL(sections[s].endOffset, 0);

    RDCASSERT(sections[s].endOffset - sections[s].startOffset > 0, sections[s].startOffset,
              sections[s].endOffset);

    if(s != 0)
      RDCASSERTEQUAL(sections[s - 1].endOffset, sections[s].startOffset);

    if(s + 1 < Section::Count)
      RDCASSERTEQUAL(sections[s].endOffset, sections[s + 1].startOffset);
  }
}

void Editor::StripNops()
{
  for(size_t i = FirstRealWord; i < spirv.size();)
  {
    while(spirv[i] == OpNopWord)
    {
      spirv.erase(spirv.begin() + i);
      addWords(i, -1);
    }

    uint32_t len = spirv[i] >> WordCountShift;

    if(len == 0)
    {
      RDCERR("Malformed SPIR-V");
      break;
    }

    i += len;
  }
}

Id Editor::MakeId()
{
  uint32_t ret = spirv[3];
  spirv[3]++;
  idOffsets.resize(spirv[3]);
  idTypes.resize(spirv[3]);
  return Id::fromWord(ret);
}

void Editor::SetName(Id id, const char *name)
{
  size_t sz = strlen(name);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], name, sz);

  uintName.insert(uintName.begin(), id.value());

  Operation op(Op::Name, uintName);

  Iter it;

  // OpName must be before OpModuleProcessed.
  for(it = Begin(Section::Debug); it < End(Section::Debug); ++it)
  {
    if(it.opcode() == Op::ModuleProcessed)
      break;
  }

  op.insertInto(spirv, it.offs());
  RegisterOp(Iter(spirv, it.offs()));
  addWords(it.offs(), op.size());
}

void Editor::AddDecoration(const Operation &op)
{
  size_t offset = sections[Section::Annotations].endOffset;
  op.insertInto(spirv, offset);
  RegisterOp(Iter(spirv, offset));
  addWords(offset, op.size());
}

void Editor::AddCapability(Capability cap)
{
  // don't add duplicate capabilities
  if(capabilities.find(cap) != capabilities.end())
    return;

  // insert the operation at the very start
  Operation op(Op::Capability, {(uint32_t)cap});
  op.insertInto(spirv, FirstRealWord);
  RegisterOp(Iter(spirv, FirstRealWord));
  addWords(FirstRealWord, op.size());
}

void Editor::AddExtension(const rdcstr &extension)
{
  // don't add duplicate extensions
  if(extensions.find(extension) != extensions.end())
    return;

  // start at the beginning
  Iter it(spirv, FirstRealWord);

  // skip past any capabilities
  while(it.opcode() == Op::Capability)
    it++;

  // insert the extension instruction
  size_t sz = extension.size();
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], extension.c_str(), sz);

  Operation op(Op::Extension, uintName);
  op.insertInto(spirv, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());
}

void Editor::AddExecutionMode(const Operation &mode)
{
  size_t offset = sections[Section::ExecutionMode].endOffset;

  mode.insertInto(spirv, offset);
  RegisterOp(Iter(spirv, offset));
  addWords(offset, mode.size());
}

Id Editor::ImportExtInst(const char *setname)
{
  Id ret = extSets[setname];

  if(ret)
    return ret;

  // start at the beginning
  Iter it(spirv, FirstRealWord);

  // skip past any capabilities and extensions
  while(it.opcode() == Op::Capability || it.opcode() == Op::Extension)
    it++;

  // insert the import instruction
  ret = MakeId();

  size_t sz = strlen(setname);
  std::vector<uint32_t> uintName((sz / 4) + 1);
  memcpy(&uintName[0], setname, sz);

  uintName.insert(uintName.begin(), ret.value());

  Operation op(Op::ExtInstImport, uintName);
  op.insertInto(spirv, it.offs());
  RegisterOp(it);
  addWords(it.offs(), op.size());

  extSets[setname] = ret;

  return ret;
}

Id Editor::AddType(const Operation &op)
{
  size_t offset = sections[Section::Types].endOffset;

  Id id = Id::fromWord(op[1]);
  idOffsets[id.value()] = offset;
  op.insertInto(spirv, offset);
  RegisterOp(Iter(spirv, offset));
  addWords(offset, op.size());
  return id;
}

Id Editor::AddVariable(const Operation &op)
{
  size_t offset = sections[Section::Variables].endOffset;

  Id id = Id::fromWord(op[2]);
  idOffsets[id.value()] = offset;
  op.insertInto(spirv, offset);
  RegisterOp(Iter(spirv, offset));
  addWords(offset, op.size());
  return id;
}

Id Editor::AddConstant(const Operation &op)
{
  size_t offset = sections[Section::Constants].endOffset;

  Id id = Id::fromWord(op[2]);
  idOffsets[id.value()] = offset;
  op.insertInto(spirv, offset);
  RegisterOp(Iter(spirv, offset));
  addWords(offset, op.size());
  return id;
}

void Editor::AddFunction(const Operation *ops, size_t count)
{
  idOffsets[ops[0][2]] = spirv.size();

  for(size_t i = 0; i < count; i++)
    ops[i].appendTo(spirv);

  RegisterOp(Iter(spirv, idOffsets[ops[0][2]]));
}

Iter Editor::GetID(Id id)
{
  size_t offs = idOffsets[id.value()];

  if(offs)
    return Iter(spirv, offs);

  return Iter();
}

Iter Editor::GetEntry(Id id)
{
  Iter it(spirv, sections[Section::EntryPoints].startOffset);
  Iter end(spirv, sections[Section::EntryPoints].endOffset);

  while(it && it < end)
  {
    OpEntryPoint entry(it);

    if(entry.entryPoint == id.value())
      return it;
    it++;
  }

  return Iter();
}

Id Editor::DeclareStructType(const std::vector<Id> &members)
{
  Id typeId = MakeId();
  AddType(OpTypeStruct(typeId, members));
  return typeId;
}

void Editor::AddOperation(Iter iter, const Operation &op)
{
  if(!iter)
    return;

  // add op
  op.insertInto(spirv, iter.offs());

  // update offsets
  addWords(iter.offs(), op.size());
}

void Editor::RegisterOp(Iter it)
{
  Op opcode = it.opcode();

  OpDecoder opdata(it);
  if(opdata.result != Id() && opdata.resultType != Id())
  {
    RDCASSERT(opdata.result.value() < idTypes.size());
    idTypes[opdata.result.value()] = opdata.resultType;
  }

  if(opdata.result != Id())
    idOffsets[opdata.result.value()] = it.offs();

  if(opcode == Op::EntryPoint)
  {
    entries.push_back(OpEntryPoint(it));
  }
  else if(opcode == Op::MemoryModel)
  {
    OpMemoryModel decoded(it);
    addressmodel = decoded.addressingModel;
    memorymodel = decoded.memoryModel;
  }
  else if(opcode == Op::Capability)
  {
    OpCapability decoded(it);
    capabilities.insert(decoded.capability);
  }
  else if(opcode == Op::Extension)
  {
    OpExtension decoded(it);
    extensions.insert(decoded.name);
  }
  else if(opcode == Op::ExtInstImport)
  {
    OpExtInstImport decoded(it);
    extSets[decoded.name] = decoded.result;
  }
  else if(opcode == Op::Function)
  {
    functions.push_back(opdata.result);
  }
  else if(opcode == Op::Variable)
  {
    variables.push_back(OpVariable(it));
  }
  else if(opcode == Op::Decorate)
  {
    OpDecorate decorate(it);

    auto it = std::lower_bound(decorations.begin(), decorations.end(), decorate,
                               [](const OpDecorate &a, const OpDecorate &b) { return a < b; });
    decorations.insert(it, decorate);

    if(decorate.decoration == Decoration::DescriptorSet)
      bindings[decorate.target].set = decorate.decoration.descriptorSet;
    if(decorate.decoration == Decoration::Binding)
      bindings[decorate.target].binding = decorate.decoration.binding;
  }
  else if(opcode == Op::TypeVoid || opcode == Op::TypeBool || opcode == Op::TypeInt ||
          opcode == Op::TypeFloat)
  {
    Scalar scalar(it);
    scalarTypes[scalar] = opdata.result;
  }
  else if(opcode == Op::TypeVector)
  {
    OpTypeVector decoded(it);

    Iter scalarIt = GetID(decoded.componentType);

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", decoded.componentType);
      return;
    }

    vectorTypes[Vector(scalarIt, decoded.componentCount)] = decoded.result;
  }
  else if(opcode == Op::TypeMatrix)
  {
    OpTypeMatrix decodedMatrix(it);

    Iter vectorIt = GetID(decodedMatrix.columnType);

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", decodedMatrix.columnType);
      return;
    }

    OpTypeVector decodedVector(vectorIt);

    Iter scalarIt = GetID(decodedVector.componentType);

    matrixTypes[Matrix(Vector(scalarIt, decodedVector.componentCount), decodedMatrix.columnCount)] =
        decodedMatrix.result;
  }
  else if(opcode == Op::TypeImage)
  {
    OpTypeImage decoded(it);

    Iter scalarIt = GetID(decoded.sampledType);

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", decoded.sampledType);
      return;
    }

    imageTypes[Image(scalarIt, decoded.dim, decoded.depth, decoded.arrayed, decoded.mS,
                     decoded.sampled, decoded.imageFormat)] = decoded.result;
  }
  else if(opcode == Op::TypeSampler)
  {
    samplerTypes[Sampler()] = opdata.result;
  }
  else if(opcode == Op::TypeSampledImage)
  {
    OpTypeSampledImage decoded(it);

    sampledImageTypes[SampledImage(decoded.imageType)] = decoded.result;
  }
  else if(opcode == Op::TypePointer)
  {
    OpTypePointer decoded(it);

    pointerTypes[Pointer(decoded.type, decoded.storageClass)] = decoded.result;
  }
  else if(opcode == Op::TypeStruct)
  {
    structTypes.insert(opdata.result);
  }
  else if(opcode == Op::TypeFunction)
  {
    OpTypeFunction decoded(it);

    functionTypes[Function(decoded.returnType, decoded.parameters)] = decoded.result;
  }
}

void Editor::UnregisterOp(Iter it)
{
  Op opcode = it.opcode();

  OpDecoder opdata(it);
  if(opdata.result != Id() && opdata.resultType != Id())
    idTypes[opdata.result.value()] = Id();

  if(opdata.result != Id())
    idOffsets[opdata.result.value()] = 0;

  if(opcode == Op::EntryPoint)
  {
    OpEntryPoint decoded(it);

    for(auto entryIt = entries.begin(); entryIt != entries.end(); ++entryIt)
    {
      if(entryIt->entryPoint == decoded.entryPoint)
      {
        entries.erase(entryIt);
        break;
      }
    }
  }
  else if(opcode == Op::Function)
  {
    for(auto funcIt = functions.begin(); funcIt != functions.end(); ++funcIt)
    {
      if(*funcIt == opdata.result)
      {
        functions.erase(funcIt);
        break;
      }
    }
  }
  else if(opcode == Op::Variable)
  {
    for(auto varIt = variables.begin(); varIt != variables.end(); ++varIt)
    {
      if(varIt->result == opdata.result)
      {
        variables.erase(varIt);
        break;
      }
    }
  }
  else if(opcode == Op::Decorate)
  {
    OpDecorate decorate(it);

    auto it = std::lower_bound(decorations.begin(), decorations.end(), decorate,
                               [](const OpDecorate &a, const OpDecorate &b) { return a < b; });
    if(it != decorations.end() && *it == decorate)
      decorations.erase(it);

    if(decorate.decoration == Decoration::DescriptorSet)
      bindings[decorate.target].set = Binding().set;
    if(decorate.decoration == Decoration::Binding)
      bindings[decorate.target].binding = Binding().binding;
  }
  else if(opcode == Op::Capability)
  {
    OpCapability decoded(it);
    capabilities.erase(decoded.capability);
  }
  else if(opcode == Op::Extension)
  {
    OpExtension decoded(it);
    extensions.erase(decoded.name);
  }
  else if(opcode == Op::ExtInstImport)
  {
    OpExtInstImport decoded(it);
    extSets.erase(decoded.name);
  }
  else if(opcode == Op::TypeVoid || opcode == Op::TypeBool || opcode == Op::TypeInt ||
          opcode == Op::TypeFloat)
  {
    Scalar scalar(it);
    scalarTypes.erase(scalar);
  }
  else if(opcode == Op::TypeVector)
  {
    OpTypeVector decoded(it);

    Iter scalarIt = GetID(decoded.componentType);

    if(!scalarIt)
    {
      RDCERR("Vector type declared with unknown scalar component type %u", decoded.componentType);
      return;
    }

    vectorTypes.erase(Vector(scalarIt, decoded.componentCount));
  }
  else if(opcode == Op::TypeMatrix)
  {
    OpTypeMatrix decodedMatrix(it);

    Iter vectorIt = GetID(decodedMatrix.columnType);

    if(!vectorIt)
    {
      RDCERR("Matrix type declared with unknown vector component type %u", decodedMatrix.columnType);
      return;
    }

    OpTypeVector decodedVector(vectorIt);

    Iter scalarIt = GetID(decodedVector.componentType);

    matrixTypes.erase(
        Matrix(Vector(scalarIt, decodedVector.componentCount), decodedMatrix.columnCount));
  }
  else if(opcode == Op::TypeImage)
  {
    OpTypeImage decoded(it);

    Iter scalarIt = GetID(decoded.sampledType);

    if(!scalarIt)
    {
      RDCERR("Image type declared with unknown scalar component type %u", decoded.sampledType);
      return;
    }

    imageTypes.erase(Image(scalarIt, decoded.dim, decoded.depth, decoded.arrayed, decoded.mS,
                           decoded.sampled, decoded.imageFormat));
  }
  else if(opcode == Op::TypeSampler)
  {
    samplerTypes.erase(Sampler());
  }
  else if(opcode == Op::TypeSampledImage)
  {
    OpTypeSampledImage decoded(it);

    sampledImageTypes.erase(SampledImage(decoded.imageType));
  }
  else if(opcode == Op::TypePointer)
  {
    OpTypePointer decoded(it);

    pointerTypes.erase(Pointer(decoded.type, decoded.storageClass));
  }
  else if(opcode == Op::TypeStruct)
  {
    structTypes.erase(opdata.result);
  }
  else if(opcode == Op::TypeFunction)
  {
    OpTypeFunction decoded(it);

    functionTypes.erase(Function(decoded.returnType, decoded.parameters));
  }
}

void Editor::addWords(size_t offs, int32_t num)
{
  // look through every section, any that are >= this point, adjust the offsets
  // note that if we're removing words then any offsets pointing directly to the removed words
  // will go backwards - but they no longer have anywhere valid to point.
  for(LogicalSection &section : sections)
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

TYPETABLE(Scalar, scalarTypes);
TYPETABLE(Vector, vectorTypes);
TYPETABLE(Matrix, matrixTypes);
TYPETABLE(Pointer, pointerTypes);
TYPETABLE(Image, imageTypes);
TYPETABLE(Sampler, samplerTypes);
TYPETABLE(SampledImage, sampledImageTypes);
TYPETABLE(Function, functionTypes);

};    // namespace rdcspv

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"
#include "core/core.h"
#include "spirv_common.h"
#include "spirv_compile.h"

static void RemoveSection(std::vector<uint32_t> &spirv, size_t offsets[rdcspv::Section::Count][2],
                          rdcspv::Section::Type section)
{
  rdcspv::Editor ed(spirv);

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

  rdcspv::Id entryId = ed.GetEntries()[0].entryPoint;

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
  std::vector<std::string> sources = {
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

  std::vector<uint32_t> spirv;
  std::string errors = rdcspv::Compile(settings, sources, spirv);

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
      // Debug
      {0x8c, 0x118},
      // Annotations
      {0x118, 0x178},
      // TypesVariables
      {0x178, 0x2a0},
      // Functions
      {0x2a0, 0x370},
  };

  SECTION("Check that SPIR-V is correct with no changes")
  {
    rdcspv::Editor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  // we remove all sections we consider optional in arbitrary order. We don't care about keeping the
  // SPIR-V valid all we're testing is the section offsets are correct.
  RemoveSection(spirv, offsets, rdcspv::Section::Extensions);

  SECTION("Check with extensions removed")
  {
    rdcspv::Editor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::Debug);

  SECTION("Check with debug removed")
  {
    rdcspv::Editor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::ExtInst);

  SECTION("Check with extension imports removed")
  {
    rdcspv::Editor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::ExecutionMode);

  SECTION("Check with execution mode removed")
  {
    rdcspv::Editor ed(spirv);

    CheckSPIRV(ed, offsets);
  }

  RemoveSection(spirv, offsets, rdcspv::Section::Annotations);

  SECTION("Check with annotations removed")
  {
    rdcspv::Editor ed(spirv);

    CheckSPIRV(ed, offsets);
  }
}

#endif