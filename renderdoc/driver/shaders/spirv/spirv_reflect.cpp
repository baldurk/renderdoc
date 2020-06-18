/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "spirv_reflect.h"
#include <limits.h>
#include <algorithm>
#include "common/formatting.h"
#include "replay/replay_driver.h"
#include "spirv_editor.h"
#include "spirv_op_helpers.h"

void FillSpecConstantVariables(ResourceId shader, const rdcarray<ShaderConstant> &invars,
                               rdcarray<ShaderVariable> &outvars,
                               const rdcarray<SpecConstant> &specInfo)
{
  StandardFillCBufferVariables(shader, invars, outvars, bytebuf());

  RDCASSERTEQUAL(invars.size(), outvars.size());

  for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
    outvars[v].value.u64v[0] = invars[v].defaultValue;

  // find any actual values specified
  for(size_t i = 0; i < specInfo.size(); i++)
  {
    for(size_t v = 0; v < invars.size() && v < outvars.size(); v++)
    {
      if(specInfo[i].specID == invars[v].byteOffset)
      {
        outvars[v].value.u64v[0] = specInfo[i].value;
      }
    }
  }
}

void AddXFBAnnotations(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                       const char *entryName, rdcarray<uint32_t> &modSpirv, uint32_t &xfbStride)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  rdcarray<SigParameter> outsig = refl.outputSignature;
  rdcarray<SPIRVInterfaceAccess> outpatch = patchData.outputs;

  rdcspv::Id entryid;
  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName)
    {
      entryid = entry.id;
      break;
    }
  }

  bool hasXFB = false;

  for(rdcspv::Iter it = editor.Begin(rdcspv::Section::ExecutionMode);
      it < editor.End(rdcspv::Section::ExecutionMode); ++it)
  {
    rdcspv::OpExecutionMode execMode(it);

    if(execMode.entryPoint == entryid && execMode.mode == rdcspv::ExecutionMode::Xfb)
    {
      hasXFB = true;
      break;
    }
  }

  if(hasXFB)
  {
    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Annotations);
        it < editor.End(rdcspv::Section::Annotations); ++it)
    {
      // remove any existing xfb decorations
      if(it.opcode() == rdcspv::Op::Decorate)
      {
        rdcspv::OpDecorate decorate(it);

        if(decorate.decoration == rdcspv::Decoration::XfbBuffer ||
           decorate.decoration == rdcspv::Decoration::XfbStride)
        {
          editor.Remove(it);
        }
      }

      // offset is trickier, need to see if it'll match one we want later
      if((it.opcode() == rdcspv::Op::Decorate &&
          rdcspv::OpDecorate(it).decoration == rdcspv::Decoration::Offset) ||
         (it.opcode() == rdcspv::Op::MemberDecorate &&
          rdcspv::OpMemberDecorate(it).decoration == rdcspv::Decoration::Offset))
      {
        for(size_t i = 0; i < outsig.size(); i++)
        {
          if(outpatch[i].structID && it.opcode() == rdcspv::Op::MemberDecorate)
          {
            rdcspv::OpMemberDecorate decoded(it);

            if(decoded.structureType == outpatch[i].structID &&
               decoded.member == outpatch[i].structMemberIndex)
            {
              editor.Remove(it);
            }
          }
          else if(!outpatch[i].structID && it.opcode() == rdcspv::Op::Decorate)
          {
            rdcspv::OpDecorate decoded(it);

            if(decoded.target == outpatch[i].ID)
            {
              editor.Remove(it);
            }
          }
        }
      }
    }
  }
  else
  {
    editor.AddExecutionMode(rdcspv::OpExecutionMode(entryid, rdcspv::ExecutionMode::Xfb));
  }

  editor.AddCapability(rdcspv::Capability::TransformFeedback);

  // find the position output and move it to the front
  for(size_t i = 0; i < outsig.size(); i++)
  {
    if(outsig[i].systemValue == ShaderBuiltin::Position)
    {
      outsig.insert(0, outsig[i]);
      outsig.erase(i + 1);

      outpatch.insert(0, outpatch[i]);
      outpatch.erase(i + 1);
      break;
    }
  }

  for(size_t i = 0; i < outsig.size(); i++)
  {
    if(outpatch[i].isArraySubsequentElement)
    {
      // do not patch anything as we only patch the base array, but reserve space in the stride
    }
    else if(outpatch[i].structID && !outpatch[i].accessChain.empty())
    {
      editor.AddDecoration(
          rdcspv::OpMemberDecorate(outpatch[i].structID, outpatch[i].structMemberIndex,
                                   rdcspv::DecorationParam<rdcspv::Decoration::Offset>(xfbStride)));
    }
    else if(outpatch[i].ID)
    {
      editor.AddDecoration(rdcspv::OpDecorate(
          outpatch[i].ID, rdcspv::DecorationParam<rdcspv::Decoration::Offset>(xfbStride)));
    }

    // components always get promoted to at least 32-bit
    uint32_t compByteSize = RDCMAX(4U, VarTypeByteSize(outsig[i].varType));

    xfbStride += outsig[i].compCount * compByteSize;
  }

  std::set<rdcspv::Id> vars;

  for(size_t i = 0; i < outpatch.size(); i++)
  {
    if(outpatch[i].ID && !outpatch[i].isArraySubsequentElement &&
       vars.find(outpatch[i].ID) == vars.end())
    {
      editor.AddDecoration(rdcspv::OpDecorate(
          outpatch[i].ID, rdcspv::DecorationParam<rdcspv::Decoration::XfbBuffer>(0)));
      editor.AddDecoration(rdcspv::OpDecorate(
          outpatch[i].ID, rdcspv::DecorationParam<rdcspv::Decoration::XfbStride>(xfbStride)));
      vars.insert(outpatch[i].ID);
    }
  }
}

const int32_t INVALID_BIND = -INT_MAX;

template <typename T>
struct bindpair
{
  Bindpoint map;
  T bindres;

  bindpair() = default;
  bindpair(const Bindpoint &m, const T &res) : map(m), bindres(res) {}
  bool operator<(const bindpair &o) const
  {
    if(map.bindset != o.map.bindset)
      return map.bindset < o.map.bindset;

    // sort invalid/not set binds to the end
    if(map.bind == INVALID_BIND && o.map.bind == INVALID_BIND)    // equal
      return false;
    if(map.bind == INVALID_BIND)    // invalid bind not less than anything
      return false;
    if(o.map.bind == INVALID_BIND)    // anything is less than invalid bind
      return true;

    return map.bind < o.map.bind;
  }
};

typedef bindpair<ConstantBlock> cblockpair;
typedef bindpair<ShaderResource> shaderrespair;

static uint32_t GetDescSet(uint32_t set)
{
  return set == ~0U ? 0 : set;
}

static int32_t GetBinding(uint32_t binding)
{
  return binding == ~0U ? INVALID_BIND : (uint32_t)binding;
}

static bool IsStrippableBuiltin(rdcspv::BuiltIn builtin)
{
  return builtin == rdcspv::BuiltIn::PointSize || builtin == rdcspv::BuiltIn::ClipDistance ||
         builtin == rdcspv::BuiltIn::CullDistance;
}

