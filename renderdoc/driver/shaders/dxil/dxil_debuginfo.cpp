/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Baldur Karlsson
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

#include "dxil_debuginfo.h"
#include "common/formatting.h"
#include "llvm_decoder.h"

namespace DXIL
{
enum class MetaDataRecord : uint32_t
{
  STRING_OLD = 1,
  VALUE = 2,
  NODE = 3,
  NAME = 4,
  DISTINCT_NODE = 5,
  KIND = 6,
  LOCATION = 7,
  OLD_NODE = 8,
  OLD_FN_NODE = 9,
  NAMED_NODE = 10,
  ATTACHMENT = 11,
  GENERIC_DEBUG = 12,
  SUBRANGE = 13,
  ENUMERATOR = 14,
  BASIC_TYPE = 15,
  FILE = 16,
  DERIVED_TYPE = 17,
  COMPOSITE_TYPE = 18,
  SUBROUTINE_TYPE = 19,
  COMPILE_UNIT = 20,
  SUBPROGRAM = 21,
  LEXICAL_BLOCK = 22,
  LEXICAL_BLOCK_FILE = 23,
  NAMESPACE = 24,
  TEMPLATE_TYPE = 25,
  TEMPLATE_VALUE = 26,
  GLOBAL_VAR = 27,
  LOCAL_VAR = 28,
  EXPRESSION = 29,
  OBJC_PROPERTY = 30,
  IMPORTED_ENTITY = 31,
  MODULE = 32,
  MACRO = 33,
  MACRO_FILE = 34,
  STRINGS = 35,
  GLOBAL_DECL_ATTACHMENT = 36,
  GLOBAL_VAR_EXPR = 37,
  INDEX_OFFSET = 38,
  INDEX = 39,
  LABEL = 40,
  COMMON_BLOCK = 44,
};

bool Program::ParseDebugMetaRecord(const LLVMBC::BlockOrRecord &metaRecord, Metadata &meta)
{
  MetaDataRecord id = (MetaDataRecord)metaRecord.id;

  auto getNonNullMeta = [this](uint64_t id) { return &m_Metadata[size_t(id)]; };
  auto getMeta = [this](uint64_t id) { return id ? &m_Metadata[size_t(id - 1)] : NULL; };
  auto getMetaString = [this](uint64_t id) { return id ? &m_Metadata[size_t(id - 1)].str : NULL; };

  if(id == MetaDataRecord::FILE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DIFile(getMeta(metaRecord.ops[1]), getMeta(metaRecord.ops[2]));
    meta.children = {getMeta(metaRecord.ops[1]), getMeta(metaRecord.ops[2])};
  }
  else if(id == MetaDataRecord::COMPILE_UNIT)
  {
    // should be at least 14 parameters
    RDCASSERT(metaRecord.ops.size() >= 14);

    // we expect it to be marked as distinct, but we'll always treat it that way
    RDCASSERT(metaRecord.ops[0] & 0x1);
    meta.isDistinct = true;

    meta.dwarf = new DICompileUnit(
        DW_LANG(metaRecord.ops[1]), getMeta(metaRecord.ops[2]), getMetaString(metaRecord.ops[3]),
        metaRecord.ops[4] != 0, getMetaString(metaRecord.ops[5]), metaRecord.ops[6],
        getMetaString(metaRecord.ops[7]), metaRecord.ops[8], getMeta(metaRecord.ops[9]),
        getMeta(metaRecord.ops[10]), getMeta(metaRecord.ops[11]), getMeta(metaRecord.ops[12]),
        getMeta(metaRecord.ops[13]));
    meta.children = {getMeta(metaRecord.ops[2]),  getMeta(metaRecord.ops[9]),
                     getMeta(metaRecord.ops[10]), getMeta(metaRecord.ops[11]),
                     getMeta(metaRecord.ops[12]), getMeta(metaRecord.ops[13])};
  }
  else if(id == MetaDataRecord::BASIC_TYPE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf =
        new DIBasicType(DW_TAG(metaRecord.ops[1]), getMetaString(metaRecord.ops[2]),
                        metaRecord.ops[3], metaRecord.ops[4], DW_ENCODING(metaRecord.ops[5]));
  }
  else if(id == MetaDataRecord::DERIVED_TYPE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DIDerivedType(DW_TAG(metaRecord.ops[1]), getMetaString(metaRecord.ops[2]),
                                   getMeta(metaRecord.ops[3]), metaRecord.ops[4],
                                   getMeta(metaRecord.ops[5]), getMeta(metaRecord.ops[6]),
                                   metaRecord.ops[7], metaRecord.ops[8], metaRecord.ops[9],
                                   DIFlags(metaRecord.ops[10]), getMeta(metaRecord.ops[11]));

    meta.children = {getMeta(metaRecord.ops[3]), getMeta(metaRecord.ops[5]),
                     getMeta(metaRecord.ops[6]), getMeta(metaRecord.ops[11])};
  }
  else if(id == MetaDataRecord::COMPOSITE_TYPE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    // TODO handle forward declarations?
    meta.dwarf = new DICompositeType(
        DW_TAG(metaRecord.ops[1]), getMetaString(metaRecord.ops[2]), getMeta(metaRecord.ops[3]),
        metaRecord.ops[4], getMeta(metaRecord.ops[5]), getMeta(metaRecord.ops[6]),
        metaRecord.ops[7], metaRecord.ops[8], metaRecord.ops[9], DIFlags(metaRecord.ops[10]),
        getMeta(metaRecord.ops[11]), getMeta(metaRecord.ops[14]));

    meta.children = {getMeta(metaRecord.ops[3]), getMeta(metaRecord.ops[5]),
                     getMeta(metaRecord.ops[6]), getMeta(metaRecord.ops[11]),
                     getMeta(metaRecord.ops[14])};
  }
  else if(id == MetaDataRecord::TEMPLATE_TYPE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf =
        new DITemplateTypeParameter(getMetaString(metaRecord.ops[1]), getMeta(metaRecord.ops[2]));

    meta.children = {getMeta(metaRecord.ops[2])};
  }
  else if(id == MetaDataRecord::TEMPLATE_VALUE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf =
        new DITemplateValueParameter(DW_TAG(metaRecord.ops[1]), getMetaString(metaRecord.ops[2]),
                                     getMeta(metaRecord.ops[3]), getMeta(metaRecord.ops[4]));

    meta.children = {getMeta(metaRecord.ops[3]), getMeta(metaRecord.ops[4])};
  }
  else if(id == MetaDataRecord::SUBPROGRAM)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DISubprogram(
        getMeta(metaRecord.ops[1]), getMetaString(metaRecord.ops[2]),
        getMetaString(metaRecord.ops[3]), getMeta(metaRecord.ops[4]), metaRecord.ops[5],
        getMeta(metaRecord.ops[6]), metaRecord.ops[7] != 0, metaRecord.ops[8] != 0, metaRecord.ops[9],
        getMeta(metaRecord.ops[10]), DW_VIRTUALITY(metaRecord.ops[11]), metaRecord.ops[12],
        DIFlags(metaRecord.ops[13]), metaRecord.ops[14] != 0, getMeta(metaRecord.ops[15]),
        getMeta(metaRecord.ops[16]), getMeta(metaRecord.ops[17]), getMeta(metaRecord.ops[18]));

    meta.children = {getMeta(metaRecord.ops[1]),  getMeta(metaRecord.ops[4]),
                     getMeta(metaRecord.ops[6]),  getMeta(metaRecord.ops[10]),
                     getMeta(metaRecord.ops[14]), getMeta(metaRecord.ops[15]),
                     getMeta(metaRecord.ops[16]), getMeta(metaRecord.ops[17])};
  }
  else if(id == MetaDataRecord::SUBROUTINE_TYPE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DISubroutineType(getMeta(metaRecord.ops[2]));

    meta.children = {getMeta(metaRecord.ops[2])};
  }
  else if(id == MetaDataRecord::GLOBAL_VAR)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    uint64_t version = metaRecord.ops[0] >> 1;

