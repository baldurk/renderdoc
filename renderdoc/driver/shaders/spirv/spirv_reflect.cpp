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

#include "spirv_reflect.h"
#include <limits.h>
#include <algorithm>
#include "common/formatting.h"
#include "replay/replay_driver.h"
#include "spirv_editor.h"
#include "spirv_op_helpers.h"

void StripCommonGLPrefixes(rdcstr &name)
{
  // remove certain common prefixes that generate only useless noise from GLSL. This is mostly
  // irrelevant for the majority of cases but is primarily relevant for single-component outputs
  // like gl_PointSize or gl_CullPrimitiveEXT
  const rdcstr prefixesToRemove[] = {
      "gl_PerVertex.",           "gl_PerVertex_var.",     "gl_MeshVerticesEXT.",
      "gl_MeshVerticesEXT_var.", "gl_MeshPrimitivesEXT.", "gl_MeshPrimitivesEXT_var.",
  };

  for(const rdcstr &prefix : prefixesToRemove)
  {
    int offs = name.find(prefix);
    if(offs == 0)
      name.erase(0, prefix.length());
  }
}

void FillSpecConstantVariables(ResourceId shader, const SPIRVPatchData &patchData,
                               const rdcarray<ShaderConstant> &invars,
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
      int32_t idx = patchData.specIDs.indexOf(specInfo[i].specID);
      if(idx == -1)
        continue;

      if(idx * sizeof(uint64_t) == invars[v].byteOffset)
      {
        outvars[v].value.u64v[0] = specInfo[i].value;
      }
    }
  }
}

void AddXFBAnnotations(const ShaderReflection &refl, const SPIRVPatchData &patchData,
                       uint32_t rastStream, const char *entryName, rdcarray<uint32_t> &modSpirv,
                       uint32_t &xfbStride)
{
  rdcspv::Editor editor(modSpirv);

  editor.Prepare();

  rdcarray<SigParameter> outsig = refl.outputSignature;
  rdcarray<SPIRVInterfaceAccess> outpatch = patchData.outputs;

  rdcspv::Id entryid;
  for(const rdcspv::EntryPoint &entry : editor.GetEntries())
  {
    if(entry.name == entryName && MakeShaderStage(entry.executionModel) == refl.stage)
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
           decorate.decoration == rdcspv::Decoration::XfbStride ||
           decorate.decoration == rdcspv::Decoration::Stream)
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
    // ignore anything from the non-rasterized stream
    if(outsig[i].stream != rastStream)
      continue;

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
    // ignore anything from the non-rasterized stream
    if(outsig[i].stream != rastStream)
      continue;

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

  // if the rasterized stream isn't 0 we need to patch any stream emits to emit 0 instead of
  // rastStream, and drop any that used to go to other streams
  if(hasXFB && rastStream != 0)
  {
    rdcspv::Id newStream = editor.AddConstantImmediate<uint32_t>(0U);

    for(rdcspv::Iter it = editor.Begin(rdcspv::Section::Functions);
        it < editor.End(rdcspv::Section::Functions); ++it)
    {
      if(it.opcode() == rdcspv::Op::EmitStreamVertex || it.opcode() == rdcspv::Op::EndStreamPrimitive)
      {
        // same format for both
        rdcspv::OpEmitStreamVertex emit(it);

        ShaderVariable stream = editor.EvaluateConstant(emit.stream, {});

        // if this was emitting to our stream, still emit but to the new stream index. If it wasn't,
        // drop it entirely
        if(stream.value.u32v[0] == rastStream)
          it.word(1) = newStream.value();
        else
          editor.Remove(it);
      }
    }
  }
}

const uint32_t INVALID_BIND = ~0U;

template <typename T>
struct sortedbind
{
  T bindres;
  rdcspv::Id id;

  sortedbind() = default;
  sortedbind(rdcspv::Id id, const T &res) : bindres(res), id(id) {}
  bool operator<(const sortedbind &o) const
  {
    if(bindres.fixedBindSetOrSpace != o.bindres.fixedBindSetOrSpace)
      return bindres.fixedBindSetOrSpace < o.bindres.fixedBindSetOrSpace;

    // sort invalid/not set binds to the end
    if(bindres.fixedBindNumber == INVALID_BIND && o.bindres.fixedBindNumber == INVALID_BIND)    // equal
      return false;
    if(bindres.fixedBindNumber == INVALID_BIND)    // invalid bind not less than anything
      return false;
    if(o.bindres.fixedBindNumber == INVALID_BIND)    // anything is less than invalid bind
      return true;

    return bindres.fixedBindNumber < o.bindres.fixedBindNumber;
  }
};

typedef sortedbind<ConstantBlock> sortedcblock;
typedef sortedbind<ShaderResource> sortedres;
typedef sortedbind<ShaderSampler> sortedsamp;

static uint32_t GetDescSet(uint32_t set)
{
  return set == ~0U ? 0 : set;
}

static int32_t GetBinding(uint32_t binding)
{
  return binding == ~0U ? INVALID_BIND : (uint32_t)binding;
}