static uint32_t CalculateMinimumByteSize(const rdcarray<ShaderConstant> &variables)
{
  if(variables.empty())
  {
    RDCERR("Unexpectedly empty array of shader constants!");
    return 0;
  }

  const ShaderConstant &last = variables.back();

  // find its offset
  uint32_t byteOffset = last.byteOffset;

  // arrays are easy
  if(last.type.descriptor.arrayByteStride > 0)
    return byteOffset + last.type.descriptor.arrayByteStride * last.type.descriptor.elements;

  if(last.type.members.empty())
  {
    // this is the last basic member
    // now calculate its size and return offset + size

    RDCASSERT(last.type.descriptor.elements <= 1);

    uint32_t basicTypeSize = 4;
    if(last.type.descriptor.type == VarType::Double)
      basicTypeSize = 8;

    uint32_t rows = last.type.descriptor.rows;
    uint32_t cols = last.type.descriptor.columns;

    // vectors are also easy
    if(rows == 1)
      return byteOffset + cols * basicTypeSize;
    if(cols == 1)
      return byteOffset + rows * basicTypeSize;

    // for matrices we need to pad 3-column or 3-row up to 4
    if(cols == 3 && last.type.descriptor.rowMajorStorage)
    {
      return byteOffset + rows * 4 * basicTypeSize;
    }
    else if(rows == 3 && !last.type.descriptor.rowMajorStorage)
    {
      return byteOffset + cols * 4 * basicTypeSize;
    }
    else
    {
      // otherwise, it's a simple size
      return byteOffset + rows * cols * basicTypeSize;
    }
  }
  else
  {
    // if this is a struct type, recurse
    return byteOffset + CalculateMinimumByteSize(last.type.members);
  }
}

// Some generators output command-line arguments as OpModuleProcessed
static bool HasCommandLineInModuleProcessed(rdcspv::Generator gen)
{
  return (gen == rdcspv::Generator::GlslangReferenceFrontEnd ||
          gen == rdcspv::Generator::ShadercoverGlslang);
}