    if(version == 0)
    {
      meta.dwarf = new DIGlobalVariable(
          getMeta(metaRecord.ops[1]), getMetaString(metaRecord.ops[2]),
          getMetaString(metaRecord.ops[3]), getMeta(metaRecord.ops[4]), metaRecord.ops[5],
          getMeta(metaRecord.ops[6]), metaRecord.ops[7] != 0, metaRecord.ops[8] != 0,
          getMeta(metaRecord.ops[9]), getMeta(metaRecord.ops[10]));

      meta.children = {getMeta(metaRecord.ops[1]), getMeta(metaRecord.ops[4]),
                       getMeta(metaRecord.ops[6]), getMeta(metaRecord.ops[9]),
                       getMeta(metaRecord.ops[10])};
    }
    else
    {
      RDCERR("Unsupported version of global variable metadata");
    }
  }
  else if(id == MetaDataRecord::LOCATION)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.debugLoc = new DebugLocation;
    meta.debugLoc->line = metaRecord.ops[1];
    meta.debugLoc->col = metaRecord.ops[2];
    meta.debugLoc->scope = getNonNullMeta(metaRecord.ops[3]);
    meta.debugLoc->inlinedAt = getMeta(metaRecord.ops[4]);

    meta.children = {getNonNullMeta(metaRecord.ops[3]), getMeta(metaRecord.ops[4])};
  }
  else if(id == MetaDataRecord::LOCAL_VAR)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DILocalVariable(
        DW_TAG(metaRecord.ops[1]), getMeta(metaRecord.ops[2]), getMetaString(metaRecord.ops[3]),
        getMeta(metaRecord.ops[4]), metaRecord.ops[5], getMeta(metaRecord.ops[6]),
        metaRecord.ops[7], DIFlags(metaRecord.ops[8]), metaRecord.ops[8]);

    meta.children = {getMeta(metaRecord.ops[2]), getMeta(metaRecord.ops[4]),
                     getMeta(metaRecord.ops[6])};
  }
  else if(id == MetaDataRecord::LEXICAL_BLOCK)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DILexicalBlock(getMeta(metaRecord.ops[1]), getMeta(metaRecord.ops[2]),
                                    metaRecord.ops[3], metaRecord.ops[4]);

    meta.children = {getMeta(metaRecord.ops[1]), getMeta(metaRecord.ops[2])};
  }
  else if(id == MetaDataRecord::SUBRANGE)
  {
    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    meta.dwarf = new DISubrange(metaRecord.ops[1], LLVMBC::BitReader::svbr(metaRecord.ops[2]));
  }
  else if(id == MetaDataRecord::EXPRESSION)
  {
    DIExpression *expr = new DIExpression;

    meta.isDistinct = (metaRecord.ops[0] & 0x1);

    expr->op = DW_OP_none;

    if(metaRecord.ops.size() > 1)
      expr->op = DW_OP(metaRecord.ops[1]);

    if(expr->op == DW_OP_bit_piece && metaRecord.ops.size() == 4)
    {
      expr->evaluated.bit_piece.offset = metaRecord.ops[2];
      expr->evaluated.bit_piece.size = metaRecord.ops[3];
    }
    else
    {
      expr->expr.assign(metaRecord.ops.data() + 1, metaRecord.ops.size() - 1);
    }

    meta.dwarf = expr;
  }
  else
  {
    return false;
  }

  return true;
};