static bool IsStrippableBuiltin(rdcspv::BuiltIn builtin, bool perPrimitive)
{
  if(perPrimitive &&
     (builtin == rdcspv::BuiltIn::PrimitiveId || builtin == rdcspv::BuiltIn::Layer ||
      builtin == rdcspv::BuiltIn::ViewportIndex || builtin == rdcspv::BuiltIn::CullPrimitiveEXT ||
      builtin == rdcspv::BuiltIn::ShadingRateKHR))
    return true;

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
  if(last.type.arrayByteStride > 0)
    return byteOffset + last.type.arrayByteStride * last.type.elements;

  if(last.type.members.empty())
  {
    // this is the last basic member
    // now calculate its size and return offset + size

    RDCASSERT(last.type.elements <= 1);

    uint32_t basicTypeSize = VarTypeByteSize(last.type.baseType);

    uint32_t rows = last.type.rows;
    uint32_t cols = last.type.columns;

    // vectors are also easy
    if(rows == 1)
      return byteOffset + cols * basicTypeSize;
    if(cols == 1)
      return byteOffset + rows * basicTypeSize;

    // for matrices we need to pad 3-column or 3-row up to 4
    if(cols == 3 && last.type.RowMajor())
    {
      return byteOffset + rows * 4 * basicTypeSize;
    }
    else if(rows == 3 && last.type.ColMajor())
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

struct StructSizes
{
  uint32_t scalarAlign = 1;
  uint32_t baseAlign = 1;
  uint32_t extendedAlign = 1;

  uint32_t scalarSize = 0;
  uint32_t baseSize = 0;
  uint32_t extendedSize = 0;
};

StructSizes CalculateStructProps(uint32_t emptyStructSize, const ShaderConstant &c)
{
  StructSizes ret;

  if(c.type.baseType != VarType::Struct)
  {
    // A scalar of size N has a scalar alignment of N.
    // A vector or matrix type has a scalar alignment equal to that of its component type.
    // An array type has a scalar alignment equal to that of its element type.
    ret.scalarAlign = VarTypeByteSize(c.type.baseType);

    // A scalar has a base alignment equal to its scalar alignment.
    ret.baseAlign = ret.scalarAlign;

    // A row-major matrix of C columns has a base alignment equal to the base alignment of a vector
    // of C matrix components.
    uint8_t vecSize = c.type.columns;
    uint8_t matSize = c.type.rows;

    // A column-major matrix has a base alignment equal to the base alignment of the matrix column
    // type.
    if(c.type.rows > 1 && c.type.ColMajor())
    {
      vecSize = c.type.rows;
      matSize = c.type.columns;
    }

    // A two-component vector has a base alignment equal to twice its scalar alignment.
    if(vecSize == 2)
      ret.baseAlign *= 2;
    // A three- or four-component vector has a base alignment equal to four times its scalar
    // alignment.
    else if(vecSize == 3 || vecSize == 4)
      ret.baseAlign *= 4;

    // An array has a base alignment equal to the base alignment of its element type.
    // N/A

    // A scalar, vector or matrix type has an extended alignment equal to its base alignment.
    ret.extendedAlign = ret.baseAlign;

    // An array or structure type has an extended alignment equal to the largest extended alignment
    // of any of its members, rounded up to a multiple of 16.
    if(c.type.elements > 1)
      ret.extendedAlign = AlignUp16(ret.extendedAlign);

    if(matSize > 1)
      ret.extendedAlign = ret.baseAlign = c.type.matrixByteStride;

    ret.scalarSize =
        ret.scalarAlign * RDCMAX(c.type.rows, (uint8_t)1) * RDCMAX(c.type.columns, (uint8_t)1);
    ret.baseSize = ret.baseAlign * matSize;
    ret.extendedSize = ret.extendedAlign * matSize;
  }
  else
  {
    for(size_t i = 0; i < c.type.members.size(); i++)
    {
      const ShaderConstant &m = c.type.members[i];

      StructSizes member = CalculateStructProps(emptyStructSize, m);
      // A structure has a scalar alignment equal to the largest scalar alignment of any of its
      // members.
      ret.scalarAlign = RDCMAX(ret.scalarAlign, member.scalarAlign);
      // A structure has a base alignment equal to the largest base alignment of any of its members.
      ret.baseAlign = RDCMAX(ret.baseAlign, member.baseAlign);
      // An array or structure type has an extended alignment equal to the largest extended
      // alignment of any of its members, rounded up to a multiple of 16.
      ret.extendedAlign = RDCMAX(ret.baseAlign, member.extendedAlign);

      if(i + 1 == c.type.members.size())
      {
        // scalar struct sizes are NOT padded up to the multiple of their alignment. It is allowed
        // in scalar packing for an outside variable to 'sit in' the padding at the end of a struct
        // due to alignment. This rule is more tight than even C packing, which normally has
        // sizeof(struct) be a multiple of its alignment
        ret.scalarSize = m.byteOffset + member.scalarSize;
        ret.baseSize = AlignUp(m.byteOffset + member.baseSize, ret.baseAlign);
        ret.extendedSize = AlignUp16(m.byteOffset + member.extendedSize);
      }
    }

    ret.extendedAlign = AlignUp16(ret.extendedAlign);

    // A structure has a base alignment equal to the largest base alignment of any of its members.
    // An empty structure has a base alignment equal to the size of the smallest scalar type
    // permitted by the capabilities declared in the SPIR-V module. (e.g., for a 1 byte aligned
    // empty struct in the StorageBuffer storage class, StorageBuffer8BitAccess or
    // UniformAndStorageBuffer8BitAccess must be declared in the SPIR-V module.)
    if(c.type.members.empty())
    {
      ret.scalarSize = 0;
      ret.scalarAlign = emptyStructSize;
      ret.baseSize = emptyStructSize;
      ret.baseAlign = emptyStructSize;
      ret.extendedSize = AlignUp16(emptyStructSize);
      ret.extendedAlign = AlignUp16(emptyStructSize);
    }
  }

  ret.scalarSize *= RDCMAX(c.type.elements, 1U);
  ret.baseSize *= RDCMAX(c.type.elements, 1U);
  ret.extendedSize *= RDCMAX(c.type.elements, 1U);

  return ret;
}

void CalculateScalarLayout(uint32_t offset, rdcarray<ShaderConstant> &consts)
{
  for(size_t i = 0; i < consts.size(); i++)
  {
    consts[i].byteOffset = offset;

    CalculateScalarLayout(offset, consts[i].type.members);

    StructSizes sizes = CalculateStructProps(1, consts[i]);
    if(consts[i].type.elements > 1)
      consts[i].type.arrayByteStride = sizes.scalarSize / consts[i].type.elements;
    offset += sizes.scalarSize;
  }
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

    sourceLanguage = source.sourceLanguage;

    rdcstr name = strings[source.file];
    // don't add empty source statements as actual files
    if(!name.empty() || !source.source.empty())
    {
      if(name.empty())
        name = "unnamed_shader";

      sources.push_back({name, source.source});
    }
  }
  else if(opdata.op == Op::SourceContinued)
  {
    OpSourceContinued continued(it);

    // in theory someone could do OpSource with no source then OpSourceContinued. We treat this mostly
    // as garbage-in, garbage-out, but just in case we have no files don't crash here and instead ignore it.
    if(!sources.empty())
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
  else if(opdata.op == Op::ExtInst || opdata.op == Op::ExtInstWithForwardRefsKHR)
  {
    OpShaderDbg dbg(it);

    // we don't care about much debug info for just reflection. Only pay attention to source files,
    // and potential names of global variables that might be missing.
    if(dbg.set == knownExtSet[ExtSet_ShaderDbg])
    {
      if(dbg.inst == ShaderDbg::Source)
      {
        rdcstr name = strings[dbg.arg<Id>(0)];
        rdcstr source = dbg.params.size() > 1 ? strings[dbg.arg<Id>(1)] : rdcstr();

        // don't add empty source statements as actual files
        if(!name.empty() || !source.empty())
        {
          if(name.empty())
            name = "unnamed_shader";

          debugSources[dbg.result] = sources.size();
          sources.push_back({name, source});
        }
      }
      else if(dbg.inst == ShaderDbg::SourceContinued)
      {
        // in theory someone could do OpSource with no source then OpSourceContinued. We treat this
        // mostly as garbage-in, garbage-out, but just in case we have no files don't crash here and
        // instead ignore it.
        if(!sources.empty())
          sources.back().contents += strings[dbg.arg<Id>(0)];
      }
      else if(dbg.inst == ShaderDbg::Function)
      {
        LineColumnInfo &info = debugFuncToLocation[dbg.result];

        debugFuncName[dbg.result] = strings[dbg.arg<Id>(0)];

        // check this source file exists - we won't have registered it if there was no source code
        auto srcit = debugSources.find(dbg.arg<Id>(2));
        if(srcit != debugSources.end())
        {
          info.fileIndex = (int32_t)srcit->second;
          info.lineStart = info.lineEnd = EvaluateConstant(dbg.arg<Id>(3), {}).value.u32v[0];
        }
      }
      else if(dbg.inst == ShaderDbg::FunctionDefinition)
      {
        funcToDebugFunc[dbg.arg<Id>(1)] = dbg.arg<Id>(0);
      }
      else if(dbg.inst == ShaderDbg::CompilationUnit)
      {
        sourceLanguage = (SourceLanguage)EvaluateConstant(dbg.arg<Id>(3), {}).value.u32v[0];

        auto srcit = debugSources.find(dbg.arg<Id>(2));
        if(srcit != debugSources.end())
          compUnitToFileIndex[dbg.result] = srcit->second;
      }
      else if(dbg.inst == ShaderDbg::EntryPoint)
      {
        debugFuncToBaseFile[dbg.arg<Id>(0)] = compUnitToFileIndex[dbg.arg<Id>(1)];
        debugFuncToCmdLine[dbg.arg<Id>(0)] = strings[dbg.arg<Id>(3)];
      }
      else if(dbg.inst == ShaderDbg::GlobalVariable)
      {
        // copy the name string to the variable string only if it's empty. If it has a name already,
        // we prefer that. If the variable is DebugInfoNone then we don't care about it's name.
        if(strings[dbg.arg<Id>(7)].empty())
          strings[dbg.arg<Id>(7)] = strings[dbg.arg<Id>(0)];
      }
    }
  }
}

void Reflector::UnregisterOp(Iter it)
{
  RDCFATAL("Reflector should not be used for editing! UnregisterOp() call invalid");
}

void Reflector::CalculateArrayTypeName(DataType &type)
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
    type.name =
        StringFormat::Fmt("%s[%s]", dataTypes[type.InnerType()].name.c_str(), lengthName.c_str());
  }
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
        CalculateArrayTypeName(type);
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
          case Dim::TileImageDataEXT: name = "TileImageData"; break;
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
      else if(type.type == DataType::RayQueryType)
      {
        type.name = StringFormat::Fmt("rayQuery%u", type.id.value());
      }
      else if(type.type == DataType::AccelerationStructureType)
      {
        type.name = StringFormat::Fmt("accelerationStructure%u", type.id.value());
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

  // finally do a last pass for arrays of pointers, now that we have pointer types
  for(auto it = dataTypes.begin(); it != dataTypes.end(); ++it)
  {
    if(it->second.type == DataType::ArrayType)
    {
      DataType *iter = &it->second;
      while(iter->type == DataType::ArrayType)
        iter = &dataTypes[iter->InnerType()];

      if(iter->type == DataType::PointerType)
        CalculateArrayTypeName(it->second);
    }
  }

  for(const MemberName &mem : memberNames)
    dataTypes[mem.id].children[mem.member].name = mem.name;

  memberNames.clear();
}

rdcarray<ShaderEntryPoint> Reflector::EntryPoints() const
{
  rdcarray<ShaderEntryPoint> ret;
  ret.reserve(entries.size());
  for(const EntryPoint &e : entries)
    ret.push_back({e.name, MakeShaderStage(e.executionModel)});
  return ret;
}

void Reflector::MakeReflection(const GraphicsAPI sourceAPI, const ShaderStage stage,
                               const rdcstr &entryPoint, const rdcarray<SpecConstant> &specInfo,
                               ShaderReflection &reflection, SPIRVPatchData &patchData) const
{
  // set global properties
  reflection.entryPoint = entryPoint;
  reflection.stage = stage;
  reflection.encoding =
      sourceAPI == GraphicsAPI::OpenGL ? ShaderEncoding::OpenGLSPIRV : ShaderEncoding::SPIRV;
  reflection.rawBytes.assign((byte *)m_SPIRV.data(), m_SPIRV.size() * sizeof(uint32_t));

  CheckDebuggable(reflection.debugInfo.debuggable, reflection.debugInfo.debugStatus);

  const EntryPoint *entry = NULL;
  for(const EntryPoint &e : entries)
  {
    if(entryPoint == e.name && MakeShaderStage(e.executionModel) == stage)
    {
      entry = &e;
      break;
    }
  }

  if(!entry)
  {
    RDCERR("Entry point %s for stage %s not found in module", entryPoint.c_str(),
           ToStr(stage).c_str());
    return;
  }

  // pick up execution mode size
  if(stage == ShaderStage::Compute || stage == ShaderStage::Task || stage == ShaderStage::Mesh)
  {
    const EntryPoint &e = *entry;

    if(e.executionModes.localSizeId.x != Id())
    {
      reflection.dispatchThreadsDimension[0] =
          EvaluateConstant(e.executionModes.localSizeId.x, specInfo).value.u32v[0];
      reflection.dispatchThreadsDimension[1] =
          EvaluateConstant(e.executionModes.localSizeId.y, specInfo).value.u32v[0];
      reflection.dispatchThreadsDimension[2] =
          EvaluateConstant(e.executionModes.localSizeId.z, specInfo).value.u32v[0];
    }
    else if(e.executionModes.localSize.x > 0)
    {
      reflection.dispatchThreadsDimension[0] = e.executionModes.localSize.x;
      reflection.dispatchThreadsDimension[1] = e.executionModes.localSize.y;
      reflection.dispatchThreadsDimension[2] = e.executionModes.localSize.z;
    }

    {
      int idx = e.executionModes.others.indexOf(rdcspv::ExecutionMode::OutputVertices);
      if(idx >= 0)
        patchData.maxVertices = e.executionModes.others[idx].outputVertices;
    }

    {
      int idx = e.executionModes.others.indexOf(rdcspv::ExecutionMode::OutputPrimitivesEXT);
      if(idx >= 0)
        patchData.maxPrimitives = e.executionModes.others[idx].outputPrimitivesEXT;
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
              EvaluateConstant(c.children[i], specInfo).value.u32v[0];
      }
    }
  }
  else
  {
    reflection.dispatchThreadsDimension[0] = reflection.dispatchThreadsDimension[1] =
        reflection.dispatchThreadsDimension[2] = 0;
  }

  switch(sourceLanguage)
  {
    case SourceLanguage::ESSL:
    case SourceLanguage::GLSL: reflection.debugInfo.encoding = ShaderEncoding::GLSL; break;
    case SourceLanguage::HLSL: reflection.debugInfo.encoding = ShaderEncoding::HLSL; break;
    case SourceLanguage::Slang: reflection.debugInfo.encoding = ShaderEncoding::Slang; break;
    case SourceLanguage::OpenCL_C:
    case SourceLanguage::OpenCL_CPP:
    case SourceLanguage::CPP_for_OpenCL:
    case SourceLanguage::Unknown:
    case SourceLanguage::Invalid:
    case SourceLanguage::SYCL:
    case SourceLanguage::HERO_C:
    case SourceLanguage::NZSL:
    case SourceLanguage::WGSL:
    case SourceLanguage::Zig:
    case SourceLanguage::Max: break;
  }

  for(size_t i = 0; i < sources.size(); i++)
  {
    reflection.debugInfo.files.push_back({sources[i].name, sources[i].contents});
  }

  switch(m_Generator)
  {
    case Generator::GlslangReferenceFrontEnd:
    case Generator::ShadercoverGlslang:
      reflection.debugInfo.compiler = reflection.debugInfo.encoding == ShaderEncoding::HLSL
                                          ? KnownShaderTool::glslangValidatorHLSL
                                          : KnownShaderTool::glslangValidatorGLSL;

      if(sourceAPI == GraphicsAPI::OpenGL &&
         reflection.debugInfo.compiler == KnownShaderTool::glslangValidatorGLSL)
        reflection.debugInfo.compiler = KnownShaderTool::glslangValidatorGLSL_OpenGL;
      break;
    case Generator::SPIRVToolsAssembler:
      reflection.debugInfo.compiler = sourceAPI == GraphicsAPI::OpenGL
                                          ? KnownShaderTool::spirv_as_OpenGL
                                          : KnownShaderTool::spirv_as;
      break;
    case Generator::spiregg: reflection.debugInfo.compiler = KnownShaderTool::dxcSPIRV; break;
    case Generator::SlangCompiler:
      reflection.debugInfo.compiler = KnownShaderTool::slangSPIRV;
      break;
    default: reflection.debugInfo.compiler = KnownShaderTool::Unknown; break;
  }

  if(!cmdline.empty())
    reflection.debugInfo.compileFlags.flags = {{"@cmdline", cmdline}};

  reflection.debugInfo.compileFlags.flags.push_back(
      {"@spirver", StringFormat::Fmt("spirv%d.%d", m_MajorVersion, m_MinorVersion)});

  reflection.debugInfo.entrySourceName = entryPoint;

  {
    auto it = funcToDebugFunc.find(entry->id);
    if(it != funcToDebugFunc.end())
    {
      rdcstr debugEntryName = debugFuncName[it->second];
      if(!debugEntryName.empty())
        reflection.debugInfo.entrySourceName = debugEntryName;
      reflection.debugInfo.entryLocation = debugFuncToLocation[it->second];
      if(debugFuncToCmdLine.find(it->second) != debugFuncToCmdLine.end())
        reflection.debugInfo.compileFlags.flags = {{"@cmdline", debugFuncToCmdLine[it->second]}};
      if(debugFuncToBaseFile.find(it->second) != debugFuncToBaseFile.end())
        reflection.debugInfo.editBaseFile = (int32_t)debugFuncToBaseFile[it->second];
    }
  }

  PreprocessLineDirectives(reflection.debugInfo.files);

  // we do a mini-preprocess of the files from the debug info to handle #line directives.
  // This means that any lines that our source file declares to be in another filename via a #line
  // get put in the right place for what the debug information hopefully matches.
  // We also concatenate duplicate lines and display them all, to handle edge cases where #lines
  // declare duplicates.

  if(knownExtSet[ExtSet_ShaderDbg] != Id() && !reflection.debugInfo.files.empty())
  {
    reflection.debugInfo.compileFlags.flags.push_back({"preferSourceDebug", "1"});
    reflection.debugInfo.sourceDebugInformation = true;
  }

  std::set<Id> usedIds;
  std::map<Id, std::set<uint32_t>> usedStructChildren;
  // for arrayed top level builtins like gl_MeshPrimitivesEXT[] there could be an access chain
  // first with just the array index, then later the access to the builtin. This map tracks those
  // first access chains so the second one can reference the original global
  std::map<Id, Id> topLevelChildChain;

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

          const DataType &pointeeType = dataTypes[dataTypes[idTypes[access.base]].InnerType()];

          if(pointeeType.type == DataType::ArrayType)
          {
            const DataType &innerType = dataTypes[pointeeType.InnerType()];

            if(innerType.type == DataType::StructType &&
               (innerType.children[0].decorations.flags & rdcspv::Decorations::HasBuiltIn))
            {
              if(access.indexes.size() == 1)
              {
                topLevelChildChain[access.result] = access.base;
              }
              else if(access.indexes.size() == 2)
              {
                usedStructChildren[access.base].insert(
                    EvaluateConstant(access.indexes[1], specInfo).value.u32v[0]);
              }
            }
          }
          // save top-level children referenced in structs
          else if(pointeeType.type == DataType::StructType)
          {
            rdcspv::Id globalId = access.base;

            if(topLevelChildChain.find(access.base) != topLevelChildChain.end())
              globalId = topLevelChildChain[access.base];

            usedStructChildren[globalId].insert(
                EvaluateConstant(access.indexes[0], specInfo).value.u32v[0]);
          }
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

  if(m_MajorVersion > 1 || m_MinorVersion >= 4)
  {
    // from SPIR-V 1.4 onwards we can trust the entry point interface list to give us all used
    // global variables. We still use the above heuristic so we can remove unused members of
    // gl_PerVertex in structs.
    usedIds.clear();
    usedIds.insert(entry->usedIds.begin(), entry->usedIds.end());
  }
  else
  {
    // before that, still consider all entry interface used just not exclusively
    usedIds.insert(entry->usedIds.begin(), entry->usedIds.end());
  }

  patchData.usedIds.reserve(usedIds.size());
  for(Id id : usedIds)
    patchData.usedIds.push_back(id);

  // arrays of elements, which can be appended to in any order and then sorted
  rdcarray<SigParameter> inputs;
  rdcarray<SigParameter> outputs;
  rdcarray<sortedcblock> cblocks;
  rdcarray<sortedsamp> samplers;
  rdcarray<sortedres> roresources, rwresources;

  // for pointer types, mapping of inner type ID to index in list (assigned sequentially)
  SparseIdMap<uint16_t> pointerTypes;

  // $Globals gathering - for GL global values
  ConstantBlock globalsblock;

  // for mesh shaders, the task-mesh communication payload
  ConstantBlock taskPayloadBlock;

  // specialisation constant gathering
  ConstantBlock specblock;

  // declare pointerTypes for all declared physical pointer types first. This allows the debugger
  // to easily match pointer types itself
  for(auto it = dataTypes.begin(); it != dataTypes.end(); ++it)
  {
    if(it->second.type == DataType::PointerType &&
       it->second.pointerType.storage == rdcspv::StorageClass::PhysicalStorageBuffer)
    {
      pointerTypes.insert(std::make_pair(it->second.InnerType(), (uint16_t)pointerTypes.size()));
    }
  }

  for(const Variable &global : globals)
  {
    if(global.storage == StorageClass::Input || global.storage == StorageClass::Output)
    {
      // variable type must be a pointer of the same storage class
      RDCASSERT(dataTypes[global.type].type == DataType::PointerType);
      RDCASSERT(dataTypes[global.type].pointerType.storage == global.storage);
      const DataType &baseType = dataTypes[dataTypes[global.type].InnerType()];

      const bool isInput = (global.storage == StorageClass::Input);

      rdcarray<SigParameter> &sigarray = (isInput ? inputs : outputs);

      // try to use the instance/variable name
      rdcstr name = strings[global.id];

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

        for(const DecorationAndParamData &d : decorations[global.id].others)
        {
          if(d.value == Decoration::Component)
            name += StringFormat::Fmt("_%u", d.component);
        }
      }

      const bool used = usedIds.find(global.id) != usedIds.end();

      // only include signature parameters that are explicitly used.
      if(!used)
        continue;

      // we want to skip any members of the builtin interface block that are completely unused and
      // just came along for the ride (usually with gl_Position, but maybe declared and still
      // unused). This is meaningless in SPIR-V and just generates useless noise, but some compilers
      // from GLSL can generate the whole gl_PerVertex as a literal translation from the implicit
      // GLSL declaration.
      //
      // Some compilers generate global variables instead of members of a global struct. If this is
      // a directly decorated builtin variable which is never used, skip it
      if(IsStrippableBuiltin(
             decorations[global.id].builtIn,
             decorations[global.id].others.contains(rdcspv::Decoration::PerPrimitiveEXT)) &&
         !used)
        continue;

      // move to the inner struct if this is an array of structs - e.g. for arrayed shader outputs
      const DataType *structType = &baseType;
      if(structType->type == DataType::ArrayType &&
         dataTypes[structType->InnerType()].type == DataType::StructType)
        structType = &dataTypes[structType->InnerType()];

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
      if(structType->type == DataType::StructType)
      {
        // look to see if this struct contains a builtin member
        bool hasBuiltins = false;
        for(size_t i = 0; i < structType->children.size(); i++)
        {
          hasBuiltins = (structType->children[i].decorations.builtIn != BuiltIn::Invalid);
          if(hasBuiltins)
            break;
        }

        // if this is the builtin struct, explode the struct and call AddSignatureParameter for each
        // member here, so we can skip unused children if we want
        if(hasBuiltins)
        {
          const std::set<uint32_t> &usedchildren = usedStructChildren[global.id];

          size_t oldSigSize = sigarray.size();

          for(uint32_t i = 0; i < (uint32_t)structType->children.size(); i++)
          {
            // skip this member if it's in a builtin struct but has no builtin decoration
            if(structType->children[i].decorations.builtIn == BuiltIn::Invalid)
              continue;

            // skip this member if it's unused and of a type that is commonly included 'by accident'
            if(IsStrippableBuiltin(structType->children[i].decorations.builtIn,
                                   structType->children[i].decorations.others.contains(
                                       rdcspv::Decoration::PerPrimitiveEXT)) &&
               usedchildren.find(i) == usedchildren.end())
              continue;

            rdcstr childname = name;

            if(!structType->children[i].name.empty())
              childname += "." + structType->children[i].name;
            else
              childname += StringFormat::Fmt("._child%zu", i);

            SPIRVInterfaceAccess patch;
            patch.accessChain = {i};

            uint32_t dummy = 0;
            AddSignatureParameter(isInput, stage, global.id, structType->id, dummy, patch,
                                  childname, dataTypes[structType->children[i].type],
                                  structType->children[i].decorations, sigarray, patchData, specInfo);
          }

          // apply stream decoration from a parent struct into newly-added members
          for(const DecorationAndParamData &d : decorations[global.id].others)
          {
            if(d.value == Decoration::Stream)
            {
              for(size_t idx = oldSigSize; idx < sigarray.size(); idx++)
              {
                sigarray[idx].stream = d.stream;
              }
            }
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
            global.storage == StorageClass::PushConstant ||
            global.storage == StorageClass::TaskPayloadWorkgroupEXT)
    {
      // variable type must be a pointer of the same storage class
      RDCASSERT(dataTypes[global.type].type == DataType::PointerType);
      RDCASSERT(dataTypes[global.type].pointerType.storage == global.storage);

      const DataType *varType = &dataTypes[dataTypes[global.type].InnerType()];

      // if the outer type is an array, get the length and peel it off.
      uint32_t arraySize = 1;
      if(varType->type == DataType::ArrayType)
      {
        // runtime arrays have no length
        if(varType->length != Id())
          arraySize = EvaluateConstant(varType->length, specInfo).value.u32v[0];
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
      const bool taskPayload = (global.storage == StorageClass::TaskPayloadWorkgroupEXT);

      rdcspv::StorageClass effectiveStorage = global.storage;
      if(ssbo)
        effectiveStorage = StorageClass::StorageBuffer;

      uint32_t bindset = 0;
      if(!pushConst)
        bindset = GetDescSet(decorations[global.id].set);

      uint32_t bind = GetBinding(decorations[global.id].binding);

      // On GL if we have a location and no binding, put that in as the bind. It is not used
      // otherwise on GL as the bindings are dynamic. This should only happen for
      // bare uniforms and not for texture/buffer type uniforms which should have a binding
      if(sourceAPI == GraphicsAPI::OpenGL)
      {
        Decorations::Flags flags = Decorations::Flags(
            decorations[global.id].flags & (Decorations::HasLocation | Decorations::HasBinding));

        if(flags == Decorations::HasLocation)
        {
          bind = decorations[global.id].location;
        }
        else if(flags == Decorations::NoFlags)
        {
          bind = ~0U;
        }
      }

      if(usedIds.find(global.id) == usedIds.end())
      {
        // ignore this variable that's not in the entry point's used interface
      }
      else if(atomicCounter)
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
        res.textureType = TextureType::Buffer;
        res.descriptorType = DescriptorType::ReadWriteBuffer;

        res.variableType.columns = 1;
        res.variableType.rows = 1;
        res.variableType.baseType = VarType::UInt;
        res.variableType.name = varType->name;

        res.fixedBindSetOrSpace = 0;
        res.fixedBindNumber = GetBinding(decorations[global.id].binding);
        res.bindArraySize = arraySize;

        rwresources.push_back(sortedres(global.id, res));
      }
      else if(varType->IsOpaqueType())
      {
        // on Vulkan should never have elements that have no binding declared but are used. On GL we
        // should have gotten a location
        // above, which will be rewritten later when looking up the pipeline state since it's
        // mutable from action to action in theory.
        RDCASSERT(bind != INVALID_BIND);

        // opaque type - buffers, images, etc
        ShaderResource res;

        res.name = strings[global.id];
        if(res.name.empty())
          res.name = StringFormat::Fmt("res%u", global.id.value());

        res.fixedBindSetOrSpace = bindset;
        res.fixedBindNumber = bind;
        res.bindArraySize = arraySize;

        if(varType->type == DataType::SamplerType)
        {
          ShaderSampler samp;
          samp.name = res.name;
          samp.fixedBindSetOrSpace = bindset;
          samp.fixedBindNumber = bind;
          samp.bindArraySize = arraySize;

          samplers.push_back(sortedsamp(global.id, samp));
        }
        else if(varType->type == DataType::AccelerationStructureType)
        {
          res.descriptorType = DescriptorType::AccelerationStructure;
          res.variableType.baseType = VarType::ReadOnlyResource;
          res.isTexture = false;
          res.isReadOnly = true;

          roresources.push_back(sortedres(global.id, res));
        }
        else
        {
          Id imageTypeId = varType->id;

          if(varType->type == DataType::SampledImageType)
          {
            imageTypeId = sampledImageTypes[varType->id].baseId;
            res.hasSampler = true;
          }

          const Image &imageType = imageTypes[imageTypeId];

          if(imageType.ms)
            res.textureType =
                imageType.arrayed ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;
          else if(imageType.dim == rdcspv::Dim::_1D)
            res.textureType =
                imageType.arrayed ? TextureType::Texture1DArray : TextureType::Texture1D;
          else if(imageType.dim == rdcspv::Dim::_2D)
            res.textureType =
                imageType.arrayed ? TextureType::Texture2DArray : TextureType::Texture2D;
          else if(imageType.dim == rdcspv::Dim::Cube)
            res.textureType =
                imageType.arrayed ? TextureType::TextureCubeArray : TextureType::TextureCube;
          else if(imageType.dim == rdcspv::Dim::_3D)
            res.textureType = TextureType::Texture3D;
          else if(imageType.dim == rdcspv::Dim::Rect)
            res.textureType = TextureType::TextureRect;
          else if(imageType.dim == rdcspv::Dim::Buffer)
            res.textureType = TextureType::Buffer;
          else if(imageType.dim == rdcspv::Dim::SubpassData)
            res.textureType = TextureType::Texture2D;

          res.isTexture = res.textureType != TextureType::Buffer;
          res.isReadOnly = imageType.sampled != 2 || imageType.dim == rdcspv::Dim::SubpassData;
          res.isInputAttachment = imageType.dim == rdcspv::Dim::SubpassData;

          res.variableType.baseType = imageType.retType.Type();

          if(res.isReadOnly)
          {
            res.descriptorType =
                res.hasSampler ? DescriptorType::ImageSampler : DescriptorType::Image;
            if(!res.isTexture)
              res.descriptorType = DescriptorType::TypedBuffer;

            roresources.push_back(sortedres(global.id, res));
          }
          else
          {
            res.descriptorType = DescriptorType::ReadWriteImage;
            if(!res.isTexture)
              res.descriptorType = DescriptorType::ReadWriteTypedBuffer;

            rwresources.push_back(sortedres(global.id, res));
          }
        }
      }
      else
      {
        if(varType->type != DataType::StructType)
        {
          if(taskPayload)
          {
            if(!patchData.invalidTaskPayload)
            {
              RDCWARN(
                  "Unhandled case - non-struct task payload. Most likely DXC bug as only one task "
                  "payload variable allowed per entry point.");
              taskPayloadBlock.name = "invalid";
              taskPayloadBlock.bufferBacked = false;
              patchData.invalidTaskPayload = true;
            }
          }
          else
          {
            // global loose variable - add to $Globals block
            RDCASSERT(varType->type == DataType::ScalarType || varType->type == DataType::VectorType ||
                      varType->type == DataType::MatrixType || varType->type == DataType::ArrayType);
            RDCASSERT(sourceAPI == GraphicsAPI::OpenGL);

            ShaderConstant constant;

            MakeConstantBlockVariable(constant, pointerTypes, effectiveStorage, *varType,
                                      strings[global.id], decorations[global.id], specInfo);

            if(arraySize > 1)
              constant.type.elements = arraySize;
            else
              constant.type.elements = 0;

            constant.byteOffset = decorations[global.id].location;

            globalsblock.variables.push_back(constant);
          }
        }
        else if(taskPayload)
        {
          taskPayloadBlock.name = strings[global.id];
          if(taskPayloadBlock.name.empty())
            taskPayloadBlock.name = StringFormat::Fmt("payload%u", global.id.value());
          taskPayloadBlock.bufferBacked = false;

          MakeConstantBlockVariables(effectiveStorage, *varType, 0, 0, taskPayloadBlock.variables,
                                     pointerTypes, specInfo);

          CalculateScalarLayout(0, taskPayloadBlock.variables);
        }
        else
        {
          // on Vulkan should never have elements that have no binding declared but are used, unless
          // it's push constants (which is handled elsewhere). On GL we should have gotten a
          // location above, which will be rewritten later when looking up the pipeline state since
          // it's mutable from action to action in theory.
          RDCASSERT(pushConst || bind != INVALID_BIND);

          if(ssbo)
          {
            ShaderResource res;

            res.isReadOnly = false;
            res.isTexture = false;
            res.name = strings[global.id];
            if(res.name.empty())
              res.name = StringFormat::Fmt("ssbo%u", global.id.value());
            res.textureType = TextureType::Buffer;
            res.descriptorType = DescriptorType::ReadWriteBuffer;

            res.fixedBindNumber = bind;
            res.fixedBindSetOrSpace = bindset;
            res.bindArraySize = arraySize;

            res.variableType.columns = 0;
            res.variableType.rows = 0;
            res.variableType.baseType = VarType::Float;
            res.variableType.name = varType->name;

            MakeConstantBlockVariables(effectiveStorage, *varType, 0, 0, res.variableType.members,
                                       pointerTypes, specInfo);

            rwresources.push_back(sortedres(global.id, res));
          }
          else
          {
            ConstantBlock cblock;

            cblock.name = strings[global.id];
            if(cblock.name.empty())
              cblock.name = StringFormat::Fmt("uniforms%u", global.id.value());
            cblock.bufferBacked = !pushConst;
            cblock.inlineDataBytes = pushConst;

            cblock.fixedBindNumber = bind;
            cblock.fixedBindSetOrSpace = bindset;
            cblock.bindArraySize = arraySize;

            MakeConstantBlockVariables(effectiveStorage, *varType, 0, 0, cblock.variables,
                                       pointerTypes, specInfo);

            if(!varType->children.empty())
              cblock.byteSize = CalculateMinimumByteSize(cblock.variables);
            else
              cblock.byteSize = 0;

            cblocks.push_back(sortedcblock(global.id, cblock));
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
    if(decorations[c.id].flags & Decorations::HasSpecId)
    {
      rdcstr name = strings[c.id];
      if(name.empty())
        name = StringFormat::Fmt("specID%u", decorations[c.id].specID);

      ShaderConstant spec;
      MakeConstantBlockVariable(spec, pointerTypes, rdcspv::StorageClass::PushConstant,
                                dataTypes[c.type], name, decorations[c.id], specInfo);
      spec.byteOffset = uint32_t(specblock.variables.size() * sizeof(uint64_t));
      spec.defaultValue = c.value.value.u64v[0];
      specblock.variables.push_back(spec);

      patchData.specIDs.push_back(decorations[c.id].specID);
    }
  }

  if(!specblock.variables.empty())
  {
    specblock.name = "Specialization Constants";
    specblock.bufferBacked = false;
    specblock.inlineDataBytes = true;
    specblock.compileConstants = true;
    specblock.byteSize = 0;
    // set the binding number to some huge value to try to sort it to the end
    specblock.fixedBindNumber = 0x8000000;
    specblock.bindArraySize = 1;

    cblocks.push_back(sortedcblock(Id(), specblock));
  }

  if(!globalsblock.variables.empty())
  {
    globalsblock.name = "$Globals";
    globalsblock.bufferBacked = false;
    globalsblock.inlineDataBytes = false;
    globalsblock.byteSize = (uint32_t)globalsblock.variables.size();
    globalsblock.fixedBindSetOrSpace = 0;
    // set the binding number to some huge value to try to sort it to the end
    globalsblock.fixedBindNumber = 0x8000001;
    globalsblock.bindArraySize = 1;

    cblocks.push_back(sortedcblock(Id(), globalsblock));
  }

  reflection.taskPayload = taskPayloadBlock;

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

    reflection.outputTopology = e.executionModes.outTopo;

    if(e.executionModes.others.contains(rdcspv::ExecutionMode::OutputPoints))
      reflection.outputTopology = Topology::PointList;
    else if(e.executionModes.others.contains(rdcspv::ExecutionMode::OutputLinesEXT))
      reflection.outputTopology = Topology::LineList;
    else if(e.executionModes.others.contains(rdcspv::ExecutionMode::OutputTrianglesEXT))
      reflection.outputTopology = Topology::TriangleList;
  }

  for(auto it = extSets.begin(); it != extSets.end(); it++)
    if(it->second == "NonSemantic.DebugPrintf")
      patchData.usesPrintf = true;

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

  for(sortedcblock &cb : cblocks)
  {
    // sort the variables within each block because we want them in offset order but they don't have
    // to be declared in offset order in the SPIR-V.
    std::sort(cb.bindres.variables.begin(), cb.bindres.variables.end());
  }

  std::sort(cblocks.begin(), cblocks.end());
  std::sort(samplers.begin(), samplers.end());
  std::sort(roresources.begin(), roresources.end());
  std::sort(rwresources.begin(), rwresources.end());

  reflection.constantBlocks.resize(cblocks.size());
  reflection.samplers.resize(samplers.size());
  reflection.readOnlyResources.resize(roresources.size());
  reflection.readWriteResources.resize(rwresources.size());

  for(size_t i = 0; i < cblocks.size(); i++)
  {
    patchData.cblockInterface.push_back(cblocks[i].id);
    reflection.constantBlocks[i] = cblocks[i].bindres;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(reflection.constantBlocks[i].fixedBindNumber == INVALID_BIND)
      reflection.constantBlocks[i].fixedBindNumber = 0;
  }

  for(size_t i = 0; i < samplers.size(); i++)
  {
    patchData.samplerInterface.push_back(samplers[i].id);
    reflection.samplers[i] = samplers[i].bindres;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(reflection.samplers[i].fixedBindNumber == INVALID_BIND)
      reflection.samplers[i].fixedBindNumber = 0;
  }

  for(size_t i = 0; i < roresources.size(); i++)
  {
    patchData.roInterface.push_back(roresources[i].id);
    reflection.readOnlyResources[i] = roresources[i].bindres;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(reflection.readOnlyResources[i].fixedBindNumber == INVALID_BIND)
      reflection.readOnlyResources[i].fixedBindNumber = 0;
  }

  for(size_t i = 0; i < rwresources.size(); i++)
  {
    patchData.rwInterface.push_back(rwresources[i].id);
    reflection.readWriteResources[i] = rwresources[i].bindres;
    // fix up any bind points marked with INVALID_BIND. They were sorted to the end
    // but from here on we want to just be able to index with the bind point
    // without any special casing.
    if(reflection.readWriteResources[i].fixedBindNumber == INVALID_BIND)
      reflection.readWriteResources[i].fixedBindNumber = 0;
  }

  // go through each pointer type and populate it. This may generate more pointer types so we repeat
  // this until it converges. This is a bit redundant but not the end of the world
  size_t numPointerTypes = 0;
  do
  {
    numPointerTypes = pointerTypes.size();

    rdcarray<Id> ids;
    for(auto it = pointerTypes.begin(); it != pointerTypes.end(); ++it)
      ids.push_back(it->first);

    // generate a variable for each of the types. This will add to pointerTypes if it finds
    // something new
    for(Id id : ids)
    {
      ShaderConstant dummy;
      MakeConstantBlockVariable(dummy, pointerTypes, dataTypes[id].pointerType.storage,
                                dataTypes[id], rdcstr(), Decorations(), specInfo);
    }

    // continue if we generated some more
  } while(pointerTypes.size() != numPointerTypes);

  // populate the pointer types
  reflection.pointerTypes.reserve(pointerTypes.size());
  for(auto it = pointerTypes.begin(); it != pointerTypes.end(); ++it)
  {
    ShaderConstant dummy;

    MakeConstantBlockVariable(dummy, pointerTypes, dataTypes[it->first].pointerType.storage,
                              dataTypes[it->first], rdcstr(), Decorations(), specInfo);

    if(it->second >= reflection.pointerTypes.size())
      reflection.pointerTypes.resize(it->second + 1);

    reflection.pointerTypes[it->second] = dummy.type;
  }

  // shouldn't have changed in the above loop!
  RDCASSERT(pointerTypes.size() == numPointerTypes);
}

void Reflector::MakeConstantBlockVariables(rdcspv::StorageClass storage, const DataType &structType,
                                           uint32_t arraySize, uint32_t arrayByteStride,
                                           rdcarray<ShaderConstant> &cblock,
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
      MakeConstantBlockVariable(cblock[i], pointerTypes, storage, structType,
                                StringFormat::Fmt("[%u]", i), decorations[structType.id], specInfo);

      cblock[i].byteOffset = relativeOffset;

      relativeOffset += arrayByteStride;
    }

    return;
  }

  if(structType.children.empty())
    return;

  cblock.resize(structType.children.size());
  for(size_t i = 0; i < structType.children.size(); i++)
  {
    rdcstr name = structType.children[i].name;
    if(name.empty())
      name = StringFormat::Fmt("_child%zu", i);
    MakeConstantBlockVariable(cblock[i], pointerTypes, storage,
                              dataTypes[structType.children[i].type], name,
                              structType.children[i].decorations, specInfo);
  }

  uint32_t emptyStructSize = 4;

  if(storage == rdcspv::StorageClass::StorageBuffer)
  {
    if(capabilities.find(rdcspv::Capability::StorageBuffer8BitAccess) != capabilities.end() ||
       capabilities.find(rdcspv::Capability::UniformAndStorageBuffer8BitAccess) != capabilities.end())
      emptyStructSize = 1;
    else if(capabilities.find(rdcspv::Capability::StorageBuffer16BitAccess) != capabilities.end() ||
            capabilities.find(rdcspv::Capability::UniformAndStorageBuffer16BitAccess) !=
                capabilities.end())
      emptyStructSize = 2;
  }
  else if(storage == rdcspv::StorageClass::Uniform)
  {
    if(capabilities.find(rdcspv::Capability::UniformAndStorageBuffer8BitAccess) != capabilities.end())
      emptyStructSize = 1;
    else if(capabilities.find(rdcspv::Capability::UniformAndStorageBuffer16BitAccess) !=
            capabilities.end())
      emptyStructSize = 2;
  }
  else if(storage == rdcspv::StorageClass::PushConstant)
  {
    if(capabilities.find(rdcspv::Capability::StoragePushConstant8) != capabilities.end())
      emptyStructSize = 1;
    else if(capabilities.find(rdcspv::Capability::StoragePushConstant16) != capabilities.end())
      emptyStructSize = 2;
  }
  else if(storage == rdcspv::StorageClass::TaskPayloadWorkgroupEXT)
  {
    return;
  }

  for(size_t i = 0; i < cblock.size(); i++)
  {
    // for structs that aren't in arrays, we need to define their byte size (stride). Without
    // knowing the packing rules this shader is complying with, this is not fully possible.
    //
    // what we do is choose the most conservative size - so that this struct's size alone doesn't
    // invalidate compliance with a particular ruleset (e.g. std140).
    //
    // we calculate the scalar, base, and extended sizes of the struct sizes of the struct. The
    // largest one that fits between this struct and the next member is the one we use. If there is
    // no next member, we always use the base size as it's impossible to tell how much trailing
    // padding the shader expected.
    //
    // If we guess wrongly small, members after this struct will need an [[offset]] decoration,
    // If we guess wrongly large the struct itself will need a [[size]] decoration
    // Since we're choosing the largest valid size, it will always be just a [[size]] which might be
    // unnecessary (if e.g. somewhere else the shader demonstrates scalar packing so the padded size
    // is larger than the scalar calculated size) but that's only present in one place.

    if(cblock[i].type.baseType == VarType::Struct && cblock[i].type.arrayByteStride == 0)
    {
      // this should not be an array - if it is SPIR-V requires an array byte stride, and this
      // calculation below is also invalid.
      RDCASSERTEQUAL(cblock[i].type.elements, 1);

      StructSizes sizes = CalculateStructProps(emptyStructSize, cblock[i]);

      uint32_t availSize = ~0U;
      if(i + 1 < cblock.size())
        availSize = cblock[i + 1].byteOffset - cblock[i].byteOffset;
      else if(arrayByteStride != 0)
        availSize = arrayByteStride - cblock[i].byteOffset;

      // expect at least the scalar size to be available otherwise this struct seems to overlap
      RDCASSERT(sizes.scalarSize <= availSize, sizes.scalarSize, availSize);

      if(sizes.extendedSize <= availSize)
        cblock[i].type.arrayByteStride = sizes.extendedSize;
      else if(sizes.baseSize <= availSize)
        cblock[i].type.arrayByteStride = sizes.baseSize;
      else
        cblock[i].type.arrayByteStride = sizes.scalarSize;
    }
  }
}

void Reflector::ApplyMatrixByteStride(const DataType &type, uint8_t matrixByteStride,
                                      rdcarray<ShaderConstant> &members) const
{
  const DataType &inner = dataTypes[type.InnerType()];

  for(ShaderConstant &m : members)
  {
    if(m.type.matrixByteStride == 0)
      m.type.matrixByteStride = matrixByteStride;

    if(inner.type == DataType::ArrayType)
      ApplyMatrixByteStride(inner, matrixByteStride, m.type.members);
  }
}

void Reflector::MakeConstantBlockVariable(ShaderConstant &outConst,
                                          SparseIdMap<uint16_t> &pointerTypes,
                                          rdcspv::StorageClass storage, const DataType &type,
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
    outConst.type.elements =
        curType->length != Id() ? EvaluateConstant(curType->length, specInfo).value.u32v[0] : ~0U;

    if(varDecorations.arrayStride != ~0U)
    {
      outConst.type.arrayByteStride = varDecorations.arrayStride;
    }
    else if(decorations[curType->id].arrayStride != ~0U)
    {
      outConst.type.arrayByteStride = decorations[curType->id].arrayStride;
    }

    if(varDecorations.matrixStride != ~0U)
      outConst.type.matrixByteStride = varDecorations.matrixStride & 0xff;
    else if(decorations[curType->id].matrixStride != ~0U)
      outConst.type.matrixByteStride = decorations[curType->id].matrixStride & 0xff;

    curType = &dataTypes[curType->InnerType()];
  }

  if(curType->type == DataType::VectorType || curType->type == DataType::MatrixType)
  {
    outConst.type.baseType = curType->scalar().Type();

    if(curType->type == DataType::VectorType || (varDecorations.flags & Decorations::RowMajor))
      outConst.type.flags |= ShaderVariableFlags::RowMajorMatrix;

    if(varDecorations.matrixStride != ~0U)
      outConst.type.matrixByteStride = varDecorations.matrixStride & 0xff;

    if(curType->type == DataType::MatrixType)
    {
      outConst.type.rows = (uint8_t)curType->vector().count;
      outConst.type.columns = (uint8_t)curType->matrix().count;
    }
    else
    {
      outConst.type.columns = (uint8_t)curType->vector().count;
    }

    outConst.type.name = curType->name;
  }
  else if(curType->type == DataType::ScalarType)
  {
    outConst.type.baseType = curType->scalar().Type();
    outConst.type.flags |= ShaderVariableFlags::RowMajorMatrix;

    outConst.type.name = curType->name;
  }
  else
  {
    if(curType->type == DataType::PointerType)
    {
      outConst.type.baseType = VarType::ULong;
      outConst.type.rows = 1;
      outConst.type.columns = 1;
      outConst.type.name = curType->name;

      // try to insert the inner type ID into the map. If it succeeds, it gets the next available
      // pointer type index (size of the map), if not then we just get the previously added index
      auto it =
          pointerTypes.insert(std::make_pair(curType->InnerType(), (uint16_t)pointerTypes.size()));

      outConst.type.pointerTypeID = it.first->second;
      return;
    }

    RDCASSERT(curType->type == DataType::StructType || curType->type == DataType::ArrayType);

    outConst.type.baseType = VarType::Struct;
    outConst.type.rows = 0;
    outConst.type.columns = 0;

    outConst.type.name = curType->name;

    MakeConstantBlockVariables(storage, *curType, outConst.type.elements,
                               outConst.type.arrayByteStride, outConst.type.members, pointerTypes,
                               specInfo);

    if(curType->type == DataType::ArrayType)
    {
      // matrix byte stride is only applied on the root variable, so as we recurse down the type
      // tree for multi-dimensional arrays of matrices we need to propagate down the stride
      if(outConst.type.matrixByteStride != 0)
        ApplyMatrixByteStride(*curType, outConst.type.matrixByteStride, outConst.type.members);

      outConst.type.name = type.name;

      // if the inner type is an array, it will be expanded in our members list. So don't also
      // redundantly keep the element count
      outConst.type.arrayByteStride *= outConst.type.elements;
      outConst.type.elements = 1;
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

  if(varDecorations.others.contains(rdcspv::Decoration::PerPrimitiveEXT))
    sig.perPrimitiveRate = true;

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
    arraySize = EvaluateConstant(varType->length, specInfo).value.u32v[0];
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

      // for mesh shaders too, ignore the root level of array-ness for outputs
      if(stage == ShaderStage::Mesh && !isInput)
      {
        arraySize = 1;
        isArray = false;
      }

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

      size_t oldSigSize = sigarray.size();

      for(size_t c = 0; c < varType->children.size(); c++)
      {
        rdcstr childName = varName;

        if(isArray)
          childName += StringFormat::Fmt("[%u]", a);

        if(!varType->children[c].name.empty())
          childName += "." + varType->children[c].name;
        else
          childName += StringFormat::Fmt("._child%zu", c);

        AddSignatureParameter(isInput, stage, globalID, varType->id, regIndex, patch, childName,
                              dataTypes[varType->children[c].type],
                              varType->children[c].decorations, sigarray, patchData, specInfo);

        // increment the member-index access chain value
        patch.accessChain.back()++;
      }

      // apply stream decoration from a parent struct into newly-added members
      for(const DecorationAndParamData &d : varDecorations.others)
      {
        if(d.value == Decoration::Stream)
        {
          for(size_t idx = oldSigSize; idx < sigarray.size(); idx++)
          {
            sigarray[idx].stream = d.stream;
          }
        }
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
  {
    if(d.value == Decoration::Component)
      sig.regChannelMask <<= d.component;
    if(d.value == Decoration::Stream)
      sig.stream = d.stream;
  }

  sig.channelUsedMask = sig.regChannelMask;

  uint32_t regStep = 1;
  if(sig.varType == VarType::Double || sig.varType == VarType::ULong || sig.varType == VarType::SLong)
    regStep = 2;

  for(uint32_t a = 0; a < arraySize; a++)
  {
    rdcstr n = varName;

    if(isArray)
      n += StringFormat::Fmt("[%u]", a);
    StripCommonGLPrefixes(n);

    sig.varName = n;

    if(varType->matrix().count <= 1)
    {
      sigarray.push_back(sig);

      regIndex += regStep;

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
        s.regIndex += m * regStep;

        sigarray.push_back(s);

        if(isInput)
          patchData.inputs.push_back(patch);
        else
          patchData.outputs.push_back(patch);

        regIndex += regStep;

        // increment the matrix column access chain
        patch.accessChain.back()++;
        patch.isArraySubsequentElement = true;
      }

      // pop the matrix column access chain
      patch.accessChain.pop_back();
    }

    sig.regIndex += RDCMAX(1U, varType->matrix().count) * regStep;
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
                          ShaderReflection &refl) {
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
                       stage, entryPoint, {}, refl, patchData);
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