namespace rdcspv
{
Reflector::Reflector()
{
}

void Reflector::Parse(const rdcarray<uint32_t> &spirvWords)
{
  Processor::Parse(spirvWords);
}

void Reflector::PreParse(uint32_t maxId)
{
  Processor::PreParse(maxId);

  strings.resize(idTypes.size());
}

void Reflector::RegisterOp(Iter it)
{
  Processor::RegisterOp(it);

  OpDecoder opdata(it);

  if(opdata.op == Op::String)
  {
    OpString string(it);

    strings[string.result] = string.string;
  }
  else if(opdata.op == Op::Name)
  {
    OpName name(it);

    // technically you could name a string - in that case we ignore the name
    if(strings[name.target].empty())
      strings[name.target] = name.name;
  }
  else if(opdata.op == Op::MemberName)
  {
    OpMemberName memberName(it);

    memberNames.push_back({memberName.type, memberName.member, memberName.name});
  }
  else if(opdata.op == Op::Variable)
  {
    OpVariable var(it);

    // variables are always pointers
    Id varType = dataTypes[var.resultType].InnerType();

    // if we don't have a name for this variable but it's a pointer to a struct that is named then
    // give the variable a name based on the type. This is a common pattern in GLSL for global
    // blocks, and since the variable is how we access commonly we should give it a recognisable
    // name.
    if(strings[var.result].empty() && dataTypes[varType].type == DataType::StructType &&
       !strings[varType].empty())
    {
      strings[var.result] = strings[varType] + "_var";
    }
  }
  else if(opdata.op == Op::ModuleProcessed)
  {
    OpModuleProcessed processed(it);

    if(HasCommandLineInModuleProcessed(m_Generator))
    {
      cmdline += " --" + processed.process;
    }
  }
  else if(opdata.op == Op::Source)
  {
    OpSource source(it);

    // glslang based tools output fake OpModuleProcessed comments at the start of pre-1.3
    // shaders source before OpModuleProcessed existed (in SPIR-V 1.1)
    if(m_MajorVersion == 1 && m_MinorVersion < 1 && HasCommandLineInModuleProcessed(m_Generator))
    {
      rdcstr &src = source.source;

      const char compileFlagPrefix[] = "// OpModuleProcessed ";
      const char endMarker[] = "#line 1\n";
      if(src.find(compileFlagPrefix) == 0)
      {
        // process compile flags
        int32_t nextLine = src.indexOf('\n');
        while(nextLine > 0)
        {
          bool finished = false;
          if(src.find(compileFlagPrefix) == 0)
          {
            size_t offs = sizeof(compileFlagPrefix) - 1;
            cmdline += " --" + src.substr(offs, nextLine - offs);
          }
          else if(src.find(endMarker) == 0)
          {
            finished = true;
          }
          else
          {
            RDCERR("Unexpected preamble line with OpModuleProcessed: %s",
                   src.substr(0, nextLine).c_str());
            break;
          }

          // erase this line
          src.erase(0, nextLine + 1);

          nextLine = src.indexOf('\n');

          if(finished)
            break;
        }
      }
    }

    sources.push_back({source.sourceLanguage, strings[source.file], source.source});
  }
  else if(opdata.op == Op::SourceContinued)
  {
    OpSourceContinued continued(it);

    sources.back().contents += continued.continuedSource;
  }
  else if(opdata.op == Op::Label)
  {
    curBlock = opdata.result;
  }
  else if(opdata.op == Op::LoopMerge)
  {
    loopBlocks.insert(curBlock);
  }
}

void Reflector::UnregisterOp(Iter it)
{
  RDCFATAL("Reflector should not be used for editing! UnregisterOp() call invalid");
}

void Reflector::PostParse()
{
  Processor::PostParse();

  // assign default names for types that we can
  for(auto it = dataTypes.begin(); it != dataTypes.end(); ++it)
  {
    Id id = it->first;
    DataType &type = it->second;

    type.name = strings[id];

    if(type.name.empty())
    {
      if(type.type == DataType::UnknownType)
      {
        // ignore
      }
      else if(type.scalar().type == Op::TypeVoid)
      {
        type.name = "void";
      }
      else if(type.scalar().type == Op::TypeBool)
      {
        type.name = "bool";
      }
      else if(type.type == DataType::StructType)
      {
        type.name = StringFormat::Fmt("struct%u", type.id.value());
      }
      else if(type.type == DataType::ArrayType)
      {
        // prefer the name
        rdcstr lengthName;

        if(type.length != Id())
        {
          lengthName = strings[type.length];

          // if not, use the constant value
          if(lengthName.empty())
            lengthName = StringiseConstant(type.length);

          // if not, it might be a spec constant, use the fallback
          if(lengthName.empty())
            lengthName = StringFormat::Fmt("_%u", type.length.value());
        }

        rdcstr basename = dataTypes[type.InnerType()].name;

        // arrays are inside-out, so we need to insert our new array length before the first array
        // length
        int arrayCharIdx = basename.indexOf('[');
        if(arrayCharIdx > 0)
        {
          type.name = StringFormat::Fmt("%s[%s]%s", basename.substr(0, arrayCharIdx).c_str(),
                                        lengthName.c_str(), basename.substr(arrayCharIdx).c_str());
        }
        else
        {
          type.name = StringFormat::Fmt("%s[%s]", dataTypes[type.InnerType()].name.c_str(),
                                        lengthName.c_str());
        }
      }
      else if(type.type < DataType::StructType)
      {
        type.name = ToStr(type.scalar().Type());

        if(type.type == DataType::VectorType)
        {
          type.name += StringFormat::Fmt("%u", type.vector().count);
        }
        else if(type.type == DataType::MatrixType)
        {
          type.name += StringFormat::Fmt("%ux%u", type.vector().count, type.matrix().count);
        }
      }
      else if(type.type == DataType::ImageType)
      {
        const Image &img = imageTypes[type.id];

        rdcstr name;

        switch(img.dim)
        {
          case Dim::_1D: name = "1D"; break;
          case Dim::_2D: name = "2D"; break;
          case Dim::_3D: name = "3D"; break;
          case Dim::Cube: name = "Cube"; break;
          case Dim::Rect: name = "Rect"; break;
          case Dim::SubpassData: name = "Subpass"; break;
          case Dim::Buffer: name = "Buffer"; break;
          case Dim::Invalid:
          case Dim::Max: name = "Invalid"; break;
        }

        name = ToStr(img.retType.Type()) + name;

        if(img.sampled == 2 && img.dim != Dim::SubpassData)
          name = "Storage" + name;

        if(img.ms)
          name += "MS";
        if(img.arrayed)
          name += "Array";

        type.name = StringFormat::Fmt("Image<%s>", name.c_str());
      }
      else if(type.type == DataType::SamplerType)
      {
        type.name = StringFormat::Fmt("sampler", type.id.value());
      }
      else if(type.type == DataType::SampledImageType)
      {
        type.name = StringFormat::Fmt("Sampled%s",
                                      dataTypes[sampledImageTypes[type.id].baseId].name.c_str());
      }
    }
  }

  // do default names for pointer types in a second pass, because they can point forward at structs
  // with higher IDs
  for(auto it = dataTypes.begin(); it != dataTypes.end(); ++it)
  {
    if(it->second.type == DataType::PointerType && it->second.name.empty())
      it->second.name = StringFormat::Fmt("%s*", dataTypes[it->second.InnerType()].name.c_str());
  }

  for(const MemberName &mem : memberNames)
    dataTypes[mem.id].children[mem.member].name = mem.name;

  memberNames.clear();
}

rdcarray<rdcstr> Reflector::EntryPoints() const
{
  rdcarray<rdcstr> ret;
  ret.reserve(entries.size());
  for(const EntryPoint &e : entries)
    ret.push_back(e.name);
  return ret;
}

ShaderStage Reflector::StageForEntry(const rdcstr &entryPoint) const
{
  for(const EntryPoint &e : entries)
    if(entryPoint == e.name)
      return MakeShaderStage(e.executionModel);
  return ShaderStage::Count;
}

void Reflector::MakeReflection(const GraphicsAPI sourceAPI, const ShaderStage stage,
                               const rdcstr &entryPoint, const rdcarray<SpecConstant> &specInfo,
                               ShaderReflection &reflection, ShaderBindpointMapping &mapping,
                               SPIRVPatchData &patchData) const
{
  // set global properties
  reflection.entryPoint = entryPoint;
  reflection.stage = stage;
  reflection.encoding = ShaderEncoding::SPIRV;
  reflection.rawBytes.assign((byte *)m_SPIRV.data(), m_SPIRV.size() * sizeof(uint32_t));

  CheckDebuggable(reflection.debugInfo.debuggable, reflection.debugInfo.debugStatus);

  const EntryPoint *entry = NULL;
  for(const EntryPoint &e : entries)
  {
    if(entryPoint == e.name)
    {
      entry = &e;
      break;
    }
  }

  if(!entry)
  {
    RDCERR("Entry point %s not found in module", entryPoint.c_str());
    return;
  }

  // pick up execution mode size
  if(stage == ShaderStage::Compute)
  {
    const EntryPoint &e = *entry;

    if(entry->executionModes.localSizeId.x != Id())
    {
      reflection.dispatchThreadsDimension[0] =
          EvaluateConstant(e.executionModes.localSizeId.x, specInfo).value.u.x;
      reflection.dispatchThreadsDimension[1] =
          EvaluateConstant(e.executionModes.localSizeId.y, specInfo).value.u.x;
      reflection.dispatchThreadsDimension[2] =
          EvaluateConstant(e.executionModes.localSizeId.z, specInfo).value.u.x;
    }
    else if(e.executionModes.localSize.x > 0)
    {
      reflection.dispatchThreadsDimension[0] = e.executionModes.localSize.x;
      reflection.dispatchThreadsDimension[1] = e.executionModes.localSize.y;
      reflection.dispatchThreadsDimension[2] = e.executionModes.localSize.z;
    }

    // vulkan spec says "If an object is decorated with the WorkgroupSize decoration, this must take
    // precedence over any execution mode set for LocalSize."
    for(auto it : constants)
    {
      const Constant &c = it.second;

      if(decorations[c.id].builtIn == BuiltIn::WorkgroupSize)
      {
        RDCASSERT(c.children.size() == 3);
        for(size_t i = 0; i < c.children.size() && i < 3; i++)
          reflection.dispatchThreadsDimension[i] =
              EvaluateConstant(c.children[i], specInfo).value.u.x;
      }
    }
  }
  else
  {
    reflection.dispatchThreadsDimension[0] = reflection.dispatchThreadsDimension[1] =
        reflection.dispatchThreadsDimension[2] = 0;
  }

  if(!cmdline.empty())
    reflection.debugInfo.compileFlags.flags = {{"@cmdline", cmdline}};

  for(size_t i = 0; i < sources.size(); i++)
  {
    switch(sources[i].lang)
    {
      case SourceLanguage::ESSL:
      case SourceLanguage::GLSL: reflection.debugInfo.encoding = ShaderEncoding::GLSL; break;
      case SourceLanguage::HLSL: reflection.debugInfo.encoding = ShaderEncoding::HLSL; break;
      case SourceLanguage::OpenCL_C:
      case SourceLanguage::OpenCL_CPP:
      case SourceLanguage::Unknown:
      case SourceLanguage::Invalid:
      case SourceLanguage::Max: break;
    }

    if(!sources[i].contents.empty())
      reflection.debugInfo.files.push_back({sources[i].name, sources[i].contents});
  }

  std::set<Id> usedIds;
  std::map<Id, std::set<uint32_t>> usedStructChildren;

  // build the static call tree from the entry point, and build a list of all IDs referenced
  {
    std::set<Id> processed;
    rdcarray<Id> pending;

    pending.push_back(entry->id);

    while(!pending.empty())
    {
      Id func = pending.back();
      pending.pop_back();

      processed.insert(func);

      ConstIter it(m_SPIRV, idOffsets[func]);

      while(it.opcode() != Op::FunctionEnd)
      {
        OpDecoder::ForEachID(it, [&usedIds](Id id, bool result) { usedIds.insert(id); });

        if(it.opcode() == Op::AccessChain || it.opcode() == Op::InBoundsAccessChain)
        {
          OpAccessChain access(it);

          // save top-level children referenced in structs
          if(dataTypes[dataTypes[idTypes[access.base]].InnerType()].type == DataType::StructType)
            usedStructChildren[access.base].insert(
                EvaluateConstant(access.indexes[0], specInfo).value.u.x);
        }

        if(it.opcode() == Op::FunctionCall)
        {
          OpFunctionCall call(it);

          if(processed.find(call.function) == processed.end())
            pending.push_back(call.function);
        }

        it++;
      }
    }
  }

  // arrays of elements, which can be appended to in any order and then sorted
  rdcarray<SigParameter> inputs;
  rdcarray<SigParameter> outputs;
  rdcarray<cblockpair> cblocks;
  rdcarray<shaderrespair> samplers, roresources, rwresources;

  // for pointer types, mapping of inner type ID to index in list (assigned sequentially)
  SparseIdMap<uint16_t> pointerTypes;

  // $Globals gathering - for GL global values
  ConstantBlock globalsblock;

  // specialisation constant gathering
  ConstantBlock specblock;

  for(const Variable &global : globals)
  {
    if(global.storage == StorageClass::Input || global.storage == StorageClass::Output)
    {
      // variable type must be a pointer of the same storage class
      RDCASSERT(dataTypes[global.type].type == DataType::PointerType);
      const DataType &baseType = dataTypes[dataTypes[global.type].InnerType()];

      const bool isInput = (global.storage == StorageClass::Input);

      rdcarray<SigParameter> &sigarray = (isInput ? inputs : outputs);

      // try to use the instance/variable name
      rdcstr name = strings[global.id];

      // for structs, use the type name
      if(name.empty() && baseType.type == DataType::StructType)
        name = baseType.name;

      // otherwise fall back to naming after the builtin or location
      if(name.empty())
      {
        if(decorations[global.id].flags & Decorations::HasBuiltIn)
          name = StringFormat::Fmt("_%s", ToStr(decorations[global.id].builtIn).c_str());
        else if(decorations[global.id].flags & Decorations::HasLocation)
          name = StringFormat::Fmt("_%s%u", isInput ? "input" : "output",
                                   decorations[global.id].location);
        else
          name = StringFormat::Fmt("_sig%u", global.id.value());
      }

      const bool used = usedIds.find(global.id) != usedIds.end();

      // we want to skip any members of the builtin interface block that are completely unused and
      // just came along for the ride (usually with gl_Position, but maybe declared and still
      // unused). This is meaningless in SPIR-V and just generates useless noise, but some compilers
      // from GLSL can generate the whole gl_PerVertex as a literal translation from the implicit
      // GLSL declaration.
      //
      // Some compilers generate global variables instead of members of a global struct. If this is
      // a directly decorated builtin variable which is never used, skip it
      if(IsStrippableBuiltin(decorations[global.id].builtIn) && !used)
        continue;

      // if this is a struct variable then either all members must be builtins, or none of them, as
      // per the SPIR-V Decoration rules:
      //
      // "When applied to a structure-type member, all members of that structure type must also be
      // decorated with BuiltIn. (No allowed mixing of built-in variables and non-built-in variables
      // within a single structure.)"
      //
      // Some old compilers might generate gl_PerVertex with unused variables having no decoration,
      // so to handle this case we treat a struct with any builtin members as if all are builtin -
      // which is still legal.
      if(baseType.type == DataType::StructType)
      {
        // look to see if this struct contains a builtin member
        bool hasBuiltins = false;
        for(size_t i = 0; i < baseType.children.size(); i++)
        {
          hasBuiltins = (baseType.children[i].decorations.builtIn != BuiltIn::Invalid);
          if(hasBuiltins)
            break;
        }

        // if this is the builtin struct, explode the struct and call AddSignatureParameter for each
        // member here, so we can skip unused children if we want
        if(hasBuiltins)
        {
          const std::set<uint32_t> &usedchildren = usedStructChildren[global.id];

          for(uint32_t i = 0; i < (uint32_t)baseType.children.size(); i++)
          {
            // skip this member if it's in a builtin struct but has no builtin decoration
            if(baseType.children[i].decorations.builtIn == BuiltIn::Invalid)
              continue;

            // skip this member if it's unused and of a type that is commonly included 'by accident'
            if(IsStrippableBuiltin(baseType.children[i].decorations.builtIn) &&
               usedchildren.find(i) == usedchildren.end())
              continue;

            rdcstr childname = name;

            if(!baseType.children[i].name.empty())
              childname += "." + baseType.children[i].name;
            else
              childname += StringFormat::Fmt(".child%zu", i);

            SPIRVInterfaceAccess patch;
            patch.accessChain = {i};

            uint32_t dummy = 0;
            AddSignatureParameter(isInput, stage, global.id, baseType.id, dummy, patch, childname,
                                  dataTypes[baseType.children[i].type],
                                  baseType.children[i].decorations, sigarray, patchData, specInfo);
          }

          // move on now, we've processed this global struct
          continue;
        }
      }

      uint32_t dummy = 0;
      AddSignatureParameter(isInput, stage, global.id, Id(), dummy, {}, name, baseType,
                            decorations[global.id], sigarray, patchData, specInfo);
    }
    else if(global.storage == StorageClass::Uniform ||
            global.storage == StorageClass::UniformConstant ||
            global.storage == StorageClass::AtomicCounter ||
            global.storage == StorageClass::StorageBuffer ||
            global.storage == StorageClass::PushConstant)
    {
      // variable type must be a pointer of the same storage class
      RDCASSERT(dataTypes[global.type].type == DataType::PointerType);
      RDCASSERT(dataTypes[global.type].pointerType.storage == global.storage);

      const DataType *varType = &dataTypes[dataTypes[global.type].InnerType()];

      // if the outer type is an array, get the length and peel it off.
      bool isArray = false;
      uint32_t arraySize = 1;
      if(varType->type == DataType::ArrayType)
      {
        isArray = true;
        // runtime arrays have no length
        if(varType->length != Id())
          arraySize = EvaluateConstant(varType->length, specInfo).value.u.x;
        else
          arraySize = ~0U;
        varType = &dataTypes[varType->InnerType()];
      }

      // new SSBOs are in the storage buffer class, previously they were in uniform with BufferBlock
      // decoration
      const bool ssbo = (global.storage == StorageClass::StorageBuffer) ||
                        (decorations[varType->id].flags & Decorations::BufferBlock);
      const bool pushConst = (global.storage == StorageClass::PushConstant);
      const bool atomicCounter = (global.storage == StorageClass::AtomicCounter);

      Bindpoint bindmap;
      // set something crazy so this doesn't overlap with a real buffer binding
      if(pushConst)
        bindmap.bindset = PushConstantBindSet;
      else
        bindmap.bindset = GetDescSet(decorations[global.id].set);

      bindmap.bind = GetBinding(decorations[global.id].binding);

      // On GL if we have a location, put that in as the bind. It will be overwritten
      // dynamically with the actual value.
      if(sourceAPI == GraphicsAPI::OpenGL && decorations[global.id].location != ~0U)
        bindmap.bind = -int32_t(decorations[global.id].location);

      bindmap.arraySize = isArray ? arraySize : 1;
      bindmap.used = usedIds.find(global.id) != usedIds.end();

      if(atomicCounter)
      {
        // GL style atomic counter variable
        RDCASSERT(sourceAPI == GraphicsAPI::OpenGL);

        ShaderResource res;

        res.isReadOnly = false;
        res.isTexture = false;
        res.name = strings[global.id];
        if(res.name.empty())
          res.name = varType->name;
        if(res.name.empty())
          res.name = StringFormat::Fmt("atomic%u", global.id.value());
        res.resType = TextureType::Buffer;

        res.variableType.descriptor.columns = 1;
        res.variableType.descriptor.rows = 1;
        res.variableType.descriptor.rowMajorStorage = false;
        res.variableType.descriptor.type = VarType::UInt;
        res.variableType.descriptor.name = varType->name;

        bindmap.bindset = 0;
        bindmap.bind = GetBinding(decorations[global.id].binding);

        rwresources.push_back(shaderrespair(bindmap, res));
      }
      else if(varType->IsOpaqueType())
      {
        // on Vulkan should never have elements that have no binding declared but are used. On GL we
        // should have gotten a location
        // above, which will be rewritten later when looking up the pipeline state since it's
        // mutable from draw to draw in theory.
        RDCASSERT(!bindmap.used || bindmap.bind != INVALID_BIND);

        // opaque type - buffers, images, etc
        ShaderResource res;

        res.name = strings[global.id];
        if(res.name.empty())
          res.name = StringFormat::Fmt("res%u", global.id.value());

        if(varType->type == DataType::SamplerType)
        {
          res.resType = TextureType::Unknown;
          res.isTexture = false;
          res.isReadOnly = true;

          samplers.push_back(shaderrespair(bindmap, res));
        }
        else
        {
          Id imageTypeId = varType->id;

          if(varType->type == DataType::SampledImageType)
            imageTypeId = sampledImageTypes[varType->id].baseId;

          const Image &imageType = imageTypes[imageTypeId];

          if(imageType.ms)
            res.resType =
                imageType.arrayed ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;
          else if(imageType.dim == rdcspv::Dim::_1D)
            res.resType = imageType.arrayed ? TextureType::Texture1DArray : TextureType::Texture1D;
          else if(imageType.dim == rdcspv::Dim::_2D)
            res.resType = imageType.arrayed ? TextureType::Texture2DArray : TextureType::Texture2D;
          else if(imageType.dim == rdcspv::Dim::Cube)
            res.resType =
                imageType.arrayed ? TextureType::TextureCubeArray : TextureType::TextureCube;
          else if(imageType.dim == rdcspv::Dim::_3D)
            res.resType = TextureType::Texture3D;
          else if(imageType.dim == rdcspv::Dim::Rect)
            res.resType = TextureType::TextureRect;
          else if(imageType.dim == rdcspv::Dim::Buffer)
            res.resType = TextureType::Buffer;
          else if(imageType.dim == rdcspv::Dim::SubpassData)
            res.resType = TextureType::Texture2D;

          res.isTexture = res.resType != TextureType::Buffer;
          res.isReadOnly = imageType.sampled != 2 || imageType.dim == rdcspv::Dim::SubpassData;

          res.variableType.descriptor.type = imageType.retType.Type();

          if(res.isReadOnly)
            roresources.push_back(shaderrespair(bindmap, res));
          else
            rwresources.push_back(shaderrespair(bindmap, res));
        }
      }
      else
      {
        if(varType->type != DataType::StructType)
        {
          // global loose variable - add to $Globals block
          RDCASSERT(varType->type == DataType::ScalarType || varType->type == DataType::VectorType ||
                    varType->type == DataType::MatrixType || varType->type == DataType::ArrayType);
          RDCASSERT(sourceAPI == GraphicsAPI::OpenGL);

          ShaderConstant constant;

          MakeConstantBlockVariable(constant, pointerTypes, *varType, strings[global.id],
                                    decorations[global.id], specInfo);

          if(isArray)
            constant.type.descriptor.elements = arraySize;
          else
            constant.type.descriptor.elements = 0;

          constant.byteOffset = decorations[global.id].location;

          globalsblock.variables.push_back(constant);
        }
        else
        {
          // on Vulkan should never have elements that have no binding declared but are used, unless
          // it's push constants (which is handled elsewhere). On GL we should have gotten a
          // location above, which will be rewritten later when looking up the pipeline state since
          // it's mutable from draw to draw in theory.
          RDCASSERT(!bindmap.used || pushConst || bindmap.bind != INVALID_BIND);

          if(ssbo)
          {
            ShaderResource res;

            res.isReadOnly = false;
            res.isTexture = false;
            res.name = strings[global.id];
            if(res.name.empty())
              res.name = StringFormat::Fmt("ssbo%u", global.id.value());
            res.resType = TextureType::Buffer;

            res.variableType.descriptor.columns = 0;
            res.variableType.descriptor.rows = 0;
            res.variableType.descriptor.rowMajorStorage = false;
            res.variableType.descriptor.rows = 0;
            res.variableType.descriptor.type = VarType::Float;
            res.variableType.descriptor.name = varType->name;

            MakeConstantBlockVariables(*varType, 0, 0, res.variableType.members, pointerTypes,
                                       specInfo);

            rwresources.push_back(shaderrespair(bindmap, res));
          }
          else
          {
            ConstantBlock cblock;

            cblock.name = strings[global.id];
            if(cblock.name.empty())
              cblock.name = StringFormat::Fmt("uniforms%u", global.id.value());
            cblock.bufferBacked = !pushConst;

            MakeConstantBlockVariables(*varType, 0, 0, cblock.variables, pointerTypes, specInfo);

            if(!varType->children.empty())
              cblock.byteSize = CalculateMinimumByteSize(cblock.variables);
            else
              cblock.byteSize = 0;

            cblocks.push_back(cblockpair(bindmap, cblock));
          }
        }
      }
    }
    else if(global.storage == StorageClass::Private ||
            global.storage == StorageClass::CrossWorkgroup ||
            global.storage == StorageClass::Workgroup)
    {
      // silently allow
    }
    else
    {
      RDCWARN("Unexpected storage class for global: %s", ToStr(global.storage).c_str());
    }
  }

  for(auto it : constants)
  {
    const Constant &c = it.second;
    if(decorations[c.id].specID != ~0U)
    {
      rdcstr name = strings[c.id];
      if(name.empty())
        name = StringFormat::Fmt("specID%u", decorations[c.id].specID);

      ShaderConstant spec;
      MakeConstantBlockVariable(spec, pointerTypes, dataTypes[c.type], name, decorations[c.id],
                                specInfo);
      spec.byteOffset = decorations[c.id].specID;
      spec.defaultValue = c.value.value.u64v[0];
      specblock.variables.push_back(spec);
    }
  }

  if(!specblock.variables.empty())
  {
    specblock.name = "Specialization Constants";
    specblock.bufferBacked = false;
    specblock.byteSize = 0;

    Bindpoint bindmap;

    // set something crazy so this doesn't overlap with a real buffer binding
    // also identify this as specialization constant data
    bindmap.bindset = SpecializationConstantBindSet;
    bindmap.bind = INVALID_BIND;
    bindmap.arraySize = 1;
    bindmap.used = true;

    // sort by spec IDs
    std::sort(specblock.variables.begin(), specblock.variables.end(),
              [](const ShaderConstant &a, const ShaderConstant &b) {
                return a.byteOffset < b.byteOffset;
              });

    cblocks.push_back(cblockpair(bindmap, specblock));
  }

  if(!globalsblock.variables.empty())
  {
    globalsblock.name = "$Globals";
    globalsblock.bufferBacked = false;
    globalsblock.byteSize = (uint32_t)globalsblock.variables.size();
    globalsblock.bindPoint = (int)cblocks.size();

    Bindpoint bindmap;
    bindmap.bindset = 0;
    bindmap.bind = INVALID_BIND;
    bindmap.arraySize = 1;
    bindmap.used = true;

    cblocks.push_back(cblockpair(bindmap, globalsblock));
  }

  // look for execution modes that affect the reflection and apply them
  {
    const EntryPoint &e = *entry;

    if(e.executionModes.depthMode == ExecutionModes::DepthGreater)
    {
      for(SigParameter &sig : outputs)
      {
        if(sig.systemValue == ShaderBuiltin::DepthOutput)
          sig.systemValue = ShaderBuiltin::DepthOutputGreaterEqual;
      }
    }
    else if(e.executionModes.depthMode == ExecutionModes::DepthLess)
    {
      for(SigParameter &sig : outputs)
      {
        if(sig.systemValue == ShaderBuiltin::DepthOutput)
          sig.systemValue = ShaderBuiltin::DepthOutputLessEqual;
      }
    }

    patchData.outTopo = e.executionModes.outTopo;
  }

  // sort system value semantics to the start of the list
  struct sig_param_sort
  {
    sig_param_sort(const rdcarray<SigParameter> &arr) : sigArray(arr) {}
    const rdcarray<SigParameter> &sigArray;

    bool operator()(const size_t idxA, const size_t idxB)
    {
      const SigParameter &a = sigArray[idxA];
      const SigParameter &b = sigArray[idxB];

      if(a.systemValue == b.systemValue)
      {
        if(a.regIndex != b.regIndex)
          return a.regIndex < b.regIndex;

        if(a.regChannelMask != b.regChannelMask)
          return a.regChannelMask < b.regChannelMask;

        return a.varName < b.varName;
      }
      if(a.systemValue == ShaderBuiltin::Undefined)
        return false;
      if(b.systemValue == ShaderBuiltin::Undefined)
        return true;

      return a.systemValue < b.systemValue;
    }
  };

  rdcarray<size_t> indices;
  {
    indices.resize(inputs.size());
    for(size_t i = 0; i < inputs.size(); i++)
      indices[i] = i;

    std::sort(indices.begin(), indices.end(), sig_param_sort(inputs));

    reflection.inputSignature.reserve(inputs.size());
    for(size_t i = 0; i < inputs.size(); i++)
      reflection.inputSignature.push_back(inputs[indices[i]]);

    rdcarray<SPIRVInterfaceAccess> inPatch = patchData.inputs;
    for(size_t i = 0; i < inputs.size(); i++)
      patchData.inputs[i] = inPatch[indices[i]];
  }

  {
    indices.resize(outputs.size());
    for(size_t i = 0; i < outputs.size(); i++)
      indices[i] = i;

    std::sort(indices.begin(), indices.end(), sig_param_sort(outputs));

    reflection.outputSignature.reserve(outputs.size());
    for(size_t i = 0; i < outputs.size(); i++)
      reflection.outputSignature.push_back(outputs[indices[i]]);

    rdcarray<SPIRVInterfaceAccess> outPatch = patchData.outputs;
    for(size_t i = 0; i < outputs.size(); i++)
      patchData.outputs[i] = outPatch[indices[i]];
  }

  size_t numInputs = 16;

  for(size_t i = 0; i < reflection.inputSignature.size(); i++)
    if(reflection.inputSignature[i].systemValue == ShaderBuiltin::Undefined)
      numInputs = RDCMAX(numInputs, (size_t)reflection.inputSignature[i].regIndex + 1);

  mapping.inputAttributes.resize(numInputs);
  for(size_t i = 0; i < numInputs; i++)
    mapping.inputAttributes[i] = -1;

  for(size_t i = 0; i < reflection.inputSignature.size(); i++)
    if(reflection.inputSignature[i].systemValue == ShaderBuiltin::Undefined)
      mapping.inputAttributes[reflection.inputSignature[i].regIndex] = (int32_t)i;

  for(cblockpair &cb : cblocks)
  {
    // sort the variables within each block because we want them in offset order but they don't have
    // to be declared in offset order in the SPIR-V.
    std::sort(cb.bindres.variables.begin(), cb.bindres.variables.end());
  }

  std::sort(cblocks.begin(), cblocks.end());
  std::sort(samplers.begin(), samplers.end());
  std::sort(roresources.begin(), roresources.end());
  std::sort(rwresources.begin(), rwresources.end());

  mapping.constantBlocks.resize(cblocks.size());
  reflection.constantBlocks.resize(cblocks.size());

  mapping.samplers.resize(samplers.size());
  reflection.samplers.resize(samplers.size());

  mapping.readOnlyResources.resize(roresources.size());
  reflection.readOnlyResources.resize(roresources.size());

  mapping.readWriteResources.resize(rwresources.size());
  reflection.readWriteResources.resize(rwresources.size());

  for(size_t i = 0; i < cblocks.size(); i++)
  {
    mapping.constantBlocks[i] = cblocks[i].map;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(mapping.constantBlocks[i].bind == INVALID_BIND)
      mapping.constantBlocks[i].bind = 0;
    reflection.constantBlocks[i] = cblocks[i].bindres;
    reflection.constantBlocks[i].bindPoint = (int32_t)i;
  }

  for(size_t i = 0; i < samplers.size(); i++)
  {
    mapping.samplers[i] = samplers[i].map;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(mapping.samplers[i].bind == INVALID_BIND)
      mapping.samplers[i].bind = 0;
    reflection.samplers[i].name = samplers[i].bindres.name;
    reflection.samplers[i].bindPoint = (int32_t)i;
  }

  for(size_t i = 0; i < roresources.size(); i++)
  {
    mapping.readOnlyResources[i] = roresources[i].map;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(mapping.readOnlyResources[i].bind == INVALID_BIND)
      mapping.readOnlyResources[i].bind = 0;
    reflection.readOnlyResources[i] = roresources[i].bindres;
    reflection.readOnlyResources[i].bindPoint = (int32_t)i;
  }

  for(size_t i = 0; i < rwresources.size(); i++)
  {
    mapping.readWriteResources[i] = rwresources[i].map;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(mapping.readWriteResources[i].bind == INVALID_BIND)
      mapping.readWriteResources[i].bind = 0;
    reflection.readWriteResources[i] = rwresources[i].bindres;
    reflection.readWriteResources[i].bindPoint = (int32_t)i;
  }

  // populate the pointer types
  reflection.pointerTypes.reserve(pointerTypes.size());
  for(auto it = pointerTypes.begin(); it != pointerTypes.end(); ++it)
  {
    ShaderConstant dummy;

    MakeConstantBlockVariable(dummy, pointerTypes, dataTypes[it->first], rdcstr(), Decorations(),
                              specInfo);

    if(it->second >= reflection.pointerTypes.size())
      reflection.pointerTypes.resize(it->second + 1);

    reflection.pointerTypes[it->second] = dummy.type;
  }
}

void Reflector::MakeConstantBlockVariables(const DataType &structType, uint32_t arraySize,
                                           uint32_t arrayByteStride, rdcarray<ShaderConstant> &cblock,
                                           SparseIdMap<uint16_t> &pointerTypes,
                                           const rdcarray<SpecConstant> &specInfo) const
{
  // we get here for multi-dimensional arrays
  if(structType.type == DataType::ArrayType)
  {
    uint32_t relativeOffset = 0;

    if(arraySize == ~0U)
      arraySize = 1;

    cblock.resize(arraySize);
    for(uint32_t i = 0; i < arraySize; i++)
    {
      MakeConstantBlockVariable(cblock[i], pointerTypes, structType, StringFormat::Fmt("[%u]", i),
                                decorations[structType.id], specInfo);

      cblock[i].byteOffset = relativeOffset;

      relativeOffset += arrayByteStride;
    }

    return;
  }

  if(structType.children.empty())
    return;

  cblock.resize(structType.children.size());
  for(size_t i = 0; i < structType.children.size(); i++)
    MakeConstantBlockVariable(cblock[i], pointerTypes, dataTypes[structType.children[i].type],
                              structType.children[i].name, structType.children[i].decorations,
                              specInfo);
}

void Reflector::MakeConstantBlockVariable(ShaderConstant &outConst,
                                          SparseIdMap<uint16_t> &pointerTypes, const DataType &type,
                                          const rdcstr &name, const Decorations &varDecorations,
                                          const rdcarray<SpecConstant> &specInfo) const
{
  outConst.name = name;
  outConst.defaultValue = 0;

  if(varDecorations.offset != ~0U)
    outConst.byteOffset = varDecorations.offset;

  const DataType *curType = &type;

  // if the type is an array, set array size and strides then unpeel the array
  if(curType->type == DataType::ArrayType)
  {
    outConst.type.descriptor.elements =
        curType->length != Id() ? EvaluateConstant(curType->length, specInfo).value.u.x : ~0U;

    if(varDecorations.arrayStride != ~0U)
    {
      RDCASSERTMSG("Stride is too large for uint16_t", varDecorations.arrayStride <= 0xffff);
      outConst.type.descriptor.arrayByteStride = RDCMIN(varDecorations.arrayStride, 0xffffu) & 0xffff;
    }
    else if(decorations[curType->id].arrayStride != ~0U)
    {
      RDCASSERTMSG("Stride is too large for uint16_t",
                   decorations[curType->id].arrayStride <= 0xffff);
      outConst.type.descriptor.arrayByteStride =
          RDCMIN(decorations[curType->id].arrayStride, 0xffffu) & 0xffff;
    }

    if(varDecorations.matrixStride != ~0U)
      outConst.type.descriptor.matrixByteStride = varDecorations.matrixStride & 0xff;
    else if(decorations[curType->id].matrixStride != ~0U)
      outConst.type.descriptor.matrixByteStride = decorations[curType->id].matrixStride & 0xff;

    curType = &dataTypes[curType->InnerType()];
  }

  if(curType->type == DataType::VectorType || curType->type == DataType::MatrixType)
  {
    outConst.type.descriptor.type = curType->scalar().Type();

    outConst.type.descriptor.rowMajorStorage =
        (curType->type == DataType::VectorType || varDecorations.flags & Decorations::RowMajor);

    if(varDecorations.matrixStride != ~0U)
      outConst.type.descriptor.matrixByteStride = varDecorations.matrixStride & 0xff;

    if(curType->type == DataType::MatrixType)
    {
      outConst.type.descriptor.rows = (uint8_t)curType->vector().count;
      outConst.type.descriptor.columns = (uint8_t)curType->matrix().count;
    }
    else
    {
      outConst.type.descriptor.columns = (uint8_t)curType->vector().count;
    }

    outConst.type.descriptor.name = curType->name;
  }
  else if(curType->type == DataType::ScalarType)
  {
    outConst.type.descriptor.type = curType->scalar().Type();
    outConst.type.descriptor.rowMajorStorage = true;

    outConst.type.descriptor.name = curType->name;
  }
  else
  {
    if(curType->type == DataType::PointerType)
    {
      outConst.type.descriptor.type = VarType::ULong;
      outConst.type.descriptor.rowMajorStorage = false;
      outConst.type.descriptor.rows = 1;
      outConst.type.descriptor.columns = 1;
      outConst.type.descriptor.name = curType->name;

      // try to insert the inner type ID into the map. If it succeeds, it gets the next available
      // pointer type index (size of the map), if not then we just get the previously added index
      auto it =
          pointerTypes.insert(std::make_pair(curType->InnerType(), (uint16_t)pointerTypes.size()));

      outConst.type.descriptor.pointerTypeID = it.first->second;
      return;
    }

    RDCASSERT(curType->type == DataType::StructType || curType->type == DataType::ArrayType);

    outConst.type.descriptor.type = VarType::Float;
    outConst.type.descriptor.rowMajorStorage = false;
    outConst.type.descriptor.rows = 0;
    outConst.type.descriptor.columns = 0;

    outConst.type.descriptor.name = curType->name;

    MakeConstantBlockVariables(*curType, outConst.type.descriptor.elements,
                               outConst.type.descriptor.arrayByteStride, outConst.type.members,
                               pointerTypes, specInfo);

    if(curType->type == DataType::ArrayType)
    {
      // if the inner type is an array, it will be expanded in our members list. So don't also
      // redundantly keep the element count
      outConst.type.descriptor.elements = 1;
    }
  }
}

void Reflector::AddSignatureParameter(const bool isInput, const ShaderStage stage,
                                      const Id globalID, const Id parentStructID, uint32_t &regIndex,
                                      const SPIRVInterfaceAccess &parentPatch, const rdcstr &varName,
                                      const DataType &type, const Decorations &varDecorations,
                                      rdcarray<SigParameter> &sigarray, SPIRVPatchData &patchData,
                                      const rdcarray<SpecConstant> &specInfo) const
{
  SigParameter sig;

  sig.needSemanticIndex = false;

  SPIRVInterfaceAccess patch;
  patch.accessChain = parentPatch.accessChain;
  patch.ID = globalID;
  patch.structID = parentStructID;
  patch.isArraySubsequentElement = parentPatch.isArraySubsequentElement;
  if(parentStructID)
    patch.structMemberIndex = patch.accessChain.back();

  const bool rowmajor = (varDecorations.flags & Decorations::RowMajor) != 0;

  sig.regIndex = regIndex;

  if(varDecorations.location != ~0U)
    sig.regIndex = regIndex = varDecorations.location;

  if(varDecorations.builtIn != BuiltIn::Invalid)
    sig.systemValue = MakeShaderBuiltin(stage, varDecorations.builtIn);

  // fragment shader outputs are implicitly colour outputs. All other builtin outputs do not have a
  // register index
  if(stage == ShaderStage::Fragment && !isInput && sig.systemValue == ShaderBuiltin::Undefined)
    sig.systemValue = ShaderBuiltin::ColorOutput;
  else if(sig.systemValue != ShaderBuiltin::Undefined)
    sig.regIndex = 0;

  const DataType *varType = &type;

  bool isArray = false;
  uint32_t arraySize = 1;
  if(varType->type == DataType::ArrayType)
  {
    arraySize = EvaluateConstant(varType->length, specInfo).value.u.x;
    isArray = true;
    varType = &dataTypes[varType->InnerType()];

    // if this is the first array level, we sometimes ignore it.
    if(patch.accessChain.empty())
    {
      // for geometry/tessellation evaluation shaders, ignore the root level of array-ness for
      // inputs
      if((stage == ShaderStage::Geometry || stage == ShaderStage::Tess_Eval) && isInput)
        arraySize = 1;

      // for tessellation control shaders, ignore the root level of array-ness for both inputs and
      // outputs
      if(stage == ShaderStage::Tess_Control)
        arraySize = 1;

      // if this is a root array in the geometry shader, don't reflect it as an array either
      if(stage == ShaderStage::Geometry && isInput)
        isArray = false;
    }

    // arrays will need an extra access chain index
    patch.accessChain.push_back(0U);
  }

  // if the current type is a struct, recurse for each member
  if(varType->type == DataType::StructType)
  {
    for(uint32_t a = 0; a < arraySize; a++)
    {
      // push the member-index access chain value
      patch.accessChain.push_back(0U);

      for(size_t c = 0; c < varType->children.size(); c++)
      {
        rdcstr childName = varName;

        if(isArray)
          childName += StringFormat::Fmt("[%u]", a);

        if(!varType->children[c].name.empty())
          childName += "." + varType->children[c].name;
        else
          childName += StringFormat::Fmt(".child%zu", c);

        AddSignatureParameter(isInput, stage, globalID, varType->id, regIndex, patch, childName,
                              dataTypes[varType->children[c].type],
                              varType->children[c].decorations, sigarray, patchData, specInfo);

        // increment the member-index access chain value
        patch.accessChain.back()++;
      }

      // pop the member-index access chain value
      patch.accessChain.pop_back();

      // increment the array-index access chain value
      if(isArray)
      {
        patch.accessChain.back()++;
        patch.isArraySubsequentElement = true;
      }
    }

    return;
  }

  // similarly for arrays (this happens for multi-dimensional arrays
  if(varType->type == DataType::ArrayType)
  {
    for(uint32_t a = 0; a < arraySize; a++)
    {
      AddSignatureParameter(isInput, stage, globalID, Id(), regIndex, patch,
                            varName + StringFormat::Fmt("[%u]", a), *varType, {}, sigarray,
                            patchData, specInfo);

      // increment the array-index access chain value
      patch.accessChain.back()++;
      patch.isArraySubsequentElement = true;
    }

    return;
  }

  sig.varType = varType->scalar().Type();

  sig.compCount = RDCMAX(1U, varType->vector().count);
  sig.stream = 0;

  sig.regChannelMask = sig.channelUsedMask = (1 << sig.compCount) - 1;

  for(const DecorationAndParamData &d : varDecorations.others)
    if(d.value == Decoration::Component)
      sig.regChannelMask <<= d.component;

  sig.channelUsedMask = sig.regChannelMask;

  for(uint32_t a = 0; a < arraySize; a++)
  {
    rdcstr n = varName;

    if(isArray)
      n += StringFormat::Fmt("[%u]", a);

    sig.varName = n;

    if(varType->matrix().count <= 1)
    {
      sigarray.push_back(sig);

      regIndex++;

      if(isInput)
        patchData.inputs.push_back(patch);
      else
        patchData.outputs.push_back(patch);
    }
    else
    {
      // use an extra access chain to get each vector out of the matrix.
      patch.accessChain.push_back(0);

      for(uint32_t m = 0; m < varType->matrix().count; m++)
      {
        SigParameter s = sig;
        s.varName = StringFormat::Fmt("%s:%s%u", n.c_str(), rowmajor ? "row" : "col", m);
        s.regIndex += m;

        sigarray.push_back(s);

        if(isInput)
          patchData.inputs.push_back(patch);
        else
          patchData.outputs.push_back(patch);

        regIndex++;

        // increment the matrix column access chain
        patch.accessChain.back()++;
        patch.isArraySubsequentElement = true;
      }

      // pop the matrix column access chain
      patch.accessChain.pop_back();
    }

    sig.regIndex += RDCMAX(1U, varType->matrix().count);
    // increment the array index access chain (if it exists)
    if(isArray)
    {
      patch.accessChain.back()++;
      patch.isArraySubsequentElement = true;
    }
  }
}
};    // namespace rdcspv

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"
#include "data/glsl_shaders.h"
#include "glslang_compile.h"