rdcstr Program::GetDebugVarName(const DIBase *d)
{
  if(d->type == DIBase::LocalVariable)
    return *d->As<DILocalVariable>()->name;
  if(d->type == DIBase::GlobalVariable)
    return *d->As<DIGlobalVariable>()->name;
  return "???";
}

rdcstr getOptMetaString(const Metadata *meta)
{
  return meta ? escapeString(meta->str).c_str() : "\"\"";
}

rdcstr DIFile::toString() const
{
  return StringFormat::Fmt("!DIFile(filename: %s, directory: %s)", getOptMetaString(file).c_str(),
                           getOptMetaString(dir).c_str());
}

rdcstr DICompileUnit::toString() const
{
  rdcstr ret = StringFormat::Fmt("!DICompileUnit(language: %s, file: %s", ToStr(lang).c_str(),
                                 file ? file->refString().c_str() : "null");

  if(producer)
    ret += ", producer: " + escapeString(*producer);
  ret += (isOptimized ? ", isOptimized: true" : ", isOptimized: false");
  if(flags)
    ret += ", flags: " + escapeString(*flags);
  ret += StringFormat::Fmt(", runtimeVersion: %llu", runtimeVersion);
  if(splitDebugFilename)
    ret += ", splitDebugFilename: " + escapeString(*splitDebugFilename);
  ret += StringFormat::Fmt(", emissionKind: %llu", emissionKind);
  if(enums)
    ret += ", enums: " + enums->refString();
  if(retainedTypes)
    ret += ", retainedTypes: " + retainedTypes->refString();
  if(subprograms)
    ret += ", subprograms: " + subprograms->refString();
  if(globals)
    ret += ", globals: " + globals->refString();
  if(imports)
    ret += ", imports: " + imports->refString();

  ret += ")";

  return ret;
}

rdcstr DIBasicType::toString() const
{
  rdcstr ret = "!DIBasicType(";
  if(tag != DW_TAG_base_type)
    ret += StringFormat::Fmt("tag: %s, ", ToStr(tag).c_str());
  ret += StringFormat::Fmt("name: %s, ", escapeString(name ? *name : rdcstr()).c_str());
  ret += StringFormat::Fmt("size: %llu, ", sizeInBits);
  ret += StringFormat::Fmt("align: %llu, ", alignInBits);
  ret += StringFormat::Fmt("encoding: %s", ToStr(encoding).c_str());
  ret += ")";
  return ret;
}

rdcstr DIDerivedType::toString() const
{
  rdcstr ret = StringFormat::Fmt("!DIDerivedType(tag: %s", ToStr(tag).c_str());
  if(name)
    ret += StringFormat::Fmt(", name: %s", escapeString(*name).c_str());
  if(scope)
    ret += StringFormat::Fmt(", scope: %s", scope->refString().c_str());
  if(file)
    ret += StringFormat::Fmt(", file: %s", file->refString().c_str());
  if(line)
    ret += StringFormat::Fmt(", line: %llu", line);
  if(base)
    ret += StringFormat::Fmt(", baseType: %s", base->refString().c_str());
  else
    ret += ", baseType: null";
  if(sizeInBits)
    ret += StringFormat::Fmt(", size: %llu", sizeInBits);
  if(alignInBits)
    ret += StringFormat::Fmt(", align: %llu", alignInBits);
  if(offsetInBits)
    ret += StringFormat::Fmt(", offset: %llu", offsetInBits);
  if(flags != DIFlagNone)
    ret += StringFormat::Fmt(", flags: %s", ToStr(flags).c_str());
  if(extra)
    ret += StringFormat::Fmt(", extraData: %s", extra->refString().c_str());
  ret += ")";
  return ret;
}

rdcstr DICompositeType::toString() const
{
  rdcstr ret = StringFormat::Fmt("!DICompositeType(tag: %s", ToStr(tag).c_str());
  if(name)
    ret += StringFormat::Fmt(", name: %s", escapeString(*name).c_str());
  if(scope)
    ret += StringFormat::Fmt(", scope: %s", scope->refString().c_str());
  if(file)
    ret += StringFormat::Fmt(", file: %s", file->refString().c_str());
  if(line)
    ret += StringFormat::Fmt(", line: %llu", line);
  if(base)
    ret += StringFormat::Fmt(", baseType: %s", base->refString().c_str());
  if(sizeInBits)
    ret += StringFormat::Fmt(", size: %llu", sizeInBits);
  if(alignInBits)
    ret += StringFormat::Fmt(", align: %llu", alignInBits);
  if(offsetInBits)
    ret += StringFormat::Fmt(", offset: %llu", offsetInBits);
  if(flags != DIFlagNone)
    ret += StringFormat::Fmt(", flags: %s", ToStr(flags).c_str());
  if(elements)
    ret += StringFormat::Fmt(", elements: %s", elements->refString().c_str());
  if(templateParams)
    ret += StringFormat::Fmt(", templateParams: %s", templateParams->refString().c_str());
  ret += ")";
  return ret;
}

rdcstr DITemplateTypeParameter::toString() const
{
  return StringFormat::Fmt("!DITemplateTypeParameter(name: %s, type: %s)",
                           escapeString(name ? *name : rdcstr()).c_str(),
                           type ? type->refString().c_str() : "null");
}

rdcstr DITemplateValueParameter::toString() const
{
  return StringFormat::Fmt("!DITemplateValueParameter(name: %s, type: %s, value: %s)",
                           escapeString(name ? *name : rdcstr()).c_str(),
                           type ? type->refString().c_str() : "null",
                           value ? value->refString().c_str() : "null");
}