TEST_CASE("Validate SPIR-V reflection", "[spirv][reflection]")
{
  ShaderType type = ShaderType::Vulkan;
  auto compiler = [&type](ShaderStage stage, const rdcstr &source, const rdcstr &entryPoint,
                          ShaderReflection &refl, ShaderBindpointMapping &mapping) {

    rdcspv::Init();
    RenderDoc::Inst().RegisterShutdownFunction(&rdcspv::Shutdown);

    rdcarray<uint32_t> spirv;
    rdcspv::CompilationSettings settings(type == ShaderType::Vulkan
                                             ? rdcspv::InputLanguage::VulkanGLSL
                                             : rdcspv::InputLanguage::OpenGLGLSL,
                                         rdcspv::ShaderStage(stage));
    settings.debugInfo = true;
    rdcstr errors = rdcspv::Compile(settings, {source}, spirv);

    INFO("SPIR-V compile output: " << errors);

    REQUIRE(!spirv.empty());

    rdcspv::Reflector spv;
    spv.Parse(spirv);

    SPIRVPatchData patchData;
    spv.MakeReflection(type == ShaderType::Vulkan ? GraphicsAPI::Vulkan : GraphicsAPI::OpenGL,
                       stage, entryPoint, {}, refl, mapping, patchData);
  };

  // test both Vulkan and GL SPIR-V reflection
  SECTION("Vulkan GLSL reflection")
  {
    type = ShaderType::Vulkan;
    TestGLSLReflection(type, compiler);
  };

  SECTION("OpenGL GLSL reflection")
  {
    type = ShaderType::GLSPIRV;
    TestGLSLReflection(type, compiler);
  };
}

#endif