rdcstr DISubprogram::toString() const
{
  rdcstr ret =
      StringFormat::Fmt("!DISubprogram(name: %s", escapeString(name ? *name : rdcstr()).c_str());
  if(linkageName)
    ret += StringFormat::Fmt(", linkageName: %s", escapeString(*linkageName).c_str());
  if(scope)
    ret += StringFormat::Fmt(", scope: %s", scope->refString().c_str());
  if(file)
    ret += StringFormat::Fmt(", file: %s", file->refString().c_str());
  else
    ret += ", file: null";
  if(line)
    ret += StringFormat::Fmt(", line: %llu", line);
  if(type)
    ret += StringFormat::Fmt(", type: %s", type->refString().c_str());
  ret += StringFormat::Fmt(", isLocal: %s", isLocal ? "true" : "false");
  ret += StringFormat::Fmt(", isDefinition: %s", isDefinition ? "true" : "false");
  if(scopeLine)
    ret += StringFormat::Fmt(", scopeLine: %llu", scopeLine);
  if(containingType)
    ret += StringFormat::Fmt(", containingType: %s", containingType->refString().c_str());

  if(virtuality)
  {
    ret += StringFormat::Fmt(", virtuality: %s", ToStr(virtuality).c_str());
    if(virtualIndex)
      ret += StringFormat::Fmt(", virtualIndex: %llu", virtualIndex);
  }

  if(flags != DIFlagNone)
    ret += StringFormat::Fmt(", flags: %s", ToStr(flags).c_str());

  ret += StringFormat::Fmt(", isOptimized: %s", isOptimized ? "true" : "false");

  if(function)
    ret += StringFormat::Fmt(", function: %s", function->refString().c_str());
  if(templateParams)
    ret += StringFormat::Fmt(", templateParams: %s", templateParams->refString().c_str());
  if(declaration)
    ret += StringFormat::Fmt(", declaration: %s", declaration->refString().c_str());
  if(variables)
    ret += StringFormat::Fmt(", variables: %s", variables->refString().c_str());

  ret += ")";
  return ret;
}

rdcstr DISubroutineType::toString() const
{
  return StringFormat::Fmt("!DISubroutineType(types: %s)",
                           types ? types->refString().c_str() : "null");
}

rdcstr DIGlobalVariable::toString() const
{
  rdcstr ret =
      StringFormat::Fmt("!DIGlobalVariable(name: %s", escapeString(name ? *name : rdcstr()).c_str());
  if(linkageName)
    ret += StringFormat::Fmt(", linkageName: %s", escapeString(*linkageName).c_str());
  if(scope)
    ret += StringFormat::Fmt(", scope: %s", scope->refString().c_str());
  if(file)
    ret += StringFormat::Fmt(", file: %s", file->refString().c_str());
  else
    ret += ", file: null";
  if(line)
    ret += StringFormat::Fmt(", line: %llu", line);
  if(type)
    ret += StringFormat::Fmt(", type: %s", type->refString().c_str());
  ret += StringFormat::Fmt(", isLocal: %s", isLocal ? "true" : "false");
  ret += StringFormat::Fmt(", isDefinition: %s", isDefinition ? "true" : "false");
  if(variable)
    ret += StringFormat::Fmt(", variable: %s", variable->refString().c_str());
  ret += ")";
  return ret;
}

rdcstr DILocalVariable::toString() const
{
  rdcstr ret = StringFormat::Fmt("!DILocalVariable(tag: %s, name: %s", ToStr(tag).c_str(),
                                 escapeString(name ? *name : rdcstr()).c_str());
  if(arg)
    ret += StringFormat::Fmt(", arg: %llu", arg);
  if(scope)
    ret += StringFormat::Fmt(", scope: %s", scope->refString().c_str());
  if(file)
    ret += StringFormat::Fmt(", file: %s", file->refString().c_str());
  else
    ret += ", file: null";
  if(line)
    ret += StringFormat::Fmt(", line: %llu", line);
  if(type)
    ret += StringFormat::Fmt(", type: %s", type->refString().c_str());
  if(flags != DIFlagNone)
    ret += StringFormat::Fmt(", flags: %s", ToStr(flags).c_str());
  if(alignInBits)
    ret += StringFormat::Fmt(", align: %llu", alignInBits);
  ret += ")";
  return ret;
}

rdcstr DIExpression::toString() const
{
  if(op == DW_OP_bit_piece)
    return StringFormat::Fmt("!DIExpression(DW_OP_bit_piece, %llu, %llu)",
                             evaluated.bit_piece.offset, evaluated.bit_piece.size);

  if(op == DW_OP_none)
    return "!DIExpression()";

  if(op == DW_OP_deref)
    return "!DIExpression(DW_OP_deref)";

  rdcstr ret = "!DIExpression(";
  for(size_t i = 0; i < expr.size(); i++)
  {
    if(i > 0)
      ret += ", ";
    ret += ToStr(expr[i]);
  }
  ret += ")";
  return ret;
}

rdcstr DILexicalBlock::toString() const
{
  rdcstr ret = "!DILexicalBlock(";
  if(scope)
    ret += StringFormat::Fmt("scope: %s", scope->refString().c_str());
  else
    ret += "scope: null";
  if(file)
    ret += StringFormat::Fmt(", file: %s", file->refString().c_str());
  if(line)
    ret += StringFormat::Fmt(", line: %llu", line);
  if(column)
    ret += StringFormat::Fmt(", column: %llu", column);
  ret += ")";
  return ret;
}

rdcstr DISubrange::toString() const
{
  rdcstr ret = "!DISubrange(";
  ret += StringFormat::Fmt("count: %llu", count);
  if(lowerBound)
    ret += StringFormat::Fmt(", lowerBound: %lld", lowerBound);
  ret += ")";
  return ret;
}

};    // namespace DXIL

template <>
rdcstr DoStringise(const DXIL::DW_LANG &el)
{
  using namespace DXIL;
  BEGIN_ENUM_STRINGISE(DW_LANG);
  {
    STRINGISE_ENUM_NAMED(DW_LANG_Unknown, "unknown");
    STRINGISE_ENUM(DW_LANG_C89);
    STRINGISE_ENUM(DW_LANG_C);
    STRINGISE_ENUM(DW_LANG_Ada83);
    STRINGISE_ENUM(DW_LANG_C_plus_plus);
    STRINGISE_ENUM(DW_LANG_Cobol74);
    STRINGISE_ENUM(DW_LANG_Cobol85);
    STRINGISE_ENUM(DW_LANG_Fortran77);
    STRINGISE_ENUM(DW_LANG_Fortran90);
    STRINGISE_ENUM(DW_LANG_Pascal83);
    STRINGISE_ENUM(DW_LANG_Modula2);
    STRINGISE_ENUM(DW_LANG_Java);
    STRINGISE_ENUM(DW_LANG_C99);
    STRINGISE_ENUM(DW_LANG_Ada95);
    STRINGISE_ENUM(DW_LANG_Fortran95);
    STRINGISE_ENUM(DW_LANG_PLI);
    STRINGISE_ENUM(DW_LANG_ObjC);
    STRINGISE_ENUM(DW_LANG_ObjC_plus_plus);
    STRINGISE_ENUM(DW_LANG_UPC);
    STRINGISE_ENUM(DW_LANG_D);
    STRINGISE_ENUM(DW_LANG_Python);
    STRINGISE_ENUM(DW_LANG_OpenCL);
    STRINGISE_ENUM(DW_LANG_Go);
    STRINGISE_ENUM(DW_LANG_Modula3);
    STRINGISE_ENUM(DW_LANG_Haskell);
    STRINGISE_ENUM(DW_LANG_C_plus_plus_03);
    STRINGISE_ENUM(DW_LANG_C_plus_plus_11);
    STRINGISE_ENUM(DW_LANG_OCaml);
    STRINGISE_ENUM(DW_LANG_Rust);
    STRINGISE_ENUM(DW_LANG_C11);
    STRINGISE_ENUM(DW_LANG_Swift);
    STRINGISE_ENUM(DW_LANG_Julia);
    STRINGISE_ENUM(DW_LANG_Dylan);
    STRINGISE_ENUM(DW_LANG_C_plus_plus_14);
    STRINGISE_ENUM(DW_LANG_Fortran03);
    STRINGISE_ENUM(DW_LANG_Fortran08);
    STRINGISE_ENUM(DW_LANG_Mips_Assembler);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::DW_TAG &el)
{
  using namespace DXIL;
  BEGIN_ENUM_STRINGISE(DW_TAG);
  {
    STRINGISE_ENUM(DW_TAG_array_type);
    STRINGISE_ENUM(DW_TAG_class_type);
    STRINGISE_ENUM(DW_TAG_entry_point);
    STRINGISE_ENUM(DW_TAG_enumeration_type);
    STRINGISE_ENUM(DW_TAG_formal_parameter);
    STRINGISE_ENUM(DW_TAG_imported_declaration);
    STRINGISE_ENUM(DW_TAG_label);
    STRINGISE_ENUM(DW_TAG_lexical_block);
    STRINGISE_ENUM(DW_TAG_member);
    STRINGISE_ENUM(DW_TAG_pointer_type);
    STRINGISE_ENUM(DW_TAG_reference_type);
    STRINGISE_ENUM(DW_TAG_compile_unit);
    STRINGISE_ENUM(DW_TAG_string_type);
    STRINGISE_ENUM(DW_TAG_structure_type);
    STRINGISE_ENUM(DW_TAG_subroutine_type);
    STRINGISE_ENUM(DW_TAG_typedef);
    STRINGISE_ENUM(DW_TAG_union_type);
    STRINGISE_ENUM(DW_TAG_unspecified_parameters);
    STRINGISE_ENUM(DW_TAG_variant);
    STRINGISE_ENUM(DW_TAG_common_block);
    STRINGISE_ENUM(DW_TAG_common_inclusion);
    STRINGISE_ENUM(DW_TAG_inheritance);
    STRINGISE_ENUM(DW_TAG_inlined_subroutine);
    STRINGISE_ENUM(DW_TAG_module);
    STRINGISE_ENUM(DW_TAG_ptr_to_member_type);
    STRINGISE_ENUM(DW_TAG_set_type);
    STRINGISE_ENUM(DW_TAG_subrange_type);
    STRINGISE_ENUM(DW_TAG_with_stmt);
    STRINGISE_ENUM(DW_TAG_access_declaration);
    STRINGISE_ENUM(DW_TAG_base_type);
    STRINGISE_ENUM(DW_TAG_catch_block);
    STRINGISE_ENUM(DW_TAG_const_type);
    STRINGISE_ENUM(DW_TAG_constant);
    STRINGISE_ENUM(DW_TAG_enumerator);
    STRINGISE_ENUM(DW_TAG_file_type);
    STRINGISE_ENUM(DW_TAG_friend);
    STRINGISE_ENUM(DW_TAG_namelist);
    STRINGISE_ENUM(DW_TAG_namelist_item);
    STRINGISE_ENUM(DW_TAG_packed_type);
    STRINGISE_ENUM(DW_TAG_subprogram);
    STRINGISE_ENUM(DW_TAG_template_type_parameter);
    STRINGISE_ENUM(DW_TAG_template_value_parameter);
    STRINGISE_ENUM(DW_TAG_thrown_type);
    STRINGISE_ENUM(DW_TAG_try_block);
    STRINGISE_ENUM(DW_TAG_variant_part);
    STRINGISE_ENUM(DW_TAG_variable);
    STRINGISE_ENUM(DW_TAG_volatile_type);
    STRINGISE_ENUM(DW_TAG_dwarf_procedure);
    STRINGISE_ENUM(DW_TAG_restrict_type);
    STRINGISE_ENUM(DW_TAG_interface_type);
    STRINGISE_ENUM(DW_TAG_namespace);
    STRINGISE_ENUM(DW_TAG_imported_module);
    STRINGISE_ENUM(DW_TAG_unspecified_type);
    STRINGISE_ENUM(DW_TAG_partial_unit);
    STRINGISE_ENUM(DW_TAG_imported_unit);
    STRINGISE_ENUM(DW_TAG_condition);
    STRINGISE_ENUM(DW_TAG_shared_type);
    STRINGISE_ENUM(DW_TAG_type_unit);
    STRINGISE_ENUM(DW_TAG_rvalue_reference_type);
    STRINGISE_ENUM(DW_TAG_template_alias);
    STRINGISE_ENUM(DW_TAG_auto_variable);
    STRINGISE_ENUM(DW_TAG_arg_variable);
    STRINGISE_ENUM(DW_TAG_coarray_type);
    STRINGISE_ENUM(DW_TAG_generic_subrange);
    STRINGISE_ENUM(DW_TAG_dynamic_type);
    STRINGISE_ENUM(DW_TAG_MIPS_loop);
    STRINGISE_ENUM(DW_TAG_format_label);
    STRINGISE_ENUM(DW_TAG_function_template);
    STRINGISE_ENUM(DW_TAG_class_template);
    STRINGISE_ENUM(DW_TAG_GNU_template_template_param);
    STRINGISE_ENUM(DW_TAG_GNU_template_parameter_pack);
    STRINGISE_ENUM(DW_TAG_GNU_formal_parameter_pack);
    STRINGISE_ENUM(DW_TAG_APPLE_property);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::DW_ENCODING &el)
{
  using namespace DXIL;
  BEGIN_ENUM_STRINGISE(DW_ENCODING);
  {
    STRINGISE_ENUM(DW_ATE_address);
    STRINGISE_ENUM(DW_ATE_boolean);
    STRINGISE_ENUM(DW_ATE_complex_float);
    STRINGISE_ENUM(DW_ATE_float);
    STRINGISE_ENUM(DW_ATE_signed);
    STRINGISE_ENUM(DW_ATE_signed_char);
    STRINGISE_ENUM(DW_ATE_unsigned);
    STRINGISE_ENUM(DW_ATE_unsigned_char);
    STRINGISE_ENUM(DW_ATE_imaginary_float);
    STRINGISE_ENUM(DW_ATE_packed_decimal);
    STRINGISE_ENUM(DW_ATE_numeric_string);
    STRINGISE_ENUM(DW_ATE_edited);
    STRINGISE_ENUM(DW_ATE_signed_fixed);
    STRINGISE_ENUM(DW_ATE_unsigned_fixed);
    STRINGISE_ENUM(DW_ATE_decimal_float);
    STRINGISE_ENUM(DW_ATE_UTF);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::DW_VIRTUALITY &el)
{
  using namespace DXIL;
  BEGIN_ENUM_STRINGISE(DW_VIRTUALITY);
  {
    STRINGISE_ENUM(DW_VIRTUALITY_none);
    STRINGISE_ENUM(DW_VIRTUALITY_virtual);
    STRINGISE_ENUM(DW_VIRTUALITY_pure_virtual);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::DIFlags &el)
{
  using namespace DXIL;
  BEGIN_BITFIELD_STRINGISE(DIFlags);
  {
    // these are manual because they're a non-bitfield within a bitfield
    if((el & DXIL::DIFlagPublic) == DXIL::DIFlagPublic)
      ret += " | DIFlagPublic";
    else if((el & DXIL::DIFlagPublic) == DXIL::DIFlagPrivate)
      ret += " | DIFlagPrivate";
    else if((el & DXIL::DIFlagPublic) == DXIL::DIFlagProtected)
      ret += " | DIFlagProtected";
    local &= ~DXIL::DIFlagPublic;
    STRINGISE_BITFIELD_BIT(DIFlagFwdDecl);
    STRINGISE_BITFIELD_BIT(DIFlagAppleBlock);
    STRINGISE_BITFIELD_BIT(DIFlagBlockByrefStruct);
    STRINGISE_BITFIELD_BIT(DIFlagVirtual);
    STRINGISE_BITFIELD_BIT(DIFlagArtificial);
    STRINGISE_BITFIELD_BIT(DIFlagExplicit);
    STRINGISE_BITFIELD_BIT(DIFlagPrototyped);
    STRINGISE_BITFIELD_BIT(DIFlagObjcClassComplete);
    STRINGISE_BITFIELD_BIT(DIFlagObjectPointer);
    STRINGISE_BITFIELD_BIT(DIFlagVector);
    STRINGISE_BITFIELD_BIT(DIFlagStaticMember);
    STRINGISE_BITFIELD_BIT(DIFlagLValueReference);
    STRINGISE_BITFIELD_BIT(DIFlagRValueReference);
  }
  END_BITFIELD_STRINGISE();
}
